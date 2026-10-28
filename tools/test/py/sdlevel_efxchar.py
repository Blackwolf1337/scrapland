# game Entities/EmbeddedFX/EmbeddedFXChar.c and EmbeddedFXCharHead.c (execfile'd by sdlevel.py, Python 1.5.2): the
# effects embedded in the characters. The constructors (exe) and the destructors on calloc memory; the alarm without a
# model (only the paths that touch no particles); then the character models loaded into a fake entity (calloc memory
# with the vtable of Player0 and the CGameModel at 0xb8, where its GetModel points) and the effects made by
# EmbeddedFXChar_Create on it: their fields, particle parameters, the particles they place, Reset, Stop, Release and
# the Updates that are in C (alarm, bank glows, maintenance). The head effect of the Crazy Gambler on its head model.
# Every node an Init needs is checked first (render node with a particle system), else that class is skipped: a
# missing one is a CHECK. rand is seeded, the frame time of the world is 0.04 and every model visible (R_SplitScreen)
# while the tests run; both are restored. Not called: the Updates still in the exe (Sentinel, Crazy Gambler,
# Mercenary, Messenger, Rusty) and the network handlers (they read a file).
def efci(a):
	return SDTest.Peek(a, '@i')
def efcb(a):
	return SDTest.Peek(a, '@b')
def efcz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def efcfree(p):
	SDTest.Call(0x133550, 'ii', p)
def efcfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def efcset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def efcints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def efcbytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def efcpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def efcadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
# a CGameParticles: attached, the parameters (no padding bytes, no pointers)
def efcparts(p):
	return (efci(p + 0xac) != 0, efcints(p, (0x00, 0x04, 0x18)), efcfl(p + 0x08, 4), efcfl(p + 0x1c, 28),
		efcfl(p + 0x9c, 2), efcbytes(p, (0x98, 0x99, 0xa4)))
# particle i: active, position, color, size
def efcpart(p, i):
	return SDTest.Call(0x21d4e0, 'tbiiFiipF', p, i, (0.0, 0.0, 0.0), 0, 0, 0, (0.0,))
def efcpartlist(p, n):
	return tuple(map(lambda i, p=p: efcpart(p, i), range(n)))
def efcnum(p):
	return SDTest.Call(0x21d470, 'tii', p)[0]
def efctrails(a, n):
	return tuple(map(lambda i, a=a: (efci(a + 4 * i) != 0, SDTest.Call(0x21e1b0, 'tii', a + 4 * i)[0],
		SDTest.Call(0x21e1a0, 'tii', a + 4 * i)[0]), range(n)))
# render node name of the CGameModel gm with a particle system (flags & 2, obj3D of class 0xa5000020)
def efcsist(gm, name):
	i = SDTest.Call(0x20f7d0, 'tiis', gm, name)[0]
	if i < 0:
		return 0
	t = efci(efci(gm) + 0xac)
	if i >= efci(t):
		return 0
	r = efci(t + 4) + 0xa8 * i
	o = efci(r + 0x9c)
	return (efcb(r + 4) & 2) and o != 0 and efci(o + 4) == -0x5affffe0
def efcnode(gm, name):
	return SDTest.Call(0x20f7d0, 'tiis', gm, name)[0] >= 0

efcworld = SDTest.Peek(0x467450, 'i')
efcplayer = SDTest.Call(0x0c1430, 'tiii', efcworld, 0)[0]
efcdelta = efci(efcworld + 0x2d4)
efcset(efcworld + 0x2d4, (0.04,))
efcsplit = SDTest.Peek(0x4a8c8c, 'i')
SDTest.Poke(0x4a8c8c, 1)
SDTest.Call(0x323c50, 'vi', 4711)

