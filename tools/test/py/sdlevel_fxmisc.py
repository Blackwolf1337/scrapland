# game World/FX/FXMisc.c (execfile'd by sdlevel.py, Python 1.5.2): the small effects in the level FZ. Pure helpers
# (InRange, the random vectors after srand, the visibility of spheres for the camera of the level and a fake one),
# element methods on elements in calloc memory that only touch their fields, the sparks update without live sparks,
# the model fade and the trail elements on a debris made of a loaded CGameModel (with a real CGameTrail), and the
# constructors and destructors of the FX elements. The frame time of the world timer is set to 0.04 and restored.
# Not called: the effects that make debris (ItemBlink, ItemFade, the Sputnik ones), Alive of the elements that step
# the debris (that needs a debris system), Sparks_Emit (31 arguments) and the element methods on entities.
def fxi(a):
	return SDTest.Peek(a, '@i')
def fxb(a):
	return SDTest.Peek(a, '@b')
def fxz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fxfree(p):
	SDTest.Call(0x133550, 'ii', p)
def fxfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fxints(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs)
def fxbytes(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs)
def fxadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
fxworld = SDTest.Peek(0x467450, 'i')
fxp = (pp[0], pp[1], pp[2])
fxdelta = fxi(fxworld + 0x2d4)
fxset(fxworld + 0x2d4, (0.04,))

# pure helpers
for frm, to in ((0.0, 1.0), (1.0, 1.0), (-5.0, 5.0), (2.0, 3.0), (-1e30, 1e30)):
	for x in (-1.0, 0.0, 0.5, 1.0, 2.0, 2.5, 3.0, 1e30, -1e30):
		check('FX_InRange' + fmt((x, frm, to)), lambda x=x, frm=frm, to=to: (SDTest.Call(0x1dc980, 'bfffi', x, frm, to,
			0)[0], SDTest.Call(0x1dc980, 'bfffF', x, frm, to, (-7.0,))))
SDTest.Call(0x323c50, 'vi', 4711)
for lo, hi in (((0.0, 0.0, 0.0), (1.0, 1.0, 1.0)), ((5.0, 5.0, 5.0), (5.0, 5.0, 5.0)), ((-3.0, 2.0, 9.0),
		(3.0, 2.0, -9.0)), ((1e30, -1e30, 0.0), (-1e30, 1e30, 0.0)), ((0.25, -0.0, 7.5), (0.25, 0.0, 8.5))):
	check('FX_RandomVec' + fmt((lo, hi)), lambda lo=lo, hi=hi: SDTest.Call(0x1dca00, 'iFFF', (9.0, 9.0, 9.0), lo, hi)[1])
for i in range(5):
	check('FX_RandomDir %d' % i, lambda: SDTest.Call(0x1dcb40, 'iF', (9.0, 9.0, 9.0))[1])
check('rand after', lambda: SDTest.Call(0x323c2f, 'i')[0])

# the visibility of a sphere: for the camera of the level (0) and a fake camera behind the player
fxcam = fxz(0x40)
fxset(fxcam, fxadd(fxp, (0.0, 100.0, -500.0)) + (0.0, 0.0, 0.0))
fxset(fxcam + 0x2c, (1.0,))
for cam in (0, fxcam):
	for p in (fxp, fxadd(fxp, (0.0, 0.0, 1000.0)), fxadd(fxp, (5000.0, 0.0, 0.0)), (0.0, 0.0, 0.0),
			fxadd(fxp, (0.0, -20000.0, 0.0)), fxadd(fxp, (0.0, 100.0, -2000.0))):
		for r, md in ((0.0, 0.0), (50.0, 0.0), (5000.0, 0.0), (50.0, 1000.0), (50.0, 1e6)):
			check('FX_IsVisible' + fmt((cam != 0, p, r, md)), lambda cam=cam, p=p, r=r, md=md: (SDTest.Call(0x1dc0c0,
				'bfffffii', p[0], p[1], p[2], r, md, cam, 0)[0], SDTest.Call(0x1dc0c0, 'bfffffii', p[0], p[1], p[2], r,
				md, cam, 1)[0]))
fxfree(fxcam)

# element methods that only touch their fields
fxe = fxz(0x1000)
def fxreset(e):
	SDTest.Call(0x3197d0, 'iiFi', e, (0.0,) * 64, 0x100)
for rva, v in ((0x1ddc80, 2.5), (0x1ddca0, -3.25), (0x1ddcc0, 1e9)):
	fxreset(fxe)
	check('FXElem_SetF %x' % rva, lambda rva=rva, v=v: (SDTest.Call(rva, 'tvif', fxe, v), fxfl(fxe, 4)))
