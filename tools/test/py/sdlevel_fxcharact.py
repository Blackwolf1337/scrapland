# game World/FX/FXCharActions.c (execfile'd by sdlevel.py, Python 1.5.2): the character action effects on elements
# made by their constructors in calloc memory: the constructors (the fields they write), the destructors and Delete
# (flags 0, the memory freed by the script), the resets that do not kill particles, the setters with the player
# (the starts that CHECK a node of the model only when the player model has that node), the random points, the
# Messenger lightning (Bezier curve and floor test), the data pack trails, and the Updates of PoliceSteal,
# BertoFlash, SebastianFlash and the beams on the paths before the particles (no entity, the timers out of range)
# on a debris of a loaded model. The frame time of the world timer is set by hand and restored; rand is seeded.
# Not called: Attach (needs the dummies of the effect models), Alive (steps a debris of a system), the resets that
# kill particles of a system that is not attached, the Updates up to the particles and BettyAttack_Emit (no
# particle system), PoliceGearConversion_Start (needs the armors of Attach), GearMissile_Run (the Update is not
# here), SetItem and SetOwner (CHECK the entity types).
def fxai(a):
	return SDTest.Peek(a, '@i')
def fxab(a):
	return SDTest.Peek(a, '@b')
def fxaz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def fxafree(p):
	SDTest.Call(0x133550, 'ii', p)
def fxafl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxaset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def fxaints(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs)
def fxabytes(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs)
def fxagints(rva, n):
	return map(lambda i, rva=rva: SDTest.Peek(rva + 4 * i, 'i'), range(n))
def fxapoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fxasrand(s):
	SDTest.Call(0x323c50, 'vi', s)
# the floats of n vec3 in chunks of up to 60 floats
def fxavecs(a, n):
	r = ()
	fxak = 0
	while fxak < 3 * n:
		fxac = min(60, 3 * n - fxak)
		r = r + fxafl(a + 4 * fxak, fxac)
		fxak = fxak + fxac
	return r
fxaworld = SDTest.Peek(0x467450, 'i')
fxaplayer = SDTest.Call(0x0c1430, 'tiii', fxaworld, 0)[0]
fxadelta = fxai(fxaworld + 0x2d4)
# the model of the player through its vtable (GetModel 0x98) and a node of it
fxapmodel = SDTest.Call(fxai(fxai(fxaplayer) + 0x98) - 0x400000, 'tii', fxaplayer)[0]
def fxanode(name):
	return SDTest.Call(0x20f7d0, 'tiis', fxapmodel, name)[0]

