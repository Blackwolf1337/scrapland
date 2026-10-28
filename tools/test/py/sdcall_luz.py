# engine3d Engine/Luz.c (run by sdcall.py after its own checks, its helpers in scope). Lights in heap memory live in
# the global list of the engine while they exist and are all destroyed at the end; the device calls go to the real
# device of the main menu. Nodes are fake (Refs kept above 1, a matrix at 0x5c, no animation at 0x40). Engine
# variables are changed for some checks and restored. Not called: Luz_SetDefault (render state of the menu),
# CLuz_Write (needs a file to write), CLuz_Draw (not in C).
def lalloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def lfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def wf(addr, floats):
	SDTest.Call('crt_memcpy', 'iiFi', addr, floats, 4 * len(floats))
def wi(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def gget(rva):
	return SDTest.Peek(rva, 'i')
def lget(l):
	# 0x04..0xbf: everything but the vftable and the node pointers
	return tpeek(l + 4, 0x2f)
def lcount():
	return SDTest.Call('Luz_NumD3D', 'i')[0], SDTest.Call('Luz_NumActivas', 'i')[0], SDTest.Call('Luz_NumTotal', 'i')[0]
def lstep(name, l, rva, sig, *args):
	check(name + fmt(args), lambda l=l, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, l) + args)[0], lget(l), lcount()))
# members: tipo, sombra, pos, dir, color (int), intensidad, rango, theta, phi, mult, sector
def lset(l, tipo, pos, dir, color, intens, rango, theta=43.0, phi=45.0, mult=1.0, sector=-1):
	wi(l + 8, tipo)
	wf(l + 0x10, pos + dir)
	wi(l + 0x28, color)
	wf(l + 0x2c, (intens,))
	wf(l + 0x34, (rango, theta, phi, mult))
	wi(l + 0xc0, sector)
# engine variables (rva) and their values to put back
lvars = (sym('g_LightEnaD3D'), sym('g_LightAmbient'), sym('g_LightMult'), sym('g_SpotlightClamp'), sym('g_LightDesat'), sym('g_SpotlightAdjust'), sym('g_LightAtten'), sym('g_LightGamma'), sym('g_SpotlightMult'), sym('g_LightSortI'),
	sym('g_LightAtten2'), sym('g_SpotlightAng'), sym('g_SpotlightAtten'), sym('g_LightMult2'), sym('g_LightTarget'), sym('g_LightDefer'), sym('g_LightSector'), sym('g_LightSort'))
lsaved = []
for v in lvars:
	lsaved.append(gget(v))
def lrestore():
	for i in range(len(lvars)):
		SDTest.Poke(lvars[i], lsaved[i])
def gf(rva, f):
	wf(rva + 0x400000, (f,))

check('Luz counts before', lcount)
NL = 6
luces = []
for i in range(NL):
	luces.append(lalloc(0xcc))
for i in range(NL):
	check('CLuz_ctor' + fmt((i,)), lambda l=luces[i]: (SDTest.Call('CLuz_ctor', 'tii', l)[0] == l, lget(l), lcount()))
lnodes = []
for i in range(3):
	n = lalloc(0xa0)
	wi(n + 8, 5)
	wf(n + 0x5c, affine())
	lnodes.append(n)
def lrefs():
	r = []
	for n in lnodes:
		r.append(SDTest.Peek(n + 8, '@i'))
	return r
def lnodo(l):
	r = []
	for off in (0xc4, 0xc8):
		p = SDTest.Peek(l + off, '@i')
		k = -1
		for i in range(len(lnodes)):
			if lnodes[i] == p:
				k = i
		if p == 0:
			k = None
		r.append(k)
	return r

# FillD3D and AdjustD3D on a buffer, with random members and several settings of the engine variables
ld3d = lalloc(0x68)
l0 = luces[0]
def lfill(l):
	SDTest.Call('CLuz_FillD3D', 'tvii', l, ld3d)
	return tpeek(ld3d, 26)
def ladjust(l):
	SDTest.Call('CLuz_FillD3D', 'tvii', l, ld3d)
	SDTest.Call('CLuz_AdjustD3D', 'tvii', l, ld3d)
	return tpeek(ld3d, 26)