fxreset(fxe)
check('FXElem_SetI8', lambda: (SDTest.Call(0x1ddce0, 'tvii', fxe, -77), fxints(fxe, (0, 4, 8, 0xc))))
for rva in (0x1dccb0, 0x1dccc0, 0x1dd1d0, 0x1dd1e0, 0x1dcc20, 0x1dcef0, 0x1dd1f0, 0x1dd200):
	fxset(fxe, (1.0, 2.0, 3.0, 4.0))
	check('FXElem_Clear/Nop %x' % rva, lambda rva=rva: (SDTest.Call(rva, 'tvii', fxe, 0), fxfl(fxe, 4)))
# FadeOutTrail without its trail (on 0): duration 0x8, t 0xc, running 0x10
for run, t, dur in ((1, 5.0, 1.0), (0, 0.5, 1.0), (0, 1.0, 1.0), (0, 2.0, 1.0)):
	fxreset(fxe)
	fxset(fxe + 8, (dur, t))
	SDTest.Poke(fxe + 0x10 - 0x400000, run)
	check('cObjElementFXFadeOutTrail_Alive' + fmt((run, t, dur)), lambda: SDTest.Call(0x1dcf00, 'tbiii', fxe, 0, 0)[0])
check('cObjElementFXFadeOutTrail_Reset', lambda: (SDTest.Call(0x1dd0a0, 'tvii', fxe, 0), fxfl(fxe + 8, 2),
	fxbytes(fxe, (0x10, 0x11))))
# the resets of ItemFade and ItemTake: their particle systems are set to 1 (only checked)
fxreset(fxe)
fxset(fxe + 0xb8, (2.0, 0.15, 1.5, 3.0))
SDTest.Poke(fxe + 0xb8 - 0x400000, 2)
SDTest.Poke(fxe + 0xb4 - 0x400000, 1)
SDTest.Poke(fxe + 0x4 - 0x400000, 0x1234)
SDTest.Poke(fxe + 0xc8 - 0x400000, 0x0101)
check('cObjElementFXItemFade_Reset', lambda: (SDTest.Call(0x1dd0d0, 'tvii', fxe, 0), fxints(fxe, (4, 0xb4, 0xb8, 0xbc,
	0xc0, 0xc4, 0xc8))))
check('cObjElementFXItemFade_Update no entity', lambda: (SDTest.Call(0x1da9d0, 'tviif', fxe, 0, 0.1),
	fxints(fxe, (4, 0xb8, 0xc0, 0xc8))))
fxt = fxz(0x700)
fxset(fxt + 0x630, (0.0, 1.0, 2.5, 0.5, 1.25))
SDTest.Poke(fxt + 0x630 - 0x400000, 1)
SDTest.Poke(fxt + 0xb8 - 0x400000, 1)
SDTest.Poke(fxt + 0x4 - 0x400000, 0x1234)
SDTest.Poke(fxt + 0x644 - 0x400000, 0x0101)
check('cObjElementFXItemTake_Reset', lambda: (SDTest.Call(0x1dd150, 'tvii', fxt, 0), fxints(fxt, (4, 8, 0xb8, 0x630,
	0x634, 0x638, 0x63c, 0x640, 0x644))))
check('cObjElementFXItemTake_Update no entity', lambda: (SDTest.Call(0x1dabd0, 'tviif', fxt, 0, 0.1),
	fxints(fxt, (4, 8, 0x638, 0x63c))))
fxplayer = SDTest.Call(0x0c1430, 'tiii', fxworld, 0)[0]
SDTest.Poke(fxt + 0x4 - 0x400000, fxplayer)
check('cObjElementFXItemTake_Update no target', lambda: (SDTest.Call(0x1dabd0, 'tviif', fxt, 0, 0.1),
	fxi(fxt + 4) == fxplayer, fxints(fxt, (8, 0x638, 0x63c))))
fxfree(fxt)
fxreset(fxe)
fxset(fxe + 8, (1.0, 2.0))
check('cObjElementFXModelNoise_Update no entity', lambda: (SDTest.Call(0x1db900, 'tviif', fxe, 0, 0.1),
	fxfl(fxe + 8, 2)))
fxreset(fxe)
for rva in (0x1db9c0, 0x1db9f0):
	check('cObjElementFXSistPart_Update off %x' % rva, lambda rva=rva: (SDTest.Call(rva, 'tviif', fxe, 0, 0.1),
		fxb(fxe + 0xb4)))
fxfree(fxe)

# the sparks without live sparks: one waits (delay), one has ended (back to the free list), one is off
fxs = fxz(0xd00)
fxset(fxs + 4, (1.0, 0.25))
SDTest.Poke(fxs + 0xa34 - 0x400000, 4)
for i in range(4):
	SDTest.Poke(fxs + 0xa38 + 4 * i - 0x400000, i)
SDTest.Poke(fxs + 0xab8 - 0x400000, 28)
for i in range(28):
	SDTest.Poke(fxs + 0xabc + 4 * i - 0x400000, i + 4)
