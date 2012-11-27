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

#include "panel.h"

#include <config.h>
#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <lib/util/num.h>
#include <main/thread/gui.h>

static void make_page (GtkWidget *flipbook, GtkWidget **hbox_main, GtkWidget **vbox_main, GtkWidget **terminal_scroll);
static void scrollfix_connect (GtkWidget *widget);
static void scrollfix_cb (GtkWidget *widget, GdkRectangle *rect, GtkWidget *scroll, GtkAdjustment *adj);

#include "panel_old.c"

/*  ┌--hbox_main-----------------------------------------------------------------------┐
 *  | ┌--panes--------------------------------------------------┐ ┌--scroll_right----┐ |
 *  | | ┌--vbox_main------------------------------------------┐ | | ┌--apt[]-------┐ | |
 *  | | | ┌--apt[]------------------------------------------┐ | | | |              | | |
 *  | | | |                       TOP                       | | | | |              | | |
 *  | | | └-------------------------------------------------┘ | | | |              | | |
 *  | | | ┌--apt[]------------------------------------------┐ | | | |              | | |
 *  | | | |                                                 | | | | |              | | |
 *  | | | |                                                 | | | | |              | | |
 *  | | | |                  LEFT (channels)                | | | | |              | | |
 *  | | | |                                                 | | | | |  RIGHT (hw)  | | |
 *  | | | |                                                 | | | | |              | | |
 *  | | | └-------------------------------------------------┘ | | | |              | | |
 *  | | | ┌--apt[]------------------------------------------┐ | | | |              | | |
 *  | | | |                      BOTTOM                     | | | | |              | | |
 *  | | | └-------------------------------------------------┘ | | | |              | | |
 *  | | └-----------------------------------------------------┘ | | |              | | |
 *  | | ┌--setup->terminal_scroll-----------------------------┐ | | |              | | |
 *  | | |                                                     | | | |              | | |
 *  | | |                                                     | | | |              | | |
 *  | | └-----------------------------------------------------┘ | | └--------------┘ | |
 *  | └---------------------------------------------------------┘ └------------------┘ |
 *  └----------------------------------------------------------------------------------┘
*/

void setup_init (Setup *setup, GtkWidget *flipbook)
{
	f_start(F_INIT);

	GtkWidget *hbox_main, *vbox_main;
	make_page(flipbook, &hbox_main, &vbox_main, &setup->terminal_scroll);

	setup->apt[SECTION_RIGHT] = container_add(gtk_vbox_new(0, 0),
	                            add_with_viewport(name_widget(gtk_event_box_new(), "m2_sbox"),
	                            container_add(name_widget(new_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_ALWAYS), "m2_scroll"),
	                            pack_start(new_alignment(M2_HALFSPACE, M2_HALFSPACE, 0, M2_HALFSPACE), 0, hbox_main))));

	setup->apt[SECTION_TOP]    = pack_start(gtk_hbox_new(0, 0), 0, vbox_main);
	setup->apt[SECTION_LEFT]   = pack_start(gtk_vbox_new(0, 0), 1, vbox_main);
	setup->apt[SECTION_BOTTOM] = pack_start(gtk_hbox_new(0, 0), 0, vbox_main);

	setup->apt[SECTION_WAYLEFT]  = NULL;
	setup->apt[SECTION_WAYRIGHT] = NULL;

	setup->apt[SECTION_NOWHERE] = container_add(gtk_vbox_new(0, 0), gtk_offscreen_window_new());

	scrollfix_connect(setup->apt[SECTION_RIGHT]);

	// make tools:
	message_init        (&setup->message,                 setup->apt);
	hardware_array_init (setup->hw,                       setup->apt);
	channel_array_init  (setup->channel, &setup->ch_sect, setup->apt);

	add_loc_menu(&setup->message.sect, SECTION_TOP, SECTION_BOTTOM, -1);
	/**/                                add_rollup_button(&setup->message.sect);
	for (int n = 0; n < M2_NUM_HW; n++) add_rollup_button(&setup->hw[n].sect);
}

void setup_register (Setup *setup)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Setup", MCF_W);

	message_register         (&setup->message, -1, setup->apt);
	hardware_array_register  (setup->hw, setup->apt);
	hardware_register_legacy (&setup->hw[0]);  // default to DAQ 0
	channel_array_register   (setup->channel, &setup->ch_sect, setup->apt);
}

