# engine3d leftovers (execfile'd at the end of sdcall.py, Python 1.5.2): Matrix3_RotFromTo on fixed vectors, the
# window procedure of the debug dialog with messages that need no dialog (no WM_INITDIALOG: it moves the focus),
# CMapa_CopyBmp into textures made here with the pixels read back, the frame time graph through the D3D
# log, the statistics pages (their state only, the texts hold memory and timer values), and ViewerCamera_Move with
# the debug actions set here. Every engine variable that is changed is put back; pointers only as == or != 0.
def emcnew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def emcdel(p):
	SDTest.Call('operator_delete', 'vi', p)
def emcri(a):
	return SDTest.Peek(a, '@i')
def emcrb(a):
	return SDTest.Peek(a, '@b')
def emcwi(a, v):
	SDTest.Poke(a - 0x400000, v)
def emcwf(a, vals):
	SDTest.Call('crt_memcpy', 'iiFi', a, vals, 4 * len(vals))
def emcrf(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def emcgi(rva):
	return SDTest.Peek(rva, 'i')
def emcgf(rva):
	return emcrf(rva + 0x400000, 1)[0]
def emcsf(rva, v):
	emcwf(rva + 0x400000, (v,))
def emcsave(rvas):
	r = []
	for a in rvas:
		r.append(emcgi(a))
	return r
def emcrestore(rvas, vals):
	for i in range(len(rvas)):
		SDTest.Poke(rvas[i], vals[i])
def emcints(a, n):
	r = ()
	for i in range(n):
		r = r + (emcri(a + 4 * i),)
	return r
def emclog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())

# ---- Matrix3_RotFromTo (rva 0x238ce0): equal, general, opposite and nearly opposite vectors ----
emcvecs = (((0.0, 0.0, 1.0), (0.0, 0.0, 1.0)), ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0)), ((0.6, 0.8, 0.0), (0.0, 0.6, 0.8)),
	((0.0, 0.0, 1.0), (0.0, 0.0, -1.0)), ((1.0, 0.0, 0.0), (-1.0, 0.0, 0.0)), ((0.0, 1.0, 0.0), (0.0, -1.0, 0.0)),
	((0.6, 0.8, 0.0), (-0.6, -0.8, 1e-07)), ((0.0, 0.0009, 0.9999996), (0.0, 0.0, 1.0)),
	((2.0, 0.0, 0.0), (0.0, 3.0, 0.0)), ((0.48, -0.6, 0.64), (-0.8, 0.0, 0.6)), ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0)),
	((0.0, 0.0005, 0.0), (0.0, -1.0, 0.0)))
for emcv in emcvecs:
	check('emc Matrix3_RotFromTo' + fmt(emcv), lambda v=emcv: SDTest.Call('Matrix3_RotFromTo', 'iFFF', v[0], v[1], (9.0,) * 9)[3])

# ---- DbgWin_DlgProc (rva 0x2373d0, stdcall: called as thiscall with a dummy this) ----
emcdbg = [sym('g_DbgWnd'), sym('g_DbgList'), sym('g_DbgEdit'), sym('g_DbgButton')] + range(sym('g_DbgClient'),
	sym('g_DbgClient') + 16, 4) + range(sym('g_DbgListRect'), sym('g_DbgListRect') + 16, 4) + range(sym('g_DbgEditRect'),
	sym('g_DbgEditRect') + 16, 4) + range(sym('g_DbgButtonRect'), sym('g_DbgButtonRect') + 16, 4)
emcdbgs = emcsave(emcdbg) + [emcgi(sym('g_E3DCmdProc'))]
def emcproc(msg, wp, lp):
	return SDTest.Call('DbgWin_DlgProc', 'tiiiiii', 0, 0, msg, wp, lp)[0]
