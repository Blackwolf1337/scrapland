# game Entities/EmbeddedFX/EmbeddedFXMisc.c (execfile'd by sdlevel.py, Python 1.5.2): the effects embedded in the
# object entities. The constructors, Reset, Stop, Release, the setters and the destructors on calloc memory; then the
# object models loaded into a fake entity (calloc memory with the vtable of cObjEntity and the CGameModel at 0xb8,
# where its GetModel points) and the effects made by EmbeddedFXMisc_Create on it: their fields, particle parameters,
# the particles they place and the points of their trails over a few Updates, then Reset, Stop, Release, Delete.
# Every node an Init needs is checked first (render node with a particle system), else that model is skipped: a
# missing one is a CHECK. rand is seeded, the frame time and the world time are fixed and every model visible
# (R_SplitScreen) while the tests run; all are restored. Only fields the code has written are read (the objects of
# Create come from operator new). Not called: the network handlers (they read a file), the shield hit (it needs the
# entity BossShield), the Update of the Moto (still in the exe).
def efmi(a):
	return SDTest.Peek(a, '@i')
def efmb(a):
	return SDTest.Peek(a, '@b')
def efmz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def efmfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def efmfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def efmset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def efmints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def efmbytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def efmpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def efmadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
# a CGameParticles: attached, the parameters (no padding bytes, no pointers)
def efmparts(p):
	return (efmi(p + 0xac) != 0, efmints(p, (0x00, 0x04, 0x18)), efmfl(p + 0x08, 4), efmfl(p + 0x1c, 28),
		efmfl(p + 0x9c, 2), efmbytes(p, (0x98, 0x99, 0xa4)))
# particle i: active, position, color, size
def efmpart(p, i):
	return SDTest.Call('CGameParticles_GetParticle', 'tbiiFiipF', p, i, (0.0, 0.0, 0.0), 0, 0, 0, (0.0,))
def efmnum(p):
	return SDTest.Call('CGameParticles_NumParticles', 'tii', p)[0]
def efmactive(p):
	return SDTest.Call('CGameParticles_NumActive', 'tii', p)[0]
# particle i if it is active (the data of the others may never have been written)
def efmpartv(p, i):
	r = efmpart(p, i)
	if r[0]:
		return r
	return (0,)
# the first n particles (at most the number the system has)
def efmpartlist(p, n):
	m = efmnum(p)
	if n > m:
		n = m
	return tuple(map(lambda i, p=p: efmpartv(p, i), range(n)))
# a CGameTrail: the CTrail fields and its points from the newest
def efmtrail(t):
	c = efmi(t)
	if not c:
		return 0
	mx = efmb(c + 0x20)
	n = efmb(c + 0x21)
	head = efmb(c + 0x22)
	pts = []
	for i in range(n):
		j = head - i - 1
		if j < 0:
			j = j + mx
		a = efmi(c + 0x28) + 0x14 * j
		pts.append((efmfl(a, 3), efmi(a + 0xc), efmfl(a + 0x10, 1)))
	return (efmbytes(c, (0, 0x0c, 0x0d, 0x0e, 0x0f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x3c)), efmi(c + 8),
		efmints(c, (0x10,)), efmfl(c + 0x14, 3), efmfl(c + 0x2c, 4), efmfl(c + 0x40, 5), tuple(pts))
def efmtrails(a, n):
	return tuple(map(lambda i, a=a: efmtrail(a + 4 * i), range(n)))
# the particles the effects place by index in the systems of these nodes (more is a CHECK)
efmmin = {'DOB_DataCube_Glow': 8, 'DOB_Interfono_Calling': 3, 'DF_Light': 2, 'DOB_MeetingPoint_Flares': 4,
	'DF_Light_02': 6, 'DOB_ExtraLife_Glow': 40, 'DOB_FlapCTF_Glow': 10, 'Dummy01': 255,
	'DOB_Conversor-Mandos_Glow': 10, 'DOB_Batery_Glow': 12, 'DOB_Treasure_Glow': 12, 'DC_Motion': 2,
	'DOB_Coin1_Glow': 2, 'DOB_Coin2_Glow': 2, 'DOB_Coin3_Glow': 2, 'DOB_Coin4_Glow': 2, 'DOB_Dollar_Glow': 2,
	'DOB_Coin1B_Glow': 2, 'DOB_Coin2B_Glow': 2, 'DOB_Coin3B_Glow': 2}
# render node name of the CGameModel gm with a particle system (flags & 2, obj3D of class 0xa5000020) of enough
# particles (nParticulas at 0xc)
def efmsist(gm, name):
	i = SDTest.Call('CGameModel_FindNodoR', 'tiis', gm, name)[0]
	if i < 0:
		return 0
	t = efmi(efmi(gm) + 0xac)
	if i >= efmi(t):
		return 0
	r = efmi(t + 4) + 0xa8 * i
	o = efmi(r + 0x9c)
	return (efmb(r + 4) & 2) and o != 0 and efmi(o + 4) == -0x5affffe0 and efmi(o + 0xc) >= efmmin.get(name, 0)
def efmnode(gm, name):
	return SDTest.Call('CGameModel_FindNodoR', 'tiis', gm, name)[0] >= 0

