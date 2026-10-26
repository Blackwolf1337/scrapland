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
	check('SM3_Load' + fmt((name,)), lambda name=name: m3scene(SDTest.Call(0x2bc5c0, 'is', name)[0]))
	m3esc[name] = SDTest.Call(0x2bc5c0, 'is', name)[0]
rdsm3 = SDTest.Peek(0x4e1f98, 'i')
SDTest.Poke(0x4e1f98, 0)
check('SM3_Load off', lambda: SDTest.Call(0x2bc5c0, 'is', models[0])[0])
SDTest.Poke(0x4e1f98, rdsm3)
for base, anim in ((models[0], 'models/elements/antenaa/anm/loop.m3d'), (models[2],
	'models/heads/headpolice&gear/headpolice.anim0.m3d'), (models[0], 'sdnone/anim.m3d')):
	SDTest.Poke(0x4e1ec0, m3esc[base])
	SDTest.Call(0x3197d0, 'iisi', 0x4e23c0 + 0x400000, anim, len(anim) + 1)
	check('CM3_Load' + fmt((anim,)), lambda anim=anim: m3scene(SDTest.Call(0x2bc360, 'is', anim)[0]))
	SDTest.Poke(0x4e1ec0, 0)
