# game Entities/Vehicle/WeaponManager.c, Weaps/ATPC.c and Weaps/Devastator.c (execfile'd by sdlevel.py, Python
# 1.5.2): the weapon manager (cWeaponManager = cTargetLock) on calloc memory with a fake owner (a vtable with GetAI
# and GetObjData, the SPoli1 model at +0xb8, a name) and Init with the real weapon and ammo tables of FZ; render nodes
# and slots are poked. The getters and setters, the script variables, the weapon list, the lock, the weapon sound
# (the fake has no sounds) and Update without transitions of the buttons (no script runs: no press, release, fire or
# weapon change, the hook state is kept). Then a vehicle of our own (Scrap.CreateEntity Car "SDWeapCar" far away,
# deleted at the end) for the native weapon functions through the call builder (Scrap.SetCallFunc, AddParam,
# Execute): ATPCOnStart/OnEnd, DevastatorPress/Release, CloudOnHit, EMIOnHit, ATPCFX and DevastatorFX on a Text item
# of the scorer, the ATPC and Devastator XplosionCall on a second vehicle and the cdecl helpers by rva.
# Not called: the Fire and Create functions (they aim through the world and run the weapon scripts), the missile
# paths of CloudOnHit and EMIOnHit, NextWeap/PrevWeap when the iNext/iPrev chain of a weapon does not come back.
def wmi(a):
	return SDTest.Peek(a, '@i')
def wmb(a):
	return SDTest.Peek(a, '@b')
def wmz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def wmfree(p):
	SDTest.Call(0x133550, 'ii', p)
def wmfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def wmset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def wmints(a, n):
	return tuple(map(lambda i, a=a: wmi(a + 4 * i), range(n)))
def wmpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def wmstr(a, s):
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
def wmsetbyte(a, v):
	SDTest.Call(0x3197d0, 'iisi', a, chr(v), 1)
def wmsrand(s):
	SDTest.Call(0x323c50, 'vi', s)
def wmstrat(p):
	if p:
		return SDTest.Peek(p, '@s')
	return None

wmworld = SDTest.Peek(0x467450, 'i')
wmnw = wmi(wmworld + 0x1bb8)
wmna = wmi(wmworld + 0x1bc4)
wmweaps = wmi(wmworld + 0x1bbc)
wmammos = wmi(wmworld + 0x1bc8)
wmlaser = SDTest.Peek(0x4487d0, 'i')
check('WM tables', lambda: (wmnw, wmna, wmi(wmworld + 0x1bc0), wmlaser))
def wmammoof(w):
	return wmi(wmweaps + 0xb0 * w + 0x98)

# ---- the fake owner and the manager ----
wmvt = wmz(0x100)
wmpoke(wmvt + 0x58, 0x4ef2d0)
wmpoke(wmvt + 0x98, 0x4fb380)
wmown = wmz(0x800)
wmname = wmz(0x20)
wmstr(wmname, 'SDFakeWM')
wmpoke(wmown, wmvt)
wmpoke(wmown + 4, wmname)
wm = wmz(0x48)
wmpoke(wm, 0x400000 + 0x3b664c)
# the lock node: a link of the manager (data set, alone in its ring)
wmpoke(wm + 0x28, wm + 0x28)
wmpoke(wm + 0x2c, wm + 0x28)
wmpoke(wm + 0x34, wm)
def wmstate():
	return (wmi(wm + 4) == wmown, wmints(wm + 8, 2), wmfl(wm + 0x10, 2), wmi(wm + 0x24), wmb(wm + 0x44),
		wmb(wm + 0x45), wmb(wm + 0x46), wmi(wmown + 0x414), wmb(wmown + 0x434), wmb(wmown + 0x435),
		wmb(wmown + 0x436), wmb(wmown + 0x437))
def wmarrays():
	r = ()
	ws = wmi(wm + 0x18)
	for w in range(wmnw):
		r = r + ((wmi(ws + 12 * w), wmi(ws + 12 * w + 4), wmfl(ws + 12 * w + 8, 1)[0]),)
	return (r, wmints(wmi(wm + 0x1c), 4 * wmna), wmfl(wmi(wm + 0x20), wmna))
