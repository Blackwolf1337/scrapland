# game World/FX/FXVehicle.c and FXWeapons.c, the big ones (execfile'd by sdlevel.py, Python 1.5.2): the constructors,
# destructors and Delete of the weapon effects on zeroed memory, the Updates of the vehicle effects (FiredEntity,
# Accelerator, EMI, Fire, Sputnik) and of the ATPC and the Tesla with a car of our own (Scrap.CreateEntity "SDFb2Car"
# far away, the model SPoli1 set by SetModel; a second one as the Tesla target), the ATPC shot and the Swarm
# emitters on debris systems of our own (one debris each, the names of the engine variables). The effects are made by
# Scrap.SetDebrisSys as in sdlevel_fxveh.py. The weapon nodes of the car (not in SPoli1) are set to node 0. The frame
# time of the world timer, the visible flag of the models, R_GetModelVertex and the player pointer of the car are set
# by hand (restored). rand is seeded before each call. Particles are read through their parameters (the ones set by
# hand while active), trails through the points written.
# Not called: the Updates with a motor of the car whose model is not loaded (a failed CHECK ends the game), Sputnik
# with the entity "Sputnik" (not in the level), the Swarm emitters out of sight of the camera only as early returns.
def fb2i(a):
	return SDTest.Peek(a, '@i')
def fb2b(a):
	return SDTest.Peek(a, '@i') & 0xff
def fb2z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fb2fl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fb2set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fb2ints(a, n):
	r = []
	for fb2k in range(n):
		r.append(fb2i(a + 4 * fb2k))
	return tuple(r)
def fb2poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fb2setbyte(a, v):
	w = fb2i(a & ~3)
	sh = 8 * (a & 3)
	fb2poke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def fb2srand(s):
	SDTest.Call(0x323c50, 'vi', s)
def fb2loaded(m):
	return SDTest.Call(0x210da0, 'tbi', m)[0]
# the parameters of a CGameParticles without its padding and pointers
def fb2part(p):
	return (fb2ints(p, 0x26), fb2i(p + 0x98) & 0xffff, fb2ints(p + 0x9c, 2), fb2b(p + 0xa4))
def fb2npart(p):
	if not fb2i(p + 0xac):
		return 0
	return SDTest.Call(0x21d4d0, 'tii', p)[0]
# particle i of a CGameParticles: active, then position, color and size while active
def fb2particle(p, i):
	r = SDTest.Call(0x21d4e0, 'tbiiFiipF', p, i, (9.0, 9.0, 9.0), 0, 0, 6, (5.0,))
	if r[0]:
		return r
	return 0
def fb2particles(p, lo, hi):
	r = ()
	for i in range(lo, hi):
		r = r + (fb2particle(p, i),)
	return r
# a CGameTrail holder: the CTrail flags, color, width and scales, the counters, uv, uLen and the points written
def fb2trail(h):
	t = fb2i(h)
	if not t:
		return None
	c = fb2i(t + 0x20)
	mx = c & 0xff
	num = (c >> 8) & 0xff
	head = (c >> 16) & 0xff
	r = (fb2ints(t + 0xc, 5), c, fb2i(t + 0x24) & 0xffff, fb2ints(t + 0x2c, 4), fb2b(t + 0x3c), fb2ints(t + 0x4c, 2))
	pts = fb2i(t + 0x28)
	if pts and mx:
		for fb2k in range(num):
			i = (head - 1 - fb2k) % mx
			r = r + (fb2ints(pts + 0x14 * i, 5),)
	return r
def fb2modelpos(m):
	return (SDTest.Call(0x20fa30, 'tiiF', m, (0.0, 0.0, 0.0))[1], SDTest.Call(0x210830, 'tiiF', m, (0.0, 0.0, 0.0))[1])
def fb2rand():
	return SDTest.Call(0x323c2f, 'i')[0]

fb2world = SDTest.Peek(0x467450, 'i')
fb2timer = fb2world + 0x2c8 + 0xc
fb2savedt = fb2i(fb2timer)
fb2dudv = SDTest.Peek(0x4a8b34, 'i')
fb2getvert = SDTest.Peek(0x4a80c4, 'i')

# ---- the constructors, destructors and Delete of FXWeapons.cpp in zeroed memory ----
# rva of ctor, dtor, Delete, size, vtable, the dwords to read, the trails and lens flares (pointers made)
fb2classes = (('ATPC', 0x1f9690, 0x1f9ab0, 0x1f9ca0, 0x1e8, 0x3cdcb0, (4, 0x180, 0x18c, 0x1dc, 0x1e4),
		(0x1c4, 0x1c8, 0x1d0, 0x1d8, 0x1e0)),
	('DevastatorShoot', 0x1f9770, 0x1f9b10, 0x1f9cd0, 0x14, 0x3cdc80, (8, 0xc, 0x10), (4,)),
	('InfernoExplosion', 0x1f97e0, 0x1f9b30, 0x1f9d10, 0x24c8, 0x3cdce0, (0x164, 0x168, 0x24c4), (0x2484, 0x24bc)),
	('InfernoMissile', 0x1f9880, 0x1f9ba0, 0x1f9da0, 0xe4, 0x3cdcc8, (0xe0,), (4, 0xdc)),
	('Laser', 0x1f9900, 0x1f9bd0, 0x1f9df0, 0x124, 0x3cdc68, (0x120,), (4,)),
	('SwarmMissile', 0x1f9970, 0x1f9c00, 0x1f9e30, 0xe0, 0x3cdc50, (0xdc,), (8,)),
	('Tesla', 0x1f99e0, 0x1f9c30, 0x1f9e70, 0x768, 0x3cdc98, (4, 8, 0x88, 0x150, 0x744, 0x748, 0x74c, 0x760, 0x764),
		(0x6f4, 0x730)))