# the constructors of the exe and the destructors and deleting destructors (flags 0) in C: vtable, particle nodes
# (offset of Node), trails (offset, count) and the fields the constructors set
efcctors = (
	('BankDirector', 0xd6930, 0xd72d0, 0xc0, (0xc,), (), (4, 8), (0xbc,)),
	('BankMaster', 0xd69a0, 0xd7010, 0x174, (0x10, 0xc4), (), (4, 0xc, 0xc0), (8,)),
	('CrazyGambler', 0xd6a30, 0xd7040, 0x218c, (0xc,), ((0x2168, 8),), (4, 8, 0xc0, 0xc4, 0xc8, 0xcc), (0xbc, 0x2188)),
	('Gear', 0xd6af0, 0xd70a0, 0x18c, (0xc, 0xd4), ((0x188, 1),), (4, 8, 0xc0, 0xd0), ()),
	('Maintenance', 0xd6b90, 0xd70d0, 0xc0, (8,), (), (4, 0xb8, 0xbc), ()),
	('Mercenary', 0xd6c10, 0xd70f0, 0x230, (0x14, 0xc8, 0x180), (), (4, 0xc, 0x10, 0xc4), (8, 0x178)),
	('Messenger', 0xd6cb0, 0xd7120, 0x338, (0x14,), ((0x1f4, 12),), (4, 8), (0xc, 0x334)),
	('Police', 0xd6d40, 0xd7180, 0xd0, (0xc,), (), (4, 8, 0xc0), ()),
	('Rusty', 0xd6df0, 0xd71b0, 0x31c, (0x10, 0xc4, 0x178, 0x23c), ((0x30c, 4),), (4, 0xc, 0xc0, 0x174, 0x228), (8,)),
	('Sentinel', 0xd6ed0, 0xd7230, 0x2bc, (0xc, 0xc0, 0x1f8), ((0x174, 4), (0x188, 4), (0x198, 4)),
		(4, 8, 0xbc, 0x1ec, 0x1f0, 0x1f4, 0x2ac), ()))
efcdeletes = {'BankDirector': 0xd72d0, 'BankMaster': 0xd7310, 'CrazyGambler': 0xd7360, 'Gear': 0xd73e0,
	'Maintenance': 0xd7430, 'Mercenary': 0xd7470, 'Messenger': 0xd74c0, 'Police': 0xd7540, 'Rusty': 0xd7580,
	'Sentinel': 0xd7620}
def efcstate(p, c):
	parts = tuple(map(lambda o, p=p: efci(p + o + 0xa8) != 0, c[4]))
	trails = []
	for o, n in c[5]:
		trails.append(efctrails(p + o, n))
	return (efci(p) - 0x400000, parts, tuple(trails), efcints(p, c[6]), efcbytes(p, c[7]))
for c in efcctors:
	for kind in ('dtor', 'Delete'):
		p = efcz(c[3])
		check('cEmbeddedFX%s_ctor %s' % (c[0], kind), lambda p=p, c=c: (SDTest.Call(c[1], 'tii', p)[0] == p,
			efcstate(p, c)))
		if kind == 'dtor' and c[0] != 'BankDirector':
			check('cEmbeddedFX%s_dtor' % c[0], lambda p=p, c=c: (SDTest.Call(c[2], 'tvi', p), efci(p) - 0x400000,
				tuple(map(lambda o, p=p: efci(p + o + 0xa8), c[4]))))
		else:
			check('cEmbeddedFX%s_Delete(0)' % c[0], lambda p=p, c=c: (SDTest.Call(efcdeletes[c[0]], 'tiii', p,
				0)[0] == p, efci(p) - 0x400000, tuple(map(lambda o, p=p: efci(p + o + 0xa8), c[4]))))
		efcfree(p)
# the alarm member on its own (ctor 0xd6dc0, dtor 0xd71a0)
efca = efcz(0xc8)
def efcalarm(a):
	return (efci(a), efcbytes(a, (0xb4, 0xb5, 0xc4)), efcfl(a + 0xb8, 3))
