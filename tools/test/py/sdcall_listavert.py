# engine3d Engine/ListaVert.c: vertex lists in heap memory, the strips and the vertex cache (Python 1.5.2, runs inside
# sdcall.py after the other checks: check, fmt, tpeek, rnd, affine come from there). Floats in memory are compared as
# their bits (Peek '@i').
LV_SIZE = {1: 0x20, 2: 0x28, 3: 0x1c, 4: 0x24, 5: 0x2c, 6: 0x34, 7: 0x24, 8: 0x20, 10: 0x20, 11: 4, 12: 0x18,
	13: 0x1c, 14: 0x12, 15: 0x16, 16: 0x16, 17: 0x1a}
# the fields of the source formats: p pos, n normal, t tex, c color, w weight
LV_LAYOUT = {1: 'pnt', 2: 'pntt', 3: 'pcct', 4: 'pwnt', 5: 'pnctt', 7: 'pnct', 10: 'pctt'}

def lvpoke(addr, words):
	for i in range(len(words)):
		SDTest.Poke(addr + 4 * i - 0x400000, words[i])
def lvsum(addr, n):
	s = 0L
	for i in range(n):
		s = (s * 31 + SDTest.Peek(addr + 4 * i, '@i')) % 1000000007
	return s
def lvnew(frmt, n):
	lv = SDTest.Call(0x133650, 'ii', 0x14)[0]
	SDTest.Call(0x23fbd0, 'tii', lv)
	SDTest.Call(0x242400, 'tvii', lv, frmt)
	if n:
		SDTest.Call(0x242470, 'tviii', lv, n, 0)
	return lv
def lvcolor():
	return int(rnd(0, 32767)) * 65536 + int(rnd(0, 65535))
# random vertices of a source format
def lvfill(lv, frmt, n):
	p = SDTest.Peek(lv, '@i')
	size = LV_SIZE[frmt]
	for i in range(n):
		f = []
		colors = []
		for k in LV_LAYOUT[frmt]:
			if k == 'p':
				f = f + [rnd(-50.0, 50.0), rnd(-50.0, 50.0), rnd(-50.0, 50.0)]
			elif k == 'n':
				f = f + [rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0)]
			elif k == 't':
				f = f + [rnd(-2.0, 2.0), rnd(-2.0, 2.0)]
			elif k == 'w':
				f = f + [rnd(0.0, 1.0)]
			else:
				colors.append(len(f))
				f = f + [0.0]
		SDTest.Call(0x3197d0, 'iiFi', p + i * size, tuple(f), size)
		for c in colors:
			SDTest.Poke(p + i * size + 4 * c - 0x400000, lvcolor())
def lvget(lv):
	p, n, tot, frmt, esc = tpeek(lv, 5)
	r = [n, tot, frmt, esc, p != 0]
	if p and LV_SIZE.has_key(frmt):
		r.append(tpeek(p, n * LV_SIZE[frmt] / 4))
	return r
def lvfree(lv):
	SDTest.Call(0x242540, 'tvi', lv)
	SDTest.Call(0x133550, 'vi', lv)
def lvstep(name, lv, rva, sig, *args):
	check(name + fmt(args), lambda lv=lv, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, lv) + args)[0], lvget(lv)))

for f in range(1, 18):
	if f != 9:
		call('ListaVert_FormatSize', 0x2415a0, 'ii', f)
	if f < 11 and f != 9:
		call('ListaVert_FormatName', 0x241670, 'si', f)
		call('ListaVert_FormatFVF', 0x2413c0, 'ii', f)
	# the XBOX formats name a vertex shader that the PC version may not have: only checked when it is there
	if f >= 12 and SDTest.Peek(0x4ad630 + 4 * (f - 12), 'i'):
		check('ListaVert_FormatFVF shader' + fmt((f,)), lambda f=f: SDTest.Call(0x2413c0, 'ii', f)[0] == SDTest.Peek(0x4ad630 + 4 * (f - 12), 'i'))
for i in range(30):
	n = (rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0))
	call('ListaVert_PackNormal', 0x241710, 'iF', n)
	packed = SDTest.Call(0x241710, 'iF', n)[0]
	call('ListaVert_UnpackNormal', 0x2417f0, 'vFi', (9.0, 9.0, 9.0), packed)
