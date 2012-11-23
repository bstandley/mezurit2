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

#include "server.h"

#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <gio/gio.h>
#pragma GCC diagnostic warning "-Wsign-conversion"
#include <stdlib.h>  // malloc()
#include <stdio.h>   // sscanf()
#include <string.h>  // strlen()

#include <lib/pile.h>
#include <lib/status.h>
#include <lib/util/str.h>

#include "send_recv.c"

#define SERVER_ID_WARNING_MSG "Warning: Control server id out of range.\n"

typedef struct
{
	char *cmd;
	int mask;
	Blob blob;

} ControlSignal;

typedef struct
{
	GSocket *perm_socket, *temp_socket;
	GStaticMutex mutex;
	Pile signals;
	gchar **argv;
	char *last_reply;

} Server;

static Server server[M2_CONTROL_MAX_SERVER];

static void free_control_signal_cb (ControlSignal *rs);

int all_pid (int code)
{
	int rv = 0x0;
	for (int pid = 0; pid < M2_NUM_PANEL; pid++) rv = rv | (code << pid);
	return rv;
}

void control_init (void)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_CONTROL_MAX_SERVER; id++)
	{
		server[id].perm_socket = NULL;
		server[id].argv = NULL;
		server[id].last_reply = NULL;

		g_static_mutex_init(&server[id].mutex);
		pile_init(&server[id].signals);
	}
}

void control_final (void)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_CONTROL_MAX_SERVER; id++)
	{
		if (server[id].perm_socket != NULL)
		{
			g_socket_close(server[id].perm_socket, NULL);
			g_object_unref(server[id].perm_socket);
		}

		g_strfreev(server[id].argv);
		pile_final(&server[id].signals, PILE_CALLBACK(free_control_signal_cb));
	}
}

void free_control_signal_cb (ControlSignal *rs)
{
	replace(rs->cmd, NULL);
}

int control_server_listen (int id)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return 0);
	f_start(F_INIT);

	if (server[id].perm_socket != NULL)
	{
		g_socket_close(server[id].perm_socket, NULL);
		g_object_unref(server[id].perm_socket);
	}

	server[id].perm_socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL);

	GInetAddress *inet_addr = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
	GSocketAddress *request_addr = g_inet_socket_address_new(inet_addr, 0);

	g_socket_bind(server[id].perm_socket, request_addr, 1, NULL);
	g_socket_listen(server[id].perm_socket, NULL);

	GSocketAddress *perm_addr = g_socket_get_local_address(server[id].perm_socket, NULL);
	int port = g_inet_socket_address_get_port((GInetSocketAddress *) perm_addr);

	g_object_unref(inet_addr);
	g_object_unref(request_addr);
	g_object_unref(perm_addr);

	return port;
}

bool control_server_poll (int id)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return 0);

	if (server[id].argv == NULL && g_socket_condition_check(server[id].perm_socket, G_IO_IN))
	{
		server[id].temp_socket = g_socket_accept(server[id].perm_socket, NULL, NULL);

		char *cmd_full _strfree_ = socket_recv(server[id].temp_socket);  // get command from client
		if (cmd_full != NULL)
		{
			f_print(F_CONTROL, "Received control command: %s\n", cmd_full);

			server[id].argv = g_strsplit(cmd_full, ";", M2_CONTROL_MAX_ARG + 1);
			return 1;
		}
	}

	return 0;
}

void control_server_reply (int id, const char *str)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return);
	f_start(F_CONTROL);

	if (server[id].perm_socket != NULL)
	{
		socket_send(server[id].temp_socket, str);
		g_socket_close(server[id].temp_socket, NULL);
		g_object_unref(server[id].temp_socket);
	}
	else replace(server[id].last_reply, cat1(str));

	g_strfreev(server[id].argv);
	server[id].argv = NULL;
}

void control_server_lock (int id)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return);
	g_static_mutex_lock(&server[id].mutex);
}

void control_server_unlock (int id)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return);
	g_static_mutex_unlock(&server[id].mutex);
}

void control_server_connect (int id, const char *cmd, int mask, BlobCallback cb, int sig, ...)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return);

	ControlSignal *cs = pile_add(&server[id].signals, malloc(sizeof(ControlSignal)));
	cs->cmd = cat1(cmd);
	cs->mask = mask;
	fill_blob(&cs->blob, cb, sig);
}

bool control_server_iterate (int id, int code)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return 0);
	f_start(F_CONTROL);
	Timer *timer _timerfree_ = timer_new();

	ControlSignal *cs = pile_first(&server[id].signals);
	while (cs != NULL)
	{
		if ((code & cs->mask) && str_equal(server[id].argv[0], cs->cmd))
		{
			Blob *blob = &cs->blob;
			char *reply _strfree_ = NULL;
#define _ACTION reply =
#define _TYPE char*
#define _CAST gchar**
#define _CALL server[id].argv
			blob_switch();
#undef _ACTION
#undef _TYPE
#undef _CAST
#undef _CALL
			if (reply != NULL) control_server_reply(id, reply);

			f_print(F_CONTROL, "match signal, dt = %f us\n", 1e6 * timer_elapsed(timer));
			return 1;
		}

		cs = pile_inc(&server[id].signals);
	}

	f_print(F_CONTROL, "unmatched signal, dt = %f us\n", 1e6 * timer_elapsed(timer));
	return 0;
}

bool upload_cmd_full (int id, const char *str)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return 0);

	if (server[id].argv == NULL)
	{
		server[id].argv = g_strsplit(str, ";", M2_CONTROL_MAX_ARG + 1);
		return 1;
	}
	else return 0;
}

char * download_last_reply (int id)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return NULL);

	char *rv = cat1(server[id].last_reply);
	replace(server[id].last_reply, NULL);
	return rv;
}

char * get_cmd (int id)
{
	f_verify(id >= 0 && id < M2_CONTROL_MAX_SERVER, SERVER_ID_WARNING_MSG, return NULL);
	return (server[id].argv != NULL) ? server[id].argv[0] : NULL;
}

#define parse_arg()                                                             \
    gchar **list __attribute__((cleanup(arg_clean))) = g_strsplit(arg, "|", 3); \
    if (list[0] == NULL || list[1] == NULL ||                                   \
       (name != NULL && !str_equal(list[0], name))) return 0;

static void arg_clean (gchar ***list);
void arg_clean (gchar ***list)
{
	g_strfreev(*list);
}

bool scan_arg_bool (const char *arg, const char *name, bool *var)
{
	parse_arg();
	int x;
	bool rv = (sscanf(list[1], "%d", &x) == 1);
	if (rv) *var = (x != 0);
	return rv;
}

bool scan_arg_int (const char *arg, const char *name, int *var)
{
	parse_arg();
	return (sscanf(list[1], "%d", var) == 1);
}

bool scan_arg_long (const char *arg, const char *name, long *var)
{
	parse_arg();
	return (sscanf(list[1], "%ld", var) == 1);
}

bool scan_arg_double (const char *arg, const char *name, double *var)
{
	parse_arg();
	return (sscanf(list[1], "%lf", var) == 1);
}

bool scan_arg_string (const char *arg, const char *name, char **var)
{
	parse_arg();
	*var = cat1(list[1]);
	return 1;
}
