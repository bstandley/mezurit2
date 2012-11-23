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

#ifndef _CONTROL_SERVER_H
#define _CONTROL_SERVER_H 1

#include <stdbool.h>
#include <lib/blob.h>

enum  // Note: Can handle no more than 10 panels (pid <= 9).
{
	M2_CODE_SETUP = 0x1 << 0,  // setup loop
	M2_CODE_GUI   = 0x1 << 1,  // thread
	M2_CODE_DAQ   = 0x1 << 11  // thread
};

enum
{
	M2_TS_ID = 0,  // terminal
	M2_LS_ID = 1   // local
};

void control_init  (void);
void control_final (void);

int all_pid (int code);
void control_server_connect (int id, const char *cmd, int mask, BlobCallback cb, int sig, ...);

int  control_server_listen  (int id);
bool control_server_poll    (int id);
void control_server_reply   (int id, const char *str);
bool control_server_iterate (int id, int code);

void control_server_lock   (int id);
void control_server_unlock (int id);

bool   upload_cmd_full     (int id, const char *str);
char * download_last_reply (int id);

char * get_cmd (int id);
bool scan_arg_bool   (const char *arg, const char *name, bool   *var);
bool scan_arg_int    (const char *arg, const char *name, int    *var);
bool scan_arg_long   (const char *arg, const char *name, long   *var);
bool scan_arg_double (const char *arg, const char *name, double *var);
bool scan_arg_string (const char *arg, const char *name, char  **var);

#endif
