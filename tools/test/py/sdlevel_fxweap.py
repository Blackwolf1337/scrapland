# game World/FX/FXWeapons.c (execfile'd by sdlevel.py, Python 1.5.2): the weapon shot effects made by
# Scrap.SetDebrisSys with the models and effect kinds of the weapon scripts (Start and Reset run on the new debris),
# then their methods on the real objects: the Updates with the frame time of the world timer set by hand (restored),
# the fields they read set before (rays, positions, the debris movement), the visible flag of the debris model poked
# for the laser (restored), IsAlive where it only reads the effect, the Tesla emitter on the ship Parked when it is
# a Car with weapon tables. Particles are read only while active, trail points only the ones written. rand is seeded
# before each use. Not called: IsAlive of the missiles and FXMisc_Alive (they step the debris: the camera decides),
# the Updates of ATPC and Tesla (in sdlevel_fxbig2.py).
def fwi(a):
	return SDTest.Peek(a, '@i')
def fwb(a):
	return SDTest.Peek(a, '@b')
def fwz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def fwfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def fwset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def fwints(a, n):
	r = []
	for i in range(n):
		r.append(fwi(a + 4 * i))
	return tuple(r)
def fwpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fwsrand(s):
	SDTest.Call('crt_srand', 'vi', s)
# a CGameTrail holder: the CTrail flags, color, width and scales, the counters, uv, uLen and the points written
# since the last clear (numTrails before head)
def fwtrail(h):
	t = fwi(h)
	if not t:
		return None
	c = fwi(t + 0x20)
	mx = c & 0xff
	num = (c >> 8) & 0xff
	head = (c >> 16) & 0xff
	r = (fwints(t + 0xc, 5), c, fwi(t + 0x24) & 0xffff, fwints(t + 0x2c, 4), fwb(t + 0x3c), fwints(t + 0x4c, 2))
	pts = fwi(t + 0x28)
	if pts and mx:
		for k in range(num):
			i = (head - 1 - k) % mx
			r = r + (fwints(pts + 0x14 * i, 5),)
	return r
# particle i of a CGameParticles: active, then position, color and size while active
def fwpart(p, i):
	r = SDTest.Call('CGameParticles_GetParticle', 'tbiiFiipF', p, i, (9.0, 9.0, 9.0), 0, 0, 6, (5.0,))
	if r[0]:
		return r
	return 0
def fwnpart(p):
	return fwi(fwi(p + 0xac) + 0xc)
# without the size: the start emits with a size that differs between runs (also in the exe alone)
def fwparts0(p, n):
	r = ()
	for i in range(n):
		x = fwpart(p, i)
		if x:
			x = x[0:3]
		r = r + (x,)
	return r
def fwparts(p, n):
	r = ()
	for i in range(n):
		r = r + (fwpart(p, i),)
	return r

fwworld = SDTest.Peek('g_pWorld', 'i')
fwtimer = fwworld + 0x2c8 + 0xc
fwsavedt = fwi(fwtimer)
fwdudv = SDTest.Peek('g_SceneDuDvRTarget', 'i')

def fwdebris(name, kind):
	fwsrand(4321)
	Scrap.SetDebrisSys(name, 1, 0.0, 0.0, 1.0, 0.0, 0, kind)
	s = SDTest.Call('cWorld_GetDebrisSys', 'tiis', fwworld, name)[0]
	d = fwi(s)
	return (s, d, fwi(d + 0x54))
fwkinds = (('Models/Weapons/Swarm/Swarm.M3D', 32, sym('cObjElementFXSwarmMissile_vftable')), ('Models/Weapons/Tesla/Shoot.M3D', 33, sym('cObjElementFXTesla_vftable')),
	('Models/Weapons/ATPC/Shoot.M3D', 34, sym('cObjElementFXATPC_vftable')), ('Models/Weapons/Laser/Shoot.M3D', 41, sym('cObjElementFXLaser_vftable')),
	('Models/Weapons/Inferno/Inferno.M3D', 43, sym('cObjElementFXInfernoMissile_vftable')), ('Models/Weapons/Inferno/Explosion.M3D', 44, sym('cObjElementFXInfernoExplosion_vftable')),
	('Models/Weapons/Devastator/Shoot.M3D', 46, sym('cObjElementFXDevastatorShoot_vftable')))
