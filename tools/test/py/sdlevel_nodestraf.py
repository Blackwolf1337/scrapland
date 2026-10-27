# game AI/NodesTraffic.c (execfile'd by sdlevel.py, Python 1.5.2): the traffic graph of FZ (g_NodesTraffic): the tracks
# and their nodes, the grid of the nodes and its cells, the node queries on fixed points (the node 100 away after and
# before, the directions, the searches by type and distance, the random points and nodes after a fixed srand), the load
# of a fresh graph from the dummies (with and without the filled in nodes) and a grid of its own. The vehicle counts
# of the tracks change with the traffic of the level: they are 0 during the searches and restored after them, and only
# their change is printed.
def ntrI(a):
	return SDTest.Peek(a, '@i')
def ntrB(a):
	return SDTest.Peek(a, '@b')
def ntrF(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def ntrAlloc(n):
	return SDTest.Call(0x1335a0, 'ii', n)[0]
def ntrFree(p):
	SDTest.Call(0x133550, 'ii', p)
def ntrSrand(s):
	SDTest.Call(0x323c50, 'vi', s)
def ntrPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def ntrAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])

# the nodes of a node array, 21 a line
def ntrNodes(name, nodes, n):
	k = 0
	while k < n:
		m = min(21, n - k)
		check('%s %d' % (name, k), lambda a=nodes + 12 * k, m=m: ntrF(a, 3 * m))
		k = k + 21

def ntrTrackFields(tr):
	return (ntrI(tr + 4), ntrI(tr + 8), ntrB(tr + 0xc), ntrF(tr + 0x10, 2), ntrI(tr + 0x18), ntrI(tr + 0x20))

# the (track, node) pairs of a cell ring
def ntrCellNodes(c):
	r = []
	if not c:
		return r
	h = ntrI(c)
	n = h
	while n and len(r) < 2000:
		r.append((ntrI(n + 8), ntrI(n + 12)))
		n = ntrI(n)
		if n == h:
			break
	return (ntrI(c + 8), r)

def ntrGridCheck(name, g, all):
	cells = ntrI(g + 4)
	num = ntrI(g + 8)
	check(name + ' grid', lambda g=g, cells=cells, num=num: (cells != 0, num, ntrI(g + 0x24), ntrI(g + 0x28),
		ntrI(g + 0x2c), ntrF(g + 0xc, 9)))
	if not all or num <= 0 or num > 20000:
		return
	for i in range(num):
		c = ntrI(cells + 4 * i)
		if c:
			check('%s cell %d' % (name, i), lambda c=c: ntrCellNodes(c))

# the cell functions on a point inside the grid
def ntrGridPoint(name, g, p):
	ins = SDTest.Call(0x0a0640, 'tbiF', g, p)[0]
	check('%s IsInside%s' % (name, fmt(p)), lambda ins=ins: ins)
	if not ins:
		return
	check('%s CellIndex%s' % (name, fmt(p)), lambda g=g, p=p: SDTest.Call(0x09d6e0, 'tiiF', g, p)[0])
	check('%s CellCoords%s' % (name, fmt(p)), lambda g=g, p=p: SDTest.Call(0x09d600, 'tbiFppp', g, p, 0, 0, 0))
	check('%s GetCellAt%s' % (name, fmt(p)), lambda g=g, p=p: ntrCellNodes(SDTest.Call(0x09d820, 'tbiFp', g, p,
		0)[2]))

def ntrList(lst, n):
	r = []
	for i in range(n):
		r.append((ntrI(lst + 8 * i), ntrI(lst + 8 * i + 4)))
	return r

def ntrFind(t, lst, p, typ, mx, radius, dmin, dmax, soft, vis):
	r = SDTest.Call(0x09dc40, 'tviFiiipfffii', t, p, typ, lst, mx, -1, radius, dmin, dmax, soft, vis)
	return (r[2], ntrList(lst, min(r[2], mx)))

