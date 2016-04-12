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

#include "daq.h"

#include <stdlib.h>  // malloc()
#include <unistd.h>
#include <math.h>

#include <lib/status.h>
#include <lib/util/str.h>
#include <lib/util/num.h>
#include <lib/hardware/timing.h>

#if COMEDI
#include <comedilib.h>
#elif NIDAQ
#include <nidaq.h>
#include <stdio.h>  // sscanf()
#elif NIDAQMX
#include <stdio.h>  // sprintf()
#include <stdint.h>
#define _NI_int64_DEFINED_
#define _NI_uInt64_DEFINED_
typedef int64_t int64;
typedef uint64_t uInt64;
#include <NIDAQmx.h>
#endif

#define NIDAQ_ADC_GAIN 1
#define NIDAQ_ADC_GAIN_ADJUST 1.0
#define NIDAQ_ADC_OFFSET 0.0

#define DAQ_ID_WARNING_MSG      "Warning: Board id out of range.\n"
#define DAQ_SETTLE_WARNING_MSG  "Warning: Requested ADC settling value out of range.\n"
#define DAQ_CONNECT_WARNING_MSG "Warning: Board not connected.\n"
#define DAQ_AO_WARNING_MSG      "Warning: AO channel out of range.\n"
#define DAQ_AI_WARNING_MSG      "Warning: AI channel out of range.\n"
#define DAQ_VOLTAGE_WARNING_MSG "Warning: Voltage out of range.\n"

enum
{
	DAQ_AO = 0,
	DAQ_AI = 1
};

struct AnalogChannel
{
	bool req;
	int known;
	double voltage, min, max;
#if COMEDI
	lsampl_t maxdata;
	comedi_range *crange;
#elif NIDAQMX
	TaskHandle task;
#endif

};

struct SubDevice
{
	struct AnalogChannel ch[M2_DAQ_MAX_CHAN];
	int N_ch;
#if COMEDI
	int num, buffer_size;
	unsigned int range;
	bool use_lsampl;
	ssize_t b_sampl;
#endif

};

struct DaqBoard
{
	char *node;
	bool is_real, is_connected, scan_prepared;

	char *info_driver, *info_full_node, *info_board, *info_board_abrv, *info_output, *info_input, *info_settle;

	// device setup
#if COMEDI
	comedi_t *comedi_dev;
#elif NIDAQ
	i16 nidaq_num, nidaq_bcode;
#endif
	struct SubDevice ao, ai;

	// multi setup (AI only, slightly complicated to avoid ghosting on multiplexed ADCs)
	int multi_chan[M2_DAQ_MAX_CHAN];
	int multi_N_chan;
	int multi_settle;
#if COMEDI
	lsampl_t multi_raw[M2_DAQ_MAX_CHAN];
	comedi_insn multi_insn[M2_DAQ_MAX_CHAN * (M2_DAQ_MAX_SETTLE + 2)];  // setup + wait(s) + read == total instructions per channel
	comedi_insnlist multi_insnlist;
#elif NIDAQ
	f64 multi_voltage[M2_DAQ_MAX_CHAN * (M2_DAQ_MAX_SETTLE + 1)];  // pre-read(s) + final read == total samples per channel
#elif NIDAQMX
	float64 multi_voltage[M2_DAQ_MAX_CHAN];
	TaskHandle multi_task;
#endif

	// scan setup (AI only)
	int scan_N_chan, scan_pci[M2_DAQ_MAX_CHAN];
	double scan_total_time;
	ssize_t scan_total, scan_offset;  // scan_total = N_pt * N_chan
	long scan_saved;                  // scan_saved refers to complete points (all samples present)
	void *scan_buffer;
	Timer *scan_timer;
	int scan_dummy_map[M2_DAQ_MAX_CHAN][2];
#if COMEDI
	comedi_cmd scan_cmd;
	unsigned int scan_chanlist[M2_DAQ_MAX_CHAN];
#elif NIDAQ
	i16 scan_phys_chan[M2_DAQ_MAX_CHAN], scan_phys_gain[M2_DAQ_MAX_CHAN];
	i16 scan_tbcode;
	u16 scan_sample_t;
#elif NIDAQMX
	TaskHandle scan_task;
#endif

};

