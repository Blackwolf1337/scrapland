# game World/CollisionGrid.c (execfile'd by sdlevel.py, Python 1.5.2): the entity grid of the world (world + 0x1bcc)
# in the level FZ: its box and cells, the entities of every cell by name, the box and segment queries around the
# player (pp), the clamp, the dummy search, the collision masks and the entity lists. Entities are compared by name
# (the hovering Parked moves with the time and is left out), pointers only with != 0. The stamp of the segment
# queries depends on the frames before the script and is not printed; steps and the query node are restored.
def gi(a):
	return SDTest.Peek(a, '@i')
def gname(e):
	if e:
		return SDTest.Peek(e + 4, '@p')
	return None
def gfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def gadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
def gkeep(names):
	r = []
	for n in names:
		if n != 'Parked':
			r.append(n)
	return r
gworld = SDTest.Peek('g_pWorld', 'i')
grid = gworld + 0x1bcc
gplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', gworld, 0)[0]
gp = (pp[0], pp[1], pp[2])
gn = (gi(grid + 0x24), gi(grid + 0x28), gi(grid + 0x2c))
gcells = gi(grid + 0x40)
gsaved = (gi(grid + 0x44), gi(grid + 0x98))
check('grid box', lambda: (gfl(grid, 9), gn, gcells != 0, gi(grid + 0x48) != 0, gi(grid + 0x38) == grid + 0x30))
SDTest.Poke(grid + 0x98 - 0x400000, 0)

# the entities of a cell: the first one, then the next ones of the cell (up to 64)
def gcell(x, y, z):
	r = []
	e = SDTest.Call('Grid_GetCell', 'tiiiii', grid, x, y, z)[0]
	while e and len(r) < 64:
		r.append(gname(e))
		e = SDTest.Call('Grid_NextInCell', 'tii', grid)[0]
	return gkeep(r)
gtotal = gn[0] * gn[1] * gn[2]
if gtotal > 0 and gtotal <= 20000:
	for z in range(gn[2]):
		for y in range(gn[1]):
			for x in range(gn[0]):
				c = gcell(x, y, z)
				if c:
					check('Grid_GetCell' + fmt((x, y, z)), lambda c=c: c)
else:
	check('grid cells', lambda: gtotal)
for x, y, z in ((-1, 0, 0), (0, -1, 0), (0, 0, -1), (gn[0], 0, 0), (0, gn[1], 0), (0, 0, gn[2]), (-100000, 5, 5)):
	check('Grid_GetCell outside' + fmt((x, y, z)), lambda x=x, y=y, z=z: gcell(x, y, z))
for p in (gp, gadd(gp, (500.0, 0.0, 0.0)), (0.0, 0.0, 0.0), (-1e9, 0.0, 1e9)):
	check('Grid_GetCellAt' + fmt(p), lambda p=p: gname(SDTest.Call('Grid_GetCellAt', 'tiiF', grid, p)[0]))
# the cell of the player by its index, its node data is the player
def gpcell():
	t = gi(gplayer + 0x3c)
	if t == grid + 0x30:
		return 'outside'
	if not t:
		return None
	return (t - gcells) / 16
check('player cell', lambda: (gpcell(), gi(gplayer + 0x40) == gplayer))
check('Grid_Add(player)', lambda: (SDTest.Call('Grid_Add', 'tvii', grid, gplayer), gpcell(),
	gname(SDTest.Call('Grid_GetCellAt', 'tiiF', grid, gfl(gplayer + 0x28, 3))[0]) != None))

# box queries: every entity of the cells of the box (the outside list once for each cell out of the box)
def gbox(lo, hi):
	r = []
	e = SDTest.Call('Grid_FirstInBox', 'tiiFF', grid, lo, hi)[0]
	while e and len(r) < 300:
		r.append(gname(e))
		e = SDTest.Call('Grid_NextInBox', 'tii', grid)[0]
	return (gkeep(r), map(lambda i: gi(grid + 0x50 + 4 * i), range(9)))
