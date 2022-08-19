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

GtkWidget * new_label (const char *str, double xalign)
{
	GtkWidget *widget = gtk_label_new(str);
	gtk_label_set_use_markup(GTK_LABEL(widget), 1);
	gtk_label_set_xalign(GTK_LABEL(widget), (gfloat) xalign);
	gtk_label_set_yalign(GTK_LABEL(widget), 0.5);
	return widget;
}

GtkWidget * new_entry (int max, int width)
{
	GtkWidget *widget = gtk_entry_new();
	gtk_entry_set_max_length  (GTK_ENTRY(widget), max);
	gtk_entry_set_width_chars (GTK_ENTRY(widget), width);
	return fix_shadow(widget);
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

GtkWidget * new_toggle_button_with_icon (const char *label_str, const char *icon, GtkPositionType pos)
{
	GtkWidget *widget = gtk_toggle_button_new_with_label(label_str);
	if (icon != NULL)
	{
		gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU));
		gtk_button_set_image_position(GTK_BUTTON(widget), pos);
	}
	return widget;
}

GtkWidget * new_button_with_icon (const char *label_str, const char *icon)
{
	GtkWidget *widget = (label_str != NULL) ? gtk_button_new_with_label(label_str) : gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU));
	return widget;
}

GtkWidget * new_item (GtkWidget *child, GtkWidget *image)
{
	GtkWidget *widget = (image != NULL) ? gtk_image_menu_item_new() : gtk_menu_item_new();

	if (image != NULL)
	{
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget), image);
		gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(widget), 1);
	}

	if (child != NULL)
	{
		container_add(child, widget);
		gtk_menu_item_set_use_underline(GTK_MENU_ITEM(widget), 1);
	}

	return widget;
}

GtkWidget * new_scrolled_window (GtkPolicyType hpolicy, GtkPolicyType vpolicy)
{
	GtkWidget *widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget), hpolicy, vpolicy);
	return widget;
}
