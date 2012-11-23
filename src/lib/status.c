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

#include "status.h"

#include <stdlib.h>  // malloc(), free()
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <glib.h>
#pragma GCC diagnostic warning "-Wsign-conversion"

#include <config.h>
#include <lib/pile.h>
#include <lib/util/str.h>

static int f_mode;
static int f_indent;

static Pile status_pile;  // holds dynamic strings temporarily until status_regenerate() is called, which "steals" them to place in status_msg[]
static bool status_dirty;
static GStaticMutex status_mutex;

static int status_len, status_loc;
static char *status_msg[M2_STATUS_MAX_MSG];

void f_init (const int mode)
{
	f_mode = mode;
	f_indent = 0;
}

void _f_trace (FContext *f_context, const int mode, const char *func)
{
	if (mode & f_mode)
	{
		switch (f_indent)
		{
			case 0 : printf(          "%s():\n", func); break;
			case 1 : printf(        "\t%s():\n", func); break;
			case 2 : printf(      "\t\t%s():\n", func); break;
			case 3 : printf(    "\t\t\t%s():\n", func); break;

			default : for (int i = 0; i < f_indent; i++) printf("\t");
			          printf("%s():\n", func);
		}

		f_indent++;
		f_context->timer = timer_new();
	}
	else f_context->timer = NULL;

	f_context->pile = NULL;
}

void _f_return (FContext *f_context)
{
	if (f_context->pile != NULL)
	{
		pile_final(f_context->pile, NULL);
		free(f_context->pile);
	}

	if (f_context->timer != NULL)
	{
		f_indent--;

		if (F_PROFILE & f_mode)
		{
			double dt = timer_elapsed(f_context->timer) * 1e3;
			switch (f_indent)
			{
				case 0 : printf(      "%0.3f ms\n", dt); break;
				case 1 : printf(    "\t%0.3f ms\n", dt); break;
				case 2 : printf(  "\t\t%0.3f ms\n", dt); break;
				case 3 : printf("\t\t\t%0.3f ms\n", dt); break;

				default : for (int i = 0; i < f_indent; i++) printf("\t");
				          printf("%0.3f ms\n", dt);
			}
		}

		timer_destroy(f_context->timer);
	}
}

void _f_print (const int mode, const char *func, const char *fmt, ...)
{
	if (mode & f_mode)
	{
		switch (f_indent)
		{
			case 0 : printf(          "%s(): ", func); break;
			case 1 : printf(        "\t%s(): ", func); break;
			case 2 : printf(      "\t\t%s(): ", func); break;
			case 3 : printf(    "\t\t\t%s(): ", func); break;

			default : for (int i = 0; i < f_indent; i++) printf("\t");
			          printf("%s(): ", func);
		}

		va_list vl;
		va_start(vl, fmt);
		vprintf(fmt, vl);
		va_end(vl);
	}
}

void * _f_add (FContext *f_context, void *ptr)
{
	if (f_context->pile == NULL)
	{
		f_context->pile = malloc(sizeof(Pile));
		pile_init(f_context->pile);
	}

	return pile_add(f_context->pile, ptr);
}

void _f_gc (FContext *f_context)
{
	if (f_context->pile != NULL) pile_gc(f_context->pile, NULL);
}

void status_init (void)
{
	f_start(F_INIT);

	g_static_mutex_init(&status_mutex);

	status_dirty = 0;
	pile_init(&status_pile);

	status_len = 0;
	status_loc = 0;
	for (int i = 0; i < M2_STATUS_MAX_MSG; i++) status_msg[i] = NULL;
}

void status_final (void)
{
	f_start(F_INIT);

	pile_final(&status_pile, NULL);

	for (int i = 0; i < M2_STATUS_MAX_MSG; i++) replace(status_msg[i], NULL);
}

void status_add (bool timestamp, char *msg_in)
{
	g_static_mutex_lock(&status_mutex);  // protect localtime() as well as status_pile

	char *msg_time _strfree_ = NULL;
	if (timestamp)
	{
		time_t tt = time(NULL);
		struct tm *stm = localtime(&tt);
		msg_time = supercat("%d/%d %d:%02d:%02d  ", stm->tm_mon + 1, stm->tm_mday, stm->tm_hour, stm->tm_min, stm->tm_sec);
	}
	
	pile_add(&status_pile, cat2(msg_time, msg_in));
	status_dirty = 1;

	g_static_mutex_unlock(&status_mutex);

	f_print(F_VERBOSE, "Status: %s", msg_in);
	free(msg_in);
}

bool status_regenerate (void)
{
	bool dirty = 0;

	g_static_mutex_lock(&status_mutex);
	if (status_dirty)
	{
		for (char *msg = pile_first(&status_pile); msg != NULL; msg = pile_inc(&status_pile))
		{
			status_loc++;
			if (status_loc >= M2_STATUS_MAX_MSG) status_loc = 0;

			if (status_msg[status_loc] != NULL) status_len -= str_length(status_msg[status_loc]);  // subtract length of overwritten message
			status_len += str_length(msg);

			replace(status_msg[status_loc], cat1(msg));
		}

		pile_gc(&status_pile, NULL);

		status_dirty = 0;
		dirty = 1;
	}
	g_static_mutex_unlock(&status_mutex);

	return dirty;
}

char * status_get_text (void)
{
	char *buf = new_str(status_len);

	int spot = 0;
	for (int i = 0; i < M2_STATUS_MAX_MSG; i++)
	{
		char *msg = status_msg[(status_loc + i + 1) % M2_STATUS_MAX_MSG];  // +1 to skip over current (last-in) message to beginning
		if (msg != NULL) spot += sprintf(&buf[spot], "%s", msg);
	}
	if (spot > 0 && buf[spot - 1] == '\n') buf[spot - 1] = '\0';

	return buf;
}

char * status_get_last_line (void)
{
	int len = str_length(status_msg[status_loc]);
	char *msg = cat1(status_msg[status_loc]);

	if (len > 0 && msg[len - 1] == '\n') msg[len - 1] = '\0';
	return msg;
}
