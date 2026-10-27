# game World/FX: the big FX leftovers (execfile'd by sdlevel.py, Python 1.5.2). The constructors, destructors and
# deleting destructors of the elements of FXMisc.c in calloc memory (the countermeasure ones are in
# sdlevel_fxcharact.py), FadeOutTrail_Update on its own trail, the lens flare constructor, and the Updates of
# DebrisCloud, Hook, MentalController DamageFX, CharacterTeleport, CharacterDeath, CharacterConversor and
# BettyAttack. The effects are made by Scrap.SetDebrisSys with their models (the new debris gets the element, Attach
# and Reset run on it); their entities are our own: a Missile, a Car and a WalkChar with the battery model, made far
# above the player and deleted at the end. Models count as visible with R_SplitScreen 1, the world timer delta is
# set by hand (both restored); rand is seeded with srand of the exe before each Update. Printed: the fields of the
# elements, the particle parameters, the particles placed by hand and the trail points written since the last Reset.
# Not called: the hook sparks (the debris system of the sparks needs the weapon scripts), the mental controller main
# effect, the Updates of GearMissile, PoliceGearConversion and EMI.
def fb1i(a):
	return SDTest.Peek(a, '@i')
def fb1b(a):
	return SDTest.Peek(a, '@i') & 0xff
def fb1z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fb1free(p):
	SDTest.Call(0x133550, 'ii', p)
def fb1fl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fb1set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fb1ints(a, n):
	r = []
	for fb1k in range(n):
		r.append(fb1i(a + 4 * fb1k))
	return tuple(r)
