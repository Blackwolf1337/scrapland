# Pure functions of the exe called by RVA through the module SDTest of the DLL (src/core/testmod.c), in the main
# menu. With the hooks on the calls reach the C versions, with them off the originals (Python 1.5.2 syntax).
OUT = 'sdcall.out'
execfile('sdlib.py')
import SDTest

def call(name, rva, sig, *args):
	check('%s%s' % (name, fmt(args)), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))

# Global/FileUtils.c
for s in ('', ' ', '  a b  ', '\ta\r\nb\t', 'abc', ' \t\v', 'a\fb \f', '\r\n', 'x  y\n'):
	call('Str_Trim', sym('Str_Trim'), 'vb', s)
pats = ('*', '?', '', 'a*', '*.py', '*.PY', 'a?c', '[a-c]x', '[!a-c]x', '[]x', '[a-', 'a[?]c', 'ab[cd', '*a*b*')
names = ('', 'a', 'abc', 'ax', 'dx', 'Init.py', 'init.PY', 'a?c', 'a]c', 'ab', 'aab')
for p in pats:
	for n in names:
		call('Str_MatchNoCase', sym('Str_MatchNoCase'), 'bss', p, n)
		call('Wild_Match', sym('Wild_Match'), 'iss', p, n)
for l in ('', 'a', ' a ; b ', '*.py;*.txt', ';;', '\t*x\t', 'a b;c'):
	for n in ('a', 'b', 'x.py', 'X.TXT', 'a b', 'c', ''):
		call('StrList_Match', sym('StrList_Match'), 'bsss', l, n, ';')
call('StrList_Match', sym('StrList_Match'), 'bsss', 'a,b c', 'c', ', ')

# Global/StrUtils.c
for s in ('', 'abc', 'x' * 20):
	for n in (1, 2, 5, 30):
		call('Str_Copy', sym('Str_Copy'), 'sbsi', '', s, n)
		call('String_Copy', sym('String_Copy'), 'sbsi', '', s, n)
for s in ('', ' a ', '\t a\tb \t', 'abc'):
	call('Str_TrimBlanks', sym('Str_TrimBlanks'), 'vb', s)

# engine3d Util/Wildcard.c
paths = ('', 'a', 'a.b', '.a', 'a.', '.', '..', 'dir\\', 'dir/', 'C:', 'C:a', 'C:\\a\\b.c', 'c:/x/y/z.tar.gz',
	'\\\\srv\\share\\f.x', 'a b .c  ', '   ', 'x\\.', 'x\\..', 'C:.', 'C:..', '..\\a', 'd:x\\', 'a:b:c', '1:2',
	'Bin\\Scrap.exe', 'x' * 300, 'd/' + 'y' * 300 + '.e', 'a.' + 'e' * 300)
for r in paths:
	call('Path_SplitDrive', sym('Path_SplitDrive'), 'vsbbbb', r, '', '', '', '')
	call('Path_SplitDrive NULL', sym('Path_SplitDrive'), 'vszbzb', r, None, '', None, '')
	for m in (0, 1, 2, 4, 8, 3, 12, 15):
		call('Path_Part', sym('Path_Part'), 'vbsi', '', r, m)
	call('Path_PartN', sym('Path_PartN'), 'vbisi', '', 5, r, 15)
	call('Path_FileName', sym('Path_FileName'), 'ss', r)
	call('Path_FirstDir', sym('Path_FirstDir'), 'vbs', '', r)
	call('Path_LastDir', sym('Path_LastDir'), 'vbs', '', r)
	call('Path_DriveNum', sym('Path_DriveNum'), 'is', r)
	call('Path_Format', sym('Path_Format'), 'vbss', '', r, '[&u|&d|&n|&e|&&|&U&D&N&E]')
	for r2 in ('', 'C:\\base\\def.ext', 'name', '.ext', 'sub\\'):
		# the original copies a single path unbounded, the C version up to MAX_FILEPATH
		if r2 or len(r) < 260:
			call('Path_Merge', sym('Path_Merge'), 'vbss', '', r, r2)
		call('Path_Merge NULL', sym('Path_Merge'), 'vbzs', '', None, r2 or 'x')
for r in ('', 'a', 'a.b', 'dir\\', 'C:', 'C:a', '..', '.\\x\\..\\y', 'a\\b\\..\\..\\c', '\\a\\.\\b',
	'C:\\a\\..\\..\\b', 'C:\\', 'D:x\\y', 'x/./y/../z.', 'Bin'):
	for b in (None, 'C:\\base\\dir', 'D:\\', 'base\\', 'C:', '\\root'):
		call('Path_Full', sym('Path_Full'), 'sbsiz', '', r, 260, b)
	call('Path_Full short', sym('Path_Full'), 'sbsiz', '', r, 4, 'C:\\base')
for p in ('*', '*.*', 'a*', '*.PY', '?b.*', 'x', '.e', 'a.', '[ab]*.[!c]', 'dir\\*.py', 'C:\\d\\*'):
	for r in ('a.py', 'AB.c', 'x', 'x.', 'dir\\b.py', 'C:\\d\\e\\f.g', 'C:\\D\\f', '.e'):
		call('Path_Match', sym('Path_Match'), 'iss', p, r)
		call('Path_MatchPath', sym('Path_MatchPath'), 'issi', p, r, 0)
		call('Path_MatchPath subdirs', sym('Path_MatchPath'), 'issi', p, r, 1)
for src in ('name.ext', 'n', 'longname.x', '.e', 'a.b.c'):
	for pat in ('*', '*.*', '?.?', 'x*.bak', '??z*.*', '(1 2)x.(-1)', '((a).*', 'd\\*.n', '*.', '.', 'a(3 1).(-2 -1 9)'):
		call('Path_Rename', sym('Path_Rename'), 'vbss', '', src, pat)
for p in ('[abc]', '[a-c]x', '[!a]', '[]', '[!]', '[a-]', '[?]', '[a', '[', 'x', '[z-a]', '[--/]'):
	for c in ('a', 'b', 'x', '-', ']', '\377', ''):
		call('Wild_MatchSet', sym('Wild_MatchSet'), 'sss', p, c)
for c in ('a', '\\', '/', ':', '\000'):
	call('Path_IsSep', sym('Path_IsSep'), 'ii', ord(c))
call('Path_GetDrive', sym('Path_GetDrive'), 'vp', 0)
call('Path_GetCwd', sym('Path_GetCwd'), 'iibi', 0, '', 260)
call('Path_GetCwd short', sym('Path_GetCwd'), 'iibi', 0, '', 3)
call('Path_GetCwd C', sym('Path_GetCwd'), 'iibi', 3, '', 260)
# not 'C:': Path_IsDir changes to the directory it remembered for that drive, the tests would go on in C:\
for r in ('Bin', 'Bin\\', 'Bin/', 'SDNoDir', 'SDNoDir\\', '.'):
	call('Path_IsDir', sym('Path_IsDir'), 'sb', r)
	call('Path_IsDir after', sym('Path_GetCwd'), 'iibi', 0, '', 260)
call('Path_MakePath', sym('Path_MakePath'), 'is', 'Bin\\Scrap.exe')
call('Path_MakePath after', sym('Path_GetCwd'), 'iibi', 0, '', 260)
for d in ('', 'C', 'C:', 'x', 'dir'):
	for n in ('', 'n'):
		call('Path_Make', sym('Path_Make'), 'vbzzzz', '', d, d + 'x\\', n, '.e')
		call('Path_Make NULL', sym('Path_Make'), 'vbzzzz', '', None, None, None, None)

# engine3d Util/Strings.c and Bprintf.c
for s, pos in (('abc', 0), ('abc', 1), ('abc', 3), ('', 0)):
	call('String_InsertChar', sym('String_InsertChar'), 'vbiii', s, ord('X'), pos, len(s))
for s in ('', '1', '123', '1234', '1234567', '-1234567', 'x123456', '12x3456789', '0000000000'):
	call('String_Thousands', sym('String_Thousands'), 'ib', s)
for n in (0, 1, -1, 999, 1000, -1000, 123456789, -2147483647 - 1):
	call('String_FromInt', sym('String_FromInt'), 'szi', None, n)
	call('String_FromInt buf', sym('String_FromInt'), 'sbi', '', n)
call('String_Dup', sym('String_Dup'), 'ss', 'dup')
for n in (1, 2, 4, 20):
	call('Bprintf', sym('Bprintf'), 'ibiss', '', n, '%s-%s', 'ab')

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

check('Lex_SetOut(out)', lambda: SDTest.Call('Lex_SetOut', 'vi', SDTest.OutFunc()))
call('Lex_SetDir', sym('Lex_SetDir'), 'vs', 'sdlex.txt')
call('Lex_Print', sym('Lex_Print'), 'vss', '[%s]', 'x')
check('Lex out', SDTest.Out)

def lexfile(name, escapes):
	call('Lex_Open', sym('Lex_Open'), 'isi', name, escapes)
	check('Lex out', SDTest.Out)
	for i in range(200):
		t = SDTest.Call('Lex_Next', 'i')[0]
		check('Lex_Next %s %d' % (name, i), lambda t=t: (t, SDTest.Peek('g_LexToken', 's'), SDTest.Peek('g_LexCh', 'i'),
			SDTest.Peek('g_LexErrors', 'i'), SDTest.Peek('g_LexWarnings', 'i'), SDTest.Out()))
		if t == -1:
			break
	call('Lex_Close', sym('Lex_Close'), 'vi', 2)
	check('Lex out', SDTest.Out)

lexfile('sdlex.txt', 1)
lexfile('sdlex.txt', 0)
lexfile('sdlex3.txt', 1)
call('Lex_Open missing', sym('Lex_Open'), 'isi', 'sdnofile.txt', 0)
check('Lex out', SDTest.Out)

def cl():
	return (SDTest.Peek(sym('g_Cl') + 0x4, 'i'), SDTest.Peek('g_Cl', 'p'), SDTest.Peek(sym('g_Cl') + 0x8, 'i'), SDTest.Out())
def step(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), cl()))

call('Lex_Open', sym('Lex_Open'), 'isi', 'sdcmd.txt', 1)
SDTest.Poke(sym('g_Cl') + 0x4, 0)
step('CmdLex_Next', sym('CmdLex_Next'), 'v')
step('CmdLex_Expect', sym('CmdLex_Expect'), 'ii', 0)
step('CmdLex_Expect', sym('CmdLex_Expect'), 'ii', 1)
step('CmdLex_Next', sym('CmdLex_Next'), 'v')
step('CmdLex_Expect', sym('CmdLex_Expect'), 'ii', 3)
step('CmdLex_Expect', sym('CmdLex_Expect'), 'ii', 2)
step('CmdLex_Next', sym('CmdLex_Next'), 'v')
for i in range(6):
	step('CmdLex_GetInt', sym('CmdLex_GetInt'), 'ip', 99)
for i in range(3):
	step('CmdLex_GetDouble', sym('CmdLex_GetDouble'), 'iD', 9.5)
check('CmdLex double', lambda: SDTest.Peek(sym('g_Cl') + 0x8, 'd'))
step('CmdLex_GetInt ident', sym('CmdLex_GetInt'), 'ip', 99)
step('CmdLex_GetDouble ident', sym('CmdLex_GetDouble'), 'iD', 9.5)
step('CmdLex_Next', sym('CmdLex_Next'), 'v')
step('CmdLex_IsChar', sym('CmdLex_IsChar'), 'ii', ord(';'))
step('CmdLex_IsChar', sym('CmdLex_IsChar'), 'ii', ord(','))
step('CmdLex_GetInt char', sym('CmdLex_GetInt'), 'ip', 99)
step('CmdLex_NeedChar', sym('CmdLex_NeedChar'), 'ii', ord(';'))
step('CmdLex_Next', sym('CmdLex_Next'), 'v')
step('CmdLex_SkipBlock', sym('CmdLex_SkipBlock'), 'vi', 0)
step('CmdLex_SkipToAny', sym('CmdLex_SkipToAny'), 'vs', ',)')
step('CmdLex_NeedChar', sym('CmdLex_NeedChar'), 'ii', ord(')'))
step('CmdLex_SkipTo', sym('CmdLex_SkipTo'), 'vi', ord(')'))
step('CmdLex_GetDouble ident', sym('CmdLex_GetDouble'), 'iD', 9.5)
step('CmdLex_SkipBlock -1', sym('CmdLex_SkipBlock'), 'vi', -1)
step('CmdLex_SkipTo end', sym('CmdLex_SkipTo'), 'vi', ord('#'))
step('CmdLex_GetInt end', sym('CmdLex_GetInt'), 'ip', 99)
step('CmdLex_GetDouble end', sym('CmdLex_GetDouble'), 'iD', 9.5)
step('CmdLex_NeedChar end', sym('CmdLex_NeedChar'), 'ii', ord(';'))
step('CmdLex_SkipToAny end', sym('CmdLex_SkipToAny'), 'vs', '{')
call('Lex_Close', sym('Lex_Close'), 'vi', 1)
check('Lex out', SDTest.Out)
call('Lex_Error', sym('Lex_Error'), 'viss', 2, 'info %s', 'no file')
check('Lex out', SDTest.Out)

# engine3d Util/Errors.c: no reports, they end the game (Con_FatalErrorf) or open a message box
for c in (0, 1, 2, 3, 49, 50, 199, 200, -1, -2):
	call('Err_Str', sym('Err_Str'), 'si', c)
errtext = 'sdtest error'
code = SDTest.Call('Err_Register', 'is', errtext)[0]
check('Err_Register', lambda: code)
call('Err_Str new', sym('Err_Str'), 'si', code)
call('Err_Unregister', sym('Err_Unregister'), 'si', code)
call('Err_Unregister again', sym('Err_Unregister'), 'si', code)
call('Err_Unregister 10', sym('Err_Unregister'), 'si', 10)
call('Err_Unregister 200', sym('Err_Unregister'), 'si', 200)
call('Dbg_FrameRect', sym('Dbg_FrameRect'), 'viiiii', 1, 2, 30, 40, 0x123456)
call('Dbg_FillRect', sym('Dbg_FillRect'), 'viiiii', 1, 2, 30, 40, 0x654321)

