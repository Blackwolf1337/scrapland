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
# not 'C:': Ruta_IsDir changes to the directory it remembered for that drive, the tests would go on in C:\
for r in ('Bin', 'Bin\\', 'Bin/', 'SDNoDir', 'SDNoDir\\', '.'):
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

# engine3d Util/Lex.c and Engine/cmd_lex.c on files written here (difftest.sh deletes them), the output through
# SDTest's callback
def writefile(name, data):
	f = open(name, 'wb')
	f.write(data)
	f.close()

writefile('sdlex.txt', 'ident _x a#b high\351z 12 0 0x1F 0x 077 08 1.5 .5 1e10 1.e-3 1e+ 2E5x\n'
	'"str" "esc\\n\\t\\x41\\101\\x\\9\\q\\"" "unterminated\n'
	'; { } ( ) , = -3 +4.5 \'c\' x #y\n'
	'/* comment /* nested */ still\n*/ after\n'
	'// line comment\n'
	'#inc "sdlex2.txt"\n'
	'#bogus directive\n'
	' #notfirst\n'
	'#inc bad\n'
	'x\r\ny\rz\n\rw\n'
	'end\n')
writefile('sdlex2.txt', 'inc1 2\n#inc "sdnofile.txt"\nlast\n')
writefile('sdlex3.txt', 'a /* open comment\n\n')
writefile('sdcmd.txt', 'Load "file" 12 -7 +3 0x10 010 09 1.5 -2.5 3 name ; \n'
	'block { a { b } c } after\n'
	'x , y ) z\n')

check('Lex_SetOut(out)', lambda: SDTest.Call(0x2cda80, 'vi', SDTest.OutFunc()))
call('Lex_SetDir', 0x2cda90, 'vs', 'sdlex.txt')
call('Lex_Print', 0x2cd0c0, 'vss', '[%s]', 'x')
check('Lex out', SDTest.Out)

def lexfile(name, escapes):
	call('Lex_Open', 0x2cd5f0, 'isi', name, escapes)
	check('Lex out', SDTest.Out)
	for i in range(200):
		t = SDTest.Call(0x2cdac0, 'i')[0]
		check('Lex_Next %s %d' % (name, i), lambda t=t: (t, SDTest.Peek(0x4e6e28, 's'), SDTest.Peek(0x432a80, 'i'),
			SDTest.Peek(0x4e6e24, 'i'), SDTest.Peek(0x4e7228, 'i'), SDTest.Out()))
		if t == -1:
			break
	call('Lex_Close', 0x2cd780, 'vi', 2)
	check('Lex out', SDTest.Out)

lexfile('sdlex.txt', 1)
lexfile('sdlex.txt', 0)
lexfile('sdlex3.txt', 1)
call('Lex_Open missing', 0x2cd5f0, 'isi', 'sdnofile.txt', 0)
check('Lex out', SDTest.Out)

def cl():
	return (SDTest.Peek(0x4e733c, 'i'), SDTest.Peek(0x4e7338, 'p'), SDTest.Peek(0x4e7340, 'i'), SDTest.Out())
def step(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), cl()))

call('Lex_Open', 0x2cd5f0, 'isi', 'sdcmd.txt', 1)
SDTest.Poke(0x4e733c, 0)
step('CmdLex_Next', 0x2ce270, 'v')
step('CmdLex_Expect', 0x2cdce0, 'ii', 0)
step('CmdLex_Expect', 0x2cdce0, 'ii', 1)
step('CmdLex_Next', 0x2ce270, 'v')
step('CmdLex_Expect', 0x2cdce0, 'ii', 3)
step('CmdLex_Expect', 0x2cdce0, 'ii', 2)
step('CmdLex_Next', 0x2ce270, 'v')
for i in range(6):
	step('CmdLex_GetInt', 0x2cddf0, 'ip', 99)
for i in range(3):
	step('CmdLex_GetDouble', 0x2cdeb0, 'iD', 9.5)
check('CmdLex double', lambda: SDTest.Peek(0x4e7340, 'd'))
step('CmdLex_GetInt ident', 0x2cddf0, 'ip', 99)
step('CmdLex_GetDouble ident', 0x2cdeb0, 'iD', 9.5)
step('CmdLex_Next', 0x2ce270, 'v')
step('CmdLex_IsChar', 0x2cdd70, 'ii', ord(';'))
step('CmdLex_IsChar', 0x2cdd70, 'ii', ord(','))
step('CmdLex_GetInt char', 0x2cddf0, 'ip', 99)
step('CmdLex_NeedChar', 0x2cdda0, 'ii', ord(';'))
step('CmdLex_Next', 0x2ce270, 'v')
step('CmdLex_SkipBlock', 0x2cdf80, 'vi', 0)
step('CmdLex_SkipToAny', 0x2ce120, 'vs', ',)')
step('CmdLex_NeedChar', 0x2cdda0, 'ii', ord(')'))
step('CmdLex_SkipTo', 0x2cdfe0, 'vi', ord(')'))
step('CmdLex_GetDouble ident', 0x2cdeb0, 'iD', 9.5)
step('CmdLex_SkipBlock -1', 0x2cdf80, 'vi', -1)
step('CmdLex_SkipTo end', 0x2cdfe0, 'vi', ord('#'))
step('CmdLex_GetInt end', 0x2cddf0, 'ip', 99)
step('CmdLex_GetDouble end', 0x2cdeb0, 'iD', 9.5)
step('CmdLex_NeedChar end', 0x2cdda0, 'ii', ord(';'))
step('CmdLex_SkipToAny end', 0x2ce120, 'vs', '{')
call('Lex_Close', 0x2cd780, 'vi', 1)
check('Lex out', SDTest.Out)
call('Lex_Error', 0x2cd830, 'viss', 2, 'info %s', 'no file')
check('Lex out', SDTest.Out)

# engine3d Util/Errores.c: no reports, they end the game (Con_FatalErrorf) or open a message box
for c in (0, 1, 2, 3, 49, 50, 199, 200, -1, -2):
	call('Err_Str', 0x27cdd0, 'si', c)
errtext = 'sdtest error'
code = SDTest.Call(0x27ce10, 'is', errtext)[0]
check('Err_Register', lambda: code)
call('Err_Str new', 0x27cdd0, 'si', code)
call('Err_Unregister', 0x27cda0, 'si', code)
call('Err_Unregister again', 0x27cda0, 'si', code)
call('Err_Unregister 10', 0x27cda0, 'si', 10)
call('Err_Unregister 200', 0x27cda0, 'si', 200)
call('Dbg_FrameRect', 0x27cc30, 'viiiii', 1, 2, 30, 40, 0x123456)
call('Dbg_FillRect', 0x27ccf0, 'viiiii', 1, 2, 30, 40, 0x654321)

# engine3d Util/Listadin.c: elements of 8 bytes written with Cad_Copy; addresses differ between the runs, so only
# offsets and contents are compared
def ldin(d, contents=1):
	n = SDTest.Peek(d + 12, '@i')
	r = [SDTest.Peek(d + 8, '@i'), n, SDTest.Peek(d + 16, '@i'), SDTest.Peek(d + 20, '@i') & 0xffff,
		SDTest.Peek(d + 22, '@b'), SDTest.Peek(d + 4, '@i') != 0]
	for i in range(n * contents):
		r.append(SDTest.Peek(SDTest.Call(0x2d4bc0, 'iii', d, i)[0], '@s'))
	return tuple(r)
# new elements are not initialized: their contents are written before the next look
def lstep(name, rva, sig, d, *args):
	def f(rva=rva, sig=sig, d=d, args=args):
		r = apply(SDTest.Call, (rva, sig, d) + args)[0]
		if sig[0] == 'i' and r and rva in (0x2d4cb0, 0x2d4d50, 0x2d4d70, 0x2d50d0):
			r = r - SDTest.Peek(d + 4, '@i')
		return (r, ldin(d, rva not in (0x2d4d50, 0x2d4d70, 0x2d50d0)))
	check(name + fmt(args), f)
def put(d, i, text):
	SDTest.Call(0x22cf50, 'sisi', SDTest.Call(0x2d4bc0, 'iii', d, i)[0], text, 8)

d = SDTest.Call(0x2d4b60, 'iii', 8, 2)[0]
check('LDin_NewBlock', lambda: ldin(d))
lstep('LDin_Count', 0x2d4c60, 'ii', d)
lstep('LDin_Add', 0x2d4d70, 'iii', d, 3)
for i in range(3):
	put(d, i, 'e%d' % i)
lstep('LDin_AddOne', 0x2d4d50, 'ii', d)
put(d, 3, 'e3')
lstep('LDin_Last', 0x2d4cb0, 'ii', d)
lstep('LDin_Insert', 0x2d50d0, 'iii', d, 1)
put(d, 1, 'ins')
lstep('LDin_Insert', 0x2d50d0, 'iii', d, 0)
put(d, 0, 'first')
check('LDin', lambda: ldin(d))
lstep('LDin_Move', 0x2d5170, 'viii', d, 0, 3)
lstep('LDin_Move', 0x2d5170, 'viii', d, 4, 1)
lstep('LDin_Move', 0x2d5170, 'viii', d, -1, 0)
lstep('LDin_Move', 0x2d5170, 'viii', d, 2, -1)
lstep('LDin_Move', 0x2d5170, 'viii', d, 2, 2)
lstep('LDin_Copy', 0x2d4e50, 'viiii', d, 0, 3, 2)
lstep('LDin_Copy', 0x2d4e50, 'viiii', d, 3, 1, 2)
lstep('LDin_Copy', 0x2d4e50, 'viiii', d, 2, 2, 1)
lstep('LDin_Delete', 0x2d4f50, 'vii', d, 1)
lstep('LDin_Delete', 0x2d4f50, 'vii', d, 3)
lstep('LDin_RemoveLast', 0x2d5000, 'vi', d)
lstep('LDin_RemoveLast', 0x2d5000, 'vi', d)
lstep('LDin_RemoveLast', 0x2d5000, 'vi', d)
lstep('LDin_RemoveLast', 0x2d5000, 'vi', d)
lstep('LDin_Add', 0x2d4d70, 'iii', d, 5)
lstep('LDin_Clear', 0x2d52d0, 'vi', d)
lstep('LDin_Add', 0x2d4d70, 'iii', d, 1)
check('LDin_Free', lambda: SDTest.Call(0x2d4b00, 'vi', d))
d = SDTest.Call(0x2d4b20, 'ii', 4)[0]
check('LDin_New', lambda: ldin(d))
lstep('LDin_Add', 0x2d4d70, 'iii', d, 300)
check('LDin_Count', lambda: SDTest.Call(0x2d4c60, 'ii', d))
lstep('LDin_Clear', 0x2d52d0, 'vi', d)
check('LDin_Free', lambda: SDTest.Call(0x2d4b00, 'vi', d))

# engine3d Util/Arch_dat.c and Dat_simp.c: lines added one by one (Arch_Load only reads files in the packs)
inilines = ('; head comment', '[Main]', 'Name = Scrap  ; the name', '  Count=12', 'flag = SI', 'Float = 1.5', 'Big = 1e3',
	'Word = 65000', 'Item = one', 'Item = two words  ', 'Empty =', 'Semi = ;only comment', 'NoEq', '', '[ Other ]  ',
	'name = other\t;tab', '[]', 'Item = three')
def arch(dat):
	r = []
	for i in range(SDTest.Call(0x2a2b80, 'ii', dat)[0] + 1):
		lin = SDTest.Call(0x2a2d70, 'iii', dat, i)[0]
		r.append((SDTest.Call(0x2a2e50, 'iii', dat, i)[0], SDTest.Peek(lin + 12, '@p')))
	return tuple(r)
# the first argument (the cArchDatos address) is not part of the name
def dcall(name, rva, sig, *args):
	check(name + fmt(args[1:]), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))
def astep(name, rva, sig, *args):
	check(name + fmt(args[1:]), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), arch(args[0])))

dat = SDTest.Call(0x2a2700, 'i')[0]
check('Arch_New', lambda: arch(dat))
for i in range(len(inilines)):
	SDTest.Call(0x2a1b80, 'visi', dat, inilines[i], i)
check('Arch_AddLine', lambda: arch(dat))
for n in ('Main', 'main', 'Other', 'other', ' Other', 'Oth', '', 'None'):
	for f in (0, 1, 3, 16):
		dcall('Arch_FindSection', 0x2a1df0, 'iisi', dat, n, f)
for k in ('Name', 'name', 'Count', 'Item', 'Empty', 'Semi', 'NoEq', 'Nam', 'Missing'):
	for f in (0, 2, 9, 10):
		dcall('Arch_FindKey', 0x2a2010, 'iisibi', dat, k, f, '', 260)
		dcall('Arch_FindKey NULL', 0x2a2010, 'iisizi', dat, k, f, None, 0)
	dcall('Arch_FindKey short', 0x2a2010, 'iisibi', dat, k, 2, '', 4)
