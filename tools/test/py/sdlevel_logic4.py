# game Logic/ part 4: NurseLogic.c (SetState), Taunts.c, Parking.c, PoliceBossLogic.c, ShipRegen.c, RustyLogic.c,
# Scrapyard.c, SentinelLogic.c, SputnikJunkyardLogic.c, Traffic.c (execfile'd by sdlevel.py, Python 1.5.2). The slots
# without code; the random car of fake lists; the police taunt kinds (the thread-safe statics of the exe); the taunt
# and warning early returns; the parking zones of a fake cParking (zones at the player, so the sphere test is not
# reached in the think); the regeneration on a fake cShipRegen (the life of the player saved and restored); a ship
# group made, its lairs set, read and deleted; the variables and the early returns of fake Rusty, PoliceBoss,
# Sentinel and Sputnik logics with the player as parent; the traffic small parts on a fake object; the AI
# parameters of the ship Parked (only the values the functions write are printed); the Delete of the logics on
# zeroed memory.
# Not called: anything that sets the state of an AI of a character (the player has none), the thinks of the ship
# groups and the traffic (they reach the world lists), the natives (they read a parameter file), the Init functions.
def lg4i(a):
	return SDTest.Peek(a, '@i')
def lg4b(a):
	return SDTest.Peek(a, '@b')
def lg4poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def lg4pokeb(a, v):
	lg4poke(a, (lg4i(a) & ~0xff) | v)
def lg4fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def lg4set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def lg4fset(a, v):
	lg4set(a, (v,))
def lg4z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def lg4free(p):
	SDTest.Call('Mem_Free', 'ii', p)
