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

#include "sweep.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/fs.h>
#include <control/server.h>

enum
{
	STEP_TO_DWELL = -98,  // avoid conflict with COMPUTE_LINEAR_*
	DWELL_TO_STEP = -99
};

static GtkWidget * make_sweep_section (Sweep *sweep);
static GtkWidget * make_jump_section (Sweep *sweep);
static void update_section_visibility (Sweep *sweep);
static double step_dwell_compute (Sweep *sweep, int side, int mode);
static void attach_range (NumericRange *nr, int row, const char *label_str, const char *unit_str, GtkWidget *table);

#include "sweep_callback.c"

void sweep_array_init (Sweep *sweep_array, GtkWidget **apt)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_MAX_CHAN; id++)
	{
		Sweep *sweep = &sweep_array[id];

		// section:
		section_init(&sweep->sect, atg(sharepath("pixmaps/tool_sweep.png")), atg(supercat("S%d", id)), apt);
		add_loc_menu(&sweep->sect, SECTION_WAYLEFT, SECTION_LEFT, SECTION_TOP, SECTION_BOTTOM, SECTION_RIGHT, SECTION_WAYRIGHT, -1);

		// channel, visibility modes:
		sweep->name_entry       = pack_start(new_entry(0, 8),                         1, sweep->sect.heading);
		sweep->vis_sweep_button = pack_start(flatten_button(gtk_toggle_button_new()), 0, sweep->sect.heading);
		sweep->vis_jump_button  = pack_start(flatten_button(gtk_toggle_button_new()), 0, sweep->sect.heading);

		set_padding(sweep->name_entry, 2);
		gtk_widget_set_can_focus(sweep->name_entry, 0);
		gtk_editable_set_editable(GTK_EDITABLE(sweep->name_entry), 0);

		gtk_widget_set_name(container_add(gtk_image_new(), sweep->vis_sweep_button), "m2_icon_sweep");
		gtk_widget_set_name(container_add(gtk_image_new(), sweep->vis_jump_button),  "m2_icon_jump");

		gtk_widget_set_tooltip_text(sweep->vis_sweep_button, "Sweep up/down controls");
		gtk_widget_set_tooltip_text(sweep->vis_jump_button,  "Jump-to-target control");

		// make sections:
		sweep->sweep_section = pack_start(make_sweep_section(sweep),                     0, sweep->sect.box);
		sweep->hsep          = pack_start(gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, sweep->sect.box);
		sweep->jump_section  = pack_start(make_jump_section(sweep),                      0, sweep->sect.box);

		// initialization:
		sweep->channel = NULL;
		sweep->follower = NULL;
		sweep->pending = 0;
		sweep->dir = sweep->request_dir = 0;
		sweep->holding = sweep->request_hold = 0;
		sweep->scanning = 0;

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sweep->stop_button), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sweep->down_button), 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sweep->up_button),   0);

		sweep->dir_dirty = 0;
		sweep->jump_dirty = 0;
	}
}

