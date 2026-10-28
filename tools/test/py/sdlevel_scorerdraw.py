# game Scorer/WindFxScorer.c, WeaponScorer.c, StatusScorer.c, TargetScorer.c and ScraplandScorer.c, the large HUD
# draws (execfile'd by sdlevel.py, Python 1.5.2): items of our own added through SScorer.Add for player 0 and set up
# the way the HUD script of the game does (its sprite sheets and frame indices). The wind, weapon and status draws
# run on the cars of our own made the target of the camera MainCam for the call (its target put back right after),
# the target mark on a second and a third car, the arrow to the attacker, the main menu logo on its sheets with the
# animation state set by hand. The draws go through the D3D log with the viewport 640x480. The scorer times, the
# engine variables, the camera target, the game time, the menu state and the font are saved and put back; the
# sounds are stopped, the items marked DelMe (the scorer removes them in its next frame) and the cars deleted.
def scdI(a):
	return SDTest.Peek(a, '@i')
def scdB(a):
	return SDTest.Peek(a, '@b')
def scdF(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def scdSet(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def scdPoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def scdPokeByte(a, v):
	scdPoke(a, (scdI(a) & ~0xff) | v)
def scdInts(a, n):
	r = ()
	for scdK in range(n):
		r = r + (scdI(a + 4 * scdK),)
	return r
def scdSrand(s):
	SDTest.Call(0x323c50, 'vi', s)
def scdNew(n):
	return SDTest.Call(0x1334d0, 'ii', n)[0]
def scdFree(p):
	SDTest.Call(0x1334e0, 'vi', p)
def scdCopy(dst, src, n):
	SDTest.Call(0x3197d0, 'iiii', dst, src, n)
# a heap copy of n bytes at the address a, put back with scdPutback
def scdKeep(a, n):
	b = scdNew(n)
	scdCopy(b, a, n)
	return (a, b, n)
def scdPutback(k):
	scdCopy(k[0], k[1], k[2])
	scdFree(k[1])
# a call with the D3D device replaced by the logging proxy: (its result, the D3D calls); the 2D draws CHECK the
# viewport, set to 640 x 480 for the call
def scdLog(fn):
	vp = (SDTest.Peek(0x4ad7e8, 'i'), SDTest.Peek(0x4ad7ec, 'i'))
	SDTest.Poke(0x4ad7e8, 640)
	SDTest.Poke(0x4ad7ec, 480)
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	SDTest.Poke(0x4ad7e8, vp[0])
	SDTest.Poke(0x4ad7ec, vp[1])
	return (r, SDTest.Out())
scdWorld = SDTest.Peek(0x467450, 'i')
scdSc = SDTest.Call(0x170ea0, 'ii', 0)[0]
def scdFind(name):
	slot = SDTest.Call(0x131f70, 'tiis', scdWorld + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', scdWorld + 4, name, slot)[0]
	if node:
		return scdI(node)
	return 0
def scdItem(name):
	return SDTest.Call(0x170dc0, 'tiis', scdSc, name)[0]
def scdSetVars(name, sets):
	for scdV in sets:
		SScorer.Set(0, name, scdV[0], scdV[1])
# a string into a char array (with its zero)
def scdStr(a, s):
	SDTest.Call(0x3197d0, 'iisi', a, s, len(s) + 1)

# ---- the cars, the camera, the items ----
# the second car in front of the camera (it looks along +z), the first and the third far away
scdPP = Scrap.UsrEntity(0).Pos
scdCP = scdPP
if Scrap.GetEntity(Scrap.GetCam(0)):
	scdCP = Scrap.GetEntity(Scrap.GetCam(0)).Pos
scdC2P = (scdCP[0] + 100.0, scdCP[1] - 50.0, scdCP[2] + 2000.0)
scdCarObj = Scrap.CreateEntity('SDScdCar', 54000.0, 50000.0, 61000.0, 'Car')
scdCar2Obj = Scrap.CreateEntity('SDScdCar2', scdC2P[0], scdC2P[1], scdC2P[2], 'Car')
scdCar3Obj = Scrap.CreateEntity('SDScdCar3', 54300.0, 50000.0, 61000.0, 'Car')
scdCar = scdFind('SDScdCar')
scdCar2 = scdFind('SDScdCar2')
scdCar3 = scdFind('SDScdCar3')
check('SCD cars', lambda: (scdCarObj is not None, scdCar2Obj is not None, scdCar3Obj is not None, scdCar != 0,
	scdCar2 != 0, scdCar3 != 0))
scdCamName = SDTest.Call(0x209220, 'tsii', scdWorld, 0)[0]
scdCam = 0
if scdCamName:
	scdCam = scdFind(scdCamName)
scdCamOk = 0
if scdCam and Scrap.GetEntity(scdCamName).IsType('FCam'):
	scdCamOk = 1
check('SCD camera', lambda: (scdCamName, scdCamOk, scdSc != 0))
# where the cars are on the screen
for scdK in (scdC2P, (54300.0, 50000.0, 61000.0)):
	check('SCD Engine_WorldToScreen' + fmt(scdK), lambda p=scdK: SDTest.Call(0x216d60, 'bFFF', p, (0.0, 0.0),
		(0.0,)))
scdCamTarget = 0
if scdCamOk:
	scdCamTarget = scdI(scdCam + 0x13c)
# fn with e the target of the camera (0: the one of the level)
def scdWith(e, fn):
	if e:
		scdPoke(scdCam + 0x13c, e)
	r = fn()
	scdPoke(scdCam + 0x13c, scdCamTarget)
	return r
scdOk = scdSc and scdCamOk and scdCar and scdCar2 and scdCar3
scdScSaved = ()
if scdSc:
	scdScSaved = scdInts(scdSc + 0x20, 3) + (scdI(scdSc + 0x50),)
	scdSet(scdSc + 0x20, (12.5, 12.25, 0.05))
scdGvars = (0x467644, 0x4624f4, 0x467584)
scdGsaved = map(lambda a: SDTest.Peek(a, 'i'), scdGvars)
scdTypes = (('scdWind', 'WindFx'), ('scdWeap', 'Weapon'), ('scdStat', 'Status'), ('scdTarg', 'Target'),
	('scdMain', 'ScraplandMainMenu'), ('scdCur', 'Sprite'), ('scdRad', 'Radar'))
scdItems = {}
scdSrand(1717)
for scdName, scdType in scdTypes:
	SScorer.Add(0, scdName, scdType)
	scdItems[scdName] = 0
	if scdSc:
		scdItems[scdName] = scdItem(scdName)
	check('SCD item ' + scdName, lambda a=scdItems[scdName]: a and scdI(a) - 0x400000)

# ---- the wind: the free space around the car by four line tests (high in the air: no hits; near the player) ----
scdA = scdItems['scdWind']
if scdA and scdOk:
	def scdWind(a, car, speed, ang, atmo, show):
		scdSet(a + 0xb0, (0.1, 0.5, 0.9, 0.3))
		scdSet(car + 0x4c4, (speed,))
		scdSet(car + 0x4cc, ang)
		scdSet(0x867644, (atmo,))
		SDTest.Poke(0x4624f4, show)
		d = scdLog(lambda a=a, car=car: scdWith(car, lambda a=a: SDTest.Call(0x188ea0, 'tvii', a, 0)))
		return (d, scdF(a + 0xb0, 4), scdI(a + 0xc0), scdI(a + 0xc4), scdF(a + 0xc8, 1))
	for scdV in ((1, 14000.0, (0.0, 0.0, 0.0), 1.0e6, 0), (1, 14000.0, (0.0, 0.0, 0.0), 1000.0, 1),
			(1, 28000.0, (0.4, -1.1, 0.0), 1.0e6, 1), (1, 10.0, (0.0, 0.5, 0.0), 1.0e6, 1),
			(2, 14000.0, (0.0, 0.0, 0.0), 1.0e6, 1), (2, 28000.0, (0.3, 1.9, 0.0), 1.0e6, 1),
			(2, 5600.0, (-0.2, 2.5, 0.0), 1.0e6, 0), (2, 42000.0, (1.2, -0.7, 0.0), 1.0e6, 1)):
		scdC = (0, scdCar, scdCar2)[scdV[0]]
		check('SCD cWindFxScorer_Draw' + fmt(scdV), lambda a=scdA, c=scdC, v=scdV: scdWind(a, c, v[1], v[2], v[3],
			v[4]))
	SDTest.Poke(0x467644, scdGsaved[0])
	SDTest.Poke(0x4624f4, scdGsaved[1])
	SDTest.Call(0x1a4390, 'tvi', scdA + 0xc0)

# ---- the weapon: the HUD sheets of the game, the weapons of the car, the slot row and the fade of a change ----
scdA = scdItems['scdWeap']
if scdA and scdOk:
	scdSetVars('scdWeap', (('Sprite', '2D/HUD/HUD.alpha.tga'), ('X', 30.0), ('Y', 26.0), ('BackOffX', 0.0),
		('BackOffY', 0.0), ('BackIndex', 1), ('Weapons3DSprite', '2D/HUD/WeaponIcons3D.alpha.tga'),
		('WeaponIconOffX', 13.0), ('WeaponIconOffY', 18.0), ('WeaponIconShadowOffX', 4.0),
		('WeaponIconShadowOffY', 4.0), ('WeaponIconAc', 6), ('WeaponIconIndex', 20), ('WeaponIconAc', 0),
		('WeaponIconIndex', 1), ('WeaponIconAc', 3), ('WeaponIconIndex', 4), ('WeaponIconAc', 2),
		('WeaponIconIndex', 7), ('WeaponIconAc', 4), ('WeaponIconIndex', 10), ('WeaponIconAc', 1),
		('WeaponIconIndex', 13), ('WeaponIconAc', 5), ('WeaponIconIndex', 16), ('UpgradeBarOffX', 6.5),
		('UpgradeBarOffY', 11.5), ('UpgradeBarIndex', 3), ('UpgradeBarRed', 251), ('UpgradeBarGreen', 241),
		('UpgradeBarBlue', 92), ('UpgradeBarAlpha', 128), ('UpgradeIconAc', 0), ('UpgradeIconIndex', 0),
		('UpgradeIconAc', 1), ('UpgradeIconIndex', 2), ('UpgradeIconAc', 2), ('UpgradeIconIndex', 3),
		('UpgradeIconAc', 7), ('UpgradeIconIndex', 5), ('UpgradeIconAc', 8), ('UpgradeIconIndex', 6),
		('UpgradeIconAc', 5), ('UpgradeIconIndex', 9), ('UpgradeIconAc', 6), ('UpgradeIconIndex', 8),
		('UpgradeIconAc', 9), ('UpgradeIconIndex', 11), ('UpgradeIconAc', 10), ('UpgradeIconIndex', 12),
		('UpgradeIconAc', 3), ('UpgradeIconIndex', 14), ('UpgradeIconAc', 4), ('UpgradeIconIndex', 15),
		('UpgradeIconAc', 11), ('UpgradeIconIndex', 17), ('UpgradeIconAc', 12), ('UpgradeIconIndex', 18),
		('AmmoBackIndex', 27), ('AmmoBackOffX', 68.0), ('AmmoBackOffY', 12.5), ('AmmoBarOffX', 61.5),
		('AmmoBarOffY', 37.0), ('AmmoBarIndex', 6), ('AmmoBarRed', 200), ('AmmoBarGreen', 210), ('AmmoBarBlue', 220),
		('AmmoBarAlpha', 230), ('AmmoTextOffX', 58.0), ('AmmoTextOffY', 66.0), ('AmmoIconOffX', 68.0),
		('AmmoIconOffY', 11.0), ('AmmoIconAc', 0), ('AmmoIconIndex', 10), ('AmmoIconAc', 1), ('AmmoIconIndex', 12),
		('AmmoIconAc', 2), ('AmmoIconIndex', 11), ('AmmoIconAc', 3), ('AmmoIconIndex', 9), ('AmmoIconAc', 4),
		('AmmoIconIndex', 0), ('AmmoFont', 'BigNumbers'), ('AmmoTextRed', 251), ('AmmoTextGreen', 241),
		('AmmoTextBlue', 174), ('AmmoTextAlpha', 255), ('FontBackIndex', 26), ('FontBackOffX', 36.0),
		('FontBackOffY', 67.0), ('SlotsOffX', 36.0), ('SlotsOffY', 2.0), ('SlotsIndex0', 7), ('SlotsIndex1', 8),
		('TabBlendingMode', 3), ('TabSprite', '2D/Menu/Tabs.alpha.tga'), ('TabSpriteIndex', 1), ('TabAlpha', 255),
		('TabRed', 255), ('TabGreen', 255), ('TabBlue', 192)))
	check('SCD Weapon sheets', lambda a=scdA: (scdI(scdI(a + 0xac) + 0x3c) != 0, scdI(scdI(a + 0xb0) + 0x3c) != 0,
		scdI(scdI(a + 0x1b8) + 0x3c) != 0))
	scdWm = scdCar + 0x5f0
	scdNW = scdI(scdWorld + 0x1bb8)
	scdNA = scdI(scdWorld + 0x1bc4)
	scdWT = scdI(scdWorld + 0x1bbc)
	scdKinds = ()
	for scdK in range(7):
		scdKinds = scdKinds + (scdI(scdWT + scdK * 0xb0 + 0x98),)
	check('SCD weapon table', lambda: (scdNW, scdNA, scdKinds, scdI(scdWm + 0x18) != 0, scdI(scdWm + 0x20) != 0))
	scdCharged = -1
	for scdK in range(7):
		if scdKinds[scdK] == 3 and scdCharged < 0:
			scdCharged = scdK
	if scdNW >= 7 and scdNA >= 4 and scdI(scdWm + 0x18) and scdI(scdWm + 0x20):
		scdWeaps = scdKeep(scdI(scdWm + 0x18), 7 * 0xc)
		scdAmmo = scdKeep(scdI(scdWm + 0x20), 4 * scdNA)
		scdSlotG = scdKeep(0x862460, 0x3c)
		scdCarW = (scdI(scdWm + 0xc), scdI(scdCar + 0x414), scdI(scdCar + 0x238))
		# the slots of the seven weapons, their upgrades and the ammo of each kind
		for scdK in range(7):
			scdPoke(scdI(scdWm + 0x18) + scdK * 0xc, (5, 1, 0, 3, 15, 0, 2)[scdK])
			scdPoke(scdI(scdWm + 0x18) + scdK * 0xc + 4, (0, 2, 0, 7, 0, 11, 0)[scdK])
			scdSet(scdI(scdWm + 0x18) + scdK * 0xc + 8, ((0.0, 150.0, 0.0, 60.0, 0.0, 200.0, 0.0)[scdK],))
		for scdK in range(scdNA):
			scdSet(scdI(scdWm + 0x20) + 4 * scdK, (7.0 + 31.0 * scdK,))
		def scdWeap(a, w, sel, life, last, change, slots, split, time, ammo):
			scdPoke(scdWm + 0xc, w)
			scdPoke(scdCar + 0x414, sel)
			scdPoke(scdCar + 0x238, life)
			scdPoke(a + 0x1a4, last[0])
			scdPoke(a + 0x1a8, last[1])
			scdSet(a + 0x1a0, (change,))
			scdSet(a + 0x1ac, (slots,))
			SDTest.Poke(0x467584, split)
			scdSet(scdSc + 0x20, (time,))
			if ammo >= 0.0:
				scdSet(scdI(scdWm + 0x20) + 4 * scdKinds[w], (ammo,))
			d = scdLog(lambda a=a: scdWith(scdCar, lambda a=a: SDTest.Call(0x188050, 'tvii', a, 0)))
			return (d, scdF(a + 0x1a0, 1), scdInts(a + 0x1a4, 2), scdF(a + 0x1ac, 1), scdInts(0x862460, 15))
		scdCases = [(0, 0, 100, (-100, -100), 0.0, 0.0, 0, 12.5, -1.0), (1, 1, 100, (1, 2), 0.0, 0.0, 0, 12.5, -1.0),
			(3, 3, 100, (1, 2), 0.0, 0.5, 0, 12.5, -1.0), (3, 3, 0, (1, 2), 0.12, 0.05, 0, 12.5, -1.0),
			(6, 6, 100, (3, 7), 0.12, 2.0, 0, 12.5, 0.0), (6, 6, 100, (6, 0), 0.0, 2.0, 0, 12.55, 0.0),
			(5, 2, 100, (5, 11), 0.0, 1.0, 1, 12.5, 0.5), (2, 4, 0, (2, 0), 0.0, 0.0, 0, 12.5, -1.0),
			(4, 4, 100, (0, 0), 0.05, 0.2, 1, 12.5, 2.0)]
		if scdCharged >= 0:
			scdCases.append((scdCharged, scdCharged, 100, (scdCharged, 0), 0.0, 0.0, 0, 12.5, 30.0))
			scdCases.append((scdCharged, scdCharged, 100, (scdCharged, 0), 0.0, 0.0, 0, 12.5, 500.0))
		for scdV in scdCases:
			check('SCD cWeaponScorer_Draw' + fmt(scdV), lambda a=scdA, v=scdV: scdWeap(a, v[0], v[1], v[2], v[3], v[4],
				v[5], v[6], v[7], v[8]))
		scdPoke(scdWm + 0xc, scdCarW[0])
		scdPoke(scdCar + 0x414, scdCarW[1])
		scdPoke(scdCar + 0x238, scdCarW[2])
		scdPutback(scdWeaps)
		scdPutback(scdAmmo)
		scdPutback(scdSlotG)
		SDTest.Poke(0x467584, scdGsaved[2])
		scdSet(scdSc + 0x20, (12.5,))

# ---- the status: life, boost segments, money, lives, the team flag and the hull; the warning and siren sounds ----
scdA = scdItems['scdStat']
if scdA and scdOk:
	scdSetVars('scdStat', (('X', 600.0), ('Y', 26.0), ('Sprite', '2D/HUD/HUD.alpha.tga'), ('BackOffX', -84.0),
		('BackOffY', 0.0), ('BackIndex', 2), ('LifeIconIndex', 14), ('LifeIconOffX', -46.0), ('LifeIconOffY', 20.0),
		('LifeTextOffX', -58.0), ('LifeTextOffY', 66.0), ('LifeFont', 'BigNumbers'), ('LifeTextRed', 232),
		('LifeTextGreen', 99), ('LifeTextBlue', 255), ('LifeTextAlpha', 255), ('FontBackIndex', 26),
		('FontBackOffX', -61.0), ('FontBackOffY', 65.0), ('LivesBackIndex', 25), ('LivesBackOffX', -10.5),
		('LivesBackOffY', 6.0), ('LivesTextOffX', -6.0), ('LivesTextOffY', -10.0), ('LivesFont', 'Horatio'),
		('LivesTextRed', 251), ('LivesTextGreen', 188), ('LivesTextBlue', 239), ('LivesTextAlpha', 255),
		('MoneyIconOffX', -85.0), ('MoneyIconOffY', 8.0), ('MoneyIconIndex', 15), ('MoneyTextOffX', -86.0),
		('MoneyTextOffY', 8.0), ('MoneyFont', 'Horatio'), ('MoneyTextRed', 143), ('MoneyTextGreen', 255),
		('MoneyTextBlue', 82), ('MoneyTextAlpha', 255), ('MoneyBackIndex', 25), ('MoneyBackOffX', -80.5),
		('MoneyBackOffY', 24.0), ('HullIconOffX', -54.0), ('HullIconOffY', 13.0), ('HullIconIndex', 13),
		('BoostIconIndex', 18), ('BoostTurboIndex', 19), ('BoostOffX', 0.0), ('BoostOffY', 0.0),
		('SpriteFlag', '2D/HUD/FlagColor.alpha.tga'), ('FlagOffX', -129.0), ('FlagOffY', 12.0)))
	check('SCD Status sheets', lambda a=scdA: (scdI(scdI(a + 0x130) + 0x3c) != 0, scdI(scdI(a + 0x134) + 0x3c) != 0,
		scdI(scdI(a + 0x1e8) + 0x3c) != 0))
	scdRegen = scdWorld + 0x2150
	scdRegenSaved = scdI(scdRegen)
	scdCarS = scdInts(scdCar + 0x238, 1) + scdInts(scdCar + 0x63c, 2) + scdInts(scdCar + 0x388, 1)
	scdMoney = scdI(scdWorld + 0x2090)
	def scdStat(a, target, life, lo, hi, team, boost, regen, alarm, last, blink, money, time):
		if target == scdCar:
			scdPoke(scdCar + 0x238, life)
			scdPoke(scdCar + 0x63c, lo)
			scdPoke(scdCar + 0x640, hi)
			scdPoke(scdCar + 0x388, team)
		scdPoke(a + 0x1cc, boost[0])
		scdSet(a + 0x1d0, (boost[1], boost[2]))
		scdPokeByte(scdRegen, regen)
		scdPokeByte(a + 0x12c, alarm)
		scdPoke(a + 0x1ac, scdMoney + last)
		scdSet(a + 0x1b0, (blink,))
		SScorer.Set(0, 'scdStat', 'MoneyIconIndex', money)
		scdSet(scdSc + 0x20, (time,))
		d = scdLog(lambda a=a, t=target: scdWith(t, lambda a=a: SDTest.Call(0x17d610, 'tvii', a, 0)))
		return (d, scdInts(a + 0xac, 2), scdInts(a + 0xec, 2), scdF(a + 0xf4, 1), scdB(a + 0x12c), scdI(a + 0x160),
			scdI(a + 0x1a8) - scdMoney, scdI(a + 0x1ac) - scdMoney, scdF(a + 0x1b0, 1), scdF(a + 0x1d4, 1))
	for scdV in ((scdCar, 100, 20, 100, 0, (0xff, 0.0, 0.25), 0, 0, -1, 0.0, 15, 12.5),
			(scdCar, 60, 20, 100, 1, (0x80, 0.4, 1.0), 1, 0, 0, 0.3, 15, 12.5),
			(scdCar, 60, 20, 100, 1, (0xff, 0.0, 0.8), 1, 1, 0, 0.3, 15, 12.65),
			(scdCar, 20, 20, 100, 2, (0x40, 0.3, 1.0), 0, 1, 0, 0.2, 15, 12.5),
			(scdCar, 8, 20, 100, 2, (0, 0.6, 1.0), 0, 0, 0, 0.0, 0, 12.5),
			(scdCar, 8, 20, 100, 0, (0x40, 0.95, 1.0), 0, 0, 0, 0.0, 0, 12.75),
			(scdCar, 0, 0, 100, 0, (0xff, 0.0, 1.6), 0, 0, 0, -0.1, 0, 12.5),
			(scdCar, 150, 20, 100, 1, (0xff, 0.0, 3.3), 0, 0, 0, 0.0, 15, 12.5),
			(scdCar, 50, 50, 50, 1, (0x80, 1.0, 1.0), 0, 0, 0, 0.0, 15, 12.5), (0, 0, 0, 0, 0, (0xff, 0.0, 0.5), 0,
			0, 0, 0.0, 15, 12.5)):
		check('SCD cStatusScorer_Draw' + fmt(scdV[1:]), lambda a=scdA, v=scdV: scdStat(a, v[0], v[1], v[2], v[3],
			v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]))
	scdPoke(scdCar + 0x238, scdCarS[0])
	scdPoke(scdCar + 0x63c, scdCarS[1])
	scdPoke(scdCar + 0x640, scdCarS[2])
	scdPoke(scdCar + 0x388, scdCarS[3])
	scdPoke(scdRegen, scdRegenSaved)
	scdSet(scdSc + 0x20, (12.5,))
	SDTest.Call(0x1a4390, 'tvi', scdA + 0xac)
	SDTest.Call(0x1a4390, 'tvi', scdA + 0xec)

# ---- the target: the mark of the target of the car (rectangle in view, arrow out of it) and the attacker arrow ----
scdA = scdItems['scdTarg']
if scdA and scdOk:
	scdSetVars('scdTarg', (('Sprite', '2D/Racer/Target.alpha.tga'), ('TargetRectIndex', 38), ('TargetArrowIndex', 16),
		('EnemyFireIndex', 4)))
	check('SCD Target sheet', lambda a=scdA: scdI(scdI(a + 0x134) + 0x3c) != 0)
	scdTgtName = scdKeep(scdCar + 0x650, 0x80)
	scdNoFire = (scdF(scdCar2 + 0x598, 1), scdF(scdCar3 + 0x598, 1))
	scdBurn = scdF(scdCar + 0x294, 1)
	scdTimerT = scdF(scdCar + 0x118, 1)
	# the attacker: SDScdCar3 hit the car (its timer at 10)
	scdSet(scdCar + 0x118, (10.0,))
	SDTest.Call(0x0f9b60, 'tvis', scdCar + 0x298, 'SDScdCar3')
	check('SCD attacker', lambda: (SDTest.Peek(scdCar + 0x2f8 + 8, '@i') == scdCar3, scdF(scdCar + 0x308, 1)))
	def scdTarg(a, name, test, view, nofire, hit, burn, split, time):
		scdStr(scdCar + 0x650, name)
		scdSet(a + 0xac, (test, view))
		scdSet(scdCar2 + 0x598, (nofire,))
		scdSet(scdCar3 + 0x598, (nofire,))
		scdSet(scdCar + 0x308, (hit,))
		scdSet(scdCar + 0x294, (burn,))
		SDTest.Poke(0x467584, split)
		scdSet(scdSc + 0x20, (time,))
		d = scdLog(lambda a=a: SDTest.Call(0x181600, 'tvii', a, scdCar))
		img = scdI(a + 0x134)
		return (d, scdF(a + 0xac, 2), SDTest.Peek(a + 0xb4, '@s'), scdB(scdCar2 + 0x385), scdB(scdCar3 + 0x385),
			scdF(img, 6), scdI(img + 0x34))
	for scdV in (('SDScdCar2', 0.0, 0.0, 0.0, 0.0, 0.0, 0, 12.5), ('SDScdCar2', 1.0, 0.5, 0.0, 0.0, 0.0, 0, 12.5),
			('SDScdCar2', 0.0, 0.98, 0.0, 9.6, 0.0, 1, 12.55), ('SDScdCar3', 0.0, 0.3, 1.0, 9.8, 0.0, 0, 12.5),
			('SDScdCar3', 0.0, 0.0, 0.0, 9.3, 0.0, 1, 12.6), ('SDScdCar3', 1.0, 0.7, 0.0, 9.9, 1.0, 0, 12.5),
			('', 0.0, 0.0, 0.0, 9.5, 0.0, 0, 12.5), ('', 0.0, 0.0, 0.0, 8.0, 0.0, 0, 12.5),
			('SDScdNone', 0.0, 0.0, 0.0, 9.7, 0.0, 0, 12.5)):
		check('SCD cTargetScorer_DrawTarget' + fmt(scdV), lambda a=scdA, v=scdV: scdTarg(a, v[0], v[1], v[2], v[3],
			v[4], v[5], v[6], v[7]))
	# the attacker link off again (the hit by the car itself)
	SDTest.Call(0x0f9b60, 'tvis', scdCar + 0x298, 'SDScdCar')
	scdPutback(scdTgtName)
	scdSet(scdCar2 + 0x598, scdNoFire[0])
	scdSet(scdCar3 + 0x598, scdNoFire[1])
	scdSet(scdCar + 0x294, scdBurn)
	scdSet(scdCar + 0x118, scdTimerT)
	SDTest.Poke(0x467584, scdGsaved[2])
	scdSet(scdSc + 0x20, (12.5,))

# ---- the main menu logo on its sheets: the animation state and the game time set by hand ----
scdA = scdItems['scdMain']
scdB2 = scdItems['scdCur']
if scdA and scdB2 and scdSc:
	scdSetVars('scdMain', (('X', 320.0), ('Y', 20.0), ('FileLogo', '2D/Menu/LogoMainMenu.alpha.tga'),
		('FileGlow', '2D/Menu/LogoMainMenuGlow.tga'), ('FileNeon', '2D/Menu/LogoMainMenuNeon.tga'),
		('FileLighting', '2D/Menu/LogoMainMenuLighting.tga')))
	scdSetVars('scdCur', (('X', 105.0), ('Y', 170.0)))
	scdNeon = scdI(scdA + 0xc0)
	scdLight = scdI(scdA + 0xc4)
	scdNF = (0, 0)
	if scdI(scdNeon + 0x3c) and scdI(scdLight + 0x3c):
		scdNF = (SDTest.Call(0x22d280, 'tii', scdNeon)[0], SDTest.Call(0x22d280, 'tii', scdLight)[0])
	check('SCD MainMenu sheets', lambda: scdNF)
	scdSets = SDTest.Peek(0x44c12c, 'i')
	scdAcc = 0
	if scdSets and SDTest.Peek(0x44c128, 'i') > 0 and scdI(scdSets + 8) and scdI(scdSets + 4) > 10:
		scdAcc = scdI(scdSets + 8) + 10 * 0x7c + 0x18
	check('SCD menu accept action', lambda: scdAcc != 0)
	if scdNF[0] > 0x26 and scdNF[1] > 0x4f and scdAcc:
		scdAnim = scdKeep(0x860428, 0x22c)
		scdApp = SDTest.Peek(0x467ba0, 'i')
		scdTime = scdInts(scdApp + 0x20, 2)
		scdAccSaved = scdI(scdAcc)
		scdFont = SDTest.Peek(0x4a8918, 'i')
		scdFontK = scdKeep(scdFont, 0x34)
		def scdMainState(v):
			# the neon tubes: waits on and off, the next tube and the groups
			w = ()
			for k in range(21):
				w = w + ((0.3, -0.1, 0.02, 0.0, 0.6, 0.11, -0.4)[(k + v) % 7],)
			scdSet(0x8605f0, w)
			scdSet(0x860544, ((0.1, -0.05, 0.0)[v % 3],))
			scdSet(0x860594, ((0.2, -0.3, 0.04), (0.0, 0.5, -0.1), (0.3, 0.3, 0.0))[v % 3])
			# the letters: their waits, the time left of a lighting and its length
			scdSet(0x86042c, (0.5, 0.01, 1.2, 0.02, 0.9, 0.0, 0.3, 0.04, 0.7))
			scdSet(0x860450, ((0.0, 0.05, 0.1, 0.0, 0.15, 0.3, 0.06, 0.12, 0.2), (0.11, 0.0, 0.2, 0.08, 0.0, 0.05,
				0.25, 0.0, 0.17), (0.18, 0.09, 0.0, 0.2, 0.07, 0.15, 0.0, 0.21, 0.0))[v % 3])
			scdSet(0x860474, (0.3, 0.25, 0.28, 0.2, 0.32, 0.4, 0.3, 0.26, 0.22))
			# the sparks: wait, time left, length, first frame, direction
			sw = ()
			st = ()
			sl = ()
			for k in range(19):
				sw = sw + ((0.4, 0.02, -0.1, 1.5)[(k + v) % 4],)
				st = st + ((0.0, 0.3, 0.1, 0.0, 0.55)[(k + v) % 5],)
				sl = sl + ((0.6, 0.7, 0.65)[k % 3],)
			scdSet(0x860498, sw)
			scdSet(0x8604f8, st)
			scdSet(0x860548, sl)
			for k in range(19):
				scdPoke(0x8605a0 + 4 * k, (k * 5 + v) % 14)
			for k in range(0, 19, 4):
				scdPoke(0x8604e4 + k, (0x01000100, 0x00010001, 0x01010000)[(k / 4 + v) % 3])
		def scdMain(a, v, cursor, accept, t, last):
			scdMainState(v)
			scdPoke(scdSc + 0x50, cursor)
			scdSet(scdAcc, (accept,))
			# the game time: t as the high word of the double (the low word 0)
			scdPoke(scdApp + 0x20, 0)
			scdPoke(scdApp + 0x24, t)
			scdSet(a + 0xac, (last,))
			scdSrand(55 + v)
			d = scdLog(lambda a=a: SDTest.Call(0x176130, 'tvii', a, 0))
			return (d, scdF(a + 0xac, 2), scdF(0x86042c, 9), scdF(0x860450, 9), scdF(0x860474, 9),
				scdF(0x8605f0, 21), scdF(0x860544, 1), scdF(0x860594, 3), scdF(0x860498, 19), scdF(0x8604f8, 19),
				scdF(0x860548, 19), scdInts(0x8605a0, 19), scdInts(0x8604e4, 5))
		# 0x40590000: 100.0, 0x40591000: 100.25
		for scdV in ((0, 0, 0.0, 0x40590000, 99.95), (1, scdB2, 1.0, 0x40590000, 99.9), (2, scdB2, 0.25, 0x40591000,
				100.0), (3, scdB2, 1.0, 0x40591000, 90.0), (4, 0, 1.0, 0x40590000, 100.0)):
			check('SCD cScraplandMainMenuScorer_Draw' + fmt((scdV[0], scdV[1] != 0, scdV[2], scdV[3], scdV[4])),
				lambda a=scdA, v=scdV: scdMain(a, v[0], v[1], v[2], v[3], v[4]))
		scdPoke(scdApp + 0x20, scdTime[0])
		scdPoke(scdApp + 0x24, scdTime[1])
		scdPoke(scdAcc, scdAccSaved)
		scdPutback(scdAnim)
		scdPutback(scdFontK)

# ---- the radar: the icons of the entities, the view cones, the target of the car and the mission marks, over the
# floor of FZ and over two floors of our own; only our entities, the player and the static ones of FZ stay active
# for the calls (the AI ones move between the runs). A sprite less image fails the CHECK of CPsaImage_DrawPoly: the
# floor maps of FZ and the noise are missing, the view cone image stands in for them ----
scdA = scdItems['scdRad']
if scdA and scdOk:
	scdSetVars('scdRad', (('X', 100.0), ('Y', 50.0), ('RadarSprite', '2D/Radar/Radar.alpha.tga'), ('RadarIndex', 1),
		('MaskSprite', '2D/Radar/Radar.alpha.tga'), ('MaskIndex', 2), ('IconsSprite', '2D/Radar/Icons.alpha.tga'),
		('IconAc', 0), ('IconIndex', 18), ('IconAc', 1), ('IconIndex', 15), ('IconAc', 2), ('IconIndex', 26),
		('IconAc', 3), ('IconIndex', 29), ('IconAc', 4), ('IconIndex', 7), ('IconAc', 5), ('IconIndex', 23),
		('IconAc', 6), ('IconIndex', 37), ('IconAc', 7), ('IconIndex', 39), ('IconAc', 8), ('IconIndex', 4),
		('IconAc', 9), ('IconIndex', 40), ('IconAc', 10), ('IconIndex', 41), ('IconAc', 11), ('IconIndex', 42),
		('IconAc', 12), ('IconIndex', 36), ('IconAc', 13), ('IconIndex', 37), ('IconAc', 14), ('IconIndex', 38),
		('IconAc', 15), ('IconIndex', 16), ('IconAc', 16), ('IconIndex', 27), ('IconAc', 17), ('IconIndex', 27),
		('IconAc', 18), ('IconIndex', 35), ('IconAc', 19), ('IconIndex', 30), ('IconAc', 20), ('IconIndex', 28),
		('IconAc', 21), ('IconIndex', 31), ('IconAc', 22), ('IconIndex', 43)))
	scdCone = scdI(scdA + 0x8c8)
	scdRadOk = scdI(scdI(scdA + 0xbc) + 0x3c) and scdI(scdI(scdA + 0xc4) + 0x3c) and scdI(scdI(scdA + 0xcc) +
		0x3c) and scdI(scdCone + 0x3c)
	check('SCD Radar sheets', lambda: scdRadOk != 0)
scdRadAll = 0
if scdA and scdOk and scdRadOk:
	scdRadImgs = scdInts(scdA + 0x8c4, 1) + scdInts(scdA + 0x8dc, 16)
	scdPoke(scdA + 0x8c4, scdCone)
	for scdK in range(16):
		scdPoke(scdA + 0x8dc + 4 * scdK, scdCone)
	# the last colors of the icons are not set by the constructor
	for scdK in range(23):
		scdPoke(scdA + 0x4cc + 4 * scdK, 0)
	scdSet(scdA + 0xac, (55.0, 55.0, 56.25, 56.25))
	scdSet(scdA + 0x8d4, (1.0,))
	scdRCam = scdNew(0x100)
	scdSet(scdRCam + 0x8c, (0.7,))
	# the places and the mission objects of our own around the car
	scdRadEnts = (('DM_AreaToPark_SDScd', 'Obj', (54300.0, 50000.0, 61200.0)),
		('DM_OutLink_Scrapyard', 'Obj', (53800.0, 50000.0, 60900.0)),
		('ParkParking_Junkyard', 'item', (54100.0, 50000.0, 61600.0)),
		('DM_OutLink_Outskirts', 'item', (60000.0, 50000.0, 55000.0)),
		('DM_Door_Elevator_GamblinDen', 'WithLife', (53500.0, 50000.0, 61500.0)),
		('DM_OutLink_SDScd', 'Obj', (54200.0, 50000.0, 60500.0)),
		('SDScdObj1', 'Obj', (54150.0, 10000.0, 61150.0)), ('SDScdObj2', 'Obj', (53900.0, 50000.0, 61300.0)),
		('SDScdObj3', 'Obj', (54400.0, 10000.0, 60800.0)), ('SDScdLife', 'WithLife', (54050.0, 50000.0, 60850.0)),
		('SDScdObj4', 'Obj', (70000.0, 50000.0, 90000.0)))
	scdRadE = {}
	for scdN2, scdT2, scdP2 in scdRadEnts:
		scdRadE[scdN2] = Scrap.CreateEntity(scdN2, scdP2[0], scdP2[1], scdP2[2], scdT2)
	check('SCD radar entities', lambda: map(lambda x: (scdRadE[x[0]] is not None, scdFind(x[0]) != 0), scdRadEnts))
	scdRE = {}
	scdRadAll = 1
	for scdN2, scdT2, scdP2 in scdRadEnts:
		scdRE[scdN2] = scdFind(scdN2)
		if not scdRE[scdN2]:
			scdRadAll = 0
	# only our entities and the static ones of FZ active
	def scdActive(name):
		for p in ('SDScd', 'DM_AreaToPark_SDScd', 'DM_OutLink_', 'DM_Door_Elevator_GamblinDen', 'Player0',
				'PlayerShip', 'DtritusPlate', 'DM_Door_EditShip_00', 'Accelerator', 'net5', 'ParkParking_'):
			if name[:len(p)] == p:
				return 1
		return 0
	scdFlags = []
	scdE = SDTest.Call(0x2092a0, 'tii', scdWorld)[0]
	while scdE:
		scdNm = SDTest.Peek(scdE + 4, '@p')
		if scdNm is None or not scdActive(scdNm):
			scdFlags.append((scdE, scdI(scdE + 0x4c)))
			scdPokeByte(scdE + 0x4c, 0)
		scdE = SDTest.Call(0x0ef310, 'tii', scdE)[0]
	check('SCD radar inactive', lambda: len(scdFlags))
	# the engine variables, the view of the radar, the floors
	scdRGvars = (0x45fe78, 0x45fe00, 0x45fe48, 0x467ad4, 0x4678d8, 0x45fe30, 0x45fe80, 0x45fe84, 0x45fe88, 0x45fe8c)
	scdRGsaved = map(lambda a: SDTest.Peek(a, 'i'), scdRGvars)
	scdFloors = scdKeep(scdWorld + 0x2f0, 0x18c4)
	scdL0 = scdWorld + 0x2f4
	scdL1 = scdL0 + 0x18c
	# a floor: its height, the ways up and down (point and angle)
	def scdLevel(l, y, ups, downs):
		if y is not None:
			scdSet(l, (y,))
		scdPoke(l + 4, len(ups))
		for k in range(len(ups)):
			scdSet(l + 8 + 12 * k, ups[k][0])
			scdSet(l + 0x68 + 4 * k, (ups[k][1],))
		scdPoke(l + 0x88, len(downs))
		for k in range(len(downs)):
			scdSet(l + 0x8c + 12 * k, downs[k][0])
			scdSet(l + 0xec + 4 * k, (downs[k][1],))
	scdCarR = scdKeep(scdCar + 0x388, 16)
	scdCar2R = scdKeep(scdCar2 + 0x388, 16)
	scdLifeR = scdRE['SDScdLife']
	scdTgtName = scdKeep(scdCar + 0x650, 0x80)
	scdEmi = scdF(scdCar + 0x58c, 1)
	scdPlayer = scdFind('Player0')
	# ents: (radarShow, radarTime) of the objects 1..4 and the outlink, the teams and cones of the cars and SDScdLife
	def scdRadar(a, target, floors, gv, objs, teams, cones, menu, emi, time):
		for k in range(5):
			e = scdRE[('SDScdObj1', 'SDScdObj2', 'SDScdObj3', 'SDScdObj4', 'DM_OutLink_SDScd')[k]]
			scdPoke(e + 0x13c, objs[k][0])
			scdSet(e + 0x10c, (objs[k][1],))
		for k in range(3):
			e = (scdCar, scdCar2, scdLifeR)[k]
			scdPoke(e + 0x388, teams[k])
			scdPoke(e + 0x38c, cones[k][0])
			scdSet(e + 0x390, cones[k][1:])
		scdPoke(scdWorld + 0x2f0, floors)
		for k in range(5):
			SDTest.Poke(scdRGvars[k], gv[k])
		scdPokeByte(a + 0x934, menu)
		scdSet(a + 0x944, ((0.0, 0.0), (300.0, -200.0))[menu])
		scdSet(scdCar + 0x58c, (emi,))
		scdSet(scdSc + 0x20, (time,))
		scdSrand(23)
		d = scdLog(lambda a=a, t=target: SDTest.Call(0x16d570, 'tviii', a, t, scdRCam))
		return (d, SDTest.Peek(0x45fe30, 'i'), scdInts(a + 0x8cc, 2), scdF(a + 0x8d4, 1), scdF(0x85fe80, 4))
	scdLevel(scdL0, None, (((54000.0, 19000.0, 61000.0), 0.5), ((54600.0, 19000.0, 61400.0), -1.2)),
		(((53000.0, 9000.0, 60000.0), 2.0),))
	scdLevel(scdL1, 20000.0, (((54100.0, 39000.0, 61050.0), 1.1), ((54300.0, 41000.0, 61000.0), 0.2),
		((60000.0, 41000.0, 60000.0), 3.0)), (((54250.0, 20500.0, 60950.0), -0.4), ((53950.0, 21000.0, 61050.0),
		2.6)))
	scdStr(scdCar + 0x650, 'SDScdCar2')
	scdOff = (0, 0.0)
	scdCone0 = (0, 0.0, 0.0)
	for scdV in ((scdCar, 1, (0, 0, 0, 0, 0), ((1, 0.0), (2, 0.0), (3, 0.0), (1, 0.0), (0, 0.0)), (0, 1, 2),
				((2, 0.8, 600.0), scdCone0, (3, 1.2, 900.0)), 0, 0.0, 12.5),
			(scdCar, 1, (1, 1, 1, 0, 1), ((1, 0.0), (2, 0.35), (3, 0.0), (1, 0.0), (1, 0.0)), (1, 1, 3),
				((1, 0.5, 300.0), (2, 0.9, 400.0), scdCone0), 0, 0.0, 12.55),
			(scdCar, 2, (1, 0, 0, 1, 0), ((1, 0.0), (2, 0.0), (3, 0.2), (1, 0.15), (1, 0.0)), (0, 2, 0),
				(scdCone0, scdCone0, (2, 0.6, 700.0)), 0, 0.0, 12.5),
			(scdCar2, 2, (1, 1, 0, 0, 0), ((1, 0.0), (2, 0.0), (3, 0.0), (1, 0.0), (0, 0.0)), (2, 0, 2),
				((3, 0.7, 500.0), scdCone0, scdCone0), 0, 0.0, 12.6),
			(scdCar2, 2, (0, 1, 1, 1, 1), ((2, 0.0), (3, 0.0), (1, 0.0), (2, 0.0), (1, 0.0)), (4, 4, 3),
				(scdCone0, scdCone0, scdCone0), 0, 0.0, 12.5),
			(scdPlayer, 1, (1, 1, 0, 0, 0), ((1, 0.0), (2, 0.0), (3, 0.0), (1, 0.0), (0, 0.0)), (3, 3, 3),
				(scdCone0, (2, 0.8, 600.0), scdCone0), 0, 0.0, 12.5),
			(scdCar, 1, (0, 0, 1, 0, 0), ((1, 0.0), (2, 0.0), (0, 0.0), (0, 0.0), (0, 0.0)), (0, 1, 2),
				(scdCone0, scdCone0, (2, 0.8, 600.0)), 1, 0.0, 12.5),
			(scdCar, 2, (1, 0, 0, 0, 0), ((1, 0.0), (2, 0.0), (0, 0.0), (0, 0.0), (0, 0.0)), (0, 1, 2),
				(scdCone0, scdCone0, scdCone0), 1, 0.0, 12.7),
			(scdCar, 1, (0, 0, 0, 0, 0), ((1, 0.0), (0, 0.0), (0, 0.0), (0, 0.0), (0, 0.0)), (0, 1, 2),
				(scdCone0, scdCone0, scdCone0), 0, 5.0, 12.5)):
		if scdV[0] and scdRadAll:
			check('SCD cRadarScorer_DrawRadar' + fmt(scdV[1:]), lambda a=scdA, v=scdV: scdRadar(a, v[0], v[1], v[2],
				v[3], v[4], v[5], v[6], v[7], v[8]))
	scdSet(scdCar + 0x58c, scdEmi)
	scdPutback(scdCarR)
	scdPutback(scdCar2R)
	scdPutback(scdTgtName)
	scdPutback(scdFloors)
	for scdK in range(len(scdRGvars)):
		SDTest.Poke(scdRGvars[scdK], scdRGsaved[scdK])
	for scdE, scdV in scdFlags:
		scdPoke(scdE + 0x4c, scdV)
	scdPokeByte(scdA + 0x934, 0)
	scdSet(scdA + 0x944, (0.0, 0.0))
	scdPoke(scdA + 0x8c4, scdRadImgs[0])
	for scdK in range(16):
		scdPoke(scdA + 0x8dc + 4 * scdK, scdRadImgs[1 + scdK])
	scdFree(scdRCam)
	scdSet(scdSc + 0x20, (12.5,))
	for scdN2, scdT2, scdP2 in scdRadEnts:
		if scdRadE[scdN2]:
			check('SCD radar entity Del ' + scdN2, lambda e=scdRadE[scdN2]: e.Del())


# ---- put back, the items marked DelMe, the cars deleted ----
if scdSc:
	for scdK in range(3):
		scdPoke(scdSc + 0x20 + 4 * scdK, scdScSaved[scdK])
	scdPoke(scdSc + 0x50, scdScSaved[3])
for scdName, scdType in scdTypes:
	if scdItems[scdName]:
		scdPokeByte(scdItems[scdName] + 0x28, scdB(scdItems[scdName] + 0x28) | 1)
for scdK in range(3):
	SDTest.Poke(scdGvars[scdK], scdGsaved[scdK])
for scdE in (scdCarObj, scdCar2Obj, scdCar3Obj):
	if scdE:
		check('SCD car Del', lambda e=scdE: e.Del())
check('SCD heap', lambda: SDTest.HeapOk())
