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

static gboolean draw_on_expose_cb (GtkWidget *widget, void *ptr, GtkWidget *child);
static void snazzy_void_void_cb (GtkWidget *widget, Blob *blob);
static void snazzy_void_int_cb (GtkWidget *widget, gint id, Blob *blob);
static void snazzy_void_ptr_cb (GtkWidget *widget, void *ptr, Blob *blob);
static gboolean snazzy_bool_ptr_cb (GtkWidget *widget, void *ptr, Blob *blob);

gboolean draw_on_expose_cb (GtkWidget *widget, void *ptr, GtkWidget *child)
{
	f_print(F_VERBOSE, "Queued extra draw signal.\n");

	gtk_widget_queue_draw(child);
	return 0;
}

void snazzy_void_void_cb (GtkWidget *widget, Blob *blob)
{
#define _ACTION 
#define _TYPE void
#define _CAST GtkWidget*
#define _CALL widget
	blob_switch();
#undef _ACTION
#undef _TYPE
#undef _CAST
#undef _CALL
}

void snazzy_void_int_cb (GtkWidget *widget, gint id, Blob *blob)
{
#define _ACTION 
#define _TYPE void
#define _CAST GtkWidget*, gint
#define _CALL widget, id
	blob_switch();
#undef _ACTION
#undef _TYPE
#undef _CAST
#undef _CALL
}

void snazzy_void_ptr_cb (GtkWidget *widget, void *ptr, Blob *blob)
{
#define _ACTION 
#define _TYPE void
#define _CAST GtkWidget*, void*
#define _CALL widget, ptr
	blob_switch();
#undef _ACTION
#undef _TYPE
#undef _CAST
#undef _CALL
}

gboolean snazzy_bool_ptr_cb (GtkWidget *widget, void *ptr, Blob *blob)
{
#define _ACTION return
#define _TYPE gboolean
#define _CAST GtkWidget*, void*
#define _CALL widget, ptr
	blob_switch();
	return 0;  // just in case blob has an erroneous sig, causing control to fall through blob_switch() and blob_switch_inner()
#undef _ACTION
#undef _TYPE
#undef _CAST
#undef _CALL
}

