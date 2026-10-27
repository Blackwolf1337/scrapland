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
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def lg4set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def lg4fset(a, v):
	lg4set(a, (v,))
def lg4z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def lg4free(p):
	SDTest.Call(0x133550, 'ii', p)
def lg4new(n):
	return SDTest.Call(0x1334d0, 'ii', n)[0]
def lg4srand(s):
	SDTest.Call(0x323c50, 'vi', s)
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
	return SDTest.Call(0x0652e0, 'tiisi', lg4world + 4, name, SDTest.Call(0x065370, 'tiis', lg4world + 4, name)[0])[0]
def lg4ent(name):
	it = lg4find(name)
	if it:
		return lg4i(it)
	return 0

lg4world = SDTest.Peek(0x467450, 'i')
lg4player = SDTest.Call(0x0c1430, 'tiii', lg4world, 0)[0]
lg4parked = lg4ent('Parked')
lg4alarm = lg4b(lg4world + 0x1c70)
lg4gc2 = lg4i(lg4world + 0x2248)
lg4gc2empty = lg4gc2 != 0 and lg4i(lg4i(lg4gc2 + 0x30) + 0xc) == 0
check('LG4 start', lambda: (lg4player != 0, lg4parked != 0, lg4alarm, lg4gc2empty))

# ---- the slots without code ----
for lg4r in (0x156080, 0x156820, 0x159320):
	check('LG4 Logic_True %x' % lg4r, lambda r=lg4r: SDTest.Call(r, 'tbi', 0)[0])
for lg4r in (0x156830, 0x159330):
	check('LG4 Logic_NoTarget %x' % lg4r, lambda r=lg4r: SDTest.Call(r, 'tii', 0)[0])
check('LG4 Logic_True1', lambda: SDTest.Call(0x158520, 'tbii', 0, 5)[0])
check('LG4 Logic_Nop1', lambda: SDTest.Call(0x158a80, 'tvii', 0, 5)[0])

# ---- the random car of a list ----
check('LG4 Logic_RandomCar NULL', lambda: SDTest.Call(0x154b20, 'ii', 0)[0])
lg4l = lg4list((lg4player,), 0)
check('LG4 Logic_RandomCar none', lambda: SDTest.Call(0x154b20, 'ii', lg4l[0])[0])
lg4freelist(lg4l)
lg4l = lg4list((lg4parked,), 1)
check('LG4 Logic_RandomCar one', lambda: SDTest.Call(0x154b20, 'ii', lg4l[0])[0] == lg4parked)
lg4freelist(lg4l)
lg4l = lg4list((lg4player, lg4parked, lg4player, lg4parked), 4)
lg4srand(41)
check('LG4 Logic_RandomCar four', lambda: map(lambda i: SDTest.Call(0x154b20, 'ii', lg4l[0])[0] == lg4parked,
	range(12)))
lg4freelist(lg4l)

# ---- the taunts ----
lg4talk = lg4fl(0x862148, 1)[0]
lg4savedt2 = lg4i(0x84d678)
if lg4talk <= 0.0:
	check('LG4 Taunts_PoliceKind', lambda: SDTest.Call(0x154bf0, 'ii', lg4player)[0])
	lg4fset(0x84d678, 0.0)
	check('LG4 Taunts_PoliceKind no wait', lambda: map(lambda i: SDTest.Call(0x154bf0, 'ii', lg4player)[0],
		range(5)))
	lg4poke(0x84d678, lg4savedt2)
	check('LG4 Taunts_PoliceKind after', lambda: SDTest.Call(0x154bf0, 'ii', lg4player)[0])
check('LG4 Taunts_Play', lambda: (SDTest.Call(0x155210, 'vssi', 'SDLg4NoShip', 'x', 3)[0],
	SDTest.Call(0x155210, 'vssi', 'Player0', 'x', 4)[0]))
