# engine leftovers 0x200000..0x240000 in the level FZ (execfile'd by sdlevel.py, Python 1.5.2): the explosion of the
# Inferno on its real debris (srand before, the rays against the floor of FZ, the particle parameters), debris
# systems with a model dropped and moved by cDebrisSys_Update with the collisions, the debug text of a pool, the floors
# of the 2D map made again from the dummies, a map element with its model in a list of its own (Find, Update, Call of
# a script function), the frames of the tab sheet of the scorer, the model preloads. Pointers are only compared.
def e4i(a):
	return SDTest.Peek(a, '@i')
def e4b(a):
	return SDTest.Peek(a, '@b')
def e4poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def e4z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def e4new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def e4free(p):
	SDTest.Call('Mem_Free', 'vi', p)
def e4ints(a, n):
	r = []
	for k in range(n):
		r.append(e4i(a + 4 * k))
	return tuple(r)
def e4fl(a, n):
	r = ()
	k = 0
	while k < n:
		m = min(64, n - k)
		r = r + SDTest.Call('crt_memcpy', 'iFii', (0.0,) * m, a + 4 * k, 4 * m)[1]
		k = k + m
	return r
def e4set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, tuple(v), 4 * len(v))
def e4srand(s):
	SDTest.Call('crt_srand', 'vi', s)
e4world = SDTest.Peek('g_pWorld', 'i')
e4pp = Scrap.UsrEntity(0).Pos

# ---- the Inferno explosion on its debris ----
e4srand(4321)
Scrap.SetDebrisSys('Models/Weapons/Inferno/Explosion.M3D', 1, 0.0, 0.0, 1.0, 0.0, 0, 44)
e4xs = SDTest.Call('cWorld_GetDebrisSys', 'tiis', e4world, 'Models/Weapons/Inferno/Explosion.M3D')[0]
e4xd = e4i(e4xs)
e4xfx = 0
if e4xd:
	e4xfx = e4i(e4xd + 0x54)
check('e4 inferno debris', lambda: (e4xs != 0, e4xd != 0, e4xfx != 0, e4xfx and e4i(e4xfx) - 0x400000))
# the parameters of the particles without the pointers and padding
def e4part(p):
	return (e4ints(p, 2), e4fl(p + 8, 4), e4i(p + 0x18), e4fl(p + 0x1c, 28), e4i(p + 0x98) & 0xffff)
if e4xfx and e4i(e4xfx) == symva('cObjElementFXInfernoExplosion_vftable'):
	for x, y, z, rng, bees in ((e4pp[0], e4pp[1] + 30.0, e4pp[2], 3000.0, 0), (e4pp[0], e4pp[1] + 500.0, e4pp[2], 100.0, 3),
			(e4pp[0] + 50.0, e4pp[1] + 5.0, e4pp[2] - 40.0, 800.0, 1)):
		e4srand(777)
		check('e4 InfernoExplosion_Explode' + fmt((rng, bees)), lambda x=x, y=y, z=z, rng=rng, bees=bees: (SDTest.Call(
			'cObjElementFXInfernoExplosion_Explode', 'tviffffii', e4xfx, x, y, z, rng, bees, 0), e4fl(e4xfx + 0x16c, 4), e4i(e4xfx + 0x17c),
			e4b(e4xfx + 0x180), e4i(e4xfx + 0x24c0)))
		e4n = e4i(e4xfx + 0x24c0)
		for k in range(0, min(e4n, 224), 37):
			check('e4 InfernoExplosion ray' + fmt((rng, bees, k)), lambda k=k: (e4fl(e4xfx + 0x184 + 12 * k, 3),
				e4fl(e4xfx + 0xc04 + 12 * k, 3), e4fl(e4xfx + 0x1684 + 4 * k, 1), e4fl(e4xfx + 0x1a04 + 4 * k, 1),
				e4fl(e4xfx + 0x1d84 + 4 * k, 1), e4fl(e4xfx + 0x2104 + 4 * k, 1)))
		check('e4 InfernoExplosion particles' + fmt((rng, bees)), lambda: e4part(e4xfx + 4))
		check('e4 InfernoExplosion rand after' + fmt((rng, bees)), lambda: SDTest.Call('crt_rand', 'i')[0])
	SDTest.Call('cObjElementFXInfernoExplosion_Reset', 'tvii', e4xfx, e4xd)

# ---- a debris system with a model, dropped and moved ----
# a model no other test makes a debris system of (the name is the key of the system)
e4dn = 'Models/Elements/FireAFXFireDrop/FireAFXFireDrop.m3d'
e4srand(99)
Scrap.SetDebrisSys(e4dn, 3, 100000.0, 0.5, 1.0, 9.8, 1, 0)
e4ds = SDTest.Call('cWorld_GetDebrisSys', 'tiis', e4world, e4dn)[0]
check('e4 debris sys', lambda: (e4ds != 0, e4i(e4ds) != 0, e4i(e4ds + 4), e4i(e4ds + 8), e4fl(e4ds + 0x10, 5),
	e4b(e4ds + 0x24), e4ints(e4ds + 0x28, 3)))