def ntrMain(t):
	nt = SDTest.Call(0x09ece0, 'tii', t)[0]
	# the floats at +0x40 are written only for a level with tracks (FZ has none)
	check('NumTracks', lambda t=t, nt=nt: (nt, ntrI(t + 0x3c), ntrF(t + 4, 1), nt and ntrF(t + 0x40, 6)))
	for i in range(6):
		check('NumByType(%d)' % i, lambda t=t, i=i: SDTest.Call(0x09ec90, 'tiii', t, i)[0])
	for i in range(5):
		check('type list %d' % i, lambda t=t, i=i: map(lambda k, l=ntrI(t + 0x10 + 4 * i): ntrI(l + 4 * k),
			range(ntrI(t + 0x28 + 4 * i))))
	tracks = ntrI(t + 0xc)
	info = ntrAlloc(24)
	saved = []
	for i in range(nt):
		tr = tracks + 0x24 * i
		saved.append(ntrI(tr + 0x1c))
		ntrPoke(tr + 0x1c, 0)
		check('track %d' % i, lambda tr=tr: ntrTrackFields(tr))
		ntrNodes('track %d nodes' % i, ntrI(tr), ntrI(tr + 4))
		check('NumNodes(%d)' % i, lambda t=t, i=i: SDTest.Call(0x09ec70, 'tiii', t, i)[0])
		check('GetRadius(%d)' % i, lambda t=t, i=i: SDTest.Call(0x09ee90, 'tfii', t, i)[0])
		check('GetInfo(%d)' % i, lambda t=t, i=i, info=info: (SDTest.Call(0x09d740, 'tiiii', t, info, i)[0] == info,
			ntrI(info), ntrI(info + 4), ntrB(info + 8), ntrF(info + 12, 2), ntrI(info + 20)))
	ntrFree(info)
	if nt > 0:
		tr = tracks
		v0 = ntrI(tr + 0x1c)
		SDTest.Call(0x0a0620, 'tvii', t, 0)
		v1 = ntrI(tr + 0x1c)
		SDTest.Call(0x09d280, 'tvii', t, 0)
		check('AddVehicle RemoveVehicle(0)', lambda v0=v0, v1=v1, tr=tr: (v1 - v0, ntrI(tr + 0x1c) - v0))
	# the node queries on some nodes of each track
	for i in range(nt):
		n = ntrI(tracks + 0x24 * i + 4)
		nodes = ntrI(tracks + 0x24 * i)
		for k in (0, 1, 2, n / 2, n - 2, n - 1):
			if k < 0 or k >= n:
				continue
			np = ntrF(nodes + 12 * k, 3)
			check('GetNode(%d, %d)' % (i, k), lambda t=t, i=i, k=k: SDTest.Call(0x09ecf0, 'tbiFii', t, (0.0,) * 3, i, k))
			check('NextAway(%d, %d)' % (i, k), lambda t=t, i=i, k=k: SDTest.Call(0x09d8a0, 'tbiFiipp', t, (0.0,) * 3,
				i, k, -1, -1))
			check('PrevAway(%d, %d)' % (i, k), lambda t=t, i=i, k=k: SDTest.Call(0x09ed50, 'tbiFiipp', t, (0.0,) * 3,
				i, k, -1, -1))
			check('GetDirs(%d, %d)' % (i, k), lambda t=t, i=i, k=k: SDTest.Call(0x09f450, 'tbiFFii', t, (9.0,) * 3,
				(9.0,) * 3, i, k))
			ntrSrand(100 + 7 * i + k)
			check('RandomPointNear(%d, %d)' % (i, k), lambda t=t, i=i, k=k: SDTest.Call(0x09eed0, 'tviiifFpp', t, i, k,
				500.0, (0.0,) * 3, -1, -1))
			ntrSrand(200 + 7 * i + k)
			check('RandomPointNear(%d, %d) far' % (i, k), lambda t=t, i=i, k=k: SDTest.Call(0x09eed0, 'tviiifFpp', t,
				i, k, 100000.0, (0.0,) * 3, -1, -1))
			for pos, d, a in ((ntrAdd(np, (-300.0, 50.0, 200.0)), (1.0, 0.0, 0.0), 3.0),
					(ntrAdd(np, (40.0, 0.0, -30.0)), (0.0, 0.0, 1.0), 0.5),
					(ntrAdd(np, (0.0, 2000.0, 0.0)), (0.0, -1.0, 0.0), 1.5)):
				ntrSrand(300 + 7 * i + k)
				check('RandomPointAhead(%d, %d)%s' % (i, k, fmt(pos)), lambda t=t, i=i, k=k, pos=pos, d=d, a=a:
					SDTest.Call(0x09f120, 'tviiiFffiFfFpp', t, i, k, pos, 10.0, 3000.0, 0, d, a, (0.0,) * 3, -1, -1))
	g = ntrI(t + 8)
	if g:
		ntrSearch(t, nt, tracks, g)
	ntrInDoor(t, nt, tracks)
	for i in range(nt):
		ntrPoke(tracks + 0x24 * i + 0x1c, saved[i])
	if SDTest.Peek(0x444d0c, 'i') == 0 and SDTest.Peek(0x444cdc, 'i') == 0:
		check('Draw state 0', lambda t=t: SDTest.Call(0x09cfa0, 'tvi', t))
	check('DrawTrack(NumTracks)', lambda t=t, nt=nt: SDTest.Call(0x09d040, 'tvii', t, nt))

