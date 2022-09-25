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

#include "plot.h"

#include <stdlib.h>
#include <math.h>

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/num.h>
#include <main/plot/axis.h>
#include <main/plot/color.h>

enum
{
	REGION_INSIDE = 0,
	REGION_TOP,
	REGION_BOTTOM,
	REGION_LEFT,
	REGION_RIGHT,
	REGION_CORNER
};

static long plot_data (cairo_t *cr, Plot *plot, long request);
static void plot_build_noqueue (Plot *plot);
static void place_marker (Plot *plot, double XM, double YM);
static void update_axis_channels (Plot* plot, Axis *axis, int *vc_by_vci);

#include "plot_draw.c"
#include "plot_callback.c"

void plot_init (Plot *plot, GtkWidget *parent)
{
	f_start(F_INIT);

	// container:
	plot->area_widget = pack_start(gtk_drawing_area_new(), 1, parent);
	gtk_widget_add_events(plot->area_widget, GDK_BUTTON_PRESS_MASK);

	// make the main right-click menu:
	plot->main_rc_menu = gtk_menu_new();
	plot->zoom_auto_item    = menu_append(gtk_menu_item_new_with_label("Zoom All: Auto"),    plot->main_rc_menu);
	plot->zoom_back_item    = menu_append(gtk_menu_item_new_with_label("Zoom All: Back"),    plot->main_rc_menu);  // TODO initially inactive?
	plot->zoom_forward_item = menu_append(gtk_menu_item_new_with_label("Zoom All: Forward"), plot->main_rc_menu);  // TODO initially inactive?
	plot->enable_item       = menu_append(gtk_check_menu_item_new_with_label("Enable plot"), plot->main_rc_menu);
	show_all(plot->main_rc_menu, NULL);

	// make axes:
	for (int a = 0; a < 3; a++) axis_init(&plot->axis[a], a, parent);

	plot->surface = NULL;

	plot->region.X = plot->region.Y = -1;
	plot->XM       = plot->YM       = -1;

	plot->displayed_total = 0;
	plot->displayed_sets = 0;
	plot->displayed_percent = 0;

	plot->active_set = 0;
	plot->draw_set = 0;

	plot->blank_svs = append_vset(new_svset(), new_vset(0));
	plot->svs = plot->blank_svs;

	plot->mcf_activity = 0;

#ifdef MINGW
	plot->doubleclick_timer = timer_new();
#endif
}

