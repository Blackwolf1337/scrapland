#!/usr/bin/env python3
# initpat.py [-w] [-v] [Scrap.exe]: the dynamic initializers of the game's globals (rva 0x03e800..0x05f6d8, called
# by the CRT from its __xc table, docs/standalone.md stage 2d) and the atexit destructors they register (rva
# 0x38c720..0x38f097), sorted into patterns. Each function is disassembled (capstone), its instructions are matched
# against the templates below, and a match gives the parameters: the object, the strings, the values, the
# constructor and the destructor. The functions that fit no template have C by hand (HAND) or are listed.
#
# Without -w it prints the count per pattern and what fits none (-v: every function with its pattern).
# -w writes
#   src/game/Global/StaticInit.inc   one macro line per function (src/game/Global/StaticInit.c expands them)
#   src/hooks.inc                    the SD_HOOK lines below the marker of stage 2d (the block is rewritten)
#   symbols/functions.tsv            a row per function, source USER_DEFINED, 'void __cdecl(void)'
import collections, glob, os, re, struct, sys

from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_GRP_JUMP, X86_OP_IMM

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
args = [a for a in sys.argv[1:] if not a.startswith('-')]
WRITE = '-w' in sys.argv
VERBOSE = '-v' in sys.argv
EXE = args[0] if args else os.path.join(root, 'game/Bin/Scrap.exe')

BASE = 0x400000
INIT_LO, INIT_HI = 0x03e800, 0x05f6d8
DTOR_LO, DTOR_HI = 0x38c720, 0x38f097
XC_A, XC_Z = 0x3a15bc, 0x3a31bc
MARKER = '/* module agent: Standalone 2d, the dynamic initializers (hooks below this line) */'

# ---- the exe ----

img = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', img, 0x3c)[0]
nsec = struct.unpack_from('<H', img, pe + 6)[0]
optsz = struct.unpack_from('<H', img, pe + 20)[0]
sections = []
for i in range(nsec):
	o = pe + 24 + optsz + 40 * i
	vsize, va, rsize, raw = struct.unpack_from('<IIII', img, o + 8)
	sections.append((va, vsize, raw, rsize))

def read(rva, n):
	for va, vsize, raw, rsize in sections:
		if va <= rva < va + vsize:
			return img[raw + rva - va:raw + rva - va + n]
	raise ValueError('rva %06x not in the image' % rva)

def cstr(rva):
	d = read(rva, 512)
	return d[:d.index(b'\0')].decode('latin-1')

def f32(bits):
	return struct.unpack('<f', struct.pack('<I', bits & 0xffffffff))[0]

def float_literal(bits):
	"""the shortest decimal float literal that gives these bits, without exponent where that stays short"""
	v = f32(bits)
	if v != v or v in (float('inf'), float('-inf')):
		return None
	def exact(t):
		return struct.unpack('<I', struct.pack('<f', float(t)))[0] == bits & 0xffffffff
	s = next('%.*g' % (p, v) for p in range(1, 10) if exact('%.*g' % (p, v)))
	if 'e' in s and 1e-4 <= abs(v) < 1e9:
		s = next(t for t in ('%.*f' % (d, v) for d in range(0, 16)) if exact(t))
	if 'e' not in s and '.' not in s:
		s += '.0'
	return s + 'f'

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

def disasm(rva, lo, hi):
	"""the instructions of the function at rva by address: follows jumps inside [lo, hi), ends at ret and tail jumps"""
	seen, todo = {}, [rva]
	while todo:
		a = todo.pop()
		while a not in seen:
			ins = next(md.disasm(read(a, 16), BASE + a), None)
			if ins is None or ins.mnemonic == 'int3':
				raise ValueError('function %06x: no code at %06x' % (rva, a))
			seen[a] = ins
			if ins.mnemonic == 'ret':
				break
			if ins.group(X86_GRP_JUMP) and ins.operands[0].type == X86_OP_IMM:
				t = ins.operands[0].imm - BASE
				inside = lo <= t < hi and abs(t - rva) < 0x1000
				if ins.mnemonic == 'jmp':
					if not inside:
						break
					a = t
					continue
				todo.append(t)
			a += ins.size
	return [(a, (seen[a].mnemonic + ' ' + seen[a].op_str).strip(), seen[a].size) for a in sorted(seen)]

# ---- the C side: hooks with their names, names of globals in the C ----

