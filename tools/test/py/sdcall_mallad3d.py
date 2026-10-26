# engine3d Engine/MallaD3D.c (execfile'd at the end of sdcall.py, Python 1.5.2): the CPU side of the vertex buffer
# meshes, on meshes, temporary objects and triangles in heap memory. No device: the buffers (Restore, CreateVB), Draw
# and the files (Read, Write, Array_Write*) are not called. Pointers are printed as flags or as indices into one
# block: the fake materials (CMaterial, 0xb0 bytes, b38 two sided, b3b two texture sets, map 0 with a name) and a
# fake node (refs, flags, object at 0x3c, a matrix at 0x9c, no animation channels).
def mdalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def mdfree(p):
	SDTest.Call(0x133550, 'vi', p)
def mdpoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def mdfset(addr, vals):
	SDTest.Call(0x3197d0, 'iiFi', addr, tuple(vals), 4 * len(vals))
def mdshorts(addr, n):
	r = []
	for i in range(n):
		r.append(SDTest.Peek(addr + 2 * i, '@i') & 0xffff)
	return r
def mdstr(s):
	p = mdalloc(len(s) + 4)
	for i in range(len(s)):
		SDTest.Call(0x3197d0, 'iibi', p + i, s[i], 1)
	return p

mdmats = mdalloc(5 * 0xb0)
def mdmat(i):
	return mdmats + i * 0xb0
# the material of a pointer: -1 for NULL
def mdmatidx(p):
	if p == 0:
		return -1
	return (p - mdmats) / 0xb0
mdmapas = mdalloc(3 * 0x60)
mdnames = (mdstr('abc'), mdstr('abd'), mdstr('ABC'))
for i in range(3):
	mdpoke(mdmapas + 0x60 * i, mdnames[i])
	mdpoke(mdmat(i) + 0x9c, mdmapas + 0x60 * i)
# mat 1 two sided, mat 2 two texture sets, mat 3 two sided without a map name, mat 4 plain
mdpoke(mdmat(1) + 0x38, 1)
mdpoke(mdmat(2) + 0x38, 0x01000000)
mdpoke(mdmat(3) + 0x38, 1)

mdnodo = mdalloc(0x100)
mdpoke(mdnodo + 4, mdnames[0])
mdpoke(mdnodo + 8, 10)
mdfset(mdnodo + 0x9c, affine())

VSIZE = {1: 0x20, 2: 0x28, 4: 0x24}
# one mesh: the members, pointers as flags, the pads masked
def mdhead(d):
	w = tpeek(d, 50)
	return (w[1], w[3], w[4] != 0, w[5] != 0, w[6], w[7], w[8], w[10] != 0, w[11:15], w[15:18] != [0, 0, 0],
		w[18] != 0, mdmatidx(w[19]), w[20] & 0xffff, w[21:36], w[36], w[37] != 0, w[38] & 0xff, w[39:43],
		w[43:45] != [0, 0], w[45] == mdnodo, w[45] != 0, w[46:50])
# the list with the contents: triangles, vertices, vertOrig, triangle flags, the skin of the first
def mdget(d):
	r = ()
	skin = 0
	while d:
		w = tpeek(d, 50)
		x = (mdhead(d),)
		if w[4]:
			x = x + (mdshorts(w[4], 3 * w[3]),)
		if w[5] and VSIZE.has_key(w[8]):
			x = x + (tpeek(w[5], w[6] * VSIZE[w[8]] / 4),)
		if w[10]:
			x = x + (mdshorts(w[10], w[6]),)
		if w[18]:
			x = x + (tpeek(w[18], w[3]),)
		if w[37]:
			if skin:
				x = x + (w[37] == skin,)
			else:
				skin = w[37]
				x = x + (tpeek(skin, 4 * w[36]),)
		r = r + (x,)
		d = w[2]
	return r
def mdnew():
	d = SDTest.Call(0x1334d0, 'ii', 200)[0]
	SDTest.Call(0x2af990, 'tii', d)
	return d

