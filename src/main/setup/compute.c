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

#include "compute.h"

#define HEADER_SANS_WARNINGS <Python.h>
#include <sans_warnings.h>

#include <config.h>
#include <lib/status.h>
#include <lib/pile.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <lib/util/num.h>
#include <control/server.h>

#define _pyfree_ __attribute__((cleanup(_py_clean_compute)))
void _py_clean_compute (PyObject **py_ob);

typedef struct
{
	double *data, *prefactor;  // indexed with vci
	int *vci_table;            // indexed with vc
	int length;                // length of vci_table

} ComputeContext;

static int    compute_pid;
static double compute_time;
static long   compute_point;
static double compute_wait;

static int compute_mode;
static bool compute_known;
static ComputeFunc *compute_cf;
static double compute_x;

static ComputeContext compute_context;
static ComputeContext compute_context_backup;

PyObject * lambda (const char *expr, bool use_x);

#include "compute_cfunc.c"

static PyMethodDef compute_methods[] =
{
	{"panel",           panel_cfunc,           METH_VARARGS, "Returns the current panel's index."},
	{"time",            time_cfunc,            METH_VARARGS, "Returns the current time in seconds."},
	{"ch",              ch_cfunc,              METH_VARARGS, "Returns the specified channel's value."},
	{"ADC",             ADC_cfunc,             METH_VARARGS, "Returns the specified ADC channel's value in Volts."},
	{"DAC",             DAC_cfunc,             METH_VARARGS, "Returns the specified DAC channel's value in Volts."},
	{"gpib_slot_add",   gpib_slot_add_cfunc,   METH_VARARGS, "Registers a gpib slot."},
	{"gpib_slot_read",  gpib_slot_read_cfunc,  METH_VARARGS, "Reads a gpib slot."},
	{"send_recv_local", send_recv_local_cfunc, METH_VARARGS, "Sends a local server command and receives the reply."},
	{"wait",            wait_cfunc,            METH_VARARGS, "Tells a trigger to wait for the specifed interval in seconds."},
	{NULL,              NULL,                  0,            NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct module_state _state;
static struct PyModuleDef moduledef =
{
	PyModuleDef_HEAD_INIT, "_mezurit2compute", NULL, sizeof(struct module_state),
	compute_methods, NULL, NULL, NULL, NULL
};
#endif

void compute_init (void)
{
	f_start(F_INIT);

	Py_NoSiteFlag = 1;
	Py_OptimizeFlag = 1;
	Py_DontWriteBytecodeFlag = 1;
	Py_Initialize();

#if PY_MAJOR_VERSION >= 3
	PyModule_Create(&moduledef);
#else
	Py_InitModule("_mezurit2compute", compute_methods);
#endif

	PyObject *main_module = PyImport_AddModule("__main__");
	PyObject *main_dict = PyModule_GetDict(main_module);

	PyDict_SetItemString(main_dict, "m2_libpath",         PyString_FromString(atg(libpath(NULL))));
	PyDict_SetItemString(main_dict, "m2_user_compute_py", PyString_FromString(atg(configpath("compute.py"))));

	PyRun_SimpleString("from sys import path");
	PyRun_SimpleString("path.append(m2_libpath)");
	PyRun_SimpleString("from mezurit2compute import *");
	PyRun_SimpleString("execfile(m2_user_compute_py)");
}

void compute_reset (void)
{
	PyRun_SimpleString("reset_gpib()");
}

void compute_final (void)
{
	Py_Finalize();
}

void compute_set_point (long j)
{
	compute_point = j;
}

void compute_set_time (double t)
{
	compute_time = t;
}

void compute_func_init (ComputeFunc *cf)
{
	f_start(F_VERBOSE);

	cf->py_f = NULL;
	cf->info = NULL;

	cf->sub_cf = NULL;
	cf->sub_py_f = NULL;
}

PyObject * lambda (const char *expr, bool use_x)
{
	// parse function:
	PyObject *main_module = PyImport_AddModule("__main__");
	PyObject *global_dict = PyModule_GetDict(main_module);

	char *str _strfree_ = supercat("lambda %s: float(%s)", use_x ? "x" : "", expr);
	PyObject *py_f = PyRun_String(str, Py_eval_input, global_dict, Py_BuildValue("{}"));

	if (py_f != NULL)
	{
		// test function:
		PyObject *py_x  _pyfree_ = use_x ? PyFloat_FromDouble(1.0) : NULL;
		PyObject *py_rv _pyfree_ = PyObject_CallFunctionObjArgs(py_f, py_x, NULL);

		if (py_rv == NULL || !PyFloat_Check(py_rv))
		{
			Py_XDECREF(py_f);
			py_f = NULL;
		}
	}

	if (py_f == NULL)
	{
		status_add(0, supercat("Warning: \'%s\' is not a valid Python expression.\n", expr));
		PyErr_Clear();
	}

	return py_f;
}

void compute_read_expr (ComputeFunc *cf, const char *expr, double prefactor)
{
	f_start(F_VERBOSE);

	Py_XDECREF(cf->py_f);
	cf->py_f = NULL;
	replace(cf->info, cat1(""));

	// reset parsing info:
	cf->parse_other = 0;
	cf->parse_exec = 0;
	array_set(cf->parse_pad, M2_GPIB_MAX_BRD, M2_GPIB_MAX_PAD, 0);
	array_set(cf->parse_dac, M2_DAQ_MAX_BRD,  M2_DAQ_MAX_CHAN, 0);
	array_set(cf->parse_adc, M2_DAQ_MAX_BRD,  M2_DAQ_MAX_CHAN, 0);

	if (str_length(expr) > 0)
	{
		// parse, test, and record mentioned channels:
		compute_mode = COMPUTE_MODE_PARSE;
		compute_cf = cf;
		cf->py_f = lambda(expr, 0);
		cf->prefactor = prefactor;
	}

	// check for invertibility and linearize:
	cf->invertible = 0;

	int N_gpib_slot, N_dac_chan, N_adc_chan;
	array_sum(cf->parse_pad, M2_GPIB_MAX_BRD, M2_GPIB_MAX_PAD, &N_gpib_slot);
	array_sum(cf->parse_dac, M2_DAQ_MAX_BRD,  M2_DAQ_MAX_CHAN, &N_dac_chan);
	array_sum(cf->parse_adc, M2_DAQ_MAX_BRD,  M2_DAQ_MAX_CHAN, &N_adc_chan);

	if (cf->py_f != NULL && !cf->parse_other && !cf->parse_exec && N_adc_chan == 0 && (N_dac_chan == 1 || N_gpib_slot == 1))
	{
		double z0 = 0, z1 = 0;

		compute_x = 0;
		compute_function_read(cf, COMPUTE_MODE_SOLVE, &z0);
		compute_x = 1;
		compute_function_read(cf, COMPUTE_MODE_SOLVE, &z1);

		f_print(F_VERBOSE, "y0: %f, y1: %f\n", z0, z1);

		if (fabs(z1 - z0) > M2_COMPUTE_EPSILON)
		{
			cf->invertible = (N_dac_chan == 1)  ? COMPUTE_INVERTIBLE_DAC :
			                 (N_gpib_slot == 1) ? COMPUTE_INVERTIBLE_GPIB : 0;
			cf->y0 = z0;
			cf->dydx = z1 - z0;

			f_print(F_VERBOSE, "y0: %f, dydx: %f\n", cf->y0, cf->dydx);
		}
	}

	// check for scanability (all ADCs on the same board):
	cf->scannable = 1;

	int scan_brd_id = -1;
	for (int id = 0; id < M2_DAQ_MAX_BRD; id++) for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++)
		if (cf->parse_adc[id][chan] > 0)
		{
			if      (scan_brd_id == -1) { scan_brd_id = id;         }
			else if (scan_brd_id != id) { cf->scannable = 0; break; }
		}
}

void compute_sub_define (ComputeFunc *cf, ComputeFunc *sub_cf, const char *expr)
{
	Py_XDECREF(cf->sub_py_f);
	cf->sub_py_f = (sub_cf != NULL && str_length(expr) > 0) ? lambda(expr, 1) : NULL;
	cf->sub_cf = (cf->sub_py_f != NULL) ? sub_cf : NULL;
}

bool compute_function_write (ComputeFunc *cf, double value)
{
	bool rv = 0;
	if (cf->py_f != NULL && cf->invertible != 0)
	{
		double physical = compute_linear_compute(cf, COMPUTE_LINEAR_INVERSE, value);

		if      (cf->invertible == COMPUTE_INVERTIBLE_DAC)  rv = (daq_AO_write    (cf->inv_id, cf->inv_chan_slot, physical) == 1);
		else if (cf->invertible == COMPUTE_INVERTIBLE_GPIB) rv = (gpib_slot_write (cf->inv_id, cf->inv_chan_slot, physical) == 1);

		if (rv && cf->sub_cf != NULL)
		{
			PyObject *py_x  _pyfree_ = PyFloat_FromDouble(value);
			PyObject *py_rv _pyfree_ = PyObject_CallFunctionObjArgs(cf->sub_py_f, py_x, NULL);

			if (py_rv != NULL) rv = compute_function_write(cf->sub_cf, PyFloat_AsDouble(py_rv));
		}
	}

	return rv;
}

bool compute_function_read (ComputeFunc *cf, int mode, double *value)
{
	// Note: compute_function_read takes about 0.6 us for a simple expression on a 2 GHz core2 machine (using old Guile system)

	if (cf->py_f != NULL)
	{
		compute_mode = mode;
		compute_known = 1;
		PyObject *py_rv _pyfree_ = PyObject_CallObject(cf->py_f, NULL);

		if (py_rv != NULL)
		{
			*value = PyFloat_AsDouble(py_rv) / cf->prefactor;
			return compute_known;
		}
	}

	*value = 0;
	return 0;
}

bool compute_function_test (ComputeFunc *cf, int mode, bool *value)
{
	if (cf->py_f != NULL)
	{
		compute_mode = mode;
		compute_known = 1;
		PyObject *py_rv _pyfree_ = PyObject_CallObject(cf->py_f, NULL);

		if (py_rv != NULL)
		{
			*value = (PyInt_AsLong(py_rv) == 1);
			return compute_known;
		}
	}

	*value = 0;
	return 0;
}

double compute_linear_compute (ComputeFunc *cf, int dir, double input)
{
	return (dir == COMPUTE_LINEAR_INVERSE)    ? (input - cf->y0) / cf->dydx :
	       (dir == COMPUTE_LINEAR_NONINVERSE) ? cf->y0 + cf->dydx * input   : 0;
}

void compute_set_context (double *data_ptr, double *prefactor_ptr, int *table_ptr, int length)
{
	compute_context.data      = data_ptr;
	compute_context.prefactor = prefactor_ptr;
	compute_context.vci_table = table_ptr;
	compute_context.length    = length;
}

void compute_save_context (void)
{
	compute_context_backup = compute_context;
}

void compute_restore_context (void)
{
	compute_context = compute_context_backup;
}

void compute_set_pid (int pid)
{
	f_start(F_UPDATE);

	compute_pid = pid;
}

void compute_set_wait (double t)
{
	compute_wait = t;
}

double compute_get_wait (void)
{
	return compute_wait;
}

void _py_clean_compute (PyObject **py_ob)
{
	Py_XDECREF(*py_ob);
}
