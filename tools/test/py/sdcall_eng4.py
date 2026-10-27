# engine leftovers 0x200000..0x240000 (execfile'd at the end of sdcall.py, Python 1.5.2): the matriz.h math of the
# engine3d (D3DX wrappers, Box_Transform, Matrix_ToEuler, Ang_Wrap), the floors of the 2D map, the debris move on fake
# debris (no model, no collision), the constructors and destructors of material.h, the lens flare holder, the render
# nodes and the render context, the screen images (CPsaImage) with their draws through the D3D log, the DebugUtils
# sphere. Pointers are only compared. Not called: the file and window parts, the debris with models (sdlevel_eng4.py).
def en4z(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def en4new(n):
	return SDTest.Call(0x1334d0, 'ii', n)[0]
def en4free(p):
	SDTest.Call(0x133550, 'vi', p)
def en4i(a):
	return SDTest.Peek(a, '@i')
def en4b(a):
	return SDTest.Peek(a, '@b')
def en4poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def en4words(a, n):
	r = []
	for k in range(n):
		r.append(en4i(a + 4 * k))
	return r
def en4fl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def en4set(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, tuple(v), 4 * len(v))
def en4log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
en4inf = 1e300 * 1e300
en4nan = en4inf - en4inf

# ---- matriz.h ----
for a in (0.0, 7.0, -7.0, 6.2831855, 6.2831860, -6.2831855, -6.3, 12.6, -12.6, 1e10, en4inf, en4nan, -0.0):
	check('en4 Ang_Wrap' + fmt((a,)), lambda a=a: SDTest.Call(0x238b80, 'ff', a)[0])
en4mats = ((1.0, 0.5, 0.25, 0.0, -0.3, 0.9, 0.1, 0.0, 0.2, -0.1, 0.95, 0.0, 10.0, -5.0, 3.0, 1.0),
	(0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.5, 2.5, -3.5, 1.0),
	(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0),
	(0.6, 0.8, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, -0.8, 0.6, 0.00001, 0.0, 4.0, 5.0, 6.0, 1.0),
	(2.0, 0.0, 0.0, 0.5, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 4.0, 0.0, 1.0, 1.0, 1.0, 2.0),
	(0.0,) * 16)
for m in en4mats:
	check('en4 Matrix_ToEuler' + fmt(m), lambda m=m: SDTest.Call(0x2392d0, 'iFF', (0.0, 0.0, 0.0), m)[1])
	check('en4 Matrix_Invert' + fmt(m), lambda m=m: SDTest.Call(0x239560, 'iFF', (9.0,) * 16, m)[0:2])
	check('en4 Matrix_Transpose' + fmt(m), lambda m=m: SDTest.Call(0x239860, 'iFF', (0.0,) * 16, m)[1])
	for n in en4mats[0:3]:
		check('en4 Matrix_MulD3D' + fmt((m, n)), lambda m=m, n=n: SDTest.Call(0x2391f0, 'iFFF', (0.0,) * 16, m, n)[1])
		check('en4 Matrix_MulTo' + fmt((m, n)), lambda m=m, n=n: SDTest.Call(0x239820, 'vFFF', (0.0,) * 16, m, n)[1])
	for s in ((1.0, 2.0, 3.0), (-0.5, 0.0, 1e20)):
		check('en4 Matrix_PostScale' + fmt((m, s)), lambda m=m, s=s: SDTest.Call(0x239210, 'vFF', m, s)[1])
	for r in ((0.1, 0.2, 0.3), (-1.0, 2.5, 0.0)):
		check('en4 Matrix_PostRotate' + fmt((m, r)), lambda m=m, r=r: SDTest.Call(0x23a100, 'vFF', m, r)[1])
	for b in ((-1.0, -2.0, -3.0, 1.0, 2.0, 3.0), (0.5, 0.5, 0.5, 0.5, 0.5, 0.5), (10.0, -4.0, 0.0, 12.0, 4.0, 1.0)):
		check('en4 Box_Transform' + fmt((m, b)), lambda m=m, b=b: SDTest.Call(0x239880, 'iFFF', (0.0,) * 6, b, m)[1])
for d in ((0.0, 0.0, 1.0), (1.0, 0.0, 0.0), (0.3, -0.7, 0.2), (0.0, 1.0, 0.0), (0.0, 0.0, 0.0), (-2.0, 5.0, -1.0)):
	check('en4 Vec3_ToAngles' + fmt(d), lambda d=d: SDTest.Call(0x239150, 'iFF', (0.0, 0.0, 0.0), d)[1])
	check('en4 Matrix_RotationAxis' + fmt(d), lambda d=d: SDTest.Call(0x23a2d0, 'iFFf', (0.0,) * 16, d, 0.7)[1])
for a in (0.0, 0.5, -1.25, 3.14159265, 100.0):
	check('en4 Matrix_RotationX' + fmt((a,)), lambda a=a: SDTest.Call(0x23a270, 'iFf', (0.0,) * 16, a)[1])
	check('en4 Matrix_RotationY' + fmt((a,)), lambda a=a: SDTest.Call(0x23a290, 'iFf', (0.0,) * 16, a)[1])
	check('en4 Matrix_RotationZ' + fmt((a,)), lambda a=a: SDTest.Call(0x23a2b0, 'iFf', (0.0,) * 16, a)[1])
for p in ((1.0, 1.333, 1.0, 1000.0), (0.5, 1.0, 10.0, 10.001), (0.01, 1.0, 1.0, 2.0), (2.0, 0.75, -5.0, 5.0)):
	check('en4 Matrix_PerspectiveD3D' + fmt(p), lambda p=p: apply(SDTest.Call, (0x23a190, 'iFffff', (7.0,) * 16) + p)[0:2])
check('en4 E3D_GetExePath', lambda: SDTest.Call(0x238910, 'i')[0] - 0x400000)
check('en4 RSqrt_InitTable', lambda: (SDTest.Call(0x238b10, 'v'), en4words(0x8abf88, 33)))

# ---- MapFloors.cpp on a fake floor list ----
en4fl0 = en4z(0x18c4)
en4poke(en4fl0, 4)
for k, y in ((0, -3.4028235e38), (1, 0.0), (2, 25.0), (3, 60.0)):
	en4set(en4fl0 + 4 + k * 0x18c, (y,))
for y in (-100.0, 0.0, 0.5, 24.9, 25.0, 59.0, 61.0, en4nan):
	check('en4 Floor_Get' + fmt((y,)), lambda y=y: SDTest.Call(0x203af0, 'tiif', en4fl0, y)[0])
for a, b in (((0.0, 1.0, 0.0), (3.0, 4.0, 12.0)), ((0.0, -5.0, 0.0), (0.0, 70.0, 0.0)), ((1.0, 30.0, 2.0), (1.0, 2.0, 2.0))):
	check('en4 Floor_Dist' + fmt((a, b)), lambda a=a, b=b: apply(SDTest.Call, (0x203a10, 'tfiffffff', en4fl0) + a + b)[0])
en4lv = en4fl0 + 4 + 2 * 0x18c
en4poke(en4lv + 4, 3)
en4set(en4lv + 8, (0.0, 26.0, 0.0, 10.0, 26.0, 5.0, -4.0, 26.0, 8.0))
en4poke(en4lv + 0x88, 2)
en4set(en4lv + 0x8c, (7.0, 24.0, 7.0, -7.0, 24.0, -7.0))
for p in ((0.0, 0.0, 0.0), (9.0, 26.0, 4.0), (-5.0, 0.0, 9.0), (6.0, 24.0, 6.0), (-100.0, 0.0, -100.0)):
	check('en4 MapLevel_NearestUp' + fmt(p), lambda p=p: SDTest.Call(0x203bb0, 'tiiF', en4lv, p)[0])
	check('en4 MapLevel_NearestDown' + fmt(p), lambda p=p: SDTest.Call(0x203b30, 'tiiF', en4lv, p)[0])
en4cmp = en4z(8)
for a, b in ((1.0, 2.0), (2.0, 1.0), (1.0, 1.0), (en4nan, 1.0), (-0.0, 0.0)):
	en4set(en4cmp, (a, b))
	check('en4 MapLevel_CompareY' + fmt((a, b)), lambda: SDTest.Call(0x203c30, 'iii', en4cmp, en4cmp + 4)[0])
check('en4 Floor_dtor', lambda: SDTest.Call(0x203a00, 'tvi', en4fl0)[0:0])
en4free(en4fl0)
en4free(en4cmp)

# ---- ObjPool.cpp: the system and the moves of fake debris ----
en4sys = en4z(0x38)
check('en4 cDebrisSys_ctor', lambda: (SDTest.Call(0x2048d0, 'tii', en4sys)[0] == en4sys, en4words(en4sys, 3),
	en4words(en4sys + 0x10, 10), en4i(en4sys + 0xc)))
check('en4 cDebrisSys_SetName', lambda: (SDTest.Call(0x205450, 'tvis', en4sys, 'Models/Nada.M3D'),
	SDTest.Peek(en4sys + 0xc, '@p')))
check('en4 cDebrisSys_SetMaxIdle', lambda: (SDTest.Call(0x205430, 'tvii', en4sys, 7), en4words(en4sys, 3),
	en4i(en4sys + 0x2c)))
en4d = en4z(0x58)
en4d2 = en4z(0x58)
check('en4 cDebrisSys_IsIdle none', lambda: SDTest.Call(0x204dd0, 'tbii', en4sys, en4d)[0])
en4poke(en4sys + 4, en4d)
check('en4 cDebrisSys_IsIdle', lambda: (SDTest.Call(0x204dd0, 'tbii', en4sys, en4d)[0],
	SDTest.Call(0x204dd0, 'tbii', en4sys, en4d2)[0]))
check('en4 cDebrisSys_GetIdle', lambda: SDTest.Call(0x204d40, 'tii', en4sys)[0] == en4d)
en4poke(en4sys + 4, 0)
for t in (0, 37, -1, 52, 200):
	check('en4 cDebris_SetFX' + fmt((t,)), lambda t=t: (SDTest.Call(0x203c50, 'bii', t, en4d)[0], en4i(en4d + 0x54)))
check('en4 cDebris_SetFX NULL', lambda: SDTest.Call(0x203c50, 'bii', 5, 0)[0])
en4oe = en4z(4)
check('en4 cObjElement_dtor', lambda: (SDTest.Call(0x204940, 'tvi', en4oe), en4i(en4oe) - 0x400000))
en4free(en4oe)
# the step without the floor: friction b, the gravity d, the life
for b, g, life, st in ((1.0, 0.0, 1.0, (0.0, 0.0, 0.0, 0.1, 0.2, 0.3, 1.0, 2.0, 3.0, 0.5, -0.5, 0.25)),
		(0.5, 10.0, 0.3, (5.0, 6.0, 7.0, 0.0, 0.0, 0.0, -2.0, 0.0, 4.0, 1.0, 1.0, 1.0)),
		(0.0, -3.0, 0.05, (1.0, 1.0, 1.0, 1.0, 1.0, 1.0, -0.0, -0.0, -0.0, -0.0, 0.0, 0.0)),
		(2.5, 9.81, 0.0, (0.0,) * 12), (1.0, 1.0, -1.0, (0.0,) * 12),
		(0.1, 0.0, en4nan, (1.0,) * 12), (0.1, 0.0, 0.07, (en4inf, 0.0, 0.0, 0.0, 0.0, 0.0, 1e30, -1e30, 0.0, 0.0, 0.0, 0.0))):
	en4set(en4sys + 0x14, (0.0, b, 1.0, g))
	en4poke(en4sys + 0x24, 0)
	en4set(en4d + 0x1c, st + (life,))
	check('en4 cDebris_Update' + fmt((b, g, life, st)), lambda: (SDTest.Call(0x204ea0, 'tbii', en4d, en4sys)[0],
		en4fl(en4d + 0x1c, 13)))
	check('en4 cDebris_IsAlive' + fmt((b, g, life, st)), lambda: (SDTest.Call(0x2053a0, 'tbii', en4d, en4sys)[0],
		en4fl(en4d + 0x1c, 13)))
check('en4 cDebrisSys_dtor', lambda: (SDTest.Call(0x204950, 'tvi', en4sys), en4words(en4sys, 3)))
en4free(en4d)
en4free(en4d2)
en4free(en4sys)

# ---- the scheduled functions ----
en4sf = en4z(0x90)
check('en4 cScheduledFunc_dtor', lambda: (SDTest.Call(0x205680, 'tvi', en4sf), en4words(en4sf, 3)))
en4free(en4sf)

# ---- material.h ----
en4mp = en4z(0x60)
check('en4 CMapa_ctor', lambda: (SDTest.Call(0x233760, 'tii', en4mp)[0] == en4mp, en4words(en4mp, 24),
	en4fl(en4mp + 0x10, 8)))
en4poke(en4mp + 0x54, 0x101)
check('en4 CMapa_HasAlpha', lambda: SDTest.Call(0x236d20, 'tbi', en4mp)[0])
en4poke(en4mp + 0x54, 0)
check('en4 CMapa_HasAlpha 0', lambda: SDTest.Call(0x236d20, 'tbi', en4mp)[0])
SDTest.Call(0x2a1940, 'tvis', en4mp, 'tex.tga')
SDTest.Call(0x2a1940, 'tvis', en4mp + 0x44, 'tex%02d.tga')
check('en4 CMapa_dtor', lambda: (SDTest.Call(0x233a30, 'tvi', en4mp), en4words(en4mp, 24)))
en4free(en4mp)
en4mt = en4z(0xb0)
check('en4 CMaterial_ctor', lambda: (SDTest.Call(0x233830, 'tii', en4mt)[0] == en4mt, en4i(en4mt) - 0x400000,
	en4words(en4mt + 4, 18), en4fl(en4mt + 0x4c, 17), en4words(en4mt + 0x90, 8)))
for k in (1.0, 0.5, 2.0, 0.0, -1.0, en4nan):
	check('en4 CMaterial_ScaleColors' + fmt((k,)), lambda k=k: (SDTest.Call(0x237020, 'tvif', en4mt, k),
		en4fl(en4mt + 0x4c, 17)))
en4poke(en4mt + 0x14, 0x80ffffff)
check('en4 CMaterial_ScaleColors alpha', lambda: (SDTest.Call(0x237020, 'tvif', en4mt, 0.25), en4fl(en4mt + 0x4c, 17)))
SDTest.Call(0x2a1940, 'tvis', en4mt + 4, 'mat1')
check('en4 CMaterial_dtor', lambda: (SDTest.Call(0x233af0, 'tvi', en4mt), en4i(en4mt) - 0x400000, en4words(en4mt + 4, 3),
	en4words(en4mt + 0x9c, 5)))
en4free(en4mt)
en4lm = en4z(0x10)
en4poke(en4lm, 0x7d6d48)
SDTest.Call(0x2a1940, 'tvis', en4lm + 4, 'lista')
check('en4 CListaMat_dtor', lambda: (SDTest.Call(0x2339d0, 'tvi', en4lm), en4i(en4lm) - 0x400000, en4words(en4lm + 4, 3)))
check('en4 CListaMat_DeleteObj', lambda: (SDTest.Call(0x2336f0, 'tiii', en4lm, 0)[0] == en4lm, en4words(en4lm + 4, 3)))
en4free(en4lm)
en4cs = en4z(4)
en4poke(en4cs, SDTest.Call(0x22cfa0, 'is', 'cadena')[0])
check('en4 Cad_dtor', lambda: (SDTest.Call(0x233be0, 'tvi', en4cs), en4i(en4cs)))
check('en4 Cad_dtor NULL', lambda: (SDTest.Call(0x233be0, 'tvi', en4cs), en4i(en4cs)))
en4free(en4cs)
en4bm = en4z(12)
SDTest.Call(0x2a9bc0, 'tviii', en4bm, 4, 3)
check('en4 CBmp_dtor', lambda: (en4i(en4bm + 8) != 0, SDTest.Call(0x2339a0, 'tvi', en4bm), en4words(en4bm, 3)))
en4free(en4bm)

# ---- ScrapEngLensFlares.cpp ----
en4lf = en4z(4)
check('en4 CGameLensFlare_ctor', lambda: (SDTest.Call(0x232e80, 'tii', en4lf)[0] == en4lf, en4i(en4lf) != 0))
en4lfo = en4i(en4lf)
check('en4 LensFlare made', lambda: (en4b(en4lfo), en4words(en4lfo + 4, 7), en4fl(en4lfo + 0x14, 4), en4b(en4lfo + 0x2c),
	en4fl(en4lfo + 0x30, 3), en4b(en4lfo + 0x3c), en4words(en4lfo + 0x40, 4)))
check('en4 CGameLensFlare_SetIntensity', lambda: (SDTest.Call(0x232f80, 'tvif', en4lf, 0.75),
	SDTest.Call(0x232f40, 'tfi', en4lf)[0]))
check('en4 CGameLensFlare_SetF30', lambda: (SDTest.Call(0x232fa0, 'tvif', en4lf, -2.5), SDTest.Call(0x232f50, 'tfi', en4lf)[0]))
check('en4 CGameLensFlare_SetF18', lambda: (SDTest.Call(0x232ff0, 'tvif', en4lf, 3.25), SDTest.Call(0x232f60, 'tfi', en4lf)[0]))
check('en4 CGameLensFlare_SetI1c', lambda: (SDTest.Call(0x233010, 'tvii', en4lf, 77), SDTest.Call(0x232f70, 'tii', en4lf)[0]))
check('en4 CGameLensFlare_SetB2c', lambda: (SDTest.Call(0x233020, 'tvii', en4lf, 1), en4b(en4lfo + 0x2c)))
check('en4 CGameLensFlare_SetPos', lambda: (SDTest.Call(0x232fc0, 'tvifff', en4lf, 1.5, -2.0, 300.0), en4fl(en4lfo + 8, 3)))
check('en4 CGameLensFlare_SetOn', lambda: (SDTest.Call(0x232f30, 'tvii', en4lf, 0), en4b(en4lfo)))
check('en4 CGameLensFlare_dtor', lambda: (SDTest.Call(0x232f10, 'tvi', en4lf), 1))
en4free(en4lf)

# ---- NodoR.cpp and RenderCtx.h ----
en4cnt = SDTest.Peek(0x4ad628, 'i')
en4r = en4z(0xa8)
check('en4 CNodoR_ctor', lambda: (SDTest.Call(0x23f3d0, 'tii', en4r)[0] == en4r, SDTest.Peek(0x4ad628, 'i') - en4cnt,
	en4words(en4r, 4), en4fl(en4r + 0x10, 32), en4words(en4r + 0x90, 6)))
en4lk = en4new(0x4c)
en4set(en4lk, (5.0,) * 19)
check('en4 NodoLook_ctor', lambda: (SDTest.Call(0x23f340, 'tii', en4lk)[0] == en4lk, en4fl(en4lk, 19),
	en4i(en4lk + 0x20), en4i(en4lk + 0x24), en4i(en4lk + 0x44), en4i(en4lk + 0x48)))
en4obj = en4z(0x10)
en4poke(en4obj + 4, 0xa5000020)
check('en4 CSistPart_Cast', lambda: SDTest.Call(0x23f310, 'ii', en4obj)[0] == en4obj)
en4own = en4z(0x130)
en4poke(en4r + 4, 2)
en4poke(en4r + 8, en4own)
check('en4 CNodoR_ApplyFX', lambda: SDTest.Call(0x23f540, 'tiiF', en4r, (0.0,) * 16)[0])
en4poke(en4own + 0x128, en4own)
check('en4 CNodoR_ApplyFX lod', lambda: SDTest.Call(0x23f540, 'tiiF', en4r, (0.0,) * 16)[0])
en4poke(en4own + 0x128, 0)
en4poke(en4r + 0xc, en4lk)
for t in (3, 1, 5):
	check('en4 CNodoR_SetLook' + fmt((t,)), lambda t=t: (SDTest.Call(0x23f7f0, 'tvii', en4r, t), en4i(en4lk)))
en4nd = en4z(0x120)
en4poke(en4nd + 8, 1)
check('en4 CNodoR_SetNodo', lambda: (SDTest.Call(0x23f890, 'tvii', en4r, en4nd), en4i(en4r + 0xa4) == en4nd,
	en4i(en4nd + 8), en4words(en4r + 0x9c, 2)))
check('en4 CNodoR_dtor', lambda: (SDTest.Call(0x23f450, 'tvi', en4r), SDTest.Peek(0x4ad628, 'i') - en4cnt,
	en4words(en4r + 0x9c, 3), en4i(en4r + 0xc), en4i(en4nd + 8)))
en4r2 = en4z(0xa8)
SDTest.Call(0x23f3d0, 'tii', en4r2)
en4poke(en4r2 + 8, en4own)
en4poke(en4own + 0x128, en4own)
check('en4 CNodoR_SetNodo lod', lambda: (SDTest.Call(0x23f890, 'tvii', en4r2, en4nd), en4i(en4r2 + 0xa4) == en4nd,
	en4i(en4nd + 8)))
en4poke(en4nd + 8, 2)
SDTest.Call(0x23f450, 'tvi', en4r2)
en4free(en4r2)
en4ctx = en4z(0xa8)
en4poke(en4own + 0xac, en4obj)
check('en4 RenderCtx_Init', lambda: (SDTest.Call(0x23fac0, 'tviii', en4ctx, en4own, 0), en4fl(en4ctx + 0x60, 16),
	en4i(en4ctx + 0xa0) == en4own, en4i(en4ctx + 0xa4) == en4obj))
check('en4 RenderCtx_Init NULL', lambda: (SDTest.Call(0x23fac0, 'tviii', en4ctx, 0, 0), en4words(en4ctx + 0xa0, 2)))
en4slot = en4z(0x10)
en4g1 = en4z(0x98)
en4g2 = en4z(0x98)
check('en4 RenderCtx_SetCanal NULL', lambda: (SDTest.Call(0x23fb10, 'tvii', en4ctx, 0), en4words(en4ctx, 6)))
for f1, f2, b, a1, a2 in ((3.75, 1.25, 0.3, 1, 1), (-2.5, 0.0, 1.0, 1, 0), (0.0, 7.0, 2.0, 0, 1), (10.0, 10.5, -1.0, 1, 1),
		(0.5, 0.5, 0.0, 0, 0), (1.0, 2.0, en4nan, 1, 1), (123456.789, -0.75, 0.999, 1, 1)):
	en4set(en4g1 + 0x58, (f1,))
	en4set(en4g2 + 0x58, (f2,))
	en4poke(en4slot, a1 and en4g1 or 0)
	en4poke(en4slot + 4, a2 and en4g2 or 0)
	en4set(en4slot + 8, (b,))
	check('en4 RenderCtx_SetCanal' + fmt((f1, f2, b, a1, a2)), lambda: (SDTest.Call(0x23fb10, 'tvii', en4ctx + 0x18,
		en4slot), en4i(en4ctx + 0x18) == en4slot, en4i(en4ctx + 0x1c), en4fl(en4ctx + 0x20, 1), en4i(en4ctx + 0x24),
		en4fl(en4ctx + 0x28, 2)))
for p in (en4ctx, en4slot, en4g1, en4g2, en4nd, en4own, en4obj, en4r):
	en4free(p)

# ---- ScrapEngSprite2D.cpp ----
en4c2 = SDTest.Peek(0x4a81f0, 'i')
def en4img(fn):
	p = en4new(0x44)
	r = fn(p)
	return (r == p, en4fl(p, 6), en4words(p + 0x18, 2), en4b(p + 0x20), en4b(p + 0x21), en4b(p + 0x22), en4fl(p + 0x24, 4), en4words(p + 0x34, 2),
		en4i(p + 0x3c) != 0, en4i(p + 0x40), SDTest.Peek(0x4a81f0, 'i') - en4c2, p)
def en4imgfree(p):
	SDTest.Call(0x22d160, 'tvi', p)
	SDTest.Call(0x313634, 'vii', p, 0x44)
en4imgs = []
for fl in (0x20, 0x24, 0x10008, 0x20007, 0x30000, 0x1000, 0x3110):
	r = en4img(lambda p, fl=fl: SDTest.Call(0x22d120, 'tiisi', p, '2D/white.tga', fl)[0])
	check('en4 CPsaImage_ctor' + fmt((fl,)), lambda r=r: r[0:11])
	en4imgs.append(r[11])
for bl in range(7):
	r = en4img(lambda p, bl=bl: SDTest.Call(0x22d0f0, 'tiisiii', p, '2D/white.tga', 0, bl, 0x1020)[0])
	check('en4 CPsaImage_ctorFlags' + fmt((bl,)), lambda r=r: r[0:11])
	en4imgs.append(r[11])
r = en4img(lambda p: SDTest.Call(0x22d0c0, 'tiisii', p, '2D/white.tga', 0, 0x2)[0])
check('en4 CPsaImage_ctorTex', lambda r=r: r[0:11])
en4imgs.append(r[11])
r = en4img(lambda p: SDTest.Call(0x22d150, 'tii', p)[0])
check('en4 CPsaImage_ctorEmpty', lambda r=r: r[0:11])
en4emp = r[11]
check('en4 CPsaImage_LoadName', lambda: (SDTest.Call(0x22d850, 'tvisi', en4emp, '2D/white.tga', 0x8), en4fl(en4emp + 8, 2),
	en4i(en4emp + 0x3c) != 0))
en4imgs.append(en4emp)
en4im = en4imgs[1]
# the draws check the viewport size (put back below); flag 4 draws without the texture size
en4vp = (SDTest.Peek(0x4ad7e8, 'i'), SDTest.Peek(0x4ad7ec, 'i'))
SDTest.Poke(0x4ad7e8, 640)
SDTest.Poke(0x4ad7ec, 480)
check('en4 CPsaImage_GetFrameUV', lambda: SDTest.Call(0x22d320, 'tbiiFF', en4im, 0, (9.0, 9.0), (9.0, 9.0)))
check('en4 CPsaImage_Draw', lambda: en4log(lambda: (SDTest.Call(0x22d3c0, 'tviff', en4im, 10.0, 20.0), en4fl(en4im, 2))))
check('en4 CPsaImage_DrawScaled', lambda: en4log(lambda: (SDTest.Call(0x22d3f0, 'tvifff', en4im, 30.0, 40.0, 2.5),
	en4fl(en4im, 6))))
check('en4 CPsaImage_DrawFrame', lambda: en4log(lambda: (SDTest.Call(0x22d440, 'tviiff', en4im, 0, 5.0, 6.0),
	en4i(en4im + 0x40))))
for md, fl in ((0, 0), (1, 1), (2, 2), (3, 3), (0, 1)):
	check('en4 CPsaImage_DrawFrameFlip' + fmt((md, fl)), lambda md=md, fl=fl: en4log(lambda md=md, fl=fl: (
		SDTest.Call(0x22d480, 'tviiffii', en4im, 0, 1.0, 2.0, md, fl), en4i(en4im + 0x38))))
en4poke(en4im + 0x38, 0)
en4pp = (0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0)
for md, n in ((4, 3), (5, 4), (6, 4)):
	check('en4 CPsaImage_DrawPoly' + fmt((md, n)), lambda md=md, n=n: en4log(lambda md=md, n=n: SDTest.Call(0x22d520,
		'tviFFiii', en4im, en4pp, en4pp, 0, n, md)[0:1]))
SDTest.Poke(0x4ad7e8, en4vp[0])
SDTest.Poke(0x4ad7ec, en4vp[1])
for p in en4imgs:
	check('en4 CPsaImage_dtor', lambda p=p: (en4imgfree(p), SDTest.Peek(0x4a81f0, 'i') - en4c2))

# ---- DebugUtils.cpp ----
en4m0 = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 10.0, 20.0, 30.0, 1.0)
for r, v, h in ((2.0, 8, 0), (1.0, 6, 5), (0.5, 4, 4), (3.0, 3, 2), (1.0, 2, 7), (0.0, 4, 4), (-1.0, 4, 4)):
	check('en4 DebugUtils_DrawSphereM' + fmt((r, v, h)), lambda r=r, v=v, h=h: en4log(lambda r=r, v=v, h=h: SDTest.Call(
		0x219780, 'vFFfiii', en4m0, (3.0, -4.0, 5.5), r, 0x404040, v, h)[0:0]))
