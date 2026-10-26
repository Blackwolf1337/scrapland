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
