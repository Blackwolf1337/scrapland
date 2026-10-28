# engine3d Engine/Grafico2D.c: screen sprites made here, their materials and their drawing through the D3D log, with
# the viewport size set for the draw (it is 0 between frames). Python 1.5.2, runs inside sdcall.py after the other
# checks: check, fmt come from there. Not with 0x40: a multi sprite needs a texture made as one (fatal CHECK).
def g2new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def g2i(a):
	return SDTest.Peek(a, '@i')
def g2ints(a, n):
	r = []
	for i in range(n):
		r.append(g2i(a + 4 * i))
	return r
def g2floats(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
# the sprite without its pointers, its material
def g2get(g):
	r = (g2ints(g, 8), g2i(g + 0x20), g2i(g + 0x24) & 0xffff, g2ints(g + 0x28, 5), g2i(g + 0x40) != 0)
	m = g2i(g + 0x3c)
	if m:
		r = r + (g2i(m + 0x14), g2i(m + 0x30), g2i(m + 0x34), g2i(m + 0x38), g2i(m + 0x3c), g2i(m + 0x40),
			SDTest.Call('CMaterial_GetMapName', 'tsii', m, 0)[0], SDTest.Call('CMaterial_GetMapName', 'tsii', m, 1)[0])
		mp = g2i(m + 0x9c)
		if mp:
			r = r + (g2i(mp + 8), g2i(mp + 0xc) & 0xffffff)
	return r
def g2log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r[0], SDTest.Out())

g0 = SDTest.Call('CGrafico2D_ctor', 'tii', g2new(0x44))[0]
check('CGrafico2D_ctor', lambda: g2get(g0))
gs = []
for flags in (0, 1, 2, 3, 4, 8, 0x10, 0x20, 0x80, 0x100, 0x1bf):
	g = SDTest.Call('CGrafico2D_ctorTex', 'tiissi', g2new(0x44), 'bmp/partic1.bmp', 'bmp/partic1.bmp', flags)[0]
	gs.append(g)
	check('CGrafico2D_ctorTex' + fmt((flags,)), lambda g=g: g2get(g))
for tex, opac, flags in (('bmp/partic1.bmp', None, 0), ('bmp/partic1.bmp', 'bmp/partic1.bmp', 0x30), ('sdnone.bmp', None, 0)):
	check('CGrafico2D_SetTexture' + fmt((tex, opac, flags)), lambda tex=tex, opac=opac, flags=flags: (SDTest.Call('CGrafico2D_SetTexture',
		'tviszi', g0, tex, opac, flags), g2get(g0)))
check('CGrafico2D_CreateMat', lambda: (SDTest.Call('CGrafico2D_CreateMat', 'tvisziiii', gs[0], 'bmp/partic1.bmp', None, 2, 1, 0x80, 0x24),
	g2get(gs[0])))
check('Grafico2D_SetPantalla', lambda: (SDTest.Call('Grafico2D_SetPantalla', 'vii', 800, 600), SDTest.Peek('g_Pantalla2DInvW', 'd'), SDTest.Call('Grafico2D_GetPantalla',
	'vpp', 0, 0)))

g2vp = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
g2scale = SDTest.Peek('g_SpriteScaleWidth', 'i')
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
g = gs[0]
g2floats(g, (100.0, 50.0, 0.25, 0.75, 2.0, 1.5, 1.0, 0.5))
g2floats(g + 0x28, (0.25, 0.0, 0.75, 1.0))
# without sinTextura the texture size is needed, which the menu does not have: nothing drawn
for pant, tam, cen, sin, rot in ((0, 0, 0, 0, 0), (1, 0, 0, 1, 1), (0, 1, 1, 1, 2), (1, 1, 0, 1, 3), (0, 0, 1, 1, 0),
	(0, 0, 0, 1, 1), (1, 1, 1, 1, 2)):
	SDTest.Poke(g + 0x20 - 0x400000, pant | tam << 8 | cen << 16 | sin << 24)
	SDTest.Poke(g + 0x24 - 0x400000, rot << 8)
	check('CGrafico2D_Render' + fmt((pant, tam, cen, sin, rot)), lambda g=g: g2log(lambda g=g: SDTest.Call('CGrafico2D_Render', 'tvi', g)))
SDTest.Call('crt_memcpy', 'iiFi', sym('g_SpriteScaleWidth') + 0x400000, (1.25,), 4)
check('CGrafico2D_Render scaled', lambda: g2log(lambda: SDTest.Call('CGrafico2D_Render', 'tvi', g)))
cols = g2new(24)
for i in range(6):
	SDTest.Poke(cols + 4 * i - 0x400000, 0x10203040 * (i + 1))
pos = (0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.5, 2.0, -1.0, 0.25)
uv = (0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.5, 0.5, 0.25, 0.75)
for tipo, n in ((4, 6), (5, 4), (6, 5), (5, 6)):
	check('CGrafico2D_Draw' + fmt((tipo, n)), lambda tipo=tipo, n=n: g2log(lambda tipo=tipo, n=n: SDTest.Call('CGrafico2D_Draw',
		'tviFFiii', g, pos[:2 * n], uv[:2 * n], 0, n, tipo)))
	check('CGrafico2D_Draw no uv' + fmt((tipo, n)), lambda tipo=tipo, n=n: g2log(lambda tipo=tipo, n=n: SDTest.Call('CGrafico2D_Draw',
		'tviFiiii', g, pos[:2 * n], 0, cols, n, tipo)))
SDTest.Poke('g_SpriteScaleWidth', g2scale)
SDTest.Poke('g_ViewportW', g2vp[0])
SDTest.Poke('g_ViewportH', g2vp[1])
for g in [g0] + gs:
	check('CGrafico2D_dtor', lambda g=g: (SDTest.Call('CGrafico2D_dtor', 'tvi', g), g2i(g + 0x40)))
