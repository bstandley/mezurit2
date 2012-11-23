import sys,os,atexit,readline,rlcompleter

sys.ps1 = '\033[94mM2>\033[0m ' if os.name == 'posix' else 'M2> '
sys.ps2 = '\033[94m...\033[0m ' if os.name == 'posix' else '... '

readline.parse_and_bind('tab: complete')

try: readline.read_history_file(os.getenv('M2_TERMINAL_HISTORY'))
except IOError: pass
atexit.register(readline.write_history_file, os.getenv('M2_TERMINAL_HISTORY'))

sys.path.append(os.getenv('M2_LIBPATH'))
from mezurit2control import *

execfile(os.getenv('M2_USER_TERMINAL_PY'))
