# engine3d Engine/Modelo.c and UtilEscena.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): models
# made by the constructor of the exe in heap memory, with fake scenes, nodes and animations around them. No model is
# loaded and nothing stays in the lists of the engine: the list heads and gModelosRender are swapped for the checks
# that need them and put back. Fake scenes that may lose their last reference get a vftable whose deleting
# destructor is CEscena_ReleaseD3D (rva 0x24ec50, thiscall with one argument): a no-op for a scene without nodes.
# Not called: crear_modelo, crear_escena and crear_animacion (they load files), CModelo_AddAnim (makes an animation
# from a scene file), CModelo_GetSector (the sector code), CModelo_BuildNodosR (the render nodes need the nodes of a
# real scene), the CHECK(0) functions and the animation players (CModelo.animate is 0 in the checks).
def moz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def mofree(p):
	SDTest.Call(0x133550, 'vi', p)
def mopoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def mopeek(addr):
	return SDTest.Peek(addr, '@i')
def mobyte(addr):
	return SDTest.Peek(addr, '@b')
# 4 bytes at addr, the last one below 0x80
def mow(addr, b0, b1, b2, b3):
	mopoke(addr, b0 + (b1 << 8) + (b2 << 16) + (b3 << 24))
def mofl(addr, fl):
	SDTest.Call(0x3197d0, 'iiFi', addr, fl, 4 * len(fl))
def mofget(addr, n):
	z = []
	for i in range(n):
		z.append(0.0)
	return SDTest.Call(0x3197d0, 'iFii', tuple(z), addr, 4 * n)[1]
def mostr(s):
	return SDTest.Call(0x22cfa0, 'is', s)[0]
def mogf(rva, f):
	mofl(rva + 0x400000, (f,))
mobjs = []
def mowho(p):
	if not p:
		return 'NULL'
	for i in range(len(mobjs)):
		if mobjs[i] == p:
			return i
	return 'other'
def moadd(p):
	mobjs.append(p)
	return p
# the members of a model; the name, the pointers as indices into mobjs, the 16 byte slots without their padding
def moget(m):
	b = []
	for i in range(8):
		b.append(mobyte(m + 0x9c + i))
	g = []
	for i in range(0x2c):
		g.append(mowho(mopeek(m + 0x12c + 4 * i)))
	return (SDTest.Peek(m + 4, '@p'), mofget(m + 8, 37), b, mopeek(m + 0xa4), mopeek(m + 0xa8) != 0,
		mowho(mopeek(m + 0xac)), mofget(m + 0xb0, 16), mowho(mopeek(m + 0xf0)), mopeek(m + 0xf4), mopeek(m + 0xf8),
		mowho(mopeek(m + 0xfc)), mopeek(m + 0x100), mopeek(m + 0x104) != 0, mofget(m + 0x108, 1), mopeek(m + 0x10c),
		mopeek(m + 0x110), mofget(m + 0x114, 3), (mobyte(m + 0x120), mobyte(m + 0x121), mobyte(m + 0x122),
		mobyte(m + 0x123)), mowho(mopeek(m + 0x124)), mowho(mopeek(m + 0x128)), g, tpeek(m + 0x1dc, 5))
def moslot(m, i):
	return mopeek(m + 0xa8) + 16 * i
def moslots(m):
	r = []
	for i in range(mopeek(m + 0xa4)):
		e = moslot(m, i)
		r.append((mowho(mopeek(e)), mowho(mopeek(e + 4)), mofget(e + 8, 1)[0], mobyte(e + 0xc), mobyte(e + 0xd)))
	return r
def moset(m, i, anim, prev, blend):
	e = moslot(m, i)
	mopoke(e, anim)
	mopoke(e + 4, prev)
	mofl(e + 8, (blend,))
# the members of an animation from 0x4 on without the scene pointer; 0x80 is a time the player copies at the start
def moaget(a):
	r = tpeek(a + 4, 0x23)
	r[0x1f] = 0
	return (SDTest.Peek(a, '@p'), r)
def mostep(name, m, rva, sig, *args):
	check(name + fmt(args), lambda m=m, rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig, m) + args)[1:],
		moget(m)))
def moescena(name, refs):
	e = moadd(moz(0x64))
	mopoke(e + 4, mostr(name))
	mopoke(e + 8, refs)
	return e
def moanimm3d(name, morph):
	am = moadd(moz(0x28))
	mopoke(am, mostr(name))
	mopoke(am + 0x14, morph)
	return am
def moanim(name, speed, frame, state, esc):
	a = moadd(moz(0x98))
	mopoke(a, mostr(name))
	mofl(a + 0x14, (speed,))
	mofl(a + 0x58, (frame,))
	mopoke(a + 0x5c, state)
	mopoke(a + 0x94, esc)
	return a
def monode(name, clase, flags, pos, rot):
	n = moadd(moz(0x50))
	mopoke(n + 4, mostr(name))
	mopoke(n + 8, 5)
	if clase:
		o = moz(0xcc)
		mopoke(o + 4, clase)
		mopoke(n + 0x3c, o)
	mopoke(n + 0x18, flags)
	mofl(n + 0x1c, pos + rot)
	return n
def molink(nodes, off):
	for i in range(len(nodes) - 1):
		mopoke(nodes[i] + off, nodes[i + 1])
	mopoke(nodes[-1] + off, 0)
def mobox(e, box):
	mofl(e + 0x24, box)
def morand(lo, hi):
	return int(rnd(lo + 0.0, hi + 0.99))

