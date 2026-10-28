# ctypeinfo.py: the C types of the sources as GCC lays them out for ScrapC.exe (docs/standalone.md, stage 3b5). A probe
# file with the declarations to look at is compiled like scrapcode_sa (mingw-w64 i686, -g), its DWARF read back from
# objdump --dwarf=info: sizes, the members of structs and unions with their offsets, array lengths, the encoding of
# base types. globals.py reads the types of the GAME_GLOBAL defines and the lengths of tables this way.
import os, re, subprocess, tempfile

from sdexe import ROOT

CC = os.environ.get('CC_I686', 'i686-w64-mingw32-gcc')
OBJDUMP = os.environ.get('OBJDUMP', 'i686-w64-mingw32-objdump')

# the options of scrapcode_sa (CMakeLists.txt) that change types, and the debug info to read
CFLAGS = ['-DSD_STANDALONE', '-std=gnu2x', '-msse2', '-finput-charset=UTF-8', '-fexec-charset=CP1252',
	'-I' + os.path.join(ROOT, 'include'), '-I' + os.path.join(ROOT, 'src'),
	'-I' + os.path.join(ROOT, 'third_party/python-1.5.2/config'),
	'-I' + os.path.join(ROOT, 'third_party/python-1.5.2/Include'),
	'-O0', '-g', '-gdwarf-5', '-fno-merge-debug-strings', '-fno-eliminate-unused-debug-types', '-w']


class ProbeError(Exception):
	pass


class CType:
	"""a type: kind base, pointer, struct, union, array, enum, func or void; size in bytes; enc of a base type (float,
	signed, unsigned, bool, char); target of a pointer or the element of an array (None for void *); count of an array
	(None when it has no size); members of a struct or union [(name, offset, CType, bit size, bit offset)]; name the
	typedef or tag name it was reached by, const when it was qualified"""

	def __init__(self, kind, size=0):
		self.kind, self.size = kind, size
		self.enc = None
		self.target = None
		self.count = None
		self.members = []
		self.name = None
		self.const = False

	def __repr__(self):
		if self.kind == 'array':
			return '%r[%s]' % (self.target, self.count)
		if self.kind == 'pointer':
			return '%r *' % self.target
		return '%s %s(%d)' % (self.kind, self.name or '', self.size)

	def is_char(self):
		return self.kind == 'base' and self.size == 1 and self.enc in ('char', 'signed', 'unsigned')


def _parse(text):
	"""the DIEs of objdump --dwarf=info: {offset: (tag, attrs, children offsets)}"""
	dies = {}
	stack = []
	head = re.compile(r'^\s*<(\d+)><([0-9a-f]+)>: Abbrev Number: (\d+)(?: \((\w+)\))?')
	attr = re.compile(r'^\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$')
	cur = None
	for line in text.splitlines():
		m = head.match(line)
		if m:
			depth, off, tag = int(m.group(1)), int(m.group(2), 16), m.group(4)
			if tag is None:	# the end of the children
				cur = None
				continue
			del stack[depth:]
			cur = (tag, {}, [])
			dies[off] = cur
			if stack:
				stack[-1][2].append(off)
			stack.append(cur)
			continue
		m = attr.match(line)
		if m and cur is not None:
			cur[1][m.group(1)] = m.group(2).strip()
	return dies


def _ref(v):
	m = re.match(r'^<0x([0-9a-f]+)>', v)
	return int(m.group(1), 16) if m else None


def _num(v):
	m = re.match(r'^(-?(?:0x[0-9a-f]+|\d+))', v)
	return int(m.group(1), 0) if m else None


def _name(v):
	m = re.match(r'^\((?:indirect )?(?:line )?string(?:, offset: 0x[0-9a-f]+)?\): (.*)$', v)
	return m.group(1) if m else v


