# game ScrapEng/ScrapEngCamera.c, ScrapEngParticles.c, ScrapEngTrails.c (execfile'd at the end of sdcall.py, Python
# 1.5.2): CGameCamera, CGameParticles and CGameTrail objects in heap memory. Floats are compared as their bits. The
# cameras get a fake render node (only its count is touched) and a node of a model loaded here; the particle wrapper
# gets a particle system made from inis here (its node is NULL, so Attach is not called). The engine variables
# poked are put back. Not called: the log paths of CamComposite_Find (file writes), CGameParticles_Attach (needs a
# node with a particle system in a model).
def scz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def scfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def sci(addr):
	return SDTest.Peek(addr, '@i')
def scb(addr):
	return sci(addr) & 0xff
def scints(addr, n):
	r = []
	for i in range(n):
		r.append(sci(addr + 4 * i))
	return r
def scpoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def scset(addr, fl):
	SDTest.Call('crt_memcpy', 'iiFi', addr, fl, 4 * len(fl))
def scini(sect, keys):
	ini = SDTest.Call('CIniFile_ctor', 'tii', scz(4))[0]
	for k, v in keys:
		SDTest.Call('CIniFile_SetStr', 'tiisss', ini, sect, k, v)
	return ini

# --- CGameCamera ---
# the values, the node, the CCamera from rot to mode
def scam(c, node):
	cam = sci(c + 0x34)
	r = (scints(c, 13), cam != 0, sci(c + 0x38) == node)
	if cam:
		r = r + (scints(cam + 8, 17),)
	return r
scvars = (sym('g_CameraDefViewAspect'), sym('g_PhysicalAspectRatio'), sym('g_ShowSceneCamera'), sym('g_CameraScene'), sym('g_CameraNodes'), sym('g_CamCompositeIdx'), sym('g_CamCompositeFrame'), sym('g_CamCompositeFPS'))
scsaved = map(lambda a: SDTest.Peek(a, 'i'), scvars)
check('scrapcam vars', lambda: scsaved)
scnode = scz(0xa8)
scpoke(scnode, 3)
C = scz(0x3c)
C2 = scz(0x3c)
check('CGameCamera_ctor', lambda: (SDTest.Call('CGameCamera_ctor', 'tii', C)[0] == C, scam(C, 0)))
check('CGameCamera_ctor 2', lambda: (SDTest.Call('CGameCamera_ctor', 'tii', C2)[0] == C2, scam(C2, 0)))
check('CGameCamera_IsLookAt', lambda: SDTest.Call('CGameCamera_IsLookAt', 'tbi', C)[0])
for on in (1, 0, 2, 1):
	check('CGameCamera_SetLookAt' + fmt((on,)), lambda on=on: (SDTest.Call('CGameCamera_SetLookAt', 'tvii', C, on),
		SDTest.Call('CGameCamera_IsLookAt', 'tbi', C)[0], sci(sci(C + 0x34) + 0x48)))
scset(C, (10.0, 20.5, -30.25, 0.25, 1.5, -0.125, 0.0, 0.0, 100.0))
check('CGameCamera_Update', lambda: (SDTest.Call('CGameCamera_Update', 'tii', C)[0] == sci(C + 0x34), scam(C, 0)))
for d in (0.0, 2.5, -40.0):
	check('CGameCamera_Move' + fmt((d,)), lambda d=d: (SDTest.Call('CGameCamera_Move', 'tvif', C, d), scam(C, 0)))
for on in (1, 0):
	SDTest.Call('CGameCamera_SetLookAt', 'tvii', C, on)
	for side, rise in ((1.0, 0.0), (-3.5, 2.25), (0.0, -7.0)):
		check('CGameCamera_Strafe' + fmt((on, side, rise)), lambda side=side, rise=rise: (SDTest.Call('CGameCamera_Strafe',
			'tviff', C, side, rise), scam(C, 0)))