# the elements: name, ctor, dtor, Delete, size, vtable, the ints and bytes the constructor writes
fxaclasses = (
	('BankMasterAttack', 0x1b5320, 0x1b5f20, 0x1b6520, 0xd4, 0x3cb72c, (4, 0xb8, 0xbc), (0xd0,)),
	('BankMasterSuction', 0x1b53c0, 0x1b5f50, 0x1b6570, 0x1ec, 0x3cb744, (4, 8, 0xbc, 0xc0), (0x1e8,)),
	('BankerSteal', 0x1b5450, 0x1b5f80, 0x1b65c0, 0xd0, 0x3cb5c4, (4, 8, 0xbc, 0xc0, 0xc4, 0xc8), (0xcc,)),
	('BertoAttack', 0x1b54f0, 0x1b5fa0, 0x1b6600, 0xcc, 0x3cb684, (4, 0xb8, 0xbc), (0xc8,)),
	('BertoFlash', 0x1b5570, 0x1b5fc0, 0x1b6640, 0x678, 0x3cb69c, (4, 8, 0xbc, 0xc0), (0x674,)),
	('BettyAttack', 0x1b5610, 0x1b6020, 0x1b66c0, 0x1a8, 0x3cb6fc, (4, 0x168, 0x16c, 0x170), (0x1a4,)),
	('BettyKill', 0x1b5700, 0x1b60c0, 0x1b66f0, 0x918, 0x3cb714, (4, 0xb8, 0xbc), (0x914,)),
	('BishopAttack', 0x1b57a0, 0x1b6120, 0x1b6770, 0xfc, 0x3cb66c, (4, 0xc, 0x10), (0xf8,)),
	('BishopSellLife', 0x1b5850, 0x1b6170, 0x1b67e0, 0x180, 0x3cb654, (4, 8, 0x16c, 0x170), (0x17c,)),
	('DTritusAttack', 0x1b58e0, 0x1b61a0, 0x1b6830, 0x1158, 0x3cb624, (4, 0xb8, 0xbc), (0x1154,)),
	('DTritusKill', 0x1b5980, 0x1b6200, 0x1b68b0, 0x918, 0x3cb63c, (4, 0xb8, 0xbc), (0x914,)),
	('FunctionarySpecialAction', 0x1b5a20, 0x1b6260, 0x1b6930, 0x1ac, 0x3cb5f4, (4, 0xb8, 0xbc), (0x1a8,)),
	('GearMissile', 0x1b5ac0, 0x1b62c0, 0x1b69b0, 0x6bc, 0x3cb5ac, (4, 0x170, 0x174, 0x178, 0x18c), (0x6b8,)),
	('MercenaryDrink', 0x1b5b90, 0x1b6340, 0x1b6a50, 0xd4, 0x3cb75c, (4, 0xb8, 0xbc), (0xd0,)),
	('MessengerAttack', 0x1b5c10, 0x1b6360, 0x1b6a90, 0x3ec, 0x3cb6b4, (4, 0xc, 0x10), (0x3e8,)),
	('MessengerTakeDataPack', 0x1b5ca0, 0x1b63c0, 0x1b6b10, 0x174, 0x3cb6cc, (4, 0xc), (0x170,)),
	('PoliceGearConversion', 0x1b5d30, 0x1b6420, 0x1b6b90, 0xa54, 0x3cb60c, (4, 8, 0xc), (0xa50,)),
	('PoliceSteal', 0x1b5df0, 0x1b64a0, 0x1b6c30, 0xcc, 0x3cb5dc, (4, 8, 0xbc, 0xc0, 0xc4), (0xc8,)),
	('SebastianFlash', 0x1b5e80, 0x1b64c0, 0x1b6c70, 0x7e0, 0x3cb6e4, (4, 0xb8, 0xbc), (0x7dc,)),
	('DebrisCloud', 0x1c9630, 0x1c98d0, 0x1c99d0, 0x1f4, 0x3cbe5c, (4, 0xbc, 0xc0), (0x1f0,)),
	('EMI', 0x1c96b0, 0x1c98f0, 0x1c9a10, 0xd20, 0x3cbe2c, (4, 0xbc, 0xc0, 0xc4, 0xd14, 0xc9c), (0xd1c,)),
	('Hook', 0x1c97b0, 0x1c9970, 0x1c9ab0, 0x15c, 0x3cbe14, (4, 8, 0x98, 0x14c), (0x158,)),
	('SonicExplosion', 0x1c9850, 0x1c99a0, 0x1c9b00, 0x220, 0x3cbe44, (0x164, 0x168), (0x21c,)),
)
def fxaclass(name):
	for c in fxaclasses:
		if c[0] == name:
			return c
def fxanew(name):
	c = fxaclass(name)
	p = fxaz(c[4])
	SDTest.Call(c[1], 'tii', p)
	return p
def fxadel(p, name):
	SDTest.Call(fxaclass(name)[2], 'tvi', p)
	fxafree(p)

# constructors, destructors and Delete; the particles of the constructor at their default values
for fxac in fxaclasses:
	fxap = fxaz(fxac[4])
	for fxao in range(0, fxac[4], 4):
		fxapoke(fxap + fxao, 0x55555555)
	check('cObjElementFX' + fxac[0] + '_ctor', lambda c=fxac, p=fxap: (SDTest.Call(c[1], 'tii', p)[0] == p,
		fxai(p) == 0x400000 + c[5], fxaints(p, c[6]), fxabytes(p, c[7])))
	check('cObjElementFX' + fxac[0] + '_dtor', lambda c=fxac, p=fxap: (SDTest.Call(c[2], 'tvi', p), fxai(p) ==
		0x7cb0e4))
	SDTest.Call(fxac[1], 'tii', fxap)
	check('cObjElementFX' + fxac[0] + '_Delete', lambda c=fxac, p=fxap: (SDTest.Call(c[3], 'tiii', p, 0)[0] == p,
		fxai(p) == 0x7cb0e4))
	fxafree(fxap)
fxap = fxanew('EMI')
check('cObjElementFXEMI links', lambda: map(lambda i, p=fxap: (fxai(p + 0xc94 + 16 * i) == p + 0xc94 + 16 * i,
	fxai(p + 0xca0 + 16 * i) == p), range(8)))