# engine variables and lists of the engine changed here, put back at the end
mog = (0x4ae638, 0x4ae770, 0x4a8ed4, 0x4ae778, 0x4ae77c, 0x4af7c8, 0x4af7cc)
mosaved = []
for v in mog:
	mosaved.append(SDTest.Peek(v, 'i'))
def morestore():
	for i in range(len(mog)):
		SDTest.Poke(mog[i], mosaved[i])
SDTest.Poke(0x4ae638, 0)
mogf(0x4ae770, 1.0)
mogf(0x4a8ed4, 0.04)
SDTest.Call(0x323c50, 'vi', 4242)
novt = moz(4)
mopoke(novt, 0x400000 + 0x24ec50)

# the constructor and the simple members
m1 = moadd(moz(0x1f0))
check('CModelo_ctor', lambda: (SDTest.Call(0x252a80, 'tii', m1)[0] == m1, moget(m1), moslots(m1)))
mostep('CModelo_SetCallback', m1, 0x255d30, 'tviiii', 0, 7, 9)
e1 = moescena('e1', 10)
for i in range(8):
	v = vec()
	mostep('CModelo_SetPivot', m1, 0x255d60, 'tviFii', v, 0, 0)
	a, b = vec(), vec()
	mobox(e1, (min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2]), max(a[0], b[0]), max(a[1], b[1]), max(a[2], b[2])))
	mopoke(m1 + 0x124, e1)
	mostep('CModelo_SetPivot rel', m1, 0x255d60, 'tviFii', (rnd(0.0, 1.0), rnd(0.0, 1.0), rnd(0.0, 1.0)), 1, 0)
	mofl(m1 + 0x14, vec())
	mostep('CModelo_MoveForward', m1, 0x254f50, 'tvif', rnd())
for i in range(8):
	mofl(m1 + 8, vec() + vec() + (rnd(0.5, 2.0), rnd(0.5, 2.0), rnd(0.5, 2.0)) + vec() + vec() + vec())
	mofl(m1 + 0x5c, affine())
	check('CModelo_GetMatrix' + fmt((i,)), lambda: SDTest.Call(0x255620, 'tviF', m1, ident)[1])
mofl(m1 + 0x5c, ident)

# slots and animations
am2 = moanimm3d('anim2.m3d', 0)
e2 = moescena('e2', 10)
mopoke(e2 + 0x3c, am2)
am3 = moanimm3d('anim3.m3d', 1)
e3 = moescena('e3', 10)
mopoke(e3 + 0x3c, am3)
a1 = moanim('Walk', 1.0, 3.25, 0, e2)
a2 = moanim('Run', 0.0, 7.5, 1, e2)
a3 = moanim('Talk', 0.5, 0.75, 2, e3)
a4 = moanim('Idle', 2.0, 11.0, 3, e2)
mofl(a1 + 0x28, vec())
mofl(a2 + 0x28, vec())
mofl(a1 + 0x1c, vec())
for s, a in ((0, a1), (1, a3), (2, 0), (3, a4)):
	mostep('CModelo_SetSlotAnim', m1, 0x255ee0, 'tviii', s, a)
check('CModelo slots', lambda: moslots(m1))
for s in range(4):
	check('CModelo_GetSlot' + fmt((s,)), lambda s=s: SDTest.Call(0x253110, 'tiii', m1, s)[0] - mopeek(m1 + 0xa8))
	check('CModelo_GetSlotAnim' + fmt((s,)), lambda s=s: mowho(SDTest.Call(0x2564f0, 'tiii', m1, s)[0]))
	check('CModelo_GetSlotFrame' + fmt((s,)), lambda s=s: SDTest.Call(0x256540, 'tfii', m1, s)[0])
	check('CModelo_GetSlotAnimM3D' + fmt((s,)), lambda s=s: mowho(SDTest.Call(0x256420, 'tiii', m1, s)[0]))
	check('CModelo_AnimState' + fmt((s,)), lambda s=s: SDTest.Call(0x254dd0, 'tiii', m1, s)[0])
def mostates():
	return (mopeek(a1 + 0x5c), mopeek(a3 + 0x5c), mopeek(a4 + 0x5c), mofget(a3 + 0x6c, 2))
for rva, name in ((0x254d70, 'CModelo_AnimPlay'), (0x254e30, 'CModelo_AnimStop'), (0x254e90, 'CModelo_AnimPause')):
	for s in range(4):
		check(name + fmt((s,)), lambda rva=rva, s=s: (SDTest.Call(rva, 'tvii', m1, s), mostates()))
	mopoke(a3 + 0x5c, 2)
for s in range(4):
	check('CModelo_AnimFade' + fmt((s,)), lambda s=s: (SDTest.Call(0x254ef0, 'tvifi', m1, 2.5, s), mostates()))
	mopoke(a1 + 0x5c, 2)
	check('CModelo_AnimFade 2' + fmt((s,)), lambda s=s: (SDTest.Call(0x254ef0, 'tvifi', m1, 0.0, s), mostates()))
for a in (a1, a2, a3, a4):
	check('CAnim_GetSpeed', lambda a=a: SDTest.Call(0x253160, 'fi', a)[0])