def fb2fields(o, offs):
	r = ()
	for a in offs:
		r = r + (fb2i(o + a),)
	return r
def fb2made(o, offs):
	r = ()
	for a in offs:
		r = r + (fb2i(o + a) != 0,)
	return r
for fb2name, fb2ctor, fb2dtor, fb2del, fb2size, fb2vt, fb2offs, fb2ptrs in fb2classes:
	fb2o = fb2z(fb2size)
	check('FB2 ctor ' + fb2name, lambda o=fb2o, c=fb2ctor, vt=fb2vt, offs=fb2offs, ptrs=fb2ptrs:
		(SDTest.Call(c, 'tii', o)[0] == o, fb2i(o) == 0x400000 + vt, fb2fields(o, offs), fb2made(o, ptrs)))
	check('FB2 dtor ' + fb2name, lambda o=fb2o, d=fb2dtor, offs=fb2offs: (SDTest.Call(d, 'tvi', o),
		fb2i(o) - 0x400000, fb2fields(o, offs)))
	fb2o = fb2z(fb2size)
	SDTest.Call(fb2ctor, 'tii', fb2o)
	check('FB2 Delete ' + fb2name, lambda o=fb2o, d=fb2del, offs=fb2offs: (SDTest.Call(d, 'tiii', o, 0)[0] == o,
		fb2i(o) - 0x400000, fb2fields(o, offs)))

# ---- the effects on the debris of a debris system of our own ----
def fb2debris(name, kind):
	fb2srand(1357)
	Scrap.SetDebrisSys(name, 1, 0.0, 0.0, 1.0, 0.0, 0, kind)
	s = SDTest.Call(0x209590, 'tiis', fb2world, name)[0]
	d = 0
	if s:
		d = fb2i(s)
	fx = 0
	if d:
		fx = fb2i(d + 0x54)
	return (s, d, fx)
fb2kinds = (('Models/GFX/DumbMotor.M3D', 30, 0x3cd9b0), ('Models/GFX/VehicleEMI.M3D', 36, 0x3cd9c8),
	('Models/GFX/Sputnik/VehicleSputnik.M3D', 39, 0x3cda28), ('Models/GFX/FiredDebris.M3D', 40, 0x3cd9e0),
	('Models/GFX/VehicleFire.M3D', 49, 0x3cd998), ('Models/Weapons/ATPC/Shoot.M3D', 34, 0x3cdcb0),
	('Models/Weapons/Tesla/Shoot.M3D', 33, 0x3cdc98))
fb2objs = {}
for fb2name, fb2kind, fb2vt in fb2kinds:
	fb2s, fb2d, fb2fx = fb2debris(fb2name, fb2kind)
	fb2ok = fb2d != 0 and fb2fx != 0 and fb2i(fb2fx) == 0x400000 + fb2vt
	if fb2ok:
		fb2objs[fb2kind] = (fb2s, fb2d, fb2fx)
	check('FB2 debris' + fmt((fb2name, fb2kind)), lambda d=fb2d, fx=fb2fx, ok=fb2ok: (d != 0, fx != 0, ok))

