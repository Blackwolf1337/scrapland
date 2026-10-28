# game Entities/Vehicle/VehicleEffects.c and VehicleEntity.c (execfile'd by sdlevel.py, Python 1.5.2): the vehicle
# functions on a vehicle of our own (Scrap.CreateEntity Car "SDVheCar" far away, the model SPoli1 set by SetModel,
# deleted at the end). No frame runs during the script, so the fields only change through the calls; every input is
# poked before the call (the random timeOfs of the constructor and the timer of the entity are not printed, the
# timer is poked where a function reads it). The script variables (VarF, VarI, VarS, VarV) by name, the trust and
# speed values, the ticks, the smoothing, the speed history and the crash, the moves Collide and Move (the flight of
# the exe plus the collision), the pilot getters without a pilot, the effects (flags, Reset, Init, Start of the
# effects on the loaded model, UpdateState) and the pure helpers TurnCurve and SndCrossFade on fake sound items.
# Not called: Update and Tick (the world timer), the network moves (no controllers), SetUsrControl with a player
# slot, EnableAI, SetPilot with a pilot model and the pilot animations (their files), NetRead and NetWrite.
def vhei(a):
	return SDTest.Peek(a, '@i')
def vheb(a):
	return SDTest.Peek(a, '@b')
def vhez(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def vhefree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def vhefl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def vheset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def vhepoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def vhestr(a, s):
	SDTest.Call('Cad_Copy', 'iisi', a, s, len(s) + 1)
def vhesetbyte(a, v):
	w = vhei(a & ~3)
	sh = 8 * (a & 3)
	vhepoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def vheints(a, offs):
	r = ()
	for o in offs:
		r = r + (vhei(a + o),)
	return r
def vhebytes(a, offs):
	r = ()
	for o in offs:
		r = r + (vheb(a + o),)
	return r
def vhesrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def vhefind(name):
	slot = SDTest.Call('HashTable_Slot', 'tiis', vheworld + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', vheworld + 4, name, slot)[0]
	if node:
		return vhei(node)
	return 0

vheworld = SDTest.Peek('g_pWorld', 'i')

# ---- the pure helpers ----
for vhex in (0.0, 0.5, -0.5, 1.0, -1.0, 1.0000001, 1.5, -1.5, 2.0, -3.25, 100.0, -12345.678):
	check('Vehicle_TurnCurve' + fmt((vhex,)), lambda x=vhex: SDTest.Call('Vehicle_TurnCurve', 'ff', x)[0])
vhesa = vhez(0xa0)
vhesb = vhez(0xa0)
def vhesnd(s):
	return vhefl(s + 0x40, 0x60 / 4)
for vhet, vhetimes, vhevola, vhevolb, vhevol4 in ((0.5, 1.0, 0.8, 0.3, 0.6), (2.0, 1.0, 0.8, 0.3, 0.6),
		(-1.0, 1.0, 0.8, 0.3, 0.6), (0.5, 0.0, 0.8, 0.3, 0.6), (0.25, -1.0, 0.7, 0.2, 0.9), (0.0, 3.0, 1.0, 1.0, 0.5)):
	vheset(vhesa + 0x40, (vhevola,))
	vheset(vhesa + 0x48, (vhet,))
	vheset(vhesa + 0x74, (vhetimes,))
	vheset(vhesb + 0x40, (vhevolb,))
	vheset(vhesb + 0x5c, (vhevol4,))
	check('Vehicle_SndCrossFade' + fmt((vhet, vhetimes, vhevola, vhevolb, vhevol4)), lambda: (SDTest.Call('Vehicle_SndCrossFade',
		'vii', vhesa, vhesb), vhesnd(vhesa), vhesnd(vhesb)))
check('Vehicle_SndCrossFade no b', lambda: (SDTest.Call('Vehicle_SndCrossFade', 'vii', vhesa, 0), vhesnd(vhesa)))

# ---- the vehicle of our own ----
vhecarobj = Scrap.CreateEntity('SDVheCar', 54000.0, 50000.0, 50000.0, 'Car')
vhecar = vhefind('SDVheCar')
check('SDVheCar', lambda: (vhecarobj is not None, vhecar != 0))
if vhecar:
	vhec = vhecar
	vhefx = vhec + 0x7f0
	check('cVehicleEntity_GetClassName', lambda c=vhec: SDTest.Call('cVehicleEntity_GetClassName', 'tii', c)[0] - 0x400000)
	for vhetype in ('Car', 'WithLife', 'Obj', 'Entity', 'car', 'WalkChar', ''):
		check('cVehicleEntity_IsType' + fmt((vhetype,)), lambda c=vhec, t=vhetype: SDTest.Call('cVehicleEntity_IsType', 'tbis', c, t)[0])
	check('cVehicleEntity_GetOwnerSlot', lambda c=vhec: SDTest.Call('cVehicleEntity_GetOwnerSlot', 'tii', c)[0])
	check('cVehicleEntity_SetUsrControl(-1)', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetUsrControl', 'tvii', c, -1), vhei(c + 0x410),
		vhefl(vhei(vhec + 0x140) + 0xc, 1), vhei(vhec + 0x7ec), vhei(vhec + 0x4fc)))
	check('effects owner', lambda c=vhec: (vhei(c + 0x7f0) == c, vhei(c + 0x80c) != 0, vhei(c + 0x810) != 0))

	# the model: the effects and motors made for it, the radii, the render nodes of the weapons
	check('cVehicleEntity_SetModel(SPoli1)', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', c, 'SPoli1', 1.0, 1.0,
		1.0, 1), vhei(vhec + 0x51c), vhefl(vhec + 0x458, 1), vhefl(vhec + 0x3a0, 1), SDTest.Peek(vhec + 0x568, '@s'),
		vhei(vhec + 0x224) != 0, vhei(vhec + 0x80c) != 0, vhei(vhec + 0x810) != 0))
	vhetr = vhei(vhec + 0x80c)
	if vhetr:
		check('trails', lambda t=vhetr, c=vhec: (vhei(t) == c, vheints(t, (4, 0x1c, 0x20, 0x24, 0x28, 0x4c)),
			vhefl(t + 8, 1), vhefl(t + 0x2c, 1), vhefl(t + 0x48, 1), vhefl(t + 0x50, 4)))
	vhenodes = vhei(vhec + 0x5f0 + 0x1c)
	if vhenodes:
		check('weapon nodes', lambda n=vhenodes: vheints(n, (0, 4, 8, 12, 16, 20, 24, 28)))
	check('cVehicleEntity_SetModel(SPoli1, path)', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', c,
		'Models/Vehicles/Ships/SPoli1/SPoli1.M3D', 1.0, 1.0, 1.0, 0), SDTest.Peek(vhec + 0x568, '@s'),
		vhefl(vhec + 0x458, 1), vhefl(vhec + 0x3a0, 1)))
	check('cVehicleEntity_GetWRadius', lambda c=vhec: SDTest.Call('cVehicleEntity_GetWRadius', 'tfi', c)[0])
	# the model and the render position fixed (the model update of the exe wobbles with the random timeOfs)
	SDTest.Call('CGameModel_SetPos', 'tviF', vhec + 0xb8, (54000.0, 50000.0, 50000.0))
	SDTest.Call('CGameModel_SetRot', 'tviF', vhec + 0xb8, (0.1, 0.2, 0.0))
	vheset(vhec + 0x94, (0.1, 0.2, 0.0, 54000.0, 50000.0, 50000.0))

	# the pilot without a pilot model
	check('cVehicleEntity_SetPilot(0, "")', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetPilot', 'tbiisfff', c, 0, '', 1.0, 1.0,
		1.0)[0], vhei(vhec + 0x51c)))
	check('cVehicleEntity_SetPilot(1)', lambda c=vhec: SDTest.Call('cVehicleEntity_SetPilot', 'tbiisfff', c, 1, 'x', 1.0, 1.0, 1.0)[0])
	for vheslot in (0, 1):
		check('cVehicleEntity_GetPilot' + fmt((vheslot,)), lambda c=vhec, s=vheslot: SDTest.Call('cVehicleEntity_GetPilot', 'tsii', c,
			s)[0])
		check('cVehicleEntity_GetPilotPos' + fmt((vheslot,)), lambda c=vhec, s=vheslot: SDTest.Call('cVehicleEntity_GetPilotPos', 'tbiiFF',
			c, s, (1.0, 2.0, 3.0), (4.0, 5.0, 6.0)))
	# without a pilot model it returns early (the anim slot is not written)
	check('cVehicleEntity_SetPilotAnim(0, 2)', lambda c=vhec: SDTest.Call('cVehicleEntity_SetPilotAnim', 'tbiii', c, 0, 2)[0])
	check('cVehicleEntity_UpdatePilotPos', lambda c=vhec: SDTest.Call('cVehicleEntity_UpdatePilotPos', 'tvii', c, 1))

	# the script variables
	vhevarf = ('WRadius', 'Style', 'Engine', 'MaxSpeed', 'Fric', 'AcMass', 'StrTrust', 'BackTrust', 'FrwTrust',
		'TrustTime', 'BoostTime', 'FxEngineTime', 'FxBoostEngineTime', 'VerticalBalance', 'EMICounter', 'MotorDCnt',
		'Speed', 'Life', 'SDNoVar', 'Traf_SDNoVar', 'Can_SDNoVar', 'InCtrl_SDNoVar', '')
	vheset(vhec + 0x46c, (1234.5,))
	for vhen in vhevarf:
		check('cVehicleEntity_VarF get' + fmt((vhen,)), lambda c=vhec, n=vhen: SDTest.Call('cVehicleEntity_VarF', 'tbisDi', c, n, 0.0,
			0))
	for vhen, vhev in (('Style', 900.0), ('Engine', 1500.0), ('TrustTime', 2.5), ('BoostTime', 1.0),
			('BoostTime', 3.0), ('FxEngineTime', 0.5), ('FxBoostEngineTime', 0.25), ('VerticalBalance', 0.75),
			('EMICounter', 0.0), ('MotorDCnt', 0.0), ('WRadius', 333.0), ('MaxSpeed', 9000.0), ('Fric', 0.1),
			('Speed', 77.0), ('StrTrust', 1.0), ('AcMass', 3.0)):
		check('cVehicleEntity_VarF set' + fmt((vhen, vhev)), lambda c=vhec, n=vhen, v=vhev: (SDTest.Call('cVehicleEntity_VarF',
			'tbisDi', c, n, v, 1), SDTest.Call('cVehicleEntity_VarF', 'tbisDi', c, n, 0.0, 0)))
	vhevari = ('FixedModel', 'Money', 'SpeedKmH', 'isInit', 'LaunchTaunts', 'MinLife', 'MaxLife', 'UsrControl',
		'isBoost', 'BoostDeact', 'Life', 'SDNoVar', 'Traf_SDNoVar', '')
	for vhen in vhevari:
		check('cVehicleEntity_VarI get' + fmt((vhen,)), lambda c=vhec, n=vhen: SDTest.Call('cVehicleEntity_VarI', 'tbispi', c, n, 0,
			0))
	for vhen, vhev in (('FixedModel', 5), ('FixedModel', 0), ('Money', 1234), ('isInit', 1), ('LaunchTaunts', 3),
			('LaunchTaunts', -1), ('MinLife', 10), ('MaxLife', 300), ('isBoost', 1), ('isBoost', 0),
			('BoostDeact', 2), ('BoostDeact', 0), ('ForceVel', 1), ('ForceAll', 1), ('AvoidCrash', 0),
			('AvoidCrash', 1), ('AnmP0', 1), ('AnmP1', 1)):
		check('cVehicleEntity_VarI set' + fmt((vhen, vhev)), lambda c=vhec, n=vhen, v=vhev: (SDTest.Call('cVehicleEntity_VarI',
			'tbispi', c, n, v, 1), SDTest.Call('cVehicleEntity_VarI', 'tbispi', c, n, 0, 0), vhefl(c + 0x490, 2)))
	vhes = vhez(0x100)
	vhestr(vhes, 'SDVheTarget')
	vhesp = vhez(4)
	vhepoke(vhesp, vhes)
	for vhen in ('Model', 'Target', 'MPil0', 'MPil1', 'SDNoVar', 'Traf_SDNoVar', ''):
		check('cVehicleEntity_VarS get' + fmt((vhen,)), lambda c=vhec, n=vhen: (SDTest.Call('cVehicleEntity_VarS', 'tbisii', c, n,
			vhesp, 0)[0], vhei(vhesp) == c + 0x568, vhei(vhesp) == c + 0x650, vhei(vhesp) == vhes))
		vhepoke(vhesp, vhes)
	check('cVehicleEntity_VarS set Target', lambda c=vhec: (SDTest.Call('cVehicleEntity_VarS', 'tbisii', c, 'Target', vhesp, 1)[0],
		SDTest.Peek(vhec + 0x650, '@s')))
	vhestr(vhes, '')
	check('cVehicleEntity_VarS set MPil0', lambda c=vhec: (SDTest.Call('cVehicleEntity_VarS', 'tbisii', c, 'MPil0', vhesp, 1)[0],
		vhei(vhec + 0x51c)))
	for vhen in ('PPil0', 'APil0', 'PPil3', 'PPil4', 'APil9', 'SDNoVar', 'Traf_SDNoVar', ''):
		check('cVehicleEntity_VarV get' + fmt((vhen,)), lambda c=vhec, n=vhen: SDTest.Call('cVehicleEntity_VarV', 'tbisDDDi', c, n,
			1.0, 2.0, 3.0, 0))

	# the trust and the flight values
	vhepad = SDTest.Peek('g_VehicleCenterPadLock', 'i')
	for vhemass, vhelife, vheminl, vheeng, vhestyle, vhecloud, vhepadv in ((2800.0, 100, 100, 2800.0, 2800.0, 0.0,
			0.0), (1500.0, 250, 100, 3000.0, 1000.0, 0.0, 0.0), (1500.0, 250, 100, 3000.0, 1000.0, 2.0, 0.0),
			(1500.0, 50, 100, 3000.0, 1000.0, 0.0, 1.0)):
		vheset(vhec + 0x230, (vhemass,))
		vhepoke(vhec + 0x238, vhelife)
		vhepoke(vhec + 0x63c, vheminl)
		vheset(vhec + 0x4a4, (vheeng,))
		vheset(vhec + 0x4a0, (vhestyle,))
		vheset(vhec + 0x590, (vhecloud,))
		vheset(symva('g_VehicleCenterPadLock'), (vhepadv,))
		check('cVehicleEntity_UpdateTrust' + fmt((vhemass, vhelife, vheminl, vheeng, vhestyle, vhecloud, vhepadv)),
			lambda c=vhec: (SDTest.Call('cVehicleEntity_UpdateTrust', 'tvi', c), vhefl(c + 0x484, 3), vhefl(c + 0x498, 2),
			vhefl(c + 0x4a8, 1)))
	vhepoke(symva('g_VehicleCenterPadLock'), vhepad)
	vheset(vhec + 0x590, (0.0,))
	check('cVehicleEntity_SetMass', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetMass', 'tvif', c, 2500.0), vhefl(c + 0x230, 1),
		vhefl(vhec + 0x484, 8)))
	vheset(vhec + 0xd4, (300.0, -400.0, 1200.0))
	check('cVehicleEntity_UpdateSpeed', lambda c=vhec: (SDTest.Call('cVehicleEntity_UpdateSpeed', 'tvi', c), vhefl(c + 0x5a0, 8)))
	vheset(vhec + 0xd4, (-30.0, 40.0, 10.0))
	check('cVehicleEntity_StartBoost', lambda c=vhec: (SDTest.Call('cVehicleEntity_StartBoost', 'tvi', c), vhefl(c + 0x5a0, 8),
		vhefl(vhec + 0x4c8, 1)))
	for vhea, vhedt in ((1.0, 0.25), (0.1, 0.25), (0.0, 0.1), (5.0, -1.0)):
		vheset(vhec + 0x490, (vhea,))
		check('cVehicleEntity_BoostTick' + fmt((vhea, vhedt)), lambda c=vhec, dt=vhedt: (SDTest.Call('cVehicleEntity_BoostTick', 'tvif', c,
			dt), vhefl(c + 0x490, 1), SDTest.Call('cVehicleEntity_IsBoostReady', 'tbi', c)[0]))
		vheset(vhec + 0x58c, (vhea,))
		check('cVehicleEntity_EMITick' + fmt((vhea, vhedt)), lambda c=vhec, dt=vhedt: (SDTest.Call('cVehicleEntity_IsEMIed', 'tii', c)[0],
			SDTest.Call('cVehicleEntity_EMITick', 'tvif', c, dt), vhefl(c + 0x58c, 1), SDTest.Call('cVehicleEntity_IsEMIed', 'tii', c)[0]))
		vheset(vhec + 0x590, (vhea,))
		vhesetbyte(vhec + 0x594, 1)
		check('cVehicleEntity_CloudTick' + fmt((vhea, vhedt)), lambda c=vhec, dt=vhedt: (SDTest.Call('cVehicleEntity_CloudTick', 'tvif', c,
			dt), vhefl(c + 0x590, 1), vheb(c + 0x594)))
	for vhefire, vhespeed, vhenf in ((0.0, 100.0, 0.5), (-1.0, 100.0, 0.0), (0.0, 500.0, 0.0), (0.0, 100.0, 0.0),
			(1.0, 0.0, 0.02)):
		vheset(vhec + 0x600, (vhefire,))
		vheset(vhec + 0x46c, (vhespeed,))
		vheset(vhec + 0x598, (vhenf,))
		check('cVehicleEntity_NoFireTick' + fmt((vhefire, vhespeed, vhenf)), lambda c=vhec: (SDTest.Call('cVehicleEntity_NoFireTick',
			'tvif', c, 0.1), vhefl(c + 0x598, 1), SDTest.Call('cVehicleEntity_CanFire', 'tii', c)[0]))
	vheset(vhec + 0x600, (0.0,))
	for vhept in (0.5, 0.0, -1.0):
		vheset(vhec + 0x470, (vhept, 100.0, -50.0, 25.0))
		vheset(vhec + 0xd4, (1.0, 2.0, 3.0))
		check('cVehicleEntity_PushTick' + fmt((vhept,)), lambda c=vhec: (SDTest.Call('cVehicleEntity_PushTick', 'tvif', c, 0.04),
			vhefl(c + 0x470, 1), vhefl(c + 0xd4, 3)))
	check('cVehicleEntity_SetCloud', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetCloud', 'tvifi', c, 4.5, 0x101), vhefl(c + 0x590, 1),
		vheb(vhec + 0x594)))
	vheset(vhec + 0x590, (0.0,))
	vhesetbyte(vhec + 0x594, 0)
	vhepoke(vhec + 0x4fc, 0)
	check('cVehicleEntity_SetNetFlag', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetNetFlag', 'tvii', c, 1), vhei(c + 0x4fc)))
	check('cVehicleEntity_GetLaunchTaunts', lambda c=vhec: SDTest.Call('cVehicleEntity_GetLaunchTaunts', 'tbi', c)[0])
	check('cVehicleEntity_UpdateOff', lambda c=vhec: (vheset(c + 0x490, (2.0,)), SDTest.Call('cVehicleEntity_UpdateOff', 'tvi', c),
		vhefl(vhec + 0x490, 1)))

	# the smoothing: the inputs poked
	def vhesmoothin(c, thrust, speed, turn, dying, f1a4):
		vheset(c + 0x420, (thrust, turn))
		vheset(c + 0x46c, (speed,))
		vheset(c + 0x4ac, (0.1, 0.2, 0.5, 0.25, 0.3, 0.6, 2000.0, 0.4))
		vheset(c + 0xac, (0.3, -2.9, 0.1))
		vheset(c + 0x4cc, (0.25, 3.0, 0.0, 0.5, 0.25, -0.25, 100.0, 200.0, -300.0, 10.0, 20.0, 30.0))
		vheset(c + 0xd4, (500.0, -200.0, 1500.0, 0.5, -0.5, 1.0))
		vhesetbyte(c + 0x1ac, dying)
		vheset(c + 0x1a4, (f1a4, 1.0))
	def vhesmoothout(c):
		return (vhefl(c + 0x4ac, 2), vhefl(c + 0x4bc, 4), vhefl(c + 0x4cc, 12))
	for vheargs in ((0.8, 3000.0, 0.5, 0, 0.0, 0.04), (-0.6, 20000.0, -0.3, 0, 0.0, 0.04),
			(1.0, 100.0, 0.0, 1, 2.0, 0.02), (0.0, 0.0, 1.0, 1, 0.5, 0.0005), (0.5, 16800.0, 2.0, 0, 0.0, 0.2)):
		vhesmoothin(vhec, vheargs[0], vheargs[1], vheargs[2], vheargs[3], vheargs[4])
		check('cVehicleEntity_Smooth' + fmt(vheargs), lambda c=vhec, dt=vheargs[5]: (SDTest.Call('cVehicleEntity_Smooth', 'tvif', c,
			dt), vhesmoothout(c)))
	vhesetbyte(vhec + 0x1ac, 0)

	# the speed history and the crash (the timer of the entity poked)
	vhetimer = vhefl(vhec + 0x118, 1)
	for vhehist, vhehit, vheacclen in ((9.9, 0.0, 0.0), (9.9995, 0.0, 0.0), (9.0, 9.95, 1e9), (9.5, 9.0, 1e9),
			(9.5, 9.99, 0.0)):
		vheset(vhec + 0x118, (10.0,))
		vheset(vhec + 0x59c, (vhehist, 500.0, 100.0, -200.0, 300.0, 5.0, 6.0, -7.0, vheacclen, vhehit))
		vheset(vhec + 0xd4, (150.0, -100.0, 250.0))
		check('cVehicleEntity_UpdateHistory' + fmt((vhehist, vhehit, vheacclen)), lambda c=vhec: (SDTest.Call('cVehicleEntity_UpdateHistory',
			'tvi', c), vhefl(c + 0x46c, 1), vhefl(c + 0x59c, 9)))
	vheset(vhec + 0x118, vhetimer)
	for vheforce in (0.0, 1e5, 1e6, 1e7, 1e9):
		check('cVehicleEntity_Crash' + fmt((vheforce,)), lambda c=vhec, f=vheforce: SDTest.Call('cVehicleEntity_Crash', 'tviif', c, 0,
			f))

	# the moves: the flight of the exe plus the collision
	vhepos = vhefl(vhec + 0x28, 3)
	vheang = vhefl(vhec + 0xac, 3)
	def vhemoveout(c):
		return (vhefl(c + 0x28, 3), vhefl(c + 0xd4, 6), vhefl(c + 0xac, 3), vhefl(c + 0x46c, 1))
	for vhevel in ((100.0, 0.0, 0.0), (0.0, -500.0, 1000.0), (0.0, 0.0, 0.0)):
		vheset(vhec + 0x28, vhepos)
		vheset(vhec + 0xac, vheang)
		vheset(vhec + 0xd4, vhevel + (0.0, 0.0, 0.0))
		vhefrom = vhez(12)
		vheset(vhefrom, (vhepos[0] - 10.0, vhepos[1] + 5.0, vhepos[2]))
		check('cVehicleEntity_Collide' + fmt(vhevel), lambda c=vhec, f=vhefrom: (SDTest.Call('cVehicleEntity_Collide', 'tviif', c, f,
			0.04), vhemoveout(c)))
		vhefree(vhefrom)
		vheset(vhec + 0x28, vhepos)
		vheset(vhec + 0xd4, vhevel + (0.0, 0.0, 0.0))
		check('cVehicleEntity_Move' + fmt(vhevel), lambda c=vhec: (SDTest.Call('cVehicleEntity_Move', 'tvif', c, 0.04),
			vhemoveout(c)))
	vheset(vhec + 0x28, vhepos)
	vheset(vhec + 0xac, vheang)
	vheset(vhec + 0xd4, (0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
	vhesp2 = vhez(12)
	vheset(vhesp2, (-100.0, 50.0, 25.0))
	check('cVehicleEntity_SetVel', lambda c=vhec, v=vhesp2: (SDTest.Call('cVehicleEntity_SetVel', 'tvii', c, v), vhefl(c + 0xd4, 3),
		vhefl(vhec + 0x46c, 1)))
	vheset(vhesp2, (40000.0, 30000.0, 0.0))
	check('cVehicleEntity_SetVel fast', lambda c=vhec, v=vhesp2: (SDTest.Call('cVehicleEntity_SetVel', 'tvii', c, v),
		vhefl(vhec + 0xd4, 3), vhefl(vhec + 0x46c, 1)))
	vhefree(vhesp2)
	vheset(vhec + 0xd4, (0.0, 0.0, 0.0))
	# the touch of two vehicles: bounce, pushed apart (moving apart, the same place)
	vhecar2obj = Scrap.CreateEntity('SDVheCar2', 54300.0, 50000.0, 50000.0, 'Car')
	vhecar2 = vhefind('SDVheCar2')
	check('SDVheCar2', lambda: (vhecar2obj is not None, vhecar2 != 0))
	if vhecar2:
		vhepos2 = vhefl(vhecar2 + 0x28, 3)
		vhenorm = vhez(12)
		vheset(vhenorm, (1.0, 0.0, 0.0))
		for vhep1, vhev1, vhev2 in (((0.0, 0.0, 0.0), (100.0, 0.0, 0.0), (-50.0, 0.0, 0.0)),
				((0.0, 0.0, 0.0), (-100.0, 10.0, 0.0), (50.0, 0.0, 0.0)), ((300.0, 0.0, 0.0), (0.0, 0.0, 0.0),
				(0.0, 0.0, 0.0)), ((295.0, 1.0, 2.0), (5.0, 0.0, 0.0), (0.0, 3.0, 0.0))):
			vheset(vhec + 0x28, (vhepos[0] + vhep1[0], vhepos[1] + vhep1[1], vhepos[2] + vhep1[2]))
			vheset(vhecar2 + 0x28, vhepos2)
			vheset(vhec + 0xd4, vhev1)
			vheset(vhecar2 + 0xd4, vhev2)
			check('cVehicleEntity_Touch' + fmt((vhep1, vhev1, vhev2)), lambda c=vhec, o=vhecar2: (SDTest.Call('cVehicleEntity_Touch',
				'tviiii', c, o, 0, vhenorm), vhefl(c + 0xd4, 3), vhefl(o + 0xd4, 3), vhefl(c + 0x46c, 1),
				vhefl(o + 0x46c, 1)))
		vhefree(vhenorm)
		vheset(vhecar2 + 0x28, vhepos2)
		vheset(vhec + 0x28, vhepos)
		vheset(vhec + 0xd4, (0.0, 0.0, 0.0))
		check('SDVheCar2 Del', lambda: vhecar2obj.Del())
	check('cVehicleEntity_GetCamPoint', lambda c=vhec: SDTest.Call('cVehicleEntity_GetCamPoint', 'tviFF', c, (0.0, 0.0, 0.0),
		(0.0, 0.0, 0.0)))
	check('cVehicleEntity_UpdateSounds', lambda c=vhec: SDTest.Call('cVehicleEntity_UpdateSounds', 'tvif', c, 0.04))

	# the effects
	def vhefxstate(f):
		return (vhebytes(f, (4, 0xc, 0x14)), vhefl(f + 8, 1), vhefl(f + 0x10, 1), vhefl(f + 0x18, 1))
	# a running timer starts the damage effects, which read a node SPoli1 does not have (a crash in both runs)
	for vhelife, vhemaxl, vheemi, vheburn, vheon, vhetime in ((100, 300, 0.0, 0.0, 0, 0.0),
			(20, 300, 0.0, 0.0, 0, 0.0), (0, 300, 0.0, 0.0, 0, 0.0), (250, 300, 0.6, 0.6, 0, 0.0)):
		vhepoke(vhec + 0x238, vhelife)
		vhepoke(vhec + 0x640, vhemaxl)
		vheset(vhec + 0x58c, (vheemi,))
		vheset(vhec + 0x294, (vheburn,))
		for vheo in (4, 0xc, 0x14):
			vhesetbyte(vhefx + vheo, vheon)
		vheset(vhefx + 8, (vhetime,))
		vheset(vhefx + 0x10, (vhetime,))
		vheset(vhefx + 0x18, (vhetime,))
		check('cVehicleEffects_UpdateState' + fmt((vhelife, vhemaxl, vheemi, vheburn, vheon, vhetime)),
			lambda f=vhefx: (SDTest.Call('cVehicleEffects_UpdateState', 'tvif', f, 0.02), vhefxstate(f)))
	vheset(vhec + 0x58c, (0.0,))
	vheset(vhec + 0x294, (0.0,))
	vhepoke(vhec + 0x238, 300)
	check('cVehicleEffects_ClearDamage', lambda f=vhefx: (SDTest.Call('cVehicleEffects_ClearDamage', 'tvi', f), vhefxstate(f)))
	check('cVehicleEffects_Reset', lambda f=vhefx: (SDTest.Call('cVehicleEffects_Reset', 'tvi', f), vhefxstate(f)))
	check('cVehicleEffects_SetActive(0)', lambda f=vhefx: SDTest.Call('cVehicleEffects_SetActive', 'tvii', f, 0))
	check('cVehicleEffects_SetOwner', lambda f=vhefx, c=vhec: (SDTest.Call('cVehicleEffects_SetOwner', 'tvii', f, c), vhei(f) == c))
	check('cVehicleEffects_Init', lambda f=vhefx, c=vhec: (SDTest.Call('cVehicleEffects_Init', 'tvi', f), vhei(f + 0x1c) != 0,
		vhei(f + 0x20) != 0))
	vhetr = vhei(vhefx + 0x1c)
	if vhetr:
		check('trails again', lambda t=vhetr, c=vhec: (vhei(t) == c, vheints(t, (4, 0x1c, 0x20, 0x24, 0x28, 0x4c)),
			vhefl(t + 0x54, 3)))
	if vhetr:
		vhecen = SDTest.Call(vhei(vhei(vhec) + 0x74) - 0x400000, 'tiiF', vhec, (0.0, 0.0, 0.0))[1]
		vhesrand(2468)
		vheset(vhec + 0x498, (9000.0,))
		for vhelz, vhefade, vhedist, vhelast, vhetime in ((-20000.0, 0.6, 0.0, 5000.0, 0.0),
				(-20000.0, 0.9, 10.0, 1.0, 0.02), (-20000.0, 0.3, 10.0, 1.0, 0.06), (-20000.0, 0.3, 10.0, 1.0, 0.01),
				(5.0, 0.2, 0.0, 1.0, 0.0), (5.0, 0.2, 3.0, 1.0, 0.0), (-1000.0, 0.7, 0.0, 300.0, 0.0)):
			vheset(vhec + 0x4e4, (300.0, 0.0, vhelz))
			vheset(vhetr + 8, (vhefade,))
			vheset(vhetr + 0x2c, (vhedist, vhecen[0] + vhelast, vhecen[1], vhecen[2]))
			vheset(vhetr + 0x48, (vhetime,))
			vheset(vhetr + 0x50, (900.0,))
			check('cVehicleTrails_Update' + fmt((vhelz, vhefade, vhedist, vhelast, vhetime)), lambda t=vhetr: (
				SDTest.Call('cVehicleTrails_Update', 'tvif', t, 0.04), vhefl(t + 8, 1), vhefl(t + 0x2c, 4), vhefl(t + 0x48, 1),
				vhefl(t + 0x50, 1)))
		vheset(vhec + 0x4e4, (0.0, 0.0, 0.0))
	vhesrand(1357)
	# the vehicle effects read a node SPoli1 does not have (a crash in both runs): only names without one
	for vhename in ('SDNoEffect', ''):
		check('cVehicleEffects_Start' + fmt((vhename,)), lambda f=vhefx, n=vhename: SDTest.Call('cVehicleEffects_Start', 'tvis', f,
			n))

	# active off and on again
	check('cVehicleEntity_SetActive(0)', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetActive', 'tvii', c, 0), vheb(c + 0x4c),
		vhefl(vhec + 0x4c0, 1)))
	check('cVehicleEntity_SetActive(0) again', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetActive', 'tvii', c, 0), vheb(c + 0x4c)))
	check('cVehicleEntity_SetActive(1)', lambda c=vhec: (SDTest.Call('cVehicleEntity_SetActive', 'tvii', c, 1), vheb(c + 0x4c)))
	vhefree(vhes)
	vhefree(vhesp)
vhefree(vhesa)
vhefree(vhesb)
if vhecarobj:
	check('SDVheCar Del', lambda: vhecarobj.Del())