efmworld = SDTest.Peek('g_pWorld', 'i')
efmdelta = efmi(efmworld + 0x2d4)
efmtime = efmi(efmworld + 0x2d0)
efmset(efmworld + 0x2d4, (0.04,))
efmset(efmworld + 0x2d0, (12.5,))
efmsplit = SDTest.Peek('g_SplitScreen', 'i')
SDTest.Poke('g_SplitScreen', 1)
efmdpon = SDTest.Peek('g_MessengerTubeState', 'i')
SDTest.Call('crt_srand', 'vi', 4711)

# name, ctor, dtor, Delete, size, particles, trails (offset, count), ints after the ctor, bytes after the ctor,
# Reset, Release, Stop (0 for the ones that are not hooked)
efmclasses = (
	('Amplifier', sym('cEmbeddedFXAmplifier_ctor'), sym('cEmbeddedFXAmplifier_dtor'), sym('cEmbeddedFXAmplifier_Delete'), 0xcc, (0x10,), ((0xc4, 1),), (4, 0xc8), (), sym('cEmbeddedFXAmplifier_Reset'), sym('cEmbeddedFXCoin_Release'), sym('cEmbeddedFXAmplifier_Stop')),
	('Battery', sym('cEmbeddedFXBattery_ctor'), sym('cEmbeddedFXBattery_dtor'), sym('cEmbeddedFXBattery_Delete'), 0x134, (0x10,), ((0xc8, 3),), (4,), (), sym('cEmbeddedFXBattery_Reset'), sym('cEmbeddedFXBattery_Release'), sym('cEmbeddedFXBattery_Stop')),
	('BossShield', sym('cEmbeddedFXBossShield_ctor'), sym('cEmbeddedFXBossShield_dtor'), sym('cEmbeddedFXBossShield_Delete'), 0x1cf0, (8,), ((0x1cb0, 16),), (4,), (), sym('cEmbeddedFXBossShield_Reset'), sym('cEmbeddedFXBossShield_Release'), 0),
	('Capsule', sym('cEmbeddedFXCapsule_ctor'), sym('cEmbeddedFXCapsule_dtor'), sym('cEmbeddedFXCapsule_Delete'), 0x218, (8, 0xb8, 0x168), (), (4,), (), 0, sym('cEmbeddedFXCapsule_Release'), 0),
	('Coin', sym('cEmbeddedFXCoin_ctor'), sym('cEmbeddedFXCoin_dtor'), sym('cEmbeddedFXCoin_Delete'), 0xc8, (0x10,), (), (4,), (), sym('cEmbeddedFXCoin_Reset'), sym('cEmbeddedFXCoin_Release@e1030'), 0),
	('Conversor', sym('cEmbeddedFXConversor_ctor'), sym('cEmbeddedFXConversor_dtor'), sym('cEmbeddedFXConversor_Delete'), 0xc4, (0x10,), (), (4,), (), sym('cEmbeddedFXConversor_Reset'), sym('cEmbeddedFXCoin_Release@e1040'), 0),
	('DTritusDesktop', sym('cEmbeddedFXDTritusDesktop_ctor'), sym('cEmbeddedFXDTritusDesktop_dtor'), sym('cEmbeddedFXDTritusDesktop_Delete'), 0xd0, (0x14,), ((0xcc, 1),), (4, 0xc, 0xc8), (), sym('cEmbeddedFXDTritusDesktop_Reset'), sym('cEmbeddedFXDTritusDesktop_Release'),
		sym('cEmbeddedFXDTritusDesktop_Stop')),
	('DataCube', sym('cEmbeddedFXDataCube_ctor'), sym('cEmbeddedFXDataCube_dtor'), sym('cEmbeddedFXDataCube_Delete'), 0x170, (0xc, 0xc0), (), (4,), (), 0, sym('cEmbeddedFXDataCube_Release'), 0),
	('DataPack', sym('cEmbeddedFXDataPack_ctor'), sym('cEmbeddedFXDataPack_dtor'), sym('cEmbeddedFXDataPack_Delete'), 0x240, (0x24, 0xd8, 0x18c), ((0x23c, 1),), (4, 0xc), (9, 0xa), sym('cEmbeddedFXDataPack_Reset'),
		sym('cEmbeddedFXDataPack_Release'), sym('cEmbeddedFXDataPack_Stop')),
	('ExtraLife', sym('cEmbeddedFXExtraLife_ctor'), sym('cEmbeddedFXExtraLife_dtor'), sym('cEmbeddedFXExtraLife_Delete'), 0xd4, (0x10,), ((0xc8, 1),), (4,), (), sym('cEmbeddedFXExtraLife_Reset'), sym('cEmbeddedFXExtraLife_Release'), sym('cEmbeddedFXExtraLife_Stop')),
	('Flag', sym('cEmbeddedFXFlag_ctor'), sym('cEmbeddedFXFlag_dtor'), sym('cEmbeddedFXFlag_Delete'), 0xd0, (0x10,), (), (4, 0xc0, 0xc4), (), 0, sym('cEmbeddedFXCoin_Release@e10f0'), 0),
	('Item', sym('cEmbeddedFXItem_ctor'), sym('cEmbeddedFXItem_dtor'), sym('cEmbeddedFXItem_Delete'), 0xcc, (0x10,), (), (4,), (), sym('cEmbeddedFXItem_Reset'), sym('cEmbeddedFXCoin_Release@e1100'), 0),
	('Matrix', sym('cEmbeddedFXMatrix_ctor'), sym('cEmbeddedFXMatrix_dtor'), sym('cEmbeddedFXMatrix_Delete'), 0x1bc, (0x10, 0xc4), ((0x1b4, 1),), (4,), (9,), sym('cEmbeddedFXMatrix_Reset'), sym('cEmbeddedFXMatrix_Release'), sym('cEmbeddedFXMatrix_Stop')),
	('MeetingPoint', sym('cEmbeddedFXMeetingPoint_ctor'), sym('cEmbeddedFXMeetingPoint_dtor'), sym('cEmbeddedFXMeetingPoint_Delete'), 0x230, (0x10, 0xc4, 0x178), (), (4, 0xc0, 0x174, 0x228, 0x22c), (),
		0, sym('cEmbeddedFXCoin_Release@e1140'), 0),
	('Moto', sym('cEmbeddedFXMoto_ctor'), sym('cEmbeddedFXMoto_dtor'), sym('cEmbeddedFXMoto_Delete'), 0x7e0, (0x44, 0xf4, 0x1a4, 0x254, 0x304, 0x3b4, 0x464, 0x514, 0x5c4, 0x674,
		0x724), (), (4,), (), sym('cEmbeddedFXMoto_Reset'), sym('cEmbeddedFXMoto_Release'), 0),
	('Treasure', sym('cEmbeddedFXTreasure_ctor'), sym('cEmbeddedFXTreasure_dtor'), sym('cEmbeddedFXTreasure_Delete'), 0xf0, (0x10,), ((0xe4, 1),), (4, 0xe8, 0xec), (), sym('cEmbeddedFXTreasure_Reset'), sym('cEmbeddedFXTreasure_Release'),
		sym('cEmbeddedFXTreasure_Stop')))
