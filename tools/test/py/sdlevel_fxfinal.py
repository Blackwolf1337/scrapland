# game World/FX: the final big FX functions (execfile'd by sdlevel.py, Python 1.5.2): the main effect of the mental
# controllers (built by its constructor with the model MentalControllerMainFX, its Init only when the model has the
# nodes, nine controllers MentalController1 set by SetController), the EMI, the GearMissile and the VehicleDamage
# Updates on the debris of debris systems of our own (Scrap.SetDebrisSys with the models of the FX engine variables;
# Start or Attach run on the new debris). The entities are our own: a Missile made far above the player and a Car
# with the model SPoli1 next to it (the target of the bolts, its EMI counter and the parent name of the missile set
# by hand). Models count as visible with R_SplitScreen 1; the frame time of the world timer, R_GetModelVertex, the
# camera position, SplitScreen and the fields of our entities are set by hand and restored. rand is seeded with srand
# of the exe before each Update. Printed: the fields of the effects, the particle parameters, the particles placed by
# hand and the trail points written.
# FX_ShipExplosion is called by rva with the debris systems of its engine variables set up by SetDebrisSys (the
# explosion with ParticleSystem elements, the smoke and fired debris with theirs), the split screen game flag set so that
# it is seen.
# Not called: the paths that need more particles or nodes than the models have (skipped by the checks before), the
# vehicle flames past the 12 vertices of the ship (the 6 of a motor may be stack garbage).
def ffni(a):
	return SDTest.Peek(a, '@i')
def ffnb(a):
	return SDTest.Peek(a, '@i') & 0xff
def ffnz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def ffnfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def ffnfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def ffnset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
# n floats of zero from a (in chunks of 64)
def ffnzero(a, n):
	while n > 0:
		k = n
		if k > 64:
			k = 64
		ffnset(a, (0.0,) * k)
		a = a + 4 * k
		n = n - k
# n floats from a (in chunks of 64)
def ffnfls(a, n):
	r = ()
	while n > 0:
		k = n
		if k > 64:
			k = 64
		r = r + ffnfl(a, k)
		a = a + 4 * k
		n = n - k
	return r
def ffnints(a, n):
	r = []
	for ffnk in range(n):
		r.append(ffni(a + 4 * ffnk))
	return tuple(r)
def ffnbytes(a, n):
	r = []
	for ffnk in range(n):
		r.append(ffnb(a + ffnk))
	return tuple(r)
def ffnpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def ffnsetbyte(a, v):
	w = ffni(a & ~3)
	sh = 8 * (a & 3)
	ffnpoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def ffnsrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def ffnrand():
	return SDTest.Call('crt_rand', 'i')[0]
def ffnloaded(m):
	return SDTest.Call('CGameModel_IsLoaded', 'tbi', m)[0]
def ffnnode(m, name):
	return SDTest.Call('CGameModel_FindRenderNode', 'tiis', m, name)[0]
def ffnnpart(p):
	if not ffni(p + 0xac):
		return 0
	return SDTest.Call('CGameParticles_NumParticles', 'tii', p)[0]
# the parameters of a CGameParticles without its padding and pointers
def ffnpart(p):
	return (ffnints(p, 0x26), ffni(p + 0x98) & 0xffff, ffnints(p + 0x9c, 2), ffnb(p + 0xa4))
# the particles lo .. hi - 1: active, then position, color and size while active
def ffnparts(p, lo, hi):
	r = ()
	if not ffni(p + 0xac):
		return r
	for ffnk in range(lo, hi):
		x = SDTest.Call('CGameParticles_GetParticle', 'tbiiFiipF', p, ffnk, (9.0, 9.0, 9.0), 0, 0, 6, (5.0,))
		if x[0]:
			r = r + ((x[1], x[2], x[3]),)
		else:
			r = r + (0,)
	return r
# a CGameTrail holder: the CTrail flags, color, width and scales, the counters, uv, uLen and the points written
def ffntrail(h):
	t = ffni(h)
	if not t:
		return None
	c = ffni(t + 0x20)
	mx = c & 0xff
	num = (c >> 8) & 0xff
	head = (c >> 16) & 0xff
	r = (ffnints(t + 0xc, 5), c, ffni(t + 0x24) & 0xffff, ffnints(t + 0x2c, 4), ffnb(t + 0x3c), ffnints(t + 0x4c, 2))
	pts = ffni(t + 0x28)
	if pts and mx:
		for ffnk in range(num):
			i = (head - 1 - ffnk) % mx
			r = r + (ffnints(pts + 0x14 * i, 5),)
	return r
