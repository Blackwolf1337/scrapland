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
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def fxafree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def fxafl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxaset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def fxaints(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs)
def fxabytes(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs)
def fxagints(rva, n):
	return map(lambda i, rva=rva: SDTest.Peek(rva + 4 * i, 'i'), range(n))
def fxapoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fxasrand(s):
	SDTest.Call('crt_srand', 'vi', s)
# the floats of n vec3 in chunks of up to 60 floats
def fxavecs(a, n):
	r = ()
	fxak = 0
	while fxak < 3 * n:
		fxac = min(60, 3 * n - fxak)
		r = r + fxafl(a + 4 * fxak, fxac)
		fxak = fxak + fxac
	return r
fxaworld = SDTest.Peek('g_pWorld', 'i')
fxaplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', fxaworld, 0)[0]
fxadelta = fxai(fxaworld + 0x2d4)
# the model of the player through its vtable (GetModel 0x98) and a node of it
fxapmodel = SDTest.Call(fxai(fxai(fxaplayer) + 0x98) - 0x400000, 'tii', fxaplayer)[0]
def fxanode(name):
	return SDTest.Call('CGameModel_FindRenderNode', 'tiis', fxapmodel, name)[0]

# the elements: name, ctor, dtor, Delete, size, vtable, the ints and bytes the constructor writes
fxaclasses = (
	('BankMasterAttack', sym('cObjElementFXBankMasterAttack_ctor'), sym('cObjElementFXBankMasterAttack_dtor'), sym('cObjElementFXBankMasterAttack_Delete'), 0xd4, sym('cObjElementFXBankMasterAttack_vftable'), (4, 0xb8, 0xbc), (0xd0,)),
	('BankMasterSuction', sym('cObjElementFXBankMasterSuction_ctor'), sym('cObjElementFXBankMasterSuction_dtor'), sym('cObjElementFXBankMasterSuction_Delete'), 0x1ec, sym('cObjElementFXBankMasterSuction_vftable'), (4, 8, 0xbc, 0xc0), (0x1e8,)),
	('BankerSteal', sym('cObjElementFXBankerSteal_ctor'), sym('cObjElementFXBankerSteal_dtor'), sym('cObjElementFXBankerSteal_Delete'), 0xd0, sym('cObjElementFXBankerSteal_vftable'), (4, 8, 0xbc, 0xc0, 0xc4, 0xc8), (0xcc,)),
	('BertoAttack', sym('cObjElementFXBertoAttack_ctor'), sym('cObjElementFXBertoAttack_dtor'), sym('cObjElementFXBertoAttack_Delete'), 0xcc, sym('cObjElementFXBertoAttack_vftable'), (4, 0xb8, 0xbc), (0xc8,)),
	('BertoFlash', sym('cObjElementFXBertoFlash_ctor'), sym('cObjElementFXBertoFlash_dtor'), sym('cObjElementFXBertoFlash_Delete'), 0x678, sym('cObjElementFXBertoFlash_vftable'), (4, 8, 0xbc, 0xc0), (0x674,)),
	('BettyAttack', sym('cObjElementFXBettyAttack_ctor'), sym('cObjElementFXBettyAttack_dtor'), sym('cObjElementFXBettyAttack_Delete'), 0x1a8, sym('cObjElementFXBettyAttack_vftable'), (4, 0x168, 0x16c, 0x170), (0x1a4,)),
	('BettyKill', sym('cObjElementFXBettyKill_ctor'), sym('cObjElementFXBettyKill_dtor'), sym('cObjElementFXBettyKill_Delete'), 0x918, sym('cObjElementFXBettyKill_vftable'), (4, 0xb8, 0xbc), (0x914,)),
	('BishopAttack', sym('cObjElementFXBishopAttack_ctor'), sym('cObjElementFXBishopAttack_dtor'), sym('cObjElementFXBishopAttack_Delete'), 0xfc, sym('cObjElementFXBishopAttack_vftable'), (4, 0xc, 0x10), (0xf8,)),
	('BishopSellLife', sym('cObjElementFXBishopSellLife_ctor'), sym('cObjElementFXBishopSellLife_dtor'), sym('cObjElementFXBishopSellLife_Delete'), 0x180, sym('cObjElementFXBishopSellLife_vftable'), (4, 8, 0x16c, 0x170), (0x17c,)),
	('DTritusAttack', sym('cObjElementFXDTritusAttack_ctor'), sym('cObjElementFXDTritusAttack_dtor'), sym('cObjElementFXDTritusAttack_Delete'), 0x1158, sym('cObjElementFXDTritusAttack_vftable'), (4, 0xb8, 0xbc), (0x1154,)),
	('DTritusKill', sym('cObjElementFXDTritusKill_ctor'), sym('cObjElementFXDTritusKill_dtor'), sym('cObjElementFXDTritusKill_Delete'), 0x918, sym('cObjElementFXDTritusKill_vftable'), (4, 0xb8, 0xbc), (0x914,)),
	('FunctionarySpecialAction', sym('cObjElementFXFunctionarySpecialAction_ctor'), sym('cObjElementFXFunctionarySpecialAction_dtor'), sym('cObjElementFXFunctionarySpecialAction_Delete'), 0x1ac, sym('cObjElementFXFunctionarySpecialAction_vftable'), (4, 0xb8, 0xbc), (0x1a8,)),
	('GearMissile', sym('cObjElementFXGearMissile_ctor'), sym('cObjElementFXGearMissile_dtor'), sym('cObjElementFXGearMissile_Delete'), 0x6bc, sym('cObjElementFXGearMissile_vftable'), (4, 0x170, 0x174, 0x178, 0x18c), (0x6b8,)),
	('MercenaryDrink', sym('cObjElementFXMercenaryDrink_ctor'), sym('cObjElementFXMercenaryDrink_dtor'), sym('cObjElementFXMercenaryDrink_Delete'), 0xd4, sym('cObjElementFXMercenaryDrink_vftable'), (4, 0xb8, 0xbc), (0xd0,)),
	('MessengerAttack', sym('cObjElementFXMessengerAttack_ctor'), sym('cObjElementFXMessengerAttack_dtor'), sym('cObjElementFXMessengerAttack_Delete'), 0x3ec, sym('cObjElementFXMessengerAttack_vftable'), (4, 0xc, 0x10), (0x3e8,)),
	('MessengerTakeDataPack', sym('cObjElementFXMessengerTakeDataPack_ctor'), sym('cObjElementFXMessengerTakeDataPack_dtor'), sym('cObjElementFXMessengerTakeDataPack_Delete'), 0x174, sym('cObjElementFXMessengerTakeDataPack_vftable'), (4, 0xc), (0x170,)),
	('PoliceGearConversion', sym('cObjElementFXPoliceGearConversion_ctor'), sym('cObjElementFXPoliceGearConversion_dtor'), sym('cObjElementFXPoliceGearConversion_Delete'), 0xa54, sym('cObjElementFXPoliceGearConversion_vftable'), (4, 8, 0xc), (0xa50,)),
	('PoliceSteal', sym('cObjElementFXPoliceSteal_ctor'), sym('cObjElementFXPoliceSteal_dtor'), sym('cObjElementFXPoliceSteal_Delete'), 0xcc, sym('cObjElementFXPoliceSteal_vftable'), (4, 8, 0xbc, 0xc0, 0xc4), (0xc8,)),
	('SebastianFlash', sym('cObjElementFXSebastianFlash_ctor'), sym('cObjElementFXSebastianFlash_dtor'), sym('cObjElementFXSebastianFlash_Delete'), 0x7e0, sym('cObjElementFXSebastianFlash_vftable'), (4, 0xb8, 0xbc), (0x7dc,)),
	('DebrisCloud', sym('cObjElementFXDebrisCloud_ctor'), sym('cObjElementFXDebrisCloud_dtor'), sym('cObjElementFXDebrisCloud_Delete'), 0x1f4, sym('cObjElementFXDebrisCloud_vftable'), (4, 0xbc, 0xc0), (0x1f0,)),
	('EMI', sym('cObjElementFXEMI_ctor'), sym('cObjElementFXEMI_dtor'), sym('cObjElementFXEMI_Delete'), 0xd20, sym('cObjElementFXEMI_vftable'), (4, 0xbc, 0xc0, 0xc4, 0xd14, 0xc9c), (0xd1c,)),
	('Hook', sym('cObjElementFXHook_ctor'), sym('cObjElementFXHook_dtor'), sym('cObjElementFXHook_Delete'), 0x15c, sym('cObjElementFXHook_vftable'), (4, 8, 0x98, 0x14c), (0x158,)),
	('SonicExplosion', sym('cObjElementFXSonicExplosion_ctor'), sym('cObjElementFXSonicExplosion_dtor'), sym('cObjElementFXSonicExplosion_Delete'), 0x220, sym('cObjElementFXSonicExplosion_vftable'), (0x164, 0x168), (0x21c,)),
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
		symva('cObjElementAbstract_vftable')))
	SDTest.Call(fxac[1], 'tii', fxap)
	check('cObjElementFX' + fxac[0] + '_Delete', lambda c=fxac, p=fxap: (SDTest.Call(c[3], 'tiii', p, 0)[0] == p,
		fxai(p) == symva('cObjElementAbstract_vftable')))
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
for fxar in (('BankMasterAttack', sym('cObjElementFXBankMasterAttack_Reset'), (0xb8, 0xbc, 0xcc)), ('BankMasterSuction', sym('cObjElementFXBankMasterSuction_Reset'), (0xbc, 0xc0, 0x1e0,
	0x1e4)), ('BankerSteal', sym('cObjElementFXBankerSteal_Reset'), (4, 8, 0xc0)), ('MercenaryDrink', sym('cObjElementFXMercenaryDrink_Reset'), (0xb8, 0xbc)), ('PoliceSteal',
	sym('cObjElementFXPoliceSteal_Reset'), (4, 8, 0xbc))):
	fxap = fxanew(fxar[0])
	for fxao in fxar[2]:
		fxapoke(fxap + fxao, 0x3f800000 + fxao)
	check('cObjElementFX' + fxar[0] + '_Reset', lambda r=fxar, p=fxap: (SDTest.Call(r[1], 'tvii', p, 0),
		fxaints(p, r[2])))
	fxadel(fxap, fxar[0])

