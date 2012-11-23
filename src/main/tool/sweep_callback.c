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

static gboolean sweep_direction_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, int dir);
static void jump_go_cb (GtkWidget *widget, Sweep *sweep);
static gboolean vismode_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, bool *vis);
static gboolean endpoint_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, NumericRange *master_nr, NumericRange *slave_nr, int side, int mode);
static gboolean stopcondition_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, bool *stop);
static gboolean jump_target_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, NumericEntry *master_ne, NumericEntry *slave_ne, int mode);

static char * set_dac_csf (gchar **argv, Sweep *sweep_array, ChanSet *chanset);
static char * sweep_direction_csf (gchar **argv, Sweep *sweep_array, ChanSet *chanset, int dir);
static char * link_setup_csf (gchar **argv, Sweep *sweep_array, ChanSet *chanset);
static char * link_clear_csf (gchar **argv, Sweep *sweep_array);

static void vismode_mcf (bool *vis, const char *signal_name, MValue value, Sweep *sweep, GtkWidget *widget);
static void endpoint_mcf (double *ptr, const char *signal_name, MValue value, Sweep *sweep, NumericRange *master_nr, NumericRange *slave_nr, int side, int mode);
static void stopcondition_mcf (bool *stop, const char *signal_name, MValue value, Sweep *sweep, GtkWidget *widget);
static void jump_target_mcf (double *master_ptr, const char *signal_name, MValue value, Sweep *sweep, NumericEntry *master_ne, NumericEntry *slave_ne, int mode);

static void set_sweep_endpoint (Sweep *sweep, NumericRange *master_nr, NumericRange *slave_nr, int side, int mode, double master);
static void set_jump_target    (Sweep *sweep, NumericEntry *master_ne, NumericEntry *slave_ne, int mode,           double master);

void update_section_visibility (Sweep *sweep)
{
	f_start(F_VERBOSE);

	set_visibility(sweep->sweep_section, sweep->vis_sweep);
	set_visibility(sweep->hsep,          sweep->vis_sweep && sweep->vis_jump);
	set_visibility(sweep->jump_section,  sweep->vis_jump);

	set_visibility(gtk_widget_get_parent(sweep->sect.box), sweep->channel != NULL && (sweep->vis_sweep || sweep->vis_jump));
}

gboolean vismode_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, bool *vis)
{
	f_start(F_CALLBACK);

	*vis = !(*vis);
	update_section_visibility(sweep);

	return 0;
}

void vismode_mcf (bool *vis, const char *signal_name, MValue value, Sweep *sweep, GtkWidget *widget)
{
	f_start(F_MCF);

	*vis = value.x_bool;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), value.x_bool);

	if (str_equal(signal_name, "panel")) update_section_visibility(sweep);
}

gboolean sweep_direction_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, int dir)
{
	f_start(F_CALLBACK);

	g_static_mutex_lock(sweep->mutex);
	bool skip = (dir == sweep->dir) || sweep->scanning;
	if (!skip)
	{
		request_sweep_dir(sweep, dir, 0);
		exec_sweep_dir(sweep);
	}
	g_static_mutex_unlock(sweep->mutex);

	return skip;
}

char * sweep_direction_csf (gchar **argv, Sweep *sweep_array, ChanSet *chanset, int dir)
{
	f_start(F_CONTROL);

	int vc;
	if (scan_arg_int(argv[1], "channel", &vc) && vc >= 0 && vc < M2_MAX_CHAN)
	{
		int ici = chanset->ici_by_vc[vc];
		if (ici != -1)
		{
			Sweep *sweep = &sweep_array[ici];
			bool warn = 0;

			g_static_mutex_lock(sweep->mutex);
			if (!sweep->scanning)
			{
				request_sweep_dir(sweep, dir, 0);
				exec_sweep_dir(sweep);
			}
			else warn = 1;
			g_static_mutex_unlock(sweep->mutex);

			if (warn) status_add(1, cat1("Sweeping is disabled when scope is running.\n"));
			return cat1(argv[0]);
		}
	}

	return cat1("argument_error");
}