def hook_names():
	names = {}
	files = [os.path.join(root, 'src/hooks.inc'), os.path.join(root, 'src/standalone/crthooks.inc')]
	for f in files:
		for m in re.finditer(r'^SD_HOOK\((\w+),\s*0x([0-9a-f]+)', open(f).read(), re.M):
			names.setdefault(int(m.group(2), 16), m.group(1))
	for m in re.finditer(r'^SD_IMPORT_CRT(?:_STDCALL)?\((\w+),(?:\s*\d+,)?\s*0x([0-9a-f]+)\)',
		open(os.path.join(root, 'src/imports.inc')).read(), re.M):
		names.setdefault(int(m.group(2), 16), m.group(1))
	return names

def global_names():
	"""#define g_Name ... Game_Ptr(0x...) or GAME_DATA(type, 0x...) of the C, by rva"""
	names = {}
	files = glob.glob(os.path.join(root, 'include/scrap/*.h')) + glob.glob(os.path.join(root, 'src/**/*.c'),
		recursive=True)
	pat = re.compile(r'^#define\s+(g_\w+)\s+.*?(?:Game_Ptr\(|GAME_DATA\([^,]+,\s*)0x([0-9a-f]+)\)+\s*(?:/\*.*)?$', re.M)
	for f in sorted(files):
		if f.endswith('StaticInit.c'):
			continue
		for m in pat.finditer(open(f).read()):
			names.setdefault(int(m.group(2), 16), m.group(1))
	return names

HOOKS = hook_names()
GLOBALS = global_names()

# the callees the templates name
GVAR_CTOR, CSTR_CTOR, CSTR_SET, CSTR_DTOR = 0x1319d0, 0x133980, 0x134040, 0x133990
PYF_CTOR, PYF_SETFUNC, PYF_DTOR, NATIVE_CTOR = 0x192170, 0x1929b0, 0x192200, 0x1921b0
ATEXIT, VEC_CTOR, VEC_DTOR, NEW_ARRAY, MEMSET, DELETE_ARRAY = 0x313a5c, 0x313a76, 0x313ae4, 0x313642, 0x319210, 0x31364b
DELETE_SIZED = 0x313634
CALLEES = dict(GVAR_CTOR=GVAR_CTOR, CSTR_CTOR=CSTR_CTOR, CSTR_SET=CSTR_SET, CSTR_DTOR=CSTR_DTOR, PYF_CTOR=PYF_CTOR,
	PYF_SETFUNC=PYF_SETFUNC, PYF_DTOR=PYF_DTOR, NATIVE_CTOR=NATIVE_CTOR, ATEXIT=ATEXIT, VEC_CTOR=VEC_CTOR,
	VEC_DTOR=VEC_DTOR, NEW_ARRAY=NEW_ARRAY, MEMSET=MEMSET, DELETE_ARRAY=DELETE_ARRAY, DELETE_SIZED=DELETE_SIZED)

# ---- the templates ----

# the SEH frame of MSVC around code that can throw (the C has no exceptions: no unwind state)
SEH_IN = ['push ebp', 'mov ebp, esp', 'push -1', 'push {_}', 'mov eax, dword ptr fs:[0]', 'push eax',
	'mov eax, dword ptr [{_}]', 'xor eax, ebp', 'push eax', 'lea eax, [ebp - 0xc]', 'mov dword ptr fs:[0], eax']
SEH_OUT = ['mov ecx, dword ptr [ebp - 0xc]', 'mov dword ptr fs:[0], ecx', 'pop ecx', 'mov esp, ebp', 'pop ebp', 'ret']
GVAR_HEAD = ['push {desc}', 'push {flags}', 'push {name}', 'mov ecx, {obj}', 'call $GVAR_CTOR']
FLOAT_IN = ['push ebp', 'mov ebp, esp', 'push ecx']
FLOAT_OUT = ['movss dword ptr [ebp - 4], xmm0', 'call $GVAR_CTOR', 'movss xmm0, dword ptr [ebp - 4]',
	'movss dword ptr [{value}], xmm0', 'movss dword ptr [{def}], xmm0', 'mov byte ptr [{type}], 4', 'mov esp, ebp',
	'pop ebp', 'ret']
FLOAT_PUSH = ['push {desc}', 'push {flags}', 'push {name}']

NUM = r'(-?(?:0x[0-9a-f]+|\d+))'

