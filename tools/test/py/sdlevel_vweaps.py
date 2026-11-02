# game Entities/WithLifeEntity.c, Entities/Vehicle/Weaps/Tesla.c and Vulcan.c (execfile'd by sdlevel.py, Python
# 1.5.2). A WithLife entity of our own (Scrap.CreateEntity WithLife "SDVwpLife" far away, deleted at the end) for the
# small virtual functions, the script variables (VarF, VarI, VarS, VarV by rva), SetPos, SetUsrName, SetActCtrl
# without a class, OnDamage and OnDeath without functions, CheckLife without a life loss and the fire (Burn, its
# fields set back before the delete). FadeSounds and SetSndTime on a fake object with fake sound items (playing set,
# so SetVolume never starts a sound). Then two cars of our own (SDVwpCar, SDVwpCar2 far away) for the Tesla and
# Vulcan natives through the call builder (Press, Release, FX on a Text item, Create, Fire) and the cdecl helpers
# by rva (AimOn, Push, Shoot), the weapon slots, levels and ammo poked; the burner link between the cars.
# Not called: Update, UpdateOff and Tick (the world timer), RenderNow, SetEmbeddedFX, CheckLife with a life loss (the
# OnGeneralDamage script), a class for SetActCtrl, the upgrade ends that run RestoreUpgradeFunc, the Tesla ray effect
# (TeslaCreate with the flag of a running shot set) and the Vulcan shells (no player camera on the cars).
def vwpi(a):
	return SDTest.Peek(a, '@i')
def vwpb(a):
	return SDTest.Peek(a, '@b')
def vwpz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def vwpfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def vwpfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def vwpset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def vwppoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def vwpstr(a, s):
	SDTest.Call('String_Copy', 'iisi', a, s, len(s) + 1)
def vwpsetbyte(a, v):
	w = vwpi(a & ~3)
	sh = 8 * (a & 3)
	vwppoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def vwpints(a, offs):
	r = ()
	for o in offs:
		r = r + (vwpi(a + o),)
	return r
def vwpsrand(s):
	SDTest.Call('crt_srand', 'vi', s)
# the next rand() of the exe: shows how many the call before used
def vwprand():
	return SDTest.Call('crt_rand', 'i')[0]
def vwpsat(p):
	if p:
		return SDTest.Peek(p, '@s')
	return None
