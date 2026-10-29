# The D3DX texture functions (src/lib/d3dx8/D3dxTex.c, execfile'd by sdcall.py, Python 1.5.2), tested by what they
# must do, not bit for bit: their own C is written from the documented behavior. CheckTextureRequirements and the
# conversions of D3DXLoadSurfaceFromSurface between the uncompressed formats are exact. The DDS files go through
# CTexture_CreateD3D (the call of the game, so the format, levels and filters are the game's) and print the format,
# size and levels exactly, and as a checksum the levels the file has in an uncompressed format or in DXT blocks of
# 4x4 and more. Every other level (DXT converted or encoded, made by the box filter) prints only whether its average
# color is within a tolerance of the average of the file level (of the top level for made levels); made levels of the
# signed formats print only their size. CBmp textures (the path of BMP and TGA) check the upload of every level
# exactly. The dithering of [video] ErrDif is off while it runs. The device calls of one CheckTextureRequirements
# and one texture are compared through the D3D log.
dxtdebug = 0
def dxtz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def dxtfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def dxtpk(a):
	return SDTest.Peek(a, '@i')
def dxtpo(a, v):
	SDTest.Poke(a - 0x400000, v)
# a COM method: sig is the return kind, the object and the arguments
def dxtcom(obj, off, sig, args):
	return apply(SDTest.Call, (dxtpk(dxtpk(obj) + off) - 0x400000, 't' + sig[0] + 'i' + sig[1:], 0, obj) + args)[0]
dxtdev = SDTest.Peek('g_pd3dDevice', 'i')
dxterrdif = SDTest.Peek('g_TexErrDif', 'i')
dxtbuild = SDTest.Peek('g_BuildDDS', 'i')
SDTest.Poke('g_TexErrDif', 0)
SDTest.Poke('g_BuildDDS', 0)
check('dxt device', lambda: dxtdev != 0)

# CheckTextureRequirements: (hr, width, height, levels, format)
def dxtctr(w, h, lv, usage, f, pool):
	return SDTest.Call('D3DX_CheckTextureRequirements', 'tiiipppipi', 0, dxtdev, w, h, lv, usage, f, pool)
for f in (0, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x22, 0x28, 0x29, 0x32, 0x33, 0x34,
		0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x43, 0x31545844, 0x32545844, 0x33545844, 0x34545844, 0x35545844,
		0x59565955, 0x32595559, 0x50, 0x46, 0x20, 99):
	check('dxt CheckTextureRequirements fmt' + fmt((f,)), lambda f=f: dxtctr(64, 32, 0, 0, f, 1))
for w, h in ((0, 0), (1, 1), (3, 5), (100, 60), (812, 812), (1204, 1204), (4096, 4096), (8192, 64), (5000, 3), (2, 0),
		(0, 7), (-1, -1), (-1, 16), (16, -1), (20000, 20000)):
	for lv in (0, 1, 3, 20, -1):
		check('dxt CheckTextureRequirements size' + fmt((w, h, lv)), lambda w=w, h=h, lv=lv: dxtctr(w, h, lv, 0, 0x15, 1))
	check('dxt CheckTextureRequirements size dxt' + fmt((w, h)), lambda w=w, h=h: dxtctr(w, h, 0, 0, 0x31545844, 1))
	check('dxt CheckTextureRequirements size scratch' + fmt((w, h)), lambda w=w, h=h: dxtctr(w, h, 0, 0, 0x35545844, 3))
check('dxt CheckTextureRequirements nulls', lambda: SDTest.Call('D3DX_CheckTextureRequirements', 'tiiiiiiiii', 0, dxtdev, 0, 0, 0, 0, 0, 1))
check('dxt CheckTextureRequirements fmt null', lambda: SDTest.Call('D3DX_CheckTextureRequirements', 'tiiipppiii', 0, dxtdev, 10, 10, 0, 0, 0, 1))
check('dxt CheckTextureRequirements size null', lambda: SDTest.Call('D3DX_CheckTextureRequirements', 'tiiiiipipi', 0, dxtdev, 0, 0, 0, 0, 0x16, 1))
for u in (0, 1, 2, 8, 0x200, 0x20b, 4, 0x10, -1):
	for p in (0, 1, 2, 3, 4):
		check('dxt CheckTextureRequirements usage pool' + fmt((u, p)), lambda u=u, p=p: dxtctr(64, 64, 0, u, 0x15, p))