cols = (0x00ffffff, 0x80402010, 0x12345678, 0, 0xff00ff, 0x7f7f7f7f)
for i in range(12):
	tipo = 5000 + i % 3
	rango = 0.0
	if i % 3 == 1:
		rango = rnd(1.0, 100.0)
	elif i % 4 == 3:
		rango = rnd(0.1, 5.0)
	lset(l0, tipo, vec(), vec(), cols[i % len(cols)], rnd(0.0, 4.0), rango, rnd(0.0, 90.0), rnd(0.0, 90.0), rnd(0.5, 2.0))
	check('CLuz_FillD3D' + fmt((i,)), lambda: lfill(l0))
	check('CLuz_AdjustD3D' + fmt((i,)), lambda: ladjust(l0))
	SDTest.Poke('g_LightAtten2', 1)
	check('CLuz_AdjustD3D atten2' + fmt((i,)), lambda: ladjust(l0))
	SDTest.Poke('g_LightAtten2', 0)
	gf(sym('g_LightGamma'), rnd(0.5, 2.5))
	gf(sym('g_LightDesat'), rnd(0.0, 1.0))
	gf(sym('g_SpotlightClamp'), rnd(0.1, 2.0))
	gf(sym('g_LightAmbient'), rnd(0.0, 0.5))
	gf(sym('g_LightMult'), rnd(0.5, 2.0))
	gf(sym('g_SpotlightMult'), rnd(0.5, 2.0))
	gf(sym('g_LightAtten'), rnd(1.0, 10.0))
	gf(sym('g_SpotlightAtten'), rnd(1.0, 20.0))
	gf(sym('g_LightMult2'), rnd(0.5, 2.0))
	check('CLuz_AdjustD3D vars' + fmt((i,)), lambda: ladjust(l0))
	SDTest.Poke('g_SpotlightAdjust', 0)
	check('CLuz_AdjustD3D no spot adjust' + fmt((i,)), lambda: ladjust(l0))
	SDTest.Poke('g_LightAtten2', 1)
	check('CLuz_AdjustD3D vars atten2' + fmt((i,)), lambda: ladjust(l0))
	lrestore()
# R_LightMultName: the multiplier only for the lights whose node name matches
lname = lalloc(16)
lpat = lalloc(16)
wi(lname, 0x696e6d6f)	# "omni02"
wi(lname + 4, 0x3230)
wi(lpat, 0x696e6d6f)	# "omni*"
wi(lpat + 4, 0x2a)
wi(lnodes[0] + 4, lname)
SDTest.Call('CLuz_SetNodo', 'tvii', l0, lnodes[0])
lsavedname = gget(sym('g_LightMultName'))
gf(sym('g_LightMult'), 3.0)
lset(l0, 5000, vec(), vec(), 0x00ffffff, 1.0, 0.0)
SDTest.Poke('g_LightMultName', lpat)
check('CLuz_AdjustD3D name match', lambda: ladjust(l0))
wi(lpat, 0x6f636f66)	# "foco"
check('CLuz_AdjustD3D name no match', lambda: ladjust(l0))
SDTest.Poke('g_LightMultName', lsavedname)
lrestore()
check('CLuz_SetNodo', lambda: (lrefs(), lnodo(l0)))

# the enable cache and the device light: R_LightDefer and R_LightEnaD3D
l1 = luces[1]
for defer in (1, 0):
	for ena in (1, 0, 2):
		SDTest.Poke('g_LightDefer', defer)
		SDTest.Poke('g_LightEnaD3D', ena)
		nm = ' defer %d ena %d' % (defer, ena)
		lstep('CLuz_UpdateD3D hide' + nm, l1, sym('CLuz_UpdateD3D'), 'tvii', 1)
		lstep('CLuz_UpdateD3D' + nm, l1, sym('CLuz_UpdateD3D'), 'tvii', 0)
		lstep('CLuz_EnableD3D 1' + nm, l1, sym('CLuz_EnableD3D'), 'tvii', 1)
		lstep('CLuz_EnableD3D 1 again' + nm, l1, sym('CLuz_EnableD3D'), 'tvii', 1)
		lstep('CLuz_EnableD3D 0' + nm, l1, sym('CLuz_EnableD3D'), 'tvii', 0)
		lstep('CLuz_Hide' + nm, l1, sym('CLuz_Hide'), 'tvii', 0)
		lstep('CLuz_UpdateD3D after hide' + nm, l1, sym('CLuz_UpdateD3D'), 'tvii', 0)
		check('Luz_EnableAll 1' + nm, lambda: (SDTest.Call('Luz_EnableAll', 'vi', 1), map(lget, luces), lcount()))
		check('Luz_EnableAll 0' + nm, lambda: (SDTest.Call('Luz_EnableAll', 'vi', 0), map(lget, luces), lcount()))