fwobjs = {}
for name, kind, vt in fwkinds:
	s, d, fx = fwdebris(name, kind)
	ok = d != 0 and fx != 0 and fwi(fx) == 0x400000 + vt
	if ok:
		fwobjs[kind] = (s, d, fx)
	check('FXW debris' + fmt((name, kind)), lambda d=d, fx=fx, ok=ok: (d != 0, fx != 0, ok))

# the debris model: position, rotation, velocities (the ones cDebris_Move adds over dt)
def fwmove(d, pos, rot, vel, avel):
	fwset(d + 0x1c, pos + rot + vel + avel)
def fwmodelpos(d):
	return SDTest.Call('CGameModel_GetPos', 'tiiF', d, (0.0, 0.0, 0.0))[1]

# ---- ATPC: Start set the trails, Reset clears ----
if fwobjs.has_key(34):
	s, d, fx = fwobjs[34]
	check('FXATPC started', lambda fx=fx: (fwi(fx + 0xcc) != 0, fwi(fx + 0x180), fwi(fx + 0x18c), fwi(fx + 0x1cc),
		fwi(fx + 0x1d4), fwi(fx + 0x1dc), fwtrail(fx + 0x1c4), fwtrail(fx + 0x1c8), fwtrail(fx + 0x1d0),
		fwtrail(fx + 0x1d8), fwtrail(fx + 0x1e0)))
	fwpoke(fx + 0x180, 7)
	fwpoke(fx + 0x18c, 8)
	fwpoke(fx + 0x1dc, 9)
	check('FXATPC_Reset', lambda fx=fx, d=d: (SDTest.Call('cObjElementFXATPC_Reset', 'tvii', fx, d), fwi(fx + 0x180), fwi(fx + 0x18c),
		fwi(fx + 0x1dc), fwtrail(fx + 0x1c4), fwtrail(fx + 0x1e0)))

# ---- DevastatorShoot: the trail fades in over the duration ----
if fwobjs.has_key(46):
	s, d, fx = fwobjs[46]
	check('FXDevastator started', lambda fx=fx: (fwfl(fx + 8, 2), fwb(fx + 0x10), fwb(fx + 0x11), fwtrail(fx + 4)))
	for first, t, dur, dt in ((1, 0.0, 1.0, 0.05), (0, 0.0, 1.0, 0.05), (0, 0.5, 1.0, 0.25), (0, 0.9, 1.0, 0.2),
			(0, 1.0, 1.0, 0.0), (0, 0.5, 0.0, 0.1), (0, -2.0, 1.0, 0.1), (0, 2.0, 1.0, -1.5), (1, 5.0, 1.0, 0.1)):
		fwset(fx + 8, (dur, t))
		fwpoke(fx + 0x10, first)
		fwset(fwtimer, (dt,))
		check('FXDevastator_Update' + fmt((first, t, dur, dt)), lambda fx=fx, d=d: (SDTest.Call('cObjElementFXDevastatorShoot_Update', 'tviif', fx,
			d, 0.0), fwfl(fx + 8, 2), fwb(fx + 0x10), fwtrail(fx + 4)))
	for first, t, dur in ((1, 2.0, 1.0), (0, 0.5, 1.0), (0, 1.0, 1.0), (0, 1.5, 1.0)):
		fwset(fx + 8, (dur, t))
		fwpoke(fx + 0x10, first)
		check('FXDevastator_IsAlive' + fmt((first, t, dur)), lambda fx=fx, d=d, s=s: (SDTest.Call('cObjElementFXDevastatorShoot_IsAlive', 'tbiii',
			fx, d, s)[0], fwtrail(fx + 4)))
	check('FXDevastator_Reset', lambda fx=fx, d=d: (SDTest.Call('cObjElementFXDevastatorShoot_Reset', 'tvii', fx, d), fwfl(fx + 8, 2),
		fwb(fx + 0x10), fwtrail(fx + 4)))
	fwpoke(fwtimer, fwsavedt)

