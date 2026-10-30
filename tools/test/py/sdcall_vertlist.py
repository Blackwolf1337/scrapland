# engine3d Engine/VertList.c: vertex lists in heap memory, the strips and the vertex cache (Python 1.5.2, runs inside
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
	lv = SDTest.Call('Mem_Calloc', 'ii', 0x14)[0]
	SDTest.Call('CVertList_ctor', 'tii', lv)
	SDTest.Call('CVertList_SetFormat', 'tvii', lv, frmt)
	if n:
		SDTest.Call('CVertList_Alloc', 'tviii', lv, n, 0)
	return lv
def lvcolor():
	return int(rnd(0.0, 32767.0)) * 65536 + int(rnd(0.0, 65535.0))
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
		SDTest.Call('crt_memcpy', 'iiFi', p + i * size, tuple(f), size)
		for c in colors:
			SDTest.Poke(p + i * size + 4 * c - 0x400000, lvcolor())
def lvget(lv):
	p, n, tot, frmt, esc = tpeek(lv, 5)
	r = [n, tot, frmt, esc, p != 0]
	if p and LV_SIZE.has_key(frmt):
		r.append(tpeek(p, n * LV_SIZE[frmt] / 4))
	return r
def lvfree(lv):
	SDTest.Call('CVertList_Free@242540', 'tvi', lv)
	SDTest.Call('Mem_Free', 'vi', lv)
# a pointer into the vertices as its offset (a NULL is not printed as a heap address)
def lvoff(r, p):
	if r == 0:
		return None
	return r - p
def lvstep(name, lv, rva, sig, *args):
	check(name + fmt(args), lambda lv=lv, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, lv) + args)[0], lvget(lv)))

# the vertex shaders of the XBOX formats 12..17, separate C objects in ScrapC.exe
lvvshaders = (sym('g_hVShader_Tex1_XBox'), sym('g_hVShader_Tex1C_XBox'), sym('g_hVShader_LTex1_XBox'),
	sym('g_hVShader_LTex1N_XBox'), sym('g_hVShader_LTex2_XBox'), sym('g_hVShader_LTex2N_XBox'))
for f in range(1, 18):
	if f != 9:
		call('VertList_FormatSize', sym('VertList_FormatSize'), 'ii', f)
	if f < 11 and f != 9:
		call('VertList_FormatName', sym('VertList_FormatName'), 'si', f)
		call('VertList_FormatFVF', sym('VertList_FormatFVF'), 'ii', f)
	# the XBOX formats name a vertex shader that the PC version may not have: only checked when it is there
	if f >= 12 and SDTest.Peek(lvvshaders[f - 12], 'i'):
		check('VertList_FormatFVF shader' + fmt((f,)), lambda f=f: SDTest.Call('VertList_FormatFVF', 'ii', f)[0] == SDTest.Peek(lvvshaders[f - 12], 'i'))
for i in range(30):
	n = (rnd(-1.0, 1.0), rnd(-1.0, 1.0), rnd(-1.0, 1.0))
	call('VertList_PackNormal', sym('VertList_PackNormal'), 'iF', n)
	packed = SDTest.Call('VertList_PackNormal', 'iF', n)[0]
	call('VertList_UnpackNormal', sym('VertList_UnpackNormal'), 'vFi', (9.0, 9.0, 9.0), packed)
for n in ((1.0, -1.0, 1.0), (-1.0, 1.0, -1.0), (0.0, -0.0, 0.0), (0.0009, -0.0009, 0.001)):
	call('VertList_PackNormal', sym('VertList_PackNormal'), 'iF', n)
for packed in (0, -1, 0x7fffffff, -0x7fffffff - 1, 0x3ff, 0x400, 0x1ff800, 0x200000, 0x12345678):
	call('VertList_UnpackNormal', sym('VertList_UnpackNormal'), 'vFi', (9.0, 9.0, 9.0), packed)