check('LG4 Taunts_Attacker NULL', lambda: (SDTest.Call(0x155460, 'vii', 0, 0)[0], SDTest.Call(0x155460, 'vii',
	lg4player, 0)[0]))
check('LG4 Taunts_Ship NULL', lambda: SDTest.Call(0x155150, 'vii', 0, 8)[0])
check('LG4 Taunts_Warnings', lambda: SDTest.Call(0x155000, 'bf', 0.5)[0])
if lg4alarm == 0:
	lg4pokeb(lg4world + 0x1c70, 1)
	check('LG4 Taunts_Warnings alarm', lambda: SDTest.Call(0x155000, 'bf', 0.5)[0])
	lg4pokeb(lg4world + 0x1c70, 0)

# ---- the parking zones ----
lg4pp = Scrap.UsrEntity(0).Pos
lg4pk = lg4z(0x34)
lg4set(lg4pk, (7.0,))
lg4poke(lg4pk + 4, -1)
def lg4pkf(p):
	return (lg4fl(p, 1), lg4ints(p, (0xc, 0x14, 0x18, 0x24, 0x2c, 0x30)), map(lambda o, p=p: lg4i(p + o) != 0, (4, 8,
		0x10, 0x1c, 0x20, 0x28)))
check('LG4 Parking_ctor', lambda: (SDTest.Call(0x1555e0, 'tii', lg4pk)[0] == lg4pk, lg4pkf(lg4pk)))
check('LG4 Parking_Find empty', lambda: (SDTest.Call(0x155bf0, 'tsiiFFi', lg4pk, 0, lg4pp, (1.0, 2.0, 3.0), 0),
	SDTest.Call(0x155bf0, 'tsiiFFi', lg4pk, 1, lg4pp, (1.0, 2.0, 3.0), 1)))
for lg4v in ('_Junkyard', 'SDLg4Zone1', '_Junkyard', 'SDLg4Zone2'):
	check('LG4 Parking_AddZone %s' % lg4v, lambda v=lg4v: (SDTest.Call(0x155700, 'tviiFs', lg4pk, 0, lg4pp, v)[0],
		lg4pkf(lg4pk)))
