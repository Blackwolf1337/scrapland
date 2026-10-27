# game Entities/CamControl.c, CamPathEntity.c, FollowCamEntity.c and Entities/Char/Type/Banker.c ... Maintenance.c
# (execfile'd by sdlevel.py, Python 1.5.2). The cameras on entities made by cWorld_CreateEntity far above the player:
# the constructors, the script variables, the camera of the path between its points (fades, quake with rand seeded,
# the end with OnEnd), the follow camera on an "Obj" made the same way (target, angles of the slot and the lerp, the
# shake, the free camera of view slot 3 without input), and the deleting destructors. The frame time and the speed
# of the world timer are set by hand and restored. The type natives through Scrap.SetCallFunc('!name'): the early
# returns with a missing character, and on Player0 the ones that only touch what is saved and restored here (sound
# 0, the weapon energy, the life, the world money and timer, the "MeEnt" globals) or the actions of the player
# (their names printed). The steal, the victim test and the gear hit on fake characters in calloc memory, the flash
# far from everyone and at the player. Not called: the natives of characters FZ does not have (CrazyGambler,
# Maintenance), the FX natives (their starts CHECK model nodes of the type), GearShoot (a missile with its effect),
# the actions that turn the view of the player (Betty, D-Tritus press and end), the sphere callbacks with other
# characters (there are none).
def ct1i(a):
	return SDTest.Peek(a, '@i')
def ct1b(a):
	return SDTest.Peek(a, '@b')
def ct1poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def ct1fl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def ct1set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def ct1ints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def ct1bytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def ct1z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def ct1free(p):
	SDTest.Call(0x133550, 'ii', p)
