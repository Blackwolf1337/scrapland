#!/usr/bin/env python3
# SDTest.Call(rva, sig, args...) in the test scripts: the number of arguments must match the signature
# (sig: optional 't', the return kind, then one letter an argument), and no global may be named out (the log file
# of sdlib). Prints the mismatches.
import re
import sys
import glob

def split_args(s):
	"""top level comma split of s up to the closing parenthesis; returns (args, rest) or None"""
	depth, cur, out, i, q = 0, '', [], 0, None
	while i < len(s):
		c = s[i]
		if q:
			cur += c
			if c == '\\':
				cur += s[i + 1]
				i += 1
			elif c == q:
				q = None
		elif c in '\'"':
			q = c
			cur += c
		elif c in '([{':
			depth += 1
			cur += c
		elif c in ')]}':
			if depth == 0:
				out.append(cur.strip())
				return out
			depth -= 1
			cur += c
		elif c == ',' and depth == 0:
			out.append(cur.strip())
			cur = ''
		else:
			cur += c
		i += 1
	return None

bad = 0
for path in sys.argv[1:] or sorted(glob.glob('tools/test/py/*.py')):
	src = open(path).read()
	if not path.endswith('sdlib.py'):
		for m in re.finditer(r'^out\s*=', src, re.M):
			print('%s:%d: a global named out hides the log file of sdlib' % (path, src.count('\n', 0, m.start()) + 1))
			bad += 1
	for m in re.finditer(r'SDTest\.Call\(', src):
		args = split_args(src[m.end():])
		if not args or len(args) < 2:
			continue
		sm = re.fullmatch(r"'([a-zA-Z@]*)'", args[1])
		if not sm or any(a.startswith('*') for a in args) or 'apply' in args[0]:
			continue
		sig = sm.group(1)
		want = len(sig) - 1 - (sig[:1] == 't')
		if len(args) - 2 != want:
			line = src.count('\n', 0, m.start()) + 1
			print('%s:%d: sig %r wants %d arguments, %d given' % (path, line, sig, want, len(args) - 2))
			bad += 1
sys.exit(1 if bad else 0)
