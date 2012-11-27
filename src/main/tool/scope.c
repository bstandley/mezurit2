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

#include "scope.h"

#include <math.h>

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <lib/util/num.h>

static void verify_timescale (Scope *scope);  // lock before calling
static void update_readout   (Scope *scope);  // locking not required

#include "scope_callback.c"

void scope_init (Scope *scope, GtkWidget **apt)
{
	f_start(F_INIT);

	section_init(&scope->sect, atg(sharepath("pixmaps/tool_scope.png")), "Scope", apt);
	add_loc_menu(&scope->sect, SECTION_WAYLEFT, SECTION_LEFT, SECTION_TOP, SECTION_BOTTOM, SECTION_RIGHT, SECTION_WAYRIGHT, -1);
	add_rollup_button(&scope->sect);
	add_orient_button(&scope->sect);

	// container:
	GtkWidget *table = pack_start(new_table(2, 3, 4, 2), 0, scope->sect.box);

	scope->rate_entry = new_numeric_entry(0, 3, 7);
	scope->time_entry = new_numeric_entry(0, 2, 7);

	set_entry_unit (scope->rate_entry, "kHz");
	set_entry_min  (scope->rate_entry, 0);
	set_entry_unit (scope->time_entry, "s");
	set_entry_min  (scope->time_entry, 0);

	table_attach(new_label("f<sub>DAQ</sub>", 0.0),    0, 0, GTK_FILL, table);
	table_attach(scope->rate_entry->widget,            1, 0, 0,        table);
	table_attach(new_label("t<sub>sample</sub>", 0.0), 0, 1, GTK_FILL, table);
	table_attach(scope->time_entry->widget,            1, 1, 0,        table);

	// controls:
	GtkWidget *control_vbox = gtk_vbox_new(0, 2);
	gtk_table_attach(GTK_TABLE(table), control_vbox, 2, 3, 0, 2, 0, 0, 0, 0);

	scope->button = pack_start(gtk_button_new(), 0, control_vbox);
	scope->image  = pack_start(gtk_image_new(),  0, control_vbox);

	set_draw_on_expose(scope->sect.full, scope->image);

	// status:
	GtkWidget *status_hbox = container_add(gtk_hbox_new(0, 0),
	                         container_add(fix_shadow(gtk_frame_new(NULL)),
	                         pack_start(name_widget(gtk_event_box_new(), "m2_align"), 0, scope->sect.box)));

	scope->status_left  = pack_start(new_text_view(1, 4), 0, status_hbox);
	scope->status_right = pack_start(new_text_view(0, 0), 1, status_hbox);

	mt_mutex_init(&scope->mutex);
	scope->master_id = -1;
	scope->scanning = 0;
}

void set_scope_runlevel (Scope *scope, int rl)
{
	f_start(F_RUN);

	gtk_widget_set_sensitive(scope->button,             rl != SCOPE_RL_STOP && rl != SCOPE_RL_HOLD);
	gtk_widget_set_sensitive(scope->rate_entry->widget, rl != SCOPE_RL_SCAN);
	gtk_widget_set_sensitive(scope->time_entry->widget, rl != SCOPE_RL_SCAN);

	gtk_button_set_label(GTK_BUTTON(scope->button), rl == SCOPE_RL_SCAN ? "CANCEL" : "SCAN");
	gtk_image_set_from_pixbuf(GTK_IMAGE(scope->image), lookup_pixbuf(rl == SCOPE_RL_SCAN  ? PIXBUF_RL_SCAN  :
	                                                                 rl == SCOPE_RL_READY ? PIXBUF_RL_READY :
	                                                                 rl == SCOPE_RL_HOLD  ? PIXBUF_RL_HOLD  :
	                                                                                        PIXBUF_RL_STOP));
}

void scope_update (Scope *scope, ChanSet *chanset)
{
	f_start(F_UPDATE);
	
	for (int id = 0; id < M2_NUM_DAQ; id++)
		scope->scan[id].N_chan = reduce_chanset(chanset, id, scope->scan[id].phys_chan);

	verify_timescale(scope);
	update_readout(scope);
}