check('emc DbgWin state', lambda: (emcdbgs[0] != 0, emcdbgs[1] != 0, emcdbgs[2] != 0, emcdbgs[3] != 0))
check('emc DbgWin_DlgProc other', lambda: emcproc(0x999, 1, 2))
check('emc DbgWin_DlgProc command 0', lambda: emcproc(0x111, 0, 0))
check('emc DbgWin_DlgProc command 3', lambda: emcproc(0x111, 3, 0))
check('emc DbgWin_DlgProc ctlcolor other', lambda: (emcproc(0x133, 0, 12345), emcproc(0x134, 0, 12345)))
check('emc DbgWin_DlgProc ctlcolor list', lambda: (emcproc(0x133, 0, emcdbgs[1]) != 0, emcproc(0x134, 0, emcdbgs[2]) != 0))
SDTest.Poke('g_E3DCmdProc', 0)
check('emc DbgWin_DlgProc enter', lambda: emcproc(0x111, 0x5010406, 0))
SDTest.Poke('g_E3DCmdProc', emcdbgs[-1])
if emcdbgs[0] == 0 and emcdbgs[1] == 0 and emcdbgs[2] == 0 and emcdbgs[3] == 0:
	check('emc DbgWin_DlgProc size', lambda: (emcproc(5, 0, (100 << 16) | 200), emcproc(5, 0, 0)))
	check('emc DbgWin_DlgProc close', lambda: (emcproc(0x10, 0, 0), emcgi(sym('g_DbgWnd'))))
emcrestore(emcdbg, emcdbgs)

# ---- CMapa_CopyBmp (rva 0x2370a0) into textures made here through the device, read back through LockRect ----
emcvp = emcsave((sym('g_ViewportW'), sym('g_ViewportH')))
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
def emccom(obj, off):
	return emcri(emcri(obj) + off) - 0x400000
emcdev = emcgi(sym('g_pd3dDevice'))
emcbmp = emcnew(0xc)
SDTest.Call('CBmp_ctor', 'tiiii', emcbmp, 5, 3)
for emcy in range(3):
	for emcx in range(5):
		emcwi(emcri(emcbmp + 8) + (emcy * 5 + emcx) * 4, (emcx * 0x3b + emcy * 0x51) | ((emcx * 0x29 + 7) << 8) |
			((emcy * 0x47 + emcx * 0x13) << 16) | (0x5a << 24))
emclr = emcnew(8)
emcmapa = SDTest.Call('Mem_Calloc', 'ii', 0x60)[0]
def emctexread(tex, bpp, x, y, n):
	hr = SDTest.Call(emccom(tex, 0x40), 'tiiiiiii', 0, tex, 0, emclr, 0, 0)[0]
	r = (hr,)
	if hr == 0:
		pitch = emcri(emclr)
		bits = emcri(emclr + 4)
		for j in range(n):
			v = emcri(bits + y * pitch + (x + j) * bpp)
			if bpp == 2:
				v = v & 0xffff
			r = r + (v,)
		SDTest.Call(emccom(tex, 0x44), 'tiiii', 0, tex, 0)
	return r
def emctexclear(tex, h):
	hr = SDTest.Call(emccom(tex, 0x40), 'tiiiiiii', 0, tex, 0, emclr, 0, 0)[0]
	if hr == 0:
		pitch = emcri(emclr)
		bits = emcri(emclr + 4)
		for j in range(h * pitch / 4):
			emcwi(bits + j * 4, 0x11223344)
		SDTest.Call(emccom(tex, 0x44), 'tiiii', 0, tex, 0)
