# game Entities/MissileEntity.c (the flight, the swarm, the network state, the model and the effects),
# Entities/Vehicle/Controls.c (the item natives and the target list of the ships) and the vehicle effects and motors
# of the big leftovers 0x0f0000..0x110000 (execfile'd by sdlevel.py, Python 1.5.2). The missiles are made by the
# SwarmCreate native far above the level (the model by SetUp), the cars by Scrap.CreateEntity far away; the script
# functions OnHit and OnBeeDeath of the missiles are Python functions of this file that record their arguments.
# No frame runs during the script; the world time is not printed (only compared through the lists).
def gbgi(a):
	return SDTest.Peek(a, '@i')
def gbgb(a):
	return SDTest.Peek(a, '@b')
def gbgs(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def gbgz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def gbgfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def gbgfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def gbgset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def gbgpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def gbgstr(a, s):
	SDTest.Call('Cad_Copy', 'iisi', a, s, len(s) + 1)
def gbgcstr(s):
	a = gbgz(len(s) + 16)
	gbgstr(a, s)
	return a
def gbgsetbyte(a, v):
	w = gbgi(a & ~3)
	sh = 8 * (a & 3)
	gbgpoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def gbgints(a, n):
	r = ()
	for k in range(n):
		r = r + (gbgi(a + 4 * k),)
	return r
def gbgsrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def gbgfind(name):
	slot = SDTest.Call('HashTable_Slot', 'tiis', gbgworld + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', gbgworld + 4, name, slot)[0]
	if node:
		return gbgi(node)
	return 0
def gbgnative(name, params):
	ok = Scrap.SetCallFunc('!' + name)
	for p in params:
		if type(p) == type(''):
			Scrap.AddParams(p)
		elif type(p) == type(0):
			Scrap.AddParami(p)
		else:
			Scrap.AddParamf(p)
	Scrap.Execute()
	return ok
def gbgistype(e, t):
	return SDTest.Call(gbgi(gbgi(e) + 0x14) - 0x400000, 'tbis', e, t)[0]
# the debris system of that name or 0 (cWorld_GetDebrisSys would make an empty one, whose NewElement hangs)
def gbgdebris(name):
	if not name:
		return 0
	slot = SDTest.Call('HashTable_Slot', 'tiis', gbgworld + 0x298, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', gbgworld + 0x298, name, slot)[0]
	if node:
		return gbgi(node)
	return 0

gbgworld = SDTest.Peek('g_pWorld', 'i')
gbgtime = SDTest.Call('Timer_GetTime', 'tfi', gbgworld + 0x2c8)[0]

# the script functions of the missiles
gbghits = []
def gbgonhit(*a):
	gbghits.append(a)
gbgbees = []
def gbgonbee(*a):
	gbgbees.append(a)
def gbgcalls():
	r = (tuple(gbghits), tuple(gbgbees))
	del gbghits[:]
	del gbgbees[:]
	return r

# ---- the missiles ----
gbgmfields = ((0x28, 'f3'), (0xac, 'f3'), (0xd4, 'f6'), (0x238, 'i'), (0x3e8, 'b'), (0x3ec, 'f4'), (0x3fc, 's'),
	(0x47c, 's'), (0x4fc, 's'), (0x57c, 'i'), (0x580, 'f1'), (0x584, 'b'), (0x585, 'b'), (0x588, 'i'), (0x58c, 'b'),
	(0x594, 'f1'), (0x598, 'f6'), (0x5b0, 'i'), (0x7b4, 'i'), (0x7b8, 'i'), (0x7bc, 'b'), (0x7bd, 'b'), (0x4c, 'b'))
def gbgm(m):
	r = ()
	for o, k in gbgmfields:
		if k == 'i':
			r = r + (gbgi(m + o),)
		elif k == 'b':
			r = r + (gbgb(m + o),)
		elif k == 's':
			r = r + (gbgs(m + o),)
		else:
			r = r + (gbgfl(m + o, strop.atoi(k[1:])),)
	return r
def gbghitnames():
	return map(lambda i: gbgs(symva('g_MissileHits') + 0x80 * i), range(4))
def gbgmhits(m):
	return map(lambda i, m=m: gbgs(m + 0x5b4 + 0x80 * i), range(4))
# the bees: position, angles, velocity, angle speed, speed
def gbgbeelist(m):
	r = []
	d = gbgi(m + 0x590)
	while d and len(r) < 20:
		r.append((d, gbgfl(d + 0x1c, 13)))
		d = gbgi(d + 0x50)
	return r
def gbgbeestate(m):
	return map(lambda x: x[1], gbgbeelist(m))
def gbgvars(m, n, s):
	a = gbgcstr(s)
	p = gbgz(4)
	gbgpoke(p, a)
	r = SDTest.Call('cMissileEntity_VarS', 'tbisii', m, n, p, 1)[0]
	gbgfree(p)
	return r

gbgsaved = gbgz(0x200)
SDTest.Call('crt_memcpy', 'iiii', gbgsaved, symva('g_MissileHits'), 0x200)
gbgdrop = gbgfl(symva('g_DebrisDrop'), 12)
gbgatm = gbgfl(symva('g_AtmosphereLimit'), 1)[0]
check('GBG atmosphere', lambda: gbgatm)
gbgmpos = (pp[0] + 3000.0, pp[1] + 100000.0, pp[2] + 3000.0)
gbgswfile = gbgs(SDTest.Peek('g_SwarmMissileFile', 'i'))
check('GBG swarm file', lambda: (gbgswfile, gbgdebris(gbgswfile) != 0))
check('GBG SwarmCreate', lambda: gbgnative('SwarmCreate', ('SDGbgSw', gbgmpos[0], gbgmpos[1], gbgmpos[2], 0.0, 0.0,
	1.0)))
gbgsw = gbgfind('SDGbgSw')
check('GBG missile', lambda: (gbgsw != 0, gbgsw and gbgistype(gbgsw, 'Missile')))
if gbgsw and gbgistype(gbgsw, 'Missile'):
	m = gbgsw
	check('GBG SwarmSetUp (SetModel)', lambda m=m: (gbgnative('SwarmSetUp', ('SDGbgSw',)), gbgm(m),
		gbgfl(m + 0xf8, 3), gbgi(m + 0xc0) != 0))
	check('GBG OnHit', lambda m=m: gbgvars(m, 'OnHit', '__main__.gbgonhit'))
	check('GBG OnBeeDeath', lambda m=m: gbgvars(m, 'OnBeeDeath', '__main__.gbgonbee'))
	# SetModel: the empty name does nothing, a second load of the same model
	check('cMissileEntity_SetModel empty', lambda m=m: (SDTest.Call('cMissileEntity_SetModel', 'tvizfffi', m, None, 1.0, 1.0, 1.0, 0),
		SDTest.Call('cMissileEntity_SetModel', 'tvisfffi', m, '', 2.0, 2.0, 2.0, 0), gbgm(m)))
	gbgset(m + 0xac, (0.25, -0.5, 0.0))
	gbgset(m + 0x28, gbgmpos)
	check('cMissileEntity_SetModel', lambda m=m: (SDTest.Call('cMissileEntity_SetModel', 'tvisfffi', m, gbgswfile, 1.0, 1.0, 1.0, 0),
		gbgm(m), gbgfl(m + 0xf8, 3), gbgb(m + 0x3e8)))
	# the bees
	gbgsys = gbgdebris(gbgswfile)
	if gbgsys:
		gbgset(symva('g_DebrisDrop'), (gbgmpos[0], gbgmpos[1], gbgmpos[2], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 2.0))
		check('cMissileEntity_AddToSwarm(3)', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, 3), gbgm(m),
			gbgbeestate(m)))
		gbgpoke(m + 0x57c, 2)
		gbgsrand(4242)
		check('cMissileEntity_AddToSwarm(2) cloud', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, 2), gbgm(m),
			gbgbeestate(m)))
		gbgpoke(m + 0x57c, 0)
		check('cMissileEntity_AddToSwarm(0)', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, 0), gbgm(m)))
		check('cMissileEntity_AddToSwarm(-1)', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, -1), gbgm(m)))
		gbgsetbyte(m + 0x58c, 1)
		check('cMissileEntity_AddToSwarm hidden', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, 2), gbgm(m)))
		gbgsetbyte(m + 0x58c, 0)
		gbgpoke(m + 0x588, 5)
		# Swarm: the render position of the missile is where the bees follow
		gbgset(m + 0xa0, (gbgmpos[0] + 10.0, gbgmpos[1] - 20.0, gbgmpos[2] + 30.0))
		gbgset(m + 0x94, (0.1, 0.2, 0.3))
		gbgset(m + 0x598, (0.0, 0.0, 0.0))
		gbgset(m + 0x5a4, (150.0,))
		gbgset(m + 0x580, (0.0,))
		gbgsrand(31)
		for dt, typ in ((0.1, 0), (0.02, 0), (0.2, 2), (0.07, 1)):
			gbgpoke(m + 0x57c, typ)
			check('cMissileEntity_Swarm' + fmt((dt, typ)), lambda m=m, dt=dt: (SDTest.Call('cMissileEntity_Swarm', 'tvif', m, dt),
				gbgm(m), gbgbeestate(m)))
		# a bee far outside the radius
		gbgbee = gbgi(m + 0x590)
		if gbgbee:
			gbgset(gbgbee + 0x1c, (gbgmpos[0] + 5000.0, gbgmpos[1], gbgmpos[2]))
			gbgset(m + 0x580, (0.2,))
			check('cMissileEntity_Swarm far bee', lambda m=m: (SDTest.Call('cMissileEntity_Swarm', 'tvif', m, 0.05), gbgm(m),
				gbgbeestate(m)))
		gbgpoke(m + 0x57c, 0)
		# Explode without flag584: the bees die where they are (OnBeeDeath), the slow ones go back to the system
		gbgsetbyte(m + 0x584, 0)
		gbgsetbyte(m + 0x585, 0)
		gbgbl = gbgbeelist(m)
		if len(gbgbl) > 1:
			gbgset(gbgbl[1][0] + 0x34, (0.0, 0.0, 0.0))
		gbgset(m + 0xd4, (0.0, 0.0, 0.0))
		check('cMissileEntity_Explode die', lambda m=m, bl=gbgbl: (SDTest.Call('cMissileEntity_Explode', 'tviF', m,
			(gbgmpos[0], gbgmpos[1] + 500.0, gbgmpos[2])), gbgm(m), map(lambda x: gbgfl(x[0] + 0x1c, 13), bl),
			gbgcalls()))
		# Explode with flag584: the bees fly to the point
		check('cMissileEntity_AddToSwarm(4) again', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, 4), gbgm(m)))
		gbgsetbyte(m + 0x584, 1)
		gbgbl = gbgbeelist(m)
		k = 0
		for d, st in gbgbl:
			gbgset(d + 0x34, (100.0 * k + 50.0, -30.0, 7.0 * k))
			gbgset(d + 0x28, (0.1 * k, -0.2 * k, 0.3))
			k = k + 1
		for gbgat in ((gbgmpos[0] + 2000.0, gbgmpos[1], gbgmpos[2] - 1000.0),):
			check('cMissileEntity_Explode fly' + fmt(gbgat), lambda m=m, bl=gbgbl, at=gbgat: (SDTest.Call('cMissileEntity_Explode',
				'tviF', m, at), gbgm(m), map(lambda x: gbgfl(x[0] + 0x1c, 13), bl), gbgcalls()))
		# the bees close to the point: the time is at least 0.01
		check('cMissileEntity_AddToSwarm(2) close', lambda m=m: (SDTest.Call('cMissileEntity_AddToSwarm', 'tvii', m, 2), gbgm(m)))
		gbgbl = gbgbeelist(m)
		for d, st in gbgbl:
			gbgset(d + 0x34, (1e6, 0.0, 0.0))
		check('cMissileEntity_Explode close', lambda m=m, bl=gbgbl: (SDTest.Call('cMissileEntity_Explode', 'tviF', m,
			gbgfl(bl[0][0] + 0x1c, 3)), gbgm(m), map(lambda x: gbgfl(x[0] + 0x1c, 13), bl), gbgcalls()))
		gbgsetbyte(m + 0x584, 0)
	# no names without a debris system: cWorld_GetDebrisSys loads it and the original hangs on a missing file
	gbgpoke(m + 0x588, 0)

	# the network state through a memory file and a controller of our own
	gbgnc = gbgz(0x60)
	SDTest.Call('cNetController_ctor', 'tii', gbgnc)
	gbgf = gbgz(0x20)
	gbgfbuf = gbgz(0x200)
	SDTest.Call('cFileMem_ctor', 'tiiiii', gbgf, gbgfbuf, 0x200, 0)
	gbgnet = gbgi(m + 0xf4)
	gbgpoke(m + 0xf4, gbgnc)
	def gbgfbytes():
		n = SDTest.Call('cFileMem_Size', 'tii', gbgf)[0]
		return map(lambda i: gbgb(gbgfbuf + i), range(n))
	def gbgwrite(m, dist2, full):
		SDTest.Call('cFileMem_Open', 'tiisi', gbgf, '', 6)
		SDTest.Call('cMissileEntity_NetWrite', 'tvifii', m, dist2, gbgf, full)
		return gbgfbytes()
	def gbgread(m, isnew):
		SDTest.Call('cFileMem_SetBuffer', 'tviiiii', gbgf, gbgfbuf, 0x200, 0, 0)
		SDTest.Call('cFileMem_Open', 'tiisi', gbgf, '', 1)
		SDTest.Call('cMissileEntity_NetRead', 'tviii', m, gbgf, isnew)
		return (gbgm(m), gbgfl(m + 0x290, 2), gbgfl(gbgnc + 4, 13), gbgb(gbgnc + 0x38))
	gbgstr(m + 0x3fc, 'SDGbgParent')
	gbgstr(m + 0x47c, 'SDGbgTarget')
	for act, dist2, full, fire in ((1, 1e6, 1, (0.5, 3.0)), (1, 3e7, 0, (0.005, 3.0)), (1, 10.0, 1, (0.2, 0.0)),
			(0, 1e6, 1, (0.5, 3.0)), (1, 1e6, 1, (2.555, 1.0))):
		gbgsetbyte(m + 0x4c, act)
		gbgset(m + 0x290, fire)
		gbgpoke(m + 0x588, 70)
		gbgpoke(m + 0x7b8, 2)
		check('cMissileEntity_NetWrite' + fmt((act, dist2, full, fire)), lambda m=m, d=dist2, f=full: gbgwrite(m, d,
			f))
		check('cMissileEntity_NetRead' + fmt((act, dist2, full, fire)), lambda m=m: gbgread(m, 1))
		check('cMissileEntity_NetRead old' + fmt((act, dist2, full, fire)), lambda m=m: gbgread(m, 0))
	# the ids of the network names (0xffff for none) read back
	for b in ((0xff, 0xff, 0x85, 0x0a), (0x03, 0x00, 0x3f, 0x00), (0x05, 0x01, 0xc0, 0xff)):
		SDTest.Call('cFileMem_Open', 'tiisi', gbgf, '', 6)
		SDTest.Call('cMissileEntity_NetWrite', 'tvifii', m, 1e6, gbgf, 1)
		n = SDTest.Call('cFileMem_Size', 'tii', gbgf)[0]
		k = 0
		for x in b:
			gbgsetbyte(gbgfbuf + n - 4 + k, x)
			k = k + 1
		check('cMissileEntity_NetRead ids' + fmt(b), lambda m=m: gbgread(m, 0))
	gbgsetbyte(gbgnc + 0x38, 0x80)
	gbgsetbyte(m + 0x4c, 0)
	check('cMissileEntity_NetWrite inactive', lambda m=m: gbgwrite(m, 1e6, 1))
	gbgpoke(m + 0xf4, gbgnet)
	gbgsetbyte(m + 0x4c, 1)
	gbgpoke(m + 0x588, 0)
	gbgpoke(m + 0x5b0, 0)
	gbgstr(m + 0x3fc, '')
	gbgstr(m + 0x47c, '')

	# SetEmbeddedFX: a deleted missile does nothing, unknown names, the effects whose debris systems are there
	gbgdead = gbgi(m + 0x20)
	gbgpoke(m + 0x20, 1)
	check('cMissileEntity_SetEmbeddedFX deleted', lambda m=m: (SDTest.Call('cMissileEntity_SetEmbeddedFX', 'tvis', m, 'EMI'), gbgm(m)))
	gbgpoke(m + 0x20, gbgdead)
	for n in ('Bogus', '', 'D', 'Dumb', 'G', 'GearMissil', 'C', 'Cloudy', 'E', 'EMIx'):
		check('cMissileEntity_SetEmbeddedFX(%s)' % n, lambda m=m, n=n: (SDTest.Call('cMissileEntity_SetEmbeddedFX', 'tvis', m, n), gbgm(m)))
	for n, g in (('DumbMotor', sym('g_MissileFXDumbMotor')), ('GearMissile', sym('g_MissileFXGearMissile')), ('Cloud', sym('g_MissileFXCloud')), ('EMI', sym('g_MissileFXEMI'))):
		gbgfxsys = gbgdebris(gbgs(SDTest.Peek(g, 'i')))
		check('GBG fx system ' + n, lambda g=g, s=gbgfxsys: (gbgs(SDTest.Peek(g, 'i')), s != 0))
		if gbgfxsys:
			gbgset(m + 0x3f0, (2.0,))
			gbgsrand(55)
			check('cMissileEntity_SetEmbeddedFX(%s)' % n, lambda m=m, n=n: (SDTest.Call('cMissileEntity_SetEmbeddedFX', 'tvis', m, n),
				gbgm(m)))

	# Tick (vf23): far above, no target, Linear; the radius grows to the top
	def gbgtick(m, dt):
		SDTest.Call('cMissileEntity_Tick', 'tvif', m, dt)
		return (gbgm(m), gbgfl(m + 0x5a4, 3), gbgcalls(), gbgmhits(m))
	gbgset(m + 0x28, gbgmpos)
	gbgset(m + 0xac, (0.0, 0.5, 0.0))
	gbgset(m + 0xd4, (100.0, 200.0, -300.0, 0.01, 0.02, 0.03))
	gbgset(m + 0x5a4, (100.0, 500.0, 2000.0))
	gbgset(m + 0x3ec, (0.0, 10.0, 1000.0, 0.5))
	gbgpoke(m + 0x7b4, 1)
	gbgpoke(m + 0x588, 0)
	gbgsetbyte(m + 0x7bd, 0)
	gbgsetbyte(m + 0x7bc, 0)
	gbgsrand(808)
	for dt, typ in ((0.1, 0), (0.05, 3), (0.1, 2), (0.1, 1), (0.3, 0)):
		gbgpoke(m + 0x57c, typ)
		check('cMissileEntity_Tick' + fmt((dt, typ)), lambda m=m, dt=dt: gbgtick(m, dt))
	gbgpoke(m + 0x57c, 0)
	# fall out of the atmosphere
	gbgsetbyte(m + 0x7bd, 1)
	check('cMissileEntity_Tick fallout', lambda m=m: gbgtick(m, 0.1))
	gbgset(m + 0x28, (gbgmpos[0], gbgatm - 1000.0, gbgmpos[2]))
	gbgsetbyte(m + 0x7bd, 1)
	check('cMissileEntity_Tick fallout low', lambda m=m: gbgtick(m, 0.1))
	gbgsetbyte(m + 0x7bd, 0)
	gbgpoke(m + 0x57c, 0)
	gbgset(m + 0x28, gbgmpos)
	# the bees die by SwDthTime
	gbgpoke(m + 0x588, 4)
	gbgset(m + 0x3ec, (1.0, 2.0))
	gbgsetbyte(m + 0x58c, 1)
	check('cMissileEntity_Tick bee death', lambda m=m: gbgtick(m, 0.1))
	gbgsetbyte(m + 0x58c, 0)
	gbgpoke(m + 0x588, 0)
	gbgset(m + 0x3ec, (0.0, 10.0))
	gbgpoke(m + 0x7b4, 0)

	# the cars: the target, the enemies of the target list
	gbgcar = None
	gbgcarobj = Scrap.CreateEntity('SDGbgCar', gbgmpos[0] + 1500.0, gbgmpos[1], gbgmpos[2], 'Car')
	gbgcar = gbgfind('SDGbgCar')
	check('SDGbgCar', lambda: (gbgcarobj is not None, gbgcar != 0))
	if gbgcar:
		gbgpoke(gbgcar + 0x238, 100)
		gbgstr(m + 0x47c, 'SDGbgCar')
		gbgset(m + 0x3f8, (1.5,))
		for dt in (0.1, 0.5, 2.0):
			check('cMissileEntity_Tick target' + fmt(dt), lambda m=m, dt=dt: gbgtick(m, dt))
		gbgpoke(gbgcar + 0x238, 0)
		check('cMissileEntity_Tick dead target', lambda m=m: gbgtick(m, 0.1))
		gbgstr(m + 0x47c, 'SDGbgNobody')
		check('cMissileEntity_Tick gone target', lambda m=m: gbgtick(m, 0.1))
		gbgpoke(gbgcar + 0x238, 100)
		# InOut through the car: in, still in, out
		gbgcarpos = gbgfl(gbgcar + 0x28, 3)
		gbgpoke(m + 0x7b4, 2)
		gbgset(m + 0xd4, (0.0, 0.0, 0.0))
		gbgset(m + 0x5a4, (400.0, 500.0, 0.0))
		for p in ((gbgcarpos[0] + 100.0, gbgcarpos[1], gbgcarpos[2]), (gbgcarpos[0] + 50.0, gbgcarpos[1], gbgcarpos[2]),
				(gbgcarpos[0] + 5000.0, gbgcarpos[1], gbgcarpos[2])):
			gbgset(m + 0x28, p)
			check('cMissileEntity_Tick inout' + fmt(p), lambda m=m: (gbgtick(m, 0.1), gbghitnames()))
		# Inside
		gbgpoke(m + 0x7b4, 3)
		gbgset(m + 0x28, (gbgcarpos[0] + 100.0, gbgcarpos[1], gbgcarpos[2]))
		check('cMissileEntity_Tick inside', lambda m=m: gbgtick(m, 0.1))
		gbgset(m + 0x28, (gbgcarpos[0] + 5000.0, gbgcarpos[1], gbgcarpos[2]))
		check('cMissileEntity_Tick inside none', lambda m=m: gbgtick(m, 0.1))
		# Hit: the car is hit, the missile explodes and goes off
		gbgpoke(m + 0x7b4, 1)
		gbgstr(m + 0x3fc, 'SDGbgParent')
		gbgset(m + 0x28, (gbgcarpos[0] + 100.0, gbgcarpos[1], gbgcarpos[2]))
		check('cMissileEntity_Tick hit', lambda m=m: gbgtick(m, 0.1))
	# the world: down through the ground at the player
	gbgsetbyte(m + 0x4c, 1)
	gbgpoke(m + 0x238, 1)
	gbgpoke(m + 0x7b4, 0)
	gbgstr(m + 0x47c, '')
	gbgset(m + 0x28, (pp[0] + 50.0, pp[1] + 300.0, pp[2] + 50.0))
	gbgset(m + 0xd4, (0.0, -10000.0, 0.0))
	gbgset(m + 0x5a4, (50.0, 500.0, 0.0))
	check('cMissileEntity_Tick ground', lambda m=m: gbgtick(m, 0.1))
	gbgsetbyte(m + 0x4c, 1)
	gbgset(m + 0x28, (pp[0] + 50.0, pp[1] + 300.0, pp[2] + 50.0))
	gbgsetbyte(m + 0x7bc, 1)
	check('cMissileEntity_Tick ground noworldhit', lambda m=m: gbgtick(m, 0.1))
	gbgsetbyte(m + 0x7bc, 0)
	gbgset(m + 0x28, gbgmpos)
	gbgset(m + 0xd4, (0.0, 0.0, 0.0))
	check('cWithLifeEntity_Vf48', lambda m=m: (SDTest.Call('cWithLifeEntity_Vf48', 'tvi', m), gbgm(m)))
	check('GBG SDGbgSw Del', lambda: Scrap.GetEntity('SDGbgSw').Del())
