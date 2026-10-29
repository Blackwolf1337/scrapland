"""Tokenizers for the rename: they split a source into identifiers, strings, comments and other text, so that the
rename touches identifiers (and the identifiers named in comments) and never the string literals."""

import re

# kinds: 'id' identifier, 'str' string or char literal, 'com' comment, 'inc' the path of an #include, 'num' a
# preprocessing number, 'ws' anything else (operators, spaces). The concatenation of the texts is the source.
_CTOKEN = re.compile(r'''
	(?P<incl>^[ \t]*\#[ \t]*include[ \t]*[<"][^>"\n]*[>"])
	|(?P<com>//[^\n]*|/\*.*?\*/)
	|(?P<str>(?:L|u8|u|U)?"(?:\\.|[^"\\\n])*"|(?:L|u|U)?'(?:\\.|[^'\\\n])*')
	|(?P<num>\.?[0-9](?:[eEpP][+-]|[0-9A-Za-z_.])*)
	|(?P<id>[A-Za-z_][A-Za-z0-9_]*)
	|(?P<ws>[ \t\r\f\v]+|\n|[^A-Za-z0-9_"'\s])
''', re.S | re.X | re.M)


def ctokens(text):
	pos = 0
	n = len(text)
	while pos < n:
		m = _CTOKEN.match(text, pos)
		if not m:
			yield ('ws', text[pos])
			pos += 1
			continue
		kind = m.lastgroup
		s = m.group(kind)
		if kind == 'incl':
			q = re.search(r'[<"]', s).start()
			yield ('ws', s[:q + 1])
			yield ('inc', s[q + 1:-1])
			yield ('ws', s[-1])
		else:
			yield (kind, s)
		pos = m.end()


# Python sources (the tests and the tools): comments, strings and identifiers
_PYTOKEN = re.compile(r'''
	(?P<com>\#[^\n]*)
	|(?P<str>[rRbBuUfF]{0,2}(?:"""(?:\\.|.)*?"""|\'\'\'(?:\\.|.)*?\'\'\'|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'))
	|(?P<num>[0-9][0-9A-Za-z_.]*)
	|(?P<id>[A-Za-z_][A-Za-z0-9_]*)
	|(?P<ws>[^A-Za-z0-9_"'\#]+)
''', re.S | re.X)


def pytokens(text):
	pos = 0
	n = len(text)
	while pos < n:
		m = _PYTOKEN.match(text, pos)
		if not m:
			yield ('ws', text[pos])
			pos += 1
			continue
		yield (m.lastgroup, m.group(m.lastgroup))
		pos = m.end()


WORD = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')

_RECORD = ('struct', 'union', 'enum')


class Tok(tuple):
	"""a token (kind, text) that knows its offset in the source"""

	def __new__(cls, kind, s, off):
		t = tuple.__new__(cls, (kind, s))
		t.off = off
		return t
_MEMBER_MACROS = ('SD_OFFSET', 'offsetof')