# engine3d Util/DynList.c: elements of 8 bytes written with String_Copy; addresses differ between the runs, so only
# offsets and contents are compared
def ldin(d, contents=1):
	n = SDTest.Peek(d + 12, '@i')
	r = [SDTest.Peek(d + 8, '@i'), n, SDTest.Peek(d + 16, '@i'), SDTest.Peek(d + 20, '@i') & 0xffff,
		SDTest.Peek(d + 22, '@b'), SDTest.Peek(d + 4, '@i') != 0]
	for i in range(n * contents):
		r.append(SDTest.Peek(SDTest.Call('DynList_Get', 'iii', d, i)[0], '@s'))
	return tuple(r)
# new elements are not initialized: their contents are written before the next look
def lstep(name, rva, sig, d, *args):
	def f(rva=rva, sig=sig, d=d, args=args):
		r = apply(SDTest.Call, (rva, sig, d) + args)[0]
		if sig[0] == 'i' and r and rva in (sym('DynList_Last'), sym('DynList_AddOne'), sym('DynList_Add'), sym('DynList_Insert')):
			r = r - SDTest.Peek(d + 4, '@i')
		return (r, ldin(d, rva not in (sym('DynList_AddOne'), sym('DynList_Add'), sym('DynList_Insert'))))
	check(name + fmt(args), f)
def put(d, i, text):
	SDTest.Call('String_Copy', 'sisi', SDTest.Call('DynList_Get', 'iii', d, i)[0], text, 8)

d = SDTest.Call('DynList_NewBlock', 'iii', 8, 2)[0]
check('DynList_NewBlock', lambda: ldin(d))
lstep('DynList_Count', sym('DynList_Count'), 'ii', d)
lstep('DynList_Add', sym('DynList_Add'), 'iii', d, 3)
for i in range(3):
	put(d, i, 'e%d' % i)
lstep('DynList_AddOne', sym('DynList_AddOne'), 'ii', d)
put(d, 3, 'e3')
lstep('DynList_Last', sym('DynList_Last'), 'ii', d)
lstep('DynList_Insert', sym('DynList_Insert'), 'iii', d, 1)
put(d, 1, 'ins')
lstep('DynList_Insert', sym('DynList_Insert'), 'iii', d, 0)
put(d, 0, 'first')
check('LDin', lambda: ldin(d))
lstep('DynList_Move', sym('DynList_Move'), 'viii', d, 0, 3)
lstep('DynList_Move', sym('DynList_Move'), 'viii', d, 4, 1)
lstep('DynList_Move', sym('DynList_Move'), 'viii', d, -1, 0)
lstep('DynList_Move', sym('DynList_Move'), 'viii', d, 2, -1)
lstep('DynList_Move', sym('DynList_Move'), 'viii', d, 2, 2)
lstep('DynList_Copy', sym('DynList_Copy'), 'viiii', d, 0, 3, 2)
lstep('DynList_Copy', sym('DynList_Copy'), 'viiii', d, 3, 1, 2)
lstep('DynList_Copy', sym('DynList_Copy'), 'viiii', d, 2, 2, 1)
lstep('DynList_Delete', sym('DynList_Delete'), 'vii', d, 1)
lstep('DynList_Delete', sym('DynList_Delete'), 'vii', d, 3)
lstep('DynList_RemoveLast', sym('DynList_RemoveLast'), 'vi', d)
lstep('DynList_RemoveLast', sym('DynList_RemoveLast'), 'vi', d)
lstep('DynList_RemoveLast', sym('DynList_RemoveLast'), 'vi', d)
lstep('DynList_RemoveLast', sym('DynList_RemoveLast'), 'vi', d)
lstep('DynList_Add', sym('DynList_Add'), 'iii', d, 5)
lstep('DynList_Clear', sym('DynList_Clear'), 'vi', d)
lstep('DynList_Add', sym('DynList_Add'), 'iii', d, 1)
check('DynList_Free', lambda: SDTest.Call('DynList_Free', 'vi', d))
d = SDTest.Call('DynList_New', 'ii', 4)[0]
check('DynList_New', lambda: ldin(d))
lstep('DynList_Add', sym('DynList_Add'), 'iii', d, 300)
check('DynList_Count', lambda: SDTest.Call('DynList_Count', 'ii', d))
lstep('DynList_Clear', sym('DynList_Clear'), 'vi', d)
check('DynList_Free', lambda: SDTest.Call('DynList_Free', 'vi', d))

# engine3d Util/DataFile.c and Dat_simp.c: lines added one by one (DataFile_Load only reads files in the packs)
inilines = ('; head comment', '[Main]', 'Name = Scrap  ; the name', '  Count=12', 'flag = SI', 'Float = 1.5', 'Big = 1e3',
	'Word = 65000', 'Item = one', 'Item = two words  ', 'Empty =', 'Semi = ;only comment', 'NoEq', '', '[ Other ]  ',
	'name = other\t;tab', '[]', 'Item = three')
def arch(dat):
	r = []
	for i in range(SDTest.Call('DataFile_Count', 'ii', dat)[0] + 1):
		lin = SDTest.Call('DataFile_Line', 'iii', dat, i)[0]
		r.append((SDTest.Call('DataFile_LineType', 'iii', dat, i)[0], SDTest.Peek(lin + 12, '@p')))
	return tuple(r)
# the first argument (the cDataFile address) is not part of the name
def dcall(name, rva, sig, *args):
	check(name + fmt(args[1:]), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))
def astep(name, rva, sig, *args):
	check(name + fmt(args[1:]), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), arch(args[0])))

dat = SDTest.Call('DataFile_New', 'i')[0]
check('DataFile_New', lambda: arch(dat))
for i in range(len(inilines)):
	SDTest.Call('DataFile_AddLine', 'visi', dat, inilines[i], i)
check('DataFile_AddLine', lambda: arch(dat))
for n in ('Main', 'main', 'Other', 'other', ' Other', 'Oth', '', 'None'):
	for f in (0, 1, 3, 16):
		dcall('DataFile_FindSection', sym('DataFile_FindSection'), 'iisi', dat, n, f)
for k in ('Name', 'name', 'Count', 'Item', 'Empty', 'Semi', 'NoEq', 'Nam', 'Missing'):
	for f in (0, 2, 9, 10):
		dcall('DataFile_FindKey', sym('DataFile_FindKey'), 'iisibi', dat, k, f, '', 260)
		dcall('DataFile_FindKey NULL', sym('DataFile_FindKey'), 'iisizi', dat, k, f, None, 0)
	dcall('DataFile_FindKey short', sym('DataFile_FindKey'), 'iisibi', dat, k, 2, '', 4)
for i in range(-1, 21):
	dcall('DataFile_SectionEnd', sym('DataFile_SectionEnd'), 'iii', dat, i)
	def val(i=i):
		r = SDTest.Call('DataFile_Value', 'iiip', dat, i, -5)
		return r[0] and (SDTest.Peek(r[0], '@s'), r[1]) or r
	check('DataFile_Value(%d)' % i, val)
	check('DataFile_Value NULL(%d)' % i, lambda i=i: SDTest.Call('DataFile_Value', 'iiiz', dat, i, None)[0] != 0)
for h in ('[abc]', '  [ a b ]  x', '[x]', '[\tlong name\t]'):
	call('DataFile_SectionName', sym('DataFile_SectionName'), 'vbsi', '', h, 260)
	call('DataFile_SectionName short', sym('DataFile_SectionName'), 'vbsi', '', h, 3)
dcall('DataFile_StrnCmp', sym('DataFile_StrnCmp'), 'iissi', dat, 'ABc', 'abd', 2)
astep('DataFile_SetValue', sym('DataFile_SetValue'), 'visi', dat, 'Scrapland', 3)
astep('DataFile_SetValue long', sym('DataFile_SetValue'), 'visi', dat, 'a much longer value than before', 3)
astep('DataFile_SetValue', sym('DataFile_SetValue'), 'visi', dat, 'x', 4)
astep('DataFile_SetValue none', sym('DataFile_SetValue'), 'visi', dat, 'x', 2)
astep('DataFile_AddSection', sym('DataFile_AddSection'), 'visi', dat, 'New', 99)
astep('DataFile_AddLine', sym('DataFile_AddLine'), 'visi', dat, 'k = v', -3)
astep('DataFile_Delete', sym('DataFile_Delete'), 'vii', dat, 1)
astep('DataFile_Delete 0', sym('DataFile_Delete'), 'vii', dat, 0)
astep('DataFile_Delete big', sym('DataFile_Delete'), 'vii', dat, 999)
astep('Dat_SetStr', sym('Dat_SetStr'), 'visss', dat, 'Main', 'Name', 'Changed')
astep('Dat_SetStr same', sym('Dat_SetStr'), 'visss', dat, 'Main', 'Name', 'Changed')
astep('Dat_SetStr new key', sym('Dat_SetStr'), 'visss', dat, 'Other', 'Added', 'yes')
astep('Dat_SetStr new section', sym('Dat_SetStr'), 'visss', dat, 'Fresh', 'k', 'v')
astep('Dat_SetBool', sym('Dat_SetBool'), 'vissi', dat, 'Fresh', 'b', 1)
astep('Dat_SetBool', sym('Dat_SetBool'), 'vissi', dat, 'Fresh', 'b', 0)
astep('Dat_SetInt', sym('Dat_SetInt'), 'vissi', dat, 'Fresh', 'n', -1234567)
# Proc_Scan of the original stops the game on a value of another type: only fitting keys and missing ones
miss = (('None', 'x'), ('Main', 'x'))
for sect, key in (('Main', 'flag'), ('Fresh', 'b')) + miss:
	dcall('Dat_GetBool', sym('Dat_GetBool'), 'iissp', dat, sect, key, 7)
# Count is 'x' since the DataFile_SetValue checks
for sect, key in (('Main', 'Word'), ('Fresh', 'n')) + miss:
	dcall('Dat_GetLong', sym('Dat_GetLong'), 'iissp', dat, sect, key, 7)
	dcall('Dat_GetInt', sym('Dat_GetLong@2a3300'), 'iissp', dat, sect, key, 7)
for sect, key in (('Main', 'Float'), ('Main', 'Big'), ('Main', 'Word')) + miss:
	dcall('Dat_GetFloat', sym('Dat_GetFloat'), 'iissp', dat, sect, key, 7)
	dcall('Dat_GetDouble', sym('Dat_GetDouble'), 'iissD', dat, sect, key, 7.5)
for sect, key in (('Main', 'Word'),) + miss:
	dcall('Dat_GetWord', sym('Dat_GetWord'), 'iissp', dat, sect, key, 7)
for sect, key in (('Main', 'flag'), ('Main', 'Count'), ('Main', 'Name'), ('Main', 'Item'), ('Main', 'Empty'), ('Main', 'Semi'),
		('Other', 'name'), ('Fresh', 'n')) + miss:
	dcall('Dat_GetText', sym('Dat_GetText'), 'iissbi', dat, sect, key, '', 8)
	dcall('Dat_GetStr', sym('Dat_GetStr'), 'iissbi', dat, sect, key, '', 260)
lst = SDTest.Call('DataFile_New', 'i')[0]
SDTest.Call('Dat_GetList', 'vissi', dat, 'Main', 'Item', lst)
check('Dat_GetList', lambda: arch(lst))
SDTest.Call('DataFile_Free', 'vi', lst)
SDTest.Call('DataFile_Dump', 'viii', dat, 0, 5)
f = SDTest.Call('crt_fopen', 'iss', 'sdini.out.txt', 'w')[0]
check('DataFile_Write', lambda: SDTest.Call('DataFile_Write', 'iii', dat, f))
SDTest.Call('crt_fclose', 'ii', f)
check('DataFile_Write file', lambda: open('sdini.out.txt').read())
SDTest.Call('DataFile_Free', 'vi', dat)
dat = SDTest.Call('DataFile_New', 'i')[0]
astep('DataFile_Load', sym('DataFile_Load'), 'iisi', dat, 'm3d.ini', 0)
dcall('DataFile_Load missing', sym('DataFile_Load'), 'iisi', dat, 'sdnofile.txt', 0)
dcall('Dat_GetBool', sym('Dat_GetBool'), 'iissp', dat, 'video', 'Sharpen', 7)
dcall('Dat_GetLong', sym('Dat_GetLong'), 'iissp', dat, 'ground', 'suelo_caras_cuadr', 7)
SDTest.Call('DataFile_Free', 'vi', dat)

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
	apply(call, ('Proc_Scan', sym('Proc_Scan'), sig, src, f) + args)
for n in ('C4', 'a0', 'G#9', 'Bb3', 'Ex-1', 'C', 'H2', 'C#b4', 'D##2'):
	call('Proc_Note', sym('Proc_Note'), 'ibszi', '', n, None, 16)
sino = SDTest.Peek(sym('g_ProcTypes') + 4, 'i')
for n in ('si', 'YES', 'false', 'Nope', 'S', ''):
	# the label without the table address, which differs between the exes
	check('Proc_TypeLookup%s' % fmt(('', n, 260)), lambda n=n: SDTest.Call('Proc_TypeLookup', 'ibsii', '', n, sino, 260))
	check('Proc_TypeLookup short%s' % fmt(('', n, 1)), lambda n=n: SDTest.Call('Proc_TypeLookup', 'ibsii', '', n, sino, 1))