static struct DaqBoard daq_board[M2_DAQ_MAX_BRD];
static Timer *global_timer;

static void daq_board_close   (struct DaqBoard *board);
static void subdevice_connect (struct DaqBoard *board, struct SubDevice *subdev, int type);
#if NIDAQ
static char * bcode_to_str (int bcode);
#elif NIDAQMX
static char * get_mx_product_type (const char *node);
static bool create_mx_scan (TaskHandle *task, char *node, float64 rate, uInt64 spc, int N_chan, int *phys_chan);
static int32 mention_mx_error (int32 code);
#endif

#include "daq_point_io.c"
#include "daq_scan.c"

void daq_init (void)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_DAQ_MAX_BRD; id++)
	{
		daq_board[id].node = cat1("");
		daq_board[id].is_real = 0;
		daq_board[id].is_connected = 0;
		daq_board[id].scan_timer = timer_new();

		daq_board[id].info_driver     = cat1("∅");
		daq_board[id].info_full_node  = cat1("∅");
		daq_board[id].info_board      = cat1("∅");
		daq_board[id].info_board_abrv = cat1("∅");
		daq_board[id].info_output     = cat1("∅");
		daq_board[id].info_input      = cat1("∅");
		daq_board[id].info_settle     = cat1("∅");
#if COMEDI
		daq_board[id].multi_insnlist.insns = daq_board[id].multi_insn;
#endif
	}

	global_timer = timer_new();
}

void daq_final (void)
{
	f_start(F_INIT);

	for (int id = 0; id < M2_DAQ_MAX_BRD; id++)
	{
		daq_board_close(&daq_board[id]);
		replace(daq_board[id].node, NULL);

		replace(daq_board[id].info_driver,     NULL);
		replace(daq_board[id].info_full_node,  NULL);
		replace(daq_board[id].info_board,      NULL);
		replace(daq_board[id].info_board_abrv, NULL);
		replace(daq_board[id].info_output,     NULL);
		replace(daq_board[id].info_input,      NULL);
		replace(daq_board[id].info_settle,     NULL);
	}
}

void daq_board_close (struct DaqBoard *board)
{
	f_start(F_UPDATE);

	if (board->is_real && board->is_connected)
	{
#if COMEDI
		comedi_close(board->comedi_dev);
#elif NIDAQ
		DAQ_Clear(board->nidaq_num);
#elif NIDAQMX
		mention_mx_error(DAQmxResetDevice(board->node));
#endif
	}
}

