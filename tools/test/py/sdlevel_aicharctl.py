# game AI/ICharControl.c, the base part of AI/AICharControl.cpp (execfile'd by sdlevel.py, Python 1.5.2): construction,
# the frame and the slots of cAICharControl. The setup of sdlevel_icharctl.py with its own names: two WalkChars far
# above the map ("SDAccWalk" with a fake action controller whose class only has the radii, "SDAccWalk2" as the target),
# a fake AI in calloc memory (its vtable: the exe's small getters, Event the setter at 0x0ee380 that keeps the event at
# 0x1a4 of the AI) and a 2D, a 3D and a base control made by their constructors. Up there the 3D corridor tests are
# free and the 2D ones fail (no floor). The path searches are never called: the parts that reach them run on copies
# of the 2D and 3D vtables with FindPath, FindRoute and StepGoal replaced by stubs of the exe. Not called: the debug
# view with its flags on (it draws).
def acci(a):
	return SDTest.Peek(a, '@i')
def accb(a):
	return SDTest.Peek(a, '@b')
def accf(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def accset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def accpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def accpokeb(a, v):
	w = acci(a & ~3)
	sh = 8 * (a & 3)
	accpoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def acccalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def accfree(p):
	SDTest.Call(0x133550, 'ii', p)
def accints(a, offs):
	r = ()
	for o in offs:
		r = r + (acci(a + o),)
	return r
def accbytes(a, offs):
	r = ()
	for o in offs:
		r = r + (accb(a + o),)
	return r
def accpos(e):
	return SDTest.Call(acci(acci(e) + 0x18) - 0x400000, 'tiiF', e, (0.0, 0.0, 0.0))[1]
def accsetpos(e, p):
	SDTest.Call(acci(acci(e) + 0x1c) - 0x400000, 'tviF', e, p)
def accadd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])

# ---- the pure parts ----
for accv in (((1.0, 2.0, 3.0), (1.0, 2.0, 3.0)), ((0.0, 0.0, 0.0), (-0.0, 0.0, -0.0)), ((1.0, 2.0, 3.0), (1.0, 2.0,
		4.0)), ((1.0, 2.0, 3.0), (1.0, 5.0, 3.0)), ((9.0, 2.0, 3.0), (1.0, 2.0, 3.0))):
	check('ACC Vec3_Equal %s' % fmt(accv), lambda v=accv: SDTest.Call(0x06d6b0, 'bFF', v[0], v[1])[0])

# the point list (0x14 byte nodes): add, clear (two nodes to the free list), add from the free list, free
accL = acccalloc(0x18)
accpoke(accL + 0x14, 2)
for accv in ((1.0, 2.0, 3.0), (4.0, 5.0, 6.0), (7.0, 8.0, 9.0)):
	SDTest.Call(0x06d720, 'tviF', accL, accv)
def acclist(l):
	h = acci(l)
	if not h:
		return (0, acci(l + 8), acci(l + 0x10), acci(l + 0xc) != 0)
	return (accf(h + 8, 3), accf(acci(h) + 8, 3), accf(acci(h + 4) + 8, 3), acci(l + 4), acci(l + 8),
		acci(l + 0x10), acci(l + 0xc) != 0)
check('ACC DListVec3_Add', lambda: acclist(accL))
SDTest.Call(0x06d8a0, 'tvi', accL)
check('ACC DListVec3_Clear', lambda: acclist(accL))
SDTest.Call(0x06d720, 'tviF', accL, (-1.0, -2.0, -3.0))
check('ACC DListVec3_Add free', lambda: acclist(accL))
SDTest.Call(0x06d4e0, 'tvi', accL)
check('ACC NWVecList_Free', lambda: acclist(accL))
accfree(accL)
# the segment list (0x20 byte nodes)
accL = acccalloc(0x18)
accpoke(accL + 0x14, 1)
for accv in ((1.0, 2.0, 3.0, 4.0, 5.0, 6.0), (7.0, 8.0, 9.0, 10.0, 11.0, 12.0)):
	SDTest.Call(0x0a27b0, 'tviF', accL, accv)
def accsegs(l):
	h = acci(l)
	if not h:
		return (0, acci(l + 8), acci(l + 0x10), acci(l + 0xc) != 0)
	return (accf(h + 8, 6), accf(acci(h) + 8, 6), acci(l + 8), acci(l + 0x10), acci(l + 0xc) != 0)
check('ACC segs', lambda: accsegs(accL))
SDTest.Call(0x06d810, 'tvi', accL)
check('ACC DListSeg_Clear', lambda: accsegs(accL))
SDTest.Call(0x0a27b0, 'tviF', accL, (1.5, 2.5, 3.5, 4.5, 5.5, 6.5))
SDTest.Call(0x06d4a0, 'tvi', accL)
check('ACC DListSeg_Free', lambda: accsegs(accL))
accfree(accL)
# the node destructor: a link out of the ring of its head, then a head that unlinks its link
accH = acccalloc(0x20)
accN = accH + 0x10
def acclink(t):
	accpoke(accH, accN)
	accpoke(accH + 4, accN)
	accpoke(accH + 8, t)
	accpoke(accH + 0xc, 0)
	accpoke(accN, accH)
	accpoke(accN + 4, accH)
	accpoke(accN + 8, t)
	accpoke(accN + 0xc, 1)
