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

#ifndef _MAIN_MENU_BUFMENU_H
#define _MAIN_MENU_BUFMENU_H 1

#include <stdbool.h>
#define HEADER_SANS_WARNINGS <gtk/gtk.h>
#include <sans_warnings.h>

typedef struct
{
	// private:

		GtkWidget *link_tzero_item, *save_header_item;
		GtkWidget *save_mcf_item, *save_scr_item;

	// public:

		bool link_tzero;                       // read by clear_cb()
		bool save_header, save_mcf, save_scr;  // read by save_buffer()

} Bufmenu;

void bufmenu_init     (Bufmenu *bufmenu, GtkWidget *menubar);
void bufmenu_register (Bufmenu *bufmenu);

#endif
