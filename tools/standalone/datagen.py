#!/usr/bin/env python3
# datagen.py <Scrap.exe> <outdir> [objects.txt]: the data of Scrap.exe 1.6.1 for ScrapC.exe (docs/standalone.md, stage
# 3). Run by the build (CMake cache path SCRAP_EXE) after the objects of scrapcode_sa, before the link of ScrapC.exe.
# Checks the build like Game_IsBuild and writes into outdir:
#   room.bin  the bytes of the original that are built in, one piece after the other
#   room.c    their input sections .room.<rva> (initialized where the exe has data and for the pieces of .text that
#             are still read or run, uninitialized in between)
#   room.ld   the sections .orig0, .orig1 ... of the room at rva 0x1000..SizeOfImage, included by cmake/ScrapC.ld:
#             the pieces of room.c and the C objects at their rvas
#   room.h    what main.c needs of the headers (import and resource directory) and the code ranges it fills with int3
# objects.txt lists the object files of scrapcode_sa (one per line). Their sections .sd.<rva> are the C objects of
# SD_AT (include/scrap/shared.h, stage 3b): each one is placed at its rva and the bytes of the original there are left
# out. They must lie in the data, must not overlap each other or the import table, and must hold what the original
# holds there: the same bytes, a pointer where the original has one, to the C function its hook jumps to (src/hooks.inc,
# the CRT entries), to the real symbol of the Python or to another C object at the original target.
# A pointer of the copy into the original's Python (from .reloc) becomes the real symbol of src/python, resolved by
# the linker; a target without a real symbol stops the build. .reloc itself is not built in (fixed base).
import hashlib, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import BASE, ROOT, Exe, ExeError, c_names

ROOM = 0x1000
MERGE = 0x10000	# zero gaps up to this size stay inside an initialized section (fewer sections)


def header(name):
	return open(os.path.join(ROOT, 'include', 'scrap', name)).read()


def define(text, name):
	return int(re.search(r'#define %s (0x[0-9a-fA-F]+)' % name, text).group(1), 16)


def ranges(text, name):
	return [(int(lo, 16), int(hi, 16)) for lo, hi in
		re.findall(r'\{ (0x[0-9a-f]+), (0x[0-9a-f]+) \}', text[text.index('#define ' + name):].split('\n\n')[0])]


game_h, crt_h, py_h = header('game.h'), header('origcrt.h'), header('origpython.h')
TIMESTAMP, SIZEOFIMAGE = define(game_h, 'GAME_TIMESTAMP'), define(game_h, 'GAME_SIZEOFIMAGE')
PY_CODE = (define(py_h, 'ORIG_PY_CODE_START'), define(py_h, 'ORIG_PY_CODE_END'))
PY_DATA = ranges(py_h, 'ORIG_PY_DATA')

# the pieces of .text that are built in; the rest of it is int3 at start
TEXT_KEEP = [
	(0x001000, 0x002070),	# the data formats of dinput8.lib (c_dfDIKeyboard and friends point here)
] + ranges(crt_h, 'ORIG_CRT_THUNKS')	# the import thunks of the game in the CRT, called through imports

# what pointers outside the original's Python point to in it, and the real symbol for each (pymap.py -c makes this
# table): the type objects of the engine (ob_type), the module table of the exe and the code of the copy hold these
PY_SYMBOLS = {
	0x2e3ab0: 'initimp',
	0x2e45d0: 'initerrno',
	0x2e5340: 'initmath',
	0x2e5910: 'initnew',
	0x2e6350: 'initregex',
	0x2e6da0: 'initsignal',
	0x2e8840: 'initstrop',
	0x2e97d0: 'init_locale',
	0x2e9ba0: 'PyMarshal_Init',
	0x432b90: 'PyString_Type',
	0x432ff0: 'PyList_Type',
	0x433128: 'PyInt_Type',
	0x433288: 'PyFloat_Type',
	0x435f3c: '_Py_NoneStruct',
	0x435f50: 'PyType_Type',
	0x437150: 'PyCode_Type',
	0x4e79cc: 'g_PyPackImport',
	0x4e7af8: 'Py_VerboseFlag',
}

# the sections of the original a C object may not lie in: the code, the resources (main.c points the resource
# directory there) and the relocations
NO_C_SECTIONS = ('.text', '.rsrc', '.reloc')


def fail(msg):
	sys.exit('datagen.py: %s' % msg)