check('cEmbeddedFXAlarm ctor', lambda: (SDTest.Call(0xd6dc0, 'tii', efca)[0] == efca, efcalarm(efca)))
check('cEmbeddedFXAlarm_Reset', lambda: (SDTest.Call(0xd83e0, 'tbi', efca)[0], efcalarm(efca)))
check('cEmbeddedFXAlarm_SetOn(1)', lambda: (SDTest.Call(0xd87c0, 'tvii', efca, 1), efcalarm(efca)))
check('cEmbeddedFXAlarm_SetOn(0)', lambda: (SDTest.Call(0xd87c0, 'tvii', efca, 0), efcalarm(efca)))
check('cEmbeddedFXAlarm_SetLights(0)', lambda: (SDTest.Call(0xd95b0, 'tvii', efca, 0), efcalarm(efca)))
check('cEmbeddedFXAlarm_SetLights(7)', lambda: (SDTest.Call(0xd95b0, 'tvii', efca, 7), efcalarm(efca)))
SDTest.Call(0xd95b0, 'tvii', efca, 0)
# on without a blink in the step and without the lights: only the times and the level
for on, cur, lvl, t, last, dt in ((0, 0, 0.0, 0.3, 0.3, 0.01), (1, 0, 0.0, 0.3, 0.3, 0.01), (1, 0, 0.9, 0.3, 0.3, 0.01),
		(1, 0, 0.98, 0.4, 0.3, 0.05), (1, 1, 0.5, 0.85, 0.8, 0.0), (0, 1, 0.5, 0.3, 0.3, 0.01), (1, 0, 0.0, 0.89, 0.85, 0.02)):
	efcpoke(efca + 0xb4, on | (cur << 8))
	efcset(efca + 0xb8, (lvl, t, last))
	check('cEmbeddedFXAlarm_Update' + fmt((on, cur, lvl, t, last, dt)), lambda dt=dt: (SDTest.Call(0xdcfc0, 'tvif',
		efca, dt), efcalarm(efca)))
check('cEmbeddedFXAlarm_Init(NULL model) skipped', lambda: 0)
check('cEmbeddedFXAlarm_Release', lambda: (SDTest.Call(0xd7890, 'tvi', efca), efci(efca), efci(efca + 4 + 0xa8)))
check('cEmbeddedFXAlarm_dtor', lambda: (SDTest.Call(0xd71a0, 'tvi', efca), efci(efca + 4 + 0xa8)))
efcfree(efca)

# EmbeddedFXChar_Create with the player and a model without an effect: *fx stays
efcslot = efcz(4)
check('EmbeddedFXChar_Create(Player0, Dtritus.M3D)', lambda: (SDTest.Call(0xd7ac0, 'visi', efcplayer, 'Dtritus.M3D',
	efcslot), efci(efcslot)))
# SetAlarm only when the model of the player is no police or gear (else it writes into its effect)
efcfx224 = efci(efcplayer + 0x224)
efcpname = SDTest.Call(0x20f7a0, 'tsi', efcplayer + 0xb8)[0]
if strop.find(efcpname, 'Police') < 0 and strop.find(efcpname, 'Gear') < 0:
	check('EmbeddedFXChar_SetAlarm(Player0)', lambda: (SDTest.Call(0xd8090, 'vsi', 'Player0', 1),
		efci(efcplayer + 0x224) == efcfx224))
efcfree(efcslot)

# the character models in a fake entity
efcvt = efci(efcplayer)
efcvec = efcz(0x20)
def efcfake(path):
	e = efcz(0x400)
	efcpoke(e, efcvt)
	SDTest.Call(0x20eb10, 'tiis', e + 0xb8, path)
	return e
def efcunfake(e):
	SDTest.Call(0x20ec10, 'tvi', e + 0xb8)
	efcfree(e)
def efcok(gm, sists, nodes):
	if not SDTest.Call(0x210da0, 'tbi', gm)[0] or not efci(gm):
		return 0
	for n in sists:
		if not efcsist(gm, n):
			return 0
	for n in nodes:
		if not efcnode(gm, n):
			return 0
	return 1
efccam = efcfl(efcworld + 0x254 + 12 * efci(efcworld + 0x50), 3)
def efcplace(gm, d, rot):
	efcset(efcvec, efcadd(efccam, d))
	SDTest.Call(0x211d20, 'tvii', gm, efcvec)
	efcset(efcvec, rot)
	SDTest.Call(0x211e50, 'tvii', gm, efcvec)

