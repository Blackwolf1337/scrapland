# engine3d Engine/Portal.c: a portal on a node made here (Python 1.5.2, runs inside sdcall.py after the other checks:
# check, fmt come from there). Floats are printed as their bits. Not tested: the debug drawing.
def ponew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def poi(addr):
	return SDTest.Peek(addr, '@i')
def points(addr, n):
	r = []
	for i in range(n):
		r.append(poi(addr + 4 * i))
	return r
def ponode(name, m):
	n = SDTest.Call('CNode3D_ctor', 'tii', ponew(0xfc))[0]
	SDTest.Call('String_Set', 'tvis', n + 4, name)
	SDTest.Poke(n + 8 - 0x400000, 1)
	SDTest.Call('crt_memcpy', 'iiFi', n + 0x5c, m, 64)
	return n
# a node turned 90 degrees about x (the portal stands upright) and moved
pn = ponode('sdportal', (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 3.0, 4.0, 5.0, 1.0))
po = SDTest.Call('CPortal_ctor', 'tii', ponew(0x68))[0]
check('CPortal_ctor', lambda: points(po + 4, 9))
SDTest.Call('CObjBase3D_SetNode', 'tvii', po, pn)
SDTest.Call('crt_memcpy', 'iiFi', po + 0xc, (4.0, 2.0), 8)
for p in ((3.0, 4.0, 5.0), (3.0, 4.0, 8.0), (10.0, 4.0, 5.0), (3.0, -6.0, 5.0), (1.0, 6.0, 2.0), (-2.5, 9.0, 4.0)):
	check('CPortal_PlaneDist' + fmt(p), lambda p=p: SDTest.Call('CPortal_PlaneDist', 'tfiF', po, p)[0])
	check('CPortal_Dist' + fmt(p), lambda p=p: SDTest.Call('CPortal_Dist', 'tfiF', po, p)[0])
check('CPortal cache', lambda: (poi(po + 0x24) & 0xff, points(po + 0x28, 16)))
for a, b in (((3.0, 4.0, 3.0), (3.0, 4.0, 7.0)), ((3.0, 4.0, 3.0), (3.0, 4.0, 4.0)), ((10.0, 4.0, 3.0), (10.0, 4.0, 7.0)),
		((2.0, 4.5, 6.0), (4.0, 3.5, 4.0)), ((1.0, 5.0, 4.0), (1.0, 5.0, 6.0))):
	check('CPortal_SegCross' + fmt((a, b)), lambda a=a, b=b: SDTest.Call('CPortal_SegCross', 'tbiFFF', po, a, b, (9.0, 9.0, 9.0)))
check('CPortal_SegCross NULL', lambda: SDTest.Call('CPortal_SegCross', 'tbiFFi', po, (3.0, 4.0, 3.0), (3.0, 4.0, 7.0), 0)[0])
z1 = ponode('sdzona1', (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
z2 = ponode('sdzona2', (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
for i, z in ((0, z1), (1, z2), (2, z2), (1, 0), (3, z1)):
	check('CPortal_SetZone' + fmt((i, z != 0)), lambda i=i, z=z: (SDTest.Call('CPortal_SetZone', 'tviii', po, i, z),
		poi(po + 0x14) == z1, poi(po + 0x14) == z2, poi(po + 0x18) == z1, poi(po + 0x18) == z2, poi(z1 + 8), poi(z2 + 8)))
check('CPortal_Delete', lambda: (SDTest.Call('CPortal_Delete', 'tiii', po, 1)[0] == po, poi(pn + 8), poi(z1 + 8), poi(z2 + 8)))
