# game leftovers below rva 0x1a8000, "Rest A" (execfile'd by sdlevel.py, Python 1.5.2). The vehicle AI (an AI of our
# own on the Car "SDRsaCar" made as in sdlevel_ivehicle.py): the track points of every kind of track on a traffic
# graph of our own (FZ has no tracks), the random track offsets after a fixed srand and the line tests of the hook
# points. The repositioning (Repos, ReposTraffic, GraphTraffic) is not called: it picks nodes near the camera by the
# traffic of the level, which differs between the runs. The network controller of a vehicle reading and writing
# crafted messages; on a second car the radar map and icons and the waypoint marks; the console, the debug draws,
# the sub menu logo and the virtual keyboard. The draws go through the D3D log with the viewport set.
def rsaI(a):
	return SDTest.Peek(a, '@i')
def rsaB(a):
	return SDTest.Peek(a, '@b')
def rsaF(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def rsaSet(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def rsaPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def rsaAlloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def rsaFree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def rsaSrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def rsaInts(a, offs):
	r = ()
	for o in offs:
		r = r + (rsaI(a + o),)
	return r
# a call with the D3D device replaced by the logging proxy: (its result, the D3D calls); the 2D draws CHECK the
# viewport, set to 640 x 480 for the call
def rsalog(fn):
	vp = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
	SDTest.Poke('g_ViewportW', 640)
	SDTest.Poke('g_ViewportH', 480)
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	SDTest.Poke('g_ViewportW', vp[0])
	SDTest.Poke('g_ViewportH', vp[1])
	return (r, SDTest.Out())
# the same with only the length of the log (the long logs of the font and debug draws)
# fn and the number of console lines it writes (the count before differs: the reference logs a video that does not
# open in the test prefix)
def rsaconlines(fn):
	start = SDTest.Peek('g_ConLine', 'i')
	r = fn()
	return (r, (SDTest.Peek('g_ConLine', 'i') - start) % 23)
def rsaloglen(fn):
	r = rsalog(fn)
	return (r[0], len(r[1]))
def rsaFind(name):
	w = SDTest.Peek('g_pWorld', 'i')
	slot = SDTest.Call('HashTable_Slot', 'tiis', w + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', w + 4, name, slot)[0]
	if node:
		return rsaI(node)
	return 0

# ---- the vehicle AI ----
rsaGroup = SDTest.Peek('g_VehicleGroup', 'i')
rsaTraffic = SDTest.Peek('g_NodesTraffic', 'i')
rsaCarObj = Scrap.CreateEntity('SDRsaCar', 54000.0, 50000.0, 61000.0, 'Car')
rsaCar = rsaFind('SDRsaCar')
check('SDRsaCar', lambda: (rsaCarObj is not None, rsaCar != 0, rsaGroup != 0, rsaTraffic != 0))
if rsaCar and rsaGroup and rsaTraffic:
	SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', rsaCar, 'SPoli1', 1.0, 1.0, 1.0, 1)
	rsaSet(rsaCar + 0xac, (0.0, 0.0, 0.0))
	rsaGroupSave = rsaInts(rsaGroup, (0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70))
	rsaCarSave = rsaInts(rsaCar, (0x420, 0x424, 0x428, 0x5e8))
	rsaP1 = rsaAlloc(0x2c)
	rsaP2 = rsaAlloc(0x18)
	rsaP3 = rsaAlloc(0x1c)
	rsaSet(rsaP1, (0.5, 0.25, 0.75))
	rsaPoke(rsaP1 + 0x10, 1)
	rsaPoke(rsaP1 + 0x1c, 3)
	rsaPoke(rsaP1 + 0x24, 40)
	rsaPoke(rsaP1 + 0x28, 60)
	rsaPoke(rsaP2 + 4, 2)
	rsaPoke(rsaP2 + 0xc, 4)
	rsaSet(rsaP2 + 0x10, (0.5,))
	rsaPoke(rsaP2 + 0x14, 1)
	rsaPoke(rsaP3, 50)
	rsaPoke(rsaP3 + 4, 3)
	rsaSet(rsaP3 + 8, (1.0, 0.5))
	rsaPoke(rsaP3 + 0x10, 35)
	rsaPoke(rsaP3 + 0x14, 50)
	rsaPoke(rsaP3 + 0x18, 15)
	rsaSrand(4321)
	rsaMem = SDTest.Call('operator_new', 'ii', 0x3b0)[0]
	rsaAI = SDTest.Call('cAIVehicle_ctor', 'tiiiiii', rsaMem, rsaCar, rsaP1, rsaP2, rsaP3)[0]
	check('cAIVehicle_ctor', lambda: rsaAI == rsaMem)
	rsaSave = rsaInts(rsaAI, (0x31c, 0x2e4, 0x2e8, 0x2ec, 0x320, 0x324, 0x328, 0x334))
	# the track points: offsets on 0..2, the plane cut of a light vehicle on 3 and 4 (tracks of FZ), the copy for the
	# others; then the random offsets
	# FZ has no traffic tracks: a graph of our own with two tracks of 6 nodes (the second a loop) in g_NodesTraffic
	# while the track points run
	rsaG = SDTest.Call('operator_new', 'ii', 0x70)[0]
	SDTest.Call('CNodesTraffic_ctor', 'tii', rsaG)
	rsaTr = SDTest.Call('Mem_Alloc', 'ii', 2 * 0x24)[0]
	rsaNodes = (((0.0, 0.0, 0.0), (2000.0, 100.0, 500.0), (4000.0, 0.0, 1500.0), (5000.0, -200.0, 3500.0),
		(5500.0, 0.0, 6000.0), (5000.0, 300.0, 9000.0)),
		((-1000.0, 50.0, -1000.0), (-3000.0, 50.0, -1500.0), (-5000.0, 400.0, -1000.0), (-6000.0, 50.0, 1000.0),
		(-4000.0, 50.0, 2500.0), (-2000.0, -300.0, 1500.0)))
	for rsaT in range(2):
		rsaNd = SDTest.Call('Mem_Alloc', 'ii', 6 * 12)[0]
		for rsaN in range(6):
			rsaSet(rsaNd + 12 * rsaN, rsaNodes[rsaT][rsaN])
		rsaA = rsaTr + 0x24 * rsaT
		for rsaK in range(9):
			rsaPoke(rsaA + 4 * rsaK, 0)
		rsaPoke(rsaA, rsaNd)
		rsaPoke(rsaA + 4, 6)
		rsaPoke(rsaA + 0xc, rsaT)
		rsaSet(rsaA + 0x10, (450.0 + 100.0 * rsaT, 800.0 - 500.0 * rsaT))
	rsaPoke(rsaG + 0xc, rsaTr)
	rsaPoke(rsaG + 0x24, 2)
	rsaTrafficSave = rsaTraffic
	SDTest.Poke('g_NodesTraffic', rsaG)
	rsaOutSave = SDTest.Peek('g_IsMapOutDoor', 'i')
	SDTest.Poke('g_IsMapOutDoor', 1)
	for rsaKind in (0, 1, 2, 3, 4, 5):
		for rsaVT in (0, 1):
			for rsaT in (0, 1):
				for rsaN in (1, 2, 4):
					rsaPoke(rsaAI + 0x31c, rsaKind)
					rsaPoke(rsaCar + 0x5e8, rsaVT)
					rsaSet(rsaAI + 0x2e4, (12.5, -3.0, 0.25))
					# the last point ahead of p along the node direction, so that the line back crosses the plane
					rsaA = SDTest.Call(rsaI(rsaI(rsaG) + 0x28) - 0x400000, 'tbiFFii', rsaG, (0.0, 0.0, 0.0),
						(0.0, 0.0, 0.0), rsaT, rsaN)[1]
					rsaP = rsaNodes[rsaT][rsaN]
					rsaSet(rsaAI + 0x320, (rsaP[0] + rsaA[0] * 1000.0, rsaP[1] + rsaA[1] * 1000.0,
						rsaP[2] + rsaA[2] * 1000.0))
					check('cAIVehicle_TrackPoint' + fmt((rsaKind, rsaVT, rsaT, rsaN)), lambda: (SDTest.Call('cAIVehicle_TrackPoint',
						'tiiFFii', rsaAI, (7.0, 8.0, 9.0), rsaP, rsaT, rsaN)[1], rsaF(rsaAI + 0x320, 3)))
					rsaSrand(77 + rsaN)
					check('cAIVehicle_TrackPointRandom' + fmt((rsaKind, rsaVT, rsaT, rsaN)), lambda: (
						SDTest.Call('cAIVehicle_TrackPointRandom', 'tiiFFii', rsaAI, (7.0, 8.0, 9.0), rsaP, rsaT, rsaN)[1],
						rsaF(rsaAI + 0x2e4, 3), rsaF(rsaAI + 0x320, 3)))
	SDTest.Poke('g_NodesTraffic', rsaTrafficSave)
	SDTest.Poke('g_IsMapOutDoor', rsaOutSave)
	check('fake traffic Delete', lambda: SDTest.Call('CNodesTraffic_Delete', 'tiii', rsaG, 1)[0] == rsaG)
	# the hook points around the vehicle at its place and turned
	for rsaRot in ((0.0, 0.0, 0.0), (0.5, 1.25, 0.0), (-1.5, 0.0, 2.0)):
		SDTest.Call(rsaI(rsaI(rsaCar) + 0x84) - 0x400000, 'tviF', rsaCar, rsaRot)
		for rsaM in (0, 1, 2, 7):
			rsaPoke(rsaAI + 0x334, rsaM)
			check('cAIVehicle_TestHookPoint' + fmt((rsaRot, rsaM)), lambda: (SDTest.Call('cAIVehicle_TestHookPoint', 'tbiF', rsaAI,
				(-1.0, -1.0, -1.0))[0:2], rsaI(rsaAI + 0x334)))
	for rsaK in range(8):
		rsaPoke(rsaAI + (0x31c, 0x2e4, 0x2e8, 0x2ec, 0x320, 0x324, 0x328, 0x334)[rsaK], rsaSave[rsaK])
	check('cAIVehicle_Delete', lambda: SDTest.Call('cAIVehicle_Delete', 'tiii', rsaAI, 1)[0] == rsaAI)
	rsaFree(rsaP1)
	rsaFree(rsaP2)
	rsaFree(rsaP3)
	for rsaK in range(6):
		rsaPoke(rsaGroup + (0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70)[rsaK], rsaGroupSave[rsaK])
	for rsaK in range(4):
		rsaPoke(rsaCar + (0x420, 0x424, 0x428, 0x5e8)[rsaK], rsaCarSave[rsaK])

# ---- the network controller of a vehicle on the car: the constructor, Changed (an active car always sends its
# position), Reset ----
if rsaCar:
	rsaNV = SDTest.Call('Mem_Calloc', 'ii', 0x78)[0]
	def rsanv(n):
		return (rsaF(n + 4, 13), rsaB(n + 0x38), rsaInts(n, (0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x5c, 0x64, 0x68,
			0x6c, 0x70)), rsaB(n + 0x54), rsaB(n + 0x59), rsaB(n + 0x60))
	check('cNetVController_ctor', lambda: (SDTest.Call('cNetVController_ctor', 'tiii', rsaNV, rsaCar)[0] == rsaNV,
		rsaI(rsaNV) - 0x400000, rsaI(rsaNV + 0x5c) == rsaCar, rsanv(rsaNV)[0:2]))
	rsaActive = rsaI(rsaCar + 0x4c)
	for rsav in ((0x7e, 1), (0x7e, 0), (0x00, 1), (0x01, 0), (0x7f, 1)):
		rsaPoke(rsaCar + 0x4c, (rsaActive & ~0xff) | rsav[1])
		check('cNetVController_Changed' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('cNetVController_Changed', 'tbiii', rsaNV, rsav[0],
			rsaCar)[0], rsaB(rsaNV + 0x38), rsaInts(rsaNV, (0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50))))
	rsaPoke(rsaCar + 0x4c, rsaActive)
	check('cNetVController_Reset', lambda: (SDTest.Call('cNetVController_Reset', 'tvi', rsaNV), rsaB(rsaNV + 0x54), rsaB(rsaNV + 0x59)))
	# Read of crafted messages through a cFileMem: two client states (a lock point, fire, cloud, EMI, a shot at
	# "net5", a hit by "net7", upgrades; then a lock on "net9" and a shot at a point) and the controls on a server
	rsaWorld = SDTest.Peek('g_pWorld', 'i')
	rsaNWeap = rsaI(rsaWorld + 0x1bb8)
	rsaNAmmo = rsaI(rsaWorld + 0x1bc4)
	rsaWS = rsaI(rsaCar + 0x5f0 + 0x18)
	rsaAm = rsaI(rsaCar + 0x5f0 + 0x20)
	def rsanvr():
		r = (rsaI(rsaNV + 0x74) & 0xffff, rsaF(rsaNV + 0x70, 1), rsaB(rsaNV + 0x38), rsaB(rsaNV + 0x60),
			rsaInts(rsaNV, (0x64, 0x68)), rsaF(rsaNV + 4, 9), rsaF(rsaCar + 0x290, 2), rsaF(rsaCar + 0x58c, 2),
			rsaB(rsaCar + 0x594), rsaInts(rsaCar, (0x238, 0x5fc)), rsaB(rsaCar + 0x5f0 + 0x44),
			rsaF(rsaCar + 0x5f0 + 0x38, 3), rsaB(rsaCar + 0x298 + 0x5c), rsaI(rsaCar + 0x298 + 0x40),
			rsaB(rsaCar + 0x298 + 0x74), rsaF(rsaCar + 0x298 + 0x30, 3), rsaB(rsaCar + 0x650), rsaB(rsaCar + 0x653))
		for k in range(rsaNAmmo):
			r = r + (rsaF(rsaAm + 4 * k, 1),)
		for k in range(rsaNWeap):
			r = r + (rsaI(rsaWS + 12 * k + 4), rsaF(rsaWS + 12 * k + 8, 1))
		return r
	def rsaword(l, k):
		return l[k] | (l[k + 1] << 8) | (l[k + 2] << 16) | (l[k + 3] << 24)
	rsaMsgF = rsaAlloc(0x20)
	rsaMsgB = rsaAlloc(0x100)
	SDTest.Call('cFileMem_ctor', 'tiiiii', rsaMsgF, rsaMsgB, 0x100, 0)
	def rsamsg(l):
		l = list(l)
		n = len(l)
		l = l + [0, 0, 0, 0]
		k = 0
		while k < n:
			rsaPoke(rsaMsgB + k, rsaword(l, k))
			k = k + 4
		SDTest.Call('cFileMem_SetBuffer', 'tviiiii', rsaMsgF, rsaMsgB, 0x100, 0, n)
		SDTest.Call('cFileMem_Open', 'tiisi', rsaMsgF, '', 1)
		return SDTest.Call('cNetVController_Read', 'tvii', rsaNV, rsaMsgF), SDTest.Call('cFileMem_Tell', 'tii', rsaMsgF)[0], rsanvr()
	rsaW = rsaI(rsaCar + 0x5fc) & 0xff
	rsaHits = (rsaB(rsaCar + 0x298 + 0x74) + 1) & 0xff
	# the burn and EMI times the messages set (0x290..0x294, 0x58c..0x590), put back after: with a burn time left the
	# next frame starts VehicleFire on the car, whose SetEntity reads the motors of a ship (NULL on a Car) and crashes
	# the original too
	rsaTimes = (rsaF(rsaCar + 0x290, 2), rsaF(rsaCar + 0x58c, 2))
	if rsaWS and rsaAm and rsaNWeap > 0:
		rsaSet(rsaCar + 0x5f0 + 0x38, (0.0, 0.0, 0.0))
		rsaPoke(rsaCar + 0x650, 0)
		check('cNetVController_Read before', lambda: rsanvr())
		check('cNetVController_Read client', lambda: rsamsg((0xf8, 0x40, 0x01, 1, 0, 0xfe, 0xff, 3, 0, 50, 0x30, 0,
			rsaW, 0x81, 5, 0, rsaHits, 7, 0, 0x05, 0x80, 0x03, 0, 4, 0x80, 0x21)))
		check('cNetVController_Read client 2', lambda: rsamsg((0x08, 0x20, 0x00, 9, 0, 0, 0, rsaW, 0x02, 2, 0, 0, 0,
			0xff, 0xff, rsaHits, 7, 0, 0, 0, 0x00)))
		rsaApp = SDTest.Peek('g_pApp', 'i')
		rsaAppFlags = rsaI(rsaApp + 0x30)
		rsaPoke(rsaApp + 0x30, rsaAppFlags | 2)
		check('cNetVController_Read server', lambda: rsamsg((0x01, 0x00, rsaW, 0, 0, 0xc0, 0x3f, 3, 0, 0x00)))
		rsaPoke(rsaApp + 0x30, rsaAppFlags)
		# Write into the cFileMem: the state for the clients (fire, a too small fire delay, the hit counters at
		# 2). The controls of a "net" vehicle need the client flag, then the base Write does not return
		def rsawr(flags, force):
			SDTest.Call('cFileMem_SetBuffer', 'tviiiii', rsaMsgF, rsaMsgB, 0x100, 0, 0)
			SDTest.Call('cFileMem_Open', 'tiisi', rsaMsgF, '', 6)
			SDTest.Call('cNetVController_Write', 'tviiiii', rsaNV, flags, rsaCar, rsaMsgF, force)
			n = SDTest.Call('cFileMem_Size', 'tii', rsaMsgF)[0]
			r = ()
			for k in range(n):
				r = r + (rsaB(rsaMsgB + k),)
			return r, rsanvr()
		rsaSet(rsaCar + 0x290, (0.5, 3.0))
		check('cNetVController_Write', lambda: rsawr(0x80, 0))
		rsaPoke(rsaNV + 0x64, 2)
		rsaPoke(rsaNV + 0x68, 2)
		rsaPoke(rsaNV + 0x6c, 1)
		rsaSet(rsaCar + 0x290, (0.005, 3.0))
		check('cNetVController_Write hits', lambda: rsawr(0xff, 1))
		check('cNetVController_Write no flags', lambda: rsawr(0x00, 0))
		rsaSet(rsaCar + 0x290, rsaTimes[0])
		rsaSet(rsaCar + 0x58c, rsaTimes[1])
	rsaFree(rsaMsgF)
	rsaFree(rsaMsgB)
	rsaFree(rsaNV)

# a second car for the draws below: the network tests left the first one dead and switched off
rsaCar2Obj = Scrap.CreateEntity('SDRsaCar2', 54000.0, 50000.0, 61000.0, 'Car')
rsaCar2 = rsaFind('SDRsaCar2')
check('SDRsaCar2', lambda: (rsaCar2Obj is not None, rsaCar2 != 0))

# ---- the radar map and icons on a Radar item, the car as target, a fake camera (its turn at 0x8c): the D3D calls,
# the view globals, the image states and the icon colors ----
rsaRad = 0
if rsaCar2 and rsaI(SDTest.Peek('g_pWorld', 'i') + 0x2f0) > 0 and SDTest.Call('Scorer_Get', 'ii', 0)[0]:
	# a Radar item of the scorer of player 0 (the exe's constructor), marked DelMe after; the draws go to the D3D log
	rsaSc = SDTest.Call('Scorer_Get', 'ii', 0)[0]
	rsaRadView = rsaF(0x400000 + sym('g_RadarScale'), 1) + rsaF(0x400000 + sym('g_RadarAngle'), 1) + rsaF(0x400000 + sym('g_RadarCenterX'), 1) + rsaF(0x400000 + sym('g_RadarCenterZ'), 1)
	SScorer.Add(0, 'SDRsaRadar', 'Radar')
	rsaRad = SDTest.Call('cScorer_FindItem', 'tiis', rsaSc, 'SDRsaRadar')[0]
	check('cRadarScorer item', lambda: rsaRad != 0)

if rsaRad and not (rsaI(rsaI(rsaRad + 0x8c8) + 0x3c) and rsaI(rsaI(rsaRad + 0xcc) + 0x3c)):
	rsaPoke(rsaRad + 0x28, rsaI(rsaRad + 0x28) | 1)
	rsaRad = 0
if rsaRad:
	rsaCam = rsaAlloc(0x100)
	rsaSet(rsaCam + 0x8c, (0.7,))
	rsaSet(rsaRad + 0x10, (100.0, 50.0))
	rsaSet(rsaRad + 0xac, (55.0, 55.0, 56.25, 56.25))
	# the last colors of the icons are not set by the constructor
	rsaPoke(rsaRad + 0x4cc + 4 * 3, 0)
	rsaPoke(rsaRad + 0x4cc + 4 * 5, 0)
	rsaPos = rsaAlloc(0x10)
	def rsaimg(p):
		return rsaF(p, 2), rsaI(p + 0x34)
	def rsarad():
		r = (rsaF(0x400000 + sym('g_RadarScale'), 1) + rsaF(0x400000 + sym('g_RadarAngle'), 1) + rsaF(0x400000 + sym('g_RadarCenterX'), 1) + rsaF(0x400000 + sym('g_RadarCenterZ'), 1), rsaimg(rsaI(rsaRad + 0x8c4)), rsaimg(rsaI(rsaRad + 0x8dc)),
			rsaimg(rsaI(rsaRad + 0xcc)))
		for k in (3, 5):
			r = r + (rsaI(rsaRad + 0x4cc + 4 * k),)
		return r
	def rsamap():
		return rsalog(lambda: SDTest.Call('cRadarScorer_DrawMap', 'tviii', rsaRad, rsaCar2, rsaCam)), rsarad()
	# a sprite less image fails the CHECK of CPsaImage_DrawPoly: the floor maps of FZ and the noise are missing,
	# so the view cone image stands in for them (put back before the item is deleted)
	rsaRadImgs = rsaInts(rsaRad, (0x8c4,) + tuple(range(0x8dc, 0x91c, 4)))
	rsaCone = rsaI(rsaRad + 0x8c8)
	check('cRadarScorer sprites', lambda: (rsaI(rsaCone + 0x3c) != 0, rsaI(rsaI(rsaRad + 0xcc) + 0x3c) != 0))
	rsaPoke(rsaRad + 0x8c4, rsaCone)
	for rsaK in range(16):
		rsaPoke(rsaRad + 0x8dc + 4 * rsaK, rsaCone)
	rsaSet(rsaRad + 0x8d4, (1.0,))
	check('cRadarScorer_DrawMap', lambda: rsamap())
	rsaSet(rsaRad + 0x8d4, (0.4,))
	check('cRadarScorer_DrawMap fade', lambda: rsamap())
	rsaPoke(rsaRad + 0x934, 1)
	rsaSet(rsaRad + 0x944, (300.0, -200.0))
	check('cRadarScorer_DrawMap menu', lambda: rsamap())
	rsaPoke(rsaRad + 0x934, 0)
	rsaSet(rsaRad + 0x944, (0.0, 0.0))
	rsaEmi = rsaF(rsaCar2 + 0x58c, 1)
	rsaSet(rsaCar2 + 0x58c, (5.0,))
	rsaSrand(17)
	check('cRadarScorer_DrawMap EMI', lambda: rsamap())
	rsaSet(rsaCar2 + 0x58c, rsaEmi)
	rsalog(lambda: SDTest.Call('cRadarScorer_DrawMap', 'tviii', rsaRad, rsaCar2, rsaCam))
	rsaPoke(rsaRad + 0xd4 + 4 * 3, 0)
	rsaPoke(rsaRad + 0xd4 + 4 * 5, 0)
	def rsaicon(pos, ang, flags, always):
		rsaSet(rsaPos, pos)
		return rsalog(lambda ang=ang, flags=flags, always=always: SDTest.Call('cRadarScorer_DrawIcon', 'tviiifiiiii', rsaRad, rsaPos,
			3, ang, 5, flags, 0xc0ff8040, 0x80ffffff, always)), rsarad()
	for rsaFade in (1.0, 0.25):
		rsaSet(rsaRad + 0x8d4, (rsaFade,))
		for rsaMenu in (0, 1):
			rsaPoke(rsaRad + 0x934, rsaMenu)
			for rsav in (((54100.0, 50000.0, 61200.0), 0.3, 0, 0), ((54100.0, 50000.0, 61200.0), 0.3, 0, 1),
					((54100.0, 1.0e6, 61200.0), 0.3, 0, 0), ((54100.0, -1.0e6, 61200.0), 0.3, 0, 1),
					((94000.0, 50000.0, 91000.0), 0.3, 0x10, 0), ((94000.0, 50000.0, 91000.0), 0.3, 0x01, 0),
					((24000.0, 50000.0, 21000.0), 1.2, 0x02, 1), ((94000.0, 50000.0, 21000.0), 0.3, 0x0c, 0),
					((14000.0, 50000.0, 81000.0), 0.3, 0x0f, 1), ((54000.0, 50000.0, 151000.0), -2.0, 0x04, 0)):
				check('cRadarScorer_DrawIcon' + fmt((rsaFade, rsaMenu) + rsav), lambda rsav=rsav: rsaicon(rsav[0],
					rsav[1], rsav[2], rsav[3]))
	rsaPoke(rsaRad + 0x8c4, rsaRadImgs[0])
	for rsaK in range(16):
		rsaPoke(rsaRad + 0x8dc + 4 * rsaK, rsaRadImgs[1 + rsaK])
	rsaPoke(rsaRad + 0x28, rsaI(rsaRad + 0x28) | 1)
	rsaSet(0x400000 + sym('g_RadarScale'), rsaRadView[0:1])
	rsaSet(0x400000 + sym('g_RadarAngle'), rsaRadView[1:2])
	rsaSet(0x400000 + sym('g_RadarCenterX'), rsaRadView[2:3])
	rsaSet(0x400000 + sym('g_RadarCenterZ'), rsaRadView[3:4])
	rsaFree(rsaCam)
	rsaFree(rsaPos)
# ---- the waypoint mark of the car on a cWaypointScorer of our own: current or not, split screen, players 0 and 1
# (the camera of player 1 does not exist: early return); where the car is on the screen decides the branch ----
if rsaCar2:
	rsaWp = rsaAlloc(0xc4)
	SDTest.Call('cWaypointScorer_ctor', 'tii', rsaWp)
	# only frame 0 of the white sheet: the rectangle, arrow and dot frames
	rsaPoke(rsaWp + 0xb4, 0)
	rsaPoke(rsaWp + 0xb8, 0)
	rsaPoke(rsaWp + 0xbc, 0)
	rsaSplit = SDTest.Peek('g_SplitScreenGame', 'i')
	def rsawp(current, split, player):
		SDTest.Poke('g_SplitScreenGame', split)
		rsaPoke(rsaWp + 0x30, player)
		rsaSet(rsaWp + 0xac, (0.0,))
		d3d = rsalog(lambda current=current: SDTest.Call('cWaypointScorer_DrawTarget', 'tviii', rsaWp, rsaCar2, current))
		img = rsaI(rsaWp + 0xc0)
		return d3d, rsaF(rsaWp + 0xac, 1), rsaB(rsaWp + 0xb0), rsaF(img, 6), rsaI(img + 0x34)
	for rsav in ((0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 1), (0, 1, 1)):
		check('cWaypointScorer_DrawTarget' + fmt(rsav), lambda rsav=rsav: rsawp(rsav[0], rsav[1], rsav[2]))
	SDTest.Poke('g_SplitScreenGame', rsaSplit)
	SDTest.Call('cWaypointScorer_dtor', 'tvi', rsaWp)
	rsaFree(rsaWp)
if rsaCarObj:
	check('SDRsaCar Del', lambda: rsaCarObj.Del())
if rsaCar2Obj:
	check('SDRsaCar2 Del', lambda: rsaCar2Obj.Del())

# ---- the early returns of the voices, the sound debug view, the messenger drop and the gear aim search ----
rsaRemain = SDTest.Peek('g_RemainTalkTime', 'i')
SDTest.Call('crt_memcpy', 'iiFi', 0x400000 + sym('g_RemainTalkTime'), (1.5,), 4)
check('Sound_PlayVoice talk', lambda: SDTest.Call('Sound_PlayVoice', 'bssii', 'Nobody', 'AnmStr', 0, 1)[0])
SDTest.Poke('g_RemainTalkTime', rsaRemain)
check('Sound_PlayVoice no alarm', lambda: SDTest.Call('Sound_PlayVoice', 'bssii', 'Nobody', 'Fire', 0, 0)[0])
check('Sound_UpdateVoice', lambda: SDTest.Call('Sound_UpdateVoice', 'vi', 1))
rsaShow = SDTest.Peek('g_S_Show', 'i')
SDTest.Poke('g_S_Show', 0)
check('CSoundXBox_DebugDraw off', lambda: SDTest.Call('CSoundXBox_DebugDraw', 'tviii', SDTest.Peek('g_pSound', 'i'), 10, 20))
SDTest.Poke('g_S_Show', rsaShow)
check('Messenger_Deliver none', lambda: SDTest.Call('Messenger_Deliver', 'biii', 0, 0, 0)[0])
# the sphere callback with the name of no entity in a memory file
rsaFBuf = rsaAlloc(0x100)
rsaFM = rsaAlloc(0x20)
SDTest.Call('cFileMem_ctor', 'tiiiii', rsaFM, rsaFBuf, 0x100, 0)
SDTest.Call('cFileMem_Open', 'tiisi', rsaFM, '', 4)
SDTest.Call('cAbstractFile_WriteString', 'tiis', rsaFM, 'SDRsaNoSuchEntity')
SDTest.Call('cFileMem_Open', 'tiisi', rsaFM, '', 0)
rsaTgt = SDTest.Peek('g_GearAimTarget', 'i')
check('GearAim_SphereFunc unknown', lambda: (SDTest.Call('GearAim_SphereFunc', 'vi', rsaFM), SDTest.Peek('g_GearAimTarget', 'i') == rsaTgt))
rsaFree(rsaFM)
rsaFree(rsaFBuf)

# ---- the full screen console: the console log of ShowConsoleLog, the console open (its time far back so it
# stands still) with a ":name<tab>" completion, the close once the slide is over ----
def rsastr(a):
	r = ''
	for k in range(0x40):
		c = rsaB(a + k)
		if not c:
			break
		r = r + chr(c)
	return r
def rsacon():
	w = rsaI(0x400000 + sym('g_ConsoleWhite'))
	r = (SDTest.PeekSpan('g_ConsoleOn', 0, 4), rsastr(0x400000 + sym('g_PlayerInput') + 0x8))
	if w:
		r = r + (rsaF(w, 2), rsaF(w + 0x10, 2), rsaI(w + 0x34))
	return r
rsaShowLog = SDTest.Peek('g_ShowConsoleLog', 'i')
if SDTest.Peek('g_ScorerWhite', 'i'):
	SDTest.Poke('g_ShowConsoleLog', 1)
	check('Scorer_DrawConsole log', lambda: (rsaloglen(lambda: SDTest.Call('Scorer_DrawConsole', 'b')[0]), rsacon()))
SDTest.Poke('g_ShowConsoleLog', rsaShowLog)
check('Scorer_DrawConsole off', lambda: (rsaloglen(lambda: SDTest.Call('Scorer_DrawConsole', 'b')[0]), rsacon()))
SDTest.Call('Scorer_SetConsole', 'vi', 1)
if SDTest.PeekSpan('g_ConsoleOn', 0, 4) & 0xff:
	rsaSet(0x400000 + sym('g_ConsoleTime'), (-1.0e9,))
	SDTest.Call('cPlayerInput_SetString', 'tvis', 0x400000 + sym('g_PlayerInput'), ':ShowConsoleL\t')
	check('Scorer_DrawConsole open', lambda: (rsaloglen(lambda: SDTest.Call('Scorer_DrawConsole', 'b')[0]), rsacon()))
	check('Scorer_DrawConsole open 2', lambda: (rsaloglen(lambda: SDTest.Call('Scorer_DrawConsole', 'b')[0]), rsacon()))
	rsaConFlags = SDTest.PeekSpan('g_ConsoleOn', 0, 4)
	SDTest.PokeSpan('g_ConsoleOn', 0, rsaConFlags | 0x10000, 4)
	check('Scorer_DrawConsole closing', lambda: (rsaloglen(lambda: SDTest.Call('Scorer_DrawConsole', 'b')[0]), rsacon()))
SDTest.Call('Scorer_SetConsole', 'vi', 0)
SDTest.PokeSpan('g_ConsoleOn', 0, SDTest.PeekSpan('g_ConsoleOn', 0, 4) & ~0xff0000, 4)

# ---- the debug drawings of CShow (not the AI views 4 and 6, they change the AI) and of NWAnalizeOn ----
rsaCShow = SDTest.Peek('g_CShow', 'i')
rsaAnalize = SDTest.Peek('g_NWAnalyzeOn', 'i')
for rsav in (1, 2, 3, 5, 7, 0, 9):
	SDTest.Poke('g_CShow', rsav)
	check('Scorer_DrawDebug' + fmt((rsav,)), lambda: rsaconlines(lambda: rsaloglen(lambda: SDTest.Call('Scorer_DrawDebug', 'v'))))
SDTest.Poke('g_CShow', 0)
SDTest.Poke('g_NWAnalyzeOn', 1)
check('Scorer_DrawDebug NWAnalizeOn', lambda: rsaconlines(lambda: rsaloglen(lambda: SDTest.Call('Scorer_DrawDebug', 'v'))))
SDTest.Poke('g_NWAnalyzeOn', rsaAnalize)
SDTest.Poke('g_CShow', rsaCShow)

# ---- the sub menu logo on a cScraplandSubMenuScorer of our own: its time far back (frame time 0.2), the ring
# state poked, rand seeded; the game time itself (the turn of the reflection) is not shown ----
rsaSub = rsaAlloc(0xd8)
SDTest.Call('cScraplandSubMenuScorer_ctor', 'tii', rsaSub)
rsaSubSave = rsaF(0x400000 + sym('g_SubMenuWait'), 1) + rsaF(0x400000 + sym('g_SubMenuRingTime'), 1) + rsaF(0x400000 + sym('g_SubMenuRingWait'), 1)
rsaSubSaveI = (rsaI(0x400000 + sym('g_SubMenuRingDir')), rsaI(0x400000 + sym('g_SubMenuRingFrame')))
def rsasub(ring, wait, ringWait, dir, frame, seed):
	rsaSet(rsaSub + 0xac, (-1.0e9,))
	rsaSet(0x400000 + sym('g_SubMenuWait'), (wait,))
	rsaSet(0x400000 + sym('g_SubMenuRingTime'), (ring,))
	rsaSet(0x400000 + sym('g_SubMenuRingWait'), (ringWait,))
	rsaPoke(0x400000 + sym('g_SubMenuRingDir'), dir)
	rsaPoke(0x400000 + sym('g_SubMenuRingFrame'), frame)
	rsaSrand(seed)
	d3d = rsalog(lambda: SDTest.Call('cScraplandSubMenuScorer_Draw', 'tvii', rsaSub, 0))
	r = (d3d, rsaF(0x400000 + sym('g_SubMenuWait'), 1), rsaF(0x400000 + sym('g_SubMenuRingTime'), 1) + rsaF(0x400000 + sym('g_SubMenuRingWait'), 1), rsaI(0x400000 + sym('g_SubMenuRingDir')),
		rsaI(0x400000 + sym('g_SubMenuRingFrame')))
	for o in (0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0):
		img = rsaI(rsaSub + o)
		r = r + ((rsaF(img, 6), rsaI(img + 0x34)),)
	return r
rsaSet(rsaSub + 0xd4, (1.5,))
# the lightning frames 1..9 do not exist on the white sheet: the ring cases end on frames out of 1..9 (the ring
# time runs out in this frame, so the frame is the start frame)
for rsav in ((0.2, 1.0, 1.0, 0, 9, 1), (0.2, 1.0, 1.0, 1, 9, 2), (0.2, 1.0, 0.5, 0, -1, 3), (0.2, 1.0, 1.0, 1, -1, 4),
		(0.0, 0.1, 1.0, 0, 0, 5), (0.0, 0.1, 1.0, 1, 4, 6), (0.0, 5.0, 1.0, 0, 0, 7), (-1.0, -1.0, 1.0, 0, 0, 8)):
	check('cScraplandSubMenuScorer_Draw' + fmt(rsav), lambda rsav=rsav: rsasub(rsav[0], rsav[1], rsav[2], rsav[3],
		rsav[4], rsav[5]))
rsaSet(0x400000 + sym('g_SubMenuWait'), rsaSubSave[0:1])
rsaSet(0x400000 + sym('g_SubMenuRingTime'), rsaSubSave[1:2])
rsaSet(0x400000 + sym('g_SubMenuRingWait'), rsaSubSave[2:3])
rsaPoke(0x400000 + sym('g_SubMenuRingDir'), rsaSubSaveI[0])
rsaPoke(0x400000 + sym('g_SubMenuRingFrame'), rsaSubSaveI[1])
SDTest.Call('cScraplandSubMenuScorer_dtor', 'tvi', rsaSub)
rsaFree(rsaSub)

# ---- the text edit draw (vf04) on a cTextScorer of our own: the scorer time poked (the cursor blink), a virtual
# keyboard of 3 x 2 keys with the cursor on 'e', the edit cursor item an own buffer; all put back after ----
rsaTx = rsaAlloc(0xdc)
SDTest.Call('cTextScorer_ctor', 'tii', rsaTx)
rsaFont = rsaI(rsaTx + 0xb8)
rsaScorer0 = SDTest.Peek('g_Scorers', 'i')
if rsaFont and rsaScorer0:
	rsaSet(rsaTx + 0x10, (100.0, 50.0, 200.0, 20.0))
	rsaKbSave = (rsaI(0x400000 + sym('g_VirtualKeyboard')), rsaI(0x400000 + sym('g_VirtualKeyboard') + 0x4), rsaI(0x400000 + sym('g_VirtualKeyboard') + 0x100),
		rsaI(0x400000 + sym('g_VirtualKeyboard') + 0x104), rsaI(0x400000 + sym('g_VirtualKeyboard') + 0x108))
	rsaInSave = (rsaI(0x400000 + sym('g_PlayerInput') + 0x8c), rsaI(0x400000 + sym('g_PlayerInput') + 0x90), rsaI(0x400000 + sym('g_PlayerInput') + 0xb0))
	rsaTimeSave = rsaI(rsaScorer0 + 0x20)
	rsaCurSave = rsaI(0x400000 + sym('g_TextEditCursor'))
	rsaCur = rsaAlloc(0xac)
	rsaPoke(0x400000 + sym('g_TextEditCursor'), rsaCur)
	rsaPoke(0x400000 + sym('g_VirtualKeyboard'), 0x64636261)
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x4, 0x6665)
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x100, 3)
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x104, 2)
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x108, 6)
	rsaSet(0x400000 + sym('g_PlayerInput') + 0x8c, (1.0, 1.0))
	def rsakeys(active, center, t):
		rsaPoke(0x400000 + sym('g_PlayerInput') + 0xb0, (rsaInSave[2] & ~0xff) | active)
		rsaPoke(rsaTx + 0xd0, (rsaI(rsaTx + 0xd0) & ~0xff00) | (center << 8))
		rsaSet(rsaScorer0 + 0x20, (t,))
		d3d = rsaloglen(lambda: SDTest.Call('cTextScorer_DrawKeyboard', 'tvii', rsaTx, 0))
		return (d3d, rsaInts(rsaFont, (0x10, 0x14, 0x20)), rsaF(rsaFont, 2), rsaI(rsaI(0x400000 + sym('g_ScorerTabs')) + 0x34),
			rsaI(rsaI(rsaTx + 0xc4) + 0x34), rsaF(rsaCur + 0x10, 2), rsaB(0x400000 + sym('g_PlayerInput') + 0xb0))
	for rsav in ((0, 0, 0.1), (0, 1, 0.3), (1, 0, 0.1), (1, 1, 0.3), (1, 0, 0.45)):
		check('cTextScorer_DrawKeyboard' + fmt(rsav), lambda rsav=rsav: rsakeys(rsav[0], rsav[1], rsav[2]))
	rsaPoke(0x400000 + sym('g_VirtualKeyboard'), rsaKbSave[0])
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x4, rsaKbSave[1])
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x100, rsaKbSave[2])
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x104, rsaKbSave[3])
	rsaPoke(0x400000 + sym('g_VirtualKeyboard') + 0x108, rsaKbSave[4])
	rsaPoke(0x400000 + sym('g_PlayerInput') + 0x8c, rsaInSave[0])
	rsaPoke(0x400000 + sym('g_PlayerInput') + 0x90, rsaInSave[1])
	rsaPoke(0x400000 + sym('g_PlayerInput') + 0xb0, rsaInSave[2])
	rsaPoke(rsaScorer0 + 0x20, rsaTimeSave)
	rsaPoke(0x400000 + sym('g_TextEditCursor'), rsaCurSave)
	rsaFree(rsaCur)
SDTest.Call('cTextScorer_dtor', 'tvi', rsaTx)
rsaFree(rsaTx)