int daq_board_connect (int id, const char *node, int settle)
{
	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return 0);
	f_verify(settle >= 0 && settle <= M2_DAQ_MAX_SETTLE, DAQ_SETTLE_WARNING_MSG, return 0);
	f_start(F_UPDATE);

	daq_board_close(&daq_board[id]);

	// update node:
	replace(daq_board[id].node, cat1(node));

	// multi setup:
	daq_board[id].multi_N_chan = 0;
	daq_board[id].multi_settle = settle;

	// scan setup:
	daq_board[id].scan_prepared = 0;
	daq_board[id].scan_buffer = NULL;

	if (str_equal(node, "dummy"))
	{
		daq_board[id].is_real = 0;
		daq_board[id].is_connected = 1;

		replace(daq_board[id].info_driver,     cat1("Dummy"));
		replace(daq_board[id].info_full_node,  cat1("N/A"));
		replace(daq_board[id].info_board,      cat1("<Virtual>"));
		replace(daq_board[id].info_board_abrv, cat1("<Virt.>"));
		replace(daq_board[id].info_settle,     cat1("N/A"));
	}
	else
	{
		daq_board[id].is_real = 1;
		daq_board[id].is_connected = 0;  // assume the worst...

#if COMEDI
		replace(daq_board[id].info_driver,    cat1("comedi"));
		replace(daq_board[id].info_full_node, cat1(node));
		replace(daq_board[id].info_settle,    supercat("%d µs", daq_board[id].multi_settle*10));

		daq_board[id].comedi_dev = comedi_open(node);
		if (daq_board[id].comedi_dev != NULL)
		{
			daq_board[id].is_connected = 1;

			int vcode = comedi_get_version_code(daq_board[id].comedi_dev);
			int v[3];  for(int i = 0; i < 3; i++) v[i] = 0xFF & (vcode >> i*8);
			replace(daq_board[id].info_board, supercat("%s (%s %d.%d.%d)", comedi_get_board_name (daq_board[id].comedi_dev),
			                                                               comedi_get_driver_name(daq_board[id].comedi_dev), v[2], v[1], v[0]));
			replace(daq_board[id].info_board_abrv, cat1(comedi_get_board_name(daq_board[id].comedi_dev)));
		}

#elif NIDAQ
		u32 version = 0;  Get_NI_DAQ_Version(&version);
		replace(daq_board[id].info_driver, supercat("Traditional NI-DAQ %d.%d%d", (version & 0x0F00) >> 8, (version & 0x00F0) >> 4, version & 0x000F));
		replace(daq_board[id].info_settle, supercat("+%d samps", daq_board[id].multi_settle));

		int num;
		if (sscanf(node, "%d", &num) == 1)
		{
			replace(daq_board[id].info_full_node, supercat("NI-DAQ:%d", num));

			if ((Init_DA_Brds((i16) num, &daq_board[id].nidaq_bcode) == 0) && (daq_board[id].nidaq_bcode != 1))
			{
				daq_board[id].is_connected = 1;
				daq_board[id].nidaq_num = (i16) num;

				replace(daq_board[id].info_board, bcode_to_str(daq_board[id].nidaq_bcode));
				replace(daq_board[id].info_board_abrv, cat1(daq_board[id].info_board));  // the same
			}
		}
		else
		{
			replace(daq_board[id].info_full_node, cat1("NI-DAQ:?"));
			status_add(0, supercat("DAQ Error: Unusable node string: %s.\n", node));
		}

#elif NIDAQMX
		uInt32 major = 0;  DAQmxGetSysNIDAQMajorVersion(&major);
		uInt32 minor = 0;  DAQmxGetSysNIDAQMinorVersion(&minor);
		replace(daq_board[id].info_driver,    supercat("NI-DAQmx %u.%u", major, minor));
		replace(daq_board[id].info_full_node, cat2("NI-DAQmx:", node));
		replace(daq_board[id].info_settle,    supercat("+%d µs", daq_board[id].multi_settle*10));

		if (DAQmxResetDevice(node) == 0)
		{
			daq_board[id].is_connected = 1;
			DAQmxCreateTask("", &daq_board[id].multi_task);
			DAQmxCreateTask("", &daq_board[id].scan_task);

			replace(daq_board[id].info_board, get_mx_product_type(node));
			replace(daq_board[id].info_board_abrv, cat1(daq_board[id].info_board));  // the same
		}

#else
		replace(daq_board[id].info_driver,    cat1("∅"));
		replace(daq_board[id].info_full_node, cat1("∅"));
		replace(daq_board[id].info_settle,    cat1("∅"));
#endif

		if (!daq_board[id].is_connected)
		{
			replace(daq_board[id].info_board, supercat("Failed to open \"%s\".", node));
			replace(daq_board[id].info_board_abrv, cat1("∅"));
		}
	}

	subdevice_connect(&daq_board[id], &daq_board[id].ao, DAQ_AO);
	subdevice_connect(&daq_board[id], &daq_board[id].ai, DAQ_AI);

	if (daq_board[id].is_real)
	{
		replace(daq_board[id].info_output, (daq_board[id].ao.N_ch > 0) ? supercat("%d ch [%0.1f, %0.1f]", daq_board[id].ao.N_ch, daq_board[id].ao.ch[0].min, daq_board[id].ao.ch[0].max) : cat1("0 ch"));
		replace(daq_board[id].info_input,  (daq_board[id].ai.N_ch > 0) ? supercat("%d ch [%0.1f, %0.1f]", daq_board[id].ai.N_ch, daq_board[id].ai.ch[0].min, daq_board[id].ai.ch[0].max) : cat1("0 ch"));
	}
	else
	{
		replace(daq_board[id].info_output, supercat("%d ch [%s, %s]", daq_board[id].ao.N_ch, quote(M2_DAQ_VIRTUAL_MIN), quote(M2_DAQ_VIRTUAL_MAX)));
		replace(daq_board[id].info_input,  supercat("%d ch [%s, %s]", daq_board[id].ai.N_ch, quote(M2_DAQ_VIRTUAL_MIN), quote(M2_DAQ_VIRTUAL_MAX)));
	}

	return daq_board[id].is_connected;
}

