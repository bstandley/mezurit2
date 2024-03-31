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

#ifndef _MAIN_TOOL_BUFFER_H
#define _MAIN_TOOL_BUFFER_H 1

#include <lib/util/mt.h>
#include <lib/hardware/timing.h>
#include <lib/varset/setvarset.h>
#include <main/setup/channel.h>
#include <main/plot/plot.h>

typedef struct
{
	// private:

		MtMutex confirming;

		bool *save_header, *save_mcf, *save_scr;  // threads: shared (read only), inherited from Bufmenu, TODO: use save_mcf, save_scr

		GtkWidget *tzero_button, *clear_button;
		GtkWidget *add_button, *save_button;
		GtkWidget *file_entry, *file_button;
		GtkWidget *main_window;                   // initialized by buffer_init(), needed for popup dialogs

		bool displayed_empty, displayed_filling;  // threads: GUI only

		int percent;                              // threads: shared (used for scan progress)
		char *filename;                           // threads: GUI only

	// public:

		MtMutex mutex;

		bool *link_tzero;  // threads: GUI only, inherited from Bufmenu

	  	// The following vars are shared between threads and protected by Buffer.mutex,
		// except for 'locked' which is only accessed by the GUI thread.

		SVSP svs;
		Timer *timer;
		bool do_time_reset;
		bool locked;

} Buffer;

void buffer_init     (Buffer *buffer, GtkWidget *parent_hbox);
void buffer_register (Buffer *buffer, int pid, ChanSet *chanset);
void buffer_update   (Buffer *buffer, ChanSet *chanset);
void buffer_final    (Buffer *buffer);

bool clear_buffer (Buffer *buffer, ChanSet *chanset, bool confirm, bool tzero);
bool add_set      (Buffer *buffer, ChanSet *chanset);  // needs outer locks
VSP  active_vsp   (Buffer *buffer);

void set_buffer_buttons (Buffer *buffer, bool empty, bool filling);
void set_scan_progress (Buffer *buffer, double frac);  // call from DAQ thread

// empty:   buffer is completely empty, including last_vs (the only set)
// primed:  last_vs is empty but previous sets have data
// filling: buffer has data and is currently filling last_vs ("opposite" of primed)

#endif
