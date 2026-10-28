# game leftovers C (execfile'd by sdlevel.py, Python 1.5.2): the Updates of the character effects in
# Entities/EmbeddedFX/EmbeddedFXChar.c (Sentinel, Crazy Gambler, Messenger, Mercenary, Rusty) and of the Moto in
# EmbeddedFXMisc.c, the native GearShootLoad (Entities/Char/Type/Gear.c) and the two strafe arcs of the 3D control in
# AI/ICharControl3D.c.
# The effects run on a fake character (calloc memory with the vtable of Player0, the CGameModel at 0xb8, life 100, no
# player slot) and a fake action controller (layer 0 on with an action of a given name and time), made by
# EmbeddedFXChar_Create on the model of the class; the model is placed in front of the camera and every model is
# visible (R_SplitScreen) unless a check turns it off. Printed: the fields the Updates write, the particles they place
# and the points of their trails. rand is seeded. The smoke of the Mercenary uses a debris of the system of
# FXMercenaryDrinkSmokeFile with particles (fxType 1, set only on a new system whose model has them). The word of the
# vertex 0x100 in the vertex sets of the gambler arcs, the bolt pauses of Rusty and the last smoke positions of the Moto
# are read before the exe writes them (heap garbage): set after Create. The Moto runs on a fake object (the vtable of
# cObjEntity). GearShootLoad runs through Scrap.SetCallFunc on a WalkChar of our own with the Gear model and its
# embedded effect (SetModel, SetEmbeddedFX).
# The strafes: the setup of sdlevel_icharctl.py (two WalkChars far above the map, a fake AI, a 3D control) with both
# arcs called directly, also with the debug lists of ShowShirkAI and ShowTestCanGoAI on (the segments of the path tests
# of the arc).
def lfci(a):
	return SDTest.Peek(a, '@i')
def lfcb(a):
	return SDTest.Peek(a, '@b')
def lfcz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def lfcfree(p):
	SDTest.Call(0x133550, 'ii', p)
def lfcfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def lfcset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def lfcints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def lfcbytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def lfcpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def lfcpokeb(a, v):
	w = lfci(a & ~3)
	sh = 8 * (a & 3)
	lfcpoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def lfcadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
# a string in calloc memory (ASCII, at most 15 characters)
def lfcstr(s):
	m = lfcz(0x10)
	i = 0
	while i < len(s):
		v = 0
		k = 0
		while k < 4 and i + k < len(s):
			v = v | (ord(s[i + k]) << (8 * k))
			k = k + 1
		lfcpoke(m + i, v)
		i = i + 4
	return m
# a CGameParticles: attached, the parameters (no padding bytes, no pointers)
def lfcparts(p):
	return (lfci(p + 0xac) != 0, lfcints(p, (0x00, 0x04, 0x18)), lfcfl(p + 0x08, 4), lfcfl(p + 0x1c, 28),
		lfcfl(p + 0x9c, 2), lfcbytes(p, (0x98, 0x99, 0xa4)))
def lfcpart(p, i):
	return SDTest.Call(0x21d4e0, 'tbiiFiipF', p, i, (0.0, 0.0, 0.0), 0, 0, 0, (0.0,))
def lfcnum(p):
	return SDTest.Call(0x21d4d0, 'tii', p)[0]
def lfcactive(p):
	return SDTest.Call(0x21d470, 'tii', p)[0]
def lfcpartv(p, i):
	r = lfcpart(p, i)
	if r[0]:
		return r
	return (0,)
def lfcpartlist(p, n):
	m = lfcnum(p)
	if n > m:
		n = m
	return (lfcactive(p), tuple(map(lambda i, p=p: lfcpartv(p, i), range(n))))
# a CGameTrail: the CTrail fields and its points from the newest
def lfctrail(t):
	c = lfci(t)
	if not c:
		return 0
	mx = lfcb(c + 0x20)
	n = lfcb(c + 0x21)
	head = lfcb(c + 0x22)
	pts = []
	for i in range(n):
		j = head - i - 1
		if j < 0:
			j = j + mx
		a = lfci(c + 0x28) + 0x14 * j
		pts.append((lfcfl(a, 3), lfci(a + 0xc), lfcfl(a + 0x10, 1)))
	return (lfcbytes(c, (0, 0x0c, 0x0d, 0x0e, 0x0f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25)), lfci(c + 8),
		lfcints(c, (0x10,)), lfcfl(c + 0x14, 3), tuple(pts))
def lfctrails(a, n):
	return tuple(map(lambda i, a=a: lfctrail(a + 4 * i), range(n)))
