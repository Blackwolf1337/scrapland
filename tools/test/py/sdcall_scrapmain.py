# game ScrapEng/ScrapEngMain.c and DebugUtils.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the
# video modes, the engine options, the callbacks, the m3d.ini helpers on an ini of the packs, world to screen with
# known matrices, the engine printf, the 640 x 480 view rect, the frame setup and the DebugUtils lines through the
# D3D log. Every engine variable poked is put back. Not called: anything that changes the video mode, plays a video
# or music, shows a loading screen, writes a file (screenshots, AVI, camera.set) or opens a window (Model_Info), the
# engine start and end, the level functions, the reset of the device and the debug keys (they switch the camera).
def smints(rva, n):
	r = []
	for i in range(n):
		r.append(SDTest.Peek(rva + 4 * i, 'i'))
	return r
def smpoke(rva, v):
	for i in range(len(v)):
		SDTest.Poke(rva + 4 * i, v[i])
def smfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def smset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def smstr(rva, s):
	SDTest.Call(0x3197d0, 'iisi', rva + 0x400000, s, len(s) + 1)
def smz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def smlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())

for f in (0, 20, 21, 22, 23, 24, 25, 26, 101, -1):
	check('Format_Bpp' + fmt((f,)), lambda f=f: SDTest.Call(0x215c50, 'ii', f)[0])
check('Eng_Identity', lambda: (SDTest.Call(0x216ad0, 'ii', 5)[0], SDTest.Call(0x216ad0, 'ii', -7)[0]))
check('Camera_GetActual', lambda: SDTest.Call(0x215c00, 'i')[0] == SDTest.Peek(0x4a8308, 'i'))
check('Video_GetStatus', lambda: SDTest.Call(0x215e60, 'i')[0])
check('Engine_IsFullScreen', lambda: SDTest.Call(0x216480, 'b')[0])
check('Keys_SkipPressed', lambda: SDTest.Call(0x215c10, 'b')[0])
check('Video_GetCurrentMode', lambda: SDTest.Call(0x215cb0, 'i')[0])
smn = SDTest.Call(0x215d20, 'iii', -1, 0)[0]
check('Video_GetModes', lambda: (smn, SDTest.Call(0x215d20, 'iiz', 0, None)[0]))
for i in range(min(smn, 40)):
	check('Video_GetModes' + fmt((i,)), lambda i=i: SDTest.Call(0x215d20, 'iib', i, ''))
check('Video_PlayStart 0', lambda: SDTest.Call(0x216d10, 'bi', 0)[0])
check('Video_Play bad', lambda: SDTest.Call(0x218010, 'bzsf', None, 'sdx', 0.0)[0])
check('Music_Play bad', lambda: SDTest.Call(0x216ae0, 'bzsf', None, 'sdx', 0.5)[0])
smvol = (smfl(0x831d04, 1)[0], smfl(0x866c70, 1)[0])
smvoli = (SDTest.Peek(0x431d04, 'i'), SDTest.Peek(0x431d18, 'i'), SDTest.Peek(0x466c70, 'i'))
check('Music_Play volume', lambda: (SDTest.Call(0x216ae0, 'bzsf', None, 'volume', smvol[0])[0], smfl(0x831d04, 1),
	smfl(0x831d18, 1)))
check('Music_Play mainvol', lambda: (SDTest.Call(0x216ae0, 'bzsf', None, 'MainVol', smvol[1])[0], smfl(0x866c70, 1)))
SDTest.Poke(0x431d04, smvoli[0])
SDTest.Poke(0x431d18, smvoli[1])
SDTest.Poke(0x466c70, smvoli[2])

# the options of Engine_Option: get all, set and toggle the harmless ones (0x20 is the screenshot flag)
for o in (0x10, 0x20, 0x100, 0x101, 0x102, 0x103, 0x30):
	check('Engine_GetOption' + fmt((o,)), lambda o=o: SDTest.Call(0x215c90, 'ii', o)[0])
smopt = (SDTest.Peek(0x4a9018, 'i'), SDTest.Peek(0x4a8efc, 'i'))
for o in (0x10, 0x100, 0x101):
	for v in (0, 1):
		check('Engine_SetOption' + fmt((o, v)), lambda o=o, v=v: (SDTest.Call(0x217b90, 'iii', o, v)[0],
			SDTest.Peek(0x4a9018, 'i'), SDTest.Peek(0x4a8efc, 'i') & 0xffff))
	check('Engine_ToggleOption' + fmt((o,)), lambda o=o: (SDTest.Call(0x217fd0, 'ii', o)[0], SDTest.Peek(0x4a9018, 'i'),
		SDTest.Peek(0x4a8efc, 'i') & 0xffff))