sdtipo = 'sdtipo'
bucle = SDTest.Peek(sym('g_ProcTypes') + 16, 'i')
check('Proc_DefineType', lambda: SDTest.Call('Proc_DefineType', 'isi', sdtipo, bucle)[0])
check('Proc_DefineType again', lambda: SDTest.Call('Proc_DefineType', 'isi', sdtipo, bucle)[0])
check('Proc_DefineType taken', lambda: SDTest.Call('Proc_DefineType', 'isi', 'tipo_si_no', bucle)[0])
call('Proc_Scan sdtipo', sym('Proc_Scan'), 'issp', 'LOCAL', '&(sdtipo)l', 7)
for i in range(16):
	if SDTest.Peek(sym('g_ProcTypes') + 12 * i, 'p') == 'sdtipo':
		SDTest.Poke(sym('g_ProcTypes') + 12 * i, 0)
check('Proc types', lambda: map(lambda i: SDTest.Peek(sym('g_ProcTypes') + 12 * i, 'p'), range(16)))

# engine3d Engine/FileIO.c: reads only (writing blocks and seeking need the pack building mode, else a CHECK
# fails), CFileIO on a block file of the packs
for t in ('ABCD', 'AB', '', 'ABCDEF', 'DUM'):
	call('IdToDword', sym('IdToDword'), 'is', t)
for n in ('Bin/Scrap.exe', 'Bin', 'sdnofile.txt', 'm3d.ini'):
	call('File_Access', sym('File_Access'), 'isi', n, 0)
	call('File_AccessPack', sym('File_AccessPack'), 'isi', n, 0)
	call('File_Check', sym('File_Check'), 'isii', n, 0, 1)
	call('File_Stat', sym('File_Stat'), 'isb', n, '')
	call('File_StatPack', sym('File_StatPack'), 'isb', n, '')
call('File_RelPath', sym('File_RelPath'), 'vbs', '', 'a/b/c.txt')
def readall(n):
	r = SDTest.Call('FileIO_ReadAll', 'iszp', n, None, 0)
	text = SDTest.Call('String_Copy', 'sbii', '', r[0], 40)[1]
	return (r[1], text)
check('FileIO_ReadAll', lambda: readall('m3d.ini'))
f = SDTest.Call('FileIO_Open', 'iss', 'm3d.ini', 'rb')[0]
check('FileIO_Open', lambda: f != 0)
call('FileIO_Open missing', sym('FileIO_Open'), 'iss', 'sdnofile.txt', 'rb')
check('FileIO_Size', lambda: SDTest.Call('FileIO_Size', 'ii', f))
for i in range(4):
	check('FileIO_Gets', lambda: (SDTest.Call('FileIO_Gets', 'sbii', '', 260, f), SDTest.Call('FileIO_Tell', 'ii', f)))
check('FileIO_Gets short', lambda: SDTest.Call('FileIO_Gets', 'sbii', '', 4, f))
check('FileIO_Getc', lambda: SDTest.Call('FileIO_Getc', 'ii', f))
check('FileIO_Read', lambda: SDTest.Call('FileIO_Read', 'ibiii', '', 3, 4, f))
check('FileIO_Tell', lambda: SDTest.Call('FileIO_Tell', 'ii', f))
check('FileIO_Close', lambda: SDTest.Call('FileIO_Close', 'ii', f))
io = SDTest.Call('Mem_Alloc', 'ii', 16)[0]
def cio():
	return (SDTest.Peek(io, '@b'), SDTest.Peek(io + 4, '@i'), SDTest.Peek(io + 8, '@i') != 0, SDTest.Peek(io + 12, '@i'))
def iostep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, io) + args), cio()))
check('CFileIO_ctor missing', lambda: (SDTest.Call('CFileIO_ctor', 'tiiss', io, 'sdnofile.dum', 'rb')[0] == io, cio()))
iostep('CFileIO_Error', sym('CFileIO_Error'), 'tiiz', None)
iostep('CFileIO_Read error', sym('CFileIO_Read'), 'tiibi', '', 4)
iostep('CFileIO_ReadString error', sym('CFileIO_ReadString'), 'tiibi', '', 4)
iostep('CFileIO_Unsupported', sym('CFileIO_Unsupported'), 'tii')
iostep('CFileIO_dtor', sym('CFileIO_dtor'), 'tvi')
check('CFileIO_ctor', lambda: (SDTest.Call('CFileIO_ctor', 'tiiss', io, 'levels/fake/map/map3d.dum', 'rb')[0] == io, cio()))
iostep('CFileIO_BeginBlockSize', sym('CFileIO_BeginBlockSize'), 'tiisp', 'DUM', 0)
iostep('CFileIO_ReadInt', sym('CFileIO_ReadInt'), 'tii')
iostep('CFileIO_ReadBool', sym('CFileIO_ReadBool'), 'tbi')
iostep('CFileIO_Read4', sym('CFileIO_Read4@21c350'), 'tiip', 0)
iostep('CFileIO_ReadStringNull', sym('CFileIO_ReadStringNull'), 'tiibip', '', 260, 7)
iostep('CFileIO_Read', sym('CFileIO_Read'), 'tiibi', '', 12)
iostep('CFileIO_Read 0', sym('CFileIO_Read'), 'tiibi', '', 0)
iostep('CFileIO_SetError', sym('CFileIO_SetError'), 'tiis', 'test')
iostep('CFileIO_Read after', sym('CFileIO_Read'), 'tiibi', '', 4)
iostep('CFileIO_ReadStringNull after', sym('CFileIO_ReadStringNull'), 'tiibip', '', 260, 7)
iostep('CFileIO_Finish', sym('CFileIO_Finish'), 'tii')
iostep('CFileIO_Close', sym('CFileIO_Close'), 'tii')
SDTest.Call('Mem_Free', 'vi', io)

# Math/MathUtils.c
execfile('sdmath.py')
def fcall(name, rva, sig, *args):
	check(name, lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))
for i in range(40):
	a, b, c, p = vec(), vec(), vec(), vec()
	box = (min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2]), max(a[0], b[0]), max(a[1], b[1]), max(a[2], b[2]))
	box2 = (c[0], c[1], c[2], p[0], p[1], p[2])
	fcall('Box_GetCorner', sym('Box_GetCorner'), 'tvFFi', box, (9.0, 9.0, 9.0), i % 10 - 1)
	fcall('Box_Merge', sym('Box_Merge'), 'vFFi', box, box2, i % 3 == 0)
	t = rnd(-1.0, 1.0)
	fcall('Float_Approach', sym('Float_Approach'), 'vFff', (a[0],), b[0], t)
	fcall('Vec3_Approach', sym('Vec3_Approach'), 'vFFf', a, b, t * 3)
	fcall('Vec3_Bounce', sym('Vec3_Bounce'), 'vFffFF', a, rnd(0.1, 5), rnd(0.1, 5), b, c)
	fcall('Tri_HasPoint', sym('Tri_HasPoint'), 'bFFFF', p, a, b, c)
	fcall('Tri_HasPoint in', sym('Tri_HasPoint'), 'bFFFF', ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3), a, b, c)
	fcall('Dist_PointSegment', sym('Dist_PointSegment'), 'fFFFF', a, b, p, (0.0, 0.0, 0.0))
	fcall('Dist2_PointSegment', sym('Dist2_PointSegment'), 'fFFFF', a, b, p, (0.0, 0.0, 0.0))
	fcall('Plane_SafeSet', sym('Plane_SafeSet'), 'tbFFFF', (0.0, 0.0, 0.0, 0.0), a, b, c)
	fcall('Quad_Solve', sym('Quad_Solve'), 'bfffFF', rnd(), rnd(), rnd(), (0.0,), (0.0,))
	fcall('Dist_PointTriangle', sym('Dist_PointTriangle'), 'fFFFFF', p, a, b, c, (0.0, 0.0, 0.0))
	fcall('Dist2_PointTriangle', sym('Dist2_PointTriangle'), 'fFFFFF', p, a, b, c, (0.0, 0.0, 0.0))
	fcall('Vec3_Reflect', sym('Vec3_Reflect'), 'vFFF', a, b, (0.0, 0.0, 0.0))
