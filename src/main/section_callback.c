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

static gboolean rollup_section_cb (GtkWidget *widget, GdkEvent *event, Section *sect);
static gboolean orient_section_cb (GtkWidget *widget, GdkEvent *event, Section *sect);
static void locate_section_cb (GtkWidget *widget, Section *sect, GtkWidget **apt, int dest);
static void popup_menu_cb (GtkWidget *widget, GtkWidget *menu);
static gboolean hide_menu_cb (GtkWidget *widget, GdkEventButton *event, GtkWidget *menu);

static void rollup_section_mcf (bool *visible, const char *str, MValue value, Section *sect);
static void orient_section_mcf (bool *horizontal, const char *str, MValue value, Section *sect);
static void locate_section_mcf (int *location, const char *str, MValue value, Section *sect, GtkWidget **apt);

static void rollup_section (Section *sect);
static void orient_section (Section *sect);
static void locate_section (Section *sect, GtkWidget **apt);

void rollup_section (Section *sect)
{
	set_visibility(sect->box, sect->visible);

	gtk_widget_set_tooltip_text(sect->rollup_button, sect->visible ? "Hide tool" : "Show tool");
	gtk_image_set_from_pixbuf(GTK_IMAGE(get_child(sect->rollup_button, 0)), lookup_pixbuf(sect->visible ? PIXBUF_ICON_ROLLUP : PIXBUF_ICON_ROLLDOWN));
	// Note: Using pre-loaded pixbufs cuts rollup_section_cb()'s exec time from 0.3 msec to 0.1 msec.
}

void orient_section (Section *sect)
{
	gtk_orientable_set_orientation(GTK_ORIENTABLE(sect->box), sect->horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

	gtk_widget_set_tooltip_text(sect->orient_button, sect->horizontal ? "Stack vertically" : "Arrange horizontally");
	gtk_image_set_from_pixbuf(GTK_IMAGE(get_child(sect->orient_button, 0)), lookup_pixbuf(sect->horizontal ? PIXBUF_ICON_VERTICAL : PIXBUF_ICON_HORIZONTAL));
}

void locate_section (Section *sect, GtkWidget **apt)  // manually set sect->location, then run this
{
	if (sect->loc_menu != NULL) gtk_widget_hide(gtk_widget_get_parent(sect->loc_menu));
	if (sect->loc_button != NULL) gtk_image_set_from_pixbuf(GTK_IMAGE(get_child(sect->loc_button, 0)), lookup_icon(sect->location));

	if (apt[sect->location] == NULL) sect->location = sect->old_location;  // handle invalid locations (for example, a corrupted MCF line)

	if (sect->old_location != sect->location)
	{
		bool old_expand_fill = sect->expand_fill;
		pack_start(sect->full, sect->expand_fill, apt[sect->location]);
		if (old_expand_fill != sect->expand_fill) set_expand_fill(sect->full, sect->expand_fill);
		
		sect->old_location = sect->location;
	}

	gtk_widget_show(sect->full);
}

gboolean rollup_section_cb (GtkWidget *widget, GdkEvent *event, Section *sect)
{
	f_start(F_CALLBACK);

	sect->visible = !sect->visible;
	rollup_section(sect);

	return 0;
}

gboolean orient_section_cb (GtkWidget *widget, GdkEvent *event, Section *sect)
{
	f_start(F_CALLBACK);

	sect->horizontal = !sect->horizontal;
	orient_section(sect);

	return 0;
}

void locate_section_cb (GtkWidget *widget, Section *sect, GtkWidget **apt, int dest)
{
	f_start(F_CALLBACK);

	sect->location = dest;
	locate_section(sect, apt);
}

void rollup_section_mcf (bool *visible, const char *str, MValue value, Section *sect)
{
	f_start(F_MCF);

	*visible = value.x_bool;
	rollup_section(sect);
}

void orient_section_mcf (bool *horizontal, const char *str, MValue value, Section *sect)
{
	f_start(F_MCF);

	*horizontal = value.x_bool;
	orient_section(sect);
}

void locate_section_mcf (int *location, const char *str, MValue value, Section *sect, GtkWidget **apt)
{
	f_start(F_MCF);

	*location = window_int(value.x_int, 0, M2_NUM_LOCATION - 1);
	locate_section(sect, apt);
}

void popup_menu_cb (GtkWidget *widget, GtkWidget *menu)
{
	f_start(F_CALLBACK);

	show_all(gtk_widget_get_parent(menu), NULL);
}

gboolean hide_menu_cb (GtkWidget *widget, GdkEventButton *event, GtkWidget *menu)
{
	f_start(F_CALLBACK);

	gtk_widget_hide(gtk_widget_get_parent(menu));
	return 0;
}
