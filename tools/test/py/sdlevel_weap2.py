# game Entities/Vehicle/Weaps/Inferno.c, Laser.c, Swarm.c (execfile'd by sdlevel.py, Python 1.5.2): the weapon
# natives called by their registry names through Scrap.SetCallFunc('!name') with the parameters of the scripts.
# On the ship Parked only what does not depend on its weapon slots, levels, target or position (they change between
# runs, also in the exe alone): the aim mode setters, the laser shot delay and the HUD items with ammo, timers and
# the scorer time poked (all restored); a Cannon item of the scorer of player 0 for the HUD natives. Two missiles
# made by InfernoCreate / SwarmCreate far above the player, set up, slowed, exploded (nothing in range) and deleted.
# rand is seeded before the effects. Natives with a missing ship that look it up silently. Not called: the fire
# natives (slots and ammo of the ship), the Hook pull (positions), the Laser hit (the camera decides the effect).
def w2i(a):
	return SDTest.Peek(a, '@i')
def w2b(a):
	return SDTest.Peek(a, '@b')
def w2poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def w2set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def w2fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def w2str(a, s):
	SDTest.Call('Cad_Copy', 'iisi', a, s, len(s) + 1)
def w2ints(a, offs):
	r = ()
	for o in offs:
		r = r + (w2i(a + o),)
	return r
def w2srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def w2native(name, params):
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
def w2find(name):
	slot = SDTest.Call('HashTable_Slot', 'tiis', w2world + 4, name)[0]
	node = SDTest.Call('PtrTable_Find', 'tiisi', w2world + 4, name, slot)[0]
	if node:
		return w2i(node)
	return 0
def w2istype(e, t):
	return SDTest.Call(w2i(w2i(e) + 0x14) - 0x400000, 'tbis', e, t)[0]
def w2vec(e, slot):
	return SDTest.Call(w2i(w2i(e) + slot) - 0x400000, 'tiiF', e, (0.0, 0.0, 0.0))[1]

w2world = SDTest.Peek('g_pWorld', 'i')
w2parked = w2find('Parked')
w2car = w2parked and w2istype(w2parked, 'Car')
check('W2 Parked', lambda: (w2parked != 0, w2car))

# ---- the aim mode setters: the mode, the angle and its tangent, the cone and the distance of the aim ----
if w2car:
	p = w2parked
	offs = (0x718, 0x6d4, 0x6d8, 0x6f4, 0x6f8)
	saved = w2ints(p, offs)
	for nat in ('InfernoPress', 'InfernoRelease', 'InfernoOnEnd', 'InfernoOnStart', 'SwarmPress', 'SwarmRelease',
			'SwarmOnEnd', 'SwarmOnStart', 'LaserPress', 'LaserRelease'):
		for i in range(len(offs)):
			w2poke(p + offs[i], 7 + i)
		check('W2 ' + nat, lambda nat=nat, p=p, offs=offs: (w2native(nat, ('Parked',)), w2ints(p, offs)))
	for i in range(len(offs)):
		w2poke(p + offs[i], saved[i])

# ---- Vehicle_IsLocalPlayer: the player slot at +0x410 in 0..3 ----
w2fake = SDTest.Call('Mem_Calloc', 'ii', 0x420)[0]
for v in (-1, 0, 1, 3, 4, 100):
	w2poke(w2fake + 0x410, v)
	check('W2 Vehicle_IsLocalPlayer(%d)' % v, lambda: SDTest.Call('Vehicle_IsLocalPlayer', 'tbi', w2fake)[0])
SDTest.Call('Mem_Free', 'ii', w2fake)
if w2car:
	check('W2 Vehicle_IsLocalPlayer(Parked)', lambda: SDTest.Call('Vehicle_IsLocalPlayer', 'tbi', w2parked)[0])

# ---- Laser_Shoot: the shot delay of the weapon block with the laser ammo poked ----
w2laserammo = SDTest.Peek('g_LaserIDAmmo', 'i')
w2ammoarr = 0
if w2car:
	w2ammoarr = w2i(w2parked + 0x5f0 + 0x20)