def lg4new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def lg4srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def lg4str(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def lg4bytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def lg4ints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def lg4vt(a):
	return lg4i(a) - 0x400000
# a node of a logic on itself, data the logic
def lg4node(l, o):
	lg4poke(l + o, l + o)
	lg4poke(l + o + 4, l + o)
	lg4poke(l + o + 0xc, l)
# an entity list with the entities of es in its ring and the count n (None: no count)
def lg4list(es, n):
	l = lg4z(0x50)
	h = l + 0x30
	prev = h
	nodes = []
	for lg4e in es:
		nd = lg4z(0x10)
		lg4poke(nd + 0xc, lg4e)
		lg4poke(prev, nd)
		lg4poke(nd + 4, prev)
		prev = nd
		nodes.append(nd)
	lg4poke(prev, h)
	lg4poke(h + 4, prev)
	if n != None:
		lg4poke(l + 0x44, n)
		lg4poke(l + 0x40, l + 0x44)
	return (l, nodes)
def lg4freelist(lg):
	for nd in lg[1]:
		lg4free(nd)
	lg4free(lg[0])
def lg4find(name):
	return SDTest.Call('PtrTable_Find@652e0', 'tiisi', lg4world + 4, name, SDTest.Call('HashTable_Slot@65370', 'tiis', lg4world + 4, name)[0])[0]
def lg4ent(name):
	it = lg4find(name)
	if it:
		return lg4i(it)
	return 0

lg4world = SDTest.Peek('g_pWorld', 'i')
lg4player = SDTest.Call('cWorld_GetUsrEntity', 'tiii', lg4world, 0)[0]
lg4parked = lg4ent('Parked')
lg4alarm = lg4b(lg4world + 0x1c70)
lg4gc2 = lg4i(lg4world + 0x2248)
lg4gc2empty = lg4gc2 != 0 and lg4i(lg4i(lg4gc2 + 0x30) + 0xc) == 0
check('LG4 start', lambda: (lg4player != 0, lg4parked != 0, lg4alarm, lg4gc2empty))

# ---- the slots without code ----
for lg4r in ('Logic_True@156080', 'Logic_True@156820', 'Logic_True@159320'):
	check('LG4 Logic_True %s' % lg4r, lambda r=lg4r: SDTest.Call(r, 'tbi', 0)[0])
for lg4r in ('Logic_NoTarget@156830', 'Logic_NoTarget@159330'):
	check('LG4 Logic_NoTarget %s' % lg4r, lambda r=lg4r: SDTest.Call(r, 'tii', 0)[0])
check('LG4 Logic_True1', lambda: SDTest.Call('Logic_True1@158520', 'tbii', 0, 5)[0])
check('LG4 Logic_Nop1', lambda: SDTest.Call('Logic_Nop1@158a80', 'tvii', 0, 5)[0])

# ---- the random car of a list ----
check('LG4 Logic_RandomCar NULL', lambda: SDTest.Call('Logic_RandomCar', 'ii', 0)[0])
lg4l = lg4list((lg4player,), 0)
check('LG4 Logic_RandomCar none', lambda: SDTest.Call('Logic_RandomCar', 'ii', lg4l[0])[0])
lg4freelist(lg4l)
lg4l = lg4list((lg4parked,), 1)
check('LG4 Logic_RandomCar one', lambda: SDTest.Call('Logic_RandomCar', 'ii', lg4l[0])[0] == lg4parked)
lg4freelist(lg4l)
lg4l = lg4list((lg4player, lg4parked, lg4player, lg4parked), 4)
lg4srand(41)
check('LG4 Logic_RandomCar four', lambda: map(lambda i: SDTest.Call('Logic_RandomCar', 'ii', lg4l[0])[0] == lg4parked,
	range(12)))
lg4freelist(lg4l)

# ---- the taunts ----
lg4talk = lg4fl(symva('g_RemainTalkTime'), 1)[0]
lg4savedt2 = lg4i(symva('g_TimeBetweeenTaunts2'))
if lg4talk <= 0.0:
	check('LG4 Taunts_PoliceKind', lambda: SDTest.Call('Taunts_PoliceKind', 'ii', lg4player)[0])
	lg4fset(symva('g_TimeBetweeenTaunts2'), 0.0)
	check('LG4 Taunts_PoliceKind no wait', lambda: map(lambda i: SDTest.Call('Taunts_PoliceKind', 'ii', lg4player)[0],
		range(5)))
	lg4poke(symva('g_TimeBetweeenTaunts2'), lg4savedt2)
	check('LG4 Taunts_PoliceKind after', lambda: SDTest.Call('Taunts_PoliceKind', 'ii', lg4player)[0])
check('LG4 Taunts_Play', lambda: (SDTest.Call('Taunts_Play', 'vssi', 'SDLg4NoShip', 'x', 3)[0],
	SDTest.Call('Taunts_Play', 'vssi', 'Player0', 'x', 4)[0]))
check('LG4 Taunts_Attacker NULL', lambda: (SDTest.Call('Taunts_Attacker', 'vii', 0, 0)[0], SDTest.Call('Taunts_Attacker', 'vii',
	lg4player, 0)[0]))
check('LG4 Taunts_Ship NULL', lambda: SDTest.Call('Taunts_Ship', 'vii', 0, 8)[0])
check('LG4 Taunts_Warnings', lambda: SDTest.Call('Taunts_Warnings', 'bf', 0.5)[0])
if lg4alarm == 0:
	lg4pokeb(lg4world + 0x1c70, 1)
	check('LG4 Taunts_Warnings alarm', lambda: SDTest.Call('Taunts_Warnings', 'bf', 0.5)[0])
	lg4pokeb(lg4world + 0x1c70, 0)

# ---- the parking zones ----
lg4pp = Scrap.UsrEntity(0).Pos
lg4pk = lg4z(0x34)
lg4set(lg4pk, (7.0,))
lg4poke(lg4pk + 4, -1)
def lg4pkf(p):
	return (lg4fl(p, 1), lg4ints(p, (0xc, 0x14, 0x18, 0x24, 0x2c, 0x30)), map(lambda o, p=p: lg4i(p + o) != 0, (4, 8,
		0x10, 0x1c, 0x20, 0x28)))
check('LG4 Parking_ctor', lambda: (SDTest.Call('Parking_ctor', 'tii', lg4pk)[0] == lg4pk, lg4pkf(lg4pk)))
check('LG4 Parking_Find empty', lambda: (SDTest.Call('Parking_Find', 'tsiiFFi', lg4pk, 0, lg4pp, (1.0, 2.0, 3.0), 0),
	SDTest.Call('Parking_Find', 'tsiiFFi', lg4pk, 1, lg4pp, (1.0, 2.0, 3.0), 1)))
for lg4v in ('_Junkyard', 'SDLg4Zone1', '_Junkyard', 'SDLg4Zone2'):
	check('LG4 Parking_AddZone %s' % lg4v, lambda v=lg4v: (SDTest.Call('Parking_AddZone', 'tviiFs', lg4pk, 0, lg4pp, v)[0],
		lg4pkf(lg4pk)))
check('LG4 Parking_AddZone ring 1', lambda: (SDTest.Call('Parking_AddZone', 'tviiFs', lg4pk, 1, lg4pp, 'SDLg4Zone3')[0],
	lg4pkf(lg4pk)))
# the zones of ring 0 in order from head
def lg4zones(p, o):
	h = lg4i(p + o)
	r = []
	n = h
	k = 0
	while n and k < 8:
		r.append((lg4str(lg4i(n + 0x14)), lg4fl(n + 8, 3) == lg4pp))
		n = lg4i(n)
		k = k + 1
		if n == h:
			break
	return r
check('LG4 Parking zones', lambda: (lg4zones(lg4pk, 4), lg4zones(lg4pk, 0x1c)))
# only _Junkyard zones in ring 0: nothing found, no sphere test
lg4pk2 = lg4z(0x34)
SDTest.Call('Parking_ctor', 'tii', lg4pk2)
SDTest.Call('Parking_AddZone', 'tviiFs', lg4pk2, 0, (1.0, 2.0, 3.0), '_Junkyard')
SDTest.Call('Parking_AddZone', 'tviiFs', lg4pk2, 0, (4.0, 5.0, 6.0), '_Junkyard')
check('LG4 Parking_Find junkyard', lambda: (SDTest.Call('Parking_Find', 'tsiiFFi', lg4pk2, 0, (0.0, 0.0, 0.0), (9.0, 9.0,
	9.0), 0), SDTest.Call('Parking_Find', 'tsiiFFi', lg4pk2, 0, (0.0, 0.0, 0.0), (9.0, 9.0, 9.0), 1), lg4pkf(lg4pk2)))
check('LG4 Parking_dtor', lambda: (SDTest.Call('Parking_dtor', 'tvi', lg4pk2)[0], lg4pkf(lg4pk2)))
lg4free(lg4pk2)
check('LG4 Parking_Farther', lambda: map(lambda v: SDTest.Call('Parking_Farther', 'bff', v[0], v[1])[0], ((1.0, 2.0),
	(2.0, 1.0), (2.0, 2.0), (-1.0, 0.0))))
check('LG4 Parking_Nearer', lambda: map(lambda v: SDTest.Call('Parking_Nearer', 'bff', v[0], v[1])[0], ((1.0, 2.0),
	(2.0, 1.0), (2.0, 2.0), (-1.0, 0.0))))
# the think: the player on foot, the zones at the player: the iterator goes on, no sphere test
check('LG4 Parking_Think', lambda: map(lambda i: (SDTest.Call('Parking_Think', 'tii', lg4pk)[0], lg4i(lg4pk + 8) != 0),
	range(4)))
lg4srand(42)
for lg4v in ((7.0, 0.5), (0.25, 0.5), (0.0, 0.0), (3.0, 1.0)):
	lg4fset(lg4pk, lg4v[0])
	check('LG4 Parking_Update%s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call('Parking_Update', 'tvif', lg4pk, v[1])[0],
		lg4fl(lg4pk, 1)))