for i in range(-1, 21):
	dcall('Arch_SectionEnd', 0x2a1d10, 'iii', dat, i)
	def val(i=i):
		r = SDTest.Call(0x2a28d0, 'iiip', dat, i, -5)
		return r[0] and (SDTest.Peek(r[0], '@s'), r[1]) or r
	check('Arch_Value(%d)' % i, val)
	check('Arch_Value NULL(%d)' % i, lambda i=i: SDTest.Call(0x2a28d0, 'iiiz', dat, i, None)[0] != 0)
for h in ('[abc]', '  [ a b ]  x', '[x]', '[\tlong name\t]'):
	call('Arch_SectionName', 0x2a2260, 'vbsi', '', h, 260)
	call('Arch_SectionName short', 0x2a2260, 'vbsi', '', h, 3)
dcall('Arch_StrnCmp', 0x2a1ce0, 'iissi', dat, 'ABc', 'abd', 2)
astep('Arch_SetValue', 0x2a2a00, 'visi', dat, 'Scrapland', 3)
astep('Arch_SetValue long', 0x2a2a00, 'visi', dat, 'a much longer value than before', 3)
astep('Arch_SetValue', 0x2a2a00, 'visi', dat, 'x', 4)
astep('Arch_SetValue none', 0x2a2a00, 'visi', dat, 'x', 2)
astep('Arch_AddSection', 0x2a1ac0, 'visi', dat, 'New', 99)
astep('Arch_AddLine', 0x2a1b80, 'visi', dat, 'k = v', -3)
astep('Arch_Delete', 0x2a2460, 'vii', dat, 1)
astep('Arch_Delete 0', 0x2a2460, 'vii', dat, 0)
astep('Arch_Delete big', 0x2a2460, 'vii', dat, 999)
astep('Dat_SetStr', 0x2a2f90, 'visss', dat, 'Main', 'Name', 'Changed')
astep('Dat_SetStr same', 0x2a2f90, 'visss', dat, 'Main', 'Name', 'Changed')
astep('Dat_SetStr new key', 0x2a2f90, 'visss', dat, 'Other', 'Added', 'yes')
astep('Dat_SetStr new section', 0x2a2f90, 'visss', dat, 'Fresh', 'k', 'v')
astep('Dat_SetBool', 0x2a2f00, 'vissi', dat, 'Fresh', 'b', 1)
astep('Dat_SetBool', 0x2a2f00, 'vissi', dat, 'Fresh', 'b', 0)
astep('Dat_SetInt', 0x2a2f40, 'vissi', dat, 'Fresh', 'n', -1234567)
# Proc_Scan of the original stops the game on a value of another type: only fitting keys and missing ones
miss = (('None', 'x'), ('Main', 'x'))
for sect, key in (('Main', 'flag'), ('Fresh', 'b')) + miss:
	dcall('Dat_GetBool', 0x2a30c0, 'iissp', dat, sect, key, 7)
# Count is 'x' since the Arch_SetValue checks
for sect, key in (('Main', 'Word'), ('Fresh', 'n')) + miss:
	dcall('Dat_GetLong', 0x2a3270, 'iissp', dat, sect, key, 7)
	dcall('Dat_GetInt', 0x2a3300, 'iissp', dat, sect, key, 7)
for sect, key in (('Main', 'Float'), ('Main', 'Big'), ('Main', 'Word')) + miss:
	dcall('Dat_GetFloat', 0x2a31e0, 'iissp', dat, sect, key, 7)
	dcall('Dat_GetDouble', 0x2a3150, 'iissD', dat, sect, key, 7.5)
for sect, key in (('Main', 'Word'),) + miss:
	dcall('Dat_GetWord', 0x2a3550, 'iissp', dat, sect, key, 7)
for sect, key in (('Main', 'flag'), ('Main', 'Count'), ('Main', 'Name'), ('Main', 'Item'), ('Main', 'Empty'), ('Main', 'Semi'),
		('Other', 'name'), ('Fresh', 'n')) + miss:
	dcall('Dat_GetText', 0x2a3390, 'iissbi', dat, sect, key, '', 8)
	dcall('Dat_GetStr', 0x2a35e0, 'iissbi', dat, sect, key, '', 260)
lst = SDTest.Call(0x2a2700, 'i')[0]
SDTest.Call(0x2a3430, 'vissi', dat, 'Main', 'Item', lst)
check('Dat_GetList', lambda: arch(lst))
SDTest.Call(0x2a2660, 'vi', lst)
SDTest.Call(0x2a2bd0, 'viii', dat, 0, 5)
f = SDTest.Call(0x344c5e, 'iss', 'sdini.out.txt', 'w')[0]
check('Arch_Write', lambda: SDTest.Call(0x2a2590, 'iii', dat, f))
SDTest.Call(0x34490b, 'ii', f)
check('Arch_Write file', lambda: open('sdini.out.txt').read())
SDTest.Call(0x2a2660, 'vi', dat)
dat = SDTest.Call(0x2a2700, 'i')[0]
astep('Arch_Load', 0x2a2780, 'iisi', dat, 'm3d.ini', 0)
dcall('Arch_Load missing', 0x2a2780, 'iisi', dat, 'sdnofile.txt', 0)
dcall('Dat_GetBool', 0x2a30c0, 'iissp', dat, 'video', 'Sharpen', 7)
dcall('Dat_GetLong', 0x2a3270, 'iissp', dat, 'suelo', 'suelo_caras_cuadr', 7)
SDTest.Call(0x2a2660, 'vi', dat)

# engine3d Util/Proc_dat.c: no bad numbers or identifiers, those open a message box in both (a note like "A#4"
# is one: the identifier ends at the '#')
for src, f, sig, args in (('12', '&l', 'issp', (0,)), ('  -5 , 7', '&l,&l', 'isspp', (0, 0)), ('3.25', '&f', 'issp', (0,)),
		('1e3', '&g', 'issD', (0.0,)), ('200', '&b', 'issp', (0,)), ('-100', '&c', 'issp', (0,)), ('30000', '&s', 'issp', (0,)),
		('65000', '&w', 'issp', (0,)), ('4000000000', '&d', 'issp', (0,)), ('SI', '&(tipo_si_no)l', 'issp', (7,)),
		('off', '&(tipo_si_no)l', 'issp', (7,)), ('True', '&(tipo_si_no)b', 'issp', (0,)), ('GLOBAL', '&(tipo_bucle)l', 'issp', (7,)),
		('C4', '&(tipo_nota)l', 'issp', (7,)), ('Bb3', '&(tipo_nota)l', 'issp', (7,)), ('gx2', '&(tipo_nota)l', 'issp', (7,)),
		('ST', '&(tipo_sample_mnst)w', 'issp', (7,)), ('hello world , next', '&t,&t', 'issbb', ('', '')),
		('hello world', '&.*t', 'issib', (5, '')), ('hello world', '&.3t', 'issb', ('',)), ('a , b', '&.0t,&t', 'issbb', ('', '')),
		('12 34', '&l &l', 'isspp', (0, 0)), ('12x', '&lx&l', 'isspp', (0, 0)), ('12', '&l &l', 'isspp', (0, 0)),
		('12,', '&l,&l', 'isspp', (0, 0)), ('12;3', '&l,&l', 'isspp', (0, 0)), ('1', '&', 'iss', ()), ('1', '&(tipo_si_no', 'iss', ()),
		('1', '&.', 'iss', ()), ('1', '&.5', 'iss', ()), ('1', '&()l', 'issp', (7,)), ('', '&l', 'issp', (7,)), ('5', '', 'iss', ()),
		(' 7', '&l', 'issp', (0,)), ('8 ', ' &l', 'issp', (0,)), ('x9', 'x&l', 'issp', (0,)), ('9', '&l&l', 'isspp', (0, 0)),
		('1 2 3', '&l&l', 'isspp', (0, 0)), ('0x10', '&l', 'issp', (0,)), ('-0', '&w', 'issp', (5,)), ('1.5e-2', '&f&t', 'issbb', ('', ''))):
	if sig[-1] == 'b' and f == '&f&t':
		sig = 'isspb'
		args = (0, '')
	apply(call, ('Proc_Scan', 0x265260, sig, src, f) + args)
for n in ('C4', 'a0', 'G#9', 'Bb3', 'Ex-1', 'C', 'H2', 'C#b4', 'D##2'):
	call('Proc_Note', 0x264a60, 'ibszi', '', n, None, 16)
sino = SDTest.Peek(0x4324b8 + 4, 'i')
for n in ('si', 'YES', 'false', 'Nope', 'S', ''):
	call('Proc_TypeLookup', 0x264c00, 'ibsii', '', n, sino, 260)
	call('Proc_TypeLookup short', 0x264c00, 'ibsii', '', n, sino, 1)
sdtipo = 'sdtipo'
bucle = SDTest.Peek(0x4324b8 + 16, 'i')
check('Proc_DefineType', lambda: SDTest.Call(0x2651b0, 'isi', sdtipo, bucle)[0])
check('Proc_DefineType again', lambda: SDTest.Call(0x2651b0, 'isi', sdtipo, bucle)[0])
check('Proc_DefineType taken', lambda: SDTest.Call(0x2651b0, 'isi', 'tipo_si_no', bucle)[0])
call('Proc_Scan sdtipo', 0x265260, 'issp', 'LOCAL', '&(sdtipo)l', 7)
for i in range(16):
	if SDTest.Peek(0x4324b8 + 12 * i, 'p') == 'sdtipo':
		SDTest.Poke(0x4324b8 + 12 * i, 0)
check('Proc types', lambda: map(lambda i: SDTest.Peek(0x4324b8 + 12 * i, 'p'), range(16)))

# engine3d Engine/ArchIO.c: reads only (writing blocks and seeking need the pack building mode, else a CHECK
# fails), CArchIO on a block file of the packs
for t in ('ABCD', 'AB', '', 'ABCDEF', 'DUM'):
	call('IdToDword', 0x21bb00, 'is', t)
for n in ('Bin/Scrap.exe', 'Bin', 'sdnofile.txt', 'm3d.ini'):
	call('File_Access', 0x21bb60, 'isi', n, 0)
	call('File_AccessPack', 0x21bae0, 'isi', n, 0)
	call('File_Check', 0x21c4f0, 'isii', n, 0, 1)
	call('File_Stat', 0x21bd90, 'isb', n, '')
	call('File_StatPack', 0x21bf60, 'isb', n, '')
call('File_RelPath', 0x21b450, 'vbs', '', 'a/b/c.txt')
def readall(n):
	r = SDTest.Call(0x21b330, 'iszp', n, None, 0)
	text = SDTest.Call(0x22cf50, 'sbii', '', r[0], 40)[1]
	return (r[1], text)
check('ArchIO_ReadAll', lambda: readall('m3d.ini'))
f = SDTest.Call(0x21bdd0, 'iss', 'm3d.ini', 'rb')[0]
check('ArchIO_Open', lambda: f != 0)
call('ArchIO_Open missing', 0x21bdd0, 'iss', 'sdnofile.txt', 'rb')
check('ArchIO_Size', lambda: SDTest.Call(0x21ba70, 'ii', f))
for i in range(4):
	check('ArchIO_Gets', lambda: (SDTest.Call(0x21b8e0, 'sbii', '', 260, f), SDTest.Call(0x21ba80, 'ii', f)))
check('ArchIO_Gets short', lambda: SDTest.Call(0x21b8e0, 'sbii', '', 4, f))
check('ArchIO_Getc', lambda: SDTest.Call(0x21b8a0, 'ii', f))
check('ArchIO_Read', lambda: SDTest.Call(0x21b9d0, 'ibiii', '', 3, 4, f))
check('ArchIO_Tell', lambda: SDTest.Call(0x21ba80, 'ii', f))
check('ArchIO_Close', lambda: SDTest.Call(0x21b850, 'ii', f))
io = SDTest.Call(0x1335a0, 'ii', 16)[0]
def cio():
	return (SDTest.Peek(io, '@b'), SDTest.Peek(io + 4, '@i'), SDTest.Peek(io + 8, '@i') != 0, SDTest.Peek(io + 12, '@i'))