# the setters
fxap = fxanew('PoliceSteal')
check('FXMisc_SetEntity PoliceSteal', lambda: (SDTest.Call('FXMisc_SetEntity@1c6db0', 'tvii', fxap, fxaplayer), fxai(fxap + 4) ==
	fxaplayer))
for fxaf in ('FXCharActions_SetEntity8', 'FXCharActions_SetEntity8@1c84d0', 'FXCharActions_SetEntity8@1c8510'):
	fxapoke(fxap + 8, 0)
	check('FXCharActions_SetEntity8 %s' % fxaf, lambda f=fxaf: (SDTest.Call(f, 'tvii', fxap, fxaplayer),
		fxai(fxap + 8) == fxaplayer))
fxadel(fxap, 'PoliceSteal')
fxap = fxanew('BankerSteal')
check('cObjElementFXBankerSteal_SetEntity', lambda: (SDTest.Call('cObjElementFXBankerSteal_SetEntity', 'tvii', fxap, fxaplayer), fxai(fxap + 4) ==
	fxaplayer, fxai(fxap + 0xbc)))
check('FXMisc_SetI8 BankerSteal', lambda: (SDTest.Call('FXMisc_SetI8@1c6f00', 'tvii', fxap, 77), fxai(fxap + 8)))
fxadel(fxap, 'BankerSteal')
fxap = fxanew('GearMissile')
fxadel(fxap, 'GearMissile')
fxap = fxanew('BishopSellLife')
check('cObjElementFXBishopSellLife_SetAttacked', lambda: (SDTest.Call('cObjElementFXBishopSellLife_SetAttacked', 'tvii', fxap, fxaplayer),
	fxai(fxap + 8) == fxaplayer))