for x in specials:
	v = (x, 1.0, x)
	fcall('Float_IsValid', sym('Float_IsValid'), 'bf', x)
	fcall('Vec3_IsValid', sym('Vec3_IsValid'), 'bF', v)
	fcall('Float_Approach s', sym('Float_Approach'), 'vFff', (x,), 1.0, 0.5)
	fcall('Dist_PointSegment s', sym('Dist_PointSegment'), 'fFFFF', v, (1.0, 2.0, 3.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
	fcall('Dist_PointSegment s2', sym('Dist_PointSegment'), 'fFFFF', (0.0, 0.0, 0.0), (1.0, 2.0, 3.0), v, (0.0, 0.0, 0.0))
	fcall('Dist2_PointSegment s', sym('Dist2_PointSegment'), 'fFFFF', (0.0, 0.0, 0.0), v, (1.0, 2.0, 3.0), (0.0, 0.0, 0.0))
	fcall('Plane_SafeSet s', sym('Plane_SafeSet'), 'tbFFFF', (0.0, 0.0, 0.0, 0.0), v, (1.0, 2.0, 3.0), (3.0, 1.0, 2.0))
	fcall('Quad_Solve s', sym('Quad_Solve'), 'bfffFF', x, 2.0, x, (0.0,), (0.0,))
	fcall('Tri_HasPoint s', sym('Tri_HasPoint'), 'bFFFF', v, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
fcall('Plane_SafeSet degenerate', sym('Plane_SafeSet'), 'tbFFFF', (0.0, 0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (2.0, 2.0, 2.0), (3.0, 3.0, 3.0))
fcall('Dist2_PointSegment point', sym('Dist2_PointSegment'), 'fFFFF', (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (3.0, 1.0, 2.0), (0.0, 0.0, 0.0))

# Math/Matrix.c, Quat.c and the rotations of MathUtils.c
ident = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
def mat():
	r = []
	for i in range(16):
		r.append(rnd(-3.0, 3.0))
	return tuple(r)
def rotmat(ang):
	return SDTest.Call('Matrix_Rotate', 'vFF', ident, ang)[1]
def affine():
	m = list(rotmat(vec()))
	m[12], m[13], m[14] = rnd(), rnd(), rnd()
	return tuple(m)
def quat():
	return (rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0))
zero3 = (0.0, 0.0, 0.0)
for i in range(30):
	a, b, m, ang = mat(), mat(), affine(), vec()
	fcall('Matrix_Identity', sym('Matrix_Identity'), 'tvF', a)
	fcall('Matrix_Copy', sym('Matrix_Copy'), 'tvFF', a, b)
	fcall('Matrix_Copy 2', sym('Matrix_Copy@15c830'), 'tvFF', a, b)
	fcall('Matrix_FromFloats', sym('Matrix_FromFloats'), 'tvFF', a, b)
	fcall('Matrix_Mul', sym('Matrix_Mul'), 'tvFFF', a, ident, b)
	fcall('Matrix_Rotate', sym('Matrix_Rotate'), 'vFF', a, ang)
	fcall('Matrix_GetEuler', sym('Matrix_GetEuler'), 'tvFF', rotmat(ang), zero3)
	fcall('Matrix_GetEuler any', sym('Matrix_GetEuler'), 'tvFF', a, zero3)
	fcall('Matrix_GetRotAng', sym('Matrix_GetRotAng'), 'tvFF', a, zero3)
	fcall('Matrix_InvertAffine', sym('Matrix_InvertAffine'), 'bFF', ident, m)
	fcall('Matrix_InvertAffine any', sym('Matrix_InvertAffine'), 'bFF', ident, a)
	fcall('Matrix_Scale', sym('Matrix_Scale'), 'tvFF', a, ang)
	fcall('Matrix_Perspective', sym('Matrix_Perspective'), 'bFffff', a, rnd(0.1, 3.0), rnd(0.5, 2.0), rnd(0.1, 10.0), rnd(5.0, 100.0))
	fcall('Matrix_RotX', sym('Matrix_RotX'), 'tvFf', a, ang[0])
	fcall('Matrix_RotY', sym('Matrix_RotY'), 'tvFf', a, ang[1])
	fcall('Matrix_RotZ', sym('Matrix_RotZ'), 'tvFf', a, ang[2])
	fcall('Matrix_RotAxis', sym('Matrix_RotAxis'), 'tvFFf', a, vec(), ang[0])
	fcall('Matrix_Translate', sym('Matrix_Translate'), 'tvFF', a, ang)
	fcall('Matrix_TransformProject', sym('Matrix_TransformProject'), 'bFFF', zero3, vec(), a)
	fcall('Matrix_TransformDir', sym('Matrix_TransformDir'), 'vFFF', zero3, vec(), a)
	fcall('Matrix_TransformParts', sym('Matrix_TransformParts'), 'vFFFFFF', (0.0,), (0.0,), (0.0,), (0.0,), vec(), a)
	fcall('Matrix_TransformParts NULL', sym('Matrix_TransformParts'), 'vzFzFFF', None, (0.0,), None, (0.0,), vec(), a)
	q1, q2 = quat(), quat()
	fcall('Quat_Mul', sym('Quat_Mul'), 'vFFF', (0.0, 0.0, 0.0, 0.0), q1, q2)
	fcall('Quat_FromAxisAngle', sym('Quat_FromAxisAngle'), 'vFFf', (0.0, 0.0, 0.0, 0.0), vec(), ang[1])
	fcall('Quat_ToAxisAngle', sym('Quat_ToAxisAngle'), 'vFFF', zero3, (0.0,), q1)
	fcall('Quat_ToMatrix', sym('Quat_ToMatrix'), 'vFF', a, q1)
	fcall('Quat_Slerp', sym('Quat_Slerp'), 'vFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, rnd(0.0, 1.0))
	fcall('Quat_Slerp same', sym('Quat_Slerp'), 'vFFFf', (0.0, 0.0, 0.0, 0.0), q1, q1, rnd(0.0, 1.0))
	v = vec()
	fcall('Vec3_GetRotAng', sym('Vec3_GetRotAng'), 'vFF', zero3, v)
	fcall('Vec3_RotateSC', sym('Vec3_RotateSC'), 'vFFFF', zero3, v, vec(), vec())
	fcall('Vec3_RotateInv', sym('Vec3_RotateInv'), 'vFFF', zero3, v, ang)
	fcall('Vec3_NormAng', sym('Vec3_NormAng'), 'vFF', zero3, (ang[0] * 3, ang[1] * 7, ang[2]))
	fcall('NormAng', sym('NormAng'), 'ff', ang[0] * 5)
	fcall('Vec2_Rotate', sym('Vec2_Rotate'), 'vFFf', (0.0, 0.0), (v[0], v[1]), ang[2])
	fcall('Vec3_Rotate', sym('Vec3_Rotate'), 'vFFF', zero3, v, ang)
	fcall('Vec3_RotateXY', sym('Vec3_RotateXY'), 'vFFff', zero3, v, ang[0], ang[1])
for x in (0.0, 3.1415925, 3.1415927, -3.1415927, 6.2831855, -6.2831855, 1e10, -1e-30, inf, nan):
	fcall('NormAng s', sym('NormAng'), 'ff', x)
fcall('Matrix_GetEuler up', sym('Matrix_GetEuler'), 'tvFF', rotmat((1.5707964, 0.3, 0.2)), zero3)
fcall('Matrix_GetEuler down', sym('Matrix_GetEuler'), 'tvFF', rotmat((-1.5707964, 0.3, 0.2)), zero3)
fcall('Matrix_Perspective flat', sym('Matrix_Perspective'), 'bFffff', ident, 1.0, 1.0, 5.0, 5.001)
fcall('Matrix_Perspective narrow', sym('Matrix_Perspective'), 'bFffff', ident, 0.0, 1.0, 1.0, 10.0)
fcall('Matrix_TransformProject w0', sym('Matrix_TransformProject'), 'bFFF', zero3, (1.0, 2.0, 3.0), (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0))
fcall('Quat_ToAxisAngle identity', sym('Quat_ToAxisAngle'), 'vFFF', zero3, (0.0,), (0.0, 0.0, 0.0, 1.0))
# the engine3d copies and the more of Engine/cuaternio.cpp; unit quaternions from Quat_FromEuler
def uquat():
	a = vec()
	return SDTest.Call('Quat_FromEuler', 'vFfff', (0.0, 0.0, 0.0, 0.0), a[0], a[1], a[2])[1]
for i in range(30):
	a, ang = mat(), vec()
	fcall('Quat_FromEuler', sym('Quat_FromEuler'), 'vFfff', (0.0, 0.0, 0.0, 0.0), ang[0], ang[1], ang[2])
	q1, q2 = uquat(), uquat()
	fcall('Quat_ToAxisAngle 2', sym('Quat_ToAxisAngle@2a4160'), 'vFFF', zero3, (0.0,), q1)
	fcall('Quat_Slerp 2', sym('Quat_Slerp@2a45e0'), 'vFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, rnd(0.0, 1.0))
	fcall('Quat_DotAbs', sym('Quat_DotAbs'), 'fFF', q1, q2)
	fcall('Quat_DotAbs same', sym('Quat_DotAbs'), 'fFF', q1, q1)
	fcall('Quat_ToMatrixD3D', sym('Quat_ToMatrixD3D'), 'vFF', a, q1)
	fcall('Quat_SlerpFlip', sym('Quat_SlerpFlip'), 'bFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, rnd(0.0, 1.0))
	fcall('Quat_SlerpFlip near', sym('Quat_SlerpFlip'), 'bFFFf', (0.0, 0.0, 0.0, 0.0), q1, q1, rnd(0.0, 1.0))
	fcall('Quat_SlerpFlip any', sym('Quat_SlerpFlip'), 'bFFFf', (0.0, 0.0, 0.0, 0.0), quat(), quat(), rnd(0.0, 1.0))
# Engine/calculo3d.cpp
for i in range(30):
	a, v, w = mat(), vec(), vec()
	fcall('Matrix_AddPos', sym('Matrix_AddPos'), 'vFF', a, v)
	fcall('Vec3_TransformH', sym('Vec3_TransformH'), 'iFFF', zero3, v, a)
	fcall('Vec3_TransformDir0', sym('Vec3_TransformDir0'), 'iFFF', zero3, v, a)
	fcall('Sph_InCone', sym('Sph_InCone'), 'bFfFFf', v, rnd(0.0, 3.0), w, vec(), rnd(0.0, 3.0))
	fcall('Vec3_StrDeg', sym('Vec3_StrDeg'), 'sFii', v, i % 4, i % 7)
	fcall('Vec3_Str', sym('Vec3_Str'), 'sFii', v, (i + 1) % 4, i % 9)
for c, r, a, d, ang in (((0.0, 0.0, 5.0), 1.0, zero3, (0.0, 0.0, 1.0), 0.5), ((0.0, 5.0, 0.0), 1.0, zero3, (0.0, 0.0, 1.0), 0.5),
		((0.0, 0.0, 5.0), 6.0, zero3, (0.0, 0.0, 1.0), 0.1), ((1.0, 1.0, 1.0), 0.5, zero3, zero3, 1.0),
		((0.0, 3.0, 4.0), 5.0, zero3, (1.0, 0.0, 0.0), 0.0), ((0.0, 0.0, -5.0), 0.1, zero3, (0.0, 0.0, 2.0), 3.1)):
	fcall('Sph_InCone s', sym('Sph_InCone'), 'bFfFFf', c, r, a, d, ang)
SDTest.Call('crt_srand', 'vi', 77)
for mn, mx, av, tr in ((0, 10, 3, 5), (5, 5, 5, 3), (5, 5, 4, 3), (-3, 3, 0, 0), (0, 1, 0, 1), (0, 1, 1, 20), (100, 200, 150, 2)):
	call('Rand_Other', sym('Rand_Other'), 'iiiii', mn, mx, av, tr)
mex = SDTest.Call('Mem_Calloc', 'ii', 64)[0]
mname = SDTest.Call('Mem_Calloc', 'ii', 16)[0]
SDTest.Call('Str_Copy', 'sisi', mname, 'sqrt', 16)
for t, hi in ((1, 0), (3, 0x3ff00000), (6, -0x40100000), (0, 0), (9, 0x40590000)):
	SDTest.Poke(mex - 0x400000, t)
	SDTest.Poke(mex + 4 - 0x400000, mname)
	SDTest.Poke(mex + 12 - 0x400000, hi)
	check('E3D_MathErr' + fmt((t, hi)), lambda: SDTest.Call('E3D_MathErr', 'ii', mex))
call('E3D_MathErr NULL', sym('E3D_MathErr'), 'ii', 0)
SDTest.Call('Mem_Free', 'vi', mex)
SDTest.Call('Mem_Free', 'vi', mname)
for q1, q2 in (((0.0, 0.0, 0.9, 0.1), (0.0, 0.0, -0.9, 0.1)), ((0.7, 0.0, 0.0, -0.7), (0.0, 0.7, 0.0, 0.7)),
		((0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0, -1.0)), ((0.0, 0.99, 0.0, -0.14), (0.0, -0.98, 0.0, -0.2))):
	for t in (0.0, 0.3, 1.0):
		fcall('Quat_SlerpFlip s', sym('Quat_SlerpFlip'), 'bFFFf', (0.0, 0.0, 0.0, 0.0), q1, q2, t)

# engine3d Engine/IniFile.c on two ini files of the packs
ini = SDTest.Call('Mem_Alloc', 'ii', 4)[0]
def istep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig, ini) + args))
check('CIniFile_ctorLoad', lambda: SDTest.Call('CIniFile_ctorLoad', 'tiisz', ini, 'm3d.ini', None)[0] == ini)
istep('CIniFile_Load', sym('CIniFile_Load'), 'tiiszi', 'map3d.ini', 'levels/menu/map/', 1)
istep('CIniFile_Load missing', sym('CIniFile_Load'), 'tiiszi', 'sdnofile.ini', None, 0)
istep('CIniFile_Count', sym('CIniFile_Count'), 'tii')
for sect, key in (('video', 'VSync'), ('video', 'Sharpen'), ('video', 'TextBPP'), ('ground', 'suelo_caras_cuadr'),
		('mapa_sombras', 'scale'), ('mapa_sombras', 'sombras_suaves'), ('ground', 'model_ambient'), ('Model', 'TexturePath'),
		('sky', 'path'), ('none', 'x'), ('video', 'none')):
	if key in ('VSync', 'Sharpen', 'sombras_suaves', 'none', 'x'):
		istep('CIniFile_GetBool', sym('CIniFile_GetBool'), 'tbissi', sect, key, 7)
		istep('CIniFile_ReadBool', sym('CIniFile_ReadBool'), 'tiissp', sect, key, 7)
	if key in ('TextBPP', 'suelo_caras_cuadr', 'none', 'x'):
		istep('CIniFile_GetInt', sym('CIniFile_GetInt'), 'tiissi', sect, key, -5)
		istep('CIniFile_ReadInt', sym('CIniFile_ReadInt'), 'tiissp', sect, key, 7)
	if key in ('scale', 'TextBPP', 'none', 'x'):
		istep('CIniFile_GetFloat', sym('CIniFile_GetFloat'), 'tfissf', sect, key, 2.5)
		istep('CIniFile_ReadFloat', sym('CIniFile_ReadFloat'), 'tiissp', sect, key, 7)
	if key in ('model_ambient', 'none', 'x'):
		istep('CIniFile_GetColor', sym('CIniFile_GetColor'), 'tiissi', sect, key, 0x11223344)
	istep('CIniFile_GetText', sym('CIniFile_GetText'), 'tsissbiz', sect, key, '', 40, None)
	istep('CIniFile_GetText def', sym('CIniFile_GetText'), 'tsissbis', sect, key, '', 40, 'default')
	istep('CIniFile_ReadText', sym('CIniFile_ReadText'), 'tiissbi', sect, key, '', 6)
	if key in ('none', 'x'):
		istep('CIniFile_GetVec2', sym('CIniFile_GetVec2'), 'tviFssF', (0.0, 0.0), sect, key, (1.0, 2.0))
		istep('CIniFile_GetVec3', sym('CIniFile_GetVec3'), 'tviFssF', (0.0, 0.0, 0.0), sect, key, (1.0, 2.0, 3.0))
for n in ('video', 'ground', 'sky', 'none'):
	istep('CIniFile_FindSection', sym('CIniFile_FindSection'), 'tiisi', n, 0)
istep('CIniFile_FindKey', sym('CIniFile_FindKey'), 'tiisbii', 'Depth', '', 40, 1)
for i in range(0, 12):
	istep('CIniFile_LineText', sym('CIniFile_LineText'), 'tsii', i)
	istep('CIniFile_LineType', sym('CIniFile_LineType'), 'tiii', i)
	istep('CIniFile_GetSectionName', sym('CIniFile_GetSectionName'), 'tbiibi', i, '', 40)
istep('CIniFile_SetStr', sym('CIniFile_SetStr'), 'tiisss', 'video', 'VSync', 'SI')
istep('CIniFile_SetStr same', sym('CIniFile_SetStr@262390'), 'tiisss', 'video', 'VSync', 'SI')
istep('CIniFile_SetInt', sym('CIniFile_SetInt'), 'tiissi', 'video', 'Depth', 32)
istep('CIniFile_SetInt 2', sym('CIniFile_SetInt@262370'), 'tiissi', 'sd', 'n', -7)
istep('CIniFile_SetFloat', sym('CIniFile_SetFloat'), 'tiissfi', 'sd', 'f', 1.0 / 3, 3)
istep('CIniFile_SetFloat 2', sym('CIniFile_SetFloat@262300'), 'tiissfi', 'sd', 'g', -2.5, 0)
istep('CIniFile_SetYesNo', sym('CIniFile_SetYesNo'), 'tiissi', 'sd', 'y', 1)
istep('CIniFile_SetBool', sym('CIniFile_SetBool'), 'tiissi', 'sd', 'b', 0)
istep('CIniFile_AddLine', sym('CIniFile_AddLine'), 'tvisi', '; sd line', -1)
istep('CIniFile_AddLine 2', sym('CIniFile_AddLine'), 'tvisi', '; sd first', 0)
istep('CIniFile_DeleteLine', sym('CIniFile_DeleteLine'), 'tvii', 3)
istep('CIniFile_Save', sym('CIniFile_Save'), 'tiiszi', 'sdini2.txt', None, 0)
check('CIniFile_Save file', lambda: open('sdini2.txt').read())
istep('CIniFile_Clear', sym('CIniFile_Clear'), 'tvi')
istep('CIniFile_Count', sym('CIniFile_Count'), 'tii')
istep('CIniFile_dtor', sym('CIniFile_dtor'), 'tvi')
check('CIniFile_ctor', lambda: SDTest.Call('CIniFile_ctor', 'tii', ini)[0] == ini)
istep('CIniFile_Count', sym('CIniFile_Count'), 'tii')
SDTest.Call('CIniFile_dtor', 'tvi', ini)
SDTest.Call('Mem_Free', 'vi', ini)

# engine3d Engine/HashTable.c
th = SDTest.Call('Mem_Alloc', 'ii', 12)[0]
def tstep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig, th) + args))
def tnode(n):
	if not n:
		return None
	return SDTest.Peek(n + 4, '@p'), SDTest.Peek(n + 8, '@i')
check('CHashTable_ctor', lambda: SDTest.Call('CHashTable_ctor', 'tiii', th, 7)[0] == th)
keys = ('a', 'b', 'abc', 'ABC', '', 'Init.py', 'x' * 40, 'a\xedb', 'h', 'o', 'v')
for i in range(len(keys)):
	tstep('CHashTable_Slot', sym('CHashTable_Slot'), 'tiis', keys[i])
	tstep('CHashTable_Add', sym('CHashTable_Add'), 'tiisii', keys[i], 100 + i, 0)
tstep('CHashTable_Add again', sym('CHashTable_Add'), 'tiisii', 'abc', 5, 0)
for k in (0, -1, 0x1234, 7):
	tstep('CHashTable_AddInt', sym('CHashTable_AddInt'), 'tiiiii', k, k * 3, 0)
tstep('CHashTable_AddInt again', sym('CHashTable_AddInt'), 'tiiiii', 0x1234, 1, 0)
for k in keys + ('c', 'Abc', '00001234', 'x' * 39):
	tstep('CHashTable_Get', sym('CHashTable_Get'), 'tiisp', k, -9)
	tstep('CHashTable_Get NULL', sym('CHashTable_Get'), 'tiisi', k, 0)
	check('CHashTable_Find' + fmt((k,)), lambda k=k: tnode(SDTest.Call('CHashTable_Find', 'tiisi', th, k, SDTest.Call('CHashTable_Slot', 'tiis', th, k)[0])[0]))
for k in (0, -1, 0x1234, 7, 8):
	tstep('CHashTable_GetInt', sym('CHashTable_GetInt'), 'tiiip', k, -9)
check('CHashTable_Insert', lambda: tnode(SDTest.Call('CHashTable_Insert', 'tiisii', th, 'abc', 2, 55)[0]))
tstep('CHashTable_List count', sym('CHashTable_List'), 'tiiii', 0, 0)
def tlist():
	r = SDTest.Call('CHashTable_GetList', 'tiipp', th, 0, -1)
	arr, n = r[1], r[2]
	nodes = []
	for i in range(n):
		nodes.append(tnode(SDTest.Peek(arr + 4 * i, '@i')))
	small = SDTest.Call('CHashTable_List', 'tiiii', th, arr, 3)[0]
	full = SDTest.Call('CHashTable_List', 'tiiii', th, arr, n)[0]
	lock = SDTest.Peek(th + 8, '@i')
	freed = SDTest.Call('CHashTable_FreeList', 'tiip', th, arr)
	return r[0], n, nodes, small, full, lock, freed, SDTest.Peek(th + 8, '@i')
check('CHashTable_GetList', tlist)
SDTest.Call('CHashTable_dtor', 'tvi', th)
SDTest.Call('Mem_Free', 'vi', th)

# engine3d Engine/ReadData.c on an ini of the packs (only pack files open, one at a time), Read_String on a buffer
# put in g_ReadString
def lee():
	return SDTest.Peek('g_ReadString', 'p'), SDTest.Peek('g_ReadStack', 'i') != 0
def leestep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), lee()))
call('Read_Error NULL', sym('Read_Error'), 'vz', None)
leestep('Read_Open', sym('Read_Open'), 'vzsi', 'levels/menu/', 'map/map3d.ini', 1)
leestep('Read_Expect', sym('Read_Expect'), 'vs', '[suelo]')
leestep('Read_Text', sym('Read_Text'), 'vzbi', 'model_ambient', '', 40)
leestep('Read_Text short', sym('Read_Text'), 'vzbi', None, '', 5)
leestep('Read_NextLine', sym('Read_NextLine'), 'i')
leestep('Read_Text', sym('Read_Text'), 'vzbi', 'TexturePath =', '', 40)
leestep('Read_Expect', sym('Read_Expect'), 'vs', '[mapa_sombras]')
for t in ('escala =', 'resolucion =', 'gamma ='):
	leestep('Read_Float', sym('Read_Float'), 'vsF', t, (0.0,))
