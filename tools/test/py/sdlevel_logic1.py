# game Logic/AlarmStatus.c ... Domination.c (execfile'd by sdlevel.py, Python 1.5.2). The alarm on a fake object in
# calloc memory (the engine variables it writes saved and restored, the steps that would start the callback or send
# the gears only when the callback is empty and the map is outdoors); a cBaseLocalLogic made by its constructor on
# the player (variables, states with the player as target, the origin, the update, the view tests) and deleted
# again; the target searches of the bank and bishop logics on fake objects with the player as parent; the slots
# without code; the data objects of capture the flag and deathmatch made, updated below their think time and
# freed; the domination zones and the baton (saved and restored) and a cDominationEnemiesLogic made and deleted.
# Not called: the GenChar constructors (they start the AI of the character), the updates and states that reach the
# AI, the think steps and hit natives of the modes (no ships of a mode in FZ), the debug texts.
def lg1i(a):
	return SDTest.Peek(a, '@i')
def lg1b(a):
	return SDTest.Peek(a, '@b')
def lg1poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def lg1pokeb(a, v):
	lg1poke(a, (lg1i(a) & ~0xff) | v)
def lg1fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def lg1set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def lg1z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def lg1free(p):
	SDTest.Call('Mem_Free', 'ii', p)
def lg1new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def lg1str(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def lg1bytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def lg1ints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
# a string object of the exe set to s (None: NULL)
def lg1cset(addr, s):
	SDTest.Call('cString_Set', 'tviz', addr, s)
def lg1srand(s):
	SDTest.Call('crt_srand', 'vi', s)
# a string copied into calloc memory
def lg1cstr(s):
	a = lg1z(len(s) + 4)
	SDTest.Call('crt_memcpy', 'iisi', a, s, len(s) + 1)
	return a

lg1world = SDTest.Peek('g_pWorld', 'i')
lg1player = SDTest.Call('cWorld_GetUsrEntity', 'tiii', lg1world, 0)[0]
lg1pp = SDTest.Call('cEntity_GetPos', 'tiiF', lg1player, (0.0, 0.0, 0.0))[1]
lg1outdoor = SDTest.Peek('g_IsMapOutDoor', 'i')
lg1actset = SDTest.Peek('g_CurActionSet', 'i')
lg1cbempty = lg1i(symva('g_AlarmCallback') + 8) == 0
check('LG1 start', lambda: (lg1player != 0, lg1outdoor, lg1actset, lg1cbempty))

# ---- AlarmStatus ----
lg1savedgc = SDTest.Peek('g_GuaranteedChars', 'p')
lg1savedrace = SDTest.Peek('g_PoliceTauntRace', 'p')
lg1savedatt = SDTest.Peek('g_GenCharAttackActive', 'i')
def lg1alarm(a):
	return (lg1fl(a, 2), lg1bytes(a, (8, 0x409, 0x40a, 0x40b, 0x40c, 0x40d, 0x40e)), SDTest.Peek(a + 9, '@s'),
		SDTest.Peek(a + 0x209, '@s'))
lg1al = lg1z(0x410)
check('LG1 Alarm_Reset', lambda: (SDTest.Call('Alarm_Reset', 'tvi', lg1al)[0], lg1alarm(lg1al), SDTest.Peek('g_GuaranteedChars', 'p'),
	SDTest.Peek('g_PoliceTauntRace', 'p') == SDTest.Peek(sym('g_PoliceTauntRace') + 0x4, 'p')))
for lg1v in (0x3f000000, -1097229926, 0, -2147483647 - 1, 0x7fc00000, -4194304):
	check('LG1 Alarm_GetValue %d' % lg1v, lambda v=lg1v: (lg1poke(lg1al + 4, v), SDTest.Call('Alarm_GetValue', 'tii',
		lg1al)[0]))
lg1poke(lg1al + 4, 0)
for lg1v in (1, 0):
	check('LG1 Alarm_SetLock %d' % lg1v, lambda v=lg1v: (SDTest.Call('Alarm_SetLock', 'tvii', lg1al, v)[0],
		SDTest.Call('Alarm_IsLocked', 'tbi', lg1al)[0], lg1alarm(lg1al)))
check('LG1 Alarm_SetGrowRate', lambda: (SDTest.Call('Alarm_SetGrowRate', 'tvif', lg1al, 0.25)[0], lg1alarm(lg1al)))
for lg1v in (1, 0, -1):
	check('LG1 Alarm_SetGrow %d' % lg1v, lambda v=lg1v: (SDTest.Call('Alarm_SetGrow', 'tvii', lg1al, v)[0],
		lg1alarm(lg1al)))
check('LG1 Alarm_SetChar', lambda: (SDTest.Call('Alarm_SetChar', 'tvis', lg1al, 'SDLg1Char')[0], lg1alarm(lg1al)))
for lg1v in (0.4, -1.0, 0.0, 0.999):
	check('LG1 Alarm_Set %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('Alarm_Set', 'tvif', lg1al, v)[0],
		lg1alarm(lg1al)))
