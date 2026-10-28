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
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def smset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def smstr(rva, s):
	SDTest.Call('crt_memcpy', 'iisi', rva + 0x400000, s, len(s) + 1)
def smz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def smlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())

for f in (0, 20, 21, 22, 23, 24, 25, 26, 101, -1):
	check('Format_Bpp' + fmt((f,)), lambda f=f: SDTest.Call('Format_Bpp', 'ii', f)[0])
check('Eng_Identity', lambda: (SDTest.Call('Eng_Identity', 'ii', 5)[0], SDTest.Call('Eng_Identity', 'ii', -7)[0]))
check('Camera_GetActual', lambda: SDTest.Call('Camera_GetActual', 'i')[0] == SDTest.Peek('g_CamActual', 'i'))
check('Video_GetStatus', lambda: SDTest.Call('Video_GetStatus', 'i')[0])
check('Engine_IsFullScreen', lambda: SDTest.Call('Engine_IsFullScreen', 'b')[0])
check('Keys_SkipPressed', lambda: SDTest.Call('Keys_SkipPressed', 'b')[0])
check('Video_GetCurrentMode', lambda: SDTest.Call('Video_GetCurrentMode', 'i')[0])
smn = SDTest.Call('Video_GetModes', 'iii', -1, 0)[0]
check('Video_GetModes', lambda: (smn, SDTest.Call('Video_GetModes', 'iiz', 0, None)[0]))
for i in range(min(smn, 40)):
	check('Video_GetModes' + fmt((i,)), lambda i=i: SDTest.Call('Video_GetModes', 'iib', i, ''))
check('Video_PlayStart 0', lambda: SDTest.Call('Video_PlayStart', 'bi', 0)[0])
check('Video_Play bad', lambda: SDTest.Call('Video_Play', 'bzsf', None, 'sdx', 0.0)[0])
check('Music_Play bad', lambda: SDTest.Call('Music_Play', 'bzsf', None, 'sdx', 0.5)[0])
smvol = (smfl(symva('g_MusicVolume'), 1)[0], smfl(symva('g_MusicMainVolume'), 1)[0])
smvoli = (SDTest.Peek('g_MusicVolume', 'i'), SDTest.Peek('g_MusicFadeTarget', 'i'), SDTest.Peek('g_MusicMainVolume', 'i'))
check('Music_Play volume', lambda: (SDTest.Call('Music_Play', 'bzsf', None, 'volume', smvol[0])[0], smfl(symva('g_MusicVolume'), 1),
	smfl(symva('g_MusicFadeTarget'), 1)))
check('Music_Play mainvol', lambda: (SDTest.Call('Music_Play', 'bzsf', None, 'MainVol', smvol[1])[0], smfl(symva('g_MusicMainVolume'), 1)))
SDTest.Poke('g_MusicVolume', smvoli[0])
SDTest.Poke('g_MusicFadeTarget', smvoli[1])
SDTest.Poke('g_MusicMainVolume', smvoli[2])

# the options of Engine_Option: get all, set and toggle the harmless ones (0x20 is the screenshot flag)
for o in (0x10, 0x20, 0x100, 0x101, 0x102, 0x103, 0x30):
	check('Engine_GetOption' + fmt((o,)), lambda o=o: SDTest.Call('Engine_GetOption', 'ii', o)[0])
smopt = (SDTest.Peek('g_ProfMode', 'i'), SDTest.Peek('g_VertexSpecular', 'i'))
for o in (0x10, 0x100, 0x101):
	for v in (0, 1):
		check('Engine_SetOption' + fmt((o, v)), lambda o=o, v=v: (SDTest.Call('Engine_SetOption', 'iii', o, v)[0],
			SDTest.Peek('g_ProfMode', 'i'), SDTest.Peek('g_VertexSpecular', 'i') & 0xffff))
	check('Engine_ToggleOption' + fmt((o,)), lambda o=o: (SDTest.Call('Engine_ToggleOption', 'ii', o)[0], SDTest.Peek('g_ProfMode', 'i'),
		SDTest.Peek('g_VertexSpecular', 'i') & 0xffff))
SDTest.Poke('g_ProfMode', smopt[0])
SDTest.Poke('g_VertexSpecular', smopt[1])

smcb = smints(0x4a8314, 4)
for i in range(4):
	check('Engine_SetCallback' + fmt((i + 1,)), lambda i=i: (SDTest.Call('Engine_SetCallback', 'vii', 0, i + 1),
		smints(0x4a8314, 4) == smcb, SDTest.Peek(0x4a8314 + 4 * i, 'i'), SDTest.Call('Engine_SetCallback', 'vii', smcb[i], i + 1),
		smints(0x4a8314, 4) == smcb))