leestep('Read_Text', sym('Read_Text'), 'vzbi', 'sombras_suaves =', '', 40)
leestep('Read_Int', sym('Read_Int'), 'vsp', 'text_tam_x =', -1)
leestep('Read_Int', sym('Read_Int'), 'vsp', 'text_tam_y =', -1)
leestep('Read_Bytes', sym('Read_Bytes'), 'vbi', '', 7)
leestep('Read_Bytes 0', sym('Read_Bytes'), 'vbi', '', 0)
for i in range(3):
	leestep('Read_Byte', sym('Read_Byte'), 'i')
for i in range(16):
	leestep('Read_NextLine', sym('Read_NextLine'), 'i')
lbuf = SDTest.Call('Mem_Alloc', 'ii', 64)[0]
saved = SDTest.Peek('g_ReadString', 'i')
for t in ('"abc" x', ' \t"" y', '"a b c d e f"', '\t"tab\there"'):
	for n in (1, 4, 40):
		SDTest.Call('Str_Copy', 'sisi', lbuf, t, 64)
		SDTest.Poke('g_ReadString', lbuf)
		check('Read_String' + fmt((t, n)), lambda n=n, lbuf=lbuf: (SDTest.Call('Read_String', 'vbi', '', n), SDTest.Peek('g_ReadString', 'i') - lbuf))
SDTest.Call('Str_Copy', 'sisi', lbuf, ' "q" rest', 64)
SDTest.Poke('g_ReadString', lbuf)
check('Read_QText', lambda lbuf=lbuf: (SDTest.Call('Read_QText', 'vzbi', None, '', 40), SDTest.Peek('g_ReadString', 'i') - lbuf))
SDTest.Poke('g_ReadString', saved)
SDTest.Call('Mem_Free', 'vi', lbuf)
leestep('Read_Close', sym('Read_Close'), 'i')

# engine3d Engine/BmpFlt.c on images in heap memory (pixels set with the exe's memcpy), TXF files written here
def bnew(w, h, create=sym('CBmpFlt_Create')):
	b = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
	SDTest.Call(create, 'tviii', b, w, h)
	return b
def bset(b, vals):
	img = SDTest.Peek(b + 8, '@i')
	for i in range(0, len(vals), 64):
		part = tuple(vals[i:i + 64])
		SDTest.Call('crt_memcpy', 'iiFi', img + 4 * i, part, 4 * len(part))
def bget(b, ch=4):
	w, h, img = SDTest.Peek(b, '@i'), SDTest.Peek(b + 4, '@i'), SDTest.Peek(b + 8, '@i')
	if not img:
		return w, h, None
	px = []
	for i in range(w * h * ch):
		px.append(SDTest.Peek(img + 4 * i, '@i'))
	return w, h, plain(px)
def bfree(b):
	img = SDTest.Peek(b + 8, '@i')
	if img:
		SDTest.Call('operator_delete_array', 'vi', img)
	SDTest.Call('Mem_Free', 'vi', b)
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
check('CBmpFlt_Pixel', lambda: SDTest.Call('CBmpFlt_Pixel', 'tiiii', ba, 3, 2)[0] - SDTest.Peek(ba + 8, '@i'))
for r in ((0, 0, 0, 0, 3, 2, 256, 0), (1, 1, 0, 0, 9, 9, 128, 128), (4, 3, 1, 1, 5, 5, 7, 300), (2, 0, 2, 1, 1, 1, -64, 32),
		(0, 0, 0, 0, 0, 2, 256, 256), (6, 0, 0, 0, 2, 2, 256, 256)):
	bcheck('CBmpFlt_Blend' + fmt(r), ba, sym('CBmpFlt_Blend'), 'tviiiiiiiiii', (r[0], r[1], bb) + r[2:])
for d in ((2, 2, 15), (3, 3, 15), (7, 5, 15), (5, 4, 5), (1, 1, 8), (4, 1, 3)):
	bd = bnew(d[0], d[1])
	bset(bd, bvals(d[0] * d[1] * 4))
	bcheck('CBmpFlt_Resample' + fmt(d), bd, sym('CBmpFlt_Resample'), 'tviii', (ba, d[2]))
	bfree(bd)
bd = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
bcheck('CBmpFlt_Resample new', bd, sym('CBmpFlt_Resample'), 'tviii', (bb, 15))
bfree(bd)
bc = bnew(4, 2)
bstep('CBmpFlt_Fill', bc, sym('CBmpFlt_Fill'), 'tviF', (0.5, -2.0, 1e30, 0.0))
for e in ((1.0, 1.0), (2.5, 1.0), (0.5, 3.0), (-1.0, 2.0), (100.0, 0.01), (1.0, 0.0)):
	bset(bc, (0.0, 0.25, 1.0, -0.5, 3.0, 10.0, 1e-8, 700.0, -30.0, 2.0, 0.1, 0.9, 5.0, 6.0, 7.0, 8.0) * 2)
	bstep('CBmpFlt_Exposure', bc, sym('CBmpFlt_Exposure'), 'tviff', e[0], e[1])