fxset(fxs + 0x730, (0.5, 0.0, 0.0, 0.0))
SDTest.Poke(fxs + 0x10 - 0x400000, 0x01010100)
check('cObjElementFXSparks_Update', lambda: (SDTest.Call(0x1dba20, 'tviif', fxs, 0, 0.1), fxfl(fxs + 4, 2),
	fxfl(fxs + 0x9b0, 1), fxfl(fxs + 0x730, 4), fxints(fxs, (0xa34, 0xa38, 0xa3c, 0xa40, 0xa44, 0xab8)),
	fxi(fxs + 0xabc + 4 * 28), fxbytes(fxs, (0x10, 0x11, 0x12, 0x13))))
fxset(fxs + 0x9b0, (0.01,))
check('cObjElementFXSparks_Update 2', lambda: (SDTest.Call(0x1dba20, 'tviif', fxs, 0, 0.1), fxfl(fxs + 4, 2),
	fxfl(fxs + 0x9b0, 1), fxfl(fxs + 0x730, 4), fxints(fxs, (0xa34, 0xa38, 0xa3c, 0xa40, 0xab8))))
fxfree(fxs)

# a debris of a loaded model: position, angles, velocity, angle speed and the life
fxd = fxz(0x60)
check('debris model', lambda: SDTest.Call(0x20eb10, 'tiis', fxd,
	'Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D')[0] == fxd)
def fxdebris(life):
	fxset(fxd + 0x1c, (1.0, 2.0, 3.0, 0.1, 0.2, 0.3, 10.0, 20.0, 30.0, 1.0, 1.0, 1.0, life))
def fxmodel():
	return (SDTest.Call(0x210760, 'tiip', fxd, 0)[0], SDTest.Call(0x2107c0, 'tviip', fxd, 0, 0)[1],
		SDTest.Call(0x20fa30, 'tiiF', fxd, (0.0, 0.0, 0.0))[1], SDTest.Call(0x210830, 'tiiF', fxd, (0.0, 0.0, 0.0))[1])
fxf = fxz(0x10)
for t, dur, fo, life, dt in ((0.0, 0.5, 0.3, 2.0, 0.1), (0.45, 0.5, 0.3, 2.0, 0.1), (1.0, 0.5, 0.3, 0.2, 0.05),
		(1.0, 0.5, 0.3, 0.05, 0.1), (1.0, 0.5, 0.3, 1.0, 0.1), (0.0, 0.0, 0.0, 1.0, 0.0)):
	fxset(fxf + 4, (t, dur, fo))
	fxdebris(life)
	check('cObjElementFXModelFade_Update' + fmt((t, dur, fo, life, dt)), lambda dt=dt: (SDTest.Call(0x1db830, 'tviif',
		fxf, fxd, dt), fxfl(fxf + 4, 3), fxmodel()))
check('cObjElementFXModelFade_Update none', lambda: SDTest.Call(0x1db830, 'tviif', fxf, 0, 0.1))
fxfree(fxf)
# the trail element: trail at 0x4, last point 0x8, min distance 0x14, stopped 0x18, on 0x19
fxtr = fxz(0x20)
def fxtrail():
	ts = fxi(fxtr + 4)
	n = fxbytes(ts, (0x20, 0x21, 0x22, 0x23, 0x24, 0x0f))
	return (n, fxfl(fxi(ts + 0x28) + 0x14 * n[4], 3), fxfl(fxtr + 8, 4), fxbytes(fxtr, (0x18, 0x19)))
check('CGameTrail_ctor', lambda: SDTest.Call(0x21ddb0, 'tii', fxtr + 4)[0] == fxtr + 4)
# cObjElementFXTrail_Update and the FadeOutTrail calls read a NULL in this fake trail in both runs: not tested
SDTest.Call(0x21de40, 'tvi', fxtr + 4)
fxfree(fxtr)
SDTest.Call(0x20ec10, 'tvi', fxd)
fxfree(fxd)
SDTest.Poke(fxworld + 0x2d4 - 0x400000, fxdelta)

