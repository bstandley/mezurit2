
from _mezurit2compute import *
from math import *

gpib_device_list = []
def reset_gpib () :  # called by compute_reset() in main program
	global gpib_device_list
	for dev in gpib_device_list :
		dev.reset()

class GPIB_Device :

	def __init__ (self, cmd, period, dummy_value, reply_fmt, write_fmt) :

		global gpib_device_list
		gpib_device_list.append(self)

		self.cmd = cmd
		self.period = period
		self.dummy_value = dummy_value
		self.reply_fmt = reply_fmt
		self.write_fmt = write_fmt

		self.eos = 0
		self.intro = '*IDN?'
		self.noninverse_fn = 0
		self.inverse_fn = 0
		self.slotid = [[-2 for i in range(32)] for i in range(8)]

	def __call__ (self, brd, pad) :
		if self.slotid[brd][pad] == -2 :
			self.slotid[brd][pad] = gpib_slot_add(brd, pad, self.eos, self.intro, self.cmd, self.period, self.dummy_value, self.reply_fmt, self.write_fmt, self.noninverse_fn, self.inverse_fn)
		return gpib_slot_read(brd, self.slotid[brd][pad])

	def reset (self) :
		self.slotid = [[-2 for i in range(32)] for i in range(8)]

def nearest_index (l, x) :  # Use to 'invert' lookup tables. List 'l' must be pre-sorted.
	if x < l[0] : return 0
	for i in range(len(l) - 1) :
		if x < l[i + 1] : return i if (x - l[i] < l[i + 1] - x) else i + 1
	return len(l) - 1

##############  Channel Definition Functions  ###############

#def panel ()             : <built-in function>
#def time  ()             : <built-in function>
#def ch    (chan)         : <built-in function>
#def DAC   (dev_id, chan) : <built-in function>
#def ADC   (dev_id, chan) : <built-in function>
#def DIO   (dev_id, chan) : <built-in function>

def DAC0 () : return DAC(0, 0)
def DAC1 () : return DAC(0, 1)

def VDAC0 () : return DAC(2, 0)
def VDAC1 () : return DAC(2, 1)

def ADC0  () : return ADC(0, 0)
def ADC1  () : return ADC(0, 1)
def ADC2  () : return ADC(0, 2)
def ADC3  () : return ADC(0, 3)
def ADC4  () : return ADC(0, 4)
def ADC5  () : return ADC(0, 5)
def ADC6  () : return ADC(0, 6)
def ADC7  () : return ADC(0, 7)
def ADC8  () : return ADC(0, 8)
def ADC9  () : return ADC(0, 9)
def ADC10 () : return ADC(0, 10)
def ADC11 () : return ADC(0, 11)
def ADC12 () : return ADC(0, 12)
def ADC13 () : return ADC(0, 13)
def ADC14 () : return ADC(0, 14)
def ADC15 () : return ADC(0, 15)

def mag    (X, Y)        : return (X**2 + Y**2)**0.5
def divide (N, D, level) : return (N / D if (abs(D) > level) else 0.0)

#####################  GPIB Functions  ######################

A8648_Freq    = GPIB_Device('FREQ:CW?',  0.5, 1e6,   '%lf',  'FREQ:CW %1.8e HZ')   # Agilent 8648 source: current frequency on Agilent 8648 source
A8648_Ampl    = GPIB_Device('POW:AMPL?', 0.5, -10,   '%lf',  'POW:AMPL %1.6f DB')  # Agilent 8648 source: current amplitude, read in dBmW (the only option), written in dBmW
A8648_Ampl_V  = GPIB_Device('POW:AMPL?', 0.5, -10,   '%lf',  'POW:AMPL %1.6f DB')  # Agilent 8648 source: current amplitude, read in dBmW (the only option), written in dBmW
SR830_SineOut = GPIB_Device('SLVL?',     0.5, 50e-3, '%lf',  'SLVL%f')             # SRS SR830 lock-in:   SINE output level
SR830_SensIn  = GPIB_Device('SENS?',     1.0, 17,    '%lf',  'SENS%0.0f')          # SRS SR830 lock-in:   input sensitivity
ITC503_Temp   = GPIB_Device('R1\r',      1.0, 300,   'R%lf', '')                   # Oxford ITC503:       current temperature on sensor 1 in Kelvin
IPS120_Field  = GPIB_Device('R8\r',      1.0, 0.0,   'R%lf', '')                   # Oxford IPS120:       set field in Tesla

