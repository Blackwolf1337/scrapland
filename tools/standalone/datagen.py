#!/usr/bin/env python3
# datagen.py --check <Scrap.exe> <objects.txt>: checks the C objects of ScrapC.exe against the data of Scrap.exe 1.6.1
# (docs/standalone.md, stages 3 and 3c2). Optional: the build of ScrapC.exe does not run it and does not need the
# original exe; the CMake target datacheck runs it with SCRAP_EXE (and rsrccheck.py on the resources).
#
# objects.txt lists the object files of the check build (the library scrapdata_check, SD_DATACHECK of
# include/scrap/shared.h), where each C object of SD_AT and SD_BSS has a section .sd.<rva> of its own. Until stage 3c2
# this tool built the room of ScrapC.exe from them (the objects at their rvas, the rest of the original's image as
# uninitialized space); ScrapC.exe links them as plain globals now, and only the check is left. It stops when a C object
#   - lies outside the data of the original (in .text, .rsrc or .reloc), overlaps another one or is not at a multiple
#     of its alignment
#   - does not hold what the original holds there: every byte must be the same, but for pointers. Where the original
#     has a pointer (.reloc), the C object must have one too, to the C function the hook of the original target jumps
#     to (src/hooks.inc, the CRT entries of imports.inc and crthooks.inc), to the real Python symbol, or to another C
#     object at the original target. A bss object needs zeros in the original
#   - leaves out a byte that the map of stage 3b2 (tools/standalone/datamap.txt) keeps: every used byte of the original's
#     data must be a C object (stage 3c1b)
# Then it prints what the C objects cover per section.
import bisect, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import BASE, PY_SYMBOLS, SIZEOFIMAGE, TIMESTAMP, Exe, ExeError, c_names, coff_sd, in_python, room_pieces
from datamap import MAP, kept, read_map

# the sections of the original a C object may not lie in: the code, the resources and the relocations
NO_C_SECTIONS = ('.text', '.rsrc', '.reloc')


def fail(msg):
	sys.exit('datagen.py: %s' % msg)


if len(sys.argv) != 4 or sys.argv[1] != '--check':
	fail('usage: datagen.py --check <Scrap.exe> <objects.txt>')
exe, objlist = sys.argv[2], sys.argv[3]
try:
	x = Exe(exe)
except ExeError as e:
	fail(str(e))
if x.stamp != TIMESTAMP or x.size != SIZEOFIMAGE:
	fail('%s is not Scrap.exe 1.6.1 (timestamp %08x, size %08x)' % (exe, x.stamp, x.size))
if x.base != BASE:
	fail('%s: image base not 0x%x' % (exe, BASE))
secs = x.secs

# the data of the original that the map of stage 3b2 keeps: it must all be C objects
whole = room_pieces(x)
try:
	keep = kept(read_map(MAP))
except (OSError, ValueError, IndexError) as e:
	fail('cannot read %s (%s)' % (MAP, e))
pieces = []
for lo, hi in whole:
	for a, z in keep:
		if a < hi and z > lo:
			pieces.append((max(a, lo), min(z, hi)))
pieces.sort()

# the C objects
cobjs = []
sd_symbols = {}	# the global symbols defined in the .sd sections of all objects, with their rvas
try:
	for line in open(objlist):
		if line.strip():
			cobjs += coff_sd(line.strip(), sd_symbols)
except OSError as e:
	fail('cannot read %s (%s): build the target scrapdata_check' % (objlist, e.strerror))
except ExeError as e:
	fail('%s: build the target scrapdata_check' % e)
if not cobjs:
	fail('%s: no C object with a section .sd.<rva> (the objects of the check build, SD_DATACHECK)' % objlist)
cobjs.sort(key=lambda o: o.lo)
hooks = c_names()
for o in cobjs:
	if o.size == 0:
		fail('%s is empty' % o.where())
	if o.lo % o.alignment:
		fail('%s: rva %06x is not a multiple of its alignment %d' % (o.where(), o.lo, o.alignment))
	sec = x.section(o.lo)
	va, n, ptr, vsize = secs.get(sec, (0, 0, 0, 0))
	if sec is None or sec in NO_C_SECTIONS or o.hi > va + max(n, vsize):
		fail('%s: rva %06x..%06x is not in the data of the original (%s)' % (o.where(), o.lo, o.hi, sec or 'no section'))
