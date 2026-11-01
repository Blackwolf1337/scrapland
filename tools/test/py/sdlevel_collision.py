# game ScrapEng/ScrapEngCollision.c (execfile'd by sdlevel.py, Python 1.5.2): the queries against the floor of the
# level around the player (pp), with the c_* switches that change their paths, the box cache of a moving sphere,
# the segment-sphere math and CVertList_GetPos on fake lists. Vectors go in and out as 'F' arguments.
def cvi(a):
	return SDTest.Peek(a, '@i')
def cvz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def cvfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def cvset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def cvadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
p0 = (pp[0], pp[1], pp[2])
cvpts = ((p0, cvadd(p0, (0, -5000.0, 0))), (p0, cvadd(p0, (3000.0, 0, 0))), (cvadd(p0, (0, 300.0, 0)),
	cvadd(p0, (0, -300.0, 0))), (p0, cvadd(p0, (0, 200.0, 0))), (p0, p0), ((0.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
	(cvadd(p0, (-800.0, -40.0, 600.0)), cvadd(p0, (900.0, -60.0, -700.0))))
# the c_* switches: schk, caps, iter, hold, sld2, sldi, scts, sbox, sboxshow
cvvars = (sym('g_CollisionSchk'), sym('g_CollisionCaps'), sym('g_CollisionIter'), sym('g_CollisionHold'), sym('g_CollisionSld2'), sym('g_CollisionSldi'), sym('g_CollisionScts'), sym('g_CollisionSbox'), sym('g_CollisionSboxShow'))
cvsaved = map(lambda a: SDTest.Peek(a, 'i'), cvvars)
check('collision vars', lambda: cvsaved)

for a, b in cvpts:
	check('Engine_TestLine' + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('Engine_TestLine', 'bFFii', a, b, 0, 0)[0],
		SDTest.Call('Engine_TestLine', 'bFFFi', a, b, (9.0, 9.0, 9.0), 0)[3], SDTest.Call('Engine_TestLine', 'bFFFF', a, b,
		(9.0, 9.0, 9.0), (0.0,) * 17)[3:], SDTest.Call('Engine_TestLine', 'bFFiF', a, b, 0, (0.0,) * 17)[3]))
for r in (5.0, 60.0, 400.0):
	check('Collision_Check' + fmt((r,)), lambda r=r: SDTest.Call('Collision_Check', 'bFf', p0, r)[0])
	for a, b in cvpts:
		for fn, nm in ((sym('Engine_TestSphere'), 'Engine_TestSphere'), (sym('Collision_Slide'), 'Collision_Slide'), (sym('Collision_Slide2D'), 'Collision_Slide2D')):
			check(nm + fmt((a, b, r)), lambda fn=fn, a=a, b=b, r=r: (SDTest.Call(fn, 'bFFfFi', a, b, r, (7.0, 7.0, 7.0),
				0), SDTest.Call(fn, 'bFFfii', a, b, r, 0, 0)[0]))
# the switches one at a time
for i, v in ((0, 0), (1, 1), (2, 2), (3, 1), (4, 1), (5, 1), (4, 0), (5, 0), (6, 1)):
	SDTest.Poke(cvvars[i], v)
	for a, b in cvpts[2:5]:
		for fn, nm in ((sym('Engine_TestSphere'), 'Engine_TestSphere'), (sym('Collision_Slide'), 'Collision_Slide'), (sym('Collision_Slide2D'), 'Collision_Slide2D')):
			check(nm + ' var' + fmt((i, v, a, b)), lambda fn=fn, a=a, b=b: (SDTest.Call(fn, 'bFFfFi', a, b, 60.0,
				(7.0, 7.0, 7.0), 0), SDTest.Call(fn, 'bFFfii', a, b, 60.0, 0, 0)[0]))
	check('Collision_Check var' + fmt((i, v)), lambda: SDTest.Call('Collision_Check', 'bFf', p0, 60.0)[0])
	SDTest.Poke(cvvars[i], cvsaved[i])

# the box cache along a path (an inverted box is a CHECK in checkbbox.cpp), then far away, then back; c_sbox off
def cvcache(c):
	return (cvi(c) & 0xffff, cvi(c) >> 16 & 0xffff, cvi(c + 4) != 0, cvfl(c + 8, 6))
cvc = cvz(0x20)
path = []
for k in range(30):
	path.append(cvadd(p0, (k * 7.0, 150.0, k * 3.0)))
path = path + [cvadd(p0, (20000.0, 0, 0)), cvadd(p0, (20010.0, 0, 0))] + path[:24]
for k in range(1, len(path)):
	check('Collision_SphereBox' + fmt((k,)), lambda k=k: (SDTest.Call('Collision_SphereBox', 'bFFfi', path[k - 1], path[k], 20.0,
		cvc)[0], cvcache(cvc)))
for fn, nm in ((sym('Engine_TestSphere'), 'Engine_TestSphere'), (sym('Collision_Slide'), 'Collision_Slide'), (sym('Collision_Slide2D'), 'Collision_Slide2D')):
	for k in range(1, 6):
		check(nm + ' cache' + fmt((k,)), lambda fn=fn, k=k: (SDTest.Call(fn, 'bFFfFi', path[k - 1], path[k], 20.0,
			(7.0, 7.0, 7.0), cvc), cvcache(cvc)))
SDTest.Poke(cvvars[7], 0)
check('Collision_SphereBox off', lambda: (SDTest.Call('Collision_SphereBox', 'bFFfi', path[0], path[1], 20.0, cvc)[0], cvcache(cvc)))
SDTest.Poke(cvvars[7], cvsaved[7])
check('Collision_SphereBox NULL', lambda: SDTest.Call('Collision_SphereBox', 'bFFfi', path[0], path[1], 20.0, 0)[0])
for lo, hi in (((-10.0, -10.0, -10.0), (10.0, 10.0, 10.0)), ((-1.0, -500.0, -1.0), (1.0, 0.0, 1.0)),
	((-3000.0, -3000.0, -3000.0), (3000.0, 3000.0, 3000.0))):
	for pos in (p0, (0.0, 0.0, 0.0), cvadd(p0, (0, -200.0, 0))):
		check('Collision_Box' + fmt((pos, lo, hi)), lambda pos=pos, lo=lo, hi=hi: SDTest.Call('Collision_Box', 'bFFF', pos, lo,
			hi)[0])

cvsec = SDTest.Call('Sector_At', 'iF', p0)[0]
check('Sector_At', lambda: cvsec)
if cvsec >= 0:
	for a, b in cvpts:
		for front in (0, 1):
			check('Collision_Sector' + fmt((a, b, front)), lambda a=a, b=b, front=front: SDTest.Call('Collision_Sector', 'iiFFi',
				cvsec, a, b, front)[0])
def cveps():
	return (cvfl(symva('g_CollisionEpsilon'), 1)[0], cvfl(symva('g_LastCollisionEpsilon'), 1)[0])
check('CollisionEpsilon_Set', lambda: (cveps(), SDTest.Call('CollisionEpsilon_Set', 'vi', 0), cveps(), SDTest.Call('CollisionEpsilon_Set', 'vi',
	1), cveps()))

# the segment-sphere math
for a, b in (((0.0, 0.0, 0.0), (10.0, 0.0, 0.0)), ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((-5.0, 1.0, 2.0), (5.0, 1.0, 2.0)),
	((1.0, 1.0, 1.0), (1.0, 1.0, 1.00001)), ((3.0, -2.0, 7.0), (-4.0, 5.5, -1.25))):
	for c, r in (((5.0, 0.0, 0.0), 1.0), ((5.0, 3.0, 0.0), 1.0), ((0.0, 0.0, 0.0), 2.0), ((12.0, 0.0, 0.0), 1.5),
		((0.5, 1.0, 1.0), 0.75), ((-20.0, 0.0, 0.0), 1.0), ((1.0, 1.0, 1.0), 0.0)):
		check('Collision_SegSphere' + fmt((a, b, c, r)), lambda a=a, b=b, c=c, r=r: (SDTest.Call('Collision_SegSphere', 'bFFffffF',
			a, b, c[0], c[1], c[2], r, (8.0, 8.0, 8.0)), SDTest.Call('Collision_SegSphere', 'bFFffffi', a, b, c[0], c[1],
			c[2], r, 0)[0]))

# CVertList_GetPos on fake lists of 4 vertices of each format (the XBOX position packed with VertList_PackNormal)
cvl = cvz(0x14)
cvv = cvz(0x100)
for i in range(0x40):
	SDTest.Poke(cvv + 4 * i - 0x400000, 0x3f800000 + i * 0x10101)
SDTest.Poke(cvl - 0x400000, cvv)
SDTest.Poke(cvl + 4 - 0x400000, 4)
SDTest.Poke(cvl + 8 - 0x400000, 4)
cvset(cvl + 0x10, (2.5,))
for frmt in (1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13):
	SDTest.Poke(cvl + 0xc - 0x400000, frmt)
	for i in (0, 3):
		check('CVertList_GetPos' + fmt((frmt, i)), lambda i=i: SDTest.Call('CVertList_GetPos', 'tiiFi', cvl, (0.0,) * 3, i)[1])
SDTest.Poke(cvl - 0x400000, 0)