def ct1str(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def ct1cstr(s):
	a = ct1z(len(s) + 4)
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
	return a
def ct1name(e):
	if e:
		return SDTest.Peek(e + 4, '@p')
	return None
# a vtable slot of the entity as rva
def ct1slot(e, off):
	return ct1i(ct1i(e) + off) - 0x400000
def ct1srand(s):
	SDTest.Call(0x323c50, 'vi', s)
# one byte, the three after it kept
def ct1pokeb(a, v):
	ct1poke(a, (ct1i(a) & ~0xff) | v)
def ct1find(name):
	slot = SDTest.Call(0x131f70, 'tiis', ct1world + 4, name)[0]
	node = SDTest.Call(0x131e50, 'tiisi', ct1world + 4, name, slot)[0]
	if node:
		return ct1i(node)
	return 0
def ct1native(name, params):
	ok = Scrap.SetCallFunc('!' + name)
	for p in params:
		if type(p) == type(''):
			Scrap.AddParams(p)
		elif type(p) == type(0):
			Scrap.AddParami(p)
		else:
			Scrap.AddParamf(p)
	Scrap.Execute()
	return ok
# the deleting destructor after the name left the table, as the world does it
def ct1delete(e):
	SDTest.Call(0x208810, 'tbis', ct1world + 4, ct1name(e))
	return SDTest.Call(ct1slot(e, 0xc), 'tiii', e, 1)[0] == e
ct1calls = []
def ct1cb(*args):
	ct1calls.append(args)
def ct1takecalls():
	r = tuple(ct1calls)
	del ct1calls[:]
	return r

ct1world = SDTest.Peek(0x467450, 'i')
ct1player = SDTest.Call(0x0c1430, 'tiii', ct1world, 0)[0]
check('CT1 cWorld_GetUsrEntity', lambda: (ct1player != 0, tuple(map(lambda i: SDTest.Call(0x0c1430, 'tiii', ct1world,
	i)[0] == ct1player, (0, 1, 2, 3)))))
ct1pp = SDTest.Call(0x0ef330, 'tiiF', ct1player, (0.0, 0.0, 0.0))[1]
ct1x = ct1pp[0]
ct1y = ct1pp[1] + 3000.0
ct1z_ = ct1pp[2]
ct1wt = ct1world + 0x2c8
ct1savedwt = ct1ints(ct1wt, (0, 4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24))
def ct1restoretimer():
	ct1k = 0
	while ct1k < 10:
		ct1poke(ct1wt + 4 * ct1k, ct1savedwt[ct1k])
		ct1k = ct1k + 1
ct1set(ct1wt + 8, (100.0, 0.02))
ct1set(ct1wt + 0x14, (1.0,))
ct1fn = ct1cstr('__main__.ct1cb')
ct1none = ct1cstr('SDNone')

# ---- the small inline virtuals of cEntity ----
check('CT1 cEntity_TestLine', lambda: SDTest.Call(0x0bd730, 'tbiii', ct1player, 0, 0)[0])
check('CT1 cEntity_TestSphere', lambda: SDTest.Call(0x0bd8a0, 'tbiiifi', ct1player, 0, 0, 1.0, 0)[0])
check('CT1 cEntity_Vf11', lambda: SDTest.Call(0x0bd430, 'tviiii', ct1player, 1, 2, 3)[0])
check('CT1 cEntity_Vf20', lambda: SDTest.Call(0x0bd440, 'tvi', ct1player)[0])
check('CT1 cEntity_Vf21', lambda: SDTest.Call(0x0bd630, 'tiii', ct1player, 5)[0])

# ---- cCamControl ("Cam") ----
def ct1camf(c):
	return (ct1fl(c + 0x7c, 13), ct1i(c + 0xb8), ct1str(c + 0xbc))
ct1cam = SDTest.Call(0x208360, 'tiisfffs', ct1world, 'SDCt1Cam', ct1x, ct1y, ct1z_, 'Cam')[0]
check('CT1 Cam', lambda: ct1cam != 0)
if ct1cam:
	check('CT1 cCamEntity_ctor', lambda: (ct1i(ct1cam) - 0x400000, ct1name(ct1cam), ct1camf(ct1cam)))
	check('CT1 cCamControl_GetClassName', lambda: SDTest.Call(0x0bd640, 'tsi', ct1cam)[0])
	check('CT1 cCamControl_IsType', lambda: tuple(map(lambda t: SDTest.Call(0x0bd6e0, 'tbis', ct1cam, t)[0],
		('Cam', 'cam', 'Entity', 'FCam', 'CamPath', ''))))
	for ct1v in (('Fov', 45.0), ('clipNear', 5.0), ('clipFar', 5000.0), ('Pos', 1.0), ('Bogus', 1.0), ('', 1.0)):
		check('CT1 cCamControl_VarF(%s)' % ct1v[0], lambda v=ct1v: (SDTest.Call(0x0bd450, 'tbisDi', ct1cam, v[0], -7.0,
			0), SDTest.Call(0x0bd450, 'tbisDi', ct1cam, v[0], v[1], 1)[0], SDTest.Call(0x0bd450, 'tbisDi', ct1cam,
			v[0], -7.0, 0), ct1camf(ct1cam)))
	for ct1v in (('UsrControl', 0), ('UsrControl', 1), ('Bogus', 0), ('', 0)):
		check('CT1 cCamControl_VarI(%s, %d)' % ct1v, lambda v=ct1v: SDTest.Call(0x0bd660, 'tbispi', ct1cam, v[0], -5,
			v[1]))
	for ct1v in (('camPos', (ct1x + 10.0, ct1y + 20.0, ct1z_ - 30.0)), ('camAng', (10.0, -20.0, 30.0)),
			('camAng', (400.0, -720.0, 0.5)), ('Bogus', (1.0, 2.0, 3.0)), ('', (1.0, 2.0, 3.0))):
		check('CT1 cCamControl_VarV(%s)' % ct1v[0], lambda v=ct1v: (SDTest.Call(0x0bd940, 'tbisDDDi', ct1cam, v[0],
			v[1][0], v[1][1], v[1][2], 1)[0], SDTest.Call(0x0bd940, 'tbisDDDi', ct1cam, v[0], 0.0, 0.0, 0.0, 0),
			ct1camf(ct1cam)))
	check('CT1 cCamControl_GetCamPos', lambda: SDTest.Call(0x0bd600, 'tvippp', ct1cam, 0, 0, 0)[1:])
	check('CT1 cCamControl_SetCamPos', lambda: (SDTest.Call(0x0bd7f0, 'tvifff', ct1cam, ct1x, ct1y, ct1z_),
		ct1camf(ct1cam)))
	check('CT1 cCamControl_NopF', lambda: (SDTest.Call(0x0bd410, 'tvif', ct1cam, 1.0), SDTest.Call(0x0bd420, 'tvif',
		ct1cam, 1.0), SDTest.Call(0x0bd650, 'tvif', ct1cam, 1.0), ct1camf(ct1cam)))
	# an Obj with a model is taken only when the model has a camera node (else a CHECK of the node)
	ct1pmodel = SDTest.Call(ct1slot(ct1player, 0x98), 'tii', ct1player)[0]
	ct1camnode = SDTest.Call(0x20f7d0, 'tiis', ct1pmodel, 'D_Cam-Composite')[0] >= 0 or \
		SDTest.Call(0x20f7d0, 'tiis', ct1pmodel, 'Camera_01')[0] >= 0
	check('CT1 player camera node', lambda: ct1camnode)
	ct1targets = ['SDNone', '', 'SDCt1Cam']
	if ct1camnode:
		ct1targets.append('Player0')
	for ct1v in ct1targets:
		check('CT1 cCamControl_SetTarget(%s)' % ct1v, lambda v=ct1v: (SDTest.Call(0x0bd740, 'tbis', ct1cam, v)[0],
			ct1camf(ct1cam)))
	check('CT1 cCamControl_VarS(EntityLink get)', lambda: (lambda r: (r[0], r[1] == ct1cam + 0xbc))(
		SDTest.Call(0x0bd8b0, 'tbispi', ct1cam, 'EntityLink', 0, 0)))
	check('CT1 cCamControl_VarS(EntityLink set)', lambda: (SDTest.Call(0x0bd8b0, 'tbispi', ct1cam, 'EntityLink',
		ct1none, 1)[0], ct1camf(ct1cam)))
	check('CT1 cCamControl_VarS(Name, Bogus)', lambda: (ct1str(SDTest.Call(0x0bd8b0, 'tbispi', ct1cam, 'Name', 0, 0)[1]),
		SDTest.Call(0x0bd8b0, 'tbispi', ct1cam, 'Bogus', 0, 0)[0]))
	# the position of view 0: only the camera of that view writes it
	ct1camname0 = SDTest.Peek(ct1world + 0x54, '@s')
	ct1viewcam = 0
	if ct1camname0:
		ct1viewcam = ct1find(ct1camname0)
	ct1savedview = ct1ints(ct1world + 0x254, (0, 4, 8))
	check('CT1 cCamControl_SetViewPos(other)', lambda: (SDTest.Call(0x0bd820, 'tviifff', ct1cam, 0, 1.0, 2.0, 3.0),
		ct1ints(ct1world + 0x254, (0, 4, 8)) == ct1savedview))
	check('CT1 view camera', lambda: (ct1camname0, ct1viewcam != 0))
	if ct1viewcam:
		check('CT1 cCamControl_SetViewPos(view)', lambda: (SDTest.Call(0x0bd820, 'tviifff', ct1viewcam, 0, 1.0, 2.0, 3.0),
			ct1fl(ct1world + 0x254, 3)))
		ct1k = 0
		while ct1k < 3:
			ct1poke(ct1world + 0x254 + 4 * ct1k, ct1savedview[ct1k])
			ct1k = ct1k + 1

# ---- cCamPathEntity ("CamPath") ----
def ct1point(p, i):
	pt = p + 0x158 + 0xb0 * i
	return (ct1str(pt), ct1fl(pt + 0x80, 10), ct1bytes(pt, (0xa8, 0xa9)), ct1fl(pt + 0xac, 1))
def ct1pathf(p):
	return (ct1fl(p + 0x150, 1), ct1i(p + 0x154), ct1camf(p))
ct1path = SDTest.Call(0x208360, 'tiisfffs', ct1world, 'SDCt1Path', ct1x + 100.0, ct1y, ct1z_, 'CamPath')[0]
check('CT1 CamPath', lambda: ct1path != 0)
if ct1path:
	check('CT1 cCamPathEntity_ctor', lambda: (ct1i(ct1path) - 0x400000, ct1pathf(ct1path), ct1point(ct1path, 0),
		ct1point(ct1path, 31)))
	ct1mem = ct1z(0xb0)
	check('CT1 cCamPathPoint_ctor', lambda: (SDTest.Call(0x0bdc70, 'tii', ct1mem)[0] == ct1mem, ct1fl(ct1mem + 0x80, 11),
		ct1bytes(ct1mem, (0, 0xa8, 0xa9, 0xaa))))
	ct1free(ct1mem)
	check('CT1 cCamPathEntity_GetClassName', lambda: SDTest.Call(0x0be030, 'tsi', ct1path)[0])
	check('CT1 cCamPathEntity_IsType', lambda: tuple(map(lambda t: SDTest.Call(0x0be520, 'tbis', ct1path, t)[0],
		('CamPath', 'Cam', 'Entity', 'FCam', ''))))
	for ct1v in (('Near05', 'Near', 4), ('Near5', 'Near', 4), ('Near32', 'Near', 4), ('Near33', 'Near', 4),
			('Near0a', 'Near', 4), ('Nearxx', 'Near', 4), ('Near05x', 'Near', 4), ('Far00', 'Far', 3), ('Fa00', 'Far', 3),
			('Near', 'Near', 4), ('Near99', 'Near', 4)):
		check('CT1 cCamPathEntity_PointIndex%s' % fmt(ct1v), lambda v=ct1v: SDTest.Call(0x0bdfd0, 'tiissi', ct1path, v[0],
			v[1], v[2])[0])
	for ct1v in (('FadeIn00', 0), ('FadeIn01', 0), ('FadeOut01', 7), ('FadeOut02', 0), ('FadeIn99', 1),
			('nWayPoint', 3), ('UsrControl', 1), ('Bogus', 1), ('', 1)):
		check('CT1 cCamPathEntity_VarI(%s, %d)' % ct1v, lambda v=ct1v: (SDTest.Call(0x0be040, 'tbispi', ct1path, v[0],
			v[1], 1)[0], SDTest.Call(0x0be040, 'tbispi', ct1path, v[0], -5, 0)))
	for ct1v in (('Near01', 20.0), ('Far01', 5000.0), ('FOV01', 60.0), ('Quake01', 2.0), ('Time00', 1.0),
			('Time01', 0.5), ('Time02', 2.0), ('Quake00', 0.0), ('Fov', 70.0), ('Bogus', 1.0), ('', 1.0)):
		check('CT1 cCamPathEntity_VarF(%s)' % ct1v[0], lambda v=ct1v: (SDTest.Call(0x0bdd50, 'tbisDi', ct1path, v[0],
			v[1], 1)[0], SDTest.Call(0x0bdd50, 'tbisDi', ct1path, v[0], -7.0, 0)))
	for ct1v in (('Pos00', (ct1x, ct1y, ct1z_)), ('Ang00', (10.0, 20.0, 0.0)), ('Pos01', (ct1x + 400.0, ct1y + 100.0,
			ct1z_ - 50.0)), ('Ang01', (-30.0, 170.0, 5.0)), ('Pos02', (ct1x + 800.0, ct1y, ct1z_)),
			('Ang02', (0.0, -170.0, 0.0)), ('camPos', (ct1x, ct1y, ct1z_)), ('Bogus', (1.0, 2.0, 3.0))):
		check('CT1 cCamPathEntity_VarV(%s)' % ct1v[0], lambda v=ct1v: (SDTest.Call(0x0be870, 'tbisDDDi', ct1path, v[0],
			v[1][0], v[1][1], v[1][2], 1)[0], SDTest.Call(0x0be870, 'tbisDDDi', ct1path, v[0], 0.0, 0.0, 0.0, 0)))
	check('CT1 cCamPathEntity points', lambda: (ct1point(ct1path, 0), ct1point(ct1path, 1), ct1point(ct1path, 2)))
	# the interpolation between point 0 and 1 for each fade, quake 2 degrees
	for ct1fade in ((0, 0), (1, 0), (0, 1), (1, 1)):
		SDTest.Call(0x0be040, 'tbispi', ct1path, 'FadeIn00', ct1fade[0], 1)
		SDTest.Call(0x0be040, 'tbispi', ct1path, 'FadeOut00', ct1fade[1], 1)
		SDTest.Call(0x0bdd50, 'tbisDi', ct1path, 'Quake00', 2.0, 1)
		for ct1f in (0.0, 0.25, 0.5, 1.0):
			ct1srand(4321)
			check('CT1 cCamPathEntity_Interp%s' % fmt((ct1fade, ct1f)), lambda f=ct1f: (SDTest.Call(0x0be1c0, 'tviiif',
				ct1path, ct1path + 0x158 + 0xb0, ct1path + 0x158, f), ct1camf(ct1path)))
	SDTest.Call(0x0bdd50, 'tbisDi', ct1path, 'Quake00', 0.0, 1)
	# a point that follows the camera SDCt1Cam
	if ct1cam:
		ct1camstr = ct1cstr('SDCt1Cam')
		check('CT1 cCamPathEntity_VarS(TgtCam02)', lambda: (SDTest.Call(0x0be740, 'tbispi', ct1path, 'TgtCam02',
			ct1camstr, 1)[0], ct1point(ct1path, 2)))
		check('CT1 cCamPathEntity_VarS(TgtCam02 get)', lambda: (lambda r: (r[0], r[1] == ct1path + 0x158 + 2 * 0xb0))(
			SDTest.Call(0x0be740, 'tbispi', ct1path, 'TgtCam02', 0, 0)))
		check('CT1 cCamPathPoint_Refresh', lambda: (SDTest.Call(0x0bd7f0, 'tvifff', ct1cam, ct1x + 5.0, ct1y + 6.0,
			ct1z_ + 7.0), SDTest.Call(0x0be570, 'tvi', ct1path + 0x158 + 2 * 0xb0), ct1point(ct1path, 2)))
	check('CT1 cCamPathEntity_VarS(OnEnd)', lambda: (SDTest.Call(0x0be740, 'tbispi', ct1path, 'OnEnd', ct1fn, 1)[0],
		SDTest.Call(0x0be740, 'tbispi', ct1path, 'OnEnd', 0, 0)[0], SDTest.Call(0x0be740, 'tbispi', ct1path, 'Bogus', 0,
		0)[0], SDTest.Call(0x0be740, 'tbispi', ct1path, '', 0, 0)[0]))
	# the way along the points: 1 s to point 1, 0.5 s to point 2, the end after it (OnEnd once)
	for ct1t in (0.0, 0.3, 0.98, 1.2, 1.47, 1.6, 2.0):
		ct1srand(77)
		check('CT1 cCamPathEntity_Update %s' % fmt(ct1t), lambda t=ct1t: (ct1set(ct1path + 0x150, (t,)),
			SDTest.Call(0x0be630, 'tvi', ct1path), ct1pathf(ct1path), ct1takecalls()))
	check('CT1 cCamPathEntity_VarF(Time set)', lambda: (SDTest.Call(0x0bdd50, 'tbisDi', ct1path, 'Time', 0.5, 1)[0],
		SDTest.Call(0x0bdd50, 'tbisDi', ct1path, 'Time', 0.0, 0), ct1pathf(ct1path), ct1takecalls()))
	check('CT1 cCamPathEntity delete', lambda: ct1delete(ct1path))

# ---- cFollowCamEntity ("FCam") on an "Obj" ----
def ct1fcf(c):
	return (ct1fl(c + 0x1c4, 12), ct1fl(c + 0x204, 11), ct1i(c + 0x200), ct1bytes(c, (0x140, 0x141)), ct1camf(c))
ct1obj = SDTest.Call(0x208360, 'tiisfffs', ct1world, 'SDCt1Obj', ct1x - 200.0, ct1y, ct1z_ + 300.0, 'Obj')[0]
ct1fc = SDTest.Call(0x208360, 'tiisfffs', ct1world, 'SDCt1FCam', ct1x - 200.0, ct1y + 50.0, ct1z_, 'FCam')[0]
check('CT1 FCam', lambda: (ct1obj != 0, ct1fc != 0))
if ct1obj and ct1fc:
	ct1set(ct1obj + 0x94, (0.25, 1.0, 0.0, ct1x - 200.0, ct1y, ct1z_ + 300.0))
	check('CT1 cFollowCamEntity_ctor', lambda: (ct1i(ct1fc) - 0x400000, ct1i(ct1fc + 0x13c), ct1fcf(ct1fc)))
	check('CT1 cFollowCamEntity_GetClassName', lambda: SDTest.Call(0x0c0410, 'tsi', ct1fc)[0])
	check('CT1 cFollowCamEntity_IsType', lambda: tuple(map(lambda t: SDTest.Call(0x0c04f0, 'tbis', ct1fc, t)[0],
		('FCam', 'Cam', 'Entity', 'CamPath', ''))))
	for ct1v in (('QuakeFactor', 2.0), ('QuakeTime', 0.0), ('HSpeed', 4.0), ('VSpeed', 3.0), ('RetSpd', 500.0),
			('CamDist', 400.0), ('CamAng', -20.0), ('CamRot', 5.0), ('AddTurn', 0.0), ('AddTurnSpeed', 0.0),
			('Fov', 60.0), ('Bogus', 1.0), ('', 1.0)):
		check('CT1 cFollowCamEntity_VarF(%s)' % ct1v[0], lambda v=ct1v: (SDTest.Call(0x0bf5a0, 'tbisDi', ct1fc, v[0],
			v[1], 1)[0], SDTest.Call(0x0bf5a0, 'tbisDi', ct1fc, v[0], -7.0, 0), ct1fcf(ct1fc)))
	ct1objstr = ct1cstr('SDCt1Obj')
	check('CT1 cFollowCamEntity_VarS(OnSwitch)', lambda: (SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'OnSwitch', ct1fn, 1)[0],
		(lambda r: (r[0], ct1str(r[1])))(SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'OnSwitch', 0, 0))))
	check('CT1 cFollowCamEntity_VarS(MainTarget)', lambda: (SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'MainTarget',
		ct1objstr, 1)[0], (lambda r: (r[0], r[1] == ct1fc + 0x141))(SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'MainTarget',
		0, 0)), ct1str(ct1fc + 0x141)))
	check('CT1 cFollowCamEntity_VarS(Target get none)', lambda: (lambda r: (r[0], ct1str(r[1])))(
		SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'Target', 0, 0)))
	check('CT1 cFollowCamEntity_VarI(FreeCam)', lambda: (SDTest.Call(0x0c0440, 'tbispi', ct1fc, 'FreeCam', 0, 0),
		SDTest.Call(0x0c0440, 'tbispi', ct1fc, 'FreeCam', 1, 1)[0], SDTest.Call(0x0c0440, 'tbispi', ct1fc, 'FreeCam',
		0, 0), ct1i(ct1fc + 0x13c), ct1takecalls()))
	# FreeCam off: the main target SDCt1Obj
	check('CT1 cFollowCamEntity_SetFreeCam(0)', lambda: (SDTest.Call(0x0c0440, 'tbispi', ct1fc, 'FreeCam', 0, 1)[0],
		ct1i(ct1fc + 0x13c) == ct1obj, ct1i(ct1obj + 0xf0) == ct1fc, ct1fcf(ct1fc), ct1takecalls()))
	check('CT1 cFollowCamEntity_VarS(Target get)', lambda: (lambda r: (r[0], ct1str(r[1])))(
		SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'Target', 0, 0)))
	check('CT1 cFollowCamEntity_VarS(Target set none)', lambda: (SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'Target',
		ct1none, 1)[0], ct1i(ct1fc + 0x13c), ct1i(ct1obj + 0xf0), ct1fcf(ct1fc), ct1takecalls()))
	check('CT1 cFollowCamEntity_VarS(Target set)', lambda: (SDTest.Call(0x0c0b10, 'tbispi', ct1fc, 'Target', ct1objstr,
		1)[0], ct1i(ct1fc + 0x13c) == ct1obj, ct1fcf(ct1fc), ct1takecalls()))
	check('CT1 cCamEntity_GetTarget', lambda: SDTest.Call(0x0c03e0, 'tii', ct1fc)[0] == ct1obj)
	check('CT1 cFollowCamEntity_VarV', lambda: (SDTest.Call(0x0c0d00, 'tbisDDDi', ct1fc, 'camPos', 1.0, 2.0, 3.0, 0),
		SDTest.Call(0x0c0d00, 'tbisDDDi', ct1fc, '', 1.0, 2.0, 3.0, 0)[0]))
	check('CT1 cFollowCamEntity_GetAng2', lambda: SDTest.Call(0x0c03f0, 'tviF', ct1fc, (0.0, 0.0))[1])
	check('CT1 cFollowCamEntity_SetAng2', lambda: (SDTest.Call(0x0c09a0, 'tviF', ct1fc, (0.5, -0.25)),
		ct1fl(ct1fc + 0x1e8, 2)))
	check('CT1 cFollowCamEntity_ResetAngles', lambda: (ct1set(ct1obj + 0x94, (0.3, 2.0, 0.0)),
		SDTest.Call(0x0c06f0, 'tvi', ct1fc), ct1fcf(ct1fc)))
	# the slot turns towards the target, the lerp places the camera behind it
	for ct1v in ((0.05, 0.4, -1.0, 0.0, 0), (0.1, 0.4, 3.0, 0.5, 0), (0.05, -2.5, 3.0, 0.0, 1), (0.2, 3.0, 0.1, 0.0, 2)):
		check('CT1 cFollowCamEntity_Slot%s' % fmt(ct1v), lambda v=ct1v: (ct1set(ct1obj + 0x94, (v[1], v[2], 0.0)),
			ct1set(ct1fc + 0x210, (v[3],)), ct1poke(ct1fc + 0x200, v[4]), SDTest.Call(0x0bed40, 'tvif', ct1fc, v[0]),
			ct1fcf(ct1fc)))
	ct1set(ct1fc + 0x210, (0.0,))
	for ct1v in ((0.0, 0.02), (0.5, 0.02), (1.0, 0.1), (0.25, 0.1), (1.0, 0.0)):
		check('CT1 cFollowCamEntity_Lerp%s' % fmt(ct1v), lambda v=ct1v: (ct1set(ct1wt + 0xc, (v[1],)),
			SDTest.Call(0x0befe0, 'tvif', ct1fc, v[0]), ct1fcf(ct1fc), ct1fl(ct1fc + 0x1f4, 3)))
	ct1set(ct1wt + 0xc, (0.02,))
	check('CT1 cCamEntity_SetAng', lambda: (SDTest.Call(0x0bfa90, 'tvifff', ct1fc, 0.1, 2.5, 0.0), ct1fcf(ct1fc),
		ct1fl(ct1fc + 0x1f4, 3)))
	SDTest.Call(0x0bf5a0, 'tbisDi', ct1fc, 'QuakeFactor', 3.0, 1)
	SDTest.Call(0x0bf5a0, 'tbisDi', ct1fc, 'QuakeTime', 0.1, 1)
	for ct1dt in (0.03, 0.04, 0.05, 0.05):
		ct1srand(99)
		check('CT1 cFollowCamEntity_Quake %s' % fmt(ct1dt), lambda dt=ct1dt: (SDTest.Call(0x0c0570, 'tvif', ct1fc, dt),
			ct1fcf(ct1fc)))
	SDTest.Call(0x0bf5a0, 'tbisDi', ct1fc, 'QuakeTime', 0.1, 1)
	ct1srand(98)
	check('CT1 cFollowCamEntity_UpdateOff', lambda: (SDTest.Call(0x0c0420, 'tvif', ct1fc, 0.05), ct1fcf(ct1fc)))
	check('CT1 cFollowCamEntity_SetTargetName', lambda: (SDTest.Call(0x0c0540, 'tbis', ct1fc, 'SDNone')[0],
		ct1fcf(ct1fc)))
	check('CT1 cFollowCamEntity_Update off', lambda: (SDTest.Call(0x0c09c0, 'tvi', ct1fc), ct1fcf(ct1fc)))
	# the free camera of view slot 3 (no input there), when the action sets have its actions
	ct1sets = SDTest.Peek(0x44c12c, 'i')
	ct1n1 = 0
	ct1n3 = 0
	if ct1sets and SDTest.Peek(0x44c128, 'i') > 3:
		ct1n1 = ct1i(ct1sets + 0xc + 4)
		ct1n3 = ct1i(ct1sets + 0x24 + 4)
	check('CT1 action sets', lambda: (ct1n1 > 19, ct1n3 > 19))
	if ct1n1 > 19 and ct1n3 > 19:
		check('CT1 cCamEntity_SetTarget(0)', lambda: (SDTest.Call(0x0c07f0, 'tvii', ct1fc, 0), ct1i(ct1obj + 0xf0),
			ct1fcf(ct1fc), ct1takecalls()))
		ct1poke(ct1fc + 0xb8, 3)
		ct1pokeb(ct1fc + 0x140, 1)
		check('CT1 cFollowCamEntity_FreeMove', lambda: (SDTest.Call(0x0bfb00, 'tvi', ct1fc), ct1fcf(ct1fc)))
		check('CT1 cFollowCamEntity_Update free', lambda: (SDTest.Call(0x0c09c0, 'tvi', ct1fc), ct1i(ct1fc + 0x13c),
			ct1fcf(ct1fc)))
		ct1poke(ct1fc + 0xb8, -1)
	check('CT1 FCam delete', lambda: (ct1delete(ct1fc), ct1i(ct1obj + 0xf0), ct1takecalls()))
	check('CT1 Obj delete', lambda: ct1delete(ct1obj))