if w2ammoarr and w2laserammo >= 0 and w2laserammo < w2i(w2world + 0x1bc4):
	p = w2parked
	a = w2ammoarr + 4 * w2laserammo
	savedammo = w2i(a)
	savedtime = w2i(p + 0x600)
	for ammo, t in ((50.0, 0.0), (15.0, 1.0), (25.0, -0.1), (5.0, 0.5), (0.0, -2.0)):
		w2set(a, (ammo,))
		w2set(p + 0x600, (t,))
		check('W2 Laser_Shoot' + fmt((ammo, t)), lambda p=p: (SDTest.Call('Laser_Shoot', 'bi', p)[0], w2fl(p + 0x600, 1)))
	w2poke(p + 0x600, savedtime)

	# ---- the HUD natives on a Cannon item of player 0 ----
	sc = SDTest.Call('Scorer_Get', 'ii', 0)[0]
	item = 0
	if sc:
		SScorer.New(0, 'SDW2Cannon', 'Cannon')
		item = SDTest.Call('cScorer_FindItem', 'tiis', sc, 'SDW2Cannon')[0]
	check('W2 HUD item', lambda sc=sc, item=item: (sc != 0, item != 0))
	if item:
		hud = (0xb4, 0xb8, 0xbc, 0xc0)
		savedsc = w2ints(sc, (0x20, 0x28))
		for ammo, t in ((50.0, 0.1), (50.0, 0.37), (5.0, 0.1), (5.0, 0.37), (12.0, 1.9)):
			w2set(a, (ammo,))
			w2set(sc + 0x20, (t,))
			check('W2 LaserFX' + fmt((ammo, t)), lambda item=item, hud=hud: (w2native('LaserFX', ('Parked', 0,
				'SDW2Cannon')), w2ints(item, hud)))
		# the target the ship shows stays (copied in before), the time over 0: the cursor turns and shrinks
		target = SDTest.Call('Nto1Node_TargetName', 'si', p + 0x6e4)[0]
		for nat, names, times in (('InfernoFX', symva('g_InfernoHudTarget'), symva('g_InfernoHudTime')), ('SwarmFX', symva('g_SwarmHudTarget'), symva('g_SwarmHudTime'))):
			savedname = SDTest.Peek(names, '@s')
			savedt = w2i(times)
			w2str(names, target)
			for t, dt in ((0.5, 0.04), (1.0, 0.02), (0.01, 0.04), (0.2, 0.0)):
				w2set(times, (t,))
				w2set(sc + 0x28, (dt,))
				check('W2 ' + nat + fmt((t, dt)), lambda nat=nat, item=item, hud=hud, times=times: (w2native(nat,
					('Parked', 0, 'SDW2Cannon')), w2ints(item, hud), w2fl(times, 1)))
			w2str(names, savedname)
			w2poke(times, savedt)
		w2poke(sc + 0x20, savedsc[0])
		w2poke(sc + 0x28, savedsc[1])
	w2poke(a, savedammo)

# ---- natives with a ship that is not there: they look it up and do nothing ----
for nat, params in (('HookActivate', ('SDW2None',)), ('HookDeActivate', ('SDW2None',)), ('HookFire', ('SDW2None',)),
		('HookPlugFX', ('SDW2None', 1)), ('SonicFire', ('SDW2None',))):
	check('W2 ' + nat + ' missing', lambda nat=nat, params=params: w2native(nat, params))
check('W2 InfernoXplosionCall missing', lambda: (w2native('InfernoXplosionCall', ('SDW2None',)),
	SDTest.Peek(symva('g_InfernoHitName'), '@s')))

# ---- the missiles ----
w2pos = (pp[0], pp[1] + 100000.0, pp[2])
missile = ((0x58c, 'b'), (0x3e8, 'b'), (0x594, 'i'), (0x3f0, 'i'), (0x3f4, 'i'), (0x3f8, 'i'), (0x5a4, 'i'),
	(0x5a8, 'i'), (0x5ac, 'i'), (0x584, 'b'), (0x585, 'b'), (0x588, 'i'), (0x7bd, 'b'), (0x238, 'i'), (0x3fc, 's'),
	(0x47c, 's'))
def w2missile(m):
	r = ()
	for o, k in missile:
		r = r + (SDTest.Peek(m + o, '@' + k),)
	return r
def w2sound(m):
	n = w2i(m + 0x148)
	if n < 1:
		return n
	s = w2i(w2i(m + 0x14c))
	if not s:
		return n
	return (n, w2ints(s, (0x4c, 0x14, 0x18, 0x10)), w2b(s + 0xc))
def w2native_is(m, off, rva):
	return w2i(m + off + 8) - 0x400000 == rva

check('W2 InfernoCreate', lambda: w2native('InfernoCreate', ('SDW2Inf', w2pos[0], w2pos[1], w2pos[2], 0.1, 0.2,
	0.3)))