fxadel(fxap, 'EMI')
fxap = fxanew('BankMasterAttack')
check('cObjElementFXBankMasterAttack particles', lambda: (fxaints(fxap + 8, (0, 4, 0x18, 0x98, 0xa4)),
	fxafl(fxap + 8 + 8, 6), fxai(fxap + 0xc0) != 0))
fxadel(fxap, 'BankMasterAttack')

# the resets without particles
for fxar in (('BankMasterAttack', 0x1c6560, (0xb8, 0xbc, 0xcc)), ('BankMasterSuction', 0x1c65a0, (0xbc, 0xc0, 0x1e0,
	0x1e4)), ('BankerSteal', 0x1c65d0, (4, 8, 0xc0)), ('MercenaryDrink', 0x1c6940, (0xb8, 0xbc)), ('PoliceSteal',
	0x1c6a60, (4, 8, 0xbc))):
	fxap = fxanew(fxar[0])
	for fxao in fxar[2]:
		fxapoke(fxap + fxao, 0x3f800000 + fxao)
	check('cObjElementFX' + fxar[0] + '_Reset', lambda r=fxar, p=fxap: (SDTest.Call(r[1], 'tvii', p, 0),
		fxaints(p, r[2])))
	fxadel(fxap, fxar[0])

# the setters
fxap = fxanew('PoliceSteal')
check('FXMisc_SetEntity PoliceSteal', lambda: (SDTest.Call(0x1c6db0, 'tvii', fxap, fxaplayer), fxai(fxap + 4) ==
	fxaplayer))
for fxaf in (0x1c6f10, 0x1c84d0, 0x1c8510):
	fxapoke(fxap + 8, 0)
	check('FXCharActions_SetEntity8 %x' % fxaf, lambda f=fxaf: (SDTest.Call(f, 'tvii', fxap, fxaplayer),
		fxai(fxap + 8) == fxaplayer))
fxadel(fxap, 'PoliceSteal')
fxap = fxanew('BankerSteal')
check('cObjElementFXBankerSteal_SetEntity', lambda: (SDTest.Call(0x1c6ad0, 'tvii', fxap, fxaplayer), fxai(fxap + 4) ==
	fxaplayer, fxai(fxap + 0xbc)))
check('FXMisc_SetI8 BankerSteal', lambda: (SDTest.Call(0x1c6f00, 'tvii', fxap, 77), fxai(fxap + 8)))
fxadel(fxap, 'BankerSteal')
fxap = fxanew('GearMissile')
check('FXChar_SetI4 GearMissile', lambda: (SDTest.Call(0x1c6fb0, 'tvii', fxap, -5), fxai(fxap + 4)))
fxadel(fxap, 'GearMissile')
fxap = fxanew('BishopSellLife')
check('cObjElementFXBishopSellLife_SetAttacked', lambda: (SDTest.Call(0x1c6df0, 'tvii', fxap, fxaplayer),
	fxai(fxap + 8) == fxaplayer))
check('cObjElementFXBishopSellLife_SetAttacked 0', lambda: (SDTest.Call(0x1c6df0, 'tvii', fxap, 0), fxai(fxap + 8)))
check('cObjElementFXBishopSellLife_SetAttacker 0', lambda: (SDTest.Call(0x1c6e40, 'tvii', fxap, 0), fxai(fxap + 4)))
check('fxa hands', lambda: (fxanode('R_Bishop_Mano_Izq'), fxanode('R_Bishop_Mano_Der')))
if fxanode('R_Bishop_Mano_Izq') > -1 and fxanode('R_Bishop_Mano_Der') > -1:
	check('cObjElementFXBishopSellLife_SetAttacker', lambda: (SDTest.Call(0x1c6e40, 'tvii', fxap, fxaplayer),
		fxai(fxap + 4) == fxaplayer, fxaints(fxap, (0x174, 0x178))))
fxadel(fxap, 'BishopSellLife')
fxap = fxanew('MercenaryDrink')
check('cObjElementFXMercenaryDrink_SetC0', lambda: (SDTest.Call(0x1c6fc0, 'tvifff', fxap, 1.5, -2.25, 1e30),
	fxafl(fxap + 0xc0, 3)))
check('cObjElementFXMercenaryDrink_Start', lambda: (SDTest.Call(0x1c7b00, 'tvii', fxap, fxaplayer), fxai(fxap + 4) ==
	fxaplayer, fxai(fxap + 0xcc)))
