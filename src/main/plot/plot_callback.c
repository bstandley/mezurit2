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

static gboolean plot_draw_cb (GtkWidget *widget, void *ptr, Plot *plot);
static gboolean plot_click_cb (GtkWidget *widget, GdkEventButton *event, Plot *plot);
static gboolean plot_minmax_cb (GtkWidget *widget, GdkEvent *event, Plot *plot, Axis *axis, int side);
static void plot_zoom_cb (GtkWidget *widget, Plot *plot, Axis *axis, int zoom);
static void plot_enable_cb (GtkWidget *widget, Plot *plot);
static void plot_style_cb (GtkWidget *widget, bool *var, Plot *plot);
static void plot_vci_cb (GtkWidget *widget, Plot *plot, Axis *axis, int vci);

static void plot_enable_mcf (void *ptr, const char *signal_name, MValue value, Plot *plot);
static void plot_style_mcf (bool *enabled, const char *signal_name, MValue value, GtkWidget *widget, Plot *plot);
static void plot_vci_mcf (int *vci, const char *signal_name, MValue value, Plot *plot, Axis *axis);
static void plot_minmax_mcf (double *var, const char *signal_name, MValue value, Plot *plot, Axis *axis, int side);

gboolean plot_draw_cb (GtkWidget *widget, void *ptr, Plot *plot)  // ptr may be a CairoContext* or a GdkEvent*, but we don't care
{
	if (plot->exposure_blocked > 0)
	{
		plot->exposure_blocked--;
		f_print(F_CALLBACK, "Skipping unnecessary exposure event.\n");
		return 0;
	}

	f_start(F_CALLBACK);

	full_plot(plot);
	plot->exposure_complete = 1;

	return 0;
}

gboolean plot_click_cb (GtkWidget *widget, GdkEventButton *event, Plot *plot)
{
	f_start(F_CALLBACK);

	f_print(F_RUN, "Button: %d, Double: %s, X: %f, Y: %f\n", event->button, event->type == GDK_2BUTTON_PRESS ? "Y" : "N", event->x, event->y);

	if (event->button == 1)
	{
		GtkWidget *minmax_widget = NULL;
#ifndef MINGW
		if (event->type == GDK_2BUTTON_PRESS)
#else
		if (timer_elapsed(plot->doubleclick_timer) < M2_WIN32_DOUBLECLICK_TIME)
#endif
		{
			for (int a = 0; a < 3 && minmax_widget == NULL; a++)
				for (int s = 0; s < 2 && minmax_widget == NULL; s++)
					if (detect_region(event->x, event->y, plot->axis[a].label[s]) == REGION_INSIDE)
						minmax_widget = plot->axis[a].limit.entry[s]->widget;
		}

		if (minmax_widget != NULL) show_all(gtk_widget_get_parent(minmax_widget), NULL);
		else
		{
			flip_surface(plot->area_widget, plot->surface);  // erase dirty elements
			place_marker(plot, event->x, event->y);
		}
#ifdef MINGW
		timer_reset(plot->doubleclick_timer);
#endif
	}
	else if (event->button == 3)
	{
		int region = detect_region(event->x, event->y, plot->region);
		f_print(F_RUN, "X: %f, Y: %f, Region: %d\n", event->x, event->y, region);

		if      (region == REGION_INSIDE) gtk_menu_popup(GTK_MENU(plot->main_rc_menu),          NULL, NULL, NULL, NULL, 0, event->time);
		else if (region == REGION_BOTTOM) gtk_menu_popup(GTK_MENU(plot->axis[X_AXIS].rc_menu),  NULL, NULL, NULL, NULL, 0, event->time);
		else if (region == REGION_LEFT)   gtk_menu_popup(GTK_MENU(plot->axis[Y1_AXIS].rc_menu), NULL, NULL, NULL, NULL, 0, event->time);
		else if (region == REGION_RIGHT)  gtk_menu_popup(GTK_MENU(plot->axis[Y2_AXIS].rc_menu), NULL, NULL, NULL, NULL, 0, event->time);
	}

	return 0;
}