efmbyname = {}
for c in efmclasses:
	efmbyname[c[0]] = c
def efmstate(p, c):
	parts = tuple(map(lambda o, p=p: efmi(p + o + 0xa8) != 0, c[5]))
	trails = []
	for o, n in c[6]:
		trails.append(efmtrails(p + o, n))
	# +4 is the entity (a heap address): only whether it is set
	ints = map(lambda o, v: (o == 4 and [v != 0] or [v])[0], c[7], efmints(p, c[7]))
	return (efmi(p) - 0x400000, parts, tuple(trails), tuple(ints), efmbytes(p, c[8]))
# the fields a Reset writes
efmresetfields = {
	'Battery': ((0xc4, 0x11c, 0x120, 0x124, 0x128, 0x12c, 0x130), ()),
	'BossShield': ((0xb8, 0x2b0, 0x4b0, 0x1c9c, 0x1ca4, 0x1ca8, 0x1cac), ()),
	'Coin': ((0xc0,), ()),
	'Conversor': ((0xc0,), ()),
	'ExtraLife': ((0xcc, 0xd0), ()),
	'Item': ((0xc0, 0xc8), ()),
	'Matrix': ((0x178, 0x17c, 0x180, 0x1b8), ()),
	'Moto': ((8, 0xc, 0x10, 0x7d4, 0x7d8), (0x7dc,)),
	'Treasure': ((0xc4, 0xc8, 0xcc), ())}
def efmreset(p, c):
	f = efmresetfields.get(c[0], ((), ()))
	trails = []
	for o, n in c[6]:
		trails.append(efmtrails(p + o, n))
	return (efmints(p, f[0]), efmbytes(p, f[1]), tuple(trails))
for c in efmclasses:
	for kind in ('dtor', 'Delete'):
		p = efmz(c[4])
		check('cEmbeddedFX%s_ctor %s' % (c[0], kind), lambda p=p, c=c: (SDTest.Call(c[1], 'tii', p)[0] == p,
			efmstate(p, c)))
		if kind == 'dtor':
			if c[9]:
				check('cEmbeddedFX%s_Reset' % c[0], lambda p=p, c=c: (SDTest.Call(c[9], 'tbi', p)[0],
					efmreset(p, c)))
			if c[11]:
				check('cEmbeddedFX%s_Stop(1)' % c[0], lambda p=p, c=c: (SDTest.Call(c[11], 'tvii', p, 1),
					efmstate(p, c)))
				check('cEmbeddedFX%s_Stop(0)' % c[0], lambda p=p, c=c: (SDTest.Call(c[11], 'tvii', p, 0),
					efmstate(p, c)))
			check('cEmbeddedFX%s_Release' % c[0], lambda p=p, c=c: (SDTest.Call(c[10], 'tvi', p), efmstate(p, c)))
			check('cEmbeddedFX%s_dtor' % c[0], lambda p=p, c=c: (SDTest.Call(c[2], 'tvi', p), efmi(p) - 0x400000,
				tuple(map(lambda o, p=p: efmi(p + o + 0xa8), c[5]))))
		else:
			check('cEmbeddedFX%s_Delete(0)' % c[0], lambda p=p, c=c: (SDTest.Call(c[3], 'tiii', p, 0)[0] == p,
				efmi(p) - 0x400000, tuple(map(lambda o, p=p: efmi(p + o + 0xa8), c[5]))))
		efmfree(p)

