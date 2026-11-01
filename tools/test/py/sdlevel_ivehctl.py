# game AI/IVehicleControl.c and IVehicleShootControl.c (execfile'd by sdlevel.py, Python 1.5.2): the move control and
# the shoot control of the vehicle AI. Two Cars of our own far away ("SDIvcCar", "SDIvcCar2" ahead of it as target)
# with the model SPoli1, the cAIVehicle of the first made by the constructor of the exe (it makes both controls) after
# a fixed srand; a second control and shoot control made directly and deleted. On the move control: the getters and
# setters, the line tests of the corridor around the car in the air and through the ground, the direct path of
# SetMove(2) and what runs on it (smoothing, the point search, the turn, the strafe, the acceleration, the arrival
# with the events the frozen AI ignores, the update), the orientation to the target (SetMove(6)), the chase parts
# with the target, the random goals. Not called: the moves that search the graph (SetPath, SetChase, RebuildPath:
# a failed search is an event that changes the state of the AI). On the shoot control: the weapon table with weapons
# given to the car for the test (slots, upgrade, ammo), the ammo sums, the ranges, the choice of the weapon and of
# the countermeasure, the modes and the updates with the target. The vehicle group: the elevators of FZ, a group of
# our own. Everything poked is restored; heap addresses are only compared.
def ivcI(a):
	return SDTest.Peek(a, '@i')
def ivcB(a):
	return SDTest.Peek(a, '@b')
def ivcF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def ivcSet(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def ivcPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def ivcSetByte(a, v):
	w = ivcI(a & ~3)
	sh = 8 * (a & 3)
	ivcPoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def ivcAlloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def ivcFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def ivcNew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def ivcSrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def ivcInts(a, offs):
	r = ()
	for o in offs:
		r = r + (ivcI(a + o),)
	return r
def ivcBytes(a, offs):
	r = ()
	for o in offs:
		r = r + (ivcB(a + o),)
	return r
def ivcSave(a, n):
	r = ()
	for k in range(n):
		r = r + (ivcI(a + 4 * k),)
	return r
def ivcRestore(a, v):
	for k in range(len(v)):
		ivcPoke(a + 4 * k, v[k])
def ivcFind(name):
	w = SDTest.Peek('g_pWorld', 'i')
	slot = SDTest.Call('HashTable_Slot', 'tiis', w + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', w + 4, name, slot)[0]
	if node:
		return ivcI(node)
	return 0
def ivcVt(e, off):
	return ivcI(ivcI(e) + off) - 0x400000
def ivcPos(e):
	return SDTest.Call(ivcVt(e, 0x18), 'tiiF', e, (0.0, 0.0, 0.0))[1]
def ivcAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])

# the doubles of the variadic SetMove as two ints (low, high)
ivcD0 = (0, 0)
ivcD05 = (0, 0x3fe00000)
ivcD1 = (0, 0x3ff00000)
ivcD2 = (0, 0x40000000)
ivcD300 = (0, 0x4072c000)
ivcD5000 = (0, 0x40b38800)

