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
	rdsaved.append(SDTest.Peek(0x4adbf0 + 4 * i, 'i'))
SDTest.Call(0x3197d0, 'iiFi', 0x4adbf0 + 0x400000, (-1.0, 0.0, 0.0, -10.0, 1.0, 0.0, 0.0, -10.0, 0.0, -1.0, 0.0, -10.0,
	0.0, 1.0, 0.0, -10.0, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, -100.0), 96)
pts = ((0.0, 0.0, 50.0), (20.0, 0.0, 50.0), (-20.0, 0.0, 50.0), (0.0, 20.0, 50.0), (0.0, 0.0, 0.5), (0.0, 0.0, 200.0),
	(9.99, -9.99, 1.01), (30.0, 30.0, 300.0), (10.0, 10.0, 100.0))
for p in pts:
	for skip in (0, 1, 2, 0x3f, 0x15):
		check('Frustum_PlaneOutside' + fmt((p, skip)), lambda p=p, skip=skip: SDTest.Call(0x249670, 'iFi', p, skip)[0])
for a in pts:
	for b in pts:
		check('Frustum_TestSeg' + fmt((a, b)), lambda a=a, b=b: SDTest.Call(0x2494a0, 'iFF', a, b)[0])
cs = []
for p in pts:
	cs = cs + list(p)
rads = (0.0, 5.0, 15.0, 1.0, 0.25, 150.0, 0.0, 400.0, -1.0)
sphout = SDTest.Call(0x1334d0, 'ii', 4 * len(pts))[0]
def rdspheres():
	SDTest.Call(0x249830, 'vFFii', tuple(cs), rads, len(pts), sphout)
	r = []
	for i in range(len(pts)):
		r.append(rdi(sphout + 4 * i))
	return r
check('Frustum_TestSpheres', rdspheres)
for i in range(24):
	SDTest.Poke(0x4adbf0 + 4 * i, rdsaved[i])

# the pixel shaders as the device has them, then all off by the mask
rdps = SDTest.Peek(0x4ada98, 'i')
for off in (rdps, -1):
	SDTest.Poke(0x4ada98, off)
	for i in range(9):
		for flags in (1, 2, 3, 0):
			check('Render_PS' + fmt((i, off, flags)), lambda i=i, flags=flags: rdlog(lambda i=i, flags=flags:
				SDTest.Call(0x249120 + 0x60 * i, 'vii', 0, flags)))
SDTest.Poke(0x4ada98, rdps)
for flags in (1, 2, 3):
	check('Render_PSFixed' + fmt((flags,)), lambda flags=flags: rdlog(lambda flags=flags: SDTest.Call(0x243ab0, 'vii', 0, flags)))

# the buffers: made, filled, their counts
ib = SDTest.Call(0x1334d0, 'ii', 0xc)[0]
vb = SDTest.Call(0x1334d0, 'ii', 0x14)[0]
for b, n in ((ib, 3), (vb, 5)):
	SDTest.Call(0x3197d0, 'iiFi', b, (0.0,) * n, 4 * n)
check('CRenderIB_Reset', lambda: (SDTest.Call(0x249a00, 'tvi', ib), rdi(ib + 4), rdi(ib + 8)))
check('CRenderVB_Reset', lambda: (SDTest.Call(0x249a40, 'tvi', vb), rdi(vb + 4), rdi(vb + 8), rdi(vb + 0xc), rdi(vb + 0x10)))
check('CRenderIB_Create', lambda: (rdlog(lambda: SDTest.Call(0x249c90, 'tvii', ib, 100)), rdi(ib) != 0, rdi(ib + 4), rdi(ib + 8)))
check('CRenderVB_Create', lambda: (rdlog(lambda: SDTest.Call(0x249d30, 'tviiii', vb, 50, 24, 0x152)), rdi(vb) != 0,
	rdi(vb + 4), rdi(vb + 8), rdi(vb + 0xc), rdi(vb + 0x10)))