# CameraScene: the fov widened or narrowed by the physical aspect
SDTest.Poke('g_CameraScene', 1)
for a in (1.3333334, 1.7777778, 1.7777777, 2.3333333, 1.25, 0.5):
	scset(0x400000 + sym('g_PhysicalAspectRatio'), (a,))
	check('CGameCamera_Update scene' + fmt((a,)), lambda: (SDTest.Call('CGameCamera_Update', 'tii', C)[0] == sci(C + 0x34),
		scam(C, 0)))
SDTest.Poke('g_CameraScene', scsaved[3])
SDTest.Poke('g_PhysicalAspectRatio', scsaved[1])
check('CGameCamera_Copy', lambda: (SDTest.Call('CGameCamera_Copy', 'tvii', C2, C), scam(C2, 0)))
scset(sci(C + 0x34) + 8, (0.5, -1.25, 2.0, 1.0, 2.0, 3.0, -4.0, 5.0, -6.0))
scset(sci(C + 0x34) + 0x3c, (0.875,))
for mode in (0x463, 0x464, 7):
	scpoke(sci(C + 0x34) + 0x48, mode)
	check('CGameCamera_FromCamera' + fmt((mode,)), lambda: (SDTest.Call('CGameCamera_FromCamera', 'tvii', C2, sci(C + 0x34)),
		scam(C2, 0)))
for v in ((0.0, 0.0, 0.0), (1.0, 2.0, 3.0), (-100.5, 7.25, 1e4)):
	check('CGameCamera_ToLocal' + fmt(v), lambda v=v: SDTest.Call('CGameCamera_ToLocal', 'tiiFF', C2, (9.0, 9.0, 9.0), v)[1:])
check('CGameCamera_SetNode', lambda: (SDTest.Call('CGameCamera_SetNode', 'tvii', C2, scnode), scam(C2, scnode), sci(scnode)))
check('CGameCamera_SetNode again', lambda: (SDTest.Call('CGameCamera_SetNode', 'tvii', C2, scnode), scam(C2, scnode), sci(scnode)))
scpoke(scnode, 3)
check('CGameCamera_SetNodeModel none', lambda: (SDTest.Call('CGameCamera_SetNodeModel', 'tbiii', C2, 0, -1)[0], scam(C2, 0),
	sci(scnode)))
check('CamComposite_DrawDebug off', lambda: SDTest.Call('CamComposite_DrawDebug', 'v'))

# a model for the node functions and CamComposite_Find
scgm = scz(0x1c)
SDTest.Call('CGameModel_ctor', 'tiis', scgm, 'Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D')
scmod = sci(scgm)
scbip = SDTest.Call('CGameModel_FindRenderNode', 'tiis', scgm, 'Bip01')[0]
check('scrapcam model', lambda: (scmod != 0, scbip))
if scmod and scbip >= 0:
	# node 0 is a unique render node (sdcall_scrapmodel.py takes its reference too)
	check('CGameCamera_SetNodeModel', lambda: (SDTest.Call('CGameCamera_SetNodeModel', 'tbiii', C2, scgm, 0)[0],
		scam(C2, 0)[0:2], sci(C2 + 0x38) != 0, sci(sci(C2 + 0x38))))
	check('CGameCamera_Update node', lambda: (SDTest.Call('CGameCamera_Update', 'tii', C2)[0] == sci(C2 + 0x34), scam(C2, 0)[0:2],
		SDTest.Peek('g_CamCompositeIdx', 'i')))
	scr = sci(C2 + 0x38)
	check('CGameCamera_SetNodeModel release', lambda: (SDTest.Call('CGameCamera_SetNodeModel', 'tbiii', C2, 0, 0)[0],
		sci(C2 + 0x38), sci(scr)))
	scini0 = scini('Cam-Composite', ())
	scini1 = scini('Cam-Composite', (('01_StartFrame', '-1'),))
	# the exact name of node Bip01 (the composite search is case sensitive); a camera not found would be logged
	scbipname = SDTest.Peek(sci(sci(sci(scmod + 0xac) + 4) + 0xa8 * scbip + 0xa4) + 4, '@p')
	scini2 = scini('Cam-Composite', (('01_StartFrame', '0'), ('02_StartFrame', '100'), ('01_Camera', scbipname)))
	scini3 = scini('Cam-Composite', (('01_StartFrame', '0'), ('02_StartFrame', '0'), ('03_StartFrame', '1e9'),
		('02_Camera', scbipname + '*')))
	for nm, ini in (('empty', scini0), ('negative', scini1), ('Bip01', scini2), ('wild', scini3)):
		check('CamComposite_Find ' + nm, lambda ini=ini: (SDTest.Call('CamComposite_Find', 'iii', scmod, ini)[0] != 0,
			SDTest.Peek('g_CamCompositeIdx', 'i'), SDTest.Peek('g_CamCompositeFrame', 'i'), SDTest.Peek('g_CamCompositeFPS', 'i')))
	scn = SDTest.Call('CamComposite_Find', 'iii', scmod, scini2)[0]
	if scn:
		check('CamComposite_Find node', lambda: SDTest.Peek(scn + 4, '@p'))
	# the text of ShowSceneCamera with the fonts of the game
	if SDTest.Peek('g_FontHoratioSmall', 'i') and SDTest.Peek('g_FontScrapBig', 'i'):
		SDTest.Poke('g_ShowSceneCamera', 1)
		scset(0x400000 + sym('g_CamCompositeFPS'), (25.0,))
		check('CamComposite_DrawDebug', lambda: (SDTest.Call('CamComposite_DrawDebug', 'v'), sci(SDTest.Peek('g_FontHoratioSmall', 'i')),
			sci(SDTest.Peek('g_FontScrapBig', 'i'))))
		SDTest.Poke('g_ShowSceneCamera', scsaved[2])