# ---- InfernoExplosion: rays drawn as trails, the core particles ----
if fwobjs.has_key(44):
	s, d, fx = fwobjs[44]
	ncores = fwnpart(fx + 0xb4)
	check('FXInfernoExplosion started', lambda fx=fx, ncores=ncores: (fwi(fx + 0xb0) != 0, fwi(fx + 0x160) != 0,
		ncores >= 2, fwfl(fx + 0x164, 2), fwi(fx + 0x24c0), fwtrail(fx + 0x2484), fwtrail(fx + 0x24bc)))
	nrays = 40
	def fwrays(fx, nrays):
		for i in range(nrays):
			fwset(fx + 0x184 + 12 * i, (i * 10.0, -i * 5.0, 7.0 + i))
			fwset(fx + 0xc04 + 12 * i, (0.6, 0.48 - i * 0.02, 0.64))
			fwset(fx + 0x1684 + 4 * i, (100.0 + 20.0 * i,))
			fwset(fx + 0x1a04 + 4 * i, (50.0 * i - 300.0,))
			fwset(fx + 0x1d84 + 4 * i, (25.0 - 3.0 * i,))
			if i % 3:
				fwset(fx + 0x2104 + 4 * i, (900.0 - 10.0 * i,))
			else:
				fwset(fx + 0x2104 + 4 * i, (3.4028234663852886e+38,))
		fwpoke(fx + 0x24c0, nrays)
		fwset(fx + 0x16c, (100.0, 200.0, -300.0))
	def fwexpl(fx, nrays):
		r = (fwfl(fx + 0x164, 2), fwfl(fx + 0x1684, nrays), fwfl(fx + 0x1a04, nrays), fwfl(fx + 0x1d84, nrays))
		for i in (0, 1, 2, 3, 14):
			r = r + (fwtrail(fx + 0x2484 + 4 * i),)
		return r
	fwrays(fx, nrays)
	for dudv in (0, 1):
		if dudv and ncores < 2:
			continue
		SDTest.Poke('g_SceneDuDvRTarget', dudv)
		for t, step, dt in ((0.0, 0.0, 0.05), (0.8, 0.002, 0.003), (0.8, 0.0, 0.1), (1.3, 0.0, 0.05), (1.6, 0.01, 0.0),
				(2.2, 0.0, 0.1), (3.0, 0.0, 0.05), (-1.0, 0.0, 0.5)):
			fwset(fx + 0x164, (t, step))
			fwset(fwtimer, (dt,))
			r = (dudv, t, step, dt)
			check('FXInfernoExplosion_Update' + fmt(r), lambda fx=fx, d=d, nrays=nrays: (SDTest.Call('cObjElementFXInfernoExplosion_Update', 'tviif',
				fx, d, 0.0), fwexpl(fx, nrays)))
			if dudv:
				check('FXInfernoExplosion cores' + fmt(r), lambda fx=fx: fwparts(fx + 0xb4, 2))
		fwrays(fx, nrays)
	SDTest.Poke('g_SceneDuDvRTarget', fwdudv)
	fwpoke(fwtimer, fwsavedt)
	check('FXInfernoExplosion_Reset', lambda fx=fx, d=d: (SDTest.Call('cObjElementFXInfernoExplosion_Reset', 'tvii', fx, d), fwfl(fx + 0x164, 2),
		fwi(fx + 0x24c0), fwtrail(fx + 0x2484), fwtrail(fx + 0x24bc)))

# ---- the missiles: the smoke trail behind the model, the fade at the end ----
def fwmissile(nm, fx, d, upd, reset, trail, part, fields, nfields, flare):
	check(nm + ' started', lambda fx=fx, trail=trail, part=part, fields=fields, nfields=nfields: (fwi(part + 0xac) != 0,
		fwfl(fields, nfields), fwtrail(trail), fwparts0(part, 4)))
	fwmove(d, (1000.0, 50.0, -200.0), (0.1, 0.5, 0.0), (300.0, 0.0, -100.0), (0.0, 0.2, 0.0))
	fwset(fields - 12, (0.0, 0.0, 0.0))
	steps = ((0.05, 0.1, -1.0), (0.05, 0.1, -1.0), (0.05, 1.0, -1.0), (0.05, 4.0, -1.0), (0.05, 0.0, -1.0),
		(0.05, 0.1, 0.3), (0.2, 0.1, 0.3), (0.3, 0.1, 0.3), (2.0, 0.0, 0.3))
	for dt, mdt, fs in steps:
		fwset(fwtimer, (dt,))
		fwset(fields + 12, (fs,))
		fwsrand(77)
		check(nm + '_Update' + fmt((dt, mdt, fs)), lambda fx=fx, d=d, upd=upd, mdt=mdt, trail=trail, fields=fields,
			nfields=nfields, flare=flare: (SDTest.Call(upd, 'tviif', fx, d, mdt), fwfl(fields - 12, 3 + nfields),
			fwtrail(trail), fwmodelpos(d), flare and fwflare(fx)))
	check(nm + '_Reset', lambda fx=fx, d=d, reset=reset, trail=trail, fields=fields, nfields=nfields, flare=flare:
		(SDTest.Call(reset, 'tvii', fx, d), fwfl(fields, nfields), fwtrail(trail), flare and fwflare(fx)))
	fwpoke(fwtimer, fwsavedt)