if ct1cam:
	check('CT1 Cam delete', lambda: ct1delete(ct1cam))

# ---- the type natives ----
# missing characters: the ones that return without a CHECK
for ct1v in ('BankMasterActionPress', 'BertoActionPress', 'BishopActionPress', 'BettyActionPress',
		'DtritusActionPress'):
	check('CT1 native %s(SDNone)' % ct1v, lambda v=ct1v: ct1native(v, ('SDNone',)))
check('CT1 native BankMasterActionPress()', lambda: ct1native('BankMasterActionPress', ('',)))
ct1ctrl = SDTest.Call(ct1slot(ct1player, 0xd4), 'tii', ct1player)[0]
def ct1act():
	return (SDTest.Call(0x062c40, 'tsii', ct1ctrl, 0)[0], ct1bytes(ct1player, (0x504, 0x505)))
check('CT1 player class', lambda: (ct1ctrl != 0, ct1str(ct1i(ct1i(ct1ctrl + 8) + 4)), ct1act()))
# what the natives change: the energy, sound 0, the life, the money, the MeEnt globals
ct1savedenergy = ct1i(ct1player + 0x498)
ct1snd = SDTest.Call(0x1a26e0, 'tiii', ct1player + 0x144, 0)[0]
ct1savedsnd = (0, 0)
if ct1snd:
	ct1savedsnd = ct1ints(ct1snd, (0x40, 0xc))
