# game World/FX/FXScene.c (execfile'd by sdlevel.py, Python 1.5.2): the cutscene effects in the level FZ, on
# elements made by their constructors in calloc memory and a debris of a loaded CGameModel. FXScene_Create only with
# model names of no scene; SetEntity with Player0; the Resets that touch no particle system; the Updates without an
# entity (the debris life to 0) and with Player0 at times that start no particles (outside the time windows,
# throttled or with every ray of EnterMatrix waiting), with the scene entities (TheEndBMoto, BishopsDtritus,
# EnterMatrixMatrix) missing in FZ and a fake capsule without embedded effect for BillArrives_A. HumanCD only with the
# main mental controller effect (its scale and last restored). The frame time of the world timer is set to 0.01 and
# restored.
# Not called: the Resets of Bishops, Decontamination and HumanCD (they kill the particles of their unattached particle
# systems, a CHECK), Attach (needs the Dummy nodes in the debris model) and Alive (needs a debris system).
def fxsi(a):
	return SDTest.Peek(a, '@i')
def fxsb(a):
	return SDTest.Peek(a, '@b')
def fxsz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fxsfree(p):
	SDTest.Call(0x133550, 'ii', p)
def fxsfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxsset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fxsints(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs)
def fxsflt(a, offs):
	return map(lambda o, a=a: fxsfl(a + o, 1)[0], offs)
def fxspoke(a, v):
	SDTest.Poke(a - 0x400000, v)
fxsworld = SDTest.Peek(0x467450, 'i')
fxsdelta = fxsi(fxsworld + 0x2d4)
fxsset(fxsworld + 0x2d4, (0.01,))
fxsplayer = SDTest.Call(0x0c1430, 'tiii', fxsworld, 0)[0]
# the model of an entity through its vtable (slot 0x98)
def fxsmodel(e):
	return SDTest.Call(fxsi(fxsi(e) + 0x98) - 0x400000, 'tii', e)[0]
def fxspos(m):
	return (SDTest.Call(0x20fa30, 'tiiF', m, (0.0, 0.0, 0.0))[1], SDTest.Call(0x210830, 'tiiF', m, (0.0, 0.0, 0.0))[1])
fxsplayermodel = fxsmodel(fxsplayer)

# name, constructor, size, SetEntity, the offset of the entity, Update, Reset (0 none)
fxsclasses = (
	('BeatPickus', 0x1de230, 0x240, 0x1e9ac0, 0xc, 0x1dec90, 0x1e9840),
	('BillArrives_A', 0x1de2f0, 0xec, 0x1e9b60, 0xc, 0x1e0e50, 0x1e9880),
	('Bishops', 0x1de390, 0x1b8, 0x1e9c00, 0x4, 0x1e3050, 0),
	('Decontamination', 0x1de450, 0x11ec, 0x1e9ca0, 0xb4, 0x1e3dc0, 0),
	('EnterMatrix', 0x1de500, 0x798, 0x1e9d40, 0xc, 0x1e4b90, 0x1e99c0),
	('HumanCD', 0x1de5a0, 0x184, 0x1e9de0, 0xc, 0x1e73e0, 0),
	('TheEndB', 0x1de630, 0xcc, 0x1e9ec0, 0xc, 0x1e7dc0, 0x1e9a70),
	('Yakuzzi', 0x1de6c0, 0xd4, 0x1e9f60, 0xc, 0x1e8280, 0x1e9a90))
fxsel = {}
for fxsc in fxsclasses:
	fxsp = fxsz(fxsc[2])
	SDTest.Call(fxsc[1], 'tii', fxsp)
	fxsel[fxsc[0]] = fxsp

# FXScene_Create with names of no scene: no debris, nothing changes
for fxsn in ('Foo.m3d', '', 'Bishops.m3dx', 'TheEndB.M3D', 'Decontamination', 'BillArrives_B.m3d', 'Yakuzzi.m3',
		'HumanCD.m3d ', 'EnterMatrix.m3d.bak', 'Be'):
	check('FXScene_Create(%s)' % fxsn, lambda n=fxsn: SDTest.Call(0x1e8880, 'vis', fxsplayer, n))

# the main mental controller effect (HumanCD writes its scale 0x4270 and last 0x4274)
fxsmain = SDTest.Peek(0x46719c, 'i')
check('MentalControllerMainFX there', lambda: fxsmain != 0)
if fxsmain:
	fxsmainsaved = fxsints(fxsmain, (0x4270, 0x4274))

# SetEntity with Player0
for fxsc in fxsclasses:
	if fxsc[0] == 'HumanCD' and not fxsmain:
		continue
	fxsp = fxsel[fxsc[0]]
	check('%s_SetEntity' % fxsc[0], lambda p=fxsp, c=fxsc: (SDTest.Call(c[3], 'tvii', p, fxsplayer),
		fxsi(p + c[4]) == fxsplayer))
	if fxsc[0] == 'HumanCD':
		check('HumanCD_SetEntity main', lambda: (fxsi(fxsmain + 0x4274), fxsfl(fxsmain + 0x4270, 1)))