gboolean endpoint_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, NumericRange *master_nr, NumericRange *slave_nr, int side, int mode)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		bool ok;
		double master = read_entry(master_nr->entry[side], &ok);
		if (ok) set_sweep_endpoint(sweep, master_nr, slave_nr, side, mode, master);
		else    write_entry(master_nr->entry[side], master_nr->value[side]);
	}

	return 0;
}

void endpoint_mcf (double *ptr, const char *signal_name, MValue value, Sweep *sweep, NumericRange *master_nr, NumericRange *slave_nr, int side, int mode)
{
	f_start(F_MCF);

	if      (str_equal(signal_name, "setup")) set_range(master_nr, side, value.x_double);  // verified later by sweep_update()
	else if (str_equal(signal_name, "panel"))
	{
		double master = check_entry(master_nr->entry[side], value.x_double);
		set_sweep_endpoint(sweep, master_nr, slave_nr, side, mode, master);
	}
}

void set_sweep_endpoint (Sweep *sweep, NumericRange *master_nr, NumericRange *slave_nr, int side, int mode, double master)
{
	bool enslave = (slave_nr != NULL && sweep->channel != NULL);

	g_static_mutex_lock(sweep->mutex);
	/**/         set_range(master_nr, side, master);
	if (enslave) set_range(slave_nr,  side, (mode == STEP_TO_DWELL || mode == DWELL_TO_STEP) ? step_dwell_compute(sweep, side, mode) :
	                                                                                           compute_linear_compute(&sweep->channel->cf, mode, master));
	if (mode == STEP_TO_DWELL || mode == DWELL_TO_STEP)
	{
		bool show_dwell = sweep->step.value[LOWER] > 0 || sweep->step.value[UPPER] > 0;
		g_static_mutex_unlock(sweep->mutex);

		set_range_vis(&sweep->dwell,    show_dwell);
		set_range_vis(&sweep->blackout, show_dwell);
	}
	else g_static_mutex_unlock(sweep->mutex);

	/**/         write_entry(master_nr->entry[side], master_nr->value[side]);
	if (enslave) write_entry(slave_nr->entry[side],  slave_nr->value[side]);
}

gboolean jump_target_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, NumericEntry *master_ne, NumericEntry *slave_ne, int mode)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		bool ok;
		double master = read_entry(master_ne, &ok);
		if (ok) set_jump_target(sweep, master_ne, slave_ne, mode, master);
		else    write_entry(master_ne, (master_ne == sweep->jump_voltage_entry) ? sweep->jump_voltage : sweep->jump_scaled);
	}

	return 0;
}

void jump_target_mcf (double *master_ptr, const char *signal_name, MValue value, Sweep *sweep, NumericEntry *master_ne, NumericEntry *slave_ne, int mode)
{
	f_start(F_MCF);

	if      (str_equal(signal_name, "setup")) *master_ptr = value.x_double;  // verified later by sweep_update()
	else if (str_equal(signal_name, "panel"))
	{
		double master = check_entry(master_ne, value.x_double);
		set_jump_target(sweep, master_ne, slave_ne, mode, master);
	}
}

void set_jump_target (Sweep *sweep, NumericEntry *master_ne, NumericEntry *slave_ne, int mode, double master)
{
	double *master_ptr = (master_ne == sweep->jump_voltage_entry) ? &sweep->jump_voltage : &sweep->jump_scaled;
	double *slave_ptr  = (slave_ne == sweep->jump_voltage_entry)  ? &sweep->jump_voltage : &sweep->jump_scaled;

	bool enslave = (sweep->channel != NULL);  // extra check, should always be true

	g_static_mutex_lock(sweep->mutex);
	/**/         *master_ptr = master;
	if (enslave) *slave_ptr  = compute_linear_compute(&sweep->channel->cf, mode, master);
	g_static_mutex_unlock(sweep->mutex);

	/**/         write_entry(master_ne, *master_ptr);
	if (enslave) write_entry(slave_ne,  *slave_ptr);
}

