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

#include "message.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/fs.h>

#include "message_callback.c"

void message_init (Message *message, GtkWidget **apt)
{
	f_start(F_INIT);

	section_init(&message->sect, atg(sharepath("pixmaps/tool_message.png")), "Message", apt);
	// (add rollup and location buttons later)

	message->mini_entry  = pack_start(set_no_show_all(size_widget(new_entry(0, 0),       M2_MESSAGE_WIDTH, -1)), 1, message->sect.heading);
	message->mini_spacer = pack_start(set_no_show_all(size_widget(new_label("", 0, 0.0), M2_MESSAGE_WIDTH, -1)), 1, message->sect.heading);

	gtk_widget_set_can_focus(message->mini_entry, 0);
	gtk_editable_set_editable(GTK_EDITABLE(message->mini_entry), 0);
	set_padding(message->mini_entry,  2);
	set_padding(message->mini_spacer, 2);

	message->scroll = container_add(size_widget(new_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS), -1, M2_MESSAGE_HEIGHT),
	                  pack_start(gtk_frame_new(NULL), 1, message->sect.box));
	GtkWidget *textview = container_add(new_text_view(0, 0), message->scroll);

	g_object_set(G_OBJECT(textview), "border-width", 1, NULL);
	message->textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	message->minimode = 1;
}

void message_register (Message *message, int pid, GtkWidget **apt)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Message", MCF_W);
	section_register(&message->sect, (pid == -1) ? "setup_message_" : atg(supercat("panel%d_message_", pid)), SECTION_TOP, apt);
	
	snazzy_connect(message->sect.box,  "show, hide", SNAZZY_VOID_VOID, BLOB_CALLBACK(revis_cb),    0x10, message);  // turn mini-readout entry on/off
	snazzy_connect(message->sect.full, "parent-set", SNAZZY_VOID_PTR,  BLOB_CALLBACK(reparent_cb), 0x10, message);  // set sect->expand_fill where appropriate
}

void message_update (Message *message)
{
	if (message->minimode)
	{
		char *text _strfree_ = status_get_last_line();
		gtk_entry_set_text(GTK_ENTRY(message->mini_entry), text);
	}
	else
	{
		char *text _strfree_ = status_get_text();
		gtk_text_buffer_set_text(message->textbuf, text, -1);
		scroll_down(message->scroll);
	}
}

void message_final (Message *message)
{
	f_start(F_INIT);

	// nothing to do
}
