# game Logic/DominationEnemies.c ... GenCharLogic.c (execfile'd by sdlevel.py, Python 1.5.2). The mode objects of
# the outdoor missions (domination sides, Enemies, ExtPolice, Friends, the gatekeeper and generic character groups)
# made on calloc memory with empty fake lists, updated below and above their think time (the thinks end at the
# empty list); the zone table of the domination sides with the player as zone; the variables and the small slots
# of fake functionary, gear and GenChar logics with the player as parent (the fields of the player they write
# saved and restored); a gatekeeper logic made on the player and deleted.
# Not called: everything that sets an AI state, plays an action on the player or needs a ship of a list (the
# think steps on real ships, the hits and deaths of the natives, MakeLogic of the ships), the constructors that
# start the AI (functionary, gear, GenChar), the Init functions (they add lists to the world).
def lg2i(a):
	return SDTest.Peek(a, '@i')
def lg2b(a):
	return SDTest.Peek(a, '@b')
def lg2poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def lg2pokeb(a, v):
	lg2poke(a, (lg2i(a) & ~0xff) | v)
def lg2fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def lg2set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def lg2z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def lg2free(p):
	SDTest.Call('Mem_Free', 'ii', p)
def lg2new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def lg2str(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def lg2bytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def lg2ints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def lg2cset(addr, s):
	SDTest.Call('cString_Set', 'tviz', addr, s)
def lg2srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def lg2cstr(s):
	a = lg2z(len(s) + 4)
	SDTest.Call('crt_memcpy', 'iisi', a, s, len(s) + 1)
	return a
# an empty entity list (the ring of 0x30 on itself, no count)
def lg2list():
	l = lg2z(0x44)
	lg2poke(l + 0x30, l + 0x30)
	lg2poke(l + 0x34, l + 0x30)
	return l
# the target node of a logic (0x08) on itself, data the logic
def lg2node(l, o):
	lg2poke(l + o, l + o)
	lg2poke(l + o + 4, l + o)
	lg2poke(l + o + 0xc, l)
def lg2vt(a):
	return lg2i(a) - 0x400000

lg2world = SDTest.Peek('g_pWorld', 'i')
lg2player = SDTest.Call('cWorld_GetUsrEntity', 'tiii', lg2world, 0)[0]
lg2alarm = lg2b(lg2world + 0x1c70)
lg2outdoor = SDTest.Peek('g_IsMapOutDoor', 'i')
lg2actset = SDTest.Peek('g_CurActionSet', 'i')
check('LG2 start', lambda: (lg2player != 0, lg2alarm, lg2outdoor, lg2actset))

# ---- the small slots ----
for lg2r in (sym('Logic_NopDtor'), sym('Logic_NopDtor@146e50'), sym('Logic_NopDtor@147810'), sym('Logic_NopDtor@149c40')):
	check('LG2 Logic_NopDtor %x' % lg2r, lambda r=lg2r: SDTest.Call(r, 'tvi', 0)[0])
for lg2r in (sym('Logic_NativeNop'), sym('Logic_NativeNop@147b60')):
	check('LG2 Logic_NativeNop %x' % lg2r, lambda r=lg2r: SDTest.Call(r, 'vi', 0)[0])
for lg2r in (sym('Logic_True1'), sym('Logic_True1@14c3f0'), sym('Logic_False1@149e40'), sym('Logic_False1@14a4a0')):
	check('LG2 byte slot1 %x' % lg2r, lambda r=lg2r: SDTest.Call(r, 'tbii', 0, 5)[0])
check('LG2 Logic_True', lambda: SDTest.Call('Logic_True@1481d0', 'tbi', 0)[0])
check('LG2 Logic_NoTarget', lambda: SDTest.Call('Logic_NoTarget@1482e0', 'tii', 0)[0])
check('LG2 GetTeam', lambda: (SDTest.Call('cDominationEnemiesLogic_GetTeam', 'tii', 0)[0], SDTest.Call('cDominationFriendsLogic_GetTeam', 'tii', 0)[0]))

# ---- domination ----
lg2savedzones = lg2ints(symva('g_DominNumZones'), (0, 4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20))
lg2savedleader = SDTest.Peek('g_DominLeaderName', 'p')
def lg2zones():
	return (lg2i(symva('g_DominNumZones')), tuple(map(lambda i: lg2i(symva('g_DominZones') + 8 * i) == lg2player, range(4))),
		lg2ints(symva('g_DominZones') + 0x4, (0, 8, 0x10, 0x18)))
for lg2v in ('', 'SDLg2None', 'Player0'):
	lg2cset(symva('g_DominLeaderName'), lg2v)
	check('LG2 cDominationEnemiesLogic_GetLeader(%s)' % lg2v, lambda: SDTest.Call('cDominationEnemiesLogic_GetLeader', 'tii', 0)[0])
lg2cset(symva('g_DominLeaderName'), lg2savedleader)
lg2l = lg2list()
lg2df = lg2new(0x14)
lg2poke(symva('g_DominNumZones'), 3)
def lg2dside(d, o):
	s = lg2i(d + 0x10)
	return (lg2i(s) == d, lg2i(s + 4) == lg2l, lg2fl(s + o, 1))
check('LG2 cDominationFriendsLogic_ctor', lambda: (SDTest.Call('cDominationFriendsLogic_ctor', 'tii', lg2df)[0] == lg2df, lg2vt(lg2df),
	lg2i(lg2df + 4), lg2ints(lg2i(lg2df + 8), (0, 4, 8, 0xc)), lg2bytes(lg2df, (0xc, 0xd)), lg2i(lg2i(lg2df + 0x10)) ==
	lg2df, lg2ints(lg2i(lg2df + 0x10), (4, 8)), lg2i(symva('g_DominNumZones'))))
lg2poke(lg2i(lg2df + 0x10) + 4, lg2l)
check('LG2 cDominationFriendsLogic_GetList', lambda: SDTest.Call('cDominationFriendsLogic_GetList', 'tii', lg2df)[0] == lg2l)
for lg2v in (0.25, 0.25, 0.125, 0.0625):
	check('LG2 cDominationFriendsLogic_Update %s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cDominationFriendsLogic_Update', 'tvif', lg2df,
		v)[0], lg2dside(lg2df, 8)))
