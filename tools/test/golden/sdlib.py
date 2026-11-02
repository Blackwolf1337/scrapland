# Shared by the golden test scripts (Python 1.5.2 syntax): the script sets OUT (output file name) and then runs execfile
# on this file. Each check writes "name = value"; tools/test/golden.sh compares the output with the stored one.
import strop, sys

out = open(OUT, 'w')

# floats with 17 digits: repr() of Python 1.5.2 shows only 12, which hides differences in the last bits.
# Python 1.5.2 has no string methods, hence no join.
# every NaN prints as nan: which NaN (sign, payload) an operation passes on depends on the order of its operands,
# which the C compiler may swap for + and *
def fmt(v):
	if type(v) == type(0.0):
		r = '%.17g' % v
		if strop.find(r, 'nan') >= 0:
			return 'nan'
		return r
	if type(v) == type(()):
		r = ''
		for x in v:
			if r:
				r = r + ', '
			r = r + fmt(x)
		return '(' + r + ')'
	return repr(v)

# R_LastDevice is "<adapter> [<driver>]  <driver version>": the adapter and driver are the system's, so only that there
# is a name compares
def devname(v, s):
	if v == 'R_LastDevice' and type(s) == type(''):
		return s != ''
	return s

# the getters of Scrap whose value is the platform's: the number of the current video mode (an index into the modes
# of the platform)
def platvalue(name, v):
	if name == 'GetVideoCurrentMode':
		return v >= 0
	return v

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