for n in ((1.0, -1.0, 1.0), (-1.0, 1.0, -1.0), (0.0, -0.0, 0.0), (0.0009, -0.0009, 0.001)):
	call('ListaVert_PackNormal', 0x241710, 'iF', n)
for packed in (0, -1, 0x7fffffff, -0x7fffffff - 1, 0x3ff, 0x400, 0x1ff800, 0x200000, 0x12345678):
	call('ListaVert_UnpackNormal', 0x2417f0, 'vFi', (9.0, 9.0, 9.0), packed)
# the field getters of one vertex, on a fake address (they only add)
for rva in (0x242770, 0x2427c0, 0x242810, 0x242870, 0x2428a0, 0x2428f0, 0x242950, 0x2429c0, 0x242a30, 0x242aa0,
		0x242ad0, 0x242b00, 0x242b60, 0x242b90):
	for f in range(-1, 18):
		check('Vert_Field' + fmt((rva, f)), lambda rva=rva, f=f: SDTest.Call(rva, 'tiii', 0x1000, f)[0] - 0x1000)

# the lists: fields, accessors, setters
lvs = {}
for frmt in (1, 2, 3, 4, 5, 7, 10):
	lv = lvnew(frmt, 4)
	lvfill(lv, frmt, 4)
	lvs[frmt] = lv
	check('CListaVert_Alloc' + fmt((frmt,)), lambda lv=lv: lvget(lv)[0:5])
	check('CListaVert_FVF' + fmt((frmt,)), lambda lv=lv: SDTest.Call(0x242be0, 'tii', lv)[0])
	check('CListaVert_Size' + fmt((frmt,)), lambda lv=lv: SDTest.Call(0x243710, 'tii', lv)[0])
	check('CListaVert_FormatName' + fmt((frmt,)), lambda lv=lv: SDTest.Call(0x242460, 'tsi', lv)[0])
	p = SDTest.Peek(lv, '@i')
	for f in range(0, 13):
		for i in (0, 3):
			# TEX1_B1 has no fields (the getter fails)
			if frmt != 4:
				check('CListaVert_GetField' + fmt((frmt, f, i)), lambda lv=lv, f=f, i=i, p=p: SDTest.Call(0x242570, 'tiiii', lv, f, i)[0] - p)
for frmt, rva in ((1, 0x242f70), (2, 0x243030), (3, 0x242d20), (4, 0x242fd0), (5, 0x242e50), (7, 0x242d90), (10, 0x242df0)):
	lv = lvs[frmt]
	p = SDTest.Peek(lv, '@i')
	for i in (0, 1, 3):
		check('CListaVert_Vert' + fmt((frmt, i)), lambda lv=lv, rva=rva, i=i, p=p: SDTest.Call(rva, 'tiii', lv, i)[0] - p)
for frmt in (1, 2, 4):
	for i in (0, 2):
		check('CListaVert_GetTex1' + fmt((frmt, i)), lambda frmt=frmt, i=i: SDTest.Call(0x242bf0, 'tviFi', lvs[frmt], (0.0,) * 8, i)[1])
for frmt, rva in ((1, 0x242290), (2, 0x242380), (3, 0x242210), (4, 0x242300)):
	v = []
	for k in range(LV_SIZE[frmt] / 4):
		v.append(rnd())
	lvstep('CListaVert_Set' + fmt((frmt,)), lvs[frmt], rva, 'tviiF', 2, tuple(v))
	lvfill(lvs[frmt], frmt, 4)
lvstep('CListaVert_SetColor', lvs[3], 0x241850, 'tviii', 0x00ff00ff, 0x12003400)
lvstep('CListaVert_SetColor', lvs[7], 0x241850, 'tviii', -1, 0)
lvstep('CListaVert_SetColor', lvs[10], 0x241850, 'tviii', 0, -0x7f000000)
for frmt in (1, 3, 10):
	box = (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)
	check('CListaVert_GetExtents' + fmt((frmt,)), lambda frmt=frmt, box=box: SDTest.Call(0x241e10, 'tviFi', lvs[frmt], box, 1)[1])
	check('CListaVert_GetExtents add' + fmt((frmt,)), lambda frmt=frmt, box=box: SDTest.Call(0x241e10, 'tviFi', lvs[frmt], box, 0)[1])