class _Types:
	def __init__(self, dies):
		self.dies = dies
		self.cache = {}

	def get(self, off):
		if off is None:
			return CType('void')
		if off in self.cache:
			return self.cache[off]
		tag, a, kids = self.dies[off]
		if tag in ('DW_TAG_typedef', 'DW_TAG_const_type', 'DW_TAG_volatile_type', 'DW_TAG_restrict_type',
				'DW_TAG_atomic_type'):
			base = self.get(_ref(a.get('DW_AT_type', '')))
			t = CType(base.kind, base.size)
			t.__dict__.update(base.__dict__)
			if tag == 'DW_TAG_typedef':
				t.name = _name(a['DW_AT_name'])
			elif tag == 'DW_TAG_const_type':
				t.const = True
			self.cache[off] = t
			return t
		if tag == 'DW_TAG_base_type':
			t = CType('base', _num(a['DW_AT_byte_size']))
			enc = a.get('DW_AT_encoding', '')
			t.enc = 'float' if '(float)' in enc else 'bool' if '(boolean)' in enc else \
				'unsigned' if '(unsigned' in enc else 'char' if '(signed char)' in enc else 'signed'
			t.name = _name(a.get('DW_AT_name', ''))
			if t.name in ('char',):
				t.enc = 'char'
		elif tag == 'DW_TAG_pointer_type':
			t = CType('pointer', _num(a.get('DW_AT_byte_size', '4')))
			self.cache[off] = t
			r = _ref(a.get('DW_AT_type', ''))
			t.target = self.get(r) if r is not None else None
			return t
		elif tag in ('DW_TAG_structure_type', 'DW_TAG_union_type'):
			t = CType('struct' if tag == 'DW_TAG_structure_type' else 'union', _num(a.get('DW_AT_byte_size', '0')) or 0)
			t.name = _name(a['DW_AT_name']) if 'DW_AT_name' in a else None
			self.cache[off] = t
			for k in kids:
				ktag, ka, _ = self.dies[k]
				if ktag != 'DW_TAG_member':
					continue
				mt = self.get(_ref(ka['DW_AT_type']))
				bits = _num(ka['DW_AT_bit_size']) if 'DW_AT_bit_size' in ka else None
				if bits is not None:
					bo = _num(ka['DW_AT_data_bit_offset'])
					t.members.append((_name(ka['DW_AT_name']) if 'DW_AT_name' in ka else None, bo // 8, mt, bits, bo % 8))
				else:
					t.members.append((_name(ka['DW_AT_name']) if 'DW_AT_name' in ka else None,
						_num(ka.get('DW_AT_data_member_location', '0')), mt, None, None))
			return t
		elif tag == 'DW_TAG_array_type':
			elem = self.get(_ref(a['DW_AT_type']))
			dims = []
			for k in kids:
				ktag, ka, _ = self.dies[k]
				if ktag == 'DW_TAG_subrange_type':
					if 'DW_AT_count' in ka:
						dims.append(_num(ka['DW_AT_count']))
					elif 'DW_AT_upper_bound' in ka:
						dims.append(_num(ka['DW_AT_upper_bound']) + 1)
					else:
						dims.append(None)
			t = elem
			for d in reversed(dims or [None]):
				arr = CType('array', t.size * d if d is not None else 0)
				arr.target, arr.count = t, d
				t = arr
		elif tag == 'DW_TAG_enumeration_type':
			t = CType('enum', _num(a.get('DW_AT_byte_size', '4')))
			t.enc = 'signed'
			t.name = _name(a['DW_AT_name']) if 'DW_AT_name' in a else None
		elif tag == 'DW_TAG_subroutine_type':
			t = CType('func', 1)
		else:
			raise ProbeError('DWARF tag %s at <0x%x> not known' % (tag, off))
		self.cache[off] = t
		return t


def probe(head, decls):
	"""compiles the C text head followed by the declarations decls (one per line) and returns ({variable name:
	CType} of its variables, {index in decls: the first error}); the lines with errors are left out"""
	bad = {}
	with tempfile.TemporaryDirectory() as tmp:
		c = os.path.join(tmp, 'probe.c')
		o = os.path.join(tmp, 'probe.o')
		first = head.count('\n') + 1
		for _ in range(8):
			lines = ['/* %d */' % i if i in bad else d for i, d in enumerate(decls)]
			open(c, 'w', encoding='utf-8').write(head + '\n' + '\n'.join(lines) + '\n')
			r = subprocess.run([CC] + CFLAGS + ['-c', c, '-o', o], capture_output=True, text=True)
			if not r.returncode:
				break
			new = 0
			for m in re.finditer(r'^.*probe\.c:(\d+):\d+: error: (.*)$', r.stderr, re.M):
				i = int(m.group(1)) - first - 1
				if 0 <= i < len(decls) and i not in bad:
					bad[i] = m.group(2)
					new += 1
			if not new:
				raise ProbeError('the probe does not compile:\n' + '\n'.join(r.stderr.splitlines()[:40]))
		else:
			raise ProbeError('the probe does not compile after 8 passes')
		text = subprocess.run([OBJDUMP, '--dwarf=info', o], capture_output=True, text=True, check=True).stdout
	dies = _parse(text)
	types = _Types(dies)
	out = {}
	for off, (tag, a, kids) in dies.items():
		if tag == 'DW_TAG_variable' and 'DW_AT_name' in a and 'DW_AT_type' in a:
			out[_name(a['DW_AT_name'])] = types.get(_ref(a['DW_AT_type']))
	return out, bad