class Scopes:
	"""The name spaces of one C source, from its tokens (no preprocessing): the names at file scope (declarations,
	the macros and their uses at file scope), the names used in each function body and each macro body (not the
	members), and the members of each struct or union (members also from x.m, x->m and SD_OFFSET(type, m, off))."""

	def __init__(self, text):
		self.globals = set()
		self.macros = set()
		self.bodies = []	# sets of the names of a function or a macro (not the members)
		self.records = []	# sets of the member names of a struct or union
		self.members = set()	# names used as members anywhere
		self.params = set()	# parameter names of the prototypes at file scope
		self.functions = []	# (name, start offset, end offset, names) of each function body
		self.member_offs = set()	# the offsets of the tokens that are members
		toks = []
		self._offs = []
		pos = 0
		for t in ctokens(text):
			if t[0] != 'com':
				toks.append(Tok(t[0], t[1], pos))
				self._offs.append(pos)
			pos += len(t[1])
		self._offs.append(pos)
		self._scan(toks)

	def _scan(self, toks):
		i = 0
		n = len(toks)
		stmt = []		# the significant tokens of the statement at file scope
		stmt_start = 0
		while i < n:
			kind, s = toks[i]
			if kind == 'ws' and s == '#':
				i = self._directive(toks, i)
				continue
			if kind == 'ws' and s.strip() == '':
				i += 1
				continue
			if kind == 'ws' and s == '{':
				last = [t for t in stmt if t[0] in ('id', 'ws')]
				if self._init_head(last):
					i = self._block(toks, i, 'init')
				elif self._record_head(last):
					i = self._block(toks, i, 'record' if self._record_head(last) != 'enum' else 'enum')
				else:
					body = set()
					for t in stmt:
						if t[0] == 'id':
							body.add(t[1])
					start = stmt_start if stmt else i
					i = self._block(toks, i, 'func', body)
					self.globals.update(self._func_name(stmt))
					self.functions.append((self._func_ident(stmt), self._offs[start], self._offs[min(i, len(self._offs) - 1)],
						body))
					stmt = []
				continue
			if kind == 'ws' and s == ';':
				self._decl(stmt)
				stmt = []
				i += 1
				continue
			if not stmt:
				stmt_start = i
			stmt.append(toks[i])
			i += 1
		self._decl(stmt)

	@staticmethod
	def _init_head(last):
		"""an = at paren depth 0 before the {: the initializer of a variable"""
		depth = 0
		for k, t in enumerate(last):
			if t == ('ws', '('):
				depth += 1
			elif t == ('ws', ')'):
				depth -= 1
			elif t == ('ws', '=') and depth == 0 and k > 0 and last[k - 1][1] not in ('=', '!', '<', '>') and \
					(k + 1 == len(last) or last[k + 1] != ('ws', '=')):
				return True
		return False

	@staticmethod
	def _record_head(last):
		"""struct/union/enum when the statement before { opens a record body (struct X {, typedef union {)"""
		for k in range(len(last) - 1, -1, -1):
			t = last[k]
			if t[0] == 'ws' and t[1] in (')', ']'):
				return None
			if t[0] == 'id' and t[1] in _RECORD:
				return t[1]
		return None

	@staticmethod
	def _func_ident(stmt):
		"""the name of a function from its head: the last name before the first ( at depth 0"""
		name = None
		for t in stmt:
			if t == ('ws', '('):
				break
			if t[0] == 'id':
				name = t[1]
		return name

	def _func_name(self, stmt):
		names = set()
		depth = 0
		for t in stmt:
			if t == ('ws', '('):
				depth += 1
			elif t == ('ws', ')'):
				depth -= 1
			elif t[0] == 'id' and depth == 0:
				names.add(t[1])
		return names

	def _decl(self, stmt):
		"""a declaration at file scope: its names outside parentheses are globals (types, the declared names); in
		parentheses the last name of each comma group of two or more names is a parameter name (CNodo3D *padre), the
		others are globals (types, the arguments of a macro)"""
		sig = [t for t in stmt if not (t[0] == 'ws' and t[1].strip() == '')]
		if len(sig) >= 4 and sig[0] == ('id', 'SD_OFFSET') and sig[2][0] == 'id' and sig[4][0] == 'id':
			self.globals.add(sig[2][1])
			self._add_member(sig[4])
			return
		prev = None
		depth = 0
		group = []
		star = False
		for t in stmt:
			if t[0] == 'id':
				if prev in ('.', '->'):
					self._add_member(t)
				elif depth == 0:
					self.globals.add(t[1])
				else:
					group.append(t[1])
			elif t[0] == 'ws':
				s = t[1]
				if s == '(':
					if depth > 0:
						self.globals.update(group)
					group = []
					star = False
					depth += 1
				elif s == '*' and all(g.startswith('__') or g.isupper() for g in group):
					star = True	# (*name): a declarator, the name is global (a function pointer or its typedef)
				elif s in (',', ')') and depth > 0:
					if len(group) >= 2 and not star:
						self.params.add(group[-1])
						self.globals.update(group[:-1])
					else:
						self.globals.update(group)
					group = []
					star = False
					if s == ')':
						depth -= 1
			if t[0] == 'ws':
				prev = '->' if (prev == '-' and t[1] == '>') else t[1]
			else:
				prev = None
		self.globals.update(group)

	def _directive(self, toks, i):
		"""a preprocessor line (with its continuations): a #define name is a macro, its body a scope of its own"""
		n = len(toks)
		line = []
		while i < n:
			kind, s = toks[i]
			if kind == 'ws' and s == '\n':
				if line and line[-1] == ('ws', '\\'):
					line.pop()
					i += 1
					continue
				break
			line.append(toks[i])
			i += 1
		sig = [t for t in line if not (t[0] == 'ws' and t[1].strip() == '')]
		if len(sig) >= 3 and sig[1] == ('id', 'define') and sig[2][0] == 'id':
			self.macros.add(sig[2][1])
			self.globals.add(sig[2][1])
			body = set()
			self._names(sig[3:], body)
			self.bodies.append(body)
		return i

	def _names(self, toks, body, record=None):
		prev = ''
		after_member_macro = 0
		depth_mm = 0
		member_at = set()
		for k, t in enumerate(toks):
			if t[0] == 'id':
				if prev in ('.', '->') or k in member_at:
					self._add_member(t)
					if record is not None:
						record.add(t[1])
				else:
					body.add(t[1])
				if t[1] in _MEMBER_MACROS:
					after_member_macro = 1
					depth_mm = 0
				prev = ''
				continue
			if t[0] != 'ws' or t[1].strip() == '':
				prev = ''
				continue
			s = t[1]
			if after_member_macro:
				if s == '(':
					depth_mm += 1
				elif s == ')':
					depth_mm -= 1
					if depth_mm == 0:
						after_member_macro = 0
				elif s == ',' and depth_mm == 1:
					# the member after the first comma of SD_OFFSET(type, member, off) / offsetof(type, member)
					for k2 in range(k + 1, len(toks)):
						t2 = toks[k2]
						if t2[0] == 'id':
							member_at.add(k2)
							break
						if t2[0] == 'ws' and t2[1].strip():
							break
					after_member_macro = 0
			prev = '->' if (prev == '-' and s == '>') else s

	def _block(self, toks, i, kind, body=None):
		"""the tokens from { to the matching }; returns the index after it"""
		n = len(toks)
		depth = 0
		inner = []
		j = i
		while j < n:
			t = toks[j]
			if t == ('ws', '{'):
				depth += 1
			elif t == ('ws', '}'):
				depth -= 1
				if depth == 0:
					break
			if t[0] == 'ws' and t[1] == '#':
				# a directive inside a body (#if, #define in a function)
				j2 = self._directive(toks, j)
				j = j2
				continue
			inner.append(t)
			j += 1
		if kind == 'func':
			self._names(inner, body)
			self.bodies.append(body)
		elif kind == 'record':
			rec = set()
			self._record(inner[1:], rec)
			self.records.append(rec)
		elif kind == 'enum':
			for t in inner:
				if t[0] == 'id':
					self.globals.add(t[1])
		else:
			b = set()
			self._names(inner, b)
			self.globals.update(b)
		return j + 1

	def _record(self, inner, rec):
		"""the members of a struct body: the last name of each declaration (before ; , : or [ ), and the types are
		globals; a nested struct body adds its members to the same record (anonymous members)"""
		decl = []
		depth = 0
		for t in inner:
			if t[0] == 'ws' and t[1].strip() == '':
				continue
			if t == ('ws', '{') or t == ('ws', '}'):
				decl = []
				continue
			if t[0] == 'ws' and t[1] in (';', ','):
				self._member(decl, rec)
				decl = []
				continue
			decl.append(t)

	def _add_member(self, t):
		self.members.add(t[1])
		off = getattr(t, 'off', None)
		if off is not None:
			self.member_offs.add(off)

	def _member(self, decl, rec):
		ids0 = []
		name = None
		depth = 0
		for k, t in enumerate(decl):
			if t[0] == 'ws' and t[1] in ('(', '['):
				depth += 1
			elif t[0] == 'ws' and t[1] in (')', ']'):
				depth -= 1
			elif t[0] == 'ws' and t[1] == ':' and depth == 0:
				break
			elif t[0] == 'id':
				if name is None and depth == 1 and decl[k - 1] == ('ws', '*'):
					name = t
					rest = decl[k + 1:]
					self._decl([('ws', '(')] + rest[1:] if rest[:1] == [('ws', ')')] else rest)
					break
				elif depth == 0:
					ids0.append(t)
				else:
					self.globals.add(t[1])
		if name is None and ids0:
			name = ids0.pop()
		self.globals.update(t[1] for t in ids0)
		if name is not None:
			rec.add(name[1])
			self._add_member(name)
