# Pure functions of the exe called by RVA through the module SDTest of the DLL (src/core/testmod.c), in the main
# menu. With the hooks on the calls reach the C versions, with them off the originals (Python 1.5.2 syntax).
OUT = 'sdcall.out'
execfile('sdlib.py')
import SDTest

def call(name, rva, sig, *args):
	check('%s%s' % (name, fmt(args)), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))

# Global/FileUtils.c
for s in ('', ' ', '  a b  ', '\ta\r\nb\t', 'abc', ' \t\v', 'a\fb \f', '\r\n', 'x  y\n'):
	call('Str_Trim', 0x131400, 'vb', s)
pats = ('*', '?', '', 'a*', '*.py', '*.PY', 'a?c', '[a-c]x', '[!a-c]x', '[]x', '[a-', 'a[?]c', 'ab[cd', '*a*b*')
names = ('', 'a', 'abc', 'ax', 'dx', 'Init.py', 'init.PY', 'a?c', 'a]c', 'ab', 'aab')
for p in pats:
	for n in names:
		call('Str_MatchNoCase', 0x1318e0, 'bss', p, n)
		call('Wild_Match', 0x22b640, 'iss', p, n)
for l in ('', 'a', ' a ; b ', '*.py;*.txt', ';;', '\t*x\t', 'a b;c'):
	for n in ('a', 'b', 'x.py', 'X.TXT', 'a b', 'c', ''):
		call('StrList_Match', 0x1317b0, 'bsss', l, n, ';')
call('StrList_Match', 0x1317b0, 'bsss', 'a,b c', 'c', ', ')

# Global/StrUtils.c
for s in ('', 'abc', 'x' * 20):
	for n in (1, 2, 5, 30):
		call('Str_Copy', 0x1365c0, 'sbsi', '', s, n)
		call('Cad_Copy', 0x22cf50, 'sbsi', '', s, n)
for s in ('', ' a ', '\t a\tb \t', 'abc'):
	call('Str_TrimBlanks', 0x136630, 'vb', s)

# engine3d Util/Comodin.c
paths = ('', 'a', 'a.b', '.a', 'a.', '.', '..', 'dir\\', 'dir/', 'C:', 'C:a', 'C:\\a\\b.c', 'c:/x/y/z.tar.gz',
	'\\\\srv\\share\\f.x', 'a b .c  ', '   ', 'x\\.', 'x\\..', 'C:.', 'C:..', '..\\a', 'd:x\\', 'a:b:c', '1:2',
	'Bin\\Scrap.exe', 'x' * 300, 'd/' + 'y' * 300 + '.e', 'a.' + 'e' * 300)
for r in paths:
	call('Ruta_Split', 0x22bc10, 'vsbbbb', r, '', '', '', '')
	call('Ruta_Split NULL', 0x22bc10, 'vszbzb', r, None, '', None, '')
	for m in (0, 1, 2, 4, 8, 3, 12, 15):
		call('Ruta_Part', 0x22cab0, 'vbsi', '', r, m)
	call('Ruta_PartN', 0x22cb90, 'vbisi', '', 5, r, 15)
	call('Ruta_FileName', 0x22cc00, 'ss', r)
	call('Ruta_FirstDir', 0x22cc70, 'vbs', '', r)
	call('Ruta_LastDir', 0x22cd20, 'vbs', '', r)
	call('Ruta_DriveNum', 0x22cdf0, 'is', r)
	call('Ruta_Format', 0x22b800, 'vbss', '', r, '[&u|&d|&n|&e|&&|&U&D&N&E]')
	for r2 in ('', 'C:\\base\\def.ext', 'name', '.ext', 'sub\\'):
		# the original copies a single path unbounded, the C version up to MAX_RUTA
		if r2 or len(r) < 260:
			call('Ruta_Merge', 0x22c190, 'vbss', '', r, r2)
		call('Ruta_Merge NULL', 0x22c190, 'vbzs', '', None, r2 or 'x')
