#!/usr/bin/env python3
"""rename.py [-v] [--table] [--check] [-w] [--only names|paths|docs]: stage 4, the English names (docs/rename.md).

Translates the Spanish identifiers of our C (src/, include/), the names of the files and folders, and the uses of
both in the tests, the tools, the symbol tables and the docs, with english.py (glossary.tsv and overrides.tsv).

Without -w it is a dry run: it prints the counts of the plan and its problems (-v every rename of an identifier and
path) and exits 1 on a problem; --table writes the plan into names.tsv (old and new name of every identifier) and
paths.tsv (old and new path), added to the rows of earlier runs. -w applies the plan: it rewrites the files, moves
the files (git mv) and writes the tables. --only splits it: names the identifiers (not in the docs), paths the files
and the paths that name them (not in the docs), docs the docs. --check exits 1 when a file would still change or
move or a problem is found: after the rename it passes, the tool is rerunnable.

What is renamed:
  - the identifiers of the C of src/ and include/ that are ours: every identifier token outside the string literals
    but the names the external headers declare (the toolchain, Win32, DirectX, the CRT, the vendored Python 1.5.2:
    the headers the build read, from build/**/*.d); a member (x.m, x->m, a field, SD_OFFSET) can have an English of
    its own (overrides.tsv, member), a local of one function too (local)
  - in comments the identifiers that are renamed, but not the original's paths (Engine/Escena.cpp), quotes, the
    sections of the ini files ([niebla]), the quotes of string literals of the C (the doc strings of the script API
    above their functions) and the English words of overrides.tsv (prose)
  - the #include paths and the paths of our files anywhere (comments, CMakeLists.txt, tools, docs)
  - the symbols of renamed functions in the inline asm strings, the keys of the tables of the tests (testsyms.inc,
    testaddrs.inc)
  - in Python files (tools/test/py, tools/*): renamed C names inside strings and comments, where they are a whole
    string ('CEscena_ctor'), a name@rva key, or a C name with an underscore, a class prefix or a case change; not the
    names the scripts see (the methods, engine variables and natives of the exe), not in paths and file names
  - symbols/functions.tsv and globals.tsv: the names and signatures of the USER_DEFINED rows
  - tools/standalone/datamap.txt: the C names of the users
  - docs/*.md and README.md: renamed C names and paths

String literals of the C are never changed: module and method names of the script API, engine variable names, file
names of the packs and every text stay.

Problems (the dry run and --check exit 1):
  - two identifiers that become one name
  - two names of one name space that become one: the names at file scope of all files and of symbols/*.tsv, the
    names of each function or macro body (with its local overrides), the members of each struct (with the member
    overrides); a new name that is a macro (ours or of the external headers), a name at file scope that becomes one
    of the external headers
  - a new name that is a C keyword, or one that english() would change again (the plan must be idempotent)
  - two paths that become one, or a new path that exists
objcmp.py compares the object files of two builds modulo the renames: the proof that a rename changed only names.
"""

import collections, glob, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import ctok, english as en

NAMES = os.path.join(HERE, 'names.tsv')
PATHS = os.path.join(HERE, 'paths.tsv')

EXTERNAL_HEADERS = [
	'/usr/i686-w64-mingw32/include/**/*.h',
	'/usr/lib/gcc/i686-w64-mingw32/*/include/**/*.h',
	os.path.join(ROOT, 'third_party', '**', '*.[ch]'),
]
C_EXT = ('.c', '.h', '.inc')
# the names of our files that are no C (the module files of the build, the resources) stay
PATH_ROOTS = ('src', 'include')
# a comment that holds a string literal of the C of this many characters or more quotes it (the doc strings of the
# script API above their functions): the quote stays
QUOTE_MIN = 16
# the data of the original as C (stage 3b): its strings are names the scripts see only where named so
GENERATED_DATA = re.compile(r'^src/game/Global/(Strings[0-9a-f]+|Globals[0-9a-f]+|Vtables)\.c$')
SEEN_STRINGS = re.compile(r'\bstr_(?:PyMethod|GVar|GVarValue|NativeFunc)_\w+\[\d*\][^=\n]*= "((?:\\.|[^"\\])*)"')
# the generated tables of the tests, whose keys are C names in strings (SD_TEST_FUNC("Str_Trim", Str_Trim, 0x..))
TEST_TABLES = ('src/core/testsyms.inc', 'src/core/testaddrs.inc')
# tokens of the original's paths in comments: Engine/Escena.cpp, objeto3d.h of the original (not ours)
ORIG_PATH = re.compile(r'[A-Za-z0-9_./]*\.(?:cpp|hpp)\b')


