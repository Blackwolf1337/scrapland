#!/usr/bin/env python3
# rsrc.py [-w] [Scrap.exe]: the resources of Scrap.exe 1.6.1 as a resource script for ScrapC.exe (docs/standalone.md,
# stage 3b3). Reads the .rsrc of the original and makes src/standalone/scrap.rc (dialogs, menus, accelerators in
# readable form) and the binary resources in src/standalone/res (icons, bitmaps, the manifest), with the original
# types, ids and languages. With -w it writes them, without it only checks that the files in the repository are what
# it would write. windres compiles scrap.rc into ScrapC.exe; rsrccheck.py compares the result with the original.
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import ROOT, Exe, ExeError

RC = 'src/standalone/scrap.rc'
RES = 'src/standalone/res'

RT_CURSOR, RT_BITMAP, RT_ICON, RT_MENU, RT_DIALOG, RT_STRING = 1, 2, 3, 4, 5, 6
RT_ACCELERATOR, RT_RCDATA, RT_GROUP_CURSOR, RT_GROUP_ICON, RT_VERSION, RT_MANIFEST = 9, 10, 12, 14, 16, 24

LANGS = {
	0x0409: 'LANG_ENGLISH, SUBLANG_ENGLISH_US',
	0x0c0a: 'LANG_SPANISH, SUBLANG_SPANISH_MODERN',
}


class RsrcError(Exception):
	pass


def resources(x):
	"""the resources of the PE image x (sdexe.Exe) as {(type, id, lang): bytes}; a name instead of an id is a str"""
	rva, size = x.dirs[2]
	if not rva:
		return {}

	def rd(o, n):
		return x.bytes(rva + o, rva + o + n)

	def name(e):
		if e & 0x80000000:
			o = e & 0x7fffffff
			return rd(o + 2, 2 * struct.unpack('<H', rd(o, 2))[0]).decode('utf-16le')
		return e

	out = {}

	def walk(o, path):
		nn, ni = struct.unpack_from('<HH', rd(o, 16), 12)
		for i in range(nn + ni):
			n, d = struct.unpack('<II', rd(o + 16 + 8 * i, 8))
			if d & 0x80000000:
				walk(d & 0x7fffffff, path + (name(n),))
			elif len(path) != 2:
				raise RsrcError('a resource entry at depth %d of the tree' % len(path))
			else:
				drva, dsize = struct.unpack('<II', rd(d, 8))
				out[path + (name(n),)] = x.bytes(drva, drva + dsize)

	walk(0, ())
	return out


class Reader:
	def __init__(self, b, what):
		self.b, self.at, self.what = b, 0, what

	def get(self, fmt):
		try:
			v = struct.unpack_from('<' + fmt, self.b, self.at)
		except struct.error:
			raise RsrcError('%s: ends at +%x' % (self.what, self.at))
		self.at += struct.calcsize('<' + fmt)
		return v if len(v) > 1 else v[0]

	def sz(self):
		end = self.at
		while self.b[end:end + 2] != b'\0\0':
			end += 2
			if end >= len(self.b):
				raise RsrcError('%s: string at +%x without end' % (self.what, self.at))
		s = self.b[self.at:end].decode('utf-16le')
		self.at = end + 2
		return s

	def sz_or_ord(self):
		if self.b[self.at:self.at + 2] == b'\xff\xff':
			self.at += 2
			return self.get('H')
		return self.sz()

	def align(self):
		self.at = (self.at + 3) & ~3


