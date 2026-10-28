# game Entities/Vehicle/Weaps/Devastator.c, the native "!DevastatorCreate" (execfile'd by sdlevel.py, Python 1.5.2).
# Two cars of our own far away (SDFnuCar with the SPoli1 model, the shooter, and SDFnuCar2, deleted at the end); the
# Devastator slots, the ammo and the weapon nodes of the shooter poked (node 0 for every slot). The shots go through
# the call builder, rand seeded: far away and 3000 above the camera. The far ones are seen (the slot node of the
# shooter is at the origin, the midpoint and the length of the shot reach the camera): the explosion particles, the
# shot trail and the sparks, also with the target SDFnuCar2 (the hit moves towards the shooter). The ones above the
# camera are not: only the slot shot, the damage sphere and the boom. With a fake player camera on the shooter the
# bullet debris (its system keeps it at any distance during the call).
# Printed: the next rand, the hit point after the call and the fields the effects of the debris get (the newest debris
# of each system). Not reached: the floor under the hit (the shots are in the air), the larger explosion of a hit
# player.
def fnui(a):
	return SDTest.Peek(a, '@i')
def fnub(a):
	return SDTest.Peek(a, '@b')
def fnuz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fnufree(p):
	SDTest.Call(0x133550, 'ii', p)
def fnufl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fnuset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fnupoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fnustr(a, s):
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
def fnusrand(s):
	SDTest.Call(0x323c50, 'vi', s)
def fnurand():
	return SDTest.Call(0x323c2f, 'i')[0]