# the field getters of one vertex, on a fake address (they only add)
for rva in ('VertLTex1_Field', 'VertLTex1N_Field', 'VertLTex1NXBox_Field', 'VertLTex1XBox_Field', 'VertLTex2_Field', 'VertLTex2N_Field', 'VertLTex2NXBox_Field', 'VertLTex2XBox_Field', 'VertLTex3_Field', 'VertTLTex1_Field',
		'VertTex1_Field', 'VertTex1CXBox_Field', 'VertTex1XBox_Field', 'VertTex2_Field'):
	for f in range(-1, 18):
		check('Vert_Field' + fmt((rva, f)), lambda rva=rva, f=f: SDTest.Call(rva, 'tiii', 0x1000, f)[0] - 0x1000)

# the lists: fields, accessors, setters
lvs = {}
for frmt in (1, 2, 3, 4, 5, 7, 10):
	lv = lvnew(frmt, 4)
	lvfill(lv, frmt, 4)
	lvs[frmt] = lv
	check('CVertList_Alloc' + fmt((frmt,)), lambda lv=lv: lvget(lv)[0:5])
	check('CVertList_FVF' + fmt((frmt,)), lambda lv=lv: SDTest.Call('CVertList_FVF', 'tii', lv)[0])
	check('CVertList_Size' + fmt((frmt,)), lambda lv=lv: SDTest.Call('CVertList_Size', 'tii', lv)[0])
	check('CVertList_FormatName' + fmt((frmt,)), lambda lv=lv: SDTest.Call('CVertList_FormatName', 'tsi', lv)[0])
	p = SDTest.Peek(lv, '@i')
	for f in range(0, 13):
		for i in (0, 3):
			# TEX1_B1 has no fields (the getter fails)
			if frmt != 4:
				check('CVertList_GetField' + fmt((frmt, f, i)), lambda lv=lv, f=f, i=i, p=p: lvoff(SDTest.Call('CVertList_GetField', 'tiiii', lv, f, i)[0], p))
for frmt, rva in ((1, sym('CVertList_Tex1')), (2, sym('CVertList_Tex2')), (3, sym('CVertList_LTex1')), (4, sym('CVertList_Tex1B1')), (5, sym('CVertList_LTex2N')), (7, sym('CVertList_LTex1N')), (10, sym('CVertList_LTex2'))):
	lv = lvs[frmt]
	p = SDTest.Peek(lv, '@i')
	for i in (0, 1, 3):
		check('CListaVert_Vert' + fmt((frmt, i)), lambda lv=lv, rva=rva, i=i, p=p: lvoff(SDTest.Call(rva, 'tiii', lv, i)[0], p))
for frmt in (1, 2, 4):
	for i in (0, 2):
		check('CVertList_GetTex1' + fmt((frmt, i)), lambda frmt=frmt, i=i: SDTest.Call('CVertList_GetTex1', 'tviFi', lvs[frmt], (0.0,) * 8, i)[1])
for frmt, rva in ((1, sym('CVertList_SetTex1')), (2, sym('CVertList_SetTex2')), (3, sym('CVertList_SetLTex1')), (4, sym('CVertList_SetTex1B1'))):
	v = []
	for k in range(LV_SIZE[frmt] / 4):
		v.append(rnd())
	lvstep('CListaVert_Set' + fmt((frmt,)), lvs[frmt], rva, 'tviiF', 2, tuple(v))
	lvfill(lvs[frmt], frmt, 4)
lvstep('CVertList_SetColor', lvs[3], sym('CVertList_SetColor'), 'tviii', 0x00ff00ff, 0x12003400)
lvstep('CVertList_SetColor', lvs[7], sym('CVertList_SetColor'), 'tviii', -1, 0)
lvstep('CVertList_SetColor', lvs[10], sym('CVertList_SetColor'), 'tviii', 0, -0x7f000000)
for frmt in (1, 3, 10):
	box = (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)
	check('CVertList_GetExtents' + fmt((frmt,)), lambda frmt=frmt, box=box: SDTest.Call('CVertList_GetExtents', 'tviFi', lvs[frmt], box, 1)[1])
	check('CVertList_GetExtents add' + fmt((frmt,)), lambda frmt=frmt, box=box: SDTest.Call('CVertList_GetExtents', 'tviFi', lvs[frmt], box, 0)[1])