def parse_dialog(b, what):
	"""a DIALOG template (DLGTEMPLATE, not DIALOGEX): a dict of its header and its controls"""
	r = Reader(b, what)
	style, exstyle, n, x, y, cx, cy = r.get('IIHhhhh')
	if style >> 16 == 0xffff and style & 0xffff == 1:
		raise RsrcError('%s: DIALOGEX, which rsrc.py does not write yet' % what)
	d = dict(style=style, exstyle=exstyle, rect=(x, y, cx, cy), menu=r.sz_or_ord(), cls=r.sz_or_ord(), title=r.sz(),
		font=None, controls=[])
	if style & 0x40:	# DS_SETFONT
		d['font'] = (r.get('H'), r.sz())
	for i in range(n):
		r.align()
		cstyle, cexstyle, x, y, cx, cy, cid = r.get('IIhhhhH')
		c = dict(style=cstyle, exstyle=cexstyle, rect=(x, y, cx, cy), id=cid, cls=r.sz_or_ord(), text=r.sz_or_ord())
		extra = r.get('H')
		c['data'] = r.b[r.at:r.at + extra]
		r.at += extra
		d['controls'].append(c)
	d['tail'] = b[r.at:]
	return d


def parse_menu(b, what):
	"""a MENU template (not MENUEX): a list of (flags, id, text, items); a popup has id None and its items"""
	r = Reader(b, what)
	if r.get('HH') != (0, 0):
		raise RsrcError('%s: MENUEX or a header of the menu with an offset, which rsrc.py does not write yet' % what)

	def items():
		out = []
		while True:
			flags = r.get('H')
			if flags & 0x10:	# MF_POPUP
				text = r.sz()
				out.append((flags, None, text, items()))
			else:
				mid = r.get('H')
				out.append((flags, mid, r.sz(), None))
			if flags & 0x80:	# MF_END
				return out

	m = items()
	if r.at != len(b):
		raise RsrcError('%s: %d bytes after the menu' % (what, len(b) - r.at))
	return m


def parse_accel(b, what):
	if len(b) % 8:
		raise RsrcError('%s: size %d is not a multiple of 8' % (what, len(b)))
	acc = [struct.unpack_from('<HHHH', b, i) for i in range(0, len(b), 8)]
	for k, (flags, key, aid, pad) in enumerate(acc):
		if pad or bool(flags & 0x80) != (k == len(acc) - 1) or flags & ~0x9f:
			raise RsrcError('%s: entry %d (flags %04x, padding %04x) is not what windres writes' % (what, k, flags, pad))
	return acc


# names of the style bits, the first match wins: (mask, value, name); masks of several bits are the type fields
WS_COMMON = [
	(0xc0000000, 0xc0000000, 'WS_POPUP | WS_CHILD'),
	(0x80000000, 0x80000000, 'WS_POPUP'), (0x40000000, 0x40000000, 'WS_CHILD'),
	(0x20000000, 0x20000000, 'WS_MINIMIZE'), (0x10000000, 0x10000000, 'WS_VISIBLE'),
	(0x08000000, 0x08000000, 'WS_DISABLED'), (0x04000000, 0x04000000, 'WS_CLIPSIBLINGS'),
	(0x02000000, 0x02000000, 'WS_CLIPCHILDREN'), (0x01000000, 0x01000000, 'WS_MAXIMIZE'),
	(0x00c00000, 0x00c00000, 'WS_CAPTION'), (0x00800000, 0x00800000, 'WS_BORDER'),
	(0x00400000, 0x00400000, 'WS_DLGFRAME'), (0x00200000, 0x00200000, 'WS_VSCROLL'),
	(0x00100000, 0x00100000, 'WS_HSCROLL'), (0x00080000, 0x00080000, 'WS_SYSMENU'),
	(0x00040000, 0x00040000, 'WS_THICKFRAME'),
]
WS_DIALOG = WS_COMMON + [(0x20000, 0x20000, 'WS_MINIMIZEBOX'), (0x10000, 0x10000, 'WS_MAXIMIZEBOX')]
WS_CONTROL = WS_COMMON + [(0x20000, 0x20000, 'WS_GROUP'), (0x10000, 0x10000, 'WS_TABSTOP')]


def bits(*names):
	return [(v, v, n) for v, n in names]