# on and off again: the callback is called, the player is not attacked outdoors
if lg1cbempty and lg1outdoor == 1:
	lg1srand(77)
	for lg1v in (1.5, 3.0, 0.5, -0.5, 2.0):
		check('LG1 Alarm_Set on %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('Alarm_Set', 'tvif', lg1al, v)[0],
			lg1alarm(lg1al), SDTest.Peek('g_GenCharAttackActive', 'i')))
	check('LG1 Alarm_SetOn', lambda: (SDTest.Call('Alarm_SetOn', 'tvii', lg1al, 0)[0], lg1alarm(lg1al),
		SDTest.Call('Alarm_SetOn', 'tvii', lg1al, 1)[0], lg1alarm(lg1al), SDTest.Call('Alarm_SetOn', 'tvii', lg1al, 1)[0],
		lg1alarm(lg1al)))
	lg1poke(symva('g_GenCharAttackActive'), lg1savedatt)
# the type of the player with the guaranteed types (the alarm on and off)
for lg1gc in (None, '', ',SDLg1Bogus,', ',Dtritus,Betty,', ',Dtritus,'):
	for lg1on in (0, 1):
		for lg1last in ('SDLg1Char', 'Dtritus'):
			lg1cset(symva('g_GuaranteedChars'), lg1gc)
			lg1pokeb(lg1al + 8, lg1on)
			SDTest.Call('crt_memcpy', 'iisi', lg1al + 0x209, lg1last, len(lg1last) + 1)
			check('LG1 Alarm_Update %s' % fmt((lg1gc, lg1on, lg1last)), lambda: (SDTest.Call('Alarm_Update', 'tvif', lg1al,
				0.125)[0], lg1alarm(lg1al)))
lg1pokeb(lg1al + 8, 0)
check('LG1 Alarm_Update locked', lambda: (lg1pokeb(lg1al + 0x40d, 1), SDTest.Call('Alarm_Update', 'tvif', lg1al, 1.0)[0],
	lg1alarm(lg1al), lg1pokeb(lg1al + 0x40d, 0)))
lg1free(lg1al)
lg1cset(symva('g_GuaranteedChars'), lg1savedgc)
lg1cset(symva('g_PoliceTauntRace'), lg1savedrace)
lg1poke(symva('g_GenCharAttackActive'), lg1savedatt)
check('LG1 alarm vars restored', lambda: (SDTest.Peek('g_GuaranteedChars', 'p') == lg1savedgc, SDTest.Peek('g_PoliceTauntRace', 'p') ==
	lg1savedrace, SDTest.Peek('g_GenCharAttackActive', 'i') == lg1savedatt))

# ---- the slots without code ----
for lg1r in ('Logic_True', 'Logic_True@142750', 'Logic_True@142870', 'Logic_True@142a10', 'Logic_True@142b00', 'Logic_True@143040', 'Logic_True@143440', 'Logic_True@144ac0', 'Logic_False', 'Logic_False@142a90'):
	check('LG1 byte slot %s' % lg1r, lambda r=lg1r: SDTest.Call(r, 'tbi', 0)[0])