lrestore()
# the global list
lstep('CLuz_SetActiva 0', l1, sym('CLuz_SetActiva'), 'tvii', 0)
lstep('CLuz_SetActiva 0 again', l1, sym('CLuz_SetActiva'), 'tvii', 0)
check('CLuz_SetActiva others', lambda: map(lget, luces))
lstep('CLuz_SetActiva 1', l1, sym('CLuz_SetActiva'), 'tvii', 1)
lstep('CLuz_SetActiva 1 again', l1, sym('CLuz_SetActiva'), 'tvii', 1)
lstep('CLuz_SetActiva 0 last', luces[NL - 1], sym('CLuz_SetActiva'), 'tvii', 0)
lstep('CLuz_SetActiva 1 last', luces[NL - 1], sym('CLuz_SetActiva'), 'tvii', 1)
check('Luz_List', lambda: SDTest.Call('Luz_List', 'v'))

# Update with and without nodes; range 0 or directional, so the frustum test of the camera is not reached
l2 = luces[2]
for i in range(8):
	lset(l2, (5001, 5002, 5000)[i % 3], vec(), vec(), cols[i % len(cols)], rnd(0.0, 3.0), 0.0, rnd(0.0, 90.0), rnd(0.0, 90.0))
	wf(lnodes[1] + 0x5c, affine())
	wf(lnodes[2] + 0x5c, affine())
	if i == 0:
		lstep('CLuz_Update no node', l2, sym('CLuz_Update'), 'tvii', 0)
		SDTest.Call('CLuz_SetNodo', 'tvii', l2, lnodes[1])
	if i == 2:
		SDTest.Call('CLuz_SetNodoObjetivo', 'tvii', l2, lnodes[2])
	if i == 5:
		SDTest.Poke('g_LightTarget', 0)
	lstep('CLuz_Update' + fmt((i,)), l2, sym('CLuz_Update'), 'tvii', 0)
	lstep('CLuz_Reset' + fmt((i,)), l2, sym('CLuz_Reset'), 'tvii', 0)
lrestore()
lset(l2, 5002, vec(), vec(), 0x00ffffff, 1.0, 0.0)
wi(l2 + 0xc0, 3)
SDTest.Poke('g_LightSector', 0)
lstep('CLuz_Update sector off', l2, sym('CLuz_Update'), 'tvii', 0)
lrestore()
check('CLuz_SetNodoObjetivo', lambda: (lrefs(), lnodo(l2)))
SDTest.Call('CLuz_SetNodoObjetivo', 'tvii', l2, 0)
SDTest.Call('CLuz_SetNodo', 'tvii', l2, 0)
check('CLuz_SetNodo NULL', lambda: (lrefs(), lnodo(l2)))
wi(l2 + 0xc0, -1)
SDTest.Call('CLuz_UpdateD3D', 'tvii', l2, 0)

# qsort order by peso
lcell = lalloc(8)
wi(lcell, luces[3])
wi(lcell + 4, luces[4])
for a, b in ((1.0, 2.0), (2.0, 1.0), (1.0, 1.0), (-0.0, 0.0), (nan, 1.0), (1.0, nan), (-inf, inf)):
	wf(luces[3] + 0x48, (a,))
	wf(luces[4] + 0x48, (b,))
	check('Luz_Compare' + fmt((a, b)), lambda: SDTest.Call('Luz_Compare', 'iii', lcell, lcell + 4)[0])

