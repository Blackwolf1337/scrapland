# engine3d Engine/Nodo3D.c (execfile'd at the end of sdcall.py, Python 1.5.2, its helpers in scope). Nodes in heap
# memory linked into a list; scenes, render nodes, looks, animation sets and most 3D objects are fake records (zeroed
# memory with the members the code reads). Pointers are printed as indices into the lists of the records, never as
# addresses. No failing CHECK: the escena, Refs and link conditions are kept. Not called: the sampling of real tracks
# (AnimTrack_* of animacion.cpp: the sets here have no track or are only looked up), CNodo3D_Render with an object or
# R_ShowLocalBox, CNodo3D_BuildD3D of a CMallaTri (a device mesh), CNodo3D_Update of a node with nodoR or NODO_FX
# (CNodoR code on a fake render node), the recalc of mesh boxes.
def nalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def nfree(p):
	SDTest.Call(0x133550, 'vi', p)
def nwf(addr, floats):
	SDTest.Call(0x3197d0, 'iiFi', addr, tuple(floats), 4 * len(floats))
def nwi(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def nri(addr):
	return SDTest.Peek(addr, '@i')
def nrf(addr, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, addr, 4 * n)[1]
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
	return ngi(0x4ad548)
def nnew(name):
	n = nalloc(0xfc)
	nodes.append(n)
	SDTest.Call(0x23cf40, 'tii', n)
	if name:
		SDTest.Call(0x2a1940, 'tvis', n + 4, name)
	return n
def nstep(name, rva, sig, n, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, n=n, args=args: (apply(SDTest.Call, (rva, sig, n) + args)[0], nall()))
def nobj(clase, size):
	o = nalloc(size)
	nwi(o + 4, clase)
	objs.append(o)
	return o

ncount0 = ncount()
check('CNodo3D_ctor', lambda: (SDTest.Call(0x23cf40, 'tii', nalloc(0xfc))[0] != 0, ncount() - ncount0))
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
	nstep('CNodo3D_SetSig' + fmt((i,)), 0x23eb80, 'tvii', nodes[i], nodes[i + 1])
	nstep('CNodo3D_SetAnt' + fmt((i,)), 0x23ea30, 'tvii', nodes[i + 1], nodes[i])
nstep('CNodo3D_SetSig same', 0x23eb80, 'tvii', nodes[0], nodes[1])
nstep('CNodo3D_SetSig NULL', 0x23eb80, 'tvii', nodes[3], 0)
nstep('CNodo3D_SetSig back', 0x23eb80, 'tvii', nodes[3], nodes[4])
nstep('CNodo3D_SetAnt NULL', 0x23ea30, 'tvii', nodes[0], 0)
nstep('CNodo3D_SetPadre', 0x23eac0, 'tvii', nodes[1], nodes[0])
nstep('CNodo3D_SetPadre 2', 0x23eac0, 'tvii', nodes[2], nodes[0])
nstep('CNodo3D_SetPadre again', 0x23eac0, 'tvii', nodes[2], nodes[0])
for nm in ('cam01', 'root', 'box', 'luz01', 'nope'):
	nstep('CNodo3D_SetPadreName' + fmt((nm,)), 0x23eb20, 'tvis', nodes[3], nm)
nstep('CNodo3D_SetPadreName NULL', 0x23eb20, 'tviz', nodes[4], None)
nstep('CNodo3D_SetPadreName cam02', 0x23eb20, 'tviz', nodes[4], 'cam02')
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
nstep('CNodo3D_SetObj3D NULL', 0x23ea60, 'tvii', nodes[0], 0)
for i, o in ((1, ocam), (2, oluz), (3, ocam2), (4, osuelo)):
	nstep('CNodo3D_SetObj3D' + fmt((i,)), 0x23ea60, 'tvii', nodes[i], o)
def nfind(name, rva, sig, n, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, n=n, args=args: nidx(apply(SDTest.Call, (rva, sig, n) + args)[0], nodes + objs))
for i in range(len(nodes)):
	for nm in (None, 'cam01', 'cam02', 'luz01', 'root', 'x'):
		nfind('CNodo3D_FindCamara' + fmt((i,)), 0x23e590, 'tiiz', nodes[i], nm)
		nfind('CNodo3D_FindLuz' + fmt((i,)), 0x23e630, 'tiiz', nodes[i], nm)
		nfind('CNodo3D_FindPrev' + fmt((i,)), 0x23e740, 'tiiz', nodes[i], nm)
		for incl in (0, 1):
			nfind('CNodo3D_Find' + fmt((i, incl)), 0x23e6d0, 'tiizi', nodes[i], nm, incl)
	for p in (None, 'cam*', '*0?', 'l*', '*', 'r?o?', 'CAM01', '*x'):
		for incl in (0, 1):
			nfind('CNodo3D_FindWild' + fmt((i, incl)), 0x23e7b0, 'tiizi', nodes[i], p, incl)
	check('CNodo3D_Clase' + fmt((i,)), lambda n=nodes[i]: SDTest.Call(0x23e800, 'ti', n)[0])
# the object of a render node
rn = nalloc(0xa8)
nfind('CNodo3D_GetObj3D NULL', 0x23d150, 'tiii', nodes[1], 0)
nfind('CNodo3D_GetObj3D r', 0x23d150, 'tiii', nodes[1], rn)
nwi(rn + 0x9c, ocam2)
nfind('CNodo3D_GetObj3D r obj', 0x23d150, 'tiii', nodes[1], rn)
nwi(rn + 0x9c, 0)
check('CSuelo_GetBox', lambda: SDTest.Call(0x23f0b0, 'tiiF', osuelo, (0.0,) * 6)[1])
# the boxes of the classes with one, the matrix with and without mObj
bbox0 = (9.0, 9.0, 9.0, 10.0, 10.0, 10.0)
nwf(nodes[4] + 0x5c, affine())
nwf(nodes[4] + 0x9c, affine())
def nbox(name, n, *args):
	check(name, lambda n=n, args=args: apply(SDTest.Call, (0x23d1a0, 'tbiFFi', n, (0.0,) * 16, bbox0) + args)[0:3])
	check(name + ' m NULL', lambda n=n, args=args: apply(SDTest.Call, (0x23d1a0, 'tbiiFi', n, 0, bbox0) + args)[0:2])
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
nstep('CNodo3D_BuildD3D suelo', 0x23e810, 'tbii', nodes[4], 7)
nstep('CNodo3D_BuildD3D luz', 0x23e810, 'tbii', nodes[2], 7)
nstep('CNodo3D_BuildD3D none', 0x23e810, 'tbii', nodes[0], 7)

# CRotacion: the quaternion of the angles in both modes, cached until the angles change
def rotnew(ang, modo):
	r = nalloc(0x24)
	SDTest.Call(0x212510, 'tviF', r, ang)
	nwi(r + 0x20, modo)
	return r
def rotget(r):
	return (nrf(r, 7), nri(r + 0x1c), nri(r + 0x20))
def rquat(r):
	return SDTest.Call(0x23f120, 'tiiF', r, (0.0,) * 4)[1]
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
	SDTest.Call(0x212510, 'tviF', a, nrf(a, 3))
	SDTest.Call(0x212510, 'tviF', b, nrf(b, 3))
	q = uquat()
	check('Rot_Combine' + fmt((i,)), lambda a=a, b=b, q=q: (SDTest.Call(0x23ec20, 'iFiFi', (0.0,) * 4, a, q, b)[1:], rotget(a), rotget(b)))
	SDTest.Call(0x212510, 'tviF', a, nrf(a, 3))
	SDTest.Call(0x212510, 'tviF', b, nrf(b, 3))
	check('Rot_Combine2' + fmt((i,)), lambda a=a, b=b: (SDTest.Call(0x23ecd0, 'iFii', (0.0,) * 4, a, b)[1], rotget(a), rotget(b)))
	check('Rot_Combine2 same' + fmt((i,)), lambda a=a: (SDTest.Call(0x23ecd0, 'iFii', (0.0,) * 4, a, a)[1], rotget(a)))

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
	check(name, lambda n=n: SDTest.Call(0x23ef80, 'tbiFF', n, (0.0,) * 3, (0.0,) * 4))
	check(name + ' color', lambda n=n: SDTest.Call(0x23ee50, 'tbiFF', n, (0.0,) * 3, (0.0,)))
	check(name + ' float', lambda n=n: SDTest.Call(0x23ef20, 'tbiF', n, (0.0,)))
	check(name + ' alpha', lambda n=n: SDTest.Call(0x23f050, 'tbip', n, 0))
	for ch in range(4):
		check(name + ' Channel' + fmt((ch,)), lambda n=n, ch=ch: nidx(SDTest.Call(0x23f0d0, 'tiii', n, ch)[0], (actx, actx + 0x18, actx + 0x30, actx + 0x48)))
		check(name + ' Track' + fmt((ch,)), lambda n=n, ch=ch: nidx(SDTest.Call(0x23ed70, 'tiii', n, ch)[0], (track,)))
		check(name + ' Track2' + fmt((ch,)), lambda n=n, ch=ch: nidx(SDTest.Call(0x23ede0, 'tiii', n, ch)[0], (track,)))
		check(name + ' TrackData' + fmt((ch,)), lambda n=n, ch=ch: SDTest.Call(0x23eeb0, 'tiii', n, ch)[0])
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
check('CNodo3D_AnimChannel', lambda: SDTest.Call(0x23d4d0, 'tbiiFF', an, actx + 0x30, (0.0,) * 3, (0.0,) * 4))
# lookups only: the track of indice 0 (map entry 1) and indice 1 (2: past the tracks), the sampling never reached
nwi(actx, 0)
nwi(actx + 0x30, 0)
nwi(actx + 0x18, pairs[1])
nwi(pairs[1], animOne)
for ind in (0, 1, 2, 7):
	nwi(an + 0x44, ind)
	for ch in range(4):
		check('CNodo3D_Track one' + fmt((ind, ch)), lambda ch=ch: nidx(SDTest.Call(0x23ed70, 'tiii', an, ch)[0], (track,)))
		check('CNodo3D_Track2 one' + fmt((ind, ch)), lambda ch=ch: nidx(SDTest.Call(0x23ede0, 'tiii', an, ch)[0], (track,)))
		check('CNodo3D_TrackData one' + fmt((ind, ch)), lambda ch=ch: SDTest.Call(0x23eeb0, 'tiii', an, ch)[0])
	nwi(pairs[1] + 4, animOne)
	check('CNodo3D_Track2 one second' + fmt((ind,)), lambda: nidx(SDTest.Call(0x23ede0, 'tiii', an, 1)[0], (track,)))
	nwi(pairs[1] + 4, 0)
nwi(an + 0x44, -1)
nwi(actx + 0x18, 0)
saved = ngi(0x4ad500)
SDTest.Poke(0x4ad500, 1)
nwi(actx, pairs[0])
nsample('CNodo3D_SampleAnim R_SkipAnim', an)
SDTest.Poke(0x4ad500, saved)
nwi(actx, 0)

# Update: without and with render node, the looks 1..5, the parent from the node or the scene
svars = (0x4ad5f4, 0x4ad5ac, 0x4ad500)
ssaved = map(ngi, svars)
def nrestore():
	for i in range(len(svars)):
		SDTest.Poke(svars[i], ssaved[i])
SDTest.Poke(0x4ad5f4, 0)
SDTest.Poke(0x4ad5ac, 0)
SDTest.Poke(0x4ad500, 0)
u0 = nnew('u0')
u1 = nnew('u1')
u2 = nnew('u2')
for u in (u0, u1, u2):
	nwi(u + 0x40, esc2)
	nwf(u + 0x1c, vec() + uquat())
	nwf(u + 0x9c, affine())
	nwf(u + 0xdc, uquat())
SDTest.Call(0x23eac0, 'tvii', u1, u0)
SDTest.Call(0x23eac0, 'tvii', u2, u1)
owner = nalloc(0x20)
nwf(owner + 0x14, vec())
rr = nalloc(0xa8)
nwi(rr + 8, owner)
def rrget():
	return (nrf(rr + 0x10, 32), nri(rr + 0x90) == ngi(0x4a8ec8))
def looknew(tipo, a1, m1, a2, m2):
	l = nalloc(0x4c)
	nwi(l, tipo)
	SDTest.Call(0x212510, 'tviF', l + 4, a1)
	nwi(l + 0x24, m1)
	SDTest.Call(0x212510, 'tviF', l + 0x28, a2)
	nwi(l + 0x4c - 4, m2)
	return l
def lookget(l):
	return (rotget(l + 4), rotget(l + 0x28))
def nupd(name, u, r):
	check(name, lambda u=u, r=r: (SDTest.Call(0x23dfe0, 'tvii', u, r), nget(u), rrget()))
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
			check('CNodo3D_Update look' + fmt((tipo, k, nidx(u, nodes))), lambda u=u, l=l: (SDTest.Call(0x23dfe0, 'tvii', u, rr), nget(u), rrget(), lookget(l)))
		nwi(rr + 0xc, 0)
		nfree(l)
SDTest.Poke(0x4ad5ac, 1)
nwf(rr + 0x10, affine())
nupd('CNodo3D_Update R_SkipTrans', u1, rr)
nupd('CNodo3D_Update R_SkipTrans NULL', u1, 0)
SDTest.Poke(0x4ad5ac, 0)
nwi(u1 + 0x18, 0x20000)
nupd('CNodo3D_Update NODO_NOUPDATE', u1, rr)
nwi(u1 + 0x18, 0)
nrestore()

# Render: the filters (which pass, the class and flag masks) and the path without object up to R_SetWorld (the
# pending world matrix of the renderer, put back afterwards)
rvars = (0x4afdb8, 0x4ad568, 0x4ad5fc, 0x4ad4ec, 0x4ad594, 0x4ad530, 0x432410)
rsaved = map(ngi, rvars)
wsaved = []
for i in range(16):
	wsaved.append(ngi(0x4ae478 + 4 * i))
def rrestore():
	for i in range(len(rvars)):
		SDTest.Poke(rvars[i], rsaved[i])
	for i in range(16):
		SDTest.Poke(0x4ae478 + 4 * i, wsaved[i])
SDTest.Poke(0x4ad530, 0)
def rset(pass_, cmask, fmask, neg):
	SDTest.Poke(0x4afdb8, pass_)
	SDTest.Poke(0x4ad568, cmask)
	SDTest.Poke(0x4ad5fc, fmask)
	SDTest.Poke(0x4ad4ec, (rsaved[3] & ~0xff) | neg)
def nworld():
	r = []
	for i in range(16):
		r.append(ngi(0x4ae478 + 4 * i))
	return r
def nrender(name, n):
	c0 = ngi(0x4a8f10)
	for i in range(16):
		SDTest.Poke(0x4ae478 + 4 * i, 0)
	SDTest.Call(0x23d800, 'tviii', n, 5, 0)
	check(name, lambda c0=c0, n=n: (ngi(0x4a8f10) - c0, nworld(), nri(n + 0x5c)))
nwf(u0 + 0x5c, affine())
rset(0, 0, 0, 0)
nrender('CNodo3D_Render', u0)
SDTest.Poke(0x4ad594, 1)
nrender('CNodo3D_Render R_SkipNodes', u0)
SDTest.Poke(0x4ad594, 0)
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
	c = SDTest.Call(0x1334d0, 'ii', 0x78)[0]
	SDTest.Call(0x29c110, 'tii', c)
	return c
fn = nnew('free')
SDTest.Call(0x23ea60, 'tvii', fn, ncam())
check('CNodo3D_FreeObj3D', lambda: (SDTest.Call(0x23ea10, 'tvi', fn), nget(fn)))
check('CNodo3D_FreeObj3D none', lambda: (SDTest.Call(0x23ea10, 'tvi', fn), nget(fn)))
nwi(fn + 0x40, esc)
SDTest.Call(0x23eac0, 'tvii', fn, nodes[0])
SDTest.Call(0x23ea60, 'tvii', fn, ncam())
nwi(fn + 0x58, ncam())
ini = SDTest.Call(0x1334d0, 'ii', 4)[0]
SDTest.Call(0x261980, 'tii', ini)
nwi(fn + 0xf8, ini)
check('CNodo3D_Free', lambda: (SDTest.Call(0x23ebb0, 'tvi', fn), nget(fn), nget(nodes[0])))
check('CNodo3D_Free again', lambda: (SDTest.Call(0x23ebb0, 'tvi', fn), nget(fn)))

# out of the list: the middle, the head, the end
for n in nodes:
	nwi(n + 0x40, 0)
for i in (2, 0, 4, 1, 3):
	nstep('CNodo3D_Unlink' + fmt((i,)), 0x23e910, 'tvi', nodes[i])
for n in nodes:
	SDTest.Call(0x23eac0, 'tvii', n, 0)
check('CNodo3D after unlink', nall)
# the destructor and the delete; only nodes without links and references (a failing CHECK would end the game)
for n in nodes:
	nwi(n + 0x3c, 0)
c0 = ncount()
for i in range(len(nodes)):
	n = nodes[i]
	if nri(n + 8) == 0 and nri(n + 0xc) == 0 and nri(n + 0x10) == 0:
		check('CNodo3D_Delete 0' + fmt((i,)), lambda n=n: (SDTest.Call(0x23d120, 'tiii', n, 0)[0] == n, nri(n), nri(n + 4) == 0, ncount() - c0))
	else:
		check('CNodo3D_Delete skipped' + fmt((i,)), lambda n=n: tpeek(n + 8, 3))
dn = SDTest.Call(0x1334d0, 'ii', 0xfc)[0]
SDTest.Call(0x23cf40, 'tii', dn)
SDTest.Call(0x2a1940, 'tvis', dn + 4, 'deleted')
check('CNodo3D_Delete 1', lambda: (SDTest.Call(0x23d120, 'tiii', dn, 1)[0] == dn, ncount() - c0))
dn = SDTest.Call(0x1334d0, 'ii', 0xfc)[0]
SDTest.Call(0x23cf40, 'tii', dn)
check('CNodo3D_dtor', lambda: (SDTest.Call(0x23d070, 'tvi', dn), nri(dn), ncount() - c0))
SDTest.Call(0x313634, 'vii', dn, 0xfc)
