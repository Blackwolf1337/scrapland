# Comparison of a C run against the pack runs of a case, block by block (a block is one case line and its reply).
# 1. Every reply is normalised (normalise.py) and cut into tokens (numbers, words, single characters).
# 2. Per block the distinct normalised pack replies (variants) are collected.
#    - One variant: the C reply must be equal; only a float (a number with a decimal point) may differ by
#      FLOAT_ABS + FLOAT_REL * magnitude (timers, positions and speeds a frame further).
#    - Several variants with the same number of tokens: a template is made place by place; a place where the variants
#      differ is a wildcard (any number when all variants have a number there, else any token). The C reply must
#      have the same number of tokens and match every other place (floats with the tolerance above).
#    - Variants with different numbers of tokens (lists of random length, scheduled calls that came or not): the
#      line is masked; it only has to agree on being an error reply ("error: ...") or not.
# 3. baselines/<case>.accepted.xz (optional, regress.py accept): a C run whose remaining differences were looked at
#    and accepted (error path probes like fc-nc-*, where the C raises at another point than the script); a line equal
#    to it passes.
# 4. masks/<case> (optional) holds lines "ignore <text>": a block whose case line contains <text> is not compared,
#    and "sub <regex> => <replacement>": applied to all replies of the case after the normaliser. Each line of a mask
#    file says why in a # comment above it.
import os
import re

from normalise import norm

HERE = os.path.dirname(os.path.abspath(__file__))
_TOK = re.compile(r"-?\d+(?:\.\d+)?(?:e[-+]?\d+)?|\w+|\s+|.", re.S)
_NUM = re.compile(r"-?\d+(?:\.\d+)?(?:e[-+]?\d+)?$")
_FLOAT = re.compile(r"-?\d+\.\d+(?:e[-+]?\d+)?$")
FLOAT_ABS = 0.1
FLOAT_REL = 0.01


class Template:
	"""The tokens of the pack variants (all of the same length), place by place literal (None), number ('n') or any
	token ('a')."""

	def __init__(self, base, wild):
		self.base, self.wild = base, wild

	def match(self, s):
		other = tokens(s)
		if len(other) != len(self.base):
			return False
		for k, want, got in zip(self.wild, self.base, other):
			if want == got or k == 'a':
				continue
			if k == 'n':
				if not _NUM.match(got):
					return False
				continue
			if not (_FLOAT.match(want) and _FLOAT.match(got)):
				return False
			a, b = float(want), float(got)
			if abs(a - b) > FLOAT_ABS + FLOAT_REL * max(abs(a), abs(b)):
				return False
		return True


def tokens(s):
	return _TOK.findall(s)


def load_mask(case):
	ignore, subs = [], []
	p = os.path.join(HERE, 'masks', case)
	if os.path.isfile(p):
		for line in open(p):
			line = line.rstrip('\n')
			if line.startswith('ignore '):
				ignore.append(line[7:])
			elif line.startswith('sub ') and ' => ' in line:
				a, b = line[4:].split(' => ', 1)
				subs.append((re.compile(a), b))
	return ignore, subs


def template(variants):
	"""A Template for variants of the same token count, or None when their token counts differ."""
	toks = [tokens(v) for v in variants]
	base = toks[0]
	if any(len(t) != len(base) for t in toks):
		return None
	wild = [None] * len(base)
	for other in toks[1:]:
		for i, (x, y) in enumerate(zip(base, other)):
			if x != y:
				wild[i] = 'n' if wild[i] in (None, 'n') and _NUM.match(x) and _NUM.match(y) else 'a'
	return Template(base, wild)


def load_json(path):
	"""A JSON file, xz compressed when its name ends in .xz."""
	import json
	if path.endswith('.xz'):
		import lzma
		with lzma.open(path, 'rb') as f:
			return json.loads(f.read().decode())
	return json.load(open(path))


def load_accepted(case):
	"""baselines/<case>.accepted.xz: a C run whose differences from the pack were accepted (regress.py accept), as
	block list, or None."""
	p = os.path.join(HERE, 'baselines', case + '.accepted.xz')
	if os.path.isfile(p):
		return load_json(p)['blocks']
	return None


def compare(case, packruns, crun):
	"""packruns: list of block lists, crun: block list; a block is [command, reply]. Returns (ok, diffs, masked)
	with diffs a list of (index, command, expected, got). A line that differs from the pack but equals the accepted
	C run of the case (same case line, same normalised reply) passes."""
	ignore, subs = load_mask(case)
	accepted = load_accepted(case)

	def nm(s):
		s = norm(s)
		for rx, rep in subs:
			s = rx.sub(rep, s)
		return s

	diffs = []
	masked = 0
	n = max([len(r) for r in packruns] + [len(crun)])
	for i in range(n):
		cmds = [r[i][0] for r in packruns if i < len(r)]
		cmd = cmds[0] if cmds else (crun[i][0] if i < len(crun) else '?')
		if any(x in cmd for x in ignore):
			continue
		variants = []
		for r in packruns:
			v = nm(r[i][1]) if i < len(r) else '<no block>'
			if v not in variants:
				variants.append(v)
		got = nm(crun[i][1]) if i < len(crun) else '<no block>'
		if len(variants) == 1:
			if got != variants[0] and not template(variants).match(got):
				diffs.append((i, cmd, variants[0], got))
			continue
		masked += 1
		t = template(variants)
		if t is None:
			errs = set(v.startswith('error:') for v in variants)
			if len(errs) == 1 and got.startswith('error:') not in errs:
				diffs.append((i, cmd, ' | '.join(variants), got))
		elif not t.match(got):
			diffs.append((i, cmd, ' | '.join(variants), got))
	if accepted is not None and diffs:
		keep = []
		for d in diffs:
			i = d[0]
			if i < len(accepted) and i < len(crun) and accepted[i][0] == crun[i][0] and \
					nm(accepted[i][1]) == nm(crun[i][1]):
				masked += 1
				continue
			keep.append(d)
		diffs = keep
	return not diffs, diffs, masked


def show_diff(exp, got, width=160):
	"""A short view of the first difference of two strings."""
	k = 0
	while k < min(len(exp), len(got)) and exp[k] == got[k]:
		k += 1
	a = max(0, k - 50)
	return 'P: %s\n      C: %s' % (exp[a:a + width].replace('\n', '\\n'), got[a:a + width].replace('\n', '\\n'))
