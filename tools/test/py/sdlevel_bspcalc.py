# engine3d Engine/Bsp.c, the map read from the .EMI of FZ (execfile'd by sdlevel.py, Python 1.5.2): the lists,
# materials and pages CBsp_LeerEMI made at the level load, the sector buffers of CBsp_InitD3D (also once more after a
# release), the statistics of CBsp_Preparar (debug level 3, the last console lines) and the compiler pieces on
# small hand-built lists.
def bsci(a):
	return SDTest.Peek(a, '@i')
def bscp(a):
	return SDTest.Peek(a, '@p')
def bscz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def bscpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
# a hash of n bytes at a (the 16 bit halves summed, the words xored)
def bschash(a, n):
	s = 0
	x = 0
	for i in range(n / 4):
		v = bsci(a + 4 * i)
		s = (s + (v & 0xffff) + ((v >> 16) & 0xffff)) % 1000003
		x = x ^ v
	if n % 4:
		v = bsci(a + n - n % 4) & ((1 << 8 * (n % 4)) - 1)
		s = (s + v) % 1000003
	return (s, x)
bscvsize = {3: 0x1c, 7: 0x24, 10: 0x20, 5: 0x2c}
def bsclv(lv):
	n, frmt = bsci(lv + 4), bsci(lv + 0xc)
	r = (n, frmt, bsci(lv) != 0)
	if bsci(lv) and bscvsize.has_key(frmt):
		r = r + bschash(bsci(lv), n * bscvsize[frmt])
	return r
def bscmat(m):
	if not m:
		return None
	r = [bscp(m + 4), bsci(m + 0x3c) & 0xffff, SDTest.Peek(m + 0x3a, '@b')]
	for i in range(5):
		mp = bsci(m + 0x9c + 4 * i)
		if mp:
			r.append((i, bscp(mp), SDTest.Peek(mp + 9, '@b')))
	return r
