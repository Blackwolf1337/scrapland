# game World/World.c (execfile'd by sdlevel.py, Python 1.5.2): cWorld in the level FZ. The world, the player
# (Player0) and the entity list masks come from the exe; pointers are only compared or turned into entity names.
# Weapon tables and the ignore list are restored, scratch weapons live in calloc memory.
def wpeek(a):
	return SDTest.Peek(a, '@i')
def wname(e):
	if e:
		return SDTest.Peek(e + 4, '@p')
	return None
def wcalloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def wfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def wfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def wints(a, n):
	return map(lambda i, a=a: wpeek(a + 4 * i), range(n))
def wadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
# the hovering vehicle Parked moves with the time since the level started: no positions that depend on it
def wstable(name, v):
	if name == 'Parked':
		return 'moving'
	return v
world = SDTest.Peek('g_pWorld', 'i')
wplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', world, 0)[0]
check('world player', lambda: wname(wplayer))
wp = (pp[0], pp[1], pp[2])

# pure helpers
for v in ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (0.3, -0.5, 0.8), (-120.5, 33.25, 7.0)):
	for ax, ay in ((0.0, 0.0), (0.5, 1.0), (-1.2, 2.9), (3.14159, -3.14159), (10.0, 100.0)):
		check('Vec3_DotViewDir' + fmt((v, ax, ay)), lambda v=v, ax=ax, ay=ay: SDTest.Call('Vec3_DotViewDir', 'fFff', v, ax, ay))
for a in range(6):
	for b in range(6):
		check('Team_IsFriend/IsEnemy' + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('Team_IsFriend', 'bii', a, b)[0],
			SDTest.Call('Team_IsEnemy', 'bii', a, b)[0]))
check('World_IsEasySingle/IsHardSingle', lambda: (SDTest.Call('World_IsEasySingle', 'b')[0], SDTest.Call('World_IsHardSingle', 'b')[0]))

# the weapon and ammo tables of the level by rva (the getters with variables the script module does not try)
wweaps = wpeek(world + 0x1bbc)
wnweap = wpeek(world + 0x1bb8)
wammos = wpeek(world + 0x1bc8)
wnammo = wpeek(world + 0x1bc4)
check('world tables', lambda: (wnweap, wnammo, wpeek(world + 0x1bc0)))
wfvars = ('MinAmmo', 'minammo', 'iPrev', 'iNext', 'iAmmo', 'Upgrd00', 'Upgrd01', 'Upgrd02', 'Upgrd16', 'Upgrd20',
	'UPGRD03', 'Upgrd1x', 'Upgrdx1', 'Upgrd123', 'Upgrd5', 'Upgrc01', 'Max', 'Charger', '')
for i in range(wnweap):
	w = wweaps + 0xb0 * i
	for v in wfvars:
		check('cWeapInfo_GetF' + fmt((i, v)), lambda w=w, v=v: SDTest.Call('cWeapInfo_GetF', 'tiisF', w, v, (-7.0,)))
	for v in ('Name', 'OnPress', 'onhit', 'OnStart', 'OnEnd', 'SetFx', 'Max', ''):
		check('cWeapInfo_GetS' + fmt((i, v)), lambda w=w, v=v: (lambda r: (r[0], r[1] and SDTest.Peek(r[1], '@s')))(
			SDTest.Call('cWeapInfo_GetS', 'tiisp', w, v, 0)))
for i in range(wnammo):
	a = wammos + 0x5c * i
	for v in ('Max', 'max', 'Charger', 'MinAmmo', 'Upgrd01', ''):
		check('cAmmoInfo_GetF' + fmt((i, v)), lambda a=a, v=v: SDTest.Call('cAmmoInfo_GetF', 'tiisF', a, v, (-7.0,)))
	for v in ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'OnStart', ''):
		check('cAmmoInfo_GetS' + fmt((i, v)), lambda a=a, v=v: (lambda r: (r[0], r[1] and SDTest.Peek(r[1], '@s')))(
			SDTest.Call('cAmmoInfo_GetS', 'tiisp', a, v, 0)))