def iostep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, io) + args), cio()))
check('CArchIO_ctor missing', lambda: (SDTest.Call(0x21b1f0, 'tiiss', io, 'sdnofile.dum', 'rb')[0] == io, cio()))
iostep('CArchIO_Error', 0x21c250, 'tiiz', None)
iostep('CArchIO_Read error', 0x21c060, 'tiibi', '', 4)
iostep('CArchIO_ReadString error', 0x21c0b0, 'tiibi', '', 4)
iostep('CArchIO_Unsupported', 0x21b990, 'tii')
iostep('CArchIO_dtor', 0x21b290, 'tvi')
check('CArchIO_ctor', lambda: (SDTest.Call(0x21b1f0, 'tiiss', io, 'levels/fake/map/map3d.dum', 'rb')[0] == io, cio()))
iostep('CArchIO_BeginBlockSize', 0x21c2f0, 'tiisp', 'DUM', 0)
iostep('CArchIO_ReadInt', 0x21bff0, 'tii')
iostep('CArchIO_ReadBool', 0x21bf80, 'tbi')
iostep('CArchIO_Read4', 0x21c350, 'tiip', 0)
iostep('CArchIO_ReadStringNull', 0x21c180, 'tiibip', '', 260, 7)
iostep('CArchIO_Read', 0x21c060, 'tiibi', '', 12)
iostep('CArchIO_Read 0', 0x21c060, 'tiibi', '', 0)
iostep('CArchIO_SetError', 0x21c410, 'tiis', 'test')
iostep('CArchIO_Read after', 0x21c060, 'tiibi', '', 4)
iostep('CArchIO_ReadStringNull after', 0x21c180, 'tiibip', '', 260, 7)
iostep('CArchIO_Finish', 0x21b440, 'tii')
iostep('CArchIO_Close', 0x21b490, 'tii')
SDTest.Call(0x133550, 'vi', io)

# Math/MathUtils.c
execfile('sdmath.py')
def fcall(name, rva, sig, *args):
	check(name, lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))
for i in range(40):
	a, b, c, p = vec(), vec(), vec(), vec()
	box = (min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2]), max(a[0], b[0]), max(a[1], b[1]), max(a[2], b[2]))
	box2 = (c[0], c[1], c[2], p[0], p[1], p[2])
	fcall('Box_GetCorner', 0x15b610, 'tvFFi', box, (9.0, 9.0, 9.0), i % 10 - 1)
	fcall('Box_Merge', 0x15b720, 'vFFi', box, box2, i % 3 == 0)
	t = rnd(-1.0, 1.0)
	fcall('Float_Approach', 0x15b7c0, 'vFff', (a[0],), b[0], t)
	fcall('Vec3_Approach', 0x15b800, 'vFFf', a, b, t * 3)
	fcall('Vec3_Bounce', 0x15b890, 'vFffFF', a, rnd(0.1, 5), rnd(0.1, 5), b, c)
	fcall('Tri_HasPoint', 0x15b9e0, 'bFFFF', p, a, b, c)
	fcall('Tri_HasPoint in', 0x15b9e0, 'bFFFF', ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3), a, b, c)
	fcall('Dist_PointSegment', 0x15bc60, 'fFFFF', a, b, p, (0.0, 0.0, 0.0))
	fcall('Dist2_PointSegment', 0x15bf60, 'fFFFF', a, b, p, (0.0, 0.0, 0.0))
	fcall('Plane_SafeSet', 0x15c1b0, 'tbFFFF', (0.0, 0.0, 0.0, 0.0), a, b, c)
	fcall('Quad_Solve', 0x15c370, 'bfffFF', rnd(), rnd(), rnd(), (0.0,), (0.0,))
	fcall('Dist_PointTriangle', 0x15c420, 'fFFFFF', p, a, b, c, (0.0, 0.0, 0.0))
	fcall('Dist2_PointTriangle', 0x15c460, 'fFFFFF', p, a, b, c, (0.0, 0.0, 0.0))
	fcall('Vec3_Reflect', 0x15c610, 'vFFF', a, b, (0.0, 0.0, 0.0))
