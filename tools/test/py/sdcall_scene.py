# engine3d Engine/Scene.c: scenes built in heap memory, nodes without objects or with small CTriMesh meshes
# (Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt come from there). Nodes are printed as
# their index in esnodes, floats as their bits. Not tested here: CScene_Read/Write, ConvertD3D, ToSuelo, CalcPesos,
# PrecalcIlum and SetAlpha with real objects (the sdlevel scenario loads scenes through them), CompactMats.
def esnew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def esi(addr):
	return SDTest.Peek(addr, '@i')
def espoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def esints(addr, n):
	r = []
	for i in range(n):
		r.append(esi(addr + 4 * i))
	return r
esnodes = []
def eswho(p):
	if p == 0:
		return None
	for i in range(len(esnodes)):
		if esnodes[i] == p:
			return i
	return '?'
def esnodo(name):
	n = SDTest.Call('CNode3D_ctor', 'tii', esnew(0xfc))[0]
	SDTest.Call('String_Set', 'tvis', n + 4, name)
	esnodes.append(n)
	return n
# a node: name, Refs, sig, ant, padre, flags, escena set, indice, i54, obj3D set
def esnget(n):
	return (SDTest.Peek(n + 4, '@p'), esi(n + 8), eswho(esi(n + 0xc)), eswho(esi(n + 0x10)), eswho(esi(n + 0x14)),
		esi(n + 0x18), esi(n + 0x40) != 0, esi(n + 0x44), esi(n + 0x54), esi(n + 0x3c) != 0)
def eslist(e):
	r = []
	n = esi(e + 0xc)
	while n:
		r.append(eswho(n))
		n = esi(n + 0xc)
	return r
def esscene():
	e = SDTest.Call('CScene_ctor', 'tii', esnew(0x64))[0]
	espoke(e + 8, 1)
	return e
# a CTriMesh with a triangle at the corners given
def esmalla(verts):
	m = SDTest.Call('CTriMesh_ctor', 'tii', esnew(0x4c))[0]
	SDTest.Call('CTriMesh_SetNumVert', 'tviii', m, len(verts), 0)
	for i in range(len(verts)):
		SDTest.Call('CTriMesh_SetVert', 'tviiF', m, i, verts[i])
	SDTest.Call('CTriMesh_SetNumTri', 'tviii', m, 1, 0)
	SDTest.Call('CTriMesh_SetTriVerts', 'tviiiii', m, 0, 0, 1, 2)
	return m

if SDTest.Peek('g_ViewportW', 'i') and SDTest.Peek('g_ViewportH', 'i'):
	check('Render_GetViewportSize', lambda: SDTest.Call('Render_GetViewportSize', 'vpp', 0, 0) == (None, SDTest.Peek('g_ViewportW', 'i'),
		SDTest.Peek('g_ViewportH', 'i')))

e = esscene()
check('CScene_ctor', lambda: (esints(e, 0x18), esi(e + 0x60) & 0xff))
for name in ('root', 'a', 'b', 'c', 'cam'):
	n = esnodo(name)
	check('CScene_AddNode' + fmt((name,)), lambda n=n: (SDTest.Call('CScene_AddNode', 'tvii', e, n), eslist(e), esnget(n)))
check('CScene nodes', lambda: map(esnget, esnodes))
check('CScene_NumNodes', lambda: SDTest.Call('CScene_NumNodes', 'tii', e)[0])
for c in (0, 0xa1000001, 0xa2000004):
	check('CScene_CountClass' + fmt((c,)), lambda c=c: SDTest.Call('CScene_CountClass', 'tiii', e, c)[0])
for i in range(len(esnodes)):
	espoke(esnodes[i] + 0x44, 10 + i)
for i in (-1, 10, 12, 14, 15):
	check('CScene_NodeObject' + fmt((i,)), lambda i=i: eswho(SDTest.Call('CScene_NodeObject', 'tiii', e, i)[0]))
for f in (None, 0, 2, 4):
	if f is None:
		fr = 0
	else:
		fr = esnodes[f]
	for c in (0, 0xa2000004):
		check('CScene_FindClass' + fmt((f, c)), lambda fr=fr, c=c: eswho(SDTest.Call('CScene_FindClass', 'tiiii', e, fr, c)[0]))
for name in ('b', 'root', 'cam', 'zz'):
	check('CScene_FindNode' + fmt((name,)), lambda name=name: eswho(SDTest.Call('CScene_FindNode', 'tiis', e, name)[0]))
check('CScene_SetMarks', lambda: (SDTest.Call('CScene_SetMarks', 'tvii', e, 77), map(esnget, esnodes)))
check('CScene_UpdateNodes', lambda: (SDTest.Call('CScene_UpdateNodes', 'tvi', e), map(esnget, esnodes)))
check('CScene_Msg calc pesos', lambda: SDTest.Call('CScene_Msg', 'tviiiiii', e, 2, 0, 0, 0, 0))
check('CScene_Msg join none', lambda: (SDTest.Call('CScene_Msg', 'tviiiiii', e, 0, 0, 0, 0, 0), eslist(e)))

# the render and update of all nodes (no objects yet) with a context without render nodes
ctx = esnew(0xa8)
for i in range(0, 0xa8, 4):
	espoke(ctx + i, 0)
check('CScene_Update', lambda: (SDTest.Call('CScene_Update', 'tvii', e, ctx), esi(e + 0x10), map(esnget, esnodes)))
check('CScene_Render', lambda: (SDTest.Call('CScene_Render', 'tvii', e, ctx), esi(e + 0x10)))