for lg1r in ('Logic_NoTarget', 'Logic_NoTarget@142a80', 'Logic_NoTarget@143050', 'Logic_NoTarget@143450', 'Logic_NoTarget@144ad0'):
	check('LG1 NoTarget %s' % lg1r, lambda r=lg1r: SDTest.Call(r, 'tii', 0)[0])
for lg1r in ('Logic_Nop1', 'Logic_Nop1@1405f0', 'Logic_Nop1@142830', 'Logic_Nop1@142840', 'Logic_Nop1@142850', 'Logic_Nop1@142860', 'Logic_Nop1@142af0'):
	check('LG1 Nop1 %s' % lg1r, lambda r=lg1r: SDTest.Call(r, 'tvii', 0, 5)[0])
check('LG1 Logic_False1', lambda: SDTest.Call('Logic_False1', 'tbii', 0, 5)[0])
for lg1r in ('cLocalLogic_VarNone', 'cLocalLogic_VarNone@141d80', 'cLocalLogic_VarNone@142480'):
	check('LG1 cLocalLogic_VarNone %s' % lg1r, lambda r=lg1r: SDTest.Call(r, 'tbisii', 0, 'State', 0, 1)[0])
check('LG1 cLocalLogic_VarVNone', lambda: SDTest.Call('cLocalLogic_VarVNone', 'tbisiiii', 0, 'Origin', 0, 0, 0, 1)[0])
check('LG1 GetName', lambda: (SDTest.Call('cBaseLocalLogic_GetName', 'tsi', 0)[0], SDTest.Call('cLocalLogic_GetName', 'tsi', 0)[0]))

# ---- cLocalLogic ----
lg1l = lg1z(8)
check('LG1 cLocalLogic_dtor', lambda: (SDTest.Call('cLocalLogic_dtor', 'tvi', lg1l)[0], lg1i(lg1l) - 0x400000))
lg1free(lg1l)
for lg1r in ('cLocalLogic_Delete', 'cCTFEnemiesLocalLogic_Delete'):
	lg1l = lg1new(8)
	check('LG1 Delete8 %s' % lg1r, lambda r=lg1r, l=lg1l: SDTest.Call(r, 'tiii', l, 1)[0] == l)
lg1l = lg1z(8)
lg1poke(lg1l + 4, lg1player)
for lg1v in ('IsLeader', 'Bogus', ''):
	check('LG1 cCTFEnemiesLocalLogic_VarI(%s)' % lg1v, lambda v=lg1v: SDTest.Call('cCTFEnemiesLocalLogic_VarI', 'tbispi', lg1l, v, 7, 0))
lg1free(lg1l)

# ---- cBaseLocalLogic on the player ----
def lg1base(l):
	return (lg1i(l) - 0x400000, lg1i(l + 4) == lg1player, lg1i(l + 8) == l + 8, lg1i(l + 0xc) == l + 8,
		lg1i(l + 0x10) == lg1player, lg1i(l + 0x14) == l, lg1i(l + 0x18), lg1fl(l + 0x1c, 1),
		lg1bytes(l, (0x20, 0x28, 0x29, 0x2a)), lg1fl(l + 0x24, 1), lg1fl(l + 0x2c, 7))
lg1savedturn = lg1i(lg1player + 0x4f0)
lg1l = lg1new(0x48)
check('LG1 cBaseLocalLogic_ctor', lambda: (SDTest.Call('cBaseLocalLogic_ctor', 'tiii', lg1l, lg1player)[0] == lg1l, lg1base(lg1l)))
check('LG1 Logic_GetLocalLogic', lambda: (SDTest.Call('Logic_GetLocalLogic', 'ii', lg1player)[0] == lg1i(lg1player + 0x74),
	SDTest.Call('Logic_GetLocalLogic', 'ii', 0)[0]))
for lg1v in (('Lock', 0), ('Lock', 1), ('AlwaysLock', 1), ('AlwaysLock', 0), ('Idle', 0), ('State', 0), ('State', 1),
		('Bogus', 0), ('', 1)):
	check('LG1 cBaseLocalLogic_VarI(%s, %d)' % lg1v, lambda v=lg1v: (SDTest.Call('cBaseLocalLogic_VarI', 'tbispi', lg1l, v[0], 3,
		v[1]), lg1base(lg1l)))