for x in specials:
	v = (x, 1.0, x)
	fcall('Float_IsValid', 0x15bbc0, 'bf', x)
	fcall('Vec3_IsValid', 0x15bbf0, 'bF', v)
	fcall('Float_Approach s', 0x15b7c0, 'vFff', (x,), 1.0, 0.5)
	fcall('Dist_PointSegment s', 0x15bc60, 'fFFFF', v, (1.0, 2.0, 3.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
	fcall('Dist_PointSegment s2', 0x15bc60, 'fFFFF', (0.0, 0.0, 0.0), (1.0, 2.0, 3.0), v, (0.0, 0.0, 0.0))
	fcall('Dist2_PointSegment s', 0x15bf60, 'fFFFF', (0.0, 0.0, 0.0), v, (1.0, 2.0, 3.0), (0.0, 0.0, 0.0))
	fcall('Plane_SafeSet s', 0x15c1b0, 'tbFFFF', (0.0, 0.0, 0.0, 0.0), v, (1.0, 2.0, 3.0), (3.0, 1.0, 2.0))
	fcall('Quad_Solve s', 0x15c370, 'bfffFF', x, 2.0, x, (0.0,), (0.0,))
	fcall('Tri_HasPoint s', 0x15b9e0, 'bFFFF', v, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
fcall('Plane_SafeSet degenerate', 0x15c1b0, 'tbFFFF', (0.0, 0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (2.0, 2.0, 2.0), (3.0, 3.0, 3.0))
fcall('Dist2_PointSegment point', 0x15bf60, 'fFFFF', (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (3.0, 1.0, 2.0), (0.0, 0.0, 0.0))

# Math/Matrix.c, Quat.c and the rotations of MathUtils.c
ident = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
def mat():
	r = []
	for i in range(16):
		r.append(rnd(-3.0, 3.0))
	return tuple(r)
def rotmat(ang):
	return SDTest.Call(0x15d100, 'vFF', ident, ang)[1]
def affine():
	m = list(rotmat(vec()))
	m[12], m[13], m[14] = rnd(), rnd(), rnd()
	return tuple(m)
def quat():
	return (rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0))
zero3 = (0.0, 0.0, 0.0)
for i in range(30):
	a, b, m, ang = mat(), mat(), affine(), vec()
	fcall('Matrix_Identity', 0x125650, 'tvF', a)
	fcall('Matrix_Copy', 0x15c680, 'tvFF', a, b)
	fcall('Matrix_Copy 2', 0x15c830, 'tvFF', a, b)
	fcall('Matrix_FromFloats', 0x15c7a0, 'tvFF', a, b)
	fcall('Matrix_Mul', 0x15c8a0, 'tvFFF', a, ident, b)
	fcall('Matrix_Rotate', 0x15d100, 'vFF', a, ang)
	fcall('Matrix_GetEuler', 0x15cbe0, 'tvFF', rotmat(ang), zero3)
	fcall('Matrix_GetEuler any', 0x15cbe0, 'tvFF', a, zero3)
	fcall('Matrix_GetRotAng', 0x15ce00, 'tvFF', a, zero3)
	fcall('Matrix_InvertAffine', 0x15ce40, 'bFF', ident, m)
	fcall('Matrix_InvertAffine any', 0x15ce40, 'bFF', ident, a)
	fcall('Matrix_Scale', 0x15d290, 'tvFF', a, ang)
	fcall('Matrix_Perspective', 0x15d350, 'bFffff', a, rnd(0.1, 3.0), rnd(0.5, 2.0), rnd(0.1, 10.0), rnd(5.0, 100.0))
	fcall('Matrix_RotX', 0x15d430, 'tvFf', a, ang[0])
	fcall('Matrix_RotY', 0x15d490, 'tvFf', a, ang[1])
	fcall('Matrix_RotZ', 0x15d4f0, 'tvFf', a, ang[2])
	fcall('Matrix_RotAxis', 0x15d550, 'tvFFf', a, vec(), ang[0])
	fcall('Matrix_Translate', 0x15d750, 'tvFF', a, ang)
	fcall('Matrix_TransformProject', 0x15d790, 'bFFF', zero3, vec(), a)
	fcall('Matrix_TransformDir', 0x15d880, 'vFFF', zero3, vec(), a)
	fcall('Matrix_TransformParts', 0x15d910, 'vFFFFFF', (0.0,), (0.0,), (0.0,), (0.0,), vec(), a)
	fcall('Matrix_TransformParts NULL', 0x15d910, 'vzFzFFF', None, (0.0,), None, (0.0,), vec(), a)
	q1, q2 = quat(), quat()
	fcall('Quat_Mul', 0x15d9f0, 'vFFF', (0.0, 0.0, 0.0, 0.0), q1, q2)
	fcall('Quat_FromAxisAngle', 0x15dad0, 'vFFf', (0.0, 0.0, 0.0, 0.0), vec(), ang[1])
	fcall('Quat_ToAxisAngle', 0x15db50, 'vFFF', zero3, (0.0,), q1)
	fcall('Quat_ToMatrix', 0x15dbf0, 'vFF', a, q1)
	fcall('Quat_Slerp', 0x15dd40, 'vFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, rnd(0.0, 1.0))
	fcall('Quat_Slerp same', 0x15dd40, 'vFFFf', (0.0, 0.0, 0.0, 0.0), q1, q1, rnd(0.0, 1.0))
	v = vec()
	fcall('Vec3_GetRotAng', 0x15df10, 'vFF', zero3, v)
	fcall('Vec3_RotateSC', 0x15dfb0, 'vFFFF', zero3, v, vec(), vec())
	fcall('Vec3_RotateInv', 0x15e060, 'vFFF', zero3, v, ang)
	fcall('Vec3_NormAng', 0x15e190, 'vFF', zero3, (ang[0] * 3, ang[1] * 7, ang[2]))
	fcall('NormAng', 0x15e2e0, 'ff', ang[0] * 5)
	fcall('Vec2_Rotate', 0x15e350, 'vFFf', (0.0, 0.0), (v[0], v[1]), ang[2])
	fcall('Vec3_Rotate', 0x15e3b0, 'vFFF', zero3, v, ang)
	fcall('Vec3_RotateXY', 0x15e4d0, 'vFFff', zero3, v, ang[0], ang[1])
for x in (0.0, 3.1415925, 3.1415927, -3.1415927, 6.2831855, -6.2831855, 1e10, -1e-30, inf, nan):
	fcall('NormAng s', 0x15e2e0, 'ff', x)
fcall('Matrix_GetEuler up', 0x15cbe0, 'tvFF', rotmat((1.5707964, 0.3, 0.2)), zero3)
fcall('Matrix_GetEuler down', 0x15cbe0, 'tvFF', rotmat((-1.5707964, 0.3, 0.2)), zero3)
fcall('Matrix_Perspective flat', 0x15d350, 'bFffff', ident, 1.0, 1.0, 5.0, 5.001)
fcall('Matrix_Perspective narrow', 0x15d350, 'bFffff', ident, 0.0, 1.0, 1.0, 10.0)
fcall('Matrix_TransformProject w0', 0x15d790, 'bFFF', zero3, (1.0, 2.0, 3.0), (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0))
fcall('Quat_ToAxisAngle identity', 0x15db50, 'vFFF', zero3, (0.0,), (0.0, 0.0, 0.0, 1.0))
# the engine3d copies and the more of Engine/cuaternio.cpp; unit quaternions from Quat_FromEuler
def uquat():
	a = vec()
	return SDTest.Call(0x2a4040, 'vFfff', (0.0, 0.0, 0.0, 0.0), a[0], a[1], a[2])[1]
for i in range(30):
	a, ang = mat(), vec()
	fcall('Quat_FromEuler', 0x2a4040, 'vFfff', (0.0, 0.0, 0.0, 0.0), ang[0], ang[1], ang[2])
	q1, q2 = uquat(), uquat()
	fcall('Quat_ToAxisAngle 2', 0x2a4160, 'vFFF', zero3, (0.0,), q1)
	fcall('Quat_Slerp 2', 0x2a45e0, 'vFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, rnd(0.0, 1.0))
	fcall('Quat_DotAbs', 0x2a4250, 'fFF', q1, q2)
	fcall('Quat_DotAbs same', 0x2a4250, 'fFF', q1, q1)
	fcall('Quat_ToMatrixD3D', 0x2a4200, 'vFF', a, q1)
	fcall('Quat_SlerpFlip', 0x2a4310, 'bFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, rnd(0.0, 1.0))
	fcall('Quat_SlerpFlip near', 0x2a4310, 'bFFFf', (0.0, 0.0, 0.0, 0.0), q1, q1, rnd(0.0, 1.0))
	fcall('Quat_SlerpFlip any', 0x2a4310, 'bFFFf', (0.0, 0.0, 0.0, 0.0), quat(), quat(), rnd(0.0, 1.0))
# Engine/calculo3d.cpp
for i in range(30):
	a, v, w = mat(), vec(), vec()
	fcall('Matrix_AddPos', 0x23a4d0, 'vFF', a, v)
	fcall('Vec3_TransformH', 0x23a510, 'iFFF', zero3, v, a)
	fcall('Vec3_TransformDir0', 0x23a5f0, 'iFFF', zero3, v, a)
	fcall('Sph_InCone', 0x23a6f0, 'bFfFFf', v, rnd(0.0, 3.0), w, vec(), rnd(0.0, 3.0))
	fcall('Vec3_StrDeg', 0x23a9b0, 'sFii', v, i % 4, i % 7)
	fcall('Vec3_Str', 0x23aa60, 'sFii', v, (i + 1) % 4, i % 9)
for c, r, a, d, ang in (((0.0, 0.0, 5.0), 1.0, zero3, (0.0, 0.0, 1.0), 0.5), ((0.0, 5.0, 0.0), 1.0, zero3, (0.0, 0.0, 1.0), 0.5),
		((0.0, 0.0, 5.0), 6.0, zero3, (0.0, 0.0, 1.0), 0.1), ((1.0, 1.0, 1.0), 0.5, zero3, zero3, 1.0),
		((0.0, 3.0, 4.0), 5.0, zero3, (1.0, 0.0, 0.0), 0.0), ((0.0, 0.0, -5.0), 0.1, zero3, (0.0, 0.0, 2.0), 3.1)):
	fcall('Sph_InCone s', 0x23a6f0, 'bFfFFf', c, r, a, d, ang)
SDTest.Call(0x323c50, 'vi', 77)
for mn, mx, av, tr in ((0, 10, 3, 5), (5, 5, 5, 3), (5, 5, 4, 3), (-3, 3, 0, 0), (0, 1, 0, 1), (0, 1, 1, 20), (100, 200, 150, 2)):
	call('Rand_Other', 0x23a680, 'iiiii', mn, mx, av, tr)
mex = SDTest.Call(0x133650, 'ii', 64)[0]
mname = SDTest.Call(0x133650, 'ii', 16)[0]
SDTest.Call(0x1365c0, 'sisi', mname, 'sqrt', 16)
for t, hi in ((1, 0), (3, 0x3ff00000), (6, -0x40100000), (0, 0), (9, 0x40590000)):
	SDTest.Poke(mex - 0x400000, t)
	SDTest.Poke(mex + 4 - 0x400000, mname)
	SDTest.Poke(mex + 12 - 0x400000, hi)
	check('E3D_MathErr' + fmt((t, hi)), lambda: SDTest.Call(0x23aae0, 'ii', mex))
call('E3D_MathErr NULL', 0x23aae0, 'ii', 0)
SDTest.Call(0x133550, 'vi', mex)
SDTest.Call(0x133550, 'vi', mname)
for q1, q2 in (((0.0, 0.0, 0.9, 0.1), (0.0, 0.0, -0.9, 0.1)), ((0.7, 0.0, 0.0, -0.7), (0.0, 0.7, 0.0, 0.7)),
		((0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0, -1.0)), ((0.0, 0.99, 0.0, -0.14), (0.0, -0.98, 0.0, -0.2))):
	for t in (0.0, 0.3, 1.0):
		fcall('Quat_SlerpFlip s', 0x2a4310, 'bFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, t)

# engine3d Engine/ArchINI.c on two ini files of the packs
ini = SDTest.Call(0x1335a0, 'ii', 4)[0]
def istep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig, ini) + args))
check('CArchINI_ctorLoad', lambda: SDTest.Call(0x2618e0, 'tiisz', ini, 'm3d.ini', None)[0] == ini)
istep('CArchINI_Load', 0x2621c0, 'tiiszi', 'map3d.ini', 'levels/menu/map/', 1)
istep('CArchINI_Load missing', 0x2621c0, 'tiiszi', 'sdnofile.ini', None, 0)
istep('CArchINI_Count', 0x261bc0, 'tii')
for sect, key in (('video', 'VSync'), ('video', 'Sharpen'), ('video', 'TextBPP'), ('suelo', 'suelo_caras_cuadr'),
		('mapa_sombras', 'escala'), ('mapa_sombras', 'sombras_suaves'), ('suelo', 'model_ambient'), ('Model', 'TexturePath'),
		('cielo', 'path'), ('none', 'x'), ('video', 'none')):
	if key in ('VSync', 'Sharpen', 'sombras_suaves', 'none', 'x'):
		istep('CArchINI_GetBool', 0x261af0, 'tbissi', sect, key, 7)
		istep('CArchINI_ReadBool', 0x262450, 'tiissp', sect, key, 7)
	if key in ('TextBPP', 'suelo_caras_cuadr', 'none', 'x'):
		istep('CArchINI_GetInt', 0x261b90, 'tiissi', sect, key, -5)
		istep('CArchINI_ReadInt', 0x262490, 'tiissp', sect, key, 7)
	if key in ('escala', 'TextBPP', 'none', 'x'):
		istep('CArchINI_GetFloat', 0x261b60, 'tfissf', sect, key, 2.5)
		istep('CArchINI_ReadFloat', 0x262470, 'tiissp', sect, key, 7)
	if key in ('model_ambient', 'none', 'x'):
		istep('CArchINI_GetColor', 0x261bd0, 'tiissi', sect, key, 0x11223344)
	istep('CArchINI_GetText', 0x261d00, 'tsissbiz', sect, key, '', 40, None)
	istep('CArchINI_GetText def', 0x261d00, 'tsissbis', sect, key, '', 40, 'default')
	istep('CArchINI_ReadText', 0x2624b0, 'tiissbi', sect, key, '', 6)
	if key in ('none', 'x'):
		istep('CArchINI_GetVec2', 0x261d50, 'tviFssF', (0.0, 0.0), sect, key, (1.0, 2.0))
		istep('CArchINI_GetVec3', 0x261e10, 'tviFssF', (0.0, 0.0, 0.0), sect, key, (1.0, 2.0, 3.0))
for n in ('video', 'suelo', 'cielo', 'none'):
	istep('CArchINI_FindSection', 0x261ad0, 'tiisi', n, 0)
istep('CArchINI_FindKey', 0x261ab0, 'tiisbii', 'Depth', '', 40, 1)
for i in range(0, 12):
	istep('CArchINI_LineText', 0x2624d0, 'tsii', i)
	istep('CArchINI_LineType', 0x2624f0, 'tiii', i)
	istep('CArchINI_GetSectionName', 0x261cb0, 'tbiibi', i, '', 40)
istep('CArchINI_SetStr', 0x2621a0, 'tiisss', 'video', 'VSync', 'SI')
istep('CArchINI_SetStr same', 0x262390, 'tiisss', 'video', 'VSync', 'SI')
istep('CArchINI_SetInt', 0x262180, 'tiissi', 'video', 'Depth', 32)
istep('CArchINI_SetInt 2', 0x262370, 'tiissi', 'sd', 'n', -7)
istep('CArchINI_SetFloat', 0x262110, 'tiissfi', 'sd', 'f', 1.0 / 3, 3)
istep('CArchINI_SetFloat 2', 0x262300, 'tiissfi', 'sd', 'g', -2.5, 0)
istep('CArchINI_SetYesNo', 0x2620e0, 'tiissi', 'sd', 'y', 1)
istep('CArchINI_SetBool', 0x2622e0, 'tiissi', 'sd', 'b', 0)
istep('CArchINI_AddLine', 0x262290, 'tvisi', '; sd line', -1)
istep('CArchINI_AddLine 2', 0x262290, 'tvisi', '; sd first', 0)
istep('CArchINI_DeleteLine', 0x2622c0, 'tvii', 3)
istep('CArchINI_Save', 0x2623b0, 'tiiszi', 'sdini2.txt', None, 0)
check('CArchINI_Save file', lambda: open('sdini2.txt').read())
istep('CArchINI_Clear', 0x261fd0, 'tvi')
istep('CArchINI_Count', 0x261bc0, 'tii')
istep('CArchINI_dtor', 0x261990, 'tvi')
check('CArchINI_ctor', lambda: SDTest.Call(0x261980, 'tii', ini)[0] == ini)
istep('CArchINI_Count', 0x261bc0, 'tii')
SDTest.Call(0x261990, 'tvi', ini)
SDTest.Call(0x133550, 'vi', ini)

# engine3d Engine/TablaHash.c
th = SDTest.Call(0x1335a0, 'ii', 12)[0]
def tstep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig, th) + args))
def tnode(n):
	if not n:
		return None
	return SDTest.Peek(n + 4, '@p'), SDTest.Peek(n + 8, '@i')
check('CTablaHash_ctor', lambda: SDTest.Call(0x2d06e0, 'tiii', th, 7)[0] == th)
keys = ('a', 'b', 'abc', 'ABC', '', 'Init.py', 'x' * 40, 'a\xedb', 'h', 'o', 'v')
for i in range(len(keys)):
	tstep('CTablaHash_Slot', 0x2d0c20, 'tiis', keys[i])
	tstep('CTablaHash_Add', 0x2d0850, 'tiisii', keys[i], 100 + i, 0)
tstep('CTablaHash_Add again', 0x2d0850, 'tiisii', 'abc', 5, 0)
for k in (0, -1, 0x1234, 7):
	tstep('CTablaHash_AddInt', 0x2d0800, 'tiiiii', k, k * 3, 0)
tstep('CTablaHash_AddInt again', 0x2d0800, 'tiiiii', 0x1234, 1, 0)
for k in keys + ('c', 'Abc', '00001234', 'x' * 39):
	tstep('CTablaHash_Get', 0x2d09c0, 'tiisp', k, -9)
	tstep('CTablaHash_Get NULL', 0x2d09c0, 'tiisi', k, 0)
	check('CTablaHash_Find' + fmt((k,)), lambda k=k: tnode(SDTest.Call(0x2d0a00, 'tiisi', th, k, SDTest.Call(0x2d0c20, 'tiis', th, k)[0])[0]))
for k in (0, -1, 0x1234, 7, 8):
	tstep('CTablaHash_GetInt', 0x2d0940, 'tiiip', k, -9)
check('CTablaHash_Insert', lambda: tnode(SDTest.Call(0x2d08d0, 'tiisii', th, 'abc', 2, 55)[0]))
tstep('CTablaHash_List count', 0x2d0b00, 'tiiii', 0, 0)
def tlist():
	r = SDTest.Call(0x2d0b70, 'tiipp', th, 0, -1)
	arr, n = r[1], r[2]
	nodes = []
	for i in range(n):
		nodes.append(tnode(SDTest.Peek(arr + 4 * i, '@i')))
	small = SDTest.Call(0x2d0b00, 'tiiii', th, arr, 3)[0]
	full = SDTest.Call(0x2d0b00, 'tiiii', th, arr, n)[0]
	lock = SDTest.Peek(th + 8, '@i')
	freed = SDTest.Call(0x2d0ab0, 'tiip', th, arr)
	return r[0], n, nodes, small, full, lock, freed, SDTest.Peek(th + 8, '@i')
check('CTablaHash_GetList', tlist)
SDTest.Call(0x2d0750, 'tvi', th)
SDTest.Call(0x133550, 'vi', th)

# engine3d Engine/LeeDatos.c on an ini of the packs (only pack files open, one at a time), Lee_String on a buffer
# put in g_LeeCad
def lee():
	return SDTest.Peek(0x4e73b8, 'p'), SDTest.Peek(0x4e73bc, 'i') != 0
def leestep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), lee()))
call('Lee_Error NULL', 0x2d5da0, 'vz', None)
leestep('Lee_Open', 0x2d5c20, 'vzsi', 'levels/menu/', 'map/map3d.ini', 1)
leestep('Lee_Expect', 0x2d5a30, 'vs', '[suelo]')
leestep('Lee_Text', 0x2d5cf0, 'vzbi', 'model_ambient', '', 40)
leestep('Lee_Text short', 0x2d5cf0, 'vzbi', None, '', 5)
leestep('Lee_NextLine', 0x2d5b20, 'i')
leestep('Lee_Text', 0x2d5cf0, 'vzbi', 'TexturePath =', '', 40)
leestep('Lee_Expect', 0x2d5a30, 'vs', '[mapa_sombras]')
for t in ('escala =', 'resolucion =', 'gamma ='):
	leestep('Lee_Float', 0x2d5e00, 'vsF', t, (0.0,))
leestep('Lee_Text', 0x2d5cf0, 'vzbi', 'sombras_suaves =', '', 40)
leestep('Lee_Int', 0x2d5fa0, 'vsp', 'text_tam_x =', -1)
leestep('Lee_Int', 0x2d5fa0, 'vsp', 'text_tam_y =', -1)
leestep('Lee_Bytes', 0x2d5e70, 'vbi', '', 7)
leestep('Lee_Bytes 0', 0x2d5e70, 'vbi', '', 0)
for i in range(3):
	leestep('Lee_Byte', 0x2d5f30, 'i')
for i in range(16):
	leestep('Lee_NextLine', 0x2d5b20, 'i')
lbuf = SDTest.Call(0x1335a0, 'ii', 64)[0]
saved = SDTest.Peek(0x4e73b8, 'i')
for t in ('"abc" x', ' \t"" y', '"a b c d e f"', '\t"tab\there"'):
	for n in (1, 4, 40):
		SDTest.Call(0x1365c0, 'sisi', lbuf, t, 64)
		SDTest.Poke(0x4e73b8, lbuf)
		check('Lee_String' + fmt((t, n)), lambda n=n, lbuf=lbuf: (SDTest.Call(0x2d58a0, 'vbi', '', n), SDTest.Peek(0x4e73b8, 'i') - lbuf))
