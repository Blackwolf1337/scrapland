# engine3d Engine/Suelo.c (run by sdcall.py after its own checks, its helpers in scope, Python 1.5.2): the geometry
# helpers on random input, collision meshes, quads and whole floors (CSuelo) in heap memory with meshes of known
# triangles, then many random spheres and sweeps against them. The engine variables the floor code uses are saved
# and put back at the end. The floors with meshes are leaked (their destructor CHECKs that no quad uses a mesh), the
# destructors run on empty ones. Heap addresses are never printed:
# meshes are printed by index. Not called: the .amc and SUEL files (need files to write or a world to read them),
# CSuelo_Draw, the debug drawing and Suelo_DrawBox (render state), the floor nodes (none exist).
def sunew(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def sunewop(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
# operator new memory cleared, for the members a constructor leaves alone (n a multiple of 4)
def suzero(addr, n):
	for i in range(0, n, 4):
		SDTest.Poke(addr + i - 0x400000, 0)
def suwf(addr, floats):
	i = 0
	while i < len(floats):
		k = min(64, len(floats) - i)
		SDTest.Call('crt_memcpy', 'iiFi', addr + 4 * i, tuple(floats[i:i + k]), 4 * k)
		i = i + k
def surf(addr, n):
	r = ()
	i = 0
	while i < n:
		k = min(64, n - i)
		r = r + SDTest.Call('crt_memcpy', 'iFii', (0.0,) * k, addr + 4 * i, 4 * k)[1]
		i = i + k
	return r
def suwi(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def suri(addr):
	return SDTest.Peek(addr, '@i')
def suwords(addr, n):
	r = ()
	for i in range(n):
		r = r + (suri(addr + 4 * i),)
	return r
# n unsigned shorts
def sushorts(addr, n):
	r = ()
	w = suwords(addr, (n + 1) / 2)
	for i in range(n):
		x = w[i / 2]
		if i % 2:
			x = (x >> 16) & 0xffff
		else:
			x = x & 0xffff
		r = r + (x,)
	return r

# the engine variables: query numbers, cache id, statistics, quad count, CollisionBalance
suvars = (sym('g_CollisionCache'), sym('g_QuadTriCacheID'), sym('g_CollisionNComp'), sym('g_BspCacheStats'), sym('g_CollisionCacheMiss'), sym('g_CollisionTestsMax'), sym('g_CollisionTests'), sym('g_CollisionTestsTotal'), sym('g_CollisionQuadCount'), sym('g_CollisionStats'),
	sym('g_CollisionStats') + 0x4, sym('g_CollisionStats') + 0x8, sym('g_CollisionStats') + 0xc, sym('g_CollisionStats') + 0x10, sym('g_CollisionStats') + 0x14, sym('g_CollisionStats2'), sym('g_CollisionBalance'))
susaved = []
for v in suvars:
	susaved.append(SDTest.Peek(v, 'i'))
def surestore():
	for i in range(len(suvars)):
		SDTest.Poke(suvars[i], susaved[i])
# the quad count from 0 (sdcall.py leaks quads, the menu may have a floor): the destructors and the wrap of the
# query number CHECK it against the quads made here
SDTest.Poke('g_CollisionQuadCount', 0)
SDTest.Call('crt_srand', 'vi', 4711)
# CollisionBalance 1.0 for a known grid
SDTest.Poke('g_CollisionBalance', 0x3f800000)
def suglobals():
	return (SDTest.Peek('g_QuadTriCacheID', 'i'), SDTest.Peek('g_CollisionNComp', 'i'), SDTest.Peek('g_CollisionTestsMax', 'i'),
		SDTest.Peek('g_CollisionTests', 'i'), SDTest.Peek('g_CollisionTestsTotal', 'i'), SDTest.Peek('g_CollisionQuadCount', 'i'))

# geometry
def suvec(lo=-5.0, hi=5.0):
	return (rnd(lo, hi), rnd(lo, hi), rnd(lo, hi))
def sutri():
	return suvec(), suvec(), suvec()
def sunear(p, d):
	return (p[0] + rnd(-d, d), p[1] + rnd(-d, d), p[2] + rnd(-d, d))
def sumid(a, b, c):
	return ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3)
z3 = (0.0, 0.0, 0.0)
for i in range(60):
	a, b, c = sutri()
	p = suvec()
	m = sumid(a, b, c)
	fcall('Suelo_PointInTri', sym('Suelo_PointInTri'), 'bFFFF', p, a, b, c)
	fcall('Suelo_PointInTri mid', sym('Suelo_PointInTri'), 'bFFFF', m, a, b, c)
	fcall('Suelo_PointInTri near', sym('Suelo_PointInTri'), 'bFFFF', sunear(m, 1.0), a, b, c)
	fcall('Suelo_PointInTri rev', sym('Suelo_PointInTri'), 'bFFFF', m, a, c, b)
	fcall('Suelo_DistPointSeg', sym('Suelo_DistPointSeg'), 'fFFFF', a, b, p, z3)
	fcall('Suelo_DistPointSeg NULL', sym('Suelo_DistPointSeg'), 'fFFFi', a, b, p, 0)
	fcall('Suelo_Dist2PointSeg', sym('Suelo_Dist2PointSeg'), 'fFFFF', a, b, p, z3)
	fcall('Suelo_Dist2PointSeg NULL', sym('Suelo_Dist2PointSeg'), 'fFFFi', a, b, p, 0)
	pl = SDTest.Call('Plane_SafeSet', 'tbFFFF', (0.0, 0.0, 0.0, 0.0), a, b, c)[1]
	r = rnd(0.0, 3.0)
	fcall('Suelo_SegPlane', sym('Suelo_SegPlane'), 'bFFfFF', p, sunear(p, 6.0), r, pl, (0.0,))
	fcall('Suelo_SegPlane near', sym('Suelo_SegPlane'), 'bFFfFF', sunear(m, 0.5), sunear(m, 0.5), r, pl, (0.0,))
	fcall('Suelo_SegPlane neg r', sym('Suelo_SegPlane'), 'bFFfFF', p, sunear(m, 1.0), -r, pl, (0.0,))
	fcall('Suelo_SegEdge', sym('Suelo_SegEdge'), 'bFFFFfF', a, b, p, sunear(p, 8.0), r, (100.0,))
	fcall('Suelo_SegEdge near', sym('Suelo_SegEdge'), 'bFFFFfF', a, b, sunear(a, 2.0), sunear(b, 2.0), r + 1.0, (100.0,))
	fcall('Suelo_SegEdge cross', sym('Suelo_SegEdge'), 'bFFFFfF', a, b, sunear(m, 3.0), sunear(m, 3.0), r, (100.0,))
	fcall('Suelo_SegEdge point', sym('Suelo_SegEdge'), 'bFFFFfF', a, a, p, sunear(a, 1.0), r, (100.0,))
	fcall('Suelo_SegEdge still', sym('Suelo_SegEdge'), 'bFFFFfF', a, b, p, p, r + 2.0, (100.0,))
for x in specials:
	v = (x, 1.0, x)
	fcall('Suelo_PointInTri s', sym('Suelo_PointInTri'), 'bFFFF', v, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
	fcall('Suelo_PointInTri s2', sym('Suelo_PointInTri'), 'bFFFF', (0.2, 0.2, 0.0), v, (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
	fcall('Suelo_DistPointSeg s', sym('Suelo_DistPointSeg'), 'fFFFF', v, (1.0, 2.0, 3.0), (0.0, 0.0, 0.0), z3)
	fcall('Suelo_DistPointSeg s2', sym('Suelo_DistPointSeg'), 'fFFFF', (0.0, 0.0, 0.0), (1.0, 2.0, 3.0), v, z3)
	fcall('Suelo_Dist2PointSeg s', sym('Suelo_Dist2PointSeg'), 'fFFFF', (0.0, 0.0, 0.0), v, (1.0, 2.0, 3.0), z3)
	fcall('Suelo_Dist2PointSeg s2', sym('Suelo_Dist2PointSeg'), 'fFFFF', (0.0, 0.0, 0.0), (1.0, 2.0, 3.0), v, z3)
	fcall('Suelo_SegPlane s', sym('Suelo_SegPlane'), 'bFFfFF', v, (1.0, -2.0, 3.0), 0.5, (0.0, 1.0, 0.0, 0.0), (0.0,))
	fcall('Suelo_SegPlane s2', sym('Suelo_SegPlane'), 'bFFfFF', (0.0, 1.0, 0.0), (1.0, -2.0, 3.0), x, (0.0, 1.0, 0.0, 0.0), (0.0,))
	fcall('Suelo_SegEdge s', sym('Suelo_SegEdge'), 'bFFFFfF', (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), v, (0.5, -1.0, 0.0), 0.5, (100.0,))
	fcall('Suelo_SegEdge s2', sym('Suelo_SegEdge'), 'bFFFFfF', (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.5, 1.0, 0.0), (0.5, -1.0, 0.0), x, (100.0,))
# exact cases: collinear points, a point on an edge, a segment parallel to the plane, an edge of length 0
fcall('Suelo_PointInTri line', sym('Suelo_PointInTri'), 'bFFFF', (0.5, 0.0, 0.0), (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
fcall('Suelo_PointInTri line out', sym('Suelo_PointInTri'), 'bFFFF', (2.0, 0.0, 0.0), (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
fcall('Suelo_PointInTri flat', sym('Suelo_PointInTri'), 'bFFFF', (0.5, 0.0, 0.0), (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (2.0, 0.0, 0.0))
fcall('Suelo_PointInTri vertex', sym('Suelo_PointInTri'), 'bFFFF', (1.0, 0.0, 0.0), (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
fcall('Suelo_SegPlane parallel', sym('Suelo_SegPlane'), 'bFFfFF', (0.0, 0.2, 0.0), (5.0, 0.2, 1.0), 0.5, (0.0, 1.0, 0.0, 0.0), (0.0,))
fcall('Suelo_SegPlane far', sym('Suelo_SegPlane'), 'bFFfFF', (0.0, 2.0, 0.0), (5.0, 3.0, 1.0), 0.5, (0.0, 1.0, 0.0, 0.0), (0.0,))
fcall('Suelo_SegEdge zero', sym('Suelo_SegEdge'), 'bFFFFfF', (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (0.0, 0.0, 0.0), (2.0, 2.0, 2.0), 0.5, (100.0,))
fcall('Suelo_SegEdge parallel', sym('Suelo_SegEdge'), 'bFFFFfF', (0.0, 0.0, 0.0), (4.0, 0.0, 0.0), (-1.0, 0.3, 0.0), (5.0, 0.3, 0.0), 0.5, (100.0,))
fcall('Suelo_Dist2PointSeg point', sym('Suelo_Dist2PointSeg'), 'fFFFF', (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (3.0, 1.0, 2.0), z3)
fcall('Suelo_DistPointSeg point', sym('Suelo_DistPointSeg'), 'fFFFF', (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (3.0, 1.0, 2.0), z3)

# collision meshes: CCollideMesh with its own vertex and triangle arrays (the triangles of tris: corners, flags)
def sumesh(verts, tris):
	m = sunew(0x34)
	check('CCollideMesh_ctor', lambda m=m: (SDTest.Call('CCollideMesh_ctor', 'tii', m)[0] == m, suwords(m, 13)))
	vp = sunew(12 * len(verts))
	f = ()
	for v in verts:
		f = f + v
	suwf(vp, f)
	tp = sunew(0x1c * len(tris))
	for i in range(len(tris)):
		t = tris[i]
		suwi(tp + 0x1c * i + 0x14, t[0] + (t[1] << 16))
		suwi(tp + 0x1c * i + 0x18, t[2] + (t[3] << 16))
	suwi(m + 0x24, len(verts))
	suwi(m + 0x28, vp)
	suwi(m + 0x2c, len(tris))
	suwi(m + 0x30, tp)
	for i in range(len(tris)):
		pl = SDTest.Call('CCollideMesh_TriPlane', 'tviFi', m, (0.0, 0.0, 0.0, 0.0), i)[1]
		check('CCollideMesh_TriPlane' + fmt((i,)), lambda pl=pl: pl)
		suwf(tp + 0x1c * i + 4, pl)
	return m
def sumtri(m, i):
	t = suri(m + 0x30) + 0x1c * i
	return (suri(t), surf(t + 4, 4), sushorts(t + 0x14, 4))
def sumget(m):
	r = (suwords(m, 3), surf(m + 0xc, 6), suri(m + 0x24))
	if suri(m + 0x28):
		r = r + (surf(suri(m + 0x28), 3 * suri(m + 0x24)),)
	r = r + (suri(m + 0x2c),)
	if suri(m + 0x30):
		for i in range(suri(m + 0x2c)):
			r = r + (sumtri(m, i),)
	return r

suverts = []
sutris = []
for i in range(12):
	a, b, c = sutri()
	if i == 3:
		c = (a[0] + (b[0] - a[0]) * 2.0, a[1] + (b[1] - a[1]) * 2.0, a[2] + (b[2] - a[2]) * 2.0)
	n = len(suverts)
	suverts.append(a)
	suverts.append(b)
	suverts.append(c)
	fl = 0
	if i == 5:
		fl = 1
	if i == 7:
		fl = 3
	sutris.append((n, n + 1, n + 2, fl))
sum1 = sumesh(suverts, sutris)
check('CCollideMesh mesh', lambda: sumget(sum1))
tbuf = sunew(0x1c)
vbuf = sunew(36)
for i in range(len(sutris)):
	check('CCollideMesh_TriWithPlane' + fmt((i,)), lambda i=i: (SDTest.Call('CCollideMesh_TriWithPlane', 'tviii', sum1, tbuf, i), suwords(tbuf, 1), surf(tbuf + 4, 4), sushorts(tbuf + 0x14, 4)))
	check('CCollideMesh_GetTri' + fmt((i,)), lambda i=i: (SDTest.Call('CCollideMesh_GetTri', 'tviiiiii', sum1, i, tbuf, vbuf, vbuf + 12, vbuf + 24), suwords(tbuf, 1), sushorts(tbuf + 0x14, 4), surf(vbuf, 9)))
	check('CCollideMesh_GetTri NULL' + fmt((i,)), lambda i=i: (SDTest.Call('CCollideMesh_GetTri', 'tviiiiii', sum1, i, 0, 0, vbuf + 12, 0), surf(vbuf, 9)))
# spheres and sweeps against each triangle
for k in range(25):
	for i in range(len(sutris)):
		t = suri(sum1 + 0x30) + 0x1c * i
		a, b, c = suverts[3 * i], suverts[3 * i + 1], suverts[3 * i + 2]
		m = sumid(a, b, c)
		r = rnd(0.05, 2.0)
		p = sunear(m, 2.5)
		q = sunear(m, 2.5)
		e = sunear(a, 1.0)
		fcall('Suelo_SphereTri', sym('Suelo_SphereTri'), 'bFfiiF', p, r, t, sum1, z3)
		fcall('Suelo_SphereTri edge', sym('Suelo_SphereTri'), 'bFfiiF', e, r, t, sum1, z3)
		fcall('Suelo_SweepTri', sym('Suelo_SweepTri'), 'bFFfiiF', p, q, r, t, sum1, z3)
		fcall('Suelo_SweepTri edge', sym('Suelo_SweepTri'), 'bFFfiiF', e, sunear(b, 1.0), r, t, sum1, z3)
		fcall('Suelo_SweepTri far', sym('Suelo_SweepTri'), 'bFFfiiF', sunear(m, 8.0), sunear(m, 8.0), r, t, sum1, z3)
		fcall('Suelo_SweepTri short', sym('Suelo_SweepTri'), 'bFFfiiF', p, sunear(p, 0.00002), r, t, sum1, z3)
		fcall('Suelo_SweepTri start', sym('Suelo_SweepTri'), 'bFFfiiF', sunear(m, 0.1), sunear(m, 3.0), r + 0.2, t, sum1, z3)

# floors: CSuelo with a grid over a box and meshes in _CollideMesh1[0] and [1]
def sumi(meshes, m):
	for i in range(len(meshes)):
		if meshes[i] == m:
			return i
	if m:
		return -2
	return -1
def suquad(q, meshes):
	n = suri(q + 0xc)
	r = (sumi(meshes, suri(q + 8)), surf(q, 2), n, surf(q + 0x14, 2), suri(q + 0x1c))
	if suri(q + 0x10):
		r = r + (sushorts(suri(q + 0x10), n),)
	return r
def suquads(alt, meshes):
	r = ()
	n = suri(alt + 0x3490)
	p = suri(alt + 0x3494)
	for i in range(n):
		q = p + 0x24 * i
		c = ()
		while q:
			c = c + (suquad(q, meshes),)
			q = suri(q + 0x20)
		r = r + (c,)
	return r
def sualt(alt):
	return (suri(alt), surf(alt + 0x346c, 9), suri(alt + 0x3490), suwords(alt + 0x3498, 2), surf(alt + 0x34a0, 4),
		suri(alt + 0x34b0) != 0)
def sucmesh(alt, i):
	return alt + 4 + 0x34 * i
# a CMallaTri of the exe with the triangles (corners, flags)
def sumalla(verts, tris):
	m = sunewop(0x4c)
	SDTest.Call('CMallaTri_ctor', 'tii', m)
	SDTest.Call('CMallaTri_SetNumVert', 'tviii', m, len(verts), 0)
	for i in range(len(verts)):
		SDTest.Call('CMallaTri_SetVert', 'tviiF', m, i, verts[i])
	SDTest.Call('CMallaTri_SetNumTri', 'tviii', m, len(tris), 0)
	for i in range(len(tris)):
		t = tris[i]
		SDTest.Call('CMallaTri_SetTriVerts', 'tviiiii', m, i, t[0], t[1], t[2])
		SDTest.Call('CMallaTri_SetTriFlags', 'tviii', m, i, t[3])
	return m
def suvecbox(lo, hi, ylo, yhi):
	return (rnd(lo, hi), rnd(ylo, yhi), rnd(lo, hi))

a = sunew(0x34b4)
check('CAlturas_ctor', lambda a=a: (SDTest.Call('CAlturas_ctor', 'tii', a)[0] == a, sualt(a), suwords(a + 4, 13), suwords(a + 0x3404, 13), suwords(a + 0x3438, 13), suwords(a + 0x1a04 + 0x34 * 127, 13)))
check('CAlturas_dtor', lambda a=a: (SDTest.Call('CAlturas_dtor', 'tvi', a), sualt(a), suglobals()))
s = sunewop(0x3524)
suzero(s, 0x3524)
check('CSuelo_ctor del', lambda s=s: (SDTest.Call('CSuelo_ctor', 'tii', s)[0] == s, suwords(s + 4, 0x1b), suglobals()))
check('CSuelo_Delete', lambda s=s: (SDTest.Call('CSuelo_Delete', 'tiii', s, 1)[0] == s, suglobals()))

su = sunew(0x3524)
check('CSuelo_ctor', lambda: (SDTest.Call('CSuelo_ctor', 'tii', su)[0] == su, suwords(su + 4, 0x1b), suglobals(), sualt(su + 0x70)))
sualt0 = su + 0x70
for f in ('CSuelo_NodosVf4', 'CSuelo_NodosVf3', 'CSuelo_NodosToD3D'):
	check('CSuelo nodes' + fmt((f,)), lambda f=f: SDTest.Call(f, 'tvii', su, 0))
check('CSuelo_Nop', lambda: SDTest.Call('CSuelo_Nop', 'tvii', su, 5))
box = sunew(24)
suwf(box, (-20.0, -5.0, -20.0, 20.0, 15.0, 20.0))
check('CAlturas_QuadInit', lambda: (SDTest.Call('CAlturas_QuadInit', 'tviiii', sualt0, 0, box, 40), sualt(sualt0), suglobals()))
# the floor (two triangles over the box), random triangles inside, one degenerate, one without collisions, one
# invalid
fv = [(-19.5, 0.0, -19.5), (19.5, 0.0, -19.5), (19.5, 0.0, 19.5), (-19.5, 0.0, 19.5)]
ft = [(0, 1, 2, 0x10), (0, 2, 3, 0x20)]
for i in range(30):
	n = len(fv)
	a = suvecbox(-19.0, 19.0, -4.0, 12.0)
	fv.append(a)
	fv.append(sunear(a, 4.0))
	if i == 4:
		fv.append(a)
	else:
		fv.append(sunear(a, 4.0))
	fl = 0
	if i == 6:
		fl = 1
	if i == 9:
		fl = 3
	ft.append((n, n + 1, n + 2, fl))
for i in range(len(fv)):
	v = fv[i]
	fv[i] = (max(-19.5, min(19.5, v[0])), v[1], max(-19.5, min(19.5, v[2])))
tm1 = sumalla(fv, ft)
mesh1 = sucmesh(sualt0, 0)
mesh2 = sucmesh(sualt0, 1)
mesh3 = sucmesh(sualt0, 2)
mlist = (mesh1, mesh2, mesh3)
check('CSuelo_AddMesh', lambda: (SDTest.Call('CSuelo_AddMesh', 'tviiiii', su, mesh1, tm1, 0, 0), sumget(mesh1), suglobals()))
check('CSuelo quads', lambda: suquads(sualt0, mlist))
# a mesh added at run time: MeshID 1
mv = []
mt = []
for i in range(8):
	n = len(mv)
	a = suvecbox(-15.0, 15.0, 0.0, 6.0)
	mv.append(a)
	mv.append(sunear(a, 3.0))
	mv.append(sunear(a, 3.0))
	mt.append((n, n + 1, n + 2, 0x40 * i))
tm2 = sumalla(mv, mt)
check('CSuelo_AddMesh id', lambda: (SDTest.Call('CSuelo_AddMesh', 'tviiipp', su, mesh2, tm2, 0, 0), sumget(mesh2), suri(sualt0), suglobals()))
check('CSuelo quads 2', lambda: suquads(sualt0, mlist))
check('CSuelo_SetMeshCollide off', lambda: (SDTest.Call('CSuelo_SetMeshCollide', 'tviiiii', su, mesh2, 0, 1, 0), suwords(mesh2, 2), suglobals()))
check('CSuelo_SetMeshCollide on', lambda: (SDTest.Call('CSuelo_SetMeshCollide', 'tviiiii', su, mesh2, 1, 1, 0), suwords(mesh2, 2), suglobals()))
check('CSuelo_BuildSectorMesh 1', lambda: (SDTest.Call('CSuelo_BuildSectorMesh', 'tviiii', su, mesh3, 1, 0), sumget(mesh3)))
check('CSuelo_BuildSectorMesh 2', lambda: (SDTest.Call('CSuelo_BuildSectorMesh', 'tviiii', su, mesh3, 2, 0), sumget(mesh3)))
check('CSuelo_CalcGaps', lambda: (SDTest.Call('CSuelo_CalcGaps', 'tvi', su), suquads(sualt0, mlist)))
check('CAlturas_SetParam', lambda: (SDTest.Call('CAlturas_SetParam', 'tvii', sualt0, 7), suwords(mesh1, 3), suwords(mesh2, 3)))
# the queries; the point lists are the floor triangles, random points near the triangles and anywhere
def supath():
	j = int(rnd(0.0, len(ft) - 0.001))
	t = ft[j]
	m = sumid(fv[t[0]], fv[t[1]], fv[t[2]])
	if rnd(0.0, 1.0) < 0.3:
		m = suvecbox(-22.0, 22.0, -6.0, 16.0)
	a = sunear(m, 2.0)
	b = sunear(a, rnd(0.0, 3.0))
	return a, b
suout = (9.0, 9.0, 9.0)
for k in range(150):
	a, b = supath()
	r = rnd(0.1, 2.5)
	check('CSuelo_CollideSphere' + fmt((k,)), lambda a=a, b=b, r=r: (SDTest.Call('CSuelo_CollideSphere', 'tbiFFfFFF', su, a, b, r, suout, suout, suout), suglobals()))
	check('CSuelo_CollideSphere NULL' + fmt((k,)), lambda a=a, b=b, r=r: SDTest.Call('CSuelo_CollideSphere', 'tbiFFfiii', su, a, b, r, 0, 0, 0))
	check('CSuelo_SphereContact' + fmt((k,)), lambda a=a, b=b, r=r: (SDTest.Call('CSuelo_SphereContact', 'tbiFFfFFF', su, a, b, r, suout, suout, suout), suglobals()))
	check('CSuelo_SphereContact NULL' + fmt((k,)), lambda a=a, b=b, r=r: SDTest.Call('CSuelo_SphereContact', 'tbiFFfiii', su, a, b, r, 0, 0, 0))
	check('CSuelo_SphereContact n' + fmt((k,)), lambda a=a, b=b, r=r: SDTest.Call('CSuelo_SphereContact', 'tbiFFfiiF', su, a, b, r, 0, 0, suout))
	check('CSuelo_SweepSphere' + fmt((k,)), lambda a=a, b=b, r=r: (SDTest.Call('CSuelo_SweepSphere', 'tbiFFfFFF', su, a, b, r, suout, suout, suout), suglobals()))
	check('CSuelo_SweepSphere NULL' + fmt((k,)), lambda a=a, b=b, r=r: SDTest.Call('CSuelo_SweepSphere', 'tbiFFfiii', su, a, b, r, 0, 0, 0))
	b2 = sunear(a, 12.0)
	check('CSuelo_SweepSphere long' + fmt((k,)), lambda a=a, b=b2, r=r: SDTest.Call('CSuelo_SweepSphere', 'tbiFFfFiF', su, a, b, r, suout, 0, suout))
# the tested and hit marks and query numbers
check('CSuelo after queries 1', lambda: sumget(mesh1))
check('CSuelo after queries 2', lambda: sumget(mesh2))
check('CSuelo_RemoveMesh', lambda: (SDTest.Call('CSuelo_RemoveMesh', 'tviiii', su, mesh2, 0, 0), suquads(sualt0, mlist), suwords(mesh2, 1), suglobals()))
# the quad functions on the quads of the grid and on a quad of their own
q0 = suri(sualt0 + 0x3494)
for i in range(suri(sualt0 + 0x3490)):
	q = q0 + 0x24 * i
	check('CCollideQuad_LowestTri' + fmt((i,)), lambda q=q: SDTest.Call('CCollideQuad_LowestTri', 'tii', q))
	for j in range(min(4, suri(q + 0xc))):
		t = suri(suri(q + 8) + 0x30) + 0x1c * (sushorts(suri(q + 0x10), j + 1)[j])
		fcall('CCollideQuad_TriMaxY' + fmt((i, j)), sym('CCollideQuad_TriMaxY'), 'tfii', q, t)
		fcall('CCollideQuad_TriMinY' + fmt((i, j)), sym('CCollideQuad_TriMinY'), 'tfii', q, t)
	check('CCollideQuad_CalcExtens' + fmt((i,)), lambda q=q: (SDTest.Call('CCollideQuad_CalcExtens', 'tvii', sualt0, q), suquad(q, mlist)))
	check('CCollideQuad_CalcGap' + fmt((i,)), lambda q=q: (SDTest.Call('CCollideQuad_CalcGap', 'tvi', q), suquad(q, mlist)))
qq = sunewop(0x24)
suzero(qq, 0x24)
check('CCollideQuad_ctor', lambda: (SDTest.Call('CCollideQuad_ctor', 'tii', qq)[0] == qq, suwords(qq, 9), suglobals()))
check('CCollideQuad_Find new', lambda: (SDTest.Call('CCollideQuad_Find', 'tiiiii', sualt0, qq, 0, mesh1)[0] == qq, suquad(qq, mlist), suwords(mesh1, 1)))
check('CCollideQuad_Find same', lambda: (SDTest.Call('CCollideQuad_Find', 'tiiiii', sualt0, qq, 0, mesh1)[0] == qq, suquad(qq, mlist)))
check('CCollideQuad_Find next', lambda: (SDTest.Call('CCollideQuad_Find', 'tiiiii', sualt0, qq, 1, mesh2)[0] == suri(qq + 0x20), suquad(suri(qq + 0x20), mlist), suwords(mesh2, 1), suglobals()))
check('CCollideQuad_Find next again', lambda: (SDTest.Call('CCollideQuad_Find', 'tiiiii', sualt0, qq, 1, mesh2)[0] == suri(qq + 0x20), suwords(mesh2, 1)))
check('CCollideQuad_AddAllTris', lambda: (SDTest.Call('CCollideQuad_AddAllTris', 'tvii', sualt0, qq), suquad(qq, mlist)))
check('CCollideQuad_CalcExtens own', lambda: (SDTest.Call('CCollideQuad_CalcExtens', 'tvii', sualt0, qq), suquad(qq, mlist)))
check('CCollideQuad_Delete', lambda: (SDTest.Call('CCollideQuad_Delete', 'tiii', qq, 1)[0] == qq, suwords(mesh1, 1), suwords(mesh2, 1), suglobals()))
# the wrap of the query number clears the tested marks
SDTest.Poke('g_CollisionNComp', -1)
check('CSuelo_NewQuery wrap', lambda: (SDTest.Call('CSuelo_NewQuery', 'tvi', su), suglobals(), suwords(mesh1, 3), sumtri(mesh1, 0), sumtri(mesh1, 5)))
check('CSuelo_NewQuery', lambda: (SDTest.Call('CSuelo_NewQuery', 'tvi', su), suglobals()))
# the tables of tabla.h
tb = sunew(8)
check('Tabla_Free', lambda: (SDTest.Call('Tabla_Alloc2', 'tvii', tb, 5), SDTest.Call('Tabla_Free@2589c0', 'tvi', tb), suwords(tb, 2)))
check('Tabla_Free empty', lambda: (SDTest.Call('Tabla_Free@2589c0', 'tvi', tb), suwords(tb, 2)))
check('Tabla_FreeObjs', lambda: (SDTest.Call('Tabla_NewFC', 'tvii', tb, 3), SDTest.Call('Tabla_FreeObjs', 'tvi', tb), suwords(tb, 2)))
check('Tabla_FreeObjs empty', lambda: (SDTest.Call('Tabla_FreeObjs', 'tvi', tb), suwords(tb, 2)))
surestore()