check('CGameCamera_dtor', lambda: (SDTest.Call('CGameCamera_dtor', 'tvi', C), sci(C + 0x34), sci(C + 0x38)))
check('CGameCamera_dtor 2', lambda: (SDTest.Call('CGameCamera_dtor', 'tvi', C2), sci(C2 + 0x34), sci(C2 + 0x38)))
scfree(C)
scfree(C2)
for i in range(len(scvars)):
	SDTest.Poke(scvars[i], scsaved[i])
check('scrapcam vars after', lambda: map(lambda a: SDTest.Peek(a, 'i'), scvars))

# --- CGameParticles ---
def scpart(p):
	r = (scb(p + 4), scints(p + 8, 11))
	if sci(p) == 0x400000 + sym('CParticleActive_vftable'):
		r = r + (scb(p + 0x34), scints(p + 0x38, 3), scints(p + 0x48, 3))
	return r
def scparts(sp):
	r = []
	ps = sci(sp + 0x11c)
	for i in range(sci(sp + 0xc)):
		r.append(scpart(sci(ps + 4 * i)))
	return r
def scsp(sp):
	return (scints(sp + 0xc, 0x32), scb(sp + 0xd4), scb(sp + 0xd5), scb(sp + 0xd6), scints(sp + 0xe0, 3), scb(sp + 0xec),
		sci(sp + 0xf8), scb(sp + 0x10c), scb(sp + 0x140), scb(sp + 0x141))
spnode = SDTest.Call('CNode3D_ctor', 'tii', scz(0xfc))[0]
SDTest.Call('String_Set', 'tvis', spnode + 4, 'sdgpart')
SDTest.Poke(spnode + 8 - 0x400000, 1)
SDTest.Poke(spnode + 0xf8 - 0x400000, scini('Particles', (('Type', 'sdgp'), ('Scale', '1.0'))))
spini = scini('sdgp', (('Particles', '5'), ('BirthRate', '20'), ('LifeTime', '0.5'), ('SizeIni', '0.5'),
	('SizeEnd', '2'), ('InitVelMin', '-1 2 -1'), ('InitVelMax', '1 4 1'), ('InitAcc', '0 -9.8 0')))