check('dxt CheckTextureRequirements scratch unknown', lambda: dxtctr(64, 64, 0, 0, 0, 3))
check('dxt CheckTextureRequirements no device', lambda: SDTest.Call('D3DX_CheckTextureRequirements', 'tiiipppipi', 0, 0, 64, 64, 0, 0, 0x15, 1))

# ---- reading the pixels back ----
def dxtblock(f):
	if f == 0x31545844:
		return 8
	if f in (0x32545844, 0x33545844, 0x34545844, 0x35545844):
		return 16
	return 0
def dxtbpp(f):
	if f in (0x15, 0x16, 0x3e, 0x3f):
		return 4
	if f == 0x14:
		return 3
	if f in (0x17, 0x18, 0x19, 0x1a, 0x1d, 0x1e, 0x33, 0x3c, 0x3d):
		return 2
	return 1
# the bytes of n at a
def dxtbytes(a, n):
	r = []
	for i in range(0, n - 3, 4):
		v = dxtpk(a + i)
		r.append(v & 0xff)
		r.append(v >> 8 & 0xff)
		r.append(v >> 16 & 0xff)
		r.append(v >> 24 & 0xff)
	for i in range(n - n % 4, n):
		r.append(SDTest.Peek(a + i, '@b'))
	return r
def dxtrowlen(f, w):
	b = dxtblock(f)
	if b:
		return (w + 3) / 4 * b
	return w * dxtbpp(f)
def dxtrows(f, h):
	if dxtblock(f):
		return (h + 3) / 4
	return h
# the rows of a level as one list of bytes
def dxtlevelbytes(base, pitch, f, w, h):
	r = []
	n = dxtrowlen(f, w)
	for y in range(dxtrows(f, h)):
		r = r + dxtbytes(base + y * pitch, n)
	return r
def dxtsum(b):
	s = 0
	x = 0
	for i in range(0, len(b) - 1, 2):
		s = (s + b[i] + 256 * b[i + 1] + i) % 1000003
		x = x ^ b[i] ^ (b[i + 1] << 8)
	if len(b) % 2:
		s = (s + b[-1]) % 1000003
	return s, x
def dxtx(v, n):
	m = (1 << n) - 1
	return (v * 510 + m) / (2 * m)
def dxts(v, n):
	half = 1 << (n - 1)
	if v >= half:
		v = v - 2 * half
	return ((v + half) * 510 + 2 * half) / (4 * half)
# 565 color as (r, g, b)
def dxt565(c):
	return dxtx(c >> 11 & 31, 5), dxtx(c >> 5 & 63, 6), dxtx(c & 31, 5)
def dxtmix(a, b, wa, wb):
	return (a * wa + b * wb + (wa + wb) / 2) / (wa + wb)