SDTest.Poke(0x4a9018, smopt[0])
SDTest.Poke(0x4a8efc, smopt[1])

smcb = smints(0x4a8314, 4)
for i in range(4):
	check('Engine_SetCallback' + fmt((i + 1,)), lambda i=i: (SDTest.Call(0x217b20, 'vii', 0, i + 1),
		smints(0x4a8314, 4) == smcb, SDTest.Peek(0x4a8314 + 4 * i, 'i'), SDTest.Call(0x217b20, 'vii', smcb[i], i + 1),
		smints(0x4a8314, 4) == smcb))
smpoke(0x4a8314, smcb)

# R_EnvMap through its variable object
smenv = SDTest.Peek(0x4adda0, 'i')
for v in (1, 0, smenv):
	check('GVar_SetIntVerbose' + fmt((v,)), lambda v=v: (SDTest.Call(0x217ae0, 'vii', 0x8add90, v),
		SDTest.Peek(0x4adda0, 'i')))
SDTest.Poke(0x4adda0, smenv)

smini = SDTest.Peek(0x431bf0, 's')
for n in ('sdtest', 'levels/sd/sd2.txt', 'C:\\sd\\x', ''):
	check('Engine_SetIniName' + fmt((n,)), lambda n=n: (SDTest.Call(0x217b00, 'is', n)[0] == 0x831bf0,
		SDTest.Peek(0x431bf0, 's')))
	smstr(0x431bf0, smini)
check('Engine_SetIniName back', lambda: SDTest.Peek(0x431bf0, 's'))
smsuelo = (SDTest.Peek(0x4a8440, 's'), smints(0x431af8, 3))
for d, n, e in (('levels/sd', 'map', (1.0, 2.0, 3.0)), ('', '', (-0.5, 1e10, 0.0))):
	check('Engine_SetSuelo' + fmt((d, n, e)), lambda d=d, n=n, e=e: (SDTest.Call(0x217dd0, 'vssF', d, n, e),
		SDTest.Peek(0x4a8440, 's'), smfl(0x831af8, 3)))
smstr(0x4a8440, smsuelo[0])
smpoke(0x431af8, smsuelo[1])
check('Engine_SetSuelo back', lambda: (SDTest.Peek(0x4a8440, 's'), smints(0x431af8, 3) == smsuelo[1]))

for p, n in (('*.ini', 'm3d.ini'), ('M3D*', 'm3d.ini'), ('[a-c]?x', 'bzx'), ('', ''), ('a*b', 'axxb'), ('a*b', 'axxc')):
	check('Wild_MatchCase' + fmt((p, n)), lambda p=p, n=n: SDTest.Call(0x218700, 'bssi', p, n, 0)[0])

# the m3d.ini helpers on m3d.ini of the packs with some keys of their own
smi = SDTest.Call(0x261980, 'tii', smz(4))[0]
check('CArchINI_Load m3d.ini', lambda: SDTest.Call(0x2621c0, 'tiiszi', smi, 'm3d.ini', None, 0)[0])
for k, v in (('c1', '10 20 30 0.5'), ('c2', '255,128,0,2'), ('c3', 'abc'), ('c4', '300 -1 7 1e3')):
	SDTest.Call(0x2621a0, 'tiisss', smi, 'sd', k, v)
