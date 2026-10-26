# engine3d Engine/MallaTri.c (execfile'd at the end of sdcall.py, Python 1.5.2): triangle meshes in heap memory.
# The arrays are printed by content, never by address; materials are small numbers (Draw, the only user, is not
# called: it always fails a CHECK). The CListaVert and CMallaD3D inputs are fake records with format VERT_TEX1 and
# others.
def mtf(addr, n):
	r = ()
	i = 0
	while i < n:
		k = min(64, n - i)
		r = r + SDTest.Call(0x3197d0, 'iFii', (0.0,) * k, addr + 4 * i, 4 * k)[1]
		i = i + k
	return r
def mtset(addr, vals):
	SDTest.Call(0x3197d0, 'iiFi', addr, tuple(vals), 4 * len(vals))
def mtget(m):
	w = tpeek(m, 13)
	r = (w[0], w[1], w[2:8], w[8] != 0, w[9] != 0, w[10] != 0, w[11] != 0, w[12] != 0, mtf(m + 0x34, 6))
	if w[8]:
		r = r + (mtf(w[8], 3 * w[2]),)
	if w[9]:
		r = r + (mtf(w[9], 2 * w[3]),)
	if w[10]:
		for i in range(w[4]):
			t = w[10] + 0x44 * i
			r = r + ((tpeek(t, 6), mtf(t + 0x18, 9), tpeek(t + 0x3c, 2)),)
	return r
def mtnew():
	m = SDTest.Call(0x1334d0, 'ii', 0x4c)[0]
	SDTest.Call(0x2626d0, 'tii', m)
	return m
# only the counts and which arrays exist, while new array items are not yet set (new[] leaves them as they were)
def mthead(m):
	w = tpeek(m, 13)
	return (w[1], w[2:8], w[8] != 0, w[9] != 0, w[10] != 0, w[11] != 0, w[12] != 0)
# heap addresses differ between the runs: not in the names
def mtfmt(args):
	a = []
	for x in args:
		if type(x) == type(0) and x >= 0x1000000:
			x = 'ptr'
		a.append(x)
	return fmt(tuple(a))
def mtstep0(name, m, rva, sig, *args):
	check(name + mtfmt(args), lambda m=m, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, m) + args), mthead(m)))
def mtstep(name, m, rva, sig, *args):
	check(name + mtfmt(args), lambda m=m, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, m) + args), mtget(m)))
# for functions that return a pointer: whether it is the expected one
def mtptr(name, m, rva, sig, want, *args):
	check(name + mtfmt(args), lambda m=m, rva=rva, sig=sig, want=want, args=args: (apply(SDTest.Call, (rva, sig, m) + args)[0] == want, mtget(m)))
def mtfill(m, nv, ntv, nt):
	for i in range(nv):
		SDTest.Call(0x264270, 'tviiF', m, i, vec())
	for i in range(ntv):
		SDTest.Call(0x264200, 'tviiF', m, i, (rnd(0.0, 1.0), rnd(0.0, 1.0)))
	for i in range(nt):
		SDTest.Call(0x264180, 'tviiiii', m, i, i % nv, (i + 1) % nv, (i + 2) % nv)
		if ntv:
			SDTest.Call(0x264100, 'tviiiii', m, i, (i + 2) % ntv, i % ntv, (i + 1) % ntv)
		SDTest.Call(0x264060, 'tviiFFF', m, i, vec(), vec(), vec())
		SDTest.Call(0x263ff0, 'tviii', m, i, 0x100 + i)
		SDTest.Call(0x263f80, 'tviii', m, i, i % 4)

m = SDTest.Call(0x1334d0, 'ii', 0x4c)[0]
check('CMallaTri_ctor', lambda m=m: (SDTest.Call(0x2626d0, 'tii', m)[0] == m, mtget(m)))
mtstep0('CMallaTri_SetNumVert', m, 0x263e40, 'tviii', 3, 0)
for i in range(3):
	mtstep0('CMallaTri_SetVert', m, 0x264270, 'tviiF', i, vec())
