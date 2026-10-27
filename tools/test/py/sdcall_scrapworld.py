# game ScrapEng/ScrapEngWorld.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the identity matrix, the
# D3D color of a color with intensity, RenderMode (every mode read, the ones that can be put back also set), the free
# file index, the box of a fake CSuelo, the clear of a 640x480 rectangle through the D3D log, the start of the world
# node walk, and the early returns of the splash screens and the loading bar (no frame is drawn: the splash files are
# missing or the switches are off). All engine variables poked are put back.
def swz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def swi(a):
	return SDTest.Peek(a, '@i')
def swset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def swlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r[0], SDTest.Out())

check('Matrix_LoadIdentity', lambda: SDTest.Call(0x214710, 'iF', (7.0,) * 16)[1])
swm = swz(0x40)
check('Matrix_LoadIdentity ret', lambda: (SDTest.Call(0x214710, 'ii', swm)[0] == swm, swi(swm), swi(swm + 4)))
check('Matrix_Self', lambda: SDTest.Call(0x215150, 'ii', swm)[0] == swm)

swc = swz(8)
for rgba in (0, -0x7fbfdff0, -1, 0x12345678):
	for k in (1.0, 0.5, 0.0, 2.0, -1.0, 1.0000001, 300.0):
		SDTest.Poke(swc - 0x400000, rgba)
		swset(swc + 4, (k,))
		check('Color_ToD3D' + fmt((rgba, k)), lambda: SDTest.Call(0x214f70, 'ii', swc)[0])

for old in (0, 1, 0x80, 0x100):
	for v in (0, 1, -5, 0x100):
		for st in (0, 1):
			check('Bool_GetSet' + fmt((old, v, st)), lambda old=old, v=v, st=st: SDTest.Call(0x214a10, 'ipii', old, v,
				st))

# RenderMode: the profiler mode, the tile switch and the vertex specular share 0x4a9018 and the bytes from 0x4a8efb
swprof = SDTest.Peek(0x4a9018, 'i')
swbytes = SDTest.Peek(0x4a8efb, 'i')
swfilter = SDTest.Peek(0x4add58, 'i')
for mode in (0x10, 0x20, 0x100, 0x101, 0x102, 0x103, 0x7, 0x11, 0x104, -1):
	check('RenderMode get' + fmt((mode,)), lambda mode=mode: SDTest.Call(0x214a30, 'iiii', mode, 0, 0)[0])
for mode, v in ((0x10, 0), (0x10, 0), (0x10, 0), (0x10, 0), (0x100, 0), (0x100, 5), (0x100, 0), (0x101, 0), (0x101, 0),
	(0x102, 0), (0x102, 0), (0x102, 0), (0x20, 0), (0x7, 1)):
	check('RenderMode set' + fmt((mode, v)), lambda mode=mode, v=v: (SDTest.Call(0x214a30, 'iiii', mode, v, 1)[0],
		SDTest.Peek(0x4a9018, 'i'), SDTest.Peek(0x4a8efb, 'i'), SDTest.Peek(0x4add58, 'i')))
SDTest.Poke(0x4a9018, swprof)
SDTest.Poke(0x4a8efb, swbytes)
SDTest.Poke(0x4add58, swfilter)

for f in ('sdnofile%d.xyz', 'sdcall.py', 'sd%dcall.py'):
	check('File_FreeIndex' + fmt((f,)), lambda f=f: SDTest.Call(0x214cf0, 'vsp', f, -1)[1])

sws = swz(0x70)
swset(sws + 0x54, (1.0, -2.5, 3.25, 100.0, 200.0, -0.0))
check('CSuelo_GetCollideExt', lambda: SDTest.Call(0x2149f0, 'tiiF', sws, (0.0,) * 6)[1])

for args in ((0.0, 0.0, 640.0, 480.0, 1, 0x102030), (10.0, 20.0, 100.0, 50.0, 0, 0xff0000), (-5.0, 470.0, 30.0, 30.0, 7,
	0)):
	check('Render_ClearRect' + fmt(args), lambda args=args: swlog(lambda args=args: apply(SDTest.Call, (0x214ee0,
		'bffffii') + args)))