ct1savedlife = ct1i(ct1player + 0x238)
ct1savedmoney = ct1i(ct1world + 0x2090)
ct1meents = (0x4460a0, 0x4461ac, 0x4462a0, 0x44665c, 0x446858)
ct1savedme = tuple(map(lambda r: SDTest.Peek(r, 'i'), ct1meents))
def ct1sndf():
	if ct1snd:
		return (ct1fl(ct1snd + 0x40, 1), ct1b(ct1snd + 0xc))
	return None
check('CT1 player saved', lambda: (ct1snd != 0, ct1sndf(), ct1fl(ct1player + 0x498, 1), ct1fl(ct1player + 0x4b0, 1),
	ct1savedlife, ct1savedmoney, tuple(map(lambda v: v != 0, ct1savedme))))
for ct1v in ('GearInitAction', 'BankMasterActionInit', 'BertoActionEnd', 'BertoActionInit', 'BankMasterActionEnd'):
	check('CT1 native %s(Player0)' % ct1v, lambda v=ct1v: (ct1native(v, ('Player0', 'SDEvent', 0.0)), ct1sndf(), ct1act()))
for ct1v in ('BankMasterHitEvent', 'BettyHitEvent', 'DtritusHitEvent', 'BertoHitEvent'):
	check('CT1 native %s(Player0)' % ct1v, lambda v=ct1v: (ct1native(v, ('Player0', '8', 0.0)), ct1fl(ct1player + 0x498, 1),
		tuple(map(lambda r: SDTest.Peek(r, 'i') == ct1player, ct1meents))))