SDTest.Call(0x1365c0, 'sisi', lbuf, ' "q" rest', 64)
SDTest.Poke(0x4e73b8, lbuf)
check('Lee_QText', lambda lbuf=lbuf: (SDTest.Call(0x2d5d20, 'vzbi', None, '', 40), SDTest.Peek(0x4e73b8, 'i') - lbuf))
SDTest.Poke(0x4e73b8, saved)
SDTest.Call(0x133550, 'vi', lbuf)
leestep('Lee_Close', 0x2d5d50, 'i')

# engine3d Engine/BmpFlt.c on images in heap memory (pixels set with the exe's memcpy), TXF files written here
def bnew(w, h, create=0x2cf500):
	b = SDTest.Call(0x133650, 'ii', 12)[0]
	SDTest.Call(create, 'tviii', b, w, h)
	return b
def bset(b, vals):
	img = SDTest.Peek(b + 8, '@i')
	for i in range(0, len(vals), 64):
		part = tuple(vals[i:i + 64])
		SDTest.Call(0x3197d0, 'iiFi', img + 4 * i, part, 4 * len(part))
def bget(b, ch=4):
	w, h, img = SDTest.Peek(b, '@i'), SDTest.Peek(b + 4, '@i'), SDTest.Peek(b + 8, '@i')
	if not img:
		return w, h, None
	px = []
	for i in range(w * h * ch):
		px.append(SDTest.Peek(img + 4 * i, '@i'))
	return w, h, px
def bfree(b):
	img = SDTest.Peek(b + 8, '@i')
	if img:
		SDTest.Call(0x31364b, 'vi', img)
	SDTest.Call(0x133550, 'vi', b)
def bstep(name, b, rva, sig, *args):
	bcheck(name + fmt(args), b, rva, sig, args)
def bcheck(name, b, rva, sig, args, ch=4):
	check(name, lambda b=b, rva=rva, sig=sig, args=args, ch=ch: (apply(SDTest.Call, (rva, sig, b) + args), bget(b, ch)))
def bstep2(name, b, rva, sig, *args):
	bcheck(name + fmt(args), b, rva, sig, args, 3)
def bvals(n, lo=0.0, hi=4.0):
	v = []
	for i in range(n):
		v.append(rnd(lo, hi))
	return v
ba = bnew(5, 4)
bb = bnew(3, 2)
bset(ba, bvals(80))
bset(bb, bvals(24, -1.0, 2.0))
check('CBmpFlt_Create', lambda: bget(ba))
check('CBmpFlt_Pixel', lambda: SDTest.Call(0x290730, 'tiiii', ba, 3, 2)[0] - SDTest.Peek(ba + 8, '@i'))
for r in ((0, 0, 0, 0, 3, 2, 256, 0), (1, 1, 0, 0, 9, 9, 128, 128), (4, 3, 1, 1, 5, 5, 7, 300), (2, 0, 2, 1, 1, 1, -64, 32),
		(0, 0, 0, 0, 0, 2, 256, 256), (6, 0, 0, 0, 2, 2, 256, 256)):
	bcheck('CBmpFlt_Blend' + fmt(r), ba, 0x2cf600, 'tviiiiiiiiii', (r[0], r[1], bb) + r[2:])
for d in ((2, 2, 15), (3, 3, 15), (7, 5, 15), (5, 4, 5), (1, 1, 8), (4, 1, 3)):
	bd = bnew(d[0], d[1])
	bset(bd, bvals(d[0] * d[1] * 4))
	bcheck('CBmpFlt_Resample' + fmt(d), bd, 0x2cf7b0, 'tviii', (ba, d[2]))
	bfree(bd)
bd = SDTest.Call(0x133650, 'ii', 12)[0]
bcheck('CBmpFlt_Resample new', bd, 0x2cf7b0, 'tviii', (bb, 15))
bfree(bd)
bc = bnew(4, 2)
bstep('CBmpFlt_Fill', bc, 0x2cf170, 'tviF', (0.5, -2.0, 1e30, 0.0))
for e in ((1.0, 1.0), (2.5, 1.0), (0.5, 3.0), (-1.0, 2.0), (100.0, 0.01), (1.0, 0.0)):
	bset(bc, (0.0, 0.25, 1.0, -0.5, 3.0, 10.0, 1e-8, 700.0, -30.0, 2.0, 0.1, 0.9, 5.0, 6.0, 7.0, 8.0) * 2)
	bstep('CBmpFlt_Exposure', bc, 0x2cf2e0, 'tviff', e[0], e[1])
