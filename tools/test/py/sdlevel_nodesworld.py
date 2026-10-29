# game AI/NodesWorld.c (execfile'd by sdlevel.py, Python 1.5.2): the 3D visibility graph of FZ (g_NodesWorld): its
# fields, the nodes with their links and the grid of the points; on fixed points inside the grid the cell and ring
# searches (modes, distances, soft), HasPointVis, PointSeen, NearestPointVis and the race points after a fixed
# srand; TestPath and TestCorner between linked nodes (with the debug segments), the random points near a node, the
# path builder and AnalizeGraph with its lists. Then the DList.h instances on lists of their own and a fresh object
# made and deleted. Nothing here moves: the graph is the one of the level file.
def nwoI(a):
	return SDTest.Peek(a, '@i')
def nwoB(a):
	return SDTest.Peek(a, '@b')
def nwoF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def nwoAlloc(n):
	return SDTest.Call('Mem_Alloc', 'ii', n)[0]
def nwoCalloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def nwoFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def nwoDelete(p, n):
	SDTest.Call('operator_delete_sized', 'vii', p, n)
def nwoSrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def nwoPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def nwoAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])

# the nodes of a pointer list as indices of the node array
def nwoIdx(nodes, lst, n):
	r = []
	for k in range(n):
		r.append((nwoI(lst + 4 * k) - nodes) / 0x14)
	return r

# the data of a ring of 0x14 nodes (vec3 after next and prev), at most m
def nwoVecRing(lst, m):
	r = []
	h = nwoI(lst)
	p = h
	while p and len(r) < m:
		r.append(nwoF(p + 8, 3))
		p = nwoI(p)
		if p == h:
			break
	return (nwoI(lst + 8), r)

def nwoIntRing(lst):
	r = []
	h = nwoI(lst)
	p = h
	while p and len(r) < 50:
		r.append(nwoI(p + 8))
		p = nwoI(p)
		if p == h:
			break
	return (nwoI(lst + 8), nwoI(lst + 4) != 0, nwoI(lst + 0x10), nwoI(lst + 0xc) != 0, r)

# the debug segments of a list, freed after
def nwoSegs(lst):
	r = []
	h = nwoI(lst)
	p = h
	while p and len(r) < 50:
		r.append(nwoF(p + 8, 6))
		q = nwoI(p)
		nwoDelete(p, 0x20)
		p = q
		if p == h:
			break
	n = nwoI(lst + 8)
	nwoPoke(lst, 0)
	nwoPoke(lst + 8, 0)
	return (n, r)

# the result and the out argument k of an SDTest.Call
def nwoPick(r, k):
	return (r[0], r[k])

def nwoInside(p, mn, mx):
	for k in range(3):
		if p[k] < mn[k] or p[k] > mx[k]:
			return 0
	return 1