for lg1v in (('ActionRadius', 0), ('ActionRadius', 1), ('Bogus', 1)):
	check('LG1 cBaseLocalLogic_VarF(%s, %d)' % lg1v, lambda v=lg1v: (SDTest.Call('cBaseLocalLogic_VarF', 'tbisDi', lg1l, v[0], 1234.5,
		v[1]), lg1base(lg1l)))
for lg1v in (('Origin', 0), ('Origin', 1), ('OriginAng', 0), ('OriginAng', 1), ('Orig', 1), ('Bogus', 1)):
	check('LG1 cBaseLocalLogic_VarV(%s, %d)' % lg1v, lambda v=lg1v: (SDTest.Call('cBaseLocalLogic_VarV', 'tbisDDDi', lg1l, v[0],
		1.5, -2.25, 1e30, v[1]), lg1base(lg1l)))
lg1tname = lg1cstr('Player0')
lg1ename = lg1cstr('')
check('LG1 cBaseLocalLogic_VarS(Target get)', lambda: lg1str(SDTest.Call('cBaseLocalLogic_VarS', 'tbispi', lg1l, 'Target', 0, 0)[1]))
check('LG1 cBaseLocalLogic_VarS(Target set)', lambda: (SDTest.Call('cBaseLocalLogic_VarS', 'tbispi', lg1l, 'Target', lg1tname, 1)[0],
	lg1base(lg1l), lg1str(SDTest.Call('cBaseLocalLogic_VarS', 'tbispi', lg1l, 'Target', 0, 0)[1])))
check('LG1 cBaseLocalLogic_VarS(Target set empty)', lambda: (SDTest.Call('cBaseLocalLogic_VarS', 'tbispi', lg1l, 'Target', lg1ename,
	1)[0], lg1base(lg1l), SDTest.Call('cBaseLocalLogic_VarS', 'tbispi', lg1l, 'Bogus', 0, 0)[0]))
lg1free(lg1tname)
lg1free(lg1ename)
for lg1v in ((2, 0), (2, 0), (5, 1), (5, 1), (5, 0), (0, 0)):
	if lg1v[1]:
		lg1t = lg1player
	else:
		lg1t = 0
	check('LG1 cBaseLocalLogic_SetState%s' % fmt(lg1v), lambda v=lg1v, t=lg1t: (SDTest.Call('cBaseLocalLogic_SetState', 'tbiii', lg1l,
		v[0], t)[0], lg1base(lg1l)))
# not idle: the same state again gives 0
lg1pokeb(lg1l + 0x28, 0)
check('LG1 cBaseLocalLogic_SetState same', lambda: (SDTest.Call('cBaseLocalLogic_SetState', 'tbiii', lg1l, 0, 0)[0], lg1base(lg1l)))
# SetIdle and Update reach the AI of the character when its action locks it
lg1quiet = lg1i(lg1player + 0x78) == 0 or lg1b(lg1l + 0x29) == 0
check('LG1 quiet', lambda: (lg1i(lg1player + 0x78) == 0, lg1quiet))
if lg1quiet:
	check('LG1 cBaseLocalLogic_SetIdle(0)', lambda: (SDTest.Call('cBaseLocalLogic_SetIdle', 'tvii', lg1l, 0)[0], lg1base(lg1l)))
check('LG1 cBaseLocalLogic_RestartState', lambda: (SDTest.Call('cBaseLocalLogic_RestartState', 'tvi', lg1l)[0], lg1base(lg1l)))
lg1pokeb(lg1l + 0x28, 1)
for lg1v in ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0), (0.0, 0.0, -0.0)):
	check('LG1 cBaseLocalLogic_SetOrigin%s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('cBaseLocalLogic_SetOrigin', 'tvifff', lg1l, v[0], v[1],
		v[2])[0], SDTest.Call('cBaseLocalLogic_SetOriginAng', 'tvifff', lg1l, v[2], v[1], v[0])[0], lg1base(lg1l)))
