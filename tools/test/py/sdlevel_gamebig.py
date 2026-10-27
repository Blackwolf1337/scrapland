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
	return SDTest.Call(0x133650, 'ii', n)[0]
def gbgfree(p):
	SDTest.Call(0x133550, 'ii', p)
def gbgfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def gbgset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def gbgpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def gbgstr(a, s):
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
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
	SDTest.Call(0x323c50, 'vi', s)
def gbgfind(name):
	slot = SDTest.Call(0x131f70, 'tiis', gbgworld + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', gbgworld + 4, name, slot)[0]
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
	slot = SDTest.Call(0x131f70, 'tiis', gbgworld + 0x298, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', gbgworld + 0x298, name, slot)[0]
	if node:
		return gbgi(node)
	return 0

gbgworld = SDTest.Peek(0x467450, 'i')
gbgtime = SDTest.Call(0x1368e0, 'tfi', gbgworld + 0x2c8)[0]

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
	return map(lambda i: gbgs(0x8476b8 + 0x80 * i), range(4))
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
	r = SDTest.Call(0x0f51b0, 'tbisii', m, n, p, 1)[0]
	gbgfree(p)
	return r

gbgsaved = gbgz(0x200)
SDTest.Call(0x3197d0, 'iiii', gbgsaved, 0x8476b8, 0x200)
gbgdrop = gbgfl(0x867408, 12)
gbgatm = gbgfl(0x867644, 1)[0]
check('GBG atmosphere', lambda: gbgatm)
gbgmpos = (pp[0] + 3000.0, pp[1] + 100000.0, pp[2] + 3000.0)
gbgswfile = gbgs(SDTest.Peek(0x44a208, 'i'))
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
	check('cMissileEntity_SetModel empty', lambda m=m: (SDTest.Call(0x0f4b60, 'tvizfffi', m, None, 1.0, 1.0, 1.0, 0),
		SDTest.Call(0x0f4b60, 'tvisfffi', m, '', 2.0, 2.0, 2.0, 0), gbgm(m)))
	gbgset(m + 0xac, (0.25, -0.5, 0.0))
	gbgset(m + 0x28, gbgmpos)
	check('cMissileEntity_SetModel', lambda m=m: (SDTest.Call(0x0f4b60, 'tvisfffi', m, gbgswfile, 1.0, 1.0, 1.0, 0),
		gbgm(m), gbgfl(m + 0xf8, 3), gbgb(m + 0x3e8)))
	# the bees
	gbgsys = gbgdebris(gbgswfile)
	if gbgsys:
		gbgset(0x867408, (gbgmpos[0], gbgmpos[1], gbgmpos[2], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 2.0))
		check('cMissileEntity_AddToSwarm(3)', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, 3), gbgm(m),
			gbgbeestate(m)))
		gbgpoke(m + 0x57c, 2)
		gbgsrand(4242)
		check('cMissileEntity_AddToSwarm(2) cloud', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, 2), gbgm(m),
			gbgbeestate(m)))
		gbgpoke(m + 0x57c, 0)
		check('cMissileEntity_AddToSwarm(0)', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, 0), gbgm(m)))
		check('cMissileEntity_AddToSwarm(-1)', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, -1), gbgm(m)))
		gbgsetbyte(m + 0x58c, 1)
		check('cMissileEntity_AddToSwarm hidden', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, 2), gbgm(m)))
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
			check('cMissileEntity_Swarm' + fmt((dt, typ)), lambda m=m, dt=dt: (SDTest.Call(0x0f59b0, 'tvif', m, dt),
				gbgm(m), gbgbeestate(m)))
		# a bee far outside the radius
		gbgbee = gbgi(m + 0x590)
		if gbgbee:
			gbgset(gbgbee + 0x1c, (gbgmpos[0] + 5000.0, gbgmpos[1], gbgmpos[2]))
			gbgset(m + 0x580, (0.2,))
			check('cMissileEntity_Swarm far bee', lambda m=m: (SDTest.Call(0x0f59b0, 'tvif', m, 0.05), gbgm(m),
				gbgbeestate(m)))
		gbgpoke(m + 0x57c, 0)
		# Explode without flag584: the bees die where they are (OnBeeDeath), the slow ones go back to the system
		gbgsetbyte(m + 0x584, 0)
		gbgsetbyte(m + 0x585, 0)
		gbgbl = gbgbeelist(m)
		if len(gbgbl) > 1:
			gbgset(gbgbl[1][0] + 0x34, (0.0, 0.0, 0.0))
		gbgset(m + 0xd4, (0.0, 0.0, 0.0))
		check('cMissileEntity_Explode die', lambda m=m, bl=gbgbl: (SDTest.Call(0x0f5650, 'tviF', m,
			(gbgmpos[0], gbgmpos[1] + 500.0, gbgmpos[2])), gbgm(m), map(lambda x: gbgfl(x[0] + 0x1c, 13), bl),
			gbgcalls()))
		# Explode with flag584: the bees fly to the point
		check('cMissileEntity_AddToSwarm(4) again', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, 4), gbgm(m)))
		gbgsetbyte(m + 0x584, 1)
		gbgbl = gbgbeelist(m)
		k = 0
		for d, st in gbgbl:
			gbgset(d + 0x34, (100.0 * k + 50.0, -30.0, 7.0 * k))
			gbgset(d + 0x28, (0.1 * k, -0.2 * k, 0.3))
			k = k + 1
		for gbgat in ((gbgmpos[0] + 2000.0, gbgmpos[1], gbgmpos[2] - 1000.0),):
			check('cMissileEntity_Explode fly' + fmt(gbgat), lambda m=m, bl=gbgbl, at=gbgat: (SDTest.Call(0x0f5650,
				'tviF', m, at), gbgm(m), map(lambda x: gbgfl(x[0] + 0x1c, 13), bl), gbgcalls()))
		# the bees close to the point: the time is at least 0.01
		check('cMissileEntity_AddToSwarm(2) close', lambda m=m: (SDTest.Call(0x0f26f0, 'tvii', m, 2), gbgm(m)))
		gbgbl = gbgbeelist(m)
		for d, st in gbgbl:
			gbgset(d + 0x34, (1e6, 0.0, 0.0))
		check('cMissileEntity_Explode close', lambda m=m, bl=gbgbl: (SDTest.Call(0x0f5650, 'tviF', m,
			gbgfl(bl[0][0] + 0x1c, 3)), gbgm(m), map(lambda x: gbgfl(x[0] + 0x1c, 13), bl), gbgcalls()))
		gbgsetbyte(m + 0x584, 0)
	# no names without a debris system: cWorld_GetDebrisSys loads it and the original hangs on a missing file
	gbgpoke(m + 0x588, 0)

	# the network state through a memory file and a controller of our own
	gbgnc = gbgz(0x60)
	SDTest.Call(0x0fa1d0, 'tii', gbgnc)
	gbgf = gbgz(0x20)
	gbgfbuf = gbgz(0x200)
	SDTest.Call(0x12e050, 'tiiiii', gbgf, gbgfbuf, 0x200, 0)
	gbgnet = gbgi(m + 0xf4)
	gbgpoke(m + 0xf4, gbgnc)
	def gbgfbytes():
		n = SDTest.Call(0x12e2a0, 'tii', gbgf)[0]
		return map(lambda i: gbgb(gbgfbuf + i), range(n))
	def gbgwrite(m, dist2, full):
		SDTest.Call(0x12e1b0, 'tiisi', gbgf, '', 6)
		SDTest.Call(0x0f3b10, 'tvifii', m, dist2, gbgf, full)
		return gbgfbytes()
	def gbgread(m, isnew):
		SDTest.Call(0x12e240, 'tviiiii', gbgf, gbgfbuf, 0x200, 0, 0)
		SDTest.Call(0x12e1b0, 'tiisi', gbgf, '', 1)
		SDTest.Call(0x0f39b0, 'tviii', m, gbgf, isnew)
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
		SDTest.Call(0x12e1b0, 'tiisi', gbgf, '', 6)
		SDTest.Call(0x0f3b10, 'tvifii', m, 1e6, gbgf, 1)
		n = SDTest.Call(0x12e2a0, 'tii', gbgf)[0]
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
	check('cMissileEntity_SetEmbeddedFX deleted', lambda m=m: (SDTest.Call(0x0f4790, 'tvis', m, 'EMI'), gbgm(m)))
	gbgpoke(m + 0x20, gbgdead)
	for n in ('Bogus', '', 'D', 'Dumb', 'G', 'GearMissil', 'C', 'Cloudy', 'E', 'EMIx'):
		check('cMissileEntity_SetEmbeddedFX(%s)' % n, lambda m=m, n=n: (SDTest.Call(0x0f4790, 'tvis', m, n), gbgm(m)))
	for n, g in (('DumbMotor', 0x448478), ('GearMissile', 0x446a3c), ('Cloud', 0x448e74), ('EMI', 0x449310)):
		gbgfxsys = gbgdebris(gbgs(SDTest.Peek(g, 'i')))
		check('GBG fx system ' + n, lambda g=g, s=gbgfxsys: (gbgs(SDTest.Peek(g, 'i')), s != 0))
		if gbgfxsys:
			gbgset(m + 0x3f0, (2.0,))
			gbgsrand(55)
			check('cMissileEntity_SetEmbeddedFX(%s)' % n, lambda m=m, n=n: (SDTest.Call(0x0f4790, 'tvis', m, n),
				gbgm(m)))

	# Tick (vf23): far above, no target, Linear; the radius grows to the top
	def gbgtick(m, dt):
		SDTest.Call(0x0f3c40, 'tvif', m, dt)
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
	check('cWithLifeEntity_Vf48', lambda m=m: (SDTest.Call(0x0f46b0, 'tvi', m), gbgm(m)))
	check('GBG SDGbgSw Del', lambda: Scrap.GetEntity('SDGbgSw').Del())