smpoke(0x4a8314, smcb)

# R_EnvMap through its variable object
smenv = SDTest.Peek('g_EnvMap', 'i')
for v in (1, 0, smenv):
	check('GVar_SetIntVerbose' + fmt((v,)), lambda v=v: (SDTest.Call('GVar_SetIntVerbose', 'vii', symva('g_GvEnvMap'), v),
		SDTest.Peek('g_EnvMap', 'i')))
SDTest.Poke('g_EnvMap', smenv)

smini = SDTest.Peek('str_431bf0', 's')
for n in ('sdtest', 'levels/sd/sd2.txt', 'C:\\sd\\x', ''):
	# with one empty path Ruta_Merge of the exe returns garbage (al 0), the C returns dest: no return then
	check('Engine_SetIniName' + fmt((n,)), lambda n=n: ((SDTest.Call('Engine_SetIniName', 'is', n)[0] == symva('str_431bf0')) or n == '',
		SDTest.Peek('str_431bf0', 's')))
	smstr(sym('str_431bf0'), smini)
check('Engine_SetIniName back', lambda: SDTest.Peek('str_431bf0', 's'))
smsuelo = (SDTest.Peek('g_SueloPath', 's'), smints(sym('g_SueloEscala'), 3))
for d, n, e in (('levels/sd', 'map', (1.0, 2.0, 3.0)), ('', '', (-0.5, 1e10, 0.0))):
	check('Engine_SetSuelo' + fmt((d, n, e)), lambda d=d, n=n, e=e: (SDTest.Call('Engine_SetSuelo', 'vssF', d, n, e),
		SDTest.Peek('g_SueloPath', 's'), smfl(symva('g_SueloEscala'), 3)))
smstr(sym('g_SueloPath'), smsuelo[0])
smpoke(sym('g_SueloEscala'), smsuelo[1])
check('Engine_SetSuelo back', lambda: (SDTest.Peek('g_SueloPath', 's'), smints(sym('g_SueloEscala'), 3) == smsuelo[1]))

for p, n in (('*.ini', 'm3d.ini'), ('M3D*', 'm3d.ini'), ('[a-c]?x', 'bzx'), ('', ''), ('a*b', 'axxb'), ('a*b', 'axxc')):
	check('Wild_MatchCase' + fmt((p, n)), lambda p=p, n=n: SDTest.Call('Wild_MatchCase', 'bssi', p, n, 0)[0])

# the m3d.ini helpers on m3d.ini of the packs with some keys of their own
smi = SDTest.Call('CArchINI_ctor', 'tii', smz(4))[0]
check('CArchINI_Load m3d.ini', lambda: SDTest.Call('CArchINI_Load', 'tiiszi', smi, 'm3d.ini', None, 0)[0])
for k, v in (('c1', '10 20 30 0.5'), ('c2', '255,128,0,2'), ('c3', 'abc'), ('c4', '300 -1 7 1e3')):
	SDTest.Call('CArchINI_SetStr', 'tiisss', smi, 'sd', k, v)
smcol = smz(8)
# Proc_Scan crashes on texts that are not number lists ('abc'), in both runs
def smnums(t):
	for c in t:
		if not c in '0123456789.,- ':
			return 0
	return 1
for s, k in (('sd', 'c1'), ('sd', 'c2'), ('sd', 'c3'), ('sd', 'c4'), ('suelo', 'model_ambient'), ('niebla', 'color'),
		('none', 'x')):
	txt = SDTest.Call('CArchINI_ReadText', 'tiissbi', smi, s, k, '', 0x40)
	check('Ini text' + fmt((s, k)), lambda txt=txt: txt)
	# only the texts with four values: another count is a CHECK
	n = 0
	if txt[0] == 0 and smnums(txt[1]):
		n = SDTest.Call('Proc_Scan', 'issiiii', txt[1], '&b&b&b&f', smz(8), smz(8), smz(8), smz(8))[0]
	if txt[0] != 0 or n == 4:
		SDTest.Call('crt_memcpy', 'iiFi', smcol, (0.0, 0.0), 8)
		check('Ini_GetColor' + fmt((s, k)), lambda s=s, k=k: (SDTest.Call('Ini_GetColor', 'vssii', s, k, smcol, smi),
			SDTest.Peek(smcol, '@i'), smfl(smcol + 4, 1)))
	for base in ('levels/sd/m3d.ini', 'C:\\sd\\x.ini', ''):
		for size in (0x104, 8):
			check('Ini_GetPath' + fmt((s, k, base, size)), lambda s=s, k=k, base=base, size=size:
				SDTest.Call('Ini_GetPath', 'issbisi', s, k, '', size, base, smi))
