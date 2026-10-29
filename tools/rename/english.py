"""The translation of the Spanish names of the original into English (stage 4, docs/rename.md).

english(name) translates one identifier, file name or folder name by its word parts: the identifier is split at the
underscores and the case changes (CNodo3D_SetPadre: C Nodo 3 D _ Set Padre), the Spanish words are translated with
tools/rename/glossary.tsv and the decisions of tools/rename/overrides.tsv, and the parts are joined again in the case
pattern of the old name (CNode3D_SetParent, MALLAD3D -> MESHD3D, g_PadreNodo -> g_ParentNode). The generators of
tools/standalone and tools/test call it for the names they derive from the data of the exe (the RTTI class names of
the vtables, the names of the engine variables), so their output stays English.

overrides.tsv, one decision per line (kind, old, new, note):
  word      a word of unsure.tsv or a glossary word with another translation: new is the English word, or empty to
            keep the old word
  weak      a word that is also English or an English abbreviation: translated only when the name has a Spanish word
            of its own (DeActivate stays, Nodo_EsDeJuego is translated)
  compound  a run of letters without a case change or underscore (LISTAVERT, mallatri): new splits it into words
            separated by spaces
  phrase    a sequence of words (new separated by spaces): the English words in English order (lista vert -> vert
            list, so CListaVert -> CVertList); the words take the case pattern of the old positions
  name      a whole identifier: its English name (for names where word by word gives bad English)
  piece     a piece between underscores (CArchINI in Fini_CArchINI_X): its English
  prefix    the start of identifiers (Arch_ of the data file functions): its English, the rest word by word
  keep      a whole identifier that is never renamed (seen by the scripts or the game, or English already)
  path      a file or folder of src/ or include/: its new path (where the translated name is taken)
  local     old, new, file, function: a local name in one function of one file (two names of one scope that
            would become the same word)
  prose     an English word that stays in comments and docs (the actual size), also where it is a renamed
            identifier
  member    a name where it is a member of a struct (x.m, x->m, the field, SD_OFFSET): its English there (esc is
            a scene as a local, a scale as a member)
"""

import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
GLOSSARY = os.path.join(HERE, 'glossary.tsv')
OVERRIDES = os.path.join(HERE, 'overrides.tsv')

# a letter run in the case pattern: an all-capital run not followed by a small letter (CE in CEscena is C + Escena),
# a capital with small letters, small letters, digits
SEG = re.compile(r'[A-Z]+(?![a-z])|[A-Z][a-z]+|[a-z]+|[0-9]+|[A-Z]')
HEX = re.compile(r'^(0x)?[0-9a-fA-F]*[0-9][0-9a-fA-F]*$')

C_KEYWORDS = set('''auto break case char const continue default do double else enum extern float for goto if inline int
long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while bool
true false nullptr typeof alignas alignof static_assert thread_local constexpr _Bool asm __asm__'''.split())


def _tsv(path):
	rows = []
	with open(path, encoding='utf-8') as f:
		lines = f.read().splitlines()
	for line in lines[1:]:
		if line.strip() and not line.startswith('#'):
			rows.append(line.split('\t'))
	return rows


