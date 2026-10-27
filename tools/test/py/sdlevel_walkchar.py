# game Entities/Char/WalkCharEntity.c, the rest of WalkCharEntity.cpp (execfile'd by sdlevel.py, Python 1.5.2).
# A WalkChar of our own made by cWorld_CreateEntity above the atmosphere limit (rand seeded), the coin model on it,
# and a fake action controller in calloc memory: the class a copy of the one of the player (its actions shared, CanFly
# set so that SetFalling changes nothing), the action of layer 0 a fake one with the fields set by hand, the layers
# off (the action names are "", nothing is active). So no action is played (an action needs the model of its class).
# The walk of the class without CanFly comes last: its floor test far above the map must find nothing. The read-only
# helpers also on Player0. The sound item 0 of our entity is a fake one while the loop sounds are tested. Not called:
# the AI made by EnableAI, the Update (its WithLife part ticks the slots by the real time), the model part of
# SetModel (the coin has no DC_Floor), SetFalling with a change and the walk, run, back and strafe actions (they play
# actions), SetEmbeddedFX (it loads effects).
def wcei(a):
	return SDTest.Peek(a, '@i')
def wceb(a):
	return SDTest.Peek(a, '@b')
def wcepoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def wcepokeb(a, v):
	wcepoke(a, (wcei(a) & ~0xff) | v)
