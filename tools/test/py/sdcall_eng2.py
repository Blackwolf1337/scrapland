# engine3d leftovers 0x2c9d90..0x2d6010 (execfile'd at the end of sdcall.py, Python 1.5.2): the palettes and the
# PCX decoder on buffers in heap memory, the light formulas of RenderCalcIlum.cpp, the shadow BSP tree of bsp2.cpp on
# fake triangles, the radiosity regions, links and area lights (fake materials, sections added to the ini in
# memory), the segments of a mesh made from a CTriMesh, and the small constructors. Pointers are only compared. Not
# called: the files (Pal_Load, the PCX readers and writers on FILEs, bmp\normales.rad), the debug drawing of the BSP.
def en2z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def en2new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def en2free(p):
	SDTest.Call('Mem_Free', 'vi', p)
def en2i(a):
	return SDTest.Peek(a, '@i')
def en2poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def en2words(a, n):
	r = []
	for k in range(n):
		r.append(en2i(a + 4 * k))
	return r
def en2fl(a, n):
	r = ()
	k = 0
	while k < n:
		m = min(64, n - k)
		r = r + SDTest.Call('crt_memcpy', 'iFii', (0.0,) * m, a + 4 * k, 4 * m)[1]
		k = k + m
	return r
def en2set(a, v):
	k = 0
	while k < len(v):
		part = tuple(v[k:k + 64])
		SDTest.Call('crt_memcpy', 'iiFi', a + 4 * k, part, 4 * len(part))
		k = k + 64
# bytes into memory a word at a time (| keeps the ints in 32 bits)
def en2wb(a, bs):
	bs = list(bs)
	while len(bs) % 4:
		bs.append(0)
	for k in range(0, len(bs), 4):
		en2poke(a + k, bs[k] | (bs[k + 1] << 8) | (bs[k + 2] << 16) | (bs[k + 3] << 24))
def en2rb(a, n):
	r = []
	for k in range(n):
		r.append((en2i(a + (k & ~3)) >> (8 * (k & 3))) & 0xff)
	return r
def en2shorts(a, n):
	r = []
	for k in range(n):
		r.append((en2i(a + 2 * (k & ~1)) >> (16 * (k & 1))) & 0xffff)
	return r
def en2str(s):
	p = en2z(len(s) + 4)
	bs = []
	for c in s:
		bs.append(ord(c))
	en2wb(p, bs)
	return p

# palettes; the shifts of the 16 bit format are put back at the end
en2sh = (SDTest.Peek('g_Pal16Shift', 'i'), SDTest.Peek(sym('g_Pal16Shift') + 0x4, 'i'))
check('en2 Pal16 shifts', lambda: (en2sh[0], en2sh[1] & 0xffff))
en2b = en2z(16)
en2pal = en2z(0x400)
for k in range(256):
	en2poke(en2pal + 4 * k, ((k * 37) & 0xff) | (((k * 91) & 0xff) << 8) | (((k * 13) & 0xff) << 16) | (5 << 24))
en2cols = ((0, 0, 0), (255, 255, 255), (12, 200, 99), (128, 64, 32), (7, 3, 250))
for m in ((0, 0x7e0, 0x1f), (0x7c00, 0x3e0, 0x1f), (0xf00, 0xf0, 0xf), (0xff0000, 0xff00, 0xff), (0x1f, 0x7e0, 0xf800),
		(0x3, 0xc, 0x30), (0xf800, 0x7e0, 0x1f)):
	check('Pal16_SetMasks' + fmt(m), lambda m=m: (apply(SDTest.Call, (sym('Pal16_SetMasks'), 'viii') + m), SDTest.Peek('g_Pal16Shift', 'i'),
		SDTest.Peek(sym('g_Pal16Shift') + 0x4, 'i') & 0xffff))
	for c in en2cols:
		check('Pal16_FromRGB' + fmt(m + c), lambda c=c: apply(SDTest.Call, (sym('Pal16_FromRGB'), 'iiii') + c)[0])
	for v in (0, 0xffff, 0x1234, 0xf81f, 0x7bef):
		# the bytes one by one: a color as one int can look like an address to SDTest.Orig
		check('Pal16_ToRGB' + fmt((m, v)), lambda v=v: (SDTest.Call('Pal16_ToRGB', 'viiii', en2b, en2b + 1, en2b + 2, v),
			en2i(en2b) & 0xff, en2i(en2b) >> 8 & 0xff, en2i(en2b) >> 16 & 0xff))
