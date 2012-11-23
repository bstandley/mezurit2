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

#ifndef _MAIN_THREAD_ACQUIRE_H
#define _MAIN_THREAD_ACQUIRE_H 1

#include <stdbool.h>
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <glib.h>
#pragma GCC diagnostic warning "-Wsign-conversion"

#include <main/panel.h>

enum
{
	RL_TOGGLE  = 0,
	RL_NO_HOLD = 100
};

typedef struct
{
	// private:

		bool gpib_running;                    // threads: shared by DAQ and GPIB, protected by ThreadVars.gpib_mutex

		double data_daq  [M2_MAX_CHAN];       // threads: DAQ only
		bool   known_daq [M2_MAX_CHAN];       //

		Timer *scope_bench_timer;             // threads: shared, protected by rl_mutex

		int pulse_vci;                        // threads: DAQ only (persists between page switches)
		double pulse_target, pulse_original;  // threads: DAQ only (persists between page switches)

	// private, when including gui*.c:

		GStaticMutex rl_mutex, gpib_mutex, data_mutex;

		int logger_rl, scope_rl;              // threads: shared by DAQ and GUI,  protected by ThreadVars.rl_mutex

		bool  gpib_paused;                    // threads: shared by GPIB and GUI, protected by ThreadVars.gpib_mutex
		int   gpib_id, gpib_pad, gpib_eos;    //
		char *gpib_msg;                       //
		bool  gpib_expect_reply;              //

		// Note: gpib_id, gpib_pad, gpib_eos, gpib_expect_reply are not set until an actual msg request occurs.

		double data_shared  [M2_MAX_CHAN];    // threads: shared, protected by ThreadVars.data_mutex
		bool   known_shared [M2_MAX_CHAN];    // threads: shared, protected by ThreadVars.data_mutex
		double data_gui     [M2_MAX_CHAN];    // threads: GUI only
		bool   known_gui    [M2_MAX_CHAN];    // threads: GUI only

		bool terminal_dirty;                  // threads: shared, implicity protected by the control server (persists between page switches)
		int catch_sweep_ici;                  // threads: shared, implicity protected by the control server (persists between page switches)
		char *catch_signal;                   // threads: shared, implicity protected by the control server (persists between page switches)

		ChanSet *chanset;                     // threads: shared, inherited from Mezurit2, built by run_gui_thread(), thereafter read-only

	// public:

		int pid;                              // threads: shared (read-only)
		Panel *panel;                         // threads: shared (read-only), set by run_gui_thread()

} ThreadVars;

void thread_init_all     (ThreadVars *tv);
void thread_register_daq (ThreadVars *tv);

gpointer run_daq_thread (gpointer data);
void stop_threads (ThreadVars *tv);

void set_recording (ThreadVars *tv, int rl);
bool set_scanning  (ThreadVars *tv, int rl);
int  get_logger_rl (ThreadVars *tv);
int  get_scope_rl  (ThreadVars *tv);

void set_scan_callback_mode (ThreadVars *tv, bool scanning);  // call from any thread

#endif
