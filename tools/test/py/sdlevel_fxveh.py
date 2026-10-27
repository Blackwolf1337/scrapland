# game World/FX/FXVehicle.c (execfile'd by sdlevel.py, Python 1.5.2): the vehicle effects made by Scrap.SetDebrisSys
# with the models of the FX engine variables and the effect kinds of the debris factory (Start and Reset run on the
# new debris), the Accelerator and Spawn (their models are not in the archives) built by their constructors in zeroed
# memory and started on the FiredDebris debris. Then Reset, Start again, the setters with a car of our own (when it is
# a Car), the Updates of FiredDebris, SmokeDebris and Spawn with the frame time of the world timer set by hand
# (restored) and the visible flag of the debris model poked (restored), IsAlive only where it returns before the
# debris step (no entity), the constructors, destructors and Delete (flags 0) on zeroed memory.
# Not called: IsAlive with the debris step (the camera decides), RenderAttached past its first test (it draws).
def fxvi(a):
	return SDTest.Peek(a, '@i')
def fxvb(a):
	return SDTest.Peek(a, '@i') & 0xff
def fxvz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fxvfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxvset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fxvints(a, n):
	r = []
	for fxvk in range(n):
		r.append(fxvi(a + 4 * fxvk))
	return tuple(r)
def fxvpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fxvsrand(s):
	SDTest.Call(0x323c50, 'vi', s)
# the parameters of a CGameParticles without its padding and pointers
def fxvpart(p):
	return (fxvints(p, 0x26), fxvi(p + 0x98) & 0xffff, fxvints(p + 0x9c, 2), fxvb(p + 0xa4))
# the max segments of a CGameTrail
def fxvmax(t):
	return SDTest.Call(0x21e1b0, 'tii', t)[0]
# a CGameTrail holder: the CTrail flags, color, width and scales, the counters, uv, uLen and the points written
def fxvtrail(h):
	t = fxvi(h)
	if not t:
		return None
	c = fxvi(t + 0x20)
	mx = c & 0xff
	num = (c >> 8) & 0xff
	head = (c >> 16) & 0xff
	r = (fxvints(t + 0xc, 5), c, fxvi(t + 0x24) & 0xffff, fxvints(t + 0x2c, 4), fxvb(t + 0x3c), fxvints(t + 0x4c, 2))
	pts = fxvi(t + 0x28)
	if pts and mx:
		for fxvk in range(num):
			i = (head - 1 - fxvk) % mx
			r = r + (fxvints(pts + 0x14 * i, 5),)
	return r
# particle i of a CGameParticles: active, then position, color and size while active
def fxvparticle(p, i):
	r = SDTest.Call(0x21d4e0, 'tbiiFiipF', p, i, (9.0, 9.0, 9.0), 0, 0, 6, (5.0,))
	if r[0]:
		return r
	return 0

fxvworld = SDTest.Peek(0x467450, 'i')
fxvtimer = fxvworld + 0x2c8 + 0xc
fxvsavedt = fxvi(fxvtimer)

def fxvdebris(name, kind):
	fxvsrand(2468)
	Scrap.SetDebrisSys(name, 1, 0.0, 0.0, 1.0, 0.0, 0, kind)
	s = SDTest.Call(0x209590, 'tiis', fxvworld, name)[0]
	d = fxvi(s)
	return (s, d, fxvi(d + 0x54))
fxvkinds = (('Models/GFX/DumbMotor.M3D', 30, 0x3cd9b0), ('Models/GFX/VehicleEMI.M3D', 36, 0x3cd9c8),
	('Models/GFX/VehicleDamage.M3D', 38, 0x3cd980), ('Models/GFX/Sputnik/VehicleSputnik.M3D', 39, 0x3cda28),
	('Models/GFX/FiredDebris.M3D', 40, 0x3cd9e0), ('Models/GFX/VehicleDeathTrail.M3D', 45, 0x3cda10),
	('Models/GFX/SmokeDebris.M3D', 48, 0x3cd9f8), ('Models/GFX/VehicleFire.M3D', 49, 0x3cd998))