data = SDTest.Call(0x1334d0, 'ii', 24 * 8)[0]
SDTest.Call(0x3197d0, 'iiFi', data, (1.0,) * 48, 48 * 4)
check('CRenderIB_Add', lambda: (SDTest.Call(0x249a90, 'tviii', ib, 4, data), rdi(ib + 8), SDTest.Call(0x249a90, 'tviii', ib, 10,
	data), rdi(ib + 8)))
check('CRenderVB_Add', lambda: (SDTest.Call(0x249b70, 'tviii', vb, 3, data), rdi(vb + 8), SDTest.Call(0x249b70, 'tviii', vb, 5,
	data), rdi(vb + 8)))
check('CRenderIB_Get', lambda: (SDTest.Call(0x249c50, 'tiip', ib, 0)[0] == rdi(ib), SDTest.Call(0x249c50, 'tiip', ib, 0)[1]))
check('CRenderVB_Get', lambda: (SDTest.Call(0x249c70, 'tiip', vb, 0)[0] == rdi(vb), SDTest.Call(0x249c70, 'tiip', vb, 0)[1]))
# the clears (the viewport is 0 between frames: the batches are not drawn first)
rect = SDTest.Call(0x1334d0, 'ii', 16)[0]
for i, v in ((0, 10), (1, 20), (2, 200), (3, 120)):
	SDTest.Poke(rect + 4 * i - 0x400000, v)
for r, flags in ((0, 1), (0, 2), (0, 3), (rect, 2), (rect, 1)):
	check('Render_Clear' + fmt((r != 0, flags)), lambda r=r, flags=flags: rdlog(lambda r=r, flags=flags:
		SDTest.Call(0x249de0, 'biiifi', r, flags, 0x102030, 1.0, 0)))
rdvp = (SDTest.Peek(0x4ad7e8, 'i'), SDTest.Peek(0x4ad7ec, 'i'))
SDTest.Poke(0x4ad7e8, 640)
SDTest.Poke(0x4ad7ec, 480)
check('Render_GetViewportSize', lambda: SDTest.Call(0x24e750, 'vpp', 0, 0))
SDTest.Poke(0x4ad7e8, rdvp[0])
SDTest.Poke(0x4ad7ec, rdvp[1])