/*  ┌--hbox_main-----------------------------------------------------------------------------------------┐
 *  | ┌--panes---------------------------------------------------------┐ ┌--scroll_right---------------┐ |
 *  | | ┌--vbox_main-------------------------------------------------┐ | | ┌--hbox_right-------------┐ | |
 *  | | | ┌--apt[]-------------------------------------------------┐ | | | | ┌--apt[]--┐ ┌--apt[]--┐ | | |
 *  | | | |                                                        | | | | | |         | |         | | | |
 *  | | | |                          TOP                           | | | | | |         | |         | | | |
 *  | | | |                                                        | | | | | |         | |         | | | |
 *  | | | └--------------------------------------------------------┘ | | | | |         | |         | | | |
 *  | | | ┌--hbox_middle-------------------------------------------┐ | | | | |         | |         | | | |
 *  | | | | ┌--scroll_left----------------┐ ┌--vbox_middle-------┐ | | | | | |         | |         | | | |
 *  | | | | | ┌--hbox_left--------------┐ | | ┌--plot_align----┐ | | | | | | |         | |         | | | |
 *  | | | | | | ┌--apt[]--┐ ┌--apt[]--┐ | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | | |         | |         | | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | | |         | |         | | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | | |         | |         | | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | | |  WAY-   | |  LEFT   | | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | | |  LEFT   | |         | | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | | |         | |         | | | | └----------------┘ | | | | | | |  RIGHT  | |  WAY-   | | | |
 *  | | | | | | |         | |         | | | | ┌--buffer_align--┐ | | | | | | |         | |  RIGHT  | | | |
 *  | | | | | | └---------┘ └---------┘ | | | |                | | | | | | | |         | |         | | | |
 *  | | | | | └-------------------------┘ | | └----------------┘ | | | | | | |         | |         | | | |
 *  | | | | └-----------------------------┘ └--------------------┘ | | | | | |         | |         | | | |
 *  | | | └--------------------------------------------------------┘ | | | | |         | |         | | | |
 *  | | | ┌--apt[]-------------------------------------------------┐ | | | | |         | |         | | | |
 *  | | | |                                                        | | | | | |         | |         | | | |
 *  | | | |                         BOTTOM                         | | | | | |         | |         | | | |
 *  | | | |                                                        | | | | | |         | |         | | | |
 *  | | | └--------------------------------------------------------┘ | | | | |         | |         | | | |
 *  | | └------------------------------------------------------------┘ | | | |         | |         | | | |
 *  | | ┌--panel->terminal_scroll------------------------------------┐ | | | |         | |         | | | |
 *  | | |                                                            | | | | |         | |         | | | |
 *  | | |                                                            | | | | └---------┘ └---------┘ | | |
 *  | | └------------------------------------------------------------┘ | | └-------------------------┘ | |
 *  | └----------------------------------------------------------------┘ └-----------------------------┘ |
 *  └----------------------------------------------------------------------------------------------------┘
*/

void panel_init (Panel *panel, int pid, GtkWidget *flipbook)
{
	f_start(F_INIT);

	panel->pid = pid;
	mt_mutex_init(&panel->sweep_mutex);
	mt_mutex_init(&panel->trigger_mutex);

	GtkWidget *hbox_main, *vbox_main;
	make_page(flipbook, &hbox_main, &vbox_main, &panel->terminal_scroll);

	GtkWidget *hbox_right = container_add(gtk_hbox_new(0, 0),
	                        add_with_viewport(name_widget(gtk_event_box_new(), "m2_sbox"),
	                        container_add(new_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_ALWAYS),
	                        pack_start(new_alignment(M2_HALFSPACE, M2_HALFSPACE, 0, M2_HALFSPACE), 0, hbox_main))));

	panel->apt[SECTION_TOP]    = pack_start(gtk_hbox_new(0, 0), 0, vbox_main);
	GtkWidget *hbox_middle     = pack_start(gtk_hbox_new(0, 0), 1, vbox_main);
	panel->apt[SECTION_BOTTOM] = pack_start(gtk_hbox_new(0, 0), 0, vbox_main);

	GtkWidget *hbox_left   = container_add(gtk_hbox_new(0, 0),
	                         add_with_viewport(name_widget(gtk_event_box_new(), "m2_sbox"),
	                         pack_start(new_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_ALWAYS),              0, hbox_middle)));
	GtkWidget *vbox_middle = container_add(gtk_vbox_new(0, 0),
	                         pack_start(new_alignment(M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE), 1, hbox_middle));

	panel->apt[SECTION_WAYLEFT] = pack_start(gtk_vbox_new(0, 0), 0, hbox_left);
	panel->apt[SECTION_LEFT]    = pack_start(gtk_vbox_new(0, 0), 0, hbox_left);

	panel->apt[SECTION_RIGHT]    = pack_start(gtk_vbox_new(0, 0), 0, hbox_right);
	panel->apt[SECTION_WAYRIGHT] = pack_start(gtk_vbox_new(0, 0), 0, hbox_right);

	panel->apt[SECTION_NOWHERE] = container_add(gtk_vbox_new(0, 0), gtk_offscreen_window_new());

	scrollfix_connect(hbox_left);
	scrollfix_connect(hbox_right);

	// make tools:
	message_init       (&panel->message, panel->apt);
	logger_init        (&panel->logger,  panel->apt);
	scope_init         (&panel->scope,   panel->apt);
	sweep_array_init   (panel->sweep,    panel->apt);
	trigger_array_init (panel->trigger,  panel->apt, &panel->trigger_sect);
	plot_init          (&panel->plot,    vbox_middle);
	buffer_init        (&panel->buffer,  vbox_middle);

	add_loc_menu(&panel->message.sect, SECTION_WAYLEFT, SECTION_LEFT, SECTION_TOP, SECTION_BOTTOM, SECTION_RIGHT, SECTION_WAYRIGHT, -1);
	add_rollup_button(&panel->message.sect);
}