def compile_template(lines):
	out = []
	for l in lines:
		names = []
		def sub(m):
			if m.group(1) == '_':
				return r'(?:-?(?:0x[0-9a-f]+|\d+))'
			names.append(m.group(1))
			return NUM
		l = re.sub(r'\$(\w+)', lambda m: hex(BASE + CALLEES[m.group(1)]), l)
		rx = re.sub(r'\\\{(\w+)\\\}', sub, re.escape(l))
		out.append((re.compile(rx + '$'), names))
	return out

def match(tmpl, code):
	"""the captures of a template on the code (numbers, addresses as rvas), the addresses of the lines; None"""
	if len(tmpl) != len(code):
		return None
	cap = {}
	for (rx, names), (a, text, size) in zip(tmpl, code):
		m = rx.match(text)
		if not m:
			return None
		for n, v in zip(names, m.groups()):
			v = int(v, 0) & 0xffffffff
			if BASE <= v < BASE + 0x600000:
				v -= BASE
			if n in cap and cap[n] != v:
				return None
			cap[n] = v
	cap['@'] = [a for a, t, s in code]
	return cap

class Kind:
	def __init__(self, name, lines, check, emit):
		self.name, self.tmpl, self.check, self.emit = name, compile_template(lines), check, emit

def gvar_fields(c, type_):
	return c['value'] == c['obj'] + 0x10 and c['def'] == c['obj'] + 0x14 and c['type'] == c['obj'] + 0xd and \
		c.get('t', type_) == type_

def gvar_args(c):
	return '0x%06x, 0x%06x, %d, 0x%06x' % (c['obj'], c['name'], c['flags'], c['desc'])

def emit_gvar_num(c):
	if c['t'] == 2:
		return 'INIT_GVAR_INT', '%s, %d' % (gvar_args(c), c['v1'] - (1 << 32) if c['v1'] >= 1 << 31 else c['v1'])
	lit = float_literal(c['v1'])
	return 'INIT_GVAR_FLOAT', '%s, %s' % (gvar_args(c), lit)