SDTest.Call('crt_memcpy', 'iiii', symva('g_MissileHits'), gbgsaved, 0x200)
gbgfree(gbgsaved)
gbgset(symva('g_DebrisDrop'), gbgdrop)
check('GBG missile hits restored', lambda: gbghitnames())

# ---- the items a ship takes ----
gbgcar = gbgfind('SDGbgCar')
if not gbgcar:
	Scrap.CreateEntity('SDGbgCar', gbgmpos[0] + 1500.0, gbgmpos[1], gbgmpos[2], 'Car')
	gbgcar = gbgfind('SDGbgCar')
gbgitem = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', gbgworld, 'SDGbgItem', gbgmpos[0], gbgmpos[1] - 2000.0, gbgmpos[2],
	'item')[0]
check('GBG item and car', lambda: (gbgitem != 0, gbgcar != 0))
if gbgcar and gbgitem:
	c = gbgcar
	def gbgitemstate():
		return (gbgfl(gbgitem + 0x228, 1), gbgb(gbgitem + 0x4c))
	gbgwm = c + 0x5f0
	gbgweaps = gbgi(gbgwm + 0x18)
	gbgammo = gbgi(gbgwm + 0x20)
	gbgnw = gbgi(gbgworld + 0x1bb8)
	gbgna = gbgi(gbgworld + 0x1bc4)
	gbgsavedw = map(lambda i: gbgi(gbgweaps + 0xc * i), range(gbgnw))
	gbgsaveda = gbgfl(gbgammo, gbgna)
	def gbgammos():
		return (map(lambda i: gbgi(gbgweaps + 0xc * i), range(gbgnw)), gbgfl(gbgammo, gbgna))
	check('GBG car weapons', lambda: (gbgnw, gbgna, gbgammos(), map(lambda i: gbgfl(gbgi(gbgworld + 0x1bc8) +
		0x5c * i + 4, 1), range(gbgna))))
	for nat in ('TakeEner', 'TakeMiss', 'TakePart'):
		check('Controls_%sNative no weapon' % nat, lambda nat=nat: (gbgnative(nat, ('SDGbgItem', 'SDGbgCar')),
			gbgammos(), gbgitemstate()))
	check('Controls_TakeEnerNative no car', lambda: (gbgnative('TakeEner', ('SDGbgItem', 'SDGbgNobody')), gbgammos()))
	check('Controls_TakeEnerNative item', lambda: (gbgnative('TakeEner', ('SDGbgItem', 'SDGbgItem')), gbgammos()))
	for nat, w, ammo in (('TakeEner', 2, 2), ('TakeEner', 4, 2), ('TakeMiss', 1, 1), ('TakeMiss', 5, 1),
			('TakePart', 0, 0), ('TakePart', 3, 0)):
		if w < gbgnw and ammo < gbgna:
			gbgpoke(gbgweaps + 0xc * w, 1)
			gbgset(gbgammo + 4 * ammo, (1.0,))
			check('Controls_%sNative weapon %d' % (nat, w), lambda nat=nat: (gbgnative(nat, ('SDGbgItem', 'SDGbgCar')),
				gbgammos(), gbgitemstate()))
			gbgmax = gbgfl(gbgi(gbgworld + 0x1bc8) + 0x5c * ammo + 4, 1)[0]
			gbgset(gbgammo + 4 * ammo, (gbgmax,))
			check('Controls_%sNative full %d' % (nat, w), lambda nat=nat: (gbgnative(nat, ('SDGbgItem', 'SDGbgCar')),
				gbgammos()))
			gbgpoke(gbgweaps + 0xc * w, gbgsavedw[w])
	for i in range(gbgnw):
		gbgpoke(gbgweaps + 0xc * i, gbgsavedw[i])
	gbgset(gbgammo, gbgsaveda)
	gbgmaxlife = gbgi(c + 0x640)
	gbglife = gbgi(c + 0x238)
	for life, maxl, god in ((10, 100, 0), (90, 100, 0), (100, 100, 0), (150, 100, 0), (10, 100, 1), (99, 100, 1)):
		gbgpoke(c + 0x238, life)
		gbgpoke(c + 0x640, maxl)
		gbgsetbyte(c + 0x23c, god)
		check('Controls_TakeLifeNative' + fmt((life, maxl, god)), lambda c=c: (gbgnative('TakeLife', ('SDGbgItem',
			'SDGbgCar')), gbgi(c + 0x238), gbgitemstate()))
	gbgsetbyte(c + 0x23c, 0)
	gbgpoke(c + 0x238, gbglife)
	gbgpoke(c + 0x640, gbgmaxlife)