bfree(bc)
bstep('CBmpFlt_Save', ba, sym('CBmpFlt_Save'), 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep('CBmpFlt_Save exists', ba, sym('CBmpFlt_Save'), 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep('CBmpFlt_Save extra', bb, sym('CBmpFlt_Save'), 'tiisisi', 'sdbmp2.TXF', 1, 'XYZW', 4)
bstep('CBmpFlt_Save ext', ba, sym('CBmpFlt_Save'), 'tiisizi', 'sdbmp.txt', 1, None, 0)
bstep('CBmpFlt_Save short', ba, sym('CBmpFlt_Save'), 'tiisizi', 'txf', 1, None, 0)
writefile('sdbad.txf', 'TXF\032' + 'x' * 40)
bl = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
bstep('CBmpFlt_Load', bl, sym('CBmpFlt_Load'), 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep('CBmpFlt_Load extra', bl, sym('CBmpFlt_Load'), 'tiisibi', 'sdbmp2.txf', 0, '', 4)
bstep('CBmpFlt_Load extra none', bl, sym('CBmpFlt_Load'), 'tiisibi', 'sdbmp.txf', 0, '', 4)
bstep('CBmpFlt_Load ext', bl, sym('CBmpFlt_Load'), 'tiisizi', 'sdbmp.dat', 0, None, 0)
bstep('CBmpFlt_Load missing', bl, sym('CBmpFlt_Load'), 'tiisizi', 'sdnone.txf', 0, None, 0)
bstep('CBmpFlt_Load bad', bl, sym('CBmpFlt_Load'), 'tiisizi', 'sdbad.txf', 0, None, 0)
writefile('sdshort.txf', 'TXF')
check('TXF_Load short', lambda bl=bl: (SDTest.Call('TXF_Load', 'isiizi', 'sdshort.txf', bl, 0, None, 0), bget(bl)))
bfree(bl)
bfree(ba)
bfree(bb)

# the RGB class of Engine/BmpFlt2.cpp
ba = bnew(5, 4, sym('CBmpFlt2_Create'))
bb = bnew(3, 2, sym('CBmpFlt2_Create'))
bset(ba, bvals(60))
bset(bb, bvals(18, -1.0, 2.0))
check('CBmpFlt2_Create', lambda: bget(ba, 3))
check('CBmpFlt2_Pixel', lambda: SDTest.Call('CBmpFlt2_Pixel', 'tiiii', ba, 3, 2)[0] - SDTest.Peek(ba + 8, '@i'))
for r in ((0, 0, 0, 0, 3, 2, 256, 0), (1, 1, 0, 0, 9, 9, 128, 128), (4, 3, 1, 1, 5, 5, 7, 300), (6, 0, 0, 0, 2, 2, 256, 256)):
	bcheck('CBmpFlt2_Blend' + fmt(r), ba, sym('CBmpFlt2_Blend'), 'tviiiiiiiiii', (r[0], r[1], bb) + r[2:], 3)
bd = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
bcheck('CBmpFlt2_Resample new', bd, sym('CBmpFlt2_Resample'), 'tviii', (bb, 15), 3)
bfree(bd)
bc = bnew(4, 2, sym('CBmpFlt2_Create'))
bstep2('CBmpFlt2_Fill', bc, sym('CBmpFlt2_Fill'), 'tviF', (0.5, -2.0, 1e30))
for k in (1.0, 2.0, -0.5, 0.0):
	bstep2('CBmpFlt2_Scale', bc, sym('CBmpFlt2_Scale'), 'tvif', k)
for e in ((1.0, 1.0), (2.5, 1.0), (0.5, 3.0), (-1.0, 2.0), (100.0, 0.01), (1.0, 0.0)):
	bset(bc, (0.0, 0.25, 1.0, -0.5, 3.0, 10.0, 1e-8, 700.0, -30.0, 2.0, 0.1, 0.9) * 2)
	bstep2('CBmpFlt2_Exposure', bc, sym('CBmpFlt2_Exposure'), 'tviff', e[0], e[1])
for e in ((1.0, 1.0), (2.2, 1.0), (0.5, 3.0), (-1.0, 2.0), (3.0, 0.0), (0.0, 1.0)):
	bset(bc, (0.0, 0.25, 1.0, -0.5, 3.0, 10.0, 1e-8, 700.0, -30.0, 2.0, 0.1, 0.9) * 2)
	bstep2('CBmpFlt2_Gamma', bc, sym('CBmpFlt2_Gamma'), 'tviff', e[0], e[1])
bfree(bc)
bstep2('CBmpFlt2_Save', ba, sym('CBmpFlt2_Save'), 'tiisizi', 'sdbmp3.txf', 0, None, 0)
bstep2('CBmpFlt2_Save exists', ba, sym('CBmpFlt2_Save'), 'tiisizi', 'sdbmp3.txf', 0, None, 0)
bstep2('CBmpFlt2_Save extra', bb, sym('CBmpFlt2_Save'), 'tiisisi', 'sdbmp4.txf', 1, 'XYZW', 4)
bstep2('CBmpFlt2_Save ext', ba, sym('CBmpFlt2_Save'), 'tiisizi', 'sdbmp.txt', 1, None, 0)
bl = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
bstep2('CBmpFlt2_Load', bl, sym('CBmpFlt2_Load'), 'tiisizi', 'sdbmp3.txf', 0, None, 0)
bstep2('CBmpFlt2_Load extra', bl, sym('CBmpFlt2_Load'), 'tiisibi', 'sdbmp4.txf', 0, '', 4)
bstep2('CBmpFlt2_Load version 1', bl, sym('CBmpFlt2_Load'), 'tiisizi', 'sdbmp.txf', 0, None, 0)
bstep2('CBmpFlt2_Load ext', bl, sym('CBmpFlt2_Load'), 'tiisizi', 'sdbmp.dat', 0, None, 0)
check('TXF2_Load short', lambda bl=bl: (SDTest.Call('TXF2_Load', 'isiizi', 'sdshort.txf', bl, 0, None, 0), bget(bl, 3)))
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
ca = bnew(6, 5, sym('CBmp_Create'))
cb = bnew(3, 4, sym('CBmp_Create'))
va = cvals(30)
cset(ca, va)
cset(cb, cvals(12))
check('CBmp_Create', lambda: bget(ca, 1))
check('CBmp_Pixel', lambda: SDTest.Call('CBmp_Pixel', 'tiiii', ca, 5, 4)[0] - SDTest.Peek(ca + 8, '@i'))
for t in (0.0, 0.5, 1.0, 0.25, -0.5, 2.0, 0.999, 0.0039):
	call('Color_Lerp', sym('Color_Lerp'), 'iiif', 0x10ff8040, -0x7f00ff01, t)
for r in ((0, 0, 0, 0, 3, 4, 15), (1, 1, 0, 0, 9, 9, 5), (4, 3, 1, 1, 5, 5, 8), (2, 0, 2, 1, 1, 1, 6), (0, 0, 0, 0, 0, 2, 15), (7, 0, 0, 0, 2, 2, 15)):
	bcheck('CBmp_Copy' + fmt(r), ca, sym('CBmp_Copy'), 'tviiiiiiiii', (r[0], r[1], cb) + r[2:], 1)
for r in ((0, 0, 0, 0, 3, 2, 256, 0), (1, 1, 0, 0, 9, 9, 128, 128), (2, 1, 1, 0, 2, 3, -300, 600), (0, 0, 0, 0, 3, 4, 0x200000, -0x200000), (4, 3, 1, 1, 5, 5, 7, 300)):
	bcheck('CBmp_Blend' + fmt(r), ca, sym('CBmp_Blend'), 'tviiiiiiiiii', (r[0], r[1], cb) + r[2:], 1)
# the game seeds rand with the time: srand of the exe
SDTest.Call('crt_srand', 'vi', 12345)
for k in (0, 1, 2, 3, 4):
	cset(ca, va)
	cstep('CBmp_Effect', ca, sym('CBmp_Effect'), 'tvii', k)
for e in ((1.0, 255.0), (2.5, 255.0), (0.5, 100.0), (-1.0, 255.0), (0.01, 255.0)):
	cset(ca, va)
	cstep('CBmp_Exposure', ca, sym('CBmp_Exposure'), 'tviff', e[0], e[1])
for e in ((1.0, 255.0), (2.2, 255.0), (0.45, 255.0), (-1.0, 128.0), (3.0, 0.0)):
	cset(ca, va)
	cstep('CBmp_Gamma', ca, sym('CBmp_Gamma'), 'tviff', e[0], e[1])
for m in ((7, 7), (1, 8), (15, 1), (8, 6), (2, 15)):
	cset(ca, va)
	cstep('CBmp_MixChannels', ca, sym('CBmp_MixChannels'), 'tviii', m[0], m[1])
cset(ca, va)
for u in (0.0, 0.1, 0.5, 0.77, 1.0):
	for v in (0.0, 0.33, 0.9, 1.0):
		check('CBmp_Sample' + fmt((u, v)), lambda u=u, v=v: SDTest.Call('CBmp_Sample', 'tiipff', ca, 0, u, v)[1])
cstep('CBmp_Sharpen', ca, sym('CBmp_Sharpen'), 'tvi')
cset(ca, va)
cstep('CBmp_Blur', ca, sym('CBmp_Blur'), 'tvi')
cset(ca, va)
cstep('CBmp_Fill', ca, sym('CBmp_Fill'), 'tvii', 0x11223344)
cset(ca, va)
cc = bnew(6, 4, sym('CBmp_Create'))
cset(cc, cvals(24))
for d in ((3, 2, 15), (3, 2, 5), (6, 4, 15), (6, 4, 9), (4, 3, 15), (7, 6, 15), (2, 2, 10), (1, 1, 15)):
	bd = bnew(d[0], d[1], sym('CBmp_Create'))
	cset(bd, cvals(d[0] * d[1]))
	bcheck('CBmp_Resample' + fmt(d), bd, sym('CBmp_Resample'), 'tviii', (cc, d[2]), 1)
	bfree(bd)
bd = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
bcheck('CBmp_Resample new', bd, sym('CBmp_Resample'), 'tviii', (ca, 15), 1)
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
cl = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
for n, fl in (('models/elements/templeyakuzzi/aceite-r.bmp', 0), ('models/elements/templeyakuzzi/aceite-r.bmp', 0x100),
		('levels/abindoor/2d/map0.tga', 0), ('2d/white.tga', 0), ('2d/loading/00icon.alpha.tga', 0),
		('levels/menu/map/map3d.0001.lmap1.tga', 0), ('sdnone.bmp', 0), ('sdnone.tex', 0), ('sdnone.TGA', 0)):
	check('CBmp_Load' + fmt((n, fl)), lambda n=n, fl=fl: (SDTest.Call('CBmp_Load', 'tiisi', cl, n, fl)[0], csum(cl)))
check('CBmp_Load map0', lambda: (SDTest.Call('CBmp_Load', 'tiisi', cl, 'levels/abindoor/2d/map0.tga', 0)[0], bget(cl, 1)))
for rva, n in ((sym('BMP_Load'), 'BMP_Load'), (sym('TEX_Load'), 'TEX_Load'), (sym('TGA_Load'), 'TGA_Load')):
	check(n + ' bad', lambda rva=rva: (SDTest.Call(rva, 'isii', 'levels/menu/map/map3d.ini', cl, 0)[0], csum(cl)))
bfree(cl)
cs = bnew(8, 3, sym('CBmp_Create'))
A, B, C, D = 0x11223344, 0x11223355, -0x66778899, 0x01020304
cset(cs, (A, A, A, A, B, C, C, D,  A, B, A, B, C, C, B, B,  D, D, C, C, C, A, A, 0x22223355))
for n, fl in (('sdimg.bmp', 0), ('sdimg.bmp', 0), ('sdimg2.BMP', 1), ('sdimg.tex', 0), ('sdimg.tex', 0), ('sdimg.tga', 0),
		('sdimg.tga', 1), ('sdimga.tga', 3), ('sdimgx.txt', 0)):
	check('CBmp_Save' + fmt((n, fl)), lambda n=n, fl=fl: (SDTest.Call('CBmp_Save', 'tiisi', cs, n, fl)[0], readback(n)))
cr = bnew(130, 2, sym('CBmp_Create'))
v = []
for i in range(130):
	v.append(i / 3)
for i in range(130):
	v.append(5)
cset(cr, v)
check('TGA_Save long', lambda: (SDTest.Call('TGA_Save', 'isii', 'sdlong.tga', cr, 1)[0], readback('sdlong.tga')))
bfree(cr)
bfree(cs)

# engine3d Engine/ReadM3D.c: the M3D values in text (an ini of the packs) and binary (a bmp, the ini read as binary)
def m3dmode(binary, labels):
	v = SDTest.Peek('g_M3DBinary', 'i')
	SDTest.Poke('g_M3DBinary', (v & -0x10000) + binary + (labels << 8))
def mstep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), SDTest.Peek('g_ReadString', 'p')))
m3dsaved = SDTest.Peek('g_M3DBinary', 'i')
m3dmode(0, 1)
SDTest.Call('Read_Open', 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
mstep('M3D_Label', sym('M3D_Label'), 'vs', '[suelo]')
mstep('M3D_ReadQuat', sym('M3D_ReadQuat'), 'vsFi', 'model_ambient =', (0.0, 0.0, 0.0, 0.0), 1)
mstep('M3D_Label', sym('M3D_Label'), 'vs', '[Model]')
mstep('M3D_ReadString', sym('M3D_ReadString'), 'ssbii', 'TexturePath =', '', 40, 0)
mstep('M3D_Label', sym('M3D_Label'), 'vs', '[mapa_sombras]')
for t in ('escala =', 'resolucion =', 'gamma ='):
	mstep('M3D_ReadFloat', sym('M3D_ReadFloat'), 'fs', t)
mstep('M3D_ReadBool', sym('M3D_ReadBool'), 'bs', 'sombras_suaves =')
mstep('M3D_ReadInt', sym('M3D_ReadInt'), 'is', 'text_tam_x =')
mstep('M3D_ReadInt', sym('M3D_ReadInt'), 'is', 'text_tam_y =')
for t in ('radiosidad =', 'ilum_global =', 'no_luz_amb =', 'modulate4x ='):
	mstep('M3D_ReadBool', sym('M3D_ReadBool'), 'bs', t)
SDTest.Call('Read_Close', 'i')
SDTest.Call('Read_Open', 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
mstep('M3D_Label', sym('M3D_Label'), 'vs', '[suelo]')
mstep('M3D_ReadVec3', sym('M3D_ReadVec3'), 'vsFi', 'model_ambient =', (0.0, 0.0, 0.0), 0)
mstep('M3D_ReadFloat', sym('M3D_ReadFloat'), 'fz', None)
SDTest.Call('Read_Close', 'i')
SDTest.Call('Read_Open', 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
mstep('M3D_Label', sym('M3D_Label'), 'vs', '[suelo]')
mstep('M3D_ReadVec2', sym('M3D_ReadVec2'), 'vsF', 'model_ambient =', (0.0, 0.0))
mstep('M3D_ReadVec2 next', sym('M3D_ReadVec2'), 'vzF', None, (0.0, 0.0))
SDTest.Call('Read_Close', 'i')
lbuf = SDTest.Call('Mem_Alloc', 'ii', 64)[0]
saved = SDTest.Peek('g_ReadString', 'i')
for t in ('"abc" x', ' "" y', '"long text here"'):
	for n in (2, 40):
		SDTest.Call('Str_Copy', 'sisi', lbuf, t, 64)
		SDTest.Poke('g_ReadString', lbuf)
		check('M3D_ReadString quoted' + fmt((t, n)), lambda n=n: SDTest.Call('M3D_ReadString', 'szbii', None, '', n, 1))
	SDTest.Call('Str_Copy', 'sisi', lbuf, t, 64)
	SDTest.Poke('g_ReadString', lbuf)
	check('M3D_ReadString quoted cad' + fmt((t,)), lambda: (SDTest.Call('M3D_ReadString', 'szzii', None, None, 0, 1), SDTest.Peek('g_M3DString', 's')))
SDTest.Poke('g_ReadString', saved)
SDTest.Call('Mem_Free', 'vi', lbuf)
m3dmode(1, 0)
SDTest.Call('Read_Open', 'vzsi', None, 'models/elements/templeyakuzzi/aceite-r.bmp', 1)
for i in range(4):
	mstep('M3D_ReadInt bin', sym('M3D_ReadInt'), 'iz', None)
mstep('M3D_ReadBool bin', sym('M3D_ReadBool'), 'bz', None)
mstep('M3D_ReadBool bin', sym('M3D_ReadBool'), 'bs', 'x')
mstep('M3D_ReadString bin', sym('M3D_ReadString'), 'szbii', None, '', 40, 0)
for i in range(3):
	mstep('M3D_ReadInt bin', sym('M3D_ReadInt'), 'iz', None)
mstep('M3D_ReadFloat bin', sym('M3D_ReadFloat'), 'fz', None)
mstep('M3D_ReadVec3 bin', sym('M3D_ReadVec3'), 'vzFi', None, (0.0, 0.0, 0.0), 1)
mstep('M3D_ReadVec2 bin', sym('M3D_ReadVec2'), 'vzF', None, (0.0, 0.0))
mstep('M3D_ReadQuat bin', sym('M3D_ReadQuat'), 'vzFi', None, (0.0, 0.0, 0.0, 0.0), 0)
mstep('M3D_ReadTransform bin', sym('M3D_ReadTransform'), 'vsF', 'x', (0.0,) * 15)
for i in range(20):
	mstep('M3D_ReadInt bin', sym('M3D_ReadInt'), 'iz', None)
mstep('M3D_ReadQuat bin', sym('M3D_ReadQuat'), 'vzFi', None, (0.0, 0.0, 0.0, 0.0), 1)
SDTest.Call('Read_Close', 'i')
SDTest.Call('Read_Open', 'vzsi', None, '2d/white.tga', 1)
SDTest.Call('Read_Bytes', 'vbi', '', 18)
for i in range(4):
	mstep('M3D_ReadInt white', sym('M3D_ReadInt'), 'iz', None)
SDTest.Call('Read_Close', 'i')
SDTest.Call('Read_Open', 'vzsi', None, 'levels/menu/map/map3d.0001.lmap1.tga', 1)
SDTest.Call('Read_Bytes', 'vbi', '', 18)
for i in range(60):
	mstep('M3D_ReadInt lmap', sym('M3D_ReadInt'), 'iz', None)
SDTest.Call('Read_Close', 'i')
SDTest.Call('Read_Open', 'vzsi', None, '2d/loading/00icon.alpha.tga', 1)
SDTest.Call('Read_Bytes', 'vbi', '', 900)
for i in range(60):
	mstep('M3D_ReadInt icon', sym('M3D_ReadInt'), 'iz', None)
SDTest.Call('Read_Close', 'i')
m3dmode(1, 1)
SDTest.Call('Read_Open', 'vzsi', None, 'levels/menu/map/map3d.ini', 1)
SDTest.Call('Read_Bytes', 'vbi', '', 37)
mstep('M3D_Label bin', sym('M3D_Label'), 'vs', '\n\r\n[Model]\r\nT')
check('M3D_Label bin cad', lambda: SDTest.Peek('g_M3DString', 's'))
mstep('M3D_ReadIndex bin', sym('M3D_ReadIndex'), 'vi', -38)
SDTest.Call('Read_Close', 'i')
SDTest.Poke('g_M3DBinary', m3dsaved)

# engine3d Engine/Table.c: the array template instances; the readers on .sm3 files of the packs at offsets where a
# count and an element size follow
def tpeek(addr, n):
	v = []
	for i in range(n):
		v.append(SDTest.Peek(addr + 4 * i, '@i'))
	return v
def tnew(rva, num, words):
	t = SDTest.Call('Mem_Calloc', 'ii', 8)[0]
	SDTest.Call(rva, 'tvii', t, num)
	n, p = SDTest.Peek(t, '@i'), SDTest.Peek(t + 4, '@i')
	r = (n, p != 0)
	if words and p:
		r = r + (tpeek(p, words),)
	SDTest.Call('Mem_Free', 'vi', t)
	return r
for rva, sz in (('Table_Alloc4', 4), ('Table_Alloc2', 2), ('Table_Alloc1C', 0x1c), ('Table_AllocC', 0xc), ('Table_Alloc1', 1), ('Table_Alloc14', 0x14),
		('Table_Alloc14@28fcc0', 0x14), ('Table_Alloc18', 0x18), ('Table_Alloc2C', 0x2c), ('Table_Alloc10', 0x10), ('Table_Alloc4@2c2b10', 4), ('Table_Alloc4@2c2b80', 4),
		('Table_Alloc8', 8), ('Table_Alloc38', 0x38)):
	for num in (1, 7):
		check('Table_Alloc' + fmt((rva, sz, num)), lambda rva=rva, num=num: tnew(rva, num, 0))
# the last 2 bytes of each element stay as new[] left them
def tzero(num):
	r = tnew(sym('Table_Alloc10Z'), num, num * 4)
	v = r[2]
	for i in range(3, len(v), 4):
		v[i] = v[i] & 0xffff
	return r
for num in (1, 5):
	check('Table_Alloc10Z' + fmt((num,)), lambda num=num: tzero(num))
# objects: the count before them and the first word of each (the constructors leave other fields as they are; the
# one of 0x2c2c60 sets no first word, there only the count)
def tobjs(rva, num, sz, first):
	t = SDTest.Call('Mem_Calloc', 'ii', 8)[0]
	SDTest.Call(rva, 'tvii', t, num)
	n, p = SDTest.Peek(t, '@i'), SDTest.Peek(t + 4, '@i')
	words = []
	for i in range(num * first):
		words.append(SDTest.Peek(p + i * sz, '@i'))
	SDTest.Call('Mem_Free', 'vi', t)
	return n, SDTest.Peek(p - 4, '@i'), words
for rva, sz, tfirst in (('Table_New8', 8, 1), ('Table_NewA8', 0xa8, 1), ('Table_New24', 0x24, 1), ('Table_NewFC', 0xfc, 1),
		('Table_NewB4', 0xb4, 1), ('Table_New1C', 0x1c, 0)):
	check('Table_New' + fmt((rva, sz)), lambda rva=rva, sz=sz, tfirst=tfirst: tobjs(rva, 3, sz, tfirst))
tio = SDTest.Call('Mem_Alloc', 'ii', 16)[0]
def topen(name, skip):
	SDTest.Call('CFileIO_ctor', 'tiiss', tio, name, 'rb')
	while skip > 0:
		k = min(skip, 1000)
		SDTest.Call('CFileIO_Read', 'tiibi', tio, '', k)
		skip = skip - k
def tclose():
	SDTest.Call('CFileIO_Close', 'tii', tio)
	SDTest.Call('CFileIO_dtor', 'tvi', tio)
sm3 = 'levels/bank/map/directordiscovered/directordiscovered.sm3'
def aread(rva, name, off, sz):
	topen(name, off)
	r = SDTest.Call(rva, 'vipp', tio, 0, 0)
	p, n = r[1], r[2]
	data = None
	if p:
		data = tpeek(p, (n * sz + 3) / 4)
		# words in the range of heap addresses change with the layout of the build: masked
		for i in range(len(data)):
			if 0x100000 <= data[i] < 0x2000000:
				data[i] = 'addr'
	tclose()
	return n, p != 0, data
for rva, sz, off in (('Array_Read2', 2, 3564), ('Array_Read4', 4, 4242), ('Array_Read8', 8, 5852), ('Array_Read10', 0x10, 1824),
		('Array_Read10@2af4e0', 0x10, 1824), ('Array_Read6', 6, 5174), ('Array_ReadC', 0xc, 7209)):
	check('Array_Read' + fmt((rva, sz)), lambda rva=rva, sz=sz, off=off: aread(rva, sm3, off, sz))
def tread(rva, name, off, sz):
	topen(name, off)
	t = SDTest.Call('Mem_Calloc', 'ii', 8)[0]
	SDTest.Call(rva, 'vii', tio, t)
	n, p = SDTest.Peek(t, '@i'), SDTest.Peek(t + 4, '@i')
	data = None
	if p:
		data = tpeek(p, (n * sz + 3) / 4)
	SDTest.Call('Mem_Free', 'vi', t)
	tclose()
	return n, p != 0, data
check('Table_Read2', lambda: tread(sym('Table_Read2'), sm3, 3564, 2))
check('Table_ReadC', lambda: tread(sym('Table_ReadC'), sm3, 7209, 0xc))
check('Table_Read1C', lambda: tread(sym('Table_Read1C'), 'levels/fake/map/fake1/fake1.sm3', 3616, 0x1c))
SDTest.Call('Mem_Free', 'vi', tio)

# engine3d Engine/AnimVert.c on vertices and sets in heap memory
def avnew(frames):
	av = SDTest.Call('Mem_Calloc', 'ii', 16)[0]
	SDTest.Call('CAnimVert_AllocTr', 'tvii', av, len(frames) / 3)
	SDTest.Call('crt_memcpy', 'iiFi', SDTest.Peek(av + 4, '@i'), tuple(frames), 4 * len(frames))
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
check('CAnimVert_Frame', lambda: SDTest.Call('CAnimVert_Frame', 'tiii', av, 2)[0] - SDTest.Peek(av + 4, '@i'))
for f, t in ((0, 0.0), (0, 0.5), (1, 0.25), (2, 0.999), (3, 0.5), (4, 0.0)):
	avstep('CAnimVert_GetPos', av, sym('CAnimVert_GetPos'), 'tviifF', f, t, (10.0, 20.0, 30.0))
avstep('CAnimVert_Scale', av, sym('CAnimVert_Scale'), 'tvifff', 2.0, -1.0, 0.5)
avstep('CAnimVert_Optimize', av, sym('CAnimVert_Optimize'), 'tvi')
for f, t in ((0, 0.0), (0, 0.5), (1, 0.25), (2, 0.75), (3, 0.5), (5, 0.0)):
	avstep('CAnimVert_GetPos 2', av, sym('CAnimVert_GetPos'), 'tviifF', f, t, (10.0, 20.0, 30.0))
for a, b, t in ((0, 1, 0.5), (1, 3, 0.25), (2, 2, 0.7), (3, 0, 1.0)):
	avstep('CAnimVert_AddPos', av, sym('CAnimVert_AddPos'), 'tviiifF', a, b, t, (1.0, 2.0, 3.0))
avstep('CAnimVert_Scale 2', av, sym('CAnimVert_Scale'), 'tvifff', 3.0, 3.0, 3.0)
avstep('CAnimVert_FreeTr2', av, sym('CAnimVert_FreeTr2'), 'tvi')
check('CAnimVert_Delete', lambda: SDTest.Call('CAnimVert_Delete', 'tiii', av, 1)[0] == av)
for frames in ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0, 1.00001, 2.0, 3.0), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0002), (0.0, 0.0, 0.0, 1e6, -1e6, 5.0, 1e-3, 0.0, -2.0)):
	av = avnew(frames)
	avstep('CAnimVert_Optimize' + fmt((frames,)), av, sym('CAnimVert_Optimize'), 'tvi')
	SDTest.Call('CAnimVert_Delete', 'tiii', av, 1)