# name, model file, the name for Create, render nodes with particle systems, other nodes, release, reset, stop
efcchars = (
	('Police', 'Models/Chars/Police/Police.M3D', 'Police.M3D', ('DF_PoliceAlarm',), (), 0xd7880, 0xd83a0, 0),
	('Gear', 'Models/Chars/Gear/Gear.M3D', 'gear.m3d', ('DF_GearShoot', 'DF_PoliceAlarm'), (), 0xd77c0, 0xd8290, 0),
	('BankDirector', 'Models/Chars/BankDirector/BankDirector.M3D', 'BankDirector.M3D', ('DF_Glow',), (), 0xd76f0, 0,
		0),
	('BankMaster', 'Models/Chars/BankMaster/BankMaster.M3D', 'BankMaster.M3D', ('DF_Glow', 'DF_Glow_Baston'), (),
		0xd7700, 0, 0),
	('Maintenance', 'Models/Chars/Maintenance/Maintenance.M3D', 'Maintenance.M3D', ('DF_Light',), (), 0xd77f0, 0, 0),
	('Sentinel', 'Models/Chars/Sentinel/Sentinel.M3D', 'Sentinel.M3D', ('DF_LaserScaner', 'DF_StatusLight',
		'DF_PoliceAlarm'), (), 0xd78e0, 0xd8490, 0xd8760),
	('MercenaryA', 'Models/Chars/MercenaryA/MercenaryA.M3D', 'MercenaryA.M3D', ('DF_Glow', 'DF_Burbujas'), (), 0xd7800,
		0xd82e0, 0),
	('MercenaryL', 'Models/Chars/MercenaryL/MercenaryL.M3D', 'MercenaryL.M3D', ('DF_Glow', 'DF_Burbujas',
		'DF_Burbujas_Pipa'), (), 0xd7800, 0xd82e0, 0),
	('Messenger', 'Models/Chars/Messenger/Messenger.M3D', 'Messenger.M3D', ('DF_GlowLighting',), ('DF_Rayos_01',
		'DF_Rayos_02', 'DF_Rayos_03', 'DF_Rayos_04', 'DF_Rayos_05', 'DF_Rayos_06', 'DF_Rayos_07', 'DF_Rayos_08'),
		0xd7830, 0xd82f0, 0xd86e0),
	('Rusty', 'Models/Chars/Rusty/Rusty.M3D', 'Rusty.M3D', ('DR_Mecanico_ColetaChispas', 'DR_Mecanico_PechoGlowA',
		'DR_Mecanico_PechoGlowB', 'DF_GlowLighting'), ('DF_LightingA', 'DF_LightingB', 'DF_LightingC'), 0xd78a0,
		0xd8420, 0xd8710),
	('CrazyGambler', 'Models/Chars/CrazyGambler/CrazyGambler.M3D', 'CrazyGambler.M3D', ('DF_Glow',),
		('R_Gambler_mano_Izq', 'R_Gambler_Cabeza-Pelos'), 0xd7730, 0xd81d0, 0xd8690))
# the fields of each effect after Init and after the calls
def efcfields(name, fx, e):
	gm = e + 0xb8
	if name == 'Police' or name == 'Gear':
		r = (efcalarm(fx + 8), efcparts(fx + 0xc))
		if name == 'Gear':
			r = r + (efci(fx + 0xd0), efcparts(fx + 0xd4))
		return r
	if name == 'BankDirector':
		return (efci(fx + 8), efcparts(fx + 0xc), efcbytes(fx, (0xbc,)))
	if name == 'BankMaster':
		return (efcbytes(fx, (8,)), efci(fx + 0xc), efcparts(fx + 0x10), efci(fx + 0xc0), efcparts(fx + 0xc4),
			efcpartlist(fx + 0xc4, 16))
	if name == 'Maintenance':
		return (efcparts(fx + 8), efcfl(fx + 0xb8, 2))
	if name == 'Sentinel':
		return (efcints(fx, (8, 0xbc, 0x1a8, 0x1ec)), efcparts(fx + 0xc), efcparts(fx + 0xc0), efcfl(fx + 0x170, 1),
			efcfl(fx + 0x184, 1), efcfl(fx + 0x1ac, 16), efctrails(fx + 0x174, 4), efctrails(fx + 0x188, 4),
			efctrails(fx + 0x198, 4), efcalarm(fx + 0x1f4), efcparts(fx + 0x1f8))
	if name == 'MercenaryA' or name == 'MercenaryL':
		r = (efcbytes(fx, (0x178,)), efcints(fx, (0xc, 0x10, 0xc4, 0x17c)), efcparts(fx + 0x14), efcparts(fx + 0xc8),
			efcpartlist(fx + 0x14, 16))
		if efcb(fx + 0x178):
			r = r + (efcparts(fx + 0x180),)
		return r
	if name == 'Messenger':
		return (efcints(fx, (0x10,)), efcints(fx, range(0xc4, 0x104, 4)), efcparts(fx + 0x14))
	if name == 'Rusty':
		return (efcints(fx, (0xc, 0xc0, 0x174, 0x22c, 0x230, 0x234, 0x238)), efcparts(fx + 0x10), efcparts(fx + 0xc4),
			efcparts(fx + 0x178), efcparts(fx + 0x23c), efcpartlist(fx + 0x10, 8), efcpartlist(fx + 0xc4, 8))
	if name == 'CrazyGambler':
		return (efci(fx + 8) == gm, efcints(fx, (0xc0, 0xc4)), efcparts(fx + 0xc), efcbytes(fx, (0xbc,)))
	return ()
