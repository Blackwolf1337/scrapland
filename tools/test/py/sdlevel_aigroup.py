# game AI/AICharGroup.cpp, AIEntityGroup.cpp, AIMove.cpp, the small AI classes and their move controls (src/game/AI/
# ICharGroup.c, AIEntityGroup.c, AIMove.c, AI<Class>.c, AIControls.c), execfile'd by sdlevel.py (Python 1.5.2).
# Pure parts first: an entity group, the points and the dummy lists of the character group on memory of our own. The
# group of the level is only read; a second one is made by its constructor from the dummies of the level and deleted.
# The repositioning (GetReposPos, FindRepos) runs on a group of our own whose points are far above the camera. Then
# two WalkChars of our own far above the map ("SDAgrWalk", "SDAgrWalk2", fake action controllers as in
# sdlevel_aichar.py) carry the AIs of the classes: their distances by state, the action checks, the shot parameters,
# the cAIMove slots and the parts of the move controls that need no path, no line test and no strafe of the base.
# Not called: the Orbit of the bank master, the CheckWait and Strafe2 overrides (the base strafe looks for the
# characters near), the slots of the strafe that fire SideBlocked.
def agrI(a):
	return SDTest.Peek(a, '@i')
def agrB(a):
	return SDTest.Peek(a, '@b')
