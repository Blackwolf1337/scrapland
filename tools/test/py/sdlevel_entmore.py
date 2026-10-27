# game Entities/ObjEntity.c, Entities/More/ModMove.c, Primitive.c, ArrowIndicator.c and Entities/Vehicle/
# VehicleAttack.c, AutoAim.c, Controls.c (execfile'd by sdlevel.py, Python 1.5.2). cObjEntity on the objects of FZ
# (reads only; values only of objects that do not move) and on "Obj" entities made by cWorld_CreateEntity far above
# the player: the constructor, the getters and setters, the script variables, Update, UpdateOff and Tick with the
# entity timer, the slot controller and the world time poked (the frame time of the world is not used), the timed
# move (cObjMove), a model (Battery) with its collision and nodes, the model link of cModMove, the embedded effect,
# a follow camera, and the deleting destructor. cNetController, cVehicleAttack, cAutoAim, the controls, cPrimitive
# and cArrowIndicator on calloc memory (owners and targets are two "WithLife" entities made the same way) and on the
# parts of those entities. Every poked engine variable is restored. Not called: Render (draws), the arrow models and
# their glow (the model names come from the game scripts), cAutoAim_Update (needs a vehicle), the network Write
# (still in the exe), MissionArrows_Switch (changes the mission arrow models of the game).
def emoi(a):
	return SDTest.Peek(a, '@i')
def emob(a):
	return SDTest.Peek(a, '@b')
def emopoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def emofl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def emoset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def emoz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def emofree(p):
	SDTest.Call(0x133550, 'ii', p)
