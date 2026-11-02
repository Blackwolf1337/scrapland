# game World/FX/FXChar.c (execfile'd by sdlevel.py, Python 1.5.2): the character effects on elements made by their
# constructors (FXMisc.c) in calloc memory: the setters with the player Player0 (its byte at 0x48c, set by
# SetAttacker, is restored), the resets that only touch fields and trails, the Updates of MoneyTransfer and Sputnik
# on a debris made of a loaded CGameModel (paths up to the emission: the frame time of the world timer is set by
# hand and restored) and FX_Character with names it ignores. rand is seeded before each use.
# Not called: Attach (needs the models of the effects), the Alive methods (they step a debris of a system), the
# resets that kill particles of a system that is not attached (Conversor, MaintenanceWork, Sputnik), SetMaintenance
# (CHECKs the node DF_Laser), the starts that make debris, the native functions (a parameter file) and the Updates
# of Conversion and MaintenanceWork (they change the model of the player).
def fxci(a):
	return SDTest.Peek(a, '@i')
def fxcb(a):
	return SDTest.Peek(a, '@b')
def fxcz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def fxcfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def fxcfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def fxcset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def fxcints(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs)
def fxcpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fxcsrand(s):
	SDTest.Call('crt_srand', 'vi', s)
fxcworld = SDTest.Peek('g_pWorld', 'i')
fxcplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', fxcworld, 0)[0]
fxcdelta = fxci(fxcworld + 0x2d4)
# the elements: ctor, dtor, size
fxcCONV = (sym('cObjElementFXCharacterConversion_ctor'), sym('cObjElementFXCharacterConversion_dtor'), 0x89c)
fxcCONVOR = (sym('cObjElementFXCharacterConversor_ctor'), sym('cObjElementFXCharacterConversor_dtor'), 0x298)
fxcDEATH = (sym('cObjElementFXCharacterDeath_ctor'), sym('cObjElementFXCharacterDeath_dtor'), 0xf68)
fxcTELE = (sym('cObjElementFXCharacterTeleport_ctor'), sym('cObjElementFXCharacterTeleport_dtor'), 0xd8)
fxcMAINT = (sym('cObjElementFXMaintenanceWork_ctor'), sym('cObjElementFXMaintenanceWork_dtor'), 0xe0)
fxcMONEY = (sym('cObjElementFXMoneyTransfer_ctor'), sym('cObjElementFXMoneyTransfer_dtor'), 0xcc)
fxcSPUT = (sym('cObjElementFXSputnik_ctor'), sym('cObjElementFXSputnik_dtor'), 0xe0)
def fxcnew(c):
	p = fxcz(c[2])
	SDTest.Call(c[0], 'tii', p)
	return p
def fxcdel(p, c):
	SDTest.Call(c[1], 'tvi', p)
	fxcfree(p)

# the setters
fxce = fxcnew(fxcMONEY)
check('cObjElementFXMoneyTransfer_SetEntities', lambda: (SDTest.Call('cObjElementFXMoneyTransfer_SetEntities', 'tviii', fxce, fxcplayer, fxcplayer),
	fxci(fxce + 4) == fxcplayer, fxci(fxce + 8) == fxcplayer))
fxcset(fxce + 0xbc, (0.5,))
check('cObjElementFXMoneyTransfer_Reset', lambda: (SDTest.Call('cObjElementFXMoneyTransfer_Reset', 'tvii', fxce, 0), fxcints(fxce, (4, 8, 0xbc,
	0xc0, 0xc4))))
fxcdel(fxce, fxcMONEY)
fxce = fxcnew(fxcDEATH)
check('cObjElementFXCharacterDeath_SetEntity', lambda: (SDTest.Call('cObjElementFXCharacterDeath_SetEntity', 'tvii', fxce, fxcplayer),
	fxci(fxce + 4) == fxcplayer))
fxcset(fxce + 0x218, (1.0, 2.0, 3.0))
fxcset(fxce + 0x7e4, (1.0, 2.0, 3.0))
check('cObjElementFXCharacterDeath_Reset', lambda: (SDTest.Call('cObjElementFXCharacterDeath_Reset', 'tvii', fxce, 0), fxcfl(fxce + 0x218, 3),
	fxcfl(fxce + 0x7e4, 3), fxcfl(fxce + 0x9c0, 2)))