# the masks of 565 stay for the 16 bit palettes
for a in ((0, 255, 12, 200, 99), (10, 20, 0, 0, 0), (0, 255, 37, 91, 13), (5, 5, 1, 2, 3), (7, 6, 1, 2, 3),
		(100, 200, 255, 0, 128)):
	check('Pal_Nearest' + fmt(a), lambda a=a: apply(SDTest.Call, (sym('Pal_Nearest'), 'iiiiiii', en2pal) + a)[0])
en2pal2 = en2z(0x400)
check('Pal_Copy', lambda: (SDTest.Call('Pal_Copy', 'viiiii', en2pal2, 3, en2pal, 7, 5), en2words(en2pal2, 10)))
def en2p16(p, n):
	c = (p + 0x200) & ~0x1ff
	return (c - p > 0, c - p <= 0x200, en2shorts(c, n))
en2p = SDTest.Call('Pal_ToPal16', 'iii', en2pal, 256)[0]
check('Pal_ToPal16', lambda: en2p16(en2p, 24))
for s in ((0.5, 1.5, 0.25), (1.0, 1.0, 1.0), (-1.0, 1e10, 0.0)):
	check('Pal16_Scale' + fmt(s), lambda s=s: (apply(SDTest.Call, (sym('Pal16_Scale'), 'viifff', en2p, 256) + s),
		en2p16(en2p, 24)))
en2free(en2p)
for s in ((2.0, 0.5, 1.0), (-1.0, 1e10, 0.0), (1.0, 1.0, 1.0)):
	check('Pal_ToPal16Scaled' + fmt(s), lambda s=s: en2p16(apply(SDTest.Call, (sym('Pal_ToPal16Scaled'), 'iiifff', en2pal, 20) +
		s)[0], 20))
for c in ((100, 50, 200, 10, 250, 0), (0, 0, 0, 255, 255, 255), (-5, 300, 7, 9, -9, 1000)):
	for t in (-40, -32, -31, 0, 17, 31, 32, 50):
		check('RGB_Blend' + fmt(c + (t,)), lambda c=c, t=t: apply(SDTest.Call, (sym('RGB_Blend'), 'vpppiiii') + c + (t,))[1:])
SDTest.Poke('g_Pal16Shift', en2sh[0])
SDTest.Poke(sym('g_Pal16Shift') + 0x4, en2sh[1])
check('en2 Pal16 shifts after', lambda: (SDTest.Peek('g_Pal16Shift', 'i'), SDTest.Peek(sym('g_Pal16Shift') + 0x4, 'i') & 0xffff))

# PCX headers and the run length decoder
en2hdr = en2z(0x80)
def en2pcx(manuf, bpp, planes, bpl, lx, ly, xmin, ymin, xmax, ymax):
	h = [manuf, 5, 1, bpp, xmin & 0xff, xmin >> 8 & 0xff, ymin & 0xff, ymin >> 8 & 0xff, xmax & 0xff, xmax >> 8 & 0xff,
		ymax & 0xff, ymax >> 8 & 0xff] + [0] * 0x74
	h[0x41] = planes
	h[0x42] = bpl & 0xff
	h[0x43] = bpl >> 8
	h[0x46] = lx & 0xff
	h[0x47] = lx >> 8
	h[0x48] = ly & 0xff
	h[0x49] = ly >> 8
	en2wb(en2hdr, h)
for h in ((10, 8, 1, 10, 0, 0, 2, 1, 11, 4), (10, 8, 1, 4, 5, 0, 0, 0, 3, 299), (10, 8, 3, 2, 0, 9, 7, 0, 3, 0),
		(11, 8, 1, 4, 4, 4, 0, 0, 3, 3), (10, 4, 1, 4, 4, 4, 0, 0, 3, 3)):
	en2pcx(h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9])
	check('Pcx_CheckHeader' + fmt(h), lambda: (SDTest.Call('Pcx_CheckHeader', 'ii', en2hdr)[0], en2shorts(en2hdr + 0x46, 2)))
en2dst = en2z(64)
en2src = en2z(64)
for planes, bpl, lx, ly, data in ((1, 4, 4, 2, (0xc3, 0x11, 0x22, 0x05, 0xc2, 0xc5, 0x07)),
		(3, 2, 2, 1, (0xc4, 0x33, 0x44, 0x55)), (1, 4, 3, 1, (0x01, 0x02, 0x03, 0xc1, 0x00)),
		(2, 4, 3, 2, (0xc0, 0x09, 0xc3, 0x41, 0xc1, 0x00, 0x01, 0x02, 0x03, 0xc1, 0x00, 0xc2, 0x05, 0x06, 0xc1, 0x00, 0xc4,
		0x07))):
	en2pcx(10, 8, planes, bpl, lx, ly, 0, 0, 0, 0)
	en2wb(en2dst, [0xee] * 64)
	en2wb(en2src, data)
	check('Pcx_Decode' + fmt((planes, bpl, lx, ly)), lambda n=planes * lx * ly: (SDTest.Call('Pcx_Decode', 'iiii', en2dst,
		en2src, en2hdr)[0] - en2src, en2rb(en2dst, n + 2)))
