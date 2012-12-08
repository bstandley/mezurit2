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

static double ave_circle_buffer (CircleBuffer *cbuf, int vci);
static void run_circle_buffer (CircleBuffer *cbuf, double *data, int N_chan);
static void compute_intervals (ScanVars *sv, Scan *scan_array, double loop_interval);
static void set_blackout (Clk *clk, double dwell, double blackout);

bool run_acquisition (ThreadVars *tv, CircleBuffer *cbuf)
{
	for (int id = 0; id < M2_NUM_DAQ; id++) if (daq_multi_tick(id) != 1)
	{
		status_add(1, cat1("Error: Acquisition failed.\n"));
		return 0;
	}

	mt_mutex_lock(&tv->gpib_mutex);
	for (int vci = 0; vci < tv->chanset->N_total_chan; vci++)
		tv->known_daq[vci] = compute_function_read(&tv->chanset->channel_by_vci[vci]->cf, COMPUTE_MODE_POINT, &tv->data_daq[vci]);
	mt_mutex_unlock(&tv->gpib_mutex);

	if (cbuf->length > 1) run_circle_buffer(cbuf, tv->data_daq, tv->chanset->N_total_chan);

	// copy to shared buffers:
	if (mt_mutex_trylock(&tv->data_mutex))  // GUI does not require the absolute latest data
	{
		for (int vci = 0; vci < tv->chanset->N_total_chan; vci++)
		{
			tv->data_shared[vci]  = tv->data_daq[vci];
			tv->known_shared[vci] = tv->known_daq[vci];
		}
		mt_mutex_unlock(&tv->data_mutex);
	}

	return 1;
}

void init_circle_buffer (Logger *logger, CircleBuffer *cbuf)
{
	cbuf->index = 0;
	cbuf->filled = 0;
	cbuf->length = logger->cbuf_length;

	logger->cbuf_dirty = 0;
}

double ave_circle_buffer (CircleBuffer *cbuf, int vci)
{
	double sum = 0;
	for (int cbi = 0; cbi < cbuf->filled; cbi++)
		sum += cbuf->data[cbi][vci];
	return sum / cbuf->filled;
}

void run_circle_buffer (CircleBuffer *cbuf, double *data, int N_chan)
{
	// update number of filled rows
	if (cbuf->filled < cbuf->length) cbuf->filled++;

	// add data to cbuf, compute new average
	for (int vci = 0; vci < N_chan; vci++)
	{
		cbuf->data[cbuf->index][vci] = data[vci];
		data[vci] = ave_circle_buffer(cbuf, vci);
	}

	// update index for next time
	cbuf->index++;
	if(cbuf->index == cbuf->length) cbuf->index = 0;
}

void run_recording (ThreadVars *tv, CircleBuffer *cbuf, Clk *clk, bool *binsize_valid, double *binsize, Logger *logger, Buffer *buffer)
{
	bool record_ok = (get_logger_rl(tv) == LOGGER_RL_RECORD);

	for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++)
		if (clk[ici].bo_enabled)
		{
			if ((timer_elapsed(clk[ici].bo_timer) < clk[ici].bo_target)) record_ok = 0;
			else clk[ici].bo_enabled = 0;
		}

	mt_mutex_lock(&buffer->mutex);
	if (buffer->do_time_reset)
	{
		init_circle_buffer(logger, cbuf);  // avoid averaging old time() data which is discontinuous
		timer_reset(buffer->timer);
		buffer->do_time_reset = 0;
	}
	else if (record_ok)  // skip data taking this cycle to avoid old time() data
	{
		VSP vs = active_vsp(buffer);
		if (vs->N_pt == 0)
		{
			append_point(vs, tv->data_daq);
			vs->data_saved = 0;
		}
		else
		{
			for (int vci = 0; vci < tv->chanset->N_total_chan; vci++)
				if (binsize_valid[vci] && fabs(tv->data_daq[vci] - vs_value(vs, vs->N_pt - 1, vci)) > binsize[vci])
				{
					append_point(vs, tv->data_daq);
					vs->data_saved = 0;
					break;
				}
		}
	}
	mt_mutex_unlock(&buffer->mutex);
}

void run_triggers (Panel *panel)
{
	mt_mutex_lock(&panel->trigger_mutex);

	for (int n = 0; n < M2_MAX_TRIG; n++) if (panel->trigger[n].any_line_dirty) trigger_parse(&panel->trigger[n]);
	for (int n = 0; n < M2_MAX_TRIG; n++) if (panel->trigger[n].armed)          trigger_check(&panel->trigger[n]);
	for (int n = 0; n < M2_MAX_TRIG; n++) if (panel->trigger[n].busy)           trigger_exec (&panel->trigger[n]);

	mt_mutex_unlock(&panel->trigger_mutex);
}