# the grid of the graph and the searches on it
def ntrSearch(t, nt, tracks, g):
	ntrGridCheck('traffic', g, 1)
	# the search points: first node of each track moved a bit, the middle of the grid box, the player
	gmax = ntrF(g + 0xc, 3)
	gmin = ntrF(g + 0x18, 3)
	pts = [((gmax[0] + gmin[0]) * 0.5, (gmax[1] + gmin[1]) * 0.5, (gmax[2] + gmin[2]) * 0.5), (pp[0], pp[1], pp[2]),
		gmin, gmax]
	for i in range(nt):
		pts.append(ntrAdd(ntrF(ntrI(tracks + 0x24 * i), 3), (50.0, 30.0, -40.0)))
	lst = ntrAlloc(8 * 300)
	for p in pts:
		ntrGridPoint('traffic', g, p)
		if not SDTest.Call(0x0a0640, 'tbiF', g, p)[0]:
			continue
		for typ in range(5):
			for dmin, dmax, soft, mx in ((0.0, 3000.0, 0, 300), (500.0, 20000.0, 0, 300), (0.0, 100000.0, 0, 300),
					(0.0, 2000.0, 1, 300), (0.0, 50000.0, 0, 3), (1000.0, 1000.0, 0, 300)):
				check('FindNodes%s %d %s %s %d %d' % (fmt(p), typ, fmt(dmin), fmt(dmax), soft, mx),
					lambda t=t, lst=lst, p=p, typ=typ, mx=mx, dmin=dmin, dmax=dmax, soft=soft:
					ntrFind(t, lst, p, typ, mx, 0.0, dmin, dmax, soft, 0))
			check('FindNodes vis%s %d' % (fmt(p), typ), lambda t=t, lst=lst, p=p, typ=typ:
				ntrFind(t, lst, p, typ, 300, 10.0, 0.0, 30000.0, 0, 1))
			for d, a in (((1.0, 0.0, 0.0), 3.2), ((0.0, 0.0, -1.0), 0.8)):
				ntrSrand(400 + typ)
				check('RandomNode%s %d%s %s' % (fmt(p), typ, fmt(d), fmt(a)), lambda t=t, p=p, typ=typ, d=d, a=a:
					SDTest.Call(0x09d9f0, 'tbiFiFffiFfpp', t, (0.0,) * 3, typ, p, 0.0, 40000.0, 0, d, a, -1, -1))
	ntrFree(lst)

# NodesTrafficInDoor.c on lists of its own: the sector counts and the tracks of the sectors, then an object of its own
# (calloc, the vtable and the tracks of the graph of FZ, no constructor: that would set the frame callback)
# with sectors made of the tracks of each type: the track test, the hidden node, the vehicle lists and the respawn
# point for fake AIs of the three vehicle classes
def ntrCalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def ntrSectors(l, n):
	r = []
	for i in range(n):
		s = l + 0x18 * i
		tr = ntrI(s + 0xc)
		r.append((ntrI(s), ntrI(s + 8), ntrI(s + 0x10), ntrB(s + 0x14), map(lambda k, tr=tr: ntrI(tr + 4 * k),
			range(ntrI(s + 0x10)))))
	return r
