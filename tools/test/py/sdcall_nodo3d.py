# engine3d Engine/Nodo3D.c (execfile'd at the end of sdcall.py, Python 1.5.2, its helpers in scope). Nodes in heap
# memory linked into a list; scenes, render nodes, looks, animation sets and most 3D objects are fake records (zeroed
# memory with the members the code reads). Pointers are printed as indices into the lists of the records, never as
# addresses. No failing CHECK: the escena, Refs and link conditions are kept. Not called: the sampling of real tracks
# (AnimTrack_* of animacion.cpp: the sets here have no track or are only looked up), CNodo3D_Render with an object or
# R_ShowLocalBox, CNodo3D_BuildD3D of a CMallaTri (a device mesh), CNodo3D_Update of a node with nodoR or NODO_FX
# (CNodoR code on a fake render node), the recalc of mesh boxes.
def nalloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def nfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def nwf(addr, floats):
	SDTest.Call('crt_memcpy', 'iiFi', addr, tuple(floats), 4 * len(floats))
def nwi(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def nri(addr):
	return SDTest.Peek(addr, '@i')
def nrf(addr, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, addr, 4 * n)[1]
def ngi(rva):
	return SDTest.Peek(rva, 'i')
def nidx(p, lst):
	if p == 0:
		return None
	for i in range(len(lst)):
		if lst[i] == p:
			return i
	return '?'
# all records a node can point to, by kind
nodes = []
objs = []
escenas = []
def nget(n):
	r = [nri(n), SDTest.Peek(n + 4, '@p'), nri(n + 8)]
	for off in (0xc, 0x10, 0x14):
		r.append(nidx(nri(n + off), nodes))
	r.append(nri(n + 0x18))
	r.append(nrf(n + 0x1c, 7))
	r.append(nidx(nri(n + 0x3c), objs))
	r.append(nidx(nri(n + 0x40), escenas))
	r.append(tpeek(n + 0x44, 4))
	r.append(nidx(nri(n + 0x58), objs))
	r.append(nrf(n + 0x5c, 16))
	r.append(nrf(n + 0x9c, 16))
	r.append(nrf(n + 0xdc, 7))
	r.append(nri(n + 0xf8) != 0)
	return tuple(r)
def nall():
	return tuple(map(nget, nodes))
def ncount():
	return ngi(sym('g_Nodo3DCount'))
def nnew(name):
	n = nalloc(0xfc)
	nodes.append(n)
	SDTest.Call('CNodo3D_ctor', 'tii', n)
	if name:
		SDTest.Call('Cad_Set', 'tvis', n + 4, name)
	return n
# arguments in check names: nodes by index, other heap pointers hidden
def nargs(args):
	r = []
	for a in args:
		if type(a) == type(0) and a in nodes:
			r.append('n' + str(nidx(a, nodes)))
		elif type(a) == type(0) and (a > 0x1000000 or a < -0x1000000):
			r.append('p')
		else:
			r.append(a)
	return tuple(r)
def nstep(name, rva, sig, n, *args):
	check(name + fmt(nargs(args)), lambda rva=rva, sig=sig, n=n, args=args: (apply(SDTest.Call, (rva, sig, n) + args)[0], nall()))
def nobj(clase, size):
	o = nalloc(size)
	nwi(o + 4, clase)
	objs.append(o)
	return o

ncount0 = ncount()
check('CNodo3D_ctor', lambda: (SDTest.Call('CNodo3D_ctor', 'tii', nalloc(0xfc))[0] != 0, ncount() - ncount0))
names = ('root', 'cam01', 'luz01', 'cam02', 'box')
for nm in names:
	nnew(nm)
check('CNodo3D_ctor nodes', lambda: (nall(), ncount() - ncount0))
esc = nalloc(0x20)
escenas.append(esc)
for n in nodes:
	nwi(n + 0x40, esc)
# the list root .. box
for i in range(len(nodes) - 1):
	nstep('CNodo3D_SetSig' + fmt((i,)), sym('CNodo3D_SetSig'), 'tvii', nodes[i], nodes[i + 1])
	nstep('CNodo3D_SetAnt' + fmt((i,)), sym('CNodo3D_SetAnt'), 'tvii', nodes[i + 1], nodes[i])
nstep('CNodo3D_SetSig same', sym('CNodo3D_SetSig'), 'tvii', nodes[0], nodes[1])
nstep('CNodo3D_SetSig NULL', sym('CNodo3D_SetSig'), 'tvii', nodes[3], 0)
nstep('CNodo3D_SetSig back', sym('CNodo3D_SetSig'), 'tvii', nodes[3], nodes[4])
nstep('CNodo3D_SetAnt NULL', sym('CNodo3D_SetAnt'), 'tvii', nodes[0], 0)
nstep('CNodo3D_SetPadre', sym('CNodo3D_SetPadre'), 'tvii', nodes[1], nodes[0])
nstep('CNodo3D_SetPadre 2', sym('CNodo3D_SetPadre'), 'tvii', nodes[2], nodes[0])
nstep('CNodo3D_SetPadre again', sym('CNodo3D_SetPadre'), 'tvii', nodes[2], nodes[0])
for nm in ('cam01', 'root', 'box', 'luz01', 'nope'):
	nstep('CNodo3D_SetPadreName' + fmt((nm,)), sym('CNodo3D_SetPadreName'), 'tvis', nodes[3], nm)
nstep('CNodo3D_SetPadreName NULL', sym('CNodo3D_SetPadreName'), 'tviz', nodes[4], None)
nstep('CNodo3D_SetPadreName cam02', sym('CNodo3D_SetPadreName'), 'tviz', nodes[4], 'cam02')
# objects: camera, light, camera, floor (a box at 0x30, no nodes), and the mesh classes with their box
ocam = nobj(0xa2000004, 0x10)
oluz = nobj(0xa3000008, 0x10)
ocam2 = nobj(0xa2000004, 0x10)
osuelo = nobj(0xa4000010, 0x48)
nwf(osuelo + 0x30, (-1.0, -2.0, -3.0, 4.0, 5.0, 6.5))
od3d = nobj(0xa1000002, 0x98)
nwf(od3d + 0x54, (0.5, -2.0, 1.0, 3.0, 7.0, 1.25))
otri = nobj(0xa1000001, 0x4c)
nwf(otri + 0x34, (-0.5, -0.25, -8.0, 0.5, 0.25, 8.0))
oother = nobj(0x00012345, 0x10)
nstep('CNodo3D_SetObj3D NULL', sym('CNodo3D_SetObj3D'), 'tvii', nodes[0], 0)
for i, o in ((1, ocam), (2, oluz), (3, ocam2), (4, osuelo)):
	nstep('CNodo3D_SetObj3D' + fmt((i,)), sym('CNodo3D_SetObj3D'), 'tvii', nodes[i], o)
def nfind(name, rva, sig, n, *args):
	check(name + fmt(nargs(args)), lambda rva=rva, sig=sig, n=n, args=args: nidx(apply(SDTest.Call, (rva, sig, n) + args)[0], nodes + objs))
for i in range(len(nodes)):
	for nm in (None, 'cam01', 'cam02', 'luz01', 'root', 'x'):
		nfind('CNodo3D_FindCamara' + fmt((i,)), sym('CNodo3D_FindCamara'), 'tiiz', nodes[i], nm)
		nfind('CNodo3D_FindLuz' + fmt((i,)), sym('CNodo3D_FindLuz'), 'tiiz', nodes[i], nm)
		nfind('CNodo3D_FindPrev' + fmt((i,)), sym('CNodo3D_FindPrev'), 'tiiz', nodes[i], nm)
		for incl in (0, 1):
			nfind('CNodo3D_Find' + fmt((i, incl)), sym('CNodo3D_Find'), 'tiizi', nodes[i], nm, incl)
	for p in (None, 'cam*', '*0?', 'l*', '*', 'r?o?', 'CAM01', '*x'):
		for incl in (0, 1):
			nfind('CNodo3D_FindWild' + fmt((i, incl)), sym('CNodo3D_FindWild'), 'tiizi', nodes[i], p, incl)
	check('CNodo3D_Clase' + fmt((i,)), lambda n=nodes[i]: SDTest.Call('CNodo3D_Clase', 'tii', n)[0])
# the object of a render node
rn = nalloc(0xa8)
nfind('CNodo3D_GetObj3D NULL', sym('CNodo3D_GetObj3D'), 'tiii', nodes[1], 0)
nfind('CNodo3D_GetObj3D r', sym('CNodo3D_GetObj3D'), 'tiii', nodes[1], rn)
nwi(rn + 0x9c, ocam2)
nfind('CNodo3D_GetObj3D r obj', sym('CNodo3D_GetObj3D'), 'tiii', nodes[1], rn)
nwi(rn + 0x9c, 0)
check('CSuelo_GetBox', lambda: SDTest.Call('CSuelo_GetBox', 'tiiF', osuelo, (0.0,) * 6)[1])
# the boxes of the classes with one, the matrix with and without mObj
bbox0 = (9.0, 9.0, 9.0, 10.0, 10.0, 10.0)
nwf(nodes[4] + 0x5c, affine())
nwf(nodes[4] + 0x9c, affine())
def nbox(name, n, *args):
	check(name, lambda n=n, args=args: apply(SDTest.Call, (sym('CNodo3D_GetBBox'), 'tbiFFi', n, (0.0,) * 16, bbox0) + args)[0:3])
	check(name + ' m NULL', lambda n=n, args=args: apply(SDTest.Call, (sym('CNodo3D_GetBBox'), 'tbiiFi', n, 0, bbox0) + args)[0:2])
nbox('CNodo3D_GetBBox suelo', nodes[4], 0)
nwi(nodes[4] + 0x3c, od3d)
nbox('CNodo3D_GetBBox mallad3d', nodes[4], 0)
nwi(od3d + 0x94, 1)
nbox('CNodo3D_GetBBox mallad3d skin', nodes[4], 0)
nwi(nodes[4] + 0x3c, otri)
nbox('CNodo3D_GetBBox mallatri', nodes[4], 0)
nwi(otri + 0x2c, 1)
nbox('CNodo3D_GetBBox mallatri skin', nodes[4], 0)
nbox('CNodo3D_GetBBox mallatri skin recalc 0', nodes[4], 0)
nwi(nodes[4] + 0x3c, oluz)
nbox('CNodo3D_GetBBox luz', nodes[4], 0)
nwi(nodes[4] + 0x3c, oother)
nbox('CNodo3D_GetBBox other', nodes[4], 0)
nwi(nodes[4] + 0x3c, 0)
nbox('CNodo3D_GetBBox no object', nodes[4], 0)
# BuildD3D only for the classes that do nothing on fake records: a floor without nodes and the others
nwi(nodes[4] + 0x3c, osuelo)
nstep('CNodo3D_BuildD3D suelo', sym('CNodo3D_BuildD3D'), 'tbii', nodes[4], 7)
nstep('CNodo3D_BuildD3D luz', sym('CNodo3D_BuildD3D'), 'tbii', nodes[2], 7)
nstep('CNodo3D_BuildD3D none', sym('CNodo3D_BuildD3D'), 'tbii', nodes[0], 7)

# CRotacion: the quaternion of the angles in both modes, cached until the angles change
def rotnew(ang, modo):
	r = nalloc(0x24)
	SDTest.Call('CRotacion_SetEuler', 'tviF', r, ang)
	nwi(r + 0x20, modo)
	return r
def rotget(r):
	return (nrf(r, 7), nri(r + 0x1c), nri(r + 0x20))
def rquat(r):
	return SDTest.Call('CRotacion_GetQuat', 'tiiF', r, (0.0,) * 4)[1]
rots = []
for i in range(16):
	a = vec()
	if i % 5 == 4:
		a = (0.0, 0.0, 0.0)
	if i % 7 == 6:
		a = (0.0, a[1], -0.0)
	rots.append(rotnew(a, i % 2))
for i in range(len(rots)):
	r = rots[i]
	check('CRotacion_GetQuat' + fmt((i,)), lambda r=r: (rquat(r), rotget(r)))
	check('CRotacion_GetQuat cached' + fmt((i,)), lambda r=r: (rquat(r), rotget(r)))
for i in range(12):
	r = rotnew((rnd(-100.0, 100.0), rnd(-1.0, 1.0), rnd(-1000.0, 1000.0)), 1)
	check('CRotacion_GetQuat big' + fmt((i,)), lambda r=r: (rquat(r), rotget(r)))
	nfree(r)
for i in range(len(rots)):
	a = rots[i]
	b = rots[(i * 7 + 3) % len(rots)]
	# fresh angles, so that the combination makes the quaternions itself
	SDTest.Call('CRotacion_SetEuler', 'tviF', a, nrf(a, 3))
	SDTest.Call('CRotacion_SetEuler', 'tviF', b, nrf(b, 3))
	q = uquat()
	check('Rot_Combine' + fmt((i,)), lambda a=a, b=b, q=q: (SDTest.Call('Rot_Combine', 'iFiFi', (0.0,) * 4, a, q, b)[1:], rotget(a), rotget(b)))
	SDTest.Call('CRotacion_SetEuler', 'tviF', a, nrf(a, 3))
	SDTest.Call('CRotacion_SetEuler', 'tviF', b, nrf(b, 3))
	check('Rot_Combine2' + fmt((i,)), lambda a=a, b=b: (SDTest.Call('Rot_Combine2', 'iFii', (0.0,) * 4, a, b)[1], rotget(a), rotget(b)))
	check('Rot_Combine2 same' + fmt((i,)), lambda a=a: (SDTest.Call('Rot_Combine2', 'iFii', (0.0,) * 4, a, a)[1], rotget(a)))

# animation channels: a set without tracks (nothing sampled) and one with a track that is only looked up
def nchain(set):
	a = nalloc(0x98)
	b = nalloc(0x40)
	nwi(a + 0x94, b)
	nwi(b + 0x3c, set)
	return a
setEmpty = nalloc(0x28)
track = nalloc(0x24)
nwi(track + 0x20, 0x1234)
tmap = nalloc(4)
nwi(tmap, 0x0201)
setOne = nalloc(0x28)
nwi(setOne + 0x18, 2)
nwi(setOne + 0x1c, tmap)
nwi(setOne + 0x20, track)
nwi(setOne + 0x24, 1)
animEmpty = nchain(setEmpty)
animOne = nchain(setOne)
actx = nalloc(0xa0)
nwf(actx + 0x60, affine())
pairs = []
for i in range(4):
	pairs.append(nalloc(8))
esc2 = nalloc(0x20)
escenas.append(esc2)
nwi(esc2 + 0x10, actx)
# a node of its own for the animation, without object
an = nnew('anim')
nwi(an + 0x40, esc2)
zero7 = (0.0,) * 7
def nsample(name, n):
	check(name, lambda n=n: SDTest.Call('CNodo3D_SampleAnim', 'tbiFF', n, (0.0,) * 3, (0.0,) * 4))
	check(name + ' color', lambda n=n: SDTest.Call('CNodo_LightColor', 'tbiFF', n, (0.0,) * 3, (0.0,)))
	check(name + ' float', lambda n=n: SDTest.Call('CNodo3D_AnimFloat', 'tbiF', n, (0.0,)))
	check(name + ' alpha', lambda n=n: SDTest.Call('CNodo3D_AnimAlpha', 'tbip', n, 0))
	for ch in range(4):
		check(name + ' Channel' + fmt((ch,)), lambda n=n, ch=ch: nidx(SDTest.Call('CNodo3D_Channel', 'tiii', n, ch)[0], (actx, actx + 0x18, actx + 0x30, actx + 0x48)))
		check(name + ' Track' + fmt((ch,)), lambda n=n, ch=ch: nidx(SDTest.Call('CNodo3D_Track', 'tiii', n, ch)[0], (track,)))
		check(name + ' Track2' + fmt((ch,)), lambda n=n, ch=ch: nidx(SDTest.Call('CNodo3D_Track2', 'tiii', n, ch)[0], (track,)))
		check(name + ' TrackData' + fmt((ch,)), lambda n=n, ch=ch: SDTest.Call('CNodo3D_TrackData', 'tiii', n, ch)[0])
nsample('CNodo3D_SampleAnim no channels', an)
nwi(an + 0x44, 0)
nsample('CNodo3D_SampleAnim indice 0 no channels', an)
nwi(actx, pairs[0])
nsample('CNodo3D_SampleAnim empty pair', an)
nwi(pairs[0], animEmpty)
nsample('CNodo3D_SampleAnim no track', an)
nwi(actx + 0x18 * 2, pairs[2])
nwi(pairs[2] + 4, animEmpty)
nsample('CNodo3D_SampleAnim second only', an)
nwi(pairs[2], animEmpty)
nwf(actx + 0x18 * 2 + 0x14, (0.5,))
nsample('CNodo3D_SampleAnim blend', an)
check('CNodo3D_AnimChannel', lambda: SDTest.Call('CNodo3D_AnimChannel', 'tbiiFF', an, actx + 0x30, (0.0,) * 3, (0.0,) * 4))
# lookups only: the track of indice 0 (map entry 1) and indice 1 (2: past the tracks), the sampling never reached
nwi(actx, 0)
nwi(actx + 0x30, 0)
nwi(actx + 0x18, pairs[1])
nwi(pairs[1], animOne)
for ind in (0, 1, 2, 7):
	nwi(an + 0x44, ind)
	for ch in range(4):
		check('CNodo3D_Track one' + fmt((ind, ch)), lambda ch=ch: nidx(SDTest.Call('CNodo3D_Track', 'tiii', an, ch)[0], (track,)))
		check('CNodo3D_Track2 one' + fmt((ind, ch)), lambda ch=ch: nidx(SDTest.Call('CNodo3D_Track2', 'tiii', an, ch)[0], (track,)))
		check('CNodo3D_TrackData one' + fmt((ind, ch)), lambda ch=ch: SDTest.Call('CNodo3D_TrackData', 'tiii', an, ch)[0])
	nwi(pairs[1] + 4, animOne)
	check('CNodo3D_Track2 one second' + fmt((ind,)), lambda: nidx(SDTest.Call('CNodo3D_Track2', 'tiii', an, 1)[0], (track,)))
	nwi(pairs[1] + 4, 0)
nwi(an + 0x44, -1)
nwi(actx + 0x18, 0)
saved = ngi(sym('g_SkipAnim'))
SDTest.Poke('g_SkipAnim', 1)
nwi(actx, pairs[0])
nsample('CNodo3D_SampleAnim R_SkipAnim', an)
SDTest.Poke('g_SkipAnim', saved)
nwi(actx, 0)

# Update: without and with render node, the looks 1..5, the parent from the node or the scene
svars = (sym('g_SkipTrans2'), sym('g_SkipTrans'), sym('g_SkipAnim'))
ssaved = map(ngi, svars)
def nrestore():
	for i in range(len(svars)):
		SDTest.Poke(svars[i], ssaved[i])
SDTest.Poke('g_SkipTrans2', 0)
SDTest.Poke('g_SkipTrans', 0)
SDTest.Poke('g_SkipAnim', 0)
u0 = nnew('u0')
u1 = nnew('u1')
u2 = nnew('u2')
for u in (u0, u1, u2):
	nwi(u + 0x40, esc2)
	nwf(u + 0x1c, vec() + uquat())
	nwf(u + 0x9c, affine())
	nwf(u + 0xdc, uquat())
SDTest.Call('CNodo3D_SetPadre', 'tvii', u1, u0)
SDTest.Call('CNodo3D_SetPadre', 'tvii', u2, u1)
owner = nalloc(0x20)
nwf(owner + 0x14, vec())
rr = nalloc(0xa8)
nwi(rr + 8, owner)
def rrget():
	return (nrf(rr + 0x10, 32), nri(rr + 0x90) == ngi(sym('g_FrameCount')))
def looknew(tipo, a1, m1, a2, m2):
	l = nalloc(0x4c)
	nwi(l, tipo)
	SDTest.Call('CRotacion_SetEuler', 'tviF', l + 4, a1)
	nwi(l + 0x24, m1)
	SDTest.Call('CRotacion_SetEuler', 'tviF', l + 0x28, a2)
	nwi(l + 0x4c - 4, m2)
	return l
def lookget(l):
	return (rotget(l + 4), rotget(l + 0x28))
def nupd(name, u, r):
	check(name, lambda u=u, r=r: (SDTest.Call('CNodo3D_Update', 'tvii', u, r), nget(u), rrget()))
for u in (u0, u1, u2):
	nupd('CNodo3D_Update NULL' + fmt((nidx(u, nodes),)), u, 0)
nwi(esc2 + 0x10, 0)
nupd('CNodo3D_Update no scene matrix', u0, 0)
nwi(esc2 + 0x10, actx)
for u in (u0, u1, u2):
	nupd('CNodo3D_Update r' + fmt((nidx(u, nodes),)), u, rr)
z3 = (0.0, 0.0, 0.0)
for tipo in (0, 1, 2, 3, 4, 5, 6):
	for k in range(4):
		a1, a2 = vec(), vec()
		if k == 1:
			a1 = z3
		if k == 2:
			a2 = z3
		if k == 3:
			a1, a2 = z3, z3
		l = looknew(tipo, a1, k % 2, a2, (k / 2) % 2)
		nwi(rr + 0xc, l)
		for u in (u0, u2):
			check('CNodo3D_Update look' + fmt((tipo, k, nidx(u, nodes))), lambda u=u, l=l: (SDTest.Call('CNodo3D_Update', 'tvii', u, rr), nget(u), rrget(), lookget(l)))
		nwi(rr + 0xc, 0)
		nfree(l)
SDTest.Poke('g_SkipTrans', 1)
nwf(rr + 0x10, affine())
nupd('CNodo3D_Update R_SkipTrans', u1, rr)
nupd('CNodo3D_Update R_SkipTrans NULL', u1, 0)
SDTest.Poke('g_SkipTrans', 0)
nwi(u1 + 0x18, 0x20000)
nupd('CNodo3D_Update NODO_NOUPDATE', u1, rr)
nwi(u1 + 0x18, 0)
nrestore()

# Render: the filters (which pass, the class and flag masks) and the path without object up to R_SetWorld (the
# pending world matrix of the renderer, put back afterwards)
rvars = (sym('g_RenderPass'), sym('g_RenderClaseMask'), sym('g_RenderFlagMask'), sym('g_RenderFlagNeg'), sym('g_SkipNodes'), sym('g_ShowLocalBox'), sym('g_WorldSet'))
rsaved = map(ngi, rvars)
wsaved = []
for i in range(16):
	wsaved.append(ngi(sym('g_DeviceStr') + 0x180 + 4 * i))
def rrestore():
	for i in range(len(rvars)):
		SDTest.Poke(rvars[i], rsaved[i])
	for i in range(16):
		SDTest.Poke(sym('g_DeviceStr') + 0x180 + 4 * i, wsaved[i])
SDTest.Poke('g_ShowLocalBox', 0)
def rset(pass_, cmask, fmask, neg):
	SDTest.Poke('g_RenderPass', pass_)
	SDTest.Poke('g_RenderClaseMask', cmask)
	SDTest.Poke('g_RenderFlagMask', fmask)
	SDTest.Poke('g_RenderFlagNeg', (rsaved[3] & ~0xff) | neg)
def nworld():
	r = []
	for i in range(16):
		r.append(ngi(sym('g_DeviceStr') + 0x180 + 4 * i))
	return r
def nrender(name, n):
	c0 = ngi(sym('g_NodesRendered'))
	for i in range(16):
		SDTest.Poke(sym('g_DeviceStr') + 0x180 + 4 * i, 0)
	SDTest.Call('CNodo3D_Render', 'tviii', n, 5, 0)
	check(name, lambda c0=c0, n=n: (ngi(sym('g_NodesRendered')) - c0, nworld(), nri(n + 0x5c)))
nwf(u0 + 0x5c, affine())
rset(0, 0, 0, 0)
nrender('CNodo3D_Render', u0)
SDTest.Poke('g_SkipNodes', 1)
nrender('CNodo3D_Render R_SkipNodes', u0)
SDTest.Poke('g_SkipNodes', 0)
for fl in (0x80, 0x08, 0x40000000, 0x40, 0x1, 0x40000041):
	nwi(u0 + 0x18, fl)
	for pass_ in (0, 3, 13, 100, 7):
		for cmask, fmask, neg in ((0, 0, 0), (0, 1, 1), (0, 1, 0), (0, 0, 1), (0xff, 0, 0), (0, 0x40, 2)):
			rset(pass_, cmask, fmask, neg)
			nrender('CNodo3D_Render' + fmt((fl, pass_, cmask, fmask, neg)), u0)
nwi(u0 + 0x18, 0)
# a node with an object: only the filters that end before the object is drawn
for pass_, cmask, fl in ((0, 0x4, 0), (0, 0x2000000, 0), (100, 0, 0), (3, 0, 0), (0, 0, 0x88)):
	rset(pass_, cmask, 1, 1)
	nwi(nodes[1] + 0x18, fl)
	nrender('CNodo3D_Render object' + fmt((pass_, cmask, fl)), nodes[1])
nwi(nodes[1] + 0x18, 0)
rrestore()

# FreeObj3D and Free with real objects: cameras (operator new, CCamara_ctor) and an ini
def ncam():
	c = SDTest.Call('operator_new', 'ii', 0x78)[0]
	SDTest.Call('CCamara_ctor', 'tii', c)
	return c
fn = nnew('free')
SDTest.Call('CNodo3D_SetObj3D', 'tvii', fn, ncam())
check('CNodo3D_FreeObj3D', lambda: (SDTest.Call('CNodo3D_FreeObj3D', 'tvi', fn), nget(fn)))
check('CNodo3D_FreeObj3D none', lambda: (SDTest.Call('CNodo3D_FreeObj3D', 'tvi', fn), nget(fn)))
nwi(fn + 0x40, esc)
SDTest.Call('CNodo3D_SetPadre', 'tvii', fn, nodes[0])
SDTest.Call('CNodo3D_SetObj3D', 'tvii', fn, ncam())
nwi(fn + 0x58, ncam())
ini = SDTest.Call('operator_new', 'ii', 4)[0]
SDTest.Call('CArchINI_ctor', 'tii', ini)
nwi(fn + 0xf8, ini)
check('CNodo3D_Free', lambda: (SDTest.Call('CNodo3D_Free', 'tvi', fn), nget(fn), nget(nodes[0])))
check('CNodo3D_Free again', lambda: (SDTest.Call('CNodo3D_Free', 'tvi', fn), nget(fn)))

# out of the list: the middle, the head, the end
for n in nodes:
	nwi(n + 0x40, 0)
for i in (2, 0, 4, 1, 3):
	nstep('CNodo3D_Unlink' + fmt((i,)), sym('CNodo3D_Unlink'), 'tvi', nodes[i])
for n in nodes:
	SDTest.Call('CNodo3D_SetPadre', 'tvii', n, 0)
check('CNodo3D after unlink', nall)
# the destructor and the delete; only nodes without links and references (a failing CHECK would end the game)
for n in nodes:
	nwi(n + 0x3c, 0)
c0 = ncount()
for i in range(len(nodes)):
	n = nodes[i]
	if nri(n + 8) == 0 and nri(n + 0xc) == 0 and nri(n + 0x10) == 0:
		check('CNodo3D_Delete 0' + fmt((i,)), lambda n=n: (SDTest.Call('CNodo3D_Delete', 'tiii', n, 0)[0] == n, nri(n), nri(n + 4) == 0, ncount() - c0))
	else:
		check('CNodo3D_Delete skipped' + fmt((i,)), lambda n=n: tpeek(n + 8, 3))
dn = SDTest.Call('operator_new', 'ii', 0xfc)[0]
SDTest.Call('CNodo3D_ctor', 'tii', dn)
SDTest.Call('Cad_Set', 'tvis', dn + 4, 'deleted')
check('CNodo3D_Delete 1', lambda: (SDTest.Call('CNodo3D_Delete', 'tiii', dn, 1)[0] == dn, ncount() - c0))
dn = SDTest.Call('operator_new', 'ii', 0xfc)[0]
SDTest.Call('CNodo3D_ctor', 'tii', dn)
check('CNodo3D_dtor', lambda: (SDTest.Call('CNodo3D_dtor', 'tvi', dn), nri(dn), ncount() - c0))
SDTest.Call('operator_delete_sized', 'vii', dn, 0xfc)