av = avnew(fr)
avstep('CAnimVert_Compress', av, sym('CAnimVert_Compress'), 'tvi')
avstep('CAnimVert_dtor', av, sym('CAnimVert_dtor'), 'tvi')
SDTest.Call('Mem_Free', 'vi', av)
eva = SDTest.Call('Mem_Calloc', 'ii', 8)[0]
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
evastep('CEVA_Alloc', sym('CEVA_Alloc'), 'tvii', 3)
check('CEVA_Get', lambda: SDTest.Call('CEVA_Get', 'tiii', eva, 2)[0] - SDTest.Peek(eva + 4, '@i'))
evastep('CEVA_MemSize', sym('CEVA_MemSize'), 'tii')
evastep('CEVA_NumAnimated', sym('CEVA_NumAnimated'), 'tii')
a = SDTest.Peek(eva + 4, '@i')
SDTest.Call('CAnimVert_AllocTr', 'tvii', a, 2)
SDTest.Call('crt_memcpy', 'iiFi', SDTest.Peek(a + 4, '@i'), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0), 24)
SDTest.Call('CAnimVert_AllocTr', 'tvii', a + 16, 1)
SDTest.Call('crt_memcpy', 'iiFi', SDTest.Peek(a + 20, '@i'), (5.0, 6.0, 7.0), 12)
SDTest.Call('CAnimVert_AllocTr', 'tvii', a + 32, 3)
SDTest.Call('crt_memcpy', 'iiFi', SDTest.Peek(a + 36, '@i'), (1.0, 2.0, 3.0, 2.0, 2.0, 3.0, 0.0, -1.0, 3.5), 36)
evastep('CEVA_MemSize', sym('CEVA_MemSize'), 'tii')
evastep('CEVA_NumAnimated', sym('CEVA_NumAnimated'), 'tii')
evastep('CEVA_Scale', sym('CEVA_Scale'), 'tvifff', 2.0, 2.0, 2.0)
evastep('CEVA_Optimize', sym('CEVA_Optimize'), 'tvi')
evastep('CEVA_MemSize', sym('CEVA_MemSize'), 'tii')
evastep('CEVA_NumAnimated', sym('CEVA_NumAnimated'), 'tii')
evastep('CEVA_Free', sym('CEVA_Free@2d4020'), 'tvi')
evastep('CEVA_Alloc', sym('CEVA_Alloc'), 'tvii', 2)
a = SDTest.Peek(eva + 4, '@i')
for i in range(2):
	SDTest.Call('CAnimVert_AllocTr', 'tvii', a + 16 * i, 2)
	SDTest.Call('crt_memcpy', 'iiFi', SDTest.Peek(a + 16 * i + 4, '@i'), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0), 24)
evastep('CEVA_Optimize static', sym('CEVA_Optimize'), 'tvi')
evastep('CEVA_Alloc', sym('CEVA_Alloc'), 'tvii', 4)
evastep('CEVA_dtor', sym('CEVA_Free'), 'tvi')
SDTest.Call('Mem_Free', 'vi', eva)

# engine3d Engine/Misc.c: logs, file and string helpers
for p in ('a.txt', 'dir/x/../y.txt', './a/./b', 'a/../../b', '../x', 'a\\b\\..\\c', 'C:\\abs\\p', '\\root', '/root', 'a..b/c', 'x/.../y',
		'..', '.', 'a/.', 'a/..', 'a//b', 'levels/menu/./map/../map/map3d.ini', 'a/b/c/../../d', 'x:', 'x:y/../z', '.hidden', 'a/.b'):
	for n in (-1, 8, 300):
		call('Path_Normalize', sym('Path_Normalize'), 'vbsi', '', p, n)
	call('Path_Copy', sym('Path_Copy'), 'sbsi', '', p, -1)
for t in ('', '  ', 'word', '  two words', '"quoted text" rest', ' "unclosed', '\t""', '"a"'):
	call('String_Unquote', sym('String_Unquote'), 'sbs', '', t)
call('String_Unquote NULL', sym('String_Unquote'), 'sbz', '', None)
for x in (1.0, 2.0, 0.25, 3.0, 100.0, 1e-6, 12345.678, 0.5, 7.0):
	fcall('RSqrt_Estimate', sym('RSqrt_Estimate'), 'ff', x)
	fcall('RSqrt', sym('RSqrt'), 'ff', x)
for pat, n in (('*.ini', 'MAP3D.INI'), ('a?c', 'ABC'), ('*', 'X'), ('x*', 'Y')):
	call('Wild_MatchLower', sym('Wild_MatchLower'), 'iss', pat, n)
for n in ('levels/menu/map/map3d.ini', 'sdnone.txt', 'Bin/Scrap.exe', 'sdcall.py', ''):
	call('File_Exists', sym('File_Exists'), 'bs', n)
	check('File_Time' + fmt((n,)), lambda n=n: SDTest.Call('File_Time', 'is', n)[0] != 0)
	call('File_TimeChanged', sym('File_TimeChanged'), 'bisi', 0, n, 5)