fxadel(fxap, 'MercenaryDrink')
for fxas in (('BankMasterAttack', 0x1c6ff0, 'DF_Batton', 0xc8), ('BertoAttack', 0x1c70b0, 'R_Berto_TroncoInf-cristal',
	0xc4)):
	check('fxa node ' + fxas[2], lambda s=fxas: fxanode(s[2]))
	if fxanode(fxas[2]) > -1:
		fxap = fxanew(fxas[0])
		check('cObjElementFX' + fxas[0] + '_Start', lambda s=fxas, p=fxap: (SDTest.Call(s[1], 'tvii', p, fxaplayer),
			fxai(p + 4) == fxaplayer, fxai(p + s[3])))
		fxadel(fxap, fxas[0])
fxap = fxanew('BettyKill')
check('cObjElementFXKill_SetAngles', lambda: (SDTest.Call(0x1c8550, 'tvifff', fxap, 0.5, 1.0, -1.5),
	SDTest.Call(0x1c8580, 'tvifff', fxap, 0.25, 2.0, 3.0), fxafl(fxap + 0x908, 3)))
for fxaf in (0x1c71b0, 0x1c7810):
	fxasrand(321)
	check('cObjElementFXKill_Start %x' % fxaf, lambda f=fxaf: (SDTest.Call(f, 'tvii', fxap, fxaplayer),
		fxai(fxap + 4) == fxaplayer, fxaints(fxap + 0xc0, range(0, 40, 4)), fxavecs(fxap + 0x1ec, 20),
		fxafl(fxap + 0x570, 30), fxafl(fxap + 0x69c, 30), fxafl(fxap + 0x7c8, 30), SDTest.Call(0x323c2f, 'i')[0]))
fxadel(fxap, 'BettyKill')
check('fxa node R_Detritus_Tronco', lambda: fxanode('R_Detritus_Tronco'))
if fxanode('R_Detritus_Tronco') > -1:
	fxap = fxanew('DTritusAttack')
	fxasrand(322)
	check('cObjElementFXDTritusAttack_Start', lambda: (SDTest.Call(0x1c74e0, 'tvii', fxap, fxaplayer),
		fxai(fxap + 0xc0), fxaints(fxap + 0xc4, range(0, 40, 4)), fxavecs(fxap + 0x31c, 20), fxafl(fxap + 0xa24, 30),
		fxafl(fxap + 0xc7c, 30), fxafl(fxap + 0xed4, 30), SDTest.Call(0x323c2f, 'i')[0]))
	fxadel(fxap, 'DTritusAttack')
fxap = fxanew('BishopAttack')
for fxal in ((0.0, 0.0, 0.0, 3.0, 4.0, 12.0), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0), (-5.5, 100.0, 7.0, 1e-20, -3e10, 0.5)):
	check('cObjElementFXBishopAttack_SetLine' + fmt(fxal), lambda l=fxal: (SDTest.Call(0x1c89a0, 'tviffffff', fxap,
		l[0], l[1], l[2], l[3], l[4], l[5]), fxafl(fxap + 0xd4, 9)))
fxadel(fxap, 'BishopAttack')

# the Functionary start on the player if it is a WalkChar (the vertices are searched once and kept in the exe)
fxawalk = SDTest.Call(fxai(fxai(fxaplayer) + 0x14) - 0x400000, 'tbis', fxaplayer, 'WalkChar')[0]
check('fxa player WalkChar', lambda: (fxawalk, SDTest.Peek(0x466fc0, 'b')))
if fxawalk:
	fxap = fxanew('FunctionarySpecialAction')
	fxasrand(323)
	check('cObjElementFXFunctionarySpecialAction_Start', lambda: (SDTest.Call(0x1c6b30, 'tvii', fxap, fxaplayer),
		SDTest.Peek(0x466fc0, 'b'), fxagints(0x466fc4, 14), fxagints(0x466ffc, 14),
		fxavecs(fxap + 0xfc, 14), SDTest.Call(0x323c2f, 'i')[0]))
	fxasrand(324)
	check('cObjElementFXFunctionarySpecialAction_Start again', lambda: (SDTest.Call(0x1c6b30, 'tvii', fxap,
		fxaplayer), fxagints(0x466ffc, 14), fxavecs(fxap + 0xfc, 14)))
	fxadel(fxap, 'FunctionarySpecialAction')

# the random points of the starts
fxap = fxanew('BankMasterSuction')
fxapoke(fxap + 4, fxaplayer)
fxapoke(fxap + 8, fxaplayer)
fxasrand(325)
check('cObjElementFXBankMasterSuction_RandomPoints', lambda: (SDTest.Call(0x1c4670, 'tvi', fxap), fxafl(fxap + 0xc4,
	14), fxavecs(fxap + 0xfc, 14), fxafl(fxap + 0x1a4, 14)))
