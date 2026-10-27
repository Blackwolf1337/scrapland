# engine3d Engine/CheckLine3D.c and CheckBBox.c (execfile'd by sdlevel.py, Python 1.5.2): the segment and box
# queries against the floor of FZ, segments and boxes around the player (pp) and far away, the cell walks, and the
# pure parts (point in triangle, box against a fake triangle, the polygon clip, the oriented box) on fixed inputs.
def en1i(a):
	return SDTest.Peek(a, '@i')
def en1z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def en1fl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def en1set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def en1add(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
def en1poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def en1r03(r):
	return (r[0], r[3])
def en1r04(r):
	return (r[0], r[4])

import math
en1mundo = SDTest.Peek(0x4afde8, 'i')
en1suelo = 0
if en1mundo:
	en1suelo = en1i(en1mundo + 0xb0)
check('en1 suelo', lambda: en1suelo != 0)
en1p = (pp[0], pp[1], pp[2])
en1segs = [(en1p, en1add(en1p, (0, -5000.0, 0))), (en1add(en1p, (0, -5000.0, 0)), en1p),
	(en1p, en1add(en1p, (3000.0, 0, 0))), (en1add(en1p, (0, 300.0, 0)), en1add(en1p, (0, -300.0, 0))),
	(en1add(en1p, (-800.0, -40.0, 600.0)), en1add(en1p, (900.0, -60.0, -700.0))), (en1p, en1p),
	(en1add(en1p, (20000.0, 0, 0)), en1add(en1p, (20000.0, -5000.0, 0))),
	(en1add(en1p, (60000.0, 100.0, 1000.0)), en1add(en1p, (0, -500.0, 0))),
	(en1add(en1p, (-300.0, 2000.0, -300.0)), en1add(en1p, (300.0, -2000.0, 300.0))),
	(en1add(en1p, (0, 50.0, 0)), en1add(en1p, (-4000.0, -100.0, 2500.0)))]
for en1k in range(8):
	en1segs.append((en1add(en1p, (en1k * 137 % 400 - 200.0, 400.0, en1k * 91 % 300 - 150.0)),
		en1add(en1p, (en1k * 53 % 200 - 100.0, -900.0, en1k * 71 % 260 - 130.0))))

if en1suelo:
	en1alt = en1suelo + 0x70
	en1info = en1z(0x10)
	en1cache = en1z(0x10)
	def en1hit(c):
		return (en1i(c) == SDTest.Peek(0x4af7d8, 'i'), en1i(c + 4), en1i(c + 8) != 0)
	def en1stats():
		return (en1i(en1suelo + 8), en1i(en1suelo + 0xc), en1i(en1suelo + 0x10))
	def en1delta(fn):
		s0 = en1stats()
		r = fn()
		s1 = en1stats()
		return (r, s1[0] - s0[0], s1[1] - s0[1], s1[2] - s0[2], SDTest.Peek(0x4af7ec, 'i'))
	en1k = 0
	for en1a, en1b in en1segs:
		en1n = fmt((en1k,))
		for en1f in (0, 1):
			check('en1 CSuelo_TestSeg' + en1n + fmt((en1f,)), lambda a=en1a, b=en1b, f=en1f: (SDTest.Call(0x26ed30,
				'tbiFFi', en1suelo, a, b, f)[0], SDTest.Peek(0x4af7ec, 'i')))
			check('en1 CSuelo_TraceSegPos' + en1n + fmt((en1f,)), lambda a=en1a, b=en1b, f=en1f:
				(en1r03(SDTest.Call(0x26df80, 'tbiFFFi', en1suelo, a, b, (9.0, 9.0, 9.0), f)), SDTest.Peek(0x4af7ec, 'i')))
		for en1f in (0, 1, 2):
			def en1ts(a=en1a, b=en1b, f=en1f):
				SDTest.Poke(en1info - 0x400000, 0)
				en1poke(en1info + 4, -1)
				en1poke(en1info + 8, 0)
				r = SDTest.Call(0x26f120, 'tbiFFFii', en1suelo, a, b, (9.0, 9.0, 9.0), en1info, f)
				return (r[0], r[3], en1hit(en1info), SDTest.Peek(0x4af7ec, 'i'))
			check('en1 CSuelo_TraceSeg' + en1n + fmt((en1f,)), en1ts)
		for en1u in (0.0, 5.0, 1000.0):
			def en1tr(a=en1a, b=en1b, u=en1u):
				en1poke(en1cache, 0)
				en1poke(en1cache + 4, 0)
				en1poke(en1cache + 8, 0)
				r1 = en1delta(lambda a=a, b=b, u=u: SDTest.Call(0x26e490, 'tbiFFifi', en1suelo, a, b, 0, u,
					en1cache)[0])
				h1 = en1hit(en1cache)
				r2 = en1delta(lambda a=a, b=b, u=u: SDTest.Call(0x26e490, 'tbiFFifi', en1suelo, a, b, 0, u,
					en1cache)[0])
				r3 = en1delta(lambda a=a, u=u: SDTest.Call(0x26e490, 'tbiFFifi', en1suelo, a, en1add(a, (0, 1.0, 0)),
					0, u, en1cache)[0])
				return (r1, h1, r2, r3, en1hit(en1cache))
			check('en1 CSuelo_Trace' + en1n + fmt((en1u,)), en1tr)
		en1k = en1k + 1
	# the own cache of the floor (cleared first; the game uses it between frames)
	en1sc = (en1i(en1suelo + 0x14), en1i(en1suelo + 0x18), en1i(en1suelo + 0x1c))
	en1poke(en1suelo + 0x1c, 0)
	for en1a, en1b in en1segs[:5]:
		check('en1 CSuelo_Trace own' + fmt((en1a, en1b)), lambda a=en1a, b=en1b: (en1delta(lambda a=a, b=b:
			SDTest.Call(0x26e490, 'tbiFFifi', en1suelo, a, b, 0, 0.0, 0)[0]), en1hit(en1suelo + 0x14)))
	en1poke(en1suelo + 0x14, en1sc[0])
	en1poke(en1suelo + 0x18, en1sc[1])
	en1poke(en1suelo + 0x1c, en1sc[2])

	# the cell walks
	en1g = en1fl(en1alt + 0x346c, 3)
	en1inv = en1fl(en1alt + 0x34a8, 2)
	check('en1 grid', lambda: (en1g, en1fl(en1alt + 0x34a0, 4), en1i(en1alt + 0x3498), en1i(en1alt + 0x349c)))
	en1cx = int((en1p[0] - en1g[0]) * en1inv[0])
	en1cz = int((en1p[2] - en1g[2]) * en1inv[1])
	for en1a, en1b in en1segs:
		check('en1 Alturas_CellStep' + fmt((en1a, en1b)), lambda a=en1a, b=en1b: SDTest.Call(0x26f840, 'bippFFF',
			en1alt, en1cx, en1cz, a, b, (5.0,)))
		check('en1 Alturas_CellStep NULL' + fmt((en1a, en1b)), lambda a=en1a, b=en1b: SDTest.Call(0x26f840,
			'bippFFi', en1alt, en1cx + 1, en1cz - 1, a, b, 0)[:3])
	en1ts0 = en1fl(0x8b11b0, 12)
	en1ts1 = (en1i(0x8b11b8), en1i(0x8b11bc))
	for en1a, en1b in en1segs:
		def en1step(a=en1a, b=en1b):
			r = []
			dx = a[0] - b[0]
			dz = a[2] - b[2]
			ix = 3.4028234663852886e+38
			iz = ix
			if dx != 0.0:
				ix = 1.0 / dx
			if dz != 0.0:
				iz = 1.0 / dz
			en1set(0x8b11b0, (ix, iz, 0.0, 0.0) + a + b)
			en1poke(0x8b11b8, int((a[0] - en1g[0]) * en1inv[0]))
			en1poke(0x8b11bc, int((a[2] - en1g[2]) * en1inv[1]))
			for i in range(6):
				r.append((SDTest.Call(0x26f9a0, 'bi', en1alt)[0], en1i(0x8b11b8), en1i(0x8b11bc)))
			return r
		check('en1 Alturas_TraceStep' + fmt((en1a, en1b)), en1step)
	en1set(0x8b11b0, en1ts0)
	en1poke(0x8b11b8, en1ts1[0])
	en1poke(0x8b11bc, en1ts1[1])

	# boxes against the floor: the query of checkbbox.cpp (usar_minmax 1 and 2)
	en1col = en1z(0x80)
	for en1c in (en1p, en1add(en1p, (0, -150.0, 0)), en1add(en1p, (0, 400.0, 0)), en1add(en1p, (25000.0, 0, 0)),
		en1add(en1p, (-1500.0, -100.0, 900.0))):
		for en1lo, en1hi in (((-10.0, -10.0, -10.0), (10.0, 10.0, 10.0)), ((-1.0, -500.0, -1.0), (1.0, 0.0, 1.0)),
			((-300.0, -300.0, -300.0), (300.0, 300.0, 300.0)), ((-40.0, -2.0, -60.0), (35.0, 2.0, 55.0))):
			for en1u in (1, 2):
				def en1bb(c=en1c, lo=en1lo, hi=en1hi, u=en1u):
					en1set(en1col + 0x18, c + (0.0,) * 12)
					if u == 1:
						en1set(en1col + 0x24, lo + hi)
					else:
						en1set(en1col + 0x3c, lo + hi)
					en1poke(en1col + 0x54, u)
					r = SDTest.Call(0x26fb10, 'tbii', en1suelo, en1col)[0]
					return (r, SDTest.Peek(0x4b11d8, 'i'), SDTest.Peek(0x4b11dc, 'i'), SDTest.Peek(0x4af7ec, 'i'))
				check('en1 CSuelo_CheckBBox' + fmt((en1c, en1lo, en1hi, en1u)), en1bb)
	# oriented boxes against the floor, the debug box through the D3D log
	def en1log(fn):
		SDTest.Out()
		SDTest.D3DLog(1)
		r = fn()
		SDTest.D3DLog(0)
		return (r, SDTest.Out())
	for en1c in (en1p, en1add(en1p, (0, -120.0, 0)), en1add(en1p, (25000.0, 0, 0)), en1add(en1p, (300.0, -60.0, 200.0))):
		for en1rot in ((1.0, 0.0), (0.70710677, 0.70710677), (0.0, 1.0)):
			en1m = (en1rot[0], 0.0, -en1rot[1], 0.0, 0.0, 1.0, 0.0, 0.0, en1rot[1], 0.0, en1rot[0], 0.0) + en1c + (1.0,)
			for en1h in ((20.0, 20.0, 20.0), (200.0, 50.0, 120.0), (5.0, 300.0, 5.0)):
				check('en1 CSuelo_CollideOBB' + fmt((en1c, en1rot, en1h)), lambda m=en1m, h=en1h: (en1log(lambda m=m,
					h=h: SDTest.Call(0x271590, 'tbiFFFF', en1suelo, m, h, (7.0, 7.0, 7.0), (8.0, 8.0, 8.0))),
					SDTest.Call(0x271590, 'tbiFFii', en1suelo, m, h, 0, 0)[0], SDTest.Peek(0x4af7ec, 'i')))

# the pure parts
en1tris = (((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 0.0, 10.0)), ((0.0, 0.0, 0.0), (10.0, 5.0, 0.0), (0.0, 5.0, 10.0)),
	((1.0, 2.0, 3.0), (1.0, 2.0, 3.0), (4.0, 5.0, 6.0)), ((-5.0, 1.0, -5.0), (5.0, -1.0, -5.0), (0.0, 0.0, 8.0)))
en1pts = ((1.0, 0.0, 1.0), (5.0, 0.0, 5.0), (0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (-1.0, 0.0, 1.0), (3.0, 2.0, 3.0),
	(2.5, 2.5, 2.5), (1.0, 2.0, 3.0), (2.0, 3.0, 4.0), (0.0, 0.5, 0.0))
for en1t in en1tris:
	for en1q in en1pts:
		check('en1 Suelo_PointInTriIn' + fmt((en1t, en1q)), lambda t=en1t, q=en1q: (SDTest.Call(0x26f660, 'bFFFF', q,
			t[0], t[1], t[2])[0], SDTest.Call(0x26f660, 'bFFFF', q, t[0], t[2], t[1])[0]))

# BoxTri on a fake triangle (verts 0 1 2) with both plane sides
en1v = en1z(36)
en1tri = en1z(0x1c)
def en1plane(t, s):
	a = (t[1][0] - t[0][0], t[1][1] - t[0][1], t[1][2] - t[0][2])
	b = (t[2][0] - t[0][0], t[2][1] - t[0][1], t[2][2] - t[0][2])
	n = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
	l = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
	if l == 0.0:
		l = 1.0
	n = (s * n[0] / l, s * n[1] / l, s * n[2] / l)
	return n + (-(n[0] * t[0][0] + n[1] * t[0][1] + n[2] * t[0][2]),)
en1boxes = (((-1.0, -1.0, -1.0), (1.0, 1.0, 1.0)), ((2.0, -1.0, 2.0), (3.0, 1.0, 3.0)), ((4.0, -2.0, -3.0),
	(6.0, 2.0, 3.0)), ((20.0, 20.0, 20.0), (30.0, 30.0, 30.0)), ((-100.0, -100.0, -100.0), (100.0, 100.0, 100.0)),
	((1.0, 0.5, 1.0), (2.0, 0.6, 2.0)), ((-2.0, 3.0, -2.0), (12.0, 4.0, 12.0)), ((2.0, 0.0, 2.0), (3.0, 0.0, 3.0)),
	((9.0, -1.0, -1.0), (11.0, 1.0, 0.5)), ((4.9, 4.9, 4.9), (5.1, 5.1, 5.1)))
for en1t in en1tris:
	en1set(en1v, en1t[0] + en1t[1] + en1t[2])
	for en1s in (1.0, -1.0):
		en1set(en1tri, (0.0,) + en1plane(en1t, en1s))
		en1poke(en1tri + 0x14, 0x10000)
		en1poke(en1tri + 0x18, 2)
		for en1lo, en1hi in en1boxes:
			check('en1 BoxTri' + fmt((en1t, en1s, en1lo, en1hi)), lambda lo=en1lo, hi=en1hi: SDTest.Call(0x271d60,
				'bFFii', lo, hi, en1tri, en1v)[0])

# the polygon clip: a square, a triangle, two points, against some planes
en1polys = (((0.0, 0.0, 0.0), (4.0, 0.0, 0.0), (4.0, 0.0, 4.0), (0.0, 0.0, 4.0)), ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0),
	(0.0, 3.0, 10.0)), ((0.0, 0.0, 0.0), (5.0, 1.0, 0.0)), ((1.0, 1.0, 1.0),), ((-3.0, 0.0, -3.0), (3.0, 0.0, -3.0),
	(5.0, 0.0, 0.0), (3.0, 0.0, 3.0), (-3.0, 0.0, 3.0), (-5.0, 0.0, 0.0)))