def fb1at(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs)
def fb1bytes(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i') & 0xff, offs)
def fb1poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fb1srand(s):
	SDTest.Call(0x323c50, 'vi', s)
def fb1rand():
	return SDTest.Call(0x323c2f, 'i')[0]
# the parameters of a CGameParticles without its pointers
def fb1part(p):
	return (fb1ints(p, 0x26), fb1i(p + 0x98) & 0xffff, fb1ints(p + 0x9c, 2), fb1b(p + 0xa4), fb1i(p + 0xac) != 0)
# the active flags, positions, colors and sizes of the particles first .. first + n - 1 (only placed ones)
def fb1parts(p, first, n):
	r = []
	if not fb1i(p + 0xac):
		return r
	for fb1k in range(first, first + n):
		x = SDTest.Call(0x21d4e0, 'tbiiFiipF', p, fb1k, (0.0, 0.0, 0.0), 0, 0, 0, (0.0,))
		if x[0]:
			r.append((x[1], x[2], x[3]))
		else:
			r.append(0)
	return r
# a CGameTrail: its state and the points written since the last Reset
def fb1trail(t):
	tr = fb1i(t)
	if not tr:
		return None
	mx = fb1b(tr + 0x20)
	num = fb1b(tr + 0x21)
	head = fb1b(tr + 0x22)
	n = head
	if head == 0 and num > 0:
		n = mx
	pts = fb1i(tr + 0x28)
	r = (fb1b(tr), fb1i(tr + 8), fb1ints(tr + 0xc, 5), fb1i(tr + 0x20), fb1i(tr + 0x24) & 0xffff, fb1ints(tr + 0x2c, 4),
		fb1b(tr + 0x3c), fb1ints(tr + 0x40, 5))
	if pts:
		r = r + (fb1ints(pts, 5 * n),)
	return r
fb1world = SDTest.Peek(0x467450, 'i')
fb1timer = fb1world + 0x2c8 + 0xc
fb1savedt = fb1i(fb1timer)
fb1split = 0x4a8c8c + 0x400000
fb1savedsplit = fb1i(fb1split)
fb1player = SDTest.Call(0x0c1430, 'tiii', fb1world, 0)[0]
fb1pp = SDTest.Call(0x0ef330, 'tiiF', fb1player, (0.0, 0.0, 0.0))[1]
fb1buf = fb1z(0x40)
fb1set(fb1buf, (100.0, 50.0, -20.0, 0.0, 0.5, 0.0))
fb1rot = fb1buf + 12
def fb1dt(dt):
	fb1set(fb1timer, (dt,))

# ---- the constructors and destructors of the FXMisc elements ----
# name, ctor, dtor, Delete, size, vtable, ints and bytes the constructor writes, offset of a trail or particles
fb1classes = (
	('FadeOutTrail', 0x1da190, 0x1da570, 0x1da6d0, 0x14, 0x3ccce4, (8, 0xc), (0x10, 0x11), 4, -1),
	('ItemFade', 0x1da200, 0x1da590, 0x1da710, 0xcc, 0x3ccd14, (0xb8, 0xbc, 0xc0, 0xc4), (0xc8, 0xc9), -1, 8),
	('ItemTake', 0x1da290, 0x1da5b0, 0x1da750, 0x648, 0x3cccfc, (0x630, 0x634, 0x638, 0x63c, 0x640), (0x644, 0x645),
		0xec, 0xc),
	('ModelFade', 0x1da330, 0x1da5e0, 0x1da7a0, 0x14, 0x3ccd44, (4, 8, 0xc), (0x10,), -1, -1),
	('ModelNoise', 0x1da370, 0x1da5f0, 0x1da7d0, 0x14, 0x3ccd5c, (8, 0xc), (0x10,), -1, -1),
	('SistPart', 0x1da3a0, 0x1da600, 0x1da800, 0xb8, 0x3cccb4, (), (0xb4, 0xb5), -1, 4),
	('SistPartDuDv', 0x1da400, 0x1da620, 0x1da840, 0xb8, 0x3ccd74, (), (0xb4, 0xb5), -1, 4),
	('Sparks', 0x1da460, 0x1da640, 0x1da880, 0xc70, 0x3ccd2c, (4, 8, 0x9b0), (0xc6c,), 0xb3c, 0xbbc),
	('Trail', 0x1da500, 0x1da6b0, 0x1da910, 0x1c, 0x3ccccc, (0x14,), (0x18, 0x19, 0x1a), 4, -1),
)
def fb1made(p, c):
	r = (fb1i(p) == 0x400000 + c[5], fb1at(p, c[6]), fb1bytes(p, c[7]))
	if c[8] >= 0:
		r = r + (fb1i(p + c[8]) != 0,)
	if c[9] >= 0:
		r = r + (fb1part(p + c[9]),)
	return r
for fb1c in fb1classes:
	fb1p = fb1z(fb1c[4])
	for fb1o in range(0, fb1c[4], 4):
		fb1poke(fb1p + fb1o, 0x55555555)
	check('cObjElementFX' + fb1c[0] + '_ctor', lambda c=fb1c, p=fb1p: (SDTest.Call(c[1], 'tii', p)[0] == p,
		fb1made(p, c)))
	check('cObjElementFX' + fb1c[0] + '_dtor', lambda c=fb1c, p=fb1p: (SDTest.Call(c[2], 'tvi', p), fb1i(p) ==
		0x7cb0e4))
	SDTest.Call(fb1c[1], 'tii', fb1p)
	check('cObjElementFX' + fb1c[0] + '_Delete', lambda c=fb1c, p=fb1p: (SDTest.Call(c[3], 'tiii', p, 0)[0] == p,
		fb1i(p) == 0x7cb0e4))
	fb1free(fb1p)

# ---- FadeOutTrail_Update on its trail: duration 0x8, t 0xc, running 0x10, on 0x11 ----
fb1e = fb1z(0x20)
SDTest.Call(0x1da190, 'tii', fb1e)
for fb1on, fb1run, fb1t, fb1d, fb1delta in ((0, 1, 0.0, 1.0, 0.25), (0, 0, 0.2, 1.0, 0.25), (1, 0, 0.2, 1.0, 0.25),
		(1, 0, 0.9, 1.0, 0.25), (1, 0, 5.0, 1.0, 0.25), (1, 1, 5.0, 1.0, 0.25), (1, 0, 0.5, 0.0, 0.0),
		(1, 0, -3.0, 1.0, 0.25)):
	fb1dt(fb1delta)
	fb1set(fb1e + 8, (fb1d, fb1t))
	fb1poke(fb1e + 0x10, fb1run | fb1on << 8)
	SDTest.Call(0x21e260, 'tvii', fb1e + 4, 0x77)
	check('cObjElementFXFadeOutTrail_Update' + fmt((fb1on, fb1run, fb1t, fb1d, fb1delta)), lambda: (SDTest.Call(0x1da950,
		'tviif', fb1e, 0, 0.0), fb1fl(fb1e + 8, 2), fb1bytes(fb1e, (0x10, 0x11)), SDTest.Call(0x21e1a0, 'tii',
		fb1e + 4)[0]))
fb1poke(fb1e + 0x10, 0)
SDTest.Call(0x1da570, 'tvi', fb1e)
fb1free(fb1e)

# ---- the lens flare constructor ----
fb1lf = fb1z(0x30)
check('cFXElementsLensFlare_ctor', lambda: (SDTest.Call(0x1cf8b0, 'tiisii', fb1lf, 'FireAFXFireDrop', fb1buf,
	fb1rot)[0] == fb1lf, fb1i(fb1lf) == 0x400000 + 0x3cc11c, fb1i(fb1lf + 0x2c) != 0))
fb1fo = fb1i(fb1lf + 0x2c)
if fb1fo:
	check('cFXElementsLensFlare_ctor flare', lambda: (fb1ints(fb1fo + 8, 4), fb1ints(fb1fo + 0x1c, 1), fb1b(fb1fo + 0x2c),
		fb1ints(fb1fo + 0x30, 1)))
check('cFXElementsLensFlare_Delete', lambda: SDTest.Call(0x1cfb90, 'tiii', fb1lf, 0)[0] == fb1lf)
fb1free(fb1lf)

# ---- our entities ----
fb1y = fb1pp[1] + 3000.0
fb1atm = fb1fl(0x867644, 1)[0]
if fb1atm + 1000.0 > fb1y and fb1atm < 50000.0:
	fb1y = fb1atm + 1000.0
fb1srand(4242)
fb1mis = SDTest.Call(0x208360, 'tiisfffs', fb1world, 'SDFb1Mis', fb1pp[0] + 300.0, fb1y, fb1pp[2], 'Missile')[0]
fb1srand(4243)
fb1walk = SDTest.Call(0x208360, 'tiisfffs', fb1world, 'SDFb1Walk', fb1pp[0], fb1y, fb1pp[2] + 500.0, 'WalkChar')[0]
if fb1walk:
	SDTest.Call(0x0fc7b0, 'tvisfffi', fb1walk, 'Models/Misc/Coins/Battery.M3D', 1.0, 1.0, 1.0, 0)
check('FB1 entities', lambda: (fb1mis != 0, fb1walk != 0, fb1walk and SDTest.Call(0x210da0, 'tbi', fb1walk +
	0xb8)[0]))
def fb1del(e):
	SDTest.Call(0x208810, 'tbis', fb1world + 4, SDTest.Peek(e + 4, '@p'))
	SDTest.Call(fb1i(fb1i(e) + 0xc) - 0x400000, 'tiii', e, 1)

# the element on the debris of a new debris system of file
def fb1debris(name, kind):
	if not name:
		return (0, 0, 0)
	fb1srand(1234)
	Scrap.SetDebrisSys(name, 1, 0.0, 0.0, 1.0, 0.0, 0, kind)
	s = SDTest.Call(0x209590, 'tiis', fb1world, name)[0]
	d = fb1i(s)
	if not d:
		return (s, 0, 0)
	return (s, d, fb1i(d + 0x54))
def fb1str(rva):
	p = SDTest.Peek(rva, 'i')
	if not p:
		return ''
	return SDTest.Peek(p, '@s')
fb1poke(fb1split, 1)

# ---- DebrisCloud: the cloud around the missile ----
fb1s, fb1d, fb1fx = fb1debris('Models/Weapons/Cloud/Cloud.M3D', 42)
fb1n = 0
if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cbe5c:
	fb1n = SDTest.Call(0x21d4d0, 'ii', fb1fx + 0xc)[0]
check('FB1 cloud', lambda: (fb1d != 0, fb1fx != 0, fb1n))
if fb1n >= 56 and fb1mis:
	fb1poke(fb1fx + 4, fb1mis)
	for fb1life, fb1top, fb1t, fb1et, fb1split1, fb1delta in ((3.0, 400.0, 0.3, 0.0, 1, 0.05), (3.0, 400.0, 2.0, 0.08, 1,
			0.05), (0.5, 400.0, 2.0, 0.08, 1, 0.05), (3.0, 1000.0, 1.0, 0.5, 1, 0.1), (3.0, 400.0, 2.0, 0.01, 0, 0.05),
			(1.2, 400.0, 2.0, 0.2, 0, 0.05)):
		fb1set(fb1mis + 0x3f0, (fb1life,))
		fb1set(fb1mis + 0x5a8, (fb1top,))
		fb1set(fb1fx + 0xbc, (fb1et, fb1t))
		fb1poke(fb1split, fb1split1)
		fb1dt(fb1delta)
		fb1srand(777)
		check('cObjElementFXDebrisCloud_Update' + fmt((fb1life, fb1top, fb1t, fb1et, fb1split1, fb1delta)), lambda:
			(SDTest.Call(0x1c9b50, 'tviif', fb1fx, fb1d, 0.0), fb1fl(fb1fx + 0xbc, 2), fb1ints(fb1fx + 0xd0, 56),
			fb1part(fb1fx + 0xc), fb1parts(fb1fx + 0xc, fb1n - 8, 8), fb1rand()))
	fb1poke(fb1split, 1)
	fb1poke(fb1fx + 4, 0)
	check('cObjElementFXDebrisCloud_Update no entity', lambda: (SDTest.Call(0x1c9b50, 'tviif', fb1fx, fb1d, 0.0),
		fb1ints(fb1fx + 0xbc, 2)))

# ---- the mental controller damage effect ----
fb1dfx = fb1z(0x3f8)
SDTest.Call(0x1d6270, 'tiisii', fb1dfx, 'MentalController0', fb1buf, fb1rot)
fb1srand(4321)
fb1ok = SDTest.Call(0x1d6750, 'tbi', fb1dfx)[0]
check('FB1 damage fx', lambda: (fb1ok, fb1ints(fb1dfx + 0xf4, 4), fb1ints(fb1dfx + 0x318, 4)))
fb1maxd = 0x4671e0 + 0x400000
fb1savedmax = fb1i(fb1maxd)
for fb1md, fb1delta, fb1sm, fb1fl1 in ((1e6, 0.05, 0.0, 0.0), (1e6, 0.5, 0.1, 0.0), (1e6, 0.05, 0.0, 0.2),
		(1e6, 1.0, 0.0, 0.2), (1.0, 0.5, 0.0, 0.0), (1e6, 0.0, 0.0, 0.0)):
	fb1set(fb1maxd, (fb1md,))
	fb1set(fb1dfx + 0xf0, (fb1sm,))
	fb1set(fb1dfx + 0x314, (fb1fl1,))
	fb1srand(99)
	check('cFXElementsMentalControllerDamageFX_Update' + fmt((fb1md, fb1delta, fb1sm, fb1fl1)), lambda: (SDTest.Call(
		0x1d7d20, 'tvif', fb1dfx, fb1delta), fb1b(fb1dfx + 0x3c), fb1ints(fb1dfx + 0xf0, 1), fb1ints(fb1dfx + 0x314, 1),
		fb1ints(fb1dfx + 0x3c8, 4), fb1part(fb1dfx + 0x40), fb1part(fb1dfx + 0x264), fb1rand()))
fb1poke(fb1maxd, fb1savedmax)
SDTest.Call(0x1d6630, 'tvi', fb1dfx)
SDTest.Call(0x1d6560, 'tiii', fb1dfx, 0)
fb1free(fb1dfx)

# ---- the hook laser on a car of our own ----
fb1carobj = Scrap.CreateEntity('SDFb1Car', 52000.0, 50000.0, 50500.0, 'Car')
fb1slot = SDTest.Call(0x131f70, 'tiis', fb1world + 4, 'SDFb1Car')[0]
fb1node = SDTest.Call(0x131e50, 'tiisi', fb1world + 4, 'SDFb1Car', fb1slot)[0]
fb1car = 0
if fb1node:
	fb1car = fb1i(fb1node)
fb1laser = SDTest.Peek(0x449d64, 'i')
fb1wnode = -1
fb1nodes = 0
fb1hasmodel = 0
if fb1car:
	fb1cm = SDTest.Call(fb1i(fb1i(fb1car) + 0x98) - 0x400000, 'tii', fb1car)[0]
	fb1hasmodel = fb1cm != 0 and SDTest.Call(0x210da0, 'tbi', fb1cm)[0]
# the weapon nodes of the car: our own table, node 0 (the root of the model) for the laser
if fb1car and fb1hasmodel and fb1laser >= 0 and fb1laser < 16:
	fb1nodes = fb1z(0x100)
	fb1savednodes = fb1i(fb1car + 0x60c)
	fb1poke(fb1car + 0x60c, fb1nodes)
	fb1wnode = 0
check('FB1 car', lambda: (fb1car != 0, fb1hasmodel, fb1laser, fb1wnode >= 0))
fb1s, fb1d, fb1fx = fb1debris('Models/Weapons/Laser/Hook.M3D', 31)
if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cbe14:
	def fb1hook(fx):
		return (fb1i(fx + 4) == fb1car, fb1i(fx + 8) == fb1player, fb1fl(fx + 0x14, 1), fb1fl(fx + 0x20, 30),
			fb1i(fx + 0x98), fb1fl(fx + 0x14c, 1), fb1fl(fx + 0x154, 1), fb1bytes(fx, (0x10, 0x150)),
			fb1part(fx + 0x9c), fb1parts(fx + 0x9c, 0, 16), fb1trail(fx + 0x18), fb1trail(fx + 0x1c))
	fb1dt(0.05)
	fb1poke(fb1fx + 0x150, 0)
	check('cObjElementFXHook_Update point no emitter', lambda: (SDTest.Call(0x1cccb0, 'tviif', fb1fx, fb1d, 0.0),
		fb1hook(fb1fx)))
	fb1poke(fb1fx + 0x150, 1)
	fb1poke(fb1fx + 0x10, 0)
	check('cObjElementFXHook_Update target no emitter', lambda: (SDTest.Call(0x1cccb0, 'tviif', fb1fx, fb1d, 0.0),
		fb1hook(fb1fx)))
	if fb1wnode >= 0:
		fb1poke(fb1fx + 4, fb1car)
		fb1poke(fb1fx + 0xc, 0)
		# with target: the end point by hand, the points between pulled to the line
		fb1set(fb1fx + 0x20, (0.0, 0.0, 0.0, 100.0, 0.0, 0.0, 200.0, 30.0, 0.0, 52300.0, 50100.0, 50500.0))
		for fb1act, fb1fade, fb1tgt, fb1sp1, fb1delta in ((1, 0.0, 0, 1, 0.05), (0, 0.1, 0, 1, 0.1), (1, 0.0, 1, 1,
				0.05), (0, 0.0, 0, 0, 0.05)):
			fb1poke(fb1fx + 0x10, fb1act)
			fb1set(fb1fx + 0x14, (fb1fade,))
			fb1poke(fb1fx + 8, fb1tgt and fb1player)
			fb1poke(fb1split, fb1sp1)
			fb1dt(fb1delta)
			fb1srand(31)
			check('cObjElementFXHook_Update target' + fmt((fb1act, fb1fade, fb1tgt, fb1sp1, fb1delta)), lambda:
				(SDTest.Call(0x1cccb0, 'tviif', fb1fx, fb1d, 0.0), fb1hook(fb1fx), fb1rand()))
		fb1poke(fb1split, 1)
		fb1poke(fb1fx + 8, 0)
		fb1poke(fb1fx + 0x150, 0)
		for fb1delta in (0.05, 0.2):
			fb1dt(fb1delta)
			check('cObjElementFXHook_Update point' + fmt((fb1delta,)), lambda: (SDTest.Call(0x1cccb0, 'tviif', fb1fx,
				fb1d, 0.0), fb1hook(fb1fx)))
		fb1poke(fb1fx + 4, 0)
if fb1nodes:
	fb1poke(fb1car + 0x60c, fb1savednodes)
	fb1free(fb1nodes)
check('SDFb1Car Del', lambda: fb1carobj.Del())

# ---- the character effects on our WalkChar ----
fb1hasmodel = 0
if fb1walk:
	fb1hasmodel = SDTest.Call(0x210da0, 'tbi', fb1walk + 0xb8)[0]
if fb1walk and fb1hasmodel:
	fb1wm = fb1walk + 0xb8
	def fb1wmodel():
		return (fb1i(fb1wm + 0xc) & 0xff, SDTest.Call(0x210da0, 'tbi', fb1wm)[0])
	# Teleport: the column at the player
	fb1s, fb1d, fb1fx = fb1debris(fb1str(0x466fb8), 64)
	check('FB1 teleport', lambda: (fb1d != 0, fb1fx != 0 and fb1i(fb1fx) == 0x400000 + 0x3cb114))
	if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cb114:
		fb1poke(fb1fx + 4, fb1player)
		fb1poke(fb1fx + 8, fb1walk)
		for fb1mode, fb1t, fb1et, fb1delta in ((1, 0.05, 0.0, 0.05), (1, 0.3, 0.03, 0.05), (1, 0.6, 0.03, 0.05),
				(2, 0.3, 0.03, 0.05), (2, 0.85, 0.03, 0.05), (2, 0.95, 0.03, 0.05), (3, 0.5, 0.03, 0.05)):
			fb1poke(fb1fx + 0xbc, fb1mode)
			fb1set(fb1fx + 0xc0, (1.0, 0.2, 0.1, fb1t, fb1et))
			if fb1mode == 2:
				fb1set(fb1fx + 0xc4, (0.1, 0.2))
			fb1dt(fb1delta)
			fb1srand(64)
			check('cObjElementFXCharacterTeleport_Update' + fmt((fb1mode, fb1t, fb1et, fb1delta)), lambda: (SDTest.Call(
				0x1adf80, 'tviif', fb1fx, fb1d, 0.0), fb1fl(fb1fx + 0xc0, 5), fb1part(fb1fx + 0xc),
				fb1parts(fb1fx + 0xc, 0, 5), fb1rand()))
		fb1poke(fb1fx + 4, 0)
		fb1poke(fb1fx + 8, 0)
	# Death: the trails up from the vertices
	fb1s, fb1d, fb1fx = fb1debris(fb1str(0x466f88), 61)
	check('FB1 death', lambda: (fb1d != 0, fb1fx != 0 and fb1i(fb1fx) == 0x400000 + 0x3cb144))
	if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cb144:
		fb1poke(fb1fx + 4, fb1walk)
		for fb1t, fb1et, fb1last, fb1delta in ((0.1, 0.03, 0.0, 0.05), (0.3, 0.03, 0.0, 0.05), (0.6, 0.0, 0.0, 0.05),
				(0.8, 0.03, 0.0, 0.05), (1.05, 0.03, 0.9, 0.05), (1.2, 0.0, 1.1, 0.05), (1.7, 0.03, 1.6, 0.05)):
			fb1set(fb1fx + 0x218, (fb1t, fb1et, fb1last))
			fb1dt(fb1delta)
			fb1srand(61)
			check('cObjElementFXCharacterDeath_Update' + fmt((fb1t, fb1et, fb1last, fb1delta)), lambda: (SDTest.Call(
				0x1ac1b0, 'tviif', fb1fx, fb1d, 0.0), fb1fl(fb1fx + 0x218, 3), fb1fl(fb1fx + 0x7e4, 30),
				fb1fl(fb1fx + 0x244, 30), fb1ints(fb1fx + 0xd84, 15), fb1part(fb1fx + 8), fb1part(fb1fx + 0xb8),
				fb1part(fb1fx + 0x168), fb1parts(fb1fx + 8, 0, 6), fb1trail(fb1fx + 0x224), fb1trail(fb1fx + 0x228),
				fb1wmodel(), fb1fl(fb1walk + 0x110 + 0x14, 1), fb1rand()))
		fb1poke(fb1fx + 4, 0)
		fb1set(fb1walk + 0x110 + 0x14, (1.0,))
	# Conversor: our WalkChar is the character, the player the conversor (no nodes of a conversor), then none
	fb1s, fb1d, fb1fx = fb1debris(fb1str(0x466e60), 62)
	fb1ct = SDTest.Call(0x20f570, 'tbiiF', fb1player + 0xb8, 0, (-1.0,))
	check('FB1 conversor', lambda: (fb1d != 0, fb1fx != 0 and fb1i(fb1fx) == 0x400000 + 0x3cb12c, fb1ct))
	if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cb12c and fb1ct[0]:
		SDTest.Call(0x1b43d0, 'tvii', fb1fx, fb1player)
		for fb1ph, fb1t, fb1et, fb1delta, fb1conv in ((1, 0.05, 0.03, 0.05, 1), (1, 0.5, 0.03, 0.05, 1), (2, 0.5, 0.03,
				0.05, 1), (2, 1.9, 0.03, 0.05, 1), (1, 0.5, 0.0, 0.05, 1), (1, 0.5, 0.03, 0.05, 0), (2, 0.4, 0.03, 0.05,
				0)):
			fb1poke(fb1fx + 4, fb1conv and fb1player)
			fb1poke(fb1fx + 0xbc, fb1ph)
			fb1poke(fb1fx + 8, fb1walk)
			fb1set(fb1fx + 0xc0, (2.0, 0.3, 0.3, fb1t, fb1et))
			fb1set(fb1fx + 0x104, (0.05, 0.0, 0.5))
			fb1dt(fb1delta)
			fb1srand(62)
			check('cObjElementFXCharacterConversor_Update' + fmt((fb1ph, fb1t, fb1et, fb1delta, fb1conv)), lambda: (SDTest.Call(
				0x1a99f0, 'tviif', fb1fx, fb1d, 0.0), fb1fl(fb1fx + 0xc0, 5), fb1fl(fb1fx + 0x104, 3),
				fb1fl(fb1fx + 0x11c, 30), fb1fl(fb1fx + 0x284, 3), fb1part(fb1fx + 0xc), fb1parts(fb1fx + 0xc, 0, 30),
				fb1trail(fb1fx + 0x110), fb1trail(fb1fx + 0x114), fb1wmodel(), fb1rand()))
		fb1poke(fb1fx + 4, 0)
		fb1poke(fb1fx + 8, 0)
	# BettyAttack: the attack at a point above our WalkChar
	fb1s, fb1d, fb1fx = fb1debris(fb1str(0x44631c), 87)
	check('FB1 betty', lambda: (fb1d != 0, fb1fx != 0 and fb1i(fb1fx) == 0x400000 + 0x3cb6fc))
	if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cb6fc:
		fb1poke(fb1fx + 4, fb1walk)
		fb1set(fb1fx + 0x174, (fb1pp[0] + 10.0, fb1y + 20.0, fb1pp[2] + 480.0))
		for fb1t, fb1last, fb1ring, fb1dudv, fb1delta in ((0.0, 0.0, 0.0, 0, 0.05), (0.03, 0.0, 10.0, 1, 0.05),
				(0.3, 0.25, 50.0, 1, 0.05), (0.7, 0.68, 120.0, 0, 0.05), (1.0, 0.9, 200.0, 1, 0.1), (1.3, 1.25, 250.0, 0,
				0.05)):
			fb1set(fb1fx + 0x168, (0.0, fb1t, fb1last))
			fb1set(fb1fx + 0x188, (fb1ring,))
			fb1savedudv = SDTest.Peek(0x4a8b34, 'i')
			SDTest.Poke(0x4a8b34, fb1dudv)
			fb1dt(fb1delta)
			fb1srand(87)
			check('cObjElementFXBettyAttack_Update' + fmt((fb1t, fb1last, fb1ring, fb1dudv, fb1delta)), lambda: (
				SDTest.Call(0x1b87a0, 'tviif', fb1fx, fb1d, 0.0), fb1fl(fb1fx + 0x168, 3), fb1fl(fb1fx + 0x188, 1),
				fb1part(fb1fx + 8), fb1parts(fb1fx + 8, 0, 2), fb1trail(fb1fx + 0x180), fb1trail(fb1fx + 0x18c),
				fb1trail(fb1fx + 0x19c), fb1rand()))
			SDTest.Poke(0x4a8b34, fb1savedudv)
		fb1poke(fb1fx + 4, 0)
		check('cObjElementFXBettyAttack_Update no entity', lambda: (SDTest.Call(0x1b87a0, 'tviif', fb1fx, fb1d, 0.0),
			fb1fl(fb1d + 0x4c, 1)))

# ---- PoliceGearConversion on the player (read only: the model, the center, the actions; the waist node 0) ----
fb1s, fb1d, fb1fx = fb1debris(fb1str(0x446fa0), 76)
fb1n = 0
if fb1fx and fb1i(fb1fx) == 0x400000 + 0x3cb60c:
	fb1n = SDTest.Call(0x21d4d0, 'ii', fb1fx + 0x18)[0]
check('FB1 gear', lambda: (fb1d != 0, fb1fx != 0, fb1n, fb1fx and fb1ints(fb1fx + 0x8ac, 3)))
if fb1n >= 0xff:
	fb1poke(fb1fx + 4, fb1player)
	fb1poke(fb1fx + 0xc8, 0)
	# a second Update hangs in the original and in the C alike (this element is not set up like the game does it)
	for fb1t, fb1last, fb1et, fb1st, fb1delta in ((0.3, 0.25, 0.0, 0.0, 0.05),):
		fb1set(fb1fx + 8, (0.0, fb1t, fb1last))
		fb1set(fb1fx + 0x848, (fb1et,))
		fb1set(fb1fx + 0xcc, (fb1st,))
		fb1dt(fb1delta)
		fb1srand(76)
		check('cObjElementFXPoliceGearConversion_Update' + fmt((fb1t, fb1last, fb1et, fb1st, fb1delta)), lambda: (
			SDTest.Call(0x1c2170, 'tviif', fb1fx, fb1d, 0.0), fb1fl(fb1fx + 8, 3), fb1fl(fb1fx + 0xcc, 1),
			fb1ints(fb1fx + 0x844, 1), fb1fl(fb1fx + 0x848, 1), fb1fl(fb1fx + 0x1a4, 30), fb1fl(fb1fx + 0x474, 20),
			fb1fl(fb1fx + 0x564, 20), fb1fl(fb1fx + 0x744, 10), fb1bytes(fb1fx, range(0x96c, 0x999)),
			fb1part(fb1fx + 0x18), fb1parts(fb1fx + 0x18, 0xfe - 59, 60), fb1trail(fb1fx + 0x834),
			fb1trail(fb1fx + 0x838), fb1rand()))
	fb1poke(fb1fx + 4, 0)
	check('cObjElementFXPoliceGearConversion_Update no entity', lambda: (SDTest.Call(0x1c2170, 'tviif', fb1fx, fb1d,
		0.0), fb1fl(fb1fx + 8, 3)))

# ---- the end ----
fb1poke(fb1split, fb1savedsplit)
fb1poke(fb1timer, fb1savedt)
if fb1walk:
	if SDTest.Call(0x210da0, 'tbi', fb1walk + 0xb8)[0]:
		SDTest.Call(0x0fc7b0, 'tvisfffi', fb1walk, '', 1.0, 1.0, 1.0, 0)
	fb1del(fb1walk)
if fb1mis:
	fb1del(fb1mis)
fb1free(fb1buf)
