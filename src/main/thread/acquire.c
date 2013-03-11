/*
 *  Copyright (C) 2012 California Institute of Technology
 *
 *  This file is part of Mezurit2, written by Brian Standley <brian@brianstandley.com>.
 *
 *  Mezurit2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option) any later version.
 *
 *  Mezurit2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this
 *  program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "acquire.h"

#include <math.h>

#include <lib/status.h>
#include <lib/util/num.h>
#include <lib/util/str.h>
#include <lib/hardware/timing.h>
#include <lib/hardware/daq.h>
#include <lib/hardware/gpib.h>
#include <control/server.h>

struct Clk
{
	double t0, t_hold, bo_target;
	Timer *bo_timer;
	bool bo_enabled;

};

struct CircleBuffer
{
	double data[M2_MAX_CBUF_LENGTH][M2_MAX_CHAN];
	int index, filled, length;

};

struct ScanVars
{
	int  counter      [M2_NUM_DAQ];
	int  read_mult    [M2_NUM_DAQ];
	long s_read_total [M2_NUM_DAQ];
	int  prog_mult;

};

struct SweepEvent
{
	bool any, zerostop, min, max, min_posthold, max_posthold;

};

static gpointer run_gpib_thread (gpointer);

static bool run_acquisition    (ThreadVars *tv, struct CircleBuffer *cbuf);
static void run_recording      (ThreadVars *tv, struct CircleBuffer *cbuf, struct Clk *clk, bool *binsize_valid, double *binsize, Logger *logger, Buffer *buffer);
static void run_triggers       (Panel *panel);
static bool run_scope_start    (ThreadVars *tv, struct ScanVars *sv, Scope *scope, double loop_interval);
static bool run_scope_continue (ThreadVars *tv, struct ScanVars *sv, Scope *scope, Buffer *buffer);
static void run_sweep_step     (Sweep *sweep, double t, struct Clk *clk, struct SweepEvent *sweep_event);
static void run_sweep_response (ThreadVars *tv, struct SweepEvent *sweep_event);

static void init_circle_buffer (Logger *logger, struct CircleBuffer *cbuf);
static void clear_sweep_event (struct SweepEvent *sweep_event);

#include "acquire_sub.c"
#include "acquire_callback.c"

void thread_init_all (ThreadVars *tv)
{
	f_start(F_INIT);

	mt_mutex_init(&tv->rl_mutex);
	mt_mutex_init(&tv->gpib_mutex);
	mt_mutex_init(&tv->data_mutex);
	mt_mutex_init(&tv->ts_mutex);

	tv->pid = -1;
	tv->panel = NULL;

	tv->terminal_dirty = 0;
	tv->pulse_vci = -1;
	tv->catch_sweep_ici = -1;
	tv->catch_signal = NULL;

	tv->scope_bench_timer = timer_new();
}

void thread_final_all (ThreadVars *tv)
{
	f_start(F_INIT);

	mt_mutex_clear(&tv->rl_mutex);
	mt_mutex_clear(&tv->gpib_mutex);
	mt_mutex_clear(&tv->data_mutex);
	mt_mutex_clear(&tv->ts_mutex);

	timer_destroy(tv->scope_bench_timer);
}

void thread_register_daq (ThreadVars *tv)
{
	f_start(F_INIT);

	control_server_connect(M2_TS_ID, "set_recording", all_pid(M2_CODE_DAQ), BLOB_CALLBACK(record_csf),        0x10, tv);
	control_server_connect(M2_LS_ID, "set_recording", all_pid(M2_CODE_DAQ), BLOB_CALLBACK(record_csf),        0x10, tv);
	control_server_connect(M2_TS_ID, "set_scanning",  all_pid(M2_CODE_DAQ), BLOB_CALLBACK(scan_csf),          0x10, tv);
	control_server_connect(M2_LS_ID, "set_scanning",  all_pid(M2_CODE_DAQ), BLOB_CALLBACK(scan_csf),          0x10, tv);

	control_server_connect(M2_LS_ID, "emit_signal",   all_pid(M2_CODE_DAQ), BLOB_CALLBACK(emit_signal_csf),   0x10, tv);
	control_server_connect(M2_TS_ID, "request_pulse", all_pid(M2_CODE_DAQ), BLOB_CALLBACK(request_pulse_csf), 0x10, tv);
	control_server_connect(M2_LS_ID, "request_pulse", all_pid(M2_CODE_DAQ), BLOB_CALLBACK(request_pulse_csf), 0x10, tv);
}

gpointer run_gpib_thread (gpointer data)
{
	ThreadVars *tv = data;
	Timer *timer _timerfree_ = timer_new();

	int sr_id, sr_pad, sr_eos;  // ignore "used uninitialized" warnings
	bool sr_expect_reply;
	char *sr_msg = NULL;

	do
	{
		if (tv->gpib_msg != NULL)
		{
			sr_id  = tv->gpib_id;
			sr_pad = tv->gpib_pad;
			sr_eos = tv->gpib_eos;

			sr_msg = tv->gpib_msg;
			tv->gpib_msg = NULL;

			sr_expect_reply = tv->gpib_expect_reply;
		}

		if (!tv->gpib_paused)
		{
			for (int id = 0; id < M2_NUM_GPIB; id++) gpib_multi_transfer(id);
			mt_mutex_unlock(&tv->gpib_mutex);

			for (int id = 0; id < M2_NUM_GPIB; id++) gpib_multi_tick(id);
		}
		else mt_mutex_unlock(&tv->gpib_mutex);

		if (sr_msg != NULL)
		{
			if (sr_eos) gpib_device_set_eos(sr_id, sr_pad, sr_eos);

			char *msg_out = gpib_string_query(sr_id, sr_pad, sr_msg, str_length(sr_msg), sr_expect_reply);
			char *reply _strfree_ = sr_expect_reply ? cat3(get_cmd(M2_TS_ID), ";msg|", msg_out) : cat1(get_cmd(M2_TS_ID));

			replace(sr_msg, NULL);

			mt_mutex_lock(&tv->ts_mutex);
			control_server_reply(M2_TS_ID, reply);
			mt_mutex_unlock(&tv->ts_mutex);
		}

		wait_and_reset(timer, 0.02);

		mt_mutex_lock(&tv->gpib_mutex);
	}
	while (tv->gpib_running);

	mt_mutex_unlock(&tv->gpib_mutex);
	return data;
}

gpointer run_daq_thread (gpointer data)
{
	ThreadVars *tv = data;

	Logger *logger = &tv->panel->logger;
	Scope  *scope  = &tv->panel->scope;
	Buffer *buffer = &tv->panel->buffer;

	// buffers setup

	struct CircleBuffer cbuf;
	init_circle_buffer(logger, &cbuf);

	double binsize       [M2_MAX_CHAN];  // index: vci
	bool   binsize_valid [M2_MAX_CHAN];  // index: vci

	for (int vci = 0; vci < tv->chanset->N_total_chan; vci++)
	{
		binsize[vci] = tv->chanset->channel_by_vci[vci]->binsize;
		binsize_valid[vci] = (binsize[vci] >= 0);
	}

	// common variables used by functions

	struct SweepEvent sweep_event[M2_MAX_CHAN];
	for (int ici = 0; ici < M2_MAX_CHAN; ici++) clear_sweep_event(&sweep_event[ici]);

	// set up computing environment

	double prefactor[M2_MAX_CHAN];  // index: vci
	for (int vci = 0; vci < tv->chanset->N_total_chan; vci++)
		prefactor[vci] = tv->chanset->channel_by_vci[vci]->cf.prefactor;

	compute_set_context(tv->data_daq, prefactor, tv->chanset->vci_by_vc, tv->chanset->N_total_chan);

	// timing

	Timer *sweep_timer _timerfree_ = timer_new();
	Timer *limit_timer _timerfree_ = timer_new();
	Timer *poll_timer  _timerfree_ = timer_new();

	double poll_target  = 1.0 / M2_TERMINAL_POLLING_RATE;
	double limit_target = 1e-3 / logger->max_rate;

	struct Clk clk[M2_MAX_CHAN];
	for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++)
	{
		clk[ici].t0 = clk[ici].t_hold = 0;  // should not need an inital value
		clk[ici].bo_enabled = 0;
		clk[ici].bo_timer = timer_new();
		clk[ici].bo_target = 0;
	}

	mt_mutex_unlock(&tv->rl_mutex);

	// main data aquisition loops

	struct ScanVars sv;
	bool scanning = 0;
	bool daq_failed = 0;

	tv->gpib_running = 1;
	tv->gpib_paused = 0;
	tv->gpib_msg = NULL;

	mt_mutex_lock(&tv->gpib_mutex);
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32
	GThread *gpib_thread = g_thread_create(run_gpib_thread, tv, 1, NULL);
#else
	GThread *gpib_thread = g_thread_new("GPIB", run_gpib_thread, tv);
#endif
	f_print(F_UPDATE, "Created GPIB thread.\n");

	mt_mutex_lock   (&tv->gpib_mutex);  // run_gpib_thread will unlock when everything is ready to go
	mt_mutex_unlock (&tv->gpib_mutex);

	long k_sleep = 1;
	while (get_logger_rl(tv) != LOGGER_RL_STOP)
	{
		// control rate:
		if (wait_and_reset(limit_timer, limit_target)) k_sleep = 1;  // reset timer either way
		else if (M2_SLEEP_MULT == k_sleep++)  // encourage os to switch back to outer thread occasionally if it isn't already
		{
			g_thread_yield();  // not appropriate for RT mode (needs fixing)
			k_sleep = 1;
		}

		// poll control server:
		if (timer_elapsed(poll_timer) > poll_target && mt_mutex_trylock(&tv->ts_mutex))
		{
			if (control_server_poll(M2_TS_ID))  // does nothing, returns 0, if a command is already in progress
				tv->terminal_dirty = !control_server_iterate(M2_TS_ID, M2_CODE_DAQ << tv->pid);
			mt_mutex_unlock(&tv->ts_mutex);
			timer_reset(poll_timer);  // reset timer only if we were over the target time and successfully locked the server for polling
		}

		if (!scanning)
		{
			if (mt_mutex_trylock(&logger->mutex))  // GUI thread only locks when necessary, i.e., rarely
			{
				if (logger->max_rate_dirty)
				{
					limit_target = 1e-3 / logger->max_rate;
					logger->max_rate_dirty = 0;
				}

				if (logger->cbuf_dirty) init_circle_buffer(logger, &cbuf);

				mt_mutex_unlock(&logger->mutex);
			}

			compute_set_time(timer_elapsed(buffer->timer));

			if (!daq_failed && !run_acquisition(tv, &cbuf))
			{
				mt_mutex_lock(&tv->rl_mutex);
				tv->logger_rl = LOGGER_RL_HOLD;
				tv->scope_rl  = SCOPE_RL_HOLD;
				mt_mutex_unlock(&tv->rl_mutex);

				daq_failed = 1;
			}

			run_recording(tv, &cbuf, clk, binsize_valid, binsize, logger, buffer);
		}

		run_triggers(tv->panel);

		if (!scanning)
		{
			if (get_scope_rl(tv) == SCOPE_RL_SCAN)
			{
				if (run_scope_start(tv, &sv, scope, limit_target)) scanning = 1;
				else set_scanning(tv, SCOPE_RL_STOP);
			}
		}
		else
		{
			if (!run_scope_continue(tv, &sv, scope, buffer))
			{
				set_scanning(tv, SCOPE_RL_READY);
				scanning = 0;
			}
		}

		if (!scanning)
		{
			bool any_event = 0;

			mt_mutex_lock(&tv->panel->sweep_mutex);
			mt_mutex_lock(&tv->gpib_mutex);

			double t = timer_elapsed(sweep_timer);  // All sweeps share the same timebase this way.
			for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++)
			{
				run_sweep_step(&tv->panel->sweep[ici], t, &clk[ici], &sweep_event[ici]);
				if (sweep_event[ici].any) any_event = 1;
			}

			for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++) exec_sweep_dir(&tv->panel->sweep[ici]);  // Actually change sweep dir/hold now that everyone has had a chance to update.
			                                                                                                 // Otherwise leader/follower relationships will get out of sync.
			mt_mutex_unlock(&tv->gpib_mutex);
			mt_mutex_unlock(&tv->panel->sweep_mutex);

			if (any_event) run_sweep_response (tv, sweep_event);
		}
	}

	if (scanning) run_scope_continue(tv, &sv, scope, buffer);

	mt_mutex_lock(&tv->gpib_mutex);
	tv->gpib_running = 0;
	mt_mutex_unlock(&tv->gpib_mutex);
	g_thread_join(gpib_thread);
	f_print(F_UPDATE, "Joined GPIB thread.\n");

	for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++) timer_destroy(clk[ici].bo_timer);

	for (int id = 0; id < M2_NUM_DAQ;  id++) daq_multi_reset(id);
	for (int id = 0; id < M2_NUM_GPIB; id++) gpib_board_reset(id);

	return data;
}

void stop_threads (ThreadVars *tv)
{
	mt_mutex_lock(&tv->panel->sweep_mutex);
	for(int n = 0; n < M2_MAX_CHAN; n++)
	{
		request_sweep_dir(&tv->panel->sweep[n], 0, 0);
		exec_sweep_dir(&tv->panel->sweep[n]);
	}
	mt_mutex_unlock(&tv->panel->sweep_mutex);

	mt_mutex_lock(&tv->rl_mutex);
	tv->logger_rl = LOGGER_RL_STOP;
	tv->scope_rl  = SCOPE_RL_STOP;
	mt_mutex_unlock(&tv->rl_mutex);
}

void set_recording (ThreadVars *tv, int rl)
{
	mt_mutex_lock(&tv->rl_mutex);
	if (tv->logger_rl != LOGGER_RL_STOP && rl != tv->logger_rl)
	{
		if (rl == RL_NO_HOLD)
		{
			if (tv->logger_rl == LOGGER_RL_HOLD) tv->logger_rl = LOGGER_RL_IDLE;
		}
		else if (tv->logger_rl != LOGGER_RL_HOLD)
		{
			tv->logger_rl = (rl == RL_TOGGLE) ? ((tv->logger_rl == LOGGER_RL_IDLE) ? LOGGER_RL_RECORD : LOGGER_RL_IDLE) : rl;
			if (tv->scope_rl == SCOPE_RL_SCAN && tv->logger_rl == LOGGER_RL_RECORD) tv->logger_rl = LOGGER_RL_WAIT;
		}
	}
	mt_mutex_unlock(&tv->rl_mutex);
}

bool set_scanning (ThreadVars *tv, int rl)
{
	mt_mutex_lock(&tv->rl_mutex);
	if (tv->scope_rl != SCOPE_RL_STOP && rl != tv->scope_rl)  // must manually escape from SCOPE_RL_STOP...
	{
		if (rl == RL_NO_HOLD)
		{
			if (tv->scope_rl == SCOPE_RL_HOLD) tv->scope_rl = SCOPE_RL_READY;
		}
		else if (tv->scope_rl != SCOPE_RL_HOLD)
		{
			tv->scope_rl = (rl == RL_TOGGLE) ? ((tv->scope_rl == SCOPE_RL_READY) ? SCOPE_RL_SCAN : SCOPE_RL_READY) : rl;
			if (tv->scope_rl == SCOPE_RL_SCAN) timer_reset(tv->scope_bench_timer);

			if (tv->scope_rl == SCOPE_RL_SCAN && tv->logger_rl == LOGGER_RL_RECORD) tv->logger_rl = LOGGER_RL_WAIT;
			if (tv->scope_rl != SCOPE_RL_SCAN && tv->logger_rl == LOGGER_RL_WAIT)   tv->logger_rl = LOGGER_RL_RECORD;
		}
	}

	bool on = (tv->scope_rl == SCOPE_RL_SCAN);
	mt_mutex_unlock(&tv->rl_mutex);
	return on;
}

int get_logger_rl (ThreadVars *tv)
{
	mt_mutex_lock(&tv->rl_mutex);
	int rl = tv->logger_rl;
	mt_mutex_unlock(&tv->rl_mutex);
	return rl;
}

int get_scope_rl (ThreadVars *tv)
{
	mt_mutex_lock(&tv->rl_mutex);
	int rl = tv->scope_rl;
	mt_mutex_unlock(&tv->rl_mutex);
	return rl;
}

void set_scan_callback_mode (ThreadVars *tv, bool scanning)
{
	if (tv->pid < 0) return;

	mt_mutex_lock(&tv->panel->sweep_mutex);
	for (int n = 0; n < M2_MAX_CHAN; n++) tv->panel->sweep[n].scanning = scanning;  // block callbacks
	mt_mutex_unlock(&tv->panel->sweep_mutex);

	mt_mutex_lock(&tv->panel->scope.mutex);
	tv->panel->scope.scanning = scanning;  // block callbacks
	mt_mutex_unlock(&tv->panel->scope.mutex);
}
