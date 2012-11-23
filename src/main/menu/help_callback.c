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

static void open_resource_cb (GtkWidget *widget, char *fmt, char *uri);
static gboolean open_resource_about_cb (GtkWidget *widget, gchar *uri, char *fmt);
static void show_about_cb (GtkWidget *widget, Help *help);
static void hide_after_response_cb (GtkWidget *widget, gint id);
static gboolean hide_after_delete_cb (GtkWidget *widget, GdkEvent *event);

void open_resource_cb (GtkWidget *widget, char *fmt, char *uri)
{
	f_start(F_CALLBACK);

#ifdef MINGW
	ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
#else
	if (str_length(fmt) > 0)
	{
		char _strfree_ *cmd = supercat(fmt, uri);
		f_print(F_RUN, "system(%s)\n", cmd);
		if (system(cmd) != 0) f_print(F_ERROR, "Error: system(%s) failed\n", cmd);
	}
	else status_add(1, cat1("Unable to open resource because no appropriate program was found.\n"));
#endif
}

gboolean open_resource_about_cb (GtkWidget *widget, gchar *uri, char *fmt)
{
	open_resource_cb(widget, fmt, uri);
	return 1;
}

void show_about_cb (GtkWidget *widget, Help *help)
{
	f_start(F_CALLBACK);

	gtk_widget_show(help->about_dialog);
}

void hide_after_response_cb (GtkWidget *widget, gint id)
{
	f_start(F_CALLBACK);

	gtk_widget_hide(widget);
}

gboolean hide_after_delete_cb (GtkWidget *widget, GdkEvent *event)
{
	f_start(F_CALLBACK);

	gtk_widget_hide(widget);
	return 1;  // stop event from propagating so widget is not actually deleted
}