fxvobjs = {}
for fxvname, fxvkind, fxvvt in fxvkinds:
	fxvs, fxvd, fxvfx = fxvdebris(fxvname, fxvkind)
	fxvok = fxvd != 0 and fxvfx != 0 and fxvi(fxvfx) == 0x400000 + fxvvt
	if fxvok:
		fxvobjs[fxvkind] = (fxvs, fxvd, fxvfx)
	check('FXV debris' + fmt((fxvname, fxvkind)), lambda d=fxvd, fx=fxvfx, ok=fxvok: (d != 0, fx != 0, ok))

# a car of our own far away: the AI ship Parked moves, its values differ between two runs of the exe
fxvcarobj = Scrap.CreateEntity('Car', 52000.0, 50000.0, 50000.0, 'SDFxvCar')
fxvslot = SDTest.Call(0x131f70, 'tiis', fxvworld + 4, 'SDFxvCar')[0]
fxvnode = SDTest.Call(0x131e50, 'tiisi', fxvworld + 4, 'SDFxvCar', fxvslot)[0]
fxvparked = 0
if fxvnode:
	fxvparked = fxvi(fxvnode)
fxvcar = 0
if fxvparked:
	fxvcar = SDTest.Call(fxvi(fxvi(fxvparked) + 0x14) - 0x400000, 'tbis', fxvparked, 'Car')[0]
# the four motors of the ship (SetEntity reads their exhaust nodes)
if fxvcar:
	for fxvk in range(4):
		if not fxvi(fxvparked + 0x728 + 4 * fxvk):
			fxvcar = 0
check('FXV car', lambda: (fxvparked != 0, fxvcar))
# the exhausts of a SetEntity: count and (motor, side) pairs
def fxvexh(a):
	n = fxvi(a)
	if n < 0 or n > 8:
		return n
	return (n, fxvints(a + 4, 2 * n))

# ---- the constructors, destructors and Delete in zeroed memory ----
# rva of ctor, dtor, Delete, size, vtable, the dwords to read
fxvclasses = (('FiredDebris', 0x1ea070, 0x1ea5b0, 0x1ea820, 0xdc, 0x3cd9e0, (0xd8,)),
	('FiredEntity', 0x1ea0d0, 0x1ea5d0, 0x1ea860, 0xd4, 0x3cd9b0, (4, 0xd0)),
	('SmokeDebris', 0x1ea140, 0x1ea5f0, 0x1ea8a0, 0xdc, 0x3cd9f8, (0xd8,)),
	('Accelerator', 0x1ea1a0, 0x1ea610, 0x1ea8e0, 0x4c0, 0x3cda58, (0x4bc,)),
	('Damage', 0x1ea220, 0x1ea670, 0x1ea960, 0x2d0, 0x3cd980, (4, 0x14, 0x2cc)),
	('DeathTrail', 0x1ea2b0, 0x1ea6a0, 0x1ea9b0, 0x14, 0x3cda10, (8, 0xc, 0x10)),
	('EMI', 0x1ea320, 0x1ea6c0, 0x1ea9f0, 0x82e4, 0x3cd9c8, (4, 0xb8, 0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0x82e0)),
	('Fire', 0x1ea3e0, 0x1ea720, 0x1eaa70, 0x1fc, 0x3cd998, (4, 0x168, 0x1f8)),
	('Spawn', 0x1ea460, 0x1ea760, 0x1eaad0, 0xdd8, 0x3cda40, (4, 0xb8, 0xbc, 0xdd4)),
	('Sputnik', 0x1ea500, 0x1ea7c0, 0x1eab50, 0x1720, 0x3cda28, (4, 0xb8, 0xbc, 0xc0, 0xc4, 0x171c)))