gbmin = gfl(grid, 3)
gbmax = gfl(grid + 0xc, 3)
for d in (10.0, 500.0, 3000.0):
	check('Grid_FirstInBox' + fmt(d), lambda d=d: gbox(gadd(gp, (-d, -d, -d)), gadd(gp, (d, d, d))))
check('Grid_FirstInBox all', lambda: gbox(gbmin, gbmax))
check('Grid_FirstInBox empty', lambda: gbox(gadd(gp, (100.0, 0.0, 0.0)), gadd(gp, (-100.0, 0.0, 0.0))))
check('Grid_FirstInBox outside', lambda: gbox(gadd(gbmax, (1000.0, 1000.0, 1000.0)), gadd(gbmax, (3000.0, 3000.0,
	3000.0))))
check('Grid_StepCell', lambda: (SDTest.Call('Grid_FirstInBox', 'tiiFF', grid, gp, gadd(gp, (3000.0, 0.0, 3000.0)))[0] != 0,
	map(lambda i: (SDTest.Call('Grid_StepCell', 'tbi', grid)[0], gi(grid + 0x68), gi(grid + 0x6c), gi(grid + 0x70)), range(12))))
SDTest.Call('Grid_EndFrame', 'tvi', grid)
check('Grid_EndFrame', lambda: gi(grid + 0x44))
check('Grid_NextInCell end', lambda: SDTest.Call('Grid_NextInCell', 'tii', grid)[0])

# segment queries: each entity once along the boxes of ext around the segment
def gseg(a, b, ext):
	r = []
	e = SDTest.Call('Grid_FirstOnSegment', 'tiiFFF', grid, a, b, ext)[0]
	steps = gi(grid + 0x98)
	while e and len(r) < 300:
		r.append(gname(e))
		e = SDTest.Call('Grid_NextOnSegment', 'tii', grid)[0]
	return (gkeep(r), steps, gi(grid + 0x98), gfl(grid + 0x80, 3), gfl(grid + 0x8c, 3))
gsegs = ((gp, gadd(gp, (0.0, -5000.0, 0.0))), (gadd(gp, (-3000.0, 50.0, 0.0)), gadd(gp, (3000.0, 50.0, 0.0))),
	(gadd(gp, (0.0, 300.0, -2000.0)), gadd(gp, (0.0, -300.0, 2000.0))), (gp, gp), (gbmin, gbmax))
for a, b in gsegs:
	for ext in ((0.0, 0.0, 0.0), (200.0, 200.0, 200.0), (2000.0, 50.0, 2000.0)):
		check('Grid_FirstOnSegment' + fmt((a, b, ext)), lambda a=a, b=b, ext=ext: gseg(a, b, ext))
gst = SDTest.Peek(grid + 0x4c, '@b')
check('Grid_NewQuery', lambda: (SDTest.Call('Grid_NewQuery', 'tvi', grid), SDTest.Peek(grid + 0x4c, '@b') == (gst % 255) + 1))
for p in (gp, gadd(gbmin, (-10.0, -10.0, -10.0)), gadd(gbmax, (10.0, 10.0, 10.0)), (0.0, 0.0, 0.0),
		(gbmin[0], gbmax[1], gbmax[2])):
	check('Grid_ClampPos' + fmt(p), lambda p=p: SDTest.Call('Grid_ClampPos', 'tiiFF', grid, (9.0, 9.0, 9.0), p)[1])
SDTest.Poke(grid + 0x44 - 0x400000, gsaved[0])
SDTest.Poke(grid + 0x98 - 0x400000, gsaved[1])

# the collision masks
for s in ('VWILR', 'vwilr', 'V', 'Ww', 'Traffic', '', 'L1'):
	check('EntityLists_Get(%s)' % s, lambda s=s: SDTest.Call('EntityLists_Get', 'is', s)[0])

