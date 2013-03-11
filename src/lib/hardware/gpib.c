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

#include "gpib.h"

#include <stdlib.h>  // free()
#include <stdio.h>  // snprintf(), sscanf()
#define HEADER_SANS_WARNINGS <Python.h>
#include <sans_warnings.h>

#include <config.h>
#include <lib/pile.h>
#include <lib/status.h>
#include <lib/util/str.h>
#include <lib/hardware/timing.h>

#if LINUXGPIB
#include <gpib/ib.h>
#elif NI488
typedef void *PVOID;
typedef char *PCHAR;
typedef wchar_t *PWCHAR;
typedef const char *LPCSTR;
typedef const wchar_t *LPCWSTR;
typedef int *PINT;
typedef short *PSHORT;
#include <ni488.h>
#endif

#define GPIB_ID_WARNING_MSG      "Warning: Board id out of range.\n"
#define GPIB_CONNECT_WARNING_MSG "Warning: Board not connected.\n"
#define GPIB_PAD_WARNING_MSG     "Warning: Primary address out of range.\n"

#define _pyfree_ __attribute__((cleanup(_py_clean_gpib)))
void _py_clean_gpib (PyObject **py_ob);

struct GpibSlot
{
	int pad;
	char *cmd, *reply_fmt, *write_fmt, *dummy_buf;
	PyObject *py_noninv_f, *py_inv_f;
	int cmdlen;

	Timer *timer;
	double dt;

	bool write_request_local, write_request_global;
	bool known_local, known_global;
	double current_local, current_global;

};

struct GpibBoard
{
	char *node;
	bool is_real, is_connected;
	int node_num;

	char *info_driver, *info_full_node, *info_board, *info_board_abrv;

	int dev[M2_GPIB_MAX_PAD];  // >=0: ready, -1: error, -2: not yet set up
	int eos[M2_GPIB_MAX_PAD];

	char buf[M2_GPIB_BUF_LENGTH + 1];
	Pile slots;

};

static struct GpibBoard gpib_board [M2_GPIB_MAX_BRD];

static void gpib_device_connect (int id, int pad);
static void free_slot_cb (struct GpibSlot *slot);

#include "gpib_slot_io.c"

void gpib_init (void)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_GPIB_MAX_BRD; id++)
	{
		gpib_board[id].node = cat1("");
		gpib_board[id].is_real = 0;
		gpib_board[id].is_connected = 0;

		gpib_board[id].info_driver     = cat1("∅");
		gpib_board[id].info_full_node  = cat1("∅");
		gpib_board[id].info_board      = cat1("∅");
		gpib_board[id].info_board_abrv = cat1("∅");

		for (int pad = 0; pad < M2_GPIB_MAX_PAD; pad++)
		{
			gpib_board[id].dev[pad] = -2;
			gpib_board[id].eos[pad] = 0;
		}

		pile_init(&gpib_board[id].slots);
	}
}

void gpib_final (void)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_GPIB_MAX_BRD; id++)
	{
		if (gpib_board[id].is_connected) gpib_board_reset(id);

		replace(gpib_board[id].node, NULL);

		replace(gpib_board[id].info_driver,     NULL);
		replace(gpib_board[id].info_full_node,  NULL);
		replace(gpib_board[id].info_board,      NULL);
		replace(gpib_board[id].info_board_abrv, NULL);

		pile_final(&gpib_board[id].slots, PILE_CALLBACK(free_slot_cb));
	}
}

int gpib_board_connect (int id, const char *node)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, GPIB_ID_WARNING_MSG, return 0);
	f_start(F_UPDATE);

	// clear devices and slots:
	if (gpib_board[id].is_connected) gpib_board_reset(id);

	// update node:
	replace(gpib_board[id].node, cat1(node));

	if (str_equal(node, "dummy"))
	{
		gpib_board[id].is_real = 0;
		gpib_board[id].is_connected = 1;

		replace(gpib_board[id].info_driver,     cat1("Dummy"));
		replace(gpib_board[id].info_full_node,  cat1("N/A"));
		replace(gpib_board[id].info_board,      cat1("<Virtual>"));
		replace(gpib_board[id].info_board_abrv, cat1("<Virt.>"));
	}
	else
	{
		gpib_board[id].is_real = 1;
		gpib_board[id].is_connected = 0;  // assume the worst...

#if LINUXGPIB
		replace(gpib_board[id].info_driver,    cat1("linux-gpib"));
		replace(gpib_board[id].info_full_node, cat1(node));

		if (sscanf(node, "/dev/gpib%d", &gpib_board[id].node_num) == 1)
		{
			int status = ibsic(gpib_board[id].node_num);
			if (!(status & ERR))
			{
				gpib_board[id].is_connected = 1;

				replace(gpib_board[id].info_board,      cat1("?"));  // TODO: find board name
				replace(gpib_board[id].info_board_abrv, cat1("?"));
			}

			f_print(F_VERBOSE, "Info: ibsta after ibsic(): %d\n", status);
		}
		else status_add(0, supercat("GPIB Error: Unusable node string: %s.\n", node));

#elif NI488
		replace(gpib_board[id].info_driver, cat1("NI-488"));

		if (sscanf(node, "%d", &gpib_board[id].node_num) == 1)
		{
			replace(gpib_board[id].info_full_node, supercat("NI-488:%d", gpib_board[id].node_num));

			int status = ibsic(gpib_board[id].node_num);
			if (!(status & ERR))
			{
				gpib_board[id].is_connected = 1;

				replace(gpib_board[id].info_board,      cat1("?"));  // TODO: find board name
				replace(gpib_board[id].info_board_abrv, cat1("?"));
			}

			f_print(F_VERBOSE, "Info: ibsta after ibsic(): %d\n", status);
		}
		else
		{
			replace(gpib_board[id].info_full_node, cat1("NI-488:?"));
			status_add(0, supercat("GPIB Error: Unusable node string: %s.\n", node));
		}

#else
		replace(gpib_board[id].info_driver,    cat1("∅"));
		replace(gpib_board[id].info_full_node, cat1("∅"));
#endif

		if (!gpib_board[id].is_connected)
		{
			replace(gpib_board[id].info_board,      supercat("Failed to open \"%s\".", node));
			replace(gpib_board[id].info_board_abrv, cat1("∅"));
		}
	}

	return gpib_board[id].is_connected;
}