for p in (en2b, en2pal, en2pal2, en2hdr, en2dst, en2src):
	en2free(p)

# the light formulas
en2k = SDTest.Peek('g_IllumSmooth', 'i')
for k in (en2k, 0, 0x3e800000, 0x3f800000):
	SDTest.Poke('g_IllumSmooth', k)
	for v in (0.0, 0.25, 0.5, 1.0, -0.5, 2.0):
		check('Illum_Smooth' + fmt((k, v)), lambda v=v: SDTest.Call('Illum_Smooth', 'ff', v)[0])
SDTest.Poke('g_IllumSmooth', en2k)
en2dest = en2z(16)
en2pos = en2z(16)
en2n = en2z(16)
en2mat = en2z(17 * 4)
en2luz = en2z(0x6c)
en2set(en2mat, (0.9, 0.7, 0.5, 1.0, 0.3, 0.25, 0.2, 0.75) + (0.0,) * 9)
def en2light(tipo, falloff, dirv):
	en2poke(en2luz, tipo)
	en2set(en2luz + 4, (0.8, 0.6, 0.4, 1.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.2, 0.3, 0.5, 10.0, 20.0, 30.0) + dirv +
		(100.0, falloff, 1.0, 0.01, 0.0001, 0.5, 1.0, 60.0))
for tipo in (1, 2, 3):
	for falloff in (1.0, 2.0):
		for dirv in ((0.0, -1.0, 0.0), (0.26726124, -0.53452247, 0.80178374)):
			en2light(tipo, falloff, dirv)
			for flags in (0, 1):
				for pos in ((10.0, 0.0, 30.0), (17.0, 1.0, 30.0), (50.0, 5.0, -20.0), (10.0, 20.0, 30.0), (200.0, 0.0, 0.0),
						(10.0, -55.0, 31.0)):
					for n in ((0.0, 1.0, 0.0), (0.0, -1.0, 0.0), (0.6, 0.8, 0.0)):
						en2set(en2dest, (0.1, 0.1, 0.1, 0.1))
						en2set(en2pos, pos)
						en2set(en2n, n)
						check('Illum_Light' + fmt((tipo, falloff, dirv, flags, pos, n)), lambda flags=flags: (SDTest.Call('Illum_Light',
							'iiiiiii', en2dest, en2pos, en2n, en2mat, en2luz, flags)[0], en2fl(en2dest, 4)))
for p in (en2dest, en2pos, en2n, en2mat, en2luz):
	en2free(p)

# Illum_VertList: a list of 6 vertices (format 1: pos, normal, tex), a fake material, real lights of the engine
en2lv = en2z(0x14)
SDTest.Call('CVertList_ctor', 'tii', en2lv)
SDTest.Call('CVertList_SetFormat', 'tvii', en2lv, 1)
SDTest.Call('CVertList_Alloc', 'tviii', en2lv, 6, 0)
en2verts = ((10.0, 0.0, 30.0, 0.0, 1.0, 0.0), (17.0, 1.0, 30.0, 0.0, 1.0, 0.0), (50.0, 5.0, -20.0, 0.6, 0.8, 0.0),
	(10.0, 20.0, 30.0, 0.0, 1.0, 0.0), (900.0, 0.0, 0.0, 0.0, 1.0, 0.0), (10.0, -5.0, 31.0, 0.0, -1.0, 0.0))
for k in range(6):
	en2set(en2i(en2lv) + 0x20 * k, en2verts[k] + (0.0, 0.0))
en2m = en2z(0xb0)
en2wb(en2m + 0x10, (40, 60, 80, 255, 200, 150, 100, 255, 10, 20, 30, 255, 0, 0, 0, 0))
en2wb(en2m + 0x28, (255, 200, 150, 255))
en2cols = en2z(6 * 16)
for tipo in (5000, 5001, 5002):
	l = en2z(0xcc)
	SDTest.Call('CLight_ctor', 'tii', l)
	en2poke(l + 8, tipo)
	en2set(l + 0x10, (10.0, 20.0, 30.0, 0.0, -1.0, 0.2))
	en2poke(l + 0x28, 0x7f40c0ff)
	en2set(l + 0x2c, (1.5,))
	en2set(l + 0x34, (120.0, 40.0, 60.0, 1.0))
	en2set(l + 0x80, (10.0, 20.0, 30.0, 0.1, -2.0, 0.3))
	en2set(en2cols, (0.05,) * 24)
	check('Illum_VertList' + fmt((tipo,)), lambda l=l: (SDTest.Call('Illum_VertList', 'iiiii', en2cols, en2lv, en2m, l)[0],
		en2fl(en2cols, 24)))
	SDTest.Call('CLight_dtor', 'tvi', l)
	en2free(l)
