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

#include "gui.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/num.h>
#include <lib/util/str.h>
#include <lib/hardware/timing.h>
#include <control/server.h>

static bool run_runlevel_updates (ThreadVars *tv, Panel *panel, int *logger_rld, int *scope_rld);  // rv indicates whether a scan is occurring
static void clear_gtk_events (double wait);

#include "gui_callback.c"

void thread_register_gui (ThreadVars *tv)
{
	f_start(F_INIT);

	control_server_connect(M2_TS_ID, "read_channel",             all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(read_channel_csf),   0x30, tv->chanset, tv->data_gui, tv->known_gui);
	control_server_connect(M2_TS_ID, "get_sweep_id",             all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(get_sweep_id_csf),   0x10, tv->chanset);
	control_server_connect(M2_TS_ID, "gpib_send_recv",           all_pid(M2_CODE_GUI) | M2_CODE_SETUP, BLOB_CALLBACK(gpib_send_recv_csf), 0x10, tv);
	control_server_connect(M2_TS_ID, "catch_sweep_zerostop",     all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_sweep_csf),    0x10, tv);
	control_server_connect(M2_TS_ID, "catch_sweep_min",          all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_sweep_csf),    0x10, tv);
	control_server_connect(M2_TS_ID, "catch_sweep_max",          all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_sweep_csf),    0x10, tv);
	control_server_connect(M2_TS_ID, "catch_sweep_min_posthold", all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_sweep_csf),    0x10, tv);
	control_server_connect(M2_TS_ID, "catch_sweep_max_posthold", all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_sweep_csf),    0x10, tv);

	control_server_connect(M2_TS_ID, "catch_scan_start",         all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_scan_csf),     0x00);
	control_server_connect(M2_TS_ID, "catch_scan_finish",        all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_scan_csf),     0x00);
	control_server_connect(M2_TS_ID, "catch_signal",             all_pid(M2_CODE_GUI),                 BLOB_CALLBACK(catch_signal_csf),   0x10, tv);
}