# (format, pool, bytes a pixel): A8R8G8B8, X8R8G8B8, R5G6B5 managed; A1R5G5B5 and system memory are refused
for emct in ((21, 1, 4), (22, 1, 4), (23, 1, 2), (25, 1, 2), (21, 2, 4)):
	emctex = 0
	if emcdev:
		emcr = SDTest.Call(emccom(emcdev, 0x50), 'tiiiiiiiiip', 0, emcdev, 32, 16, 1, 0, emct[0], emct[1], 0)
		emctex = emcr[1]
		if emcr[0] < 0:
			emctex = 0
	check('emc CMapa_CopyBmp texture' + fmt(emct), lambda: emctex != 0)
	if emctex:
		emctexclear(emctex, 16)
		emcwi(emcmapa + 0x34, emctex)
		for emcc in ((0, 0, 0, 0, 5, 3), (10, 4, 1, 1, 3, 2), (28, 14, 0, 0, 10, 10), (3, 1, 4, 2, 9, 9), (-1, 0, 0, 0, 2, 2),
				(0, 0, -1, 0, 2, 2), (0, 0, 0, 0, 0, 2), (0, 0, 5, 0, 2, 2), (40, 0, 0, 0, 2, 2), (0, 16, 0, 0, 2, 2)):
			check('emc CMapa_CopyBmp' + fmt((emct, emcc)), lambda c=emcc, t=emctex, b=emct[2]: (SDTest.Call('CMapa_CopyBmp',
				'tbiiiiiiii', emcmapa, c[0], c[1], emcbmp, c[2], c[3], c[4], c[5])[0],
				emctexread(t, b, min(max(c[0], 0), 28), min(max(c[1], 0), 14), 4),
				emctexread(t, b, min(max(c[0], 0), 28), min(max(c[1], 0), 14) + 1, 3)))
		check('emc CMapa_CopyBmp no image', lambda: SDTest.Call('CMapa_CopyBmp', 'tbiiiiiiii', emcmapa, 0, 0, 0, 0, 0, 2, 2)[0])
		SDTest.Call(emccom(emctex, 8), 'tiii', 0, emctex)
emcwi(emcmapa + 0x34, 0)
check('emc CMapa_CopyBmp no texture', lambda: SDTest.Call('CMapa_CopyBmp', 'tbiiiiiiii', emcmapa, 0, 0, emcbmp, 0, 0, 2, 2)[0])
SDTest.Call('Mem_Free', 'vi', emcmapa)
SDTest.Call('CBmp_Delete', 'tiii', emcbmp, 1)
emcdel(emclr)

# ---- Motor_DrawFpsGraph (rva 0x22e9b0) through the D3D log, the clock set here ----
emcclock = (sym('g_FrameMs'), sym('g_Fps'), sym('g_TimeMsF'))
emcclocks = emcsave(emcclock)
def emcgraph():
	pos = emcgi(sym('g_FpsGraphPos'))
	prev = (pos + 29) % 30
	return (SDTest.Peek('g_FpsText', 's'), emcgf(sym('g_FpsTextTime')), pos, emcrf(sym('g_FpsGraph') + 0x400000 + prev * 16, 4),
		emcrf(sym('g_FpsGraphPeak') + 0x400000 + prev * 16, 4), emcgf(sym('g_FpsScaleTime')), emcgf(sym('g_FpsScale')))
emcgk = 0
for emcg in ((0.02, 50.0, 3000.0), (0.0333, 30.0, 3100.0), (0.016, 60.0, 5200.0), (0.1, 61.0, 5300.0),
		(0.05, 20.0, 8000.0), (0.0, 0.0, 8001.0)):
	emcsf(sym('g_FrameMs'), emcg[0])
	emcsf(sym('g_Fps'), emcg[1])
	emcsf(sym('g_TimeMsF'), emcg[2])
	if emcgk < 3:
		check('emc Motor_DrawFpsGraph' + fmt(emcg), lambda: (emclog(lambda: SDTest.Call('Motor_DrawFpsGraph', 'vf', 160.0)),
			emcgraph()))
	else:
		check('emc Motor_DrawFpsGraph' + fmt(emcg), lambda: (SDTest.Call('Motor_DrawFpsGraph', 'vf', 100.0), emcgraph()))
	emcgk = emcgk + 1
for emcgk in range(28):
	emcsf(sym('g_FrameMs'), 0.001 * emcgk)
	SDTest.Call('Motor_DrawFpsGraph', 'vf', 160.0)
check('emc Motor_DrawFpsGraph ring', lambda: (emcgraph(), emcrf(sym('g_FpsGraph') + 0x400000, 8), emcrf(sym('g_FpsGraphPeak') + 0x400000, 8)))