fxadel(fxap, 'BankMasterSuction')
fxap = fxanew('BertoFlash')
fxapoke(fxap + 8, fxaplayer)
fxasrand(326)
check('cObjElementFXBertoFlash_RandomPoints', lambda: (SDTest.Call(0x1c4790, 'tvi', fxap), fxavecs(fxap + 0xc4, 60),
	fxavecs(fxap + 0x394, 60)))
fxadel(fxap, 'BertoFlash')
fxap = fxanew('SebastianFlash')
fxasrand(327)
check('cObjElementFXSebastianFlash_RandomPoints', lambda: (SDTest.Call(0x1c48e0, 'tvi', fxap), fxavecs(fxap + 0xc0,
	75), fxavecs(fxap + 0x444, 75)))
fxadel(fxap, 'SebastianFlash')

# the Betty shot needs a particle system: not called; the countermeasure elements of the end of the range are in the
# class loop above
# the Messenger lightning near the player (the branches stop at the floor)
fxapos = fxafl(fxaplayer + 0x28, 3)
fxap = fxanew('MessengerAttack')
for fxal in ((0.0, 50.0, 0.0, 300.0, 80.0, 200.0), (0.0, 0.0, 0.0, 0.0, 0.0, 0.0), (-100.0, 200.0, 50.0, 400.0,
	-30.0, -250.0)):
	fxasrand(328)
	check('cObjElementFXMessengerAttack_SetLine' + fmt(fxal), lambda l=fxal: (SDTest.Call(0x1c8b50, 'tviffffff',
		fxap, fxapos[0] + l[0], fxapos[1] + l[1], fxapos[2] + l[2], fxapos[0] + l[3], fxapos[1] + l[4],
		fxapos[2] + l[5]), fxafl(fxap + 0xc4, 9), fxavecs(fxap + 0x100, 10), fxavecs(fxap + 0x178, 50),
		fxafl(fxap + 0x3d0, 6)))
fxadel(fxap, 'MessengerAttack')

# the data pack trails from the player to itself
fxap = fxanew('MessengerTakeDataPack')
fxapoke(fxap + 4, fxaplayer)
fxapoke(fxap + 8, fxaplayer)
fxasrand(329)
check('cObjElementFXMessengerTakeDataPack_Start', lambda: (SDTest.Call(0x1c9270, 'tvi', fxap), fxavecs(fxap + 0xd0,
	12), fxafl(fxap + 0x160, 4)))
fxadel(fxap, 'MessengerTakeDataPack')

# a debris of a loaded model for the Updates
fxad = fxaz(0x60)
check('fxa debris model', lambda: SDTest.Call(0x20eb10, 'tiis', fxad,
	'Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D')[0] == fxad)
def fxadebris(life):
	fxaset(fxad + 0x1c, (1.0, 2.0, 3.0, 0.1, 0.2, 0.3, 10.0, 20.0, 30.0, 1.0, 1.0, 1.0, life))
def fxamodel():
	return (SDTest.Call(0x20fa30, 'tiiF', fxad, (0.0, 0.0, 0.0))[1], SDTest.Call(0x210830, 'tiiF', fxad,
		(0.0, 0.0, 0.0))[1], fxafl(fxad + 0x4c, 1))
fxaset(fxaworld + 0x2d4, (0.01,))

# no entity: the debris ends (or the Update returns)
for fxau in (('BankMasterAttack', 0x1b6cf0), ('BertoAttack', 0x1b7e20), ('BertoFlash', 0x1b8230), ('BettyKill',
	0x1ba040), ('DTritusAttack', 0x1bc6d0), ('DTritusKill', 0x1bcb10), ('SebastianFlash', 0x1c4030), ('PoliceSteal',
	0x1c3b70), ('BankerSteal', 0x1b7700), ('BankMasterSuction', 0x1b70d0), ('FunctionarySpecialAction', 0x1bcf50),
	('MessengerAttack', 0x1c0f40), ('MessengerTakeDataPack', 0x1c17d0), ('MercenaryDrink', 0x1c03e0),
	('BishopAttack', 0x1ba480), ('BishopSellLife', 0x1bb1b0)):
	fxap = fxanew(fxau[0])
	fxadebris(1.0)
	check('cObjElementFX' + fxau[0] + '_Update none', lambda u=fxau, p=fxap: (SDTest.Call(u[1], 'tviif', p, fxad,
		0.1), fxai(p + 4), fxafl(fxad + 0x4c, 1)))
	fxadel(fxap, fxau[0])