def in_python(rva):
	return PY_CODE[0] <= rva < PY_CODE[1] or any(lo <= rva < hi for lo, hi in PY_DATA)


def align(x):
	return (x + 0xfff) & ~0xfff


class CObj:
	"""a section .sd.<rva> of an object file: one C object of SD_AT"""

	def __init__(self, path, name, rva, size, bss, alignment, data, relocs):
		self.path, self.name, self.rva, self.size, self.bss = path, name, rva, size, bss
		self.alignment, self.data, self.relocs = alignment, data, relocs
		self.lo, self.hi = rva, rva + size

	def where(self):
		return '%s (%s)' % (self.name, os.path.relpath(self.path))


# the global symbols defined in the .sd sections of all objects, with their rvas
sd_symbols = {}


def coff_sd(path):
	"""the .sd.<rva> sections of a COFF object file, with their relocations as {offset: (symbol, target)}: target is
	the rva of a target in a .sd section of the file (symbol value plus the addend in place), None for a global
	symbol of another section or file (a C function, the Python, an SD_AT object of another file)"""
	try:
		f = open(path, 'rb')
	except OSError as e:
		fail('cannot read %s (%s)' % (path, e.strerror))
	with f:
		hdr = f.read(20)
		if len(hdr) < 20:
			fail('%s: no COFF object' % path)
		machine, nsec, _, symptr, nsym, optsz, _ = struct.unpack('<HHIIIHH', hdr)
		if machine != 0x14c:
			fail('%s: not an i386 COFF object (machine %04x)' % (path, machine))
		f.seek(20 + optsz)
		table = f.read(40 * nsec)
		f.seek(symptr)
		syms = f.read(18 * nsym)
		strtab = f.read()

		def string(o):
			return strtab[o:strtab.index(b'\0', o)].decode()

		secs = []
		for i in range(nsec):
			raw = table[40 * i:40 * i + 8]
			name = string(int(raw[1:].rstrip(b'\0'))) if raw.startswith(b'/') else raw.rstrip(b'\0').decode()
			size, ptr, relptr, _, nrel, _, flags = struct.unpack_from('<IIIIHHI', table, 40 * i + 16)
			secs.append((name, size, ptr, relptr, nrel, flags))
		sd = {}
		for i, (name, size, ptr, relptr, nrel, flags) in enumerate(secs):
			if name.startswith('.sd.'):
				try:
					sd[i + 1] = int(name[4:], 0)
				except ValueError:
					fail('%s: section %s: SD_AT needs an rva number' % (path, name))
		if not sd:
			return []

		def symbol(k):
			raw = syms[18 * k:18 * k + 8]
			name = string(struct.unpack_from('<I', raw, 4)[0]) if raw[:4] == b'\0\0\0\0' else raw.rstrip(b'\0').decode()
			value, secnum, _, sclass, naux = struct.unpack_from('<IhHBB', syms, 18 * k + 8)
			return name, value, secnum, sclass, naux

		k = 0
		while k < nsym:
			name, value, secnum, sclass, naux = symbol(k)
			if sclass == 2 and secnum in sd:	# IMAGE_SYM_CLASS_EXTERNAL
				sd_symbols[name] = sd[secnum] + value
			k += 1 + naux

		out = []
		for i, (name, size, ptr, relptr, nrel, flags) in enumerate(secs):
			if i + 1 not in sd:
				continue
			bss = bool(flags & 0x80)
			data = b'\0' * size
			if not bss:
				f.seek(ptr)
				data = f.read(size)
			alignment = 1 << ((flags >> 20 & 0xf) - 1) if flags >> 20 & 0xf else 16
			f.seek(relptr)
			rel = f.read(10 * nrel)
			relocs = {}
			for k in range(nrel):
				at, symidx, typ = struct.unpack_from('<IIH', rel, 10 * k)
				if typ != 6:	# IMAGE_REL_I386_DIR32
					fail('%s: %s: relocation type %d at +%x, only absolute pointers are placed' % (path, name, typ, at))
				sname, value, secnum, sclass, _ = symbol(symidx)
				inplace = struct.unpack_from('<I', data, at)[0]
				if secnum in sd:
					relocs[at] = (sname, sd[secnum] + value + inplace)
				elif sclass != 2:
					fail('%s: %s: the pointer at +%x leads to %s, static C that has no rva of the original'
						% (path, name, at, sname))
				elif inplace:
					fail('%s: %s: the pointer at +%x has an offset to %s' % (path, name, at, sname))
				else:
					relocs[at] = (sname, None)
			out.append(CObj(path, name, sd[i + 1], size, bss, alignment, data, relocs))
		return out


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

