# engine3d Engine/Render.c (the decompiled part of render.cpp): the frustum tests on planes set here, the pixel shader
# switches, the dynamic buffers and the clears through the D3D log. Python 1.5.2, runs inside sdcall.py after the other
# checks: check, fmt come from there.
def rdi(a):
	return SDTest.Peek(a, '@i')
def rdlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())

# the frustum: six planes (normal, d), a box of -10..10 in x and y and 1..100 in z
rdsaved = []
for i in range(24):
	rdsaved.append(SDTest.Peek(sym('g_Frustum') + 4 * i, 'i'))
SDTest.Call('crt_memcpy', 'iiFi', sym('g_Frustum') + 0x400000, (-1.0, 0.0, 0.0, -10.0, 1.0, 0.0, 0.0, -10.0, 0.0, -1.0, 0.0, -10.0,
	0.0, 1.0, 0.0, -10.0, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, -100.0), 96)
pts = ((0.0, 0.0, 50.0), (20.0, 0.0, 50.0), (-20.0, 0.0, 50.0), (0.0, 20.0, 50.0), (0.0, 0.0, 0.5), (0.0, 0.0, 200.0),
	(9.99, -9.99, 1.01), (30.0, 30.0, 300.0), (10.0, 10.0, 100.0))
for p in pts:
	for skip in (0, 1, 2, 0x3f, 0x15):
		check('Frustum_PlaneOutside' + fmt((p, skip)), lambda p=p, skip=skip: SDTest.Call('Frustum_PlaneOutside', 'iFi', p, skip)[0])
for a in pts:
	for b in pts:
		check('Frustum_TestSeg' + fmt((a, b)), lambda a=a, b=b: SDTest.Call('Frustum_TestSeg', 'iFF', a, b)[0])
cs = []
for p in pts:
	cs = cs + list(p)
rads = (0.0, 5.0, 15.0, 1.0, 0.25, 150.0, 0.0, 400.0, -1.0)
sphout = SDTest.Call('operator_new', 'ii', 4 * len(pts))[0]
def rdspheres():
	SDTest.Call('Frustum_TestSpheres', 'vFFii', tuple(cs), rads, len(pts), sphout)
	r = []
	for i in range(len(pts)):
		r.append(rdi(sphout + 4 * i))
	return r
check('Frustum_TestSpheres', rdspheres)
for i in range(24):
	SDTest.Poke(sym('g_Frustum') + 4 * i, rdsaved[i])

# the pixel shaders as the device has them, then all off by the mask
rdps = SDTest.Peek('g_PSOff', 'i')
# the nine shader functions, one after the other in the original (0x60 apart)
rdpsfn = ('Render_PS0', 'Render_PS1', 'Render_PS2', 'Render_PS3', 'Render_PS4', 'Render_PS5', 'Render_PS6',
	'Render_PS7', 'Render_PS8')
for off in (rdps, -1):
	SDTest.Poke('g_PSOff', off)
	for i in range(9):
		for flags in (1, 2, 3, 0):
			check('Render_PS' + fmt((i, off, flags)), lambda i=i, flags=flags: rdlog(lambda i=i, flags=flags:
				SDTest.Call(rdpsfn[i], 'vii', 0, flags)))
SDTest.Poke('g_PSOff', rdps)
for flags in (1, 2, 3):
	check('Render_PSFixed' + fmt((flags,)), lambda flags=flags: rdlog(lambda flags=flags: SDTest.Call('Render_PSFixed', 'vii', 0, flags)))

# the buffers: made, filled, their counts
ib = SDTest.Call('operator_new', 'ii', 0xc)[0]
vb = SDTest.Call('operator_new', 'ii', 0x14)[0]
for b, n in ((ib, 3), (vb, 5)):
	SDTest.Call('crt_memcpy', 'iiFi', b, (0.0,) * n, 4 * n)
check('CRenderIB_Reset', lambda: (SDTest.Call('CRenderIB_Reset', 'tvi', ib), rdi(ib + 4), rdi(ib + 8)))
check('CRenderVB_Reset', lambda: (SDTest.Call('CRenderVB_Reset', 'tvi', vb), rdi(vb + 4), rdi(vb + 8), rdi(vb + 0xc), rdi(vb + 0x10)))
check('CRenderIB_Create', lambda: (rdlog(lambda: SDTest.Call('CRenderIB_Create', 'tvii', ib, 100)), rdi(ib) != 0, rdi(ib + 4), rdi(ib + 8)))
check('CRenderVB_Create', lambda: (rdlog(lambda: SDTest.Call('CRenderVB_Create', 'tviiii', vb, 50, 24, 0x152)), rdi(vb) != 0,
	rdi(vb + 4), rdi(vb + 8), rdi(vb + 0xc), rdi(vb + 0x10)))
data = SDTest.Call('operator_new', 'ii', 24 * 8)[0]
SDTest.Call('crt_memcpy', 'iiFi', data, (1.0,) * 48, 48 * 4)
check('CRenderIB_Add', lambda: (SDTest.Call('CRenderIB_Add', 'tviii', ib, 4, data), rdi(ib + 8), SDTest.Call('CRenderIB_Add', 'tviii', ib, 10,
	data), rdi(ib + 8)))