check('cObjElementFXBishopSellLife_SetAttacked 0', lambda: (SDTest.Call('cObjElementFXBishopSellLife_SetAttacked', 'tvii', fxap, 0), fxai(fxap + 8)))
check('cObjElementFXBishopSellLife_SetAttacker 0', lambda: (SDTest.Call('cObjElementFXBishopSellLife_SetAttacker', 'tvii', fxap, 0), fxai(fxap + 4)))
check('fxa hands', lambda: (fxanode('R_Bishop_Mano_Izq'), fxanode('R_Bishop_Mano_Der')))
if fxanode('R_Bishop_Mano_Izq') > -1 and fxanode('R_Bishop_Mano_Der') > -1:
	check('cObjElementFXBishopSellLife_SetAttacker', lambda: (SDTest.Call('cObjElementFXBishopSellLife_SetAttacker', 'tvii', fxap, fxaplayer),
		fxai(fxap + 4) == fxaplayer, fxaints(fxap, (0x174, 0x178))))
fxadel(fxap, 'BishopSellLife')
fxap = fxanew('MercenaryDrink')
check('cObjElementFXMercenaryDrink_SetC0', lambda: (SDTest.Call('cObjElementFXMercenaryDrink_SetC0', 'tvifff', fxap, 1.5, -2.25, 1e30),
	fxafl(fxap + 0xc0, 3)))
