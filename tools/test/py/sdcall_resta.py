# game leftovers below rva 0x1a8000, "Rest A" (execfile'd by sdcall.py after its other checks, Python 1.5.2): the
# pure parts on heap memory: the Steam ID check, the Steam context clear and stubs, the dot test of SAIPy, the
# constructors and destructors of the input actions, sets and text input, the key press on the virtual keyboard,
# the node turn of an action class and the destructors of the weapon and ammo infos. Not called: what needs Steam or
# GOG (the interfaces, the ticket and the ID), the input sets of the exe (rebuilt only at the start).
def rsai(a):
	return SDTest.Peek(a, '@i')
def rsab(a):
	return SDTest.Peek(a, '@b')
def rsaz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def rsafree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def rsawi(a, v):
	SDTest.Poke(a - 0x400000, v)
def rsaf(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def rsawf(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def rsaints(a, n):
	return tuple(map(lambda i, a=a: SDTest.Peek(a + 4 * i, '@i'), range(n)))
def rsabytes(a, n):
	return tuple(map(lambda i, a=a: SDTest.Peek(a + i, '@b'), range(n)))

# CSteamID::IsValid on (account, bits)
rsaid = rsaz(8)
for rsav in ((1, 0x01100000), (0, 0x01100000), (1, 0x01100005), (1, 0x01100004), (5, 0x01700000), (5, 0x01700001),
		(0, 0x01300000), (7, 0x01300000), (7, 0x00300000), (7, 0x05300000), (7, 0x04400000), (7, 0x01b00000),
		(7, 0x01a00000), (7, 0x01000000), (7, -0x7f000000 + 0x00100001), (3, 0x02200000)):
	rsawi(rsaid, rsav[0])
	rsawi(rsaid + 4, rsav[1])
	check('SteamID_IsValid' + fmt(rsav), lambda: SDTest.Call('SteamID_IsValid', 'tbi', rsaid)[0])
rsafree(rsaid)

# the context cleared, the stubs
rsactx = rsaz(0x60)
for rsai2 in range(0x18):
	rsawi(rsactx + 4 * rsai2, 0x1000 + rsai2)
check('SteamContext_Clear', lambda: (SDTest.Call('SteamContext_Clear', 'tvi', rsactx), rsaints(rsactx, 0x18)))
for rsai2 in range(6):
	rsawi(rsactx + 4 * rsai2, -1)
check('SteamStub_Clear3', lambda: (SDTest.Call('SteamStub_Clear3', 'viii', rsactx, rsactx + 8, rsactx + 0x10), rsaints(rsactx, 6)))
rsawi(rsactx, 77)
check('SteamStub_Clear1', lambda: (SDTest.Call('SteamStub_Clear1', 'ii', rsactx)[0], rsai(rsactx)))
check('CCallbackImpl_GetSize', lambda: SDTest.Call('CCallbackImpl_GetSize', 'tii', rsactx)[0])
# the ticket response on a fake callback object: ticket 0x1234 at 0x14
for rsav in ((0x1234, 1), (0x1234, 2), (0x1235, 1)):
	rsawi(rsactx + 0x14, 0x1234)
	rsawi(rsactx + 0x18, 0x0100)
	rsawi(rsactx + 0x40, rsav[0])
	rsawi(rsactx + 0x44, rsav[1])
	check('SteamTicket_OnResponse' + fmt(rsav), lambda: (SDTest.Call('SteamTicket_OnResponse', 'tvii', rsactx, rsactx + 0x40),
		rsabytes(rsactx + 0x18, 2)))
rsafree(rsactx)

# the dot products of a point with vectors against r * r
rsavec = rsaz(0x30)
rsawf(rsavec, (1.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.5, 0.5, 0.5, -1.0, 0.25, 3.0))
for rsav in ((4, (3.0, 2.0, 1.0), 1.0), (4, (3.0, 2.0, 1.0), 2.0), (3, (1.0, 1.0, 1.0), 1.0), (0, (0.0, 0.0, 0.0), 5.0),
		(2, (0.5, 0.25, 0.0), 0.5), (4, (1.0, 1.0, 1.0), 0.0), (1, (2.0, 0.0, 0.0), -1.5)):
	check('Vec3_AllDotsReach' + fmt(rsav), lambda rsav=rsav: SDTest.Call('Vec3_AllDotsReach', 'biiFf', rsavec, rsav[0], rsav[1],
		rsav[2])[0])
rsafree(rsavec)

# the turned node of an action class
rsanm = rsaz(0x40)
for rsai2 in range(0x10):
	rsawi(rsanm + 4 * rsai2, 0x01010101)
check('cActNodeMod_ctor', lambda: (SDTest.Call('cActNodeMod_ctor', 'tii', rsanm)[0] == rsanm, rsaints(rsanm, 0x10)))
rsafree(rsanm)

# the input objects: constructors, destructors (names set through cString_Set) and the vector deleting destructors
rsaia = rsaz(0x80)
check('cInputAction_ctor', lambda: (SDTest.Call('cInputAction_ctor', 'tii', rsaia)[0] == rsaia, rsai(rsaia + 4),
	rsai(rsaia + 0x28 + 8), rsai(rsaia + 0x64 + 8)))
SDTest.Call('cString_Set', 'tvis', rsaia + 4, 'Jump')
check('cInputAction_dtor', lambda: (SDTest.Call('cInputAction_dtor', 'tvi', rsaia), rsai(rsaia + 4)))
check('cInputAction_Delete 0', lambda: (SDTest.Call('cInputAction_ctor', 'tii', rsaia)[0] == rsaia,
	SDTest.Call('cInputAction_Delete', 'tiii', rsaia, 0)[0] == rsaia, rsai(rsaia + 4)))
rsafree(rsaia)
rsaas = rsaz(0x10)
check('cActionSet_ctor', lambda: (SDTest.Call('cActionSet_ctor', 'tii', rsaas)[0] == rsaas, rsaints(rsaas, 3)))
SDTest.Call('cString_Set', 'tvis', rsaas, 'Fly')
# a table of 2 actions made by cActionSet_Init, freed by the destructor
check('cActionSet_Init', lambda: (SDTest.Call('cActionSet_Init', 'tviis', rsaas, 2, 'Fly'), rsai(rsaas + 4), rsai(rsaas + 8) != 0))
check('cActionSet_dtor', lambda: (SDTest.Call('cActionSet_dtor', 'tvi', rsaas), rsaints(rsaas, 3)))
SDTest.Call('cActionSet_ctor', 'tii', rsaas)
check('cActionSet_Delete 0', lambda: (SDTest.Call('cActionSet_Delete', 'tiii', rsaas, 0)[0] == rsaas, rsaints(rsaas, 3)))
rsafree(rsaas)
# a table of 3 sets: Init, then the Free of Table.h
rsatab = rsaz(8)
check('ActionSetTable_Init', lambda: (SDTest.Call('ActionSetTable_Init', 'tvii', rsatab, 3), rsai(rsatab), rsai(rsatab + 4) != 0))
check('ActionSetTable_Free', lambda: (SDTest.Call('ActionSetTable_Free', 'tvi', rsatab), rsaints(rsatab, 2)))
rsafree(rsatab)
# the vector deleting destructor of new cInputAction[2] (flags 3): returns the block
rsablk = SDTest.Call('operator_new_array', 'ii', 4 + 2 * 0x7c)[0]
rsawi(rsablk, 2)
SDTest.Call('cInputAction_ctor', 'tii', rsablk + 4)
SDTest.Call('cInputAction_ctor', 'tii', rsablk + 4 + 0x7c)
check('cInputAction_Delete 3', lambda: SDTest.Call('cInputAction_Delete', 'tiii', rsablk + 4, 3)[0] == rsablk)

rsapi = rsaz(0xb4)
for rsai2 in range(0xb4 / 4):
	rsawi(rsapi + 4 * rsai2, 0x11111111)
check('cPlayerInput_ctor', lambda: (SDTest.Call('cPlayerInput_ctor', 'tii', rsapi)[0] == rsapi, rsaints(rsapi, 2),
	rsabytes(rsapi + 8, 0x80) == (0,) * 0x80, rsaints(rsapi + 0x88, 5), rsai(rsapi + 0x9c + 8), rsab(rsapi + 0xb0)))
# the key press: the cursor on a cell, the text grows, backspace, the limit
for rsav in ((0.5, 0.5, 0, 0), (1.5, 0.5, 0, 0), (1.5, 0.5, 1, 0), (1.5, 0.5, 0, 1), (100.0, 100.0, 0, 0)):
	rsawf(rsapi + 0x8c, (rsav[0], rsav[1]))
	SDTest.Poke(rsapi + 0x94 - 0x400000, rsav[2] | rsav[3] << 8)
	check('cPlayerInput_PressKey' + fmt(rsav), lambda: (SDTest.Call('cPlayerInput_PressKey', 'tvii', rsapi, 0), rsaints(rsapi, 2),
		SDTest.Peek(rsapi + 8, '@s'), rsabytes(rsapi + 0x94, 2), SDTest.Call('cPlayerInput_GetCursorChar', 'tbi', rsapi)[0]))
check('cPlayerInput_dtor', lambda: (SDTest.Call('cPlayerInput_dtor', 'tvi', rsapi), rsai(rsapi + 0x9c + 8)))
rsafree(rsapi)
check('Input_GetActionSet', lambda: SDTest.Call('Input_GetActionSet', 'i')[0])

# the hold of the presses over a set change: a set with 2 actions, pressed bytes and values set by hand
rsaas = rsaz(0x10)
SDTest.Call('cActionSet_ctor', 'tii', rsaas)
SDTest.Call('cActionSet_Init', 'tviis', rsaas, 2, 'Hold')
rsaact = rsai(rsaas + 8)
for rsai2 in range(2):
	rsawi(rsaact + 0x7c * rsai2, 0x01000101)
	rsawf(rsaact + 0x7c * rsai2 + 8, (0.1, 0.2, 0.3, 0.4, 0.75, 0.5, 0.25, 1.0))
check('cActionSet_HoldPressed', lambda: (SDTest.Call('cActionSet_HoldPressed', 'tvi', rsaas), rsabytes(rsaact, 4), rsaf(rsaact + 8, 8),
	rsabytes(rsaact + 0x7c, 4), rsaf(rsaact + 0x7c + 8, 8)))
SDTest.Call('cActionSet_dtor', 'tvi', rsaas)
rsafree(rsaas)

# the destructors of the infos of the weapon tables on objects of the exe constructors
rsawp = rsaz(0xb0)
SDTest.Call('cWeapInfo_ctor', 'tii', rsawp)
SDTest.Call('cString_Set', 'tvis', rsawp, 'Laser')
check('cWeapInfo_dtor', lambda: (SDTest.Call('cWeapInfo_dtor', 'tvi', rsawp), rsai(rsawp), rsai(rsawp + 4 + 8)))
SDTest.Call('cWeapInfo_ctor', 'tii', rsawp)
check('cWeapInfo_Delete 0', lambda: (SDTest.Call('cWeapInfo_Delete', 'tiii', rsawp, 0)[0] == rsawp, rsai(rsawp)))
rsafree(rsawp)
rsaam = rsaz(0x5c)
SDTest.Call('cAmmoInfo_ctor', 'tii', rsaam)
SDTest.Call('cString_Set', 'tvis', rsaam, 'Bullets')
check('cAmmoInfo_dtor', lambda: (SDTest.Call('cAmmoInfo_dtor', 'tvi', rsaam), rsai(rsaam), rsai(rsaam + 0xc + 8)))
SDTest.Call('cAmmoInfo_ctor', 'tii', rsaam)
check('cAmmoInfo_Delete 0', lambda: (SDTest.Call('cAmmoInfo_Delete', 'tiii', rsaam, 0)[0] == rsaam, rsai(rsaam)))
rsafree(rsaam)
# the table Init of both on a fresh table, freed with the vector deleting destructors (flags 3)
rsatab = rsaz(8)
check('WeapTable_Init', lambda: (SDTest.Call('WeapTable_Init', 'tvii', rsatab, 3), rsai(rsatab), rsai(rsatab + 4) != 0))
check('cWeapInfo_Delete 3', lambda: SDTest.Call('cWeapInfo_Delete', 'tiii', rsai(rsatab + 4), 3)[0] == rsai(rsatab + 4) - 4)
rsawi(rsatab, 0)
rsawi(rsatab + 4, 0)
check('AmmoTable_Init', lambda: (SDTest.Call('AmmoTable_Init', 'tvii', rsatab, 2), rsai(rsatab), rsai(rsatab + 4) != 0))
check('cAmmoInfo_Delete 3', lambda: SDTest.Call('cAmmoInfo_Delete', 'tiii', rsai(rsatab + 4), 3)[0] == rsai(rsatab + 4) - 4)
rsafree(rsatab)

# the MSVC std::string of the GOG code on strings of our own (0x18 bytes: the small buffer or the pointer, size,
# capacity): assign, insert, erase, the parts of itself, operator+ with a moved string, to_string, the digits
def rsastr(s):
	cap = rsai(s + 0x14)
	if cap >= 16:
		p = SDTest.Peek(s, '@p')
	else:
		p = SDTest.Peek(s, '@s')
	return (p, rsai(s + 0x10), cap)
def rsanew():
	s = rsaz(0x18)
	rsawi(s + 0x14, 15)
	return s
def rsadel(s):
	SDTest.Call('MsvcString_Tidy', 'tvi', s)
	rsafree(s)
rsas = rsanew()
for rsav in ('hello', 'a string longer than fifteen', '', 'exactly15chars!', 'sixteen chars ab'):
	check('MsvcString_Assign' + fmt((rsav,)), lambda rsav=rsav: (SDTest.Call('MsvcString_Assign', 'tiisi', rsas, rsav, len(rsav))[0] == rsas,
		rsastr(rsas)))
SDTest.Call('MsvcString_Assign', 'tiisi', rsas, 'abcdef', 6)
for rsav in ((0, 'XY'), (3, '12345678901234567890'), (6, '!'), (2, '')):
	check('MsvcString_Insert' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_Insert', 'tiiisi', rsas, rsav[0], rsav[1],
		len(rsav[1]))[0] == rsas, rsastr(rsas)))
