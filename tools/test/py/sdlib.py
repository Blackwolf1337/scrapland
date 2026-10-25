# Shared by the test scripts that tools/test/difftest.sh runs in the game (Python 1.5.2 syntax). The script sets
# OUT (output file name) and then runs execfile on this file.
import sys

out = open(OUT, 'w')

# floats with 17 digits: repr() of Python 1.5.2 shows only 12, which hides differences in the last bits.
# Python 1.5.2 has no string methods, hence no join.
def fmt(v):
	if type(v) == type(0.0):
		return '%.17g' % v
	if type(v) == type(()):
		r = ''
		for x in v:
			if r:
				r = r + ', '
			r = r + fmt(x)
		return '(' + r + ')'
	return repr(v)

def check(name, fn):
	try:
		r = fmt(fn())
	except:
		r = 'exception %s: %s' % (sys.exc_type, sys.exc_value)
	out.write('%s = %s\n' % (name, r))
	out.flush() # a crash in the game must not lose the lines before it

def finish():
	out.write('done\n')
	out.close()