lg4nn = SDTest.Call('Parking_NewNode', 'tii', lg4pk + 4)[0]
check('LG4 Parking_NewNode', lambda: (lg4nn != 0, lg4pkf(lg4pk)))
lg4poke(lg4nn + 8, 0)
lg4zd = lg4z(0x10)
SDTest.Call('cString_ctor', 'tii', lg4zd + 0xc)
SDTest.Call('cString_Set', 'tviz', lg4zd + 0xc, 'SDLg4Zone4')
lg4set(lg4zd, (1.0, 2.0, 3.0))
check('LG4 Parking_ListAdd', lambda: (SDTest.Call('Parking_ListAdd', 'tvii', lg4pk + 4, lg4zd)[0], lg4zones(lg4pk, 4),
	lg4pkf(lg4pk)))
check('LG4 cParkingZone_dtor', lambda: SDTest.Call('cParkingZone_dtor', 'tvi', lg4zd)[0])
lg4free(lg4zd)
SDTest.Call('cString_dtor', 'tvi', lg4nn + 0x14)
SDTest.Call('operator_delete_sized', 'vii', lg4nn, 0x18)
lg4poke(lg4pk + 0x18, 2)
check('LG4 Parking_ClearList pool', lambda: (SDTest.Call('Parking_ClearList', 'tvi', lg4pk + 4)[0], lg4pkf(lg4pk)))
check('LG4 Parking_FreePool', lambda: (SDTest.Call('Parking_FreePool', 'tvi', lg4pk + 4)[0], lg4pkf(lg4pk)))
check('LG4 Parking_dtor rest', lambda: (SDTest.Call('Parking_dtor', 'tvi', lg4pk)[0], lg4pkf(lg4pk)))
lg4free(lg4pk)

