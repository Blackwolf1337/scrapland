# engine3d Engine/Grafico3D.c: sprites made here (Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt
# come from there). Not tested: the drawing (device).
def g3new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def g3i(addr):
	return SDTest.Peek(addr, '@i')
def g3get(g):
	m = g3i(g + 0x20)
	r = (g3i(g + 4), [g3i(g + 8), g3i(g + 0xc), g3i(g + 0x10), g3i(g + 0x14), g3i(g + 0x18)], g3i(g + 0x1c) != 0, m != 0,
		g3i(g + 0x24) != 0)
	if m:
		r = r + (g3i(m + 0x14), g3i(m + 0x30), g3i(m + 0x34), (g3i(m + 0x38) >> 16) & 0xff, SDTest.Call('CMaterial_GetMapName', 'tsii', m, 0)[0],
			SDTest.Call('CMaterial_GetMapName', 'tsii', m, 1)[0])
	return r
g1 = SDTest.Call('CGraphic3D_ctor', 'tii', g3new(0x3c))[0]
check('CGraphic3D_ctor', lambda: g3get(g1))
g2 = SDTest.Call('CGraphic3D_ctorTex', 'tiisFF', g3new(0x3c), 'bmp/partic1.bmp', (1.0, 2.0, 3.0), (4.0, 0.5))[0]
check('CGraphic3D_ctorTex', lambda: g3get(g2))
for tex, opac in (('bmp/partic1.bmp', None), ('bmp/partic1.bmp', 'bmp/partic1.bmp'), ('sdnone.bmp', None)):
	check('CGraphic3D_SetTexture' + fmt((tex, opac)), lambda tex=tex, opac=opac: (SDTest.Call('CGraphic3D_SetTexture', 'tvisz', g2, tex, opac),
		g3get(g2)))
n = SDTest.Call('CNode3D_ctor', 'tii', g3new(0xfc))[0]
SDTest.Poke(n + 8 - 0x400000, 1)
check('CGraphic3D_SetNode', lambda: (SDTest.Call('CGraphic3D_SetNode', 'tvii', g1, n), g3i(n + 8), SDTest.Call('CGraphic3D_SetNode', 'tvii', g1, n),
	g3i(n + 8), SDTest.Call('CGraphic3D_SetNode', 'tvii', g1, 0), g3i(n + 8), SDTest.Call('CGraphic3D_SetNode', 'tvii', g1, n), g3i(n + 8)))
check('CGraphic3D_Init', lambda: (SDTest.Call('CGraphic3D_Init', 'tvi', g3new(0x3c)), 1))
for g in (g1, g2):
	check('CGraphic3D_Delete', lambda g=g: (SDTest.Call('CGraphic3D_Delete', 'tiii', g, 1)[0] == g, g3i(n + 8)))