check('cWeaponManager_Init', lambda: (SDTest.Call(0x11aad0, 'tvii', wm, wmown), wmstate(), wmarrays()))

# the getters on the empty manager
def wmgetters():
	r = ()
	for w in range(wmnw + 1):
		r = r + ((SDTest.Call(0x11a8f0, 'tiii', wm, w)[0], SDTest.Call(0x11aa30, 'tiii', wm, w)[0],
			SDTest.Call(0x11aa80, 'tfii', wm, w)[0]),)
	for w in range(wmnw):
		r = r + ((SDTest.Call(0x11a0f0, 'tiii', wm, w)[0], SDTest.Call(0x11a870, 'tfii', wm, w)[0],
			SDTest.Call(0x11a7b0, 'tiiii', wm, w, 0)[0]),)
	for a in range(wmna + 2):
		r = r + ((SDTest.Call(0x11a6c0, 'tfii', wm, a)[0], SDTest.Call(0x11a760, 'tiii', wm, a)[0]),)
	return r
check('WM getters empty', wmgetters)
for w in (-3, 0, 1, wmnw - 1, wmnw, 99):
	check('cWeaponManager_FixWeapon' + fmt((w,)), lambda w=w: SDTest.Call(0x11a6f0, 'ii', w)[0])

# render nodes for the slots: ammo kind a gets 1 + a % 4 slots
wmnodes = wmi(wm + 0x1c)
for a in range(wmna):
	for s in range(4):
		if s <= a % 4:
			wmpoke(wmnodes + 16 * a + 4 * s, s)
check('WM nodes', lambda: (wmints(wmnodes, 4 * wmna), map(lambda a: SDTest.Call(0x11a760, 'tiii', wm, a)[0],
	range(wmna + 1))))
for w in range(wmnw):
	for mask in (0, 1, 3, 0xf, 0x1f, 6, -1):
		check('cWeaponManager_SetSlots' + fmt((w, mask)), lambda w=w, mask=mask: (SDTest.Call(0x11b370, 'tiiii', wm, w,
			mask)[0], SDTest.Call(0x11a8f0, 'tiii', wm, w)[0], SDTest.Call(0x11a7b0, 'tiiii', wm, w, 0)[0]))
check('cWeaponManager_SetSlots out', lambda: (SDTest.Call(0x11b370, 'tiiii', wm, wmnw, 3)[0],
	SDTest.Call(0x11b370, 'tiiii', wm, 1000, 3)[0]))
for w, s, on in ((0, 0, 1), (0, 3, 1), (1, 1, 1), (1, 1, 0), (2, 2, 1), (2, 5, 1), (wmnw - 1, 0, 1), (wmnw, 0, 1),
		(3, 2, 7), (3, 0, 0)):
	check('cWeaponManager_SetSlot' + fmt((w, s, on)), lambda w=w, s=s, on=on: (SDTest.Call(0x11b530, 'tbiiii', wm, w,
		s, on)[0], SDTest.Call(0x11a8f0, 'tiii', wm, w)[0]))
def wmhasslots():
	r = ()
	for w in range(wmnw + 1):
		t = ()
		for s in (-1, 0, 1, 2, 3, 4, 5):
			t = t + (SDTest.Call(0x11a9f0, 'tbiii', wm, w, s)[0],)
		r = r + (t,)
	return r
check('cTargetLock_HasSlot', wmhasslots)
# NumSlots with the warning: no slot is an error, the laser gets slot 0
for w in range(wmnw):
	SDTest.Call(0x11b370, 'tiiii', wm, w, 0)
	check('cWeaponManager_NumSlots warn' + fmt((w,)), lambda w=w: (SDTest.Call(0x11a7b0, 'tiiii', wm, w, 1)[0],
		SDTest.Call(0x11a8f0, 'tiii', wm, w)[0]))

# the weapon list
for s in ('1,2,3,4,5,6,7,8,9,10,11,12', '0,15,7', '7', '', '3,,5', '1,2', '15,15,15,15,15,15,15,15,15,15,15,15,15',
		'2,1,x', 'abc'):
	check('cWeaponManager_SetWeapList' + fmt((s,)), lambda s=s: (SDTest.Call(0x11b430, 'tvis', wm, s),
		wmarrays()[0], wmstrat(SDTest.Call(0x11a920, 'tii', wm)[0])))