# the dummy search: all dummies, then a pattern (lowercase wildcards)
check('Dummies_Draw', lambda: SDTest.Peek('g_ShowDummy', 'p') == '' and SDTest.Call('Dummies_Draw', 'v'))
for pat, wild in ((None, 0), ('*', 1), ('sdnodummy', 0)):
	SDTest.Call('DummySearch_Start', 'vzi', pat, wild)
	for i in range(6):
		check('DummySearch_NextEuler' + fmt((pat, i)), lambda: (lambda r: (r[0], r[0] and r[1], r[0] and r[2],
			r[0] and r[3]))(SDTest.Call('DummySearch_NextEuler', 'bbiFF', '', 64, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))))
	SDTest.Call('DummySearch_Start', 'vzi', pat, wild)
	for i in range(6):
		check('DummySearch_Next' + fmt((pat, i)), lambda: (lambda r: (r[0], r[0] and r[1], r[0] and r[2],
			r[0] and r[3], r[4] & 0xff))(SDTest.Call('DummySearch_Next', 'bbiFFp', '', 64, (0.0, 0.0, 0.0), (0.0, 0.0), 0)))
	check('DummySearch_Next short' + fmt((pat,)), lambda: (lambda r: (r[0], r[0] and r[1]))(
		SDTest.Call('DummySearch_Next', 'bbiFFp', '', 4, (0.0, 0.0, 0.0), (0.0, 0.0), 0)))

# the entity lists of the world
def gstr(p):
	if p:
		return SDTest.Peek(p, '@s')
	return None
for l in ('Traffic', 'Enemies', 'SDList'):
	lst = SDTest.Call('cWorld_GetList', 'tiis', gworld, l)[0]
	if not lst:
		check('cWorld_GetList(%s)' % l, lambda: None)
		continue
	check('cEntityList(%s)' % l, lambda lst=lst: (SDTest.Peek(lst, '@p'), gi(lst + 0x38) == lst, gi(lst + 0x40) != 0,
		gi(gi(lst + 0x40))))
	check('cEntityList_GetFirst(%s)' % l, lambda lst=lst: gname(SDTest.Call('cEntityList_GetFirst', 'tii', lst)[0]))
	for p, ex in ((gp, 0), (gp, gplayer), (gadd(gp, (5000.0, 0.0, 0.0)), 0), (gadd(gp, (0.0, 30000.0, 0.0)), 0)):
		check('cEntityList_Nearest' + fmt((l, p, ex != 0)), lambda lst=lst, p=p, ex=ex: (lambda r: (gname(r[0]),
			r[2]))(SDTest.Call('cEntityList_Nearest', 'tiiFiF', lst, p, ex, (0.0,))))
	check('cEntityList_Nearest nodist' + fmt((l,)), lambda lst=lst: gname(SDTest.Call('cEntityList_Nearest', 'tiiFii', lst, gp,
		0, 0)[0]))
	for v in ('Num', 'num', 'First', 'first', 'Near', 'NearPlayer0', 'nearPlayer0', 'NearSDNoEntity', 'Count', ''):
		check('cEntityList_Var get' + fmt((l, v)), lambda lst=lst, v=v: (lambda r: (r[0], gstr(r[1]), r[2], r[3]))(
			SDTest.Call('cEntityList_Var', 'tiisipDp', lst, v, 0, 0, 0.5, 77)))
		check('cEntityList_Var get nos' + fmt((l, v)), lambda lst=lst, v=v: (lambda r: (r[0], r[1], r[2]))(
			SDTest.Call('cEntityList_Var', 'tiisiiDp', lst, v, 0, 0, 0.5, 77)))
	for v in ('Num', 'First', 'OnDeath', 'OnDamage', ''):
		check('cEntityList_Var set nos' + fmt((l, v)), lambda lst=lst, v=v: SDTest.Call('cEntityList_Var', 'tiisiiii', lst, v, 1,
			0, 0, 0)[0])
