# game AI/IVehicle.c (execfile'd by sdlevel.py, Python 1.5.2): the vehicle AI. Pure parts first (the plane cut of a
# segment, the state names, the group states), then an AI of our own: a Car "SDIvhCar" far away with the model SPoli1,
# its cAIVehicle made by the constructor of the exe with valid parameter blocks after a fixed srand, and a second car
# "SDIvhCar2" ahead of it as hook target. On it the getters, the state mapping of GetState, the path width on the
# traffic graph of FZ, IsMoving, the damage clock of Think, the update slots in FREEZE, the script variables, the
# hook range tests, the turn speeds, the track kind, the kept state, the parameter checks, the events and the changes
# to the free states (FREEZE, FREEZE_OBJ_NOT_ACCESS, FREEZE_LOST, FREEZE_CHANGE_STATE), then the destructor. The AIs
# of the level are not touched; the counters and the change timer of the vehicle group that the state changes move
# are restored at the end. Not called: the states that move the vehicle (their decisions are still the exe's), the
# python callbacks (CheckTarget only when OnObjectiveNullAI has no function), the indoor traffic (FZ is outdoor) and
# the item searches print only whether they agree with each other (the items respawn with the AIs of the level).
def ivhI(a):
	return SDTest.Peek(a, '@i')
def ivhB(a):
	return SDTest.Peek(a, '@b')
def ivhF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def ivhSet(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def ivhPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def ivhSetByte(a, v):
	w = ivhI(a & ~3)
	sh = 8 * (a & 3)
	ivhPoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def ivhAlloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def ivhFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def ivhSrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def ivhInts(a, offs):
	r = ()
	for o in offs:
		r = r + (ivhI(a + o),)
	return r
def ivhFind(name):
	w = SDTest.Peek('g_pWorld', 'i')
	slot = SDTest.Call('HashTable_Slot', 'tiis', w + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', w + 4, name, slot)[0]
	if node:
		return ivhI(node)
	return 0
def ivhVt(e, off):
	return ivhI(ivhI(e) + off) - 0x400000
def ivhPos(e):
	return SDTest.Call(ivhVt(e, 0x18), 'tiiF', e, (0.0, 0.0, 0.0))[1]
def ivhAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])

# ---- the pure parts ----
ivhPlanes = ((0.0, 0.0, 1.0, 0.0), (0.0, 1.0, 0.0, -10.0), (0.6, 0.0, 0.8, 2.5), (1.0, 2.0, -2.0, 0.0))
ivhSegs = (((1.0, 2.0, -3.0), (4.0, 5.0, 6.0)), ((1.0, 1.0, 0.0), (1.0, 1.0, 5.0)), ((1.0, 1.0, 0.0), (1.0, 1.0, -5.0)),
	((0.0, 20.0, 0.0), (0.0, 0.0, 0.0)), ((-7.5, 3.25, 9.0), (12.0, -40.0, -1.5)), ((2.0, 2.0, 2.0), (3.0, 3.0, 3.0)))
for ivhPl in ivhPlanes:
	for ivhSg in ivhSegs:
		check('SegmentPlane_Hit' + fmt((ivhSg[0], ivhSg[1], ivhPl)), lambda s=ivhSg, p=ivhPl: SDTest.Call('SegmentPlane_Hit',
			'iFFFF', (0.0, 0.0, 0.0), s[0], s[1], p)[0:2])
# stdcall: called as thiscall with a dummy this
for ivhS in range(-1, 19):
	check('cAIVehicle_StateName(%d)' % ivhS, lambda s=ivhS: SDTest.Call('cAIVehicle_StateName', 'tviib', 0, s, 'keep')[1])
	check('cAIVehicle_IsGroupState(%d)' % ivhS, lambda s=ivhS: SDTest.Call('cAIVehicle_IsGroupState', 'tbii', 0, s)[0])
for ivhS in range(-1, 13):
	check('cAIVehicle_InternStateName(%d)' % ivhS, lambda s=ivhS: SDTest.Call('cAIVehicle_InternStateName', 'tviib', 0, s, 'keep')[1])

# ---- an AI of our own ----
ivhGroup = SDTest.Peek('g_VehicleGroup', 'i')
ivhTraffic = SDTest.Peek('g_NodesTraffic', 'i')
ivhNodesWorld = SDTest.Peek('g_NodesWorld', 'i')
if SDTest.Peek('g_IsMapOutDoor', 'i'):
	ivhPickT = ivhTraffic
else:
	ivhPickT = SDTest.Peek('g_NodesTrafficInDoor', 'i')
ivhCarObj = Scrap.CreateEntity('SDIvhCar', 54000.0, 50000.0, 60000.0, 'Car')
ivhCar = ivhFind('SDIvhCar')
ivhCar2Obj = Scrap.CreateEntity('SDIvhCar2', 54000.0, 50000.0, 55000.0, 'Car')
ivhCar2 = ivhFind('SDIvhCar2')
check('SDIvhCar', lambda: (ivhCarObj is not None, ivhCar != 0, ivhCar2 != 0, ivhGroup != 0, ivhTraffic != 0,
	ivhNodesWorld != 0))