check('LG4 Parking_AddZone ring 1', lambda: (SDTest.Call(0x155700, 'tviiFs', lg4pk, 1, lg4pp, 'SDLg4Zone3')[0],
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
SDTest.Call(0x1555e0, 'tii', lg4pk2)
SDTest.Call(0x155700, 'tviiFs', lg4pk2, 0, (1.0, 2.0, 3.0), '_Junkyard')
SDTest.Call(0x155700, 'tviiFs', lg4pk2, 0, (4.0, 5.0, 6.0), '_Junkyard')
check('LG4 Parking_Find junkyard', lambda: (SDTest.Call(0x155bf0, 'tsiiFFi', lg4pk2, 0, (0.0, 0.0, 0.0), (9.0, 9.0,
	9.0), 0), SDTest.Call(0x155bf0, 'tsiiFFi', lg4pk2, 0, (0.0, 0.0, 0.0), (9.0, 9.0, 9.0), 1), lg4pkf(lg4pk2)))
check('LG4 Parking_dtor', lambda: (SDTest.Call(0x155650, 'tvi', lg4pk2)[0], lg4pkf(lg4pk2)))
lg4free(lg4pk2)
check('LG4 Parking_Farther', lambda: map(lambda v: SDTest.Call(0x155e90, 'bff', v[0], v[1])[0], ((1.0, 2.0),
	(2.0, 1.0), (2.0, 2.0), (-1.0, 0.0))))
check('LG4 Parking_Nearer', lambda: map(lambda v: SDTest.Call(0x155eb0, 'bff', v[0], v[1])[0], ((1.0, 2.0),
	(2.0, 1.0), (2.0, 2.0), (-1.0, 0.0))))
# the think: the player on foot, the zones at the player: the iterator goes on, no sphere test
check('LG4 Parking_Think', lambda: map(lambda i: (SDTest.Call(0x1557a0, 'tii', lg4pk)[0], lg4i(lg4pk + 8) != 0),
	range(4)))
lg4srand(42)
for lg4v in ((7.0, 0.5), (0.25, 0.5), (0.0, 0.0), (3.0, 1.0)):
	lg4fset(lg4pk, lg4v[0])
	check('LG4 Parking_Update%s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call(0x155f70, 'tvif', lg4pk, v[1])[0],
		lg4fl(lg4pk, 1)))
lg4nn = SDTest.Call(0x155df0, 'tii', lg4pk + 4)[0]
check('LG4 Parking_NewNode', lambda: (lg4nn != 0, lg4pkf(lg4pk)))
lg4poke(lg4nn + 8, 0)
lg4zd = lg4z(0x10)
SDTest.Call(0x133980, 'tii', lg4zd + 0xc)
SDTest.Call(0x134040, 'tviz', lg4zd + 0xc, 'SDLg4Zone4')
lg4set(lg4zd, (1.0, 2.0, 3.0))
check('LG4 Parking_ListAdd', lambda: (SDTest.Call(0x155690, 'tvii', lg4pk + 4, lg4zd)[0], lg4zones(lg4pk, 4),
	lg4pkf(lg4pk)))
check('LG4 cParkingZone_dtor', lambda: SDTest.Call(0x155680, 'tvi', lg4zd)[0])
lg4free(lg4zd)
SDTest.Call(0x133990, 'tvi', lg4nn + 0x14)
SDTest.Call(0x313634, 'vii', lg4nn, 0x18)
lg4poke(lg4pk + 0x18, 2)
check('LG4 Parking_ClearList pool', lambda: (SDTest.Call(0x155b10, 'tvi', lg4pk + 4)[0], lg4pkf(lg4pk)))
check('LG4 Parking_FreePool', lambda: (SDTest.Call(0x155bb0, 'tvi', lg4pk + 4)[0], lg4pkf(lg4pk)))
check('LG4 Parking_dtor rest', lambda: (SDTest.Call(0x155650, 'tvi', lg4pk)[0], lg4pkf(lg4pk)))
lg4free(lg4pk)

# ---- the regeneration ----
lg4rg = lg4z(0x14)
def lg4rgf(r):
	return (lg4b(r), lg4i(r + 4) != 0, lg4fl(r + 8, 1), lg4b(r + 0xc), lg4i(r + 0x10))
check('LG4 ShipRegen_ctor', lambda: (SDTest.Call(0x156200, 'tii', lg4rg)[0] == lg4rg, lg4rgf(lg4rg)))
check('LG4 ShipRegen_Set NULL', lambda: (SDTest.Call(0x1564c0, 'tviiff', lg4rg, 0, 1.0, 2.0)[0], lg4rgf(lg4rg)))
def lg4shipf(r):
	s = lg4i(r + 4)
	return (SDTest.Peek(s, '@s'), lg4fl(s + 0x80, 3), lg4i(s + 0x8c))
check('LG4 ShipRegen_Set player', lambda: (SDTest.Call(0x1564c0, 'tviiff', lg4rg, lg4player, 4.0, 8.0)[0],
	lg4rgf(lg4rg), lg4shipf(lg4rg)))
check('LG4 ShipRegen_UpdateShip not a ship', lambda: (SDTest.Call(0x156250, 'tvif', lg4rg, 0.5)[0],
	lg4shipf(lg4rg)))
lg4life = lg4i(lg4player + 0x238)
lg4savedsplit = lg4i(0x867584)
for lg4v in ((10.0, 0.5, 1), (0.25, 0.5, 1), (0.25, 0.5, 0), (0.25, 0.5, 1)):
	lg4fset(lg4rg + 8, lg4v[0])
	if lg4v[2]:
		lg4poke(lg4rg + 0x10, lg4i(lg4player + 0x238))
	check('LG4 ShipRegen_Update%s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call(0x156590, 'tvif', lg4rg, v[1])[0],
		lg4rgf(lg4rg), lg4i(lg4player + 0x238) - lg4life))
	lg4poke(lg4player + 0x238, lg4life)