INIT_KINDS = [
	# cGVar of int (type 2) or float (4) with a constant
	Kind('gvar number', GVAR_HEAD + ['mov dword ptr [{value}], {v1}', 'mov dword ptr [{def}], {v1}',
		'mov byte ptr [{type}], {t}', 'ret'],
		lambda c: gvar_fields(c, c['t']) and c['t'] in (2, 4) and (c['t'] == 2 or float_literal(c['v1'])),
		emit_gvar_num),
	# cGVar of float from a float of the data, also times a factor over a divisor, or plus a term
	Kind('gvar float of data', FLOAT_IN + ['movss xmm0, dword ptr [{src}]', 'mov ecx, {obj}'] + FLOAT_PUSH
		+ FLOAT_OUT, lambda c: gvar_fields(c, 4),
		lambda c: ('INIT_GVAR_FLOAT_DATA', '%s, 0x%06x' % (gvar_args(c), c['src']))),
	Kind('gvar float of data mul div', FLOAT_IN + ['movss xmm0, dword ptr [{src}]', 'mov ecx, {obj}',
		'mulss xmm0, dword ptr [{mul}]'] + FLOAT_PUSH + ['divss xmm0, dword ptr [{div}]'] + FLOAT_OUT,
		lambda c: gvar_fields(c, 4),
		lambda c: ('INIT_GVAR_FLOAT_MULDIV', '%s, 0x%06x, 0x%06x, 0x%06x' % (gvar_args(c), c['src'], c['mul'],
			c['div']))),
	Kind('gvar float of data add', FLOAT_IN + ['movss xmm0, dword ptr [{src}]', 'mov ecx, {obj}',
		'addss xmm0, dword ptr [{add}]'] + FLOAT_PUSH + FLOAT_OUT, lambda c: gvar_fields(c, 4),
		lambda c: ('INIT_GVAR_FLOAT_ADD', '%s, 0x%06x, 0x%06x' % (gvar_args(c), c['src'], c['add']))),
	# cGVar of a string: two cString (value, default), the destructor frees both
	Kind('gvar string', SEH_IN + GVAR_HEAD + ['mov ecx, {value}', 'call $CSTR_CTOR', 'mov ecx, {def}',
		'mov dword ptr [ebp - 4], 0', 'call $CSTR_CTOR', 'push {s}', 'mov ecx, {value}', 'mov byte ptr [ebp - 4], 1',
		'call $CSTR_SET', 'push {s}', 'mov ecx, {def}', 'call $CSTR_SET', 'push {fini}', 'mov byte ptr [{type}], 1',
		'call $ATEXIT', 'add esp, 4'] + SEH_OUT, lambda c: gvar_fields(c, 1),
		lambda c: ('INIT_GVAR_STRING', '%s, 0x%06x' % (gvar_args(c), c['s']))),
	# cGVar of a script function: a cPy_Function at +0x10 bound to a name (or none)
	Kind('gvar function', SEH_IN + GVAR_HEAD + ['mov ecx, {value}', 'call $PYF_CTOR', 'push {s}', 'mov ecx, {value}',
		'mov dword ptr [ebp - 4], 0', 'call $PYF_SETFUNC', 'push {fini}', 'mov byte ptr [{type}], 6', 'call $ATEXIT',
		'add esp, 4'] + SEH_OUT, lambda c: c['value'] == c['obj'] + 0x10 and c['type'] == c['obj'] + 0xd,
		lambda c: ('INIT_GVAR_FUNCTION', '%s, %s' % (gvar_args(c), '0x%06x' % c['s'] if c['s'] else '0'))),
	# an entry of the registry of C functions for scripts
	Kind('native function', ['push {func}', 'push {name}', 'mov ecx, {obj}', 'call $NATIVE_CTOR', 'ret'],
		lambda c: c['func'] in HOOKS, lambda c: ('INIT_NATIVE_FUNC', '0x%06x, 0x%06x, 0x%06x, %s' % (c['obj'], c['name'],
			c['func'], HOOKS[c['func']]))),
	# only the destructor of a global without constructor code
	Kind('atexit', ['push {fini}', 'call $ATEXIT', 'pop ecx', 'ret'], lambda c: True,
		lambda c: ('INIT_ATEXIT', '')),
	# an array of objects through the vector constructor iterator
	Kind('vector', ['push {dtor}', 'push {ctor}', 'push {count}', 'push {size}', 'push {obj}', 'call $VEC_CTOR',
		'push {fini}', 'call $ATEXIT', 'pop ecx', 'ret'], lambda c: c['ctor'] in HOOKS and c['dtor'] in HOOKS,
		lambda c: ('INIT_VECTOR', '0x%06x, %d, %d, %s, %s' % (c['obj'], c['size'], c['count'], HOOKS[c['ctor']],
			HOOKS[c['dtor']]))),
	# a constructor without arguments or with one to three, then the destructor
	Kind('object', ['mov ecx, {obj}', 'call {ctor}', 'push {fini}', 'call $ATEXIT', 'pop ecx', 'ret'],
		lambda c: c['ctor'] in HOOKS, lambda c: ('INIT_OBJECT', '0x%06x, %s' % (c['obj'], HOOKS[c['ctor']]))),
	Kind('object 1', ['push {a1}', 'mov ecx, {obj}', 'call {ctor}', 'push {fini}', 'call $ATEXIT', 'pop ecx', 'ret'],
		lambda c: c['ctor'] in HOOKS, lambda c: ('INIT_OBJECT_ARGS', '0x%06x, %s, %s' % (c['obj'],
			HOOKS[c['ctor']], arg(c['a1'])))),
	Kind('object 3', ['push {a3}', 'push {a2}', 'push {a1}', 'mov ecx, {obj}', 'call {ctor}', 'push {fini}',
		'call $ATEXIT', 'pop ecx', 'ret'], lambda c: c['ctor'] in HOOKS,
		lambda c: ('INIT_OBJECT_ARGS', '0x%06x, %s, %s, %s, %s' % (c['obj'], HOOKS[c['ctor']], arg(c['a1']),
			arg(c['a2']), arg(c['a3'])))),
	# a constructor without destructor, called or jumped to
	Kind('call', ['mov ecx, {obj}', 'jmp {ctor}'], lambda c: c['ctor'] in HOOKS,
		lambda c: ('INIT_CALL', '0x%06x, %s' % (c['obj'], HOOKS[c['ctor']]))),
	Kind('call 1', ['push {a1}', 'mov ecx, {obj}', 'call {ctor}', 'ret'], lambda c: c['ctor'] in HOOKS,
		lambda c: ('INIT_CALL_ARGS', '0x%06x, %s, %s' % (c['obj'], HOOKS[c['ctor']], arg(c['a1'])))),
	Kind('jump', ['jmp {ctor}'], lambda c: c['ctor'] in HOOKS, lambda c: ('INIT_JUMP', HOOKS[c['ctor']])),
	# an array of objects, the constructor called in a loop
	Kind('array', ['push esi', 'push edi', 'mov edi, {obj}', 'mov esi, {count}', 'nop dword ptr [eax]', 'mov ecx, edi',
		'call {ctor}', 'add edi, {size}', 'sub esi, 1', 'jne {loop}', 'pop edi', 'pop esi', 'ret'],
		lambda c: c['ctor'] in HOOKS and c['loop'] == c['@'][5],
		lambda c: ('INIT_ARRAY', '0x%06x, %d, %d, %s' % (c['obj'], c['size'], c['count'], HOOKS[c['ctor']]))),
	# a vector of three floats
	Kind('vector3', ['mov dword ptr [{obj}], {x}', 'mov dword ptr [{y_}], {y}', 'mov dword ptr [{z_}], {z}', 'ret'],
		lambda c: c['y_'] == c['obj'] + 4 and c['z_'] == c['obj'] + 8 and all(float_literal(c[k]) for k in 'xyz'),
		lambda c: ('INIT_VECTOR3', '0x%06x, %s, %s, %s' % (c['obj'], float_literal(c['x']), float_literal(c['y']),
			float_literal(c['z'])))),
	# the slots of a HashTable (its numSlots is data): new[], cleared, locked and caseSensitive as one word
	Kind('hash table', ['xor ecx, ecx', 'mov eax, {count}', 'mov edx, {size}', 'mul edx', 'seto cl', 'neg ecx',
		'or ecx, eax', 'push ecx', 'call $NEW_ARRAY', 'push {bytes}', 'push 0', 'push eax',
		'mov dword ptr [{slots}], eax', 'call $MEMSET', 'push {fini}', 'mov word ptr [{word}], {wv}', 'call $ATEXIT',
		'add esp, 0x14', 'ret'],
		lambda c: c['count'] * c['size'] < 1 << 31 and c['word'] == c['slots'] + 4,
		lambda c: ('INIT_HASHTABLE', '0x%06x, 0x%x, %d, 0x%x, %d, %d' % (c['slots'] - 4, c['count'], c['size'],
			c['bytes'], c['wv'] & 0xff, c['wv'] >> 8))),
]

