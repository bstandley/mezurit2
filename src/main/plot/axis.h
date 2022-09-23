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

#ifndef _MAIN_PLOT_AXIS_H
#define _MAIN_PLOT_AXIS_H 1

#include <lib/entry.h>
#include <lib/varset/setvarset.h>
#include <main/plot/color.h>

enum
{
	ZOOM_BACK,
	ZOOM_FORWARD,
	ZOOM_AUTO
};

enum
{
	X_AXIS = 0,
	Y1_AXIS,
	Y2_AXIS
};

typedef struct
{
	int X, Y;  // size in pixels of surface
	double X0, X1, Y0, Y1;

} Region;

typedef struct
{
	// public:

		bool enabled;
		Color color;
		GtkWidget *enable_item;

} Brush;

typedef struct
{
	// public:

		int i, vci;
		bool keep_vci;
		char *name, *tick_format, *marker_format;
		double tick;

		NumericRange limit;
		HistoryRange limit_history;

		Brush points, lines;
		Region label[2];

		GtkWidget *rc_menu, *ch_menu, *zoom_auto_item, *zoom_back_item, *zoom_forward_item;

} Axis;

void axis_init  (Axis *axis, int i, GtkWidget *parent);
void axis_final (Axis *axis);

void update_tick      (Axis *axis, double span);
bool zoom_axis        (Axis *axis, SVSP svs, int mode);
void set_axis_channel (Axis *axis, VSP vs);
void set_axis_color   (Axis *axis, ColorScheme *colorscheme, int offset);

double scale_point   (double point, Axis *axis, double min_px, double max_px);
double descale_point (double pixel, Axis *axis, double min_px, double max_px);

#endif