check('CAnim_GetSpeed NULL', lambda: SDTest.Call(0x253160, 'fi', 0)[0])
SDTest.Poke(0x4ae638, 1)
check('CAnim_GetSpeed paused', lambda: SDTest.Call(0x253160, 'fi', a1)[0])
SDTest.Poke(0x4ae638, 0)
mogf(0x4ae770, 1.5)
check('CAnim_GetSpeed mult', lambda: SDTest.Call(0x253160, 'fi', a3)[0])
mogf(0x4ae770, 1.0)

# the animation group
for i in range(4):
	mopoke(m1 + 0x12c + 4 * i, (a1, a2, a3, a4)[i])
for i in (0, 1, 3, 4, 43, 44, -1):
	check('CModelo_GetAnim' + fmt((i,)), lambda i=i: mowho(SDTest.Call(0x256480, 'tiii', m1, i)[0]))
	check('CModelo_GetAnimName' + fmt((i,)), lambda i=i: SDTest.Call(0x2564c0, 'tsii', m1, i)[0])
for n in ('Walk', 'walk', 'TALK', 'Idle', 'Jump', ''):
	check('CModelo_FindAnim' + fmt((n,)), lambda n=n: SDTest.Call(0x256490, 'tiis', m1, n)[0])
def moevents(a):
	r = []
	ev = mopeek(a + 4)
	evs = []
	while ev:
		evs.append(ev)
		ev = mopeek(ev + 8)
	for ev in evs:
		arg = mopeek(ev + 4)
		k = None
		for j in range(len(evs)):
			if evs[j] == arg:
				k = j
		r.append((mopeek(ev), k, arg == 0, mofget(ev + 0xc, 1)[0], mopeek(ev + 0x10)))
	return (r, evs)
mopoke(a1 + 0x3c, 4)
mopoke(a1 + 0x40, 20)
mopoke(a4 + 0x3c, 0)
mopoke(a4 + 0x40, 1)
for n, s, e in (('Walk', -1.0, -1.0), ('walk', 2.5, 7.0), ('Idle', -0.5, 3.0), ('Jump', 1.0, 2.0), ('Walk', 9.0, -2.0)):
	check('CModelo_AddAnimEvents' + fmt((n, s, e)), lambda n=n, s=s, e=e: SDTest.Call(0x254c50, 'tbisff', m1, n, s, e)[0])
for a in (a1, a4):
	r, evs = moevents(a)
	check('CModelo_AddAnimEvents events', lambda r=r: r)
	for ev in evs:
		mofree(ev)
	mopoke(a + 4, 0)

# CModelo_SetAnim: blending from the old animation, the player started
def mosetanim(name, idx, slot):
	check(name + fmt((idx, slot)), lambda idx=idx, slot=slot: (SDTest.Call(0x2556b0, 'tviii', m1, idx, slot),
		moslots(m1), moaget(a1)[1], moaget(a2)[1]))
for s in range(4):
	moset(m1, s, 0, 0, 0.0)
mofl(a2 + 0x14, (0.0,))
mosetanim('CModelo_SetAnim', 0, 0)
mosetanim('CModelo_SetAnim same', 0, 0)
mosetanim('CModelo_SetAnim speed 0', 1, 0)
mofl(a2 + 0x14, (2.0,))
mosetanim('CModelo_SetAnim blend', 0, 0)
mofl(moslot(m1, 0) + 8, (0.3,))
mosetanim('CModelo_SetAnim blend 0.3', 1, 0)
mofl(moslot(m1, 0) + 8, (0.8,))
mosetanim('CModelo_SetAnim blend 0.8', 0, 0)
mofl(moslot(m1, 0) + 8, (0.5,))
mosetanim('CModelo_SetAnim blend 0.5', 1, 0)
SDTest.Poke(0x4ae638, 1)
mosetanim('CModelo_SetAnim paused', 0, 0)
SDTest.Poke(0x4ae638, 0)
mosetanim('CModelo_SetAnim none', 50, 0)
mosetanim('CModelo_SetAnim slot 2', 3, 2)
mosetanim('CModelo_SetAnim slot 2 back', 0, 2)

# CModelo_UpdateAnims without the players (animate 0): blending and the morph subsets
md = moz(80)
for i in range(10):
	first = morand(0, 9)
	last = morand(first, 9)
	mow(md + 8 * i, ord('t'), ord('0') + i, morand(0, 7), morand(0, 3))
	mow(md + 8 * i + 4, morand(0, 30), first, last, morand(0, 100))
mopoke(a3 + 0x8c, md)
mow(m1 + 0x9c, 0, 0, 1, 0)
for s in range(4):
	moset(m1, s, 0, 0, 0.0)
moset(m1, 0, a1, a2, 0.5)
moset(m1, 1, a3, 0, 0.0)
moset(m1, 2, a4, a1, 0.01)
moset(m1, 3, 0, a2, 0.7)
mofl(a3 + 0x14, (0.5,))
mow(a3 + 0x90, 4, 0, 0, 0)
def moupdate(i):
	SDTest.Call(0x254fc0, 'tvifi', m1, 0.04, -1)
	return (moslots(m1), mofget(a3 + 0x14, 1), mobyte(a3 + 0x90), mobyte(a3 + 0x91), mofget(m1 + 8, 3))
for i in range(16):
	check('CModelo_UpdateAnims' + fmt((i,)), lambda i=i: moupdate(i))
	if i == 7:
		mow(a3 + 0x90, 9, 2, 0, 0)
		moset(m1, 0, a1, a2, 0.9)