# render node name of the CGameModel gm with a particle system (flags & 2, obj3D of class 0xa5000020)
def lfcsist(gm, name):
	i = SDTest.Call(0x20f7d0, 'tiis', gm, name)[0]
	if i < 0:
		return 0
	t = lfci(lfci(gm) + 0xac)
	if i >= lfci(t):
		return 0
	r = lfci(t + 4) + 0xa8 * i
	o = lfci(r + 0x9c)
	return (lfcb(r + 4) & 2) and o != 0 and lfci(o + 4) == -0x5affffe0
def lfcnode(gm, name):
	return SDTest.Call(0x20f7d0, 'tiis', gm, name)[0] >= 0
def lfcok(gm, sists, nodes):
	if not SDTest.Call(0x210da0, 'tbi', gm)[0] or not lfci(gm):
		return 0
	for n in sists:
		if not lfcsist(gm, n):
			return 0
	for n in nodes:
		if not lfcnode(gm, n):
			return 0
	return 1

lfcworld = SDTest.Peek(0x467450, 'i')
lfcplayer = SDTest.Call(0x0c1430, 'tiii', lfcworld, 0)[0]
lfcvt = lfci(lfcplayer)
lfcdelta = lfci(lfcworld + 0x2d4)
lfcset(lfcworld + 0x2d4, (0.04,))
lfcsplit = SDTest.Peek(0x4a8c8c, 'i')
SDTest.Poke(0x4a8c8c, 1)
SDTest.Call(0x323c50, 'vi', 2718)
lfcvec = lfcz(0x20)
lfccam = lfcfl(lfcworld + 0x254 + 12 * lfci(lfcworld + 0x50), 3)

# the fake action controller: layer 0 with the action of name s at time t
def lfcctrl(s, t):
	c = lfcz(0x80)
	a = lfcz(0x40)
	n = lfcstr(s)
	lfcpoke(a + 8, n)
	lfcpoke(c + 0x14, a)
	lfcpoke(c + 0x1c, 1)
	lfcset(c + 0x10, (t,))
	return (c, a, n)
def lfcctrlname(c, s):
	lfcfree(c[2])
	n = lfcstr(s)
	lfcpoke(c[1] + 8, n)
	return (c[0], c[1], n)
def lfcctrlfree(c):
	lfcfree(c[2])
	lfcfree(c[1])
	lfcfree(c[0])
def lfcfake(path, c):
	e = lfcz(0x600)
	lfcpoke(e, lfcvt)
	SDTest.Call(0x20eb10, 'tiis', e + 0xb8, path)
	lfcpoke(e + 0x238, 100)
	lfcpoke(e + 0x424, -1)
	lfcpoke(e + 0x234, c)
	return e
def lfcunfake(e):
	SDTest.Call(0x20ec10, 'tvi', e + 0xb8)
	lfcfree(e)
def lfcplace(gm, d, rot):
	lfcset(lfcvec, lfcadd(lfccam, d))
	SDTest.Call(0x211d20, 'tvii', gm, lfcvec)
	lfcset(lfcvec, rot)
	SDTest.Call(0x211e50, 'tvii', gm, lfcvec)
# the model visible or not: R_SplitScreen and the visible flags of the model
def lfcvisible(gm, on):
	m = lfci(gm)
	if on:
		SDTest.Poke(0x4a8c8c, 1)
		lfcpoke(m + 0x120, lfcvisflags)
	else:
		SDTest.Poke(0x4a8c8c, 0)
		lfcpoke(m + 0x120, lfcvisflags & ~0xff00)
# Create of the effect on the fake; the effect or 0
def lfcmake(name, path, create, sists, nodes, c):
	e = lfcfake(path, c)
	gm = e + 0xb8
	if not SDTest.Call(lfci(lfcvt + 0x14) - 0x400000, 'tbis', e, 'WithLife')[0] or not lfcok(gm, sists, nodes):
		check('LFC %s nodes' % name, lambda: 'skipped')
		lfcunfake(e)
		return (0, 0, 0)
	slot = lfcz(4)
	SDTest.Call(0xd7ac0, 'visi', e, create, slot)
	fx = lfci(slot)
	lfcfree(slot)
	check('LFC %s created' % name, lambda fx=fx, e=e: (fx != 0, fx and lfci(fx) - 0x400000, fx and lfci(fx + 4) == e))
	return (e, gm, fx)
def lfcdone(e, fx, rel, dele):
	if fx:
		SDTest.Call(rel, 'tvi', fx)
		SDTest.Call(dele, 'tiii', fx, 1)
	if e:
		lfcunfake(e)

# ---- Sentinel ----
lfcc = lfcctrl('Walk', 1.0)
lfcs = lfcmake('Sentinel', 'Models/Chars/Sentinel/Sentinel.M3D', 'Sentinel.M3D', ('DF_LaserScaner', 'DF_StatusLight',
	'DF_PoliceAlarm'), (), lfcc[0])