def ffnstr(rva):
	p = SDTest.Peek(rva, 'i')
	if not p:
		return ''
	return SDTest.Peek(p, '@s')
def ffnmodelpos(m):
	return (SDTest.Call('CGameModel_GetPos', 'tiiF', m, (0.0, 0.0, 0.0))[1], SDTest.Call('CGameModel_GetRot', 'tiiF', m, (0.0, 0.0, 0.0))[1])

ffnworld = SDTest.Peek('g_pWorld', 'i')
ffntimer = ffnworld + 0x2c8 + 0xc
ffnsavedt = ffni(ffntimer)
ffnsplit = sym('g_SplitScreen') + 0x400000
ffnsavedsplit = ffni(ffnsplit)
ffngame = sym('g_SplitScreenGame') + 0x400000
ffnsavedgame = ffni(ffngame)
ffngetvert = SDTest.Peek('g_GetModelVertex', 'i')
ffncam = ffnworld + 0x254 + 12 * ffni(ffnworld + 0x50)
ffnsavedcam = ffnints(ffncam, 3)
ffnplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', ffnworld, 0)[0]
ffnpp = SDTest.Call('cEntity_GetPos', 'tiiF', ffnplayer, (0.0, 0.0, 0.0))[1]
ffny = ffnpp[1] + 3000.0
ffnatm = ffnfl(symva('g_AtmosphereLimit'), 1)[0]
if ffnatm + 1000.0 > ffny and ffnatm < 50000.0:
	ffny = ffnatm + 1000.0
def ffndt(dt):
	ffnset(ffntimer, (dt,))

# ---- the main effect of the mental controllers ----
ffnbuf = ffnz(0x40)
ffnset(ffnbuf, (ffnpp[0], ffny + 2000.0, ffnpp[2], 0.0, 0.4, 0.0))
ffnmain = ffnz(0x4278)
SDTest.Call('cFXElementsMentalControllerMainFX_ctor', 'tiisii', ffnmain, 'MentalControllerMainFX', ffnbuf, ffnbuf + 12)
ffnmm = ffnmain + 4
ffnmok = ffnloaded(ffnmm)
ffnmnodes = (-1, -1, -1)
if ffnmok:
	ffnmnodes = (ffnnode(ffnmm, 'DC_Root'), ffnnode(ffnmm, 'DF_DataFluid'), ffnnode(ffnmm, 'DF_TubeGlow'))
ffnmok = ffnmok and ffnmnodes[0] != -1 and ffnmnodes[1] > -1 and ffnmnodes[2] > -1
ffnmn = (0, 0)
if ffnmok:
	ffnsrand(1717)
	ffnmok = SDTest.Call('cFXElementsMentalControllerMainFX_Init', 'tbi', ffnmain)[0]
	ffnmn = (ffnnpart(ffnmain + 0x3c94), ffnnpart(ffnmain + 0x3e64))
check('FFN main fx', lambda: (ffnmok, ffnmnodes[0] != -1, ffnmnodes[1] != -1, ffnmnodes[2] != -1, ffnmn))
# the nine controllers around it
ffnctrls = []
ffnctrlok = 1
for ffnk in range(9):
	ffnc = ffnz(0x40)
	ffnset(ffnbuf, (ffnpp[0] + 900.0 * (ffnk - 4), ffny + 2000.0 + 50.0 * ffnk, ffnpp[2] + 300.0 * (ffnk % 3), 0.0,
		0.7 * ffnk, 0.0))
	SDTest.Call('cFXElementsMentalController_ctor', 'tiisii', ffnc, 'MentalController1', ffnbuf, ffnbuf + 12)
	ffnctrls.append(ffnc)
	if not ffnloaded(ffnc + 4):
		ffnctrlok = 0
check('FFN controllers', lambda: ffnctrlok)
def ffnmainfx(m):
	return (ffnb(m + 0x3c), ffni(m + 0x40), ffnfl(m + 0x74, 8), ffnfl(m + 0x2ac4, 40), ffnfl(m + 0x2e48, 40),
		ffnfl(m + 0x31cc, 40), ffnints(m + 0x3550, 40), ffnints(m + 0x38d4, 40), ffnbytes(m + 0x3d80, 64),
		ffnfls(m + 0x94, 60), ffnfls(m + 0xb20, 60), ffnfls(m + 0x15ac, 60), ffnfls(m + 0x2038, 60),
		ffnfl(m + 0x3f14, 3), ffnfl(m + 0x3f20, 30), ffnfl(m + 0x3f98, 30), ffnfls(m + 0x4010, 90),
		ffnints(m + 0x4180, 30), ffnints(m + 0x41f8, 30), ffnfl(m + 0x4270, 1), ffni(m + 0x4274))
