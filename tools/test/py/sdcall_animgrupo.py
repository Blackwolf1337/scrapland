# engine3d Engine/AnimGrupo.c: animation groups on fake scenes, nodes and animations in heap memory (Python 1.5.2, runs
# inside sdcall.py after the other checks: check, fmt, tpeek come from there). Pointers are printed as indices of
# the objects made here, floats as their bits. Fake scenes keep Refs above 0, so no destructor of the exe runs on
# them. Not tested: AnimGrupo_AlignNode with a node position from the animation frames (CAnimNodo_GetPosRot true).
def agcalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def agfree(p):
	SDTest.Call(0x133550, 'vi', p)
def agnew(n):
	return SDTest.Call(0x1334d0, 'ii', n)[0]
def agnewa(n):
	return SDTest.Call(0x313642, 'ii', n)[0]
def agdel(p, n):
	SDTest.Call(0x313634, 'vii', p, n)
def agdela(p):
	SDTest.Call(0x31364b, 'vi', p)
def agwi(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def agwf(addr, floats):
	SDTest.Call(0x3197d0, 'iiFi', addr, floats, 4 * len(floats))
def agi(addr):
	return SDTest.Peek(addr, '@i')
def agb(addr):
	return agi(addr) & 0xff
def agw(addr):
	return agi(addr) & 0xffff
# a string in Mem_Calloc memory, ASCII only
agstrs = []
def agstr(s):
	p = agcalloc((len(s) + 4) / 4 * 4)
	i = 0
	while i < len(s):
		w = 0
		for k in range(4):
			if i + k < len(s):
				w = w | (ord(s[i + k]) << (8 * k))
		agwi(p + i, w)
		i = i + 4
	agstrs.append(p)
	return p
# the index of p in objs, None for NULL, '?' for something else
def agidx(p, objs):
	if p == 0:
		return None
	for i in range(len(objs)):
		if objs[i] == p:
			return i
	return '?'
# the name shows only the string arguments, the others are heap pointers
def agcall(name, rva, sig, *args):
	shown = []
	for a in args:
		if type(a) == type(''):
			shown.append(a)
	check(name + fmt(tuple(shown)), lambda rva=rva, sig=sig, args=args: apply(SDTest.Call, (rva, sig) + args))

# MorphData_Find on 10 steps with some tags
md = agcalloc(0x50)
for i, tag in ((2, 0x6261), (5, 0x6463), (7, 0x6261), (9, 0x61)):
	agwi(md + 8 * i, tag)
for t in ('ab', 'cd', 'a', 'zz', 'abc', 'ba', 'dc'):
	agcall('MorphData_Find', 0x251ac0, 'iis', md, t)
agwi(md, 0x7a7a)
agcall('MorphData_Find first', 0x251ac0, 'iis', md, 'zz')

# the event lists
evs = []
def agev(t, id, arg):
	e = agnew(0x14)
	SDTest.Call(0x2bb8e0, 'tiifii', e, t, id, arg)
	evs.append(e)
	return e
def agevlist(lst):
	r = []
	e = agi(lst)
	while e:
		r.append((agidx(e, evs), agi(e), agi(e + 4), agi(e + 12), agi(e + 16)))
		e = agi(e + 8)
	return r
evl = agcalloc(4)
for t, id in ((5.0, 1), (1.0, 2), (3.0, 3), (3.0, 1), (5.0, 7)):
	SDTest.Call(0x2bba70, 'tiii', evl, agev(t, id, id * 10))
check('AnimEventos list', lambda: agevlist(evl))
check('AnimEventos_Free', lambda: (SDTest.Call(0x251a90, 'tvi', evl), agi(evl)))
check('AnimEventos_Free empty', lambda: (SDTest.Call(0x251a90, 'tvi', evl), agi(evl)))

# MorphData_Read from an ini made here
ini = SDTest.Call(0x1335a0, 'ii', 4)[0]
SDTest.Call(0x261980, 'tii', ini)
for key, val in (('00_Data', '1 2 3 4 5 6'), ('00_Tag', 'ab'), ('01_Data', ' 0 0 1 9 9 255'), ('02_Data', '4 7 25 0 1 100'),
		('02_Tag', 'xyz'), ('04_Data', '2  3\t4 5 6 7'), ('04_Tag', 'Q7'), ('05_Data', '0 1 200 2 3 4')):
	SDTest.Call(0x2621a0, 'tiisss', ini, 'Morphing:grpm', key, val)
for key, val in (('00_Data', '2 0 30 1 0 50'), ('01_Data', '1 0 30 0 0 50'), ('01_Tag', 'zz')):
	SDTest.Call(0x2621a0, 'tiisss', ini, 'Morphing:anim1', key, val)
mdr = agcalloc(0x50)
for i in range(7):
	agwi(mdr + 8 * i, 0x11223344)
	agwi(mdr + 8 * i + 4, 0x55667788)
	check('MorphData_Read' + fmt((i,)), lambda i=i: (SDTest.Call(0x251b30, 'bisiii', ini, 'Morphing:grpm', i, mdr + 8 * i, 5)[0],
		agi(mdr + 8 * i), agi(mdr + 8 * i + 4)))
check('MorphData_Read none', lambda: (SDTest.Call(0x251b30, 'bisiii', ini, 'nosect', 0, mdr, 5)[0], agi(mdr), agi(mdr + 4)))

# fake scenes: a CEscena of 0x50 bytes, nodes of 0x60 bytes in a list by sig (0xc)
nodes = []
def agnode(name, iobj, escena=0, padre=0, pos=None):
	n = agcalloc(0x60)
	agwi(n + 4, agstr(name))
	agwi(n + 0x14, padre)
	agwi(n + 0x40, escena)
	agwi(n + 0x44, iobj)
	if pos:
		agwf(n + 0x1c, pos)
	nodes.append(n)
	return n
def aglink(lst):
	for i in range(len(lst) - 1):
		agwi(lst[i] + 0xc, lst[i + 1])
	return lst[0]
def agescena(first=0, nodo=None, ini=0):
	e = agcalloc(0x50)
	agwi(e + 8, 10)
	agwi(e + 0xc, first)
	if nodo:
		agwi(e + 0x40, agstr(nodo))
	agwi(e + 0x4c, ini)
	return e
# a CAnimacion with nanim empty nodes of 0x24 bytes (new[] with the count before them)
def aganimacion(name, primero, frames, morph, nanim):
	a = agcalloc(0x30)
	agwi(a, agstr(name))
	agwf(a + 8, (30.0,))
	agwi(a + 0xc, primero)
	agwi(a + 0x10, frames)
	agwi(a + 0x14, morph)
	arr = agnewa(4 + 0x24 * nanim)
	agwi(arr, nanim)
	for k in range(9 * nanim):
		agwi(arr + 4 + 4 * k, 0)
	agwi(a + 0x20, arr + 4)
	agwi(a + 0x24, nanim)
	return a
def aganimnodo(a, i):
	return agi(a + 0x20) + 0x24 * i

# groups by AnimGrupo_New: everything but pointers and the memory the constructor of CAnimCtrl leaves
grupos = []
def aggrupo(g, a):
	r = [SDTest.Peek(g, '@p')]
	r.append(agevlist(g + 4))
	c = g + 8
	r.append((agi(c), agi(c + 4), agi(c + 8), agi(c + 12), agb(c + 0x10)))
	v = []
	for off in range(0x14, 0x30, 4) + [0x34, 0x38, 0x40, 0x4c, 0x50, 0x54, 0x58, 0x60, 0x80]:
		v.append(agi(c + off))
	r.append(v)
	r.append(agb(c + 0x5c))
	nodo = agi(c + 0x30)
	if nodo:
		nodo = (nodo - agi(a + 0x20)) / 0x24
	else:
		nodo = None
	r.append(nodo)
	r.append(agi(c + 0x3c) == g + 4)
	m = agi(g + 0x8c)
	if m:
		r.append(tpeek(m, 20))
		r.append(agw(g + 0x90))
	else:
		r.append(None)
	r.append(agidx(agi(g + 0x94), escenas))
	return r
escenas = []
def agnewgrupo(name, escena, anim, nombre):
	r = SDTest.Call(0x252000, 'iiiz', escena, anim, nombre)[0]
	grupos.append(r)
	check(name + fmt((nombre,)), lambda g=r, a=agi(anim + 0x3c), anim=anim: (aggrupo(g, a), agi(anim + 8)))
	return r

am = aganimacion('anim1', 2, 7, 0, 3)
tab = agcalloc(4)
agwi(tab, 0x00010200)
agwi(am + 0x18, 4)
agwi(am + 0x1c, tab)
anim = agescena()
agwi(anim + 0x3c, am)
esc = agescena(aglink([agnode('x', 3), agnode('root', 1), agnode('y', 2)]), 'root')
esc2 = agescena(aglink([agnode('x', 3)]), 'nonode')
esc3 = agescena(aglink([agnode('root', 2)]), None, ini)
escenas = [anim, esc, esc2, esc3]
agnewgrupo('AnimGrupo_New', esc, anim, 'grp')
agnewgrupo('AnimGrupo_New', esc2, anim, None)
agnewgrupo('AnimGrupo_New', esc2, anim, '')
agwi(am + 0x14, 1)
agnewgrupo('AnimGrupo_New morph', esc, anim, 'grp')
agwi(am + 0x10, 5)
agnewgrupo('AnimGrupo_New morph ini', esc3, anim, 'grpm')
agnewgrupo('AnimGrupo_New morph ini name', esc3, anim, None)
agnewgrupo('AnimGrupo_New morph no section', esc3, anim, 'none')
agwi(am + 0x10, 1)
agnewgrupo('AnimGrupo_New morph 1 frame', esc, anim, 'one')
agwi(am + 0x14, 0)
am2 = aganimacion('self', 0, 4, 0, 1)
anim2 = agescena(aglink([agnode('root', 0)]), 'root')
agwi(anim2 + 0x3c, am2)
escenas.append(anim2)
agnewgrupo('AnimGrupo_New self', anim2, anim2, 'me')

# the slots of a scene
slots = agcalloc(44 * 4)
def agslots():
	r = []
	for i in range(44):
		r.append(agidx(agi(slots + 4 * i), grupos))
	return r
for g in grupos[:5]:
	agcall('CAnimGrupos_Add', 0x251d00, 'tiii', slots, g)
agwi(slots + 4, 0)
agcall('CAnimGrupos_Add hole', 0x251d00, 'tiii', slots, grupos[5])
agwi(slots + 4 * 41, grupos[6])
agwi(slots + 4 * 43, grupos[7])
check('CAnimGrupos slots', agslots)
agcall('CAnimGrupos_Count', 0x2529b0, 'tii', slots)
for i in (0, 1, 4, 5, 41, 43, 44, -1, 1000):
	check('CAnimGrupos_Get' + fmt((i,)), lambda i=i: agidx(SDTest.Call(0x251d70, 'tiii', slots, i)[0], grupos))
for n in ('grp', 'GRP', 'anim1', 'grpm', 'one', 'none', 'me', 'nothing', ''):
	check('CAnimGrupos_Find' + fmt((n,)), lambda n=n: (lambda r: (agidx(r[0], grupos), r[1]))(SDTest.Call(0x251d90, 'tiisp', slots, n, -7)))
	check('CAnimGrupos_Find noidx' + fmt((n,)), lambda n=n: agidx(SDTest.Call(0x251d90, 'tiisi', slots, n, 0)[0], grupos))
check('CAnimGrupos_Find NULL', lambda: SDTest.Call(0x251d90, 'tiizp', slots, None, -7))
check('CAnimGrupo_dtor', lambda: (SDTest.Call(0x251980, 'tvi', grupos[8]), agi(grupos[8]), agi(grupos[8] + 4),
	agi(grupos[8] + 0x8c), agi(grupos[8] + 0x94), agi(anim2 + 8)))
agdel(grupos[8], 0x98)
check('CAnimGrupos_Clear', lambda: (SDTest.Call(0x251940, 'tvi', slots), agslots(), agi(anim + 8)))
agcall('CAnimGrupos_Count empty', 0x2529b0, 'tii', slots)
for i in range(44):
	agwi(slots + 4 * i, 1)
agcall('CAnimGrupos_Count full', 0x2529b0, 'tii', slots)
agwi(slots + 4 * 7, 0)
agwi(slots + 4 * 42, 0)
agcall('CAnimGrupos_Count 42', 0x2529b0, 'tii', slots)
for i in range(44):
	agwi(slots + 4 * i, 0)
# a group with a morph table, its events and a name destroyed by Clear
g = agnew(0x98)
agwi(g, 0)
agwi(g + 4, 0)
agwi(g + 0x8c, agnewa(0x50))
agwi(g + 0x94, anim)
SDTest.Call(0x2a1940, 'tvis', g, 'fake')
SDTest.Call(0x2bba70, 'tiii', g + 4, agev(2.0, 4, 0))
SDTest.Call(0x2bba70, 'tiii', g + 4, agev(1.0, 4, 0))
SDTest.Call(0x251d00, 'tiii', slots, g)
check('CAnimGrupos_Clear fake', lambda: (SDTest.Call(0x251940, 'tvi', slots), agslots(), agi(anim + 8)))

# AnimGrupo_AlignNode: the animation node of a scene node moved (the node has no frames, so its position comes from
# the scene node of the animation), and an animation without the node: all nodes vertex animations
amA = aganimacion('align', 0, 1, 0, 2)
pos = agcalloc(12)
agwf(pos, (1.0, 2.0, 3.0))
an = aganimnodo(amA, 1)
agwi(an + 4, 1)
agwi(an + 8, pos)
animA = agescena(aglink([agnode('other', 0), agnode('bone', 1, 0, 0, (0.5, 0.25, -1.0))]))
agwi(animA + 0x3c, amA)
escA = agescena(aglink([agnode('bone', 0, 0, 0, (1.5, -2.0, 3.25))]))
agcall('AnimGrupo_AlignNode NULL', 0x251df0, 'biiz', escA, animA, None)
check('AnimGrupo_AlignNode', lambda: (SDTest.Call(0x251df0, 'biiz', escA, animA, 'bone')[0], tpeek(pos, 3), agb(amA + 0x14)))
amV = aganimacion('vert', 0, 1, 0, 3)
dummy = agcalloc(16)
for i in range(3):
	agwi(aganimnodo(amV, i) + 0x20, dummy)
animV = agescena(aglink([agnode('bone', 0)]))
agwi(animV + 0x3c, amV)
check('AnimGrupo_AlignNode vert', lambda: (SDTest.Call(0x251df0, 'biiz', escA, animV, 'nobone')[0], agb(amV + 0x14)))

# AnimGrupo_MapNodes: the table of the animation for the nodes of a scene, matched by name and parent name
escM = agcalloc(0x50)
agwi(escM + 8, 10)
pp = agnode('p', -1)
pq = agnode('q', -1)
px = agnode('x', -1)
pz = agnode('z', -1)
escM_nodes = [agnode('a', 0, escM, pp), agnode('b', 3, escM), agnode('c', -1, escM), agnode('d', 1, escM, pp),
	agnode('e', 5, escM), agnode('f', 2, escM), agnode('g', 6, escM, px)]
agwi(escM + 0xc, aglink(escM_nodes))
animM_nodes = [agnode('a', 9, 0, px), agnode('a', 4, 0, pp), agnode('b', 7, 0, pq), agnode('c', 1, 0), agnode('d', 254, 0, pz),
	agnode('e', 0, 0), agnode('e', 8, 0), agnode('g', 3, 0, pq), agnode('g', 11, 0, pz)]
amM = aganimacion('map', 0, 1, 0, 1)
animM = agescena(aglink(animM_nodes))
agwi(animM + 0x3c, amM)
def agtabla(a):
	n = agi(a + 0x18)
	p = agi(a + 0x1c)
	r = []
	for i in range(n):
		r.append(agb(p + i))
	return n, r
check('AnimGrupo_MapNodes', lambda: (SDTest.Call(0x2525b0, 'vii', escM, animM), agtabla(amM)))
agdela(agi(amM + 0x1c))
agwi(amM + 0x18, 0)
agwi(amM + 0x1c, 0)
# the scene as its own animation
escS = agcalloc(0x50)
agwi(escS + 8, 10)
agwi(escS + 0xc, aglink([agnode('s', 2, escS), agnode('t', 0, escS)]))
agwi(escS + 0x3c, amM)
check('AnimGrupo_MapNodes self', lambda: (SDTest.Call(0x2525b0, 'vii', escS, escS), agtabla(amM)))
agdela(agi(amM + 0x1c))
agwi(amM + 0x1c, 0)

# AnimGrupo_Compact: animated nodes to the front, the empty ones at the end removed
amC = aganimacion('compact', 0, 1, 0, 4)
agwi(aganimnodo(amC, 1) + 0x20, dummy)
agwi(aganimnodo(amC, 3) + 0x20, dummy)
animC_nodes = [agnode('n0', 0), agnode('n1', 1), agnode('n2', 2), agnode('n3', 3), agnode('nx', -1)]
animC = agescena(aglink(animC_nodes))
agwi(animC + 0x3c, amC)
def agcompact():
	r = []
	for n in animC_nodes:
		r.append(agi(n + 0x44))
	v = []
	for i in range(agi(amC + 0x24)):
		v.append(agi(aganimnodo(amC, i) + 0x20) == dummy)
	return r, agi(amC + 0x24), agi(agi(amC + 0x20) - 4), v
check('AnimGrupo_Compact', lambda: (SDTest.Call(0x2528b0, 'vi', animC), agcompact()))
check('AnimGrupo_Compact again', lambda: (SDTest.Call(0x2528b0, 'vi', animC), agcompact()))

# the rest freed: the nodes arrays without their destructors (their members are cleared first)
for a in (am, am2, amA, amV, amM, amC):
	arr = agi(a + 0x20)
	for k in range(9 * agi(arr - 4)):
		agwi(arr + 4 * k, 0)
	agdela(arr - 4)
	agfree(a)
# the one group not in the slots any more
SDTest.Call(0x251980, 'tvi', grupos[1])
agdel(grupos[1], 0x98)
check('AnimGrupo refs at the end', lambda: (agi(anim + 8), agi(anim2 + 8)))
for p in nodes + escenas + [escA, animA, animV, escM, animM, escS, animC, md, mdr, evl, slots, tab, pos, dummy]:
	agfree(p)
for p in agstrs:
	agfree(p)
SDTest.Call(0x261990, 'tvi', ini)
agfree(ini)