def nwoMain(w):
	n = nwoI(w + 0x1ac)
	nodes = nwoI(w + 0x1a8)
	grid = nwoI(w + 0x18c)
	check('fields', lambda w=w, n=n: (n, nwoB(w + 0x70), nwoF(w + 4, 24), nwoF(w + 0x64, 3), nwoF(w + 0x74, 6),
		SDTest.Peek(w + 0x8c, '@s'), nwoB(w + 0x18b), nwoI(w + 0x198), nwoI(w + 0x1c0), nwoI(w + 0x1c4),
		nwoI(w + 0x1f8)))
	check('globals', lambda: (SDTest.Call('crt_memcpy', 'iFii', (0.0,) * 1, symva('g_NWNodeRadius'), 4)[1],
		SDTest.Call('crt_memcpy', 'iFii', (0.0,) * 1, symva('g_NWF60'), 4)[1], SDTest.Peek('g_NWDebugSegs', 'i'),
		SDTest.Peek('g_NWDrawMode', 'i')))
	k = 0
	while k < n:
		m = min(5, n - k)
		check('nodes %d' % k, lambda nodes=nodes, k=k, m=m: map(lambda i, nodes=nodes: (nwoF(nodes + 0x14 * i, 3),
			nwoIdx(nodes, nwoI(nodes + 0x14 * i + 0x10), nwoI(nodes + 0x14 * i + 0xc))), range(k, k + m)))
		k = k + 5
	if not grid or n <= 0:
		return
	vt = nwoI(grid)
	nwoCellAt = nwoI(vt + 8) - 0x400000
	gmax = nwoF(grid + 0xc, 3)
	gmin = nwoF(grid + 0x18, 3)
	check('grid', lambda grid=grid, vt=vt: (nwoI(grid + 8), nwoF(grid + 0xc, 6), nwoF(grid + 0x30, 3),
		SDTest.Call(nwoI(vt + 0x18) - 0x400000, 'tvippp', grid, 0, 0, 0),
		SDTest.Call(nwoI(vt + 0x1c) - 0x400000, 'tiiF', grid, (0.0,) * 3)[1]))
	# the test points: some nodes moved a bit, the middle of the grid, the player
	pts = [((gmax[0] + gmin[0]) * 0.5, (gmax[1] + gmin[1]) * 0.5, (gmax[2] + gmin[2]) * 0.5), (pp[0], pp[1], pp[2])]
	for i in (0, n / 7, (2 * n) / 7, n / 2, (5 * n) / 7, n - 1):
		pts.append(nwoAdd(nwoF(nodes + 0x14 * i, 3), (130.0, 40.0, -270.0)))
	lst = nwoAlloc(4 * 40)
	for p in pts:
		if not nwoInside(p, gmin, gmax):
			check('outside%s' % fmt(p), lambda: 1)
			continue
		cell = SDTest.Call(nwoCellAt, 'tbiFp', grid, p, 0)[2]
		for mode, radius, dmin, dmax, mx, use in ((1, 0.0, 0.0, 1e9, 40, 0), (2, 0.0, 500.0, 30000.0, 40, 1),
				(0, 60.0, 0.0, 50000.0, 40, 1), (3, 0.0, 0.0, 1e9, 3, 0)):
			check('CollectCell%s %d %s %s %s %d %d' % (fmt(p), mode, fmt(radius), fmt(dmin), fmt(dmax), mx, use),
				lambda w=w, p=p, cell=cell, lst=lst, mode=mode, radius=radius, dmin=dmin, dmax=dmax, mx=mx, use=use,
				nodes=nodes: (lambda r, nodes=nodes, lst=lst: (r[2], nwoIdx(nodes, lst, r[2])))(SDTest.Call('CNodesWorld_CollectCell',
				'tviFiiipfffii', w, p, cell, lst, mx, 0, radius, dmin, dmax, mode, use)))
		for mode, radius, dmin, dmax, mx, soft in ((1, 0.0, 0.0, 30000.0, 40, 0), (2, 0.0, 0.0, 20000.0, 40, 0),
				(0, 60.0, 1000.0, 40000.0, 40, 0), (1, 0.0, 0.0, 2000.0, 5, 1), (2, 0.0, 15000.0, 15000.0, 40, 1),
				(1, 0.0, 30000.0, 90000.0, 12, 0)):
			check('GetPointsVis%s %d %s %s %s %d %d' % (fmt(p), mode, fmt(radius), fmt(dmin), fmt(dmax), mx, soft),
				lambda w=w, p=p, lst=lst, mode=mode, radius=radius, dmin=dmin, dmax=dmax, mx=mx, soft=soft,
				nodes=nodes: (lambda r, nodes=nodes, lst=lst: (r[2], nwoIdx(nodes, lst, r[2])))(SDTest.Call('CNodesWorld_GetPointsVis',
				'tviFiipfffii', w, p, lst, mx, 0, radius, dmin, dmax, soft, mode)))
			check('SearchRings%s %d %s %s %s %d %d' % (fmt(p), mode, fmt(radius), fmt(dmin), fmt(dmax), mx, soft),
				lambda w=w, p=p, lst=lst, mode=mode, radius=radius, dmin=dmin, dmax=dmax, mx=mx, soft=soft,
				nodes=nodes: (lambda r, nodes=nodes, lst=lst: (r[2], nwoIdx(nodes, lst, r[2])))(SDTest.Call('CNodesWorld_SearchRings',
				'tviFiipfffii', w, p, lst, mx, 0, radius, dmin, dmax, soft, mode)))
		for radius in (0.0, 80.0):
			check('HasPointVis%s %s' % (fmt(p), fmt(radius)), lambda w=w, p=p, radius=radius:
				SDTest.Call('CNodesWorld_HasPointVis', 'tbiFf', w, p, radius)[0])
		check('PointSeen%s' % fmt(p), lambda w=w, p=p: SDTest.Call('CNodesWorld_PointSeen', 'tbiF', w, p)[0])
		# NearestPointVis CHECKs that it found a point: the same search first
		if SDTest.Call('CNodesWorld_GetPointsVis', 'tviFiipfffii', w, p, lst, 7, 0, 0.0, 0.0, nwoF(w + 0x60, 1)[0], 1, 2)[2] > 0:
			check('NearestPointVis%s' % fmt(p), lambda w=w, p=p: SDTest.Call('CNodesWorld_NearestPointVis', 'tviFF', w, p, (0.0,) * 3)[2])
		for dmin, dmax in ((0.0, 20000.0), (5000.0, 60000.0), (100.0, 1000.0)):
			nwoSrand(700 + int(dmin))
			check('GetRacePoint%s %s %s' % (fmt(p), fmt(dmin), fmt(dmax)), lambda w=w, p=p, dmin=dmin, dmax=dmax:
				SDTest.Call('CNodesWorld_GetRacePoint', 'tbiFFffi', w, (0.0,) * 3, p, dmin, dmax, 0)[0:2])
			for d, a in (((1.0, 0.0, 0.0), 1.0), ((0.0, 0.0, -1.0), 0.3), ((0.0, 1.0, 0.0), 3.0)):
				nwoSrand(800 + int(dmin))
				check('GetRacePointDir%s %s %s%s %s' % (fmt(p), fmt(dmin), fmt(dmax), fmt(d), fmt(a)),
					lambda w=w, p=p, dmin=dmin, dmax=dmax, d=d, a=a: SDTest.Call('CNodesWorld_GetRacePointDir', 'tbiFFffiFf', w,
					(0.0,) * 3, p, dmin, dmax, 0, d, a)[0:2])
	nwoFree(lst)
	nwoPaths(w, n, nodes)
	nwoBuild(w, n, nodes)
	nwoAnalize(w)