def ffnmainfx2(m):
	return (ffnparts(m + 0x3c94, 0, 40), ffnparts(m + 0x3e64, 0, 80), ffntrail(m + 0x3c58), ffntrail(m + 0x3c5c),
		ffntrail(m + 0x3d44), ffntrail(m + 0x3d48), ffntrail(m + 0x4178), ffntrail(m + 0x417c))
if ffnmok:
	ffnpoke(ffnsplit, 1)
	check('cFXElementsMentalControllerMainFX_Update no controllers', lambda: (SDTest.Call('cFXElementsMentalControllerMainFX_Update', 'tvif', ffnmain,
		0.05), ffnb(ffnmain + 0x3c)))
if ffnmok and ffnctrlok and ffnmn[0] >= 225 and ffnmn[1] >= 80:
	for ffnk in range(9):
		SDTest.Call('cFXElementsMentalControllerMainFX_SetController', 'tviii', ffnmain, ffnk, ffnctrls[ffnk])
	# phase, last, scale, glow time, duration, pause, dt
	for ffnph, ffnlast, ffnsc, ffngt, ffngd, ffngp, ffndt1 in ((3, -2, 1.0, 0.0, 0.0, 0.0, 0.05),
			(3, -2, 1.0, 0.0, 0.0, 0.0, 0.05), (4, 1, 1.0, 0.5, 0.3, 0.0, 0.1), (2, 0, 3.0, 0.4, 1.0, 0.0, 0.05),
			(2, 2, 1.5, 0.3, 0.4, 0.0, 0.2), (3, 5, 1.0, 0.0, 0.0, 0.01, 0.05), (2, -1, 1.0, 0.0, 0.5, -1.0, 0.1),
			(1, -2, 1.0, 0.1, 0.5, 0.0, 0.3), (5, -2, 1.0, 0.1, 0.5, 0.0, 0.05), (3, 0, 2.95, 0.0, 0.0, 0.0, 1.5)):
		SDTest.Call('cFXElementsMentalControllerMainFX_SetPhase', 'tvii', ffnmain, ffnph)
		ffnpoke(ffnmain + 0x4274, ffnlast)
		ffnset(ffnmain + 0x4270, (ffnsc,))
		ffnset(ffnmain + 0x3f14, (ffngt, ffngd, ffngp))
		ffnsrand(4711)
		ffnr = (ffnph, ffnlast, ffnsc, ffngt, ffngd, ffngp, ffndt1)
		check('cFXElementsMentalControllerMainFX_Update' + fmt(ffnr), lambda dt=ffndt1: (SDTest.Call('cFXElementsMentalControllerMainFX_Update', 'tvif',
			ffnmain, dt), ffnmainfx(ffnmain), ffnmainfx2(ffnmain), ffnrand()))
	ffnset(ffnmain + 0x4270, (1.0,))
	ffnpoke(ffnmain + 0x4274, -2)
	# not seen: far from the camera (cleared once), then near it
	ffnpoke(ffnsplit, 0)
	ffntop = ffnfl(ffnmain + 0x44, 3)
	for ffnofs in (5000.0, 5000.0, 100.0):
		ffnset(ffncam, (ffntop[0] + ffnofs, ffntop[1], ffntop[2]))
		ffnsrand(4712)
		check('cFXElementsMentalControllerMainFX_Update not seen' + fmt((ffnofs,)), lambda: (SDTest.Call('cFXElementsMentalControllerMainFX_Update',
			'tvif', ffnmain, 0.05), ffnmainfx(ffnmain), ffnmainfx2(ffnmain), ffnloaded(ffnmm), ffnrand()))
	for ffnk in range(3):
		ffnpoke(ffncam + 4 * ffnk, ffnsavedcam[ffnk])
ffnpoke(ffnsplit, ffnsavedsplit)
SDTest.Call('cFXElementsMentalControllerMainFX_dtor', 'tvi', ffnmain)
ffnfree(ffnmain)
for ffnc in ffnctrls:
	SDTest.Call('cFXElementsMentalController_dtor', 'tvi', ffnc)
	ffnfree(ffnc)

