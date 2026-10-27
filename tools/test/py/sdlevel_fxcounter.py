# game World/FX/FXCountermeasures.c, FXElementsOrbit.c, FXElementsPress.c (execfile'd by sdlevel.py, Python 1.5.2):
# the countermeasure effects made by Scrap.SetDebrisSys with the models and effect numbers of the weapon scripts (the
# new debris gets the effect, Start and Reset run on it), then their Reset/IsAlive/Explode/Update; the map elements of
# the Orbit and Press levels made from their models in Models/Elements (not linked into the world); the setters on
# zeroed memory. rand is seeded with srand of the exe before each use, the world timer delta is set by hand for the
# updates (and restored). Not called: the hook laser start (needs a ship with weapon nodes), the debris IsAlive of
# the effects that move the debris (the camera distance decides), DebrisCloud Warm (the update needs the entity of
# the countermeasure), the Press loader (SaveVar GameAct may be missing in FZ: a crash in both).
def fxi(a):
	return SDTest.Peek(a, '@i')
def fxb(a):
	return SDTest.Peek(a, '@i') & 0xff
def fxz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fxfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fxints(a, n):
	r = []
	for i in range(n):
		r.append(fxi(a + 4 * i))
	return tuple(r)
def fxpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
# the parameters of a CGameParticles without its padding and pointers
def fxpart(p):
	return (fxints(p, 0x26), fxi(p + 0x98) & 0xffff, fxints(p + 0x9c, 2), fxb(p + 0xa4))
def fxsrand(s):
	SDTest.Call(0x323c50, 'vi', s)
fxworld = SDTest.Peek(0x467450, 'i')
fxbuf = fxz(0x40)

# the camera position of the view
check('cWorld_GetCamPos', lambda: (SDTest.Call(0x1ce910, 'tiii', fxworld, fxbuf)[0] == fxbuf,
	fxfl(fxbuf, 3) == fxfl(fxworld + 0x254 + 12 * fxi(fxworld + 0x50), 3)))

# the effects on new debris; the idle list (sys + 0) starts with the debris Init made last
def fxdebris(name, kind):
	fxsrand(1234)
	Scrap.SetDebrisSys(name, 1, 0.0, 0.0, 1.0, 0.0, 0, kind)
	s = SDTest.Call(0x209590, 'tiis', fxworld, name)[0]
	d = fxi(s)
	return (s, d, fxi(d + 0x54))
fxkinds = (('Models/Weapons/Laser/Hook.M3D', 31, 0x3cbe14), ('Models/Weapons/EMI/EMI.M3D', 35, 0x3cbe2c),
	('Models/Weapons/Cloud/Cloud.M3D', 42, 0x3cbe5c), ('Models/Weapons/Sonic/Xplde.M3D', 50, 0x3cbe44))
fxobjs = {}
for name, kind, vt in fxkinds:
	fxsys, d, fx = fxdebris(name, kind)
	fxobjs[kind] = (fxsys, d, fx)
	check('FX debris' + fmt((name, kind)), lambda d=d, fx=fx, vt=vt: (d != 0, fx != 0, fx != 0 and fxi(fx) == 0x400000 + vt))

# hook: Start (the Dummy01 particles, the two trails) and Reset ran in SetDebrisSys
fxsys, d, fx = fxobjs[31]
if fx and fxi(fx) == 0x400000 + 0x3cbe14:
	check('FXHook started', lambda fx=fx: (fxi(fx + 0x148) != 0, fxi(fx + 0x98), fxints(fx + 0x14c, 1),
		fxints(fx + 0x154, 1), SDTest.Call(0x21e1b0, 'tii', fx + 0x18)[0], SDTest.Call(0x21e1b0, 'tii', fx + 0x1c)[0]))
	fxpoke(fx + 0x154, 0x3f800000)
	check('FXHook_Reset', lambda fx=fx, d=d: (SDTest.Call(0x1cee70, 'tvii', fx, d), fxints(fx + 0x154, 1)))
	check('FXHook_StopTrails', lambda fx=fx: (SDTest.Call(0x1ce5a0, 'tvi', fx), fxi(fx + 0x148) != 0))