# sums of r g b a over a level, and the pixel count
def dxtavg(b, f, w, h):
	s = [0.0, 0.0, 0.0, 0.0]
	bs = dxtblock(f)
	if bs:
		bw = (w + 3) / 4
		for by in range((h + 3) / 4):
			for bx in range(bw):
				o = (by * bw + bx) * bs
				ao = o
				if bs == 16:
					o = o + 8
				c0 = b[o] | b[o + 1] << 8
				c1 = b[o + 2] | b[o + 3] << 8
				p0 = dxt565(c0)
				p1 = dxt565(c1)
				if c0 > c1 or f != 0x31545844:
					pal = (p0 + (255,), p1 + (255,), (dxtmix(p0[0], p1[0], 2, 1), dxtmix(p0[1], p1[1], 2, 1), dxtmix(p0[2], p1[2], 2, 1), 255),
						(dxtmix(p0[0], p1[0], 1, 2), dxtmix(p0[1], p1[1], 1, 2), dxtmix(p0[2], p1[2], 1, 2), 255))
				else:
					pal = (p0 + (255,), p1 + (255,), (dxtmix(p0[0], p1[0], 1, 1), dxtmix(p0[1], p1[1], 1, 1), dxtmix(p0[2], p1[2], 1, 1), 255), (0, 0, 0, 0))
				if f == 0x35545844 or f == 0x34545844:
					a0 = b[ao]
					a1 = b[ao + 1]
					apal = [a0, a1]
					if a0 > a1:
						for i in range(1, 7):
							apal.append(dxtmix(a0, a1, 7 - i, i))
					else:
						for i in range(1, 5):
							apal.append(dxtmix(a0, a1, 5 - i, i))
						apal = apal + [0, 255]
					abits = []
					for i in range(6):
						v = b[ao + 2 + i]
						for k in range(8):
							abits.append(v >> k & 1)
				for y in range(4):
					if by * 4 + y >= h:
						break
					row = b[o + 4 + y]
					for x in range(4):
						if bx * 4 + x >= w:
							break
						p = pal[row >> (2 * x) & 3]
						s[0] = s[0] + p[0]
						s[1] = s[1] + p[1]
						s[2] = s[2] + p[2]
						i = y * 4 + x
						if f == 0x33545844 or f == 0x32545844:
							s[3] = s[3] + (b[ao + i / 2] >> (4 * (i % 2)) & 15) * 17
						elif f == 0x35545844 or f == 0x34545844:
							s[3] = s[3] + apal[abits[3 * i] | abits[3 * i + 1] << 1 | abits[3 * i + 2] << 2]
						else:
							s[3] = s[3] + p[3]
		return s, w * h
	n = dxtrowlen(f, w)
	k = dxtbpp(f)
	for y in range(h):
		for x in range(w):
			o = y * n + x * k
			if k == 4:
				v = b[o] | b[o + 1] << 8 | b[o + 2] << 16
				va = b[o + 3]
			elif k == 3:
				v = b[o] | b[o + 1] << 8 | b[o + 2] << 16
			else:
				v = b[o]
				if k == 2:
					v = v | b[o + 1] << 8
			if f == 0x15:
				p = (v >> 16 & 0xff, v >> 8 & 0xff, v & 0xff, va)
			elif f == 0x16 or f == 0x14:
				p = (v >> 16 & 0xff, v >> 8 & 0xff, v & 0xff, 255)
			elif f == 0x17:
				p = dxt565(v) + (255,)
			elif f == 0x18 or f == 0x19:
				a = 255
				if f == 0x19:
					a = (v >> 15) * 255
				p = (dxtx(v >> 10 & 31, 5), dxtx(v >> 5 & 31, 5), dxtx(v & 31, 5), a)
			elif f == 0x1a:
				p = (dxtx(v >> 8 & 15, 4), dxtx(v >> 4 & 15, 4), dxtx(v & 15, 4), dxtx(v >> 12 & 15, 4))
			elif f == 0x3c:
				p = (dxts(v & 0xff, 8), dxts(v >> 8 & 0xff, 8), 128, 255)
			elif f == 0x3d:
				p = (dxts(v & 31, 5), dxts(v >> 5 & 31, 5), 128, dxtx(v >> 10 & 63, 6))
			elif f == 0x3e:
				p = (dxts(v & 0xff, 8), dxts(v >> 8 & 0xff, 8), 128, v >> 16 & 0xff)
			elif f == 0x3f:
				p = (dxts(v & 0xff, 8), dxts(v >> 8 & 0xff, 8), dxts(v >> 16 & 0xff, 8), dxts(va, 8))
			else:
				return None
			for i in range(4):
				s[i] = s[i] + p[i]
	return s, w * h
