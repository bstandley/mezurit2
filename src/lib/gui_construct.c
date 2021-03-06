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

GtkWidget * new_box (GtkOrientation orientation, gint spacing)
{
#if GTK_MAJOR_VERSION < 3
	GtkWidget *widget = (orientation == GTK_ORIENTATION_HORIZONTAL) ? gtk_hbox_new(0, spacing) : gtk_vbox_new(0, spacing);
#else
	GtkWidget *widget = gtk_box_new(orientation, spacing);
#endif
	return widget;
}

GtkWidget * new_label (const char *str, double xalign)
{
	GtkWidget *widget = gtk_label_new(str);
	gtk_label_set_use_markup(GTK_LABEL(widget), 1);
	gtk_misc_set_alignment(GTK_MISC(widget), (gfloat) xalign, 0.5);
	return widget;
}

GtkWidget * new_alignment (guint top, guint bottom, guint left, guint right)
{
	GtkWidget *widget = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(widget), top, bottom, left, right);
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
#if GTK_MAJOR_VERSION < 3
	GtkWidget *widget = gtk_table_new(1, 1, 0);
	gtk_table_set_row_spacings(GTK_TABLE(widget), row_spacing);
	gtk_table_set_col_spacings(GTK_TABLE(widget), col_spacing);
#else
	GtkWidget *widget = gtk_grid_new();
	gtk_grid_set_row_spacing   (GTK_GRID(widget), row_spacing);
	gtk_grid_set_column_spacing(GTK_GRID(widget), col_spacing);
#endif
	return widget;
}

GtkWidget * new_toggle_button_with_icon (const char *label_str, const char *icon, GtkPositionType pos)
{
	GtkWidget *widget = gtk_toggle_button_new_with_label(label_str);
	if (icon != NULL)
	{
		gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU));
		gtk_button_set_image_position(GTK_BUTTON(widget), pos);
	}
	return widget;
}

GtkWidget * new_button_with_icon (const char *label_str, const char *icon)
{
	GtkWidget *widget = (label_str != NULL) ? gtk_button_new_with_label(label_str) : gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(widget), gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU));
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

GtkWidget * new_combo_text (void)
{
#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 24
	return gtk_combo_box_new_text();
#else
	return gtk_combo_box_text_new();
#endif
}