DS = bits((0x1, 'DS_ABSALIGN'), (0x2, 'DS_SYSMODAL'), (0x4, 'DS_3DLOOK'), (0x8, 'DS_FIXEDSYS'),
	(0x10, 'DS_NOFAILCREATE'), (0x20, 'DS_LOCALEDIT'), (0x40, 'DS_SETFONT'), (0x80, 'DS_MODALFRAME'),
	(0x100, 'DS_NOIDLEMSG'), (0x200, 'DS_SETFOREGROUND'), (0x400, 'DS_CONTROL'), (0x800, 'DS_CENTER'),
	(0x1000, 'DS_CENTERMOUSE'), (0x2000, 'DS_CONTEXTHELP'))

# the predefined control classes (an ordinal in the template) with the names of their style bits
BUTTON_TYPES = ['BS_PUSHBUTTON', 'BS_DEFPUSHBUTTON', 'BS_CHECKBOX', 'BS_AUTOCHECKBOX', 'BS_RADIOBUTTON', 'BS_3STATE',
	'BS_AUTO3STATE', 'BS_GROUPBOX', 'BS_USERBUTTON', 'BS_AUTORADIOBUTTON', None, 'BS_OWNERDRAW']
STATIC_TYPES = ['SS_LEFT', 'SS_CENTER', 'SS_RIGHT', 'SS_ICON', 'SS_BLACKRECT', 'SS_GRAYRECT', 'SS_WHITERECT',
	'SS_BLACKFRAME', 'SS_GRAYFRAME', 'SS_WHITEFRAME', 'SS_USERITEM', 'SS_SIMPLE', 'SS_LEFTNOWORDWRAP', 'SS_OWNERDRAW',
	'SS_BITMAP', 'SS_ENHMETAFILE', 'SS_ETCHEDHORZ', 'SS_ETCHEDVERT', 'SS_ETCHEDFRAME']
EDIT = [(0x3, 1, 'ES_CENTER'), (0x3, 2, 'ES_RIGHT')] + bits((0x4, 'ES_MULTILINE'), (0x8, 'ES_UPPERCASE'),
	(0x10, 'ES_LOWERCASE'), (0x20, 'ES_PASSWORD'), (0x40, 'ES_AUTOVSCROLL'), (0x80, 'ES_AUTOHSCROLL'),
	(0x100, 'ES_NOHIDESEL'), (0x400, 'ES_OEMCONVERT'), (0x800, 'ES_READONLY'), (0x1000, 'ES_WANTRETURN'))