# TestPath and TestCorner on linked nodes; the debug segments with the debug switch on
def nwoPaths(w, n, nodes):
	dbg = nwoCalloc(0x18)
	sw = SDTest.Peek('g_NWDebugSegs', 'i')
	for i in (0, 1, n / 9, n / 4, n / 3, n / 2, (2 * n) / 3, n - 2):
		a = nodes + 0x14 * i
		if nwoI(a + 0xc) <= 0:
			continue
		b = nwoI(nwoI(a + 0x10))
		pa = nwoF(a, 3)
		pb = nwoF(b, 3)
		pf = nwoF(nodes + 0x14 * ((i + n / 3) % n), 3)
		for q in (pb, pf, nwoAdd(pa, (0.0, 3000.0, 0.0))):
			for radius, mode in ((0.0, 0), (100.0, 0), (400.0, 1), (1500.0, 0)):
				check('TestPath(%d)%s %s %d' % (i, fmt(q), fmt(radius), mode), lambda w=w, pa=pa, q=q, radius=radius,
					mode=mode: SDTest.Call('CNodesWorld_TestPath', 'tbiFFffii', w, pa, q, radius, radius, 0, mode)[0])
		SDTest.Poke('g_NWDebugSegs', 1)
		check('TestPath(%d) segs' % i, lambda w=w, pa=pa, pb=pb, dbg=dbg: (SDTest.Call('CNodesWorld_TestPath', 'tbiFFffii', w, pa, pb,
			50.0, 50.0, dbg, 0)[0], nwoSegs(dbg)))
		SDTest.Poke('g_NWDebugSegs', sw)
		if nwoI(b + 0xc) <= 0:
			continue
		c = nwoI(nwoI(b + 0x10) + 4 * (nwoI(b + 0xc) - 1))
		pc = nwoF(c, 3)
		for radius in (0.0, 100.0, 600.0):
			for flat in (0, 1):
				check('TestCorner(%d)%s %d' % (i, fmt(radius), flat), lambda w=w, pa=pa, pb=pb, pc=pc, radius=radius,
					flat=flat: SDTest.Call('CNodesWorld_TestCorner', 'tbiFFFffii', w, pa, pb, pc, radius, radius, flat, 0)[0])
		SDTest.Poke('g_NWDebugSegs', 1)
		for flat in (0, 1):
			check('TestCorner(%d) segs %d' % (i, flat), lambda w=w, pa=pa, pb=pb, pc=pc, flat=flat, dbg=dbg:
				(SDTest.Call('CNodesWorld_TestCorner', 'tbiFFFffii', w, pa, pb, pc, 40.0, 40.0, flat, dbg)[0], nwoSegs(dbg)))
		SDTest.Poke('g_NWDebugSegs', sw)
		for md in (100.0, 100000.0):
			nwoSrand(900 + i)
			check('RandomNearNode(%d) %s' % (i, fmt(md)), lambda w=w, i=i, md=md: SDTest.Call('CNodesWorld_RandomNearNode', 'tviifF', w, i,
				md, (0.0,) * 3)[1])
		for pos, d, a in ((nwoAdd(pa, (-500.0, 0.0, 300.0)), (1.0, 0.0, 0.0), 2.0),
				(nwoAdd(pa, (40.0, 10.0, -30.0)), (0.0, 0.0, 1.0), 0.5)):
			nwoSrand(950 + i)
			check('RandomAheadNode(%d)%s' % (i, fmt(pos)), lambda w=w, i=i, pos=pos, d=d, a=a: SDTest.Call('CNodesWorld_RandomAheadNode',
				'tviiFfffFfF', w, i, pos, 10.0, 20000.0, 300.0, d, a, (0.0,) * 3)[3])
	nwoFree(dbg)

