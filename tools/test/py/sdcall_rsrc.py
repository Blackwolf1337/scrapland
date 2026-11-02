# The resources of the exe's module (execfile'd at the end of sdcall.py, Python 1.5.2): in ScrapC.exe its own of
# src/standalone/scrap.rc (docs/standalone.md, stage 3b3), in the original its .rsrc. The ones ScrapC.exe keeps (the
# icons and the manifest; the menus, dialogs, accelerators and the bitmap went with the Win32 window code) found by
# type and id, with their size and a sum of their bytes, and the icons loaded.
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

rsmod = rsw('GetModuleHandleA', 'iz', (None,))[0]
check('rs GetModuleHandleA', lambda: rsmod == 0x400000)

# (type, id): RT_ICON, RT_GROUP_ICON, RT_MANIFEST
rsres = ((3, 1), (3, 2), (3, 3), (3, 4), (3, 5), (3, 6), (14, 101), (14, 106), (24, 1))
def rsfind(k):
	h = rsw('FindResourceA', 'iiii', (rsmod, k[1], k[0]))[0]
	if not h:
		return 0
	n = rsw('SizeofResource', 'iii', (rsmod, h))[0]
	p = rsw('LockResource', 'ii', (rsw('LoadResource', 'iii', (rsmod, h))[0],))[0]
	return 1, n, rssum(p, n)
for rsk in rsres:
	check('rs FindResourceA' + fmt(rsk), lambda k=rsk: rsfind(k))
for rsk in (101, 106):
	check('rs LoadIconA' + fmt((rsk,)), lambda k=rsk: rsw('LoadIconA', 'iii', (rsmod, k))[0] != 0)
