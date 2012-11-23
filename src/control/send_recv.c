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

static bool   socket_send (GSocket *skt, const char *str);
static char * socket_recv (GSocket *skt);

bool socket_send_sub (GSocket *skt, const void *ptr, size_t len);
bool socket_recv_sub (GSocket *skt, void *ptr,       size_t len);

bool socket_send (GSocket *skt, const char *str)
{
	size_t len = strlen(str), llen = sizeof(size_t);
	if (socket_send_sub(skt, &len, llen))
		return socket_send_sub(skt, str, len + 1);

	return 0;
}

char * socket_recv (GSocket *skt)
{
	size_t len, llen = sizeof(size_t);
	if (socket_recv_sub(skt, &len, llen))
	{
		char *str = new_str((int) len);
		if (!socket_recv_sub(skt, str, len + 1)) replace(str, NULL);

		return str;
	}

	return NULL;
}

bool socket_send_sub (GSocket *skt, const void *ptr, size_t len)
{
	return g_socket_send(skt, ptr, len, NULL, NULL) == (gssize) len;
}

bool socket_recv_sub (GSocket *skt, void *ptr, size_t len)
{
	return g_socket_receive(skt, ptr, len, NULL, NULL) == (gssize) len;
}
