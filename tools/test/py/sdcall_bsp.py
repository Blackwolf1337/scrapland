# engine3d Engine/Bsp.c (execfile'd at the end of sdcall.py, Python 1.5.2): the light map code of the floor on fake
# lists, regions, pages and maps in heap memory (calloc, or the engine's new where the code frees them). Pointers
# are only compared; lists in output are turned into their index. Not called: the drawing (D3D), the TXF files and
# messages (file writes, dialogs), the material of a page (loads a texture), the debug drawing.
def bspz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def bspnew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def bspi(a):
	return SDTest.Peek(a, '@i')
def bspfl(a, n):
	r = ()
	i = 0
	while i < n:
		k = min(64, n - i)
		r = r + SDTest.Call('crt_memcpy', 'iFii', (0.0,) * k, a + 4 * i, 4 * k)[1]
		i = i + k
	return r
def bspset(a, v):
	i = 0
	while i < len(v):
		part = tuple(v[i:i + 64])
		SDTest.Call('crt_memcpy', 'iiFi', a + 4 * i, part, 4 * len(part))
		i = i + 64
def bsppoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def bspbyte(a, v):
	w = bspi(a & ~3)
	s = 8 * (a & 3)
	bsppoke(a & ~3, (w & ~(0xff << s)) | ((v & 0xff) << s))
def bspflat(vs):
	r = []
	for v in vs:
		r = r + list(v)
	return r
def bsprnds(n, lo, hi):
	r = []
	for i in range(n):
		r.append(rnd(lo, hi))
	return r
# CBmp and CBmpFlt images: header in calloc memory, pixels of the engine
def bspbmp(w, h, vals):
	b = bspz(12)
	SDTest.Call('CBmp_Create', 'tviii', b, w, h)
	img = bspi(b + 8)
	for i in range(len(vals)):
		bsppoke(img + 4 * i, vals[i])
	return b
def bspflt2(at, w, h, vals):
	SDTest.Call('CBmpFlt2_Create', 'tviii', at, w, h)
	bspset(bspi(at + 8), vals)
def bspcols(n):
	r = []
	for i in range(n):
		r.append(int(rnd(0.0, 65536.0)) + (int(rnd(0.0, 128.0)) << 16) + (int(rnd(0.0, 128.0)) << 24))
	return r
def bspimg(b, ch):
	w, h, img = bspi(b), bspi(b + 4), bspi(b + 8)
	if not img:
		return (w, h, None)
	if ch == 1:
		return (w, h, tpeek(img, w * h))
	return (w, h, bspfl(img, w * h * ch))
def bspidx(p, lst):
	for i in range(len(lst)):
		if lst[i] == p:
			return i
	if p:
		return 'ptr'
	return None
bsptexx = SDTest.Peek('g_ShadowMapTexX', 'i')
bsptexy = SDTest.Peek('g_ShadowMapTexY', 'i')
check('bsp texture size', lambda: (bsptexx, bsptexy, SDTest.Peek('g_LightmapPackMode', 'i'), SDTest.Peek('g_LightmapDxtPadding', 'i'),
	SDTest.Peek('g_ShadowMapModulate4x', 'i'), SDTest.Peek('g_R_ForceLMap', 'i'), SDTest.Peek('g_LmapSecond', 'i'), SDTest.Peek('g_LightmapLimitUV', 'i')))
check('bsp scale', lambda: bspfl(symva('g_ShadowMapScale'), 1) + bspfl(symva('g_ShadowMapRes'), 1) + bspfl(symva('g_BspLodDist'), 6) + bspfl(symva('g_BspLodLevel'), 1))

# Plane_Calc
for bspvs in (tuple(vecs(3)), tuple(vecs(3)), tuple(vecs(3)), ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0), (1.0, 2.0, 3.0)),
		((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (2.0, 2.0, 2.0)), ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1e-20, 0.0)),
		((nan, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)), ((0.0, 0.0, 0.0), (inf, 0.0, 0.0), (0.0, 1.0, 0.0)),
		((5.0, -2.0, 1e6), (5.0, -2.0, -1e6), (-3.0, 4.0, 0.5))):
	check('Plane_Calc' + fmt(bspvs), lambda vs=bspvs: SDTest.Call('Plane_Calc', 'bFFFF', (9.0, 9.0, 9.0, 9.0), vs[0], vs[1],
		vs[2])[0:2])

# fake lists: vertices, normals, uv and light map uv; tri 0..2 one list, 3 alone
bsptv = []
bsptris = []
for bsp_i in range(4):
	bspt = bspz(0x224)
	bspv = vecs(3)
	bsptv.append(bspv)
	bspset(bspt + 4, bspflat(bspv))
	bspset(bspt + 0x28, bspflat(vecs(3)))
	bspset(bspt + 0x4c, bsprnds(12, 0.0, 1.0))
	bsptris.append(bspt)
bsppoke(bsptris[0], bsptris[1])
bsppoke(bsptris[1], bsptris[2])
check('BspTri_CalcPlanes', lambda: (SDTest.Call('BspTri_CalcPlanes', 'vi', bsptris[0]), SDTest.Call('BspTri_CalcPlanes', 'vi', bsptris[3]),
	map(lambda bsp_t: (bspfl(bsp_t + 0x94, 4), bspi(bsp_t + 0xa4)), bsptris)))
for bspt in bsptris:
	check('BspTri_CalcCenter', lambda bsp_t=bspt: (SDTest.Call('BspTri_CalcCenter', 'vi', bsp_t), bspfl(bsp_t + 0x1a8, 5)))
	check('BspTri_Side', lambda bsp_t=bspt: SDTest.Call('BspTri_Side', 'ii', bsp_t)[0])
check('BspTri_ListBox', lambda: SDTest.Call('BspTri_ListBox', 'iFi', (0.0,) * 6, bsptris[0])[1])
check('BspTri_ListBox empty', lambda: SDTest.Call('BspTri_ListBox', 'iFi', (0.0,) * 6, 0)[1])
check('BspTri_AddBox', lambda: SDTest.Call('BspTri_AddBox', 'iFFi', (0.0,) * 6, (-1.0, -2.0, -3.0, 1.0, 2.0, 3.0),
	bsptris[3])[1])
check('BspTri_AddBox nan', lambda: SDTest.Call('BspTri_AddBox', 'iFFi', (0.0,) * 6, (nan, 0.0, inf, nan, -inf, 0.0),
	bsptris[2])[1])
