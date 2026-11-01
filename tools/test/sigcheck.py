#!/usr/bin/env python3
# The calls of the exe in the test scripts:
#   - SDTest.Call(rva or name, sig, args...): the number of arguments must match the signature (sig: optional 't', the
#     return kind, then one letter an argument)
#   - every name of the game (the first argument of SDTest.Call, Peek, Poke, ConvU, sym() and symva()) is in
#     src/core/testsyms.inc (kept by hand since the tag hooked-final)
#   - by name, the signature fits the C function: cdecl with or without 't' (this as the first argument), at least its
#     arguments when it takes '...'; fastcall with one argument as 't' (ecx), without arguments as plain; stdcall as 't'
#     with a dummy this (the callee pops like thiscall). A vec3_t by value is 3 words, a double or 64 bit int 2; a struct
#     of another type by value is not counted
#   - no global may be named out (the log file of sdlib), sym or symva (its functions)
# Prints the mismatches.
import re
import sys
import glob
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))


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


def read_table():
	"""{key: (kind, C name, rva)} of src/core/testsyms.inc"""
	out = {}
	text = open(os.path.join(ROOT, 'src', 'core', 'testsyms.inc')).read()
	for kind, key, name, rva in re.findall(r'^SD_TEST_(\w+)\("([^"]+)", (\w+), (?:\d+, )?(0x[0-9a-f]+)\)', text, re.M):
		out[key] = (kind, name, int(rva, 16))
	return out


PROTO = re.compile(r'(?:^|[;}\n])\s*((?:static\s+|inline\s+|extern\s+)*[\w\s\*]+?)\b(__thiscall|__fastcall|__stdcall|__cdecl|'
	r'WINAPI)?\s*\b(\w+)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*([;{])', re.M)


def prototypes(names):
	"""{name: (calling convention, parameters)} of the C functions names, the definition before a declaration"""
	out = {}
	paths = sorted(glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True) +
		glob.glob(os.path.join(ROOT, 'src', '**', '*.[ch]'), recursive=True))
	for path in paths:
		text = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', open(path, encoding='utf-8', errors='replace').read(), flags=re.S)
		for m in PROTO.finditer(text):
			name, typ, body = m.group(3), m.group(1), m.group(5) == '{'
			if name not in names or not re.search(r'\w', typ) or re.search(r'\b(return|else|case|goto|sizeof|do)\b', typ):
				continue
			if name in out and (out[name][2] or not body):
				continue
			out[name] = (m.group(2) or '__cdecl', m.group(4), body)
	return out


WORDS = {'vec3_t': 3, 'double': 2, 'int64_t': 2, 'uint64_t': 2}
ONE_WORD = ('int', 'uint', 'uint32_t', 'int32_t', 'float', 'byte', 'char', 'short', 'uint16_t', 'int16_t', 'uint8_t',
	'int8_t', 'bool', 'long', 'DWORD', 'HWND', 'size_t', 'uintptr_t', 'ushort', 'BOOL', 'UINT', 'WPARAM', 'LPARAM',
	'HANDLE', 'HINSTANCE', 'HDC', 'LONG', 'WORD')


def words(params):
	"""the 4 byte words of the parameters (None for a struct by value) and whether there is '...'"""
	params = re.sub(r'\([^()]*\)', '*', params).strip()	# a function pointer is a word
	n, varargs = 0, False
	for p in (params.split(',') if params not in ('', 'void') else []):
		p = p.strip()
		if p == '...':
			varargs = True
		elif '*' in p or '[' in p:
			n += 1
		else:
			t = re.sub(r'\b(const|struct|unsigned|signed|volatile)\b', '', p).split()
			t = t[0] if t else 'int'
			if t in WORDS:
				n += WORDS[t]
			elif t in ONE_WORD:
				n += 1
			else:
				return None, varargs
	return n, varargs


def fits(conv, params, thiscall, nargs):
	n, varargs = words(params)
	if conv == '__thiscall':
		return thiscall and (n is None or nargs == n or varargs and nargs >= n)
	if conv in ('__stdcall', 'WINAPI'):
		return thiscall and (n is None or nargs == n + 1) or n == 0 and nargs == 0
	if conv == '__fastcall':
		return n is None and thiscall or n == 0 and nargs == 0 or n == 1 and thiscall and nargs == 1
	# cdecl: 't' only says the first argument is this, the same call
	return n is None or nargs == n or varargs and nargs >= n


NAME_USE = re.compile(r"(?:SDTest\.(?:Call|Peek|Poke|ConvU|PeekSpan|PokeSpan)\(\s*|\bsym(?:va)?\(\s*)'([^']*)'")

bad = 0
table = read_table()
calls = []
for path in sys.argv[1:] or sorted(glob.glob(os.path.join(ROOT, 'tools/test/py/*.py'))):
	src = open(path).read()
	if not path.endswith('sdlib.py'):
		for m in re.finditer(r'^(out|sym|symva)\s*=', src, re.M):
			print('%s:%d: a global named %s hides %s of sdlib' % (path, src.count('\n', 0, m.start()) + 1, m.group(1),
				'the log file' if m.group(1) == 'out' else 'its function'))
			bad += 1
	for m in NAME_USE.finditer(src):
		if m.group(1) not in table:
			print('%s:%d: %s is not in src/core/testsyms.inc: run tools/test/testsyms.py -w' % (path,
				src.count('\n', 0, m.start()) + 1, m.group(1)))
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
		line = src.count('\n', 0, m.start()) + 1
		if len(args) - 2 != want:
			print('%s:%d: sig %r wants %d arguments, %d given' % (path, line, sig, want, len(args) - 2))
			bad += 1
		nm = re.fullmatch(r"'([^']*)'", args[0])
		if nm and nm.group(1) in table and table[nm.group(1)][0] in ('FUNC', 'CRT'):
			calls.append((path, line, table[nm.group(1)][1], sig))
protos = prototypes(set(c[2] for c in calls))
for path, line, name, sig in calls:
	if name not in protos:
		print('%s:%d: no C prototype of %s' % (path, line, name))
		bad += 1
		continue
	conv, params, _ = protos[name]
	if not fits(conv, params, sig[:1] == 't', len(sig) - 1 - (sig[:1] == 't')):
		print('%s:%d: sig %r does not fit %s %s(%s)' % (path, line, sig, conv, name, params))
		bad += 1
sys.exit(1 if bad else 0)