# the constructors and destructors: name, ctor, dtor, deleting destructor, size, particle systems, trails (offset,
# count), lens flares, the dwords and the bytes the constructor sets
fxclasses = (
	('CharacterConversion', 0x1a7f20, 0x1a8390, 0x1a85d0, 0x89c, (0xc, 0xbc), ((0x17c, 5),), (),
		(4, 8, 0x16c, 0x170), (0x898,)),
	('CharacterConversor', 0x1a7fd0, 0x1a8400, 0x1a8660, 0x298, (0xc,), ((0x110, 3),), (),
		(0xbc, 0xc0, 0xcc, 0xd0, 0x100), (0x294,)),
	('CharacterDeath', 0x1a8080, 0x1a8460, 0x1a86e0, 0xf68, (8, 0xb8, 0x168), ((0x224, 8),), (), (4,), (0xf64,)),
	('CharacterTeleport', 0x1a8120, 0x1a84e0, 0x1a8780, 0xd8, (0xc,), (), (), (0xbc, 0xc0, 0xcc, 0xd0), (0xd4,)),
	('MaintenanceWork', 0x1a81b0, 0x1a8500, 0x1a87c0, 0xe0, (0xc,), ((0xc8, 2),), (),
		(4, 8, 0xbc, 0xc0, 0xd4, 0xd8), (0xdc,)),
	('MoneyTransfer', 0x1a8270, 0x1a8560, 0x1a8840, 0xcc, (0xc,), (), (), (4, 8, 0xbc, 0xc0, 0xc4), (0xc8,)),
	('Sputnik', 0x1a8300, 0x1a8580, 0x1a8880, 0xe0, (8,), (), (), (4, 0xb8, 0xbc, 0xc0, 0xc8), (0xdc,)),
	('SceneBeatPickus', 0x1de230, 0x1de740, 0x1de970, 0x240, (0x1c, 0xd0, 0x184), (), (),
		(4, 8, 0xc, 0xcc, 0x180, 0x234, 0x238), (0x23c,)),
	('SceneBillArrives_A', 0x1de2f0, 0x1de770, 0x1de9c0, 0xec, (0x1c,), (), (0x14,), (4, 8, 0xc, 0x10, 0x18, 0xcc),
		(0xe8,)),
	('SceneBishops', 0x1de390, 0x1de7a0, 0x1dea00, 0x1b8, (0xc, 0xbc), ((0x184, 12),), (),
		(4, 8, 0x16c, 0x174, 0x178, 0x17c), (0x1b4,)),
	('SceneDecontamination', 0x1de450, 0x1de810, 0x1dea90, 0x11ec, (4,), ((0x248, 40),), (),
		(0xb4, 0xb8, 0x100, 0x104), (0x11e8,)),
	('SceneEnterMatrix', 0x1de500, 0x1de870, 0x1deb10, 0x798, (0x10,), ((0x28c, 16),), (0x790,), (4, 8, 0xc), (0x794,)),
	('SceneHumanCD', 0x1de5a0, 0x1de8f0, 0x1debb0, 0x184, (0x10, 0xc0), (), (), (4, 8, 0xc), (0x180,)),
	('SceneTheEndB', 0x1de630, 0x1de920, 0x1dec00, 0xcc, (0x14,), (), (0xc4,), (4, 8, 0xc, 0x10), (0xc8,)),
	('SceneYakuzzi', 0x1de6c0, 0x1de950, 0x1dec50, 0xd4, (0x10,), (), (), (4, 8, 0xc, 0xc0), (0xd0,)))
def fxstate(p, c):
	parts = map(lambda o, p=p: (SDTest.Peek(p + o + 0xa8, '@i'), SDTest.Peek(p + o + 0xac, '@i')), c[5])
	trails = []
	for o, n in c[6]:
		for i in range(n):
			trails.append(SDTest.Peek(p + o + 4 * i, '@i') != 0)
	lens = map(lambda o, p=p: SDTest.Peek(p + o, '@i') != 0, c[7])
	return (fxi(p) - 0x400000, parts, trails, lens, fxints(p, c[8]), fxbytes(p, c[9]))
for c in fxclasses:
	p = fxz(c[4])
	SDTest.Call(0x3197d0, 'iiFi', p, (1.0,) * 4, 16)
	check('%s_ctor' % c[0], lambda p=p, c=c: (SDTest.Call(c[1], 'tii', p)[0] == p, fxstate(p, c)))
	check('%s_dtor' % c[0], lambda p=p, c=c: (SDTest.Call(c[2], 'tvi', p), fxi(p) - 0x400000))
	SDTest.Call(c[1], 'tii', p)
	check('%s_Delete(0)' % c[0], lambda p=p, c=c: (SDTest.Call(c[3], 'tiii', p, 0)[0] == p, fxi(p) - 0x400000))
	fxfree(p)
	p = SDTest.Call(0x1334d0, 'ii', c[4])[0]
	SDTest.Call(c[1], 'tii', p)
	check('%s_Delete(1)' % c[0], lambda p=p, c=c: SDTest.Call(c[3], 'tiii', p, 1)[0] == p)
p = SDTest.Call(0x1334d0, 'ii', 4)[0]
SDTest.Poke(p - 0x400000, 0)
check('cObjElementAbstract_Delete(0)', lambda p=p: (SDTest.Call(0x1a85a0, 'tiii', p, 0)[0] == p, fxi(p) - 0x400000))
check('cObjElementAbstract_Delete(1)', lambda p=p: SDTest.Call(0x1a85a0, 'tiii', p, 1)[0] == p)