bfree(bc)
bstep('CBmpFlt_Save', ba, 0x2cf260, 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep('CBmpFlt_Save exists', ba, 0x2cf260, 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep('CBmpFlt_Save extra', bb, 0x2cf260, 'tiisisi', 'sdbmp2.TXF', 1, 'XYZW', 4)
bstep('CBmpFlt_Save ext', ba, 0x2cf260, 'tiisizi', 'sdbmp.txt', 1, None, 0)
bstep('CBmpFlt_Save short', ba, 0x2cf260, 'tiisizi', 'txf', 1, None, 0)
writefile('sdbad.txf', 'TXF\032' + 'x' * 40)
bl = SDTest.Call(0x133650, 'ii', 12)[0]
bstep('CBmpFlt_Load', bl, 0x2cf560, 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep('CBmpFlt_Load extra', bl, 0x2cf560, 'tiisibi', 'sdbmp2.txf', 0, '', 4)
bstep('CBmpFlt_Load extra none', bl, 0x2cf560, 'tiisibi', 'sdbmp.txf', 0, '', 4)
bstep('CBmpFlt_Load ext', bl, 0x2cf560, 'tiisizi', 'sdbmp.dat', 0, None, 0)
bstep('CBmpFlt_Load missing', bl, 0x2cf560, 'tiisizi', 'sdnone.txf', 0, None, 0)
bstep('CBmpFlt_Load bad', bl, 0x2cf560, 'tiisizi', 'sdbad.txf', 0, None, 0)
writefile('sdshort.txf', 'TXF')
check('TXF_Load short', lambda bl=bl: (SDTest.Call(0x2ceea0, 'isiizi', 'sdshort.txf', bl, 0, None, 0), bget(bl)))
bfree(bl)
bfree(ba)
bfree(bb)

# the RGB class of Engine/BmpFlt2.cpp
ba = bnew(5, 4, 0x2d0350)
bb = bnew(3, 2, 0x2d0350)
bset(ba, bvals(60))
bset(bb, bvals(18, -1.0, 2.0))
check('CBmpFlt2_Create', lambda: bget(ba, 3))
check('CBmpFlt2_Pixel', lambda: SDTest.Call(0x2906b0, 'tiiii', ba, 3, 2)[0] - SDTest.Peek(ba + 8, '@i'))
for r in ((0, 0, 0, 0, 3, 2, 256, 0), (1, 1, 0, 0, 9, 9, 128, 128), (4, 3, 1, 1, 5, 5, 7, 300), (6, 0, 0, 0, 2, 2, 256, 256)):
	bcheck('CBmpFlt2_Blend' + fmt(r), ba, 0x2d0450, 'tviiiiiiiiii', (r[0], r[1], bb) + r[2:], 3)
bd = SDTest.Call(0x133650, 'ii', 12)[0]
bcheck('CBmpFlt2_Resample new', bd, 0x2d05f0, 'tviii', (bb, 15), 3)
bfree(bd)
bc = bnew(4, 2, 0x2d0350)
bstep2('CBmpFlt2_Fill', bc, 0x2cff80, 'tviF', (0.5, -2.0, 1e30))
for k in (1.0, 2.0, -0.5, 0.0):
	bstep2('CBmpFlt2_Scale', bc, 0x2cffe0, 'tvif', k)
for e in ((1.0, 1.0), (2.5, 1.0), (0.5, 3.0), (-1.0, 2.0), (100.0, 0.01), (1.0, 0.0)):
	bset(bc, (0.0, 0.25, 1.0, -0.5, 3.0, 10.0, 1e-8, 700.0, -30.0, 2.0, 0.1, 0.9) * 2)
	bstep2('CBmpFlt2_Exposure', bc, 0x2d0130, 'tviff', e[0], e[1])
for e in ((1.0, 1.0), (2.2, 1.0), (0.5, 3.0), (-1.0, 2.0), (3.0, 0.0), (0.0, 1.0)):
	bset(bc, (0.0, 0.25, 1.0, -0.5, 3.0, 10.0, 1e-8, 700.0, -30.0, 2.0, 0.1, 0.9) * 2)
	bstep2('CBmpFlt2_Gamma', bc, 0x2d0230, 'tviff', e[0], e[1])
bfree(bc)
bstep2('CBmpFlt2_Save', ba, 0x2d00b0, 'tiisizi', 'sdbmp3.txf', 0, None, 0)
bstep2('CBmpFlt2_Save exists', ba, 0x2d00b0, 'tiisizi', 'sdbmp3.txf', 0, None, 0)
bstep2('CBmpFlt2_Save extra', bb, 0x2d00b0, 'tiisisi', 'sdbmp4.txf', 1, 'XYZW', 4)
bstep2('CBmpFlt2_Save ext', ba, 0x2d00b0, 'tiisizi', 'sdbmp.txt', 1, None, 0)
bl = SDTest.Call(0x133650, 'ii', 12)[0]
bstep2('CBmpFlt2_Load', bl, 0x2d03b0, 'tiisizi', 'sdbmp3.txf', 0, None, 0)
bstep2('CBmpFlt2_Load extra', bl, 0x2d03b0, 'tiisibi', 'sdbmp4.txf', 0, '', 4)
bstep2('CBmpFlt2_Load version 1', bl, 0x2d03b0, 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep2('CBmpFlt2_Load ext', bl, 0x2d03b0, 'tiisizi', 'sdbmp.dat', 0, None, 0)
check('TXF2_Load short', lambda bl=bl: (SDTest.Call(0x2cfca0, 'isiizi', 'sdshort.txf', bl, 0, None, 0), bget(bl, 3)))
bfree(bl)
bfree(ba)
bfree(bb)

# engine3d Engine/Bmp.c: RGBA byte images, pixels poked as ints (Poke takes an rva, the exe is at 0x400000)
def cpix():
	v = []
	for i in range(4):
		v.append(int(rnd(0.0, 256.0)))
	a = v[3]
	if a >= 128:
		a = a - 256
	return v[0] + (v[1] << 8) + (v[2] << 16) + (a << 24)
def cset(b, vals):
	img = SDTest.Peek(b + 8, '@i')
	for i in range(len(vals)):
		SDTest.Poke(img + 4 * i - 0x400000, vals[i])
def cvals(n):
	v = []
	for i in range(n):
		v.append(cpix())
	return v
def cstep(name, b, rva, sig, *args):
	bcheck(name + fmt(args), b, rva, sig, args, 1)
ca = bnew(6, 5, 0x2a9bc0)
cb = bnew(3, 4, 0x2a9bc0)
va = cvals(30)
cset(ca, va)
cset(cb, cvals(12))
check('CBmp_Create', lambda: bget(ca, 1))
check('CBmp_Pixel', lambda: SDTest.Call(0x236ca0, 'tiiii', ca, 5, 4)[0] - SDTest.Peek(ca + 8, '@i'))
for t in (0.0, 0.5, 1.0, 0.25, -0.5, 2.0, 0.999, 0.0039):
	call('Color_Lerp', 0x2a8740, 'iiif', 0x10ff8040, -0x7f00ff01, t)
for r in ((0, 0, 0, 0, 3, 4, 15), (1, 1, 0, 0, 9, 9, 5), (4, 3, 1, 1, 5, 5, 8), (2, 0, 2, 1, 1, 1, 6), (0, 0, 0, 0, 0, 2, 15), (7, 0, 0, 0, 2, 2, 15)):
	bcheck('CBmp_Copy' + fmt(r), ca, 0x2a8c10, 'tviiiiiiiii', (r[0], r[1], cb) + r[2:], 1)
for r in ((0, 0, 0, 0, 3, 2, 256, 0), (1, 1, 0, 0, 9, 9, 128, 128), (2, 1, 1, 0, 2, 3, -300, 600), (0, 0, 0, 0, 3, 4, 0x200000, -0x200000), (4, 3, 1, 1, 5, 5, 7, 300)):
	bcheck('CBmp_Blend' + fmt(r), ca, 0x2aa6c0, 'tviiiiiiiiii', (r[0], r[1], cb) + r[2:], 1)
# the game seeds rand with the time: srand of the exe
SDTest.Call(0x323c50, 'vi', 12345)
for k in (0, 1, 2, 3, 4):
	cset(ca, va)
	cstep('CBmp_Effect', ca, 0x2a8dd0, 'tvii', k)
for e in ((1.0, 255.0), (2.5, 255.0), (0.5, 100.0), (-1.0, 255.0), (0.01, 255.0)):
	cset(ca, va)
	cstep('CBmp_Exposure', ca, 0x2a97e0, 'tviff', e[0], e[1])
for e in ((1.0, 255.0), (2.2, 255.0), (0.45, 255.0), (-1.0, 128.0), (3.0, 0.0)):
	cset(ca, va)
	cstep('CBmp_Gamma', ca, 0x2a99e0, 'tviff', e[0], e[1])
for m in ((7, 7), (1, 8), (15, 1), (8, 6), (2, 15)):
	cset(ca, va)
	cstep('CBmp_MixChannels', ca, 0x2aa470, 'tviii', m[0], m[1])
cset(ca, va)
for u in (0.0, 0.1, 0.5, 0.77, 1.0):
	for v in (0.0, 0.33, 0.9, 1.0):
		check('CBmp_Sample' + fmt((u, v)), lambda u=u, v=v: SDTest.Call(0x2aa920, 'tiipff', ca, 0, u, v)[1])
cstep('CBmp_Sharpen', ca, 0x2aab80, 'tvi')
cset(ca, va)
cstep('CBmp_Blur', ca, 0x2a8830, 'tvi')
cset(ca, va)
cstep('CBmp_Fill', ca, 0x2a8bc0, 'tvii', 0x11223344)
cset(ca, va)
cc = bnew(6, 4, 0x2a9bc0)
cset(cc, cvals(24))
for d in ((3, 2, 15), (3, 2, 5), (6, 4, 15), (6, 4, 9), (4, 3, 15), (7, 6, 15), (2, 2, 10), (1, 1, 15)):
	bd = bnew(d[0], d[1], 0x2a9bc0)
	cset(bd, cvals(d[0] * d[1]))
	bcheck('CBmp_Resample' + fmt(d), bd, 0x2aaff0, 'tviii', (cc, d[2]), 1)
	bfree(bd)
bd = SDTest.Call(0x133650, 'ii', 12)[0]
bcheck('CBmp_Resample new', bd, 0x2aaff0, 'tviii', (ca, 15), 1)
bfree(bd)
bfree(cc)
bfree(ca)
bfree(cb)

# the image files of Engine/Bmp.c: loaded from the packs, saved below the game directory and read back
def csum(b):
	w, h, img = SDTest.Peek(b, '@i'), SDTest.Peek(b + 4, '@i'), SDTest.Peek(b + 8, '@i')
	if not img:
		return w, h, None
	s = 0L
	for i in range(w * h):
		s = (s * 31 + SDTest.Peek(img + 4 * i, '@i')) % 1000000007
	return w, h, s
def readback(name):
	try:
		f = open(name, 'rb')
	except IOError:
		return None
	d = f.read()
	f.close()
	return d
cl = SDTest.Call(0x133650, 'ii', 12)[0]
for n, fl in (('models/elements/templeyakuzzi/aceite-r.bmp', 0), ('models/elements/templeyakuzzi/aceite-r.bmp', 0x100),
		('levels/abindoor/2d/map0.tga', 0), ('2d/white.tga', 0), ('2d/loading/00icon.alpha.tga', 0),
		('levels/menu/map/map3d.0001.lmap1.tga', 0), ('sdnone.bmp', 0), ('sdnone.tex', 0), ('sdnone.TGA', 0)):
	check('CBmp_Load' + fmt((n, fl)), lambda n=n, fl=fl: (SDTest.Call(0x2aa3a0, 'tiisi', cl, n, fl)[0], csum(cl)))
check('CBmp_Load map0', lambda: (SDTest.Call(0x2aa3a0, 'tiisi', cl, 'levels/abindoor/2d/map0.tga', 0)[0], bget(cl, 1)))
for rva, n in ((0x2a9c20, 'BMP_Load'), (0x2a9fc0, 'TEX_Load'), (0x2aa0b0, 'TGA_Load')):
	check(n + ' bad', lambda rva=rva: (SDTest.Call(rva, 'isii', 'levels/menu/map/map3d.ini', cl, 0)[0], csum(cl)))
bfree(cl)
cs = bnew(8, 3, 0x2a9bc0)
A, B, C, D = 0x11223344, 0x11223355, -0x66778899, 0x01020304
cset(cs, (A, A, A, A, B, C, C, D,  A, B, A, B, C, C, B, B,  D, D, C, C, C, A, A, 0x22223355))
for n, fl in (('sdimg.bmp', 0), ('sdimg.bmp', 0), ('sdimg2.BMP', 1), ('sdimg.tex', 0), ('sdimg.tex', 0), ('sdimg.tga', 0),
		('sdimg.tga', 1), ('sdimga.tga', 3), ('sdimgx.txt', 0)):
	check('CBmp_Save' + fmt((n, fl)), lambda n=n, fl=fl: (SDTest.Call(0x2a9740, 'tiisi', cs, n, fl)[0], readback(n)))
cr = bnew(130, 2, 0x2a9bc0)
v = []
for i in range(130):
	v.append(i / 3)
for i in range(130):
	v.append(5)
cset(cr, v)
check('TGA_Save long', lambda: (SDTest.Call(0x2a9450, 'isii', 'sdlong.tga', cr, 1)[0], readback('sdlong.tga')))
bfree(cr)
bfree(cs)

# engine3d Engine/LeeM3D.c: the M3D values in text (an ini of the packs) and binary (a bmp, the ini read as binary)
def m3dmode(binary, labels):
	v = SDTest.Peek(0x4e1fa4, 'i')
	SDTest.Poke(0x4e1fa4, (v & -0x10000) + binary + (labels << 8))
def mstep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), SDTest.Peek(0x4e73b8, 'p')))
m3dsaved = SDTest.Peek(0x4e1fa4, 'i')
m3dmode(0, 1)
SDTest.Call(0x2d5c20, 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
mstep('M3D_Label', 0x2bc850, 'vs', '[suelo]')
mstep('M3D_ReadQuat', 0x2bcbb0, 'vsFi', 'model_ambient =', (0.0, 0.0, 0.0, 0.0), 1)
mstep('M3D_Label', 0x2bc850, 'vs', '[Model]')
mstep('M3D_ReadString', 0x2bd080, 'ssbii', 'TexturePath =', '', 40, 0)
mstep('M3D_Label', 0x2bc850, 'vs', '[mapa_sombras]')
for t in ('escala =', 'resolucion =', 'gamma ='):
	mstep('M3D_ReadFloat', 0x2bd430, 'fs', t)
mstep('M3D_ReadBool', 0x2bcfb0, 'bs', 'sombras_suaves =')
mstep('M3D_ReadInt', 0x2bea70, 'is', 'text_tam_x =')
mstep('M3D_ReadInt', 0x2bea70, 'is', 'text_tam_y =')
for t in ('radiosidad =', 'ilum_global =', 'no_luz_amb =', 'modulate4x ='):
	mstep('M3D_ReadBool', 0x2bcfb0, 'bs', t)
SDTest.Call(0x2d5d50, 'i')
SDTest.Call(0x2d5c20, 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
mstep('M3D_Label', 0x2bc850, 'vs', '[suelo]')
mstep('M3D_ReadVec3', 0x2bce80, 'vsFi', 'model_ambient =', (0.0, 0.0, 0.0), 0)
mstep('M3D_ReadFloat', 0x2bd430, 'fz', None)
SDTest.Call(0x2d5d50, 'i')
SDTest.Call(0x2d5c20, 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
mstep('M3D_Label', 0x2bc850, 'vs', '[suelo]')
mstep('M3D_ReadVec2', 0x2bcd90, 'vsF', 'model_ambient =', (0.0, 0.0))
mstep('M3D_ReadVec2 next', 0x2bcd90, 'vzF', None, (0.0, 0.0))
SDTest.Call(0x2d5d50, 'i')
lbuf = SDTest.Call(0x1335a0, 'ii', 64)[0]
saved = SDTest.Peek(0x4e73b8, 'i')
for t in ('"abc" x', ' "" y', '"long text here"'):
	for n in (2, 40):
		SDTest.Call(0x1365c0, 'sisi', lbuf, t, 64)
		SDTest.Poke(0x4e73b8, lbuf)
		check('M3D_ReadString quoted' + fmt((t, n)), lambda n=n: SDTest.Call(0x2bd080, 'szbii', None, '', n, 1))
	SDTest.Call(0x1365c0, 'sisi', lbuf, t, 64)
	SDTest.Poke(0x4e73b8, lbuf)
	check('M3D_ReadString quoted cad' + fmt((t,)), lambda: (SDTest.Call(0x2bd080, 'szzii', None, None, 0, 1), SDTest.Peek(0x4e1fc0, 's')))
SDTest.Poke(0x4e73b8, saved)
SDTest.Call(0x133550, 'vi', lbuf)
m3dmode(1, 0)
SDTest.Call(0x2d5c20, 'vzsi', None, 'models/elements/templeyakuzzi/aceite-r.bmp', 1)
for i in range(4):
	mstep('M3D_ReadInt bin', 0x2bea70, 'iz', None)
mstep('M3D_ReadBool bin', 0x2bcfb0, 'bz', None)
mstep('M3D_ReadBool bin', 0x2bcfb0, 'bs', 'x')
mstep('M3D_ReadString bin', 0x2bd080, 'szbii', None, '', 40, 0)
for i in range(3):
	mstep('M3D_ReadInt bin', 0x2bea70, 'iz', None)
mstep('M3D_ReadFloat bin', 0x2bd430, 'fz', None)
mstep('M3D_ReadVec3 bin', 0x2bce80, 'vzFi', None, (0.0, 0.0, 0.0), 1)
mstep('M3D_ReadVec2 bin', 0x2bcd90, 'vzF', None, (0.0, 0.0))
mstep('M3D_ReadQuat bin', 0x2bcbb0, 'vzFi', None, (0.0, 0.0, 0.0, 0.0), 0)
mstep('M3D_ReadTransform bin', 0x2bca60, 'vsF', 'x', (0.0,) * 15)
for i in range(20):
	mstep('M3D_ReadInt bin', 0x2bea70, 'iz', None)
mstep('M3D_ReadQuat bin', 0x2bcbb0, 'vzFi', None, (0.0, 0.0, 0.0, 0.0), 1)
SDTest.Call(0x2d5d50, 'i')
SDTest.Call(0x2d5c20, 'vzsi', None, '2d/white.tga', 1)
SDTest.Call(0x2d5e70, 'vbi', '', 18)
for i in range(4):
	mstep('M3D_ReadInt white', 0x2bea70, 'iz', None)
SDTest.Call(0x2d5d50, 'i')
SDTest.Call(0x2d5c20, 'vzsi', None, 'levels/menu/map/map3d.0001.lmap1.tga', 1)
SDTest.Call(0x2d5e70, 'vbi', '', 18)
for i in range(60):
	mstep('M3D_ReadInt lmap', 0x2bea70, 'iz', None)
SDTest.Call(0x2d5d50, 'i')
SDTest.Call(0x2d5c20, 'vzsi', None, '2d/loading/00icon.alpha.tga', 1)
SDTest.Call(0x2d5e70, 'vbi', '', 900)
for i in range(60):
	mstep('M3D_ReadInt icon', 0x2bea70, 'iz', None)
SDTest.Call(0x2d5d50, 'i')
m3dmode(1, 1)
SDTest.Call(0x2d5c20, 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
SDTest.Call(0x2d5e70, 'vbi', '', 37)
mstep('M3D_Label bin', 0x2bc850, 'vs', '\n\r\n[Model]\r\nT')
check('M3D_Label bin cad', lambda: SDTest.Peek(0x4e1fc0, 's'))
mstep('M3D_ReadIndex bin', 0x2be9f0, 'vi', -38)
SDTest.Call(0x2d5d50, 'i')
SDTest.Poke(0x4e1fa4, m3dsaved)

# engine3d Engine/Tabla.c: the array template instances; the readers on .sm3 files of the packs at offsets where a
# count and an element size follow
def tpeek(addr, n):
	v = []
	for i in range(n):
		v.append(SDTest.Peek(addr + 4 * i, '@i'))
	return v
def tnew(rva, num, words):
	t = SDTest.Call(0x133650, 'ii', 8)[0]
	SDTest.Call(rva, 'tvii', t, num)
	n, p = SDTest.Peek(t, '@i'), SDTest.Peek(t + 4, '@i')
	r = (n, p != 0)
	if words and p:
		r = r + (tpeek(p, words),)
	SDTest.Call(0x133550, 'vi', t)
	return r
for rva, sz in ((0x250b50, 4), (0x261190, 2), (0x261200, 0x1c), (0x261280, 0xc), (0x2614d0, 1), (0x26b8b0, 0x14),
		(0x28fcc0, 0x14), (0x28fd30, 0x18), (0x28fea0, 0x2c), (0x29b930, 0x10), (0x2c2b10, 4), (0x2c2b80, 4),
		(0x2c2bf0, 8), (0x2d2000, 0x38)):
	for num in (1, 7):
		check('Tabla_Alloc' + fmt((rva, sz, num)), lambda rva=rva, num=num: tnew(rva, num, 0))
# the last 2 bytes of each element stay as new[] left them
def tzero(num):
	r = tnew(0x255f40, num, num * 4)
	v = r[2]
	for i in range(3, len(v), 4):
		v[i] = v[i] & 0xffff
	return r
for num in (1, 5):
	check('Tabla_Alloc10Z' + fmt((num,)), lambda num=num: tzero(num))
# objects: the count before them and the first word of each (the constructors leave other fields as they are)
def tobjs(rva, num, sz):
	t = SDTest.Call(0x133650, 'ii', 8)[0]
	SDTest.Call(rva, 'tvii', t, num)
	n, p = SDTest.Peek(t, '@i'), SDTest.Peek(t + 4, '@i')
	first = []
	for i in range(num):
		first.append(SDTest.Peek(p + i * sz, '@i'))
	SDTest.Call(0x133550, 'vi', t)
	return n, SDTest.Peek(p - 4, '@i'), first
for rva, sz in ((0x255ff0, 8), (0x2560d0, 0xa8), (0x261300, 0x24), (0x2613e0, 0xfc), (0x28fdb0, 0xb4), (0x2c2c60, 0x1c)):
	check('Tabla_New' + fmt((rva, sz)), lambda rva=rva, sz=sz: tobjs(rva, 3, sz))
tio = SDTest.Call(0x1335a0, 'ii', 16)[0]
def topen(name, skip):
	SDTest.Call(0x21b1f0, 'tiiss', tio, name, 'rb')
	while skip > 0:
		k = min(skip, 1000)
		SDTest.Call(0x21c060, 'tiibi', tio, '', k)
		skip = skip - k
def tclose():
	SDTest.Call(0x21b490, 'tii', tio)
	SDTest.Call(0x21b290, 'tvi', tio)
sm3 = 'levels/bank/map/directordiscovered/directordiscovered.sm3'
def aread(rva, name, off, sz):
	topen(name, off)
	r = SDTest.Call(rva, 'vipp', tio, 0, 0)
	p, n = r[1], r[2]
	data = None
	if p:
		data = tpeek(p, (n * sz + 3) / 4)
	tclose()
	return n, p != 0, data
for rva, sz, off in ((0x2af2e0, 2, 3564), (0x2af360, 4, 4242), (0x2af3e0, 8, 5852), (0x2af460, 0x10, 1824),
		(0x2af4e0, 0x10, 1824), (0x2af560, 6, 5174), (0x2af5e0, 0xc, 7209)):
	check('Array_Read' + fmt((rva, sz)), lambda rva=rva, sz=sz, off=off: aread(rva, sm3, off, sz))
def tread(rva, name, off, sz):
	topen(name, off)
	t = SDTest.Call(0x133650, 'ii', 8)[0]
	SDTest.Call(rva, 'vii', tio, t)
	n, p = SDTest.Peek(t, '@i'), SDTest.Peek(t + 4, '@i')
	data = None
	if p:
		data = tpeek(p, (n * sz + 3) / 4)
	SDTest.Call(0x133550, 'vi', t)
	tclose()
	return n, p != 0, data
check('Tabla_Read2', lambda: tread(0x258290, sm3, 3564, 2))
check('Tabla_ReadC', lambda: tread(0x2583a0, sm3, 7209, 0xc))
check('Tabla_Read1C', lambda: tread(0x258310, 'levels/fake/map/fake1/fake1.sm3', 3616, 0x1c))
SDTest.Call(0x133550, 'vi', tio)

# engine3d Engine/AnimVert.c on vertices and sets in heap memory
def avnew(frames):
	av = SDTest.Call(0x133650, 'ii', 16)[0]
	SDTest.Call(0x2d44a0, 'tvii', av, len(frames) / 3)
	SDTest.Call(0x3197d0, 'iiFi', SDTest.Peek(av + 4, '@i'), tuple(frames), 4 * len(frames))
	return av
def avget(av):
	n, tr, tr2 = SDTest.Peek(av, '@i'), SDTest.Peek(av + 4, '@i'), SDTest.Peek(av + 12, '@i')
	r = [n, SDTest.Peek(av + 8, '@i')]
	if tr:
		r.append(tpeek(tr, n * 3))
	else:
		r.append(None)
	if tr2:
		b = []
		for i in range(n * 3):
			b.append(SDTest.Peek(tr2 + i, '@b'))
		r.append(b)
	else:
		r.append(None)
	return r
def avstep(name, av, rva, sig, *args):
	check(name + fmt(args), lambda av=av, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, av) + args), avget(av)))
fr = (0.0, 1.0, 2.0,  0.5, 1.5, -2.0,  3.0, -1.0, 2.25,  -4.0, 1.0, 2.0)
av = avnew(fr)
check('CAnimVert_AllocTr', lambda: avget(av))
check('CAnimVert_Frame', lambda: SDTest.Call(0x2d38b0, 'tiii', av, 2)[0] - SDTest.Peek(av + 4, '@i'))
for f, t in ((0, 0.0), (0, 0.5), (1, 0.25), (2, 0.999), (3, 0.5), (4, 0.0)):
	avstep('CAnimVert_GetPos', av, 0x2d3930, 'tviifF', f, t, (10.0, 20.0, 30.0))
avstep('CAnimVert_Scale', av, 0x2d4380, 'tvifff', 2.0, -1.0, 0.5)
avstep('CAnimVert_Optimize', av, 0x2d49b0, 'tvi')
for f, t in ((0, 0.0), (0, 0.5), (1, 0.25), (2, 0.75), (3, 0.5), (5, 0.0)):
	avstep('CAnimVert_GetPos 2', av, 0x2d3930, 'tviifF', f, t, (10.0, 20.0, 30.0))
for a, b, t in ((0, 1, 0.5), (1, 3, 0.25), (2, 2, 0.7), (3, 0, 1.0)):
	avstep('CAnimVert_AddPos', av, 0x2d3bb0, 'tviiifF', a, b, t, (1.0, 2.0, 3.0))
avstep('CAnimVert_Scale 2', av, 0x2d4380, 'tvifff', 3.0, 3.0, 3.0)
avstep('CAnimVert_FreeTr2', av, 0x2d3fd0, 'tvi')
check('CAnimVert_Delete', lambda: SDTest.Call(0x2d3510, 'tiii', av, 1)[0] == av)
for frames in ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0, 1.00001, 2.0, 3.0), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0002), (0.0, 0.0, 0.0, 1e6, -1e6, 5.0, 1e-3, 0.0, -2.0)):
	av = avnew(frames)
	avstep('CAnimVert_Optimize' + fmt((frames,)), av, 0x2d49b0, 'tvi')
	SDTest.Call(0x2d3510, 'tiii', av, 1)