# ---- the regeneration ----
lg4rg = lg4z(0x14)
def lg4rgf(r):
	return (lg4b(r), lg4i(r + 4) != 0, lg4fl(r + 8, 1), lg4b(r + 0xc), lg4i(r + 0x10))
check('LG4 ShipRegen_ctor', lambda: (SDTest.Call('ShipRegen_ctor', 'tii', lg4rg)[0] == lg4rg, lg4rgf(lg4rg)))
check('LG4 ShipRegen_Set NULL', lambda: (SDTest.Call('ShipRegen_Set', 'tviiff', lg4rg, 0, 1.0, 2.0)[0], lg4rgf(lg4rg)))
def lg4shipf(r):
	s = lg4i(r + 4)
	return (SDTest.Peek(s, '@s'), lg4fl(s + 0x80, 3), lg4i(s + 0x8c))
check('LG4 ShipRegen_Set player', lambda: (SDTest.Call('ShipRegen_Set', 'tviiff', lg4rg, lg4player, 4.0, 8.0)[0],
	lg4rgf(lg4rg), lg4shipf(lg4rg)))
check('LG4 ShipRegen_UpdateShip not a ship', lambda: (SDTest.Call('ShipRegen_UpdateShip', 'tvif', lg4rg, 0.5)[0],
	lg4shipf(lg4rg)))
lg4life = lg4i(lg4player + 0x238)
lg4savedsplit = lg4i(symva('g_SplitScreenGame'))
for lg4v in ((10.0, 0.5, 1), (0.25, 0.5, 1), (0.25, 0.5, 0), (0.25, 0.5, 1)):
	lg4fset(lg4rg + 8, lg4v[0])
	if lg4v[2]:
		lg4poke(lg4rg + 0x10, lg4i(lg4player + 0x238))
	check('LG4 ShipRegen_Update%s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call('ShipRegen_Update', 'tvif', lg4rg, v[1])[0],
		lg4rgf(lg4rg), lg4i(lg4player + 0x238) - lg4life))
	lg4poke(lg4player + 0x238, lg4life)
lg4poke(symva('g_SplitScreenGame'), 1)
lg4fset(lg4rg + 8, 0.25)
check('LG4 ShipRegen_Update split', lambda: (SDTest.Call('ShipRegen_Update', 'tvif', lg4rg, 0.5)[0], lg4rgf(lg4rg)))
lg4poke(symva('g_SplitScreenGame'), lg4savedsplit)
check('LG4 ShipRegen_dtor', lambda: SDTest.Call('ShipRegen_dtor', 'tvi', lg4rg)[0])
lg4free(lg4rg)
check('LG4 player life', lambda: lg4i(lg4player + 0x238) == lg4life)

# ---- a ship group ----
lg4sg = lg4z(4)
check('LG4 cShipGroup_ctor', lambda: (SDTest.Call('cShipGroup_ctor', 'tiis', lg4sg, 'SDLg4Group')[0] == lg4sg,
	lg4i(lg4sg) != 0))
lg4sd = lg4i(lg4sg)
def lg4sdf(d):
	return (lg4i(d + 0x14), lg4bytes(d, (0x1c, 0x1d)), lg4i(d + 0x20) != 0, lg4i(d + 0x24), lg4fl(d + 0x28, 1),
		lg4i(d + 8) != 0)
check('LG4 cShipGroupData_ctor', lambda: lg4sdf(lg4sd))
check('LG4 cShipGroup lairs', lambda: (SDTest.Call('cShipGroup_SetLair', 'tvisF', lg4sg, 'SDLg4A', (1.0, 2.0, 3.0))[0],
	SDTest.Call('cShipGroupData_SetLair', 'tvisF', lg4sd, 'SDLg4B', (4.0, 5.0, 6.0))[0],
	SDTest.Call('cShipGroup_GetLair', 'tvisF', lg4sg, 'SDLg4A', (0.0, 0.0, 0.0)), SDTest.Call('cShipGroup_GetLair', 'tvisF', lg4sg, 'SDLg4B',
	(0.0, 0.0, 0.0)), SDTest.Call('cShipGroup_GetLair', 'tvisF', lg4sg, 'sdlg4a', (9.0, 9.0, 9.0))))