# the largest channel difference of two averages
def dxtdiff(a, b):
	d = 0.0
	for i in range(4):
		e = abs(a[0][i] / a[1] - b[0][i] / b[1])
		if e > d:
			d = e
	return d

# the levels of a DDS file: (format, width, height, bytes)
def dxtfile(buf, size):
	h, w, mips = dxtpk(buf + 12), dxtpk(buf + 16), dxtpk(buf + 28)
	if dxtpk(buf + 80) & 4:
		f = dxtpk(buf + 84)
	elif dxtpk(buf + 88) == 32:
		f = 0x15
	elif dxtpk(buf + 92) == 0xf800:
		f = 0x17
	elif dxtpk(buf + 92) == 0x7c00:
		f = 0x18
	else:
		f = 0x1a
	if mips < 1:
		mips = 1
	r = []
	off = 128
	while len(r) < mips:
		n = dxtrowlen(f, w) * dxtrows(f, h)
		if off + n > size:
			break
		r.append((f, w, h, dxtlevelbytes(buf + off, dxtrowlen(f, w), f, w, h)))
		off = off + n
		if w == 1 and h == 1:
			break
		w = max(1, w / 2)
		h = max(1, h / 2)
	return r
# the levels of a texture: (format, width, height, bytes)
def dxttex(tex):
	n = dxtcom(tex, 0x34, 'ii', ())
	desc = dxtz(32)
	lr = dxtz(8)
	r = []
	for lv in range(n):
		dxtcom(tex, 0x38, 'iiii', (lv, desc))
		f, w, h = dxtpk(desc), dxtpk(desc + 0x18), dxtpk(desc + 0x1c)
		b = None
		if dxtcom(tex, 0x40, 'iiiiii', (lv, lr, 0, 0x10)) >= 0:
			b = dxtlevelbytes(dxtpk(lr + 4), dxtpk(lr), f, w, h)
			dxtcom(tex, 0x44, 'iii', (lv,))
		r.append((f, w, h, b))
	dxtfree(desc)
	dxtfree(lr)
	return r
# per level the size, and the checksum where it must be exact, else whether the average is close
def dxtcompare(levels, file):
	r = []
	ref0 = dxtavg(file[0][3], file[0][0], file[0][1], file[0][2])
	for i in range(len(levels)):
		f, w, h, b = levels[i]
		if b is None:
			r.append((f, w, h, 'not lockable'))
			continue
		same = i < len(file) and file[i][1] == w and file[i][2] == h
		raw = not dxtblock(f) and not dxtblock(file[0][0])
		if same and (f == file[i][0] and (not dxtblock(f) or (w >= 4 and h >= 4)) or raw):
			r.append((f, w, h, dxtsum(b)))
			continue
		if not same and f in (0x3c, 0x3d, 0x3e, 0x3f):
			# D3DX makes the signed levels from the level above, rounding again each time (an alpha of 255 has
			# wrapped to the lowest Q): only the size is compared
			r.append((f, w, h))
			continue
		a = dxtavg(b, f, w, h)
		ref = ref0
		if same:
			ref = dxtavg(file[i][3], file[i][0], w, h)
		if a is None or ref is None:
			r.append((f, w, h, 'no average'))
			continue
		d = dxtdiff(a, ref)
		if dxtdebug:
			r.append((f, w, h, int(d * 10) / 10.0))
		else:
			# made levels of 4 bit channels drift most (D3DX: up to 16 at 1x1)
			tol = 3
			if not same and f == 0x1a:
				tol = 24
			elif not same or dxtblock(f) or dxtblock(file[0][0]):
				tol = 12
			r.append((f, w, h, d <= tol))
	return r