# a mesh by hand: 4 vertices of VERT_TEX1, 3 triangles (the last degenerate), a second mesh in the list
d1 = SDTest.Call(0x1334d0, 'ii', 200)[0]
check('CMallaD3D_ctor', lambda d1=d1: (SDTest.Call(0x2af990, 'tii', d1)[0] == d1, mdhead(d1)))
mdvft = SDTest.Peek(d1, '@i')
def mdverts(d, n):
	SDTest.Call(0x242400, 'tvii', d + 0x14, 1)
	SDTest.Call(0x242470, 'tviii', d + 0x14, n, 0)
	for i in range(n):
		SDTest.Call(0x242290, 'tviiF', d + 0x14, i, vec() + vec() + (rnd(0.0, 1.0), rnd(0.0, 1.0)))
mdverts(d1, 4)
check('CMallaD3D_AllocTri', lambda d1=d1: (SDTest.Call(0x2b1e70, 'tvii', d1, 3), mdhead(d1)))
for i, t in ((0, (0, 1, 2)), (1, (2, 3, 0)), (2, (1, 1, 1))):
	check('CMallaD3D_SetTri' + fmt((i, t)), lambda d1=d1, i=i, t=t: (apply(SDTest.Call, (0x2b2030, 'tviiiii', d1, i) + t), mdshorts(SDTest.Peek(d1 + 0x10, '@i') + 6 * i, 3)))
check('CMallaD3D_SetTri all', lambda d1=d1: mdget(d1))
for i in range(3):
	check('CMallaD3D_TriNormal' + fmt((i,)), lambda d1=d1, i=i: SDTest.Call(0x2b0790, 'tiiFi', d1, (9.0,) * 3, i)[1])
check('CMallaD3D_CalcCenter', lambda d1=d1: (SDTest.Call(0x2b0a20, 'tvi', d1), mdhead(d1)))
d2 = mdnew()
mdverts(d2, 3)
SDTest.Call(0x2b1e70, 'tvii', d2, 1)
SDTest.Call(0x2b2030, 'tviiiii', d2, 0, 2, 1, 0)
mdpoke(d1 + 8, d2)
for first in (0, 1):
	box = vec() + vec()
	check('CMallaD3D_CalcBox' + fmt((box, first)), lambda d1=d1, box=box, first=first: (SDTest.Call(0x2b0960, 'tviFi', d1, box, first)[1], mdget(d1)))
	check('CMallaD3D_GetBox' + fmt((box, first)), lambda d1=d1, box=box, first=first: SDTest.Call(0x2aff30, 'tviFi', d1, box, first)[1])
check('CMallaD3D_CheckList', lambda d1=d1: SDTest.Call(0x2b02e0, 'tvi', d1))
check('CMallaD3D_Check', lambda d1=d1: SDTest.Call(0x2b20e0, 'tii', d1))
check('CMallaD3D_Invalidate', lambda d1=d1: (SDTest.Call(0x2afea0, 'tvii', d1, 0), mdget(d1)))
check('CMallaD3D_SetNodo', lambda d1=d1: (SDTest.Call(0x2b1bd0, 'tvii', d1, mdnodo), SDTest.Peek(mdnodo + 8, '@i'), mdget(d1)))
check('CMallaD3D_SetNodo again', lambda d1=d1: (SDTest.Call(0x2b1bd0, 'tvii', d1, mdnodo), SDTest.Peek(mdnodo + 8, '@i')))
# the center of a skinned mesh goes through the node matrix
mdskin = mdalloc(64)
mdpoke(d1 + 0x94, mdskin)
check('CMallaD3D_CalcCenter skin', lambda d1=d1: (SDTest.Call(0x2b0a20, 'tvi', d1), mdhead(d1)))
check('CMallaD3D_CalcBox skin', lambda d1=d1: SDTest.Call(0x2b0960, 'tviFi', d1, (0.0,) * 6, 1)[1])
mdpoke(d1 + 0x94, 0)
# no animation channels in the node: no vertex animation
nodor = mdalloc(0xa0)
mdpoke(nodor + 0x94, 0x1234)
mdpoke(d1 + 8, 0)
check('CMallaD3D_SetRender', lambda d1=d1, nodor=nodor: (SDTest.Call(0x2b0650, 'tvii', d1, nodor), tpeek(nodor + 0x94, 2)))
check('CMallaD3D_SetRender NULL', lambda d1=d1: SDTest.Call(0x2b0650, 'tvii', d1, 0))
mdpoke(d1 + 8, d2)
check('CMallaD3D_SetNodo NULL', lambda d1=d1: (SDTest.Call(0x2b1bd0, 'tvii', d1, 0), SDTest.Peek(mdnodo + 8, '@i'), mdhead(d1)))
check('CMallaD3D_Init', lambda d2=d2: (SDTest.Call(0x2b1d50, 'tvi', d2), mdhead(d2)))
SDTest.Call(0x23fc00, 'tvi', d2 + 0x14)
check('CMallaD3D_dtor', lambda d2=d2: (SDTest.Call(0x2afa70, 'tvi', d2), SDTest.Peek(d2, '@i') == mdvft, SDTest.Peek(d2 + 0x14, '@i')))
mdpoke(d1 + 8, 0)
check('CMallaD3D_Delete 0', lambda d1=d1: (SDTest.Call(0x2afbe0, 'tiii', d1, 0)[0] == d1, mdhead(d1)))
SDTest.Call(0x313634, 'vii', d1, 200)
SDTest.Call(0x313634, 'vii', d2, 200)