bscbsp = SDTest.Peek(0x4b2b38, 'i')
check('bsc map', lambda: (bscbsp != 0, SDTest.Peek(0x4b2b00, 'i'), SDTest.Peek(0x432970, 'i')))
if bscbsp:
	# the pages
	bscpages = []
	bsclm = SDTest.Peek(0x4b2b3c, 'i')
	while bsclm and len(bscpages) < 200:
		bscpages.append(bsclm)
		bsclm = bsci(bsclm)
	def bscpage(lm):
		l2 = bsci(lm + 0x2c)
		r = (bsci(lm + 0x28), bsci(lm + 0xc), bsci(lm + 0x18), bscmat(bsci(lm + 0x1c)), l2 != 0)
		if l2:
			r = r + (bsci(l2 + 0x28), bscmat(bsci(l2 + 0x1c)))
		return r
	check('CBsp_LeerEMI pages', lambda: len(bscpages))
	for bsc_i in range(len(bscpages)):
		check('CBsp_LeerEMI page' + fmt((bsc_i,)), lambda lm=bscpages[bsc_i]: bscpage(lm))
	def bscpageidx(lm):
		for i in range(len(bscpages)):
			if bscpages[i] == lm:
				return i
		if lm:
			return 'ptr'
		return None
	# the lists
	bscn = bsci(bscbsp + 0x4e208)
	check('CBsp_LeerEMI lists', lambda: (bscn, bsci(bscbsp + 0x61a8c)))
	def bsclist(t):
		return (bsci(t + 0xa8), bsci(t + 0x1c8), bsci(t + 0x1d0), bscpageidx(bsci(t + 0x1c0)), bscmat(bsci(t + 0xac)),
			bsci(t) != 0, bsci(t + 0x1c4) != 0, bschash(bsci(t + 0x1d4), bsci(t + 0x1d0) * 6), bsclv(t + 0x1d8), bsclv(t + 0x1ec),
			bsclv(t + 0x200))
	for bsc_i in range(bscn):
		check('CBsp_LeerEMI list' + fmt((bsc_i,)), lambda t=bsci(bscbsp + 8 + 4 * bsc_i): bsclist(t))
	# the device buffers of the sectors and lists
	def bsczonas():
		n, p = bsci(bscbsp + 0x61a98), bsci(bscbsp + 0x61a9c)
		r = [n]
		for i in range(n):
			z = p + 0xb4 * i
			v = []
			for k in range(8):
				b = z + 8 + 0x14 * k
				v.append((bsci(b) != 0, bsci(b + 4), bsci(b + 8), bsci(b + 0xc), bsci(b + 0x10)))
			r.append((v, bsci(z + 0xa8) != 0, bsci(z + 0xac), bsci(z + 0xb0)))
		return r
	def bsclistvb():
		r = []
		for i in range(bscn):
			t = bsci(bscbsp + 8 + 4 * i)
			r.append((bsci(t + 0x214) != 0, bsci(t + 0x218) != 0, bsci(t + 0x21c), bsci(t + 0x220)))
		return r
	def bscfxmats():
		return (bscmat(bsci(bscbsp + 0x61aa0)), bscmat(bsci(bscbsp + 0x61aa4)))
	check('CBsp_InitD3D zonas', bsczonas)
	check('CBsp_InitD3D lists', bsclistvb)
	check('CBsp_InitD3D mats', bscfxmats)
	bscfx = (bsci(bscbsp + 0x61aa0), bsci(bscbsp + 0x61aa4))
	# Bsp_ReleaseD3D and Bsp_InitD3D, as at a device reset
	check('Bsp_ReleaseD3D', lambda: (SDTest.Call(0x286c80, 'v'), bsczonas()[1], bsclistvb()[0:4]))
	check('Bsp_InitD3D', lambda: (SDTest.Call(0x286cb0, 'v'), bsczonas()))
	check('Bsp_InitD3D lists', bsclistvb)
	check('Bsp_InitD3D mats', lambda: (bscfxmats(), bsci(bscbsp + 0x61aa0) == bscfx[0], bsci(bscbsp + 0x61aa4) == bscfx[1]))
	# the statistics at debug level 3: the console lines written last
	def bscconsole():
		r = [SDTest.Peek(0x445a44, 'i')]
		for i in range(23):
			r.append(SDTest.Peek(0x444e68 + 0x84 * i, 's'))
		return r
	bsclevel = SDTest.Peek(0x445a74, 'i')
	check('CBsp_Preparar off', lambda: (SDTest.Call(0x28efe0, 'tvi', bscbsp), SDTest.Peek(0x445a44, 'i')))
	SDTest.Poke(0x445a74, 3)
	check('CBsp_Preparar', lambda: (SDTest.Call(0x28efe0, 'tvi', bscbsp), bscconsole()))
	SDTest.Poke(0x445a74, bsclevel)
	# a map without its .EMI
	bscfake = bscz(0x61aa8)
	bscname = bscz(32)
	SDTest.Call(0x3197d0, 'iisi', bscname, 'Levels/FZ/Map/sdbscnone.sm3', 28)
	bscpoke(bscfake, bscname)
	check('CBsp_LeerEMI none', lambda: (SDTest.Call(0x28c650, 'tii', bscfake)[0], bsci(bscfake + 0x4e208)))

# the lights of a scene made here into the light table of the compiler
def bscfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def bscwf(a, fl):
	SDTest.Call(0x3197d0, 'iiFi', a, fl, 4 * len(fl))
def bscstr(s):
	p = bscz(len(s) + 1)
	SDTest.Call(0x3197d0, 'iisi', p, s, len(s))
	return p
