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

#ifndef _MAIN_SETUP_COMPUTE_H
#define _MAIN_SETUP_COMPUTE_H 1

#include <stdbool.h>

#include <lib/hardware/daq.h>
#include <lib/hardware/gpib.h>

enum
{
	COMPUTE_MODE_PARSE = 0x1 << 0,
	COMPUTE_MODE_SOLVE = 0x1 << 1,
	COMPUTE_MODE_POINT = 0x1 << 2,
	COMPUTE_MODE_TEST  = 0x1 << 3,
	COMPUTE_MODE_SCAN  = 0x1 << 4
};

enum
{
	COMPUTE_LINEAR_INVERSE,
	COMPUTE_LINEAR_NONINVERSE
};

enum
{
	COMPUTE_INVERTIBLE_DAC  = 1,
	COMPUTE_INVERTIBLE_DIO  = 2,
	COMPUTE_INVERTIBLE_GPIB = 3
};

typedef struct
{
	// private:

		double prefactor;
		double y0, dydx;
		int inv_id, inv_chan_slot;  // chan for DAQ/DIO, slot for GPIB
		void *sub_py_f;

		bool parse_other;

	// public:

		void *py_f;    // use void* instead of PyObject* to avoid including Python.h
		void *sub_cf;  // use void* instead of ComputeFunc* for obvious reasons

		int invertible;
		bool scannable, parse_exec;
		char *info;

		int parse_dac [M2_DAQ_MAX_BRD][M2_DAQ_MAX_CHAN];
		int parse_adc [M2_DAQ_MAX_BRD][M2_DAQ_MAX_CHAN];
		int parse_dio [M2_DAQ_MAX_BRD][M2_DAQ_MAX_CHAN];
		int parse_pad [M2_GPIB_MAX_BRD][M2_GPIB_MAX_PAD];

} ComputeFunc;

// Note: No locking is explicitly required for computation, because Python can be used
//       in only one thread at a time anyway without extra precautions (which are not
//       used in Mezurit2). Most functions are called from the DAQ thread, except for
//       init and parsing which is performed in single-threaded mode.

void compute_init  (void);
void compute_reset (void);
void compute_final (void);

void compute_set_context (double *data_ptr, double *prefactor_ptr, int *table_ptr, int length);
void compute_save_context (void);
void compute_restore_context (void);

void   compute_set_pid   (int pid);
void   compute_set_time  (double t);
void   compute_set_point (long j);
void   compute_set_wait  (double t);
double compute_get_wait  (void);

void   compute_func_init      (ComputeFunc *cf);
void   compute_read_expr      (ComputeFunc *cf, const char *expr, double prefactor);
void   compute_sub_define     (ComputeFunc *cf, ComputeFunc *sub_cf, const char *expr);
bool   compute_function_read  (ComputeFunc *cf, int mode, double *value);
bool   compute_function_test  (ComputeFunc *cf, int mode, bool *value);
bool   compute_function_write (ComputeFunc *cf, double value);
double compute_linear_compute (ComputeFunc *cf, int dir, double input);

#endif