check('CRenderVB_Add', lambda: (SDTest.Call('CRenderVB_Add', 'tviii', vb, 3, data), rdi(vb + 8), SDTest.Call('CRenderVB_Add', 'tviii', vb, 5,
	data), rdi(vb + 8)))
check('CRenderIB_Get', lambda: (SDTest.Call('CRenderIB_Get', 'tiip', ib, 0)[0] == rdi(ib), SDTest.Call('CRenderIB_Get', 'tiip', ib, 0)[1]))
check('CRenderVB_Get', lambda: (SDTest.Call('CRenderVB_Get', 'tiip', vb, 0)[0] == rdi(vb), SDTest.Call('CRenderVB_Get', 'tiip', vb, 0)[1]))
# the clears (the viewport is 0 between frames: the batches are not drawn first)
rect = SDTest.Call('operator_new', 'ii', 16)[0]
for i, v in ((0, 10), (1, 20), (2, 200), (3, 120)):
	SDTest.Poke(rect + 4 * i - 0x400000, v)
for r, flags in ((0, 1), (0, 2), (0, 3), (rect, 2), (rect, 1)):
	check('Render_Clear' + fmt((r != 0, flags)), lambda r=r, flags=flags: rdlog(lambda r=r, flags=flags:
		SDTest.Call('Render_Clear', 'biiifi', r, flags, 0x102030, 1.0, 0)))
rdvp = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
check('Render_GetViewportSize', lambda: SDTest.Call('Render_GetViewportSize', 'vpp', 0, 0))
SDTest.Poke('g_ViewportW', rdvp[0])
SDTest.Poke('g_ViewportH', rdvp[1])

