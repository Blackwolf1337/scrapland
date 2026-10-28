# Shared by the test scripts that tools/test/difftest.sh runs in the game (Python 1.5.2 syntax). The script sets
# OUT (output file name) and then runs execfile on this file.
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

def check(name, fn):
	try:
		r = fmt(fn())
	except:
		r = 'exception %s: %s' % (sys.exc_type, sys.exc_value)
	out.write('%s = %s\n' % (name, r))
	out.flush() # a crash in the game must not lose the lines before it

# whether the code pointer p (a vtable slot) is the function at rva: the original's, or in ScrapC.exe the C function
# its hook jumps to (the vtables there are C since stage 3b1 and point to the C directly)
def isfn(p, rva):
	if p - 0x400000 == rva:
		return 1
	return SDTest.Peek(rva, 'b') == 0xe9 and p == 0x400000 + rva + 5 + SDTest.Peek(rva + 1, 'i')

def finish():
	out.write('done\n')
	out.close()
