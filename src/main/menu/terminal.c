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

#include "terminal.h"

#ifndef MINGW
#define HEADER_SANS_WARNINGS <vte/vte.h>
#include <sans_warnings.h>
#else
#include <stdlib.h>
#include <windows.h>
#endif
#define HEADER_SANS_WARNINGS <Python.h>
#include <sans_warnings.h>

#include <lib/status.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/fs.h>

#include "terminal_callback.c"

void terminal_init (Terminal *terminal, GtkWidget *menubar, GtkWidget *scroll, int port)
{
	f_start(F_INIT);

#ifndef MINGW
	terminal->vte = container_add(vte_terminal_new(), scroll);
	vte_terminal_set_size(VTE_TERMINAL(terminal->vte), 120, 500);
	terminal->no_auto_respawn = 0;
#endif

	g_setenv("PYTHONSTARTUP",       atg(sharepath("terminal_startup.py")), 1);
	g_setenv("M2_USER_TERMINAL_PY", atg(configpath("terminal.py")),        1);
	g_setenv("M2_TERMINAL_HISTORY", atg(configpath("history")),            1);
	g_setenv("M2_CONTROLPORT",      atg(supercat("%d", port)),             1);
	g_setenv("M2_LIBPATH",          atg(libpath(NULL)),                    1);

	GtkWidget *menu = set_submenu(gtk_menu_new(), menu_append(gtk_menu_item_new_with_label("Terminal"), menubar));

	terminal->restart_item = menu_append(gtk_menu_item_new_with_label("Restart process"), menu);
	terminal->abort_item   = menu_append(gtk_menu_item_new_with_label("Abort command"),   menu);
}

void terminal_register (Terminal *terminal, ThreadVars *tv)
{
	f_start(F_INIT);

#ifndef MINGW
	snazzy_connect(terminal->vte,          "child-exited", SNAZZY_VOID_INT,  BLOB_CALLBACK(respawn_after_exit_cb), 0x10, terminal);
#endif
	snazzy_connect(terminal->restart_item, "activate",     SNAZZY_VOID_VOID, BLOB_CALLBACK(respawn_cb),            0x10, terminal);
	snazzy_connect(terminal->abort_item,   "activate",     SNAZZY_VOID_VOID, BLOB_CALLBACK(abort_control_cb),      0x10, tv);
}

void spawn_terminal (Terminal *terminal)
{
	f_start(F_RUN);

#ifdef MINGW
	char *exe = atg(supercat("python%d.%d.exe", PY_MAJOR_VERSION, PY_MINOR_VERSION));
	system(atg(supercat("start \"%s %s: Terminal\" \"%s\" -O -B", quote(PROG2), quote(VERSION), atg(libpath(exe)))));
#else
	char *argv[2];
	argv[0] = atg(supercat("python%d.%d", PY_MAJOR_VERSION, PY_MINOR_VERSION));
	argv[1] = NULL;
	vte_terminal_spawn_async(VTE_TERMINAL(terminal->vte), VTE_PTY_DEFAULT, NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, NULL, NULL);
#endif
}

void quit_terminal (Terminal *terminal)
{
	f_start(F_RUN);

#ifndef MINGW
	char *msg _strfree_ = cat1("quit()\n");
	vte_terminal_feed_child(VTE_TERMINAL(terminal->vte), msg, (glong) str_length(msg));
	terminal->no_auto_respawn = 1;
#endif
}
