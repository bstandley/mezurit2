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

#include "trigger.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <control/server.h>

enum { TRIGGER_IF = 0 };

static void trigger_array_update_visibility (Trigger *trigger_array, MtMutex *mutex);
static void update_line_array_vis (Trigger *trigger, MtMutex *mutex, int plus);
static void start_trigger (Trigger *trigger);

#include "trigger_callback.c"

void trigger_array_init (Trigger *trigger_array, GtkWidget **apt, Section *sect)
{
	f_start(F_INIT);

	section_init(sect, atg(sharepath("pixmaps/tool_trigger.png")), "Trigger", apt);
	add_loc_menu(sect, SECTION_WAYLEFT, SECTION_LEFT, SECTION_TOP, SECTION_BOTTOM, SECTION_RIGHT, SECTION_WAYRIGHT, -1);
	add_rollup_button(sect);
	add_orient_button(sect);

	for (int id = 0; id < M2_MAX_TRIG; id++)
	{
		Trigger *trigger = &trigger_array[id];

		trigger->vbox = pack_start(set_no_show_all(new_box(GTK_ORIENTATION_VERTICAL, 4)), 0, sect->box);

		GtkWidget *hbox0 = pack_start(new_box(GTK_ORIENTATION_HORIZONTAL, 4), 0, trigger->vbox);  // "if" section
		trigger->hbox1   = pack_start(new_box(GTK_ORIENTATION_HORIZONTAL, 4), 0, trigger->vbox);  // "then" section

		/**/                     pack_start(new_label("IF:", 0.5),                   0, hbox0);
		trigger->line_entry[0] = pack_start(new_entry(0, 12),                        1, hbox0);
		trigger->arm_button    = pack_start(gtk_toggle_button_new_with_label("ARM"), 0, hbox0);

		GtkWidget *vbox10 = pack_start(new_box(GTK_ORIENTATION_VERTICAL, 0), 0, trigger->hbox1);
		GtkWidget *vbox11 = pack_start(new_box(GTK_ORIENTATION_VERTICAL, 0), 1, trigger->hbox1);

		trigger->force_button = pack_start(gtk_toggle_button_new(), 0, vbox10);
		GtkWidget *image = container_add(gtk_image_new_from_pixbuf(lookup_pixbuf(PIXBUF_ICON_ACTION)), trigger->force_button);
		set_draw_on_expose(sect->full, image);

		for (int l = 0; l < M2_TRIGGER_LINES; l++)
		{
			if (l > 0)
			{
				GtkWidget *line_hbox = pack_start(new_box(GTK_ORIENTATION_HORIZONTAL, 0), 0, vbox11);
				if (l > 1) set_no_show_all(line_hbox);

				/**/        trigger->line_entry[l] = pack_start(new_entry(0, 10),               1, line_hbox);
				if (l == 1) trigger->plus_button   = pack_end  (gtk_button_new_with_label("+"), 0, line_hbox);
			}

			trigger->line_expr[l] = NULL;
			trigger->line_dirty[l] = 0;
			trigger->line_cf[l].py_f = NULL;
			trigger->line_cf[l].info = NULL;
		}

		trigger->adjusted = 0;

		trigger->ready = trigger->armed = trigger->busy = 0;
		trigger->any_line_dirty = trigger->arm_dirty = trigger->busy_dirty = 0;
		trigger->timer = timer_new();
	}
}

void trigger_array_final (Trigger *trigger_array)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_MAX_TRIG; id++)
	{
		timer_destroy(trigger_array[id].timer);

		for (int l = 0; l < M2_TRIGGER_LINES; l++)
		{
			replace(trigger_array[id].line_expr[l], NULL);
			replace(trigger_array[id].line_cf[l].info, NULL);
		}
	}
}