lfcvisflags = 0
if lfcs[2]:
	lfce = lfcs[0]
	lfcfx = lfcs[2]
	lfcvisflags = lfci(lfci(lfcs[1]) + 0x120)
	lfcplace(lfcs[1], (0.0, -60.0, 400.0), (0.0, 0.4, 0.0))
	lfcset(lfce + 0x94, (0.2, 0.3, 0.0))
	lfcset(lfce + 0x3fc, (0.1,))
	def lfcsent(fx):
		return (lfcints(fx, (0x1a8, 0x1ec, 0x1f0)), lfcfl(fx + 0x184, 1), lfcfl(fx + 0x1ac, 16), lfcfl(fx + 0x1dc, 4),
			lfcpartlist(fx + 0xc, 2), lfcpartlist(fx + 0xc0, 1), lfctrails(fx + 0x174, 4), lfctrails(fx + 0x188, 4),
			lfctrails(fx + 0x198, 4), lfcbytes(fx, (0x2a8, 0x2a9)))
	for lfcst, lfcdt in ((1, 0.05), (2, 0.05), (2, 0.01), (3, 0.05), (3, 0.02), (3, 0.03), (0, 0.05), (3, 0.05),
			(2, 0.04), (1, 0.05)):
		lfcpoke(lfce + 0x38c, lfcst)
		check('LFC Sentinel_Update' + fmt((lfcst, lfcdt)), lambda fx=lfcfx, dt=lfcdt: (SDTest.Call(0xdee20, 'tvif',
			fx, dt), lfcsent(fx)))
	# the player path: a player slot and the action Action.. (danger: no victim, so alert), then another action
	lfcpoke(lfce + 0x424, 0)
	lfcc = lfcctrlname(lfcc, 'ActionLoop')
	check('LFC Sentinel_Update player Action', lambda fx=lfcfx: (SDTest.Call(0xdee20, 'tvif', fx, 0.05),
		lfcsent(fx)))
	lfcpoke(lfce + 0x38c, 0)
	lfcc = lfcctrlname(lfcc, 'Relax')
	check('LFC Sentinel_Update player Relax', lambda fx=lfcfx: (SDTest.Call(0xdee20, 'tvif', fx, 0.05),
		lfcsent(fx)))
	lfcpoke(lfce + 0x424, -1)
	# no life
	lfcpoke(lfce + 0x38c, 3)
	lfcpoke(lfce + 0x238, 0)
	check('LFC Sentinel_Update dead', lambda fx=lfcfx: (SDTest.Call(0xdee20, 'tvif', fx, 0.05), lfcsent(fx)))
	lfcpoke(lfce + 0x238, 100)
	# not visible: near (the scanner still sweeps) and far
	lfcvisible(lfcs[1], 0)
	for lfcd in ((0.0, 0.0, 300.0), (0.0, 0.0, 9000.0), (0.0, 200.0, 600.0)):
		lfcplace(lfcs[1], lfcd, (0.0, -1.2, 0.0))
		check('LFC Sentinel_Update hidden' + fmt(lfcd), lambda fx=lfcfx: (SDTest.Call(0xdee20, 'tvif', fx, 0.05),
			lfcsent(fx)))
	lfcvisible(lfcs[1], 1)
	# the alarm of the world, also with a camera
	lfcal = lfcb(lfcworld + 0x1c70)
	lfcpokeb(lfcworld + 0x1c70, 1)
	check('LFC Sentinel_Update alarm', lambda fx=lfcfx: (SDTest.Call(0xdee20, 'tvif', fx, 0.05), lfcsent(fx),
		lfcpartlist(fx + 0x1f8, 12)))
	lfcpoke(lfce + 0xf0, lfcplayer)
	check('LFC Sentinel_Update alarm camera', lambda fx=lfcfx: (SDTest.Call(0xdee20, 'tvif', fx, 0.05),
		lfcbytes(fx, (0x2a8, 0x2a9))))
	lfcpoke(lfce + 0xf0, 0)
	lfcpokeb(lfcworld + 0x1c70, lfcal)
lfcdone(lfcs[0], lfcs[2], 0xd78e0, 0xd7620)

# ---- Crazy Gambler: SetHead in Init runs the Update too ----
lfcc = lfcctrlname(lfcc, 'Relax1')
lfcset(lfcc[0] + 0x10, (2.0,))
lfcs = lfcmake('CrazyGambler', 'Models/Chars/CrazyGambler/CrazyGambler.M3D', 'CrazyGambler.M3D', ('DF_Glow',),
	('R_Gambler_mano_Izq', 'R_Gambler_Cabeza-Pelos'), lfcc[0])