# ---- the car of our own ----
def fb2find(name):
	slot = SDTest.Call(0x131f70, 'tiis', fb2world + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', fb2world + 4, name, slot)[0]
	if node:
		return fb2i(node)
	return 0
fb2carobj = Scrap.CreateEntity('SDFb2Car', 56000.0, 50000.0, 50000.0, 'Car')
fb2car = fb2find('SDFb2Car')
fb2iscar = 0
if fb2car:
	fb2iscar = SDTest.Call(fb2i(fb2i(fb2car) + 0x14) - 0x400000, 'tbis', fb2car, 'Car')[0]
if fb2iscar:
	SDTest.Call(0x109db0, 'tvisfffi', fb2car, 'SPoli1', 1.0, 1.0, 1.0, 1)
fb2hasmodel = 0
if fb2iscar:
	fb2hasmodel = fb2loaded(fb2car + 0xb8)
if not fb2hasmodel:
	fb2car = 0
check('FB2 car', lambda: (fb2carobj is not None, fb2iscar, fb2hasmodel, fb2car != 0))
# the motors: exhaust nodes and their models loaded
def fb2motors(c):
	r = ()
	for k in range(4):
		m = fb2i(c + 0x728 + 4 * k)
		if m:
			r = r + ((fb2ints(m + 0x38, 2), fb2loaded(m), fb2loaded(m + 0x1c)),)
		else:
			r = r + (None,)
	return r
fb2loadedmotor = None
if fb2car:
	check('FB2 car motors', lambda: fb2motors(fb2car))
	check('FB2 car fields', lambda: (fb2b(fb2car + 0x4c), fb2i(fb2car + 0x20), fb2i(fb2car + 0xf0) != 0,
		fb2i(fb2car + 0x224) != 0, fb2fl(fb2car + 0x28c, 1), fb2fl(fb2car + 0x294, 1), fb2fl(fb2car + 0x46c, 1),
		fb2fl(fb2car + 0x4e4, 3)))
	# a motor model that is loaded: (motor, side) for the exhausts of Fire and EMI
	for fb2k in range(4):
		fb2m = fb2i(fb2car + 0x728 + 4 * fb2k)
		if fb2m and fb2loadedmotor is None:
			if fb2loaded(fb2m):
				fb2loadedmotor = (fb2k, 0)
			elif fb2loaded(fb2m + 0x1c):
				fb2loadedmotor = (fb2k, 1)
	check('FB2 car loaded motor', lambda: fb2loadedmotor)
	# SPoli1 has no motors (SetEntity of EMI and Fire would read through NULL): a motor of our own whose model 0 is a
	# copy of the CGameModel of the car, put in as motor 0 while the effects run (taken out again)
	fb2fakemotor = fb2z(0x100)
	fb2set(fb2fakemotor, fb2fl(fb2car + 0xb8, 7))
	fb2poke(fb2fakemotor + 0x38, -1)
	fb2poke(fb2fakemotor + 0x3c, -1)
	fb2carmotor0 = fb2i(fb2car + 0x728)
	fb2carmodel = fb2car + 0xb8
	fb2carvis = fb2i(fb2carmodel) + 0x121
	fb2carvisword = fb2i(fb2carvis & ~3)
	fb2carplayer = fb2i(fb2car + 0xf0)
	fb2fakeplayer = fb2z(0x100)
	fb2carpos = fb2fl(fb2car + 0x28, 3)
	def fb2carat(pos, rot):
		SDTest.Call(0x211d20, 'tviF', fb2carmodel, pos)
		SDTest.Call(0x211e50, 'tviF', fb2carmodel, rot)
	fb2carmpos = fb2modelpos(fb2carmodel)

# ---- FiredEntity: the smoke and fire behind the entity ----
if fb2car and fb2objs.has_key(30):
	fb2s, fb2d, fb2fx = fb2objs[30]
	def fb2fe(fx):
		return (fb2i(fx + 4) != 0, fb2b(fx + 8), fb2fl(fx + 0xc, 4), fb2fl(fx + 0xcc, 1), fb2part(fx + 0x1c))
	fb2carat((56000.0, 50000.0, 50000.0), (0.0, 0.3, 0.0))
	check('FB2 FiredEntity_SetEntity', lambda fx=fb2fx: (SDTest.Call(0x1f9150, 'tvii', fx, fb2car),
		fb2i(fx + 4) == fb2car, fb2fe(fx)))
	fb2poke(fb2timer, 0)
	check('FB2 FiredEntity_Update not visible', lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1eb8d0, 'tviif', fx, d, 0.1),
		fb2fe(fx)))
	fb2setbyte(fb2carvis, 1)
	# the moves: (position, rotation, frame time, time of the effect, first)
	for fb2pos, fb2rot, fb2dt, fb2t, fb2first in (((56000.0, 50000.0, 50000.0), (0.0, 0.3, 0.0), 0.01, 0.0, 0),
			((56000.0, 50000.0, 50000.0), (0.0, 0.3, 0.0), 0.01, 0.0, 1),
			((56000.0, 50000.0, 50000.0), (0.0, 0.3, 0.0), 0.04, 0.0, 1),
			((57500.0, 50200.0, 49000.0), (0.1, 0.5, 0.2), 0.01, 0.0, 1),
			((56000.0, 50000.0, 50000.0), (0.0, -1.0, 0.0), 0.02, 0.02, 1),
			((56003.0, 50000.0, 50001.0), (0.5, 2.0, 0.1), 0.05, 0.0, 1),
			((61000.0, 51000.0, 47000.0), (0.0, 0.0, 0.0), 0.1, 0.2, 1)):
		fb2carat(fb2pos, fb2rot)
		fb2set(fb2timer, (fb2dt,))
		fb2set(fb2fx + 0xcc, (fb2t,))
		fb2setbyte(fb2fx + 8, fb2first)
		fb2srand(4711)
		check('FB2 FiredEntity_Update' + fmt((fb2pos, fb2rot, fb2dt, fb2t, fb2first)), lambda fx=fb2fx, d=fb2d:
			(SDTest.Call(0x1eb8d0, 'tviif', fx, d, 0.1), fb2fe(fx), fb2modelpos(d)))
	fb2poke(fb2carvis & ~3, fb2carvisword)
	fb2poke(fb2fx + 4, 0)
	check('FB2 FiredEntity_Update no entity', lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1eb8d0, 'tviif', fx, d, 0.1),
		fb2fe(fx)))
	fb2poke(fb2timer, fb2savedt)