check('File_TimeDiff', lambda: SDTest.Call('File_TimeDiff', 'issi', 'sdcall.py', 'sdcall.py', -7))
call('File_TimeDiff missing', sym('File_TimeDiff'), 'issi', 'sdnone.txt', 'sdcall.py', -7)
writefile('sdbk.txt', 'backup')
call('File_Backup', sym('File_Backup'), 'isi', 'sdbk.txt', 0)
check('File_Backup file', lambda: (readback('sdbk.txt'), readback('sdbk.txt.bak')))
for f in (sym('E3D_LogPrint'), sym('E3D_LogDebug'), sym('E3D_LogDebug2'), sym('E3D_LogVerbose'), sym('E3D_LogBlue'), sym('E3D_LogWhite'), sym('E3D_LogInfo'), sym('E3D_Log')):
	call('E3D_Log', f, 'viss', sym('g_FileLog') + 0x400000, 'sdlog %s', 'x')
for m in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0xff, 0):
	check('cLog_Write' + fmt((m,)), lambda m=m: SDTest.Call('cLog_Write', 'tviis', sym('g_FileLog') + 0x400000, m, 'sdlog write'))
call('E3D_DebugPrintf', sym('E3D_DebugPrintf'), 'vs', 'sdlog debug')

# engine3d Engine/Camera.c on cameras in heap memory; nodes are fake (only Refs), kept above 1
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
cam = SDTest.Call('Mem_Calloc', 'ii', 0x78)[0]
cam2 = SDTest.Call('Mem_Calloc', 'ii', 0x78)[0]
nod = SDTest.Call('Mem_Calloc', 'ii', 16)[0]
nod2 = SDTest.Call('Mem_Calloc', 'ii', 16)[0]
SDTest.Poke(nod + 8 - 0x400000, 5)
SDTest.Poke(nod2 + 8 - 0x400000, 7)
check('CCamera_ctor', lambda: (SDTest.Call('CCamera_ctor', 'tii', cam)[0] == cam, camget(cam)))
for i in range(12):
	SDTest.Call('crt_memcpy', 'iiFi', cam + 8, vec() + vec() + vec(), 36)
	SDTest.Call('crt_memcpy', 'iiFi', cam + 0x2c, (rnd(0.1, 2.0), rnd(5.0, 500.0), 0.0, 1000.0, rnd(0.1, 2.5), rnd(0.5, 2.0), rnd(0.5, 2.0)), 28)
	if i % 3 == 2:
		SDTest.Poke(cam + 0x48 - 0x400000, 0)
	else:
		SDTest.Poke(cam + 0x48 - 0x400000, 0x463)
	camstep('CCamera_ViewMatrix', cam, sym('CCamera_ViewMatrix'), 'tviF', (0.0,) * 16)
	camget16('CCamera_GetView', cam, sym('CCamera_GetView'))
	camstep('CCamera_ProjMatrix', cam, sym('CCamera_ProjMatrix'), 'tbiF', (0.0,) * 16)
	camget16('CCamera_GetProj', cam, sym('CCamera_GetProj'))
	camstep('CCamera_Move', cam, sym('CCamera_Move'), 'tvif', rnd())
	camstep('CCamera_Strafe', cam, sym('CCamera_Strafe'), 'tviff', rnd(), rnd())
	camstep('CCamera_UpdateVectors', cam, sym('CCamera_UpdateVectors'), 'tvi')
	camstep('CCamera_LookAt', cam, sym('CCamera_LookAt'), 'tviFFF', (0.0,) * 16, vec(), vec())
camstep('CCamera_LookAt zero', cam, sym('CCamera_LookAt'), 'tviFFF', (0.0,) * 16, (0.0, 0.0, 0.0), (0.0, 1.0, 0.0))
camstep('CCamera_LookAt parallel', cam, sym('CCamera_LookAt'), 'tviFFF', (0.0,) * 16, (0.0, 2.0, 0.0), (0.0, 1.0, 0.0))
SDTest.Call('crt_memcpy', 'iiFi', cam + 0x2c, (5.0, 5.001, 0.0, 1000.0, 1.0, 1.0, 1.0), 28)
camstep('CCamera_ProjMatrix flat', cam, sym('CCamera_ProjMatrix'), 'tbiF', (0.0,) * 16)
SDTest.Call('crt_memcpy', 'iiFi', cam + 0x2c, (1.0, 100.0, 0.0, 1000.0, 0.001, 1.0, 1.0), 28)
camstep('CCamera_ProjMatrix narrow', cam, sym('CCamera_ProjMatrix'), 'tbiF', (0.0,) * 16)
def refs():
	return SDTest.Peek(nod + 8, '@i'), SDTest.Peek(nod2 + 8, '@i')
check('CCamera_SetNode', lambda: (SDTest.Call('CCamera_SetNode', 'tvii', cam, nod), refs(), SDTest.Peek(cam + 0x70, '@i') == nod))
check('CCamera_SetNode 2', lambda: (SDTest.Call('CCamera_SetNode', 'tvii', cam, nod2), refs(), SDTest.Peek(cam + 0x70, '@i') == nod2))
check('CCamera_SetNodeTarget', lambda: (SDTest.Call('CCamera_SetNodeTarget', 'tvii', cam, nod), refs()))
check('CCamera_ctorCopy', lambda: (SDTest.Call('CCamera_ctorCopy', 'tiii', cam2, cam)[0] == cam2, refs(), camget(cam2)[2:19] == camget(cam)[2:19]))
check('CCamera_dtor', lambda: (SDTest.Call('CCamera_dtor', 'tvi', cam2), refs(), camget(cam2)))
check('CCamera_Clear', lambda: (SDTest.Call('CCamera_Clear', 'tvi', cam), refs(), camget(cam)))
SDTest.Call('CCamera_SetNode', 'tvii', cam, nod)
check('CCamera_Delete', lambda: (SDTest.Call('CCamera_Delete', 'tiii', cam, 0)[0] == cam, refs(), camget(cam)))
check('CCamera_SetNode NULL', lambda: (SDTest.Call('CCamera_SetNode', 'tvii', cam, 0), refs()))
SDTest.Call('Mem_Free', 'vi', cam)
SDTest.Call('Mem_Free', 'vi', cam2)
SDTest.Call('Mem_Free', 'vi', nod)
SDTest.Call('Mem_Free', 'vi', nod2)

# AI/AStar.c: the binary heap with the A* callbacks over fake ElemAStar records, and the int key hash table
NEL = 12
els = []
refs = SDTest.Call('Mem_Calloc', 'ii', NEL * 8)[0]
for i in range(NEL):
	e = SDTest.Call('Mem_Calloc', 'ii', 0x14)[0]
	SDTest.Poke(e - 0x400000, 100 + i)
	if i % 3:
		SDTest.Poke(e + 0x10 - 0x400000, refs + 8 * i)
	els.append(e)
def setf(i, f):
	SDTest.Call('crt_memcpy', 'iiFi', els[i] + 8, (f,), 4)
fs = (5.0, 3.0, 8.0, 1.0, 9.0, 3.0, 7.0, 2.0, 6.0, 4.0, 0.5, 10.0)
for i in range(NEL):
	setf(i, fs[i])
heap = SDTest.Call('Mem_Calloc', 'ii', 0x1c)[0]
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
check('cBinaryHeap_ctor', lambda: (SDTest.Call('cBinaryHeap_ctor', 'tiiiiii', heap, 8, symva('AStar_ElemLess'), symva('AStar_ElemEq'), symva('AStar_ElemSetIndex'))[0] == heap, heapget()))
for i in range(10):
	check('cBinaryHeap_Push' + fmt((i,)), lambda i=i: (SDTest.Call('cBinaryHeap_Push', 'tbip', heap, els[i])[0], heapget()))
check('cBinaryHeap_Top', lambda: (lambda r: (r[0], slotel(r[1])))(SDTest.Call('cBinaryHeap_Top', 'tbip', heap, 0)))
for i in (0, 1, 4, 8, 9):
	check('cBinaryHeap_Get' + fmt((i,)), lambda i=i: (lambda r: (r[0], slotel(r[1])))(SDTest.Call('cBinaryHeap_Get', 'tbipi', heap, 0, i)))
def hfind(i):
	r = SDTest.Call('cBinaryHeap_Find', 'tbippp', heap, els[i], 0, 0)
	return r[0], r[2], slotel(r[3])
for i in (3, 7, 11):
	check('cBinaryHeap_Find' + fmt((i,)), lambda i=i: hfind(i))
setf(11, 0.25)
hstepn('cBinaryHeap_Set 11 5', sym('cBinaryHeap_Set'), 'tbipi', (els[11], 5))
setf(2, 20.0)
hstepn('cBinaryHeap_Set 2 1', sym('cBinaryHeap_Set'), 'tbipi', (els[2], 1))
hstepn('cBinaryHeap_Set 2 0', sym('cBinaryHeap_Set'), 'tbipi', (els[2], 0))
hstep('cBinaryHeap_Count', sym('cBinaryHeap_Count'), 'tii')
setf(6, -1.0)
hstep('cBinaryHeap_Fix', sym('cBinaryHeap_Fix'), 'tvii', 7)
for i in range(10):
	hstep('cBinaryHeap_Pop', sym('cBinaryHeap_Pop'), 'tbi')
hstep('cBinaryHeap_SiftUp', sym('cBinaryHeap_SiftUp'), 'tbii', 1)
hstep('cBinaryHeap_SiftDown', sym('cBinaryHeap_SiftDown'), 'tbii', 1)
hstep('cBinaryHeap_Clear', sym('cBinaryHeap_Clear'), 'tvi')
check('cBinaryHeap_Delete', lambda: SDTest.Call('cBinaryHeap_Delete', 'tiii', heap, 0)[0] == heap)
SDTest.Call('Mem_Free', 'vi', heap)
for e in els:
	SDTest.Call('Mem_Free', 'vi', e)
SDTest.Call('Mem_Free', 'vi', refs)
tbl = SDTest.Call('Mem_Calloc', 'ii', 12)[0]
SDTest.Poke(tbl - 0x400000, 7)
SDTest.Poke(tbl + 4 - 0x400000, SDTest.Call('Mem_Calloc', 'ii', 28)[0])
def tstep(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig, tbl) + args)[0])
for k in (0, 1, 15, 16, 255, 256, 0x7fffffff, -1, 12345, 0x1000, 3):
	tstep('AStarTable_Slot', sym('AStarTable_Slot'), 'tiii', k)
	check('AStarTable_Add' + fmt((k,)), lambda k=k: SDTest.Call('AStarTable_Add', 'tbiiFi', tbl, k, (k % 1000, 1, 7), 0)[0:1])
check('AStarTable_Add again', lambda: SDTest.Call('AStarTable_Add', 'tbiiFi', tbl, 15, (1, 2, 3), 1)[0:1])
check('AStarTable_Add again quiet', lambda: SDTest.Call('AStarTable_Add', 'tbiiFi', tbl, 256, (1, 2, 3), 0)[0:1])
def tget(k):
	r = SDTest.Call('AStarTable_Get', 'tbiip', tbl, k, 0)
	if not r[0]:
		return r[0]
	return r[0], tpeek(r[1], 4)
for k in (0, 15, 256, -1, 9, 3, 77):
	check('AStarTable_Get' + fmt((k,)), lambda k=k: tget(k))
	check('AStarTable_Find' + fmt((k,)), lambda k=k: SDTest.Call('AStarTable_Find', 'tiiii', tbl, k, SDTest.Call('AStarTable_Slot', 'tiii', tbl, k)[0])[0] != 0)
check('AStarTable_Insert', lambda: tpeek(SDTest.Call('AStarTable_Insert', 'tiiiiF', tbl, 99, 2, (4, 5, 6))[0], 4))
check('AStarTable_Clear', lambda: (SDTest.Call('AStarTable_Clear', 'tvi', tbl), tpeek(SDTest.Peek(tbl + 4, '@i'), 7)))
check('AStarTable_Add after', lambda: SDTest.Call('AStarTable_Add', 'tbiiFi', tbl, 5, (1, 2, 3), 0)[0:1])
check('AStarTable_dtor', lambda: (SDTest.Call('AStarTable_dtor', 'tvi', tbl), tpeek(tbl, 3)))
SDTest.Call('Mem_Free', 'vi', tbl)
# the tests of single modules (tools/test/py/sdcall_*.py); a missing one is skipped
for part in ('sdcall_vertlist.py', 'sdcall_trimesh.py', 'sdcall_material.py', 'sdcall_light.py', 'sdcall_node3d.py',
		'sdcall_animation.py', 'sdcall_animgroup.py', 'sdcall_texture.py', 'sdcall_meshd3d.py', 'sdcall_ground.py',
		'sdcall_trails.py', 'sdcall_model.py', 'sdcall_scene.py', 'sdcall_particlesystem.py', 'sdcall_rendertarget.py',
		'sdcall_controlanim.py', 'sdcall_portal.py', 'sdcall_graphic3d.py', 'sdcall_utild3d.py', 'sdcall_text.py', 'sdcall_graphic2d.py', 'sdcall_render.py',
		'sdcall_multisprite.py', 'sdcall_flare.py', 'sdcall_fonts.py',
		'sdcall_scrapfont.py', 'sdcall_readm3d.py', 'sdcall_m3d.py', 'sdcall_scrapmodel.py', 'sdcall_scrapcam.py', 'sdcall_scrapworld.py', 'sdcall_scrapmain.py', 'sdcall_app.py', 'sdcall_sound.py', 'sdcall_music.py', 'sdcall_bsp.py', 'sdcall_input.py', 'sdcall_render2.py', 'sdcall_eng2.py', 'sdcall_eng3.py', 'sdcall_eng4.py', 'sdcall_restb.py', 'sdcall_resta.py', 'sdcall_engmisc.py', 'sdcall_sa1b.py', 'sdcall_nosleep.py', 'sdcall_nosleep2.py', 'sdcall_nowait.py', 'sdcall_d3dx.py', 'sdcall_d3dxasm.py', 'sdcall_affinedecomp.py', 'sdcall_crtconv.py', 'sdcall_d3dxtex.py', 'sdcall_rsrc.py'):
	try:
		f = open(part)
	except IOError:
		continue
	f.close()
	execfile(part)
finish()