for rsav in ((0, 2), (5, 3), (10, 100), (3, 0)):
	check('MsvcString_Erase' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_Erase', 'tiiii', rsas, rsav[0], rsav[1])[0] == rsas,
		rsastr(rsas)))
rsas2 = rsanew()
SDTest.Call('MsvcString_Assign', 'tiisi', rsas2, 'the source string here', 22)
for rsav in ((0, 3), (4, 6), (10, 100), (22, 5)):
	check('MsvcString_AssignSub' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_AssignSub', 'tiiiii', rsas, rsas2, rsav[0],
		rsav[1])[0] == rsas, rsastr(rsas)))
for rsav in ((0, 0, 4), (3, 2, 50), (5, 0, 0)):
	check('MsvcString_InsertSub' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_InsertSub', 'tiiiiii', rsas, rsav[0], rsas2,
		rsav[1], rsav[2])[0] == rsas, rsastr(rsas)))
# a part of itself
SDTest.Call('MsvcString_Assign', 'tiisi', rsas, 'selfcopy', 8)
check('MsvcString_AssignSub self', lambda: (SDTest.Call('MsvcString_AssignSub', 'tiiiii', rsas, rsas, 2, 4)[0] == rsas, rsastr(rsas)))
SDTest.Call('MsvcString_Assign', 'tiisi', rsas, 'abcdefgh', 8)
for rsav in ((2, 2, 3), (2, 4, 2), (5, 1, 2)):
	SDTest.Call('MsvcString_Assign', 'tiisi', rsas, 'abcdefgh', 8)
	check('MsvcString_InsertSub self' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_InsertSub', 'tiiiiii', rsas, rsav[0], rsas,
		rsav[1], rsav[2])[0] == rsas, rsastr(rsas)))
