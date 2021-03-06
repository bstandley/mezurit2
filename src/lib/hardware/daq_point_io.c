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

#if COMEDI
lsampl_t dev_null;
lsampl_t ten_us_in_ns = 10000;
#endif

void daq_multi_reset (int id)
{
	// -----------------------------
	// bad id:         complain
	// not connected:  do nothing
	// no driver:      work normally
	// dummy:          work normally
	// -----------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return);
	f_verify(daq_board[id].is_connected,     NULL,               return);

	for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++) daq_board[id].ai.ch[chan].req = 0;
	daq_board[id].multi_N_chan = 0;

#if NIDAQMX	
	mention_mx_error(DAQmxClearTask(daq_board[id].multi_task));  // also stops task, if necessary
#endif
}

int daq_multi_tick (int id)
{
	// ------------------------------------------------------------------------
	// bad id:                   complain,                   return 0 (failure)
	// no channels:              do nothing,                 return 1 (success)
	// not connected (w/ chan):  do nothing,                 return 0 (failure)
	// no driver:                do nothing,                 return 1 (success)
	// dummy:                    set voltages to sine waves, return ?
	// ------------------------------------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].multi_N_chan > 0, NULL,               return 1);
	f_verify(daq_board[id].is_connected,     NULL,               return 0);

	struct DaqBoard *board = &daq_board[id];
	if (board->is_real)
	{
#if COMEDI
		int rv = comedi_do_insnlist(board->comedi_dev, &board->multi_insnlist);
		if (rv != (int) board->multi_insnlist.n_insns)
		{
			f_print(F_ERROR, "comedi_do_insnlist() returned %d\n", rv);
			return 0;
		}
#elif NIDAQ
		if (AI_VRead_Scan(board->nidaq_num, board->multi_voltage) != 0) return 0;
#elif NIDAQMX
		if (mention_mx_error(DAQmxReadAnalogF64(board->multi_task, 1, -1, DAQmx_Val_GroupByScanNumber, board->multi_voltage, (uInt32) board->multi_N_chan, NULL, NULL)) != 0) return 0;
#endif

		for (int pci = 0; pci < board->multi_N_chan; pci++)
		{
			int chan = board->multi_chan[pci];
			board->ai.ch[chan].known = 1;
#if COMEDI
			board->ai.ch[chan].voltage = comedi_to_phys(board->multi_raw[pci], board->ai.ch[chan].crange, board->ai.ch[chan].maxdata);
#elif NIDAQ
			board->ai.ch[chan].voltage = board->multi_voltage[pci * (board->multi_settle + 1) + board->multi_settle];  // board->multi_settle + 1 = increment, use last sample of the "bunch"
#elif NIDAQMX
			board->ai.ch[chan].voltage = board->multi_voltage[pci];
#else
			board->ai.ch[chan].known = 2;
#endif
		}
	}
	else
	{
		double t = timer_elapsed(global_timer);

		for (int pci = 0; pci < board->multi_N_chan; pci++)
		{
			int chan = board->multi_chan[pci];
			board->ai.ch[chan].known = 1;
			board->ai.ch[chan].voltage = cos(2*U_PI * chan * t);
		}
	}

	return 1;
}

