#!/usr/bin/env python3
"""Compares two traces of the deterministic test mode (SCRAP_TEST_DET_TRACE=<file>, include/scrap/testdet.h) and
shows where the two runs part: the lines around the first difference, the callers of crt_rand as function names.

  dettrace.py <trace A> <trace B> [ScrapC binary]

Each line is "<step> <what>": step (the clock went on), rand <caller> <state>, event <SDL event type>, command file,
level loaded. Two runs of one build on one case have equal traces. With the binary the callers are resolved with
addr2line (Linux build, started by the mode without address space randomization at 0x555555554000)."""
import subprocess
import sys

BASE = 0x555555554000


def name(line, exe):
	p = line.split()
	if exe and len(p) == 4 and p[1] == 'rand':
		out = subprocess.run(['addr2line', '-f', '-e', exe, hex(int(p[2], 16) - BASE)], capture_output=True, text=True)
		return '%s rand %s %s' % (p[0], out.stdout.split('\n')[0] or p[2], p[3])
	return line


def main():
	if len(sys.argv) < 3:
		sys.exit(__doc__)
	a, b = (open(p, errors='replace').read().split('\n') for p in sys.argv[1:3])
	exe = sys.argv[3] if len(sys.argv) > 3 else None
	i = 0
	while i < min(len(a), len(b)) and a[i] == b[i]:
		i += 1
	if i == len(a) == len(b):
		print('equal: %d lines' % i)
		return 0
	print('first difference at line %d (of %d and %d)' % (i + 1, len(a), len(b)))
	for k in range(max(i - 8, 0), i + 8):
		la, lb = (name(x[k], exe) if k < len(x) else '-' for x in (a, b))
		print('%s %-60s | %s' % ('  ' if la == lb else '!=', la, lb))
	return 1


if __name__ == '__main__':
	sys.exit(main())
