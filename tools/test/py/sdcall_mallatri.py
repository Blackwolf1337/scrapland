# engine3d Engine/MallaTri.c (execfile'd at the end of sdcall.py, Python 1.5.2): triangle meshes in heap memory.
# The arrays are printed by content, never by address; materials are small numbers (Draw, the only user, is not
# called: it always fails a CHECK). The CVertList and CMeshD3D inputs are fake records with format VERT_TEX1 and
# others.
def mtf(addr, n):
	r = ()
	i = 0
	while i < n:
		k = min(64, n - i)
		r = r + SDTest.Call('crt_memcpy', 'iFii', (0.0,) * k, addr + 4 * i, 4 * k)[1]
		i = i + k
	return r
def mtset(addr, vals):
	SDTest.Call('crt_memcpy', 'iiFi', addr, tuple(vals), 4 * len(vals))
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
	m = SDTest.Call('operator_new', 'ii', 0x4c)[0]
	SDTest.Call('CTriMesh_ctor', 'tii', m)
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
		SDTest.Call('CTriMesh_SetVert', 'tviiF', m, i, vec())
	for i in range(ntv):
		SDTest.Call('CTriMesh_SetTVert', 'tviiF', m, i, (rnd(0.0, 1.0), rnd(0.0, 1.0)))
	for i in range(nt):
		SDTest.Call('CTriMesh_SetTriVerts', 'tviiiii', m, i, i % nv, (i + 1) % nv, (i + 2) % nv)
		if ntv:
			SDTest.Call('CTriMesh_SetTriTVerts', 'tviiiii', m, i, (i + 2) % ntv, i % ntv, (i + 1) % ntv)
		SDTest.Call('CTriMesh_SetTriNormals', 'tviiFFF', m, i, vec(), vec(), vec())
		SDTest.Call('CTriMesh_SetTriMat', 'tviii', m, i, 0x100 + i)
		SDTest.Call('CTriMesh_SetTriFlags', 'tviii', m, i, i % 4)

m = SDTest.Call('operator_new', 'ii', 0x4c)[0]
check('CTriMesh_ctor', lambda m=m: (SDTest.Call('CTriMesh_ctor', 'tii', m)[0] == m, mtget(m)))
mtstep0('CTriMesh_SetNumVert', m, sym('CTriMesh_SetNumVert'), 'tviii', 3, 0)
for i in range(3):
	mtstep0('CTriMesh_SetVert', m, sym('CTriMesh_SetVert'), 'tviiF', i, vec())