en2free(en2i(en2lv))
for p in (en2lv, en2m, en2cols):
	en2free(p)

# the shadow BSP: 24 small horizontal triangles on a grid, three materials (casting, transparent, transparent that
# casts with MAT_SHADOW)
en2mats = en2z(3 * 0xb0)
en2poke(en2mats + 0xb0 + 0x38, 0x00010000)
en2poke(en2mats + 0x160 + 0x38, 0x00010000)
en2poke(en2mats + 0x160 + 0x3c, 0x20)
def en2tri(k):
	x, y, z = (k % 6) * 40.0 - 100.0, (k % 3) * 20.0 - 10.0, (k / 6) * 50.0 - 75.0
	return (x, y, z, x + 5.0, y, z, x, y, z + 5.0, 0.0, 1.0, 0.0, -y)
def en2node(n):
	nd = en2new(0x20)
	en2wb(nd, [0xcd] * 0x20)
	SDTest.Call('Bsp2Node_ctor', 'tii', nd)
	if n:
		SDTest.Call('Table_Alloc38', 'tvii', nd + 0x18, n)
		for k in range(n):
			p = en2i(nd + 0x1c) + 0x38 * k
			en2set(p, en2tri(k))
			en2poke(p + 0x34, en2mats + 0xb0 * ((k / 4) % 3))
	return nd
# the plane only of the split nodes: new leaves leave it unset
def en2leaf(nd):
	if en2i(nd + 0x10) or en2i(nd + 0x14):
		return (en2fl(nd, 4), en2i(nd + 0x10) != 0, en2i(nd + 0x14) != 0, en2i(nd + 0x18))
	return (en2i(nd + 0x18), en2fl(en2i(nd + 0x1c), 3))
en2nd = en2node(0)
check('Bsp2Node_ctor', lambda: en2words(en2nd, 8))
SDTest.Call('operator_delete_sized', 'vii', en2nd, 0x20)
en2root = en2node(24)
en2luces = en2z(4)
en2poke(en2luces, en2root)
for pl in ((1.0, 0.0, 0.0, -2.5), (0.0, 1.0, 0.0, -10.0), (0.0, 0.0, 1.0, 0.0), (0.0, 1.0, 0.0, 1000.0)):
	check('Bsp2_Split count' + fmt(pl), lambda pl=pl: SDTest.Call('Bsp2_Split', 'viiFpp', 1, en2root, pl, 7, 7)[2:])
check('Bsp2_ChoosePlane', lambda: SDTest.Call('Bsp2_ChoosePlane', 'tbiiF', en2luces, en2root, (9.0,) * 4)[0:2])
check('Bsp2_AddTri count', lambda: (SDTest.Call('Bsp2_AddTri', 'viippiiiii', 1, en2root, 3, 4, 1, en2i(en2root + 0x1c), 0, 0,
	0)[1:], SDTest.Call('Bsp2_AddTri', 'viippiiiii', 1, en2root, 3, 4, -1, en2i(en2root + 0x1c), 0, 0, 0)[1:]))
en2c0 = (SDTest.Peek('g_Bsp2NTriCmp', 'i'), SDTest.Peek('g_Bsp2NComp', 'i'))
check('Bsp2_Build', lambda: (SDTest.Call('Bsp2_Build', 'tviii', en2luces, en2root, 0), en2leaf(en2root),
	SDTest.Call('Bsp2_Count', 'ii', en2root)[0]))
def en2walk(nd, depth):
	if not nd:
		return ()
	r = (depth, en2leaf(nd))
	return r + en2walk(en2i(nd + 0x10), depth + 1) + en2walk(en2i(nd + 0x14), depth + 1)
check('Bsp2 tree', lambda: en2walk(en2root, 0))
check('Bsp2_Count', lambda: (SDTest.Call('Bsp2_Count', 'ii', en2root)[0], SDTest.Call('Bsp2_Count', 'ii', 0)[0]))
en2segs = []
for k in (0, 5, 7, 13, 23):
	t = en2tri(k)
	en2segs.append(((t[0] + 1.0, t[1] + 50.0, t[2] + 1.0), (t[0] + 1.0, t[1] - 50.0, t[2] + 1.0)))
	en2segs.append(((t[0] + 1.0, t[1] - 50.0, t[2] + 1.0), (t[0] + 1.5, t[1] + 50.0, t[2] + 1.5)))
	en2segs.append(((t[0] + 3.0, t[1] + 50.0, t[2] + 3.0), (t[0] + 3.0, t[1] - 50.0, t[2] + 3.0)))
