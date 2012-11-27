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

#ifndef _MAIN_TOOL_TRIGGER_H
#define _MAIN_TOOL_TRIGGER_H 1

#include <lib/util/mt.h>
#include <lib/hardware/timing.h>
#include <main/section.h>
#include <main/setup/channel.h>

typedef struct
{
	// private:

		GtkWidget  *line_entry [M2_TRIGGER_LINES];  // threads: GUI only (first element is for the "if" expression)
		ComputeFunc line_cf    [M2_TRIGGER_LINES];  // threads: DAQ only
		char       *line_expr  [M2_TRIGGER_LINES];  // threads: shared, protected by Panel.trigger_mutex
		bool        line_dirty [M2_TRIGGER_LINES];  // threads: shared, protected by Panel.trigger_mutex

		bool ready, arm_dirty, busy_dirty;          // threads: shared, protected by Panel.trigger_mutex

		int cur;           // DAQ thread only
		double wait_time;  // DAQ thread only
		Timer *timer;      // DAQ thread only

		GtkWidget *arm_button, *force_button;
		GtkWidget *plus_button;
		GtkWidget *vbox, *hbox1;
		bool adjusted;

	// public:

		bool armed, busy, any_line_dirty;  // threads: shared, protected by Panel.trigger_mutex

} Trigger;

void trigger_array_init     (Trigger *trigger_array, GtkWidget **apt, Section *sect);
void trigger_array_register (Trigger *trigger_array, int pid, GtkWidget **apt, Section *sect, MtMutex *mutex);
void trigger_array_update   (Trigger *trigger_array);
void trigger_array_final    (Trigger *trigger_array);

void trigger_parse (Trigger *trigger);  // call from DAQ thread
void trigger_check (Trigger *trigger);  // call from DAQ thread
void trigger_exec  (Trigger *trigger);  // call from DAQ thread

bool set_trigger_buttons_all (Trigger *trigger_array, MtMutex *mutex);

#endif
