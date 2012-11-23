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

#ifndef _MAIN_MENU_TERMINAL_H
#define _MAIN_MENU_TERMINAL_H 1

#include <main/thread/gui.h>

typedef struct
{
	// private:

		GtkWidget *vte, *abort_item, *restart_item;

} Terminal;

void terminal_init     (Terminal *terminal, GtkWidget *menubar, GtkWidget *scroll, int port);
void terminal_register (Terminal *terminal, ThreadVars *tv);

void spawn_terminal (Terminal *terminal);
void quit_terminal  (Terminal *terminal);

#endif