void sweep_array_register (Sweep *sweep_array, int pid, GtkWidget **apt, ChanSet *chanset)
{
	f_start(F_INIT);
	Timer *bench_timer _timerfree_ = timer_new();

	mcf_register(NULL, "# DAC Control", MCF_W);

	for (int id = 0; id < M2_MAX_CHAN; id++)
	{
		Sweep *sweep = &sweep_array[id];
		char *prefix = atg(supercat("panel%d_sweep%d_", pid, id));

		section_register(&sweep->sect, atg(supercat("panel%d_dac_control%d_", pid, id)), SECTION_RIGHT, apt);

		mcf_register(NULL, atg(cat2(prefix, "_chan")), MCF_INT);  // OBSOLETE VAR

		// section visibility

		int vis_sweep_var = mcf_register(&sweep->vis_sweep, atg(cat2(prefix, "show_sweep")), MCF_BOOL | MCF_W | MCF_DEFAULT, 0);
		int vis_jump_var  = mcf_register(&sweep->vis_jump,  atg(cat2(prefix, "show_jump")),  MCF_BOOL | MCF_W | MCF_DEFAULT, 1);

		mcf_connect(vis_sweep_var, "setup, panel", BLOB_CALLBACK(vismode_mcf), 0x20, sweep, sweep->vis_sweep_button);
		mcf_connect(vis_jump_var,  "setup, panel", BLOB_CALLBACK(vismode_mcf), 0x20, sweep, sweep->vis_jump_button);

		snazzy_connect(sweep->vis_sweep_button, "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(vismode_cb), 0x20, sweep, &sweep->vis_sweep);
		snazzy_connect(sweep->vis_jump_button,  "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(vismode_cb), 0x20, sweep, &sweep->vis_jump);

		// jump

		int jump_voltage_var = mcf_register(&sweep->jump_voltage, atg(cat2(prefix, "jump_voltage")), MCF_DOUBLE);
		int jump_scaled_var  = mcf_register(&sweep->jump_scaled,  atg(cat2(prefix, "jump_scaled")),  MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.0);

		mcf_connect(jump_voltage_var, "panel",        BLOB_CALLBACK(jump_target_mcf), 0x31, sweep, sweep->jump_voltage_entry, sweep->jump_scaled_entry,  COMPUTE_LINEAR_NONINVERSE);
		mcf_connect(jump_scaled_var,  "setup, panel", BLOB_CALLBACK(jump_target_mcf), 0x31, sweep, sweep->jump_scaled_entry,  sweep->jump_voltage_entry, COMPUTE_LINEAR_INVERSE);

		snazzy_connect(sweep->jump_voltage_entry->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(jump_target_cb), 0x31, sweep, sweep->jump_voltage_entry, sweep->jump_scaled_entry,  COMPUTE_LINEAR_NONINVERSE);
		snazzy_connect(sweep->jump_scaled_entry->widget,  "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(jump_target_cb), 0x31, sweep, sweep->jump_scaled_entry,  sweep->jump_voltage_entry, COMPUTE_LINEAR_INVERSE);
		snazzy_connect(sweep->jump_button,                "clicked",                          SNAZZY_VOID_VOID, BLOB_CALLBACK(jump_go_cb),     0x10, sweep);

		// sweep

		snazzy_connect(sweep->stop_button, "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(sweep_direction_cb), 0x11, sweep,  0);
		snazzy_connect(sweep->down_button, "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(sweep_direction_cb), 0x11, sweep, -1);
		snazzy_connect(sweep->up_button,   "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(sweep_direction_cb), 0x11, sweep,  1);

		for sides
		{
			int voltage_var  = mcf_register(&sweep->voltage.value[s],  atg(cat3(prefix, "voltage_",  s == LOWER ? "down" : "up")), MCF_DOUBLE);
			int scaled_var   = mcf_register(&sweep->scaled.value[s],   atg(cat3(prefix, "scaled_",   s == LOWER ? "down" : "up")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, s == LOWER ? -1.0 : 1.0);
			int step_var     = mcf_register(&sweep->step.value[s],     atg(cat3(prefix, "step_",     s == LOWER ? "down" : "up")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.0);
			int rate_var     = mcf_register(&sweep->rate.value[s],     atg(cat3(prefix, "rate_",     s == LOWER ? "down" : "up")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.1);
			int dwell_var    = mcf_register(&sweep->dwell.value[s],    atg(cat3(prefix, "dwell_",    s == LOWER ? "down" : "up")), MCF_DOUBLE);
			int blackout_var = mcf_register(&sweep->blackout.value[s], atg(cat3(prefix, "blackout_", s == LOWER ? "down" : "up")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.0);
			int hold_var     = mcf_register(&sweep->hold.value[s],     atg(cat3(prefix, "hold_",     s == LOWER ? "down" : "up")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.0);
			int endstop_var  = mcf_register(&sweep->endstop[s],        atg(cat3(prefix, "endstop_",  s == LOWER ? "down" : "up")), MCF_BOOL   | MCF_W | MCF_DEFAULT, 0);
			int zerostop_var = mcf_register(&sweep->zerostop[s],       atg(cat3(prefix, "zerostop_", s == LOWER ? "down" : "up")), MCF_BOOL   | MCF_W | MCF_DEFAULT, 0);

			mcf_connect(voltage_var,  "panel",        BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->voltage,  &sweep->scaled,  s, COMPUTE_LINEAR_NONINVERSE);
			mcf_connect(scaled_var,   "setup, panel", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->scaled,   &sweep->voltage, s, COMPUTE_LINEAR_INVERSE);
			mcf_connect(step_var,     "setup, panel", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->step,     &sweep->dwell,   s, STEP_TO_DWELL);
			mcf_connect(rate_var,     "setup, panel", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->rate,     &sweep->dwell,   s, STEP_TO_DWELL);
			mcf_connect(dwell_var,    "setup, panel", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->dwell,    &sweep->step,    s, DWELL_TO_STEP);
			mcf_connect(blackout_var, "setup, panel", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->blackout, NULL,            s, 0);
			mcf_connect(hold_var,     "setup, panel", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->hold,     NULL,            s, 0);
			mcf_connect(endstop_var,  "setup, panel", BLOB_CALLBACK(stopcondition_mcf), 0x20, sweep, sweep->endstop_button[s]);
			mcf_connect(zerostop_var, "setup, panel", BLOB_CALLBACK(stopcondition_mcf), 0x20, sweep, sweep->zerostop_button[s]);

			snazzy_connect(sweep->voltage.entry[s]->widget,  "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->voltage,  &sweep->scaled,  s, COMPUTE_LINEAR_NONINVERSE);
			snazzy_connect(sweep->scaled.entry[s]->widget,   "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->scaled,   &sweep->voltage, s, COMPUTE_LINEAR_INVERSE);
			snazzy_connect(sweep->step.entry[s]->widget,     "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->step,     &sweep->dwell,   s, STEP_TO_DWELL);
			snazzy_connect(sweep->rate.entry[s]->widget,     "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->rate,     &sweep->dwell,   s, STEP_TO_DWELL);
			snazzy_connect(sweep->dwell.entry[s]->widget,    "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->dwell,    &sweep->step,    s, DWELL_TO_STEP);
			snazzy_connect(sweep->blackout.entry[s]->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->blackout, NULL,            s, 0);
			snazzy_connect(sweep->hold.entry[s]->widget,     "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(endpoint_cb),      0x32, sweep, &sweep->hold,     NULL,            s, 0);
			snazzy_connect(sweep->endstop_button[s],         "button-release-event",             SNAZZY_BOOL_PTR, BLOB_CALLBACK(stopcondition_cb), 0x20, sweep, &sweep->endstop[s]);
			snazzy_connect(sweep->zerostop_button[s],        "button-release-event",             SNAZZY_BOOL_PTR, BLOB_CALLBACK(stopcondition_cb), 0x20, sweep, &sweep->zerostop[s]);
		}
	}

	control_server_connect(M2_TS_ID, "set_dac",          M2_CODE_DAQ << pid, BLOB_CALLBACK(set_dac_csf),         0x20, sweep_array, chanset);
	control_server_connect(M2_LS_ID, "set_dac",          M2_CODE_DAQ << pid, BLOB_CALLBACK(set_dac_csf),         0x20, sweep_array, chanset);
	control_server_connect(M2_TS_ID, "sweep_stop",       M2_CODE_DAQ << pid, BLOB_CALLBACK(sweep_direction_csf), 0x21, sweep_array, chanset,  0);
	control_server_connect(M2_LS_ID, "sweep_stop",       M2_CODE_DAQ << pid, BLOB_CALLBACK(sweep_direction_csf), 0x21, sweep_array, chanset,  0);
	control_server_connect(M2_TS_ID, "sweep_down",       M2_CODE_DAQ << pid, BLOB_CALLBACK(sweep_direction_csf), 0x21, sweep_array, chanset, -1);
	control_server_connect(M2_LS_ID, "sweep_down",       M2_CODE_DAQ << pid, BLOB_CALLBACK(sweep_direction_csf), 0x21, sweep_array, chanset, -1);
	control_server_connect(M2_TS_ID, "sweep_up",         M2_CODE_DAQ << pid, BLOB_CALLBACK(sweep_direction_csf), 0x21, sweep_array, chanset,  1);
	control_server_connect(M2_LS_ID, "sweep_up",         M2_CODE_DAQ << pid, BLOB_CALLBACK(sweep_direction_csf), 0x21, sweep_array, chanset,  1);
	control_server_connect(M2_TS_ID, "sweep_link_setup", M2_CODE_GUI << pid, BLOB_CALLBACK(link_setup_csf),      0x20, sweep_array, chanset);
	control_server_connect(M2_TS_ID, "sweep_link_clear", M2_CODE_GUI << pid, BLOB_CALLBACK(link_clear_csf),      0x10, sweep_array);

	f_print(F_BENCH, "Elapsed time: %f msec\n", timer_elapsed(bench_timer) * 1e3);
}

void sweep_register_legacy (Sweep *sweep, const char *prefix)
{
	f_start(F_INIT);

	mcf_register(NULL, atg(cat2(prefix, "_dac_chan")), MCF_INT);  // obsolete var

	for sides
	{
		int scaled_var   = mcf_register(&sweep->scaled.value[s], atg(cat3(prefix, "scaled_" ,  s == LOWER ? "down" : "up")), MCF_DOUBLE);
		int rate_var     = mcf_register(&sweep->rate.value[s],   atg(cat3(prefix, "rate_",     s == LOWER ? "down" : "up")), MCF_DOUBLE);
		int hold_var     = mcf_register(&sweep->hold.value[s],   atg(cat3(prefix, "hold_",     s == LOWER ? "down" : "up")), MCF_DOUBLE);
		int endstop_var  = mcf_register(&sweep->endstop[s],      atg(cat3(prefix, "endstop_",  s == LOWER ? "down" : "up")), MCF_BOOL);
		int zerostop_var = mcf_register(&sweep->zerostop[s],     atg(cat3(prefix, "zerostop_", s == LOWER ? "down" : "up")), MCF_BOOL);

		mcf_connect_with_note(scaled_var,   "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(endpoint_mcf),      0x32, NULL,  NULL,         NULL, 0, 0);
		mcf_connect_with_note(rate_var,     "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(endpoint_mcf),      0x32, NULL,  NULL,         NULL, 0, 0);
		mcf_connect_with_note(hold_var,     "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(endpoint_mcf),      0x32, sweep, &sweep->hold, NULL, s, 0);
		mcf_connect_with_note(endstop_var,  "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(stopcondition_mcf), 0x20, sweep, sweep->endstop_button[s]);
		mcf_connect_with_note(zerostop_var, "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(stopcondition_mcf), 0x20, sweep, sweep->zerostop_button[s]);
	}
}

void sweep_array_update (Sweep *sweep_array, ChanSet *chanset)
{
	f_start(F_UPDATE);

	for (int id = 0; id < M2_MAX_CHAN; id++)
	{
		Sweep *sweep = &sweep_array[id];

		int vci = (id < chanset->N_inv_chan) ? chanset->vci_by_ici[id] : -1;
		sweep->channel = (vci != -1) ? chanset->channel_by_vci[vci] : NULL;

		set_visibility(sweep->sect.full, sweep->channel != NULL);
		update_section_visibility(sweep);

		char *name_str    = atg(sweep->channel != NULL ? cat1(sweep->channel->desc)                            : cat1("—"));
		char *tooltip_str = atg(sweep->channel != NULL ? cat1(sweep->channel->desc_long)                       : cat1("None"));
		char *scaled_str  = atg(sweep->channel != NULL ? supercat("X<sub>%d</sub> ",  chanset->vc_by_vci[vci]) : cat1("X"));
		char *unit_str    = atg(sweep->channel != NULL ? cat1(sweep->channel->full_unit)                       : cat1(""));
		char *step_str    = atg(sweep->channel != NULL ? supercat("ΔX<sub>%d</sub> ", chanset->vc_by_vci[vci]) : cat1("ΔX"));
		char *rate_str    = atg(sweep->channel != NULL ? cat2(sweep->channel->full_unit, "/s")                 : cat1(""));

		// channel:
		gtk_entry_set_text(GTK_ENTRY(sweep->name_entry), name_str);
		gtk_widget_set_tooltip_markup(sweep->name_entry, tooltip_str);

		// swap scaled up/down if necessary:
		rectify_range(&sweep->scaled);

		// compute voltages, if possible:
		if (sweep->channel != NULL)
		{
			set_range(&sweep->voltage, LOWER, compute_linear_compute(&sweep->channel->cf, COMPUTE_LINEAR_INVERSE, sweep->scaled.value[LOWER]));
			set_range(&sweep->voltage, UPPER, compute_linear_compute(&sweep->channel->cf, COMPUTE_LINEAR_INVERSE, sweep->scaled.value[UPPER]));

			sweep->jump_voltage = compute_linear_compute(&sweep->channel->cf, COMPUTE_LINEAR_INVERSE, sweep->jump_scaled);
		}

		// compute dwell time:
		for sides
		{
			set_range(&sweep->step,  s, check_entry(sweep->step.entry[s], sweep->step.value[s]));
			set_range(&sweep->dwell, s, step_dwell_compute(sweep, s, STEP_TO_DWELL));
		}

		// set widgets:
		gtk_label_set_markup(GTK_LABEL(sweep->scaled.label),      scaled_str);
		gtk_label_set_markup(GTK_LABEL(sweep->jump_scaled_label), scaled_str);
		gtk_label_set_markup(GTK_LABEL(sweep->step.label),        step_str);

		bool show_dwell = sweep->step.value[LOWER] > 0 || sweep->step.value[UPPER] > 0;
		set_range_vis(&sweep->dwell,    show_dwell);
		set_range_vis(&sweep->blackout, show_dwell);

		for sides
		{
			set_entry_unit(sweep->scaled.entry[s], unit_str);
			set_entry_unit(sweep->step.entry[s],   unit_str);
			set_entry_unit(sweep->rate.entry[s],   rate_str);

			if (sweep->channel != NULL) write_entry(sweep->voltage.entry[s],  sweep->voltage.value[s]);
			/**/                        write_entry(sweep->scaled.entry[s],   sweep->scaled.value[s]);
			/**/                        write_entry(sweep->step.entry[s],     sweep->step.value[s]);
			/**/                        write_entry(sweep->rate.entry[s],     sweep->rate.value[s]);
			/**/                        write_entry(sweep->hold.entry[s],     sweep->hold.value[s]);
			/**/                        write_entry(sweep->dwell.entry[s],    sweep->dwell.value[s]);
			/**/                        write_entry(sweep->blackout.entry[s], sweep->blackout.value[s]);
		}

		set_entry_unit(sweep->jump_scaled_entry, unit_str);

		if (sweep->channel != NULL) write_entry(sweep->jump_voltage_entry, sweep->jump_voltage);
		/**/                        write_entry(sweep->jump_scaled_entry,  sweep->jump_scaled);
	}
}

void sweep_array_final (Sweep *sweep_array)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_MAX_CHAN; id++)
	{
		for sides
		{
			destroy_entry(sweep_array[id].voltage.entry[s]);
			destroy_entry(sweep_array[id].scaled.entry[s]);
			destroy_entry(sweep_array[id].step.entry[s]);
			destroy_entry(sweep_array[id].rate.entry[s]);
			destroy_entry(sweep_array[id].dwell.entry[s]);
			destroy_entry(sweep_array[id].blackout.entry[s]);
			destroy_entry(sweep_array[id].hold.entry[s]);
		}
		destroy_entry(sweep_array[id].jump_voltage_entry);
		destroy_entry(sweep_array[id].jump_scaled_entry);
	}
}

void request_sweep_dir (Sweep *sweep, int dir, bool hold)
{
	if (sweep->channel == NULL) return;  // extra check

	f_start(F_RUN);

	if (!sweep->pending)
	{
		sweep->pending = 1;

		sweep->request_dir = dir;
		sweep->request_hold = hold;

		f_print(F_RUN, "Set sweep \"%s\" direction request to %d%s\n", sweep->channel->desc, dir, hold ? " (holding)" : "");

		if (sweep->follower != NULL) request_sweep_dir(sweep->follower, hold ? 0 : dir, 0);  // Setting pending = 1 above will stop circular leader/follower arrangements.
		                                                                                     // Also, don't propagate hold parameter, and set dir = 0 if leader plans to hold.
	}
	else f_print(F_RUN, "Doing nothing... (request already pending)\n");
}

void exec_sweep_dir (Sweep *sweep)
{
	if (sweep->channel == NULL) return;  // extra check

	if (sweep->pending)
	{
		sweep->pending = 0;

		if (sweep->dir != sweep->request_dir) sweep->dir_dirty = 1;  // tell GUI thread to update {stop, down, up} buttons

		sweep->dir     = sweep->request_dir;
		sweep->holding = sweep->request_hold;

		if (sweep->follower != NULL) exec_sweep_dir(sweep->follower);  // setting pending = 0 above will stop circular leader/follower arrangements
	}
}

void set_sweep_scanning_all (Sweep *sweep_array, bool scanning)
{
	for (int id = 0; id < M2_MAX_CHAN; id++)
	{
		gtk_widget_set_sensitive(sweep_array[id].stop_button, !scanning);
		gtk_widget_set_sensitive(sweep_array[id].down_button, !scanning);
		gtk_widget_set_sensitive(sweep_array[id].up_button,   !scanning);
		gtk_widget_set_sensitive(sweep_array[id].jump_button, !scanning);
	}
}

bool set_sweep_buttons_all (Sweep *sweep_array, int N_inv_chan)
{
	int  dir       [M2_MAX_CHAN];
	bool dir_dirty [M2_MAX_CHAN];

	// copy state from DAQ thread:
	mt_mutex_lock(sweep_array[0].mutex);
	for (int n = 0; n < N_inv_chan; n++)
	{
		dir[n]       = sweep_array[n].dir;
		dir_dirty[n] = sweep_array[n].dir_dirty;
	}
	mt_mutex_unlock(sweep_array[0].mutex);

	bool events = 0;
	for (int n = 0; n < N_inv_chan; n++)
	{
		if (dir_dirty[n])
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sweep_array[n].stop_button), dir[n] ==  0);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sweep_array[n].down_button), dir[n] == -1);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sweep_array[n].up_button),   dir[n] ==  1);
			events = 1;
		}
	}

	return events;
}

double step_dwell_compute (Sweep *sweep, int side, int mode)
{
	f_start(F_VERBOSE);

	double rate = sweep->rate.value[side];
	return (mode == DWELL_TO_STEP)             ? sweep->dwell.value[side] * rate / 1e3 :
	       (mode == STEP_TO_DWELL && rate > 0) ? sweep->step.value[side]  / rate * 1e3 : 0.0;
}

void attach_range (NumericRange *nr, int row, const char *label_str, const char *unit_str, GtkWidget *table)
{
	nr->label = table_attach(new_label(label_str, 1, 0.0), 0, row, table);

	for sides
	{
		nr->entry[s] = new_numeric_entry(1, 6, 8);
		set_entry_unit(nr->entry[s], unit_str);
		table_attach(nr->entry[s]->widget, s == LOWER ? 1 : 2, row, table);
	}
}

GtkWidget * make_sweep_section (Sweep *sweep)
{
	f_start(F_VERBOSE);

	GtkWidget *table = set_margins(new_table(0, 0), 2, 2, 8, 4);

	// buttons:
	sweep->stop_button = table_attach(gtk_toggle_button_new_with_label("STOP"), 0, 0, table);
	sweep->down_button = table_attach(gtk_toggle_button_new_with_label("DOWN"), 1, 0, table);
	sweep->up_button   = table_attach(gtk_toggle_button_new_with_label("UP"),   2, 0, table);

	// ranges:
	attach_range(&sweep->voltage,  1, "V<sub>DAC</sub>",   "V",   table);
	attach_range(&sweep->scaled,   2, "?",                 "?",   table);
	attach_range(&sweep->step,     3, "?",                 "?",   table);
	attach_range(&sweep->rate,     4, "r<sub>sweep</sub>", "?/s", table);
	attach_range(&sweep->dwell,    5, "t<sub>dwell</sub>", "ms",  table);
	attach_range(&sweep->blackout, 6, "t<sub>black</sub>", "%",   table);
	attach_range(&sweep->hold,     7, "t<sub>hold</sub>",  "s",   table);

	for sides
	{
		// preset range: 0 ... inf
		set_entry_min(sweep->step.entry[s],     0);
		set_entry_min(sweep->rate.entry[s],     0);
		set_entry_min(sweep->dwell.entry[s],    0);
		set_entry_min(sweep->hold.entry[s],     0);

		// preset range: 0 .. 100
		set_entry_min(sweep->blackout.entry[s], 0);
		set_entry_max(sweep->blackout.entry[s], 100);

		// prevent limits based on other half of ranges
		sweep->step.entry[s]->want_min     = sweep->step.entry[s]->want_max     = 0;
		sweep->rate.entry[s]->want_min     = sweep->rate.entry[s]->want_max     = 0;
		sweep->dwell.entry[s]->want_min    = sweep->dwell.entry[s]->want_max    = 0;
		sweep->blackout.entry[s]->want_min = sweep->blackout.entry[s]->want_max = 0;
		sweep->hold.entry[s]->want_min     = sweep->hold.entry[s]->want_max     = 0;
	}

	// stop conditions:
	GtkWidget *down_hbox = table_attach(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), 1, 8, table);
	GtkWidget *up_hbox   = table_attach(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), 2, 8, table);

	sweep->endstop_button[LOWER]  = pack_start (flatten_button(size_widget(gtk_toggle_button_new(), -1, 22)), 0, down_hbox);
	sweep->zerostop_button[UPPER] = pack_end   (flatten_button(size_widget(gtk_toggle_button_new(), -1, 22)), 0, down_hbox);
	sweep->zerostop_button[LOWER] = pack_start (flatten_button(size_widget(gtk_toggle_button_new(), -1, 22)), 0, up_hbox);
	sweep->endstop_button[UPPER]  = pack_end   (flatten_button(size_widget(gtk_toggle_button_new(), -1, 22)), 0, up_hbox);

	gtk_widget_set_tooltip_text(sweep->endstop_button[LOWER],  "Stop at lower bound");
	gtk_widget_set_tooltip_text(sweep->zerostop_button[UPPER], "Stop at zero when sweeping up");
	gtk_widget_set_tooltip_text(sweep->zerostop_button[LOWER], "Stop at zero when sweeping down");
	gtk_widget_set_tooltip_text(sweep->endstop_button[UPPER],  "Stop at upper bound");

	container_add(new_label("|←", 0, 0.0), sweep->endstop_button[LOWER]);
	container_add(new_label("→0", 0, 1.0), sweep->zerostop_button[UPPER]);
	container_add(new_label("0←", 0, 0.0), sweep->zerostop_button[LOWER]);
	container_add(new_label("→|", 0, 1.0), sweep->endstop_button[UPPER]);

	return table;
}

GtkWidget * make_jump_section (Sweep *sweep)
{
	f_start(F_VERBOSE);

	GtkWidget *hbox = set_margins(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), 2, 2, 14, 4);

	GtkWidget *table = pack_start(new_table(0, 2),                          0, hbox);
	GtkWidget *vbox  = pack_start(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), 0, hbox);

	sweep->jump_voltage_entry = new_numeric_entry(1, 6, 8);
	sweep->jump_scaled_entry  = new_numeric_entry(1, 6, 8);

	set_entry_unit(sweep->jump_voltage_entry, "V");

	/**/                       table_attach(new_label("V<sub>DAC</sub> ", 1, 0.0), 0, 0, table);
	sweep->jump_scaled_label = table_attach(new_label("?",                1, 0.0), 0, 1, table);
	/**/                       table_attach(sweep->jump_voltage_entry->widget,     1, 0, table);
	/**/                       table_attach(sweep->jump_scaled_entry->widget,      1, 1, table);

	sweep->jump_button = gtk_button_new_with_label("SET");
	gtk_box_set_center_widget(GTK_BOX(vbox), sweep->jump_button);

	return hbox;
}
