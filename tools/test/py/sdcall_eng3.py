# engine3d leftovers 0x29c000..0x2b2000 (execfile'd at the end of sdcall.py, Python 1.5.2): the string members, the
# blend table, the keyboard state and the IEntrada classes on objects made here, the frame lock, the 2D lists and
# backgrounds, the mesh view, the lens flare list, the gamma ramp and the shader modes through the D3D log, the texture
# effects on a 128x128 bitmap of the packs, the animation constructors and the camera methods. Every engine variable
# that is changed is put back. Pointers are printed as == or != 0, never as addresses.
def en3new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def en3del(p):
	SDTest.Call('operator_delete', 'vi', p)
def en3calloc(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def en3free(p):
	SDTest.Call('Mem_Free', 'vi', p)
def en3ri(a):
	return SDTest.Peek(a, '@i')
def en3rb(a):
	return SDTest.Peek(a, '@b')
def en3wi(a, v):
	SDTest.Poke(a - 0x400000, v)
def en3wb(a, v):
	w = en3ri(a)
	en3wi(a, (w & -256) | (v & 0xff))
def en3wf(a, vals):
	SDTest.Call('crt_memcpy', 'iiFi', a, vals, 4 * len(vals))
def en3rf(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def en3gi(rva):
	return SDTest.Peek(rva, 'i')
def en3gs(rva, v):
	SDTest.Poke(rva, v)
def en3ints(a, n):
	r = []
	for i in range(n):
		r.append(en3ri(a + 4 * i))
	return r
def en3bytes(a, n):
	r = []
	for i in range(n):
		r.append(en3rb(a + i))
	return r
def en3hash(a, n, step):
	h = 0
	i = 0
	while i < n:
		v = en3ri(a + 4 * i)
		h = (h * 31 + (v & 0xffff)) & 0xffffff
		h = (h * 31 + ((v >> 16) & 0xffff)) & 0xffffff
		i = i + step
	return h
def en3log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
def en3save(rvas):
	r = []
	for a in rvas:
		r.append(en3gi(a))
	return r
def en3restore(rvas, vals):
	for i in range(len(rvas)):
		en3gs(rvas[i], vals[i])
en3timef = en3gi(sym('g_TimeMsF'))
def en3settime(t):
	en3wf(sym('g_TimeMsF') + 0x400000, (t,))

# ---- the blend modes of ScrapEngMaterial.cpp ----
for en3m in range(7):
	check('BlendMode_GetDst/Src' + fmt((en3m,)), lambda m=en3m: (SDTest.Call('BlendMode_GetDst', 'ii', m)[0],
		SDTest.Call('BlendMode_GetSrc', 'ii', m)[0]))

# ---- Cad_Set, Cad_Printf ----
en3slot = en3new(4)
en3wi(en3slot, 0)
def en3cad(s=en3slot):
	p = en3ri(s)
	if p == 0:
		return None
	return SDTest.Peek(p, '@s')
check('Cad_Set abc', lambda s=en3slot: (SDTest.Call('Cad_Set', 'tviz', s, 'abc'), en3cad()))
check('Cad_Set again', lambda s=en3slot: (SDTest.Call('Cad_Set', 'tviz', s, 'longer string'), en3cad()))
check('Cad_Set NULL', lambda s=en3slot: (SDTest.Call('Cad_Set', 'tviz', s, None), en3cad()))
check('Cad_Set NULL empty', lambda s=en3slot: (SDTest.Call('Cad_Set', 'tviz', s, None), en3cad()))
check('Cad_Printf', lambda s=en3slot: (SDTest.Call('Cad_Printf', 'visis', s, '%d-%s', 42, 'xy'), en3cad()))
check('Cad_Printf 2', lambda s=en3slot: (SDTest.Call('Cad_Printf', 'visi', s, 'n=%05d', -3), en3cad()))
check('Cad_Printf NULL', lambda s=en3slot: (SDTest.Call('Cad_Printf', 'viz', s, None), en3cad()))
check('Cad_Printf NULL empty', lambda s=en3slot: (SDTest.Call('Cad_Printf', 'viz', s, None), en3cad()))
en3del(en3slot)

# ---- Box_Set ----
en3box = en3new(24)
check('Box_Set', lambda b=en3box: (SDTest.Call('Box_Set', 'tiiFF', b, (1.0, -2.0, 3.5), (4.0, 5.25, -6.0))[0] == b,
	en3rf(b, 6)))
en3del(en3box)

# ---- teclado.cpp: the key table and the ring of characters ----
en3tk = en3ints(symva('g_TecladoKeys'), 64)
en3tb = en3ints(symva('g_TecladoBuf'), 16)
en3tsc = en3save((sym('g_TecladoBufStart'), sym('g_TecladoBufCount')))
en3gs(sym('g_TecladoBufStart'), 3)
en3gs(sym('g_TecladoBufCount'), 0)
def en3keys():
	r = []
	for k in (0x1c, 0x1d, 0x1e, 0x2a, 0x30, 0x36, 0x38, 0x9c, 0x9d, 0xb6, 0xb8, 0xff):
		r.append(en3rb(symva('g_TecladoKeys') + k))
	return (r, en3gi(sym('g_TecladoBufStart')), en3gi(sym('g_TecladoBufCount')))
check('Teclado_Clear', lambda: (SDTest.Call('Teclado_Clear', 'v'), en3hash(symva('g_TecladoKeys'), 64, 1)))
for en3kd in ((0x41, 0x001e0001), (0x42, 0x401e0001), (0x2a, 0x00360001), (0x43, 0x01360001), (0x44, 0x011d0001),
		(0x45, 0x011c0001), (0x46, 0x01380001), (0x47, 0x00300001), (0x1234, 0x40ff0001), (0x48, 0x00ff0001)):
	check('Teclado_KeyDown' + fmt(en3kd), lambda a=en3kd: (SDTest.Call('Teclado_KeyDown', 'vii', a[0], a[1]), en3keys()))
en3dest = en3new(80)
def en3chars(n, d=en3dest):
	SDTest.Call('crt_memcpy', 'iiFi', d, (0.0,) * 20, 80)
	r = SDTest.Call('Teclado_GetChars', 'iii', d, n)[0]
	w = en3ints(d, 20)
	c = []
	for i in range(20):
		c.append(w[i] & 0xffff)
		c.append((w[i] >> 16) & 0xffff)
	return (r, c)
for en3n in (0, -1, 1, 3, 9, 31, 32, 40):
	check('Teclado_GetChars' + fmt((en3n,)), lambda n=en3n: en3chars(n))
for en3i in range(30):
	SDTest.Call('Teclado_KeyDown', 'vii', 0x61 + en3i, 0x00100001)
check('Teclado_GetChars wrapped', lambda: (en3chars(32), en3chars(5), en3gi(sym('g_TecladoBufCount'))))
for en3ku in (0x001e0001, 0x01360001, 0x011d0001, 0x011c0001, 0x00380001, 0x01b80001):
	check('Teclado_KeyUp' + fmt((en3ku,)), lambda a=en3ku: (SDTest.Call('Teclado_KeyUp', 'vii', 7, a), en3keys()))
en3del(en3dest)
for en3i in range(64):
	en3gs(sym('g_TecladoKeys') + 4 * en3i, en3tk[en3i])
for en3i in range(16):
	en3gs(sym('g_TecladoBuf') + 4 * en3i, en3tb[en3i])
en3restore((sym('g_TecladoBufStart'), sym('g_TecladoBufCount')), en3tsc)

# ---- the paused time (only what does not depend on the clock) ----
en3ts = en3save((sym('g_TiempoPausado'), sym('g_TiempoReanudado'), sym('g_TiempoPausa')))
en3gs(sym('g_TiempoPausa'), 12345)
en3gs(sym('g_TiempoReanudado'), 0)
check('Tiempo_Pausa on again', lambda: (SDTest.Call('Tiempo_Pausa', 'vi', 1), en3gi(sym('g_TiempoPausa')), en3gi(sym('g_TiempoReanudado'))))
check('Tiempo_Pausa off', lambda: (SDTest.Call('Tiempo_Pausa', 'vi', 0), en3gi(sym('g_TiempoPausa')), en3gi(sym('g_TiempoReanudado'))))
check('Tiempo_Pausa off again', lambda: (SDTest.Call('Tiempo_Pausa', 'vi', 0), en3gi(sym('g_TiempoPausa')), en3gi(sym('g_TiempoReanudado'))))
check('Tiempo_Pausa on', lambda: (SDTest.Call('Tiempo_Pausa', 'vi', 1), en3gi(sym('g_TiempoPausa')) != 0, en3gi(sym('g_TiempoReanudado'))))
en3restore((sym('g_TiempoPausado'), sym('g_TiempoReanudado'), sym('g_TiempoPausa')), en3ts)
check('Tiempo_Get', lambda: SDTest.Call('Tiempo_Get', 'i')[0] != 0)

# ---- entrada.cpp: objects made here; the flag of the one of the engine is cleared for them and put back ----
en3inic = en3gi(sym('g_EntradaInic'))
check('Entrada inic', lambda: en3inic & 0xff)
en3gs(sym('g_EntradaInic'), en3inic & -256)
def en3ent(e, size):
	return (en3ri(e) - 0x400000, en3hash(e + 4, 0x110 / 4, 1), en3bytes(e + 0x10, 4), en3ints(e + 4, 3),
		SDTest.Peek('g_EntradaInic', 'b'))
if 1:
	en3w = en3new(0x11c)
	check('CEntradaWindows_ctor', lambda w=en3w: (SDTest.Call('CEntradaWindows_ctor', 'tii', w)[0] == w, en3ent(w, 0x11c),
		en3rb(w + 0x114), en3ri(w + 0x118)))
	for en3msg in ((0x201, 0), (0x204, 0), (0x207, 0), (0x202, 0), (0x203, 0), (0x206, 5), (0x205, 0), (0x208, 0),
			(0x204, 1), (0x100, 0)):
		check('CEntradaWindows_WndProc' + fmt(en3msg), lambda w=en3w, m=en3msg: (SDTest.Call('CEntradaWindows_WndProc', 'tviiiii', w,
			0, m[0], m[1], 0), en3ri(w + 0x118)))
	en3wi(en3w + 0x118, 7)
	check('CEntradaWindows_ReadMouse off', lambda w=en3w: (SDTest.Call('CEntradaWindows_ReadMouse', 'tiii', w, 0)[0], en3ints(w + 4, 4)))
	check('CEntradaWindows_ReadKeys off', lambda w=en3w: SDTest.Call('CEntradaWindows_ReadKeys', 'tii', w)[0])
	en3k6 = en3ints(symva('g_TecladoKeys'), 64)
	for en3i in range(64):
		en3gs(sym('g_TecladoKeys') + 4 * en3i, (en3i * 0x01020304) & 0x01010001)
	en3wb(en3w + 0x114, 1)
	check('CEntradaWindows_ReadKeys', lambda w=en3w: (SDTest.Call('CEntradaWindows_ReadKeys', 'tii', w)[0], en3hash(w + 0x14, 64, 1),
		en3bytes(w + 0x14, 8)))
	for en3i in range(64):
		en3gs(sym('g_TecladoKeys') + 4 * en3i, en3k6[en3i])
	en3wb(en3w + 0x114, 0)
	check('CEntradaWindows_AcquireKeys', lambda w=en3w: (SDTest.Call('CEntradaWindows_AcquireKeys', 'tiii', w, 1)[0],
		SDTest.Call('CEntradaWindows_AcquireKeys', 'tiii', w, 0)[0]))
	check('CEntradaWindows_Init', lambda w=en3w: SDTest.Call('CEntradaWindows_Init', 'tiiiii', w, 0, 0, 1)[0])
	check('CEntradaWindows_Release', lambda w=en3w: SDTest.Call('CEntradaWindows_Release', 'tii', w)[0])
	check('CEntradaWindows_Delete', lambda w=en3w: (SDTest.Call('CEntradaWindows_Delete', 'tiii', w, 1)[0] == w,
		SDTest.Peek('g_EntradaInic', 'b')))
	en3d = en3new(0x120)
	check('CEntradaDInput_ctor', lambda d=en3d: (SDTest.Call('CEntradaDInput_ctor', 'tii', d)[0] == d, en3ent(d, 0x120),
		en3ints(d + 0x114, 3)))
	check('CEntradaDInput_AcquireMouse', lambda d=en3d: (SDTest.Call('CEntradaDInput_AcquireMouse', 'tiii', d, 1)[0],
		SDTest.Call('CEntradaDInput_AcquireMouse', 'tiii', d, 0)[0]))
	check('CEntradaDInput_AcquireKeys', lambda d=en3d: (SDTest.Call('CEntradaDInput_AcquireKeys', 'tiii', d, 1)[0],
		SDTest.Call('CEntradaDInput_AcquireKeys', 'tiii', d, 0)[0]))
	check('CEntradaDInput_ReadMouse', lambda d=en3d: (SDTest.Call('CEntradaDInput_ReadMouse', 'tiii', d, 0)[0], en3ints(d + 4, 4)))
	check('CEntradaDInput_ReadKeys', lambda d=en3d: SDTest.Call('CEntradaDInput_ReadKeys', 'tii', d)[0])
	for en3msg in ((6, 1), (6, 0x10001), (6, 0), (0x211, 0), (0x212, 0), (0x231, 0), (0x232, 0), (0x210, 0), (0x201, 0)):
		check('CEntradaDInput_WndProc' + fmt(en3msg), lambda d=en3d, m=en3msg: (SDTest.Call('CEntradaDInput_WndProc', 'tviiiii', d,
			0, m[0], m[1], 0), en3ints(d + 0x114, 3)))
	check('CEntradaDInput_Release', lambda d=en3d: (SDTest.Call('CEntradaDInput_Release', 'tii', d)[0], en3ints(d + 0x114, 3)))
	check('CEntradaDInput_Delete', lambda d=en3d: (SDTest.Call('CEntradaDInput_Delete', 'tiii', d, 1)[0] == d,
		SDTest.Peek('g_EntradaInic', 'b')))
	en3e = en3new(0x114)
	check('IEntrada_ctor', lambda e=en3e: (SDTest.Call('IEntrada_ctor', 'tii', e)[0] == e, en3ent(e, 0x114)))
	check('IEntrada_dtor', lambda e=en3e: (SDTest.Call('IEntrada_dtor', 'tvi', e), en3ri(e) - 0x400000,
		SDTest.Peek('g_EntradaInic', 'b')))
	check('IEntrada_Delete', lambda e=en3e: (SDTest.Call('IEntrada_Delete', 'tiii', e, 0)[0] == e, SDTest.Peek('g_EntradaInic', 'b')))
	en3del(en3e)
en3gs(sym('g_EntradaInic'), en3inic)

# ---- RenderLock.cpp through the D3D log ----
en3rl = (sym('g_RenderLock'), sym('g_RenderLockFrames'), sym('g_RenderLockNoise'), sym('g_TextLockIdx'))
en3rls = en3save(en3rl)
def en3rlptrs():
	r = []
	for i in range(5):
		r.append(en3gi(sym('g_TextLock') + 4 * i) != 0)
	return (r, en3gi(sym('g_TextLockIdx')))
en3gs(sym('g_RenderLock'), 0)
check('RenderLock_Frame off', lambda: en3log(lambda: SDTest.Call('RenderLock_Frame', 'v')))
check('RenderLock_ReleaseD3D', lambda: (en3log(lambda: SDTest.Call('RenderLock_ReleaseD3D', 'v')), en3rlptrs()))
check('RenderLock_InitD3D', lambda: (en3log(lambda: SDTest.Call('RenderLock_InitD3D', 'v')), en3rlptrs()))
for en3buf, en3show in ((2, 0), (3, 1), (1, 0), (0, 0), (4, 0)):
	en3gs(sym('g_RenderLockFrames'), en3buf)
	en3gs(sym('g_RenderLockNoise'), en3show)
	en3gs(sym('g_TextLockIdx'), 0)
	check('RenderLock_DrawMem' + fmt((en3buf, en3show)), lambda: (en3log(lambda: SDTest.Call('RenderLock_DrawMem', 'v')),
		en3rlptrs()))
	check('RenderLock_Clear' + fmt((en3buf, en3show)), lambda: (en3log(lambda: SDTest.Call('RenderLock_Clear', 'v')),
		en3rlptrs()))
en3gs(sym('g_RenderLockFrames'), 2)
check('RenderLock_CopyBack', lambda: en3log(lambda: SDTest.Call('RenderLock_CopyBack', 'v')))
for en3mode in (1, 2, 3, 4):
	en3gs(sym('g_RenderLock'), en3mode)
	check('RenderLock_Frame' + fmt((en3mode,)), lambda: (en3log(lambda: SDTest.Call('RenderLock_Frame', 'v')), en3rlptrs()))
en3restore(en3rl, en3rls)

# ---- ListaGrf2D.cpp (the draws need a viewport, 640x480 while they run) ----
en3vp = en3save((sym('g_ViewportW'), sym('g_ViewportH')))
en3gs(sym('g_ViewportW'), 640)
en3gs(sym('g_ViewportH'), 480)
en3l = en3new(8)
en3fakes = (0x1000, 0x2000, 0x3000, 0x4000)
def en3lista(l=en3l):
	r = []
	n = en3ri(l)
	while n and len(r) < 10:
		g = en3ri(n + 4)
		k = g
		for i in range(4):
			if g == en3fakes[i]:
				k = 'f%d' % i
		r.append(k)
		last = n
		n = en3ri(n)
	u = en3ri(l + 4)
	return (r, en3ri(l) == 0, u == 0 or en3ri(u) == 0)
check('CListaGrf2D_ctor', lambda l=en3l: (SDTest.Call('CListaGrf2D_ctor', 'tii', l)[0] == l, en3ints(l, 2)))
for en3g in en3fakes:
	check('CListaGrf2D_Add' + fmt((en3g,)), lambda l=en3l, g=en3g: (SDTest.Call('CListaGrf2D_Add', 'tvii', l, g), en3lista()))
for en3g in (0x2000, 0x1000, 0x4000, 0x3000):
	check('CListaGrf2D_Remove' + fmt((en3g,)), lambda l=en3l, g=en3g: (SDTest.Call('CListaGrf2D_Remove', 'tvii', l, g),
		en3lista()))
check('CListaGrf2D_DeleteAll empty', lambda l=en3l: (SDTest.Call('CListaGrf2D_DeleteAll', 'tvi', l), en3ints(l, 2)))
en3grfs = []
for en3i in range(3):
	en3grfs.append(SDTest.Call('CGrafico2D_ctorTex', 'tiiszi', en3new(0x44), 'bmp/rgb_white.tga', None, 0)[0])
for en3g in en3grfs:
	SDTest.Call('CListaGrf2D_Add', 'tvii', en3l, en3g)
check('CListaGrf2D_Render', lambda l=en3l: en3log(lambda l=l: SDTest.Call('CListaGrf2D_Render', 'tvi', l)))
check('CListaGrf2D_Delete', lambda l=en3l, g=en3grfs[1]: (SDTest.Call('CListaGrf2D_Delete', 'tvii', l, g),
	en3ri(en3ri(l) + 4) == en3grfs[0], en3ri(en3ri(l + 4) + 4) == en3grfs[2]))
check('CListaGrf2D_DeleteAll', lambda l=en3l: (SDTest.Call('CListaGrf2D_DeleteAll', 'tvi', l), en3ints(l, 2)))
en3del(en3l)

# ---- fondo2d.cpp ----
def en3fondo(f):
	return (en3ints(f, 3), en3ri(f + 0xc), en3rf(f + 0x10, 1), en3ri(f + 0x14) != 0)
en3f = en3new(0x18)
check('CFondo2D_ctor', lambda f=en3f: (SDTest.Call('CFondo2D_ctor', 'tii', f)[0] == f, en3fondo(f),
	SDTest.Peek(en3ri(f + 0x14) + 4, '@p')))
check('CFondo2D_Render', lambda f=en3f: en3log(lambda f=f: SDTest.Call('CFondo2D_Render', 'tvi', f)))
check('CBmp_dtor', lambda f=en3f: (SDTest.Call('CBmp_dtor', 'tvi', f), en3fondo(f)))
en3f2 = en3new(0x18)
check('CFondo2D_ctorFile', lambda f=en3f2: (SDTest.Call('CFondo2D_ctorFile', 'tiis', f, '2d/textures/menu/escudo-b.bump.bmp')[0] == f,
	en3ri(f), en3ri(f + 4), en3ri(f + 8) != 0, en3ri(f + 0x14) == en3ri(en3f + 0x14)))
check('CBmp_dtor file', lambda f=en3f2: (SDTest.Call('CBmp_dtor', 'tvi', f), en3ints(f, 3)))
en3del(en3f)
en3del(en3f2)
en3fr = (sym('g_FondoPendiente'), sym('g_FondoFrame'), sym('g_FondoGrf'), sym('g_FondoColor'), sym('g_FondoColorNuevo'))
en3frs = en3save(en3fr)
en3fact = en3ints(symva('g_FondoActual'), 0x108 / 4)
en3fnue = en3ints(symva('g_FondoNuevo'), 0x108 / 4)
def en3fstate():
	return (en3rb(symva('g_FondoPendiente')), SDTest.Peek('g_FondoActual', 's'), SDTest.Peek('g_FondoNuevo', 's'), en3gi(sym('g_FondoColor')),
		en3gi(sym('g_FondoColorNuevo')), en3gi(sym('g_FondoGrf')) != 0)
check('Fondo2D state', en3fstate)
check('Fondo2D_Set', lambda: (SDTest.Call('Fondo2D_Set', 'vsii', 'bmp/rgb_white.tga', 0x123456, 1), en3fstate()))
check('Fondo2D_Set NULL', lambda: (SDTest.Call('Fondo2D_Set', 'vzii', None, 0x80ff00ff, 1), en3fstate()))
check('Fondo2D_Reset', lambda: (SDTest.Call('Fondo2D_Reset', 'v'), en3fstate()))
if en3frs[2] == 0:
	en3gs(sym('g_FondoFrame'), -7)
	SDTest.Call('Fondo2D_Set', 'vsii', 'bmp/rgb_white.tga', 0x445566, 1)
	check('Fondo2D_Render', lambda: (en3log(lambda: SDTest.Call('Fondo2D_Render', 'v')), en3fstate(),
		en3gi(sym('g_FondoFrame')) == en3gi(sym('g_FrameCount'))))
	check('Fondo2D_Render not pending', lambda: (en3log(lambda: SDTest.Call('Fondo2D_Render', 'v')), en3fstate()))
	check('Fondo2D_Release', lambda: (SDTest.Call('Fondo2D_Release', 'v'), en3fstate()))
check('Fondo2D_Release none', lambda: (SDTest.Call('Fondo2D_Release', 'v'), en3fstate()))
for en3i in range(0x108 / 4):
	en3gs(sym('g_FondoActual') + 4 * en3i, en3fact[en3i])
	en3gs(sym('g_FondoNuevo') + 4 * en3i, en3fnue[en3i])
en3restore(en3fr, en3frs)
en3restore((sym('g_ViewportW'), sym('g_ViewportH')), en3vp)

# ---- efec_malla.cpp ----
en3em = (sym('g_EfecMallaOn'), sym('g_EfecMallaStencil'), sym('g_NumPantallas'), sym('g_NumModelos'))
en3ems = en3save(en3em)
en3emp = en3ints(symva('g_Pantallas'), 20)
en3emk = en3ri(symva('g_TecladoKeys') + 0x1c)
en3emk2 = en3ri(symva('g_TecladoKeys') + 0x30)
en3gs(sym('g_NumPantallas'), 0)
en3gs(sym('g_NumModelos'), 0)
def en3emstate():
	return (en3save(en3em), en3ri(symva('g_Pantallas')) == 0x5000, en3ri(symva('g_Pantallas') + 0x4) == 0x5004, en3ri(symva('g_Modelos')) == 0x6000,
		en3rb(symva('g_TecladoKeys') + 0x32), en3rb(symva('g_TecladoKeys') + 0x1d))
check('EfecMalla_AddPantalla', lambda: (SDTest.Call('EfecMalla_AddPantalla', 'vi', 0x5000), SDTest.Call('EfecMalla_AddPantalla', 'vi', 0x5004),
	en3emstate()))
check('EfecMalla_AddModelo', lambda: (SDTest.Call('EfecMalla_AddModelo', 'vi', 0x6000), en3emstate()))
en3gs(sym('g_NumPantallas'), 0)
en3gs(sym('g_NumModelos'), 0)
en3gs(sym('g_EfecMallaOn'), 0)
check('EfecMalla_Render off', lambda: (en3log(lambda: SDTest.Call('EfecMalla_Render', 'v')), en3emstate()))
check('EfecMalla_RenderModelos', lambda: en3log(lambda: SDTest.Call('EfecMalla_RenderModelos', 'v')))
check('EfecMalla_RenderPantallas', lambda: en3log(lambda: SDTest.Call('EfecMalla_RenderPantallas', 'v')))
en3wb(symva('g_TecladoKeys') + 0x32, 1)
check('EfecMalla_Render M only', lambda: (en3log(lambda: SDTest.Call('EfecMalla_Render', 'v')), en3emstate()))
en3wb(symva('g_TecladoKeys') + 0x1d, 1)
check('EfecMalla_Render Ctrl+M', lambda: (en3log(lambda: SDTest.Call('EfecMalla_Render', 'v')), en3emstate(),
	en3ri(symva('g_EfecMallaMat') + 0x40), en3ri(symva('g_EfecMallaMat') + 0x1c)))
check('EfecMalla_Render on', lambda: (en3log(lambda: SDTest.Call('EfecMalla_Render', 'v')), en3emstate()))
en3gs(sym('g_EfecMallaStencil'), 1)
check('EfecMalla_Render stencil', lambda: (en3log(lambda: SDTest.Call('EfecMalla_Render', 'v')), en3emstate()))
en3wi(symva('g_TecladoKeys') + 0x1c, en3emk)
en3wi(symva('g_TecladoKeys') + 0x30, en3emk2)
for en3i in range(20):
	en3gs(sym('g_Pantallas') + 4 * en3i, en3emp[en3i])
en3restore(en3em, en3ems)

# ---- the window helpers ----
check('Win_SetTextf', lambda: SDTest.Call('Win_SetTextf', 'iisi', 0, 'x%d', 5)[0])
check('Dlg_SetFloat', lambda: SDTest.Call('Dlg_SetFloat', 'biifi', 0, 1000, 1.5, 2)[0])
check('Dlg_GetFloat', lambda: SDTest.Call('Dlg_GetFloat', 'biiF', 0, 1000, (2.5,)))

# ---- LensFlare.cpp: the list and the members ----
en3lfl = en3ints(symva('g_LensFlareRender'), 8)
en3lfr = (sym('g_nLensFlareRender'), sym('g_LensFlareMat'))
en3lfs = en3save(en3lfr)
en3settime(4321.75)
en3gs(sym('g_nLensFlareRender'), 0)
en3lfa = en3new(0x50)
en3lfb = en3new(0x50)
en3lfc = en3new(0x50)
def en3lfname(p):
	if p == en3lfa:
		return 'a'
	if p == en3lfb:
		return 'b'
	if p == en3lfc:
		return 'c'
	if p == 0:
		return 0
	return '?'
def en3lflist():
	r = []
	for i in range(8):
		r.append(en3lfname(en3ri(symva('g_LensFlareRender') + 4 * i)))
	return (en3gi(sym('g_nLensFlareRender')), r, en3ri(en3lfa + 4), en3ri(en3lfb + 4), en3ri(en3lfc + 4), en3rb(en3lfa),
		en3rb(en3lfb), en3rb(en3lfc))
def en3lf(p):
	w = en3ints(p, 20)
	return (w[0] & 0xff, w[1], en3rf(p + 8, 6), w[7], en3rf(p + 0x20, 1), w[11] & 0xff, en3rf(p + 0x30, 3),
		w[15] & 0xff, en3rf(p + 0x40, 3), w[19])
for en3p in (en3lfa, en3lfb, en3lfc):
	check('CLensFlare_ctor', lambda p=en3p: (SDTest.Call('CLensFlare_ctor', 'tii', p)[0] == p, en3lf(p)))
check('CLensFlare_SetOn a', lambda: (SDTest.Call('CLensFlare_SetOn', 'tvii', en3lfa, 1), en3lflist(), en3ri(en3lfa + 0x4c)))
check('LensFlare_Set b', lambda: (SDTest.Call('LensFlare_Set', 'vii', en3lfb, 1), en3lflist(), en3ri(en3lfb + 0x4c)))
check('CLensFlare_SetOn c', lambda: (SDTest.Call('CLensFlare_SetOn', 'tvii', en3lfc, 1), en3lflist()))
check('CLensFlare_SetOn a again', lambda: (SDTest.Call('CLensFlare_SetOn', 'tvii', en3lfa, 1), en3lflist()))
check('CLensFlare_SetOn b off', lambda: (SDTest.Call('CLensFlare_SetOn', 'tvii', en3lfb, 0), en3lflist()))
check('LensFlare_Update b off', lambda: (SDTest.Call('LensFlare_Update', 'vi', en3lfb), en3lflist()))
check('LensFlare_Update NULL', lambda: (SDTest.Call('LensFlare_Update', 'vi', 0), en3lflist()))
check('LensFlare_Set b', lambda: (SDTest.Call('LensFlare_Set', 'vii', en3lfb, 1), en3lflist()))
check('CLensFlare_dtor a', lambda: (SDTest.Call('CLensFlare_dtor', 'tvi', en3lfa), en3lflist()))
check('CLensFlare_dtor a off', lambda: (SDTest.Call('CLensFlare_dtor', 'tvi', en3lfa), en3lflist()))
check('LensFlare_Set a', lambda: (SDTest.Call('LensFlare_Set', 'vii', en3lfa, 1), en3lflist()))
check('LensFlare_ClearAll', lambda: (SDTest.Call('LensFlare_ClearAll', 'i')[0], en3lflist()))
check('LensFlare_ClearAll empty', lambda: (SDTest.Call('LensFlare_ClearAll', 'i')[0], en3lflist()))
check('LensFlare_Release', lambda: SDTest.Call('LensFlare_Release', 'i')[0])
en3gs(sym('g_LensFlareRender'), 0x11)
en3gs(sym('g_LensFlareRender') + 0x1c, 0x22)
en3gs(sym('g_nLensFlareRender'), 5)
check('LensFlare_InitList', lambda: (SDTest.Call('LensFlare_InitList', 'i')[0], en3lflist()))
check('CLensFlare_SetIntens', lambda: (SDTest.Call('CLensFlare_SetIntens', 'tvif', en3lfa, 0.75), en3lf(en3lfa)))
check('CLensFlare_SetTraceInterval', lambda: (SDTest.Call('CLensFlare_SetTraceInterval', 'tvif', en3lfa, 0.125), en3lf(en3lfa)))
check('CLensFlare_SetMargen', lambda: (SDTest.Call('CLensFlare_SetMargen', 'tvif', en3lfa, 0.3), en3lf(en3lfa)))
check('CLensFlare_SetPos', lambda: (SDTest.Call('CLensFlare_SetPos', 'tvifff', en3lfa, 1.5, -2.25, 30.0), en3lf(en3lfa)))
check('CLensFlare_SetEscala', lambda: (SDTest.Call('CLensFlare_SetEscala', 'tvif', en3lfa, 2.0), en3lf(en3lfa)))
check('CLensFlare_SetTipo', lambda: (SDTest.Call('CLensFlare_SetTipo', 'tvii', en3lfa, 3), en3lf(en3lfa)))
check('CLensFlare_SetRelativo', lambda: (SDTest.Call('CLensFlare_SetRelativo', 'tvii', en3lfa, 0x101), en3lf(en3lfa)))
check('LensFlare_Init', lambda: (SDTest.Call('LensFlare_Init', 'i')[0], en3gi(sym('g_LensFlareMat')) != 0,
	en3gi(sym('g_LensFlareMat')) == en3lfs[1]))
for en3i in range(8):
	en3gs(sym('g_LensFlareRender') + 4 * en3i, en3lfl[en3i])
en3restore(en3lfr, en3lfs)
en3del(en3lfa)
en3del(en3lfb)
en3del(en3lfc)

# ---- RenderGamma.cpp through the D3D log ----
en3gm = (sym('g_VideoGammaRGB'), sym('g_VideoScaleBlue'), sym('g_VideoScaleRed'), sym('g_VideoScaleRGB'), sym('g_VideoScaleGreen'), sym('g_Gamma2x'))
en3gms = en3save(en3gm)
en3gmc = (sym('g_GammaLast2x'), sym('g_GammaLastRGB'), sym('g_GammaLastScale'), sym('g_GammaLastRed'), sym('g_GammaLastGreen'), sym('g_GammaLastBlue'))
en3gmcs = en3save(en3gmc)
def en3gamma(g, b, r, s, gr, x2):
	en3wf(sym('g_VideoGammaRGB') + 0x400000, (g,))
	en3wf(sym('g_VideoScaleBlue') + 0x400000, (b,))
	en3wf(sym('g_VideoScaleRed') + 0x400000, (r,))
	en3wf(sym('g_VideoScaleRGB') + 0x400000, (s,))
	en3wf(sym('g_VideoScaleGreen') + 0x400000, (gr,))
	en3gs(sym('g_Gamma2x'), x2)
	return (en3log(lambda: SDTest.Call('RenderGamma_Begin', 'v')), en3gi(sym('g_GammaLast2x')), en3rf(symva('g_GammaLastRGB'), 5))
for en3ga in ((1.0, 1.0, 1.0, 1.0, 1.0, 0), (1.0, 1.0, 1.0, 1.0, 1.0, 0), (0.5, 1.0, 1.0, 1.0, 1.0, 0),
		(2.2, 1.0, 1.0, 1.0, 1.0, 0), (1.3, 0.9, 1.1, 1.0, 0.8, 0), (1.0, 1.0, 1.0, 1.5, 1.0, 1),
		(0.7, 0.2, 3.0, 0.8, 1.0, 1), (1.0, 1.0, 1.0, 0.0, 1.0, 0), (5.0, 1.0, 1.0, 1.0, 1.0, 0)):
	check('RenderGamma_Begin' + fmt(en3ga), lambda a=en3ga: en3gamma(a[0], a[1], a[2], a[3], a[4], a[5]))
check('RenderGamma_ReleaseD3D', lambda: (SDTest.Call('RenderGamma_ReleaseD3D', 'v'), en3gi(sym('g_GammaLast2x')), en3rf(symva('g_GammaLastRGB'), 5)))
en3restore(en3gm, en3gms)
en3restore(en3gmc, en3gmcs)
en3wf(symva('g_GammaLastRGB'), (-1.0,))
SDTest.Call('RenderGamma_Begin', 'v')
check('RenderGamma restored', lambda: en3save(en3gmc) == en3gmcs)

# ---- RenderShader.cpp ----
en3sn = []
for en3i in range(32):
	en3s = SDTest.Peek(sym('g_ShaderTab') + 0x4 + 0x18 * en3i, 'p')
	if en3s == None:
		break
	en3sn.append(en3s)
for en3s in en3sn:
	check('Shader_Find' + fmt((en3s,)), lambda s=en3s: SDTest.Call('Shader_Find', 'is', s)[0])
check('Shader_Find lower', lambda: SDTest.Call('Shader_Find', 'is', 'glowmap')[0])
en3id = en3new(12)
check('CMatId_ctor', lambda i=en3id: (SDTest.Call('CMatId_ctor', 'tii', en3id)[0] == en3id, en3ri(en3id),
	en3ri(en3id + 8)))
check('CMatId_dtor', lambda: (SDTest.Call('CMatId_dtor', 'tvi', en3id), en3ri(en3id)))
en3sh = (sym('g_EnvBump'), sym('g_EnvMap'), sym('g_GlowMap'), sym('g_PSOn'), sym('g_ShaderModeMask'))
en3shs = en3save(en3sh)
en3mat = SDTest.Call('CMaterial_ctor', 'tii', en3new(0xb0))[0]
en3maps = (en3calloc(0x60), en3calloc(0x60), en3calloc(0x60))
def en3reg(env, bump, glow, ps, alpha, m0, m3, m4, b3b, transp, src, dst, atrib):
	en3gs(sym('g_EnvBump'), bump)
	en3gs(sym('g_EnvMap'), env)
	en3gs(sym('g_GlowMap'), glow)
	en3gs(sym('g_PSOn'), ps)
	en3wb(en3maps[0] + 0x54, alpha)
	en3wi(en3mat + 0x9c, m0 and en3maps[0])
	en3wi(en3mat + 0xa8, m3 and en3maps[1])
	en3wi(en3mat + 0xac, m4 and en3maps[2])
	en3wb(en3mat + 0x3b, b3b)
	en3wb(en3mat + 0x3a, transp)
	en3wi(en3mat + 0x30, src)
	en3wi(en3mat + 0x34, dst)
	w = en3ri(en3mat + 0x3c)
	en3wi(en3mat + 0x3c, (w & -65536) | atrib)
	en3wi(en3id, -1)
	SDTest.Call('CMatId_Register', 'tvii', en3id, en3mat)
	return (en3ri(en3id), en3bytes(en3id + 4, 3), en3ri(en3id + 8), en3gi(sym('g_ShaderModeMask')))
for en3sa in ((0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 5, 6, 0), (1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 5, 6, 0),
		(1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 5, 6, 0), (1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 5, 6, 0),
		(1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 5, 6, 0), (1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 5, 6, 0),
		(1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 5, 6, 0), (0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 5, 6, 0),
		(1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 5, 6, 0x200), (1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 5, 6, 0x200),
		(0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 3, 0x800), (0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 3, 0),
		(0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 2, 3, 0x800), (1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 5, 6, 0),
		(1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 3, 0x800)):
	check('CMatId_Register' + fmt(en3sa), lambda a=en3sa: en3reg(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
		a[8], a[9], a[10], a[11], a[12]))
SDTest.Call('Cad_Set', 'tviz', en3mat + 4, 'x(+shader: Glowmap) y')
check('CMatId_Register shader name', lambda: en3reg(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 6, 0x200))
check('CMatId_Register shader name off', lambda: en3reg(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 6, 0))
SDTest.Call('Cad_Set', 'tviz', en3mat + 4, None)
check('CMatId_Check current', lambda: (SDTest.Call('CMatId_Check', 'tbii', en3id, en3mat)[0], en3ri(en3id)))
en3gs(sym('g_EnvBump'), 0)
en3gs(sym('g_EnvMap'), 0)
en3gs(sym('g_GlowMap'), 0)
check('RenderGamma_LogShaderMask', lambda: (SDTest.Call('RenderGamma_LogShaderMask', 'v'), en3gi(sym('g_ShaderModeMask'))))
check('CMatId_Check stale', lambda: (SDTest.Call('CMatId_Check', 'tbii', en3id, en3mat)[0], en3ri(en3id),
	en3ri(en3id + 8)))
en3wi(en3mat + 0x9c, 0)
en3wi(en3id, -1)
check('CMatId_Check deferred', lambda: (SDTest.Call('CMatId_Check', 'tbii', en3id, en3mat)[0], en3ri(en3id),
	en3bytes(en3id + 4, 3)))
en3restore(en3sh, en3shs)
check('RenderGamma_LogShaderMask restored', lambda: (SDTest.Call('RenderGamma_LogShaderMask', 'v'), en3gi(sym('g_ShaderModeMask')) == en3shs[4]))
en3wi(en3mat + 0x9c, 0)
en3wi(en3mat + 0xa8, 0)
en3wi(en3mat + 0xac, 0)
SDTest.Call('CMaterial_dtor', 'tvi', en3mat)
en3del(en3mat)
for en3p in en3maps:
	en3free(en3p)
en3del(en3id)

# ---- fxtextura.cpp ----
for en3s in ('abc', 'x(fxlava)', '(FXSCROLL)y', 'a(b)(+fx:NewsPanel)', '(fxlav', '((fxlava)', 'q(+fx:newspanel)',
		'(fxscroll(fxlava)'):
	check('FXTextura_Kind' + fmt((en3s,)), lambda s=en3s: SDTest.Call('FXTextura_Kind', 'bs', s)[0])
en3mapa = en3calloc(0x60)
SDTest.Call('Cad_Set', 'tviz', en3mapa, 'mapa (fx)')
SDTest.Call('Cad_Set', 'tviz', en3mapa + 4, '2d/textures/menu/escudo-b.bump.bmp')
check('FXTextura_Create unknown', lambda: SDTest.Call('FXTextura_Create', 'iii', 0, en3mapa)[0])
check('FXTextura_Create 4', lambda: SDTest.Call('FXTextura_Create', 'iii', 4, en3mapa)[0])
en3np = SDTest.Call('FXTextura_Create', 'iii', 3, en3mapa)[0]
check('CFXNewsPanel', lambda p=en3np: (p != 0, en3ri(p) - 0x400000, en3ri(p + 4)))
check('CFXNewsPanel_GetTexture', lambda p=en3np: SDTest.Call('CFXNewsPanel_GetTexture', 'tii', p)[0] == en3gi(sym('g_NewsPanelTarget')))
check('CFXNewsPanel_Update', lambda p=en3np: (SDTest.Call('CFXNewsPanel_Update', 'tiii', p, en3mapa)[0],
	en3ri(p + 4) == en3gi(sym('g_TimeMsInt'))))
check('IFXTextura_GetTexture', lambda p=en3np: SDTest.Call('IFXTextura_GetTexture', 'tii', p)[0])
check('CFXNewsPanel_Delete', lambda p=en3np: SDTest.Call('CFXNewsPanel_Delete', 'tiii', p, 1)[0] == p)
en3nq = SDTest.Call('CFXNewsPanel_ctor', 'tii', en3new(8))[0]
check('CFXNewsPanel_ctor', lambda p=en3nq: (en3ri(p) - 0x400000, en3ri(p + 4)))
check('IFXTextura_dtor', lambda p=en3nq: (SDTest.Call('IFXTextura_dtor', 'tvi', p), en3ri(p) - 0x400000))
check('IFXTextura_Delete', lambda p=en3nq: SDTest.Call('IFXTextura_Delete', 'tiii', p, 1)[0] == p)
en3cosi = SDTest.Peek('g_LavaCosInic', 'b')
check('lava table before', lambda: en3cosi)
def en3tex(p):
	b = en3ri(p + 4)
	o = en3ri(p + 8)
	return (en3ri(p) - 0x400000, en3ints(b, 2), en3ints(o, 2), en3ri(b + 8) != 0, en3ri(o + 8) != 0,
		en3ri(p + 0xc), en3hash(en3ri(b + 8), 128 * 128, 13), en3hash(en3ri(o + 8), 128 * 128, 13))
en3lava = SDTest.Call('FXTextura_Create', 'iii', 1, en3mapa)[0]
check('CFXTexLava_ctor', lambda p=en3lava: (p != 0, en3tex(p), SDTest.Peek('g_LavaCosInic', 'b')))
check('lava cos', lambda: (en3rf(symva('g_LavaCos'), 64), en3rf(symva('g_LavaCos') + 256, 64), en3rf(symva('g_LavaCos') + 512, 64),
	en3rf(symva('g_LavaCos') + 768, 64)))
en3fake = en3calloc(0x60)
for en3t in (4321.75, 4330.0, 4371.75, 4400.5, 100000.0, 7.0):
	en3settime(en3t)
	check('CFXTexLava_Update' + fmt((en3t,)), lambda p=en3lava, m=en3fake: (SDTest.Call('CFXTexLava_Update', 'tiii', p, m)[0],
		en3tex(p), en3ints(symva('g_LavaOffset'), 128)))
en3scroll = SDTest.Call('FXTextura_Create', 'iii', 2, en3mapa)[0]
check('CFXTexScroll_ctor', lambda p=en3scroll: (p != 0, en3tex(p), en3ints(p + 0x10, 2)))
en3wi(en3scroll + 0xc, 0)
for en3t in (100.0, 120.0, 151.0, 400.0, 4000.0):
	en3settime(en3t)
	check('CFXTexScroll_Update' + fmt((en3t,)), lambda p=en3scroll, m=en3fake: (SDTest.Call('CFXTexScroll_Update', 'tiii', p,
		m)[0], en3tex(p), en3ints(p + 0x10, 2)))
check('CFXTexLava_dtor', lambda p=en3lava: (SDTest.Call('CFXTexLava_dtor', 'tvi', p), en3ints(p, 4)))
check('CFXTexLava_Delete', lambda p=en3lava: SDTest.Call('CFXTexLava_Delete', 'tiii', p, 1)[0] == p)
check('CFXTexScroll_Delete', lambda p=en3scroll: SDTest.Call('CFXTexScroll_Delete', 'tiii', p, 1)[0] == p)
en3sc2 = SDTest.Call('CFXTexScroll_ctor', 'tiis', en3new(0x18), '2d/textures/menu/escudo-b.bump.bmp')[0]
check('CFXTexScroll_dtor', lambda p=en3sc2: (SDTest.Call('CFXTexScroll_dtor', 'tvi', p), en3ints(p, 3)))
en3del(en3sc2)
SDTest.Call('Cad_Set', 'tviz', en3mapa, None)
SDTest.Call('Cad_Set', 'tviz', en3mapa + 4, None)
en3free(en3mapa)
en3free(en3fake)
en3gs(sym('g_TimeMsF'), en3timef)

# ---- the animation constructors and destructors ----
def en3cnt():
	return en3gi(sym('g_AnimNodoCount'))
en3c0 = en3cnt()
def en3nodo(n):
	w = en3ints(n, 9)
	r = [w[0] & 0xffff, (w[0] >> 16) & 0xffff, w[1] & 0xffff, (w[1] >> 16) & 0xffff]
	for i in range(2, 9):
		r.append(w[i] != 0)
	return (r, en3cnt() - en3c0)
en3n = en3new(0x24)
check('CAnimNodo_ctorFrames', lambda n=en3n: (SDTest.Call('CAnimNodo_ctorFrames', 'tiiii', n, 5, 2)[0] == n, en3nodo(n)))
check('CAnimNodo_dtor', lambda n=en3n: (SDTest.Call('CAnimNodo_dtor', 'tvi', n), en3nodo(n)))
check('CAnimNodo_ctor', lambda n=en3n: (SDTest.Call('CAnimNodo_ctor', 'tii', n)[0] == n, en3nodo(n)))
check('CAnimNodo_Delete', lambda n=en3n: (SDTest.Call('CAnimNodo_Delete', 'tiii', n, 1)[0] == n, en3cnt() - en3c0))
en3v = en3new(4 + 0x24 * 3)
en3wi(en3v, 3)
for en3i in range(3):
	SDTest.Call('CAnimNodo_ctorFrames', 'tiiii', en3v + 4 + 0x24 * en3i, en3i + 1, en3i)
check('CAnimNodo vector', lambda v=en3v: (en3nodo(v + 4), en3nodo(v + 4 + 0x48)))
check('CAnimNodo_Delete vector', lambda v=en3v: (SDTest.Call('CAnimNodo_Delete', 'tiii', v + 4, 3)[0] == v,
	en3cnt() - en3c0))
en3v = en3new(4 + 0x24 * 2)
en3wi(en3v, 2)
for en3i in range(2):
	SDTest.Call('CAnimNodo_ctor', 'tii', en3v + 4 + 0x24 * en3i)
check('CAnimNodo_Delete vector kept', lambda v=en3v: (SDTest.Call('CAnimNodo_Delete', 'tiii', v + 4, 2)[0] == v,
	en3cnt() - en3c0, en3ri(v)))
en3del(en3v)
def en3anim(a):
	w = en3ints(a, 13)
	return (w[0] != 0, w[1], en3rf(a + 8, 1), w[3], w[4], w[5] & 0xff, w[6], w[7] != 0, w[8] != 0, w[9], w[10] != 0,
		w[11], w[12], en3cnt() - en3c0)
en3a = en3new(0x34)
check('CAnimacion_ctor', lambda a=en3a: (SDTest.Call('CAnimacion_ctor', 'tii', a)[0] == a, en3anim(a)))
SDTest.Call('CAnimacion_Init', 'tviiii', en3a, 3, 10, 1)
SDTest.Call('Cad_Set', 'tviz', en3a, 'anim')
en3wi(en3a + 0x18, 4)
en3wi(en3a + 0x1c, en3new(4))
check('CAnimacion_Init', lambda a=en3a: en3anim(a))
check('CAnimacion_dtor', lambda a=en3a: (SDTest.Call('CAnimacion_dtor', 'tvi', a), en3anim(a)))
en3del(en3a)
en3t = en3new(8)
en3wi(en3t, 4)
en3wi(en3t + 4, en3new(16))
check('Tabla_Free', lambda t=en3t: (SDTest.Call('Tabla_Free@2ab830', 'tvi', t), en3ints(t, 2)))
check('Tabla_Free empty', lambda t=en3t: (SDTest.Call('Tabla_Free@2ab830', 'tvi', t), en3ints(t, 2)))
en3del(en3t)
en3blk = SDTest.Peek(sym('g_MemStats') + 8, 'i')
en3st = en3new(8)
en3wi(en3st, SDTest.Call('Mem_Alloc', 'ii', 10)[0])
check('AnimStream_dtor', lambda s=en3st: (SDTest.Call('AnimStream_dtor', 'tvi', s), SDTest.Peek(sym('g_MemStats') + 8, 'i') - en3blk))
en3wi(en3st, 0)
check('AnimBlock_dtor', lambda s=en3st: SDTest.Call('AnimBlock_dtor', 'tvi', s))
en3del(en3st)

# ---- the camera methods (no node) ----
en3cn = en3gi(sym('g_CamaraNodos'))
en3cam = SDTest.Call('CCamara_ctor', 'tii', en3new(0x78))[0]
def en3camr(c):
	return en3rf(c + 0x14, 3) + en3rf(c + 0x3c, 1) + en3rf(c + 0x4c, 3)
for en3v in (0, 1):
	en3gs(sym('g_CamaraNodos'), en3v)
	check('CCamara_Update' + fmt((en3v,)), lambda c=en3cam: (SDTest.Call('CCamara_Update', 'tvii', c, 0), en3camr(c)))
en3gs(sym('g_CamaraNodos'), en3cn)
check('CCamara_Render', lambda c=en3cam: (SDTest.Call('CCamara_Render', 'tviii', c, 0, 0), en3camr(c)))
check('CCamara_InitD3D', lambda c=en3cam: (SDTest.Call('CCamara_InitD3D', 'tvii', c, 0), en3camr(c)))
check('CCamara_ReleaseD3D', lambda c=en3cam: (SDTest.Call('CCamara_ReleaseD3D', 'tvii', c, 0), en3camr(c)))
check('CCamara_Delete', lambda c=en3cam: SDTest.Call('CCamara_Delete', 'tiii', c, 1)[0] == c)