def accnodes():
	return (acci(accH) == accH, acci(accH + 4) == accH, acci(accH + 8) == 0x1234, acci(accN) == accN,
		acci(accN + 4) == accN, acci(accN + 8))
acclink(0x1234)
SDTest.Call(0x06d430, 'tvi', accN)
check('ACC Nto1Node_dtor link', accnodes)
acclink(0x1234)
SDTest.Call(0x06d430, 'tvi', accH)
check('ACC Nto1Node_dtor head', accnodes)
accfree(accH)

# ---- the characters, the fake AI and the controls ----
accworld = SDTest.Peek(0x467450, 'i')
accP = (-35000.0, 62000.0, 41000.0)
accP2 = (accP[0] + 300.0, accP[1], accP[2] + 100.0)
SDTest.Call(0x323c50, 'vi', 913)
accw = SDTest.Call(0x208360, 'tiisfffs', accworld, 'SDAccWalk', accP[0], accP[1], accP[2], 'WalkChar')[0]
accw2 = SDTest.Call(0x208360, 'tiisfffs', accworld, 'SDAccWalk2', accP2[0], accP2[1], accP2[2], 'WalkChar')[0]
check('ACC WalkChars', lambda: (accw != 0, accw2 != 0, accw and acci(accw + 0x234), accw2 and acci(accw2 + 0x78)))
if accw and accw2:
	accsetpos(accw, accP)
	accsetpos(accw2, accP2)
	accac = acccalloc(0x80)
	acccls = acccalloc(0x200)
	accpoke(accac + 8, acccls)
	accset(acccls + 0x164, (40.0,))
	accset(acccls + 0x16c, (25.0,))
	accpoke(accw + 0x234, accac)
	accset(accw + 0x45c, (30.0,))
	# the fake AI: Changed and GetChanged its byte 0x28, GetTarget the node at 0x38 (target SDAccWalk2), CanJump its
	# byte 0x48, Event the setter of 0x1a4, the states at 0x1a0 and 0x1a4, CheckState "mov al,1"
	accAV = acccalloc(0x100)
	accA = acccalloc(0x200)
	accpoke(accAV + 0x10, 0x465f70)
	accpoke(accAV + 0x38, 0x485730)
	accpoke(accAV + 0x3c, 0x485710)
	accpoke(accAV + 0x58, 0x4ee380)
	accpoke(accAV + 0x7c, 0x469900)
	accpoke(accAV + 0x80, 0x469910)
	accpoke(accAV + 0xb8, 0x4d81b0)
	accpoke(accAV + 0xc0, 0x4698e0)
	accpoke(accA, accAV)
	accpoke(accA + 0x38, accA + 0x38)
	accpoke(accA + 0x3c, accA + 0x38)
	accpoke(accA + 0x40, accw2)
	accpoke(accA + 0x1a0, 7)
	accnode = accA + 0x38
	# a node with no target
	accnode0 = acccalloc(0x10)
	accarg = acccalloc(8)
	accpoke(accarg, 1)
	accm2 = SDTest.Call(0x1334d0, 'ii', 0x360)[0]
	accc2 = SDTest.Call(0x073180, 'tiiiii', accm2, accw, accA, accarg)[0]
	accm3 = SDTest.Call(0x1334d0, 'ii', 0x360)[0]
	accc3 = SDTest.Call(0x079d10, 'tiiiii', accm3, accw, accA, accarg)[0]
	accmb = SDTest.Call(0x1334d0, 'ii', 0x360)[0]
	acccb = SDTest.Call(0x06cba0, 'tiiiii', accmb, accw, accA, accarg)[0]
	def accslots(c):
		r = ()
		for o in (0x4, 0x14, 0x24, 0x34, 0xfc, 0x144, 0x154, 0x164, 0x174, 0x184, 0x194, 0x1a4):
			r = r + ((acci(c + o) - 0x400000, accf(c + o + 4, 3)),)
		return r
	def accbase(c):
		return (acci(c) - 0x400000, acci(c + 0x44) == accA, acci(c + 0x48) == accw, acci(c + 0x4c) == accw + 0x4ec,
			accf(c + 0x50, 4), accf(c + 0x60, 2), accf(c + 0x68, 3), accints(c, (0x74, 0x78, 0x7c, 0x94, 0x98, 0x9c,
			0x130, 0x134, 0x1b8, 0x1fc, 0x20c, 0x23c, 0xf8)), accf(c + 0x80, 2), accf(c + 0x8c, 2), accf(c + 0xa4, 6),
			accf(c + 0xc0, 1), (acci(c + 0xc4) == c + 0xc4, acci(c + 0xc8) == c + 0xc4, acci(c + 0xcc),
			acci(c + 0xd0) == accA), accf(c + 0xd8, 3), accf(c + 0xe8, 4), accf(c + 0x10c, 3),
			accints(c, (0x118, 0x11c, 0x120, 0x124, 0x128, 0x12c)), accf(c + 0x138, 2), accf(c + 0x1bc, 2),
			accf(c + 0x1c4, 3), accf(c + 0x1d4, 1), accf(c + 0x1d8, 6), accf(c + 0x1f0, 3), accf(c + 0x204, 2),
			accf(c + 0x214, 3), accf(c + 0x220, 3), accf(c + 0x230, 3), accf(c + 0x240, 54),
			accints(c, (0x318, 0x31c, 0x320, 0x324, 0x328, 0x32c, 0x330, 0x334, 0x338, 0x33c, 0x340, 0x344, 0x348,
			0x34c, 0x350, 0x354, 0x358, 0x35c)), accbytes(c, (0x88, 0xa0, 0xa1, 0xbc, 0xbd, 0xd4, 0xd5, 0x140, 0x141,
			0x1d0, 0x1d1, 0x200, 0x201, 0x210, 0x22d, 0x22e, 0x22f)), accslots(c))
	check('ACC ctor 2D', lambda: (accc2 == accm2, accbase(accc2)))
	check('ACC ctor 3D', lambda: (accc3 == accm3, accbase(accc3)))
	check('ACC ctor base', lambda: (acccb == accmb, accbase(acccb)))

	# the copies of the vtables with the path searches stubbed: FindPath "mov al,1; ret 4", FindRoute "mov al,1; ret",
	# StepGoal "ret 4"
	def accstubvt(src, n):
		v = acccalloc(4 * n)
		i = 0
		while i < n:
			accpoke(v + 4 * i, SDTest.Peek(src + 4 * i, 'i'))
			i = i + 1
		accpoke(v + 0xe8, 0x485a60)
		accpoke(v + 0x148, 0x4d81b0)
		accpoke(v + 0xf0, 0x4bd410)
		return v
	accvt2 = accstubvt(0x3a5acc, 103)
	accvt3 = accstubvt(0x3a5d18, 101)
	accc = ((accc2, '2D', acci(accc2), accvt2), (accc3, '3D', acci(accc3), accvt3))
	def accctl():
		return accf(accw + 0x4ec, 6)
	def accev():
		return acci(accA + 0x1a4)
	for accr in accc:
		accC = accr[0]
		accn = accr[1]
		# the accessors
		accpoke(accC + 0x94, 2)
		accpoke(accC + 0x98, 1)
		accpoke(accC + 0x20c, 2)
		accpoke(accC + 0xcc, 0x5678)
		accset(accC + 0x10c, (1.0, 2.0, 3.0))
		accset(accC + 0x68, (4.0, 5.0, 6.0))
		accset(accC + 0xa4, (7.0, 8.0, 9.0, 10.0, 11.0, 12.0))
		accpokeb(accC + 0x1d0, 1)
		check('ACC %s accessors' % accn, lambda C=accC: (SDTest.Call(0x070ba0, 'tii', C)[0], SDTest.Call(0x071840,
			'tii', C)[0], SDTest.Call(0x071850, 'tii', C)[0], SDTest.Call(0x070bb0, 'tii', C)[0],
			SDTest.Call(0x071f60, 'tbi', C)[0], SDTest.Call(0x071f70, 'tbi', C)[0], SDTest.Call(0x070b10, 'tbi', C)[0],
			SDTest.Call(0x071530, 'tviF', C, (0.0, 0.0, 0.0))[1], SDTest.Call(0x071550, 'tviF', C, (0.0, 0.0, 0.0))[1],
			SDTest.Call(0x071820, 'tviF', C, (0.0, 0.0, 0.0))[1], SDTest.Call(0x071800, 'tviF', C, (0.0, 0.0, 0.0))[1]))
		accpoke(accC + 0xcc, 0)
		accpokeb(accC + 0x1d0, 0)
		accpoke(accC + 0x94, 0)
		accpoke(accC + 0x98, 0)
		accpoke(accC + 0x20c, 0)
		# Stop, the restart of the slots
		check('ACC %s Stop' % accn, lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0)),
			SDTest.Call(0x070ae0, 'tvi', C), accctl()))
		for acco in (0x4, 0x14, 0x24, 0x34, 0xfc, 0x144, 0x154, 0x164, 0x174, 0x184, 0x194, 0x1a4):
			accset(accC + acco + 4, (0.05 + acco / 1000.0,))
		check('ACC %s RestartSlots' % accn, lambda C=accC: (SDTest.Call(0x070b20, 'tvi', C), accslots(C)))
		# the strafe and the walk values
		accset(accC + 0xf4, (0.75,))
		for accs in (0, 1, 2, 3):
			accpoke(accC + 0x9c, accs)
			check('ACC %s StrafeWalk %d' % (accn, accs), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x06f970, 'tvi', C), accctl()))
		accpoke(accC + 0x9c, 0)
		for accbs in ((0, 0), (1, 0), (0, 1)):
			accpokeb(accC + 0xd4, accbs[0])
			accpokeb(accC + 0x200, accbs[1])
			check('ACC %s WalkFwd %s' % (accn, fmt(accbs)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x070250, 'tvi', C), acci(C + 0x94), accctl(), accpoke(C + 0x94, 0),
				SDTest.Call(0x06fec0, 'tvi', C), acci(C + 0x94), accctl()))
		accpokeb(accC + 0xd4, 0)
		accpokeb(accC + 0x200, 0)
		accpoke(accC + 0x94, 0)
		# the side directions to the goal
		for accg in ((500.0, 0.0, 0.0), (0.0, 30.0, -400.0), (0.0, 100.0, 0.0), (-3.0, 4.0, 12.0)):
			accset(accC + 0x10c, accadd(accP, accg))
			for accs in (0, 1, 2, 3):
				check('ACC %s SideDir %s %d' % (accn, fmt(accg), accs), lambda C=accC, s=accs: SDTest.Call(0x071860,
					'tviFi', C, (9.0, 9.0, 9.0), s)[1])
		# the check of the target of the follow and away moves
		accact = accb(accw2 + 0x4c)
		check('ACC %s target active' % accn, lambda: accact)
		for acct in ((0, accnode, 1), (3, accnode, 1), (6, accnode, 1), (3, accnode, 0), (4, accnode0, 1),
				(5, accnode0, 1), (2, accnode0, 1)):
			accpoke(accC + 0x74, acct[0])
			accpoke(accC + 0x1b4, acct[1])
			accpokeb(accw2 + 0x4c, acct[2])
			accpoke(accA + 0x1a4, 0)
			check('ACC %s CheckTarget %s' % (accn, fmt(acct[0:1] + acct[2:3])), lambda C=accC: (SDTest.Call(0x06d7a0,
				'tbi', C)[0], accev()))
			accpokeb(accw2 + 0x4c, accact)
		accpoke(accC + 0x74, 0)
		accpoke(accC + 0x1b4, accnode)
		# the line path, its end and its start
		check('ACC %s DirectPath' % accn, lambda C=accC: (SDTest.Call(0x070290, 'tbiF', C, accadd(accP, (700.0,
			0.0, 300.0)))[0:2], accf(C + 0x68, 3), accf(C + 0x10c, 3), acci(C + 0x120), acci(C + 0x130) ==
			acci(acci(C + 0x118) + 4) + 8, accf(acci(C + 0x118) + 8, 3), accf(acci(acci(C + 0x118) + 4) + 8, 3),
			acci(C + 0x11c) == acci(acci(C + 0x118) + 4)))
		check('ACC %s AtPathEnd' % accn, lambda C=accC: (SDTest.Call(0x071f10, 'tbi', C)[0], accpoke(C + 0x130, 0),
			SDTest.Call(0x071f10, 'tbi', C)[0], accpoke(C + 0x130, acci(C + 0x118) + 8), SDTest.Call(0x071f10, 'tbi',
			C)[0], accpoke(C + 0x130, acci(acci(C + 0x118) + 4) + 8)))
		check('ACC %s GoalAtStart' % accn, lambda C=accC: (SDTest.Call(0x071ea0, 'tbi', C)[0],
			accset(C + 0x10c, accP), SDTest.Call(0x071ea0, 'tbi', C)[0], accset(C + 0x10c, accadd(accP, (0.0, 0.0,
			1.0))), SDTest.Call(0x071ea0, 'tbi', C)[0]))
		# the goal on a line
		for accab in (((100.0, 0.0, 0.0), (900.0, 0.0, 0.0)), ((0.0, 0.0, 0.0), (10.0, 20.0, 30.0)),
				((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((-50.0, 10.0, 0.0), (400.0, -90.0, 700.0))):
			accset(accC + 0x10c, (1.0, 2.0, 3.0))
			check('ACC %s GoalOnLine %s' % (accn, fmt(accab)), lambda C=accC, ab=accab: (SDTest.Call(0x071570,
				'tbiFF', C, accadd(accP, ab[0]), accadd(accP, ab[1]))[0], accf(C + 0x10c, 3)))
		accnps = acci(0x844448)
		accpoke(0x844448, 5)
		check('ACC %s GoalOnLine 5' % accn, lambda C=accC: (SDTest.Call(0x071570, 'tbiFF', C, accadd(accP, (0.0,
			0.0, 0.0)), accadd(accP, (600.0, 0.0, 0.0)))[0], accf(C + 0x10c, 3)))
		accpoke(0x844448, accnps)
		# the turns of the view: to the target (or along avoidDir) and to the goal
		for icct in (((0.0, 0.0, 0.0), (500.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0)),
				((0.2, 1.0, 0.0), (0.0, 100.0, -500.0), (0.0, 0.0, 0.0), (0.5, -0.5)),
				((0.0, -2.5, 0.0), (10.0, -300.0, 10.0), (1.0, 0.0, 1.0), (3.0, -3.0)),
				((1.2, 3.0, 0.0), (-400.0, 0.0, 400.0), (0.0, 0.5, 0.0), (0.0, 0.0))):
			accset(accw + 0x428, icct[0])
			accset(accC + 0x10c, accadd(accP, icct[1]))
			accset(accC + 0xd8, icct[2])
			for accsp in (0.25, 1.0):
				SDTest.Call(0x073060, 'tvif', accC, accsp)
				accset(accC + 0x80, icct[3])
				check('ACC %s TurnFollow %s %s' % (accn, fmt(icct), fmt(accsp)), lambda C=accC: (SDTest.Call(0x06e710,
					'tvi', C), accf(C + 0x60, 2), accf(C + 0x80, 2), accf(C + 0x138, 2), accf(accw + 0x4ec, 2)))
				accset(accC + 0x80, icct[3])
				check('ACC %s DirTurn %s %s' % (accn, fmt(icct), fmt(accsp)), lambda C=accC: (SDTest.Call(0x06ea60,
					'tvi', C), accf(C + 0x60, 2), accf(C + 0x80, 2), accf(C + 0x138, 2), accf(accw + 0x4ec, 2)))
		accset(accC + 0xd8, (0.0, 0.0, 0.0))
		# the checks of the view against the way to the target and against dir (event 6)
		for accvw in ((0.0, 0.0, 0.0), (0.0, -1.8925469, 0.0), (0.0, 1.2490458, 0.0), (0.3, -1.9, 0.0)):
			accset(accw + 0x428, accvw)
			accset(accC + 0x220, (-3.0, 0.0, -1.0))
			accpoke(accA + 0x1a4, 0)
			check('ACC %s AwayCheck %s' % (accn, fmt(accvw)), lambda C=accC: (SDTest.Call(0x06ddb0, 'tvi', C), accev(),
				accpoke(accA + 0x1a4, 0), SDTest.Call(0x06de80, 'tvi', C), accev()))
		accset(accw + 0x428, (0.0, 0.0, 0.0))
		# the characters near: none, then the target next to the character
		check('ACC %s CharsNear' % accn, lambda C=accC: (SDTest.Call(0x06dd40, 'tbi', C)[0], acci(0x8443a0)))
		accsetpos(accw2, accadd(accP, (30.0, 0.0, 10.0)))
		check('ACC %s CharsNear near' % accn, lambda C=accC: (SDTest.Call(0x06dd40, 'tbi', C)[0], acci(0x8443a0),
			acci(0x844350) == accw2, acci(0x8443a4) == accw))
		check('ACC %s FindAvoid near' % accn, lambda C=accC: (SDTest.Call(0x06f9e0, 'tbi', C)[0], accbytes(C,
			(0xd4, 0xd5, 0x201))))
		accsetpos(accw2, accP2)
		accpokeb(accC + 0xd5, 1)
		check('ACC %s FindAvoid' % accn, lambda C=accC: (SDTest.Call(0x06f9e0, 'tbi', C)[0], accbytes(C, (0xd4, 0xd5,
			0x201))))
		# the strafe: the collision timers, the avoid update without a linked character, the jump
		accset(accC + 0x5c, (0.25,))
		accset(accC + 0xf4, (0.75,))
		accpokeb(accC + 0xe4, 0)	# not set by the constructor, written only when FindAvoid runs
		for accst in ((0, 0, 0.0, 0, 0), (1, 0, 0.0, 0, 0), (1, 1, 2.0, 0, 0), (1, 0, 2.0, 1, 1), (2, 0, 2.0, 2, 0)):
			accpoke(accC + 0x94, accst[0])
			accpokeb(accC + 0xd4, accst[1])
			accset(accC + 0xe8, (accst[2], accst[2], accst[2]))
			accpoke(accC + 0x98, accst[3])
			accpokeb(accA + 0x48, accst[4])
			accset(accC + 0x24 + 4, (0.0,))
			accpoke(accA + 0x1a4, 0)
			check('ACC %s Strafe %s' % (accn, fmt(accst)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x06fc90, 'tvi', C), accctl(), accf(C + 0xc0, 1), accf(C + 0xe8, 3),
				accints(C, (0x98,)), accbytes(C, (0xd4, 0xd5, 0xe4)), accev()))
			accpokeb(accA + 0x48, 0)
		accpoke(accC + 0x94, 0)
		accpoke(accC + 0x98, 0)
		accpokeb(accC + 0xd4, 0)
		# the walk away from the target
		accpoke(accC + 0x1b4, accnode)
		for accwk in ((0, 0, 100.0, 200.0), (1, 0, 400.0, 900.0), (1, 2, 100.0, 200.0), (1, 2, 400.0, 900.0),
				(1, 2, 100.0, 1000.0), (1, 1, 400.0, 900.0)):
			accpokeb(accC + 0x22c, accwk[0])
			accpoke(accC + 0x94, accwk[1])
			accset(accC + 0x1c0, (accwk[2],))
			accset(accC + 0x1bc, (accwk[3],))
			accset(accC + 0x4 + 4, (0.0,))
			accset(accC + 0x14 + 4, (0.0,))
			check('ACC %s AwayWalk %s' % (accn, fmt(accwk)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x06fed0, 'tvi', C), acci(C + 0x94), accb(C + 0x22d), accctl()))
		accpoke(accC + 0x94, 0)
		accpokeb(accC + 0x22c, 0)
		check('ACC %s Draw' % accn, lambda C=accC: (SDTest.Call(0x070330, 'tvi', C), accctl()))

		# ---- on the vtable copy without the path searches ----
		accpoke(accC, accr[3])
		accset(accC + 0x5c, (0.25,))
		accset(accC + 0x50, (40.0,))
		SDTest.Call(0x070290, 'tbiF', accC, accadd(accP, (700.0, 0.0, 300.0)))
		accpoke(accC + 0x1fc, 1)
		check('ACC %s CheckPathEnd' % accn, lambda C=accC: (SDTest.Call(0x06d930, 'tbi', C)[0], acci(C + 0x1fc),
			accb(C + 0x200), accf(C + 0x68, 3), accf(C + 0x10c, 3)))
		accpoke(accC + 0x1fc, 0)
		check('ACC %s CheckPathEnd 0' % accn, lambda C=accC: SDTest.Call(0x06d930, 'tbi', C)[0])
		# the walk along a path of four points
		def accpath(C, pts):
			SDTest.Call(0x06d8a0, 'tvi', C + 0x118)
			for p in pts:
				SDTest.Call(0x06d720, 'tviF', C + 0x118, accadd(accP, p))
			h = acci(C + 0x118)
			accpoke(C + 0x11c, acci(h + 4))
			accpoke(C + 0x130, acci(h + 4) + 8)
			accset(C + 0x68, accf(h + 8, 3))
		accpts = ((0.0, 0.0, 0.0), (300.0, 0.0, 50.0), (500.0, 20.0, 400.0), (900.0, 0.0, 900.0))
		accpts = (accpts[3], accpts[2], accpts[1], accpts[0])
		def accpathstate(C):
			h = acci(C + 0x118)
			it = acci(C + 0x11c)
			pp = acci(C + 0x130)
			prev = acci(C + 0x134)
			r = ()
			n = h
			i = 0
			while i < 4:
				r = r + ((it == n, pp == n + 8, prev == n + 8),)
				n = acci(n + 4)
				i = i + 1
			return (accf(C + 0x10c, 3), r, accbytes(C, (0xbd, 0x141, 0x200, 0x22e)))
		for accg in ((100.0, 0.0, 10.0), (-300.0, 50.0, 0.0)):
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, accg))
			check('ACC %s UpdatePath %s' % (accn, fmt(accg)), lambda C=accC: (SDTest.Call(0x06d9c0, 'tbi', C)[0],
				accpathstate(C)))
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, accg))
			accset(accw + 0x428, (0.0, 0.7, 0.0))
			check('ACC %s UpdateRoutePath %s' % (accn, fmt(accg)), lambda C=accC: (SDTest.Call(0x06db50, 'tbi', C)[0],
				accpathstate(C), accf(C + 0x230, 3)))
			accset(accw + 0x428, (0.0, 0.0, 0.0))
		# the next goal along the path: near the goal, lost goal, far
		for accng in (((10.0, 0.0, 5.0), 0), ((100.0, 50.0, 300.0), 1), ((5000.0, 0.0, 0.0), 0), ((-200.0, 0.0,
				600.0), 1)):
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, accng[0]))
			accpokeb(accC + 0x141, accng[1])
			check('ACC %s NextGoal %s' % (accn, fmt(accng)), lambda C=accC: (SDTest.Call(0x070bc0, 'tvi', C),
				accpathstate(C)))
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, accng[0]))
			check('ACC %s NextGoalPath %s' % (accn, fmt(accng)), lambda C=accC: (SDTest.Call(0x0710d0, 'tvi', C),
				accpathstate(C)))
			accpokeb(accC + 0x141, 0)
		# the checks of the path of GO_TO, the follow and ROUTE (the slot fires at once)
		for accst in (0, 1):
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, (100.0, 0.0, 10.0)))
			accset(accC + 0xfc + 4, (0.0,))
			accset(accC + 0x194 + 4, (0.0,))
			accpokeb(accC + 0x200, accst)
			accpoke(accC + 0x1fc, accst)
			check('ACC %s CheckGoTo %d' % (accn, accst), lambda C=accC: (SDTest.Call(0x06ec30, 'tbi', C)[0],
				accpathstate(C), accf(C + 0xa4, 6) == accpos(accw) + accf(C + 0x10c, 3)))
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, (100.0, 0.0, 10.0)))
			accset(accC + 0xfc + 4, (0.0,))
			accset(accC + 0x194 + 4, (0.0,))
			accpokeb(accC + 0x200, accst)
			check('ACC %s CheckFollow %d' % (accn, accst), lambda C=accC: (SDTest.Call(0x06ee80, 'tbi', C)[0],
				accpathstate(C)))
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, (-100.0, 0.0, 10.0)))
			accset(accC + 0xfc + 4, (0.0,))
			check('ACC %s CheckRoute %d' % (accn, accst), lambda C=accC: (SDTest.Call(0x06e950, 'tbi', C)[0],
				accpathstate(C)))
		accpokeb(accC + 0x200, 0)
		accpoke(accC + 0x1fc, 0)
		# CanMove: the next segment of the path near the character
		for accg in ((100.0, 0.0, 10.0), (2000.0, 0.0, 0.0)):
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, accg))
			accset(accC + 0x34 + 4, (0.0,))
			check('ACC %s CanMove %s' % (accn, fmt(accg)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x06df10, 'tbi', C)[0], accb(C + 0xa1), accctl()))
		# the wait: the strafe while the target leaves the line, its time
		accpoke(accC + 0x1b4, accnode)
		for accwt in ((1, 0, 0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), (0, 1, 0.0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
				(0, 0, 0.0, (100.0, 0.0, 200.0), accP2), (0, 1, 0.0, (100.0, 0.0, 200.0), accP2),
				(0, 1, 0.0, (100.0, 0.0, 200.0), (accP2[0] - 50.0, accP2[1], accP2[2] + 500.0)),
				(0, 1, 0.0, (-100.0, 0.0, 200.0), (accP2[0] + 50.0, accP2[1], accP2[2] - 500.0)),
				(0, 1, 0.5, accP, accP2), (0, 1, 2.0, accP, accP2)):
			accpokeb(accC + 0x1d0, accwt[0])
			accpokeb(accC + 0x1d1, accwt[1])
			accset(accC + 0x1d4, (accwt[2],))
			accset(accC + 0x1d8, accadd(accP, accwt[3]))
			accset(accC + 0x1e4, accwt[4])
			accset(accC + 0x1a4 + 4, (0.0,))
			check('ACC %s CheckWait %s' % (accn, fmt(accwt)), lambda C=accC: (SDTest.Call(0x06e090, 'tbi', C)[0],
				accbytes(C, (0x1d1,)), accints(C, (0x9c,)), accf(C + 0x1d4, 7)))
			accpokeb(accC + 0x1d0, accwt[0])
			accpokeb(accC + 0x1d1, accwt[1])
			accset(accC + 0x1d4, (accwt[2],))
			accset(accC + 0x1d8, accadd(accP, accwt[3]))
			accset(accC + 0x1e4, accwt[4])
			accset(accC + 0x1a4 + 4, (0.0,))
			check('ACC %s Wait %s' % (accn, fmt(accwt)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0, 5.0,
				6.0)), SDTest.Call(0x0722e0, 'tbif', C, 0.25)[0], accf(C + 0x1d4, 1), accctl(), accints(C, (0x9c,))))
		accpokeb(accC + 0x1d0, 0)
		accpokeb(accC + 0x1d1, 0)
		accpoke(accC + 0x9c, 0)
		# the orbit around the target
		accset(accC + 0x1bc, (900.0, 100.0))
		for accor in ((0, 1, 0, 1, (0.0, 0.0, 0.0)), (0, 1, 0, 1, (0.0, 3.0, 0.0)), (0, 1, 0, 1, (0.0, 1.8925469, 0.0)),
				(0, 1, 2, 0, (0.0, 1.8925469, 0.0)), (1, 1, 0, 0, (0.0, 1.8925469, 0.0)), (2, 0, 0, 0, (0.0, 0.0, 0.0)),
				(1, 1, 1, 0, (0.0, -1.2, 0.0))):
			accpoke(accC + 0x20c, accor[0])
			accpokeb(accC + 0x1d0, accor[1])
			accpoke(accC + 0x94, accor[2])
			accpokeb(accC + 0x210, accor[3])
			accset(accw2 + 0x428, accor[4])
			accset(accC + 0x214, (0.0, 0.0, 1.0))
			accset(accC + 0x1f0, accP2)
			accset(accC + 0x184 + 4, (0.0,))
			accset(accC + 0x24 + 4, (1.0,))
			check('ACC %s Orbit %s' % (accn, fmt(accor)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x06f520, 'tvi', C), accints(C, (0x20c,)), accbytes(C, (0x210,)),
				accf(C + 0x214, 3), accf(C + 0x8c, 2), accctl()))
		accset(accw2 + 0x428, (0.0, 0.0, 0.0))
		for accso in ((0.0, 0.0, 0.0), (0.0, 1.8925469, 0.0), (0.0, -2.0, 0.0), (0.0, 3.0, 0.0)):
			accset(accw2 + 0x428, accso)
			for accd in (150.0, 400.0):
				check('ACC %s StartOrbit %s %s' % (accn, fmt(accso), fmt(accd)), lambda C=accC, d=accd:
					(SDTest.Call(0x071a10, 'tbif', C, d)[0], acci(C + 0x20c)))
		accset(accw2 + 0x428, (0.0, 0.0, 0.0))
		accpoke(accC + 0x20c, 0)
		accpokeb(accC + 0x1d0, 0)
		accpoke(accC + 0x94, 0)
		# the avoid update without a linked character and with the target linked
		check('ACC %s UpdateAvoid' % accn, lambda C=accC: (SDTest.Call(0x06f0e0, 'tvi', C), accints(C, (0x98,)),
			accbytes(C, (0xd4, 0xd5)), accf(C + 0xd8, 3), acci(C + 0xcc)))
		# the link node stays in its own ring (the head of SDAccWalk2 is not touched), the target near
		accsetpos(accw2, accadd(accP, (50.0, 0.0, 20.0)))
		for accua in ((0, 1, 0, 2, (-900.0, 0.0, 0.0), (900.0, 0.0, 300.0)), (2, 1, 0, 2, (-900.0, 0.0, 0.0), (900.0,
				0.0, 300.0)), (0, 0, 1, 2, (-900.0, 0.0, 0.0), (900.0, 0.0, 300.0)), (0, 1, 0, 2, (50.0, 0.0, 20.0),
				(900.0, 0.0, 300.0)), (0, 1, 0, 3, (50.0, 0.0, 20.0), (900.0, 0.0, 300.0)), (0, 1, 0, 2, (-900.0, 0.0,
				0.0), (-900.0, 0.0, 0.0))):
			accpoke(accC + 0x94, accua[0])
			accpoke(accC + 0x98, accua[1])
			accpokeb(accC + 0xd4, accua[2])
			accpoke(accC + 0x74, accua[3])
			accset(accC + 0x68, accadd(accP, accua[4]))
			accset(accC + 0x10c, accadd(accP, accua[5]))
			accpoke(accC + 0xcc, accw2)
			check('ACC %s UpdateAvoid linked %s' % (accn, fmt(accua)), lambda C=accC: (SDTest.Call(0x06f0e0, 'tvi', C),
				accints(C, (0x98,)), accbytes(C, (0xd4, 0xd5)), accf(C + 0xd8, 3), acci(C + 0xcc) == accw2,
				acci(C + 0xc4) == C + 0xc4))
			accpoke(accC + 0xcc, 0)
		accsetpos(accw2, accP2)
		accpoke(accC + 0x94, 0)
		accpoke(accC + 0x98, 0)
		accpokeb(accC + 0xd4, 0)
		accpoke(accC + 0x74, 0)

		# the frame by mode: hold, the checks of the AI, the handler
		def accframe(C):
			return (accints(C, (0x74, 0x94, 0x98, 0x9c, 0x20c)), accbytes(C, (0xd4, 0x1d0, 0x200)), accf(C + 0x10c, 3),
				accf(C + 0x68, 3), accf(C + 0x60, 2), accf(C + 0x80, 2), accctl(), accev())
		accpoke(accC + 0x1b4, accnode)
		accset(accC + 0x220, (1.0, 0.0, -1.0))
		accset(accC + 0x1bc, (900.0, 100.0))
		accpokeb(accC + 0x22c, 1)
		for accup in ((0, 0, 0), (1, 0, 0), (2, 0, 0), (5, 0, 0), (6, 0, 0), (7, 0, 0), (5, 1, 0), (5, 0, 1), (3, 0, 0),
				(4, 0, 0), (6, 0, 2)):
			accpath(accC, accpts)
			accset(accC + 0x10c, accadd(accP, (100.0, 0.0, 10.0)))
			accpoke(accC + 0x74, accup[0])
			accpokeb(accC + 0xbc, accup[1])
			accpokeb(accC + 0x1d0, 1)
			if accup[2] == 1:
				accpoke(accAV + 0xb8, 0x52cf30)
			if accup[2] == 2:
				accpokeb(accA + 0x28, 1)
			accpoke(accA + 0x1a4, 0)
			check('ACC %s Update %s' % (accn, fmt(accup)), lambda C=accC: (accset(accw + 0x4ec, (1.0, 2.0, 3.0, 4.0,
				5.0, 6.0)), SDTest.Call(0x06e5b0, 'tbif', C, 0.25)[0], accframe(C)))
			accpoke(accAV + 0xb8, 0x4d81b0)
			accpokeb(accA + 0x28, 0)
			accpokeb(accC + 0xbc, 0)
		# the handlers by themselves
		for accrv in ((0x0720e0, 'UpdateAway'), (0x0721e0, 'UpdateDir'), (0x072290, 'UpdateGoToDirect'),
				(0x072230, 'UpdateGoTo'), (0x072180, 'UpdateRoute'), (0x072030, 'UpdateFollowMode'),
				(0x071f80, 'UpdateFollowPath')):
			for accdr in (0, 1):
				accpath(accC, accpts)
				accset(accC + 0x10c, accadd(accP, (100.0, 0.0, 10.0)))
				accpokeb(accC + 0x1d0, accdr)
				accpoke(accA + 0x1a4, 0)
				check('ACC %s %s %d' % (accn, accrv[1], accdr), lambda C=accC, rv=accrv[0]: (accset(accw + 0x4ec, (1.0,
					2.0, 3.0, 4.0, 5.0, 6.0)), SDTest.Call(rv, 'tbif', C, 0.25)[0], accframe(C)))
		check('ACC %s UpdateFollowB' % accn, lambda C=accC: (SDTest.Call(0x06dd30, 'tbi', C)[0], accframe(C)))
		accpokeb(accC + 0x1d0, 0)
		accpoke(accC + 0x74, 0)
		accpokeb(accC + 0x22c, 0)
		accpoke(accC + 0x1b4, 0)
		accpoke(accC, accr[2])

	# the end: the controls deleted, the fakes freed, the characters deleted
	check('ACC delete', lambda: (SDTest.Call(0x0731c0, 'tiii', accc2, 1)[0] == accc2,
		SDTest.Call(0x079d50, 'tiii', accc3, 1)[0] == accc3, SDTest.Call(0x06d6f0, 'tiii', acccb, 1)[0] == acccb))
	accpoke(accw + 0x234, 0)
	accfree(accac)
	accfree(acccls)
	accfree(accA)
	accfree(accAV)
	accfree(accnode0)
	accfree(accarg)
	accfree(accvt2)
	accfree(accvt3)
	for accE in (accw, accw2):
		check('ACC delete entity', lambda e=accE: (SDTest.Call(0x208810, 'tbis', accworld + 4, SDTest.Peek(e + 4,
			'@p')), SDTest.Call(acci(acci(e) + 0xc) - 0x400000, 'tiii', e, 1)[0] == e)[1])