def agrF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def agrSet(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def agrPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def agrPokeB(a, v):
	w = agrI(a & ~3)
	sh = 8 * (a & 3)
	agrPoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def agrAlloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def agrFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def agrNew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def agrInts(a, offs):
	r = ()
	for o in offs:
		r = r + (agrI(a + o),)
	return r
def agrBytes(a, offs):
	r = ()
	for o in offs:
		r = r + (agrB(a + o),)
	return r
# the characters of s at a (a multiple of 4 bytes, zeros after it)
def agrPutStr(a, s):
	i = 0
	while i <= len(s):
		w = 0
		j = 0
		while j < 4 and i + j < len(s):
			w = w | (ord(s[i + j]) << (8 * j))
			j = j + 1
		agrPoke(a + i, w)
		i = i + 4
def agrStr(s):
	p = agrAlloc((len(s) + 8) & ~3)
	agrPutStr(p, s)
	return p
def agrSetPos(e, p):
	SDTest.Call(agrI(agrI(e) + 0x1c) - 0x400000, 'tviF', e, p)
def agrAdd(a, b):
	return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def agrSrand(n):
	SDTest.Call('crt_srand', 'vi', n)
agrStrs = ()
def agrS(s):
	global agrStrs
	p = agrStr(s)
	agrStrs = agrStrs + (p,)
	return p

# ---- cAIEntityGroup ----
agrEg = agrAlloc(0x60)
agrLimit = agrF(symva('g_TimeBetwChangeStateAI'), 1)[0]
check('AGR EntityGroup ctor', lambda: (SDTest.Call('cAIEntityGroup_ctor', 'tii', agrEg)[0] == agrEg, agrI(agrEg) - 0x400000,
	agrI(agrEg + 0x54), agrB(agrEg + 0x58), agrF(agrEg + 0x5c, 1)[0] == agrLimit))
def agrEgAdd(a=0):
	r = ()
	for k in range(21):
		r = r + (SDTest.Call('cAIEntityGroup_Add', 'tbii', agrEg, 0x1000 + 16 * k)[0], agrI(agrEg + 0x54))
	return r
check('AGR EntityGroup Add', agrEgAdd)
def agrEgGet(a=0):
	r = ()
	for k in range(20):
		r = r + (SDTest.Call('cAIEntityGroup_Get', 'tiii', agrEg, k)[0] - 0x1000,)
	return r
check('AGR EntityGroup Get', agrEgGet)
check('AGR EntityGroup Num Clear', lambda: (SDTest.Call('cAIEntityGroup_Num', 'tii', agrEg)[0], SDTest.Call('cAIEntityGroup_Clear', 'tvi',
	agrEg), agrI(agrEg + 0x54), SDTest.Call('cAIEntityGroup_Num', 'tii', agrEg)[0], SDTest.Call('cAIEntityGroup_Add', 'tbii', agrEg, 0x50)[0],
	agrI(agrEg + 4) - 0x50))
for agrT in (0.0, 0.025, agrLimit, 0.5, -1.0):
	agrSet(agrEg + 0x5c, (agrT,))
	check('AGR EntityGroup CanChange %s' % fmt(agrT), lambda: SDTest.Call('cAIEntityGroup_CanChange', 'tbi', agrEg)[0])
check('AGR EntityGroup times', lambda: (SDTest.Call('cAIEntityGroup_ResetChange', 'tvi', agrEg), agrF(agrEg + 0x5c, 1),
	SDTest.Call('cAIEntityGroup_Update', 'tvif', agrEg, 0.25), agrF(agrEg + 0x5c, 1), SDTest.Call('cAIEntityGroup_Update', 'tvif', agrEg, 0.125),
	agrF(agrEg + 0x5c, 1), SDTest.Call('cAIEntityGroup_AllowChange', 'tvi', agrEg), agrF(agrEg + 0x5c, 1)[0] == agrLimit))
check('AGR EntityGroup Delete dtor', lambda: (agrPoke(agrEg, 0), SDTest.Call('cAIEntityGroup_Delete', 'tiii', agrEg, 0)[0] == agrEg,
	agrI(agrEg) - 0x400000, agrPoke(agrEg, 0), SDTest.Call('cAIEntityGroup_dtor', 'tvi', agrEg), agrI(agrEg) - 0x400000))
agrFree(agrEg)

# ---- the points of the character group and the Nto1 copies ----
agrHd = agrAlloc(0x10)
agrPoke(agrHd, agrHd)
agrPoke(agrHd + 4, agrHd)
agrPoke(agrHd + 8, 0x1234)
agrPt = agrAlloc(0x1c)
check('AGR point ctor', lambda: (agrPoke(agrPt + 8, 7), SDTest.Call('cAICGPoint_ctor', 'tii', agrPt)[0] == agrPt,
	agrI(agrPt) == agrPt, agrI(agrPt + 4) == agrPt, agrI(agrPt + 8), agrI(agrPt + 0xc)))
agrPoke(agrPt + 0xc, agrPt)
def agrRing(a=0):
	return (agrI(agrHd) == agrPt, agrI(agrHd + 4) == agrPt, agrI(agrPt) == agrHd, agrI(agrPt + 8), agrI(agrHd) == agrHd,
		agrI(agrPt) == agrPt)
check('AGR UnlinkOpt', lambda: (SDTest.Call('Nto1Node_UnlinkOpt', 'tvii', 0, 0), SDTest.Call('Nto1Head_Link', 'tvii', agrHd, agrPt),
	agrRing(), SDTest.Call('Nto1Node_UnlinkOpt', 'tvii', 0, agrPt), agrRing()))
check('AGR point dtor', lambda: (SDTest.Call('Nto1Head_Link', 'tvii', agrHd, agrPt), agrRing(), SDTest.Call('cAICGPoint_dtor', 'tvi',
	agrPt), agrRing(), SDTest.Call('cAICGPoint_Delete', 'tiii', agrPt, 0)[0] == agrPt, agrRing()))
agrPoke(agrPt + 0xc, 0)
check('AGR point dtor head', lambda: (SDTest.Call('cAICGPoint_dtor', 'tvi', agrPt), agrI(agrPt) == agrPt, agrI(agrPt + 8)))
agrFree(agrPt)
agrFree(agrHd)
agrArr = SDTest.Call('operator_new_array', 'ii', 4 + 3 * 0x1c)[0]
agrPoke(agrArr, 3)
for agrK in range(3):
	SDTest.Call('cAICGPoint_ctor', 'tii', agrArr + 4 + 0x1c * agrK)
	agrPoke(agrArr + 4 + 0x1c * agrK + 0xc, agrArr + 4 + 0x1c * agrK)
check('AGR point Delete array', lambda: SDTest.Call('cAICGPoint_Delete', 'tiii', agrArr + 4, 3)[0] == agrArr)
agrPt = agrNew(0x1c)
SDTest.Call('cAICGPoint_ctor', 'tii', agrPt)
check('AGR point Delete one', lambda: SDTest.Call('cAICGPoint_Delete', 'tiii', agrPt, 1)[0] == agrPt)

# the dummy lists: two position nodes, a free list of one
agrL = agrAlloc(0x18)
agrN1 = agrNew(0x14)
agrN2 = agrNew(0x14)
agrPoke(agrN1, agrN2)
agrPoke(agrN1 + 4, agrN2)
agrPoke(agrN2, agrN1)
agrPoke(agrN2 + 4, agrN1)
agrPoke(agrL, agrN1)
agrPoke(agrL + 4, agrN2)
agrPoke(agrL + 8, 2)
agrPoke(agrL + 0x14, 1)
check('AGR PosList_Clear', lambda: (SDTest.Call('cAICGPosList_Clear', 'tvi', agrL), agrI(agrL), agrI(agrL + 4), agrI(agrL + 8),
	agrI(agrL + 0xc) == agrN1, agrI(agrL + 0x10), agrI(agrL + 0x14)))
agrZd = agrAlloc(0x98)
SDTest.Call('crt_memcpy', 'iiii', agrZd + 0x80, agrL, 0x18)
check('AGR ZoneData dtor', lambda: (SDTest.Call('cAICGZoneData_dtor', 'tvi', agrZd), agrInts(agrZd, (0x80, 0x88, 0x8c, 0x90))))
agrFree(agrZd)
# zone nodes, the inner list of the second with a free node
agrZ1 = agrNew(0xa0)
agrZ2 = agrNew(0xa0)
for agrZ in (agrZ1, agrZ2):
	for agrO in (0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c):
		agrPoke(agrZ + agrO, 0)
agrPoke(agrZ2 + 0x94, agrNew(0x14))
agrPoke(agrZ2 + 0x98, 1)
agrPoke(agrZ1, agrZ2)
agrPoke(agrZ1 + 4, agrZ2)
agrPoke(agrZ2, agrZ1)
agrPoke(agrZ2 + 4, agrZ1)
agrPoke(agrL, agrZ1)
agrPoke(agrL + 4, agrZ1)
agrPoke(agrL + 8, 2)
agrPoke(agrL + 0xc, 0)
agrPoke(agrL + 0x10, 0)
agrPoke(agrL + 0x14, 1)
check('AGR ZoneList_Clear', lambda: (SDTest.Call('cAICGZoneList_Clear', 'tvi', agrL), agrI(agrL), agrI(agrL + 4), agrI(agrL + 8),
	agrI(agrL + 0xc) == agrZ1, agrI(agrL + 0x10)))
check('AGR ZoneList_FreePool', lambda: (SDTest.Call('cAICGZoneList_FreePool', 'tvi', agrL), agrInts(agrL, (0, 8, 0xc, 0x10))))
check('AGR ZoneList dtor', lambda: (SDTest.Call('cAICGZoneList_dtor', 'tvi', agrL), agrInts(agrL, (0, 8, 0xc, 0x10))))
agrFree(agrL)

# ---- the character group of the level (read only) and one of our own from the same dummies ----
agrWorld = SDTest.Peek('g_pWorld', 'i')
agrGroup = SDTest.Peek('g_CharGroup', 'i')
def agrZones(g):
	r = ()
	n = agrI(g + 0x64)
	if n > 12:
		n = 12
	for k in range(n):
		z = agrI(g + 0x60) + 0x88 * k
		pts = ()
		np = agrI(z + 0x84)
		for j in range(np):
			p = agrI(z + 0x80) + 0x1c * j
			if j < 4:
				pts = pts + (agrF(p + 0x10, 3), agrI(p + 0xc) == p)
		r = r + ((SDTest.Peek(z, '@s'), np, pts),)
	return r
check('AGR group', lambda: (agrGroup != 0, agrGroup and agrI(agrGroup) - 0x400000))
if agrGroup:
	check('AGR group zones', lambda: (SDTest.Call('cAICharGroup_GetZones', 'tii', agrGroup)[0] == agrI(agrGroup + 0x60),
		SDTest.Call('cAICharGroup_NumZones', 'tii', agrGroup)[0], SDTest.Call('cAIEntityGroup_Num', 'tii', agrGroup)[0], agrZones(agrGroup)))
	agrG2 = agrNew(0x68)
	check('AGR group ctor', lambda: (SDTest.Call('cAICharGroup_ctor', 'tii', agrG2)[0] == agrG2, agrI(agrG2) - 0x400000,
		agrInts(agrG2, (0x54, 0x64)), agrF(agrG2 + 0x5c, 1)[0] == agrLimit, agrZones(agrG2) == agrZones(agrGroup),
		agrZones(agrG2)))
	check('AGR group Delete', lambda: SDTest.Call('cAICharGroup_Delete', 'tiii', agrG2, 1)[0] == agrG2)

# ---- two WalkChars of our own ----
agrP = (-38000.0, 63000.0, 41000.0)
agrQ = agrAdd(agrP, (200.0, 0.0, 0.0))
agrSrand(4711)
agrW = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', agrWorld, 'SDAgrWalk', agrP[0], agrP[1], agrP[2], 'WalkChar')[0]
agrW2 = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', agrWorld, 'SDAgrWalk2', agrQ[0], agrQ[1], agrQ[2], 'WalkChar')[0]
check('AGR WalkChars', lambda: (agrW != 0, agrW2 != 0, agrW and agrI(agrW + 0x78) == 0, agrW and agrI(agrW + 0x4bc + 8)
	== agrW, agrW2 and agrI(agrW2 + 0x254 + 8) == agrW2))
agrOk = agrW and agrW2 and agrI(agrW + 0x4bc + 8) == agrW and agrI(agrW2 + 0x254 + 8) == agrW2
if agrOk:
	agrSetPos(agrW, agrP)
	agrSetPos(agrW2, agrQ)
	agrSet(agrW + 0x428, (0.0, 0.0, 0.0))
	agrSet(agrW2 + 0x428, (0.0, 0.0, 0.0))
	agrFakes = ()
	for agrE in ((agrW, 'SDAgrZone'), (agrW2, 'Bishop')):
		agrAc = agrAlloc(0x80)
		agrCls = agrAlloc(0x200)
		agrAct = agrAlloc(0x80)
		agrPoke(agrAc + 8, agrCls)
		agrPoke(agrAc + 0x14, agrAct)
		agrPoke(agrAct + 0x64, 0x10)
		agrPoke(agrCls + 4, agrS(agrE[1]))
		agrSet(agrCls + 0x164, (40.0,))
		agrSet(agrCls + 0x16c, (25.0,))
		agrPoke(agrE[0] + 0x234, agrAc)
		agrSet(agrE[0] + 0x45c, (30.0,))
		agrFakes = agrFakes + ((agrAc, agrCls, agrAct),)
	agrActName = agrAlloc(0x20)
	agrActName2 = agrAlloc(0x20)
	agrPoke(agrFakes[0][2] + 8, agrActName)
	agrPoke(agrFakes[1][2] + 8, agrActName2)
	# the action of layer 0 of e: name, or none
	def agrAction(e, name):
		if e == agrW:
			ac = agrFakes[0][0]
			buf = agrActName
		else:
			ac = agrFakes[1][0]
			buf = agrActName2
		if name:
			agrPutStr(buf, name)
			agrPokeB(ac + 0x1c, 1)
		else:
			agrPokeB(ac + 0x1c, 0)
	agrGroupTime = 0
	if agrGroup:
		agrGroupTime = agrI(agrGroup + 0x5c)

	# ---- the repositioning on a group of our own, the points far above the camera ----
	agrCur = agrI(agrWorld + 0x50)
	agrCam = agrF(agrWorld + 0x254 + 12 * agrCur, 3)
	agrCamName = SDTest.Peek(agrWorld + 0x54, '@s')
	agrCamEnt = 0
	if agrCamName:
		agrSlot = SDTest.Call('HashTable_Slot@65370', 'tiis', agrWorld + 4, agrCamName)[0]
		agrNode = SDTest.Call('PtrTable_Find@652e0', 'tiisi', agrWorld + 4, agrCamName, agrSlot)[0]
		if agrNode:
			agrCamEnt = agrI(agrNode)
	check('AGR camera', lambda: (agrCamEnt != 0, SDTest.Call('Floor_Get', 'tiif', agrWorld + 0x2f0, agrCam[1])[0],
		SDTest.Call('Floor_Get', 'tiif', agrWorld + 0x2f0, agrCam[1] + 30000.0)[0]))
	if agrCamEnt:
		agrLook = SDTest.Call('Vec3_Rotate', 'iFFF', (0.0, 0.0, 0.0), (0.0, 0.0, 1.0), agrF(agrCamEnt + 0x88, 3))[1]
		agrLen = (agrLook[0] * agrLook[0] + agrLook[2] * agrLook[2]) ** 0.5
		if agrLen < 0.001:
			agrLook = (1.0, 0.0, 0.0)
			agrLen = 1.0
		agrLx = agrLook[0] / agrLen
		agrLz = agrLook[2] / agrLen
		agrFg = agrAlloc(0x68)
		agrZs = agrAlloc(2 * 0x88)
		agrPoke(agrFg + 0x60, agrZs)
		agrPoke(agrFg + 0x64, 2)
		agrPutStr(agrZs, 'SDAgrZone')
		agrPutStr(agrZs + 0x88, 'SDAgrOther')
		agrPts = ()
		for agrZd in ((0, (1500.0, -1000.0, -3000.0, 800.0)), (1, (-500.0, 500.0))):
			agrN = len(agrZd[1])
			agrA = SDTest.Call('operator_new_array', 'ii', 4 + agrN * 0x1c)[0]
			agrPoke(agrA, agrN)
			for agrK in range(agrN):
				agrPp = agrA + 4 + 0x1c * agrK
				SDTest.Call('cAICGPoint_ctor', 'tii', agrPp)
				agrPoke(agrPp + 0xc, agrPp)
				agrSet(agrPp + 0x10, (agrCam[0] + agrLx * agrZd[1][agrK], agrCam[1] + 30000.0, agrCam[2] + agrLz *
					agrZd[1][agrK]))
				agrPts = agrPts + (agrPp,)
			agrPoke(agrZs + 0x88 * agrZd[0] + 0x80, agrA + 4)
			agrPoke(agrZs + 0x88 * agrZd[0] + 0x84, agrN)
		def agrHeld(a=0):
			r = ()
			for p in agrPts:
				t = agrI(p + 8)
				r = r + ((t == agrW) + 2 * (t == agrW2) + 4 * (t != 0 and t != agrW and t != agrW2),)
			return r
		def agrFind(minD, maxD, flag, seed):
			agrSrand(seed)
			r = SDTest.Call('cAICharGroup_FindRepos', 'tbiFiffi', agrFg, (0.0, 0.0, 0.0), agrW, minD, maxD, flag)
			return (r[0], r[0] and r[1], agrHeld())
		check('AGR FindRepos', lambda: (agrFind(0.0, 1.0e6, 0, 77), agrFind(0.0, 1.0e6, 0, 78), agrFind(0.0, 1.0e6, 1, 79),
			agrFind(0.0, 1.0e6, 1, 80), agrFind(1.0e5, 1.0e6, 1, 81), agrFind(0.0, 100.0, 1, 82),
			agrFind(0.0, 100.0, 0, 83), agrFind(0.0, 100.0, 0, 84)))
		agrPutStr(agrFakes[0][1] + 0x100, 'SDAgrNone')
		agrPoke(agrFakes[0][1] + 4, agrFakes[0][1] + 0x100)
		check('AGR FindRepos no zone', lambda: agrFind(0.0, 1.0e6, 0, 85))
		agrPoke(agrFakes[0][1] + 4, agrStrs[0])
		def agrGet(name, tgt):
			r = SDTest.Call('cAICharGroup_GetReposPos', 'tbiFsFi', agrFg, (1.0, 2.0, 3.0), name, (0.0, 0.0, 0.0), tgt)
			return (r[0], r[0] and r[2], agrHeld())
		check('AGR GetReposPos', lambda: (agrGet('SDAgrOther', 0), agrGet('SDAgrOther', agrW2), agrGet('SDAgrOther',
			agrW2), agrGet('SDAgrOther', agrW2), agrGet('SDAgrZone', 0), agrGet('SDAgrNo', agrW2)))
		for agrPp in agrPts:
			if agrI(agrPp + 8):
				SDTest.Call('Nto1Node_UnlinkOpt', 'tvii', 0, agrPp)
		check('AGR repos released', lambda: (agrHeld(), agrI(agrW + 0x4bc) == agrW + 0x4bc))
		for agrK in (0, 1):
			SDTest.Call('cAICGPoint_Delete', 'tiii', agrI(agrZs + 0x88 * agrK + 0x80), 3)
		agrFree(agrZs)
		agrFree(agrFg)

	# ---- the AIs of the classes on SDAgrWalk, the target SDAgrWalk2 ----
	agrArg = agrAlloc(8)
	agrAIs = {}
	for agrT in (('Desktop', sym('cAIDesktop_ctor'), 0x380), ('Gear', sym('cAIGear_ctor'), 0x390), ('Human', sym('cAIHuman_ctor'), 0x380),
			('Mayor', sym('cAIMayor_ctor'), 0x380), ('Messenger', sym('cAIMessenger_ctor'), 0x390), ('Nurse', sym('cAINurse_ctor'), 0x384),
			('Police', sym('cAIPolice_ctor'), 0x380), ('Sentinel', sym('cAISentinel_ctor'), 0x380), ('Bishop', sym('cAIBishop_ctor'), 0x398),
			('BankDirector', sym('cAIBankDirector_ctor'), 0x384), ('BankMaster', sym('cAIBankMaster_ctor'), 0x384)):
		agrSrand(900 + len(agrAIs))
		agrM = agrNew(agrT[2])
		agrAi = SDTest.Call(agrT[1], 'tiiii', agrM, agrW, agrArg)[0]
		agrPokeB(agrAi + 0x370, 0)
		agrAIs[agrT[0]] = agrAi
		agrOwn = ()
		if agrT[0] in ('Gear', 'Bishop'):
			agrOwn = (agrI(agrAi + 0x380), agrB(agrAi + 0x384))
		elif agrT[0] == 'Nurse':
			agrOwn = (agrB(agrAi + 0x380),)
		check('AGR ctor %s' % agrT[0], lambda m=agrM, a=agrAi, s=agrOwn: (a == m, agrI(a) - 0x400000,
			agrI(agrI(a + 0x19c)) - 0x400000, agrI(a + 0x198) == agrW, agrInts(a, (0x1a0, 0x1a4)), agrB(a + 0x35c),
			agrB(a + 0x48), agrB(a + 0x83), agrI(a + 0x44) == a, agrI(agrI(a + 0x19c) + 0x44) == a, s))
		SDTest.Call('Nto1Head_Link', 'tvii', agrW2 + 0x254, agrAi + 0x38)
	agrCtrl = agrW + 0x4ec
	# the target link of an AI back to SDAgrWalk2 (a change of state gives it up)
	def agrRelink(a):
		if agrI(a + 0x40) != agrW2:
			if agrI(a + 0x40):
				SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', a + 0x38)
			SDTest.Call('Nto1Head_Link', 'tvii', agrW2 + 0x254, a + 0x38)
	def agrAct(a=0):
		return agrB(agrCtrl + 0x18)
	def agrDists(a):
		return agrF(a + 0x1b0, 4)
	# the distances by state
	for agrT in (('Gear', sym('cAIGear_SetDists')), ('Human', sym('cAIHuman_SetDists')), ('Mayor', sym('cAIMayor_SetDists')), ('Messenger', sym('cAIMessenger_SetDists')),
			('Nurse', sym('cAINurse_SetDists')), ('Police', sym('cAIPolice_SetDists')), ('Sentinel', sym('cAISentinel_SetDists'))):
		for agrS0 in (0, 2, 3, 6, 7):
			agrA = agrAIs[agrT[0]]
			agrPoke(agrA + 0x1a0, agrS0)
			agrSet(agrA + 0x1b0, (1.0, 2.0, 3.0, 4.0))
			if agrT[0] == 'Messenger':
				agrSet(agrA + 0x38c, (1234.0,))
			check('AGR SetDists %s %d' % (agrT[0], agrS0), lambda a=agrA, f=agrT[1]: (SDTest.Call(f, 'tvi', a),
				agrDists(a)))
		agrPoke(agrA + 0x1a0, 0)
	# the shot parameters (the target without an AI), then the distances of state 2
	for agrT in (('Gear', sym('cAIGear_SetShoot'), (0x388, 0x38c)), ('Messenger', sym('cAIMessenger_SetShoot'), (0x380, 0x384, 0x388, 0x38c))):
		agrA = agrAIs[agrT[0]]
		agrPoke(agrA + 0x1a0, 2)
		check('AGR SetShoot %s' % agrT[0], lambda a=agrA, t=agrT: (SDTest.Call(t[1], 'tvi', a), agrI(a + t[2][0]),
			agrF(a + t[2][1], len(t[2]) - 1), agrDists(a)))
		agrPoke(agrA + 0x1a0, 0)
	# the action checks: (actMin, actMax, direct, priority, walk flag)
	for agrT in (('Mayor', sym('cAIMayor_CheckActDist')), ('Police', sym('cAIPolice_CheckActDist')), ('Sentinel', sym('cAISentinel_CheckActDist'))):
		agrA = agrAIs[agrT[0]]
		agrC = agrI(agrA + 0x19c)
		for agrV in ((100.0, 300.0, 1, 0x10, 1), (250.0, 300.0, 1, 0x10, 1), (100.0, 150.0, 1, 0x10, 1),
				(100.0, 300.0, 0, 0x10, 1), (100.0, 300.0, 1, 0x66, 1), (100.0, 300.0, 1, 0x65, 0),
				(200.0, 200.0, 1, 0x10, 1)):
			agrSet(agrA + 0x1b0, (agrV[0], agrV[1]))
			agrPokeB(agrC + 0x1d0, agrV[2])
			agrPoke(agrFakes[0][2] + 0x64, agrV[3])
			agrPokeB(agrFakes[0][2] + 0x49, agrV[4])
			agrPokeB(agrCtrl + 0x18, 7)
			check('AGR CheckActDist %s %s' % (agrT[0], fmt(agrV)), lambda a=agrA, f=agrT[1]: (SDTest.Call(f, 'tvi', a),
				agrAct(), SDTest.Call('cAIChar_GetDirect2', 'tbi', a)[0]))
	# the nurse at a dazed target
	agrA = agrAIs['Nurse']
	agrPokeB(agrFakes[0][2] + 0x49, 1)
	agrPoke(agrFakes[0][2] + 0x64, 0x10)
	for agrV in (('Dazed', 150.0, 0), ('Dazed', 150.0, 1), ('Dazed', 250.0, 0), ('Dazed', 250.0, 1), ('', 150.0, 1),
			('Relax', 150.0, 0)):
		agrAction(agrW2, agrV[0])
		agrSet(agrA + 0x1b4, (agrV[1],))
		agrPokeB(agrA + 0x380, agrV[2])
		check('AGR CheckActDist Nurse %s' % fmt(agrV), lambda a=agrA: (SDTest.Call('cAINurse_CheckActDist', 'tvi', a), agrAct(),
			agrB(a + 0x380)))
	agrAction(agrW2, '')
	check('AGR Nurse SetState', lambda a=agrA: (agrPokeB(a + 0x380, 0), SDTest.Call('cAINurse_SetState', 'bii', a, 0)[0],
		agrInts(a, (0x1a0, 0x1a4)), agrB(a + 0x380), SDTest.Call('cAINurse_SetStateIntern', 'bii', a, 0)[0], agrInts(a, (0x1a0,
		0x1a4)), agrB(a + 0x380)))
	# the shots of the gear and the messenger: (standTime, shootAng, shootFactor, priority, act before)
	agrA = agrAIs['Gear']
	agrC = agrI(agrA + 0x19c)
	agrPokeB(agrC + 0x1d0, 1)
	for agrV in ((3.0, 1.0, 0, 0x10, 0), (3.0, 1.5, 0, 0x10, 0), (0.5, 1.5, 0, 0x10, 1), (3.0, 1.5, 3, 0x10, 0),
			(3.0, 1.5, 0, 0x66, 1), (3.0, 1.5, 2, 0x10, 1)):
		agrSet(agrA + 0x1b0, (100.0, 300.0))
		agrSet(agrA + 0x358, (agrV[0],))
		agrSet(agrA + 0x38c, (agrV[1],))
		agrPoke(agrA + 0x388, agrV[2])
		agrPoke(agrFakes[0][2] + 0x64, agrV[3])
		agrPokeB(agrCtrl + 0x18, agrV[4])
		agrPoke(agrA + 0x380, 0)
		agrSrand(31)
		check('AGR CheckActDist Gear %s' % fmt(agrV), lambda a=agrA: (SDTest.Call('cAIGear_CheckActDist', 'tvi', a), agrAct(),
			agrB(a + 0x384), agrI(a + 0x380) == agrI(agrW2 + 0x238), agrF(a + 0x358, 1)))
	agrA = agrAIs['Messenger']
	agrC = agrI(agrA + 0x19c)
	agrPokeB(agrC + 0x1d0, 1)
	for agrV in ((1.0, 0, 0x10, 0, 1, 0.5), (1.6, 0, 0, 0, 1, 0.5), (1.6, 0, 0x10, 0, 1, 0.5), (1.6, 0, 0, 1, 1, 0.5),
			(1.6, 0, 0, 1, 1, 2.0), (1.6, 0, 0, 0, 0, 0.5), (1.6, 3, 0, 0, 1, 0.5)):
		agrSet(agrA + 0x1b0, (100.0, 300.0))
		agrSet(agrA + 0x384, (agrV[0], 1.0))
		agrPoke(agrA + 0x380, agrV[1])
		agrPoke(agrFakes[0][2] + 0x64, agrV[2])
		agrPokeB(agrCtrl + 0x18, agrV[3])
		agrPokeB(agrFakes[0][2] + 0x49, agrV[4])
		agrSet(agrA + 0x354, (agrV[5],))
		agrSrand(37)
		check('AGR CheckActDist Messenger %s' % fmt(agrV), lambda a=agrA: (SDTest.Call('cAIMessenger_CheckActDist', 'tvi', a), agrAct(),
			agrF(a + 0x354, 1)))
	agrPoke(agrFakes[0][2] + 0x64, 0x10)
	agrPokeB(agrFakes[0][2] + 0x49, 0)
	# the desktop: (state, walk, strafe, action)
	agrA = agrAIs['Desktop']
	for agrV in ((5, 0.0, 0.0, ''), (5, 0.0, 0.5, ''), (6, 1.0, 0.0, ''), (6, 0.5, 0.0, ''), (1, 0.0, 0.0, ''),
			(5, 0.0, 0.0, 'Fall'), (5, 0.0, 0.0, 'ActUpR'), (5, 0.0, 0.0, 'ActUp')):
		agrPoke(agrA + 0x1a0, agrV[0])
		agrSet(agrCtrl + 0xc, (agrV[1], agrV[2]))
		agrAction(agrW, agrV[3])
		agrPokeB(agrCtrl + 0x18, 7)
		check('AGR Desktop UpdateAction0 %s' % fmt(agrV), lambda a=agrA: (SDTest.Call('cAIDesktop_UpdateAction0', 'tvi', a), agrAct()))
	agrAction(agrW, '')
	agrPoke(agrA + 0x1a0, 0)
	agrSet(agrCtrl + 0xc, (0.0, 0.0))
	# the gear blocked by a paused AI it ran into
	agrA = agrAIs['Gear']
	agrC = agrI(agrA + 0x19c)
	agrHasColl = agrI(agrW + 0x3e8 + 0xc) != 0
	check('AGR Gear UpdateAction free', lambda a=agrA, c=agrC: (agrPokeB(agrCtrl + 0x18, 0), agrPokeB(c + 0xd4, 0),
		SDTest.Call('cAIGear_UpdateAction', 'tvi', a), agrAct(), agrPokeB(c + 0xd4, 1), SDTest.Call('cAIGear_UpdateAction', 'tvi', a), agrAct()))
	if agrHasColl:
		SDTest.Call('Nto1Head_Link', 'tvii', agrW2 + 0x254, agrW + 0x3e8)
		agrPoke(agrW2 + 0x78, agrAIs['Human'])
		for agrV in ((0, 0), (0, 1), (1, 1)):
			agrPoke(agrA + 0x1a0, agrV[0])
			agrPokeB(agrAIs['Human'] + 0x84, agrV[1])
			agrPokeB(agrCtrl + 0x18, 0)
			check('AGR Gear UpdateAction %s' % fmt(agrV), lambda a=agrA: (SDTest.Call('cAIGear_UpdateAction', 'tvi', a), agrAct()))
		agrPokeB(agrAIs['Human'] + 0x84, 0)
		agrPoke(agrW2 + 0x78, 0)
		agrPoke(agrA + 0x1a0, 0)
		SDTest.Call('Nto1Node_UnlinkChecked@6af70', 'tvi', agrW + 0x3e8)
	agrPokeB(agrC + 0xd4, 0)
	check('AGR Gear IsBlocked', lambda a=agrA, c=agrC: (agrPokeB(a + 0x384, 1), SDTest.Call('cAIGear_IsBlocked', 'tbi', a)[0],
		SDTest.Call('cAIControlGear_Vf89', 'tbi', c)[0], agrPokeB(a + 0x384, 0), SDTest.Call('cAIControlGear_Vf89', 'tbi', c)[0]))

	# ---- cAIMove on the AI of the gear ----
	check('AGR AIMove getters', lambda a=agrA: (SDTest.Call('cAIMove_CanJump', 'tbi', a)[0], SDTest.Call('cAIMove_GetTargetEnt', 'tii', a)[0]
		== agrW2, SDTest.Call('cAIMove_GetTarget', 'tii', a)[0] == a + 0x38, SDTest.Call('cAIMove_GetStopped', 'tbi', a)[0],
		SDTest.Call('cAIMove_GetArrived', 'tbi', a)[0], SDTest.Call('cAIMove_IsPaused', 'tbi', a)[0], SDTest.Call('cAIMove_Event', 'tbii', a, 3)[0],
		SDTest.Call('cAIMove_CanTarget', 'tbii', a, 0)[0], SDTest.Call('cAIMove_CanTarget', 'tbii', a, agrW2)[0] == agrB(agrW2 + 0x4c)))
	def agrVarS(a, name, set):
		r = SDTest.Call('cAIMove_VarS', 'tbispi', a, name, 0, set)
		return (r[0], r[0] and SDTest.Peek(r[1], '@s'))
	check('AGR AIMove Var', lambda a=agrA: (agrVarS(a, 'Target', 0), agrVarS(a, 'Target', 1), agrVarS(a, '', 0),
		agrVarS(a, 'Targets', 0), agrVarS(a, 'Ta', 0), SDTest.Call('cAIMove_VarI', 'tbispi', a, '', 3, 0)[0],
		SDTest.Call('cAIMove_VarI', 'tbispi', a, 'SDAgrX', 3, 0)[0], SDTest.Call('cAIMove_VarF', 'tbisDi', a, '', 1.5, 0)[0],
		SDTest.Call('cAIMove_VarF', 'tbisDi', a, 'SDAgrX', 1.5, 0)[0], SDTest.Call('cAIMove_VarV', 'tbisiiii', a, '', 0, 0, 0, 0)[0]))
	check('AGR AIMove CheckStopped', lambda a=agrA: (agrSet(a + 0x8c, (5.0,)), SDTest.Call('cAIMove_CheckStopped', 'tvif', a, 0.5),
		agrF(a + 0x8c, 1), agrB(a + 0x82)))
	agrIs = agrI(agrA + 0x1a4)
	agrPoke(agrA + 0x1a4, 10)
	check('AGR AIMove CheckStopped moving', lambda a=agrA: (agrSet(a + 0x8c, (2.0,)), agrPokeB(a + 0x82, 1),
		SDTest.Call('cAIMove_CheckStopped', 'tvif', a, 0.5), agrF(a + 0x8c, 1), agrB(a + 0x82)))
	agrPoke(agrA + 0x1a4, agrIs)
	check('AGR AIMove Update', lambda a=agrA: (SDTest.Call('cAIMove_Update', 'tvif', a, 0.5), agrF(a + 0xc, 1),
		SDTest.Call('cAIMove_UpdateOff', 'tvif', a, 0.25), agrF(a + 0xc, 1)))
	agrPokeB(agrA + 0x48, 1)
	for agrV in ((50.0, 61), (0.5, 62), (0.0, 63)):
		agrSrand(agrV[1])
		check('AGR AIMove RandomPoint %s' % fmt(agrV), lambda a=agrA, r=agrV[0]: SDTest.Call('cAIMove_RandomPoint', 'tiiFFf', a,
			(0.0, 0.0, 0.0), agrAdd(agrP, (0.0, 500.0, 0.0)), r)[1])
	agrPokeB(agrA + 0x48, 0)
	agrMv = agrAlloc(0x90)
	check('AGR AIMove ctorDefault dtor', lambda: (SDTest.Call('cAIMove_ctorDefault', 'tii', agrMv)[0] == agrMv, agrI(agrMv) - 0x400000,
		agrI(agrMv + 0x38) == agrMv + 0x38, agrInts(agrMv, (0x40, 0x44)), SDTest.Call('cAIMove_dtor', 'tvi', agrMv),
		agrI(agrMv) - 0x400000, agrI(agrMv + 0x38) == agrMv + 0x38))
	agrFree(agrMv)

	# ---- the move controls ----
	for agrT in (('Gear', sym('cAIControlGear_SetTurnSpeed')), ('Messenger', sym('cAIControlMessenger_SetTurnSpeed'))):
		agrC = agrI(agrAIs[agrT[0]] + 0x19c)
		for agrV in (0.3, 0.55, 0.56, 0.8, 1.0):
			check('AGR SetTurnSpeed %s %s' % (agrT[0], fmt(agrV)), lambda c=agrC, f=agrT[1], v=agrV: (SDTest.Call(f,
				'tvif', c, v), agrF(c + 0x8c, 2), agrF(c + 0x204, 2)))
	agrC = agrI(agrAIs['Messenger'] + 0x19c)
	check('AGR Messenger UpdateFollowMode', lambda c=agrC: (agrI(c + 0x120), SDTest.Call('cAIControlMessenger_UpdateFollowMode', 'tbif', c, 0.5)[0]))
	agrC = agrI(agrAIs['Gear'] + 0x19c)
	agrAction(agrW, 'PostFusion')
	for agrF0 in (sym('cAIControlGear_TurnFollow'), sym('cAIControlGear_Turn'), sym('cAIControlGear_DirTurn')):
		agrSet(agrCtrl, (1.0, 2.0))
		check('AGR Gear turn %x' % agrF0, lambda c=agrC, f=agrF0: (SDTest.Call(f, 'tvi', c), agrF(agrCtrl, 2)))
	agrAction(agrW, '')
	# the walk of the bank director and of the nurse: (walkDir, far, near, orbitSide)
	agrPokeB(agrCtrl + 0x18, 0)
	for agrT in (('BankDirector', sym('cAIControlBank_Walk')), ('BankMaster', sym('cAIControlBank_Walk@82340')), ('Nurse', sym('cAIControlNurse_Walk'))):
		agrA = agrAIs[agrT[0]]
		agrC = agrI(agrA + 0x19c)
		agrRelink(agrA)
		agrPoke(agrC + 0x1b4, agrA + 0x38)
		agrSet(agrC + 0x5c, (0.0,))
		agrSet(agrC + 0xf4, (0.75,))
		agrPokeB(agrC + 0xd4, 0)
		agrPokeB(agrC + 0x200, 0)
		for agrV in ((0, 150.0, 50.0, 0), (0, 300.0, 250.0, 0), (1, 300.0, 50.0, 0), (1, 150.0, 50.0, 0),
				(2, 300.0, 150.0, 0), (2, 300.0, 250.0, 0), (1, 150.0, 50.0, 1), (0, 150.0, 50.0, 2)):
			agrPoke(agrC + 0x94, agrV[0])
			agrSet(agrC + 0x1bc, (agrV[1], agrV[2]))
			agrPoke(agrC + 0x20c, agrV[3])
			agrSet(agrC + 4 + 4, (100.0,))
			agrSet(agrCtrl + 0xc, (9.0,))
			check('AGR Walk %s %s' % (agrT[0], fmt(agrV)), lambda c=agrC, f=agrT[1]: (SDTest.Call(f, 'tvi', c),
				agrI(c + 0x94), agrF(agrCtrl + 0xc, 1)))
		agrPoke(agrC + 0x20c, 0)
		agrPoke(agrC + 0x94, 0)
		agrPoke(agrC + 0x1b4, 0)
	# the strafe parameters by the target
	for agrT in (('Bishop', sym('cAIControlBishop_SetParams')), ('Gear', sym('cAIControlGear_SetParams'))):
		agrA = agrAIs[agrT[0]]
		agrC = agrI(agrA + 0x19c)
		check('AGR SetParams %s' % agrT[0], lambda a=agrA, c=agrC, f=agrT[1]: (agrSet(c + 0x37c, (1.0, 2.0)),
			agrPoke(c + 0x1b4, 0), SDTest.Call(f, 'tvi', c), agrF(c + 0x37c, 2), agrPoke(c + 0x1b4, a + 0x38),
			SDTest.Call(f, 'tvi', c), agrF(c + 0x37c, 2)))
	# the strafe of the bishop, the gear and the sentinel: (target class, target action, own action, walkDir, time,
	# strafeAng, side, other, lifeMark above the life)
	for agrT in (('Bishop', sym('cAIControlBishop_Strafe'), sym('cAIControlShoot_Walk')), ('Gear', sym('cAIControlGear_Strafe'), sym('cAIControlShoot_Walk@83940')), ('Sentinel', sym('cAIControlSentinel_Strafe'), sym('cAIControlSentinel_Walk'))):
		agrA = agrAIs[agrT[0]]
		agrC = agrI(agrA + 0x19c)
		agrRelink(agrA)
		agrPoke(agrC + 0x1b4, agrA + 0x38)
		agrSet(agrC + 0x5c, (0.25,))
		agrSet(agrC + 0xf4, (0.75,))
		if agrT[0] != 'Sentinel':
			agrSet(agrC + 0x37c, (3.0,))
		for agrV in (('Bishop', '', '', 1, 1.0, 2.0, 0, 0, 0), ('Bishop', '', '', 0, 1.0, 2.0, 1, 0, 0),
				('Bishop', '', '', 0, 1.0, 2.0, 2, 0, 0), ('Gear', '', '', 0, 5.0, 2.0, 0, 0, 0),
				('Dtritus', '', '', 0, 5.0, 1.0, 1, 0, 0), ('Bishop', 'Action', '', 1, 1.0, 2.0, 2, 1, 0),
				('Gear', 'ActionInit', '', 0, 5.0, 2.0, 1, 0, 0), ('Bishop', 'ActionEnd', '', 0, 1.0, 2.0, 1, 0, 0),
				('Bishop', '', 'ActionEnd', 0, 1.0, 2.0, 1, 0, 0), ('Bishop', '', 'ActionInit', 0, 1.0, 2.0, 1, 0, 0),
				('SDAgrX', '', '', 0, 1.0, 2.0, 1, 0, 0), ('SDAgrX', '', '', 0, 5.0, 2.0, 1, 0, 0),
				('SDAgrX', '', '', 0, 1.0, 2.0, 0, 0, 1), ('Bishop', '', '', 0, 1.0, 2.0, 1, 1, 0), ('', '', '', 0, 1.0,
				2.0, 1, 0, 0)):
			if agrV[0]:
				agrPutStr(agrFakes[1][1] + 0x100, agrV[0])
				agrPoke(agrFakes[1][1] + 4, agrFakes[1][1] + 0x100)
				agrPoke(agrW2 + 0x234, agrFakes[1][0])
			else:
				agrPoke(agrW2 + 0x234, 0)
			agrAction(agrW2, agrV[1])
			agrAction(agrW, agrV[2])
			agrPoke(agrC + 0x94, agrV[3])
			agrSet(agrC + 0x374, (agrV[4],))
			if agrT[0] != 'Sentinel':
				agrSet(agrC + 0x380, (agrV[5],))
			agrPoke(agrC + 0x370, agrV[6])
			agrPokeB(agrC + 0x378, agrV[7])
			agrPoke(agrC + 0xf8, agrI(agrW + 0x238) + 10 * agrV[8])
			agrSet(agrC + 0x360 + 4, (100.0,))
			agrSet(agrCtrl + 0x10, (9.0,))
			agrSrand(53)
			check('AGR Strafe %s %s' % (agrT[0], fmt(agrV)), lambda c=agrC, f=agrT[1]: (SDTest.Call(f, 'tvi', c),
				agrI(c + 0x370), agrF(c + 0x374, 1), agrB(c + 0x378), agrF(agrCtrl + 0x10, 1), agrI(c + 0xf8) ==
				agrI(agrW + 0x238)))
			agrPoke(agrW2 + 0x234, agrFakes[1][0])
		agrAction(agrW, '')
		agrAction(agrW2, '')
		agrPoke(agrFakes[1][1] + 4, agrStrs[1])
		check('AGR Strafe Walk %s' % agrT[0], lambda c=agrC, f=agrT[2]: (agrPoke(c + 0x370, 1), agrSet(agrCtrl + 0xc,
			(9.0,)), SDTest.Call(f, 'tvi', c), agrF(agrCtrl + 0xc, 1)))
		agrPoke(agrC + 0x370, 0)
		agrPokeB(agrC + 0x378, 1)
		agrPoke(agrC + 0x1b4, 0)

	# ---- the end: the AIs deleted (one by its destructor), the fakes freed, the characters deleted ----
	agrSet(agrCtrl, (0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
	check('AGR Desktop dtor', lambda a=agrAIs['Desktop']: (SDTest.Call('cAIDesktop_dtor', 'tvi', a), agrI(a) - 0x400000,
		SDTest.Call('operator_delete_sized', 'vii', a, 0x380)))
	for agrT in (('Gear', sym('cAIGear_Delete')), ('Human', sym('cAIHuman_Delete')), ('Mayor', sym('cAIMayor_Delete')), ('Messenger', sym('cAIMessenger_Delete')),
			('Nurse', sym('cAINurse_Delete')), ('Police', sym('cAIPolice_Delete')), ('Sentinel', sym('cAISentinel_Delete')), ('Bishop', sym('cAIBishop_Delete')),
			('BankDirector', sym('cAIBankDirector_Delete')), ('BankMaster', sym('cAIBankMaster_Delete'))):
		check('AGR Delete %s' % agrT[0], lambda a=agrAIs[agrT[0]], f=agrT[1]: SDTest.Call(f, 'tiii', a, 1)[0] == a)
	check('AGR target head', lambda: agrI(agrW2 + 0x254) == agrW2 + 0x254)
	if agrGroup:
		agrPoke(agrGroup + 0x5c, agrGroupTime)
	agrFree(agrArg)
	for agrE in (agrW, agrW2):
		agrPoke(agrE + 0x234, 0)
	for agrFk in agrFakes:
		for agrP2 in agrFk:
			agrFree(agrP2)
	agrFree(agrActName)
	agrFree(agrActName2)
	for agrE in (agrW, agrW2):
		check('AGR delete entity', lambda e=agrE: (SDTest.Call('PtrTable_Delete@208810', 'tbis', agrWorld + 4, SDTest.Peek(e + 4,
			'@p')), SDTest.Call(agrI(agrI(e) + 0xc) - 0x400000, 'tiii', e, 1)[0] == e)[1])
for agrP2 in agrStrs:
	agrFree(agrP2)