# PoliceSteal: the target missing, the debris near its end, the coin timer short of 0.04
fxap = fxanew('PoliceSteal')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXPoliceSteal_Update no target', lambda: (SDTest.Call(0x1c3b70, 'tviif', fxap, fxad, 0.1),
	fxai(fxap + 8), fxamodel()))
fxapoke(fxap + 8, fxaplayer)
for fxalife, fxat in ((0.5, 0.0), (0.65, 0.0), (1.0, 0.0), (1.0, 0.02)):
	fxadebris(fxalife)
	fxaset(fxap + 0xbc, (fxat,))
	check('cObjElementFXPoliceSteal_Update' + fmt((fxalife, fxat)), lambda: (SDTest.Call(0x1c3b70, 'tviif', fxap,
		fxad, 0.1), fxai(fxap + 4) == fxaplayer, fxai(fxap + 8) == fxaplayer, fxafl(fxap + 0xbc, 1), fxamodel()))
fxadel(fxap, 'PoliceSteal')

# BertoFlash: no target, then the time past the fade out (the debris ends after it follows the entity)
fxap = fxanew('BertoFlash')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXBertoFlash_Update no target', lambda: (SDTest.Call(0x1b8230, 'tviif', fxap, fxad, 0.1),
	fxai(fxap + 8), fxafl(fxad + 0x4c, 1)))
fxapoke(fxap + 8, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0xbc, (0.5, 0.9))
check('cObjElementFXBertoFlash_Update late', lambda: (SDTest.Call(0x1b8230, 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0xbc, 2), fxamodel()))
fxadel(fxap, 'BertoFlash')

# SebastianFlash: past 1.5 s
fxap = fxanew('SebastianFlash')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0xb8, (0.25, 1.6))
check('cObjElementFXSebastianFlash_Update late', lambda: (SDTest.Call(0x1c4030, 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0xb8, 2), fxamodel()))
fxadel(fxap, 'SebastianFlash')

# BankerSteal: the target missing, the debris near its end, the coin wait not reached (the player is its own target)
fxap = fxanew('BankerSteal')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXBankerSteal_Update no target', lambda: (SDTest.Call(0x1b7700, 'tviif', fxap, fxad, 0.1),
	fxai(fxap + 8), fxamodel()))
fxapoke(fxap + 8, fxaplayer)
for fxalife, fxat in ((0.5, 0.0), (1.0, 0.0), (1.0, 0.1)):
	fxadebris(fxalife)
	fxaset(fxap + 0xc0, (fxat,))
	check('cObjElementFXBankerSteal_Update' + fmt((fxalife, fxat)), lambda: (SDTest.Call(0x1b7700, 'tviif', fxap,
		fxad, 0.1), fxafl(fxap + 0xc0, 1), fxaints(fxap, (0xc4, 0xc8)), fxamodel()))
fxadel(fxap, 'BankerSteal')

# BankMasterSuction: no target
fxap = fxanew('BankMasterSuction')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXBankMasterSuction_Update no target', lambda: (SDTest.Call(0x1b70d0, 'tviif', fxap, fxad, 0.1),
	fxafl(fxad + 0x4c, 1)))
fxadel(fxap, 'BankMasterSuction')

# MercenaryDrink: the drop timer short of 0.03
fxap = fxanew('MercenaryDrink')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0xb8, (0.0, 0.25))
check('cObjElementFXMercenaryDrink_Update early', lambda: (SDTest.Call(0x1c03e0, 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0xb8, 2), fxamodel()))
fxadel(fxap, 'MercenaryDrink')

# BishopSellLife: no attacker, the emission timer short of 0.02
fxap = fxanew('BishopSellLife')
fxapoke(fxap + 8, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0x16c, (0.5, 0.0))
check('cObjElementFXBishopSellLife_Update early', lambda: (SDTest.Call(0x1bb1b0, 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0x16c, 2), fxai(fxap + 4)))
fxadel(fxap, 'BishopSellLife')

SDTest.Poke(fxaworld + 0x2d4 - 0x400000, fxadelta)
SDTest.Call(0x20ec10, 'tvi', fxad)
fxafree(fxad)
check('rand after fxcharact', lambda: SDTest.Call(0x323c2f, 'i')[0])