# the path builder from points near some nodes
def nwoBuild(w, n, nodes):
	path = nwoCalloc(0x18)
	np = nwoAlloc(4)
	for i in (3, n / 5, n / 2, n - 4):
		start = nwoAdd(nwoF(nodes + 0x14 * i, 3), (60.0, 20.0, 45.0))
		for depth, d, direct, flat in ((1, (1.0, 0.0, 0.0), 0, 0), (6, (1.0, 0.0, 0.0), 1, 0),
				(12, (0.0, 0.0, -1.0), 0, 1), (20, (0.3, 0.9, 0.1), 1, 1)):
			nwoPoke(np, nodes + 0x14 * ((i + 7) % n))
			check('BuildPath(%d) %d%s %d %d' % (i, depth, fmt(d), direct, flat), lambda w=w, path=path, np=np,
				depth=depth, d=d, start=start, direct=direct, flat=flat, nodes=nodes: (SDTest.Call('CNodesWorld_BuildPath',
				'tbiiiFFiifi', w, path, depth, d, start, direct, np, 80.0, flat)[0], nwoVecRing(path, 30),
				(nwoI(np) - nodes) / 0x14))
			SDTest.Call('DListVec3_Clear', 'tvi', path)
	nwoFree(np)
	nwoFree(path)

# AnalizeGraph on the graph of the level: its three lists; the switches of the debug view restored after
def nwoAnalize(w):
	s1 = SDTest.Peek('g_NWAnalyzeOn', 'i')
	s2 = SDTest.Peek('g_NWDrawMode', 'i')
	check('AnalizeGraph', lambda w=w: (SDTest.Call('CNodesWorld_AnalyzeGraph', 'tvi', w), SDTest.Peek('g_NWAnalyzeOn', 'i'),
		SDTest.Peek('g_NWDrawMode', 'i'), nwoVecRing(w + 0x310, 30), nwoVecRing(w + 0x328, 30), nwoVecRing(w + 0x340, 30)))
	SDTest.Poke('g_NWAnalyzeOn', s1)
	SDTest.Poke('g_NWDrawMode', s2)
	for off in (0x310, 0x328, 0x340):
		SDTest.Call('DListVec3_Clear', 'tvi', w + off)