fnuworld = SDTest.Peek(0x467450, 'i')
def fnufind(name):
	slot = SDTest.Call(0x131f70, 'tiis', fnuworld + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', fnuworld + 4, name, slot)[0]
	if node:
		return fnui(node)
	return 0
def fnucall(func, params):
	Scrap.SetCallFunc(func)
	for p in params:
		if type(p) == type(''):
			Scrap.AddParams(p)
		elif type(p) == type(0):
			Scrap.AddParami(p)
		else:
			Scrap.AddParamf(p)
	Scrap.Execute()
# the newest debris of the system of the file variable at rva (the system is made when missing), its element
def fnudebris(rva):
	name = SDTest.Peek(SDTest.Peek(rva, 'i'), '@s')
	sys = SDTest.Call(0x209590, 'tiis', fnuworld, name)[0]
	if not sys:
		return (0, 0)
	d = fnui(sys + 4)
	if not d:
		return (sys, 0)
	return (sys, d)
def fnuhitfx():
	sys, d = fnudebris(0x449228)
	if not d or not fnui(d + 0x54):
		return d != 0
	p = fnui(d + 0x54) + 4
	return (fnufl(d + 0x4c, 1), fnui(p + 0x18), fnufl(p + 8, 2), fnufl(p + 0x14, 1), fnufl(p + 0x1c, 1),
		fnufl(p + 0x2c, 6), fnufl(p + 0x50, 6), fnufl(p + 0x74, 6), fnufl(p + 0x8c, 3), fnub(p + 0x98), fnub(p + 0x99))
def fnushootfx():
	sys, d = fnudebris(0x449130)
	if not d or not fnui(d + 0x54):
		return d != 0
	e = fnui(d + 0x54)
	return (fnufl(d + 0x4c, 1), fnufl(e + 8, 1), fnub(e + 0x11))
def fnusparks():
	sys, d = fnudebris(0x4491c0)
	if not d or not fnui(d + 0x54):
		return d != 0
	e = fnui(d + 0x54)
	n = fnui(e + 0xa34)
	r = (fnufl(sys + 0x14, 1), fnufl(d + 0x4c, 1), fnub(e + 0xc6c), n)
	k = 0
	while k < n and k < 3:
		j = fnui(e + 0xa38 + 4 * k)
		r = r + ((j, fnufl(e + 0x30 + 12 * j, 3), fnufl(e + 0x1b0 + 12 * j, 3), fnufl(e + 0x330 + 12 * j, 3),
			fnufl(e + 0x4b0 + 12 * j, 3), fnufl(e + 0x630 + 4 * j, 1), fnufl(e + 0x7b0 + 4 * j, 1),
			fnui(e + 0x830 + 4 * j), fnui(e + 0x930 + 4 * j), fnui(e + 0x9b4 + 4 * j)),)
		k = k + 1
	return r
def fnubullet():
	sys, d = fnudebris(0x449058)
	if not d:
		return sys != 0
	return (fnufl(d + 0x1c, 12), fnufl(d + 0x4c, 1))

fnucar = Scrap.CreateEntity('SDFnuCar', 60000.0, 50000.0, 50000.0, 'Car')
fnucar2 = Scrap.CreateEntity('SDFnuCar2', 61500.0, 50000.0, 50000.0, 'Car')
fnuc = fnufind('SDFnuCar')
fnuc2 = fnufind('SDFnuCar2')
check('SDFnuCar', lambda: (fnucar is not None, fnucar2 is not None, fnuc != 0, fnuc2 != 0))
if fnuc and fnuc2:
	check('SDFnuCar SetModel SPoli1', lambda c=fnuc: (SDTest.Call(0x109db0, 'tvisfffi', c, 'SPoli1', 1.0, 1.0, 1.0, 1),
		fnui(c + 0xb8) != 0, fnui(c + 0x5f0 + 0x1c) != 0, fnui(c + 0xf0)))
	fnuwm = fnuc + 0x5f0
	fnuws = fnui(fnuwm + 0x18)
	fnuam = fnui(fnuwm + 0x20)
	fnunodes = fnui(fnuwm + 0x1c)
	fnudw = SDTest.Peek(0x448830, 'i')
	fnuda = SDTest.Peek(0x449108, 'i')
	check('SDFnuCar weapons', lambda: (fnudw, fnuda, fnuws != 0, fnuam != 0, fnunodes != 0))
	if fnuws and fnuam and fnunodes and fnui(fnuc + 0xb8):
		fnuslots = fnui(fnuws + 12 * fnudw)
		fnuup = fnui(fnuws + 12 * fnudw + 4)
		fnuammo = fnui(fnuam + 4 * fnuda)
		fnunsaved = map(lambda k: fnui(fnunodes + 0x10 * fnuda + 4 * k), range(4))
		for fnuk in range(4):
			fnupoke(fnunodes + 0x10 * fnuda + 4 * fnuk, 0)
		fnudevs = SDTest.Peek(0x448f48, 's')
		fnudevv = SDTest.Peek(0x448fc8, 's')
		fnucenter = fnufl(0x849258, 3)
		fnushake = fnui(fnuc + 0xf0)
		fnufake = fnuz(0x300)
		fnucur = fnui(fnuworld + 0x50)
		fnucam = fnufl(fnuworld + 0x254 + 12 * fnucur, 3)
		fnuup3 = (fnucam[0], fnucam[1] + 3000.0, fnucam[2])
		fnufar = (90000.0, 90000.0, 90000.0)
		# no slot: nothing happens
		fnupoke(fnuws + 12 * fnudw, 0)
		fnusrand(3)
		check('native !DevastatorCreate no slot', lambda p=fnufar: (fnucall('!DevastatorCreate', ('SDFnuCar', p[0],
			p[1], p[2], '', 0)), fnurand(), fnufl(0x849258, 3)))
		# the bullets stay (the system moves them at any distance from the cameras)
		fnubsys = fnudebris(0x449058)[0]
		fnubrad = 0
		if fnubsys:
			fnubrad = fnui(fnubsys + 0x14)
		for fnup, fnut, fnusl, fnua, fnushot, fnuu, fnupl, fnuseed in (
				(fnufar, '', 0xf, 3.0, 0, 0, 0, 4), (fnufar, '', 0x5, 7.0, 2, 7, 0, 5), (fnufar, '', 0xa, 1.0, 1, 0, 0, 6),
				(fnufar, 'SDFnuCar2', 0x2, 0.0, 0, 0, 0, 9), (fnufar, '', 0x4, 1.0, 0, 0, 1, 10),
				(fnuup3, '', 0xf, 2.0, 1, 0, 0, 7), (fnuup3, '', 0x9, 5.0, 3, 7, 0, 8), (fnuup3, '', 0x8, 2.0, 5, 0, 1, 11)):
			fnupoke(fnuws + 12 * fnudw, fnusl)
			fnupoke(fnuws + 12 * fnudw + 4, fnuu)
			fnuset(fnuam + 4 * fnuda, (fnua,))
			if fnupl:
				fnupoke(fnuc + 0xf0, fnufake)
				if fnubsys:
					fnuset(fnubsys + 0x14, (1e30,))
			fnusrand(fnuseed)
			fnucall('!DevastatorCreate', ('SDFnuCar', fnup[0], fnup[1], fnup[2], fnut, fnushot))
			fnupoke(fnuc + 0xf0, fnushake)
			fnuname = fmt((fnup == fnuup3, fnut, fnusl, fnua, fnushot, fnuu, fnupl, fnuseed))
			check('native !DevastatorCreate' + fnuname, lambda: (fnurand(), fnufl(0x849258, 3),
				SDTest.Peek(0x448f48, 's'), SDTest.Peek(0x448fc8, 's')))
			if fnup == fnufar:
				check('DevastatorCreate hit' + fnuname, fnuhitfx)
				check('DevastatorCreate shoot' + fnuname, fnushootfx)
				check('DevastatorCreate sparks' + fnuname, fnusparks)
			if fnupl:
				check('DevastatorCreate bullet' + fnuname, fnubullet)
				if fnubsys:
					fnupoke(fnubsys + 0x14, fnubrad)
		fnupoke(fnuws + 12 * fnudw, fnuslots)
		fnupoke(fnuws + 12 * fnudw + 4, fnuup)
		fnupoke(fnuam + 4 * fnuda, fnuammo)
		for fnuk in range(4):
			fnupoke(fnunodes + 0x10 * fnuda + 4 * fnuk, fnunsaved[fnuk])
		fnuset(0x849258, fnucenter)
		fnustr(0x848f48, fnudevs)
		fnustr(0x848fc8, fnudevv)
		fnufree(fnufake)
if fnucar:
	check('SDFnuCar Del', lambda: fnucar.Del())
if fnucar2:
	check('SDFnuCar2 Del', lambda: fnucar2.Del())

# ---- the radiosity of the map compiler (engine3d Engine/BspCalc.c) on a fake map of two lists 200 above the player:
# a floor square F (normal up) and 6 above it a ceiling square C (normal down) whose material is an area light by the
# mats_luces_rad section of the ini of the scene, with a glow map (luzA) and a falloff of 2. CBsp_RadAreas (the areas
# of the lists, the area light and lglow settings, the local pass into rad), then CBsp_RadGlobal with one bounce of
# the light maps rad2 (C counted as a plain list again). The TXF cache is off (no files), the TXF folder is the one of
# sdlevel_bspcalc.py.
def fnufls(a, n):
	r = ()
	i = 0
	while i < n:
		k = min(32, n - i)
		r = r + fnufl(a + 4 * i, k)
		i = i + k
	return r
def fnustrz(s):
	p = fnuz(len(s) + 1)
	SDTest.Call(0x3197d0, 'iisi', p, s, len(s))
	return p
def fnupokeb(a, v):
	w = fnui(a & ~3)
	sh = 8 * (a & 3)
	fnupoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
fnurg = (0x432950, 0x432954, 0x432958, 0x43295c, 0x4b2b14, 0x432968, 0x43296c, 0x4b2b60, 0x4b2b64, 0x4d5cf0, 0x4d5d08,
	0x4b2b74, 0x4b2b24,
	0x4b2b48, 0x4b2b4c, 0x4b2b50, 0x4b2b54, 0x4d5d78, 0x4d5d7c, 0x4d5d80, 0x4d5d84)
fnursaved = []
for fnu_a in fnurg:
	fnursaved.append(SDTest.Peek(fnu_a, 'i'))
check('fnu radiosity globals', lambda: (SDTest.Peek(0x432958, 'i'), SDTest.Peek(0x43295c, 'i'), SDTest.Peek(0x4b2b74, 'i'),
	SDTest.Peek(0x432968, 'i')))
for fnu_a, fnu_v in ((0x432950, 1.0), (0x432954, 1.0), (0x43296c, 1.0), (0x4b2b60, 1.0), (0x4b2b64, 1.0)):
	fnuset(fnu_a + 0x400000, (fnu_v,))
# small light map pages (the areas of CBsp_IniciarAreas take a buffer of the size of a page)
for fnu_a, fnu_v in ((0x432958, 32), (0x43295c, 32), (0x4b2b14, 1), (0x432968, 1), (0x4d5cf0, 0), (0x4d5d08, 0),
		(0x4b2b74, 0), (0x4b2b24, 0)):
	SDTest.Poke(fnu_a, fnu_v)
fnumat = []
for fnu_n in ('sdfnumatF', 'sdfnumatC'):
	fnu_m = fnuz(0xb0)
	SDTest.Call(0x233830, 'tii', fnu_m)
	SDTest.Call(0x2a1940, 'tvis', fnu_m + 4, fnu_n)
	fnumat.append(fnu_m)
fnuo = (pp[0] + 20.0, pp[1] + 200.0, pp[2] + 20.0)
def fnutri(a, b, c, y, ny):
	o = fnuo
	t = fnuz(0x224)
	SDTest.Call(0x281d40, 'tii', t)
	fnuset(t + 4, (o[0] + a[0], o[1] + y, o[2] + a[1], o[0] + b[0], o[1] + y, o[2] + b[1], o[0] + c[0], o[1] + y,
		o[2] + c[1]))
	fnuset(t + 0x28, (0.0, ny, 0.0) * 3)
	return t
def fnulist(ts, mat):
	for i in range(len(ts) - 1):
		fnupoke(ts[i], ts[i + 1])
	l = ts[0]
	SDTest.Call(0x287bf0, 'vi', l)
	fnupoke(l + 0xac, mat)
	b = SDTest.Call(0x287930, 'iFi', (0.0,) * 6, l)[1]
	e = SDTest.Call(0x287ac0, 'vFifpppp', b, 1, 1.0, 0, 0, 0, 0)
	SDTest.Call(0x2900d0, 'viiiiii', l, e[2], e[3], e[4], e[5], 0)
	lx, ly = fnui(l + 0xd4), fnui(l + 0xd8)
	SDTest.Call(0x28ff20, 'tviiii', l + 0x114, lx, ly, 0)
	cob = fnuz(12)
	SDTest.Call(0x28ff20, 'tviiii', cob, lx, ly, 0xffff0000)
	SDTest.Call(0x285d80, 'vii', l, cob)
	SDTest.Call(0x28ff90, 'tviiiF', l + 0xb0, lx, ly, (0.1, 0.2, 0.3))
	SDTest.Call(0x28ff90, 'tviiiF', l + 0x108, lx, ly, (0.5, 0.6, 0.7))
	return l
fnuF = fnulist([fnutri((0.0, 0.0), (0.0, 6.0), (6.0, 0.0), 0.0, 1.0), fnutri((6.0, 0.0), (0.0, 6.0), (6.0, 6.0), 0.0, 1.0)],
	fnumat[0])
fnuC = fnulist([fnutri((0.0, 0.0), (6.0, 0.0), (0.0, 6.0), 6.0, -1.0), fnutri((6.0, 0.0), (6.0, 6.0), (0.0, 6.0), 6.0, -1.0)],
	fnumat[1])
SDTest.Call(0x290000, 'tviiiF', fnuC + 0xc8, fnui(fnuC + 0xd4), fnui(fnuC + 0xd8), (2.0, 1.0, 0.5, 1.0))
fnuset(fnuC + 0x150, (2.0,))
fnuini = SDTest.Call(0x261980, 'tii', fnuz(4))[0]
SDTest.Call(0x2621a0, 'tiisss', fnuini, 'mats_luces_rad', 'sdfnumatC', '200 100 50 1.5')
SDTest.Call(0x2621a0, 'tiisss', fnuini, 'lglow_mult', '_default_', '255 128 64 2.0')
fnuesc = fnuz(0x64)
fnupoke(fnuesc + 0x4c, fnuini)
fnumod = fnuz(0x128)
fnupoke(fnumod + 0x124, fnuesc)
fnumap = fnuz(0x61aa8)
fnupoke(fnumap, fnustrz('sdbsctxf/map.sm3'))
fnupoke(fnumap + 4, fnumod)
fnupoke(fnumap + 8, fnuF)
fnupoke(fnumap + 12, fnuC)
fnupoke(fnumap + 0x4e208, 2)
def fnuflt(b, k):
	img = fnui(b + 8)
	if not img:
		return (fnui(b), fnui(b + 4), 0)
	return (fnui(b), fnui(b + 4), fnufls(img, k * fnui(b) * fnui(b + 4)))
def fnuradlist(l):
	r = (fnui(l + 0xd4), fnui(l + 0xd8), fnui(l + 0xdc), fnui(l + 0xe0), fnub(l + 0x154), fnub(l + 0x155), fnub(l + 0x156),
		fnui(l + 0x1a4), fnui(l + 0x13c), fnufl(l + 0x140, 1), fnufl(l + 0x150, 1), fnufl(l + 0x178, 1))
	t = l
	while t:
		r = r + ((fnui(t + 0x184), fnui(t + 0x18c), fnui(t + 0x194), fnui(t + 0x19c), fnufl(t + 0x1a8, 5)),)
		t = fnui(t)
	return r
def fnuints(a, n):
	return tuple(map(lambda i, a=a: fnui(a + 4 * i), range(n)))
check('fnu lists', lambda: (fnuradlist(fnuF), fnuradlist(fnuC)))
# the sample normals of bmp\normales.rad are not in the game data: a table of one normal stands in for them
fnunorm = (fnui(0x8e73b0), fnui(0x8e73b4))
fnunv = fnuz(12)
fnuset(fnunv, (0.0, 1.0, 0.0))
fnupoke(0x8e73b0, 1)
fnupoke(0x8e73b4, fnunv)
check('CBsp_RadAreas', lambda: (SDTest.Call(0x293af0, 'tvi', fnumap), fnuradlist(fnuF), fnuradlist(fnuC),
	fnuflt(fnuF + 0xfc, 3), fnuflt(fnuC + 0xfc, 3), fnuflt(fnuC + 0xc8, 4), fnuints(0x8d5d78, 4)))
fnupokeb(fnuC + 0x154, 0)
check('CBsp_RadGlobal', lambda: (SDTest.Call(0x292060, 'tvi', fnumap), fnuradlist(fnuF), fnuradlist(fnuC),
	fnuflt(fnuF + 0xb0, 3), fnuflt(fnuC + 0xb0, 3), fnuflt(fnuF + 0xfc, 3), fnuflt(fnuF + 0x108, 3),
	fnuints(0x8b2b48, 4)))
for fnu_i in range(len(fnurg)):
	SDTest.Poke(fnurg[fnu_i], fnursaved[fnu_i])
fnupoke(0x8e73b0, fnunorm[0])
fnupoke(0x8e73b4, fnunorm[1])
fnufree(fnunv)
# the triangles one by one (they are calloc memory, the destructor deletes the next one)
for fnu_l in (fnuF, fnuC):
	fnu_ts = []
	while fnu_l:
		fnu_ts.append(fnu_l)
		fnu_l = fnui(fnu_l)
	for fnu_t in fnu_ts:
		fnupoke(fnu_t, 0)
		SDTest.Call(0x282580, 'tvi', fnu_t)
		fnufree(fnu_t)