# tipo, sombra, color, (intensidad f30 rango theta phi mult f44), position + direction, node flags, active
bscluces = (
	(5000, 1, 0x7f4080ff, (1.5, 0.25, 300.0, 30.0, 40.0, 1.0, 0.75), (10.0, 20.0, 30.0, 0.0, -1.0, 0.0), 0x4000000, 1),
	(5002, 0, 0x10203040, (0.5, 0.0, 0.0, 43.0, 45.0, 1.0, 1.0), (1.0, 2.0, 3.0, 0.0, 0.0, 0.0), 0xc000000, 1),
	(5001, 1, 0x00ffffff, (-2.0, 1.0, 50.0, 20.0, 25.0, 2.0, 0.5), (-5.0, 6.0, 7.0, 3.0, 4.0, 12.0), 0x8000000, 1),
	(5001, 1, 0x00ffffff, (1.0, 1.0, 50.0, 20.0, 25.0, 2.0, 0.5), (0.0, 0.0, 0.0, 1.0, 0.0, 0.0), 0x4000000, 0),
	(5002, 1, 0x00808080, (0.75, 0.0, 0.0, 43.0, 45.0, 1.0, 1.0), (0.0, 0.0, 0.0, 1e-30, 1e-30, 0.0), 0x4000000, 1))
bscnl = SDTest.Peek(0x4ac434, 'i')
bscls = []
bscprev = 0
bscesc = bscz(0x64)
for bsc_i in range(len(bscluces) + 1):
	bscnd = bscz(0xfc)
	bscpoke(bscnd + 4, bscstr('sdbscluz' + fmt((bsc_i,))))
	if bscprev:
		bscpoke(bscprev + 0xc, bscnd)
	else:
		bscpoke(bscesc + 0xc, bscnd)
	bscprev = bscnd
	if bsc_i == 2:
		# a node without an object between the lights
		continue
	bsc_d = bscluces[bsc_i - (bsc_i > 2)]
	bscl = bscz(0xcc)
	SDTest.Call(0x23ab70, 'tii', bscl)
	bscpoke(bscl + 8, bsc_d[0])
	bscpoke(bscl + 0xc, bsc_d[1] << 8)
	bscpoke(bscl + 0x28, bsc_d[2])
	bscwf(bscl + 0x2c, bsc_d[3])
	bscwf(bscl + 0x80, bsc_d[4])
	bscpoke(bscnd + 0x18, bsc_d[5])
	bscpoke(bscnd + 0x3c, bscl)
	bscls.append((bscl, bsc_d[6]))
bscpoke(bscesc + 0x14, 0x00204080)
bscwf(bscesc + 0x18, (1.5,))
def bscactivar():
	for l, on in bscls:
		SDTest.Call(0x23cb20, 'tvii', l, on)
def bsctabla():
	n = SDTest.Peek(0x4b2b44, 'i')
	r = [n]
	for i in range(min(n, 8)):
		e = 0x4b2c20 + 0x118 * i
		r.append((bscfl(e + 0x400000, 6), SDTest.Peek(e + 0x18, 'i'), bscfl(e + 0x40001c, 1), SDTest.Peek(e + 0x20, 'i'),
			bscfl(e + 0x400024, 29), SDTest.Peek(e + 0x98, 's')))
	return tuple(r)
def bscactivas():
	r = []
	for l, on in bscls:
		r.append(SDTest.Peek(l + 0xba, '@b'))
	return r
def bscambiente():
	return (bsci(bscesc + 0x14), bscfl(bscesc + 0x18, 1))
bscactivar()
check('Bsp_TomarLuces', lambda: (SDTest.Call(0x290930, 'vi', bscesc), bsctabla(), bscactivas(), bscambiente()))
# with the multiplier of the negative lights and the ambient of the ini
bscini = SDTest.Call(0x261980, 'tii', bscz(4))[0]
SDTest.Call(0x2621a0, 'tiisss', bscini, 'mapa_sombras', 'negative_lights_mult', '0.5')
SDTest.Call(0x2621a0, 'tiisss', bscini, 'mapa_sombras', 'lightmap_ambient', '0.2 0.4 0.8')
bscpoke(bscesc + 0x4c, bscini)
bscactivar()
check('Bsp_TomarLuces ini', lambda: (SDTest.Call(0x290930, 'vi', bscesc), bsctabla(), bscactivas(), bscambiente()))
SDTest.Call(0x2621a0, 'tiisss', bscini, 'mapa_sombras', 'negative_lights_mult', '1')
SDTest.Call(0x2621a0, 'tiisss', bscini, 'mapa_sombras', 'lightmap_ambient', '0 0 0')
bscactivar()
check('Bsp_TomarLuces ini 1', lambda: (SDTest.Call(0x290930, 'vi', bscesc), bsctabla()[3], bscambiente()))
check('Bsp_TomarLuces lights', lambda: SDTest.Peek(0x4ac434, 'i') == bscnl)

