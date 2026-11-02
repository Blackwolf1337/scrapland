# engine3d Engine/NodeFX.c, RenderEVA.c (the particle corners), RenderMeshD3D.c (the batch tables, their orders),
# ModelFX.c (the materials), CalcWeights.c, CalcIllum.c, Graphic3D.c and Portal.c leftovers, the table.h instances, on
# objects made here (Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt come from there). Floats
# are printed as their bits. Not tested: the drawing of the skinned and animated meshes, the particles and the model
# effects (they need a model of a level drawn in a frame).
def rn2new(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def rn2i(a):
	return SDTest.Peek(a, '@i')
def rn2ints(a, n):
	r = []
	for i in range(n):
		r.append(rn2i(a + 4 * i))
	return r
def rn2pk(a, v):
	SDTest.Poke(a - 0x400000, v)
def rn2pf(a, t):
	SDTest.Call('crt_memcpy', 'iiFi', a, t, 4 * len(t))
def rn2pb(a, v):
	rn2pk(a & ~3, (rn2i(a & ~3) & ~(0xff << (8 * (a & 3)))) | (v << (8 * (a & 3))))
def rn2log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
rn2id = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
rn2frame = SDTest.Peek('g_FrameCount', 'i')
rn2now = SDTest.Peek('g_TimeMs', 'i')

# the table.h instances
rn2t = rn2new(8)
for rva in ('Table_Free@2b8dd0', 'Table_Free@2bbcf0', 'Table_Free@2bbd20'):
	rn2pk(rn2t, 4)
	rn2pk(rn2t + 4, SDTest.Call('operator_new_array', 'ii', 32)[0])
	check('Table_Free' + fmt((rva,)), lambda rva=rva: (SDTest.Call(rva, 'tvi', rn2t), rn2ints(rn2t, 2),
		SDTest.Call(rva, 'tvi', rn2t), rn2ints(rn2t, 2)))
check('Table_Alloc10', lambda: (SDTest.Call('Table_Alloc10@2b8e00', 'tvii', rn2t, 5), rn2i(rn2t), rn2i(rn2t + 4) != 0))

# the qsort orders of the batches: entries in cells, the meshes and materials faked
def rn2entry(off, v):
	e = rn2new(0x50)
	SDTest.Call('crt_memcpy', 'iiFi', e + off, (v,), 4)
	c = rn2new(4)
	rn2pk(c, e)
	return c
for a, b in ((1.0, 2.0), (2.0, 1.0), (3.0, 3.0), (-1.0, 0.0), (0.0, -0.0)):
	check('RenderTransp_Compare' + fmt((a, b)), lambda a=a, b=b: SDTest.Call('RenderTransp_Compare', 'iii', rn2entry(0x4c, a),
		rn2entry(0x4c, b))[0])
	check('RenderObj_Compare' + fmt((a, b)), lambda a=a, b=b: SDTest.Call('RenderObj_Compare', 'iii', rn2entry(0x48, a),
		rn2entry(0x48, b))[0])
def rn2mentry(tex, mat, skin):
	m = rn2new(0xc8)
	rn2pk(m + 0x4c, mat)
	rn2pk(m + 0x94, skin)
	e = rn2new(0x50)
	rn2pk(e, m)
	rn2pk(e + 0x44, tex)
	c = rn2new(4)
	rn2pk(c, e)
	return c
for ta, tb, ma, mb, sa, sb in ((1, 2, 0, 0, 0, 0), (5, 2, 0, 0, 0, 0), (3, 3, 0x100, 0x80, 0, 0), (3, 3, 0x80, 0x100, 0, 0),
		(3, 3, 0x80, 0x80, 1, 0), (3, 3, 0x80, 0x80, 0, 1), (3, 3, 0x80, 0x80, 1, 1)):
	check('RenderMesh_Compare' + fmt((ta, tb, ma, mb, sa, sb)), lambda ta=ta, tb=tb, ma=ma, mb=mb, sa=sa, sb=sb:
		SDTest.Call('RenderMesh_Compare', 'iii', rn2mentry(ta, ma, sa), rn2mentry(tb, mb, sb))[0])

# the batches when nothing is queued, a transparent list without a mesh (only its group counted)
check('RenderMeshD3D_Flush', lambda: rn2log(lambda: (SDTest.Call('RenderMeshD3D_Flush', 'i')[0], SDTest.Peek('g_UsedMesh', 'i'))))
check('RenderObjBase3D_Flush', lambda: rn2log(lambda: (SDTest.Call('RenderObjBase3D_Flush', 'i')[0], SDTest.Peek('g_UsedObj', 'i'))))
check('ParticleSystem_Draw NULL', lambda: rn2log(lambda: (SDTest.Call('ParticleSystem_Draw', 'iii', 0, 0)[0], SDTest.Peek('g_UsedObj', 'i'))))
rn2grp = SDTest.Peek('g_TranspGroup', 'i')
check('RenderMeshD3D_AddTransp NULL', lambda: (SDTest.Call('RenderMeshD3D_AddTransp', 'iiiii', 0, sym('g_MatIdentity') + 0x400000, 0, 0)[0],
	SDTest.Peek('g_TranspGroup', 'i') - rn2grp, SDTest.Call('RenderMeshD3D_AddTransp', 'iiiii', 0, sym('g_MatIdentity') + 0x400000, 1, 0)[0],
	SDTest.Peek('g_TranspGroup', 'i') - rn2grp, SDTest.Peek('g_UsedTransp', 'i')))
check('RenderMeshD3D_Flush2', lambda: rn2log(lambda: (SDTest.Call('RenderMeshD3D_Flush2', 'i')[0], SDTest.Peek('g_UsedTransp', 'i'),
	SDTest.Peek('g_TranspGroup', 'i'))))
check('MeshD3D_Render NULL', lambda: (SDTest.Call('MeshD3D_Render', 'iii', 0, 0)[0], SDTest.Peek('g_UsedMesh', 'i')))

# the stubs
for rva in ('FXMisc_Nop@2b96d0', 'FXMisc_Nop@2b96e0', 'FXMisc_Nop@2b9e40', 'FXMisc_Nop@2ba7a0'):
	check('Obj3D nop' + fmt((rva,)), lambda rva=rva: SDTest.Call(rva, 'tvii', rn2t, 7))
check('Graphic3DVert_ctor', lambda: SDTest.Call('Graphic3DVert_ctor', 'tii', rn2t)[0] == rn2t)
def rn2g3(g):
	m = rn2i(g + 0x20)
	r = (rn2ints(g + 4, 6), rn2i(g + 0x1c), m != 0, rn2ints(g + 0x28, 5))
	if m:
		r = r + (rn2i(m + 0x14), rn2i(m + 0x30), rn2i(m + 0x34), SDTest.Call('CMaterial_GetMapName', 'tsii', m, 0)[0],
			SDTest.Call('CMaterial_GetMapName', 'tsii', m, 1)[0])
	return r
for tex, opac in (('bmp/partic1.bmp', None), ('bmp/partic1.bmp', 'bmp/partic1.bmp')):
	check('CGraphic3D_ctorTex2' + fmt((tex, opac)), lambda tex=tex, opac=opac: rn2g3(SDTest.Call('CGraphic3D_ctorTex2', 'tiisz',
		rn2new(0x3c), tex, opac)[0]))

# the weights and the preillumination of a mesh without them
rn2m = rn2new(0xc8)
check('CMeshD3D_CalcWeights none', lambda: SDTest.Call('CMeshD3D_CalcWeights', 'tbii', rn2m, 0)[0])
check('CMeshD3D_CalcIllum no mat', lambda: (SDTest.Call('CMeshD3D_CalcIllum', 'tviii', rn2m, 0, 0), rn2ints(rn2m + 0x14, 5)))

# the debug drawing of a portal
rn2pn = SDTest.Call('CNode3D_ctor', 'tii', rn2new(0xfc))[0]
rn2pk(rn2pn + 8, 1)
rn2pf(rn2pn + 0x5c, (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 3.0, 4.0, 5.0, 1.0))
rn2po = SDTest.Call('CPortal_ctor', 'tii', rn2new(0x68))[0]
SDTest.Call('CObjBase3D_SetNode', 'tvii', rn2po, rn2pn)
rn2pf(rn2po + 0xc, (4.0, 2.0))
rn2sp = SDTest.Peek('g_ShowPortals', 'i')
SDTest.Poke('g_ShowPortals', 1)
check('CPortal_DrawDebug', lambda: (rn2log(lambda: SDTest.Call('CPortal_DrawDebug', 'tvii', rn2po, 0x123456)), SDTest.Peek('g_MatFX1', 'i'),
	(SDTest.Peek(sym('g_PortalMat') + 0x38, 'i') >> 8) & 0xff, SDTest.Peek(sym('g_PortalMat') + 0x40, 'i')))
SDTest.Poke('g_ShowPortals', rn2sp)

# the corners of particles: static and active ones, turned, sparks, moved to the camera, faded
rn2bill = (0.8, 0.0, -0.6, 0.0, 0.1, 0.9, 0.2, 0.0, 0.6, -0.1, 0.8, 0.0, 10.0, 20.0, -30.0, 1.0)
rn2view = (0.8, 0.1, 0.6, 0.0, 0.0, 0.9, -0.1, 0.0, -0.6, 0.2, 0.8, 0.0, 1.0, -2.0, 50.0, 1.0)
rn2b = rn2new(64)
rn2pf(rn2b, rn2bill)
rn2v = rn2new(64)
rn2pf(rn2v, rn2view)
rn2out = rn2new(0x70)
def rn2part(active, rot, sparks):
	if active:
		p = SDTest.Call('CParticleActive_ctor', 'tii', rn2new(0x90))[0]
	else:
		p = SDTest.Call('CParticleStatic_ctor', 'tii', rn2new(0x34))[0]
	rn2pb(p + 4, 1)
	rn2pf(p + 8, (1.5, -2.0, 3.0, 1.25, 2.5))
	rn2pk(p + 0x1c, -0x7f007fc0)
	rn2pf(p + 0x20, (rot, 0.125, 0.25, 0.75, 0.5))
	if active:
		rn2pb(p + 0x44, sparks)
		rn2pf(p + 0x48, (40.0, -25.0, 10.0))
		rn2pf(p + 0x80, (0.5, 6.0))
		rn2pf(p + 0x40, (0.0,))
	return p
for active, rot, sparks in ((0, 0.0, 0), (0, 0.7, 0), (1, 0.0, 0), (1, -1.3, 0), (1, 0.0, 1), (1, 0.4, 1)):
	rn2p = rn2part(active, rot, sparks)
	for alpha, zbias in ((1.0, 0.0), (0.5, 0.0), (1.0, 0.3), (0.25, 2.0)):
		check('Particle_Quad' + fmt((active, rot, sparks, alpha, zbias)), lambda p=rn2p, alpha=alpha, zbias=zbias:
			(SDTest.Call('Particle_Quad', 'viiiiff', p, rn2b, rn2v, rn2out, alpha, zbias), rn2ints(rn2out, 28)))

# the node effects: nodes with user properties, a parent, render nodes with a look
def rn2ini(keys):
	ini = SDTest.Call('CIniFile_ctor', 'tii', rn2new(4))[0]
	for k, v in keys:
		SDTest.Call('CIniFile_SetStr', 'tiisss', ini, 'NodeEffect', k, v)
	return ini
def rn2node(m, ini):
	n = SDTest.Call('CNode3D_ctor', 'tii', rn2new(0xfc))[0]
	rn2pk(n + 8, 1)
	rn2pf(n + 0x5c, m)
	rn2pk(n + 0xf8, ini)
	return n
def rn2r(frame, pos):
	r = rn2new(0xa8)
	rn2pb(r + 4, 2)
	rn2pk(r + 0xc, rn2new(0x4c))
	rn2pf(r + 0x10, rn2id)
	rn2pf(r + 0x40, pos)
	rn2pk(r + 0x90, frame)
	return r
rn2parent = rn2node((1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 10.0, 0.0, 1.0), 0)
def rn2fxnode(keys, pos):
	n = rn2node((0.6, 0.0, -0.8, 0.0, 0.0, 1.0, 0.0, 0.0, 0.8, 0.0, 0.6, 0.0) + pos + (1.0,), rn2ini(keys))
	rn2pk(n + 0x14, rn2parent)
	return n
def rn2spring(fx):
	return (rn2i(fx) == symva('CNodeFX_Spring_vftable'), rn2ints(fx + 4, 6), rn2i(fx + 0x20) - rn2i(fx + 0x1c), rn2ints(fx + 0x24, 7))
def rn2wheel(fx):
	return (rn2i(fx) == symva('CNodeFX_Wheel_vftable'), rn2ints(fx + 4, 16))
for t, keys in (('Spring', ()), ('Cloth', (('Damping', '0.3'),)), ('Hair', (('Spring', '0.25'), ('Torsion', '0.9'))),
		('Bust', (('Damp2', '0.75'),))):
	check('NodeFX_New' + fmt((t, keys)), lambda t=t, keys=keys: rn2spring(SDTest.Call('NodeFX_New', 'isi', t,
		rn2fxnode(keys, (1.0, 2.0, 3.0)))[0]))
for t, keys in (('Wheel', ()), ('Roll', ()), ('wheel', (('Radius', '2'), ('Rotation', '1 0 0.5'), ('Translation', '0 0 1'),
		('Roll', '0 1 0'), ('NoiseVel', '0 0 0'), ('NoiseAmt', '1 2 3')))):
	check('NodeFX_New' + fmt((t, keys)), lambda t=t, keys=keys: rn2wheel(SDTest.Call('NodeFX_New', 'isi', t,
		rn2fxnode(keys, (1.0, 2.0, 3.0)))[0]))
check('NodeFX_New unknown', lambda: SDTest.Call('NodeFX_New', 'isi', 'Foo', rn2fxnode((), (1.0, 2.0, 3.0)))[0])
check('NodeFX_New Type', lambda: rn2spring(SDTest.Call('NodeFX_New', 'iii', 0, rn2fxnode((('Type', 'Spring'),),
	(1.0, 2.0, 3.0)))[0]))

# a spring: a reset step, a blend between the steps, the next step after the time moved on
rn2sn = rn2fxnode((('Damping', '0.2'), ('Spring', '0.5'), ('Torsion', '0.6'), ('Damp2', '0.5')), (1.0, 2.0, 3.0))
rn2s = SDTest.Call('NodeFX_New', 'isi', 'Spring', rn2sn)[0]
rn2sr = rn2r(rn2frame - 5, (1.0, 2.0, 3.0))
def rn2apply(fx, n, r):
	return (SDTest.Call(SDTest.Peek(rn2i(fx) + 8, '@i') - 0x400000, 'tiiiii', fx, n + 0x5c, n, r)[0], rn2ints(n + 0x5c, 16))
check('CNodeFX_Spring_Apply reset', lambda: (rn2apply(rn2s, rn2sn, rn2sr), rn2spring(rn2s)))
rn2pk(rn2sr + 0x90, rn2frame)
rn2pf(rn2sn + 0x8c, (4.0, 1.0, -2.0))
rn2pk(rn2s + 0x1c, rn2now - 10)
rn2pk(rn2s + 0x20, rn2now + 30)
check('CNodeFX_Spring_Apply blend', lambda: (rn2apply(rn2s, rn2sn, rn2sr), rn2spring(rn2s)))
rn2pk(rn2s + 0x1c, rn2now + 5)
rn2pf(rn2sn + 0x8c, (-3.0, 6.0, 2.0))
check('CNodeFX_Spring_Apply step', lambda: (rn2apply(rn2s, rn2sn, rn2sr), rn2spring(rn2s)))
rn2pk(rn2s + 0x1c, 0)
rn2pk(rn2s + 0x20, 0)
check('CNodeFX_Spring_Apply step2', lambda: (rn2apply(rn2s, rn2sn, rn2sr), rn2spring(rn2s)))
check('CNodeFX_Spring_Type', lambda: SDTest.Call('CNodeFX_Spring_Type', 'tbi', rn2s)[0])
rn2s2 = SDTest.Call('NodeFX_Clone', 'ii', rn2s)[0]
check('NodeFX_Clone spring', lambda: rn2spring(rn2s2))
check('CNodeFX_Spring_Reset', lambda: (SDTest.Call('CNodeFX_Spring_Reset', 'tvi', rn2s), rn2spring(rn2s)))
check('CNodeFX_Spring_Delete', lambda: (SDTest.Call('CNodeFX_Spring_Delete', 'tiii', rn2s2, 0)[0] == rn2s2, rn2i(rn2s2) == symva('INodeFX_vftable'),
	SDTest.Call('CNodeFX_Spring_Delete', 'tiii', rn2s2, 1)[0] == rn2s2))

# a wheel turned by the way it moved from the render node, and the same frame again
# the seconds of the frame fixed (the roll per second)
rn2dt = SDTest.Peek('g_FrameSec', 'i')
SDTest.Call('crt_memcpy', 'iiFi', sym('g_FrameSec') + 0x400000, (0.03125,), 4)
rn2wn = rn2fxnode((('Radius', '2'), ('Rotation', '1 0 0.5'), ('Translation', '0.5 0 1')), (1.0, 2.0, 3.0))
rn2w = SDTest.Call('NodeFX_New', 'isi', 'Wheel', rn2wn)[0]
rn2wr = rn2r(rn2frame - 5, (-2.0, 2.5, 7.0))
def rn2look(r):
	l = rn2i(r + 0xc)
	return (rn2i(l), rn2ints(l + 4, 3), rn2i(l + 0x20) & 3)
check('CNodeFX_Wheel_Apply', lambda: (rn2apply(rn2w, rn2wn, rn2wr), rn2ints(rn2w + 0x44, 3), rn2look(rn2wr)))
rn2pk(rn2wr + 0x90, rn2frame)
check('CNodeFX_Wheel_Apply again', lambda: (rn2apply(rn2w, rn2wn, rn2wr), rn2ints(rn2w + 0x44, 3), rn2look(rn2wr)))
rn2pk(rn2wr + 0x90, rn2frame - 1)
rn2pf(rn2wr + 0x40, (8.0, 2.0, -1.0))
check('CNodeFX_Wheel_Apply 2', lambda: (rn2apply(rn2w, rn2wn, rn2wr), rn2ints(rn2w + 0x44, 3), rn2look(rn2wr)))
rn2rn = rn2fxnode((('Radius', '0'),), (1.0, 2.0, 3.0))
rn2rw = SDTest.Call('NodeFX_New', 'isi', 'Roll', rn2rn)[0]
rn2rr = rn2r(rn2frame - 1, (0.0, 0.0, 0.0))
check('CNodeFX_Wheel_Apply roll', lambda: (rn2apply(rn2rw, rn2rn, rn2rr), rn2ints(rn2rw + 0x44, 3), rn2look(rn2rr)))
SDTest.Poke('g_FrameSec', rn2dt)
check('CNodeFX_Wheel_Type', lambda: SDTest.Call('CNodeFX_Wheel_Type', 'tbi', rn2w)[0])
check('NodeFX_Clone wheel', lambda: rn2wheel(SDTest.Call('NodeFX_Clone', 'ii', rn2w)[0])[1][:13])
check('CNodeFX_Wheel_Reset', lambda: (SDTest.Call('CNodeFX_Wheel_Reset', 'tii', rn2w)[1:], rn2ints(rn2w + 0x44, 3)))
rn2base = rn2new(4)
SDTest.Call('INodeFX_ctor@2b2c20', 'tvi', rn2base)
check('INodeFX', lambda: (rn2i(rn2base) == symva('INodeFX_vftable'), SDTest.Call('INodeFX_Type', 'tbi', rn2base)[0],
	SDTest.Call('NodeFX_Clone', 'ii', rn2base)[0], SDTest.Call('INodeFX_Delete', 'tiii', rn2base, 1)[0] == rn2base))
check('CNodeFX_Wheel_Delete', lambda: SDTest.Call('CNodeFX_Wheel_Delete', 'tiii', rn2w, 1)[0] == rn2w)

# the materials and overlays of the model effects made again
def rn2mat(m):
	return (rn2i(m + 0x30), rn2i(m + 0x34), rn2i(m + 0x38), rn2i(m + 0x3c) & 0xffff, rn2i(m + 0x40),
		SDTest.Call('CMaterial_GetMapName', 'tsii', m, 0)[0], SDTest.Call('CMaterial_GetMapName', 'tsii', m, 3)[0], SDTest.Call('CMaterial_GetMapName', 'tsii', m, 4)[0])
check('ModelFX_InitD3D', lambda: (SDTest.Call('ModelFX_InitD3D', 'v'), rn2mat(SDTest.Peek('g_MatElectric', 'i')),
	rn2mat(SDTest.Peek('g_MatFire', 'i')), rn2mat(SDTest.Peek('g_MatNoise', 'i')), rn2i(SDTest.Peek('g_MatNoise', 'i') + 0x9c) != 0,
	SDTest.Peek(sym('g_FXGraphicStatic') + 0x20, 'i'), SDTest.Peek(sym('g_FXGraphicNoise1') + 0x20, 'i'), SDTest.Peek(sym('g_FXGraphicNoise2') + 0x20, 'i')))
