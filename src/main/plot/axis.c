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

#include "axis.h"

#include <math.h>

#include <lib/status.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/num.h>

void axis_init (Axis *axis, int i, GtkWidget *parent)
{
	f_start(F_INIT);

	axis->i = i;
	axis->vci = -1;
	axis->keep_vci = 0;

	axis->name          = cat1("");
	axis->tick_format   = cat1("");
	axis->marker_format = cat1("");

	for sides
	{
		axis->limit.entry[s] = new_numeric_entry(1, 7, 9);
		attach_window(axis->limit.entry[s]->widget, parent);
	}

	set_range(&axis->limit, LOWER, -1.0);
	set_range(&axis->limit, UPPER, -1.0);

	axis->limit_history.occupied = 0;
	axis->limit_history.current = -1;

	axis->points.color.r = 1.0;
	axis->points.color.g = 1.0;
	axis->points.color.b = 1.0;

	axis->lines.color = darken(axis->points.color);

	// right-click menu:
	axis->rc_menu = gtk_menu_new();
	char *text = atg(cat1(i == X_AXIS ? "X" : (i == Y1_AXIS ? "Y1" : "Y2")));

	axis->zoom_auto_item                 = menu_append(gtk_menu_item_new_with_label(atg(cat3("Zoom ", text, ": Auto"))),    axis->rc_menu);
	axis->zoom_back_item                 = menu_append(gtk_menu_item_new_with_label(atg(cat3("Zoom ", text, ": Back"))),    axis->rc_menu);
	axis->zoom_forward_item              = menu_append(gtk_menu_item_new_with_label(atg(cat3("Zoom ", text, ": Forward"))), axis->rc_menu);
	if (i != 0) axis->lines.enable_item  = menu_append(gtk_check_menu_item_new_with_label("Show lines"),                    axis->rc_menu);
	if (i != 0) axis->points.enable_item = menu_append(gtk_check_menu_item_new_with_label("Show points"),                   axis->rc_menu);

	axis->ch_menu = set_submenu(gtk_menu_new(), menu_append(gtk_menu_item_new_with_label("Channel"), axis->rc_menu));
	show_all(axis->rc_menu, NULL);
}

void set_axis_color (Axis *axis, ColorScheme *colorscheme, int offset)
{
	f_start(F_RUN);

	if (axis->vci != -1)
	{
		axis->lines.color = colorscheme->data[(offset + axis->vci) % M2_NUM_COLOR];
		axis->points.color = lighten(axis->lines.color);
	}
}

void axis_final (Axis *axis)
{
	f_start(F_INIT);

	replace(axis->name,          NULL);
	replace(axis->tick_format,   NULL);
	replace(axis->marker_format, NULL);

	for sides destroy_entry(axis->limit.entry[s]);
}

bool zoom_axis (Axis *axis, SVSP svs, int mode)
{
	f_start(F_RUN);

	bool changed = 0;

	if (mode == ZOOM_BACK || mode == ZOOM_FORWARD)
	{
		if (history_range_increment(&axis->limit_history, &axis->limit, mode == ZOOM_BACK ? -1 : 1))
		{
			update_tick(axis, axis->limit.value[UPPER] - axis->limit.value[LOWER]);
			for sides { write_entry(axis->limit.entry[s], axis->limit.value[s]); }
			changed = 1;
		}
	}
	else if (mode == ZOOM_AUTO && axis->vci != -1 && total_pts(svs) > 0)  // checking total_pts will guarantee that min and max will be set
	{
		double min_value = 0, max_value = 0;  // initialize to quiet a compiler warning
		bool first = 1;

		for (int i = 0; i < svs->N_set; i++) for (int j = 0; j < svs->data[i]->N_pt; j++)
		{
			double value = vs_value(svs->data[i], j, axis->vci);

			min_value = first ? value : min_double(value, min_value);
			max_value = first ? value : max_double(value, max_value);

			first = 0;
		}

		update_tick(axis, max_value - min_value);
		double lower = floor((min_value - 1e-6) / axis->tick) * axis->tick;
		double upper = ceil ((max_value + 1e-6) / axis->tick) * axis->tick;

		if (lower != axis->limit.value[LOWER] || upper != axis->limit.value[UPPER])
		{
			for sides
			{
				set_range(&axis->limit, s, s == LOWER ? lower : upper);
				write_entry(axis->limit.entry[s], axis->limit.value[s]);
			}
			history_range_append(&axis->limit_history, &axis->limit);
			changed = 1;
		}
	}

	return changed;
}

void set_axis_channel (Axis *axis, VSP vs)
{
	f_start(F_VERBOSE);

	for (int vci = -1; vci < vs->N_col; vci++)
	{
		GtkWidget *widget = get_child(axis->ch_menu, vci + 1);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), vci == axis->vci);
	}

	replace(axis->name, cat1(axis->vci == -1 ? "none" : get_colname(vs, axis->vci)));
}

void update_tick (Axis *axis, double span)
{
	f_start(F_VERBOSE);

	int decimal_places = 5;
	double span_max = 0.0005;
	axis->tick = 0.00001;

	bool order_of_mag = 0;
	while (span > span_max)
	{
		if (order_of_mag && decimal_places > 0) decimal_places--;

		axis->tick *= order_of_mag ? 2 : 5;
		span_max   *= order_of_mag ? 2 : 5;

		order_of_mag = !order_of_mag;
	}

	replace(axis->tick_format,   supercat("%%1.%df", decimal_places));
	replace(axis->marker_format, supercat("%%1.%df", decimal_places + 2));
}

double scale_point (double point, Axis *axis, double min_px, double max_px)
{
	double min = axis->limit.value[LOWER];
	double max = axis->limit.value[UPPER];

	return min_px + (point - min) / (max - min) * (max_px - min_px);
}

double descale_point (double pixel, Axis *axis, double min_px, double max_px)
{
	double min = axis->limit.value[LOWER];
	double max = axis->limit.value[UPPER];

	return min + (pixel - min_px) * (max - min) / (max_px - min_px);
}