check('cObjElementFXMercenaryDrink_Start', lambda: (SDTest.Call('cObjElementFXMercenaryDrink_Start', 'tvii', fxap, fxaplayer), fxai(fxap + 4) ==
	fxaplayer, fxai(fxap + 0xcc)))
fxadel(fxap, 'MercenaryDrink')
for fxas in (('BankMasterAttack', sym('cObjElementFXBankMasterAttack_Start'), 'DF_Batton', 0xc8), ('BertoAttack', sym('cObjElementFXBertoAttack_Start'), 'R_Berto_TroncoInf-cristal',
	0xc4)):
	check('fxa node ' + fxas[2], lambda s=fxas: fxanode(s[2]))
	if fxanode(fxas[2]) > -1:
		fxap = fxanew(fxas[0])
		check('cObjElementFX' + fxas[0] + '_Start', lambda s=fxas, p=fxap: (SDTest.Call(s[1], 'tvii', p, fxaplayer),
			fxai(p + 4) == fxaplayer, fxai(p + s[3])))
		fxadel(fxap, fxas[0])
fxap = fxanew('BettyKill')
check('cObjElementFXKill_SetAngles', lambda: (SDTest.Call('cObjElementFXKill_SetAngles', 'tvifff', fxap, 0.5, 1.0, -1.5),
	SDTest.Call('cObjElementFXKill_SetAngles@1c8580', 'tvifff', fxap, 0.25, 2.0, 3.0), fxafl(fxap + 0x908, 3)))
for fxaf in ('cObjElementFXKill_Start', 'cObjElementFXKill_Start@1c7810'):
	fxasrand(321)
	check('cObjElementFXKill_Start %s' % fxaf, lambda f=fxaf: (SDTest.Call(f, 'tvii', fxap, fxaplayer),
		fxai(fxap + 4) == fxaplayer, fxaints(fxap + 0xc0, range(0, 40, 4)), fxavecs(fxap + 0x1ec, 20),
		fxafl(fxap + 0x570, 30), fxafl(fxap + 0x69c, 30), fxafl(fxap + 0x7c8, 30), SDTest.Call('crt_rand', 'i')[0]))
fxadel(fxap, 'BettyKill')
check('fxa node R_Detritus_Tronco', lambda: fxanode('R_Detritus_Tronco'))
if fxanode('R_Detritus_Tronco') > -1:
	fxap = fxanew('DTritusAttack')
	fxasrand(322)
	check('cObjElementFXDTritusAttack_Start', lambda: (SDTest.Call('cObjElementFXDTritusAttack_Start', 'tvii', fxap, fxaplayer),
		fxai(fxap + 0xc0), fxaints(fxap + 0xc4, range(0, 40, 4)), fxavecs(fxap + 0x31c, 20), fxafl(fxap + 0xa24, 30),
		fxafl(fxap + 0xc7c, 30), fxafl(fxap + 0xed4, 30), SDTest.Call('crt_rand', 'i')[0]))
	fxadel(fxap, 'DTritusAttack')