ivcGroup = SDTest.Peek('g_VehicleGroup', 'i')
ivcNodesWorld = SDTest.Peek('g_NodesWorld', 'i')
ivcCarObj = Scrap.CreateEntity('SDIvcCar', 56000.0, 50000.0, 60000.0, 'Car')
ivcCar = ivcFind('SDIvcCar')
ivcCar2Obj = Scrap.CreateEntity('SDIvcCar2', 56000.0, 50000.0, 52000.0, 'Car')
ivcCar2 = ivcFind('SDIvcCar2')
check('SDIvcCar', lambda: (ivcCarObj is not None, ivcCar != 0, ivcCar2 != 0, ivcGroup != 0, ivcNodesWorld != 0))
if ivcCar and ivcCar2 and ivcGroup and ivcNodesWorld:
	for ivcC in (ivcCar, ivcCar2):
		SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', ivcC, 'SPoli1', 1.0, 1.0, 1.0, 1)
		ivcSet(ivcC + 0xac, (0.0, 0.0, 0.0))
	ivcGroupSave = ivcSave(ivcGroup + 0x5c, 6) + (ivcI(ivcGroup + 0xf0),)
	ivcCarSave = ivcSave(ivcCar + 0xac, 3) + ivcSave(ivcCar + 0xd4, 6) + (ivcI(ivcCar + 0x46c),)
	ivcCtlSave = ivcSave(ivcCar + 0x414, 17)
	ivcCar2Active = ivcB(ivcCar2 + 0x4c)
	# the parameter blocks of the AI in their ranges (a value out of them is a CHECK)
	ivcP1 = ivcAlloc(0x2c)
	ivcP2 = ivcAlloc(0x18)
	ivcP3 = ivcAlloc(0x1c)
	ivcSet(ivcP1, (0.5, 0.25, 0.75))
	ivcPoke(ivcP1 + 0xc, 0)
	ivcPoke(ivcP1 + 0x10, 1)
	ivcPoke(ivcP1 + 0x14, 0)
	ivcPoke(ivcP1 + 0x18, 0)
	ivcPoke(ivcP1 + 0x1c, 3)
	ivcPoke(ivcP1 + 0x20, 0)
	ivcPoke(ivcP1 + 0x24, 40)
	ivcPoke(ivcP1 + 0x28, 60)
	ivcPoke(ivcP2, 0)
	ivcPoke(ivcP2 + 4, 2)
	ivcPoke(ivcP2 + 8, 1)
	ivcPoke(ivcP2 + 0xc, 4)
	ivcSet(ivcP2 + 0x10, (0.5,))
	ivcPoke(ivcP2 + 0x14, 5)
	ivcPoke(ivcP3, 50)
	ivcPoke(ivcP3 + 4, 3)
	ivcSet(ivcP3 + 8, (1.0, 0.5))
	ivcPoke(ivcP3 + 0x10, 35)
	ivcPoke(ivcP3 + 0x14, 50)
	ivcPoke(ivcP3 + 0x18, 15)
	ivcSrand(777)
	ivcMem = ivcNew(0x3b0)
	ivcAI = SDTest.Call('cAIVehicle_ctor', 'tiiiiii', ivcMem, ivcCar, ivcP1, ivcP2, ivcP3)[0]
	ivcC = ivcI(ivcAI + 0x194)
	ivcS = ivcI(ivcAI + 0x198)
	ivcCtl = ivcCar + 0x414
	def ivcCtls(a=0):
		return ivcF(ivcCtl + 4, 5) + (ivcBytes(ivcCtl, (0x20, 0x21, 0x22, 0x23, 0x24)),)
	# the fields of the move control the code writes (not the radius: the model of the car)
	def ivcCF(a=0):
		return (ivcF(ivcC + 0x38, 5), ivcInts(ivcC, (0x88, 0x8c, 0xac, 0xd8, 0x18c)), ivcF(ivcC + 0x90, 2),
			ivcF(ivcC + 0xa4, 2), ivcBytes(ivcC, (4, 0xb0, 0xb1, 0xf8, 0x100, 0x108, 0x174, 0x188, 0x189, 0x190,
			0x192, 0x193, 0x19c, 0x19d)), ivcF(ivcC + 0xf0, 2), ivcF(ivcC + 0xfc, 1), ivcF(ivcC + 0x104, 1),
			ivcF(ivcC + 0x160, 2), ivcF(ivcC + 0x178, 4), ivcF(ivcC + 0x194, 2))
	def ivcPts(a=0):
		return (ivcF(ivcC + 0x7c, 3), ivcF(ivcC + 0x98, 3), ivcF(ivcC + 0xc4, 3), ivcF(ivcC + 0x1a0, 3))
	check('cAIVehicle_ctor', lambda: (ivcAI == ivcMem, ivcC != 0, ivcS != 0, ivcI(ivcC) - 0x400000,
		ivcI(ivcS) - 0x400000, ivcI(ivcC + 0x28) == ivcAI, ivcI(ivcC + 0x2c) == ivcCar, ivcI(ivcC + 0x30) == ivcCtl,
		ivcI(ivcS + 0xc) == ivcAI, ivcI(ivcS + 0x44) == ivcCar, ivcI(ivcS + 0x48) == ivcCtl))
	check('cAIVehicleControl after ctor', lambda: (ivcCF(), ivcPts(), ivcInts(ivcC, (0x4c, 0x50, 0x54, 0x58, 0x60, 0x64,
		0x68, 0x6c, 0x70, 0x78, 0xe0, 0xe4)), ivcF(ivcC + 0x5c, 1), ivcF(ivcC + 0x74, 1), ivcF(ivcC + 0x1b0, 54),
		ivcF(ivcC + 0x288, 12), ivcB(ivcCar + 0x71c), ivcI(ivcC + 0x34) == ivcI(ivcCar + 0x3a0)))

	# a move control of its own, made and deleted
	ivcSrand(778)
	ivcM2 = ivcNew(0x2b8)
	ivcC2 = SDTest.Call('cAIVehicleControl_ctor', 'tiiiii', ivcM2, ivcCar, ivcAI, ivcP2)[0]
	check('cAIVehicleControl_ctor', lambda: (ivcC2 == ivcM2, ivcI(ivcC2) - 0x400000, ivcF(ivcC2 + 0x38, 4),
		ivcInts(ivcC2, (0x88, 0x8c, 0xac, 0x15c, 0x18c, 0x1ac, 0xd0, 0xd4, 0xd8, 0xdc, 0xe0, 0xe4, 0xe8, 0xec)),
		ivcF(ivcC2 + 0x7c, 3), ivcF(ivcC2 + 0x90, 6), ivcF(ivcC2 + 0xc4, 3), ivcF(ivcC2 + 0x168, 3),
		ivcF(ivcC2 + 0x178, 4), ivcF(ivcC2 + 0x194, 2), ivcF(ivcC2 + 0x1a0, 3), ivcBytes(ivcC2, (4, 0xb0, 0xb1, 0xf8,
		0x100, 0x174, 0x188, 0x189, 0x190, 0x191, 0x192, 0x193, 0x19c, 0x19d)), ivcF(ivcC2 + 0x1b0, 54),
		ivcF(ivcC2 + 0x288, 12), ivcF(ivcC2 + 0xc, 3), ivcF(ivcC2 + 0x120, 3), ivcF(ivcC2 + 0x150, 3)))
	check('cAIVehicleControl_Delete', lambda: SDTest.Call('cAIVehicleControl_Delete', 'tiii', ivcC2, 1)[0] == ivcC2)

	# the getters and setters
	check('cAIVehicleControl getters', lambda: (SDTest.Call('cAIVehicleControl_GetGoal', 'tiiF', ivcC, (0.0,) * 3)[1],
		SDTest.Call('cAIVehicleControl_GetDest', 'tiiF', ivcC, (0.0,) * 3)[1], SDTest.Call('cAIVehicleControl_GetParams', 'tii', ivcC)[0] == ivcC + 0x4c,
		SDTest.Call('cAIVehicleControl_GetParamsInit', 'tii', ivcC)[0] == ivcC + 0x64, SDTest.Call('cAIVehicleControl_GetFlag', 'tbi', ivcC)[0],
		SDTest.Call('cAIVehicleControl_InSight', 'tbi', ivcC)[0], SDTest.Call('cAIVehicleControl_GetStrafing', 'tbi', ivcC)[0]))
	for ivcV in (0, 1):
		check('cAIVehicleControl_SetInertia(%d)' % ivcV, lambda v=ivcV: (SDTest.Call('cAIVehicleControl_SetInertia', 'tvii', ivcC, v),
			ivcB(ivcC + 0xb1)))
		check('cAIVehicleControl_SetDirect(%d)' % ivcV, lambda v=ivcV: (SDTest.Call('cAIVehicleControl_SetDirect', 'tvii', ivcC, v),
			ivcB(ivcC + 0xb0)))
	check('cAIVehicleControl_SetRotStatic', lambda: (SDTest.Call('cAIVehicleControl_SetRotStatic', 'tviff', ivcC, 0.375, 0.0625),
		ivcF(ivcC + 0xa4, 2)))
	ivcSave2 = ivcI(ivcC + 0x60)
	for ivcV in (6, 4, 3, 0):
		ivcPoke(ivcC + 0x60, ivcV)
		check('cAIVehicleControl_ApplyParams(%d)' % ivcV, lambda: (SDTest.Call('cAIVehicleControl_ApplyParams', 'tvi', ivcC),
			ivcB(ivcCar + 0x71c)))
	ivcPoke(ivcC + 0x60, ivcSave2)
	SDTest.Call('cAIVehicleControl_ApplyParams', 'tvi', ivcC)
	check('cAIVehicleControl_ClearDest', lambda: (SDTest.Call('cAIVehicleControl_ClearDest', 'tvi', ivcC), ivcPts()))
	check('cAIVehicleControl_ResetLife', lambda: (SDTest.Call('cAIVehicleControl_ResetLife', 'tvi', ivcC), ivcCF()))
	check('cAIVehicleControl_Stop', lambda: (SDTest.Call('cAIVehicleControl_Stop', 'tvi', ivcC), ivcCtls()))
	check('cAIVehicleControl_ResetClear', lambda: (SDTest.Call('cAIVehicleControl_ResetClear', 'tvi', ivcC), ivcCF(), ivcPts(), ivcCtls()))
	check('cAIVehicleControl_ClearAll', lambda: (SDTest.Call('cAIVehicleControl_ClearAll', 'tvi', ivcC), ivcPts(), ivcCtls()))

	# the line tests around the car (in the air) and down through the level
	ivcP = ivcPos(ivcCar)
	for ivcD in ((0.0, 0.0, -8000.0), (3000.0, 500.0, 2000.0), (0.0, 0.0, -30000.0), (0.0, 5000.0, 0.0),
			(0.0, -5000.0, 0.0), (0.0, 0.0, 0.0), (0.0, -200000.0, 0.0), (20000.0, -150000.0, 10000.0),
			(1.0e-20, 0.0, 0.0)):
		ivcB2 = ivcAdd(ivcP, ivcD)
		check('cAIVehicleControl_TestCorridor' + fmt((ivcD,)), lambda b=ivcB2: (SDTest.Call('cAIVehicleControl_TestCorridor', 'tbiFFf', ivcC,
			ivcP, b, 123.0)[0], ivcF(ivcC + 0x1b0, 54)))
		check('cAIVehicleControl_TestEndTo' + fmt((ivcD,)), lambda b=ivcB2: (SDTest.Call('cAIVehicleControl_TestEndTo', 'tbiFF', ivcC,
			ivcP, b)[0], ivcF(ivcC + 0x288, 12)))
	for ivcDir, ivcL in (((0.0, 0.0, -1.0), 500.0), ((0.6, 0.0, 0.8), 3000.0), ((0.0, 1.0, 0.0), 0.0),
			((0.0, -1.0, 0.0), 1.0e9), ((0.0, 0.0, 0.0), 100.0), ((0.5, 0.5, 0.70710677), -50.0)):
		check('cAIVehicleControl_TestEnd' + fmt((ivcDir, ivcL)), lambda d=ivcDir, l=ivcL: (SDTest.Call('cAIVehicleControl_TestEnd',
			'tbiFFf', ivcC, ivcP, d, l)[0], ivcF(ivcC + 0x288, 12)))

	# SetMove(2): the direct path to a point ahead in the air; what runs on it
	ivcGoal = ivcAdd(ivcP, (2000.0, 300.0, -9000.0))
	def ivcMove2(boost, d=0):
		return SDTest.Call('cAIVehicleControl_SetMove', 'biifffiiiiiiiii', ivcC, 2, ivcGoal[0], ivcGoal[1], ivcGoal[2],
			ivcD300[0], ivcD300[1], boost, ivcD1[0], ivcD1[1], ivcD2[0], ivcD2[1], ivcD05[0], ivcD05[1])[0]
	check('cAIVehicleControl_SetMove(2)', lambda: (ivcMove2(1), ivcCF(), ivcPts(), ivcI(ivcC + 0xe8) != 0))
	check('cAIVehicleControl_SetMove(2) again', lambda: (ivcMove2(0), ivcCF(), ivcPts()))
	check('cAIVehicleControl path nodes', lambda: (SDTest.Call('cAIVehicleControl_NodeIsFirst', 'tbi', ivcC)[0],
		SDTest.Call('cAIVehicleControl_NodeIsLast', 'tbi', ivcC)[0], SDTest.Call('cAIVehicleControl_NodeOnWay', 'tbi', ivcC)[0], ivcI(ivcC + 0xd4) != 0))
	check('cAIVehicleControl_CheckPathFree', lambda: (SDTest.Call('cAIVehicleControl_CheckPathFree', 'tbi', ivcC)[0], ivcCF()))
	ivcPoke(ivcC + 0x18c, 1)
	check('cAIVehicleControl_CheckPathFree state 1', lambda: (SDTest.Call('cAIVehicleControl_CheckPathFree', 'tbi', ivcC)[0], ivcCF(),
		ivcPts()))
	ivcPoke(ivcC + 0x18c, 0)
	check('cAIVehicleControl_Smooth', lambda: (SDTest.Call('cAIVehicleControl_Smooth', 'tvi', ivcC), ivcPts(), ivcF(ivcC + 0x1b0, 54)))
	# the point search and the update of the path only where the segment to the goal is free (else a graph search)
	ivcClear = SDTest.Call('cAIVehicleControl_TestCorridor', 'tbiFFf', ivcC, ivcP, ivcGoal, 0.0)[0]
	check('corridor to the goal', lambda: ivcClear)
	if ivcClear:
		check('cAIVehicleControl_FindPoint', lambda: (SDTest.Call('cAIVehicleControl_FindPoint', 'tbi', ivcC)[0], ivcCF(), ivcPts(),
			ivcI(ivcC + 0xec)))
	for ivcV in (0, 1):
		SDTest.Call('cAIVehicleControl_SetDirect', 'tvii', ivcC, ivcV)
		ivcSet(ivcC + 0x90, (0.0, 0.0))
		ivcSet(ivcCar + 0xac, (0.25, -0.5, 0.0))
		check('cAIVehicleControl_TurnToPoint direct %d' % ivcV, lambda: (SDTest.Call('cAIVehicleControl_TurnToPoint', 'tvi', ivcC),
			ivcF(ivcC + 0x44, 2), ivcF(ivcC + 0x90, 2), ivcF(ivcC + 0xf0, 2), ivcCtls(), ivcF(ivcCar + 0xac, 3)))
		check('cAIVehicleControl_TurnToPoint again %d' % ivcV, lambda: (SDTest.Call('cAIVehicleControl_TurnToPoint', 'tvi', ivcC),
			ivcF(ivcC + 0x90, 2), ivcCtls()))
	SDTest.Call('cAIVehicleControl_SetDirect', 'tvii', ivcC, 0)
	ivcSet(ivcCar + 0xac, (0.0, 0.0, 0.0))
	check('cAIVehicleControl_StrafeToPoint', lambda: (SDTest.Call('cAIVehicleControl_StrafeToPoint', 'tvi', ivcC), ivcCtls()))
	check('cAIVehicleControl_StrafeToPoint copy', lambda: (SDTest.Call('cAIVehicleControl_StrafeToPoint@93cf0', 'tvi', ivcC), ivcCtls()))
	for ivcBrake in (0, 1, 2):
		for ivcStop in (0, 1):
			ivcPoke(ivcC + 0x54, ivcBrake)
			ivcSetByte(ivcC + 0x108, ivcStop)
			check('cAIVehicleControl_AccelPath' + fmt((ivcBrake, ivcStop)), lambda: (SDTest.Call('cAIVehicleControl_AccelPath', 'tvi',
				ivcC), ivcCtls(), ivcBytes(ivcC, (0xf8,)), ivcF(ivcC + 0x104, 1)))
	ivcPoke(ivcC + 0x54, 1)
	ivcSetByte(ivcC + 0x108, 0)
	# the velocity of the car toward the goal and along the nose
	ivcSet(ivcCar + 0xd4, (100.0, 20.0, -700.0))
	ivcSetByte(ivcC + 0xb1, 0)
	check('cAIVehicleControl_AccelPath moving', lambda: (SDTest.Call('cAIVehicleControl_AccelPath', 'tvi', ivcC), ivcCtls(),
		ivcF(ivcCar + 0xd4, 3)))
	ivcSetByte(ivcC + 0xb1, 1)
	ivcSet(ivcCar + 0xd4, (0.0, 0.0, 0.0))
	# the strafe slots with a fixed srand
	for ivcDt in (0.1, 0.25, 0.5, 3.0):
		ivcSrand(55)
		ivcSet(ivcC + 0x40, (ivcDt,))
		check('cAIVehicleControl_StrafeRandom' + fmt((ivcDt,)), lambda: (SDTest.Call('cAIVehicleControl_StrafeRandom', 'tvi', ivcC),
			ivcF(ivcC + 0x178, 4), ivcCtls()))
	for ivcOn, ivcLife in ((0, 0), (1, 0), (1, -5), (1, 0)):
		ivcSrand(56)
		ivcSetByte(ivcC + 0x192, ivcOn)
		ivcPoke(ivcC + 0xac, ivcI(ivcCar + 0x238) - ivcLife)
		ivcSet(ivcC + 0x40, (0.3,))
		check('cAIVehicleControl_StrafeDodge' + fmt((ivcOn, ivcLife)), lambda: (SDTest.Call('cAIVehicleControl_StrafeDodge', 'tbi', ivcC)[0],
			ivcF(ivcC + 0x178, 4), ivcF(ivcC + 0x194, 2), ivcI(ivcC + 0xac) - ivcI(ivcCar + 0x238), ivcCtls()))
	ivcSetByte(ivcC + 0x192, 0)
	# the arrival: point and goal are the same on the direct path, the events of a frozen AI do nothing
	for ivcDist in (300.0, 3000.0, 30000.0):
		ivcSet(ivcC + 0x38, (ivcDist,))
		check('cAIVehicleControl_CheckArrive' + fmt((ivcDist,)), lambda: (SDTest.Call('cAIVehicleControl_CheckArrive', 'tvi', ivcC),
			ivcInts(ivcAI, (0x19c, 0x1a4)), ivcCF()))
	ivcSet(ivcC + 0x38, (300.0,))
	for ivcDt in (0.05, 0.3):
		ivcSrand(57)
		check('cAIVehicleControl_Update(2)' + fmt((ivcDt,)), lambda d=ivcDt: (SDTest.Call('cAIVehicleControl_Update', 'tvif', ivcC, d),
			ivcCF(), ivcPts(), ivcCtls(), ivcInts(ivcAI, (0x19c, 0x1a4))))
		check('cAIVehicleControl_UpdateDirect' + fmt((ivcDt,)), lambda d=ivcDt: (SDTest.Call('cAIVehicleControl_UpdateDirect', 'tbif', ivcC,
			d)[0], ivcCtls()))
		if ivcClear:
			ivcSrand(58)
			check('cAIVehicleControl_UpdatePoint' + fmt((ivcDt,)), lambda d=ivcDt: (SDTest.Call('cAIVehicleControl_UpdatePoint', 'tbif', ivcC,
				d)[0], ivcCF(), ivcPts(), ivcCtls()))
	check('cAIVehicleControl_Draw', lambda: SDTest.Call('cAIVehicleControl_Draw', 'tvi', ivcC))
	# the random goals
	ivcSrand(59)
	check('cAIVehicleControl_RandomDest', lambda: (SDTest.Call('cAIVehicleControl_RandomDest', 'tviF', ivcC, (0.0,) * 3)[1], ivcPts()))
	ivcMove2(0)
	ivcSrand(60)
	check('cAIVehicleControl_RandomNear', lambda: (SDTest.Call('cAIVehicleControl_RandomNear', 'tviF', ivcC, ivcAdd(ivcGoal, (0.0, 0.0,
		500.0)))[1], ivcI(ivcC + 0xd4) != 0))
	check('cAIVehicleControl_RandomNear goal', lambda: SDTest.Call('cAIVehicleControl_RandomNear', 'tviF', ivcC, ivcF(ivcC + 0x7c, 3))[1])
	check('cAIVehicleControl_SetMove(0)', lambda: (SDTest.Call('cAIVehicleControl_SetMove', 'bii', ivcC, 0)[0], ivcCF(), ivcPts(),
		ivcCtls()))

	# the target SDIvcCar2 (the node of the AI is left alone, only its target is set)
	if ivcCar2Active:
		ivcPoke(ivcAI + 0x40, ivcCar2)
		check('cAIVehicleControl_SetMove(6)', lambda: (SDTest.Call('cAIVehicleControl_SetMove', 'bii', ivcC, 6)[0],
			ivcI(ivcC + 0x15c) == ivcAI + 0x38, ivcCF()))
		check('cAIVehicleControl_CheckTarget', lambda: SDTest.Call('cAIVehicleControl_CheckTarget', 'tbi', ivcC)[0])
		for ivcDt in (0.1, 0.4):
			ivcSet(ivcC + 0x40, (ivcDt,))
			check('cAIVehicleControl_ChaseLine' + fmt((ivcDt,)), lambda: (SDTest.Call('cAIVehicleControl_ChaseLine', 'tbi', ivcC)[0],
				ivcCF(), ivcPts()))
		ivcSet(ivcC + 0x90, (0.0, 0.0))
		check('cAIVehicleControl_TurnToTarget', lambda: (SDTest.Call('cAIVehicleControl_TurnToTarget', 'tvi', ivcC), ivcF(ivcC + 0x44, 2),
			ivcF(ivcC + 0x90, 2), ivcCtls()))
		check('cAIVehicleControl_UpdateOrient', lambda: (SDTest.Call('cAIVehicleControl_UpdateOrient', 'tbif', ivcC, 0.1)[0], ivcCtls()))
		check('cAIVehicleControl_Update(6)', lambda: (SDTest.Call('cAIVehicleControl_Update', 'tvif', ivcC, 0.2), ivcCF(), ivcCtls()))
		for ivcMin, ivcMax in ((5000.0, 20000.0), (1000.0, 4000.0), (20000.0, 40000.0), (8000.0, 8000.0)):
			for ivcHy in (0, 1):
				ivcSet(ivcC + 0x160, (ivcMax, ivcMin))
				ivcSetByte(ivcC + 0x188, ivcHy)
				ivcSetByte(ivcC + 0x189, ivcHy)
				check('cAIVehicleControl_AccelChase' + fmt((ivcMin, ivcMax, ivcHy)), lambda: (SDTest.Call('cAIVehicleControl_AccelChase',
					'tvi', ivcC), ivcCtls(), ivcBytes(ivcC, (0xf8, 0x188, 0x189))))
				ivcSrand(61)
				ivcSet(ivcC + 0x40, (0.5,))
				check('cAIVehicleControl_StrafeChase' + fmt((ivcMin, ivcMax, ivcHy)), lambda: (SDTest.Call('cAIVehicleControl_StrafeChase',
					'tvi', ivcC), ivcF(ivcC + 0x178, 4), ivcCtls()))
				check('cAIVehicleControl_AccelToTarget' + fmt((ivcMin, ivcMax, ivcHy)), lambda: (SDTest.Call('cAIVehicleControl_AccelToTarget',
					'tvi', ivcC), ivcCtls(), ivcF(ivcC + 0xfc, 1)))
		ivcSetByte(ivcC + 0x188, 0)
		ivcSetByte(ivcC + 0x189, 0)
		check('cAIVehicleControl_SetOrient', lambda: (SDTest.Call('cAIVehicleControl_SetOrient', 'tbii', ivcC, ivcAI + 0x38)[0], ivcCF()))
		check('cAIVehicleControl_SetMove(0) after', lambda: (SDTest.Call('cAIVehicleControl_SetMove', 'bii', ivcC, 0)[0], ivcCF()))

	# ---- the shoot control ----
	def ivcSF(a=0):
		return (ivcInts(ivcS, (4, 8, 0x100, 0x118, 0x124, 0x128, 0x17c)), ivcF(ivcS + 0x40, 1), ivcF(ivcS + 0x164, 4),
			ivcBytes(ivcS, (0x178, 0x180)), ivcI(ivcS + 0x174) == ivcAI + 0x38)
	def ivcWeaps(a=0):
		r = ()
		for k in range(7):
			e = ivcS + 0x4c + 0x18 * k
			if ivcB(e + 0x15):
				r = r + ((ivcI(e + 0xc), 1, ivcI(e), ivcI(e + 0x10)),)
			else:
				r = r + ((ivcI(e + 0xc), 0),)
		return r
	check('cAIVehicleShoot after ctor', lambda: (ivcSF(), ivcWeaps(), ivcInts(ivcS, (0x12c, 0x130, 0x13c, 0x140, 0x144,
		0x148, 0x14c)), ivcF(ivcS + 0x134, 2)))
	ivcM3 = ivcNew(0x184)
	ivcS2 = SDTest.Call('cAIVehicleShoot_ctor', 'tiiiii', ivcM3, ivcCar, ivcAI, ivcP3)[0]
	check('cAIVehicleShoot_ctor', lambda: (ivcS2 == ivcM3, ivcI(ivcS2) - 0x400000, ivcInts(ivcS2, (4, 8, 0x100,
		0x118, 0x124, 0x128, 0x174, 0x17c)), ivcI(ivcS2 + 0x17c) == ivcI(ivcCar + 0x238), ivcB(ivcS2 + 0x180),
		ivcF(ivcS2 + 0x164, 4), ivcF(ivcS2 + 0x40, 1), ivcF(ivcS2 + 0x14, 3), ivcF(ivcS2 + 0x34, 3)))
	check('cAIVehicleShoot_Delete', lambda: SDTest.Call('cAIVehicleShoot_Delete', 'tiii', ivcS2, 1)[0] == ivcS2)
	check('cAIVehicleShoot getters', lambda: (SDTest.Call('cAIVehicleShoot_GetInfo', 'tvippp', ivcS, 0, 0, 0)[1:],
		SDTest.Call('cAIVehicleShoot_GetParams', 'tii', ivcS)[0] == ivcS + 0x12c, SDTest.Call('cAIVehicleShoot_GetParamsInit', 'tii', ivcS)[0] == ivcS + 0x148,
		SDTest.Call('cAIVehicleShoot_GetWeaps', 'tii', ivcS)[0] == ivcS + 0x4c, SDTest.Call('cAIVehicleShoot_HasHook', 'tbi', ivcS)[0]))
	ivcIds = ()
	for ivcA in (sym('g_LaserIDWeap'), sym('g_VulcanIDWeap'), sym('g_DevastatorIDWeap'), sym('g_ATPCIDWeap'), sym('g_TeslaIDWeap'), sym('g_SwarmIDWeap'), sym('g_InfernoIDWeap')):
		ivcIds = ivcIds + (SDTest.Peek(ivcA, 'i'),)
	check('weapon ids', lambda: ivcIds)
	for ivcW in ivcIds + (-1, 99):
		check('cAIVehicleShoot_WeaponName(%d)' % ivcW, lambda w=ivcW: SDTest.Call('cAIVehicleShoot_WeaponName', 'tviib', 0, w, '')[1])
	# the weapons of the car for the test: slots, the upgraded vulcan, ammo of every kind
	ivcWm = ivcCar + 0x5f0
	ivcWs = ivcI(ivcWm + 0x18)
	ivcAmmo = ivcI(ivcWm + 0x20)
	ivcNW = ivcI(SDTest.Peek('g_pWorld', 'i') + 0x1bb8)
	ivcNA = ivcI(SDTest.Peek('g_pWorld', 'i') + 0x1bc4)
	check('weapon tables', lambda: (ivcWs != 0, ivcAmmo != 0, ivcNW, ivcNA))
	ivcOk = ivcWs and ivcAmmo and ivcNA >= 4
	for ivcW in ivcIds:
		if ivcW < 0 or ivcW >= ivcNW:
			ivcOk = 0
	if ivcOk:
		# the target first: the inferno sums measure the distance to it
		ivcPoke(ivcAI + 0x40, ivcCar2)
		ivcPoke(ivcS + 0x174, ivcAI + 0x38)
		ivcWsSave = ivcSave(ivcWs, 3 * ivcNW)
		ivcAmmoSave = ivcSave(ivcAmmo, ivcNA)
		def ivcAmmos(a=0):
			return ivcF(ivcAmmo, 4)
		def ivcGive(slots, upg):
			for k in range(7):
				ivcPoke(ivcWs + 12 * ivcIds[k], slots[k])
				ivcPoke(ivcWs + 12 * ivcIds[k] + 4, upg[k])
			SDTest.Call('cAIVehicleShoot_InitWeapons', 'tvi', ivcS)
		def ivcAmmoSums(a=0):
			r = ()
			for d in (100.0, 50000.0):
				r = r + (SDTest.Call('cAIVehicleShoot_AmmoAt', 'tfif', ivcS, d)[0], SDTest.Call('cAIVehicleShoot_MaxAmmoAt', 'tfif', ivcS, d)[0])
			for k in range(3):
				for d in (100.0, 50000.0):
					r = r + (SDTest.Call('cAIVehicleShoot_UsesAmmo', 'tbiif', ivcS, k, d)[0],)
			for f in (0.0, 0.25, 0.9, 2.0):
				r = r + (SDTest.Call('cAIVehicleShoot_HasAmmo', 'tbif', ivcS, f)[0],)
			r = r + (SDTest.Call('cAIVehicleShoot_MaxAmmo', 'tfi', ivcS)[0], SDTest.Call('cAIVehicleShoot_TotalAmmo', 'tfi', ivcS)[0])
			for w in ivcIds + (-1,):
				r = r + (SDTest.Call('cAIVehicleShoot_WeapAmmo', 'tfii', ivcS, w)[0],)
			for d in (100.0, 10000.0, 100000.0):
				r = r + (SDTest.Call('cAIVehicleShoot_InfernoReady', 'tbif', ivcS, d)[0],)
			return r
		for ivcSl, ivcUp, ivcAm in (((0, 0, 0, 0, 0, 0, 0), (0, 0, 0, 0, 0, 0, 0), (0.0, 0.0, 0.0, 0.0)),
				((1, 1, 0, 0, 1, 0, 0), (0, 2, 0, 0, 0, 0, 0), (50.0, 0.0, 120.0, 30.0)),
				((1, 1, 1, 1, 1, 1, 1), (1, 1, 0, 2, 0, 0, 1), (300.0, 200.0, 0.0, 150.0)),
				((0, 0, 3, 1, 0, 1, 1), (0, 0, 1, 1, 0, 0, 0), (10.0, 500.0, 40.0, 0.5))):
			ivcSet(ivcAmmo, ivcAm)
			check('cAIVehicleShoot_InitWeapons' + fmt((ivcSl, ivcUp)), lambda s=ivcSl, u=ivcUp: (ivcGive(s, u),
				ivcWeaps()))
			check('cAIVehicleShoot ammo' + fmt((ivcSl, ivcUp, ivcAm)), lambda: ivcAmmoSums())
			if ivcCar2Active:
				ivcPoke(ivcAI + 0x40, ivcCar2)
				ivcPoke(ivcS + 0x174, ivcAI + 0x38)
				# the kind of the vulcan is read also when the car does not have it (then the original leaves a
				# value of the stack there): only with the vulcan
				ivcRange = ()
				for ivcW in ivcIds + (-1,):
					if ivcW != ivcIds[1] or ivcB(ivcS + 0x79):
						ivcRange = ivcRange + (SDTest.Call('cAIVehicleShoot_InRange', 'tbii', ivcS, ivcW)[0],)
				check('cAIVehicleShoot_InRange' + fmt((ivcSl,)), lambda r=ivcRange: r)
				for ivcSeed in (1, 2, 3):
					ivcSrand(ivcSeed)
					check('cAIVehicleShoot_TargetInSight' + fmt((ivcSl, ivcSeed)), lambda: (SDTest.Call('cAIVehicleShoot_TargetInSight',
						'tbii', ivcS, 1)[0], ivcI(ivcS + 0x100), ivcI(ivcCtl)))
					ivcSrand(ivcSeed)
					check('cAIVehicleShoot_SelectCM' + fmt((ivcSl, ivcSeed)), lambda: (SDTest.Call('cAIVehicleShoot_SelectCM', 'tvi',
						ivcS), ivcI(ivcS + 0x118), ivcI(ivcCtl)))
				check('cAIVehicleShoot_TargetInSight 0', lambda: (SDTest.Call('cAIVehicleShoot_TargetInSight', 'tbii', ivcS, 0)[0],
					ivcI(ivcS + 0x100)))
				for ivcA, ivcB2, ivcCm in ((1, 1, 0), (1, 2, 1), (0, 2, 0), (1, 0, 0), (2, 3, 1), (0, 0, 0)):
					ivcSrand(ivcA + 4 * ivcB2)
					check('cAIVehicleShoot_SetMode' + fmt((ivcSl, ivcA, ivcB2, ivcCm)), lambda a=ivcA, b=ivcB2,
						c=ivcCm: (SDTest.Call('cAIVehicleShoot_SetMode', 'tbiiii', ivcS, a, b, c)[0], ivcSF(), ivcCtls()))
					for ivcIn in (0, 1):
						ivcSetByte(ivcC + 0x174, ivcIn)
						for ivcDt in (0.25, 0.6):
							ivcSrand(9)
							check('cAIVehicleShoot_Update' + fmt((ivcIn, ivcDt)), lambda d=ivcDt: (SDTest.Call('cAIVehicleShoot_Update',
								'tvif', ivcS, d), ivcSF(), ivcCtls()))
							ivcSrand(10)
							check('cAIVehicleShoot_Update2' + fmt((ivcIn, ivcDt)), lambda d=ivcDt: (SDTest.Call(
								'cAIVehicleShoot_Update2', 'tvif', ivcS, d), ivcSF(), ivcCtls()))
				ivcSetByte(ivcC + 0x174, 0)
				ivcPoke(ivcS + 0x17c, ivcI(ivcCar + 0x238) + 10)
				ivcSetByte(ivcS + 0x180, 1)
				check('cAIVehicleShoot_PullFire', lambda: (SDTest.Call('cAIVehicleShoot_PullFire', 'tvi', ivcS), ivcCtls(), ivcSF()))
				check('cAIVehicleShoot_CheckTarget', lambda: (SDTest.Call('cAIVehicleShoot_CheckTarget', 'tbi', ivcS)[0],
					SDTest.Call('cAIVehicleShoot_CheckTargetCM', 'tbi', ivcS)[0]))
				ivcSrand(12)
				check('cAIVehicleShoot_WeapListChanged', lambda: (SDTest.Call('cAIVehicleShoot_WeapListChanged', 'tvi', ivcS), ivcSF(),
					ivcWeaps(), ivcI(ivcCtl)))
				SDTest.Call('cAIVehicleShoot_SetMode', 'tbiiii', ivcS, 0, 0, 0)
		ivcRestore(ivcWs, ivcWsSave)
		ivcRestore(ivcAmmo, ivcAmmoSave)
		SDTest.Call('cAIVehicleShoot_InitWeapons', 'tvi', ivcS)
	check('cAIVehicleShoot_Draw', lambda: SDTest.Call('cAIVehicleShoot_Draw', 'tvi', ivcS))

	# ---- the vehicle group ----
	ivcNE = ivcI(ivcGroup + 0xec)
	check('cAIVehicleGroup elevators', lambda: (ivcNE, ivcF(ivcGroup + 0x74, 3 * ivcNE)))
	check('cAIVehicleGroup_FindElevators', lambda: (SDTest.Call('cAIVehicleGroup_FindElevators', 'tvi', ivcGroup), ivcI(ivcGroup + 0xec),
		ivcF(ivcGroup + 0x74, 3 * ivcI(ivcGroup + 0xec))))
	ivcTests = ((0.0, 0.0, 0.0), (56000.0, 50000.0, 60000.0))
	for ivcK in range(ivcI(ivcGroup + 0xec)):
		ivcE = ivcF(ivcGroup + 0x74 + 12 * ivcK, 3)
		ivcTests = ivcTests + (ivcE, ivcAdd(ivcE, (300.0, 0.0, 0.0)), ivcAdd(ivcE, (0.0, -1000.0, 2000.0)))
	for ivcT in ivcTests:
		for ivcD in (0.0, 299.0, 500.0, 5000.0):
			check('cAIVehicleGroup_ElevatorNear' + fmt((ivcT, ivcD)), lambda t=ivcT, d=ivcD: SDTest.Call('cAIVehicleGroup_ElevatorNear',
				'tbiFf', ivcGroup, t, d)[0])
	check('cAIVehicleGroup_ResetRepos', lambda: (SDTest.Call('cAIVehicleGroup_ResetRepos', 'tvi', ivcGroup),
		SDTest.Call('cAIVehicleGroup_ReposReady', 'tbi', ivcGroup)[0]))
	check('cAIVehicleGroup_Draw', lambda: SDTest.Call('cAIVehicleGroup_Draw', 'tvi', ivcGroup))
	ivcM4 = ivcNew(0xf4)
	ivcG2 = SDTest.Call('cAIVehicleGroup_ctor', 'tii', ivcM4)[0]
	check('cAIVehicleGroup_ctor', lambda: (ivcG2 == ivcM4, ivcI(ivcG2) - 0x400000, ivcInts(ivcG2, (0x54, 0x60, 0x64,
		0x68, 0x6c, 0x70, 0xec)), ivcB(ivcG2 + 0x58), ivcF(ivcG2 + 0x5c, 1), ivcF(ivcG2 + 0x74, 3 * ivcI(ivcG2 + 0xec)),
		SDTest.Call('cAIVehicleGroup_ReposReady', 'tbi', ivcG2)[0]))
	check('cAIVehicleGroup_Delete', lambda: SDTest.Call('cAIVehicleGroup_Delete', 'tiii', ivcG2, 1)[0] == ivcG2)

	# away again
	ivcPoke(ivcAI + 0x40, 0)
	ivcPoke(ivcS + 0x174, 0)
	check('cAIVehicle_Delete', lambda: SDTest.Call('cAIVehicle_Delete', 'tiii', ivcAI, 1)[0] == ivcAI)
	ivcFree(ivcP1)
	ivcFree(ivcP2)
	ivcFree(ivcP3)
	ivcRestore(ivcGroup + 0x5c, ivcGroupSave[0:6])
	ivcPoke(ivcGroup + 0xf0, ivcGroupSave[6])
	ivcRestore(ivcCar + 0xac, ivcCarSave[0:3])
	ivcRestore(ivcCar + 0xd4, ivcCarSave[3:9])
	ivcPoke(ivcCar + 0x46c, ivcCarSave[9])
	ivcRestore(ivcCtl, ivcCtlSave)
if ivcCarObj:
	check('SDIvcCar Del', lambda: ivcCarObj.Del())
if ivcCar2Obj:
	check('SDIvcCar2 Del', lambda: ivcCar2Obj.Del())