def fxvfields(o, offs):
	r = ()
	for a in offs:
		r = r + (fxvi(o + a),)
	return r
fxvmade = {}
for fxvname, fxvctor, fxvdtor, fxvdel, fxvsize, fxvvt, fxvoffs in fxvclasses:
	fxvo = fxvz(fxvsize)
	check('FXV ctor ' + fxvname, lambda o=fxvo, c=fxvctor, vt=fxvvt, offs=fxvoffs: (SDTest.Call(c, 'tii', o)[0] == o,
		fxvi(o) == 0x400000 + vt, fxvfields(o, offs)))
	fxvmade[fxvname] = fxvo
	check('FXV dtor ' + fxvname, lambda o=fxvo, d=fxvdtor, offs=fxvoffs: (SDTest.Call(d, 'tvi', o), fxvi(o) - 0x400000,
		fxvfields(o, offs)))
	fxvo = fxvz(fxvsize)
	SDTest.Call(fxvctor, 'tii', fxvo)
	check('FXV Delete ' + fxvname, lambda o=fxvo, d=fxvdel, offs=fxvoffs: (SDTest.Call(d, 'tiii', o, 0)[0] == o,
		fxvi(o) - 0x400000, fxvfields(o, offs)))

# a Start attaches the particles again: release the ones a Start before attached (CGameParticles_Release)
def fxvrel(fx, offs):
	for o in offs:
		SDTest.Call(0x21d1c0, 'tvi', fx + o)
# ---- FiredDebris and SmokeDebris: Start/Reset, the Update along the way of the debris ----
# the debris model: position, rotation, velocities (the ones cDebris_Move adds over dt)
def fxvmove(d, pos, rot, vel, avel):
	fxvset(d + 0x1c, pos + rot + vel + avel)
def fxvmodelpos(d):
	return SDTest.Call(0x20fa30, 'tiiF', d, (0.0, 0.0, 0.0))[1]
# scale and spread (0xd0) are set by the vehicle code: only after they are poked
def fxvdeb(fx):
	return (fxvfl(fx + 0xb4, 2), fxvb(fx + 0xbc), fxvfl(fx + 0xc0, 4))
def fxvdebrisfx(nm, kind, start, reset, upd, steps):
	s, d, fx = fxvobjs[kind]
	p = fx + 4
	check(nm + ' started', lambda fx=fx, p=p: (fxvi(p + 0xac) != 0, fxvdeb(fx), fxvpart(p)))
	check(nm + '_Start', lambda fx=fx, d=d, p=p, start=start: (fxvrel(fx, (4,)), SDTest.Call(start, 'tvii', fx, d),
		fxvdeb(fx), fxvpart(p)))
	fxvset(fx + 0xb4, (3.0,))
	fxvset(fx + 0xc0, (1.0, 2.0, 3.0))
	check(nm + '_Reset', lambda fx=fx, d=d, p=p, reset=reset: (SDTest.Call(reset, 'tvii', fx, d), fxvdeb(fx),
		fxvparticle(p, 0)))
	model = fxvi(d)
	visaddr = model + 0x120
	visword = fxvi(visaddr)
	fxvset(fx + 0xd0, (40.0, 30.0))
	fxvmove(d, (1000.0, 50.0, -200.0), (0.1, 0.5, 0.0), (2500.0, 300.0, -900.0), (0.0, 0.2, 0.0))
	for vis, dt, mdt, life, md in steps:
		fxvpoke(visaddr, (visword & ~0xff00) | (vis << 8))
		fxvset(fxvtimer, (dt,))
		fxvset(d + 0x4c, (life,))
		fxvset(fx + 0xcc, (md,))
		fxvsrand(31)
		r = (vis, dt, mdt, life, md)
		check(nm + '_Update' + fmt(r), lambda fx=fx, d=d, p=p, upd=upd, mdt=mdt: (SDTest.Call(upd, 'tviif', fx, d, mdt),
			fxvdeb(fx), fxvpart(p), fxvparticle(p, 0), fxvparticle(p, 1), fxvmodelpos(d)))
	fxvpoke(visaddr, visword)
	fxvpoke(fxvtimer, fxvsavedt)
	SDTest.Call(reset, 'tvii', fx, d)