mogf(0x4ae770, 0.0)
check('CModelo_UpdateAnims speed 0', lambda: moupdate(16))
mogf(0x4ae770, 1.0)
for t in ('t0', 't5', 't9', 'xx', 'T1'):
	for s in (1,):
		check('CModelo_SetMorphSubset' + fmt((t,)), lambda t=t, s=s: (SDTest.Call(0x254850, 'tbiis', m1, s, t)[0],
			mofget(a3 + 0x14, 1), mobyte(a3 + 0x90), mobyte(a3 + 0x91)))
		mofl(a3 + 0x14, (0.5,))

# the animation offset and the render context
ctx = moz(0xb0)
def moctx():
	r = []
	for i in range(4):
		c = ctx + 0x18 * i
		s = mopeek(c)
		k = None
		for j in range(4):
			if s == moslot(m1, j):
				k = j
		r.append((k, s == 0, tpeek(c + 4, 1), mofget(c + 8, 1), tpeek(c + 0xc, 1), mofget(c + 0x10, 2)))
	return (r, mofget(ctx + 0x60, 16), mopeek(ctx + 0xa0) == m1, mopeek(ctx + 0xa4) == mopeek(m1 + 0xac))
for i in range(6):
	moset(m1, 0, (a1, a1, 0, a2, a1, a1)[i], (a2, 0, a2, a1, a2, a2)[i], rnd(0.0, 1.0))
	mopoke(ctx, (moslot(m1, 0), moslot(m1, 0), moslot(m1, 0), moslot(m1, 0), 0, moslot(m1, 0))[i])
	mofl(ctx + 0x14, (rnd(0.0, 1.0),))
	check('CModelo_AnimOffset' + fmt((i,)), lambda: (SDTest.Call(0x255530, 'tvii', m1, ctx), mofget(m1 + 0x2c, 3)))
for i in range(4):
	mofl(a1 + 0x58, (rnd(0.0, 30.0),))
	mofl(a2 + 0x58, (rnd(0.0, 30.0),))
	moset(m1, 0, a1, (a2, 0, a2, a1)[i], rnd(-0.5, 1.5))
	moset(m1, 1, (0, a2, a4, a3)[i], 0, rnd(0.0, 1.0))
	moset(m1, 2, (a4, 0, a1, 0)[i], (0, 0, a2, a3)[i], rnd(0.0, 1.0))
	mofl(m1 + 8, vec() + vec() + (rnd(0.5, 2.0), rnd(0.5, 2.0), rnd(0.5, 2.0)) + vec() + vec() + vec())
	mofl(m1 + 0x5c, affine())
	check('CModelo_SetupRender' + fmt((i,)), lambda: (SDTest.Call(0x2531e0, 'tvii', m1, ctx), moctx(), mofget(m1 + 0x2c, 3)))
mofl(m1 + 0x5c, ident)

# the render nodes and the nodes of the scene
monodes = []
for name, clase, flags in (('Bip01', 0xa1000001, 0), ('Bip01 Head', 0xa1000002, 0x40000000), ('Dummy01', 0, 0x80),
		('omni01', 0xa3000008, 0), ('Mesh02', 0xa5000020, 0x40000000)):
	monodes.append(monode(name, clase, flags, vec(), quat()))
molink(monodes, 0xc)
tabr = moadd(moz(8))
arr = moadd(moz(0xa8 * 5))
for i in range(5):
	mopoke(arr + 0xa8 * i + 0xa4, monodes[i])
mopoke(tabr, 5)
mopoke(tabr + 4, arr)
mopoke(m1 + 0xac, tabr)
for i in range(5):
	for anim in (0, 1):
		mofl(m1 + 8, vec() + vec() + (rnd(0.5, 2.0), rnd(0.5, 2.0), rnd(0.5, 2.0)) + vec() + vec() + vec())
		check('CModelo_GetNodeMatrix' + fmt((i, anim)), lambda i=i, anim=anim: SDTest.Call(0x253440, 'tiiFiiiii', m1,
			ident, i, anim, 0, 0, 0)[1])
def mofindr(n, wild, nocase):
	r = SDTest.Call(0x2552a0, 'tiispii', m1, n, 99, wild, nocase)
	k = None
	if r[0]:
		k = (r[0] - arr) / 0xa8
	return (k, r[1])
for n in ('Bip01', 'bip01', 'BIP01 HEAD', 'Bip01*', 'bip01*', 'BIP01 H*', 'dum?y01', 'Dummy01', 'x', '*', '', 'mesh0[12]'):
	for wild in (0, 1):
		for nocase in (0, 1):
			check('CModelo_FindNodoR' + fmt((n, wild, nocase)), lambda n=n, wild=wild, nocase=nocase: mofindr(n, wild, nocase))
mopoke(m1 + 0xac, 0)
mopoke(e1 + 0xc, monodes[0])
for n in ('Bip01', 'bip01', 'Dummy01', 'nonode', 'omni01'):
	check('CModelo_FindNodo' + fmt((n,)), lambda n=n: mowho(SDTest.Call(0x255270, 'tiis', m1, n)[0]))
	check('CModelo_FindNodoWild' + fmt((n,)), lambda n=n: mowho(SDTest.Call(0x255500, 'tiis', m1, n + '*')[0]))
	check('CModelo_SetPivotNodo' + fmt((n,)), lambda n=n: (SDTest.Call(0x255e70, 'tbis', m1, n)[0], mofget(m1 + 0x38, 3)))
