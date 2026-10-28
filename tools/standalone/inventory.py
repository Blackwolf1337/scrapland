#!/usr/bin/env python3
# inventory.py [Scrap.exe]: what the C code still needs from Scrap.exe, for the standalone build.
# Code pointers in the data sections (from .reloc), the SD_IMPORTs and the imported DLL functions,
# each split into game code with a C hook, game code without one, and library code.
import re, struct, sys, collections

EXE = sys.argv[1] if len(sys.argv) > 1 else 'game/Bin/Scrap.exe'
GAME_LO, GAME_HI = 0x05f6d8, 0x2d6010	# game and engine code; below is Python 1.5.2, above the CRT and other libraries

b = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', b, 0x3c)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
opt = pe + 24
dirs = opt + 96
secs = []
for i in range(nsec):
	o = opt + struct.unpack_from('<H', b, pe + 20)[0] + i * 40
	name = b[o:o + 8].rstrip(b'\0').decode()
	vsz, va, rsz, rptr = struct.unpack_from('<IIII', b, o + 8)
	secs.append((name, va, vsz, rptr, rsz))

def off(rva):
	for n, va, vsz, rp, rs in secs:
		if va <= rva < va + max(vsz, rs):
			return rp + rva - va if rva - va < rs else None
	return None

def sec(rva):
	for n, va, vsz, rp, rs in secs:
		if va <= rva < va + max(vsz, rs):
			return n
	return '?'

text = next(s for s in secs if s[0] == '.text')
base = 0x400000

hooks = {}
for l in open('src/hooks.inc'):
	m = re.match(r'SD_HOOK\((\w+),\s*0x([0-9a-f]+)', l)
	if m:
		hooks[int(m.group(2), 16)] = m.group(1)
imports = {}
for l in open('src/imports.inc'):
	m = re.match(r'SD_IMPORT(?:_STDCALL)?\((\w+),(?:\s*\d+,)?\s*0x([0-9a-f]+)', l)
	if m:
		imports[int(m.group(2), 16)] = m.group(1)
names = {}
for l in open('symbols/functions.tsv'):
	f = l.rstrip('\n').split('\t')
	try:
		names[int(f[0], 16)] = f[4] if len(f) > 4 else ''
	except ValueError:
		pass

def kind(rva):
	if rva in hooks:
		return 'hooked'
	if GAME_LO <= rva < GAME_HI:
		return 'game, no C'
	return 'library'

# relocations: absolute pointers; keep the ones outside .text that point into .text
rrva, rsz = struct.unpack_from('<II', b, dirs + 5 * 8)
p, end = off(rrva), off(rrva) + rsz
codeptrs = collections.Counter()
datakinds = collections.Counter()
while p < end:
	page, blk = struct.unpack_from('<II', b, p)
	if blk == 0:
		break
	for i in range((blk - 8) // 2):
		e = struct.unpack_from('<H', b, p + 8 + 2 * i)[0]
		if e >> 12 != 3:
			continue
		at = page + (e & 0xfff)
		if sec(at) == '.text':
			continue
		o = off(at)
		if o is None:
			continue
		tgt = struct.unpack_from('<I', b, o)[0] - base
		if text[1] <= tgt < text[1] + text[2]:
			codeptrs[tgt] += 1
		else:
			datakinds[sec(tgt)] += 1
	p += blk

print('code pointers in data: %d places, %d targets' % (sum(codeptrs.values()), len(codeptrs)))
c = collections.Counter(kind(t) for t in codeptrs)
for k in ('hooked', 'game, no C', 'library'):
	print('  %-11s %5d targets' % (k, c[k]))
print('data pointers in data:', dict(datakinds))
print('SD_IMPORT: %d' % len(imports))
c = collections.Counter(kind(t) for t in imports)
for k in ('game, no C', 'library'):
	print('  %-11s %5d' % (k, c[k]))

# DLL imports
irva = struct.unpack_from('<I', b, dirs + 1 * 8)[0]
p = off(irva)
dlls = collections.Counter()
while True:
	oft, _, _, nm, ft = struct.unpack_from('<IIIII', b, p)
	if nm == 0:
		break
	dll = b[off(nm):b.index(b'\0', off(nm))].decode()
	q = off(oft or ft)
	while struct.unpack_from('<I', b, q)[0]:
		dlls[dll] += 1
		q += 4
	p += 20
print('DLL imports:', ', '.join('%s %d' % kv for kv in sorted(dlls.items())))

if '-v' in sys.argv:
	print('\ngame code pointed to from data, no C:')
	for t in sorted(t for t in codeptrs if kind(t) == 'game, no C'):
		print('  %06x %s (%d)' % (t, names.get(t, '?'), codeptrs[t]))
	print('\nSD_IMPORT of game code:')
	for t in sorted(t for t in imports if kind(t) == 'game, no C'):
		print('  %06x %s' % (t, imports[t]))