# the DList.h instances on lists of their own
def nwoLists():
	l = nwoCalloc(0x18)
	for v, f in ((10, sym('NWIntList_Add')), (20, sym('NWIntList_Add@a24d0')), (30, sym('NWIntList_Add'))):
		check('IntList_Add(%d)' % v, lambda l=l, v=v, f=f: (SDTest.Call(f, 'tvip', l, v)[0], nwoIntRing(l)))
	h = nwoI(l)
	nwoPoke(l + 4, nwoI(h))
	check('IntList_AddBefore iter', lambda l=l: (SDTest.Call('NWIntList_AddBefore', 'tvip', l, 40)[0], nwoIntRing(l), nwoI(l) == h))
	nwoPoke(l + 4, nwoI(l))
	check('IntList_AddBefore head', lambda l=l: (SDTest.Call('NWIntList_AddBefore', 'tvip', l, 50)[0], nwoIntRing(l)))
	nwoPoke(l + 0x14, 2)
	check('IntList_RemoveHead', lambda l=l: (SDTest.Call('NWIntList_RemoveHead', 'tvi', l), nwoIntRing(l)))
	check('IntList_Add pooled', lambda l=l: (SDTest.Call('NWIntList_Add@a24d0', 'tvip', l, 60)[0], nwoIntRing(l)))
	check('IntList_Clear', lambda l=l: (SDTest.Call('NWIntList_Clear', 'tvi', l), nwoIntRing(l)))
	check('IntList_Add 2', lambda l=l: (SDTest.Call('NWIntList_Add', 'tvip', l, 70)[0], SDTest.Call('NWIntList_Add', 'tvip', l, 80)[0],
		nwoIntRing(l)))
	check('IntList_Clear 2', lambda l=l: (SDTest.Call('NWIntList_Clear@a5870', 'tvi', l), nwoIntRing(l)))
	check('IntList_Free', lambda l=l: (SDTest.Call('NWIntList_Free', 'tvi', l), nwoIntRing(l)))
	SDTest.Call('NWIntList_Add', 'tvip', l, 90)
	check('IntList_Free 2', lambda l=l: (SDTest.Call('NWIntList_Free@a22e0', 'tvi', l), nwoIntRing(l)))
	SDTest.Call('NWIntList_Add', 'tvip', l, 91)
	check('IntList_FreeAt0c', lambda l=l: (SDTest.Call('NWIntList_FreeAt0c', 'tvi', l - 0xc), nwoIntRing(l)))
	# the vec3 lists
	for v, f in (((1.0, 2.0, 3.0), sym('NWVecList_Add')), ((4.5, -5.0, 6.25), sym('NWVecList_Add')), ((7.0, 8.0, -9.5), sym('NWVecList_Add'))):
		check('VecList_Add%s' % fmt(v), lambda l=l, v=v, f=f: (SDTest.Call(f, 'tviF', l, v)[0], nwoVecRing(l, 10)))
	check('VecList_RemoveHead', lambda l=l: (SDTest.Call('NWVecList_RemoveHead', 'tvi', l), nwoVecRing(l, 10)))
	check('VecList_RemoveHead 2', lambda l=l: (SDTest.Call('NWVecList_RemoveHead@ac7a0', 'tvi', l), nwoVecRing(l, 10), nwoI(l + 0x10)))
	check('VecList_Clear', lambda l=l: (SDTest.Call('NWVecList_Clear', 'tvi', l), nwoVecRing(l, 10), nwoI(l + 0x10)))
	SDTest.Call('NWVecList_Add', 'tviF', l, (1.0, 1.0, 1.0))
	check('VecList_Clear 2', lambda l=l: (SDTest.Call('NWVecList_Clear@a5990', 'tvi', l), nwoVecRing(l, 10), nwoI(l + 0x10)))
	SDTest.Call('NWVecList_Add', 'tviF', l, (2.0, 2.0, 2.0))
	check('VecList_Free', lambda l=l: (SDTest.Call('NWVecList_Free', 'tvi', l), nwoVecRing(l, 10), nwoI(l + 0x10)))
	nwoPoke(l + 0x14, 0)
	# the triangles and the segments
	tri = (1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0)
	check('TriList_Add', lambda l=l, tri=tri: (SDTest.Call('NWTriList_Add', 'tviF', l, tri)[0], SDTest.Call('NWTriList_Add', 'tviF', l,
		(0.5,) * 15)[0], nwoI(l + 8), nwoF(nwoI(l) + 8, 15), nwoF(nwoI(nwoI(l)) + 8, 15)))
	check('TriList_Clear', lambda l=l: (SDTest.Call('NWTriList_Clear', 'tvi', l), nwoI(l), nwoI(l + 8)))
	for seg in ((1.0, 2.0, 3.0, 4.0, 5.0, 6.0), (-1.0, 0.5, 2.0, 3.0, -4.0, 8.0), (9.0, 9.0, 9.0, 0.0, 0.0, 0.0)):
		check('SegList_Append%s' % fmt(seg), lambda l=l, seg=seg: (SDTest.Call('NWSegList_Append', 'tviF', l, seg)[0],
			nwoI(l + 8), nwoF(nwoI(l) + 8, 6), nwoF(nwoI(nwoI(l) + 4) + 8, 6)))
	check('SegList ring', lambda l=l: nwoSegs(l))
	# the points of the generation: links and box zeroed in a new node, pooled and deleted ones
	check('PointList_NewNode', lambda l=l: (lambda p, l=l: (p != 0, nwoI(p + 0x14), nwoI(p + 0x24), nwoI(p + 0x40),
		nwoDelete(p, 0x44)))(SDTest.Call('NWPointList_NewNode', 'tii', l)[0]))
	pt = (1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0, 5.0, 6.0, -7.0, -8.0, -9.0)
	check('PointList_Add', lambda l=l, pt=pt: (SDTest.Call('NWPointList_Add', 'tviF', l, pt)[0], SDTest.Call('NWPointList_Add', 'tviF', l,
		(0.0,) * 15)[0], nwoI(l + 8), nwoF(nwoI(l) + 8, 3), nwoF(nwoI(nwoI(l)) + 8, 3), nwoF(nwoI(nwoI(l)) + 0x2c, 6)))
	ph = nwoI(l)
	SDTest.Call('NWIntList_Add', 'tvip', ph + 0x14, 5)
	SDTest.Call('NWIntList_Add', 'tvip', ph + 0x14, 6)
	nwoPoke(l + 0x14, 1)
	check('PointList_Clear pooled', lambda l=l, ph=ph: (SDTest.Call('NWPointList_Clear', 'tvi', l), nwoI(l), nwoI(l + 8),
		nwoI(l + 0x10), nwoI(l + 0xc) != 0, nwoIntRing(ph + 0x14)))
	check('PointList_NewNode pooled', lambda l=l, ph=ph: (lambda p, ph=ph: (p != 0, nwoI(p + 0x1c)))(SDTest.Call('NWPointList_NewNode',
		'tii', l)[0]))
	nwoPoke(l + 0x14, 0)
	SDTest.Call('NWPointList_Add', 'tviF', l, pt)
	ph = nwoI(l)
	SDTest.Call('NWIntList_Add', 'tvip', ph + 0x14, 7)
	check('PointList_Release deleted', lambda l=l, ph=ph: (nwoI(l + 8), SDTest.Call('NWPointList_Release', 'tvii', l, ph)[0] == None,
		nwoI(l + 0x10)))
	nwoPoke(l, 0)
	nwoPoke(l + 8, 0)
	b = nwoAlloc(24)
	check('Box_Init', lambda b=b: (SDTest.Call('NWBox_Init', 'tvi', b), nwoF(b, 6)))
	nwoFree(b)
	nwoFree(l)

