# game ScrapEng/ScrapEngWorld.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the identity matrix, the
# D3D color of a color with intensity, RenderMode (every mode read, the ones that can be put back also set), the free
# file index, the box of a fake CSuelo, the clear of a 640x480 rectangle through the D3D log, the start of the world
# node walk, and the early returns of the splash screens and the loading bar (no frame is drawn: the splash files are
# missing or the switches are off). All engine variables poked are put back.
def swz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def swi(a):
	return SDTest.Peek(a, '@i')
def swset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def swlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r[0], SDTest.Out())

check('Matrix_LoadIdentity', lambda: SDTest.Call('Matrix_LoadIdentity', 'iF', (7.0,) * 16)[1])
swm = swz(0x40)
check('Matrix_LoadIdentity ret', lambda: (SDTest.Call('Matrix_LoadIdentity', 'ii', swm)[0] == swm, swi(swm), swi(swm + 4)))
check('Matrix_Self', lambda: SDTest.Call('Matrix_Self', 'ii', swm)[0] == swm)

swc = swz(8)
for rgba in (0, -0x7fbfdff0, -1, 0x12345678):
	for k in (1.0, 0.5, 0.0, 2.0, -1.0, 1.0000001, 300.0):
		SDTest.Poke(swc - 0x400000, rgba)
		swset(swc + 4, (k,))
		check('Color_ToD3D' + fmt((rgba, k)), lambda: SDTest.Call('Color_ToD3D', 'ii', swc)[0])

for old in (0, 1, 0x80, 0x100):
	for v in (0, 1, -5, 0x100):
		for st in (0, 1):
			check('Bool_GetSet' + fmt((old, v, st)), lambda old=old, v=v, st=st: SDTest.Call('Bool_GetSet', 'ipii', old, v,
				st))

# RenderMode: the profiler mode, the tile switch and the vertex specular share 0x4a9018 and the bytes from 0x4a8efb
# (g_TileOn and the three byte flags after it, separate C objects in ScrapC.exe: read and written as the span)
swprof = SDTest.Peek('g_ProfMode', 'i')
swbytes = SDTest.PeekSpan('g_TileOn', 0, 4)
swfilter = SDTest.Peek('g_TexFilter', 'i')
for mode in (0x10, 0x20, 0x100, 0x101, 0x102, 0x103, 0x7, 0x11, 0x104, -1):
	check('RenderMode get' + fmt((mode,)), lambda mode=mode: SDTest.Call('RenderMode', 'iiii', mode, 0, 0)[0])
for mode, v in ((0x10, 0), (0x10, 0), (0x10, 0), (0x10, 0), (0x100, 0), (0x100, 5), (0x100, 0), (0x101, 0), (0x101, 0),
	(0x102, 0), (0x102, 0), (0x102, 0), (0x20, 0), (0x7, 1)):
	check('RenderMode set' + fmt((mode, v)), lambda mode=mode, v=v: (SDTest.Call('RenderMode', 'iiii', mode, v, 1)[0],
		SDTest.Peek('g_ProfMode', 'i'), SDTest.PeekSpan('g_TileOn', 0, 4), SDTest.Peek('g_TexFilter', 'i')))
SDTest.Poke('g_ProfMode', swprof)
SDTest.PokeSpan('g_TileOn', 0, swbytes, 4)
SDTest.Poke('g_TexFilter', swfilter)

for f in ('sdnofile%d.xyz', 'sdcall.py', 'sd%dcall.py'):
	check('File_FreeIndex' + fmt((f,)), lambda f=f: SDTest.Call('File_FreeIndex', 'vsp', f, -1)[1])

sws = swz(0x70)
swset(sws + 0x54, (1.0, -2.5, 3.25, 100.0, 200.0, -0.0))
check('CSuelo_GetCollideExt', lambda: SDTest.Call('CSuelo_GetCollideExt', 'tiiF', sws, (0.0,) * 6)[1])

for args in ((0.0, 0.0, 640.0, 480.0, 1, 0x102030), (10.0, 20.0, 100.0, 50.0, 0, 0xff0000), (-5.0, 470.0, 30.0, 30.0, 7,
	0)):
	check('Render_ClearRect' + fmt(args), lambda args=args: swlog(lambda args=args: apply(SDTest.Call, (sym('Render_ClearRect'),
		'bffffii') + args)))