# lists made here: the texels of their triangles, then the vertex lists of a fake map
def bsctri(v, n=(0.0,) * 9, uv=(0.0,) * 6, uvl=(0.0,) * 6, col=(0, 0, 0)):
	t = bscz(0x224)
	SDTest.Call(0x281d40, 'tii', t)
	bscwf(t + 4, v)
	bscwf(t + 0x28, n)
	bscwf(t + 0x4c, uv)
	bscwf(t + 0x64, uvl)
	for i in range(3):
		bscpoke(t + 0x7c + 4 * i, col[i])
	return t
def bsclink(ts):
	for i in range(len(ts) - 1):
		bscpoke(ts[i], ts[i + 1])
	return ts[0]
def bscbmp(w, h, c):
	b = bscz(12)
	SDTest.Call(0x28ff20, 'tviiii', b, w, h, c)
	return b
def bscswap(t):
	return (t & 0xff000000) | (t & 0xff) << 16 | (t & 0xff00) | (t >> 16 & 0xff)
bscsaved = (SDTest.Peek(0x432950, 'i'), SDTest.Peek(0x432954, 'i'))
SDTest.Poke(0x432950, 0x40000000)
SDTest.Poke(0x432954, 0x3f800000)
check('bsc texels', lambda: (SDTest.Peek(0x432958, 'i'), SDTest.Peek(0x43295c, 'i')))
bscta = bsctri((0.3, 0.0, 0.2, 0.1, 0.0, 4.6, 4.4, 0.0, 0.3))
bsctb = bsctri((4.4, 0.0, 0.3, 0.1, 0.0, 4.6, 4.7, 0.0, 4.9))
bsctc = bsctri((1.0, 0.0, 0.0, 1.5, 3.0, 0.2, 0.8, 0.5, 3.3))
bscmts = [bscta, bsctb, bsctc]
bscl1 = bsclink([bscta, bsctb])
SDTest.Call(0x287bf0, 'vi', bscl1)
SDTest.Call(0x287bf0, 'vi', bsctc)
for bsc_t, bsc_e in ((bscl1, (-2, -2, 12, 12)), (bsctc, (-3, -1, 11, 13))):
	for bsc_i in range(4):
		bscpoke(bsc_t + 0xdc + 4 * bsc_i, bsc_e[bsc_i])
	SDTest.Call(0x28ff20, 'tviiii', bsc_t + 0x114, 16, 16, 0)
SDTest.Call(0x28ff20, 'tviiii', bsctc + 0x120, 16, 16, 0)
bscpoke(bsctc + 0x12c, 0x00332211)
bsccob = bscbmp(16, 16, 0xffff0000)
def bscpix(b, x, y):
	return bsci(bsci(b + 8) + 4 * (16 * y + x))
def bsccobs(b):
	r = []
	for y in range(16):
		s = ''
		for x in range(16):
			s = s + '%02x' % (bscpix(b, x, y) & 0xff)
		r.append(s)
	return tuple(r)
def bscmask(b):
	r = []
	for y in range(16):
		s = ''
		for x in range(16):
			v = bscpix(b, x, y)
			c = '?'
			if v == 0:
				c = '.'
			elif v == bscswap(0x00332211):
				c = 'k'
			for i in range(len(bscmts)):
				if v == bscswap(bscmts[i]):
					c = chr(ord('a') + i)
			s = s + c
		r.append(s)
	return tuple(r)
def bscext2(t):
	return (bsci(t + 0xec), bsci(t + 0xf0), bsci(t + 0xf4), bsci(t + 0xf8))
check('BspTri_Mascara', lambda: (SDTest.Call(0x285d80, 'vii', bscl1, bsccob), bscext2(bscta), bscext2(bsctb), bsccobs(bsccob),
	bscmask(bscl1 + 0x114), bscpix(bsccob, 5, 5) >> 8))
