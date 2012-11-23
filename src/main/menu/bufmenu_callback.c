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

static void check_item_cb (GtkWidget *widget, bool *var);
static void check_item_mcf (bool *var, const char *signal_name, MValue value, GtkWidget *widget);

void check_item_cb (GtkWidget *widget, bool *var)
{
	f_start(F_CALLBACK);

	*var = !(*var);
	set_item_checked(widget, *var);
}

void check_item_mcf (bool *var, const char *signal_name, MValue value, GtkWidget *widget)
{
	f_start(F_MCF);

	*var = value.x_bool;
	set_item_checked(widget, value.x_bool);
}

