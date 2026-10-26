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

call('Lex_SetOut', 0x2cda80, 'vi', SDTest.OutFunc())
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
check('ArchIO_Getc', lambda: SDTest.Call(0x21b8a0, 'bi', f))
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
finish()