def efcafter(name, fx):
	if name == 'Sentinel':
		return (efcints(fx, (0x1a8, 0x1ec)), efcfl(fx + 0x170, 1), efcfl(fx + 0x184, 1), efcfl(fx + 0x1ac, 16),
			efctrails(fx + 0x174, 4), efctrails(fx + 0x188, 4), efctrails(fx + 0x198, 4), efcalarm(fx + 0x1f4))
	if name == 'Messenger':
		return (efcints(fx, range(0x104, 0x134, 4)), efctrails(fx + 0x1f4, 12), efcints(fx, range(0x2b4, 0x334, 4)))
	if name == 'Rusty':
		return (efcints(fx, range(0x2ec, 0x2fc, 4)), efctrails(fx + 0x30c, 4), efcnum(fx + 0x178))
	if name == 'CrazyGambler':
		return (efcfl(fx + 0xc8, 2), efcfl(fx + 0xd8, 8), efcints(fx, range(0x1ef8, 0x1f58, 4)), efctrails(fx + 0x2168, 8),
			efcnum(fx + 0xc))
	if name == 'MercenaryA' or name == 'MercenaryL':
		return efcints(fx, (0xc,))
	if name == 'Police' or name == 'Gear':
		return efcalarm(fx + 8)
	return ()

# the Updates in C
def efcupdates(name, fx, e):
	gm = e + 0xb8
	if name == 'Police' or name == 'Gear':
		upd = {'Police': 0xdcf90, 'Gear': 0xdaea0}[name]
		a = fx + 8
		for lights in (1, 0):
			SDTest.Call(0xd95b0, 'tvii', a, lights)
			SDTest.Call(0xd87c0, 'tvii', a, 1)
			for dt in (0.05, 0.1, 0.1, 0.3, 0.2, 0.15, 0.1, 0.0, 0.25):
				check('%s_Update on' % name + fmt((lights, dt)), lambda upd=upd, fx=fx, a=a, dt=dt, lights=lights:
					(SDTest.Call(upd, 'tvif', fx, dt), efcalarm(a), efcnum(a + 4), lights and efcpartlist(a + 4, 12)))
			SDTest.Call(0xd87c0, 'tvii', a, 0)
			for dt in (0.1, 0.1, 0.1):
				check('%s_Update off' % name + fmt((lights, dt)), lambda upd=upd, fx=fx, a=a, dt=dt, lights=lights:
					(SDTest.Call(upd, 'tvif', fx, dt), efcalarm(a), efcnum(a + 4), lights and efcpartlist(a + 4, 12)))
	if name == 'BankDirector' or name == 'BankMaster':
		upd = {'BankDirector': 0xd9780, 'BankMaster': 0xd9c00}[name]
		p = {'BankDirector': fx + 0xc, 'BankMaster': fx + 0x10}[name]
		on = {'BankDirector': fx + 0xbc, 'BankMaster': fx + 8}[name]
		for d in ((0.0, 0.0, 300.0), (0.0, 0.0, -300.0), (0.0, 600.0, 300.0), (200.0, -1000.0, 50.0), (0.0, 2000.0, 1.0),
				(0.0, 0.0, 0.0), (-400.0, 30.0, 0.0)):
			for rot in ((0.0, 0.0, 0.0), (0.0, 3.14159, 0.0), (0.3, 1.5, 0.0), (0.0, -2.5, 0.0), (0.0, 7.0, 0.0)):
				efcplace(gm, d, rot)
				check('%s_Update' % name + fmt((d, rot)), lambda upd=upd, fx=fx, p=p, on=on, name=name: (SDTest.Call(upd,
					'tvif', fx, 0.04), efcb(on), efcb(on) and efcpartlist(p, 3),
					name == 'BankMaster' and SDTest.Call(0x21d480, 'tfi', fx + 0xc4)[0]))
	if name == 'Maintenance':
		for dt in (0.1, 0.15, 0.3, 0.7, 0.21, 0.0, 1.0, 0.25):
			check('Maintenance_Update' + fmt((dt,)), lambda fx=fx, dt=dt: (SDTest.Call(0xdaec0, 'tvif', fx, dt),
				efcfl(fx + 0xb8, 2), efcnum(fx + 8), efcparts(fx + 8)))

