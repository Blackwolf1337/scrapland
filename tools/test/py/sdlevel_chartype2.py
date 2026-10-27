# game Entities/Char/Type/Mayor.c ... Sentinel.c, Entities/Char/WalkCharControl.c and WalkCharEntity.c
# (execfile'd by sdlevel.py, Python 1.5.2). The walking characters of FZ are listed first (their classes). The
# control on calloc memory: constructor, the script variables, clear, reset and latch, and the input of a frame with
# the action values of the player slot set by hand (saved and restored): turn, walk, strafe, the pad, the run, the
# buttons, the lock on a target and the center of the view with WalkCameraCenter set. A WalkChar made by
# cWorld_CreateEntity far above the player (rand seeded): the constructor fields, the view lock, the victim test and
# the energy (the controller of the player borrowed, read only), the target of its control and the deleting
# destructor. The bounce of the burp debris on calloc memory, the sentinel target with the alarm flags set by hand,
# the listener of a police on calloc memory. The natives on Player0 that only touch what is saved and restored here
# (the "MeEnt" globals, the nurse name, bytes of the player) or the actions of the player (their names printed).
# Not called: the FX natives (they load models and start effects), the Listening natives (they CHECK the listen
# action, the player has none), PoliceBossAction (it would play the Action of the player), MessengerAttack and the
# hits of the nurse on others (FZ has no other characters).
def ct2i(a):
	return SDTest.Peek(a, '@i')
def ct2b(a):
	return SDTest.Peek(a, '@b')
def ct2poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def ct2fl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def ct2set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def ct2ints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def ct2bytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def ct2z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def ct2free(p):
	SDTest.Call(0x133550, 'ii', p)
def ct2str(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def ct2name(e):
	if e:
		return SDTest.Peek(e + 4, '@p')
	return None
def ct2slot(e, off):
	return ct2i(ct2i(e) + off) - 0x400000
def ct2srand(s):
	SDTest.Call(0x323c50, 'vi', s)
def ct2pokeb(a, v):
	ct2poke(a, (ct2i(a) & ~0xff) | v)
def ct2native(name, params):
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
def ct2delete(e):
	SDTest.Call(0x208810, 'tbis', ct2world + 4, ct2name(e))
	return SDTest.Call(ct2slot(e, 0xc), 'tiii', e, 1)[0] == e
def ct2cls(e):
	c = SDTest.Call(ct2slot(e, 0xd4), 'tii', e)[0]
	if c and ct2i(c + 8):
		return ct2str(ct2i(ct2i(c + 8) + 4))
	return None

ct2world = SDTest.Peek(0x467450, 'i')
ct2player = SDTest.Call(0x0c1430, 'tiii', ct2world, 0)[0]
ct2pp = SDTest.Call(0x0ef330, 'tiiF', ct2player, (0.0, 0.0, 0.0))[1]
ct2ctrl = SDTest.Call(ct2slot(ct2player, 0xd4), 'tii', ct2player)[0]
def ct2act():
	return (SDTest.Call(0x062c40, 'tsii', ct2ctrl, 0)[0], ct2bytes(ct2player, (0x504, 0x505)))

# ---- the walking characters of the level ----
ct2chars = []
ct2n = ct2i(ct2world + 0x10)
ct2k = 0
while ct2n != ct2world + 0x10 and ct2i(ct2n + 0xc) and ct2k < 5000:
	ct2e = ct2i(ct2n + 0xc)
	if SDTest.Call(ct2slot(ct2e, 0x14), 'tbis', ct2e, 'WalkChar')[0]:
		ct2chars.append((ct2name(ct2e), ct2cls(ct2e)))
	ct2n = ct2i(ct2n)
	ct2k = ct2k + 1
check('CT2 walk chars', lambda: (ct2player != 0, ct2ctrl != 0, len(ct2chars), tuple(ct2chars[:20])))
ct2slotp = SDTest.Call(0x0d38b0, 'tii', ct2player)[0]
check('CT2 player slot', lambda: (ct2slotp, ct2cls(ct2player), ct2act()))

# ---- cWalkCharControl on calloc memory ----
def ct2cf(c):
	return (ct2fl(c, 6), ct2bytes(c, (0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20)), ct2i(c + 0x24) == c + 0x24,
		ct2i(c + 0x28) == c + 0x24, ct2i(c + 0x2c) != 0, ct2i(c + 0x30) == c)
ct2c = ct2z(0x34)
check('CT2 cWalkCharControl_ctor', lambda: (SDTest.Call(0x0cfff0, 'tii', ct2c)[0] == ct2c, ct2cf(ct2c)))
for ct2v in (('Walk', 2.5), ('Str', -1.25), ('HRot', 0.75), ('VRot', -0.5), ('Act', 0.75), ('Act', 0.5), ('Act', 0.25),
		('Wa', 1.0), ('St', 1.0), ('Bogus', 1.0), ('', 1.0)):
	check('CT2 cWalkCharControl_VarF%s' % fmt(ct2v), lambda v=ct2v: (SDTest.Call(0x0d0110, 'tbisDi', ct2c, v[0], v[1],
		1)[0], SDTest.Call(0x0d0110, 'tbisDi', ct2c, v[0], -9.0, 0), ct2cf(ct2c)))
ct2set(ct2c, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0))
ct2poke(ct2c + 0x18, 0x01010101)
ct2poke(ct2c + 0x1c, 0x01010101)
ct2pokeb(ct2c + 0x20, 1)
check('CT2 cWalkCharControl_Latch', lambda: (SDTest.Call(0x0d03f0, 'tviif', ct2c, 0, 0.5), ct2cf(ct2c)))
check('CT2 cWalkCharControl_Reset(1)', lambda: (SDTest.Call(0x0d0390, 'tvii', ct2c, 1), ct2cf(ct2c)))
ct2poke(ct2c + 0x18, 0x01010101)
check('CT2 cWalkCharControl_Reset(0)', lambda: (SDTest.Call(0x0d0390, 'tvii', ct2c, 0), ct2cf(ct2c)))
ct2poke(ct2c + 0x1c, 0x01010101)
check('CT2 cWalkCharControl_Clear', lambda: (SDTest.Call(0x0d00c0, 'tvi', ct2c), ct2cf(ct2c)))
ct2set(ct2c + 8, (7.0,))
check('CT2 cWalkCharControl_RestartCenter', lambda: (SDTest.Call(0x0d0380, 'tvi', ct2c), ct2cf(ct2c)))
check('CT2 cWalkCharControl_Update disabled', lambda: (ct2pokeb(ct2c + 0x1e, 1), SDTest.Call(0x0d0410, 'tviif', ct2c,
	ct2player, 0.05), ct2cf(ct2c)))
