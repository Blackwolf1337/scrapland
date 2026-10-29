# engine3d Engine/Trails.c (execfile'd at the end of sdcall.py, Python 1.5.2): trails in heap memory. Nothing is
# drawn and no D3D resource is made: the builders write into heap buffers handed in as the locked ones, the buffer
# counters of the engine and the camera position are set before and put back after. The render lists of the engine
# are used for real (the trails leave them before they are freed); ResetAll and InitLists only run when the lists
# were empty before. Not called: Trails_Init, Trails_Release, Trails_Render (D3D).
def trz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def trfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def trpoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def trfloats(addr, fl):
	SDTest.Call('crt_memcpy', 'iiFi', addr, fl, 4 * len(fl))
# the members, the point array only as set or not
def trget(t):
	w = tpeek(t, 21)
	return (w[0:10], w[10] != 0, w[11:21])
# the first n points (only the ones written: new[] leaves the rest as it was)
def trpts(t, n):
	p = SDTest.Peek(t + 0x28, '@i')
	if not p:
		return None
	return tpeek(p, 5 * n)
def trlist():
	r = [SDTest.Peek('g_nTrails', 'i')]
	for i in range(17):
		r.append(SDTest.Peek(sym('g_nTrailsRender') + 4 * i, 'i'))
	return r
# where a trail is in the render lists: its slot holds it
def trwhere(t):
	tex = SDTest.Peek(t + 8, '@i')
	ind = SDTest.Peek(t + 4, '@i')
	if ind < 0 or ind >= 256 or tex < 0 or tex >= 17:
		return (tex, ind)
	return (tex, ind, SDTest.Peek(sym('g_TrailsRender') + 4 * (256 * tex + ind), 'i') == t)
def trstate(t, n):
	return (trget(t), trpts(t, n), trwhere(t), trlist())
def trstep(name, t, n, rva, sig, *args):
	check(name + fmt(args), lambda t=t, n=n, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, t) + args)[0],
		trstate(t, n)))

trlist0 = trlist()
check('Trails lists before', lambda: trlist0)
treye = SDTest.Call('Render_GetEyePos', 'i')[0]
treyesave = tpeek(treye, 3)
trfloats(treye, (3.5, -20.25, 7.0))
trdim = SDTest.Peek('g_DiffuseMod4x', 'i')
trbuf = (SDTest.Peek('g_TrailsVertex', 'i'), SDTest.Peek('g_TrailsPrims', 'i'), SDTest.Peek('g_TrailsIndex', 'i'))

T = trz(0x54)
T2 = trz(0x54)
check('CTrail_ctor', lambda: (SDTest.Call('CTrail_ctor', 'tii', T)[0] == T, trstate(T, 0)))
check('CTrail_ctor 2', lambda: (SDTest.Call('CTrail_ctor', 'tii', T2)[0] == T2, trstate(T2, 0)))
trstep('CTrail_SetMaxTrails', T, 0, sym('CTrail_SetMaxTrails'), 'tvii', 8)
trstep('CTrail_SetMaxTrails same', T, 0, sym('CTrail_SetMaxTrails'), 'tvii', 8)
trstep('CTrail_SetMaxTrails 2', T2, 0, sym('CTrail_SetMaxTrails'), 'tvii', 8)
trstep('CTrail_SetTransp', T, 0, sym('CTrail_SetTransp'), 'tvii', 40)
trstep('CTrail_SetColor', T, 0, sym('CTrail_SetColor'), 'tvii', 0x2080c0f0)
trstep('CTrail_SetSmooth', T, 0, sym('CTrail_SetSmooth'), 'tvii', 1)
trstep('CTrail_SetUV', T, 0, sym('CTrail_SetUV'), 'tviff', 0.25, -0.5)
trstep('CTrail_SetOrient', T, 0, sym('CTrail_SetOrient'), 'tvifffi', 1.5, 2.5, -3.5, 1)
trstep('CTrail_SetType', T, 0, sym('CTrail_SetType'), 'tvii', 2)
trstep('CTrail_SetSegments', T, 0, sym('CTrail_SetSegments'), 'tvii', 6)
trstep('CTrail_SetPremul', T, 0, sym('CTrail_SetPremul'), 'tvii', 1)
trstep('CTrail_SetWidth', T, 0, sym('CTrail_SetWidth'), 'tvif', 2.75)
trstep('CTrail_SetScale', T, 0, sym('CTrail_SetScale'), 'tviff', 1.5, 0.25)
trstep('CTrail_SetUVScale', T, 0, sym('CTrail_SetUVScale'), 'tviff', 3.0, 0.75)
trstep('CTrail_SetFade', T, 0, sym('CTrail_SetFade'), 'tvii', 1)
trstep('CTrail_SetULen', T, 0, sym('CTrail_SetULen'), 'tviff', 7.0, 9.0)
trstep('CTrail_SetTexture', T, 0, sym('CTrail_SetTexture'), 'tvii', 4)
trstep('CTrail_SetTexture same', T, 0, sym('CTrail_SetTexture'), 'tvii', 4)

