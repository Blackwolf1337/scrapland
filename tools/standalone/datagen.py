#!/usr/bin/env python3
# datagen.py <Scrap.exe> <outdir>: the data of Scrap.exe 1.6.1 for ScrapC.exe (docs/standalone.md, stage 3a). Run by
# the build (CMake cache path SCRAP_EXE). Checks the build like Game_IsBuild and writes into outdir:
#   room.bin  the bytes of the pieces below, one after the other
#   room.c    the sections .orig0, .orig1 ... of the room at rva 0x1000..SizeOfImage: initialized where the exe has
#             data (and the pieces of .text that are still read or run), uninitialized in between
#   room.ld   their places, included by cmake/ScrapC.ld
#   room.h    what main.c needs of the headers (import and resource directory) and the code ranges it fills with int3
# A pointer of the copy into the original's Python (from .reloc) becomes the real symbol of src/python, resolved by
# the linker; a target without a real symbol stops the build. .reloc itself is not built in (fixed base).
import os, re, struct, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
BASE = 0x400000
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


def fail(msg):
	sys.exit('datagen.py: %s' % msg)


def in_python(rva):
	return PY_CODE[0] <= rva < PY_CODE[1] or any(lo <= rva < hi for lo, hi in PY_DATA)


def align(x):
	return (x + 0xfff) & ~0xfff


if len(sys.argv) != 3:
	fail('usage: datagen.py <Scrap.exe> <outdir>')
exe, outdir = sys.argv[1], sys.argv[2]
try:
	b = open(exe, 'rb').read()
except OSError as e:
	fail('cannot read %s (%s): set SCRAP_EXE to Scrap.exe 1.6.1' % (exe, e.strerror))
if b[:2] != b'MZ' or len(b) < 0x40 or b[struct.unpack_from('<I', b, 0x3c)[0]:][:4] != b'PE\0\0':
	fail('%s: no PE file' % exe)
pe = struct.unpack_from('<I', b, 0x3c)[0]
stamp = struct.unpack_from('<I', b, pe + 8)[0]
opt = pe + 24
size = struct.unpack_from('<I', b, opt + 56)[0]
if stamp != TIMESTAMP or size != SIZEOFIMAGE:
	fail('%s is not Scrap.exe 1.6.1 (timestamp %08x, size %08x)' % (exe, stamp, size))
if struct.unpack_from('<I', b, opt + 28)[0] != BASE:
	fail('%s: image base not 0x%x' % (exe, BASE))
dirs = [struct.unpack_from('<II', b, opt + 96 + 8 * i) for i in range(16)]
secs = {}
for i in range(struct.unpack_from('<H', b, pe + 6)[0]):
	o = opt + struct.unpack_from('<H', b, pe + 20)[0] + i * 40
	name = b[o:o + 8].rstrip(b'\0').decode()
	vsize, va, raw, ptr = struct.unpack_from('<IIII', b, o + 8)
	secs[name] = (va, min(vsize, raw), ptr)


def off(rva):
	for va, n, ptr in secs.values():
		if va <= rva < va + n:
			return ptr + rva - va
	fail('rva %06x has no bytes in the file' % rva)


def u32(rva):
	return struct.unpack_from('<I', b, off(rva))[0]


# the pieces: the kept parts of .text and every section with bytes but .text and .reloc
text_va, text_n, _ = secs['.text']
pieces = [(lo, hi or text_va + text_n) for lo, hi in TEXT_KEEP]
for name, (va, n, ptr) in sorted(secs.items(), key=lambda s: s[1][0]):
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
rva, n = dirs[5]
end = rva + n
while rva < end:
	page, blk = struct.unpack_from('<II', b, off(rva))
	if blk < 8:
		break
	for i in range((blk - 8) // 2):
		e = struct.unpack_from('<H', b, off(rva + 8 + 2 * i))[0]
		at = page + (e & 0xfff)
		if e >> 12 != 3 or not emitted(at) or in_python(at):
			continue
		t = u32(at) - BASE
		if not in_python(t):
			continue
		if t not in PY_SYMBOLS:
			fail('the pointer at rva %06x leads to rva %06x of the original\'s Python, which has no real symbol'
				% (at, t))
		if not any(lo <= at and at + 4 <= hi for lo, hi in pieces):
			fail('the pointer at rva %06x crosses the end of a piece' % at)
		fix[at] = PY_SYMBOLS[t]
	rva += blk

# groups of pieces become initialized sections, the space between them uninitialized ones
groups = []
for lo, hi in pieces:
	if groups and lo - groups[-1][-1][1] < MERGE:
		groups[-1].append((lo, hi))
	else:
		groups.append([(lo, hi)])
if groups[0][0][0] != ROOM:
	fail('the first piece does not start at rva 0x%x' % ROOM)
sections = []	# (start, end, pieces or None)
for i, g in enumerate(groups):
	start = g[0][0] & ~0xfff
	sections.append((start, g[-1][1], g))
	nxt = groups[i + 1][0][0] & ~0xfff if i + 1 < len(groups) else SIZEOFIMAGE
	if align(g[-1][1]) < nxt:
		sections.append((align(g[-1][1]), nxt, None))
	elif align(g[-1][1]) > nxt:
		fail('the section of rva %06x runs into the next one' % start)

os.makedirs(outdir, exist_ok=True)
binpath = os.path.abspath(os.path.join(outdir, 'room.bin'))
blob = bytearray()
asm = []
for k, (start, stop, g) in enumerate(sections):
	if g is None:
		asm += ['.section .orig%d,"bw"' % k, '.space 0x%x' % (stop - start)]
		continue
	asm += ['.section .orig%d,"dw"' % k]
	if k == 0:
		asm += ['.globl _orig_room', '_orig_room:']
	at = start
	for lo, hi in g:
		if lo > at:
			asm.append('.zero 0x%x' % (lo - at))
		at = lo
		for p in sorted(f for f in fix if lo <= f < hi) + [hi]:
			if p > at:
				asm.append('.incbin "%s", 0x%x, 0x%x' % (binpath, len(blob) + at - lo, p - at))
			if p < hi:
				asm.append('.long _%s' % fix[p])
			at = p + 4
		blob += b[off(lo):off(lo) + hi - lo]
		at = hi
asm.append('.text')

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
	'room.c': ('/* %s */\n__asm__(\n%s);\n' % (gen, ''.join('\t"%s\\n"\n' % l.replace('"', '\\"') for l in asm)))
		.encode(),
	'room.ld': ('/* %s: the room of Scrap.exe at its rvas */\n%s' % (gen, ''.join(
		'  .orig%d __image_base__ + 0x%06x : { KEEP (*(.orig%d)) }\n' % (k, s[0], k) for k, s in enumerate(sections))))
		.encode(),
	'room.h': ('/* %s */\n#define ROOM_IMPORT_RVA 0x%06x\n#define ROOM_RESOURCE_RVA 0x%06x\n#define ROOM_RESOURCE_SIZE '
		'0x%x\n/* the code of the original that is not built in, int3 at start */\n#define ROOM_INT3 \\\n\t{ %s }\n'
		% (gen, dirs[1][0], dirs[2][0], dirs[2][1], ', '.join('{ 0x%06x, 0x%06x }' % r for r in int3))).encode(),
}
for name, data in files.items():
	open(os.path.join(outdir, name), 'wb').write(data)
print('datagen.py: %d sections, %d bytes of data, %d pointers into the Python lead to the real one'
	% (len(sections), len(blob), len(fix)))
