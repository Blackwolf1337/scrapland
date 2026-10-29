# game AI/NodesWorldSup.c and AI/NodesWorld2D.c (execfile'd by sdlevel.py, Python 1.5.2). FZ has only the 3D graph
# (g_NodesWorld): the CNodesWorldSup methods and the 2D queries run on it read only (the 2D searches only use the
# CNodesWorldSup part and slots both vtables share). Fresh CNodesWorld2D objects load the 2D graph files of other
# levels from the packs (no file of FZ), build their grid in a box of their own and run the queries and AnalizeGraph;
# the generation tools (buffer, sets of points, links, pairs) run on points and cells set up here. Nothing writes a
# file: SavePath, SaveInfPath and Load with arg >= 0 (the generation) are left out.
def n2dI(a):
	return SDTest.Peek(a, '@i')
def n2dB(a):
	return SDTest.Peek(a, '@b')
def n2dF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def n2dSetF(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def n2dCalloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def n2dFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def n2dNew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def n2dDelete(p, n):
	SDTest.Call('operator_delete_sized', 'vii', p, n)
def n2dPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def n2dAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def n2dInside(p, mn, mx):
	for k in range(3):
		if p[k] < mn[k] or p[k] > mx[k]:
			return 0
	return 1
def n2dPick(r, k):
	return (r[0], r[k])

# the index of a pointer in a list of known pointers, 'x' for an unknown one, 0 for NULL
def n2dIdx(v, known):
	if v == 0:
		return 0
	for k in range(len(known)):
		if known[k] == v:
			return k + 1
	return 'x'

# the data words of a ring (node + 8), at most m
def n2dRing(lst, m):
	r = []
	h = n2dI(lst)
	p = h
	while p and len(r) < m:
		r.append(n2dI(p + 8))
		p = n2dI(p)
		if p == h:
			break
	return r

# a ring of pointers as indices of the known ones, with count, iterator set and free nodes
def n2dRingIdx(lst, known):
	return (n2dI(lst + 8), map(lambda v, known=known: n2dIdx(v, known), n2dRing(lst, 60)), n2dI(lst + 4) != 0,
		n2dI(lst + 0x10))

# a ring of 0x14 nodes as vec3, at most m
def n2dVecRing(lst, m):
	r = []
	h = n2dI(lst)
	p = h
	while p and len(r) < m:
		r.append(n2dF(p + 8, 3))
		p = n2dI(p)
		if p == h:
			break
	return (n2dI(lst + 8), r)

def n2dNodeIdx(nodes, v):
	return (v - nodes) / 0x14

# the debug segments of a list, freed after
def n2dSegs(lst):
	r = []
	h = n2dI(lst)
	p = h
	while p and len(r) < 50:
		r.append(n2dF(p + 8, 6))
		q = n2dI(p)
		n2dDelete(p, 0x20)
		p = q
		if p == h:
			break
	n = n2dI(lst + 8)
	n2dPoke(lst, 0)
	n2dPoke(lst + 8, 0)
	return (n, r)

# ---- the 3D graph of FZ: the CNodesWorldSup methods and the 2D queries ----

def n2dSupW(w, n, nodes):
	check('n2d GetBox', lambda w=w: SDTest.Call('CNodesWorldSup_GetBox', 'tviFF', w, (0.0,) * 3, (0.0,) * 3)[1:3])
	check('n2d IsCreated', lambda w=w: (SDTest.Call('CNodesWorldSup_IsCreated', 'tbi', w)[0], SDTest.Call('CNodesWorldSup_NumVisPoints', 'tii', w)[0],
		SDTest.Call('CNodesWorldSup_GetMaxVisDist', 'tfi', w)[0]))
	for i in (0, 1, n / 3, n - 1):
		check('n2d GetVisPoint(%d)' % i, lambda w=w, i=i, nodes=nodes: SDTest.Call('CNodesWorldSup_GetVisPoint', 'tiii', w, i)[0] ==
			nodes + 0x14 * i)
	k = 0
	for v in (nodes, nodes + 0x14 * (n - 1), nodes - 0x14, nodes + 0x14 * n, nodes + 0x14 * (n / 2), nodes + 7, 0):
		check('n2d FileOf(%d)' % k, lambda w=w, v=v: SDTest.Call('CNodesWorldSup_FileOf', 'tiii', w, v)[0])
		k = k + 1
	lst = n2dCalloc(4 * 8)
	for k in range(4):
		n2dPoke(lst + 4 * k, nodes + 0x14 * ((k * n) / 5))
	for c in (0, 1, 4):
		check('n2d CommonFile(%d)' % c, lambda w=w, lst=lst, c=c: SDTest.Call('CNodesWorldSup_CommonFile', 'tiiii', w, lst, c)[0])
	for c in (0, 2, 4):
		check('n2d InList(%d)' % c, lambda w=w, lst=lst, c=c, nodes=nodes, n=n: (SDTest.Call('CNodesWorldSup_InList', 'tbiiii', w,
			lst, c, nodes + 0x14 * ((n * 2) / 5))[0], SDTest.Call('CNodesWorldSup_InList', 'tbiiii', w, lst, c, nodes + 4)[0]))
	for i in (0, n / 4, n / 2, n - 1):
		a = nodes + 0x14 * i
		if n2dI(a + 0xc) <= 0:
			continue
		for pos, mx in ((n2dF(n2dI(n2dI(a + 0x10)), 3), 40), (n2dF(a, 3), 2), ((1.0, 2.0, 3.0), 40)):
			check('n2d OtherLinks(%d)%s %d' % (i, fmt(pos), mx), lambda w=w, a=a, pos=pos, lst=lst, mx=mx, nodes=nodes:
				(lambda r, lst=lst, nodes=nodes: (r[2], map(lambda k, lst=lst, nodes=nodes: n2dNodeIdx(nodes,
				n2dI(lst + 4 * k)), range(min(r[2], 8)))))(SDTest.Call('CNodesWorldSup_OtherLinks', 'tviiFiip', w, a, pos, lst, min(mx,
				8), 0)))
	n2dFree(lst)
	# LastNodePos sets nodeInit: the words put back after
	keep = []
	for k in range(3):
		keep.append(n2dI(w + 0x64 + 4 * k))
	check('n2d LastNodePos', lambda w=w: (SDTest.Call('CNodesWorldSup_LastNodePos', 'tviF', w, (0.0,) * 3)[1], n2dF(w + 0x64, 3)))
	for k in range(3):
		n2dPoke(w + 0x64 + 4 * k, keep[k])
	mn = n2dF(w + 0x74, 3)
	mx = n2dF(w + 0x80, 3)
	for p in (mn, mx, n2dAdd(mn, (1000.0, 500.0, 2500.0)), n2dF(nodes, 3), (pp[0], pp[1], pp[2]), (-1e9, 0.0, 0.0)):
		check('n2d SnapPos%s' % fmt(p), lambda w=w, p=p: SDTest.Call('CNodesWorldSup_SnapPos', 'tviFF', w, (0.0,) * 3, p)[1])
		check('n2d InGrid%s' % fmt(p), lambda w=w, p=p: SDTest.Call('CNodesWorldSup_InGrid', 'tbiF', w, p)[0])
	for box in ((0.0, 0.0, 0.0, 1000.0, 2000.0, 3000.0), mn + mx, (5.0, -5.0, 7.0, 4.0, 900.0, -10.0)):
		check('n2d BoxCells%s' % fmt(box), lambda w=w, box=box: SDTest.Call('CNodesWorldSup_BoxCells', 'tiiF', w, box)[0])

# the bodies and the floor of the 2D graph at points of FZ
def n2dGround(w, n, nodes):
	check('n2d globals', lambda: (n2dF(symva('g_NW2DBodies'), 12), n2dF(symva('g_NW2DGroundDist'), 1), n2dF(symva('g_NW2DHeight'), 1),
		SDTest.Peek('g_NWDebugSegs', 'i'), SDTest.Peek('g_NW2DDrawMode', 'i')))
	pts = [(pp[0], pp[1], pp[2]), n2dAdd((pp[0], pp[1], pp[2]), (0.0, 150.0, 0.0)),
		n2dAdd((pp[0], pp[1], pp[2]), (300.0, 40.0, -250.0))]
	for i in (0, n / 3, (2 * n) / 3):
		pts.append(n2dF(nodes + 0x14 * i, 3))
	for p in pts:
		for h in (0.0, 50.0):
			check('n2d Ground%s %s' % (fmt(p), fmt(h)), lambda w=w, p=p, h=h: SDTest.Call('CNodesWorld2D_Ground', 'tbiFFf', w, p,
				(0.0,) * 3, h)[0:2])
		for d in ((0.0, -3000.0, 0.0), (400.0, 0.0, 300.0)):
			q = n2dAdd(p, d)
			check('n2d GroundSees%s%s' % (fmt(p), fmt(d)), lambda w=w, p=p, q=q: SDTest.Call('CNodesWorld2D_GroundSees', 'tbiFFf', w, p,
				q, 30.0)[0])
			check('n2d GroundLine%s%s' % (fmt(p), fmt(d)), lambda w=w, p=p, q=q: SDTest.Call('CNodesWorld2D_GroundLine', 'tbiFFiii', w,
				p, q, 0, 0, 0)[0])
		for f in ('CNodesWorld2D_FitAt@b43d0', 'CNodesWorld2D_FitAt'):
			for down, foot, r in ((500.0, 40.0, 155.0), (5000.0, 12.0, 35.0)):
				check('n2d FitAt(%s)%s %s %s' % (f, fmt(p), fmt(down), fmt(r)), lambda w=w, f=f, p=p, down=down,
					foot=foot, r=r: SDTest.Call(f, 'tbifffFfffff', w, p[0], p[1], p[2], (0.0,) * 3, down, foot, r,
					189.22, 221.446)[0:2])
		for d in ((200.0, 0.0, 150.0), (-90.0, 30.0, 0.0), (0.0, 0.0, 0.0), (500.0, -100.0, -400.0)):
			q = n2dAdd(p, d)
			for up, foot, r, h2, h1 in ((442.89, 40.0, 155.0, 189.22, 221.446), (100.0, 12.0, 35.0, 52.0, 52.0)):
				check('n2d Walk%s%s %s %s' % (fmt(p), fmt(d), fmt(up), fmt(r)), lambda w=w, p=p, q=q, up=up, foot=foot,
					r=r, h2=h2, h1=h1: (SDTest.Call('CNodesWorld2D_Walk', 'tbiFFfffff', w, p, q, up, foot, r, h2, h1)[0],
					SDTest.Call('CNodesWorld2D_WalkFit', 'tbiFFfffff', w, p, q, up, foot, r, h2, h1)[0]))
			# TestWalk on these points does not return in either run (the game is killed): not called
		for d in ((0.0, 0.0, 0.0), (300.0, 10.0, 200.0), (0.0, 400.0, 0.0), (-800.0, 0.0, 50.0)):
			q = n2dAdd(p, d)
			for r, h in ((0.0, 30.0), (40.0, 100.0), (155.0, 50.0)):
				check('n2d TestPath%s%s %s %s' % (fmt(p), fmt(d), fmt(r), fmt(h)), lambda w=w, p=p, q=q, r=r, h=h:
					(SDTest.Call('CNodesWorld2D_TestPath', 'tbiFFffffi', w, p, q, r, r, h, 7.0, 0)[0], SDTest.Call('CNodesWorld2D_TestStep', 'tbiFFff', w,
					p, q, r, h)[0]))
			sw = SDTest.Peek('g_NWDebugSegs', 'i')
			SDTest.Poke('g_NWDebugSegs', 1)
			dbg = n2dCalloc(0x18)
			check('n2d TestPath%s%s segs' % (fmt(p), fmt(d)), lambda w=w, p=p, q=q, dbg=dbg: (SDTest.Call('CNodesWorld2D_TestPath',
				'tbiFFffffi', w, p, q, 25.0, 25.0, 40.0, 0.0, dbg)[0], n2dSegs(dbg)))
			n2dFree(dbg)
			SDTest.Poke('g_NWDebugSegs', sw)
		for d1, d2 in (((300.0, 0.0, 0.0), (0.0, 0.0, 300.0)), ((200.0, 50.0, 100.0), (-100.0, 0.0, 250.0)),
				((0.0, 0.0, 0.0), (100.0, 0.0, 0.0)), ((0.0, 300.0, 0.0), (150.0, 0.0, -40.0))):
			b = n2dAdd(p, d1)
			c = n2dAdd(b, d2)
			for r, h in ((0.0, 30.0), (40.0, 100.0)):
				for flat in (0, 1):
					check('n2d TestCorner%s%s%s %s %s %d' % (fmt(p), fmt(d1), fmt(d2), fmt(r), fmt(h), flat), lambda w=w,
						p=p, b=b, c=c, r=r, h=h, flat=flat: SDTest.Call('CNodesWorld2D_TestCorner', 'tbiFFFffffii', w, p, b, c, r, r, h, 3.0,
						flat, 0)[0])
			sw = SDTest.Peek('g_NWDebugSegs', 'i')
			SDTest.Poke('g_NWDebugSegs', 1)
			dbg = n2dCalloc(0x18)
			for flat in (0, 1):
				check('n2d TestCorner%s%s%s segs %d' % (fmt(p), fmt(d1), fmt(d2), flat), lambda w=w, p=p, b=b, c=c,
					flat=flat, dbg=dbg: (SDTest.Call('CNodesWorld2D_TestCorner', 'tbiFFFffffii', w, p, b, c, 20.0, 20.0, 40.0, 0.0, flat,
					dbg)[0], n2dSegs(dbg)))
			n2dFree(dbg)
			SDTest.Poke('g_NWDebugSegs', sw)

# the point searches of the 2D graph on a graph with a grid; list holds 40 pointers
def n2dSearch(w, n, nodes, tag, lst):
	grid = n2dI(w + 0x18c)
	gmax = n2dF(grid + 0xc, 3)
	gmin = n2dF(grid + 0x18, 3)
	pts = [((gmax[0] + gmin[0]) * 0.5, (gmax[1] + gmin[1]) * 0.5, (gmax[2] + gmin[2]) * 0.5), (pp[0], pp[1], pp[2])]
	for i in (0, n / 5, n / 2, n - 1):
		pts.append(n2dAdd(n2dF(nodes + 0x14 * i, 3), (60.0, 30.0, -45.0)))
	for p in pts:
		if not n2dInside(p, gmin, gmax):
			check('n2d %s outside%s' % (tag, fmt(p)), lambda: 1)
			continue
		for mode, r, h1, h2, dmin, dmax, mx, soft in ((1, 0.0, 30.0, 0.0, 0.0, 30000.0, 40, 0),
				(2, 0.0, 0.0, 0.0, 0.0, 20000.0, 40, 0), (3, 0.0, 0.0, 0.0, 1000.0, 40000.0, 40, 0),
				(1, 0.0, 60.0, 0.0, 0.0, 2000.0, 5, 1), (2, 0.0, 0.0, 0.0, 15000.0, 15000.0, 40, 1),
				(3, 0.0, 0.0, 0.0, 300.0, 90000.0, 12, 0), (0, 40.0, 30.0, 10.0, 0.0, 3000.0, 3, 0)):
			for f, nm in ((sym('CNodesWorld2D_GetPointsVis'), 'GetPointsVis'), (sym('CNodesWorld2D_SearchRings'), 'SearchRings')):
				check('n2d %s %s%s %d %s %s %s %d %d' % (tag, nm, fmt(p), mode, fmt(h1), fmt(dmin), fmt(dmax), mx,
					soft), lambda w=w, f=f, p=p, lst=lst, mode=mode, r=r, h1=h1, h2=h2, dmin=dmin, dmax=dmax, mx=mx,
					soft=soft, nodes=nodes: (lambda res, lst=lst, nodes=nodes: (res[2], map(lambda k, lst=lst,
					nodes=nodes: n2dNodeIdx(nodes, n2dI(lst + 4 * k)), range(res[2]))))(SDTest.Call(f,
					'tviFiipfffffii', w, p, lst, mx, 0, r, h1, h2, dmin, dmax, soft, mode)))
		for r, h1, h2 in ((0.0, 0.0, 0.0), (40.0, 30.0, 10.0)):
			check('n2d %s HasPointVis%s %s' % (tag, fmt(p), fmt(r)), lambda w=w, p=p, r=r, h1=h1, h2=h2:
				SDTest.Call('CNodesWorld2D_HasPointVis', 'tbiFfff', w, p, r, h1, h2)[0])
		# NearestPointVis CHECKs that it found a point: the same search first
		if SDTest.Call('CNodesWorld2D_GetPointsVis', 'tviFiipfffffii', w, p, lst, 7, 0, 0.0, 0.0, 0.0, 0.0, n2dF(w + 0x60, 1)[0], 1,
				2)[2] > 0:
			check('n2d %s NearestPointVis%s' % (tag, fmt(p)), lambda w=w, p=p: SDTest.Call('CNodesWorld2D_NearestPointVis', 'tviFF', w, p,
				(0.0,) * 3)[2])

# the path builder of the 2D graph from points near some nodes (TestPath of the original for the direct start)
def n2dBuild(w, n, nodes, tag):
	path = n2dCalloc(0x18)
	np = n2dCalloc(4)
	grid = n2dI(w + 0x18c)
	gmax = n2dF(grid + 0xc, 3)
	gmin = n2dF(grid + 0x18, 3)
	for i in (1, n / 4, n / 2, n - 2):
		start = n2dAdd(n2dF(nodes + 0x14 * i, 3), (50.0, 20.0, 40.0))
		if not n2dInside(start, gmin, gmax):
			continue
		for depth, d, direct, flat in ((1, (1.0, 0.0, 0.0), 0, 0), (6, (1.0, 0.0, 0.0), 1, 0),
				(12, (0.0, 0.0, -1.0), 0, 1), (20, (0.3, 0.9, 0.1), 1, 1)):
			n2dPoke(np, nodes + 0x14 * ((i + 3) % n))
			check('n2d %s BuildPath(%d) %d%s %d %d' % (tag, i, depth, fmt(d), direct, flat), lambda w=w, path=path,
				np=np, depth=depth, d=d, start=start, direct=direct, flat=flat, nodes=nodes: (SDTest.Call('CNodesWorld2D_BuildPath',
				'tbiiiFFiifffi', w, path, depth, d, start, direct, np, 30.0, 20.0, 10.0, flat)[0], n2dVecRing(path, 30),
				n2dNodeIdx(nodes, n2dI(np))))
			SDTest.Call('DListVec3_Clear', 'tvi', path)
	n2dFree(np)
	n2dFree(path)

def n2dOnW(w):
	n = n2dI(w + 0x1ac)
	nodes = n2dI(w + 0x1a8)
	check('n2d NodesWorld', lambda n=n, w=w: (n, n2dI(w + 0x258), n2dI(w + 0x1b0) != 0, n2dI(w + 0x1c0)))
	if n <= 0:
		return
	n2dSupW(w, n, nodes)
	n2dGround(w, n, nodes)
	if n2dI(w + 0x18c):
		lst = n2dCalloc(4 * 40)
		n2dSearch(w, n, nodes, 'W', lst)
		n2dFree(lst)
		n2dBuild(w, n, nodes, 'W')

# ---- fresh objects ----

# the fields a constructor sets
def n2dFields(m):
	return (n2dI(m) - 0x400000, n2dF(m + 4, 0x18), n2dB(m + 0x70), n2dF(m + 0x74, 6), n2dB(m + 0x8c), n2dB(m + 0x18b),
		map(lambda k, m=m: n2dI(m + 0x18c + 4 * k), range(31)), map(lambda k, m=m: n2dI(m + 0x258 + 4 * k), range(1)),
		map(lambda k, m=m: n2dI(m + 0x25c + 4 * k), range(42)), n2dF(m + 0x304, 3),
		map(lambda k, m=m: n2dI(m + 0x310 + 4 * k), range(19)), n2dB(m + 0x35c))

def n2dNodes(m):
	n = n2dI(m + 0x1ac)
	nodes = n2dI(m + 0x1a8)
	r = []
	for i in range(min(n, 40)):
		a = nodes + 0x14 * i
		r.append((n2dF(a, 3), map(lambda k, a=a, nodes=nodes: n2dNodeIdx(nodes, n2dI(n2dI(a + 0x10) + 4 * k)),
			range(min(n2dI(a + 0xc), 12)))))
	return (n, r)

# a 2D graph of another level loaded and with its grid in the box of its nodes (indoors: IsMapOutDoor 0 for the grid)
def n2dLoaded(m, path):
	check('n2d LoadGraph %s' % path, lambda m=m, path=path: (SDTest.Call('CNodesWorldSup_LoadGraph', 'tbis', m, path)[0], n2dB(m + 0x70),
		n2dI(m + 0x258), map(lambda k, m=m: (n2dI(m + 0x208 + 4 * k) - n2dI(m + 0x1a8)) / 0x14, range(min(n2dI(m +
		0x258), 4))), n2dNodes(m)))
	n = n2dI(m + 0x1ac)
	nodes = n2dI(m + 0x1a8)
	if n <= 0:
		return 0
	mn = list(n2dF(nodes, 3))
	mx = list(n2dF(nodes, 3))
	for i in range(n):
		p = n2dF(nodes + 0x14 * i, 3)
		for k in range(3):
			mn[k] = min(mn[k], p[k])
			mx[k] = max(mx[k], p[k])
	n2dSetF(m + 0x74, (mn[0] - 100.0, mn[1] - 100.0, mn[2] - 100.0, mx[0] + 100.0, mx[1] + 100.0, mx[2] + 100.0))
	out = SDTest.Peek('g_IsMapOutDoor', 'i')
	SDTest.Poke('g_IsMapOutDoor', 0)
	check('n2d BuildGrid %s' % path, lambda m=m: (SDTest.Call('CNodesWorldSup_BuildGrid', 'tvi', m), n2dI(m + 0x18c) != 0))
	SDTest.Poke('g_IsMapOutDoor', out)
	grid = n2dI(m + 0x18c)
	if not grid:
		return 0
	check('n2d grid %s' % path, lambda grid=grid: (n2dI(grid) - 0x400000, n2dI(grid + 8), n2dF(grid + 0xc, 6),
		map(lambda k, grid=grid: n2dI(grid + 0x24 + 4 * k), range(3)), n2dF(grid + 0x30, 3)))
	cnt = []
	for i in range(min(n2dI(grid + 8), 200)):
		c = n2dI(n2dI(grid + 4) + 4 * i)
		if c:
			cnt.append((i, map(lambda v, nodes=nodes: n2dNodeIdx(nodes, v), n2dRing(c, 20))))
	check('n2d grid cells %s' % path, lambda cnt=cnt: cnt)
	# the instance functions through the slots of the grid
	vt = n2dI(grid)
	c = (n2dF(nodes, 3), n2dF(nodes + 0x14 * (n - 1), 3), n2dF(grid + 0xc, 3), n2dF(grid + 0x18, 3))
	for p in c:
		check('n2d grid fns %s%s' % (path, fmt(p)), lambda grid=grid, vt=vt, p=p, nodes=nodes: (
			SDTest.Call(n2dI(vt + 0x20) - 0x400000, 'tbiF', grid, p)[0],
			SDTest.Call(n2dI(vt + 0x28) - 0x400000, 'tiiF', grid, p)[0],
			SDTest.Call(n2dI(vt + 0x14) - 0x400000, 'tbiFppp', grid, p, 0, 0, 0)[2:5],
			(lambda r, nodes=nodes: (r[0], r[2] != 0 and map(lambda v, nodes=nodes: n2dNodeIdx(nodes, v),
			n2dRing(r[2], 20)) or 0))(SDTest.Call(n2dI(vt + 8) - 0x400000, 'tbiFp', grid, p, 0)),
			SDTest.Call(n2dI(vt + 0x18) - 0x400000, 'tvippp', grid, 0, 0, 0)[1:4],
			SDTest.Call(n2dI(vt + 0x1c) - 0x400000, 'tiiF', grid, (0.0,) * 3)[1]))
	check('n2d grid outside %s' % path, lambda grid=grid, vt=vt: SDTest.Call(n2dI(vt + 0x20) - 0x400000, 'tbiF', grid,
		(-1e9, 0.0, 0.0))[0])
	check('n2d GetCell %s' % path, lambda grid=grid, vt=vt, nodes=nodes: (lambda r, nodes=nodes: (r[0], r[1:4],
		r[4] != 0 and map(lambda v, nodes=nodes: n2dNodeIdx(nodes, v), n2dRing(r[4], 20)) or 0))(SDTest.Call(n2dI(vt +
		0xc) - 0x400000, 'tbipppp', grid, 0, 0, 0, 0)))
	return n

# AnalizeGraph of the 2D graph on a loaded graph: its three lists; the switches of the debug view restored after
def n2dAnalize(m, path):
	s1 = SDTest.Peek('g_NWAnalyzeOn', 'i')
	s2 = SDTest.Peek('g_NW2DDrawMode', 'i')
	check('n2d AnalizeGraph %s' % path, lambda m=m: (SDTest.Call('CNodesWorld2D_AnalyzeGraph', 'tvi', m), SDTest.Peek('g_NWAnalyzeOn', 'i'),
		SDTest.Peek('g_NW2DDrawMode', 'i'), n2dVecRing(m + 0x310, 30), n2dVecRing(m + 0x328, 30), n2dVecRing(m + 0x340, 30)))
	SDTest.Poke('g_NWAnalyzeOn', s1)
	SDTest.Poke('g_NW2DDrawMode', s2)
	for off in (0x310, 0x328, 0x340):
		SDTest.Call('DListVec3_Clear', 'tvi', m + off)

def n2dFresh2D():
	m = n2dNew(0x378)
	check('n2d ctor 2D', lambda m=m: (SDTest.Call('CNodesWorld2D_ctor', 'tii', m)[0] == m, n2dFields(m),
		map(lambda k, m=m: n2dI(m + 0x360 + 4 * k), range(6))))
	return m

def n2dDelete2D(m):
	check('n2d Delete 2D', lambda m=m: SDTest.Call('CNodesWorld2D_Delete', 'tiii', m, 1)[0] == m)

def n2dFiles():
	lst = n2dCalloc(4 * 40)
	for path in ('levels/junkyard/ai/path2dai', 'levels/bank/ai/path2dai', 'levels/fz/ai/path2dai'):
		m = n2dFresh2D()
		n = n2dLoaded(m, path)
		if n > 0:
			nodes = n2dI(m + 0x1a8)
			vs = [nodes, nodes + 0x14 * (n - 1), nodes + 0x14 * (n / 2)]
			if n2dI(m + 0x258) >= 2:
				vs.append(n2dI(m + 0x20c))
				vs.append(n2dI(m + 0x20c) - 0x14)
			for v in vs:
				check('n2d FileOf %s %d' % (path, (v - nodes) / 0x14), lambda m=m, v=v: SDTest.Call('CNodesWorldSup_FileOf', 'tiii', m,
					v)[0])
			n2dSearch(m, n, nodes, path, lst)
			n2dBuild(m, n, nodes, path)
			if n < 200:
				n2dAnalize(m, path)
		check('n2d FreeGraph %s' % path, lambda m=m: (SDTest.Call('CNodesWorldSup_FreeGraph', 'tvi', m), n2dI(m + 0x1a8), n2dI(m + 0x18c),
			n2dI(m + 0x1ac)))
		n2dDelete2D(m)
	n2dFree(lst)
	# LoadGraph and Create of the class: the name of the level, the vtable slots 0x0c and 0x10
	m = n2dFresh2D()
	nm = 'levels/junkyard'
	SDTest.Call('crt_memcpy', 'iisi', m + 0x8c, nm, len(nm) + 1)
	check('n2d Create 2D', lambda m=m: (SDTest.Call('CNodesWorldSup_Create', 'tbi', m)[0], n2dB(m + 0x70), n2dNodes(m)))
	check('n2d LoadGraph 2D', lambda m=m: (SDTest.Call('CNodesWorld2D_LoadGraph', 'tbi', m)[0], n2dB(m + 0x70), n2dI(m + 0x1ac),
		n2dI(m + 0x258)))
	check('n2d ClearLists 2D', lambda m=m: (SDTest.Call('CNodesWorld2D_ClearLists', 'tvi', m), n2dI(m + 0x190), n2dI(m + 0x1f0)))
	check('n2d Load(-2)', lambda m=m: (SDTest.Call('CNodesWorld2D_Load', 'tviisF', m, -2, '', (300.0, 400.0, 500.0))[0],
		n2dF(m + 4, 23), n2dF(m + 0x74, 6), n2dB(m + 0x70), n2dI(m + 0x1a8), n2dF(symva('g_NW2DBodies'), 12)))
	# Load(-1) builds the grid in the collision box of FZ: only when the nodes of the file are inside it
	box = SDTest.Call('World_GetCollideBox', 'iF', (0.0,) * 6)[1]
	m2 = n2dFresh2D()
	SDTest.Call('CNodesWorldSup_LoadGraph', 'tbis', m2, 'levels/junkyard/ai/path2dai')
	ok = n2dI(m2 + 0x1ac) > 0
	for i in range(n2dI(m2 + 0x1ac)):
		if not n2dInside(n2dF(n2dI(m2 + 0x1a8) + 0x14 * i, 3), box[0:3], box[3:6]):
			ok = 0
	n2dDelete2D(m2)
	# a grid of cells of 750 over the box: not too many
	cells = 1.0
	for k in range(3):
		cells = cells * (int((box[k + 3] - box[k]) / 750.0) + 1)
	if cells > 200000.0:
		ok = 0
	check('n2d Load(-1) inside', lambda ok=ok: ok)
	if ok:
		out = SDTest.Peek('g_IsMapOutDoor', 'i')
		SDTest.Poke('g_IsMapOutDoor', 0)
		check('n2d Load(-1)', lambda m=m, nm=nm: (SDTest.Call('CNodesWorld2D_Load', 'tviisF', m, -1, nm, (0.0, 0.0, 0.0))[0],
			n2dF(m + 4, 23), n2dF(m + 0x74, 6), n2dB(m + 0x70), n2dNodes(m), n2dI(m + 0x18c) != 0))
		SDTest.Poke('g_IsMapOutDoor', out)
	check('n2d Load(-1) missing', lambda m=m: (SDTest.Call('CNodesWorld2D_Load', 'tviisF', m, -1, 'levels/none', (0.0, 0.0,
		0.0))[0], n2dF(m + 4, 23), n2dB(m + 0x70), n2dI(m + 0x1a8)))
	n2dDelete2D(m)

# ---- the generation tools on cells and points set up here ----

def n2dGen():
	m = n2dFresh2D()
	# a buffer of 4 x 2 x 3 cells of 100 50 80 from (-200, 0, 100), 5 hash lists of sets
	n2dSetF(m + 4, (50.0, 25.0, 40.0, 100.0, 0.0, 0.0, 0.0, 50.0, 0.0, 0.0, 0.0, 80.0, 100.0, 50.0, 80.0))
	n2dSetF(m + 0x50, (5.0,))
	n2dSetF(m + 0x74, (-200.0, 0.0, 100.0, 200.0, 100.0, 340.0))
	for off, v in ((0x1b4, 4), (0x1b8, 2), (0x1bc, 3), (0x1c0, 24), (0x1ec, 5)):
		n2dPoke(m + off, v)
	buf = n2dCalloc(24 * 8)
	sets = n2dCalloc(5 * 0x18)
	n2dPoke(m + 0x1b0, buf)
	n2dPoke(m + 0x1e8, sets)
	for i in (0, 5, 11, 23):
		check('n2d BufferPos(%d)' % i, lambda m=m, i=i: (lambda p, m=m: (p, SDTest.Call('CNodesWorldSup_BufferIndex', 'tiiF', m, p)[0]))(
			SDTest.Call('CNodesWorldSup_BufferPos', 'tviFi', m, (0.0,) * 3, i)[1]))
	check('n2d CheckBuffer', lambda m=m: SDTest.Call('CNodesWorldSup_CheckBuffer', 'tbi', m)[0])
	for p in ((-150.0, 10.0, 120.0), (199.0, 99.0, 339.0), (0.0, 50.0, 220.0), (-1.0, 49.9, 180.0)):
		check('n2d BufferIndex%s' % fmt(p), lambda m=m, p=p: (SDTest.Call('CNodesWorldSup_BufferIndex', 'tiiF', m, p)[0],
			SDTest.Call('CNodesWorldSup_SnapPos', 'tviFF', m, (0.0,) * 3, p)[1]))
	for a, b in ((0, 0), (3, 4), (-7, 2), (100, 3)):
		check('n2d Hash(%d, %d)' % (a, b), lambda m=m, a=a, b=b: SDTest.Call('CNodesWorldSup_Hash', 'tiiii', m, a, b)[0])
	for box in ((0.0, 0.0, 0.0, 1000.0, 200.0, 300.0), (-10.0, 5.0, 0.0, 30.0, 6.0, 1.0)):
		check('n2d BoxCells%s' % fmt(box), lambda m=m, box=box: SDTest.Call('CNodesWorldSup_BoxCells', 'tiiF', m, box)[0])
	# the cell states: a cell of each state, the others 1
	st = (0, 3, 4, 5, 6, 1, 2, 4, 6, 0)
	for i in range(24):
		n2dPoke(buf + 8 * i, st[i % len(st)])
	check('n2d GetInfoNodes', lambda m=m: (SDTest.Call('CNodesWorldSup_GetInfoNodes', 'tvi', m), map(lambda k, m=m: n2dVecRing(m + 0x28c +
		0x18 * k, 10), range(5))))
	# points in some cells
	pos = ((-150.0, 10.0, 120.0), (-120.0, 30.0, 130.0), (-60.0, 20.0, 150.0), (50.0, 70.0, 300.0),
		(150.0, 90.0, 200.0), (-180.0, 60.0, 260.0))
	pts = []
	for p in pos:
		SDTest.Call('NWPointList_Add', 'tviF', m + 0x190, p + (0.0,) * 12)
		pts.append(n2dI(m + 0x190) + 8)
	cells = []
	for k in range(len(pts)):
		cells.append(SDTest.Call('CNodesWorldSup_BufferIndex', 'tiiF', m, pos[k])[0])
	check('n2d point cells', lambda cells=cells: cells)
	# the sets of the cells: users and points as sorted indices (the order by address and the hash lists depend on
	# the heap addresses), which cells share a set, the sets in all hash lists
	def n2dSets(m, buf, sets, pts):
		r = []
		seen = []
		for i in range(24):
			v = n2dI(buf + 8 * i + 4)
			if v:
				l = map(lambda x, pts=pts: n2dIdx(x, pts), n2dRing(v + 0xc, 60))
				l.sort()
				if not v in seen:
					seen.append(v)
				r.append((i, n2dI(v), n2dI(v + 0x14), l, n2dIdx(v, seen)))
		t = 0
		for k in range(5):
			t = t + n2dI(sets + 0x18 * k + 8)
		return (r, t)
	adds = ((0, 0), (1, 0), (0, 5), (1, 5), (2, 5), (2, 0), (3, 7), (0, 7), (4, 7), (1, 7), (4, 0), (5, 9), (5, 0),
		(0, 9), (3, 9), (2, 9), (1, 9), (4, 9), (3, 23), (4, 23))
	for p, c in adds:
		check('n2d AddVis(%d, %d)' % (p, c), lambda m=m, p=p, c=c, pts=pts, buf=buf, sets=sets, n2dSets=n2dSets:
			(SDTest.Call('CNodesWorldSup_AddVis', 'tviii', m, pts[p], c), n2dSets(m, buf, sets, pts)))
	for k in range(len(pts)):
		c = cells[k]
		if not n2dI(buf + 8 * c + 4):
			SDTest.Call('CNodesWorldSup_AddVis', 'tviii', m, pts[k], c)
	check('n2d sets', lambda m=m, pts=pts, buf=buf, sets=sets, n2dSets=n2dSets: n2dSets(m, buf, sets, pts))
	for p in range(len(pts)):
		check('n2d VisHas(%d)' % p, lambda m=m, p=p, pts=pts: map(lambda c, m=m, v=pts[p]: SDTest.Call('CNodesWorldSup_VisHas',
			'tbiii', m, v, c)[0], (0, 1, 5, 7, 9, 23)))
		check('n2d NearestVisDist(%d)' % p, lambda m=m, p=p, pos=pos: SDTest.Call('CNodesWorld2D_NearestVisDist', 'tfiF', m, pos[p])[0])
	# the links: LinkPoint of the last one, then all points
	check('n2d LinkPoint', lambda m=m, pts=pts: (SDTest.Call('CNodesWorldSup_LinkPoint', 'tvii', m, pts[5]), n2dI(m + 0x1c4),
		map(lambda v, pts=pts: n2dLinks(v, pts), pts)))
	check('n2d LinkPoints', lambda m=m, pts=pts: (SDTest.Call('CNodesWorldSup_LinkPoints', 'tvi', m), n2dI(m + 0x1c4),
		map(lambda v, pts=pts: n2dLinks(v, pts), pts)))
	for a in range(len(pts)):
		check('n2d Linked(%d)' % a, lambda m=m, a=a, pts=pts: map(lambda b, m=m, a=a, pts=pts: (SDTest.Call('CNodesWorldSup_Linked',
			'tbiii', m, pts[a], pts[b])[0], SDTest.Call('CNodesWorldSup_PointLinked', 'tbiii', m, pts[a], pts[b])[0]), range(len(pts))))
	for a, b, c in ((0, 2, 9), (0, 4, 9), (3, 1, 7), (5, 5, 0), (1, 3, 23), (2, 4, 0)):
		check('n2d Unreachable(%d, %d, %d)' % (a, b, c), lambda m=m, a=a, b=b, c=c, pts=pts: SDTest.Call('CNodesWorldSup_Unreachable',
			'tbiiii', m, pts[a], pts[b], c)[0])
	pl = n2dCalloc(0x18)
	check('n2d GetPairs', lambda m=m, pl=pl, pts=pts: (SDTest.Call('CNodesWorld2D_GetPairs', 'tvii', m, pl), n2dPairs(pl, pts)))
	pair = n2dCalloc(12)
	for a, b, d in ((0, 1, 5.0), (1, 0, 5.0), (2, 3, 1.0), (2, 3, 1.0), (4, 5, 1e9)):
		n2dPoke(pair, pts[a])
		n2dPoke(pair + 4, pts[b])
		n2dSetF(pair + 8, (d,))
		check('n2d InsertPair(%d, %d, %s)' % (a, b, fmt(d)), lambda m=m, pl=pl, pair=pair, pts=pts: (
			SDTest.Call('CNodesWorldSup_InsertPair', 'tviii', m, pl, pair), n2dPairs(pl, pts)))
	for x, a, b in ((0, 0, 1), (0, 3, 2), (0, 1, 4), (0, 5, 5)):
		n2dPoke(pair, x)
		n2dPoke(pair + 4, pts[a])
		n2dPoke(pair + 8, pts[b])
		check('n2d HasPair(%d, %d)' % (a, b), lambda m=m, pl=pl, pair=pair: SDTest.Call('CNodesWorldSup_HasPair', 'tbiii', m, pl,
			pair)[0])
	SDTest.Call('NWVecList_Clear', 'tvi', pl)
	n2dFree(pl)
	n2dFree(pair)
	# the extra points linked to the points they walk to
	SDTest.Call('NWVecList_Add', 'tviF', m + 0x1f0, (-100.0, 40.0, 140.0))
	SDTest.Call('NWVecList_Add', 'tviF', m + 0x1f0, (pp[0], pp[1], pp[2]))
	check('n2d LinkExtra', lambda m=m: (SDTest.Call('CNodesWorld2D_LinkExtra', 'tvi', m), n2dI(m + 0x198), n2dI(m + 0x1c4),
		n2dI(m + 0x1f4) != 0))
	# ReleaseVis of a used set leaves a ring the next walk never leaves, in both runs: only the empty one
	for c in (0,):
		check('n2d ReleaseVis(%d)' % c, lambda m=m, c=c, pts=pts, buf=buf, sets=sets, n2dSets=n2dSets: (
			SDTest.Call('CNodesWorldSup_ReleaseVis', 'tvii', m, c), n2dSets(m, buf, sets, pts)))
	check('n2d FreeTemp', lambda m=m: (SDTest.Call('CNodesWorldSup_FreeTemp', 'tvi', m), n2dI(m + 0x1b0), n2dI(m + 0x1e8)))
	n2dDelete2D(m)

# the links of a point as sorted indices (their order follows the addresses of the points)
def n2dLinks(v, pts):
	l = map(lambda x, pts=pts: n2dIdx(x, pts), n2dRing(v + 0xc, 60))
	l.sort()
	return (n2dI(v + 0x14), l)

def n2dPairs(pl, pts):
	r = []
	h = n2dI(pl)
	p = h
	while p and len(r) < 40:
		r.append((n2dIdx(n2dI(p + 8), pts), n2dIdx(n2dI(p + 0xc), pts), n2dF(p + 0x10, 1)))
		p = n2dI(p)
		if p == h:
			break
	return (n2dI(pl + 8), r)

# ---- the DList.h instances and the grid on their own ----

def n2dLists():
	l = n2dCalloc(0x18)
	# the sets of points (0x2c nodes): Add, the iterator removed, the head removed, Clear with a pool
	d = n2dCalloc(0x24)
	for k in range(3):
		n2dPoke(d, 10 + k)
		n2dPoke(d + 4, k)
		check('n2d VisList_Add(%d)' % k, lambda l=l, d=d: (SDTest.Call('NWVisList_Add', 'tvii', l, d)[0], n2dI(l + 8),
			n2dRing(l, 10)))
	SDTest.Call('NWIntList_Add', 'tvip', n2dI(l) + 0x14, 77)
	n2dPoke(l + 4, n2dI(n2dI(l)))
	check('n2d VisList_RemoveIter', lambda l=l: (SDTest.Call('NWVisList_RemoveIter', 'tvi', l), n2dI(l + 8), n2dRing(l, 10),
		n2dI(l + 4) == n2dI(n2dI(l)), n2dI(l + 0x10)))
	n2dPoke(l + 0x14, 2)
	n2dPoke(l + 4, n2dI(l))
	check('n2d VisList_RemoveIter head', lambda l=l: (SDTest.Call('NWVisList_RemoveIter', 'tvi', l), n2dI(l + 8), n2dRing(l, 10),
		n2dI(l + 4) == n2dI(l), n2dI(l + 0x10)))
	check('n2d VisList_RemoveHead', lambda l=l: (SDTest.Call('NWVisList_RemoveHead', 'tvi', l), n2dI(l + 8), n2dRing(l, 10),
		n2dI(l + 0x10)))
	for k in range(2):
		SDTest.Call('NWVisList_Add', 'tvii', l, d)
	check('n2d VisList_Add pooled', lambda l=l: (n2dI(l + 8), n2dI(l + 0x10), n2dRing(l, 10)))
	SDTest.Call('NWIntList_Add', 'tvip', n2dI(l) + 0x14, 78)
	n2dPoke(l + 0x14, 1)
	check('n2d VisList_Clear', lambda l=l: (SDTest.Call('NWVisList_Clear', 'tvi', l), n2dI(l), n2dI(l + 8), n2dI(l + 0x10)))
	node = SDTest.Call('operator_new', 'ii', 0x2c)[0]
	SDTest.Call('crt_memcpy', 'iiii', node + 8, d, 0x24)
	n2dPoke(node + 0x14, 0)
	n2dPoke(node + 0x18, 0)
	n2dPoke(node + 0x1c, 0)
	n2dPoke(node + 0x20, 0)
	n2dPoke(node + 0x24, 0)
	SDTest.Call('NWIntList_Add', 'tvip', node + 0x14, 79)
	check('n2d VisList_Release deleted', lambda l=l, node=node: (SDTest.Call('NWVisList_Release', 'tvii', l, node)[0] == None,
		n2dI(l + 0x10)))
	n2dPoke(l + 0x14, 0)
	n2dFree(d)
	# the regions of the generation (0x24 nodes)
	r = n2dCalloc(0x1c)
	n2dPoke(r, 5)
	check('n2d RegionList_Add', lambda l=l, r=r: (SDTest.Call('NW2DRegionList_Add', 'tvii', l, r)[0], SDTest.Call('NW2DRegionList_Add', 'tvii',
		l, r)[0], n2dI(l + 8), n2dRing(l, 10)))
	SDTest.Call('NWVecList_Add', 'tviF', n2dI(l) + 0xc, (1.0, 2.0, 3.0))
	n2dPoke(l + 0x14, 1)
	check('n2d RegionList_Clear', lambda l=l: (SDTest.Call('NW2DRegionList_Clear', 'tvi', l), n2dI(l), n2dI(l + 8), n2dI(l + 0x10),
		n2dI(n2dI(l + 0xc) + 0x14)))
	check('n2d RegionList_FreePool', lambda l=l: (SDTest.Call('NW2DRegionList_FreePool', 'tvi', l), n2dI(l + 0xc), n2dI(l + 0x10)))
	n2dPoke(l + 0x14, 0)
	SDTest.Call('NW2DRegionList_Add', 'tvii', l, r)
	SDTest.Call('NWVecList_Add', 'tviF', n2dI(l) + 0xc, (4.0, 5.0, 6.0))
	nd = n2dI(l)
	n2dPoke(l, 0)
	n2dPoke(l + 8, 0)
	check('n2d RegionList_Release deleted', lambda l=l, nd=nd: (SDTest.Call('NW2DRegionList_Release', 'tvii', l, nd)[0] == None,
		n2dI(l + 0x10)))
	SDTest.Call('NWVecList_Add', 'tviF', r + 4, (7.0, 8.0, 9.0))
	SDTest.Call('NWVecList_Add', 'tviF', r + 4, (7.5, 8.5, 9.5))
	n2dPoke(r + 0x18, 1)
	SDTest.Call('NWVecList_RemoveHead', 'tvi', r + 4)
	check('n2d Region_dtor', lambda r=r: (SDTest.Call('NW2DRegion_dtor', 'tvi', r), n2dI(r + 4), n2dI(r + 0xc), n2dI(r + 0x10)))
	n2dFree(r)
	# the vec3 lists: in front of the iterator, at the end; the int list at the end
	for v in ((1.0, 2.0, 3.0), (4.0, 5.0, 6.0)):
		check('n2d VecList_Append%s' % fmt(v), lambda l=l, v=v: (SDTest.Call('NWVecList_Append', 'tviF', l, v)[0],
			n2dVecRing(l, 10)))
	n2dPoke(l + 4, n2dI(l))
	check('n2d VecList_AddBefore head', lambda l=l: (SDTest.Call('NWVecList_AddBefore', 'tviF', l, (7.0, 8.0, 9.0))[0],
		n2dVecRing(l, 10)))
	n2dPoke(l + 4, n2dI(n2dI(l)))
	check('n2d VecList_AddBefore iter', lambda l=l: (SDTest.Call('NWVecList_AddBefore', 'tviF', l, (-1.0, -2.0, -3.0))[0],
		n2dVecRing(l, 10)))
	check('n2d VecList_Add', lambda l=l: (SDTest.Call('NWVecList_Add@b8a90', 'tviF', l, (0.5, 0.25, 0.125))[0], n2dVecRing(l, 10)))
	SDTest.Call('NWVecList_Clear', 'tvi', l)
	for v in (11, 12, 13):
		check('n2d IntList_Append(%d)' % v, lambda l=l, v=v: (SDTest.Call('NWIntList_Append', 'tvip', l, v)[0], n2dI(l + 8),
			n2dRing(l, 10)))
	check('n2d IntList_Add', lambda l=l: (SDTest.Call('NWIntList_Add@b8a20', 'tvip', l, 14)[0], n2dRing(l, 10)))
	# FindSorted on a list sorted by value
	for key in (5, 11, 12, 13, 14, 20):
		check('n2d FindSorted(%d)' % key, lambda l=l, key=key: (lambda r, l=l: (r[0], n2dIdx(r[1], [n2dI(l),
			n2dI(n2dI(l)), n2dI(n2dI(n2dI(l))), n2dI(n2dI(n2dI(n2dI(l))))])))(SDTest.Call('CNodesWorldSup_FindSorted', 'tbiiip', 0, l, key,
			0)))
	n2dPoke(l + 0x14, 3)
	check('n2d IntList_Clear', lambda l=l: (SDTest.Call('NWIntList_Clear@b8f80', 'tvi', l), n2dI(l), n2dI(l + 8), n2dI(l + 0x10)))
	check('n2d IntList_FreeAt0c', lambda l=l: (SDTest.Call('NWIntList_FreeAt0c@b8950', 'tvi', l - 0xc), n2dI(l + 0xc), n2dI(l + 0x10)))
	check('n2d FindSorted empty', lambda l=l: SDTest.Call('CNodesWorldSup_FindSorted', 'tbiiip', 0, l, 1, 5)[0:2])
	n2dPoke(l + 0x14, 0)
	# the free points of the generation deleted with their links
	pl = n2dCalloc(0x18)
	SDTest.Call('NWPointList_Add', 'tviF', pl, (1.0, 2.0, 3.0) + (0.0,) * 12)
	SDTest.Call('NWIntList_Add', 'tvip', n2dI(pl) + 0x14, 5)
	SDTest.Call('NWPointList_Add', 'tviF', pl, (4.0, 5.0, 6.0) + (0.0,) * 12)
	n2dPoke(pl + 0x14, 2)
	SDTest.Call('NWPointList_Clear', 'tvi', pl)
	check('n2d PointList_FreePool', lambda pl=pl: (n2dI(pl + 0x10), SDTest.Call('NWPointList_FreePool', 'tvi', pl), n2dI(pl + 0xc),
		n2dI(pl + 0x10)))
	n2dFree(pl)
	n2dFree(l)
	# a grid on its own
	g = n2dNew(0x3c)
	check('n2d Grid_ctor', lambda g=g: (SDTest.Call('NWGrid_ctor', 'tiiFFiii', g, (100.0, 50.0, 30.0), (-100.0, 0.0, -30.0),
		4, 2, 3)[0] == g, n2dI(g) - 0x400000, n2dI(g + 8), n2dF(g + 0xc, 6), n2dI(g + 0x24), n2dF(g + 0x30, 3),
		map(lambda k, g=g: n2dI(n2dI(g + 4) + 4 * k), range(24))))
	for p, v in (((0.0, 10.0, 0.0), 1), ((-99.0, 49.0, 29.0), 2), ((0.0, 10.0, 0.0), 3), ((100.0, 50.0, 30.0), 4)):
		check('n2d Grid_Add%s %d' % (fmt(p), v), lambda g=g, p=p, v=v: (SDTest.Call('NWGrid_Add', 'tbiFp', g, p, v)[0],
			map(lambda k, g=g: (lambda c: c and n2dRing(c, 10) or 0)(n2dI(n2dI(g + 4) + 4 * k)), range(24))))
	check('n2d Grid_dtor', lambda g=g: (SDTest.Call('NWGrid_dtor', 'tvi', g), n2dI(g) - 0x400000))
	SDTest.Call('NWGrid_ctor', 'tiiFFiii', g, (10.0, 10.0, 10.0), (0.0, 0.0, 0.0), 1, 1, 1)
	SDTest.Call('NWGrid_Add', 'tbiFp', g, (5.0, 5.0, 5.0), 9)
	check('n2d Grid_Free', lambda g=g: (SDTest.Call('NWGrid_Free', 'tvi', g), n2dI(g + 8)))
	SDTest.Call('NWGrid_ctor', 'tiiFFiii', g, (10.0, 10.0, 10.0), (0.0, 0.0, 0.0), 2, 1, 1)
	check('n2d Grid_Delete', lambda g=g: SDTest.Call('NWGrid_Delete', 'tiii', g, 1)[0] == g)
	# CNodesWorldSup on its own: the constructor, the deleting destructor
	s = n2dNew(0x360)
	check('n2d ctor Sup', lambda s=s: (SDTest.Call('CNodesWorldSup_ctor', 'tii', s)[0] == s, n2dFields(s)))
	check('n2d Delete Sup', lambda s=s: SDTest.Call('CNodesWorldSup_Delete', 'tiii', s, 1)[0] == s)
	# the destructor of CNodesWorld2D without the delete
	m = n2dFresh2D()
	check('n2d dtor 2D', lambda m=m: (SDTest.Call('CNodesWorld2D_dtor', 'tvi', m), n2dI(m) - 0x400000, n2dI(m + 0x360)))
	n2dDelete(m, 0x378)

n2dW = SDTest.Peek('g_NodesWorld', 'i')
check('n2d start', lambda: (n2dW != 0, SDTest.Peek('g_NodesWorld2D', 'i') != 0, SDTest.Peek('g_IsMapOutDoor', 'i')))
if n2dW:
	n2dOnW(n2dW)
n2dFiles()
n2dGen()
n2dLists()