# points: the colors carry the inverted alpha in the top byte
trcols = (0x00ffffff, 0x80402010, 0xff123456, 0x40ff8000, 0x7f7f7f7f, 0x01000000, 0xc0a0b0c0)
trn = 0
def tradd(t, i):
	v = vec()
	k = i % 3
	if k == 0:
		SDTest.Call('CTrail_Add', 'tviffffi', t, v[0], v[1], v[2], rnd(0.1, 3.0), trcols[i % len(trcols)])
	elif k == 1:
		SDTest.Call('CTrail_AddPos', 'tvifff', t, v[0], v[1], v[2])
	else:
		SDTest.Call('CTrail_AddPosColor', 'tvifffi', t, v[0], v[1], v[2], trcols[(i + 3) % len(trcols)])
for i in range(10):
	check('CTrail_Add' + fmt((i,)), lambda i=i: (tradd(T, i), trstate(T, min(i + 1, 8))))

# the builders on heap buffers; the counters of the engine set to known values
def trbuild(t, rva, vtx0):
	SDTest.Poke('g_TrailsVertex', vtx0)
	SDTest.Poke('g_TrailsPrims', 7)
	SDTest.Poke('g_TrailsIndex', 11)
	vb = trz(0x1c * 80)
	ib = trz(2 * 160)
	cell = trz(8)
	trpoke(cell, vb)
	trpoke(cell + 4, ib)
	r = SDTest.Call(rva, 'tiiii', t, cell, cell + 4)[0]
	dv = SDTest.Peek(cell, '@i') - vb
	di = SDTest.Peek(cell + 4, '@i') - ib
	verts = idx = None
	if dv >= 0 and dv <= 0x1c * 80:
		verts = plain(tpeek(vb, dv / 4))
	if di >= 0 and di <= 2 * 160:
		idx = plain(tpeek(ib, (di + 3) / 4))
	g = (SDTest.Peek('g_TrailsVertex', 'i'), SDTest.Peek('g_TrailsPrims', 'i'), SDTest.Peek('g_TrailsIndex', 'i'))
	trfree(cell)
	trfree(ib)
	trfree(vb)
	return (r, dv, di, verts, idx, g)
def trset(t, typ, orient, fade, smooth, premul, transp, seg):
	SDTest.Call('CTrail_SetType', 'tvii', t, typ)
	SDTest.Call('CTrail_SetOrient', 'tvifffi', t, rnd(-5.0, 5.0), rnd(-5.0, 5.0), rnd(-5.0, 5.0), orient)
	SDTest.Call('CTrail_SetFade', 'tvii', t, fade)
	SDTest.Call('CTrail_SetSmooth', 'tvii', t, smooth)
	SDTest.Call('CTrail_SetPremul', 'tvii', t, premul)
	SDTest.Call('CTrail_SetTransp', 'tvii', t, transp)
	SDTest.Call('CTrail_SetSegments', 'tvii', t, seg)
	SDTest.Call('CTrail_SetScale', 'tviff', t, rnd(0.1, 3.0), rnd(0.1, 3.0))
	SDTest.Call('CTrail_SetUV', 'tviff', t, rnd(-1.0, 1.0), rnd(-1.0, 1.0))
	SDTest.Call('CTrail_SetUVScale', 'tviff', t, rnd(0.1, 2.0), rnd(0.1, 2.0))
	SDTest.Call('CTrail_SetULen', 'tviff', t, rnd(1.0, 8.0), 1.0)