def arg(v):
	"""an address of the exe as its string (the only one is the name of cVirtualKeyboard_Set), else the number"""
	return 'GAME_STR_AT(0x%06x)' % v if v >= 0x1000 else '%d' % v

DTOR_KINDS = [
	Kind('call', ['mov ecx, {obj}', 'jmp {func}'], lambda c: c['func'] in HOOKS,
		lambda c: ('FINI_CALL', '0x%06x, %s' % (c['obj'], HOOKS[c['func']]))),
	Kind('call seh', SEH_IN + ['mov ecx, {obj}', 'mov dword ptr [ebp - 4], 0', 'call {func}'] + SEH_OUT,
		lambda c: c['func'] in HOOKS, lambda c: ('FINI_CALL', '0x%06x, %s' % (c['obj'], HOOKS[c['func']]))),
	Kind('call 2', ['mov ecx, {o1}', 'call {f1}', 'mov ecx, {o2}', 'jmp {f2}'],
		lambda c: c['f1'] in HOOKS and c['f2'] in HOOKS,
		lambda c: ('FINI_GVAR_STRING', '0x%06x' % (c['o2'] - 0x10)) if c['f1'] == c['f2'] == CSTR_DTOR
			and c['o1'] == c['o2'] + 4 else
			('FINI_CALL2', '0x%06x, %s, 0x%06x, %s' % (c['o1'], HOOKS[c['f1']], c['o2'], HOOKS[c['f2']]))),
	Kind('vector', ['push {dtor}', 'push {count}', 'push {size}', 'push {obj}', 'call $VEC_DTOR', 'ret'],
		lambda c: c['dtor'] in HOOKS,
		lambda c: ('FINI_VECTOR', '0x%06x, %d, %d, %s' % (c['obj'], c['size'], c['count'], HOOKS[c['dtor']]))),
	# a DList: cleared by its function, then the free list deleted
	*[Kind('dlist', ['mov ecx, {obj}', 'call {func}', 'mov eax, dword ptr [{free}]', 'mov ecx, eax', test, 'je {end}',
		'mov eax, dword ptr [eax]', 'push {size}', 'push ecx', 'mov dword ptr [{free}], eax', 'call $DELETE_SIZED',
		'mov eax, dword ptr [{free}]', 'add esp, 8', 'mov ecx, eax', test, 'jne {loop}', 'mov dword ptr [{num}], 0',
		'ret'], lambda c: c['func'] in HOOKS and c['free'] == c['obj'] + 0xc and c['num'] == c['obj'] + 0x10
			and c['end'] == c['@'][-2] and c['loop'] == c['@'][6],
		lambda c: ('FINI_DLIST', '0x%06x, %s, %d' % (c['obj'], HOOKS[c['func']], c['size'])))
		for test in ('test eax, eax', 'test ecx, ecx')],
	# a CTable: delete[] of its elements, pointer and count cleared
	Kind('table', ['mov eax, dword ptr [{p}]', 'test eax, eax', 'je {end}', 'push eax', 'call $DELETE_ARRAY',
		'add esp, 4', 'mov dword ptr [{p}], 0', 'mov dword ptr [{obj}], 0', 'ret'],
		lambda c: c['end'] == c['@'][-1] and c['p'] == c['obj'] + 4,
		lambda c: ('FINI_TABLE', '0x%06x' % c['obj'])),
]