if lfcs[2]:
	lfce = lfcs[0]
	lfcfx = lfcs[2]
	lfcvisflags = lfci(lfci(lfcs[1]) + 0x120)
	lfcplace(lfcs[1], (0.0, -60.0, 400.0), (0.0, 0.7, 0.0))
	# the vertex set word 16 of each arc (vertex 0x100) is never cleared by the exe: zeroed here
	for lfci2 in range(8):
		lfcpoke(lfcfx + 0x1f58 + 0x42 * lfci2 + 0x20, 0)
	def lfcgarc(fx, i):
		tail = lfci(fx + 0x1f18 + 4 * i)
		if tail < 0 or tail > 0x37:
			tail = -1
		pts = ()
		if tail >= 0:
			pts = (lfcints(fx + 0x1778 + 0xf0 * i, tuple(map(lambda k: 4 * k, range(tail + 1)))),
				lfcfl(fx + 0xf8 + 0x2d0 * i, 3 * (tail + 1)))
		return (lfcfl(fx + 0xd8 + 4 * i, 1), lfcints(fx, (0x1ef8 + 4 * i, 0x1f18 + 4 * i, 0x1f38 + 4 * i)),
			lfcints(fx + 0x1f58 + 0x42 * i, (0, 4, 8, 12, 16, 20, 24, 28)), pts, lfctrail(fx + 0x2168 + 4 * i))
	def lfcgam(fx):
		return (lfcfl(fx + 0xc8, 2), lfcbytes(fx, (0xbc,)), tuple(map(lambda i, fx=fx: lfcgarc(fx, i), range(8))),
			lfcpartlist(fx + 0xc, 16))
	check('LFC CrazyGambler after Create', lambda fx=lfcfx: lfcgam(fx))
	for lfct, lfcdt in ((2.0, 0.03), (2.0, 0.03), (2.0, 0.01), (2.0, 0.05), (6.0, 0.03), (6.0, 0.4), (1.0, 0.03),
			(3.0, 0.03), (3.0, 0.03), (3.0, 0.5), (3.0, 0.03), (3.0, 0.03)):
		lfcset(lfcc[0] + 0x10, (lfct,))
		check('LFC CrazyGambler_Update' + fmt((lfct, lfcdt)), lambda fx=lfcfx, dt=lfcdt: (SDTest.Call(0xda0e0,
			'tvif', fx, dt), lfcgam(fx)))
	lfcc = lfcctrlname(lfcc, 'Walk')
	check('LFC CrazyGambler_Update Walk', lambda fx=lfcfx: (SDTest.Call(0xda0e0, 'tvif', fx, 0.03), lfcgam(fx)))
	lfcvisible(lfcs[1], 0)
	check('LFC CrazyGambler_Update hidden', lambda fx=lfcfx: (SDTest.Call(0xda0e0, 'tvif', fx, 0.03), lfcgam(fx)))
	lfcvisible(lfcs[1], 1)
lfcdone(lfcs[0], lfcs[2], 0xd7730, 0xd7360)

# ---- Messenger ----
lfcc = lfcctrlname(lfcc, 'Walk')
lfcs = lfcmake('Messenger', 'Models/Chars/Messenger/Messenger.M3D', 'Messenger.M3D', ('DF_GlowLighting',),
	('DF_Rayos_01', 'DF_Rayos_02', 'DF_Rayos_03', 'DF_Rayos_04', 'DF_Rayos_05', 'DF_Rayos_06', 'DF_Rayos_07',
	'DF_Rayos_08'), lfcc[0])
if lfcs[2]:
	lfce = lfcs[0]
	lfcfx = lfcs[2]
	lfcvisflags = lfci(lfci(lfcs[1]) + 0x120)
	lfcplace(lfcs[1], (0.0, -60.0, 400.0), (0.0, 0.7, 0.0))
	lfcset(lfce + 0xa0, lfcadd(lfccam, (0.0, -60.0, 400.0)))
	def lfcmes(fx):
		return (lfcbytes(fx, (0xc,)), lfcfl(fx + 0x104, 12), lfcints(fx, tuple(range(0x134, 0x1f4, 4))),
			lfcfl(fx + 0x224, 36), lfcints(fx, tuple(range(0x2b4, 0x334, 4))), lfctrails(fx + 0x1f4, 12),
			lfcpartlist(fx + 0x14, 48))
	for lfcdt in (0.05, 0.05, 0.02, 0.1, 0.05, 0.3, 0.01, 0.05):
		check('LFC Messenger_Update' + fmt((lfcdt,)), lambda fx=lfcfx, dt=lfcdt: (SDTest.Call(0xdc1d0, 'tvif', fx,
			dt), lfcmes(fx)))
	lfcc = lfcctrlname(lfcc, 'Relax')
	check('LFC Messenger_Update Relax', lambda fx=lfcfx: (SDTest.Call(0xdc1d0, 'tvif', fx, 0.05), lfcmes(fx)))
	lfcc = lfcctrlname(lfcc, 'Relax2')
	check('LFC Messenger_Update Relax2', lambda fx=lfcfx: (SDTest.Call(0xdc1d0, 'tvif', fx, 0.05), lfcmes(fx)))
	lfcset(lfce + 0xa0, lfcadd(lfccam, (0.0, 0.0, 9000.0)))
	check('LFC Messenger_Update far', lambda fx=lfcfx: (SDTest.Call(0xdc1d0, 'tvif', fx, 0.05), lfcmes(fx)))