check('CMallaTri_SetVert all', lambda m=m: mtget(m))
mtstep0('CMallaTri_SetNumVert grow', m, 0x263e40, 'tviii', 5, 1)
check('CMallaTri_SetNumVert grow copy', lambda m=m: mtf(SDTest.Peek(m + 0x20, '@i'), 9))
mtstep0('CMallaTri_SetNumVert less', m, 0x263e40, 'tviii', 4, 1)
mtstep0('CMallaTri_SetNumVert grow 2', m, 0x263e40, 'tviii', 11, 1)
check('CMallaTri_SetNumVert grow 2 copy', lambda m=m: mtf(SDTest.Peek(m + 0x20, '@i'), 9))
mtstep0('CMallaTri_SetNumTVert', m, 0x263c00, 'tviii', 2, 0)
mtstep0('CMallaTri_SetNumTVert grow', m, 0x263c00, 'tviii', 6, 1)
mtstep0('CMallaTri_SetNumTri', m, 0x263cf0, 'tviii', 2, 0)
check('CMallaTri_SetNumTri ctor', lambda m=m: (tpeek(SDTest.Peek(m + 0x28, '@i') + 0x3c, 2), tpeek(SDTest.Peek(m + 0x28, '@i') + 0x80, 2)))
mtstep0('CMallaTri_SetNumTri grow', m, 0x263cf0, 'tviii', 3, 1)
check('CMallaTri_SetNumTri grow ctor', lambda m=m: tpeek(SDTest.Peek(m + 0x28, '@i') + 0x44 * 5 + 0x3c, 2))
mtfill(m, 11, 6, 3)
check('CMallaTri filled', lambda m=m: mtget(m))
mtstep('CMallaTri_SetTriVerts', m, 0x264180, 'tviiiii', 1, 4, 5, 6)
mtstep('CMallaTri_SetTriTVerts', m, 0x264100, 'tviiiii', 2, 5, 4, 3)
mtstep('CMallaTri_SetTriNormals', m, 0x264060, 'tviiFFF', 0, (0.0, 0.0, 0.0), (1e-30, 0.0, 0.0), (nan, 1.0, 0.0))
mtstep('CMallaTri_SetTriMat', m, 0x263ff0, 'tviii', 2, 0x77)
mtstep('CMallaTri_SetTriFlags', m, 0x263f80, 'tviii', 1, 0x55)
mtstep('CMallaTri_SetTVert', m, 0x264200, 'tviiF', 5, (0.25, -3.5))
for i in range(3):
	check('CMallaTri_GetTri' + fmt((i,)), lambda m=m, i=i: SDTest.Call(0x2643c0, 'tiii', m, i)[0] - SDTest.Peek(m + 0x28, '@i'))
	for k in range(3):
		check('CMallaTri_GetTriVert' + fmt((i, k)), lambda m=m, i=i, k=k: SDTest.Call(0x263980, 'tviFii', m, (9.0,) * 8, i, k)[1])
		check('CMallaTri_GetTriVert2' + fmt((i, k)), lambda m=m, i=i, k=k: SDTest.Call(0x263aa0, 'tviFii', m, (9.0,) * 8, i, k)[1])
for i in (0, 3, 10):
	check('CMallaTri_GetVert' + fmt((i,)), lambda m=m, i=i: SDTest.Call(0x264420, 'tiiFi', m, (9.0,) * 3, i)[1])
mtstep0('CMallaTri_AddVert', m, 0x2637b0, 'tii')
mtstep0('CMallaTri_AddTVert', m, 0x263780, 'tii')
mtstep0('CMallaTri_AddTri', m, 0x2634e0, 'tii')
mtstep0('CMallaTri_SetTriVerts new', m, 0x264180, 'tviiiii', 3, 11, 0, 2)
mtstep0('CMallaTri_SetTriTVerts new', m, 0x264100, 'tviiiii', 3, 6, 6, 0)
mtstep0('CMallaTri_SetTriNormals new', m, 0x264060, 'tviiFFF', 3, vec(), vec(), (0.0, -2.0, 0.0))
mtstep0('CMallaTri_SetTVert new', m, 0x264200, 'tviiF', 6, (0.5, 0.75))
mtstep('CMallaTri_SetVert new', m, 0x264270, 'tviiF', 11, vec())
for first in (0, 1):
	box = vec() + vec()
	check('CMallaTri_CalcBox' + fmt((box, first)), lambda m=m, box=box, first=first: (SDTest.Call(0x263880, 'tviFi', m, box, first)[1], mtget(m)))
	check('CMallaTri_GetBox' + fmt((box, first)), lambda m=m, box=box, first=first: SDTest.Call(0x2627f0, 'tviFi', m, box, first)[1])