check('en4 DebugUtils_DrawSphere', lambda: en4log(lambda: SDTest.Call(0x219d20, 'vFfiii', (0.0, 1.0, 2.0), 1.25, 0xffffff,
	5, 0)[0:0]))
check('en4 Font_SetLanguage', lambda: SDTest.Call(0x21b1e0, 'vs', 'Spanish')[0:0])

# ---- MapElements.cpp: an element without model and the empty list ----
en4el = en4z(4)
check('en4 Elements_ctor', lambda: (SDTest.Call(0x202e10, 'tii', en4el)[0] == en4el, en4i(en4el)))
check('en4 Elements_Update empty', lambda: SDTest.Call(0x203550, 'tvif', en4el, 0.1)[0:0])
check('en4 Elements_Find empty', lambda: SDTest.Call(0x203440, 'tiisffff', en4el, 'x', 0.0, 0.0, 0.0, 1.0)[0])
check('en4 Elements_dtor empty', lambda: (SDTest.Call(0x202e40, 'tvi', en4el), en4i(en4el)))
en4me = en4new(0x2c)
check('en4 cMapElement_ctor', lambda: (SDTest.Call(0x202ca0, 'tiiiii', en4me, 0, 0, 0)[0] == en4me, en4i(en4me) - 0x400000,
	en4i(en4me + 0x24)))