# ammo, upgrades, upgrade ammo
for a, v in ((0, 10.5), (1, -3.0), (wmna - 1, 1e6), (wmna, 5.0), (wmna + 3, 5.0)):
	check('cWeaponManager_SetAmmo' + fmt((a, v)), lambda a=a, v=v: (SDTest.Call(0x11b280, 'tfiif', wm, a, v)[0],
		SDTest.Call(0x11a6c0, 'tfii', wm, a)[0]))
for w in range(wmnw):
	for up in (0, 1, 2, 3, 7, 8, 9, 10, 15, 16, -1):
		check('cWeaponManager_SetUpgrade' + fmt((w, up)), lambda w=w, up=up: (SDTest.Call(0x11b5c0, 'tbiii', wm, w,
			up)[0], SDTest.Call(0x11aa30, 'tiii', wm, w)[0]))
	for v in (-1.0, 0.0, 0.5, 120.25, 199.99, 200.0, 1e9):
		check('cWeaponManager_SetUAmmo' + fmt((w, v)), lambda w=w, v=v: (SDTest.Call(0x11b650, 'tbiif', wm, w, v)[0],
			SDTest.Call(0x11aa80, 'tfii', wm, w)[0]))
check('cWeaponManager_SetUpgrade out', lambda: (SDTest.Call(0x11b5c0, 'tbiii', wm, wmnw, 1)[0],
	SDTest.Call(0x11b650, 'tbiif', wm, wmnw, 5.0)[0]))
for i in range(wmna):
	SDTest.Call(0x11b280, 'tfiif', wm, i, 3.0 + 7.0 * i)
check('WM getters full', wmgetters)
# WeaponNeed: a random order of six weapons
SDTest.Call(0x11b430, 'tvis', wm, '15,15,15,15,15,15,15,15,15,15,15,15')
for seed in (1, 2, 3, 77, 12345):
	wmsrand(seed)
	check('cWeaponManager_WeaponNeed' + fmt((seed,)), lambda: SDTest.Call(0x11c260, 'tii', wm)[0])

# next and previous weapon, only where the chain comes back to the start
def wmchain(w, off):
	seen = {}
	i = w
	while 1:
		if i < 0 or i >= wmnw or seen.has_key(i):
			return 0
		seen[i] = 1
		i = wmi(wmweaps + 0xb0 * i + off)
		if i == w:
			return 1
for w in range(wmnw):
	if wmchain(w, 0x90):
		check('cWeaponManager_NextWeap' + fmt((w,)), lambda w=w: SDTest.Call(0x11b1a0, 'tiii', wm, w)[0])
	if wmchain(w, 0x94):
		check('cWeaponManager_PrevWeap' + fmt((w,)), lambda w=w: SDTest.Call(0x11b210, 'tiii', wm, w)[0])

# the script variables
wmfvars = ('UAmmo00', 'UAmmo01', 'UAmmo05', 'UAmmo99', 'UAmmo1', 'iUAmmo', 'AcUAmmo', 'Ammo00', 'Ammo01', 'Ammo09',
	'Ammo0', 'Ammoi', 'AmmoAc', 'FireStamp', 'CMStamp', 'Bogus', 'ammo00', '')
for v in wmfvars:
	check('cWeaponManager_VarF get' + fmt((v,)), lambda v=v: SDTest.Call(0x11a190, 'tbisDi', wm, v, -7.0, 0))
for v, x in (('UAmmo01', 33.5), ('iUAmmo', 250.0), ('AcUAmmo', 12.0), ('Ammo01', 4.75), ('Ammoi', 8.5),
		('AmmoAc', 2.25), ('FireStamp', -1.5), ('CMStamp', 0.75), ('Bogus', 1.0), ('UAmmo99', 1.0)):
	check('cWeaponManager_VarF set' + fmt((v, x)), lambda v=v, x=x: (SDTest.Call(0x11a190, 'tbisDi', wm, v, x, 1),
		SDTest.Call(0x11a190, 'tbisDi', wm, v, -7.0, 0), wmfl(wmi(wm + 0x20), wmna), wmfl(wm + 0x10, 2)))