# the debris of the pool: the model is the one of the system, no effect
def e4deb(d):
	return (e4fl(d + 0x1c, 13), e4i(d + 0x54), SDTest.Call('CGameModel_GetPos', 'tiiF', d, (0.0, 0.0, 0.0))[1],
		SDTest.Call('CGameModel_GetRot', 'tiiF', d, (0.0, 0.0, 0.0))[1])
def e4active(s):
	r = []
	d = e4i(s + 4)
	while d and len(r) < 8:
		r.append(e4deb(d))
		d = e4i(d + 0x50)
	return tuple(r)
if e4ds:
	for v in ((0, e4pp[0], e4pp[1] + 40.0, e4pp[2]), (1, 10.0, 20.0, 30.0), (2, 100.0, 50.0, -30.0), (3, 90.0, 0.0, 45.0)):
		apply(Scrap.SetDebrisValue, v)
	check('e4 DropDebris', lambda: (Scrap.DropDebris(e4dn, 2.0), e4i(e4ds + 8), e4active(e4ds)))
	for v in ((0, e4pp[0] + 5.0, e4pp[1] + 2.0, e4pp[2] - 3.0), (2, 0.0, -400.0, 0.0), (3, 0.0, 0.0, 0.0)):
		apply(Scrap.SetDebrisValue, v)
	check('e4 DropDebris 2', lambda: (Scrap.DropDebris(e4dn, 0.12), e4i(e4ds + 8), e4active(e4ds)))
	for dt in (0.03, 0.03, 0.06, 0.2, 0.0, 0.05, 0.09, 0.2, 0.2):
		check('e4 cDebrisSys_Update' + fmt((dt,)), lambda dt=dt: (SDTest.Call('cDebrisSys_Update', 'tvif', e4ds, dt), e4fl(e4ds + 0x10, 1),
			e4i(e4ds + 8), e4active(e4ds)))
	# the debug text of the pool (O_Show); the font checks the viewport size (put back below)
	e4oshow = SDTest.Peek('g_OShow', 'i')
	e4vp = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
	SDTest.Poke('g_ViewportW', 640)
	SDTest.Poke('g_ViewportH', 480)
	SDTest.Poke('g_OShow', 1)
	check('e4 cDebrisSys_ShowInfo', lambda: SDTest.Call('cDebrisSys_ShowInfo', 'tviiiF', e4ds, 10, 20, (0.0,))[1:])
	SDTest.Poke('g_OShow', 0)
	check('e4 cDebrisSys_ShowInfo off', lambda: SDTest.Call('cDebrisSys_ShowInfo', 'tviiiF', e4ds, 10, 20, (5.0,))[1:])
	SDTest.Poke('g_OShow', e4oshow)
	SDTest.Poke('g_ViewportW', e4vp[0])
	SDTest.Poke('g_ViewportH', e4vp[1])
	# a debris dropped far from the cameras goes back into the pool at once
	e4set(e4ds + 0x14, (1.0,))
	apply(Scrap.SetDebrisValue, (0, e4pp[0] + 5000.0, e4pp[1], e4pp[2]))
	check('e4 DropDebris far', lambda: (Scrap.DropDebris(e4dn, 1.0), e4i(e4ds + 8), len(e4active(e4ds))))
	check('e4 cDebrisSys_SetMaxIdle', lambda: (SDTest.Call('cDebrisSys_SetMaxIdle', 'tvii', e4ds, 2), e4i(e4ds), e4i(e4ds + 8),
		e4i(e4ds + 0x2c)))
	check('e4 cDebrisSys_NewElement', lambda: (SDTest.Call('cDebrisSys_NewElement', 'tii', e4ds)[0] != 0, e4i(e4ds + 8)))
Scrap.SetDebrisSys(e4dn, 0)

# ---- the floors of the 2D map from the dummies of FZ ----
e4fl0 = e4z(0x18c4)
check('e4 Floor_ctor', lambda: (SDTest.Call('Floor_ctor', 'tii', e4fl0)[0] == e4fl0, e4i(e4fl0)))
check('e4 Floor levels', lambda: (e4fl(e4fl0 + 4, 1), e4ints(e4fl0 + 8, 1), e4ints(e4fl0 + 4 + 0x18c, 3)))
check('e4 world floors', lambda: (e4i(e4world + 0x2f0), e4fl(e4world + 0x2f4, 1)))
for y in (e4pp[1], -1000.0, 1000.0):
	check('e4 Floor_Get world' + fmt((y,)), lambda y=y: SDTest.Call('Floor_Get', 'tiif', e4world + 0x2f0, y)[0])
check('e4 Floor_Dist world', lambda: SDTest.Call('Floor_Dist', 'tfiffffff', e4world + 0x2f0, e4pp[0], e4pp[1], e4pp[2], 0.0,
	0.0, 0.0)[0])
