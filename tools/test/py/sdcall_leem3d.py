# engine3d Engine/LeeM3D.c: scenes of the .SM3 caches of the packs and an animation of a .CM3 for one of them, a
# missing cache, SM3_UseScenes off. The scenes are kept. Python 1.5.2, runs inside sdcall.py after the other checks:
# check, fmt come from there.
def m3i(a):
	return SDTest.Peek(a, '@i')
def m3scene(e):
	if not e:
		return None
	r = [SDTest.Peek(e + 4, '@p'), m3i(e + 8)]
	n = m3i(e + 0xc)
	names = []
	while n and len(names) < 200:
		names.append(SDTest.Peek(n + 4, '@p'))
		n = m3i(n + 0xc)
	r.append(names)
	for o in range(0x14, 0x3c, 4):
		r.append(m3i(e + o))
	r = r + [m3i(e + 0x3c) != 0, m3i(e + 0x48), m3i(e + 0x58), SDTest.Peek(e + 0x60, '@b')]
	return r
models = ('models/elements/antenaa/antenaa.m3d', 'models/elements/antenaa/antenaa.lod1.m3d',
	'models/heads/headpolice&gear/headpolice.m3d', 'models/elements/anilloestructurad/anilloestructurad.m3d',
	'sdnone/none.m3d')
m3esc = {}
for name in models:
	check('SM3_Load' + fmt((name,)), lambda name=name: m3scene(SDTest.Call('SM3_Load', 'is', name)[0]))
	m3esc[name] = SDTest.Call('SM3_Load', 'is', name)[0]
rdsm3 = SDTest.Peek('g_SM3UseScenes', 'i')
SDTest.Poke('g_SM3UseScenes', 0)
check('SM3_Load off', lambda: SDTest.Call('SM3_Load', 'is', models[0])[0])
SDTest.Poke('g_SM3UseScenes', rdsm3)
for base, anim in ((models[0], 'models/elements/antenaa/anm/loop.m3d'), (models[2],
	'models/heads/headpolice&gear/headpolice.anim0.m3d'), (models[0], 'sdnone/anim.m3d')):
	SDTest.Poke('g_EscenaAnimPadre', m3esc[base])
	SDTest.Call('crt_memcpy', 'iisi', sym('g_M3DName') + 0x400000, anim, len(anim) + 1)
	check('CM3_Load' + fmt((anim,)), lambda anim=anim: m3scene(SDTest.Call('CM3_Load', 'is', anim)[0]))
	SDTest.Poke('g_EscenaAnimPadre', 0)
# CM3_Save and SM3_Save are not tested: an open for writing is a fatal CHECK in the test setup
for ok in (0, 1, 0, 0):
	check('M3D_SetOk' + fmt((ok,)), lambda ok=ok: (SDTest.Call('M3D_SetOk', 'vi', ok), SDTest.Peek('g_M3DOk', 'i') & 0xff))

# the node and name utilities
for a, b in ((1.0, 1.0), (1.0, 1.0000001), (1.0, 1.001), (-2.0, -2.0000002), (2.0, -2.0), (0.0, 1e-30), (0.0, -0.0),
	(1e-30, 1e-30 * 1.0000001), (-1.0, 1.0)):
	check('Float_CasiIgual' + fmt((a, b)), lambda a=a, b=b: SDTest.Call('Float_CasiIgual', 'bff', a, b)[0])
m3nb = SDTest.Call('operator_new', 'ii', 0x100)[0]
for nm in ('Bip Spine', 'Bip_x', 'BipB', 'Bone01', 'bip x', 'DR_door', 'dm_x', 'D_Cam-1', 'Actor_A', 'Traffic_3',
	'NAVEBERTO', 'Navebert', 'flare01 (tam=12)', 'x(+tam = 7)', 'b(tam)', '(TAM=3x)', 'n\351me ', ''):
	SDTest.Call('crt_memcpy', 'iisi', m3nb, nm, len(nm) + 1)
	nodo = SDTest.Call('operator_new', 'ii', 0xfc)[0]
	SDTest.Poke(nodo + 4 - 0x400000, m3nb)
	SDTest.Poke(nodo + 0x18 - 0x400000, 0x10)
	m3out = SDTest.Call('operator_new', 'ii', 4)[0]
	SDTest.Poke(m3out - 0x400000, -5)
	check('Nodo names' + fmt((nm,)), lambda nodo=nodo, m3out=m3out: (SDTest.Call('Nodo_MarcarBip', 'vi', nodo), m3i(nodo + 0x18),
		SDTest.Call('Nodo_EsDeJuego', 'bi', nodo)[0], SDTest.Call('Nodo_ParamInt', 'bisi', m3nb, 'tam', m3out)[0], m3i(m3out),
		SDTest.Call('Nodo_LimpiarNombre', 'vi', m3nb), SDTest.Peek(m3nb, '@s')))
mat = SDTest.Call('CMaterial_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0xb0)[0])[0]
rdmu = []
for a in ('g_M3DUseOpacity', 'g_M3DUseBumpmap'):
	rdmu.append(SDTest.PeekSpan(a, 0, 4))
for atrib in (0, 0x200):
	SDTest.Poke(mat + 0x3c - 0x400000, atrib)
	for use in (0, 0x01000100, 0x01010101):
		SDTest.PokeSpan('g_M3DUseOpacity', 0, use, 4)
		SDTest.PokeSpan('g_M3DUseBumpmap', 0, use, 4)
		check('M3D_MapaActivo' + fmt((atrib, use)), lambda mat=mat: map(lambda i, mat=mat: SDTest.Call('M3D_MapaActivo', 'bii',
			mat, i)[0], (0, 1, 2, 3, 4, 5)))
SDTest.PokeSpan('g_M3DUseOpacity', 0, rdmu[0], 4)
SDTest.PokeSpan('g_M3DUseBumpmap', 0, rdmu[1], 4)
for name in (models[2], models[0]):
	e = m3esc[name]
	n = m3i(e + 0xc)
	nodes = []
	while n:
		nodes.append(n)
		n = m3i(n + 0xc)
	for i in range(1, len(nodes), 3):
		for j in (0, i / 2, i - 1):
			for cr in (0, 1):
				check('Nodo_EsAntecesor' + fmt((name, i, j, cr)), lambda nodes=nodes, i=i, j=j, cr=cr:
					SDTest.Call('Nodo_EsAntecesor', 'biii', nodes[i], nodes[j], cr)[0])
	for nd in nodes[-3:]:
		check('Escena_CheckDuplicados' + fmt((name,)), lambda e=e, nd=nd: SDTest.Call('Escena_CheckDuplicados', 'vii', e, nd))
	check('Escena_Flares/Sprites' + fmt((name,)), lambda e=e: (SDTest.Call('Escena_Flares', 'vi', e), SDTest.Call('Escena_Sprites',
		'vi', e), m3scene(e)))
	check('Escena_Limpiar' + fmt((name,)), lambda e=e: (SDTest.Call('Escena_Limpiar', 'vis', e, 'sd'), m3scene(e)))

# The M3D reader itself is not tested: the packs hold no .M3D files, only the .CM3 and .SM3 caches.