dxtfiles = ('levels/fake/map/dds/down.000.dds', 'models/weapons/sonic/dds/minasonica-r.dds',
	'2d/textures/commercial/dds/templo-transicion00-d.dds', 'levels/fake/map/dds/downtransicion18l-d.alpha.dds',
	'2d/textures/downtown/dds/downluzborde01-d.alpha.dds', '2d/gfx/dds/characterdazed.alpha.dds',
	'bmp/dds/noise_static.alpha.dds', '2d/textures/temple/dds/templo-transicion00-d.alpha.dds',
	'2d/gfx/dds/circletrail.dds', '2d/textures/menu/dds/escudo-r.dds', '2d/menu/dds/circuitflare.dds',
	'2d/dds/arrow.alpha.dds', '2d/dds/marker.alpha.dds', 'bmp/dds/partic1.dds')
# CTexture_CreateD3D with the file in memory: bpp, alpha, bump, compressed, mipmap
dxtreqs = ((32, 0, 0, 1, 1), (32, 0, 0, 1, 0), (32, 1, 0, 0, 1), (32, 0, 0, 0, 1), (24, 0, 0, 0, 0), (16, 0, 0, 0, 1),
	(16, 1, 0, 0, 1), (8, 0, 0, 0, 0), (32, 0, 1, 0, 1), (32, 1, 1, 0, 0), (16, 0, 1, 0, 1), (16, 1, 1, 0, 1),
	(16, 0, 1, 1, 1))
def dxtentry(name):
	t = SDTest.Call('operator_new', 'ii', 0x258)[0]
	SDTest.Call('CTexture_ctor', 'tiizzii', t, name, None, 0, 0)
	return t
def dxtfreeentry(t):
	SDTest.Call('CTexture_dtor', 'tvi', t)
	SDTest.Call('operator_delete', 'vi', t)
def dxtmake(t, req, buf, size, dev=None):
	if dev is None:
		dev = dxtdev
	bpp, alpha, bump, comp, mip = req
	dxtpo(t + 0x21c, bpp)
	dxtpo(t + 0x224, alpha)
	dxtpo(t + 0x23c, mip | bump << 8)
	dxtpo(t + 0x240, comp)
	dxtpo(t + 0x230, buf)
	dxtpo(t + 0x234, size)
	hr = SDTest.Call('CTexture_CreateD3D', 'tiii', t, dev)[0]
	dxtpo(t + 0x230, 0)
	return hr
for dxtname in dxtfiles:
	r = SDTest.Call('FileIO_ReadAll', 'ispp', dxtname, 0, 0)
	dxtbuf, dxtsize = r[1], r[2]
	if not dxtbuf:
		check('dxt file ' + dxtname, lambda: 'missing')
		continue
	dxtlv = dxtfile(dxtbuf, dxtsize)
	check('dxt file ' + dxtname, lambda dxtlv=dxtlv: len(dxtlv))
	for dxtreq in dxtreqs:
		t = dxtentry('sdxtex.dds')
		hr = dxtmake(t, dxtreq, dxtbuf, dxtsize)
		tex = dxtpk(t + 0x228)
		if tex:
			check('dxt CTexture_CreateD3D ' + dxtname + fmt(dxtreq), lambda hr=hr, tex=tex, dxtlv=dxtlv: (hr,
				dxtcompare(dxttex(tex), dxtlv)))
		else:
			check('dxt CTexture_CreateD3D ' + dxtname + fmt(dxtreq), lambda hr=hr: (hr, 'none'))
		dxtfreeentry(t)
	if dxtname == 'bmp/dds/partic1.dds':
		# the header alone and a cut level
		for n in (128, 128 + 1000):
			t = dxtentry('sdxtex.dds')
			check('dxt CTexture_CreateD3D cut' + fmt((n,)), lambda t=t, n=n: (dxtmake(t, (32, 1, 0, 0, 1), dxtbuf, n) < 0,
				dxtpk(t + 0x228) != 0))
			dxtfreeentry(t)
	SDTest.Call('Ptr_Delete', 'vp', dxtbuf)