def wcefl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def wceset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def wcez(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def wcefree(p):
	SDTest.Call(0x133550, 'ii', p)
def wcestr(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def wcebytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
# a string in calloc memory (ASCII, 4 characters an int)
def wcetext(s):
	p = wcez(len(s) + 8)
	j = 0
	while j < len(s):
		v = 0
		k = 0
		while k < 4 and j + k < len(s):
			v = v | (ord(s[j + k]) << (8 * k))
			k = k + 1
		wcepoke(p + j, v)
		j = j + 4
	return p

wceworld = SDTest.Peek(0x467450, 'i')
wceplayer = SDTest.Call(0x0c1430, 'tiii', wceworld, 0)[0]
wcepp = SDTest.Call(0x0ef330, 'tiiF', wceplayer, (0.0, 0.0, 0.0))[1]
wcepc = SDTest.Call(wcei(wcei(wceplayer) + 0xd4) - 0x400000, 'tii', wceplayer)[0]
wcecls = 0
if wcepc:
	wcecls = wcei(wcepc + 8)
check('WCE player', lambda: (wceplayer != 0, wcepc != 0, wcecls != 0))

# ---- the read-only helpers on Player0 ----
if wcepc:
	check('WCE Player0 helpers', lambda: (SDTest.Call(0x0d6780, 'tbi', wceplayer)[0], SDTest.Call(0x0d6800, 'tbi',
		wceplayer)[0], SDTest.Call(0x0d6890, 'tbi', wceplayer)[0], SDTest.Call(0x0d6840, 'tbi', wceplayer)[0],
		SDTest.Call(0x0d37f0, 'tsi', wceplayer)[0], SDTest.Call(0x0d38c0, 'tfi', wceplayer)[0],
		SDTest.Call(0x0d3730, 'tii', wceplayer)[0] == wcepc, SDTest.Call(0x0d38b0, 'tii', wceplayer)[0],
		SDTest.Call(0x0d38a0, 'tsi', wceplayer)[0]))
	for wcen in ('CanFly', 'ActionPriority', 'isWalking', 'isFalling', 'ActionPress', 'UsePress', 'DisableControl',
			'AutoSelect', 'IgnoreUsrControl', 'UsrControl', 'Money', 'MoneyReward', 'nBatteries'):
		check('WCE Player0 VarI(%s)' % wcen, lambda n=wcen: SDTest.Call(0x0d3900, 'tbispi', wceplayer, n, -7, 0))
	check('WCE Player0 class', lambda: (wcestr(wcei(wcecls + 4)), wcefl(wcecls + 0x138, 1), wcefl(wcecls + 0x14c, 4),
		wcestr(wcei(wcecls + 0x15c)), wcefl(wcecls + 0x160, 8), wcebytes(wcecls, (0x184, 0x185, 0x186, 0x187, 0x188,
		0x189, 0x18a, 0x18b)), wcefl(wcecls + 0x18c, 1)))
	check('WCE alarm flags', lambda: (wceb(wceworld + 0x2074), wceb(wceworld + 0x1c70)))

# ---- our WalkChar ----
# above the atmosphere limit of the ships (no map there), at least 3000 above the player
wceatm = wcefl(0x867644, 1)[0]
wcey = wcepp[1] + 3000.0
if wceatm + 1000.0 > wcey and wceatm < 50000.0:
	wcey = wceatm + 1000.0
check('WCE height', lambda: (wceatm, wcey - wcepp[1]))
SDTest.Call(0x323c50, 'vi', 77)
wcew = SDTest.Call(0x208360, 'tiisfffs', wceworld, 'SDWceWalk', wcepp[0], wcey, wcepp[2] + 500.0, 'WalkChar')[0]
check('WCE WalkChar', lambda: (wcew != 0, wcew and wcei(wcew + 0x234), wcew and wcei(wcew + 0x39c) == wcew))
# the fields the walk writes
def wcef(e):
	return (wcefl(e + 0x28, 3), wcefl(e + 0xac, 3), wcefl(e + 0xd4, 6), wcefl(e + 0x3f8, 1), wcefl(e + 0x428, 13),
		wcefl(e + 0x45c, 2), wcebytes(e, (0x464, 0x465, 0x4d4, 0x4dc)), wcefl(e + 0x468, 8), wcefl(e + 0x494, 2),
		wcefl(e + 0x4e0, 3))
if wcew:
	check('WCE new', lambda: (wcef(wcew), wcefl(wcew + 0x230, 1), wcei(wcew + 0x424), wcefl(wcew + 0x4ec, 6)))
	# no controller: the walk does nothing
	check('WCE no ctrl', lambda: (SDTest.Call(0x0d3730, 'tii', wcew)[0], SDTest.Call(0x0d38c0, 'tfi', wcew)[0],
		SDTest.Call(0x0d2560, 'tviii', wcew, 1, 0), SDTest.Call(0x0d22e0, 'tvif', wcew, 0.1),
		SDTest.Call(0x0d3080, 'tvif', wcew, 0.1), SDTest.Call(0x0d5e30, 'tvif', wcew, 0.1),
		SDTest.Call(0x0d5300, 'tvif', wcew, 0.1), SDTest.Call(0x0d3310, 'tvif', wcew, 0.1),
		SDTest.Call(0x0d4c60, 'tvii', wcew, 1), SDTest.Call(0x0d6780, 'tbi', wcew)[0],
		SDTest.Call(0x0d6800, 'tbi', wcew)[0], SDTest.Call(0x0d6890, 'tbi', wcew)[0],
		SDTest.Call(0x0d6840, 'tbi', wcew)[0], SDTest.Call(0x0d37f0, 'tsi', wcew)[0], wcef(wcew)))
	check('WCE getters', lambda: (SDTest.Call(0x0d38a0, 'tsi', wcew)[0], SDTest.Call(0x0d38b0, 'tii', wcew)[0],
		SDTest.Call(0x0d3ec0, 'tbis', wcew, 'WalkChar')[0], SDTest.Call(0x0d3ec0, 'tbis', wcew, 'WithLife')[0],
		SDTest.Call(0x0d3ec0, 'tbis', wcew, 'walkchar')[0], SDTest.Call(0x0d3ec0, 'tbis', wcew, 'Car')[0],
		SDTest.Call(0x0d3ec0, 'tbis', wcew, 'Obj')[0], SDTest.Call(0x0d3770, 'tiiF', wcew, (0.0,) * 3)[1],
		SDTest.Call(0x0d3790, 'tiiF', wcew, (0.0,) * 3)[1], SDTest.Call(0x0d5be0, 'tbii', wcew, 5)[0],
		SDTest.Call(0x0d3f20, 'tviii', wcew, 1, 2), SDTest.Call(0x0d3f30, 'tviiii', wcew, 1, 2, 3)))
	wceset(wcew + 0x44c, (0.25, -1.5, 0.125))
	wceset(wcew + 0x3fc, (2.5,))
	check('WCE render angles', lambda: (SDTest.Call(0x0d3770, 'tiiF', wcew, (0.0,) * 3)[1],
		SDTest.Call(0x0d3790, 'tiiF', wcew, (0.0,) * 3)[1], SDTest.Call(0x0d3840, 'tviFF', wcew, (0.0,) * 3,
		(0.0,) * 3)[1:]))
	# the energy
	for wcev in (-1.0, 0.25, 2.0, 0.0):
		check('WCE SetEnergy %s' % fmt(wcev), lambda v=wcev: (SDTest.Call(0x0d4bc0, 'tvif', wcew, v),
			wcefl(wcew + 0x498, 1)))
	for wcev in (0.5, 1.5, -1.0, 1.0):
		check('WCE SetEnergyUsed %s' % fmt(wcev), lambda v=wcev: (SDTest.Call(0x0d4c00, 'tvif', wcew, v),
			wcefl(wcew + 0x498, 1)))
	for wcev in (0.0, 0.5, -0.5, -2.25, 1.0, 0.99989998):
		check('WCE energy frac %s' % fmt(wcev), lambda v=wcev: (wceset(wcew + 0x498, (v,)),
			SDTest.Call(0x0d3740, 'tfi', wcew)[0], SDTest.Call(0x0d38e0, 'tii', wcew)[0]))
	wceset(wcew + 0x498, (0.0,))
	check('WCE SetMass', lambda: (SDTest.Call(0x0d4df0, 'tvif', wcew, 2.5), wcefl(wcew + 0x230, 1)))
	for wcev in (10.0, -3.0, 0.0):
		check('WCE SetYModelDisp %s' % fmt(wcev), lambda v=wcev: (SDTest.Call(0x0d52b0, 'tvif', wcew, v),
			wcefl(wcew + 0xf8, 3), wcefl(wcew + 0x494, 1), wcefl(wcew + 0x45c, 1)))
	# the view
	wceset(wcew + 0x4f4, (9.0,))
	check('WCE SetViewAng', lambda: (SDTest.Call(0x0d51a0, 'tviF', wcew, (0.5, -0.25, 0.125)), wcefl(wcew + 0x428, 12),
		wcefl(wcew + 0x4f4, 1)))
	for wcev in ((0.0, 0.0, 0.0), (100.0, -50.0, 3.0), (wcepp[0], wcepp[1], wcepp[2])):
		check('WCE SetViewFrom %s' % fmt(wcev[0] - wcepp[0]), lambda v=wcev: (SDTest.Call(0x0d5240, 'tviF', wcew, v),
			wcefl(wcew + 0x428, 12)))
	check('WCE SetAng', lambda: (SDTest.Call(0x0d4ba0, 'tviF', wcew, (0.1, 0.2, 0.3)), wcefl(wcew + 0x428, 3),
		wcefl(wcew + 0xac, 3)))
	# the push of an AI
	wcesp = wcefl(wcew + 0x28, 3)
	check('WCE SetPush', lambda: (SDTest.Call(0x0d4fe0, 'tbiF', wcew, (10.0, -2.0, 4.0))[0],
		SDTest.Call(0x0d4fe0, 'tbiF', wcew, (1.0, 1.0, 1.0))[0], wceb(wcew + 0x4dc), wcefl(wcew + 0x4e0, 3)))
	check('WCE ApplyPush', lambda: (SDTest.Call(0x0d5030, 'tvif', wcew, 0.5), wcefl(wcew + 0x28, 3),
		wcefl(wcew + 0x4e0, 3), SDTest.Call(0x0d5030, 'tvif', wcew, 0.5), wcefl(wcew + 0x28, 3)))
	wcepoke(wcew + 0x424, 2)
	check('WCE SetPush slot', lambda: (wcepokeb(wcew + 0x4dc, 0), SDTest.Call(0x0d4fe0, 'tbiF', wcew, (1.0, 1.0,
		1.0))[0], SDTest.Call(0x0d5030, 'tvif', wcew, 0.5), wcefl(wcew + 0x28, 3)))
	wcepoke(wcew + 0x424, -1)
	wcepokeb(wcew + 0x4dc, 0)
	wceset(wcew + 0x28, wcesp)
	# the player slot (only when slot 2 of the world is free)
	if not wcei(wceworld + 0x288 + 8):
		check('WCE SetUsrControl', lambda: (SDTest.Call(0x0d50c0, 'tvii', wcew, 2), wcei(wceworld + 0x288 + 8) == wcew,
			wcei(wcew + 0x424), wcefl(wcei(wcew + 0x140) + 0xc, 1), SDTest.Call(0x0d50c0, 'tvii', wcew, -1),
			wcei(wceworld + 0x288 + 8), wcei(wcew + 0x424), wcefl(wcei(wcew + 0x140) + 0xc, 1)))
	check('WCE SetUsrControl -1', lambda: (SDTest.Call(0x0d50c0, 'tvii', wcew, -1), wcei(wcew + 0x424),
		wcefl(wcei(wcew + 0x140) + 0xc, 1)))
	# the collision sphere
	wceprim = wcew + 0x39c
	if wcei(wceprim) == wcew:
		SDTest.Call(0x0f9430, 'tvif', wceprim, 40.0)
		wcewp = wcefl(wcew + 0x28, 3)
		for wcev in ((0.0, 0.0, 0.0), (wcewp[0] + 100.0, wcewp[1], wcewp[2]), (wcewp[0] + 10.0, wcewp[1] + 5.0,
				wcewp[2])):
			check('WCE TestLine %s' % fmt(wcev[0] - wcewp[0]), lambda v=wcev: (lambda r: (r[0], r[2]))(
				SDTest.Call(0x0d3f10, 'tbiFF', wcew, v, (wcewp[0], wcewp[1], wcewp[2]))))
			check('WCE TestSphere %s' % fmt(wcev[0] - wcewp[0]), lambda v=wcev: SDTest.Call(0x0d5940, 'tbiiFfi', wcew,
				0, v, 30.0, 0)[0])
	# the unlinks
	check('WCE Unlink nodes', lambda: (SDTest.Call(0x0d4170, 'tvi', wcew + 0x3e8), wcei(wcew + 0x3e8) == wcew + 0x3e8,
		SDTest.Call(0x0d4090, 'tvi', wcew), wcei(wcew + 0x528), wcefl(wcew + 0x56c, 1)))

# ---- with the fake controller ----
wcefc = 0
if wcew and wcecls:
	wcefc = wcez(0x7c)
	wcefa = wcez(0x80)
	# a copy of the class (0x20c bytes, its actions shared): CanFly set, SetFalling then changes nothing
	wcecc = wcez(0x20c)
	SDTest.Call(0x3197d0, 'iiii', wcecc, wcecls, 0x20c)
	def wcecanfly(on):
		wcepokeb(wcecc + 0x186, on)
	wcecanfly(1)
	wcepoke(wcefc + 4, wcew)
	wcepoke(wcefc + 8, wcecc)
	wcepoke(wcefc + 0x14, wcefa)
	wcepoke(wcew + 0x234, wcefc)
	# the action: turn speed and maximum, root motion, walk, hover, move type, look at, look y, x, head node, priority
	def wceact(vals):
		wceset(wcefa + 0x38, (vals[0], vals[1]))
		wcepoke(wcefa + 0x48, vals[2] | (vals[3] << 8) | (vals[4] << 16))
		wcepoke(wcefa + 0x4c, vals[5])
		wceset(wcefa + 0x50, (vals[6], vals[7], vals[8]))
		wcepoke(wcefa + 0x60, vals[9])
		wcepoke(wcefa + 0x64, vals[10])
	wceact((2.0, 0.5, 0, 1, 0, 1, 0.0, 0.0, 0.0, 0, 3))
	# the coin model (IsVisible, the effects)
	check('WCE SetModel coin', lambda: (SDTest.Call(0x0fc7b0, 'tvisfffi', wcew, 'Models/Misc/Coins/Battery.M3D', 1.0,
		1.0, 1.0, 0), SDTest.Call(0x210da0, 'tbi', wcew + 0xb8)[0]))
	wcehasmodel = SDTest.Call(0x210da0, 'tbi', wcew + 0xb8)[0]
	check('WCE fake ctrl', lambda: (SDTest.Call(0x0d3730, 'tii', wcew)[0] == wcefc, SDTest.Call(0x0d38c0, 'tfi',
		wcew)[0], SDTest.Call(0x0d6780, 'tbi', wcew)[0], SDTest.Call(0x0d6840, 'tbi', wcew)[0],
		SDTest.Call(0x0d37f0, 'tsi', wcew)[0], SDTest.Call(0x0d2560, 'tviii', wcew, 0, 0), wcei(wcew + 0x78)))
	# the name of the action of layer 0 (layer on)
	def wceanim(name):
		p = wcetext(name)
		wcepoke(wcefa + 8, p)
		wcepokeb(wcefc + 0x1c, 1)
		r = (SDTest.Call(0x0d6780, 'tbi', wcew)[0], SDTest.Call(0x0d6800, 'tbi', wcew)[0], SDTest.Call(0x0d6890,
			'tbi', wcew)[0], SDTest.Call(0x0d37f0, 'tsi', wcew)[0])
		wcepokeb(wcefc + 0x1c, 0)
		wcepoke(wcefa + 8, 0)
		wcefree(p)
		return r
	for wcev in ('Run', 'Runner', 'Walk', 'Sprint', 'Back', 'BackX', 'StrRight', 'StrLeftX', 'StrLef', 'Ru', 'Relax',
			'RelaxAlarm', ''):
		check('WCE anim name %s' % wcev, lambda v=wcev: wceanim(v))
	check('WCE Touch Player0', lambda: (wceset(wcew + 0xd4, (1.0, 2.0, 3.0)), SDTest.Call(0x0d1a60, 'tviiii', wcew,
		wceplayer, 0, 0), wcefl(wcew + 0xd4, 3), wcebytes(wceplayer, (0x48,))))
	# Turn: the view by the control
	for wcev in ((0.5, 0.25, 0.0, 0.1, 1), (0.0, 3.0, 0.5, 1.0, 1), (0.0, -3.0, -0.5, 1.0, 1), (-1.0, 0.1, 1.0, 0.05, 1),
			(0.0, 3.0, 0.5, 1.0, 0), (0.0, -3.0, -0.5, 1.0, 0), (0.25, 0.2, 0.0, 0.5, 0)):
		check('WCE Turn %s' % fmt(wcev), lambda v=wcev: (wcecanfly(v[4]), wceset(wcew + 0x4ec, (v[0], v[1])),
			wceset(wcew + 0x428, (v[2],)), SDTest.Call(0x0d22e0, 'tvif', wcew, v[3]), wcefl(wcew + 0x428, 6)))
	wcecanfly(1)
	wceset(wcew + 0x4ec, (0.0, 0.0))
	# Accel: walk, strafe, view y, velocity, dt
	for wcev in ((0.1, 0.1, 0.0, (0.0, 0.0, 0.0), 0.1), (1.0, 0.0, 0.5, (0.0, 0.0, 0.0), 0.1),
			(0.5, -1.0, 2.0, (100.0, 5.0, -30.0), 0.05), (0.0, 0.0, 0.0, (1.0, 0.5, 1.0), 0.1),
			(1.0, 1.0, -1.0, (5000.0, 0.0, 5000.0), 0.02)):
		check('WCE Accel %s' % fmt(wcev), lambda v=wcev: (wceset(wcew + 0x4f8, (v[0], v[1])), wceset(wcew + 0x42c,
			(v[2],)), wceset(wcew + 0xd4, v[3]), SDTest.Call(0x0d5300, 'tvif', wcew, v[4]), wcefl(wcew + 0xd4, 3)))
	# Fly: walk, hover, inputs (walk, strafe, jump), view x, y, velocity (the hover needs the model)
	wcesp = wcefl(wcew + 0x28, 3)
	wcefly = ((1, 0, (1.0, 0.0, 0.5), 0.2, 0.3, (0.0, 0.0, 0.0)), (1, 0, (0.1, 0.1, 0.0), 0.0, 0.0, (10.0, 0.0, 0.0)),
		(0, 0, (1.0, 0.0, 0.0), 0.0, 0.0, (1.0, 1.0, 1.0)), (1, 1, (1.0, 1.0, 1.0), 0.0, 0.0, (1.0, 1.0, 1.0)))
	if wcehasmodel:
		wcefly = wcefly + ((0, 1, (1.0, 1.0, 1.0), 0.5, -0.5, (200.0, -100.0, 50.0)), (1, 1, (0.0, 0.0, 0.0), 0.0,
			0.0, (1.0, 1.0, 1.0)))
	for wcev in wcefly:
		check('WCE Fly %s' % fmt(wcev[:3]), lambda v=wcev: (wceact((2.0, 0.5, 0, v[0], v[1], 1, 0.0, 0.0, 0.0, 0,
			3)), wceset(wcew + 0x4f8, v[2]), wceset(wcew + 0x428, (v[3], v[4])), wceset(wcew + 0xd4, v[5]),
			SDTest.Call(0x0d3310, 'tvif', wcew, 0.1), wcefl(wcew + 0xd4, 3), wcefl(wcew + 0x28, 3)))
	wceset(wcew + 0x28, wcesp)
	wceset(wcew + 0x4f8, (0.0, 0.0, 0.0))
	wceset(wcew + 0x428, (0.0, 0.0, 0.0))
	wceact((2.0, 0.5, 0, 1, 0, 1, 0.0, 0.0, 0.0, 0, 3))
	# Fall: falling, velocity y, the step of the walk (moved2), move type, dt (CanFly: no change of the falling state)
	for wcev in ((1, 5.0, 1000.0, 1, 0.05), (1, -50.0, 1000.0, 1, 0.05), (1, 20000.0, 1.0, 0, 0.5),
			(0, 0.0, 0.0, 0, 0.1), (0, -3.0, 0.001, 1, 0.1), (0, -3.0, 1000.0, 1, 0.1), (0, 0.0, 1000.0, 0, 0.1)):
		check('WCE Fall %s' % fmt(wcev), lambda v=wcev: (wcepokeb(wcew + 0x464, v[0]), wceset(wcew + 0xd4, (1.0, v[1],
			2.0)), wceset(wcew + 0x3f8, (v[2],)), wcepoke(wcefa + 0x4c, v[3]), SDTest.Call(0x0d3080, 'tvif', wcew,
			v[4]), wcef(wcew)))
	wceset(wcew + 0x28, wcesp)
	wcepoke(wcefa + 0x4c, 1)
	# WalkAnim: falling, walking, view lock, move type, walk, strafe, walk time, root motion (on, x, z), ang y, the
	# move and direction of the stop, dt; only the moves to the side with a move type (no action is played)
	wcecases = (
		(1, 0, 0, 1, 0.5, 0.5, 0.0, (0, 0.0, 0.0), 0.0, (0.0, 0.0), 0.1),
		(1, 1, 1, 1, 1.0, 0.0, 0.5, (1, 2.0, 2.0), 0.5, (0.0, 0.0), 0.05),
		(0, 0, 0, 1, 0.0, 0.0, 0.0, (0, 0.0, 0.0), 0.0, (0.0, 0.0), 0.1),
		(0, 0, 0, 1, 0.0, 0.0, 0.0, (1, 3.0, -2.0), 0.5, (0.0, 0.0), 0.1),
		(0, 0, 1, 1, 0.0, 1.0, 0.0, (0, 0.0, 0.0), 0.0, (0.0, 0.0), 0.1),
		(0, 1, 1, 2, 0.0, 0.6, 0.0, (0, 0.0, 0.0), 0.0, (0.0, 0.0), 0.1),
		(0, 1, 1, 3, 0.0, -1.0, 0.0, (0, 0.0, 0.0), 0.0, (0.0, 0.0), 0.1),
		(0, 0, 1, 1, 0.1, -0.9, 0.0, (1, 1.0, 1.0), 0.3, (0.0, 0.0), 0.05),
		(0, 1, 1, 2, -0.2, -0.5, 0.0, (0, 0.0, 0.0), 0.0, (0.0, 0.0), 0.1),
		(0, 1, 1, 2, 0.0, 0.0, 0.5, (0, 0.0, 0.0), 0.0, (0.4, 1.2), 0.1),
		(0, 1, 1, 1, 0.0, 0.0, 0.5, (0, 0.0, 0.0), 0.0, (0.9, -1.0), 0.1))
	wcek = 0
	for wcev in wcecases:
		check('WCE WalkAnim %d' % wcek, lambda v=wcev: (wcepokeb(wcew + 0x464, v[0]), wcepokeb(wcew + 0x465, v[1]),
			wcepokeb(wcew + 0x4d4, v[2]), wcepoke(wcefa + 0x4c, v[3]), wceset(wcew + 0x4f8, (v[4], v[5])),
			wceset(wcew + 0x478, (v[6],)), wcepokeb(wcefa + 0x48, v[7][0]), wceset(wcefc + 0x6c, (v[7][1], 7.0,
			v[7][2])), wceset(wcew + 0xb0, (v[8],)), wceset(wcew + 0x470, v[9]), wceset(wcew + 0xd4, (10.0, 1.0,
			-5.0)), SDTest.Call(0x0d5e30, 'tvif', wcew, v[10]), wcef(wcew), wcefl(wcefc + 0x5c, 8)))
		wcek = wcek + 1
	wcepokeb(wcefa + 0x48, 0)
	wcepokeb(wcew + 0x465, 0)
	wcepokeb(wcew + 0x4d4, 0)
	wcepokeb(wcew + 0x464, 1)
	wceset(wcew + 0x4f8, (0.0, 0.0))
	wceset(wcew + 0x478, (0.0,))
	wceset(wcew + 0x28, wcesp)
	# Idle: view lock, max turn, view y, ang y, displ, lock start, look at (the listener is Player0), dt
	wcepl = wcetext('Player0')
	check('WCE VarS(SelectedEntity, Player0)', lambda: (SDTest.Call(0x0d5970, 'tbispi', wcew, 'SelectedEntity', wcepl,
		1)[0], wcei(wcew + 0x528) == wceplayer))
	for wcev in ((0, 0.5, 1.0, 0.0, 0.0, 0, 0.0, 0.1), (1, 0.5, 2.0, -1.0, 0.25, 0, 0.0, 0.05),
			(1, 0.2, -2.5, 2.5, 0.0, 1, 0.0, 0.1), (0, 0.5, 0.0, 0.0, 0.0, 0, 3.0, 0.1),
			(1, 3.0, 0.5, 0.25, 0.0, 0, 0.5, 0.2)):
		check('WCE Idle %s' % fmt(wcev), lambda v=wcev: (wcepokeb(wcew + 0x4d4, v[0]), wceact((2.0, v[1], 0, 1, 0, 1,
			v[6], 0.1, -0.2, 0, 3)), wceset(wcew + 0x42c, (v[2],)), wceset(wcew + 0xb0, (v[3],)), wceset(wcew + 0x468,
			(v[4],)), wcepokeb(wcew + 0x50b, v[5]), wceset(wcew + 0x458, (100.0,)), SDTest.Call(0x0d41b0, 'tvif',
			wcew, v[7]), wcefl(wcew + 0xac, 3), wcefl(wcew + 0xe0, 3), wcefl(wcew + 0x428, 12), wcefl(wcew + 0x458, 1),
			wceb(wcew + 0x50b), wcefl(wcew + 0x4f4, 1)))
	# the relax action active in layer 1: its time counts down
	wcerelax = SDTest.Call(0x060590, 'tiis', wcecls, 'Relax')[0]
	check('WCE Relax action', lambda: wcerelax != 0)
	if wcerelax:
		wcepoke(wcefc + 0x24, wcerelax)
		wcepokeb(wcefc + 0x2c, 1)
		check('WCE Idle relax', lambda: (wceset(wcew + 0x42c, (0.0,)), wceset(wcew + 0xb0, (0.0,)), wceset(wcew + 0x468,
			(0.0,)), wceset(wcew + 0x458, (100.0,)), SDTest.Call(0x0d41b0, 'tvif', wcew, 0.25),
			wcefl(wcew + 0x458, 1), wcefl(wcew + 0x428, 3)))
		check('WCE PlayWalk active', lambda: (SDTest.Call(0x0d23c0, 'tvisss', wcew, 'Relax', 'Walk', 'WalkEnd'),
			SDTest.Call(0x0d23c0, 'tvisss', wcew, 'Walk', 'Relax', 'WalkEnd'), wcei(wcew + 0x490) != 0,
			wcei(wcefc + 0xc) != 0))
		wcepoke(wcefc + 0x24, 0)
		wcepokeb(wcefc + 0x2c, 0)
	wcepokeb(wcew + 0x4d4, 0)
	wceact((2.0, 0.5, 0, 1, 0, 1, 0.0, 0.0, 0.0, 0, 3))
	# the script variables
	wcepokeb(wcew + 0x464, 1)
	for wcev in (('InCtrl_Walk', 0.5, 1), ('InCtrl_Walk', 0.0, 0), ('InCtrl_Bogus', 1.0, 0), ('SlidingAndFallingTime',
			0.0, 0), ('EnergyBarDecr', 0.5, 1), ('EnergyBarDecr', 0.0, 0), ('EnergyBarConstantDecr', 2.0, 1),
			('EnergyBarConstantDecr', 0.0, 0), ('TimeToExtraRelax', 3.5, 1), ('TimeToExtraRelax', 0.0, 0),
			('EnergyToRecharge', 0.3, 1), ('EnergyToRecharge', 2.0, 1), ('EnergyToRecharge', 0.0, 0), ('Battery', 0.0,
			0), ('Energy', 0.75, 1), ('Energy', -1.0, 1), ('Energy', 0.0, 0), ('FloorDist', 12.5, 1), ('FloorDist',
			0.0, 0), ('DisplAng', 90.0, 1), ('DisplAng', 0.0, 0), ('YModelDisp', 5.0, 1), ('YModelDisp', 0.0, 0),
			('Bogus', 1.0, 0), ('En', 0.0, 0)):
		check('WCE VarF%s' % fmt(wcev), lambda v=wcev: (SDTest.Call(0x0d2b00, 'tbisDi', wcew, v[0], v[1], v[2]),
			wcefl(wcew + 0x494, 1), wcefl(wcew + 0x4f8, 1)))
	wceset(wcew + 0x4f8, (0.0,))
	for wcev in (('CanFly', 0, 0), ('Money', 123, 1), ('Money', 0, 0), ('nBatteries', 0, 0), ('MoneyReward', 45, 1),
			('MoneyReward', 0, 0), ('ActionPriority', 0, 0), ('IgnoreUsrControl', 1, 1), ('IgnoreUsrControl', 0, 0),
			('IgnoreUsrControl', 0, 1), ('UsrControl', 0, 0), ('UsrControl', -1, 1), ('DisableControl', 2, 1),
			('DisableControl', 0, 0), ('DisableControl', 0, 1), ('AutoSelect', 1, 1), ('AutoSelect', 0, 0),
			('AutoSelect', 0, 1), ('isFalling', 0, 0), ('isFalling', 1, 1), ('isWalking', 0, 0), ('ActionPress', 0, 0),
			('UsePress', 0, 0), ('Bogus', 0, 0), ('CanFly', 1, 1)):
		check('WCE VarI%s' % fmt(wcev), lambda v=wcev: (SDTest.Call(0x0d3900, 'tbispi', wcew, v[0], v[1], v[2]),
			wcebytes(wcew, (0x48c, 0x50a, 0x530))))
	wcetx = wcetext('x')
	wceem = wcetext('')
	for wcev in (('CollideEntity', 0, 0), ('CollideEntity', wceem, 1), ('CollideEntity', wcetx, 1),
			('AutoSelectedEntity', 0, 0), ('SelectedEntity', 0, 0), ('SelectedEntity', wceem, 1),
			('SelectedEntity', 0, 0), ('SelectedEntity', wcepl, 1), ('SelectedEntity', 0, 0)):
		check('WCE VarS(%s, %d)' % (wcev[0], wcev[2]), lambda v=wcev: (lambda r: (r[0], wcestr(r[1])))(
			SDTest.Call(0x0d5970, 'tbispi', wcew, v[0], v[1], v[2])))
	for wcev in (('ViewAng', (0.0, 0.0, 0.0), 0), ('ViewAng', (90.0, 45.0, -30.0), 1), ('ViewAng', (0.0, 0.0, 0.0), 0),
			('ViewPos', (wcepp[0], wcepp[1], wcepp[2]), 1), ('ViewAng', (0.0, 0.0, 0.0), 0)):
		check('WCE VarV(%s, %d)' % (wcev[0], wcev[2]), lambda v=wcev: (SDTest.Call(0x0d5bf0, 'tbisDDDi', wcew, v[0],
			v[1][0], v[1][1], v[1][2], v[2]), wcefl(wcew + 0x428, 12)))
	# SetVel (falling: SetFalling(1) changes nothing), SetActive, UpdateSelect, EnableAI off
	check('WCE SetVel', lambda: (SDTest.Call(0x0d5160, 'tviF', wcew, (1.0, 2.0, 3.0)), wcefl(wcew + 0xd4, 3),
		wcepokeb(wcew + 0x1ac, 1), SDTest.Call(0x0d5160, 'tviF', wcew, (4.0, 5.0, 6.0)), wcefl(wcew + 0xd4, 3),
		wcepokeb(wcew + 0x1ac, 0), wceb(wcew + 0x464)))
	check('WCE SetActive', lambda: (SDTest.Call(0x0d4b60, 'tvii', wcew, 1), wceb(wcew + 0x4c), wcebytes(wcew, (0x4d4,
		0x465)), wcefl(wcew + 0x4d8, 1)))
	check('WCE UpdateSelect', lambda: (wceset(wcew + 0x56c, (2.0,)), SDTest.Call(0x0d4920, 'tvif', wcew, 0.5),
		wcefl(wcew + 0x56c, 1)))
	if wcehasmodel:
		for wcev in (0.1, 0.5, 2.0):
			check('WCE UpdateSlotRate %s' % fmt(wcev), lambda v=wcev: (SDTest.Call(0x0d4f50, 'tvif', wcew, v),
				wcefl(wcei(wcew + 0x140) + 0xc, 1)))
		# the hit flash with the life below the maximum
		wceml = wcei(wcew + 0x22c)
		for wcev in ((wceml, 0.0, 0.1), (wceml + 100, 0.0, 0.25), (wceml, -1.0, 0.5), (wceml, -1.0, 0.75),
				(wceml, -1.0, 0.75)):
			check('WCE UpdateHitFlash %s' % fmt(wcev[1:]), lambda v=wcev: (wcepoke(wcew + 0x22c, v[0]), v[1] < 0.0 or
				wceset(wcew + 0x4b8, (v[1],)), SDTest.Call(0x0d2460, 'tvif', wcew, v[2]), wcefl(wcew + 0x4b8, 1),
				SDTest.Call(0x210760, 'tiii', wcew + 0xb8, 0)[0], wceb(wcew + 0x4c), wcei(wcew + 0x238)))
		wcepoke(wcew + 0x22c, wceml)
		SDTest.Call(0x0fc9d0, 'tvii', wcew, 0)
	# the loop sound 0: a fake item
	wcesn = (wcei(wcew + 0x148), wcei(wcew + 0x14c))
	wcesi = wcez(0xa0)
	wcesa = wcez(8)
	wcepoke(wcesa, wcesi)
	wcepoke(wcew + 0x148, 1)
	wcepoke(wcew + 0x14c, wcesa)
	wceset(wcesi + 0x4c, (0.2, 0.9))
	wceset(wcesi + 0x60, (0.5, 1.5))
	wceset(wcesi + 0x74, (0.0, 0.5))
	wcepoke(wcesi + 0x88, 1)
	for wcev in ((0.0, 0.0, 0.0), (100.0, 0.0, 0.0), (100000.0, 5.0, 0.0), (3.0, -4.0, 12.0)):
		check('WCE FlySound %s' % fmt(wcev), lambda v=wcev: (wceset(wcew + 0xd4, v), SDTest.Call(0x0d57b0, 'tvif', wcew,
			0.1), wcefl(wcesi + 0x40, 3)))
	for wcev in ((0.5, 0.1), (1.0, 0.25), (0.0, 1.0), (0.25, 0.0)):
		check('WCE WalkSound %s' % fmt(wcev), lambda v=wcev: (wceset(wcew + 0x47c, (v[0],)), SDTest.Call(0x0d5880,
			'tvif', wcew, v[1]), wcefl(wcesi + 0x40, 3), wcefl(wcesi + 0x74, 1)))
	check('WCE WalkSound off', lambda: (wcepoke(wcesi + 0x88, 2), SDTest.Call(0x0d5880, 'tvif', wcew, 0.1),
		wcefl(wcesi + 0x40, 3)))
	wcepoke(wcew + 0x148, wcesn[0])
	wcepoke(wcew + 0x14c, wcesn[1])
	wcefree(wcesa)
	wcefree(wcesi)
	# the walk of a slot far above the map (the class flies): walk, strafe, hRot, vRot, push, dt
	wceset(wcew + 0x28, wcesp)
	wceset(wcew + 0xd4, (0.0, 0.0, 0.0))
	def wcewalk(v):
		wcepokeb(wcew + 0x464, 1)
		wceset(wcew + 0x4ec, (v[2], v[3], 9.0, v[0], v[1]))
		if v[4]:
			SDTest.Call(0x0d4fe0, 'tbiF', wcew, v[4])
		SDTest.Call(0x0d1b60, 'tvif', wcew, v[5])
		return (wcef(wcew), wcefl(wcew + 0x460, 1), wcei(wcew + 0x3f0) != 0, wcefl(wcew + 0x498, 1),
			wcebytes(wcew, (0x504, 0x505, 0x506, 0x507, 0x508, 0x509)))
	for wcev in ((0.0, 0.0, 0.0, 0.0, None, 0.05), (1.0, 0.5, 0.25, -0.1, None, 0.05),
			(0.0, 0.0, 0.0, 0.0, (20000.0, 0.0, 0.0), 0.001), (0.0, 0.0, 0.0, 0.0, (0.0, 0.0, 0.0), 0.05),
			(-1.0, 0.0, 0.0, 0.0, None, 0.1)):
		check('WCE Walk %s' % fmt(wcev[:4]), lambda v=wcev: wcewalk(v))
	wceset(wcew + 0x4ec, (0.0, 0.0, 0.0, 0.0, 0.0))
	wcepokeb(wcew + 0x4dc, 0)
	# the slot: the view of the slot (extrapolated), the selection, the turn of the body
	wceex = SDTest.Peek(0x447a08, 'i')
	wcepokeb(wcew + 0x48c, 1)
	for wcev in ((1, 0, 0), (0, 0, 0), (1, 1, 0), (1, 1, 1)):
		check('WCE Tick %s' % fmt(wcev), lambda v=wcev: (SDTest.Poke(0x447a08, v[0]), wceset(wcew + 0x434, (0.5, -1.0,
			2.0)), wcepokeb(wcew + 0x1ac, v[1]), wcepokeb(wcew + 0x1e0, v[2]), SDTest.Call(0x0d3f40, 'tvif', wcew,
			0.05), wcefl(wcew + 0x428, 12), wcefl(wcew + 0xac, 3), wcei(wcew + 0x570)))
	SDTest.Poke(0x447a08, wceex)
	wcepokeb(wcew + 0x1ac, 0)
	wcepokeb(wcew + 0x1e0, 0)
	wcepokeb(wcew + 0x48c, 0)
	# last (a floor up there would change the falling state and play an action): the walk of a class that walks
	wcecanfly(0)
	wceset(wcew + 0x28, wcesp)
	wceset(wcew + 0xd4, (0.0, 0.0, 0.0))
	check('WCE Walk falling', lambda: wcewalk((0.5, 0.0, 0.0, 0.0, None, 0.05)))
	check('WCE Walk falling 2', lambda: wcewalk((0.0, 1.0, 0.1, 0.0, None, 0.1)))
	wceset(wcew + 0x4ec, (0.0, 0.0, 0.0, 0.0, 0.0))
	# the end: the controller off, the model released (SetModel with no file), the entity deleted
	wcepoke(wcew + 0x234, 0)
	wcefree(wcefa)
	wcefree(wcefc)
	wcefree(wcecc)
	check('WCE SetModel none', lambda: (SDTest.Call(0x0d4e30, 'tvisfffi', wcew, '', 1.0, 1.0, 1.0, 0),
		SDTest.Call(0x210da0, 'tbi', wcew + 0xb8)[0], wcefl(wcew + 0x494, 1)))
	wcefree(wcetx)
	wcefree(wceem)
	check('WCE delete', lambda: (SDTest.Call(0x208810, 'tbis', wceworld + 4, SDTest.Peek(wcew + 4, '@p')),
		SDTest.Call(wcei(wcei(wcew) + 0xc) - 0x400000, 'tiii', wcew, 1)[0] == wcew)[1])
	wcefree(wcepl)