# the fake entity needs GetModel at this + 0xb8 and IsType(WithLife)
efcfakeok = isfn(efci(efcvt + 0x98), 0xfb380)
check('fake entity GetModel', lambda: efcfakeok)
for c in efcchars:
	if not efcfakeok:
		break
	name = c[0]
	e = efcfake(c[1])
	gm = e + 0xb8
	if not SDTest.Call(efci(efcvt + 0x14) - 0x400000, 'tbis', e, 'WithLife')[0]:
		check('fake entity IsType(WithLife)', lambda: 0)
		efcunfake(e)
		break
	if not efcok(gm, c[3], c[4]):
		check('%s nodes' % name, lambda: 'skipped')
		efcunfake(e)
		continue
	check('%s model' % name, lambda gm=gm: (SDTest.Call(0x210da0, 'tbi', gm)[0], SDTest.Call(0x20f7a0, 'tsi', gm)[0]))
	slot = efcz(4)
	# SetHead runs the Update of the Crazy Gambler (still in the exe): not visible there, it would read the action
	# controller of the fake entity
	if name == 'CrazyGambler':
		SDTest.Poke(0x4a8c8c, 0)
		efcvis = efci(efci(gm) + 0x120)
		efcpoke(efci(gm) + 0x120, efcvis & ~0xff00)
	check('EmbeddedFXChar_Create(%s)' % name, lambda e=e, c=c, slot=slot: (SDTest.Call(0xd7ac0, 'visi', e, c[2], slot),
		efci(slot) != 0, efci(efci(slot)) - 0x400000, efci(efci(slot) + 4) == e))
	fx = efci(slot)
	if name == 'CrazyGambler':
		SDTest.Poke(0x4a8c8c, 1)
		efcpoke(efci(gm) + 0x120, efcvis)
	check('%s fields' % name, lambda name=name, fx=fx, e=e: efcfields(name, fx, e))
	if name == 'Police':
		check('EmbeddedFXChar_Create(Police) again', lambda e=e, c=c, slot=slot: (SDTest.Call(0xd7ac0, 'visi', e,
			c[2], slot), efci(efci(slot)) - 0x400000, efci(efci(slot) + 4) == e))
		fx = efci(slot)
		check('Police fields again', lambda name=name, fx=fx, e=e: efcfields(name, fx, e))
	efcupdates(name, fx, e)
	if c[6]:
		check('%s Reset' % name, lambda c=c, fx=fx, name=name: (SDTest.Call(c[6], 'tbi', fx)[0], efcafter(name, fx)))
	if c[7]:
		check('%s Stop(1)' % name, lambda c=c, fx=fx, name=name: (SDTest.Call(c[7], 'tvii', fx, 1),
			efcafter(name, fx)))
		check('%s Stop(0)' % name, lambda c=c, fx=fx, name=name: (SDTest.Call(c[7], 'tvii', fx, 0),
			efcafter(name, fx)))
	# after Release the particle systems may be gone: reading them is a CHECK (SistPart)
	check('%s Release' % name, lambda c=c, fx=fx: SDTest.Call(c[5], 'tvi', fx))
	n = name
	if n[:9] == 'Mercenary':
		n = 'Mercenary'
	check('%s Delete(1)' % name, lambda n=n, fx=fx: SDTest.Call(efcdeletes[n], 'tiii', fx, 1)[0] == fx)
	efcfree(slot)
	efcunfake(e)