# the setters and the trail clears on calloc memory: rva, signature, offset written, arguments
efms = efmz(0x2000)
for rva, sig, offs, args in ((sym('cEmbeddedFXBossShield_SetMode'), 'tvii', (0x1c9c,), (7,)), (sym('cEmbeddedFXCoin_SetScale'), 'tvif', (0xc0,), (2.5,)),
		(sym('cEmbeddedFXExtraLife_SetScale'), 'tvif', (0xcc,), (-1.5,)), (sym('cEmbeddedFXCoin_SetScale@e45b0'), 'tvif', (0xc0,), (0.75,)), (sym('cEmbeddedFXMatrix_SetScale'), 'tvif', (0x1b8,), (6.0,)),
		(sym('cEmbeddedFXTreasure_SetScale'), 'tvif', (0xec,), (8.0,)), (sym('cEmbeddedFXBossShield_SetHit'), 'tvif', (0x1ca4,), (1.0,)), (sym('cEmbeddedFXTreasure_SetType'), 'tvii', (0xe8,), (3,)),
		(sym('cEmbeddedFXCoin_SetColor'), 'tvii', (0xc4,), (0x123456,)), (sym('cEmbeddedFXCoin_SetColor@e2a20'), 'tvii', (0xc4,), (-2,)),
		(sym('cEmbeddedFXMoto_SetF7d4'), 'tviff', (0x7d4, 0x7d8, 0x7dc), (0.5, 3.0))):
	check('setter %x' % rva + fmt(args), lambda rva=rva, sig=sig, offs=offs, args=args: (apply(SDTest.Call,
		(rva, sig, efms) + args), efmints(efms, offs)))
check('cEmbeddedFXDataPack_SetOn(1)', lambda: (SDTest.Call('cEmbeddedFXDataPack_SetOn', 'tvii', efms, 0x101), efmbytes(efms, (9, 10))))
check('cEmbeddedFXDataPack_On', lambda: (SDTest.Call('cEmbeddedFXDataPack_SetOn', 'tvii', efms, 0), SDTest.Call('cEmbeddedFXDataPack_On', 'tvi', efms),
	efmbytes(efms, (9, 10))))
for name, s in (('YellowS', 1), ('Yellow', 1), ('Green', 1), ('Red', 1), ('RED', 1), ('Blue', 1)):
	efmset(efms + 0xc4, (0.0, 0.0, 0.0))
	check('cEmbeddedFXFlag_SetColor' + fmt((name,)), lambda name=name: (SDTest.Call('cEmbeddedFXFlag_SetColor', 'tvis', efms, name),
		efmints(efms, (0xc4, 0xc8, 0xcc))))
efmfree(efms)

# the fake entity: the vtable of cObjEntity, GetModel at this + 0xb8
efmvt = sym('cObjEntity_vftable') + 0x400000
efmvec = efmz(0x20)
def efmfake(path):
	e = efmz(0x400)
	efmpoke(e, efmvt)
	SDTest.Call('CGameModel_ctor', 'tiis', e + 0xb8, path)
	return e
def efmunfake(e):
	SDTest.Call('CGameModel_Reset', 'tvi', e + 0xb8)
	efmfree(e)
def efmok(gm, sists, nodes):
	if not SDTest.Call('CGameModel_IsLoaded', 'tbi', gm)[0] or not efmi(gm):
		return 0
	for n in sists:
		if not efmsist(gm, n):
			return 0
	for n in nodes:
		if not efmnode(gm, n):
			return 0
	return 1
# channel 0 of the model has an animation (with a name, or any index when anyok)
def efmchan0(gm, anyok):
	mo = efmi(gm)
	if efmi(mo + 0xa4) <= 0 or not efmi(efmi(mo + 0xa8)):
		return 0
	anim = SDTest.Call('CGameModel_ChanInfo', 'tbiiip', gm, 0, 0, -1)[1]
	if anim < 0:
		return anyok
	return SDTest.Call('CGameModel_AnimName', 'tsii', gm, anim)[0] != None
efmcam = efmfl(efmworld + 0x254 + 12 * efmi(efmworld + 0x50), 3)
def efmplace(gm, d, rot):
	efmset(efmvec, efmadd(efmcam, d))
	SDTest.Call('CGameModel_SetPos', 'tvii', gm, efmvec)
	efmset(efmvec, rot)
	SDTest.Call('CGameModel_SetRot', 'tvii', gm, efmvec)