bool run_scope_start (ThreadVars *tv, ScanVars *sv, Scope *scope, double loop_interval)
{
	bool ok = 0;
	if (scope->master_id != -1)  // extra check
	{
		mt_mutex_lock(&tv->panel->sweep_mutex);
		for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++)
		{
			request_sweep_dir(&tv->panel->sweep[ici], 0, 0);
			exec_sweep_dir(&tv->panel->sweep[ici]);
		}
		mt_mutex_unlock(&tv->panel->sweep_mutex);

		compute_intervals(sv, scope->scan, loop_interval);

		set_scan_progress(&tv->panel->buffer, 0);

		if (tv->pulse_vci != -1)
		{
			compute_function_read (&tv->chanset->channel_by_vci[tv->pulse_vci]->cf, COMPUTE_MODE_POINT, &tv->pulse_original);
			compute_function_write(&tv->chanset->channel_by_vci[tv->pulse_vci]->cf, tv->pulse_target);
		}

		if (scan_array_start(scope->scan, tv->scope_bench_timer)) ok = 1;
		else scan_array_stop(scope->scan, sv->s_read_total);
	}

	if (ok)
	{
		mt_mutex_lock(&tv->ts_mutex);
		if (str_equal(get_cmd(M2_TS_ID), "catch_scan_start")) control_server_reply(M2_TS_ID, "catch_scan_start");
		mt_mutex_unlock(&tv->ts_mutex);
	}
	else set_scan_callback_mode(tv, 0);  // unblock callbacks

	return ok;
}

bool run_scope_continue (ThreadVars *tv, ScanVars *sv, Scope *scope, Buffer *buffer)
{
	
	double elapsed = daq_SCAN_elapsed(scope->master_id);
	if (get_scope_rl(tv) == SCOPE_RL_SCAN && elapsed < scope->scan[scope->master_id].total_time)
	{
		scan_array_read(scope->scan, sv->counter, sv->read_mult, sv->s_read_total);  // increments counter

		if (sv->counter[scope->master_id] % sv->prog_mult == 0) set_scan_progress(buffer, elapsed / scope->scan[scope->master_id].total_time);

		return 1;
	}
	else
	{
		scan_array_stop(scope->scan, sv->s_read_total);

		if (tv->pulse_vci != -1) compute_function_write(&tv->chanset->channel_by_vci[tv->pulse_vci]->cf, tv->pulse_original);

		set_scan_progress(buffer, elapsed / scope->scan[scope->master_id].total_time);

		scan_array_process(scope->scan, buffer, tv->chanset);

		set_scan_callback_mode(tv, 0);  // unblock callbacks

		mt_mutex_lock(&tv->ts_mutex);
		if (str_equal(get_cmd(M2_TS_ID), "catch_scan_finish")) control_server_reply(M2_TS_ID, "catch_scan_finish");
		mt_mutex_unlock(&tv->ts_mutex);

		return 0;
	}
}

void compute_intervals (ScanVars *sv, Scan *scan_array, double loop_interval)
{
	sv->prog_mult = floor_int(1.0/M2_SCOPE_PROGRESS_RATE / loop_interval);

	f_print(F_BENCH, "loop interval: %f ms\n", loop_interval * 1e3);
	f_print(F_BENCH, "progress multiple: %d\n", sv->prog_mult);

	for (int id = 0; id < M2_NUM_DAQ; id++)
	{
		sv->counter[id] = 0;
		sv->read_mult[id] = floor_int(scan_array[id].read_interval / loop_interval);
		sv->s_read_total[id] = 0;

		f_print(F_BENCH, "DAQ%d: read interval: %f ms\n", id, scan_array[id].read_interval * 1e3);
		f_print(F_BENCH, "DAQ%d: read multiple: %d\n",    id, sv->read_mult[id]);
	}
}

