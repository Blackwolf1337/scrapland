#!/usr/bin/env python3
# rvacheck.py: the C of ScrapC.exe computes no address from an rva of the original (docs/standalone.md, stage 3c1b).
# Run by the build after the link of ScrapC.exe. What the compiler checks itself (include/scrap/game.h): Game_Ptr
# exists in ScrapC.exe only for the test module (SD_GAME_RVA), GAME_DATA only for the slots of the import address
# table. This tool checks the rest in the sources:
#   - every GAME_FUNC(name, rva) names the C function that the hook at rva jumps to (src/hooks.inc, the CRT entries)
#   - game_base, the load address, appears only in the allowed places: the hook installer, the test module's rva
#     lookup, the room code of main.c and dataprobe.c (stage 3c removes it), Game_Init and game.h
#   - SD_ORIG, the fixed address of the room, is gone
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import ROOT, c_names

ALLOWED_BASE = {'include/scrap/game.h', 'src/core/game.c', 'src/core/hook.c', 'src/core/testmod.c',
	'src/standalone/main.c', 'src/standalone/dataprobe.c'}
NUMDEF = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+(0x[0-9a-fA-F]+)\b', re.M)
FUNC = re.compile(r'\bGAME_FUNC\(\s*(\w+)\s*,\s*(\w+)\s*\)')


def sources():
	for top in ('src', 'include'):
		for d, _, fs in os.walk(os.path.join(ROOT, top)):
			rel = os.path.relpath(d, ROOT)
			if rel.startswith('src/python'):
				continue
			for f in sorted(fs):
				if f.endswith(('.c', '.h', '.inc')):
					yield os.path.relpath(os.path.join(d, f), ROOT)


def macro_params(t, at):
	"""the parameters of the function-like define the text at t[at] lies in"""
	ls = t.rfind('\n', 0, at) + 1
	while ls > 1 and t[ls - 2] == '\\':
		ls = t.rfind('\n', 0, ls - 1) + 1
	m = re.match(r'[ \t]*#[ \t]*define[ \t]+\w+\(([^)]*)\)', t[ls:])
	return [p.strip() for p in m.group(1).split(',')] if m else []


def main():
	hooks = c_names()
	texts = {p: open(os.path.join(ROOT, p), encoding='utf-8').read() for p in sources()}
	numdefs = {}
	for t in texts.values():
		for n, v in NUMDEF.findall(t):
			numdefs.setdefault(n, set()).add(int(v, 16))
	bad = []
	funcs = 0
	for path, t in sorted(texts.items()):
		local = {n: int(v, 16) for n, v in NUMDEF.findall(t)}
		for m in FUNC.finditer(t):
			name, tok = m.groups()
			line = t.count('\n', 0, m.start()) + 1
			if tok in macro_params(t, m.start()):
				continue	# the parameters of a macro (game.h, StaticInit.c): its uses are checked
			if re.match(r'^0x[0-9a-fA-F]+$', tok):
				rva = int(tok, 16)
			elif tok in local:
				rva = local[tok]
			elif len(numdefs.get(tok, ())) == 1:
				rva = list(numdefs[tok])[0]
			else:
				bad.append('%s:%d: GAME_FUNC(%s, %s): the rva is no number' % (path, line, name, tok))
				continue
			funcs += 1
			if hooks.get(rva) != name:
				bad.append('%s:%d: GAME_FUNC(%s, %s): the hook at rva %06x jumps to %s' % (path, line, name, tok, rva,
					hooks.get(rva, 'no C function')))
		# the script functions of the dynamic initializers: INIT_NATIVE_FUNC(rva, fn, obj, name, func, cfunc)
		for m in re.finditer(r'^INIT_NATIVE_FUNC\(.*, (0x[0-9a-f]+), (\w+)\)$', t, re.M):
			funcs += 1
			rva, name = int(m.group(1), 16), m.group(2)
			if hooks.get(rva) != name:
				bad.append('%s:%d: %s for rva %06x, the hook there jumps to %s' % (path, t.count('\n', 0, m.start()) + 1,
					name, rva, hooks.get(rva, 'no C function')))
		if path not in ALLOWED_BASE:
			for m in re.finditer(r'\bgame_base\b', t):
				bad.append('%s:%d: game_base outside the allowed places' % (path, t.count('\n', 0, m.start()) + 1))
		for m in re.finditer(r'\bSD_ORIG\b', t):
			bad.append('%s:%d: SD_ORIG, an address of the room' % (path, t.count('\n', 0, m.start()) + 1))
	for b in bad[:30]:
		print('rvacheck.py: %s' % b, file=sys.stderr)
	if bad:
		sys.exit('rvacheck.py: %d uses of rvas in the C of ScrapC.exe (docs/standalone.md, stage 3c1b)' % len(bad))
	print('rvacheck.py: %d GAME_FUNC and script functions name their hooks, no rva used outside the allowed places'
		% funcs)


if __name__ == '__main__':
	main()