SDTest.Call('crt_srand', 'vi', 17)
SP = SDTest.Call('ParticleSystem_FromNode', 'iii', spnode, spini)[0]
check('scrapcam ParticleSystem', lambda: SP != 0)
G = scz(0xb0)
# 0x9a, 0x9b and 0xa5..0xa7 are not set
def scgp(g):
	return (scints(g, 0x26), scb(g + 0x98), scb(g + 0x99), scints(g + 0x9c, 2), scb(g + 0xa4), sci(g + 0xa8),
		sci(g + 0xac) == SP)
check('CGameParticles_ctor', lambda: (SDTest.Call('CGameParticles_ctor', 'tii', G)[0] == G, scgp(G)))
if SP:
	scpoke(G + 0xac, SP)
	check('CGameParticles_Apply', lambda: (SDTest.Call('CGameParticles_Apply', 'tvi', G), scsp(SP)))
	scset(G, (0.0, 0.0, 0.75, 1.25, 0.5, 2.0, 0.0, 0.25, 1.0, 2.0, 3.0))
	scpoke(G, 0x11223344)
	scpoke(G + 4, 0x55667788)
	scpoke(G + 0x18, 33)
	scpoke(G + 0x98, 0x0302)
	scpoke(G + 0xa4, 1)
	check('CGameParticles_Apply set', lambda: (SDTest.Call('CGameParticles_Apply', 'tvi', G), scsp(SP)))
	check('CGameParticles_NumParticles', lambda: SDTest.Call('CGameParticles_NumParticles', 'tii', G)[0])
	check('CGameParticles_NumActive', lambda: SDTest.Call('CGameParticles_NumActive', 'tii', G)[0])
	for on in (1, 0, 2):
		check('CGameParticles_SetEnabled' + fmt((on,)), lambda on=on: (SDTest.Call('CGameParticles_SetEnabled', 'tvii', G, on),
			SDTest.Call('CGameParticles_IsEnabled', 'tbi', G)[0]))
		check('CGameParticles_SetVisible' + fmt((on,)), lambda on=on: (SDTest.Call('CGameParticles_SetVisible', 'tvii', G, on),
			SDTest.Call('CGameParticles_IsVisible', 'tbi', G)[0]))
		check('CGameParticles_SetColorRampOn' + fmt((on,)), lambda on=on: (SDTest.Call('CGameParticles_SetColorRampOn', 'tvii', G, on),
			sci(SP + 0x10)))
		check('CGameParticles_SetSparksOn' + fmt((on,)), lambda on=on: (SDTest.Call('CGameParticles_SetSparksOn', 'tvii', G, on),
			sci(SP + 0x10)))
	SDTest.Call('CGameParticles_SetEnabled', 'tvii', G, 1)
	SDTest.Call('CGameParticles_SetVisible', 'tvii', G, 1)
	for a in (0.25, 1.0):
		check('CGameParticles_SetAlpha' + fmt((a,)), lambda a=a: (SDTest.Call('CGameParticles_SetAlpha', 'tvif', G, a),
			SDTest.Call('CGameParticles_GetAlpha', 'tfi', G)[0], sci(SP + 0xf8)))
	for i, x, sz, col, rot in ((0, 1.5, 0.5, 0x10203040, 0.25), (3, -2.0, 1.75, -1, -1.0), (4, 100.0, 0.0, 0, 3.0)):
		check('CGameParticles_SetParticle' + fmt((i, x)), lambda i=i, x=x, sz=sz, col=col, rot=rot: (SDTest.Call(
			'CGameParticles_SetParticle', 'tviiffffiif', G, i, x, x * 2, -x, sz, col, 0, rot), scparts(SP), scb(SP + 0x10c)))
	for i in (0, 1, 3, 4):
		check('CGameParticles_GetParticle' + fmt((i,)), lambda i=i: SDTest.Call('CGameParticles_GetParticle', 'tbiiFFFpF', G, i,
			(9.0, 9.0, 9.0), (8.0, 8.0, 8.0), (7.0,), 6, (5.0,)))
		check('CGameParticles_GetParticle nulls' + fmt((i,)), lambda i=i: SDTest.Call('CGameParticles_GetParticle', 'tbiiiiiii', G, i,
			0, 0, 0, 0, 0)[0])
	check('CGameParticles_Kill', lambda: (SDTest.Call('CGameParticles_Kill', 'tvii', G, 3), scparts(SP), scb(SP + 0x10c)))
	check('CGameParticles_KillRange', lambda: (SDTest.Call('CGameParticles_KillRange', 'tviii', G, 0, 2), scparts(SP), scb(SP + 0x10c)))
	check('CGameParticles_KillRange empty', lambda: (SDTest.Call('CGameParticles_KillRange', 'tviii', G, 5, 5), scparts(SP)))
	SDTest.Call('crt_srand', 'vi', 19)
	check('CGameParticles_EmitN', lambda: (SDTest.Call('CGameParticles_EmitN', 'tviii', G, 2, 0), scparts(SP), scb(SP + 0x10c)))
	SDTest.Call('crt_srand', 'vi', 23)
	check('CGameParticles_EmitDt', lambda: (SDTest.Call('CGameParticles_EmitDt', 'tviiif', G, 2, 1, 0.05), scparts(SP),
		scb(SP + 0x10c)))
	scm = scz(0x40)
	scset(scm, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 5.0, -6.0, 7.0, 1.0))
	SDTest.Call('crt_srand', 'vi', 29)
	check('CGameParticles_EmitM', lambda: (SDTest.Call('CGameParticles_EmitM', 'tviiii', G, 2, 1, scm), scparts(SP),
		scb(SP + 0x10c)))
	SDTest.Call('crt_srand', 'vi', 31)
	check('CGameParticles_Emit', lambda: (SDTest.Call('CGameParticles_Emit', 'tviiifi', G, 3, 1, 0.02, scm), scparts(SP),
		scb(SP + 0x10c)))
	check('CGameParticles_KillAll', lambda: (SDTest.Call('CGameParticles_KillAll', 'tvi', G), scparts(SP), scb(SP + 0x10c)))
	scramp = scz(0x24)
	check('CGameParticles_SetColorRamp', lambda: (SDTest.Call('CGameParticles_SetColorRamp', 'tvii', G, scramp), sci(SP + 0x144) == scramp))
	check('CGameParticles_SetColorRamp none', lambda: (SDTest.Call('CGameParticles_SetColorRamp', 'tvii', G, 0), sci(SP + 0x144)))
	scpos = scz(0x30)
	check('CGameParticles_SetEmitPos', lambda: (SDTest.Call('CGameParticles_SetEmitPos', 'tviii', G, scpos, 4), sci(SP + 0x148) == scpos,
		scb(SP + 0x14c), scb(SP + 0x14d)))
	check('CGameParticles_SetEmitPos none', lambda: (SDTest.Call('CGameParticles_SetEmitPos', 'tviii', G, 0, 0), sci(SP + 0x148),
		scb(SP + 0x14c)))
