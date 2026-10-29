#!/usr/bin/env python3
# Names the hooks of one block of src/hooks.inc in symbols/functions.tsv: the calling convention and the Ghidra
# signature come from the C prototype in include/scrap/*.h (or the .c files). Runs tools/symname.py, 4 at a time.
# usage: tools/hooksym.py <block>   (block: the text of its marker comment, e.g. "Engine/Model" or "Model.c")
#        tools/hooksym.py -n <block>  only prints the symname arguments
import glob
import re
import subprocess
import sys

root = __file__.rsplit('/tools/', 1)[0]

def block_hooks(key):
	hooks, on = [], False
	for line in open(root + '/src/hooks.inc'):
		if line.startswith('/*'):
			on = key in line
			continue
		m = re.match(r'SD_HOOK\((\w+),\s*0x([0-9a-f]+)', line)
		if on and m:
			hooks.append((m.group(1), m.group(2)))
	return hooks

def prototypes():
	protos = {}
	files = glob.glob(root + '/include/scrap/*.h') + glob.glob(root + '/src/**/*.c', recursive=True)
	pat = re.compile(r'^([A-Za-z_][\w \*]*?)\s*\b(__thiscall|__fastcall|__stdcall)?\s*(\w+)\(([^;{]*)\)\s*[;{]',
		re.M)
	for f in files:
		src = re.sub(r'/\*.*?\*/', '', open(f).read(), flags=re.S)
		for m in pat.finditer(src):
			ret, cc, name, args = m.groups()
			if ret.strip() in ('return', 'else', 'if', 'static') or name in protos:
				continue
			protos[name] = (' '.join(ret.replace('static', '').replace('inline', '').split()), cc or '__cdecl',
				' '.join(args.split()))
	return protos

SCALAR = {'int': 'int', 'int32_t': 'int', 'uint32_t': 'uint', 'unsigned': 'uint', 'unsigned int': 'uint',
	'byte': 'byte', 'uint8_t': 'byte', 'char': 'char', 'int16_t': 'short', 'uint16_t': 'ushort', 'short': 'short',
	'float': 'float', 'double': 'double', 'void': 'void', 'HRESULT': 'int', 'int8_t': 'char'}

def gtype(t):
	t = ' '.join(t.replace('const ', '').replace('struct ', '').split())
	stars = t.count('*')
	base = t.replace('*', '').strip()
	if stars:
		if base == 'char' and stars == 1:
			return 'char *'
		return 'void ' + '*' * stars if base not in SCALAR or base == 'void' else SCALAR[base] + ' ' + '*' * stars
	return SCALAR.get(base, 'int')

def gsig(ret, cc, args):
	parts = [a.strip() for a in args.split(',') if a.strip() and a.strip() != 'void']
	if cc == '__thiscall' and parts:
		parts = parts[1:]
	out = []
	for p in parts:
		m = re.match(r'(.*?)(\w+)(\s*\[[^\]]*\])?$', p)
		t, n, arr = (m.group(1), m.group(2), m.group(3)) if m else (p, 'arg', None)
		if not t.strip():
			t, n = n, 'arg'
		out.append('%s %s' % (gtype(t + (' *' if arr else '')), n))
	return '%s %s(%s)' % (gtype(ret), cc, ', '.join(out) if out else 'void')

def main():
	args = sys.argv[1:]
	dry = args[:1] == ['-n']
	if dry:
		args = args[1:]
	if len(args) != 1:
		sys.exit(__doc__ or 'usage: tools/hooksym.py [-n] <block>')
	protos = prototypes()
	groups, missing = [], []
	for name, rva in block_hooks(args[0]):
		if name not in protos:
			missing.append(name)
			continue
		ret, cc, a = protos[name]
		groups.append([rva, name, cc, gsig(ret, cc, a)])
	seen = set()
	groups = [g for g in groups if not (g[0] in seen or seen.add(g[0]))]
	for i in range(0, len(groups), 4):
		cmd = []
		for g in groups[i:i + 4]:
			cmd += (['--'] if cmd else []) + g
		if dry:
			print(' '.join(repr(c) if ' ' in c else c for c in cmd))
		else:
			subprocess.run([root + '/tools/symname.py'] + cmd, check=True)
	for n in missing:
		print('no prototype:', n, file=sys.stderr)

main()
