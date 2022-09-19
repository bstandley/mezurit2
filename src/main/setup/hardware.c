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

#include "hardware.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/entry.h>
#include <lib/util/str.h>
#include <lib/util/fs.h>
#include <lib/hardware/daq.h>
#include <lib/hardware/gpib.h>

#if COMEDI
#define HW_DAQ_DRIVER_ID 0
#elif NIDAQ
#define HW_DAQ_DRIVER_ID 1
#elif NIDAQMX
#define HW_DAQ_DRIVER_ID 2
#else
#define HW_DAQ_DRIVER_ID 3
#endif

#if LINUXGPIB
#define HW_GPIB_DRIVER_ID 0
#elif NI488
#define HW_GPIB_DRIVER_ID 1
#else
#define HW_GPIB_DRIVER_ID 3
#endif

enum
{
	HW_DAQ,
	HW_GPIB
};

static void hardware_update (Hardware *hw);

#include "hardware_callback.c"

void hardware_array_init (Hardware *hw_array, GtkWidget **apt)
{
	f_start(F_INIT);

	for (int n = 0; n < M2_NUM_HW; n++)
	{
		Hardware *hw = &hw_array[n];

		char *icon_filename = NULL;
		char *label_str = NULL;
		if (n < M2_NUM_DAQ)
		{
			hw->type = HW_DAQ;
			hw->id = n;
			hw->real = (hw->id != M2_VDAQ_ID);
			icon_filename = atg(sharepath("pixmaps/tool_daq.png"));
			label_str = hw->real ? atg(supercat("DAQ %d", hw->id)) : atg(cat1("VDAQ"));
		}
		else
		{
			hw->type = HW_GPIB;
			hw->id = n - M2_NUM_DAQ;
			hw->real = (hw->id != M2_VGPIB_ID);
			icon_filename = atg(sharepath("pixmaps/tool_gpib.png"));
			label_str = hw->real ? atg(supercat("GPIB %d", hw->id)) : atg(cat1("VGPIB"));
		}

		// section:
		section_init(&hw->sect, icon_filename, label_str, apt);
		// (add rollup button later)

		hw->mini_entry  = pack_start(set_no_show_all(size_widget(new_entry(0, 0),    M2_HARDWARE_WIDTH, -1)), 1, hw->sect.heading);
		hw->mini_spacer = pack_start(set_no_show_all(size_widget(new_label("", 0.0), M2_HARDWARE_WIDTH, -1)), 1, hw->sect.heading);

		gtk_widget_set_can_focus(hw->mini_entry, 0);
		gtk_editable_set_editable(GTK_EDITABLE(hw->mini_entry), 0);
		set_padding(hw->mini_entry,  2);
		set_padding(hw->mini_spacer, 2);

		for (int d = 0; d < 4; d++) hw->node[d] = cat1("");
		hw->dummy  = 1;  // set defaults for non-real boards which are not registered and therefore omitted from mcf_load_defaults()
		hw->settle = 0;

		GtkWidget *textview = container_add(new_text_view(4, 4),
		                      pack_start(gtk_frame_new(NULL),                        1, hw->sect.box));
		GtkWidget *hbox     = pack_end  (gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4), 0, hw->sect.box);

		g_object_set(G_OBJECT(textview), "border-width", 1, NULL);
		hw->textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

		hw->node_entry   = hw->real ? pack_start(new_entry(0, -1),                         1, hbox) : NULL;
		hw->dummy_button =            pack_end  (gtk_check_button_new_with_label("Dummy"), 0, hbox);

		if (!hw->real)
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hw->dummy_button), hw->dummy);
			gtk_widget_set_sensitive(hw->dummy_button, 0);
		}

		hw->block_update = 0;
		hw->dirty = 1;  // run hardware_update() on !real hardware at startup
	}
}

void hardware_array_block (Hardware *hw_array)
{
	for (int n = 0; n < M2_NUM_HW; n++) hw_array[n].block_update = 1;
}

void hardware_array_unblock (Hardware *hw_array)
{
	for (int n = 0; n < M2_NUM_HW; n++)
	{
		hw_array[n].block_update = 0;
		if (hw_array[n].dirty) hardware_update(&hw_array[n]);
	}
}

void hardware_update (Hardware *hw)
{
	f_start(F_UPDATE);

	if (hw->block_update) hw->dirty = 1;
	else
	{
		hw->dirty = 0;

		if (hw->node_entry != NULL) gtk_widget_set_sensitive(hw->node_entry, !hw->dummy);

		if (hw->type == HW_DAQ)
		{
			bool c = daq_board_connect(hw->id, hw->dummy ? "dummy" : hw->node[HW_DAQ_DRIVER_ID], hw->settle);

			gtk_text_buffer_set_text(hw->textbuf, atg(supercat("Driver:\t%s\nMode:\t%s\nBoard:\t%s%s",
			                                                   daq_board_info(hw->id, "driver"),
			                                                   c ? "Connected" : "Not connected",
			                                                   daq_board_info(hw->id, "board"),
			                                                   c ? atg(supercat("\nInput:\t%s\nOutput:\t%s\nSettling:\t%s",
			                                                                    daq_board_info(hw->id, "input"),
			                                                                    daq_board_info(hw->id, "output"),
			                                                                    daq_board_info(hw->id, "settle"))) : "")), -1);

			gtk_entry_set_text(GTK_ENTRY(hw->mini_entry), atg(supercat("%s (%s)", daq_board_info(hw->id, "board_abrv"), daq_board_info(hw->id, "full_node"))));
		}
		else if (hw->type == HW_GPIB)
		{
			bool c = gpib_board_connect(hw->id, hw->dummy ? "dummy" : hw->node[HW_GPIB_DRIVER_ID]);

			gtk_text_buffer_set_text(hw->textbuf, atg(supercat("Driver:\t%s\nMode:\t%s\nBoard:\t%s",
			                                                   gpib_board_info(hw->id, "driver"),
			                                                   c ? "Connected" : "Not connected",
			                                                   gpib_board_info(hw->id, "board"))), -1);

			gtk_entry_set_text(GTK_ENTRY(hw->mini_entry), atg(supercat("%s (%s)", gpib_board_info(hw->id, "board_abrv"), gpib_board_info(hw->id, "full_node"))));
		}
	}
}

