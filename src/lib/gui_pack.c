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

GtkWidget * pack_start (GtkWidget *widget, bool expand_fill, GtkWidget *box)
{
	GtkWidget *parent = deparent(widget);
	gtk_box_pack_start(GTK_BOX(box), widget, expand_fill, expand_fill, 0);
	if (parent != NULL) g_object_unref(G_OBJECT(widget));
	return widget;
}

GtkWidget * pack_end (GtkWidget *widget, bool expand_fill, GtkWidget *box)
{
	GtkWidget *parent = deparent(widget);
	gtk_box_pack_end(GTK_BOX(box), widget, expand_fill, expand_fill, 0);
	if (parent != NULL) g_object_unref(G_OBJECT(widget));
	return widget;
}

GtkWidget * container_add (GtkWidget *widget, GtkWidget *container)
{
	GtkWidget *parent = deparent(widget);
	gtk_container_add(GTK_CONTAINER(container), widget);
	if (parent != NULL) g_object_unref(G_OBJECT(widget));
	return widget;
}

GtkWidget * paned_pack (GtkWidget *widget, int spot, bool resize, GtkWidget *paned)
{
	if (spot == 1) gtk_paned_pack1(GTK_PANED(paned), widget, resize, 0);
	else           gtk_paned_pack2(GTK_PANED(paned), widget, resize, 0);
	return widget;
}

GtkWidget * table_attach (GtkWidget *widget, int left, int top, GtkWidget *table)
{
	return table_attach_full(widget, left, top, 1, 1, table);
}

GtkWidget * table_attach_full (GtkWidget *widget, int left, int top, int width, int height, GtkWidget *table)
{
	gtk_grid_attach(GTK_GRID(table), widget, left, top, width, height);
	return widget;
}

GtkWidget * menu_append (GtkWidget *widget, GtkWidget *menu)
{
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
	return widget;
}

GtkWidget * add_with_viewport (GtkWidget *widget, GtkWidget *parent)
{
	gtk_container_add(GTK_CONTAINER(parent), widget);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_widget_get_parent(widget)), GTK_SHADOW_NONE);
	return widget;
}