lfcdone(lfcs[0], lfcs[2], 0xd7830, 0xd74c0)

# ---- Mercenary: the smoke system with particles when it is new ----
lfcsmoke = SDTest.Peek(0x446c28, 'p')
lfcgm = lfcz(0x1c)
SDTest.Call(0x20eb10, 'tiis', lfcgm, lfcsmoke)
lfcsmokeok = lfcok(lfcgm, ('Dummy01',), ())
SDTest.Call(0x20ec10, 'tvi', lfcgm)
lfcfree(lfcgm)
lfcsys = SDTest.Call(0x209590, 'tiis', lfcworld, lfcsmoke)[0]
lfcsysnew = lfcsys and not lfci(lfcsys) and not lfci(lfcsys + 4)
check('LFC smoke system', lambda: (lfcsmoke, lfcsmokeok, lfcsys != 0, lfcsysnew))
if lfcsmokeok and lfcsysnew:
	lfcpoke(lfcsys + 0x30, 1)
def lfcsmokepart():
	if not lfcsys:
		return 0
	d = lfci(lfcsys + 4)
	if not d or not lfci(d + 0x54):
		return 0
	el = lfci(d + 0x54)
	return (lfcbytes(el, (0xb4, 0xb5)), lfcparts(el + 4))
for lfcm in ('MercenaryA', 'MercenaryL'):
	lfcc = lfcctrlname(lfcc, 'Walk')
	lfcs = lfcmake(lfcm, 'Models/Chars/%s/%s.M3D' % (lfcm, lfcm), lfcm + '.M3D', ('DF_Glow', 'DF_Burbujas'), (),
		lfcc[0])
	if lfcs[2]:
		lfce = lfcs[0]
		lfcfx = lfcs[2]
		lfcvisflags = lfci(lfci(lfcs[1]) + 0x120)
		lfcplace(lfcs[1], (0.0, -60.0, 400.0), (0.0, 0.7, 0.0))
		def lfcmerc(fx):
			r = (lfcbytes(fx, (8, 0x178)), lfcfl(fx + 0xc, 1), lfcparts(fx + 0xc8), lfcpartlist(fx + 0xc8, 7),
				lfcsmokepart())
			if lfcb(fx + 0x178) and lfci(fx + 0x17c) != -1:
				r = r + (lfcparts(fx + 0x180),)
			return r
		for lfcname, lfcdt in (('Walk', 0.03), ('Walk', 0.03), ('Walk', 0.06), ('Drink', 0.06), ('Walk', 0.06),
				('Walk', 0.08), ('TalkHappy', 0.06), ('Walk', 0.06), ('Walk', 0.06), ('Relax0', 0.06), ('Walk', 0.1)):
			lfcc = lfcctrlname(lfcc, lfcname)
			check('LFC %s_Update' % lfcm + fmt((lfcname, lfcdt)), lambda fx=lfcfx, dt=lfcdt: (SDTest.Call(0xdb1d0,
				'tvif', fx, dt), lfcmerc(fx)))
		lfcvisible(lfcs[1], 0)
		check('LFC %s_Update hidden' % lfcm, lambda fx=lfcfx: (SDTest.Call(0xdb1d0, 'tvif', fx, 0.06), lfcmerc(fx)))
		lfcvisible(lfcs[1], 1)
	lfcdone(lfcs[0], lfcs[2], 0xd7800, 0xd7470)

# ---- Rusty: the pauses of the bolts are not set by Reset (zeroed here) ----
lfcc = lfcctrlname(lfcc, 'Walk')
lfcs = lfcmake('Rusty', 'Models/Chars/Rusty/Rusty.M3D', 'Rusty.M3D', ('DR_Mecanico_ColetaChispas',
	'DR_Mecanico_PechoGlowA', 'DR_Mecanico_PechoGlowB', 'DF_GlowLighting'), ('DF_LightingA', 'DF_LightingB',
	'DF_LightingC'), lfcc[0])
