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
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def ntfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def ntfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def ntsetfl(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def ntpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
# a vtable slot of the entity as rva
def ntslot(e, off):
	return ntpeek(ntpeek(e) + off) - 0x400000
def ntstable(name, v):
	if name == 'Parked':
		return 'moving'
	return v

ntworld = SDTest.Peek('g_pWorld', 'i')
ntplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', ntworld, 0)[0]
check('entity player', lambda: ntname(ntplayer))

# the world ring by cEntity_Next: the entities of each class (GetClassName through the vtable)
ntclasses = {}
ntobjs = []
ntitems = []
ntcount = 0
nte = SDTest.Call('cWorld_GetFirstEntity', 'tii', ntworld)[0]
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
	nte = SDTest.Call('cEntity_Next', 'tii', nte)[0]
ntkeys = ntclasses.keys()
ntkeys.sort()
check('entity count', lambda: ntcount)
check('entity classes', lambda: map(lambda k: (k, ntclasses[k]), ntkeys))
check('entity Obj', lambda: len(ntobjs))
check('cEntity_NextInList(player)', lambda: ntname(SDTest.Call('cEntity_NextInList', 'tii', ntplayer)[0]))
check('cEntity_GetListName(player)', lambda: SDTest.Call('cEntity_GetListName', 'tsi', ntplayer)[0])

# the base class functions on the player: reads only
for v in ('ActiveCMask', 'Active', 'isLocalLogic', 'isAI', 'Bogus', 'Activ', 'ActiveCMaskX', ''):
	check('cEntity_VarI(player, %s)' % v, lambda v=v: SDTest.Call('cEntity_VarI', 'tbispi', ntplayer, v, -5, 0))
for v in ('Name', 'Type', 'NextInSlot', 'NextInList', 'EntityList', 'Bogus', 'Nam', ''):
	check('cEntity_VarS(player, %s)' % v, lambda v=v: (lambda r: (r[0], r[0] and ntstr(r[1])))(
		SDTest.Call('cEntity_VarS', 'tbispi', ntplayer, v, 0, 0)))
for v in ('Pos', 'Po', 'Bogus', ''):
	check('cEntity_VarV(player, %s)' % v, lambda v=v: SDTest.Call('cEntity_VarV', 'tbisDDDi', ntplayer, v, -1.0, -2.0, -3.0, 0))
for v in ('Life', 'Bogus'):
	check('cEntity_VarF(player, %s)' % v, lambda v=v: SDTest.Call('cEntity_VarF', 'tbisDi', ntplayer, v, -1.0, 0))
check('cEntity_GetPos(player)', lambda: (lambda r: (r[0] != 0, r[1]))(SDTest.Call('cEntity_GetPos', 'tiiF', ntplayer,
	(0.0, 0.0, 0.0))))
check('cEntity_GetAI(player)', lambda: SDTest.Call('cEntity_GetAI', 'tii', ntplayer)[0] != 0)
check('cEntity_IsType(player)', lambda: map(lambda t: SDTest.Call('cEntity_IsType', 'tiis', ntplayer, t)[0],
	('Entity', 'entity', 'WalkChar', '', 'Entityx')))

# plain entities: the second one with a taken name
check('nameCount before', lambda: ntpeek(ntworld + 0x30))
nt1 = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', ntworld, 'SDEnt', 100.0, 200.0, 300.0, 'Entity')[0]
nt2 = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', ntworld, 'SDEnt', -1.5, 2.25, -3.125, 'Entity')[0]
nt3 = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', ntworld, 'SDEntB', 0.0, 0.0, 0.0, 'Entity')[0]
check('cEntity_ctor names', lambda: (ntname(nt1), ntname(nt2), ntname(nt3), ntpeek(ntworld + 0x30)))
def ntfields(e):
	return (ntpeek(e) == symva('cEntity_vftable'), ntfl(e + 0x28, 3), ntints(e + 0x44, 2), SDTest.Peek(e + 0x4c, '@b'),
		ntpeek(e + 0x14) == e, ntpeek(e + 0x24) == e, ntpeek(e + 0x40) == e, ntpeek(e + 0x5c) == e,
		ntpeek(e + 0x6c) == e, ntpeek(e + 0x10) == ntworld, ntpeek(e + 0x20), ntpeek(e + 0x3c), ntpeek(e + 0x58),
		ntpeek(e + 0x60), ntpeek(e + 0x70), ntints(e + 0x74, 2), ntpeek(e + 0x64) == e + 0x64)
for e in (nt1, nt2, nt3):
	check('cEntity_ctor fields %s' % ntname(e), lambda e=e: ntfields(e))
check('world ring order', lambda: (ntpeek(ntworld + 0x10) == nt3 + 8, ntpeek(nt3 + 8) == nt2 + 8,
	ntpeek(nt2 + 8) == nt1 + 8, ntpeek(nt1 + 0xc) == nt2 + 8, ntpeek(nt3 + 0xc) == ntworld + 0x10))
check('cEntity_Next', lambda: (ntname(SDTest.Call('cEntity_Next', 'tii', nt3)[0]), ntname(SDTest.Call('cEntity_Next', 'tii', nt2)[0])))
check('cWorld_GetFirstEntity new', lambda: ntname(SDTest.Call('cWorld_GetFirstEntity', 'tii', ntworld)[0]))
check('Scrap.GetEntity(SDEnt).Name', lambda: Scrap.GetEntity('SDEnt').Name)
check('cEntity_GetClassName', lambda: SDTest.Call('cEntity_GetClassName', 'tsi', nt1)[0])
check('cEntity_IsType', lambda: map(lambda t: SDTest.Call('cEntity_IsType', 'tiis', nt1, t)[0], ('Entity', 'entity', 'Obj', '')))
check('cEntity_GetAI', lambda: SDTest.Call('cEntity_GetAI', 'tii', nt1)[0])
check('cEntity_Update/Render/UpdateOff', lambda: (SDTest.Call('cEntity_Update', 'tvi', nt1), SDTest.Call('cEntity_Render', 'tvii', nt1, 0),
	SDTest.Call('cEntity_UpdateOff', 'tvi', nt1), ntfields(nt1)))
check('cEntity_GetTargetAngle', lambda: SDTest.Call('cEntity_GetTargetAngle', 'tviFfp', nt1, (0.5, 1.0), 100.0, 77))
check('cEntity_SetEmbeddedFX', lambda: SDTest.Call('cEntity_SetEmbeddedFX', 'tvis', nt1, 'SDFX'))
check('cEntity_SetModel', lambda: SDTest.Call('cEntity_SetModel', 'tvisfffi', nt1, 'SDModel', 1.0, 1.0, 1.0, 0))

# positions
check('cEntity_GetPos', lambda: (lambda r: (r[0] != 0, r[1]))(SDTest.Call('cEntity_GetPos', 'tiiF', nt2, (9.0, 9.0, 9.0))))
check('cEntity_SetPos', lambda: (SDTest.Call('cEntity_SetPos', 'tviF', nt2, (7.5, -8.25, 1e10)), ntfl(nt2 + 0x28, 3)))
for v in (('Pos', 1, (11.0, 22.5, -33.75)), ('Pos', 1, (1e40, -1e-50, 0.1)), ('Pos', 0, (0.0, 0.0, 0.0)), ('Pos', 2, (4.0, 5.0, 6.0)),
		('Pos', 256, (-4.0, -5.0, -6.0)), ('pos', 1, (1.0, 1.0, 1.0)), ('', 1, (1.0, 1.0, 1.0)), ('AI_Pos', 0, (0.0, 0.0, 0.0)),
		('LLogicPos', 0, (0.0, 0.0, 0.0))):
	n, s, p = v
	check('cEntity_VarV' + fmt(v), lambda n=n, s=s, p=p: (SDTest.Call('cEntity_VarV', 'tbisDDDi', nt2, n, p[0], p[1], p[2], s),
		ntfl(nt2 + 0x28, 3)))
for v in ('Pos', 'AI_x', 'LLogicx', ''):
	check('cEntity_VarF(%s)' % v, lambda v=v: (SDTest.Call('cEntity_VarF', 'tbisDi', nt2, v, 2.5, 0),
		SDTest.Call('cEntity_VarF', 'tbisDi', nt2, v, 2.5, 1)))

# the aim from nt1 placed above the player: the floor and, with the mask of the player, the player
ntpp = SDTest.Call('cEntity_GetPos', 'tiiF', ntplayer, (0.0, 0.0, 0.0))[1]
ntsetfl(nt1 + 0x28, (ntpp[0], ntpp[1] + 300.0, ntpp[2]))
ntmask = ntpeek(ntplayer + 0x48)
def ntaim(ax, ay, d, m):
	r = SDTest.Call('cEntity_AimWeapon', 'tviffFFpi', nt1, ax, ay, (d,), (0.0, 0.0, 0.0), 77, m)
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
	check('cEntity_VarI' + fmt(v), lambda n=n, s=s, i=i: (SDTest.Call('cEntity_VarI', 'tbispi', nt1, n, i, s),
		SDTest.Peek(nt1 + 0x4c, '@b')))
check('cEntity_SetActive(0)', lambda: (SDTest.Call('cEntity_SetActive', 'tvii', nt1, 0), SDTest.Peek(nt1 + 0x4c, '@b'),
	ntpeek(nt1 + 0x3c)))
check('cEntity_SetActive(1)', lambda: (SDTest.Call('cEntity_SetActive', 'tvii', nt1, 1), SDTest.Peek(nt1 + 0x4c, '@b')))
ntpoke(nt1 + 0x48, 6)
for f, n in ((sym('cEntity_CollisionOff'), 'Off'), (sym('cEntity_CollisionOff'), 'Off'), (sym('cEntity_CollisionOn'), 'On'), (sym('cEntity_CollisionOn'), 'On')):
	check('cEntity_Collision' + n, lambda f=f: (SDTest.Call(f, 'tvi', nt1), ntints(nt1 + 0x44, 2)))
for v in (('ActiveCMask', 0, 9), ('ActiveCMask', 1, 0), ('ActiveCMask', 0, 0), ('ActiveCMask', 1, 0), ('ActiveCMask', 1, 5),
		('ActiveCMask', 1, 5), ('ActiveCMask', 0, 5), ('ActiveCMask', 512, 0), ('isLocalLogic', 0, 0), ('isLocalLogic', 1, 0),
		('isAI', 0, 0), ('isAI', 1, 3), ('AI_isAI', 0, 0), ('LLogicisAI', 0, 0), ('Bogus', 0, 0), ('Bogus', 1, 0), ('', 0, 0)):
	n, s, i = v
	check('cEntity_VarI' + fmt(v), lambda n=n, s=s, i=i: (SDTest.Call('cEntity_VarI', 'tbispi', nt1, n, i, s),
		ntints(nt1 + 0x44, 2)))
ntpoke(nt1 + 0x48, 0)
ntpoke(nt1 + 0x44, 0)

# the entity list ring: SDEntList (its local logic factory gives NULL)
check('Scrap.CreateEntityList(SDEntList)', lambda: Scrap.CreateEntityList('SDEntList'))
ntlist = SDTest.Call('cWorld_GetList', 'tiis', ntworld, 'SDEntList')[0]
def ntcnt():
	return ntpeek(ntpeek(ntlist + 0x40))
def ntgets(e, n):
	r = SDTest.Call('cEntity_VarS', 'tbispi', e, n, 0, 0)
	return (r[0], r[0] and ntstr(r[1]))
# the strings for the set of EntityList (a char * by pointer)
def ntcstr(s):
	a = ntcalloc(32)
	SDTest.Call('Cad_Copy', 'iisi', a, s, len(s) + 1)
	return a
ntsdl = ntcstr('SDEntList')
ntsdn = ntcstr('SDNoList')
ntsde = ntcstr('')
check('list count', lambda: ntcnt())
for e in (nt1, nt2):
	check('cEntity_VarS(EntityList set) %s' % ntname(e), lambda e=e: (SDTest.Call('cEntity_VarS', 'tbispi', e, 'EntityList',
		ntsdl, 1)[0], ntcnt(), ntpeek(e + 0x58) == ntlist, ntpeek(e + 0x60) == ntpeek(ntlist + 0x40), ntints(e + 0x74, 2)))
check('cEntityList_GetFirst', lambda: ntname(SDTest.Call('cEntityList_GetFirst', 'tii', ntlist)[0]))
for e in (nt1, nt2, nt3):
	for v in ('EntityList', 'NextInList', 'NextInSlot', 'Name', 'Type'):
		check('cEntity_VarS(%s) %s' % (v, ntname(e)), lambda e=e, v=v: ntgets(e, v))
	check('cEntity_GetListName %s' % ntname(e), lambda e=e: SDTest.Call('cEntity_GetListName', 'tsi', e)[0])
	check('cEntity_NextInList %s' % ntname(e), lambda e=e: ntname(SDTest.Call('cEntity_NextInList', 'tii', e)[0]))
check('cEntity_SetList(SDNoList)', lambda: (SDTest.Call('cEntity_SetList', 'tbis', nt2, 'SDNoList')[0], ntcnt(),
	ntpeek(nt2 + 0x58), ntpeek(nt2 + 0x60), ntpeek(nt2 + 0x50) == nt2 + 0x50))
check('cEntity_SetList(SDEntList) again', lambda: (SDTest.Call('cEntity_SetList', 'tbis', nt2, 'SDEntList')[0], ntcnt()))
check('cEntity_SetList(SDEntList) twice', lambda: (SDTest.Call('cEntity_SetList', 'tbis', nt2, 'SDEntList')[0], ntcnt()))
check('cEntity_SetList("")', lambda: (SDTest.Call('cEntity_SetList', 'tbis', nt2, '')[0], ntcnt(), ntpeek(nt2 + 0x58)))
check('cEntity_SetList(NULL)', lambda: (SDTest.Call('cEntity_SetList', 'tbii', nt3, 0)[0], ntcnt()))
check('cEntity_VarS(EntityList set SDNoList)', lambda: (SDTest.Call('cEntity_VarS', 'tbispi', nt3, 'EntityList', ntsdn, 1)[0],
	ntcnt()))
check('cEntity_VarS(EntityList set "")', lambda: (SDTest.Call('cEntity_VarS', 'tbispi', nt3, 'EntityList', ntsde, 1)[0],
	ntcnt()))
check('cEntity_SetList(SDEntList) nt3', lambda: (SDTest.Call('cEntity_SetList', 'tbis', nt3, 'SDEntList')[0], ntcnt()))
for v in ('Name', 'Type', 'NextInSlot', 'Bogus'):
	check('cEntity_VarS(%s set)' % v, lambda v=v: SDTest.Call('cEntity_VarS', 'tbispi', nt3, v, ntsde, 1)[0])

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
	check('Nto1Node_LinkByName' + fmt((n == ntn1, name)), lambda n=n, name=name: (SDTest.Call('Nto1Node_LinkByName', 'vis', n, name),
		ntnode(n), SDTest.Call('Nto1Node_TargetName', 'si', n)[0]))
check('Nto1Node_LinkAs', lambda: (SDTest.Call('Nto1Node_LinkAs', 'vii', ntn2, ntn1), ntnode(ntn2), SDTest.Call('Nto1Node_TargetName', 'si', ntn2)[0],
	ntpeek(ntn2 + 4) == ntplayer + 0x64))
check('player links', lambda: ntpeek(ntplayer + 0x64) == ntn2)
check('Nto1Node_LinkByName unlink', lambda: (SDTest.Call('Nto1Node_LinkByName', 'vis', ntn1, 'SDNoEntity'), ntnode(ntn1)))
check('Nto1Node_LinkAs unlinked', lambda: (SDTest.Call('Nto1Node_LinkAs', 'vii', ntn2, ntn1), ntnode(ntn2),
	SDTest.Call('Nto1Node_TargetName', 'si', ntn2)[0]))
check('Nto1Node_LinkAs into nt3', lambda: (SDTest.Call('Nto1Node_LinkByName', 'vis', ntn1, 'SDEntB'),
	SDTest.Call('Nto1Node_LinkAs', 'vii', ntn2, ntn1), ntnode(ntn2), ntpeek(nt3 + 0x64) == ntn2, ntpeek(nt3 + 0x68) == ntn1))
# the destructor of nt3 unlinks both nodes (Nto1Head_UnlinkAll of its links)
check('cEntity_Del(nt2)', lambda: (SDTest.Call('cEntity_Del', 'tbi', nt2)[0], ntpeek(nt2 + 0x20) == ntworld,
	ntpeek(ntworld + 0x20) == nt2 + 0x18))
check('cEntity_Del(nt2) again', lambda: SDTest.Call('cEntity_Del', 'tbi', nt2)[0])
check('name of nt3 out', lambda: SDTest.Call('PtrTable_Delete@208810', 'tbis', ntworld + 4, 'SDEntB')[0])
check('cEntity_Delete(nt3)', lambda: (SDTest.Call('cEntity_Delete', 'tiii', nt3, 1)[0] == nt3, ntcnt(), ntnode(ntn1), ntnode(ntn2),
	ntname(SDTest.Call('cEntity_Next', 'tii', nt2)[0]) == ntname(nt1)))
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
	return (ntname(e), ntpeek(e) == symva('cItemEntity_vftable'), ntfl(e + 0x228, 1), ntpeek(e + 0x22c), ntstr(e + 0x230), ntfl(e + 0x2b0, 4),
		ntpeek(e + 0x2c0) & 0xffffff, ntfl(e + 0x2c4, 1), ntstr(e + 0x2c8), SDTest.Peek(e + 0x348, '@b'),
		ntname(ntpeek(e + 0x354)), ntpeek(e + 0x35c) & 0xffff, ntfl(e + 0x360, 1), SDTest.Peek(e + 0x364, '@b'),
		ntints(e + 0x368, 2), ntpeek(e + 0x48), ntfl(e + 0xd4, 3))
def ntitem(e, tag):
	check('cItemEntity fields %s' % tag, lambda e=e: ntitemfields(e))
	for v in ntivi:
		check('cItemEntity_VarI(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call('cItemEntity_VarI', 'tbispi', e, v, -5, 0))
	for v in ntivf:
		check('cItemEntity_VarF(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call('cItemEntity_VarF', 'tbisDi', e, v, -5.0, 0))
	for v in ntivs:
		check('cItemEntity_VarS(%s, %s)' % (tag, v), lambda e=e, v=v: (lambda r: (r[0], r[0] and ntstr(r[1])))(
			SDTest.Call('cItemEntity_VarS', 'tbispi', e, v, 0, 0)))
	for v in ntivv:
		check('cItemEntity_VarV(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call('cItemEntity_VarV', 'tbisDDDi', e, v, -1.0, -2.0,
			-3.0, 0))
	check('cItemEntity_IsType(%s)' % tag, lambda e=e: map(lambda t, e=e: SDTest.Call('cItemEntity_IsType', 'tbis', e, t)[0],
		('item', 'Obj', 'Entity', 'Car', 'itemx', '')))
	check('cItemEntity getters %s' % tag, lambda e=e: (SDTest.Call('cItemEntity_GetClassName', 'tsi', e)[0], SDTest.Call('cItemEntity_GetRadius', 'tfi', e)[0],
		SDTest.Call('cItemEntity_GetBackTime', 'tfi', e)[0], SDTest.Call('cItemEntity_GetOwner', 'tsi', e)[0],
		(lambda r: (r[0] != 0, r[1]))(SDTest.Call('cItemEntity_GetOrigin', 'tiiF', e, (0.0, 0.0, 0.0)))))
check('items', lambda: len(ntitems))
for i in range(len(ntitems)):
	if i < 6:
		ntitem(ntitems[i], 'level %d' % i)
nti = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', ntworld, 'SDItem', ntpp[0] + 500.0, ntpp[1], ntpp[2], 'item')[0]
check('cItemEntity_ctor', lambda: ntpeek(nti + 0x34c) == nti + 0x34c and ntpeek(nti + 0x358) == nti)
ntitem(nti, 'SDItem')
ntcalls = []
def ntontake(*args):
	ntcalls.append(args)
def ntvarsi(n, s):
	a = ntcstr(s)
	r = SDTest.Call('cItemEntity_VarS', 'tbispi', nti, n, a, 1)[0]
	ntfree(a)
	return r
for n, s in (('TakeSound', 'SDSound'), ('TakeSound', ''), ('Owner', 'SDNoOwner'), ('TgTypeMarker', 'rBgnk'),
		('TgTypeMarker', 'RX'), ('TgTypeMarker', ''), ('OnTake', '__main__.ntontake'), ('OnReset', '__main__.ntontake'),
		('Name', 'x'), ('Bogus', 'x')):
	check('cItemEntity_VarS(SDItem, %s set %s)' % (n, s), lambda n=n, s=s: (ntvarsi(n, s), ntitemfields(nti)))
for v in (('FadeInEffect', 2), ('FadeOutEffect', 0), ('Blink', 7), ('AutoFace', 1), ('AppearsIfFree', 1), ('OnlyUser', 0),
		('Blink', 0), ('AutoFace', 0), ('Bogus', 1), ('Blink', 256)):
	n, i = v
	check('cItemEntity_VarI(SDItem, %s set %d)' % v, lambda n=n, i=i: (SDTest.Call('cItemEntity_VarI', 'tbispi', nti, n, i, 1)[0],
		SDTest.Call('cItemEntity_VarI', 'tbispi', nti, n, -5, 0)))
for v in (('TakeSoundVol', 0.75), ('Radius', 250.5), ('rTime', 5.0), ('rTime', 0.0), ('BackTime', 2.0), ('BackTime', 0.0),
		('rTime', -1.0), ('Bogus', 1.0)):
	n, f = v
	check('cItemEntity_VarF(SDItem, %s set %s)' % (n, fmt(f)), lambda n=n, f=f: (SDTest.Call('cItemEntity_VarF', 'tbisDi', nti, n, f, 1)[0],
		ntitemfields(nti)))
check('cItemEntity_VarV(SDItem, Origin set)', lambda: (SDTest.Call('cItemEntity_VarV', 'tbisDDDi', nti, 'Origin', 1.5, -2.5, 1e10, 1)[0],
	ntfl(nti + 0x2b0, 3)))
check('cItemEntity_VarV(SDItem, Vel set)', lambda: (SDTest.Call('cItemEntity_VarV', 'tbisDDDi', nti, 'Vel', 10.0, 0.0, -5.0, 1)[0],
	ntfl(nti + 0xd4, 3), SDTest.Call('cItemEntity_VarV', 'tbisDDDi', nti, 'Vel', 0.0, 0.0, 0.0, 0)))
check('cItemEntity_SetOrigin', lambda: (SDTest.Call('cItemEntity_SetOrigin', 'tviF', nti, (ntpp[0] + 600.0, ntpp[1], ntpp[2])),
	ntfl(nti + 0x2b0, 3)))
check('cItemEntity_SetPos', lambda: (SDTest.Call('cItemEntity_SetPos', 'tviF', nti, (ntpp[0] + 700.0, ntpp[1], ntpp[2])),
	ntpeek(nti + 0x22c), ntfl(nti + 0x28, 3)))
for t in (3.0, 0.0, -2.0, 1.0):
	check('cItemEntity_SetBackTime(%s)' % fmt(t), lambda t=t: (SDTest.Call('cItemEntity_SetBackTime', 'tvif', nti, t), ntfl(nti + 0x2bc, 1),
		ntfl(nti + 0x28, 3), len(ntcalls)))
for t in (3.0, 4.0, 0.0, -1.0, 2.0, 0.0):
	check('cItemEntity_SetRespawn(%s)' % fmt(t), lambda t=t: (SDTest.Call('cItemEntity_SetRespawn', 'tvif', nti, t), ntitemfields(nti)))
for s in ('RBGNK', 'rbgnk', 'Rx?', '', 'kN', 'A'):
	check('cItemEntity_SetTypeMarker(%s)' % s, lambda s=s: (SDTest.Call('cItemEntity_SetTypeMarker', 'tvis', nti, s), ntpeek(nti + 0x35c) & 0xffff))
for s in ('', 'Player0', 'SDNoOwner'):
	check('cItemEntity_SetOwner(%s)' % s, lambda s=s: (SDTest.Call('cItemEntity_SetOwner', 'tvis', nti, s), ntitemfields(nti)))
SDTest.Call('cItemEntity_SetRespawn', 'tvif', nti, 0.0)
# the sphere: the other entity nt1 (its touch slot does nothing) or none
ntip = SDTest.Call('cEntity_GetPos', 'tiiF', nti, (0.0, 0.0, 0.0))[1]
for d in ((0.0, 0.0, 0.0), (100.0, 0.0, 0.0), (0.5, 0.25, 0.0), (300.0, 0.0, 0.0), (0.0, -260.0, 0.0), (1000.0, 1000.0, 0.0)):
	c = (ntip[0] + d[0], ntip[1] + d[1], ntip[2] + d[2])
	for r in (0.0, 10.0, 100.0):
		for o in (0, nt1):
			check('cItemEntity_TestSphere' + fmt((d, r, o != 0)), lambda c=c, r=r, o=o: SDTest.Call('cItemEntity_TestSphere', 'tbiiFfi', nti, o,
				c, r, 0)[0])
# the touch of nt1: its mask, OnlyUser and the type marker decide; OnTake gets (item, taker)
def nttouch():
	SDTest.Call('cItemEntity_Touch', 'tviiii', nti, nt1, 0, 0)
	return (map(lambda c: c, ntcalls), ntname(ntpeek(nti + 0x354)))
check('cItemEntity_Touch mask 0', lambda: nttouch())
ntpoke(nt1 + 0x48, 0x11)
check('cItemEntity_Touch', lambda: nttouch())
check('cItemEntity_Touch again', lambda: nttouch())
SDTest.Call('cItemEntity_VarI', 'tbispi', nti, 'OnlyUser', 1, 1)
check('cItemEntity_Touch OnlyUser', lambda: nttouch())
SDTest.Call('cItemEntity_VarI', 'tbispi', nti, 'OnlyUser', 0, 1)
SDTest.Call('cItemEntity_SetTypeMarker', 'tvis', nti, 'R')
check('cItemEntity_Touch type R', lambda: nttouch())
SDTest.Call('cItemEntity_SetTypeMarker', 'tvis', nti, '')
check('cItemEntity_SetRespawn(1) taker', lambda: (SDTest.Call('cItemEntity_SetRespawn', 'tvif', nti, 1.0), ntname(ntpeek(nti + 0x354)),
	ntpeek(nti + 0x34c) == nti + 0x34c))
check('cItemEntity_Touch taken', lambda: nttouch())
ntpoke(nt1 + 0x48, 0)
check('cItemEntity_NetNeedsSend', lambda: (SDTest.Call('cItemEntity_NetNeedsSend', 'tbif', nti, 1e30)[0], ntfl(symva('g_ItemSyncDist'), 1)))
ntpoke(nti + 0x22c, 0)
check('cItemEntity_NetNeedsSend far', lambda: (SDTest.Call('cItemEntity_NetNeedsSend', 'tbif', nti, 1e30)[0], ntpeek(symva('g_ItemSyncCount')) != 0))
check('name of SDItem out', lambda: SDTest.Call('PtrTable_Delete@208810', 'tbis', ntworld + 4, 'SDItem')[0])
check('cItemEntity_Delete', lambda: SDTest.Call('cItemEntity_Delete', 'tiii', nti, 1)[0] == nti)

# cMissileEntity: SDMissile made by cWorld_CreateEntity without a model; the variables, the setters, the flight math
# (Wobble after srand), the touch list (restored), then off and deleted. Not called: Hit with too few bees (it
# explodes), Update and LifeTick (they use the frame time).
ntm = SDTest.Call('cWorld_CreateEntity', 'tiisfffs', ntworld, 'SDMissile', ntpp[0], ntpp[1] + 400.0, ntpp[2], 'Missile')[0]
def ntmfields(e):
	return (ntname(e), ntpeek(e) == symva('cMissileEntity_vftable'), ntfl(e + 0xac, 3), ntfl(e + 0xd4, 6), ntpeek(e + 0x48),
		SDTest.Peek(e + 0x3e8, '@b'), ntfl(e + 0x3ec, 4), ntstr(e + 0x3fc), ntstr(e + 0x47c), ntstr(e + 0x4fc),
		ntints(e + 0x57c, 2), map(lambda o, e=e: SDTest.Peek(e + o, '@b'), (0x584, 0x585, 0x58c, 0x7bc, 0x7bd)),
		ntpeek(e + 0x588), ntpeek(e + 0x590), ntfl(e + 0x594, 7), ntpeek(e + 0x5b0),
		map(lambda i, e=e: ntstr(e + 0x5b4 + 0x80 * i), range(4)), ntints(e + 0x7b4, 2), ntfl(e + 0x28c, 1))
check('cMissileEntity_ctor', lambda: ntmfields(ntm))
for v in ('HideMain', 'nSwarm', 'NetnSwarm', 'FallOutAtmosphere', 'HiddenSwarm', 'NoWorldHit', 'AddToSwarm', 'Bogus', ''):
	check('cMissileEntity_VarI(%s)' % v, lambda v=v: SDTest.Call('cMissileEntity_VarI', 'tbispi', ntm, v, -5, 0))
for v in ('SwDthTime', 'RadiusGrow', 'TopRadius', 'Radius', 'SwarmSpeed', 'TurnSpeed', 'LifeTime', 'Speed', 'Bogus', ''):
	check('cMissileEntity_VarF(%s)' % v, lambda v=v: SDTest.Call('cMissileEntity_VarF', 'tbisDi', ntm, v, -5.0, 0))
for v in ('OnBeeDeath', 'OnHit', 'HitType', 'Type', 'Parent', 'Target', 'Name', 'Bogus', ''):
	check('cMissileEntity_VarS(%s)' % v, lambda v=v: (lambda r: (r[0], r[0] and ntstr(r[1])))(
		SDTest.Call('cMissileEntity_VarS', 'tbispi', ntm, v, 0, 0)))
for v in ('Pos', 'Vel', 'Bogus', ''):
	check('cMissileEntity_VarV(%s)' % v, lambda v=v: SDTest.Call('cMissileEntity_VarV', 'tbisDDDi', ntm, v, -1.0, -2.0, -3.0, 0))
check('cMissileEntity_IsType', lambda: map(lambda t: SDTest.Call('cMissileEntity_IsType', 'tbis', ntm, t)[0],
	('Missile', 'WithLife', 'Entity', 'Car', 'missile', '')))
check('cMissileEntity getters', lambda: (SDTest.Call('cMissileEntity_GetClassName', 'tsi', ntm)[0], SDTest.Call('cMissileEntity_GetSwarmCount', 'tii', ntm)[0]))
for v in (('SwDthTime', 2.5, 1), ('SwDthTime', 3.5, 0), ('RadiusGrow', 7.0, 1), ('TopRadius', 1500.0, 1), ('Radius', 50.0, 1),
		('SwarmSpeed', 20000.0, 1), ('TurnSpeed', 90.0, 1), ('TurnSpeed', -45.5, 1), ('LifeTime', 4.0, 1), ('Speed', 1000.0, 1),
		('Speed', 0.0, 256), ('Bogus', 1.0, 1)):
	n, f, st = v
	check('cMissileEntity_VarF set' + fmt(v), lambda n=n, f=f, st=st: (SDTest.Call('cMissileEntity_VarF', 'tbisDi', ntm, n, f, st),
		ntmfields(ntm)))
for v in (('HideMain', 3), ('FallOutAtmosphere', 2), ('NoWorldHit', 1), ('HiddenSwarm', 1), ('HiddenSwarm', 0), ('nSwarm', 0),
		('NoWorldHit', 0), ('Bogus', 1)):
	n, i = v
	check('cMissileEntity_VarI set' + fmt(v), lambda n=n, i=i: (SDTest.Call('cMissileEntity_VarI', 'tbispi', ntm, n, i, 1)[0],
		ntmfields(ntm)))
def ntmvars(n, s):
	a = ntcstr(s)
	r = SDTest.Call('cMissileEntity_VarS', 'tbispi', ntm, n, a, 1)[0]
	ntfree(a)
	return r
for n, s in (('HitType', 'inout'), ('HitType', 'Inside'), ('HitType', 'None'), ('HitType', 'Bogus'), ('HitType', 'Hit'),
		('Type', 'cloud'), ('Type', 'Dumb'), ('Type', 'Fall'), ('Type', 'x'), ('Type', 'Linear'), ('Parent', 'Player0'),
		('Target', 'SDEnt'), ('OnHit', '__main__.ntontake'), ('OnBeeDeath', '__main__.ntontake'), ('Bogus', 'x')):
	check('cMissileEntity_VarS set(%s, %s)' % (n, s), lambda n=n, s=s: (ntmvars(n, s), ntmfields(ntm),
		(lambda r: (r[0], r[0] and ntstr(r[1])))(SDTest.Call('cMissileEntity_VarS', 'tbispi', ntm, n, 0, 0))))
for f, sig, a in ((sym('cMissileEntity_SetLifeTime'), 'tvif', 2.0), (sym('cMissileEntity_SetType'), 'tvii', 3), (sym('cMissileEntity_SetCount'), 'tvii', 0), (sym('cMissileEntity_SetNoWorldHit'), 'tvii', 1),
		(sym('cMissileEntity_SetParent'), 'tvis', 'SDParent'), (sym('cMissileEntity_SetRadius'), 'tvif', 60.0), (sym('cMissileEntity_SetRadiusGrow'), 'tvif', 8.0), (sym('cMissileEntity_SetSpeed'), 'tvif', 500.0),
		(sym('cMissileEntity_SetSwDthTime'), 'tvif', 1.25), (sym('cMissileEntity_SetFlag584'), 'tvii', 0), (sym('cMissileEntity_SetFlag585'), 'tvii', 1), (sym('cMissileEntity_SetSwarmSpeed'), 'tvif', 1234.5),
		(sym('cMissileEntity_SetTarget'), 'tvis', 'SDTarget'), (sym('cMissileEntity_SetTarget'), 'tvii', 0), (sym('cMissileEntity_SetTopRadius'), 'tvif', 900.0), (sym('cMissileEntity_SetTurnSpeed'), 'tvif', 0.75),
		(sym('cMissileEntity_SetType'), 'tvii', 0), (sym('cMissileEntity_SetHiddenSwarm'), 'tvii', 1), (sym('cMissileEntity_SetHiddenSwarm'), 'tvii', 0), (sym('cMissileEntity_SetHideMain'), 'tvii', 1), (sym('cMissileEntity_SetHideMain'), 'tvii', 0)):
	check('cMissileEntity setter %x' % f + fmt(a), lambda f=f, sig=sig, a=a: (SDTest.Call(f, sig, ntm, a), ntmfields(ntm)))
# the flight math
for a in ((0.0, 0.0, 0.0), (0.5, 1.0, 0.0), (-1.2, 3.0, 2.0), (7.0, -7.0, 12.0)):
	check('cMissileEntity_SetAng' + fmt(a), lambda a=a: (SDTest.Call('cMissileEntity_SetAng', 'tviF', ntm, a), ntfl(ntm + 0xac, 3)))
	check('cMissileEntity_UpdateVel' + fmt(a), lambda: (SDTest.Call('cMissileEntity_UpdateVel', 'tvi', ntm), ntfl(ntm + 0xd4, 3)))
for v in ((100.0, 0.0, 0.0), (0.0, -50.0, 0.0), (3.0, 4.0, -12.0), (0.0, 0.0, 0.0), (-1e5, 2e5, 3.5)):
	check('cMissileEntity_SetVel' + fmt(v), lambda v=v: (SDTest.Call('cMissileEntity_SetVel', 'tviF', ntm, v), ntmfields(ntm)))
ntsetfl(ntm + 0xe0, (0.25, -0.5, 1.0))
for dt in (0.1, 0.5, 2.0):
	check('cMissileEntity_RotStep(%s)' % fmt(dt), lambda dt=dt: (SDTest.Call('cMissileEntity_RotStep', 'tvif', ntm, dt), ntmfields(ntm)))
	check('cMissileEntity_Fall(%s)' % fmt(dt), lambda dt=dt: (SDTest.Call('cMissileEntity_Fall', 'tvif', ntm, dt), ntmfields(ntm)))
ntsetfl(ntm + 0xac, (0.0, 0.0, -0.5))
check('cMissileEntity_Fall z < 0', lambda: (SDTest.Call('cMissileEntity_Fall', 'tvif', ntm, 0.25), ntfl(ntm + 0xac, 3)))
SDTest.Call('crt_srand', 'vi', 7)
for k in (1.0, 0.5, 4.0, -2.0):
	check('cMissileEntity_Wobble(%s)' % fmt(k), lambda k=k: (SDTest.Call('cMissileEntity_Wobble', 'tvif', ntm, k), ntmfields(ntm)))
check('cMissileEntity_ScaleWeapon', lambda: (SDTest.Call('cMissileEntity_ScaleWeapon', 'tvii', ntm, 0), ntmfields(ntm)))
# the touch list of the missiles (a global): the player goes in, the plain entity nt1 does not
ntmhits = map(lambda i: ntstr(0x8476b8 + 0x80 * i), range(4))
check('missile hits before', lambda: ntmhits)
def ntmtouch(o):
	SDTest.Call('cMissileEntity_Touch', 'tviiii', ntm, o, 0, 0)
	return map(lambda i: ntstr(0x8476b8 + 0x80 * i), range(4))
check('cMissileEntity_Touch(0)', lambda: ntmtouch(0))
check('cMissileEntity_Touch(nt1)', lambda: ntmtouch(nt1))
check('cMissileEntity_Touch(player)', lambda: ntmtouch(ntplayer))
SDTest.Call('cMissileEntity_SetParent', 'tvis', ntm, 'Player0')
check('cMissileEntity_Touch(player) parent', lambda: ntmtouch(ntplayer))
for i in range(4):
	if ntmhits[i]:
		SDTest.Call('Cad_Copy', 'iisi', 0x8476b8 + 0x80 * i, ntmhits[i], len(ntmhits[i]) + 1)
	else:
		ntsetfl(0x8476b8 + 0x80 * i, (0.0,))
check('missile hits restored', lambda: map(lambda i: ntstr(0x8476b8 + 0x80 * i), range(4)))
# InOut: leaving it calls OnHit for the names the missile went through
ntcalls = []
SDTest.Call('Cad_Copy', 'iisi', ntm + 0x5b4, 'SDHitA', 7)
SDTest.Call('Cad_Copy', 'iisi', ntm + 0x6b4, 'SDHitC', 7)
check('cMissileEntity_SetHitType(InOut)', lambda: (SDTest.Call('cMissileEntity_SetHitType', 'tvii', ntm, 2), ntpeek(ntm + 0x7b4), len(ntcalls)))
check('cMissileEntity_SetHitType(InOut) again', lambda: (SDTest.Call('cMissileEntity_SetHitType', 'tvii', ntm, 2), len(ntcalls)))
check('cMissileEntity_SetHitType(None)', lambda: (SDTest.Call('cMissileEntity_SetHitType', 'tvii', ntm, 0), ntpeek(ntm + 0x7b4), ntcalls))
# hidden swarm bees are only counted
SDTest.Call('cMissileEntity_SetCount', 'tvii', ntm, 5)
SDTest.Poke(ntm + 0x58c - 0x400000, 1)
check('cMissileEntity_Hit(2) hidden', lambda: (SDTest.Call('cMissileEntity_Hit', 'tbiii', ntm, 2, 0)[0], ntmfields(ntm)))
check('cMissileEntity_Hit(3) hidden', lambda: (SDTest.Call('cMissileEntity_Hit', 'tbiii', ntm, 3, 0)[0], ntmfields(ntm)))
check('cMissileEntity_SetSwarm(0) hidden', lambda: (SDTest.Call('cMissileEntity_SetSwarm', 'tvii', ntm, 0), ntmfields(ntm)))
check('cMissileEntity_OnDeath', lambda: (SDTest.Call('cMissileEntity_OnDeath', 'tvi', ntm), ntstr(ntm + 0x47c)))
check('cMissileEntity_SetActive(0)', lambda: (SDTest.Call('cMissileEntity_SetActive', 'tvii', ntm, 0), SDTest.Peek(ntm + 0x4c, '@b')))
check('name of SDMissile out', lambda: SDTest.Call('PtrTable_Delete@208810', 'tbis', ntworld + 4, 'SDMissile')[0])
check('cMissileEntity_Delete', lambda: SDTest.Call('cMissileEntity_Delete', 'tiii', ntm, 1)[0] == ntm)

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
ntdoor = (ntpeek(symva('g_DoorFront')), ntfl(symva('g_DoorAngY'), 1), ntfl(symva('g_DoorPos'), 3))
ntf = ntcalloc(0x14)
check('door func', lambda: (SDTest.Call('cPy_Function_ctor', 'tii', ntf)[0] == ntf, SDTest.Call('cPy_Function_SetNative', 'tbii', ntf, symva('Doors_SphereEntity'))[0]))
def ntsphere(name, pos, ang):
	ntpoke(symva('g_DoorFront'), 0)
	ntsetfl(symva('g_DoorAngY'), (ang,))
	ntsetfl(symva('g_DoorPos'), pos)
	SDTest.Call('cPy_Function_AddParamS', 'tvis', ntf, name)
	r = SDTest.Call('cPy_Function_Execute', 'tiii', ntf, 0)[0]
	return (r, map(lambda i: SDTest.Peek(symva('g_DoorFront') + i, '@b'), range(4)))
for name in ('Player0', 'SDEnt', 'SDNoEntity', ''):
	for d in ((100.0, 0.0, 0.0), (-100.0, 0.0, 0.0), (0.0, 0.0, 100.0), (0.0, 50.0, -100.0), (0.0, 0.0, 0.0), (70.0, -9.0, -70.0)):
		for ang in (0.0, 1.0, -2.0, 3.1, 7.0):
			p = (ntpp[0] + d[0], ntpp[1] + d[1], ntpp[2] + d[2])
			check('Doors_SphereEntity' + fmt((name, d, ang)), lambda name=name, p=p, ang=ang: ntsphere(name, p, ang))
SDTest.Call('cPy_Function_dtor', 'tvi', ntf)
ntfree(ntf)
ntpoke(symva('g_DoorFront'), ntdoor[0])
ntsetfl(symva('g_DoorAngY'), ntdoor[1])
ntsetfl(symva('g_DoorPos'), ntdoor[2])
check('door globals restored', lambda: (ntpeek(symva('g_DoorFront')), ntfl(symva('g_DoorAngY'), 1), ntfl(symva('g_DoorPos'), 3)) == ntdoor)
