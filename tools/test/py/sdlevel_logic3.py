# game Logic/GenCharLogic.c (part 3) ... NurseLogic.c (execfile'd by sdlevel.py, Python 1.5.2). The slots without
# code; the variables, setters and the early returns of fake GenChar, LittlePolice, Mayor, Maintenance and nurse
# logics with the player as parent (the fields of the player they write and the engine variables poked saved and
# restored); the item list of the world on a fake cInfItem (items added, walked, the free list, deleted); the AI
# control on a fake object with empty lists (the steps that do not reach a sentinel, gear or police); the nurse
# group and the messengers on fake objects with empty lists; the money string; a human logic made and deleted; the
# Delete of the logics on zeroed memory.
# Not called: the constructors that start the AI (GenChar types, nurses), anything that sets an AI state, plays an
# action on the player or moves it, the natives (they read a parameter file), the Init functions (they add lists
# to the world), the messenger think (not in C).
def lg3i(a):
	return SDTest.Peek(a, '@i')
def lg3b(a):
	return SDTest.Peek(a, '@b')
def lg3poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def lg3pokeb(a, v):
	lg3poke(a, (lg3i(a) & ~0xff) | v)
def lg3fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def lg3set(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def lg3z(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def lg3free(p):
	SDTest.Call('Mem_Free', 'ii', p)
def lg3new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def lg3str(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
def lg3bytes(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs))
def lg3ints(a, offs):
	return tuple(map(lambda o, a=a: SDTest.Peek(a + o, '@i'), offs))
def lg3cset(addr, s):
	SDTest.Call('cString_Set', 'tviz', addr, s)
def lg3srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def lg3cstr(s):
	a = lg3z(len(s) + 4)
	SDTest.Call('crt_memcpy', 'iisi', a, s, len(s) + 1)
	return a
# an empty entity list (the ring of 0x30 on itself, no count)
def lg3list():
	l = lg3z(0x44)
	lg3poke(l + 0x30, l + 0x30)
	lg3poke(l + 0x34, l + 0x30)
	return l
# a node of a logic on itself, data the logic
def lg3node(l, o):
	lg3poke(l + o, l + o)
	lg3poke(l + o + 4, l + o)
	lg3poke(l + o + 0xc, l)
def lg3vt(a):
	return lg3i(a) - 0x400000
# a world list is empty (the first node of its ring has no data); 0 if it does not exist
def lg3empty(off):
	l = lg3i(lg3world + off)
	return l != 0 and lg3i(lg3i(l + 0x30) + 0xc) == 0
def lg3fset(a, v):
	lg3set(a, (v,))

lg3world = SDTest.Peek('g_pWorld', 'i')
lg3player = SDTest.Call('cWorld_GetUsrEntity', 'tiii', lg3world, 0)[0]
lg3alarm = lg3b(lg3world + 0x1c70)
lg3locked = SDTest.Call('Alarm_IsLocked', 'tbi', lg3world + 0x1c68)[0]
lg3notguar = lg3b(lg3world + 0x2073)
lg3gc1 = lg3empty(0x2244)
lg3gc2 = lg3empty(0x2248)
lg3gc3 = lg3empty(0x224c)
check('LG3 start', lambda: (lg3player != 0, lg3alarm, lg3locked, lg3notguar, lg3gc1, lg3gc2, lg3gc3))

# ---- the slots without code ----
for lg3r in ('Logic_Nop1@14ceb0', 'Logic_Nop1@14d920', 'Logic_Nop1@14d930', 'Logic_Nop1@14db50', 'Logic_Nop1@14db60', 'Logic_Nop1@14db70', 'Logic_Nop1@14dd80', 'Logic_Nop1@154800'):
	check('LG3 Logic_Nop1 %s' % lg3r, lambda r=lg3r: SDTest.Call(r, 'tvii', 0, 5)[0])
check('LG3 Logic_Nop2', lambda: SDTest.Call('Logic_Nop2', 'tviii', 0, 5, 6)[0])
check('LG3 Logic_False1', lambda: SDTest.Call('Logic_False1@14d910', 'tbii', 0, 5)[0])
check('LG3 Logic_True1', lambda: SDTest.Call('Logic_True1@1545a0', 'tbii', 0, 5)[0])
check('LG3 Logic_True2', lambda: SDTest.Call('Logic_True2', 'tbiii', 0, 5, 6)[0])
for lg3r in ('Logic_True@14dca0', 'Logic_True@14dd90', 'Logic_True@152350', 'Logic_True@1530b0'):
	check('LG3 Logic_True %s' % lg3r, lambda r=lg3r: SDTest.Call(r, 'tbi', 0)[0])
for lg3r in ('Logic_False@14dd20', 'Logic_False@153dc0'):
	check('LG3 Logic_False %s' % lg3r, lambda r=lg3r: SDTest.Call(r, 'tbi', 0)[0])
for lg3r in ('Logic_NoTarget@14dd10', 'Logic_NoTarget@152530', 'Logic_NoTarget@1530c0', 'Logic_NoTarget@153dd0'):
	check('LG3 Logic_NoTarget %s' % lg3r, lambda r=lg3r: SDTest.Call(r, 'tii', 0)[0])
lg3m = lg3z(0x14)
check('LG3 cMessengers ctor dtor', lambda: (SDTest.Call('cMessengers_ctor', 'tii', lg3m)[0] == lg3m, SDTest.Call('cMessengers_dtor', 'tvi',
	lg3m)[0], SDTest.Call('Logic_NopDtor@154280', 'tvi', lg3m)[0]))

# ---- the fields of the player the logics write ----
def lg3psave():
	return (lg3i(lg3player + 0x380), lg3i(lg3player + 0x388), lg3i(lg3player + 0x38c), lg3i(lg3player + 0x390),
		lg3i(lg3player + 0x394), lg3i(lg3player + 0x4f0), lg3i(lg3player + 0x4f8))
def lg3prestore(v):
	lg3poke(lg3player + 0x380, v[0])
	lg3poke(lg3player + 0x388, v[1])
	lg3poke(lg3player + 0x38c, v[2])
	lg3poke(lg3player + 0x390, v[3])
	lg3poke(lg3player + 0x394, v[4])
	lg3poke(lg3player + 0x4f0, v[5])
	lg3poke(lg3player + 0x4f8, v[6])
def lg3pf():
	return (lg3b(lg3player + 0x381), lg3i(lg3player + 0x388), lg3i(lg3player + 0x38c), lg3fl(lg3player + 0x390, 2))
lg3psaved = lg3psave()
# a fake logic of the class with the player as parent
def lg3mk(n, vt):
	f = lg3z(n)
	lg3poke(f, 0x400000 + vt)
	lg3poke(f + 4, lg3player)
	lg3node(f, 8)
	return f
def lg3gcf(f):
	return (lg3i(f + 0x18), lg3bytes(f, (0x20, 0x28, 0x29, 0x2a, 0x58, 0x59, 0x5a)), lg3fl(f + 0x48, 2),
		lg3i(f + 0x50), lg3str(lg3i(f + 0x54)), lg3fl(f + 0x5c, 2), lg3i(f + 0x64))

# ---- GenChar ----
lg3gc = lg3mk(0x68, sym('cGenCharLocalLogic_vftable'))
lg3cset(lg3gc + 0x54, 'SDLg3Def')
for lg3v in ((6, 1), (0, 0), (5, 1), (0, 0), (10, 1), (0, 0), (0, 1), (0, 0)):
	check('LG3 cGenCharLocalLogic_VarI(DefaultState)%s' % fmt(lg3v), lambda v=lg3v: (SDTest.Call('cGenCharLocalLogic_VarI', 'tbispi',
		lg3gc, 'DefaultState', v[0], v[1]), lg3gcf(lg3gc)))
check('LG3 cGenCharLocalLogic_VarI(State)', lambda: SDTest.Call('cGenCharLocalLogic_VarI', 'tbispi', lg3gc, 'State', 3, 0))
lg3tname = lg3cstr('SDLg3Target')
check('LG3 cGenCharLocalLogic_VarS set', lambda: (SDTest.Call('cGenCharLocalLogic_VarS', 'tbispi', lg3gc, 'DefaultTarget', lg3tname,
	1)[0], lg3gcf(lg3gc)))
check('LG3 cGenCharLocalLogic_VarS get', lambda: lg3str(SDTest.Call('cGenCharLocalLogic_VarS', 'tbispi', lg3gc, 'DefaultTarget', 0,
	0)[1]))
lg3free(lg3tname)
check('LG3 cGenCharLocalLogic setters', lambda: (SDTest.Call('cGenCharLocalLogic_SetDefaultState', 'tvii', lg3gc, 5)[0], SDTest.Call('cGenCharLocalLogic_SetDefaultTarget',
	'tvis', lg3gc, 'SDLg3Other')[0], lg3gcf(lg3gc)))
for lg3v in ('PoliceBoss', 'Betty', 'Berto', 'Mayor', 'Bertolo', 'Dtritus', 'Police', ''):
	check('LG3 Logic_IsSuspectType(%s)' % lg3v, lambda v=lg3v: SDTest.Call('Logic_IsSuspectType', 'bs', v)[0])
lg3srand(31)
check('LG3 cGenCharLocalLogic_ResetSpecial', lambda: (SDTest.Call('cGenCharLocalLogic_ResetSpecial', 'tvi', lg3gc)[0], lg3gcf(lg3gc)))
check('LG3 cGenCharLocalLogic_OnRobbery', lambda: (SDTest.Call('cGenCharLocalLogic_OnRobbery', 'tviif', lg3gc, lg3player, 0.0)[0],
	SDTest.Call('cGenCharLocalLogic_OnRobbery', 'tviif', lg3gc, lg3player, -1.0)[0], lg3gcf(lg3gc)))
lg3pokeb(lg3gc + 0x29, 1)
check('LG3 cGenCharLocalLogic busy hits', lambda: (SDTest.Call('cGenCharLocalLogic_OnHit', 'tvii', lg3gc, lg3player)[0],
	SDTest.Call('cGenCharLocalLogic_OnSeen', 'tvii', lg3gc, lg3player)[0], lg3gcf(lg3gc)))
lg3pokeb(lg3gc + 0x29, 0)
lg3poke(lg3gc + 0x18, 2)
check('LG3 cGenCharLocalLogic_OnSeen accusing', lambda: (SDTest.Call('cGenCharLocalLogic_OnSeen', 'tvii', lg3gc, lg3player)[0],
	lg3gcf(lg3gc)))
lg3poke(lg3gc + 0x18, 0)
lg3savedrefuse = lg3i(symva('g_GenCharPoliceRefuse'))
lg3fset(symva('g_GenCharPoliceRefuse'), 1000.0)
lg3srand(32)
check('LG3 cGenCharLocalLogic_GivesMoney refused', lambda: (map(lambda i: SDTest.Call('cGenCharLocalLogic_GivesMoney', 'tbii', lg3gc,
	lg3player)[0], range(3)), lg3gcf(lg3gc)))
lg3poke(symva('g_GenCharPoliceRefuse'), lg3savedrefuse)
lg3fset(lg3player + 0x4f8, 1.0)
check('LG3 cGenCharLocalLogic_Discover walking', lambda: (SDTest.Call('cGenCharLocalLogic_Discover', 'tvi', lg3gc)[0], lg3gcf(lg3gc)))
lg3prestore(lg3psaved)
# CanAttack: the early returns by the engine variables, the logic of the target and the flag
lg3savedca = lg3ints(symva('g_EnergyBarActive'), (0,)) + lg3ints(symva('g_SpecialActionActive'), (0,)) + lg3ints(symva('g_GenCharAttackActive'), (0,))
for lg3v in ((1, 2, 1, 0), (0, 2, 1, 1), (1, 3, 1, 1), (1, 2, 0, 1), (1, 2, 1, 1)):
	lg3poke(symva('g_EnergyBarActive'), lg3v[0])
	lg3poke(symva('g_SpecialActionActive'), lg3v[1])
	lg3poke(symva('g_GenCharAttackActive'), lg3v[2])
	lg3pokeb(lg3gc + 0x58, lg3v[3])
	check('LG3 cGenCharLocalLogic_CanAttack%s' % fmt(lg3v), lambda: SDTest.Call('cGenCharLocalLogic_CanAttack', 'tbii', lg3gc,
		lg3player)[0])
lg3poke(symva('g_EnergyBarActive'), lg3savedca[0])
lg3poke(symva('g_SpecialActionActive'), lg3savedca[1])
lg3poke(symva('g_GenCharAttackActive'), lg3savedca[2])
check('LG3 cGenCharLocalLogic_CanAttack restored', lambda: (lg3ints(symva('g_EnergyBarActive'), (0,)) + lg3ints(symva('g_SpecialActionActive'), (0,)) +
	lg3ints(symva('g_GenCharAttackActive'), (0,))) == lg3savedca)
check('LG3 LittlePolice vf25', lambda: SDTest.Call('cGenCharLocalLogic_CanAttack@1518f0', 'tbii', lg3gc, lg3player)[0])
check('LG3 cGenCharLocalLogic_dtor', lambda: (SDTest.Call('cGenCharLocalLogic_dtor', 'tvi', lg3gc)[0], lg3vt(lg3gc)))
lg3free(lg3gc)

# ---- LittlePolice ----
lg3lp = lg3mk(0x78, sym('cLittlePoliceLocalLogic_vftable'))
def lg3lpf(f):
	return (lg3gcf(f), lg3fl(f + 0x68, 2), lg3b(f + 0x70), lg3i(f + 0x74))
for lg3v in ((7, 1), (0, 0), (-2, 1), (0, 0)):
	check('LG3 cLittlePoliceLocalLogic_VarI(PoliceResult)%s' % fmt(lg3v), lambda v=lg3v: (SDTest.Call('cLittlePoliceLocalLogic_VarI',
		'tbispi', lg3lp, 'PoliceResult', v[0], v[1]), lg3lpf(lg3lp)))
for lg3v in ((5, 1), (2, 0), (0, 0)):
	check('LG3 cLittlePoliceLocalLogic_VarI(DefaultState)%s' % fmt(lg3v), lambda v=lg3v: (SDTest.Call('cLittlePoliceLocalLogic_VarI',
		'tbispi', lg3lp, 'DefaultState', v[0], v[1]), lg3lpf(lg3lp)))
check('LG3 cLittlePoliceLocalLogic_TargetDone no target', lambda: SDTest.Call('cLittlePoliceLocalLogic_TargetDone', 'tbi', lg3lp)[0])
for lg3v in ((0, 0, 0), (4, 0, 0), (4, 1, 0), (2, 0, 0), (0, 0, 1)):
	lg3poke(lg3lp + 0x18, lg3v[0])
	lg3pokeb(lg3lp + 0x70, lg3v[1])
	lg3pokeb(lg3lp + 0x29, lg3v[2])
	check('LG3 cLittlePoliceLocalLogic_UpdateView%s' % fmt(lg3v), lambda: (SDTest.Call('cLittlePoliceLocalLogic_UpdateView', 'tvi', lg3lp)[0],
		lg3pf()))
lg3prestore(lg3psaved)
lg3poke(lg3lp + 0x18, 0)
lg3pokeb(lg3lp + 0x70, 0)
lg3pokeb(lg3lp + 0x29, 1)
check('LG3 cLittlePoliceLocalLogic_UpdateLook busy', lambda: (SDTest.Call('cLittlePoliceLocalLogic_UpdateLook', 'tvi', lg3lp)[0], lg3pf()))
lg3pokeb(lg3lp + 0x29, 0)
if lg3alarm == 0 and lg3notguar == 0:
	check('LG3 cLittlePoliceLocalLogic_UpdateLook', lambda: (SDTest.Call('cLittlePoliceLocalLogic_UpdateLook', 'tvi', lg3lp)[0], lg3pf()))
lg3prestore(lg3psaved)
check('LG3 cLittlePoliceLocalLogic_UpdateBusy', lambda: (SDTest.Call('cLittlePoliceLocalLogic_UpdateBusy', 'tbi', lg3lp)[0], lg3lpf(lg3lp)))
lg3pokeb(lg3lp + 0x29, 1)
check('LG3 cLittlePoliceLocalLogic hits', lambda: (SDTest.Call('cLittlePoliceLocalLogic_OnHit', 'tvii', lg3lp, 0)[0], SDTest.Call('cLittlePoliceLocalLogic_OnSeen',
	'tvii', lg3lp, lg3player)[0], SDTest.Call('cLittlePoliceLocalLogic_GivesMoney', 'tbii', lg3lp, lg3player)[0], lg3lpf(lg3lp)))
lg3pokeb(lg3lp + 0x29, 0)
check('LG3 cLittlePoliceLocalLogic_OnSeen other', lambda: (SDTest.Call('cLittlePoliceLocalLogic_OnSeen', 'tvii', lg3lp, 0)[0],
	SDTest.Call('cLittlePoliceLocalLogic_GivesMoney', 'tbii', lg3lp, 0)[0], lg3lpf(lg3lp)))
lg3savedasks = lg3i(symva('g_LittlePoliceAsksForMoney'))
for lg3v in ((0, -1.0), (1, 1.0), (1, -1.0)):
	lg3poke(symva('g_LittlePoliceAsksForMoney'), lg3v[0])
	lg3fset(lg3lp + 0x60, lg3v[1])
	if lg3v[0] == 0 or (lg3gc1 and lg3gc3):
		check('LG3 cLittlePoliceLocalLogic_FindTarget%s' % fmt(lg3v), lambda: (SDTest.Call('cLittlePoliceLocalLogic_FindTarget', 'tii',
			lg3lp)[0] == lg3player, lg3lpf(lg3lp)))
lg3poke(symva('g_LittlePoliceAsksForMoney'), lg3savedasks)
check('LG3 LittlePolice asks restored', lambda: lg3i(symva('g_LittlePoliceAsksForMoney')) == lg3savedasks)
lg3free(lg3lp)

# ---- Mayor ----
lg3my = lg3mk(0xe8, sym('cMayorLocalLogic_vftable'))
for lg3v in (0.0, 1e9, -1.0):
	lg3fset(lg3my + 0x5c, lg3v)
	check('LG3 cMayorLocalLogic_TargetDone %s' % fmt(lg3v), lambda: SDTest.Call('cMayorLocalLogic_TargetDone', 'tbi', lg3my)[0])
check('LG3 cMayorLocalLogic_SetListener', lambda: (SDTest.Call('cMayorLocalLogic_SetListener', 'tvii', lg3my, lg3player)[0],
	SDTest.Peek(lg3my + 0x68, '@s'), SDTest.Call('cMayorLocalLogic_SetListener', 'tvii', lg3my, 0)[0], SDTest.Peek(lg3my + 0x68, '@s')))
lg3fset(lg3my + 0x60, 1.0)
check('LG3 cMayorLocalLogic_FindTarget later', lambda: SDTest.Call('cMayorLocalLogic_FindTarget', 'tii', lg3my)[0])
lg3savedmayor = lg3ints(symva('g_MayorTarget'), (0,)) + lg3ints(symva('g_MayorSearchEnt'), (0,)) + lg3ints(symva('g_MayorTargetDist'), (0,))
lg3fset(lg3my + 0x60, -1.0)
check('LG3 cMayorLocalLogic_FindTarget', lambda: (SDTest.Call('cMayorLocalLogic_FindTarget', 'tii', lg3my)[0] != 0, lg3i(symva('g_MayorSearchEnt')) ==
	lg3player))
lg3poke(symva('g_MayorTarget'), lg3savedmayor[0])
lg3poke(symva('g_MayorSearchEnt'), lg3savedmayor[1])
lg3poke(symva('g_MayorTargetDist'), lg3savedmayor[2])
lg3free(lg3my)

# ---- Maintenance ----
lg3mt = lg3mk(0xdc, sym('cMaintenanceLocalLogic_vftable'))
lg3set(lg3mt + 0xc8, (1.5, -250.25, 3.0))
lg3set(lg3mt + 0x30, (7.0, 8.0, 9.0))
check('LG3 cMaintenanceLocalLogic_VarV', lambda: (SDTest.Call('cMaintenanceLocalLogic_VarV', 'tbisDDDi', lg3mt, 'RepositionPoint', 0.0,
	0.0, 0.0, 0), SDTest.Call('cMaintenanceLocalLogic_VarV', 'tbisDDDi', lg3mt, 'Origin', 0.0, 0.0, 0.0, 0)))
lg3free(lg3mt)

# ---- the item list ----
lg3it = lg3z(0x64)
lg3k = 0
while lg3k < 0x19:
	lg3poke(lg3it + 4 * lg3k, -1)
	lg3k = lg3k + 1
check('LG3 cInfItem_ctor', lambda: (SDTest.Call('cInfItem_ctor', 'tii', lg3it)[0] == lg3it, lg3vt(lg3it),
	map(lambda i: lg3i(lg3it + 4 + 4 * i), range(24))))
# item descriptions: a string object and a value
def lg3item(s, v):
	d = lg3z(8)
	lg3cset(d, s)
	lg3poke(d + 4, v)
	return d
def lg3walk(first, nxt):
	r = []
	it = lg3z(4)
	d = first(it)
	while d:
		r.append((SDTest.Peek(lg3i(d), '@s'), lg3i(d + 4)))
		d = nxt(it)
	lg3free(it)
	return r
def lg3lives():
	return lg3walk(lambda it: SDTest.Call('cInfItem_FirstLife', 'tiii', lg3it, it)[0], lambda it: SDTest.Call('cInfItem_NextLife', 'tiii',
		lg3it, it)[0])
def lg3ammo(k):
	return lg3walk(lambda it, k=k: SDTest.Call('cInfItem_FirstAmmo', 'tiiii', lg3it, k, it)[0], lambda it, k=k: SDTest.Call(
		'cInfItem_NextAmmo', 'tiiii', lg3it, k, it)[0])
def lg3lists():
	return map(lambda o: lg3ints(lg3it + o, (8, 0x10)), (4, 0x1c, 0x34, 0x4c))
check('LG3 cInfItem empty walks', lambda: (lg3lives(), lg3ammo(0), lg3ammo(1), lg3ammo(2)))
lg3items = []
for lg3v in (('Life1', 10), ('Life2', 20), ('Life3', 30)):
	lg3d = lg3item(lg3v[0], lg3v[1])
	lg3items.append(lg3d)
	check('LG3 cInfItem_AddLife(%s)' % lg3v[0], lambda d=lg3d: (SDTest.Call('cInfItem_AddLife', 'tvii', lg3it, d)[0],
		lg3lists()))
for lg3v in ((0, 'Ammo0a', 1), (1, 'Ammo1a', 2), (2, 'Ammo2a', 3), (0, 'Ammo0b', 4), (2, 'Ammo2b', 5)):
	lg3d = lg3item(lg3v[1], lg3v[2])
	lg3items.append(lg3d)
	check('LG3 cInfItem_AddAmmo%s' % fmt(lg3v), lambda v=lg3v, d=lg3d: (SDTest.Call('cInfItem_AddAmmo', 'tviii', lg3it, v[0],
		d)[0], lg3lists()))
check('LG3 cInfItem walks', lambda: (lg3lives(), lg3ammo(0), lg3ammo(1), lg3ammo(2)))
# the free list of the life items keeps two nodes, the others none
lg3poke(lg3it + 0x18, 2)
check('LG3 InfItemList_Clear life', lambda: (SDTest.Call('InfItemList_Clear@14e0f0', 'tvi', lg3it + 4)[0], lg3lists(), lg3ints(lg3it +
	4, (0, 4, 0x14)), lg3lives()))
lg3d = lg3item('Life4', 40)
lg3items.append(lg3d)
check('LG3 InfItemList_Add pooled', lambda: (SDTest.Call('InfItemList_Add@14dfe0', 'tvii', lg3it + 4, lg3d)[0], lg3lists(),
	lg3lives()))
check('LG3 InfItemList_FreePool', lambda: (SDTest.Call('InfItemList_FreePool@14e1d0', 'tvi', lg3it + 4)[0], lg3lists()))
check('LG3 InfItemList_Clear ammo', lambda: (SDTest.Call('InfItemList_Clear', 'tvi', lg3it + 0x1c)[0], lg3lists(), lg3ammo(0)))
check('LG3 cInfItem_dtor', lambda: (SDTest.Call('cInfItem_dtor', 'tvi', lg3it)[0], lg3vt(lg3it), lg3lists()))
lg3free(lg3it)
for lg3d in lg3items:
	lg3cset(lg3d, None)
	lg3free(lg3d)
lg3it = lg3new(0x64)
check('LG3 cInfItem_Delete', lambda: (SDTest.Call('cInfItem_ctor', 'tii', lg3it)[0] == lg3it, SDTest.Call('cInfItem_Delete', 'tiii',
	lg3it, 1)[0] == lg3it))

# ---- the AI control on a fake object with empty lists ----
lg3gearcb = lg3i(symva('g_CreateGear') + 8) == 0 and lg3i(symva('g_CreatePolice') + 8) == 0
lg3savedbell = lg3i(symva('g_PoliceLogicBelligerant'))
lg3ls = lg3list()
lg3lg = lg3list()
lg3ac = lg3z(0x3c)
lg3k = 0
while lg3k < 0xf:
	lg3poke(lg3ac + 4 * lg3k, -1)
	lg3k = lg3k + 1
def lg3acf(a):
	return (lg3ints(a, (0, 4)), lg3b(a + 8), lg3i(a + 0xc) == lg3player, lg3fl(a + 0x10, 1), lg3bytes(a, (0x14, 0x15,
		0x16, 0x30, 0x31, 0x32)), lg3i(a + 0x18) == lg3player, lg3i(a + 0x1c) == a + 0x1c, lg3i(a + 0x20) == a + 0x1c,
		lg3i(a + 0x24), lg3i(a + 0x28) == a, lg3fl(a + 0x2c, 1))
if lg3gearcb:
	check('LG3 AICtrl_ctor', lambda: (SDTest.Call('AICtrl_ctor', 'tii', lg3ac)[0] == lg3ac, lg3acf(lg3ac), lg3ints(lg3ac,
		(0x34, 0x38)), lg3i(symva('g_PoliceLogicBelligerant'))))
else:
	lg3k = 0
	while lg3k < 0xf:
		lg3poke(lg3ac + 4 * lg3k, 0)
		lg3k = lg3k + 1
	lg3poke(lg3ac, -1)
	lg3poke(lg3ac + 0x1c, lg3ac + 0x1c)
	lg3poke(lg3ac + 0x20, lg3ac + 0x1c)
	lg3poke(lg3ac + 0x28, lg3ac)
lg3poke(symva('g_PoliceLogicBelligerant'), lg3savedbell)
lg3poke(lg3ac + 0x34, lg3ls)
lg3poke(lg3ac + 0x38, lg3lg)
check('LG3 AICtrl_RaiseLevel', lambda: (SDTest.Call('AICtrl_RaiseLevel', 'tvii', lg3ac, 2)[0], SDTest.Call('AICtrl_RaiseLevel', 'tvii',
	lg3ac, 1)[0], lg3i(lg3ac)))
check('LG3 AICtrl player', lambda: (SDTest.Call('AICtrl_CanFuse', 'tbii', lg3ac, lg3player)[0], SDTest.Call('AICtrl_CanFuse', 'tbii',
	lg3ac, 0)[0], SDTest.Call('AICtrl_CanBePolice', 'tbii', lg3ac, lg3player)[0], SDTest.Call('AICtrl_GearToPolice', 'tbii', lg3ac,
	lg3player)[0]))
lg3out = lg3z(4)
lg3poke(lg3out, -1)
check('LG3 AICtrl_Fuse player', lambda: (SDTest.Call('AICtrl_Fuse', 'tbiii', lg3ac, lg3player, lg3out)[0], lg3i(lg3out)))
lg3free(lg3out)
if lg3alarm == 0:
	check('LG3 AICtrl_SendAccuser', lambda: (SDTest.Call('AICtrl_SendAccuser', 'tbii', lg3ac, 0)[0], SDTest.Call('AICtrl_SendAccuser', 'tbii',
		lg3ac, 1)[0]))
	check('LG3 AICtrl_SendSentinelToNPC', lambda: (SDTest.Call('AICtrl_SendSentinelToNPC', 'tvii', lg3ac, lg3player)[0],
		lg3acf(lg3ac)))
lg3pos = lg3z(12)
lg3set(lg3pos, (10.0, 20.0, 30.0))
lg3fset(lg3ac + 0x2c, 1.0)
check('LG3 AICtrl_SendSentinelToWatch later', lambda: (SDTest.Call('AICtrl_SendSentinelToWatch', 'tvii', lg3ac, lg3pos)[0],
	lg3acf(lg3ac)))
check('LG3 AICtrl_WatchNow', lambda: (SDTest.Call('AICtrl_WatchNow', 'tvii', lg3ac, lg3pos)[0], lg3acf(lg3ac)))
check('LG3 AICtrl_UpdateSentinels', lambda: (SDTest.Call('AICtrl_UpdateSentinels', 'tvi', lg3ac)[0], lg3acf(lg3ac)))
check('LG3 AICtrl_UpdateGears', lambda: (SDTest.Call('AICtrl_UpdateGears', 'tvi', lg3ac)[0], lg3acf(lg3ac)))
if lg3gc2 and lg3gc3:
	check('LG3 AICtrl_GearsChase', lambda: (SDTest.Call('AICtrl_GearsChase', 'tvii', lg3ac, lg3player)[0], lg3acf(lg3ac)))
	check('LG3 AICtrl_Belligerant', lambda: (SDTest.Call('AICtrl_Belligerant', 'tvi', lg3ac)[0], lg3acf(lg3ac)))
	if lg3alarm == 0:
		lg3srand(33)
		for lg3v in (0.25, 0.125, 0.25, 0.375):
			check('LG3 AICtrl_Update %s' % fmt(lg3v), lambda v=lg3v: (SDTest.Call('AICtrl_Update', 'tvif', lg3ac, v)[0],
				lg3acf(lg3ac)))
		lg3pokeb(lg3ac + 8, 1)
		check('LG3 AICtrl_Think after the alarm', lambda: (SDTest.Call('AICtrl_Think', 'tvi', lg3ac)[0], lg3acf(lg3ac)))
lg3free(lg3pos)
check('LG3 AICtrl_dtor', lambda: (SDTest.Call('AICtrl_dtor', 'tvi', lg3ac)[0], lg3acf(lg3ac)))
lg3free(lg3ac)

# ---- the messengers ----
lg3ms = lg3z(0x14)
lg3poke(lg3ms + 0xc, 5)
lg3poke(lg3ms + 0x10, lg3ls)
lg3savedtube = lg3i(symva('g_MessengerTubeState'))
for lg3v in (0, 1):
	lg3poke(symva('g_MessengerTubeState'), lg3v)
	lg3srand(34 + lg3v)
	check('LG3 cMessengers_FindDesk tube %d' % lg3v, lambda: (SDTest.Call('cMessengers_FindDesk', 'tii', lg3ms)[0] != 0,
		lg3i(lg3ms + 0xc)))
lg3poke(symva('g_MessengerTubeState'), lg3savedtube)
check('LG3 cMessengers_SetStateAll', lambda: (SDTest.Call('cMessengers_SetStateAll', 'tviii', lg3ms, -1, 0)[0], SDTest.Call('cMessengers_SetStateAll',
	'tviii', lg3ms, 3, lg3player)[0]))
lg3free(lg3ms)

# ---- the nurses ----
lg3nu = lg3z(8)
lg3poke(lg3nu, -1)
check('LG3 cNurses_ctor', lambda: (SDTest.Call('cNurses_ctor', 'tii', lg3nu)[0] == lg3nu, lg3fl(lg3nu, 1)))
lg3poke(lg3nu + 4, lg3ls)
for lg3v in (0.5, 0.125, 0.5, 0.25):
	check('LG3 cNurses_Update %s' % fmt(lg3v), lambda v=lg3v: (SDTest.Call('cNurses_Update', 'tvif', lg3nu, v)[0],
		lg3fl(lg3nu, 1)))
check('LG3 cNurses_Think', lambda: SDTest.Call('cNurses_Think', 'tvi', lg3nu)[0])
lg3free(lg3nu)
lg3nl = lg3mk(0x4c, sym('cNurseLocalLogic_vftable'))
def lg3nlf(f):
	return (lg3i(f + 0x18), lg3bytes(f, (0x20, 0x28, 0x29, 0x2a)), lg3fl(f + 0x48, 1))
check('LG3 cNurseLocalLogic_OnRobbery', lambda: (SDTest.Call('cNurseLocalLogic_OnRobbery', 'tviif', lg3nl, lg3player, 0.0)[0],
	SDTest.Call('cNurseLocalLogic_OnRobbery', 'tviif', lg3nl, lg3player, -2.0)[0], lg3nlf(lg3nl)))
lg3pokeb(lg3nl + 0x29, 1)
check('LG3 cNurseLocalLogic busy hits', lambda: (SDTest.Call('cNurseLocalLogic_OnHit', 'tvii', lg3nl, lg3player)[0],
	SDTest.Call('cNurseLocalLogic_OnSeen', 'tvii', lg3nl, lg3player)[0], lg3nlf(lg3nl)))
lg3pokeb(lg3nl + 0x29, 0)
lg3savedrefuse = lg3i(symva('g_NursePoliceRefuse'))
lg3fset(symva('g_NursePoliceRefuse'), 1000.0)
lg3srand(36)
check('LG3 cNurseLocalLogic_GivesMoney refused', lambda: (map(lambda i: SDTest.Call('cNurseLocalLogic_GivesMoney', 'tbii', lg3nl,
	lg3player)[0], range(3)), lg3nlf(lg3nl)))
lg3poke(symva('g_NursePoliceRefuse'), lg3savedrefuse)
if lg3alarm == 0:
	check('LG3 cNurseLocalLogic_Update relax', lambda: (SDTest.Call('cNurseLocalLogic_Update', 'tvif', lg3nl, 0.125)[0],
		lg3nlf(lg3nl), lg3pf()))
	lg3esc = lg3fl(symva('g_NurseEscapeTime'), 1)[0]
	if lg3esc > 0.5:
		lg3poke(lg3nl + 0x18, 2)
		check('LG3 cNurseLocalLogic_Update escape', lambda: (SDTest.Call('cNurseLocalLogic_Update', 'tvif', lg3nl, 0.125)[0],
			SDTest.Call('cNurseLocalLogic_Update', 'tvif', lg3nl, 0.25)[0], lg3nlf(lg3nl), lg3pf()))
lg3prestore(lg3psaved)
lg3free(lg3nl)

# ---- the money string ----
lg3mo = lg3z(0x1c)
if lg3i(symva('g_OnMoneyReachCallback') + 8) == 0:
	check('LG3 cStrMoney_ctor', lambda: (SDTest.Call('cStrMoney_ctor', 'tii', lg3mo)[0] == lg3mo, SDTest.Peek(lg3mo, '@s'),
		lg3bytes(lg3mo, (0x10, 0x11)), lg3fl(lg3mo + 0x14, 1), lg3i(lg3mo + 0x18)))
else:
	SDTest.Call('crt_memcpy', 'iisi', lg3mo, '0000000000000000', 16)
	lg3poke(lg3mo + 0x10, 0x100)
def lg3mof(m):
	return (SDTest.Peek(m, '@s'), lg3bytes(m, (0x10, 0x11)), lg3fl(m + 0x14, 1), lg3i(m + 0x18),
		SDTest.Call('cStrMoney_GetStr', 'tsi', m)[0])
check('LG3 cStrMoney_GetStr', lambda: lg3mof(lg3mo))
lg3poke(lg3mo + 0x11, 0)
check('LG3 cStrMoney_GetStr none', lambda: SDTest.Call('cStrMoney_GetStr', 'tsi', lg3mo)[0])
for lg3v in (1234, 1234, 1234, 1234, 1234, 907, -5, 2000000000):
	lg3poke(lg3mo + 0x18, lg3v)
	check('LG3 cStrMoney_UpdateDigits %d' % lg3v, lambda: (SDTest.Call('cStrMoney_UpdateDigits', 'tvi', lg3mo)[0], lg3mof(lg3mo)))
if lg3i(symva('g_OnMoneyReachCallback') + 8) == 0:
	for lg3v in ((55, 0.5), (55, 0.25), (-3, 0.5), (12, 1.0)):
		lg3poke(lg3mo + 0x18, lg3v[0])
		check('LG3 cStrMoney_Update%s' % fmt(lg3v), lambda v=lg3v: (SDTest.Call('cStrMoney_Update', 'tvif', lg3mo, v[1])[0],
			lg3mof(lg3mo)))
lg3free(lg3mo)

# ---- a human logic made and deleted ----
lg3savedhuman = lg3i(symva('g_HumanEnt'))
lg3hu = lg3new(0x5c)
check('LG3 cHumanLocalLogic_ctor', lambda: (SDTest.Call('cHumanLocalLogic_ctor', 'tiii', lg3hu, lg3player)[0] == lg3hu, lg3vt(lg3hu),
	lg3i(lg3hu + 4) == lg3player, lg3i(symva('g_HumanEnt')) == lg3player, lg3i(lg3hu + 0x48 + 8) != 0, lg3b(lg3hu + 0x48 + 0x10)))
check('LG3 cHumanLocalLogic_Delete', lambda: SDTest.Call('cHumanLocalLogic_Delete', 'tiii', lg3hu, 1)[0] == lg3hu)
lg3poke(symva('g_HumanEnt'), lg3savedhuman)

# ---- Humphrey on a fake functionary logic ----
lg3hf = lg3mk(0xbc, sym('cHumphreyLocalLogic_vftable'))
lg3node(lg3hf, 0x68)
check('LG3 cHumphreyLocalLogic_AssignDesktop none', lambda: (SDTest.Call('cHumphreyLocalLogic_AssignDesktop', 'tvif', lg3hf, 0.0)[0],
	lg3ints(lg3hf, (0x70, 0x78))))
check('LG3 cHumphreyLocalLogic_ReleaseDesktop none', lambda: (SDTest.Call('cHumphreyLocalLogic_ReleaseDesktop', 'tvi', lg3hf)[0], lg3ints(lg3hf,
	(0x70, 0x78))))
lg3poke(lg3hf + 0x78, lg3player)
lg3out = lg3z(12)
check('LG3 cHumphreyLocalLogic_GetWorkAng', lambda: (SDTest.Call('cHumphreyLocalLogic_GetWorkAng', 'tiii', lg3hf, lg3out)[0] == lg3out,
	lg3fl(lg3out, 3)))
check('LG3 cHumphreyLocalLogic_AssignDesktop forced', lambda: (SDTest.Call('cHumphreyLocalLogic_AssignDesktop', 'tvif', lg3hf, 0.0)[0],
	lg3ints(lg3hf, (0x70, 0x78)) == (0, lg3player)))
lg3free(lg3out)
lg3free(lg3hf)

# ---- the logic control ----
check('LG3 Logic small', lambda: (SDTest.Call('Logic_SetOnFloor', 'bi', 0)[0], SDTest.Call('Logic_SetOriginCallback', 'bi', 0)[0]))
lg3lc = lg3z(0x1c8)
lg3set(lg3lc + 8, (0.25, 0.0, 0.5, 2.0))
check('LG3 Logic_UpdateTaunts', lambda: (SDTest.Call('Logic_UpdateTaunts', 'tvif', lg3lc, 0.125)[0], lg3fl(lg3lc + 8, 4),
	lg3b(lg3lc + 4)))
lg3free(lg3lc)

# ---- the Delete of the logics on zeroed memory ----
for lg3v in (('cLittlePoliceLocalLogic_Delete', 0x78, sym('cLittlePoliceLocalLogic_vftable')), ('cMaintenanceLocalLogic_Delete', 0xdc, sym('cMaintenanceLocalLogic_vftable')), ('cMayorLocalLogic_Delete', 0xe8, sym('cMayorLocalLogic_vftable')),
		('cMercenaryLocalLogic_Delete', 0x68, sym('cMercenaryLocalLogic_vftable')), ('cMessengerLocalLogic_Delete', 0x68, sym('cMessengerLocalLogic_vftable')), ('cNurseLocalLogic_Delete', 0x4c, sym('cNurseLocalLogic_vftable'))):
	lg3d = lg3new(lg3v[1])
	lg3k = 0
	while lg3k < lg3v[1] / 4:
		lg3poke(lg3d + 4 * lg3k, 0)
		lg3k = lg3k + 1
	lg3poke(lg3d, 0x400000 + lg3v[2])
	lg3poke(lg3d + 4, lg3player)
	lg3node(lg3d, 8)
	lg3savedmt = lg3ints(symva('g_MaintenanceShip'), (0,)) + lg3ints(symva('g_MaintenanceFirstShip'), (0,))
	check('LG3 Delete %s' % lg3v[0], lambda v=lg3v, d=lg3d: (SDTest.Call(v[0], 'tiii', d, 1)[0] == d,
		lg3ints(symva('g_MaintenanceShip'), (0,)) + lg3ints(symva('g_MaintenanceFirstShip'), (0,))))
	lg3poke(symva('g_MaintenanceShip'), lg3savedmt[0])
	lg3poke(symva('g_MaintenanceFirstShip'), lg3savedmt[1])
lg3d = lg3new(0xbc)
lg3k = 0
while lg3k < 0x2f:
	lg3poke(lg3d + 4 * lg3k, 0)
	lg3k = lg3k + 1
lg3poke(lg3d, 0x400000 + sym('cHumphreyLocalLogic_vftable'))
lg3poke(lg3d + 4, lg3player)
lg3node(lg3d, 8)
lg3node(lg3d, 0x68)
lg3saveddesklist = lg3i(symva('g_DesktopsList'))
check('LG3 cHumphreyLocalLogic_Delete', lambda: SDTest.Call('cHumphreyLocalLogic_Delete', 'tiii', lg3d, 1)[0] == lg3d)
lg3poke(symva('g_DesktopsList'), lg3saveddesklist)

lg3free(lg3ls)
lg3free(lg3lg)
check('LG3 player restored', lambda: lg3psave() == lg3psaved)