SDTest.Call('CArchINI_dtor', 'tvi', smi)

check('DVF_Process name', lambda: (SDTest.Call('DVF_Process', 'vsi', 'sddvf', 0), SDTest.Peek('g_DVFName', 's'),
	SDTest.Call('DVF_Process', 'vzi', None, 7), SDTest.Peek('g_DVFName', 's')))
smload = SDTest.Peek('g_LoadingScreen', 'i')
check('Loading_RandomName', lambda: SDTest.Call('Loading_RandomName', 'vbi', '', 0x104))
check('Loading_RandomName short', lambda: SDTest.Call('Loading_RandomName', 'vbi', '', 6))
SDTest.Poke('g_LoadingScreen', smload)
smcam = smz(0x3c)
check('Camera_SaveRestore read', lambda: (SDTest.Call('Camera_SaveRestore', 'vii', smcam, 0), smfl(smcam, 9)))

# world to screen with a known view and projection (put back after)
smmats = (smints(sym('g_DeviceStr') + 0x1c0, 16), smints(sym('g_DeviceStr') + 0x100, 16))
smset(symva('g_DeviceStr') + 0x1c0, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
smset(symva('g_DeviceStr') + 0x100, (1.5, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -1.0, 0.0))
for p in ((0.0, 0.0, 10.0), (3.0, -2.0, 25.0), (100.0, 0.0, 10.0), (0.0, -50.0, 10.0), (0.0, 0.0, -10.0),
		(0.0, 0.0, 0.0), (5.0, 5.0, 0.0), (-0.0, 0.0, 1e-30), (1e30, 1.0, 2.0)):
	check('Engine_WorldToScreen' + fmt(p), lambda p=p: (SDTest.Call('Engine_WorldToScreen', 'bFFF', p, (0.0, 0.0), (0.0,)),
		SDTest.Call('Engine_WorldToScreen', 'bFii', p, 0, 0)[0], SDTest.Call('Engine_WorldToScreen', 'bFiF', p, 0, (7.0,))[2]))
	check('Engine_DirToScreen' + fmt(p), lambda p=p: SDTest.Call('Engine_DirToScreen', 'vFF', p, (0.0, 0.0))[2])
smpoke(sym('g_DeviceStr') + 0x1c0, smmats[0])
smpoke(sym('g_DeviceStr') + 0x100, smmats[1])

# the text functions: the cursor of Texto.c and the state of the two fonts of Eng_Printf
def smcur():
	return (SDTest.Peek('g_TextoX', 'i'), SDTest.Peek('g_TextoY', 'i'))
smcur0 = smcur()
check('Eng_PrintfXY', lambda: (SDTest.Call('Eng_PrintfXY', 'viisi', 100, 50, 'sd %d', 7), smcur()))
check('Eng_PrintfXY none', lambda: (SDTest.Call('Eng_PrintfXY', 'viiz', 7, 9, None), smcur()))
check('Eng_PrintfRel', lambda: (SDTest.Call('Eng_PrintfRel', 'viiss', 10, -20, 'rel %s', 'x'), smcur()))
for p in ((0.0, 0.0, 0.0), (10.0, 50.0, 300.0)):
	check('Eng_Printf3D' + fmt(p), lambda p=p: (SDTest.Call('Eng_Printf3D', 'vFsi', p, '3d %d', 3), smcur()))
check('Eng_Printf3D none', lambda: (SDTest.Call('Eng_Printf3D', 'vFz', (1.0, 2.0, 3.0), None), smcur()))
SDTest.Poke('g_TextoX', smcur0[0])
SDTest.Poke('g_TextoY', smcur0[1])
smfonts = (SDTest.Peek('g_FontBigNumbers', 'i'), SDTest.Peek('g_FontHoratio', 'i'))
if smfonts[0] and smfonts[1]:
	# the font prints CHECK the viewport size
	smvpwh = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
	SDTest.Poke('g_ViewportW', 640)
	SDTest.Poke('g_ViewportH', 480)
	smfsave = (smints(smfonts[0] - 0x400000, 13), smints(smfonts[1] - 0x400000, 13))
	for fnt in (1, 2):
		for align in (0, 1, 2, 3):
			check('Eng_Printf' + fmt((fnt, align)), lambda fnt=fnt, align=align: (SDTest.Call('Eng_Printf', 'viffiisi', fnt,
				100.0, 200.0, align, 0xff8040, 'sd %d', 42), smints(smfonts[fnt - 1] - 0x400000, 6)))
	smpoke(smfonts[0] - 0x400000, smfsave[0])
	smpoke(smfonts[1] - 0x400000, smfsave[1])
	SDTest.Poke('g_ViewportW', smvpwh[0])
	SDTest.Poke('g_ViewportH', smvpwh[1])

# the view in 640 x 480 coordinates, then the whole view again; the viewport, projection and frustum put back
smvp = (smints(sym('g_Viewport'), 6), smints(sym('g_DeviceStr') + 0x100, 16), smints(sym('g_Frustum'), 24), smints(sym('g_ViewportFull'), 6))
for r in ((0.0, 0.0, 640.0, 480.0), (64.0, 48.0, 320.0, 240.0), (600.0, 10.0, 100.0, 100.0)):
	check('Eng_SetViewRect' + fmt(r), lambda r=r: smlog(lambda r=r: SDTest.Call('Eng_SetViewRect', 'bffff', r[0], r[1], r[2],
		r[3])[0]))
check('Eng_ResetViewRect', lambda: smlog(lambda: SDTest.Call('Eng_ResetViewRect', 'b')[0]))
smpoke(sym('g_Viewport'), smvp[0])
smpoke(sym('g_DeviceStr') + 0x100, smvp[1])
smpoke(sym('g_Frustum'), smvp[2])
smpoke(sym('g_ViewportFull'), smvp[3])

# the frame setup (as the engine does it every frame) when no callback would run
def smsetup():
	return (SDTest.Peek('g_ShowCollision', 'i'), SDTest.Peek('g_WireFrame', 'i'), SDTest.Peek('g_LightmapMode2', 'i'), SDTest.Peek('g_LightmapOff', 'i'),
		SDTest.Peek('g_ModelosOn', 'b'), SDTest.Peek('g_UsarVB', 'b'), SDTest.Peek('g_AmbientD3D', 'i'), smfl(symva('g_CollisionEpsilon'), 1))
if SDTest.Peek(0x4a8318, 'i') == 0 or SDTest.Peek('g_CallbacksOn', 'b') == 0:
	check('Engine_FrameSetup', lambda: (smlog(lambda: SDTest.Call('Engine_FrameSetup', 'b')[0]), smsetup()))

# DebugUtils.cpp: the lines through the D3D log
smm = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 10.0, 20.0, 30.0, 1.0)
sma = (0.0, 1.0, 2.0)
smb = (3.0, -4.0, 5.5)
smo = (-0.0, 0.0, -0.0)
check('DebugUtils_DrawLine', lambda: smlog(lambda: SDTest.Call('DebugUtils_DrawLine', 'vFFi', sma, smb, 0xff00ff)))
check('DebugUtils_DrawLineM', lambda: smlog(lambda: SDTest.Call('Debug_DrawLineM@219740', 'vFFFi', smm, sma, smb, 0x123456)))
check('DebugUtils_DrawBox', lambda: smlog(lambda: SDTest.Call('DebugUtils_DrawBox', 'vFFi', (-1.0, -2.0, -3.0), (1.0, 2.0, 3.0),
	0xff)))
