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

#ifndef _LIB_HARDWARE_GPIB_H
#define _LIB_HARDWARE_GPIB_H 1

// configuration

void gpib_init  (void);
void gpib_final (void);

int    gpib_board_connect (int id, const char *node);
char * gpib_board_info    (int id, const char *info);  // returns internal string (do not free)
void   gpib_board_reset   (int id);

void gpib_device_set_eos (int id, int pad, int eos);

// basic operation

char * gpib_string_query (int id, int pad, char *cmd, int cmdlen, int expect_reply);

// asynchronous operation

int gpib_slot_add   (int id, int pad, const char *cmd, double dt, double dummy_value, const char *reply_fmt, const char *write_fmt, void *py_noninv_f, void *py_inv_f);
int gpib_slot_read  (int id, int s, double *x);
int gpib_slot_write (int id, int s, double target);

int  gpib_multi_tick     (int id);
void gpib_multi_transfer (int id);

// Notes on thread safety:
//
//   1) gpib_slot_read() and gpib_slot_write(), and gpib_multi_transfer() should
//      be called one at a time. This is accomplished externally through
//      ThreadVars.gpib_mutex.
//
//   2) gpib_slot_add(), gpib_multi_tick(), and gpib_string_query() should be
//      called one at a time. This is accomplished by parsing the channels during
//      the panel startup phase, when only the GUI thread is running. Channels
//      which contain conditionals may generate calls to gpib_slot_add() during
//      regular operation, which could be bad . . .

#endif