# operator+ and the move
rsas3 = rsanew()
for rsav in (('ACHIEVEMENT_', '12'), ('', 'long enough to be on the heap'), ('x', '')):
	SDTest.Call('MsvcString_Assign', 'tiisi', rsas2, rsav[1], len(rsav[1]))
	check('MsvcString_PrependMove' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_PrependMove', 'iisi', rsas3, rsav[0], rsas2)[0] ==
		rsas3, rsastr(rsas3), rsastr(rsas2)))
	SDTest.Call('MsvcString_Tidy', 'tvi', rsas3)
check('MsvcString_ctorMove', lambda: (SDTest.Call('MsvcString_Assign', 'tiisi', rsas2, 'moved string text here', 22) and 0,
	SDTest.Call('MsvcString_ctorMove', 'tvii', rsas3, rsas2), rsastr(rsas3), rsastr(rsas2)))
SDTest.Call('MsvcString_Tidy', 'tvi', rsas3)
for rsav in (0, 7, -7, 2147483647, -2147483647 - 1, 1000000):
	check('MsvcString_ToString' + fmt((rsav,)), lambda rsav=rsav: (SDTest.Call('MsvcString_ToString', 'iii', rsas3, rsav)[0] == rsas3,
		rsastr(rsas3)))
	SDTest.Call('MsvcString_Tidy', 'tvi', rsas3)