wmivars = ('WeapSnd', 'Upgrd00', 'Upgrd01', 'Upgrd99', 'iWeaponNeed', 'AcUpgrd', 'iUpgrd', 'isWpn00', 'isWpn03',
	'isWpn99', 'nWeap00', 'nWeap02', 'nWeap99', 'nSlot00', 'nSlot01', 'nSlot99', 'WeapA00', 'WeapB01', 'WeapD02',
	'WeapE00', 'AcAmmo', 'AcWeap', 'iWeap', 'iAmmo', 'Bogus', '')
for v in wmivars:
	wmsrand(5)
	check('cWeaponManager_VarI get' + fmt((v,)), lambda v=v: SDTest.Call(0x11abf0, 'tbispi', wm, v, -7, 0))
for v, x in (('WeapSnd', 3), ('Upgrd01', 1), ('AcUpgrd', 2), ('iUpgrd', 0), ('isWpn01', 5), ('isWpn99', 1),
		('WeapA01', 0), ('WeapC02', 1), ('AcWeap', 2), ('iWeap', 1), ('nWeap01', 1), ('Bogus', 1)):
	check('cWeaponManager_VarI set' + fmt((v, x)), lambda v=v, x=x: (SDTest.Call(0x11abf0, 'tbispi', wm, v, x, 1),
		SDTest.Call(0x11abf0, 'tbispi', wm, v, -7, 0), wmstate()))
SDTest.Call(0x11abf0, 'tbispi', wm, 'AcWeap', wmi(wm + 0xc), 1)
wmbuf = wmz(0x80)
wmstr(wmbuf, '1,3,7,15')
check('cWeaponManager_VarS', lambda: (wmstrat(SDTest.Call(0x11c030, 'tbispi', wm, 'WeapList', 0, 0)[1]),
	SDTest.Call(0x11c030, 'tbispi', wm, 'WeapList', wmbuf, 1)[0],
	wmstrat(SDTest.Call(0x11c030, 'tbispi', wm, 'WeapList', 0, 0)[1]),
	SDTest.Call(0x11c030, 'tbispi', wm, 'Bogus', 0, 0)))

# the model of the owner for VarV and Update
SDTest.Call(0x20eb10, 'tiis', wmown + 0xb8, 'Models/Vehicles/Ships/SPoli1/SPoli1.M3D')
SDTest.Call(0x211d20, 'tviF', wmown + 0xb8, (pp[0] + 500.0, pp[1] + 80.0, pp[2] - 300.0))
check('WM owner model', lambda: SDTest.Call(0x210da0, 'tbi', wmown + 0xb8)[0])
for v in ('WPosA00', 'WPosB00', 'WPosA01', 'WPosD03', 'WPosE00', 'WPosA99', 'WPos00', 'Bogus'):
	for setv in (0, 1):
		check('cWeaponManager_VarV' + fmt((v, setv)), lambda v=v, setv=setv: SDTest.Call(0x11c0c0, 'tbisDDDi', wm, v,
			-1.0, -2.0, -3.0, setv))

# the lock
check('cTargetLock lock off', lambda: (SDTest.Call(0x11a720, 'tsi', wm)[0], SDTest.Call(0x11a740, 'tii', wm)[0]))
check('cWeaponManager_LockEntity(Player0)', lambda: (SDTest.Call(0x11b2b0, 'tvis', wm, 'Player0'), wmb(wm + 0x44),
	wmi(wm + 0x30) != 0, wmi(wm + 0x28) != wm + 0x28, SDTest.Call(0x11a720, 'tsi', wm)[0],
	SDTest.Call(0x11a740, 'tii', wm)[0]))
check('cWeaponManager_LockPoint', lambda: (SDTest.Call(0x11b2e0, 'tviF', wm, (1.5, -2.5, 3.25)), wmb(wm + 0x44),
	wmi(wm + 0x30), wmi(wm + 0x28) == wm + 0x28, SDTest.Call(0x11a720, 'tsi', wm)[0],
	SDTest.Call(0x11a740, 'tii', wm)[0] == wm + 0x38, wmfl(wm + 0x38, 3)))