ct2pokeb(ct2c + 0x1e, 0)

# a WalkChar far above the player, the target of the lock below
ct2srand(321)
ct2w = SDTest.Call(0x208360, 'tiisfffs', ct2world, 'SDCt2Walk', ct2pp[0], ct2pp[1] + 3000.0, ct2pp[2] + 500.0,
	'WalkChar')[0]
check('CT2 WalkChar', lambda: ct2w != 0)

# the input of the character set (3): value and prev of the slot of the player
ct2sets = SDTest.Peek(0x44c12c, 'i')
ct2nact = 0
ct2items = 0
if ct2sets and SDTest.Peek(0x44c128, 'i') > 3:
	ct2nact = ct2i(ct2sets + 0x24 + 4)
	ct2items = ct2i(ct2sets + 0x24 + 8)
check('CT2 action set 3', lambda: (ct2nact > 22, ct2slotp >= 0, ct2slotp < 4))
def ct2inval(i):
	return ct2items + 0x7c * i + 0x18 + 4 * ct2slotp
def ct2inprev(i):
	return ct2items + 0x7c * i + 0x8 + 4 * ct2slotp
# vectors: the values of actions 2..20 (21 and 22 stay 0: the arrow of a listener), the prev of 19 and 20
def ct2input(vals, prev19, prev20):
	ct2j = 0
	while ct2j < 19:
		ct2set(ct2inval(ct2j + 2), (vals[ct2j],))
		ct2j = ct2j + 1
	ct2set(ct2inprev(19), (prev19,))
	ct2set(ct2inprev(20), (prev20,))