void panel_final (Panel *panel)
{
	f_start(F_INIT);

	message_final       (&panel->message);
	logger_final        (&panel->logger);
	scope_final         (&panel->scope);
	sweep_array_final   (panel->sweep);
	trigger_array_final (panel->trigger);
	plot_final          (&panel->plot);
	buffer_final        (&panel->buffer);

	mt_mutex_clear(&panel->sweep_mutex);
	mt_mutex_clear(&panel->trigger_mutex);
}

void panel_register (Panel *panel, ChanSet *chanset)
{
	f_start(F_INIT);

	message_register       (&panel->message, panel->pid, panel->apt);
	logger_register        (&panel->logger,  panel->pid, panel->apt);
	scope_register         (&panel->scope,   panel->pid, panel->apt);
	sweep_array_register   (panel->sweep,    panel->pid, panel->apt, chanset);
	trigger_array_register (panel->trigger,  panel->pid, panel->apt, &panel->trigger_sect, &panel->trigger_mutex);
	plot_register          (&panel->plot,    panel->pid);
	buffer_register        (&panel->buffer,  panel->pid, chanset);
}

void panel_register_legacy (Panel *panel)
{
	f_start(F_INIT);

	logger_register_legacy (&panel->logger);
	plot_register_legacy   (&panel->plot);
	scope_register_legacy  (&panel->scope);

	for (int n = 0; n < M2_OLD_NUM_SWEEP; n++) sweep_register_legacy(&panel->sweep[n], atg(supercat("sweep%d_", n)));
}

void scrollfix_connect (GtkWidget *widget)
{
	GtkWidget *scroll = gtk_widget_get_ancestor(widget, GTK_TYPE_SCROLLED_WINDOW);
	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));

	snazzy_connect(widget, "size-allocate", SNAZZY_VOID_PTR, BLOB_CALLBACK(scrollfix_cb), 0x20, scroll, adj);
}

void scrollfix_cb (GtkWidget *widget, GdkRectangle *rect, GtkWidget *scroll, GtkAdjustment *adj)
{
	f_start(F_CALLBACK);

	GtkRequisition req;
	gtk_widget_size_request(widget, &req);
	int vis_height = (int) gtk_adjustment_get_page_size(adj);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, vis_height < req.height + 2 * M2_HALFSPACE ? GTK_POLICY_ALWAYS : GTK_POLICY_NEVER);
	gtk_widget_queue_resize(scroll);
}

void make_page (GtkWidget *flipbook, GtkWidget **hbox_main, GtkWidget **vbox_main, GtkWidget **terminal_scroll)
{
	*hbox_main = pack_start(set_no_show_all(gtk_hbox_new(0,0)), 1, flipbook);
#ifndef MINGW
	GtkWidget *panes = pack_start(gtk_vpaned_new(), 1, *hbox_main);

	GtkWidget *align_main = paned_pack(new_alignment(M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE, 0), 1, 1, panes);
	*terminal_scroll      = container_add(new_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS),
	                        container_add(fix_shadow(gtk_frame_new(NULL)),
	                        paned_pack(new_alignment(0, 0, 0, M2_HALFSPACE),                       2, 0, panes)));
#else
	GtkWidget *align_main = pack_start(new_alignment(M2_HALFSPACE, M2_HALFSPACE, M2_HALFSPACE, 0), 1, *hbox_main);
	*terminal_scroll      = NULL;
#endif
	*vbox_main = container_add(gtk_vbox_new(0, 0), align_main);
}