# the TGA the game loads through D3DX (bmp/norm1.alpha.psa, 1024 x 32, 32 bits, rows bottom up): the top level as a
# checksum, the made ones by their average against it
def dxttga(levels):
	r = []
	f0, w0, h0, b0 = levels[0]
	ref = dxtavg(b0, f0, w0, h0)
	for f, w, h, b in levels:
		if b is None:
			r.append((f, w, h, 'not lockable'))
		elif f == f0 and w == w0:
			r.append((f, w, h, dxtsum(b)))
		else:
			r.append((f, w, h, dxtdiff(dxtavg(b, f, w, h), ref) <= 12))
	return r
r = SDTest.Call('FileIO_ReadAll', 'ispp', 'bmp/norm1.alpha.psa', 0, 0)
dxtbuf, dxtsize = r[1], r[2]
check('dxt file bmp/norm1.alpha.psa', lambda: (dxtbuf != 0, dxtsize))
if dxtbuf:
	for dxtreq in ((32, 1, 0, 0, 1), (32, 0, 0, 0, 0), (32, 1, 0, 1, 0)):
		t = dxtentry('sdxtex.psa')
		hr = dxtmake(t, dxtreq, dxtbuf, dxtsize)
		tex = dxtpk(t + 0x228)
		if tex:
			check('dxt CTexture_CreateD3D tga' + fmt(dxtreq), lambda hr=hr, tex=tex: (hr, dxttga(dxttex(tex))))
		else:
			check('dxt CTexture_CreateD3D tga' + fmt(dxtreq), lambda hr=hr: (hr, 'none'))
		dxtfreeentry(t)
	SDTest.Call('Ptr_Delete', 'vp', dxtbuf)

# ---- LoadSurfaceFromSurface ----
dxtpix = (0x00000000, 0xffffffff, 0x80808080, 0x7f7f7f7f, 0x12345678, 0xff0000ff, 0x00ff00ff, 0x0000ffff, 0x01020304,
	0xfefdfcfb, 0x08070605, 0xf0e0d0c0, 0x3c3c3c3c, 0xc3c3c3c3, 0x40404040, 0xb8b8b8b8, 0x07070707, 0x0f0f0f0f,
	0x10101010, 0x84848484, 0x7b7b7b7b, 0x04040404, 0x03030303, 0xfcfcfcfc, 0x88888888, 0x77777777, 0x44444444,
	0xbbbbbbbb, 0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00)
def dxtsurf(w, h, f):
	p = dxtz(4)
	hr = dxtcom(dxtdev, 0x6c, 'iiiiii', (w, h, f, p))
	s = dxtpk(p)
	dxtfree(p)
	if hr < 0:
		return 0
	return s
def dxtsurfbytes(s, w, h, f):
	lr = dxtz(8)
	if dxtcom(s, 0x24, 'iiiii', (lr, 0, 0x10)) < 0:
		dxtfree(lr)
		return None
	r = dxtlevelbytes(dxtpk(lr + 4), dxtpk(lr), f, w, h)
	dxtcom(s, 0x28, 'ii', ())
	dxtfree(lr)
	return r
def dxtfill(s, w, h, pix):
	lr = dxtz(8)
	dxtcom(s, 0x24, 'iiiii', (lr, 0, 0))
	base, pitch = dxtpk(lr + 4), dxtpk(lr)
	k = 0
	for y in range(h):
		for x in range(w):
			dxtpo(base + y * pitch + 4 * x, pix[k % len(pix)])
			k = k + 1
	dxtcom(s, 0x28, 'ii', ())
	dxtfree(lr)
def dxtrelease(s):
	dxtcom(s, 8, 'ii', ())
def dxtlsfs(d, s, flt, ck):
	return SDTest.Call('D3DX_LoadSurfaceFromSurface', 'tiiiiiiiiii', 0, d, 0, 0, s, 0, 0, flt, ck)[0]
dxtsrc = dxtsurf(8, 4, 0x15)
dxtfill(dxtsrc, 8, 4, dxtpix)
# a smooth one for the DXT targets
dxtgrad = []
for y in range(4):
	for x in range(8):
		dxtgrad.append((0x10 + 12 * x) << 24 | (0x20 + 16 * x) << 16 | (0x30 + 8 * y) << 8 | (0xa0 - 8 * x))