ct1poke(ct1player + 0x498, ct1savedenergy)
check('CT1 native BettyRestoreTime(Player0)', lambda: (ct1native('BettyRestoreTime', ('Player0', 'SDEvent', 0.0)),
	ct1fl(ct1wt + 0x14, 5)))
ct1restoretimer()
ct1set(ct1wt + 8, (100.0, 0.02))
for ct1v in ('BankMasterAttack', 'BertoAttack'):
	check('CT1 native %s(Player0)' % ct1v, lambda v=ct1v: (ct1poke(ct1player + 0x498, ct1savedenergy), ct1native(v,
		('Player0',)), ct1fl(ct1player + 0x498, 1), ct1act()))
ct1poke(ct1player + 0x498, ct1savedenergy)
for ct1v in (('BankDirectorActionPress', 1), ('BankDirectorActionRelease', 1), ('BankDirectorEndActionInit', 1),
		('BankMasterActionPress', 1), ('GearActionPress', 1), ('GearEndAction', 3), ('BishopEndActionEnd', 3),
		('GearEndActionEnd', 3)):
	if ct1v[1] == 1:
		ct1params = ('Player0',)
	else:
		ct1params = ('Player0', 'SDEvent', 0.0)
	check('CT1 native %s(Player0)' % ct1v[0], lambda v=ct1v[0], params=ct1params: (ct1native(v, params), ct1act()))