fxap = fxanew('BishopAttack')
for fxal in ((0.0, 0.0, 0.0, 3.0, 4.0, 12.0), (1.0, 2.0, 3.0, 1.0, 2.0, 3.0), (-5.5, 100.0, 7.0, 1e-20, -3e10, 0.5)):
	check('cObjElementFXBishopAttack_SetLine' + fmt(fxal), lambda l=fxal: (SDTest.Call('cObjElementFXBishopAttack_SetLine', 'tviffffff', fxap,
		l[0], l[1], l[2], l[3], l[4], l[5]), fxafl(fxap + 0xd4, 9)))
fxadel(fxap, 'BishopAttack')

# the Functionary start on the player if it is a WalkChar (the vertices are searched once and kept in the exe)
fxawalk = SDTest.Call(fxai(fxai(fxaplayer) + 0x14) - 0x400000, 'tbis', fxaplayer, 'WalkChar')[0]
check('fxa player WalkChar', lambda: (fxawalk, SDTest.Peek('g_FXFunctionaryInit', 'b')))
if fxawalk:
	fxap = fxanew('FunctionarySpecialAction')
	fxasrand(323)
	check('cObjElementFXFunctionarySpecialAction_Start', lambda: (SDTest.Call('cObjElementFXFunctionarySpecialAction_Start', 'tvii', fxap, fxaplayer),
		SDTest.Peek('g_FXFunctionaryInit', 'b'), fxagints(sym('g_FXFunctionaryVertex'), 14), fxagints(sym('g_FXFunctionaryColor'), 14),
		fxavecs(fxap + 0xfc, 14), SDTest.Call('crt_rand', 'i')[0]))
	fxasrand(324)
	check('cObjElementFXFunctionarySpecialAction_Start again', lambda: (SDTest.Call('cObjElementFXFunctionarySpecialAction_Start', 'tvii', fxap,
		fxaplayer), fxagints(sym('g_FXFunctionaryColor'), 14), fxavecs(fxap + 0xfc, 14)))
	fxadel(fxap, 'FunctionarySpecialAction')

# the random points of the starts
fxap = fxanew('BankMasterSuction')
fxapoke(fxap + 4, fxaplayer)
fxapoke(fxap + 8, fxaplayer)
fxasrand(325)
check('cObjElementFXBankMasterSuction_RandomPoints', lambda: (SDTest.Call('cObjElementFXBankMasterSuction_RandomPoints', 'tvi', fxap), fxafl(fxap + 0xc4,
	14), fxavecs(fxap + 0xfc, 14), fxafl(fxap + 0x1a4, 14)))
fxadel(fxap, 'BankMasterSuction')
fxap = fxanew('BertoFlash')
fxapoke(fxap + 8, fxaplayer)
fxasrand(326)
check('cObjElementFXBertoFlash_RandomPoints', lambda: (SDTest.Call('cObjElementFXBertoFlash_RandomPoints', 'tvi', fxap), fxavecs(fxap + 0xc4, 60),
	fxavecs(fxap + 0x394, 60)))