CLASSES = {
	0x80: ('Button', [(0xf, i, n) for i, n in enumerate(BUTTON_TYPES) if n and i] + bits((0x20, 'BS_LEFTTEXT'),
		(0x40, 'BS_ICON'), (0x80, 'BS_BITMAP')) + [(0x300, 0x300, 'BS_CENTER'), (0x300, 0x100, 'BS_LEFT'),
		(0x300, 0x200, 'BS_RIGHT'), (0xc00, 0xc00, 'BS_VCENTER'), (0xc00, 0x400, 'BS_TOP'), (0xc00, 0x800, 'BS_BOTTOM')]
		+ bits((0x1000, 'BS_PUSHLIKE'), (0x2000, 'BS_MULTILINE'), (0x4000, 'BS_NOTIFY'), (0x8000, 'BS_FLAT'))),
	0x81: ('Edit', EDIT + bits((0x2000, 'ES_NUMBER'))),
	0x82: ('Static', [(0x1f, i, n) for i, n in enumerate(STATIC_TYPES) if i] + bits((0x40, 'SS_REALSIZECONTROL'),
		(0x80, 'SS_NOPREFIX'), (0x100, 'SS_NOTIFY'), (0x200, 'SS_CENTERIMAGE'), (0x400, 'SS_RIGHTJUST'),
		(0x800, 'SS_REALSIZEIMAGE'), (0x1000, 'SS_SUNKEN'), (0x2000, 'SS_EDITCONTROL'))
		+ [(0xc000, 0xc000, 'SS_WORDELLIPSIS'), (0xc000, 0x4000, 'SS_ENDELLIPSIS'), (0xc000, 0x8000, 'SS_PATHELLIPSIS')]),
	0x83: ('ListBox', bits((0x1, 'LBS_NOTIFY'), (0x2, 'LBS_SORT'), (0x4, 'LBS_NOREDRAW'), (0x8, 'LBS_MULTIPLESEL'),
		(0x10, 'LBS_OWNERDRAWFIXED'), (0x20, 'LBS_OWNERDRAWVARIABLE'), (0x40, 'LBS_HASSTRINGS'),
		(0x80, 'LBS_USETABSTOPS'), (0x100, 'LBS_NOINTEGRALHEIGHT'), (0x200, 'LBS_MULTICOLUMN'),
		(0x400, 'LBS_WANTKEYBOARDINPUT'), (0x800, 'LBS_EXTENDEDSEL'), (0x1000, 'LBS_DISABLENOSCROLL'),
		(0x2000, 'LBS_NODATA'), (0x4000, 'LBS_NOSEL'), (0x8000, 'LBS_COMBOBOX'))),
	0x84: ('ScrollBar', bits((0x1, 'SBS_VERT'))),
	0x85: ('ComboBox', [(0x3, 1, 'CBS_SIMPLE'), (0x3, 2, 'CBS_DROPDOWN'), (0x3, 3, 'CBS_DROPDOWNLIST')]
		+ bits((0x10, 'CBS_OWNERDRAWFIXED'), (0x20, 'CBS_OWNERDRAWVARIABLE'), (0x40, 'CBS_AUTOHSCROLL'),
		(0x80, 'CBS_OEMCONVERT'), (0x100, 'CBS_SORT'), (0x200, 'CBS_HASSTRINGS'), (0x400, 'CBS_NOINTEGRALHEIGHT'),
		(0x800, 'CBS_DISABLENOSCROLL'), (0x2000, 'CBS_UPPERCASE'), (0x4000, 'CBS_LOWERCASE'))),
}
# the classes by name (commctrl.h, richedit.h); RICHEDIT is the Rich Edit 1.0 class of riched32.dll
NAMED = {
	'systreeview32': bits((0x1, 'TVS_HASBUTTONS'), (0x2, 'TVS_HASLINES'), (0x4, 'TVS_LINESATROOT'),
		(0x8, 'TVS_EDITLABELS'), (0x10, 'TVS_DISABLEDRAGDROP'), (0x20, 'TVS_SHOWSELALWAYS'), (0x40, 'TVS_RTLREADING'),
		(0x80, 'TVS_NOTOOLTIPS'), (0x100, 'TVS_CHECKBOXES'), (0x200, 'TVS_TRACKSELECT'), (0x400, 'TVS_SINGLEEXPAND'),
		(0x800, 'TVS_INFOTIP'), (0x1000, 'TVS_FULLROWSELECT'), (0x2000, 'TVS_NOSCROLL'), (0x4000, 'TVS_NONEVENHEIGHT'),
		(0x8000, 'TVS_NOHSCROLL')),
	'richedit': EDIT + bits((0x2000, 'ES_DISABLENOSCROLL'), (0x4000, 'ES_SUNKEN'), (0x8000, 'ES_SAVESEL')),
}


def style_names(value, table, default=0):
	"""value as names of its bits; windres ORs the style onto default, so the default bits it lacks are NOT ones"""
	names = []
	left = value
	for mask, v, n in table:
		if left & mask == v and v:
			names.append(n)
			left &= ~mask
	if left:
		names.append('0x%x' % left)
	names += ['NOT %s' % n for mask, v, n in table if v and mask == v and default & v == v and not value & v]
	return ' | '.join(names) or '0'