# Luz_Select over all lights of the list (the menu's own too): which of ours, in which order, their peso and enable
lout = lalloc(64)
def lidx(p):
	for i in range(NL):
		if luces[i] == p:
			return i
	return -1
def lsel(pos, sector, center, radius, maxOn):
	r = SDTest.Call('Luz_Select', 'iFiFfii', pos, sector, center, radius, lout, maxOn)
	n = r[0]
	order = []
	for i in range(n):
		order.append(lidx(SDTest.Peek(lout + 4 * i, '@i')))
	st = []
	for l in luces:
		st.append((SDTest.Peek(l + 0x48, '@i'), SDTest.Peek(l + 0xb8, '@i') & 0xffffff))
	return n, order, st, lcount()
def lsetup():
	lset(luces[0], 5000, (0.0, 0.0, 0.0), (0.0, -1.0, 0.0), 0x00ffffff, 1.0, 50.0)
	lset(luces[1], 5001, (0.0, 10.0, 0.0), (0.0, -1.0, 0.0), 0x00ff8040, 2.0, 100.0, 30.0, 40.0)
	lset(luces[2], 5002, (0.0, 0.0, 0.0), (0.3, -1.0, 0.2), 0x00808080, 0.5, 0.0)
	lset(luces[3], 5000, (40.0, 0.0, 0.0), (0.0, -1.0, 0.0), 0x00ffffff, 3.0, 5.0)
	lset(luces[4], 5000, (5.0, 5.0, 5.0), (0.0, -1.0, 0.0), 0x00ffffff, 1.5, 0.0, 43.0, 45.0, 1.0, 3)
	lset(luces[5], 5001, (0.0, -10.0, 0.0), (0.0, -1.0, 0.0), 0x00ffffff, 1.0, 0.0, 10.0, 20.0)
	# FillD3D and the device light, not Update: with a range Update tests the sphere against the camera frustum
	for l in luces:
		SDTest.Call('CLuz_FillD3D', 'tvii', l, l + 0x4c)
		SDTest.Call('CLuz_UpdateD3D', 'tvii', l, 0)
lsetup()
for i in range(10):
	p = vec()
	c = (p[0] + rnd(-1.0, 1.0), p[1] + rnd(-1.0, 1.0), p[2] + rnd(-1.0, 1.0))
	for sector, maxOn in ((-1, 8), (3, 2), (2, 0), (3, 16)):
		check('Luz_Select' + fmt((i, sector, maxOn)), lambda p=p, c=c, sector=sector, maxOn=maxOn: lsel(p, sector, c, rnd(0.5, 5.0), maxOn))
gf(sym('g_LightSortI'), -1.0)
check('Luz_Select sort -1', lambda: lsel((1.0, 2.0, 3.0), -1, (1.0, 2.0, 3.0), 2.0, 4))
gf(sym('g_LightSortI'), 0.0)
check('Luz_Select sort 0', lambda: lsel((1.0, 2.0, 3.0), -1, (1.0, 2.0, 3.0), 2.0, 4))
gf(sym('g_LightSortI'), nan)
check('Luz_Select sort nan', lambda: lsel((1.0, 2.0, 3.0), -1, (1.0, 2.0, 3.0), 2.0, 4))
lrestore()
SDTest.Poke('g_LightSort', 2)
check('Luz_Select R_LightSort 2', lambda: lsel((1.0, 2.0, 3.0), -1, (1.0, 2.0, 3.0), 2.0, 4))
lrestore()
SDTest.Poke('g_SpotlightAng', 0)
check('Luz_Select no spot angle', lambda: lsel((0.0, 0.0, 0.0), -1, (0.0, 0.0, 0.0), 1.0, 16))
lrestore()
SDTest.Poke('g_LightEnaD3D', 0)
check('Luz_Select ena 0', lambda: lsel((0.0, 0.0, 0.0), -1, (0.0, 0.0, 0.0), 1.0, 3))
lrestore()
SDTest.Poke('g_LightSector', 0)
check('Luz_Select no sectors', lambda: lsel((0.0, 0.0, 0.0), 2, (0.0, 0.0, 0.0), 1.0, 3))
lrestore()
lstep('CLuz_Hide 3', luces[3], sym('CLuz_Hide'), 'tvii', 0)
check('Luz_Select hidden', lambda: lsel((40.0, 0.0, 0.0), -1, (40.0, 0.0, 0.0), 1.0, 16))
SDTest.Call('CLuz_UpdateD3D', 'tvii', luces[3], 0)
SDTest.Call('E3D_SetShadowLight', 'vii', 0, 0)