# the hook IsAlive on zeroed memory without emitter: with target the fade time decides
fh = fxz(0x15c)
fadetime = fxfl(0x449d0c + 0x400000, 1)[0]
for wt in (0, 1):
	for fade in (0.0, 0.1, fadetime, 1.0, -1.0):
		fxpoke(fh + 0x150, wt)
		fxpoke(fh + 0x10, 1)
		fxset(fh + 0x14, (fade,))
		check('FXHook_IsAlive' + fmt((wt, fade)), lambda fh=fh: (SDTest.Call(0x1cec60, 'tbiii', fh, 0, 0)[0], fxb(fh + 0x10)))

# EMI: Start set the 8 trails; IsAlive is 0 without entity
fxsys, d, fx = fxobjs[35]
if fx and fxi(fx) == 0x400000 + 0x3cbe2c:
	check('FXEMI started', lambda fx=fx: (fxi(fx + 8), fxi(fx + 0xb8) != 0, fxints(fx + 0xd4, 8), fxints(fx + 0xc54, 16),
		fxi(fx + 0xd18), SDTest.Call(0x21e1b0, 'tii', fx + 0xf4)[0], SDTest.Call(0x21e1b0, 'tii', fx + 0x110)[0]))
	check('FXEMI_SetEntity', lambda fx=fx: (SDTest.Call(0x1ceed0, 'tvii', fx, 0), fxi(fx + 4)))
	check('FXEMI_IsAlive', lambda fx=fx, d=d, fxsys=fxsys: SDTest.Call(0x1cec40, 'tbiii', fx, d, fxsys)[0])
	fxpoke(fx + 0xbc, 5)
	fxpoke(fx + 0xd18, 7)
	check('FXEMI_Reset', lambda fx=fx, d=d: (SDTest.Call(0x1cedb0, 'tvii', fx, d), fxi(fx + 4), fxi(fx + 0xbc),
		fxi(fx + 0xc4), fxints(fx + 0xd4, 8), fxints(fx + 0xc54, 8), fxi(fx + 0xd18)))

# debris cloud: Start and Reset fill the random numbers
fxsys, d, fx = fxobjs[42]
if fx and fxi(fx) == 0x400000 + 0x3cbe5c:
	check('FXDebrisCloud started', lambda fx=fx: (fxi(fx + 8), fxi(fx + 0xb8) != 0, fxints(fx + 0xd0, 0x48)))
	check('FXDebrisCloud_SetEntity', lambda fx=fx: (SDTest.Call(0x1ceec0, 'tvii', fx, 0), fxi(fx + 4)))
	fxsrand(99)
	check('FXDebrisCloud_Reset', lambda fx=fx, d=d: (SDTest.Call(0x1ced40, 'tvii', fx, d), fxi(fx + 4), fxi(fx + 0xbc),
		fxi(fx + 0xc0), fxints(fx + 0xd0, 0x48)))

# sonic explosion: Explode fills the rings and the particle parameters, Update grows the rings
fxsys, d, fx = fxobjs[50]
if fx and fxi(fx) == 0x400000 + 0x3cbe44:
	check('FXSonic started', lambda fx=fx: (fxi(fx + 0xb0) != 0, fxi(fx + 0x160) != 0))
	for x, y, z, seed in ((0.0, 0.0, 0.0, 77), (1500.5, -20.25, 333.0, 78)):
		fxsrand(seed)
		check('FXSonic_Explode' + fmt((x, y, z)), lambda fx=fx, x=x, y=y, z=z: (SDTest.Call(0x1cef10, 'tvifff', fx, x,
			y, z), fxints(fx + 0x16c, 44), fxpart(fx + 4)))
	fxtimer = fxworld + 0x2c8 + 0xc
	fxsavedt = fxi(fxtimer)
	fxdudv = SDTest.Peek(0x4a8b34, 'i')
	nrings = fxi(fxi(fx + 0x160) + 0xc)
	check('FXSonic rings', lambda nrings=nrings: (nrings >= 8, fxdudv))
	fxset(fxtimer, (0.05,))
	SDTest.Poke(0x4a8b34, 0)
	check('FXSonic_Update dudv off', lambda fx=fx: (SDTest.Call(0x1ce230, 'tviif', fx, 0, 0.0), fxints(fx + 0x164, 2),
		fxints(fx + 0x17c, 8), fxints(fx + 0x1bc, 8)))
	if nrings >= 8:
		SDTest.Poke(0x4a8b34, 1)
		for dt in (0.05, 0.25, 1.5):
			fxset(fxtimer, (dt,))
			check('FXSonic_Update' + fmt((dt,)), lambda fx=fx: (SDTest.Call(0x1ce230, 'tviif', fx, 0, 0.0),
				fxints(fx + 0x164, 2), fxints(fx + 0x17c, 8), fxints(fx + 0x1bc, 8), fxints(fx + 0x1fc, 8)))
	SDTest.Poke(0x4a8b34, fxdudv)
	fxpoke(fxtimer, fxsavedt)
	check('FXSonic_Reset', lambda fx=fx, d=d: (SDTest.Call(0x1ceea0, 'tvii', fx, d), fxints(fx + 0x164, 2)))

