# sdexe.py: what datagen.py, datamap.py and vtables.py read of Scrap.exe 1.6.1 and of the hook lists
# (docs/standalone.md, stage 3).
import os, re, struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
BASE = 0x400000


class ExeError(Exception):
	pass


class Exe:
	"""The PE image: its sections by name (va, size with bytes in the file, file offset, virtual size), the data
	directories and the rvas of its absolute pointers (.reloc, HIGHLOW)."""

	def __init__(self, path):
		try:
			b = open(path, 'rb').read()
		except OSError as e:
			raise ExeError('cannot read %s (%s): set SCRAP_EXE to Scrap.exe 1.6.1' % (path, e.strerror))
		if b[:2] != b'MZ' or len(b) < 0x40 or b[struct.unpack_from('<I', b, 0x3c)[0]:][:4] != b'PE\0\0':
			raise ExeError('%s: no PE file' % path)
		self.b = b
		pe = struct.unpack_from('<I', b, 0x3c)[0]
		opt = pe + 24
		self.stamp = struct.unpack_from('<I', b, pe + 8)[0]
		self.size = struct.unpack_from('<I', b, opt + 56)[0]
		self.base = struct.unpack_from('<I', b, opt + 28)[0]
		self.dirs = [struct.unpack_from('<II', b, opt + 96 + 8 * i) for i in range(16)]
		self.secs = {}
		for i in range(struct.unpack_from('<H', b, pe + 6)[0]):
			o = opt + struct.unpack_from('<H', b, pe + 20)[0] + i * 40
			name = b[o:o + 8].rstrip(b'\0').decode()
			vsize, va, raw, ptr = struct.unpack_from('<IIII', b, o + 8)
			self.secs[name] = (va, min(vsize, raw), ptr, vsize)
		self._relocs = None

	def off(self, rva):
		"""file offset of rva, None where the file has no bytes"""
		for va, n, ptr, vsize in self.secs.values():
			if va <= rva < va + n:
				return ptr + rva - va
		return None

	def section(self, rva):
		for name, (va, n, ptr, vsize) in self.secs.items():
			if va <= rva < va + max(n, vsize):
				return name
		return None

	def bytes(self, lo, hi):
		"""the bytes of lo..hi, zero where the file has none (the zero tail of a section)"""
		out = bytearray(hi - lo)
		for va, n, ptr, vsize in self.secs.values():
			a, z = max(lo, va), min(hi, va + n)
			if a < z:
				out[a - lo:z - lo] = self.b[ptr + a - va:ptr + z - va]
		return bytes(out)

	def u32(self, rva):
		o = self.off(rva)
		if o is None:
			raise ExeError('rva %06x has no bytes in the file' % rva)
		return struct.unpack_from('<I', self.b, o)[0]

	def relocs(self):
		"""the rvas of the absolute pointers"""
		if self._relocs is None:
			self._relocs = set()
			rva, n = self.dirs[5]
			end = rva + n
			while rva < end:
				page, blk = struct.unpack_from('<II', self.b, self.off(rva))
				if blk < 8:
					break
				for i in range((blk - 8) // 2):
					e = struct.unpack_from('<H', self.b, self.off(rva + 8 + 2 * i))[0]
					if e >> 12 == 3:
						self._relocs.add(page + (e & 0xfff))
				rva += blk
		return self._relocs

	def ptr(self, at):
		"""the rva a relocated pointer at rva at points to, None if at holds no pointer"""
		return self.u32(at) - BASE if at in self.relocs() else None


def c_names():
	"""the C function of ScrapC.exe for each rva of the original that jumps to one: the SD_HOOKs of src/hooks.inc and
	the entries of the original's CRT (the SD_IMPORT_CRT of src/imports.inc and src/standalone/crthooks.inc)"""
	names = {}
	pat = re.compile(r'^SD_(?:HOOK|IMPORT_CRT|IMPORT_CRT_STDCALL)\((\w+),(?:\s*\d+,)?\s*0x([0-9a-fA-F]+)', re.M)
	for f in ('src/hooks.inc', 'src/imports.inc', 'src/standalone/crthooks.inc'):
		for name, rva in pat.findall(open(os.path.join(ROOT, f)).read()):
			names.setdefault(int(rva, 16), name)
	return names


def header(name):
	return open(os.path.join(ROOT, 'include', 'scrap', name)).read()


def define(text, name):
	return int(re.search(r'#define %s (0x[0-9a-fA-F]+)' % name, text).group(1), 16)


def ranges(text, name):
	return [(int(lo, 16), int(hi, 16)) for lo, hi in
		re.findall(r'\{ (0x[0-9a-f]+), (0x[0-9a-f]+) \}', text[text.index('#define ' + name):].split('\n\n')[0])]


game_h, py_h = header('game.h'), header('origpython.h')
TIMESTAMP, SIZEOFIMAGE = define(game_h, 'GAME_TIMESTAMP'), define(game_h, 'GAME_SIZEOFIMAGE')
PY_CODE = (define(py_h, 'ORIG_PY_CODE_START'), define(py_h, 'ORIG_PY_CODE_END'))
PY_DATA = ranges(py_h, 'ORIG_PY_DATA')


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


def in_python(rva):
	return PY_CODE[0] <= rva < PY_CODE[1] or any(lo <= rva < hi for lo, hi in PY_DATA)


def room_pieces(x):
	"""the parts of the original with bytes that the room can hold: every section with bytes but .text, .reloc and
	.rsrc (the resources are src/standalone/scrap.rc since stage 3b3), sorted. Of .text nothing is built in since
	stage 3c1b (until then the data formats of dinput8.lib at 0x001000..0x002070, which ScrapC.exe takes from mingw's
	libdinput8 now, and until stage 3c1a the import thunks of the game in the CRT), it is int3 at start"""
	pieces = []
	for name, (va, n, ptr, vsize) in sorted(x.secs.items(), key=lambda s: s[1][0]):
		if name not in ('.text', '.reloc', '.rsrc') and n:
			pieces.append((va, va + n))
	pieces.sort()
	for (lo, hi), (lo2, hi2) in zip(pieces, pieces[1:]):
		if hi > lo2:
			raise ExeError('pieces %06x..%06x and %06x..%06x overlap' % (lo, hi, lo2, hi2))
	return pieces


class CObj:
	"""a section .sd.<rva> of an object file: one C object of SD_AT"""

	def __init__(self, path, name, rva, size, bss, alignment, data, relocs):
		self.path, self.name, self.rva, self.size, self.bss = path, name, rva, size, bss
		self.alignment, self.data, self.relocs = alignment, data, relocs
		self.lo, self.hi = rva, rva + size

	def where(self):
		return '%s (%s)' % (self.name, os.path.relpath(self.path))


def coff_sd(path, sd_symbols):
	"""the .sd.<rva> sections of a COFF object file, with their relocations as {offset: (symbol, target, addend)}:
	target is the rva of a target in a .sd section of the file (symbol value plus the addend in place), None for a
	global symbol of another section or file (a C function, the Python, an SD_AT object of another file) with the
	addend in place. The global symbols of the .sd sections go into sd_symbols with their rvas. A section of
	SD_BSS (uninitialized) is bss."""
	try:
		f = open(path, 'rb')
	except OSError as e:
		raise ExeError('cannot read %s (%s)' % (path, e.strerror))
	with f:
		hdr = f.read(20)
		if len(hdr) < 20:
			raise ExeError('%s: no COFF object' % path)
		machine, nsec, _, symptr, nsym, optsz, _ = struct.unpack('<HHIIIHH', hdr)
		if machine != 0x14c:
			raise ExeError('%s: not an i386 COFF object (machine %04x)' % (path, machine))
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
					raise ExeError('%s: section %s: SD_AT needs an rva number' % (path, name))
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
					raise ExeError('%s: %s: relocation type %d at +%x, only absolute pointers are placed' % (path, name, typ, at))
				sname, value, secnum, sclass, _ = symbol(symidx)
				inplace = struct.unpack_from('<I', data, at)[0]
				if secnum in sd:
					relocs[at] = (sname, sd[secnum] + value + inplace, 0)
				elif sclass != 2:
					raise ExeError('%s: %s: the pointer at +%x leads to %s, static C that has no rva of the original'
						% (path, name, at, sname))
				else:
					relocs[at] = (sname, None, inplace)
			out.append(CObj(path, name, sd[i + 1], size, bss, alignment, data, relocs))
		return out
