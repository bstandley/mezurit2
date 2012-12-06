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

static GdkPixbuf * lookup_icon (int loc);

#include "section_callback.c"

void section_init (Section *sect, const char *icon_filename, const char *label_str, GtkWidget **apt)
{
	f_start(F_INIT);

	sect->location = sect->old_location = SECTION_NOWHERE;
	sect->expand_fill = 0;  // default

	sect->full = pack_start(set_no_show_all(new_alignment(M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE)), sect->expand_fill, apt[sect->location]);

	GtkWidget *vbox = container_add(new_box(GTK_ORIENTATION_VERTICAL, 0),
	                  container_add(new_alignment(1, 1, 1, 1),
	                  container_add(name_widget(gtk_event_box_new(), "m2_section"),
	                  container_add(new_alignment(1, 1, 1, 1),
	                  container_add(name_widget(gtk_event_box_new(), "m2_border"), sect->full)))));

	sect->heading = pack_start(new_box(GTK_ORIENTATION_HORIZONTAL, 0),                         0, vbox);
	sect->box     = container_add(new_box(GTK_ORIENTATION_VERTICAL, 4),
	                pack_start(set_no_show_all(new_alignment(3, 3, 6, 3)), 1, vbox));

	GtkWidget *icon  = pack_start(gtk_image_new_from_file(icon_filename),                      0, sect->heading);
	GtkWidget *label = pack_start(new_label(atg(supercat(M2_HEADING_FORMAT, label_str)), 0.0), 0, sect->heading);

	set_padding(icon,  2);
	set_padding(label, 1);

	sect->rollup_button = sect->orient_button = NULL;
	sect->loc_button = sect->loc_menu = NULL;
	for (int i = 0; i < M2_NUM_LOCATION; i++) sect->loc_item[i] = NULL;
}

void add_orient_button (Section *sect)
{
	f_start(F_INIT);

	sect->orient_button = pack_end(name_widget(flatten_button(gtk_button_new()), "m2_button"), 0, sect->heading);

	container_add(gtk_image_new(), sect->orient_button);
	set_draw_on_expose(sect->full, sect->orient_button);
}

void add_rollup_button (Section *sect)
{
	f_start(F_INIT);
	
	sect->rollup_button = pack_end(name_widget(flatten_button(gtk_button_new()), "m2_button"), 0, sect->heading);

	container_add(gtk_image_new(), sect->rollup_button);
	set_draw_on_expose(sect->full, sect->rollup_button);
}

void add_loc_menu (Section *sect, ...)
{
	f_start(F_INIT);

	sect->loc_button = pack_end(name_widget(flatten_button(gtk_button_new()), "m2_button"), 0, sect->heading);

	container_add(gtk_image_new(), sect->loc_button);
	set_draw_on_expose(sect->full, sect->loc_button);
	gtk_widget_set_tooltip_markup(sect->loc_button, "Move tool");

	sect->loc_menu = attach_window(new_box(GTK_ORIENTATION_HORIZONTAL, 0), sect->full);  // any parent will do

	va_list vl;
	va_start(vl, sect);
	for (int loc = va_arg(vl, int); loc >= 0 && loc < M2_NUM_LOCATION; loc = va_arg(vl, int))
	{
		sect->loc_item[loc] = pack_start(flatten_button(name_widget(gtk_button_new(), "m2_button")), 0, sect->loc_menu);
		container_add(gtk_image_new_from_pixbuf(lookup_icon(loc)), sect->loc_item[loc]);
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

GdkPixbuf * lookup_icon (int loc)
{
	switch (loc)
	{
		case SECTION_WAYLEFT  : return lookup_pixbuf(PIXBUF_ICON_WAYLEFT);
		case SECTION_LEFT     : return lookup_pixbuf(PIXBUF_ICON_LEFT);
		case SECTION_TOP      : return lookup_pixbuf(PIXBUF_ICON_TOP);
		case SECTION_BOTTOM   : return lookup_pixbuf(PIXBUF_ICON_BOTTOM);
		case SECTION_RIGHT    : return lookup_pixbuf(PIXBUF_ICON_RIGHT);
		case SECTION_WAYRIGHT : return lookup_pixbuf(PIXBUF_ICON_WAYRIGHT);
		default               : f_print(F_ERROR, "Error: Location out of range.\n");
		                        return NULL;
	}
}