check('LG1 cBaseLocalLogic_SetOriginFrom', lambda: (SDTest.Call('cBaseLocalLogic_SetOriginFrom', 'tvii', lg1l, lg1player)[0], lg1base(lg1l)))
for lg1v in (500.0, 500.0, 0.0):
	check('LG1 cBaseLocalLogic_SetActionRadius %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('cBaseLocalLogic_SetActionRadius', 'tvif', lg1l,
		v)[0], lg1base(lg1l)))
check('LG1 cBaseLocalLogic_Refresh', lambda: (SDTest.Call('cBaseLocalLogic_Refresh', 'tvi', lg1l)[0], lg1base(lg1l)))
check('LG1 cBaseLocalLogic_UpdateBusy', lambda: (SDTest.Call('cBaseLocalLogic_UpdateBusy', 'tbi', lg1l)[0], lg1base(lg1l)))
check('LG1 cBishopLocalLogic_UpdateBusy', lambda: (SDTest.Call('cBishopLocalLogic_UpdateBusy', 'tbi', lg1l)[0], lg1base(lg1l)))
lg1srand(5)
check('LG1 cBaseLocalLogic_Vf15', lambda: map(lambda i, l=lg1l: SDTest.Call('cBaseLocalLogic_Vf15', 'tiii', l, i)[0] & 0xff, range(12)))
for lg1v in (0.25, 0.5):
	if lg1quiet:
		check('LG1 cBaseLocalLogic_Update %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('cBaseLocalLogic_Update', 'tvif', lg1l, v)[0],
			lg1base(lg1l), lg1fl(lg1player + 0x4f0, 1)))
lg1poke(lg1player + 0x4f0, lg1savedturn)
# the view tests from the player
lg1pts = ((lg1pp[0], lg1pp[1], lg1pp[2]), (lg1pp[0] + 300.0, lg1pp[1] + 50.0, lg1pp[2]), (lg1pp[0], lg1pp[1],
	lg1pp[2] + 400.0), (lg1pp[0], lg1pp[1], lg1pp[2] - 400.0), (lg1pp[0] - 900.0, lg1pp[1] + 500.0, lg1pp[2] + 900.0),
	(lg1pp[0] + 5000.0, lg1pp[1], lg1pp[2]))
for lg1p in lg1pts:
	for lg1v in ((1000.0, 1.5707963705062866, 1.5707963705062866, 0), (1000.0, 3.0, 0.5, 1), (100000.0, 6.3, 6.3, 1)):
		check('LG1 Logic_InView%s' % fmt((lg1p, lg1v)), lambda p=lg1p, v=lg1v: SDTest.Call('Logic_InView', 'biFfffi', lg1player,
			p, v[0], v[1], v[2], v[3])[0])
check('LG1 Logic_InView self', lambda: SDTest.Call('Logic_InView', 'biFfffi', lg1player, lg1pp, 10.0, 0.1, 0.1, 1)[0])
lg1lists = map(lambda o: lg1i(lg1world + o), (0x2244, 0x2248, 0x2234, 0x21fc, 0x2200))
lg1listsok = 0 not in lg1lists
check('LG1 char lists', lambda: lg1listsok)
if lg1listsok:
	for lg1k in lg1lists:
		check('LG1 Logic_FindInView', lambda k=lg1k: (SDTest.Call('Logic_FindInView', 'iiifffii', lg1player, k, 100000.0, 6.3, 6.3,
			0, 0)[0] != 0, SDTest.Call('Logic_FindInView', 'iiifffii', lg1player, k, 1000.0, 1.0, 1.0, 1, 0)[0] != 0))
# the player as parent of fake GenChar logics: the target tests and searches
lg1gc = lg1z(0x68)
lg1poke(lg1gc + 4, lg1player)
for lg1v in ((0, 0.0), (1, 0.0), (1, 100.0)):
	if lg1v[0]:
		lg1poke(lg1gc + 0x10, lg1player)
	else:
		lg1poke(lg1gc + 0x10, 0)
	lg1set(lg1gc + 0x5c, (lg1v[1],))
	check('LG1 cBankDirectorLocalLogic_LoseTarget%s' % fmt(lg1v), lambda: (SDTest.Call('cBankDirectorLocalLogic_LoseTarget', 'tbi', lg1gc)[0],
		SDTest.Call('cBankDirectorLocalLogic_LoseTarget@140750', 'tbi', lg1gc)[0]))