void trigger_array_register (Trigger *trigger_array, int pid, GtkWidget **apt, Section *sect, MtMutex *mutex)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Triggers", MCF_W);
	section_register(sect, atg(supercat("panel%d_trigger_", pid)), SECTION_LEFT, apt);

	for (int id = 0; id < M2_MAX_TRIG; id++)
	{
		Trigger *trigger = &trigger_array[id];
		char *prefix = atg(supercat("panel%d_trigger%d_", pid, id));

		for (int l = 0; l < M2_TRIGGER_LINES; l++)
		{
			char *suffix = atg(l == TRIGGER_IF ? cat1("if") : supercat("line%d", l));
			int var = mcf_register(&trigger->line_expr[l], atg(cat2(prefix, suffix)), MCF_STRING | MCF_W | MCF_DEFAULT, "");

			mcf_connect(var, "setup, panel", BLOB_CALLBACK(trigger_expr_mcf), 0x22, trigger_array, mutex, id, l);

			/**/        snazzy_connect(trigger->line_entry[l], "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(trigger_expr_cb),   0x22, trigger_array, mutex, id, l);
			if (l == 1) snazzy_connect(trigger->line_entry[l], "draw",                             SNAZZY_BOOL_PTR, BLOB_CALLBACK(trigger_expose_cb), 0x10, trigger);
		}

		snazzy_connect(trigger->arm_button,   "button-release-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(trigger_button_cb), 0x20, trigger, mutex);
		snazzy_connect(trigger->force_button, "button-release-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(trigger_button_cb), 0x20, trigger, mutex);
		snazzy_connect(trigger->plus_button,  "clicked",              SNAZZY_VOID_VOID, BLOB_CALLBACK(trigger_plus_cb),   0x20, trigger, mutex);
	}

	control_server_connect(M2_TS_ID, "arm_trigger",    M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, mutex);  // lock inside callback
	control_server_connect(M2_LS_ID, "arm_trigger",    M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, NULL);   // no locking (already locked)
	control_server_connect(M2_TS_ID, "disarm_trigger", M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, mutex);
	control_server_connect(M2_LS_ID, "disarm_trigger", M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, NULL);
	control_server_connect(M2_TS_ID, "force_trigger",  M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, mutex);
	control_server_connect(M2_LS_ID, "force_trigger",  M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, NULL);
	control_server_connect(M2_TS_ID, "cancel_trigger", M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, mutex);
	control_server_connect(M2_LS_ID, "cancel_trigger", M2_CODE_DAQ << pid, BLOB_CALLBACK(trigger_csf), 0x21, trigger_array, NULL);
}

void trigger_array_update (Trigger *trigger_array)
{
	f_start(F_UPDATE);

	for (int id = 0; id < M2_MAX_TRIG; id++)
	{
		trigger_array[id].cur = 1;
		trigger_array[id].wait_time = -1.0;

		update_line_array_vis(&trigger_array[id], NULL, 0);
	}

	trigger_array_update_visibility(trigger_array, NULL);
}

void update_line_array_vis (Trigger *trigger, MtMutex *mutex, int plus)
{
	f_start(F_VERBOSE);

	bool filled[M2_TRIGGER_LINES];
	if (mutex != NULL) mt_mutex_lock(mutex);
	for (int l = 0; l < M2_TRIGGER_LINES; l++) filled[l] = (str_length(trigger->line_expr[l]) > 0);
	if (mutex != NULL) mt_mutex_unlock(mutex);

	set_visibility(trigger->hbox1,                filled[0]);
	gtk_widget_set_sensitive(trigger->arm_button, filled[0]);

	if (filled[0])
	{
		int l_show = 1 + plus;
		for (int l = 2; l < M2_TRIGGER_LINES - plus; l++) if (filled[l]) l_show = l + plus;

		for (int l = 2; l < M2_TRIGGER_LINES; l++) set_visibility(gtk_widget_get_parent(trigger->line_entry[l]), l <= l_show);
		gtk_widget_set_sensitive(trigger->plus_button, filled[l_show] && l_show + 1 < M2_TRIGGER_LINES);

		pack_end(trigger->plus_button, 0, gtk_widget_get_parent(trigger->line_entry[l_show]));

		if (plus) gtk_widget_grab_focus(trigger->line_entry[l_show]);
	}
}

void trigger_array_update_visibility (Trigger *trigger_array, MtMutex *mutex)
{
	f_start(F_VERBOSE);

	if (mutex != NULL) mt_mutex_lock(mutex);
	int id_show = 0;  // index of last trigger that should be visible (one greater than index of last occupied trigger, if possible)
	for (int id = 1; id < M2_MAX_TRIG; id++) if (str_length(trigger_array[id - 1].line_expr[TRIGGER_IF]) > 0) id_show = id;
	if (mutex != NULL) mt_mutex_unlock(mutex);

	for (int id = 0; id < M2_MAX_TRIG; id++) set_visibility(trigger_array[id].vbox, id <= id_show);
}

bool set_trigger_buttons_all (Trigger *trigger_array, MtMutex *mutex)
{
	bool armed     [M2_MAX_TRIG], busy       [M2_MAX_TRIG];
	bool arm_dirty [M2_MAX_TRIG], busy_dirty [M2_MAX_TRIG];

	// copy state from DAQ thread:
	mt_mutex_lock(mutex);
	for (int n = 0; n < M2_MAX_TRIG; n++)
	{
		armed[n]      = trigger_array[n].armed;
		arm_dirty[n]  = trigger_array[n].arm_dirty;
		busy[n]       = trigger_array[n].busy;
		busy_dirty[n] = trigger_array[n].busy_dirty;
	}
	mt_mutex_unlock(mutex);

	bool events = 0;
	for (int n = 0; n < M2_MAX_TRIG; n++)
	{
		if (arm_dirty[n])
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trigger_array[n].arm_button), armed[n]);  // may "toggle" but won't "release" so it won't trigger a callback
			events = 1;
		}

		if (busy_dirty[n])
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trigger_array[n].force_button), busy[n]);
			events = 1;
		}
	}

	return events;
}