check('LG4 cShipGroup lair set again', lambda: (SDTest.Call('cShipGroup_SetLair', 'tvisF', lg4sg, 'SDLg4A', (7.0, 8.0, 9.0))[0],
	SDTest.Call('cShipGroup_GetLair', 'tvisF', lg4sg, 'SDLg4A', (0.0, 0.0, 0.0))))
check('LG4 cShipGroup_DeleteLair', lambda: (SDTest.Call('cShipGroup_DeleteLair', 'tvis', lg4sg, 'SDLg4A')[0],
	SDTest.Call('cShipGroup_DeleteLair', 'tvis', lg4sg, 'SDLg4A')[0], SDTest.Call('cShipGroup_GetLair', 'tvisF', lg4sg, 'SDLg4A', (5.0, 5.0,
	5.0)), SDTest.Call('cShipGroup_GetLair', 'tvisF', lg4sg, 'SDLg4B', (0.0, 0.0, 0.0))))
for lg4v in (0.125, 0.25, 0.0625):
	check('LG4 cShipGroup_Update %s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call('cShipGroup_Update', 'tvif', lg4sg, v)[0],
		lg4fl(lg4sd + 0x28, 1)))
check('LG4 cShipGroup_dtor', lambda: SDTest.Call('cShipGroup_dtor', 'tvi', lg4sg)[0])
lg4poke(lg4sg, 0)
check('LG4 cShipGroup_dtor none', lambda: SDTest.Call('cShipGroup_dtor', 'tvi', lg4sg)[0])
lg4free(lg4sg)
check('LG4 ShipGroup_OnAIEnd NULL', lambda: SDTest.Call('ShipGroup_OnAIEnd', 'bi', 0)[0])
if lg4parked:
	lg4sl = SDTest.Call('ShipGroup_MakeLogic', 'ii', lg4parked)[0]
	check('LG4 ShipGroup_MakeLogic', lambda: (lg4sl != 0, lg4vt(lg4sl), lg4i(lg4sl + 4) == lg4parked))
	check('LG4 cScrapyardLocalLogic_VarV other', lambda: SDTest.Call('cScrapyardLocalLogic_VarV', 'tbisDDDi', lg4sl, 'Bogus', 1.0, 2.0,
		3.0, 0))
	check('LG4 cScrapyardLocalLogic_Delete', lambda: SDTest.Call('cScrapyardLocalLogic_Delete', 'tiii', lg4sl, 1)[0] == lg4sl)

# ---- fake logics with the player as parent ----
def lg4psave():
	return (lg4i(lg4player + 0x380), lg4i(lg4player + 0x388), lg4i(lg4player + 0x38c), lg4i(lg4player + 0x390),
		lg4i(lg4player + 0x394), lg4i(lg4player + 0x4ec), lg4i(lg4player + 0x4f0))
def lg4prestore(v):
	lg4poke(lg4player + 0x380, v[0])
	lg4poke(lg4player + 0x388, v[1])
	lg4poke(lg4player + 0x38c, v[2])
	lg4poke(lg4player + 0x390, v[3])
	lg4poke(lg4player + 0x394, v[4])
	lg4poke(lg4player + 0x4ec, v[5])
	lg4poke(lg4player + 0x4f0, v[6])
def lg4pf():
	return (lg4b(lg4player + 0x381), lg4i(lg4player + 0x388), lg4i(lg4player + 0x38c), lg4fl(lg4player + 0x390, 2))
lg4psaved = lg4psave()
def lg4mk(n, vt):
	f = lg4z(n)
	lg4poke(f, 0x400000 + vt)
	lg4poke(f + 4, lg4player)
	lg4node(f, 8)
	return f

# PoliceBoss: the search time not over, then over (no character near when the list is empty)
lg4pb = lg4mk(0x68, sym('cPoliceBossLocalLogic_vftable'))
lg4set(lg4pb + 0x60, (2.0,))
check('LG4 cPoliceBossLocalLogic_FindTarget wait', lambda: (SDTest.Call('cPoliceBossLocalLogic_FindTarget', 'tii', lg4pb)[0],
	lg4fl(lg4pb + 0x60, 1)))