if ct2nact > 22 and ct2slotp >= 0 and ct2slotp < 4:
	ct2savedin = []
	ct2j = 2
	while ct2j < 23:
		ct2savedin.append((ct2i(ct2inval(ct2j)), ct2i(ct2inprev(ct2j))))
		ct2j = ct2j + 1
	ct2wcc = ct2i(0x8472b4)
	ct2vecs = (
		((0.0,) * 19, 0.0, 0.0),
		((0.5, 0.1, 0.25, 1.0, 1.0, 0.0, 0.7, 0.3, 1.0, 0.2, 0.2, 0.6, 0.0, 0.0, 0.0, 1.0, 0.9, 1.0, 0.0), 0.0, 0.0),
		((0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0), 0.0, 0.0),
		((0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.9), 0.0, 0.1),
		((0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.3), 0.0, 0.9),
		((1.0, 0.0, 0.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.0), 0.0, 0.0))
	ct2k = 0
	for ct2vec in ct2vecs:
		check('CT2 cWalkCharControl_Update %d' % ct2k, lambda v=ct2vec: (ct2input(v[0], v[1], v[2]),
			SDTest.Call(0x0d0410, 'tviif', ct2c, ct2player, 0.05), ct2cf(ct2c)))
		ct2k = ct2k + 1
	# the lock with a target: the angles from the target to the player
	if ct2w:
		check('CT2 cWalkCharControl_SetTarget', lambda: (SDTest.Call(0x0d0060, 'tvii', ct2c, ct2w), ct2i(ct2c + 0x2c) == ct2w))
		check('CT2 cWalkCharControl_Update lock', lambda: (ct2input(ct2vecs[3][0], 0.0, 0.1), SDTest.Call(0x0d0410, 'tviif',
			ct2c, ct2player, 0.05), ct2cf(ct2c)))
		check('CT2 cWalkCharControl_SetTarget(0)', lambda: (SDTest.Call(0x0d0060, 'tvii', ct2c, 0), ct2cf(ct2c)))
	# the center of the view after WalkCameraCenter seconds without input
	ct2set(0x8472b4, (2.0,))
	SDTest.Call(0x0d0380, 'tvi', ct2c)
	for ct2v in ((0, 1.5), (0, 1.0), (5, 0.05), (0, 3.0)):
		check('CT2 cWalkCharControl_Update center%s' % fmt(ct2v), lambda v=ct2v: (ct2input(ct2vecs[v[0]][0], 0.0, 0.0),
			SDTest.Call(0x0d0410, 'tviif', ct2c, ct2player, v[1]), ct2cf(ct2c)))
	ct2poke(0x8472b4, ct2wcc)
	ct2j = 2
	while ct2j < 23:
		ct2poke(ct2inval(ct2j), ct2savedin[ct2j - 2][0])
		ct2poke(ct2inprev(ct2j), ct2savedin[ct2j - 2][1])
		ct2j = ct2j + 1
check('CT2 cWalkCharControl_dtor', lambda: (SDTest.Call(0x0d1410, 'tvi', ct2c), ct2cf(ct2c)))
ct2free(ct2c)

# ---- the WalkChar ----
def ct2wf(e):
	return (ct2i(e) - 0x400000, ct2i(e + 0x424), ct2fl(e + 0x428, 12), ct2fl(e + 0x458, 2), ct2bytes(e, (0x464, 0x465,
		0x48c, 0x4d4, 0x4dc, 0x530)), ct2fl(e + 0x468, 5), ct2fl(e + 0x480, 3), ct2fl(e + 0x494, 2), ct2fl(e + 0x4b0, 3),
		ct2fl(e + 0x4d8, 1), ct2ints(e, (0x22c, 0x238, 0x404, 0x408, 0x4cc, 0x4d0, 0x56c, 0x570, 0x3fc)),
		ct2fl(e + 0x230, 1), ct2fl(e + 0x3f8, 1), ct2i(e + 0x48) & 0x10,
		tuple(map(lambda o, e=e: ct2i(e + o) == e, (0x3f4, 0x4c4, 0x52c, 0x540))))