lg4poke(0x867584, 1)
lg4fset(lg4rg + 8, 0.25)
check('LG4 ShipRegen_Update split', lambda: (SDTest.Call(0x156590, 'tvif', lg4rg, 0.5)[0], lg4rgf(lg4rg)))
lg4poke(0x867584, lg4savedsplit)
check('LG4 ShipRegen_dtor', lambda: SDTest.Call(0x156230, 'tvi', lg4rg)[0])
lg4free(lg4rg)
check('LG4 player life', lambda: lg4i(lg4player + 0x238) == lg4life)

# ---- a ship group ----
lg4sg = lg4z(4)
check('LG4 cShipGroup_ctor', lambda: (SDTest.Call(0x1570b0, 'tiis', lg4sg, 'SDLg4Group')[0] == lg4sg,
	lg4i(lg4sg) != 0))
lg4sd = lg4i(lg4sg)
def lg4sdf(d):
	return (lg4i(d + 0x14), lg4bytes(d, (0x1c, 0x1d)), lg4i(d + 0x20) != 0, lg4i(d + 0x24), lg4fl(d + 0x28, 1),
		lg4i(d + 8) != 0)
check('LG4 cShipGroupData_ctor', lambda: lg4sdf(lg4sd))
check('LG4 cShipGroup lairs', lambda: (SDTest.Call(0x1574d0, 'tvisF', lg4sg, 'SDLg4A', (1.0, 2.0, 3.0))[0],
	SDTest.Call(0x1574e0, 'tvisF', lg4sd, 'SDLg4B', (4.0, 5.0, 6.0))[0],
	SDTest.Call(0x157c90, 'tvisF', lg4sg, 'SDLg4A', (0.0, 0.0, 0.0)), SDTest.Call(0x157c90, 'tvisF', lg4sg, 'SDLg4B',
	(0.0, 0.0, 0.0)), SDTest.Call(0x157c90, 'tvisF', lg4sg, 'sdlg4a', (9.0, 9.0, 9.0))))
check('LG4 cShipGroup lair set again', lambda: (SDTest.Call(0x1574d0, 'tvisF', lg4sg, 'SDLg4A', (7.0, 8.0, 9.0))[0],
	SDTest.Call(0x157c90, 'tvisF', lg4sg, 'SDLg4A', (0.0, 0.0, 0.0))))
check('LG4 cShipGroup_DeleteLair', lambda: (SDTest.Call(0x157680, 'tvis', lg4sg, 'SDLg4A')[0],
	SDTest.Call(0x157680, 'tvis', lg4sg, 'SDLg4A')[0], SDTest.Call(0x157c90, 'tvisF', lg4sg, 'SDLg4A', (5.0, 5.0,
	5.0)), SDTest.Call(0x157c90, 'tvisF', lg4sg, 'SDLg4B', (0.0, 0.0, 0.0))))
for lg4v in (0.125, 0.25, 0.0625):
	check('LG4 cShipGroup_Update %s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call(0x158160, 'tvif', lg4sg, v)[0],
		lg4fl(lg4sd + 0x28, 1)))