if lfcs[2]:
	lfce = lfcs[0]
	lfcfx = lfcs[2]
	lfcvisflags = lfci(lfci(lfcs[1]) + 0x120)
	lfcplace(lfcs[1], (0.0, -60.0, 400.0), (0.0, 0.7, 0.0))
	lfcset(lfcfx + 0x2fc, (0.0, 0.0, 0.0, 0.0))
	def lfcrusty(fx):
		return (lfcbytes(fx, (8,)), lfcfl(fx + 0x228, 1), lfcfl(fx + 0x2ec, 8), lfcparts(fx + 0x178),
			lfcactive(fx + 0x178), lfctrails(fx + 0x30c, 4), lfcpartlist(fx + 0x23c, 16))
	for lfcdt in (0.05, 0.05, 0.1, 0.02, 0.3, 0.05, 0.05, 0.2, 0.01, 0.6, 0.05, 0.05):
		check('LFC Rusty_Update' + fmt((lfcdt,)), lambda fx=lfcfx, dt=lfcdt: (SDTest.Call(0xdd760, 'tvif', fx, dt),
			lfcrusty(fx)))
	lfcvisible(lfcs[1], 0)
	check('LFC Rusty_Update hidden', lambda fx=lfcfx: (SDTest.Call(0xdd760, 'tvif', fx, 0.05), lfcrusty(fx)))
	lfcvisible(lfcs[1], 1)
lfcdone(lfcs[0], lfcs[2], 0xd78a0, 0xd7580)
lfcctrlfree(lfcc)

# ---- Moto: a fake object (the vtable of cObjEntity); the engine positions of the last smoke set first (the exe reads
# them before it writes them) ----
lfce = lfcz(0x400)
lfcpoke(lfce, 0x3b367c + 0x400000)
SDTest.Call(0x20eb10, 'tiis', lfce + 0xb8, 'Levels/ABIndoor/Map/TheEndA/Moto.M3D')
lfcgm = lfce + 0xb8
lfcfx = 0
if lfcok(lfcgm, ('DF_MotorTrust_01', 'DF_MotorTrust_02', 'DF_MotorTrust_03', 'DF_MotorTrust_04', 'DF_MotorTrust_05',
		'DF_EscapeTube_01', 'DF_EscapeTube_02', 'DF_EscapeTube_03', 'DF_EscapeTube_04', 'DF_EscapeTube_05',
		'DF_EscapeTube_06'), ()):
	lfcslot = lfcz(4)
	SDTest.Call(0xe1320, 'visi', lfce, 'Moto.M3D', lfcslot)
	lfcfx = lfci(lfcslot)
	lfcfree(lfcslot)
check('LFC Moto created', lambda: (lfcfx != 0, lfcfx and lfci(lfcfx) - 0x400000))
if lfcfx:
	lfcplace(lfcgm, (0.0, -100.0, 600.0), (0.1, 0.8, 0.0))
	lfcset(lfcfx + 0x14, lfcadd(lfccam, (-30.0, -90.0, 560.0)) + lfcadd(lfccam, (150.0, -120.0, 500.0)) +
		lfcadd(lfccam, (-200.0, -120.0, 480.0)))
	def lfcmoto(fx):
		return (lfcfl(fx + 8, 12), lfcbytes(fx, (0x7dc,)), tuple(map(lambda i, fx=fx: (lfcparts(fx + 0x44 + 0xb0 * i),
			lfcpartlist(fx + 0x44 + 0xb0 * i, 6)), range(3))), lfcpartlist(fx + 0x254, 16), lfcpartlist(fx + 0x304, 16))
	check('LFC Moto_Update off', lambda fx=lfcfx: (SDTest.Call(0xe90f0, 'tvif', fx, 0.05), lfcfl(fx + 8, 3)))
	for lfcab, lfcdts in (((0.7, 0.3), (0.01, 0.015, 0.03, 0.05, 0.0, 0.1)), ((1.0, 0.0), (0.03, 0.04)), ((0.0, 1.0),
			(0.03, 0.025)), ((1.5, 0.5), (0.05,))):
		SDTest.Call(0xe4640, 'tviff', lfcfx, lfcab[0], lfcab[1])
		for lfcdt in lfcdts:
			check('LFC Moto_Update' + fmt((lfcab, lfcdt)), lambda fx=lfcfx, dt=lfcdt: (SDTest.Call(0xe90f0, 'tvif',
				fx, dt), lfcmoto(fx)))
	SDTest.Call(0xe1150, 'tvi', lfcfx)
	SDTest.Call(0xe0d80, 'tiii', lfcfx, 1)
lfcunfake(lfce)
lfcfree(lfcvec)
check('LFC rand after effects', lambda: SDTest.Call(0x323c2f, 'i')[0])