# the Resets: the floats set to 1.0 first, the entity pointers to the player
SDTest.Call(0x323c50, 'vi', 4711)
fxsresets = (
	('BeatPickus', (4, 8, 0xc, 0xcc, 0x180, 0x234, 0x238)),
	('BillArrives_A', (4, 8, 0xc, 0x10, 0x18, 0xcc)),
	('EnterMatrix', (4, 8, 0xc)),
	('TheEndB', (4, 8, 0xc, 0x10)),
	('Yakuzzi', (4, 8, 0xc, 0xc0)))
for fxsn, fxsoffs in fxsresets:
	fxsp = fxsel[fxsn]
	for fxso in fxsoffs:
		fxsset(fxsp + fxso, (1.0,))
	fxspoke(fxsp + 0xc, fxsplayer)
	fxsrva = 0
	for fxsc in fxsclasses:
		if fxsc[0] == fxsn:
			fxsrva = fxsc[6]
	check('%s_Reset' % fxsn, lambda p=fxsp, r=fxsrva, o=fxsoffs: (SDTest.Call(r, 'tvii', p, 0), fxsints(p, o)))
# EnterMatrix: the trails cleared, 0x18c to -1 and new random waits at 0x1cc
check('EnterMatrix_Reset arrays', lambda p=fxsel['EnterMatrix']: (fxsfl(p + 0x18c, 16), fxsfl(p + 0x1cc, 16)))
check('rand after Reset', lambda: SDTest.Call(0x323c2f, 'i')[0])

# a debris of a loaded model for the Updates
fxsd = fxsz(0x60)
check('fxs debris model', lambda: SDTest.Call(0x20eb10, 'tiis', fxsd,
	'Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D')[0] == fxsd)
def fxslife(v):
	fxsset(fxsd + 0x4c, (v,))

# without an entity: the debris life to 0, nothing else
for fxsc in fxsclasses:
	if not fxsc[5]:
		continue
	fxsp = fxsel[fxsc[0]]
	fxspoke(fxsp + fxsc[4], 0)
	fxslife(5.0)
	check('%s_Update no entity' % fxsc[0], lambda p=fxsp, c=fxsc: (SDTest.Call(c[5], 'tviif', p, fxsd, 0.1),
		fxsfl(fxsd + 0x4c, 1)))

# with Player0: the debris follows the model of the player
def fxsfollow():
	return fxspos(fxsd) == fxspos(fxsplayermodel)
def fxsupd(name, offs, fl):
	c = None
	for fxsc in fxsclasses:
		if fxsc[0] == name:
			c = fxsc
	p = fxsel[name]
	fxspoke(p + c[4], fxsplayer)
	for o, v in fl:
		fxsset(p + o, (v,))
	fxslife(5.0)
	return (SDTest.Call(c[5], 'tviif', p, fxsd, 0.1), fxsflt(p, offs), fxsfollow(), fxsfl(fxsd + 0x4c, 1))
SDTest.Call(0x323c50, 'vi', 4711)
# BeatPickus: t, tPrev, dust, debris, sparks, hit
fxsoffs = (4, 8, 0xcc, 0x180, 0x234, 0x238)
for fxsfl0 in (((4, 100.0), (8, 1.0)), ((4, 20.0), (8, 1.0), (0xcc, 0.0)), ((4, 40.0), (8, 1.0), (0xcc, 0.0),
		(0x238, 1.0)), ((4, 5.0), (8, 2.0))):
	check('BeatPickus_Update' + fmt(fxsfl0), lambda f=fxsfl0: fxsupd('BeatPickus', fxsoffs, f))
# BillArrives_A with a capsule without embedded effect (0x224 0)
fxscap = fxsz(0x240)
fxspoke(fxsel['BillArrives_A'] + 0x10, fxscap)
fxsoffs = (4, 8, 0x18, 0xcc)
for fxsfl0 in (((4, 100.0), (8, 100.0)), ((4, 11.5), (8, 1.0), (0xcc, 0.0)), ((4, 13.5), (8, 1.0), (0xcc, 0.0)),
		((4, 18.2), (8, 18.2)), ((4, 2.0), (8, 1.0))):
	check('BillArrives_A_Update' + fmt(fxsfl0), lambda f=fxsfl0: (fxsupd('BillArrives_A', fxsoffs, f),
		fxsi(fxsel['BillArrives_A'] + 0x10) == fxscap))