# the walk state after the start; WorldInfo_Reset CHECKs that no 2D sprite is left
def swstate():
	return (SDTest.Peek('g_WorldInfoNode', 'i') != 0, SDTest.Peek('g_WorldInfoDummy', 'i') != 0, SDTest.Peek('g_WorldInfoWild', 'b'),
		SDTest.Peek('g_WorldInfoPattern', 's'))
swwalk = swstate()
for pat, wild in (('DM_Abc*', 1), ('DM_Abc*', 0), ('', 1)):
	check('WorldInfo_Begin' + fmt((pat, wild)), lambda pat=pat, wild=wild: (SDTest.Call('WorldInfo_Begin', 'vsi', pat, wild),
		swstate()))
check('WorldInfo_Begin NULL', lambda: (SDTest.Call('WorldInfo_Begin', 'vii', 0, 1), swstate()))
if SDTest.Peek('g_CountSprite2D', 'i') == 0:
	check('WorldInfo_Reset', lambda: (SDTest.Call('WorldInfo_Reset', 'v'), swstate()))
SDTest.Call('WorldInfo_Begin', 'vsi', swwalk[3], swwalk[2])

# the splash screens: off, then a missing file (only the name is kept); the last name put back
swlast = SDTest.Peek('g_SplashLast', 's')
swon = SDTest.Peek('g_ShowSplashScreen', 'i')
swtime = SDTest.Peek('g_SplashTime', 'i')
SDTest.Poke('g_ShowSplashScreen', 0)
for fn in ('Splash_Black', 'Splash_Promos', 'Splash_Logos', 'Splash_Reset'):
	check('Splash off' + fmt((fn,)), lambda fn=fn: (SDTest.Call(fn, 'v'), SDTest.Peek('g_SplashLast', 's')))
SDTest.Poke('g_ShowSplashScreen', 1)
for name in ('sdnosplash.dds', '', 'sdnosplash2.dds'):
	check('Splash_Show missing' + fmt((name,)), lambda name=name: (SDTest.Call('Splash_Show', 'vsiii', name, 1, 100, 1),
		SDTest.Peek('g_SplashLast', 's'), SDTest.Peek('g_SplashTime', 'i') == swtime))
SDTest.Poke('g_ShowSplashScreen', swon)
SDTest.Call('crt_memcpy', 'iisi', symva('g_SplashLast'), swlast, len(swlast) + 1)

# the loading bar without the device switch (only the step count), the loading screen switch off
swload = (SDTest.Peek('g_DrawProgressBar', 'i'), SDTest.Peek('g_LoadingBar', 'i'), SDTest.Peek('g_ProgressBarCount', 'i'), SDTest.Peek('g_ProgressBar', 'i'),
	SDTest.Peek('g_VerUniones', 'i'), SDTest.Peek('g_CallbacksOn', 'i'))
SDTest.Poke('g_CallbacksOn', swload[5] & ~0xff)
check('Loading_DrawBack off', lambda: SDTest.Call('Loading_DrawBack', 'v'))
check('Loading_DrawBack2 off', lambda: SDTest.Call('Loading_DrawBack2', 'v'))
SDTest.Poke('g_DrawProgressBar', swload[0] & ~0xff)
check('Loading_Step off', lambda: (SDTest.Call('Loading_Step', 'vi', -1), SDTest.Peek('g_ProgressBarCount', 'i') == swload[2]))
SDTest.Poke('g_DrawProgressBar', (swload[0] & ~0xff) | 1)
SDTest.Poke('g_LoadingBar', 1)
SDTest.Poke('g_ProgressBar', 50)
SDTest.Poke('g_VerUniones', swload[4] & ~0xff00)
SDTest.Poke('g_ProgressBarCount', -3)
for k in range(20):
	check('Loading_Step -1' + fmt((k,)), lambda: (SDTest.Call('Loading_Step', 'vi', -1), SDTest.Peek('g_ProgressBarCount', 'i')))
check('Loading_Step 50', lambda: (SDTest.Call('Loading_Step', 'vi', 50), SDTest.Peek('g_ProgressBarCount', 'i')))
SDTest.Poke('g_DrawProgressBar', swload[0])
SDTest.Poke('g_LoadingBar', swload[1])
SDTest.Poke('g_ProgressBarCount', swload[2])
SDTest.Poke('g_ProgressBar', swload[3])
SDTest.Poke('g_VerUniones', swload[4])
SDTest.Poke('g_CallbacksOn', swload[5])