# ---- the map elements, made from their models (pos and rot in fxbuf) ----
fxset(fxbuf, (100.0, 50.0, -20.0, 0.0, 0.5, 0.0))
fxrot = fxbuf + 12
fxmaxdist = 0x4671f8 + 0x400000
savedmax = fxi(fxmaxdist)

mc = fxz(0x40)
check('cFXElementsMentalController_ctor', lambda mc=mc: (SDTest.Call(0x1d6240, 'tiisii', mc, 'MentalController1', fxbuf,
	fxrot)[0] == mc, fxi(mc) == 0x400000 + 0x3cc7f4, fxi(mc + 4) != 0))
check('cFXElementsMentalController_Init', lambda mc=mc: (SDTest.Call(0x1d66c0, 'tbi', mc)[0], fxi(mc + 0x2c),
	fxints(mc + 0x30, 3)))
for maxd in (1e30, 0.0):
	fxset(fxmaxdist, (maxd,))
	check('cFXElementsMentalController_Update' + fmt((maxd,)), lambda mc=mc: (SDTest.Call(0x1d7c30, 'tvif', mc, 0.0),
		fxb(mc + 0x3c)))
fxpoke(fxmaxdist, savedmax)

dfx = fxz(0x3f8)
check('cFXElementsMentalControllerDamageFX_ctor', lambda dfx=dfx: (SDTest.Call(0x1d6270, 'tiisii', dfx,
	'MentalController0', fxbuf, fxrot)[0] == dfx, fxi(dfx) == 0x400000 + 0x3cc804))
fxsrand(4321)
check('cFXElementsMentalControllerDamageFX_Init', lambda dfx=dfx: (SDTest.Call(0x1d6750, 'tbi', dfx)[0],
	fxi(dfx + 0x2c), fxints(dfx + 0x30, 3), fxi(dfx + 0xec) != 0, fxi(dfx + 0x310) != 0,
	fxints(dfx + 0xf0, (0x264 - 0xf0) / 4), fxints(dfx + 0x314, (0x3f8 - 0x314) / 4)))
check('cFXElementsMentalControllerDamageFX_ReleaseParticles', lambda dfx=dfx: (SDTest.Call(0x1d6630, 'tvi', dfx),
	fxi(dfx + 0xec), fxi(dfx + 0x310)))
check('cFXElementsMentalControllerDamageFX_Delete', lambda dfx=dfx: SDTest.Call(0x1d6560, 'tiii', dfx, 0)[0] == dfx)

mfx = fxz(0x4278)
check('cFXElementsMentalControllerMainFX_ctor', lambda mfx=mfx: (SDTest.Call(0x1d62e0, 'tiisii', mfx,
	'MentalControllerMainFX', fxbuf, fxrot)[0] == mfx, fxi(mfx) == 0x400000 + 0x3cc814, fxints(mfx + 0x50, 9),
	fxints(mfx + 0x4270, 2)))