void subdevice_connect (struct DaqBoard *board, struct SubDevice *subdev, int type)
{
	f_start(F_UPDATE);

	for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++)
	{
		subdev->ch[chan].req = 0;
		subdev->ch[chan].voltage = 0.0;
		subdev->ch[chan].known = (type == DAQ_AO) ? 2 : 0;
	}

	if (board->is_connected)
	{
		if (board->is_real)
		{
#if COMEDI
			subdev->num = comedi_find_subdevice_by_type(board->comedi_dev, (type == DAQ_AO) ? COMEDI_SUBD_AO : COMEDI_SUBD_AI, 0);
			if (subdev->num != -1)
			{
				subdev->N_ch = min_int(comedi_get_n_channels(board->comedi_dev, (unsigned int) subdev->num), M2_DAQ_MAX_CHAN);

				subdev->range = 0;  // 0 corresponds to the "first" range available
				for (int chan = 0; chan < subdev->N_ch; chan++)
				{
					subdev->ch[chan].maxdata = comedi_get_maxdata (board->comedi_dev, (unsigned int) subdev->num, (unsigned int) chan);
					subdev->ch[chan].crange  = comedi_get_range   (board->comedi_dev, (unsigned int) subdev->num, (unsigned int) chan, subdev->range);

					subdev->ch[chan].min = subdev->ch[chan].crange->min;
					subdev->ch[chan].max = subdev->ch[chan].crange->max;
				}

				int page_size = (int) sysconf(_SC_PAGE_SIZE);
				int size_request = (M2_DAQ_COMEDI_BUFFER_SIZE / page_size) * page_size;
				if (size_request != M2_DAQ_COMEDI_BUFFER_SIZE)
					f_print(F_ERROR, "default_size: %d, page_size: %d, size_request: %d\n", M2_DAQ_COMEDI_BUFFER_SIZE, page_size, size_request);

				int max_buffer_size = comedi_get_max_buffer_size (board->comedi_dev, (unsigned int) subdev->num);
				/**/                  comedi_set_buffer_size     (board->comedi_dev, (unsigned int) subdev->num, (unsigned int) min_int(size_request, max_buffer_size));
				subdev->buffer_size = comedi_get_buffer_size     (board->comedi_dev, (unsigned int) subdev->num);

				int flags = comedi_get_subdevice_flags(board->comedi_dev, (unsigned int) subdev->num);
				subdev->use_lsampl = flags & SDF_LSAMPL;
				subdev->b_sampl = subdev->use_lsampl ? sizeof(lsampl_t) : sizeof(sampl_t);

				f_print(F_VERBOSE, "Subdevice %d: %s\n", subdev->num, type == DAQ_AO ? "AO" : "AI");
				f_print(F_VERBOSE, "\tbuffer_size: %d max_buffer_size: %d\n", subdev->buffer_size, max_buffer_size);
				f_print(F_VERBOSE, "\tnum channels: %d\n", subdev->N_ch);
				if (subdev->N_ch > 0)
				{
					f_print(F_VERBOSE, "\tch0 maxdata: %d (%s channel specific)\n",                         subdev->ch[0].maxdata,                                comedi_maxdata_is_chan_specific (board->comedi_dev, (unsigned int) subdev->num) == 0 ? "not" : "");
					f_print(F_VERBOSE, "\tch0 range: [%0.3f, %0.3f] (%s channel specific)\n", subdev->ch[0].crange->min, subdev->ch[0].crange->max, comedi_range_is_chan_specific   (board->comedi_dev, (unsigned int) subdev->num) == 0 ? "not" : "");
				}
			}
			else status_add(0, cat1("Warning: Cannot find a valid subdevice.\n"));
#elif NIDAQ
			subdev->N_ch = (type == DAQ_AO) ? 2 : 16;
			for (int chan = 0; chan < subdev->N_ch; chan++)
			{
				subdev->ch[chan].min = -10.0;
				subdev->ch[chan].max =  10.0;
			}
#elif NIDAQMX

			for (subdev->N_ch = 0; subdev->N_ch < M2_DAQ_MAX_CHAN; subdev->N_ch++)
			{
				TaskHandle task;
				DAQmxCreateTask("", &task);
				int32 rv = (type == DAQ_AO) ? DAQmxCreateAOVoltageChan(task, atg(supercat("%s/ao%d", board->node, subdev->N_ch)), "",     -1.0, 1.0, DAQmx_Val_Volts, NULL) :
				                              DAQmxCreateAIVoltageChan(task, atg(supercat("%s/ai%d", board->node, subdev->N_ch)), "", -1, -1.0, 1.0, DAQmx_Val_Volts, NULL);
				DAQmxClearTask(task);
				if (rv != 0) break;
			}

			for (int chan = 0; chan < subdev->N_ch; chan++)
			{
				DAQmxCreateTask("", &subdev->ch[chan].task);

				float64 voltage;
				if (type == DAQ_AO)
				{
					DAQmxCreateAOVoltageChan(subdev->ch[chan].task, atg(supercat("%s/ao%d", board->node, chan)), "xyz", -10.0, 10.0, DAQmx_Val_Volts, NULL);  // example: "Dev1/ao0"
					subdev->ch[chan].min = (DAQmxGetAOMin(subdev->ch[chan].task, "xyz", &voltage) == 0) ? voltage : -9.9;
					subdev->ch[chan].max = (DAQmxGetAOMax(subdev->ch[chan].task, "xyz", &voltage) == 0) ? voltage :  9.9;
				}
				else
				{
					DAQmxCreateAIVoltageChan(subdev->ch[chan].task, atg(supercat("%s/ai%d", board->node, chan)), "xyz", -1, -10.0, 10.0, DAQmx_Val_Volts, NULL);  // example: "Dev1/ai0"
					subdev->ch[chan].min = (DAQmxGetAIMin(subdev->ch[chan].task, "xyz", &voltage) == 0) ? voltage : -9.9;
					subdev->ch[chan].max = (DAQmxGetAIMax(subdev->ch[chan].task, "xyz", &voltage) == 0) ? voltage :  9.9;
				}

			}
#endif
		}
		else
		{
			subdev->N_ch = (type == DAQ_AO) ? M2_DAQ_VIRTUAL_NUM_DAC : M2_DAQ_VIRTUAL_NUM_ADC;
			for (int chan = 0; chan < subdev->N_ch; chan++)
			{
				subdev->ch[chan].min = M2_DAQ_VIRTUAL_MIN;
				subdev->ch[chan].max = M2_DAQ_VIRTUAL_MAX;
			}
		}
	}
	else subdev->N_ch = 0;
}