# ---- GearShootLoad on a WalkChar of our own with the Gear model and its embedded effect ----
def lfcnative(name, params):
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
SDTest.Call(0x323c50, 'vi', 31337)
lfcG = lfcadd(lfccam, (0.0, -60.0, 400.0))
lfcg = SDTest.Call(0x208360, 'tiisfffs', lfcworld, 'SDLfcGear', lfcG[0], lfcG[1], lfcG[2], 'WalkChar')[0]
check('LFC gear WalkChar', lambda: lfcg != 0)
if lfcg:
	SDTest.Call(lfci(lfci(lfcg) + 0x30) - 0x400000, 'tvisfffi', lfcg, 'Models/Chars/Gear/Gear.M3D', 1.0, 1.0, 1.0, 0)
	lfcgok = lfcok(lfcg + 0xb8, ('DF_GearShoot', 'DF_PoliceAlarm'), ())
	check('LFC gear model', lambda: (lfcgok, SDTest.Call(0x20f7a0, 'tsi', lfcg + 0xb8)[0]))
	if lfcgok:
		SDTest.Call(lfci(lfci(lfcg) + 0x3c) - 0x400000, 'tvis', lfcg, 'Gear.M3D')
		lfcfx = lfci(lfcg + 0x224)
		check('LFC gear effect', lambda: (lfcfx != 0, lfcfx and lfci(lfcfx) - 0x400000))
	if lfcgok and lfcfx:
		lfcvisflags = lfci(lfci(lfcg + 0xb8) + 0x120)
		def lfcgear(fx):
			return (lfcparts(fx + 0xd4), lfcactive(fx + 0xd4), lfcnum(fx + 0xd4))
		SDTest.Poke(0x4a8c8c, 1)
		for lfcd in (0.25, 0.0, 1.5):
			check('LFC GearShootLoad' + fmt((lfcd,)), lambda d=lfcd, fx=lfcfx: (lfcnative('GearShootLoad',
				('SDLfcGear', 'Ev', d)), lfcgear(fx), SDTest.Call(0x323c2f, 'i')[0]))
		lfcpoke(lfcg + 0x224, 0)
		check('LFC GearShootLoad no effect', lambda: (lfcnative('GearShootLoad', ('SDLfcGear', 'Ev', 0.5)),
			SDTest.Call(0x323c2f, 'i')[0]))
		lfcpoke(lfcg + 0x224, lfcfx)
		lfcvisible(lfcg + 0xb8, 0)
		check('LFC GearShootLoad hidden', lambda fx=lfcfx: (lfcnative('GearShootLoad', ('SDLfcGear', 'Ev', 0.5)),
			lfcgear(fx), SDTest.Call(0x323c2f, 'i')[0]))
		lfcvisible(lfcg + 0xb8, 1)
	check('LFC gear delete', lambda: (SDTest.Call(0x208810, 'tbis', lfcworld + 4, 'SDLfcGear'),
		SDTest.Call(lfci(lfci(lfcg) + 0xc) - 0x400000, 'tiii', lfcg, 1)[0] == lfcg)[1])
SDTest.Poke(0x4a8c8c, lfcsplit)
lfcpoke(lfcworld + 0x2d4, lfcdelta)
# ---- the strafe arcs of the 3D control ----
def lfcpos(e):
	return SDTest.Call(lfci(lfci(e) + 0x18) - 0x400000, 'tiiF', e, (0.0, 0.0, 0.0))[1]
def lfcsetpos(e, p):
	SDTest.Call(lfci(lfci(e) + 0x1c) - 0x400000, 'tviF', e, p)
# the points of a debug list (at most 40)
def lfclist(l):
	r = []
	n = lfci(l)
	k = 0
	while n and k < 40:
		r.append(lfcfl(n + 8, 3))
		n = lfci(n)
		k = k + 1
	return (lfci(l + 8), tuple(r))
lfcP = (-36000.0, 60000.0, 42000.0)
SDTest.Call(0x323c50, 'vi', 1213)
lfcw = SDTest.Call(0x208360, 'tiisfffs', lfcworld, 'SDLfcWalk', lfcP[0], lfcP[1], lfcP[2], 'WalkChar')[0]
lfcw2 = SDTest.Call(0x208360, 'tiisfffs', lfcworld, 'SDLfcWalk2', lfcP[0] + 300.0, lfcP[1], lfcP[2] + 100.0,
	'WalkChar')[0]