# ---- Accelerator: made by its constructor, started on the FiredDebris debris ----
if fb2car and fb2objs.has_key(40):
	fb2s, fb2d, fb2fd = fb2objs[40]
	fb2ac = fb2z(0x4c0)
	SDTest.Call(0x1ea1a0, 'tii', fb2ac)
	SDTest.Call(0x1f8430, 'tvii', fb2ac, fb2d)
	fb2acn = fb2npart(fb2ac + 0xc)
	def fb2acc(ac):
		return (fb2ints(ac + 0xbc, 32), fb2ints(ac + 0x17c, 16), fb2ints(ac + 0x1bc, 48), fb2ints(ac + 0x27c, 96),
			fb2ints(ac + 0x3fc, 48))
	SDTest.Call(0x1f95c0, 'tvii', fb2ac, fb2car)
	fb2carat((56000.0, 50000.0, 50000.0), (0.2, 0.7, -0.1))
	SDTest.Poke(0x4a80c4, 1)
	fb2srand(2024)
	check('FB2 Accelerator_InitRays', lambda ac=fb2ac: (SDTest.Call(0x1f48c0, 'tvi', ac), fb2acc(ac)))
	check('FB2 Accelerator particles', lambda: fb2acn)
	# off: no player
	check('FB2 Accelerator_Update no player', lambda ac=fb2ac, d=fb2d: (SDTest.Call(0x1ed170, 'tviif', ac, d, 0.05),
		fb2acc(ac), fb2modelpos(d)))
	if fb2acn >= 32:
		fb2srand(2025)
		SDTest.Call(0x1f48c0, 'tvi', fb2ac)
		fb2poke(fb2car + 0xf0, fb2fakeplayer)
		for fb2dt in (0.05, 0.02, 0.1, 0.07, 0.04, 0.2):
			fb2srand(77)
			check('FB2 Accelerator_Update' + fmt((fb2dt,)), lambda ac=fb2ac, d=fb2d, dt=fb2dt: (SDTest.Call(0x1ed170,
				'tviif', ac, d, dt), fb2acc(ac), fb2trail(ac + 0x13c), fb2trail(ac + 0x140), fb2trail(ac + 0x178),
				fb2particles(ac + 0xc, 0, 8)))
		fb2poke(fb2car + 0xf0, fb2carplayer)
		check('FB2 Accelerator_Update rays off', lambda ac=fb2ac, d=fb2d: (SDTest.Call(0x1ed170, 'tviif', ac, d,
			0.05), fb2acc(ac), fb2trail(ac + 0x13c)))
	SDTest.Poke(0x4a80c4, fb2getvert)
	check('FB2 Accelerator_Delete', lambda ac=fb2ac: SDTest.Call(0x1ea8e0, 'tiii', ac, 0)[0] == ac)
	SDTest.Call(0x1f8cf0, 'tvii', fb2fd, fb2d)

# the effect back to none on the car
def fb2nofx():
	SDTest.Call(fb2i(fb2i(fb2car) + 0xb0) - 0x400000, 'tvii', fb2car, 0)
	SDTest.Call(fb2i(fb2i(fb2car) + 0xb4) - 0x400000, 'tviif', fb2car, 0, 0.0)

# ---- Sputnik: the phases without the entity Sputnik ----
if fb2car and fb2objs.has_key(39):
	fb2s, fb2d, fb2fx = fb2objs[39]
	def fb2sput(fx):
		return (fb2i(fx + 4) != 0, fb2fl(fx + 0xb8, 2), fb2i(fx + 0xc0), fb2fl(fx + 0xc4, 1), fb2i(fx + 0x190))
	# the entity Sputnik is not in the level: SetEntity leaves the node -1
	fb2sputnik = fb2find('Sputnik')
	check('FB2 Sputnik entity', lambda: fb2sputnik != 0)
	if fb2sputnik:
		fb2poke(fb2fx + 4, fb2car)
		fb2setbyte(fb2fx + 0xc8, fb2b(fb2car + 0x4c))
		fb2poke(fb2fx + 0x190, -1)
	else:
		SDTest.Call(0x1f9460, 'tvii', fb2fx, fb2car)
	check('FB2 Sputnik_SetEntity', lambda fx=fb2fx: fb2sput(fx))
	for fb2phase, fb2t, fb2dur, fb2dt in ((1, 0.0, 0.8, 0.05), (1, 0.85, 2.0, 0.05), (1, 1.5, 2.0, 0.1),
			(1, 3.0, 2.0, 0.05), (1, 2.5, 2.0, 0.6), (2, 0.5, 3.0, 0.05), (2, 1.0, 3.0, 0.1), (3, 1.0, 2.0, 0.05)):
		fb2poke(fb2fx + 0xc0, fb2phase)
		fb2set(fb2fx + 0xb8, (fb2t, fb2t))
		fb2set(fb2fx + 0xc4, (fb2dur,))
		fb2set(fb2timer, (fb2dt,))
		fb2srand(99)
		check('FB2 Sputnik_Update' + fmt((fb2phase, fb2t, fb2dur, fb2dt)), lambda fx=fb2fx, d=fb2d: (SDTest.Call(
			0x1f3960, 'tviif', fx, d, 0.0), fb2sput(fx), fb2modelpos(d), fb2b(fb2car + 0x4c)))
	fb2nofx()
	fb2poke(fb2fx + 4, 0)
	fb2poke(fb2timer, fb2savedt)

