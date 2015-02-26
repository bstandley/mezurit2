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
	daq_board[id].multi_dio    = 0;

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
	// dummy:                    set voltages to sine waves,
	//                           toggle DIO at n-Hz,         return ?
	// ------------------------------------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,                            DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].multi_N_chan > 0 || daq_board[id].multi_dio, NULL,               return 1);
	f_verify(daq_board[id].is_connected,                                NULL,               return 0);

	struct DaqBoard *board = &daq_board[id];
	if (board->is_real)
	{
		if (board->multi_N_chan > 0)
		{
#if COMEDI
			if (comedi_do_insnlist(board->comedi_dev, &board->multi_insnlist) != board->multi_N_chan) return 0;
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
#elif NIDAQ || NIDAQMX
				board->ai.ch[chan].voltage = board->multi_voltage[pci];
#else
				board->ai.ch[chan].known = 2;
#endif
			}
		}

		if (board->multi_dio)
		{
#if COMEDI
			for (int chan = 0; chan < board->dio.N_ch; chan += 16)  // also tested with 4 bit chunks
				if (comedi_dio_bitfield2(board->comedi_dev, (unsigned int) board->dio.num, 0, &board->multi_bits[chan/16], (unsigned int) chan) != 0) return 0;
#elif NIDAQ
			;  // TODO: DIO
#elif NIDAQMX
			;  // TODO: DIO
#endif

			for (int chan = 0; chan < board->dio.N_ch; chan++)
			{
				board->dio.ch[chan].known = 1;
#if COMEDI
				board->dio.ch[chan].voltage = (board->multi_bits[chan/16] & (((unsigned int) 0x1) << (chan % 16))) ? 1.0 : 0.0;
#elif NIDAQ
				board->dio.ch[chan].known = 2;  // TODO: DIO
#elif NIDAQMX
				board->dio.ch[chan].known = 2;  // TODO: DIO
#else
				board->dio.ch[chan].known = 2;
#endif
			}
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

		for (int chan = 0; chan < board->dio.N_ch; chan++)
			if (!board->dio.ch[chan].writeable)
			{
				board->dio.ch[chan].known = 1;
				board->dio.ch[chan].voltage = (fmod(chan * t, 1.0) < 0.5) ? 1.0 : 0.0;
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
			for (int pci = 0; pci < board->multi_N_chan; pci++)
			{
				board->multi_insn[pci].insn     = INSN_READ;
				board->multi_insn[pci].n        = 1;
				board->multi_insn[pci].data     = &board->multi_raw[pci];
				board->multi_insn[pci].subdev   = (unsigned int) board->ai.num;
				board->multi_insn[pci].chanspec = CR_PACK((unsigned int) board->multi_chan[pci], board->ai.range, AREF_GROUND);
			}
			board->multi_insnlist.n_insns = (unsigned int) board->multi_N_chan;
			board->multi_insnlist.insns = board->multi_insn;
#elif NIDAQ
			i16 local_phys_chan[M2_DAQ_MAX_CHAN], local_phys_gain[M2_DAQ_MAX_CHAN];
			for (int pci = 0; pci < board->multi_N_chan; pci++)
			{
				local_phys_chan[pci] = (i16) board->multi_chan[pci];
				local_phys_gain[pci] = NIDAQ_ADC_GAIN;
			}
			SCAN_Setup(board->nidaq_num, (i16) board->multi_N_chan, local_phys_chan, local_phys_gain);
#elif NIDAQMX

			mention_mx_error(DAQmxClearTask(board->multi_task));
			create_mx_scan(&board->multi_task, board->node, -1, 1, board->multi_N_chan, board->multi_chan);
			mention_mx_error(DAQmxStartTask(board->multi_task));  // starts task so it will be ready for OnDemand reads in the future
#endif
		}

		daq_multi_tick(id);  // get complete set of values for immediate use
	}

	*voltage = daq_board[id].ai.ch[chan].voltage;
	return daq_board[id].ai.ch[chan].known;
}

int daq_DIO_read (int id, int chan, double *voltage)
{
	// ------------------------------------------------------
	// bad id:         complain,           return 0 (failure)
	// not connected:  do nothing,         return 2 (unknown)
	// bad chan:       complain,           return 0
	// no driver:      add to MS, tick MS, return ?
	// dummy:          add to MS, tick MS, return ?
	// ------------------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,             DAQ_ID_WARNING_MSG,  return 0);
	f_verify(daq_board[id].is_connected,                 NULL,                return 2);
	f_verify(chan >= 0 && chan < daq_board[id].dio.N_ch, DAQ_DIO_WARNING_MSG, return 0);

	if (!daq_board[id].dio.ch[chan].req)
	{
		// add this chan to multi config (only really needs to happen once, as all DIO channels are read if any one channel is requested)
		daq_board[id].dio.ch[chan].req = 1;

		// in this case multi config is just turning it on . . .
		daq_board[id].multi_dio = 1;

		daq_multi_tick(id);  // get complete set of values for immediate use
	}

	*voltage = daq_board[id].dio.ch[chan].voltage;
	return daq_board[id].dio.ch[chan].known;
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

int daq_DIO_write (int id, int chan, double voltage)
{
	// ----------------------------------------------
	// bad id:         complain,   return 0 (failure)
	// not connected:  do nothing, return 0
	// bad chan:       complain,   return 0
	// bad voltage:    complain,   return 0
	// no driver:      do nothing, return 0
	// ----------------------------------------------

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,             DAQ_ID_WARNING_MSG,      return 0);
	f_verify(daq_board[id].is_connected,                 NULL,                    return 0);
	f_verify(chan >= 0 && chan < daq_board[id].dio.N_ch, DAQ_DIO_WARNING_MSG,     return 0);
	f_verify(voltage >= daq_board[id].dio.ch[chan].min &&
	         voltage <= daq_board[id].dio.ch[chan].max,  DAQ_VOLTAGE_WARNING_MSG, return 0);

	bool rv = 1;
	if (daq_board[id].is_real)
	{
		if (!daq_board[id].dio.ch[chan].writeable)
		{
#if COMEDI
			rv = (comedi_dio_config(daq_board[id].comedi_dev, (unsigned int) daq_board[id].dio.num, (unsigned int) chan, COMEDI_OUTPUT) == 0);
#elif NIDAQ
			rv = 0;  // TODO: DIO
#elif NIDAQMX
			rv = 0;  // TODO: DIO
#else
			rv = 0;
#endif
			if (rv) daq_board[id].dio.ch[chan].writeable = 1;
		}

#if COMEDI
		if (rv) rv = (comedi_dio_write(daq_board[id].comedi_dev, (unsigned int) daq_board[id].dio.num, (unsigned int) chan, voltage >= 0.5) == 1);
#elif NIDAQ
		if (rv) rv = 0;  // TODO: DIO
#elif NIDAQMX
		if (rv) rv = 0;  // TODO: DIO
#else
		rv = 0;  // without a driver the channel can never be set writeable, so rv should already be 0 at this point . . .
#endif
	}
	else daq_board[id].dio.ch[chan].writeable = 1;  // remind dummy boards to not generate a waveform for this particular channel

	if (rv)
	{
		daq_board[id].dio.ch[chan].known = 1;
		daq_board[id].dio.ch[chan].voltage = (voltage >= 0.5) ? 1.0 : 0.0;
	}

	return rv ? 1 : 0;
}
