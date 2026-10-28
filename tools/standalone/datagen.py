#!/usr/bin/env python3
# datagen.py <Scrap.exe> <outdir> [objects.txt]: the data of Scrap.exe 1.6.1 for ScrapC.exe (docs/standalone.md, stage
# 3). Run by the build (CMake cache path SCRAP_EXE) after the objects of scrapcode_sa, before the link of ScrapC.exe.
# Checks the build like Game_IsBuild and writes into outdir:
#   room.bin  the bytes of the original that are built in, one piece after the other
#   room.c    their input sections .room.<rva> (initialized where the exe has data and for the pieces of .text that
#             are still read or run, uninitialized in between)
#   room.ld   the sections .orig0, .orig1 ... of the room at rva 0x1000..SizeOfImage, included by cmake/ScrapC.ld:
#             the pieces of room.c and the C objects at their rvas
#   room.h    what main.c needs of the headers (the import directory), the code ranges it fills with int3 and the
#             data the probe of dataprobe.c watches
# objects.txt lists the object files of scrapcode_sa (one per line). Their sections .sd.<rva> are the C objects of
# SD_AT (include/scrap/shared.h, stage 3b): each one is placed at its rva and the bytes of the original there are left
# out. They must lie in the data, must not overlap each other or the import table, and must hold what the original
# holds there: the same bytes, a pointer where the original has one, to the C function its hook jumps to (src/hooks.inc,
# the CRT entries), to the real symbol of the Python or to another C object at the original target.
# A pointer of the copy into the original's Python (from .reloc) becomes the real symbol of src/python, resolved by
# the linker; a target without a real symbol stops the build. .reloc itself is not built in (fixed base).
# Of the data only the rows of tools/standalone/datamap.txt with a user are built in (stage 3b2), the rest of the room
# stays zero. Since stage 3c1b that may only be the import part (the import directory, the import address table, the
# names and the import thunks of the game): any other byte of the original the map keeps must be a C object, else
# the build stops.
import bisect, hashlib, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import (BASE, PY_SYMBOLS, SIZEOFIMAGE, TEXT_KEEP, TIMESTAMP, Exe, ExeError, c_names, coff_sd, in_python,
	room_pieces)
from datamap import MAP, kept, read_map

ROOM = 0x1000
MERGE = 0x10000	# zero gaps up to this size stay inside an initialized section (fewer sections)


# the sections of the original a C object may not lie in: the code, the resources and the relocations
NO_C_SECTIONS = ('.text', '.rsrc', '.reloc')


def fail(msg):
	sys.exit('datagen.py: %s' % msg)


def align(x):
	return (x + 0xfff) & ~0xfff


if len(sys.argv) not in (3, 4):
	fail('usage: datagen.py <Scrap.exe> <outdir> [objects.txt]')
exe, outdir = sys.argv[1], sys.argv[2]
try:
	x = Exe(exe)
except ExeError as e:
	fail(str(e))
if x.stamp != TIMESTAMP or x.size != SIZEOFIMAGE:
	fail('%s is not Scrap.exe 1.6.1 (timestamp %08x, size %08x)' % (exe, x.stamp, x.size))
if x.base != BASE:
	fail('%s: image base not 0x%x' % (exe, BASE))
secs = x.secs

# the pieces: the kept parts of .text and every section with bytes but .text and .reloc, of the data only what the
# map of stage 3b2 (tools/standalone/datamap.txt) keeps
text_va, text_n, _, _ = secs['.text']
whole = room_pieces(x)
try:
	keep = kept(read_map(MAP))
except (OSError, ValueError, IndexError) as e:
	fail('cannot read %s (%s)' % (MAP, e))
pieces = []
for lo, hi in whole:
	if (lo, hi) in TEXT_KEEP:
		pieces.append((lo, hi))
		continue
	for a, z in keep:
		if a < hi and z > lo:
			pieces.append((max(a, lo), min(z, hi)))
pieces.sort()
piece_starts = [lo for lo, hi in pieces]


def emitted(rva, n=1):
	"""rva..rva+n lies in one piece"""
	i = bisect.bisect_right(piece_starts, rva) - 1
	return i >= 0 and rva + n <= pieces[i][1]


# pointers into the original's Python, from .reloc
fix = {}
for at in x.relocs():
	if not emitted(at) or in_python(at):
		continue
	t = x.u32(at) - BASE
	if not in_python(t):
		continue
	if t not in PY_SYMBOLS:
		fail('the pointer at rva %06x leads to rva %06x of the original\'s Python, which has no real symbol' % (at, t))
	if not emitted(at, 4):
		fail('the pointer at rva %06x crosses the end of a piece' % at)
	fix[at] = PY_SYMBOLS[t]