SDTest.Call(0x3197d0, 'iiii', 0x8476b8, gbgsaved, 0x200)
gbgfree(gbgsaved)
gbgset(0x867408, gbgdrop)
check('GBG missile hits restored', lambda: gbghitnames())

# ---- the items a ship takes ----
gbgcar = gbgfind('SDGbgCar')
if not gbgcar:
	Scrap.CreateEntity('SDGbgCar', gbgmpos[0] + 1500.0, gbgmpos[1], gbgmpos[2], 'Car')
	gbgcar = gbgfind('SDGbgCar')
gbgitem = SDTest.Call(0x208360, 'tiisfffs', gbgworld, 'SDGbgItem', gbgmpos[0], gbgmpos[1] - 2000.0, gbgmpos[2],
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
			# a list made by Rebuild also has the enemies of the level (their count is not printed)
			r = r + (min(gbgi(l), 3), gbgfl(l + 4, 1), gbgi(l + 0x80c), gbgs(l + 8))
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
	check('cVehicleO648_IsStale no list', lambda o=o: SDTest.Call(0x0ff790, 'tbi', o)[0])
	check('cVehicleO648_Rebuild no list', lambda o=o: (SDTest.Call(0x0ff070, 'tvi', o), gbgtl(o)))
	check('cVehicleO648_Update no list', lambda o=o: (SDTest.Call(0x0ff6a0, 'tvif', o, 0.1), gbgtl(o)))
	check('cVehicleO648_Cycle no list', lambda o=o: (SDTest.Call(0x0ff480, 'tvii', o, 1), gbgtl(o)))
	check('cVehicleO648_PickInView no list', lambda o=o: (SDTest.Call(0x0ff5f0, 'tvi', o), gbgtl(o)))
	check('cVehicleO648_SetSlot(-1) no list', lambda o=o: (SDTest.Call(0x0ff3f0, 'tvii', o, -1), gbgtl(o)))
	check('cVehicleO648_SetSlot(0)', lambda o=o: (SDTest.Call(0x0ff3f0, 'tvii', o, 0), gbgtl(o),
		gbgfl(gbgi(o + 4) + 0x808, 1)[0] == -gbgfl(0x847b74, 1)[0]))
	check('cVehicleO648_SetSlot(1) again', lambda o=o: (SDTest.Call(0x0ff3f0, 'tvii', o, 1), gbgtl(o)))
	gbgemi = gbgfl(gbgcar + 0x58c, 1)
	for emi in (0.0, 5.0):
		gbgset(gbgcar + 0x58c, (emi,))
		check('cVehicleO648_IsBlocked' + fmt(emi), lambda o=o: SDTest.Call(0x0ff030, 'tbi', o)[0])
	gbgset(gbgcar + 0x58c, (5.0,))
	gbgpadlock = gbgfl(0x847ba4, 1)
	gbgset(0x847ba4, (1.0,))
	check('cVehicleO648_IsBlocked padlock', lambda o=o: SDTest.Call(0x0ff030, 'tbi', o)[0])
	gbgset(0x847ba4, gbgpadlock)
	gbgstr(o + 8, 'SDGbgCar3')
	check('cVehicleO648_Cycle blocked', lambda o=o: (SDTest.Call(0x0ff480, 'tvii', o, 1), gbgtl(o)))
	gbgstr(o + 8, 'SDGbgCar3')
	check('cVehicleO648_Update blocked', lambda o=o: (SDTest.Call(0x0ff6a0, 'tvif', o, 0.1), gbgtl(o)))
	gbgstr(o + 8, 'SDGbgCar3')
	check('cVehicleO648_PickInView blocked', lambda o=o: (SDTest.Call(0x0ff5f0, 'tvi', o), gbgtl(o)))
	gbgset(gbgcar + 0x58c, (0.0,))
	# the attacker time: long ago, then now (without attacker)
	gbghit = gbgfl(gbgcar + 0x308, 1)
	l = gbgi(o + 4)
	for hit in (-1000.0, gbgtime + 100.0):
		gbgset(gbgcar + 0x308, (hit,))
		check('cVehicleO648_Rebuild' + fmt(hit - gbgtime > 0), lambda o=o: (SDTest.Call(0x0ff070, 'tvi', o), gbgi(l) > 1,
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
		check('cVehicleO648_IsStale' + fmt((cur, target)), lambda o=o: SDTest.Call(0x0ff790, 'tbi', o)[0])
		check('cVehicleO648_Cycle' + fmt((nxt, cur, target)), lambda o=o, n=nxt: (SDTest.Call(0x0ff480, 'tvii', o, n),
			gbgtl(o)))
	gbgmylist(o, l, 0, 'SDGbgNobody')
	check('cVehicleO648_IsStale missing', lambda o=o: SDTest.Call(0x0ff790, 'tbi', o)[0])
	gbgmylist(o, l, 0, 'SDGbgCar3')
	gbgsetbyte(gbgc3 + 0x4c, 0)
	check('cVehicleO648_IsStale inactive', lambda o=o: SDTest.Call(0x0ff790, 'tbi', o)[0])
	gbgsetbyte(gbgc3 + 0x4c, 1)
	gbgmylist(o, l, 0, 'SDGbgCar3')
	gbgpoke(l, 1)
	check('cVehicleO648_IsStale one', lambda o=o: SDTest.Call(0x0ff790, 'tbi', o)[0])
	gbgmylist(o, l, 0, 'SDGbgCar3')
	gbgset(l + 0x808, (gbgtime - 100.0,))
	check('cVehicleO648_IsStale old', lambda o=o: SDTest.Call(0x0ff790, 'tbi', o)[0])
	# Update: the target stays, is gone, the wait
	for target, wait, team in (('SDGbgCar3', 1.0, 5), ('SDGbgCar3', 1.0, 3), ('SDGbgNobody', 1.0, 5),
			('SDGbgCar', 1.0, 5), ('', 0.0, 5)):
		gbgmylist(o, l, 0, target)
		gbgset(l + 4, (wait,))
		gbgpoke(gbgc3 + 0x388, team)
		check('cVehicleO648_Update' + fmt((target, wait, team)), lambda o=o: (SDTest.Call(0x0ff6a0, 'tvif', o, 0.25),
			gbgtl(o)))
	gbgpoke(gbgc3 + 0x388, 5)
	# PickInView: the car 2 straight ahead
	gbgcpos = gbgfl(gbgcar + 0x28, 3)
	gbgcang = gbgfl(gbgcar + 0xac, 3)
	for p in ((gbgcpos[0], gbgcpos[1], gbgcpos[2] + 800.0), (gbgcpos[0] + 800.0, gbgcpos[1], gbgcpos[2]),
			(gbgcpos[0], gbgcpos[1], gbgcpos[2] - 800.0)):
		SDTest.Call(gbgi(gbgi(gbgc2) + 0x1c) - 0x400000, 'tviF', gbgc2, p)
		gbgstr(o + 8, 'SDGbgCar3')
		check('cVehicleO648_PickInView' + fmt(p), lambda o=o: (SDTest.Call(0x0ff5f0, 'tvi', o), gbgtl(o)))
	check('cVehicleO648_SetSlot(-1)', lambda o=o: (SDTest.Call(0x0ff3f0, 'tvii', o, -1), gbgtl(o)))
	gbgpoke(gbgcar + 0x388, gbgteams[0])
	gbgpoke(gbgc2 + 0x388, gbgteams[1])
	gbgpoke(gbgc3 + 0x388, gbgteams[2])
	gbgstr(o + 8, '')
	# the constructor and destructor on memory of our own
	gbgo = gbgz(0x88)
	gbgpoke(gbgo, 0x1234)
	gbgpoke(gbgo + 4, 0x5678)
	gbgpoke(gbgo + 8, 0x41414141)
	check('cVehicleO648_ctor', lambda: (SDTest.Call(0x0fefb0, 'tii', gbgo)[0] == gbgo, gbgints(gbgo, 3)))
	check('cVehicleO648_SetSlot(2) own', lambda: (SDTest.Call(0x0ff3f0, 'tvii', gbgo, 2), gbgi(gbgo + 4) != 0))
	check('cVehicleO648_dtor', lambda: (SDTest.Call(0x0ff010, 'tvi', gbgo), gbgi(gbgo + 4) != 0))
	gbgfree(gbgo)
for n in ('SDGbgCar', 'SDGbgCar2', 'SDGbgCar3', 'SDGbgItem'):
	if gbgfind(n):
		check('GBG %s Del' % n, lambda n=n: Scrap.GetEntity(n).Del())