ct1k = 0
while ct1k < len(ct1meents):
	SDTest.Poke(ct1meents[ct1k], ct1savedme[ct1k])
	ct1k = ct1k + 1
if ct1snd:
	ct1poke(ct1snd + 0x40, ct1savedsnd[0])
	ct1poke(ct1snd + 0xc, ct1savedsnd[1])
ct1poke(ct1player + 0x498, ct1savedenergy)

# the money, the victim test and the steal on fake characters
ct1fa = ct1z(0x574)
ct1fb = ct1z(0x574)
def ct1money(e):
	return ct1ints(e, (0x4cc, 0x4d0))
check('CT1 ChType_CanRob', lambda: (SDTest.Call(0x0c13d0, 'bii', 0, 0)[0], SDTest.Call(0x0c13d0, 'bii', 0, ct1player)[0],
	ct1i(ct1player + 0x4cc)))
check('CT1 ChType_BankerVictim', lambda: (SDTest.Call(0x0c1360, 'ii', ct1fa)[0], (ct1poke(ct1fa + 0x53c, ct1player),
	SDTest.Call(0x0c1360, 'ii', ct1fa)[0] == ct1player)[1]))
ct1poke(ct1fa + 0x53c, 0)
for ct1v in ((100, 40, 1.0, 0), (100, 40, 0.25, 0), (10, 40, 1.0, 1), (0, 0, 1.0, 1), (5, -10, 0.5, 0)):
	check('CT1 ChType_Steal(player)%s' % fmt(ct1v), lambda v=ct1v: (ct1poke(ct1fa + 0x4cc, v[0]),
		ct1poke(ct1fa + 0x4d0, v[1]), ct1poke(ct1world + 0x2090, 50), SDTest.Call(0x0c1480, 'bppizf', ct1player, ct1fa,
		30, None, v[2])[0], ct1money(ct1fa), ct1i(ct1world + 0x2090)))