check('CTriMesh_SetVert all', lambda m=m: mtget(m))
mtstep0('CTriMesh_SetNumVert grow', m, sym('CTriMesh_SetNumVert'), 'tviii', 5, 1)
check('CTriMesh_SetNumVert grow copy', lambda m=m: mtf(SDTest.Peek(m + 0x20, '@i'), 9))
mtstep0('CTriMesh_SetNumVert less', m, sym('CTriMesh_SetNumVert'), 'tviii', 4, 1)
mtstep0('CTriMesh_SetNumVert grow 2', m, sym('CTriMesh_SetNumVert'), 'tviii', 11, 1)
check('CTriMesh_SetNumVert grow 2 copy', lambda m=m: mtf(SDTest.Peek(m + 0x20, '@i'), 9))
mtstep0('CTriMesh_SetNumTVert', m, sym('CTriMesh_SetNumTVert'), 'tviii', 2, 0)
mtstep0('CTriMesh_SetNumTVert grow', m, sym('CTriMesh_SetNumTVert'), 'tviii', 6, 1)
mtstep0('CTriMesh_SetNumTri', m, sym('CTriMesh_SetNumTri'), 'tviii', 2, 0)
check('CTriMesh_SetNumTri ctor', lambda m=m: (tpeek(SDTest.Peek(m + 0x28, '@i') + 0x3c, 2), tpeek(SDTest.Peek(m + 0x28, '@i') + 0x80, 2)))
mtstep0('CTriMesh_SetNumTri grow', m, sym('CTriMesh_SetNumTri'), 'tviii', 3, 1)
check('CTriMesh_SetNumTri grow ctor', lambda m=m: tpeek(SDTest.Peek(m + 0x28, '@i') + 0x44 * 5 + 0x3c, 2))
mtfill(m, 11, 6, 3)
check('CTriMesh filled', lambda m=m: mtget(m))
mtstep('CTriMesh_SetTriVerts', m, sym('CTriMesh_SetTriVerts'), 'tviiiii', 1, 4, 5, 6)
mtstep('CTriMesh_SetTriTVerts', m, sym('CTriMesh_SetTriTVerts'), 'tviiiii', 2, 5, 4, 3)
mtstep('CTriMesh_SetTriNormals', m, sym('CTriMesh_SetTriNormals'), 'tviiFFF', 0, (0.0, 0.0, 0.0), (1e-30, 0.0, 0.0), (nan, 1.0, 0.0))
mtstep('CTriMesh_SetTriMat', m, sym('CTriMesh_SetTriMat'), 'tviii', 2, 0x77)
mtstep('CTriMesh_SetTriFlags', m, sym('CTriMesh_SetTriFlags'), 'tviii', 1, 0x55)
mtstep('CTriMesh_SetTVert', m, sym('CTriMesh_SetTVert'), 'tviiF', 5, (0.25, -3.5))
for i in range(3):
	check('CTriMesh_GetTri' + fmt((i,)), lambda m=m, i=i: SDTest.Call('CTriMesh_GetTri', 'tiii', m, i)[0] - SDTest.Peek(m + 0x28, '@i'))
	for k in range(3):
		check('CTriMesh_GetTriVert' + fmt((i, k)), lambda m=m, i=i, k=k: SDTest.Call('CTriMesh_GetTriVert', 'tviFii', m, (9.0,) * 8, i, k)[1])
		check('CTriMesh_GetTriVert2' + fmt((i, k)), lambda m=m, i=i, k=k: SDTest.Call('CTriMesh_GetTriVert@263aa0', 'tviFii', m, (9.0,) * 8, i, k)[1])
for i in (0, 3, 10):
	check('CTriMesh_GetVert' + fmt((i,)), lambda m=m, i=i: SDTest.Call('CTriMesh_GetVert', 'tiiFi', m, (9.0,) * 3, i)[1])
mtstep0('CTriMesh_AddVert', m, sym('CTriMesh_AddVert'), 'tii')
mtstep0('CTriMesh_AddTVert', m, sym('CTriMesh_AddTVert'), 'tii')
mtstep0('CTriMesh_AddTri', m, sym('CTriMesh_AddTri'), 'tii')
mtstep0('CTriMesh_SetTriVerts new', m, sym('CTriMesh_SetTriVerts'), 'tviiiii', 3, 11, 0, 2)
mtstep0('CTriMesh_SetTriTVerts new', m, sym('CTriMesh_SetTriTVerts'), 'tviiiii', 3, 6, 6, 0)
mtstep0('CTriMesh_SetTriNormals new', m, sym('CTriMesh_SetTriNormals'), 'tviiFFF', 3, vec(), vec(), (0.0, -2.0, 0.0))
mtstep0('CTriMesh_SetTVert new', m, sym('CTriMesh_SetTVert'), 'tviiF', 6, (0.5, 0.75))
mtstep('CTriMesh_SetVert new', m, sym('CTriMesh_SetVert'), 'tviiF', 11, vec())
for first in (0, 1):
	box = vec() + vec()
	check('CTriMesh_CalcBox' + fmt((box, first)), lambda m=m, box=box, first=first: (SDTest.Call('CTriMesh_CalcBox', 'tviFi', m, box, first)[1], mtget(m)))
	check('CTriMesh_GetBox' + fmt((box, first)), lambda m=m, box=box, first=first: SDTest.Call('CTriMesh_GetBox', 'tviFi', m, box, first)[1])
mtstep('CTriMesh_Transform', m, sym('CTriMesh_Transform'), 'tviF', affine())
mtstep('CTriMesh_Transform any', m, sym('CTriMesh_Transform'), 'tviF', mat())
mtstep('CTriMesh_RemoveTri', m, sym('CTriMesh_RemoveTri'), 'tvii', 1)
mtstep('CTriMesh_RemoveTri last', m, sym('CTriMesh_RemoveTri'), 'tvii', 2)
for rva in ('CTriMesh_NoFile', 'CTriMesh_NoFile@262be0'):
	check('CTriMesh_NoFile' + fmt((rva,)), lambda m=m, rva=rva: SDTest.Call(rva, 'tbiii', m, 0, 0))