for a, z in zip(cobjs, cobjs[1:]):
	if a.hi > z.lo:
		fail('%s at rva %06x..%06x and %s at rva %06x..%06x overlap' % (a.where(), a.lo, a.hi, z.where(), z.lo, z.hi))


def c_name(sym):
	"""the C name of an assembler symbol: _name (cdecl, thiscall, data), _name@n (stdcall), @name@n (fastcall)"""
	m = re.match(r'^[_@](\w+?)(@\d+)?$', sym)
	return m.group(1) if m else None


def check(o):
	"""the C object holds what the original holds at its place"""
	orig = x.bytes(o.lo, o.hi)
	at = 0
	while at < o.size:
		rva = o.lo + at
		if at in o.relocs:
			sym, target, addend = o.relocs[at]
			if rva not in x.relocs():
				fail('%s: a pointer to %s at rva %06x, where the original has none' % (o.where(), sym, rva))
			t = x.u32(rva) - BASE
			if target is None and sym in sd_symbols:
				target = sd_symbols[sym] + addend
			if target is not None:
				if target != t:
					fail('%s: the pointer at rva %06x leads to rva %06x, the original\'s to %06x'
						% (o.where(), rva, target, t))
			elif addend or c_name(sym) not in (hooks.get(t), PY_SYMBOLS.get(t)):
				fail('%s: the pointer at rva %06x is %s%s, the original\'s leads to rva %06x (%s)'
					% (o.where(), rva, sym, ' + %d' % addend if addend else '', t,
					hooks.get(t) or PY_SYMBOLS.get(t) or 'no C function'))
			at += 4
			continue
		if o.data[at] != orig[at]:
			fail('%s: byte %02x at rva %06x, the original has %02x' % (o.where(), o.data[at], rva, orig[at]))
		at += 1


for o in cobjs:
	check(o)

# every byte the map keeps lies in a C object; the pointers of the original into its Python there are C objects too
starts = [o.lo for o in cobjs]


def covered_to(rva):
	"""the end of the run of C objects from rva (rva itself when no object holds it)"""
	i = bisect.bisect_right(starts, rva) - 1
	while i >= 0 and i < len(cobjs) and cobjs[i].lo <= rva < cobjs[i].hi:
		rva = cobjs[i].hi
		i += 1
	return rva


missing = []
for lo, hi in pieces:
	at = lo
	while at < hi:
		end = covered_to(at)
		if end > at:
			at = end
			continue
		nxt = starts[bisect.bisect_right(starts, at)] if bisect.bisect_right(starts, at) < len(starts) else hi
		missing.append((at, min(hi, nxt)))
		at = min(hi, nxt)
for lo, hi in missing[:20]:
	print('datagen.py: rva %06x..%06x of the original is used but no C object' % (lo, hi), file=sys.stderr)
if missing:
	fail('%d pieces of the original that the map keeps are no C object: make them C or drop their users from %s '
		'(docs/standalone.md, stage 3c1b)' % (len(missing), os.path.relpath(MAP)))
py = sum(1 for o in cobjs for at in o.relocs if in_python(x.u32(o.lo + at) - BASE))

# what the C objects cover per section
print('datagen.py: %d C objects match Scrap.exe 1.6.1, %d pointers into the Python lead to the real one'
	% (len(cobjs), py))
for name, (va, n, ptr, vsize) in sorted(secs.items(), key=lambda s: s[1][0]):
	end = va + max(n, vsize)
	inc = [o for o in cobjs if va <= o.lo < end]
	if not inc:
		continue
	bss = [o for o in inc if o.bss]
	print('datagen.py:   %-8s %7d bytes in %d C objects%s' % (name, sum(o.size for o in inc), len(inc),
		' (%d of them bss in %d)' % (sum(o.size for o in bss), len(bss)) if bss else ''))