# a thief with the vtable of the player and no logic
ct1poke(ct1fb, ct1i(ct1player))
check('CT1 ChType_Steal(fake)', lambda: (ct1poke(ct1fb + 0x4cc, 100), SDTest.Call(0x0c1480, 'bppizf', ct1fb, ct1fa, 30,
	None, 1.0)[0], ct1money(ct1fb), ct1money(ct1fa)))
ct1poke(ct1world + 0x2090, ct1savedmoney)
ct1free(ct1fa)
ct1free(ct1fb)
# the gear ball: not the shooter, the life of the player down (restored)
ct1savedtarget = SDTest.Peek(0x446758, 's')
check('CT1 ChType_GearHitChar', lambda: (SDTest.Call(0x0c6910, 'vsi', 'SDNone', 0), SDTest.Call(0x0c6910, 'vsi', '',
	0), SDTest.Call(0x0c6910, 'vsi', 'Player0', 1), ct1i(ct1player + 0x238), ct1savedtarget))
ct1poke(ct1player + 0x238, ct1savedlife)
SDTest.Call(0x22cf50, 'iisi', 0x400000 + 0x446758, 'Player0', 8)
check('CT1 ChType_GearHitChar(shooter)', lambda: (SDTest.Call(0x0c6910, 'vsi', 'Player0', 0), ct1i(ct1player + 0x238)))
SDTest.Call(0x22cf50, 'iisi', 0x400000 + 0x446758, ct1savedtarget, len(ct1savedtarget) + 1)
ct1poke(ct1player + 0x238, ct1savedlife)
# the flash: far away, at the player (it is not an AI)
def ct1flash():
	return tuple(map(lambda r: SDTest.Peek(r, 'i'), (0x446b18, 0x446b1c, 0x446b20)))