en2segs = en2segs + [((-150.0, 5.0, -100.0), (150.0, 5.0, 100.0)), ((-102.0, 100.0, -80.0), (110.0, -100.0, 90.0)),
	((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((-98.0, 60.0, -73.0), (-98.0, -9.0, -73.0))]
for a, b in en2segs:
	check('Bsp2_CheckLine' + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('Bsp2_CheckLine', 'iFFii', a, b, 0, en2root)[0],
		SDTest.Call('Bsp2_CheckLine', 'iFFFi', a, b, (9.0, 9.0, 9.0), en2root)[3]))
	check('BspLights_CheckLine' + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('BspLights_CheckLine', 'tiiFFi', en2luces, a, b, 0)[0],
		SDTest.Call('BspLights_CheckLine', 'tiiFFF', en2luces, a, b, (9.0, 9.0, 9.0))[3]))
check('Bsp2 counters', lambda: (SDTest.Peek('g_Bsp2NTriCmp', 'i') - en2c0[0], SDTest.Peek('g_Bsp2NComp', 'i') - en2c0[1]))
for p in ((0.0, 0.0, 0.0), (-90.0, 10.0, 60.0), (90.0, -50.0, -60.0), (2.5, 10.0, 2.5)):
	check('Bsp2_FindLeaf' + fmt(p), lambda p=p: (en2leaf(SDTest.Call('Bsp2_FindLeaf', 'iFi', p, en2root)[0]),
		en2leaf(SDTest.Call('BspLights_FindLeaf', 'tiiF', en2luces, p)[0])))
check('Bsp2_Stats', lambda: (SDTest.Call('Bsp2_Stats', 'vi', 0), SDTest.Call('Bsp2_Stats', 'vi', 1)))
# the first leaf and the node above it
en2f = en2root
while en2i(en2i(en2f + 0x10) + 0x10):
	en2f = en2i(en2f + 0x10)
en2t = en2z(0x38)
check('Bsp2Tri_Copy', lambda: (SDTest.Call('Bsp2Tri_Copy', 'iii', en2t, en2i(en2i(en2f + 0x10) + 0x1c))[0] == en2t,
	en2fl(en2t, 13), en2i(en2t + 0x34) - en2mats))
check('Bsp2_AddTri', lambda: (SDTest.Call('Bsp2_AddTri', 'viippiiFFF', 0, en2f, 0, 0, 1, en2t, (1.0, 2.0, 3.0),
	(4.0, 5.0, 6.0), (7.0, 8.0, 9.0))[1:3], en2fl(en2i(en2i(en2f + 0x10) + 0x1c), 13),
	en2i(en2i(en2i(en2f + 0x10) + 0x1c) + 0x34) - en2mats))
en2free(en2t)
check('BspLights_dtor', lambda: (SDTest.Call('BspLights_dtor', 'tvi', en2luces), en2i(en2luces)))
# Bsp2Node_dtor on a node of its own with children from a split
en2nd = en2node(10)
SDTest.Call('Bsp2_Split', 'viiFii', 0, en2nd, (1.0, 0.0, 0.0, -50.0), 0, 0)
check('Bsp2_Split', lambda: en2walk(en2nd, 0))
check('Bsp2Node_dtor', lambda: (SDTest.Call('Bsp2Node_dtor', 'tvi', en2nd), en2words(en2nd + 0x10, 4)))
SDTest.Call('operator_delete_sized', 'vii', en2nd, 0x20)
en2free(en2luces)
en2free(en2mats)

# the CTable instances
en2tab = en2z(8)
check('Table_AllocC', lambda: (SDTest.Call('Table_AllocC@2d2e10', 'tvii', en2tab, 4), en2i(en2tab), en2i(en2tab + 4) != 0))
SDTest.Call('operator_delete_array', 'vi', en2i(en2tab))
en2poke(en2tab, 0)
en2poke(en2tab + 4, 0)
# only the elements written before are printed
en2tn = [0]
for n in (3, 3, 5, 2, 0, -1, 4):
	def en2r(n=n):
		SDTest.Call('Table_Resize38', 'tvii', en2tab, n)
		r = (en2i(en2tab), en2i(en2tab + 4) != 0)
		if en2i(en2tab + 4):
			r = r + (en2words(en2i(en2tab + 4), 14 * min(min(en2i(en2tab), en2tn[0]), 3)),)
			for k in range(14 * en2i(en2tab)):
				en2poke(en2i(en2tab + 4) + 4 * k, 0x1000 * n + k)
		en2tn[0] = en2i(en2tab)
		return r
	check('Table_Resize38' + fmt((n,)), en2r)