# texture vertex 0 for triangles without them; 0 texture vertices free them
m2 = mtnew()
mtstep('CTriMesh_CreateTVert empty', m2, sym('CTriMesh_CreateTVert'), 'tvi')
SDTest.Call('CTriMesh_SetNumVert', 'tviii', m2, 4, 0)
SDTest.Call('CTriMesh_SetNumTri', 'tviii', m2, 2, 0)
mtfill(m2, 4, 0, 2)
mtstep('CTriMesh_CreateTVert', m2, sym('CTriMesh_CreateTVert'), 'tvi')
mtstep('CTriMesh_CreateTVert again', m2, sym('CTriMesh_CreateTVert'), 'tvi')
mtstep('CTriMesh_SetNumTVert 0', m2, sym('CTriMesh_SetNumTVert'), 'tviii', 0, 1)
mtstep('CTriMesh_SetNumTVert 0 again', m2, sym('CTriMesh_SetNumTVert'), 'tviii', 0, 1)

def mtvert():
	return vec() + vec() + (rnd(0.0, 1.0), rnd(0.0, 1.0))
m3 = mtnew()
for i in range(3):
	mtstep('CTriMesh_AddTriVerts', m3, sym('CTriMesh_AddTriVerts'), 'tviFFFiii', mtvert(), mtvert(), mtvert(), 0x200 + i, 0, i + 1)
m4 = mtnew()
for i in range(3):
	mtstep('CTriMesh_AddTriVerts tvert', m4, sym('CTriMesh_AddTriVerts'), 'tviFFFiii', mtvert(), mtvert(), mtvert(), 0x300 + i, 1, 7 * i)

# merges: all triangles, filtered by flags, without texture vertices, with skins
m5 = mtnew()
mtstep('CTriMesh_Add empty', m5, sym('CTriMesh_Add'), 'tviiii', m2, 0, 0)
mtstep('CTriMesh_Add', m5, sym('CTriMesh_Add'), 'tviiii', m, 0, 0)
mtstep('CTriMesh_Add mask', m5, sym('CTriMesh_Add'), 'tviiii', m3, 1, 1)
mtstep('CTriMesh_Add mask 0', m5, sym('CTriMesh_Add'), 'tviiii', m4, 2, 0)
mtstep('CTriMesh_Add want 2', m5, sym('CTriMesh_Add'), 'tviiii', m4, 0, 2)
m6 = mtnew()
mtstep('CTriMesh_Add no tvert', m6, sym('CTriMesh_Add'), 'tviiii', m3, 0, 0)
mtstep('CTriMesh_Add tvert', m6, sym('CTriMesh_Add'), 'tviiii', m4, 0, 1)
check('CTriMesh_AllocSkin', lambda m=m: SDTest.Call('CTriMesh_AllocSkin', 'tii', m)[0] == SDTest.Peek(m + 0x2c, '@i'))
nv = SDTest.Peek(m + 8, '@i')
skin = SDTest.Peek(m + 0x2c, '@i')
for i in range(nv):
	mtset(skin + 16 * i, (i, 1.0 / (i + 1), -i, 0.5))
	check('CTriMesh_GetSkin' + fmt((i,)), lambda m=m, i=i, skin=skin: SDTest.Call('CTriMesh_GetSkin', 'tiii', m, i)[0] - skin)
m7 = mtnew()
mtstep('CTriMesh_Add skin', m7, sym('CTriMesh_Add'), 'tviiii', m, 1, 0)
check('CTriMesh_Add skin data', lambda m7=m7: mtf(SDTest.Peek(m7 + 0x2c, '@i'), 4 * SDTest.Peek(m7 + 8, '@i')))
mtstep('CTriMesh_Add skin 2', m7, sym('CTriMesh_Add'), 'tviiii', m, 3, 1)
check('CTriMesh_Add skin data 2', lambda m7=m7: mtf(SDTest.Peek(m7 + 0x2c, '@i'), 4 * SDTest.Peek(m7 + 8, '@i')))
m8 = SDTest.Call('operator_new', 'ii', 0x4c)[0]
check('CTriMesh_ctorCopy', lambda m8=m8, m=m: (SDTest.Call('CTriMesh_ctorCopy', 'tiii', m8, m)[0] == m8, mtget(m8)))

