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

#include "gui.h"

#include <stdlib.h>  // malloc(), free()
#include <string.h>

#include <lib/status.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <lib/hardware/timing.h>

static char *last_dirname;
static GtkCssProvider *css_provider;

static void destroy_snazzy_var (Blob *blob, GClosure *closure);
static GtkWidget * deparent (GtkWidget *widget);

#include "gui_callback.c"
#include "gui_construct.c"
#include "gui_pack.c"

// Snazzy callbacks:

void snazzy_connect (GtkWidget *widget, const char *signal_list, int type, BlobCallback cb, int sig, ...)
{
	GCallback gcb = (type == SNAZZY_VOID_VOID) ? G_CALLBACK(snazzy_void_void_cb) :
	                (type == SNAZZY_VOID_INT)  ? G_CALLBACK(snazzy_void_int_cb)  :
	                (type == SNAZZY_VOID_PTR)  ? G_CALLBACK(snazzy_void_ptr_cb)  :
	                (type == SNAZZY_BOOL_PTR)  ? G_CALLBACK(snazzy_bool_ptr_cb)  : NULL;

	if (gcb == NULL)
	{
		f_print(F_ERROR, "Error: Unrecognized callback function type.\n");
		return;
	}

	char *str _strfree_ = cat1(signal_list);
	for (char *signal_name = strtok(str, ", "); signal_name != NULL; signal_name = strtok(NULL, ", "))
	{
		Blob *blob = malloc(sizeof(Blob));
		fill_blob(blob, cb, sig);
		g_signal_connect_data(widget, signal_name, gcb, blob, (GClosureNotify) destroy_snazzy_var, 0);
	}
}

void destroy_snazzy_var (Blob *blob, GClosure *closure)
{
	free(blob);
}

// Utilities:

void show_all (GtkWidget *widget, void *ptr)
{
	if (G_TYPE_CHECK_INSTANCE_TYPE(widget, GTK_TYPE_CONTAINER)) gtk_container_foreach(GTK_CONTAINER(widget), (GtkCallback) show_all, NULL);

	if (G_TYPE_CHECK_INSTANCE_TYPE(widget, GTK_TYPE_MENU_ITEM))
	{
		GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
		if (sub != NULL) show_all(sub, NULL);
	}

	if (!gtk_widget_get_no_show_all(widget)) gtk_widget_show(widget);
}

GtkWidget * set_text_view_text (GtkWidget *widget, const char *str)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
	gtk_text_buffer_set_text(buffer, str, -1);
	return widget;
}

char * run_file_chooser (const char *title, int action, const char *preset)
{
	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(title, NULL, action == FILE_CHOOSER_OPEN ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
	if (action == FILE_CHOOSER_SAVE) gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER(chooser), 1);
	if (preset != NULL)
	{
		char *dirname _strfree_ = extract_dir(preset);
		char *basname _strfree_ = extract_base(preset);

		if (dirname != NULL)                    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), dirname);
		if (dirname != NULL && basname != NULL) gtk_file_chooser_set_current_name  (GTK_FILE_CHOOSER(chooser), basname);
	}
	else gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), last_dirname);

	bool ok = (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT);
	char *filename = ok ? catg(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser))) : NULL;
	g_object_unref(chooser);

	if (preset == NULL && filename != NULL) replace(last_dirname, extract_dir(filename));

	return filename;
}

bool run_yes_no_dialog (GtkWidget *window, const char *message)
{
	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s", message);

	bool rv = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES);
	gtk_widget_destroy(dialog);

	return rv;
}

GtkWidget * set_expand_fill (GtkWidget *widget, bool expand_fill)
{
	guint padding;
	GtkPackType packtype;

	GtkBox *box = GTK_BOX(gtk_widget_get_parent(widget));

	gtk_box_query_child_packing(box, widget, NULL,        NULL,        &padding, &packtype);
	gtk_box_set_child_packing  (box, widget, expand_fill, expand_fill, padding,  packtype);

	return widget;
}

GtkWidget * size_widget (GtkWidget *widget, gint x, gint y)
{
	gtk_widget_set_size_request(widget, x, y);
	return widget;
}

GtkWidget * flatten_button (GtkWidget *widget)
{
	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
	gtk_widget_set_focus_on_click(widget, 0);
	return widget;
}

GtkWidget * set_padding (GtkWidget *widget, guint padding)  // TODO: redundant with set_margins?
{
	gtk_container_child_set(GTK_CONTAINER(gtk_widget_get_parent(widget)), widget, "padding", padding, NULL);
	return widget;
}

GtkWidget * set_margins (GtkWidget *widget, gint top, gint bottom, gint left, gint right)
{
	gtk_widget_set_margin_top   (widget, top);
	gtk_widget_set_margin_bottom(widget, bottom);
	gtk_widget_set_margin_start (widget, left);
	gtk_widget_set_margin_end   (widget, right);

	return widget;
}

GtkWidget * deparent (GtkWidget *widget)
{
	GtkWidget *parent = gtk_widget_get_parent(widget);
	if (parent != NULL)
	{
		g_object_ref(G_OBJECT(widget));
		gtk_container_remove(GTK_CONTAINER(parent), widget);
	}

	return parent;
}

GtkWidget * attach_window (GtkWidget *target, GtkWidget *parent)
{
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_decorated         (GTK_WINDOW(window), 0);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(window), 1);
	gtk_window_set_skip_pager_hint   (GTK_WINDOW(window), 1);
	gtk_window_set_transient_for     (GTK_WINDOW(window), GTK_WINDOW(gtk_widget_get_toplevel(parent)));
	gtk_window_set_position          (GTK_WINDOW(window), GTK_WIN_POS_MOUSE);

	return container_add(target, window);
}

GtkWidget * set_submenu (GtkWidget *submenu, GtkWidget *item)
{
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	return submenu;
}

GtkWidget * set_no_show_all (GtkWidget *widget)
{
	gtk_widget_set_no_show_all(widget, 1);
	return widget;
}

GtkWidget * set_visibility (GtkWidget *widget, bool visible)
{
	if (visible) gtk_widget_show(widget);
	else         gtk_widget_hide(widget);
	return widget;
}

GtkWidget * get_child (GtkWidget *widget, int n)
{
	GList *list = gtk_container_get_children(GTK_CONTAINER(widget));
	GtkWidget *child = (n < 0)                         ? GTK_WIDGET(g_list_nth_data(list, g_list_length(list) - 1)) :
	                   (n < (int) g_list_length(list)) ? GTK_WIDGET(g_list_nth_data(list, (guint) n))               : NULL;
	g_list_free(list);
	return child;
}

void scroll_down (GtkWidget *widget)
{
	GtkAdjustment *adjust = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(widget));
	gtk_adjustment_set_value(adjust, gtk_adjustment_get_upper(adjust));
}

// Persistent state for run_file_chooser(), CSS theme:

void gui_init (void)
{
	f_start(F_INIT);

	last_dirname = catg(g_get_current_dir());

	css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_path(css_provider, atg(sharepath("themes/lightdark.css")), NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
	
void gui_final (void)
{
	f_start(F_INIT);

	free(last_dirname);

	gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css_provider));
	g_object_unref(G_OBJECT(css_provider));
}