a, b = lvs[1], lvs[3]
check('CListaVert_Swap', lambda: (SDTest.Call(0x2424f0, 'tvii', a, b), lvget(a), lvget(b)))
SDTest.Call(0x2424f0, 'tvii', a, b)
for frmt in (1, 2, 4):
	for norm in (0, 1):
		lvstep('CListaVert_Transform' + fmt((frmt,)), lvs[frmt], 0x243720, 'tviFi', affine(), norm)
	lvstep('CListaVert_Transform ident' + fmt((frmt,)), lvs[frmt], 0x243720, 'tviFi', ident, 1)
	lvfill(lvs[frmt], frmt, 4)
# a zero normal stays zero
SDTest.Call(0x3197d0, 'iiFi', SDTest.Peek(lvs[1], '@i') + 12, (0.0, 0.0, 0.0), 12)
lvstep('CListaVert_Transform zero', lvs[1], 0x243720, 'tviFi', ident, 1)
lvfill(lvs[1], 1, 4)

# the XBOX formats: packed and back
for frmt in (1, 7):
	for ps, ts in ((0.01, 4096.0), (0.001, 32768.0), (1.0, 1.0)):
		lv = lvnew(frmt, 4)
		lvfill(lv, frmt, 4)
		lvstep('CListaVert_PackTex1' + fmt((frmt,)), lv, 0x241210, 'tviff', ps, ts)
		lvfree(lv)
	lv = lvnew(frmt, 4)
	lvfill(lv, frmt, 4)
	lvstep('CListaVert_Pack 12' + fmt((frmt,)), lv, 0x240500, 'tviiff', 12, 0.01, 1024.0)
	lvfree(lv)
	# VERT_POS_XBOX: the positions divided by esc must stay inside -1..1
	lv = lvnew(frmt, 4)
	lvfill(lv, frmt, 4)
	SDTest.Call(0x3197d0, 'iiFi', lv + 0x10, (64.0,), 4)
	lvstep('CListaVert_Pack 11' + fmt((frmt,)), lv, 0x240500, 'tviiff', 11, 0.5, 2.0)
	lvfree(lv)
for frmt in (3, 5, 7, 10):
	for tex2 in (0, 1):
		for ps, ts in ((0.01, 4096.0), (0.0005, 30000.0)):
			lv = lvnew(frmt, 4)
			lvfill(lv, frmt, 4)
			lvstep('CListaVert_ToXBox' + fmt((frmt, tex2)), lv, 0x240a50, 'tviffi', ps, ts, tex2)
			if frmt == 7:
				lvstep('CListaVert_FromXBox' + fmt((tex2,)), lv, 0x241260, 'tviff', ps, ts)
			lvfree(lv)
for frmt in lvs.keys():
	lvfree(lvs[frmt])

# the LFVF blocks of two models of the packs, at the offset of the block
lvio = SDTest.Call(0x1335a0, 'ii', 16)[0]
for name, off in (('levels/fz/map/map3d_2.sm3', 2270), ('levels/menu/map/asteroids/tasteroid5.sm3', 2348)):
	SDTest.Call(0x21b1f0, 'tiiss', lvio, name, 'rb')
	while off > 0:
		k = min(off, 1000)
		SDTest.Call(0x21c060, 'tiibi', lvio, '', k)
		off = off - k
	lv = lvnew(0, 0)
	def lvread(lv=lv):
		SDTest.Call(0x2400b0, 'tvii', lv, lvio)
		r = lvget(lv)
		return r[0:5], lvsum(SDTest.Peek(lv, '@i'), r[0] * LV_SIZE[r[2]] / 4), SDTest.Peek(lvio, '@b')
	check('CListaVert_Read' + fmt((name,)), lvread)
	lvfree(lv)
	SDTest.Call(0x21b490, 'tii', lvio)
	SDTest.Call(0x21b290, 'tvi', lvio)
SDTest.Call(0x133550, 'vi', lvio)