# visible, frame time, Update dt, the debris life, the minimum distance
fxvsteps = ((1, 0.05, 0.02, 5.0, 100.0), (1, 0.05, 0.05, 5.0, 100.0), (1, 0.2, 0.1, 0.3, 100.0),
	(0, 0.05, 0.05, 5.0, 100.0), (1, 0.01, 0.01, 0.5, 100.0), (1, 0.05, 0.3, 5.0, 0.0), (1, 0.5, 0.25, 2.0, 50.0),
	(1, 0.05, 0.0, 1.1, 5000.0), (1, 0.0, 0.0, 9.0, 100.0))
if fxvobjs.has_key(40):
	fxvdebrisfx('FXFiredDebris', 40, 0x1f8290, 0x1f8cf0, 0x1eabd0, fxvsteps)
if fxvobjs.has_key(48):
	fxvdebrisfx('FXSmokeDebris', 48, 0x1f8390, 0x1f8d60, 0x1ec980, fxvsteps)

# ---- FiredEntity ----
if fxvobjs.has_key(30):
	fxvs, fxvd, fxvfx = fxvobjs[30]
	def fxvfe(fx):
		return (fxvi(fx + 4) != 0, fxvb(fx + 8), fxvfl(fx + 0xcc, 1))
	check('FXFiredEntity started', lambda fxvfx=fxvfx: (fxvi(fxvfx + 0xc8) != 0, fxvfe(fxvfx)))
	fxvset(fxvfx + 0xcc, (2.0,))
	fxvpoke(fxvfx + 8, 1)
	check('FXFiredEntity_Start', lambda fxvfx=fxvfx, fxvd=fxvd: (fxvrel(fxvfx, (0x1c,)), SDTest.Call(0x1f8330, 'tvii', fxvfx, fxvd), fxvfe(fxvfx)))
	if fxvcar:
		check('FXFiredEntity_SetEntity', lambda fxvfx=fxvfx: (SDTest.Call(0x1f9150, 'tvii', fxvfx, fxvparked),
			fxvi(fxvfx + 4) == fxvparked, fxvfl(fxvfx + 0xc, 4), fxvfe(fxvfx)))
	fxvpoke(fxvfx + 8, 1)
	check('FXFiredEntity_Reset', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8d40, 'tvii', fxvfx, fxvd), fxvfe(fxvfx)))

# ---- Damage: Start, IsAlive without entity is not called (it steps the debris), SetEntity, RenderAttached ----
if fxvobjs.has_key(38):
	fxvs, fxvd, fxvfx = fxvobjs[38]
	def fxvdmg(fx):
		return (fxvi(fx + 4) != 0, fxvb(fx + 8), fxvi(fx + 0x14) != 0, fxvb(fx + 0x18),
			fxvints(fx + 0x17c, 2), fxvb(fx + 0x240), fxvi(fx + 0x244), fxvexh(fx + 0x248))
	check('FXDamage started', lambda fxvfx=fxvfx, fxvd=fxvd: (fxvi(fxvfx + 0x14) == fxvd, fxvi(fxvfx + 0xc8) != 0, fxvi(fxvfx + 0x178) != 0,
		fxvi(fxvfx + 0x230) != 0, fxvdmg(fxvfx)))
	fxvpoke(fxvfx + 8, 0)
	fxvpoke(fxvfx + 0x18, 1)
	fxvpoke(fxvfx + 0x17c, 5)
	check('FXDamage_Start', lambda fxvfx=fxvfx, fxvd=fxvd: (fxvrel(fxvfx, (0x1c, 0xcc, 0x184)), SDTest.Call(0x1f8540, 'tvii', fxvfx, fxvd), fxvdmg(fxvfx)))
	check('FXVehicle_RenderAttached no entity', lambda fxvfx=fxvfx: (SDTest.Call(0x1f9050, 'tvi', fxvfx), fxvdmg(fxvfx)))
	if fxvcar:
		check('FXDamage_SetEntity', lambda fxvfx=fxvfx: (SDTest.Call(0x1f91c0, 'tvii', fxvfx, fxvparked),
			fxvi(fxvfx + 4) == fxvparked, fxvfl(fxvfx + 0x234, 3), fxvdmg(fxvfx)))
	fxvpoke(fxvfx + 0x244, 3)
	check('FXDamage_Reset', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8de0, 'tvii', fxvfx, fxvd), fxvdmg(fxvfx)))