def rel(p):
	return os.path.relpath(p, ROOT)


def read(path):
	with open(path, encoding='utf-8', errors='surrogateescape', newline='') as f:
		return f.read()


def write(path, text):
	with open(path, 'w', encoding='utf-8', errors='surrogateescape', newline='') as f:
		f.write(text)


def git_files():
	out = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True, check=True).stdout
	return [l for l in out.splitlines() if l]


def external_files():
	"""the headers (and the vendored Python sources) outside src/ and include/ that the build reads, from the
	dependency files of the build (build/**/*.d), else every header of the toolchain"""
	files = set()
	for d in glob.glob(os.path.join(ROOT, 'build', '**', '*.d'), recursive=True):
		for w in read(d).replace('\\\n', ' ').split():
			if w.endswith(('.h', '.c', '.inc')) and os.path.isabs(w):
				w = os.path.normpath(w)
				# build/ holds the codecmp worktree, a copy of our own sources
				if not w.startswith((os.path.join(ROOT, 'src') + '/', os.path.join(ROOT, 'include') + '/',
						os.path.join(ROOT, 'build') + '/')):
					files.add(w)
	if not files:
		for pat in EXTERNAL_HEADERS:
			files.update(glob.glob(pat, recursive=True))
	return sorted(files)


def external_names():
	"""the names the external headers declare: their file scope names, macros and members (not the parameter names
	of their prototypes and not their locals); and the macros alone"""
	names = set()
	macros = set()
	for f in external_files():
		try:
			t = open(f, encoding='latin-1').read()
		except OSError:
			continue
		sc = ctok.Scopes(t)
		names |= sc.globals | sc.macros | sc.members
		macros |= sc.macros
	return names, macros


