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

#include "bufmenu.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>

#include "bufmenu_callback.c"

void bufmenu_init (Bufmenu *bufmenu, GtkWidget *menubar)
{
	f_start(F_INIT);

	GtkWidget *menu = set_submenu(gtk_menu_new(), menu_append(new_item(new_label("_Buffer", 0.0), NULL), menubar));

	bufmenu->link_tzero_item  = menu_append(new_item(new_label("Set T=0 on clear", 0.0),               gtk_image_new()), menu);
	/**/                        menu_append(gtk_separator_menu_item_new(),                                               menu);
	bufmenu->save_header_item = menu_append(new_item(new_label("Save column headings",           0.0), gtk_image_new()), menu);
	bufmenu->save_mcf_item    = menu_append(new_item(new_label("Save config alongside data",     0.0), gtk_image_new()), menu);
	bufmenu->save_scr_item    = menu_append(new_item(new_label("Save screenshot alongside data", 0.0), gtk_image_new()), menu);

	gtk_widget_set_sensitive(bufmenu->save_mcf_item, 0);
	gtk_widget_set_sensitive(bufmenu->save_scr_item, 0);
}

void bufmenu_register (Bufmenu *bufmenu)
{
	f_start(F_INIT);
	
	int link_tzero_var  = mcf_register(&bufmenu->link_tzero,  "link_tzero_to_clear",       MCF_BOOL | MCF_W | MCF_DEFAULT, 1);
	int save_header_var = mcf_register(&bufmenu->save_header, "save_channel_names",        MCF_BOOL | MCF_W | MCF_DEFAULT, 1);
	int save_mcf_var    = mcf_register(&bufmenu->save_mcf,    "save_config_with_data",     MCF_BOOL | MCF_W | MCF_DEFAULT, 0);
	int save_scr_var    = mcf_register(&bufmenu->save_scr,    "save_screenshot_with_data", MCF_BOOL | MCF_W | MCF_DEFAULT, 0);

	mcf_connect(link_tzero_var,  "setup, panel", BLOB_CALLBACK(check_item_mcf), 0x10, bufmenu->link_tzero_item);
	mcf_connect(save_header_var, "setup, panel", BLOB_CALLBACK(check_item_mcf), 0x10, bufmenu->save_header_item);
	mcf_connect(save_mcf_var,    "setup, panel", BLOB_CALLBACK(check_item_mcf), 0x10, bufmenu->save_mcf_item);
	mcf_connect(save_scr_var,    "setup, panel", BLOB_CALLBACK(check_item_mcf), 0x10, bufmenu->save_scr_item);

	snazzy_connect(bufmenu->link_tzero_item,  "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(check_item_cb), 0x10, &bufmenu->link_tzero);
	snazzy_connect(bufmenu->save_header_item, "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(check_item_cb), 0x10, &bufmenu->save_header);
	snazzy_connect(bufmenu->save_mcf_item,    "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(check_item_cb), 0x10, &bufmenu->save_mcf);
	snazzy_connect(bufmenu->save_scr_item,    "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(check_item_cb), 0x10, &bufmenu->save_scr);
}
