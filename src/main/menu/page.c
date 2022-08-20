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

#include "page.h"

#include <lib/status.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>

#include "page_callback.c"

void page_init (Page *page, GtkWidget *menubar)
{
	f_start(F_INIT);

	GtkWidget *menu = set_submenu(gtk_menu_new(), menu_append(gtk_menu_item_new_with_label("Page"), menubar));

	/**/                                         page->setup_item      = menu_append(new_radio_item("Setup"), menu);
	for (int pid = 0; pid < M2_NUM_PANEL; pid++) page->panel_item[pid] = menu_append(new_radio_item(atg(supercat("Panel %d", pid))), menu);
}

void page_register (Page *page, ThreadVars *tv, Config *config, Terminal *terminal)
{
	f_start(F_INIT);

	/**/                                         snazzy_connect(page->setup_item,      "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(set_page_cb), 0x31, page, config, tv, -1);
	for (int pid = 0; pid < M2_NUM_PANEL; pid++) snazzy_connect(page->panel_item[pid], "button-release-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(set_page_cb), 0x31, page, config, tv, pid);

	snazzy_connect(page->main_window, "delete-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(exit_cb), 0x30, page, tv, terminal);

	control_server_connect(M2_TS_ID, "hello",     M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(hello_csf),     0x00);
	control_server_connect(M2_TS_ID, "set_panel", M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(set_panel_csf), 0x30, page, config, tv);
	control_server_connect(M2_TS_ID, "get_panel", M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(get_panel_csf), 0x10, tv);
}

void set_page (Page *page, Config *config, int pid)
{
	f_start(F_UPDATE);

	for (int i = 0; i < 6; i++) gtk_widget_set_sensitive(config->load_item[i],     pid == -1);
	/**/                        gtk_widget_set_sensitive(config->load_preset_item, pid == -1);

	if (page->terminal_vte != NULL) container_add(page->terminal_vte, pid == -1 ? page->setup->terminal_scroll : page->panel_array[pid].terminal_scroll);
	message_update(pid == -1 ? &page->setup->message : &page->panel_array[pid].message);

	if (pid >= 0) gtk_widget_hide(page->flipbook);
	set_flipbook_page(page->flipbook, pid + 1);

	char *title _strfree_ = pid == -1 ? supercat("%s %s: Setup",    quote(PROG2), quote(VERSION)) :
	                                    supercat("%s %s: Panel %d", quote(PROG2), quote(VERSION), pid);
	gtk_window_set_title(GTK_WINDOW(page->main_window), title);

	/**/                                   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(page->setup_item),    pid == -1);
	for (int i = 0; i < M2_NUM_PANEL; i++) gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(page->panel_item[i]), pid == i);

	if (page->terminal_vte) gtk_widget_grab_focus(page->terminal_vte);
}