class Plan:
	def __init__(self):
		self.tr = en.translator()
		self.files = git_files()
		self.cfiles = [f for f in self.files if f.startswith(PATH_ROOTS) and f.endswith(C_EXT)]
		self.problems = []
		self.ext, self.ext_macros = external_names()
		self.file_tokens = {}		# C file -> set of code identifiers
		self.scopes = {}		# C file -> ctok.Scopes
		self.c_strings = set()		# the names the scripts see: the methods, engine variables and natives
		self.literals = {}		# the longer string literals by their start, for the quotes of them in comments
		self.universe = collections.Counter()
		for f in self.cfiles:
			ids = set()
			for kind, s in ctok.ctokens(read(os.path.join(ROOT, f))):
				if kind == 'id':
					ids.add(s)
					self.universe[s] += 1
				elif kind == 'str' and f not in TEST_TABLES:
					body = s[1:-1]
					if len(body) >= QUOTE_MIN:
						self.literals.setdefault(body[:QUOTE_MIN], set()).add(body)
			self.file_tokens[f] = ids
			if GENERATED_DATA.match(f):
				# the strings of the original that the scripts see: the names of the methods, the engine variables and
				# the native functions (not the texts of the asserts)
				for m in SEEN_STRINGS.finditer(read(os.path.join(ROOT, f))):
					self.c_strings.update(ctok.WORD.findall(m.group(1)))
			self.scopes[f] = ctok.Scopes(read(os.path.join(ROOT, f)))
		self.sym_names = set()
		for name in self.symbol_names():
			self.sym_names.add(name)
		self.names = {}
		self.make_names()
		self.paths = {}
		self.make_paths()
		self.use_names = True
		self.use_paths = True

	# ---- identifiers ----

	def symbol_names(self):
		for row in self.tsv_rows('symbols/functions.tsv', 2, (4,)):
			yield row
		for row in self.tsv_rows('symbols/globals.tsv', 1, (2,)):
			yield row

	def tsv_rows(self, path, src_col, name_cols):
		lines = read(os.path.join(ROOT, path)).split('\n')
		for line in lines[1:]:
			c = line.split('\t')
			if len(c) > src_col and c[src_col] == 'USER_DEFINED':
				for k in name_cols:
					for w in ctok.WORD.findall(c[k]):
						yield w

	def ours(self, name):
		return name not in self.ext and name not in en.C_KEYWORDS

	def make_names(self):
		cands = set(self.universe) | self.sym_names
		for name in sorted(cands):
			if not self.ours(name):
				continue
			new = self.tr.english(name)
			if new != name:
				self.names[name] = new
		# problems
		by_new = collections.defaultdict(list)
		for old, new in self.names.items():
			by_new[new].append(old)
		for new, olds in sorted(by_new.items()):
			if len(olds) > 1:
				self.problem('names %s all become %s' % (', '.join(sorted(olds)), new))
		glob_names = set()
		macros = set(self.ext_macros)
		for sc in self.scopes.values():
			glob_names |= sc.globals
			macros |= sc.macros
		glob_names |= self.sym_names
		for old, new in sorted(self.names.items()):
			if new in en.C_KEYWORDS:
				self.problem('%s becomes the keyword %s' % (old, new))
			if self.tr.english(new) != new:
				self.problem('%s becomes %s, which would become %s' % (old, new, self.tr.english(new)))
			if new in macros and new not in self.names:
				self.problem('%s becomes %s, a macro' % (old, new))
			elif old in glob_names and new in self.ext:
				self.problem('%s becomes %s, a name of the external headers' % (old, new))
		# each name space must stay one to one: the names at file scope of all files, each function and macro body
		# (with its local overrides), each struct
		spaces = [('the names at file scope', glob_names, {})]
		for f, sc in sorted(self.scopes.items()):
			fnames = {id(b): name for name, _, _, b in sc.functions}
			for b in sc.bodies:
				fn = fnames.get(id(b))
				spaces.append(('%s of %s' % (fn or 'a macro', f), b, self.tr.locals.get((f, fn), {})))
			for r in sc.records:
				spaces.append(('a struct of ' + f, r, self.tr.members))
		for what, space, lm in spaces:
			new_of = {}
			for x in space:
				y = lm.get(x) or self.names.get(x, x)
				if y in new_of and new_of[y] != x:
					a_, b_ = sorted((new_of[y], x))
					if a_ in self.names or b_ in self.names or a_ in lm or b_ in lm:
						self.problem('%s and %s both become %s in %s' % (a_, b_, y, what))
				new_of[y] = x

	def problem(self, msg):
		self.problems.append(msg)

	# ---- paths ----

	def path_part(self, part):
		stem, dot, ext = part.partition('.')
		return self.tr.english(stem) + dot + ext

	def make_paths(self):
		for f in self.files:
			if not f.startswith(PATH_ROOTS):
				continue
			if f.startswith(('src/python/', 'src/standalone/res/')):
				continue
			parts = f.split('/')
			new = '/'.join(parts[:1] + [self.path_part(p) for p in parts[1:]])
			if f in self.tr.paths:
				new = self.tr.paths[f]
			if new != f:
				self.paths[f] = new
		existing = set(self.files)
		by_new = collections.defaultdict(list)
		for old, new in self.paths.items():
			by_new[new.lower()].append(old)
			if new in existing and new not in self.paths:
				self.problem('path %s becomes %s, which exists' % (old, new))
		for new, olds in by_new.items():
			if len(olds) > 1:
				self.problem('paths %s all become %s' % (', '.join(olds), new))

	def path_map(self):
		"""the renames of path strings: the files and the folders, longest first, also without src/ and include/ (the
		#include "scrap/x.h" form) and as bare file names"""
		m = {}
		if not self.use_paths:
			return m
		for old, new in self.paths.items():
			m[old] = new
			for root in ('src/', 'include/'):
				if old.startswith(root):
					m[old[len(root):]] = new[len(root):]
			o = old.split('/')
			n = new.split('/')
			for k in range(1, len(o)):
				m['/'.join(o[:k])] = '/'.join(n[:k])
		# the short forms of the comments and docs (Engine/Escena.c, Escena.c), where no other file has them
		if not hasattr(self, '_suffixes'):
			self._suffixes = collections.Counter()
			for f in set(self.files) | set(self.paths):
				parts = f.split('/')
				for k in (1, 2):
					if len(parts) > k:
						self._suffixes['/'.join(parts[-k:])] += 1
		for old, new in self.paths.items():
			o = old.split('/')
			n = new.split('/')
			for k in (1, 2):
				short = '/'.join(o[-k:])
				if self._suffixes[short] == 1 and short not in m:
					m[short] = '/'.join(n[-k:])
		return m

	# ---- rewriting ----

	def ident(self, s, member, local):
		"""the new name of an identifier token: as a member, in a function with local overrides, else the plan"""
		if not self.use_names:
			return s
		if member:
			return self.tr.members.get(s) or self.names.get(s, s)
		if local and s in local:
			return local[s]
		return self.names.get(s, s)

	def rename_c(self, text, f=None):
		out = []
		asm = 0
		sc = ctok.Scopes(text)
		spans = []
		for fn, a, b, _ in sc.functions:
			lm = self.tr.locals.get((f, fn))
			if lm:
				spans.append((a, b, lm))
		pos = 0
		for kind, s in ctok.ctokens(text):
			off = pos
			pos += len(s)
			if kind == 'id':
				local = None
				for a, b, lm in spans:
					if a <= off < b:
						local = lm
				out.append(self.ident(s, off in sc.member_offs, local))
				if s in ('__asm__', 'asm', '__asm'):
					asm = 1
				continue
			if kind == 'str':
				if asm:
					s = self.rename_asm(s)
				elif f in TEST_TABLES:
					s = self.rename_py_string(s)
				out.append(s)
				continue
			if kind == 'ws' and asm:
				if s == '(':
					asm += 1
				elif s == ')':
					asm -= 1
					if asm == 1:
						asm = 0
				elif s == ';' and asm == 1:
					asm = 0
			if kind == 'com':
				s = self.rename_text(s)
			elif kind == 'inc':
				s = self.rename_include(s)
			out.append(s)
		return ''.join(out)

	def rename_asm(self, s):
		"""the symbols of an asm string: _name, @name@8 (fastcall), name@8"""
		return re.sub(r'(\b_?|@)([A-Za-z][A-Za-z0-9_]*)(@\d+)?', lambda m: self.asm_sym(m), s)

	def asm_sym(self, m):
		whole = m.group(0)
		lead = ''
		name = m.group(2)
		if m.group(1) in ('_', '@'):
			lead = m.group(1)
		elif whole.startswith('_') and name not in self.names:
			lead = '_'
			name = whole[1:].split('@')[0]
		if name in self.names:
			return lead + self.names[name] + (m.group(3) or '')
		return whole

	def rename_include(self, s):
		pm = self.path_map()
		return pm.get(s, s)

	def rename_text(self, s, distinctive=False):
		"""identifiers and our paths in a comment or doc text, not in the original's paths and not in quotes"""
		pm = self.path_map_sorted()
		spans = []
		for m in ORIG_PATH.finditer(s):
			spans.append((m.start(), m.end()))
		for m in re.finditer(r'"[^"\n]*"', s):
			spans.append((m.start(), m.end()))
		# the sections of the ini files ([niebla] of m3d.ini)
		for m in re.finditer(r'\[[a-z_]+\]', s):
			spans.append((m.start(), m.end()))
		spans.extend(self.quotes(s))

		def inspan(a, b):
			for x, y in spans:
				if a >= x and b <= y:
					return True
			return False

		out = []
		pos = 0
		for m in re.finditer(r'[A-Za-z0-9_][A-Za-z0-9_./-]*', s):
			out.append(s[pos:m.start()])
			pos = m.end()
			w = m.group(0)
			if inspan(m.start(), m.end()):
				out.append(w)
				continue
			out.append(self.rename_word(w, pm, distinctive))
		out.append(s[pos:])
		return ''.join(out)

	def quotes(self, s):
		"""the spans of s that are string literals of the C"""
		out = []
		i = 0
		n = len(s)
		while i + QUOTE_MIN <= n:
			cands = self.literals.get(s[i:i + QUOTE_MIN])
			best = 0
			if cands:
				for lit in cands:
					if len(lit) > best and s.startswith(lit, i):
						best = len(lit)
			if best:
				out.append((i, i + best))
				i += best
			else:
				i += 1
		return out

	def path_map_sorted(self):
		if not hasattr(self, '_pms'):
			pm = self.path_map()
			self._pms = (pm, sorted(pm, key=len, reverse=True))
		return self._pms

	def rename_word(self, w, pm, distinctive):
		"""one word of a text: a path (of our files, possibly with a trailing dot or line suffix) or identifiers joined
		by dots and slashes"""
		pmap, keys = pm
		trail = ''
		core = w
		while core and core[-1] in '.-/':
			trail = core[-1] + trail
			core = core[:-1]
		if core in pmap:
			return pmap[core] + trail
		if '/' in core and core.split('/')[0] in ('src', 'include', 'scrap', 'game', 'core', 'standalone', 'lib'):
			for k in keys:
				if core.startswith(k + '/') or core == k:
					return pmap[k] + core[len(k):] + trail
		return re.sub(r'[A-Za-z_][A-Za-z0-9_]*', lambda m: self.rename_ident_in_text(m.group(0), distinctive), w)

	def distinct(self, name):
		"""a name that is surely a C name in a text: with an underscore, a class prefix or a case change (not the
		plain words nodo, Suelo, which may be words of the scripts)"""
		return '_' in name.strip('_') or re.search(r'[a-z0-9][A-Z]|^[CcI][A-Z][a-z]', name) is not None

	def rename_ident_in_text(self, name, distinctive):
		new = self.names.get(name) if self.use_names else None
		if new is None or (name in self.tr.prose and name.islower()):
			return name
		if distinctive and (not self.distinct(name) or name in self.c_strings):
			return name
		return new

	def rename_py(self, text, tests):
		out = []
		for kind, s in ctok.pytokens(text):
			if kind == 'str':
				s = self.rename_py_string(s)
			elif kind == 'com':
				s = self.rename_text(s, distinctive=True)
			out.append(s)
		return ''.join(out)

	def rename_py_string(self, s):
		m = re.match(r'^([rRbBuUfF]{0,2})(\'\'\'|"""|\'|")(.*)(\2)$', s, re.S)
		if not m:
			return s
		pre, q, body, _ = m.groups()
		key = body.split('@')[0]
		if self.use_names and key in self.names and re.match(r'^[A-Za-z_][A-Za-z0-9_]*(@[0-9a-f]+)?$', body):
			return pre + q + self.names[key] + body[len(key):] + q
		body = self.rename_text_py(body)
		return pre + q + body + q

	def rename_text_py(self, body):
		pm = self.path_map_sorted()[0]
		def sub(m):
			w = m.group(0)
			if w in pm:
				return pm[w]
			return w
		body = re.sub(r'(?:src|include)/[A-Za-z0-9_./]+', sub, body)
		def word(m):
			w = m.group(0)
			if re.search(r'[\\:]|\.[A-Za-z][A-Za-z0-9]{0,3}\b', w):
				return w	# a path or a file name: the packs and the files of the game
			return re.sub(r'[A-Za-z_][A-Za-z0-9_]*', lambda m: self.rename_ident_in_text(m.group(0), True), w)
		return re.sub(r'[A-Za-z0-9_./\\:-]+', word, body)

	def rename_docs(self, text):
		"""a doc: the old names and paths of all runs (names.tsv, paths.tsv), the docs may name what an earlier run
		renamed"""
		names, paths = self.names, self.paths
		self.names = dict(load_table(NAMES), **names)
		self.paths = dict(load_table(PATHS), **paths)
		for k in ('_pms', '_suffixes'):
			if hasattr(self, k):
				delattr(self, k)
		try:
			return self.rename_text(text)
		finally:
			self.names, self.paths = names, paths
			for k in ('_pms', '_suffixes'):
				if hasattr(self, k):
					delattr(self, k)

	def rename_tsv(self, text, src_col, cols):
		lines = text.split('\n')
		for i in range(1, len(lines)):
			c = lines[i].split('\t')
			if len(c) > src_col and c[src_col] == 'USER_DEFINED':
				for k in cols:
					if k < len(c):
						c[k] = re.sub(r'[A-Za-z_][A-Za-z0-9_]*', lambda m: self.names.get(m.group(0), m.group(0)), c[k])
				lines[i] = '\t'.join(c)
		return '\n'.join(lines)

	def rename_datamap(self, text):
		return re.sub(r'(?<![A-Za-z0-9_])[A-Za-z_][A-Za-z0-9_]*', lambda m: self.names.get(m.group(0), m.group(0)), text)

	def rename_cmake(self, text):
		pm, keys = self.path_map_sorted()
		def sub(m):
			w = m.group(0)
			for k in keys:
				if w == k or w.startswith(k + '/'):
					return pm[k] + w[len(k):]
			return w
		return re.sub(r'(?:src|include)/[A-Za-z0-9_./*]+', sub, text)

	def rewrites(self, only=None):
		"""(path, new text) of every file that changes; only: 'names' the identifiers but in the docs, 'paths' the
		paths but in the docs, 'docs' the docs (*.md)"""
		self.use_names = only != 'paths'
		self.use_paths = only != 'names'
		if hasattr(self, '_pms'):
			del self._pms
		for f in self.files:
			if (only == 'docs') != f.endswith('.md'):
				continue
			if only == 'paths' and f in ('symbols/functions.tsv', 'symbols/globals.tsv', 'tools/standalone/datamap.txt'):
				continue
			if only == 'names' and (f.endswith('CMakeLists.txt') or f.endswith('.sh')):
				continue
			p = os.path.join(ROOT, f)
			if not os.path.isfile(p) or os.path.islink(p):
				continue
			if f.startswith('tools/rename/') or f == 'docs/rename.md':
				continue
			if f in self.cfiles:
				t = read(p)
				n = self.rename_c(t, f)
			elif f.endswith('.py') and f.startswith('tools/'):
				t = read(p)
				n = self.rename_py(t, f.startswith('tools/test/py/'))
			elif f == 'symbols/functions.tsv':
				t = read(p)
				n = self.rename_tsv(t, 2, (4, 5))
			elif f == 'symbols/globals.tsv':
				t = read(p)
				n = self.rename_tsv(t, 1, (2, 3))
			elif f == 'tools/standalone/datamap.txt':
				t = read(p)
				n = self.rename_datamap(t)
			elif f.endswith('.md'):
				t = read(p)
				n = self.rename_docs(t)
			elif f.endswith('CMakeLists.txt') or f.endswith('.sh'):
				t = read(p)
				n = self.rename_cmake(t)
			else:
				continue
			if n != t:
				yield f, n


