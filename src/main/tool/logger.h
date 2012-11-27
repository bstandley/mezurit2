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

#ifndef _MAIN_TOOL_LOGGER_H
#define _MAIN_TOOL_LOGGER_H 1

#include <lib/util/mt.h>
#include <lib/hardware/timing.h>
#include <main/section.h>
#include <main/setup/channel.h>

enum
{
	LOGGER_RL_STOP   = 1,
	LOGGER_RL_HOLD   = 2,
	LOGGER_RL_IDLE   = 3,
	LOGGER_RL_RECORD = 4,
	LOGGER_RL_WAIT   = 5
};

typedef struct
{
	// private:

		Section sect;
		NumericEntry *max_rate_entry;
		GtkWidget *cbuf_length_widget;
		GtkWidget *reader_labels, *reader_units, *reader_types;
		GtkWidget *reader_values;  // updated by run_reader_status() using buffered data

		guint reader_hash;              // threads: GUI only
		char *reader_str;               // threads: GUI only
		GtkTextBuffer *reader_textbuf;  // threads: GUI only

		int minus_width, period_width, digit_width;

		bool resizer_queued;            // threads: GUI only
		double resizer_delay;           // threads: GUI only
		Timer *resizer_timer;           // threads: GUI only
		int resizer_w0, resizer_w1;     // threads: GUI only

		int block_cbuf_length_cb;       // threads: GUI only

	// public:

		MtMutex mutex;
		GtkWidget *button, *image, *gpib_button;

	  	// The following vars are shared between threads and protected by Logger.mutex.

		double max_rate;  // units: kHz
		int cbuf_length;
		bool max_rate_dirty, cbuf_dirty;  // initialized by DAQ thread

} Logger;

void logger_init     (Logger *logger, GtkWidget **apt);
void logger_register (Logger *logger, int pid, GtkWidget **apt);
void logger_update   (Logger *logger, ChanSet *chanset);
void logger_final    (Logger *logger);

void run_reader_status   (Logger *logger, ChanSet *chanset, bool *known, double *data);
void set_logger_runlevel (Logger *logger, int rl);
void set_logger_scanning (Logger *logger, bool scanning);

void logger_register_legacy (Logger *logger);

#endif