def fwflare(fx):
	f = fwi(fx + 0xdc)
	if not f:
		return None
	return (fwb(fx + 0xd8), fwb(f), fwints(f + 8, 3), fwi(f + 0x14), fwi(f + 0x1c), fwi(f + 0x30))

if fwobjs.has_key(43):
	s, d, fx = fwobjs[43]
	check('FXInfernoMissile flare', lambda fx=fx: fwi(fx + 0xdc) != 0)
	fwmissile('FXInfernoMissile', fx, d, sym('cObjElementFXInfernoMissile_Update'), sym('cObjElementFXInfernoMissile_Reset'), fx + 4, fx + 8, fx + 0xc4, 5, 0)
	check('FXInfernoMissile_SetFlare', lambda fx=fx: (SDTest.Call('cObjElementFXInfernoMissile_SetFlare', 'tvii', fx, 1), fwb(fx + 0xd8)))
	fwmissile('FXInfernoMissile flare', fx, d, sym('cObjElementFXInfernoMissile_Update'), sym('cObjElementFXInfernoMissile_Reset'), fx + 4, fx + 8, fx + 0xc4, 5, 1)
	check('FXInfernoMissile_SetFlare 0', lambda fx=fx: (SDTest.Call('cObjElementFXInfernoMissile_SetFlare', 'tvii', fx, 0), fwb(fx + 0xd8)))
if fwobjs.has_key(32):
	s, d, fx = fwobjs[32]
	check('FXSwarmMissile node', lambda fx=fx: fwi(fx + 4))
	if fwi(fx + 4) != -1:
		fwmissile('FXSwarmMissile', fx, d, sym('cObjElementFXSwarmMissile_Update'), sym('cObjElementFXSwarmMissile_Reset'), fx + 8, fx + 0xc, fx + 0xc8, 5, 0)
	fwpoke(fx + 4, -1)
	check('FXSwarmMissile_Update no node', lambda fx=fx, d=d: (SDTest.Call('cObjElementFXSwarmMissile_Update', 'tviif', fx, d, 0.5),
		fwfl(fx + 0xd0, 1), fwmodelpos(d)))

# ---- Laser: Shoot sets the beam, Update moves it (hidden: only moves; seen: the trail and the particles) ----
if fwobjs.has_key(41):
	s, d, fx = fwobjs[41]
	npl = fwnpart(fx + 8)
	check('FXLaser started', lambda fx=fx, npl=npl: (fwi(fx + 0xb4) != 0, npl, fwfl(fx + 0xc4, 1), fwb(fx + 0x120),
		fwtrail(fx + 4)))
	fwset(fx + 0x110, (0.0,))
	def fwlaser(fx):
		return fwfl(fx + 0xc4, 23) + (fwb(fx + 0x120),)
	for a, b, t in (((0.0, 0.0, 0.0), (10000.0, 0.0, 0.0), 1.0), ((5.0, 5.0, 5.0), (5.0, 5.0, 5.0), 0.5),
			((-100.0, 20.0, 3000.0), (2500.0, -700.0, 1000.0), 0.25), ((1.0, 2.0, 3.0), (4.0, 6.0, 3.0), 0.0)):
		check('FXLaser_Shoot' + fmt((a, b, t)), lambda fx=fx, a=a, b=b, t=t: (SDTest.Call('cObjElementFXLaser_Shoot', 'tvifffffff', fx,
			a[0], a[1], a[2], b[0], b[1], b[2], t), fwlaser(fx)))
	model = fwi(d)
	visaddr = model + 0x120
	visword = fwi(visaddr)
	SDTest.Call('cObjElementFXLaser_Shoot', 'tvifffffff', fx, 100.0, -50.0, 20.0, 3600.0, 450.0, -480.0, 0.5)
	for vis, dt in ((0, 0.05), (0, 0.05), (1, 0.05), (1, 0.1), (1, 0.15), (0, 0.05), (1, 0.05), (1, 0.05), (1, 0.05),
			(1, 0.1), (1, 0.3)):
		fwpoke(visaddr, (visword & ~0xff00) | (vis << 8))
		fwset(fwtimer, (dt,))
		check('FXLaser_Update' + fmt((vis, dt)), lambda fx=fx, d=d, npl=npl, s=s: (SDTest.Call('cObjElementFXLaser_Update', 'tviif', fx, d,
			0.7), fwlaser(fx), fwtrail(fx + 4), fwparts(fx + 8, npl), fwmodelpos(d),
			SDTest.Call('cObjElementFXLaser_IsAlive', 'tbiii', fx, d, s)[0]))
	fwpoke(visaddr, visword)
	fwpoke(fwtimer, fwsavedt)
	for dist, ln in ((1.0, 2.0), (2.0, 2.0), (3.0, 2.0), (0.0, -1.0)):
		fwset(fx + 0x10c, (dist,))
		fwset(fx + 0xf0, (ln,))
		check('FXLaser_IsAlive' + fmt((dist, ln)), lambda fx=fx, d=d, s=s: SDTest.Call('cObjElementFXLaser_IsAlive', 'tbiii', fx, d, s)[0])
	fwpoke(fx + 0xc4, 0x3f800000)
	check('FXLaser_Reset', lambda fx=fx, d=d, npl=npl: (SDTest.Call('cObjElementFXLaser_Reset', 'tvii', fx, d), fwfl(fx + 0xc4, 1),
		fwb(fx + 0x120), fwtrail(fx + 4), fwparts(fx + 8, npl)))

