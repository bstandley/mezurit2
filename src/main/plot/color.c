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

#include "color.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/util/str.h>
#include <lib/util/num.h>

static void color_register (Color *color, const char *prefix, int r, int g, int b);

Color darken (Color c)
{
	c.r = max_double(c.r - 0.2, 0.0);
	c.g = max_double(c.g - 0.2, 0.0);
	c.b = max_double(c.b - 0.2, 0.0);

	return c;
}

Color lighten (Color c)
{
	c.r = min_double(c.r + 0.2, 1.0);
	c.g = min_double(c.g + 0.2, 1.0);
	c.b = min_double(c.b + 0.2, 1.0);

	return c;
}

void color_register (Color *color, const char *prefix, int r, int g, int b)
{
	f_start(F_NONE);

	int red_var   = mcf_register(&color->r, atg(cat2(prefix, "red")),   MCF_DOUBLE | MCF_W | MCF_DEFAULT, r / 255.0);
	int green_var = mcf_register(&color->g, atg(cat2(prefix, "green")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, g / 255.0);
	int blue_var  = mcf_register(&color->b, atg(cat2(prefix, "blue")),  MCF_DOUBLE | MCF_W | MCF_DEFAULT, b / 255.0);

	mcf_connect(red_var,   "setup, panel", BLOB_CALLBACK(set_double_mcf), 0x00);
	mcf_connect(green_var, "setup, panel", BLOB_CALLBACK(set_double_mcf), 0x00);
	mcf_connect(blue_var,  "setup, panel", BLOB_CALLBACK(set_double_mcf), 0x00);
}

void colorscheme_register (ColorScheme *colorscheme)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Colors", MCF_W);

	color_register(&colorscheme->background, "plot_background_",   0,   0,   0);  // black
	color_register(&colorscheme->axis,       "plot_axis_",       220, 220, 220);  // off white
	color_register(&colorscheme->label,      "plot_label_",      255, 255, 255);  // white
	color_register(&colorscheme->minmax,     "plot_minmax)",     128, 128, 190);  // blue grey
	color_register(&colorscheme->minmax_bg,  "plot_minmax_bg_",   30,  30,  30);  // dark grey
	color_register(&colorscheme->marker,     "plot_marker_",     140, 255, 140);  // light green
	color_register(&colorscheme->display,    "plot_display_",    200, 230, 255);  // light blue
	color_register(&colorscheme->display_bg, "plot_display_bg_",   0,  80, 140);  // dark blue
	color_register(&colorscheme->data[0],    "plot_color0_",       0, 205,   0);  // green
	color_register(&colorscheme->data[1],    "plot_color1_",      50,  50, 255);  // blue
	color_register(&colorscheme->data[2],    "plot_color2_",     220,   0,   0);  // red
	color_register(&colorscheme->data[3],    "plot_color3_",       0, 200, 220);  // cyan
	color_register(&colorscheme->data[4],    "plot_color4_",     245, 230,   0);  // yellow
	color_register(&colorscheme->data[5],    "plot_color5_",     215,   0, 187);  // magenta
	color_register(&colorscheme->data[6],    "plot_color6_",     255, 126,   0);  // orange
}