av = avnew(fr)
avstep('CAnimVert_Compress', av, 0x2d40c0, 'tvi')
avstep('CAnimVert_dtor', av, 0x2d3440, 'tvi')
SDTest.Call(0x133550, 'vi', av)
eva = SDTest.Call(0x133650, 'ii', 8)[0]
def evaget():
	n, a = SDTest.Peek(eva, '@i'), SDTest.Peek(eva + 4, '@i')
	r = [n, a != 0]
	if a:
		r.append(SDTest.Peek(a - 4, '@i'))
		for i in range(n):
			r.append(avget(a + 16 * i))
	return r
def evastep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, eva) + args), evaget()))
evastep('CEVA_Alloc', 0x2d4570, 'tvii', 3)
check('CEVA_Get', lambda: SDTest.Call(0x2d3d50, 'tiii', eva, 2)[0] - SDTest.Peek(eva + 4, '@i'))
evastep('CEVA_MemSize', 0x2d4670, 'tii')
evastep('CEVA_NumAnimated', 0x2d4770, 'tii')
a = SDTest.Peek(eva + 4, '@i')
SDTest.Call(0x2d44a0, 'tvii', a, 2)
SDTest.Call(0x3197d0, 'iiFi', SDTest.Peek(a + 4, '@i'), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0), 24)
SDTest.Call(0x2d44a0, 'tvii', a + 16, 1)
SDTest.Call(0x3197d0, 'iiFi', SDTest.Peek(a + 20, '@i'), (5.0, 6.0, 7.0), 12)
SDTest.Call(0x2d44a0, 'tvii', a + 32, 3)
SDTest.Call(0x3197d0, 'iiFi', SDTest.Peek(a + 36, '@i'), (1.0, 2.0, 3.0, 2.0, 2.0, 3.0, 0.0, -1.0, 3.5), 36)
evastep('CEVA_MemSize', 0x2d4670, 'tii')
evastep('CEVA_NumAnimated', 0x2d4770, 'tii')
evastep('CEVA_Scale', 0x2d35b0, 'tvifff', 2.0, 2.0, 2.0)
evastep('CEVA_Optimize', 0x2d4810, 'tvi')
evastep('CEVA_MemSize', 0x2d4670, 'tii')
evastep('CEVA_NumAnimated', 0x2d4770, 'tii')
evastep('CEVA_Free', 0x2d4020, 'tvi')
evastep('CEVA_Alloc', 0x2d4570, 'tvii', 2)
a = SDTest.Peek(eva + 4, '@i')
for i in range(2):
	SDTest.Call(0x2d44a0, 'tvii', a + 16 * i, 2)
	SDTest.Call(0x3197d0, 'iiFi', SDTest.Peek(a + 16 * i + 4, '@i'), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0), 24)
evastep('CEVA_Optimize static', 0x2d4810, 'tvi')
evastep('CEVA_Alloc', 0x2d4570, 'tvii', 4)
evastep('CEVA_dtor', 0x2d3400, 'tvi')
SDTest.Call(0x133550, 'vi', eva)

# engine3d Engine/Varios.c: logs, file and string helpers
for p in ('a.txt', 'dir/x/../y.txt', './a/./b', 'a/../../b', '../x', 'a\\b\\..\\c', 'C:\\abs\\p', '\\root', '/root', 'a..b/c', 'x/.../y',
		'..', '.', 'a/.', 'a/..', 'a//b', 'levels/menu/./map/../map/map3d.ini', 'a/b/c/../../d', 'x:', 'x:y/../z', '.hidden', 'a/.b'):
	for n in (-1, 8, 300):
		call('Ruta_Normalize', 0x238540, 'vbsi', '', p, n)
	call('Ruta_Copy', 0x2388b0, 'sbsi', '', p, -1)
for t in ('', '  ', 'word', '  two words', '"quoted text" rest', ' "unclosed', '\t""', '"a"'):
	call('Cad_Unquote', 0x238920, 'sbs', '', t)
call('Cad_Unquote NULL', 0x238920, 'sbz', '', None)
for x in (1.0, 2.0, 0.25, 3.0, 100.0, 1e-6, 12345.678, 0.5, 7.0):
	fcall('RSqrt_Estimate', 0x238a40, 'ff', x)
	fcall('RSqrt', 0x238a80, 'ff', x)
for pat, n in (('*.ini', 'MAP3D.INI'), ('a?c', 'ABC'), ('*', 'X'), ('x*', 'Y')):
	call('Wild_MatchLower', 0x238850, 'iss', pat, n)
for n in ('levels/menu/map/map3d.ini', 'sdnone.txt', 'Bin/Scrap.exe', 'sdcall.py', ''):
	call('File_Exists', 0x2384e0, 'bs', n)
	check('File_Time' + fmt((n,)), lambda n=n: SDTest.Call(0x238500, 'is', n)[0] != 0)
	call('File_TimeChanged', 0x238440, 'bisi', 0, n, 5)
check('File_TimeDiff', lambda: SDTest.Call(0x238770, 'issi', 'sdcall.py', 'sdcall.py', -7))
call('File_TimeDiff missing', 0x238770, 'issi', 'sdnone.txt', 'sdcall.py', -7)
writefile('sdbk.txt', 'backup')
call('File_Backup', 0x2387d0, 'isi', 'sdbk.txt', 0)
check('File_Backup file', lambda: (readback('sdbk.txt'), readback('sdbk.txt.bak')))
for f in (0x238070, 0x237d80, 0x238260, 0x238380, 0x237be0, 0x237b00, 0x238180, 0x237ca0):
	call('E3D_Log', f, 'viss', 0x4ab914 + 0x400000, 'sdlog %s', 'x')
for m in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0xff, 0):
	check('cLog_Write' + fmt((m,)), lambda m=m: SDTest.Call(0x237e40, 'tviis', 0x4ab914 + 0x400000, m, 'sdlog write'))
call('E3D_DebugPrintf', 0x237f60, 'vs', 'sdlog debug')

# engine3d Engine/Camara.c on cameras in heap memory; nodes are fake (only Refs), kept above 1
def camget(c):
	return tpeek(c, 30)
def camstep(name, c, rva, sig, *args):
	check(name + fmt(args), lambda c=c, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, c) + args), camget(c)))