# ---- EMI: the rays along the vertices of the car ----
if fb2car and fb2objs.has_key(36):
	fb2s, fb2d, fb2fx = fb2objs[36]
	fb2emin = fb2npart(fb2fx + 8)
	def fb2emi(fx):
		r = (fb2i(fx + 4) != 0, fb2fl(fx + 0xb8, 2), fb2ints(fx + 0xd0, 3), fb2ints(fx + 0x160, 32),
			fb2ints(fx + 0x41e0, 48))
		for k in (0, 1, 9, 15):
			r = r + (fb2ints(fx + 0x1e0 + 0x300 * k, 12), fb2ints(fx + 0x31e0 + 0x100 * k, 4),
				fb2ints(fx + 0x42a0 + 0x400 * k, 8))
		return r
	# the entity and one exhaust set by hand (SetEntity reads the motors)
	fb2poke(fb2car + 0x728, fb2fakemotor)
	fb2poke(fb2fx + 4, fb2car)
	fb2poke(fb2fx + 0xdc, 1)
	fb2poke(fb2fx + 0xe0, 0)
	fb2poke(fb2fx + 0xe4, 0)
	check('FB2 EMI particles', lambda: (fb2emin, fb2loaded(fb2fakemotor)))
	fb2carat((56000.0, 50000.0, 50000.0), (0.1, 0.4, 0.0))
	SDTest.Poke(0x4a80c4, 1)
	fb2set(fb2timer, (0.03,))
	check('FB2 EMI_Update no player', lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1f0aa0, 'tviif', fx, d, 0.0),
		fb2emi(fx)))
	if fb2emin >= 48:
		fb2poke(fb2car + 0xf0, fb2fakeplayer)
		check('FB2 EMI_Update not visible', lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1f0aa0, 'tviif', fx, d, 0.0),
			fb2emi(fx)))
		fb2setbyte(fb2carvis, 1)
		fb2set(fb2d + 0x4c, (2.0,))
		for fb2dt, fb2ddt, fb2life in ((0.03, 0.0, 2.0), (0.03, 0.01, 2.0), (0.025, 0.0, 2.0), (0.01, 0.0, 2.0),
				(0.03, 0.0, 2.0), (0.05, 0.0, 0.7), (0.03, 0.0, 0.4), (0.2, 0.0, 2.0), (0.03, 0.0, 2.0)):
			fb2set(fb2timer, (fb2dt,))
			fb2set(fb2d + 0x4c, (fb2life,))
			fb2srand(31)
			check('FB2 EMI_Update' + fmt((fb2dt, fb2ddt, fb2life)), lambda fx=fb2fx, d=fb2d, ddt=fb2ddt: (SDTest.Call(
				0x1f0aa0, 'tviif', fx, d, ddt), fb2emi(fx), fb2trail(fx + 0x82a0), fb2trail(fx + 0x82a4),
				fb2particles(fx + 8, 0, 6)))
		fb2poke(fb2car + 0xf0, fb2carplayer)
		fb2poke(fb2carvis & ~3, fb2carvisword)
	SDTest.Poke(0x4a80c4, fb2getvert)
	fb2poke(fb2car + 0x728, fb2carmotor0)
	fb2nofx()
	fb2poke(fb2fx + 4, 0)
	SDTest.Call(0x1f8e60, 'tvii', fb2fx, fb2d)
	fb2poke(fb2timer, fb2savedt)

