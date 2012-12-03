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

#ifndef _MAIN_SECTION_H
#define _MAIN_SECTION_H 1

#include <stdbool.h>
#define HEADER_SANS_WARNINGS <gtk/gtk.h>
#include <sans_warnings.h>

enum
{
	SECTION_LEFT = 0,  // matches 0.74b
	SECTION_RIGHT,     // matches 0.74b
	SECTION_TOP,       // matches 0.74b
	SECTION_BOTTOM,    // matches 0.74b
	SECTION_WAYLEFT,
	SECTION_WAYRIGHT,
	SECTION_NOWHERE
};
#define M2_NUM_LOCATION 6
#define M2_NUM_APTS     7

typedef struct
{
	bool visible, expand_fill, horizontal;
	int location, old_location;

	GtkWidget *full, *heading, *box;
	GtkWidget *rollup_button, *orient_button;
	GtkWidget *loc_button, *loc_menu, *loc_item[M2_NUM_LOCATION];

} Section;

void section_init     (Section *sect, const char *icon_filename, const char *label_str, GtkWidget **apt);
void section_register (Section *sect, const char *prefix, int default_loc, GtkWidget **apt);

void add_orient_button (Section *sect);
void add_rollup_button (Section *sect);
void add_loc_menu      (Section *sect, ...);

#endif
