# engine3d leftovers (execfile'd at the end of sdcall.py, Python 1.5.2): Matrix3_RotFromTo on fixed vectors, the
# window procedure of the debug dialog with messages that need no dialog (no WM_INITDIALOG: it moves the focus),
# CMapa_CopyBmp into the texture of a background with the pixels read back, the frame time graph through the D3D
# log, the statistics pages (their state only, the texts hold memory and timer values), and ViewerCamera_Move with
# the debug actions set here. Every engine variable that is changed is put back; pointers only as == or != 0.
def emcnew(n):
	return SDTest.Call(0x1334d0, 'ii', n)[0]
def emcdel(p):
	SDTest.Call(0x1334e0, 'vi', p)
def emcri(a):
	return SDTest.Peek(a, '@i')
def emcrb(a):
	return SDTest.Peek(a, '@b')
def emcwi(a, v):
	SDTest.Poke(a - 0x400000, v)
def emcwf(a, vals):
	SDTest.Call(0x3197d0, 'iiFi', a, vals, 4 * len(vals))
def emcrf(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
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
	check('emc Matrix3_RotFromTo' + fmt(emcv), lambda v=emcv: SDTest.Call(0x238ce0, 'iFFF', v[0], v[1], (9.0,) * 9)[3])

# ---- DbgWin_DlgProc (rva 0x2373d0, stdcall: called as thiscall with a dummy this) ----
emcdbg = range(0x4ab920, 0x4ab970, 4)
emcdbgs = emcsave(emcdbg) + [emcgi(0x4ab8f8)]
def emcproc(msg, wp, lp):
	return SDTest.Call(0x2373d0, 'tiiiiii', 0, 0, msg, wp, lp)[0]
check('emc DbgWin state', lambda: (emcdbgs[0] != 0, emcdbgs[1] != 0, emcdbgs[2] != 0, emcdbgs[3] != 0))
check('emc DbgWin_DlgProc other', lambda: emcproc(0x999, 1, 2))
check('emc DbgWin_DlgProc command 0', lambda: emcproc(0x111, 0, 0))
check('emc DbgWin_DlgProc command 3', lambda: emcproc(0x111, 3, 0))
check('emc DbgWin_DlgProc ctlcolor other', lambda: (emcproc(0x133, 0, 12345), emcproc(0x134, 0, 12345)))
check('emc DbgWin_DlgProc ctlcolor list', lambda: (emcproc(0x133, 0, emcdbgs[1]) != 0, emcproc(0x134, 0, emcdbgs[2]) != 0))
SDTest.Poke(0x4ab8f8, 0)
check('emc DbgWin_DlgProc enter', lambda: emcproc(0x111, 0x5010406, 0))
SDTest.Poke(0x4ab8f8, emcdbgs[-1])
if emcdbgs[0] == 0 and emcdbgs[1] == 0 and emcdbgs[2] == 0 and emcdbgs[3] == 0:
	check('emc DbgWin_DlgProc size', lambda: (emcproc(5, 0, (100 << 16) | 200), emcproc(5, 0, 0)))
	check('emc DbgWin_DlgProc close', lambda: (emcproc(0x10, 0, 0), emcgi(0x4ab920)))
emcrestore(emcdbg, emcdbgs)

# ---- CMapa_CopyBmp (rva 0x2370a0) into the texture of a background, read back through LockRect ----
emcvp = emcsave((0x4ad7e8, 0x4ad7ec))
SDTest.Poke(0x4ad7e8, 640)
SDTest.Poke(0x4ad7ec, 480)
emcf = emcnew(0x18)
SDTest.Call(0x2a5110, 'tii', emcf)
emcmat = emcri(emcf + 0x14)
emcmapa = 0
if emcmat:
	emcmapa = emcri(emcmat + 0x9c)
emctex = 0
if emcmapa:
	emctex = emcri(emcmapa + 0x34)
	if not emctex:
		SDTest.Call(0x235dc0, 'tvii', emcmat, emcgi(0x4ad6f4))
		emctex = emcri(emcmapa + 0x34)
check('emc CMapa_CopyBmp texture', lambda: (emcmat != 0, emcmapa != 0, emctex != 0))
emcbmp = emcnew(0xc)
SDTest.Call(0x27cfe0, 'tiiii', emcbmp, 5, 3)
for emcy in range(3):
	for emcx in range(5):
		emcwi(emcri(emcbmp + 8) + (emcy * 5 + emcx) * 4, (emcx * 0x3b + emcy * 0x51) | ((emcx * 0x29 + 7) << 8) |
			((emcy * 0x47 + emcx * 0x13) << 16) | (0x5a << 24))
emclr = emcnew(0x20)
emcbpp = 4
if emctex:
	if SDTest.Call(emcri(emcri(emctex) + 0x38) - 0x400000, 'tiiiii', 0, emctex, 0, emclr)[0] == 0:
		check('emc CMapa_CopyBmp desc', lambda: emcints(emclr, 8))
		if emcri(emclr) == 0x17:
			emcbpp = 2
def emctexread(x, y, n):
	vt = emcri(emctex)
	hr = SDTest.Call(emcri(vt + 0x40) - 0x400000, 'tiiiiiii', 0, emctex, 0, emclr, 0, 0)[0]
	r = (hr,)
	if hr == 0:
		pitch = emcri(emclr)
		bits = emcri(emclr + 4)
		for j in range(n):
			v = emcri(bits + y * pitch + (x + j) * emcbpp)
			if emcbpp == 2:
				v = v & 0xffff
			r = r + (v,)
		SDTest.Call(emcri(vt + 0x44) - 0x400000, 'tiiii', 0, emctex, 0)
	return r
def emcclamp(v):
	return max(0, min(v, 250))
if emctex:
	for emcc in ((0, 0, 0, 0, 5, 3), (10, 4, 1, 1, 3, 2), (250, 254, 0, 0, 10, 10), (3, 1, 4, 2, 9, 9),
			(-1, 0, 0, 0, 2, 2), (0, 0, -1, 0, 2, 2), (0, 0, 0, 0, 0, 2), (0, 0, 5, 0, 2, 2), (300, 0, 0, 0, 2, 2)):
		check('emc CMapa_CopyBmp' + fmt(emcc), lambda c=emcc: (SDTest.Call(0x2370a0, 'tbiiiiiiii', emcmapa, c[0], c[1],
			emcbmp, c[2], c[3], c[4], c[5])[0], emctexread(emcclamp(c[0]), emcclamp(c[1]), 3),
			emctexread(emcclamp(c[0]), emcclamp(c[1]) + 1, 2)))
	check('emc CMapa_CopyBmp no image', lambda: SDTest.Call(0x2370a0, 'tbiiiiiiii', emcmapa, 0, 0, 0, 0, 0, 2, 2)[0])
SDTest.Call(0x27d2f0, 'tiii', emcbmp, 1)
emcdel(emclr)
emcz = SDTest.Call(0x133650, 'ii', 0x60)[0]
check('emc CMapa_CopyBmp no texture', lambda: SDTest.Call(0x2370a0, 'tbiiiiiiii', emcz, 0, 0, emcbmp, 0, 0, 2, 2)[0])
SDTest.Call(0x133550, 'vi', emcz)
SDTest.Call(0x2a5160, 'tvi', emcf)
emcdel(emcf)

# ---- Motor_DrawFpsGraph (rva 0x22e9b0) through the D3D log, the clock set here ----
emcclock = (0x4a8ed4, 0x431ba8, 0x4a8ed0)
emcclocks = emcsave(emcclock)
def emcgraph():
	pos = emcgi(0x4a9498)
	prev = (pos + 29) % 30
	return (SDTest.Peek(0x4a9090, 's'), emcgf(0x4a90d0), pos, emcrf(0x4a90d8 + 0x400000 + prev * 16, 4),
		emcrf(0x4a92b8 + 0x400000 + prev * 16, 4), emcgf(0x4a949c), emcgf(0x431d00))
emcgk = 0
for emcg in ((0.02, 50.0, 3000.0), (0.0333, 30.0, 3100.0), (0.016, 60.0, 5200.0), (0.1, 61.0, 5300.0),
		(0.05, 20.0, 8000.0), (0.0, 0.0, 8001.0)):
	emcsf(0x4a8ed4, emcg[0])
	emcsf(0x431ba8, emcg[1])
	emcsf(0x4a8ed0, emcg[2])
	if emcgk < 3:
		check('emc Motor_DrawFpsGraph' + fmt(emcg), lambda: (emclog(lambda: SDTest.Call(0x22e9b0, 'vf', 160.0)),
			emcgraph()))
	else:
		check('emc Motor_DrawFpsGraph' + fmt(emcg), lambda: (SDTest.Call(0x22e9b0, 'vf', 100.0), emcgraph()))
	emcgk = emcgk + 1
for emcgk in range(28):
	emcsf(0x4a8ed4, 0.001 * emcgk)
	SDTest.Call(0x22e9b0, 'vf', 160.0)
check('emc Motor_DrawFpsGraph ring', lambda: (emcgraph(), emcrf(0x4a90d8 + 0x400000, 8), emcrf(0x4a92b8 + 0x400000, 8)))

# ---- Motor_DrawStats (rva 0x2308c0): the counters it clears, the clock set here ----
emcst = (0x4a9018, 0x4a94a0, 0x4a8fb8, 0x4af7e0, 0x4af7e4, 0x4af7e8, 0x4af7f0, 0x4b25f4, 0x4b25f8, 0x4b25fc)
emcsts = emcsave(emcst)
def emcstats():
	return (emcgf(0x4a94a0), emcints(0x4af7e0 + 0x400000, 5), emcints(0x4b25f0 + 0x400000, 4)[1:])
for emcm in ((1, 0, 1), (3, 0, 1), (4, 0, 1), (4, 0, 0), (5, 0, 1), (5, 1, 1), (5, 1, 0), (6, 0, 1), (0, 0, 1)):
	SDTest.Poke(0x4a9018, emcm[0])
	SDTest.Poke(0x4a8fb8, emcm[1])
	emcsf(0x4a8ed0, 20000.0 + 900.0 * emcm[2])
	emcsf(0x4a94a0, 19000.0)
	emcsf(0x4a8ed4, 0.025)
	emcsf(0x431ba8, 40.0)
	SDTest.Poke(0x4af7e0, 11)
	SDTest.Poke(0x4af7e4, 12)
	SDTest.Poke(0x4af7e8, 13)
	SDTest.Poke(0x4af7f0, 14)
	SDTest.Poke(0x4b25f4, 3 * emcm[2])
	SDTest.Poke(0x4b25f8, 2)
	SDTest.Poke(0x4b25fc, 4096)
	check('emc Motor_DrawStats' + fmt(emcm), lambda: (SDTest.Call(0x2308c0, 'v'), emcstats()))
emcrestore(emcst, emcsts)
emcrestore(emcclock, emcclocks)
emcrestore((0x4ad7e8, 0x4ad7ec), emcvp)

# ---- ViewerCamera_Move (rva 0x218080) on the viewer camera, the debug actions and its variables set here ----
emccam = emcgi(0x4a830c)
emccc = 0
if emccam:
	emccc = emcri(emccam + 0x34)
check('emc viewer camera', lambda: (emccam != 0, emccc != 0))
emcset = 0
emcacts = 0
if emcgi(0x44c128) > 1:
	emcset = emcgi(0x44c12c) + 0xc
	if emcri(emcset + 4) > 23:
		emcacts = emcri(emcset + 8)
if emccam and emccc and emcacts:
	emcvv = (0x4a87b0, 0x4a87c8, 0x4a8610, 0x4a8660, 0x4a86f0, 0x4a85b0, 0x431b04, 0x4a8ed4)
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
		return (emcrf(emccam, 12), emcgi(0x4a8660), emcgf(0x431b04), emcri(emccc + 0x48), emcrf(emccc + 8, 9))
	def emccamset(pos, rot, tgt):
		emcwf(emccam, pos + rot + tgt)
		emcwf(emccc + 8, rot + pos + tgt)
	for emci in emcaidx:
		emcact(emci, 0.0, 0.0)
	emcsf(0x4a87b0, 2.5)
	emcsf(0x4a87c8, 9000.0)
	emcsf(0x4a8610, 0.0)
	emcsf(0x4a86f0, 1.0)
	emcsf(0x431b04, 1.0)
	emcsf(0x4a8ed4, 0.05)
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
		SDTest.Poke(0x4a8660, emcc[0])
		SDTest.Poke(0x4a85b0, emcc[1])
		emcsf(0x4a8610, emcc[2])
		emcact(2, 0.0, emcc[3])
		emcact(5, 0.0, emcc[4])
		emcact(16, 0.0, emcc[5])
		emcact(18, 0.0, emcc[6] * 1.0)
		emccamset(emcc[7], emcc[8], emcc[9])
		check('emc ViewerCamera_Move' + fmt(emcc[0:7]), lambda: (SDTest.Call(0x218080, 'vi', emccam), emccamstate()))
	for emci in (2, 5, 16, 18):
		emcact(emci, 0.0, 0.0)
	emccamset((5.0, 6.0, 7.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
	emcact(23, 0.0, 1.0)
	check('emc ViewerCamera_Move toggle', lambda: (SDTest.Call(0x218080, 'vi', emccam), emccamstate()))
	emcact(23, 1.0, 1.0)
	check('emc ViewerCamera_Move held', lambda: (SDTest.Call(0x218080, 'vi', emccam), emccamstate()))
	emcact(23, 0.0, 0.0)
	emcact(14, 0.0, 1.0)
	check('emc ViewerCamera_Move faster', lambda: (SDTest.Call(0x218080, 'vi', emccam), emccamstate()))
	emcact(14, 0.0, 0.0)
	emcact(15, 0.25, 0.75)
	check('emc ViewerCamera_Move slower', lambda: (SDTest.Call(0x218080, 'vi', emccam), emccamstate()))
	for emci in range(len(emcaidx)):
		emcwi(emcacts + emcaidx[emci] * 0x7c + 8, emcas[emci][0])
		emcwi(emcacts + emcaidx[emci] * 0x7c + 0x18, emcas[emci][1])
	for emci in range(13):
		emcwi(emccam + emci * 4, emccams[emci])
	for emci in range(28):
		emcwi(emccc + emci * 4, emcccs[emci])
	emcrestore(emcvv, emcvvs)
