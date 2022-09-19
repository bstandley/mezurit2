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

#ifndef _LIB_GUI_H
#define _LIB_GUI_H 1

#include <stdbool.h>
#include <gtk/gtk.h>

#include <lib/blob.h>

// Persistent state for run_file_chooser(), CSS theme:

void gui_init  (void);
void gui_final (void);

// Snazzy callbacks:

enum
{
	SNAZZY_VOID_VOID,  // options for type argument of snazzy_connect(),
	SNAZZY_VOID_INT,   // match to user_function() appropriate for
	SNAZZY_VOID_PTR,   // signal_list according the the GTK docs
	SNAZZY_BOOL_PTR
};

void snazzy_connect (GtkWidget *widget, const char *signal_list, int type, BlobCallback cb, int sig, ...);

// Utilities:

void show_all    (GtkWidget *widget, void *ptr);
void scroll_down (GtkWidget *widget);

GtkWidget * set_no_show_all    (GtkWidget *widget);
GtkWidget * set_visibility     (GtkWidget *widget, bool visible);
GtkWidget * size_widget        (GtkWidget *widget, gint x, gint y);
GtkWidget * flatten_button     (GtkWidget *widget);
GtkWidget * set_padding        (GtkWidget *widget, guint padding);
GtkWidget * set_margins        (GtkWidget *widget, gint top, gint bottom, gint left, gint right);
GtkWidget * set_expand_fill    (GtkWidget *widget, bool expand_fill);
GtkWidget * set_text_view_text (GtkWidget *widget, const char *str);
GtkWidget * get_child          (GtkWidget *widget, int n);
GtkWidget * attach_window      (GtkWidget *target, GtkWidget *parent);

enum
{
	FILE_CHOOSER_OPEN,
	FILE_CHOOSER_SAVE,
	FILE_CHOOSER_PRESAVE
};

char * run_file_chooser  (const char *title, int action, const char *preset);
bool   run_yes_no_dialog (const char *message);

// Custom constructors:

GtkWidget * new_label (const char *str, double xalign);
GtkWidget * new_entry (int max, int width);
GtkWidget * new_text_view (gint left_margin, gint right_margin);
GtkWidget * new_table (guint row_spacing, guint col_spacing);
GtkWidget * new_radio_item (const char *label_str);
GtkWidget * new_scrolled_window (GtkPolicyType hpolicy, GtkPolicyType vpolicy);

// Custom packing:

GtkWidget * pack_start        (GtkWidget *widget, bool expand_fill, GtkWidget *box);
GtkWidget * pack_end          (GtkWidget *widget, bool expand_fill, GtkWidget *box);
GtkWidget * container_add     (GtkWidget *widget, GtkWidget *container);
GtkWidget * paned_pack        (GtkWidget *widget, int spot, bool resize, GtkWidget *paned);
GtkWidget * table_attach      (GtkWidget *widget, int left, int top,                        GtkWidget *table);
GtkWidget * table_attach_full (GtkWidget *widget, int left, int top, int width, int height, GtkWidget *table);
GtkWidget * menu_append       (GtkWidget *widget, GtkWidget *menu);
GtkWidget * set_submenu       (GtkWidget *submenu, GtkWidget *item);

#endif