a, b = lvs[1], lvs[3]
check('CVertList_Swap', lambda: (SDTest.Call('CVertList_Swap', 'tvii', a, b), lvget(a), lvget(b)))
SDTest.Call('CVertList_Swap', 'tvii', a, b)
for frmt in (1, 2, 4):
	for norm in (0, 1):
		lvstep('CVertList_Transform' + fmt((frmt,)), lvs[frmt], sym('CVertList_Transform'), 'tviFi', affine(), norm)
	lvstep('CVertList_Transform ident' + fmt((frmt,)), lvs[frmt], sym('CVertList_Transform'), 'tviFi', ident, 1)
	lvfill(lvs[frmt], frmt, 4)
# a zero normal stays zero
SDTest.Call('crt_memcpy', 'iiFi', SDTest.Peek(lvs[1], '@i') + 12, (0.0, 0.0, 0.0), 12)
lvstep('CVertList_Transform zero', lvs[1], sym('CVertList_Transform'), 'tviFi', ident, 1)
lvfill(lvs[1], 1, 4)

# the XBOX formats: packed and back
for frmt in (1, 7):
	for ps, ts in ((0.01, 4096.0), (0.001, 32768.0), (1.0, 1.0)):
		lv = lvnew(frmt, 4)
		lvfill(lv, frmt, 4)
		lvstep('CVertList_PackTex1' + fmt((frmt,)), lv, sym('CVertList_PackTex1'), 'tviff', ps, ts)
		lvfree(lv)
	lv = lvnew(frmt, 4)
	lvfill(lv, frmt, 4)
	lvstep('CVertList_Pack 12' + fmt((frmt,)), lv, sym('CVertList_Pack'), 'tviiff', 12, 0.01, 1024.0)
	lvfree(lv)
	# VERT_POS_XBOX: the positions divided by esc must stay inside -1..1
	lv = lvnew(frmt, 4)
	lvfill(lv, frmt, 4)
	SDTest.Call('crt_memcpy', 'iiFi', lv + 0x10, (64.0,), 4)
	lvstep('CVertList_Pack 11' + fmt((frmt,)), lv, sym('CVertList_Pack'), 'tviiff', 11, 0.5, 2.0)
	lvfree(lv)
for frmt in (3, 5, 7, 10):
	for tex2 in (0, 1):
		for ps, ts in ((0.01, 4096.0), (0.0005, 30000.0)):
			lv = lvnew(frmt, 4)
			lvfill(lv, frmt, 4)
			lvstep('CVertList_ToXBox' + fmt((frmt, tex2)), lv, sym('CVertList_ToXBox'), 'tviffi', ps, ts, tex2)
			if frmt == 7:
				lvstep('CVertList_FromXBox' + fmt((tex2,)), lv, sym('CVertList_FromXBox'), 'tviff', ps, ts)
			lvfree(lv)
for frmt in lvs.keys():
	lvfree(lvs[frmt])

# the LFVF blocks of two models of the packs, at the offset of the block
lvio = SDTest.Call('Mem_Alloc', 'ii', 16)[0]
for name, off in (('levels/fz/map/map3d_2.sm3', 2270), ('levels/menu/map/asteroids/tasteroid5.sm3', 2348)):
	SDTest.Call('CFileIO_ctor', 'tiiss', lvio, name, 'rb')
	while off > 0:
		k = min(off, 1000)
		SDTest.Call('CFileIO_Read', 'tiibi', lvio, '', k)
		off = off - k
	lv = lvnew(0, 0)
	def lvread(lv=lv):
		SDTest.Call('CVertList_Read', 'tvii', lv, lvio)
		r = lvget(lv)
		return r[0:5], lvsum(SDTest.Peek(lv, '@i'), r[0] * LV_SIZE[r[2]] / 4), SDTest.Peek(lvio, '@b')
	check('CVertList_Read' + fmt((name,)), lvread)
	lvfree(lv)
	SDTest.Call('CFileIO_Close', 'tii', lvio)
	SDTest.Call('CFileIO_dtor', 'tvi', lvio)
SDTest.Call('Mem_Free', 'vi', lvio)