check('BspTri_Count', lambda: (SDTest.Call('BspTri_Count', 'ii', bsptris[0])[0], SDTest.Call('BspTri_Count', 'ii', 0)[0]))
# points on the plane and off it, normals and uv at them
for bsp_i in range(4):
	bspt = bsptris[bsp_i]
	bspv = bsptv[bsp_i]
	for bsp_a, bsp_b in ((0.25, 0.25), (0.0, 0.0), (1.0, 0.0), (0.6, 0.7), (-2.0, 3.0)):
		bsp_p = []
		for bsp_k in range(3):
			bsp_p.append(bspv[0][bsp_k] + bsp_a * (bspv[1][bsp_k] - bspv[0][bsp_k]) + bsp_b * (bspv[2][bsp_k] - bspv[0][bsp_k]))
		bsp_p = tuple(bsp_p)
		for bsp_lm in (0, 1):
			check('BspTri_Interp' + fmt((bsp_i, bsp_a, bsp_b, bsp_lm)), lambda bsp_t=bspt, bsp_p=bsp_p, bsp_lm=bsp_lm: (SDTest.Call('BspTri_Interp', 'viFFFi', bsp_t, bsp_p,
				(9.0, 9.0, 9.0), (9.0, 9.0), bsp_lm)[2:], SDTest.Call('BspTri_Interp', 'viFiFi', bsp_t, bsp_p, 0, (9.0, 9.0), bsp_lm)[2],
				SDTest.Call('BspTri_Interp', 'viFFii', bsp_t, bsp_p, (9.0, 9.0, 9.0), 0, bsp_lm)[2]))
		check('BspTri_TexelOf' + fmt((bsp_i, bsp_a, bsp_b)), lambda bsp_t=bspt, bsp_p=bsp_p: SDTest.Call('BspTri_TexelOf', 'tbiFpp', bsp_t, bsp_p, 0, 0)[0:1] +
			SDTest.Call('BspTri_TexelOf', 'tbiFpp', bsp_t, bsp_p, 0, 0)[2:])
	check('BspTri_Interp off' + fmt((bsp_i,)), lambda bsp_t=bspt: SDTest.Call('BspTri_Interp', 'viFFFi', bsp_t, vec(), (9.0, 9.0, 9.0),
		(9.0, 9.0), 0)[2:])

# the plane of a light map: texel to world and back
bspplanes = ((0.0, 0.5, 0.5, 1.0), (1.0, 0.0, 0.0, -2.0), (0.0, 0.0, 0.0, 1.0), (0.3, -0.9, 0.2, 7.5),
	(nan, 1.0, 0.0, 0.0))
for bsppl in bspplanes:
	for bsp_axis in (0, 1, 2):
		for bsp_u, bsp_v, bsp_s in ((1.5, -2.25, 2.0), (0.0, 0.0, 0.0), (100.0, 3.0, -0.5)):
			check('Bsp_TexelToWorld' + fmt((bsppl, bsp_axis, bsp_u, bsp_v, bsp_s)), lambda pl=bsppl, bsp_axis=bsp_axis, bsp_u=bsp_u, bsp_v=bsp_v, bsp_s=bsp_s: SDTest.Call(
				'Bsp_TexelToWorld', 'vFffifF', (9.0, 9.0, 9.0), bsp_u, bsp_v, bsp_axis, bsp_s, pl)[1])
for bsp_axis in (0, 1, 2, 5):
	check('Bsp_WorldToTexel' + fmt((bsp_axis,)), lambda bsp_axis=bsp_axis: SDTest.Call('Bsp_WorldToTexel', 'vFFif', (9.0, 9.0), (1.5, -2.0,
		7.25), bsp_axis, 3.0)[1])
for bsp_i in (0, 1, 5, 12345, -1, 0x7fffffff):
	for bsp_n in (1, 7, 0, -1):
		check('Bsp_Hammersley' + fmt((bsp_i, bsp_n)), lambda bsp_i=bsp_i, bsp_n=bsp_n: SDTest.Call('Bsp_Hammersley', 'iFii', (9.0, 9.0), bsp_i, bsp_n)[1])
bspout = bspz(8)
for bsp_c in ((1.0, 0.5, 0.25), (0.0, 0.0, 0.0), (-1.0, -2.0, -3.0), (300.0, 2.0, 1.0), (nan, 1.0, 1.0), (1.0, nan, 0.0),
		(0.1, 0.9, 0.3), (2.0, 2.0, 2.0)):
	check('Bsp_ColorIntens' + fmt(bsp_c), lambda bsp_c=bsp_c: (SDTest.Call('Bsp_ColorIntens', 'viF', bspout, bsp_c), bspi(bspout),
		bspfl(bspout + 4, 1)))
for bsp_z in (0.0, 0.5, -1.0, 2.0):
	for bsp_u in (0.0, 0.25, 0.7, -3.5):
		check('Bsp_DirSphere' + fmt((bsp_z, bsp_u)), lambda bsp_z=bsp_z, bsp_u=bsp_u: SDTest.Call('Bsp_DirSphere', 'iFff', (9.0, 9.0, 9.0), bsp_z, bsp_u)[1])
for bsp_i in range(3):
	bspb1 = tuple(bsprnds(3, -50.0, 0.0) + bsprnds(3, 0.0, 50.0))
	bspb2 = tuple(bsprnds(3, -50.0, 0.0) + bsprnds(3, 0.0, 50.0))
	for bsp_axis in (0, 1, 2):
		for bsp_s in (1.0, 0.37, 8.0):
			check('Bsp_TexelBox' + fmt((bsp_i, bsp_axis, bsp_s)), lambda bsp_b=bspb1, bsp_axis=bsp_axis, bsp_s=bsp_s: SDTest.Call('Bsp_TexelBox', 'vFifpppp', bsp_b,
				bsp_axis, bsp_s, 0, 0, 0, 0)[2:])
			check('Bsp_SameSize' + fmt((bsp_i, bsp_axis, bsp_s)), lambda b1=bspb1, b2=bspb2, bsp_axis=bsp_axis, bsp_s=bsp_s: (SDTest.Call('Bsp_SameSize',
				'bFifF', b1, bsp_axis, bsp_s, b2)[0], SDTest.Call('Bsp_SameSize', 'bFifF', b1, bsp_axis, bsp_s, b1)[0]))
for bsp_i in range(6):
	bspff = (vec(), vec(), vec(), vec())
	check('Bsp_FormFactor' + fmt((bsp_i,)), lambda bsp_f=bspff: (SDTest.Call('Bsp_FormFactor', 'fFFFFf', bsp_f[0], bsp_f[1], bsp_f[2], bsp_f[3], 2.0)[0],
		SDTest.Call('Bsp_FormFactor', 'fFFFFf', bsp_f[0], bsp_f[1], bsp_f[0], bsp_f[3], 2.0)[0], SDTest.Call('Bsp_FormFactor', 'fFFFFf', bsp_f[0],
		(0.0, 0.0, 1.0), bsp_f[2], (0.0, 0.0, -1.0), 0.5)[0]))

# the areas of the level of detail
bspga = bspz(0x224)
check('bsp lod counters', lambda: tpeek(symva('g_BspLodCount'), 4))
for bsp_k in (0.1, 2.0, 50.0, nan):
	bspset(bspga + 0x178, (bsp_k,))
	for bsp_d in (0.0, 10.0, 100.0, 1000.0, 1e6, -1.0):
		for bsp_g in (0, 1):
			check('BspTri_GetAreas' + fmt((bsp_k, bsp_d, bsp_g)), lambda bsp_d=bsp_d, bsp_g=bsp_g: (SDTest.Call('BspTri_GetAreas', 'iifi', bspga, bsp_d, bsp_g)[0] -
				bspga, tpeek(symva('g_BspLodCount'), 4)))

# regions: form factor, the light through links
def bspreg(c, n, area, col):
	r = bspz(0x64)
	bspset(r + 0x18, (area,) + c + n + (0.0, 0.0, 0.0) + col)
	return r
bspr = []
for bsp_i in range(3):
	bspr.append(bspreg(vec(), vec(), rnd(0.5, 3.0), tuple(bsprnds(4, 0.0, 1.0))))