check('en4 cMapElement_Init', lambda: SDTest.Call(0x203310, 'tbi', en4me)[0])
check('en4 cMapElement_Delete', lambda: (SDTest.Call(0x202e60, 'tiii', en4me, 0)[0] == en4me, en4i(en4me) - 0x400000))
SDTest.Call(0x313634, 'vii', en4me, 0x2c)
en4free(en4el)

# ---- Motor.cpp: the small methods, the null input ----
for b in (0, 0x10, 0x30, 0xffff, 0x110, 0x8):
	check('en4 CMotorApp_ConfirmDevice' + fmt((b,)), lambda b=b: SDTest.Call(0x22da10, 'tiiiii', 0, 0, b, 0)[0])
en4fc = SDTest.Peek(0x4a8f78, 'i')
check('en4 CMotorApp_CountFrame', lambda: (SDTest.Call(0x22fd50, 'tii', 0)[0], SDTest.Peek(0x4a8f78, 'i') - en4fc))
SDTest.Poke(0x4a8f78, en4fc)
check('en4 CD3DApplication_Nothing', lambda: SDTest.Call(0x22f420, 'tii', 0)[0])
check('en4 CEntradaNula stubs', lambda: (SDTest.Call(0x22fe40, 'tiii', 0, 5)[0], SDTest.Call(0x22fe50, 'tiii', 0, 5)[0],
	SDTest.Call(0x230430, 'tiiiii', 0, 1, 2, 3)[0], SDTest.Call(0x230870, 'tiii', 0, 1)[0], SDTest.Call(0x230880, 'tii', 0)[0],
	SDTest.Call(0x231170, 'tii', 0)[0], SDTest.Call(0x22f430, 'tviiiii', 0, 1, 2, 3, 4)[0:0]))