check('LG2 cDominationFriendsSide_Think', lambda: SDTest.Call('cDominationFriendsSide_Think', 'tvi', lg2i(lg2df + 0x10))[0])
check('LG2 FindShipOfZone empty', lambda: SDTest.Call('cDominationFriendsLogic_FindShipOfZone', 'tiii', lg2df, lg2player)[0])
# one node with the player in the list, its zone the player in the table
lg2n = lg2z(0x10)
lg2poke(lg2n, lg2l + 0x30)
lg2poke(lg2n + 4, lg2l + 0x30)
lg2poke(lg2n + 0xc, lg2player)
lg2poke(lg2l + 0x30, lg2n)
lg2poke(lg2l + 0x34, lg2n)
check('LG2 FindShipOfZone none', lambda: SDTest.Call('cDominationFriendsLogic_FindShipOfZone', 'tiii', lg2df, lg2player)[0])
check('LG2 NameTable_Add friends', lambda: SDTest.Call('NameTable_Add', 'tbispi', lg2df + 4, 'Player0', lg2player, 0)[0])
check('LG2 FindShipOfZone', lambda: (SDTest.Call('cDominationFriendsLogic_FindShipOfZone', 'tiii', lg2df, lg2player)[0], SDTest.Call('cDominationFriendsLogic_FindShipOfZone',
	'tiii', lg2df, lg2df)[0]))
lg2k = 0
while lg2k < 4:
	lg2poke(symva('g_DominZones') + 8 * lg2k, 0)
	lg2poke(symva('g_DominZones') + 0x4 + 8 * lg2k, lg2k + 1)
	lg2k = lg2k + 1
lg2poke(symva('g_DominZones') + 8, lg2player)
lg2poke(symva('g_DominZones') + 0x4 + 8, 3)
for lg2v in ('Player0', 'Player0', 'SDLg2None'):
	check('LG2 cDominationFriendsLogic_ReleaseZone(%s)' % lg2v, lambda v=lg2v: (SDTest.Call('cDominationFriendsLogic_ReleaseZone', 'tiis', lg2df,
		v)[0] == lg2player, lg2zones()))
check('LG2 cDominationFriendsLogic_Delete', lambda: SDTest.Call('cDominationFriendsLogic_Delete', 'tiii', lg2df, 1)[0] == lg2df)
lg2poke(lg2l + 0x30, lg2l + 0x30)
lg2poke(lg2l + 0x34, lg2l + 0x30)
lg2free(lg2n)
lg2df = lg2z(0x14)
check('LG2 cDominationFriendsLogic dtor', lambda: (SDTest.Call('cDominationFriendsLogic_ctor', 'tii', lg2df)[0] == lg2df,
	SDTest.Call('cDominationFriendsLogic_dtor', 'tvi', lg2df)[0], lg2vt(lg2df), lg2ints(lg2df, (4, 8)), lg2i(symva('g_DominNumZones'))))
lg2free(lg2df)
# the enemy side made by its constructor of part 1
lg2de = lg2new(0x14)
check('LG2 cDominationEnemiesLogic', lambda: (SDTest.Call('cDominationEnemiesLogic_ctor', 'tii', lg2de)[0] == lg2de, lg2vt(lg2de)))
lg2poke(lg2i(lg2de + 0x10) + 4, lg2l)
check('LG2 cDominationEnemiesLogic_GetList', lambda: SDTest.Call('cDominationEnemiesLogic_GetList', 'tii', lg2de)[0] == lg2l)
for lg2v in (0.25, 0.25, 0.125, 0.0625):
	check('LG2 cDominationEnemiesLogic_Update %s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cDominationEnemiesLogic_Update', 'tvif', lg2de,
		v)[0], lg2dside(lg2de, 0xc), lg2i(lg2i(lg2de + 0x10) + 8)))
check('LG2 cDominationSide_Think', lambda: SDTest.Call('cDominationSide_Think', 'tvi', lg2i(lg2de + 0x10))[0])
check('LG2 NameTable_Add enemies', lambda: SDTest.Call('NameTable_Add', 'tbispi', lg2de + 4, 'Player0', lg2player, 0)[0])
lg2poke(symva('g_DominZones') + 0x4 + 8, 4)
for lg2v in ('Player0', 'Player0', ''):
	check('LG2 cDominationEnemiesLogic_ReleaseZone(%s)' % lg2v, lambda v=lg2v: (SDTest.Call('cDominationEnemiesLogic_ReleaseZone', 'tiis', lg2de,
		v)[0] == lg2player, lg2zones()))
check('LG2 cDominationEnemiesLogic_Delete', lambda: SDTest.Call('cDominationEnemiesLogic_Delete', 'tiii', lg2de, 1)[0] == lg2de)
lg2k = 0
while lg2k < 9:
	lg2poke(symva('g_DominNumZones') + 4 * lg2k, lg2savedzones[lg2k])
	lg2k = lg2k + 1