# ---- EMI: Start set the trails and the marks, IsAlive 0 without entity ----
if fxvobjs.has_key(36):
	fxvs, fxvd, fxvfx = fxvobjs[36]
	def fxvemi(fx):
		return (fxvi(fx + 4) != 0, fxvints(fx + 0xb8, 6), fxvints(fx + 0x160, 16), fxvints(fx + 0x1a0, 16),
			fxvints(fx + 0x41e0, 48), fxvints(fx + 0x42a0, 4), fxvints(fx + 0x42a0 + 0x3c00, 4), fxvb(fx + 0x82e0))
	check('FXVehicleEMI started', lambda fxvfx=fxvfx: (fxvi(fxvfx + 0xb4) != 0, fxvemi(fxvfx), fxvmax(fxvfx + 0x82a0),
		fxvtrail(fxvfx + 0x82dc)))
	fxvpoke(fxvfx + 0x160, 7)
	fxvpoke(fxvfx + 0x1a0, 8)
	fxvpoke(fxvfx + 0x4220, 9)
	fxvpoke(fxvfx + 0x42a0, 0x12345678)
	fxvpoke(fxvfx + 0xc0, 4)
	check('FXVehicleEMI_Start 4', lambda fxvfx=fxvfx, fxvd=fxvd: (fxvrel(fxvfx, (8,)), SDTest.Call(0x1f8680, 'tvii', fxvfx, fxvd), fxvemi(fxvfx)))
	fxvpoke(fxvfx + 0xc0, 16)
	fxvpoke(fxvfx + 0x4220 + 0x3c, 9)
	fxvpoke(fxvfx + 0x42a0 + 0x3c00, 0x12345678)
	fxvpoke(fxvfx + 0xbc, 3)
	check('FXVehicleEMI_IsAlive no entity', lambda fxvfx=fxvfx, fxvd=fxvd, fxvs=fxvs: (SDTest.Call(0x1f8bb0, 'tbiii', fxvfx, fxvd, fxvs)[0],
		fxvemi(fxvfx)))
	if fxvcar:
		check('FXVehicleEMI_SetEntity', lambda fxvfx=fxvfx: (SDTest.Call(0x1f92c0, 'tvii', fxvfx, fxvparked),
			fxvi(fxvfx + 4) == fxvparked, fxvexh(fxvfx + 0xdc)))
		fxvpoke(fxvfx + 4, 0)
	check('FXVehicleEMI_Reset', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8e60, 'tvii', fxvfx, fxvd), fxvemi(fxvfx),
		fxvtrail(fxvfx + 0x82a0)))

