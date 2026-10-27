# game Entities/Entity.c and Entities/Game/Doors.c (execfile'd by sdlevel.py, Python 1.5.2): cEntity in the level FZ.
# The entities of the level are counted by class through the world ring (cEntity_Next); the base class functions run
# on Player0 (only reads) and on plain "Entity" entities made by cWorld_CreateEntity: the fields of the
# constructor, the unique names, the script variables, positions, aim, the collision mask, the entity list ring,
# the links by name on nodes in calloc memory, Del and the deleting destructor (after the name left the table, as
# the world does it). The doors: FZ has no door, so only the native functions that need none: ChangeDoorState with
# an unknown state on an "Obj" (a warning), AcceleratorOnEnter with a car that is no "Car" or an object that is
# no "Obj", and the sphere callback of the door slots on the player with door positions and angles set by hand
# (the door globals are restored). Not called: the slot functions (they need the actions of a door) and
# AcceleratorOnEnter on a real car (it pushes it).
def ntpeek(a):
	return SDTest.Peek(a, '@i')
def ntints(a, n):
	return map(lambda i, a=a: ntpeek(a + 4 * i), range(n))
def ntname(e):
	if e:
		return SDTest.Peek(e + 4, '@p')
	return None
def ntstr(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def ntcalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def ntfree(p):
	SDTest.Call(0x133550, 'ii', p)
def ntfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def ntsetfl(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def ntpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
# a vtable slot of the entity as rva
def ntslot(e, off):
	return ntpeek(ntpeek(e) + off) - 0x400000
def ntstable(name, v):
	if name == 'Parked':
		return 'moving'
	return v

ntworld = SDTest.Peek(0x467450, 'i')
ntplayer = SDTest.Call(0x0c1430, 'tiii', ntworld, 0)[0]
check('entity player', lambda: ntname(ntplayer))

# the world ring by cEntity_Next: the entities of each class (GetClassName through the vtable)
ntclasses = {}
ntobjs = []
ntitems = []
ntcount = 0
nte = SDTest.Call(0x2092a0, 'tii', ntworld)[0]
while nte and ntcount < 5000:
	ntcount = ntcount + 1
	c = SDTest.Call(ntslot(nte, 0x10), 'tsi', nte)[0]
	if ntclasses.has_key(c):
		ntclasses[c] = ntclasses[c] + 1
	else:
		ntclasses[c] = 1
	if SDTest.Call(ntslot(nte, 0x14), 'tbis', nte, 'Obj')[0]:
		ntobjs.append(ntname(nte))
	if c == 'item':
		ntitems.append(nte)
	nte = SDTest.Call(0x0ef310, 'tii', nte)[0]
ntkeys = ntclasses.keys()
ntkeys.sort()
check('entity count', lambda: ntcount)
check('entity classes', lambda: map(lambda k: (k, ntclasses[k]), ntkeys))
check('entity Obj', lambda: len(ntobjs))
check('cEntity_NextInList(player)', lambda: ntname(SDTest.Call(0x0ef2f0, 'tii', ntplayer)[0]))
check('cEntity_GetListName(player)', lambda: SDTest.Call(0x0ef2e0, 'tsi', ntplayer)[0])

# the base class functions on the player: reads only
for v in ('ActiveCMask', 'Active', 'isLocalLogic', 'isAI', 'Bogus', 'Activ', 'ActiveCMaskX', ''):
	check('cEntity_VarI(player, %s)' % v, lambda v=v: SDTest.Call(0x0ef380, 'tbispi', ntplayer, v, -5, 0))
for v in ('Name', 'Type', 'NextInSlot', 'NextInList', 'EntityList', 'Bogus', 'Nam', ''):
	check('cEntity_VarS(player, %s)' % v, lambda v=v: (lambda r: (r[0], r[0] and ntstr(r[1])))(
		SDTest.Call(0x0ef9b0, 'tbispi', ntplayer, v, 0, 0)))
for v in ('Pos', 'Po', 'Bogus', ''):
	check('cEntity_VarV(player, %s)' % v, lambda v=v: SDTest.Call(0x0efc70, 'tbisDDDi', ntplayer, v, -1.0, -2.0, -3.0, 0))
for v in ('Life', 'Bogus'):
	check('cEntity_VarF(player, %s)' % v, lambda v=v: SDTest.Call(0x0ef250, 'tbisDi', ntplayer, v, -1.0, 0))
check('cEntity_GetPos(player)', lambda: (lambda r: (r[0] != 0, r[1]))(SDTest.Call(0x0ef330, 'tiiF', ntplayer,
	(0.0, 0.0, 0.0))))
check('cEntity_GetAI(player)', lambda: SDTest.Call(0x0ef2d0, 'tii', ntplayer)[0] != 0)
check('cEntity_IsType(player)', lambda: map(lambda t: SDTest.Call(0x0ef5d0, 'tiis', ntplayer, t)[0],
	('Entity', 'entity', 'WalkChar', '', 'Entityx')))

# plain entities: the second one with a taken name
check('nameCount before', lambda: ntpeek(ntworld + 0x30))
nt1 = SDTest.Call(0x208360, 'tiisfffs', ntworld, 'SDEnt', 100.0, 200.0, 300.0, 'Entity')[0]
nt2 = SDTest.Call(0x208360, 'tiisfffs', ntworld, 'SDEnt', -1.5, 2.25, -3.125, 'Entity')[0]
nt3 = SDTest.Call(0x208360, 'tiisfffs', ntworld, 'SDEntB', 0.0, 0.0, 0.0, 'Entity')[0]
check('cEntity_ctor names', lambda: (ntname(nt1), ntname(nt2), ntname(nt3), ntpeek(ntworld + 0x30)))
def ntfields(e):
	return (ntpeek(e) == 0x7b25f8, ntfl(e + 0x28, 3), ntints(e + 0x44, 2), SDTest.Peek(e + 0x4c, '@b'),
		ntpeek(e + 0x14) == e, ntpeek(e + 0x24) == e, ntpeek(e + 0x40) == e, ntpeek(e + 0x5c) == e,
		ntpeek(e + 0x6c) == e, ntpeek(e + 0x10) == ntworld, ntpeek(e + 0x20), ntpeek(e + 0x3c), ntpeek(e + 0x58),
		ntpeek(e + 0x60), ntpeek(e + 0x70), ntints(e + 0x74, 2), ntpeek(e + 0x64) == e + 0x64)
for e in (nt1, nt2, nt3):
	check('cEntity_ctor fields %s' % ntname(e), lambda e=e: ntfields(e))
check('world ring order', lambda: (ntpeek(ntworld + 0x10) == nt3 + 8, ntpeek(nt3 + 8) == nt2 + 8,
	ntpeek(nt2 + 8) == nt1 + 8, ntpeek(nt1 + 0xc) == nt2 + 8, ntpeek(nt3 + 0xc) == ntworld + 0x10))
check('cEntity_Next', lambda: (ntname(SDTest.Call(0x0ef310, 'tii', nt3)[0]), ntname(SDTest.Call(0x0ef310, 'tii', nt2)[0])))
check('cWorld_GetFirstEntity new', lambda: ntname(SDTest.Call(0x2092a0, 'tii', ntworld)[0]))
check('Scrap.GetEntity(SDEnt).Name', lambda: Scrap.GetEntity('SDEnt').Name)
check('cEntity_GetClassName', lambda: SDTest.Call(0x0ef360, 'tsi', nt1)[0])
check('cEntity_IsType', lambda: map(lambda t: SDTest.Call(0x0ef5d0, 'tiis', nt1, t)[0], ('Entity', 'entity', 'Obj', '')))
check('cEntity_GetAI', lambda: SDTest.Call(0x0ef2d0, 'tii', nt1)[0])
check('cEntity_Update/Render/UpdateOff', lambda: (SDTest.Call(0x0ef9a0, 'tvi', nt1), SDTest.Call(0x0ef760, 'tvii', nt1, 0),
	SDTest.Call(0x0ef370, 'tvi', nt1), ntfields(nt1)))
check('cEntity_GetTargetAngle', lambda: SDTest.Call(0x0ef350, 'tviFfp', nt1, (0.5, 1.0), 100.0, 77))
check('cEntity_SetEmbeddedFX', lambda: SDTest.Call(0x0ef940, 'tvis', nt1, 'SDFX'))
check('cEntity_SetModel', lambda: SDTest.Call(0x0ef960, 'tvisfffi', nt1, 'SDModel', 1.0, 1.0, 1.0, 0))

# positions
check('cEntity_GetPos', lambda: (lambda r: (r[0] != 0, r[1]))(SDTest.Call(0x0ef330, 'tiiF', nt2, (9.0, 9.0, 9.0))))
check('cEntity_SetPos', lambda: (SDTest.Call(0x0ef980, 'tviF', nt2, (7.5, -8.25, 1e10)), ntfl(nt2 + 0x28, 3)))
for v in (('Pos', 1, (11.0, 22.5, -33.75)), ('Pos', 1, (1e40, -1e-50, 0.1)), ('Pos', 0, (0.0, 0.0, 0.0)), ('Pos', 2, (4.0, 5.0, 6.0)),
		('Pos', 256, (-4.0, -5.0, -6.0)), ('pos', 1, (1.0, 1.0, 1.0)), ('', 1, (1.0, 1.0, 1.0)), ('AI_Pos', 0, (0.0, 0.0, 0.0)),
		('LLogicPos', 0, (0.0, 0.0, 0.0))):
	n, s, p = v
	check('cEntity_VarV' + fmt(v), lambda n=n, s=s, p=p: (SDTest.Call(0x0efc70, 'tbisDDDi', nt2, n, p[0], p[1], p[2], s),
		ntfl(nt2 + 0x28, 3)))
for v in ('Pos', 'AI_x', 'LLogicx', ''):
	check('cEntity_VarF(%s)' % v, lambda v=v: (SDTest.Call(0x0ef250, 'tbisDi', nt2, v, 2.5, 0),
		SDTest.Call(0x0ef250, 'tbisDi', nt2, v, 2.5, 1)))

# the aim from nt1 placed above the player: the floor and, with the mask of the player, the player
ntpp = SDTest.Call(0x0ef330, 'tiiF', ntplayer, (0.0, 0.0, 0.0))[1]
ntsetfl(nt1 + 0x28, (ntpp[0], ntpp[1] + 300.0, ntpp[2]))
ntmask = ntpeek(ntplayer + 0x48)
def ntaim(ax, ay, d, m):
	r = SDTest.Call(0x0ef110, 'tviffFFpi', nt1, ax, ay, (d,), (0.0, 0.0, 0.0), 77, m)
	n = ntname(r[3])
	return (ntstable(n, (r[1], r[2])), n)
for ax, ay in ((0.0, 0.0), (1.5707964, 0.0), (-1.5707964, 0.0), (0.3, 2.0), (-0.4, -1.0), (1.2, 3.1)):
	for d in (100.0, 5000.0):
		for m in (0, ntmask):
			check('cEntity_AimWeapon' + fmt((ax, ay, d, m)), lambda ax=ax, ay=ay, d=d, m=m: ntaim(ax, ay, d, m))

# the active flag and the collision mask
for v in (('Active', 1, 0), ('Active', 0, 0), ('Active', 1, 1), ('Active', 0, 1), ('Active', 256, 0), ('Active', 7, 1),
		('Active', 0, 0)):
	n, s, i = v
	check('cEntity_VarI' + fmt(v), lambda n=n, s=s, i=i: (SDTest.Call(0x0ef380, 'tbispi', nt1, n, i, s),
		SDTest.Peek(nt1 + 0x4c, '@b')))
check('cEntity_SetActive(0)', lambda: (SDTest.Call(0x0ef910, 'tvii', nt1, 0), SDTest.Peek(nt1 + 0x4c, '@b'),
	ntpeek(nt1 + 0x3c)))
check('cEntity_SetActive(1)', lambda: (SDTest.Call(0x0ef910, 'tvii', nt1, 1), SDTest.Peek(nt1 + 0x4c, '@b')))
ntpoke(nt1 + 0x48, 6)
for f, n in ((0x0ef040, 'Off'), (0x0ef040, 'Off'), (0x0ef8f0, 'On'), (0x0ef8f0, 'On')):
	check('cEntity_Collision' + n, lambda f=f: (SDTest.Call(f, 'tvi', nt1), ntints(nt1 + 0x44, 2)))
for v in (('ActiveCMask', 0, 9), ('ActiveCMask', 1, 0), ('ActiveCMask', 0, 0), ('ActiveCMask', 1, 0), ('ActiveCMask', 1, 5),
		('ActiveCMask', 1, 5), ('ActiveCMask', 0, 5), ('ActiveCMask', 512, 0), ('isLocalLogic', 0, 0), ('isLocalLogic', 1, 0),
		('isAI', 0, 0), ('isAI', 1, 3), ('AI_isAI', 0, 0), ('LLogicisAI', 0, 0), ('Bogus', 0, 0), ('Bogus', 1, 0), ('', 0, 0)):
	n, s, i = v
	check('cEntity_VarI' + fmt(v), lambda n=n, s=s, i=i: (SDTest.Call(0x0ef380, 'tbispi', nt1, n, i, s),
		ntints(nt1 + 0x44, 2)))
ntpoke(nt1 + 0x48, 0)
ntpoke(nt1 + 0x44, 0)

# the entity list ring: SDEntList (its local logic factory gives NULL)
check('Scrap.CreateEntityList(SDEntList)', lambda: Scrap.CreateEntityList('SDEntList'))
ntlist = SDTest.Call(0x209240, 'tiis', ntworld, 'SDEntList')[0]
def ntcnt():
	return ntpeek(ntpeek(ntlist + 0x40))
def ntgets(e, n):
	r = SDTest.Call(0x0ef9b0, 'tbispi', e, n, 0, 0)
	return (r[0], r[0] and ntstr(r[1]))
# the strings for the set of EntityList (a char * by pointer)
def ntcstr(s):
	a = ntcalloc(32)
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
	return a
ntsdl = ntcstr('SDEntList')
ntsdn = ntcstr('SDNoList')
ntsde = ntcstr('')
check('list count', lambda: ntcnt())
for e in (nt1, nt2):
	check('cEntity_VarS(EntityList set) %s' % ntname(e), lambda e=e: (SDTest.Call(0x0ef9b0, 'tbispi', e, 'EntityList',
		ntsdl, 1)[0], ntcnt(), ntpeek(e + 0x58) == ntlist, ntpeek(e + 0x60) == ntpeek(ntlist + 0x40), ntints(e + 0x74, 2)))
check('cEntityList_GetFirst', lambda: ntname(SDTest.Call(0x1a7bc0, 'tii', ntlist)[0]))
for e in (nt1, nt2, nt3):
	for v in ('EntityList', 'NextInList', 'NextInSlot', 'Name', 'Type'):
		check('cEntity_VarS(%s) %s' % (v, ntname(e)), lambda e=e, v=v: ntgets(e, v))
	check('cEntity_GetListName %s' % ntname(e), lambda e=e: SDTest.Call(0x0ef2e0, 'tsi', e)[0])
	check('cEntity_NextInList %s' % ntname(e), lambda e=e: ntname(SDTest.Call(0x0ef2f0, 'tii', e)[0]))
check('cEntity_SetList(SDNoList)', lambda: (SDTest.Call(0x0ef620, 'tbis', nt2, 'SDNoList')[0], ntcnt(),
	ntpeek(nt2 + 0x58), ntpeek(nt2 + 0x60), ntpeek(nt2 + 0x50) == nt2 + 0x50))
check('cEntity_SetList(SDEntList) again', lambda: (SDTest.Call(0x0ef620, 'tbis', nt2, 'SDEntList')[0], ntcnt()))
check('cEntity_SetList(SDEntList) twice', lambda: (SDTest.Call(0x0ef620, 'tbis', nt2, 'SDEntList')[0], ntcnt()))
check('cEntity_SetList("")', lambda: (SDTest.Call(0x0ef620, 'tbis', nt2, '')[0], ntcnt(), ntpeek(nt2 + 0x58)))
check('cEntity_SetList(NULL)', lambda: (SDTest.Call(0x0ef620, 'tbii', nt3, 0)[0], ntcnt()))
check('cEntity_VarS(EntityList set SDNoList)', lambda: (SDTest.Call(0x0ef9b0, 'tbispi', nt3, 'EntityList', ntsdn, 1)[0],
	ntcnt()))
check('cEntity_VarS(EntityList set "")', lambda: (SDTest.Call(0x0ef9b0, 'tbispi', nt3, 'EntityList', ntsde, 1)[0],
	ntcnt()))
check('cEntity_SetList(SDEntList) nt3', lambda: (SDTest.Call(0x0ef620, 'tbis', nt3, 'SDEntList')[0], ntcnt()))
for v in ('Name', 'Type', 'NextInSlot', 'Bogus'):
	check('cEntity_VarS(%s set)' % v, lambda v=v: SDTest.Call(0x0ef9b0, 'tbispi', nt3, v, ntsde, 1)[0])

# links by name on two nodes in calloc memory (data set: a link, not a head)
ntn1 = ntcalloc(16)
ntn2 = ntcalloc(16)
for n in (ntn1, ntn2):
	ntpoke(n, n)
	ntpoke(n + 4, n)
	ntpoke(n + 12, n)
def ntnode(n):
	return (ntpeek(n) == n, ntpeek(n + 4) == n, ntname(ntpeek(n + 8)))
for n, name in ((ntn1, 'Player0'), (ntn1, 'SDEnt'), (ntn1, 'SDEntB'), (ntn2, 'SDEnt'), (ntn1, 'SDNoEntity'), (ntn2, ''),
		(ntn1, 'Player0')):
	check('Nto1Node_LinkByName' + fmt((n == ntn1, name)), lambda n=n, name=name: (SDTest.Call(0x0ef6e0, 'vis', n, name),
		ntnode(n), SDTest.Call(0x0efc50, 'si', n)[0]))
check('Nto1Node_LinkAs', lambda: (SDTest.Call(0x0ef060, 'vii', ntn2, ntn1), ntnode(ntn2), SDTest.Call(0x0efc50, 'si', ntn2)[0],
	ntpeek(ntn2 + 4) == ntplayer + 0x64))
check('player links', lambda: ntpeek(ntplayer + 0x64) == ntn2)
check('Nto1Node_LinkByName unlink', lambda: (SDTest.Call(0x0ef6e0, 'vis', ntn1, 'SDNoEntity'), ntnode(ntn1)))
check('Nto1Node_LinkAs unlinked', lambda: (SDTest.Call(0x0ef060, 'vii', ntn2, ntn1), ntnode(ntn2),
	SDTest.Call(0x0efc50, 'si', ntn2)[0]))
check('Nto1Node_LinkAs into nt3', lambda: (SDTest.Call(0x0ef6e0, 'vis', ntn1, 'SDEntB'),
	SDTest.Call(0x0ef060, 'vii', ntn2, ntn1), ntnode(ntn2), ntpeek(nt3 + 0x64) == ntn2, ntpeek(nt3 + 0x68) == ntn1))
# the destructor of nt3 unlinks both nodes (Nto1Head_UnlinkAll of its links)
check('cEntity_Del(nt2)', lambda: (SDTest.Call(0x0ef0c0, 'tbi', nt2)[0], ntpeek(nt2 + 0x20) == ntworld,
	ntpeek(ntworld + 0x20) == nt2 + 0x18))
check('cEntity_Del(nt2) again', lambda: SDTest.Call(0x0ef0c0, 'tbi', nt2)[0])
check('name of nt3 out', lambda: SDTest.Call(0x208810, 'tbis', ntworld + 4, 'SDEntB')[0])
check('cEntity_Delete(nt3)', lambda: (SDTest.Call(0x0eee00, 'tiii', nt3, 1)[0] == nt3, ntcnt(), ntnode(ntn1), ntnode(ntn2),
	ntname(SDTest.Call(0x0ef310, 'tii', nt2)[0]) == ntname(nt1)))
check('Scrap.GetEntity(SDEntB)', lambda: Scrap.GetEntity('SDEntB'))
ntfree(ntn1)
ntfree(ntn2)
ntfree(ntsdl)
ntfree(ntsdn)
ntfree(ntsde)

# cItemEntity: the items of the level (reads only) and SDItem, made by cWorld_CreateEntity without a model and
# deleted again (its Update and Tick are not called: no model)
ntivi = ('FadeInEffect', 'FadeOutEffect', 'Blink', 'AutoFace', 'AppearsIfFree', 'OnlyUser', 'Active', 'Bogus', '')
ntivf = ('TakeSoundVol', 'Radius', 'rTime', 'BackTime', 'Bogus', '')
ntivs = ('TakeSound', 'Owner', 'Name', 'Type', 'OnTake', 'TgTypeMarker', 'Bogus', '')
ntivv = ('Origin', 'Vel', 'Pos', 'Bogus', '')
def ntitemfields(e):
	return (ntname(e), ntpeek(e) == 0x7b294c, ntfl(e + 0x228, 1), ntpeek(e + 0x22c), ntstr(e + 0x230), ntfl(e + 0x2b0, 4),
		ntpeek(e + 0x2c0) & 0xffffff, ntfl(e + 0x2c4, 1), ntstr(e + 0x2c8), SDTest.Peek(e + 0x348, '@b'),
		ntname(ntpeek(e + 0x354)), ntpeek(e + 0x35c) & 0xffff, ntfl(e + 0x360, 1), SDTest.Peek(e + 0x364, '@b'),
		ntints(e + 0x368, 2), ntpeek(e + 0x48), ntfl(e + 0xd4, 3))
def ntitem(e, tag):
	check('cItemEntity fields %s' % tag, lambda e=e: ntitemfields(e))
	for v in ntivi:
		check('cItemEntity_VarI(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call(0x0f0fa0, 'tbispi', e, v, -5, 0))
	for v in ntivf:
		check('cItemEntity_VarF(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call(0x0f0d20, 'tbisDi', e, v, -5.0, 0))
	for v in ntivs:
		check('cItemEntity_VarS(%s, %s)' % (tag, v), lambda e=e, v=v: (lambda r: (r[0], r[0] and ntstr(r[1])))(
			SDTest.Call(0x0f1f70, 'tbispi', e, v, 0, 0)))
	for v in ntivv:
		check('cItemEntity_VarV(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call(0x0f2220, 'tbisDDDi', e, v, -1.0, -2.0,
			-3.0, 0))
	check('cItemEntity_IsType(%s)' % tag, lambda e=e: map(lambda t, e=e: SDTest.Call(0x0f1270, 'tbis', e, t)[0],
		('item', 'Obj', 'Entity', 'Car', 'itemx', '')))
	check('cItemEntity getters %s' % tag, lambda e=e: (SDTest.Call(0x0f0f80, 'tsi', e)[0], SDTest.Call(0x0f0f90, 'tfi', e)[0],
		SDTest.Call(0x0f0f70, 'tfi', e)[0], SDTest.Call(0x0f0f60, 'tsi', e)[0],
		(lambda r: (r[0] != 0, r[1]))(SDTest.Call(0x0f0f40, 'tiiF', e, (0.0, 0.0, 0.0)))))
check('items', lambda: len(ntitems))
for i in range(len(ntitems)):
	if i < 6:
		ntitem(ntitems[i], 'level %d' % i)
nti = SDTest.Call(0x208360, 'tiisfffs', ntworld, 'SDItem', ntpp[0] + 500.0, ntpp[1], ntpp[2], 'item')[0]
check('cItemEntity_ctor', lambda: ntpeek(nti + 0x34c) == nti + 0x34c and ntpeek(nti + 0x358) == nti)
ntitem(nti, 'SDItem')
ntcalls = []
def ntontake(*args):
	ntcalls.append(args)
def ntvarsi(n, s):
	a = ntcstr(s)
	r = SDTest.Call(0x0f1f70, 'tbispi', nti, n, a, 1)[0]
	ntfree(a)
	return r
for n, s in (('TakeSound', 'SDSound'), ('TakeSound', ''), ('Owner', 'SDNoOwner'), ('TgTypeMarker', 'rBgnk'),
		('TgTypeMarker', 'RX'), ('TgTypeMarker', ''), ('OnTake', '__main__.ntontake'), ('OnReset', '__main__.ntontake'),
		('Name', 'x'), ('Bogus', 'x')):
	check('cItemEntity_VarS(SDItem, %s set %s)' % (n, s), lambda n=n, s=s: (ntvarsi(n, s), ntitemfields(nti)))
for v in (('FadeInEffect', 2), ('FadeOutEffect', 0), ('Blink', 7), ('AutoFace', 1), ('AppearsIfFree', 1), ('OnlyUser', 0),
		('Blink', 0), ('AutoFace', 0), ('Bogus', 1), ('Blink', 256)):
	n, i = v
	check('cItemEntity_VarI(SDItem, %s set %d)' % v, lambda n=n, i=i: (SDTest.Call(0x0f0fa0, 'tbispi', nti, n, i, 1)[0],
		SDTest.Call(0x0f0fa0, 'tbispi', nti, n, -5, 0)))
for v in (('TakeSoundVol', 0.75), ('Radius', 250.5), ('rTime', 5.0), ('rTime', 0.0), ('BackTime', 2.0), ('BackTime', 0.0),
		('rTime', -1.0), ('Bogus', 1.0)):
	n, f = v
	check('cItemEntity_VarF(SDItem, %s set %s)' % (n, fmt(f)), lambda n=n, f=f: (SDTest.Call(0x0f0d20, 'tbisDi', nti, n, f, 1)[0],
		ntitemfields(nti)))
check('cItemEntity_VarV(SDItem, Origin set)', lambda: (SDTest.Call(0x0f2220, 'tbisDDDi', nti, 'Origin', 1.5, -2.5, 1e10, 1)[0],
	ntfl(nti + 0x2b0, 3)))
check('cItemEntity_VarV(SDItem, Vel set)', lambda: (SDTest.Call(0x0f2220, 'tbisDDDi', nti, 'Vel', 10.0, 0.0, -5.0, 1)[0],
	ntfl(nti + 0xd4, 3), SDTest.Call(0x0f2220, 'tbisDDDi', nti, 'Vel', 0.0, 0.0, 0.0, 0)))
check('cItemEntity_SetOrigin', lambda: (SDTest.Call(0x0f1730, 'tviF', nti, (ntpp[0] + 600.0, ntpp[1], ntpp[2])),
	ntfl(nti + 0x2b0, 3)))
check('cItemEntity_SetPos', lambda: (SDTest.Call(0x0f17a0, 'tviF', nti, (ntpp[0] + 700.0, ntpp[1], ntpp[2])),
	ntpeek(nti + 0x22c), ntfl(nti + 0x28, 3)))
for t in (3.0, 0.0, -2.0, 1.0):
	check('cItemEntity_SetBackTime(%s)' % fmt(t), lambda t=t: (SDTest.Call(0x0f17d0, 'tvif', nti, t), ntfl(nti + 0x2bc, 1),
		ntfl(nti + 0x28, 3), len(ntcalls)))
for t in (3.0, 4.0, 0.0, -1.0, 2.0, 0.0):
	check('cItemEntity_SetRespawn(%s)' % fmt(t), lambda t=t: (SDTest.Call(0x0f1840, 'tvif', nti, t), ntitemfields(nti)))
for s in ('RBGNK', 'rbgnk', 'Rx?', '', 'kN', 'A'):
	check('cItemEntity_SetTypeMarker(%s)' % s, lambda s=s: (SDTest.Call(0x0f1b60, 'tvis', nti, s), ntpeek(nti + 0x35c) & 0xffff))
for s in ('', 'Player0', 'SDNoOwner'):
	check('cItemEntity_SetOwner(%s)' % s, lambda s=s: (SDTest.Call(0x0f1760, 'tvis', nti, s), ntitemfields(nti)))
SDTest.Call(0x0f1840, 'tvif', nti, 0.0)
# the sphere: the other entity nt1 (its touch slot does nothing) or none
ntip = SDTest.Call(0x0ef330, 'tiiF', nti, (0.0, 0.0, 0.0))[1]
for d in ((0.0, 0.0, 0.0), (100.0, 0.0, 0.0), (0.5, 0.25, 0.0), (300.0, 0.0, 0.0), (0.0, -260.0, 0.0), (1000.0, 1000.0, 0.0)):
	c = (ntip[0] + d[0], ntip[1] + d[1], ntip[2] + d[2])
	for r in (0.0, 10.0, 100.0):
		for o in (0, nt1):
			check('cItemEntity_TestSphere' + fmt((d, r, o != 0)), lambda c=c, r=r, o=o: SDTest.Call(0x0f1da0, 'tbiiFfi', nti, o,
				c, r, 0)[0])
# the touch of nt1: its mask, OnlyUser and the type marker decide; OnTake gets (item, taker)
def nttouch():
	SDTest.Call(0x0f0c10, 'tviiii', nti, nt1, 0, 0)
	return (map(lambda c: c, ntcalls), ntname(ntpeek(nti + 0x354)))
check('cItemEntity_Touch mask 0', lambda: nttouch())
ntpoke(nt1 + 0x48, 0x11)
check('cItemEntity_Touch', lambda: nttouch())
check('cItemEntity_Touch again', lambda: nttouch())
SDTest.Call(0x0f0fa0, 'tbispi', nti, 'OnlyUser', 1, 1)
check('cItemEntity_Touch OnlyUser', lambda: nttouch())
SDTest.Call(0x0f0fa0, 'tbispi', nti, 'OnlyUser', 0, 1)
SDTest.Call(0x0f1b60, 'tvis', nti, 'R')
check('cItemEntity_Touch type R', lambda: nttouch())
SDTest.Call(0x0f1b60, 'tvis', nti, '')
check('cItemEntity_SetRespawn(1) taker', lambda: (SDTest.Call(0x0f1840, 'tvif', nti, 1.0), ntname(ntpeek(nti + 0x354)),
	ntpeek(nti + 0x34c) == nti + 0x34c))
check('cItemEntity_Touch taken', lambda: nttouch())
ntpoke(nt1 + 0x48, 0)
check('cItemEntity_NetNeedsSend', lambda: (SDTest.Call(0x0f21a0, 'tbif', nti, 1e30)[0], ntfl(0x8476ac, 1)))
ntpoke(nti + 0x22c, 0)
check('cItemEntity_NetNeedsSend far', lambda: (SDTest.Call(0x0f21a0, 'tbif', nti, 1e30)[0], ntpeek(0x8476b4) != 0))
check('name of SDItem out', lambda: SDTest.Call(0x208810, 'tbis', ntworld + 4, 'SDItem')[0])
check('cItemEntity_Delete', lambda: SDTest.Call(0x0f0be0, 'tiii', nti, 1)[0] == nti)

# the doors: ChangeDoorState with an unknown state, AcceleratorOnEnter that stops at a type check
def ntnative(f, params):
	check('Scrap.SetCallFunc(%s)' % f, lambda f=f: Scrap.SetCallFunc(f))
	for p in params:
		if type(p) == type(''):
			Scrap.AddParams(p)
		else:
			Scrap.AddParamf(p)
	check('Scrap.Execute() %s' % f + fmt(tuple(params)), lambda: Scrap.Execute())
if ntobjs:
	ntnative('!ChangeDoorState', [ntobjs[0], 'SDNoState', 1.5])
ntnative('!AcceleratorOnEnter', ['Player0', 'Player0'])
ntnative('!AcceleratorOnEnter', ['Player0', 'SDEnt'])
ntnative('!AcceleratorOnEnter', ['Player0', 'Parked'])
check('Scrap.SetCallFunc() clear', lambda: Scrap.SetCallFunc(''))

# the sphere callback of the door slots (Doors_SphereEntity) on a cPy_Function in calloc memory
ntdoor = (ntpeek(0x847600), ntfl(0x847604, 1), ntfl(0x847658, 3))
ntf = ntcalloc(0x14)
check('door func', lambda: (SDTest.Call(0x192170, 'tii', ntf)[0] == ntf, SDTest.Call(0x192920, 'tbii', ntf, 0x4f0570)[0]))
def ntsphere(name, pos, ang):
	ntpoke(0x847600, 0)
	ntsetfl(0x847604, (ang,))
	ntsetfl(0x847658, pos)
	SDTest.Call(0x192430, 'tvis', ntf, name)
	r = SDTest.Call(0x192650, 'tiii', ntf, 0)[0]
	return (r, map(lambda i: SDTest.Peek(0x847600 + i, '@b'), range(4)))
for name in ('Player0', 'SDEnt', 'SDNoEntity', ''):
	for d in ((100.0, 0.0, 0.0), (-100.0, 0.0, 0.0), (0.0, 0.0, 100.0), (0.0, 50.0, -100.0), (0.0, 0.0, 0.0), (70.0, -9.0, -70.0)):
		for ang in (0.0, 1.0, -2.0, 3.1, 7.0):
			p = (ntpp[0] + d[0], ntpp[1] + d[1], ntpp[2] + d[2])
			check('Doors_SphereEntity' + fmt((name, d, ang)), lambda name=name, p=p, ang=ang: ntsphere(name, p, ang))
SDTest.Call(0x192200, 'tvi', ntf)
ntfree(ntf)
ntpoke(0x847600, ntdoor[0])
ntsetfl(0x847604, ntdoor[1])
ntsetfl(0x847658, ntdoor[2])
check('door globals restored', lambda: (ntpeek(0x847600), ntfl(0x847604, 1), ntfl(0x847658, 3)) == ntdoor)