# class, model file, name for Create, render nodes with particle systems, other nodes
efmmodels = (
	('DataPack', 'Models/Chars/Messenger/DataPack.M3D', 'DataPack.M3D', ('DOB_DataPack_Glow', 'DOB_DataPack_Letras',
		'DF_Laser'), ('DC_Root',)),
	('DataCube', 'Models/Misc/DataCube/DataCube.M3D', 'DataCube.m3d', ('DOB_DataCube_Glow', 'DOB_DataCube_Flares'),
		()),
	('Capsule', 'Levels/Orbit/Map/BillArrives_A/Capsule.M3D', 'Capsule.M3D', ('DS_CapsulaBill_Motor_01',
		'DS_CapsulaBill_Motor_02', 'DS_CapsulaBill_Motor_03'), ()),
	('Matrix', 'Models/Misc/MatrixInt/MatrixInt.M3D', 'MatrixInt.m3d', ('DOB_DataCube_Glow', 'DOB_Matrix_Flares'),
		('DC_Root',)),
	('Matrix', 'Models/Misc/MatrixExt/MatrixExt.M3D', 'MatrixExt.m3d', ('DOB_DataCube_Glow', 'DOB_Matrix_Flares'),
		('DC_Root',)),
	('Matrix', 'Levels/GDB/Map/EnterMatrix/Matrix.M3D', 'Matrix.m3d', ('DOB_DataCube_Glow', 'DOB_Matrix_Flares'),
		('DC_Root',)),
	('Amplifier', 'Models/Misc/Amplifier/Amplifier.M3D', 'Amplifier.m3d', ('DOB_Amplificador_Flares',),
		('OB_Amplificador_Cuerpo',)),
	('DTritusDesktop', 'Models/Misc/DesktopPress/DTritusDesktop.M3D', 'DTritusDesktop.m3d', ('DOB_Interfono_Calling',),
		('OB_Interfono_Base',)),
	('MeetingPoint', 'Models/Misc/MeetingPoint/MeetingPoint.M3D', 'MeetingPoint.m3d', ('DF_Light',
		'DOB_MeetingPoint_Flares', 'DF_Light_02'), ()),
	('MeetingPoint', 'Models/Misc/MeetingPoint-Race/MeetingPoint-Race.M3D', 'MeetingPoint-Race.m3d', ('DF_Light',
		'DOB_MeetingPoint_Flares', 'DF_Light_02'), ()),
	('ExtraLife', 'Models/Misc/Coins/ExtraLife.M3D', 'ExtraLife.m3d', ('DOB_ExtraLife_Glow',), ('OB_ExtraLife_Barra_01',
		'DC_Root')),
	('Flag', 'Models/Misc/Flag/Flag.M3D', 'Flag.m3d', ('DOB_FlapCTF_Glow',), ()),
	('Flag', 'Models/Misc/Flag/FlagGreen.M3D', 'FlagGreen.m3d', ('DOB_FlapCTF_Glow',), ()),
	('Flag', 'Models/Misc/Flag/FlagRed.M3D', 'FlagRed.m3d', ('DOB_FlapCTF_Glow',), ()),
	('Flag', 'Models/Misc/Flag/FlagPl.M3D', 'FlagPl.m3d', ('DOB_FlapCTF_Glow',), ()),
	('Moto', 'Levels/ABIndoor/Map/TheEndA/Moto.M3D', 'Moto.M3D', ('DF_MotorTrust_01', 'DF_MotorTrust_02',
		'DF_MotorTrust_03', 'DF_MotorTrust_04', 'DF_MotorTrust_05', 'DF_EscapeTube_01', 'DF_EscapeTube_02',
		'DF_EscapeTube_03', 'DF_EscapeTube_04', 'DF_EscapeTube_05', 'DF_EscapeTube_06'), ()),
	('BossShield', 'Models/GFX/BossShield/BossShield.M3D', 'BossShield.M3D', ('Dummy01',), ()),
	('Conversor', 'Models/Misc/Conversor/Conversor.M3D', 'Conversor.m3d', ('DOB_Conversor-Mandos_Glow',), ()),
	('Battery', 'Models/Misc/Coins/Battery.M3D', 'Battery.m3d', ('DOB_Batery_Glow',), ('DC_Root',)),
	('Treasure', 'Models/Misc/Coins/TreasureA.M3D', 'TreasureA.m3d', ('DOB_Treasure_Glow',), ('DC_Root',)),
	('Treasure', 'Models/Misc/Coins/TreasureB.M3D', 'TreasureB.m3d', ('DOB_Treasure_Glow',), ('DC_Root',)),
	('Treasure', 'Models/Misc/Coins/TreasureC.M3D', 'TreasureC.m3d', ('DOB_Treasure_Glow',), ('DC_Root',)),
	('Treasure', 'Models/Misc/Coins/TreasureA-Ext.M3D', 'TreasureA-Ext.m3d', ('DOB_Treasure_Glow',), ('DC_Root',)),
	('Coin', 'Models/Misc/Coins/Coin1.M3D', 'Coin1.m3d', ('DOB_Coin1_Glow',), ()),
	('Coin', 'Models/Misc/Coins/Coin2.M3D', 'Coin2.m3d', ('DOB_Coin2_Glow',), ()),
	('Coin', 'Models/Misc/Coins/Coin3.M3D', 'Coin3.m3d', ('DOB_Coin3_Glow',), ()),
	('Coin', 'Models/Misc/Coins/Coin4.M3D', 'Coin4.m3d', ('DOB_Coin4_Glow',), ()),
	('Coin', 'Models/Misc/Coins/Dollar.M3D', 'Dollar.m3d', ('DOB_Dollar_Glow',), ()),
	('Coin', 'Models/Misc/Coins/BCoin1.M3D', 'BCoin1.m3d', ('DOB_Coin1B_Glow',), ()),
	('Coin', 'Models/Misc/Coins/BCoin2.M3D', 'BCoin2.m3d', ('DOB_Coin2B_Glow',), ()),
	('Coin', 'Models/Misc/Coins/BCoin3.M3D', 'BCoin3.m3d', ('DOB_Coin3B_Glow',), ()),
	('Item', 'Models/Items/Life/Life.M3D', 'Life.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/Ener/Ener.M3D', 'Ener.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/Miss/Miss.M3D', 'Miss.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/Part/Part.M3D', 'Part.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/vAim/vAim.M3D', 'vAim.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/dBom/dBom.M3D', 'dBom.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/iSiz/iSiz.M3D', 'iSiz.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/sRck/sRck.M3D', 'sRck.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/tTra/tTra.M3D', 'tTra.m3d', ('DC_Motion',), ()),
	('Item', 'Models/Items/aSpd/aSpd.M3D', 'aSpd.m3d', ('DC_Motion',), ()))

