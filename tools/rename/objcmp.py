#!/usr/bin/env python3
"""objcmp.py <old build objects> <new build objects>: the proof of a rename (stage 4, docs/rename.md).

Compares the object files of two builds (the CMakeFiles/*.dir/**/*.obj of a copy of the build before the rename and
of the build after it) modulo the renames of tools/rename/names.tsv and paths.tsv:
  - the code: the disassembly of each object with its relocations, the old symbol names mapped to the new ones; in an
    instruction relocated against .rdata (a string literal, whose offset moves when a stringized name in a CHECK text
    changes length) the immediates are masked
  - the data: the relocations of .data and .rdata by symbol, in order, without their offsets
An object of a moved source is found by its new path. Prints the first difference of each object that differs and
exits 1 when one does. Needs i686-w64-mingw32-objdump.
"""
import multiprocessing, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OBJDUMP = 'i686-w64-mingw32-objdump'
TOK = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
RELOC = re.compile(r'(\s+[0-9a-f]+: \w+\t)(.*)$')


def table(path):
	m = {}
	if os.path.exists(path):
		for line in open(path, encoding='utf-8').read().splitlines()[1:]:
			if line:
				a, b = line.split('\t')[:2]
				m[a] = b
	return m


NAMES = table(os.path.join(HERE, 'names.tsv'))
PATHS = table(os.path.join(HERE, 'paths.tsv'))


def mapname(m):
	t = m.group(0)
	for pre in ('', '_', '__imp__', 'sd_obj_', 'sd_'):
		if t.startswith(pre) and t[len(pre):] in NAMES:
			return pre + NAMES[t[len(pre):]]
	return t


def dump(path, mapit):
	out = subprocess.run([OBJDUMP, '-dr', '--no-show-raw-insn', '-M', 'intel', path], capture_output=True,
		text=True).stdout
	res = []
	for l in out.splitlines():
		if 'file format' in l:
			continue
		if mapit:
			m = RELOC.match(l)
			if m:
				l = m.group(1) + TOK.sub(mapname, m.group(2))
			else:
				l = re.sub(r'<[^>]*>', lambda m: TOK.sub(mapname, m.group(0)), l)
		res.append(l)
	for i in range(len(res)):
		m = RELOC.match(res[i])
		if m and m.group(2).startswith('.rdata'):
			j = i - 1
			while j >= 0 and RELOC.match(res[j]):
				j -= 1
			if j >= 0:
				res[j] = re.sub(r'0x[0-9a-f]+', 'X', res[j])
	rel = subprocess.run([OBJDUMP, '-r', '-j', '.data', '-j', '.rdata', path], capture_output=True, text=True).stdout
	rl = []
	for l in rel.splitlines():
		if re.match(r'[0-9a-f]{8} ', l):
			if mapit:
				l = TOK.sub(mapname, l)
			parts = l.split()
			if not parts[-1].startswith('.'):
				rl.append(' '.join(parts[1:]))
	return res, rl


def new_rel(rel):
	m = re.match(r'(CMakeFiles/[^/]+\.dir/)(.*)\.obj$', rel)
	if m and m.group(2) in PATHS:
		return m.group(1) + PATHS[m.group(2)] + '.obj'
	return rel


def compare(args):
	old_dir, new_dir, rel = args
	n = os.path.join(new_dir, new_rel(rel))
	if not os.path.exists(n):
		n = os.path.join(new_dir, rel)	# not moved (yet)
	if not os.path.exists(n):
		return (rel, 'no object ' + n)
	d1, r1 = dump(os.path.join(old_dir, rel), True)
	d2, r2 = dump(n, False)
	for what, a, b in (('code', d1, d2), ('data relocation', r1, r2)):
		if a != b:
			for x, y in zip(a, b):
				if x != y:
					return (rel, '%s: %s | %s' % (what, x.strip(), y.strip()))
			return (rel, '%s: %d lines, %d lines' % (what, len(a), len(b)))
	return None


def main():
	if len(sys.argv) != 3:
		sys.exit(__doc__)
	old_dir, new_dir = sys.argv[1], sys.argv[2]
	rels = []
	for d, _, fs in os.walk(old_dir):
		for f in fs:
			if f.endswith('.obj'):
				rels.append(os.path.relpath(os.path.join(d, f), old_dir))
	rels.sort()
	with multiprocessing.Pool() as p:
		res = p.map(compare, [(old_dir, new_dir, r) for r in rels])
	bad = [r for r in res if r]
	for r in bad:
		print('objcmp.py: %s: %s' % r)
	print('objcmp.py: %d objects, %d differ' % (len(rels), len(bad)))
	sys.exit(1 if bad else 0)


if __name__ == '__main__':
	main()