# Bishops: t and the detritus (none in FZ)
check('BishopsDtritus there', lambda: Scrap.GetEntity('BishopsDtritus') != None)
if Scrap.GetEntity('BishopsDtritus') == None:
	for fxsfl0 in (((0x16c, 20.0), (0x180, 0.5)), ((0x16c, 1.0), (0x180, 0.5))):
		check('Bishops_Update' + fmt(fxsfl0), lambda f=fxsfl0: (fxsupd('Bishops', (0x16c, 0x180), f),
			fxsi(fxsel['Bishops'] + 8)))
# Decontamination: t2, t, no target name
check('Decontamination_Update', lambda: (fxsupd('Decontamination', (0x100, 0x104), ((0x100, 3.0), (0x104, 4.0))),
	fxsi(fxsel['Decontamination'] + 0xb8)))
# HumanCD: outside the windows, in the scale and last window, in the scale window
if fxsmain:
	fxsoffs = (4, 8, 0x17c)
	for fxsfl0 in (((4, 5.0), (8, 1.0), (0x17c, 0.0)), ((4, 20.0), (8, 1.0), (0x17c, 0.0)),
			((4, 26.0), (8, 1.0), (0x17c, 0.0)), ((4, 33.0), (8, 1.0), (0x17c, 0.0)), ((4, 36.5), (8, 1.0),
			(0x17c, 0.0))):
		check('HumanCD_Update' + fmt(fxsfl0), lambda f=fxsfl0: (fxsupd('HumanCD', fxsoffs, f),
			fxsi(fxsmain + 0x4274), fxsfl(fxsmain + 0x4270, 1)))
	fxspoke(fxsmain + 0x4270, fxsmainsaved[0])
	fxspoke(fxsmain + 0x4274, fxsmainsaved[1])
# TheEndB: no moto in FZ
check('TheEndBMoto there', lambda: Scrap.GetEntity('TheEndBMoto') != None)
if Scrap.GetEntity('TheEndBMoto') == None:
	for fxsfl0 in (((4, 24.0), (8, 1.0)), ((4, 2.0), (8, 1.0))):
		check('TheEndB_Update' + fmt(fxsfl0), lambda f=fxsfl0: (fxsupd('TheEndB', (4, 8), f),
			fxsi(fxsel['TheEndB'] + 0x10)))
# EnterMatrix: every ray waiting (life -1, wait 5), in the first part, the second part and with the lens flare (its
# position 0x8, intensity 0x14, 0x18 and 0x1c in the flare object)
check('EnterMatrixMatrix there', lambda: Scrap.GetEntity('EnterMatrixMatrix') != None)
fxsem = fxsel['EnterMatrix']
fxsset(fxsem + 0x18c, (-1.0,) * 16 + (5.0,) * 16)
def fxsflare():
	f = fxsi(fxsem + 0x790)
	return (fxsfl(f + 8, 5), fxsi(f + 0x1c))
if Scrap.GetEntity('EnterMatrixMatrix') == None:
	for fxsfl0 in (((4, 5.0), (8, 1.0)), ((4, 17.0), (8, 16.0)), ((4, 25.0), (8, 24.0)), ((4, 40.0), (8, 39.0))):
		check('EnterMatrix_Update' + fmt(fxsfl0), lambda f=fxsfl0: (fxsupd('EnterMatrix', (4, 8), f),
			fxsfl(fxsem + 0x18c, 32)))
	# the flare is set (only then its fields are read)
	for fxsfl0 in (((4, 30.5), (8, 30.4)), ((4, 31.5), (8, 31.0)), ((4, 32.9), (8, 32.5))):
		check('EnterMatrix_Update flare' + fmt(fxsfl0), lambda f=fxsfl0: (fxsupd('EnterMatrix', (4, 8), f),
			fxsfl(fxsem + 0x18c, 32), fxsflare()))
# Yakuzzi: throttled (no particles)
check('Yakuzzi_Update', lambda: fxsupd('Yakuzzi', (4, 8, 0xc0), ((4, 5.0), (8, 1.0), (0xc0, 0.0))))
check('rand after Update', lambda: SDTest.Call(0x323c2f, 'i')[0])

SDTest.Call(0x20ec10, 'tvi', fxsd)
fxsfree(fxsd)
fxsfree(fxscap)
SDTest.Poke(fxsworld + 0x2d4 - 0x400000, fxsdelta)
# the elements go by their deleting destructors
fxsdeletes = (('BeatPickus', 0x1de970), ('BillArrives_A', 0x1de9c0), ('Bishops', 0x1dea00),
	('Decontamination', 0x1dea90), ('EnterMatrix', 0x1deb10), ('HumanCD', 0x1debb0), ('TheEndB', 0x1dec00),
	('Yakuzzi', 0x1dec50))
for fxsn, fxsrva in fxsdeletes:
	fxsp = fxsel[fxsn]
	SDTest.Call(fxsrva, 'tiii', fxsp, 0)
	fxsfree(fxsp)