check('LFC WalkChars', lambda: (lfcw != 0, lfcw2 != 0))
if lfcw and lfcw2:
	lfcsetpos(lfcw, lfcP)
	lfcsetpos(lfcw2, (lfcP[0] + 300.0, lfcP[1], lfcP[2] + 100.0))
	lfcac = lfcz(0x80)
	lfccls = lfcz(0x200)
	lfcpoke(lfcac + 8, lfccls)
	lfcset(lfccls + 0x164, (40.0,))
	lfcset(lfccls + 0x16c, (25.0,))
	lfcpoke(lfcw + 0x234, lfcac)
	lfcset(lfcw + 0x45c, (30.0,))
	lfcAV = lfcz(0x80)
	lfcA = lfcz(0x100)
	lfcpoke(lfcAV + 0x38, 0x485730)
	lfcpoke(lfcAV + 0x58, 0x46a4f0)
	lfcpoke(lfcA, lfcAV)
	lfcpoke(lfcA + 0x38, lfcA + 0x38)
	lfcpoke(lfcA + 0x3c, lfcA + 0x38)
	lfcpoke(lfcA + 0x40, lfcw2)
	lfcarg = lfcz(8)
	lfcm3 = SDTest.Call(0x1334d0, 'ii', 0x360)[0]
	lfcc3 = SDTest.Call(0x079d10, 'tiiiii', lfcm3, lfcw, lfcA, lfcarg)[0]
	check('LFC 3D control', lambda: (lfcc3 == lfcm3, lfci(lfcc3) - 0x400000))
	lfcshow = SDTest.Peek(0x4443e8, 'i')
	lfcshowtest = SDTest.Peek(0x444e3c, 'i')
	for lfcshowon in (0, 1):
		SDTest.Poke(0x4443e8, lfcshowon)
		SDTest.Poke(0x444e3c, lfcshowon)
		for lfcgoal in ((600.0, 0.0, 200.0), (0.0, 500.0, 0.0), (0.0, 0.0, 0.0), (-300.0, -200.0, 50.0)):
			lfcset(lfcc3 + 0x10c, lfcadd(lfcP, lfcgoal))
			for lfcargs in (((1.0, 0.0, 0.0), (0.0, 0.0, 1.0), 200.0, 1, 1), ((1.0, 0.0, 0.0), (0.0, 0.0, 1.0), 200.0,
					0, 0), ((0.0, 0.0, 3.0), (-1.0, 0.5, 0.0), 150.0, 1, 0), ((2.0, 1.0, 0.0), (2.0, 1.0, 0.0), 120.0,
					0, 1), ((0.0, 0.0, 0.0), (0.0, 1.0, 0.0), 80.0, 1, 1), ((-1.0, 0.0, -1.0), (1.0, 0.0, 1.0), 400.0,
					1, 1)):
				lfcE = lfcadd(lfcP, (150.0, 0.0, 60.0))
				check('LFC 3D StrafeBack' + fmt((lfcshowon, lfcgoal, lfcargs)), lambda a=lfcargs, E=lfcE:
					(SDTest.Call(0x07f250, 'tbiFFFfii', lfcc3, E, a[0], a[1], a[2], a[3], a[4])[0], lfclist(lfcc3 + 0x348),
					lfci(lfcc3 + 0x318 + 8)))
				check('LFC 3D StrafeFwd' + fmt((lfcshowon, lfcgoal, lfcargs)), lambda a=lfcargs, E=lfcE:
					(SDTest.Call(0x07fd50, 'tbiFFFfii', lfcc3, E, a[0], a[1], a[2], a[3], a[4])[0],
					lfci(lfcc3 + 0x318 + 8), lfci(lfcc3 + 0x330 + 8)))
		# a strafe through the other character: the forward arc hits it
		lfcset(lfcc3 + 0x10c, lfcadd(lfcP, (600.0, 0.0, 200.0)))
		lfcE = lfcadd(lfcP, (150.0, 0.0, 50.0))
		check('LFC 3D StrafeFwd hit %d' % lfcshowon, lambda E=lfcE: SDTest.Call(0x07fd50, 'tbiFFFfii', lfcc3, E,
			(-1.0, 0.0, -0.3), (1.0, 0.0, 0.3), 160.0, 1, 1)[0])
	SDTest.Poke(0x4443e8, lfcshow)
	SDTest.Poke(0x444e3c, lfcshowtest)
	check('LFC 3D delete', lambda: SDTest.Call(0x079d50, 'tiii', lfcc3, 1)[0] == lfcc3)
	lfcpoke(lfcw + 0x234, 0)
	lfcfree(lfcac)
	lfcfree(lfccls)
	lfcfree(lfcA)
	lfcfree(lfcAV)
	lfcfree(lfcarg)
for lfcE in (lfcw, lfcw2):
	if lfcE:
		check('LFC delete entity', lambda e=lfcE: (SDTest.Call(0x208810, 'tbis', lfcworld + 4, SDTest.Peek(e + 4,
			'@p')), SDTest.Call(lfci(lfci(e) + 0xc) - 0x400000, 'tiii', e, 1)[0] == e)[1])
check('LFC rand after', lambda: SDTest.Call(0x323c2f, 'i')[0])