for rsav in ((0, 0), (12345, 0), (-1, 0), (-1, -1), (0x2a, 0x100000), (999999999, 1)):
	check('MsvcString_ToStringU64' + fmt(rsav), lambda rsav=rsav: (SDTest.Call('MsvcString_ToStringU64', 'iiii', rsas3, rsav[0],
		rsav[1])[0] == rsas3, rsastr(rsas3)))
	SDTest.Call('MsvcString_Tidy', 'tvi', rsas3)
check('MsvcString_Tidy', lambda: (SDTest.Call('MsvcString_Tidy', 'tvi', rsas), rsastr(rsas)))
rsadel(rsas)
rsadel(rsas2)
rsadel(rsas3)
# the allocator: a small and a big block back
for rsav in (0, 16, 0x1000, 0x2345):
	rsap = SDTest.Call('MsvcAlloc_Allocate', 'tiii', 0, rsav)[0]
	check('MsvcAlloc_Allocate' + fmt((rsav,)), lambda rsap=rsap, rsav=rsav: (rsap != 0, rsav >= 0x1000 and (rsap & 0x1f)))
	if rsap:
		SDTest.Call('MsvcAlloc_Deallocate', 'tviii', 0, rsap, rsav)
for rsav in (0, 4, 0x400, 0x1000):
	rsap = SDTest.Call('PtrVector_Allocate', 'tiii', 0, rsav)[0]
	check('PtrVector_Allocate' + fmt((rsav,)), lambda rsap=rsap, rsav=rsav: (rsap != 0, rsav >= 0x400 and (rsap & 0x1f)))
	if rsap:
		SDTest.Call('PtrVector_Deallocate', 'tviii', 0, rsap, rsav)