# triangles: 3 ushorts each, poked as words
def trinew(tris):
	n = len(tris) / 3
	if len(tris) % 2:
		tris = tris + (0,)
	t = SDTest.Call(0x133650, 'ii', len(tris) * 2)[0]
	w = []
	for i in range(0, len(tris), 2):
		w.append(tris[i] + tris[i + 1] * 65536)
	lvpoke(t, w)
	return t
def triget(t, n):
	r = []
	for i in range(n * 3):
		r.append(SDTest.Peek(t + 2 * i, '@b') + SDTest.Peek(t + 2 * i + 1, '@b') * 256)
	return r
def tristep(name, t, n, rva, sig, *args):
	check(name + fmt(args), lambda t=t, n=n, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, t) + args)[0], triget(t, n)))
grid = (0, 3, 1,  1, 3, 4,  1, 4, 2,  2, 4, 5,  3, 6, 4,  4, 6, 7,  4, 7, 5,  5, 7, 8)
apart = (0, 1, 2,  3, 4, 5,  6, 7, 8,  2, 1, 6)
fan = (0, 1, 2,  0, 2, 3,  0, 3, 4,  0, 4, 5,  0, 5, 6,  6, 7, 8)
for tris in (grid, apart, fan):
	n = len(tris) / 3
	t = trinew(tris)
	tristep('Strip_FindFirst', t, n, 0x2430a0, 'iiii', n, 0)
	tristep('Strip_FindFirst odd', t, n, 0x2430a0, 'iiii', n, 1)
	tristep('Strip_FindNew', t, n, 0x243320, 'iiii', n, 0)
	for k in range(n):
		a, b = tris[3 * k], tris[3 * k + 1]
		for d in (0, 1):
			check('Strip_Next' + fmt((k, d)), lambda t=t, n=n, a=a, b=b, d=d: SDTest.Call(0x243560, 'iiiiiip', t, n, a, b, d, 7))
		check('Strip_Length' + fmt((k,)), lambda t=t, n=n, k=k, a=a, b=b: (SDTest.Call(0x243220, 'iiiiiii', k, t, n, 0, a, b)[0], triget(t, n)))
	in_ = lvnew(3, 9)
	lvfill(in_, 3, 9)
	out = lvnew(0, 0)
	check('ListaVert_Strip' + fmt((tris,)), lambda out=out, in_=in_, t=t, n=n: (SDTest.Call(0x23fc60, 'biiii', out, in_, t, n)[0], lvget(out), triget(t, n)))
	lvfree(out)
	lvfree(in_)
	SDTest.Call(0x133550, 'vi', t)
	t = trinew(tris)
	def vcache():
		r = []
		for i in range(32):
			r.append(SDTest.Peek(0x4ad648 + 4 * i, 'i'))
		return r, SDTest.Peek(0x4ad6c8, 'i'), SDTest.Peek(0x4ad6cc, 'i'), SDTest.Peek(0x4ad6d0, 'i')
	check('VertCache_Optimize' + fmt((tris,)), lambda t=t, n=n: (SDTest.Call(0x240390, 'bii', t, n)[0], triget(t, n), vcache()))
	SDTest.Call(0x133550, 'vi', t)
t = trinew((1, 2, 3,  7, 8, 9))
for k in (-4, -1, 0, 1, 2, 3, 5):
	tristep('Tri_Rotate', t, 2, 0x243670, 'vii', k)
tristep('Tri_Mark', t, 2, 0x243620, 'vi')
tristep('Strip_Next marked', t, 2, 0x243560, 'iiiiiip', 2, 2, 3, 0, 7)
tristep('Tri_Unmark', t, 2, 0x2436c0, 'vi')
SDTest.Call(0x133550, 'vi', t)
call('VertCache_Reset', 0x243950, 'v')
check('VertCache after reset', vcache)
for v in (5, 6, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 5, 6, 22):
	call('VertCache_Add', 0x2438c0, 'vi', v)
	call('VertCache_Oldest', 0x243990, 'i')
check('VertCache after adds', vcache)
for v in (5, 6, 7, 20, 22, 99, -1):
	call('VertCache_Find', 0x2439d0, 'ii', v)
call('VertCache_Reset', 0x243950, 'v')
