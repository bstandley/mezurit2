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

static void trigger_plus_cb (GtkWidget *widget, Trigger *trigger, MtMutex *mutex);
static gboolean trigger_expose_cb (GtkWidget *widget, GdkEvent *event, Trigger *trigger);
static gboolean trigger_button_cb (GtkToggleButton *button, GdkEvent *event, Trigger *trigger, MtMutex *mutex);
static gboolean trigger_expr_cb (GtkWidget *widget, GdkEvent *event, Trigger *trigger_array, MtMutex *mutex, int i, int l);
static void trigger_expr_mcf (void *ptr, const char *signal_name, MValue value, Trigger *trigger_array, MtMutex *mutex, int i, int l);
static char * trigger_csf (gchar **argv, Trigger *trigger_array, MtMutex *mutex);

void trigger_plus_cb (GtkWidget *widget, Trigger *trigger, MtMutex *mutex)
{
	f_start(F_CALLBACK);

	update_line_array_vis(trigger, mutex, 1);
}

gboolean trigger_expose_cb (GtkWidget *widget, GdkEvent *event, Trigger *trigger)
{
	if (trigger->adjusted) return 0;
	f_start(F_CALLBACK);

	GtkAllocation alloc;
	gtk_widget_get_allocation(widget, &alloc);
	gtk_widget_set_size_request(trigger->plus_button, -1, alloc.height - 6);

	trigger->adjusted = 1;
	return 0;
}

gboolean trigger_button_cb (GtkToggleButton *button, GdkEvent *event, Trigger *trigger, MtMutex *mutex)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_toggle_button_get_active(button);
	bool arm = (GTK_WIDGET(button) == trigger->arm_button);

	mt_mutex_lock(mutex);
	if (arm)
	{
		trigger->armed = !was_active;
		trigger->arm_dirty = 1;
	}
	else
	{
		if (!was_active) start_trigger(trigger);
		else
		{
			trigger->busy = 0;
			trigger->busy_dirty = 1;
		}
	}
	mt_mutex_unlock(mutex);

	return 0;
}

gboolean trigger_expr_cb (GtkWidget *widget, GdkEvent *event, Trigger *trigger_array, MtMutex *mutex, int i, int l)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		Trigger *trigger = &trigger_array[i];
		char *new_expr = cat1(gtk_entry_get_text(GTK_ENTRY(widget)));

		mt_mutex_lock(mutex);
		replace(trigger->line_expr[l], new_expr);
		trigger->line_dirty[l] = 1;
		trigger->any_line_dirty = 1;
		mt_mutex_unlock(mutex);

		update_line_array_vis(trigger, mutex, 0);
		trigger_array_update_visibility(trigger_array, mutex);
	}

	return 0;
}

void trigger_expr_mcf (void *ptr, const char *signal_name, MValue value, Trigger *trigger_array, MtMutex *mutex, int i, int l)
{
	f_start(F_MCF);

	Trigger *trigger = &trigger_array[i];
	char *new_expr = cat1(value.string);

	mt_mutex_lock(mutex);
	replace(trigger->line_expr[l], new_expr);
	trigger->line_dirty[l] = 1;
	trigger->any_line_dirty = 1;
	mt_mutex_unlock(mutex);

	gtk_entry_set_text(GTK_ENTRY(trigger->line_entry[l]), value.string);
	if (str_equal(signal_name, "panel"))
	{
		update_line_array_vis(trigger, mutex, 0);
		trigger_array_update_visibility(trigger_array, mutex);
	}
}

char * trigger_csf (gchar **argv, Trigger *trigger_array, MtMutex *mutex)
{
	f_start(F_CONTROL);

	int id;  // trigger index
	if (scan_arg_int(argv[1], "id", &id) && id >= 0 && id < M2_MAX_TRIG)
	{
		Trigger *trigger = &trigger_array[id];

		if (mutex != NULL) mt_mutex_lock(mutex);
		if      (str_equal(argv[0], "arm_trigger")    && !trigger->armed) { trigger->armed = 1; trigger->arm_dirty = 1; }
		else if (str_equal(argv[0], "disarm_trigger") &&  trigger->armed) { trigger->armed = 0; trigger->arm_dirty = 1; }
		else if (str_equal(argv[0], "force_trigger")  && !trigger->busy)  { start_trigger(trigger); }
		else if (str_equal(argv[0], "cancel_trigger") &&  trigger->busy)  { trigger->busy = 0; trigger->busy_dirty = 1; }
		if (mutex != NULL) mt_mutex_unlock(mutex);

		return cat1(argv[0]);
	}
	else return cat1("argument_error");
}