# triangles: 3 ushorts each, poked as words
def trinew(tris):
	n = len(tris) / 3
	if len(tris) % 2:
		tris = tris + (0,)
	t = SDTest.Call('Mem_Calloc', 'ii', len(tris) * 2)[0]
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
	tristep('Strip_FindFirst', t, n, sym('Strip_FindFirst'), 'iiii', n, 0)
	tristep('Strip_FindFirst odd', t, n, sym('Strip_FindFirst'), 'iiii', n, 1)
	tristep('Strip_FindNew', t, n, sym('Strip_FindNew'), 'iiii', n, 0)
	for k in range(n):
		a, b = tris[3 * k], tris[3 * k + 1]
		for d in (0, 1):
			check('Strip_Next' + fmt((k, d)), lambda t=t, n=n, a=a, b=b, d=d: SDTest.Call('Strip_Next', 'iiiiiip', t, n, a, b, d, 7))
		check('Strip_Length' + fmt((k,)), lambda t=t, n=n, k=k, a=a, b=b: (SDTest.Call('Strip_Length', 'iiiiiii', k, t, n, 0, a, b)[0], triget(t, n)))
	in_ = lvnew(3, 9)
	lvfill(in_, 3, 9)
	lvout = lvnew(0, 0)
	check('VertList_Strip' + fmt((tris,)), lambda lvout=lvout, in_=in_, t=t, n=n: (SDTest.Call('VertList_Strip', 'biiii', lvout, in_, t, n)[0], lvget(lvout), triget(t, n)))
	lvfree(lvout)
	lvfree(in_)
	SDTest.Call('Mem_Free', 'vi', t)
	t = trinew(tris)
	def vcache():
		r = []
		for i in range(16):
			r.append(SDTest.Peek(sym('g_VCacheVert') + 4 * i, 'i'))
		for i in range(16):
			r.append(SDTest.Peek(sym('g_VCacheAge') + 4 * i, 'i'))
		return r, SDTest.Peek('g_VCacheCount', 'i'), SDTest.Peek('g_VCacheHits', 'i'), SDTest.Peek('g_VCacheMisses', 'i')
	check('VertCache_Optimize' + fmt((tris,)), lambda t=t, n=n: (SDTest.Call('VertCache_Optimize', 'bii', t, n)[0], triget(t, n), vcache()))
	SDTest.Call('Mem_Free', 'vi', t)
# the cache compare of triangles with vertices in the cache, out of it and both, the cache filled by hand
SDTest.Call('VertCache_Reset', 'v')
for v in (2, 9, 30000, 3):
	SDTest.Call('VertCache_Add', 'vi', v)
t = trinew((1, 2, 3,  7, 8, 9,  3, 2, 9,  30000, 5, 32767,  2, 9, 3))
for i in range(5):
	for j in range(5):
		check('VertCache_Compare' + fmt((i, j)), lambda t=t, i=i, j=j: SDTest.Call('VertCache_Compare', 'iii', t + 6 * i, t + 6 * j)[0])
SDTest.Call('Mem_Free', 'vi', t)
t = trinew((1, 2, 3,  7, 8, 9))
for k in (-4, -1, 0, 1, 2, 3, 5):
	tristep('Tri_Rotate', t, 2, sym('Tri_Rotate'), 'vii', k)
tristep('Tri_Mark', t, 2, sym('Tri_Mark'), 'vi')
tristep('Strip_Next marked', t, 2, sym('Strip_Next'), 'iiiiiip', 2, 2, 3, 0, 7)
tristep('Tri_Unmark', t, 2, sym('Tri_Unmark'), 'vi')
SDTest.Call('Mem_Free', 'vi', t)
call('VertCache_Reset', sym('VertCache_Reset'), 'v')
check('VertCache after reset', vcache)
for v in (5, 6, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 5, 6, 22):
	call('VertCache_Add', sym('VertCache_Add'), 'vi', v)
	call('VertCache_Oldest', sym('VertCache_Oldest'), 'i')
check('VertCache after adds', vcache)
for v in (5, 6, 7, 20, 22, 99, -1):
	call('VertCache_Find', sym('VertCache_Find'), 'ii', v)
call('VertCache_Reset', sym('VertCache_Reset'), 'v')
