#!/usr/bin/env python3
# uncovered.py [Scrap.exe]: bytes of the game and engine code (rva 0x05f6d8..0x2d6010) that are no int3 padding,
# not in a function of symbols/functions.tsv and not hooked: functions Ghidra missed, switch tables, and the tiny
# functions under 5 bytes that were never hooked (wipe=2 of docs/standalone.md turns all of them into int3).
import re, sys

EXE = sys.argv[1] if len(sys.argv) > 1 else 'game/Bin/Scrap.exe'
LO, HI = 0x05f6d8, 0x2d6010
b = open(EXE, 'rb').read()

def code(rva, n):
	o = rva - 0x1000 + 0x400	# .text: rva 0x1000 at file offset 0x400
	return b[o:o + n]

hooks = {}
for l in open('src/hooks.inc'):
	m = re.match(r'SD_HOOK\((\w+),\s*0x([0-9a-f]+),\s*(\d+)', l)
	if m:
		hooks[int(m.group(2), 16)] = int(m.group(3))
cov = bytearray(HI - LO)
tiny = []
for l in open('symbols/functions.tsv'):
	f = l.rstrip('\n').split('\t')
	try:
		a, sz = int(f[0], 16), int(f[1])
	except ValueError:
		continue
	if not LO <= a < HI:
		continue
	if a in hooks:
		sz = max(sz, hooks[a])
	elif sz < 5:
		tiny.append((a, sz, f[4] if len(f) > 4 else ''))
		continue
	for x in range(a, min(a + sz, HI)):
		cov[x - LO] = 1

print('tiny functions, not hooked: %d' % len(tiny))
for a, sz, n in tiny:
	print('  %06x %d %s %s' % (a, sz, code(a, sz).hex(), n))
runs = []
for i in range(HI - LO):
	if not cov[i] and code(LO + i, 1) != b'\xcc':
		if runs and runs[-1][1] == LO + i:
			runs[-1][1] += 1
		else:
			runs.append([LO + i, LO + i + 1])

def is_table(s, e):
	# dwords that point into .text (0x401000..0x78f097), after up to 3 bytes of nop alignment
	d = code(s, e - s)
	for skip in range(4):
		ds = d[skip:]
		if len(ds) >= 8 and all(0x401000 <= int.from_bytes(ds[i:i + 4], 'little') < 0x790000
			for i in range(0, len(ds) - len(ds) % 4, 4)):
			return True
	return False

code_runs = [r for r in runs if not is_table(*r)]
print('uncovered runs: %d (%d bytes), %d look like code' % (len(runs), sum(e - s for s, e in runs), len(code_runs)))
for s, e in code_runs:
	print('  %06x %4d %s' % (s, e - s, code(s, min(e - s, 16)).hex()))