int daq_AI_read (int id, int chan, double *voltage)
{
	// ------------------------------------------------------
	// bad id:         complain,           return 0 (failure)
	// not connected:  do nothing,         return 2 (unknown)
	// bad chan:       complain,           return 0
	// no driver:      add to MS, tick MS, return ?
	// dummy:          add to MS, tick MS, return ?
	// ------------------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].is_connected,                NULL,               return 2);
	f_verify(chan >= 0 && chan < daq_board[id].ai.N_ch, DAQ_AI_WARNING_MSG, return 0);

	if (!daq_board[id].ai.ch[chan].req)
	{
		struct DaqBoard *board = &daq_board[id];

		// add this chan to multi config
		board->ai.ch[chan].req = 1;

		// regen multi config
		board->multi_N_chan = 0;
		for (int c = 0; c < board->ai.N_ch; c++)
			if (board->ai.ch[c].req)
				board->multi_chan[board->multi_N_chan++] = c;

		if (board->is_real && board->multi_N_chan > 0)
		{
#if COMEDI
			int inc = board->multi_settle + 2;
			for (int pci = 0; pci < board->multi_N_chan; pci++)
			{
				board->multi_insn[pci*inc].insn     = INSN_READ;
				board->multi_insn[pci*inc].n        = 0;
				board->multi_insn[pci*inc].subdev   = (unsigned int) board->ai.num;
				board->multi_insn[pci*inc].chanspec = CR_PACK((unsigned int) board->multi_chan[pci], board->ai.range, AREF_GROUND);
				board->multi_insn[pci*inc].data     = &dev_null;

				for (int s = 1; s <= board->multi_settle; s++)
				{
					board->multi_insn[pci*inc + s].insn = INSN_WAIT;
					board->multi_insn[pci*inc + s].n    = 1;
					board->multi_insn[pci*inc + s].data = &ten_us_in_ns;
				}

				board->multi_insn[pci*inc + board->multi_settle + 1].insn     = INSN_READ;  // second time around should be more accurate now that the (multiplexed) ADC has settled
				board->multi_insn[pci*inc + board->multi_settle + 1].n        = 1;
				board->multi_insn[pci*inc + board->multi_settle + 1].subdev   = (unsigned int) board->ai.num;
				board->multi_insn[pci*inc + board->multi_settle + 1].chanspec = CR_PACK((unsigned int) board->multi_chan[pci], board->ai.range, AREF_GROUND);
				board->multi_insn[pci*inc + board->multi_settle + 1].data     = &board->multi_raw[pci];
			}
			board->multi_insnlist.n_insns = (unsigned int) (board->multi_N_chan * inc);  // board->multi_insnlist.insns already set to board->multi_insn
#elif NIDAQ
			int inc = board->multi_settle + 1;
			i16 local_phys_chan[M2_DAQ_MAX_CHAN * inc], local_phys_gain[M2_DAQ_MAX_CHAN * inc];
			for (int pci = 0; pci < board->multi_N_chan; pci++)
			{
				for (int s = 0; s <= board->multi_settle; s++)
				{
					local_phys_chan[pci*inc + s] = (i16) board->multi_chan[pci];
					local_phys_gain[pci*inc + s] = NIDAQ_ADC_GAIN;
				}
			}
			SCAN_Setup(board->nidaq_num, (i16) (board->multi_N_chan * inc), local_phys_chan, local_phys_gain);
#elif NIDAQMX

			mention_mx_error(DAQmxClearTask(board->multi_task));
			create_mx_scan(&board->multi_task, board->node, -1, 1, board->multi_N_chan, board->multi_chan);

			float64 conv_rate = -1, conv_max_rate = -1;
			mention_mx_error(DAQmxGetAIConvRate(board->multi_task, &conv_rate));
			mention_mx_error(DAQmxGetAIConvMaxRate(board->multi_task, &conv_max_rate));
			if (conv_rate > 0 && conv_max_rate > 0)
			{
				double tau = 1.0 / conv_max_rate + board->multi_settle * 10e-6;
				if (mention_mx_error(DAQmxSetAIConvRate(board->multi_task, 1.0 / tau)) == 0)
					status_add(1, supercat("ADC conversion time changed from %1.1f µs to %1.1f µs.\n", 1e6 / conv_rate, 1e6 * tau));
			}

			mention_mx_error(DAQmxStartTask(board->multi_task));  // starts task so it will be ready for OnDemand reads in the future
#endif
		}

		daq_multi_tick(id);  // get complete set of values for immediate use
	}

	*voltage = daq_board[id].ai.ch[chan].voltage;
	return daq_board[id].ai.ch[chan].known;
}

int daq_AO_read (int id, int chan, double *voltage)
{
	// ----------------------------------------------
	// bad id:         complain,   return 0 (failure)
	// not connected:  do nothing, return 2 (unknown)
	// bad chan:       complain,   return 0
	// ----------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].is_connected,                NULL,               return 2);
	f_verify(chan >= 0 && chan < daq_board[id].ao.N_ch, DAQ_AO_WARNING_MSG, return 0);

	*voltage = daq_board[id].ao.ch[chan].voltage;
	return daq_board[id].ao.ch[chan].known;
}

int daq_AO_write (int id, int chan, double voltage)
{
	// ----------------------------------------------
	// bad id:         complain,   return 0 (failure)
	// not connected:  do nothing, return 0
	// bad chan:       complain,   return 0
	// bad voltage:    complain,   return 0
	// no driver:      do nothing, return 0
	// ----------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            DAQ_ID_WARNING_MSG,      return 0);
	f_verify(daq_board[id].is_connected,                NULL,                    return 0);
	f_verify(chan >= 0 && chan < daq_board[id].ao.N_ch, DAQ_AO_WARNING_MSG,      return 0);
	f_verify(voltage >= daq_board[id].ao.ch[chan].min &&
	         voltage <= daq_board[id].ao.ch[chan].max,  DAQ_VOLTAGE_WARNING_MSG, return 0);

	bool rv = 1;
	if (daq_board[id].is_real)
	{
#if COMEDI
		lsampl_t raw = comedi_from_phys(voltage, daq_board[id].ao.ch[chan].crange, daq_board[id].ao.ch[chan].maxdata);
		rv = (comedi_data_write(daq_board[id].comedi_dev, (unsigned int) daq_board[id].ao.num, (unsigned int) chan, daq_board[id].ao.range, AREF_GROUND, raw) == 1);
#elif NIDAQ
		rv = (AO_VWrite(daq_board[id].nidaq_num, (i16) chan, voltage) == 0);
#elif NIDAQMX
		rv = (mention_mx_error(DAQmxWriteAnalogScalarF64(daq_board[id].ao.ch[chan].task, 1, 0, voltage, NULL)) == 0);
#else
		rv = 0;
#endif
	}

	if (rv)
	{
		daq_board[id].ao.ch[chan].known = 1;
		daq_board[id].ao.ch[chan].voltage = voltage;
	}

	return rv ? 1 : 0;
}