# ---- Tesla: Start set the 16 trails; IsAlive with an emitter that does not shoot; the emitter Parked ----
if fwobjs.has_key(33):
	s, d, fx = fwobjs[33]
	def fwtesla(fx):
		r = (fwi(fx + 4) != 0, fwb(fx + 8), fwi(fx + 0x88), fwints(fx + 0x9c, 1), fwints(fx + 0x734, 4),
			fwfl(fx + 0x744, 2), fwi(fx + 0x74c) & 0xffff, fwints(fx + 0x750, 4), fwfl(fx + 0x760, 1))
		return r
	check('FXTesla started', lambda fx=fx: (fwi(fx + 0x14c) != 0, fwi(fx + 0x150), fwtesla(fx), fwtrail(fx + 0x6f4),
		fwtrail(fx + 0x6f8), fwtrail(fx + 0x724), fwtrail(fx + 0x730)))
	fwem = fwz(0x700)
	check('FXTesla_IsAlive no emitter', lambda fx=fx, d=d, s=s: (SDTest.Call('cObjElementFXTesla_IsAlive', 'tbiii', fx, d, s)[0],
		fwtesla(fx)))
	fwpoke(fx + 4, fwem)
	for shoot, t, fs in ((1, 0.5, -1.0), (0, 0.5, -1.0), (0, 0.5, 0.45), (0, 0.5, 0.3), (0, 0.5, 0.0), (1, 2.0, 1.0),
			(0, 0.0, 0.0)):
		fwset(fx + 0x744, (t, fs))
		fwpoke(fx + 0x74c, shoot | 0x100)
		check('FXTesla_IsAlive' + fmt((shoot, t, fs)), lambda fx=fx, d=d, s=s: (SDTest.Call('cObjElementFXTesla_IsAlive', 'tbiii', fx, d,
			s)[0], fwtesla(fx)))
	# the ship Parked by the entity table of the world
	slot = SDTest.Call('HashTable_Slot', 'tiis', fwworld + 4, 'Parked')[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', fwworld + 4, 'Parked', slot)[0]
	parked = 0
	if node:
		parked = fwi(node)
	iscar = 0
	if parked:
		iscar = SDTest.Call(fwi(fwi(parked) + 0x14) - 0x400000, 'tbis', parked, 'Car')[0]
	check('FXTesla Parked', lambda parked=parked, iscar=iscar: (parked != 0, iscar))
	# the weapon slots of Parked change between runs (also in the exe alone): no SetEmitter on it
	fwpoke(fx + 4, 0)
	fwpoke(fx + 0x9c, 0x01000101)
	fwset(fx + 0x734, (1.0, 2.0, 3.0, 4.0))
	fwset(fx + 0x750, (5.0, 6.0, 7.0, 8.0))
	check('FXTesla_Reset', lambda fx=fx, d=d: (SDTest.Call('cObjElementFXTesla_Reset', 'tvii', fx, d), fwtesla(fx), fwtrail(fx + 0x6f4),
		fwtrail(fx + 0x700), fwtrail(fx + 0x730)))