check('BspTri_Mascara 2', lambda: (SDTest.Call(0x285d80, 'vii', bsctc, bsccob), bscext2(bsctc), bsccobs(bsccob),
	bscmask(bsctc + 0x114), bscmask(bsctc + 0x120)))
SDTest.Poke(0x432950, bscsaved[0])
SDTest.Poke(0x432954, bscsaved[1])

# the vertex lists of a fake map: lists of the same material, page and sector joined
bscmata = bscz(0xb0)
SDTest.Call(0x233830, 'tii', bscmata)
SDTest.Call(0x2a1940, 'tvis', bscmata + 4, 'sdbscmatA')
bscmatb = bscz(0xb0)
SDTest.Call(0x233830, 'tii', bscmatb)
SDTest.Call(0x2a1940, 'tvis', bscmatb + 4, 'sdbscmatB')
bscpoke(bscmatb + 0x38, bsci(bscmatb + 0x38) | 0x01000000)
bscpg1 = bscz(0x30)
bscpoke(bscpg1 + 0x28, 1)
bscpg2 = bscz(0x30)
bscpoke(bscpg2 + 0x28, 2)
bscmap = bscz(0x61aa8)
bscvtris = []
def bscvlist(mat, lm, zona, flags, tris):
	ts = []
	for v, n, uv, uvl, col in tris:
		t = bsctri(v, n, uv, uvl, col)
		bscpoke(t + 0xac, mat)
		bscpoke(t + 0x1c0, lm)
		bscpoke(t + 0x1c8, zona)
		bscpoke(t + 0xa8, flags)
		ts.append(t)
		bscvtris.append(t)
	n = bsci(bscmap + 0x4e208)
	bscpoke(bscmap + 8 + 4 * n, bsclink(ts))
	bscpoke(bscmap + 0x4e208, n + 1)
