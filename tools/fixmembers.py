#!/usr/bin/env python3
# Moves the field accesses of a struct to new paths after a field was folded into an embedded struct: builds
# (cmake build dir build/), then rewrites each "'<struct>' has no member named 'x'" at its line and column by the
# map, until no such error is left. Type changes (a byte * field that is now a struct pointer) still need a look:
# byte arithmetic like *(float *)(p->field + 8) compiles silently with the new pointer size, tools/codecmp.sh
# shows it.
#
# usage: tools/fixmembers.py <root> <struct> <map file: "old new" per line, # comments>
#        tools/fixmembers.py <root> - <map file: "struct old new" per line> (several structs at once)
import re, subprocess, sys, os
root, struct, mapfile = sys.argv[1:4]
rows = [l.split() for l in open(mapfile) if l.strip() and not l.startswith('#')]
if struct == '-':
	m = {(r[0], r[1]): r[2] for r in rows}
	pat = re.compile(r"^(\S+?):(\d+):(\d+): error: '(?:struct )?(\w+)'(?: \{aka '(?:struct )?(\w+)'\})? has no member named '(\w+)'")
else:
	m = {(struct, r[0]): r[1] for r in rows}
	pat = re.compile(r"^(\S+?):(\d+):(\d+): error: '(?:struct )?(\w+)'(?: \{aka '(?:struct )?(\w+)'\})? has no member named '(\w+)'")
# gcc columns count a tab as up to 8 display columns
def byteidx(s, col):
	d = 1
	for i, c in enumerate(s):
		if d == col:
			return i
		d = (d - 1) // 8 * 8 + 9 if c == '\t' else d + 1
	return None

for rnd in range(10):
	r = subprocess.run(['cmake', '--build', 'build', '-j%d' % os.cpu_count(), '--', '-k'], cwd=root,
		capture_output=True, text=True, env=dict(os.environ, LC_ALL='C'))
	hits = {}
	for line in (r.stdout + r.stderr).splitlines():
		g = pat.match(line)
		# the typedef name or, with {aka}, the struct tag
		st = g and next((t for t in (g.group(4), g.group(5)) if t and (t, g.group(6)) in m), None)
		if st:
			hits.setdefault(g.group(1), set()).add((int(g.group(2)), int(g.group(3)), g.group(6), st))
	if not hits:
		left = [l for l in (r.stdout + r.stderr).splitlines() if ': error:' in l]
		print('round %d: done, %d other errors' % (rnd, len(set(left))))
		break
	n = 0
	for f, hs in hits.items():
		lines = open(f).read().split('\n')
		# right to left per line, so the columns stay valid
		for ln, col, name, st in sorted(hs, key=lambda h: (h[0], -h[1])):
			s = lines[ln - 1]
			i = byteidx(s, col)
			# at the -> or ., or at the name itself when gcc adds a "did you mean"
			if i is not None and not s[i:].startswith(name):
				g = re.match(r'(->|\.)\s*', s[i:])
				i = i + g.end() if g else None
			# a macro argument (offsetof): gcc points at the macro, the name must be unique in the line
			if (i is None or not re.match(r'%s\b' % name, s[i:])) and len(re.findall(r'\b%s\b' % name, s)) == 1:
				i = re.search(r'\b%s\b' % name, s).start()
			if i is None or not re.match(r'%s\b' % name, s[i:]) or (i and re.match(r'\w', s[i - 1])):
				print('not found: %s:%d:%d %s' % (f, ln, col, name)); continue
			lines[ln - 1] = s[:i] + m[(st, name)] + s[i + len(name):]
			n += 1
		open(f, 'w').write('\n'.join(lines))
	print('round %d: %d replaced in %d files' % (rnd, n, len(hits)))
	if not n:
		break