if wnweap > 0:
	w = wweaps
	wold = map(lambda v, w=w: SDTest.Call('cWeapInfo_GetF', 'tiisF', w, v, (0.0,))[1][0], ('Upgrd02', 'iAmmo', 'MinAmmo'))
	for v, x in (('Upgrd02', 1.0), ('Upgrd02', 0.5), ('Upgrd02', 0.75), ('iAmmo', 2.9), ('iAmmo', -1.7),
			('MinAmmo', 12.5), ('Upgrd1x', 1.0), ('Bogus', 1.0)):
		check('cWeapInfo_SetF' + fmt((v, x)), lambda w=w, v=v, x=x: (SDTest.Call('cWeapInfo_SetF', 'tiisf', w, v, x)[0],
			SDTest.Call('cWeapInfo_GetF', 'tiisF', w, v, (0.0,))))
	SDTest.Call('cWeapInfo_SetF', 'tiisf', w, 'Upgrd02', wold[0])
	SDTest.Call('cWeapInfo_SetF', 'tiisf', w, 'iAmmo', wold[1])
	SDTest.Call('cWeapInfo_SetF', 'tiisf', w, 'MinAmmo', wold[2])
	check('cWeapInfo restored', lambda w=w: map(lambda v, w=w: SDTest.Call('cWeapInfo_GetF', 'tiisF', w, v, (0.0,))[1][0],
		('Upgrd02', 'iAmmo', 'MinAmmo')))
if wnammo > 0:
	a = wammos
	wold = map(lambda v, a=a: SDTest.Call('cAmmoInfo_GetF', 'tiisF', a, v, (0.0,))[1][0], ('Max', 'Charger'))
	for v, x in (('Max', 77.25), ('Charger', -3.5), ('Bogus', 1.0)):
		check('cAmmoInfo_SetF' + fmt((v, x)), lambda a=a, v=v, x=x: (SDTest.Call('cAmmoInfo_SetF', 'tiisf', a, v, x)[0],
			SDTest.Call('cAmmoInfo_GetF', 'tiisF', a, v, (0.0,))))
	SDTest.Call('cAmmoInfo_SetF', 'tiisf', a, 'Max', wold[0])
	SDTest.Call('cAmmoInfo_SetF', 'tiisf', a, 'Charger', wold[1])

# constructors on scratch memory filled with 0x55 (the destructors 0x19fa70/0x19fa40 free the strings again)
for ctor, dtor, size, setS, getS in ((sym('cWeapInfo_ctor'), sym('cWeapInfo_dtor'), 0xb0, sym('cWeapInfo_SetS'), sym('cWeapInfo_GetS')),
		(sym('cAmmoInfo_ctor'), sym('cAmmoInfo_dtor'), 0x5c, sym('cAmmoInfo_SetS'), sym('cAmmoInfo_GetS'))):
	m = wcalloc(size)
	SDTest.Call('crt_memset', 'iiii', m, 0x55, size)
	check('ctor %x' % ctor, lambda ctor=ctor, m=m: SDTest.Call(ctor, 'tii', m)[0] == m)
	if size == 0xb0:
		check('cWeapInfo_ctor fields', lambda m=m: (wpeek(m), wints(m + 0x90, 8), wints(m + 0x0c, 2), wints(m + 0x84, 2)))
	else:
		check('cAmmoInfo_ctor fields', lambda m=m: (wpeek(m), wints(m + 0x4, 2), wints(m + 0x14, 2), wints(m + 0x50, 2)))
	for v, s in (('Name', 'SDWeap'), ('name', 'SDWeap2'), ('OnPress', 'SDNoModule.x'), ('OnHit', ''), ('SetFx', 'x'),
			('Bogus', 'x')):
		check('SetS %x' % setS + fmt((v, s)), lambda setS=setS, getS=getS, m=m, v=v, s=s: (SDTest.Call(setS, 'tiiss', m, v,
			s)[0], (lambda r: (r[0], r[1] and SDTest.Peek(r[1], '@s')))(SDTest.Call(getS, 'tiisp', m, v, 0))))
	SDTest.Call(dtor, 'tvi', m)
	wfree(m)