void verify_timescale (Scope *scope)  // lock before calling
{
	f_start(F_RUN);

	scope->master_id = -1;
	for (int id = 0; id < M2_NUM_DAQ; id++)
	{
		Scan *scan = &scope->scan[id];

		// scan N_chan and phys_chan already set by scope_update()
		scan->total_time = scope->timescale_time;
		scan->rate_kHz   = scope->timescale_rate;

		/**/                   daq_SCAN_prepare(id, scan);  // may update scan.daq_rate_kHz, scan.status
		if (scan->status == 4) daq_SCAN_prepare(id, scan);  // try again, if necessary

		// find scan with longest total_time: (should all be similar except for rounding errors)
		if (scan->status == 1 && (scope->master_id == -1 || scan->total_time > scope->scan[scope->master_id].total_time))
			scope->master_id = id;
	}
}

void scope_final (Scope *scope)
{
	f_start(F_INIT);

	mt_mutex_clear(&scope->mutex);
	destroy_entry(scope->rate_entry);
	destroy_entry(scope->time_entry);
}

void scope_register (Scope *scope, int pid, GtkWidget **apt)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Scope", MCF_W);
	section_register(&scope->sect, atg(supercat("panel%d_scope_", pid)), SECTION_LEFT, apt);

	int rate_var = mcf_register(&scope->timescale_rate, atg(supercat("panel%d_scope_rate_kHz",  pid)), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 20.0);
	int time_var = mcf_register(&scope->timescale_time, atg(supercat("panel%d_scope_time_sec",  pid)), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.5);
	/**/           mcf_register(NULL,                   atg(supercat("panel%d_scope_overwrite", pid)), MCF_BOOL);  // Obsolete

	mcf_connect(rate_var, "setup, panel", BLOB_CALLBACK(timescale_mcf), 0x20, scope, scope->rate_entry);
	mcf_connect(time_var, "setup, panel", BLOB_CALLBACK(timescale_mcf), 0x20, scope, scope->time_entry);

	snazzy_connect(scope->rate_entry->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(timescale_cb), 0x30, scope, scope->rate_entry, &scope->timescale_rate);
	snazzy_connect(scope->time_entry->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(timescale_cb), 0x30, scope, scope->time_entry, &scope->timescale_time);
}

