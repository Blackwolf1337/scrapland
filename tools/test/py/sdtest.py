# Calls engine script functions and writes the results to sdtest.out (Python 1.5.2 syntax). tools/test/difftest.sh
# runs it with the C functions and with the originals and compares the two outputs. The game reads it from its
# root directory through -PythonExecute:execfile(...); the engine's command line drops quotes and splits at spaces,
# so difftest.sh spells the file name with chr().
import sys
import Scrap, SWeap

out = open('sdtest.out', 'w')

def check(name, fn):
	try:
		r = repr(fn())
	except:
		r = 'exception %s: %s' % (sys.exc_type, sys.exc_value)
	out.write('%s = %s\n' % (name, r))

check('Scrap.GetMoney()', lambda: Scrap.GetMoney())
check('Scrap.GetMoney(1)', lambda: Scrap.GetMoney(1))
check('Scrap.GetLevelPath()', lambda: Scrap.GetLevelPath())
check('Scrap.GetLevelPath("x")', lambda: Scrap.GetLevelPath("x"))
check('SWeap.GetNWeap()', lambda: SWeap.GetNWeap())
check('SWeap.GetNWeap(None)', lambda: SWeap.GetNWeap(None))
out.write('done\n')
out.close()