# the texture stage effects of a material made here, its reflection map (2) and bump map (3) set up by hand; the time
# of the engine fixed for each call (the effects move with it), srand before the jitter
def rdpokef(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
rmat = SDTest.Call('CMaterial_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0xb0)[0])[0]
for i in (0, 2, 3):
	SDTest.Call('CMaterial_SetMap', 'tviis', rmat, i, 'bmp/partic1.bmp')
for i in (2, 3):
	mp = rdi(rmat + 0x9c + 4 * i)
	rdpokef(mp + 0x10, (0.25, -0.5, 1.5, 0.75, 0.0, 0.0, 0.3, 2.0))
	SDTest.Poke(mp + 0x54 - 0x400000, 0x100)
rdtime = SDTest.Peek('g_TimeMs', 'i')
rdhw = SDTest.Peek('g_UseHWEnvMap', 'i')
rdview = SDTest.Peek('g_EnvMapViewDep', 'i')
SDTest.Poke('g_UseHWEnvMap', 1)
def rdtimed(t, fn):
	SDTest.Poke('g_TimeMs', t)
	return rdlog(fn)
for t in (0, 12345, 987654321):
	for view in (0, 1, 2):
		SDTest.Poke('g_EnvMapViewDep', view)
		for flags in (1, 2, 4, 7):
			check('Mat_StageEnvBump' + fmt((t, view, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
				SDTest.Call('Mat_StageEnvBump', 'viiii', rmat, flags, 1, 0)))
	for flags in (1, 2, 5):
		check('Mat_StageScroll' + fmt((t, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
			SDTest.Call('Mat_StageScroll', 'viiiif', rmat, 2, flags, 1, 0.5)))
		check('Tex_StageTransform' + fmt((t, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
			SDTest.Call('Tex_StageTransform', 'viiiffff', 0, flags, 2, 0.3, -0.7, 2.0, 1.25)))
		check('Mat_StageBumpMat' + fmt((t, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
			SDTest.Call('Mat_StageBumpMat', 'viiifff', rmat, flags, 0, 2.0, 0.5, 0.75)))
SDTest.Poke('g_EnvMapViewDep', rdview)
SDTest.Poke('g_UseHWEnvMap', rdhw)
SDTest.Poke('g_TimeMs', rdtime)
check('Mat_StageScroll still', lambda: rdlog(lambda: SDTest.Call('Mat_StageScroll', 'viiiif', rmat, 0, 5, 0, 1.0)))
SDTest.Call('crt_srand', 'vi', 77)
for flags in (1, 2, 3):
	check('Tex_StageJitter' + fmt((flags,)), lambda flags=flags: rdlog(lambda flags=flags: SDTest.Call('Tex_StageJitter', 'viiifff', 0,
		flags, 1, 0.25, 0.5, 1.5)))
# the pixel shader alpha and the D3D material with and without DiffuseMod4x and a forced material
rdmod = SDTest.Peek('g_DiffuseMod4x', 'i')
cols = (0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 20.0)
for transp, atrib in ((0, 0), (1, 0), (1, 0x1000)):
	SDTest.Poke(rmat + 0x38 - 0x400000, transp << 16)
	SDTest.Poke(rmat + 0x3c - 0x400000, atrib)
	for forced in (0, rmat):
		SDTest.Poke('g_MatForced', forced)
		check('Render_PSConstAlpha' + fmt((transp, atrib, forced != 0)), lambda: rdlog(lambda: SDTest.Call('Render_PSConstAlpha', 'vii',
			rmat, 2)))
		for mod in (0, 1):
			SDTest.Poke('g_DiffuseMod4x', mod)
			check('Render_D3DMaterial' + fmt((transp, forced != 0, mod)), lambda: rdlog(lambda: SDTest.Call('Render_D3DMaterial', 'viF',
				rmat, cols)))
SDTest.Poke('g_MatForced', 0)
SDTest.Poke('g_DiffuseMod4x', rdmod)
check('Render_Get', lambda: (SDTest.Call('Render_GetProj', 'i')[0], SDTest.Call('Render_GetView', 'i')[0], SDTest.Call('Render_GetBillboard', 'i')[0],
	SDTest.Call('Render_GetEyePos', 'i')[0]))

# the world matrix: given to the device or only remembered, read back
wm = (1.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 4.0, 5.0, 6.0, 1.0)
rdworld = []
for i in range(16):
	rdworld.append(SDTest.Peek(sym('g_MatWorld') + 4 * i, 'i'))
def rdw():
	r = [SDTest.Peek('g_WorldSet', 'i')]
	for i in range(16):
		r.append(SDTest.Peek(sym('g_MatWorld') + 4 * i, 'i'))
	return r
check('Render_SetWorldMatrix defer', lambda: (rdlog(lambda: SDTest.Call('Render_SetWorldMatrix', 'iFi', wm, 1)), rdw()))
check('Render_SetWorld', lambda: (rdlog(lambda: SDTest.Call('Render_SetWorld', 'i')), rdw(), rdlog(lambda: SDTest.Call('Render_SetWorld', 'i'))))
check('Render_WorldDirty', lambda: (SDTest.Call('Render_WorldDirty', 'v'), rdw()))
check('Render_SetWorldMatrix', lambda: (rdlog(lambda: SDTest.Call('Render_SetWorldMatrix', 'iFi', wm[4:] + wm[:4], 0)), rdw()))
check('Render_SetWorldD3D', lambda: (rdlog(lambda: SDTest.Call('Render_SetWorldD3D', 'iF', wm)), rdw()))
check('Render_GetWorld', lambda: (SDTest.Call('Render_GetWorld', 'iF', (0.0,) * 16), SDTest.Call('Render_GetWorld', 'ii', 0)))
for i in range(16):
	SDTest.Poke(sym('g_MatWorld') + 4 * i, rdworld[i])
check('Render_GetDriver', lambda: SDTest.Call('Render_GetDriver', 'i')[0] == sym('g_DriverStr') + 0x400000)
# the state RenderIniD3D left: the device texts and the logged device, the 1/2048 matrix, the texture caps, the
# shaders made, the default materials
check('RenderIniD3D texts', lambda: (devname('R_LastDevice', SDTest.Peek('g_DriverStr', 's')), SDTest.Peek('g_DeviceStr', 's'),
	devname('R_LastDevice', SDTest.Peek('g_DriverName', 'p')),
	SDTest.Peek('g_VersionStr', 's'), SDTest.Peek('g_DeviceLogged', 'i')))
def rdinistate():
	r = []
	for i in range(16):
		r.append(SDTest.Peek(sym('g_MatBias') + 4 * i, 'i'))
	for a in (sym('g_MipmapsOn'), sym('g_TexRepeat'), sym('g_TexNoPow2'), sym('g_TexPow2'), sym('g_UseShaders2'), sym('g_UseShaders'), sym('g_D3DOn')):
		r.append(SDTest.Peek(a, 'b'))
	for a in (sym('g_PSOn'), sym('g_EnvBump'), sym('g_EnvMap'), sym('g_FogEnable')):
		r.append(SDTest.Peek(a, 'i'))
	for i in range(30):
		r.append(SDTest.Peek(sym('g_PSHandles') + 4 * i, 'i') != 0)
	for a in (sym('g_MatWhite'), sym('g_MatCheckerboard'), sym('g_MatGray'), sym('g_MatInvis'), sym('g_MatBump'),
			sym('g_MatGround')):
		m = SDTest.Peek(a, 'i')
		r.append(m and (SDTest.Peek(m + 4, '@p'), SDTest.Peek(m + 0x39, '@b'), SDTest.Peek(m + 0x40, '@i')))
	return r
check('RenderIniD3D state', rdinistate)
rect2 = SDTest.Call('operator_new', 'ii', 16)[0]
for i, v in ((0, -5), (1, 300), (2, 50), (3, 700)):
	SDTest.Poke(rect2 + 4 * i - 0x400000, v)
for r, c1, c2 in ((rect, 0xff0000, 0xff), (rect2, 0x808080, 0x10203040)):
	check('Render_DrawTexRect' + fmt((r != 0, c1, c2)), lambda r=r, c1=c1, c2=c2: rdlog(lambda r=r, c1=c1, c2=c2:
		SDTest.Call('Render_DrawTexRect', 'viiii', r, c1, c2, 0)))
check('Render_ClearForced', lambda: (SDTest.Call('Render_ClearForced', 'v'), SDTest.Peek('g_MatForced', 'i'), SDTest.Peek('g_MatCurrent', 'i')))
# a frame begun and its view ended again (the viewport is 0 between frames)
check('Render_BeginFrame', lambda: (rdlog(lambda: SDTest.Call('Render_BeginFrame', 'v')), SDTest.Peek('g_ViewportW', 'i') != 0,
	SDTest.Peek('g_PShaderCount', 'i')))
check('Render_EndView', lambda: (rdlog(lambda: SDTest.Call('Render_EndView', 'v')), SDTest.Peek('g_ViewportW', 'i')))

# Render_SetMaterial with the material of the stage tests: blending, alpha test, lighting, the second texture set,
# a forced material and the default material and texture modes (the last material forgotten before each)
check('Render_ResetState', lambda: rdlog(lambda: SDTest.Call('Render_ResetState', 'v')))
rdsm = []
for a in ('g_UseShaders', 'g_UseDefMaterial', 'g_UseDefTexture', 'g_MatForced', 'g_MatFX1', 'g_AlphaFog'):
	rdsm.append((a, SDTest.PeekSpan(a, 0, 4)))
def rdset(pass_):
	SDTest.Call('Render_SetMaterial', 'iii', 0, -1)
	return rdlog(lambda pass_=pass_: SDTest.Call('Render_SetMaterial', 'iii', rmat, pass_))
rdsaved38 = (rdi(rmat + 0x30), rdi(rmat + 0x34), rdi(rmat + 0x38), rdi(rmat + 0x3c), rdi(rmat + 0x40))
# without the pixel shader modes (the mode of this material fails a CHECK with a texture)
SDTest.PokeSpan('g_UseShaders', 0, SDTest.PeekSpan('g_UseShaders', 0, 4) & ~0xff, 4)
for src, dst in ((5, 6), (2, 2), (2, 1)):
	for b38 in (0, 0x0100, 0x010000, 0x01000000, 0x01010100):
		for atrib in (0, 0x20, 0x400, 0x800, 0x40, 0x1000020):
			SDTest.Poke(rmat + 0x30 - 0x400000, src)
			SDTest.Poke(rmat + 0x34 - 0x400000, dst)
			SDTest.Poke(rmat + 0x38 - 0x400000, b38)
			SDTest.Poke(rmat + 0x3c - 0x400000, atrib)
			for pass_ in (0, 1):
				check('Render_SetMaterial' + fmt((src, dst, b38, atrib, pass_)), lambda pass_=pass_: rdset(pass_))
for fog in (0, 1, 2):
	SDTest.Poke('g_AlphaFog', fog)
	check('Render_SetMaterial fog' + fmt((fog,)), lambda: (rdset(0), rdset(1)))
SDTest.Poke('g_MatForced', rmat)
check('Render_SetMaterial forced', lambda: rdset(0))
SDTest.Poke('g_MatForced', 0)
for usedef in (1, 2):
	SDTest.Poke('g_UseDefMaterial', usedef)
	check('Render_SetMaterial default material' + fmt((usedef,)), lambda: rdset(0))
SDTest.Poke('g_UseDefMaterial', 0)
for tex in (1, 2, 3, 4):
	SDTest.Poke('g_UseDefTexture', tex)
	check('Render_SetMaterial default texture' + fmt((tex,)), lambda: rdset(0))
check('Render_SetMaterial same', lambda: (rdset(0), rdlog(lambda: SDTest.Call('Render_SetMaterial', 'iii', rmat, 0))))
check('Render_SetMaterial none', lambda: (SDTest.Call('Render_SetMaterial', 'iii', 0, -1), SDTest.Peek('g_MatCurrent', 'i')))
for a, v in rdsm:
	SDTest.PokeSpan(a, 0, v, 4)
# the pixel shader modes with a material without textures (the shader of its mode is switched as it changes)
rmat2 = SDTest.Call('CMaterial_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0xb0)[0])[0]
SDTest.PokeSpan('g_UseShaders', 0, SDTest.PeekSpan('g_UseShaders', 0, 4) | 1, 4)
for transp in (0, 1):
	SDTest.Poke(rmat2 + 0x38 - 0x400000, transp << 16)
	check('Render_SetMaterial shaders' + fmt((transp,)), lambda: (rdlog(lambda: SDTest.Call('Render_SetMaterial', 'iii', 0, -1)),
		rdlog(lambda: SDTest.Call('Render_SetMaterial', 'iii', rmat2, 0)), rdlog(lambda: SDTest.Call('Render_SetMaterial', 'iii', rmat2, 0)),
		SDTest.Peek('g_ShaderCurrent', 'i')))
for a, v in rdsm:
	SDTest.PokeSpan(a, 0, v, 4)
for i in range(5):
	SDTest.Poke(rmat + 0x30 + 4 * i - 0x400000, rdsaved38[i])

# the tiles of the projection, the projection, the cull, the lightmap material, the maps on the stages, the buffer
# draws and the view (projection, view and the tile state put back after)
def rdints(a, n):
	r = []
	for i in range(n):
		r.append(SDTest.Peek(a + 4 * i, 'i'))
	return r
def rdheap(a, n):
	r = []
	for i in range(n):
		r.append(rdi(a + 4 * i))
	return r
rdproj = rdints(sym('g_MatProj'), 16)
rdview = rdints(sym('g_MatView'), 16)
rdtiles = (SDTest.Peek('g_Tiles', 'i'), SDTest.Peek('g_TileState', 'i'), SDTest.Peek('g_TileX', 'i'), SDTest.Peek('g_TileY', 'i'),
	SDTest.PeekSpan('g_TileOn', 0, 4))
pm = (1.5, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -0.1, 0.0)
SDTest.Poke('g_Tiles', 2)
for step in range(7):
	on = (0, 1, 0, 0, 1, 0, 0)[step]
	SDTest.PokeSpan('g_TileOn', 0, (rdtiles[4] & ~0xff) | on, 4)
	check('Render_ProjTile' + fmt((step,)), lambda: (rdlog(lambda: SDTest.Call('Render_ProjTile', 'bF', pm)), SDTest.Peek('g_TileState', 'i'),
		SDTest.Peek('g_TileX', 'i'), SDTest.Peek('g_TileY', 'i'), SDTest.Peek('g_TileOn', 'b'), rdints(sym('g_MatProj'), 16)))
for a, v in ((sym('g_Tiles'), rdtiles[0]), (sym('g_TileState'), rdtiles[1]), (sym('g_TileX'), rdtiles[2]), (sym('g_TileY'), rdtiles[3])):
	SDTest.Poke(a, v)
SDTest.PokeSpan('g_TileOn', 0, rdtiles[4], 4)
check('Render_SetProj', lambda: (rdlog(lambda: SDTest.Call('Render_SetProj', 'iF', pm)), rdints(sym('g_MatProj'), 16)))
rdcull = SDTest.Peek('g_CullMode', 'i')
for cm in (0, 1):
	SDTest.Poke('g_CullMode', cm)
	for mode in (1, 2, 3):
		check('Render_SetCull' + fmt((cm, mode)), lambda mode=mode: rdlog(lambda mode=mode: SDTest.Call('Render_SetCull', 'vi', mode)))
SDTest.Poke('g_CullMode', rdcull)
for mode in (0, 2, 4):
	check('Render_SetLightmapMat' + fmt((mode,)), lambda mode=mode: (SDTest.Call('Render_SetLightmapMat', 'vii', rmat, mode),
		SDTest.Peek('g_LightmapOp', 'i'), rdi(rmat + 8), SDTest.Call('Render_SetLightmapMat', 'vii', 0, 0), rdi(rmat + 8)))
mp = rdi(rmat + 0x9c)
rdmp = (rdi(mp + 8), rdi(mp + 0xc))
for b8, bc, lm in ((0, 0, 0), (1, 3, 0), (1, 1, 1), (0, 2, 0), (1, 0, 2)):
	SDTest.Poke(mp + 8 - 0x400000, (rdmp[0] & ~0xff00ff) | b8 | lm << 8)
	SDTest.Poke(mp + 0xc - 0x400000, (rdmp[1] & ~0xff) | bc)
	for stage in (0, 3):
		check('Render_SetMap' + fmt((b8, bc, lm, stage)), lambda stage=stage: rdlog(lambda stage=stage:
			SDTest.Call('Render_SetMap', 'iii', stage, mp)))
SDTest.Poke(mp + 8 - 0x400000, rdmp[0])
SDTest.Poke(mp + 0xc - 0x400000, rdmp[1])
check('Render_SetMap none', lambda: (rdlog(lambda: SDTest.Call('Render_SetMap', 'iii', 1, 0)), rdlog(lambda: SDTest.Call('Render_SetMap',
	'iii', 1, 0))))
check('Render_DrawVBIBFmt', lambda: rdlog(lambda: SDTest.Call('Render_DrawVBIBFmt', 'iiiiiiii', rdi(vb), rdi(ib), 2, 50, 10, 3, 4)))
lv = SDTest.Call('operator_new', 'ii', 0x14)[0]
SDTest.Poke(lv - 0x400000, 0)
SDTest.Poke(lv + 4 - 0x400000, 40)
SDTest.Poke(lv + 0xc - 0x400000, 2)
check('Render_DrawVBIB', lambda: rdlog(lambda: SDTest.Call('Render_DrawVBIB', 'iiiii', rdi(vb), rdi(ib), lv, 12)))
md = SDTest.Call('operator_new', 'ii', 0x100)[0]
SDTest.Call('crt_memcpy', 'iiFi', md, (0.0,) * 64, 0x100)
for ntri, strip, base, start, lim in ((20, 0, 2, 5, -1), (20, 0, 0, 0, 7), (0, 10, 0, 0, -1)):
	SDTest.Poke(md + 0xc - 0x400000, ntri)
	SDTest.Poke(md + 0x18 - 0x400000, 33)
	SDTest.Poke(md + 0x9c - 0x400000, base)
	SDTest.Poke(md + 0xa0 - 0x400000, start)
	SDTest.Poke(md + 0xa4 - 0x400000, strip)
	SDTest.Poke(md + 0xa8 - 0x400000, 2)
	SDTest.Poke(md + 0xac - 0x400000, rdi(vb))
	SDTest.Poke(md + 0xb0 - 0x400000, (strip == 0) * rdi(ib))
	rdlim = SDTest.Peek('g_VBIBTriLimit', 'i')
	SDTest.Poke('g_VBIBTriLimit', lim)
	check('Render_DrawMeshD3D' + fmt((ntri, strip, base, start, lim)), lambda: rdlog(lambda: SDTest.Call('Render_DrawMeshD3D', 'ii', md)))
	SDTest.Poke('g_VBIBTriLimit', rdlim)
vm = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 5.0, -3.0, 2.0, 1.0)
check('Render_SetView', lambda: (rdlog(lambda: SDTest.Call('Render_SetView', 'iF', vm)), rdints(sym('g_MatBillboard'), 16), rdints(sym('g_EyePos'), 3)))
def rdfl(v):
	r = SDTest.Call('operator_new', 'ii', 64)[0]
	for i in range(16):
		SDTest.Poke(r + 4 * i - 0x400000, v[i])
	return r
SDTest.Call('Render_SetProj', 'ii', rdfl(rdproj))
SDTest.Call('Render_SetView', 'ii', rdfl(rdview))

# chunk F: the visibility tests on the planes of the frustum tests, the rectangles, the buffers made from data, the
# camera, the version string; frustum, view, projection and viewport put back after
rdplanes = rdints(sym('g_Frustum'), 24)
SDTest.Call('crt_memcpy', 'iiFi', sym('g_Frustum') + 0x400000, (-1.0, 0.0, 0.0, -10.0, 1.0, 0.0, 0.0, -10.0, 0.0, -1.0, 0.0, -10.0,
	0.0, 1.0, 0.0, -10.0, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, -100.0), 96)
mid = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 50.0, 1.0)
far = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 300.0, 50.0, 1.0)
for m in (mid, far):
	for box in ((-1.0, -1.0, -1.0, 1.0, 1.0, 1.0), (-200.0, -2.0, -2.0, 200.0, 2.0, 2.0)):
		for scale in (1.0, 0.5):
			check('Box_IsVisible' + fmt((m[13], box[0], scale)), lambda m=m, box=box, scale=scale: SDTest.Call('Box_IsVisible', 'bFFsf',
				m, box, 'sd', scale)[0])
for c, r in (((0.0, 0.0, 50.0), 1.0), ((0.0, 30.0, 50.0), 5.0), ((0.0, 30.0, 50.0), 25.0)):
	check('Sph_IsVisible' + fmt((c, r)), lambda c=c, r=r: SDTest.Call('Sph_IsVisible', 'bFfi', c, r, 0)[0])
check('Frustum_SegVisible', lambda: (SDTest.Call('Frustum_SegVisible', 'bFF', (0.0, 0.0, 50.0), (0.0, 30.0, 50.0))[0],
	SDTest.Call('Frustum_SegVisible', 'bFF', (20.0, 0.0, 50.0), (30.0, 0.0, 50.0))[0]))
for i in range(24):
	SDTest.Poke(sym('g_Frustum') + 4 * i, rdplanes[i])
vp = SDTest.Call('operator_new', 'ii', 24)[0]
for i, v in ((0, 10), (1, 20), (2, 640), (3, 480), (4, 0), (5, 0x3f800000)):
	SDTest.Poke(vp + 4 * i - 0x400000, v)
for box in ((0.0, 0.0, 0.0, 1.0, 1.0, 0.0), (0.25, 0.1, 0.0, 0.5, 0.75, 0.0), (-0.5, 0.5, 0.0, 2.0, 0.4, 0.0),
	(0.7, 0.7, 0.0, 0.2, 0.9, 0.0), (0.33333, 0.66666, 0.0, 0.4999, 0.9999, 0.0)):
	check('Render_ClipRect' + fmt(box), lambda box=box: SDTest.Call('Render_ClipRect', 'biiF', SDTest.Call('operator_new', 'ii', 16)[0], vp,
		box)[0:1])
	out16 = SDTest.Call('operator_new', 'ii', 16)[0]
	check('Render_ClipRect out' + fmt(box), lambda box=box, out16=out16: (SDTest.Call('Render_ClipRect', 'biiF', out16, vp, box),
		rdheap(out16, 4)))
rdvp2 = rdints(sym('g_Viewport'), 6)
SDTest.Call('crt_memcpy', 'iiii', sym('g_Viewport') + 0x400000, vp, 24)
for box, proj in (((0.25, 0.1, 0.0, 0.5, 0.75, 0.0), 1), ((0.0, 0.0, 0.0, 1.0, 1.0, 0.0), 0)):
	check('Render_SetViewRect' + fmt((box, proj)), lambda box=box, proj=proj: rdlog(lambda box=box, proj=proj:
		SDTest.Call('Render_SetViewRect', 'bFi', box, proj)))
check('Render_SetViewRect none', lambda: rdlog(lambda: SDTest.Call('Render_SetViewRect', 'bii', 0, 0)))
check('Render_GetViewport', lambda: (SDTest.Call('Render_GetViewportW', 'i')[0], SDTest.Call('Render_GetViewportH', 'i')[0]))
for i in range(6):
	SDTest.Poke(sym('g_Viewport') + 4 * i, rdvp2[i])
check('Render_StencilBits', lambda: SDTest.Call('Render_StencilBits', 'i')[0])
for v in (0x0101, 0x0800, 0xffff, 0x12345):
	check('Render_VersionStr' + fmt((v,)), lambda v=v: SDTest.Call('Render_VersionStr', 'si', v)[0])
check('Render_NoIndexedBlend', lambda: rdlog(lambda: SDTest.Call('Render_NoIndexedBlend', 'vi', SDTest.Peek('g_pd3dDevice', 'i'))))
tris = SDTest.Call('operator_new', 'ii', 36)[0]
for i in range(9):
	SDTest.Poke(tris + 4 * i - 0x400000, i * 0x10001)
cell = SDTest.Call('operator_new', 'ii', 4)[0]
for dyn in (0, 1):
	check('Render_CreateIB' + fmt((dyn,)), lambda dyn=dyn: (rdlog(lambda dyn=dyn: SDTest.Call('Render_CreateIB', 'iiiii', cell, tris, 6,
		dyn)), rdi(cell) != 0))
	SDTest.Poke(lv - 0x400000, data)
	SDTest.Poke(lv + 4 - 0x400000, 6)
	check('Render_CreateVB' + fmt((dyn,)), lambda dyn=dyn: (rdlog(lambda dyn=dyn: SDTest.Call('Render_CreateVB', 'iiii', cell, lv, dyn)),
		rdi(cell) != 0))
cam = SDTest.Call('CCamera_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0x78)[0])[0]
check('Render_SetCamera', lambda: (rdlog(lambda: SDTest.Call('Render_SetCamera', 'vi', cam)), rdints(sym('g_MatProj'), 16), rdints(sym('g_MatView'), 16),
	rdints(sym('g_Frustum'), 24)))
for i in range(24):
	SDTest.Poke(sym('g_Frustum') + 4 * i, rdplanes[i])
SDTest.Call('Render_SetProj', 'ii', rdfl(rdproj))
SDTest.Call('Render_SetView', 'ii', rdfl(rdview))

# Render_ToScreen with a view and projection set here and the test viewport
SDTest.Call('Render_SetView', 'iF', (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
SDTest.Call('Render_SetProj', 'iF', (1.0, 0.0, 0.0, 0.0, 0.0, 1.3333, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -1.001, 0.0))
SDTest.Call('crt_memcpy', 'iiii', sym('g_Viewport') + 0x400000, vp, 24)
for p in ((0.0, 0.0, 10.0), (3.0, -2.0, 10.0), (20.0, 0.0, 10.0), (0.0, 0.0, 0.5), (0.0, 0.0, -10.0), (-9.99, 7.4, 10.0),
	(0.0, 0.0, 2000.0)):
	check('Render_ToScreen' + fmt(p), lambda p=p: SDTest.Call('Render_ToScreen', 'iFF', p, (-1.0, -1.0)))
for i in range(6):
	SDTest.Poke(sym('g_Viewport') + 4 * i, rdvp2[i])
SDTest.Call('Render_SetProj', 'ii', rdfl(rdproj))
SDTest.Call('Render_SetView', 'ii', rdfl(rdview))

# Frustum_Calc from views and projections given here (one without an inverse); the planes put back after
idm = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
for v, p in ((idm, pm), (vm, pm), ((0.8, 0.0, -0.6, 0.0, 0.0, 1.0, 0.0, 0.0, 0.6, 0.0, 0.8, 0.0, 10.0, -5.0, 30.0, 1.0),
	(1.2, 0.0, 0.0, 0.0, 0.0, 1.6, 0.0, 0.0, 0.0, 0.0, 1.0001, 1.0, 0.0, 0.0, -0.50005, 0.0)), (idm, (0.0,) * 16)):
	check('Frustum_Calc' + fmt((v[12], p[0])), lambda v=v, p=p: (SDTest.Call('Frustum_Calc', 'vFF', v, p), rdints(sym('g_Frustum'), 24)))
for i in range(24):
	SDTest.Poke(sym('g_Frustum') + 4 * i, rdplanes[i])

# Render_MeshD3D with the mesh of the buffer draws and the material without textures, no render node
SDTest.Poke(md + 0xc - 0x400000, 20)
SDTest.Poke(md + 0x18 - 0x400000, 33)
SDTest.Poke(md + 0x9c - 0x400000, 0)
SDTest.Poke(md + 0xa0 - 0x400000, 0)
SDTest.Poke(md + 0xa4 - 0x400000, 0)
SDTest.Poke(md + 0xb0 - 0x400000, rdi(ib))
SDTest.Poke(md + 0x4c - 0x400000, rmat2)
rdvb = SDTest.PeekSpan('g_UseVB', 0, 4)
rdhw = SDTest.Peek('g_UseHWEnvMap', 'i')
SDTest.Poke('g_UseHWEnvMap', 1)
rdtris = SDTest.Peek('g_RenderTris', 'i')
for b3b in (0, 0x01000000):
	SDTest.Poke(rmat2 + 0x38 - 0x400000, b3b)
	check('Render_MeshD3D' + fmt((b3b != 0,)), lambda: (SDTest.Call('Render_SetMaterial', 'iii', 0, -1), rdlog(lambda: SDTest.Call('Render_MeshD3D',
		'iii', md, 0)), SDTest.Peek('g_RenderTris', 'i') - rdtris))
	rdtris = SDTest.Peek('g_RenderTris', 'i')
SDTest.Poke(rmat2 + 0x38 - 0x400000, 0)
SDTest.Poke('g_UseHWEnvMap', rdhw)

# the render functions of the shader modes of both tables through the D3D log: with and without the pixel shaders,
# both R_EnvBlend and DiffuseMod4x, a material with all maps, one with maps 0 and 4 for Scroll (it must have no reflection),
# grey as the lightmap; time and rand fixed for each call
rdsh = (SDTest.Peek('g_PSOff', 'i'), SDTest.Peek('g_EnvBlend', 'i'), SDTest.Peek('g_DiffuseMod4x', 'i'), SDTest.Peek('g_LightmapMat', 'i'),
	SDTest.Peek('g_TimeMs', 'i'), rdi(rmat + 0x38))
SDTest.Call('CMaterial_SetMap', 'tviis', rmat, 4, 'bmp/partic1.bmp')
rmat3 = SDTest.Call('CMaterial_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0xb0)[0])[0]
for i in (0, 4):
	SDTest.Call('CMaterial_SetMap', 'tviis', rmat3, i, 'bmp/partic1.bmp')
SDTest.Poke(rmat + 0x38 - 0x400000, 1 << 16)
SDTest.Poke('g_LightmapMat', SDTest.Peek('g_MatGray', 'i'))
SDTest.Poke('g_TimeMs', 54321)
def rdshader(fn, mat, flags):
	SDTest.Call('crt_srand', 'vi', 77)
	return rdlog(lambda fn=fn, mat=mat, flags=flags: SDTest.Call(fn, 'vii', mat, flags))
for tab in (sym('g_ShaderTab'), sym('g_ShaderTab2')):
	for i in range(26):
		fn = SDTest.Peek(tab + 0x18 * i + 0x14, 'i')
		if not fn:
			continue
		mat = rmat
		if i == 14:
			mat = rmat3
		for off in (rdsh[0], -1):
			SDTest.Poke('g_PSOff', off)
			for blend in (0, 1):
				SDTest.Poke('g_EnvBlend', blend)
				for mod in (0, 1):
					SDTest.Poke('g_DiffuseMod4x', mod)
					for flags in (1, 4, 5, 2):
						check('Shader' + fmt((tab, i, off == -1, blend, mod, flags)), lambda fn=fn - 0x400000,
							mat=mat, flags=flags: rdshader(fn, mat, flags))
# the modes that move with the time and rand at other times, one past 2^31 ms, with the pixel shaders and colors
SDTest.Poke('g_PSOff', rdsh[0])
rdcol = (rdi(rmat + 0x14), rdi(rmat + 0x1c))
SDTest.Poke(rmat + 0x14 - 0x400000, 0x40ffc080)
SDTest.Poke(rmat + 0x1c - 0x400000, 0x00a00000)
for t in (0, 987654321, -16):
	SDTest.Poke('g_TimeMs', t)
	for tab, i in ((sym('g_ShaderTab'), 7), (sym('g_ShaderTab'), 8), (sym('g_ShaderTab'), 10), (sym('g_ShaderTab'), 11), (sym('g_ShaderTab'), 13), (sym('g_ShaderTab'), 15),
		(sym('g_ShaderTab2'), 9)):
		fn = SDTest.Peek(tab + 0x18 * i + 0x14, 'i') - 0x400000
		for flags in (1, 5):
			check('Shader time' + fmt((t, tab, i, flags)), lambda fn=fn, flags=flags: rdshader(fn, rmat, flags))
SDTest.Poke(rmat + 0x14 - 0x400000, rdcol[0])
SDTest.Poke(rmat + 0x1c - 0x400000, rdcol[1])
for a, v in ((sym('g_PSOff'), rdsh[0]), (sym('g_EnvBlend'), rdsh[1]), (sym('g_DiffuseMod4x'), rdsh[2]), (sym('g_LightmapMat'), rdsh[3]), (sym('g_TimeMs'), rdsh[4]),
	(rmat + 0x38 - 0x400000, rdsh[5])):
	SDTest.Poke(a, v)