# a fresh object: the constructor, ClearLists on its empty lists and the deleting destructor
# TestSurface and TestSurfaceAround on the fresh object with the box of the level (they keep the hit segment in it)
def nwoSurface(w, m):
	for k in range(6):
		nwoPoke(m + 0x74 + 4 * k, nwoI(w + 0x74 + 4 * k))
	tri = nwoCalloc(0x44)
	nodes = nwoI(w + 0x1a8)
	n = nwoI(w + 0x1ac)
	for i in (0, n / 3, (2 * n) / 3):
		pa = nwoAdd(nwoF(nodes + 0x14 * i, 3), (10.0, 20.0, 30.0))
		for d in ((0.0, -20000.0, 0.0), (0.0, 30000.0, 0.0), (15000.0, 0.0, 0.0), (0.0, 0.0, -15000.0),
				(3000.0, -9000.0, 2000.0)):
			for keep in (0, 1):
				nwoPoke(m + 0x3b0, keep)
				for k in range(6):
					nwoPoke(m + 0x3c0 + 4 * k, 0)
				check('TestSurface(%d)%s %d' % (i, fmt(d), keep), lambda m=m, pa=pa, d=d, tri=tri:
					(nwoPick(SDTest.Call('CNodesWorld_TestSurface', 'tbiFFFi', m, pa, nwoAdd(pa, d), (0.0,) * 3, tri), 3), nwoF(tri + 0x34, 1),
					nwoI(tri + 0x40) & 1, nwoF(m + 0x3c0, 6)))
		nwoPoke(m + 0x3b0, 0)
		check('TestSurfaceAround(%d)' % i, lambda m=m, pa=pa, tri=tri: (nwoPick(SDTest.Call('CNodesWorld_TestSurfaceAround', 'tbiFFi', m, pa,
			(0.0,) * 3, tri), 2), nwoF(m + 0x3c0, 6)))
	nwoFree(tri)