# ---- Fire ----
if fxvobjs.has_key(49):
	fxvs, fxvd, fxvfx = fxvobjs[49]
	def fxvfire(fx):
		return (fxvi(fx + 4) != 0, fxvi(fx + 0x16c), fxvi(fx + 0x174), fxvb(fx + 0x1f8))
	check('FXVehicleFire started', lambda fxvfx=fxvfx: (fxvi(fxvfx + 0xb4) != 0, fxvi(fxvfx + 0x164) != 0, fxvfire(fxvfx)))
	fxvpoke(fxvfx + 0x16c, 4)
	fxvpoke(fxvfx + 0x174, 5)
	check('FXVehicleFire_Start', lambda fxvfx=fxvfx, fxvd=fxvd: (fxvrel(fxvfx, (8, 0xb8)), SDTest.Call(0x1f87b0, 'tvii', fxvfx, fxvd), fxvfire(fxvfx)))
	check('FXVehicleFire_IsAlive no entity', lambda fxvfx=fxvfx, fxvd=fxvd, fxvs=fxvs: (SDTest.Call(0x1f8c00, 'tbiii', fxvfx, fxvd, fxvs)[0],
		fxvfire(fxvfx)))
	if fxvcar:
		check('FXVehicleFire_SetEntity', lambda fxvfx=fxvfx: (SDTest.Call(0x1f9390, 'tvii', fxvfx, fxvparked),
			fxvi(fxvfx + 4) == fxvparked, fxvexh(fxvfx + 0x174)))
	fxvpoke(fxvfx + 0x168, 6)
	check('FXVehicleFire_Reset', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8f30, 'tvii', fxvfx, fxvd), fxvi(fxvfx + 0x168), fxvfire(fxvfx)))

# ---- Sputnik ----
if fxvobjs.has_key(39):
	fxvs, fxvd, fxvfx = fxvobjs[39]
	def fxvsput(fx):
		return (fxvi(fx + 4) != 0, fxvints(fx + 0xb8, 3), fxvfl(fx + 0xc4, 1), fxvb(fx + 0x171c))
	check('FXVehicleSputnik started', lambda fxvfx=fxvfx: (fxvi(fxvfx + 0xb4) != 0, fxvsput(fxvfx), fxvints(fxvfx + 0xaec, 195),
		fxvints(fxvfx + 0x1104, 195)))
	check('FXVehicleSputnik_Start', lambda fxvfx=fxvfx, fxvd=fxvd: (fxvrel(fxvfx, (8,)), SDTest.Call(0x1f88e0, 'tvii', fxvfx, fxvd), fxvsput(fxvfx),
		fxvtrail(fxvfx + 0x194), fxvtrail(fxvfx + 0x19c), fxvtrail(fxvfx + 0x1c4)))
	check('FXVehicleSputnik_IsAlive no entity', lambda fxvfx=fxvfx, fxvd=fxvd, fxvs=fxvs: SDTest.Call(0x1f8c80, 'tbiii', fxvfx, fxvd, fxvs)[0])
	check('FXVehicleSputnik_SetC0', lambda fxvfx=fxvfx: (SDTest.Call(0x1f9660, 'tvii', fxvfx, -7), fxvi(fxvfx + 0xc0)))
	check('FXVehicleSputnik_SetC4', lambda fxvfx=fxvfx: (SDTest.Call(0x1f9670, 'tvif', fxvfx, 2.75), fxvfl(fxvfx + 0xc4, 1)))
	if fxvcar:
		check('FXVehicleSputnik_SetEntity', lambda fxvfx=fxvfx: (SDTest.Call(0x1f9460, 'tvii', fxvfx, fxvparked),
			fxvi(fxvfx + 4) == fxvparked, fxvexh(fxvfx + 0xcc), fxvb(fxvfx + 0xc8), fxvi(fxvfx + 0x190), fxvsput(fxvfx)))
		fxvpoke(fxvfx + 4, 0)
	fxvpoke(fxvfx + 0xb8, 5)
	fxvsrand(808)
	check('FXVehicleSputnik_Reset', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8fb0, 'tvii', fxvfx, fxvd), fxvsput(fxvfx),
		fxvints(fxvfx + 0xaec, 195), fxvints(fxvfx + 0x1104, 195), fxvtrail(fxvfx + 0x194)))