# the getters return the address of the matrix passed in, which differs between the processes
def camget16(name, c, rva):
	def f(c=c, rva=rva):
		r = SDTest.Call(rva, 'tiiF', c, (0.0,) * 16)
		return ((r[0] != 0, r[1]), camget(c))
	check(name + fmt(((0.0,) * 16,)), f)
cam = SDTest.Call(0x133650, 'ii', 0x78)[0]
cam2 = SDTest.Call(0x133650, 'ii', 0x78)[0]
nod = SDTest.Call(0x133650, 'ii', 16)[0]
nod2 = SDTest.Call(0x133650, 'ii', 16)[0]
SDTest.Poke(nod + 8 - 0x400000, 5)
SDTest.Poke(nod2 + 8 - 0x400000, 7)
check('CCamara_ctor', lambda: (SDTest.Call(0x29c110, 'tii', cam)[0] == cam, camget(cam)))
for i in range(12):
	SDTest.Call(0x3197d0, 'iiFi', cam + 8, vec() + vec() + vec(), 36)
	SDTest.Call(0x3197d0, 'iiFi', cam + 0x2c, (rnd(0.1, 2.0), rnd(5.0, 500.0), 0.0, 1000.0, rnd(0.1, 2.5), rnd(0.5, 2.0), rnd(0.5, 2.0)), 28)
	if i % 3 == 2:
		SDTest.Poke(cam + 0x48 - 0x400000, 0)
	else:
		SDTest.Poke(cam + 0x48 - 0x400000, 0x463)
	camstep('CCamara_ViewMatrix', cam, 0x29c7e0, 'tviF', (0.0,) * 16)
	camget16('CCamara_GetView', cam, 0x29c5f0)
	camstep('CCamara_ProjMatrix', cam, 0x29ca80, 'tbiF', (0.0,) * 16)
	camget16('CCamara_GetProj', cam, 0x29c5d0)
	camstep('CCamara_Move', cam, 0x29cc10, 'tvif', rnd())
	camstep('CCamara_Strafe', cam, 0x29cc60, 'tviff', rnd(), rnd())
	camstep('CCamara_UpdateVectors', cam, 0x29ce10, 'tvi')
	camstep('CCamara_LookAt', cam, 0x29c330, 'tviFFF', (0.0,) * 16, vec(), vec())
camstep('CCamara_LookAt zero', cam, 0x29c330, 'tviFFF', (0.0,) * 16, (0.0, 0.0, 0.0), (0.0, 1.0, 0.0))
camstep('CCamara_LookAt parallel', cam, 0x29c330, 'tviFFF', (0.0,) * 16, (0.0, 2.0, 0.0), (0.0, 1.0, 0.0))
SDTest.Call(0x3197d0, 'iiFi', cam + 0x2c, (5.0, 5.001, 0.0, 1000.0, 1.0, 1.0, 1.0), 28)
camstep('CCamara_ProjMatrix flat', cam, 0x29ca80, 'tbiF', (0.0,) * 16)
SDTest.Call(0x3197d0, 'iiFi', cam + 0x2c, (1.0, 100.0, 0.0, 1000.0, 0.001, 1.0, 1.0), 28)
camstep('CCamara_ProjMatrix narrow', cam, 0x29ca80, 'tbiF', (0.0,) * 16)
def refs():
	return SDTest.Peek(nod + 8, '@i'), SDTest.Peek(nod2 + 8, '@i')
check('CCamara_SetNodo', lambda: (SDTest.Call(0x29cd30, 'tvii', cam, nod), refs(), SDTest.Peek(cam + 0x70, '@i') == nod))
check('CCamara_SetNodo 2', lambda: (SDTest.Call(0x29cd30, 'tvii', cam, nod2), refs(), SDTest.Peek(cam + 0x70, '@i') == nod2))
check('CCamara_SetNodoObjetivo', lambda: (SDTest.Call(0x29cda0, 'tvii', cam, nod), refs()))
check('CCamara_ctorCopy', lambda: (SDTest.Call(0x29bf50, 'tiii', cam2, cam)[0] == cam2, refs(), camget(cam2)[2:19] == camget(cam)[2:19]))
check('CCamara_dtor', lambda: (SDTest.Call(0x29c210, 'tvi', cam2), refs(), camget(cam2)))
check('CCamara_Clear', lambda: (SDTest.Call(0x29c610, 'tvi', cam), refs(), camget(cam)))
SDTest.Call(0x29cd30, 'tvii', cam, nod)
check('CCamara_Delete', lambda: (SDTest.Call(0x29c300, 'tiii', cam, 0)[0] == cam, refs(), camget(cam)))
check('CCamara_SetNodo NULL', lambda: (SDTest.Call(0x29cd30, 'tvii', cam, 0), refs()))
SDTest.Call(0x133550, 'vi', cam)
SDTest.Call(0x133550, 'vi', cam2)
SDTest.Call(0x133550, 'vi', nod)
SDTest.Call(0x133550, 'vi', nod2)

# AI/AStar.c: the binary heap with the A* callbacks over fake ElemAStar records, and the int key hash table
NEL = 12
els = []
refs = SDTest.Call(0x133650, 'ii', NEL * 8)[0]
for i in range(NEL):
	e = SDTest.Call(0x133650, 'ii', 0x14)[0]
	SDTest.Poke(e - 0x400000, 100 + i)
	if i % 3:
		SDTest.Poke(e + 0x10 - 0x400000, refs + 8 * i)
	els.append(e)
def setf(i, f):
	SDTest.Call(0x3197d0, 'iiFi', els[i] + 8, (f,), 4)
fs = (5.0, 3.0, 8.0, 1.0, 9.0, 3.0, 7.0, 2.0, 6.0, 4.0, 0.5, 10.0)
for i in range(NEL):
	setf(i, fs[i])
heap = SDTest.Call(0x133650, 'ii', 0x1c)[0]
def eli(addr):
	for i in range(NEL):
		if els[i] == addr:
			return i
	return addr == 0 and -1 or -2
def heapget():
	n, a = SDTest.Peek(heap + 8, '@i'), SDTest.Peek(heap + 4, '@i')
	order = []
	for i in range(1, n + 1):
		order.append(eli(SDTest.Peek(a + 4 * i, '@i')))
	return n, SDTest.Peek(heap + 12, '@i'), order, tpeek(refs, NEL * 2)
def slotel(slot):
	if not slot:
		return None
	return eli(SDTest.Peek(slot, '@i'))
def hstep(name, rva, sig, *args):
	hstepn(name + fmt(args), rva, sig, args)
def hstepn(name, rva, sig, args):
	check(name, lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, heap) + args)[0], heapget()))
check('cBinaryHeap_ctor', lambda: (SDTest.Call(0x0992b0, 'tiiiiii', heap, 8, 0x49b0e0, 0x49b0c0, 0x499a10)[0] == heap, heapget()))
for i in range(10):
	check('cBinaryHeap_Push' + fmt((i,)), lambda i=i: (SDTest.Call(0x0996a0, 'tbip', heap, els[i])[0], heapget()))
check('cBinaryHeap_Top', lambda: (lambda r: (r[0], slotel(r[1])))(SDTest.Call(0x099a60, 'tbip', heap, 0)))
for i in (0, 1, 4, 8, 9):
	check('cBinaryHeap_Get' + fmt((i,)), lambda i=i: (lambda r: (r[0], slotel(r[1])))(SDTest.Call(0x099a30, 'tbipi', heap, 0, i)))
def hfind(i):
	r = SDTest.Call(0x099a80, 'tbippp', heap, els[i], 0, 0)
	return r[0], r[2], slotel(r[3])
for i in (3, 7, 11):
	check('cBinaryHeap_Find' + fmt((i,)), lambda i=i: hfind(i))
setf(11, 0.25)
hstepn('cBinaryHeap_Set 11 5', 0x09a0b0, 'tbipi', (els[11], 5))
setf(2, 20.0)
hstepn('cBinaryHeap_Set 2 1', 0x09a0b0, 'tbipi', (els[2], 1))
hstepn('cBinaryHeap_Set 2 0', 0x09a0b0, 'tbipi', (els[2], 0))
hstep('cBinaryHeap_Count', 0x099b20, 'tii')
setf(6, -1.0)
hstep('cBinaryHeap_Fix', 0x09a100, 'tvii', 7)
for i in range(10):
	hstep('cBinaryHeap_Pop', 0x099840, 'tbi')
hstep('cBinaryHeap_SiftUp', 0x09a130, 'tbii', 1)
hstep('cBinaryHeap_SiftDown', 0x09a1f0, 'tbii', 1)
hstep('cBinaryHeap_Clear', 0x099810, 'tvi')
check('cBinaryHeap_Delete', lambda: SDTest.Call(0x099520, 'tiii', heap, 0)[0] == heap)
SDTest.Call(0x133550, 'vi', heap)
for e in els:
	SDTest.Call(0x133550, 'vi', e)
SDTest.Call(0x133550, 'vi', refs)
tbl = SDTest.Call(0x133650, 'ii', 12)[0]
SDTest.Poke(tbl - 0x400000, 7)
SDTest.Poke(tbl + 4 - 0x400000, SDTest.Call(0x133650, 'ii', 28)[0])
def tstep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig, tbl) + args)[0])
for k in (0, 1, 15, 16, 255, 256, 0x7fffffff, -1, 12345, 0x1000, 3):
	tstep('AStarTable_Slot', 0x099f30, 'tiii', k)
	check('AStarTable_Add' + fmt((k,)), lambda k=k: SDTest.Call(0x099620, 'tbiiFi', tbl, k, (k % 1000, 1, 7), 0)[0:1])
check('AStarTable_Add again', lambda: SDTest.Call(0x099620, 'tbiiFi', tbl, 15, (1, 2, 3), 1)[0:1])
check('AStarTable_Add again quiet', lambda: SDTest.Call(0x099620, 'tbiiFi', tbl, 256, (1, 2, 3), 0)[0:1])
def tget(k):
	r = SDTest.Call(0x0999c0, 'tbiip', tbl, k, 0)
	if not r[0]:
		return r[0]
	return r[0], tpeek(r[1], 4)
for k in (0, 15, 256, -1, 9, 3, 77):
	check('AStarTable_Get' + fmt((k,)), lambda k=k: tget(k))
	check('AStarTable_Find' + fmt((k,)), lambda k=k: SDTest.Call(0x099990, 'tiiii', tbl, k, SDTest.Call(0x099f30, 'tiii', tbl, k)[0])[0] != 0)
check('AStarTable_Insert', lambda: tpeek(SDTest.Call(0x0996f0, 'tiiiiF', tbl, 99, 2, (4, 5, 6))[0], 4))
check('AStarTable_Clear', lambda: (SDTest.Call(0x099890, 'tvi', tbl), tpeek(SDTest.Peek(tbl + 4, '@i'), 7)))
check('AStarTable_Add after', lambda: SDTest.Call(0x099620, 'tbiiFi', tbl, 5, (1, 2, 3), 0)[0:1])
check('AStarTable_dtor', lambda: (SDTest.Call(0x09ac50, 'tvi', tbl), tpeek(tbl, 3)))
SDTest.Call(0x133550, 'vi', tbl)
# the tests of single modules (tools/test/py/sdcall_*.py); a missing one is skipped
for part in ('sdcall_listavert.py', 'sdcall_mallatri.py', 'sdcall_material.py', 'sdcall_luz.py', 'sdcall_nodo3d.py',
		'sdcall_animacion.py', 'sdcall_animgrupo.py', 'sdcall_textura.py', 'sdcall_mallad3d.py', 'sdcall_suelo.py',
		'sdcall_trails.py', 'sdcall_modelo.py', 'sdcall_escena.py', 'sdcall_sistpart.py', 'sdcall_rendertarget.py',
		'sdcall_controlanim.py', 'sdcall_portal.py', 'sdcall_grafico3d.py', 'sdcall_utild3d.py', 'sdcall_texto.py', 'sdcall_grafico2d.py', 'sdcall_render.py',
		'sdcall_multisprite.py', 'sdcall_flare.py', 'sdcall_fuentes.py',
		'sdcall_scrapfont.py', 'sdcall_leem3d.py', 'sdcall_m3d.py', 'sdcall_scrapmodel.py', 'sdcall_scrapcam.py', 'sdcall_scrapworld.py', 'sdcall_scrapmain.py', 'sdcall_app.py', 'sdcall_sound.py', 'sdcall_music.py', 'sdcall_bsp.py', 'sdcall_input.py', 'sdcall_render2.py', 'sdcall_eng2.py', 'sdcall_eng3.py', 'sdcall_eng4.py', 'sdcall_restb.py', 'sdcall_resta.py', 'sdcall_engmisc.py', 'sdcall_nosleep.py'):
	try:
		f = open(part)
	except IOError:
		continue
	f.close()
	execfile(part)
finish()
