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

#ifndef _MAIN_SETUP_HARDWARE_H
#define _MAIN_SETUP_HARDWARE_H 1

#include <main/section.h>

typedef struct  // Note: All members are accessed by only the GUI thread.
{
	// private:

		int type;          // HW_DAQ or HW_GPIB
		int id;            // board id to use with *_board_connect()
		bool real, dummy;  // real is set once at init, dummy can change in callbacks
		int settle;        // DAQ-only
		char *node[4];

		Section sect;
		GtkWidget *mini_entry, *mini_spacer;
		GtkWidget *node_entry, *dummy_button;
		GtkTextBuffer *textbuf;

		bool block_update, dirty;  // Used to prevent calls to *_board_connect() until all
		                           // config lines have been processed. For example, the
		                           // node string could be garbage but dummy=1 so it's OK.

} Hardware;

void hardware_array_init     (Hardware *hw_array, GtkWidget **apt);
void hardware_array_register (Hardware *hw_array, GtkWidget **apt);
void hardware_array_final    (Hardware *hw_array);

void hardware_array_block   (Hardware *hw_array);
void hardware_array_unblock (Hardware *hw_array);

void hardware_register_legacy (Hardware *hw);

#endif