# Table<cWeapInfo>/<cAmmoInfo> made with Init (rva 0x19fd40, 0x19fc60) and freed
for init, free, n in ((sym('WeapTable_Init'), sym('WeapTable_Free'), 3), (sym('AmmoTable_Init'), sym('AmmoTable_Free'), 2), (sym('AmmoTable_Init'), sym('AmmoTable_Free'), 1)):
	t = wcalloc(8)
	SDTest.Call(init, 'tvii', t, n)
	check('Table init %x' % init, lambda t=t: (wpeek(t), wpeek(t + 4) != 0))
	SDTest.Call(free, 'tvi', t)
	check('Table_Free %x' % free, lambda t=t: wints(t, 2))
	SDTest.Call(free, 'tvi', t)
	wfree(t)

# names, cameras, lists
for i in range(4):
	check('cWorld_GetCamName(%d)' % i, lambda i=i: SDTest.Call('cWorld_GetCamName', 'tsii', world, i)[0])
wfirst = SDTest.Call('cWorld_GetFirstEntity', 'tii', world)[0]
check('cWorld_GetFirstEntity', lambda: wname(wfirst))
for l in ('Traffic', 'Enemies', 'SDList', 'traffic', 'SDNoList', ''):
	check('cWorld_GetList(%s)' % l, lambda l=l: SDTest.Call('cWorld_GetList', 'tiis', world, l)[0] != 0)
for p in (wp, (0.0, 0.0, 0.0), (1000.0, 2000.0, 3000.0), (-1e30, 5.0, 1e30)):
	check('cWorld_GetMinCamDist' + fmt(p), lambda p=p: (SDTest.Call('cWorld_GetMinCamDist', 'tfiF', world, p)[0],
		SDTest.Call('cWorld_GetMinCamDistSq', 'tfiF', world, p)[0]))
for n in ('Player0', 'SDNoEntity', '', 'Parked', wname(wfirst)):
	check('cWorld_UniqueName(%s)' % n, lambda n=n: (SDTest.Call('cWorld_UniqueName', 'tsis', world, n)[0], wpeek(world + 0x30)))
	check('cWorld_GetObjData(%s)' % n, lambda n=n: SDTest.Call('cWorld_GetObjData', 'tiis', world, n)[0] != 0)
check('cWorld_ShowDebrisInfo', lambda: SDTest.Call('cWorld_ShowDebrisInfo', 'tviii', world, 10, 20))
check('World_InputEvents', lambda: SDTest.Call('World_InputEvents', 'v'))
check('cWorld_UpdateListeners', lambda: (SDTest.Call('cWorld_UpdateListeners', 'tvif', world, 0.05), wfl(world + 0x254, 12)))

# the entity queries on the grid: masks of the lists and of the player
wmasks = (SDTest.Call('EntityLists_Get', 'is', 'Traffic')[0], SDTest.Call('EntityLists_Get', 'is', 'Enemies')[0], wpeek(wplayer + 0x48))
check('world masks', lambda: wmasks)
wsegs = ((wp, wadd(wp, (0.0, -5000.0, 0.0))), (wadd(wp, (-3000.0, 50.0, 0.0)), wadd(wp, (3000.0, 50.0, 0.0))),
	(wadd(wp, (0.0, 300.0, -2000.0)), wadd(wp, (0.0, -300.0, 2000.0))), ((0.0, 0.0, 0.0), (0.0, 1.0, 0.0)))
sdwcalls = []
def sdwcb(*args):
	sdwcalls.append(args)
def wtline(m, a, b, ex, flag):
	r = SDTest.Call('cWorld_TestLineEntities', 'tbiiiFFFpi', world, ex, m, a, b, (0.0, 0.0, 0.0), 0, flag)
	n = wname(r[4])
	return (r[0], wstable(n, r[3]), n, wpeek(world + 0x284) & 0xff)