w2inf = w2find('SDW2Inf')
check('W2 InfernoCreate missile', lambda: (w2inf != 0, w2inf and w2istype(w2inf, 'Missile')))
if w2inf and w2istype(w2inf, 'Missile'):
	m = w2inf
	check('W2 InfernoCreate fields', lambda m=m: (w2missile(m), w2native_is(m, 0x7d4, sym('Inferno_HitNative')), w2vec(m, 0x80)))
	w2poke(m + 0x7b8, 0)
	check('W2 InfernoSetUp', lambda m=m: (w2native('InfernoSetUp', ('SDW2Inf',)), w2missile(m), w2i(m + 0x7b8),
		w2sound(m)))
	for speed in (30000.0, 12000.0, 5000.0):
		w2set(m + 0x3f4, (speed,))
		check('W2 InfernoOnObjSlot' + fmt(speed), lambda m=m: (w2native('InfernoOnObjSlot', ('SDW2Inf', 3)),
			w2fl(m + 0x3f4, 1)))
	w2poke(m + 0x7b8, 0)
	w2srand(1234)
	check('W2 InfernoServerHit', lambda m=m: (w2native('InfernoServerHit', ('SDW2Inf',)), w2fl(symva('g_InfernoPos'), 3),
		w2fl(symva('g_InfernoRange'), 1), w2i(symva('g_InfernoOwner')), w2i(symva('g_InfernoMissile')) == m))
	if w2istype(m, 'WithLife'):
		# a mass, else the push of the explosion is infinite
		w2set(m + 0x230, (100.0,))
		w2srand(4321)
		check('W2 InfernoXplosionCall', lambda m=m: (w2native('InfernoXplosionCall', ('SDW2Inf',)),
			SDTest.Peek(symva('g_InfernoHitName'), '@s'), w2i(m + 0x238), w2vec(m, 0x88)))

check('W2 SwarmCreate', lambda: w2native('SwarmCreate', ('SDW2Sw', w2pos[0] + 500.0, w2pos[1], w2pos[2], -0.2, 1.0,
	0.0)))
w2sw = w2find('SDW2Sw')
check('W2 SwarmCreate missile', lambda: (w2sw != 0, w2sw and w2istype(w2sw, 'Missile')))
if w2sw and w2istype(w2sw, 'Missile'):
	m = w2sw
	check('W2 SwarmCreate fields', lambda m=m: (w2missile(m), w2native_is(m, 0x7c0, sym('Swarm_OnBeeHitNative')),
		w2native_is(m, 0x7d4, sym('Swarm_HitFXNative')), w2vec(m, 0x80)))
	check('W2 SwarmSetUp', lambda m=m: (w2native('SwarmSetUp', ('SDW2Sw',)), w2missile(m), w2i(m + 0x3ec),
		w2sound(m)))
	w2srand(777)
	check('W2 SwarmHit', lambda: w2native('SwarmHit', ('SDW2Sw', '', '')))
	w2srand(778)
	check('W2 SwarmServerHit', lambda: w2native('SwarmServerHit', ('SDW2Sw', '', '')))
	# the bee death effect needs the bee debris of the missile
	if w2i(m + 0x590):
		w2srand(779)
		check('W2 Swarm_BeeDeath', lambda m=m: SDTest.Call('Swarm_BeeDeath', 'vifffi', m, w2pos[0], w2pos[1], w2pos[2], 1))

# ---- Laser_Push: the Swarm missile pushes the Inferno missile away (both far above, masses set) ----
if w2inf and w2sw and w2istype(w2inf, 'WithLife') and w2istype(w2sw, 'WithLife'):
	w2set(w2inf + 0x230, (100.0,))
	for n in (1, 3):
		check('W2 Laser_Push(%d)' % n, lambda n=n: (SDTest.Call('Laser_Push', 'viii', w2sw, w2inf, n), w2vec(w2inf, 0x88)))
if w2inf:
	check('W2 SDW2Inf Del', lambda: Scrap.GetEntity('SDW2Inf').Del())
if w2sw:
	check('W2 SDW2Sw Del', lambda: Scrap.GetEntity('SDW2Sw').Del())

# ---- the Sonic explosion position ----
w2srand(99)
check('W2 SonicXplde', lambda: (w2native('SonicXplde', (w2pos[0], w2pos[1], w2pos[2])), w2fl(symva('g_SonicHitPos'), 3)))