if ct2w:
	check('CT2 cWalkCharEntity_ctor', lambda: (ct2wf(ct2w), ct2i(ct2w + 0x234) != 0, ct2i(ct2w + 0x51c) == ct2w + 0x4ec,
		ct2fl(ct2w + 0x4ec, 6)))
	ct2saved234 = ct2i(ct2w + 0x234)
	ct2savedvl = ct2i(ct2w + 0x4d4)
	for ct2v in ((1, 0.25), (0, 0.25), (0, 0.5), (1, 0.1)):
		check('CT2 cWalkCharEntity_UpdateViewLock%s' % fmt(ct2v), lambda v=ct2v: (ct2pokeb(ct2w + 0x4d4, v[0]),
			SDTest.Call(0x0d18e0, 'tvif', ct2w, v[1]), ct2bytes(ct2w, (0x4d4, 0x465)), ct2fl(ct2w + 0x4d8, 1)))
	check('CT2 cWalkCharEntity_CanActOnVictim', lambda: (SDTest.Call(0x0d1990, 'tbii', ct2w, 0)[0],
		SDTest.Call(0x0d1990, 'tbii', ct2w, 1)[0]))
	ct2poke(ct2w + 0x234, ct2ctrl)
	ct2poke(ct2w + 0x53c, ct2player)
	for ct2v in (0.0, 0.5, -0.5):
		check('CT2 cWalkCharEntity_CanActOnVictim %s' % fmt(ct2v), lambda v=ct2v: (ct2set(ct2w + 0x498, (v,)),
			SDTest.Call(0x0d1990, 'tbii', ct2w, 0)[0], SDTest.Call(0x0d1990, 'tbii', ct2w, 1)[0]))
	ct2poke(ct2w + 0x53c, 0)
	for ct2v in ((1, 0.25), (0, 0.25)):
		check('CT2 cWalkCharEntity_UpdateViewLock ctrl%s' % fmt(ct2v), lambda v=ct2v: (ct2pokeb(ct2w + 0x4d4, v[0]),
			SDTest.Call(0x0d18e0, 'tvif', ct2w, v[1]), ct2bytes(ct2w, (0x4d4, 0x465)), ct2fl(ct2w + 0x4d8, 1)))
	# the energy: the press bytes and 0x506/0x508 clear, so no callback of the class runs
	ct2poke(ct2w + 0x504, 0)
	ct2poke(ct2w + 0x508, 0)
	for ct2v in ((0.0, 0.5, 0.1), (0.0, 0.01, 1.0), (0.0, -2.0, 0.5), (2.0, 0.5, 0.25), (2.0, 0.9, 1.0), (0.5, 0.0, 0.0)):
		check('CT2 cWalkCharEntity_ActionTick%s' % fmt(ct2v), lambda v=ct2v: (ct2set(ct2w + 0x4b4, (v[0],)),
			ct2set(ct2w + 0x498, (v[1],)), SDTest.Call(0x0d14c0, 'tvif', ct2w, v[2]), ct2fl(ct2w + 0x498, 1)))
	ct2poke(ct2w + 0x234, ct2saved234)
	ct2poke(ct2w + 0x4d4, ct2savedvl)
	check('CT2 cWalkCharEntity_ActionTick none', lambda: (ct2set(ct2w + 0x498, (0.5,)), SDTest.Call(0x0d14c0, 'tvif',
		ct2w, 1.0), ct2fl(ct2w + 0x498, 1)))
	check('CT2 control SetTarget(player)', lambda: (SDTest.Call(0x0d0060, 'tvii', ct2w + 0x4ec, ct2player),
		ct2i(ct2w + 0x518) == ct2player, SDTest.Call(0x0d0060, 'tvii', ct2w + 0x4ec, 0), ct2i(ct2w + 0x518)))
	check('CT2 cWalkCharEntity_Unlink3e8', lambda: (SDTest.Call(0x0d1a10, 'tvi', ct2w), ct2i(ct2w + 0x3e8) == ct2w + 0x3e8,
		ct2i(ct2w + 0x3f0)))
	check('CT2 WalkChar delete', lambda: ct2delete(ct2w))

# ---- the bounce of the burp debris ----
ct2bvol = ct2fl(0x8470d4, 1)[0]
check('CT2 PoliceBossBrupBounceSoundVol', lambda: ct2bvol)
if not ct2bvol > 0.0:
	ct2d = ct2z(0x60)
	ct2hit = ct2z(0x40)
	ct2set(ct2hit + 0x24, (0.0, 1.0, 0.0))
	for ct2v in ((1.0, (0.5, -1.0, 2.0), (100.0, -300.0, 20.0)), (1.0, (0.1, 0.2, 0.3), (10.0, -50.0, 5.0)),
			(0.1, (1.0, 1.0, 1.0), (1.0, -2.0, 3.0)), (0.3, (0.0, 0.0, 0.0), (0.0, -100.0, 0.0))):
		ct2srand(55)
		check('CT2 ChType_PoliceBossBrupBounce%s' % fmt(ct2v), lambda v=ct2v: (ct2set(ct2d + 0x4c, (v[0],)),
			ct2set(ct2d + 0x28, v[1] + v[2]), SDTest.Call(0x0ce470, 'vii', ct2d, ct2hit), ct2fl(ct2d + 0x1c, 13)))
	ct2free(ct2d)
	ct2free(ct2hit)

# ---- the sentinel target with the alarm flags set by hand ----
ct2fe = ct2z(0x574)
ct2savedal = (ct2b(ct2world + 0x1c70), ct2b(ct2world + 0x21f9))
for ct2v in ((0, 0), (1, 0), (0, 1), (1, 1)):
	check('CT2 ChType_SentinelTarget%s' % fmt(ct2v), lambda v=ct2v: (ct2pokeb(ct2world + 0x1c70, v[0]),
		ct2pokeb(ct2world + 0x21f9, v[1]), SDTest.Call(0x0cffd0, 'b')[0], SDTest.Call(0x0cfc10, 'ii', 0)[0],
		SDTest.Call(0x0cfc10, 'ii', ct2fe)[0], (ct2poke(ct2fe + 0x53c, ct2player), SDTest.Call(0x0cfc10, 'ii',
		ct2fe)[0] == ct2player)[1], ct2poke(ct2fe + 0x53c, 0)))