for bsp_i in range(3):
	for bsp_k in range(3):
		check('BspRegion_FormFactor' + fmt((bsp_i, bsp_k)), lambda bsp_a=bspr[bsp_i], bsp_b=bspr[bsp_k]: SDTest.Call('BspRegion_FormFactor', 'fii', bsp_a, bsp_b)[0])
bspl1 = bspz(12)
bspl2 = bspz(12)
bsppoke(bspl1 + 4, bspr[1])
bspset(bspl1 + 8, (0.5,))
bsppoke(bspl2 + 4, bspr[0])
bspset(bspl2 + 8, (2.0,))
bsppoke(bspr[0] + 0x60, bspl1)
bsppoke(bspr[2] + 0x60, bspl2)
bsppoke(bspr[0], bspr[1])
bsppoke(bspr[0] + 0xc, bspr[2])
check('BspRegion_Accumulate', lambda: (SDTest.Call('BspRegion_Accumulate', 'viF', bspr[0], (0.1, 0.2, 0.3, 1.0)),
	map(lambda bsp_r: bspfl(bsp_r + 0x50, 4), bspr)))

# the light maps of a list: texels summed, the average around one
bspst = bspz(0x224)
bspflt2(bspst + 0xb0, 4, 3, bsprnds(36, 0.0, 2.0))
SDTest.Call('CBmp_Create', 'tviii', bspst + 0x114, 4, 3)
bspmimg = bspi(bspst + 0x11c)
bspmv = bspcols(12)
bspmv[1] = 0
bspmv[6] = 0
bspmv[11] = 0
for bsp_i in range(12):
	bsppoke(bspmimg + 4 * bsp_i, bspmv[bsp_i])
for bsp_x, bsp_y in ((0, 0), (1, 0), (3, 2), (2, 1), (4, 0), (0, 3), (-1, 1)):
	check('BspTri_SumTexel' + fmt((bsp_x, bsp_y)), lambda bsp_x=bsp_x, bsp_y=bsp_y: SDTest.Call('BspTri_SumTexel', 'viiiFp', bspst, bsp_x, bsp_y,
		(0.5, 0.25, 0.125, 1.0), 2)[1:])
	check('BspTri_Average' + fmt((bsp_x, bsp_y)), lambda bsp_x=bsp_x, bsp_y=bsp_y: SDTest.Call('BspTri_Average', 'iFiii', (9.0,) * 4, bspst, bsp_x, bsp_y)[1])
check('BspTri_Average far', lambda: SDTest.Call('BspTri_Average', 'iFiii', (9.0,) * 4, bspst, 50, 50)[1])
for bsp_x0, bsp_y0, bsp_x, bsp_y, bsp_c in ((0, 0, 0, 0, 0), (0, 0, 2, 0, 0), (1, 1, 2, 2, 0), (0, 0, 1, 0, 0), (0, 0, 100000, 0, 0),
		(5, 0, 4, 0, 0)):
	if bsp_x - bsp_x0 >= 0 and bsp_x - bsp_x0 < 4 and bsp_y - bsp_y0 >= 0 and bsp_y - bsp_y0 < 3:
		bsp_p = bspmv[(bsp_y - bsp_y0) * 4 + bsp_x - bsp_x0]
		bsp_c = (bsp_p & -0x1000000) | ((bsp_p & 0xff) << 16) | (bsp_p & 0xff00) | ((bsp_p >> 16) & 0xff)
	check('BspTri_IsTexel' + fmt((bsp_x0, bsp_y0, bsp_x, bsp_y)), lambda bsp_x0=bsp_x0, bsp_y0=bsp_y0, bsp_x=bsp_x, bsp_y=bsp_y, bsp_c=bsp_c: (SDTest.Call('BspTri_IsTexel', 'biiiiii', bsp_x0,
		bsp_y0, bsp_x, bsp_y, bsp_c, bspst)[0], SDTest.Call('BspTri_IsTexel', 'biiiiii', bsp_x0, bsp_y0, bsp_x, bsp_y, bsp_c + 1, bspst)[0]))

# the extents of a light map
bspxt = bspz(0x224)
for bsp_e in ((0, 0, 10, 20), (5, 5, 4, 4), (-3, 2, 1000, 1), (0, 0, bsptexx - 1, bsptexy - 1), (0, 0, bsptexx, 0)):
	check('BspTri_SetExt' + fmt(bsp_e), lambda bsp_e=bsp_e: (SDTest.Call('BspTri_SetExt', 'viiiiii', bspxt, bsp_e[0], bsp_e[1], bsp_e[2], bsp_e[3], 0),
		tpeek(bspxt + 0xd4, 6)))
	bsppoke(bspxt + 0xec, 7)
	bsppoke(bspxt + 0xf8, -9)
	check('BspTri_GetExt' + fmt(bsp_e), lambda: (SDTest.Call('BspTri_GetExt', 'vipppp', bspxt, 0, 0, 0, 0)[1:],
		SDTest.Call('BspTri_GetExt2', 'vipppp', bspxt, 0, 0, 0, 0)[1:]))

# a vertex shared with another list
bspva = bspz(0x224)
bspvb = bspz(0x224)
bspset(bspva + 4, bspflat(((1.0, 2.0, 3.0), (4.0, 5.0, 6.0), (7.0, 8.0, 9.0))))
bspset(bspva + 0x28, bspflat(((0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0))))
for bsp_i, bsp_v in ((0, 1), (1, 2), (2, 3)):
	bsppoke(bspva + 0x88 + 4 * bsp_i, bsp_v)
bspset(bspvb + 4, bspflat(((7.0, 8.0, 9.0), (1.0, 2.0, 3.0), (0.0, 0.0, 0.0))))
bspset(bspvb + 0x28, bspflat(((1.0, 0.00005, 0.0), (0.0, 0.0, 0.9), (0.0, 0.0, 0.0))))
for bsp_i, bsp_v in ((0, 9), (1, 8), (2, 7)):
	bsppoke(bspvb + 0x88 + 4 * bsp_i, bsp_v)
for bsp_nv in (0, 1, 2):
	check('BspTri_VertEqual' + fmt((bsp_nv,)), lambda bsp_nv=bsp_nv: (SDTest.Call('BspTri_VertEqual', 'biii', bspva, bsp_nv, bspvb)[0],
		SDTest.Call('BspTri_VertEqual', 'biii', bspvb, bsp_nv, bspva)[0]))
# the same id needs the same position
bsppoke(bspvb + 0x88 + 8, 2)
bspset(bspvb + 4 + 0x18, (4.0, 5.0, 6.0))
bspset(bspvb + 0x28 + 0x18, (0.0, 1.0, 0.0))
check('BspTri_VertEqual id', lambda: SDTest.Call('BspTri_VertEqual', 'biii', bspva, 1, bspvb)[0])