lg4set(lg4pb + 0x60, (0.0,))
check('LG4 cPoliceBossLocalLogic_FindTarget zero', lambda: (SDTest.Call('cPoliceBossLocalLogic_FindTarget', 'tii', lg4pb)[0],
	lg4fl(lg4pb + 0x60, 1)))
if lg4gc2empty:
	lg4set(lg4pb + 0x60, (-1.0,))
	check('LG4 cPoliceBossLocalLogic_FindTarget', lambda: (SDTest.Call('cPoliceBossLocalLogic_FindTarget', 'tii', lg4pb)[0],
		lg4fl(lg4pb + 0x60, 1)))
lg4free(lg4pb)

# Rusty: the variables of the base logic, the next junk point of a fake list
lg4ru = lg4mk(0x84, sym('cRustyLocalLogic_vftable'))
lg4poke(lg4ru + 0x18, 5)
check('LG4 cRustyLocalLogic_VarI', lambda: (SDTest.Call('cRustyLocalLogic_VarI', 'tbispi', lg4ru, 'State', 0, 0),
	SDTest.Call('cRustyLocalLogic_VarI', 'tbispi', lg4ru, 'Idle', 3, 0), SDTest.Call('cRustyLocalLogic_VarI', 'tbispi', lg4ru, 'Bogus', 0, 0)[0]))
check('LG4 cRustyLocalLogic_VarS', lambda: SDTest.Call('cRustyLocalLogic_VarS', 'tbispi', lg4ru, 'Bogus', 0, 0)[0])
lg4savedjunk = lg4ints(symva('g_JunkPointsList'), (0,)) + lg4ints(symva('g_RustyLoops'), (0,))
lg4savedlast = lg4i(symva('s_RustyLastJunkPoint'))
lg4l = lg4list((lg4player, lg4parked, lg4player), 3)
lg4poke(symva('g_JunkPointsList'), lg4l[0])
lg4srand(43)
check('LG4 cRustyLocalLogic_NextJunk', lambda: map(lambda i: (SDTest.Call('cRustyLocalLogic_NextJunk', 'tvi', lg4ru)[0],
	lg4i(symva('s_RustyLastJunkPoint')), lg4i(lg4ru + 0x68) == lg4parked), range(6)))
lg4poke(symva('g_JunkPointsList'), lg4savedjunk[0])
lg4poke(symva('s_RustyLastJunkPoint'), lg4savedlast)
lg4freelist(lg4l)
lg4free(lg4ru)

# Sentinel: the slots that do not reach the AI (busy, the player not involved, idle for the return)
lg4se = lg4mk(0x50, sym('cSentinelLocalLogic_vftable'))
def lg4sef(f):
	return (lg4i(f + 0x18), lg4bytes(f, (0x28, 0x29, 0x2a, 0x4c)), lg4fl(f + 0x48, 1))
lg4pokeb(lg4se + 0x4c, 1)
check('LG4 cSentinelLocalLogic_Seen', lambda: (SDTest.Call('cSentinelLocalLogic_Seen', 'tbi', lg4se)[0], lg4sef(lg4se)))
check('LG4 cSentinelLocalLogic_OnAlarm', lambda: SDTest.Call('cSentinelLocalLogic_OnAlarm', 'tvii', lg4se, lg4player)[0])
lg4pokeb(lg4se + 0x29, 1)
check('LG4 cSentinelLocalLogic busy', lambda: (SDTest.Call('cSentinelLocalLogic_OnRobbery', 'tviif', lg4se, lg4player, 5.0)[0],
	SDTest.Call('cSentinelLocalLogic_OnAttack', 'tvii', lg4se, lg4player)[0], SDTest.Call('cSentinelLocalLogic_OnUsr', 'tvii', lg4se, lg4player)[0],
	SDTest.Call('cSentinelLocalLogic_OnUsr@158a90', 'tvii', lg4se, lg4player)[0], SDTest.Call('cSentinelLocalLogic_GivesMoney', 'tbii', lg4se, lg4player)[0],
	lg4sef(lg4se)))
