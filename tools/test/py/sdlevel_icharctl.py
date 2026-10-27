# game AI/ICharControl.c, ICharControl2D.c, ICharControl3D.c (execfile'd by sdlevel.py, Python 1.5.2): the move control
# of the character AI. Two WalkChars of our own far above the map at fixed coordinates ("SDIccWalk" with a fake action
# controller whose class only has the radii, "SDIccWalk2" as the other character), a fake AI in calloc memory (its
# vtable: GetTarget returns the node at 0x38 with SDIccWalk2 as target, Event the exe's "xor al,al; ret 4"), and one 2D
# and one 3D control made by their constructors on them. On those the pure parts (normalize, distances, the keep and
# strafe distances, the angle tests, the floor tests of the 2D control up there), the setup of the moves (SetMove with
# the kinds that need no path search, the speeds, Away, SetDir, GoToDirect), the turn of the view, the walk to and
# from the target, the arrival, the follow update on its direct branch, the strafe tests (2D with the forward side only)
# and the destructors. Not called: the path searches (FZ has no 2D graph, and the 3D searches CHECK for points outside
# the graph) and everything that goes through them (GoTo, Follow, Route, the follow update after a lost way).
def icci(a):
	return SDTest.Peek(a, '@i')
def iccb(a):
	return SDTest.Peek(a, '@b')