check('LG2 domination restored', lambda: (lg2ints(symva('g_DominNumZones'), (0, 4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20)) ==
	lg2savedzones, SDTest.Peek('g_DominLeaderName', 'p') == lg2savedleader))

# ---- Enemies ----
lg2en = lg2z(0x10)
lg2k = 0
while lg2k < 4:
	lg2poke(lg2en + 4 * lg2k, -1)
	lg2k = lg2k + 1
def lg2enf(e):
	return (lg2i(e) == lg2player, lg2bytes(e, (4, 5, 6)), lg2fl(e + 8, 1), lg2i(e + 0xc) == lg2l)
check('LG2 cEnemies_ctor', lambda: (SDTest.Call('cEnemies_ctor', 'tii', lg2en)[0] == lg2en, lg2ints(lg2en, (0, 4, 8, 0xc))))
lg2poke(lg2en + 0xc, lg2l)
for lg2v in (0.25, 0.25, 0.125, 0.5):
	check('LG2 cEnemies_Update %s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cEnemies_Update', 'tvif', lg2en, v)[0],
		lg2enf(lg2en)))
check('LG2 cEnemies_Think', lambda: (SDTest.Call('cEnemies_Think', 'tvi', lg2en)[0], lg2enf(lg2en)))
lg2pokeb(lg2en + 5, 1)
check('LG2 cEnemies_Think attacking', lambda: (SDTest.Call('cEnemies_Think', 'tvi', lg2en)[0], lg2enf(lg2en)))
check('LG2 cEnemies_Attack', lambda: (SDTest.Call('cEnemies_Attack', 'tvii', lg2en, 0)[0], SDTest.Call('cEnemies_Attack', 'tvii',
	lg2en, lg2player)[0], lg2enf(lg2en)))
check('LG2 cEnemies_Relax', lambda: SDTest.Call('cEnemies_Relax', 'tvi', lg2en)[0])
lg2free(lg2en)
lg2savedign = lg2b(lg2world + 0x2174)
for lg2v in (('IgnoreFriends', 0, 1), ('IgnoreFriends', 7, 0), ('IgnoreFriends', 0, 0), ('IgnoreFriends', 0, 1),
		('IgnoreFriends', -3, 1), ('IgnoreFriends', 0, 0), ('Bogus', 1, 1), ('', 1, 0)):
	check('LG2 cEnemiesLocalLogic_VarI%s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cEnemiesLocalLogic_VarI', 'tbispi', 0, v[0], v[1],
		1 - v[2]), lg2b(lg2world + 0x2174)))
lg2pokeb(lg2world + 0x2174, lg2savedign)
lg2e = lg2z(8)
check('LG2 cEnemiesLocalLogic_dtor', lambda: (SDTest.Call('cEnemiesLocalLogic_dtor', 'tvi', lg2e)[0], lg2vt(lg2e)))
lg2free(lg2e)
lg2e = lg2new(8)
check('LG2 cEnemiesLocalLogic_Delete', lambda: SDTest.Call('cEnemiesLocalLogic_Delete', 'tiii', lg2e, 1)[0] == lg2e)

# ---- ExtPolice ----
lg2outcop = SDTest.Call('cWorld_GetList', 'tiis', lg2world, 'OutCop')[0]
lg2copsfree = lg2outcop == 0 or lg2i(lg2i(lg2outcop + 0x30) + 0xc) == 0
lg2wavefree = lg2i(symva('g_PoliceWaveEliminated') + 8) == 0
check('LG2 police', lambda: (lg2copsfree, lg2wavefree))
lg2savedchase = lg2i(symva('g_ExtPoliceChasing'))
lg2ep = lg2z(0x18)
def lg2epf(e):
	return (lg2fl(e, 1), lg2b(e + 4), lg2i(e + 8) == lg2player, lg2fl(e + 0xc, 1), lg2i(e + 0x10), lg2i(e + 0x14) ==
		lg2l, lg2i(symva('g_ExtPoliceChasing')))
if lg2wavefree:
	lg2poke(symva('g_ExtPoliceChasing'), 5)
	check('LG2 cExtPolice_ctor', lambda: (SDTest.Call('cExtPolice_ctor', 'tii', lg2ep)[0] == lg2ep, lg2epf(lg2ep)))
	lg2poke(symva('g_ExtPoliceChasing'), lg2savedchase)
lg2poke(lg2ep + 0x14, lg2l)
if lg2alarm == 0:
	for lg2v in (0.25, 0.25, 0.125, 0.5):
		check('LG2 cExtPolice_Update %s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cExtPolice_Update', 'tvif', lg2ep, v)[0],
			lg2epf(lg2ep)))
	lg2set(lg2ep, (0.25,))
	check('LG2 cExtPolice_Think', lambda: (SDTest.Call('cExtPolice_Think', 'tvi', lg2ep)[0], lg2epf(lg2ep),
		SDTest.Call('cExtPolice_Think', 'tvi', lg2ep)[0], lg2epf(lg2ep)))
lg2free(lg2ep)
if lg2copsfree:
	check('LG2 ExtPolice_IsChased', lambda: (SDTest.Call('ExtPolice_IsChased', 'bi', lg2player)[0], SDTest.Call('ExtPolice_IsChased', 'bi',
		0)[0]))
	check('LG2 ExtPolice_CallCop', lambda: SDTest.Call('ExtPolice_CallCop', 'ii', lg2player)[0])
for lg2v in (('', '', 0), ('SDLg2None', 'Player0', 0), ('Player0', 'Player0', 1)):
	check('LG2 ExtPolice_CopDamage%s' % fmt(lg2v), lambda v=lg2v: SDTest.Call('ExtPolice_CopDamage', 'vssi', v[0], v[1], v[2])[0])