# ---- the target list ----
gbgc2obj = Scrap.CreateEntity('SDGbgCar2', gbgmpos[0] + 1500.0, gbgmpos[1], gbgmpos[2] + 800.0, 'Car')
gbgc3obj = Scrap.CreateEntity('SDGbgCar3', gbgmpos[0] + 1500.0, gbgmpos[1], gbgmpos[2] + 1600.0, 'Car')
gbgc2 = gbgfind('SDGbgCar2')
gbgc3 = gbgfind('SDGbgCar3')
check('GBG cars 2 3', lambda: (gbgc2 != 0, gbgc3 != 0))
if gbgcar and gbgc2 and gbgc3:
	o = gbgcar + 0x648
	def gbgtl(o):
		l = gbgi(o + 4)
		r = (gbgi(o) == gbgcar, l != 0, gbgs(o + 8))
		if l:
			# a list made by Rebuild also has the enemies of the level (their count is not printed); a new list
			# has no names
			r = r + (min(gbgi(l), 3), gbgfl(l + 4, 1), gbgi(l + 0x80c))
			if gbgi(l) > 0:
				r = r + (gbgs(l + 8),)
		return r
	gbgteams = (gbgi(gbgcar + 0x388), gbgi(gbgc2 + 0x388), gbgi(gbgc3 + 0x388))
	check('GBG teams', lambda: gbgteams)
	gbgpoke(gbgcar + 0x388, 7)
	gbgpoke(gbgc2 + 0x388, 4)
	gbgpoke(gbgc3 + 0x388, 5)
	gbgpoke(gbgcar + 0x238, 100)
	gbgpoke(gbgc2 + 0x238, 100)
	gbgpoke(gbgc3 + 0x238, 100)
	check('cVehicleO648 state', lambda o=o: gbgtl(o))
	check('cVehicleO648_IsStale no list', lambda o=o: SDTest.Call('cVehicleO648_IsStale', 'tbi', o)[0])
	check('cVehicleO648_Rebuild no list', lambda o=o: (SDTest.Call('cVehicleO648_Rebuild', 'tvi', o), gbgtl(o)))
	check('cVehicleO648_Update no list', lambda o=o: (SDTest.Call('cVehicleO648_Update', 'tvif', o, 0.1), gbgtl(o)))
	check('cVehicleO648_Cycle no list', lambda o=o: (SDTest.Call('cVehicleO648_Cycle', 'tvii', o, 1), gbgtl(o)))
	check('cVehicleO648_PickInView no list', lambda o=o: (SDTest.Call('cVehicleO648_PickInView', 'tvi', o), gbgtl(o)))
	check('cVehicleO648_SetSlot(-1) no list', lambda o=o: (SDTest.Call('cVehicleO648_SetSlot', 'tvii', o, -1), gbgtl(o)))
	check('cVehicleO648_SetSlot(0)', lambda o=o: (SDTest.Call('cVehicleO648_SetSlot', 'tvii', o, 0), gbgtl(o),
		gbgfl(gbgi(o + 4) + 0x808, 1)[0] == -gbgfl(symva('g_TgtRegenerationTime'), 1)[0]))
	check('cVehicleO648_SetSlot(1) again', lambda o=o: (SDTest.Call('cVehicleO648_SetSlot', 'tvii', o, 1), gbgtl(o)))
	gbgemi = gbgfl(gbgcar + 0x58c, 1)
	for emi in (0.0, 5.0):
		gbgset(gbgcar + 0x58c, (emi,))
		check('cVehicleO648_IsBlocked' + fmt(emi), lambda o=o: SDTest.Call('cVehicleO648_IsBlocked', 'tbi', o)[0])
	gbgset(gbgcar + 0x58c, (5.0,))
	gbgpadlock = gbgfl(symva('g_VehicleCenterPadLock'), 1)
	gbgset(symva('g_VehicleCenterPadLock'), (1.0,))
	check('cVehicleO648_IsBlocked padlock', lambda o=o: SDTest.Call('cVehicleO648_IsBlocked', 'tbi', o)[0])
	gbgset(symva('g_VehicleCenterPadLock'), gbgpadlock)
	gbgstr(o + 8, 'SDGbgCar3')
	check('cVehicleO648_Cycle blocked', lambda o=o: (SDTest.Call('cVehicleO648_Cycle', 'tvii', o, 1), gbgtl(o)))
	gbgstr(o + 8, 'SDGbgCar3')
	check('cVehicleO648_Update blocked', lambda o=o: (SDTest.Call('cVehicleO648_Update', 'tvif', o, 0.1), gbgtl(o)))
	gbgstr(o + 8, 'SDGbgCar3')
	check('cVehicleO648_PickInView blocked', lambda o=o: (SDTest.Call('cVehicleO648_PickInView', 'tvi', o), gbgtl(o)))
	gbgset(gbgcar + 0x58c, (0.0,))
	# the attacker time: long ago, then now (without attacker)
	gbghit = gbgfl(gbgcar + 0x308, 1)
	l = gbgi(o + 4)
	for hit in (-1000.0, gbgtime + 100.0):
		gbgset(gbgcar + 0x308, (hit,))
		check('cVehicleO648_Rebuild' + fmt(hit - gbgtime > 0), lambda o=o: (SDTest.Call('cVehicleO648_Rebuild', 'tvi', o), gbgi(l) > 1,
			gbgs(l + 8), gbgs(o + 8), gbgi(l + 0x80c)))
	gbgset(gbgcar + 0x308, gbghit)
	# a list of our own: the cars 2 and 3
	def gbgmylist(o, l, cur, target):
		gbgpoke(l, 2)
		gbgpoke(l + 0x80c, cur)
		gbgstr(l + 8, 'SDGbgCar2')
		gbgstr(l + 0x88, 'SDGbgCar3')
		gbgset(l + 0x808, (gbgtime,))
		gbgstr(o + 8, target)
	for nxt, cur, target in ((1, 0, 'SDGbgCar2'), (1, 1, 'SDGbgCar3'), (0, 0, 'SDGbgCar2'), (0, 1, 'SDGbgCar3'),
			(1, 0, 'SDGbgCar3'), (0, 1, 'SDGbgCar2')):
		gbgmylist(o, l, cur, target)
		check('cVehicleO648_IsStale' + fmt((cur, target)), lambda o=o: SDTest.Call('cVehicleO648_IsStale', 'tbi', o)[0])
		check('cVehicleO648_Cycle' + fmt((nxt, cur, target)), lambda o=o, n=nxt: (SDTest.Call('cVehicleO648_Cycle', 'tvii', o, n),
			gbgtl(o)))
	gbgmylist(o, l, 0, 'SDGbgNobody')
	check('cVehicleO648_IsStale missing', lambda o=o: SDTest.Call('cVehicleO648_IsStale', 'tbi', o)[0])
	gbgmylist(o, l, 0, 'SDGbgCar3')
	gbgsetbyte(gbgc3 + 0x4c, 0)
	check('cVehicleO648_IsStale inactive', lambda o=o: SDTest.Call('cVehicleO648_IsStale', 'tbi', o)[0])
	gbgsetbyte(gbgc3 + 0x4c, 1)
	gbgmylist(o, l, 0, 'SDGbgCar3')
	gbgpoke(l, 1)
	check('cVehicleO648_IsStale one', lambda o=o: SDTest.Call('cVehicleO648_IsStale', 'tbi', o)[0])
	gbgmylist(o, l, 0, 'SDGbgCar3')
	gbgset(l + 0x808, (gbgtime - 100.0,))
	check('cVehicleO648_IsStale old', lambda o=o: SDTest.Call('cVehicleO648_IsStale', 'tbi', o)[0])
	# Update: the target stays, is gone, the wait
	for target, wait, team in (('SDGbgCar3', 1.0, 5), ('SDGbgCar3', 1.0, 3), ('SDGbgNobody', 1.0, 5),
			('SDGbgCar', 1.0, 5), ('', 0.0, 5)):
		gbgmylist(o, l, 0, target)
		gbgset(l + 4, (wait,))
		gbgpoke(gbgc3 + 0x388, team)
		check('cVehicleO648_Update' + fmt((target, wait, team)), lambda o=o: (SDTest.Call('cVehicleO648_Update', 'tvif', o, 0.25),
			gbgtl(o)))
	gbgpoke(gbgc3 + 0x388, 5)
	# PickInView: the car 2 straight ahead
	gbgcpos = gbgfl(gbgcar + 0x28, 3)
	gbgcang = gbgfl(gbgcar + 0xac, 3)
	for p in ((gbgcpos[0], gbgcpos[1], gbgcpos[2] + 800.0), (gbgcpos[0] + 800.0, gbgcpos[1], gbgcpos[2]),
			(gbgcpos[0], gbgcpos[1], gbgcpos[2] - 800.0)):
		SDTest.Call(gbgi(gbgi(gbgc2) + 0x1c) - 0x400000, 'tviF', gbgc2, p)
		gbgstr(o + 8, 'SDGbgCar3')
		check('cVehicleO648_PickInView' + fmt(p), lambda o=o: (SDTest.Call('cVehicleO648_PickInView', 'tvi', o), gbgtl(o)))
	check('cVehicleO648_SetSlot(-1)', lambda o=o: (SDTest.Call('cVehicleO648_SetSlot', 'tvii', o, -1), gbgtl(o)))
	gbgpoke(gbgcar + 0x388, gbgteams[0])
	gbgpoke(gbgc2 + 0x388, gbgteams[1])
	gbgpoke(gbgc3 + 0x388, gbgteams[2])
	gbgstr(o + 8, '')
	# the constructor and destructor on memory of our own
	gbgo = gbgz(0x88)
	gbgpoke(gbgo, 0x1234)
	gbgpoke(gbgo + 4, 0x5678)
	gbgpoke(gbgo + 8, 0x41414141)
	check('cVehicleO648_ctor', lambda: (SDTest.Call('cVehicleO648_ctor', 'tii', gbgo)[0] == gbgo, gbgints(gbgo, 3)))
	check('cVehicleO648_SetSlot(2) own', lambda: (SDTest.Call('cVehicleO648_SetSlot', 'tvii', gbgo, 2), gbgi(gbgo + 4) != 0))
	check('cVehicleO648_dtor', lambda: (SDTest.Call('cVehicleO648_dtor', 'tvi', gbgo), gbgi(gbgo + 4) != 0))
	gbgfree(gbgo)