rt20 = sqrt(20)
A8648_Ampl_V.noninverse_fn = lambda x_dBmW : (10 ** (x_dBmW / 20)) / rt20  # returns V
A8648_Ampl_V.inverse_fn    = lambda x_V    : 20 * log10(rt20 * x_V)        # returns dB

sens_table = [2e-9, 5e-9, 1e-8, 2e-8, 5e-8, 1e-7, 2e-7, 5e-7, 1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3, 1e-2, 2e-2, 5e-2, 1e-1, 2e-1, 5e-1, 1.0]
SR830_SensIn.noninverse_fn = lambda x : sens_table[int(x)]
SR830_SensIn.inverse_fn    = lambda x : nearest_index(sens_table, x)

ITC503_Temp.eos   = IPS120_Field.eos   = 0x400 | 0xd
ITC503_Temp.intro = IPS120_Field.intro = 'V\r'

##############  Trigger Instruction Functions ###############

# These are the subset of the terminal functions which are
# appropriate as trigger actions. The wait() function
# takes the role of xleep() so as to not block the
# aquisition thread.

def send_recv_local_check (cmd_full) :
	reply_full = send_recv_local(cmd_full)
	cmd = cmd_full.split(';')[0]
	reply = reply_full.split(';')[0]
	return (cmd == reply)

#def wait            (delta_t)    : <built-in function>
def set_recording    (on)         : return send_recv_local_check('set_recording;on|{0:d}'.format(on))

def set_dac          (ch, target) : return send_recv_local_check('set_dac;channel|{0:d};target|{1:f}'.format(ch, target))
def set_dio          (ch, target) : return set_dac(ch, target)  # uses same mechanism as above, i.e. setting an invertible channel's value
def sweep_stop       (ch)         : return send_recv_local_check('sweep_stop;channel|{0:d}'.format(ch))
def sweep_down       (ch)         : return send_recv_local_check('sweep_down;channel|{0:d}'.format(ch))
def sweep_up         (ch)         : return send_recv_local_check('sweep_up;channel|{0:d}'.format(ch))

def request_pulse    (ch, target) : return send_recv_local_check('request_pulse;channel|{0:d};target|{1:f}'.format(ch, target))
def cancel_pulse     ()           : return send_recv_local_check('request_pulse;channel|-1')
def fire_scope       ()           : return send_recv_local_check('set_scanning;on|1')
def cancel_scope     ()           : return send_recv_local_check('set_scanning;on|0')
def fire_scope_pulse (ch, target) : return fire_scope() if request_pulse(ch, target) else False

def arm_trigger      (_id)        : return send_recv_local_check('arm_trigger;id|{0:d}'.format(_id))
def disarm_trigger   (_id)        : return send_recv_local_check('disarm_trigger;id|{0:d}'.format(_id))
def force_trigger    (_id)        : return send_recv_local_check('force_trigger;id|{0:d}'.format(_id))
def cancel_trigger   (_id)        : return send_recv_local_check('cancel_trigger;id|{0:d}'.format(_id))
def emit_signal      (signal)     : return send_recv_local_check('emit_signal;name|' + signal)

def new_set          ()           : return send_recv_local_check('new_set')
def t_zero           ()           : return send_recv_local_check('tzero')
def save_data        (filename)   : return send_recv_local_check('save;filename|' + filename)
