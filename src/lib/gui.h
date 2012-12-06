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
#define HEADER_SANS_WARNINGS <gtk/gtk.h>
#include <sans_warnings.h>

#include <lib/blob.h>

// Persistent state for run_file_chooser(), lookup_pixbuf():

void gui_init  (bool darkpanel);
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

void show_all           (GtkWidget *widget, void *ptr);
void set_draw_on_expose (GtkWidget *widget, GtkWidget *child);
void set_flipbook_page  (GtkWidget *widget, int page);
void scroll_down        (GtkWidget *widget);

int get_N_children     (GtkWidget *widget);
int get_N_vis_children (GtkWidget *widget);

GtkWidget * name_widget        (GtkWidget *widget, const char *name);
GtkWidget * set_no_show_all    (GtkWidget *widget);
GtkWidget * set_visibility     (GtkWidget *widget, bool visible);
GtkWidget * size_widget        (GtkWidget *widget, gint x, gint y);
GtkWidget * fix_shadow         (GtkWidget *widget);
GtkWidget * flatten_button     (GtkWidget *widget);
GtkWidget * set_padding        (GtkWidget *widget, guint padding);
GtkWidget * set_expand_fill    (GtkWidget *widget, bool expand_fill);
GtkWidget * set_text_view_text (GtkWidget *widget, const char *str);
GtkWidget * set_item_checked   (GtkWidget *widget, bool checked);
GtkWidget * get_child          (GtkWidget *widget, int n);
GtkWidget * attach_window      (GtkWidget *target, GtkWidget *parent);

char * run_file_chooser  (const char *title, int action, const char *stock_id, const char *preset);
bool   run_yes_no_dialog (const char *message);

// Custom constructors:

GtkWidget * new_box (GtkOrientation orientation, gint spacing);
GtkWidget * new_label (const char *str, double xalign);
GtkWidget * new_alignment (guint top, guint bottom, guint left, guint right);
GtkWidget * new_entry (int max, int width);
GtkWidget * new_combo_text (void);
GtkWidget * new_text_view (gint left_margin, gint right_margin);
GtkWidget * new_button_with_icon (const char *label_str, const char *icon);
GtkWidget * new_toggle_button_with_icon (const char *label_str, const char *icon, GtkPositionType pos);
GtkWidget * new_table (guint row_spacing, guint col_spacing);
GtkWidget * new_item (GtkWidget *child, GtkWidget *image);
GtkWidget * new_scrolled_window (GtkPolicyType hpolicy, GtkPolicyType vpolicy);

// Custom packing:

GtkWidget * pack_start        (GtkWidget *widget, bool expand_fill, GtkWidget *box);
GtkWidget * pack_end          (GtkWidget *widget, bool expand_fill, GtkWidget *box);
GtkWidget * container_add     (GtkWidget *widget, GtkWidget *container);
GtkWidget * combo_text_append (GtkWidget *widget, const char *text);
GtkWidget * paned_pack        (GtkWidget *widget, int spot, bool resize, GtkWidget *paned);
GtkWidget * add_with_viewport (GtkWidget *widget, GtkWidget *parent);
GtkWidget * table_attach      (GtkWidget *widget, int left, int top,                        GtkAttachOptions h_opts,                                                      GtkWidget *table);
GtkWidget * table_attach_full (GtkWidget *widget, int left, int top, int width, int height, GtkAttachOptions h_opts, GtkAttachOptions v_opts, int xpadding, int ypadding, GtkWidget *table);
GtkWidget * menu_append       (GtkWidget *widget, GtkWidget *menu);
GtkWidget * set_submenu       (GtkWidget *submenu, GtkWidget *item);

// Pre-loaded images:

enum
{
	PIXBUF_ICON_CHECK = 0,
	PIXBUF_ICON_BLANK,
	PIXBUF_ICON_PAGE,
	PIXBUF_ICON_ACTION,
	PIXBUF_ICON_ROLLUP,
	PIXBUF_ICON_ROLLDOWN,
	PIXBUF_ICON_HORIZONTAL,
	PIXBUF_ICON_VERTICAL,
	PIXBUF_ICON_WAYLEFT,
	PIXBUF_ICON_LEFT,
	PIXBUF_ICON_TOP,
	PIXBUF_ICON_BOTTOM,
	PIXBUF_ICON_RIGHT,
	PIXBUF_ICON_WAYRIGHT,
	PIXBUF_ICON_SWEEP,
	PIXBUF_ICON_JUMP,
	PIXBUF_RL_STOP,
	PIXBUF_RL_HOLD,
	PIXBUF_RL_WAIT,
	PIXBUF_RL_READY,
	PIXBUF_RL_IDLE,
	PIXBUF_RL_RECORD,
	PIXBUF_RL_SCAN
};

GdkPixbuf * lookup_pixbuf (int id);

#endif
