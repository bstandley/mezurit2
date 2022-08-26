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

static void abort_control_cb (GtkWidget *widget, ThreadVars *tv);
static void respawn_cb (GtkWidget *widget, Terminal *terminal);
#ifndef MINGW
static void respawn_after_exit_cb (GtkWidget *widget, int status, Terminal *terminal);
#endif

void abort_control_cb (GtkWidget *widget, ThreadVars *tv)
{
	f_start(F_CALLBACK);

	mt_mutex_lock(&tv->ts_mutex);
	if (get_cmd(M2_TS_ID) != NULL)
	{
		control_server_reply(M2_TS_ID, "abort");
		tv->catch_sweep_ici = -1;  // only member of ThreadVars which needs to be reset
	}
	mt_mutex_unlock(&tv->ts_mutex);
}

void respawn_cb (GtkWidget *widget, Terminal *terminal)
{
	f_start(F_CALLBACK);

	spawn_terminal(terminal);  // does *not* give python a chance to shutdown gracefully

#ifdef MINGW
	status_add(0, cat1("Launched new terminal window.\n"));
#endif
}

#ifndef MINGW
void respawn_after_exit_cb (GtkWidget *widget, int status, Terminal *terminal)
{
	if (terminal->no_auto_respawn)
	{
		f_print(F_CALLBACK, "Skipping unnecessary callback.\n");
		return;
	}

	f_start(F_CALLBACK);

	spawn_terminal(terminal);

	status_add(0, supercat("Command line process exited with status %d, restarted automatically.\n", status));
}
#endif