lg4pokeb(lg4se + 0x29, 0)
lg4srand(44)
check('LG4 cSentinelLocalLogic not the player', lambda: (SDTest.Call('cSentinelLocalLogic_OnRobbery', 'tviif', lg4se, lg4parked,
	-1000.0)[0], SDTest.Call('cSentinelLocalLogic_OnRobbery', 'tviif', lg4se, lg4player, 0.0)[0], SDTest.Call('cSentinelLocalLogic_OnUsr', 'tvii', lg4se,
	lg4parked)[0], SDTest.Call('cSentinelLocalLogic_OnUsr@158a90', 'tvii', lg4se, 0)[0], lg4sef(lg4se)))
lg4poke(lg4se + 0x18, 2)
check('LG4 cSentinelLocalLogic_Watch not watching', lambda: (SDTest.Call('cSentinelLocalLogic_Watch', 'tviF', lg4se, (1.0, 2.0,
	3.0))[0], lg4sef(lg4se)))
lg4pokeb(lg4se + 0x28, 1)
check('LG4 cSentinelLocalLogic_Return idle', lambda: (SDTest.Call('cSentinelLocalLogic_Return', 'tvi', lg4se)[0], lg4sef(lg4se)))
lg4pokeb(lg4se + 0x4c, 0)
lg4savedalarm = lg4ints(lg4world + 0x2070, (0,))
for lg4v in ((0, 0, 0), (1, 0, 0), (0, 1, 1), (1, 1, 0), (2, 0, 0), (2, 0, 1), (3, 0, 0), (4, 1, 0), (6, 0, 0)):
	lg4poke(lg4se + 0x18, lg4v[0])
	lg4pokeb(lg4world + 0x2071, lg4v[1])
	lg4pokeb(lg4world + 0x2072, lg4v[2])
	check('LG4 cSentinelLocalLogic_UpdateState%s' % fmt(lg4v), lambda: (SDTest.Call('cSentinelLocalLogic_UpdateState', 'tviif', lg4se,
		lg4player, 0.5)[0], lg4pf(), lg4sef(lg4se)))
lg4poke(lg4world + 0x2070, lg4savedalarm[0])
lg4prestore(lg4psaved)
lg4free(lg4se)

# Sputnik: the update of state 7 without the camera of the editor
lg4sp = lg4mk(0x68, sym('cSputnikJunkyardLocalLogic_vftable'))
lg4poke(lg4sp + 0x18, 7)
lg4savedsp = lg4ints(symva('g_SputnikPisser'), (0,)) + lg4ints(symva('g_SputnikTime'), (0,))
if lg4ent('EditCamera') == 0:
	lg4fset(symva('g_SputnikTime'), 3.0)
	check('LG4 cSputnikJunkyardLocalLogic_Update no camera', lambda: (SDTest.Call('cSputnikJunkyardLocalLogic_Update', 'tvif', lg4sp, 0.5)[0],
		lg4fl(symva('g_SputnikTime'), 1)))
lg4poke(symva('g_SputnikPisser'), lg4savedsp[0])
lg4poke(symva('g_SputnikTime'), lg4savedsp[1])
lg4free(lg4sp)

# ---- the traffic ----
lg4tr = lg4z(0x2c)
SDTest.Call('HashTable_ctor@1596f0', 'tiiii', lg4tr, 0x1c9, 1)
lg4set(lg4tr + 0x14, (0.25, 0.1875))
def lg4trf(t):
	return (lg4fl(t + 0x14, 2), lg4i(t + 0xc), lg4i(t + 0x10))
for lg4v in (0.125, 0.0625, 0.03125):
	check('LG4 cEnemiesTraffic_Update %s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call('cEnemiesTraffic_Update', 'tvif', lg4tr, v)[0],
		lg4trf(lg4tr)))
lg4srand(45)
check('LG4 Traffic_FightChance', lambda: (SDTest.Call('Traffic_FightChance', 'bii', lg4player, lg4player)[0], map(lambda i:
	SDTest.Call('Traffic_FightChance', 'bii', lg4player, lg4parked)[0], range(6))))
lg4savedprob = lg4i(symva('g_ProbTrafficFighting'))
lg4fset(symva('g_ProbTrafficFighting'), 50.0)
check('LG4 Traffic_FightChance 50', lambda: map(lambda i: SDTest.Call('Traffic_FightChance', 'bii', lg4player, lg4parked)[0],
	range(8)))