def wtsphere(m, c, r, ex, a, b):
	x = SDTest.Call('cWorld_TestSphereEntities', 'tbiiiFfpii', world, ex, m, c, r, 0, a, b)
	return (x[0], wname(x[2]), wpeek(world + 0x284) & 0xff)
for m in wmasks:
	for a, b in wsegs:
		for ex in (0, wplayer):
			check('cWorld_TestLineEntities' + fmt((m, a, b, ex != 0)), lambda m=m, a=a, b=b, ex=ex: wtline(m, a, b, ex, 3))
		for r in (10.0, 400.0, 3000.0):
			check('cWorld_LineFirst' + fmt((m, a, b, r)), lambda m=m, a=a, b=b, r=r: (lambda x: (x[0], wname(x[3])))(
				SDTest.Call('cWorld_LineFirst', 'tbiiiFFfp', world, wplayer, m, a, b, r, 0)))
			check('cWorld_LineCall' + fmt((m, a, b, r)), lambda m=m, a=a, b=b, r=r: (SDTest.Call('cWorld_LineCall', 'tbiiFFfs', world,
				m, a, b, r, '__main__.sdwcb')[0], len(sdwcalls)))
	for c in (wp, wadd(wp, (500.0, 0.0, 0.0)), (0.0, 0.0, 0.0)):
		for r in (10.0, 500.0, 5000.0):
			# b goes to the TestSphere of the entities as a pointer: only 0
			for ex, a, b in ((0, 1, 0), (wplayer, 0, 0), (wplayer, 2, 0)):
				check('cWorld_TestSphereEntities' + fmt((m, c, r, ex != 0, a, b)), lambda m=m, c=c, r=r, ex=ex, a=a, b=b:
					wtsphere(m, c, r, ex, a, b))
			for f in (0, 1):
				check('cWorld_SphereCall' + fmt((m, c, r, f)), lambda m=m, c=c, r=r, f=f: (SDTest.Call('cWorld_SphereCall', 'tbiiFfsi',
					world, m, c, r, '__main__.sdwcb', f)[0], len(sdwcalls)))
check('callbacks', lambda: map(lambda c: wstable(c[0], c), sdwcalls))
# the ignore list: one entry, emptied by the next line or sphere test
check('cWorld_AddIgnore(player)', lambda: (SDTest.Call('cWorld_AddIgnore', 'tvii', world, wplayer),
	SDTest.Call('cWorld_IsIgnored', 'tbii', world, wplayer)[0], SDTest.Call('cWorld_IsIgnored', 'tbii', world, wfirst)[0]))
check('cWorld_TestSphereEntities ignored', lambda: wtsphere(wmasks[2], wp, 300.0, 0, 1, 0))
check('cWorld_IsIgnored after', lambda: SDTest.Call('cWorld_IsIgnored', 'tbii', world, wplayer)[0])
check('cWorld_AddIgnore(player) 2', lambda: SDTest.Call('cWorld_AddIgnore', 'tvii', world, wplayer))
check('cWorld_TestLineEntities ignored', lambda: wtline(wmasks[2], wadd(wp, (0.0, 500.0, 0.0)), wadd(wp,
	(0.0, -500.0, 0.0)), 0, 1))
check('cWorld_IsIgnored after 2', lambda: SDTest.Call('cWorld_IsIgnored', 'tbii', world, wplayer)[0])

# the "Car" in view of the player
def wview(mode, rng, mind):
	x = SDTest.Call('Entity_FindInView', 'iifFFif', wplayer, rng, (-9.0,), (-9.0,), mode, mind)
	n = wname(x[0])
	return (n, wstable(n, (x[1], x[2])), x[1][0] != -9.0, x[2][0] != -9.0)
for mode in (0, 1, 2, 3):
	for rng, mind in ((1000.0, -1.0), (100000.0, -1.0), (100000.0, 0.5), (1.0, -2.0)):
		check('Entity_FindInView' + fmt((mode, rng, mind)), lambda mode=mode, rng=rng, mind=mind: wview(mode, rng, mind))