# ---- DeathTrail: Start; Update, IsAlive and Reset are the code of the Devastator shot ----
if fxvobjs.has_key(45):
	fxvs, fxvd, fxvfx = fxvobjs[45]
	check('FXDeathTrail started', lambda fxvfx=fxvfx: (fxvfl(fxvfx + 8, 2), fxvi(fxvfx + 0x10) & 0xffff, fxvtrail(fxvfx + 4)))
	check('FXDeathTrail_Start', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8670, 'tvii', fxvfx, fxvd), fxvmax(fxvfx + 4)))
	for fxvfirst, fxvt, fxvdur, fxvdt in ((1, 0.0, 1.0, 0.05), (0, 0.5, 1.0, 0.25), (0, 2.0, 1.0, 0.1)):
		fxvset(fxvfx + 8, (fxvdur, fxvt))
		fxvpoke(fxvfx + 0x10, fxvfirst)
		fxvset(fxvtimer, (fxvdt,))
		check('FXDeathTrail_Update' + fmt((fxvfirst, fxvt, fxvdur, fxvdt)), lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f0a10, 'tviif', fxvfx,
			fxvd, 0.0), fxvfl(fxvfx + 8, 2), fxvb(fxvfx + 0x10), fxvtrail(fxvfx + 4)))
		check('FXDeathTrail_IsAlive' + fmt((fxvfirst, fxvt, fxvdur, fxvdt)), lambda fxvfx=fxvfx, fxvd=fxvd, fxvs=fxvs:
			(SDTest.Call(0x1f8b80, 'tbiii', fxvfx, fxvd, fxvs)[0], fxvtrail(fxvfx + 4)))
	fxvpoke(fxvtimer, fxvsavedt)
	check('FXDeathTrail_Reset', lambda fxvfx=fxvfx, fxvd=fxvd: (SDTest.Call(0x1f8e30, 'tvii', fxvfx, fxvd), fxvfl(fxvfx + 8, 2),
		fxvb(fxvfx + 0x10), fxvtrail(fxvfx + 4)))