trsegs = (8, 6, 3, 2, 1)
trtransp = (0, 40, 255, 128)
for i in range(64):
	typ = i % 4
	orient = (i / 4) % 3
	fade = (i / 3) % 2
	smooth = (i / 5) % 2
	premul = (i / 7) % 2
	seg = trsegs[i % len(trsegs)]
	# with one point drawn the ribbon would add its smooth end from registers never set
	if typ == 0 and seg == 1:
		smooth = 0
	SDTest.Poke('g_DiffuseMod4x', (i / 11) % 2)
	trset(T, typ, orient, fade, smooth, premul, trtransp[i % len(trtransp)], seg)
	check('CTrail_Build' + fmt((i, typ, orient, fade, smooth, premul, seg)), lambda: (trbuild(T, sym('CTrail_Build'), 100), trget(T)))
SDTest.Poke('g_DiffuseMod4x', trdim)

# short trails: 2, 3 and 5 points, every builder directly
for npts in (2, 3, 5):
	check('CTrail_Reset' + fmt((npts,)), lambda: (SDTest.Call('CTrail_Reset', 'tvi', T), trstate(T, 1)))
	for i in range(npts):
		tradd(T, i + 20)
	for rva in ('CTrail_BuildRibbon', 'CTrail_BuildFan', 'CTrail_BuildCross', 'CTrail_BuildQuads'):
		for orient in (0, 1, 2):
			trset(T, 0, orient, npts % 2, 1, npts / 3, 40, 8)
			check('CTrail_Build direct' + fmt((npts, rva, orient)), lambda rva=rva: (trbuild(T, rva, 0), trget(T)))
trset(T, 4, 0, 0, 0, 0, 0, 8)
check('CTrail_Build unknown type', lambda: trbuild(T, sym('CTrail_Build'), 5))
SDTest.Call('CTrail_SetType', 'tvii', T, 0)
check('CTrail_Build no points', lambda: (SDTest.Call('CTrail_Reset', 'tvi', T), trbuild(T, sym('CTrail_Build'), 5)))

# the point setters on a full ring
for i in range(9):
	tradd(T, i + 40)
check('CTrail full', lambda: trstate(T, 8))
trstep('CTrail_SetLast', T, 8, sym('CTrail_SetLast'), 'tviFFp', (1.0, 2.0, 3.0), (4.5,), 0x11223344)
trstep('CTrail_SetLast NULL', T, 8, sym('CTrail_SetLast'), 'tviiFi', 0, (6.5,), 0)
trstep('CTrail_SetLast color', T, 8, sym('CTrail_SetLast'), 'tviiip', 0, 0, 0x55667788)
trstep('CTrail_SetLastPos', T, 8, sym('CTrail_SetLastPos'), 'tvifff', -1.0, -2.0, -3.0)
trstep('CTrail_SetLastPosColor', T, 8, sym('CTrail_SetLastPosColor'), 'tvifffi', 7.0, 8.0, 9.0, 0x01020304)
for i in (0, 1, 3, 6, 7, 8, -1):
	trstep('CTrail_SetPoint', T, 8, sym('CTrail_SetPoint'), 'tviiFFp', i, (i * 1.5, 2.0, -i * 0.5), (0.5 + i,), 0x10101010 * (i & 7))