SDTest.Call('Table_Resize38', 'tvii', en2tab, 0)
en2free(en2tab)

# radiosity: links, regions and area lights
def en2nenl():
	return SDTest.Peek('g_BspNumLinks', 'i')
en2n0 = en2nenl()
en2e = []
for k in range(5):
	e = en2new(12)
	sig = 0
	if k in (1, 2, 4):
		sig = en2e[k - 1]
	check('BspLink_ctor' + fmt((k,)), lambda e=e, k=k, sig=sig: (SDTest.Call('BspLink_ctor', 'tiiiif', e, sig, 0x1234 + k,
		0.5 * k)[0] == e, en2i(e) == sig, en2i(e + 4), en2fl(e + 8, 1), en2nenl() - en2n0))
	en2e.append(e)
check('BspLink_Delete', lambda: (SDTest.Call('BspLink_Delete', 'tiii', en2e[2], 1)[0] == en2e[2], en2nenl() - en2n0))
check('BspLink_dtor', lambda: (SDTest.Call('BspLink_dtor', 'tvi', en2e[4]), en2words(en2e[4], 2), en2nenl() - en2n0))
check('BspLink_Delete 0', lambda: (SDTest.Call('BspLink_Delete', 'tiii', en2e[4], 0)[0] == en2e[4], en2nenl() - en2n0))
SDTest.Call('operator_delete_sized', 'vii', en2e[4], 12)
def en2region(x, y, lx, ly, padre):
	r = en2new(100)
	en2wb(r, [0xab] * 100)
	SDTest.Call('BspRegion_ctor', 'tiiiiiii', r, x, y, lx, ly, padre)
	return r
en2r0 = en2region(0x12345, 7, -1, 20, 0)
check('BspRegion_ctor', lambda: (en2words(en2r0, 25), en2i(en2r0 + 0x10)))
en2set(en2r0 + 0x28, (0.0, 1.0, 0.0, -5.0, 9.0, 9.0, 9.0, 9.0, 0.25, 0.5, 0.75, 1.0))
en2kids = []
for k in range(4):
	en2kids.append(en2region(k, k + 1, 2, 3, en2r0))
	en2poke(en2r0 + 4 * k, en2kids[k])
check('BspRegion_ctor padre', lambda: (en2words(en2kids[1], 25)[:4], en2fl(en2kids[1] + 0x18, 18), en2words(en2kids[1] + 0x60, 1),
	en2i(en2kids[1] + 0x10) == en2r0))
en2poke(en2kids[2] + 4, en2region(1, 1, 1, 1, en2kids[2]))
for k in range(3):
	e = en2new(12)
	SDTest.Call('BspLink_ctor', 'tiiiif', e, en2i(en2r0 + 0x60), en2kids[k], 1.0)
	en2poke(en2r0 + 0x60, e)
check('BspRegion_dtor', lambda: (SDTest.Call('BspRegion_dtor', 'tvi', en2r0), en2words(en2r0, 5), en2i(en2r0 + 0x60),
	en2nenl() - en2n0))
SDTest.Call('operator_delete_sized', 'vii', en2r0, 100)
en2a = en2z(0x20)
en2wb(en2a, [0x77] * 0x20)
check('RadAreaLight_ctor', lambda: (SDTest.Call('RadAreaLight_ctor', 'tii', en2a)[0] == en2a, en2words(en2a, 8)))
en2a2 = en2new(0x20)
SDTest.Call('RadAreaLight_ctor', 'tii', en2a2)
SDTest.Call('String_Set', 'tvis', en2a2 + 4, 'name')
en2poke(en2a, en2a2)
check('RadAreaLight_Delete', lambda: (SDTest.Call('RadAreaLight_Delete', 'tiii', en2a, 0)[0] == en2a, en2words(en2a, 2)))
en2free(en2a)
en2rad = en2z(12)
check('CRadiosity_ctor', lambda: (SDTest.Call('CRadiosity_ctor', 'tiiz', en2rad, None)[0] == en2rad, en2i(en2rad + 4) != 0,
	en2i(en2rad + 8)))
for sect, key, val in (('luz1', 'rgbi', '10 20 30 2.5'), ('luz1', 'atten', '5 60'), ('luz1', 'falloff', '1.5'),
		('light2', 'x', '1'), ('luz3', 'rgbi', '1 2 3 0.5'), ('luz3', 'falloff', '3')):
	SDTest.Call('CIniFile_SetStr', 'tiisss', en2rad, sect, key, val)