# ---- Motor_DrawStats (rva 0x2308c0): the counters it clears, the clock set here ----
emcst = (sym('g_ProfMode'), sym('g_ProfResetTime'), sym('g_ProfNoReset'), sym('g_BspCacheStats'), sym('g_CollisionCacheMiss'), sym('g_CollisionTestsMax'), sym('g_CollisionTestsTotal'), sym('g_TexCached'), sym('g_TexUsed'), sym('g_TexUsedPixels'))
emcsts = emcsave(emcst)
def emcstats():
	return (emcgf(sym('g_ProfResetTime')), emcints(sym('g_BspCacheStats') + 0x400000, 3) + emcints(sym('g_CollisionTests') + 0x400000, 1) +
		emcints(sym('g_CollisionTestsTotal') + 0x400000, 1), emcints(sym('g_TexCached') + 0x400000, 1) + emcints(sym('g_TexUsed') + 0x400000, 1) +
		emcints(sym('g_TexUsedPixels') + 0x400000, 1))
for emcm in ((1, 0, 1), (3, 0, 1), (4, 0, 1), (4, 0, 0), (5, 0, 1), (5, 1, 1), (5, 1, 0), (6, 0, 1), (0, 0, 1)):
	SDTest.Poke('g_ProfMode', emcm[0])
	SDTest.Poke('g_ProfNoReset', emcm[1])
	emcsf(sym('g_TimeMsF'), 20000.0 + 900.0 * emcm[2])
	emcsf(sym('g_ProfResetTime'), 19000.0)
	emcsf(sym('g_FrameMs'), 0.025)
	emcsf(sym('g_Fps'), 40.0)
	SDTest.Poke('g_BspCacheStats', 11)
	SDTest.Poke('g_CollisionCacheMiss', 12)
	SDTest.Poke('g_CollisionTestsMax', 13)
	SDTest.Poke('g_CollisionTestsTotal', 14)
	SDTest.Poke('g_TexCached', 3 * emcm[2])
	SDTest.Poke('g_TexUsed', 2)
	SDTest.Poke('g_TexUsedPixels', 4096)
	check('emc Motor_DrawStats' + fmt(emcm), lambda: (SDTest.Call('Motor_DrawStats', 'v'), emcstats()))
emcrestore(emcst, emcsts)
emcrestore(emcclock, emcclocks)
emcrestore((sym('g_ViewportW'), sym('g_ViewportH')), emcvp)

# ---- ViewerCamera_Move (rva 0x218080) on the viewer camera, the debug actions and its variables set here ----
emccam = emcgi(sym('g_ViewerCamera'))
emccc = 0
if emccam:
	emccc = emcri(emccam + 0x34)
check('emc viewer camera', lambda: (emccam != 0, emccc != 0))
emcset = 0
emcacts = 0
if emcgi(sym('g_ActionSets')) > 1:
	emcset = emcgi(sym('g_ActionSets') + 0x4) + 0xc
	if emcri(emcset + 4) > 23:
		emcacts = emcri(emcset + 8)