for frm in (0, monodes[0], monodes[2], monodes[4]):
	for clase in (0, 0xa1000001, 0xa1000002, 0xa3000008, 0xa5000020, 0x12345):
		check('CModelo_FindClase' + fmt((mowho(frm), clase)), lambda frm=frm, clase=clase: mowho(SDTest.Call(0x255250,
			'tiiii', m1, frm, clase)[0]))
check('CModelo_FindClase no scene', lambda: (mopoke(m1 + 0x124, 0), mowho(SDTest.Call(0x255250, 'tiiii', m1, 0, 0)[0]))[1])
check('CModelo_FindNodo no scene', lambda: mowho(SDTest.Call(0x255270, 'tiis', m1, 'Bip01')[0]))
check('CModelo_SetPivotNodo no scene', lambda: SDTest.Call(0x255e70, 'tbis', m1, 'Bip01')[0])
mopoke(m1 + 0x124, e1)

# CModelo_SetEscena: references, the flags from the nodes and the box
def moescstep(name, esc):
	check(name, lambda esc=esc: (SDTest.Call(0x2558f0, 'tvii', m1, esc), moget(m1), mopeek(e1 + 8), mopeek(e2 + 8),
		mopeek(e3 + 8)))
mopoke(e2 + 0xc, monodes[0])
mobox(e2, (0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
moescstep('CModelo_SetEscena zero box', e2)
mobox(e3, (-1.0, -2.0, -3.0, 1.0, 2.0, 3.0))
mopoke(e3 + 0xc, monodes[2])
mow(m1 + 0x9c, 0, 0, 1, 0)
moescstep('CModelo_SetEscena', e3)
for i in range(5):
	mopoke(monodes[i] + 0x18, (0, 0x40000000, 0x40000088, 0x8, 0xa8)[(i + 2) % 5])
mopoke(e1 + 0xc, monodes[0])
mobox(e1, (0.0, 0.0, 0.0, 0.0, 0.0, 1e-30))
moescstep('CModelo_SetEscena flags', e1)
moescstep('CModelo_SetEscena same', e1)
moescstep('CModelo_SetEscena NULL', 0)
mopoke(e2 + 0xc, 0)
mopoke(e3 + 0xc, 0)
mopoke(e1 + 0xc, 0)
check('CModelo_ReleaseD3D', lambda: (SDTest.Call(0x2530c0, 'tviii', m1, 0, 0), mopoke(m1 + 0x124, e1),
	SDTest.Call(0x2530c0, 'tviii', m1, 0, 1), SDTest.Call(0x253190, 'tviii', m1, 0, 0),
	SDTest.Call(0x253190, 'tviii', m1, 0, 1), moget(m1))[5])
check('Modelo_Check', lambda: SDTest.Call(0x2563f0, 'vi', m1))

# the LODs
m2 = moadd(moz(0x1f0))
SDTest.Call(0x252a80, 'tii', m2)
eA = moescena('models\\Car.m3d', 10)
eB = moescena('models\\Car.lod1.m3d', 10)
mopoke(m1 + 0x124, eA)
mopoke(m2 + 0x124, eB)
mostep('CModelo_SetNumLods', m1, 0x2546a0, 'tvii', 2)
def molods(m):
	p = mopeek(m + 0x104)
	r = []
	for i in range(mopeek(m + 0x100)):
		r.append((mowho(mopeek(p + 8 * i)), mopeek(p + 8 * i + 4)))
	return r
mopoke(eB + 0x44, mostr('models\\Car.m3d'))
check('CModelo_SetLod', lambda: (SDTest.Call(0x2546b0, 'tviii', m1, 1, m2), molods(m1), mowho(mopeek(m2 + 0x128))))
mopoke(m2 + 0x128, 0)
mopoke(eA + 0x60, 1)
mopoke(eB + 0x44, mostr('other\\CAR.M3D'))
check('CModelo_SetLod sm3', lambda: (SDTest.Call(0x2546b0, 'tviii', m1, 0, m2), molods(m1), mowho(mopeek(m2 + 0x128))))
p = mopeek(m1 + 0x104)
mopoke(p, 0)
mopoke(p + 8, 0)
check('CModelo_Delete', lambda: (SDTest.Call(0x253070, 'tiii', m2, 1)[0] == m2, mopeek(eB + 8)))

# tabla.h instances and the vector deleting destructors
t = moz(8)
mopoke(t, 3)
mopoke(t + 4, SDTest.Call(0x313642, 'ii', 16)[0])
check('Tabla_Free', lambda: (SDTest.Call(0x252cd0, 'tvi', t), tpeek(t, 2)))
check('Tabla_Free empty', lambda: (SDTest.Call(0x252cd0, 'tvi', t), tpeek(t, 2)))
SDTest.Call(0x255ff0, 'tvii', t, 3)
check('Tabla_DeleteLods', lambda: (SDTest.Call(0x252d00, 'tvi', t), tpeek(t, 2)))
check('Tabla_DeleteLods empty', lambda: (SDTest.Call(0x252d00, 'tvi', t), tpeek(t, 2)))
c = moz(8)
check('CLod_dtor', lambda: SDTest.Call(0x252d80, 'tvi', c))
check('CLod_DeleteVec 0', lambda: SDTest.Call(0x252f00, 'tiii', c, 0)[0] == c)
check('CLod_DeleteVec 1', lambda: SDTest.Call(0x252f00, 'tiii', c, 1)[0] == c)
SDTest.Call(0x255ff0, 'tvii', t, 2)
p = mopeek(t + 4)
check('CLod_DeleteVec 3', lambda: SDTest.Call(0x252f00, 'tiii', p, 3)[0] - p)
mopoke(t, 0)
mopoke(t + 4, 0)
SDTest.Call(0x255ff0, 'tvii', t, 2)
p = mopeek(t + 4)
check('CLod_DeleteVec 2', lambda: SDTest.Call(0x252f00, 'tiii', p, 2)[0] - p)
mofree(p - 4)
mopoke(t, 0)
mopoke(t + 4, 0)
nr = monode('nr', 0, 0, vec(), quat())
mopoke(nr + 8, 10)
SDTest.Call(0x2560d0, 'tvii', t, 3)
p = mopeek(t + 4)
for i in range(3):
	mopoke(p + 0xa8 * i + 0xa4, nr)
check('CNodoR_DeleteVec 3', lambda: (SDTest.Call(0x252fc0, 'tiii', p, 3)[0] - p, mopeek(nr + 8)))
e = moz(0xa8)
SDTest.Call(0x23f3d0, 'tii', e)
mopoke(e + 0xa4, nr)
check('CNodoR_DeleteVec 0', lambda: (SDTest.Call(0x252fc0, 'tiii', e, 0)[0] == e, mopeek(nr + 8)))
SDTest.Call(0x23f3d0, 'tii', e)
mopoke(e + 0xa4, nr)
check('CNodoR_DeleteVec 1', lambda: (SDTest.Call(0x252fc0, 'tiii', e, 1)[0] == e, mopeek(nr + 8)))
mofree(t)

# luz.h
l = moz(0xcc)
for tipo in (5000, 5001, 5002):
	mopoke(l + 8, tipo)
	check('CLuz_TipoNombre' + fmt((tipo,)), lambda: SDTest.Call(0x2565b0, 'tsi', l)[0])
mofree(l)

# util_escena.cpp: the compare helpers
for a in (None, '', 'abc', 'ABC', 'abd'):
	for b in (None, '', 'abc', 'Abc', 'ab'):
		call('Cad_EqualNoCase', 0x2578a0, 'izz', a, b)
la, lb = moz(0x1c), moz(0x1c)
pa, pb = moz(4), moz(4)
mopoke(pa, la)
mopoke(pb, lb)
for x in specials:
	for y in (0.0, 1.0, -1.0, nan):
		mofl(la + 0x18, (x,))
		mofl(lb + 0x18, (y,))
		check('ListaModelo_CompareKey' + fmt((x, y)), lambda: (SDTest.Call(0x257ee0, 'iii', pa, pb)[0],
			SDTest.Call(0x257ee0, 'iii', pb, pa)[0]))
mofree(la)
mofree(lb)
mofree(pa)
mofree(pb)

# the scene list entries on a list of our own
ehead = moadd(moz(4))
e6 = moescena('e6', 1)
e7 = moescena('e7', 1)
mopoke(e6, novt)
mopoke(e7, novt)
def moleget(le):
	return (mowho(mopeek(le)), SDTest.Peek(le + 4, '@p'), SDTest.Peek(le + 8, '@p'), mofget(le + 0xc, 3),
		mopeek(le + 0x18), mowho(mopeek(le + 0x1c)))
def molenew(esc, name, name2, escala, flags):
	le = moadd(moz(0x20))
	check('CListaEscena_ctor' + fmt((name, name2, escala, flags)), lambda le=le, esc=esc, name=name, name2=name2,
		escala=escala, flags=flags: (SDTest.Call(0x256600, 'tiiizzFii', le, esc, name, name2, escala, flags,
		ehead)[0] == le, moleget(le), mowho(mopeek(ehead)), mopeek(esc + 8)))
	return le
le1 = molenew(e6, 'a.m3d', None, (1.0, 1.0, 1.0), 1)
le2 = molenew(e7, 'B.M3D', 'tex', (2.0, 1.0, 1.0), 3)
le3 = molenew(e6, 'a.m3d', '', (1.0, 2.0, 1.0), 0x8)
for n, n2, esc, f, mask in (('a.m3d', None, (1.0, 1.0, 1.0), 1, 1), ('A.M3D', '', (1.0, 1.0, 1.0), 1, 1),
		('a.m3d', None, (1.0, 2.0, 1.0), 1, 1), ('a.m3d', None, (5.0, 5.0, 5.0), 8, 0), ('b.m3d', 'TEX', (2.0, 1.0, 1.0), 3, 3),
		('b.m3d', 'tex', (2.0, 1.0, 1.0), 1, 2), ('b.m3d', None, (2.0, 1.0, 1.0), 0, 0), ('c.m3d', None, (1.0, 1.0, 1.0), 0, 0),
		(None, None, (1.0, 1.0, 1.0), 0, 0), ('a.m3d', 'x', (1.0, 2.0, 1.0), 8, 8)):
	check('CListaEscena_Find' + fmt((n, n2, esc, f, mask)), lambda n=n, n2=n2, esc=esc, f=f, mask=mask:
		mowho(SDTest.Call(0x257640, 'tiizzFii', le1, n, n2, esc, f, mask)[0]))
SDTest.Poke(0x4ae77c, le1)
for esc in (0, e6, e7, e1):
	check('Escena_Next' + fmt((mowho(esc),)), lambda esc=esc: mowho(SDTest.Call(0x257280, 'ii', esc)[0]))
check('Escenas_Msg', lambda: SDTest.Call(0x257240, 'viiiii', 1, 0, 0, 0, 0))
SDTest.Poke(0x4ae77c, mosaved[4])

# the model list and gModelosRender
mhead = moadd(moz(4))
m3 = moadd(moz(0x1f0))
SDTest.Call(0x252a80, 'tii', m3)
mow(m3 + 0x120, 0, 0, 0, 0)
e8 = moescena('e8', 10)
mopoke(m3 + 0x124, e8)
def molmget(lm):
	return (mowho(mopeek(lm)), SDTest.Peek(lm + 4, '@p'), SDTest.Peek(lm + 8, '@p'), mopeek(lm + 0xc),
		mowho(mopeek(lm + 0x10)), mopeek(lm + 0x14), mofget(lm + 0x18, 1))
lm1 = moadd(moz(0x1c))
check('CListaModelo_ctor', lambda: (SDTest.Call(0x2566e0, 'tiiizzii', lm1, m3, 'm3', None, 2, mhead)[0] == lm1,
	molmget(lm1), mowho(mopeek(mhead)), mowho(mopeek(m3 + 0xfc))))
m4 = moadd(moz(0x1f0))
SDTest.Call(0x252a80, 'tii', m4)
mow(m4 + 0x120, 0, 0, 0, 0)
mopoke(m4 + 0x124, e8)
lm2 = moadd(moz(0x1c))
check('CListaModelo_ctor 2', lambda: (SDTest.Call(0x2566e0, 'tiiizzii', lm2, m4, 'm4', 'x', 6, mhead)[0] == lm2,
	molmget(lm1), molmget(lm2), mowho(mopeek(mhead)), mowho(mopeek(m4 + 0xfc))))
for n, n2, f, mask in (('m3', None, 2, 2), ('M3', '', 0, 0), ('m4', 'X', 6, 4), ('m4', None, 6, 4), ('m5', None, 0, 0)):
	check('CListaModelo_Find' + fmt((n, n2, f, mask)), lambda n=n, n2=n2, f=f, mask=mask:
		mowho(SDTest.Call(0x257750, 'tiizzii', lm1, n, n2, f, mask)[0]))
nrender = mosaved[5]
slotsaved = mopeek(0x400000 + 0x4ae7c8 + 4 * nrender)
def morender(lm):
	idx = mopeek(lm + 0x14)
	r = (SDTest.Peek(0x4af7c8, 'i') - nrender, idx)
	if idx >= 0:
		r = r + (mopeek(0x400000 + 0x4ae7c8 + 4 * idx) == lm,)
	return r + (mobyte(m3 + 0x120), mobyte(m3 + 0x121))
mow(m3 + 0x120, 0, 1, 0, 0)
check('Modelo_SetEnable 1', lambda: (SDTest.Call(0x256c50, 'vii', m3, 1), morender(lm1)))
check('Modelo_SetEnable 1 again', lambda: (SDTest.Call(0x256c50, 'vii', m3, 1), morender(lm1)))
check('CModelo_Disable', lambda: (SDTest.Call(0x2530a0, 'tvi', m3), morender(lm1)))
check('CModelo_Enable', lambda: (SDTest.Call(0x2530b0, 'tvi', m3), morender(lm1)))
check('Modelo_SetEnable 0', lambda: (SDTest.Call(0x256c50, 'vii', m3, 0), morender(lm1)))
check('Modelo_UpdateRender', lambda: (SDTest.Call(0x2568a0, 'vi', m3), morender(lm1)))
mopoke(0x400000 + 0x4ae7c8 + 4 * nrender, slotsaved)
SDTest.Poke(0x4af7cc, mosaved[6])
# gModelosRender with only lm1 for the animation update
mopoke(0x400000 + 0x4ae7c8, lm1)
SDTest.Poke(0x4af7c8, 1)
mopoke(lm1 + 0x14, 0)
mow(m3 + 0x120, 1, 0, 0, 0)
mow(m3 + 0x9c, 0, 0, 1, 0)
moset(m3, 0, a1, a2, 0.9)
moset(m3, 1, a4, a1, 0.2)
check('Modelos_UpdateAnims', lambda: (SDTest.Call(0x256c80, 'vf', 0.04), moslots(m3)))
SDTest.Poke(0x4af7c8, 0)
check('Modelos_Render', lambda: (SDTest.Call(0x2572b0, 'v'), SDTest.Peek(0x4af7c8, 'i')))
mopoke(0x400000 + 0x4ae7c8, SDTest.Peek(0x4ae7c8, 'i'))
SDTest.Poke(0x4ae7c8, mopeek(0x400000 + 0x4ae7c8))
SDTest.Poke(0x4af7c8, nrender)
mopoke(lm1 + 0x14, -1)
mow(m3 + 0x120, 0, 0, 0, 0)
for s in range(4):
	moset(m3, s, 0, 0, 0.0)
SDTest.Poke(0x4ae778, mopeek(mhead))
for n in ('Modelo', 'modelo', 'm3', ''):
	check('Modelo_FindName' + fmt((n,)), lambda n=n: mowho(SDTest.Call(0x257820, 'is', n)[0]))
# the dump through SDTest's callback (it gets the format strings), gModelosRender empty
SDTest.Poke(0x4ae77c, le1)
SDTest.Poke(0x4af7c8, 0)
check('UtilEscena_Dump', lambda: (SDTest.Call(0x256f90, 'vi', SDTest.OutFunc()), SDTest.Out()))
check('UtilEscena_ReleaseD3D', lambda: (SDTest.Call(0x256e60, 'v'), moget(m3), moget(m4)))
check('UtilEscena_InitD3D', lambda: (SDTest.Call(0x256f00, 'v'), moget(m3), moget(m4)))
SDTest.Poke(0x4af7c8, nrender)
SDTest.Poke(0x4ae77c, mosaved[4])
SDTest.Poke(0x4ae778, mosaved[3])

# Escena_Release on our scene list: the entry is freed when the caller and the list hold the scene
def morelease(name, head, esc, refs, force):
	mopoke(esc + 8, refs)
	SDTest.Poke(0x4ae77c, head)
	r = SDTest.Call(0x2569f0, 'bii', esc, force)[0]
	h = SDTest.Peek(0x4ae77c, 'i')
	SDTest.Poke(0x4ae77c, mosaved[4])
	check(name + fmt((refs, force)), lambda r=r, h=h, esc=esc: (r, mowho(h), mopeek(esc + 8)))
	return h
morelease('Escena_Release NULL', le1, 0, 0, 0)
morelease('Escena_Release refs 3', le1, e6, 3, 0)
morelease('Escena_Release refs 3 force', le1, e6, 3, 1)
morelease('Escena_Release shared', le1, e6, 2, 0)
morelease('Escena_Release not in list', le1, e1, 2, 1)
h = morelease('Escena_Release second', le1, e7, 2, 1)
check('Escena_Release list', lambda: moleget(le1)[5])
h = morelease('Escena_Release first', le1, e6, 2, 1)
check('Escena_Release list', lambda h=h: (mowho(h), moleget(le3)[5]))
mopoke(e6 + 8, 2)
mopoke(le3, e6)
check('Escena_Release no list', lambda: SDTest.Call(0x2569f0, 'bii', e6, 1)[0])

# Modelo_Destroy: the model of the list deleted with its entry, then one without entry
pm = moz(4)
SDTest.Poke(0x4ae77c, 0)
SDTest.Poke(0x4ae778, mopeek(mhead))
def modestroy(name, m, force):
	mopoke(pm, m)
	check(name, lambda force=force: (SDTest.Call(0x256b50, 'vii', pm, force), mopeek(pm), mowho(SDTest.Peek(0x4ae778, 'i')),
		mopeek(e8 + 8)))
modestroy('Modelo_Destroy second', m4, 0)
check('Modelo_Destroy list', lambda: molmget(lm1))
m5 = moadd(moz(0x1f0))
SDTest.Call(0x252a80, 'tii', m5)
mopoke(m5 + 0x124, e8)
modestroy('Modelo_Destroy not in list', m5, 1)
modestroy('Modelo_Destroy NULL', 0, 0)
modestroy('Modelo_Destroy first', m3, 0)
SDTest.Poke(0x4ae778, mosaved[3])
SDTest.Poke(0x4ae77c, mosaved[4])

# CListaModelo_dtor alone, then UtilEscena_FreeAll on lists of our own
m6 = moadd(moz(0x1f0))
SDTest.Call(0x252a80, 'tii', m6)
mopoke(m6 + 0x124, e8)
lm3 = moadd(moz(0x1c))
mopoke(mhead, 0)
SDTest.Call(0x2566e0, 'tiiizzii', lm3, m6, 'm6', 'six', 2, mhead)
check('CListaModelo_dtor', lambda: (SDTest.Call(0x2567c0, 'tvi', lm3), molmget(lm3), mopeek(e8 + 8)))
mofree(lm3)
m7 = moadd(moz(0x1f0))
SDTest.Call(0x252a80, 'tii', m7)
mopoke(m7 + 0x124, e8)
lm4 = moadd(moz(0x1c))
mopoke(mhead, 0)
SDTest.Call(0x2566e0, 'tiiizzii', lm4, m7, 'm7', None, 2, mhead)
mopoke(ehead, 0)
e9 = moescena('e9', 0)
e10 = moescena('e10', 0)
mopoke(e9, novt)
mopoke(e10, novt)
le4 = moadd(moz(0x20))
le5 = moadd(moz(0x20))
SDTest.Call(0x256600, 'tiiizzFii', le4, e9, 'e9', None, (1.0, 1.0, 1.0), 2, ehead)
SDTest.Call(0x256600, 'tiiizzFii', le5, e10, 'e10', 'x', (1.0, 1.0, 1.0), 0, ehead)
SDTest.Poke(0x4ae778, lm4)
SDTest.Poke(0x4ae77c, le4)
check('UtilEscena_FreeAll', lambda: (SDTest.Call(0x256d30, 'v'), SDTest.Peek(0x4ae778, 'i'), SDTest.Peek(0x4ae77c, 'i'),
	mopeek(e9 + 8), mopeek(e10 + 8), mopeek(e8 + 8)))
SDTest.Poke(0x4ae778, mosaved[3])
SDTest.Poke(0x4ae77c, mosaved[4])

# the first model: its slots emptied and the group cleared (the group would delete the fake animations)
for s in range(4):
	moset(m1, s, 0, 0, 0.0)
for i in range(4):
	mopoke(m1 + 0x12c + 4 * i, 0)
mopoke(m1 + 0x124, e1)
check('CModelo_dtor', lambda: (SDTest.Call(0x252dd0, 'tvi', m1), SDTest.Peek(m1 + 4, '@i'), tpeek(m1 + 0xa4, 3),
	tpeek(m1 + 0x100, 2), mopeek(m1 + 0x124), mopeek(e1 + 8)))
morestore()