en2names = ('plain', 'm(+area_light: luz1)', 'm( +area_light:luz2)x', 'old(+luz2)', 'o(+ luz3 )y', '(+nada)(+luz1)',
	'(', '(+', '( ', '(x(+luz2', 'a(+AREA_LIGHT:luz1')
en2mt = []
for k in range(len(en2names)):
	m = en2z(0xb0)
	en2poke(m + 4, en2str(en2names[k]))
	en2wb(m + 0x14, (250, 128, 3, 255))
	if k in (1, 2, 10):
		en2poke(m + 0x3c, 0x100)
	en2mt.append(m)
en2dl = en2z(0x20)
for k in range(len(en2names)):
	m = en2mt[k]
	en2wb(en2dl, [0x55] * 0x20)
	en2poke(en2dl + 4, 0)
	check('CRadiosity_ReadMat' + fmt((en2names[k],)), lambda m=m: (SDTest.Call('CRadiosity_ReadMat', 'tbiii', en2rad, m, 0)[0],
		SDTest.Call('CRadiosity_ReadMat', 'tbiii', en2rad, m, en2dl)[0], en2words(en2dl, 1), SDTest.Peek(en2dl + 4, '@p'),
		en2rb(en2dl + 8, 4), en2fl(en2dl + 0xc, 5)))
	if en2i(en2dl + 4):
		SDTest.Call('String_Set', 'tviz', en2dl + 4, None)
en2free(en2dl)
en2got = []
for k in (0, 1, 3, 4, 1, 10):
	m = en2mt[k]
	check('CRadiosity_Find' + fmt((en2names[k],)), lambda m=m: SDTest.Call('CRadiosity_Find', 'tiii', en2rad, m)[0] != 0)
	def en2get(m=m):
		l = SDTest.Call('CRadiosity_Get', 'tiii', en2rad, m)[0]
		en2got.append(l)
		if not l:
			return l
		return (en2got.index(l), SDTest.Peek(l + 4, '@p'), en2rb(l + 8, 4), en2fl(l + 0xc, 5), en2i(en2rad + 8) == l,
			SDTest.Call('CRadiosity_Find', 'tiii', en2rad, m)[0] == l)
	check('CRadiosity_Get' + fmt((en2names[k],)), en2get)
	en2c = en2z(8)
	check('CRadiosity_GetColor' + fmt((en2names[k],)), lambda m=m, c=en2c: (SDTest.Call('CRadiosity_GetColor', 'tbiii', en2rad, m,
		c)[0], en2rb(c, 4), en2fl(c + 4, 1)))
	en2free(en2c)
check('CRadiosity_Add plain', lambda: SDTest.Call('CRadiosity_Add', 'tiii', en2rad, en2mt[0])[0])
check('CRadiosity_ReadMatWarn', lambda: (SDTest.Call('CRadiosity_ReadMatWarn', 'tbiii', en2rad, en2mt[0], 0)[0],
	SDTest.Call('CRadiosity_ReadMatWarn', 'tbiii', en2rad, en2mt[1], 0)[0], SDTest.Call('CRadiosity_ReadMatWarn', 'tbiii', en2rad, en2mt[4], 0)[0]))
check('CRadiosity_dtor', lambda: (SDTest.Call('CRadiosity_dtor', 'tvi', en2rad), en2words(en2rad + 4, 2)))
for m in en2mt:
	en2free(en2i(m + 4))
	en2free(m)
en2free(en2rad)
# the normals as read before
en2nrm = (SDTest.Peek('g_RadNormals', 'i'), SDTest.Peek(sym('g_RadNormals') + 0x4, 'i'))
en2v = en2z(36)
SDTest.Poke('g_RadNormals', 3)
SDTest.Poke(sym('g_RadNormals') + 0x4, en2v)
check('Rad_Normals', lambda: (lambda r: (r[0] == en2v, r[1]))(SDTest.Call('Rad_Normals', 'ip', 0)))
SDTest.Poke('g_RadNormals', en2nrm[0])
SDTest.Poke(sym('g_RadNormals') + 0x4, en2nrm[1])
en2free(en2v)

# the small constructors and the node of a 3D object
en2o = en2z(16)
en2wb(en2o, [0x99] * 16)
check('CEVA_ctor', lambda: (SDTest.Call('CEVA_ctor', 'tii', en2o)[0] == en2o, en2words(en2o, 4)))
en2wb(en2o, [0x99] * 16)
check('CAnimVert_ctor', lambda: (SDTest.Call('CAnimVert_ctor', 'tii', en2o)[0] == en2o, en2words(en2o, 4)))
en2free(en2o)
en2ob = en2z(12)
en2n1 = en2z(12)
en2n2 = en2z(12)
en2poke(en2n1 + 8, 5)
en2poke(en2n2 + 8, 3)
for nd in (en2n1, en2n2, 0, en2n2, en2n2):
	check('CObjBase3D_SetNode' + fmt((nd == en2n1, nd == en2n2)), lambda nd=nd: (SDTest.Call('CObjBase3D_SetNode', 'tvii', en2ob, nd),
		en2i(en2ob + 8) == nd, en2i(en2n1 + 8), en2i(en2n2 + 8)))