# the LUZ blocks of two model files of the packs (activa 1 and 0)
lio = lalloc(16)
lctx = lalloc(8)
wi(lctx + 4, lnodes[1])
def lread(l, name, skip):
	SDTest.Call('CArchIO_ctor', 'tiiss', lio, name, 'rb')
	while skip > 0:
		k = min(skip, 1000)
		SDTest.Call('CArchIO_Read', 'tiibi', lio, '', k)
		skip = skip - k
	r = SDTest.Call('CLuz_Read', 'tbiii', l, lio, lctx)[0]
	SDTest.Call('CArchIO_Close', 'tii', lio)
	SDTest.Call('CArchIO_dtor', 'tvi', lio)
	return r, lget(l), lnodo(l), lrefs(), lcount()
lrd = lalloc(0xcc)
SDTest.Call('CLuz_ctor', 'tii', lrd)
check('CLuz_Read ab', lambda: lread(lrd, 'levels/ab/map/map3d.sm3', 1483))
lrd2 = lalloc(0xcc)
SDTest.Call('CLuz_ctor', 'tii', lrd2)
check('CLuz_Read abindoor', lambda: lread(lrd2, 'levels/abindoor/map/map3d.sm3', 1458))
lio2 = lalloc(8)
check('IObjeto3D_Read', lambda: SDTest.Call('IObjeto3D_Read', 'tbiii', lio2, 0, 0)[0])
check('IObjeto3D_Write', lambda: SDTest.Call('IObjeto3D_Write', 'tbiii', lio2, 0, 0)[0])
check('IObjeto3D_dtor', lambda: (SDTest.Call('IObjeto3D_dtor', 'tvi', lio2), SDTest.Peek(lio2, '@i') - 0x400000))
check('IObjeto3D_Delete', lambda: (SDTest.Call('IObjeto3D_Delete', 'tiii', lio2, 0)[0] == lio2, SDTest.Peek(lio2, '@i') - 0x400000))
lbox = lalloc(24)
wf(lbox, (1.0, 2.0, 3.0, 4.0, 5.0, 6.0))
check('Box_Zero', lambda: (SDTest.Call('Box_Zero', 'tii', lbox)[0] == lbox, tpeek(lbox, 6)))
check('Luz_SetDirLight', lambda: SDTest.Call('Luz_SetDirLight', 'viFF', 200, (0.5, 0.25, 1.0, 1.0), (0.0, -1.0, 0.0)))

# all destroyed: the list and the counts back as before
check('CLuz_dtor read', lambda: (SDTest.Call('CLuz_dtor', 'tvi', lrd), lget(lrd), lrefs(), lcount()))
check('CLuz_Delete read', lambda: (SDTest.Call('CLuz_Delete', 'tiii', lrd2, 0)[0] == lrd2, lget(lrd2), lrefs(), lcount()))
SDTest.Call('CLuz_SetNodo', 'tvii', l0, 0)
for i in range(NL):
	if i % 2:
		check('CLuz_dtor' + fmt((i,)), lambda l=luces[i]: (SDTest.Call('CLuz_dtor', 'tvi', l), SDTest.Peek(l, '@i') - 0x400000, lget(l), lcount()))
	else:
		check('CLuz_Delete' + fmt((i,)), lambda l=luces[i]: (SDTest.Call('CLuz_Delete', 'tiii', l, 0)[0] == l, lget(l), lcount()))
check('Luz refs after', lrefs)
SDTest.Call('E3D_SetShadowLight', 'vii', 0, 0)
lrestore()
for p in luces + lnodes + [ld3d, lname, lpat, lcell, lout, lio, lctx, lrd, lrd2, lio2, lbox]:
	lfree(p)