smcol = smz(8)
for s, k in (('sd', 'c1'), ('sd', 'c2'), ('sd', 'c3'), ('sd', 'c4'), ('suelo', 'model_ambient'), ('niebla', 'color'),
		('none', 'x')):
	txt = SDTest.Call(0x2624b0, 'tiissbi', smi, s, k, '', 0x40)
	check('Ini text' + fmt((s, k)), lambda txt=txt: txt)
	# only the texts with four values: another count is a CHECK
	n = 0
	if txt[0] == 0:
		n = SDTest.Call(0x265260, 'issiiii', txt[1], '&b&b&b&f', smz(8), smz(8), smz(8), smz(8))[0]
	if txt[0] != 0 or n == 4:
		SDTest.Call(0x3197d0, 'iiFi', smcol, (0.0, 0.0), 8)
		check('Ini_GetColor' + fmt((s, k)), lambda s=s, k=k: (SDTest.Call(0x218740, 'vssii', s, k, smcol, smi),
			SDTest.Peek(smcol, '@i'), smfl(smcol + 4, 1)))
	for base in ('levels/sd/m3d.ini', 'C:\\sd\\x.ini', ''):
		for size in (0x104, 8):
			check('Ini_GetPath' + fmt((s, k, base, size)), lambda s=s, k=k, base=base, size=size:
				SDTest.Call(0x2187c0, 'issbisi', s, k, '', size, base, smi))
SDTest.Call(0x261990, 'tvi', smi)

check('DVF_Process name', lambda: (SDTest.Call(0x2175d0, 'vsi', 'sddvf', 0), SDTest.Peek(0x4b2840, 's'),
	SDTest.Call(0x2175d0, 'vzi', None, 7), SDTest.Peek(0x4b2840, 's')))
smload = SDTest.Peek(0x431b08, 'i')
check('Loading_RandomName', lambda: SDTest.Call(0x2178e0, 'vbi', '', 0x104))
check('Loading_RandomName short', lambda: SDTest.Call(0x2178e0, 'vbi', '', 6))
SDTest.Poke(0x431b08, smload)
smcam = smz(0x3c)
check('Camera_SaveRestore read', lambda: (SDTest.Call(0x216840, 'vii', smcam, 0), smfl(smcam, 9)))