# samples of a texel: the center, the corners, 7 x 7 inside the list
for bsp_i in range(4):
	bspt = bsptris[bsp_i]
	bsp_axis = bspi(bspt + 0xa4)
	bsp_c = []
	for bsp_k in range(3):
		bsp_c.append((bsptv[bsp_i][0][bsp_k] + bsptv[bsp_i][1][bsp_k] + bsptv[bsp_i][2][bsp_k]) / 3.0)
	bsp_c.remove(bsp_c[bsp_axis])
	for bsp_s in (1.0, 0.5):
		bsp_x0 = int(bsp_c[0] * bsp_s)
		bsp_y0 = int(bsp_c[1] * bsp_s)
		for bsp_x, bsp_y in ((bsp_x0, bsp_y0), (bsp_x0 - 1, bsp_y0), (bsp_x0 + 3, bsp_y0 - 2), (bsp_x0 + 50, bsp_y0)):
			check('BspTri_Sample' + fmt((bsp_i, bsp_s, bsp_x - bsp_x0, bsp_y - bsp_y0)), lambda bsp_t=bspt, bsp_x=bsp_x, bsp_y=bsp_y, bsp_axis=bsp_axis, bsp_s=bsp_s: (SDTest.Call(
				'BspTri_Sample', 'iFiiifiii', (9.0, 9.0, 9.0), bsp_x, bsp_y, bsp_axis, bsp_s, bsp_t, bsptris[0], 0)[1], SDTest.Call('BspTri_Sample',
				'iFiiifiiF', (9.0, 9.0, 9.0), bsp_x, bsp_y, bsp_axis, bsp_s, bsp_t, bsptris[0], (9.0,))[1:]))
# a texture sampled at the texels
bsptex = bspbmp(4, 4, bspcols(16))
for bsp_i in (0, 3):
	bspt = bsptris[bsp_i]
	bsp_axis = bspi(bspt + 0xa4)
	for bsp_u, bsp_v in ((0.5, 0.5), (2.25, -3.0), (17.0, 4.5)):
		check('Bsp_SampleTex' + fmt((bsp_i, bsp_u, bsp_v)), lambda bsp_t=bspt, bsp_u=bsp_u, bsp_v=bsp_v, bsp_axis=bsp_axis: SDTest.Call('Bsp_SampleTex', 'iFffifii',
			(9.0, 9.0, 9.0), bsp_u, bsp_v, bsp_axis, 1.0, bsp_t, bsptex)[1])
	for bsp_n in (0, 1, 2, -1):
		check('Bsp_SampleTexAA' + fmt((bsp_i, bsp_n)), lambda bsp_t=bspt, bsp_n=bsp_n, bsp_axis=bsp_axis: SDTest.Call('Bsp_SampleTexAA', 'iFiiifiii',
			(9.0, 9.0, 9.0), 3, -2, bsp_axis, 0.5, bsp_t, bsptex, bsp_n)[1])

# images: created filled, copied, turned, freed
bspb = bspz(12)
check('CBmp_CreateFill', lambda: (SDTest.Call('CBmp_CreateFill', 'tviiii', bspb, 3, 2, -0x10000), bspimg(bspb, 1)))
check('CBmp_CreateFill again', lambda: (SDTest.Call('CBmp_CreateFill', 'tviiii', bspb, 2, 2, 0x100020), bspimg(bspb, 1)))
bspb2 = bspz(12)
check('CBmpFlt2_CreateFill', lambda: (SDTest.Call('CBmpFlt2_CreateFill', 'tviiiF', bspb2, 2, 3, (0.5, -1.0, 2.0)), bspimg(bspb2, 3)))
bspb4 = bspz(12)
check('CBmpFlt_CreateFill', lambda: (SDTest.Call('CBmpFlt_CreateFill', 'tviiiF', bspb4, 3, 1, (0.5, -1.0, 2.0, 4.0)),
	bspimg(bspb4, 4)))
bspsrc = bspbmp(3, 2, bspcols(6))
check('CBmp_Assign', lambda: (SDTest.Call('CBmp_Assign', 'tvii', bspb, bspsrc), bspimg(bspb, 1)))
check('CBmp_Rotate', lambda: (SDTest.Call('CBmp_Rotate', 'tvii', bspb, bspsrc), bspimg(bspb, 1)))
bspsrc2 = bspz(12)
bspflt2(bspsrc2, 3, 2, bsprnds(18, -1.0, 1.0))
check('CBmpFlt2_Assign', lambda: (SDTest.Call('CBmpFlt2_Assign', 'tvii', bspb2, bspsrc2), bspimg(bspb2, 3)))
check('CBmpFlt2_Rotate', lambda: (SDTest.Call('CBmpFlt2_Rotate', 'tvii', bspb2, bspsrc2), bspimg(bspb2, 3)))
check('CBmpFlt2_dtor', lambda: (SDTest.Call('CBmpFlt2_dtor', 'tvi', bspb2), bspimg(bspb2, 3)))
check('CBmpFlt_dtor', lambda: (SDTest.Call('CBmpFlt_dtor', 'tvi', bspb4), bspimg(bspb4, 4)))

# a list turned for the packing
bsprt = bspz(0x224)
bsppoke(bsprt + 0xd4, 2)
bsppoke(bsprt + 0xd8, 3)
bspflt2(bsprt + 0xb0, 2, 3, bsprnds(18, 0.0, 1.0))
SDTest.Call('CBmp_Create', 'tviii', bsprt + 0x114, 2, 3)
for bsp_i in range(6):
	bsppoke(bspi(bsprt + 0x11c) + 4 * bsp_i, 0x10101 * (bsp_i + 1))
bspset(bsprt + 0x64, (0.1, 0.2, 0.3, 0.4, 0.5, 0.6))
check('BspTri_Rotate', lambda: (SDTest.Call('BspTri_Rotate', 'vi', bsprt), tpeek(bsprt + 0xd4, 2), bspimg(bsprt + 0xb0, 3),
	bspimg(bsprt + 0x114, 1), bspfl(bsprt + 0x64, 6)))
check('BspTri_Rotate again', lambda: (SDTest.Call('BspTri_Rotate', 'vi', bsprt), tpeek(bsprt + 0xd4, 2)))

# the areas of a list: averaged over boxes, regions over them
def bspareas(t, recs):
	SDTest.Call('Table_Alloc18', 'tvii', t, len(recs))
	p = bspi(t + 4)
	for i in range(len(recs)):
		r = recs[i]
		bspset(p + 0x18 * i, r[0:4])
		bsppoke(p + 0x18 * i + 0x10, r[4] | (r[5] << 16))
		bspset(p + 0x18 * i + 0x14, r[6:7])
def bsparecs(t):
	n, p = bspi(t), bspi(t + 4)
	r = [n]
	for i in range(n):
		r.append((bspfl(p + 0x18 * i, 4), bspi(p + 0x18 * i + 0x10), bspfl(p + 0x18 * i + 0x14, 1)))
	return r
bspsrcs = []
for bsp_i in range(16):
	bspsrcs.append(tuple(bsprnds(3, -20.0, 20.0)) + (rnd(0.1, 1.0), bsp_i % 4, bsp_i / 4, rnd(0.0, 1.0)))
bspas = bspz(8)
bspareas(bspas, bspsrcs)
bspad = bspz(8)
for bsp_w, bsp_h, bsp_d in ((4, 4, 2), (4, 4, 4), (3, 2, 2), (8, 8, 8), (4, 4, 1)):
	check('BspTri_AreasLod' + fmt((bsp_w, bsp_h, bsp_d)), lambda bsp_w=bsp_w, bsp_h=bsp_h, bsp_d=bsp_d: (SDTest.Call('BspTri_AreasLod', 'viiiii', bspad, bspas, bsp_w, bsp_h,
		bsp_d), bsparecs(bspad)))