void gpib_board_reset (int id)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, GPIB_ID_WARNING_MSG, return);
	f_start(F_UPDATE);

	for (int pad = 0; pad < M2_GPIB_MAX_PAD; pad++)
	{
		if (gpib_board[id].is_real && gpib_board[id].dev[pad] >= 0)
		{
#if LINUXGPIB || NI488
			ibclr(gpib_board[id].dev[pad]);
			ibonl(gpib_board[id].dev[pad], 0);
#endif
		}

		gpib_board[id].dev[pad] = -2;
		gpib_board[id].eos[pad] = 0;
	}

	pile_gc(&gpib_board[id].slots, PILE_CALLBACK(free_slot_cb));
}

char * gpib_board_info (int id, const char *info)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD, GPIB_ID_WARNING_MSG, return NULL);

	if      (str_equal(info, "driver"))     return gpib_board[id].info_driver;
	else if (str_equal(info, "full_node"))  return gpib_board[id].info_full_node;
	else if (str_equal(info, "board"))      return gpib_board[id].info_board;
	else if (str_equal(info, "board_abrv")) return gpib_board[id].info_board_abrv;
	else                                    return cat1("∅");
}
void gpib_device_connect (int id, int pad)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD,   GPIB_ID_WARNING_MSG,      return);
	f_verify(gpib_board[id].is_connected,       GPIB_CONNECT_WARNING_MSG, return);
	f_verify(pad >= 0 && pad < M2_GPIB_MAX_PAD, GPIB_PAD_WARNING_MSG,     return);
	f_start(F_UPDATE);

	if (gpib_board[id].is_real)
	{
#if LINUXGPIB || NI488
		gpib_board[id].dev[pad] = ibdev(gpib_board[id].node_num, pad, NO_SAD, T1s, 1, gpib_board[id].eos[pad]);

		if (gpib_board[id].dev[pad] >= 0) ibclr(gpib_board[id].dev[pad]);
#else
		gpib_board[id].dev[pad] = -1;
#endif
	}
	else gpib_board[id].dev[pad] = pad;

	if (gpib_board[id].dev[pad] < 0) status_add(0, supercat("Warning: Unable to connect to GPIB device %d via board %d.\n", pad, gpib_board[id].node_num));
}

char * gpib_string_query (int id, int pad, char *cmd, int cmdlen, int expect_reply)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD,   GPIB_ID_WARNING_MSG,      return NULL);
	f_verify(gpib_board[id].is_connected,       GPIB_CONNECT_WARNING_MSG, return NULL);
	f_verify(pad >= 0 && pad < M2_GPIB_MAX_PAD, GPIB_PAD_WARNING_MSG,     return NULL);

	if (gpib_board[id].dev[pad] == -2) gpib_device_connect(id, pad);

	if (gpib_board[id].dev[pad] >= 0)
	{
		long last = 0;
		if (gpib_board[id].is_real)
		{
#if LINUXGPIB
			ibwrt(gpib_board[id].dev[pad], cmd, cmdlen);
#elif NI488
			ibwrt(gpib_board[id].dev[pad], (PVOID) cmd, cmdlen);
#endif

			if (expect_reply != 0)
			{
#if LINUXGPIB || NI488
				ibrd (gpib_board[id].dev[pad], gpib_board[id].buf, M2_GPIB_BUF_LENGTH);
				last = ibcntl;
#endif
			}
		}
		gpib_board[id].buf[last] = '\0';

		return gpib_board[id].buf;
	}
	else return NULL;
}

void gpib_device_set_eos (int id, int pad, int eos)
{
	f_verify(id >= 0 && id < M2_GPIB_MAX_BRD,   GPIB_ID_WARNING_MSG,      return);
	f_verify(gpib_board[id].is_connected,       GPIB_CONNECT_WARNING_MSG, return);
	f_verify(pad >= 0 && pad < M2_GPIB_MAX_PAD, GPIB_PAD_WARNING_MSG,     return);

	gpib_board[id].eos[pad] = eos;
}

void _py_clean_gpib (PyObject **py_ob)
{
	Py_XDECREF(*py_ob);
}