gboolean stopcondition_cb (GtkWidget *widget, GdkEvent *event, Sweep *sweep, bool *stop)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	g_static_mutex_lock(sweep->mutex);
	*stop = !was_active;
	g_static_mutex_unlock(sweep->mutex);

	return 0;
}

void stopcondition_mcf (bool *stop, const char *signal_name, MValue value, Sweep *sweep, GtkWidget *widget)
{
	f_start(F_MCF);

	g_static_mutex_lock(sweep->mutex);
	*stop = value.x_bool;
	g_static_mutex_unlock(sweep->mutex);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), value.x_bool);
}

void jump_go_cb (GtkWidget *widget, Sweep *sweep)
{
	f_start(F_CALLBACK);

	if (sweep->channel != NULL)  // extra check
	{
		g_static_mutex_lock(sweep->mutex);
		if (!sweep->scanning)
		{
			request_sweep_dir(sweep, 0, 0);
			exec_sweep_dir(sweep);
			sweep->jump_dirty = 1;
		}
		g_static_mutex_unlock(sweep->mutex);
	}
}

char * set_dac_csf (gchar **argv, Sweep *sweep_array, ChanSet *chanset)
{
	f_start(F_CONTROL);

	int vc;
	double target;
	if (scan_arg_int    (argv[1], "channel", &vc) && vc >= 0 && vc < M2_MAX_CHAN &&
	    scan_arg_double (argv[2], "target",  &target))
	{
		int vci = chanset->vci_by_vc[vc];
		if (vci != -1)
		{
			// Note: This callback is connected with M2_CODE_DAQ so it is OK to call compute_function_write(), even though it uses Python.
			bool success = compute_function_write(&chanset->channel_by_vci[vci]->cf, target);
			int ici = chanset->ici_by_vc[vc];

			if (success && ici != -1)
			{
				request_sweep_dir(&sweep_array[ici], 0, 0);
				exec_sweep_dir(&sweep_array[ici]);
			}

			return supercat("%s;success|%d", argv[0], success ? 1 : 0);
		}
	}

	return cat1("argument_error");
}

char * link_clear_csf (gchar **argv, Sweep *sweep_array)
{
	f_start(F_CONTROL);

	g_static_mutex_lock(sweep_array[0].mutex);
	for (int ici = 0; ici < M2_MAX_CHAN; ici++) sweep_array[ici].follower = NULL;
	g_static_mutex_unlock(sweep_array[0].mutex);

	return cat1(argv[0]);
}

char * link_setup_csf (gchar **argv, Sweep *sweep_array, ChanSet *chanset)
{
	f_start(F_CONTROL);

	Sweep *first  = NULL;
	Sweep *leader = NULL;

	bool mentioned[M2_MAX_CHAN];
	for (int vc = 0; vc < M2_MAX_CHAN; vc++) mentioned[vc] = 0;

	for (int i = 1; argv[i] != NULL; i++)
	{
		int vc;
		if (scan_arg_int(argv[i], "channel", &vc) && vc >= 0 && vc < M2_MAX_CHAN && !mentioned[vc])
		{
			mentioned[vc] = 1;

			int ici = chanset->ici_by_vc[vc];
			if (ici != -1)
			{
				if (first == NULL)
				{
					first = leader = &sweep_array[ici];
					g_static_mutex_lock(first->mutex);

					f_print(F_RUN, "First leader: \"%s\"\n", leader->channel->desc);
				}
				else
				{
					leader->follower = &sweep_array[ici];
					leader = leader->follower;

					f_print(F_RUN, "Next leader:  \"%s\"\n", leader->channel->desc);
				}
			}
		}
	}

	if (first != NULL)
	{
		if (first != leader) leader->follower = first;  // close the chain
		g_static_mutex_unlock(first->mutex);
	}

	return cat1(argv[0]);  // reply now (if no usable channels were mentioned, then nothing was done...)
}