# the walk state after the start; WorldInfo_Reset CHECKs that no 2D sprite is left
def swstate():
	return (SDTest.Peek(0x4a81f4, 'i') != 0, SDTest.Peek(0x4a81f8, 'i') != 0, SDTest.Peek(0x4a81fc, 'b'),
		SDTest.Peek(0x4a8200, 's'))
swwalk = swstate()
for pat, wild in (('DM_Abc*', 1), ('DM_Abc*', 0), ('', 1)):
	check('WorldInfo_Begin' + fmt((pat, wild)), lambda pat=pat, wild=wild: (SDTest.Call(0x214460, 'vsi', pat, wild),
		swstate()))
check('WorldInfo_Begin NULL', lambda: (SDTest.Call(0x214460, 'vii', 0, 1), swstate()))
if SDTest.Peek(0x4a81f0, 'i') == 0:
	check('WorldInfo_Reset', lambda: (SDTest.Call(0x214790, 'v'), swstate()))
SDTest.Call(0x214460, 'vsi', swwalk[3], swwalk[2])

# the splash screens: off, then a missing file (only the name is kept); the last name put back
swlast = SDTest.Peek(0x4a8800, 's')
swon = SDTest.Peek(0x4a85c8, 'i')
swtime = SDTest.Peek(0x4a8904, 'i')
SDTest.Poke(0x4a85c8, 0)
for fn in (0x215170, 0x2151c0, 0x215220, 0x215a10):
	check('Splash off' + fmt((fn,)), lambda fn=fn: (SDTest.Call(fn, 'v'), SDTest.Peek(0x4a8800, 's')))
SDTest.Poke(0x4a85c8, 1)
for name in ('sdnosplash.dds', '', 'sdnosplash2.dds'):
	check('Splash_Show missing' + fmt((name,)), lambda name=name: (SDTest.Call(0x215280, 'vsiii', name, 1, 100, 1),
		SDTest.Peek(0x4a8800, 's'), SDTest.Peek(0x4a8904, 'i') == swtime))
SDTest.Poke(0x4a85c8, swon)
SDTest.Call(0x3197d0, 'iisi', 0x8a8800, swlast, len(swlast) + 1)

# the loading bar without the device switch (only the step count), the loading screen switch off
swload = (SDTest.Peek(0x4a8324, 'i'), SDTest.Peek(0x4a8598, 'i'), SDTest.Peek(0x4a8328, 'i'), SDTest.Peek(0x4a86d8, 'i'),
	SDTest.Peek(0x4ad6f0, 'i'), SDTest.Peek(0x431ae4, 'i'))
SDTest.Poke(0x431ae4, swload[5] & ~0xff)
check('Loading_DrawBack off', lambda: SDTest.Call(0x2159b0, 'v'))
check('Loading_DrawBack2 off', lambda: SDTest.Call(0x215740, 'v'))
SDTest.Poke(0x4a8324, swload[0] & ~0xff)
check('Loading_Step off', lambda: (SDTest.Call(0x2157a0, 'vi', -1), SDTest.Peek(0x4a8328, 'i') == swload[2]))
SDTest.Poke(0x4a8324, (swload[0] & ~0xff) | 1)
SDTest.Poke(0x4a8598, 1)
SDTest.Poke(0x4a86d8, 50)
SDTest.Poke(0x4ad6f0, swload[4] & ~0xff00)
SDTest.Poke(0x4a8328, -3)
for k in range(20):
	check('Loading_Step -1' + fmt((k,)), lambda: (SDTest.Call(0x2157a0, 'vi', -1), SDTest.Peek(0x4a8328, 'i')))
check('Loading_Step 50', lambda: (SDTest.Call(0x2157a0, 'vi', 50), SDTest.Peek(0x4a8328, 'i')))
SDTest.Poke(0x4a8324, swload[0])
SDTest.Poke(0x4a8598, swload[1])
SDTest.Poke(0x4a8328, swload[2])
SDTest.Poke(0x4a86d8, swload[3])
SDTest.Poke(0x4ad6f0, swload[4])
SDTest.Poke(0x431ae4, swload[5])