check('cWeaponManager_LockEntity(NoSuch)', lambda: (SDTest.Call(0x11b2b0, 'tvis', wm, 'SDNoSuchEntity'),
	wmb(wm + 0x44), wmi(wm + 0x30), SDTest.Call(0x11a720, 'tsi', wm)[0], SDTest.Call(0x11a740, 'tii', wm)[0] != 0))
check('cWeaponManager_LockEntity(NULL)', lambda: (SDTest.Call(0x11b2b0, 'tvii', wm, 0), wmb(wm + 0x44),
	SDTest.Call(0x11a740, 'tii', wm)[0]))
check('cWeaponManager_LockPoint(NULL)', lambda: (SDTest.Call(0x11b2e0, 'tvii', wm, 0), wmb(wm + 0x44)))
for v in (1, 0x101, 0):
	check('cWeaponManager_SetFlag46' + fmt((v,)), lambda v=v: (SDTest.Call(0x11b360, 'tvii', wm, v), wmb(wm + 0x46)))

# the weapon sound (the fake owner has none): -1 without fire or ammo
for fire, a, snd in ((0, 5.0, 3), (1, 5.0, 3), (1, 0.0, 4), (1, 5.0, -1), (1, 5.0, 3)):
	wmsetbyte(wmown + 0x436, fire)
	SDTest.Call(0x11b280, 'tfiif', wm, wmammoof(wmi(wm + 0xc)), a)
	check('cWeaponManager_SetWeapSnd' + fmt((fire, a, snd)), lambda snd=snd: (SDTest.Call(0x11b6d0, 'tvii', wm, snd),
		wmi(wm + 0x24)))
check('cWeaponManager_NextWeapSnd', lambda: (SDTest.Call(0x11bf60, 'tvii', wm, 0), wmi(wm + 0x24)))
check('cWeaponManager_PlayStartSnd', lambda: (SDTest.Call(0x11c210, 'tvi', wm), wmi(wm + 0x24)))
wmsetbyte(wmown + 0x436, 0)
SDTest.Call(0x11b6d0, 'tvii', wm, -1)

# Update: the stamps and the ammo charge, no button changes (and fire with the stamps negative or no CanFire)
for i in range(wmna):
	SDTest.Call(0x11b280, 'tfiif', wm, i, 1.0 + 2.5 * i)
for fs, cs, dt, fire, cm, cur in ((0.0, 0.0, 0.04, 0, 0, -1), (-0.5, -0.25, 0.1, 0, 0, -1), (-3.0, -3.0, 0.5, 1, 0, -1),
		(2.0, 1.0, 0.0, 1, 0, -1), (-50.0, -50.0, 0.04, 0, 0, 1), (-50.0, -50.0, 0.04, 0, 0, -5),
		(-50.0, -50.0, 0.04, 0, 0, 1000), (0.0, 0.0, 1000.0, 0, 0, -1)):
	wmset(wm + 0x10, (fs, cs))
	if cur != -1:
		wmpoke(wm + 0xc, cur)
	wmsetbyte(wmown + 0x434, fire)
	wmsetbyte(wmown + 0x436, fire)
	wmsetbyte(wmown + 0x435, cm)
	wmsetbyte(wmown + 0x437, cm)
	check('cWeaponManager_Update' + fmt((fs, cs, dt, fire, cm, cur)), lambda dt=dt: (SDTest.Call(0x11b7b0, 'tvif',
		wm, dt), wmstate(), wmarrays()))
SDTest.Call(0x20f3f0, 'tvi', wmown + 0xb8)
check('cWeaponManager_Update no model', lambda: (SDTest.Call(0x11b7b0, 'tvif', wm, 0.1), wmstate()))
SDTest.Call(0x20ec10, 'tvi', wmown + 0xb8)
check('cWeaponManager_Update model freed', lambda: (SDTest.Call(0x11b7b0, 'tvif', wm, 0.1), wmstate()))
for p in (wmi(wm + 0x18), wmi(wm + 0x1c), wmi(wm + 0x20), wm, wmbuf, wmname, wmown, wmvt):
	wmfree(p)

