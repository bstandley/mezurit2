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

void free_slot_cb (struct GpibSlot *slot)
{
	free(slot->cmd);
	free(slot->reply_fmt);
	free(slot->write_fmt);
	free(slot->dummy_buf);
}

int gpib_slot_add (int id, int pad, const char *cmd, double dt, double dummy_value, const char *reply_fmt, const char *write_fmt, void *py_noninv_f, void *py_inv_f)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD,   GPIB_ID_WARNING_MSG,  return -1);
	f_verify(gpib_board[id].is_connected,       NULL,                 return -1);
	f_verify(pad >= 0 && pad < M2_GPIB_MAX_PAD, GPIB_PAD_WARNING_MSG, return -1);

	if (gpib_board[id].dev[pad] == -2) gpib_device_connect(id, pad);

	struct GpibSlot *slot = malloc(sizeof(struct GpibSlot));

	slot->pad = pad;
	slot->cmd = cat1(cmd);
	slot->reply_fmt = cat1(reply_fmt);
	slot->write_fmt = cat1(write_fmt);
	slot->py_noninv_f = py_noninv_f;
	slot->py_inv_f = py_inv_f;
	slot->cmdlen = str_length(cmd);

	if (gpib_board[id].is_real) slot->dummy_buf = NULL;
	else
	{
		slot->dummy_buf = new_str(M2_GPIB_BUF_LENGTH);
		snprintf(slot->dummy_buf, M2_GPIB_BUF_LENGTH, reply_fmt, dummy_value);
	}

	slot->timer = timer_new();
	slot->dt = dt;

	slot->write_request_local = slot->write_request_global = 0;
	slot->known_local         = slot->known_global         = 0;
	slot->current_local       = slot->current_global       = 0.0;
	
	pile_add(&gpib_board[id].slots, slot);
	return (int) gpib_board[id].slots.occupied - 1;  // slot id
}

int gpib_slot_read (int id, int s, double *x)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, NULL, return 0);
	f_verify(gpib_board[id].is_connected,     NULL, return 0);

	struct GpibSlot *slot = pile_item(&gpib_board[id].slots, (size_t) s);
	if (slot == NULL)
	{
		f_print(F_ERROR, "Error: Slot index out of range.\n");
		return 0;
	}
	else if (slot->known_global)
	{
		if (slot->py_noninv_f == NULL) *x = slot->current_global;  // straight through
		else
		{
			PyObject *py_x  _pyfree_ = PyFloat_FromDouble(slot->current_global);
			PyObject *py_rv _pyfree_ = PyObject_CallFunctionObjArgs(slot->py_noninv_f, py_x, NULL);
			
			if (py_rv != NULL) *x = PyFloat_AsDouble(py_rv);
			else return 0;
		}

		return 1;
	}
	else return 0;
}

int gpib_slot_write (int id, int s, double target)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, GPIB_ID_WARNING_MSG, return 0);
	f_verify(gpib_board[id].is_connected,     NULL,                return 0);

	struct GpibSlot *slot = pile_item(&gpib_board[id].slots, (size_t) s);
	if (slot == NULL)
	{
		f_print(F_ERROR, "Error: Slot index out of range.\n");
		return 0;
	}
	else
	{
		if (slot->py_inv_f == NULL) slot->current_global = target;  // straight through
		else
		{
			PyObject *py_x  _pyfree_ = PyFloat_FromDouble(target);
			PyObject *py_rv _pyfree_ = PyObject_CallFunctionObjArgs(slot->py_inv_f, py_x, NULL);

			if (py_rv != NULL) slot->current_global = PyFloat_AsDouble(py_rv);
			else return 0;
		}

		slot->known_global = 1;
		slot->write_request_global = 1;
		return 1;
	}
}

int gpib_multi_tick (int id)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, GPIB_ID_WARNING_MSG, return 0);
	f_verify(gpib_board[id].is_connected,     NULL,                return 0);

	struct GpibSlot *slot = pile_first(&gpib_board[id].slots);
	while (slot != NULL)
	{
		if (slot->write_request_local)
		{
			timer_reset(slot->timer);
			slot->write_request_local = 0;

			if (!gpib_board[id].is_real) snprintf(slot->dummy_buf, M2_GPIB_BUF_LENGTH, slot->reply_fmt, slot->current_local);

			snprintf(gpib_board[id].buf, M2_GPIB_BUF_LENGTH, slot->write_fmt, slot->current_local);
			gpib_string_query(id, slot->pad, gpib_board[id].buf, str_length(gpib_board[id].buf), 0);
		}
		else if (overtime_then_reset(slot->timer, slot->dt))
		{
			gpib_string_query(id, slot->pad, slot->cmd, slot->cmdlen, 1);  // writes to gpib_board[id].buf automatically
			slot->known_local = (sscanf(gpib_board[id].is_real ? gpib_board[id].buf : slot->dummy_buf, slot->reply_fmt, &slot->current_local) == 1);
		}

		slot = pile_inc(&gpib_board[id].slots);
	}

	return 1;
}

void gpib_multi_transfer (int id)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, GPIB_ID_WARNING_MSG, return);
	f_verify(gpib_board[id].is_connected,     NULL,                return);

	struct GpibSlot *slot = pile_first(&gpib_board[id].slots);
	while (slot != NULL)
	{
		if (slot->write_request_global)
		{
			slot->write_request_global = 0;
			slot->write_request_local = 1;

			slot->current_local = slot->current_global;
		}
		else if (!slot->write_request_local)
		{
			slot->known_global = slot->known_local;
			slot->current_global = slot->current_local;
		}

		slot = pile_inc(&gpib_board[id].slots);
	}
}