# ---- Friends ----
lg2fr = lg2z(8)
lg2poke(lg2fr, -1)
lg2poke(lg2fr + 4, -1)
check('LG2 cFriends_ctor', lambda: (SDTest.Call('cFriends_ctor', 'tii', lg2fr)[0] == lg2fr, lg2ints(lg2fr, (0, 4))))
lg2poke(lg2fr + 4, lg2l)
for lg2v in (1.0, 0.25, 0.375, 1.75):
	check('LG2 cFriends_Update %s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cFriends_Update', 'tvif', lg2fr, v)[0],
		lg2fl(lg2fr, 1)))
check('LG2 cFriends_Think', lambda: SDTest.Call('cFriends_Think', 'tvi', lg2fr)[0])
check('LG2 cFriends_Attack', lambda: (SDTest.Call('cFriends_Attack', 'tvii', lg2fr, 0)[0], SDTest.Call('cFriends_Attack', 'tvii', lg2fr,
	lg2player)[0]))
lg2free(lg2fr)
check('LG2 Friends_IsEnemyTarget', lambda: (SDTest.Call('Friends_IsEnemyTarget', 'tbiii', 0, lg2player, lg2player)[0],
	SDTest.Call('Friends_IsEnemyTarget', 'tbiii', 0, lg2player, 0)[0]))

# ---- the fields of the player the logics write ----
def lg2psave():
	return (lg2i(lg2player + 0x380), lg2i(lg2player + 0x388), lg2i(lg2player + 0x38c), lg2i(lg2player + 0x390),
		lg2i(lg2player + 0x394))
def lg2prestore(v):
	lg2poke(lg2player + 0x380, v[0])
	lg2poke(lg2player + 0x388, v[1])
	lg2poke(lg2player + 0x38c, v[2])
	lg2poke(lg2player + 0x390, v[3])
	lg2poke(lg2player + 0x394, v[4])
def lg2pf():
	return (lg2b(lg2player + 0x381), lg2i(lg2player + 0x388), lg2i(lg2player + 0x38c), lg2fl(lg2player + 0x390, 2))
lg2psaved = lg2psave()
# a fake entity for the timer of the special action (0x110)
lg2fe = lg2z(0x200)
def lg2timer():
	return lg2fl(lg2fe + 0x110, 10)

# ---- functionary on a fake logic ----
lg2deskname = SDTest.Peek('g_DesktopsListName', 'p')
lg2desks = 0
if lg2deskname:
	lg2desks = SDTest.Call('cWorld_GetList', 'tiis', lg2world, lg2deskname)[0]
lg2saveddesklist = lg2i(symva('g_DesktopsList'))
check('LG2 desktops', lambda: (lg2deskname, lg2desks != 0, lg2saveddesklist != 0))
def lg2mkfn(n):
	f = lg2z(n)
	lg2poke(f, 0x400000 + sym('cFunctionaryLocalLogic_vftable'))
	lg2poke(f + 4, lg2player)
	lg2node(f, 8)
	lg2node(f, 0x68)
	lg2pokeb(f + 0x28, 1)
	lg2pokeb(f + 0xad, 1)
	return f
def lg2fnf(f):
	return (lg2i(f + 0x18), lg2fl(f + 0x7c, 11), lg2bytes(f, (0x5a, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2)), lg2i(f + 0xb4))
lg2fn = lg2mkfn(0xbc)
for lg2v in (('ProbTalkWhenWorking', 0.5, 1), ('ProbTalkWhenWorking', 0.0, 0), ('ProbTalkWhenPlaying', 1.0, 1),
		('ProbTalkWhenPlaying', 0.0, 0), ('ProbTalkWhenWalking', 0.25, 1), ('ProbTalkWhenWalking', 0.0, 0),
		('EscapeRadiusSqr', 2.5, 1), ('EscapeRadiusSqr', 0.0, 0), ('GoToWorkSpeed', 0.75, 1), ('GoToWorkSpeed', 0.0, 0),
		('TimeToSitDown', 3.0, 1), ('TimeToSitDown', 0.0, 0), ('TimeToStopWorking', 1e30, 1),
		('TimeToStopWorking', 0.0, 0), ('ActionRadius', 0.0, 0), ('Bogus', 1.0, 1)):
	check('LG2 cFunctionaryLocalLogic_VarF%s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cFunctionaryLocalLogic_VarF', 'tbisDi', lg2fn,
		v[0], v[1], v[2]), lg2fnf(lg2fn)))
for lg2nm in ('ForceSpecialBehaviour', 'ForceCancelSpecialBehaviour', 'EscapeWithinRadius', 'FastEscape',
		'IgnoreAlarm', 'IgnoreBankerAttack', 'StopInRelax'):
	for lg2v in ((1, 1), (5, 0), (2, 1), (5, 0), (1, 1), (0, 1)):
		check('LG2 cFunctionaryLocalLogic_VarI%s' % fmt((lg2nm, lg2v)), lambda n=lg2nm, v=lg2v: (SDTest.Call('cFunctionaryLocalLogic_VarI',
			'tbispi', lg2fn, n, v[0], v[1]), lg2fnf(lg2fn)))
for lg2v in (('WorkArea', 0), ('KeepWorking', 0), ('Bogus', 0)):
	check('LG2 cFunctionaryLocalLogic_VarI%s' % fmt(lg2v), lambda v=lg2v: SDTest.Call('cFunctionaryLocalLogic_VarI', 'tbispi', lg2fn, v[0],
		7, v[1]))