ct2pokeb(ct2world + 0x1c70, ct2savedal[0])
ct2pokeb(ct2world + 0x21f9, ct2savedal[1])
# the listener of a police that is a player: falling, the listener cleared, the player as listener
for ct2v in ((1, 1, ct2player), (0, 0, ct2player), (0, 1, ct2player), (0, 1, 0)):
	check('CT2 ChType_PoliceFindListener%s' % fmt(ct2v[:2]), lambda v=ct2v: (ct2pokeb(ct2fe + 0x464, v[0]),
		ct2pokeb(ct2fe + 0x530, v[1]), ct2poke(ct2fe + 0x528, v[2]), (lambda r: (r[0], r[1] == ct2player, r[2], r[3] != 0))(
		SDTest.Call(0x0cd520, 'bippp', ct2fe, 0, 0, 0))))
ct2free(ct2fe)

# ---- the natives on Player0 ----
ct2meents = (0x446bac, 0x446ff8, 0x446e50)
ct2savedme = tuple(map(lambda r: SDTest.Peek(r, 'i'), ct2meents))
ct2savednurse = SDTest.Peek(0x446dd0, 's')
ct2savedb = ct2ints(ct2player, (0x48c, 0x530))
ct2savedmove = ct2ints(ct2player, tuple(range(0x174, 0x1e4, 4)))
ct2loop = SDTest.Call(ct2slot(ct2player, 0xd0), 'tiis', ct2player, 'ActionLoop')[0]
ct2logic = SDTest.Call(0x141ac0, 'ii', ct2player)[0]
check('CT2 player before', lambda: (ct2loop != 0, ct2logic != 0, ct2i(ct2player + 0x528), ct2i(ct2player + 0x53c) != 0,
	ct2b(ct2player + 0x464), ct2act()))
for ct2v in ('SentinelActionPress', 'SentinelActionRelease', 'NurseActionPress', 'PoliceActionPress'):
	check('CT2 native %s(Player0)' % ct2v, lambda v=ct2v: (ct2native(v, ('Player0',)), ct2act(), SDTest.Peek(0x446dd0, 's')))
for ct2v in ('NurseTryHammerAgain', 'NurseBackHitControl', 'MayorEndAction', 'PoliceBossBrup', 'PoliceRestoreFreeTurn'):
	check('CT2 native %s(Player0)' % ct2v, lambda v=ct2v: (ct2native(v, ('Player0', 'SDEvent', 0.0)), ct2act(),
		ct2b(ct2player + 0x48c), ct2fl(ct2player + 0x174 + 0x30, 2), ct2b(ct2player + 0x174 + 0x38),
		tuple(map(lambda r: SDTest.Peek(r, 'i') == ct2player, ct2meents))))
if not ct2loop:
	for ct2v in (('NurseInitHammerLoop', ('Player0', 'SDEvent', 0.0)), ('NurseActionRelease', ('Player0',))):
		check('CT2 native %s(Player0)' % ct2v[0], lambda v=ct2v: (ct2native(v[0], v[1]), ct2act(),
			SDTest.Peek(0x446e50, 'i') == ct2player))
if not ct2i(ct2player + 0x53c):
	check('CT2 native SentinelPoint(Player0)', lambda: (ct2native('SentinelPoint', ('Player0', 'SDEvent', 0.0)),
		ct2act(), ct2fl(ct2world + 0x1c6c, 1)))
if not ct2logic and not ct2i(ct2player + 0x528):
	check('CT2 native PoliceEndActionMoney(Player0)', lambda: (ct2native('PoliceEndActionMoney', ('Player0', 'SDEvent',
		0.0)), ct2act(), ct2b(ct2player + 0x530), ct2i(ct2player + 0x520) == ct2player + 0x520))
ct2k = 0
while ct2k < len(ct2meents):
	SDTest.Poke(ct2meents[ct2k], ct2savedme[ct2k])
	ct2k = ct2k + 1
SDTest.Call(0x22cf50, 'iisi', 0x400000 + 0x446dd0, ct2savednurse, len(ct2savednurse) + 1)
ct2poke(ct2player + 0x48c, ct2savedb[0])
ct2poke(ct2player + 0x530, ct2savedb[1])
ct2k = 0
while ct2k < len(ct2savedmove):
	ct2poke(ct2player + 0x174 + 4 * ct2k, ct2savedmove[ct2k])
	ct2k = ct2k + 1