# by hand: rva -> C name (in the file of the class of the object)
HAND = {
	0x04b8b0: 'Swarm_InitAmmoCosts',
	0x04d8c0: 'Steam_InitTicketCallback', 0x38d860: 'Steam_FiniTicketCallback',
	0x04df30: 'PcPad_InitPads', 0x38da50: 'PcPad_FiniPads',
	0x051be0: 'ChatScorer_InitMsgs',
	0x0554d0: 'PyFunction_InitSphereFunc', 0x38e3a0: 'PyFunction_FiniSphereFunc',
	0x05b3b0: 'Misc_InitE3DDir',
	0x05bdc0: 'Render_InitMatrices',
	0x05d6f0: 'GroundPortal_InitPaintAll', 0x38ee30: 'GroundPortal_FiniPaintAll',
	0x05d940: 'CheckBBox_InitResponseFaces',
	0x05e6e0: 'Bsp_InitLights',
	0x38d950: 'ActionSet_FiniActionSets',
	0x38ec70: 'Material_FiniMatList',
	0x38ece0: 'Misc_FiniFileLog',
	0x38ee70: 'Texture_FiniTexList',
	# the destructors of function statics, registered by the C of their functions
	0x38dcb0: 'Taunts_FiniCounts', 0x38ed20: 'Light_FiniMat', 0x38edb0: 'Ground_FiniCollideMat', 0x38f010: 'Portal_FiniMat',
	0x38f040: 'RenderGround_FiniBatchTab', 0x38f070: 'Radiosity_FiniNormals',
}

# names for generated functions where the pattern gives none
NAMES = {
	0x05b3f0: 'Init_RSqrtTable',
	0x05b210: 'Init_MatList',
	0x05b360: 'Init_FileLog',
	0x05dcd0: 'Init_TexList',
}

# ---- naming ----

def ident(s):
	s = re.sub(r'[^0-9A-Za-z]+', '_', s).strip('_')
	return s or 'x'

def class_of(func):
	return re.sub(r'_(ctor\w*|dtor|Clear|Release|Reset|Free\w*|DeleteAll|Set)$', '', func)

def obj_name(rva):
	n = GLOBALS.get(rva)
	return n[2:] if n else '%06x' % rva

def class_name(func, obj):
	cls, o = class_of(func), obj_name(obj)
	if cls.startswith('Stub_') or o.lower().startswith(cls.lower()):
		return o
	return cls + '_' + o

def init_name(kind, c):
	if kind.startswith('gvar'):
		return 'GVar_' + ident(cstr(c['name']))
	if kind == 'native function':
		return 'NativeFunc_' + ident(cstr(c['name']))
	if 'ctor' in c and 'obj' in c:
		return class_name(HOOKS[c['ctor']], c['obj'])
	if kind == 'hash table':
		return obj_name(c['slots'] - 4)
	if kind == 'vector3':
		return obj_name(c['obj'])
	return None

# ---- the scan ----