def ntrInDoor(t, nt, tracks):
	l = ntrCalloc(20 * 0x18)
	cnt = ntrCalloc(4)
	for sec in (5, 7, 5, 9, 7, 5, 11):
		check('TrafficSectors_Add(%d)' % sec, lambda l=l, cnt=cnt, sec=sec: (SDTest.Call(0x0a1b10, 'tviiii', 0, l, cnt,
			sec), ntrI(cnt)))
	ntrFree(cnt)
	check('TrafficSectors_Add sectors', lambda l=l: map(lambda i, l=l: (ntrI(l + 0x18 * i), ntrI(l + 0x18 * i + 8)),
		range(5)))
	# two sectors with nodes, their tracks each once
	pairs = ((3, 0), (1, 2), (3, 5), (2, 1), (1, 7), (6, 6))
	nodes = ntrAlloc(8 * len(pairs))
	k = 0
	for a, b in pairs:
		ntrPoke(nodes + 8 * k, a)
		ntrPoke(nodes + 8 * k + 4, b)
		k = k + 1
	ntrPoke(l + 4, nodes)
	ntrPoke(l + 8, 5)
	ntrPoke(l + 0x18 + 4, nodes + 8 * 5)
	ntrPoke(l + 0x18 + 8, 1)
	check('TrafficSectors_SetTracks', lambda l=l: (SDTest.Call(0x0a1b90, 'tviii', 0, l, 2), ntrSectors(l, 2)))
	ntrFree(ntrI(l + 0xc))
	ntrFree(ntrI(l + 0x18 + 0xc))
	ntrFree(nodes)
	ntrFree(l)
	f = ntrCalloc(0xfa4)
	ntrPoke(f, ntrI(t))
	ntrPoke(f + 0xc, tracks)
	ntrPoke(f + 0x24, nt)
	ntrPoke(f + 0x3c, ntrI(t + 0x3c))
	lists = []
	for k in range(5):
		m = ntrI(t + 0x28 + 4 * k)
		if m <= 0:
			continue
		src = ntrI(t + 0x10 + 4 * k)
		secs = ntrCalloc(2 * 0x18)
		for s in range(2):
			tr = ntrAlloc(4 * m)
			for j in range(m):
				ntrPoke(tr + 4 * j, ntrI(src + 4 * j))
			ntrPoke(secs + 0x18 * s, 100 + 10 * k + s)
			ntrPoke(secs + 0x18 * s + 8, 3 + 2 * s)
			ntrPoke(secs + 0x18 * s + 0xc, tr)
			ntrPoke(secs + 0x18 * s + 0x10, m)
			ntrPoke(secs + 0x18 * s + 0x14, s == 0 or k % 2)
		ntrPoke(f + 0x70 + 8 * k, secs)
		ntrPoke(f + 0x74 + 8 * k, 2)
		lists.append(secs)
		check('indoor sectors %d' % k, lambda secs=secs: ntrSectors(secs, 2))
	for i in range(nt):
		check('HiddenNode(%d)' % i, lambda f=f, i=i: SDTest.Call(0x0a10b0, 'tiii', f, i)[0])
		for typ in range(5):
			check('SectorHasTrack(%d, %d)' % (i, typ), lambda f=f, i=i, typ=typ: SDTest.Call(0x0a1d00, 'tbiii', f, i,
				typ)[0])
	veh = ntrCalloc(0x600)
	ai = ntrCalloc(0x400)
	ntrPoke(ai + 0x190, veh)
	for cls in (0, 1, 2):
		ntrPoke(veh + 0x5e8, cls)
		for sd in (1, 2, 3):
			ntrSrand(500 + 10 * cls + sd)
			check('GetRespawnPoint class %d %d' % (cls, sd), lambda f=f, ai=ai: SDTest.Call(0x0a1dd0, 'tbiipFpp', f, ai,
				-1, (0.0,) * 3, -1, -1))
		check('AddVehicle class %d' % cls, lambda f=f, ai=ai: (SDTest.Call(0x0a1c80, 'tvii', f, ai),
			ntrI(f + 0x228), ntrI(f + 0x614), ntrI(f + 0xde8)))
	check('AI lists', lambda f=f, ai=ai: (ntrI(f + 0x98) == ai, ntrI(f + 0x22c) == ai, ntrI(f + 0x618) == ai))
	ntrFree(ai)
	ntrFree(veh)
	for secs in lists:
		ntrFree(ntrI(secs + 0xc))
		ntrFree(ntrI(secs + 0x18 + 0xc))
		ntrFree(secs)
	ntrFree(f)

