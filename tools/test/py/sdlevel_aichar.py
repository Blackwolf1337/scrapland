# game AI/IChar.c (execfile'd by sdlevel.py, Python 1.5.2): the AI of the walking characters (cAIChar). Pure parts first
# (the vector compare, the state names, the group and repos states, the Nto1 link copies on nodes of our own), then
# AIs of our own: two WalkChars far above the map ("SDAicWalk", "SDAicWalk2", each with a fake action controller whose
# class has only the radii) and a cAIChar on each, made by the constructor after a fixed srand (a third one with the
# 3D control). On them the getters, the script variables, the distances by state, the pause and the action, the kept
# state, the changes of state that need no path search (STATIC*, LOST_PATH*, ORIENT_DINAMIC*, the pending change
# while the character group does not let it, IN_ROUTE and PATROL_ZONE when they only change the speed), the internal
# states of the moves without a path search, the events, the think of LOST_PATH and IN_ROUTE, the look test and the
# start of a talk of the two. The timer of the character group is restored at the end. Not called: the repositioning
# (it moves the character near the camera), the pause and repos slots (camera and model), the talk actions (they play
# actions on the action controller) and every state whose move searches a path (FZ has no 2D graph).
def aicI(a):
	return SDTest.Peek(a, '@i')
def aicB(a):
	return SDTest.Peek(a, '@b')