check('LG4 cShipGroup_dtor', lambda: SDTest.Call(0x157230, 'tvi', lg4sg)[0])
lg4poke(lg4sg, 0)
check('LG4 cShipGroup_dtor none', lambda: SDTest.Call(0x157230, 'tvi', lg4sg)[0])
lg4free(lg4sg)
check('LG4 ShipGroup_OnAIEnd NULL', lambda: SDTest.Call(0x157e60, 'bi', 0)[0])
if lg4parked:
	lg4sl = SDTest.Call(0x158400, 'ii', lg4parked)[0]
	check('LG4 ShipGroup_MakeLogic', lambda: (lg4sl != 0, lg4vt(lg4sl), lg4i(lg4sl + 4) == lg4parked))
	check('LG4 cScrapyardLocalLogic_VarV other', lambda: SDTest.Call(0x158260, 'tbisDDDi', lg4sl, 'Bogus', 1.0, 2.0,
		3.0, 0))
	check('LG4 cScrapyardLocalLogic_Delete', lambda: SDTest.Call(0x157310, 'tiii', lg4sl, 1)[0] == lg4sl)

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
lg4pb = lg4mk(0x68, 0x3c0a3c)
lg4set(lg4pb + 0x60, (2.0,))
check('LG4 cPoliceBossLocalLogic_FindTarget wait', lambda: (SDTest.Call(0x156090, 'tii', lg4pb)[0],
	lg4fl(lg4pb + 0x60, 1)))
lg4set(lg4pb + 0x60, (0.0,))
check('LG4 cPoliceBossLocalLogic_FindTarget zero', lambda: (SDTest.Call(0x156090, 'tii', lg4pb)[0],
	lg4fl(lg4pb + 0x60, 1)))
if lg4gc2empty:
	lg4set(lg4pb + 0x60, (-1.0,))
	check('LG4 cPoliceBossLocalLogic_FindTarget', lambda: (SDTest.Call(0x156090, 'tii', lg4pb)[0],
		lg4fl(lg4pb + 0x60, 1)))
lg4free(lg4pb)

# Rusty: the variables of the base logic, the next junk point of a fake list
lg4ru = lg4mk(0x84, 0x3c0d58)
lg4poke(lg4ru + 0x18, 5)
check('LG4 cRustyLocalLogic_VarI', lambda: (SDTest.Call(0x156900, 'tbispi', lg4ru, 'State', 0, 0),
	SDTest.Call(0x156900, 'tbispi', lg4ru, 'Idle', 3, 0), SDTest.Call(0x156900, 'tbispi', lg4ru, 'Bogus', 0, 0)[0]))
check('LG4 cRustyLocalLogic_VarS', lambda: SDTest.Call(0x157030, 'tbispi', lg4ru, 'Bogus', 0, 0)[0])
lg4savedjunk = lg4ints(0x84d844, (0, 4))
lg4savedlast = lg4i(0x828bcc)
lg4l = lg4list((lg4player, lg4parked, lg4player), 3)
lg4poke(0x84d844, lg4l[0])
lg4srand(43)
check('LG4 cRustyLocalLogic_NextJunk', lambda: map(lambda i: (SDTest.Call(0x156e30, 'tvi', lg4ru)[0],
	lg4i(0x828bcc), lg4i(lg4ru + 0x68) == lg4parked), range(6)))
lg4poke(0x84d844, lg4savedjunk[0])
lg4poke(0x828bcc, lg4savedlast)
lg4freelist(lg4l)
lg4free(lg4ru)

# Sentinel: the slots that do not reach the AI (busy, the player not involved, idle for the return)
lg4se = lg4mk(0x50, 0x3c156c)
def lg4sef(f):
	return (lg4i(f + 0x18), lg4bytes(f, (0x28, 0x29, 0x2a, 0x4c)), lg4fl(f + 0x48, 1))
lg4pokeb(lg4se + 0x4c, 1)
check('LG4 cSentinelLocalLogic_Seen', lambda: (SDTest.Call(0x158550, 'tbi', lg4se)[0], lg4sef(lg4se)))
check('LG4 cSentinelLocalLogic_OnAlarm', lambda: SDTest.Call(0x158530, 'tvii', lg4se, lg4player)[0])
lg4pokeb(lg4se + 0x29, 1)
check('LG4 cSentinelLocalLogic busy', lambda: (SDTest.Call(0x158840, 'tviif', lg4se, lg4player, 5.0)[0],
	SDTest.Call(0x158970, 'tvii', lg4se, lg4player)[0], SDTest.Call(0x158a00, 'tvii', lg4se, lg4player)[0],
	SDTest.Call(0x158a90, 'tvii', lg4se, lg4player)[0], SDTest.Call(0x158b10, 'tbii', lg4se, lg4player)[0],
	lg4sef(lg4se)))
