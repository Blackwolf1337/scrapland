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
