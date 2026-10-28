# game AI/AStar.c (execfile'd by sdlevel.py, Python 1.5.2): the A* searches CAStar and CAStar2D over the 3D graph of FZ
# (g_NodesWorld). The searches of g_AStar between points near nodes of the graph and the player (the result, the path,
# the flags, the lists after), fresh CAStar and CAStar2D objects on the same graph (constructor, the elements and
# table items, the searches, the point queries, the closed list, Reset, the destructors), CAStarSup and the hash table
# on their own. Only points inside the grid of the graph: the point searches need a cell.
def aloI(a):
	return SDTest.Peek(a, '@i')
def aloF(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def aloNew(n):
	return SDTest.Call(0x1334d0, 'ii', n)[0]
def aloCalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def aloFree(p):
	SDTest.Call(0x133550, 'ii', p)
def aloDelete(p, n):
	SDTest.Call(0x313634, 'vii', p, n)
def aloSrand(s):
	SDTest.Call(0x323c50, 'vi', s)
def aloAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def aloInside(p, mn, mx):
	for k in range(3):
		if p[k] < mn[k] or p[k] > mx[k]:
			return 0
	return 1

# a node pointer as the index in the node array, -1 for NULL, 'x' for one outside it
def aloIdx(v, nodes, n):
	if v == 0:
		return -1
	if v < nodes or v >= nodes + 0x14 * n or (v - nodes) % 0x14:
		return 'x'
	return (v - nodes) / 0x14

# the points of a path (0x14 nodes), at most m, and the count
def aloPath(lst, m):
	r = []
	h = aloI(lst)
	p = h
	while p and len(r) < m:
		r.append(aloF(p + 8, 3))
		p = aloI(p)
		if p == h:
			break
	return (aloI(lst + 8), r)

# the lists of a search object: numList, numClosed, the open count, the closed nodes (at most 12)
def aloLists(a, nodes, n):
	nc = aloI(a + 0x20)
	cl = []
	for k in range(min(nc, 12)):
		cl.append(aloIdx(aloI(aloI(aloI(a + 0x1c) + 4 * k)), nodes, n))
	return (aloI(a + 0x10), nc, aloI(aloI(a + 0x18) + 8), cl)

# an element of the list: node, parent node, f, g, its table item (index, state, elem)
def aloElem(e, nodes, n):
	par = aloI(e + 4)
	pn = -2
	if par:
		pn = aloIdx(aloI(par), nodes, n)
	info = aloI(e + 0x10)
	iv = 0
	if info:
		iv = (aloI(info), aloI(info + 4), aloI(info + 8), aloI(info + 0xc) == aloI(e))
	return (aloIdx(aloI(e), nodes, n), pn, aloF(e + 8, 2), iv)

# FindPath of 3D and 2D: the result, the path, the two flags and the state
def aloFind(a, path, to, frm, r, stupid, flat, h1, h2):
	if flat:
		res = SDTest.Call(0x09a5e0, 'tbiiFFffffippp', a, path, to, frm, r, 99.0, h1, h2, stupid, 7, 7, 7)
	else:
		res = SDTest.Call(0x099b30, 'tbiiFFffippp', a, path, to, frm, r, 99.0, stupid, 7, 7, 7)
	return (res[0], res[3:6], aloPath(path, 60))

# the pairs of points of the searches: near nodes and the player, inside the grid
def aloPairs(w, n, nodes):
	grid = aloI(w + 0x18c)
	gmax = aloF(grid + 0xc, 3)
	gmin = aloF(grid + 0x18, 3)
	off = (30.0, 20.0, -25.0)
	pn = []
	for i in (0, 5, n / 7, n / 3, n / 2, (2 * n) / 3, n - 1):
		pn.append(aloAdd(aloF(nodes + 0x14 * i, 3), off))
	pl = (pp[0], pp[1], pp[2])
	pairs = [(pn[0], pn[4]), (pn[3], pn[5]), (pl, pn[6]), (pn[1], aloF(nodes + 0x14 * 6, 3)), (pl, pl),
		(pn[2], pl), (pn[6], pn[0]), (aloAdd(pl, (0.0, 300.0, 0.0)), pn[3])]
	r = []
	for a, b in pairs:
		if aloInside(a, gmin, gmax) and aloInside(b, gmin, gmax):
			r.append((a, b))
		else:
			check('alo outside%s%s' % (fmt(a), fmt(b)), lambda: 1)
	return r

# the searches of a search object on the pairs; stupid with a fixed srand
def aloSearches(tag, a, w, n, nodes, pairs, flat):
	path = aloCalloc(0x18)
	k = 0
	for to, frm in pairs:
		for r, stupid in ((0.0, 0), (60.0, 0), (25.0, 4)):
			aloSrand(1200 + k)
			check('alo %s FindPath(%d) %s %d' % (tag, k, fmt(r), stupid), lambda a=a, path=path, to=to, frm=frm, r=r,
				stupid=stupid, flat=flat: aloFind(a, path, to, frm, r, stupid, flat, 30.0, 10.0))
			check('alo %s lists(%d) %s %d' % (tag, k, fmt(r), stupid), lambda a=a, nodes=nodes, n=n: (aloLists(a, nodes,
				n), aloF(a + 0x28, 7)))
			SDTest.Call(0x06d8a0, 'tvi', path)
		k = k + 1
	aloFree(path)

def aloMain(w):
	n = aloI(w + 0x1ac)
	nodes = aloI(w + 0x1a8)
	a = SDTest.Peek(0x444c78, 'i')
	check('alo AStar', lambda a=a, w=w: (a != 0, a and aloI(a) - 0x400000, a and aloI(a + 0x44) == w,
		SDTest.Peek(0x444c90, 'i'), n))
	if not aloI(w + 0x18c) or n <= 0:
		return
	pairs = aloPairs(w, n, nodes)
	if a:
		check('alo AStar fields', lambda a=a: (aloF(a + 4, 1), aloI(a + 0x14), aloI(a + 0x24), aloI(aloI(a + 8)),
			aloI(aloI(a + 0x18) + 0xc)))
		aloSearches('g', a, w, n, nodes, pairs, 0)
	aloFresh(w, n, nodes, pairs)
	aloFresh2D(w, n, nodes, pairs)
	a2 = SDTest.Peek(0x444c7c, 'i')
	if a2:
		w2 = aloI(a2 + 0x44)
		n2 = aloI(w2 + 0x1ac)
		check('alo AStar2D', lambda a2=a2, n2=n2: (aloI(a2) - 0x400000, n2, aloF(a2 + 4, 1)))
		if n2 > 1 and aloI(w2 + 0x18c):
			nodes2 = aloI(w2 + 0x1a8)
			p2 = []
			for i, j in ((0, n2 - 1), (n2 / 2, 0)):
				p2.append((aloAdd(aloF(nodes2 + 0x14 * i, 3), (10.0, 5.0, -10.0)), aloF(nodes2 + 0x14 * j, 3)))
			aloSearches('g2d', a2, w2, n2, nodes2, p2, 1)

# the point queries of a search object
def aloQueries(tag, a, w, n, nodes, pairs, flat):
	lst = aloCalloc(4 * 40)
	k = 0
	for p, q in pairs[0:4]:
		for mx in (1, 7, 30):
			if flat:
				check('alo %s GetPointsVis(%d) %d' % (tag, k, mx), lambda a=a, p=p, lst=lst, mx=mx, nodes=nodes, n=n:
					(lambda r, lst=lst, nodes=nodes, n=n: (r[2], map(lambda i, lst=lst, nodes=nodes, n=n: aloIdx(aloI(lst +
					4 * i), nodes, n), range(r[2]))))(SDTest.Call(0x09a6e0, 'tviFiipfff', a, p, lst, mx, 0, 40.0, 30.0,
					10.0)))
			else:
				check('alo %s GetPointsVis(%d) %d' % (tag, k, mx), lambda a=a, p=p, lst=lst, mx=mx, nodes=nodes, n=n:
					(lambda r, lst=lst, nodes=nodes, n=n: (r[2], map(lambda i, lst=lst, nodes=nodes, n=n: aloIdx(aloI(lst +
					4 * i), nodes, n), range(r[2]))))(SDTest.Call(0x099bf0, 'tviFiipf', a, p, lst, mx, 0, 40.0)))
			pv = (0x099bb0, 0x09a680)[flat]
			check('alo %s PointsVis(%d) %d' % (tag, k, mx), lambda a=a, p=p, lst=lst, mx=mx, nodes=nodes, n=n, pv=pv:
				(lambda r, lst=lst, nodes=nodes, n=n: (r[2], map(lambda i, lst=lst, nodes=nodes, n=n: aloIdx(aloI(lst +
				4 * i), nodes, n), range(r[2]))))(SDTest.Call(pv, 'tviFiip', a, p, lst, mx, 0)))
			pf = (0x099c30, 0x09a740)[flat]
			for file in (0, 1, -1):
				check('alo %s PointsVisFile(%d) %d %d' % (tag, k, mx, file), lambda a=a, p=p, lst=lst, mx=mx, file=file,
					nodes=nodes, n=n, pf=pf: (lambda r, lst=lst, nodes=nodes, n=n: (r[2], r[3], map(lambda i, lst=lst,
					nodes=nodes, n=n: aloIdx(aloI(lst + 4 * i), nodes, n), range(r[2]))))(SDTest.Call(pf, 'tviFiippi', a,
					p, lst, mx, 0, 9, file)))
		tp = (0x0997e0, 0x09a520)[flat]
		check('alo %s TestPath(%d)' % (tag, k), lambda a=a, p=p, q=q, tp=tp: (SDTest.Call(tp, 'tbiFF', a, p, q)[0],
			SDTest.Call(tp, 'tbiFF', a, q, p)[0]))
		check('alo %s Dist(%d)' % (tag, k), lambda a=a, p=p, q=q: (SDTest.Call(0x09b000, 'tfiFF', a, p, q)[0],
			SDTest.Call(0x09b060, 'tfiF', a, p)[0], SDTest.Call(0x09b060, 'tfiF', a, q)[0]))
		k = k + 1
	for i in (0, 1, n / 2, n - 1):
		v = nodes + 0x14 * i
		check('alo %s FileOf(%d)' % (tag, i), lambda a=a, v=v: (SDTest.Call(0x099b10, 'tiii', a, v)[0],
			SDTest.Call(0x09a5d0, 'tiii', a, v)[0]))
		if aloI(v + 0xc) > 0:
			pos = aloF(aloI(aloI(v + 0x10)), 3)
			for f in (0x099e50, 0x09a9c0):
				check('alo %s OtherLinks(%d) %x' % (tag, i, f), lambda a=a, v=v, pos=pos, lst=lst, nodes=nodes, n=n, f=f:
					(lambda r, lst=lst, nodes=nodes, n=n: (r[2], map(lambda k, lst=lst, nodes=nodes, n=n: aloIdx(aloI(lst +
					4 * k), nodes, n), range(min(r[2], 20)))))(SDTest.Call(f, 'tviiFiip', a, v, pos, lst, 20, 0)))
	aloFree(lst)

# the element calls on an object after a search
def aloElems(tag, a, nodes, n):
	lst = aloI(a + 0xc)
	for i in (0, 1, n / 2, n - 1):
		check('alo %s elem(%d)' % (tag, i), lambda lst=lst, i=i, nodes=nodes, n=n: aloElem(lst + 0x14 * i, nodes, n))
		v = nodes + 0x14 * i
		check('alo %s GetElem(%d)' % (tag, i), lambda a=a, v=v, lst=lst, nodes=nodes, n=n: (lambda r, v=v,
			nodes=nodes, n=n: (r[0], r[1] and aloElem(r[1], nodes, n), r[2] != 0 and aloI(r[2] + 0xc) == v))(SDTest.Call(
			0x09b690, 'tbiipp', a, v, 0, 0)))
		check('alo %s TableGet(%d)' % (tag, i), lambda a=a, v=v: (lambda r, v=v: (r[0], r[1] != 0 and aloI(r[1] + 0xc) ==
			v))(SDTest.Call(0x09b710, 'tbiip', a, v, 0)))
	check('alo %s TableGet bad' % tag, lambda a=a, nodes=nodes: SDTest.Call(0x09b710, 'tbiip', a, nodes + 4, 0))
	check('alo %s GetElem bad' % tag, lambda a=a, nodes=nodes: SDTest.Call(0x09b690, 'tbiipp', a, nodes + 4, 0, 0)[0:2])
	check('alo %s GetTable' % tag, lambda a=a: SDTest.Call(0x09b100, 'tii', a)[0] == aloI(a + 8))
	check('alo %s Count' % tag, lambda a=a: SDTest.Call(0x099b20, 'tii', aloI(a + 0x18))[0] == aloI(aloI(a + 0x18) + 8))
	# the closed list: an element in and out again
	nc = aloI(a + 0x20)
	if nc > 2:
		e = aloI(aloI(a + 0x1c) + 4 * (nc - 1))
		pe = aloCalloc(4)
		SDTest.Poke(pe - 0x400000, e)
		check('alo %s CloseRemove' % tag, lambda a=a, nc=nc, nodes=nodes, n=n: (SDTest.Call(0x09aed0, 'tbii', a, nc)[0],
			SDTest.Call(0x09aed0, 'tbii', a, 0)[0], aloLists(a, nodes, n)))
		check('alo %s CloseAdd' % tag, lambda a=a, pe=pe, e=e, nodes=nodes, n=n: (SDTest.Call(0x09ae80, 'tbii', a,
			pe)[0], aloLists(a, nodes, n), aloElem(e, nodes, n)))
		aloFree(pe)
	check('alo %s Reset' % tag, lambda a=a, lst=lst, nodes=nodes, n=n: (SDTest.Call(0x09b7a0, 'tvi', a), aloLists(a,
		nodes, n), map(lambda i, lst=lst, nodes=nodes, n=n: aloElem(lst + 0x14 * i, nodes, n)[3], (0, 1, n / 2))))

# the lists full: AddElem until it fails, then a search that cannot add its start
def aloFull(tag, a, pairs, flat):
	el = (1.0, 2.0, 3.0, 4.0, 5.0)
	check('alo %s AddElem' % tag, lambda a=a, el=el: (aloI(a + 0x10), aloI(a + 0x14), SDTest.Call(0x09b760, 'tbiF', a,
		el)[0], SDTest.Call(0x09b760, 'tbiF', a, el)[0], aloI(a + 0x10)))
	path = aloCalloc(0x18)
	to, frm = pairs[0]
	check('alo %s FindPath full' % tag, lambda a=a, path=path, to=to, frm=frm, flat=flat: aloFind(a, path, to, frm,
		0.0, 0, flat, 30.0, 10.0))
	SDTest.Call(0x06d8a0, 'tvi', path)
	aloFree(path)

def aloFields(a, nodes, n):
	return (aloI(a) - 0x400000, aloF(a + 4, 1), aloI(aloI(a + 8)), aloI(aloI(a + 8) + 4) != 0, aloI(a + 0x10),
		aloI(a + 0x14), aloI(aloI(a + 0x18) + 8), aloI(aloI(a + 0x18) + 0xc), aloI(a + 0x20), aloI(a + 0x24),
		aloF(a + 0x28, 7))

def aloFresh(w, n, nodes, pairs):
	m = aloNew(0x48)
	check('alo fresh ctor', lambda m=m, w=w, nodes=nodes, n=n: (SDTest.Call(0x099330, 'tiii', m, w)[0] == m,
		aloFields(m, nodes, n), aloI(m + 0x44) == w))
	for i in (0, 1, n / 2, n - 1):
		check('alo fresh elem(%d)' % i, lambda m=m, i=i, nodes=nodes, n=n: aloElem(aloI(m + 0xc) + 0x14 * i, nodes, n))
	check('alo fresh DrawDebug', lambda m=m: (SDTest.Peek(0x444c90, 'i'), SDTest.Call(0x099820, 'tvi', m)))
	aloSearches('fresh', m, w, n, nodes, pairs[0:4], 0)
	aloQueries('fresh', m, w, n, nodes, pairs, 0)
	path = aloCalloc(0x18)
	to, frm = pairs[1]
	aloFind(m, path, to, frm, 0.0, 0, 0, 0.0, 0.0)
	SDTest.Call(0x06d8a0, 'tvi', path)
	aloFree(path)
	aloElems('fresh', m, nodes, n)
	# NearestStart CHECKs that the start has a point: the same search first
	lst = aloCalloc(4 * 8)
	if SDTest.Call(0x0a9530, 'tviFiipfffii', w, aloF(m + 0x28, 3), lst, 7, 0, 0.0, 0.0, aloF(w + 0x60, 1)[0], 1,
			2)[2] > 0:
		check('alo fresh NearestStart', lambda m=m: (SDTest.Call(0x099ae0, 'tvi', m), aloF(m + 0x28, 3)))
	aloFree(lst)
	aloFull('fresh', m, pairs, 0)
	check('alo fresh Delete', lambda m=m: SDTest.Call(0x099590, 'tiii', m, 1)[0] == m)
	# the destructor alone
	m = aloNew(0x48)
	SDTest.Call(0x099330, 'tiii', m, w)
	check('alo fresh dtor', lambda m=m: (SDTest.Call(0x0994b0, 'tvi', m), aloI(m) - 0x400000))
	aloDelete(m, 0x48)

def aloFresh2D(w, n, nodes, pairs):
	m = aloNew(0x50)
	check('alo fresh2d ctor', lambda m=m, w=w, nodes=nodes, n=n: (SDTest.Call(0x09a2f0, 'tiii', m, w)[0] == m,
		aloFields(m, nodes, n), aloF(m + 0x48, 2)))
	check('alo fresh2d DrawDebug', lambda m=m: SDTest.Call(0x09a560, 'tvi', m))
	aloSearches('fresh2d', m, w, n, nodes, pairs[0:3], 1)
	check('alo fresh2d h', lambda m=m: (aloF(m + 0x40, 1), aloF(m + 0x48, 2)))
	aloQueries('fresh2d', m, w, n, nodes, pairs[0:3], 1)
	aloElems('fresh2d', m, nodes, n)
	lst = aloCalloc(4 * 8)
	if SDTest.Call(0x0b4890, 'tviFiipfffffii', w, aloF(m + 0x28, 3), lst, 7, 0, 0.0, 0.0, 0.0, 0.0, aloF(w + 0x60, 1)[0],
			1, 2)[2] > 0:
		check('alo fresh2d NearestStart', lambda m=m: (SDTest.Call(0x09a5a0, 'tvi', m), aloF(m + 0x28, 3)))
	aloFree(lst)
	aloFull('fresh2d', m, pairs, 1)
	check('alo fresh2d Delete', lambda m=m: SDTest.Call(0x09a490, 'tiii', m, 1)[0] == m)
	m = aloNew(0x50)
	SDTest.Call(0x09a2f0, 'tiii', m, w)
	check('alo fresh2d dtor', lambda m=m: (SDTest.Call(0x09a420, 'tvi', m), aloI(m) - 0x400000))
	aloDelete(m, 0x50)

# CAStarSup and the hash table on their own
def aloSup():
	m = aloNew(0x44)
	check('alo Sup ctor', lambda m=m: (SDTest.Call(0x09ab50, 'tii', m)[0] == m, aloI(m) - 0x400000, aloF(m + 4, 1),
		aloI(aloI(m + 8)), aloI(m + 0x20), aloF(m + 0x28, 7)))
	check('alo Sup Delete', lambda m=m: SDTest.Call(0x09ad20, 'tiii', m, 1)[0] == m)
	m = aloNew(0x44)
	SDTest.Call(0x09ab50, 'tii', m)
	check('alo Sup dtor', lambda m=m: (SDTest.Call(0x09acb0, 'tvi', m), aloI(m) - 0x400000))
	aloDelete(m, 0x44)
	t = aloCalloc(0xc)
	check('alo Table ctor', lambda t=t: (SDTest.Call(0x09aae0, 'tiii', t, 7)[0] == t, aloI(t), aloI(t + 4) != 0,
		SDTest.Peek(t + 8, '@b'), map(lambda k, t=t: aloI(aloI(t + 4) + 4 * k), range(7))))
	for key in (5, 0x12345, 5, 77):
		check('alo Table Add %x' % key, lambda t=t, key=key: SDTest.Call(0x099620, 'tbiiFi', t, key, (1.0, 2.0, 3.0),
			0)[0])
	check('alo Table dtor', lambda t=t: (SDTest.Call(0x09ae50, 'tvi', t), aloI(t), aloI(t + 4)))
	aloFree(t)
	check('alo InDoor BuildGrid', lambda: SDTest.Call(0x0a1dc0, 'tvi', 0))

# ---- the generation tools of CNodesWorld and CNodesWorld2D on a fresh object with cells and points set up here ----

def aloSetF(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def aloPoke(a, v):
	SDTest.Poke(a - 0x400000, v)

# the index of a pointer in a list of known pointers, 'x' for an unknown one, 0 for NULL
def aloKnown(v, known):
	if v == 0:
		return 0
	for k in range(len(known)):
		if known[k] == v:
			return k + 1
	return 'x'

def aloRing(lst, m):
	r = []
	h = aloI(lst)
	p = h
	while p and len(r) < m:
		r.append(aloI(p + 8))
		p = aloI(p)
		if p == h:
			break
	return r

def aloLinks(v, pts):
	l = map(lambda x, pts=pts: aloKnown(x, pts), aloRing(v + 0xc, 60))
	l.sort()
	return (aloI(v + 0x14), l)

def aloPairList(pl, pts):
	r = []
	h = aloI(pl)
	p = h
	while p and len(r) < 40:
		r.append((aloKnown(aloI(p + 8), pts), aloKnown(aloI(p + 0xc), pts), aloF(p + 0x10, 1)))
		p = aloI(p)
		if p == h:
			break
	return (aloI(pl + 8), r)

# the debug list of the found cells: index and the two points
def aloFoundList(m, pts):
	r = []
	h = aloI(m + 0x25c)
	p = h
	while p and len(r) < 20:
		r.append((aloI(p + 8), aloKnown(aloI(p + 0xc), pts), aloKnown(aloI(p + 0x10), pts)))
		p = aloI(p)
		if p == h:
			break
	return (aloI(m + 0x264), r)

# a buffer of 4 x 2 x 3 cells of 100 50 80 from (-200, dy, 100), 5 hash lists of sets, 6 points in the sets of
# their cells and some more, the cells of tmp1c8 and list1d0
def aloGenSetup(m, dy):
	aloSetF(m + 4, (50.0, 25.0, 40.0, 100.0, 0.0, 0.0, 0.0, 50.0, 0.0, 0.0, 0.0, 80.0, 100.0, 50.0, 80.0, 400.0))
	aloSetF(m + 0x50, (5.0,))
	aloSetF(m + 0x58, (300.0,))
	aloSetF(m + 0x74, (-200.0, dy, 100.0, 200.0, dy + 100.0, 340.0))
	for off, v in ((0x1b4, 4), (0x1b8, 2), (0x1bc, 3), (0x1c0, 24), (0x1ec, 5)):
		aloPoke(m + off, v)
	buf = aloCalloc(24 * 8)
	sets = aloCalloc(5 * 0x18)
	aloPoke(m + 0x1b0, buf)
	aloPoke(m + 0x1e8, sets)
	st = (4, 3, 4, 5, 6, 1, 2, 4, 5, 0)
	for i in range(24):
		aloPoke(buf + 8 * i, st[i % len(st)])
	pos = ((-150.0, 10.0, 120.0), (-120.0, 30.0, 130.0), (-60.0, 20.0, 150.0), (50.0, 70.0, 300.0),
		(150.0, 90.0, 200.0), (-180.0, 60.0, 260.0))
	pts = []
	for p in pos:
		SDTest.Call(0x0a2690, 'tviF', m + 0x190, (p[0], p[1] + dy, p[2]) + (0.0,) * 12)
		pts.append(aloI(m + 0x190) + 8)
	adds = ((0, 0), (1, 0), (0, 5), (1, 5), (2, 5), (2, 0), (3, 7), (0, 7), (4, 7), (1, 7), (4, 0), (5, 9), (5, 0),
		(0, 9), (3, 9), (2, 9), (1, 9), (4, 9), (3, 23), (4, 23), (0, 2), (3, 2), (1, 12), (2, 12), (5, 13), (4, 13))
	for p, c in adds:
		SDTest.Call(0x0ba080, 'tviii', m, pts[p], c)
	# LinkPoints CHECKs that the cell of each point has a set
	for k in range(len(pts)):
		c = SDTest.Call(0x0b98c0, 'tiiF', m, (pos[k][0], pos[k][1] + dy, pos[k][2]))[0]
		if not aloI(buf + 8 * c + 4):
			SDTest.Call(0x0ba080, 'tviii', m, pts[k], c)
	# the searches read the set of every cell they take: the cells without a set 3, 5 or 6
	k = 0
	for i in range(24):
		if not aloI(buf + 8 * i + 4):
			aloPoke(buf + 8 * i, (3, 5, 6)[k % 3])
			k = k + 1
	cl = aloCalloc(4 * 24)
	n = 0
	for i in range(24):
		if not aloI(buf + 8 * i) in (3, 6):
			aloPoke(cl + 4 * n, i)
			n = n + 1
	aloPoke(m + 0x1c8, cl)
	aloPoke(m + 0x1cc, n)
	for i in (3, 8, 13, 18, 23, 2):
		SDTest.Call(0x0a2460, 'tvip', m + 0x1d0, i)
	return (buf, sets, pts, pos)

# the cell searches on the pairs of points
def aloFindLinks(tag, m, pts, dy, rvas):
	pair = aloCalloc(8)
	dm = SDTest.Peek(rvas[3], 'i')
	for a, b in ((0, 1), (0, 2), (1, 2), (3, 4), (0, 4), (2, 5), (5, 3), (1, 1)):
		aloPoke(pair, pts[a])
		aloPoke(pair + 4, pts[b])
		for box in ((-200.0, dy, 100.0, 200.0, dy + 100.0, 340.0), (-200.0, dy, 100.0, 0.0, dy + 50.0, 260.0),
				(-60.0, dy + 10.0, 150.0, 140.0, dy + 90.0, 330.0)):
			check('alo %s FindLinkBox(%d, %d)%s' % (tag, a, b, fmt(box)), lambda m=m, pair=pair, box=box, f=rvas[0]:
				(lambda r: (r[0], r[2]))(SDTest.Call(f, 'tbiiFp', m, pair, box, -1)))
		check('alo %s FindLinkCell(%d, %d)' % (tag, a, b), lambda m=m, pair=pair, f=rvas[1]: SDTest.Call(f, 'tbiip', m,
			pair, -1))
		if rvas[2]:
			check('alo %s FindLinkFree(%d, %d)' % (tag, a, b), lambda m=m, pair=pair, f=rvas[2]: SDTest.Call(f, 'tbiip',
				m, pair, -1))
	# with the debug view on: the found cells into the first debug list
	SDTest.Poke(rvas[3], 1)
	aloPoke(pair, pts[0])
	aloPoke(pair + 4, pts[2])
	check('alo %s FindLink debug' % tag, lambda m=m, pair=pair, rvas=rvas, pts=pts, dy=dy: (SDTest.Call(rvas[0], 'tbiiFp',
		m, pair, (-200.0, dy, 100.0, 200.0, dy + 100.0, 340.0), -1)[0], SDTest.Call(rvas[1], 'tbiip', m, pair, -1),
		rvas[2] and SDTest.Call(rvas[2], 'tbiip', m, pair, -1), aloFoundList(m, pts)))
	SDTest.Poke(rvas[3], dm)
	SDTest.Call(0x0a5900, 'tvi', m + 0x25c)
	aloFree(pair)

def aloGen3D(dy):
	tag = '3d%d' % int(dy)
	m = aloNew(0x3f0)
	SDTest.Call(0x0a21b0, 'tii', m)
	buf, sets, pts, pos = aloGenSetup(m, dy)
	for k in range(len(pos)):
		p = (pos[k][0], pos[k][1] + dy, pos[k][2])
		check('alo %s NearestVisDist(%d)' % (tag, k), lambda m=m, p=p: SDTest.Call(0x0ac5a0, 'tfiF', m, p)[0])
	aloFindLinks(tag, m, pts, dy, (0x0a7e80, 0x0a8270, 0x0a8530, 0x444d48))
	check('alo %s NextInit' % tag, lambda m=m: SDTest.Call(0x0ad3d0, 'tbiF', m, (0.0,) * 3))
	pl = aloCalloc(0x18)
	check('alo %s GetPairs' % tag, lambda m=m, pl=pl, pts=pts: (SDTest.Call(0x0a8a40, 'tvii', m, pl), aloPairList(pl,
		pts)))
	SDTest.Call(0x0a5900, 'tvi', pl)
	# the links: then the cell searches and the pairs again
	check('alo %s LinkPoints' % tag, lambda m=m, pts=pts: (SDTest.Call(0x0ba8d0, 'tvi', m), aloI(m + 0x1c4),
		map(lambda v, pts=pts: aloLinks(v, pts), pts)))
	aloFindLinks(tag + ' linked', m, pts, dy, (0x0a7e80, 0x0a8270, 0x0a8530, 0x444d48))
	check('alo %s GetPairs linked' % tag, lambda m=m, pl=pl, pts=pts: (SDTest.Call(0x0a8a40, 'tvii', m, pl),
		aloPairList(pl, pts)))
	SDTest.Call(0x0a5900, 'tvi', pl)
	aloFree(pl)
	# the extra points linked to the points within linkDist
	SDTest.Call(0x0a2540, 'tviF', m + 0x1f0, (-100.0, 40.0 + dy, 140.0))
	SDTest.Call(0x0a2540, 'tviF', m + 0x1f0, (100.0, 60.0 + dy, 250.0))
	SDTest.Call(0x0a2540, 'tviF', m + 0x1f0, (pp[0], pp[1], pp[2]))
	check('alo %s LinkExtra' % tag, lambda m=m, pts=pts: (SDTest.Call(0x0ac200, 'tvi', m), aloI(m + 0x198),
		aloI(m + 0x1c4), aloI(m + 0x1f4) != 0, aloI(m + 0x194) == 0, map(lambda v, pts=pts: aloLinks(v, pts), pts)))
	check('alo %s FreeTemp' % tag, lambda m=m: (SDTest.Call(0x0b9180, 'tvi', m), aloI(m + 0x1b0), aloI(m + 0x1e8)))
	check('alo %s AddExtraPoints' % tag, lambda m=m: (SDTest.Call(0x0a93d0, 'tvi', m), aloI(m + 0x1f8)))
	check('alo %s Delete' % tag, lambda m=m: SDTest.Call(0x0a2430, 'tiii', m, 1)[0] == m)

def aloGen2D(dy):
	tag = '2d%d' % int(dy)
	m = aloNew(0x378)
	SDTest.Call(0x0ad950, 'tii', m)
	buf, sets, pts, pos = aloGenSetup(m, dy)
	# the free searches walk the bodies on the floor: only far above the level, where there is no floor
	free = 0
	if dy > 100000.0:
		free = 0x0b39d0
	aloFindLinks(tag, m, pts, dy, (0x0b3370, 0x0b3730, free, 0x444ddc))
	if free:
		check('alo %s NextInit' % tag, lambda m=m: SDTest.Call(0x0b7810, 'tbiF', m, (0.0,) * 3))
	check('alo %s FreeTemp' % tag, lambda m=m: (SDTest.Call(0x0b9180, 'tvi', m), aloI(m + 0x1b0), aloI(m + 0x1e8)))
	check('alo %s Delete' % tag, lambda m=m: SDTest.Call(0x0ada70, 'tiii', m, 1)[0] == m)


# the whole generation on a small box of FZ: a fresh CNodesWorld with the node size of the level, the start at a
# free cell found in the air above nodes of the graph; nodeInitDummy 0 (the region may reach the sides of the box),
# then 1 (the level is open there)
def aloPoints(m, pts):
	r = []
	h = aloI(m + 0x190)
	p = h
	while p and len(r) < 40:
		r.append((aloF(p + 8, 3), aloLinks(p + 8, pts), aloF(p + 0x2c, 6)))
		p = aloI(p)
		if p == h:
			break
	return (aloI(m + 0x198), r)

def aloPointList(m):
	r = []
	h = aloI(m + 0x190)
	p = h
	while p and len(r) < 200:
		r.append(p + 8)
		p = aloI(p)
		if p == h:
			break
	return r

def aloStates(m):
	buf = aloI(m + 0x1b0)
	r = []
	if buf:
		for i in range(min(aloI(m + 0x1c0), 200)):
			r.append(aloI(buf + 8 * i))
	return r

def aloSetDummy(m, v):
	w = aloI(m + 0x188)
	aloPoke(m + 0x188, (w & 0xffffff) | (v << 24))

def aloGenerate(w):
	nodes = aloI(w + 0x1a8)
	m = aloNew(0x3f0)
	SDTest.Call(0x0a21b0, 'tii', m)
	SDTest.Call(0x0ab650, 'tviisF', m, -2, '', (1400.0, 1400.0, 1400.0))
	snap = aloI(aloI(m) + 0x1c) - 0x400000
	r = aloF(0x844d30, 1)[0]
	starts = []
	for k in (100, 300, 400, 600):
		for dy in (0.0, 1500.0, 3000.0):
			p = aloF(nodes + 0x14 * k, 3)
			p = SDTest.Call(snap, 'tiiFF', m, (0.0,) * 3, (p[0], p[1] + dy, p[2]))[1]
			if not SDTest.Call(0x212fe0, 'bFFF', p, (-700.0, -700.0, -700.0), (700.0, 700.0, 700.0))[0] and \
					not SDTest.Call(0x213d60, 'bFFfii', p, p, r, 0, 0)[0]:
				starts.append(p)
				break
	check('alo gen starts', lambda starts=starts: starts)
	k = 0
	for p in starts[0:3]:
		for f40, sz in ((3000.0, 2), (25000.0, 1), (2000.0, 3)):
			aloSetF(m + 0x40, (f40,))
			aloSetDummy(m, 0)
			s = sz * 1400.0
			mn = (p[0] - s, p[1] - s, p[2] - s)
			mx = (p[0] + s, p[1] + s, p[2] + s)
			check('alo gen Generate(%d) %s %d' % (k, fmt(f40), sz), lambda m=m, p=p, mn=mn, mx=mx: (SDTest.Call(0x0a7a00,
				'tbiFFF', m, p, mn, mx)[0], aloI(m + 0x198), aloI(m + 0x1c4), aloI(m + 0x1c0), aloI(m + 0x1cc),
				aloI(m + 0x1f8), aloF(m + 0x304, 3), SDTest.Peek(m + 0x368, '@b')))
			pts = aloPointList(m)
			check('alo gen points(%d) %s %d' % (k, fmt(f40), sz), lambda m=m, pts=pts: aloPoints(m, pts))
			check('alo gen states(%d) %s %d' % (k, fmt(f40), sz), lambda m=m: aloStates(m))
			if k == 0 and sz == 3:
				SDTest.Call(aloI(aloI(m) + 0x50) - 0x400000, 'tvi', m)
				aloDrawViews('gen', m, 0x0a5b10, 0x444d48, (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), pts)
			SDTest.Call(aloI(aloI(m) + 0x90) - 0x400000, 'tvi', m)
			SDTest.Call(0x0a5ab0, 'tvi', m + 0x190)
		# the region reaches the side of the box: open
		aloSetDummy(m, 1)
		s = 1400.0
		mn = (p[0] - s, p[1] - s, p[2] - s)
		mx = (p[0] + s, p[1] + s, p[2] + s)
		check('alo gen Generate(%d) open' % k, lambda m=m, p=p, mn=mn, mx=mx: (SDTest.Call(0x0a7a00, 'tbiFFF', m, p, mn,
			mx)[0], aloI(m + 0x198), aloI(m + 0x1cc), aloF(m + 0x304, 3), SDTest.Peek(m + 0x368, '@b')))
		SDTest.Call(aloI(aloI(m) + 0x90) - 0x400000, 'tvi', m)
		SDTest.Call(0x0a5ab0, 'tvi', m + 0x190)
		k = k + 1
	check('alo gen Delete', lambda m=m: SDTest.Call(0x0a2430, 'tiii', m, 1)[0] == m)

# the 2D steps of the generation far above the level, where the bodies find no floor (the walk tests are not
# reached): the buffer, a region with the debug regions on, the next start
def aloGenerate2D():
	m = aloNew(0x378)
	SDTest.Call(0x0ad950, 'tii', m)
	SDTest.Call(0x0b6200, 'tviisF', m, -2, '', (300.0, 400.0, 500.0))
	q = (1000.0, 1000000.0, -2000.0)
	mn = (q[0] - 600.0, q[1] - 800.0, q[2] - 1000.0)
	mx = (q[0] + 600.0, q[1] + 800.0, q[2] + 1000.0)
	# the buffer is relative to the box of the object (Generate sets it)
	aloSetF(m + 0x74, mn + mx)
	p = SDTest.Call(aloI(aloI(m) + 0x1c) - 0x400000, 'tiiFF', m, (0.0,) * 3, q)[1]
	check('alo gen2d IniBuffer', lambda m=m, p=p, mn=mn, mx=mx: (SDTest.Call(0x0b5860, 'tbiFFF', m, p, mn, mx)[0],
		aloI(m + 0x1c0), aloI(m + 0x1cc), aloI(m + 0x1ec), aloStates(m)))
	SDTest.Call(0x0a2690, 'tviF', m + 0x190, p + (0.0,) * 6 + (3.4e38, 3.4e38, 3.4e38, -3.4e38, -3.4e38, -3.4e38))
	dm = SDTest.Peek(0x444ddc, 'i')
	SDTest.Poke(0x444ddc, 1)
	check('alo gen2d CreateRegion', lambda m=m, p=p: (SDTest.Call(0x0b20f0, 'tviF', m, p)[0], aloStates(m),
		aloI(m + 0x1d8), aloI(m + 0x368), aloI(m + 0x360) and aloI(aloI(m + 0x360) + 8), aloI(m + 0x360) and
		aloI(aloI(m + 0x360) + 0x14), aloF(aloI(m + 0x190) + 0x2c, 6)))
	SDTest.Poke(0x444ddc, dm)
	check('alo gen2d NextInit', lambda m=m: SDTest.Call(0x0b7810, 'tbiF', m, (0.0,) * 3))
	SDTest.Call(aloI(aloI(m) + 0x50) - 0x400000, 'tvi', m)
	sn = SDTest.Peek(0x444e54, 'i')
	for v in (-1, 2):
		SDTest.Poke(0x444e54, v)
		aloDrawViews('gen2d%d' % v, m, 0x0b1210, 0x444ddc, (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11), [])
	SDTest.Poke(0x444e54, sn)
	SDTest.Call(aloI(aloI(m) + 0x90) - 0x400000, 'tvi', m)
	check('alo gen2d Delete', lambda m=m: SDTest.Call(0x0ada70, 'tiii', m, 1)[0] == m)

# the debug views through the D3D log (the length and the hash of the logged text): the views of all modes on an
# object with lists, a small node array of three nodes and the error lists set up here
def aloD3D(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	fn()
	SDTest.D3DLog(0)
	t = SDTest.Out()
	return (len(t), hash(t))

def aloDrawViews(tag, m, rva, mode, modes, pts):
	font = SDTest.Peek(0x4a8908, 'i')
	keep = (aloI(font + 0x10), aloI(font + 0x14))
	nodes = aloCalloc(3 * 0x14)
	links = aloCalloc(4 * 4)
	pos = ((100.0, 200.0, 300.0), (-50.0, 20.0, 400.0), (0.0, -10.0, -20.0))
	for i in range(3):
		aloSetF(nodes + 0x14 * i, pos[i])
	aloPoke(links, nodes + 0x14)
	aloPoke(links + 4, nodes + 0x28)
	aloPoke(links + 8, nodes)
	aloPoke(nodes + 0xc, 2)
	aloPoke(nodes + 0x10, links)
	aloPoke(nodes + 0x14 + 0xc, 1)
	aloPoke(nodes + 0x14 + 0x10, links + 8)
	aloPoke(m + 0x1a8, nodes)
	aloPoke(m + 0x1ac, 3)
	# links through the level: 6 points (two triples in 3D, three pairs in 2D), 3 nodes without links, 2 one way
	# links
	for off, n in ((0x310, 6), (0x328, 3), (0x340, 4)):
		for i in range(n):
			p = pos[i % 3]
			SDTest.Call(0x06d720, 'tviF', m + off, (p[0] + i, p[1], p[2] - i))
	SDTest.Call(0x06d720, 'tviF', m + 0x1f0, (5.0, 6.0, 7.0))
	dm = SDTest.Peek(mode, 'i')
	for v in modes:
		SDTest.Poke(mode, v)
		check('alo %s Draw(%d)' % (tag, v), lambda m=m, rva=rva, font=font: (aloD3D(lambda m=m, rva=rva:
			SDTest.Call(rva, 'tvi', m)), aloI(font + 0x10), aloI(font + 0x14)))
	SDTest.Poke(mode, dm)
	aloPoke(font + 0x10, keep[0])
	aloPoke(font + 0x14, keep[1])
	aloPoke(m + 0x1a8, 0)
	aloPoke(m + 0x1ac, 0)
	for off in (0x310, 0x328, 0x340, 0x1f0):
		SDTest.Call(0x06d8a0, 'tvi', m + off)
	aloFree(links)
	aloFree(nodes)

aloW = SDTest.Peek(0x444d34, 'i')
check('alo start', lambda: (aloW != 0, SDTest.Peek(0x444c7c, 'i') != 0))
if aloW:
	aloMain(aloW)
aloSup()
aloGen3D(0.0)
aloGen3D(1000000.0)
aloGen2D(0.0)
aloGen2D(1000000.0)
if aloW:
	aloGenerate(aloW)
aloGenerate2D()