# a fresh graph: Init (Load with fill) is the graph of the level again; Load without fill only the dummies
def ntrFresh(t, fill):
	m = SDTest.Call(0x1334d0, 'ii', 0x70)[0]
	check('fresh ctor %d' % fill, lambda m=m: SDTest.Call(0x09b970, 'tii', m)[0] == m)
	if fill:
		SDTest.Call(0x09f720, 'tvi', m)
	else:
		SDTest.Call(0x09f730, 'tvii', m, 0)
	nt = ntrI(m + 0x24)
	check('fresh %d' % fill, lambda m=m, nt=nt: (nt, ntrI(m + 0x3c), ntrF(m + 4, 1), ntrF(m + 0x40, 6),
		map(lambda i, m=m: ntrI(m + 0x28 + 4 * i), range(5))))
	for i in range(5):
		check('fresh %d type list %d' % (fill, i), lambda m=m, i=i: map(lambda k, l=ntrI(m + 0x10 + 4 * i):
			ntrI(l + 4 * k), range(ntrI(m + 0x28 + 4 * i))))
	tracks = ntrI(m + 0xc)
	for i in range(nt):
		tr = tracks + 0x24 * i
		check('fresh %d track %d' % (fill, i), lambda tr=tr: ntrTrackFields(tr))
		if fill and t and i < ntrI(t + 0x24):
			ot = ntrI(t + 0xc) + 0x24 * i
			check('fresh %d track %d same nodes' % (fill, i), lambda tr=tr, ot=ot: ntrI(tr + 4) == ntrI(ot + 4) and
				map(lambda k, a=ntrI(tr), b=ntrI(ot): ntrI(a + 4 * k) == ntrI(b + 4 * k), range(3 * ntrI(tr + 4))) ==
				[1] * (3 * ntrI(tr + 4)))
		else:
			ntrNodes('fresh %d track %d nodes' % (fill, i), ntrI(tr), ntrI(tr + 4))
	g = ntrI(m + 8)
	if g:
		ntrGridCheck('fresh %d' % fill, g, not fill)
	check('fresh %d Delete' % fill, lambda m=m: SDTest.Call(0x09bbf0, 'tiii', m, 1)[0] == m)

# a grid of its own: 3 2 4 cells, nodes added by hand, the cell functions, then deleted
def ntrOwnGrid():
	g = SDTest.Call(0x1334d0, 'ii', 0x3c)[0]
	check('own grid ctor', lambda g=g: SDTest.Call(0x09b820, 'tiiFFiii', g, (1000.0, 500.0, 2000.0),
		(-1000.0, -500.0, 0.0), 3, 2, 4)[0] == g)
	info = ntrAlloc(8)
	pts = ((0.0, 0.0, 0.0), (-1000.0, -500.0, 0.0), (1000.0, 500.0, 2000.0), (999.0, 499.0, 1999.0), (-333.4, 0.1, 500.0),
		(-333.3, 0.1, 500.0), (250.0, -250.0, 1250.0), (0.0, 0.0, 0.0), (700.0, 100.0, 1600.0))
	k = 0
	for p in pts:
		ntrPoke(info, k)
		ntrPoke(info + 4, 10 * k + 1)
		check('own grid Add%s' % fmt(p), lambda g=g, p=p, info=info: SDTest.Call(0x0a0540, 'tbiFi', g, p, info)[0])
		k = k + 1
	ntrFree(info)
	ntrGridCheck('own', g, 1)
	for p in pts + ((1000.1, 0.0, 0.0), (0.0, -500.1, 0.0), (0.0, 0.0, 2000.5), (-2000.0, 0.0, 0.0)):
		ntrGridPoint('own', g, p)
	for x, y, z in ((0, 0, 0), (2, 1, 3), (1, 0, 2), (0, 1, 1)):
		check('own grid GetCell(%d, %d, %d)' % (x, y, z), lambda g=g, x=x, y=y, z=z: (lambda r: (r[0], r[1], r[2], r[3],
			ntrCellNodes(r[4])))(SDTest.Call(0x09d7b0, 'tbipppp', g, x, y, z, 0)))
	check('own grid GetDivs', lambda g=g: SDTest.Call(0x09ec50, 'tvippp', g, 0, 0, 0))
	check('own grid GetCellSize', lambda g=g: SDTest.Call(0x09d9d0, 'tviF', g, (0.0,) * 3))
	check('own grid Delete', lambda g=g: SDTest.Call(0x09bb90, 'tiii', g, 1)[0] == g)

ntrT = SDTest.Peek(0x444c98, 'i')
check('NodesTraffic', lambda: (ntrT != 0, SDTest.Peek(0x4677e8, 'i')))
if ntrT:
	ntrMain(ntrT)
ntrFresh(ntrT, 1)
ntrFresh(ntrT, 0)
ntrOwnGrid()