lg4pokeb(lg4se + 0x29, 0)
lg4srand(44)
check('LG4 cSentinelLocalLogic not the player', lambda: (SDTest.Call(0x158840, 'tviif', lg4se, lg4parked,
	-1000.0)[0], SDTest.Call(0x158840, 'tviif', lg4se, lg4player, 0.0)[0], SDTest.Call(0x158a00, 'tvii', lg4se,
	lg4parked)[0], SDTest.Call(0x158a90, 'tvii', lg4se, 0)[0], lg4sef(lg4se)))
lg4poke(lg4se + 0x18, 2)
check('LG4 cSentinelLocalLogic_Watch not watching', lambda: (SDTest.Call(0x158560, 'tviF', lg4se, (1.0, 2.0,
	3.0))[0], lg4sef(lg4se)))
lg4pokeb(lg4se + 0x28, 1)
check('LG4 cSentinelLocalLogic_Return idle', lambda: (SDTest.Call(0x158fc0, 'tvi', lg4se)[0], lg4sef(lg4se)))
lg4pokeb(lg4se + 0x4c, 0)
lg4savedalarm = lg4ints(lg4world + 0x2070, (0,))
for lg4v in ((0, 0, 0), (1, 0, 0), (0, 1, 1), (1, 1, 0), (2, 0, 0), (2, 0, 1), (3, 0, 0), (4, 1, 0), (6, 0, 0)):
	lg4poke(lg4se + 0x18, lg4v[0])
	lg4pokeb(lg4world + 0x2071, lg4v[1])
	lg4pokeb(lg4world + 0x2072, lg4v[2])
	check('LG4 cSentinelLocalLogic_UpdateState%s' % fmt(lg4v), lambda: (SDTest.Call(0x158b90, 'tviif', lg4se,
		lg4player, 0.5)[0], lg4pf(), lg4sef(lg4se)))
lg4poke(lg4world + 0x2070, lg4savedalarm[0])
lg4prestore(lg4psaved)
lg4free(lg4se)

# Sputnik: the update of state 7 without the camera of the editor
lg4sp = lg4mk(0x68, 0x3c169c)
lg4poke(lg4sp + 0x18, 7)
lg4savedsp = lg4ints(0x84db70, (0, 4))
if lg4ent('EditCamera') == 0:
	lg4fset(0x84db74, 3.0)
	check('LG4 cSputnikJunkyardLocalLogic_Update no camera', lambda: (SDTest.Call(0x1593e0, 'tvif', lg4sp, 0.5)[0],
		lg4fl(0x84db74, 1)))
lg4poke(0x84db70, lg4savedsp[0])
lg4poke(0x84db74, lg4savedsp[1])
lg4free(lg4sp)

# ---- the traffic ----
lg4tr = lg4z(0x2c)
SDTest.Call(0x1596f0, 'tiiii', lg4tr, 0x1c9, 1)
lg4set(lg4tr + 0x14, (0.25, 0.1875))
def lg4trf(t):
	return (lg4fl(t + 0x14, 2), lg4i(t + 0xc), lg4i(t + 0x10))
for lg4v in (0.125, 0.0625, 0.03125):
	check('LG4 cEnemiesTraffic_Update %s' % fmt(lg4v), lambda v=lg4v: (SDTest.Call(0x15b4f0, 'tvif', lg4tr, v)[0],
		lg4trf(lg4tr)))
lg4srand(45)
check('LG4 Traffic_FightChance', lambda: (SDTest.Call(0x159aa0, 'bii', lg4player, lg4player)[0], map(lambda i:
	SDTest.Call(0x159aa0, 'bii', lg4player, lg4parked)[0], range(6))))
