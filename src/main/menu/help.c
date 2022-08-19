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

#include "help.h"

#include <stdlib.h>  // system()

#include <lib/status.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>

#ifdef MINGW
#include <windows.h>
#endif

#include "help_callback.c"

void help_init (Help *help, GtkWidget *menubar)
{
	f_start(F_INIT);

	help->about_dialog = gtk_about_dialog_new();

	const gchar *authors[] = { "Brian Standley <brian@brianstandley.com>\n",
	                           "Testers:\nBockrath group at UC Riverside\nLau group at UC Riverside\n",
	                           "Special thanks:\nTengfei Miao\nHang Zhang\n",
	                           "Inspired by \"Mezurit,\" which was written\nby Marc Bockrath and David Cobden",
	                           NULL };

	gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG(help->about_dialog), quote(PROG2));
	gtk_about_dialog_set_logo         (GTK_ABOUT_DIALOG(help->about_dialog), NULL);
	gtk_about_dialog_set_version      (GTK_ABOUT_DIALOG(help->about_dialog), quote(VERSION));
	gtk_about_dialog_set_copyright    (GTK_ABOUT_DIALOG(help->about_dialog), "© 2012 California Institute of Technology");
	gtk_about_dialog_set_website      (GTK_ABOUT_DIALOG(help->about_dialog), M2_URL);
    gtk_about_dialog_set_comments     (GTK_ABOUT_DIALOG(help->about_dialog), "Scriptable continuous and triggered data acquisition for Linux and Windows");
	gtk_about_dialog_set_authors      (GTK_ABOUT_DIALOG(help->about_dialog), authors);

	gchar *license_text;
	g_file_get_contents(atg(sharepath("COPYING")), &license_text, NULL, NULL);
	if (license_text != NULL) gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(help->about_dialog), license_text);
	g_free(license_text);

	GtkWidget *menu = set_submenu(gtk_menu_new(), menu_append(gtk_menu_item_new_with_label("Help"), menubar));

	help->online_item  = menu_append(gtk_menu_item_new_with_label("Documentation (Online)"), menu);
	help->offline_item = menu_append(gtk_menu_item_new_with_label("Manual (PDF)"),           menu);
	help->about_item   = menu_append(gtk_menu_item_new_with_label("About"),                  menu);
}

void help_register (Help *help)
{
	f_start(F_INIT);

	char *url_fmt = NULL, *pdf_fmt = NULL;
#ifndef MINGW
	if (system("which xdg-open > /dev/null 2>&1") == 0) url_fmt = pdf_fmt = atg(cat1("xdg-open %s"));
	else
	{
		if      (system("which firefox  > /dev/null 2>&1") == 0) url_fmt = atg(cat1("firefox %s &"));
		else if (system("which chromium > /dev/null 2>&1") == 0) url_fmt = atg(cat1("chromium %s &"));

		if      (system("which evince   > /dev/null 2>&1") == 0) pdf_fmt = atg(cat1("evince %s &"));
		else if (system("which xpdf     > /dev/null 2>&1") == 0) pdf_fmt = atg(cat1("xpdf %s &"));
	}
#endif

	char *filename = atg(sharepath("manual.pdf"));
	if (filename == NULL) gtk_widget_set_sensitive(help->offline_item, 0);

	/**/                  snazzy_connect(help->online_item,  "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(open_resource_cb), 0x20, cat1(url_fmt), cat3("http://", M2_URL, "/doc"));
	if (filename != NULL) snazzy_connect(help->offline_item, "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(open_resource_cb), 0x20, cat1(pdf_fmt), cat1(filename));
	/**/                  snazzy_connect(help->about_item,   "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(show_about_cb),    0x10, help);

	snazzy_connect(help->about_dialog, "activate-link", SNAZZY_BOOL_PTR, BLOB_CALLBACK(open_resource_about_cb), 0x10, cat1(url_fmt));
	snazzy_connect(help->about_dialog, "response",      SNAZZY_VOID_INT, BLOB_CALLBACK(hide_after_response_cb), 0x00);
	snazzy_connect(help->about_dialog, "delete-event",  SNAZZY_BOOL_PTR, BLOB_CALLBACK(hide_after_delete_cb),   0x00);
}