lg1poke(lg1gc + 0x10, 0)
check('LG1 cBishopLocalLogic_LoseTarget', lambda: SDTest.Call('cBishopLocalLogic_LoseTarget', 'tbi', lg1gc)[0])
if lg1listsok:
	lg1savedagr = SDTest.Peek('g_BankDirectorAgressive', 'i')
	for lg1v in ((0, 1.0), (0, -1.0), (1, -1.0)):
		lg1poke(symva('g_BankDirectorAgressive'), lg1v[0])
		for lg1r in ('cBankDirectorLocalLogic_FindTarget', 'cBankMasterLocalLogic_FindTarget', 'cBishopLocalLogic_FindTarget'):
			lg1set(lg1gc + 0x60, (lg1v[1],))
			check('LG1 FindTarget %s %s' % (lg1r, fmt(lg1v)), lambda r=lg1r: (SDTest.Call(r, 'tii', lg1gc)[0] ==
				lg1player, lg1fl(lg1gc + 0x60, 1)))
	lg1poke(symva('g_BankDirectorAgressive'), lg1savedagr)
lg1srand(9)
for lg1v in ((0, 0, 50.0), (0, 0, 0.0), (1, 0, 50.0), (0x10001, lg1player, 100.0), (0x101, lg1player, 101.0)):
	lg1poke(lg1gc + 0x28, lg1v[0])
	check('LG1 Logic_Chance%s' % fmt((lg1v[0], lg1v[1] != 0, lg1v[2])), lambda v=lg1v: map(lambda i, v=v:
		SDTest.Call('Logic_Chance', 'biif', lg1gc, v[1], v[2])[0], range(6)))
lg1poke(lg1gc + 0x28, 0)
check('LG1 cBossLocalLogic_Vf33', lambda: (lg1poke(lg1gc + 0x18, 3), SDTest.Call('cBossLocalLogic_Vf33', 'tvii', lg1gc, 1)[0],
	lg1poke(lg1gc + 0x18, 8), SDTest.Call('cBossLocalLogic_Vf33', 'tvii', lg1gc, 0)[0], lg1i(lg1gc + 0x18)))
lg1free(lg1gc)
# a functionary without its desktop (there is none in FZ)
lg1fn = lg1z(0xbc)
lg1poke(lg1fn + 0x68, lg1fn + 0x68)
lg1poke(lg1fn + 0x6c, lg1fn + 0x68)
lg1poke(lg1fn + 0x74, lg1fn)
for lg1r in ('cBertoLocalLogic_AssignDesktop', 'cBettyLocalLogic_AssignDesktop'):
	check('LG1 AssignDesktop %s' % lg1r, lambda r=lg1r: (SDTest.Call(r, 'tvii', lg1fn, 0)[0], lg1ints(lg1fn, (0x70,
		0x78))))
for lg1r in ('cBertoLocalLogic_ReleaseDesktop', 'cBertoLocalLogic_ReleaseDesktop@142a20'):
	check('LG1 ReleaseDesktop %s' % lg1r, lambda r=lg1r: (SDTest.Call(r, 'tvi', lg1fn)[0], lg1ints(lg1fn, (0x70,
		0x78))))
lg1free(lg1fn)
check('LG1 cBaseLocalLogic_Delete', lambda: (SDTest.Call('cBaseLocalLogic_Delete', 'tiii', lg1l, 0)[0] == lg1l, lg1i(lg1l) - 0x400000,
	lg1i(lg1l + 8) == lg1l + 8, lg1i(lg1l + 0x10)))