check('LG2 cFunctionaryLocalLogic_VarI(KeepWorking, 0) set', lambda: (SDTest.Call('cFunctionaryLocalLogic_VarI', 'tbispi', lg2fn,
	'KeepWorking', 0, 1), lg2fnf(lg2fn)))
if lg2deskname and lg2desks == 0:
	check('LG2 cFunctionaryLocalLogic_VarI(KeepWorking, 1) set', lambda: (SDTest.Call('cFunctionaryLocalLogic_VarI', 'tbispi', lg2fn,
		'KeepWorking', 1, 1), lg2fnf(lg2fn), lg2ints(lg2fn, (0x70, 0x78))))
	check('LG2 cFunctionaryLocalLogic_AssignDesktop', lambda: (SDTest.Call('cFunctionaryLocalLogic_AssignDesktop', 'tvif', lg2fn, 0.0)[0],
		lg2ints(lg2fn, (0x70, 0x78))))
lg2pokeb(lg2fn + 0xaa, 0)
lg2pokeb(lg2fn + 0xad, 1)
check('LG2 cFunctionaryLocalLogic_VarS', lambda: (SDTest.Call('cFunctionaryLocalLogic_VarS', 'tbispi', lg2fn, 'ForcedDesktopAssigned', 0,
	0), SDTest.Call('cFunctionaryLocalLogic_VarS', 'tbispi', lg2fn, 'Bogus', 0, 0)))
lg2srand(11)
for lg2v in ((8, 0), (8, 1), (0, 1), (6, 0), (7, 1), (3, 1), (2, 0)):
	lg2poke(lg2fn + 0x18, lg2v[0])
	lg2pokeb(lg2fn + 0xa9, lg2v[1])
	check('LG2 cFunctionaryLocalLogic_CanTalk%s' % fmt(lg2v), lambda: map(lambda i: SDTest.Call('cFunctionaryLocalLogic_CanTalk', 'tbi',
		lg2fn)[0], range(6)))
lg2poke(lg2fn + 0x18, 0)
lg2pokeb(lg2fn + 0xa9, 0)
check('LG2 cFunctionaryLocalLogic_FastEscape', lambda: SDTest.Call('cFunctionaryLocalLogic_FastEscape', 'tbi', lg2fn)[0])
check('LG2 cFunctionaryLocalLogic_GetDesktop', lambda: (SDTest.Call('cFunctionaryLocalLogic_GetDesktop', 'tii', lg2fn)[0], lg2poke(lg2fn + 0x78,
	lg2player), SDTest.Call('cFunctionaryLocalLogic_GetDesktop', 'tii', lg2fn)[0] == lg2player, lg2poke(lg2fn + 0x78, 0), lg2poke(lg2fn + 0x70,
	lg2player), SDTest.Call('cFunctionaryLocalLogic_GetDesktop', 'tii', lg2fn)[0] == lg2player, lg2poke(lg2fn + 0x70, 0)))
lg2poke(lg2fn + 0x78, lg2player)
lg2out = lg2z(12)
check('LG2 cFunctionaryLocalLogic_GetWorkAng', lambda: (SDTest.Call('cFunctionaryLocalLogic_GetWorkAng', 'tiii', lg2fn, lg2out)[0] == lg2out,
	lg2fl(lg2out, 3)))
lg2free(lg2out)
lg2srand(12)
check('LG2 cFunctionaryLocalLogic_StopWork forced', lambda: (SDTest.Call('cFunctionaryLocalLogic_StopWork', 'tvii', lg2fn, 0)[0],
	lg2fnf(lg2fn)))
lg2poke(lg2fn + 0x78, 0)
check('LG2 cFunctionaryLocalLogic_StopWork', lambda: (SDTest.Call('cFunctionaryLocalLogic_StopWork', 'tvii', lg2fn, 0)[0], lg2fnf(lg2fn)))
for lg2v in ((0, 0.0), (8, 0.0), (8, 1.5)):
	lg2poke(lg2fn + 0x18, lg2v[0])
	lg2set(lg2fn + 0x88, (lg2v[1],))
	check('LG2 cFunctionaryLocalLogic_Vf33%s' % fmt(lg2v), lambda: (SDTest.Call('cFunctionaryLocalLogic_Vf33', 'tvii', lg2fn, 0)[0],
		lg2fnf(lg2fn)))
lg2poke(lg2fn + 0x18, 0)
check('LG2 cFunctionaryLocalLogic desktop', lambda: (SDTest.Call('cFunctionaryLocalLogic_SetKeepWorking', 'tvii', lg2fn, 0)[0], SDTest.Call('cFunctionaryLocalLogic_ReleaseDesktop',
	'tvi', lg2fn)[0], SDTest.Call('cFunctionaryLocalLogic_FreeDesktop', 'tvi', lg2fn)[0], lg2fnf(lg2fn)))
lg2poke(lg2fn + 0x78, lg2player)
check('LG2 cFunctionaryLocalLogic_AssignDesktop forced', lambda: (SDTest.Call('cFunctionaryLocalLogic_AssignDesktop', 'tvif', lg2fn, 0.0)[0],
	lg2ints(lg2fn, (0x70, 0x78)) == (0, lg2player)))
lg2poke(lg2fn + 0x78, 0)
lg2pokeb(lg2fn + 0xb1, 1)
check('LG2 cFunctionaryLocalLogic_OnRobbery', lambda: SDTest.Call('cFunctionaryLocalLogic_OnRobbery', 'tviif', lg2fn, 0, 1.0)[0])
for lg2v in ((0, 0), (4, 0), (2, 0), (10, 0), (0, 1)):
	lg2poke(lg2fn + 0x18, lg2v[0])
	lg2pokeb(lg2fn + 0x29, lg2v[1])
	check('LG2 cFunctionaryLocalLogic_UpdateView%s' % fmt(lg2v), lambda: (SDTest.Call('cFunctionaryLocalLogic_UpdateView', 'tvi', lg2fn)[0],
		lg2pf()))