def vwpfind(name):
	slot = SDTest.Call('HashTable_Slot', 'tiis', vwpworld + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', vwpworld + 4, name, slot)[0]
	if node:
		return vwpi(node)
	return 0
def vwpcall(func, params):
	Scrap.SetCallFunc(func)
	for p in params:
		if type(p) == type(''):
			Scrap.AddParams(p)
		elif type(p) == type(0):
			Scrap.AddParami(p)
		else:
			Scrap.AddParamf(p)
	Scrap.Execute()

vwpworld = SDTest.Peek('g_pWorld', 'i')
vwps = vwpz(0x100)
vwpsp = vwpz(4)
# VarS by rva: the string pointer in vwpsp (vwps before), the result and the string after
def vwpvars(l, n, set):
	vwppoke(vwpsp, vwps)
	r = SDTest.Call('cWithLifeEntity_VarS', 'tbisii', l, n, vwpsp, set)[0]
	return (r, vwpi(vwpsp) == vwps, vwpsat(vwpi(vwpsp)))

# ---- the WithLife entity ----
vwplife = Scrap.CreateEntity('SDVwpLife', 61000.0, 50000.0, 50000.0, 'WithLife')
vwpl = vwpfind('SDVwpLife')
check('SDVwpLife', lambda: (vwplife is not None, vwpl != 0))
if vwpl:
	check('cWithLifeEntity_ctor', lambda l=vwpl: (vwpi(l) == symva('cWithLifeEntity_vftable'), vwpi(l + 0x48), vwpi(l + 0x22c),
		vwpfl(l + 0x230, 1), vwpi(l + 0x234), vwpi(l + 0x238), vwpb(l + 0x23c), vwpfl(l + 0x28c, 3),
		vwpfl(l + 0x228, 1), vwpi(l + 0x3e4), vwpi(l + 0x24c) == l, vwpi(l + 0x25c) == l, vwpi(l + 0x2d4) == l,
		vwpi(l + 0x39c) == l, vwpi(l + 0x3c0) == l, vwpi(l + 0x3dc) == l, SDTest.Peek(l + 0x3bc, '@s'),
		SDTest.Peek(l + 0x3d8, '@s'), vwpi(l + 0x3ac), vwpi(l + 0x3c8), vwpi(l + 0x240) == l + 0x240,
		vwpi(l + 0x250), vwpi(l + 0x3e0), vwpfl(l + 0x3a0, 1)))
	check('cWithLifeEntity_GetClassName', lambda l=vwpl: SDTest.Call('cWithLifeEntity_GetClassName', 'tsi', l)[0])
	for vwpn in ('WithLife', 'Obj', 'Entity', 'Car', 'SDNoType', ''):
		check('cWithLifeEntity_IsType' + fmt((vwpn,)), lambda l=vwpl, n=vwpn: SDTest.Call('cWithLifeEntity_IsType', 'tiis', l, n)[0] & 0xff)
	check('cWithLifeEntity small', lambda l=vwpl: (SDTest.Call('cWithLifeEntity_GetRadius', 'tfi', l)[0],
		SDTest.Call('cWithLifeEntity_GetNodeSlot', 'tii', l)[0], SDTest.Call('cWithLifeEntity_IsEMIed', 'tbi', l)[0], SDTest.Call('cWithLifeEntity_GetBurner', 'tii', l)[0],
		SDTest.Call('cWithLifeEntity_GetAction', 'tiis', l, 'Walk')[0], SDTest.Call('cWithLifeEntity_GetSndPos', 'tiiF', l, (0.0, 0.0, 0.0))[1]))

	# the script variables
	for vwpn in ('Mass', 'FireRemainTime', 'FireAtenuationFactor', 'FireDelayTime', 'VRadius', 'ConeDist', 'SDNoVar',
			'Hit_SDNoVar', ''):
		check('cWithLifeEntity_VarF get' + fmt((vwpn,)), lambda l=vwpl, n=vwpn: SDTest.Call('cWithLifeEntity_VarF', 'tbisDi', l, n,
			0.0, 0))
	for vwpn, vwpv in (('FireRemainTime', 2.5), ('FireAtenuationFactor', 0.5), ('FireDelayTime', 0.25),
			('Mass', 300.0), ('VRadius', 40.0), ('FireAtenuationFactor', -1.0), ('FireRemainTime', 0.0),
			('FireDelayTime', 0.0)):
		check('cWithLifeEntity_VarF set' + fmt((vwpn, vwpv)), lambda l=vwpl, n=vwpn, v=vwpv: (SDTest.Call('cWithLifeEntity_VarF',
			'tbisDi', l, n, v, 1), SDTest.Call('cWithLifeEntity_VarF', 'tbisDi', l, n, 0.0, 0)))
	for vwpn in ('HeadAnimIndex', 'Invulnerable', 'Life', 'PrevLife', 'Hit_SDNoVar', 'SDNoVar', ''):
		check('cWithLifeEntity_VarI get' + fmt((vwpn,)), lambda l=vwpl, n=vwpn: SDTest.Call('cWithLifeEntity_VarI', 'tbispi', l, n,
			0, 0))
	for vwpn, vwpv in (('HeadAnimIndex', 7), ('Invulnerable', 5), ('Life', 77), ('PrevLife', 33),
			('Invulnerable', 0), ('Life', 77), ('PrevLife', 33), ('HeadAnimIndex', 0)):
		check('cWithLifeEntity_VarI set' + fmt((vwpn, vwpv)), lambda l=vwpl, n=vwpn, v=vwpv: (SDTest.Call('cWithLifeEntity_VarI',
			'tbispi', l, n, v, 1), SDTest.Call('cWithLifeEntity_VarI', 'tbispi', l, n, 0, 0), vwpb(l + 0x23c)))
	vwpstr(vwps, 'SDVwpBuf')
	for vwpn in ('Descriptor', 'Attacker', 'ActCtrl', 'HeadModel', 'Pyromaniac', 'Hit_Target', 'SDNoVar',
			'Hit_SDNoVar', 'OnDeath', ''):
		check('cWithLifeEntity_VarS get' + fmt((vwpn,)), lambda l=vwpl, n=vwpn: vwpvars(l, n, 0))
	for vwpn, vwpv in (('Descriptor', 'SDVwpDesc'), ('HeadModel', 'SDVwpHead'), ('OnDeath', ''), ('OnDamage', ''),
			('ActCtrl', ''), ('ActCtrl', 'SDVwpNoClass'), ('Descriptor', ''), ('Attacker', 'SDVwpNo')):
		vwpstr(vwps, vwpv)
		check('cWithLifeEntity_VarS set' + fmt((vwpn, vwpv)), lambda l=vwpl, n=vwpn: (vwpvars(l, n, 1),
			vwpvars(l, n, 0), vwpi(l + 0x234)))
	for vwpn in ('Vel', 'Hit_Pos', 'Hit_SDNoVar', 'SDNoVar', ''):
		check('cWithLifeEntity_VarV get' + fmt((vwpn,)), lambda l=vwpl, n=vwpn: SDTest.Call('cWithLifeEntity_VarV', 'tbisDDDi', l,
			n, 1.0, 2.0, 3.0, 0))
	check('cWithLifeEntity_VarV set Vel', lambda l=vwpl: (SDTest.Call('cWithLifeEntity_VarV', 'tbisDDDi', l, 'Vel', 1.0, -2.5,
		3.25, 1), SDTest.Call('cWithLifeEntity_VarV', 'tbisDDDi', l, 'Vel', 0.0, 0.0, 0.0, 0), vwpfl(l + 0xd4, 3)))
	SDTest.Call('cWithLifeEntity_VarV', 'tbisDDDi', vwpl, 'Vel', 0.0, 0.0, 0.0, 1)

	# the setters
	check('cWithLifeEntity_SetPos', lambda l=vwpl: (SDTest.Call('cWithLifeEntity_SetPos', 'tviF', l, (61000.0, 50100.0, 50000.0)),
		vwpfl(l + 0x28, 3)))
	check('cWithLifeEntity_SetActive 1', lambda l=vwpl: (SDTest.Call('cWithLifeEntity_SetActive', 'tvii', l, 1), vwpb(l + 0x4c)))
	check('cWithLifeEntity_SetUsrName', lambda l=vwpl: (SDTest.Call('cWithLifeEntity_SetUsrName', 'tvis', l, 'SDVwpUsr'),
		vwpsat(vwpi(l + 0x250)), SDTest.Call('cWithLifeEntity_SetUsrName', 'tvii', l, symva('str_Empty')), vwpi(l + 0x250)))
	for vwpn in ('', 'SDVwpNoClass'):
		check('cWithLifeEntity_SetActCtrl' + fmt((vwpn,)), lambda l=vwpl, n=vwpn: (SDTest.Call('cWithLifeEntity_SetActCtrl', 'tvis', l,
			n), vwpi(l + 0x234)))
	check('cWithLifeEntity_SetActCtrl NULL', lambda l=vwpl: (SDTest.Call('cWithLifeEntity_SetActCtrl', 'tvii', l, 0), vwpi(l + 0x234)))
	check('cWithLifeEntity_OnDamage', lambda l=vwpl: SDTest.Call('cWithLifeEntity_OnDamage', 'tvii', l, 5))
	check('cWithLifeEntity_OnDeath', lambda l=vwpl: SDTest.Call('cWithLifeEntity_OnDeath', 'tvi', l))
	for vwpprev, vwplf in ((0, 50), (40, 50), (50, 50), (-5, -10), (0, -3)):
		vwppoke(vwpl + 0x22c, vwpprev)
		vwppoke(vwpl + 0x238, vwplf)
		check('cWithLifeEntity_CheckLife' + fmt((vwpprev, vwplf)), lambda l=vwpl: (SDTest.Call('cWithLifeEntity_CheckLife', 'tvi', l),
			vwpi(l + 0x22c), vwpi(l + 0x238)))

	# the fire of the Napalm (no burner: the attacker stays empty)
	for vwprem, vwpdel, vwpatt, vwpacc, vwplf, vwpgod, vwpdt in ((0.0, 0.25, 0.5, 0.0, 100, 0, 0.05),
			(2.0, 0.0, 0.5, 0.0, 100, 0, 0.05), (2.0, 0.25, -1.0, 0.0, 100, 0, 0.05),
			(2.0, 0.25, -1.5, 0.0, 100, 0, 0.05), (2.0, 0.25, 0.5, 0.0, 100, 0, 0.05),
			(2.0, 0.25, 0.5, 0.01, 100, 0, 0.05), (2.0, 0.1, 0.0, -0.5, 100, 0, 0.3), (2.0, 0.25, 0.5, 0.0, 100, 1, 0.05),
			(1.0, 0.01, 3.0, 0.0, 100, 0, 2.0), (0.5, 0.3, -0.75, 0.2, 10, 0, 0.1)):
		vwpset(vwpl + 0x28c, (vwpatt, vwpdel, vwprem))
		vwpset(vwpl + 0x228, (vwpacc,))
		vwppoke(vwpl + 0x238, vwplf)
		vwpsetbyte(vwpl + 0x23c, vwpgod)
		check('cWithLifeEntity_Burn' + fmt((vwprem, vwpdel, vwpatt, vwpacc, vwplf, vwpgod, vwpdt)), lambda l=vwpl,
			dt=vwpdt: (SDTest.Call('cWithLifeEntity_Burn', 'tvif', l, dt), vwpfl(l + 0x228, 1), vwpfl(l + 0x294, 1),
			vwpi(l + 0x238), vwpvars(l, 'Attacker', 0), vwpb(l + 0x30c)))
	vwpset(vwpl + 0x28c, (-1.0, 0.0, 0.0))
	vwpset(vwpl + 0x228, (0.0,))
	vwpsetbyte(vwpl + 0x23c, 0)
	vwppoke(vwpl + 0x238, 100)
	vwppoke(vwpl + 0x22c, 100)

# ---- FadeSounds and SetSndTime on a fake object (the life sounds at +0x3c4: count, items) ----
vwpfake = vwpz(0x400)
vwparr = vwpz(8)
vwpit = vwpz(0xa0)
vwppoke(vwpfake + 0x3c8, 2)
vwppoke(vwpfake + 0x3cc, vwparr)
vwppoke(vwparr, vwpit)
def vwpitem(it):
	return (vwpfl(it + 0x40, 3), vwpb(it + 0xc), vwpb(it + 0x9c))
for vwptime, vwpt0, vwpt1, vwpv0, vwpvol, vwpdt in ((0.5, 1.0, 0.25, 0.8, 0.1, 0.05), (0.0, 1.0, 0.25, 0.8, 0.1, 0.05),
		(-0.5, 0.0, 0.25, 0.8, 0.1, 0.05), (-0.5, 0.001, 0.25, 0.8, 0.1, 0.05), (-0.5, 1.0, 0.25, 0.8, 0.1, 0.05),
		(-0.5, 0.1, 0.25, 0.8, 0.79, 0.05), (-0.2, 1.0, 0.25, 0.8, 0.5, 0.05), (-0.2, 1.0, 0.25, 0.8, 0.1, 0.05),
		(-0.2, 1.0, 0.0, 0.8, 0.1, 0.05), (-0.25, 1.0, 0.25, 0.8, 0.3, 0.05), (-3.0, 2.0, 1.0, 0.5, 0.0, 0.1)):
	vwpset(vwpit + 0x40, (vwpvol, 0.5, vwptime, vwpv0))
	vwpset(vwpit + 0x60, (1.25,))
	vwpset(vwpit + 0x74, (vwpt0, vwpt1))
	vwppoke(vwpit + 0x9c, 1)
	vwppoke(vwpit + 0xc, 0)
	check('cWithLifeEntity_FadeSounds' + fmt((vwptime, vwpt0, vwpt1, vwpv0, vwpvol, vwpdt)), lambda dt=vwpdt: (
		SDTest.Call('cWithLifeEntity_FadeSounds', 'tvif', vwpfake, dt), vwpitem(vwpit)))
for vwpt, vwpidx in ((0.25, 0), (1.5, 0), (-2.0, 0), (0.5, 1), (0.5, 2), (0.5, -1)):
	check('cWithLifeEntity_SetSndTime' + fmt((vwpt, vwpidx)), lambda t=vwpt, i=vwpidx: (SDTest.Call('cWithLifeEntity_SetSndTime',
		'tvifi', vwpfake, t, i), vwpfl(vwpit + 0x48, 1)))
vwpfree(vwpit)
vwpfree(vwparr)
vwpfree(vwpfake)

# ---- the Tesla and the Vulcan on cars of our own ----
vwpcar = Scrap.CreateEntity('SDVwpCar', 60000.0, 50000.0, 50000.0, 'Car')
vwpcar2 = Scrap.CreateEntity('SDVwpCar2', 60300.0, 50000.0, 50000.0, 'Car')
vwpc = vwpfind('SDVwpCar')
vwpc2 = vwpfind('SDVwpCar2')
check('SDVwpCar', lambda: (vwpcar is not None, vwpcar2 is not None, vwpc != 0, vwpc2 != 0))
if vwpc and vwpc2:
	vwpwm = vwpc + 0x5f0
	vwpws = vwpi(vwpwm + 0x18)
	vwpam = vwpi(vwpwm + 0x20)
	vwptw = SDTest.Peek('g_TeslaIDWeap', 'i')
	vwpta = SDTest.Peek('g_TeslaIDAmmo', 'i')
	vwpvw = SDTest.Peek('g_VulcanIDWeap', 'i')
	vwpva = SDTest.Peek('g_VulcanIDAmmo', 'i')
	check('SDVwpCar weapons', lambda: (vwptw, vwpta, vwpvw, vwpva, vwpints(vwpws + 12 * vwptw, (0, 4)),
		vwpints(vwpws + 12 * vwpvw, (0, 4)), vwpfl(vwpam + 4 * vwpta, 1), vwpfl(vwpam + 4 * vwpva, 1),
		vwpi(vwpc + 0x614), vwpi(vwpc + 0xf0), vwpi(vwpc2 + 0xf0)))
	# the aim help stays off (its Update searches the world from the vehicle)
	vwpaimon = vwpi(vwpc + 0x71c)
	vwpsetbyte(vwpc + 0x71c, 0)
	vwppad = SDTest.Peek('g_VehicleCenterPadLock', 'i')
	def vwpaim(c):
		return (vwpi(c + 0x718), vwpfl(c + 0x6d4, 2), vwpfl(c + 0x6f4, 5), vwpb(c + 0x636))
	def vwpvel(c):
		return SDTest.Call(vwpi(vwpi(c) + 0x88) - 0x400000, 'tiiF', c, (0.0, 0.0, 0.0))[1]
	def vwpsetvel(c, v):
		SDTest.Call(vwpi(vwpi(c) + 0x8c) - 0x400000, 'tviF', c, v)
	def vwpweap(w, a):
		return (vwpints(vwpws + 12 * w, (0, 4)), vwpfl(vwpws + 12 * w + 8, 1), vwpfl(vwpam + 4 * a, 1),
			vwpfl(vwpwm + 0x10, 1), vwpi(vwpwm + 0x24))

	# the burner link between the cars
	check('cWithLifeEntity_SetBurner', lambda: (SDTest.Call('cWithLifeEntity_SetBurner', 'tvii', vwpc, vwpc2),
		SDTest.Call('cWithLifeEntity_GetBurner', 'tii', vwpc)[0] == vwpc2, vwpvars(vwpc, 'Pyromaniac', 0)))
	vwpstr(vwps, 'SDVwpCar')
	check('cWithLifeEntity_VarS set Pyromaniac', lambda: (vwpvars(vwpc, 'Pyromaniac', 1),
		SDTest.Call('cWithLifeEntity_GetBurner', 'tii', vwpc)[0] == vwpc, vwpvars(vwpc, 'Pyromaniac', 0)))
	vwpstr(vwps, 'SDVwpCar2')
	check('cWithLifeEntity_VarS set Pyromaniac 2', lambda: (vwpvars(vwpc, 'Pyromaniac', 1),
		SDTest.Call('cWithLifeEntity_GetBurner', 'tii', vwpc)[0] == vwpc2))

	# the aim helps
	for vwpp in (0.0, 1.0):
		vwpset(symva('g_VehicleCenterPadLock'), (vwpp,))
		check('Tesla_AimOn' + fmt((vwpp,)), lambda: (SDTest.Call('Tesla_AimOn', 'vi', vwpc), vwpaim(vwpc)))
		for vwpup in (0, 1, 2):
			vwppoke(vwpws + 12 * vwpvw + 4, vwpup)
			check('Vulcan_AimOn' + fmt((vwpp, vwpup)), lambda: (SDTest.Call('Vulcan_AimOn', 'vi', vwpc), vwpaim(vwpc)))
	vwppoke(symva('g_VehicleCenterPadLock'), vwppad)
	vwppoke(vwpws + 12 * vwpvw + 4, 0)
	for vwpf in ('!TeslaPress', '!TeslaRelease', '!VulcanPress', '!VulcanRelease', '!TeslaPress'):
		check('native ' + vwpf, lambda f=vwpf: (vwpcall(f, ('SDVwpCar',)), vwpaim(vwpc)))
	vwpsetbyte(vwpc + 0x636, 0)

	# the pushes on SDVwpCar2
	check('SDVwpCar2', lambda: (vwpfl(vwpc2 + 0x230, 1), vwpvel(vwpc2), vwpfl(vwpc2 + 0x470, 4)))
	for vwpup, vwpn in ((0, 1), (0, 3), (5, 2), (5, 0), (6, 1)):
		vwppoke(vwpws + 12 * vwptw + 4, vwpup)
		vwpsetvel(vwpc2, (10.0, -20.0, 30.0))
		check('Tesla_Push' + fmt((vwpup, vwpn)), lambda n=vwpn: (SDTest.Call('Tesla_Push', 'viii', vwpc, vwpc2, n),
			vwpvel(vwpc2), vwpfl(vwpc2 + 0x470, 4)))
	check('Tesla_Push none', lambda: SDTest.Call('Tesla_Push', 'viii', vwpc, 0, 1))
	check('Tesla_Push life', lambda: (SDTest.Call('Tesla_Push', 'viii', vwpc, vwpl, 2), vwpfl(vwpl + 0xd4, 3)))
	vwppoke(vwpws + 12 * vwptw + 4, 0)
	for vwpn in (1, 4, 0):
		vwpsetvel(vwpc2, (10.0, -20.0, 30.0))
		check('Vulcan_Push' + fmt((vwpn,)), lambda n=vwpn: (SDTest.Call('Vulcan_Push', 'viii', vwpc, vwpc2, n),
			vwpvel(vwpc2)))
	check('Vulcan_Push none', lambda: SDTest.Call('Vulcan_Push', 'viii', vwpc, 0, 1))
	vwpsetvel(vwpc2, (0.0, 0.0, 0.0))
	if vwpl:
		SDTest.Call('cWithLifeEntity_VarV', 'tbisDDDi', vwpl, 'Vel', 0.0, 0.0, 0.0, 1)

	# the HUD items on a Text item of player 0
	SScorer.New(0, 'SDVwpItem', 'Text')
	vwpitm = SDTest.Call('cScorer_FindItem', 'tiis', SDTest.Call('Scorer_Get', 'ii', 0)[0], 'SDVwpItem')[0]
	check('SDVwpItem', lambda: vwpitm != 0)
	if vwpitm:
		def vwphud(it):
			return (vwpfl(it + 0xb4, 3), vwpi(it + 0xc0))
		vwpsaved = vwpints(vwpitm + 0xb4, (0, 4, 8, 12))
		vwpsc = SDTest.Call('Scorer_Get', 'ii', 0)[0]
		vwpscdt = vwpi(vwpsc + 0x28)
		vwpaims = vwpints(vwpc + 0x6d8, (0, 0x24, 0x28, 0x2c))
		for vwpup, vwpfire, vwpspd, vwpang, vwpmax, vwpmin, vwprot, vwpdt in ((0, 0, 0.0, 0.3, 0.5, 0.1, 1.0, 0.04),
				(0, 0, 0.0, 0.3, 0.5, 0.1, -0.01, 0.04), (0, 0, 0.0, 0.3, 0.5, 0.1, 7.0, 0.1),
				(0, 0, -1.0, 0.3, 0.5, 0.1, 1.0, 0.04), (0, 0, -1.0, 0.1, 0.5, 0.1, 1.0, 0.04),
				(6, 1, -1.0, 0.12, 0.5, 0.1, 1.0, 0.04), (6, 0, -1.0, 0.12, 0.5, 0.1, 1.0, 0.04),
				(6, 1, -1.0, 0.4, 0.5, 0.1, 1.0, 0.04), (0, 0, -1.0, 0.6, 0.5, 0.1, 1.0, 0.04),
				(0, 0, 2.0, 0.0, 0.5, 0.1, -3.0, 0.04)):
			vwppoke(vwpws + 12 * vwptw + 4, vwpup)
			vwpsetbyte(vwpc + 0x436, vwpfire)
			vwpset(vwpc + 0x6fc, (vwpmax, vwpmin, vwpspd))
			vwpset(vwpc + 0x6d8, (vwpang,))
			vwpset(vwpitm + 0xb4, (vwprot,))
			vwpset(vwpsc + 0x28, (vwpdt,))
			check('native !TeslaFX' + fmt((vwpup, vwpfire, vwpspd, vwpang, vwpmax, vwpmin, vwprot, vwpdt)),
				lambda: (vwpcall('!TeslaFX', ('SDVwpCar', 0, 'SDVwpItem')), vwphud(vwpitm)))
		check('native !TeslaFX no item', lambda: vwpcall('!TeslaFX', ('SDVwpCar', 0, 'SDVwpNoItem')))
		vwppoke(vwpws + 12 * vwptw + 4, 0)
		for vwpup, vwpfire, vwprot, vwpdt in ((0, 0, 1.0, 0.04), (0, 0, -0.01, 0.04), (0, 0, 7.0, 0.1),
				(2, 1, 1.0, 0.04), (2, 0, 1.0, 0.04), (0, 1, 1.0, 0.04), (2, 1, 3.0, 0.5)):
			vwppoke(vwpws + 12 * vwpvw + 4, vwpup)
			vwpsetbyte(vwpc + 0x436, vwpfire)
			vwpset(vwpitm + 0xb4, (vwprot,))
			vwpset(vwpsc + 0x28, (vwpdt,))
			check('native !VulcanFX' + fmt((vwpup, vwpfire, vwprot, vwpdt)), lambda: (vwpcall('!VulcanFX',
				('SDVwpCar', 0, 'SDVwpItem')), vwphud(vwpitm)))
		vwppoke(vwpws + 12 * vwpvw + 4, 0)
		vwpsetbyte(vwpc + 0x436, 0)
		vwppoke(vwpsc + 0x28, vwpscdt)
		for vwpk in range(4):
			vwppoke(vwpitm + 0xb4 + 4 * vwpk, vwpsaved[vwpk])
		vwppoke(vwpc + 0x6d8, vwpaims[0])

	# TeslaCreate with the flag of a running shot (no ray effect), the hit target SDVwpCar2
	vwptslots = vwpi(vwpws + 12 * vwptw)
	for vwpsl, vwpflag, vwpp, vwptg in ((3, 1, 0.0, 'SDVwpCar2'), (0xf, 1, 0.0, 'SDVwpCar2'), (1, 1, 1.0, 'SDVwpCar2'),
			(3, 1, 0.0, ''), (3, 1, 0.0, 'SDVwpNoSuch'), (0, 0, 0.0, 'SDVwpCar2'), (3, 1, 0.0, 'SDVwpLife')):
		vwppoke(vwpws + 12 * vwptw, vwpsl)
		vwpsetbyte(vwpc + 0x636, vwpflag)
		vwpset(symva('g_VehicleCenterPadLock'), (vwpp,))
		vwpset(vwpc + 0x6fc, (0.5, 0.1, -1.0))
		vwpset(vwpc + 0x6d8, (0.2,))
		vwpsetvel(vwpc2, (0.0, 0.0, 0.0))
		check('native !TeslaCreate' + fmt((vwpsl, vwpflag, vwpp, vwptg)), lambda tg=vwptg: (vwpcall('!TeslaCreate',
			('SDVwpCar', 60100.0, 50000.0, 50000.0, tg)), vwpaim(vwpc), vwpvel(vwpc2)))
	vwppoke(symva('g_VehicleCenterPadLock'), vwppad)
	vwpsetbyte(vwpc + 0x636, 0)
	vwpsetvel(vwpc2, (0.0, 0.0, 0.0))

	# the shots: Shoot and Fire (the aim into the empty sky far away)
	vwpvslots = vwpi(vwpws + 12 * vwpvw)
	vwptam = vwpfl(vwpam + 4 * vwpta, 1)
	vwpvam = vwpfl(vwpam + 4 * vwpva, 1)
	for vwpa, vwpup in ((0.0, 0), (5.0, 0), (5.0, 1), (5.0, 2)):
		vwpset(vwpam + 4 * vwpva, (vwpa,))
		vwppoke(vwpws + 12 * vwpvw + 4, vwpup)
		vwpset(vwpwm + 0x10, (0.0,))
		check('Vulcan_Shoot' + fmt((vwpa, vwpup)), lambda: (SDTest.Call('Vulcan_Shoot', 'vi', vwpc),
			vwpweap(vwpvw, vwpva)))
	for vwpsl, vwpa, vwpup, vwpua in ((0, 5.0, 0, 0.0), (1, 5.0, 0, 0.0), (3, 20.0, 1, 50.0), (0xf, 20.0, 2, 50.0),
			(1, 0.0, 0, 0.0), (1, 0.25, 0, 0.0)):
		vwppoke(vwpws + 12 * vwpvw, vwpsl)
		vwpset(vwpam + 4 * vwpva, (vwpa,))
		vwppoke(vwpws + 12 * vwpvw + 4, vwpup)
		vwpset(vwpws + 12 * vwpvw + 8, (vwpua,))
		vwpset(vwpwm + 0x10, (0.0,))
		check('native !VulcanFire' + fmt((vwpsl, vwpa, vwpup, vwpua)), lambda: (vwpcall('!VulcanFire',
			('SDVwpCar',)), vwpweap(vwpvw, vwpva), vwpaim(vwpc)))
	for vwpsl, vwpa, vwpup, vwpua in ((0, 5.0, 0, 0.0), (1, 5.0, 0, 0.0), (3, 20.0, 5, 50.0), (0xf, 20.0, 6, 50.0),
			(1, 0.0, 0, 0.0), (3, 0.5, 0, 0.0)):
		vwppoke(vwpws + 12 * vwptw, vwpsl)
		vwpset(vwpam + 4 * vwpta, (vwpa,))
		vwppoke(vwpws + 12 * vwptw + 4, vwpup)
		vwpset(vwpws + 12 * vwptw + 8, (vwpua,))
		vwpset(vwpwm + 0x10, (0.0,))
		check('native !TeslaFire' + fmt((vwpsl, vwpa, vwpup, vwpua)), lambda: (vwpcall('!TeslaFire',
			('SDVwpCar',)), vwpweap(vwptw, vwpta)))
	vwppoke(vwpws + 12 * vwptw + 4, 0)
	vwppoke(vwpws + 12 * vwpvw + 4, 0)

	# VulcanCreate without slots: far away (not seen) and at the camera (the hit particles), rand seeded
	vwppoke(vwpws + 12 * vwpvw, 0)
	vwpcur = vwpi(vwpworld + 0x50)
	vwpcam = vwpfl(vwpworld + 0x254 + 12 * vwpcur, 3)
	vwpnorm = vwpi(symva('g_VulcanHitUseNormal'))
	vwphd = vwpfl(symva('g_FXVulcanHitDistance'), 1)[0]
	for vwpx, vwpy, vwpz_, vwptg, vwpn, vwpseed in ((60100.0, 50000.0, 50000.0, '', 1, 3),
			(60100.0, 50000.0, 50000.0, 'SDVwpCar2', 1, 3), (vwpcam[0], vwpcam[1], vwpcam[2], 'SDVwpCar2', 1, 5),
			(vwpcam[0], vwpcam[1], vwpcam[2], '', 0, 6), (vwpcam[0], vwpcam[1], vwpcam[2], 'SDVwpCar2', 0, 8),
			(vwpcam[0] + 300.0, vwpcam[1], vwpcam[2] + 200.0, 'SDVwpCar2', 1, 7)):
		vwppoke(symva('g_VulcanHitUseNormal'), vwpn)
		vwpsetvel(vwpc2, (0.0, 0.0, 0.0))
		vwpsrand(vwpseed)
		check('native !VulcanCreate' + fmt((vwpx == vwpcam[0], vwptg, vwpn, vwpseed)), lambda x=vwpx, y=vwpy,
			z=vwpz_, tg=vwptg: (SDTest.Call('FX_IsVisible', 'bfffffii', x, y, z, 500.0, vwphd, 0, 0)[0],
			vwpcall('!VulcanCreate', ('SDVwpCar', x, y, z, tg)), vwprand(), vwpvel(vwpc2)))
	vwppoke(symva('g_VulcanHitUseNormal'), vwpnorm)
	vwppoke(vwpws + 12 * vwptw, vwptslots)
	vwppoke(vwpws + 12 * vwpvw, vwpvslots)
	vwpset(vwpam + 4 * vwpta, vwptam)
	vwpset(vwpam + 4 * vwpva, vwpvam)
	vwpsetvel(vwpc2, (0.0, 0.0, 0.0))
	vwppoke(vwpc + 0x71c, vwpaimon)
	if vwpl:
		SDTest.Call('cWithLifeEntity_VarV', 'tbisDDDi', vwpl, 'Vel', 0.0, 0.0, 0.0, 1)
if vwpcar:
	check('SDVwpCar Del', lambda: vwpcar.Del())
if vwpcar2:
	check('SDVwpCar2 Del', lambda: vwpcar2.Del())
if vwplife:
	check('SDVwpLife Del', lambda: vwplife.Del())
vwpfree(vwpsp)
vwpfree(vwps)