# ---- Accelerator and Spawn: made by hand, started on the FiredDebris debris ----
if fxvobjs.has_key(40):
	fxvs, fxvd, fxvfd = fxvobjs[40]
	fxvac = fxvz(0x4c0)
	SDTest.Call(0x1ea1a0, 'tii', fxvac)
	def fxvacc(ac):
		return (fxvi(ac + 4) != 0, fxvi(ac + 8) != 0, fxvints(ac + 0xbc, 32), fxvb(ac + 0x4bc))
	fxvset(fxvac + 0xbc, (1.0, 2.0))
	fxvset(fxvac + 0xfc + 0x3c, (3.0,))
	check('FXAccelerator_Start', lambda fxvac=fxvac, fxvd=fxvd: (SDTest.Call(0x1f8430, 'tvii', fxvac, fxvd), fxvi(fxvac + 0xb8) != 0,
		fxvacc(fxvac), fxvtrail(fxvac + 0x13c), fxvtrail(fxvac + 0x140), fxvtrail(fxvac + 0x178)))
	check('FXAccelerator_SetAccelerator', lambda fxvac=fxvac: (SDTest.Call(0x1f9120, 'tvii', fxvac, fxvac), fxvacc(fxvac)))
	if fxvcar:
		check('FXAccelerator_SetOwner', lambda fxvac=fxvac: (SDTest.Call(0x1f95c0, 'tvii', fxvac, fxvparked), fxvacc(fxvac)))
	fxvset(fxvac + 0xbc, (1.0, 2.0))
	fxvset(fxvac + 0xfc + 0x3c, (3.0,))
	check('FXAccelerator_Reset', lambda fxvac=fxvac, fxvd=fxvd: (SDTest.Call(0x1f8da0, 'tvii', fxvac, fxvd), fxvacc(fxvac),
		fxvtrail(fxvac + 0x13c)))
	check('FXAccelerator_Delete', lambda fxvac=fxvac: SDTest.Call(0x1ea8e0, 'tiii', fxvac, 0)[0] == fxvac)

	fxvsp = fxvz(0xdd8)
	SDTest.Call(0x1ea460, 'tii', fxvsp)
	def fxvspawn(sp):
		n = fxvi(sp + 0xdd0)
		r = (fxvi(sp + 4) != 0, fxvfl(sp + 0xb8, 2), n, fxvb(sp + 0xdd4))
		if n > 0 and n <= 75:
			r = r + (fxvints(sp + 0x7e0, n), fxvints(sp + 0x90c, n), fxvints(sp + 0xa38, n), fxvints(sp + 0xb64, n))
		return r
	fxvpoke(fxvsp + 0xdd0, 3)
	check('FXSpawn_Start', lambda fxvsp=fxvsp, fxvd=fxvd: (SDTest.Call(0x1f8860, 'tvii', fxvsp, fxvd), fxvi(fxvsp + 0xb4) != 0,
		fxvspawn(fxvsp), fxvtrail(fxvsp + 0xc90), fxvtrail(fxvsp + 0xca0)))
	check('FXSpawn_Update no entity', lambda fxvsp=fxvsp, fxvd=fxvd: (SDTest.Call(0x1f3140, 'tviif', fxvsp, fxvd, 0.1), fxvspawn(fxvsp)))
	if fxvcar:
		check('FXSpawn_SetEntity', lambda fxvsp=fxvsp: (SDTest.Call(0x1f9620, 'tvii', fxvsp, fxvparked), fxvspawn(fxvsp)))
		for fxvdt in (0.05, 0.1, 0.25, 0.0, 0.4, 0.2, 0.3):
			fxvset(fxvtimer, (fxvdt,))
			fxvsrand(57)
			check('FXSpawn_Update' + fmt((fxvdt,)), lambda fxvsp=fxvsp, fxvd=fxvd: (SDTest.Call(0x1f3140, 'tviif', fxvsp, fxvd, 0.5),
				fxvspawn(fxvsp), fxvtrail(fxvsp + 0xc90), fxvmodelpos(fxvd)))
		fxvpoke(fxvtimer, fxvsavedt)
		# the ship back to no effect, as the IsAlive of the effects do
		SDTest.Call(fxvi(fxvi(fxvparked) + 0xb0) - 0x400000, 'tvii', fxvparked, 0)
		SDTest.Call(fxvi(fxvi(fxvparked) + 0xb4) - 0x400000, 'tviif', fxvparked, 0, 0.0)
	check('FXSpawn_Reset', lambda fxvsp=fxvsp, fxvd=fxvd: (SDTest.Call(0x1f8f60, 'tvii', fxvsp, fxvd), fxvspawn(fxvsp), fxvtrail(fxvsp + 0xc90)))
	check('FXSpawn_Delete', lambda fxvsp=fxvsp: SDTest.Call(0x1eaad0, 'tiii', fxvsp, 0)[0] == fxvsp)
	SDTest.Call(0x1f8cf0, 'tvii', fxvfd, fxvd)

# ---- the end of FXScene.cpp: the names and the float of the Decontamination scene ----
fxvdc = fxvz(0x100)
check('FXSceneDecontamination_SetNames', lambda: (SDTest.Call(0x1ea000, 'tviss', fxvdc, 'DecontaminationDtritus',
	'Decon'), SDTest.Peek(fxvdc + 0xbc, '@s'), SDTest.Peek(fxvdc + 0xdc, '@s'), fxvints(fxvdc + 0xbc, 16)))
check('FXSceneDecontamination_SetFC', lambda: (SDTest.Call(0x1ea050, 'tvif', fxvdc, 145.0), fxvfl(fxvdc + 0xfc, 1)))
if fxvcarobj is not None:
	check('SDFxvCar Del', lambda: fxvcarobj.Del())