def rc_string(s):
	out = '"'
	for ch in s:
		if ch == '"':
			out += '""'
		elif ch == '\\':
			out += '\\\\'
		elif ch == '\t':
			out += '\\t'
		elif ch == '\n':
			out += '\\n'
		elif ord(ch) < 0x20:
			out += '\\x%02x' % ord(ch)
		else:
			out += ch
	return out + '"'


def rc_id(v):
	return rc_string(v) if isinstance(v, str) else '%d' % v


WS_CHILD_VISIBLE = 0x50000000	# what windres adds to the style of a CONTROL


def dialog_rc(rid, d, what):
	if d['menu'] or d['cls'] or d['exstyle'] or d['tail'].strip(b'\0') or len(d['tail']) > 3:
		raise RsrcError('%s: a menu, class, extended style or data after the controls, which rsrc.py does not write' % what)
	if d['title'] and d['style'] & 0xc00000 != 0xc00000:
		raise RsrcError('%s: a caption without WS_CAPTION, windres would add it' % what)
	lines = ['%s DIALOG %d, %d, %d, %d' % ((rc_id(rid),) + d['rect']),
		'STYLE %s' % style_names(d['style'], WS_DIALOG + DS)]
	if d['title']:
		lines.append('CAPTION %s' % rc_string(d['title']))
	if d['font']:
		lines.append('FONT %d, %s' % (d['font'][0], rc_string(d['font'][1])))
	lines.append('BEGIN')
	for c in d['controls']:
		if c['data']:
			raise RsrcError('%s: control %d has creation data, which rsrc.py does not write' % (what, c['id']))
		if isinstance(c['cls'], str):
			table, cls = NAMED.get(c['cls'].lower(), []), rc_string(c['cls'])
		elif c['cls'] in CLASSES:
			name, table = CLASSES[c['cls']]
			cls = '0x%x /* %s */' % (c['cls'], name)
		else:
			raise RsrcError('%s: control %d of class ordinal 0x%x' % (what, c['id'], c['cls']))
		style = style_names(c['style'], WS_CONTROL + table, WS_CHILD_VISIBLE)
		cid = -1 if c['id'] == 0xffff else c['id']	# IDC_STATIC
		line = '\tCONTROL %s, %d, %s, %s, %d, %d, %d, %d' % ((rc_id(c['text']), cid, cls, style) + c['rect'])
		if c['exstyle']:
			line += ', 0x%x' % c['exstyle']
		lines.append(line)
	return lines + ['END']


MENU_FLAGS = [(0x1, 'GRAYED'), (0x2, 'INACTIVE'), (0x8, 'CHECKED'), (0x20, 'MENUBARBREAK'), (0x40, 'MENUBREAK'),
	(0x4000, 'HELP')]


def menu_rc(rid, m, what):
	lines = ['%s MENU' % rc_id(rid)]

	def items(m, ind):
		lines.append(ind[:-1] + 'BEGIN')
		for flags, mid, text, sub in m:
			opts = [n for v, n in MENU_FLAGS if flags & v]
			if flags & ~(0x90 | sum(v for v, n in MENU_FLAGS)):
				raise RsrcError('%s: menu item %r with flags %04x' % (what, text, flags))
			if sub is not None:
				lines.append('%sPOPUP %s' % (ind, ', '.join([rc_string(text)] + opts)))
				items(sub, ind + '\t')
			elif not (flags & ~0x80 or mid or text):
				lines.append('%sMENUITEM SEPARATOR' % ind)
			else:
				lines.append('%sMENUITEM %s' % (ind, ', '.join([rc_string(text), '%d' % mid] + opts)))
		lines.append(ind[:-1] + 'END')

	items(m, '\t')
	return lines


VK = {0x08: 'VK_BACK', 0x09: 'VK_TAB', 0x0d: 'VK_RETURN', 0x13: 'VK_PAUSE', 0x1b: 'VK_ESCAPE', 0x20: 'VK_SPACE',
	0x21: 'VK_PRIOR', 0x22: 'VK_NEXT', 0x23: 'VK_END', 0x24: 'VK_HOME', 0x25: 'VK_LEFT', 0x26: 'VK_UP',
	0x27: 'VK_RIGHT', 0x28: 'VK_DOWN', 0x2d: 'VK_INSERT', 0x2e: 'VK_DELETE'}