void thread_register_panel (ThreadVars *tv, Panel *panel)
{
	f_start(F_INIT);
	
	control_server_connect(M2_TS_ID, "clear_buffer", M2_CODE_GUI << panel->pid, BLOB_CALLBACK(clear_csf),      0x30, tv, &panel->buffer, &panel->plot);
	control_server_connect(M2_TS_ID, "gpib_pause",   M2_CODE_GUI << panel->pid, BLOB_CALLBACK(gpib_pause_csf), 0x20, tv, &panel->logger);

	snazzy_connect(panel->logger.button,       "clicked",              SNAZZY_VOID_VOID, BLOB_CALLBACK(record_cb),     0x10, tv);
	snazzy_connect(panel->scope.button,        "clicked",              SNAZZY_VOID_VOID, BLOB_CALLBACK(scan_cb),       0x10, tv);
	snazzy_connect(panel->buffer.clear_button, "clicked",              SNAZZY_VOID_VOID, BLOB_CALLBACK(clear_cb),      0x30, tv, &panel->buffer, &panel->plot);
	snazzy_connect(panel->logger.gpib_button,  "button-release-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(gpib_pause_cb), 0x10, tv);
}

void run_gui_thread (ThreadVars *tv, Channel *channel_array, Panel *panel_array)
{
	f_start(F_UPDATE);

	if (tv->pid < 0) return;  // extra check

	Panel   *panel   = &panel_array[tv->pid];
	ChanSet *chanset = tv->chanset;
	Buffer  *buffer  = &panel->buffer;
	Plot    *plot    = &panel->plot;

	tv->panel = panel;  // save for daq_thread

	// setup updates

	compute_reset();
	compute_set_pid(tv->pid);
	build_chanset(chanset, channel_array);

	buffer_update(buffer, chanset);

	plot_update_channels(plot, chanset->vc_by_vci, buffer->svs);
	plot->exposure_blocked = 0;
	plot->exposure_complete = 0;

	logger_update        (&panel->logger, chanset);
	scope_update         (&panel->scope,  chanset);
	sweep_array_update   (panel->sweep,   chanset);
	trigger_array_update (panel->trigger);

	clear_gtk_events(0);

	if (!plot->exposure_complete)
	{
		full_plot(plot);  // redraw plot in case nothing caused an "expose" event
		plot->needs_context_regen = 1;
	}

	// shared variables

	for (int vci = 0; vci < chanset->N_total_chan; vci++)
	{
		tv->data_gui[vci]  = tv->data_shared[vci]  = tv->data_daq[vci]  = 0;
		tv->known_gui[vci] = tv->known_shared[vci] = tv->known_daq[vci] = 0;
	}

	// timing

	double primary_interval         = 0.8 / M2_DEFAULT_GUI_RATE;
	double secondary_interval       = 0.2 / M2_DEFAULT_GUI_RATE;
	double primary_boost_interval   = 0.8 / M2_BOOST_GUI_RATE;
	double secondary_boost_interval = 0.2 / M2_BOOST_GUI_RATE;

	Timer *reader_timer = timer_new();
	Timer *buffer_timer = timer_new();

	double reader_target = 1.0 / M2_MAX_READER_RATE;
	double buffer_target = 1.0 / M2_MAX_BUFFER_STATUS_RATE;
	
	// in-flight plotting:
	plot_reset(plot);
	plot_open(plot);

	// show as ready to go (or not)

	int logger_rld = tv->logger_rl = buffer->locked ? LOGGER_RL_HOLD : LOGGER_RL_IDLE;
	int scope_rld  = tv->scope_rl  = (panel->scope.master_id != -1) ? (buffer->locked ? SCOPE_RL_HOLD : SCOPE_RL_READY) : SCOPE_RL_STOP;

	set_logger_runlevel (&panel->logger, tv->logger_rl);
	set_scope_runlevel  (&panel->scope,  tv->scope_rl);
	set_logger_scanning (&panel->logger, 0);

	set_buffer_buttons(buffer, total_pts(buffer->svs) == 0, buffer->svs->last_vs->N_pt > 0);
	clear_gtk_events(0);

	// start daq thread:
	MtThread daq_thread = mt_thread_create(run_daq_thread, tv);
	f_print(F_UPDATE, "Created DAQ thread.\n");

	while (get_logger_rl(tv) != LOGGER_RL_STOP)
	{
		clear_gtk_events(5e-5);

		mt_mutex_lock(&tv->ts_mutex);
		if (tv->terminal_dirty)
		{
			bool matched = control_server_iterate(M2_TS_ID, M2_CODE_GUI << tv->pid);
			if (!matched) control_server_reply(M2_TS_ID, "command_error");
			tv->terminal_dirty = 0;
		}
		mt_mutex_unlock(&tv->ts_mutex);

		xleep(plot->draw_request < M2_BOOST_THRESHOLD_PTS ? primary_interval : primary_boost_interval);  // primary sleeper

		if (status_regenerate())
		{
			message_update(&panel->message);
			clear_gtk_events(0);
		}

		bool scanning = run_runlevel_updates(tv, panel, &logger_rld, &scope_rld);

		if (!scanning && overtime_then_reset(reader_timer, reader_target))
		{
			// copy data from DAQ thread:
			mt_mutex_lock(&tv->data_mutex);
			for (int vci = 0; vci < chanset->N_total_chan; vci++)
			{
				tv->data_gui[vci]  = tv->data_shared[vci];
				tv->known_gui[vci] = tv->known_shared[vci];
			}
			mt_mutex_unlock(&tv->data_mutex);

			run_reader_status(&panel->logger, chanset, tv->known_gui, tv->data_gui);
			clear_gtk_events(5e-5);
		}

		if (overtime_then_reset(buffer_timer, buffer_target))
			if (run_buffer_status(buffer, &plot->display))
				clear_gtk_events(5e-5);

		if (plot->enabled)  // check how many points need to be plotted
		{
			mt_mutex_lock(&buffer->mutex);
			plot->draw_request = min_long(total_pts(buffer->svs) - plot->draw_total, M2_MAX_GRADUAL_PTS);
			mt_mutex_unlock(&buffer->mutex);
		}
		else plot->draw_request = 0;

		xleep(plot->draw_request < M2_BOOST_THRESHOLD_PTS ? secondary_interval : secondary_boost_interval);  // secondary sleeper

		if (plot->draw_request > 0 || plot->display.str_dirty || plot->display.percent != plot->display.old_percent) plot_tick(plot);  // in-flight plotting
	}

	plot_close(plot);

	mt_thread_join(daq_thread);
	f_print(F_UPDATE, "Joined DAQ thread.\n");

	tv->panel = NULL;
	compute_set_pid(-1);
}

bool run_runlevel_updates (ThreadVars *tv, Panel *panel, int *logger_rld, int *scope_rld)
{
	bool events       = set_trigger_buttons_all (panel->trigger, &panel->trigger_mutex);
	bool sweep_events = set_sweep_buttons_all   (panel->sweep, tv->chanset->N_inv_chan);

	// update runlevels:
	mt_mutex_lock(&tv->rl_mutex);
	int logger_rl = tv->logger_rl;
	int scope_rl  = tv->scope_rl;
	mt_mutex_unlock(&tv->rl_mutex);

	if (*logger_rld != logger_rl)
	{
		set_logger_runlevel(&panel->logger, logger_rl);
		*logger_rld = logger_rl;
		events = 1;
	}

	if (*scope_rld != scope_rl)
	{
		set_scope_runlevel(&panel->scope, scope_rl);
		*scope_rld = scope_rl;

		set_logger_scanning    (&panel->logger, scope_rl == SCOPE_RL_SCAN);
		set_sweep_scanning_all (panel->sweep,   scope_rl == SCOPE_RL_SCAN);
		events = 1;
	}

	// exec events:
	if (events || sweep_events) clear_gtk_events(5e-5);

	// clear dir_dirty only after buttons have actually flipped on the screen:
	if (sweep_events)
	{
		mt_mutex_lock(&panel->sweep_mutex);
		for (int n = 0; n < tv->chanset->N_inv_chan; n++) panel->sweep[n].dir_dirty = 0;
		mt_mutex_unlock(&panel->sweep_mutex);
	}

	return (scope_rl == SCOPE_RL_SCAN);
}

void clear_gtk_events (double wait)
{
	while (gtk_events_pending())
	{
		gtk_main_iteration();
		if (wait > 0) xleep(wait);
	}
}