lg4savedprob = lg4i(0x84dc58)
lg4fset(0x84dc58, 50.0)
check('LG4 Traffic_FightChance 50', lambda: map(lambda i: SDTest.Call(0x159aa0, 'bii', lg4player, lg4parked)[0],
	range(8)))
lg4poke(0x84dc58, lg4savedprob)
lg4l = lg4list((), 0)
lg4poke(lg4tr + 0x1c, lg4l[0])
check('LG4 cEnemiesTraffic_RemoveFarthest empty', lambda: SDTest.Call(0x159c10, 'tvi', lg4tr)[0])
lg4freelist(lg4l)
check('LG4 Traffic NULL', lambda: (SDTest.Call(0x15ab20, 'ii', 0)[0], SDTest.Call(0x15ae60, 'bi', 0)[0],
	SDTest.Call(0x15b2f0, 'vi', 0)[0], SDTest.Call(0x15b3c0, 'vi', 0)[0]))
check('LG4 cEnemiesTraffic_dtor', lambda: (SDTest.Call(0x159860, 'tvi', lg4tr)[0], lg4ints(lg4tr, (0, 4))))
lg4free(lg4tr)
# the AI parameters of Parked: random ones, then the defaults again
lg4pa = 0
if lg4parked:
	lg4pa = lg4i(lg4parked + 0x78)
if lg4pa:
	def lg4paf(a):
		p1 = SDTest.Call(0x08bc20, 'tii', a)[0]
		p2 = SDTest.Call(0x08bc40, 'tii', a)[0]
		p3 = SDTest.Call(0x08bc60, 'tii', a)[0]
		return (lg4fl(p1, 2), lg4i(p2 + 0xc), lg4fl(p2 + 0x10, 1), lg4i(p2 + 0x14), lg4i(p3), lg4fl(p3 + 8, 1))
	def lg4painit(a):
		i1 = SDTest.Call(0x08bc30, 'tii', a)[0]
		i2 = SDTest.Call(0x08bc50, 'tii', a)[0]
		i3 = SDTest.Call(0x08bc70, 'tii', a)[0]
		return (lg4fl(i1, 2), lg4i(i2 + 0xc), lg4fl(i2 + 0x10, 1), lg4i(i2 + 0x14), lg4i(i3), lg4fl(i3 + 8, 1))
	lg4srand(46)
	check('LG4 PoliceAI_RandomParams', lambda a=lg4pa: (SDTest.Call(0x15b3c0, 'vi', a)[0], lg4paf(a)))
	check('LG4 PoliceAI_RestoreParams', lambda a=lg4pa: (SDTest.Call(0x15b2f0, 'vi', a)[0],
		lg4paf(a) == lg4painit(a)))
	SDTest.Call(0x0868e0, 'tvi', lg4pa)

# ---- the Delete of the logics on zeroed memory ----
lg4savedglob = lg4ints(0x84d844, (0,)) + lg4ints(0x84db70, (0,))
for lg4v in ((0x156050, 0x68, 0x3c0a3c), (0x1567e0, 0x84, 0x3c0d58), (0x1584f0, 0x50, 0x3c156c),
		(0x1592e0, 0x68, 0x3c169c)):
	lg4d = lg4new(lg4v[1])
	lg4k = 0
	while lg4k < lg4v[1] / 4:
		lg4poke(lg4d + 4 * lg4k, 0)
		lg4k = lg4k + 1
	lg4poke(lg4d, 0x400000 + lg4v[2])
	lg4poke(lg4d + 4, lg4player)
	lg4node(lg4d, 8)
	check('LG4 Delete %x' % lg4v[0], lambda v=lg4v, d=lg4d: (SDTest.Call(v[0], 'tiii', d, 1)[0] == d,
		lg4i(0x84d844) == 0, lg4i(0x84db70) == 0))
lg4poke(0x84d844, lg4savedglob[0])
lg4poke(0x84db70, lg4savedglob[1])
check('LG4 player restored', lambda: lg4psave() == lg4psaved)