en1planes = (((1.0, 0.0, 0.0), 2.0), ((-1.0, 0.0, 0.0), -2.0), ((0.0, 0.0, 1.0), 1.0), ((0.70710677, 0.0, 0.70710677),
	3.0), ((1.0, 0.0, 0.0), 20.0), ((1.0, 0.0, 0.0), -20.0), ((0.0, 1.0, 0.0), 0.0), ((0.6, 0.8, 0.0), 1.5),
	((1.0, 0.0, 0.0), 0.0))
for en1poly in en1polys:
	en1flat = ()
	for en1q in en1poly:
		en1flat = en1flat + en1q
	en1flat = en1flat + (0.5,) * (36 - len(en1flat))
	for en1n, en1d in en1planes:
		check('en1 Poly_ClipPlane' + fmt((en1poly, en1n, en1d)), lambda n=en1n, d=en1d, c=len(en1poly), f=en1flat:
			SDTest.Call(0x2723e0, 'vFfpF', n, d, c, f)[2:])

# the oriented box: clip of triangles and the depth of points
for en1rot in ((1.0, 0.0), (0.70710677, 0.70710677), (0.8, -0.6)):
	for en1c in ((0.0, 0.0, 0.0), (3.0, 1.0, -2.0)):
		en1m = (en1rot[0], 0.0, -en1rot[1], 0.0, 0.0, 1.0, 0.0, 0.0, en1rot[1], 0.0, en1rot[0], 0.0) + en1c + (1.0,)
		for en1h in ((2.0, 2.0, 2.0), (5.0, 0.5, 1.0), (0.1, 0.1, 0.1)):
			for en1t in en1tris:
				check('en1 Box_ClipTri' + fmt((en1m, en1h, en1t)), lambda m=en1m, h=en1h, t=en1t:
					en1r04(SDTest.Call(0x271b00, 'iFFFF', m, h, t[0] + t[1] + t[2], (7.0, 7.0, 7.0))))
			for en1q in en1pts:
				check('en1 Box_PointDepth' + fmt((en1m, en1h, en1q)), lambda m=en1m, h=en1h, q=en1q:
					SDTest.Call(0x272d70, 'fFFF', m, h, q)[0])