void plot_register (Plot *plot, int pid)
{
	f_start(F_INIT);

	char *prefix = atg(supercat("panel%d_plot_", pid));

	/**/              mcf_register(NULL,          "# Plot",                     MCF_W);
	int enabled_var = mcf_register(&plot->enabled, atg(cat2(prefix, "enable")), MCF_BOOL | MCF_W | MCF_DEFAULT, 1);

	mcf_connect(enabled_var, "setup, panel", BLOB_CALLBACK(plot_enable_mcf), 0x20, plot->enable_item, plot);

	snazzy_connect(plot->zoom_auto_item,    "activate",             SNAZZY_VOID_VOID, BLOB_CALLBACK(plot_zoom_cb),   0x21, plot, NULL, ZOOM_AUTO);
	snazzy_connect(plot->zoom_back_item,    "activate",             SNAZZY_VOID_VOID, BLOB_CALLBACK(plot_zoom_cb),   0x21, plot, NULL, ZOOM_BACK);
	snazzy_connect(plot->zoom_forward_item, "activate",             SNAZZY_VOID_VOID, BLOB_CALLBACK(plot_zoom_cb),   0x21, plot, NULL, ZOOM_FORWARD);
	snazzy_connect(plot->enable_item,       "button-release-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(plot_enable_cb), 0x20, &plot->enabled, plot);

	snazzy_connect(plot->area_widget, "size-allocate",      SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_size_cb),  0x10, plot);
	snazzy_connect(plot->area_widget, "draw",               SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_draw_cb),  0x10, plot);
	snazzy_connect(plot->area_widget, "button-press-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_click_cb), 0x10, plot);

	for (int a = 0; a < 3; a++)
	{
		Axis *axis = &plot->axis[a];
		char *name = atg(cat1(a == X_AXIS ? "x_axis_" : (a == Y1_AXIS ? "y1_axis_" : "y2_axis_")));

		// channel, min, max

		int vci_var = mcf_register(&axis->vci,                atg(cat3(prefix, name, "channel")), MCF_INT    | MCF_W | MCF_DEFAULT, -1);
		int min_var = mcf_register(&axis->limit.value[LOWER], atg(cat3(prefix, name, "min")),     MCF_DOUBLE | MCF_W | MCF_DEFAULT, -1.0);
		int max_var = mcf_register(&axis->limit.value[UPPER], atg(cat3(prefix, name, "max")),     MCF_DOUBLE | MCF_W | MCF_DEFAULT,  1.0);

		mcf_connect(vci_var, "setup, panel", BLOB_CALLBACK(plot_vci_mcf),    0x20, plot, axis);
		mcf_connect(min_var, "setup, panel", BLOB_CALLBACK(plot_minmax_mcf), 0x21, plot, axis, LOWER);
		mcf_connect(max_var, "setup, panel", BLOB_CALLBACK(plot_minmax_mcf), 0x21, plot, axis, UPPER);

		snazzy_connect(axis->limit.entry[LOWER]->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_minmax_cb), 0x21, plot, axis, LOWER);
		snazzy_connect(axis->limit.entry[UPPER]->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_minmax_cb), 0x21, plot, axis, UPPER);

		// zooms

		snazzy_connect(axis->zoom_auto_item,    "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(plot_zoom_cb), 0x21, plot, axis, ZOOM_AUTO);
		snazzy_connect(axis->zoom_back_item,    "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(plot_zoom_cb), 0x21, plot, axis, ZOOM_BACK);
		snazzy_connect(axis->zoom_forward_item, "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(plot_zoom_cb), 0x21, plot, axis, ZOOM_FORWARD);

		// brush style

		if (a != 0)
		{
			int lines_var  = mcf_register(&axis->lines.enabled,  atg(cat3(prefix, name, "lines")),  MCF_BOOL | MCF_W | MCF_DEFAULT, 1);
			int points_var = mcf_register(&axis->points.enabled, atg(cat3(prefix, name, "points")), MCF_BOOL | MCF_W | MCF_DEFAULT, 0);

			mcf_connect(lines_var,  "setup, panel", BLOB_CALLBACK(plot_enable_mcf), 0x20, axis->lines.enable_item,  plot);
			mcf_connect(points_var, "setup, panel", BLOB_CALLBACK(plot_enable_mcf), 0x20, axis->points.enable_item, plot);

			snazzy_connect(axis->lines.enable_item,  "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_enable_cb), 0x20, &axis->lines.enabled,  plot);
			snazzy_connect(axis->points.enable_item, "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_enable_cb), 0x20, &axis->points.enabled, plot);
		}
	}
}

void plot_register_legacy (Plot *plot)
{
	f_start(F_INIT);

	int enabled_var = mcf_register(&plot->enabled, "logger_pref_show_plot", MCF_BOOL);

	mcf_connect_with_note(enabled_var, "setup", "Mapped obsolete var to 'Panel A'.\n", BLOB_CALLBACK(plot_enable_mcf), 0x10, plot);

	for (int a = 0; a < 3; a++)
	{
		Axis *axis = &plot->axis[a];

		int vci_var = mcf_register(&axis->vci,                atg(supercat("logger_axis%d_channel", a)), MCF_INT);
		int min_var = mcf_register(&axis->limit.value[LOWER], atg(supercat("logger_axis%d_min",     a)), MCF_DOUBLE);
		int max_var = mcf_register(&axis->limit.value[UPPER], atg(supercat("logger_axis%d_max",     a)), MCF_DOUBLE);
		/**/          mcf_register(NULL,                      atg(supercat("logger_axis%d_oldmin",  a)), MCF_DOUBLE);  // obsolete var
		/**/          mcf_register(NULL,                      atg(supercat("logger_axis%d_oldmax",  a)), MCF_DOUBLE);  // obsolete var

		mcf_connect_with_note(vci_var, "setup", "Mapped obsolete var to 'Panel A'.\n", BLOB_CALLBACK(plot_vci_mcf),    0x20, plot, axis);
		mcf_connect_with_note(min_var, "setup", "Mapped obsolete var to 'Panel A'.\n", BLOB_CALLBACK(plot_minmax_mcf), 0x21, plot, axis, LOWER);
		mcf_connect_with_note(max_var, "setup", "Mapped obsolete var to 'Panel A'.\n", BLOB_CALLBACK(plot_minmax_mcf), 0x21, plot, axis, UPPER);
	}
}

void plot_final (Plot *plot)
{
	f_start(F_INIT);

	if (plot->surface != NULL) cairo_surface_destroy(plot->surface);
	for (int a = 0; a < 3; a++)	axis_final(&plot->axis[a]);

	free_svset(plot->blank_svs);
}

void plot_update_channels (Plot *plot, int *vc_by_vci, SVSP svs)  // call when vset columns change
{
	f_start(F_UPDATE);

	if (svs == NULL || svs->N_set < 1)
	{
		svs = plot->blank_svs;
		f_print(F_ERROR, "Error: plot->svs should have at least one vset.\n");
	}

	plot->svs = svs;  // svs may have changed due to a clear_buffer()
	for (int a = 0; a < 3; a++) update_axis_channels(plot, &plot->axis[a], vc_by_vci);
}

void update_axis_channels (Plot *plot, Axis *axis, int *vc_by_vci)
{
	f_start(F_UPDATE);

	VSP vs = plot->svs->data[0];

	GList *list = gtk_container_get_children(GTK_CONTAINER(axis->ch_menu));
	for (guint n = 0; n < g_list_length(list); n++) gtk_widget_destroy(GTK_WIDGET(g_list_nth_data(list, n)));
	g_list_free(list);

	for (int vci = -1; vci < vs->N_col; vci++)
	{
		char *label_str = atg(vci == -1 ? cat1("none") : supercat("X<sub>%d</sub>: %s", vc_by_vci[vci], get_colname(vs, vci)));

		GtkWidget *item = menu_append(new_radio_item(label_str), axis->ch_menu);
		snazzy_connect(item, "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(plot_vci_cb), 0x21, plot, axis, vci);
	}
	show_all(axis->ch_menu, NULL);

	// update axis.vci if necessary

	int vci_request = axis->vci;  // start with old vci
	if (!axis->keep_vci || vci_request < -1 || vci_request >= vs->N_col)
	{
		vci_request = -1;  // default in case old axis.name doesn't match any new col names
		for (int vci = 0; vci < vs->N_col; vci++)
			if (str_equal(axis->name, get_colname(vs, vci)))
			{
				vci_request = vci;
				break;  // match the first one
			}
	}
	axis->vci = vci_request;  // TODO is vci_request var actually necessary?
	axis->keep_vci = 0;
	set_axis_channel(axis, vs);
}

void place_marker (Plot *plot, double XM, double YM)
{
	f_start(F_RUN);

	if (detect_region(XM, YM, plot->region) == REGION_INSIDE)
	{
		plot->XM = XM;
		plot->YM = YM;
	}
	else
	{
		plot->XM = -1;
		plot->YM = -1;
	}
}

void plot_build (Plot *plot)
{
	f_start(F_RUN);

	plot_build_noqueue(plot);
	gtk_widget_queue_draw(plot->area_widget);
}

void plot_build_noqueue (Plot *plot)
{
	if (plot->mcf_activity)  // TODO move elsewhere?
	{
		plot->mcf_activity = 0;
		for (int a = 0; a < 3; a++)
			history_range_append(&plot->axis[a].limit_history, &plot->axis[a].limit);
	}

	VSP vs = plot->svs->data[0];  // plot->svs must have at least one set
	Region *r = &plot->region;  // use reference to allow modifications
	cairo_t *cr = cairo_create(plot->surface);

	// background
	cairo_rectangle(cr, 0, 0, r->X, r->Y);
	set_source_rgb(cr, plot->colorscheme->background);
	cairo_fill_preserve(cr);
	cairo_stroke(cr);

	// buffer display
	draw_buffer_symbol(cr, plot->region, plot->colorscheme);
	draw_buffer_readout(cr, plot->region, plot->colorscheme, plot->displayed_total, plot->displayed_sets);
	draw_scope_symbol(cr, plot->region, plot->colorscheme);
	draw_scope_progress(cr, plot->region, plot->colorscheme, plot->displayed_percent);

	// handle x ticks
	cairo_new_path(cr);
	cairo_set_font_size(cr, M2_TICK_LABEL_SIZE);

	// simulate drawing to find tick label lengths
	double x_lw0, x_lw1, y1_lw_max, y2_lw_max;

	for (int a = 0; a < 3; a++)
	{
		if (a == X_AXIS) draw_axis(cr, &plot->axis[a], vs, *r, plot->colorscheme, &x_lw0, &x_lw1, NULL);
		else             draw_axis(cr, &plot->axis[a], vs, *r, plot->colorscheme, NULL,   NULL,   a == Y1_AXIS ? &y1_lw_max : &y2_lw_max);
	}

	double XM_temp = (plot->XM - r->X0) / (r->X1 - r->X0);
	double YM_temp = (plot->YM - r->Y0) / (r->Y1 - r->Y0);

	r->X0 =        max_double(y1_lw_max + 2 * M2_TICK_LABEL_PAD + M2_TICK_LENGTH_LARGE, x_lw0 / 2 + M2_TICK_LABEL_PAD) + (plot->axis[Y1_AXIS].vci != -1 ? M2_YBORDER : 0);
	r->X1 = r->X - max_double(y2_lw_max + 2 * M2_TICK_LABEL_PAD + M2_TICK_LENGTH_LARGE, x_lw1 / 2 + M2_TICK_LABEL_PAD) - (plot->axis[Y2_AXIS].vci != -1 ? M2_YBORDER : 0);
	r->Y0 =        M2_TICK_LABEL_SIZE / 2 + M2_TICK_LABEL_PAD;
	r->Y1 = r->Y - M2_TICK_LABEL_SIZE - 2 * M2_TICK_LABEL_PAD - M2_TICK_LENGTH_LARGE - M2_XBORDER;

	if (plot->XM > -0.5)
	{
		plot->XM = XM_temp * (r->X1 - r->X0) + r->X0;
		plot->YM = YM_temp * (r->Y1 - r->Y0) + r->Y0;
	}

	for (int a = 0; a < 3; a++)
		draw_axis(cr, &plot->axis[a], vs, *r, plot->colorscheme, NULL, NULL, NULL);

	cairo_destroy(cr);

	// reset in-flight plotting
	plot->active_set = 0;
	plot->draw_set = 0;
	plot->draw_total = 0;
}

bool plot_active (Plot *plot)
{
	return (plot->enabled && plot->axis[X_AXIS].vci != -1);
}

void plot_tick (Plot *plot, long draw_request)  // bite-size plotting session for smooth operation
{
	cairo_t *cr = cairo_create(plot->surface);

	draw_request -= plot_data(cr, plot, draw_request);
	while (draw_request > 0 && plot->active_set + 1 < plot->svs->N_set)  // move to next set until request is satisfied or we run out of sets
	{
		plot->active_set++;
		plot->draw_set = 0;

		draw_request -= plot_data(cr, plot, draw_request);
	}

	cairo_destroy(cr);
	gtk_widget_queue_draw_area(plot->area_widget, (gint) plot->region.X0, (gint) plot->region.Y0, (gint) (plot->region.X1 - plot->region.X0), (gint) (plot->region.Y1 - plot->region.Y0));  // TODO properly calculate, use cairo regions
}

long plot_data (cairo_t *cr, Plot *plot, long draw_request)
{
	VSP vs = plot->svs->data[plot->active_set];

	long begin = plot->draw_set;
	plot->draw_set = min_long(begin + draw_request, vs->N_pt);  // update plot.draw_set to reflect last plotted point
	plot->draw_total += plot->draw_set - begin;

	for (int a = 1; a < 3; a++)
	{
		if (plot->axis[a].vci != -1)
		{
			set_axis_color(&plot->axis[a], plot->colorscheme, plot->active_set + plot->axis[a].vci);

			draw_axis_lines (cr, &plot->axis[X_AXIS], &plot->axis[a], vs, plot->region, begin, plot->draw_set);
			draw_axis_points(cr, &plot->axis[X_AXIS], &plot->axis[a], vs, plot->region, begin, plot->draw_set);
		}
	}

	f_print(F_VERBOSE, "Info: Plotted %ld points.\n", plot->draw_set - begin);

	return plot->draw_set - begin;
}

void plot_buffer (Plot *plot, long total, int sets)
{
	if (plot->displayed_total != total || plot->displayed_sets != sets)
	{
		cairo_t *cr = cairo_create(plot->surface);
		cairo_rectangle_int_t rect[2];
		rect[0] = erase_buffer_readout(cr, plot->region, plot->colorscheme, plot->displayed_total, plot->displayed_sets);
		rect[1] = draw_buffer_readout(cr, plot->region, plot->colorscheme, total, sets);
		cairo_destroy(cr);

		cairo_region_t *dr = cairo_region_create_rectangles(rect, 2);
		gtk_widget_queue_draw_region(plot->area_widget, dr);
		cairo_region_destroy(dr);

		plot->displayed_total = total;
		plot->displayed_sets = sets;
	}
}

void plot_scope (Plot *plot, int percent)
{
	if (plot->displayed_percent != percent)
	{
		cairo_t *cr = cairo_create(plot->surface);
		cairo_rectangle_int_t rect = draw_scope_progress(cr, plot->region, plot->colorscheme, percent);
		cairo_destroy(cr);

		cairo_region_t *dr = cairo_region_create_rectangle(&rect);
		gtk_widget_queue_draw_region(plot->area_widget, dr);
		cairo_region_destroy(dr);

		plot->displayed_percent = percent;
	}
}