VK.update((0x70 + i, 'VK_F%d' % (i + 1)) for i in range(24))
ACC_FLAGS = [(0x1, 'VIRTKEY'), (0x2, 'NOINVERT'), (0x4, 'SHIFT'), (0x8, 'CONTROL'), (0x10, 'ALT')]


def accel_rc(rid, acc):
	lines = ['%s ACCELERATORS' % rc_id(rid), 'BEGIN']
	for flags, key, aid, pad in acc:
		if flags & 1 and (0x30 <= key <= 0x39 or 0x41 <= key <= 0x5a):
			k = '"%c"' % key
		elif flags & 1:
			k = VK.get(key, '0x%02x' % key)
		elif 0x20 < key < 0x7f and key not in b'"\\^':
			k = '"%c"' % key
		else:
			k = '%d' % key
		lines.append('\t%s' % ', '.join([k, '%d' % aid] + [n for v, n in ACC_FLAGS if flags & v]))
	return lines + ['END']


def ico_file(group, icons, what):
	"""the .ico of a group icon: its directory with the offsets of the images, then the RT_ICON images"""
	r = Reader(group, what)
	res, typ, n = r.get('HHH')
	if res or typ != 1 or len(group) != 6 + 14 * n:
		raise RsrcError('%s: no icon group' % what)
	entries = [r.get('BBBBHHIH') for i in range(n)]
	head = struct.pack('<HHH', 0, 1, n)
	data = b''
	at = 6 + 16 * n
	for w, h, colors, rsv, planes, bpp, size, iid in entries:
		img = icons[iid]
		if size != len(img) or not planes or not bpp:
			raise RsrcError('%s: icon %d: size %d of %d, planes %d, bits %d (windres would change them)'
				% (what, iid, size, len(img), planes, bpp))
		head += struct.pack('<BBBBHHII', w, h, colors, rsv, planes, bpp, size, at + len(data))
		data += img
	return head + data, [e[7] for e in entries]


def bmp_file(b, what):
	"""the .bmp of a bitmap resource: a BITMAPFILEHEADER before it (windres takes it off again)"""
	if len(b) < 40:
		raise RsrcError('%s: no bitmap' % what)
	size, w, h, planes, bpp, comp, isize, xr, yr, used = struct.unpack_from('<IiiHHIIiiI', b, 0)
	colors = used or (1 << bpp if bpp <= 8 else 0)
	off = 14 + size + 4 * colors + (12 if comp == 3 and size == 40 else 0)
	return b'BM' + struct.pack('<IHHI', 14 + len(b), 0, 0, off) + b


