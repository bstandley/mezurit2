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

static void reparent_cb (GtkWidget *widget, GtkWidget *old, Message *message);
static void revis_cb (GtkWidget *widget, Message *message);

void reparent_cb (GtkWidget *widget, GtkWidget *old, Message *message)
{
	f_start(F_CALLBACK);

	message->sect.expand_fill = !message->minimode || message->sect.location == SECTION_TOP || message->sect.location == SECTION_BOTTOM;
	// locate_section_cb() will call set_expand_fill() automatically
}

void revis_cb (GtkWidget *widget, Message *message)
{
	f_start(F_CALLBACK);

	message->minimode = !gtk_widget_get_visible(widget);

	set_visibility(message->mini_entry,   message->minimode);
	set_visibility(message->mini_spacer, !message->minimode);

	message->sect.expand_fill = !message->minimode || message->sect.location == SECTION_TOP || message->sect.location == SECTION_BOTTOM;
	set_expand_fill(message->sect.full, message->sect.expand_fill);

	message_update(message);
}