gboolean plot_minmax_cb (GtkWidget *widget, GdkEvent *event, Plot *plot, Axis *axis, int side)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		bool ok;
		double target = read_entry(axis->limit.entry[side], &ok);
		if (ok)
		{
			set_range(&axis->limit, side, target);
			write_entry(axis->limit.entry[side], target);

			update_tick(axis, axis->limit.value[UPPER] - axis->limit.value[LOWER]);
			history_range_append(&axis->limit_history, &axis->limit);

			full_plot(plot);
		}
		else write_entry(axis->limit.entry[side], axis->limit.value[side]);

		if (event->type == GDK_FOCUS_CHANGE) gtk_widget_hide(gtk_widget_get_parent(widget));
	}

	return 0;
}

void plot_minmax_mcf (double *var, const char *signal_name, MValue value, Plot *plot, Axis *axis, int side)
{
	f_start(F_MCF);

	*var = value.x_double;

	if (rectify_range(&axis->limit)) write_entry(axis->limit.entry[1 - side], axis->limit.value[1 - side]);
	/**/                             write_entry(axis->limit.entry[side], *var);

	update_tick(axis, axis->limit.value[UPPER] - axis->limit.value[LOWER]);

	if      (str_equal(signal_name, "setup")) plot->mcf_activity = 1;
	else if (str_equal(signal_name, "panel"))
	{
		history_range_append(&axis->limit_history, &axis->limit);
		full_plot(plot);
	}
}

void plot_enable_cb (GtkWidget *widget, Plot *plot)
{
	f_start(F_CALLBACK);

	plot->enabled = !plot->enabled;
	set_item_checked(plot->enable_item, plot->enabled);

	full_plot(plot);
}

void plot_enable_mcf (void *ptr, const char *signal_name, MValue value, Plot *plot)
{
	f_start(F_MCF);

	plot->enabled = value.x_bool;
	set_item_checked(plot->enable_item, value.x_bool);

	if (str_equal(signal_name, "panel")) full_plot(plot);
}

void plot_style_cb (GtkWidget *widget, bool *enabled, Plot *plot)
{
	f_start(F_CALLBACK);

	*enabled = !(*enabled);
	set_item_checked(widget, *enabled);

	full_plot(plot);
}

void plot_style_mcf (bool *enabled, const char *signal_name, MValue value, GtkWidget *widget, Plot *plot)
{
	f_start(F_MCF);

	*enabled = value.x_bool;
	set_item_checked(widget, value.x_bool);

	if (str_equal(signal_name, "panel")) full_plot(plot);
}

void plot_zoom_cb (GtkWidget *widget, Plot *plot, Axis *axis, int zoom)
{
	f_start(F_CALLBACK);

	bool changed = 0;
	if (axis != NULL) { if (zoom_axis(axis, plot->svs, zoom)) changed = 1; }
	else
	{
		for (int a = 0; a < 3; a++)  // zoom all axes
			if (zoom_axis(&plot->axis[a], plot->svs, zoom))
				changed = 1;
	}

	if (changed) full_plot(plot);
}

void plot_vci_cb (GtkWidget *widget, Plot *plot, Axis *axis, int vci)
{
	f_start(F_CALLBACK);

	set_axis_channel(axis, plot->svs->data[0], vci);
	full_plot(plot);
}

void plot_vci_mcf (int *vci, const char *signal_name, MValue value, Plot *plot, Axis *axis)
{
	f_start(F_MCF);

	if (str_equal(signal_name, "setup"))
	{
		*vci = value.x_int;
		axis->keep_vci = 1;
	}
	else if (str_equal(signal_name, "panel"))
	{
		set_axis_channel(axis, plot->svs->data[0], window_int(value.x_int, -1, plot->svs->data[0]->N_col - 1));
		full_plot(plot);
	}
}
