# engine3d Engine/CheckLine3D.c and CheckBBox.c (execfile'd by sdlevel.py, Python 1.5.2): the segment and box
# queries against the floor of FZ, segments and boxes around the player (pp) and far away, the cell walks, and the
# pure parts (point in triangle, box against a fake triangle, the polygon clip, the oriented box) on fixed inputs.
def en1i(a):
	return SDTest.Peek(a, '@i')
def en1z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def en1fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def en1set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def en1add(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
def en1poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def en1r03(r):
	return (r[0], r[3])
def en1r04(r):
	return (r[0], r[4])

import math
en1mundo = SDTest.Peek('g_World', 'i')
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
		return (en1i(c) == SDTest.Peek('g_QuadTriCacheID', 'i'), en1i(c + 4), en1i(c + 8) != 0)
	def en1stats():
		return (en1i(en1suelo + 8), en1i(en1suelo + 0xc), en1i(en1suelo + 0x10))
	def en1delta(fn):
		s0 = en1stats()
		r = fn()
		s1 = en1stats()
		return (r, s1[0] - s0[0], s1[1] - s0[1], s1[2] - s0[2], SDTest.Peek('g_CollisionTests', 'i'))
	en1k = 0
	for en1a, en1b in en1segs:
		en1n = fmt((en1k,))
		for en1f in (0, 1):
			check('en1 CGround_TestSeg' + en1n + fmt((en1f,)), lambda a=en1a, b=en1b, f=en1f: (SDTest.Call('CGround_TestSeg',
				'tbiFFi', en1suelo, a, b, f)[0], SDTest.Peek('g_CollisionTests', 'i')))
			check('en1 CGround_TraceSegPos' + en1n + fmt((en1f,)), lambda a=en1a, b=en1b, f=en1f:
				(en1r03(SDTest.Call('CGround_TraceSegPos', 'tbiFFFi', en1suelo, a, b, (9.0, 9.0, 9.0), f)), SDTest.Peek('g_CollisionTests', 'i')))
		for en1f in (0, 1, 2):
			def en1ts(a=en1a, b=en1b, f=en1f):
				SDTest.Poke(en1info - 0x400000, 0)
				en1poke(en1info + 4, -1)
				en1poke(en1info + 8, 0)
				r = SDTest.Call('CGround_TraceSeg', 'tbiFFFii', en1suelo, a, b, (9.0, 9.0, 9.0), en1info, f)
				return (r[0], r[3], en1hit(en1info), SDTest.Peek('g_CollisionTests', 'i'))
			check('en1 CGround_TraceSeg' + en1n + fmt((en1f,)), en1ts)
		for en1u in (0.0, 5.0, 1000.0):
			def en1tr(a=en1a, b=en1b, u=en1u):
				en1poke(en1cache, 0)
				en1poke(en1cache + 4, 0)
				en1poke(en1cache + 8, 0)
				r1 = en1delta(lambda a=a, b=b, u=u: SDTest.Call('CGround_Trace', 'tbiFFifi', en1suelo, a, b, 0, u,
					en1cache)[0])
				h1 = en1hit(en1cache)
				r2 = en1delta(lambda a=a, b=b, u=u: SDTest.Call('CGround_Trace', 'tbiFFifi', en1suelo, a, b, 0, u,
					en1cache)[0])
				r3 = en1delta(lambda a=a, u=u: SDTest.Call('CGround_Trace', 'tbiFFifi', en1suelo, a, en1add(a, (0, 1.0, 0)),
					0, u, en1cache)[0])
				return (r1, h1, r2, r3, en1hit(en1cache))
			check('en1 CGround_Trace' + en1n + fmt((en1u,)), en1tr)
		en1k = en1k + 1
	# the own cache of the floor (cleared first; the game uses it between frames)
	en1sc = (en1i(en1suelo + 0x14), en1i(en1suelo + 0x18), en1i(en1suelo + 0x1c))
	en1poke(en1suelo + 0x1c, 0)
	for en1a, en1b in en1segs[:5]:
		check('en1 CGround_Trace own' + fmt((en1a, en1b)), lambda a=en1a, b=en1b: (en1delta(lambda a=a, b=b:
			SDTest.Call('CGround_Trace', 'tbiFFifi', en1suelo, a, b, 0, 0.0, 0)[0]), en1hit(en1suelo + 0x14)))
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
		check('en1 Heights_CellStep' + fmt((en1a, en1b)), lambda a=en1a, b=en1b: SDTest.Call('Heights_CellStep', 'bippFFF',
			en1alt, en1cx, en1cz, a, b, (5.0,)))
		check('en1 Heights_CellStep NULL' + fmt((en1a, en1b)), lambda a=en1a, b=en1b: SDTest.Call('Heights_CellStep',
			'bippFFi', en1alt, en1cx + 1, en1cz - 1, a, b, 0)[:3])
	en1ts0 = en1fl(symva('g_TraceCell'), 10) + en1fl(symva('g_NTortas'), 1) + en1fl(symva('g_NResponseFaces'), 1)
	en1ts1 = (en1i(symva('g_TraceCell') + 0x8), en1i(symva('g_TraceCell') + 0xc))
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
			en1set(symva('g_TraceCell'), (ix, iz, 0.0, 0.0) + a + b)
			en1poke(symva('g_TraceCell') + 0x8, int((a[0] - en1g[0]) * en1inv[0]))
			en1poke(symva('g_TraceCell') + 0xc, int((a[2] - en1g[2]) * en1inv[1]))
			for i in range(6):
				r.append((SDTest.Call('Heights_TraceStep', 'bi', en1alt)[0], en1i(symva('g_TraceCell') + 0x8), en1i(symva('g_TraceCell') + 0xc)))
			return r
		check('en1 Heights_TraceStep' + fmt((en1a, en1b)), en1step)
	en1set(symva('g_TraceCell'), en1ts0[:10])
	en1set(symva('g_NTortas'), en1ts0[10:11])
	en1set(symva('g_NResponseFaces'), en1ts0[11:])
	en1poke(symva('g_TraceCell') + 0x8, en1ts1[0])
	en1poke(symva('g_TraceCell') + 0xc, en1ts1[1])

	# boxes against the floor: the query of checkbbox.cpp (use_minmax 1 and 2)
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
					r = SDTest.Call('CGround_CheckBBox', 'tbii', en1suelo, en1col)[0]
					return (r, SDTest.Peek('g_NTortas', 'i'), SDTest.Peek('g_NResponseFaces', 'i'), SDTest.Peek('g_CollisionTests', 'i'))
				check('en1 CGround_CheckBBox' + fmt((en1c, en1lo, en1hi, en1u)), en1bb)
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
				check('en1 CGround_CollideOBB' + fmt((en1c, en1rot, en1h)), lambda m=en1m, h=en1h: (en1log(lambda m=m,
					h=h: SDTest.Call('CGround_CollideOBB', 'tbiFFFF', en1suelo, m, h, (7.0, 7.0, 7.0), (8.0, 8.0, 8.0))),
					SDTest.Call('CGround_CollideOBB', 'tbiFFii', en1suelo, m, h, 0, 0)[0], SDTest.Peek('g_CollisionTests', 'i')))

# the pure parts
en1tris = (((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 0.0, 10.0)), ((0.0, 0.0, 0.0), (10.0, 5.0, 0.0), (0.0, 5.0, 10.0)),
	((1.0, 2.0, 3.0), (1.0, 2.0, 3.0), (4.0, 5.0, 6.0)), ((-5.0, 1.0, -5.0), (5.0, -1.0, -5.0), (0.0, 0.0, 8.0)))
en1pts = ((1.0, 0.0, 1.0), (5.0, 0.0, 5.0), (0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (-1.0, 0.0, 1.0), (3.0, 2.0, 3.0),
	(2.5, 2.5, 2.5), (1.0, 2.0, 3.0), (2.0, 3.0, 4.0), (0.0, 0.5, 0.0))
for en1t in en1tris:
	for en1q in en1pts:
		check('en1 Ground_PointInTriIn' + fmt((en1t, en1q)), lambda t=en1t, q=en1q: (SDTest.Call('Ground_PointInTriIn', 'bFFFF', q,
			t[0], t[1], t[2])[0], SDTest.Call('Ground_PointInTriIn', 'bFFFF', q, t[0], t[2], t[1])[0]))

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
			check('en1 BoxTri' + fmt((en1t, en1s, en1lo, en1hi)), lambda lo=en1lo, hi=en1hi: SDTest.Call('BoxTri',
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
			SDTest.Call('Poly_ClipPlane', 'vFfpF', n, d, c, f)[2:])

# the oriented box: clip of triangles and the depth of points
for en1rot in ((1.0, 0.0), (0.70710677, 0.70710677), (0.8, -0.6)):
	for en1c in ((0.0, 0.0, 0.0), (3.0, 1.0, -2.0)):
		en1m = (en1rot[0], 0.0, -en1rot[1], 0.0, 0.0, 1.0, 0.0, 0.0, en1rot[1], 0.0, en1rot[0], 0.0) + en1c + (1.0,)
		for en1h in ((2.0, 2.0, 2.0), (5.0, 0.5, 1.0), (0.1, 0.1, 0.1)):
			for en1t in en1tris:
				check('en1 Box_ClipTri' + fmt((en1m, en1h, en1t)), lambda m=en1m, h=en1h, t=en1t:
					en1r04(SDTest.Call('Box_ClipTri', 'iFFFF', m, h, t[0] + t[1] + t[2], (7.0, 7.0, 7.0))))
			for en1q in en1pts:
				check('en1 Box_PointDepth' + fmt((en1m, en1h, en1q)), lambda m=en1m, h=en1h, q=en1q:
					SDTest.Call('Box_PointDepth', 'fFFF', m, h, q)[0])

# Engine/SueloPortal.cpp: the rectangle constructor, the viewport of a seen sector and the debug drawing, through the
# D3D log (the projection and the viewport set back by Portals_SetView(NULL))
en1zp = en1z(0x20)
for en1k in range(8):
	en1poke(en1zp + 4 * en1k, 0x11111111)
check('en1 PaintZone_ctor', lambda: (SDTest.Call('PaintZone_ctor', 'tii', en1zp)[0] == en1zp, map(en1i, (en1zp, en1zp + 4,
	en1zp + 8, en1zp + 0xc, en1zp + 0x10, en1zp + 0x14, en1zp + 0x18, en1zp + 0x1c))))
en1pv = SDTest.Peek('g_PortalClip', 'i')
en1sp = SDTest.Peek('g_ShowPortals', 'i')
en1pad = SDTest.Peek('g_PortalBand', 'i')
def en1lg(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
# the viewport size is 0 while the script runs (a CHECK in Render_GetViewportW)
en1vw = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
for en1v in (0, 1):
	SDTest.Poke('g_PortalClip', en1v)
	for en1pd in (0, 3):
		SDTest.Poke('g_PortalBand', en1pd)
		for en1r in ((-1.0, -1.0, 0.0, 1.0, 1.0, 1.0), (-0.5, -0.25, 0.1, 0.5, 0.75, 0.9), (0.3, 0.3, 0.0, 0.31, 0.3, 1.0),
			(-2.0, -3.0, 0.0, 3.0, 2.0, 1.0), (0.9, -0.95, 0.0, 1.5, -0.9, 1.0)):
			en1poke(en1zp, 7)
			en1set(en1zp + 4, en1r)
			check('en1 Portals_SetView' + fmt((en1v, en1pd, en1r)), lambda: en1lg(lambda: (SDTest.Call('Portals_SetView',
				'bi', en1zp)[0], SDTest.Call('Portals_SetView', 'bi', 0)[0])))
SDTest.Poke('g_PortalBand', en1pad)
SDTest.Poke('g_PortalClip', en1pv)
en1nzp = SDTest.Peek('g_NumPaintZones', 'i')
en1zps = en1fl(symva('g_PaintZones'), 24)
en1set(symva('g_PaintZones'), (0.0, -1.0, -1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, -0.5, -0.5, 0.0, 0.25, 0.5, 1.0, 0.0, 0.0, 0.1, 0.2,
	0.0, 0.3, 0.4, 0.5, 0.0))
en1poke(symva('g_PaintZones'), 0)
en1poke(symva('g_PaintZones') + 0x1c, 1)
en1poke(symva('g_PaintZones') + 0x20, 3)
en1poke(symva('g_PaintZones') + 0x3c, 2)
en1poke(symva('g_PaintZones') + 0x40, 5)
en1poke(symva('g_PaintZones') + 0x5c, 1)
SDTest.Poke('g_NumPaintZones', 3)
for en1v in (0, 1, 2, 3):
	SDTest.Poke('g_ShowPortals', en1v)
	check('en1 Portals_DebugDraw' + fmt((en1v,)), lambda: en1lg(lambda: (SDTest.Call('Portals_DebugDraw', 'v'),)))
SDTest.Poke('g_ShowPortals', en1sp)
SDTest.Poke('g_NumPaintZones', en1nzp)
en1set(symva('g_PaintZones'), en1zps)
SDTest.Poke('g_ViewportW', en1vw[0])
SDTest.Poke('g_ViewportH', en1vw[1])

# Engine/stencil.cpp: the capture switch, the shadow light of a fake light, the debug line, the switch with the frame
# state it clears, and the buffers made again (through the D3D log)
en1sg = (symva('g_ShadowsStencil'), symva('g_StencilCapture'), symva('g_StencilMultFrame'), symva('g_ShadowLight'), symva('g_ShadowLightType'), symva('g_ShadowLightPos'), symva('g_ShadowLightPos') + 0x4, symva('g_ShadowLightPos') + 0x8, symva('g_ShadowLightDir'), symva('g_ShadowLightDir') + 0x4,
	symva('g_ShadowLightDir') + 0x8, symva('g_ShadowRange'), symva('g_ShadowCos'), symva('g_StencilRender'), symva('g_RenderClassMask'), symva('g_RenderFlagMask'), symva('g_RenderPass'), symva('g_StencilSegs'), symva('g_StencilInv'), symva('g_StencilGroups'))
en1ss = map(en1i, en1sg)
en1s4ec = SDTest.Peek('g_RenderFlagNeg', 'b')
def en1st():
	return (en1i(symva('g_ShadowsStencil')), en1i(symva('g_StencilCapture')), en1fl(symva('g_StencilMultFrame'), 1), en1i(symva('g_ShadowLight')) != 0, en1i(symva('g_ShadowLightType')),
		en1fl(symva('g_ShadowLightPos'), 3), en1fl(symva('g_ShadowLightDir'), 3), en1fl(symva('g_ShadowRange'), 1), en1fl(symva('g_ShadowCos'), 1), en1i(symva('g_StencilRender')),
		en1i(symva('g_RenderClassMask')), en1i(symva('g_RenderFlagMask')), SDTest.Peek('g_RenderFlagNeg', 'b'), en1i(symva('g_RenderPass')), en1i(symva('g_StencilSegs')), en1i(symva('g_StencilInv')),
		en1i(symva('g_StencilGroups')))
en1roff = SDTest.Peek('g_RenderOff', 'i')
for en1v in ((0, 0), (1, 0), (1, 1), (0, 1)):
	en1poke(symva('g_RenderOff'), (en1roff & ~0xff) | en1v[0])
	en1poke(symva('g_ShadowsStencil'), en1v[1])
	for en1on in (0, 1, 7):
		check('en1 ShadowsStencil_Capture' + fmt((en1v, en1on)), lambda on=en1on: (SDTest.Call('ShadowsStencil_Capture', 'vi', on),
			en1i(symva('g_StencilCapture')), en1fl(symva('g_StencilMultFrame'), 1)))
en1poke(symva('g_RenderOff'), en1roff)
en1luz = en1z(0xcc)
for en1t in (0x138a, 2, 3):
	for en1dir in ((0.0, -1.0, 0.0), (3.0, -4.0, 12.0), (0.0, 0.0, 0.0)):
		for en1rg in (0.0, 250.0):
			for en1cap in ((1, 1), (1, 0), (0, 1)):
				def en1sl(t=en1t, d=en1dir, rg=en1rg, cap=en1cap):
					en1poke(en1luz + 8, t)
					en1set(en1luz + 0x80, (10.0, 20.0, -30.0) + d)
					en1set(en1luz + 0x34, (rg, 0.0, 45.0))
					en1poke(symva('g_ShadowsStencil'), cap[0])
					en1poke(symva('g_StencilCapture'), cap[1])
					SDTest.Call('E3D_SetShadowLight', 'vii', en1luz, 0)
					r1 = en1st()
					SDTest.Call('E3D_SetShadowLight', 'vii', 0, 0)
					return (r1, en1i(symva('g_ShadowLight')))
				check('en1 E3D_SetShadowLight' + fmt((en1t, en1dir, en1rg, en1cap)), en1sl)
en1poke(symva('g_ShadowsStencil'), en1ss[0])
check('en1 Stencil_DebugLine', lambda: en1lg(lambda: (SDTest.Call('Stencil_DebugLine', 'vFFi', (1.0, 2.0, 3.0), (-4.0, 5.0, 6.5),
	0xff00ff),)))
for en1on in (0, 1, 0, 1):
	check('en1 ShadowsStencil_Set' + fmt((en1on,)), lambda on=en1on: (SDTest.Call('ShadowsStencil_Set', 'vi', on), en1st()))
check('en1 Stencil_Free Init', lambda: en1lg(lambda: (SDTest.Call('Stencil_Free', 'v'), SDTest.Call('Stencil_Init', 'v'), en1st(),
	en1i(symva('g_StencilBlock')) != 0, en1i(symva('g_StencilSegments')) != 0, en1i(symva('g_StencilTriInv')) != 0, en1i(symva('g_StencilVB')) != 0, en1i(symva('g_StencilIB')) != 0,
	en1i(symva('g_StencilVB2')) != 0, en1i(symva('g_StencilVB3')) != 0, SDTest.Peek('g_StencilShadows', 'i'))))
for en1k in range(len(en1sg)):
	en1poke(en1sg[en1k], en1ss[en1k])
en1poke(symva('g_RenderFlagNeg'), (SDTest.Peek('g_RenderFlagNeg', 'i') & 0xffffff00) | en1s4ec)

# the DX8 SDK helpers: the order of display modes, the string conversions, the GUID strings and the depth format
# check of the adapter of g_Engine
en1dm = en1z(0x20)
for en1a, en1b in (((640, 480, 60, 22), (800, 600, 60, 22)), ((800, 600, 60, 22), (640, 480, 60, 22)),
	((800, 480, 60, 22), (800, 600, 60, 22)), ((800, 600, 60, 22), (800, 480, 60, 22)), ((800, 600, 75, 22),
	(800, 600, 60, 23)), ((800, 600, 60, 23), (800, 600, 60, 22)), ((800, 600, 60, 22), (800, 600, 85, 22)),
	((0x80000000, 1, 0, -1), (5, 1, 0, 3)), ((7, 7, 0, -5), (7, 7, 0, 2))):
	for en1k in range(4):
		en1poke(en1dm + 4 * en1k, en1a[en1k])
		en1poke(en1dm + 16 + 4 * en1k, en1b[en1k])
	check('en1 D3DApp_SortModes' + fmt((en1a, en1b)), lambda: SDTest.Call('D3DApp_SortModes', 'iii', en1dm, en1dm + 16)[0])
en1wb = en1z(0x80)
def en1wfill(a, n, v):
	for k in range(n):
		en1poke(a + 4 * k, v)
def en1ints(a, n):
	r = []
	for k in range(n):
		r.append(en1i(a + 4 * k))
	return r
for en1s in ('', 'abc', 'Hello World 123', 'x'):
	for en1c in (-1, 1, 4, 0, 30):
		for en1fn in ('DXUtil_AnsiToGeneric', 'DXUtil_AnsiToGeneric@2765b0'):
			check('en1 DXUtil_AnsiToGeneric' + fmt((en1fn, en1s, en1c)), lambda fn=en1fn, s=en1s, c=en1c:
				SDTest.Call(fn, 'vbsi', 'qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq', s, c)[1])
		for en1fn in ('DXUtil_AnsiToWide', 'DXUtil_GenericToWide'):
			def en1aw(fn=en1fn, s=en1s, c=en1c):
				en1wfill(en1wb + 4, 30, 0x41414141)
				SDTest.Call(fn, 'visi', en1wb + 4, s, c)
				return en1ints(en1wb, 16)
			check('en1 DXUtil_AnsiToWide' + fmt((en1fn, en1s, en1c)), en1aw)
		for en1fn in ('DXUtil_WideToAnsi', 'DXUtil_WideToGeneric'):
			def en1wa(fn=en1fn, s=en1s, c=en1c):
				en1wfill(en1wb, 32, 0)
				for k in range(len(s)):
					en1poke(en1wb + 2 * k, (en1i(en1wb + 2 * k) & ~0xffff) | ord(s[k]))
				return SDTest.Call(fn, 'vbii', 'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz', en1wb, c)[1]
			check('en1 DXUtil_WideToAnsi' + fmt((en1fn, en1s, en1c)), en1wa)
en1guid = en1z(0x10)
for en1g in ((0x12345678, 0x9abcdef0, 0x01234567, 0x89abcdef), (0, 0, 0, 0), (-1, -1, -1, -1), (0x00ff00ff, 0x0f0f0f0f,
	0x7f, 0x80000000)):
	for en1k in range(4):
		en1poke(en1guid + 4 * en1k, en1g[en1k])
	check('en1 DXUtil_GUIDToString' + fmt(en1g), lambda: SDTest.Call('DXUtil_GUIDToString', 'vib', en1guid,
		'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww')[1])
for en1s in ('{12345678-9ABC-DEF0-1234-56789ABCDEF0}', '{12345678-9abc-def0-1234-56789abcdef0}', 'abc', '',
	'{1234-5678}', '{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}', '{1-2-3-4567-89ABCDEF0123}'):
	def en1sg(s=en1s):
		en1wfill(en1guid, 4, 0x11111111)
		return (SDTest.Call('DXUtil_StringToGUID', 'isi', s, en1guid)[0], en1ints(en1guid, 4))
	check('en1 DXUtil_StringToGUID' + fmt((en1s,)), en1sg)
en1motor = SDTest.Peek('g_Engine', 'i')
en1d3d = 0
if en1motor:
	en1d3d = en1i(en1motor + 0x2a3d4)
check('en1 d3d', lambda: en1d3d != 0)
if en1d3d:
	for en1f in (22, 23, 21):
		for en1ds in (80, 77, 75, 79, 71, 0):
			check('en1 D3DApp_DepthFormatOk' + fmt((en1f, en1ds)), lambda f=en1f, ds=en1ds: SDTest.Call('D3DApp_DepthFormatOk',
				'biiiii', en1d3d, 0, 1, f, ds)[0])
	en1mb = (en1i(en1motor + 0x2a5b4), en1i(en1motor + 0x2a5b8))
	for en1f in (22, 23):
		for en1db in ((16, 0), (15, 1), (24, 0), (24, 8), (24, 4), (32, 0), (16, 9), (24, 9), (32, 1), (40, 0)):
			def en1fd(f=en1f, db=en1db):
				en1poke(en1motor + 0x2a5b4, db[0])
				en1poke(en1motor + 0x2a5b8, db[1])
				return SDTest.Call('CD3DApp_FindDepthStencilFormat', 'tiiiiip', en1motor, 0, 1, f, 0x5a5a)
			check('en1 CD3DApp_FindDepthStencilFormat' + fmt((en1f, en1db)), en1fd)
	en1poke(en1motor + 0x2a5b4, en1mb[0])
	en1poke(en1motor + 0x2a5b8, en1mb[1])

# CD3DApplication: the constructor and destructor on a buffer (g_Engine set back), the window style, the back buffer
# description (a new device id) and pause; these change the state of the device, so they come last
if en1motor:
	en1app = en1z(0x2a600)
	def en1appf(a):
		return map(lambda o, a=a: en1i(a + o), (0, 0x2a37c, 0x2a380, 0x2a384, 0x2a388, 0x2a38c, 0x2a390, 0x2a394,
			0x2a3cc, 0x2a3d0, 0x2a3d4, 0x2a3d8, 0x2a4f0, 0x2a520, 0x2a524, 0x2a57c, 0x2a5a8, 0x2a5b0, 0x2a5b4,
			0x2a5b8, 0x2a5bc, 0x2a5c0, 0x2a5c4, 0x2a5c8, 0x2a5cc, 0x2a5d0, 0x2a5d4, 0x2a5d8))
	for en1k in range(0, 0x2a600, 4):
		if en1k >= 0x2a370:
			en1poke(en1app + en1k, 0x5a5a5a5a)
	def en1ctor():
		r = SDTest.Call('CD3DApp_ctor', 'tii', en1app)[0] == en1app
		m = SDTest.Peek('g_Engine', 'i') == en1app
		f = en1appf(en1app)
		title = SDTest.Peek(en1i(en1app + 0x2a5ac), '@s')
		SDTest.Call('CD3DApp_dtor', 'tvi', en1app)
		m2 = SDTest.Peek('g_Engine', 'i')
		SDTest.Poke('g_Engine', en1motor)
		return (r, m, f, title, m2)
	check('en1 CD3DApp_ctor dtor', en1ctor)
	check('en1 CD3DApp_AdjustWindowForChange', lambda: SDTest.Call('CD3DApp_AdjustWindowForChange', 'tii', en1motor)[0])
	en1devid = en1i(en1motor + 0x2a5a8)
	check('en1 CD3DApp_UpdateBackBufferDesc', lambda: (en1lg(lambda: (SDTest.Call('CD3DApp_UpdateBackBufferDesc', 'tvi', en1motor),)),
		en1ints(en1motor + 0x2a4b0, 16), en1fl(symva('g_PhysicalAspectRatio'), 1), en1i(en1motor + 0x2a5a8) - en1devid))
	en1pc = SDTest.Peek('g_AppPausedCount', 'i')
	for en1p in (1, 1, 0, 0):
		check('en1 CD3DApp_Pause' + fmt((en1p,)), lambda p=en1p: (SDTest.Call('CD3DApp_Pause', 'tvii', en1motor, p),
			SDTest.Peek('g_AppPausedCount', 'i') - en1pc, en1i(en1motor + 0x2a38c)))

# the counter of the engine with a fixed time poked (the real one differs between the runs), the base set back
en1tf = (en1i(symva('g_TimerFixed')), en1i(symva('g_TimerFixed') + 0x4), en1i(symva('g_TimerBase')), en1i(symva('g_TimerBase') + 0x4))
en1tq = en1z(8)
for en1v in ((12345, 0), (0x7fff0000, 3), (5, 0)):
	en1poke(symva('g_TimerFixed'), en1v[0])
	en1poke(symva('g_TimerFixed') + 0x4, en1v[1])
	check('en1 Timer_GetFreq' + fmt(en1v), lambda: (SDTest.Call('Timer_GetFreq', 'ii', en1tq)[0], en1ints(en1tq, 2)))
	for en1fps in (60.0, 30.0, 0.5, 1000000.0):
		check('en1 Timer_FixedStep' + fmt((en1v, en1fps)), lambda f=en1fps: (SDTest.Call('Timer_FixedStep', 'if', f)[0],
			en1i(symva('g_TimerFixed')), en1i(symva('g_TimerFixed') + 0x4)))
	check('en1 Timer_FixedStep 0' + fmt(en1v), lambda: (SDTest.Call('Timer_FixedStep', 'if', 0.0)[0], en1i(symva('g_TimerFixed')),
		en1i(symva('g_TimerFixed') + 0x4)))
for en1k in range(2):
	en1poke(symva('g_TimerFixed') + 4 * en1k, en1tf[en1k])
	en1poke(symva('g_TimerBase') + 4 * en1k, en1tf[2 + en1k])