check('BspTri_AreasLod empty', lambda: (SDTest.Call('BspTri_AreasLod', 'viiiii', bspad, bspz(8), 4, 4, 2), tpeek(bspad, 2)))
bspres = bspz(8)
check('Table_Resize2C', lambda: (SDTest.Call('Table_Resize2C', 'tvii', bspres, 3), bspi(bspres)))
bsppoke(bspi(bspres + 4), 77)
bsppoke(bspi(bspres + 4) + 0x2c * 2, 88)
check('Table_Resize2C grow', lambda: (SDTest.Call('Table_Resize2C', 'tvii', bspres, 5), bspi(bspres), bspi(bspi(bspres + 4)),
	bspi(bspi(bspres + 4) + 0x58)))
check('Table_Resize2C free', lambda: (SDTest.Call('Table_Resize2C', 'tvii', bspres, 0), tpeek(bspres, 2)))
check('Table_Resize14 copy', lambda: (SDTest.Call('Table_Resize14@296390', 'tvii', bspres, 2), bspi(bspres), SDTest.Call('Table_Resize14@296390',
	'tvii', bspres, 4), bspi(bspres), SDTest.Call('Table_Resize14@296390', 'tvii', bspres, -1), tpeek(bspres, 2)))
check('Table_Free copies', lambda: (SDTest.Call('Table_Alloc18', 'tvii', bspres, 2), SDTest.Call('Table_Free@281f70', 'tvi', bspres),
	tpeek(bspres, 2), SDTest.Call('Table_Alloc18', 'tvii', bspres, 2), SDTest.Call('Table_Free@281fa0', 'tvi', bspres), tpeek(bspres, 2),
	SDTest.Call('Table_Alloc18', 'tvii', bspres, 2), SDTest.Call('Table_Free@282060', 'tvi', bspres), tpeek(bspres, 2)))
def bspregf(r):
	return (bspi(r + 0x14) & 0xff, bspfl(r + 0x18, 4), bspi(r + 0x38), bspi(r + 0x3c))
check('BspRegion_New none', lambda: (SDTest.Call('BspRegion_New', 'iiiiiii', 0, 0, 0, 4, 0, bspas)[0],
	SDTest.Call('BspRegion_New', 'iiiiiii', 5, 5, 2, 2, 0, bspas)[0]))
bsprg = SDTest.Call('BspRegion_New', 'iiiiiii', 0, 0, 4, 4, 0, bspas)[0]
check('BspRegion_New', lambda: (bsprg != 0, bspregf(bsprg)))
check('BspRegion_New part', lambda: bspregf(SDTest.Call('BspRegion_New', 'iiiiiii', 1, 2, 2, 2, 0, bspas)[0]))
def bspwalk(r):
	if not r:
		return None
	return (bspregf(r), bspwalk(bspi(r)), bspwalk(bspi(r + 4)), bspwalk(bspi(r + 8)), bspwalk(bspi(r + 0xc)))
check('BspRegion_Split', lambda: (SDTest.Call('BspRegion_Split', 'vii', bsprg, bspas), bspwalk(bsprg)))
# the color of the tree into the texels of the list of color 0x123456 (mask colors are R and B swapped)
bspmat = bspz(0xb0)
SDTest.Call('CMaterial_ctor', 'tii', bspmat)
bspct = bspz(0x224)
bsppoke(bspct + 0xac, bspmat)
bspflt2(bspct + 0xfc, 4, 4, bsprnds(48, 0.0, 1.0))
bspmk = []
for bsp_i in range(16):
	if bsp_i % 3:
		bspmk.append(0x563412)
	else:
		bspmk.append(0x123456)
SDTest.Call('CBmp_Create', 'tviii', bspct + 0x114, 4, 4)
for bsp_i in range(16):
	bsppoke(bspi(bspct + 0x11c) + 4 * bsp_i, bspmk[bsp_i])
SDTest.Call('BspRegion_Accumulate', 'viF', bsprg, (0.25, 0.5, 0.75, 1.0))
check('BspRegion_Color', lambda: (SDTest.Call('BspRegion_Color', 'iFiii', (9.0,) * 4, bsprg, bspct, 0x123456)[1],
	bspimg(bspct + 0xfc, 3), bspfl(bsprg + 0x40, 8)))
check('BspRegion_Color none', lambda: SDTest.Call('BspRegion_Color', 'iFiii', (9.0,) * 4, 0, bspct, 0x123456)[1])

# a page of light maps: a free place, a light map copied in, a list put in
bsppg = bspz(0x30)
check('BspLMap_Init', lambda: (SDTest.Call('BspLMap_Init', 'tviii', bsppg, 16, 12), tpeek(bsppg + 0x20, 2), tpeek(bsppg + 4, 2),
	bspi(bsppg + 0xc) != 0, bspi(bspi(bsppg + 0xc)), bspi(bsppg + 0x18) != 0))
bsppimg = bspi(bsppg + 0xc)
for bsp_x, bsp_y in ((0, 0), (1, 0), (5, 3), (6, 3), (5, 4), (15, 11)):
	bsppoke(bsppimg + 4 * (bsp_y * 16 + bsp_x), 0xffffff)
bspsaved = (SDTest.Peek('g_LightmapPackMode', 'i'), SDTest.Peek('g_LightmapDxtPadding', 'i'))
for bsp_mode in (0, 1, 2, 3):
	SDTest.Poke('g_LightmapPackMode', bsp_mode)
	for bsp_pad in (0, 4):
		SDTest.Poke('g_LightmapDxtPadding', bsp_pad)
		for bsp_w, bsp_h in ((3, 2), (2, 3), (16, 12), (17, 1), (1, 1), (5, 9), (12, 5)):
			check('BspLMap_Find' + fmt((bsp_mode, bsp_pad, bsp_w, bsp_h)), lambda bsp_w=bsp_w, bsp_h=bsp_h: SDTest.Call('BspLMap_Find', 'biiipp', bsppg + 4, bsp_w,
				bsp_h, 0, 0))
SDTest.Poke('g_LightmapPackMode', bspsaved[0])
SDTest.Poke('g_LightmapDxtPadding', bspsaved[1])
bspcl = bspz(0x224)
bspflt2(bspcl + 0xb0, 3, 2, (0.5, 0.25, 1.5, 2.0, 0.0, 0.1, -0.5, 3.0, 0.2, 0.9, 0.9, 0.9, 1.25, 1.0, 0.75, 0.0, 0.0,
	0.0))
bspflt2(bspcl + 0xbc, 3, 2, bsprnds(18, 0.0, 1.5))
SDTest.Call('CBmp_Create', 'tviii', bspcl + 0x114, 3, 2)
for bsp_i in range(6):
	bsppoke(bspi(bspcl + 0x11c) + 4 * bsp_i, (0, 0x10203, 0x40506, 0, 7, 0x1000000)[bsp_i])
bspset(bspcl + 0x64, (0.1, 0.2, 0.3, 0.4, 0.5, 0.6))
check('BspLMap_Copy', lambda: (SDTest.Call('BspLMap_Copy', 'viiiiiiiii', bsppg + 4, 9, 6, bspcl + 0xb0, bspcl + 0x114, 0, 0, 3,
	2), bspimg(bsppg + 4, 1)))
check('BspLMap_Copy part', lambda: (SDTest.Call('BspLMap_Copy', 'viiiiiiiii', bsppg + 4, 2, 7, bspcl + 0xb0, bspcl + 0x114, 1,
	1, 2, 1), bspimg(bsppg + 4, 1)))