def load_table(path):
	m = {}
	if os.path.exists(path):
		for line in read(path).splitlines()[1:]:
			if line:
				a, b = line.split('\t')[:2]
				m[a] = b
	return m


def save_table(path, head, m):
	write(path, head + '\n' + ''.join('%s\t%s\n' % (a, m[a]) for a in sorted(m)))


def main():
	args = sys.argv[1:]
	wr = '-w' in args
	only = None
	if '--only' in args:
		only = args[args.index('--only') + 1]
		if only not in ('names', 'paths', 'docs'):
			sys.exit('rename.py: --only names, paths or docs')
	plan = Plan()
	for p in plan.problems:
		print('problem:', p)
	print('rename.py: %d identifiers in the plan, %d paths to move, %d problems' % (len(plan.names), len(plan.paths),
		len(plan.problems)))
	if '-v' in args:
		for old in sorted(plan.names):
			print('%s\t%s' % (old, plan.names[old]))
		for old in sorted(plan.paths):
			print('%s\t%s' % (old, plan.paths[old]))
	changes = list(plan.rewrites(only))
	moves = plan.paths if only in (None, 'paths') else {}
	if '--check' in args:
		for f, _ in changes:
			print('rename.py: would change', f)
		for old, new in sorted(moves.items()):
			print('rename.py: would move %s to %s' % (old, new))
		sys.exit(1 if plan.problems or changes or moves else 0)
	if '--table' in args:
		save_tables(plan)
	if not wr:
		print('rename.py: %d files would change, %d would move' % (len(changes), len(moves)))
		sys.exit(1 if plan.problems else 0)
	if plan.problems:
		sys.exit('rename.py: not applied, resolve the problems first')
	for f, n in changes:
		write(os.path.join(ROOT, f), n)
	for old, new in sorted(moves.items()):
		os.makedirs(os.path.dirname(os.path.join(ROOT, new)), exist_ok=True)
		subprocess.run(['git', 'mv', old, new], cwd=ROOT, check=True)
	save_tables(plan)
	print('rename.py: %d files rewritten, %d moved' % (len(changes), len(moves)))


def save_tables(plan):
	"""names.tsv and paths.tsv: the renames of this plan added to the ones of earlier runs"""
	names = load_table(NAMES)
	names.update(plan.names)
	save_table(NAMES, 'old\tnew', names)
	paths = load_table(PATHS)
	paths.update(plan.paths)
	save_table(PATHS, 'old\tnew', paths)


if __name__ == '__main__':
	main()
