# Shared by the test scripts that tools/test/difftest.sh runs in the game (Python 1.5.2 syntax). The script sets
# OUT (output file name) and then runs execfile on this file.
import strop, sys, SDTest

out = open(OUT, 'w')

# the functions and data of the exe by name (src/core/testsyms.inc, written by tools/test/testsyms.py): sym is the rva
# that SDTest.Call, Peek and Poke take (they also take the name itself; in ScrapC.exe the address of the C function or
# object minus the load address), symva the address (0x400000 + rva) as the pointers of the game hold it. The scripts
# take the load address 0x400000 in many places: Wine loads Scrap.exe and ScrapC.exe at their preferred base, a run
# where SDTest.Base() is another says so in its output.
def sym(name):
	return SDTest.Sym(name)

def symva(name):
	return 0x400000 + SDTest.Sym(name)

if SDTest.Base() != 0x400000:
	out.write('sdlib: the exe is loaded at %x, the scripts take 0x400000\n' % SDTest.Base())

# floats with 17 digits: repr() of Python 1.5.2 shows only 12, which hides differences in the last bits.
# Python 1.5.2 has no string methods, hence no join.
# every NaN prints as nan: which NaN (sign, payload) an operation passes on depends on the order of its operands,
# which the C compiler may swap for + and *
# Every int goes through SDTest.Orig: an address of a C function or C object of ScrapC.exe (absolute or as an rva) prints
# as the original's (stage 3c2), so vtable pointers, string pointers and function rvas compare between the runs.
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
	if type(v) == type(0):
		return repr(SDTest.Orig(v))
	if type(v) == type([]):
		return lrepr(v)
	return repr(v)

# a value that holds no address (pixels, colors, hashes): its ints as longs, which fmt prints as they are; a color or
# a packed number can be the address of a C object or function in ScrapC.exe
def plain(v):
	if type(v) == type(0):
		return long(v)
	if type(v) == type([]):
		return map(plain, v)
	if type(v) == type(()):
		return tuple(map(plain, v))
	return v

# repr of a list or of what it holds, with the ints through SDTest.Orig
def lrepr(v):
	if type(v) == type(0):
		return repr(SDTest.Orig(v))
	if type(v) == type([]) or type(v) == type(()):
		r = ''
		for x in v:
			if r:
				r = r + ', '
			r = r + lrepr(x)
		if type(v) == type([]):
			return '[' + r + ']'
		if len(v) == 1:
			return '(' + r + ',)'
		return '(' + r + ')'
	return repr(v)

def check(name, fn):
	try:
		r = fmt(fn())
	except:
		r = 'exception %s: %s' % (sys.exc_type, sys.exc_value)
	out.write('%s = %s\n' % (name, r))
	out.flush() # a crash in the game must not lose the lines before it

# whether the code pointer p (a vtable slot) is the function at rva: the original's or, in ScrapC.exe, the C function
# (rva is a name's sym() there, the C function itself); in the hooked run of the DLL the C function the hook at rva
# jumps to
def isfn(p, rva):
	if p - 0x400000 == rva:
		return 1
	return SDTest.Peek(rva, 'b') == 0xe9 and p == 0x400000 + rva + 5 + SDTest.Peek(rva + 1, 'i')

def finish():
	out.write('done\n')
	out.close()