lg2prestore(lg2psaved)
lg2poke(lg2fn + 0x18, 0)
lg2pokeb(lg2fn + 0x29, 0)
lg2poke(lg2fn + 4, lg2fe)
check('LG2 cFunctionaryLocalLogic_Vf02', lambda: (SDTest.Call('cFunctionaryLocalLogic_Vf02', 'tbi', lg2fn)[0], lg2timer()))
lg2poke(lg2fn + 4, lg2player)
check('LG2 cFunctionaryLocalLogic_SlowEffect', lambda: (SDTest.Call('cFunctionaryLocalLogic_SlowEffect', 'tvii', lg2fn, lg2fe)[0], lg2timer()))
check('LG2 cFunctionaryLocalLogic_NormalSpeed', lambda: (SDTest.Call('cFunctionaryLocalLogic_NormalSpeed', 'tvii', lg2fn, lg2fe)[0], lg2timer()))
check('LG2 Functionary_WorkRelax(0)', lambda: SDTest.Call('Functionary_WorkRelax', 'vi', 0)[0])
check('LG2 cFunctionaryLocalLogic_dtor', lambda: (SDTest.Call('cFunctionaryLocalLogic_dtor', 'tvi', lg2fn)[0], lg2vt(lg2fn), lg2ints(lg2fn,
	(0x70, 0x10)), lg2i(symva('g_DesktopsList'))))
lg2free(lg2fn)
lg2poke(symva('g_DesktopsList'), lg2saveddesklist)
lg2fn = lg2new(0xb8)
lg2k = 0
while lg2k < 0x2e:
	lg2poke(lg2fn + 4 * lg2k, 0)
	lg2k = lg2k + 1
lg2poke(lg2fn, 0x400000 + sym('cFunctionaryLocalLogic_vftable'))
lg2poke(lg2fn + 4, lg2player)
lg2node(lg2fn, 8)
lg2node(lg2fn, 0x68)
check('LG2 cFunctionaryLocalLogic_Delete', lambda: (SDTest.Call('cFunctionaryLocalLogic_Delete', 'tiii', lg2fn, 1)[0] == lg2fn,
	lg2i(symva('g_DesktopsList'))))
lg2poke(symva('g_DesktopsList'), lg2saveddesklist)

# ---- the gatekeepers ----
lg2gk = lg2z(8)
lg2poke(lg2gk, -1)
lg2poke(lg2gk + 4, lg2l)
check('LG2 cGateKeepers_ctor', lambda: (SDTest.Call('cGateKeepers_ctor', 'tii', lg2gk)[0] == lg2gk, lg2fl(lg2gk, 1)))
for lg2v in (1.0, 0.25, 0.5, 2.0):
	check('LG2 cGateKeepers_Update %s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cGateKeepers_Update', 'tvif', lg2gk, v)[0],
		lg2fl(lg2gk, 1)))
check('LG2 cGateKeepers_Think', lambda: SDTest.Call('cGateKeepers_Think', 'tvi', lg2gk)[0])
lg2free(lg2gk)
lg2hide = SDTest.Call(lg2i(lg2i(lg2player) + 0xd0) - 0x400000, 'tiis', lg2player, 'Hide')[0]
check('LG2 player Hide', lambda: lg2hide != 0)
def lg2gkf(g):
	return (lg2vt(g), lg2i(g + 4) == lg2player, lg2i(g + 0x18), lg2bytes(g, (0x20, 0x28, 0x29, 0x2a)), lg2fl(g + 0x48,
		1))
lg2g = SDTest.Call('GateKeepers_MakeLogic', 'ii', lg2player)[0]
check('LG2 GateKeepers_MakeLogic', lambda: (lg2g != 0, lg2gkf(lg2g), lg2pf()))
check('LG2 cGateKeeperLocalLogic_Vf16', lambda: SDTest.Call('cGateKeeperLocalLogic_Vf16', 'tvii', lg2g, 0)[0])
check('LG2 cGateKeeperLocalLogic_SetState(0)', lambda: (SDTest.Call('cGateKeeperLocalLogic_SetState', 'tbiii', lg2g, 0, 0)[0], lg2gkf(lg2g),
	lg2pf()))
if lg2alarm == 0:
	check('LG2 cGateKeeperLocalLogic_RestartState', lambda: (SDTest.Call('cGateKeeperLocalLogic_RestartState', 'tvi', lg2g)[0], lg2gkf(lg2g)))
if lg2hide == 0:
	check('LG2 cGateKeeperLocalLogic_SetState(1)', lambda: (SDTest.Call('cGateKeeperLocalLogic_SetState', 'tbiii', lg2g, 1, 0)[0],
		lg2gkf(lg2g)))
	check('LG2 cGateKeeperLocalLogic hits', lambda: (SDTest.Call('cGateKeeperLocalLogic_OnHit', 'tvii', lg2g, 0)[0], SDTest.Call('cGateKeeperLocalLogic_Vf14',
		'tvii', lg2g, 0)[0], SDTest.Call('cGateKeeperLocalLogic_Vf11', 'tvii', lg2g, 0)[0], SDTest.Call('cGateKeeperLocalLogic_OnRobbery', 'tviif', lg2g, 0,
		1.0)[0], lg2gkf(lg2g)))