fxsrand(555)
check('cFXElementsMentalControllerMainFX_Init', lambda mfx=mfx: (SDTest.Call(0x1d6d90, 'tbi', mfx)[0],
	fxints(mfx + 0x2c, 9), fxi(mfx + 0x3d40) != 0, fxi(mfx + 0x3f10) != 0, fxints(mfx + 0x2ac4, 225),
	fxints(mfx + 0x3550, 450), fxints(mfx + 0x3f14, 63), fxints(mfx + 0x4180, 60),
	SDTest.Call(0x21e1b0, 'tii', mfx + 0x3c58)[0], SDTest.Call(0x21e1b0, 'tii', mfx + 0x3d80 - 4)[0],
	SDTest.Call(0x21e1b0, 'tii', mfx + 0x417c)[0]))
for ph in (1, 2, 3, 4, 5):
	check('cFXElementsMentalControllerMainFX_SetPhase' + fmt((ph,)), lambda mfx=mfx, ph=ph: (SDTest.Call(0x1d7ae0,
		'tvii', mfx, ph), fxi(mfx + 0x40), fxints(mfx + 0x74, 8)))
for idx in (0, 4, 8):
	check('cFXElementsMentalControllerMainFX_SetController' + fmt((idx,)), lambda mfx=mfx, idx=idx, mc=mc:
		(SDTest.Call(0x1d7a70, 'tviii', mfx, idx, mc), fxi(mfx + 0x50 + 4 * idx) == mc))
check('cFXElementsMentalControllerMainFX_Clear', lambda mfx=mfx: SDTest.Call(0x1d6660, 'tvi', mfx))
check('cFXElementsMentalControllerMainFX_ReleaseFluid', lambda mfx=mfx: (SDTest.Call(0x1d6650, 'tvi', mfx),
	fxi(mfx + 0x3d40)))
check('cFXElementsMentalControllerMainFX_Delete', lambda mfx=mfx: SDTest.Call(0x1d65f0, 'tiii', mfx, 0)[0] == mfx)
check('cFXElementsMentalController_Delete', lambda mc=mc: SDTest.Call(0x1d6530, 'tiii', mc, 0)[0] == mc)

# the orbit buoys: a buoy made like the loader does (element, vtable, particles), the controller by its constructor
boya = fxz(0xf0)
SDTest.Call(0x202ca0, 'tiisii', boya, 'OrbitBoya0', fxbuf, fxrot)
fxpoke(boya, 0x400000 + 0x3cc574)
SDTest.Call(0x21d1b0, 'tii', boya + 0x40)
check('cFXElementsOrbitBoya_Init', lambda boya=boya: (SDTest.Call(0x1d59e0, 'tbi', boya)[0], fxi(boya + 0x2c),
	fxints(boya + 0x30, 3), fxi(boya + 0xec) != 0))
check('cFXElementsOrbitBoya_Update', lambda boya=boya: SDTest.Call(0x1d5ee0, 'tvif', boya, 0.0))
oc = fxz(0x1ac)
SDTest.Call(0x1d5820, 'tiisii', oc, 'OrbitBoyaController', fxbuf, fxrot)
check('cFXElementsOrbitBoyaController_Init', lambda oc=oc: SDTest.Call(0x1d5aa0, 'tbi', oc)[0])
for t, c, n, b in ((0, 0, 0, boya), (1, 3, 7, boya), (1, 1, 2, 0), (0, 2, 5, boya)):
	check('cFXElementsOrbitBoyaController_SetBoya' + fmt((t, c, n, b != 0)), lambda oc=oc, t=t, c=c, n=n, b=b:
		(SDTest.Call(0x1d5e60, 'tviiiii', oc, t, c, n, b), fxi(oc + 0x2c + 4 * (c * 8 + n)) == b,
		fxi(oc + 0xac + 4 * (c * 8 + n))))
for dt in (0.0, 0.1, 0.7, 1.3):
	check('cFXElementsOrbitBoyaController_Update' + fmt((dt,)), lambda oc=oc, dt=dt: (SDTest.Call(0x1d5f00, 'tvif', oc,
		dt), fxints(oc + 0x12c, 32)))
elhead = fxi(fxworld + 0x1bb4)
check('FXElementsOrbit_Create', lambda elhead=elhead: (SDTest.Call(0x1d5ab0, 'v'), fxi(fxworld + 0x1bb4) == elhead))
