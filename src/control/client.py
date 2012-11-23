
import _mezurit2control
from sys import stdout
from math import sin,pi

########################  Utils  ############################

def printnow (string) :
	stdout.write(string)
	stdout.flush()

def cmd (cmd_full) : return cmd_full.split(';')[0]

def arg (cmd_full, i) :
	s = cmd_full.split(';')
	return s[i+1].split('|')[1] if len(s) > i + 1 else ''

########################  Basics  ###########################

echo_mode = False
def set_echo (x) :
	global echo_mode
	echo_mode = x

last_reply = ''
def get_last_reply () :
	global last_reply
	return last_reply

def send_recv (cmd_full) :
	if (echo_mode) : printnow('   Command: ' + cmd_full + '\n')
	global last_reply
	last_reply = _mezurit2control.send_recv_rcmd(cmd_full)
	if (echo_mode) : printnow('   Reply:   ' + last_reply + '\n')
	return last_reply

def send_recv_check (cmd_full) :
	reply = send_recv(cmd_full)
	return cmd(reply) == cmd(cmd_full)

def hello (msg) :
	reply = send_recv('hello;msg_out|' + msg)
	return cmd(reply) == 'hello' and arg(reply, 0) == msg

def xleep (t) : return _mezurit2control.xleep(t)

def gpib (brd, pad, msg, eos=0, expect_reply=True) :
	reply = send_recv('gpib_send_recv;id|{0:d};pad|{1:d};eos|{2:d};msg|{3:s};expect_reply|{4:d}'.format(brd, pad, eos, msg, expect_reply))
	if expect_reply : return arg(reply, 0) if cmd(reply) == 'gpib_send_recv' else 'Error'
	else : printnow('Message sent.\n')

# Examples: gpib(0, 8, 'OFF', expect_reply=False) >> 'Sent'
#           gpib(0, 8, '*IDN?')                   >> 'TOASTMASTER 5000'

##################  Test the connection  ###################

print 'Success\n' if hello('handshake!') else 'Failure\n'

#######################  Acquisition  #######################

def set_recording (on)     : return send_recv_check('set_recording;on|{0:d}'.format(on))
def gpib_pause    (paused) : return send_recv_check('gpib_pause;paused|{0:d}'.format(paused))

def get_time () :
	reply = send_recv('get_time')
	return float(arg(reply, 0)) if cmd(reply) == 'get_time' else 0.0

def read_channel (channel) :
	reply = send_recv('read_channel;channel|{0:d}'.format(channel))
	ok = (cmd(reply) == 'read_channel')
	if not(ok) : printnow('Error! Bad channel!\n')
	return [float(arg(reply, 0)), bool(int(arg(reply, 1)))] if ok else [0.0, False]

def channel_value (channel) : return read_channel(channel)[0]
def channel_known (channel) : return read_channel(channel)[1]

####################  Panel Variables  ######################

def load_user_default     ()         : return arg(send_recv('load_config;mode|user_default'),              0) == '1'
def save_user_default     ()         : return arg(send_recv('save_config;mode|user_default'),              0) == '1'
def load_system_default   ()         : return arg(send_recv('load_config;mode|sys_default'),               0) == '1'
def load_internal_default ()         : return arg(send_recv('load_config;mode|internal_default'),          0) == '1'
def load_last_config      ()         : return arg(send_recv('load_config;mode|last'),                      0) == '1'
def load_config_file      (filename) : return arg(send_recv('load_config;mode|file;filename|' + filename), 0) == '1'
def save_config_file      (filename) : return arg(send_recv('save_config;mode|file;filename|' + filename), 0) == '1'

def set_panel (pid) : send_recv_check('set_panel;pid|{0:d}'.format(pid))
def get_panel () :
	reply = send_recv('get_panel')
	return int(arg(reply, 0)) if cmd(reply) == 'get_panel' else -2

def set_var (line) :
	reply = send_recv('set_var;line|' + line)
	return cmd(reply) == 'set_var' and arg(reply, 0) == '1'

def get_var (line) :
	reply = send_recv('get_var;line|' + line)
	return arg(reply, 1) if cmd(reply) == 'get_var' and arg(reply, 0) == '1' else ''

def make_prefix (pid, tool, tid) : return 'panel{0:d}_{1:s}{2:d}_'.format(pid, tool, tid)

def get_sweep_id (channel) :
	reply = send_recv('get_sweep_id;channel|{0:d}'.format(channel))
	return int(arg(reply, 0)) if cmd(reply) == 'get_sweep_id' else -2

######################  DAC Control  ########################

def set_dac    (channel, target) : return send_recv_check('set_dac;channel|{0:d};target|{1:f}'.format(channel, target))
def sweep_stop (channel)         : return send_recv_check('sweep_stop;channel|{0:d}'.format(channel))
def sweep_down (channel)         : return send_recv_check('sweep_down;channel|{0:d}'.format(channel))
def sweep_up   (channel)         : return send_recv_check('sweep_up;channel|{0:d}'.format(channel))