# the secondary page has the size of the light maps: only with one large enough
if bsptexx >= 32 and bsptexy >= 32:
	check('BspLMap_Insert', lambda: (SDTest.Call('BspLMap_Insert', 'biiii', bspcl, bsppg, 3, 2)[0], bspi(bspcl + 0x1c0) == bsppg,
		bspfl(bspcl + 0x64, 6), bspi(bspcl + 0xb8), bspi(bspcl + 0xc4), tpeek(bsppg + 0x20, 2), bspi(bsppg + 0x2c) != 0))
	bsppg2 = bspi(bsppg + 0x2c)
	check('BspLMap_Insert page 2', lambda: (tpeek(bsppg2 + 4, 2) == [bsptexx, bsptexy], tpeek(bsppg2 + 0x20, 2) ==
		[bsptexx + 1, bsptexy + 1], tpeek(bspi(bsppg2 + 0xc), 8)))
else:
	bsppg2 = bspnew(0x30)
	for bsp_i in range(12):
		bsppoke(bsppg2 + 4 * bsp_i, 0)
	bsppoke(bsppg + 0x2c, bsppg2)
bspflt2(bspcl + 0xb0, 20, 20, (0.0,) * 1200)
check('BspLMap_Insert full', lambda: (SDTest.Call('BspLMap_Insert', 'biiii', bspcl, bsppg, 20, 20)[0], tpeek(bsppg + 0x20, 2),
	SDTest.Call('BspLMap_Insert', 'biiii', bspcl, bsppg, 25, 30)[0], SDTest.Call('BspLMap_Insert', 'biiii', bspcl, 0, 2, 2)[0]))
bspm1 = bspz(4)
bspm2 = bspz(4)
bsppoke(bsppg + 0x1c, bspm1)
bsppoke(bsppg2 + 0x1c, bspm2)
bspsaved = SDTest.Peek('g_R_ForceLMap', 'i')
for bsp_f in (-1, 0, 1):
	SDTest.Poke('g_R_ForceLMap', bsp_f)
	check('BspLMap_GetMat' + fmt((bsp_f,)), lambda: bspidx(SDTest.Call('BspLMap_GetMat', 'ii', bsppg)[0], (bspm1, bspm2)))
SDTest.Poke('g_R_ForceLMap', bspsaved)
bsppoke(bsppg + 0x1c, 0)
bsppoke(bsppg2 + 0x1c, 0)
check('BspLMap_dtor', lambda: (SDTest.Call('BspLMap_dtor', 'tvi', bsppg), tpeek(bsppg, 12)))

# the sectors: vertex buffers by format, the table of them
bspzn = bspz(0xb4)
check('BspZone_ctor', lambda: (SDTest.Call('BspZone_ctor', 'tii', bspzn)[0] == bspzn, tpeek(bspzn, 45)))
for bsp_f in (3, 7, 10, 5):
	check('BspZone_GetVB' + fmt((bsp_f,)), lambda bsp_f=bsp_f: SDTest.Call('BspZone_GetVB', 'tiii', bspzn, bsp_f)[0] - bspzn)
check('BspZone_dtor', lambda: (SDTest.Call('BspZone_dtor', 'tvi', bspzn), tpeek(bspzn, 45)))
check('BspZone_Delete', lambda: SDTest.Call('BspZone_Delete', 'tiii', bspzn, 0)[0] == bspzn)
check('BspVB_Unwind', lambda: (SDTest.Call('BspVB_Unwind', 'tvi', bspzn + 8), tpeek(bspzn + 8, 5)))
def bspzonas(t):
	n, p = bspi(t), bspi(t + 4)
	r = [n]
	for i in range(n):
		w = tpeek(p + 0xb4 * i, 45)
		r.append((w[0], w[2:]))
	return r
bsptz = bspz(8)
check('BspZone table', lambda: (SDTest.Call('Table_NewB4', 'tvii', bsptz, 3), bspzonas(bsptz)))
check('Table_FreeB4', lambda: (SDTest.Call('Table_FreeB4', 'tvi', bsptz), tpeek(bsptz, 2)))
SDTest.Call('Table_NewB4', 'tvii', bsptz, 2)
check('BspZone_Delete array', lambda bsp_p=bspi(bsptz + 4): SDTest.Call('BspZone_Delete', 'tiii', bsp_p, 3)[0] == bsp_p - 4)
bsppoke(bsptz, 0)
bsppoke(bsptz + 4, 0)

# a list built and destroyed with its parts
def bsplist():
	t = bspnew(0x224)
	SDTest.Call('BspTri_ctor', 'tii', t)
	return t
bsptc = bspz(0x224)
check('BspTri_ctor', lambda: (SDTest.Call('BspTri_ctor', 'tii', bsptc)[0] == bsptc, tpeek(bsptc, 0x89)))
bsppoke(bsptc, bsplist())
bsppoke(bsptc + 0x1d4, bspnew(6))
bspflt2(bsptc + 0xb0, 2, 2, (1.0,) * 12)
SDTest.Call('String_Set', 'tvis', bsptc + 0x138, 'luz_area')
SDTest.Call('Table_Alloc18', 'tvii', bsptc + 0x184, 3)
bspnb = bspnew(0x20)
bsppoke(bspnb, 0)
bsppoke(bspnb + 4, 0)
SDTest.Call('String_Set', 'tvis', bspnb + 4, 'name')
bsppoke(bsptc + 0x134, bspnb)
bsppoke(bsptc + 0x1bc, SDTest.Call('BspRegion_New', 'iiiiiii', 0, 0, 2, 2, 0, bspas)[0])
SDTest.Call('CBmp_Create', 'tviii', bsptc + 0x120, 2, 1)
check('BspTri_dtor', lambda: (SDTest.Call('BspTri_dtor', 'tvi', bsptc), tpeek(bsptc, 0x89)))
bspnb = bspnew(0x20)
bsppoke(bspnb, 0)
bsppoke(bspnb + 4, 0)
SDTest.Call('String_Set', 'tvis', bspnb + 4, 'otro')
check('BspName_dtor', lambda: (SDTest.Call('BspName_dtor', 'tvi', bspnb), bspi(bspnb + 4)))
bspnd = bspz(0x2c)
SDTest.Call('CVertList_ctor', 'tii', bspnd + 0x14)
bsppoke(bspnd + 0x10, bspnew(6))
bspnd2 = bspnew(0x2c)
bsppoke(bspnd2, 0)
bsppoke(bspnd2 + 0x10, 0)
bsppoke(bspnd2 + 0x28, 0)
SDTest.Call('CVertList_ctor', 'tii', bspnd2 + 0x14)
bsppoke(bspnd, bspnd2)
check('BspNode_dtor', lambda: (SDTest.Call('BspNode_dtor', 'tvi', bspnd), tpeek(bspnd, 11)))

# materials, transparent (no diffuse map) or not
def bspmaterial(transp, diffuse):
	m = bspz(0xb0)
	SDTest.Call('CMaterial_ctor', 'tii', m)
	bspbyte(m + 0x3a, transp)
	bsppoke(m + 0x14, diffuse)
	return m
bspmats = (bspmaterial(1, 0x40302010), bspmaterial(0, 0x11223344), bspmaterial(1, -0x7f7f7f80))
bspbyte(bspmats[2] + 0x3b, 1)
# the orders: pointer differences of equal materials are only big
def bspord(r):
	if r > -3 and r < 3:
		return r
	return 'big'