def iccf(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def iccset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def iccpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def iccpokeb(a, v):
	w = icci(a & ~3)
	sh = 8 * (a & 3)
	iccpoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def icccalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def iccfree(p):
	SDTest.Call(0x133550, 'ii', p)
def iccints(a, offs):
	r = ()
	for o in offs:
		r = r + (icci(a + o),)
	return r
def iccbytes(a, offs):
	r = ()
	for o in offs:
		r = r + (iccb(a + o),)
	return r
def iccpos(e):
	return SDTest.Call(icci(icci(e) + 0x18) - 0x400000, 'tiiF', e, (0.0, 0.0, 0.0))[1]
def iccsetpos(e, p):
	SDTest.Call(icci(icci(e) + 0x1c) - 0x400000, 'tviF', e, p)
def iccadd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
# the high words of doubles (the low words 0) for the variadic SetMove
iccD = {1.0: 0x3ff00000, 2.0: 0x40000000, 0.5: 0x3fe00000, 100.0: 0x40590000, 300.0: 0x4072c000, 0.75: 0x3fe80000,
	250.0: 0x406f4000, 1.5: 0x3ff80000, 50.0: 0x40490000}

# the pure normalize
for iccv in ((3.0, 4.0, 12.0), (0.0, 0.0, 0.0), (-1.0, 0.0, 0.0), (1e-20, 2e-20, 0.0), (1e20, -3e20, 7.0)):
	check('ICC Vec3_NormalizeTo %s' % fmt(iccv), lambda v=iccv: SDTest.Call(0x072340, 'iFF', (0.0, 0.0, 0.0), v)[1])

iccworld = SDTest.Peek(0x467450, 'i')
iccP = (-37000.0, 61000.0, 43000.0)
SDTest.Call(0x323c50, 'vi', 911)
iccw = SDTest.Call(0x208360, 'tiisfffs', iccworld, 'SDIccWalk', iccP[0], iccP[1], iccP[2], 'WalkChar')[0]
iccw2 = SDTest.Call(0x208360, 'tiisfffs', iccworld, 'SDIccWalk2', iccP[0] + 300.0, iccP[1], iccP[2] + 100.0,
	'WalkChar')[0]
check('ICC WalkChars', lambda: (iccw != 0, iccw2 != 0, iccw and icci(iccw + 0x234), iccw2 and icci(iccw2 + 0x78)))
if iccw and iccw2:
	iccsetpos(iccw, iccP)
	iccsetpos(iccw2, (iccP[0] + 300.0, iccP[1], iccP[2] + 100.0))
	# the fake action controller of SDIccWalk: the class with the radius and the floor radius
	iccac = icccalloc(0x80)
	icccls = icccalloc(0x200)
	iccpoke(iccac + 8, icccls)
	iccset(icccls + 0x164, (40.0,))
	iccset(icccls + 0x16c, (25.0,))
	iccpoke(iccw + 0x234, iccac)
	iccset(iccw + 0x45c, (30.0,))
	# the fake AI
	iccAV = icccalloc(0x80)
	iccA = icccalloc(0x100)
	iccpoke(iccAV + 0x38, 0x485730)
	iccpoke(iccAV + 0x58, 0x46a4f0)
	iccpoke(iccA, iccAV)
	iccpoke(iccA + 0x38, iccA + 0x38)
	iccpoke(iccA + 0x3c, iccA + 0x38)
	iccpoke(iccA + 0x40, iccw2)
	iccnode = iccA + 0x38
	iccarg = icccalloc(8)
	iccm2 = SDTest.Call(0x1334d0, 'ii', 0x360)[0]
	iccc2 = SDTest.Call(0x073180, 'tiiiii', iccm2, iccw, iccA, iccarg)[0]
	iccm3 = SDTest.Call(0x1334d0, 'ii', 0x360)[0]
	iccc3 = SDTest.Call(0x079d10, 'tiiiii', iccm3, iccw, iccA, iccarg)[0]
	def iccbase(c):
		return (icci(c) - 0x400000, icci(c + 0x44) == iccA, icci(c + 0x48) == iccw, icci(c + 0x4c) == iccw + 0x4ec,
			iccf(c + 0x50, 4), iccints(c, (0x74, 0x78, 0x7c, 0x94, 0x98, 0x9c, 0x1fc, 0x20c)), iccf(c + 0x8c, 2),
			iccf(c + 0xe8, 4), iccf(c + 0x204, 2), iccf(c + 0x220, 3), iccf(c + 0x230, 3),
			iccbytes(c, (0xbc, 0xbd, 0xd4, 0xd5, 0x140, 0x141, 0x1d0, 0x200, 0x210, 0x22e, 0x22f)))
	check('ICC ctor 2D', lambda: (iccc2 == iccm2, iccbase(iccc2)))
	check('ICC ctor 3D', lambda: (iccc3 == iccm3, iccbase(iccc3)))
	iccc = ((iccc2, '2D', 0x078030, 0x078080, 0x0780c0, 0x078170, 0x0784b0, 0x078550, 0x078220, 0x0782e0, 0x0783a0,
		0x0731f0, 0x077720, 0x077930, 0x077640, 0x077050, 0x077170, 0x0789e0),
		(iccc3, '3D', 0x07e630, 0x07e690, 0x07e6e0, 0x07e800, 0x07eac0, 0x07eb60, 0x07e850, 0x07e910, 0x07e9d0,
		0x079d80, 0x07df30, 0x07e160, 0x07de50, 0x07d890, 0x07da00, 0x07ee50))
	iccpts = ((0.0, 0.0, 0.0), (3.0, -4.0, 12.0), (-250.5, 10.25, 1000.0), (1e6, 1e6, -1e6))
	for iccr in iccc:
		iccC = iccr[0]
		iccn = iccr[1]
		# the distances
		for iccp in iccpts:
			check('ICC %s Dist %s' % (iccn, fmt(iccp)), lambda r=iccr, p=iccp: (SDTest.Call(r[2], 'tfiFF', r[0], p,
				(1.0, 2.0, 3.0))[0], SDTest.Call(r[3], 'tfiFF', r[0], (1.0, 2.0, 3.0), p)[0]))
		# the speeds
		for iccv in (0.0, 0.25, 0.55, 0.5500001, 0.75, 1.0, 2.5, -1.0):
			check('ICC %s SetSpeed %s' % (iccn, fmt(iccv)), lambda C=iccC, v=iccv: (SDTest.Call(0x073040, 'tvif', C, v),
				iccf(C + 0xf4, 1), iccf(C + 0x8c, 2), iccf(C + 0x204, 2)))
		check('ICC %s SetTurnSpeed' % iccn, lambda C=iccC: (SDTest.Call(0x073060, 'tvif', C, 3.0), iccf(C + 0x8c, 2),
			iccf(C + 0xf4, 1)))
		# the small ones
		check('ICC %s Reset' % iccn, lambda C=iccC: (iccset(iccw + 0x4ec, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
			SDTest.Call(0x073100, 'tvi', C), iccf(C + 0x68, 3), iccf(C + 0x10c, 3), iccf(iccw + 0x4ec, 6)))
		check('ICC %s Stop2' % iccn, lambda C=iccC: (iccset(iccw + 0x4ec, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
			iccset(C + 0x68, (1.0, 2.0, 3.0)), SDTest.Call(0x0730f0, 'tvi', C), iccf(C + 0x68, 3), iccf(iccw + 0x4ec, 6)))
		check('ICC %s ClearAvoid' % iccn, lambda C=iccC: (SDTest.Call(0x072470, 'tvi', C), iccf(C + 0xa4, 6)))
		check('ICC %s SetHold' % iccn, lambda C=iccC: (iccset(iccw + 0x4ec, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
			SDTest.Call(0x073160, 'tvii', C, 0), iccb(C + 0xbc), iccf(iccw + 0x4ec, 6), SDTest.Call(0x073160, 'tvii', C,
			1), iccb(C + 0xbc), iccf(iccw + 0x4ec, 6), SDTest.Call(0x073160, 'tvii', C, 0)))
		check('ICC %s flags' % iccn, lambda C=iccC: (iccpokeb(C + 0xd4, 1), iccpokeb(C + 0xe4, 1),
			SDTest.Call(0x073140, 'tbi', C)[0], SDTest.Call(0x073150, 'tbi', C)[0], iccpokeb(C + 0xd4, 0),
			iccpokeb(C + 0xe4, 0), SDTest.Call(0x073140, 'tbi', C)[0], SDTest.Call(0x073150, 'tbi', C)[0],
			SDTest.Call(0x072f10, 'tvi', C)))
		# the keep and strafe distances to the other character
		check('ICC %s KeepDist' % iccn, lambda r=iccr: (SDTest.Call(r[4], 'tfii', r[0], iccw2)[0],
			SDTest.Call(r[5], 'tfii', r[0], iccw2)[0]))
		iccshirk = iccf(0x8443d0, 1)
		iccset(0x8443d0, (0.75,))
		check('ICC %s KeepDist shirk' % iccn, lambda r=iccr: SDTest.Call(r[4], 'tfii', r[0], iccw2)[0])
		iccset(0x8443d0, iccshirk)
		for iccd in ((0.0, 0.0, 0.0), (300.0, 0.0, 100.0), (290.0, 5.0, 90.0), (-500.0, 0.0, 0.0)):
			iccq = iccadd(iccP, iccd)
			check('ICC %s InWay %s' % (iccn, fmt(iccd)), lambda r=iccr, q=iccq: SDTest.Call(r[7], 'tbifffi', r[0],
				q[0], q[1], q[2], iccw2)[0])
		# Near: the 3D control measures from the entity of its link node
		iccpoke(iccC + 0xcc, iccw2)
		for iccd in ((0.0, 0.0, 0.0), (250.0, 0.0, 100.0), (-200.0, 30.0, 0.0)):
			iccsetpos(iccw, iccadd(iccP, iccd))
			check('ICC %s Near %s' % (iccn, fmt(iccd)), lambda r=iccr: SDTest.Call(r[6], 'tbii', r[0], iccw2)[0])
		iccpoke(iccC + 0xcc, 0)
		iccsetpos(iccw, iccP)
		# the angle tests by the view of the character
		for iccvw in ((0.0, 0.0, 0.0), (0.0, 1.5, 0.0), (0.3, -2.0, 0.0), (0.0, 3.1, 0.2)):
			iccset(iccw + 0x428, iccvw)
			check('ICC %s InFront %s' % (iccn, fmt(iccvw)), lambda r=iccr: (SDTest.Call(r[8], 'tbiif', r[0], iccw2,
				0.5)[0], SDTest.Call(r[8], 'tbiif', r[0], iccw2, 1.5)[0], SDTest.Call(r[9], 'tbiif', r[0], iccw2,
				0.5)[0], SDTest.Call(r[9], 'tbiif', r[0], iccw2, 2.0)[0]))
		# the goal, the dest and the point of the path
		iccpp = icccalloc(16)
		iccpoke(iccC + 0x130, iccpp)
		for iccg in (((200.0, 0.0, 0.0), (100.0, 0.0, 10.0)), ((200.0, 0.0, 0.0), (100.0, 0.0, 80.0)),
				((0.0, 50.0, 300.0), (0.0, 0.0, 150.0)), ((-100.0, -20.0, 0.0), (100.0, 0.0, 0.0))):
			iccset(iccC + 0x68, iccadd(iccP, iccg[0]))
			iccset(iccpp, iccadd(iccP, iccg[1]))
			check('ICC %s GoalAhead %s' % (iccn, fmt(iccg)), lambda r=iccr: SDTest.Call(r[10], 'tbi', r[0])[0])
		iccpoke(iccC + 0x130, 0)
		iccfree(iccpp)
		# Facing: the ways to the other character and to the goal
		for iccg in ((500.0, 0.0, 200.0), (-500.0, 10.0, 0.0), (300.0, 0.0, 100.0), (0.0, 0.0, 0.0)):
			iccset(iccC + 0x10c, iccadd(iccP, iccg))
			for iccwd in (0, 2):
				iccpoke(iccC + 0x94, iccwd)
				check('ICC %s Facing %s %d' % (iccn, fmt(iccg), iccwd), lambda r=iccr: SDTest.Call(r[11], 'tbiiFF',
					r[0], iccw2, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
		iccpoke(iccC + 0x94, 0)
		# the turn
		for icct in (((0.0, 0.0, 0.0), (500.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0)),
				((0.2, 1.0, 0.0), (0.0, 100.0, -500.0), (0.0, 0.0, 0.0), (0.5, -0.5)),
				((0.0, -2.5, 0.0), (10.0, -300.0, 10.0), (1.0, 0.0, 1.0), (3.0, -3.0)),
				((1.2, 3.0, 0.0), (-400.0, 0.0, 400.0), (0.0, 0.5, 0.0), (0.0, 0.0))):
			iccset(iccw + 0x428, icct[0])
			iccset(iccC + 0x10c, iccadd(iccP, icct[1]))
			iccset(iccC + 0xd8, icct[2])
			iccset(iccC + 0x80, icct[3])
			for iccsp in (0.25, 1.0):
				SDTest.Call(0x073060, 'tvif', iccC, iccsp)
				check('ICC %s Turn %s %s' % (iccn, fmt(icct), fmt(iccsp)), lambda r=iccr: (SDTest.Call(r[12], 'tvi',
					r[0]), iccf(r[0] + 0x60, 2), iccf(r[0] + 0x80, 2), iccf(r[0] + 0x138, 2), iccf(iccw + 0x4ec, 2)))
		iccset(iccC + 0xd8, (0.0, 0.0, 0.0))
		iccset(iccw + 0x428, (0.0, 0.0, 0.0))
		# the walk to and from the target (the node of the fake AI)
		iccpoke(iccC + 0x1b4, iccnode)
		iccset(iccC + 0xf4, (0.75,))
		iccset(iccC + 0x5c, (0.25,))
		for iccwk in ((0, 100.0, 200.0, 0), (0, 400.0, 900.0, 0), (0, 50.0, 100.0, 0), (1, 100.0, 200.0, 0),
				(1, 400.0, 900.0, 0), (2, 100.0, 200.0, 0), (2, 400.0, 900.0, 0), (1, 400.0, 900.0, 1),
				(2, 100.0, 900.0, 0)):
			iccpoke(iccC + 0x94, iccwk[0])
			iccset(iccC + 0x1c0, (iccwk[1],))
			iccset(iccC + 0x1bc, (iccwk[2],))
			iccpokeb(iccC + 0xd4, iccwk[3])
			check('ICC %s Walk %s' % (iccn, fmt(iccwk)), lambda r=iccr: (SDTest.Call(r[13], 'tvi', r[0]),
				icci(r[0] + 0x94), iccf(iccw + 0x4f8, 1), iccf(r[0] + 0x8, 1)))
		iccpokeb(iccC + 0xd4, 0)
		iccpoke(iccC + 0x94, 0)
		# the arrival: the goal at the end of the path
		for icca in ((10.0, 0.0, 0.0), (100.0, 0.0, 0.0), (1000.0, 0.0, 0.0)):
			iccset(iccC + 0x68, iccadd(iccP, icca))
			iccset(iccC + 0x10c, iccadd(iccP, icca))
			iccset(iccC + 0x54, (50.0,))
			check('ICC %s CheckArrived %s' % (iccn, fmt(icca)), lambda r=iccr: (SDTest.Call(r[14], 'tvi', r[0]),
				iccf(r[0] + 0x68, 3) == iccf(r[0] + 0x10c, 3)))
		# the sides
		for iccs in (0, 1, 2, 3, 4, 5):
			check('ICC %s SideBlocked %d' % (iccn, iccs), lambda r=iccr, s=iccs: SDTest.Call(r[15], 'tbii', r[0], s)[0])
		# the moves without a path search
		check('ICC %s SetMove 0' % iccn, lambda C=iccC: (SDTest.Call(0x072980, 'bii', C, 0)[0], iccints(C, (0x74,
			0x78)), iccf(C + 0x68, 3)))
		check('ICC %s SetMove 5' % iccn, lambda C=iccC: (SDTest.Call(0x072980, 'biifff', C, 5, 1.0, 0.5, -2.0)[0],
			iccints(C, (0x74, 0x78)), iccf(C + 0x220, 3), iccf(C + 0x10c, 3)))
		check('ICC %s SetMove 6' % iccn, lambda C=iccC: (SDTest.Call(0x072980, 'biii', C, 6, 1)[0], iccints(C, (0x74,
			0x78)), icci(C + 0x1b4) == iccnode, iccbytes(C, (0x1d0, 0x22c)), iccf(C + 0x1bc, 2), iccf(C + 0xf4, 1),
			iccf(C + 0x8c, 2)))
		check('ICC %s SetMove 2' % iccn, lambda C=iccC: (SDTest.Call(0x072980, 'biifffiiii', C, 2, iccP[0] + 500.0,
			iccP[1], iccP[2], 0, iccD[100.0], 0, iccD[0.75])[0], iccints(C, (0x74, 0x78, 0x120)), iccf(C + 0x54, 2),
			iccf(C + 0xf4, 1), iccf(C + 0x68, 3), iccf(C + 0x10c, 3), icci(C + 0x130) != 0))
		check('ICC %s SetMove 2 again' % iccn, lambda C=iccC: (SDTest.Call(0x072980, 'biifffiiii', C, 2, iccP[0] +
			500.0, iccP[1], iccP[2], 0, iccD[50.0], 0, iccD[0.5])[0], iccints(C, (0x74, 0x78, 0x120)),
			iccf(C + 0x54, 2), iccf(C + 0xf4, 1)))
		check('ICC %s SetMove 9' % iccn, lambda C=iccC: (SDTest.Call(0x072980, 'bii', C, 9)[0], iccints(C, (0x74,
			0x78))))
		check('ICC %s GoToDirect' % iccn, lambda C=iccC: (SDTest.Call(0x072fc0, 'tbiFff', C, (iccP[0], iccP[1],
			iccP[2] + 700.0), 300.0, 1.0)[0], iccf(C + 0x54, 2), iccf(C + 0x68, 3), iccf(C + 0x10c, 3)))
		check('ICC %s SetDir' % iccn, lambda C=iccC: (SDTest.Call(0x072db0, 'tviF', C, (0.0, 0.0, 0.0)),
			iccf(C + 0x10c, 3), SDTest.Call(0x072db0, 'tviF', C, (3.0, 0.0, -4.0)), iccf(C + 0x10c, 3),
			iccf(C + 0x220, 3)))
		check('ICC %s Away' % iccn, lambda C=iccC: (SDTest.Call(0x072c90, 'tviii', C, iccnode, 0),
			icci(C + 0x1b4) == iccnode, iccb(C + 0x22c), iccf(C + 0x1bc, 2), iccf(C + 0xf4, 1)))
		# the follow update on the direct branch with a free way (the slot fires at once)
		iccpoke(iccC + 0x1b4, iccnode)
		iccpokeb(iccC + 0x1d0, 1)
		iccset(iccC + 0x1bc, (900.0, 100.0))
		iccset(iccC + 0x5c, (0.5,))
		check('ICC %s UpdateFollow direct' % iccn, lambda r=iccr: (SDTest.Call(r[16], 'tbi', r[0])[0],
			iccbytes(r[0], (0x1d0, 0x200)), iccf(r[0] + 0x68, 3), iccf(r[0] + 0x10c, 3), iccf(r[0] + 0x148, 2)))
		iccpokeb(iccC + 0x1d0, 0)
		# the strafe: the forward side only (the back side of the 2D control needs a full action class)
		iccset(iccC + 0x10c, iccadd(iccP, (600.0, 0.0, 200.0)))
		for iccs in (1, 2):
			check('ICC %s TryStrafe %d' % (iccn, iccs), lambda r=iccr, s=iccs: SDTest.Call(r[17], 'tbiiiififi', r[0],
				s, iccw2, 0, 200.0, 1, 150.0, 1)[0])
			check('ICC %s TryStrafe none %d' % (iccn, iccs), lambda r=iccr, s=iccs: SDTest.Call(r[17], 'tbiiiififi',
				r[0], s, iccw2, 0, 200.0, 0, 150.0, 0)[0])

	# the 2D control: the floor tests (no floor up there) and the strafe paths of Avoid that end before TryStrafe
	check('ICC 2D TestLine', lambda: (SDTest.Call(0x077010, 'tbiFF', iccc2, iccP, iccadd(iccP, (100.0, 0.0,
		0.0)))[0], SDTest.Call(0x077010, 'tbiFF', iccc2, iccP, iccP)[0]))
	check('ICC 2D FloorAt', lambda: SDTest.Call(0x078610, 'tbiFF', iccc2, iccadd(iccP, (5.0, 1.0, 2.0)), (0.0, 0.0,
		0.0)))
	check('ICC 2D FloorAhead', lambda: (SDTest.Call(0x0750f0, 'tbiF', iccc2, iccadd(iccP, (100.0, 0.0, 0.0)))[0],
		SDTest.Call(0x0750f0, 'tbiF', iccc2, iccP)[0]))
	check('ICC 3D TestLine', lambda: SDTest.Call(0x07d860, 'tbiFF', iccc3, iccP, iccadd(iccP, (100.0, 0.0, 0.0)))[0])
	for iccr in iccc:
		iccC = iccr[0]
		iccn = iccr[1]
		# InWay: dest at the other character
		iccset(iccC + 0x68, iccadd(iccP, (300.0, 0.0, 100.0)))
		for iccm in (1, 3):
			iccpoke(iccC + 0x74, iccm)
			check('ICC %s Avoid in way %d' % (iccn, iccm), lambda C=iccC: (SDTest.Call(0x0724b0, 'tbiip', C, iccw2,
				0x100)[0:2], iccints(C, (0x98,)), iccbytes(C, (0xd4, 0xd5)), iccf(C + 0xd8, 3)))
		iccset(iccC + 0x68, iccadd(iccP, (-900.0, 0.0, 0.0)))
		# the goal behind: not facing
		iccset(iccC + 0x10c, iccadd(iccP, (-900.0, 0.0, 0.0)))
		iccpoke(iccC + 0x94, 0)
		check('ICC %s Avoid behind' % iccn, lambda C=iccC: (SDTest.Call(0x0724b0, 'tbiip', C, iccw2, 0x100)[0:2],
			iccints(C, (0x98,)), iccf(C + 0xd8, 3)))
		# the goal ahead, walking back: blocked
		iccset(iccC + 0x10c, iccadd(iccP, (900.0, 0.0, 300.0)))
		iccpoke(iccC + 0x94, 2)
		check('ICC %s Avoid back' % iccn, lambda C=iccC: (SDTest.Call(0x0724b0, 'tbiip', C, iccw2, 0x100)[0:2],
			iccints(C, (0x98,)), iccbytes(C, (0xd4,))))
		iccpoke(iccC + 0x94, 0)
	# the 3D control: the whole strafe of Avoid
	check('ICC 3D Avoid', lambda: (SDTest.Call(0x0724b0, 'tbiip', iccc3, iccw2, 0x100)[0:2], iccints(iccc3, (0x98,)),
		iccbytes(iccc3, (0xd4,)), iccf(iccc3 + 0xd8, 3)))
	# the random step of the 3D control along a direct path
	SDTest.Call(0x072fc0, 'tbiFff', iccc3, iccadd(iccP, (0.0, 0.0, 900.0)), 100.0, 1.0)
	SDTest.Call(0x323c50, 'vi', 4242)
	check('ICC 3D StepGoal', lambda: SDTest.Call(0x07ec30, 'tviF', iccc3, iccadd(iccP, (0.0, 0.0, 400.0)))[1])
	check('ICC 3D StepGoal dest', lambda: SDTest.Call(0x07ec30, 'tviF', iccc3, iccadd(iccP, (0.0, 0.0, 900.0)))[1])

	# the strafe with both sides (the back side of the 2D control reads the floor radius of the fake class)
	for iccr in iccc:
		iccset(iccr[0] + 0x10c, iccadd(iccP, (600.0, 0.0, 200.0)))
		iccpoke(iccr[0] + 0x94, 0)
		for iccs in (1, 2):
			check('ICC %s TryStrafe both %d' % (iccr[1], iccs), lambda r=iccr, s=iccs: SDTest.Call(r[17],
				'tbiiiififi', r[0], s, iccw2, 1, 200.0, 1, 150.0, 1)[0])

	# the end: the controls deleted, the fakes freed, the characters deleted
	check('ICC delete', lambda: (SDTest.Call(0x0731c0, 'tiii', iccc2, 1)[0] == iccc2,
		SDTest.Call(0x079d50, 'tiii', iccc3, 1)[0] == iccc3))
	iccpoke(iccw + 0x234, 0)
	iccfree(iccac)
	iccfree(icccls)
	iccfree(iccA)
	iccfree(iccAV)
	iccfree(iccarg)
	for iccE in (iccw, iccw2):
		check('ICC delete entity', lambda e=iccE: (SDTest.Call(0x208810, 'tbis', iccworld + 4, SDTest.Peek(e + 4,
			'@p')), SDTest.Call(icci(icci(e) + 0xc) - 0x400000, 'tiii', e, 1)[0] == e)[1])
