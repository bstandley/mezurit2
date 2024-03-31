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

GtkWidget * new_label (const char *str, bool markup, double xalign)
{
	GtkWidget *widget = gtk_label_new(str);
	if (markup) gtk_widget_set_name(widget, "m2_markup");
	gtk_label_set_use_markup(GTK_LABEL(widget), markup);
	gtk_label_set_xalign(GTK_LABEL(widget), (gfloat) xalign);
	gtk_label_set_yalign(GTK_LABEL(widget), 0.5);
	return widget;
}

GtkWidget * new_entry (int max, int width)
{
	GtkWidget *widget = gtk_entry_new();
	gtk_entry_set_max_length  (GTK_ENTRY(widget), max);
	gtk_entry_set_width_chars (GTK_ENTRY(widget), width);
	return widget;
}

GtkWidget * new_text_view (gint left_margin, gint right_margin)
{
	GtkWidget *widget = gtk_text_view_new();
	gtk_text_view_set_left_margin    (GTK_TEXT_VIEW(widget), left_margin);
	gtk_text_view_set_right_margin   (GTK_TEXT_VIEW(widget), right_margin);
	gtk_text_view_set_editable       (GTK_TEXT_VIEW(widget), 0);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(widget), 0);
	return widget;
}

GtkWidget * new_table (guint row_spacing, guint col_spacing)
{
	GtkWidget *widget = gtk_grid_new();
	gtk_grid_set_row_spacing   (GTK_GRID(widget), row_spacing);
	gtk_grid_set_column_spacing(GTK_GRID(widget), col_spacing);
	return widget;
}

GtkWidget * new_radio_item (const char *label_str)
{
	GtkWidget *widget = gtk_check_menu_item_new_with_label(label_str);
	gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(widget), 1);

	GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));
	gtk_label_set_use_markup(GTK_LABEL(child), 1);
	return widget;
}

GtkWidget * new_scrolled_window (GtkPolicyType hpolicy, GtkPolicyType vpolicy)
{
	GtkWidget *widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget), hpolicy, vpolicy);
	return widget;
}