# the temporary objects: vertices of 0x24 bytes (8 floats, index, bone), triangles of 10 bytes
mdA = vec() + vec() + (0.25, 0.5)
mdB = vec() + vec() + (0.75, 0.0)
mdC = vec() + vec() + (0.0, 1.0)
mdvs = ((mdA, 0, 1), (mdB, 1, 0), (mdA, 2, 1), (mdA, 3, 2), (mdB, 4, 0), (mdC, 5, 0), (mdB, 6, 1))
vb = mdalloc(0x24 * len(mdvs))
for i in range(len(mdvs)):
	v, idx, bone = mdvs[i]
	mdfset(vb + 0x24 * i, v)
	mdpoke(vb + 0x24 * i + 0x20, idx + (bone << 16))
tb = mdalloc(12 * 3)
for i, t, fl in ((0, (0, 1, 2), 7), (1, (3, 4, 5), 9), (2, (6, 0, 4), 0)):
	mdpoke(tb + 12 * i, fl)
	mdpoke(tb + 12 * i + 4, t[0] + (t[1] << 16))
	mdpoke(tb + 12 * i + 8, t[2])
for i in range(len(mdvs)):
	for j in range(len(mdvs)):
		check('MallaD3D_CompareVert' + fmt((i, j)), lambda i=i, j=j: SDTest.Call(0x2b0720, 'iii', vb + 0x24 * i, vb + 0x24 * j)[0])
ib = mdalloc(4 * len(mdvs))
for i in range(len(mdvs)):
	mdpoke(ib + 4 * i, len(mdvs) - 1 - i)
SDTest.Poke(0x4de92c, vb)
for i in range(len(mdvs)):
	for j in (0, 3, 6):
		check('MallaD3D_CompareVertIdx' + fmt((i, j)), lambda i=i, j=j: SDTest.Call(0x2b0c50, 'iii', ib + 4 * i, ib + 4 * j)[0])
def mdobj(o):
	w = tpeek(o, 5)
	r = (mdmatidx(w[0]), w[1], w[2], w[3] != 0, w[4] != 0)
	if w[3]:
		r = r + (tpeek(w[3], 9 * w[1]),)
	if w[4]:
		r = r + (mdshorts(w[4], 5 * w[2]),)
	return r
o = mdalloc(0x14)
check('MallaD3DObj_AllocVert', lambda o=o: (SDTest.Call(0x2afdf0, 'vii', o, len(mdvs)), mdobj(o)[0:5]))
check('MallaD3DObj_AllocTri', lambda o=o: (SDTest.Call(0x2afda0, 'vii', o, 3), mdobj(o)[0:5]))
check('MallaD3DObj_SetMat', lambda o=o: (SDTest.Call(0x2afd70, 'vii', o, mdmat(2)), mdobj(o)[0:5]))
for i in range(len(mdvs)):
	SDTest.Call(0x2afd10, 'viii', o, i, vb + 0x24 * i)
for i in range(3):
	SDTest.Call(0x2afc70, 'viii', o, i, tb + 12 * i)
check('MallaD3DObj_SetVert SetTri', lambda o=o: mdobj(o))
check('MallaD3DObj_Weld', lambda o=o: (SDTest.Call(0x2b2130, 'ii', o)[0], mdobj(o)))
check('MallaD3DObj_Weld again', lambda o=o: (SDTest.Call(0x2b2130, 'ii', o)[0], mdobj(o)))
check('MallaD3DObj_Free', lambda o=o: (SDTest.Call(0x2afe40, 'vi', o), mdobj(o)))
check('MallaD3DObj_Free again', lambda o=o: (SDTest.Call(0x2afe40, 'vi', o), mdobj(o)))
mdfree(o)
mdn = SDTest.Peek(0x4de928, 'i')
for k in range(5):
	SDTest.Poke(0x4dc128 + 4 * k + 20 * mdn, 0x1234 + k)