en4ent = en4new(0x114)
en4entb = SDTest.Peek(0x4db428, 'i')
check('en4 CEntradaNula_Delete', lambda: (SDTest.Call(0x22d9e0, 'tiii', en4ent, 0)[0] == en4ent, en4i(en4ent) - 0x400000,
	SDTest.Peek(0x4db428, 'i') & 0xff))
SDTest.Poke(0x4db428, en4entb)
check('en4 CEntradaNula_Delete free', lambda: SDTest.Call(0x22d9e0, 'tiii', en4ent, 1)[0] == en4ent)
SDTest.Poke(0x4db428, en4entb)
en4col = en4z(8)
for c in (0x01020304, -1, 0x7f00ff80):
	en4poke(en4col, c)
	en4set(en4col + 4, (0.75,))
	check('en4 Motor_ColorStr' + fmt((c,)), lambda: SDTest.Call(0x22fd60, 'si', en4col)[0])
en4free(en4col)
for b in (0, 1, -1, 2):
	check('en4 Motor_BoolStr' + fmt((b,)), lambda b=b: SDTest.Call(0x22fdb0, 'si', b)[0])
en4mv = en4z(0x19c)
en4set(en4mv, (3.0,) * 64)
en4set(en4mv + 0x100, (3.0,) * 39)
check('en4 cMusicVoice_ctor', lambda: (SDTest.Call(0x231180, 'tii', en4mv)[0] == en4mv, en4b(en4mv), en4words(en4mv + 0x104, 3),
	en4b(en4mv + 0x10c), en4i(en4mv + 0x10d), en4i(en4mv + 0x18c), en4words(en4mv + 0x190, 3)))
