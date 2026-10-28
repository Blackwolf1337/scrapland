# game Entities/Vehicle/VehicleMotorsEffects.c: cVehicleMotorsEffects_Exhaust (rva 0x10f210), the exhaust particles
# of each FxClass type (execfile'd by sdlevel.py, Python 1.5.2). On cars of our own far away: SPoli2 with the motor
# MPoli1 at slot 0 (two models, a DF_MotorTrust node each, 25 particles), then MBankMaster1 at the same slot (also
# DF_MotorTrust_02, the second particles type 21 needs), SPoli1 with MPoli3 (one model), and the ships SLifeBoat and
# TMed2 whose exhausts are nodes of the ship model (the effects of cVehicleMotors; TMed2 has 4 particles, the types
# that fit). The colors of the effects are set (the constructor leaves them), the type is poked; each type runs with
# several thrusts, speeds, cloud and fade values, rand seeded before each call and the particles killed. Printed: the
# parameters of the particles, the particles placed by hand, the alpha of the system and the next rand. Also one side
# without node, disabled motor models, the type 0 and unknown types. A type runs only on particles with room for its
# indices and sprites. At the end the particles are emptied and stopped, the cars deleted.
def exhi(a):
	return SDTest.Peek(a, '@i')
def exhb(a):
	return SDTest.Peek(a, '@i') & 0xff
def exhz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def exhpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def exhsrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def exhrand():
	return SDTest.Call('crt_rand', 'i')[0]
def exhints(a, n):
	r = []
	for exhk in range(n):
		r.append(exhi(a + 4 * exhk))
	return tuple(r)
# the parameters of a CGameParticles without its pointers
def exhpart(p):
	return (exhints(p, 0x26), exhi(p + 0x98) & 0xffff, exhints(p + 0x9c, 2), exhb(p + 0xa4))
def exhnum(p):
	return SDTest.Call('CGameParticles_NumParticles', 'tii', p)[0]
# the multi sprite count of the system, 99 without one
def exhsprites(p):
	ms = exhi(exhi(p + 0xac) + 0x118)
	if not ms:
		return 99
	return exhi(ms + 8)
def exhparticles(p):
	r = []
	for exhk in range(exhnum(p)):
		x = SDTest.Call('CGameParticles_GetParticle', 'tbiiFiipF', p, exhk, (9.0, 9.0, 9.0), 0, 0, 6, (5.0,))
		if x[0]:
			r.append((exhk, x[1], x[2], x[3]))
	return r
def exhkill(p):
	SDTest.Call('CGameParticles_KillRange', 'tviii', p, 0, exhnum(p))
def exhalpha(p):
	return SDTest.Call('CGameParticles_GetAlpha', 'tfi', p)[0]
# the particles of side s of the effects e (0 for none)
def exhps(e, s, j):
	return exhi(e + 0x24 + 8 * s + 4 * j)
def exhsides(e):
	r = []
	for exhs in (0, 1):
		if exhi(e + 0x14 + 8 * exhs) != -1 and exhps(e, exhs, 0):
			r.append(exhs)
	return r
# the effects: on, type, nodes, particles as != 0, color, scale, offset, exhausts
def exheff(e):
	return (exhb(e + 0xc), exhi(e + 0x10), exhints(e + 0x14, 4), exhps(e, 0, 0) != 0, exhps(e, 0, 1) != 0,
		exhps(e, 1, 0) != 0, exhps(e, 1, 1) != 0, exhints(e + 0x6c, 2), exhints(e + 0x74, 2), exhi(e + 0x7c))
def exhstate(e):
	r = []
	for exhs in (0, 1):
		for exhj in (0, 1):
			p = exhps(e, exhs, exhj)
			if p:
				r.append((exhs, exhj, exhpart(p), exhparticles(p), exhalpha(p)))
	return r