def scan():
	xc = struct.unpack('<%dI' % ((XC_Z - XC_A) // 4), read(XC_A, XC_Z - XC_A))
	order = [x - BASE for x in xc if x and INIT_LO <= x - BASE < INIT_HI]
	inits, dtors = [], {}
	for rva in order:
		code = disasm(rva, INIT_LO, INIT_HI)
		rec = dict(rva=rva, size=code[-1][0] + code[-1][2] - rva, kind=None, code=code)
		for k in INIT_KINDS:
			c = match(k.tmpl, code)
			if c is not None and k.check(c):
				rec.update(kind=k.name, cap=c, macro=k.emit(c), base=init_name(k.name, c))
				break
		fini = [i for i, (a, t, s) in enumerate(code) if t == 'call ' + hex(BASE + ATEXIT)]
		for i in fini:
			m = re.match(r'push (0x[0-9a-f]+)$', [t for a, t, s in code[:i] if t.startswith('push ')][-1])
			f = int(m.group(1), 16) - BASE
			rec.setdefault('finis', []).append(f)
			dtors[f] = dict(rva=f, init=rva)
		inits.append(rec)
	# the destructors that game code registers for its function statics (push imm32 in the code outside the range)
	code_all = read(0x1000, DTOR_LO - 0x1000)
	for m in re.finditer(rb'\x68(?=(....))', code_all, re.S):
		v = struct.unpack('<I', m.group(1))[0] - BASE
		src = 0x1000 + m.start()
		if not DTOR_LO <= v < DTOR_HI or INIT_LO <= src < INIT_HI or v in dtors:
			continue
		# the push is followed by the call of atexit within three instructions
		ins = list(md.disasm(read(src, 32), BASE + src))[:4]
		if any(i.mnemonic == 'call' and i.op_str == hex(BASE + ATEXIT) for i in ins):
			dtors[v] = dict(rva=v, init=None, by=src)
	for f, rec in dtors.items():
		code = disasm(f, DTOR_LO, DTOR_HI)
		rec.update(size=code[-1][0] + code[-1][2] - f, kind=None, code=code)
		for k in DTOR_KINDS:
			c = match(k.tmpl, code)
			if c is not None and k.check(c):
				rec.update(kind=k.name, cap=c, macro=k.emit(c))
				break
	return inits, dtors

def dtor_base(d):
	c = d.get('cap')
	if not d.get('kind') or c.get('obj', c.get('o1')) is None:
		return None
	func = c.get('func') or c.get('dtor') or c.get('f1')
	if d['kind'] == 'table':
		return obj_name(c['obj'])
	return class_name(HOOKS[func], c.get('obj', c.get('o1')))

def name_all(inits, dtors):
	for r in inits:
		if r['kind'] == 'atexit':
			r['base'] = dtor_base(dtors[r['finis'][0]])
	used = collections.Counter()
	for r in inits:
		r['name'] = NAMES.get(r['rva']) or HAND.get(r['rva']) or \
			('Init_' + r['base'] if r.get('base') else 'Init_%06x' % r['rva'])
		used[r['name']] += 1
	for r in inits:
		if used[r['name']] > 1:
			r['name'] += '_%06x' % r['rva']
	byrva = dict((r['rva'], r) for r in inits)
	for f, d in dtors.items():
		init = byrva.get(d['init'])
		if f in NAMES or f in HAND:
			d['name'] = NAMES.get(f) or HAND[f]
		elif init and init.get('base') and init['rva'] not in HAND and len(init['finis']) == 1:
			d['name'] = 'Fini_' + init['name'][5:]
		elif dtor_base(d):
			d['name'] = 'Fini_' + dtor_base(d)
		else:
			d['name'] = 'Fini_%06x' % f

def coverage(inits, dtors):
	"""bytes of the two ranges that are no int3 padding and in no function"""
	gaps = []
	for lo, hi, funcs in ((INIT_LO, INIT_HI, inits), (DTOR_LO, DTOR_HI, dtors.values())):
		cov = bytearray(hi - lo)
		for r in funcs:
			for a, t, s in r['code']:
				if lo <= a < hi:
					cov[a - lo:a - lo + s] = b'\1' * s
		d = read(lo, hi - lo)
		i = 0
		while i < hi - lo:
			if not cov[i] and d[i] != 0xcc:
				j = i
				while j < hi - lo and not cov[j] and d[j] != 0xcc:
					j += 1
				gaps.append((lo + i, j - i))
				i = j
			else:
				i += 1
	return gaps

# ---- output ----

def inc_lines(inits, dtors):
	out = ['/*', ' * Generated by tools/standalone/initpat.py -w, do not edit: the dynamic initializers of the game in the order of',
		' * the __xc table of the CRT, each after the destructor it registers. Expanded by StaticInit.c.', ' */']
	done = set()
	for r in inits:
		for f in r.get('finis', []):
			d = dtors[f]
			if d.get('kind') and f not in HAND:
				out.append('%s(0x%06x, %s, %s)' % (d['macro'][0], f, d['name'], d['macro'][1]))
			done.add(f)
		if r.get('kind') and r['rva'] not in HAND:
			fini = dtors[r['finis'][0]]['name'] if r.get('finis') else ''
			a = r['macro'][1]
			if fini:
				a = fini + (', ' + a if a else '')
			out.append('%s(0x%06x, %s%s)' % (r['macro'][0], r['rva'], r['name'], ', ' + a if a else ''))
	rest = [d for f, d in sorted(dtors.items()) if f not in done and d.get('kind') and f not in HAND]
	if rest:
		out.append('')
		out.append('/* the destructors of function statics, registered by the game code */')
		for d in rest:
			out.append('%s(0x%06x, %s, %s)' % (d['macro'][0], d['rva'], d['name'], d['macro'][1]))
	return out

def write(inits, dtors):
	hooks = []
	for r in sorted(inits, key=lambda r: r['rva']):
		if r.get('kind') or r['rva'] in HAND:
			hooks.append('SD_HOOK(%s, 0x%06x, %d)' % (r['name'], r['rva'], r['size']))
	for f, d in sorted(dtors.items()):
		if d.get('kind') or f in HAND:
			hooks.append('SD_HOOK(%s, 0x%06x, %d)' % (d['name'], f, d['size']))
	open(os.path.join(root, 'src/game/Global/StaticInit.inc'), 'w').write('\n'.join(inc_lines(inits, dtors)) + '\n')
	# the hooks block: from the marker up to the next marker or the end
	path = os.path.join(root, 'src/hooks.inc')
	text = open(path).read()
	i = text.index(MARKER) + len(MARKER) + 1
	j = text.find('\n/* module agent:', i)
	j = len(text) if j < 0 else j + 1
	tail = text[j:]
	open(path, 'w').write(text[:i] + '\n'.join(hooks) + '\n' + ('\n' + tail if tail else ''))
	# functions.tsv: replace or add the rows of these rvas
	path = os.path.join(root, 'symbols/functions.tsv')
	lines = open(path).read().split('\n')
	head, rows = lines[0], [l for l in lines[1:] if l]
	mine = {}
	for r in list(inits) + list(dtors.values()):
		if r.get('kind') or r['rva'] in HAND:
			mine[r['rva']] = '%08x\t%d\tUSER_DEFINED\t__cdecl\t%s\tvoid __cdecl(void)' % (r['rva'], r['size'], r['name'])
	rows = [l for l in rows if int(l.split('\t')[0], 16) not in mine] + list(mine.values())
	rows.sort(key=lambda l: int(l.split('\t')[0], 16))
	open(path, 'w').write(head + '\n' + '\n'.join(rows) + '\n')

def main():
	inits, dtors = scan()
	name_all(inits, dtors)
	for title, funcs in (('initializers', inits), ('destructors', sorted(dtors.values(), key=lambda d: d['rva']))):
		count = collections.Counter(r['kind'] or ('by hand' if r['rva'] in HAND else None) for r in funcs)
		print('%s: %d' % (title, len(funcs)))
		for k, n in count.most_common():
			print('  %5d %s' % (n, k or 'no pattern'))
		for r in funcs:
			if VERBOSE or not r['kind'] and r['rva'] not in HAND:
				print('    %06x %4d %-24s %s' % (r['rva'], r['size'], r['kind'] or '-', r.get('name')))
	gaps = coverage(inits, dtors)
	for a, n in gaps:
		print('not in a function: %06x %d bytes' % (a, n))
	miss = [r for r in inits + list(dtors.values()) if not r['kind'] and r['rva'] not in HAND]
	if WRITE:
		write(inits, dtors)
		print('written: src/game/Global/StaticInit.inc, src/hooks.inc, symbols/functions.tsv')
	return 1 if miss or gaps else 0

if __name__ == '__main__':
	sys.exit(main())