# the C objects
cobjs = []
sd_symbols = {}	# the global symbols defined in the .sd sections of all objects, with their rvas
if len(sys.argv) == 4:
	try:
		for line in open(sys.argv[3]):
			if line.strip():
				cobjs += coff_sd(line.strip(), sd_symbols)
	except ExeError as e:
		fail(str(e))
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
	for d, what in ((1, 'import directory'), (12, 'import address table')):
		lo, size = x.dirs[d]
		if lo < o.hi and o.lo < lo + size:
			fail('%s: rva %06x..%06x overlaps the %s, which main.c fills' % (o.where(), o.lo, o.hi, what))
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

# the units of the room: the pieces without the C objects, and the C objects
units = []	# (lo, hi, CObj or None)
for lo, hi in pieces:
	at = lo
	for o in cobjs:
		if o.hi <= at or o.lo >= hi:
			continue
		if o.lo > at:
			units.append((at, o.lo, None))
		at = o.hi
	if at < hi:
		units.append((at, hi, None))
units += [(o.lo, o.hi, o) for o in cobjs]
units.sort(key=lambda u: u[0])

# since stage 3c1b only the import part of the original is built in (the import thunks of TEXT_KEEP, the import
# directory, the import address table and the names, with the zeros between them; stage 3c1a removes it): any other
# byte of the original the map keeps must be a C object
imports = [[lo, hi] for lo, hi in TEXT_KEEP]
for lo, hi, sec, users, kind in sorted(read_map(MAP)):
	if kind == 'import' or (kind == 'zero' and imports and imports[-1][1] == lo):
		if imports and imports[-1][1] >= lo & ~3:
			imports[-1][1] = max(imports[-1][1], hi)
		else:
			imports.append([lo, hi])
imports = [(lo & ~3, (hi + 3) & ~3) for lo, hi in imports]
for lo, hi, o in units:
	if o is None and not any(a <= lo and hi <= z for a, z in imports):
		fail('rva %06x..%06x of the original would be built in, which is no C object and not the import part: make '
			'it C or drop its users from %s (docs/standalone.md, stage 3c1b)' % (lo, hi, os.path.relpath(MAP)))
for at in fix:
	if not any(lo <= at and at + 4 <= hi for lo, hi, o in units):
		fail('the pointer at rva %06x crosses the edge of a C object' % at)

# groups of units become sections: initialized ones for the bytes of the original and the C objects with data,
# uninitialized ones for the C objects of SD_BSS (bss) and the space between the groups. A unit joins the group
# before it when it starts in the group's last page, else when both hold data and the gap is below MERGE or both
# are bss


def bss_unit(u):
	return u[2] is not None and u[2].bss


groups = []
for u in units:
	if groups:
		g = groups[-1]
		gbss = all(bss_unit(v) for v in g)
		if u[0] < align(g[-1][1]) or (bss_unit(u) == gbss and (gbss or u[0] - g[-1][1] < MERGE)):
			g.append(u)
			continue
	groups.append([u])
sections = []	# (start, end, units or None)
if groups[0][0][0] & ~0xfff > ROOM:
	# the code before the first piece (since stage 3c1b nothing of the original's .text before its CRT is built in)
	sections.append((ROOM, groups[0][0][0] & ~0xfff, None))
for i, g in enumerate(groups):
	start = g[0][0] & ~0xfff
	sections.append((start, g[-1][1], g))
	nxt = groups[i + 1][0][0] & ~0xfff if i + 1 < len(groups) else SIZEOFIMAGE
	if align(g[-1][1]) < nxt:
		sections.append((align(g[-1][1]), nxt, None))
	elif align(g[-1][1]) > nxt:
		fail('the section of rva %06x runs into the next one' % start)