dxtgsrc = dxtsurf(8, 4, 0x15)
dxtfill(dxtgsrc, 8, 4, dxtgrad)
dxtgavg = dxtavg(dxtsurfbytes(dxtgsrc, 8, 4, 0x15), 0x15, 8, 4)
check('dxt LoadSurfaceFromSurface source', lambda: dxtsurfbytes(dxtsrc, 8, 4, 0x15) == dxtsurfbytes(dxtsrc, 8, 4, 0x15))
for f in (0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x32, 0x33, 0x34, 0x3c, 0x3d, 0x3e, 0x3f):
	for flt in (2, 1, 5):
		d = dxtsurf(8, 4, f)
		if not d:
			check('dxt LoadSurfaceFromSurface' + fmt((f, flt)), lambda: 'no surface')
			continue
		check('dxt LoadSurfaceFromSurface' + fmt((f, flt)), lambda d=d, f=f, flt=flt: (dxtlsfs(d, dxtsrc, flt, 0),
			dxtsurfbytes(d, 8, 4, f)))
		dxtrelease(d)
for f in (0x31545844, 0x33545844, 0x35545844):
	d = dxtsurf(8, 4, f)
	check('dxt LoadSurfaceFromSurface dxt' + fmt((f,)), lambda d=d, f=f: (dxtlsfs(d, dxtgsrc, 2, 0),
		dxtdiff(dxtavg(dxtsurfbytes(d, 8, 4, f), f, 8, 4), dxtgavg) <= 10))
	dxtrelease(d)
for w, h in ((4, 2), (16, 8), (3, 3)):
	d = dxtsurf(w, h, 0x15)
	check('dxt LoadSurfaceFromSurface point' + fmt((w, h)), lambda d=d, w=w, h=h: (dxtlsfs(d, dxtsrc, 2, 0),
		dxtsurfbytes(d, w, h, 0x15)))
	for flt in (3, 4, 5):
		check('dxt LoadSurfaceFromSurface scaled' + fmt((w, h, flt)), lambda d=d, w=w, h=h, flt=flt: (dxtlsfs(d,
			dxtgsrc, flt, 0), dxtdiff(dxtavg(dxtsurfbytes(d, w, h, 0x15), 0x15, w, h), dxtgavg) <= 12))
	dxtrelease(d)
for ck in (0xff0000ff, 0x12345678, 0x80808080):
	d = dxtsurf(8, 4, 0x15)
	check('dxt LoadSurfaceFromSurface color key' + fmt((ck,)), lambda d=d, ck=ck: (dxtlsfs(d, dxtsrc, 2, ck),
		dxtsurfbytes(d, 8, 4, 0x15)))
	dxtrelease(d)
for f in (0x16, 0x17, 0x18, 0x19, 0x1a, 0x3c, 0x3d, 0x3e, 0x3f):
	s2 = dxtsurf(8, 4, f)
	if not s2:
		continue
	dxtlsfs(s2, dxtsrc, 2, 0)
	d = dxtsurf(8, 4, 0x15)
	check('dxt LoadSurfaceFromSurface back' + fmt((f,)), lambda d=d, s2=s2: (dxtlsfs(d, s2, 2, 0),
		dxtsurfbytes(d, 8, 4, 0x15)))
	dxtrelease(d)
	dxtrelease(s2)
# every 5 and 6 bit value of R5G6B5 into the formats of the game
dxt565 = dxtsurf(64, 1, 0x17)
dxtlr = dxtz(8)
dxtcom(dxt565, 0x24, 'iiiii', (dxtlr, 0, 0))
for i in range(0, 64, 2):
	v = long((i & 31) << 11 | i << 5 | (31 - (i & 31))) + long((i + 1 & 31) << 11 | (i + 1) << 5 | (31 - (i + 1 & 31))) * 65536L
	if v >= 2147483648L:
		v = v - 4294967296L
	dxtpo(dxtpk(dxtlr + 4) + 2 * i, int(v))