for lg2v in ((0, 0), (1, 0), (0, 1)):
	lg2poke(lg2g + 0x18, lg2v[0])
	lg2pokeb(lg2g + 0x29, lg2v[1])
	check('LG2 cGateKeeperLocalLogic_UpdateView%s' % fmt(lg2v), lambda: (SDTest.Call('cGateKeeperLocalLogic_UpdateView', 'tvi', lg2g)[0],
		lg2pf()))
lg2poke(lg2g + 0x18, 0)
lg2pokeb(lg2g + 0x29, 0)
check('LG2 cGateKeeperLocalLogic_Delete', lambda: SDTest.Call('cGateKeeperLocalLogic_Delete', 'tiii', lg2g, 1)[0] == lg2g)
lg2prestore(lg2psaved)
lg2g = lg2z(0x4c)
lg2poke(lg2g, 0x400000 + sym('cGateKeeperLocalLogic_vftable'))
lg2poke(lg2g + 4, lg2player)
lg2node(lg2g, 8)
check('LG2 cGateKeeperLocalLogic_dtor', lambda: (SDTest.Call('cGateKeeperLocalLogic_dtor', 'tvi', lg2g)[0], lg2vt(lg2g)))
lg2free(lg2g)

# ---- gear on a fake logic ----
def lg2mkge(g):
	lg2poke(g, 0x400000 + sym('cGearLocalLogic_vftable'))
	lg2poke(g + 4, lg2player)
	lg2node(g, 8)
	lg2pokeb(g + 0x28, 1)
lg2ge = lg2z(0x5c)
lg2mkge(lg2ge)
def lg2gef(g):
	return (lg2i(g + 0x18), lg2fl(g + 0x48, 2), lg2b(g + 0x50), lg2i(g + 0x54), lg2str(lg2i(g + 0x58)))
for lg2v in (('DefaultState', 4, 1), ('DefaultState', 0, 0), ('DefaultState', 3, 1), ('DefaultState', 0, 1),
		('State', 0, 0), ('Bogus', 1, 1)):
	check('LG2 cGearLocalLogic_VarI%s' % fmt(lg2v), lambda v=lg2v: (SDTest.Call('cGearLocalLogic_VarI', 'tbispi', lg2ge, v[0], v[1],
		v[2]), lg2gef(lg2ge)))
lg2tname = lg2cstr('SDLg2Target')
check('LG2 cGearLocalLogic_VarS set', lambda: (SDTest.Call('cGearLocalLogic_VarS', 'tbispi', lg2ge, 'DefaultTarget', lg2tname,
	1)[0], lg2gef(lg2ge)))
check('LG2 cGearLocalLogic_VarS get', lambda: lg2str(SDTest.Call('cGearLocalLogic_VarS', 'tbispi', lg2ge, 'DefaultTarget', 0, 0)[1]))
check('LG2 cGearLocalLogic_VarS Target', lambda: lg2str(SDTest.Call('cGearLocalLogic_VarS', 'tbispi', lg2ge, 'Target', 0, 0)[1]))
lg2free(lg2tname)
check('LG2 cGearLocalLogic defaults', lambda: (SDTest.Call('cGearLocalLogic_SetDefaultState', 'tvii', lg2ge, 4)[0], SDTest.Call('cGearLocalLogic_SetDefaultTarget',
	'tvis', lg2ge, 'SDLg2Other')[0], SDTest.Call('cGearLocalLogic_GetDefaultState', 'tii', lg2ge)[0], lg2str(SDTest.Call('cGearLocalLogic_GetDefaultTarget', 'tii',
	lg2ge)[0]), lg2gef(lg2ge)))
lg2pokeb(lg2ge + 0x29, 1)
check('LG2 cGearLocalLogic busy hits', lambda: (SDTest.Call('cGearLocalLogic_Vf15', 'tbii', lg2ge, lg2player)[0],
	SDTest.Call('cGearLocalLogic_Vf14', 'tvii', lg2ge, lg2player)[0], SDTest.Call('cGearLocalLogic_Vf14@14aea0', 'tvii', lg2ge, lg2player)[0],
	SDTest.Call('cGearLocalLogic_OnHit', 'tvii', lg2ge, lg2player)[0], lg2gef(lg2ge)))
lg2pokeb(lg2ge + 0x29, 0)
check('LG2 cGearLocalLogic_OnRobbery', lambda: (SDTest.Call('cGearLocalLogic_OnRobbery', 'tviif', lg2ge, lg2player, 0.0)[0],
	SDTest.Call('cGearLocalLogic_OnRobbery', 'tviif', lg2ge, lg2player, -1.0)[0], lg2gef(lg2ge)))
check('LG2 cGearLocalLogic_Relax', lambda: SDTest.Call('cGearLocalLogic_Relax', 'tvi', lg2ge)[0])
lg2poke(lg2ge + 0x10, lg2player)
for lg2v in (1, 2, 3, 0, 4):
	lg2poke(lg2ge + 0x18, lg2v)
	lg2pokeb(lg2ge + 0x50, 1)
	check('LG2 cGearLocalLogic_UpdateAnim %d' % lg2v, lambda: (SDTest.Call('cGearLocalLogic_UpdateAnim', 'tviif', lg2ge, lg2player,
		0.125)[0], lg2gef(lg2ge), lg2pf()))
lg2prestore(lg2psaved)
lg2poke(lg2ge + 0x10, 0)
check('LG2 cGearLocalLogic_dtor', lambda: (SDTest.Call('cGearLocalLogic_dtor', 'tvi', lg2ge)[0], lg2vt(lg2ge)))
lg2free(lg2ge)
lg2ge = lg2new(0x5c)
lg2k = 0
while lg2k < 0x17:
	lg2poke(lg2ge + 4 * lg2k, 0)
	lg2k = lg2k + 1