fxadel(fxap, 'BertoFlash')
fxap = fxanew('SebastianFlash')
fxasrand(327)
check('cObjElementFXSebastianFlash_RandomPoints', lambda: (SDTest.Call('cObjElementFXSebastianFlash_RandomPoints', 'tvi', fxap), fxavecs(fxap + 0xc0,
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
	check('cObjElementFXMessengerAttack_SetLine' + fmt(fxal), lambda l=fxal: (SDTest.Call('cObjElementFXMessengerAttack_SetLine', 'tviffffff',
		fxap, fxapos[0] + l[0], fxapos[1] + l[1], fxapos[2] + l[2], fxapos[0] + l[3], fxapos[1] + l[4],
		fxapos[2] + l[5]), fxafl(fxap + 0xc4, 9), fxavecs(fxap + 0x100, 10), fxavecs(fxap + 0x178, 50),
		fxafl(fxap + 0x3d0, 6)))
fxadel(fxap, 'MessengerAttack')

# the data pack trails from the player to itself
fxap = fxanew('MessengerTakeDataPack')
fxapoke(fxap + 4, fxaplayer)
fxapoke(fxap + 8, fxaplayer)
fxasrand(329)
check('cObjElementFXMessengerTakeDataPack_Start', lambda: (SDTest.Call('cObjElementFXMessengerTakeDataPack_Start', 'tvi', fxap), fxavecs(fxap + 0xd0,
	12), fxafl(fxap + 0x160, 4)))
fxadel(fxap, 'MessengerTakeDataPack')

# a debris of a loaded model for the Updates
fxad = fxaz(0x60)
check('fxa debris model', lambda: SDTest.Call('CGameModel_ctor', 'tiis', fxad,
	'Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D')[0] == fxad)
def fxadebris(life):
	fxaset(fxad + 0x1c, (1.0, 2.0, 3.0, 0.1, 0.2, 0.3, 10.0, 20.0, 30.0, 1.0, 1.0, 1.0, life))
def fxamodel():
	return (SDTest.Call('CGameModel_GetPos', 'tiiF', fxad, (0.0, 0.0, 0.0))[1], SDTest.Call('CGameModel_GetRot', 'tiiF', fxad,
		(0.0, 0.0, 0.0))[1], fxafl(fxad + 0x4c, 1))
fxaset(fxaworld + 0x2d4, (0.01,))

# no entity: the debris ends (or the Update returns)
for fxau in (('BankMasterAttack', sym('cObjElementFXBankMasterAttack_Update')), ('BertoAttack', sym('cObjElementFXBertoAttack_Update')), ('BertoFlash', sym('cObjElementFXBertoFlash_Update')), ('BettyKill',
	sym('cObjElementFXBettyKill_Update')), ('DTritusAttack', sym('cObjElementFXDTritusAttack_Update')), ('DTritusKill', sym('cObjElementFXDTritusKill_Update')), ('SebastianFlash', sym('cObjElementFXSebastianFlash_Update')), ('PoliceSteal',
	sym('cObjElementFXPoliceSteal_Update')), ('BankerSteal', sym('cObjElementFXBankerSteal_Update')), ('BankMasterSuction', sym('cObjElementFXBankMasterSuction_Update')), ('FunctionarySpecialAction', sym('cObjElementFXFunctionarySpecialAction_Update')),
	('MessengerAttack', sym('cObjElementFXMessengerAttack_Update')), ('MessengerTakeDataPack', sym('cObjElementFXMessengerTakeDataPack_Update')), ('MercenaryDrink', sym('cObjElementFXMercenaryDrink_Update')),
	('BishopAttack', sym('cObjElementFXBishopAttack_Update')), ('BishopSellLife', sym('cObjElementFXBishopSellLife_Update'))):
	fxap = fxanew(fxau[0])
	fxadebris(1.0)
	check('cObjElementFX' + fxau[0] + '_Update none', lambda u=fxau, p=fxap: (SDTest.Call(u[1], 'tviif', p, fxad,
		0.1), fxai(p + 4), fxafl(fxad + 0x4c, 1)))
	fxadel(fxap, fxau[0])

# PoliceSteal: the target missing, the debris near its end, the coin timer short of 0.04
fxap = fxanew('PoliceSteal')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXPoliceSteal_Update no target', lambda: (SDTest.Call('cObjElementFXPoliceSteal_Update', 'tviif', fxap, fxad, 0.1),
	fxai(fxap + 8), fxamodel()))
fxapoke(fxap + 8, fxaplayer)
for fxalife, fxat in ((0.5, 0.0), (0.65, 0.0), (1.0, 0.0), (1.0, 0.02)):
	fxadebris(fxalife)
	fxaset(fxap + 0xbc, (fxat,))
	check('cObjElementFXPoliceSteal_Update' + fmt((fxalife, fxat)), lambda: (SDTest.Call('cObjElementFXPoliceSteal_Update', 'tviif', fxap,
		fxad, 0.1), fxai(fxap + 4) == fxaplayer, fxai(fxap + 8) == fxaplayer, fxafl(fxap + 0xbc, 1), fxamodel()))
fxadel(fxap, 'PoliceSteal')

# BertoFlash: no target, then the time past the fade out (the debris ends after it follows the entity)
fxap = fxanew('BertoFlash')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXBertoFlash_Update no target', lambda: (SDTest.Call('cObjElementFXBertoFlash_Update', 'tviif', fxap, fxad, 0.1),
	fxai(fxap + 8), fxafl(fxad + 0x4c, 1)))