trstep('CTrail_SetPoint NULL', T, 8, sym('CTrail_SetPoint'), 'tviiiii', 2, 0, 0, 0)
trstep('CTrail_Move', T, 8, sym('CTrail_Move'), 'tvifff', 0.5, -1.25, 100.0)

# the render lists: two trails, texture changes, activation and removal
check('Trails lists T', lambda: (trwhere(T), trwhere(T2), trlist()))
trstep('CTrail_SetTexture active', T, 8, sym('CTrail_SetTexture'), 'tvii', 6)
trstep('CTrail_SetTexture 2', T2, 0, sym('CTrail_SetTexture'), 'tvii', 6)
trstep('CTrail_SetActive T2', T2, 0, sym('CTrail_SetActive'), 'tvii', 1)
check('Trails lists both', lambda: (trwhere(T), trwhere(T2), trlist()))
trstep('CTrail_SetActive off', T, 8, sym('CTrail_SetActive'), 'tvii', 0)
check('Trails lists moved', lambda: (trwhere(T), trwhere(T2), trlist()))
trstep('CTrail_SetActive off again', T, 8, sym('CTrail_SetActive'), 'tvii', 0)
check('Trail_SetRender on', lambda: (SDTest.Call('Trail_SetRender', 'vii', T, 1), trstate(T, 8), trwhere(T2)))
check('Trail_SetRender on again', lambda: (SDTest.Call('Trail_SetRender', 'vii', T, 1), trstate(T, 8)))
check('Trails_UpdateRender', lambda: (SDTest.Call('Trails_UpdateRender', 'vi', T), trstate(T, 8)))
check('Trails_UpdateRender NULL', lambda: (SDTest.Call('Trails_UpdateRender', 'vi', 0), trlist()))
trstep('CTrail_SetActive T2 off', T2, 0, sym('CTrail_SetActive'), 'tvii', 0)

# Copy into T2 (same size), then shrink it to nothing
def trcopy():
	return (SDTest.Call('CTrail_Copy', 'tvii', T2, T)[0], trstate(T2, 8))
check('CTrail_Copy', trcopy)
check('Trails lists copy', lambda: (trwhere(T), trwhere(T2), trlist()))
for i in range(8):
	trstep('CTrail_Shrink' + fmt((i,)), T2, 8, sym('CTrail_Shrink'), 'tvi')
check('CTrail_Copy again', trcopy)
trstep('CTrail_Clear', T2, 1, sym('CTrail_Clear'), 'tvi')
trstep('CTrail_SetMaxTrails same active', T, 1, sym('CTrail_SetMaxTrails'), 'tvii', 8)
for i in range(3):
	tradd(T, i + 60)
	tradd(T2, i + 70)
trstep('CTrail_SetTexture T2', T2, 3, sym('CTrail_SetTexture'), 'tvii', 2)
check('Trails lists two', lambda: (trwhere(T), trwhere(T2), trlist()))

if trlist0[0] == 0:
	check('Trails_ResetAll', lambda: (SDTest.Call('Trails_ResetAll', 'i')[0], trstate(T, 3), trstate(T2, 3)))
	check('Trails_InitLists', lambda: (SDTest.Call('Trails_InitLists', 'i')[0], trlist()))
	tradd(T, 80)
	check('Trails after InitLists', lambda: (trwhere(T), trlist()))
else:
	check('Trails_ResetAll skipped', lambda: trlist0[0])

check('CTrail_dtor', lambda: (SDTest.Call('CTrail_dtor', 'tvi', T), trget(T), trlist()))
check('CTrail_dtor 2', lambda: (SDTest.Call('CTrail_dtor', 'tvi', T2), trget(T2), trlist()))
trfree(T)
trfree(T2)
for i in range(3):
	trpoke(treye + 4 * i, treyesave[i])
SDTest.Poke('g_TrailsVertex', trbuf[0])
SDTest.Poke('g_TrailsPrims', trbuf[1])
SDTest.Poke('g_TrailsIndex', trbuf[2])
check('Trails lists after', trlist)