dxtcom(dxt565, 0x28, 'ii', ())
dxtfree(dxtlr)
for f in (0x15, 0x16, 0x18, 0x1a, 0x3c, 0x3d, 0x3e, 0x3f):
	d = dxtsurf(64, 1, f)
	check('dxt LoadSurfaceFromSurface R5G6B5' + fmt((f,)), lambda d=d, f=f: (dxtlsfs(d, dxt565, 2, 0), dxtsurfbytes(d, 64, 1, f)))
	dxtrelease(d)
dxtrelease(dxt565)
check('dxt LoadSurfaceFromSurface no target', lambda: dxtlsfs(0, dxtsrc, 2, 0) < 0)
check('dxt LoadSurfaceFromSurface no source', lambda: dxtlsfs(dxtsrc, 0, 2, 0) < 0)
dxtrelease(dxtsrc)
dxtrelease(dxtgsrc)

# CBmp textures: the levels from CBmp_Resample, uploaded by LoadSurfaceFromSurface (POINT)
dxtbmpreqs = ((32, 1, 0, 1), (32, 0, 0, 1), (24, 0, 0, 1), (16, 0, 0, 1), (16, 1, 0, 1), (32, 0, 1, 1), (32, 1, 1, 1),
	(16, 0, 1, 1), (16, 1, 1, 1), (16, 1, 0, 0))
for dxtreq in dxtbmpreqs:
	t = dxtentry('sdxbmp.bmp')
	bm = SDTest.Call('operator_new', 'ii', 12)[0]
	SDTest.Call('CBmp_ctor', 'tiiii', bm, 16, 8)
	img = dxtpk(bm + 8)
	for i in range(16 * 8):
		dxtpo(img + 4 * i, dxtpix[i % len(dxtpix)] ^ (i * 0x010305))
	bpp, alpha, bump, mip = dxtreq
	dxtpo(t + 0x22c, bm)
	dxtpo(t + 0x210, 16)
	dxtpo(t + 0x214, 8)
	dxtpo(t + 0x21c, bpp)
	dxtpo(t + 0x224, alpha)
	dxtpo(t + 0x23c, mip | bump << 8)
	hr = SDTest.Call('CTexture_CreateD3D', 'tiii', t, dxtdev)[0]
	tex = dxtpk(t + 0x228)
	def dxtbmplevels(tex):
		r = []
		for f, w, h, b in dxttex(tex):
			if b is None:
				r.append((f, w, h, None))
			else:
				r.append((f, w, h, dxtsum(b)))
		return r
	if tex:
		check('dxt CTexture_CreateD3D CBmp' + fmt(dxtreq), lambda hr=hr, tex=tex: (hr, dxtbmplevels(tex)))
	else:
		check('dxt CTexture_CreateD3D CBmp' + fmt(dxtreq), lambda hr=hr: (hr, 'none'))
	dxtfreeentry(t)

# the device calls, through the D3D log
def dxtlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return r, SDTest.Out()
check('dxt CheckTextureRequirements log', lambda: dxtlog(lambda: SDTest.Call('D3DX_CheckTextureRequirements', 'tiiipppipi', 0,
	SDTest.Peek('g_pd3dDevice', 'i'), 30, 20, 0, 0, 0x14, 1)))
r = SDTest.Call('FileIO_ReadAll', 'ispp', 'bmp/dds/partic1.dds', 0, 0)
dxtbuf, dxtsize = r[1], r[2]
if dxtbuf:
	t = dxtentry('sdxtex.dds')
	check('dxt CTexture_CreateD3D log', lambda t=t: dxtlog(lambda t=t: (dxtmake(t, (16, 0, 0, 0, 1), dxtbuf, dxtsize, SDTest.Peek('g_pd3dDevice', 'i')),)))
	dxtfreeentry(t)
	SDTest.Call('Ptr_Delete', 'vp', dxtbuf)

SDTest.Poke('g_TexErrDif', dxterrdif)
SDTest.Poke('g_BuildDDS', dxtbuild)
