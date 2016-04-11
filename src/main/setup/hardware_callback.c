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

static gboolean hardware_node_cb (GtkWidget *widget, GdkEvent *event, Hardware *hw);
static gboolean hardware_dummy_cb (GtkToggleButton *button, GdkEvent *event, Hardware *hw);
static void revis_cb (GtkWidget *widget, Hardware *hw);

static void hardware_node_mcf (void *ptr, const char *signal_name, MValue value, Hardware *hw, int d);
static void hardware_dummy_mcf (void *ptr, const char *signal_name, MValue value, Hardware *hw);
static void hardware_settle_mcf (void *ptr, const char *signal_name, MValue value, Hardware *hw);

gboolean hardware_node_cb (GtkWidget *widget, GdkEvent *event, Hardware *hw)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		if      (hw->type == HW_DAQ)  { replace(hw->node[HW_DAQ_DRIVER_ID],  cat1(gtk_entry_get_text(GTK_ENTRY(widget)))); }
		else if (hw->type == HW_GPIB) { replace(hw->node[HW_GPIB_DRIVER_ID], cat1(gtk_entry_get_text(GTK_ENTRY(widget)))); }
		hardware_update(hw);
	}

	return 0;
}

void hardware_node_mcf (void *ptr, const char *signal_name, MValue value, Hardware *hw, int d)
{
	f_start(F_MCF);

	replace(hw->node[d], cat1(value.string));  // preserve alternative driver nodes

	if ((hw->type == HW_DAQ  && d == HW_DAQ_DRIVER_ID) ||
	    (hw->type == HW_GPIB && d == HW_GPIB_DRIVER_ID))
	{
		gtk_entry_set_text(GTK_ENTRY(hw->node_entry), value.string);
		hardware_update(hw);
	}
}

gboolean hardware_dummy_cb (GtkToggleButton *button, GdkEvent *event, Hardware *hw)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_toggle_button_get_active(button);
	hw->dummy = !was_active;

	hardware_update(hw);
	return 0;
}

void hardware_dummy_mcf (void *ptr, const char *signal_name, MValue value, Hardware *hw)
{
	f_start(F_MCF);

	hw->dummy = value.x_bool;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->dummy_button), value.x_bool);

	hardware_update(hw);
}

void hardware_settle_mcf (void *ptr, const char *signal_name, MValue value, Hardware *hw)
{
	f_start(F_MCF);

	hw->settle = value.x_int;

	hardware_update(hw);
}

void revis_cb (GtkWidget *widget, Hardware *hw)
{
	f_start(F_CALLBACK);

	bool minimode = !gtk_widget_get_visible(widget);

	set_visibility(hw->mini_entry,   minimode);
	set_visibility(hw->mini_spacer, !minimode);
}