for n in ('SDGbgCar', 'SDGbgCar2', 'SDGbgCar3', 'SDGbgItem'):
	if gbgfind(n):
		check('GBG %s Del' % n, lambda n=n: Scrap.GetEntity(n).Del())

# ---- the vehicle effects, motors, flight and model (the ship SDGbgCar4 with the model SPoli1) ----
def gbgnew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def gbgdelete(p):
	SDTest.Call('operator_delete', 'vi', p)
def gbgpoison(a, n):
	for k in range(n):
		gbgpoke(a + 4 * k, 0x5a5a5a5a)
# the parameters of a CGameParticles without its padding and the node
def gbgpart(p):
	return (gbgints(p, 0x98 / 4), gbgb(p + 0x98), gbgb(p + 0x99), gbgints(p + 0x9c, 2), gbgb(p + 0xa4))
gbgve = gbgnew(0x24)
gbgpoison(gbgve, 9)
check('cVehicleEffects_ctor', lambda: (SDTest.Call('cVehicleEffects_ctor', 'tii', gbgve)[0] == gbgve, gbgints(gbgve, 9)))
gbgtu = gbgnew(0x17c)
gbgpoison(gbgtu, 0x17c / 4)
check('cVehicleTubes_ctor', lambda: (SDTest.Call('cVehicleTubes_ctor', 'tii', gbgtu)[0] == gbgtu, gbgi(gbgtu),
	gbgints(gbgtu + 0x1c + 0xa8, 2), gbgints(gbgtu + 0x1c + 0xb0 + 0xa8, 2), gbgi(gbgtu + 4)))
