#!/usr/bin/env python3
# Moves the field accesses of a struct to new paths after a field was folded into an embedded struct: builds
# (cmake build dir build/), then rewrites each "'<struct>' has no member named 'x'" at its line and column by the
# map, until no such error is left. Type changes (a byte * field that is now a struct pointer) still need a look:
# byte arithmetic like *(float *)(p->field + 8) compiles silently with the new pointer size, tools/codecmp.sh
# shows it.
#
# usage: tools/fixmembers.py <root> <struct> <map file: "old new" per line, # comments>
import re, subprocess, sys, os
root, struct, mapfile = sys.argv[1:4]
m = dict(l.split() for l in open(mapfile) if l.strip() and not l.startswith('#'))
pat = re.compile(r"^(\S+?):(\d+):(\d+): error: '%s' has no member named '(\w+)'" % re.escape(struct))
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
		if g and g.group(4) in m:
			hits.setdefault(g.group(1), set()).add((int(g.group(2)), int(g.group(3)), g.group(4)))
	if not hits:
		left = [l for l in (r.stdout + r.stderr).splitlines() if ': error:' in l]
		print('round %d: done, %d other errors' % (rnd, len(set(left))))
		break
	n = 0
	for f, hs in hits.items():
		lines = open(f).read().split('\n')
		# right to left per line, so the columns stay valid
		for ln, col, name in sorted(hs, key=lambda h: (h[0], -h[1])):
			s = lines[ln - 1]
			i = byteidx(s, col)
			if i is not None:
				g = re.match(r'(->|\.)\s*', s[i:])
				i = i + g.end() if g else None
			if i is None or not re.match(r'%s\b' % name, s[i:]) or (i and re.match(r'\w', s[i - 1])):
				print('not found: %s:%d:%d %s' % (f, ln, col, name)); continue
			lines[ln - 1] = s[:i] + m[name] + s[i + len(name):]
			n += 1
		open(f, 'w').write('\n'.join(lines))
	print('round %d: %d replaced in %d files' % (rnd, n, len(hits)))