for r in ('', 'a', 'a.b', 'dir\\', 'C:', 'C:a', '..', '.\\x\\..\\y', 'a\\b\\..\\..\\c', '\\a\\.\\b',
	'C:\\a\\..\\..\\b', 'C:\\', 'D:x\\y', 'x/./y/../z.', 'Bin'):
	for b in (None, 'C:\\base\\dir', 'D:\\', 'base\\', 'C:', '\\root'):
		call('Ruta_Full', 0x22c7c0, 'sbsiz', '', r, 260, b)
	call('Ruta_Full short', 0x22c7c0, 'sbsiz', '', r, 4, 'C:\\base')
for p in ('*', '*.*', 'a*', '*.PY', '?b.*', 'x', '.e', 'a.', '[ab]*.[!c]', 'dir\\*.py', 'C:\\d\\*'):
	for r in ('a.py', 'AB.c', 'x', 'x.', 'dir\\b.py', 'C:\\d\\e\\f.g', 'C:\\D\\f', '.e'):
		call('Ruta_Match', 0x22c300, 'iss', p, r)
		call('Ruta_MatchPath', 0x22c440, 'issi', p, r, 0)
		call('Ruta_MatchPath subdirs', 0x22c440, 'issi', p, r, 1)
for src in ('name.ext', 'n', 'longname.x', '.e', 'a.b.c'):
	for pat in ('*', '*.*', '?.?', 'x*.bak', '??z*.*', '(1 2)x.(-1)', '((a).*', 'd\\*.n', '*.', '.', 'a(3 1).(-2 -1 9)'):
		call('Ruta_Rename', 0x22c510, 'vbss', '', src, pat)
for p in ('[abc]', '[a-c]x', '[!a]', '[]', '[!]', '[a-]', '[?]', '[a', '[', 'x', '[z-a]', '[--/]'):
	for c in ('a', 'b', 'x', '-', ']', '\377', ''):
		call('Wild_MatchSet', 0x22b590, 'sss', p, c)
for c in ('a', '\\', '/', ':', '\000'):
	call('Ruta_IsSep', 0x22c170, 'ii', ord(c))
call('Ruta_GetDrive', 0x22b410, 'vp', 0)
call('Ruta_GetCwd', 0x22ce50, 'iibi', 0, '', 260)
call('Ruta_GetCwd short', 0x22ce50, 'iibi', 0, '', 3)
call('Ruta_GetCwd C', 0x22ce50, 'iibi', 3, '', 260)
for r in ('Bin', 'Bin\\', 'Bin/', 'SDNoDir', 'SDNoDir\\', '.', 'C:'):
	call('Ruta_IsDir', 0x22ba00, 'sb', r)
	call('Ruta_IsDir after', 0x22ce50, 'iibi', 0, '', 260)
call('Ruta_MakePath', 0x22bb60, 'is', 'Bin\\Scrap.exe')
call('Ruta_MakePath after', 0x22ce50, 'iibi', 0, '', 260)
for d in ('', 'C', 'C:', 'x', 'dir'):
	for n in ('', 'n'):
		call('Ruta_Make', 0x22b6e0, 'vbzzzz', '', d, d + 'x\\', n, '.e')
		call('Ruta_Make NULL', 0x22b6e0, 'vbzzzz', '', None, None, None, None)

# engine3d Util/Cadenas.c and Bprintf.c
for s, pos in (('abc', 0), ('abc', 1), ('abc', 3), ('', 0)):
	call('Cad_InsertChar', 0x22cf10, 'vbiii', s, ord('X'), pos, len(s))
for s in ('', '1', '123', '1234', '1234567', '-1234567', 'x123456', '12x3456789', '0000000000'):
	call('Cad_Thousands', 0x22d000, 'ib', s)
for n in (0, 1, -1, 999, 1000, -1000, 123456789, -2147483647 - 1):
	call('Cad_FromInt', 0x22d090, 'szi', None, n)
	call('Cad_FromInt buf', 0x22d090, 'sbi', '', n)
call('Cad_Dup', 0x22cfa0, 'ss', 'dup')
for n in (1, 2, 4, 20):
	call('Bprintf', 0x2389a0, 'ibiss', '', n, '%s-%s', 'ab')
finish()