def mdentry(i):
	r = []
	for k in range(5):
		r.append(SDTest.Peek(0x4dc128 + 20 * i + 4 * k, 'i'))
	return r
check('MallaD3D_NewObj', lambda mdn=mdn: (mdn, SDTest.Call(0x2afc10, 'i')[0], SDTest.Peek(0x4de928, 'i'), mdentry(mdn)))
SDTest.Poke(0x4de928, mdn)

# groups: the material and how many triangles
gb = mdalloc(16 * 8)
mdgs = ((0, 1, 0), (1, 1, 1), (2, 1, 2), (-1, 1, 3), (0, 1, 4), (-1, 1, 5), (3, 1, 6), (4, 0x7ff, 7))
for i in range(len(mdgs)):
	m, n, idx = mdgs[i]
	if m >= 0:
		mdpoke(gb + 16 * i, mdmat(m))
	mdpoke(gb + 16 * i + 4, n)
	mdpoke(gb + 16 * i + 8, idx)
for i in range(len(mdgs)):
	for j in range(len(mdgs)):
		if i != j:
			check('MallaD3D_CompareGrupo' + fmt((i, j)), lambda i=i, j=j: SDTest.Call(0x2b0ab0, 'iii', gb + 16 * i, gb + 16 * j)[0])

# triangles of a CMallaTri: 6 vertices, 4 texture vertices, 6 triangles, materials 0, 1 (two sided), 2 (two texture
# sets), 3 and 4; triangles 3 and 4 with the same normals so their vertices weld
def mdmalla(skin):
	m = SDTest.Call(0x1334d0, 'ii', 0x4c)[0]
	SDTest.Call(0x2626d0, 'tii', m)
	SDTest.Call(0x263e40, 'tviii', m, 6, 0)
	SDTest.Call(0x263c00, 'tviii', m, 4, 0)
	SDTest.Call(0x263cf0, 'tviii', m, 6, 0)
	for i in range(6):
		SDTest.Call(0x264270, 'tviiF', m, i, vec())
	for i in range(4):
		SDTest.Call(0x264200, 'tviiF', m, i, (rnd(0.0, 1.0), rnd(0.0, 1.0)))
	n = (vec(), vec(), vec())
	tris = ((0, 1, 2), (2, 3, 4), (4, 5, 0), (1, 3, 5), (3, 1, 0), (5, 4, 3))
	mats = (0, mdmat(1), mdmat(2), mdmat(4), mdmat(4), mdmat(3))
	for i in range(6):
		apply(SDTest.Call, (0x264180, 'tviiiii', m, i) + tris[i])
		SDTest.Call(0x264100, 'tviiiii', m, i, i % 4, (i + 1) % 4, (i + 2) % 4)
		if i in (3, 4):
			apply(SDTest.Call, (0x264060, 'tviiFFF', m, i) + n)
		else:
			SDTest.Call(0x264060, 'tviiFFF', m, i, vec(), vec(), vec())
		SDTest.Call(0x263ff0, 'tviii', m, i, mats[i])
		SDTest.Call(0x263f80, 'tviii', m, i, 0x10 * i + 1)
	if skin:
		p = SDTest.Call(0x263f10, 'tii', m)[0]
		for i in range(6):
			mdpoke(p + 16 * i, 0x01000000 * i + 0x10000 * (i % 3) + 0x100 * (5 - i) + i)
			mdfset(p + 16 * i + 4, (rnd(0.0, 1.0), rnd(0.0, 1.0), rnd(0.0, 1.0)))
	return m
def mdtri(t):
	w = tpeek(t, 30)
	w[27] = mdmatidx(w[27])
	return w
m1 = mdmalla(0)
m2 = mdmalla(1)
tt = mdalloc(0x78 * 4)
for m in (m1, m2):
	for i in range(6):
		for keep in (0, 1):
			check('MallaD3D_FillTri' + fmt((m == m2, i, keep)), lambda m=m, i=i, keep=keep: (SDTest.Call(0x2b1ef0, 'viiii', tt, m, i, keep), mdtri(tt)))
