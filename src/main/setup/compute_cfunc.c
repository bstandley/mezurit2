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

static PyObject * panel_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * time_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * ch_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * ADC_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * DAC_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * gpib_slot_add_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * gpib_slot_read_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * send_recv_local_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * wait_cfunc (PyObject *py_self, PyObject *py_args);

PyObject * panel_cfunc (PyObject *py_self, PyObject *py_args)
{
	return PyLong_FromLong((long) compute_pid);
}

PyObject * time_cfunc (PyObject *py_self, PyObject *py_args)
{
	double t = 0;

	if      (compute_mode & (COMPUTE_MODE_POINT | COMPUTE_MODE_SCAN)) t = compute_time;
	else if (compute_mode & COMPUTE_MODE_PARSE) compute_cf->parse_other = 1;

	return PyFloat_FromDouble(t);
}

PyObject * ch_cfunc (PyObject *py_self, PyObject *py_args)
{
	int chan = -1;
	PyArg_ParseTuple(py_args, "i", &chan);

	double x = 0;
	if (compute_mode & (COMPUTE_MODE_POINT | COMPUTE_MODE_SCAN))
	{
		if (chan >= 0 && chan < compute_context.length)
		{
			int vci = compute_context.vci_table[chan];
			x = compute_context.prefactor[vci] * compute_context.data[vci];
		}
		else compute_known = 0;
	}
	else if (compute_mode & COMPUTE_MODE_PARSE) compute_cf->parse_other = 1;

	return PyFloat_FromDouble(x);
}

PyObject * ADC_cfunc (PyObject *py_self, PyObject *py_args)
{
	int id = -1, chan = -1;
	PyArg_ParseTuple(py_args, "ii", &id, &chan);

	double x = 0;

	if       (compute_mode & COMPUTE_MODE_POINT) { if (daq_AI_read(id, chan, &x) != 1) compute_known = 0; }
	else if  (compute_mode & COMPUTE_MODE_SCAN)  { daq_AI_convert(id, chan, compute_point, &x); }
	else if ((compute_mode & COMPUTE_MODE_PARSE) && daq_AI_valid(id, chan))
	{
		compute_cf->parse_adc[id][chan]++;
		replace(compute_cf->info, supercat("%s\n   DAQ %d, ADC %d", compute_cf->info, id, chan));
	}

	return PyFloat_FromDouble(x);
}

PyObject * DAC_cfunc (PyObject *py_self, PyObject *py_args)
{
	int id = -1, chan = -1;
	PyArg_ParseTuple(py_args, "ii", &id, &chan);

	double x = 0;

	if      (compute_mode & COMPUTE_MODE_POINT) { if (daq_AO_read(id, chan, &x) != 1) compute_known = 0; }
	else if (compute_mode & COMPUTE_MODE_SCAN)  { daq_AO_convert(id, chan, compute_point, &x); }
	else if (compute_mode & COMPUTE_MODE_SOLVE)
	{
		if (daq_AO_valid(id, chan)) x = compute_x;
		else                        compute_known = 0;
	}
	else if ((compute_mode & COMPUTE_MODE_PARSE) && daq_AO_valid(id, chan))
	{
		compute_cf->parse_dac[id][chan]++;
		compute_cf->inv_id = id;
		compute_cf->inv_chan_slot = chan;
		replace(compute_cf->info, supercat("%s\n   DAQ %d, DAC %d", compute_cf->info, id, chan));
	}

	return PyFloat_FromDouble(x);
}

PyObject * gpib_slot_add_cfunc (PyObject *py_self, PyObject *py_args)
{
	f_start(F_NONE);

	int      id           = (int) PyLong_AsLong (PyTuple_GetItem(py_args, 0));
	int      pad          = (int) PyLong_AsLong (PyTuple_GetItem(py_args, 1));
	int      eos          = (int) PyLong_AsLong (PyTuple_GetItem(py_args, 2));
	char     *intro       = PyString_AsString   (PyTuple_GetItem(py_args, 3));
	char     *cmd         = PyString_AsString   (PyTuple_GetItem(py_args, 4));
	double   period       = PyFloat_AsDouble    (PyTuple_GetItem(py_args, 5));
	double   dummy_value  = PyFloat_AsDouble    (PyTuple_GetItem(py_args, 6));
	char     *reply_fmt   = PyString_AsString   (PyTuple_GetItem(py_args, 7));
	char     *write_fmt   = PyString_AsString   (PyTuple_GetItem(py_args, 8));
	PyObject *py_noninv_f =                      PyTuple_GetItem(py_args, 9);
	PyObject *py_inv_f    =                      PyTuple_GetItem(py_args, 10);

	if (!PyCallable_Check(py_noninv_f)) py_noninv_f = NULL;
	if (!PyCallable_Check(py_inv_f))    py_inv_f    = NULL;

	if (eos != 0) gpib_device_set_eos(id, pad, eos);
	int s = gpib_slot_add(id, pad, cmd, period, dummy_value, reply_fmt, write_fmt, py_noninv_f, py_inv_f);

	if ((compute_mode & COMPUTE_MODE_PARSE) && (s >= 0) && (str_length(write_fmt) > 0))
	{
		compute_cf->parse_pad[id][pad]++;
		compute_cf->inv_id = id;
		compute_cf->inv_chan_slot = s;

		char *idn = (str_length(intro) > 0) ? gpib_string_query(id, pad, intro, str_length(intro), 1) : NULL;
		char *str_idn = atg(idn != NULL ? supercat("%s / ", atg(str_strip_end(idn, "\n\r"))) : cat1(""));

		replace(compute_cf->info, supercat("%s\n   GPIB %d, PAD %d: %s%s", compute_cf->info, id, pad, str_idn, atg(str_strip_end(cmd, "?"))));
	}

	return PyLong_FromLong((long) s);
}

PyObject * gpib_slot_read_cfunc (PyObject *py_self, PyObject *py_args)
{
	int id = -1, s = -1;
	PyArg_ParseTuple(py_args, "ii", &id, &s);

	double x = 0;

	if      (compute_mode & (COMPUTE_MODE_POINT | COMPUTE_MODE_SCAN)) { if (gpib_slot_read(id, s, &x) == 0) compute_known = 0; }
	else if (compute_mode & COMPUTE_MODE_SOLVE)                       { x = compute_x;                                         }

	return PyFloat_FromDouble(x);
}

PyObject * send_recv_local_cfunc (PyObject *py_self, PyObject *py_args)
{
	char *cmd_full = PyString_AsString(PyTuple_GetItem(py_args, 0));

	if (compute_mode & COMPUTE_MODE_PARSE) compute_cf->parse_exec = 1;
	else if (upload_cmd_full(M2_LS_ID, cmd_full))
	{
		control_server_iterate(M2_LS_ID, M2_CODE_DAQ << compute_pid);
		char *reply_full _strfree_ = download_last_reply(M2_LS_ID);
		PyObject *rv = PyString_FromString(reply_full);
		return rv;
	}

	return PyString_FromString("a");
}

PyObject * wait_cfunc (PyObject *py_self, PyObject *py_args)
{
	double t = PyFloat_AsDouble(PyTuple_GetItem(py_args, 0));

	if (compute_mode & COMPUTE_MODE_PARSE) compute_cf->parse_exec = 1;
	else compute_wait = t;

	return PyBool_FromLong(1);
}