scpoke(scnode, 5)
scpoke(G + 0xa8, scnode)
check('CGameParticles_Release', lambda: (SDTest.Call('CGameParticles_Release', 'tvi', G), sci(G + 0xa8), sci(G + 0xac), sci(scnode)))
scpoke(G + 0xa8, scnode)
scpoke(G + 0xac, SP)
check('CGameParticles_Release 2', lambda: (SDTest.Call('CGameParticles_Release@21db50', 'tvi', G), sci(G + 0xa8), sci(G + 0xac), sci(scnode)))
check('CGameParticles_Release none', lambda: (SDTest.Call('CGameParticles_Release@21db50', 'tvi', G), sci(G + 0xa8), sci(G + 0xac)))
scpoke(G + 0x98, 0x07070505)
check('CGameParticles_Init', lambda: (SDTest.Call('CGameParticles_Init', 'tvi', G), scgp(G), sci(G + 0x98)))
scfree(G)

# --- CGameTrail ---
def sctr(g):
	t = sci(g)
	w = scints(t, 21)
	r = (w[0:10], w[10] != 0, w[11:21])
	p = sci(t + 0x28)
	if p:
		r = r + (scints(p, 5 * scb(t + 0x20)),)
	return r
def sclist():
	r = [SDTest.Peek('g_nTrails', 'i')]
	for i in range(17):
		r.append(SDTest.Peek(sym('g_nTrailsRender') + 4 * i, 'i'))
	return r