if ivhCar and ivhCar2 and ivhGroup:
	for ivhC in (ivhCar, ivhCar2):
		SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', ivhC, 'SPoli1', 1.0, 1.0, 1.0, 1)
		ivhSet(ivhC + 0xac, (0.0, 0.0, 0.0))
	ivhGroupSave = ivhInts(ivhGroup, (0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70))
	ivhCarSave = ivhInts(ivhCar, (0x420, 0x424, 0x428, 0x5e8))
	# the parameter blocks, all values in their ranges (a value out of them is a CHECK)
	ivhP1 = ivhAlloc(0x2c)
	ivhP2 = ivhAlloc(0x18)
	ivhP3 = ivhAlloc(0x1c)
	ivhSet(ivhP1, (0.5, 0.25, 0.75))
	ivhPoke(ivhP1 + 0x10, 1)
	ivhPoke(ivhP1 + 0x1c, 3)
	ivhPoke(ivhP1 + 0x24, 40)
	ivhPoke(ivhP1 + 0x28, 60)
	ivhPoke(ivhP2 + 4, 2)
	ivhPoke(ivhP2 + 0xc, 4)
	ivhSet(ivhP2 + 0x10, (0.5,))
	ivhPoke(ivhP2 + 0x14, 1)
	ivhPoke(ivhP3, 50)
	ivhPoke(ivhP3 + 4, 3)
	ivhSet(ivhP3 + 8, (1.0, 0.5))
	ivhPoke(ivhP3 + 0x10, 35)
	ivhPoke(ivhP3 + 0x14, 50)
	ivhPoke(ivhP3 + 0x18, 15)
	ivhSrand(4321)
	ivhMem = SDTest.Call('operator_new', 'ii', 0x3b0)[0]
	ivhAI = SDTest.Call('cAIVehicle_ctor', 'tiiiiii', ivhMem, ivhCar, ivhP1, ivhP2, ivhP3)[0]
	ivhCtrl = ivhI(ivhAI + 0x194)
	ivhShoot = ivhI(ivhAI + 0x198)
	def ivhState(a=0):
		return ivhInts(ivhAI, (0x19c, 0x1a0, 0x1a4, 0x90))
	check('cAIVehicle_ctor', lambda: (ivhAI == ivhMem, ivhI(ivhAI) - 0x400000, ivhI(ivhAI + 0x190) == ivhCar,
		ivhCtrl != 0, ivhShoot != 0, ivhState(), ivhF(ivhAI + 0x1b4, 3), ivhInts(ivhAI, (0x1c0, 0x1c4, 0x1c8, 0x1cc,
		0x1d0, 0x1d4, 0x1d8, 0x1dc)), ivhF(ivhAI + 0x1e0, 3), ivhInts(ivhAI, (0x1ec, 0x1f0, 0x1fc, 0x208))))
	check('cAIVehicle_ctor fields', lambda: (ivhInts(ivhAI, (0x1ac, 0x2d8, 0x2dc, 0x31c, 0x338, 0x33c, 0x350, 0x3ac,
		0x15c)), ivhF(ivhAI + 0x2e4, 3), ivhF(ivhAI + 0x2f0, 10), ivhF(ivhAI + 0x320, 3), ivhB(ivhAI + 0x1a8),
		ivhB(ivhAI + 0x2e0), ivhB(ivhAI + 0x318), ivhB(ivhAI + 0x32c), ivhB(ivhAI + 0x340), ivhB(ivhAI + 0x354),
		ivhB(ivhAI + 0x3a8), ivhB(ivhAI + 0x3a9), ivhB(ivhAI + 0x3aa), ivhF(ivhAI + 0x1b0, 1), ivhF(ivhAI + 0x330, 1)))
	check('control and shoot params', lambda: (ivhInts(ivhCtrl, (0x4c, 0x50, 0x54, 0x58, 0x60, 0x64, 0x68, 0x78)),
		ivhF(ivhCtrl + 0x5c, 1), ivhInts(ivhShoot, (0x12c, 0x130, 0x13c, 0x140, 0x144)), ivhF(ivhShoot + 0x134, 2),
		ivhB(ivhCar + 0x71c)))

	# the getters
	check('cAIVehicle_GetVehicle', lambda: SDTest.Call('cAIVehicle_GetVehicle', 'tii', ivhAI)[0] == ivhCar)
	check('cAIVehicle_GetControl', lambda: SDTest.Call('cAIVehicle_GetControl', 'tii', ivhAI)[0] == ivhCtrl)
	for ivhRva, ivhName in ((sym('cAIVehicle_GetState'), 'GetState'), (sym('cAIVehicle_GetPrevState'), 'GetPrevState'), (sym('cAIVehicle_GetInternState'), 'GetInternState'),
			(sym('cAIVehicle_GetTrackType'), 'GetTrackType')):
		check('cAIVehicle_' + ivhName, lambda r=ivhRva: SDTest.Call(r, 'tii', ivhAI)[0])
	check('cAIVehicle_GetChanged', lambda: SDTest.Call('cAIVehicle_GetChanged', 'tbi', ivhAI)[0])
	for ivhEv in (0, 2, 11, 12, 13):
		check('cAIVehicle_GetStateFor(%d)' % ivhEv, lambda e=ivhEv: SDTest.Call('cAIVehicle_GetStateFor', 'tiii', ivhAI, e)[0])
	check('cAIVehicle_GetFadeDists', lambda: SDTest.Call('cAIVehicle_GetFadeDists', 'tviFF', ivhAI, (0.0,), (0.0,))[1:])
	for ivhRva, ivhWant in ((sym('cAIVehicle_GetParams1'), ivhAI + 0x1b4), (sym('cAIVehicle_GetParamsInit'), ivhAI + 0x1e0), (sym('cAIVehicle_GetParams2'), ivhCtrl + 0x4c),
			(sym('cAIVehicle_GetParams2Init'), ivhCtrl + 0x64), (sym('cAIVehicle_GetParams3'), ivhShoot + 0x12c), (sym('cAIVehicle_GetParams3Init'), ivhShoot + 0x148)):
		check('cAIVehicle_GetParams %x' % ivhRva, lambda r=ivhRva, w=ivhWant: SDTest.Call(r, 'tii', ivhAI)[0] == w)
	check('cAIVehicle_GetStateName', lambda: SDTest.Call('cAIVehicle_GetStateName', 'tvib', ivhAI, 'x')[1])
	check('cAIVehicle_GetInternStateName', lambda: SDTest.Call('cAIVehicle_GetInternStateName', 'tvib', ivhAI, 'x')[1])
	check('cAIVehicle_ControlFlag', lambda: (SDTest.Call('cAIVehicle_ControlFlag', 'tbi', ivhAI)[0],
		SDTest.Call('cAIVehicle_ControlFlag2', 'tbi', ivhAI)[0]))

	# GetState for every state (LOST_PATH by the kept state)
	ivhSave = ivhInts(ivhAI, (0x19c, 0x1a4, 0x20c, 0x2d8))
	for ivhS in range(0, 18):
		ivhPoke(ivhAI + 0x19c, ivhS)
		check('cAIVehicle_GetState state %d' % ivhS, lambda: SDTest.Call('cAIVehicle_GetState', 'tii', ivhAI)[0])
	for ivhS in (12, 2, 0):
		ivhPoke(ivhAI + 0x19c, 13)
		ivhPoke(ivhAI + 0x20c, ivhS)
		check('cAIVehicle_GetState LOST_PATH saved %d' % ivhS, lambda: SDTest.Call('cAIVehicle_GetState', 'tii', ivhAI)[0])
	# the width of the traffic path (tracks of the graph of FZ)
	if ivhTraffic and ivhI(ivhTraffic + 0x24) >= 2:
		for ivhS, ivhIn, ivhT in ((4, 1, 0), (9, 3, 1), (4, 0, 0), (3, 1, 0), (4, 1, -1), (9, 2, 0), (0, 1, 1)):
			ivhPoke(ivhAI + 0x19c, ivhS)
			ivhPoke(ivhAI + 0x1a4, ivhIn)
			ivhPoke(ivhAI + 0x2d8, ivhT)
			check('cAIVehicle_GetPathWidth' + fmt((ivhS, ivhIn, ivhT)), lambda: SDTest.Call('cAIVehicle_GetPathWidth', 'tfi', ivhAI)[0])
	# IsMoving: the states and the controls of the vehicle
	for ivhS, ivhIn in ((2, 1), (2, 0), (2, 10), (2, 11), (0, 1), (8, 3), (11, 1), (15, 1), (16, 1), (17, 1), (3, 4),
			(12, 9)):
		for ivhCtl in ((0.0, 0.0, 0.0), (0.5, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, 1e-30)):
			ivhPoke(ivhAI + 0x19c, ivhS)
			ivhPoke(ivhAI + 0x1a4, ivhIn)
			ivhSet(ivhCar + 0x420, ivhCtl)
			check('cAIVehicle_IsMoving' + fmt((ivhS, ivhIn, ivhCtl)), lambda: SDTest.Call('cAIVehicle_IsMoving', 'tbi', ivhAI)[0])
	for ivhK in range(3):
		ivhPoke(ivhCar + 0x420 + 4 * ivhK, ivhCarSave[ivhK])
	ivhPoke(ivhAI + 0x19c, ivhSave[0])
	ivhPoke(ivhAI + 0x1a4, ivhSave[1])
	ivhPoke(ivhAI + 0x20c, ivhSave[2])
	ivhPoke(ivhAI + 0x2d8, ivhSave[3])

	# the damage clock of Think in states without a move of their own
	for ivhS, ivhDt, ivhDmg, ivhCh in ((0, 1.0, 1.5, 0), (0, 1.0, 2.5, 0), (4, 0.25, 2.75, 1), (9, 3.5, 0.0, 0),
			(11, 0.0, 3.0, 1), (15, 0.5, -1.0, 0)):
		ivhPoke(ivhAI + 0x19c, ivhS)
		ivhSet(ivhAI + 0xc, (ivhDt,))
		ivhSet(ivhAI + 0x1b0, (ivhDmg,))
		ivhPoke(ivhAI + 0x1ac, 77)
		ivhSetByte(ivhAI + 0x28, ivhCh)
		check('cAIVehicle_Think' + fmt((ivhS, ivhDt, ivhDmg, ivhCh)), lambda: (SDTest.Call('cAIVehicle_Think', 'tbi', ivhAI)[0],
			ivhF(ivhAI + 0x1b0, 1), ivhI(ivhAI + 0x1ac)))
		check('cAIVehicle_React' + fmt((ivhS,)), lambda: SDTest.Call('cAIVehicle_React', 'tvi', ivhAI))
		check('cAIVehicle_CheckItems' + fmt((ivhS,)), lambda: SDTest.Call('cAIVehicle_CheckItems', 'tvi', ivhAI))
	ivhSetByte(ivhAI + 0x28, 0)
	ivhPoke(ivhAI + 0x19c, ivhSave[0])
	# the update slots in FREEZE: the reaction slot and the graph slot (camera in the graph or not, no case)
	for ivhDt in (0.1, 0.15, 0.6, 1.25):
		ivhSet(ivhAI + 0xc, (ivhDt,))
		check('cAIVehicle_UpdateThink' + fmt((ivhDt,)), lambda: (SDTest.Call('cAIVehicle_UpdateThink', 'tvi', ivhAI),
			ivhF(ivhAI + 0x14, 2)))
		if ivhNodesWorld:
			check('cAIVehicle_UpdateGraph' + fmt((ivhDt,)), lambda: (SDTest.Call('cAIVehicle_UpdateGraph', 'tvi', ivhAI),
				ivhF(ivhAI + 0x174, 2)))
	check('cAIVehicle_Update FREEZE', lambda: (SDTest.Call('cAIVehicle_Update', 'tvif', ivhAI, 0.05), ivhState()))
	# UpdateRoute without Repos and in a state it leaves alone
	for ivhRep, ivhIn in ((0, 0), (1, 1), (1, 9)):
		ivhSetByte(ivhAI + 0x86, ivhRep)
		ivhPoke(ivhAI + 0x1a4, ivhIn)
		check('cAIVehicle_UpdateRoute' + fmt((ivhRep, ivhIn)), lambda: (SDTest.Call('cAIVehicle_UpdateRoute', 'tvi', ivhAI),
			ivhState()))
	ivhSetByte(ivhAI + 0x86, 0)
	ivhPoke(ivhAI + 0x1a4, ivhSave[1])

	# the script variables: "Repos" here, the rest of cAIMove
	for ivhN, ivhV, ivhSt in (('Repos', 0, 0), ('Repos', 5, 1), ('Repos', 0, 0), ('Repos', 0, 1), ('Repos', -3, 1),
			('Repos', 0, 0), ('', 1, 1), ('SDNoVar', 0, 0), ('repos', 1, 1), ('Re', 0, 0)):
		check('cAIVehicle_VarI' + fmt((ivhN, ivhV, ivhSt)), lambda n=ivhN, v=ivhV, s=ivhSt: (SDTest.Call('cAIVehicle_VarI',
			'tbispi', ivhAI, n, v, s), ivhB(ivhAI + 0x86)))
	ivhSetByte(ivhAI + 0x86, 0)
	for ivhN in ('', 'SDNoVar'):
		check('cAIVehicle_VarF' + fmt((ivhN,)), lambda n=ivhN: SDTest.Call('cAIVehicle_VarF', 'tbisDi', ivhAI, n, 1.5, 0))
		check('cAIVehicle_VarS' + fmt((ivhN,)), lambda n=ivhN: SDTest.Call('cAIVehicle_VarS', 'tbispi', ivhAI, n, 0, 0)[0])
		check('cAIVehicle_VarV' + fmt((ivhN,)), lambda n=ivhN: SDTest.Call('cAIVehicle_VarV', 'tbisDDDi', ivhAI, n, 1.0, 2.0,
			3.0, 0))

	# near and hook range from the position of the car, its nose to -z
	ivhP = ivhPos(ivhCar)
	for ivhD in ((0.0, 0.0, 0.0), (14999.0, 0.0, 0.0), (15001.0, 0.0, 0.0), (0.0, -8660.0, 12247.0),
			(10000.0, 10000.0, 3000.0), (-20000.0, 0.0, 0.0)):
		check('cAIVehicle_IsNear' + fmt((ivhD,)), lambda d=ivhD: SDTest.Call('cAIVehicle_IsNear', 'tbiF', ivhAI, ivhAdd(ivhP,
			d))[0])
	for ivhD in ((0.0, 0.0, -5000.0), (0.0, 0.0, 5000.0), (0.0, 0.0, -30000.0), (3000.0, 0.0, -3000.0),
			(2000.0, 0.0, -3000.0), (0.0, 0.0, -1.0), (0.0, 2500.0, -2500.0), (0.0, 0.0, -19999.0), (100.0, 0.0, 0.0)):
		check('cAIVehicle_InHookRangePos' + fmt((ivhD,)), lambda d=ivhD: SDTest.Call('cAIVehicle_InHookRangePos', 'tbiF', ivhAI,
			ivhAdd(ivhP, d))[0])
	check('radii', lambda: (ivhF(ivhCar + 0x3a0, 1), ivhF(ivhCar2 + 0x3a0, 1), ivhPos(ivhCar2)))
	check('cAIVehicle_InHookRange car2', lambda: SDTest.Call('cAIVehicle_InHookRange', 'tbii', ivhAI, ivhCar2)[0])
	check('cAIVehicle_CanHook car2', lambda: SDTest.Call('cAIVehicle_CanHook', 'tbii', ivhAI, ivhCar2)[0])
	check('cAIVehicle_InHookRange self', lambda: SDTest.Call('cAIVehicle_InHookRange', 'tbii', ivhAI, ivhCar)[0])
	ivhSet(ivhCar + 0xac, (0.0, 3.1415927, 0.0))
	check('cAIVehicle_InHookRange car2 turned', lambda: SDTest.Call('cAIVehicle_InHookRange', 'tbii', ivhAI, ivhCar2)[0])
	check('cAIVehicle_CanHook car2 turned', lambda: SDTest.Call('cAIVehicle_CanHook', 'tbii', ivhAI, ivhCar2)[0])
	ivhSet(ivhCar + 0xac, (0.5, 0.0, 0.0))
	check('cAIVehicle_InHookRangePos turned', lambda: SDTest.Call('cAIVehicle_InHookRangePos', 'tbiF', ivhAI, ivhAdd(ivhP, (0.0, 2000.0,
		-3000.0)))[0])
	ivhSet(ivhCar + 0xac, (0.0, 0.0, 0.0))

	# turn speeds, inertia, the boost flag from CONTROL_AIM
	for ivhV in (0.0, 1.0, 1500.0, -7.0, 3.0e30, 0.1):
		check('cAIVehicle_SetRotSpeed' + fmt((ivhV,)), lambda v=ivhV: (SDTest.Call('cAIVehicle_SetRotSpeed', 'tvif', ivhAI, v),
			ivhF(ivhAI + 0x304, 2)))
	check('cAIVehicle_SetRotStatic', lambda: (SDTest.Call('cAIVehicle_SetRotStatic', 'tviff', ivhAI, 0.125, 3.5), ivhF(ivhAI + 0x304, 2),
		ivhF(ivhCtrl + 0xa4, 2)))
	for ivhV in (1, 0):
		check('cAIVehicle_SetInertia(%d)' % ivhV, lambda v=ivhV: (SDTest.Call('cAIVehicle_SetInertia', 'tvii', ivhAI, v),
			ivhB(ivhCtrl + 0xb1)))
	for ivhV in (5, 4, 3, 1):
		ivhPoke(ivhCtrl + 0x60, ivhV)
		check('cAIVehicle_ApplyParams aim %d' % ivhV, lambda: (SDTest.Call('cAIVehicle_ApplyParams', 'tvi', ivhAI),
			ivhB(ivhCar + 0x71c)))
	# the kind of traffic track by the class of the vehicle
	if ivhPickT:
		for ivhCls in (0, 1, 2, 3):
			ivhPoke(ivhCar + 0x5e8, ivhCls)
			for ivhK in range(3):
				ivhSrand(100 + ivhK)
				check('cAIVehicle_PickTrackType' + fmt((ivhCls, ivhK)), lambda: (SDTest.Call('cAIVehicle_PickTrackType', 'tvi', ivhAI),
					ivhI(ivhAI + 0x31c)))
		ivhPoke(ivhCar + 0x5e8, ivhCarSave[3])

	# the kept state
	ivhBuf = ivhAlloc(0xcc)
	ivhSet(ivhAI + 0x4c, (1.0, 2.0, 3.0, 4.5))
	ivhSet(ivhAI + 0x64, (-1.0, -2.0, -3.0))
	ivhSet(ivhAI + 0x2f0, (6.0,))
	ivhSet(ivhAI + 0x30c, (7.0, 8.0, 9.0))
	ivhSetByte(ivhAI + 0x1a8, 1)
	ivhSetByte(ivhAI + 0x32c, 1)
	check('cAIVehicle_SaveState', lambda: (SDTest.Call('cAIVehicle_SaveState', 'tvii', ivhAI, ivhBuf), ivhInts(ivhBuf, (0, 4)),
		ivhF(ivhBuf + 8, 4), ivhB(ivhBuf + 0x18), ivhB(ivhBuf + 0x19), ivhF(ivhBuf + 0x9c, 4), ivhB(ivhBuf + 0xac),
		ivhF(ivhBuf + 0xb0, 1), ivhB(ivhBuf + 0xb4), ivhF(ivhBuf + 0xb8, 2), ivhB(ivhBuf + 0xc0),
		ivhF(ivhBuf + 0xc4, 2)))
	ivhSetByte(ivhAI + 0x1a8, 0)
	ivhSetByte(ivhAI + 0x32c, 0)

	# the decisions with SDIvhCar2 as target (the node of the link is left alone, only its target is set) and the
	# life full (no item search): reactions, the hook slots and the items
	ivhPoke(ivhAI + 0x40, ivhCar2)
	ivhSet(ivhAI + 0xc, (0.25,))
	check('target', lambda: (ivhB(ivhCar2 + 0x4c), ivhI(ivhCar + 0x238), ivhI(ivhCar + 0x640), ivhB(ivhAI + 0x340)))
	for ivhIn in (9, 0, 4):
		ivhPoke(ivhAI + 0x1a4, ivhIn)
		for ivhBoost in (0, 1):
			ivhPoke(ivhAI + 0x1c0, ivhBoost)
			check('cAIVehicle_ReactGetAway' + fmt((ivhIn, ivhBoost)), lambda: (SDTest.Call('cAIVehicle_ReactGetAway', 'tvi', ivhAI),
				ivhB(ivhCtrl + 4), ivhState()))
		ivhSetByte(ivhCtrl + 4, 0)
	# not with the state 0: it tests a line to a target the test AI does not have (a crash in both runs)
	for ivhIn, ivhDist in ((4, 0.0), (9, 1000.0), (9, 6000.0), (1, 0.0)):
		for ivhBoost in (0, 1):
			ivhPoke(ivhAI + 0x1a4, ivhIn)
			ivhSet(ivhAI + 0x2f0, (ivhDist,))
			ivhPoke(ivhAI + 0x1c0, ivhBoost)
			check('cAIVehicle_ReactGetAwayStop' + fmt((ivhIn, ivhDist, ivhBoost)), lambda: (SDTest.Call('cAIVehicle_ReactGetAwayStop', 'tvi',
				ivhAI), ivhB(ivhCtrl + 4), ivhState()))
	ivhPoke(ivhAI + 0x1c0, 0)
	ivhSetByte(ivhCtrl + 4, 0)
	ivhPoke(ivhAI + 0x1a4, 0)
	for ivhChase in (SDTest.Peek('g_AIChaseDefault1', 'i'), SDTest.Peek('g_MinDistChaseElevatorAI', 'i'), 0x40000000):
		ivhPoke(ivhAI + 0x310, ivhChase)
		check('cAIVehicle_ReactFighting %x' % ivhChase, lambda: (SDTest.Call('cAIVehicle_ReactFighting', 'tvi', ivhAI), ivhState(),
			ivhF(ivhAI + 0x310, 2)))
	for ivhT in (0.0, 15.0, 15.5):
		ivhSet(ivhAI + 0x2c, (ivhT,))
		check('cAIVehicle_ReactLostPath' + fmt((ivhT,)), lambda: (SDTest.Call('cAIVehicle_ReactLostPath', 'tvi', ivhAI), ivhState(),
			ivhB(ivhCar + 0x5ec)))
		check('cAIVehicle_ReactLostPath copy' + fmt((ivhT,)), lambda: (SDTest.Call('cAIVehicle_ReactLostPath@87600', 'tvi', ivhAI), ivhState()))
	SDTest.Call('cAIVehicle_SetState', 'bii', ivhAI, 0)
	if ivhNodesWorld:
		for ivhDt in (0.1, 0.3):
			ivhSet(ivhAI + 0xc, (ivhDt,))
			check('cAIVehicle_ThinkLostPath' + fmt((ivhDt,)), lambda: (SDTest.Call('cAIVehicle_ThinkLostPath', 'tvi', ivhAI), ivhState(),
				ivhF(ivhAI + 0x164, 2)))
			check('cAIVehicle_ThinkLostPath copy' + fmt((ivhDt,)), lambda: (SDTest.Call('cAIVehicle_ThinkLostPath@88070', 'tvi', ivhAI),
				ivhState()))
	# the hook slots: no hook point yet, without and with thrust (FACTOR_HOOK_MAP 3: at random)
	ivhSrand(2024)
	for ivhThrust in (0.0, 0.5):
		ivhSet(ivhCar + 0x420, (ivhThrust,))
		for ivhIn in (4, 5, 6, 7, 9):
			ivhPoke(ivhAI + 0x1a4, ivhIn)
			for ivhRva, ivhName in ((sym('cAIVehicle_ThinkGoTo'), 'ThinkGoTo'), (sym('cAIVehicle_ThinkChase'), 'ThinkChase'), (sym('cAIVehicle_ThinkFighting'), 'ThinkFighting'),
					(sym('cAIVehicle_ThinkGetAway'), 'ThinkGetAway'), (sym('cAIVehicle_ThinkGetAwayStop'), 'ThinkGetAwayStop')):
				ivhSet(ivhAI + 0xc, (0.3,))
				check('cAIVehicle_%s' % ivhName + fmt((ivhThrust, ivhIn)), lambda r=ivhRva: (SDTest.Call(r, 'tvi',
					ivhAI), SDTest.Call('cTargetLock_GetPoint', 'tii', ivhCar + 0x5f0)[0] != 0, ivhState(), ivhF(ivhAI + 0x184, 2)))
	# RACE with a target only (without one the vehicles of the group are tried)
	for ivhT, ivhIn in ((20.0, 1), (0.0, 1), (0.0, 6), (14.5, 6)):
		ivhPoke(ivhAI + 0x40, ivhCar2)
		ivhPoke(ivhAI + 0x1a4, ivhIn)
		ivhSet(ivhAI + 0x330, (ivhT,))
		ivhSet(ivhAI + 0xc, (0.3,))
		check('cAIVehicle_ThinkRace' + fmt((ivhT, ivhIn)), lambda: (SDTest.Call('cAIVehicle_ThinkRace', 'tvi', ivhAI), ivhState(),
			ivhI(ivhAI + 0x40) == ivhCar2, ivhF(ivhAI + 0x330, 1)))
	for ivhK in range(3):
		ivhPoke(ivhCar + 0x420 + 4 * ivhK, ivhCarSave[ivhK])
	SDTest.Call('cWeaponManager_LockPoint', 'tvii', ivhCar + 0x5f0, 0)
	ivhPoke(ivhAI + 0x1a4, 0)
	SDTest.Call('cAIVehicle_SetState', 'bii', ivhAI, 0)
	ivhPoke(ivhAI + 0x40, ivhCar2)
	# the items with the life full
	ivhPoke(ivhAI + 0x1ac, ivhI(ivhCar + 0x238))
	for ivhIn in (5, 6, 7, 8, 9, 3):
		ivhPoke(ivhAI + 0x1a4, ivhIn)
		for ivhRva, ivhName in ((sym('cAIVehicle_ItemsChase'), 'ItemsChase'), (sym('cAIVehicle_ItemsFighting'), 'ItemsFighting'), (sym('cAIVehicle_ItemsGetAway'), 'ItemsGetAway'),
				(sym('cAIVehicle_ItemsGetAwayStop'), 'ItemsGetAwayStop'), (sym('cAIVehicle_ItemsInRoute'), 'ItemsInRoute')):
			check('cAIVehicle_%s' % ivhName + fmt((ivhIn,)), lambda r=ivhRva: (SDTest.Call(r, 'tvi', ivhAI),
				ivhState(), ivhB(ivhAI + 0x340)))
	ivhPoke(ivhAI + 0x1a4, 0)
	SDTest.Call('cAIVehicle_SetState', 'bii', ivhAI, 0)
	ivhPoke(ivhAI + 0x40, ivhCar2)
	# the ammo items for a vehicle without weapons (only then: with weapons the items of the level are searched)
	ivhUses = ()
	for ivhK in range(3):
		for ivhFac in (0.0, 3.0e38):
			ivhUses = ivhUses + (SDTest.Call('cAIVehicleShoot_UsesAmmo', 'tbiif', ivhShoot, ivhK, ivhFac)[0],)
	check('cAIVehicleShoot_UsesAmmo', lambda: ivhUses)
	if ivhUses != (0, 0, 0, 0, 0, 0):
		ivhSkip = 1
	else:
		ivhSkip = 0
	for ivhS in range(1, 4 - 3 * ivhSkip):
		ivhSrand(ivhS)
		check('cAIVehicle_FindAmmo srand %d' % ivhS, lambda: (SDTest.Call('cAIVehicle_FindAmmo', 'tvii', ivhAI, 0),
			ivhI(ivhAI + 0x38c) == 0, ivhI(ivhAI + 0x3ac)))
	ivhD = ivhAlloc(8)
	if not ivhSkip:
		for ivhFac in (0.0, 0.5, 3.0e38):
			check('cAIVehicle_NearestAmmoAny' + fmt((ivhFac,)), lambda f=ivhFac: SDTest.Call('cAIVehicle_NearestAmmoAny', 'tiiFiif', ivhAI,
				(0.0,), ivhD, 0, f)[0:2])
		ivhPoke(ivhAI + 0x1a4, 6)
		ivhSrand(9)
		check('cAIVehicle_FightingAfterItem', lambda: (SDTest.Call('cAIVehicle_FightingAfterItem', 'tvi', ivhAI), ivhState(),
			ivhB(ivhAI + 0x354), ivhI(ivhAI + 0x38c) == 0, ivhI(ivhAI + 0x3ac)))
	SDTest.Call('cString_dtor', 'tvi', ivhD)
	ivhFree(ivhD)
	ivhSetByte(ivhAI + 0x340, 1)
	check('cAIVehicle_FightingAfterItem item', lambda: (SDTest.Call('cAIVehicle_FightingAfterItem', 'tvi', ivhAI), ivhState()))
	ivhSetByte(ivhAI + 0x340, 0)
	ivhPoke(ivhAI + 0x1a4, 0)
	ivhPoke(ivhAI + 0x40, 0)
	SDTest.Call('cAIVehicle_SetState', 'bii', ivhAI, 0)

	# the parameter checks with the valid blocks
	check('cAIVehicle_CheckParams', lambda: (SDTest.Call('cAIVehicle_CheckParams1', 'tvii', ivhAI, ivhP1),
		SDTest.Call('cAIVehicle_CheckParams2', 'tvii', ivhAI, ivhP2), SDTest.Call('cAIVehicle_CheckParams3', 'tvii', ivhAI, ivhP3),
		SDTest.Call('cAIVehicle_Setup', 'tviiii', ivhAI, ivhP1, ivhP2, ivhP3)))
	ivhPoke(ivhP1 + 0x18, 1)
	ivhPoke(ivhP2 + 0x14, 10)
	ivhSet(ivhP3 + 8, (3.1415925, 1.0))
	check('cAIVehicle_Setup edges', lambda: SDTest.Call('cAIVehicle_Setup', 'tviiii', ivhAI, ivhP1, ivhP2, ivhP3))

	# the debug text (to the console)
	check('cAIVehicle_Debugf', lambda: (SDTest.Call('cAIVehicle_Debugf', 'vis', ivhAI, 'SDIvh debug'),
		SDTest.Call('cAIVehicle_Debugf', 'visi', ivhAI, 'SDIvh %d', 7)))

	# the events that end in a free state or in nothing
	for ivhS, ivhIn, ivhEv in ((0, 0, 2), (0, 0, 7), (0, 0, 5), (2, 0, 3), (4, 0, 0), (9, 0, 4), (1, 0, 3), (7, 4, 1),
			(13, 0, 2)):
		ivhPoke(ivhAI + 0x19c, ivhS)
		ivhPoke(ivhAI + 0x1a4, ivhIn)
		for ivhRva, ivhName in ((sym('cAIVehicle_EventDefault'), 'EventDefault'), (sym('cAIVehicle_EventChase'), 'EventChase'), (sym('cAIVehicle_EventFighting'), 'EventFighting'),
				(sym('cAIVehicle_EventGetAway'), 'EventGetAway'), (sym('cAIVehicle_EventGetAwayStop'), 'EventGetAwayStop'), (sym('cAIVehicle_EventGoTo'), 'EventGoTo'),
				(sym('cAIVehicle_EventInRoute'), 'EventInRoute'), (sym('cAIVehicle_EventLostPath'), 'EventLostPath'), (sym('cAIVehicle_EventTraffic'), 'EventTraffic')):
			# not: the moves (arrived in a chase, a fight, a traffic node, near an item), the lost path, the python
			# callbacks (arrived of GO_TO, the target gone) and the target of a get away
			if ivhEv == 2 and ivhName in ('EventChase', 'EventFighting', 'EventGoTo', 'EventTraffic'):
				continue
			if ivhEv in (0, 1, 3) and ivhName == 'EventDefault':
				continue
			if ivhEv in (2, 4) and ivhIn in (4, 8, 9):
				continue
			check('cAIVehicle_%s' % ivhName + fmt((ivhS, ivhIn, ivhEv)), lambda r=ivhRva, e=ivhEv: (SDTest.Call(r,
				'tbii', ivhAI, e)[0], ivhState()))
	ivhPoke(ivhAI + 0x19c, 0)
	ivhPoke(ivhAI + 0x1a4, 0)
	check('cAIVehicle_Event 1', lambda: (SDTest.Call('cAIVehicle_Event', 'tvii', ivhAI, 1), ivhState()))
	ivhPoke(ivhAI + 0x19c, 13)
	check('cAIVehicle_Event 7 LOST_PATH', lambda: (SDTest.Call('cAIVehicle_Event', 'tvii', ivhAI, 7), ivhState()))
	check('cAIVehicle_Event 2 FREEZE_LOST', lambda: (SDTest.Call('cAIVehicle_Event', 'tvii', ivhAI, 2), ivhState()))
	check('cAIVehicle_EventDefault 1', lambda: (SDTest.Call('cAIVehicle_EventDefault', 'tbii', ivhAI, 1)[0], ivhState()))

	# the changes to the free states, from outside, from inside and from a kept state
	for ivhS in (0, 11, 15, 16, 0):
		check('cAIVehicle_SetState(%d)' % ivhS, lambda s=ivhS: (SDTest.Call('cAIVehicle_SetState', 'bii', ivhAI, s)[0],
			ivhState(), ivhB(ivhAI + 0x20), ivhB(ivhAI + 0x28)))
		check('cAIVehicle_SetStateIntern(%d)' % ivhS, lambda s=ivhS: (SDTest.Call('cAIVehicle_SetStateIntern', 'bii', ivhAI, s)[0],
			ivhState()))
	for ivhS in (0, 11, 15, 16, 17, -1, 0):
		ivhPoke(ivhBuf, ivhS)
		check('cAIVehicle_RestoreState(%d)' % ivhS, lambda: (SDTest.Call('cAIVehicle_RestoreState', 'tbii', ivhAI, ivhBuf)[0],
			ivhState()))
		check('cAIVehicle_RestoreStateIntern(%d)' % ivhS, lambda: (SDTest.Call('cAIVehicle_RestoreStateIntern', 'tbii', ivhAI, ivhBuf)[0],
			ivhState()))
	# the changes to the states with a target (SDIvhCar2), a goal or a route: the timer of the group set full before
	# each (a change sets it to 0), the doubles of the arguments as two ints (low, high)
	def ivhFull(a=0):
		ivhSet(ivhGroup + 0x5c, (1.0e9,))
	def ivhChange(sig, args):
		ivhFull()
		return (apply(SDTest.Call, (sym('cAIVehicle_SetState'), sig, ivhAI) + args)[0], ivhState(), ivhI(ivhAI + 0x40) == ivhCar2,
			ivhF(ivhAI + 0x304, 2), ivhF(ivhAI + 0x4c, 4), ivhF(ivhAI + 0x310, 2), ivhI(ivhAI + 0x5c))
	ivhD1 = (0, 0x3ff00000)
	ivhD2 = (0, 0x40000000)
	ivhD5000 = (0, 0x40b38800)
	ivhD20000 = (0, 0x40d38800)
	ivhD300 = (0, 0x4072c000)
	ivhSrand(31)
	# the states that move CHECK the radius of the vehicle against RADIUS_SHIP_GRAPH
	ivhRadius = SDTest.Call(ivhVt(ivhCar, 0xa0), 'tfi', ivhCar)[0]
	check('radius graph', lambda: (ivhRadius, ivhF(symva('g_NWNodeRadius'), 1)))
	if ivhRadius < ivhF(symva('g_NWNodeRadius'), 1)[0]:
		ivhMoves = 1
	else:
		ivhMoves = 0
	for ivhName, ivhSig, ivhArgs in (('FIGHTING', 'biii', (2, ivhCar2)), ('FIGHTING_FREEZE', 'biii', (8, ivhCar2)),
			('CHASE', 'biiiiiii', (7, ivhCar2) + ivhD5000 + ivhD20000), ('GET_AWAY', 'biii', (12, ivhCar2)),
			('GET_AWAY_STOP', 'biiiii', (5, ivhCar2) + ivhD5000), ('FIGHTING_HOOK', 'biii', (10, ivhCar2)),
			('GO_TO', 'biifffiiiiiiiiii', (1, 54000.0, 50000.0, 70000.0) + ivhD300 + (1,) + ivhD2 + ivhD1 + ivhD2 +
				(1,)),
			('RACE', 'biifffiiii', (6, 54000.0, 50500.0, 65000.0) + ivhD300 + (0, 1)),
			('IN_ROUTE', 'biiiiiii', (3, 1) + ivhD1 + ivhD2), ('FREEZE', 'bii', (0,))):
		if ivhMoves or ivhName == 'FREEZE':
			check('cAIVehicle_SetState ' + ivhName, lambda s=ivhSig, a=ivhArgs: ivhChange(s, a))
	if ivhNodesWorld and ivhMoves:
		for ivhS in (13, 14):
			check('cAIVehicle_SetState(%d)' % ivhS, lambda s=ivhS: (ivhChange('bii', (s,)), ivhB(ivhCar + 0x5ec)))
		# FREEZE with the lane off waits (without FREEZE_CHANGE_STATE)
		check('cAIVehicle_SetState(0) lane off', lambda: (ivhChange('bii', (0,)), ivhB(ivhCar + 0x5ec)))
		SDTest.Call('cVehicleTraffic_SetOff', 'tvii', ivhCar + 0x5c4, 0)
		check('cAIVehicle_SetState(0) lane on', lambda: ivhChange('bii', (0,)))
	if ivhMoves:
		# not let through by the group: pending with the name of its target, FREEZE_CHANGE_STATE meanwhile, then
		# taken from the pending state
		ivhSet(ivhGroup + 0x5c, (0.0,))
		check('cAIVehicle_SetState FIGHTING pending', lambda: (SDTest.Call('cAIVehicle_SetState', 'biii', ivhAI, 2, ivhCar2)[0],
			ivhState(), SDTest.Peek(ivhAI + 0xa9, '@s')))
		ivhFull()
		check('cAIVehicle_RestoreState pending', lambda: (SDTest.Call('cAIVehicle_RestoreState', 'tbii', ivhAI, ivhAI + 0x90)[0],
			ivhState(), ivhI(ivhAI + 0x40) == ivhCar2))
		check('cAIVehicle_SetState FREEZE again', lambda: (ivhChange('bii', (0,)), ivhI(ivhAI + 0x40)))
		# a target the AI cannot have, a state that does not exist
		check('cAIVehicle_SetState FIGHTING null', lambda: ivhChange('biii', (2, 0)))
		check('cAIVehicle_SetState unknown', lambda: ivhChange('bii', (17,)))

	# a pending change to FREEZE taken in the update once the group lets it (its timer set full)
	SDTest.Call('cAIVehicle_SetState', 'bii', ivhAI, 15)
	ivhPoke(ivhAI + 0x90, 0)
	ivhSet(ivhGroup + 0x5c, (1.0e9,))
	check('cAIVehicle_Update pending', lambda: (SDTest.Call('cAIVehicle_Update', 'tvif', ivhAI, 0.05), ivhState()))
	if ivhNodesWorld:
		check('cAIVehicle_UpdateOff', lambda: (SDTest.Call('cAIVehicle_UpdateOff', 'tvif', ivhAI, 0.05), ivhState()))
	ivhPoke(ivhAI + 0x19c, 0)
	# CheckTarget without a target (only when OnObjectiveNullAI has no function: the callback runs level scripts)
	if SDTest.Peek(0x4447dc, 'i') == 0:
		check('cAIVehicle_CheckTarget', lambda: (SDTest.Call('cAIVehicle_CheckTarget', 'tbi', ivhAI)[0], ivhState()))
		check('cAIVehicle_ThinkHook', lambda: (SDTest.Call('cAIVehicle_ThinkHook', 'tvi', ivhAI), ivhState(), ivhF(ivhAI + 0x330, 1)))

	# the item searches agree with each other
	ivhD = ivhAlloc(8)
	ivhE1 = SDTest.Call('cAIVehicle_NearestLife', 'tiiFii', ivhAI, (0.0,), ivhD, 0)
	ivhE2 = SDTest.Call('cAIVehicle_GetNearestItemLife', 'tii', ivhAI)[0]
	SDTest.Call('cAIVehicle_FindLife', 'tvii', ivhAI, 0)
	check('cAIVehicle items life', lambda: (ivhE1[0] == ivhE2, ivhI(ivhAI + 0x34c) == ivhE2))
	for ivhK in range(3):
		ivhA1 = SDTest.Call('cAIVehicle_NearestAmmo', 'tiiiFii', ivhAI, ivhK, (0.0,), ivhD, 0)
		ivhA2 = SDTest.Call('cAIVehicle_NearestAmmo', 'tiiiFii', ivhAI, ivhK, (0.0,), ivhD, 0)
		check('cAIVehicle items ammo %d' % ivhK, lambda a=ivhA1, b=ivhA2: (a[0] == b[0], a[1] == b[1]))
	SDTest.Call('cString_dtor', 'tvi', ivhD)
	ivhFree(ivhD)
	check('cAIVehicle_WeapListChanged', lambda: SDTest.Call('cAIVehicle_WeapListChanged', 'tvi', ivhAI))

	# away again
	check('cAIVehicle_Delete', lambda: SDTest.Call('cAIVehicle_Delete', 'tiii', ivhAI, 1)[0] == ivhAI)
	ivhFree(ivhBuf)
	ivhFree(ivhP1)
	ivhFree(ivhP2)
	ivhFree(ivhP3)
	for ivhK in range(6):
		ivhPoke(ivhGroup + (0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70)[ivhK], ivhGroupSave[ivhK])
	for ivhK in range(4):
		ivhPoke(ivhCar + (0x420, 0x424, 0x428, 0x5e8)[ivhK], ivhCarSave[ivhK])
if ivhCarObj:
	check('SDIvhCar Del', lambda: ivhCarObj.Del())
if ivhCar2Obj:
	check('SDIvhCar2 Del', lambda: ivhCar2Obj.Del())