fxapoke(fxap + 8, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0xbc, (0.5, 0.9))
check('cObjElementFXBertoFlash_Update late', lambda: (SDTest.Call('cObjElementFXBertoFlash_Update', 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0xbc, 2), fxamodel()))
fxadel(fxap, 'BertoFlash')

# SebastianFlash: past 1.5 s
fxap = fxanew('SebastianFlash')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0xb8, (0.25, 1.6))
check('cObjElementFXSebastianFlash_Update late', lambda: (SDTest.Call('cObjElementFXSebastianFlash_Update', 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0xb8, 2), fxamodel()))
fxadel(fxap, 'SebastianFlash')

# BankerSteal: the target missing, the debris near its end, the coin wait not reached (the player is its own target)
fxap = fxanew('BankerSteal')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXBankerSteal_Update no target', lambda: (SDTest.Call('cObjElementFXBankerSteal_Update', 'tviif', fxap, fxad, 0.1),
	fxai(fxap + 8), fxamodel()))
fxapoke(fxap + 8, fxaplayer)
for fxalife, fxat in ((0.5, 0.0), (1.0, 0.0), (1.0, 0.1)):
	fxadebris(fxalife)
	fxaset(fxap + 0xc0, (fxat,))
	check('cObjElementFXBankerSteal_Update' + fmt((fxalife, fxat)), lambda: (SDTest.Call('cObjElementFXBankerSteal_Update', 'tviif', fxap,
		fxad, 0.1), fxafl(fxap + 0xc0, 1), fxaints(fxap, (0xc4, 0xc8)), fxamodel()))
fxadel(fxap, 'BankerSteal')

# BankMasterSuction: no target
fxap = fxanew('BankMasterSuction')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
check('cObjElementFXBankMasterSuction_Update no target', lambda: (SDTest.Call('cObjElementFXBankMasterSuction_Update', 'tviif', fxap, fxad, 0.1),
	fxafl(fxad + 0x4c, 1)))
fxadel(fxap, 'BankMasterSuction')

# MercenaryDrink: the drop timer short of 0.03
fxap = fxanew('MercenaryDrink')
fxapoke(fxap + 4, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0xb8, (0.0, 0.25))
check('cObjElementFXMercenaryDrink_Update early', lambda: (SDTest.Call('cObjElementFXMercenaryDrink_Update', 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0xb8, 2), fxamodel()))
fxadel(fxap, 'MercenaryDrink')

# BishopSellLife: no attacker, the emission timer short of 0.02
fxap = fxanew('BishopSellLife')
fxapoke(fxap + 8, fxaplayer)
fxadebris(1.0)
fxaset(fxap + 0x16c, (0.5, 0.0))
check('cObjElementFXBishopSellLife_Update early', lambda: (SDTest.Call('cObjElementFXBishopSellLife_Update', 'tviif', fxap, fxad, 0.1),
	fxafl(fxap + 0x16c, 2), fxai(fxap + 4)))
fxadel(fxap, 'BishopSellLife')

SDTest.Poke(fxaworld + 0x2d4 - 0x400000, fxadelta)
SDTest.Call('CGameModel_Reset', 'tvi', fxad)
fxafree(fxad)
check('rand after fxcharact', lambda: SDTest.Call('crt_rand', 'i')[0])