for p in (en2ob, en2n1, en2n2):
	en2free(p)

# the segments of the meshes from a CTriMesh with MESHD3D_POS: an octahedron (closed) and a fan with an edge of
# three triangles, materials with a name and a map
en2segm = en2z(3 * 0xb0)
en2segn = (en2str('segA'), en2str('segB'), en2str('segC'))
en2segmap = en2z(3 * 0x60)
for k in range(3):
	en2poke(en2segmap + 0x60 * k, en2segn[k])
	en2poke(en2segm + 0xb0 * k + 4, en2segn[k])
	en2poke(en2segm + 0xb0 * k + 0x9c, en2segmap + 0x60 * k)
def en2malla(verts, tris, mats):
	m = en2new(0x4c)
	SDTest.Call('CTriMesh_ctor', 'tii', m)
	SDTest.Call('CTriMesh_SetNumVert', 'tviii', m, len(verts), 0)
	SDTest.Call('CTriMesh_SetNumTVert', 'tviii', m, 3, 0)
	SDTest.Call('CTriMesh_SetNumTri', 'tviii', m, len(tris), 0)
	for k in range(len(verts)):
		SDTest.Call('CTriMesh_SetVert', 'tviiF', m, k, verts[k])
	for k in range(3):
		SDTest.Call('CTriMesh_SetTVert', 'tviiF', m, k, (0.5 * k, 1.0 - 0.25 * k))
	for k in range(len(tris)):
		apply(SDTest.Call, (sym('CTriMesh_SetTriVerts'), 'tviiiii', m, k) + tris[k])
		SDTest.Call('CTriMesh_SetTriTVerts', 'tviiiii', m, k, 0, 1, 2)
		SDTest.Call('CTriMesh_SetTriNormals', 'tviiFFF', m, k, (0.0, 1.0, 0.0), (0.0, 1.0, 0.0), (0.0, 1.0, 0.0))
		SDTest.Call('CTriMesh_SetTriMat', 'tviii', m, k, en2segm + 0xb0 * mats[k])
		SDTest.Call('CTriMesh_SetTriFlags', 'tviii', m, k, k + 1)
	return m
def en2seglist(d):
	r = ()
	while d:
		x = (en2i(d + 0xc), en2i(d + 0x2c), en2i(d + 0x30), en2i(d + 0x34))
		if en2i(d + 0x3c):
			x = x + (en2fl(en2i(d + 0x3c), 4 * en2i(d + 0x30)),)
		if en2i(d + 0x40):
			x = x + (en2shorts(en2i(d + 0x40), 4 * en2i(d + 0x34)),)
		r = r + (x,)
		d = en2i(d + 8)
	return r
en2oct = ((0.0, 10.0, 0.0), (10.0, 0.0, 0.0), (0.0, 0.0, 10.0), (-10.0, 0.0, 0.0), (0.0, 0.0, -10.0), (0.0, -10.0, 0.0))
en2octt = ((0, 1, 4), (0, 2, 1), (0, 3, 2), (0, 4, 3), (5, 4, 1), (5, 1, 2), (5, 2, 3), (5, 3, 4))
en2fan = ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 5.0, 10.0), (0.0, -5.0, 10.0), (0.0, 10.0, -3.0), (4.0, 4.0, 4.0))
en2fant = ((0, 1, 2), (1, 0, 3), (1, 0, 4), (2, 4, 5), (5, 2, 3))
for verts, tris, mats in ((en2oct, en2octt, (0, 0, 0, 0, 0, 0, 0, 0)), (en2oct, en2octt, (0, 1, 2, 0, 1, 2, 0, 1)),
		(en2fan, en2fant, (0, 1, 0, 1, 2))):
	m = en2malla(verts, tris, mats)
	for flags in (0x12, 0x13):
		d = en2new(200)
		check('CMeshD3D_CalcSegments' + fmt((len(verts), mats, flags)), lambda d=d, m=m, flags=flags:
			(SDTest.Call('CMeshD3D_ctorMesh', 'tiiiii', d, m, flags, 0)[0] == d, en2seglist(d)))
		SDTest.Call('CMeshD3D_Delete', 'tiii', d, 1)
	SDTest.Call('CTriMesh_Delete', 'tiii', m, 1)
for p in en2segn:
	en2free(p)
en2free(en2segm)
en2free(en2segmap)