SDTest.Call('Bsp_MatInfo', 'vi', bspmats[0])
SDTest.Call('Bsp_MatInfo', 'vi', bspmats[1])
# comparisons of lists: material, page, sector
bspcmp = []
for bsp_i in range(5):
	bsp_t = bspz(0x224)
	bsppoke(bsp_t + 0xac, bspmats[(0, 1, 0, 2, 1)[bsp_i]])
	bsppoke(bsp_t + 0x1c8, (-1, 0, 2, 0, -1)[bsp_i])
	bspcmp.append(bsp_t)
bsppa = bspz(0x30)
bsppb = bspz(0x30)
bsppoke(bsppa + 0x28, 2)
bsppoke(bsppb + 0x28, 1)
bsppoke(bspcmp[1] + 0x1c0, bsppa)
bsppoke(bspcmp[3] + 0x1c0, bsppb)
bsppoke(bspcmp[4] + 0x1c0, bsppa)
bspqa = bspz(4)
bspqb = bspz(4)
for bsp_i in range(5):
	for bsp_k in range(5):
		bsppoke(bspqa, bspcmp[bsp_i])
		bsppoke(bspqb, bspcmp[bsp_k])
		check('BspTri_CmpZone' + fmt((bsp_i, bsp_k)), lambda: bspord(SDTest.Call('BspTri_CmpZone', 'iii', bspqa, bspqb)[0]))
		check('BspTri_CmpMat' + fmt((bsp_i, bsp_k)), lambda: bspord(SDTest.Call('BspTri_CmpMat', 'iii', bspqa, bspqb)[0]))
bspid1 = bspz(8)
bspid2 = bspz(8)
bsppoke(bspid1 + 4, 7)
bsppoke(bspid2 + 4, -3)
check('BspTri_CmpId', lambda: (SDTest.Call('BspTri_CmpId', 'iii', bspid1, bspid2)[0], SDTest.Call('BspTri_CmpId', 'iii', bspid2,
	bspid1)[0]))

# a map with lists: the sectors, the colors of the lists without light map, the area lights out, destroyed
bspmap = bspz(0x61aa8)
bspml = []
for bsp_i in range(4):
	bsp_t = bsplist()
	bsppoke(bsp_t + 0xac, bspmats[(0, 1, 2, 1)[bsp_i]])
	bspnd_ = bspz(0x100)
	bsppoke(bspnd_ + 0x18, (0, 0x20000080, 0, 0x20000080)[bsp_i])
	bsppoke(bsp_t + 0x1c4, bspnd_)
	bspbyte(bsp_t + 0x154, 1)
	for bsp_k in range(3):
		bsppoke(bsp_t + 0x7c + 4 * bsp_k, 0x111111 * (bsp_k + 1))
	bspml.append(bsp_t)
	bsppoke(bspmap + 8 + 4 * bsp_i, bsp_t)
bsppoke(bspml[0], bsplist())
bsppoke(bspi(bspml[0]) + 0xac, bspmats[0])
for bsp_k in range(3):
	bsppoke(bspi(bspml[0]) + 0x7c + 4 * bsp_k, 7)
bsppoke(bspmap + 0x4e208, 4)
check('CBsp_ColorSinLmap', lambda: (SDTest.Call('CBsp_ColorSinLmap', 'tvi', bspmap), map(lambda bsp_t: (bspi(bsp_t + 0xa8),
	tpeek(bsp_t + 0x7c, 3)), bspml), tpeek(bspi(bspml[0]) + 0x7c, 3)))
check('CBsp_CreateZones', lambda: (SDTest.Call('CBsp_CreateZones', 'tvi', bspmap), tpeek(bspmap + 0x61a98, 1),
	map(lambda bsp_i: bspidx(bspi(bspmap + 8 + 4 * bsp_i), bspml), range(4)), bspidx(bspi(bspi(bspmap + 0x61a9c)), bspml),
	map(lambda bsp_t: bspidx(bspi(bsp_t + 0x1cc), bspml), bspml)))
check('CBsp_RemoveLights', lambda: (SDTest.Call('CBsp_RemoveLights', 'tvi', bspmap), bspi(bspmap + 0x4e208),
	map(lambda bsp_i: bspidx(bspi(bspmap + 8 + 4 * bsp_i), bspml), range(4))))
SDTest.Call('String_Set', 'tvis', bspmap, 'Levels/FZ/FZ')
check('bsp globals', lambda: (SDTest.Peek('g_BspTriTmp', 'i') != 0, SDTest.Peek('g_BspLMaps', 'i') != 0))
if not SDTest.Peek('g_BspTriTmp', 'i') and not SDTest.Peek('g_BspLMaps', 'i'):
	check('CBsp_dtor', lambda: (SDTest.Call('CBsp_dtor', 'tvi', bspmap), tpeek(bspmap, 4), bspi(bspmap + 0x4e208),
		tpeek(bspmap + 0x61a98, 4)))

# the region trees of a map: the glow and the light of the areas, the light passes
bspgm = bspz(0x61aa8)
bspgl = bsplist()
bsppoke(bspgm + 8, bspgl)
bsppoke(bspgm + 0x4e208, 1)
bspflt2(bspgl + 0x108, 4, 4, bsprnds(48, 0.0, 3.0))
SDTest.Call('CBmp_Create', 'tviii', bspgl + 0x114, 4, 4)
bspareas(bspgl + 0x184, bspsrcs[0:7])
check('CBsp_AreasGlow', lambda: (SDTest.Call('CBsp_AreasGlow', 'tvi', bspgm), bsparecs(bspgl + 0x184), bsparecs(bspgl + 0x18c),
	bsparecs(bspgl + 0x194), bsparecs(bspgl + 0x19c)))
check('CBsp_AreasMax', lambda: (SDTest.Call('CBsp_AreasMax', 'tvi', bspgm), bspfl(bspgl + 0x158, 9), tpeek(bspgl + 0x17c, 2)))
bsppoke(bspct, 0)
bsppoke(bspct + 0x1bc, bsprg)
bsppoke(bspgm + 8, bspct)
check('CBsp_IllumRegions', lambda: (SDTest.Call('CBsp_IllumRegions', 'tvii', bspgm, 1), bspimg(bspct + 0xfc, 3),
	bspfl(bsprg + 0x40, 8)))
check('CBsp_IllumRegions 0', lambda: SDTest.Call('CBsp_IllumRegions', 'tvii', bspgm, 0))

# a node out of the map
bspno = bspz(0x100)
for bsp_refs, bsp_sig, bsp_ant in ((2, 1, 1), (3, 1, 1), (1, 0, 0), (1, 1, 0), (0, 0, 0)):
	bsppoke(bspno + 8, bsp_refs)
	bsppoke(bspno + 0xc, bsp_sig)
	bsppoke(bspno + 0x10, bsp_ant)
	bsppoke(bspno + 0x18, 0x100)
	check('Bsp_MarkNode' + fmt((bsp_refs, bsp_sig, bsp_ant)), lambda: (SDTest.Call('Bsp_MarkNode', 'vii', bspno, 0), bspi(bspno + 0x18)))
bsppoke(bspno + 0xc, 0)
bsppoke(bspno + 0x10, 0)
bsppoke(bspno + 0x18, 0x10000000)
check('BspTri_Zone', lambda: (SDTest.Call('BspTri_Zone', 'ii', 0)[0], SDTest.Call('BspTri_Zone', 'ii', bspno)[0]))
bsppoke(bspct + 0x1c8, -1)
check('BspTri_ZoneName', lambda: SDTest.Call('BspTri_ZoneName', 'si', bspct)[0])