lg1poke(lg1l, 0x400000 + sym('cBaseLocalLogic_vftable'))
check('LG1 cBaseLocalLogic_dtor', lambda: (SDTest.Call('cBaseLocalLogic_dtor', 'tvi', lg1l)[0], lg1i(lg1l) - 0x400000))
lg1poke(lg1l, 0x400000 + sym('cBaseLocalLogic_vftable'))
check('LG1 cBaseLocalLogic_Delete free', lambda: SDTest.Call('cBaseLocalLogic_Delete', 'tiii', lg1l, 1)[0] == lg1l)
check('LG1 Logic_AsCar', lambda: (SDTest.Call('Logic_AsCar', 'ii', 0)[0], SDTest.Call('Logic_AsCar', 'ii', lg1player)[0]))
check('LG1 Logic_FindCar', lambda: (SDTest.Call('Logic_FindCar', 'is', '')[0], SDTest.Call('Logic_FindCar', 'is', 'Player0')[0],
	SDTest.Call('Logic_FindCar', 'is', 'SDLg1None')[0]))

# ---- the data of capture the flag and deathmatch ----
lg1h = lg1z(4)
check('LG1 cCTFEnemies_ctor', lambda: (SDTest.Call('cCTFEnemies_ctor', 'tii', lg1h)[0] == lg1h, lg1i(lg1h) != 0))
lg1d = lg1i(lg1h)
def lg1ctf(d):
	return (lg1str(lg1i(d)), lg1i(d + 4), lg1fl(d + 8, 2))
check('LG1 cCTFEnemies leader', lambda: (SDTest.Call('cCTFEnemies_GetLeader', 'tsi', lg1h)[0], SDTest.Call('cCTFEnemies_SetLeader', 'tvis', lg1h,
	'SDLg1Leader')[0], SDTest.Call('cCTFEnemies_GetLeader', 'tsi', lg1h)[0], SDTest.Call('cCTFEnemies_GetList', 'tii', lg1h)[0], lg1ctf(lg1d)))
check('LG1 cCTFEnemies_SetBonus', lambda: (SDTest.Call('cCTFEnemies_SetBonus', 'tvif', lg1h, 0.3)[0], lg1ctf(lg1d)))
for lg1v in (0.2, 0.25, 0.04):
	check('LG1 cCTFEnemies_Update %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('cCTFEnemies_Update', 'tvif', lg1h, v)[0],
		lg1ctf(lg1d)))
check('LG1 cCTFEnemies_dtor', lambda: SDTest.Call('cCTFEnemies_dtor', 'tvi', lg1h)[0])
check('LG1 cCTFFriends_ctor', lambda: (SDTest.Call('cCTFFriends_ctor', 'tii', lg1h)[0] == lg1h, lg1i(lg1h) != 0))
lg1d = lg1i(lg1h)
for lg1v in (0.2, 0.25, 0.04):
	check('LG1 cCTFFriends_Update %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('cCTFFriends_Update', 'tvif', lg1h, v)[0],
		lg1i(lg1d), lg1fl(lg1d + 4, 1), SDTest.Call('cCTFFriends_GetList', 'tii', lg1h)[0]))
check('LG1 cCTFFriends_dtor', lambda: SDTest.Call('cCTFFriends_dtor', 'tvi', lg1h)[0])
lg1free(lg1h)
lg1h = lg1z(8)
lg1poke(lg1h, -1)
lg1poke(lg1h + 4, -1)
check('LG1 cDeathMatch_ctor', lambda: (SDTest.Call('cDeathMatch_ctor', 'tii', lg1h)[0] == lg1h, lg1ints(lg1h, (0, 4))))
for lg1v in (0.3, 0.15):
	check('LG1 cDeathMatch_Update %s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('cDeathMatch_Update', 'tvif', lg1h, v)[0],
		lg1fl(lg1h, 1)))
check('LG1 cDeathMatch_dtor', lambda: SDTest.Call('cDeathMatch_dtor', 'v')[0])
lg1free(lg1h)

# ---- domination ----
# g_DominNumZones and the four zones, g_DominBatonPos and g_DominBatonState: separate C objects in ScrapC.exe
lg1zonewords = [symva('g_DominNumZones')] + map(lambda o: symva('g_DominZones') + o, (0, 4, 8, 0xc, 0x10, 0x14, 0x18,
	0x1c))
lg1batonwords = [symva('g_DominBatonPos'), symva('g_DominBatonPos') + 4, symva('g_DominBatonPos') + 8,
	symva('g_DominBatonState')]