# the listener vector and the GOG state (not signed in in the tests)
rsavec = rsaz(0x10)
check('GogListeners_ctor', lambda: (SDTest.Call('GogListeners_ctor', 'tii', rsavec)[0] == rsavec, rsaints(rsavec, 3)))
rsafree(rsavec)
check('Gog_GetID', lambda: (SDTest.Peek('g_GogReady', 'b'), SDTest.Call('Gog_GetID', 'i')[0] == 0))
rsavec = rsaz(8)
rsawi(rsavec, 0x400000 + sym('galaxy_api_SelfRegisteringListener_IAuthListener_vftable'))
check('IGalaxyListener_dtor', lambda: (SDTest.Call('IGalaxyListener_dtor', 'tvi', rsavec), rsai(rsavec) - 0x400000))
check('IGalaxyListener_Delete', lambda: (SDTest.Call('IGalaxyListener_Delete', 'tiii', rsavec, 0)[0] == rsavec, rsai(rsavec) - 0x400000))
rsafree(rsavec)
# the moves of unique_ptrs
rsavec = rsaz(0x20)
for rsai2 in range(4):
	rsawi(rsavec + 4 * rsai2, 0x100 + rsai2)
check('PtrVector_UninitMove', lambda: (SDTest.Call('PtrVector_UninitMove', 'iiii', rsavec, rsavec + 12, rsavec + 16)[0] - rsavec,
	rsaints(rsavec, 8)))
rsafree(rsavec)