def sweep_link       (channel1, channel2) : return send_recv_check('sweep_link_setup;channel|{0:d};channel|{1:d}'.format(channel1, channel2))
def sweep_link_clear ()                   : return send_recv_check('sweep_link_clear')

def set_follower    (leader, follower, expr) : return send_recv_check('set_follower;leader|{0:d};follower|{1:d};expr|{2:s}'.format(leader, follower, expr))
def remove_follower (leader)                 : return send_recv_check('set_follower;leader|{0:d};follower|-1'.format(leader))
def show_followers  ()                       : return send_recv_check('show_followers')

def catch_sweep (event, channel) : return send_recv_check('catch_sweep_' + event + ';channel|{0:d}'.format(channel))

#########################  Scope  ###########################

def request_pulse     (channel, target) : return send_recv_check('request_pulse;channel|{0:d};target|{1:f}'.format(channel, target))
def cancel_pulse      ()                : return send_recv_check('request_pulse;channel|-1')
def catch_scan_start  ()                : return send_recv_check('catch_scan_start')
def catch_scan_finish ()                : return send_recv_check('catch_scan_finish')
def fire_scope        ()                : return send_recv_check('set_scanning;on|1')
def fire_scope_pulse  (channel, target) : return fire_scope() if request_pulse(channel, target) else False
def cancel_scope      ()                : return send_recv_check('set_scanning;on|0')

#######################  Triggering  ########################

def arm_trigger    (_id)    : return send_recv_check('arm_trigger;id|{0:d}'.format(_id))
def disarm_trigger (_id)    : return send_recv_check('disarm_trigger;id|{0:d}'.format(_id))
def force_trigger  (_id)    : return send_recv_check('force_trigger;id|{0:d}'.format(_id))
def cancel_trigger (_id)    : return send_recv_check('cancel_trigger;id|{0:d}'.format(_id))
def catch_signal   (signal) : return send_recv_check('catch_signal;name|' + signal)

####################  Buffer Management  ####################

def new_set      ()               : return send_recv_check('new_set')
def t_zero       ()               : return send_recv_check('tzero')
def save_data    (filename)       : return send_recv_check('save;filename|' + filename)
def clear_buffer (confirm, tzero) : return send_recv('clear_buffer;confirm|{0:d};tzero|{1:d}'.format(confirm, tzero))

####################  Sweep Automation  #####################

def make_range (V_i, V_f, N) : return map(lambda n : V_i + (V_f - V_i) * float(n) / float(N), range(N + 1))

def return_to_zero (ch) :

	if (not channel_known(ch)) or channel_value(ch) == 0.0 : return

	up = (channel_value(ch) < 0.0)

	var = make_prefix(get_panel(), 'sweep', get_sweep_id(ch)) + 'zerostop_' + 'up' if up else 'down'
	original = get_var(var)
	set_var(var + '=1')

	if up : sweep_up(ch)
	else  : sweep_down(ch)
	catch_sweep('zerostop', ch)

	set_var(var + '=' + original)

def megasweep_estimate (sweep_ch, N_step) :

	prefix = 'panel{0:d}_sweep{1:d}_'.format(get_panel(), get_sweep_id(sweep_ch))

	rate    = [float(get_var(prefix + 'rate_up')),    float(get_var(prefix + 'rate_down'))]
	V_sweep =  float(get_var(prefix + 'scaled_up')) - float(get_var(prefix + 'scaled_down'))
	t_hold  =  float(get_var(prefix + 'hold_up'))   + float(get_var(prefix + 'hold_down'))
	
	t_sweep = V_sweep / rate[0] + V_sweep / rate[1] + t_hold
	return t_sweep * N_step / 3600.0

def autosweep (sweep_ch, save_dir, step_ch, step_range, filename) :

	stepping = (step_ch >= 0)
	saveup = (save_dir == 1)

	if not channel_known(sweep_ch) :
		printnow('Sweep channel initial value unknown. Unable to start autosweep.\n')
		return

	if stepping and (not channel_known(step_ch)) :
		printnow('Step channel initial value unknown. Unable to start autosweep.\n')
		return

	def do_sweep (ch, sweep_dir) :
		if sweep_dir == 1 :
			sweep_up(ch)
			printnow(', sweep up')
			catch_sweep('max_posthold', ch)
		else :
			sweep_down(ch)
			printnow(', sweep down')
			catch_sweep('min_posthold', ch)

	prefix = make_prefix(get_panel(), 'sweep', get_sweep_id(sweep_ch))
	map(lambda opt : set_var(prefix + opt), ('zerostop_down=0', 'zerostop_up=0', 'endstop_down=1', 'endstop_up=1'))

	printnow('Clearing buffer...\n')
	clear_buffer(True, False)
	set_recording(True)

	V_init = 0.0
	if stepping :
		V_init = channel_value(step_ch)
		printnow('Read inital V = {0:.4f}\n'.format(V_init))

	printnow('Commencing autosweep...\n')
	i = 0
	V_last = V_init
	for V_cur in step_range :

		printnow('  i: {0:d}'.format(i))
		i += 1

		if stepping :
			for V_x in make_range(V_last, V_cur, 20) : set_dac(step_ch, V_x)
			printnow(', V = {0:.4f}'.format(V_cur))
			V_last = V_cur

		do_sweep(sweep_ch, -1 if saveup else 1)
		clear_buffer(False, False)
		printnow(', cleared buffer')

		do_sweep(sweep_ch, 1 if saveup else -1)
		save_data(filename)
		printnow(', saved data\n')
		clear_buffer(False, False)

	if stepping :
		for V_x in make_range(V_last, V_init, 20) : set_dac(step_ch, V_x)
		printnow('Restored inital V = {0:.4f}\n'.format(V_init))

	return_to_zero(sweep_ch)
	set_recording(False)
	printnow('Autosweep complete!\n')

