#!/usr/bin/env python3
# Static checks of the hooks against Scrap.exe, for mistakes the differential tests may not reach:
#   ret     the stack bytes the C definition pops (calling convention, arguments) against the ret of the original
#   calls   functions of src/hooks.inc and src/imports.inc the original calls but the C body never names
# The C side comes from the preprocessed sources (build/compile_commands.json, cmake must have run), the original
# from objdump -d of game/Bin/Scrap.exe. Output only for findings; "calls" has false positives (tables of
# function pointers, calls the C version leaves out on purpose).
#
# usage: tools/hookcheck.py [ret|calls ...]   (default both)
import collections, glob, json, os, re, shlex, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
work = os.path.join(root, 'build', 'hookcheck')
os.makedirs(work, exist_ok=True)
checks = sys.argv[1:] or ['ret', 'calls']

hooks = re.findall(r'^SD_HOOK\((\w+), (0x[0-9a-f]+), (\d+)\)', open(os.path.join(root, 'src/hooks.inc')).read(), re.M)
imports = dict((int(a, 16), n) for n, a in
	re.findall(r'^SD_IMPORT\((\w+), (0x[0-9a-f]+)\)', open(os.path.join(root, 'src/imports.inc')).read(), re.M))
byaddr = collections.defaultdict(set)
for n, a, sz in hooks:
	byaddr[int(a, 16)].add(n)
for a, n in imports.items():
	byaddr[a].add(n)

# the instructions of the exe by address (image base 0x400000)
exe = os.path.join(root, 'game', 'Bin', 'Scrap.exe')
dis = subprocess.run(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', exe], capture_output=True, text=True).stdout
code = {}
for line in dis.split('\n'):
	m = re.match(r'\s+([0-9a-f]+):\s+(.*)$', line)
	if m:
		code[int(m.group(1), 16) - 0x400000] = m.group(2)
addrs = sorted(code)

def instructions(rva, size):
	import bisect
	i = bisect.bisect_left(addrs, rva)
	while i < len(addrs) and addrs[i] < rva + size:
		yield code[addrs[i]]
		i += 1

# function bodies of the preprocessed C sources
bodies, protos = {}, {}
for e in json.load(open(os.path.join(root, 'build', 'compile_commands.json'))):
	if '/src/game/' not in e['file']:
		continue
	args, skip = [], False
	for x in e.get('arguments') or shlex.split(e['command']):
		if skip:
			skip = False
		elif x == '-o':
			skip = True
		elif x != '-c':
			args.append(x)
	out = os.path.join(work, os.path.basename(e['file']) + '.i')
	subprocess.run(args + ['-E', '-P', '-o', out], cwd=e['directory'], check=True)
	src = re.sub(r'__attribute__\(\(__(\w+)__\)\)', r'__\1', open(out).read())
	for m in re.finditer(r'^([^\n;{}()#]*?)\b(\w+)\s*\(([^;{}]*?)\)\s*\{', src, re.M):
		i, depth = m.end(), 1
		while depth and i < len(src):
			depth += {'{': 1, '}': -1}.get(src[i], 0)
			i += 1
		protos[m.group(2)] = (m.group(1), m.group(3))
		bodies[m.group(2)] = bodies.get(m.group(2), '') + src[m.end():i]

def argsize(p):
	p = p.strip()
	if p in ('void', '') or '*' in p:
		return 0 if p in ('void', '') else 4
	if re.search(r'\b(double|int64_t|uint64_t|long long)\b', p):
		return 8
	if re.search(r'\bvec3_t\b', p):
		return 12
	return 4

def popped(name):
	ret, params = protos[name]
	ps = [p for p in re.split(r',(?![^()]*\))', params) if p.strip()]
	if any('...' in p for p in ps):
		return 0
	skip = 1 if '__thiscall' in ret else 2 if '__fastcall' in ret else 0 if '__stdcall' in ret else None
	return 0 if skip is None else sum(argsize(p) for p in ps[skip:])

def fullbody(name, seen):
	seen.add(name)
	b = bodies.get(name, '')
	for c in set(re.findall(r'\b(\w+)\s*\(', b)):
		if c in bodies and c not in seen:
			b += fullbody(c, seen)
	return b

for n, a, sz in hooks:
	if n not in protos:
		continue
	ins = list(instructions(int(a, 16), int(sz)))
	if 'ret' in checks:
		rets = set()
		for x in ins:
			m = re.match(r'ret\s*(0x[0-9a-f]+)?$', x)
			if m:
				rets.add(int(m.group(1), 16) if m.group(1) else 0)
		if rets and rets != {popped(n)}:
			print('ret   %-28s %s: C pops %d, the original %s' % (n, a, popped(n), sorted(rets)))
	if 'calls' in checks:
		b = fullbody(n, set())
		missing = set()
		for x in ins:
			m = re.match(r'call\s+0x([0-9a-f]+)$', x)
			if m:
				names = byaddr.get(int(m.group(1), 16) - 0x400000)
				if names and names != {'CheckFailed'} and not any(re.search(r'\b%s\b' % k, b) for k in names):
					missing.add('/'.join(sorted(names)))
		if missing:
			print('calls %-28s %s: %s' % (n, a, ', '.join(sorted(missing))))
