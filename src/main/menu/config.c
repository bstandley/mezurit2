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

#include "config.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>

enum
{
	MENU_FILE = 0,          // load and save
	MENU_USER_DEFAULT,      // load and save
	MENU_SYS_DEFAULT,       // load only
	MENU_LAST,              // load only
	MENU_INTERNAL_DEFAULT,  // load only
	MENU_EXAMPLE_FILE       // load only
};

#include "config_callback.c"

void config_init (Config *config, GtkWidget *menubar)
{
	f_start(F_INIT);

	GtkWidget *menu = set_submenu(gtk_menu_new(), menu_append(new_item(new_label("_Config", 0.0), NULL), menubar));

	config->load_item[MENU_FILE] = menu_append(new_item(new_label("Load from file...", 0.0), gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU)), menu);
	config->load_item[MENU_LAST] = menu_append(new_item(new_label("Load last",         0.0), gtk_image_new_from_stock(GTK_STOCK_UNDO, GTK_ICON_SIZE_MENU)), menu);
	config->load_preset_item     = menu_append(new_item(new_label("Load preset",       0.0), NULL),                                                         menu);

	GtkWidget *preset_menu = set_submenu(gtk_menu_new(), config->load_preset_item);

	config->load_item[MENU_USER_DEFAULT]     = menu_append(new_item(new_label("User defaults",     0.0), NULL), preset_menu);
	config->load_item[MENU_SYS_DEFAULT]      = menu_append(new_item(new_label("System defaults",   0.0), NULL), preset_menu);
	config->load_item[MENU_INTERNAL_DEFAULT] = menu_append(new_item(new_label("Internal defaults", 0.0), NULL), preset_menu);
	config->load_item[MENU_EXAMPLE_FILE]     = menu_append(new_item(new_label("Example...",        0.0), NULL), preset_menu);

	menu_append(gtk_separator_menu_item_new(), menu);

	config->save_item[MENU_FILE]         = menu_append(new_item(new_label("Save to file...",    0.0), gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU)), menu);
	config->save_item[MENU_USER_DEFAULT] = menu_append(new_item(new_label("Save user defaults", 0.0), NULL),                                                         menu);
}

void config_register (Config *config, Hardware *hw_array, Panel **tv_panel, OldVars *oldvars)
{
	f_start(F_INIT);

	for (int i = 0; i < 6; i++) snazzy_connect(config->load_item[i], "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(config_cb), 0x32, hw_array, tv_panel, oldvars, 1, i);
	for (int i = 0; i < 2; i++) snazzy_connect(config->save_item[i], "activate", SNAZZY_VOID_VOID, BLOB_CALLBACK(config_cb), 0x32, hw_array, tv_panel, oldvars, 0, i);

	control_server_connect(M2_TS_ID, "load_config", M2_CODE_SETUP,                        BLOB_CALLBACK(config_csf), 0x30, hw_array, tv_panel, oldvars);
	control_server_connect(M2_TS_ID, "save_config", M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(config_csf), 0x30, hw_array, tv_panel, oldvars);
	control_server_connect(M2_TS_ID, "set_var",     M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(var_csf),    0x10, tv_panel);
	control_server_connect(M2_TS_ID, "get_var",     M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(var_csf),    0x10, tv_panel);
}