# the TXF cache, off
bsptx = bspz(0x250)
SDTest.Call('crt_memcpy', 'iisi', bsptx + 8, 'Levels/FZ/_TXF/FZ.lmap.1', 25)
check('BspTxf_SetName', lambda: (SDTest.Call('BspTxf_SetName', 'tvii', bsptx, 4), SDTest.Peek(bsptx + 0x10c, '@s'),
	SDTest.Call('BspTxf_SetName', 'tvii', bsptx, -1), SDTest.Peek(bsptx + 0x10c, '@s')))
check('BspTxf_Check off', lambda: (SDTest.Call('BspTxf_Check', 'tbiiii', bsptx, 3, 7, 1)[0], SDTest.Call('BspTxf_Check', 'tbiiii',
	bsptx, 3, 0, 0)[0]))
check('BspTxf_Save off', lambda: (SDTest.Call('BspTxf_Save', 'tvii', bsptx, bspct + 0xfc), tpeek(bsptx + 0x230, 6)))
check('BspTxf_Load off', lambda: (SDTest.Call('BspTxf_Load', 'tviiii', bsptx, bspgm + 8, 1, 3), tpeek(bsptx + 0x230, 7)))
check('BspTxf_dtor', lambda: SDTest.Call('BspTxf_dtor', 'tvi', bsptx))

# the functions mundo.cpp calls, without the light maps (mapa_sombras usar 0)
bspusar = SDTest.Peek('g_ShadowMapUse', 'i')
check('bsp usar', lambda: bspusar)
if not bspusar:
	for bsp_rva, bsp_name in ((sym('Bsp_Free'), 'Bsp_Free'), (sym('Bsp_ReleaseD3D'), 'Bsp_ReleaseD3D'), (sym('Bsp_InitD3D'), 'Bsp_InitD3D'),
			(sym('Bsp_Render'), 'Bsp_Render'), (sym('Bsp_RenderTransp'), 'Bsp_RenderTransp'), (sym('Bsp_RenderPortals'), 'Bsp_RenderPortals')):
		check(bsp_name, lambda bsp_rva=bsp_rva: (SDTest.Call(bsp_rva, 'v'), SDTest.Peek('g_Bsp', 'i')))
	check('Bsp_Create', lambda: (SDTest.Call('Bsp_Create', 'v'), SDTest.Peek('g_Bsp', 'i')))

# the links of two regions: only the cases that end before the floor is traced
bspe1 = bspreg((0.0, 0.0, 0.0), (0.0, 0.0, 1.0), 1.0, (0.0, 0.0, 0.0, 1.0))
bspe2 = bspreg((0.0, 0.0, 5.0), (0.0, 0.0, -1.0), 1.0, (0.0, 0.0, 0.0, 1.0))
check('BspRegion_Link null', lambda: (SDTest.Call('BspRegion_Link', 'tiiiiif', bspgm, 0, bspe2, 0, 0.001)[0],
	SDTest.Call('BspRegion_Link', 'tiiiiif', bspgm, bspe1, 0, 0, 0.001)[0]))
check('BspRegion_Link alone', lambda: SDTest.Call('BspRegion_Link', 'tiiiiif', bspgm, bspe1, bspe2, 0, 0.001)[0])
bsppoke(bspe1 + 0x10, bspe2)
bsppoke(bspe2 + 0x10, bspe1)
check('BspRegion_Link dark', lambda: (SDTest.Call('BspRegion_Link', 'tiiiiif', bspgm, bspe1, bspe2, 0, 0.001)[0],
	bspfl(bspe1 + 0x50, 4), bspfl(bspe2 + 0x50, 4)))

# a map of one list through the radiosity steps: the areas of its texels, the regions, the light, the colors
bspone = bspz(0x61aa8)
bspol = bsplist()
bsppoke(bspone + 8, bspol)
bsppoke(bspone + 0x4e208, 1)
bspset(bspol + 4, (0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 3.0, 0.0))
bspset(bspol + 0x28, (0.0, 0.0, 1.0) * 3)
bspset(bspol + 0x64, (0.0, 0.0, 0.5, 0.0, 0.0, 0.5))
bsppoke(bspol + 0xac, bspmat)
bsppoke(bspol + 0xdc, 0)
bsppoke(bspol + 0xe0, 0)
bspbyte(bspol + 0x154, 0)
bsppoke(bspol + 0x7c, 0x10203040)
bsppoke(bspol + 0x80, -0x7f000000)
bsppoke(bspol + 0x84, 0x7f000000)
SDTest.Call('BspTri_CalcPlanes', 'vi', bspol)
SDTest.Call('CBmp_Create', 'tviii', bspol + 0x114, 4, 4)
bspsw = (bspol & -0x1000000) | ((bspol & 0xff) << 16) | (bspol & 0xff00) | ((bspol >> 16) & 0xff)
for bsp_i in range(16):
	if bsp_i % 5:
		bsppoke(bspi(bspol + 0x11c) + 4 * bsp_i, bspsw)
	else:
		bsppoke(bspi(bspol + 0x11c) + 4 * bsp_i, 0)
bspflt2(bspol + 0xb0, 4, 4, bsprnds(48, 0.0, 1.5))
bspflt2(bspol + 0xfc, 4, 4, bsprnds(48, 0.0, 1.0))
bspflt2(bspol + 0x108, 4, 4, bsprnds(48, 0.0, 1.0))
# the sample table has the size of a light map
if bsptexx * bsptexy >= 16:
	check('CBsp_InitAreas', lambda: (SDTest.Call('CBsp_InitAreas', 'tvi', bspone), bspi(bspol + 0x1a4),
		(bspi(bspol + 0x154) >> 16) & 0xff, bsparecs(bspol + 0x184), tpeek(bspol + 0x18c, 1), tpeek(bspol + 0x194, 1),
		tpeek(bspol + 0x19c, 1), tpeek(symva('g_BspAreasTotal'), 4), bspfl(bspol + 0x1a8, 5)))
	# IniciarRegiones on these areas: a CHECK (region->area > 0.0, the sqrt of a negative area)
check('CBsp_AdjustGamma', lambda: (SDTest.Call('CBsp_AdjustGamma', 'tvi', bspone), bspimg(bspol + 0xb0, 3), tpeek(bspol + 0x7c, 3)))
check('BspTri_CalcGlow none', lambda: (SDTest.Call('BspTri_CalcGlow', 'vi', bspol), bspi(bspol + 0xd0)))
# the texels of no list filled from their neighbours
bspfi = bspz(12)
SDTest.Call('CBmpFlt_Create', 'tviii', bspfi, 5, 4)
bspset(bspi(bspfi + 8), bsprnds(80, -1.0, 2.0))
bspfm = bspbmp(5, 4, (1, 0, 0, 7, 0, 0, 0, 0, 0, 0, 9, 0, 3, 0, 5, 0, 0, 0, 0, 2))
check('CBmpFlt_FillGaps', lambda: (SDTest.Call('CBmpFlt_FillGaps', 'vii', bspfi, bspfm), bspimg(bspfi, 4)))
check('CBmpFlt_FillGaps none', lambda: SDTest.Call('CBmpFlt_FillGaps', 'vii', bspz(12), bspfm))
