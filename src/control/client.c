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

#define HEADER_SANS_WARNINGS <Python.h>
#include <sans_warnings.h>
#define HEADER_SANS_WARNINGS <gio/gio.h>
#include <sans_warnings.h>

#include <lib/util/str.h>
#include <lib/hardware/timing.h>

#include "send_recv.c"

static GSocketAddress *server_skt_addr;

static PyObject * send_recv_rcmd_cfunc (PyObject *py_self, PyObject *py_args);
static PyObject * xleep_cfunc          (PyObject *py_self, PyObject *py_args);

PyMODINIT_FUNC init_mezurit2control (void);

static PyMethodDef control_methods[] =
{
	{"send_recv_rcmd", send_recv_rcmd_cfunc, METH_VARARGS, "Sends command and waits for reply."},
	{"xleep",          xleep_cfunc,          METH_VARARGS, "Sleeps for specified time in seconds (decimals allowed."},
	{NULL,             NULL,                 0,            NULL}
};

PyMODINIT_FUNC init_mezurit2control (void)
{
	PyObject *module = Py_InitModule("_mezurit2control", control_methods);
	PyModule_AddStringConstant(module, "VERSION", quote(VERSION));
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 36
	g_type_init();
#endif

	char *str = getenv("M2_CONTROLPORT");
	int port;
	if (str != NULL && sscanf(str, "%d", &port) == 1)
	{
		printf("\nConnecting to control port %d... ", port);

		GInetAddress *server_inet_addr = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
		server_skt_addr = g_inet_socket_address_new(server_inet_addr, (guint16) port);
		g_object_unref(server_inet_addr);
	}
	else
	{
		printf("\nUnable to find control port...");
		server_skt_addr = NULL;
	}
}

PyObject * send_recv_rcmd_cfunc (PyObject *py_self, PyObject *py_args)
{
	char *cmd = PyString_AsString(PyTuple_GetItem(py_args, 0));
	char *reply _strfree_ = NULL;

	if (server_skt_addr != NULL)
	{
		GSocket *skt = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL);
		g_socket_connect(skt, server_skt_addr, NULL, NULL);

		if (socket_send(skt, cmd)) reply = socket_recv(skt);  // wait here for server to respond

		g_socket_close(skt, NULL);
	}

	return PyString_FromString(reply != NULL ? reply : "");
}

PyObject * xleep_cfunc (PyObject *py_self, PyObject *py_args)
{
	double t = PyFloat_AsDouble(PyTuple_GetItem(py_args, 0));
	if (t >= 0) xleep(t);

	return PyBool_FromLong(t >= 0);
}