int daq_AO_valid (int id, int chan)
{
	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            NULL, return 0);
	f_verify(daq_board[id].is_connected,                NULL, return 0);
	f_verify(chan >= 0 && chan < daq_board[id].ao.N_ch, NULL, return 0);

	return 1;
}

int daq_AI_valid (int id, int chan)
{
	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            NULL, return 0);
	f_verify(daq_board[id].is_connected,                NULL, return 0);
	f_verify(chan >= 0 && chan < daq_board[id].ai.N_ch, NULL, return 0);

	return 1;
}

char * daq_board_info (int id, const char *info)
{
	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return NULL);

	if      (str_equal(info, "driver"))     return daq_board[id].info_driver;
	else if (str_equal(info, "full_node"))  return daq_board[id].info_full_node;
	else if (str_equal(info, "board"))      return daq_board[id].info_board;
	else if (str_equal(info, "board_abrv")) return daq_board[id].info_board_abrv;
	else if (str_equal(info, "output"))     return daq_board[id].info_output;
	else if (str_equal(info, "input"))      return daq_board[id].info_input;
	else if (str_equal(info, "settle"))     return daq_board[id].info_settle;
	else                                    return cat1("∅");
}

#if NIDAQ
char * bcode_to_str (int bcode)
{
	switch (bcode)
	{
		case 7   : return cat1("PC-DIO-24");
		case 8   : return cat1("AT-DIO-32F");
		case 12  : return cat1("PC-DIO-96");
		case 13  : return cat1("PC-LPM-16");
		case 15  : return cat1("AT-AO-6");
		case 25  : return cat1("AT-MIO-16E-2");
		case 26  : return cat1("AT-AO-10");
		case 32  : return cat1("NEC-MIO-16E-4");
		case 35  : return cat1("DAQCard DIO-24");
		case 36  : return cat1("AT-MIO-16E-10");
		case 37  : return cat1("AT-MIO-16DE-10");
		case 38  : return cat1("AT-MIO-64E-3");
		case 39  : return cat1("AT-MIO-16XE-50");
		case 40  : return cat1("NEC-AI-16E-4");
		case 41  : return cat1("NEC-MIO-16XE-50");
		case 42  : return cat1("NEC-AI-16XE-50");
		case 44  : return cat1("AT-MIO-16E-1");
		case 50  : return cat1("AT-MIO-16XE-10");
		case 51  : return cat1("AT-AI-16XE-10");
		case 52  : return cat1("DAQCard-AI-16XE-50");
		case 53  : return cat1("DAQCard-AI-16E-4");
		case 65  : return cat1("PC-DIO-24/PnP"); 
		case 66  : return cat1("PC-DIO-96/PnP");
		case 67  : return cat1("AT-DIO-32HS");
		case 68  : return cat1("DAQCard-6533");
		case 75  : return cat1("DAQPad-6507/8"); 
		case 76  : return cat1("DAQPad-6020E (USB)");
		case 88  : return cat1("DAQCard-6062E");
		case 89  : return cat1("DAQCard-6715");
		case 90  : return cat1("DAQCard-6023E");
		case 91  : return cat1("DAQCard-6024E");
		case 200 : return cat1("PCI-DIO-96");
		case 202 : return cat1("PCI-MIO-16XE-50");
		case 203 : return cat1("PCI-5102");
		case 204 : return cat1("PCI-MIO-16XE-10");
		case 205 : return cat1("PCI-MIO-16E-1");
		case 206 : return cat1("PCI-MIO-16E-4");
		case 207 : return cat1("PXI-6070E");
		case 208 : return cat1("PXI-6040E");
		case 209 : return cat1("PXI-6030E");
		case 210 : return cat1("PXI-6011E");
		case 211 : return cat1("PCI-DIO-32HS");
		case 215 : return cat1("PXI-6533");
		case 216 : return cat1("PCI-6534");
		case 218 : return cat1("PXI-6534");
		case 220 : return cat1("PCI-6031E (MIO-64XE-10)");
		case 221 : return cat1("PCI-6032E (AI-16XE-10)");
		case 222 : return cat1("PCI-6033E (AI-64XE-10)");
		case 223 : return cat1("PCI-6071E (MIO-64E-1)");
		case 232 : return cat1("PCI-6602");
		case 233 : return cat1("NI 4451 (PCI)");
		case 234 : return cat1("NI 4452 (PCI)");
		case 235 : return cat1("NI 4551 (PCI)");
		case 236 : return cat1("NI 4552 (PCI)");
		case 237 : return cat1("PXI-6602");
		case 240 : return cat1("PXI-6508");
		case 241 : return cat1("PCI-6110");
		case 244 : return cat1("PCI-6111");
		case 256 : return cat1("PCI-6503");
		case 257 : return cat1("PXI-6503");
		case 258 : return cat1("PXI-6071E");
		case 259 : return cat1("PXI-6031E");
		case 261 : return cat1("PCI-6711");
		case 262 : return cat1("PCI-6711");
		case 263 : return cat1("PCI-6713");
		case 264 : return cat1("PXI-6713");
		case 265 : return cat1("PCI-6704");
		case 266 : return cat1("PXI-6704");
		case 267 : return cat1("PCI-6023E");
		case 268 : return cat1("PXI-6023E");
		case 269 : return cat1("PCI-6024E");
		case 270 : return cat1("PXI-6024E");
		case 271 : return cat1("PCI-6025E");
		case 272 : return cat1("PXI-6025E");
		case 273 : return cat1("PCI-6052E");
		case 274 : return cat1("PXI-6052E");
		case 275 : return cat1("DAQPad-6070E (1394)");
		case 276 : return cat1("DAQPad-6052E (1394)");
		case 285 : return cat1("PCI-6527");
		case 286 : return cat1("PXI-6527");
		case 308 : return cat1("PCI-6601");
		case 311 : return cat1("PCI-6703");
		case 314 : return cat1("PCI-6034E");
		case 315 : return cat1("PXI-6034E");
		case 316 : return cat1("PCI-6035E");
		case 317 : return cat1("PXI-6035E");
		case 318 : return cat1("PXI-6703");
		case 319 : return cat1("PXI-6608");
		case 321 : return cat1("NI 4454 (PCI)");
		case 327 : return cat1("PCI-6608");
		case 329 : return cat1("NI 6222 (PCI)");
		case 330 : return cat1("NI 6222 (PXI)");
		case 331 : return cat1("NI 6224 (Ethernet)");
		case 332 : return cat1("DAQPad-6052E (USB)");
		case 335 : return cat1("NI 4472 (PXI/CompactPCI)");
		case 338 : return cat1("PCI-6115");
		case 339 : return cat1("PXI-6115");
		case 340 : return cat1("PCI-6120");
		case 341 : return cat1("PXI-6120");
		case 342 : return cat1("NI 4472 (PCI)");
		case 348 : return cat1("NI 6036E (PCI)");
		case 349 : return cat1("NI 6731 (PCI)");
		case 350 : return cat1("NI 6733 (PCI)");
		case 351 : return cat1("NI 6731 (PXI/Compact PCI)");
		case 352 : return cat1("NI 6733 (PXI/Compact PCI)");
		case 353 : return cat1("PCI-4474");
		case 354 : return cat1("PXI-4474");
		case 361 : return cat1("DAQPad-6052E (1394)");
		case 366 : return cat1("PCI-6013");
		case 367 : return cat1("PCI-6014");
		default  : return cat1("Unknown");
	}
}
#elif NIDAQMX
char * get_mx_product_type (const char *node)
{
	char *str = new_str(100);
	DAQmxGetDevProductType(node, str, 100);
	return str;
}