bscq = (0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
bscnrm = (0.0, 1.0, 0.0, 0.3337, 0.9426, 0.0012, -0.70711, 0.70711, 0.0)
bscuv = (0.0, 0.0, 1.0, 0.0, 0.0, 1.0)
bscvlist(bscmata, bscpg1, 0, 0, ((bscq, bscnrm, bscuv, bscuv, (1, 2, 3)), (bscq, bscnrm, bscuv, (0.5,) * 6, (1, 2, 3)),
	((0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0), bscnrm, bscuv, bscuv, (1, 2, 3))))
bscvlist(bscmatb, bscpg1, 1, 0, ((bscq, bscnrm, bscuv, bscuv, (7, 7, 7)),))
bscvlist(bscmata, bscpg1, 0, 0, ((bscq, bscnrm, bscuv, bscuv, (1, 2, 4)), (bscq, bscnrm, bscuv, bscuv, (1, 2, 3))))
bscvlist(bscmata, bscpg2, 0, 0, ((bscq, bscnrm, bscuv, bscuv, (5, 5, 5)),))
bscvlist(bscmatb, 0, -1, 1, ((bscq, bscnrm, bscuv, bscuv, (9, 9, 9)), (bscq, bscnrm, (0.25,) * 6, bscuv, (9, 9, 9))))
bscvlist(bscmatb, 0, -1, 1, ((bscq, bscnrm, bscuv, bscuv, (9, 9, 9)),))
def bscfls(a, n):
	r = ()
	i = 0
	while i < n:
		k = min(32, n - i)
		r = r + bscfl(a + 4 * i, k)
		i = i + k
	return r
def bscvidx(t):
	for i in range(len(bscvtris)):
		if bscvtris[i] == t:
			return i
	if t:
		return 'ptr'
	return None
def bscvmap():
	n = bsci(bscmap + 0x4e208)
	r = [n]
	for i in range(n):
		t = bsci(bscmap + 8 + 4 * i)
		ch = []
		c = t
		while c and len(ch) < 50:
			ch.append(bscvidx(c))
			c = bsci(c)
		nt = bsci(t + 0x1d0)
		ix = []
		for k in range(nt * 3):
			w = bsci(bsci(t + 0x1d4) + 2 * k)
			ix.append(w & 0xffff)
		lv = t + 0x1d8
		nv, frmt = bsci(lv + 4), bsci(lv + 0xc)
		r.append((tuple(ch), nt, tuple(ix), nv, frmt, bscfls(bsci(lv), nv * bscvsize[frmt] / 4)))
	return tuple(r)
check('CBsp_CrearBuffers', lambda: (SDTest.Call(0x28d520, 'tvi', bscmap), bscvmap()))

# the direct light of two lists above the player, the lights made here, the shadows traced through the floor of FZ
bscgl = (0x432950, 0x432954, 0x4b2b04, 0x4b2b0c, 0x4b2b24, 0x4b2b74, 0x4b2b20, 0x4b2b14, 0x4b2b18, 0x4d5d20)
bscglsaved = []
for bsc_a in bscgl:
	bscglsaved.append(SDTest.Peek(bsc_a, 'i'))
check('bsc light globals', lambda: tuple(bscglsaved))
SDTest.Poke(0x432950, 0x3f800000)
SDTest.Poke(0x432954, 0x3f800000)
SDTest.Poke(0x4b2b24, 0)
SDTest.Poke(0x4b2b74, 0)
bscesc2 = bscz(0x64)
bscpoke(bscesc2 + 0x14, 0x00102030)
bscwf(bscesc2 + 0x18, (0.5,))
bscls = []
bscprev = 0
bsco = (pp[0], pp[1] + 1.5, pp[2])
for bsc_d in ((5000, 1, 0x7fffffff, (1.0, 0.0, 0.0, 30.0, 40.0, 1.0, 0.5), (bsco[0] + 1.0, bsco[1] + 40.0, bsco[2] + 2.0, 0.0, -1.0, 0.0),
		0x4000000), (5002, 1, 0x00c0e0ff, (0.75, 0.0, 0.0, 43.0, 45.0, 1.0, 1.0), (0.0, 0.0, 0.0, 0.3, -1.0, 0.2), 0),
		(5000, 1, 0x00ffffff, (2.0, 0.0, 0.0, 30.0, 40.0, 1.0, 1.0), (bsco[0] + 3.0, bsco[1] - 200.0, bsco[2], 0.0, -1.0, 0.0), 0),
		(5001, 0, 0x00ff8040, (1.5, 0.0, 300.0, 20.0, 25.0, 1.0, 2.0), (bsco[0], bsco[1] + 10.0, bsco[2] + 4.0, 0.0, -1.0, 0.0), 0)):
	bscnd = bscz(0xfc)
	bscpoke(bscnd + 4, bscstr('sdbscdir' + fmt((len(bscls),))))
	if bscprev:
		bscpoke(bscprev + 0xc, bscnd)
	else:
		bscpoke(bscesc2 + 0xc, bscnd)
	bscprev = bscnd
	bscl = bscz(0xcc)
	SDTest.Call(0x23ab70, 'tii', bscl)
	bscpoke(bscl + 8, bsc_d[0])
	bscpoke(bscl + 0xc, bsc_d[1] << 8)
	bscpoke(bscl + 0x28, bsc_d[2])
	bscwf(bscl + 0x2c, bsc_d[3])
	bscwf(bscl + 0x80, bsc_d[4])
	bscpoke(bscnd + 0x18, bsc_d[5])
	bscpoke(bscnd + 0x3c, bscl)
	bscls.append((bscl, 1))
check('Bsp_TomarLuces 2', lambda: (SDTest.Call(0x290930, 'vi', bscesc2), bsctabla()))
# the lists: a square of two triangles and one triangle of a node without shadows
bscup = (0.0, 1.0, 0.0) * 3
def bscdtri(a, b, c, nodo):
	o = bsco
	t = bsctri((o[0] + a[0], o[1], o[2] + a[1], o[0] + b[0], o[1], o[2] + b[1], o[0] + c[0], o[1], o[2] + c[1]), bscup)
	bscpoke(t + 0x1c4, nodo)
	return t
bscnd1 = bscz(0xfc)
bscnd2 = bscz(0xfc)
bscpoke(bscnd2 + 0x18, 0x40)
bscda = bscdtri((0.0, 0.0), (0.0, 6.0), (6.0, 0.0), bscnd1)
bscdb = bscdtri((6.0, 0.0), (0.0, 6.0), (6.0, 6.0), bscnd1)
bscdc = bscdtri((-4.0, -1.0), (-4.0, 2.0), (-1.0, -1.0), bscnd2)
bscdts = [bscda, bscdb, bscdc]
def bscdlist(ts):
	l = bsclink(ts)
	SDTest.Call(0x287bf0, 'vi', l)
	bscpoke(l + 0xac, bscmata)
	b = SDTest.Call(0x287930, 'iFi', (0.0,) * 6, l)[1]
	e = SDTest.Call(0x287ac0, 'vFifpppp', b, 1, 1.0, 0, 0, 0, 0)
	SDTest.Call(0x2900d0, 'viiiiii', l, e[2], e[3], e[4], e[5], 0)
	lx, ly = bsci(l + 0xd4), bsci(l + 0xd8)
	SDTest.Call(0x28ff20, 'tviiii', l + 0x114, lx, ly, 0)
	SDTest.Call(0x285d80, 'vii', l, bscbmp(lx, ly, 0xffff0000))
	return l
bscdl1 = bscdlist([bscda, bscdb])
bscdl2 = bscdlist([bscdc])
bscdmap = bscz(0x61aa8)
bscpoke(bscdmap, bscstr('sdbsctxf/map.sm3'))
bscdmod = bscz(0x128)
bscpoke(bscdmod + 0x124, bscesc2)
bscpoke(bscdmap + 4, bscdmod)
bscpoke(bscdmap + 8, bscdl1)
bscpoke(bscdmap + 12, bscdl2)
bscpoke(bscdmap + 0x4e208, 2)
def bscflt2(b):
	img = bsci(b + 8)
	if not img:
		return (bsci(b), bsci(b + 4), 0)
	return (bsci(b), bsci(b + 4), bscfls(img, 3 * bsci(b) * bsci(b + 4)))
def bscdlight():
	r = []
	for l in (bscdl1, bscdl2):
		r.append((bsci(l + 0xd4), bsci(l + 0xd8), bscflt2(l + 0xb0), bscflt2(l + 0xbc), bscflt2(l + 0x108)))
	return tuple(r)
# hard shadows, all three maps kept (the fading of mapa_sombras aten needs the hit point, which CSuelo_Trace CHECKs)
SDTest.Poke(0x4b2b04, 0)
SDTest.Poke(0x4b2b0c, 0)
SDTest.Poke(0x4b2b20, 1)
SDTest.Poke(0x4b2b14, 1)
SDTest.Poke(0x4b2b18, 0)
check('CBsp_LuzDirecta', lambda: (SDTest.Call(0x2888c0, 'tvi', bscdmap), bscdlight()))
# without the ambient, the lineal attenuation and the secondary maps
SDTest.Poke(0x4b2b0c, 0)
SDTest.Poke(0x4b2b20, 0)
SDTest.Poke(0x4b2b14, 0)
SDTest.Poke(0x4b2b18, 1)
SDTest.Poke(0x4d5d20, 1 - bscglsaved[9])
check('CBsp_LuzDirecta 2', lambda: (SDTest.Call(0x2888c0, 'tvi', bscdmap), bscdlight()))
# soft shadows on the small list
SDTest.Poke(0x4d5d20, bscglsaved[9])
SDTest.Poke(0x4b2b04, 1)
SDTest.Poke(0x4b2b18, 0)
bscpoke(bscdmap + 0x4e208, 1)
bscpoke(bscdmap + 8, bscdl2)
SDTest.Call(0x323c50, 'vi', 0x4321)
check('CBsp_LuzDirecta soft', lambda: (SDTest.Call(0x2888c0, 'tvi', bscdmap), bscdlight()[1]))
for bsc_i in range(len(bscgl)):
	SDTest.Poke(bscgl[bsc_i], bscglsaved[bsc_i])
