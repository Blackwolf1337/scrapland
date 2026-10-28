# sdexe.py: what datagen.py and vtables.py read of Scrap.exe 1.6.1 and of the hook lists (docs/standalone.md, stage 3).
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