def scstep(name, g, rva, sig, *args):
	check(name + fmt(args), lambda g=g, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, g) + args)[0],
		sctr(g), sclist()))
check('scrapcam trails lists', sclist)
T = scz(4)
T2 = scz(4)
check('CGameTrail_ctor', lambda: (SDTest.Call('CGameTrail_ctor', 'tii', T)[0] == T, sci(T) != 0, sctr(T)))
check('CGameTrail_ctor 2', lambda: (SDTest.Call('CGameTrail_ctor', 'tii', T2)[0] == T2, sci(T2) != 0))
# the new points of SetMaxTrails are what new[] left (heap of earlier objects): cleared before they are printed
def scmaxtrails(g, n):
	r = SDTest.Call('CGameTrail_SetMaxTrails', 'tvii', g, n)[0]
	scset(sci(sci(g) + 0x28), (0.0,) * (5 * n))
	return r
check('CGameTrail_SetMaxTrails' + fmt((8,)), lambda: (scmaxtrails(T, 8), sctr(T), sclist()))
scmaxtrails(T2, 8)
check('CGameTrail_GetMaxTrails', lambda: SDTest.Call('CGameTrail_GetMaxTrails', 'tii', T)[0])
scstep('CGameTrail_SetTransp', T, sym('CGameTrail_SetTransp'), 'tvii', 40)
check('CGameTrail_GetTransp', lambda: SDTest.Call('CGameTrail_GetTransp', 'tii', T)[0])
scstep('CGameTrail_SetColor', T, sym('CGameTrail_SetColor'), 'tvii', 0x2080c0f0)
scstep('CGameTrail_SetSmooth', T, sym('CGameTrail_SetSmooth'), 'tvii', 1)
scstep('CGameTrail_SetUV', T, sym('CGameTrail_SetUV'), 'tviff', 0.25, -0.5)
scstep('CGameTrail_SetOrient', T, sym('CGameTrail_SetOrient'), 'tvifffi', 1.5, 2.5, -3.5, 1)
scstep('CGameTrail_SetType', T, sym('CGameTrail_SetType'), 'tvii', 2)
scstep('CGameTrail_SetSegments', T, sym('CGameTrail_SetSegments'), 'tvii', 6)
scstep('CGameTrail_SetPremul', T, sym('CGameTrail_SetPremul'), 'tvii', 1)
scstep('CGameTrail_SetWidth', T, sym('CGameTrail_SetWidth'), 'tvif', 2.75)
scstep('CGameTrail_SetScale', T, sym('CGameTrail_SetScale'), 'tviff', 1.5, 0.25)
scstep('CGameTrail_SetUVScale', T, sym('CGameTrail_SetUVScale'), 'tviff', 3.0, 0.75)
scstep('CGameTrail_SetFade', T, sym('CGameTrail_SetFade'), 'tvii', 1)
scstep('CGameTrail_SetULen', T, sym('CGameTrail_SetULen'), 'tviff', 7.0, 9.0)
scstep('CGameTrail_SetTexture', T, sym('CGameTrail_SetTexture'), 'tvii', 3)
scstep('CGameTrail_AddPos', T, sym('CGameTrail_AddPos'), 'tvifff', 1.0, 2.0, 3.0)
scstep('CGameTrail_AddPosColor', T, sym('CGameTrail_AddPosColor'), 'tvifffi', -1.0, 0.5, 8.0, 0x80402010)
scstep('CGameTrail_Add', T, sym('CGameTrail_Add'), 'tviffffi', 4.0, -4.0, 0.25, 1.5, 0xff123456)
check('CGameTrail_IsRender', lambda: SDTest.Call('CGameTrail_IsRender', 'tbi', T)[0])
scstep('CGameTrail_SetLastPos', T, sym('CGameTrail_SetLastPos'), 'tvifff', 9.0, 8.0, 7.0)
scstep('CGameTrail_SetLastPosColor', T, sym('CGameTrail_SetLastPosColor'), 'tvifffi', 6.0, 5.0, 4.0, 0x01020304)
check('CGameTrail_SetLast', lambda: (SDTest.Call('CGameTrail_SetLast', 'tviFFp', T, (0.5, 0.25, 0.125), (3.0,), 0x7f7f7f7f),
	sctr(T)))