exhworld = SDTest.Peek('g_pWorld', 'i')
def exhfind(name):
	slot = SDTest.Call('HashTable_Slot', 'tiis', exhworld + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', exhworld + 4, name, slot)[0]
	if node:
		return exhi(node)
	return 0
# a car of our own with the model file (inDir as cVehicleEntity_SetModel takes it)
def exhcar(name, x, model, indir):
	obj = Scrap.CreateEntity(name, x, 50000.0, 52000.0, 'Car')
	car = exhfind(name)
	iscar = 0
	if car:
		iscar = SDTest.Call(exhi(exhi(car) + 0x14) - 0x400000, 'tbis', car, 'Car')[0]
	loaded = 0
	if iscar:
		SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', car, model, 1.0, 1.0, 1.0, indir)
		loaded = SDTest.Call('CGameModel_IsLoaded', 'tbi', car + 0xb8)[0]
	check('EXH car ' + name, lambda o=obj, c=car, i=iscar, l=loaded: (o is not None, c != 0, i, l))
	if not loaded:
		car = 0
	return (obj, car)

# thrust, k, f4bc, speed, cloud, fade, seed
exhinputs = ((1.0, 1.0, 1.0, 1.0, 0, 1.0, 11), (0.5, 0.3, 0.8, 0.6, 0, 0.5, 12), (-0.5, 0.0, 1.0, 1.0, 0, 1.0, 13),
	(0.7, 0.5, 1.0, 0.0, 0, 1.0, 14), (0.7, 0.5, 1.0, 1.0, 1, 1.0, 15), (0.7, 0.5, 1.0, 1.0, 1, 1.0, 16),
	(0.7, 0.5, 1.0, 1.0, 1, 1.0, 17), (0.0, -0.4, 1.0, 0.5, 1, 0.8, 18), (0.9, 0.8, 0.5, 2.0, 0, 0.25, 19),
	(0.2, 0.9, 1.0, 0.0, 1, 1.0, 20))
exhtypes = (1, 5, 6, 10, 11, 12, 13, 14, 15, 20, 21, 25, 26, 27, 30, 35, 36, 40, 45, 50, 55, 60)
# the particles a type needs: the highest index + 1
exhneed = {1: 4, 5: 6, 6: 8, 10: 5, 11: 5, 12: 7, 13: 6, 14: 9, 15: 9, 20: 5, 21: 5, 25: 5, 26: 5, 27: 9, 30: 5, 35: 5,
	36: 6, 40: 5, 45: 5, 50: 9, 55: 5, 60: 6}
# the sides of e where type t fits
def exhfits(e, t):
	ok = 0
	for exhs in exhsides(e):
		p = exhps(e, exhs, 0)
		if exhnum(p) < exhneed[t]:
			return 0
		if t != 1 and t != 5 and exhsprites(p) < 9:
			return 0
		if t == 21:
			p2 = exhps(e, exhs, 1)
			if not p2 or exhnum(p2) < 2 or exhsprites(p2) < 9:
				return 0
		ok = 1
	return ok
def exhrun(name, e, t, inp):
	exhpoke(e + 0x10, t)
	for exhs in (0, 1):
		for exhj in (0, 1):
			p = exhps(e, exhs, exhj)
			if p:
				exhkill(p)
	exhsrand(inp[6])
	check('cVehicleMotorsEffects_Exhaust ' + name + fmt((t,) + inp), lambda e=e, i=inp: (SDTest.Call('cVehicleMotorsEffects_Exhaust',
		'tviffffif', e, i[0], i[1], i[2], i[3], i[4], i[5]), exhstate(e), exhrand()))
def exhtypesrun(name, e, types, inputs):
	for exht in types:
		if not exhfits(e, exht):
			check('EXH skip ' + name + fmt((exht,)), lambda: 1)
			continue
		for exhinp in inputs:
			exhrun(name, e, exht, exhinp)
# the models of the motor m at sides (with a node) enabled or disabled
def exhmodels(m, sides, on):
	for exhs in sides:
		if exhi(m + 0x38 + 4 * exhs) != -1:
			if on:
				SDTest.Call('CGameModel_Enable', 'tvi', m + 0x1c * exhs)
			else:
				SDTest.Call('CGameModel_Disable', 'tvi', m + 0x1c * exhs)
# the particles empty and stopped: no birth, no life, no emit positions
def exhstop(e):
	for exhs in (0, 1):
		for exhj in (0, 1):
			p = exhps(e, exhs, exhj)
			if p:
				exhkill(p)
				exhpoke(p + 0x18, 0)
				exhpoke(p + 0x1c, 0)
				SDTest.Call('CGameParticles_Apply', 'tvi', p)
				SDTest.Call('CGameParticles_SetEmitPos', 'tviii', p, 0, 0)

# ---- SPoli2 (two motor slots) with the motor MPoli1, then MBankMaster1 ----
exhobj, exhc = exhcar('SDExhCar', 58000.0, 'SPoli2', 1)
if exhc:
	exhms = exhc + 0x720
	check('EXH SetMotor MPoli1', lambda: SDTest.Call('cVehicleMotors_SetMotor', 'tiiisfff', exhms, 0, 'MPoli1', 1.0, 1.0, 1.0)[0])
	exhm = exhi(exhc + 0x728)
	exhe = 0
	if exhm:
		exhe = exhm + 0x74
		SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', exhe, 0xd25a14)
		check('EXH MPoli1 effects', lambda: (exheff(exhe), exhints(exhm + 0x38, 2)))
		for exhs in exhsides(exhe):
			check('EXH MPoli1 side %d' % exhs, lambda s=exhs: (exhnum(exhps(exhe, s, 0)), exhsprites(exhps(exhe, s, 0)),
				SDTest.Call('CGameModel_IsEnabled', 'tbi', exhm + 0x1c * s)[0]))
	if exhe and exhsides(exhe):
		# the motor models seen
		exhmodels(exhm, (0, 1), 1)
		exhtypesrun('MPoli1', exhe, exhtypes, exhinputs)
		# no type, unknown types
		for exht in (0, 2, 59, 61, -1):
			exhrun('MPoli1', exhe, exht, exhinputs[0])
		# one side without node
		exhnode = exhi(exhe + 0x1c)
		exhpoke(exhe + 0x1c, -1)
		exhtypesrun('MPoli1 side 0', exhe, (1, 10, 14, 27, 60), exhinputs[:3])
		exhpoke(exhe + 0x1c, exhnode)
		# the color, scale and offset of type 1
		SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', exhe, 0x30a0ff)
		SDTest.Call('cVehicleMotorsEffects_SetScale', 'tvif', exhe, 1.75)
		SDTest.Call('cVehicleMotorsEffects_SetOffset', 'tvif', exhe, -12.5)
		exhtypesrun('MPoli1 scaled', exhe, (1, 5), exhinputs[:2])
		# the motor models disabled: nothing, then side 1 only
		exhmodels(exhm, (0, 1), 0)
		exhrun('MPoli1 disabled', exhe, 10, exhinputs[0])
		exhmodels(exhm, (1,), 1)
		exhrun('MPoli1 side 0 disabled', exhe, 10, exhinputs[0])
		exhmodels(exhm, (0,), 1)
		exhstop(exhe)
	check('EXH SetMotor MBankMaster1', lambda: SDTest.Call('cVehicleMotors_SetMotor', 'tiiisfff', exhms, 0, 'MBankMaster1', 1.0, 1.0,
		1.0)[0])
	exhm = exhi(exhc + 0x728)
	exhe = 0
	if exhm:
		exhe = exhm + 0x74
		SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', exhe, 0x1e64c8)
		check('EXH MBankMaster1 effects', lambda: (exheff(exhe), exhints(exhm + 0x38, 2)))
		for exhs in exhsides(exhe):
			check('EXH MBankMaster1 side %d' % exhs, lambda s=exhs: (exhnum(exhps(exhe, s, 0)),
				exhsprites(exhps(exhe, s, 0)), exhps(exhe, s, 1) and exhnum(exhps(exhe, s, 1)),
				exhps(exhe, s, 1) and exhsprites(exhps(exhe, s, 1))))
	if exhe and exhsides(exhe):
		exhmodels(exhm, (0, 1), 1)
		exhtypesrun('MBankMaster1', exhe, (21, 20, 1), exhinputs)
		exhstop(exhe)
	check('SDExhCar Del', lambda: exhobj.Del())

# ---- SPoli1 (one motor slot) with the motor MPoli3 (45 particles) ----
exhobj, exhc = exhcar('SDExhCar3', 59200.0, 'SPoli1', 1)
if exhc:
	exhms = exhc + 0x720
	check('EXH SetMotor MPoli3', lambda: SDTest.Call('cVehicleMotors_SetMotor', 'tiiisfff', exhms, 0, 'MPoli3', 1.0, 1.0, 1.0)[0])
	exhm = exhi(exhc + 0x728)
	exhe = 0
	if exhm:
		exhe = exhm + 0x74
		SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', exhe, 0x50f0a0)
		check('EXH MPoli3 effects', lambda: (exheff(exhe), exhints(exhm + 0x38, 2)))
		for exhs in exhsides(exhe):
			check('EXH MPoli3 side %d' % exhs, lambda s=exhs: (exhnum(exhps(exhe, s, 0)), exhsprites(exhps(exhe, s, 0)),
				SDTest.Call('CGameModel_IsEnabled', 'tbi', exhm + 0x1c * s)[0]))
	if exhe and exhsides(exhe):
		exhmodels(exhm, (0, 1), 1)
		exhtypesrun('MPoli3', exhe, exhtypes, exhinputs[:3] + exhinputs[4:5])
		exhstop(exhe)
	check('SDExhCar3 Del', lambda: exhobj.Del())

# ---- SLifeBoat: the exhaust a node of the ship model ----
exhobj, exhc = exhcar('SDExhCar4', 59800.0, 'SLifeBoat', 1)
if exhc:
	exhe = exhc + 0x740
	SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', exhe, 0xff8c28)
	check('EXH SLifeBoat effects', lambda: exheff(exhe))
	for exhs in exhsides(exhe):
		check('EXH SLifeBoat side %d' % exhs, lambda s=exhs: (exhnum(exhps(exhe, s, 0)),
			exhsprites(exhps(exhe, s, 0)), SDTest.Call('CGameModel_IsEnabled', 'tbi', exhc + 0xb8)[0]))
	if exhsides(exhe):
		exhtypesrun('SLifeBoat', exhe, exhtypes, exhinputs[:3] + exhinputs[6:7])
		exhstop(exhe)
	check('SDExhCar4 Del', lambda: exhobj.Del())

# ---- the traffic ship TMed2: the exhausts of the ship model ----
exhobj, exhc = exhcar('SDExhCar2', 58600.0, 'Models/Vehicles/OutTraffic/TMed2.M3D', 0)
if exhc:
	exhe = exhc + 0x740
	SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', exhe, 0xc8b496)
	check('EXH TMed2 effects', lambda: exheff(exhe))
	for exhs in exhsides(exhe):
		check('EXH TMed2 side %d' % exhs, lambda s=exhs: (exhnum(exhps(exhe, s, 0)), exhsprites(exhps(exhe, s, 0)),
			SDTest.Call('CGameModel_IsEnabled', 'tbi', exhc + 0xb8)[0]))
	if exhsides(exhe):
		exhtypesrun('TMed2', exhe, exhtypes, exhinputs[:4])
		exhstop(exhe)
	check('SDExhCar2 Del', lambda: exhobj.Del())