# the input sections: a run of pieces (with zeros between them) up to the next C object, or a C object
os.makedirs(outdir, exist_ok=True)
binpath = os.path.abspath(os.path.join(outdir, 'room.bin'))
blob = bytearray()
asm = []
ld = []
for k, (start, stop, g) in enumerate(sections):
	entries = []	# (offset in the section, input section, the size of a C object)
	if g is None:
		name = '.room.%06x' % start
		asm.append('.section %s,"bw"' % name)
		if k == 0:
			asm += ['.globl _orig_room', '_orig_room:']
		asm.append('.space 0x%x' % (stop - start))
		entries.append((0, name, None))
	at = start
	run = None
	for lo, hi, o in g or ():
		if o is not None:
			entries.append((lo - start, o.name, o.size))
			at, run = hi, None
			continue
		if run is None:
			run = '.room.%06x' % at
			asm.append('.section %s,"dw"' % run)
			if not entries and k == 0:
				asm += ['.globl _orig_room', '_orig_room:']
			entries.append((at - start, run, None))
		if lo > at:
			asm.append('.zero 0x%x' % (lo - at))
		at = lo
		for p in sorted(f for f in fix if lo <= f < hi) + [hi]:
			if p > at:
				asm.append('.incbin "%s", 0x%x, 0x%x' % (binpath, len(blob) + at - lo, p - at))
			if p < hi:
				asm.append('.long _%s' % fix[p])
			at = p + 4
		blob += x.bytes(lo, hi)
		at = hi
	body = ''
	for at, name, size in entries:
		body += '    . = 0x%x;\n    KEEP (*(%s))\n' % (at, name)
		if size is not None:
			body += '    ASSERT (. == 0x%x, "%s: not the size datagen.py read");\n' % (at + size, name)
	ld.append('  .orig%d __image_base__ + 0x%06x :\n  {\n%s  }\n' % (k, start, body))
asm.append('.text')
digest = hashlib.sha1(blob).hexdigest()

int3 = []
at = text_va
for lo, hi in whole:
	if lo >= text_va + text_n:
		break
	if lo > at:
		int3.append((at, lo))
	at = hi
if at < text_va + text_n:
	int3.append((at, text_va + text_n))

# the data of the probe (dataprobe.c): the pieces but for the code of the CRT thunks and the resources, which other
# DLLs read (Wine reads some of them outside the reach of a vectored handler)
rsrc = secs['.rsrc']
probe = [(lo, hi) for lo, hi in whole if (lo, hi) not in TEXT_KEEP and lo != rsrc[0]]

gen = 'generated by tools/standalone/datagen.py from Scrap.exe 1.6.1, do not edit'
files = {
	'room.bin': bytes(blob),
	# the digest makes room.c change with room.bin, which ccache does not hash (.incbin)
	'room.c': ('/* %s */\n__asm__(\n\t"# room.bin sha1 %s\\n"\n%s);\n' % (gen, digest,
		''.join('\t"%s\\n"\n' % l.replace('"', '\\"') for l in asm))).encode(),
	'room.ld': ('/* %s: the room of Scrap.exe at its rvas, with the C objects of SD_AT */\n%s' % (gen, ''.join(ld)))
		.encode(),
	'room.h': ('/* %s */\n#define ROOM_IMPORT_RVA 0x%06x\n#define ROOM_IAT_RVA 0x%06x\n#define ROOM_IAT_SIZE 0x%x\n/* the code of the original that is not built in, '
		'int3 at start */\n#define ROOM_INT3 \\\n\t{ %s }\n/* the data of the original that dataprobe.c watches */\n'
		'#define ROOM_PROBE \\\n\t{ %s }\n'
		% (gen, x.dirs[1][0], x.dirs[12][0], x.dirs[12][1],
			', '.join('{ 0x%06x, 0x%06x }' % r for r in int3), ', '.join('{ 0x%06x, 0x%06x }' % r for r in probe))).encode(),
}
# always written: outputs older than the objects would make every build run this again (room.c of the same text is a
# ccache hit)
for name, data in files.items():
	open(os.path.join(outdir, name), 'wb').write(data)

# what is left of the original per section: its bytes built in (non-zero ones) and what C objects cover
print('datagen.py: %d sections, %d bytes of data, %d pointers into the Python lead to the real one, %d C objects'
	% (len(sections), len(blob), len(fix), len(cobjs)))
for name, (va, n, ptr, vsize) in sorted(secs.items(), key=lambda s: s[1][0]):
	end = va + max(n, vsize)
	kept = [(max(lo, va), min(hi, end)) for lo, hi, o in units if o is None and lo < end and hi > va]
	if not kept and not any(va <= o.lo < end for o in cobjs):
		continue
	orig = sum(hi - lo for lo, hi in kept)
	nonzero = sum(len(x.bytes(lo, hi).replace(b'\0', b'')) for lo, hi in kept)
	inc = [o for o in cobjs if va <= o.lo < end]
	bss = [o for o in inc if o.bss]
	print('datagen.py:   %-8s %7d bytes of the original (%d non-zero), %d in %d C objects%s'
		% (name, orig, nonzero, sum(o.size for o in inc), len(inc),
		' (%d of them bss in %d)' % (sum(o.size for o in bss), len(bss)) if bss else ''))
