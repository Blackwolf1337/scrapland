# engine3d Engine/Flare.c: a flare made here on a fake node, its fade through renders at set times and positions
# (g_pd3dDevice 0 so the deferred draw returns at once, the device for the viewport passed), its sprite draw through
# the D3D log. Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt come from there.
def fli(a):
	return SDTest.Peek(a, '@i')
def flpokef(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def flget(f):
	g = fli(f + 0x34)
	r = []
	for o in range(0x0c, 0x24, 4):
		r.append(fli(f + o))
	r = r + [SDTest.Peek(f + 0x24, '@b'), SDTest.Peek(f + 0x25, '@b')]
	for o in range(0x28, 0x3c, 4):
		r.append(fli(f + o) != 0 and o == 0x34 or fli(f + o))
	if g:
		r = r + [fli(g), fli(g + 4), fli(g + 0x10), fli(g + 0x14), fli(g + 0x38)]
	return r
a = SDTest.Call('operator_new', 'ii', 44)[0]
b = SDTest.Call('operator_new', 'ii', 44)[0]
vals = (1.0, 2.0, -3.0, 0.5, 0.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0)
flpokef(a, vals)
for i in (-1, 0, 3, 6, 10):
	v = list(vals)
	if i >= 0:
		v[i] = v[i] + 1.0
	flpokef(b, tuple(v))
	check('Floats11_Equal' + fmt((i,)), lambda: SDTest.Call('Floats11_Equal', 'bii', a, b)[0])
f = SDTest.Call('CFlare_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0x3c)[0])[0]
check('CFlare_ctor', lambda f=f: (fli(f), fli(f + 4), flget(f)))
check('CFlare_SetTextureAdd', lambda f=f: (SDTest.Call('CFlare_SetTextureAdd', 'tviss', f, 'bmp/rgb_white.tga', 'bmp/rgb_grey.tga'),
	flget(f)))
check('CFlare_SetTexture', lambda f=f: (SDTest.Call('CFlare_SetTexture', 'tvizziii', f, 'bmp/rgb_white.tga', None, 2, 1, 180),
	flget(f), SDTest.Peek(fli(fli(f + 0x34) + 0x3c) + 0x30, '@i')))
nodo = SDTest.Call('operator_new', 'ii', 0x100)[0]
SDTest.Call('crt_memcpy', 'iiFi', nodo + 0x5c, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0,
	1.0), 64)
SDTest.Poke(nodo + 4 - 0x400000, 0)
SDTest.Poke(f + 8 - 0x400000, nodo)
rdproj = []
rdviewm = []
for i in range(16):
	rdproj.append(SDTest.Peek(sym('g_MatProj') + 4 * i, 'i'))
	rdviewm.append(SDTest.Peek(sym('g_MatView') + 4 * i, 'i'))
# a projection of 90 degrees, the view the identity, the eye at 0
SDTest.Call('crt_memcpy', 'iiFi', sym('g_MatProj') + 0x400000, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0,
	0.0, -0.1, 0.0), 64)
SDTest.Call('crt_memcpy', 'iiFi', sym('g_MatView') + 0x400000, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0), 64)
rdfl = (SDTest.Peek('g_TimeMsF', 'i'), SDTest.Peek('g_pd3dDevice', 'i'), SDTest.Peek('g_FlareAttenDist', 'i'), SDTest.Peek('g_FlareAttenDistEnd', 'i'),
	SDTest.Peek('g_FlareFadeIn', 'i'), SDTest.Peek('g_FlareFadeOut', 'i'), SDTest.Peek('g_FlareShowInfo', 'i'), SDTest.Peek('g_ShowFlaresInfo', 'i'))
rdfl2 = (SDTest.Peek('g_SkipFlares', 'i'), SDTest.Peek('g_Overdraw', 'i'), SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
dev = rdfl[1]
SDTest.Poke('g_SkipFlares', 0)
SDTest.Poke('g_Overdraw', 0)
SDTest.Poke('g_FlareShowInfo', 0)
SDTest.Poke('g_ShowFlaresInfo', 0)
flpokef(sym('g_FlareFadeIn') + 0x400000, (2.0,))
flpokef(sym('g_FlareFadeOut') + 0x400000, (0.5,))
def flrender(f, t, intens):
	flpokef(sym('g_TimeMsF') + 0x400000, (t,))
	flpokef(f + 0x28, (intens,))
	SDTest.Poke('g_pd3dDevice', 0)
	SDTest.Call('CFlare_Render', 'tviii', f, dev, 0)
	SDTest.Poke('g_pd3dDevice', dev)
	return flget(f)
SDTest.Poke('g_FlareAttenDist', 1)
flpokef(sym('g_FlareAttenDistEnd') + 0x400000, (50.0,))
for aten in (0, 1):
	SDTest.Poke('g_FlareAttenDist', aten)
	for pos in ((0.0, 0.0, 10.0), (3.0, -2.0, 5.0), (0.0, 0.0, -5.0), (30.0, 0.0, 10.0), (0.0, 0.0, 0.01),
		(0.0, 0.0, 200.0)):
		flpokef(nodo + 0x5c + 48, pos)
		flpokef(f + 0xc, pos)
		for t in (1000.0, 1016.0, 1100.0, 1101.0, 1500.0, 3000000000.0):
			for intens in (1.0, 0.0, 0.3):
				check('CFlare_Render' + fmt((aten, pos, t, intens)), lambda f=f, t=t, intens=intens:
					flrender(f, t, intens))
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
flpokef(f + 0x18, (0.5, 0.25))
check('CFlare_Draw', lambda f=f: rdlog(lambda f=f: SDTest.Call('CFlare_Draw', 'tvi', f)))
SDTest.Call('crt_memcpy', 'iiFi', sym('g_MatProj') + 0x400000, (0.0,) * 16, 64)
for i in range(16):
	SDTest.Poke(sym('g_MatProj') + 4 * i, rdproj[i])
	SDTest.Poke(sym('g_MatView') + 4 * i, rdviewm[i])
for addr, v in ((sym('g_TimeMsF'), rdfl[0]), (sym('g_FlareAttenDist'), rdfl[2]), (sym('g_FlareAttenDistEnd'), rdfl[3]), (sym('g_FlareFadeIn'), rdfl[4]),
	(sym('g_FlareFadeOut'), rdfl[5]), (sym('g_FlareShowInfo'), rdfl[6]), (sym('g_ShowFlaresInfo'), rdfl[7]), (sym('g_SkipFlares'), rdfl2[0]), (sym('g_Overdraw'), rdfl2[1]),
	(sym('g_ViewportW'), rdfl2[2]), (sym('g_ViewportH'), rdfl2[3])):
	SDTest.Poke(addr, v)
SDTest.Poke(f + 8 - 0x400000, 0)
check('CFlare_Delete', lambda f=f: SDTest.Call('CFlare_Delete', 'tiii', f, 1)[0] == f)
