#!/usr/bin/env python3
# Static checks of the hooks against Scrap.exe, for mistakes the differential tests may not reach:
#   ret     the stack bytes the C definition pops (calling convention, arguments) against the ret of the original
#   calls   functions of src/hooks.inc and src/imports.inc the original calls but the C body never names
#   size    hooks of functions shorter than the 5 byte jmp
#   eax     hooks the C version declares void although callers in the exe read eax after the call (the original
#           left a pointer there that they use)
# The C side comes from the preprocessed sources (build/compile_commands.json, cmake must have run), the original
# from objdump -d of game/Bin/Scrap.exe. Output only for findings; "calls" has false positives (tables of
# function pointers, calls the C version leaves out on purpose).
#
# usage: tools/hookcheck.py [ret|calls|eax|size ...]   (default all)
import collections, glob, json, os, re, shlex, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
work = os.path.join(root, 'build', 'hookcheck')
os.makedirs(work, exist_ok=True)
checks = sys.argv[1:] or ['ret', 'calls', 'eax', 'size']

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
	if re.search(r'\bmatrix_t\b', p):
		return 64
	return 4

def popped(name):
	ret, params = protos[name]
	ps = [p for p in re.split(r',(?![^()]*\))', params) if p.strip()]
	if any('...' in p for p in ps):
		return 0
	skip = 1 if '__thiscall' in ret else 2 if '__fastcall' in ret else 0 if '__stdcall' in ret else None
	return 0 if skip is None else sum(argsize(p) for p in ps[skip:])

# the instructions after each call of an address, for the eax check
callers = collections.defaultdict(list)
for i, a in enumerate(addrs):
	m = re.match(r'call\s+0x([0-9a-f]+)$', code[a])
	if m:
		callers[int(m.group(1), 16) - 0x400000].append(i)

def reads_eax(i):
	# the first instruction after the call that touches eax: does it read it?
	for k in range(i + 1, min(i + 10, len(addrs))):
		x = code[addrs[k]]
		m = re.match(r'(\w+)\s*(.*)', x)
		op, args = m.group(1), m.group(2)
		ops = [o.strip() for o in re.split(r',(?![^\[]*\])', args)] if args else []
		if op in ('call', 'ret', 'jmp') or op.startswith('j'):
			return False
		if op == 'lahf':
			return False
		if not re.search(r'\b(eax|ax|al|ah)\b', args) or op in ('nop', 'xchg'):
			continue
		# writes that do not depend on the old value: or eax,-1, setcc al, imul eax,r,imm
		if (op == 'or' and ops[1] in ('0xffffffff', '-1')) or op.startswith('set') or (op == 'imul' and len(ops) == 3):
			return False
		if op in ('xor', 'sub') and len(ops) == 2 and ops[0] == ops[1]:
			return False
		dst, srcs = (ops[0], ops[1:]) if ops else ('', [])
		if op in ('mov', 'lea', 'movzx', 'movsx', 'pop', 'movd', 'setne', 'sete', 'cvttss2si', 'cvttsd2si') and \
			re.match(r'(eax|ax|al)$', dst):
			return any(re.search(r'\b(eax|ax|al|ah)\b', o) for o in srcs)
		return True
	return False

# the names in each body and the C functions it calls, once; a hook's names include those of everything it calls
idents = dict((n, set(re.findall(r'\w+', b))) for n, b in bodies.items())
calls = dict((n, set(c for c in re.findall(r'\b(\w+)\s*\(', b) if c in bodies)) for n, b in bodies.items())

def fullnames(name):
	seen, todo, names = {name}, [name], set()
	while todo:
		n = todo.pop()
		names |= idents.get(n, set())
		for c in calls.get(n, ()):
			if c not in seen:
				seen.add(c)
				todo.append(c)
	return names

for n, a, sz in hooks:
	# the hook writes a 5 byte jmp; hook.c skips shorter functions at runtime
	if 'size' in checks and int(sz) < 5:
		print('size  %-28s %s: only %s bytes, can not be hooked' % (n, a, sz))
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
	if 'eax' in checks and re.match(r'\s*(static\s+)?void\s*(__\w+\s*)?$', protos[n][0]):
		users = ['%x' % (addrs[i] + 0x400000) for i in callers.get(int(a, 16), []) if reads_eax(i)]
		if users:
			print('eax   %-28s %s: void in C, %d callers read eax (%s)' % (n, a, len(users), ' '.join(users[:4])))
	if 'calls' in checks:
		b = fullnames(n)
		missing = set()
		for x in ins:
			m = re.match(r'call\s+0x([0-9a-f]+)$', x)
			if m:
				names = byaddr.get(int(m.group(1), 16) - 0x400000)
				if names and names != {'CheckFailed'} and not names & b:
					missing.add('/'.join(sorted(names)))
		if missing:
			print('calls %-28s %s: %s' % (n, a, ', '.join(sorted(missing))))