gbgtr = gbgnew(0x60)
gbgpoison(gbgtr, 0x60 / 4)
check('cVehicleTrails_ctor', lambda: (SDTest.Call('cVehicleTrails_ctor', 'tii', gbgtr)[0] == gbgtr, gbgi(gbgtr), gbgi(gbgtr + 4)))
gbgpoke(gbgtu + 4, 0)
gbgpoke(gbgtr + 4, 0)
gbgpoke(gbgve + 0x1c, gbgtr)
gbgpoke(gbgve + 0x20, gbgtu)
check('cVehicleEffects_Update counts 0', lambda: (SDTest.Call('cVehicleEffects_Update', 'tvif', gbgve, 0.1), gbgints(gbgve, 9) ==
	(0, 0, 0, 0, 0, 0, 0, gbgtr, gbgtu)))
check('cVehicleEffects_dtor', lambda: (SDTest.Call('cVehicleEffects_dtor', 'tvi', gbgve), gbgints(gbgve, 9)))
check('cVehicleEffects_dtor empty', lambda: (SDTest.Call('cVehicleEffects_dtor', 'tvi', gbgve), gbgints(gbgve, 9)))
check('cVehicleEffects_Update none', lambda: SDTest.Call('cVehicleEffects_Update', 'tvif', gbgve, 0.1))
gbgdelete(gbgve)

# the motors on memory of our own
gbgms = gbgnew(0xc8)
gbgpoison(gbgms, 0xc8 / 4)
check('cVehicleMotors_ctor', lambda: (SDTest.Call('cVehicleMotors_ctor', 'tii', gbgms)[0] == gbgms, gbgi(gbgms) - 0x400000,
	gbgints(gbgms + 4, 7), gbgi(gbgms + 0x20), gbgi(gbgms + 0x30)))
gbgm1 = gbgnew(0x11c)
gbgpoison(gbgm1, 0x11c / 4)
check('cVehicleMotor_ctor', lambda: (SDTest.Call('cVehicleMotor_ctor', 'tii', gbgm1)[0] == gbgm1, gbgints(gbgm1 + 0x38, 2),
	gbgb(gbgm1 + 0x40), gbgb(gbgm1 + 0x60), gbgb(gbgm1 + 0x6c), gbgints(gbgm1 + 0x64, 4), gbgi(gbgm1 + 0x74)))
gbgm2 = gbgnew(0x11c)
SDTest.Call('cVehicleMotor_ctor', 'tii', gbgm2)
check('cVehicleMotor_dtor', lambda: (SDTest.Call('cVehicleMotor_dtor', 'tvi', gbgm2), gbgints(gbgm2 + 0x38, 2), gbgi(gbgm2 + 0x74)))
gbgdelete(gbgm2)
gbgpoke(gbgms + 0xc, gbgm1)
check('cVehicleMotors_dtor', lambda: (SDTest.Call('cVehicleMotors_dtor', 'tvi', gbgms), gbgi(gbgms) - 0x400000,
	gbgi(gbgms + 0xc) == gbgm1, gbgi(gbgms + 0x20)))
gbgdelete(gbgms)

