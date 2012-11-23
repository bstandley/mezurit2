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

char * build_type (ComputeFunc *cf, bool use_all)
{
	f_start(F_NONE);

	char *type = cat1("");;

	for (int id = 0; id < M2_DAQ_MAX_BRD; id++) for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++)
	{
		bool is_dac = (cf->parse_dac[id][chan] > 0);
		bool is_adc = (cf->parse_adc[id][chan] > 0);

		if (is_dac || is_adc)
		{
			char *code = atg((id == 0) ? cat1("") : (id == M2_VDAQ_ID) ? cat1("V-") : supercat("%d-", id));
			char *str  = atg(supercat("%s%s%d%s", is_adc ? "←" : "", code, chan, is_dac ? "→" : ""));

			if (str_length(type) == 0) { replace(type, cat1(str));                       }
			else if (use_all)          { replace(type, cat3(type, ", ", str));           }
			else                       { replace(type, cat2(type, ",...")); return type; }
		}
	}

	for (int id = 0; id < M2_GPIB_MAX_BRD; id++) for (int pad = 0; pad < M2_GPIB_MAX_PAD; pad++)
		if (cf->parse_pad[id][pad] > 0)
		{
			char *code = atg((id == 0) ? cat1("G") : (id == M2_VGPIB_ID) ? cat1("VG") : supercat("G%d-", id));
			char *str  = atg(supercat("%s%d", code, pad));

			if (str_length(type) == 0) { replace(type, cat1(str));                       }
			else if (use_all)          { replace(type, cat3(type, ", ", str));           }
			else                       { replace(type, cat2(type, ",...")); return type; }
		}

	return type;
}

void build_chanset (ChanSet *chanset, Channel *channel_array)
{
	f_start(F_UPDATE);

	chanset->N_gpib = 0;
	array_set(chanset->scope_ai_count, M2_DAQ_MAX_BRD, M2_DAQ_MAX_CHAN, 0);

	int vci = 0, ici = 0;
	for (int vc = 0; vc < M2_MAX_CHAN; vc++)
	{
		Channel *channel = &channel_array[vc];
		ComputeFunc *cf = &channel->cf;

		compute_read_expr(cf, channel->expr, lookup_SI_factor(channel->prefix));

		chanset->vci_by_vc[vc] = -1;  // defaults
		chanset->ici_by_vc[vc] = -1;

		replace(channel->full_unit, cat1(""));
		replace(channel->type,      cat1(""));
		replace(channel->desc,      cat1(""));
		replace(channel->desc_long, cat1(""));

		if (cf->parse_exec) status_add(0, supercat("X%d: Definition should not contain trigger instructions. This channel will be ignored.\n", vc));
		else if (cf->py_f != NULL)
		{
			if (cf->invertible == COMPUTE_INVERTIBLE_DAC || cf->invertible == COMPUTE_INVERTIBLE_GPIB)
			{
				chanset->vci_by_ici[ici] = vci;
				chanset->ici_by_vc[vc] = ici;
				ici++;
			}

			if (cf->scannable)
			{
				for (int id = 0; id < M2_DAQ_MAX_BRD; id++) for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++)
					chanset->scope_ai_count[id][chan] += cf->parse_adc[id][chan];
			}

			for (int id = 0; id < M2_GPIB_MAX_BRD; id++) for (int pad = 0; pad < M2_GPIB_MAX_PAD; pad++)
				chanset->N_gpib += cf->parse_pad[id][pad];

			chanset->vc_by_vci[vci] = vc;
			chanset->vci_by_vc[vc] = vci;
			chanset->channel_by_vci[vci] = channel;
			vci++;

			char *type      = atg(build_type(cf, 0));
			char *desc_type = atg(supercat(" (%s)", atg(build_type(cf, 1))));

			replace(channel->full_unit, cat2(atg(lookup_SI_symbol(channel->prefix)), channel->unit));
			replace(channel->type,      cat1(str_length(type) > 0 ? type : " "));
			replace(channel->desc,      supercat("X%d: %s%s",      vc, channel->name, str_length(desc_type) > 3 ? desc_type : ""));
			replace(channel->desc_long, supercat("X%d: %s (%s)%s", vc, channel->name, channel->full_unit, cf->info));
		}
	}

	chanset->N_total_chan = vci;  // save totals
	chanset->N_inv_chan   = ici;

	show_followers(channel_array, 0);
}

int reduce_chanset (ChanSet *chanset, int brd_id, int *scan_ai_chan)
{
	f_start(F_UPDATE);

	int N = 0;
	for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++)
		if (chanset->scope_ai_count[brd_id][chan] > 0)
		{
			scan_ai_chan[N] = chan;
			N++;
		}

	return N;
}

VSP prepare_vset (ChanSet *chanset)
{
	f_start(F_RUN);

	VSP vs = new_vset(chanset->N_total_chan);
	if (vs != NULL)
	{
		for (int vci = 0; vci < chanset->N_total_chan; vci++)
		{
			Channel *channel = chanset->channel_by_vci[vci];

			set_colname(vs, vci, channel->name);
			set_colunit(vs, vci, channel->full_unit);
			set_colsave(vs, vci, channel->save);
		}
	}

	return vs;
}

double lookup_SI_factor (int code)
{
	switch (code)
	{
		case PREFIX_giga  : return 1e9;
		case PREFIX_mega  : return 1e6;
		case PREFIX_kilo  : return 1e3;
		case PREFIX_none  : return 1;
		case PREFIX_milli : return 1e-3;
		case PREFIX_micro : return 1e-6;
		case PREFIX_nano  : return 1e-9;
		case PREFIX_pico  : return 1e-12;
		default           : f_print(F_ERROR, "Error: Code out of range.\n");
		                    return 1;
	}
}

char * lookup_SI_symbol (int code)
{
	switch (code)
	{
		case PREFIX_giga  : return cat1("G");
		case PREFIX_mega  : return cat1("M");
		case PREFIX_kilo  : return cat1("k");
		case PREFIX_none  : return cat1("");
		case PREFIX_milli : return cat1("m");
		case PREFIX_micro : return cat1("μ");
		case PREFIX_nano  : return cat1("n");
		case PREFIX_pico  : return cat1("p");
		default           : f_print(F_ERROR, "Error: Code out of range.\n");
		                    return cat1("");
	}
}