# ---- our entities: a missile and a car next to it ----
ffnsrand(4242)
ffnmis = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', ffnworld, 'SDFfnMis', ffnpp[0] + 300.0, ffny, ffnpp[2], 'Missile')[0]
ffnmispos = (ffnpp[0] + 300.0, ffny, ffnpp[2])
ffncarobj = Scrap.CreateEntity('SDFfnCar', ffnpp[0] + 450.0, ffny + 20.0, ffnpp[2] + 60.0, 'Car')
def ffnfind(name):
	slot = SDTest.Call('HashTable_Slot', 'tiis', ffnworld + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', ffnworld + 4, name, slot)[0]
	if node:
		return ffni(node)
	return 0
ffncar = ffnfind('SDFfnCar')
ffniscar = 0
if ffncar:
	ffniscar = SDTest.Call(ffni(ffni(ffncar) + 0x14) - 0x400000, 'tbis', ffncar, 'Car')[0]
if ffniscar:
	SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', ffncar, 'SPoli1', 1.0, 1.0, 1.0, 1)
ffncarok = ffniscar and ffnloaded(ffncar + 0xb8)
if not ffncarok:
	ffncar = 0
check('FFN entities', lambda: (ffnmis != 0, ffnmis and ffnb(ffnmis + 0x4c), ffncarobj is not None, ffniscar,
	ffncarok))
def ffndel(e):
	SDTest.Call('PtrTable_Delete@208810', 'tbis', ffnworld + 4, SDTest.Peek(e + 4, '@p'))
	SDTest.Call(ffni(ffni(e) + 0xc) - 0x400000, 'tiii', e, 1)
# the element on the debris of a new debris system of file
def ffndebris(name, kind):
	if not name:
		return (0, 0, 0)
	ffnsrand(1234)
	Scrap.SetDebrisSys(name, 1, 0.0, 0.0, 1.0, 0.0, 0, kind)
	s = SDTest.Call('cWorld_GetDebrisSys', 'tiis', ffnworld, name)[0]
	d = 0
	if s:
		d = ffni(s)
	fx = 0
	if d:
		fx = ffni(d + 0x54)
	return (s, d, fx)
ffnsavedmis = ()
ffnsavedcar = ()
if ffnmis:
	ffnsavedmis = ffnints(ffnmis + 0x3f0, 1) + ffnints(ffnmis + 0x5a4, 2) + ffnints(ffnmis + 0x3fc, 32)
if ffncar:
	ffnsavedcar = ffnints(ffncar + 0x58c, 1) + ffnints(ffncar + 0xf0, 1)
	ffncarmodel = ffncar + 0xb8
	ffncarpos = ffnmodelpos(ffncarmodel)
	def ffncarat(pos, rot):
		SDTest.Call('CGameModel_SetPos', 'tviF', ffncarmodel, pos)
		SDTest.Call('CGameModel_SetRot', 'tviF', ffncarmodel, rot)
ffnpoke(ffnsplit, 1)

# ---- EMI: the bolts from the missile to the world and to our car ----
ffns, ffnd, ffnfx = ffndebris(ffnstr(sym('g_MissileFXEMI')), 35)
ffnn = 0
if ffnfx and ffni(ffnfx) == 0x400000 + sym('cObjElementFXEMI_vftable'):
	ffnn = ffnnpart(ffnfx + 0xc)
check('FFN emi', lambda: (ffnd != 0, ffnfx != 0, ffnn))
if ffnn >= 17 and ffnmis and ffncar:
	def ffnemi(fx):
		r = (ffni(fx + 4) != 0, ffnfl(fx + 0xbc, 3), ffnfl(fx + 0xd4, 8), ffnfl(fx + 0xc54, 16), ffni(fx + 0xd14),
			ffnfl(fx + 0xd18, 1))
		for ffnk in range(8):
			r = r + (ffni(fx + 0xc9c + 16 * ffnk) == ffncar, ffni(fx + 0xc9c + 16 * ffnk) == 0)
		return r
	def ffnemi2(fx):
		r = (ffnpart(fx + 0xc), ffnparts(fx + 0xc, 0, 17))
		for ffnk in (0, 1, 2, 7):
			r = r + (ffnfls(fx + 0x114 + 120 * ffnk, 18), ffnfls(fx + 0x4d4 + 240 * ffnk, 36), ffntrail(fx + 0xf4 +
				4 * ffnk))
		return r
	ffnzero(ffnfx + 0x114, (0xc54 - 0x114) / 4)
	ffnpoke(ffnfx + 4, ffnmis)
	ffnset(ffnmis + 0x5a4, (500.0, 2000.0))
	# the life left of the missile, the time since hidden, the time, the glow time, its emi counter, the frame time
	for ffnlife, ffnhide, ffnt, ffngt, ffnemic, ffnsp, ffndt1 in ((2.0, 0.0, 0.3, 0.05, 1.0, 1, 0.05),
			(2.0, 0.0, 0.5, 0.0, 1.0, 1, 0.05), (2.0, 0.0, 0.7, 0.0, 1.0, 1, 0.03), (0.9, 0.0, 0.9, 0.05, 0.0, 1, 0.05),
			(0.7, 0.0, 1.0, 0.05, 1.0, 1, 0.2), (2.0, 0.1, 1.2, 0.05, 1.0, 0, 0.05), (2.0, 0.5, 1.3, 0.05, 1.0, 0, 0.05),
			(2.0, 0.0, 1.5, 0.05, 1.0, 1, 0.5), (0.5, 0.0, 0.1, 0.05, 1.0, 1, 0.05)):
		ffnset(ffnmis + 0x3f0, (ffnlife,))
		ffnset(ffnfx + 0xbc, (ffnhide,))
		ffnset(ffnfx + 0xc4, (ffnt,))
		ffnset(ffnfx + 0xd18, (ffngt,))
		ffnset(ffncar + 0x58c, (ffnemic,))
		ffnpoke(ffnsplit, ffnsp)
		ffndt(ffndt1)
		ffnsrand(35)
		ffnr = (ffnlife, ffnhide, ffnt, ffngt, ffnemic, ffnsp, ffndt1)
		check('cObjElementFXEMI_Update' + fmt(ffnr), lambda: (SDTest.Call('cObjElementFXEMI_Update', 'tviif', ffnfx, ffnd, 0.0),
			ffnemi(ffnfx), ffnemi2(ffnfx), ffnmodelpos(ffnd), ffnrand()))
	ffnpoke(ffnsplit, 1)
	ffnpoke(ffnfx + 4, 0)
	check('cObjElementFXEMI_Update no entity', lambda: (SDTest.Call('cObjElementFXEMI_Update', 'tviif', ffnfx, ffnd, 0.0),
		ffnemi(ffnfx)))
	SDTest.Call('cObjElementFXEMI_Reset', 'tvii', ffnfx, ffnd)
	check('FFN emi reset', lambda: ffnemi(ffnfx))

# ---- GearMissile: the electric ball on the missile, its bolts to the world and to our car ----
ffns, ffnd, ffnfx = (0, 0, 0)
ffngearok = 0
if ffnmis and ffnstr(sym('g_MissileFXGearMissile')):
	# the nodes of the model checked on a model of our own first (Attach CHECKs Dummy01 and Dummy02)
	ffngm = ffnz(0x1c)
	SDTest.Call('CGameModel_ctor', 'tiis', ffngm, ffnstr(sym('g_MissileFXGearMissile')))
	ffngearok = ffnloaded(ffngm) and ffnnode(ffngm, 'Dummy01') > -1 and ffnnode(ffngm, 'Dummy02') > -1
	SDTest.Call('CGameModel_Reset', 'tvi', ffngm)
	ffnfree(ffngm)
if ffngearok:
	ffns, ffnd, ffnfx = ffndebris(ffnstr(sym('g_MissileFXGearMissile')), 71)
ffnn = (0, 0)
if ffnfx and ffni(ffnfx) == 0x400000 + sym('cObjElementFXGearMissile_vftable'):
	ffnn = (ffnnpart(ffnfx + 0xc), ffnnpart(ffnfx + 0xc0))
check('FFN gear', lambda: (ffngearok, ffnd != 0, ffnfx != 0, ffnn))
if ffnn[0] >= 12 and ffnn[1] >= 30 and ffncar:
	def ffngear(fx):
		return (ffni(fx + 4) != 0, ffnfl(fx + 0x170, 3), ffnfl(fx + 0x17c, 3), ffnfl(fx + 0x18c, 11),
			ffnfl(fx + 0x690, 10))
	def ffngear2(fx):
		r = (ffnpart(fx + 0xc), ffnpart(fx + 0xc0), ffnparts(fx + 0xc, 0, 12), ffnparts(fx + 0xc0, 0, 30),
			ffntrail(fx + 0x188))
		for ffnk in (0, 1, 9):
			r = r + (ffnfls(fx + 0x1e0 + 120 * ffnk, 18), ffntrail(fx + 0x1b8 + 4 * ffnk))
		return r
	ffnzero(ffnfx + 0x1e0, (0x690 - 0x1e0) / 4)
	ffnset(ffnfx + 0x17c, (ffnmispos[0] - 100.0, ffnmispos[1], ffnmispos[2] + 30.0))
	# the parent of the missile is our car
	SDTest.Call('crt_memcpy', 'iisi', ffnmis + 0x3fc, 'SDFfnCar', 9)
	# the missile on, the debris life, emit time, time, trail step, frame time, dt
	for ffnon, ffnlife, ffnet, ffnt, ffnts, ffndt1, ffnddt in ((1, 2.0, 0.05, 0.3, 0.0, 0.05, 0.02),
			(1, 2.0, 0.0, 0.5, 0.01, 0.05, 0.02), (1, 2.0, 0.03, 0.6, 0.03, 0.03, 0.02),
			(1, 0.45, 0.02, 0.7, 0.0, 0.05, 0.05), (1, 0.35, 0.03, 0.8, 0.01, 0.05, 0.1),
			(1, 2.0, 0.05, 0.1, 0.0, 0.3, 0.02), (0, 2.0, 0.05, 0.9, 0.0, 0.05, 0.02),
			(0, 0.3, 0.05, 1.0, 0.0, 0.05, 0.02)):
		if ffnon:
			ffnpoke(ffnfx + 4, ffnmis)
		else:
			ffnpoke(ffnfx + 4, 0)
		ffnset(ffnd + 0x4c, (ffnlife,))
		ffnset(ffnfx + 0x170, (ffnet,))
		ffnset(ffnfx + 0x178, (ffnt,))
		ffnset(ffnfx + 0x18c, (ffnts,))
		ffndt(ffndt1)
		ffnsrand(71)
		ffnr = (ffnon, ffnlife, ffnet, ffnt, ffnts, ffndt1, ffnddt)
		check('cObjElementFXGearMissile_Update' + fmt(ffnr), lambda ddt=ffnddt: (SDTest.Call('cObjElementFXGearMissile_Update', 'tviif', ffnfx,
			ffnd, ddt), ffngear(ffnfx), ffngear2(ffnfx), ffnmodelpos(ffnd), ffnfl(ffnd + 0x4c, 1), ffnrand()))
	# the missile off: the model to its position and angles, the trail step 0.03
	ffnsetbyte(ffnmis + 0x4c, 0)
	ffnpoke(ffnfx + 4, ffnmis)
	ffnset(ffnd + 0x4c, (2.0,))
	ffnsrand(72)
	check('cObjElementFXGearMissile_Update missile off', lambda: (SDTest.Call('cObjElementFXGearMissile_Update', 'tviif', ffnfx, ffnd, 0.02),
		ffngear(ffnfx), ffnmodelpos(ffnd), ffnfl(ffnd + 0x4c, 1), ffnrand()))
	ffnsetbyte(ffnmis + 0x4c, 1)
	ffnpoke(ffnfx + 4, 0)

# ---- VehicleDamage: the damage of our car ----
ffns, ffnd, ffnfx = ffndebris('Models/GFX/VehicleDamage.M3D', 38)
ffnok = ffnfx != 0 and ffni(ffnfx) == 0x400000 + sym('cObjElementFXVehicleDamage_vftable')
ffnn = (0, 0, 0)
if ffnok:
	ffnn = (ffnnpart(ffnfx + 0x1c), ffnnpart(ffnfx + 0xcc), ffnnpart(ffnfx + 0x184))
ffnefx = 0
if ffncar:
	ffnefx = ffni(ffncar + 0x224)
check('FFN damage', lambda: (ffnd != 0, ffnok, ffnn, ffnefx != 0, ffnefx and ffnb(ffnefx + 0x189)))
if ffnok and ffncar and ffnefx and ffnn[0] and ffnn[1] and ffnn[2]:
	def ffndmg(fx):
		return (ffni(fx + 4) != 0, ffnb(fx + 8), ffnints(fx + 0xc, 2), ffnb(fx + 0x18), ffnfl(fx + 0x17c, 2),
			ffnfl(fx + 0x234, 3), ffnb(fx + 0x240), ffnfl(fx + 0x244, 1), ffni(ffnefx + 0x1a4) == fx)
	def ffndmg2(fx):
		return (ffnpart(fx + 0x1c), ffnpart(fx + 0xcc), ffnpart(fx + 0x184))
	# a motor of our own (a copy of the model of the car) as motor 0, its exhaust 0 the one of the effect
	ffnfakemotor = ffnz(0x100)
	ffnset(ffnfakemotor, ffnfl(ffncar + 0xb8, 7))
	ffnpoke(ffnfakemotor + 0x38, -1)
	ffnpoke(ffnfakemotor + 0x3c, -1)
	ffncarmotor0 = ffni(ffncar + 0x728)
	ffnpoke(ffncar + 0x728, ffnfakemotor)
	ffnfakeplayer = ffnz(0x100)
	ffnpoke(ffnfx + 4, ffncar)
	ffnpoke(ffnfx + 0x248, 1)
	ffnpoke(ffnfx + 0x24c, 0)
	ffnpoke(ffnfx + 0x250, 0)
	ffnpoke(ffnfx + 8, 1)
	SDTest.Poke('g_GetModelVertex', 1)
	ffnsavedlv = ffnints(ffncar + 0x4e4, 3)
	ffnset(ffncar + 0x4e4, (40.0, -10.0, 300.0))
	ffnbase = ffncarpos[0][0]
	# pct, level, emit time, trail time, the place of the car, player, split game, frame time, the first flag
	for ffnpct, ffnlv, ffnet, ffntt, ffnmove, ffnpl, ffngm, ffndt1, ffnfirst in ((8, 0.3, 0.05, 0.0, 0.0, 1, 0, 0.05, 1),
			(8, 0.3, 0.02, 0.0, 0.0, 1, 0, 0.05, 1), (6, 0.26, 0.05, 0.0, 0.0, 1, 0, 0.1, 0),
			(20, 0.7, 0.05, 0.0, 0.0, 1, 0, 0.05, 0), (24, 0.95, 0.05, 0.0, 0.0, 1, 0, 0.05, 0),
			(8, 0.3, 0.05, 0.0, 0.0, 0, 0, 0.05, 1), (8, 0.3, 0.05, 0.0, 1000.0, 0, 0, 0.05, 1),
			(8, 0.3, 0.05, 0.03, 30.0, 0, 0, 0.05, 0), (8, 0.3, 0.05, 0.05, 2.0, 0, 1, 0.05, 1),
			(8, 0.3, 0.05, 0.0, 600.0, 0, 1, 0.02, 0), (8, 0.3, 0.0, 0.0, 0.0, 0, 0, 0.05, 1)):
		ffnpoke(ffnfx + 0xc, 50)
		ffnpoke(ffnfx + 0x10, ffnpct)
		ffnset(ffnfx + 0x17c, (ffnlv, ffnet))
		ffnset(ffnfx + 0x244, (ffntt,))
		ffnsetbyte(ffnfx + 0x240, ffnfirst)
		ffncarat((ffnbase + ffnmove, ffncarpos[0][1], ffncarpos[0][2]), (0.1, 0.3, 0.05))
		if ffnpl:
			ffnpoke(ffncar + 0xf0, ffnfakeplayer)
		else:
			ffnpoke(ffncar + 0xf0, 0)
		ffnpoke(ffngame, ffngm)
		ffnset(ffnd + 0x4c, (1.5,))
		ffndt(ffndt1)
		ffnsrand(38)
		ffnr = (ffnpct, ffnlv, ffnet, ffntt, ffnmove, ffnpl, ffngm, ffndt1, ffnfirst)
		check('cObjElementFXVehicleDamage_Update' + fmt(ffnr), lambda: (SDTest.Call('cObjElementFXVehicleDamage_Update', 'tviif', ffnfx, ffnd,
			0.0), ffndmg(ffnfx), ffndmg2(ffnfx), ffnmodelpos(ffnd), ffnrand()))
		ffnpoke(ffngame, ffnsavedgame)
	ffnpoke(ffncar + 0xf0, ffnsavedcar[1])
	ffnpoke(ffnefx + 0x1a4, 0)
	# not active, not seen
	ffnpoke(ffnfx + 8, 0)
	check('cObjElementFXVehicleDamage_Update not active', lambda: (SDTest.Call('cObjElementFXVehicleDamage_Update', 'tviif', ffnfx, ffnd, 0.0),
		ffndmg(ffnfx), ffnmodelpos(ffnd)))
	ffnpoke(ffnsplit, 0)
	check('cObjElementFXVehicleDamage_Update not seen', lambda: (SDTest.Call('cObjElementFXVehicleDamage_Update', 'tviif', ffnfx, ffnd, 0.0),
		ffndmg(ffnfx)))
	ffnpoke(ffnsplit, 1)
	ffnpoke(ffnfx + 4, 0)
	check('cObjElementFXVehicleDamage_Update no entity', lambda: (SDTest.Call('cObjElementFXVehicleDamage_Update', 'tviif', ffnfx, ffnd, 0.0),
		ffndmg(ffnfx)))
	for ffnk in range(3):
		ffnpoke(ffncar + 0x4e4 + 4 * ffnk, ffnsavedlv[ffnk])
	ffnpoke(ffncar + 0x728, ffncarmotor0)
	SDTest.Poke('g_GetModelVertex', ffngetvert)
	ffnfree(ffnfakemotor)
	ffnfree(ffnfakeplayer)

# ---- FX_ShipExplosion: the debris, the explosion and its flames ----
ffns, ffnd, ffnfx = ffndebris(ffnstr(sym('g_FXShipExplosionFile')), 1)
ffnexpok = ffnfx != 0 and ffni(ffnfx) == 0x400000 + sym('cObjElementFXParticleSystem_vftable')
ffnsmk = ffndebris(ffnstr(sym('g_FXSmokeDebrisFile')), 48)[0]
ffnfrd = ffndebris(ffnstr(sym('g_FXFiredDebrisFile')), 40)[0]
ffnexps = ffns
check('FFN explosion', lambda: (ffnexpok, ffnexpok and ffnnpart(ffnfx + 4), ffnsmk != 0, ffnfrd != 0))
# the first n debris of the active list of a system up to stop (the head before the call; the older ones are from
# the explosion of sdlevel.py without srand): position, speed, life and the smoke element fields
def ffnsysdebris(s, n, stop):
	r = ()
	if not s:
		return r
	d = ffni(s + 4)
	while d and d != stop and n > 0:
		fx = ffni(d + 0x54)
		r = r + ((ffnfl(d + 0x1c, 3), ffnfl(d + 0x34, 3), ffnfl(d + 0x4c, 1), fx != 0 and ffnfl(fx + 0xcc, 3),
			fx != 0 and ffnb(fx + 0xd8)),)
		d = ffni(d + 0x50)
		n = n - 1
	return r
def ffnexplosion(stop):
	d = ffni(ffnexps + 4)
	if not d or d == stop:
		return None
	fx = ffni(d + 0x54)
	r = (ffnfl(d + 0x4c, 1), ffnmodelpos(d)[0])
	if fx:
		r = r + (ffnbytes(fx + 0xb4, 2), ffnpart(fx + 4))
	return r
if ffnexpok and ffnnpart(ffnfx + 4) and ffnsmk and ffnfrd:
	# offset from the missile (or the player for the ground), speed, scale, time, flames, collision, split game
	for ffnat, ffnofs, ffnv, ffnsc, ffnt, ffnfl1, ffncol, ffngm in ((0, (900000.0, 0.0, 0.0), (0.0, 0.0, 0.0), 1.0,
			1.0, 0, 0, 0), (0, (0.0, 0.0, 0.0), (100.0, 0.0, -50.0), 1.0, 1.0, 0, 0, 1),
			(0, (40.0, -20.0, 10.0), (0.0, 30.0, 0.0), 0.5, 2.0, 2, 0, 1),
			(1, (0.0, 0.0, 0.0), (-200.0, 10.0, 80.0), 1.5, 0.5, 3, 1, 1),
			(0, (10.0, 0.0, 0.0), (0.0, 0.0, 0.0), 0.02, 0.02, 1, 1, 1)):
		ffnbase = ffnmispos
		if ffnat:
			ffnbase = ffnpp
		ffnp = (ffnbase[0] + ffnofs[0], ffnbase[1] + ffnofs[1], ffnbase[2] + ffnofs[2])
		ffnpoke(ffngame, ffngm)
		ffnsrand(1950)
		ffnr = (ffnat, ffnofs, ffnv, ffnsc, ffnt, ffnfl1, ffncol, ffngm)
		ffnheads = (ffni(ffnexps + 4), ffni(ffnsmk + 4), ffni(ffnfrd + 4))
		check('FX_ShipExplosion' + fmt(ffnr), lambda p=ffnp, v=ffnv, sc=ffnsc, t=ffnt, n=ffnfl1, c=ffncol: (
			SDTest.Call('FX_ShipExplosion', 'vffffffffii', p[0], p[1], p[2], v[0], v[1], v[2], sc, t, n, c),
			ffnexplosion(ffnheads[0]), ffnsysdebris(ffnsmk, 4, ffnheads[1]), ffnsysdebris(ffnfrd, 4, ffnheads[2]),
			ffnrand()))
		ffnpoke(ffngame, ffnsavedgame)

# ---- the end ----
ffnpoke(ffnsplit, ffnsavedsplit)
ffnpoke(ffngame, ffnsavedgame)
ffnpoke(ffntimer, ffnsavedt)
if ffncar:
	ffncarat(ffncarpos[0], ffncarpos[1])
	ffnpoke(ffncar + 0x58c, ffnsavedcar[0])
if ffnmis:
	ffnpoke(ffnmis + 0x3f0, ffnsavedmis[0])
	ffnpoke(ffnmis + 0x5a4, ffnsavedmis[1])
	ffnpoke(ffnmis + 0x5a8, ffnsavedmis[2])
	for ffnk in range(32):
		ffnpoke(ffnmis + 0x3fc + 4 * ffnk, ffnsavedmis[3 + ffnk])
	ffndel(ffnmis)
if ffncarobj is not None:
	check('SDFfnCar Del', lambda: ffncarobj.Del())
ffnfree(ffnbuf)
