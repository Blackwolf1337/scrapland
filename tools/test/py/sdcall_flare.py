# engine3d Engine/Flare.c: a flare made here on a fake node, its fade through renders at set times and positions
# (g_pd3dDevice 0 so the deferred draw returns at once, the device for the viewport passed), its sprite draw through
# the D3D log. Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt come from there.
def fli(a):
	return SDTest.Peek(a, '@i')
def flpokef(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
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
a = SDTest.Call(0x1334d0, 'ii', 44)[0]
b = SDTest.Call(0x1334d0, 'ii', 44)[0]
vals = (1.0, 2.0, -3.0, 0.5, 0.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0)
flpokef(a, vals)
for i in (-1, 0, 3, 6, 10):
	v = list(vals)
	if i >= 0:
		v[i] = v[i] + 1.0
	flpokef(b, tuple(v))
	check('Floats11_Equal' + fmt((i,)), lambda: SDTest.Call(0x297bd0, 'bii', a, b)[0])
f = SDTest.Call(0x297c90, 'tii', SDTest.Call(0x1334d0, 'ii', 0x3c)[0])[0]
check('CFlare_ctor', lambda f=f: (fli(f), fli(f + 4), flget(f)))
check('CFlare_SetTextureAdd', lambda f=f: (SDTest.Call(0x298360, 'tviss', f, 'bmp/rgb_white.tga', 'bmp/rgb_grey.tga'),
	flget(f)))
check('CFlare_SetTexture', lambda f=f: (SDTest.Call(0x298380, 'tvizziii', f, 'bmp/rgb_white.tga', None, 2, 1, 180),
	flget(f), SDTest.Peek(fli(fli(f + 0x34) + 0x3c) + 0x30, '@i')))
nodo = SDTest.Call(0x1334d0, 'ii', 0x100)[0]
SDTest.Call(0x3197d0, 'iiFi', nodo + 0x5c, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0,
	1.0), 64)
SDTest.Poke(nodo + 4 - 0x400000, 0)
SDTest.Poke(f + 8 - 0x400000, nodo)
rdproj = []
rdviewm = []
for i in range(16):
	rdproj.append(SDTest.Peek(0x4ae3f8 + 4 * i, 'i'))
	rdviewm.append(SDTest.Peek(0x4ae4b8 + 4 * i, 'i'))
# a projection of 90 degrees, the view the identity, the eye at 0
SDTest.Call(0x3197d0, 'iiFi', 0x4ae3f8 + 0x400000, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0,
	0.0, -0.1, 0.0), 64)
SDTest.Call(0x3197d0, 'iiFi', 0x4ae4b8 + 0x400000, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0), 64)
rdfl = (SDTest.Peek(0x4a8ed0, 'i'), SDTest.Peek(0x4ad6f4, 'i'), SDTest.Peek(0x4d6d90, 'i'), SDTest.Peek(0x4329a0, 'i'),
	SDTest.Peek(0x4329a4, 'i'), SDTest.Peek(0x4329a8, 'i'), SDTest.Peek(0x4d6d94, 'i'), SDTest.Peek(0x4d6da8, 'i'))
rdfl2 = (SDTest.Peek(0x4d6dc0, 'i'), SDTest.Peek(0x4a8f2c, 'i'), SDTest.Peek(0x4ad7e8, 'i'), SDTest.Peek(0x4ad7ec, 'i'))
dev = rdfl[1]
SDTest.Poke(0x4d6dc0, 0)
SDTest.Poke(0x4a8f2c, 0)
SDTest.Poke(0x4d6d94, 0)
SDTest.Poke(0x4d6da8, 0)
flpokef(0x4329a4 + 0x400000, (2.0, 0.5))
def flrender(f, t, intens):
	flpokef(0x4a8ed0 + 0x400000, (t,))
	flpokef(f + 0x28, (intens,))
	SDTest.Poke(0x4ad6f4, 0)
	SDTest.Call(0x297e50, 'tviii', f, dev, 0)
	SDTest.Poke(0x4ad6f4, dev)
	return flget(f)
SDTest.Poke(0x4d6d90, 1)
flpokef(0x4329a0 + 0x400000, (50.0,))
for aten in (0, 1):
	SDTest.Poke(0x4d6d90, aten)
	for pos in ((0.0, 0.0, 10.0), (3.0, -2.0, 5.0), (0.0, 0.0, -5.0), (30.0, 0.0, 10.0), (0.0, 0.0, 0.01),
		(0.0, 0.0, 200.0)):
		flpokef(nodo + 0x5c + 48, pos)
		flpokef(f + 0xc, pos)
		for t in (1000.0, 1016.0, 1100.0, 1101.0, 1500.0, 3000000000.0):
			for intens in (1.0, 0.0, 0.3):
				check('CFlare_Render' + fmt((aten, pos, t, intens)), lambda f=f, t=t, intens=intens:
					flrender(f, t, intens))
SDTest.Poke(0x4ad7e8, 640)
SDTest.Poke(0x4ad7ec, 480)
flpokef(f + 0x18, (0.5, 0.25))
check('CFlare_Draw', lambda f=f: rdlog(lambda f=f: SDTest.Call(0x297e30, 'tvi', f)))
SDTest.Call(0x3197d0, 'iiFi', 0x4ae3f8 + 0x400000, (0.0,) * 16, 64)
for i in range(16):
	SDTest.Poke(0x4ae3f8 + 4 * i, rdproj[i])
	SDTest.Poke(0x4ae4b8 + 4 * i, rdviewm[i])
for addr, v in ((0x4a8ed0, rdfl[0]), (0x4d6d90, rdfl[2]), (0x4329a0, rdfl[3]), (0x4329a4, rdfl[4]),
	(0x4329a8, rdfl[5]), (0x4d6d94, rdfl[6]), (0x4d6da8, rdfl[7]), (0x4d6dc0, rdfl2[0]), (0x4a8f2c, rdfl2[1]),
	(0x4ad7e8, rdfl2[2]), (0x4ad7ec, rdfl2[3])):
	SDTest.Poke(addr, v)
SDTest.Poke(f + 8 - 0x400000, 0)
check('CFlare_Delete', lambda f=f: SDTest.Call(0x297da0, 'tiii', f, 1)[0] == f)