mtstep('CMallaTri_Transform', m, 0x264720, 'tviF', affine())
mtstep('CMallaTri_Transform any', m, 0x264720, 'tviF', mat())
mtstep('CMallaTri_RemoveTri', m, 0x2637e0, 'tvii', 1)
mtstep('CMallaTri_RemoveTri last', m, 0x2637e0, 'tvii', 2)
for rva in (0x262830, 0x262be0):
	check('CMallaTri_NoFile' + fmt((rva,)), lambda m=m, rva=rva: SDTest.Call(rva, 'tbiii', m, 0, 0))

# texture vertex 0 for triangles without them; 0 texture vertices free them
m2 = mtnew()
mtstep('CMallaTri_CreateTVert empty', m2, 0x263460, 'tvi')
SDTest.Call(0x263e40, 'tviii', m2, 4, 0)
SDTest.Call(0x263cf0, 'tviii', m2, 2, 0)
mtfill(m2, 4, 0, 2)
mtstep('CMallaTri_CreateTVert', m2, 0x263460, 'tvi')
mtstep('CMallaTri_CreateTVert again', m2, 0x263460, 'tvi')
mtstep('CMallaTri_SetNumTVert 0', m2, 0x263c00, 'tviii', 0, 1)
mtstep('CMallaTri_SetNumTVert 0 again', m2, 0x263c00, 'tviii', 0, 1)

def mtvert():
	return vec() + vec() + (rnd(0.0, 1.0), rnd(0.0, 1.0))
m3 = mtnew()
for i in range(3):
	mtstep('CMallaTri_AddTriVerts', m3, 0x263510, 'tviFFFiii', mtvert(), mtvert(), mtvert(), 0x200 + i, 0, i + 1)
m4 = mtnew()
for i in range(3):
	mtstep('CMallaTri_AddTriVerts tvert', m4, 0x263510, 'tviFFFiii', mtvert(), mtvert(), mtvert(), 0x300 + i, 1, 7 * i)

# merges: all triangles, filtered by flags, without texture vertices, with skins
m5 = mtnew()
mtstep('CMallaTri_Add empty', m5, 0x2630c0, 'tviiii', m2, 0, 0)
mtstep('CMallaTri_Add', m5, 0x2630c0, 'tviiii', m, 0, 0)
mtstep('CMallaTri_Add mask', m5, 0x2630c0, 'tviiii', m3, 1, 1)
mtstep('CMallaTri_Add mask 0', m5, 0x2630c0, 'tviiii', m4, 2, 0)
mtstep('CMallaTri_Add want 2', m5, 0x2630c0, 'tviiii', m4, 0, 2)
m6 = mtnew()
mtstep('CMallaTri_Add no tvert', m6, 0x2630c0, 'tviiii', m3, 0, 0)
mtstep('CMallaTri_Add tvert', m6, 0x2630c0, 'tviiii', m4, 0, 1)
check('CMallaTri_AllocSkin', lambda m=m: SDTest.Call(0x263f10, 'tii', m)[0] == SDTest.Peek(m + 0x2c, '@i'))
nv = SDTest.Peek(m + 8, '@i')
skin = SDTest.Peek(m + 0x2c, '@i')
for i in range(nv):
	mtset(skin + 16 * i, (i, 1.0 / (i + 1), -i, 0.5))
	check('CMallaTri_GetSkin' + fmt((i,)), lambda m=m, i=i, skin=skin: SDTest.Call(0x2646c0, 'tiii', m, i)[0] - skin)
m7 = mtnew()
mtstep('CMallaTri_Add skin', m7, 0x2630c0, 'tviiii', m, 1, 0)
check('CMallaTri_Add skin data', lambda m7=m7: mtf(SDTest.Peek(m7 + 0x2c, '@i'), 4 * SDTest.Peek(m7 + 8, '@i')))
mtstep('CMallaTri_Add skin 2', m7, 0x2630c0, 'tviiii', m, 3, 1)
check('CMallaTri_Add skin data 2', lambda m7=m7: mtf(SDTest.Peek(m7 + 0x2c, '@i'), 4 * SDTest.Peek(m7 + 8, '@i')))
m8 = SDTest.Call(0x1334d0, 'ii', 0x4c)[0]
check('CMallaTri_ctorCopy', lambda m8=m8, m=m: (SDTest.Call(0x262510, 'tiii', m8, m)[0] == m8, mtget(m8)))

# CListaVert: 4 vertices of 0x34 bytes (the largest format) read in each format
lv = SDTest.Call(0x133650, 'ii', 16)[0]
vb = SDTest.Call(0x133650, 'ii', 0x34 * 4)[0]
vals = []
for i in range(0x34):
	vals.append(rnd())
