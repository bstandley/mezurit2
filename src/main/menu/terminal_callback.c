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

#ifndef MINGW
	char *msg _strfree_ = cat1("Restarting . . .   ");
	vte_terminal_feed(VTE_TERMINAL(terminal->vte), msg, (glong) str_length(msg));
#endif
	spawn_terminal(terminal);

	status_add(0, cat1("Restarted command line process.\n"));
}