bool create_mx_scan (TaskHandle *task, char *node, float64 rate, uInt64 spc, int N_chan, int *phys_chan)
{
	char *str _strfree_ = new_str((str_length(node) + 8) * N_chan);  // Device name plus 8 for '/aiXX' multiplied by the number of channels.
	int spot = 0;
	for (int pci = 0; pci < N_chan; pci++)
		spot += sprintf(&str[spot], pci == 0 ? "%s/ai%d" : ",%s/ai%d", node, phys_chan[pci]);

	int32 rv0 = mention_mx_error(DAQmxCreateTask("", task));
	int32 rv1 = mention_mx_error(DAQmxCreateAIVoltageChan(*task, str, "", -1, -10.0, 10.0, DAQmx_Val_Volts, NULL));

	if (rate < 0) return (rv0 == 0 && rv1 == 0);
	else
	{
		int32 rv2 = mention_mx_error(DAQmxCfgSampClkTiming(*task, "OnboardClock", rate, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, spc));
		int32 rv3 = mention_mx_error(DAQmxSetReadReadAllAvailSamp(*task, 1));

		return (rv0 == 0 && rv1 == 0 && rv2 == 0 && rv3 == 0);
	}
}

int32 mention_mx_error (int32 code)
{
	if (code != 0)
	{
		int bytes = DAQmxGetErrorString(code, NULL, 0);
		char *str _strfree_ = new_str(bytes - 1);  // new_str() allows for null termination automatically
		DAQmxGetErrorString(code, str, (uInt32) bytes);
		status_add(1, cat3("DAQmx error: ", str, "\n"));
	}

	return code;
}
#endif