def emostr(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def emoname(e):
	if e:
		return SDTest.Peek(e + 4, '@p')
	return None
def emocstr(s):
	a = emoz(len(s) + 4)
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
	return a
def emoints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def emobytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
# a vtable slot of the entity as rva
def emoslot(e, off):
	return emoi(emoi(e) + off) - 0x400000
def emoistype(e, t):
	return SDTest.Call(emoslot(e, 0x14), 'tbis', e, t)[0]
# bytes into memory four at a time (little endian, the top byte folded into the sign)
def emobytesto(a, bs):
	bs = bs + [0, 0, 0]
	i = 0
	while i < len(bs) - 3:
		v = bs[i] + bs[i + 1] * 256 + bs[i + 2] * 65536 + (bs[i + 3] & 127) * 16777216
		if bs[i + 3] >= 128:
			v = v - 2147483647 - 1
		emopoke(a + i, v)
		i = i + 4
def emoshort(v):
	if v < 0:
		v = v + 65536
	return [v & 255, v / 256]
def emobyte(v):
	if v < 0:
		v = v + 256
	return [v]
# one byte, the three after it kept
def emopokeb(a, v):
	emobytesto(a, [v, emob(a + 1), emob(a + 2), emob(a + 3)])

emoworld = SDTest.Peek(0x467450, 'i')
emoplayer = SDTest.Call(0x0c1430, 'tiii', emoworld, 0)[0]
emopp = SDTest.Call(0x0ef330, 'tiiF', emoplayer, (0.0, 0.0, 0.0))[1]
emowt = emoworld + 0x2c8
# the world time and frame time fixed while the tests run (the raw words saved)
emosavedwt = emoints(emowt, (8, 0xc, 0x14))
emoset(emowt + 8, (100.0,))
emoset(emowt + 0xc, (0.02,))

# ---- the Obj entities of the level: by class; values only of the ones that do not move ----
emoclasses = {}
emostatic = []
emocount = 0
emoe = SDTest.Call(0x2092a0, 'tii', emoworld)[0]
while emoe and emocount < 5000:
	emocount = emocount + 1
	if SDTest.Call(0x0fbad0, 'tbis', emoe, 'Obj')[0]:
		emoc = SDTest.Call(emoslot(emoe, 0x10), 'tsi', emoe)[0]
		if emoclasses.has_key(emoc):
			emoclasses[emoc] = emoclasses[emoc] + 1
		else:
			emoclasses[emoc] = 1
		if emoc == 'Obj' and emofl(emoe + 0xd4, 6) == (0.0,) * 6 and len(emostatic) < 8:
			emostatic.append(emoe)
	emoe = SDTest.Call(0x0ef310, 'tii', emoe)[0]
emokeys = emoclasses.keys()
emokeys.sort()
check('Obj classes', lambda: map(lambda k: (k, emoclasses[k]), emokeys))
check('Obj static', lambda: len(emostatic))
# the getters on every Obj (class independent results only)
def emogetters(e):
	return (SDTest.Call(0x0fb380, 'tii', e)[0] == e + 0xb8, SDTest.Call(0x0fb3d0, 'tii', e)[0],
		SDTest.Call(0x0fb740, 'tfi', e)[0], SDTest.Call(0x0fb710, 'tsi', e)[0],
		map(lambda t, e=e: SDTest.Call(0x0fbad0, 'tbis', e, t)[0], ('Obj', 'obj', 'Entity', 'WithLife', '')))
emoe = SDTest.Call(0x2092a0, 'tii', emoworld)[0]
emocount = 0
emoall = []
while emoe and emocount < 5000:
	emocount = emocount + 1
	if SDTest.Call(0x0fbad0, 'tbis', emoe, 'Obj')[0]:
		emoall.append(emogetters(emoe))
	emoe = SDTest.Call(0x0ef310, 'tii', emoe)[0]
check('Obj getters', lambda: emoall)
def emovecs(e):
	r = []
	for f in (0x0fb3b0, 0x0fb2a0, 0x0fb340, 0x0fb300, 0x0fb320, 0x0fb2e0, 0x0fb720, 0x0fb2c0, 0x0fb3e0, 0x0fb360):
		r.append(SDTest.Call(f, 'tiiF', e, (9.0, 9.0, 9.0))[1])
	return r
emoivars = ('RadarShow', 'MoveFade', 'UseRealTime', 'MoveViewAng', 'MapCollision', 'RenderEffect', 'OffModPosAsPos',
	'UseLinkAng', 'Active', 'Bogus', '')
emofvars = ('MoveTime', 'TimeSpeed', 'EndTimeSpeed', 'FadeTimeSpeed', 'RenderEffectParam0', 'OffMul', 'DeltaOffMul',
	'Bogus', '')
emosvars = ('OnEndMove', 'CamName', 'NetRes', 'OnObjSlot', 'Name', 'Type', 'Bogus', '')
emovvars = ('MoveAng', 'MovePos', 'SndPos', 'CamTgt', 'Ang', 'Pos', 'OffModPos', 'NodePos_DC_Root',
	'NT_NodePos_DC_Root', 'PvRot', 'Bogus', '')
# the names that need a model (CHECK without one) and the node names that need the node
def emovarok(e, v):
	if v in ('RenderEffect', 'RenderEffectParam0'):
		return SDTest.Call(0x210da0, 'tbi', e + 0xb8)[0]
	if v[:8] == 'NodePos_' or v[:11] == 'NT_NodePos_':
		return SDTest.Call(0x210da0, 'tbi', e + 0xb8)[0] and SDTest.Call(0x20f7d0, 'tiis', e + 0xb8, 'DC_Root')[0] > 0
	return 1
def emogets(e, tag):
	for v in emoivars:
		if emovarok(e, v):
			check('cObjEntity_VarI(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call(0x0fb7d0, 'tbispi', e, v, -5, 0))
	for v in emofvars:
		if emovarok(e, v):
			check('cObjEntity_VarF(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call(0x0fafd0, 'tbisDi', e, v, -5.0, 0))
	for v in emosvars:
		check('cObjEntity_VarS(%s, %s)' % (tag, v), lambda e=e, v=v: (lambda r: (r[0], r[0] and emostr(r[1])))(
			SDTest.Call(0x0fd0a0, 'tbispi', e, v, 0, 0)))
	for v in emovvars:
		if emovarok(e, v):
			check('cObjEntity_VarV(%s, %s)' % (tag, v), lambda e=e, v=v: SDTest.Call(0x0fd2c0, 'tbisDDDi', e, v, -1.0,
				-2.0, -3.0, 0))
for emoi_ in range(len(emostatic)):
	emoe = emostatic[emoi_]
	check('Obj static %d' % emoi_, lambda e=emoe: (emoname(e), emovecs(e), SDTest.Call(0x0fb400, 'tbi', e)[0],
		SDTest.Call(0x0fb570, 'tviFF', e, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))))
	if emoi_ < 3:
		emogets(emoe, 'static %d' % emoi_)
check('Player0 Obj', lambda: (emogetters(emoplayer), SDTest.Call(0x0fb570, 'tviFF', emoplayer, (0.0, 0.0, 0.0),
	(0.0, 0.0, 0.0))[1:]))

# ---- own Obj entities ----
emox = emopp[0] + 400.0
emoy = emopp[1] + 3000.0
emoz_ = emopp[2] + 200.0
emoA = SDTest.Call(0x208360, 'tiisfffs', emoworld, 'SDObjA', emox, emoy, emoz_, 'Obj')[0]
emoB = SDTest.Call(0x208360, 'tiisfffs', emoworld, 'SDObjB', emox + 100.0, emoy, emoz_ - 50.0, 'Obj')[0]
check('cObjEntity_ctor', lambda: (emoA != 0, emoB != 0, emoname(emoA), emoname(emoB)))
# the fields the constructor writes (the time of the slot controller is random)
def emofields(e):
	slot = emoi(e + 0x140)
	return (emoi(e) - 0x400000, emofl(e + 0x28, 3), emofl(e + 0x7c, 15), emofl(e + 0xd4, 6), emob(e + 0xec),
		emoi(e + 0xf0) != 0, emoi(e + 0xf4) != 0, emofl(e + 0xf8, 4), emoi(e + 0x108), emofl(e + 0x10c, 1),
		emob(e + 0x138), emoi(e + 0x13c), slot != 0, slot and emofl(slot + 8, 2), emoi(e + 0x15c) == e,
		emoi(e + 0x148), emoi(e + 0x224) != 0)
def emomove(e):
	m = e + 0x174
	return (emofl(m, 14), emobytes(m, (0x38, 0x6c)), emofl(m + 0x3c, 6), emoi(m + 0x68), emoi(m + 0x5c) != 0)
def emomod(e):
	m = e + 0x1e4
	return (emoi(m) == e, emoi(m + 0xc) == e, emoi(m + 0x10), emoname(emoi(m + 0x1c)), emoi(m + 0x20) == e,
		emoi(m + 0x24), emofl(m + 0x28, 5), emobytes(m, (0x3c, 0x3d)))
def emotimer(e):
	return (emofl(e + 0x110, 4), emob(e + 0x120), emofl(e + 0x124, 5))
for emoe in (emoA, emoB):
	check('Obj fields %s' % emoname(emoe), lambda emoe=emoe: (emofields(emoe), emomove(emoe), emomod(emoe), emotimer(emoe)))
check('Obj getters own', lambda: (emogetters(emoA), emovecs(emoA)))

# setters
check('cObjEntity_SetPos', lambda: (SDTest.Call(0x0fc980, 'tviF', emoA, (emox, emoy + 10.0, emoz_)),
	emofl(emoA + 0x28, 3), emofl(emoA + 0x7c, 12)))
check('cObjEntity_SetAng', lambda: (SDTest.Call(0x0fc6a0, 'tviF', emoA, (0.25, -1.5, 3.0)), emofl(emoA + 0x7c, 15)))
check('cObjEntity_SetVel', lambda: (SDTest.Call(0x0fca50, 'tviF', emoA, (10.0, -2.0, 0.5)), emofl(emoA + 0xd4, 3)))
check('cObjEntity_SetRotVel', lambda: (SDTest.Call(0x0fc740, 'tviF', emoA, (0.1, 0.2, -0.3)), emofl(emoA + 0xe0, 3)))
check('cObjEntity_GetTargetAngle', lambda: (lambda r: (r[1], emoname(r[2])))(SDTest.Call(0x0fb420, 'tviFfp', emoA,
	(0.5, 1.0, 0.0), 500.0, 0)))
check('cObjEntity_GetCamera', lambda: SDTest.Call(0x0fb570, 'tviFF', emoA, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
check('cObjEntity_GetCollision no model', lambda: SDTest.Call(0x0fb400, 'tbi', emoA)[0])
check('cObjEntity_Switch no model', lambda: SDTest.Call(0x0fca00, 'tvii', emoA, 1))

# the script variables: set and read back
for emov in (('RadarShow', 3), ('MoveFade', 2), ('UseRealTime', 5), ('UseRealTime', 0), ('MoveViewAng', 0),
		('MapCollision', 0), ('OffModPosAsPos', 1), ('OffModPosAsPos', 0), ('UseLinkAng', 7), ('Active', 0),
		('Active', 1), ('Bogus', 1)):
	check('cObjEntity_VarI(A, %s set %d)' % emov, lambda emov=emov: (SDTest.Call(0x0fb7d0, 'tbispi', emoA, emov[0], emov[1], 1)[0],
		SDTest.Call(0x0fb7d0, 'tbispi', emoA, emov[0], -5, 0)))
for emov in (('TimeSpeed', 2.0), ('EndTimeSpeed', 0.5), ('FadeTimeSpeed', 0.25), ('OffMul', 0.5), ('DeltaOffMul', -0.25),
		('MoveTime', 0.0), ('Bogus', 1.0)):
	check('cObjEntity_VarF(A, %s set %s)' % (emov[0], fmt(emov[1])), lambda emov=emov: (SDTest.Call(0x0fafd0, 'tbisDi', emoA, emov[0],
		emov[1], 1)[0], SDTest.Call(0x0fafd0, 'tbisDi', emoA, emov[0], -5.0, 0), emotimer(emoA)))
check('cObjEntity_VarF(A, TimeSpeed set 1)', lambda: SDTest.Call(0x0fafd0, 'tbisDi', emoA, 'TimeSpeed', 1.0, 1)[0])
for emov in (('MoveAng', (90.0, -45.0, 30.0)), ('MovePos', (emox + 100.0, emoy - 50.0, emoz_ + 25.0)),
		('CamTgt', (1.0, 2.0, 3.0)), ('Ang', (10.0, 20.0, -30.0)), ('OffModPos', (4.0, 5.0, 6.0)), ('SndPos', (1.0, 1.0, 1.0)),
		('Pos', (emox, emoy, emoz_)), ('Bogus', (0.0, 0.0, 0.0))):
	check('cObjEntity_VarV(A, %s set)' % emov[0], lambda emov=emov: (SDTest.Call(0x0fd2c0, 'tbisDDDi', emoA, emov[0], emov[1][0],
		emov[1][1], emov[1][2], 1)[0], SDTest.Call(0x0fd2c0, 'tbisDDDi', emoA, emov[0], 0.0, 0.0, 0.0, 0), emofl(emoA + 0x28, 3),
		emofl(emoA + 0x7c, 15)))
emocalls = []
def emocb(*args):
	emocalls.append(args)
emofn = emocstr('__main__.emocb')
emoempty = emocstr('')
emonr = emocstr('SDNoResource')
for emov, emoa in (('OnObjSlot', emofn), ('OnEndMove', emofn), ('NetRes', emonr), ('CamName', emofn), ('Name', emofn)):
	check('cObjEntity_VarS(A, %s set)' % emov, lambda emov=emov, emoa=emoa: (SDTest.Call(0x0fd0a0, 'tbispi', emoA, emov, emoa, 1)[0],
		emob(emoA + 0xec), (lambda r: (r[0], r[0] and emostr(r[1])))(SDTest.Call(0x0fd0a0, 'tbispi', emoA, emov, 0, 0))))
check('cEntity_SetNetResource', lambda: (SDTest.Call(0x0fc950, 'tvis', emoA, 'SDNoResource'), emob(emoA + 0xec)))
for emoe in (emoA, emoB):
	emogets(emoe, emoname(emoe))

# Update, UpdateOff and Tick with the frame of the entity timer and the slot controller set
def emoprep(e, dt, st, si):
	emoset(e + 0x104, (100.0,))
	emoset(e + 0x11c, (dt,))
	slot = emoi(e + 0x140)
	emoset(slot + 4, (st, si))
def emostate(e):
	slot = emoi(e + 0x140)
	return (emofl(e + 0x28, 3), emofl(e + 0x7c, 15), emofl(e + 0xd4, 6), emoints(e, (0x108, 0x13c)), emofl(e + 0x10c, 1),
		emofl(slot + 4, 2), emofl(e + 0x1a4, 2), emobytes(e + 0x174, (0x38,)), emofl(e + 0x110, 4))
def emoupdate(e, fn, dt, st, si):
	emoprep(e, dt, st, si)
	SDTest.Call(fn, 'tvi', e)
	r = (emostate(e), emocalls[:])
	del emocalls[:]
	return r
SDTest.Call(0x0fca50, 'tviF', emoA, (30.0, 0.0, -20.0))
emosavedmu = SDTest.Peek(0x447a08, 'i')
for emov in ((0.02, 0.01, 0.05), (0.03, 0.05, 0.05), (0.2, 0.01, 0.05), (0.0, 0.01, 0.05), (1e-7, 0.01, 0.05),
		(0.04, -0.01, 0.05)):
	check('cObjEntity_Update%s' % fmt(emov), lambda emov=emov: emoupdate(emoA, 0x0fca80, emov[0], emov[1], emov[2]))
SDTest.Poke(0x447a08, 1)
for emov in ((0.02, 0.01, 0.05), (0.1, 0.03, 0.05)):
	check('cObjEntity_Update extrapolated%s' % fmt(emov), lambda emov=emov: emoupdate(emoA, 0x0fca80, emov[0], emov[1], emov[2]))
SDTest.Poke(0x447a08, emosavedmu)
for emov in ((0.02, 0.01, 0.05), (1e-7, 0.01, 0.05)):
	check('cObjEntity_UpdateOff%s' % fmt(emov), lambda emov=emov: emoupdate(emoA, 0x0fb750, emov[0], emov[1], emov[2]))
check('cObjEntity_RadarTick', lambda: (SDTest.Call(0x0faf60, 'tvif', emoA, 0.5), emoints(emoA, (0x108,)),
	emofl(emoA + 0x10c, 1)))
check('cObjEntity_NewSlot', lambda: (SDTest.Call(0x0fac10, 'tvi', emoB), emoi(emoB + 0x140) != 0,
	emofl(emoi(emoB + 0x140) + 8, 2)))
# the entity timer: the same world time does nothing, else one step of the world frame (scaled back with UseRealTime)
def emotimerstep(e, last, real, speed):
	emoset(e + 0x104, (last,))
	emopokeb(e + 0x138, real)
	emoset(emowt + 0x14, (speed,))
	SDTest.Call(0x0fab40, 'tvi', e)
	emopoke(emowt + 0x14, emosavedwt[2])
	return (emofl(e + 0x104, 1), emotimer(e))
for emov in ((100.0, 0, 1.0), (99.0, 0, 1.0), (98.0, 1, 2.0), (97.0, 1, 0.005), (96.0, 1, 0.5)):
	check('cObjEntity_UpdateTimer%s' % fmt(emov), lambda emov=emov: emotimerstep(emoB, emov[0], emov[1], emov[2]))

# the timed move: MovePos and MoveAng in MoveTime with each fade
def emotick(e, dt):
	SDTest.Call(0x0fc580, 'tvif', e, dt)
	r = (emofl(e + 0x28, 3), emofl(e + 0xac, 3), emofl(e + 0xd4, 6), emofl(e + 0x174, 14), emobytes(e + 0x174, (0x38,)),
		emoi(e + 0x1dc), emofl(e + 0x10c, 1), emocalls[:])
	del emocalls[:]
	return r
for emof in (0, 1, 2, 3, 7):
	SDTest.Call(0x0fc980, 'tviF', emoB, (emox, emoy, emoz_))
	SDTest.Call(0x0fc6a0, 'tviF', emoB, (0.5, 3.0, -0.25))
	SDTest.Call(0x0fca50, 'tviF', emoB, (1.0, 2.0, 3.0))
	SDTest.Call(0x0fc740, 'tviF', emoB, (0.5, 0.25, 0.125))
	check('ObjMove setup %d' % emof, lambda f=emof: (SDTest.Call(0x0fd2c0, 'tbisDDDi', emoB, 'MovePos', emox + 300.0,
		emoy - 100.0, emoz_ + 50.0, 1)[0], SDTest.Call(0x0fd2c0, 'tbisDDDi', emoB, 'MoveAng', -170.0, 170.0, 45.0, 1)[0],
		SDTest.Call(0x0fb7d0, 'tbispi', emoB, 'MoveFade', f, 1)[0], SDTest.Call(0x0fd0a0, 'tbispi', emoB, 'OnEndMove',
		emofn, 1)[0], SDTest.Call(0x0fafd0, 'tbisDi', emoB, 'MoveTime', 0.5, 1)[0], emomove(emoB)))
	check('cObjEntity_SetVel moving %d' % emof, lambda: (SDTest.Call(0x0fca50, 'tviF', emoB, (9.0, 9.0, 9.0)),
		emofl(emoB + 0xd4, 3)))
	for emodt in (0.1, 0.15, 0.2, 0.1, 0.05):
		check('cObjEntity_Tick move %d %s' % (emof, fmt(emodt)), lambda dt=emodt: emotick(emoB, dt))
check('cObjMove_SetTime', lambda: map(lambda t: (SDTest.Call(0x0f9260, 'tvif', emoB + 0x174, t),
	emofl(emoB + 0x174 + 0x30, 2), emob(emoB + 0x174 + 0x38)), (0.0, -1.0, 2.0, 1e-30)))
check('cObjMove_Tick start', lambda: (SDTest.Call(0x0f8e20, 'tviif', emoB + 0x174, emoB, 0.25), emofl(emoB + 0x28, 3),
	emofl(emoB + 0x174, 14)))
check('cObjMove_SetTime off', lambda: (SDTest.Call(0x0f9260, 'tvif', emoB + 0x174, 0.0), emob(emoB + 0x174 + 0x38)))

# the cModMove variables and the update without a link
for emov in (('OffMul', 0.5), ('DeltaOffMul', 2.0), ('Bogus', 1.0)):
	check('cModMove_VarF(%s)' % emov[0], lambda emov=emov: (SDTest.Call(0x0f8450, 'tbisDi', emoB + 0x1e4, emov[0], emov[1], 1)[0],
		SDTest.Call(0x0f8450, 'tbisDi', emoB + 0x1e4, emov[0], 0.0, 0)))
for emov in (('OffModPosAsPos', 3), ('UseLinkAng', 1), ('Bogus', 1)):
	check('cModMove_VarI(%s)' % emov[0], lambda emov=emov: (SDTest.Call(0x0f8550, 'tbispi', emoB + 0x1e4, emov[0], emov[1], 1)[0],
		SDTest.Call(0x0f8550, 'tbispi', emoB + 0x1e4, emov[0], -1, 0)))
check('cModMove_VarV(OffModPos)', lambda: (SDTest.Call(0x0f8ca0, 'tbisDDDi', emoB + 0x1e4, 'OffModPos', 7.0, 8.0, 9.0,
	1)[0], SDTest.Call(0x0f8ca0, 'tbisDDDi', emoB + 0x1e4, 'OffModPos', 0.0, 0.0, 0.0, 0)))
check('cModMove_VarS(LinkModel get)', lambda: SDTest.Call(0x0f8bc0, 'tbispi', emoB + 0x1e4, 'LinkModel', emofn, 0)[0])
def emomodupd(e, dt):
	SDTest.Call(0x0f87d0, 'tvif', e + 0x1e4, dt)
	return (emofl(e + 0x94, 6), emofl(e + 0x1e4 + 0x34, 2))
check('cModMove_Update as pos', lambda: emomodupd(emoB, 0.1))
SDTest.Call(0x0f8550, 'tbispi', emoB + 0x1e4, 'OffModPosAsPos', 0, 1)
for emodt in (0.1, -1.0):
	check('cModMove_Update no link %s' % fmt(emodt), lambda emodt=emodt: emomodupd(emoB, emodt))

# a model on A: collision, effects, nodes, the link of B to its root node
emomodel = 'Models/Misc/Coins/Battery.M3D'
check('cObjEntity_SetModel', lambda: (SDTest.Call(0x0fc7b0, 'tvisfffi', emoA, emomodel, 1.0, 1.0, 1.0, 0),
	SDTest.Call(0x210da0, 'tbi', emoA + 0xb8)[0], SDTest.Call(0x20f7a0, 'tsi', emoA + 0xb8)[0], emofl(emoA + 0xf8, 3),
	emoi(emoA + 0x224) != 0))
emohas = SDTest.Call(0x210da0, 'tbi', emoA + 0xb8)[0] and SDTest.Call(0x20f7d0, 'tiis', emoA + 0xb8, 'DC_Root')[0] > 0
check('Battery DC_Root', lambda: emohas)
if emohas:
	emogets(emoA, 'A model')
	check('cObjEntity_UpdateModel', lambda: (SDTest.Call(0x0faaf0, 'tvi', emoA), SDTest.Call(0x20fa30, 'tiiF',
		emoA + 0xb8, (0.0, 0.0, 0.0))[1]))
	for emov in (1, 0):
		check('cObjEntity_VarI(A, MapCollision set %d)' % emov, lambda emov=emov: (SDTest.Call(0x0fb7d0, 'tbispi', emoA,
			'MapCollision', emov, 1)[0], SDTest.Call(0x0fb7d0, 'tbispi', emoA, 'MapCollision', -1, 0)))
	for emov in (2, 0):
		check('cObjEntity_VarI(A, RenderEffect set %d)' % emov, lambda emov=emov: (SDTest.Call(0x0fb7d0, 'tbispi', emoA,
			'RenderEffect', emov, 1)[0], SDTest.Call(0x0fb7d0, 'tbispi', emoA, 'RenderEffect', -1, 0)))
	check('cObjEntity_VarF(A, RenderEffectParam0 set)', lambda: (SDTest.Call(0x0fafd0, 'tbisDi', emoA,
		'RenderEffectParam0', 0.75, 1)[0], SDTest.Call(0x0fafd0, 'tbisDi', emoA, 'RenderEffectParam0', 0.0, 0)))
	check('cObjEntity_SetEffectParam', lambda: (SDTest.Call(0x0fc9e0, 'tviif', emoA, 0, 0.0),
		SDTest.Call(0x0fc9d0, 'tvii', emoA, 0), SDTest.Call(0x0fafd0, 'tbisDi', emoA, 'RenderEffectParam0', 0.0, 0)))
	check('cObjEntity_VarV(A, PvRot/PvPos set)', lambda: (SDTest.Call(0x0fd2c0, 'tbisDDDi', emoA, 'PvRot', 0.0, 0.0,
		0.0, 1)[0], SDTest.Call(0x0fd2c0, 'tbisDDDi', emoA, 'PvPos', 0.0, 0.0, 0.0, 1)[0]))
	check('cObjEntity_SetActive', lambda: map(lambda on: (SDTest.Call(0x0fc630, 'tvii', emoA, on), emob(emoA + 0x4c),
		SDTest.Call(0x210da0, 'tbi', emoA + 0xb8)[0]), (0, 1)))
	emolink = emocstr('SDObjA,DC_Root')
	emounlink = emocstr(',DC_Root')
	check('cModMove LinkModel', lambda: (SDTest.Call(0x0fd0a0, 'tbispi', emoB, 'LinkModel', emolink, 1)[0], emomod(emoB),
		emoi(emoA + 0x1e8) == emoB + 0x1f8, emoi(emoB + 0x1f8) == emoA + 0x1e8))
	for emov in ((0.25, 0.0, 0, 0.1), (0.5, 1.0, 1, 0.1), (0.5, 4.0, 1, 0.5), (1.0, 0.0, 0, 0.0)):
		check('cModMove_Update linked%s' % fmt(emov), lambda emov=emov: (SDTest.Call(0x0f8450, 'tbisDi', emoB + 0x1e4, 'OffMul',
			emov[0], 1), SDTest.Call(0x0f8450, 'tbisDi', emoB + 0x1e4, 'DeltaOffMul', emov[1], 1),
			SDTest.Call(0x0f8550, 'tbispi', emoB + 0x1e4, 'UseLinkAng', emov[2], 1), emomodupd(emoB, emov[3])))
	check('cObjEntity_Update B linked', lambda: emoupdate(emoB, 0x0fca80, 0.02, 0.01, 0.05))
	check('cModMove unlink', lambda: (SDTest.Call(0x0fd0a0, 'tbispi', emoB, 'LinkModel', emounlink, 1)[0], emomod(emoB),
		emoi(emoA + 0x1e8) == emoA + 0x1e8))
	check('cModMove_Link again', lambda: (SDTest.Call(0x0f8640, 'tviss', emoB + 0x1e4, 'SDObjA', 'DC_Root'),
		emomod(emoB)))
	# the effect of the Battery model needs its glow node (a missing one is a CHECK)
	if SDTest.Call(0x20f7d0, 'tiis', emoA + 0xb8, 'DOB_Batery_Glow')[0] >= 0:
		SDTest.Call(0x323c50, 'vi', 4321)
		check('cObjEntity_SetEmbeddedFX', lambda: (SDTest.Call(0x0fc770, 'tvis', emoA, 'SDNone'), emoi(emoA + 0x224) != 0))
		check('cObjEntity_Update A model', lambda: emoupdate(emoA, 0x0fca80, 0.02, 0.01, 0.05))
		check('cObjEntity_ReleaseFX', lambda: (SDTest.Call(0x0fafa0, 'tvi', emoA), emoi(emoA + 0x224)))
	emofree(emolink)
	emofree(emounlink)
# the follow camera of A
emocam = SDTest.Call(0x208360, 'tiisfffs', emoworld, 'SDObjCam', emox, emoy + 100.0, emoz_, 'FCam')[0]
check('FCam', lambda: emocam != 0)
if emocam:
	check('cCamEntity_SetTarget(A)', lambda: (SDTest.Call(0x0c07f0, 'tvii', emocam, emoA), emoi(emoA + 0xf0) == emocam))
	check('cObjEntity_VarS(A, CamName)', lambda: (lambda r: (r[0], emostr(r[1])))(SDTest.Call(0x0fd0a0, 'tbispi', emoA,
		'CamName', 0, 0)))
	check('cObjEntity_GetCamera cam', lambda: SDTest.Call(0x0fb570, 'tviFF', emoA, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
	check('cObjEntity_SetAng cam', lambda: (SDTest.Call(0x0fc6a0, 'tviF', emoA, (0.1, 0.2, 0.3)), emofl(emoA + 0x7c, 15)))

# the deleting destructor (after the name left the table, as the world does it)
def emodelete(e, slot):
	SDTest.Call(0x208810, 'tbis', emoworld + 4, emoname(e))
	return SDTest.Call(slot, 'tiii', e, 1)[0] == e
check('cObjEntity_Delete(A)', lambda: (emodelete(emoA, 0x0fa5e0), emocam and emoi(emocam + 0x13c),
	emoname(emoi(emoB + 0x200))))
if emocam:
	check('FCam delete', lambda: emodelete(emocam, emoslot(emocam, 0xc)))

# ---- cNetController on calloc memory with B ----
emonc = emoz(0x5c)
check('cNetController_ctor', lambda: (SDTest.Call(0x0fa1d0, 'tii', emonc)[0] == emonc, emoi(emonc) - 0x400000,
	emofl(emonc + 4, 13), emob(emonc + 0x38), emoints(emonc, (0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50)),
	emobytes(emonc, (0x54, 0x55, 0x56, 0x57, 0x58, 0x59))))
def emonet(nc):
	return (emofl(nc + 4, 13), emob(nc + 0x38), emoints(nc, (0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50)),
		emobytes(nc, (0x54, 0x55, 0x56, 0x57, 0x58, 0x59)))
def emobstate():
	return (emofl(emoB + 0x28, 3), emofl(emoB + 0xac, 3), emofl(emoB + 0xd4, 6))
SDTest.Call(0x0fc980, 'tviF', emoB, (emox, emoy, emoz_))
SDTest.Call(0x0fc6a0, 'tviF', emoB, (0.5, 1.0, 1.5))
SDTest.Call(0x0fca50, 'tviF', emoB, (5.0, 6.0, 7.0))
SDTest.Call(0x0fc740, 'tviF', emoB, (0.5, 0.6, 0.7))
for emov in (0x7e, 0x7e, 0x7e, 0x7f, 0x02, 0x00):
	check('cNetController_Changed(%x)' % emov, lambda emov=emov: (SDTest.Call(0x0fac90, 'tbiii', emonc, emov, emoB)[0], emonet(emonc)))
SDTest.Call(0x0fc980, 'tviF', emoB, (emox + 5.0, emoy, emoz_))
SDTest.Call(0x0fc6a0, 'tviF', emoB, (0.52, 1.2, 1.5))
for emov in (0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e):
	check('cNetController_Changed small', lambda emov=emov: (SDTest.Call(0x0fac90, 'tbiii', emonc, emov, emoB)[0], emonet(emonc)))
check('cNetController_Reset', lambda: (SDTest.Call(0x0fc600, 'tvi', emonc), emonet(emonc)))
check('cNetController_Changed after Reset', lambda: (SDTest.Call(0x0fac90, 'tbiii', emonc, 0x7e, emoB)[0], emonet(emonc)))
# Apply: a fresh state (age 0.5) on a client, the smoothing distances of the engine; a server; an old state
emoapp = SDTest.Peek(0x467ba0, 'i')
emosavedflags = emoi(emoapp + 0x30)
def emoapply(pos, flags, age, dt, move, appflags):
	emoset(emonc + 4, (age,))
	emoset(emonc + 8, pos + (0.6, 1.3, 1.6, 8.0, 9.0, 10.0, 0.1, 0.2, 0.3))
	SDTest.Poke(emonc + 0x38 - 0x400000, flags)
	SDTest.Call(0x0fc980, 'tviF', emoB, (emox, emoy, emoz_))
	SDTest.Call(0x0fc6a0, 'tviF', emoB, (0.5, 1.0, 1.5))
	SDTest.Call(0x0fca50, 'tviF', emoB, (5.0, 6.0, 7.0))
	SDTest.Call(0x0fc740, 'tviF', emoB, (0.5, 0.6, 0.7))
	emopoke(emoapp + 0x30, appflags)
	SDTest.Call(0x0fa610, 'tviifi', emonc, emoB, dt, move)
	emopoke(emoapp + 0x30, emosavedflags)
	return (emobstate(), emofl(emonc + 4, 1))
for emod in (0.0, 1.0, 50.0, 200.0, 1000.0, 100000.0):
	check('cNetController_Apply client %s' % fmt(emod), lambda emod=emod: emoapply((emox + emod, emoy - emod, emoz_ + 0.5 * emod), 0x7e, 0.5,
		0.02, 1, 0))
check('cNetController_Apply client part', lambda: emoapply((emox + 10.0, emoy, emoz_), 0x26, 0.5, 0.02, 1, 0))
check('cNetController_Apply server', lambda: emoapply((emox + 10.0, emoy, emoz_), 0x7e, 0.5, 0.02, 1, 2))
check('cNetController_Apply server part', lambda: emoapply((emox + 10.0, emoy, emoz_), 0x52, 0.5, 0.02, 1, 2))
for emov in ((0.4, 1), (0.4, 0), (0.0, 1), (-0.1, 0), (0.01, 1)):
	check('cNetController_Apply old %s' % fmt(emov), lambda emov=emov: emoapply((emox + 10.0, emoy, emoz_), 0x7e, emov[0], 0.02, emov[1],
		0))
# Read from a memory file: packed and full messages
emobuf = emoz(64)
emofm = emoz(0x20)
SDTest.Call(0x12e050, 'tiiiii', emofm, emobuf, 64, 0)
def emoread(bs, floats):
	emobytesto(emobuf, bs)
	for o, v in floats:
		emoset(emobuf + o, v)
	SDTest.Call(0x12e1b0, 'tiisi', emofm, '', 0)
	SDTest.Call(0x0fbb20, 'tvii', emonc, emofm)
	return (emonet(emonc), emoi(emofm + 0x10) - emobuf)
emopacked = [0x7e] + emoshort(100) + emoshort(-20) + emoshort(5) + emobyte(64) + emobyte(-100) + emobyte(127) + \
	emoshort(300) + emoshort(-1) + emoshort(0) + emobyte(10) + emobyte(-10) + emobyte(3)
check('cNetController_Read packed', lambda: emoread(emopacked, ()))
emofull = [0x7f] + [0] * 12 + emoshort(16000) + emoshort(-32000) + emoshort(100) + [0] * 12 + emoshort(1000) + \
	emoshort(-5) + emoshort(77)
check('cNetController_Read full', lambda: emoread(emofull, ((1, (emox, emoy, emoz_)), (19, (1.5, -2.5, 1e5)))))
check('cNetController_Read pos', lambda: emoread([0x03], ((1, (1e9, -1e9, 0.0)),)))
# Write: the state of B packed or full into the empty file, then read back
def emowrite(flags, force, pos, ang, vel, rot):
	SDTest.Call(0x0fc980, 'tviF', emoB, pos)
	SDTest.Call(0x0fc6a0, 'tviF', emoB, ang)
	SDTest.Call(0x0fca50, 'tviF', emoB, vel)
	SDTest.Call(0x0fc740, 'tviF', emoB, rot)
	emobytesto(emobuf, [0] * 40)
	SDTest.Call(0x12e1b0, 'tiisi', emofm, '', 4)
	SDTest.Call(0x0fbef0, 'tviiiii', emonc, flags, emoB, emofm, force)
	n = emoi(emofm + 0x10) - emobuf
	r = (n, emobytes(emobuf, range(n)), emonet(emonc))
	SDTest.Call(0x12e1b0, 'tiisi', emofm, '', 0)
	SDTest.Call(0x0fbb20, 'tvii', emonc, emofm)
	return (r, emonet(emonc))
for emov in ((0x7e, 1, (emox, emoy, emoz_), (0.5, -2.0, 3.0), (100.0, -50.0, 3.0), (0.5, -1.0, 2.0)),
		(0x7f, 1, (emox, emoy, emoz_), (0.5, -2.0, 3.0), (100.0, -50.0, 3.0), (0.5, -1.0, 2.0)),
		(0x7e, 1, (1e7, -1e7, 0.0), (7.0, -7.0, 100.0), (40000.0, 30000.0, -1.0), (20.0, -20.0, 0.05)),
		(0x7f, 1, (1e7, -1e7, 0.0), (7.0, -7.0, 100.0), (40000.0, 30000.0, -1.0), (300000.0, -300000.0, 0.05)),
		(0x7e, 0, (emox, emoy, emoz_), (0.5, -2.0, 3.0), (100.0, -50.0, 3.0), (0.5, -1.0, 2.0)),
		(0x7e, 0, (emox, emoy, emoz_), (0.5, -2.0, 3.0), (100.0, -50.0, 3.0), (0.5, -1.0, 2.0)),
		(0x26, 1, (emox, emoy, emoz_), (3.1, 3.2, -3.3), (1.0, 1.0, 1.0), (0.001, -0.001, 0.0)),
		(0x01, 1, (emox, emoy, emoz_), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))):
	check('cNetController_Write%s' % fmt(emov[:2]), lambda v=emov: emowrite(v[0], v[1], v[2], v[3], v[4], v[5]))
emofree(emofm)
emofree(emobuf)
emofree(emonc)

# ---- two WithLife entities: cVehicleAttack, cPrimitive, cArrowIndicator ----
emoW1 = SDTest.Call(0x208360, 'tiisfffs', emoworld, 'SDLifeA', emox, emopp[1] + 20000.0, emoz_, 'WithLife')[0]
emoW2 = SDTest.Call(0x208360, 'tiisfffs', emoworld, 'SDLifeB', emox + 150.0, emopp[1] + 20000.0, emoz_ + 80.0,
	'WithLife')[0]
check('WithLife', lambda: (emoW1 != 0, emoW2 != 0, emoW1 and emoistype(emoW1, 'WithLife')))
if emoW1 and emoW2:
	# their parts after the constructors
	def emoatt(a):
		return (emoname(emoi(a + 8)), emoname(emoi(a + 0x18)), emoname(emoi(a + 0x28)), emofl(a + 0x30, 3), emoi(a + 0x3c) != 0,
			emoi(a + 0x40), emofl(a + 0x44, 3), emob(a + 0x50), emoints(a, (0x54, 0x58)), emob(a + 0x5c),
			emoname(emoi(a + 0x68)), emofl(a + 0x70, 1), emob(a + 0x74), emofl(a + 0x78, 1))
	def emoarrow(a):
		return (emoi(a) != 0, emoints(a, (4, 8, 0xc)), emob(a + 0x10), emofl(a + 0x14, 1), emob(a + 0x18),
			emoints(a, (0x1c, 0x20)), map(lambda i, a=a: emostr(emoi(a + 0x24 + 4 * i)), range(8)),
			emobytes(a, (0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71)), emoints(a, (0x74, 0x78)), emofl(a + 0x7c, 2),
			emobytes(a, (0x84, 0x85, 0x86, 0x87)))
	for emoe in (emoW1, emoW2):
		check('WithLife parts %s' % emoname(emoe), lambda emoe=emoe: (emoatt(emoe + 0x298), emoarrow(emoe + 0x314),
			emoi(emoe + 0x39c) == emoe, emofl(emoe + 0x3a0, 1), emob(emoe + 0x3a4)))

	# cVehicleAttack in calloc memory, owner W1
	emova = emoz(0x7c)
	check('cVehicleAttack_ctor', lambda: (SDTest.Call(0x0f9920, 'tii', emova)[0] == emova, emoatt(emova)))
	emopoke(emova + 0x3c, emoW1)
	check('cVehicleAttack_GetPoint', lambda: SDTest.Call(0x0f9b40, 'tii', emova)[0] == emova + 0x30)
	check('cVehicleAttack_VarI', lambda: SDTest.Call(0x0f9b50, 'tbispi', emova, 'Target', 1, 0))
	check('cHitInfo_GetName none', lambda: SDTest.Call(0x0f9aa0, 'tsi', emova)[0])
	check('cVehicleAttack_ShotAt(SDLifeB, 2)', lambda: (SDTest.Call(0x0f9c10, 'tvisi', emova, 'SDLifeB', 2), emoatt(emova),
		emoatt(emoW2 + 0x298)))
	check('cVehicleAttack_ShotAt(SDObjB, 1)', lambda: (SDTest.Call(0x0f9c10, 'tvisi', emova, 'SDObjB', 1), emoatt(emova)))
	check('cVehicleAttack_ShotAt(none)', lambda: (SDTest.Call(0x0f9c10, 'tvisi', emova, 'SDNobody', 1),
		SDTest.Call(0x0f9c10, 'tvisi', emova, '', 1), emoatt(emova)))
	check('cVehicleAttack_ShotAtPoint', lambda: (SDTest.Call(0x0f9d00, 'tviFi', emova, (1.0, 2.0, 3.0), 3), emoatt(emova)))
	check('cHitInfo_GetName', lambda: SDTest.Call(0x0f9aa0, 'tsi', emova)[0])
	check('cVehicleAttack_HitBy', lambda: map(lambda n: (SDTest.Call(0x0f9b60, 'tvis', emova, n), emoatt(emova)),
		('SDLifeB', 'SDLifeA', 'SDNobody', 'SDLifeB')))
	emofnb = emocstr('SDLifeB')
	check('cVehicleAttack_VarS', lambda: (SDTest.Call(0x0fa020, 'tbispi', emova, 'Target', emofnb, 1)[0],
		(lambda r: (r[0], emostr(r[1])))(SDTest.Call(0x0fa020, 'tbispi', emova, 'Target', 0, 0)),
		SDTest.Call(0x0fa020, 'tbispi', emova, 'Target', emoempty, 1)[0], SDTest.Call(0x0fa020, 'tbispi', emova, 'Bogus', 0,
		0)[0], emoatt(emova)))
	check('cVehicleAttack_VarV', lambda: (SDTest.Call(0x0fa0c0, 'tbisDDDi', emova, 'Pos', 4.0, 5.0, 6.0, 1)[0],
		SDTest.Call(0x0fa0c0, 'tbisDDDi', emova, 'Pos', 0.0, 0.0, 0.0, 0), SDTest.Call(0x0fa0c0, 'tbisDDDi', emova, 'Po',
		0.0, 0.0, 0.0, 0)[0], emoatt(emova)))
	check('cVehicleAttack_Update not a car', lambda: (SDTest.Call(0x0f9d90, 'tvi', emova), emoatt(emova)))
	check('cVehicleAttack_NetChanged', lambda: map(lambda i: (SDTest.Call(0x0f9ab0, 'tbip', emova, 0), emoatt(emova)),
		range(3)))
	check('cVehicleAttack_NetChanged NULL', lambda: (SDTest.Call(0x0f9ab0, 'tbii', emova, 0)[0], emoatt(emova)))
	check('cVehicleAttack_NetFlags', lambda: map(lambda f: (SDTest.Call(0x0fa1a0, 'tiiip', emova, f, 7), emoatt(emova)),
		(0x93, 0x93, 0x05, 0x00)))
	for emot in (100.0, 99.0):
		emopoke(emova + 0x78, 0)
		check('cVehicleAttack_Expire %s' % fmt(emot), lambda emot=emot: (emoset(emowt + 8, (emot,)), SDTest.Call(0x0f9a00, 'tvi', emova),
			emoatt(emova)))
	emoset(emowt + 8, (100.0,))
	check('cVehicleAttack_Expire later', lambda: (SDTest.Call(0x0f9a00, 'tvi', emova), emoatt(emova)))
	# the target and the attacker unlinked before the memory goes
	SDTest.Call(0x0f9d00, 'tviFi', emova, (0.0, 0.0, 0.0), 0)
	SDTest.Call(0x0f9b60, 'tvis', emova, 'SDLifeA')
	SDTest.Call(0x0ef6e0, 'vis', emova + 0x10, '')
	SDTest.Call(0x0ef6e0, 'vis', emova + 0x20, '')
	emofree(emova)
	emofree(emofnb)

	# cPrimitive in calloc memory with the owner W1, and the sphere of W2
	emopr = emoz(0xc)
	emopr2 = emoW2 + 0x39c
	check('cPrimitive_ctor', lambda: (SDTest.Call(0x0f9290, 'tii', emopr)[0] == emopr, emoints(emopr, (0, 4, 8))))
	check('cPrimitive_SetOwner', lambda: (SDTest.Call(0x0f9420, 'tvii', emopr, emoW1), emoi(emopr) == emoW1))
	check('cPrimitive_SetOn no size', lambda: (SDTest.Call(0x0f9400, 'tvii', emopr, 1), emob(emopr + 8)))
	for emor in (0.0, 50.0):
		check('cPrimitive_SetSize %s' % fmt(emor), lambda emor=emor: (SDTest.Call(0x0f9430, 'tvif', emopr, emor), emofl(emopr + 4, 1),
			emob(emopr + 8)))
	check('cPrimitive_SetOn', lambda: map(lambda on: (SDTest.Call(0x0f9400, 'tvii', emopr, on), emob(emopr + 8)), (0, 1)))
	check('cPrimitive_VarF', lambda: (SDTest.Call(0x0f92b0, 'tbisDi', emopr, 'VRadius', 0.0, 0),
		SDTest.Call(0x0f92b0, 'tbisDi', emopr, 'VRadius', 80.0, 1)[0], emofl(emopr + 4, 1), SDTest.Call(0x0f92b0, 'tbisDi',
		emopr, 'VRadiu', 0.0, 0)[0], SDTest.Call(0x0f92b0, 'tbisDi', emopr, '', 0.0, 0)[0]))
	emow1 = SDTest.Call(0x0ef330, 'tiiF', emoW1, (0.0, 0.0, 0.0))[1]
	def emoseg(f, a, b):
		return SDTest.Call(f, 'tbiFF', emopr, a, b)
	for emod in ((0.0, 200.0), (79.0, 200.0), (81.0, 200.0), (0.0, 50.0)):
		emosa = (emow1[0] - emod[1], emow1[1] + emod[0], emow1[2])
		emosb = (emow1[0] + emod[1], emow1[1] + emod[0], emow1[2] + 1.0)
		check('cPrimitive_TestSegment%s' % fmt(emod), lambda a=emosa, b=emosb: (emoseg(0x0f9340, a, b),
			emoseg(0x0f9370, a, b)))
	emotesting = emoi(emoworld + 0x284)
	emopokeb(emoworld + 0x284, 1)
	for emod in ((0.0, 0.0, 10.0), (100.0, 0.0, 10.0), (89.0, 0.0, 10.0), (91.0, 0.0, 10.0), (0.5, 0.2, 1.0)):
		emosc = (emow1[0] + emod[0], emow1[1] + emod[1], emow1[2])
		check('cPrimitive_TestSphere%s' % fmt(emod), lambda c=emosc, d=emod: (SDTest.Call(0x0f94b0, 'tbiiFfi', emopr,
			emoW2, c, d[2], 0)[0], SDTest.Call(0x0f94f0, 'tbiiFfi', emopr, emoW2, c, d[2], 0)[0],
			SDTest.Call(0x0f94f0, 'tbiiFfF', emopr, emoW2, c, d[2], (c[0] + 500.0, c[1], c[2]))[0]))
	emopoke(emoworld + 0x284, emotesting)
	check('cPrimitive_SphereHit no other', lambda: SDTest.Call(0x0f94f0, 'tbiiFfi', emopr, 0, emow1, 1.0, 0)[0])
	SDTest.Call(0x0f9430, 'tvif', emopr2, 30.0)
	for emod in ((10.0, 0.0, 0.0, 0), (0.0, 0.0, 0.0, 0), (3.0, 40.0, 4.0, 0), (3.0, 40.0, 4.0, 1), (0.0, 0.0, 0.0, 1),
			(3.0, 200.0, 4.0, 1)):
		emoso = (emow1[0] + emod[0], emow1[1] + emod[1], emow1[2] + emod[2])
		check('cPrimitive_UnelasticDisp%s' % fmt(emod), lambda o=emoso, d=emod: SDTest.Call(0x0f96f0, 'tviFFii', emopr,
			o, emow1, emopr2, d[3]))
	SDTest.Call(0x0f9430, 'tvif', emopr2, 0.0)
	check('cPrimitive_UnelasticDisp no sphere', lambda: SDTest.Call(0x0f96f0, 'tviFFii', emopr, emow1, emow1, emopr2, 0))
	emofree(emopr)

	# cArrowIndicator in calloc memory (parent W1) and the arrows of W1 and W2
	emoar = emoz(0x88)
	check('cArrowIndicator_ctor', lambda: (SDTest.Call(0x0f5f40, 'tii', emoar)[0] == emoar, emoarrow(emoar)))
	check('cArrowIndicator_SetParent', lambda: (SDTest.Call(0x0f74f0, 'tvii', emoar, emoW1), emoi(emoar) == emoW1))
	for emos in ('Opt0:first', 'nocolon', 'Opt1:', ':third', 'a:b:c', 'x:5', 'x:6', 'x:7', 'x:8', 'x:9'):
		check('cArrowIndicator_AddOpt(%s)' % emos, lambda emos=emos: (SDTest.Call(0x0f60f0, 'tbis', emoar, emos)[0], emoi(emoar + 0x1c)))
	check('cArrowIndicator_GetOpt', lambda: map(lambda i: SDTest.Call(0x0f6b00, 'tsii', emoar, i)[0], (-1, 0, 3, 7, 8)))
	check('cArrowIndicator_SetOpt', lambda: map(lambda i: SDTest.Call(0x0f74b0, 'tbiis', emoar, i, 'Set:%d' % i)[0],
		(-1, 1, 8, 9)))
	check('cArrowIndicator_SetIOpt', lambda: map(lambda i: (SDTest.Call(0x0f7710, 'tbii', emoar, i)[0], emoi(emoar + 0x20)),
		(-1, 2, 8, 7)))
	check('cArrowIndicator_NextOpt', lambda: map(lambda i: (SDTest.Call(0x0f6fd0, 'tvi', emoar), emoi(emoar + 0x20)), range(3)))
	check('cArrowIndicator_PrevOpt', lambda: map(lambda i: (SDTest.Call(0x0f7090, 'tvi', emoar), emoi(emoar + 0x20)), range(3)))
	for emov in ('CanPhoto', 'AlwaysUse', 'ArrowCanView', 'ArrowCanMission', 'ArrowCanUse', 'ArrnOpt', 'ArriOpt', 'UseAngSel',
			'UseMapCheck', 'UseAbsAngSel', 'Bogus', ''):
		check('cArrowIndicator_VarI(%s)' % emov, lambda emov=emov: (SDTest.Call(0x0f6b80, 'tbispi', emoar, emov, -5, 0),
			SDTest.Call(0x0f6b80, 'tbispi', emoar, emov, 2, 1)[0], SDTest.Call(0x0f6b80, 'tbispi', emoar, emov, -5, 0)))
	for emov in (('ArriOpt', 1), ('ArriOpt', 20), ('ArrnOpt', 3), ('ArrowCanMission', 0)):
		check('cArrowIndicator_VarI(%s set %d)' % emov, lambda emov=emov: (SDTest.Call(0x0f6b80, 'tbispi', emoar, emov[0], emov[1], 1)[0],
			emoarrow(emoar)))
	for emov in (('ConeDist', 1500.0), ('ConeFOV', 45.0), ('Bogus', 1.0)):
		check('cArrowIndicator_VarF(%s)' % emov[0], lambda emov=emov: (SDTest.Call(0x0f6910, 'tbisDi', emoar, emov[0], 0.0, 0),
			SDTest.Call(0x0f6910, 'tbisDi', emoar, emov[0], emov[1], 1)[0], SDTest.Call(0x0f6910, 'tbisDi', emoar, emov[0], 0.0, 0)))
	def emoars(n):
		return (lambda r: (r[0], r[0] and emostr(r[1])))(SDTest.Call(0x0f7d60, 'tbispi', emoar, n, 0, 0))
	emostrs = {}
	for emos in ('relax', 'ALERT', 'danger', 'off', 'bogus', 'killer', 'green', 'Red', 'blue', 'none', 'Opt9:x', '__main__.emocb'):
		emostrs[emos] = emocstr(emos)
	for emon, emos in (('ConeStatus', 'relax'), ('ConeStatus', 'ALERT'), ('ConeStatus', 'danger'), ('ConeStatus', 'off'),
			('ConeStatus', 'bogus'), ('TgType', 'killer'), ('TgType', 'green'), ('TgType', 'Red'), ('TgType', 'blue'),
			('TgType', 'none'), ('TgType', 'bogus'), ('AddArrOpt', 'Opt9:x'), ('ArrOpt0', 'Opt9:x'), ('ArrOpt8', 'Opt9:x'),
			('ArrOpt9', 'Opt9:x'), ('ArrOptx', 'Opt9:x'), ('Bogus', 'off')):
		check('cArrowIndicator_VarS(%s set %s)' % (emon, emos), lambda emon=emon, emos=emos: (SDTest.Call(0x0f7d60, 'tbispi', emoar, emon,
			emostrs[emos], 1)[0], emoars(emon), SDTest.Call(0x0f6b20, 'tii', emoar)[0], emoarrow(emoar)))
	for emot in (0, 1, 2, 3, 4, 5, -1):
		emopoke(emoar + 0x74, emot)
		emopoke(emoar + 0x78, emot)
		check('cArrowIndicator colors %d' % emot, lambda: (SDTest.Call(0x0f6b20, 'tii', emoar)[0], emoars('TgType'),
			emoars('ConeStatus')))
	for emov in ('ArrOpt0', 'ArrOpt3', 'ArrOpt8', 'AddArrOpt', 'UseFunc'):
		check('cArrowIndicator_VarS(%s)' % emov, lambda emov=emov: emoars(emov))
	check('cArrowIndicator_SetShown', lambda: (SDTest.Call(0x0f7270, 'tvii', emoar, 1), emob(emoar + 0x18),
		SDTest.Call(0x0f7270, 'tvii', emoar, 0), emob(emoar + 0x18)))
	# the use and view functions with the options
	check('cArrowIndicator_VarS(UseFunc set)', lambda: SDTest.Call(0x0f7d60, 'tbispi', emoar, 'UseFunc',
		emostrs['__main__.emocb'], 1)[0])
	check('cArrowIndicator_SetViewFunc', lambda: SDTest.Call(0x0f7490, 'tvis', emoar, '__main__.emocb'))
	def emoexec(canuse, iopt):
		emopokeb(emoar + 0x6d, canuse)
		emopoke(emoar + 0x20, iopt)
		SDTest.Call(0x0f6160, 'tvii', emoar, emoplayer)
		r = emocalls[:]
		del emocalls[:]
		return r
	check('cArrowIndicator_Execute', lambda: map(lambda v: emoexec(v[0], v[1]), ((1, 0), (0, 1), (1, 2), (0, 4))))
	check('cArrowIndicator_ClearOpts', lambda: (SDTest.Call(0x0f65d0, 'tvi', emoar), emoarrow(emoar)))
	check('cArrowIndicator_Execute no opts', lambda: emoexec(1, 0))
	check('cArrowIndicator_SetUseFunc empty', lambda: SDTest.Call(0x0f7470, 'tvis', emoar, ''))
	check('cArrowIndicator_dtor', lambda: SDTest.Call(0x0f6040, 'tvi', emoar))
	emofree(emoar)
	for emos in emostrs.keys():
		emofree(emostrs[emos])

	# the selection: W1 looks at W2 (both with spheres and arrows)
	SDTest.Call(0x0f9430, 'tvif', emoW1 + 0x39c, 20.0)
	SDTest.Call(0x0f9430, 'tvif', emoW2 + 0x39c, 30.0)
	SDTest.Call(0x0f6b80, 'tbispi', emoW2 + 0x314, 'ArrowCanUse', 1, 1)
	emopw1 = SDTest.Call(0x0ef330, 'tiiF', emoW1, (0.0, 0.0, 0.0))[1]
	emopw2 = SDTest.Call(0x0ef330, 'tiiF', emoW2, (0.0, 0.0, 0.0))[1]
	emoang = SDTest.Call(0x0f6ff0, 'iFF', (0.0, 0.0), (emopw2[0] - emopw1[0], emopw2[2] - emopw1[2]))[1]
	check('Vec2_Normalize', lambda: map(lambda v: SDTest.Call(0x0f6ff0, 'iFF', (9.0, 9.0), v)[1], ((3.0, 4.0), (0.0, 0.0),
		(-1e-20, 0.0), (1e30, 1e30), (0.5, -0.25))))
	def emovis(a, b, ang, mx, mark, useang):
		r = SDTest.Call(0x0f6270, 'biiffFii', a, b, ang, mx, (-1.0,), mark, useang)
		return (r, emob(b + 0x314 + 0x18))
	for emov in ((0.0, 5000.0, 0, 0), (0.0, 5000.0, 1, 1), (1.0, 5000.0, 1, 1), (2.5, 5000.0, 1, 1), (-1.3, 5000.0, 1, 1),
			(0.0, 100.0, 1, 0), (0.0, 120.0, 1, 0), (3.14, 5000.0, 0, 1)):
		check('ArrowIndicator_Visible%s' % fmt(emov), lambda emov=emov: emovis(emoW1, emoW2, emov[0], emov[1], emov[2], emov[3]))
	for emov in (('UseAngSel', 0), ('UseAbsAngSel', 1), ('UseMapCheck', 0)):
		SDTest.Call(0x0f6b80, 'tbispi', emoW2 + 0x314, emov[0], emov[1], 1)
		check('ArrowIndicator_Visible W2 %s %d' % emov, lambda: map(lambda a: emovis(emoW1, emoW2, a, 5000.0, 1, 1),
			(0.0, 1.0, 2.0, -2.0)))
	check('ArrowIndicator_Visible self', lambda: emovis(emoW1, emoW1, 0.0, 5000.0, 1, 1))
	def emofind(a, ang):
		r = SDTest.Call(0x0f6610, 'iif', a, ang)[0]
		return (emoname(r), emobytes(emoW1 + 0x314, (0x6c, 0x18)), emobytes(emoW2 + 0x314, (0x6c, 0x18)))
	check('ArrowIndicator_FindSel', lambda: map(lambda a: emofind(emoW1, a), (0.0, 1.5, -2.0)))
	check('ArrowIndicator_ClearSel', lambda: (SDTest.Call(0x0f6580, 'v'), emobytes(emoW2 + 0x314, (0x6c, 0x18))))

	# cAutoAim in calloc memory: the owner and target are the Obj B and the WithLife W2
	emoaa = emoz(0x50)
	def emoaim(a):
		return (emofl(a, 5), emoname(emoi(a + 0x1c)), emofl(a + 0x24, 9), emoi(a + 0x48), emob(a + 0x4c))
	check('cAutoAim_ctor', lambda: (SDTest.Call(0x0fda70, 'tii', emoaa)[0] == emoaa, emoaim(emoaa),
		emoi(emoaa + 0x20) == emoaa))
	for emov in (('X', 0.0), ('Y', 0.0), ('Speed', 90.0), ('DistLimit', 15000.0), ('HiAng', 20.0), ('DeltaAng', 5.0),
			('LoAng', 2.0), ('AngLimit', 10.0), ('Bogus', 1.0)):
		check('cAutoAim_VarF(%s)' % emov[0], lambda emov=emov: (SDTest.Call(0x0fdb20, 'tbisDi', emoaa, emov[0], 0.0, 0),
			SDTest.Call(0x0fdb20, 'tbisDi', emoaa, emov[0], emov[1], 1)[0], SDTest.Call(0x0fdb20, 'tbisDi', emoaa, emov[0], 0.0, 0),
			emoaim(emoaa)))
	check('cAutoAim_SetAngle', lambda: (SDTest.Call(0x0fe780, 'tvif', emoaa, 0.3), emoaim(emoaa)))
	check('cAutoAim_SetRange', lambda: (SDTest.Call(0x0fe7b0, 'tvif', emoaa, 12345.0), emoaim(emoaa)))
	emoaas = {}
	for emos in ('Select', 'off', 'KEEP', 'Magnet', 'search', 'Bogus'):
		emoaas[emos] = emocstr(emos)
	for emos in ('Select', 'off', 'KEEP', 'Magnet', 'search', 'Bogus'):
		check('cAutoAim_VarS(Status %s)' % emos, lambda emos=emos: (SDTest.Call(0x0fe7d0, 'tbispi', emoaa, 'Status', emoaas[emos], 1)[0],
			emoi(emoaa + 0x48), (lambda r: (r[0], emostr(r[1])))(SDTest.Call(0x0fe7d0, 'tbispi', emoaa, 'Status', 0, 0))))
	check('cAutoAim_VarS(Target)', lambda: ((lambda r: (r[0], emostr(r[1])))(SDTest.Call(0x0fe7d0, 'tbispi', emoaa,
		'Target', 0, 0)), SDTest.Call(0x0fe7d0, 'tbispi', emoaa, 'Target', emoaas['off'], 1)[0],
		SDTest.Call(0x0fe7d0, 'tbispi', emoaa, 'Bogus', 0, 0)[0]))
	check('cAutoAim_SetSlot', lambda: (SDTest.Call(0x0fde50, 'tvifF', emoaa, 0.1, (0.05, -0.02)), emoaim(emoaa),
		SDTest.Call(0x0fde50, 'tvifi', emoaa, 0.2, 0), emoaim(emoaa)))
	def emoaimpos(status, now):
		emopoke(emoaa + 0x48, status)
		emoset(emowt + 8, (now,))
		r = SDTest.Call(0x0fdea0, 'tiiFi', emoaa, (0.0, 0.0, 0.0), emoB)
		emoset(emowt + 8, (100.0,))
		return (r[0] != 0, r[1])
	for emov in ((0, 100.0), (0, 100.05), (1, 100.3), (2, 100.1), (4, 100.1)):
		check('cAutoAim_GetAimPos%s' % fmt(emov), lambda emov=emov: emoaimpos(emov[0], emov[1]))
	SDTest.Call(0x0c9a20, 'tvii', emoW2, emoaa + 0x14)
	for emov in ((2, 100.1), (3, 100.1), (4, 100.1), (4, 100.3), (1, 100.1)):
		check('cAutoAim_GetAimPos target%s' % fmt(emov), lambda emov=emov: emoaimpos(emov[0], emov[1]))
	SDTest.Call(0x0ef6e0, 'vis', emoaa + 0x14, '')
	emofree(emoaa)
	for emos in emoaas.keys():
		emofree(emoaas[emos])

	check('cObjEntity_Delete(W1, W2)', lambda: (emodelete(emoW1, emoslot(emoW1, 0xc)), emodelete(emoW2, emoslot(emoW2,
		0xc))))
check('cObjEntity_Delete(B)', lambda: emodelete(emoB, 0x0fa5e0))

# ---- the controls of a vehicle in calloc memory, a fake vehicle with a network state ----
emoct = emoz(0x48)
check('Controls_ctor', lambda: (SDTest.Call(0x0ff8a0, 'tii', emoct)[0] == emoct, emoints(emoct, (0, 0x18, 0x1c, 0x2c, 0x30)),
	emofl(emoct + 4, 5), emobytes(emoct, range(0x20, 0x29)), emofl(emoct + 0x34, 4)))
for emov in (('Acc', 0.5), ('VStr', -1.0), ('Str', 0.25), ('HRot', 2.0), ('VRot', -0.75), ('Bogus', 1.0)):
	check('Controls_VarF(%s)' % emov[0], lambda emov=emov: (SDTest.Call(0x0ff940, 'tbisDi', emoct, emov[0], emov[1], 1)[0],
		SDTest.Call(0x0ff940, 'tbisDi', emoct, emov[0], 0.0, 0)))
check('Controls_SetWeapon', lambda: map(lambda w: (emopoke(emoct + 0x20, 0x01010101), emopoke(emoct + 0x18, 7),
	SDTest.Call(0x0ffc40, 'tvii', emoct, w), SDTest.Call(0x0ffb90, 'tii', emoct)[0], emobytes(emoct, range(0x20, 0x24)),
	emoi(emoct + 0x18)), (3, 3, 0)))
for emov in ((1.0, 0.5), (0.5, 0.0), (2.0, -1.0), (0.3, 0.01)):
	check('Controls_Rumble%s' % fmt(emov), lambda emov=emov: (SDTest.Call(0x0ffc60, 'tviff', emoct, emov[0], emov[1]),
		SDTest.Call(0x0ffc90, 'tviff', emoct, emov[0] * 2.0, emov[1] * 2.0), emofl(emoct + 0x34, 4)))
emoveh = emoz(0x100)
emonet_ = emoz(0x80)
emopoke(emoveh + 0xf4, emonet_)
for emobits in (0x0001, 0x8001, 0x8000, 0x0102, 0x0a00, 0x0600, 0x0300, 0x8d03):
	for emoprev in (0, 1):
		check('Controls_FromNet(%x, %d)' % (emobits, emoprev), lambda emobits=emobits, emoprev=emoprev: (emopoke(emonet_ + 0x74, emobits),
			emopokeb(emoct + 0x20, emoprev), SDTest.Call(0x0ffba0, 'tviif', emoct, emoveh, 0.1),
			emobytes(emoct, range(0x20, 0x28)), emofl(emoct + 0xc, 2)))
check('Controls_Reset', lambda: (SDTest.Call(0x0ff8c0, 'tvi', emoct), emoints(emoct, (0, 0x18)), emofl(emoct + 4, 5),
	emobytes(emoct, range(0x20, 0x29)), emofl(emoct + 0x34, 4)))
emofree(emoveh)
emofree(emonet_)
emofree(emoct)

check('GameModelList_Free', lambda: (SDTest.Call(0x0f6500, 'v'), SDTest.Peek(0x4479a8, 'i'), SDTest.Peek(0x4479ac, 'i')))
emofree(emofn)
emofree(emoempty)
emofree(emonr)
emopoke(emowt + 8, emosavedwt[0])
emopoke(emowt + 0xc, emosavedwt[1])
emopoke(emowt + 0x14, emosavedwt[2])
check('entmore world time restored', lambda: emoints(emowt, (8, 0xc, 0x14)) == emosavedwt)