fxcdel(fxce, fxcDEATH)
fxce = fxcnew(fxcTELE)
fxcset(fxce + 0xbc, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0))
fxcpoke(fxce + 0xbc, 2)
check('cObjElementFXCharacterTeleport_Reset', lambda: (SDTest.Call('cObjElementFXCharacterTeleport_Reset', 'tvii', fxce, 0), fxcints(fxce, (0xbc,
	0xc0, 0xc4, 0xc8, 0xcc, 0xd0))))
fxcdel(fxce, fxcTELE)
fxce = fxcnew(fxcMAINT)
check('cObjElementFXMaintenanceWork_SetShip', lambda: (SDTest.Call('cObjElementFXMaintenanceWork_SetShip', 'tvii', fxce, fxcplayer),
	fxci(fxce + 8) == fxcplayer))
fxcdel(fxce, fxcMAINT)
fxce = fxcz(0x10)
check('FXChar_SetI8', lambda: (SDTest.Call('FXMisc_SetI8@1b43c0', 'tvii', fxce, 1234), fxcints(fxce, (0, 4, 8))))
fxcfree(fxce)

# Conversion: the attacker byte 0x48c of the player is restored after SetAttacker
fxce = fxcnew(fxcCONV)
fxcsaved = fxci(fxcplayer + 0x48c)
check('cObjElementFXCharacterConversion_SetAttacked', lambda: (SDTest.Call('cObjElementFXCharacterConversion_SetAttacked', 'tvii', fxce, fxcplayer),
	fxci(fxce + 8) == fxcplayer))
check('cObjElementFXCharacterConversion_SetAttacked 0', lambda: (SDTest.Call('cObjElementFXCharacterConversion_SetAttacked', 'tvii', fxce, 0),
	fxci(fxce + 8)))
check('cObjElementFXCharacterConversion_SetAttacker', lambda: (SDTest.Call('cObjElementFXCharacterConversion_SetAttacker', 'tvii', fxce, fxcplayer),
	fxci(fxce + 4) == fxcplayer, fxcb(fxcplayer + 0x48c)))
fxcpoke(fxcplayer + 0x48c, fxcsaved)
check('cObjElementFXCharacterConversion_SetAttacker 0', lambda: (SDTest.Call('cObjElementFXCharacterConversion_SetAttacker', 'tvii', fxce, 0),
	fxci(fxce + 4)))
fxcset(fxce + 0x16c, (1.0, 2.0, 3.0, 4.0))
fxcsrand(4711)
check('cObjElementFXCharacterConversion_Reset', lambda: (SDTest.Call('cObjElementFXCharacterConversion_Reset', 'tvii', fxce, 0),
	fxcints(fxce, (4, 8, 0x16c, 0x170, 0x174, 0x178)), fxcfl(fxce + 0x190, 6), fxcfl(fxce + 0x508, 3),
	fxcfl(fxce + 0x514, 3), fxcfl(fxce + 0x63c, 2), fxcfl(fxce + 0x640, 2), fxcfl(fxce + 0x768, 2),
	fxcfl(fxce + 0x894, 1), SDTest.Call('crt_rand', 'i')[0]))
fxcdel(fxce, fxcCONV)

# Conversor: the character in phase 0 (no model effect), the nodes of the conversor (the player has none of them)
fxce = fxcnew(fxcCONVOR)
check('cObjElementFXCharacterConversor_SetCharacter 0', lambda: (SDTest.Call('cObjElementFXCharacterConversor_SetCharacter', 'tvii', fxce, 0),
	fxci(fxce + 8)))
check('cObjElementFXCharacterConversor_SetCharacter', lambda: (SDTest.Call('cObjElementFXCharacterConversor_SetCharacter', 'tvii', fxce, fxcplayer),
	fxci(fxce + 8) == fxcplayer, fxci(fxce + 0xbc)))
check('cObjElementFXCharacterConversor_SetConversor 0', lambda: (SDTest.Call('cObjElementFXCharacterConversor_SetConversor', 'tvii', fxce, 0),
	fxci(fxce + 4)))
check('cObjElementFXCharacterConversor_SetConversor', lambda: (SDTest.Call('cObjElementFXCharacterConversor_SetConversor', 'tvii', fxce, fxcplayer),
	fxci(fxce + 4) == fxcplayer, fxcints(fxce, (0xd8, 0xdc, 0xe0, 0xe4, 0xe8, 0xec, 0xf0, 0xf4, 0xf8, 0xfc, 0x100))))
fxcdel(fxce, fxcCONVOR)

