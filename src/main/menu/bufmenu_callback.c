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

static gboolean check_item_cb (GtkWidget *widget, GdkEvent *event, bool *var);
static void check_item_mcf (bool *var, const char *signal_name, MValue value, GtkWidget *widget);

gboolean check_item_cb (GtkWidget *widget, GdkEvent *event, bool *var)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	*var = !was_active;

	return 0;
}

void check_item_mcf (bool *var, const char *signal_name, MValue value, GtkWidget *widget)
{
	f_start(F_MCF);

	*var = value.x_bool;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), value.x_bool);
}