def lg1zonesget(words):
	return tuple(map(lambda a: SDTest.Peek(a, '@i'), words))
lg1savedzones = lg1zonesget(lg1zonewords)
lg1savedbaton = lg1zonesget(lg1batonwords)
def lg1zones():
	return (lg1i(symva('g_DominNumZones')), tuple(map(lambda i: lg1i(symva('g_DominZones') + 8 * i) == lg1player, range(4))),
		lg1ints(symva('g_DominZones') + 0x4, (0, 8, 0x10, 0x18)))
for lg1v in ((1.0, 2.0, 3.0, 0), (-5.5, 0.0, 1e20, 2), (4.0, 5.0, 6.0, 1)):
	check('LG1 Domination_SetBatonState%s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('Domination_SetBatonState', 'vFi', v[:3], v[3])[0],
		lg1fl(symva('g_DominBatonPos'), 3), lg1i(symva('g_DominBatonState'))))
lg1poke(symva('g_DominNumZones'), 0)
for lg1k in range(4):
	check('LG1 Domination_AddZone %d' % lg1k, lambda: (SDTest.Call('Domination_AddZone', 'vi', lg1player)[0], lg1zones()))
for lg1v in ((0, 2), (3, 1), (1, 0), (2, 1)):
	check('LG1 Domination_SetZoneState%s' % fmt(lg1v), lambda v=lg1v: (SDTest.Call('Domination_SetZoneState', 'vii', v[0], v[1])[0],
		lg1zones()))
for lg1st in ((0, 1, 2, 4), (1, 1, 0, 3), (2, 4, 2, 4), (0, 0, 0, 0), (1, 3, 1, 3)):
	lg1k = 0
	while lg1k < 4:
		lg1poke(symva('g_DominZones') + 0x4 + 8 * lg1k, lg1st[lg1k])
		lg1k = lg1k + 1
	for lg1p in lg1pts[:3]:
		check('LG1 cDominationLogic_NearestZone%s' % fmt((lg1st, lg1p)), lambda p=lg1p: SDTest.Call('cDominationLogic_NearestZone', 'tiiF', 0,
			p)[0])
lg1dl = lg1new(0x14)
check('LG1 cDominationEnemiesLogic_ctor', lambda: (SDTest.Call('cDominationEnemiesLogic_ctor', 'tii', lg1dl)[0] == lg1dl, lg1i(lg1dl) -
	0x400000, lg1i(lg1dl + 4), lg1i(lg1dl + 8) != 0, lg1ints(lg1i(lg1dl + 8), (0, 4, 8, 0xc)), lg1bytes(lg1dl, (0xc,
	0xd)), lg1i(lg1i(lg1dl + 0x10)) == lg1dl, lg1ints(lg1i(lg1dl + 0x10), (4, 8, 0xc)), lg1i(symva('g_DominNumZones'))))
lg1poke(symva('g_DominNumZones'), 3)
check('LG1 cDominationLogic_Delete(0)', lambda: (SDTest.Call('cDominationLogic_Delete', 'tiii', lg1dl, 0)[0] == lg1dl, lg1i(lg1dl) -
	0x400000, lg1ints(lg1dl, (4, 8)), lg1i(symva('g_DominNumZones'))))
lg1poke(symva('g_DominNumZones'), 3)
check('LG1 cDominationEnemiesLogic_Delete', lambda: (SDTest.Call('cDominationEnemiesLogic_Delete', 'tiii', lg1dl, 1)[0] == lg1dl,
	lg1i(symva('g_DominNumZones'))))
lg1k = 0
while lg1k < 9:
	lg1poke(lg1zonewords[lg1k], lg1savedzones[lg1k])
	lg1k = lg1k + 1
lg1k = 0
while lg1k < 4:
	lg1poke(lg1batonwords[lg1k], lg1savedbaton[lg1k])
	lg1k = lg1k + 1
check('LG1 domination restored', lambda: (lg1zonesget(lg1zonewords) ==
	lg1savedzones, lg1zonesget(lg1batonwords) == lg1savedbaton))