# world to screen with a known view and projection (put back after)
smmats = (smints(0x4ae4b8, 16), smints(0x4ae3f8, 16))
smset(0x8ae4b8, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
smset(0x8ae3f8, (1.5, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -1.0, 0.0))
for p in ((0.0, 0.0, 10.0), (3.0, -2.0, 25.0), (100.0, 0.0, 10.0), (0.0, -50.0, 10.0), (0.0, 0.0, -10.0),
		(0.0, 0.0, 0.0), (5.0, 5.0, 0.0), (-0.0, 0.0, 1e-30), (1e30, 1.0, 2.0)):
	check('Engine_WorldToScreen' + fmt(p), lambda p=p: (SDTest.Call(0x216d60, 'bFFF', p, (0.0, 0.0), (0.0,)),
		SDTest.Call(0x216d60, 'bFii', p, 0, 0)[0], SDTest.Call(0x216d60, 'bFiF', p, 0, (7.0,))[2]))
	check('Engine_DirToScreen' + fmt(p), lambda p=p: SDTest.Call(0x216f20, 'vFF', p, (0.0, 0.0))[2])
smpoke(0x4ae4b8, smmats[0])
smpoke(0x4ae3f8, smmats[1])

# the text functions: the cursor of Texto.c and the state of the two fonts of Eng_Printf
def smcur():
	return (SDTest.Peek(0x4b1cf0, 'i'), SDTest.Peek(0x4b1cf4, 'i'))
smcur0 = smcur()
check('Eng_PrintfXY', lambda: (SDTest.Call(0x216cb0, 'viisi', 100, 50, 'sd %d', 7), smcur()))
check('Eng_PrintfXY none', lambda: (SDTest.Call(0x216cb0, 'viiz', 7, 9, None), smcur()))
check('Eng_PrintfRel', lambda: (SDTest.Call(0x216c50, 'viiss', 10, -20, 'rel %s', 'x'), smcur()))
for p in ((0.0, 0.0, 0.0), (10.0, 50.0, 300.0)):
	check('Eng_Printf3D' + fmt(p), lambda p=p: (SDTest.Call(0x216bf0, 'vFsi', p, '3d %d', 3), smcur()))
check('Eng_Printf3D none', lambda: (SDTest.Call(0x216bf0, 'vFz', (1.0, 2.0, 3.0), None), smcur()))
SDTest.Poke(0x4b1cf0, smcur0[0])
SDTest.Poke(0x4b1cf4, smcur0[1])
smfonts = (SDTest.Peek(0x4a8910, 'i'), SDTest.Peek(0x4a891c, 'i'))
if smfonts[0] and smfonts[1]:
	smfsave = (smints(smfonts[0] - 0x400000, 13), smints(smfonts[1] - 0x400000, 13))
	for fnt in (1, 2):
		for align in (0, 1, 2, 3):
			check('Eng_Printf' + fmt((fnt, align)), lambda fnt=fnt, align=align: (SDTest.Call(0x2170d0, 'viffiisi', fnt,
				100.0, 200.0, align, 0xff8040, 'sd %d', 42), smints(smfonts[fnt - 1] - 0x400000, 6)))
	smpoke(smfonts[0] - 0x400000, smfsave[0])
	smpoke(smfonts[1] - 0x400000, smfsave[1])

# the view in 640 x 480 coordinates, then the whole view again; the viewport, projection and frustum put back
smvp = (smints(0x4ad7e0, 6), smints(0x4ae3f8, 16), smints(0x4adbf0, 24), smints(0x4ad7f8, 6))
for r in ((0.0, 0.0, 640.0, 480.0), (64.0, 48.0, 320.0, 240.0), (600.0, 10.0, 100.0, 100.0)):
	check('Eng_SetViewRect' + fmt(r), lambda r=r: smlog(lambda r=r: SDTest.Call(0x217be0, 'bffff', r[0], r[1], r[2],
		r[3])[0]))
check('Eng_ResetViewRect', lambda: smlog(lambda: SDTest.Call(0x2177c0, 'b')[0]))
smpoke(0x4ad7e0, smvp[0])
smpoke(0x4ae3f8, smvp[1])
smpoke(0x4adbf0, smvp[2])
smpoke(0x4ad7f8, smvp[3])

# the frame setup (as the engine does it every frame) when no callback would run
def smsetup():
	return (SDTest.Peek(0x4a8f70, 'i'), SDTest.Peek(0x4b2b30, 'i'), SDTest.Peek(0x4b2b28, 'i'), SDTest.Peek(0x4b2b2c, 'i'),
		SDTest.Peek(0x432418, 'b'), SDTest.Peek(0x431bac, 'b'), SDTest.Peek(0x4ad748, 'i'), smfl(0x83241c, 1))
if SDTest.Peek(0x4a8318, 'i') == 0 or SDTest.Peek(0x431ae4, 'b') == 0:
	check('Engine_FrameSetup', lambda: (smlog(lambda: SDTest.Call(0x2177d0, 'b')[0]), smsetup()))

# DebugUtils.cpp: the lines through the D3D log
smm = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 10.0, 20.0, 30.0, 1.0)
sma = (0.0, 1.0, 2.0)
smb = (3.0, -4.0, 5.5)
smo = (-0.0, 0.0, -0.0)
check('DebugUtils_DrawLine', lambda: smlog(lambda: SDTest.Call(0x219760, 'vFFi', sma, smb, 0xff00ff)))
check('DebugUtils_DrawLineM', lambda: smlog(lambda: SDTest.Call(0x219740, 'vFFFi', smm, sma, smb, 0x123456)))
check('DebugUtils_DrawBox', lambda: smlog(lambda: SDTest.Call(0x219720, 'vFFi', (-1.0, -2.0, -3.0), (1.0, 2.0, 3.0),
	0xff)))
check('DebugUtils_DrawBoxM', lambda: smlog(lambda: SDTest.Call(0x219400, 'vFFFi', smm, smo, smb, 0xff0000)))
check('DebugUtils_DrawTri', lambda: smlog(lambda: SDTest.Call(0x219d90, 'vFFFi', sma, smb, smo, 0x8080)))
check('DebugUtils_DrawTriM', lambda: smlog(lambda: SDTest.Call(0x219d50, 'vFFFFi', smm, sma, smb, smo, 0x8080)))
for p, s in ((sma, 4.0), (smo, 1.5), (smb, -2.0)):
	check('DebugUtils_DrawAxes' + fmt((p, s)), lambda p=p, s=s: smlog(lambda p=p, s=s: SDTest.Call(0x219010, 'vFFf',
		smm, p, s)))
	check('DebugUtils_DrawPointM' + fmt((p, s)), lambda p=p, s=s: smlog(lambda p=p, s=s: SDTest.Call(0x219160, 'vFFfi',
		smm, p, s, 0xabcdef)))
	check('DebugUtils_DrawPoint' + fmt((p, s)), lambda p=p, s=s: smlog(lambda p=p, s=s: SDTest.Call(0x2192b0, 'vFfi', p,
		s, 7)))