# the pieces: the kept parts of .text and every section with bytes but .text and .reloc
text_va, text_n, _, _ = secs['.text']
pieces = [(lo, hi or text_va + text_n) for lo, hi in TEXT_KEEP]
for name, (va, n, ptr, vsize) in sorted(secs.items(), key=lambda s: s[1][0]):
	if name not in ('.text', '.reloc') and n:
		pieces.append((va, va + n))
pieces.sort()
for (lo, hi), (lo2, hi2) in zip(pieces, pieces[1:]):
	if hi > lo2:
		fail('pieces %06x..%06x and %06x..%06x overlap' % (lo, hi, lo2, hi2))


def emitted(rva):
	return any(lo <= rva < hi for lo, hi in pieces)


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
	if not any(lo <= at and at + 4 <= hi for lo, hi in pieces):
		fail('the pointer at rva %06x crosses the end of a piece' % at)
	fix[at] = PY_SYMBOLS[t]

# the C objects
cobjs = []
if len(sys.argv) == 4:
	for line in open(sys.argv[3]):
		if line.strip():
			cobjs += coff_sd(line.strip())
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


def check(o):
	"""the C object holds what the original holds at its place"""
	orig = x.bytes(o.lo, o.hi)
	at = 0
	while at < o.size:
		rva = o.lo + at
		if at in o.relocs:
			sym, target = o.relocs[at]
			if rva not in x.relocs():
				fail('%s: a pointer to %s at rva %06x, where the original has none' % (o.where(), sym, rva))
			t = x.u32(rva) - BASE
			if target is not None:
				if target != t:
					fail('%s: the pointer at rva %06x leads to rva %06x, the original\'s to %06x'
						% (o.where(), rva, target, t))
			elif sd_symbols.get(sym, t + 1) != t and (sym[1:] not in (hooks.get(t), PY_SYMBOLS.get(t)) or sym[0] != '_'):
				fail('%s: the pointer at rva %06x is %s, the original\'s leads to rva %06x (%s)'
					% (o.where(), rva, sym, t, hooks.get(t) or PY_SYMBOLS.get(t) or 'no C function'))
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
for at in fix:
	if not any(lo <= at and at + 4 <= hi for lo, hi, o in units):
		fail('the pointer at rva %06x crosses the edge of a C object' % at)

# groups of units become initialized sections, the space between them uninitialized ones
groups = []
for u in units:
	if groups and u[0] - groups[-1][-1][1] < MERGE:
		groups[-1].append(u)
	else:
		groups.append([u])
if groups[0][0][0] != ROOM:
	fail('the first piece does not start at rva 0x%x' % ROOM)
sections = []	# (start, end, units or None)
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
		asm += ['.section %s,"bw"' % name, '.space 0x%x' % (stop - start)]
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
for lo, hi in pieces:
	if lo >= text_va + text_n:
		break
	if lo > at:
		int3.append((at, lo))
	at = hi
if at < text_va + text_n:
	int3.append((at, text_va + text_n))

gen = 'generated by tools/standalone/datagen.py from Scrap.exe 1.6.1, do not edit'
files = {
	'room.bin': bytes(blob),
	# the digest makes room.c change with room.bin, which ccache does not hash (.incbin)
	'room.c': ('/* %s */\n__asm__(\n\t"# room.bin sha1 %s\\n"\n%s);\n' % (gen, digest,
		''.join('\t"%s\\n"\n' % l.replace('"', '\\"') for l in asm))).encode(),
	'room.ld': ('/* %s: the room of Scrap.exe at its rvas, with the C objects of SD_AT */\n%s' % (gen, ''.join(ld)))
		.encode(),
	'room.h': ('/* %s */\n#define ROOM_IMPORT_RVA 0x%06x\n#define ROOM_RESOURCE_RVA 0x%06x\n#define ROOM_RESOURCE_SIZE '
		'0x%x\n/* the code of the original that is not built in, int3 at start */\n#define ROOM_INT3 \\\n\t{ %s }\n'
		% (gen, x.dirs[1][0], x.dirs[2][0], x.dirs[2][1], ', '.join('{ 0x%06x, 0x%06x }' % r for r in int3))).encode(),
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
	print('datagen.py:   %-8s %7d bytes of the original (%d non-zero), %d in %d C objects'
		% (name, orig, nonzero, sum(o.size for o in inc), len(inc)))