for i in range(4):
	SDTest.Call(0x2b1ef0, 'viiii', tt + 0x78 * i, m1, i, i % 2)
	check('MallaD3D_TriPlane' + fmt((i,)), lambda i=i: SDTest.Call(0x2b1c30, 'viFF', tt + 0x78 * i, (9.0,) * 3, (9.0,) * 4)[1:])
for i in range(4):
	for j in range(4):
		check('MallaD3D_CompareTri' + fmt((i, j)), lambda i=i, j=j: SDTest.Call(0x2b0b60, 'iii', tt + 0x78 * i, tt + 0x78 * j)[0])
for g in (0, 1, 3, 7):
	for t in (0, 1, 2, 3):
		check('MallaD3D_GrupoFits' + fmt((g, t)), lambda g=g, t=t: SDTest.Call(0x2b20b0, 'iii', gb + 16 * g, tt + 0x78 * t)[0])
check('MallaD3D_GrupoFits NULL', lambda: (SDTest.Call(0x2b20b0, 'iii', 0, tt)[0], SDTest.Call(0x2b20b0, 'iii', gb, 0)[0]))
for n in (0x7ff, 0x800, 0x801, -1, 0):
	mdpoke(gb + 16 * 7 + 4, n)
	check('MallaD3D_GrupoFits' + fmt((n,)), lambda: SDTest.Call(0x2b20b0, 'iii', gb + 16 * 7, tt + 0x78 * 3)[0])

# whole meshes from the CMallaTri: the flags, a node (its flags clear MALLAD3D_POS), skins, the mesh at 0x30
SDTest.Call(0x323c50, 'vi', 4321)
def mdbuild(name, m, flags, nodo, nflags):
	if nodo:
		mdpoke(nodo + 0x18, nflags)
	d = SDTest.Call(0x1334d0, 'ii', 200)[0]
	check(name + fmt((flags, nodo != 0, nflags)), lambda d=d, m=m, flags=flags, nodo=nodo: (SDTest.Call(0x2af820, 'tiiiii', d, m, flags, nodo)[0] == d, SDTest.Peek(0x4de928, 'i'), mdget(d)))
	if nodo:
		check(name + ' nodo' + fmt((flags,)), lambda d=d, nodo=nodo: (SDTest.Peek(nodo + 8, '@i'), SDTest.Peek(nodo + 0x3c, '@i') == d))
		mdpoke(nodo + 0x3c, 0)
	check(name + ' Delete' + fmt((flags,)), lambda d=d: SDTest.Call(0x2afbe0, 'tiii', d, 1)[0] == d)
for flags in (0, 1, 2, 4, 8, 0x22, 0x27):
	mdbuild('CMallaD3D_ctorMalla', m1, flags, 0, 0)
mdbuild('CMallaD3D_ctorMalla nodo', m1, 2, mdnodo, 0)
mdbuild('CMallaD3D_ctorMalla nodo', m1, 0x12, mdnodo, 8)
# a skinned mesh needs the node: its center goes through the node matrix
mdbuild('CMallaD3D_ctorMalla skin nodo', m2, 0, mdnodo, 0)
mdbuild('CMallaD3D_ctorMalla skin nodo', m2, 0x23, mdnodo, 0x80)
check('CMallaD3D nodo refs', lambda: SDTest.Peek(mdnodo + 8, '@i'))
mdpoke(m2 + 0x30, m1)
mdbuild('CMallaD3D_ctorMalla d3d', m2, 2, 0, 0)
mdpoke(m2 + 0x30, 0)
# Build into a constructed mesh
d = mdnew()
check('CMallaD3D_Build', lambda d=d: (SDTest.Call(0x2b0c80, 'tviii', d, m1, 0x20), mdget(d)))
check('CMallaD3D_Build CalcBox', lambda d=d: (SDTest.Call(0x2b0960, 'tviFi', d, (0.0,) * 6, 1)[1], mdget(d)))
check('CMallaD3D_Build CheckList', lambda d=d: SDTest.Call(0x2b02e0, 'tvi', d))
SDTest.Call(0x2afbe0, 'tiii', d, 1)

for x in (m1, m2):
	SDTest.Call(0x2627b0, 'tiii', x, 1)
for x in (vb, tb, ib, gb, tt, nodor, mdskin, mdnodo, mdmapas, mdmats) + mdnames:
	mdfree(x)