# the texture stage effects of a material made here, its reflection map (2) and bump map (3) set up by hand; the time
# of the engine fixed for each call (the effects move with it), srand before the jitter
def rdpokef(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
rmat = SDTest.Call(0x233830, 'tii', SDTest.Call(0x1334d0, 'ii', 0xb0)[0])[0]
for i in (0, 2, 3):
	SDTest.Call(0x236030, 'tviis', rmat, i, 'bmp/partic1.bmp')
for i in (2, 3):
	mp = rdi(rmat + 0x9c + 4 * i)
	rdpokef(mp + 0x10, (0.25, -0.5, 1.5, 0.75, 0.0, 0.0, 0.3, 2.0))
	SDTest.Poke(mp + 0x54 - 0x400000, 0x100)
rdtime = SDTest.Peek(0x4a8ef0, 'i')
rdhw = SDTest.Peek(0x431bdc, 'i')
rdview = SDTest.Peek(0x4adb58, 'i')
SDTest.Poke(0x431bdc, 1)
def rdtimed(t, fn):
	SDTest.Poke(0x4a8ef0, t)
	return rdlog(fn)
for t in (0, 12345, 987654321):
	for view in (0, 1, 2):
		SDTest.Poke(0x4adb58, view)
		for flags in (1, 2, 4, 7):
			check('Mat_StageEnvBump' + fmt((t, view, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
				SDTest.Call(0x249f40, 'viiii', rmat, flags, 1, 0)))
	for flags in (1, 2, 5):
		check('Mat_StageScroll' + fmt((t, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
			SDTest.Call(0x24a4c0, 'viiiif', rmat, 2, flags, 1, 0.5)))
		check('Tex_StageTransform' + fmt((t, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
			SDTest.Call(0x24a5f0, 'viiiffff', 0, flags, 2, 0.3, -0.7, 2.0, 1.25)))
		check('Mat_StageBumpMat' + fmt((t, flags)), lambda t=t, flags=flags: rdtimed(t, lambda flags=flags:
			SDTest.Call(0x24a770, 'viiifff', rmat, flags, 0, 2.0, 0.5, 0.75)))
SDTest.Poke(0x4adb58, rdview)
SDTest.Poke(0x431bdc, rdhw)
SDTest.Poke(0x4a8ef0, rdtime)
check('Mat_StageScroll still', lambda: rdlog(lambda: SDTest.Call(0x24a4c0, 'viiiif', rmat, 0, 5, 0, 1.0)))
SDTest.Call(0x323c50, 'vi', 77)
for flags in (1, 2, 3):
	check('Tex_StageJitter' + fmt((flags,)), lambda flags=flags: rdlog(lambda flags=flags: SDTest.Call(0x24a390, 'viiifff', 0,
		flags, 1, 0.25, 0.5, 1.5)))
# the pixel shader alpha and the D3D material with and without DiffuseMod4x and a forced material
rdmod = SDTest.Peek(0x431be8, 'i')
cols = (0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 20.0)
for transp, atrib in ((0, 0), (1, 0), (1, 0x1000)):
	SDTest.Poke(rmat + 0x38 - 0x400000, transp << 16)
	SDTest.Poke(rmat + 0x3c - 0x400000, atrib)
	for forced in (0, rmat):
		SDTest.Poke(0x4ad754, forced)
		check('Render_PSConstAlpha' + fmt((transp, atrib, forced != 0)), lambda: rdlog(lambda: SDTest.Call(0x249ed0, 'vii',
			rmat, 2)))
		for mod in (0, 1):
			SDTest.Poke(0x431be8, mod)
			check('Render_D3DMaterial' + fmt((transp, forced != 0, mod)), lambda: rdlog(lambda: SDTest.Call(0x24a890, 'viF',
				rmat, cols)))
SDTest.Poke(0x4ad754, 0)
SDTest.Poke(0x431be8, rdmod)
check('Render_Get', lambda: (SDTest.Call(0x24c4d0, 'i')[0], SDTest.Call(0x24cdb0, 'i')[0], SDTest.Call(0x24cdc0, 'i')[0],
	SDTest.Call(0x24cdd0, 'i')[0]))

# the world matrix: given to the device or only remembered, read back
wm = (1.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 4.0, 5.0, 6.0, 1.0)
rdworld = []
for i in range(16):
	rdworld.append(SDTest.Peek(0x4ae478 + 4 * i, 'i'))
def rdw():
	r = [SDTest.Peek(0x432410, 'i')]
	for i in range(16):
		r.append(SDTest.Peek(0x4ae478 + 4 * i, 'i'))
	return r
check('Render_SetWorldMatrix defer', lambda: (rdlog(lambda: SDTest.Call(0x24bac0, 'iFi', wm, 1)), rdw()))
check('Render_SetWorld', lambda: (rdlog(lambda: SDTest.Call(0x24bc00, 'i')), rdw(), rdlog(lambda: SDTest.Call(0x24bc00, 'i'))))
check('Render_WorldDirty', lambda: (SDTest.Call(0x24bbb0, 'v'), rdw()))
check('Render_SetWorldMatrix', lambda: (rdlog(lambda: SDTest.Call(0x24bac0, 'iFi', wm[4:] + wm[:4], 0)), rdw()))
check('Render_SetWorldD3D', lambda: (rdlog(lambda: SDTest.Call(0x24bb30, 'iF', wm)), rdw()))
check('Render_GetWorld', lambda: (SDTest.Call(0x24bbc0, 'iF', (0.0,) * 16), SDTest.Call(0x24bbc0, 'ii', 0)))
for i in range(16):
	SDTest.Poke(0x4ae478 + 4 * i, rdworld[i])
check('Render_GetCaps', lambda: SDTest.Call(0x24af80, 'i')[0] == 0x4ae1f8 + 0x400000)
rect2 = SDTest.Call(0x1334d0, 'ii', 16)[0]
for i, v in ((0, -5), (1, 300), (2, 50), (3, 700)):
	SDTest.Poke(rect2 + 4 * i - 0x400000, v)
for r, c1, c2 in ((rect, 0xff0000, 0xff), (rect2, 0x808080, 0x10203040)):
	check('Render_DrawTexRect' + fmt((r != 0, c1, c2)), lambda r=r, c1=c1, c2=c2: rdlog(lambda r=r, c1=c1, c2=c2:
		SDTest.Call(0x24aa10, 'viiii', r, c1, c2, 0)))
check('Render_ClearForced', lambda: (SDTest.Call(0x24af30, 'v'), SDTest.Peek(0x4ad754, 'i'), SDTest.Peek(0x4ae580, 'i')))
# a frame begun and its view ended again (the viewport is 0 between frames)
check('Render_BeginFrame', lambda: (rdlog(lambda: SDTest.Call(0x24b880, 'v')), SDTest.Peek(0x4ad7e8, 'i') != 0,
	SDTest.Peek(0x4add70, 'i')))
check('Render_EndView', lambda: (rdlog(lambda: SDTest.Call(0x24aee0, 'v')), SDTest.Peek(0x4ad7e8, 'i')))

# Render_SetMaterial with the material of the stage tests: blending, alpha test, lighting, the second texture set,
# a forced material and the default material and texture modes (the last material forgotten before each)
check('Render_ResetState', lambda: rdlog(lambda: SDTest.Call(0x24c730, 'v')))
rdsm = []
for a in (0x43240d, 0x4ad8e0, 0x4ad868, 0x4ad754, 0x4ad74c, 0x4ada80):
	rdsm.append((a, SDTest.Peek(a, 'i')))
def rdset(pass_):
	SDTest.Call(0x24bf30, 'iii', 0, -1)
	return rdlog(lambda pass_=pass_: SDTest.Call(0x24bf30, 'iii', rmat, pass_))
rdsaved38 = (rdi(rmat + 0x30), rdi(rmat + 0x34), rdi(rmat + 0x38), rdi(rmat + 0x3c), rdi(rmat + 0x40))
# without the pixel shader modes (the mode of this material fails a CHECK with a texture)
SDTest.Poke(0x43240d, SDTest.Peek(0x43240d, 'i') & ~0xff)
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
	SDTest.Poke(0x4ada80, fog)
	check('Render_SetMaterial fog' + fmt((fog,)), lambda: (rdset(0), rdset(1)))
SDTest.Poke(0x4ad754, rmat)
check('Render_SetMaterial forced', lambda: rdset(0))
SDTest.Poke(0x4ad754, 0)
for usedef in (1, 2):
	SDTest.Poke(0x4ad8e0, usedef)
	check('Render_SetMaterial default material' + fmt((usedef,)), lambda: rdset(0))
SDTest.Poke(0x4ad8e0, 0)
for tex in (1, 2, 3, 4):
	SDTest.Poke(0x4ad868, tex)
	check('Render_SetMaterial default texture' + fmt((tex,)), lambda: rdset(0))
check('Render_SetMaterial same', lambda: (rdset(0), rdlog(lambda: SDTest.Call(0x24bf30, 'iii', rmat, 0))))
check('Render_SetMaterial none', lambda: (SDTest.Call(0x24bf30, 'iii', 0, -1), SDTest.Peek(0x4ae580, 'i')))
for a, v in rdsm:
	SDTest.Poke(a, v)
# the pixel shader modes with a material without textures (the shader of its mode is switched as it changes)
rmat2 = SDTest.Call(0x233830, 'tii', SDTest.Call(0x1334d0, 'ii', 0xb0)[0])[0]
SDTest.Poke(0x43240d, SDTest.Peek(0x43240d, 'i') | 1)
for transp in (0, 1):
	SDTest.Poke(rmat2 + 0x38 - 0x400000, transp << 16)
	check('Render_SetMaterial shaders' + fmt((transp,)), lambda: (rdlog(lambda: SDTest.Call(0x24bf30, 'iii', 0, -1)),
		rdlog(lambda: SDTest.Call(0x24bf30, 'iii', rmat2, 0)), rdlog(lambda: SDTest.Call(0x24bf30, 'iii', rmat2, 0)),
		SDTest.Peek(0x4ae590, 'i')))
for a, v in rdsm:
	SDTest.Poke(a, v)
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
rdproj = rdints(0x4ae3f8, 16)
rdview = rdints(0x4ae4b8, 16)
rdtiles = (SDTest.Peek(0x432414, 'i'), SDTest.Peek(0x4ae5d4, 'i'), SDTest.Peek(0x4ae5d8, 'i'), SDTest.Peek(0x4ae5dc, 'i'),
	SDTest.Peek(0x4a8efb, 'i'))
pm = (1.5, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -0.1, 0.0)
SDTest.Poke(0x432414, 2)
for step in range(7):
	on = (0, 1, 0, 0, 1, 0, 0)[step]
	SDTest.Poke(0x4a8efb, (rdtiles[4] & ~0xff) | on)
	check('Render_ProjTile' + fmt((step,)), lambda: (rdlog(lambda: SDTest.Call(0x24c4e0, 'bF', pm)), SDTest.Peek(0x4ae5d4, 'i'),
		SDTest.Peek(0x4ae5d8, 'i'), SDTest.Peek(0x4ae5dc, 'i'), SDTest.Peek(0x4a8efb, 'i') & 0xff, rdints(0x4ae3f8, 16)))
for a, v in ((0x432414, rdtiles[0]), (0x4ae5d4, rdtiles[1]), (0x4ae5d8, rdtiles[2]), (0x4ae5dc, rdtiles[3]),
	(0x4a8efb, rdtiles[4])):
	SDTest.Poke(a, v)
check('Render_SetProj', lambda: (rdlog(lambda: SDTest.Call(0x24c6a0, 'iF', pm)), rdints(0x4ae3f8, 16)))
rdcull = SDTest.Peek(0x4ad910, 'i')
for cm in (0, 1):
	SDTest.Poke(0x4ad910, cm)
	for mode in (1, 2, 3):
		check('Render_SetCull' + fmt((cm, mode)), lambda mode=mode: rdlog(lambda mode=mode: SDTest.Call(0x24c700, 'vi', mode)))
SDTest.Poke(0x4ad910, rdcull)
for mode in (0, 2, 4):
	check('Render_SetLightmapMat' + fmt((mode,)), lambda mode=mode: (SDTest.Call(0x24c880, 'vii', rmat, mode),
		SDTest.Peek(0x432408, 'i'), rdi(rmat + 8), SDTest.Call(0x24c880, 'vii', 0, 0), rdi(rmat + 8)))
mp = rdi(rmat + 0x9c)
rdmp = (rdi(mp + 8), rdi(mp + 0xc))
for b8, bc, lm in ((0, 0, 0), (1, 3, 0), (1, 1, 1), (0, 2, 0), (1, 0, 2)):
	SDTest.Poke(mp + 8 - 0x400000, (rdmp[0] & ~0xff00ff) | b8 | lm << 8)
	SDTest.Poke(mp + 0xc - 0x400000, (rdmp[1] & ~0xff) | bc)
	for stage in (0, 3):
		check('Render_SetMapa' + fmt((b8, bc, lm, stage)), lambda stage=stage: rdlog(lambda stage=stage:
			SDTest.Call(0x24c910, 'iii', stage, mp)))
SDTest.Poke(mp + 8 - 0x400000, rdmp[0])
SDTest.Poke(mp + 0xc - 0x400000, rdmp[1])
check('Render_SetMapa none', lambda: (rdlog(lambda: SDTest.Call(0x24c910, 'iii', 1, 0)), rdlog(lambda: SDTest.Call(0x24c910,
	'iii', 1, 0))))
check('Render_DrawVBIBFmt', lambda: rdlog(lambda: SDTest.Call(0x24cac0, 'iiiiiiii', rdi(vb), rdi(ib), 2, 50, 10, 3, 4)))
lv = SDTest.Call(0x1334d0, 'ii', 0x14)[0]
SDTest.Poke(lv - 0x400000, 0)
SDTest.Poke(lv + 4 - 0x400000, 40)
SDTest.Poke(lv + 0xc - 0x400000, 2)
check('Render_DrawVBIB', lambda: rdlog(lambda: SDTest.Call(0x24cd30, 'iiiii', rdi(vb), rdi(ib), lv, 12)))
md = SDTest.Call(0x1334d0, 'ii', 0x100)[0]
SDTest.Call(0x3197d0, 'iiFi', md, (0.0,) * 64, 0x100)
for ntri, strip, base, start, lim in ((20, 0, 2, 5, -1), (20, 0, 0, 0, 7), (0, 10, 0, 0, -1)):
	SDTest.Poke(md + 0xc - 0x400000, ntri)
	SDTest.Poke(md + 0x18 - 0x400000, 33)
	SDTest.Poke(md + 0x9c - 0x400000, base)
	SDTest.Poke(md + 0xa0 - 0x400000, start)
	SDTest.Poke(md + 0xa4 - 0x400000, strip)
	SDTest.Poke(md + 0xa8 - 0x400000, 2)
	SDTest.Poke(md + 0xac - 0x400000, rdi(vb))
	SDTest.Poke(md + 0xb0 - 0x400000, (strip == 0) * rdi(ib))
	rdlim = SDTest.Peek(0x4ad820, 'i')
	SDTest.Poke(0x4ad820, lim)
	check('Render_DrawMallaD3D' + fmt((ntri, strip, base, start, lim)), lambda: rdlog(lambda: SDTest.Call(0x24cb50, 'ii', md)))
	SDTest.Poke(0x4ad820, rdlim)
vm = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 5.0, -3.0, 2.0, 1.0)
check('Render_SetView', lambda: (rdlog(lambda: SDTest.Call(0x24cde0, 'iF', vm)), rdints(0x4ae4f8, 16), rdints(0x4adda8, 3)))
def rdfl(v):
	r = SDTest.Call(0x1334d0, 'ii', 64)[0]
	for i in range(16):
		SDTest.Poke(r + 4 * i - 0x400000, v[i])
	return r
SDTest.Call(0x24c6a0, 'ii', rdfl(rdproj))
SDTest.Call(0x24cde0, 'ii', rdfl(rdview))

# chunk F: the visibility tests on the planes of the frustum tests, the rectangles, the buffers made from data, the
# camera, the version string; frustum, view, projection and viewport put back after
rdplanes = rdints(0x4adbf0, 24)
SDTest.Call(0x3197d0, 'iiFi', 0x4adbf0 + 0x400000, (-1.0, 0.0, 0.0, -10.0, 1.0, 0.0, 0.0, -10.0, 0.0, -1.0, 0.0, -10.0,
	0.0, 1.0, 0.0, -10.0, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 1.0, -100.0), 96)
mid = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 50.0, 1.0)
far = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 300.0, 50.0, 1.0)
for m in (mid, far):
	for box in ((-1.0, -1.0, -1.0, 1.0, 1.0, 1.0), (-200.0, -2.0, -2.0, 200.0, 2.0, 2.0)):
		for scale in (1.0, 0.5):
			check('Box_IsVisible' + fmt((m[13], box[0], scale)), lambda m=m, box=box, scale=scale: SDTest.Call(0x24d0e0, 'bFFsf',
				m, box, 'sd', scale)[0])
for c, r in (((0.0, 0.0, 50.0), 1.0), ((0.0, 30.0, 50.0), 5.0), ((0.0, 30.0, 50.0), 25.0)):
	check('Sph_IsVisible' + fmt((c, r)), lambda c=c, r=r: SDTest.Call(0x24d2e0, 'bFfi', c, r, 0)[0])
check('Frustum_SegVisible', lambda: (SDTest.Call(0x24d2c0, 'bFF', (0.0, 0.0, 50.0), (0.0, 30.0, 50.0))[0],
	SDTest.Call(0x24d2c0, 'bFF', (20.0, 0.0, 50.0), (30.0, 0.0, 50.0))[0]))
for i in range(24):
	SDTest.Poke(0x4adbf0 + 4 * i, rdplanes[i])
vp = SDTest.Call(0x1334d0, 'ii', 24)[0]
for i, v in ((0, 10), (1, 20), (2, 640), (3, 480), (4, 0), (5, 0x3f800000)):
	SDTest.Poke(vp + 4 * i - 0x400000, v)
for box in ((0.0, 0.0, 0.0, 1.0, 1.0, 0.0), (0.25, 0.1, 0.0, 0.5, 0.75, 0.0), (-0.5, 0.5, 0.0, 2.0, 0.4, 0.0),
	(0.7, 0.7, 0.0, 0.2, 0.9, 0.0), (0.33333, 0.66666, 0.0, 0.4999, 0.9999, 0.0)):
	check('Render_ClipRect' + fmt(box), lambda box=box: SDTest.Call(0x24e220, 'biiF', SDTest.Call(0x1334d0, 'ii', 16)[0], vp,
		box)[0:1])
	out16 = SDTest.Call(0x1334d0, 'ii', 16)[0]
	check('Render_ClipRect out' + fmt(box), lambda box=box, out16=out16: (SDTest.Call(0x24e220, 'biiF', out16, vp, box),
		rdheap(out16, 4)))
rdvp2 = rdints(0x4ad7e0, 6)
SDTest.Call(0x3197d0, 'iiii', 0x4ad7e0 + 0x400000, vp, 24)
for box, proj in (((0.25, 0.1, 0.0, 0.5, 0.75, 0.0), 1), ((0.0, 0.0, 0.0, 1.0, 1.0, 0.0), 0)):
	check('Render_SetViewRect' + fmt((box, proj)), lambda box=box, proj=proj: rdlog(lambda box=box, proj=proj:
		SDTest.Call(0x24e3b0, 'bFi', box, proj)))
check('Render_SetViewRect none', lambda: rdlog(lambda: SDTest.Call(0x24e3b0, 'bii', 0, 0)))
check('Render_GetViewport', lambda: (SDTest.Call(0x24e650, 'i')[0], SDTest.Call(0x24e620, 'i')[0]))
for i in range(6):
	SDTest.Poke(0x4ad7e0 + 4 * i, rdvp2[i])
check('Render_StencilBits', lambda: SDTest.Call(0x24e6f0, 'i')[0])
for v in (0x0101, 0x0800, 0xffff, 0x12345):
	check('Render_VersionStr' + fmt((v,)), lambda v=v: SDTest.Call(0x24d570, 'si', v)[0])
check('Render_NoIndexedBlend', lambda: rdlog(lambda: SDTest.Call(0x24e390, 'vi', SDTest.Peek(0x4ad6f4, 'i'))))
tris = SDTest.Call(0x1334d0, 'ii', 36)[0]
for i in range(9):
	SDTest.Poke(tris + 4 * i - 0x400000, i * 0x10001)
cell = SDTest.Call(0x1334d0, 'ii', 4)[0]
for dyn in (0, 1):
	check('Render_CreateIB' + fmt((dyn,)), lambda dyn=dyn: (rdlog(lambda dyn=dyn: SDTest.Call(0x24d380, 'iiiii', cell, tris, 6,
		dyn)), rdi(cell) != 0))
	SDTest.Poke(lv - 0x400000, data)
	SDTest.Poke(lv + 4 - 0x400000, 6)
	check('Render_CreateVB' + fmt((dyn,)), lambda dyn=dyn: (rdlog(lambda dyn=dyn: SDTest.Call(0x24d460, 'iiii', cell, lv, dyn)),
		rdi(cell) != 0))
cam = SDTest.Call(0x29c110, 'tii', SDTest.Call(0x1334d0, 'ii', 0x78)[0])[0]
check('Render_SetCamara', lambda: (rdlog(lambda: SDTest.Call(0x24d5a0, 'vi', cam)), rdints(0x4ae3f8, 16), rdints(0x4ae4b8, 16),
	rdints(0x4adbf0, 24)))
for i in range(24):
	SDTest.Poke(0x4adbf0 + 4 * i, rdplanes[i])
SDTest.Call(0x24c6a0, 'ii', rdfl(rdproj))
SDTest.Call(0x24cde0, 'ii', rdfl(rdview))

# Render_ToScreen with a view and projection set here and the test viewport
SDTest.Call(0x24cde0, 'iF', (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
SDTest.Call(0x24c6a0, 'iF', (1.0, 0.0, 0.0, 0.0, 0.0, 1.3333, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -1.001, 0.0))
SDTest.Call(0x3197d0, 'iiii', 0x4ad7e0 + 0x400000, vp, 24)
for p in ((0.0, 0.0, 10.0), (3.0, -2.0, 10.0), (20.0, 0.0, 10.0), (0.0, 0.0, 0.5), (0.0, 0.0, -10.0), (-9.99, 7.4, 10.0),
	(0.0, 0.0, 2000.0)):
	check('Render_ToScreen' + fmt(p), lambda p=p: SDTest.Call(0x24cf00, 'iFF', p, (-1.0, -1.0)))
for i in range(6):
	SDTest.Poke(0x4ad7e0 + 4 * i, rdvp2[i])
SDTest.Call(0x24c6a0, 'ii', rdfl(rdproj))
SDTest.Call(0x24cde0, 'ii', rdfl(rdview))

# Frustum_Calc from views and projections given here (one without an inverse); the planes put back after
idm = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
for v, p in ((idm, pm), (vm, pm), ((0.8, 0.0, -0.6, 0.0, 0.0, 1.0, 0.0, 0.0, 0.6, 0.0, 0.8, 0.0, 10.0, -5.0, 30.0, 1.0),
	(1.2, 0.0, 0.0, 0.0, 0.0, 1.6, 0.0, 0.0, 0.0, 0.0, 1.0001, 1.0, 0.0, 0.0, -0.50005, 0.0)), (idm, (0.0,) * 16)):
	check('Frustum_Calc' + fmt((v[12], p[0])), lambda v=v, p=p: (SDTest.Call(0x24d640, 'vFF', v, p), rdints(0x4adbf0, 24)))
for i in range(24):
	SDTest.Poke(0x4adbf0 + 4 * i, rdplanes[i])

# Render_MallaD3D with the mesh of the buffer draws and the material without textures, no render node
SDTest.Poke(md + 0xc - 0x400000, 20)
SDTest.Poke(md + 0x18 - 0x400000, 33)
SDTest.Poke(md + 0x9c - 0x400000, 0)
SDTest.Poke(md + 0xa0 - 0x400000, 0)
SDTest.Poke(md + 0xa4 - 0x400000, 0)
SDTest.Poke(md + 0xb0 - 0x400000, rdi(ib))
SDTest.Poke(md + 0x4c - 0x400000, rmat2)
rdvb = SDTest.Peek(0x431bac, 'i')
rdhw = SDTest.Peek(0x431bdc, 'i')
SDTest.Poke(0x431bdc, 1)
rdtris = SDTest.Peek(0x4a8f00, 'i')
for b3b in (0, 0x01000000):
	SDTest.Poke(rmat2 + 0x38 - 0x400000, b3b)
	check('Render_MallaD3D' + fmt((b3b != 0,)), lambda: (SDTest.Call(0x24bf30, 'iii', 0, -1), rdlog(lambda: SDTest.Call(0x24bc40,
		'iii', md, 0)), SDTest.Peek(0x4a8f00, 'i') - rdtris))
	rdtris = SDTest.Peek(0x4a8f00, 'i')
SDTest.Poke(rmat2 + 0x38 - 0x400000, 0)
SDTest.Poke(0x431bdc, rdhw)