def aicF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def aicSet(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def aicPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def aicPokeB(a, v):
	w = aicI(a & ~3)
	sh = 8 * (a & 3)
	aicPoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def aicAlloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def aicFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def aicInts(a, offs):
	r = ()
	for o in offs:
		r = r + (aicI(a + o),)
	return r
def aicBytes(a, offs):
	r = ()
	for o in offs:
		r = r + (aicB(a + o),)
	return r
def aicSetPos(e, p):
	SDTest.Call(aicI(aicI(e) + 0x1c) - 0x400000, 'tviF', e, p)
def aicAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
# the high words of doubles (the low words 0) for the variadic functions
aicD = {0.5: 0x3fe00000, 0.75: 0x3fe80000, 1.0: 0x3ff00000, 50.0: 0x40490000, 100.0: 0x40590000,
	300.0: 0x4072c000, 250.0: 0x406f4000}

# ---- the pure parts ----
for aicV in (((1.0, 2.0, 3.0), (1.0, 2.0, 3.0)), ((1.0, 2.0, 3.0), (1.0, 2.0, 3.5)), ((0.0, 0.0, 0.0), (-0.0, 0.0, 0.0)),
		((5.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((-3.4e38, -3.4e38, -3.4e38), (-3.4e38, -3.4e38, -3.4e38)),
		((0.0, 7.0, 0.0), (0.0, -7.0, 0.0))):
	check('AIC Vec3_Differ %s' % fmt(aicV), lambda v=aicV: SDTest.Call('Vec3_Differ', 'iFF', v[0], v[1])[0])
# thiscall without a use of this: called with a dummy this
for aicS in range(-1, 18):
	check('AIC StateName %d' % aicS, lambda s=aicS: SDTest.Call('cAIChar_StateName', 'tviib', 0, s, 'keep')[1])
	check('AIC IsGroupState %d' % aicS, lambda s=aicS: SDTest.Call('cAIChar_IsGroupState', 'tbii', 0, s)[0])
	check('AIC CanRepos %d' % aicS, lambda s=aicS: SDTest.Call('cAIChar_CanRepos', 'tbii', 0, s)[0])
for aicS in range(-1, 15):
	check('AIC InternStateName %d' % aicS, lambda s=aicS: SDTest.Call('cAIChar_InternStateName', 'tviib', 0, s, 'keep')[1])
check('AIC EventNone', lambda: (SDTest.Call('cAIChar_EventNone', 'tbii', 0, 2)[0], SDTest.Call('cAIChar_EventNone@6a500', 'tbii', 0, 7)[0]))

# the Nto1 links: a head with a fake target and two nodes
aicHd = aicAlloc(0x10)
aicN1 = aicAlloc(0x10)
aicN2 = aicAlloc(0x10)
aicPoke(aicHd, aicHd)
aicPoke(aicHd + 4, aicHd)
aicPoke(aicHd + 8, 0x1234)
for aicN in (aicN1, aicN2):
	aicPoke(aicN, aicN)
	aicPoke(aicN + 4, aicN)
	aicPoke(aicN + 0xc, aicN)
def aicRing(a=0):
	return (aicI(aicHd) == aicN2, aicI(aicHd) == aicN1, aicI(aicHd + 4) == aicN1, aicI(aicN2) == aicN1,
		aicI(aicN1) == aicHd, aicI(aicN1 + 4) == aicN2, aicI(aicN1 + 4) == aicHd, aicI(aicN1 + 8), aicI(aicN2 + 8),
		aicI(aicN1) == aicN1, aicI(aicN2) == aicN2, aicI(aicHd) == aicHd)
check('AIC Nto1Head_Link', lambda: (SDTest.Call('Nto1Head_Link', 'tvii', aicHd, aicN1), aicRing(),
	SDTest.Call('Nto1Head_Link', 'tvii', aicHd, aicN2), aicRing()))
check('AIC Nto1Node_UnlinkChecked', lambda: (SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', aicN1), aicRing(),
	SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', aicN2), aicRing()))
aicFree(aicHd)
aicFree(aicN1)
aicFree(aicN2)

# ---- AIs of our own ----
aicWorld = SDTest.Peek('g_pWorld', 'i')
aicGroup = SDTest.Peek('g_CharGroup', 'i')
aicP = (-41000.0, 63000.0, 47000.0)
aicQ = aicAdd(aicP, (200.0, 0.0, 0.0))
SDTest.Call('crt_srand', 'vi', 1717)
aicW = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', aicWorld, 'SDAicWalk', aicP[0], aicP[1], aicP[2], 'WalkChar')[0]
aicW2 = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', aicWorld, 'SDAicWalk2', aicQ[0], aicQ[1], aicQ[2], 'WalkChar')[0]
check('AIC WalkChars', lambda: (aicW != 0, aicW2 != 0, aicGroup != 0, aicW and aicI(aicW + 0x78) == 0,
	aicW2 and aicI(aicW2 + 0x254 + 8) == aicW2, aicW2 and aicB(aicW2 + 0x4c), aicW and aicI(aicW + 0x3e8 + 0xc) != 0))
if aicW and aicW2 and aicI(aicW2 + 0x254 + 8) == aicW2 and aicI(aicW + 0x254 + 8) == aicW:
	aicSetPos(aicW, aicP)
	aicSetPos(aicW2, aicQ)
	aicSet(aicW + 0x428, (0.0, 0.0, 0.0))
	aicSet(aicW2 + 0x428, (0.0, 0.0, 0.0))
	# the fake action controllers: the class with the radii, the action of layer 0 with its priority
	aicFakes = ()
	for aicE in (aicW, aicW2):
		aicAc = aicAlloc(0x80)
		aicCls = aicAlloc(0x200)
		aicAct = aicAlloc(0x80)
		aicPoke(aicAc + 8, aicCls)
		aicPoke(aicAc + 0x14, aicAct)
		aicPoke(aicAct + 0x64, 0x10)
		aicSet(aicCls + 0x164, (40.0,))
		aicSet(aicCls + 0x16c, (25.0,))
		aicPoke(aicE + 0x234, aicAc)
		aicSet(aicE + 0x45c, (30.0,))
		aicFakes = aicFakes + ((aicAc, aicCls, aicAct),)
	aicGroupTime = 0
	if aicGroup:
		aicGroupTime = aicI(aicGroup + 0x5c)
	aicArg = aicAlloc(8)
	SDTest.Call('crt_srand', 'vi', 2468)
	aicM = SDTest.Call('operator_new', 'ii', 0x380)[0]
	aicA = SDTest.Call('cAIChar_ctor', 'tiiii', aicM, aicW, aicArg)[0]
	aicM2 = SDTest.Call('operator_new', 'ii', 0x380)[0]
	aicA2 = SDTest.Call('cAIChar_ctor', 'tiiii', aicM2, aicW2, aicArg)[0]
	aicC = aicI(aicA + 0x19c)
	# the orient flag is not set by the constructor
	aicPokeB(aicA + 0x370, 0)
	aicPokeB(aicA2 + 0x370, 0)
	def aicState(a=0):
		return aicInts(aicA, (0x1a0, 0x1a4, 0x90, 0x1a8, 0x5c)) + aicBytes(aicA, (0x20, 0x28, 0x84, 0x85, 0x1c1, 0x1c2,
			0x35d, 0x35e)) + (aicI(aicA + 0x40) == aicW2, aicI(aicA + 0x40) == 0, aicI(aicC + 0x74))
	def aicCtor(a):
		return (aicI(a) - 0x400000, aicI(a + 0x198) == aicW or aicI(a + 0x198) == aicW2, aicI(a + 0x19c) != 0,
			aicI(aicI(a + 0x19c)) - 0x400000, aicI(a + 0x1ac) == aicI(a + 0x198) + 0x4ec, aicInts(a, (0x1a0, 0x1a4,
			0x1a8, 0x5c, 0x90, 0x24, 0x360, 0x364, 0x368, 0x28c, 0x290)), aicF(a + 0x1b0, 4), aicF(a + 0x354, 2),
			aicF(a + 0x36c, 1), aicF(a + 0x374, 3), aicF(a + 0x2c, 1), aicBytes(a, (0x20, 0x28, 0x48, 0x84, 0x86,
			0x1c0, 0x1c1, 0x1c2, 0x35c, 0x35d, 0x35e)), aicI(a + 0x38) == a + 0x38, aicI(a + 0x44) == a)
	check('AIC ctor', lambda: (aicA == aicM, aicCtor(aicA)))
	check('AIC ctor 2', lambda: (aicA2 == aicM2, aicCtor(aicA2)))

	# the getters
	check('AIC getters', lambda: (SDTest.Call('cAIChar_GetState', 'tii', aicA)[0], SDTest.Call('cAIChar_GetInternState', 'tii', aicA)[0],
		SDTest.Call('cAIChar_GetControl', 'tii', aicA)[0] == aicC, SDTest.Call('cAIChar_GetControl@69800', 'tii', aicA)[0] == aicC,
		SDTest.Call('cAIChar_GetLast', 'tii', aicA)[0] == aicA + 0x28c, SDTest.Call('cAIChar_IsPaused', 'tbi', aicA)[0],
		SDTest.Call('cAIChar_CanTalk', 'tbi', aicA)[0], SDTest.Call('cAIChar_IsTalking', 'tbi', aicA)[0],
		SDTest.Call('cAIChar_GetRepos', 'tbi', aicA)[0], SDTest.Call('cAIChar_GetChanged', 'tbi', aicA)[0],
		SDTest.Call('cAIChar_GetDirect', 'tbi', aicA)[0], SDTest.Call('cAIChar_GetDirect2', 'tbi', aicA)[0],
		SDTest.Call('cAIChar_GetStateName', 'tvib', aicA, 'x')[1], SDTest.Call('cAIChar_GetInternStateName', 'tvib', aicA, 'x')[1]))
	for aicV in (0, 1):
		aicPokeB(aicC + 0x1d0, aicV)
		check('AIC GetDirect %d' % aicV, lambda: (SDTest.Call('cAIChar_GetDirect', 'tbi', aicA)[0],
			SDTest.Call('cAIChar_GetDirect2', 'tbi', aicA)[0]))
	# the script variables
	check('AIC VarI', lambda: (SDTest.Call('cAIChar_VarI', 'tbispi', aicA, 'Repos', 7, 0), SDTest.Call('cAIChar_VarI', 'tbispi',
		aicA, 'Repos', 1, 1), aicB(aicA + 0x86), SDTest.Call('cAIChar_VarI', 'tbispi', aicA, 'Repos', 0, 0),
		SDTest.Call('cAIChar_VarI', 'tbispi', aicA, 'Repos', 5, 1), aicB(aicA + 0x86), SDTest.Call('cAIChar_VarI', 'tbispi', aicA,
		'Repos', 0, 1), aicB(aicA + 0x86), SDTest.Call('cAIChar_VarI', 'tbispi', aicA, '', 3, 0),
		SDTest.Call('cAIChar_VarI', 'tbispi', aicA, 'SDAicNone', 3, 0)))
	check('AIC VarF', lambda: (SDTest.Call('cAIChar_VarF', 'tbisDi', aicA, '', 1.5, 0),
		SDTest.Call('cAIChar_VarF', 'tbisDi', aicA, 'SDAicNone', 1.5, 0)))
	check('AIC VarS VarV empty', lambda: (SDTest.Call('cAIChar_VarS', 'tbisii', aicA, '', 0, 0)[0],
		SDTest.Call('cAIChar_VarV', 'tbisiiii', aicA, '', 0, 0, 0, 0)[0]))
	# the distances by state
	for aicS in (0, 2, 3, 7, 6):
		aicPoke(aicA + 0x1a0, aicS)
		aicSet(aicA + 0x1b0, (1.0, 2.0, 3.0, 4.0))
		check('AIC SetDists %d' % aicS, lambda: (SDTest.Call('cAIChar_SetDists', 'tvi', aicA), aicF(aicA + 0x1b0, 4),
			SDTest.Call('cAIChar_SetDists2', 'tvi', aicA), aicF(aicA + 0x1b0, 4)))
	aicPoke(aicA + 0x1a0, 0)
	check('AIC CheckParams', lambda: SDTest.Call('cAIChar_CheckParams', 'tvii', aicA, 0))
	# the pause and the action button
	aicSet(aicW + 0x4ec, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0))
	aicPokeB(aicW + 0x504, 1)
	check('AIC SetPaused', lambda: (SDTest.Call('cAIChar_SetPaused', 'tvii', aicA, 1), aicB(aicA + 0x84), aicF(aicW + 0x4ec, 6),
		aicB(aicW + 0x504), SDTest.Call('cAIChar_SetPaused', 'tvii', aicA, 0), aicB(aicA + 0x84)))
	check('AIC SetPauseCheck', lambda: (SDTest.Call('cAIChar_SetPauseCheck', 'tvii', aicA, 1), aicBytes(aicA, (0x84, 0x1c2)),
		aicPokeB(aicA + 0x84, 1), SDTest.Call('cAIChar_SetPauseCheck', 'tvii', aicA, 0), aicBytes(aicA, (0x84, 0x1c2))))
	aicPokeB(aicW + 0x504, 1)
	check('AIC UpdateAction0', lambda: (SDTest.Call('cAIChar_UpdateAction0', 'tvi', aicA), aicB(aicW + 0x504)))
	aicPokeB(aicA + 0x28, 1)
	aicPokeB(aicW + 0x504, 1)
	check('AIC SetAction', lambda: (SDTest.Call('cAIChar_SetAction', 'tvii', aicA, 2), aicI(aicA + 0x1a8), aicB(aicW + 0x504),
		SDTest.Call('cAIChar_SetAction', 'tvii', aicA, 0), aicI(aicA + 0x1a8), aicB(aicW + 0x504), SDTest.Call('cAIChar_UpdateMove', 'tvif',
		aicA, 0.5), aicB(aicW + 0x504)))
	# the target: SDAicWalk2
	SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicA + 0x38)
	check('AIC target', lambda: (aicI(aicA + 0x40) == aicW2, aicI(aicW2 + 0x254) == aicA + 0x38))
	for aicT in ((100.0, 300.0, 0x10, 1), (250.0, 300.0, 0x10, 1), (100.0, 150.0, 0x10, 1), (100.0, 300.0, 0x66, 1),
			(100.0, 300.0, 0x65, 1), (100.0, 300.0, 0x10, 0), (199.0, 201.0, 0x10, 1), (200.0, 900.0, 0x10, 1)):
		aicSet(aicA + 0x1b0, (aicT[0], aicT[1]))
		aicPoke(aicFakes[0][2] + 0x64, aicT[2])
		aicPokeB(aicC + 0x1d0, aicT[3])
		aicPokeB(aicW + 0x504, 7)
		check('AIC CheckActDist %s' % fmt(aicT), lambda: (SDTest.Call('cAIChar_CheckActDist', 'tvi', aicA), aicB(aicW + 0x504)))
	aicPoke(aicFakes[0][2] + 0x64, 0x10)
	aicSet(aicA + 0x1b0, (100.0, 300.0))
	for aicT in ((0.0, 0.0, 1, 0.25), (0.5, 0.0, 0, 0.25), (0.0, -1.0, 1, 0.125), (0.0, 0.0, 0, 3.0)):
		aicSet(aicW2 + 0x4f8, (aicT[0], aicT[1]))
		aicPokeB(aicW + 0x504, aicT[2])
		aicSet(aicA + 0xc, (aicT[3],))
		aicSet(aicA + 0x158 + 4, (0.0,))
		check('AIC UpdateAction1 %s' % fmt(aicT), lambda: (SDTest.Call('cAIChar_UpdateAction1', 'tvi', aicA), aicF(aicA + 0x354, 2),
			aicB(aicW + 0x504), aicF(aicA + 0x158 + 4, 1)))
	aicSet(aicW2 + 0x4f8, (0.0, 0.0))
	aicPokeB(aicC + 0x1d0, 0)
	# IsMoving by the internal state, the state, the control and the walk of the character
	aicSave = aicInts(aicA, (0x1a0, 0x1a4))
	for aicIn in range(0, 14):
		for aicS in (0, 1, 2, 4, 7, 8):
			for aicT in ((0.0, 0.0, 0, 0), (0.5, 0.0, 0, 0), (0.0, -1.0, 1, 0), (0.25, 0.0, 0, 1)):
				aicPoke(aicA + 0x1a0, aicS)
				aicPoke(aicA + 0x1a4, aicIn)
				aicSet(aicW + 0x4f8, (aicT[0], aicT[1]))
				aicPokeB(aicC + 0x1d0, aicT[2])
				aicPokeB(aicC + 0xd4, aicT[3])
				check('AIC IsMoving %d %d %s' % (aicIn, aicS, fmt(aicT)), lambda: SDTest.Call('cAIChar_IsMoving', 'tbi',
					aicA)[0])
	aicPokeB(aicC + 0x1d0, 0)
	aicPokeB(aicC + 0xd4, 0)
	aicSet(aicW + 0x4f8, (0.0, 0.0))
	aicPoke(aicA + 0x1a0, aicSave[0])
	aicPoke(aicA + 0x1a4, aicSave[1])
	# the target there: CheckAction and React do nothing
	aicPoke(aicA + 0x1a8, 1)
	check('AIC CheckAction', lambda: (SDTest.Call('cAIChar_CheckAction', 'tbi', aicA)[0], aicPoke(aicA + 0x1a8, 0),
		SDTest.Call('cAIChar_CheckAction', 'tbi', aicA)[0]))
	for aicS in (0, 4, 8, 9, 2):
		aicPoke(aicA + 0x1a0, aicS)
		aicPokeB(aicA + 0x28, 0)
		check('AIC React %d' % aicS, lambda: (SDTest.Call('cAIChar_React', 'tbi', aicA)[0], aicState()))
	aicPoke(aicA + 0x1a0, 0)
	# the entity run into
	check('AIC GetCollide none', lambda: SDTest.Call('cAIChar_GetCollide', 'tii', aicA)[0] == 0)
	if aicI(aicW + 0x3e8 + 0xc):
		SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicW + 0x3e8)
		check('AIC GetCollide', lambda: SDTest.Call('cAIChar_GetCollide', 'tii', aicA)[0] == aicW2)
		SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', aicW + 0x3e8)
	# the look test of two characters by their views
	for aicV in (((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((0.0, -1.5707963, 0.0), (0.0, 0.0, 0.0)),
			((0.0, 1.5707963, 0.0), (0.0, 1.5707963, 0.0)), ((0.0, 0.0, 0.0), (0.0, 1.2, 0.0)),
			((1.2, -1.5707963, 0.0), (0.0, 3.0, 0.0)), ((-0.9, 2.0, 0.3), (0.4, -0.5, 0.0))):
		aicSet(aicW + 0x428, aicV[0])
		aicSet(aicW2 + 0x428, aicV[1])
		for aicFl in (0, 1):
			aicPokeB(aicA + 0x48, aicFl)
			check('AIC Facing %s %d' % (fmt(aicV), aicFl), lambda: (SDTest.Call('cAIChar_Facing', 'tbiii', aicA, aicW,
				aicW2)[0], SDTest.Call('cAIChar_Facing', 'tbiii', aicA, aicW2, aicW)[0]))
	aicPokeB(aicA + 0x48, 0)
	aicSet(aicW + 0x428, (0.0, 0.0, 0.0))
	aicSet(aicW2 + 0x428, (0.0, 0.0, 0.0))

	# the kept state
	aicBuf = aicAlloc(0xc8)
	def aicSaved(s):
		return (aicInts(s, (0, 4, 0xbc)), aicF(s + 8, 5), SDTest.Peek(s + 0x1c, '@s'), aicF(s + 0x9c, 3),
			aicF(s + 0xac, 4), aicF(s + 0xc0, 1), aicBytes(s, (0xa8, 0xa9, 0xc4, 0xc5, 0xc6, 0xc7)))
	aicSet(aicA + 0x4c, (1.0, 2.0, 3.0, 4.0))
	aicSet(aicA + 0x60, (0.75, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0))
	aicPokeB(aicA + 0x80, 1)
	aicPokeB(aicA + 0x1c0, 1)
	check('AIC SaveState', lambda: (SDTest.Call('cAIChar_SaveState', 'tvii', aicA, aicBuf), aicSaved(aicBuf)))
	check('AIC SaveTo', lambda: (SDTest.Call('cAIChar_SaveTo', 'tiii', aicA, aicBuf)[0] == aicBuf, aicSaved(aicBuf)))
	SDTest.Call('cAIChar_SaveState', 'tvii', aicA, aicA + 0x1c4)
	aicSet(aicA + 0x4c, (0.0, 0.0, 0.0, 0.0))
	aicPokeB(aicA + 0x1c0, 0)
	SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', aicA + 0x38)
	check('AIC RestoreSaved', lambda: (SDTest.Call('cAIChar_RestoreSaved', 'tvii', aicA, aicBuf), aicF(aicA + 0x4c, 4),
		aicF(aicA + 0x60, 8), aicBytes(aicA, (0x80, 0x1c0, 0x20, 0x28)), aicI(aicA + 0x40) == aicW2))
	aicPokeB(aicBuf + 0x1c, 0)
	check('AIC RestoreSaved no target', lambda: (SDTest.Call('cAIChar_RestoreSaved', 'tvii', aicA, aicBuf), aicI(aicA + 0x40) == 0,
		aicI(aicA + 0x38) == aicA + 0x38))
	check('AIC BeginChange', lambda: (aicSet(aicA + 0x2c, (2.5,)), SDTest.Call('cAIChar_BeginChange', 'tvii', aicA, 0),
		aicSaved(aicA + 0x28c), aicF(aicA + 0x2c, 1), aicState()))

	# the internal states without a path search
	SDTest.Call('cAICharControl_ClearAvoid', 'tvi', aicC)
	aicSet(aicA + 0xc, (0.0,))
	SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicA + 0x38)
	aicPokeB(aicA + 0x28, 0)
	for aicArgs in ((0,), (1,), (3, aicP[0] + 500.0, aicP[1], aicP[2], 0, aicD[300.0], 0, aicD[0.75]), (11,),
			(6, 1.0, 0.0, -1.0), (8, 1), (9, 0), (12, aicW2), (0,)):
		check('AIC SetInternState %s' % fmt(aicArgs[0]), lambda a=aicArgs: (apply(SDTest.Call, (sym('cAIChar_SetInternState'), 'vii' +
			'fff' * (a[0] in (3, 6)) + 'i' * (len(a) - 1 - 3 * (a[0] in (3, 6))), aicA) + a), aicState(),
			aicF(aicA + 0x4c, 6), aicF(aicA + 0x64, 3), aicI(aicA + 0x24), aicB(aicA + 0x370), aicB(aicC + 0xbc),
			aicF(aicC + 0x68, 3), aicF(aicC + 0x54, 1), aicF(aicC + 0x220, 3), aicI(aicC + 0x1b4) == aicA + 0x38))
	# ChangeStateMore: the states the default change takes
	aicSet(aicC + 0xa4, (aicP[0], aicP[1], aicP[2] + 300.0, aicP[0] + 7.0, aicP[1], aicP[2]))
	for aicS in (11, 12, 13, 14, 15, 10, 10):
		check('AIC ChangeStateMore %d' % aicS, lambda s=aicS: (SDTest.Call('cAIChar_ChangeStateMore', 'tbiiii', aicA, s, 1, 0)[0],
			aicState(), aicF(aicA + 0x374, 3), aicF(aicC + 0x68, 3)))
		SDTest.Call('cAICharControl_ClearAvoid', 'tvi', aicC)
	# the changes of state from outside and from inside
	for aicF2, aicArgs in ((sym('cAIChar_SetState'), (12,)), (sym('cAIChar_SetState'), (0,)), (sym('cAIChar_SetStateIntern'), (13,)), (sym('cAIChar_SetStateIntern'), (11,)),
			(sym('cAIChar_SetStateIntern'), (14,)), (sym('cAIChar_SetState'), (10,)), (sym('cAIChar_SetStateIntern'), (0,))):
		check('AIC change %x %s' % (aicF2, fmt(aicArgs)), lambda f=aicF2, a=aicArgs: (apply(SDTest.Call, (f,
			'bi' + 'i' * len(a), aicA) + a)[0], aicState(), aicB(aicA + 0x1c2)))
	if aicGroup:
		# ORIENT_DINAMIC_OBJECTIVE from inside (the group lets it), then the pending change from outside
		check('AIC change ORIENT', lambda: (SDTest.Call('cAIChar_SetStateIntern', 'biiii', aicA, 8, aicW2, 1)[0], aicState(),
			aicB(aicA + 0x370), aicF(aicA + 0x1b0, 4), aicI(aicGroup + 0x5c)))
		aicSet(aicGroup + 0x5c, (-1.0,))
		check('AIC change pending', lambda: (SDTest.Call('cAIChar_SetState', 'biiii', aicA, 9, aicW2, 0)[0], aicState(),
			SDTest.Peek(aicA + 0xac, '@s'), aicB(aicA + 0x157)))
		check('AIC RestoreStateIntern pending', lambda: (SDTest.Call('cAIChar_RestoreStateIntern', 'tbii', aicA, aicA + 0x90)[0],
			aicState(), aicB(aicA + 0x370)))
		aicSet(aicGroup + 0x5c, (-1.0,))
		check('AIC CanTarget fails', lambda: (SDTest.Call('cAIChar_SetState', 'biiii', aicA, 8, 0, 0)[0], aicState()))
		aicPoke(aicGroup + 0x5c, aicGroupTime)
		aicPoke(aicA + 0x90, 15)
		# IN_ROUTE and PATROL_ZONE again while FOLLOW_THE_PATH runs: only the speed changes
		for aicS in (1, 6):
			aicPoke(aicA + 0x1a0, aicS)
			aicPoke(aicA + 0x1a4, 10)
			aicSet(aicA + 0x60, (0.5,))
			aicSet(aicA + 0x70, (aicP[0], aicP[1], aicP[2], 250.0))
			aicPokeB(aicA + 0x87, 0)
			check('AIC Resume %d' % aicS, lambda: (SDTest.Call('cAIChar_Resume', 'tvi', aicA), aicState(), aicF(aicA + 0x60, 1),
				aicF(aicC + 0xf4, 1), aicF(aicA + 0x36c, 1), aicF(aicA + 0x70, 4)))
			check('AIC AfterRepos %d' % aicS, lambda: (SDTest.Call('cAIChar_AfterRepos', 'tvi', aicA), aicState()))
		aicPoke(aicGroup + 0x5c, aicGroupTime)
	SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)
	check('AIC RestoreState', lambda: (aicPoke(aicBuf, 13), SDTest.Call('cAIChar_RestoreState', 'tbii', aicA, aicBuf)[0],
		aicState(), aicPoke(aicBuf, 0), SDTest.Call('cAIChar_RestoreState', 'tbii', aicA, aicBuf)[0], aicState(), aicPoke(aicBuf, 99),
		SDTest.Call('cAIChar_RestoreState', 'tbii', aicA, aicBuf)[0]))

	# the events
	SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicA + 0x38)
	SDTest.Call('cAIChar_SaveState', 'tvii', aicA, aicA + 0x1c4)
	check('AIC EventChaseBack', lambda: (SDTest.Call('cAIChar_EventChaseBack', 'tbii', aicA, 8)[0], aicB(aicA + 0x1c1),
		SDTest.Call('cAIChar_EventChaseBack', 'tbii', aicA, 9)[0], aicB(aicA + 0x1c1), SDTest.Call('cAIChar_EventChaseBack', 'tbii', aicA, 5)[0]))
	aicSet(aicA + 0x64, (0.0, 0.0, 1.0))
	for aicIn, aicEv in ((2, 2), (0, 2), (6, 6), (2, 6), (0, 3)):
		aicPoke(aicA + 0x1a4, aicIn)
		check('AIC EventGoTo %d %d' % (aicIn, aicEv), lambda e=aicEv: (SDTest.Call('cAIChar_EventGoTo', 'tbii', aicA, e)[0],
			aicState(), aicF(aicC + 0x220, 3)))
	aicSet(aicA + 0x374, (-3.4028234663852886e38,) * 3)
	aicSet(aicC + 0x68, aicQ)
	check('AIC EventLostPath none', lambda: (SDTest.Call('cAIChar_EventLostPath', 'tbii', aicA, 2)[0], aicState()))
	aicSet(aicA + 0x374, aicAdd(aicP, (0.0, 0.0, 600.0)))
	check('AIC EventLostPath', lambda: (SDTest.Call('cAIChar_EventLostPath', 'tbii', aicA, 2)[0], aicState(), aicF(aicC + 0x68, 3),
		SDTest.Call('cAIChar_EventLostPath', 'tbii', aicA, 2)[0], aicState(), SDTest.Call('cAIChar_EventLostPath', 'tbii', aicA, 3)[0],
		SDTest.Call('cAIChar_EventLostPath', 'tbii', aicA, 7)[0], aicState(), SDTest.Call('cAIChar_SetPauseCheck', 'tvii', aicA, 0)))
	SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicA + 0x38)
	for aicS, aicEv in ((0, 1), (0, 3), (0, 7), (0, 2), (0, 5), (10, 0), (0, 0), (2, 0), (3, 0)):
		aicPoke(aicA + 0x1a0, aicS)
		SDTest.Call('cAICharControl_ClearAvoid', 'tvi', aicC)
		check('AIC EventDefault %d %d' % (aicS, aicEv), lambda e=aicEv: (SDTest.Call('cAIChar_EventDefault', 'tbii', aicA, e)[0],
			aicState(), aicF(aicA + 0x374, 3), aicI(aicA + 0x1c4)))
	SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)
	for aicS, aicIn, aicEv in ((0, 0, 5), (0, 0, 2), (4, 0, 2), (4, 2, 2), (7, 0, 8), (7, 0, 1), (2, 0, 4),
			(3, 0, 6), (8, 0, 9), (1, 12, 6), (6, 12, 6), (5, 0, 3), (10, 0, 7)):
		aicPoke(aicA + 0x1a0, aicS)
		aicPoke(aicA + 0x1a4, aicIn)
		aicPokeB(aicA + 0x35e, 1)
		check('AIC Event %d %d %d' % (aicS, aicIn, aicEv), lambda e=aicEv: (SDTest.Call('cAIChar_Event', 'tbii', aicA, e)[0],
			aicState()))
	SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)
	# the turn to a talk is taken already; the patrol event 10 keeps the way into the zone
	aicPoke(aicA + 0x1a4, 12)
	aicPokeB(aicA + 0x35e, 1)
	check('AIC EventTurn taken', lambda: (SDTest.Call('cAIChar_EventInRoute', 'tbii', aicA, 6)[0], SDTest.Call('cAIChar_EventPatrol', 'tbii',
		aicA, 6)[0], SDTest.Call('cAIChar_EventInRoute', 'tbii', aicA, 3)[0], SDTest.Call('cAIChar_EventPatrol', 'tbii', aicA, 3)[0],
		SDTest.Call('cAIChar_EventGetAway', 'tbii', aicA, 3)[0], aicState()))
	aicSet(aicA + 0x70, (aicP[0], aicP[1], aicP[2] - 5000.0, 100.0))
	check('AIC EventPatrol 10 away', lambda: (SDTest.Call('cAIChar_EventPatrol', 'tbii', aicA, 10)[0], aicState()))
	SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)

	# the think of the states
	SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicA + 0x38)
	for aicT in (20.0, 5.0, 15.0):
		aicPoke(aicA + 0x1a0, 10)
		aicSet(aicA + 0x2c, (aicT,))
		check('AIC Think LOST_PATH %s' % fmt(aicT), lambda: (SDTest.Call('cAIChar_Think', 'tvi', aicA), aicState()))
	SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicA + 0x38)
	aicPoke(aicA + 0x1a0, 11)
	aicSet(aicA + 0x1b8, (100.0, 300.0))
	check('AIC Think LOST_PATH_CHASE', lambda: (SDTest.Call('cAIChar_Think', 'tvi', aicA), aicState(), aicF(aicC + 0x68, 3),
		aicF(aicA + 0x4c, 3), aicF(aicA + 0x58, 1)))
	SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)
	for aicS in (0, 2, 3, 4, 5, 7):
		aicPoke(aicA + 0x1a0, aicS)
		check('AIC Think nothing %d' % aicS, lambda: (SDTest.Call('cAIChar_Think', 'tvi', aicA), aicState()))
	# IN_ROUTE: the pauses of FOLLOW_THE_PATH and their end
	aicPoke(aicA + 0x1a0, 1)
	SDTest.Call('cAIChar_SaveState', 'tvii', aicA, aicA + 0x1c4)
	for aicSeed in (17, 18, 19, 25):
		aicPoke(aicA + 0x1a4, 10)
		aicPokeB(aicA + 0x1c0, 1)
		aicSet(aicA + 0x2c, (100.0,))
		SDTest.Call('crt_srand', 'vi', aicSeed)
		check('AIC ThinkInRoute pause %d' % aicSeed, lambda: (SDTest.Call('cAIChar_ThinkInRoute', 'tvi', aicA), aicState(),
			aicF(aicA + 0x2c, 1), aicI(aicA + 0x1c8)))
	aicPokeB(aicA + 0x1c0, 0)
	aicPoke(aicA + 0x1a4, 10)
	check('AIC ThinkInRoute no pause', lambda: (SDTest.Call('cAIChar_ThinkInRoute', 'tvi', aicA), aicState()))
	aicPoke(aicA + 0x1a4, 1)
	aicPokeB(aicA + 0x1c0, 1)
	aicSet(aicA + 0x2c, (0.0,))
	aicSet(aicW + 0x458, (3.0,))
	check('AIC ThinkInRoute paused', lambda: (SDTest.Call('cAIChar_ThinkInRoute', 'tvi', aicA), aicState(), aicF(aicW + 0x458, 1)))
	aicPokeB(aicA + 0x1c0, 0)
	check('AIC ThinkInRoute end pause', lambda: (SDTest.Call('cAIChar_ThinkInRoute', 'tvi', aicA), aicState(), aicF(aicA + 0x2c, 1),
		aicB(aicC + 0xbc)))
	aicPoke(aicA + 0x1a4, 12)
	aicPokeB(aicA + 0x35e, 0)
	check('AIC ThinkInRoute talk not turned', lambda: (SDTest.Call('cAIChar_ThinkInRoute', 'tvi', aicA), aicState()))
	# PATROL_ZONE: out of the zone FOLLOW_THE_PATH relaxes
	aicPoke(aicA + 0x1a0, 6)
	for aicZ in ((0.0, 0.0, 0.0, 100.0), (0.0, 0.0, -300.0, 100.0), (0.0, 0.0, -300.0, 400.0)):
		aicPoke(aicA + 0x1a4, 10)
		aicPokeB(aicA + 0x83, 1)
		aicPokeB(aicA + 0x1c0, 0)
		aicSet(aicA + 0x2c, (3.0,))
		aicSet(aicA + 0x70, (aicP[0] + aicZ[0], aicP[1] + aicZ[1], aicP[2] + aicZ[2], aicZ[3]))
		check('AIC ThinkPatrol %s' % fmt(aicZ), lambda: (SDTest.Call('cAIChar_ThinkPatrol', 'tvi', aicA), aicState(),
			aicF(aicA + 0x2c, 1)))
	aicPokeB(aicA + 0x83, 0)
	aicPoke(aicA + 0x1a4, 10)
	check('AIC ThinkPatrol not in path', lambda: (SDTest.Call('cAIChar_ThinkPatrol', 'tvi', aicA), aicState()))
	aicPoke(aicA + 0x1a4, 1)
	aicPokeB(aicA + 0x1c0, 0)
	check('AIC ThinkPatrol end pause', lambda: (SDTest.Call('cAIChar_ThinkPatrol', 'tvi', aicA), aicState()))
	SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)

	# the update in STATIC and the update off
	aicPoke(aicA + 0x90, 15)
	aicSet(aicA + 0x36c, (1.0,))
	check('AIC Update STATIC', lambda: (SDTest.Call('cAIChar_Update', 'tvif', aicA, 0.25), aicF(aicA + 0xc, 1),
		aicF(aicA + 0x36c, 1), aicState()))
	check('AIC UpdateOff', lambda: (aicSet(aicA + 0x2c, (1.0,)), SDTest.Call('cAIChar_UpdateOff', 'tvif', aicA, 0.5),
		aicF(aicA + 0x2c, 1)))
	check('AIC UpdateThink', lambda: (aicSet(aicA + 0xc, (2.0,)), SDTest.Call('cAIChar_UpdateThink', 'tvi', aicA),
		aicF(aicA + 0x14, 1), aicState()))
	check('AIC CheckPause off', lambda: (SDTest.Call('cAIChar_CheckPause', 'tvi', aicA), aicSet(aicA + 0xc, (0.0,)),
		SDTest.Call('cAIChar_CheckRepos', 'tvi', aicA), aicState()))

	# a talk starts: both walk and run into each other at the same height
	aicPoke(aicW2 + 0x78, aicA2)
	if aicI(aicW + 0x3e8 + 0xc):
		SDTest.Call('Nto1Head_Link', 'tvii', aicW2 + 0x254, aicW + 0x3e8)
	for aicT in ((1, 10, 1, 10, 10.0, 0.0), (0, 10, 1, 10, 10.0, 0.0), (1, 10, 1, 10, 4.0, 0.0),
			(1, 10, 0, 10, 10.0, 0.0), (1, 10, 1, 1, 10.0, 0.0), (1, 2, 6, 2, 10.0, 0.0), (1, 10, 1, 10, 10.0, 150.0),
			(1, 10, 1, 10, 10.0, 60.0), (1, 3, 1, 10, 10.0, 0.0)):
		aicPokeB(aicA + 0x35c, aicT[0])
		aicPoke(aicA + 0x1a0, 1)
		aicPoke(aicA + 0x1a4, aicT[1])
		aicPoke(aicA2 + 0x1a0, aicT[2])
		aicPoke(aicA2 + 0x1a4, aicT[3])
		aicSet(aicA + 0x36c, (aicT[4],))
		aicSetPos(aicW2, aicAdd(aicQ, (0.0, aicT[5], 0.0)))
		check('AIC CheckTalk %s' % fmt(aicT), lambda: (SDTest.Call('cAIChar_ReactInRoute', 'tvi', aicA), aicState(),
			aicInts(aicA2, (0x1a0, 0x1a4)), aicI(aicA2 + 0x40) == aicW, aicI(aicA + 0x40) == aicW2))
		SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA, 0)
		SDTest.Call('cAIChar_SetStateIntern', 'bii', aicA2, 0)
	aicSetPos(aicW2, aicQ)
	aicPoke(aicW2 + 0x78, 0)
	if aicI(aicW + 0x3e8 + 0xc):
		SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', aicW + 0x3e8)

	# the end: a third AI with the 3D control, then the AIs deleted, the fakes freed, the characters deleted
	aicPokeB(aicFakes[0][1] + 0x186, 1)
	SDTest.Call('crt_srand', 'vi', 1357)
	aicM3 = SDTest.Call('operator_new', 'ii', 0x380)[0]
	aicA3 = SDTest.Call('cAIChar_ctor', 'tiiii', aicM3, aicW, aicArg)[0]
	check('AIC ctor 3D', lambda: (aicA3 == aicM3, aicCtor(aicA3)))
	check('AIC Delete', lambda: (SDTest.Call('cAIChar_Delete', 'tiii', aicA3, 1)[0] == aicA3, SDTest.Call('cAIChar_Delete', 'tiii',
		aicA, 1)[0] == aicA, SDTest.Call('cAIChar_Delete', 'tiii', aicA2, 1)[0] == aicA2))
	if aicGroup:
		aicPoke(aicGroup + 0x5c, aicGroupTime)
	aicFree(aicBuf)
	aicFree(aicArg)
	for aicE in (aicW, aicW2):
		aicPoke(aicE + 0x234, 0)
	for aicFk in aicFakes:
		for aicP2 in aicFk:
			aicFree(aicP2)
	for aicE in (aicW, aicW2):
		check('AIC delete entity', lambda e=aicE: (SDTest.Call('PtrTable_Delete@208810', 'tbis', aicWorld + 4, SDTest.Peek(e + 4,
			'@p')), SDTest.Call(aicI(aicI(e) + 0xc) - 0x400000, 'tiii', e, 1)[0] == e)[1])
