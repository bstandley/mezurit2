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

static gboolean plot_draw_cb (GtkWidget *widget, cairo_t *crd, Plot *plot);
static gboolean plot_size_cb (GtkWidget *widget, cairo_t *crd, Plot *plot);
static gboolean plot_click_cb (GtkWidget *widget, GdkEventButton *event, Plot *plot);
static gboolean plot_minmax_cb (GtkWidget *widget, GdkEvent *event, Plot *plot, Axis *axis, int side);
static void plot_zoom_cb (GtkWidget *widget, Plot *plot, Axis *axis, int zoom);
static gboolean plot_enable_cb (GtkWidget *widget, GdkEvent *event, bool *enabled, Plot *plot);
static gboolean plot_vci_cb (GtkWidget *widget, GdkEvent *event, Plot *plot, Axis *axis, int vci);

static void plot_enable_mcf (bool *enabled, const char *signal_name, MValue value, GtkWidget *widget, Plot *plot);
static void plot_vci_mcf (int *var, const char *signal_name, MValue value, Plot *plot, Axis *axis);
static void plot_minmax_mcf (double *var, const char *signal_name, MValue value, Plot *plot, Axis *axis, int side);

gboolean plot_draw_cb (GtkWidget *widget, cairo_t *cr, Plot *plot)
{
	f_start(F_CALLBACK);

	cairo_set_source_surface(cr, plot->surface, 0, 0);
	cairo_paint(cr);

	if (plot->XM > -0.5) draw_marker(cr, plot->axis, plot->region, plot->colorscheme, plot->XM, plot->YM);  // TODO check clip region, draw only necessary parts?

	return 0;
}

gboolean plot_size_cb (GtkWidget *widget, cairo_t *cr, Plot *plot)
{
	f_start(F_CALLBACK);

	GtkAllocation alloc;
	gtk_widget_get_allocation(widget, &alloc);

	if (plot->surface != NULL) cairo_surface_destroy(plot->surface);
	plot->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, alloc.width, alloc.height);
	plot->region.X = alloc.width;
	plot->region.Y = alloc.height;

	plot_build_noqueue(plot);  // draw signal is automatically emitted along with size-allocate

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
			place_marker(plot, event->x, event->y);
			gtk_widget_queue_draw(plot->area_widget);
		}
#ifdef MINGW
		timer_reset(plot->doubleclick_timer);
#endif
	}
	else if (event->button == 3)
	{
		int region = detect_region(event->x, event->y, plot->region);
		f_print(F_RUN, "X: %f, Y: %f, Region: %d\n", event->x, event->y, region);

		if      (region == REGION_INSIDE) gtk_menu_popup_at_pointer(GTK_MENU(plot->main_rc_menu),          (GdkEvent *) event);
		else if (region == REGION_BOTTOM) gtk_menu_popup_at_pointer(GTK_MENU(plot->axis[X_AXIS].rc_menu),  (GdkEvent *) event);
		else if (region == REGION_LEFT)   gtk_menu_popup_at_pointer(GTK_MENU(plot->axis[Y1_AXIS].rc_menu), (GdkEvent *) event);
		else if (region == REGION_RIGHT)  gtk_menu_popup_at_pointer(GTK_MENU(plot->axis[Y2_AXIS].rc_menu), (GdkEvent *) event);
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

gboolean plot_enable_cb (GtkWidget *widget, GdkEvent *event, bool *enabled, Plot *plot)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	*enabled = !was_active;

	full_plot(plot);

	return 0;
}

void plot_enable_mcf (bool *enabled, const char *signal_name, MValue value, GtkWidget *widget, Plot *plot)
{
	f_start(F_MCF);

	*enabled = value.x_bool;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), value.x_bool);

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

gboolean plot_vci_cb (GtkWidget *widget, GdkEvent *event, Plot *plot, Axis *axis, int vci)
{
	f_start(F_CALLBACK);

	if (vci != axis->vci)
	{
		axis->vci = vci;

		set_axis_channel(axis, plot->svs->data[0]);
		full_plot(plot);
	}

	gtk_widget_hide(gtk_widget_get_parent(gtk_widget_get_parent(widget)));
	return 1;  // prevent event propagation, since the radios were toggled explicity by set_axis_channel(), and we just re-hid the menu
}

void plot_vci_mcf (int *var, const char *signal_name, MValue value, Plot *plot, Axis *axis)
{
	f_start(F_MCF);

	if (str_equal(signal_name, "setup"))
	{
		axis->vci = value.x_int;
		axis->keep_vci = 1;
	}
	else if (str_equal(signal_name, "panel"))
	{
		int vci = window_int(value.x_int, -1, plot->svs->data[0]->N_col - 1);
		if (vci != axis->vci)
		{
			axis->vci = vci;

			set_axis_channel(axis, plot->svs->data[0]);
			full_plot(plot);
		}
	}
}