# ---- Fire: the flames along 16 vertices of the car and 8 of a motor ----
if fb2car and fb2objs.has_key(49):
	fb2s, fb2d, fb2fx = fb2objs[49]
	def fb2fire(fx):
		return (fb2i(fx + 4) != 0, fb2fl(fx + 0x168, 2), fb2i(fx + 0x174), fb2part(fx + 8), fb2part(fx + 0xb8))
	# one exhaust on the motor of our own; a seed whose rand after the 18 before (two sizes, 16 vertices) is odd
	fb2poke(fb2car + 0x728, fb2fakemotor)
	fb2poke(fb2fx + 4, fb2car)
	fb2poke(fb2fx + 0x174, 1)
	fb2poke(fb2fx + 0x178, 0)
	fb2poke(fb2fx + 0x17c, 0)
	fb2seed = 1
	while fb2seed < 200:
		fb2srand(fb2seed)
		for fb2k in range(18):
			fb2rand()
		if fb2rand() & 1:
			break
		fb2seed = fb2seed + 1
	check('FB2 Fire seed', lambda: fb2seed)
	fb2carat((56000.0, 50000.0, 50000.0), (0.0, 0.2, 0.1))
	fb2setbyte(fb2carvis, 1)
	SDTest.Poke(0x4a80c4, 1)
	fb2saved = fb2ints(fb2car + 0x28c, 3) + fb2ints(fb2car + 0x46c, 1) + fb2ints(fb2car + 0x4e4, 3)
	for fb2burn, fb2sf, fb2speed, fb2lv, fb2t, fb2et, fb2life, fb2pl in ((0.0, 0.5, 0.0, (0.0, 0.0, 0.0), 0.1, 0.1,
			2.0, 0), (1.0, 0.5, 300.0, (100.0, -20.0, -300.0), 0.1, 0.03, 2.0, 0),
			(1.0, 0.5, 300.0, (100.0, -20.0, -300.0), 0.1, 0.1, 2.0, 0),
			(1.0, 2.0, 100.0, (10.0, 50.0, 400.0), 0.3, 0.2, 0.2, 1),
			(2.5, 0.0, 600.0, (-80.0, 0.0, -1000.0), 1.0, 0.06, 1.0, 0)):
		fb2set(fb2car + 0x294, (fb2burn,))
		fb2set(fb2car + 0x28c, (fb2sf,))
		fb2set(fb2car + 0x46c, (fb2speed,))
		fb2set(fb2car + 0x4e4, fb2lv)
		fb2set(fb2fx + 0x168, (fb2t, fb2et))
		fb2set(fb2d + 0x4c, (fb2life,))
		if fb2pl:
			fb2poke(fb2car + 0xf0, fb2fakeplayer)
		fb2set(fb2timer, (0.02,))
		fb2srand(fb2seed)
		fb2r = (fb2burn, fb2sf, fb2speed, fb2lv, fb2t, fb2et, fb2life, fb2pl)
		check('FB2 Fire_Update' + fmt(fb2r), lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1f1b90, 'tviif', fx, d, 0.01),
			fb2fire(fx), fb2modelpos(d)))
		fb2poke(fb2car + 0xf0, fb2carplayer)
	fb2set(fb2car + 0x28c, fb2saved[0:3])
	fb2set(fb2car + 0x46c, fb2saved[3:4])
	fb2set(fb2car + 0x4e4, fb2saved[4:7])
	SDTest.Poke(0x4a80c4, fb2getvert)
	fb2poke(fb2car + 0x728, fb2carmotor0)
	fb2poke(fb2carvis & ~3, fb2carvisword)
	fb2nofx()
	fb2poke(fb2fx + 4, 0)
	fb2poke(fb2timer, fb2savedt)

# ---- ATPC: the shot from the car and the Updates of the beam ----
if fb2objs.has_key(34):
	fb2s, fb2d, fb2fx = fb2objs[34]
	fb2atn = fb2npart(fb2fx + 0x20)
	def fb2atpc(fx):
		return (fb2i(fx + 4) != 0, fb2fl(fx + 8, 6), fb2fl(fx + 0x180, 1), fb2i(fx + 0x188), fb2i(fx + 0x18c),
			fb2fl(fx + 0x1c0, 1), fb2ints(fx + 0x1cc, 1), fb2ints(fx + 0x1d4, 1), fb2fl(fx + 0x1dc, 1))
	check('FB2 ATPC particles', lambda: (fb2atn, SDTest.Peek(0x448c5c, 'i')))
	fb2wm = 0
	if fb2car:
		fb2wm = fb2i(fb2car + 0x5f0 + 0x18)
	check('FB2 ATPC car weapons', lambda: fb2wm != 0)
	if fb2car and fb2wm and fb2atn >= 9:
		fb2atw = SDTest.Peek(0x4487e8, 'i')
		fb2slots = fb2i(fb2wm + 12 * fb2atw)
		fb2poke(fb2wm + 12 * fb2atw, 0)
		fb2srand(5)
		check('FB2 ATPC_Shoot', lambda fx=fb2fx: (SDTest.Call(0x1fde50, 'tviisffffff', fx, fb2car, 'SDTarget',
			1000.0, 2000.0, 3000.0, -500.0, 1800.0, 9000.0), fb2atpc(fx), fb2part(fx + 0x20)))
		fb2poke(fb2wm + 12 * fb2atw, fb2slots)
		for fb2dudvon in (0, 1):
			SDTest.Poke(0x4a8b34, fb2dudvon)
			for fb2t, fb2dt, fb2up in ((0.0, 0.02, 0), (0.05, 0.03, 0), (0.1, 0.05, 1), (0.3, 0.01, 0), (0.5, 0.1, 0),
					(2.0, 0.05, 0), (2.0, 0.05, 0)):
				fb2set(fb2fx + 0x180, (fb2t,))
				fb2set(fb2timer, (fb2dt,))
				if fb2up:
					fb2poke(fb2fx + 4, fb2car)
				else:
					fb2poke(fb2fx + 4, 0)
				fb2r = (fb2dudvon, fb2t, fb2dt, fb2up)
				check('FB2 ATPC_Update' + fmt(fb2r), lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1f9f00, 'tviif', fx, d,
					0.0), fb2atpc(fx), fb2trail(fx + 0x1c4), fb2trail(fx + 0x1c8), fb2trail(fx + 0x1d0),
					fb2trail(fx + 0x1d8), fb2trail(fx + 0x1e0), fb2particles(fx + 0x20, fb2atn - 8, fb2atn),
					fb2modelpos(d)))
		SDTest.Poke(0x4a8b34, fb2dudv)
	fb2poke(fb2fx + 4, 0)
	fb2poke(fb2timer, fb2savedt)