# a debris of a loaded model: the Updates move it
fxcd = fxcz(0x60)
check('fxc debris model', lambda: SDTest.Call('CGameModel_ctor', 'tiis', fxcd,
	'Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D')[0] == fxcd)
def fxcdebris(life):
	fxcset(fxcd + 0x1c, (1.0, 2.0, 3.0, 0.1, 0.2, 0.3, 10.0, 20.0, 30.0, 1.0, 1.0, 1.0, life))
def fxcmodel():
	return (SDTest.Call('CGameModel_GetPos', 'tiiF', fxcd, (0.0, 0.0, 0.0))[1], SDTest.Call('CGameModel_GetRot', 'tiiF', fxcd,
		(0.0, 0.0, 0.0))[1])

# Sputnik: the start on the player (its model has no Sputnik node), the Update up to the node test
fxce = fxcnew(fxcSPUT)
check('FXSputnik_Start', lambda: (SDTest.Call('FXSputnik_Start', 'tvii', fxce, fxcplayer), fxci(fxce + 4) == fxcplayer,
	fxci(fxce + 0xc4)))
check('FXSputnik_SetA', lambda: (SDTest.Call('FXSputnik_SetA', 'tvif', fxce, 2.5), fxcfl(fxce + 0xc0, 1)))
check('FXSputnik_SetB', lambda: (SDTest.Call('FXSputnik_SetB', 'tvif', fxce, 7.0), fxcfl(fxce + 0xc8, 1)))
fxcdebris(1.0)
# no node: the Update ends before the emission even if a model had one
fxcpoke(fxce + 0xc4, -1)
for fxcdt, fxct, fxcdur in ((0.01, 0.0, 0.0), (0.04, 0.0, 0.0), (0.04, 3.0, 2.5), (0.04, 1.0, 2.5)):
	fxcset(fxcworld + 0x2d4, (fxcdt,))
	fxcset(fxce + 0xb8, (0.0, fxct, fxcdur))
	check('cObjElementFXSputnik_Update' + fmt((fxcdt, fxct, fxcdur)), lambda: (SDTest.Call('cObjElementFXSputnik_Update', 'tviif', fxce,
		fxcd, 0.1), fxcfl(fxce + 0xb8, 3), fxci(fxce + 4) == fxcplayer, fxcmodel()))
fxcpoke(fxce + 4, 0)
check('cObjElementFXSputnik_Update no entity', lambda: (SDTest.Call('cObjElementFXSputnik_Update', 'tviif', fxce, fxcd, 0.1),
	fxcfl(fxce + 0xb8, 3)))
fxcdel(fxce, fxcSPUT)

# MoneyTransfer: no entities, a debris at the end of its life, then the coin timer
fxce = fxcnew(fxcMONEY)
fxcset(fxcworld + 0x2d4, (0.01,))
check('cObjElementFXMoneyTransfer_Update none', lambda: (SDTest.Call('cObjElementFXMoneyTransfer_Update', 'tviif', fxce, fxcd, 0.1),
	fxcints(fxce, (4, 8, 0xbc))))
SDTest.Call('cObjElementFXMoneyTransfer_SetEntities', 'tviii', fxce, fxcplayer, fxcplayer)
for fxclife, fxct in ((0.5, 0.0), (1.0, 0.0), (1.0, 0.02)):
	fxcdebris(fxclife)
	fxcset(fxce + 0xbc, (fxct,))
	check('cObjElementFXMoneyTransfer_Update' + fmt((fxclife, fxct)), lambda: (SDTest.Call('cObjElementFXMoneyTransfer_Update', 'tviif', fxce,
		fxcd, 0.1), fxci(fxce + 4) == fxcplayer, fxci(fxce + 8) == fxcplayer, fxcfl(fxce + 0xbc, 1), fxcmodel()))
fxcdel(fxce, fxcMONEY)
SDTest.Poke(fxcworld + 0x2d4 - 0x400000, fxcdelta)
SDTest.Call('CGameModel_Reset', 'tvi', fxcd)
fxcfree(fxcd)

# the effects by name that do nothing for these names
for fxcname in ('Nothing', 'CharacterSputnik', 'CharacterSputnikNothing', 'Ch', 'FunctionarySpecial'):
	check('FX_Character ' + fxcname, lambda n=fxcname: SDTest.Call('FX_Character', 'vis', fxcplayer, n)[0])
check('rand after fxchar', lambda: SDTest.Call('crt_rand', 'i')[0])