check('CT1 Logic_Flash far', lambda: (SDTest.Call(0x0c9180, 'iFfi', (ct1x, ct1y + 5000.0, ct1z_), 100.0, -1), ct1flash()))
check('CT1 Logic_Flash player', lambda: (SDTest.Call(0x0c9180, 'iFfi', ct1pp, 500.0, 1), ct1flash()))
check('CT1 ChType_DirAngles', lambda: tuple(map(lambda d: SDTest.Call(0x0c3780, 'vFF', d, (0.0, 0.0, 0.0))[2],
	((0.0, 0.0, 1.0), (1.0, 2.0, -3.0), (0.0, 1.0, 0.0), (0.0, 0.0, 0.0)))))
ct1mem = ct1z(0x30)
check('CT1 cAIChar_TestFlags', lambda: (SDTest.Call(0x0c9750, 'tbi', ct1mem)[0], (ct1pokeb(ct1mem + 0x2a, 1),
	SDTest.Call(0x0c9750, 'tbi', ct1mem)[0])[1], (ct1pokeb(ct1mem + 0x28, 1), ct1pokeb(ct1mem + 0x2a, 0),
	SDTest.Call(0x0c9750, 'tbi', ct1mem)[0])[2]))
ct1free(ct1mem)
ct1free(ct1fn)
ct1free(ct1none)
ct1restoretimer()