lg2mkge(lg2ge)
check('LG2 cGearLocalLogic_Delete', lambda: SDTest.Call('cGearLocalLogic_Delete', 'tiii', lg2ge, 1)[0] == lg2ge)

# ---- the GenChar group ----
lg2savedgg = lg2ints(symva('g_BishopAttackOnAlarm'), (0,)) + lg2ints(symva('g_BankDirectorAgressive'), (0,)) + lg2ints(symva('g_GenCharAttackActive'), (0,))
lg2gg = lg2z(0x1c)
lg2k = 0
while lg2k < 7:
	lg2poke(lg2gg + 4 * lg2k, -5)
	lg2k = lg2k + 1
def lg2ggf(g):
	return (lg2i(g), lg2i(g + 4), lg2fl(g + 8, 1), lg2i(g + 0x18) != 0)
check('LG2 cGenCharGroup_ctor', lambda: (SDTest.Call('cGenCharGroup_ctor', 'tii', lg2gg)[0] == lg2gg, lg2ggf(lg2gg),
	lg2i(symva('g_BishopAttackOnAlarm')) == lg2i(0x84cc04), lg2i(symva('g_BankDirectorAgressive')) == lg2i(0x84cb74), lg2i(symva('g_GenCharAttackActive')) == lg2i(0x84d1e0)))
lg2poke(symva('g_BishopAttackOnAlarm'), lg2savedgg[0])
lg2poke(symva('g_BankDirectorAgressive'), lg2savedgg[1])
lg2poke(symva('g_GenCharAttackActive'), lg2savedgg[2])
check('LG2 cGenCharGroup_TakeAttack', lambda: (map(lambda i: SDTest.Call('cGenCharGroup_TakeAttack', 'tbi', lg2gg)[0], range(4)),
	lg2ggf(lg2gg)))
check('LG2 cGenCharGroup level', lambda: (SDTest.Call('cGenCharGroup_GetLevel', 'tii', lg2gg)[0], SDTest.Call('cGenCharGroup_RaiseLevel', 'tvii', lg2gg,
	5)[0], SDTest.Call('cGenCharGroup_RaiseLevel', 'tvii', lg2gg, 2)[0], SDTest.Call('cGenCharGroup_GetLevel', 'tii', lg2gg)[0]))
check('LG2 cGenCharGroup_Update', lambda: (SDTest.Call('cGenCharGroup_Update', 'tvif', lg2gg, 0.0625)[0], lg2ggf(lg2gg),
	SDTest.Call('cGenCharGroup_Update', 'tvif', lg2gg, 0.0625)[0], lg2ggf(lg2gg)))
check('LG2 cGenCharGroup_dtor', lambda: SDTest.Call('cGenCharGroup_dtor', 'tvi', lg2gg)[0])
lg2free(lg2gg)
check('LG2 genchar vars restored', lambda: lg2ints(symva('g_BishopAttackOnAlarm'), (0,)) + lg2ints(symva('g_BankDirectorAgressive'), (0,)) + lg2ints(symva('g_GenCharAttackActive'), (0,))
	== lg2savedgg)

# ---- GenChar on a fake logic ----
lg2gc = lg2z(0x68)
lg2poke(lg2gc, 0x400000 + sym('cGenCharLocalLogic_vftable'))
lg2poke(lg2gc + 4, lg2player)
lg2node(lg2gc, 8)
lg2poke(lg2gc + 0x50, 1234)
lg2cset(lg2gc + 0x54, 'SDLg2S54')
check('LG2 cGenCharLocalLogic getters', lambda: (SDTest.Call('cGenCharLocalLogic_SetB58', 'tvii', lg2gc, 7)[0], lg2b(lg2gc + 0x58),
	SDTest.Call('cGenCharLocalLogic_GetI50', 'tii', lg2gc)[0], lg2str(SDTest.Call('cGenCharLocalLogic_GetS54', 'tii', lg2gc)[0])))
for lg2v in ((0, 0), (3, 0), (11, 0), (5, 0), (0, 1)):
	lg2poke(lg2gc + 0x18, lg2v[0])
	lg2pokeb(lg2gc + 0x29, lg2v[1])
	check('LG2 cGenCharLocalLogic_UpdateView%s' % fmt(lg2v), lambda: (SDTest.Call('cGenCharLocalLogic_UpdateView', 'tvi', lg2gc)[0],
		lg2pf()))
lg2prestore(lg2psaved)
check('LG2 cGenCharLocalLogic_CanDiscover', lambda: SDTest.Call('cGenCharLocalLogic_CanDiscover', 'tbii', lg2gc, lg2player)[0])
check('LG2 cGenCharLocalLogic_dtor', lambda: (SDTest.Call('cGenCharLocalLogic_dtor', 'tvi', lg2gc)[0], lg2vt(lg2gc), lg2i(lg2gc + 0x10)))
lg2free(lg2gc)
lg2gc = lg2new(0x68)
lg2k = 0
while lg2k < 0x1a:
	lg2poke(lg2gc + 4 * lg2k, 0)
	lg2k = lg2k + 1
lg2poke(lg2gc, 0x400000 + sym('cGenCharLocalLogic_vftable'))
lg2poke(lg2gc + 4, lg2player)
lg2node(lg2gc, 8)
check('LG2 cGenCharLocalLogic_Delete', lambda: SDTest.Call('cGenCharLocalLogic_Delete', 'tiii', lg2gc, 1)[0] == lg2gc)

lg2free(lg2fe)
lg2free(lg2l)
check('LG2 player restored', lambda: lg2psave() == lg2psaved)