def nwoFresh(w):
	m = SDTest.Call('operator_new', 'ii', 0x3f0)[0]
	check('fresh ctor', lambda m=m: (SDTest.Call('CNodesWorld_ctor', 'tii', m)[0] == m, nwoI(m) - 0x400000, nwoF(m + 0x40, 1),
		nwoF(m + 0x54, 4), nwoB(m + 0x360), nwoI(m + 0x364), map(lambda k, m=m: nwoI(m + 0x3d8 + 4 * k), range(6)),
		nwoI(m + 0x1ac), nwoI(m + 0x18c)))
	check('fresh ClearLists', lambda m=m: (SDTest.Call('CNodesWorld_ClearLists', 'tvi', m), nwoI(m + 0x190), nwoI(m + 0x1f0)))
	if w and nwoI(w + 0x1ac) > 0:
		nwoSurface(w, m)
	check('fresh Load(-2)', lambda m=m: (SDTest.Call('CNodesWorld_Load', 'tviisF', m, -2, '', (300.0, 400.0, 500.0))[0],
		nwoF(m + 4, 23), nwoF(m + 0x74, 6), nwoB(m + 0x70), nwoI(m + 0x1ac)))
	if w:
		# the graph file of the level again: the same nodes as the graph of the level
		nm = SDTest.Peek(w + 0x8c, '@s')
		check('fresh Load(-1)', lambda m=m, nm=nm: (SDTest.Call('CNodesWorld_Load', 'tviisF', m, -1, nm, (0.0, 0.0, 0.0))[0],
			nwoF(m + 4, 23), nwoF(m + 0x74, 6), SDTest.Peek(m + 0x8c, '@s') == nm, nwoB(m + 0x70), nwoI(m + 0x1ac),
			nwoI(m + 0x18c) != 0))
		check('fresh Load(-1) same', lambda m=m, w=w: (nwoI(m + 0x1ac) == nwoI(w + 0x1ac), nwoF(m + 4, 23) == nwoF(w + 4, 23),
			map(lambda i, a=nwoI(m + 0x1a8), b=nwoI(w + 0x1a8): nwoF(a + 0x14 * i, 3) == nwoF(b + 0x14 * i, 3) and
			nwoI(a + 0x14 * i + 0xc) == nwoI(b + 0x14 * i + 0xc), range(min(nwoI(m + 0x1ac), nwoI(w + 0x1ac)))) ==
			[1] * min(nwoI(m + 0x1ac), nwoI(w + 0x1ac))))
	check('fresh Delete', lambda m=m: SDTest.Call('CNodesWorld_Delete', 'tiii', m, 1)[0] == m)

nwoW = SDTest.Peek('g_NodesWorld', 'i')
check('NodesWorld', lambda: (nwoW != 0, SDTest.Peek('g_IsMapOutDoor', 'i')))
if nwoW:
	nwoMain(nwoW)
nwoLists()
nwoFresh(nwoW)