check('en4 cMusicVoice_dtor', lambda: SDTest.Call(0x2311b0, 'tvi', en4mv)[0:0])
en4free(en4mv)

# ---- varios.cpp: the debug consoles (off in the tests), the log end ----
check('en4 Dbg state', lambda: (SDTest.Peek(0x4ab8ec, 'i'), SDTest.Peek(0x4ab91c, 'b'), SDTest.Peek(0x4ab91d, 'b'),
	SDTest.Peek(0x4ab920, 'i')))
check('en4 DbgStd_Printf', lambda: SDTest.Call(0x237830, 'vsi', 'x %d', 5)[0:0])
check('en4 DbgWin_Printf', lambda: SDTest.Call(0x237950, 'vsi', 'x %d', 5)[0:0])
check('en4 DbgStd_Open', lambda: SDTest.Call(0x2378f0, 'vi', 0)[0:0])
check('en4 DbgWin_Open', lambda: SDTest.Call(0x237ab0, 'vi', 0)[0:0])
for n, sz in ((3, 10), (10, 10), (12, 10), (0, 4)):
	check('en4 DbgWin_AddLine' + fmt((n, sz)), lambda n=n, sz=sz: SDTest.Call(0x238320, 'vbii', 'abcdefghijklmnop', n, sz)[1][0:17])
check('en4 Window_GetNormalRect NULL', lambda: SDTest.Call(0x238130, 'vii', 0, 0)[0:0])
en4lg = en4z(8)
check('en4 cLog_dtor', lambda: (SDTest.Call(0x2373a0, 'tvi', en4lg), en4words(en4lg, 2)))
en4free(en4lg)