# CVertList: 4 vertices of 0x34 bytes (the largest format) read in each format
lv = SDTest.Call('Mem_Calloc', 'ii', 16)[0]
vb = SDTest.Call('Mem_Calloc', 'ii', 0x34 * 4)[0]
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
			check('CVertList_GetNormal' + fmt((f, i)), lambda lv=lv, i=i: SDTest.Call('CVertList_GetNormal', 'tiiFi', lv, (9.0,) * 3, i)[1])
		check('CVertList_GetUV' + fmt((f, i)), lambda lv=lv, i=i: SDTest.Call('CVertList_GetUV', 'tiiFi', lv, (9.0,) * 2, i)[1])

# CMeshD3D: a list of two meshes over VERT_TEX1 vertices, the first with triangle flags
def d3dnew(nxt, tris, nverts, flags, matv):
	d = SDTest.Call('Mem_Calloc', 'ii', 0x50)[0]
	idx = SDTest.Call('Mem_Calloc', 'ii', 6 * len(tris) + 4)[0]
	shorts = []
	for t in tris:
		shorts = shorts + list(t)
	if len(shorts) % 2:
		shorts.append(0)
	for i in range(0, len(shorts), 2):
		SDTest.Poke(idx + 2 * i - 0x400000, shorts[i] + (shorts[i + 1] << 16))
	v = SDTest.Call('Mem_Calloc', 'ii', 0x20 * nverts)[0]
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
		fl = SDTest.Call('Mem_Calloc', 'ii', 4 * len(flags))[0]
		for i in range(len(flags)):
			SDTest.Poke(fl + 4 * i - 0x400000, flags[i])
		SDTest.Poke(d + 0x48 - 0x400000, fl)
	SDTest.Poke(d + 0x4c - 0x400000, matv)
	return d
d2 = d3dnew(0, ((1, 0, 2),), 3, (), 0x88)
d1 = d3dnew(d2, ((0, 1, 2), (2, 3, 0)), 4, (5, 6), 0x77)
m9 = mtnew()
mtstep('CTriMesh_AddD3D', m9, sym('CTriMesh_AddD3D'), 'tviii', d1, 0)
mtstep('CTriMesh_AddD3D all', m9, sym('CTriMesh_AddD3D'), 'tviii', d1, 1)
mtstep('CTriMesh_AddD3D second', m9, sym('CTriMesh_AddD3D'), 'tviii', d2, 1)
m10 = SDTest.Call('operator_new', 'ii', 0x4c)[0]
check('CTriMesh_ctorD3D', lambda m10=m10, d1=d1: (SDTest.Call('CTriMesh_ctorD3D', 'tiiii', m10, d1, 1)[0] == m10, mtget(m10)))
mtstep('CTriMesh_Add d3d', m10, sym('CTriMesh_Add'), 'tviiii', m9, 4, 1)

# teardown: Clear, dtor and Delete; the rest freed with Delete
mtstep('CTriMesh_Clear', m9, sym('CTriMesh_Clear'), 'tvi')
mtstep('CTriMesh_Clear again', m9, sym('CTriMesh_Clear'), 'tvi')
mtstep('CTriMesh_Init', m10, sym('CTriMesh_Init'), 'tvi')
check('CTriMesh_dtor', lambda m8=m8: (SDTest.Call('CTriMesh_dtor', 'tvi', m8), mtget(m8)))
check('CTriMesh_Delete 0', lambda m=m: (SDTest.Call('CTriMesh_Delete', 'tiii', m, 0)[0] == m, mtget(m)))
check('CMeshTri_ctor', lambda lv=lv: (SDTest.Call('CMeshTri_ctor', 'tii', lv - 0x3c)[0] == lv - 0x3c, tpeek(lv, 4)))
for x in (m, m2, m3, m4, m5, m6, m7, m8, m9, m10):
	SDTest.Call('CTriMesh_Delete', 'tiii', x, 1)
SDTest.Call('Mem_Free', 'vi', vb)
SDTest.Call('Mem_Free', 'vi', lv)