# ---- Tesla: the rays of slots 0 and 2 of the car, no target (the aim of the car), then a second car of our own as
# the target of the hit info of the car (linked by AddLink, unlinked again) ----
if fb2car and fb2objs.has_key(33):
	fb2s, fb2d, fb2fx = fb2objs[33]
	fb2tn = fb2npart(fb2fx + 0xa0)
	if fb2tn > 64:
		fb2tn = 64
	fb2car2obj = Scrap.CreateEntity('SDFb2Car2', 57000.0, 50300.0, 49000.0, 'Car')
	fb2car2 = fb2find('SDFb2Car2')
	if fb2car2:
		SDTest.Call(0x109db0, 'tvisfffi', fb2car2, 'SPoli1', 1.0, 1.0, 1.0, 1)
	# the Tesla nodes of slots 0 and 2 (not in SPoli1, NodoMatrix would fail its CHECK) set to node 0 (restored)
	fb2wn = fb2i(fb2car + 0x60c) + 16 * SDTest.Peek(0x44a828, 'i')
	fb2wnsaved = fb2ints(fb2wn, 3)
	check('FB2 Tesla car2', lambda: (fb2tn, fb2car2obj is not None, fb2car2 != 0, fb2b(fb2car + 0x636),
		fb2fl(fb2car + 0x6dc, 2), fb2wnsaved))
	fb2poke(fb2wn, 0)
	fb2poke(fb2wn + 8, 0)
	def fb2tesla(fx):
		r = (fb2i(fx + 4) != 0, fb2i(fx + 0x88) == fb2car2, fb2b(fx + 8), fb2fl(fx + 0x8c, 3), fb2i(fx + 0x9c),
			fb2fl(fx + 0x734, 4), fb2fl(fx + 0x744, 2), fb2i(fx + 0x74c) & 0xffff, fb2fl(fx + 0x750, 4),
			fb2fl(fx + 0x760, 1))
		return r
	def fb2teslapts(fx):
		r = ()
		for k in (0, 2):
			r = r + (fb2fl(fx + 0x154 + 240 * k, 60), fb2fl(fx + 0x514 + 120 * k, 30))
			for j in range(4):
				r = r + (fb2trail(fx + 0x6f4 + 16 * k + 4 * j),)
		return r
	fb2poke(fb2fx + 0x9c, 0x00010001)
	# target, first, dt, step times, time, fade start
	for fb2tg, fb2first, fb2dt, fb2st, fb2t, fb2fs in ((0, 1, 0.02, 0.0, 0.0, -1.0), (0, 0, 0.03, 0.0, 0.02, -1.0),
			(0, 0, 0.2, 10.0, 0.05, -1.0), (1, 0, 0.05, 0.0, 0.25, -1.0), (1, 0, 0.1, 10.0, 0.3, -1.0),
			(1, 0, 0.1, 10.0, 0.5, 0.4), (0, 0, 0.05, 0.0, 0.6, 0.4)):
		if fb2tg and fb2car2:
			SDTest.Call(0x0c9a20, 'tvii', fb2car2, fb2car + 0x298)
		fb2poke(fb2fx + 4, fb2car)
		fb2setbyte(fb2fx + 0x74c, 1)
		fb2setbyte(fb2fx + 0x74d, fb2first)
		fb2set(fb2fx + 0x744, (fb2t, fb2fs))
		fb2set(fb2fx + 0x750, (fb2st, fb2st, fb2st, fb2st))
		fb2set(fb2timer, (fb2dt,))
		fb2srand(77)
		fb2r = (fb2tg, fb2first, fb2dt, fb2st, fb2t, fb2fs)
		check('FB2 Tesla_Update' + fmt(fb2r), lambda fx=fb2fx, d=fb2d: (SDTest.Call(0x1fc0a0, 'tviif', fx, d, 0.0),
			fb2tesla(fx), fb2teslapts(fx), fb2particles(fx + 0xa0, 0, fb2tn), fb2modelpos(d), fb2rand()))
		if fb2tg and fb2car2:
			SDTest.Call(0x0d4170, 'tvi', fb2car + 0x298)
	fb2poke(fb2fx + 4, 0)
	fb2poke(fb2fx + 0x88, 0)
	fb2poke(fb2fx + 0x9c, 0)
	fb2poke(fb2timer, fb2savedt)
	fb2poke(fb2wn, fb2wnsaved[0])
	fb2poke(fb2wn + 8, fb2wnsaved[2])
	if fb2car2obj is not None:
		check('SDFb2Car2 Del', lambda: fb2car2obj.Del())

