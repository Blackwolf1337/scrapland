# engine3d Engine/Animacion.c (execfile'd at the end of sdcall.py, Python 1.5.2): animations and nodes in heap memory
# with random tracks, the optimization into constants and streams, and animations read from the packs. Pointers are
# printed as offsets or as "set", never as addresses. Not called: CAnimNodo_FreeTracks (always fails its CHECK(0)).
# CAnimacion_Write and the node writers run on a CArchIO with the error flag set, which writes nothing.
def anew(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def afree(p):
	SDTest.Call(0x133550, 'vi', p)
def awf(addr, vals):
	i = 0
	while i < len(vals):
		k = min(64, len(vals) - i)
		SDTest.Call(0x3197d0, 'iiFi', addr + 4 * i, tuple(vals[i:i + k]), 4 * k)
		i = i + k
# the bytes of s one by one (a 'b' argument can not hold NUL bytes)
def awb(addr, s):
	for i in range(len(s)):
		v = SDTest.Peek(addr + i, '@i')
		SDTest.Poke(addr + i - 0x400000, (v & ~0xff) | ord(s[i]))
def awi(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def ari(addr):
	return SDTest.Peek(addr, '@i')
def afl(addr, n):
	r = ()
	i = 0
	while i < n:
		k = min(64, n - i)
		r = r + SDTest.Call(0x3197d0, 'iFii', (0.0,) * k, addr + 4 * i, 4 * k)[1]
		i = i + k
	return r
def arb(addr, n):
	r = []
	for i in range(n):
		r.append(SDTest.Peek(addr + i, '@b'))
	return r
def ablocks():
	return SDTest.Peek(0x44b628 + 8, 'i')
def acount():
	return SDTest.Peek(0x4dc104, 'i')
def aint(lo, hi):
	return int(rnd(lo + 0.0, hi + 0.999))
def aoff(p, base):
	if p == 0:
		return None
	return p - base
def aget(a):
	return (ari(a + 8), ari(a + 0xc), ari(a + 0x10), SDTest.Peek(a + 0x14, '@b'), ari(a + 0x18), ari(a + 0x24),
		ari(a + 0x20) != 0, ari(a + 0x28) != 0, ari(a + 0x2c), ari(a + 0x30))
def anodo(a, i):
	return ari(a + 0x20) + 0x24 * i
def nhead(n):
	w0, w1 = ari(n), ari(n + 4)
	return (w0 & 0xffff, (w0 >> 16) & 0xffff, w1 & 0xffff, (w1 >> 16) & 0xffff)
def nptrs(n):
	r = []
	for off in (8, 12, 16, 20, 24, 28, 32):
		r.append(ari(n + off) != 0)
	return tuple(r)
def nstate(n):
	return nhead(n), nptrs(n)
def nflags(n):
	return SDTest.Call(0x2aeb00, 'tii', n)[0]
# the values of the tracks: plain ones as arrays, streams by their bytes
def ntracks(n):
	h = nhead(n)
	r = []
	for bit, off, size in ((1, 8, 3), (2, 12, 4), (4, 16, 1), (8, 20, 1), (16, 24, 1)):
		p = ari(n + off)
		if not p:
			r.append(None)
		elif h[2] & h[3] & bit:
			r.append(arb(p, ari(p) & 0xffff))
		elif h[2] & bit:
			r.append(afl(p, size))
		elif bit == 8:
			r.append(tpeek(p, h[1]))
		else:
			r.append(afl(p, size * h[1]))
	p = ari(n + 28)
	if p and not (h[2] & 0x80):
		r.append(arb(p, h[1]))
	return r
def ascale():
	return rnd(0.25, 3.0)
def uq():
	a = vec()
	return SDTest.Call(0x2a4040, 'vFfff', (0.0, 0.0, 0.0, 0.0), a[0], a[1], a[2])[1]
# unit: only unit quaternions (CAnimNodo_Optimize CHECKs their dot products)
def nfill(n, flags, unit=0):
	h = nhead(n)
	nf = h[1]
	if flags & 1:
		v = []
		for i in range(nf):
			v = v + list(vec())
		awf(ari(n + 8), v)
	if flags & 2:
		v = []
		for i in range(nf):
			if i % 3 == 2 and not unit:
				v = v + list(quat())
			else:
				v = v + list(uq())
		awf(ari(n + 12), v)
	if flags & 4:
		v = []
		for i in range(nf):
			v.append(rnd(0.1, 2.0))
		awf(ari(n + 16), v)
	if flags & 8:
		for i in range(nf):
			awi(ari(n + 20) + 4 * i, aint(0, 255) + 256 * aint(0, 255) + 65536 * aint(0, 255) + 16777216 * aint(0, 127))
	if flags & 0x10:
		v = []
		for i in range(nf):
			v.append(rnd(0.0, 4.0))
		awf(ari(n + 24), v)
	if flags & 0x80:
		s = ''
		for i in range(nf):
			x = aint(0, 255)
			if i % 4 == 1:
				x = 255
			elif i % 4 == 2:
				x = 0
			s = s + chr(x)
		awb(ari(n + 28), s)
def asf(k):
	if k % 5 == 0:
		return 0.0
	if k % 7 == 3:
		return 0.99999994
	return rnd(0.0, 0.999)
# the frames to ask: around both ends and some inside
def aframes(n):
	h = nhead(n)
	first, nf = h[0], h[1]
	r = [first - 1, first, first + nf - 1, first + nf, first + nf + 3]
	for i in range(8):
		r.append(first + aint(0, nf - 1))
	if first == 0:
		r[0] = 0x7fffffff
	return r
# the getters that the tracks of the node allow without failing a CHECK
def nquery(name, n):
	h = nhead(n)
	fl = nflags(n)
	opt, stm = h[2], h[3]
	k = 0
	for f in aframes(n):
		sf = asf(k)
		k = k + 1
		a = fmt((f, sf))
		if (fl & 3) == 3 or (fl & 3) == 0:
			check(name + ' CAnimNodo_GetPosRot' + a, lambda n=n, f=f, sf=sf: SDTest.Call(0x2aec30, 'tbiFFif', n, zero3, (0.0, 0.0, 0.0, 0.0), f, sf))
		if (fl & 3) == 3 and not (opt & 3):
			check(name + ' CAnimNodo_GetPosRotFrame' + a, lambda n=n, f=f: SDTest.Call(0x2aeb60, 'tbiFFi', n, zero3, (0.0, 0.0, 0.0, 0.0), f))
		if fl & 4:
			check(name + ' CAnimNodo_GetFOV' + a, lambda n=n, f=f, sf=sf: SDTest.Call(0x2ae9a0, 'tbiFif', n, (0.0,), f, sf))
		if (fl & 0x18) == 0x18 and not (opt & 0x18):
			check(name + ' CAnimNodo_GetColor' + a, lambda n=n, f=f, sf=sf: SDTest.Call(0x2ae7d0, 'tbiFFif', n, zero3, (0.0,), f, sf))
		if not (opt & 0x80):
			check(name + ' CAnimNodo_GetVisible' + a, lambda n=n, f=f, sf=sf: SDTest.Call(0x2aef40, 'tbipif', n, 0, f, sf))
		check(name + ' CAnimNodo_TrackKind' + fmt((f,)), lambda n=n, f=f: (SDTest.Call(0x2abbc0, 'tiiiii', n, f, 0, 1)[0],
			SDTest.Call(0x2abbc0, 'tiiiii', n, f, 0, 2)[0], SDTest.Call(0x2abbc0, 'tiiiii', n, f, 0, 4)[0],
			SDTest.Call(0x2abbc0, 'tiiiii', n, f, 0, 3)[0], SDTest.Call(0x2abbc0, 'tiiiii', n, f, 0, 8)[0]))
		if f - h[0] >= 0 and f - h[0] < h[1]:
			check(name + ' CAnimNodo_FrameIndex' + fmt((f,)), lambda n=n, f=f: SDTest.Call(0x2adb50, 'tiiiii', n, f, 1, 0)[0])

SDTest.Call(0x323c50, 'vi', 0x5a17)

# the pure functions
for i in range(40):
	fcall('Vec3_Pack', 0x2acd40, 'iF', (rnd(-500.0, 500.0), rnd(-500.0, 500.0), rnd(-500.0, 500.0)))
	fcall('Vec3_AngDiff', 0x2ada20, 'vFFF', zero3, vec(), vec())
	fcall('Vec3_AngDiff big', 0x2ada20, 'vFFF', zero3, (rnd(-3.0, 3.0), rnd(-6.0, 6.0), rnd(-9.0, 9.0)), vec())
	q = quat()
	fcall('Quat_Similar same', 0x2adfd0, 'bFF', q, q)
	fcall('Quat_Similar near', 0x2adfd0, 'bFF', q, (q[0] * 1.0000001, q[1] * 0.9999999, q[2] * 1.0000002, q[3] * 0.9999998))
	fcall('Quat_Similar far', 0x2adfd0, 'bFF', q, (q[0] * 1.000001, q[1], q[2], q[3]))
	fcall('Quat_Similar any', 0x2adfd0, 'bFF', q, quat())
	fcall('Quat_Similar sign', 0x2adfd0, 'bFF', q, (q[0], -q[1], q[2], q[3]))
	fcall('Quat_ToEulerNeg', 0x2ad0a0, 'vFF', zero3, uq())
	fcall('Quat_ToEulerNeg any', 0x2ad0a0, 'vFF', zero3, quat())
for v in ((-512.25, 511.5, 511.9), (511.9, -512.4, -512.5), (0.0, 0.0, 0.0), (-0.3, 0.3, -0.7), (100.5, -100.5, 7.99)):
	fcall('Vec3_Pack s', 0x2acd40, 'iF', v)
for x in specials:
	fcall('Vec3_AngDiff s', 0x2ada20, 'vFFF', zero3, (x, 3.1415927, -x), (0.0, -3.1415927, 1.0))
	fcall('Quat_Similar s', 0x2adfd0, 'bFF', (x, 1.0, x, 0.5), (1.0, 1.0, x, 0.5))
	fcall('Quat_Similar s2', 0x2adfd0, 'bFF', (x, x, x, x), (x, x, x, x))
	fcall('Quat_Similar s3', 0x2adfd0, 'bFF', (0.5, x, 0.25, 1.0), (0.5, 2.0, 0.25, 1.0))
for a, b in (((3.1415927, -3.1415927, 0.0), (-3.1415927, 3.1415927, 6.2831855)), ((0.0, 0.0, 0.0), (3.1415925, -3.1415925, 3.1415928)),
		((10.0, -10.0, 0.0), (-10.0, 10.0, 20.0))):
	fcall('Vec3_AngDiff e', 0x2ada20, 'vFFF', zero3, a, b)
for a, b in (((0.0, 1.0, 1.0, 1.0), (1e-30, 1.0, 1.0, 1.0)), ((-0.0, 1.0, 1.0, 1.0), (0.0, 1.0, 1.0, 1.0)),
		((1.0, 1.0, 1.0, 1.0), (1.0000002, 0.9999998, 1.0, 1.0)), ((-1.0, 1.0, 1.0, 1.0), (-1.0000003, 1.0, 1.0, 1.0))):
	fcall('Quat_Similar e', 0x2adfd0, 'bFF', a, b)

# the stream of the optimizer
ast = anew(8)
def sget():
	p = ari(ast)
	return ari(ast + 4), arb(p, ari(ast + 4))
for d in (-128, 127, 0, -1, 77):
	check('AnimStream_AddByte' + fmt((d,)), lambda d=d: (SDTest.Call(0x2ace20, 'tvii', ast, d)[0], sget()))
for d in (-32768, 32767, 0x1234, -2):
	check('AnimStream_AddShort' + fmt((d,)), lambda d=d: (SDTest.Call(0x2ad020, 'tvii', ast, d)[0], sget()))
for s, k in ((0, 1), (0x7fff, 65535), (5, 3), (-3, 0x12345)):
	check('AnimStream_AddHeader' + fmt((s, k)), lambda s=s, k=k: (SDTest.Call(0x2acf20, 'tviii', ast, s, k)[0], sget()))
abuf = anew(16)
for s in ('abcdef', '\000\377x', 'z'):
	awb(abuf, s)
	check('AnimStream_AddBytes' + fmt((s,)), lambda s=s: (SDTest.Call(0x2acea0, 'tviii', ast, abuf, len(s))[0], sget()))
afree(abuf)
afree(ari(ast))
afree(ast)

# AnimBlock_Free: freed unless inside the active block
def afreed(p, flags):
	b = ablocks()
	SDTest.Call(0x2abb50, 'vii', p, flags)
	return b - ablocks()
ablk = anew(12)
amem = anew(256)
check('AnimBlock_Free', lambda: afreed(anew(16), 3))
awi(ablk, amem)
awi(ablk + 4, 256)
SDTest.Poke(0x4dc090, ablk)
check('AnimBlock_Free inside', lambda: (afreed(amem + 8, 0x8000), afreed(amem + 255, 0)))
check('AnimBlock_Free outside', lambda: afreed(anew(16), 0))
# CAnimNodo_ReadTrack into the fake block: plain tracks only, a stream would read its size from the file
aio = anew(16)
SDTest.Poke(aio - 0x400000, 1)
rn = anew(0x24)
awi(rn, 0x00050002)
awi(rn + 4, 0x00000001)
for flag, size in ((0, 12), (1, 12), (2, 16), (4, 4)):
	check('CAnimNodo_ReadTrack' + fmt((flag, size)), lambda flag=flag, size=size: (aoff(SDTest.Call(0x2ad110, 'tviiipi', rn, aio, flag, 0, size)[1], amem), ari(ablk + 8)))
SDTest.Poke(0x4dc090, 0)
afree(rn)
afree(ablk)
afree(amem)

# an animation of nodes with random tracks
c0 = acount()
an = anew(0x34)
check('CAnimacion_Init', lambda: (SDTest.Call(0x2ae560, 'tviiii', an, 5, 40, 3)[0], aget(an), acount() - c0))
check('CAnimacion_Init again', lambda: (SDTest.Call(0x2ae560, 'tviiii', an, 6, 41, 1)[0], aget(an), acount() - c0))
for i in range(8):
	check('CAnimacion_GetNodo' + fmt((i,)), lambda i=i: aoff(SDTest.Call(0x2ae760, 'tiii', an, i)[0], ari(an + 0x20)))
anodes = []
for i in range(6):
	anodes.append(anodo(an, i))
for i, nf, first, flags in ((0, 12, 3, 0x9f), (1, 20, 0, 3), (2, 1, 5, 0x9f), (3, 0, 0, 3), (4, 8, 2, 0x1000), (5, 30, 70, 0x1f)):
	n = anodes[i]
	check('CAnimNodo_Init' + fmt((i, nf, first, flags)), lambda n=n, nf=nf, first=first, flags=flags: (SDTest.Call(0x2ae2c0, 'tviiii', n, nf, first, flags)[0], nstate(n)))
	nfill(n, flags)
	check('CAnimNodo_GetFlags' + fmt((i,)), lambda n=n: nflags(n))
for i in (0, 1, 2, 5):
	nquery('nodo' + fmt((i,)), anodes[i])
n0 = anodes[0]
check('CAnimNodo_Translate', lambda: (SDTest.Call(0x2ade90, 'tbiF', n0, vec())[0], afl(ari(n0 + 8), 36)))
check('CAnimNodo_Translate none', lambda: SDTest.Call(0x2ade90, 'tbiF', anodes[3], vec())[0])
check('CAnimNodo_Scale', lambda: (SDTest.Call(0x2ae160, 'tviF', n0, (ascale(), ascale(), ascale()))[0], afl(ari(n0 + 8), 36)))
s = ascale()
check('CAnimacion_Scale', lambda: (SDTest.Call(0x2ae220, 'tviF', an, (s, s, s))[0], afl(ari(n0 + 8), 36), afl(ari(anodes[1] + 8), 60)))
check('CAnimNodo_GetVert none', lambda: SDTest.Call(0x2ae7b0, 'tiii', n0, 0)[0])
# a node of a single value per track, then a node with hand made streams
n2 = anodes[2]
awi(n2 + 4, 0x7f)
nquery('const', n2)
check('CAnimNodo_Translate const', lambda: (SDTest.Call(0x2ade90, 'tbiF', n2, vec())[0], afl(ari(n2 + 8), 3)))
awi(n2 + 4, 0)
n5 = anodes[5]
SDTest.Call(0x2ae2c0, 'tviiii', n5, 30, 70, 0)
astreams = []
for start, keys in ((0, 1), (2, 3), (9, 25), (40, 2)):
	pp = anew(6 + 12 * keys + 4)
	pr = anew(6 + 16 * keys + 4)
	pf = anew(6 + 4 * keys + 4)
	for p, size in ((pp, 12), (pr, 16), (pf, 4)):
		awi(p, 6 + size * keys + start * 65536)
		awi(p + 4, keys)
	v, w, x = [], [], []
	for k in range(keys):
		v = v + list(vec())
		w = w + list(uq())
		x.append(rnd(0.2, 1.5))
	awf(pp + 6, v)
	awf(pr + 6, w)
	awf(pf + 6, x)
	awi(n5 + 4, 0x70007)
	awi(n5 + 8, pp)
	awi(n5 + 12, pr)
	awi(n5 + 16, pf)
	nquery('stream' + fmt((start, keys)), n5)
	SDTest.Call(0x2ae2c0, 'tviiii', n5, 30, 70, 0)
	check('CAnimNodo_Init free streams', lambda n5=n5: nstate(n5))

# EVA: vertices with frames that move or not
n4 = anodes[4]
check('CAnimNodo_SetEVA', lambda: (SDTest.Call(0x2ae6c0, 'tvii', n4, 3)[0], nptrs(n4), ari(ari(n4 + 32))))
aeva = ari(n4 + 32)
for i in range(3):
	check('CAnimNodo_GetVert' + fmt((i,)), lambda i=i: aoff(SDTest.Call(0x2ae7b0, 'tiii', n4, i)[0], ari(aeva + 4)))
def evafill(e, moving):
	for i in range(ari(e)):
		av = ari(e + 4) + 16 * i
		SDTest.Call(0x2d44a0, 'tvii', av, 4)
		v = list(vec())
		r = []
		for f in range(4):
			if i in moving and f > 0:
				r = r + [v[0] + rnd(-1.0, 1.0), v[1], v[2] + rnd(-0.5, 0.5)]
			else:
				r = r + v
		awf(ari(av + 4), r)
def evaget(n):
	e = ari(n + 32)
	if not e:
		return None
	r = [ari(e)]
	for i in range(ari(e)):
		av = ari(e + 4) + 16 * i
		w = tpeek(av, 4)
		t2 = None
		if w[3]:
			t2 = arb(w[3], 3 * w[0])
		r.append((w[0], w[1] != 0, w[2], t2))
	return r
evafill(aeva, (1,))
check('CAnimNodo_OptimizeEVA', lambda: (SDTest.Call(0x2af070, 'tvi', n4)[0], evaget(n4)))
check('CAnimacion_Scale eva', lambda: (SDTest.Call(0x2ae220, 'tviF', an, (s, s, s))[0], evaget(n4)))
check('CAnimNodo_SetEVA again', lambda: (SDTest.Call(0x2ae6c0, 'tvii', n4, 2)[0], nptrs(n4), ari(ari(n4 + 32))))
evafill(ari(n4 + 32), ())
check('CAnimNodo_OptimizeEVA static', lambda: (SDTest.Call(0x2af070, 'tvi', n4)[0], evaget(n4), nptrs(n4)))
SDTest.Call(0x2ae6c0, 'tvii', n4, 1)

# the table of objects and swapping nodes
atab = anew(8)
awi(atab, 0x00030201)
awi(atab + 4, 0x09070605)
awi(an + 0x18, 8)
awi(an + 0x1c, atab)
for i in range(10):
	check('CAnimacion_GetNodoTabla' + fmt((i,)), lambda i=i: aoff(SDTest.Call(0x2ae280, 'tiii', an, i)[0], ari(an + 0x20)))
awi(an + 0x18, 0)
awi(an + 0x1c, 0)
afree(atab)
p0 = ari(anodes[0] + 8)
check('CAnimacion_SwapNodos', lambda: (SDTest.Call(0x2af0d0, 'tviii', an, 3, 0)[0], nstate(anodes[0]), nstate(anodes[3]), ari(anodes[3] + 8) == p0, acount() - c0))
check('CAnimacion_SwapNodos back', lambda: (SDTest.Call(0x2af0d0, 'tviii', an, 0, 3)[0], nstate(anodes[0]), nstate(anodes[3]), ari(anodes[0] + 8) == p0, acount() - c0))
check('CAnimacion_SwapNodos same', lambda: (SDTest.Call(0x2af0d0, 'tviii', an, 3, 3)[0], nstate(anodes[3]), acount() - c0))

# Trim drops the empty nodes at the end
at = anew(0x34)
SDTest.Call(0x2ae560, 'tviiii', at, 6, 10, 0)
SDTest.Call(0x2ae2c0, 'tviiii', anodo(at, 0), 5, 0, 1)
SDTest.Call(0x2ae2c0, 'tviiii', anodo(at, 2), 6, 1, 0x80)
SDTest.Call(0x2ae2c0, 'tviiii', anodo(at, 4), 3, 0, 0)
nfill(anodo(at, 0), 1)
pt = ari(anodo(at, 0) + 8)
check('CAnimacion_Trim', lambda: (SDTest.Call(0x2adc00, 'tvi', at)[0], aget(at), nstate(anodo(at, 0)), nstate(anodo(at, 1)), nstate(anodo(at, 2)),
	ari(anodo(at, 0) + 8) == pt, acount() - c0))
check('CAnimacion_Trim again', lambda: (SDTest.Call(0x2adc00, 'tvi', at)[0], aget(at), acount() - c0))
check('CAnimacion_Init free', lambda: (SDTest.Call(0x2ae560, 'tviiii', at, 0, 0, 0)[0], aget(at), acount() - c0))
afree(at)

# the optimization before writing: tracks constant (within the errors), changing only in the middle, changing all over
def onode(n, nf, lead, trail, eps):
	SDTest.Call(0x2ae2c0, 'tviiii', n, nf, 0, 7)
	p0, pl = vec(), vec()
	q0, ql = uq(), uq()
	f0, fl = rnd(0.5, 1.5), rnd(0.5, 1.5)
	v, w, x = [], [], []
	for i in range(nf):
		if i < lead:
			v = v + [p0[0] + rnd(-eps, eps), p0[1], p0[2] - rnd(0.0, eps)]
			w = w + list(q0)
			x.append(f0 + rnd(-eps, eps) / 100)
		elif i >= nf - trail:
			v = v + [pl[0], pl[1] + rnd(-eps, eps), pl[2]]
			w = w + list(ql)
			x.append(fl)
		else:
			v = v + list(vec())
			w = w + list(uq())
			x.append(rnd(0.5, 1.5))
	awf(ari(n + 8), v)
	awf(ari(n + 12), w)
	awf(ari(n + 16), x)
ao = anew(0x34)
SDTest.Call(0x2ae560, 'tviiii', ao, 8, 30, 0)
aos = (('stream', 30, 10, 10, 0.004), ('const', 15, 15, 0, 0.004), ('none', 12, 0, 0, 0.0), ('trail', 25, 3, 9, 0.0),
	('lead', 20, 8, 1, 0.001), ('short', 13, 7, 6, 0.0), ('one', 1, 1, 0, 0.0), ('noisy', 18, 9, 9, 0.02))
for i in range(len(aos)):
	name, nf, lead, trail, eps = aos[i]
	n = anodo(ao, i)
	onode(n, nf, lead, trail, eps)
	if name == 'trail':
		awf(ari(n + 8) + 12 * 5, (nan,))
		awf(ari(n + 16) + 4 * 4, (nan,))
	if name == 'lead':
		awf(ari(n + 16) + 4 * 2, (nan,))
	check('CAnimNodo_Optimize ' + name, lambda n=n: (SDTest.Call(0x2ac0e0, 'tvi', n)[0], nstate(n), ntracks(n)))
	nquery('optimized ' + name, n)
SDTest.Call(0x2ae560, 'tviiii', ao, 0, 0, 0)
check('CAnimacion_Init optimized', lambda: (aget(ao), acount() - c0))
optanims = SDTest.Peek(0x4dc0d4, 'i')
SDTest.Call(0x2ae560, 'tviiii', ao, 1, 30, 0)
onode(anodo(ao, 0), 30, 10, 10, 0.0)
SDTest.Poke(0x4dc0d4, 0)
check('CAnimNodo_Optimize off', lambda: (SDTest.Call(0x2ac0e0, 'tvi', anodo(ao, 0))[0], nstate(anodo(ao, 0))))
SDTest.Poke(0x4dc0d4, optanims)
SDTest.Call(0x2ae560, 'tviiii', ao, 0, 0, 0)

# writing to a CArchIO in error: the nodes are optimized, nothing is written
aw = anew(0x34)
SDTest.Call(0x2ae560, 'tviiii', aw, 3, 30, 0)
onode(anodo(aw, 0), 30, 10, 10, 0.0)
onode(anodo(aw, 1), 20, 20, 0, 0.0)
SDTest.Call(0x2ae2c0, 'tviiii', anodo(aw, 2), 9, 4, 0x9f)
nfill(anodo(aw, 2), 0x9f, 1)
for flag, off, size in ((1, 8, 12), (2, 12, 16), (0x80, 28, 1)):
	for hdr in (0, 1):
		check('CAnimNodo_WriteTrack' + fmt((flag, hdr)), lambda flag=flag, off=off, size=size, hdr=hdr: (SDTest.Call(0x2ad390, 'tviiiiii', anodo(aw, 2), aio, flag, ari(anodo(aw, 2) + off), size, hdr)[0], SDTest.Peek(aio, '@b')))
check('CAnimNodo_WriteData', lambda: (SDTest.Call(0x2ad420, 'tvii', anodo(aw, 2), aio)[0], nstate(anodo(aw, 2))))
check('CAnimNodo_WriteHeader', lambda: (SDTest.Call(0x2ad720, 'tvii', anodo(aw, 2), aio)[0], nstate(anodo(aw, 2))))
check('CAnimacion_Write', lambda: (SDTest.Call(0x2ad210, 'tvii', aw, aio)[0], aget(aw), nstate(anodo(aw, 0)), ntracks(anodo(aw, 0)),
	nstate(anodo(aw, 1)), ntracks(anodo(aw, 1)), nstate(anodo(aw, 2))))
SDTest.Call(0x2ae560, 'tviiii', aw, 0, 0, 0)
afree(aw)
afree(aio)
check('CAnimacion_Init end', lambda: (SDTest.Call(0x2ae560, 'tviiii', an, 0, 0, 0)[0], aget(an), acount() - c0))
afree(an)
afree(ao)

# animations of the packs, the ANI block 108 bytes into the file
ario = anew(16)
def aopen(name, skip):
	SDTest.Call(0x21b1f0, 'tiiss', ario, name, 'rb')
	while skip > 0:
		k = min(skip, 1000)
		SDTest.Call(0x21c060, 'tiibi', ario, '', k)
		skip = skip - k
def aclose():
	SDTest.Call(0x21b490, 'tii', ario)
	SDTest.Call(0x21b290, 'tvi', ario)
def amstats():
	return SDTest.Peek(0x4dc078 + 8, 'i')
def nread(name, n, mem):
	h = nhead(n)
	r = [h, nflags(n)]
	for off in (8, 12, 16, 20, 24, 28):
		r.append(aoff(ari(n + off), mem))
	e = ari(n + 32)
	if e:
		r.append(ari(e))
		for i in range(min(3, ari(e))):
			av = SDTest.Call(0x2ae7b0, 'tiii', n, i)[0]
			w = tpeek(av, 4)
			r.append((w[0], w[1] != 0, w[2], w[3] != 0))
	return r
for name in ('models/vehicles/ships/sfunc1/anm/leftflap.cm3', 'models/doors/elevator/anm/down.cm3',
		'models/chars/berto/anm/berto_morph_eyes.cm3', 'levels/orbit/map/falling/fallinganm/play.cm3',
		'levels/orbit/map/billarrives_a/billarrives_aanm/play.cm3', 'levels/press/map/humancd/humancdanm/case_opencase.cm3',
		'levels/gamblinden/map/meetgambler/meetgambleranm/crazyhead_invisible.cm3',
		'levels/orbit/map/billarrives_c/billarrives_canm/play.cm3'):
	ar = anew(0x34)
	aopen(name, 108)
	b0, m0, c1 = ablocks(), amstats(), acount()
	check('CAnimacion_Read' + fmt((name,)), lambda ar=ar, b0=b0, m0=m0, c1=c1: (SDTest.Call(0x2abca0, 'tvii', ar, ario)[0], aget(ar),
		arb(ari(ar + 0x1c), ari(ar + 0x18)), ablocks() - b0, amstats() - m0, acount() - c1, SDTest.Peek(ario, '@b')))
	aclose()
	mem = ari(ar + 0x28)
	for i in range(ari(ar + 0x24)):
		n = anodo(ar, i)
		check('read nodo' + fmt((i,)), lambda n=n, mem=mem: nread(name, n, mem))
		if i < 12:
			nquery('read' + fmt((i,)), n)
		h = nhead(n)
		if ari(n + 8) and not (h[3] & 1) and i < 6:
			check('read CAnimNodo_Translate' + fmt((i,)), lambda n=n: (SDTest.Call(0x2ade90, 'tbiF', n, vec())[0], afl(ari(n + 8), 3)))
	for i in range(min(ari(ar + 0x18), 50)):
		check('read CAnimacion_GetNodoTabla' + fmt((i,)), lambda ar=ar, i=i: aoff(SDTest.Call(0x2ae280, 'tiii', ar, i)[0], ari(ar + 0x20)))
	check('read CAnimacion_Init free', lambda ar=ar: (SDTest.Call(0x2ae560, 'tviiii', ar, 0, 0, 0)[0], aget(ar)))
	afree(ari(ar + 0x1c))
	check('read freed', lambda b0=b0, c1=c1: (ablocks() - b0, acount() - c1))
	afree(ar)
afree(ario)