check('CGameTrail_SetLast nulls', lambda: (SDTest.Call('CGameTrail_SetLast', 'tviiii', T, 0, 0, 0), sctr(T)))
check('CGameTrail_SetPoint', lambda: (SDTest.Call('CGameTrail_SetPoint', 'tviiFFp', T, 1, (-0.5, -0.25, 11.0), (0.75,), 0x40ff8000),
	sctr(T)))
scstep('CGameTrail_Move', T, sym('CGameTrail_Move'), 'tvifff', 0.5, -1.5, 2.5)
scstep('CGameTrail_Shrink', T, sym('CGameTrail_Shrink'), 'tvi')
check('CGameTrail_Copy', lambda: (SDTest.Call('CGameTrail_Copy', 'tvii', T2, T)[0], sctr(T2), sclist()))
scstep('CGameTrail_Reset', T, sym('CGameTrail_Reset'), 'tvi')
scstep('CGameTrail_Clear', T2, sym('CGameTrail_Clear'), 'tvi')
for a, b in (((0.0, 0.0, 0.0), (7.0, -14.0, 21.0)), ((1.5, 2.5, -3.5), (1.5, 2.5, -3.5)), ((-1e3, 0.1, 3.0),
		(2e3, -0.3, 0.7))):
	SDTest.Call('CGameTrail_Reset', 'tvi', T)
	check('CGameTrail_AddLine' + fmt((a, b)), lambda a=a, b=b: (apply(SDTest.Call, (sym('CGameTrail_AddLine'), 'tviffffff', T) + a + b),
		sctr(T)))
scpts = ((0.0, 0.0, 0.0), (1.0, 4.0, -2.0), (5.0, -3.0, 0.5), (8.0, 1.0, 1.0), (-2.0, 0.25, 6.0), (3.0, 3.0, 3.0))
def scflat(pts):
	r = ()
	for p in pts:
		r = r + p
	return r
for n in (1, 2, 3, 4, 6):
	pts = scflat(scpts[0:n])
	check('CGameTrail_BezierCoefs' + fmt((n,)), lambda n=n, pts=pts: SDTest.Call('CGameTrail_BezierCoefs', 'tviiFF', T, n, pts,
		(0.0,) * (3 * n))[1:])
	cf = SDTest.Call('CGameTrail_BezierCoefs', 'tviiFF', T, n, pts, (0.0,) * (3 * n))[2]
	for t in (0.0, 0.3, 0.5, 1.0):
		check('CGameTrail_BezierPoint' + fmt((n, t)), lambda n=n, cf=cf, t=t: SDTest.Call('CGameTrail_BezierPoint', 'tviiFFf', T, n, cf,
			(9.0, 9.0, 9.0), t)[2])
	SDTest.Call('CGameTrail_Reset', 'tvi', T)
	check('CGameTrail_AddBezier' + fmt((n,)), lambda n=n, pts=pts: (SDTest.Call('CGameTrail_AddBezier', 'tviiFi', T, n, pts,
		0x10203040)[1], sctr(T)))
# SetMaxTrails on a trail with points is a CHECK (!m_pTrails): no tests with fewer points
check('CGameTrail_dtor', lambda: (SDTest.Call('CGameTrail_dtor', 'tvi', T), sclist()))
check('CGameTrail_dtor 2', lambda: (SDTest.Call('CGameTrail_dtor', 'tvi', T2), sclist()))
scfree(T)
scfree(T2)