# ---- a vehicle of our own for the native functions ----
wmcar = Scrap.CreateEntity('Car', 50000.0, 50000.0, 50000.0, 'SDWeapCar')
wmcar2 = Scrap.CreateEntity('Car', 50300.0, 50000.0, 50000.0, 'SDWeapCar2')
def wmentity(name):
	slot = SDTest.Call(0x131f70, 'tiis', wmworld + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', wmworld + 4, name, slot)[0]
	if node:
		return wmi(node)
	return 0
wmc = wmentity('SDWeapCar')
wmc2 = wmentity('SDWeapCar2')
check('SDWeapCar', lambda: (wmcar is not None, wmcar2 is not None, wmc != 0, wmc2 != 0))
def wmcall(func, params):
	Scrap.SetCallFunc(func)
	for p in params:
		if type(p) == type(''):
			Scrap.AddParams(p)
		elif type(p) == type(0):
			Scrap.AddParami(p)
		else:
			Scrap.AddParamf(p)
	Scrap.Execute()
if wmc and wmc2:
	cwm = wmc + 0x5f0
	check('SDWeapCar weapons', lambda: (wmi(cwm + 4) == wmc, wmints(cwm + 8, 2), wmfl(cwm + 0x10, 2), wmi(cwm + 0x24),
		wmi(wmc + 0x414)))
	# the aim help stays off (its Update searches the world from the vehicle)
	aimon = wmi(wmc + 0x71c)
	wmsetbyte(wmc + 0x71c, 0)
	def wmaim(c):
		return (wmi(c + 0x718), wmfl(c + 0x6d4, 2), wmfl(c + 0x6f4, 2))
	for f in ('!ATPCOnStart', '!ATPCOnEnd', '!DevastatorPress', '!DevastatorRelease'):
		check('native ' + f, lambda f=f: (wmcall(f, ('SDWeapCar',)), wmaim(wmc)))
	pad = SDTest.Peek(0x447ba4, 'i')
	wmset(0x847ba4, (1.0,))
	check('native !ATPCOnStart pad', lambda: (wmcall('!ATPCOnStart', ('SDWeapCar',)), wmaim(wmc)))
	check('Devastator_SetUp pad', lambda: (SDTest.Call(0x120540, 'vi', wmc), wmaim(wmc)))
	wmpoke(0x847ba4, pad)
	check('Devastator_SetUp', lambda: (SDTest.Call(0x120540, 'vi', wmc), wmaim(wmc)))
	# the cloud on a vehicle
	for strong in (0, 1, 7):
		check('native !CloudOnHit' + fmt((strong,)), lambda strong=strong: (wmcall('!CloudOnHit', ('SDCloud', 'x',
			'SDWeapCar', strong)), wmfl(wmc + 0x590, 1), wmb(wmc + 0x594)))
	check('native !CloudOnHit none', lambda: wmcall('!CloudOnHit', ('SDCloud', 'x', 'SDNoSuchEntity', 1)))
	# the EMI: SDWeapCar is the EMI, SDWeapCar2 300 away the target
	emir = wmfl(wmc + 0x5a4, 1)
	emit = wmfl(wmc2 + 0x58c, 1)
	for r, t, seed in ((1000.0, 0.0, 1), (100.0, 0.0, 1), (100.0, 0.0, 2), (100.0, 0.0, 3), (100.0, 0.5, 4),
			(100.0, 3.0, 5), (100.0, 8.0, 6), (290.0, 0.0, 7), (0.0, 0.0, 8)):
		wmset(wmc + 0x5a4, (r,))
		wmset(wmc2 + 0x58c, (t,))
		wmsrand(seed)
		check('native !EMIOnHit' + fmt((r, t, seed)), lambda: (wmcall('!EMIOnHit', ('SDWeapCar', 'x', 'SDWeapCar2')),
			wmfl(wmc2 + 0x58c, 1)))
	check('native !EMIOnHit self', lambda: (wmcall('!EMIOnHit', ('SDWeapCar', 'x', 'SDWeapCar')),
		wmfl(wmc + 0x58c, 1)))
	check('native !EMIOnHit none', lambda: wmcall('!EMIOnHit', ('SDNoSuchEntity', 'x', 'SDWeapCar')))
	wmset(wmc + 0x5a4, emir)
	wmset(wmc2 + 0x58c, emit)

	# the HUD items of ATPC and Devastator on a Text item of player 0
	SScorer.New(0, 'SDWeapItem', 'Text')
	item = SDTest.Call(0x170dc0, 'tiis', SDTest.Call(0x170ea0, 'ii', 0)[0], 'SDWeapItem')[0]
	check('SDWeapItem', lambda item=item: item != 0)
	atpcw = SDTest.Peek(0x4487e8, 'i')
	devw = SDTest.Peek(0x448830, 'i')
	cws = wmi(cwm + 0x18)
	if item:
		def wmitem(item):
			return (wmfl(item + 0xb4, 3), wmi(item + 0xc0))
		saved = wmints(item + 0xb4, 4)
		for up in (0, 9):
			wmpoke(cws + 12 * atpcw + 4, up)
			for fs in (-5.0, -2.5, -1.0, -0.3, -0.25, -0.2, -0.1, -0.01, 0.0, 0.5):
				wmset(cwm + 0x10, (fs,))
				check('native !ATPCFX' + fmt((up, fs)), lambda item=item: (wmcall('!ATPCFX', ('SDWeapCar', 0,
					'SDWeapItem')), wmitem(item)))
		wmpoke(cws + 12 * atpcw + 4, 0)
		check('native !ATPCFX no item', lambda: wmcall('!ATPCFX', ('SDWeapCar', 0, 'SDNoItem')))
		sc = SDTest.Call(0x170ea0, 'ii', 0)[0]
		sctime = wmints(sc + 0x20, 3)
		for up, fire, ang, t, dt in ((0, 0, 1.0, 0.0, 0.04), (0, 0, 0.01, 0.0, 0.04), (0, 0, -0.01, 0.0, 0.04),
				(0, 0, -2.0, 0.0, 0.04), (0, 0, 7.0, 0.0, 0.1), (8, 1, 1.0, 0.0, 0.04), (8, 0, 1.0, 0.0, 0.04),
				(7, 1, 0.0, 0.3, 0.04), (7, 1, 0.0, 1.7, 0.04), (7, 1, 0.0, 12.5, 0.04), (7, 0, 0.5, 1.7, 0.04)):
			wmpoke(cws + 12 * devw + 4, up)
			wmsetbyte(wmc + 0x436, fire)
			wmset(item + 0xb4, (ang,))
			wmset(sc + 0x20, (t,))
			wmset(sc + 0x28, (dt,))
			check('native !DevastatorFX' + fmt((up, fire, ang, t, dt)), lambda item=item: (wmcall('!DevastatorFX',
				('SDWeapCar', 0, 'SDWeapItem')), wmitem(item)))
		for i in range(3):
			wmpoke(sc + 0x20 + 4 * i, sctime[i])
		wmpoke(cws + 12 * devw + 4, 0)
		wmsetbyte(wmc + 0x436, 0)
		for i in range(4):
			wmpoke(item + 0xb4 + 4 * i, saved[i])

	# the cdecl helpers: the ATPC recharge and the Devastator start (the car has no sounds)
	atpca = SDTest.Peek(0x448e4c, 'i')
	deva = SDTest.Peek(0x449108, 'i')
	for up, a in ((0, 0.0), (0, 10.0), (9, 10.0), (9, 0.0), (10, 10.0)):
		wmpoke(cws + 12 * atpcw + 4, up)
		wmset(wmi(cwm + 0x20) + 4 * atpca, (a,))
		wmset(cwm + 0x10, (1.0,))
		check('ATPC_SetRecharge' + fmt((up, a)), lambda: (SDTest.Call(0x11c310, 'vi', wmc), wmfl(cwm + 0x10, 1)))
	wmpoke(cws + 12 * atpcw + 4, 0)
	for slots, a, fs, snd in ((0, 5.0, 0.0, -1), (1, 0.0, 0.0, -1), (1, 5.0, 0.0, -1), (3, 5.0, 0.0, -1),
			(0xf, 5.0, 0.5, -1), (0xf, 5.0, -0.2, 2), (0xf, 5.0, 0.01, -1)):
		wmpoke(cws + 12 * devw, slots)
		wmset(wmi(cwm + 0x20) + 4 * deva, (a,))
		wmset(cwm + 0x10, (fs,))
		wmpoke(cwm + 0x24, snd)
		check('Devastator_Start' + fmt((slots, a, fs, snd)), lambda: (SDTest.Call(0x11e150, 'vi', wmc),
			wmfl(cwm + 0x10, 1), wmi(cwm + 0x24), wmaim(wmc)))
	wmpoke(cws + 12 * devw, 0)

	# the explosions on SDWeapCar2 (the shooters do not exist); its life, velocity and push are read
	def wmvel(c):
		return SDTest.Call(wmi(wmi(c) + 0x88) - 0x400000, 'tiiF', c, (0.0, 0.0, 0.0))[1]
	def wmhit(c):
		h = wmi(c + 0x4fc)
		if h:
			return wmi(h + 0x64)
		return None
	cp = SDTest.Call(wmi(wmi(wmc2) + 0x18) - 0x400000, 'tiiF', wmc2, (0.0, 0.0, 0.0))[1]
	check('SDWeapCar2', lambda: (cp, wmi(wmc2 + 0x238), wmb(wmc2 + 0x23c), wmfl(wmc2 + 0x230, 1),
		wmfl(wmc2 + 0x3a0, 1), wmvel(wmc2), wmhit(wmc2)))
	shooter = SDTest.Peek(0x448a90, 's')
	victim = SDTest.Peek(0x448b10, 's')
	slotsv = wmints(0x848b90, 1)
	wmstr(0x848a90, '')
	for ns, dx, dist, seed in ((1.0, 0.0, 0.5, 3), (2.0, 10.0, 30.0, 3), (4.0, 100.0, 120.0, 3),
			(3.0, 0.0, 0.0, 9), (1.0, -200.0, 5000.0, 3)):
		wmset(0x848b90, (ns,))
		wmsrand(seed)
		check('native !ATPCXplosionCall' + fmt((ns, dx, dist, seed)), lambda dx=dx, dist=dist: (
			wmcall('!ATPCXplosionCall', ('SDWeapCar2', cp[0] - dx, cp[1] + 7.0, cp[2] - 3.0, dist)),
			wmi(wmc2 + 0x238), wmvel(wmc2), wmhit(wmc2), SDTest.Peek(0x448b10, 's')))
	check('native !ATPCXplosionCall none', lambda: (wmcall('!ATPCXplosionCall', ('SDNoSuchEntity', 0.0, 0.0, 0.0,
		1.0)), SDTest.Peek(0x448b10, 's')))
	wmpoke(0x848b90, slotsv[0])
	wmstr(0x848a90, shooter)
	wmstr(0x848b10, victim)
	devs = SDTest.Peek(0x448f48, 's')
	devv = SDTest.Peek(0x448fc8, 's')
	center = wmfl(0x849258, 3)
	wmstr(0x848f48, '')
	for dx, dy, dz, seed in ((0.0, 0.0, 1.0, 4), (0.0, 0.0, 0.0, 4), (0.0, 0.5, 0.0, 6), (0.3, 0.0, 1.0, 4),
			(50.0, 20.0, 1.0, 4), (300.0, -100.0, 1.0, 4), (5000.0, 0.0, 1.0, 4)):
		wmset(0x849258, (cp[0] - dx, cp[1] - dy, cp[2] + dz))
		wmsrand(seed)
		check('native !DevastatorXplosionCall' + fmt((dx, dy, dz, seed)), lambda: (
			wmcall('!DevastatorXplosionCall', ('SDWeapCar2',)), wmi(wmc2 + 0x238), wmvel(wmc2), wmhit(wmc2),
			SDTest.Peek(0x448fc8, 's')))
	check('native !DevastatorXplosionCall none', lambda: wmcall('!DevastatorXplosionCall', ('SDNoSuchEntity',)))
	wmset(0x849258, center)
	wmstr(0x848f48, devs)
	wmstr(0x848fc8, devv)
	wmpoke(wmc + 0x71c, aimon)
if wmcar:
	check('SDWeapCar Del', lambda: wmcar.Del())
if wmcar2:
	check('SDWeapCar2 Del', lambda: wmcar2.Del())