mtset(vb, vals)
SDTest.Poke(lv - 0x400000, vb)
SDTest.Poke(lv + 4 - 0x400000, 4)
SDTest.Poke(lv + 8 - 0x400000, 4)
for f in (1, 2, 3, 4, 6, 7, 10):
	SDTest.Poke(lv + 12 - 0x400000, f)
	for i in range(4):
		if f in (1, 2, 4, 7):
			check('CListaVert_GetNormal' + fmt((f, i)), lambda lv=lv, i=i: SDTest.Call(0x264490, 'tiiFi', lv, (9.0,) * 3, i)[1])
		check('CListaVert_GetUV' + fmt((f, i)), lambda lv=lv, i=i: SDTest.Call(0x264580, 'tiiFi', lv, (9.0,) * 2, i)[1])

# CMallaD3D: a list of two meshes over VERT_TEX1 vertices, the first with triangle flags
def d3dnew(nxt, tris, nverts, flags, matv):
	d = SDTest.Call(0x133650, 'ii', 0x50)[0]
	idx = SDTest.Call(0x133650, 'ii', 6 * len(tris) + 4)[0]
	shorts = []
	for t in tris:
		shorts = shorts + list(t)
	if len(shorts) % 2:
		shorts.append(0)
	for i in range(0, len(shorts), 2):
		SDTest.Poke(idx + 2 * i - 0x400000, shorts[i] + (shorts[i + 1] << 16))
	v = SDTest.Call(0x133650, 'ii', 0x20 * nverts)[0]
	vals = []
	for i in range(8 * nverts):
		vals.append(rnd())
	mtset(v, vals)
	SDTest.Poke(d + 8 - 0x400000, nxt)
	SDTest.Poke(d + 0xc - 0x400000, len(tris))
	SDTest.Poke(d + 0x10 - 0x400000, idx)
	SDTest.Poke(d + 0x14 - 0x400000, v)
	SDTest.Poke(d + 0x18 - 0x400000, nverts)
	SDTest.Poke(d + 0x1c - 0x400000, nverts)
	SDTest.Poke(d + 0x20 - 0x400000, 1)
	if flags:
		fl = SDTest.Call(0x133650, 'ii', 4 * len(flags))[0]
		for i in range(len(flags)):
			SDTest.Poke(fl + 4 * i - 0x400000, flags[i])
		SDTest.Poke(d + 0x48 - 0x400000, fl)
	SDTest.Poke(d + 0x4c - 0x400000, matv)
	return d
d2 = d3dnew(0, ((1, 0, 2),), 3, (), 0x88)
d1 = d3dnew(d2, ((0, 1, 2), (2, 3, 0)), 4, (5, 6), 0x77)
m9 = mtnew()
mtstep('CMallaTri_AddD3D', m9, 0x262c00, 'tviii', d1, 0)
mtstep('CMallaTri_AddD3D all', m9, 0x262c00, 'tviii', d1, 1)
mtstep('CMallaTri_AddD3D second', m9, 0x262c00, 'tviii', d2, 1)
m10 = SDTest.Call(0x1334d0, 'ii', 0x4c)[0]
check('CMallaTri_ctorD3D', lambda m10=m10, d1=d1: (SDTest.Call(0x2625f0, 'tiiii', m10, d1, 1)[0] == m10, mtget(m10)))
mtstep('CMallaTri_Add d3d', m10, 0x2630c0, 'tviiii', m9, 4, 1)

# teardown: Clear, dtor and Delete; the rest freed with Delete
mtstep('CMallaTri_Clear', m9, 0x2642e0, 'tvi')
mtstep('CMallaTri_Clear again', m9, 0x2642e0, 'tvi')
mtstep('CMallaTri_Init', m10, 0x263ba0, 'tvi')
check('CMallaTri_dtor', lambda m8=m8: (SDTest.Call(0x262790, 'tvi', m8), mtget(m8)))
check('CMallaTri_Delete 0', lambda m=m: (SDTest.Call(0x2627b0, 'tiii', m, 0)[0] == m, mtget(m)))
check('CTriMalla_ctor', lambda lv=lv: (SDTest.Call(0x262770, 'tii', lv - 0x3c)[0] == lv - 0x3c, tpeek(lv, 4)))
for x in (m, m2, m3, m4, m5, m6, m7, m8, m9, m10):
	SDTest.Call(0x2627b0, 'tiii', x, 1)
SDTest.Call(0x133550, 'vi', vb)
SDTest.Call(0x133550, 'vi', lv)