lg4poke(symva('g_ProbTrafficFighting'), lg4savedprob)
lg4l = lg4list((), 0)
lg4poke(lg4tr + 0x1c, lg4l[0])
check('LG4 cEnemiesTraffic_RemoveFarthest empty', lambda: SDTest.Call('cEnemiesTraffic_RemoveFarthest', 'tvi', lg4tr)[0])
lg4freelist(lg4l)
check('LG4 Traffic NULL', lambda: (SDTest.Call('Traffic_FindOther', 'ii', 0)[0], SDTest.Call('Traffic_OnAIEnd', 'bi', 0)[0],
	SDTest.Call('PoliceAI_RestoreParams', 'vi', 0)[0], SDTest.Call('PoliceAI_RandomParams', 'vi', 0)[0]))
check('LG4 cEnemiesTraffic_dtor', lambda: (SDTest.Call('cEnemiesTraffic_dtor', 'tvi', lg4tr)[0], lg4ints(lg4tr, (0, 4))))
lg4free(lg4tr)
# the AI parameters of Parked: random ones, then the defaults again
lg4pa = 0
if lg4parked:
	lg4pa = lg4i(lg4parked + 0x78)
if lg4pa:
	def lg4paf(a):
		p1 = SDTest.Call('cAIVehicle_GetParams1', 'tii', a)[0]
		p2 = SDTest.Call('cAIVehicle_GetParams2', 'tii', a)[0]
		p3 = SDTest.Call('cAIVehicle_GetParams3', 'tii', a)[0]
		return (lg4fl(p1, 2), lg4i(p2 + 0xc), lg4fl(p2 + 0x10, 1), lg4i(p2 + 0x14), lg4i(p3), lg4fl(p3 + 8, 1))
	def lg4painit(a):
		i1 = SDTest.Call('cAIVehicle_GetParamsInit', 'tii', a)[0]
		i2 = SDTest.Call('cAIVehicle_GetParams2Init', 'tii', a)[0]
		i3 = SDTest.Call('cAIVehicle_GetParams3Init', 'tii', a)[0]
		return (lg4fl(i1, 2), lg4i(i2 + 0xc), lg4fl(i2 + 0x10, 1), lg4i(i2 + 0x14), lg4i(i3), lg4fl(i3 + 8, 1))
	lg4srand(46)
	check('LG4 PoliceAI_RandomParams', lambda a=lg4pa: (SDTest.Call('PoliceAI_RandomParams', 'vi', a)[0], lg4paf(a)))
	check('LG4 PoliceAI_RestoreParams', lambda a=lg4pa: (SDTest.Call('PoliceAI_RestoreParams', 'vi', a)[0],
		lg4paf(a) == lg4painit(a)))
	SDTest.Call('cAIVehicle_ApplyParams', 'tvi', lg4pa)

# ---- the Delete of the logics on zeroed memory ----
lg4savedglob = lg4ints(symva('g_JunkPointsList'), (0,)) + lg4ints(symva('g_SputnikPisser'), (0,))
for lg4v in (('cPoliceBossLocalLogic_Delete', 0x68, sym('cPoliceBossLocalLogic_vftable')), ('cRustyLocalLogic_Delete', 0x84, sym('cRustyLocalLogic_vftable')), ('cSentinelLocalLogic_Delete', 0x50, sym('cSentinelLocalLogic_vftable')),
		('cSputnikJunkyardLocalLogic_Delete', 0x68, sym('cSputnikJunkyardLocalLogic_vftable'))):
	lg4d = lg4new(lg4v[1])
	lg4k = 0
	while lg4k < lg4v[1] / 4:
		lg4poke(lg4d + 4 * lg4k, 0)
		lg4k = lg4k + 1
	lg4poke(lg4d, 0x400000 + lg4v[2])
	lg4poke(lg4d + 4, lg4player)
	lg4node(lg4d, 8)
	check('LG4 Delete %s' % lg4v[0], lambda v=lg4v, d=lg4d: (SDTest.Call(v[0], 'tiii', d, 1)[0] == d,
		lg4i(symva('g_JunkPointsList')) == 0, lg4i(symva('g_SputnikPisser')) == 0))
lg4poke(symva('g_JunkPointsList'), lg4savedglob[0])
lg4poke(symva('g_SputnikPisser'), lg4savedglob[1])
check('LG4 player restored', lambda: lg4psave() == lg4psaved)
