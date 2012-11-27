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

#ifndef _MAIN_TOOL_SWEEP_H
#define _MAIN_TOOL_SWEEP_H 1

#include <lib/entry.h>
#include <lib/util/mt.h>
#include <main/section.h>
#include <main/setup/channel.h>

typedef struct
{
	// private:

		MtMutex *mutex;  // inherited from Panel

		bool vis_sweep, vis_jump;  // threads: GUI only
		double jump_voltage;       // threads: GUI only, but protected with Panel.sweep_mutex anyway

		Section sect;
		GtkWidget *name_entry;
		GtkWidget *sweep_section, *jump_section, *hsep;
		GtkWidget *vis_sweep_button, *vis_jump_button;
		GtkWidget *stop_button, *down_button, *up_button;
		GtkWidget *endstop_button[2], *zerostop_button[2];

		NumericEntry *jump_voltage_entry, *jump_scaled_entry;
		GtkWidget *jump_scaled_label;

		int request_dir;             // threads: shared, protected by Panel.sweep_mutex
		bool request_hold, pending;  // threads: shared, protected by Panel.sweep_mutex
		void *follower;              // threads: shared, protected by Panel.sweep_mutex

	// public:

		Channel *channel;

	  	// The following vars are shared between threads and protected by Panel.sweep_mutex,
		// except for the GTK members of the NumericRanges.

		NumericRange voltage, scaled, rate, hold;
		NumericRange step, dwell, blackout;  // dwell units: msec, blackout unites: percent
		bool endstop[2], zerostop[2];
		double jump_scaled;

		GtkWidget *jump_button;

		int dir;
		bool dir_dirty, jump_dirty;
		bool holding, scanning;

} Sweep;

void sweep_array_init     (Sweep *sweep_array, GtkWidget **apt);
void sweep_array_register (Sweep *sweep_array, int pid, GtkWidget **apt, ChanSet *chanset);
void sweep_array_update   (Sweep *sweep_array, ChanSet *chanset);
void sweep_array_final    (Sweep *sweep_array);

void request_sweep_dir (Sweep *sweep, int dir, bool hold);
void exec_sweep_dir    (Sweep *sweep);

void set_sweep_scanning_all (Sweep *sweep_array, bool scanning);
bool set_sweep_buttons_all  (Sweep *sweep_array, int N_inv_chan);

void sweep_register_legacy (Sweep *sweep, const char *prefix);

#endif