void hardware_array_final (Hardware *hw_array)
{
	f_start(F_INIT);

	for (int n = 0; n < M2_NUM_HW; n++) for (int d = 0; d < 4; d++) replace(hw_array[n].node[d], NULL);
}

void hardware_array_register (Hardware *hw_array, GtkWidget **apt)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Hardware", MCF_W);

	for (int n = 0; n < M2_NUM_HW; n++)
	{
		Hardware *hw = &hw_array[n];

		int dummy_var = -1, settle_var = -1, node_var0 = -1, node_var1 = -1, node_var2 = -1;
		if (hw->type == HW_DAQ)
		{
			section_register(&hw->sect, atg(supercat("daq%d_", hw->id)), SECTION_RIGHT, apt);

			if (hw->real)
			{
				dummy_var  = mcf_register(&hw->dummy,   atg(supercat("daq%d_dummy",        hw->id)), MCF_BOOL   | MCF_W | MCF_DEFAULT, hw->id != 0);
				settle_var = mcf_register(&hw->settle,  atg(supercat("daq%d_settling",     hw->id)), MCF_INT    | MCF_W | MCF_DEFAULT, 0);
				node_var0  = mcf_register(&hw->node[0], atg(supercat("daq%d_node_comedi",  hw->id)), MCF_STRING | MCF_W | MCF_DEFAULT, atg(supercat("/dev/comedi%d", hw->id)));
				node_var1  = mcf_register(&hw->node[1], atg(supercat("daq%d_node_nidaq",   hw->id)), MCF_STRING | MCF_W | MCF_DEFAULT, atg(supercat("%d",            hw->id + 1)));
				node_var2  = mcf_register(&hw->node[2], atg(supercat("daq%d_node_nidaqmx", hw->id)), MCF_STRING | MCF_W | MCF_DEFAULT, atg(supercat("Dev%d",         hw->id + 1)));
			}
		}
		else if (hw->type == HW_GPIB)
		{
			section_register(&hw->sect, atg(supercat("gpib%d_", hw->id)), SECTION_RIGHT, apt);

			if (hw->real)
			{
				dummy_var = mcf_register(&hw->dummy,   atg(supercat("gpib%d_dummy",          hw->id)), MCF_BOOL   | MCF_W | MCF_DEFAULT, hw->id != 0);
				node_var0 = mcf_register(&hw->node[0], atg(supercat("gpib%d_node_linuxgpib", hw->id)), MCF_STRING | MCF_W | MCF_DEFAULT, atg(supercat("/dev/gpib%d", hw->id)));
				node_var1 = mcf_register(&hw->node[1], atg(supercat("gpib%d_node_ni488",     hw->id)), MCF_STRING | MCF_W | MCF_DEFAULT, atg(supercat("%d",          hw->id)));
			}
		}
	
		if (dummy_var  != -1) mcf_connect(dummy_var,  "setup", BLOB_CALLBACK(hardware_dummy_mcf),  0x10, hw);
		if (settle_var != -1) mcf_connect(settle_var, "setup", BLOB_CALLBACK(hardware_settle_mcf), 0x10, hw);
		if (node_var0  != -1) mcf_connect(node_var0,  "setup", BLOB_CALLBACK(hardware_node_mcf),   0x11, hw, 0);
		if (node_var1  != -1) mcf_connect(node_var1,  "setup", BLOB_CALLBACK(hardware_node_mcf),   0x11, hw, 1);
		if (node_var2  != -1) mcf_connect(node_var2,  "setup", BLOB_CALLBACK(hardware_node_mcf),   0x11, hw, 2);
		// don't connect the "no driver" node

		if (hw->real)
		{
			snazzy_connect(hw->node_entry,   "key-press-event, focus-out-event", SNAZZY_BOOL_PTR, BLOB_CALLBACK(hardware_node_cb),  0x10, hw);
			snazzy_connect(hw->dummy_button, "button-release-event",             SNAZZY_BOOL_PTR, BLOB_CALLBACK(hardware_dummy_cb), 0x10, hw);
		}

		snazzy_connect(hw->sect.box, "show, hide", SNAZZY_VOID_VOID, BLOB_CALLBACK(revis_cb), 0x10, hw);
	}
}

void hardware_register_legacy (Hardware *hw)
{
	f_start(F_INIT);

	int dummy_var = mcf_register(&hw->dummy, "dummy_daq", MCF_BOOL);
	mcf_connect_with_note(dummy_var, "setup", "Mapped obsolete hardware var onto 'Primary' device.\n", BLOB_CALLBACK(hardware_dummy_mcf), 0x10, hw);
}