if emccam and emccc and emcacts:
	emcvv = (sym('g_ViewerClipNear'), sym('g_ViewerClipFar'), sym('g_ViewerFOV'), sym('g_ViewerCameraTgt'), sym('g_ViewerCameraVel'), sym('g_ViewerCameraInv'), sym('g_ViewerSpeed'), sym('g_FrameMs'))
	emcvvs = emcsave(emcvv)
	emccams = emcints(emccam, 13)
	emcccs = emcints(emccc, 28)
	emcaidx = (2, 3, 4, 5, 9, 14, 15, 16, 18, 23)
	emcas = []
	for emci in emcaidx:
		emcas.append(emcints(emcacts + emci * 0x7c + 8, 1) + emcints(emcacts + emci * 0x7c + 0x18, 1))
	def emcact(i, prev, val):
		emcwf(emcacts + i * 0x7c + 8, (prev,))
		emcwf(emcacts + i * 0x7c + 0x18, (val,))
	def emccamstate():
		return (emcrf(emccam, 12), emcgi(sym('g_ViewerCameraTgt')), emcgf(sym('g_ViewerSpeed')), emcri(emccc + 0x48), emcrf(emccc + 8, 9))
	def emccamset(pos, rot, tgt):
		emcwf(emccam, pos + rot + tgt)
		emcwf(emccc + 8, rot + pos + tgt)
	for emci in emcaidx:
		emcact(emci, 0.0, 0.0)
	emcsf(sym('g_ViewerClipNear'), 2.5)
	emcsf(sym('g_ViewerClipFar'), 9000.0)
	emcsf(sym('g_ViewerFOV'), 0.0)
	emcsf(sym('g_ViewerCameraVel'), 1.0)
	emcsf(sym('g_ViewerSpeed'), 1.0)
	emcsf(sym('g_FrameMs'), 0.05)
	# (tgt, inv, fov, fwd, side, move, pan, pos, rot, target)
	for emcc in ((0, 1, 0.0, 0.5, -0.25, 0.0, 0, (10.0, 20.0, 30.0), (0.1, 0.2, 0.0), (0.0, 0.0, 0.0)),
			(0, 0, 60.0, 1.0, 1.0, 0.0, 0, (10.0, 20.0, 30.0), (0.1, 0.2, 0.0), (0.0, 0.0, 0.0)),
			(0, 1, 0.0, 0.75, -0.5, 0.6, 1, (10.0, 20.0, 30.0), (0.1, 0.2, 0.0), (0.0, 0.0, 0.0)),
			(0, 1, 0.0, 0.0, 0.0, -0.3, 0, (1.0, -2.0, 3.0), (0.0, 1.5, 0.0), (0.0, 0.0, 0.0)),
			(1, 1, 0.0, 0.8, 0.2, 0.0, 1, (0.0, 0.0, -50.0), (0.0, 0.0, 0.0), (0.0, 5.0, 0.0)),
			(1, 1, 0.0, 0.4, 0.3, 0.0, 0, (0.0, 0.0, -50.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
			(1, 1, 0.0, 0.0, 0.5, 0.0, 0, (0.0, 3.0, -10.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
			(1, 1, 0.0, 0.0, 0.0, 0.9, 0, (0.0, 0.0, -10.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
			(1, 1, 45.0, 0.2, 0.0, 0.0, 0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))):
		SDTest.Poke('g_ViewerCameraTgt', emcc[0])
		SDTest.Poke('g_ViewerCameraInv', emcc[1])
		emcsf(sym('g_ViewerFOV'), emcc[2])
		emcact(2, 0.0, emcc[3])
		emcact(5, 0.0, emcc[4])
		emcact(16, 0.0, emcc[5])
		emcact(18, 0.0, emcc[6] * 1.0)
		emccamset(emcc[7], emcc[8], emcc[9])
		check('emc ViewerCamera_Move' + fmt(emcc[0:7]), lambda: (SDTest.Call('ViewerCamera_Move', 'vi', emccam), emccamstate()))
	for emci in (2, 5, 16, 18):
		emcact(emci, 0.0, 0.0)
	emccamset((5.0, 6.0, 7.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
	emcact(23, 0.0, 1.0)
	check('emc ViewerCamera_Move toggle', lambda: (SDTest.Call('ViewerCamera_Move', 'vi', emccam), emccamstate()))
	emcact(23, 1.0, 1.0)
	check('emc ViewerCamera_Move held', lambda: (SDTest.Call('ViewerCamera_Move', 'vi', emccam), emccamstate()))
	emcact(23, 0.0, 0.0)
	emcact(14, 0.0, 1.0)
	check('emc ViewerCamera_Move faster', lambda: (SDTest.Call('ViewerCamera_Move', 'vi', emccam), emccamstate()))
	emcact(14, 0.0, 0.0)
	emcact(15, 0.25, 0.75)
	check('emc ViewerCamera_Move slower', lambda: (SDTest.Call('ViewerCamera_Move', 'vi', emccam), emccamstate()))
	for emci in range(len(emcaidx)):
		emcwi(emcacts + emcaidx[emci] * 0x7c + 8, emcas[emci][0])
		emcwi(emcacts + emcaidx[emci] * 0x7c + 0x18, emcas[emci][1])
	for emci in range(13):
		emcwi(emccam + emci * 4, emccams[emci])
	for emci in range(28):
		emcwi(emccc + emci * 4, emcccs[emci])
	emcrestore(emcvv, emcvvs)
