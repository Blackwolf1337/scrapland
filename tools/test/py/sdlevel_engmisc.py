# engine3d leftovers in the level FZ (execfile'd by sdlevel.py, Python 1.5.2): the stencil volumes drawn from groups,
# segments and triangles written here (the vertex buffers read back through their Lock), CLuz_Render on a light made
# here, CNodo3D_DebugDraw on nodes made here and on a mesh node of a model of the level, and the camera page of the
# statistics. The device calls go through the D3D log; the engine variables and the world matrix are put back.
def emcri(a):
	return SDTest.Peek(a, '@i')
def emcwi(a, v):
	SDTest.Poke(a - 0x400000, v)
def emcwf(a, vals):
	SDTest.Call(0x3197d0, 'iiFi', a, vals, 4 * len(vals))
def emcrf(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def emcgi(rva):
	return SDTest.Peek(rva, 'i')
def emcsf(rva, v):
	emcwf(rva + 0x400000, (v,))
def emcz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def emcfree(p):
	SDTest.Call(0x133550, 'vi', p)
def emcsave(rvas):
	r = []
	for a in rvas:
		r.append(emcgi(a))
	return r
def emcrestore(rvas, vals):
	for i in range(len(rvas)):
		SDTest.Poke(rvas[i], vals[i])
def emclog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
def emcstr(s):
	p = emcz(len(s) + 4)
	i = 0
	while i < len(s):
		v = 0
		for j in range(4):
			if i + j < len(s):
				v = v | (ord(s[i + j]) << (8 * j))
		if v & 0x80000000:
			v = v - 0x100000000
		emcwi(p + i, v)
		i = i + 4
	return p
emcvp = emcsave((0x4ad7e8, 0x4ad7ec))
SDTest.Poke(0x4ad7e8, 640)
SDTest.Poke(0x4ad7ec, 480)
emcworld = emcrf(0x4ae478 + 0x400000, 16)
emcworldset = emcgi(0x432410)
emceye = emcrf(0x4adda8 + 0x400000, 3)
emcident = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
emcm1 = (0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 120.5, -30.0, 44.25, 1.0)

# ---- Stencil_DrawSegs and Stencil_DrawTris (rva 0x265510, 0x2661b0) ----
emcsv = (0x4afdd0, 0x4afdd8, 0x4afde0, 0x390064, 0x390168, 0x3901c8)
emcsvs = emcsave(emcsv)
emcblk = emcgi(0x4afddc)
emcsegp = emcgi(0x4afdcc)
emctrip = emcgi(0x4afdd4)
emcvbs = (emcgi(0x3902b0), emcgi(0x3902b4), emcgi(0x3902b8), emcgi(0x3902bc))
check('emc stencil buffers', lambda: (emcblk != 0, emcsegp != 0, emctrip != 0, emcvbs[0] != 0, emcvbs[1] != 0,
	emcvbs[2] != 0, emcvbs[3] != 0))
# (range, cos, direccional, foco, light position, light direction, segment end, triangle end)
emcgroups = ((100.0, 0.0, 0, 0, (1.0, 2.0, 3.0), (0.0, -1.0, 0.0), 3, 2),
	(200.0, 0.5, 0, 1, (0.0, 50.0, 0.0), (0.0, -1.0, 0.0), 6, 4),
	(50.0, 0.0, 1, 0, (0.0, 1000.0, 0.0), (0.0, -1.0, 0.0), 8, 5),
	(80.0, 0.0, 0, 0, (5.0, 5.0, 5.0), (0.0, -1.0, 0.0), 8, 5),
	(300.0, 0.9, 0, 1, (-10.0, 20.0, 30.0), (0.6, -0.8, 0.0), 10, 7))
emcsegs = (((1.0, 2.0, 3.0), (10.0, 2.0, 3.0)), ((50.0, -40.0, 3.0), (200.0, 2.0, 3.0)),
	((1.5, 2.0, 3.25), (-60.0, -70.0, 10.0)),
	((0.0, 0.0, 0.0), (0.0, 50.0, 0.0)), ((150.0, 40.0, 0.0), (10.0, -100.0, 20.0)), ((0.0, -500.0, 0.0), (3.0, 49.0, 1.0)),
	((7.0, 0.0, -9.0), (0.0, 1000.0, 0.0)), ((1e4, 0.0, 0.0), (-3.5, 2.0, 999.0)),
	((-10.0, 20.0, 30.0), (100.0, -100.0, 30.0)), ((-10.0, 19.0, 30.0), (-200.0, -50.0, 30.0)))
emctris = (((1.0, 2.0, 3.0), (20.0, 2.0, 3.0), (1.0, 300.0, 3.0)), ((5.0, -5.0, 9.0), (-30.0, 10.0, 0.0), (0.0, 0.0, 0.0)),
	((0.0, 0.0, 0.0), (10.0, -60.0, 0.0), (0.0, 50.0, 0.0)), ((400.0, 0.0, 0.0), (0.0, -100.0, 50.0), (2.0, 3.0, 4.0)),
	((7.0, 0.0, -9.0), (0.0, 1000.0, 0.0), (5.0, 5.0, 5.0)),
	((-10.0, 20.0, 30.0), (60.0, -40.0, 30.0), (-10.0, -200.0, 30.0)), ((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (2.0, 2.0, 2.0)))
def emcvbread(vb, n):
	vt = emcri(vb)
	hr = SDTest.Call(emcri(vt + 0x2c) - 0x400000, 'tiiiiipi', 0, vb, 0, 0, 0, 0)
	r = (hr[0],)
	if hr[0] == 0:
		k = 0
		while k < n:
			c = min(n - k, 60)
			r = r + emcrf(hr[1] + k * 4, c)
			k = k + c
		SDTest.Call(emcri(vt + 0x30) - 0x400000, 'tiii', 0, vb)
	return r
if emcblk and emcsegp and emctrip and emcvbs[0] and emcvbs[1] and emcvbs[2] and emcvbs[3]:
	emcbs = []
	for emck in range(len(emcgroups)):
		emcbs.append(emcrf(emcblk + emck * 0x80, 32))
	for emck in range(len(emcgroups)):
		emcg = emcgroups[emck]
		emcb = emcblk + emck * 0x80
		emcwf(emcb + 0x10, (emcg[0], emcg[1]))
		emcwi(emcb + 0x18, emcg[2] | (emcg[3] << 8))
		emcwf(emcb + 0x1c, emcg[4] + emcg[5])
		emcwf(emcb + 0x34, emcm1[0:12] + (emck * 10.0, 1.0, -2.0 * emck, 1.0))
		emcwi(emcb + 0x74, emcg[6])
		emcwi(emcb + 0x7c, emcg[7])
	for emck in range(len(emcsegs)):
		emcwf(emcsegp + emck * 24, emcsegs[emck][0] + emcsegs[emck][1])
	for emck in range(len(emctris)):
		emcwf(emctrip + emck * 36, emctris[emck][0] + emctris[emck][1] + emctris[emck][2])
	SDTest.Poke(0x4afdd0, len(emcsegs))
	SDTest.Poke(0x4afdd8, len(emctris))
	SDTest.Poke(0x4afde0, len(emcgroups))
	emcsf(0x3901c8, 500.0)
	for emcfisq in (0, 1):
		SDTest.Poke(0x390064, emcfisq)
		check('emc Stencil_DrawSegs 0' + fmt((emcfisq,)), lambda: (emclog(lambda: SDTest.Call(0x265510, 'vi', 0)),
			emcvbread(emcvbs[0], len(emcsegs) * 12)))
		check('emc Stencil_DrawTris 0' + fmt((emcfisq,)), lambda: (emclog(lambda: SDTest.Call(0x2661b0, 'vi', 0)),
			emcvbread(emcvbs[2], len(emctris) * 9), emcvbread(emcvbs[3], len(emctris) * 9)))
	check('emc Stencil_DrawSegs 1', lambda: emclog(lambda: SDTest.Call(0x265510, 'vi', 1)))
	check('emc Stencil_DrawTris 1', lambda: emclog(lambda: SDTest.Call(0x2661b0, 'vi', 1)))
	SDTest.Poke(0x390168, 0)
	check('emc Stencil_DrawSegs no draw', lambda: emclog(lambda: SDTest.Call(0x265510, 'vi', 0)))
	check('emc Stencil_DrawTris no draw', lambda: emclog(lambda: SDTest.Call(0x2661b0, 'vi', 0)))
	SDTest.Poke(0x4afde0, 0)
	check('emc Stencil_DrawSegs no groups', lambda: emclog(lambda: SDTest.Call(0x265510, 'vi', 0)))
	for emck in range(len(emcgroups)):
		emcwf(emcblk + emck * 0x80, emcbs[emck])
emcrestore(emcsv, emcsvs)

# ---- CLuz_Render (rva 0x23bb70) on a light in calloc memory with a node name, the world and eye set here ----
emcsl = emcgi(0x4ac288)
emcluz = emcz(0xcc)
emcnodo = emcz(0x100)
emcwi(emcnodo + 4, emcstr('luzA'))
emcwi(emcluz + 0xc4, emcnodo)
SDTest.Call(0x24bac0, 'vFi', emcm1, 1)
emcwf(0x4adda8 + 0x400000, (300.0, -20.0, 80.0))
# (tipo, mostrar, R_ShowLights, rango, phi, direction)
for emcc in ((5000, 0, 0, 50.0, 45.0, (0.0, -1.0, 0.0)), (5000, 1, 0, 0.0, 45.0, (0.0, -1.0, 0.0)),
		(5000, 1, 1, 50.0, 45.0, (0.0, -1.0, 0.0)), (5001, 0, 2, 80.0, 30.0, (0.0, -2.0, 0.0)),
		(5001, 0, 3, 80.0, 60.0, (0.0, 0.0, 0.0)), (5002, 0, 3, 0.0, 45.0, (1.0, 0.0, 0.0)),
		(5000, 1, 3, 120.0, 45.0, (0.0, 1.0, 0.0))):
	emcwi(emcluz + 8, emcc[0])
	emcwi(emcluz + 0xc, emcc[1])
	SDTest.Poke(0x4ac288, emcc[2])
	emcwf(emcluz + 0x34, (emcc[3], 0.0, emcc[4]))
	emcwf(emcluz + 0x50, (0.25, 0.5, 1.0, 1.0))
	emcwf(emcluz + 0x80, (10.0, 20.0, -30.0) + emcc[5])
	emcwi(emcluz + 0xbc, 37)
	emcwi(emcluz + 0xc0, -1)
	check('emc CLuz_Render' + fmt(emcc), lambda: emclog(lambda: SDTest.Call(0x23bb70, 'tviii', emcluz,
		SDTest.Peek(0x4ad6f4, 'i'), 0)))
SDTest.Poke(0x4ac288, emcsl)

# ---- CNodo3D_DebugDraw (rva 0x23d9d0) ----
emcdv = (0x4ad530, 0x4ad610, 0x4ad518, 0x4ad5dc)
emcdvs = emcsave(emcdv)
emcempty = emcstr('')
emcpat = emcstr('*nod*')
emcpad = emcz(0x100)
emcwi(emcpad + 4, emcstr('padre'))
emcwf(emcpad + 0x5c, emcident[0:12] + (5.0, 6.0, 7.0, 1.0))
emcn = emcz(0x100)
emcwi(emcn + 4, emcstr('nodoX'))
emcwi(emcn + 0x14, emcpad)
emcwi(emcn + 0x44, 7)
emcwf(emcn + 0x5c, emcm1)
emcnr = emcz(0xa8)
emclook = emcz(8)
emcwi(emclook, 1)
emcwi(emcnr + 0xc, emclook)
SDTest.Poke(0x4ad5dc, emcempty)
# (R_ShowLocalBox, min, max, filter, render node)
for emcc in ((0, 0, 0, 0, 0), (1, 0, 0, 0, 0), (1, 8, 0, 0, 0), (1, 0, 6, 0, 0), (1, 7, 7, 0, 0), (1, 0, 0, 1, 0),
		(3, 0, 0, 0, 1), (4, 0, 0, 0, 0), (5, 0, 0, 0, 1), (6, 0, 0, 0, 0)):
	SDTest.Poke(0x4ad530, emcc[0])
	SDTest.Poke(0x4ad610, emcc[1])
	SDTest.Poke(0x4ad518, emcc[2])
	if emcc[3]:
		SDTest.Poke(0x4ad5dc, emcpat)
	else:
		SDTest.Poke(0x4ad5dc, emcempty)
	check('emc CNodo3D_DebugDraw' + fmt(emcc), lambda c=emcc: emclog(lambda c=c: SDTest.Call(0x23d9d0, 'tbii', emcn,
		c[4] and emcnr)[0]))
emcwi(emcn + 0x14, 0)
SDTest.Poke(0x4ad530, 6)
check('emc CNodo3D_DebugDraw root', lambda: emclog(lambda: SDTest.Call(0x23d9d0, 'tbii', emcn, 0)[0]))
SDTest.Poke(0x4ad5dc, emcempty)
SDTest.Poke(0x4ad610, 0)
SDTest.Poke(0x4ad518, 0)
# a mesh node of the first models of the level with its render node (boxes, modes 2 to 4)
emcmesh = []
emck = 0
while emck < SDTest.Peek(0x4ae7c4, 'i') and emck < 40 and len(emcmesh) < 2:
	emcmo = emcgi(0x4ae7c8 + emck * 4)
	emck = emck + 1
	if not emcmo or not emcri(emcmo + 0xac):
		continue
	emct = emcri(emcmo + 0xac)
	emcj = 0
	while emcj < emcri(emct) and len(emcmesh) < 2:
		emcrn = emcri(emct + 4) + emcj * 0xa8
		emcj = emcj + 1
		emcnd = emcri(emcrn + 0xa4)
		if emcnd and emcri(emcnd + 0x3c) and emcri(emcri(emcnd + 0x3c) + 4) == -1593835518:
			emcmesh.append((emcnd, emcrn))
check('emc mesh nodes', lambda: len(emcmesh))
for emcm in emcmesh:
	for emcmode in (2, 3, 4):
		SDTest.Poke(0x4ad530, emcmode)
		check('emc CNodo3D_DebugDraw mesh' + fmt((emcmode,)), lambda m=emcm: emclog(lambda m=m: SDTest.Call(0x23d9d0,
			'tbii', m[0], m[1])[0]))
emcrestore(emcdv, emcdvs)

# ---- Motor_SceneDlgProc (rva 0x22dcf0, stdcall) without a dialog: the scene of a mesh node, a fake selection ----
def emcdlg(msg, wp, lp):
	return SDTest.Call(0x22dcf0, 'tiiiiii', 0, 0, msg, wp, lp)[0]
check('emc Motor_SceneDlgProc other', lambda: (emcdlg(0x999, 0, 0), emcdlg(0x111, 3, 0), emcdlg(0x111, 0, 0),
	emcdlg(0x10, 0, 0)))
check('emc Motor_SceneDlgProc init NULL', lambda: emcdlg(0x110, 0, 0))
if len(emcmesh):
	emcesc = emcri(emcmesh[0][0] + 0x40)
	check('emc Motor_SceneDlgProc init', lambda: (emcdlg(0x110, 0, emcesc), SDTest.Peek(0x4a9050, 's')))
	emcnm = emcz(0x60)
	emcwi(emcnm + 8, -402)
	emcwi(emcnm + 0x5c, emcmesh[0][0])
	check('emc Motor_SceneDlgProc notify', lambda: emcdlg(0x4e, 0, emcnm))
	emcwi(emcnm + 8, -401)
	check('emc Motor_SceneDlgProc notify other', lambda: emcdlg(0x4e, 0, emcnm))
	emcfree(emcnm)

# ---- Motor_DrawStats (rva 0x2308c0) page 2: the camera of the world and the light counts ----
emcpm = emcgi(0x4a9018)
if emcgi(0x4afde8):
	SDTest.Poke(0x4a9018, 2)
	check('emc Motor_DrawStats 2', lambda: SDTest.Call(0x2308c0, 'v'))
SDTest.Poke(0x4a9018, emcpm)

SDTest.Call(0x24bac0, 'vFi', emcworld, 1)
SDTest.Poke(0x432410, emcworldset)
emcwf(0x4adda8 + 0x400000, emceye)
emcrestore((0x4ad7e8, 0x4ad7ec), emcvp)