# the fields each effect has written after Create, by class: ints, bytes, particles (offset, how many listed), trails
efmcreated = {
	'DataPack': ((0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0xd4, 0x188), (9, 0xa), ((0x24, 64), (0xd8, 64), (0x18c, 64)),
		((0x23c, 1),)),
	'DataCube': ((8, 0xbc), (), ((0xc, 64), (0xc0, 64)), ()),
	'Capsule': ((), (), ((8, 64), (0xb8, 64), (0x168, 64)), ()),
	'Matrix': ((0xc, 0xc0, 0x174, 0x178, 0x17c, 0x180, 0x1b8), (9,), ((0x10, 64), (0xc4, 64)), ((0x1b4, 1),)),
	'Amplifier': ((0xc, 0xc0, 0xc8), (), ((0x10, 64),), ((0xc4, 1),)),
	'DTritusDesktop': ((0xc, 0x10, 0xc4, 0xc8), (), ((0x14, 64),), ((0xcc, 1),)),
	'MeetingPoint': ((0xc, 0xc0, 0x174, 0x228, 0x22c), (), ((0x10, 64), (0xc4, 64), (0x178, 64)), ()),
	'ExtraLife': ((0xc, 0xc0, 0xc4, 0xcc, 0xd0), (), ((0x10, 64),), ((0xc8, 1),)),
	'Flag': ((0xc, 0xc0, 0xc4, 0xc8, 0xcc), (), ((0x10, 64),), ()),
	'Moto': ((8, 0xc, 0x10, 0x38, 0x3c, 0x40, 0x7d4, 0x7d8), (0x7dc,), ((0x44, 64), (0xf4, 64), (0x1a4, 64),
		(0x254, 64), (0x304, 64), (0x3b4, 64), (0x464, 64), (0x514, 64), (0x5c4, 64), (0x674, 64), (0x724, 64)), ()),
	'BossShield': ((0x1c9c, 0x1ca4, 0x1ca8, 0x1cac), (), ((8, 64),), ((0x1cb0, 16),)),
	'Conversor': ((0xc, 0xc0), (), ((0x10, 64),), ()),
	'Battery': ((0xc, 0xc0, 0xc4, 0x11c, 0x120, 0x124, 0x128, 0x12c, 0x130), (), ((0x10, 64),), ((0xc8, 3),)),
	'Treasure': ((0xc, 0xc0, 0xc4, 0xc8, 0xcc, 0xe8, 0xec), (), ((0x10, 64),), ((0xe4, 1),)),
	'Coin': ((0xc, 0xc0, 0xc4), (), ((0x10, 64),), ()),
	'Item': ((0xc, 0xc0, 0xc4, 0xc8), (), ((0x10, 64),), ())}
def efmfields(name, fx, extra):
	f = efmcreated[name]
	parts = []
	for o, n in f[2]:
		parts.append((efmparts(fx + o), efmactive(fx + o), SDTest.Call('CGameParticles_IsVisible', 'tbi', fx + o)[0],
			SDTest.Call('CGameParticles_GetAlpha', 'tfi', fx + o)[0], efmpartlist(fx + o, n)))
	trails = []
	for o, n in f[3]:
		trails.append(efmtrails(fx + o, n))
	return (efmints(fx, f[0] + extra[0]), efmbytes(fx, f[1] + extra[1]), tuple(parts), tuple(trails))
# the fields an Update writes on top (visible and the ones only set while it runs)
efmupdated = {
	'DataPack': ((), (8,)), 'Matrix': ((0x184, 0x188, 0x18c) + tuple(range(0x190, 0x1b4, 4)), (8,)),
	'Amplifier': ((), (8,)), 'DTritusDesktop': ((), (8,)), 'MeetingPoint': ((), (8,)), 'ExtraLife': ((), (8,)),
	'Flag': ((), (8,)), 'BossShield': ((0x1ca0,), ()), 'Conversor': ((), (8,)), 'Battery': ((), (8,)),
	'Treasure': ((0xd0, 0xd4, 0xd8, 0xdc, 0xe0), (8,)), 'Coin': ((), (8,)), 'Item': ((), (8,))}