def minisweep_up (sweep_ch, N_step, filename) :
	autosweep(sweep_ch, 1, -1, range(N_step), filename)

def minisweep_down (sweep_ch, N_step, filename) :
	autosweep(sweep_ch, -1, -1, range(N_step), filename)

def megasweep_up (sweep_ch, step_ch, V_start, V_end, N_step, filename) :
	"""
	sweep_ch  Channel to sweep.               Example: 2 for channel X2.
	step_ch   Channel to step between sweeps. Example: 3 for channel X3.
	"""
	autosweep(sweep_ch, 1, step_ch, make_range(V_start, V_end, N_step), filename)

def megasweep_down (sweep_ch, step_ch, V_start, V_end, N_step, filename) :
	"""
	sweep_ch  Channel to sweep.               Example: 2 for channel X2.
	step_ch   Channel to step between sweeps. Example: 3 for channel X3.
	V_start   Starting voltage for step_ch.
	V_end     Ending voltage for step_ch.
	N_step    Number of steps, e.g. V_step = (V_end - V_start) / N_step.
	          (N_sweeps = N_step + 1 so as to include both endpoints.)
	filename  Absolute or relative (to current dir.) to save file.
	"""
	autosweep(sweep_ch, -1, step_ch, make_range(V_start, V_end, N_step), filename)

####################  Demo & Benchmark  #####################

def demo () : # load control_demo.mcf

	def print_rv (rv) :
		printnow('   Return:  {0} {1}\n'.format(rv, type(rv)))

	set_echo(True)

	printnow('\nPanel Variables...\n\n')

	printnow('get_panel()\n')
	pid = get_panel()
	print_rv(pid)

	printnow('get_sweep_id(1)\n')
	sid1 = get_sweep_id(1)
	print_rv(sid1)

	printnow('make_prefix({0}, \'sweep\', {1})\n'.format(pid, sid1))
	sp1 = make_prefix(pid, 'sweep', sid1)
	print_rv(sp1)

	printnow('set_var(\'' + sp1 + 'zerostop_down=1\')\n')
	print_rv(set_var(sp1 + 'zerostop_down=1'))

	printnow('\nAcquisition...\n\n')

	printnow('get_time()\n')
	print_rv(get_time())

	printnow('channel_value(3)\n')
	print_rv(channel_value(3))

	printnow('\nDAC Control...\n\n')

	printnow('set_dac(1, 0.0)\n')
	print_rv(set_dac(1, 0.0))

	printnow('sweep_up(1)\n')
	print_rv(sweep_up(1))
	xleep(5.0)

	printnow('sweep_stop(1)\n')
	print_rv(sweep_stop(1))
	xleep(1.0)

	printnow('sweep_down(1)\n')
	print_rv(sweep_down(1))

	printnow('catch_sweep(\'zerostop\', 1)\n')
	print_rv(catch_sweep('zerostop', 1))

	printnow('set_var(\'' + sp1 + 'zerostop_down=0\')\n')
	print_rv(set_var(sp1 + 'zerostop_down=0'))

	set_echo(False)

def bench () :

	printnow('Testing speed of set_dac()...\n')

	t0 = get_time()
	V_i = channel_value(1)
	for x in range (0, 1001) :
		set_dac(1, V_i + sin(2 * pi * x/1000.0))
	dt = get_time() - t0
	printnow('   1000 steps in {0:f} seconds = {1:f} msec/command\n'.format(dt, dt/1000.0*1e3))
	# 2.6 msec/command in V0.76 using GSockets (UNIX or INET - same) (same as with sys/socket.h)

def demo_linkage (ch1, ch2) :

	printnow('Testing sweep linkage feature...\n')

	set_dac(ch1, 0.0)
	set_dac(ch2, 0.1)
	sweep_link(ch1, ch2)

	clear_buffer(False, False)
	t_zero()
	set_recording(True)

	xleep(0.5)
	sweep_up(ch1)
	xleep(1.0)
	sweep_down(ch2)

	catch_sweep('zerostop', ch1)
	xleep(0.5)
	set_recording(False)