# EmbeddedFXCharHead: the head models
efch = efcz(0xc4)
def efchead(h):
	return (efci(h) - 0x400000, efci(h + 0xb8), efcfl(h + 0xbc, 1), efcbytes(h, (0xc0,)), efcparts(h + 8))
check('cEmbeddedFXHeadCrazyGambler_ctor', lambda: (SDTest.Call(0xdf850, 'tii', efch)[0] == efch, efci(efch + 4),
	efchead(efch)))
check('cEmbeddedFXHeadCrazyGambler_dtor', lambda: (SDTest.Call(0xdf8d0, 'tvi', efch), efci(efch) - 0x400000))
check('cEmbeddedFXHeadCrazyGambler_ctor 2', lambda: SDTest.Call(0xdf850, 'tii', efch)[0] == efch)
check('cEmbeddedFXHeadCrazyGambler_Tick no owner', lambda: (SDTest.Call(0xdf930, 'tvi', efch), efchead(efch)))
check('cEmbeddedFXHeadCrazyGambler_Delete(0)', lambda: (SDTest.Call(0xdf8f0, 'tiii', efch, 0)[0] == efch,
	efci(efch) - 0x400000))
efcfree(efch)
for path, ok in (('Models/Heads/HeadGambler/HeadGambler.M3D', 1), ('Models/Heads/HeadBetty/HeadBetty.M3D', 0)):
	gm = efcz(0x1c)
	SDTest.Call(0x20eb10, 'tiis', gm, path)
	check('head model' + fmt((path,)), lambda gm=gm: (SDTest.Call(0x210da0, 'tbi', gm)[0],
		SDTest.Call(0x20f7a0, 'tsi', gm)[0]))
	if ok and not efcok(gm, ('DF_GlowLighting',), ('R_Gambler_Cabeza-Pelos',)):
		check('head nodes', lambda: 'skipped')
		ok = -1
	if ok >= 0:
		h = SDTest.Call(0xdfab0, 'ii', gm)[0]
		check('EmbeddedFXCharHead_Create' + fmt((path,)), lambda h=h: h != 0)
		if h:
			check('head fields', lambda h=h, gm=gm: (efci(h + 4) == gm, efchead(h)))
			for i in range(4):
				check('cEmbeddedFXHeadCrazyGambler_Tick %d' % i, lambda h=h: (SDTest.Call(0xdf930, 'tvi', h),
					efcfl(h + 0xbc, 1), efcpartlist(h + 8, 24)))
			efcset(efcworld + 0x2d4, (0.01,))
			for i in range(3):
				check('cEmbeddedFXHeadCrazyGambler_Tick short %d' % i, lambda h=h: (SDTest.Call(0xdf930, 'tvi', h),
					efcfl(h + 0xbc, 1)))
			efcset(efcworld + 0x2d4, (0.04,))
			check('cEmbeddedFXHeadCrazyGambler_Clear', lambda h=h: (SDTest.Call(0xdfaa0, 'tvi', h), efcnum(h + 8)))
			check('cEmbeddedFXHeadCrazyGambler_Release', lambda h=h: (SDTest.Call(0xdfa90, 'tvi', h), efchead(h)))
			check('cEmbeddedFXHeadCrazyGambler_Delete(1)', lambda h=h: SDTest.Call(0xdf8f0, 'tiii', h, 1)[0] == h)
	SDTest.Call(0x20ec10, 'tvi', gm)
	efcfree(gm)

efcfree(efcvec)
check('rand after', lambda: SDTest.Call(0x323c2f, 'i')[0])
SDTest.Poke(0x4a8c8c, efcsplit)
efcpoke(efcworld + 0x2d4, efcdelta)