efmupdates = {'Amplifier': sym('cEmbeddedFXAmplifier_Update'), 'Battery': sym('cEmbeddedFXBattery_Update'), 'BossShield': sym('cEmbeddedFXBossShield_Update'), 'Coin': sym('cEmbeddedFXCoin_Update'), 'Conversor': sym('cEmbeddedFXConversor_Update'),
	'DTritusDesktop': sym('cEmbeddedFXDTritusDesktop_Update'), 'DataPack': sym('cEmbeddedFXDataPack_Update'), 'ExtraLife': sym('cEmbeddedFXExtraLife_Update'), 'Flag': sym('cEmbeddedFXFlag_Update'), 'Item': sym('cEmbeddedFXItem_Update'),
	'Matrix': sym('cEmbeddedFXMatrix_Update'), 'MeetingPoint': sym('cEmbeddedFXMeetingPoint_Update'), 'Treasure': sym('cEmbeddedFXTreasure_Update')}
# the BossShield slots: t, speed, dir, size, color of a few
def efmslots(fx):
	r = []
	for i in (0, 1, 2, 77, 254):
		r.append((efmfl(fx + 0xb8 + 4 * i, 1), efmfl(fx + 0x4b4 + 4 * i, 1), efmfl(fx + 0x8b0 + 12 * i, 3),
			efmfl(fx + 0x14a4 + 4 * i, 1), efmi(fx + 0x18a0 + 4 * i), efmpartv(fx + 8, i)))
	return tuple(r)
def efmupd(name, fx, dt, extra):
	SDTest.Call(efmupdates[name], 'tvif', fx, dt)
	if name == 'BossShield':
		return (efmfields(name, fx, extra), efmslots(fx), efmactive(fx + 8))
	return efmfields(name, fx, extra)

# the places of the model to the camera and the time steps of the Updates
efmplaces = (((0.0, 0.0, 300.0), (0.0, 0.0, 0.0)), ((0.0, 500.0, 20000.0), (0.0, 1.5, 0.0)),
	((-3000.0, 100.0, 50000.0), (0.3, -2.5, 0.0)))