# the cameras: a camera object on the node "cam", the floats at 0x2c and 0x30 set unless 0
cam = SDTest.Call('CCamera_ctor', 'tii', esnew(0x78))[0]
SDTest.Call('CNode3D_SetObj3D', 'tvii', esnodes[4], cam)
for a, b in ((0x3fc00000, 0), (0, 0x42c80000), (0x40000000, 0x43480000), (0x7fc00000, 0)):
	check('CScene_Msg camaras' + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('CScene_Msg', 'tviiiiii', e, 3, a, b, 0, 0),
		esi(cam + 0x2c), esi(cam + 0x30)))
check('CScene_CountClass cam', lambda: SDTest.Call('CScene_CountClass', 'tiii', e, 0xa2000004)[0])
check('CScene_FindClass cam', lambda: eswho(SDTest.Call('CScene_FindClass', 'tiiii', e, 0, 0xa2000004)[0]))

# the scale: positions of all nodes, the vertices of a mesh
for i in range(len(esnodes)):
	n = esnodes[i]
	SDTest.Call('crt_memcpy', 'iiFi', n + 0x1c, (1.5 * i, -2.0 + i, 0.25 * i), 12)
m1 = esmalla(((1.0, 2.0, 3.0), (-1.0, 0.5, 2.0), (0.0, -4.0, 1.0)))
SDTest.Call('CNode3D_SetObj3D', 'tvii', esnodes[1], m1)
for s in ((1.0, 1.0, 1.0), (2.0, 0.5, -1.0), (1.0, 3.0, 1.0)):
	check('CScene_SetScale' + fmt(s), lambda s=s: (SDTest.Call('CScene_SetScale', 'tviF', e, s)[1], map(lambda n: esints(n + 0x1c, 3),
		esnodes), esints(esi(m1 + 0x20), 9)))

# the box: from the mesh of node "a" (the others have none), merged into esout
esout = esnew(0x18)
SDTest.Call('crt_memcpy', 'iiFi', esout, (-9.0, -9.0, -9.0, 9.0, 9.0, 9.0), 0x18)
for first in (1, 0):
	check('CScene_UpdateBox' + fmt((first,)), lambda first=first: (SDTest.Call('CScene_UpdateBox', 'tviii', e, esout, first),
		esints(e + 0x24, 6), esints(esout, 6)))
check('CScene_UpdateBox NULL', lambda: (SDTest.Call('CScene_UpdateBox', 'tviii', e, 0, 1), esints(e + 0x24, 6)))

# join: the meshes of the children of the NODE_JOIN2 node "b" into it, the joined nodes deleted
m2 = esmalla(((4.0, 0.0, 0.0), (0.0, 4.0, 0.0), (0.0, 0.0, 4.0)))
m3 = esmalla(((-1.0, -1.0, -1.0), (2.0, -1.0, 0.0), (0.0, 3.0, 0.0)))
d = esnodo('d')
SDTest.Call('CScene_AddNode', 'tvii', e, d)
SDTest.Call('CNode3D_SetObj3D', 'tvii', d, m2)
SDTest.Call('CNode3D_SetParent', 'tvii', d, esnodes[2])
SDTest.Call('CNode3D_SetObj3D', 'tvii', esnodes[3], m3)
SDTest.Call('CNode3D_SetParent', 'tvii', esnodes[3], d)
for n in esnodes[1:]:
	if n != esnodes[2] and esi(n + 0x14) == 0:
		SDTest.Call('CNode3D_SetParent', 'tvii', n, esnodes[0])
espoke(esnodes[2] + 0x18, esi(esnodes[2] + 0x18) | 2)
check('CScene before join', lambda: map(esnget, esnodes))
check('CScene_Join', lambda: (SDTest.Call('CScene_Join', 'tviii', e, 0, 0), eslist(e)))
jm = esi(esnodes[2] + 0x3c)
if jm:
	check('CScene_Join mesh', lambda: (esi(jm + 8), esi(jm + 0x10), esints(esi(jm + 0x20), 3 * esi(jm + 8))))
check('CScene after join', lambda: map(lambda i: esnget(esnodes[i]), (0, 1, 2, 4)))

# the rest removed and deleted
check('CScene_RemoveNode', lambda: (SDTest.Call('CScene_RemoveNode', 'tvii', e, esnodes[4]), eslist(e), esnget(esnodes[4])))
SDTest.Call('CNode3D_FreeObj3D', 'tvi', esnodes[4])
SDTest.Call('CNode3D_Delete', 'tiii', esnodes[4], 1)
check('CScene_DeleteNodes', lambda: (SDTest.Call('CScene_DeleteNodes', 'tvi', e), eslist(e), esi(e + 0xc)))
espoke(e + 8, 0)
check('CScene_Delete', lambda: SDTest.Call('CScene_Delete', 'tiii', e, 1)[0] == e)

# Table_Resize4 on a table of its own
t = esnew(8)
espoke(t, 0)
espoke(t + 4, 0)
old = 0
for n in (3, 3, 5, 2, 0, 0, 4):
	# only the part copied from the old table is printed
	k = min(old, n)
	check('Table_Resize4' + fmt((n,)), lambda n=n, k=k: (SDTest.Call('Table_Resize4', 'tvii', t, n), esi(t), esi(t + 4) != 0,
		esi(t + 4) and esints(esi(t + 4), k)))
	if esi(t + 4):
		for i in range(esi(t)):
			espoke(esi(t + 4) + 4 * i, 100 * n + i)
	old = esi(t)
SDTest.Call('Table_Resize4', 'tvii', t, 0)
