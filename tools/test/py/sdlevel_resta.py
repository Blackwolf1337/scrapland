# game leftovers below rva 0x1a8000, "Rest A" (execfile'd by sdlevel.py, Python 1.5.2). The vehicle AI (an AI of our
# own on the Car "SDRsaCar" made as in sdlevel_ivehicle.py): the track points of every kind of track on a traffic graph
# of our own (FZ has no tracks), the random track offsets after a fixed srand and the line tests of the hook points. The repositioning (Repos, ReposTraffic,
# GraphTraffic) is not called: it picks nodes near the camera by the traffic of the level, which differs between the
# runs.
def rsaI(a):
	return SDTest.Peek(a, '@i')
def rsaB(a):
	return SDTest.Peek(a, '@b')
def rsaF(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def rsaSet(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def rsaPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def rsaAlloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def rsaFree(p):
	SDTest.Call(0x133550, 'ii', p)
def rsaSrand(s):
	SDTest.Call(0x323c50, 'vi', s)
def rsaInts(a, offs):
	r = ()
	for o in offs:
		r = r + (rsaI(a + o),)
	return r
def rsaFind(name):
	w = SDTest.Peek(0x467450, 'i')
	slot = SDTest.Call(0x131f70, 'tiis', w + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', w + 4, name, slot)[0]
	if node:
		return rsaI(node)
	return 0

# ---- the vehicle AI ----
rsaGroup = SDTest.Peek(0x444b84, 'i')
rsaTraffic = SDTest.Peek(0x444c98, 'i')
rsaCarObj = Scrap.CreateEntity('SDRsaCar', 54000.0, 50000.0, 61000.0, 'Car')
rsaCar = rsaFind('SDRsaCar')
check('SDRsaCar', lambda: (rsaCarObj is not None, rsaCar != 0, rsaGroup != 0, rsaTraffic != 0))
if rsaCar and rsaGroup and rsaTraffic:
	SDTest.Call(0x109db0, 'tvisfffi', rsaCar, 'SPoli1', 1.0, 1.0, 1.0, 1)
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
	rsaMem = SDTest.Call(0x1334d0, 'ii', 0x3b0)[0]
	rsaAI = SDTest.Call(0x086390, 'tiiiiii', rsaMem, rsaCar, rsaP1, rsaP2, rsaP3)[0]
	check('cAIVehicle_ctor', lambda: rsaAI == rsaMem)
	rsaSave = rsaInts(rsaAI, (0x31c, 0x2e4, 0x2e8, 0x2ec, 0x320, 0x324, 0x328, 0x334))
	# the track points: offsets on 0..2, the plane cut of a light vehicle on 3 and 4 (tracks of FZ), the copy for the
	# others; then the random offsets
	# FZ has no traffic tracks: a graph of our own with two tracks of 6 nodes (the second a loop) in g_NodesTraffic
	# while the track points run
	rsaG = SDTest.Call(0x1334d0, 'ii', 0x70)[0]
	SDTest.Call(0x09b970, 'tii', rsaG)
	rsaTr = SDTest.Call(0x1335a0, 'ii', 2 * 0x24)[0]
	rsaNodes = (((0.0, 0.0, 0.0), (2000.0, 100.0, 500.0), (4000.0, 0.0, 1500.0), (5000.0, -200.0, 3500.0),
		(5500.0, 0.0, 6000.0), (5000.0, 300.0, 9000.0)),
		((-1000.0, 50.0, -1000.0), (-3000.0, 50.0, -1500.0), (-5000.0, 400.0, -1000.0), (-6000.0, 50.0, 1000.0),
		(-4000.0, 50.0, 2500.0), (-2000.0, -300.0, 1500.0)))
	for rsaT in range(2):
		rsaNd = SDTest.Call(0x1335a0, 'ii', 6 * 12)[0]
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
	SDTest.Poke(0x444c98, rsaG)
	rsaOutSave = SDTest.Peek(0x4677e8, 'i')
	SDTest.Poke(0x4677e8, 1)
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
					check('cAIVehicle_TrackPoint' + fmt((rsaKind, rsaVT, rsaT, rsaN)), lambda: (SDTest.Call(0x08a850,
						'tiiFFii', rsaAI, (7.0, 8.0, 9.0), rsaP, rsaT, rsaN)[1], rsaF(rsaAI + 0x320, 3)))
					rsaSrand(77 + rsaN)
					check('cAIVehicle_TrackPointRandom' + fmt((rsaKind, rsaVT, rsaT, rsaN)), lambda: (
						SDTest.Call(0x08b120, 'tiiFFii', rsaAI, (7.0, 8.0, 9.0), rsaP, rsaT, rsaN)[1],
						rsaF(rsaAI + 0x2e4, 3), rsaF(rsaAI + 0x320, 3)))
	SDTest.Poke(0x444c98, rsaTrafficSave)
	SDTest.Poke(0x4677e8, rsaOutSave)
	check('fake traffic Delete', lambda: SDTest.Call(0x09bbf0, 'tiii', rsaG, 1)[0] == rsaG)
	# the hook points around the vehicle at its place and turned
	for rsaRot in ((0.0, 0.0, 0.0), (0.5, 1.25, 0.0), (-1.5, 0.0, 2.0)):
		SDTest.Call(rsaI(rsaI(rsaCar) + 0x84) - 0x400000, 'tviF', rsaCar, rsaRot)
		for rsaM in (0, 1, 2, 7):
			rsaPoke(rsaAI + 0x334, rsaM)
			check('cAIVehicle_TestHookPoint' + fmt((rsaRot, rsaM)), lambda: (SDTest.Call(0x08ab20, 'tbiF', rsaAI,
				(-1.0, -1.0, -1.0))[0:2], rsaI(rsaAI + 0x334)))
	for rsaK in range(8):
		rsaPoke(rsaAI + (0x31c, 0x2e4, 0x2e8, 0x2ec, 0x320, 0x324, 0x328, 0x334)[rsaK], rsaSave[rsaK])
	check('cAIVehicle_Delete', lambda: SDTest.Call(0x0868b0, 'tiii', rsaAI, 1)[0] == rsaAI)
	rsaFree(rsaP1)
	rsaFree(rsaP2)
	rsaFree(rsaP3)
	for rsaK in range(6):
		rsaPoke(rsaGroup + (0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70)[rsaK], rsaGroupSave[rsaK])
	for rsaK in range(4):
		rsaPoke(rsaCar + (0x420, 0x424, 0x428, 0x5e8)[rsaK], rsaCarSave[rsaK])
if rsaCarObj:
	check('SDRsaCar Del', lambda: rsaCarObj.Del())