# ---- the Swarm emitters on debris systems of our own (one debris each) at the camera ----
fb2bee = SDTest.Peek(0x44a5c0, 'p')
fb2expl = SDTest.Peek(0x44a520, 'p')
fb2smoke = SDTest.Peek(0x4673c0, 'p')
check('FB2 swarm files', lambda: (fb2bee, fb2expl, fb2smoke))
fb2swarm = {}
if fb2bee and fb2expl and fb2smoke:
	for fb2nm, fb2kind in ((fb2bee, 1), (fb2expl, 1), (fb2smoke, 48)):
		fb2swarm[fb2nm] = fb2debris(fb2nm, fb2kind)
		check('FB2 swarm debris ' + fb2nm, lambda nm=fb2nm: (fb2swarm[nm][1] != 0, fb2swarm[nm][2] != 0))
fb2cam = fb2fl(fb2world + 0x254 + 12 * fb2i(fb2world + 0x50), 3)
def fb2sp(nm):
	s, d, fx = fb2swarm[nm]
	if not fx:
		return None
	return (fb2fl(d + 0x1c, 12), fb2fl(d + 0x4c, 1), fb2b(fx + 0xb4), fb2b(fx + 0xb5), fb2part(fx + 4))
def fb2smk(nm):
	s, d, fx = fb2swarm[nm]
	if not fx:
		return None
	return (fb2fl(d + 0x1c, 12), fb2fl(d + 0x4c, 1), fb2fl(fx + 0xcc, 3), fb2b(fx + 0xd8))
if len(fb2swarm) == 3:
	for fb2ofs, fb2v, fb2size, fb2time, fb2n in (((10.0, 20.0, 30.0), (0.0, 0.0, 0.0), 1.0, 0.5, 3),
			((100.0, -50.0, 0.0), (300.0, 0.0, -100.0), 0.5, 1.2, 1), ((0.0, 0.0, 0.0), (10.0, 20.0, 30.0), 2.0, 0.25, 7),
			((900000.0, 0.0, 0.0), (0.0, 0.0, 0.0), 0.02, 0.5, 2)):
		fb2p = (fb2cam[0] + fb2ofs[0], fb2cam[1] + fb2ofs[1], fb2cam[2] + fb2ofs[2])
		fb2srand(123)
		fb2r = (fb2ofs, fb2v, fb2size, fb2time, fb2n)
		check('FB2 FXSwarm_BeeDeath' + fmt(fb2r), lambda p=fb2p, v=fb2v, size=fb2size, time=fb2time, n=fb2n:
			(SDTest.Call(0x1ff190, 'vffffffffi', p[0], p[1], p[2], v[0], v[1], v[2], size, time, n), fb2sp(fb2bee)))
	for fb2ofs, fb2v, fb2size, fb2time, fb2dir in (((10.0, 20.0, 30.0), (0.0, 0.0, 0.0), 1.0, 0.5, (0.0, 0.0, 1.0)),
			((100.0, -50.0, 0.0), (300.0, 0.0, -100.0), 0.5, 1.2, (1.0, -2.0, 0.5)),
			((0.0, -200.0, 0.0), (10.0, 20.0, 30.0), 2.0, 0.25, (0.0, 0.0, 0.0)),
			((900000.0, 0.0, 0.0), (0.0, 0.0, 0.0), 0.02, 0.5, (0.0, 1.0, 0.0))):
		fb2p = (fb2cam[0] + fb2ofs[0], fb2cam[1] + fb2ofs[1], fb2cam[2] + fb2ofs[2])
		fb2srand(321)
		fb2r = (fb2ofs, fb2v, fb2size, fb2time, fb2dir)
		check('FB2 FXSwarm_Hit' + fmt(fb2r), lambda p=fb2p, v=fb2v, size=fb2size, time=fb2time, dr=fb2dir:
			(SDTest.Call(0x1ffea0, 'vfffffffffff', p[0], p[1], p[2], v[0], v[1], v[2], size, time, dr[0], dr[1],
			dr[2]), fb2sp(fb2bee), fb2sp(fb2expl), fb2smk(fb2smoke)))

if fb2car:
	fb2carat(fb2carmpos[0], fb2carmpos[1])
if fb2carobj is not None:
	check('SDFb2Car Del', lambda: fb2carobj.Del())
