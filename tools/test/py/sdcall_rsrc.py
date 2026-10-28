# The resources of the exe's module (execfile'd at the end of sdcall.py, Python 1.5.2): in ScrapC.exe its own of
# src/standalone/scrap.rc (docs/standalone.md, stage 3b3), in the original its .rsrc. Each one found by type and id as
# the game finds them, with its size and a sum of its bytes; the menus, accelerators and icons loaded; each dialog
# created from its template (no dialog procedure) with its caption and the texts of its controls, then destroyed.
# The Win32 functions come from SDTest.Proc (stdcall: as thiscall with a dummy this). Since stage 3c1a ScrapC.exe has
# its own import table, the one of the original is no longer built in.
rsfns = {}
def rsw(fn, sig, args):
	if not rsfns.has_key(fn):
		dll = 'user32.dll'
		if fn in ('GetModuleHandleA', 'FindResourceA', 'SizeofResource', 'LoadResource', 'LockResource'):
			dll = 'kernel32.dll'
		rsfns[fn] = SDTest.Proc(dll, fn)
	return apply(SDTest.Call, (rsfns[fn], 't' + sig[0] + 'i' + sig[1:], 0) + args)

# by halves of words: the % of a negative int is not the same in the two Pythons for -2147483648 (the -x of
# i_divmod in intobject.c overflows, MSVC and GCC do different things with it)
def rssum(p, n):
	s = 0
	i = 0
	while i + 4 <= n:
		w = SDTest.Peek(p + i, '@i')
		s = ((s * 7 + (w & 0xffff)) * 7 + (w >> 16 & 0xffff)) % 1000003
		i = i + 4
	while i < n:
		s = (s * 7 + SDTest.Peek(p + i, '@b')) % 1000003
		i = i + 1
	return s

def rstext(buf):
	k = strop.find(buf, '\0')
	if k >= 0:
		return buf[:k]
	return buf

rsmod = rsw('GetModuleHandleA', 'iz', (None,))[0]
check('rs GetModuleHandleA', lambda: rsmod == 0x400000)

# (type, id): RT_BITMAP, RT_ICON, RT_MENU, RT_DIALOG, RT_ACCELERATOR, RT_GROUP_ICON, RT_MANIFEST
rsres = ((2, 148), (3, 1), (3, 2), (3, 3), (3, 4), (3, 5), (3, 6), (4, 141), (4, 142), (4, 210), (5, 143), (5, 144),
	(5, 153), (5, 154), (5, 162), (5, 163), (5, 211), (9, 113), (9, 200), (9, 214), (14, 101), (14, 106), (24, 1))
def rsfind(k):
	h = rsw('FindResourceA', 'iiii', (rsmod, k[1], k[0]))[0]
	if not h:
		return 0
	n = rsw('SizeofResource', 'iii', (rsmod, h))[0]
	p = rsw('LockResource', 'ii', (rsw('LoadResource', 'iii', (rsmod, h))[0],))[0]
	if k == (5, 153):
		return 1, n	# windres writes the class name SysTreeView32 of a control in capitals
	return 1, n, rssum(p, n)
for rsk in rsres:
	check('rs FindResourceA' + fmt(rsk), lambda k=rsk: rsfind(k))

def rsmenu(mid):
	m = rsw('LoadMenuA', 'iii', (rsmod, mid))[0]
	if m:
		rsw('DestroyMenu', 'ii', (m,))
	return m != 0
for rsk in (141, 142, 210):
	check('rs LoadMenuA' + fmt((rsk,)), lambda k=rsk: rsmenu(k))
for rsk in ((rsmod, 113), (rsmod, 200), (rsmod, 214), (0, 200)):
	check('rs LoadAcceleratorsA' + fmt((rsk[0] != 0, rsk[1])), lambda k=rsk: rsw('LoadAcceleratorsA', 'iii', k)[0] != 0)
for rsk in (101, 106):
	check('rs LoadIconA' + fmt((rsk,)), lambda k=rsk: rsw('LoadIconA', 'iii', (rsmod, k))[0] != 0)

# the dialogs with the ids of their controls (not the IDC_STATIC ones)
rsdlgs = ((143, (1,)), (144, (1002, 1000, 1012, 1014, 1001, 1, 2, 1013, 1003, 1016)), (153, (1, 2, 1015, 1020)),
	(154, (1022, 1030, 1031)), (162, (1034, 1033, 1037, 1038, 1042, 1039, 1043, 1040, 1055, 1054, 1044, 1041, 1, 2,
	1035, 1036, 1047, 1048, 1056, 1050, 1051, 1049)), (163, (1030, 1031, 1022)), (211, (1000, 1)))
def rsdialog(did, ids):
	d = rsw('CreateDialogParamA', 'iiiiii', (rsmod, did, 0, 0, 0))[0]
	if not d:
		return 0
	r = [rstext(rsw('GetWindowTextA', 'iibi', (d, '', 1024))[1])]
	for i in ids:
		c = rsw('GetDlgItem', 'iii', (d, i))[0]
		r.append((i, c != 0, rstext(rsw('GetDlgItemTextA', 'iiibi', (d, i, '', 1024))[1])))
	rsw('DestroyWindow', 'ii', (d,))
	return tuple(r)
for rsk in rsdlgs:
	check('rs CreateDialogParamA' + fmt((rsk[0],)), lambda k=rsk: rsdialog(k[0], k[1]))
