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

#ifndef _MAIN_TOOL_SCOPE_H
#define _MAIN_TOOL_SCOPE_H 1

#include <lib/util/mt.h>
#include <main/section.h>
#include <main/tool/buffer.h>

enum
{
	SCOPE_RL_STOP  = -1,
	SCOPE_RL_HOLD  = -2,
	SCOPE_RL_READY = -3,
	SCOPE_RL_SCAN  = -4
};

typedef struct
{
	// private:

		double timescale_rate;  // threads: shared, protected by Scope.mutex (units: kHz)
		double timescale_time;  // threads: shared, protected by Scope.mutex (units: seconds)

		Section sect;
		NumericEntry *rate_entry, *time_entry;
		GtkWidget *status_left, *status_right;

	// public:

		MtMutex mutex;
		GtkWidget *button, *image;

	  	// The following vars are shared between threads, protected by Scope.mutex,
		// with the following exception: Array 'scan' is not locked while scanning
		// because GUI thread is blocked via 'scanning' anyway.

		Scan scan[M2_NUM_DAQ];
		int master_id;  // index of longest Scan (they may differ due to rounding errors)
		bool scanning;  // prevents callbacks while scanning

} Scope;

void scope_init     (Scope *scope, GtkWidget **apt);
void scope_register (Scope *scope, int pid, GtkWidget **apt);
void scope_update   (Scope *scope, ChanSet *chanset);
void scope_final    (Scope *scope);

bool scan_array_start   (Scan *scan_array, Timer *timer);                                      // call from DAQ thread
void scan_array_read    (Scan *scan_array, int *counter, int *read_mult, long *s_read_total);  // call from DAQ thread
void scan_array_stop    (Scan *scan_array, long *s_read_total);                                // call from DAQ thread
void scan_array_process (Scan *scan_array, Buffer *buffer, ChanSet *chanset);                  // call from DAQ thread

void scope_register_legacy (Scope *scope);

void set_scope_runlevel (Scope *scope, int rl);

#endif