def generate(x):
	"""scrap.rc and the files of res/ as {path: bytes}"""
	rs = resources(x)
	lines = ['/*',
		' * The resources of Scrap.exe 1.6.1 for ScrapC.exe (docs/standalone.md, stage 3b3), generated by',
		' * tools/standalone/rsrc.py from the .rsrc of the original, do not edit. Types, ids and languages are the',
		' * original\'s. The predefined control classes are ordinals as in the original (windres would write a class name',
		' * as a string), the icon images get the ids 1, 2 ... in the order of the ICON lines, as in the original.',
		' */',
		'#include <windows.h>', '#include <commctrl.h>', '', '#pragma code_page(65001)']
	files = {}
	icons = {}
	for (t, rid, lang), b in rs.items():
		if t == RT_ICON:
			icons[rid] = b
	used_icons = []
	lang_now = None
	for t, rid, lang in rs:
		if isinstance(t, str) or isinstance(rid, str):
			raise RsrcError('resource %s %s lang %04x: a named resource, which rsrc.py does not write yet' % (t, rid, lang))
	# by type and id, the manifest last; the icon groups in the order of their ids, which numbers their images
	for t, rid, lang in sorted(rs, key=lambda k: (k[0] == RT_MANIFEST,) + k):
		b = rs[t, rid, lang]
		what = 'resource %d %d lang %04x' % (t, rid, lang)
		if t == RT_ICON:
			continue
		if lang != lang_now:
			if lang not in LANGS:
				raise RsrcError('%s: no name for the language' % what)
			lines += ['', 'LANGUAGE %s' % LANGS[lang]]
			lang_now = lang
		lines.append('')
		if t == RT_DIALOG:
			lines += dialog_rc(rid, parse_dialog(b, what), what)
		elif t == RT_MENU:
			lines += menu_rc(rid, parse_menu(b, what), what)
		elif t == RT_ACCELERATOR:
			lines += accel_rc(rid, parse_accel(b, what))
		elif t == RT_GROUP_ICON:
			name = 'icon%d.ico' % rid
			files[name], ids = ico_file(b, icons, what)
			for k, iid in enumerate(ids):
				if iid != len(used_icons) + 1 or rs.get((RT_ICON, iid, lang)) is None:
					raise RsrcError('%s: image %d is icon %d, windres would number it %d'
						% (what, k, iid, len(used_icons) + 1))
				used_icons.append(iid)
			lines.append('%d ICON "res/%s"' % (rid, name))
		elif t == RT_BITMAP:
			name = 'bitmap%d.bmp' % rid
			files[name] = bmp_file(b, what)
			lines.append('%d BITMAP "res/%s"' % (rid, name))
		elif t == RT_MANIFEST:
			name = 'manifest%d.xml' % rid
			files[name] = b
			lines.append('%d RT_MANIFEST "res/%s"' % (rid, name))
		else:
			name = 'type%d_%d.bin' % (t, rid)
			files[name] = b
			lines.append('%d %s "res/%s"' % (rid, 'RCDATA' if t == RT_RCDATA else '%d' % t, name))
	if sorted(used_icons) != sorted(k[1] for k in rs if k[0] == RT_ICON):
		raise RsrcError('icons outside the icon groups')
	text = '\n'.join(lines) + '\n'
	if any(n in text for m, v, n in NAMED['richedit'][len(EDIT):]):
		text = text.replace('#include <commctrl.h>\n', '#include <commctrl.h>\n#include <richedit.h>\n')
	out = {os.path.join(RES, n): b for n, b in files.items()}
	out[RC] = text.encode('utf-8')
	return out


def main():
	args = [a for a in sys.argv[1:] if a != '-w']
	exe = args[0] if args else os.path.join(ROOT, 'game', 'Bin', 'Scrap.exe')
	try:
		x = Exe(exe)
		files = generate(x)
	except (ExeError, RsrcError) as e:
		sys.exit('rsrc.py: %s' % e)
	have = set(os.path.join(RES, n) for n in os.listdir(os.path.join(ROOT, RES))) if os.path.isdir(
		os.path.join(ROOT, RES)) else set()
	if '-w' in sys.argv:
		os.makedirs(os.path.join(ROOT, RES), exist_ok=True)
		for path, b in files.items():
			open(os.path.join(ROOT, path), 'wb').write(b)
		for path in sorted(have - set(files)):
			os.remove(os.path.join(ROOT, path))
		print('rsrc.py: wrote %s and %d files in %s' % (RC, len(files) - 1, RES))
		return
	bad = [p for p, b in files.items() if not os.path.exists(os.path.join(ROOT, p))
		or open(os.path.join(ROOT, p), 'rb').read() != b] + sorted(have - set(files))
	for p in bad:
		print('rsrc.py: %s differs from what the .rsrc of the original gives' % p)
	if bad:
		sys.exit(1)
	print('rsrc.py: %s and %d files in %s are up to date' % (RC, len(files) - 1, RES))


if __name__ == '__main__':
	main()