void run_sweep_step (Sweep *sweep, double t, Clk *clk, SweepEvent *sweep_event)
{
	double dt = t - clk->t0;

	if (sweep->dir == 0 || sweep->dir_dirty)
	{
		clk->t0 += dt;
		if (sweep->jump_dirty)
		{
			if (sweep->channel != NULL)
			{
				bool success = compute_function_write(&sweep->channel->cf, sweep->jump_scaled);
				status_add(1, supercat("Set \"%s\" to %f: %s\n", sweep->channel->desc, sweep->jump_scaled, success ? "Success" : "Failure"));
			}
			sweep->jump_dirty = 0;
		}
	}
	else
	{
		if (sweep->holding)
		{
			clk->t0     += dt;
			clk->t_hold += dt;

			if (sweep->dir == -1 && clk->t_hold >= sweep->hold.value[LOWER])
			{
				request_sweep_dir(sweep, sweep->endstop[LOWER] ? 0 : 1, 0);  // stop or turn around
				sweep_event->min_posthold = sweep_event->any = 1;
			}
			else if (sweep->dir == 1 && clk->t_hold >= sweep->hold.value[UPPER])
			{
				request_sweep_dir(sweep, sweep->endstop[UPPER] ? 0 : -1, 0);
				sweep_event->max_posthold = sweep_event->any = 1;
			}
		}
		else
		{
			// Note: sweep.dir != 0 should ensure that sweep.channel != NULL

			double current;
			if (compute_function_read(&sweep->channel->cf, COMPUTE_MODE_POINT, &current))
			{
				double dt_adj = round_down_double(dt, sweep->dwell.value[sweep->dir == 1 ? UPPER : LOWER] / 1e3);
				if (dt_adj > 0.0)
				{
					double target = (sweep->dir == 1) ? current + sweep->rate.value[UPPER] * dt_adj : 
					                                    current - sweep->rate.value[LOWER] * dt_adj;

					if (sweep->dir == -1 && target < sweep->scaled.value[LOWER])
					{
						// make sure we hit the endpoint exactly:
						compute_function_write(&sweep->channel->cf, sweep->scaled.value[LOWER]);
						set_blackout(clk, sweep->dwell.value[LOWER], sweep->blackout.value[LOWER]);
						request_sweep_dir(sweep, -1, 1);  // hold at min
						clk->t_hold = 0;
						sweep_event->min = sweep_event->any = 1;
					}
					else if (sweep->dir == 1 && target > sweep->scaled.value[UPPER])
					{
						// make sure we hit the endpoint exactly:
						compute_function_write(&sweep->channel->cf, sweep->scaled.value[UPPER]);
						set_blackout(clk, sweep->dwell.value[UPPER], sweep->blackout.value[UPPER]);
						request_sweep_dir(sweep, 1, 1);  // hold at max
						clk->t_hold = 0;
						sweep_event->max = sweep_event->any = 1;
					}
					else if ((sweep->zerostop[UPPER] && current < 0 && target >= 0) ||
					         (sweep->zerostop[LOWER] && current > 0 && target <= 0))
					{
						compute_function_write(&sweep->channel->cf, 0);  // make sure we hit the endpoint exactly
						int side = (sweep->dir == 1) ? UPPER : LOWER;
						set_blackout(clk, sweep->dwell.value[side], sweep->blackout.value[side]);
						request_sweep_dir(sweep, 0, 0);  // stop sweep
						sweep_event->zerostop = sweep_event->any = 1;
					}
					else if (compute_function_write(&sweep->channel->cf, target))
					{
						int side = (sweep->dir == 1) ? UPPER : LOWER;
						set_blackout(clk, sweep->dwell.value[side], sweep->blackout.value[side]);
						clk->t0 += dt_adj;
					}
				}
			}
			else
			{
				request_sweep_dir(sweep, 0, 0);  // stop sweep
				status_add(1, cat3("Warning: Will not start sweep \"", sweep->channel->desc, "\" from unknown current value.\n"));
			}
		}
	}
}

void set_blackout (Clk *clk, double dwell, double blackout)
{
	clk->bo_enabled = 1;
	timer_reset(clk->bo_timer);
	clk->bo_target = (dwell / 1e3) * (blackout / 100.0);
}

void run_sweep_response (ThreadVars *tv, SweepEvent *sweep_event)
{
	mt_mutex_lock(&tv->ts_mutex);
	if (tv->catch_sweep_ici != -1)
	{
		char *cmd = get_cmd(M2_TS_ID);

		if ((sweep_event[tv->catch_sweep_ici].zerostop     && (str_equal(cmd, "catch_sweep_zerostop")))     ||
		    (sweep_event[tv->catch_sweep_ici].min          && (str_equal(cmd, "catch_sweep_min")))          ||
		    (sweep_event[tv->catch_sweep_ici].max          && (str_equal(cmd, "catch_sweep_max")))          ||
		    (sweep_event[tv->catch_sweep_ici].min_posthold && (str_equal(cmd, "catch_sweep_min_posthold"))) ||
		    (sweep_event[tv->catch_sweep_ici].max_posthold && (str_equal(cmd, "catch_sweep_max_posthold"))))
		{
			tv->catch_sweep_ici = -1;
			control_server_reply(M2_TS_ID, cmd);
		}
	}
	mt_mutex_unlock(&tv->ts_mutex);

	for (int ici = 0; ici < tv->chanset->N_inv_chan; ici++) clear_sweep_event(&sweep_event[ici]);
}

void clear_sweep_event (SweepEvent *sweep_event)
{
	sweep_event->any          = 0;
	sweep_event->zerostop     = 0;
	sweep_event->min          = 0;
	sweep_event->max          = 0;
	sweep_event->min_posthold = 0;
	sweep_event->max_posthold = 0;
}
