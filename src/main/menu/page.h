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

#ifndef _MAIN_MENU_PAGE_H
#define _MAIN_MENU_PAGE_H 1

#include <main/menu/config.h>
#include <main/menu/terminal.h>

typedef struct
{
	// private:

		GtkWidget *setup_item, *panel_item[M2_NUM_PANEL];

		Setup *setup;        // inherited from Mezurit2
		Panel *panel_array;  // inherited from Mezurit2
		Terminal *terminal;  // inherited from Mezurit2

	// public:

		GtkWidget *main_window, *worktop;  // externally initialized before calling page_init()

} Page;

void page_init     (Page *page, GtkWidget *menubar);
void page_register (Page *page, ThreadVars *tv, Config *config);

void set_page (Page *page, Config *config, int pid);

#endif
