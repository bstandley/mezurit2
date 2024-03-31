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

#include "section.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/fs.h>
#include <lib/util/num.h>

static char * lookup_icon (int loc);

#include "section_callback.c"

void section_init (Section *sect, const char *icon_filename, const char *label_str, GtkWidget **apt)
{
	f_start(F_INIT);

	sect->location = sect->old_location = SECTION_NOWHERE;
	sect->expand_fill = 0;  // default

	sect->full = pack_start(set_no_show_all(set_margins(gtk_frame_new(NULL), M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE)), sect->expand_fill, apt[sect->location]);
	gtk_widget_set_name(sect->full, "m2_section");
	GtkWidget *vbox = container_add(set_margins(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), 2, 2, 2, 2), sect->full);

	sect->heading = pack_start(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0),                                         0, vbox);
	sect->box     = pack_start(set_no_show_all(set_margins(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4), 3, 3, 6, 3)), 1, vbox);
	
	gtk_widget_set_name(sect->heading, "m2_heading");

	GtkWidget *icon  = pack_start(gtk_image_new_from_file(icon_filename), 0, sect->heading);
	GtkWidget *label = pack_start(new_label(label_str, 0, 0.0),           0, sect->heading);

	set_padding(icon,  2);
	set_padding(label, 1);

	sect->rollup_button = sect->orient_button = NULL;
	sect->loc_button = sect->loc_menu = NULL;
	for (int i = 0; i < M2_NUM_LOCATION; i++) sect->loc_item[i] = NULL;
}

void add_orient_button (Section *sect)
{
	f_start(F_INIT);

	sect->orient_button = pack_end(flatten_button(gtk_button_new()), 0, sect->heading);

	container_add(gtk_image_new(), sect->orient_button);
}

void add_rollup_button (Section *sect)
{
	f_start(F_INIT);
	
	sect->rollup_button = pack_end(flatten_button(gtk_button_new()), 0, sect->heading);

	container_add(gtk_image_new(), sect->rollup_button);
}

void add_loc_menu (Section *sect, ...)
{
	f_start(F_INIT);

	sect->loc_button = pack_end(flatten_button(gtk_button_new()), 0, sect->heading);

	container_add(gtk_image_new(), sect->loc_button);
	gtk_widget_set_tooltip_text(sect->loc_button, "Move tool");

	sect->loc_menu = attach_window(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), sect->full);  // any parent will do

	va_list vl;
	va_start(vl, sect);
	for (int loc = va_arg(vl, int); loc >= 0 && loc < M2_NUM_LOCATION; loc = va_arg(vl, int))
	{
		sect->loc_item[loc] = pack_start(flatten_button(gtk_button_new()), 0, sect->loc_menu);
		gtk_widget_set_name(container_add(gtk_image_new(), sect->loc_item[loc]), atg(lookup_icon(loc)));
	}
	va_end(vl);
}

void section_register (Section *sect, const char *prefix, int default_loc, GtkWidget **apt)
{
	f_start(F_INIT);

	int reo_var = (sect->orient_button != NULL) ? mcf_register(&sect->horizontal, atg(cat2(prefix, "section_horizontal")), MCF_BOOL | MCF_W | MCF_DEFAULT, 0)           : -1;
	int vis_var = (sect->rollup_button != NULL) ? mcf_register(&sect->visible,    atg(cat2(prefix, "section_visible")),    MCF_BOOL | MCF_W | MCF_DEFAULT, 1)           : -1;
	int loc_var = (sect->loc_button    != NULL) ? mcf_register(&sect->location,   atg(cat2(prefix, "section_location")),   MCF_INT  | MCF_W | MCF_DEFAULT, default_loc) :
	                                              mcf_register(&sect->location,   NULL,                                    MCF_INT  |         MCF_DEFAULT, default_loc);

	if (sect->orient_button != NULL) mcf_connect(reo_var, "setup, panel", BLOB_CALLBACK(orient_section_mcf), 0x10, sect);
	if (sect->rollup_button != NULL) mcf_connect(vis_var, "setup, panel", BLOB_CALLBACK(rollup_section_mcf), 0x10, sect);
	/**/                             mcf_connect(loc_var, "setup, panel", BLOB_CALLBACK(locate_section_mcf), 0x20, sect, apt);

	if (sect->orient_button != NULL) snazzy_connect(sect->orient_button,                   "button-release-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(orient_section_cb), 0x10, sect);
	if (sect->rollup_button != NULL) snazzy_connect(sect->rollup_button,                   "button-release-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(rollup_section_cb), 0x10, sect);
	if (sect->loc_button    != NULL) snazzy_connect(sect->loc_button,                      "clicked",              SNAZZY_VOID_VOID, BLOB_CALLBACK(popup_menu_cb),     0x10, sect->loc_menu);
	if (sect->loc_menu      != NULL) snazzy_connect(gtk_widget_get_parent(sect->loc_menu), "focus-out-event",      SNAZZY_BOOL_PTR,  BLOB_CALLBACK(hide_menu_cb),      0x10, sect->loc_menu);

	for (int i = 0; i < M2_NUM_LOCATION; i++)
		if (sect->loc_item[i] != NULL) snazzy_connect(sect->loc_item[i], "clicked", SNAZZY_VOID_VOID, BLOB_CALLBACK(locate_section_cb), 0x21, sect, apt, i);
}

char * lookup_icon (int loc)
{
	switch (loc)
	{
		case SECTION_WAYLEFT  : return cat1("m2_icon_wayleft");
		case SECTION_LEFT     : return cat1("m2_icon_left");
		case SECTION_TOP      : return cat1("m2_icon_top");
		case SECTION_BOTTOM   : return cat1("m2_icon_bottom");
		case SECTION_RIGHT    : return cat1("m2_icon_right");
		case SECTION_WAYRIGHT : return cat1("m2_icon_wayright");
		default               : f_print(F_ERROR, "Error: Location out of range.\n");
		                        return NULL;
	}
}
