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

#ifndef _MAIN_PLOT_PLOT_H
#define _MAIN_PLOT_PLOT_H 1

#ifdef MINGW
#include <lib/hardware/timing.h>
#endif
#include <lib/varset/setvarset.h>
#include <main/plot/axis.h>

typedef struct
{
	// private:

		Axis axis[3];
		ColorScheme *colorscheme;  // inherited from Panel

		GtkWidget *area_widget, *main_rc_menu;
		GtkWidget *enable_item, *zoom_auto_item, *zoom_back_item, *zoom_forward_item;
#ifdef MINGW
		Timer *doubleclick_timer;
#endif
		cairo_surface_t *surface;  // internal image buffer
		Region region;             // dimensions of surface and margins of plottable space
		double XM, YM;             // location of clicked point (XM = -1 for none)

		long displayed_total;
		int displayed_sets;
		int displayed_percent;

		int active_set;  // index of currently-plotting varset
		long draw_set;   // points already drawn from currently-plotting varset
		bool mcf_activity;

		SVSP svs, blank_svs;

	// public:

		bool enabled;

		long draw_total;    // total points already drawn
		long draw_request;  // total points to plot next tick

} Plot;

// Note: All plotting is done in the GUI thread. No locking is required.
//       The data buffer is safe to read as specified by draw_request.

void plot_init     (Plot *plot, GtkWidget *parent);
void plot_register (Plot *plot, int pid);
void plot_final    (Plot *plot);

void plot_register_legacy (Plot *plot);

void plot_update_channels (Plot *plot, int *vc_by_vci, SVSP svs);  // call when vset columns change

void full_plot   (Plot *plot);
void plot_reset  (Plot *plot);
void plot_tick   (Plot *plot);
void plot_buffer (Plot *plot, long total, int sets);
void plot_scope  (Plot *plot, int percent);

#endif