void scope_register_legacy (Scope *scope)
{
	f_start(F_INIT);

	int rate0_var = mcf_register(&scope->timescale_rate, "daq_rate_kHz",    MCF_DOUBLE);
	int time0_var = mcf_register(&scope->timescale_time, "daq_time_msec",   MCF_DOUBLE);
	int rate1_var = mcf_register(&scope->timescale_rate, "scope_rate_kHz",  MCF_DOUBLE);
	int time1_var = mcf_register(&scope->timescale_time, "scope_time_msec", MCF_DOUBLE);  // Note: Name was a mistake. Actually seconds.
	/**/            mcf_register(NULL,                   "scope_overwrite", MCF_BOOL);  // Obsolete

	mcf_connect_with_note(rate0_var, "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(timescale_mcf),    0x20, scope, scope->rate_entry);
	mcf_connect_with_note(time0_var, "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(old_daq_time_mcf), 0x10, scope);
	mcf_connect_with_note(rate1_var, "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(timescale_mcf),    0x20, scope, scope->rate_entry);
	mcf_connect_with_note(time1_var, "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(timescale_mcf),    0x20, scope, scope->time_entry);
}

void update_readout (Scope *scope)  // locking not required
{
	f_start(F_RUN);

	char *left  [M2_NUM_DAQ];
	char *right [M2_NUM_DAQ];

	for (int id = 0; id < M2_NUM_DAQ; id++)
	{
		Scan *scan = &scope->scan[id];

		if (scan->N_chan > 0)
		{
			left[id] = atg(id != M2_VDAQ_ID ? supercat("DAQ%d:\n", id) : cat1("VDAQ:\n"));

			if (scan->status == 1)
			{
				char *points_str = atg(scan->N_pt < 1000    ? supercat("%d pt",              scan->N_pt)     :
				                       scan->N_pt < 1000000 ? supercat("%1.2f kpt", (double) scan->N_pt/1e3) :
				                                              supercat("%1.2f Mpt", (double) scan->N_pt/1e6));
				right[id] = atg(supercat("%s (%d ch/pt)\n%1.6f kHz", points_str, scan->N_chan, scan->rate_kHz));
			}
			else right[id] = atg(cat1("Setup error"));
		}
		else left[id] = right[id] = NULL;
	}

	set_text_view_text(scope->status_left,  atg(join_lines(left,  "\n", M2_NUM_DAQ)));
	set_text_view_text(scope->status_right, atg(join_lines(right, "\n", M2_NUM_DAQ)));
}

bool scan_array_start (Scan *scan_array, Timer *timer)
{
	double t_called = timer_elapsed(timer);

	for (int id = 0; id < M2_NUM_DAQ; id++)
		if (scan_array[id].status == 1)
		{
			if (daq_SCAN_start(id) != 1)
			{
				f_print(F_ERROR, "Error: daq_SCAN_start() failed.\n");
				return 0;
			}
		}

	f_print(F_BENCH, "Info: Called at %1.3f msec, returned %1.3f msec later.\n", 1e3 * t_called, 1e3 * (timer_elapsed(timer) - t_called));
	return 1;
}

void scan_array_read (Scan *scan_array, int *counter, int *read_mult, long *s_read_total)
{
	for (int id = 0; id < M2_NUM_DAQ; id++)
	{
		if (scan_array[id].status == 1 && counter[id] % read_mult[id] == 0 && counter[id] != 0)  // skip first time
			s_read_total[id] += daq_SCAN_read(id);

		counter[id]++;
	}
}

void scan_array_stop (Scan *scan_array, long *s_read_total)
{
	for (int id = 0; id < M2_NUM_DAQ; id++)
		if (scan_array[id].status == 1)
		{
			s_read_total[id] += daq_SCAN_stop(id);  // clean up extra data and stop device (if necessary)
			f_print(F_RUN, "Info: daq_SCAN_read(%d) saved %ld samples.\n", id, s_read_total[id]);
		}
}

void scan_array_process (Scan *scan_array, Buffer *buffer, ChanSet *chanset)
{
	f_start(F_RUN);

	double full_data [M2_MAX_CHAN];
	double prefactor [M2_MAX_CHAN];

	for (int vci = 0; vci < chanset->N_total_chan; vci++)
		prefactor[vci] = chanset->channel_by_vci[vci]->cf.prefactor;

	compute_save_context();
	compute_set_context(full_data, prefactor, chanset->vci_by_vc, chanset->N_total_chan);

	mt_mutex_lock(&buffer->mutex);
	if (buffer->svs->last_vs->N_pt > 0) add_set(buffer, chanset);
	VSP vs = buffer->svs->last_vs;

	for (int id = 0; id < M2_NUM_DAQ; id++)
	{
		Scan *scan = &scan_array[id];
		if (scan->status == 1)
		{
			for (long j = 0; j < scan->N_pt; j++)
			{
				compute_set_time((double) j / (scan->rate_kHz * 1e3));
				compute_set_point(j);

				for (int vci = 0; vci < chanset->N_total_chan; vci++)
					compute_function_read(&chanset->channel_by_vci[vci]->cf, COMPUTE_MODE_SCAN, &full_data[vci]);

				append_point(vs, full_data);
			}

			daq_SCAN_prepare(id, &scan_array[id]);  // re-prepare scan (timescale should not have changed because callbacks are blocked during scanning)
		}
	}
	
	if (buffer->svs->last_vs->N_pt > 0) add_set(buffer, chanset);  // add another set for future logging if this one got any data
	mt_mutex_unlock(&buffer->mutex);

	compute_restore_context();  // put logger settings back
}