e4free(e4fl0)

# ---- a map element with its model in a list of its own ----
e4el = e4z(4)
e4me = e4new(0x2c)
e4pr = e4z(0x18)
e4set(e4pr, (e4pp[0] + 10.0, e4pp[1], e4pp[2] + 10.0, 0.25, 1.0, -0.5))
check('e4 cMapElement_ctor', lambda: (SDTest.Call('cMapElement_ctor', 'tiisii', e4me, 'FireAFXFireDrop', e4pr, e4pr + 12)[0] == e4me,
	SDTest.Peek(e4me + 0x24, '@p'), e4i(e4me + 0x20), e4i(e4me + 0x28), e4i(e4me + 4) != 0))
check('e4 cMapElement pos', lambda: (SDTest.Call('CGameModel_GetPos', 'tiiF', e4me + 4, (0.0, 0.0, 0.0))[1],
	SDTest.Call('CGameModel_GetRot', 'tiiF', e4me + 4, (0.0, 0.0, 0.0))[1]))
e4poke(e4el, e4me)
for nm, d in (('FireAFXFireDrop', 1.0), ('FireAFXFireDrop', 0.0), ('Other', 100.0), ('FireAFXFireDro', 100.0)):
	check('e4 Elements_Find' + fmt((nm, d)), lambda nm=nm, d=d: SDTest.Call('Elements_Find', 'tiisffff', e4el, nm, e4pp[0] + 10.0,
		e4pp[1] + 0.5, e4pp[2] + 10.0, d)[0] == e4me)
check('e4 cMapElement_Update', lambda: (SDTest.Call('cMapElement_Update', 'tvif', e4me, 0.1), SDTest.Call('CGameModel_GetPos', 'tiiF', e4me + 4,
	(0.0, 0.0, 0.0))[1]))
check('e4 Elements_Update', lambda: (SDTest.Call('Elements_Update', 'tvif', e4el, 0.05), SDTest.Call('CGameModel_GetPos', 'tiiF', e4me + 4,
	(0.0, 0.0, 0.0))[1]))
e4calls = []
def e4cb(name, x, y, z, ax, ay):
	e4calls.append((name, x, y, z, ax, ay))
check('e4 Elements_Call', lambda: (SDTest.Call('Elements_Call', 'tvis', e4el, '__main__.e4cb'), tuple(e4calls)))
check('e4 cMapElement_Delete', lambda: (SDTest.Call('cMapElement_Delete', 'tiii', e4me, 0)[0] == e4me, e4i(e4me) - 0x400000))
SDTest.Call('operator_delete_sized', 'vii', e4me, 0x2c)
e4poke(e4el, 0)
e4free(e4el)
e4free(e4pr)

# ---- the frames of the tab sheet of the scorer ----
e4tab = SDTest.Peek('g_ScorerTabs', 'i')
if e4tab:
	e4nf = SDTest.Call('CPsaImage_GetNumFrames', 'tii', e4tab)[0]
	check('e4 CPsaImage_GetNumFrames', lambda: e4nf)
	for k in range(min(e4nf, 6)):
		check('e4 CPsaImage_GetFrameSize' + fmt((k,)), lambda k=k: SDTest.Call('CPsaImage_GetFrameSize', 'tbiipp', e4tab, k, 0, 0))
		check('e4 CPsaImage_GetFrameUV' + fmt((k,)), lambda k=k: SDTest.Call('CPsaImage_GetFrameUV', 'tbiiFF', e4tab, k, (9.0, 9.0),
			(9.0, 9.0)))

# ---- the preloads ----
check('e4 Object3D_Preload', lambda: SDTest.Call('Object3D_Preload', 'vsF', 'Models/Weapons/Swarm/Swarm.M3D', (1.0, 1.0, 1.0))[0:0])
check('e4 Anm_Preload', lambda: SDTest.Call('Anm_Preload', 'vss', 'Models/Elements/FireAFXFireDrop/FireAFXFireDrop.m3d',
	'Models/Elements/FireAFXFireDrop/Anm/Loop.m3d')[0:0])

# ---- Motor.cpp ----
check('e4 World_GetChecked', lambda: SDTest.Call('World_GetChecked', 'i')[0] == SDTest.Peek('g_World', 'i'))
# the commands of the debug console of the engine (the output goes to the debug console, off)
e4vu = SDTest.PeekSpan('g_ShowJoins', 0, 4)
for c in ('listar luces', 'LISTAR', 'mem', 'ver uniones', 'ver uniones', 'nada', 'arbo', 'imbecil', 'Idiota', 'capullo'):
	e4srand(55)
	check('e4 E3D_Command' + fmt((c,)), lambda c=c: (SDTest.Call('E3D_Command', 'vs', c), SDTest.PeekSpan('g_ShowJoins', 0, 4) & 0xff,
		SDTest.Call('crt_rand', 'i')[0]))
SDTest.PokeSpan('g_ShowJoins', 0, e4vu, 4)
