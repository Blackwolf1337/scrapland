# Rest B (game leftovers from 0x240000, execfile'd by sdlevel.py, Python 1.5.2): the visibility test of the models
# of the render list in pass 1 (their last world matrix), and what the level frames drew with the C versions of
# CModelo_Render and the LOD choice (the flags they leave on the models).
def rsbli(a):
	return SDTest.Peek(a, '@i')
def rsblb(a):
	return SDTest.Peek(a, '@i') & 0xff
rsblpass = SDTest.Peek(0x4afdb8, 'i')
rsbln = SDTest.Peek(0x4af7c8, 'i')
check('models in the render list', lambda: rsbln > 0)
SDTest.Poke(0x4afdb8, 1)
rsblk = 0
while rsblk < rsbln and rsblk < 60:
	rsblm = rsbli(SDTest.Peek(0x4ae7c8 + rsblk * 4, 'i'))
	if rsblm:
		check('CModelo_Visibility' + fmt((rsblk, SDTest.Peek(rsbli(rsblm + 4), '@s'))), lambda: (
			SDTest.Call(0x254940, 'tbii', rsblm, rsblm + 0xb0)[0], rsblb(rsblm + 0x120), rsblb(rsblm + 0x121),
			rsbli(rsblm + 0x100)))
	rsblk = rsblk + 1
SDTest.Poke(0x4afdb8, rsblpass)
def rsblfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
SDTest.Poke(0x4afdb8, 1)
rsblk = 0
while rsblk < rsbln and rsblk < 12:
	rsblm = rsbli(SDTest.Peek(0x4ae7c8 + rsblk * 4, 'i'))
	if rsblm:
		check('CModelo_Render' + fmt((rsblk,)), lambda: (SDTest.Call(0x253610, 'tvi', rsblm), rsblb(rsblm + 0x121),
			rsblfl(rsblm + 0xb0, 16)))
		check('CModelo_UpdateLOD' + fmt((rsblk,)), lambda: SDTest.Call(0x2542c0, 'tbi', rsblm)[0])
	rsblk = rsblk + 1
SDTest.Poke(0x4afdb8, rsblpass)

# Stencil_Malla on the device meshes of the first models that have their shadow data: a light set here, the
# distance multiplier raised so the meshes count as near, the counters of the volumes cleared and put back after
def rsblmeshes():
	r = []
	k = 0
	while k < rsbln and len(r) < 3:
		m = rsbli(SDTest.Peek(0x4ae7c8 + k * 4, 'i'))
		k = k + 1
		if not m or not rsbli(m + 0xac):
			continue
		t = rsbli(m + 0xac)
		n = rsbli(t)
		p = rsbli(t + 4)
		j = 0
		while j < n and len(r) < 3:
			nodo = rsbli(p + j * 0xa8 + 0xa4)
			j = j + 1
			if not nodo:
				continue
			o = rsbli(nodo + 0x3c)
			if not o or rsbli(o + 4) != -1593835518:
				continue
			if rsbli(o + 0x3c) and rsbli(o + 0x40) and rsbli(o + 0x44) and rsbli(o + 0xb4) and not (rsbli(rsbli(o + 0xb4) + 0x18) & 0x20):
				r.append(o)
	return r
rsblvars = (0x4afdb0, 0x4afdc0, 0x4afdc4, 0x432578, 0x4afdd0, 0x4afdd8, 0x4afde0, 0x4afde4, 0x43257c, 0x432580,
	0x4afdc8, 0x390030, 0x390034, 0x390038, 0x39006c, 0x390070, 0x390074, 0x39004c)
rsblsave = map(lambda a: SDTest.Peek(a, 'i'), rsblvars)
rsblms = rsblmeshes()
check('stencil meshes', lambda: (len(rsblms), SDTest.Peek(0x4afddc, 'i') != 0, SDTest.Peek(0x4afdcc, 'i') != 0,
	SDTest.Peek(0x4afdd4, 'i') != 0))
def rsblsetf(rva, v):
	SDTest.Call(0x3197d0, 'iiFi', rva + 0x400000, (v,), 4)
def rsblblock(i):
	b = SDTest.Peek(0x4afddc, 'i') + i * 0x80
	return (rsbli(b) != 0, rsblfl(b + 4, 4), rsblb(b + 0x18), rsblb(b + 0x19), rsblfl(b + 0x1c, 6),
		rsblfl(b + 0x34, 16), rsbli(b + 0x74), rsbli(b + 0x7c))
def rsblstate():
	segs = SDTest.Peek(0x4afdd0, 'i')
	tris = SDTest.Peek(0x4afdd8, 'i')
	sp = SDTest.Peek(0x4afdcc, 'i')
	tp = SDTest.Peek(0x4afdd4, 'i')
	f = []
	for j in range(min(48, 0x10000)):
		f.append(SDTest.Peek(0x3902c0 + j, 'b'))
	r = (segs, tris, SDTest.Peek(0x4afde0, 'i'), SDTest.Peek(0x4afde4, 'i'), f)
	if segs:
		r = r + (rsblfl(sp, 6 * min(segs, 4)), rsblfl(sp + (segs - 1) * 0x18, 6))
	if tris:
		r = r + (rsblfl(tp, 9 * min(tris, 3)), rsblfl(tp + (tris - 1) * 0x24, 9))
	return r
if len(rsblms) and SDTest.Peek(0x4afddc, 'i') and SDTest.Peek(0x4afdcc, 'i') and SDTest.Peek(0x4afdd4, 'i'):
	rsblluz = SDTest.Call(0x133650, 'ii', 0xcc)[0]
	for rsblcase in ((0, 1, 1, 0x1388), (1, 0, 1, 0x1388), (1, 1, 0, 0x1388), (1, 1, 1, 0x1388), (1, 1, 1, 0x138a),
			(1, 1, 1, 0x1389)):
		SDTest.Poke(0x4afdd0, 0)
		SDTest.Poke(0x4afdd8, 0)
		SDTest.Poke(0x4afde0, 0)
		SDTest.Poke(0x4afde4, 0)
		SDTest.Poke(0x4afdb0, rsblcase[0])
		SDTest.Poke(0x4afdc0, rsblcase[1])
		SDTest.Poke(0x4afdc4, rsblcase[2] and rsblluz)
		SDTest.Poke(0x43257c, rsblcase[3])
		rsblsetf(0x432578, 1e10)
		rsblsetf(0x432580, 3000.0)
		rsblsetf(0x4afdc8, 0.5)
		SDTest.Call(0x3197d0, 'iiFi', 0x390030 + 0x400000, (pp[0] + 50.0, pp[1] + 900.0, pp[2] - 30.0), 12)
		SDTest.Call(0x3197d0, 'iiFi', 0x39006c + 0x400000, (0.0, -1.0, 0.0), 12)
		for rsblm in rsblms:
			check('Stencil_Malla' + fmt(rsblcase), lambda: (SDTest.Call(0x267530, 'viiiii', 0, 0, 0, rsblm, 0),
				rsblstate()))
		if SDTest.Peek(0x4afde0, 'i'):
			check('Stencil_Malla block', lambda: rsblblock(0))
	SDTest.Call(0x133550, 'vi', rsblluz)
for rsblk in range(len(rsblvars)):
	SDTest.Poke(rsblvars[rsblk], rsblsave[rsblk])