check('DebugUtils_DrawBoxM', lambda: smlog(lambda: SDTest.Call('Debug_DrawBoxM@219400', 'vFFFi', smm, smo, smb, 0xff0000)))
check('DebugUtils_DrawTri', lambda: smlog(lambda: SDTest.Call('DebugUtils_DrawTri', 'vFFFi', sma, smb, smo, 0x8080)))
check('DebugUtils_DrawTriM', lambda: smlog(lambda: SDTest.Call('Debug_DrawTriM@219d50', 'vFFFFi', smm, sma, smb, smo, 0x8080)))
for p, s in ((sma, 4.0), (smo, 1.5), (smb, -2.0)):
	check('DebugUtils_DrawAxes' + fmt((p, s)), lambda p=p, s=s: smlog(lambda p=p, s=s: SDTest.Call('DebugUtils_DrawAxes', 'vFFf',
		smm, p, s)))
	check('DebugUtils_DrawPointM' + fmt((p, s)), lambda p=p, s=s: smlog(lambda p=p, s=s: SDTest.Call('Debug_DrawPointM@219160', 'vFFfi',
		smm, p, s, 0xabcdef)))
	check('DebugUtils_DrawPoint' + fmt((p, s)), lambda p=p, s=s: smlog(lambda p=p, s=s: SDTest.Call('DebugUtils_DrawPoint', 'vFfi', p,
		s, 7)))