efmdts = (0.04, 0.1, 0.25, 0.0, 0.5, 0.02)
for c in efmmodels:
	name = c[0]
	k = efmbyname[name]
	e = efmfake(c[1])
	gm = e + 0xb8
	if not efmok(gm, c[3], c[4]):
		check('%s nodes' % c[2], lambda: 'skipped')
		efmunfake(e)
		continue
	check('%s model' % c[2], lambda gm=gm: (SDTest.Call('CGameModel_IsLoaded', 'tbi', gm)[0], SDTest.Call('CGameModel_Name', 'tsi', gm)[0]))
	slot = efmz(4)
	check('EmbeddedFXMisc_Create(%s)' % c[2], lambda e=e, c=c, slot=slot: (SDTest.Call('EmbeddedFXMisc_Create', 'visi', e, c[2], slot),
		efmi(slot) != 0, efmi(efmi(slot)) - 0x400000, efmi(efmi(slot) + 4) == e))
	fx = efmi(slot)
	check('%s fields' % c[2], lambda name=name, fx=fx: efmfields(name, fx, ((), ())))
	if name == 'DataPack':
		check('EmbeddedFXMisc_Create(DataPack) again', lambda e=e, c=c, slot=slot: (SDTest.Call('EmbeddedFXMisc_Create', 'visi', e,
			c[2], slot), efmi(efmi(slot)) - 0x400000, efmi(efmi(slot) + 4) == e))
		fx = efmi(slot)
		check('DataPack fields again', lambda name=name, fx=fx: efmfields(name, fx, ((), ())))
	# the Updates of Matrix.m3d and DTritusDesktop read the animation of channel 0: a CHECK without one
	upd = efmupdates.has_key(name)
	if upd and (c[2] == 'Matrix.m3d' or name == 'DTritusDesktop') and not efmchan0(gm, name == 'DTritusDesktop'):
		check('%s Update' % c[2], lambda: 'skipped, no animation')
		upd = 0
	if upd:
		extra = efmupdated[name]
		if name == 'BossShield':
			# speed and regen are not written before the first Update: 0
			# nor the slots a mode out of range never writes: speed, dir, size, color
			for efma, efmn in ((0x4b4, 255), (0x8b0, 765), (0x14a4, 510)):
				for efmk in range(0, efmn, 51):
					efmset(fx + efma + 4 * efmk, (0.0,) * 51)
			efmset(fx + 0x1ca0, (0.0,))
		for d, rot in efmplaces:
			efmplace(gm, d, rot)
			for dt in efmdts:
				check('%s Update' % c[2] + fmt((d, rot, dt)), lambda name=name, fx=fx, dt=dt, extra=extra:
					efmupd(name, fx, dt, extra))
		if name == 'DataPack':
			for on in (1, 0, 1):
				efmpoke(fx + 8, (efmi(fx + 8) & 0xff) | (on << 8))
				for dt in (0.1, 0.3, 0.5):
					check('DataPack Update on' + fmt((on, dt)), lambda name=name, fx=fx, dt=dt, extra=extra:
						efmupd(name, fx, dt, extra))
		if name == 'MeetingPoint':
			check('cEmbeddedFXMeetingPoint_SetCountDown', lambda fx=fx: (SDTest.Call('cEmbeddedFXMeetingPoint_SetCountDown', 'tviff', fx, 0.5, 440.0),
				efmints(fx, (0x174, 0x228, 0x22c))))
			for dt in (0.1, 0.2, 0.3, 0.02, 0.4, 0.3, 0.5, 0.5):
				check('MeetingPoint Update count down' + fmt((dt,)), lambda name=name, fx=fx, dt=dt, extra=extra:
					efmupd(name, fx, dt, extra))
		if name == 'BossShield':
			for mode, hit in ((1, 0.0), (1, 0.5), (2, 1.5), (2, 0.0), (3, 0.0), (1, 0.25), (0, 0.0)):
				efmpoke(fx + 0x1c9c, mode)
				efmset(fx + 0x1ca4, (hit,))
				for dt in (0.04, 0.3):
					check('BossShield Update mode' + fmt((mode, hit, dt)), lambda name=name, fx=fx, dt=dt,
						extra=extra: efmupd(name, fx, dt, extra))
		if name == 'Treasure' or name == 'Battery':
			for i in range(12):
				check('%s Update more %d' % (c[2], i), lambda name=name, fx=fx, extra=extra: efmupd(name, fx, 0.07,
					extra))
		# the model effect on: Coin, Item, Conversor, ExtraLife and the rays of the Battery stop
		mo = efmi(gm)
		efmfx = efmi(mo + 0x10c)
		efmpoke(mo + 0x10c, 1)
		check('%s Update with a model effect' % c[2], lambda name=name, fx=fx, extra=extra: efmupd(name, fx, 0.04,
			extra))
		efmpoke(mo + 0x10c, efmfx)
		# hidden: no split screen, the visible flag of the model off
		SDTest.Poke('g_SplitScreen', 0)
		efmvis = efmi(mo + 0x120)
		efmpoke(mo + 0x120, efmvis & ~0xff00)
		check('%s model visible' % c[2], lambda gm=gm: SDTest.Call('CGameModel_IsVisible', 'tbi', gm)[0])
		check('%s Update hidden' % c[2], lambda name=name, fx=fx, extra=extra: efmupd(name, fx, 0.04, extra))
		SDTest.Poke('g_SplitScreen', 1)
		efmpoke(mo + 0x120, efmvis)
		check('%s Update shown again' % c[2], lambda name=name, fx=fx, extra=extra: efmupd(name, fx, 0.04, extra))
	if k[9]:
		check('%s Reset' % c[2], lambda k=k, fx=fx: (SDTest.Call(k[9], 'tbi', fx)[0], efmreset(fx, k)))
	if k[11]:
		check('%s Stop(1)' % c[2], lambda k=k, fx=fx: (SDTest.Call(k[11], 'tvii', fx, 1), efmreset(fx, k)))
		check('%s Stop(0)' % c[2], lambda k=k, fx=fx: (SDTest.Call(k[11], 'tvii', fx, 0), efmreset(fx, k)))
	if name == 'Moto':
		check('cEmbeddedFXMoto_EnginePos', lambda fx=fx: SDTest.Call('cEmbeddedFXMoto_EnginePos', 'tiiF', fx, (0.0, 0.0, 0.0))[1])
	# after Release the particle systems may be gone: reading them is a CHECK (SistPart)
	check('%s Release' % c[2], lambda k=k, fx=fx: (SDTest.Call(k[10], 'tvi', fx), efmstate(fx, k)))
	check('%s Delete(1)' % c[2], lambda k=k, fx=fx: SDTest.Call(k[3], 'tiii', fx, 1)[0] == fx)
	efmfree(slot)
	efmunfake(e)

# Create with a model without an effect: *fx stays
e = efmfake('Models/Misc/Coins/Coin1.M3D')
slot = efmz(4)
check('EmbeddedFXMisc_Create(Dtritus.M3D)', lambda e=e, slot=slot: (SDTest.Call('EmbeddedFXMisc_Create', 'visi', e, 'Dtritus.M3D',
	slot), efmi(slot)))
check('EmbeddedFXMisc_Create(VAim.m3d)', lambda e=e, slot=slot: (SDTest.Call('EmbeddedFXMisc_Create', 'visi', e, 'VAim.m3d', slot),
	efmi(slot)))
efmfree(slot)
efmunfake(e)

efmfree(efmvec)
check('rand after', lambda: SDTest.Call('crt_rand', 'i')[0])
SDTest.Poke('g_SplitScreen', efmsplit)
SDTest.Poke('g_MessengerTubeState', efmdpon)
efmpoke(efmworld + 0x2d4, efmdelta)
efmpoke(efmworld + 0x2d0, efmtime)