gbgc4obj = Scrap.CreateEntity('SDGbgCar4', gbgmpos[0], gbgmpos[1] - 3000.0, gbgmpos[2] + 5000.0, 'Car')
gbgc4 = gbgfind('SDGbgCar4')
check('SDGbgCar4', lambda: (gbgc4obj is not None, gbgc4 != 0))
if gbgc4:
	c = gbgc4
	check('GBG SetModel SPoli1', lambda c=c: (SDTest.Call('cVehicleEntity_SetModel', 'tvisfffi', c, 'SPoli1', 1.0, 1.0, 1.0, 1),
		gbgi(c + 0x51c), gbgi(c + 0x224) != 0, gbgi(c + 0x80c) != 0, gbgi(c + 0x810) != 0))
	# the wind smoke at the ship
	gbgwind = gbgs(SDTest.Peek('g_WindFxSmokeFile', 'i'))
	gbgwsys = gbgdebris(gbgwind)
	check('GBG wind smoke', lambda: (gbgwind, gbgwsys != 0, gbgfl(symva('g_WindFXSmokeMaxDist'), 1), gbgfl(symva('g_WindFXSmokeSizeDiv'), 1)))
	gbgwel = 0
	if gbgwsys:
		gbgcpos = SDTest.Call(gbgi(gbgi(c) + 0x74) - 0x400000, 'tiiF', c, (0.0, 0.0, 0.0))[1]
		gbgsrand(1357)
		for d in ((0.0, -300.0, 0.0), (100.0, -200.0, 50.0), (0.0, 0.0, 0.0), (1e6, 0.0, 0.0)):
			gbgwd = SDTest.Call('cDebrisSys_GetIdle', 'tii', gbgwsys)[0]
			gbgwel = gbgwd and gbgi(gbgwd + 0x54)
			# the original reads the element without a check
			if gbgwel:
				check('cVehicleEffects_WindSmoke' + fmt(d), lambda c=c, d=d, el=gbgwel: (SDTest.Call('cVehicleEffects_WindSmoke',
					'tvifff', c + 0x7f0, gbgcpos[0] + d[0], gbgcpos[1] + d[1], gbgcpos[2] + d[2]),
					gbgpart(el + 4), SDTest.Call('crt_rand', 'i')[0]))
	# the escape tubes on a tubes object of our own: the particles are copies of the particles of a wind smoke element
	# (attached to its own system)
	gbgsp = gbgwel and gbgwel + 4
	# MaxSpeed of our car is 3500 or 5600 from run to run (in the original too), the tubes scale with it: fixed here
	gbgset(c + 0x498, (3500.0,))
	check('GBG tube particles', lambda: (gbgsp != 0, gbgsp and gbgi(gbgsp + 0xac) != 0, gbgfl(c + 0x498, 1)))
	if gbgsp and gbgi(gbgsp + 0xac) and gbgi(c + 0x224):
		gbgtu = gbgnew(0x17c)
		SDTest.Call('cVehicleTubes_ctor', 'tii', gbgtu)
		SDTest.Call('crt_memcpy', 'iiii', gbgtu + 0x1c, gbgsp, 0xb0)
		SDTest.Call('crt_memcpy', 'iiii', gbgtu + 0x1c + 0xb0, gbgsp, 0xb0)
		gbgpoke(gbgtu, c)
		gbgpoke(gbgtu + 4, 2)
		gbgpoke(gbgtu + 0x14, 0)
		gbgpoke(gbgtu + 0x18, -1)
		# the model counts as seen in the last frame
		gbgmodelo = gbgi(c + 0xb8)
		gbgvis = gbgb(gbgmodelo + 0x121)
		gbgsetbyte(gbgmodelo + 0x121, 1)
		gbgsrand(2468)
		for kind, dt, lvz, nodes in ((0, 0.01, 0.0, (0, -1)), (0, 0.03, -3000.0, (0, -1)), (1, 0.03, 500.0, (-1, 0)),
				(1, 0.05, -100000.0, (0, 0)), (2, 0.05, 0.0, (0, 0)), (0, 0.02, -1000.0, (-1, -1))):
			gbgpoke(gbgtu + 0xc, kind)
			gbgpoke(gbgtu + 0x14, nodes[0])
			gbgpoke(gbgtu + 0x18, nodes[1])
			gbgset(c + 0x4e4, (0.0, 0.0, lvz))
			check('cVehicleTubes_Update' + fmt((kind, dt, lvz, nodes)), lambda dt=dt: (SDTest.Call('cVehicleTubes_Update', 'tvif',
				gbgtu, dt), gbgfl(gbgtu + 0x10, 1), gbgpart(gbgtu + 0x1c), gbgpart(gbgtu + 0x1c + 0xb0),
				SDTest.Call('crt_rand', 'i')[0]))
		gbgsetbyte(c + 0x4c, 0)
		check('cVehicleTubes_Update inactive', lambda: (SDTest.Call('cVehicleTubes_Update', 'tvif', gbgtu, 0.5), gbgfl(gbgtu + 0x10, 1)))
		gbgsetbyte(c + 0x4c, 1)
		gbgpoke(gbgtu + 4, 1)
		gbgpoke(gbgtu + 0x14, 0)
		gbgpoke(gbgtu + 0x18, -1)
		gbgve = gbgnew(0x24)
		SDTest.Call('cVehicleEffects_ctor', 'tii', gbgve)
		gbgpoke(gbgve + 0x20, gbgtu)
		check('cVehicleEffects_Update tubes', lambda: (SDTest.Call('cVehicleEffects_Update', 'tvif', gbgve, 0.04), gbgfl(gbgtu + 0x10, 1),
			gbgpart(gbgtu + 0x1c)))
		gbgdelete(gbgve)
		gbgsetbyte(gbgmodelo + 0x121, gbgvis)
		gbgset(c + 0x4e4, (0.0, 0.0, 0.0))
		# the copies are not released
		gbgdelete(gbgtu)
	# the shoot effects on m_FollowSistPart2 of the embedded FX (a copy of the wind smoke particles when it has
	# none), the weapon nodes of slot 0 set to node 0, the model counted as seen
	gbgfx = gbgi(c + 0x224)
	gbgnodes = gbgi(c + 0x5f0 + 0x1c)
	check('GBG shoot setup', lambda: (gbgfx != 0, gbgnodes != 0, gbgfx and gbgi(gbgfx + 0x184) != 0, gbgsp != 0))
	if gbgfx and gbgnodes and (gbgi(gbgfx + 0x184) or (gbgsp and gbgi(gbgsp + 0xac))):
		gbgfxsaved = gbgz(0xb0)
		SDTest.Call('crt_memcpy', 'iiii', gbgfxsaved, gbgfx + 0xd8, 0xb0)
		if not gbgi(gbgfx + 0x184):
			SDTest.Call('crt_memcpy', 'iiii', gbgfx + 0xd8, gbgsp, 0xb0)
		gbgmodelo = gbgi(c + 0xb8)
		gbgvis = gbgb(gbgmodelo + 0x121)
		gbgsetbyte(gbgmodelo + 0x121, 1)
		gbgweap = map(lambda a: gbgi(a), (symva('g_LaserIDWeap'), symva('g_VulcanIDWeap'), symva('g_DevastatorIDWeap'), symva('g_SwarmIDWeap'), symva('g_ATPCIDWeap'), symva('g_InfernoIDWeap'), symva('g_TeslaIDWeap')))
		gbgammoid = map(lambda a: gbgi(a), (symva('g_VulcanIDAmmo'), symva('g_DevastatorIDAmmo'), symva('g_SwarmIDAmmo'), symva('g_ATPCIDAmmo'), symva('g_InfernoIDAmmo')))
		gbgnsaved = map(lambda k: gbgi(gbgnodes + 0x10 * k), gbgammoid)
		for k in gbgammoid:
			gbgpoke(gbgnodes + 0x10 * k, 0)
		gbgset(gbgfx + 0x198, (10.0, 20.0, -30.0))
		check('GBG shoot weapons', lambda: (gbgweap, gbgammoid))
		gbgsrand(9753)
		for w in gbgweap + [12345]:
			check('cVehicleEffects_Shoot' + fmt(w), lambda c=c, w=w: (SDTest.Call('cVehicleEffects_Shoot', 'tviii', c + 0x7f0, w, 0),
				gbgpart(gbgfx + 0xd8), SDTest.Call('crt_rand', 'i')[0]))
		gbgsetbyte(gbgmodelo + 0x121, 0)
		check('cVehicleEffects_Shoot unseen', lambda c=c: (SDTest.Call('cVehicleEffects_Shoot', 'tviii', c + 0x7f0, gbgweap[1], 0),
			SDTest.Call('crt_rand', 'i')[0]))
		gbgfol = gbgb(gbgfx + 0x189)
		gbgsetbyte(gbgfx + 0x189, 1)
		check('cVehicleEffects_Shoot followed', lambda c=c: (SDTest.Call('cVehicleEffects_Shoot', 'tviii', c + 0x7f0, gbgweap[1], 0),
			gbgpart(gbgfx + 0xd8), SDTest.Call('crt_rand', 'i')[0]))
		gbgsetbyte(gbgfx + 0x189, gbgfol)
		k = 0
		for a in gbgammoid:
			gbgpoke(gbgnodes + 0x10 * a, gbgnsaved[k])
			k = k + 1
		gbgsetbyte(gbgmodelo + 0x121, gbgvis)
		SDTest.Call('crt_memcpy', 'iiii', gbgfx + 0xd8, gbgfxsaved, 0xb0)
		gbgfree(gbgfxsaved)
	# the flight
	gbgstepf = ((0x28, 3), (0xac, 3), (0xd4, 3), (0xe0, 3), (0x45c, 5), (0x46c, 2), (0x490, 1), (0x590, 1))
	def gbgstep(c):
		r = ()
		for o, n in gbgstepf:
			r = r + (gbgfl(c + o, n),)
		return r
	gbgstepsaved = gbgfl(c + 0x418, 0x4a0 / 4 - 0x418 / 4)
	gbgposlow = (gbgmpos[0], 20000.0, gbgmpos[2] + 5000.0)
	gbgset(c + 0x230, (900.0,))
	gbgset(c + 0x480, (1.5, 9000.0, 4000.0, 3.0, 0.0, 2.0, 12000.0, 0.8))
	for st in (((0.3, -0.2, 1.0, 0.1, -0.05), 0, 0, 100.0, 0.0), ((1.5, -2.0, 1.0, 0.4, 0.2), 0, 0, 5000.0, 0.0),
			((-1.2, 1.7, -1.0, -0.3, 0.6), 0, 0, -20.0, 0.0), ((0.0, 0.0, -1.0, 0.0, 0.0), 0, 1, 3000.0, 0.0),
			((0.5, 0.5, 1.0, 0.0, 0.0), 1, 0, 8500.0, 0.0), ((0.5, 0.5, 1.0, 0.0, 0.0), 1, 0, 8500.0, 15500.0),
			((0.5, 0.5, 0.5, 0.2, 0.1), 1, 0, 8000.0, 0.0), ((0.0, 0.0, 0.0, 0.0, 0.0), 0, 0, 0.0, 30000.0)):
		sticks, boost, full, thrust, speed = st
		gbgset(c + 0x28, gbgposlow)
		gbgset(c + 0xac, (0.2, -1.0, 0.1))
		gbgset(c + 0xd4, (100.0, -50.0, 300.0))
		gbgset(c + 0x418, sticks)
		gbgsetbyte(c + 0x438, boost)
		gbgsetbyte(c + 0x439, full)
		gbgset(c + 0x45c, (thrust,))
		gbgset(c + 0x46c, (speed,))
		gbgset(c + 0x490, (0.0,))
		check('cVehicleEntity_Step' + fmt(st), lambda c=c: (SDTest.Call('cVehicleEntity_Step', 'tvif', c, 0.05), gbgstep(c)))
	gbgsetbyte(c + 0x438, 0)
	gbgsetbyte(c + 0x439, 0)
	# the push, above the atmosphere, the free space magnet
	gbgset(c + 0x470, (0.5, 100.0, 200.0, -300.0))
	gbgset(c + 0x28, gbgposlow)
	check('cVehicleEntity_Step push', lambda c=c: (SDTest.Call('cVehicleEntity_Step', 'tvif', c, 0.1), gbgstep(c),
		gbgfl(c + 0x470, 1)))
	gbgspace = gbgfl(symva('g_TopLimit'), 1)[0]
	for y in (gbgatm + 100.0, (gbgatm + gbgspace) * 0.5, gbgspace + 10.0):
		gbgset(c + 0x28, (gbgmpos[0], y, gbgmpos[2]))
		gbgset(c + 0xac, (0.3, 0.0, 0.0))
		check('cVehicleEntity_Step high' + fmt(y), lambda c=c: (SDTest.Call('cVehicleEntity_Step', 'tvif', c, 0.1), gbgstep(c)))
	gbgmag = (gbgi(symva('g_FreeSpaceMagnet')), gbgfl(symva('g_FreeSpaceMagnetCenterX'), 1), gbgfl(symva('g_FreeSpaceMagnetCenterY'), 1), gbgfl(symva('g_FreeSpaceMagnetCenterZ'), 1), gbgfl(symva('g_FreeSpaceMagnetLoRadius'), 1),
		gbgfl(symva('g_FreeSpaceMagnetHiRadius'), 1), gbgfl(symva('g_FreeSpaceMagnetAccel'), 1))
	check('GBG magnet', lambda: gbgmag)
	gbgpoke(symva('g_FreeSpaceMagnet'), 1)
	gbgset(symva('g_FreeSpaceMagnetCenterX'), (0.0,))
	gbgset(symva('g_FreeSpaceMagnetCenterY'), (0.0,))
	gbgset(symva('g_FreeSpaceMagnetCenterZ'), (0.0,))
	gbgset(symva('g_FreeSpaceMagnetLoRadius'), (1000.0,))
	gbgset(symva('g_FreeSpaceMagnetHiRadius'), (60000.0,))
	gbgset(symva('g_FreeSpaceMagnetAccel'), (500.0,))
	for p in (gbgposlow, (100.0, 200.0, 300.0), (gbgmpos[0], 30000.0, 90000.0)):
		gbgset(c + 0x28, p)
		check('cVehicleEntity_Step magnet' + fmt(p), lambda c=c: (SDTest.Call('cVehicleEntity_Step', 'tvif', c, 0.1), gbgstep(c)))
	gbgpoke(symva('g_FreeSpaceMagnet'), gbgmag[0])
	gbgset(symva('g_FreeSpaceMagnetCenterX'), gbgmag[1])
	gbgset(symva('g_FreeSpaceMagnetCenterY'), gbgmag[2])
	gbgset(symva('g_FreeSpaceMagnetCenterZ'), gbgmag[3])
	gbgset(symva('g_FreeSpaceMagnetLoRadius'), gbgmag[4])
	gbgset(symva('g_FreeSpaceMagnetHiRadius'), gbgmag[5])
	gbgset(symva('g_FreeSpaceMagnetAccel'), gbgmag[6])
	gbgset(c + 0x418, gbgstepsaved)

	# the model of a slot
	def gbgmodel(c):
		return (SDTest.Call('CGameModel_GetPos', 'tiiF', c + 0xb8, (0.0, 0.0, 0.0))[1],
			SDTest.Call('CGameModel_GetRot', 'tiiF', c + 0xb8, (0.0, 0.0, 0.0))[1])
	gbgblur = map(lambda a: gbgfl(a, 1)[0], (symva('g_MotionBlurFactorBegin'), symva('g_MotionBlurFactorEnd'), symva('g_MotionBlurOffVar'), symva('g_MotionBlurTime'), symva('g_MotionBlurEndTime')))
	gbgtimer = gbgfl(c + 0x118, 2)
	gbgofs = gbgfl(c + 0x3e8, 1)
	gbgset(c + 0x3e8, (0.75,))
	gbgset(c + 0x4c4, (9000.0, 0.3))
	gbgset(c + 0x4f0, (2000.0, 0.0, -4000.0))
	gbgset(c + 0x7e8, (1.25,))
	gbgcam = gbgz(0x240)
	gbgset(gbgcam + 0x220, (0.06,))
	gbgsrand(5150)
	for t, y, fixed, cam, slot, dying in ((10.0, 20000.0, 0, 0, -1, 0), (10.0, 20000.0, 1, 0, -1, 0),
			(12.5, gbgatm + 5000.0, 0, 0, -1, 0), (12.5, gbgspace + 5000.0, 1, 0, -1, 0), (3.0, 20000.0, 0, 1, 0, 0),
			(3.25, 20000.0, 0, 1, 1, 0), (7.0, gbgatm + 100.0, 0, 1, 2, 0), (4.0, 20000.0, 0, 0, -1, 1)):
		gbgset(c + 0x118, (t, 0.02))
		gbgset(c + 0x28, (gbgmpos[0], y, gbgmpos[2]))
		gbgset(c + 0x94, (0.1, -0.2, 0.05, gbgmpos[0], y, gbgmpos[2]))
		gbgsetbyte(c + 0x638, fixed)
		gbgsetbyte(c + 0x1ac, dying)
		gbgset(c + 0x1a4, (1.0, 0.5))
		if cam:
			gbgpoke(c + 0xf0, gbgcam)
		else:
			gbgpoke(c + 0xf0, 0)
		gbgpoke(c + 0x410, slot)
		check('cVehicleEntity_UpdateModel' + fmt((t, y, fixed, cam, slot, dying)), lambda c=c: (
			SDTest.Call('cVehicleEntity_UpdateModel', 'tvi', c), gbgmodel(c), gbgfl(gbgcam + 0xa8, 1), gbgfl(gbgcam + 0x214, 3),
			map(lambda a: gbgfl(a, 1)[0], (symva('g_MotionBlurFactorBegin'), symva('g_MotionBlurFactorEnd'), symva('g_MotionBlurOffVar'), symva('g_MotionBlurTime'), symva('g_MotionBlurEndTime')))))
	gbgpoke(c + 0xf0, 0)
	gbgpoke(c + 0x410, -1)
	gbgsetbyte(c + 0x638, 0)
	gbgsetbyte(c + 0x1ac, 0)
	gbgfree(gbgcam)
	k = 0
	for a in (symva('g_MotionBlurFactorBegin'), symva('g_MotionBlurFactorEnd'), symva('g_MotionBlurOffVar'), symva('g_MotionBlurTime'), symva('g_MotionBlurEndTime')):
		gbgset(a, (gbgblur[k],))
		k = k + 1
	gbgset(c + 0x118, gbgtimer)
	gbgset(c + 0x3e8, gbgofs)
	# the controls of a player ship from the actions of player 0 (poked and restored)
	gbgsets = gbgi(symva('g_ActionSets') + 0x4)
	gbgset1 = gbgsets and gbgi(symva('g_ActionSets')) > 1 and gbgsets + 0xc
	gbgacts = gbgset1 and gbgi(gbgset1 + 8)
	gbgnact = gbgset1 and gbgi(gbgset1 + 4)
	check('GBG action set', lambda: (gbgacts != 0, gbgnact))
	if gbgacts and gbgnact >= 32:
		gbgctlp = c + 0x414
		def gbgctl(p):
			return (gbgi(p), gbgfl(p + 4, 7), map(lambda o, p=p: gbgb(p + o), range(0x20, 0x29)), gbgfl(p + 0x2c, 6))
		def gbgactset(vals):
			for i in range(32):
				gbgset(gbgacts + 0x7c * i + 8, (0.0,))
				gbgset(gbgacts + 0x7c * i + 0x18, (0.0,))
			for i, v in vals:
				gbgset(gbgacts + 0x7c * i + 0x18, (v,))
		gbgactsaved = map(lambda i: (gbgfl(gbgacts + 0x7c * i + 8, 1), gbgfl(gbgacts + 0x7c * i + 0x18, 1)), range(32))
		gbgctlsaved = gbgfl(gbgctlp, 0x44 / 4)
		gbgcamd = (gbgfl(symva('g_VehicleCamDist'), 1), gbgfl(symva('g_VehicleCamDist2'), 1))
		gbgpad = gbgfl(symva('g_Pads') + 0xc, 4)
		gbgcurset = gbgi(symva('g_CurActionSet'))
		gbglock = gbgfl(symva('g_VehicleCenterPadLock'), 1)
		# NextWeap and PrevWeap loop when the iNext or iPrev chain of the weapon does not come back
		gbgnw = gbgi(gbgworld + 0x1bb8)
		gbgweapt = gbgi(gbgworld + 0x1bbc)
		def gbgchain(w, off):
			seen = {}
			i = w
			while 1:
				if i < 0 or i >= gbgnw or seen.has_key(i):
					return 0
				seen[i] = 1
				i = gbgi(gbgweapt + 0xb0 * i + off)
				if i == w:
					return 1
		gbglaser = gbgi(symva('g_LaserIDWeap'))
		gbgchains = (gbgchain(gbglaser, 0x90), gbgchain(gbglaser, 0x94))
		check('GBG weapon chains', lambda: (gbglaser, gbgchains))
		gbgpoke(c + 0x410, 0)
		gbgset(gbgctlp + 4, (0.0,) * 16)
		gbgpoke(gbgctlp, gbglaser)
		gbgstr(c + 0x650, 'Player0')
		for vals, lock in (((), 0.0), (((8, 1.0), (7, 0.3), (5, 0.7), (4, 0.2), (2, 0.4), (3, 0.1), (10, 0.6),
				(11, 0.1), (13, 0.5), (12, 0.2), (9, 1.0), (16, 1.0)), 0.0), (((6, 1.0), (8, 0.2), (16, 0.0)), 0.0),
				(((18, 1.0), (4, 0.25), (5, 0.5), (2, 0.75), (3, 0.125), (6, 0.5), (7, 0.25)), 0.0),
				(((22, 1.0), (2, 0.3), (5, 0.1)), 0.5), (((22, 1.0), (8, 0.0)), 0.5), (((2, 0.0),), 0.5),
				(((8, 0.6),), 0.5), (((14, 1.0),), 0.0), (((15, 1.0),), 0.0), (((24, 1.0), (25, 1.0)), 0.0), (((31, 1.0),), 0.0),
				(((31, 1.0),), 0.0), (((31, 1.0),), 0.0), (((31, 1.0),), 0.0), (((19, 1.0), (20, 1.0), (21, 1.0)), 0.0),
				(((17, 1.0), (16, 1.0)), 0.0)):
			if (vals == ((14, 1.0),) and not gbgchains[0]) or (vals == ((15, 1.0),) and not gbgchains[1]):
				continue
			gbgactset(vals)
			gbgset(symva('g_VehicleCenterPadLock'), (lock,))
			check('cVehicleControls_Tick' + fmt((vals, lock)), lambda c=c: (SDTest.Call('cVehicleControls_Tick', 'tviif', gbgctlp, c,
				0.05), gbgctl(gbgctlp), gbgfl(symva('g_VehicleCamDist'), 1), gbgs(c + 0x650)))
		# BlockSwitchAdvance in the strafe mode
		gbgbsa = gbgi(symva('g_VehicleBlockSwitchAdvance'))
		gbgpoke(symva('g_VehicleBlockSwitchAdvance'), 1)
		gbgactset(((18, 1.0), (6, 0.5), (7, 0.25), (8, 1.0)))
		check('cVehicleControls_Tick block switch', lambda c=c: (SDTest.Call('cVehicleControls_Tick', 'tviif', gbgctlp, c, 0.05),
			gbgctl(gbgctlp)))
		gbgpoke(symva('g_VehicleBlockSwitchAdvance'), gbgbsa)
		# the empty weapon: the next one after TimeWSwitchNo
		gbgpoke(gbgctlp, gbgi(c + 0x5f0 + 0xc))
		if not gbgchains[0]:
			gbgpoke(gbgctlp, gbgi(c + 0x5f0 + 0xc) + 1)
		for vals in (((16, 1.0),), ((16, 1.0),), ((17, 1.0),), ((16, 1.0),), ((16, 1.0),), ((16, 1.0),), ()):
			gbgactset(vals)
			gbgsetbyte(gbgctlp + 0x20, 1)
			gbgsetbyte(gbgctlp + 0x21, 1)
			check('cVehicleControls_Tick empty' + fmt(vals), lambda c=c: (SDTest.Call('cVehicleControls_Tick', 'tviif', gbgctlp, c,
				0.5), gbgctl(gbgctlp)))
		# the rumble of the pad (action set 1, a camera block, the last hit just now)
		gbgcam = gbgz(0x240)
		gbgpoke(c + 0xf0, gbgcam)
		gbgpoke(symva('g_CurActionSet'), 1)
		gbgset(c + 0x118, (50.0, 0.02))
		for hit, p220, p224, shake, ra, rb in ((49.8, 0.0, 0.0, 0.5, 0.3, 0.2), (49.0, 0.5, 0.04, 3.0, 2.0, 0.0),
				(10.0, 0.5, 0.0, 0.0, 0.0, 0.0), (49.99, 0.01, 1.0, 1.5, 0.7, 1.2)):
			gbgactset(())
			gbgset(c + 0x308, (hit,))
			gbgset(gbgcam + 0x220, (p220, p224))
			gbgset(c + 0x4c8, (shake,))
			gbgset(gbgctlp + 0x34, (ra, rb, 1.0, 2.0))
			check('cVehicleControls_Tick rumble' + fmt((hit, p220, p224, shake, ra, rb)), lambda c=c: (SDTest.Call('cVehicleControls_Tick',
				'tviif', gbgctlp, c, 0.05), gbgctl(gbgctlp), gbgfl(symva('g_Pads') + 0xc, 4)))
		gbgpoke(c + 0xf0, 0)
		gbgfree(gbgcam)
		gbgpoke(symva('g_CurActionSet'), gbgcurset)
		gbgset(symva('g_Pads') + 0xc, gbgpad)
		gbgset(symva('g_VehicleCenterPadLock'), gbglock)
		gbgset(symva('g_VehicleCamDist'), gbgcamd[0])
		gbgset(symva('g_VehicleCamDist2'), gbgcamd[1])
		for i in range(32):
			gbgset(gbgacts + 0x7c * i + 8, gbgactsaved[i][0])
			gbgset(gbgacts + 0x7c * i + 0x18, gbgactsaved[i][1])
		gbgset(gbgctlp, gbgctlsaved)
		gbgpoke(c + 0x410, -1)
		gbgstr(c + 0x650, '')
	check('GBG SDGbgCar4 Del', lambda: Scrap.GetEntity('SDGbgCar4').Del())