class Translator:
	def __init__(self):
		self.words = {}		# spanish word -> english word ('' keeps it)
		self.weak = set()
		self.compounds = {}	# letter run -> list of words
		self.phrases = {}	# tuple of words -> tuple of english words
		self.names = {}		# whole identifier -> english
		self.keep = set()
		self.prefixes = {}
		self.pieces = {}
		self.paths = {}
		self.locals = {}	# (file, function) -> {old: new}
		self.members = {}
		self.prose = set()
		for row in _tsv(GLOSSARY):
			self.words[row[0]] = row[1]
		for row in _tsv(OVERRIDES):
			kind, old = row[0], row[1]
			new = row[2] if len(row) > 2 else ''
			if kind == 'word':
				self.words[old] = new or old
			elif kind == 'weak':
				self.weak.add(old)
				if new:
					self.words[old] = new
			elif kind == 'compound':
				self.compounds[old] = new.split()
			elif kind == 'phrase':
				self.phrases[tuple(old.split())] = tuple(new.split())
			elif kind == 'name':
				self.names[old] = new
			elif kind == 'keep':
				self.keep.add(old)
			elif kind == 'prefix':
				self.prefixes[old] = new
			elif kind == 'piece':
				self.pieces[old] = new
			elif kind == 'path':
				self.paths[old] = new
			elif kind == 'local':
				self.locals.setdefault((row[3], row[4]), {})[old] = new
			elif kind == 'member':
				self.members[old] = new
			elif kind == 'prose':
				self.prose.add(old)
			else:
				raise ValueError('overrides.tsv: unknown kind %r' % kind)
		self.maxphrase = max([len(p) for p in self.phrases] or [1])
		self._cache = {}

	def spanish(self, word):
		"""the English of a Spanish word (lower case), None when it is no Spanish word"""
		en = self.words.get(word)
		if en is None or en == word:
			return None
		return en

	# ---- the word parts of a name ----

	@staticmethod
	def style(seg):
		if seg.isdigit():
			return 'D'
		if len(seg) == 1:
			return 'T' if seg.isupper() else 'L'
		if seg.isupper():
			return 'U'
		if seg[0].isupper():
			return 'T'
		return 'L'

	@staticmethod
	def styled(word, style):
		if style == 'U':
			return word.upper()
		if style == 'T':
			return word[0].upper() + word[1:]
		if style == 'L':
			return word[0].lower() + word[1:]
		return word

	def parts(self, piece):
		"""the words of one piece between underscores: a list of groups, one per letter run, each a list of
		[word, style, text] (text: the run as written, None for the words of a compound, which are written together
		in the style of their run: LISTAVERT is one group of two words)"""
		groups = []
		for seg in SEG.findall(piece):
			low = seg.lower()
			st = self.style(seg)
			if low in self.compounds and low not in self.words:
				groups.append([[w, st, None] for w in self.compounds[low]])
			else:
				groups.append([[low, st, seg]])
		return groups

	def translate_piece(self, piece, context):
		if piece in self.pieces:
			return self.pieces[piece]
		if HEX.match(piece):
			return piece
		groups = self.parts(piece)
		if not groups:
			return piece
		letters = [g[0][2] for g in groups if g[0][2] is None or not g[0][2].isdigit()]
		segs = SEG.findall(piece)
		upper = all(x.isupper() for x in segs if not x.isdigit())
		lower = all(x.islower() for x in segs if not x.isdigit())
		flat = [w for g in groups for w in g]
		words = [w[0] for w in flat]
		done = [None] * len(flat)	# the written form of the words of a phrase
		# phrases, longest first, left to right; a word that only moves keeps its form (INI, the D of 2D), the
		# English words are written like the first word at the start of the piece, else capitalized
		i = 0
		while i < len(flat):
			match = False
			for n in range(min(self.maxphrase, len(flat) - i), 1, -1):
				key = tuple(words[i:i + n])
				if key not in self.phrases:
					continue
				new = self.phrases[key]
				if len(new) != n:
					raise ValueError('phrase %r -> %r: not the same number of words' % (key, new))
				olds = {}
				for k in range(n):
					olds.setdefault(words[i + k], flat[i + k])
				first = flat[i]
				for k in range(n):
					w = new[k]
					if upper:
						form = w.upper()
					elif lower:
						form = w.lower()
					elif w in olds and olds[w][2] is not None and not self.spanish(w):
						form = olds[w][2]
						if i == 0 and k == 0 and first[1] == 'L':
							form = form[0].lower() + form[1:]
					elif i == 0 and k == 0:
						form = self.styled(w, 'L' if first[1] == 'L' else 'T')
					else:
						form = self.styled(w, 'T')
					done[i + k] = form
				i += n
				match = True
				break
			if not match:
				i += 1
		out = []
		k = 0
		for g in groups:
			for w in g:
				if done[k] is not None:
					out.append(done[k])
				else:
					en = self.spanish(w[0])
					if en is not None and (w[0] not in self.weak or context):
						word = en
					else:
						word = w[0]
					if w[2] is not None and word == w[0]:
						out.append(w[2])
					elif w[1] == 'D':
						out.append(word)
					elif w[2] is None and w is not g[0]:
						# the later words of a compound: in the case of the run (Listavert -> Vertlist)
						out.append(word.upper() if w[1] == 'U' else word.lower())
					else:
						out.append(self.styled(word, w[1]))
				k += 1
		return ''.join(out)

	def has_spanish(self, name):
		"""a word of the name that is Spanish and not weak (the context for the weak words)"""
		for piece in name.split('_'):
			if piece in self.pieces:
				return True
			if HEX.match(piece):
				continue
			for g in self.parts(piece):
				for w in g:
					if w[0] not in self.weak and self.spanish(w[0]) is not None:
						return True
		ws = [w[0] for piece in name.split('_') if not HEX.match(piece) for g in self.parts(piece) for w in g]
		for n in range(2, self.maxphrase + 1):
			for i in range(len(ws) - n + 1):
				if tuple(ws[i:i + n]) in self.phrases:
					return True
		return False

	def english(self, name):
		r = self._cache.get(name)
		if r is not None:
			return r
		if name in self.names:
			r = self.names[name]
		elif name in self.keep:
			r = name
		elif any(name.startswith(p) for p in self.prefixes):
			p = max((p for p in self.prefixes if name.startswith(p)), key=len)
			rest = name[len(p):]
			r = self.prefixes[p] + ('_'.join(self.translate_piece(q, True) for q in rest.split('_')) if rest else '')
		else:
			context = self.has_spanish(name)
			r = '_'.join(self.translate_piece(p, context) for p in name.split('_'))
		self._cache[name] = r
		return r


_translator = None


def english(name):
	"""the English name of an identifier (the same name when it has no Spanish word)"""
	global _translator
	if _translator is None:
		_translator = Translator()
	return _translator.english(name)


def translator():
	english('')
	return _translator