void start_trigger (Trigger *trigger)
{
	f_start(F_RUN);

	if (trigger->ready)
	{
		if (trigger->armed)
		{
			trigger->armed = 0;
			trigger->arm_dirty = 1;
		}

		trigger->cur = 1;
		trigger->wait_time = -1.0;

		trigger->busy = 1;
		trigger->busy_dirty = 1;
	}
}

void trigger_parse (Trigger *trigger)
{
	f_start(F_RUN);

	for (int l = 0; l < M2_TRIGGER_LINES; l++)
	{
		if (trigger->line_dirty[l])
		{
			compute_read_expr(&trigger->line_cf[l], trigger->line_expr[l], 1.0);

			if (trigger->line_cf[l].parse_exec && l == TRIGGER_IF)
			{
				status_add(0, cat1("Trigger IF statement should not contain trigger instructions.\n"));

				if (trigger->armed)
				{
					trigger->armed = 0;
					trigger->arm_dirty = 1;
				}
				trigger->ready = 0;
			}
			else trigger->ready = 1;

			trigger->line_dirty[l] = 0;
		}
	}

	trigger->any_line_dirty = 0;
}

void trigger_check (Trigger *trigger)
{
	if (trigger->ready)  // extra check
	{
		bool rv = 0;
		compute_function_test(&trigger->line_cf[TRIGGER_IF], COMPUTE_MODE_POINT, &rv);

		if (rv) start_trigger(trigger);
	}
}

void trigger_exec (Trigger *trigger)
{
	if (trigger->wait_time > 0)
	{
		if (timer_elapsed(trigger->timer) < trigger->wait_time) return;
		else trigger->wait_time = -1.0;
	}

	f_start(F_RUN);

	while (trigger->cur < M2_TRIGGER_LINES)
	{
		bool rv;
		compute_set_wait(-1.0);
		compute_function_test(&trigger->line_cf[trigger->cur], COMPUTE_MODE_POINT, &rv);
		trigger->wait_time = compute_get_wait();
		if (!rv) break;

		trigger->cur++;

		if (trigger->wait_time > 0)
		{
			timer_reset(trigger->timer);
			return;  // i.e. still busy
		}
	}

	trigger->busy = 0;
	trigger->busy_dirty = 1;
}
