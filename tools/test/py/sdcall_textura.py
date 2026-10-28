# engine3d Engine/Textura.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the pure helpers, the
# bump, DUDV and ramp conversions on heap images, and the texture cache on a list of its own. The list head of the
# engine (rva 0x4b2610) is set aside while the cache functions run and put back at the end, so no entry of the
# engine is touched. Entries are made by the constructor of the exe; the only files are BMP and TGA files written
# here (no PNG or DDC: those are read through the packs), and no device texture is made (the device argument is
# NULL, device textures are fake COM objects whose Release is a "xor eax,eax; ret 4" of the exe).
def tz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def tpoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)

for v in (0, 0x14, 0x15, 0x19, 0x1a, 0x20, 0x21, 0x22, 0x29, 0x3c, 0x42, 0x50, 99, 100, 0x66, 0x67, -1, -2147483647 - 1,
		0x31545844, 0x31545845, 0x32545844, 0x32595559, 0x33545844, 0x34545844, 0x34545845, 0x35545844, 0x59565955,
		0x7fffffff):
	call('D3DFmt_Name', sym('D3DFmt_Name'), 'si', v)
for bpp in (0, 4, 8, 9, 16, 24, 32, -1):
	for alpha in (0, 1, 0x100):
		for pal in (0, 1):
			for bump in (0, 1):
				for comp in (0, 1):
					call('Tex_Format', sym('Tex_Format'), 'iiiiiii', bpp, alpha, pal, bump, comp, 1)
for n in (-4, -1, 0, 1, 2, 3, 4, 6, 8, 1024, 1023, 0x40000000, 0x7fffffff, -2147483647 - 1):
	call('Tex_IsPower2', sym('Tex_IsPower2'), 'bi', n)
for s in ('', 'a', 'a/b/c', '//x\\y/', 'no'):
	call('Ruta_ToBackslash', sym('Ruta_ToBackslash'), 'vb', s)
# names without a '.' are left out: CHECK(ext)
for n in ('a.alpha.tga', 'A.ALPHA.TGA', '.alpha.tga', 'alpha.tga', 'x.alpha', 'd.x/a.bump.png', 'a.bump.x.tga', 'a.tga',
		'abc.cube.dds', 'q.dudv.bmp', 'q.dudvx.bmp'):
	for e in ('.alpha', '.bump', '.cube', '.dudv', '', 'a'):
		call('Ruta_HasSubExt', sym('Ruta_HasSubExt'), 'bss', n, e)

# bmp.h: the constructor with a size and the deleting destructor
cb = tz(12)
check('CBmp_ctor', lambda cb=cb: (SDTest.Call('CBmp_ctor', 'tiiii', cb, 3, 2)[0] == cb, SDTest.Peek(cb, '@i'),
	SDTest.Peek(cb + 4, '@i'), SDTest.Peek(cb + 8, '@i') != 0))
check('CBmp_Delete 0', lambda cb=cb: (SDTest.Call('CBmp_Delete', 'tiii', cb, 0)[0] == cb, tpeek(cb, 3)))
SDTest.Call('Mem_Free', 'vi', cb)
cb = SDTest.Call('operator_new', 'ii', 12)[0]
SDTest.Call('CBmp_ctor', 'tiiii', cb, 5, 1)
check('CBmp_Delete 1', lambda cb=cb: SDTest.Call('CBmp_Delete', 'tiii', cb, 1)[0] == cb)

# the conversions; the pixels include the limits of the bump and DUDV ranges
edge = (0x00000000, 0x00004040, 0x00004141, 0x0000b7b7, 0x0000b8b8, 0xffffff, 0x12345678, -1, 0x00008080, 0x00007f80,
	0x0000807f, 0x00000480, 0x00008004, 0x0000ff00, 0x000000ff)
def texpix(n):
	v = list(edge)
	while len(v) < n:
		v.append(cpix())
	return v[:n]
for w, h in ((5, 4), (1, 1), (3, 7), (8, 2)):
	for rva, name in ((sym('Bmp_ToBump'), 'Bmp_ToBump'), (sym('Bmp_ToDudv'), 'Bmp_ToDudv'), (sym('Bmp_ToRamp'), 'Bmp_ToRamp')):
		tb = bnew(w, h, sym('CBmp_Create'))
		cset(tb, texpix(w * h))
		bcheck(name + fmt((w, h)), tb, rva, 'vi', (), 1)
		bfree(tb)

# the cache on a list of its own
thead = SDTest.Peek('g_TexList', 'i')
tcount = SDTest.Peek('g_TexCount', 'i')
tglob = (sym('g_TexCreated'), sym('g_TexCached'), sym('g_TexUsed'), sym('g_TexUsedPixels'), sym('g_TexPurgeFrame'), sym('g_CachedTex'))
tsaved = []
for g in tglob:
	tsaved.append(SDTest.Peek(g, 'i'))
SDTest.Poke('g_TexList', 0)
tents = []
def twho(p):
	if not p:
		return 'NULL'
	for i in range(len(tents)):
		if tents[i] == p:
			return i
	return 'new'
def tnewe(name, opac, kind, flags):
	t = SDTest.Call('operator_new', 'ii', 0x258)[0]
	SDTest.Call('CTextura_ctor', 'tiizzii', t, name, opac, kind, flags)
	tents.append(t)
	return t
def tget(t):
	if not t:
		return None
	b = []
	for i in range(0x23c, 0x246):
		b.append(SDTest.Peek(t + i, '@b'))
	bm = SDTest.Peek(t + 0x22c, '@i')
	if bm:
		bm = tpeek(bm, 2), SDTest.Peek(bm + 8, '@i') != 0
	return (SDTest.Peek(t, '@i'), twho(SDTest.Peek(t + 4, '@i')), SDTest.Peek(t + 8, '@s'), SDTest.Peek(t + 0x10c, '@s'),
		tpeek(t + 0x210, 6), SDTest.Peek(t + 0x228, '@i') != 0, bm, SDTest.Peek(t + 0x230, '@i') != 0,
		SDTest.Peek(t + 0x238, '@i'), b, tpeek(t + 0x248, 4))
def tlist():
	r = []
	t = SDTest.Peek('g_TexList', 'i')
	while t and len(r) < 50:
		r.append(twho(t))
		t = SDTest.Peek(t + 4, '@i')
	return r, SDTest.Peek('g_TexCount', 'i') - tcount
def tcall(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: twho(apply(SDTest.Call, (rva, sig) + args)[0]))
# entries as arguments: their index in the name
def tecall(name, rva, sig, t, *args):
	check(name + fmt((twho(t),) + args), lambda rva=rva, sig=sig, t=t, args=args: apply(SDTest.Call, (rva, sig, t) + args)[0])

t0 = tnewe('sdtex_a.bmp', None, 7, 0)
t1 = tnewe('SDTEX_A.BMP', 'sdopac.bmp', 2, 0x20)
t2 = tnewe('sdtex_b.tga', '', 0, 0x40)
t3 = tnewe('sdtex_a.bmp', 'Other.bmp', 1, 0x1a0)
for t in tents:
	check('CTextura_ctor', lambda t=t: tget(t))
check('Textura list', tlist)
for n in ('sdtex_a.bmp', 'Sdtex_A.Bmp', 'sdtex_b.tga', 'sdtex_b.TGA', 'none.bmp', ''):
	for o in (None, '', 'sdopac.bmp', 'SDOPAC.BMP', 'other.bmp', 'x.bmp'):
		tcall('Textura_Lookup', sym('Textura_Lookup'), 'izz', n, o)
tcall('Textura_Find', sym('Textura_Find'), 'izz', 'sdtex_b.tga', None)
tcall('Textura_Find', sym('Textura_Find'), 'izz', 'sdtex_a.bmp', 'sdopac.bmp')
tcall('Textura_Find', sym('Textura_Find'), 'izz', 'nothing.bmp', None)
call('Textura_AddRef NULL', sym('Textura_AddRef'), 'ii', 0)
call('Textura_Release NULL', sym('Textura_Release'), 'ii', 0)
tecall('Textura_AddRef', sym('Textura_AddRef'), 'ii', t0)
tecall('Textura_AddRef', sym('Textura_AddRef'), 'ii', t0)
tecall('Textura_Release', sym('Textura_Release'), 'ii', t0)
tecall('Textura_Release', sym('Textura_Release'), 'ii', t0)
tecall('Textura_Release', sym('Textura_Release'), 'ii', t1)
tecall('Textura_Release', sym('Textura_Release'), 'ii', t2)
for t in tents:
	check('Textura refs', lambda t=t: SDTest.Peek(t, '@i'))
tinfo = tz(20)
for t in tents:
	check('Textura_GetInfo', lambda t=t: (SDTest.Call('Textura_GetInfo', 'iii', t, tinfo)[0], tpeek(tinfo, 5)))
check('Textura_GetInfo NULL', lambda: SDTest.Call('Textura_GetInfo', 'iii', 0, tinfo)[0])
check('Textura_GetInfo no info', lambda: SDTest.Call('Textura_GetInfo', 'iii', t0, 0)[0])

# fake device textures
tvt = tz(64)
for i in range(16):
	tpoke(tvt + 4 * i, symva('cEntity_Vf21'))
def tfake():
	o = tz(4)
	tpoke(o, tvt)
	return o
tfakes = []
for i in range(6):
	tfakes.append(tfake())
def tfwho(p):
	if not p:
		return 'NULL'
	for i in range(len(tfakes)):
		if tfakes[i] == p:
			return i
	return 'other'
def ttex():
	r = []
	for t in tents:
		r.append(tfwho(SDTest.Peek(t + 0x228, '@i')))
	return r
for i in range(4):
	tpoke(tents[i] + 0x228, tfakes[i])
check('Textura_GetD3DByName', lambda: tfwho(SDTest.Call('Textura_GetD3DByName', 'izz', 'sdtex_a.bmp', 'other.bmp')[0]))
check('Textura_GetD3DByName none', lambda: SDTest.Call('Textura_GetD3DByName', 'izz', 'sdx.bmp', None)[0])
check('Textura_GetD3D', lambda: tfwho(SDTest.Call('Textura_GetD3D', 'iii', t2, 0)[0]))
check('Textura_GetD3D NULL', lambda: SDTest.Call('Textura_GetD3D', 'iii', 0, 0)[0])
check('Textura_Load', lambda: tfwho(SDTest.Call('Textura_Load', 'iii', t1, 0)[0]))
check('Textura_ReleaseD3D', lambda: (SDTest.Call('Textura_ReleaseD3D', 'izz', 'sdtex_a.bmp', None)[0], ttex()))
check('Textura_ReleaseD3D none', lambda: (SDTest.Call('Textura_ReleaseD3D', 'izz', 'sdtex_a.bmp', 'x.bmp')[0], ttex()))
check('Textura_ReleaseAllD3D', lambda: (SDTest.Call('Textura_ReleaseAllD3D', 'i')[0], ttex()))
# no texture, no file: CTextura_CreateD3D refuses the NULL device
check('Textura_GetD3D create', lambda: (SDTest.Call('Textura_GetD3D', 'iii', t0, 0)[0], tget(t0)))
tpoke(t3 + 0x228, tfakes[3])
check('Textura_Recreate', lambda: (SDTest.Call('Textura_Recreate', 'izzi', 'sdtex_a.bmp', 'other.bmp', 0)[0], ttex()))
check('Textura_Recreate none', lambda: SDTest.Call('Textura_Recreate', 'izzi', 'sdnone.bmp', None, 0)[0])
tpoke(t0 + 0x228, tfakes[0])
check('Textura_RecreateAll', lambda: (SDTest.Call('Textura_RecreateAll', 'ii', 0)[0], ttex()))
check('CTextura_CreateD3D', lambda: (SDTest.Call('CTextura_CreateD3D', 'tiii', t1, 0)[0], tget(t1)))
# with failed set a device is never used (with LoadTextures 1 or 2 the call ends there, else at the device check)
check('CTextura_CreateD3D failed', lambda: (tpoke(t1 + 0x240, 0x100), SDTest.Call('CTextura_CreateD3D', 'tiii', t1, tfakes[5])[0], tget(t1)))
tpoke(t1 + 0x240, 0)

# raw file entries (PngData of new[], fake textures) for the purge of Textura_Load
frame = SDTest.Peek('g_FrameCount', 'i')
for i in range(4):
	t = tents[i]
	tpoke(t + 0x230, SDTest.Call('operator_new_array', 'ii', 8)[0])
	tpoke(t + 0x228, tfakes[i])
	tpoke(t + 0x238, frame - (50, 300, 120, 0)[i])
	tpoke(t + 0x210, i + 2)
	tpoke(t + 0x214, 3)
def tstats():
	r = []
	for i in range(len(tglob)):
		r.append(SDTest.Peek(tglob[i], 'i') - tsaved[i])
	r[4] = SDTest.Peek('g_TexPurgeFrame', 'i') == frame
	frames = []
	for t in tents:
		frames.append(SDTest.Peek(t + 0x238, '@i') - frame)
	return r, frames, ttex()
check('Textura_Recreate raw', lambda: SDTest.Call('Textura_Recreate', 'izzi', 'sdtex_b.tga', '', 0)[0])
for limit in (20, 4, 3, 1, 0):
	SDTest.Poke('g_CachedTex', limit)
	check('Textura_FindPurge' + fmt((limit,)), lambda: (twho(SDTest.Call('Textura_FindPurge', 'i')[0]), tstats()))
SDTest.Poke('g_CachedTex', 1)
SDTest.Poke('g_TexPurgeFrame', frame - 1)
check('Textura_Load purge', lambda: (tfwho(SDTest.Call('Textura_Load', 'iii', t0, 0)[0]), tstats()))
check('Textura_Load again', lambda: (tfwho(SDTest.Call('Textura_Load', 'iii', t0, 0)[0]), tstats()))
SDTest.Poke('g_TexPurgeFrame', frame - 1)
check('Textura_Load lost', lambda: (tfwho(SDTest.Call('Textura_Load', 'iii', t1, 0)[0]), tstats()))
check('Textura_GetD3D raw', lambda: (tfwho(SDTest.Call('Textura_GetD3D', 'iii', t1, 0)[0]), tstats()))
check('Textura_ReleaseAllD3D raw', lambda: (SDTest.Call('Textura_ReleaseAllD3D', 'i')[0], ttex()))
SDTest.Poke('g_CachedTex', tsaved[5])

# the sprite table: only a table already built (CMultiSprite_Build reads files)
check('Textura_GetMultiSprite NULL', lambda: SDTest.Call('Textura_GetMultiSprite', 'iiz', 0, 'x')[0])
check('Textura_GetMultiSprite no name', lambda: SDTest.Call('Textura_GetMultiSprite', 'iiz', t2, None)[0])
tpoke(t2 + 0x250, 1)
check('Textura_GetMultiSprite built', lambda: SDTest.Call('Textura_GetMultiSprite', 'iiz', t2, 'sdx')[0] - t2)
tpoke(t2 + 0x250, 0)

# the DDS export: only the paths that end before writing anything (the entry is in a directory that does not exist,
# so there is no "_dds" directory next to it)
tdb = bnew(4, 4, sym('CBmp_Create'))
cset(tdb, texpix(16))
tdds = tnewe('sdnodir/sdx.bmp', None, 0, 0)
for fl in (0, 0x100, 0x140, 0x180, 0x1100):
	tpoke(tdds + 0x220, fl)
	check('CTextura_SaveDDS' + fmt((twho(tdds), 0, 0)), lambda tdds=tdds, tdb=tdb: SDTest.Call('CTextura_SaveDDS', 'tiiiii', tdds, tdb, 0, 0)[0])
	check('CTextura_SaveDDS keep' + fmt((twho(tdds), 1, 1)), lambda tdds=tdds, tdb=tdb: SDTest.Call('CTextura_SaveDDS', 'tiiiii', tdds, tdb, 1, 1)[0])
tpoke(tdds + 0x23d, 1)
check('CTextura_SaveDDS bump' + fmt((twho(tdds), 0, 0)), lambda tdds=tdds, tdb=tdb: SDTest.Call('CTextura_SaveDDS', 'tiiiii', tdds, tdb, 0, 0)[0])

# Textura_Add and the file kinds of CTextura_LoadFile (without load nothing is read); BMP files written by CBmp_Save
tsrc = bnew(4, 2, sym('CBmp_Create'))
cset(tsrc, texpix(8))
tfiles = ('sdtexv.bmp', 'sdtexv.dudv.bmp', 'sdtexo.bmp', 'sdtexv.alpha.tga', 'sdtexv.bump.tga', 'sdtexv.cube.bmp',
	'sdtexv.tex', 'sdtexv.dds', 'sdtexv.ddx', 'sdtexv.ddv', 'sdtexnodot')
for n in tfiles[:6]:
	SDTest.Call('CBmp_Save', 'tiisi', tsrc, n, 1)
for n in tfiles[6:]:
	writefile(n, 'x')
call('Textura_Add NULL', sym('Textura_Add'), 'izzii', None, None, 0, 0)
call('Textura_Add known', sym('Textura_Add'), 'izzii', 'sdtex_a.bmp', None, 0, 0)
call('Textura_Add missing', sym('Textura_Add'), 'izzii', 'sdnotex.bmp', None, 0, 0)
check('Textura list', tlist)
for n, o, fl in (('sdtexv.bmp', None, 0), ('sdtexv.alpha.tga', '', 0x20), ('sdtexv.bump.tga', None, 0),
		('sdtexv.cube.bmp', None, 0x40), ('sdtexv.tex', 'sdtexo.bmp', 0), ('sdtexv.dds', 'sdtexo.bmp', 0x100),
		('sdtexv.ddx', None, 0), ('sdtexv.ddv', None, 0), ('sdtexnodot', None, 0), ('sdtexv.dudv.bmp', None, 0)):
	call('Textura_Add', sym('Textura_Add'), 'izzii', n, o, 3, fl)
	check('Textura_Add entry', lambda n=n, o=o: tget(SDTest.Call('Textura_Lookup', 'izz', n, o)[0]))
check('Textura list', tlist)

# loading with pixels into entries of the test list
for n, o in (('sdtexv.bmp', None), ('sdtexv.bmp', 'sdtexo.bmp'), ('sdtexv.bmp', 'sdnoopac.bmp'), ('sdtexv.dudv.bmp', None),
		('sdnotex.bmp', None)):
	t = tnewe(n, o, 0, 0)
	check('CTextura_LoadFile' + fmt((n, o)), lambda t=t: (SDTest.Call('CTextura_LoadFile', 'tiii', t, 1)[0], tget(t)))
	bm = SDTest.Peek(t + 0x22c, '@i')
	if bm:
		check('CTextura_LoadFile pixels', lambda bm=bm: bget(bm, 1))

# deleting: one by entry, one by name, then the rest
for t in tents:
	tpoke(t + 0x228, 0)
	tpoke(t + 0x0, 0)
check('Textura_Delete', lambda: (SDTest.Call('Textura_Delete', 'vi', t2), tlist()))
check('Textura_Delete NULL', lambda: (SDTest.Call('Textura_Delete', 'vi', 0), tlist()))
check('Textura_Remove', lambda: (SDTest.Call('Textura_Remove', 'izz', 'sdtex_a.bmp', 'sdopac.bmp')[0], tlist()))
check('Textura_Remove none', lambda: (SDTest.Call('Textura_Remove', 'izz', 'sdtex_a.bmp', 'sdopac.bmp')[0], tlist()))
check('Textura_DeleteAll', lambda: (SDTest.Call('Textura_DeleteAll', 'i')[0], tlist()))
SDTest.Poke('g_TexList', thead)
for i in range(len(tglob)):
	SDTest.Poke(tglob[i], tsaved[i])
for n in tfiles:
	SDTest.Call('crt_remove', 'is', n)
bfree(tsrc)
bfree(tdb)
for o in tfakes:
	SDTest.Call('Mem_Free', 'vi', o)
SDTest.Call('Mem_Free', 'vi', tvt)
SDTest.Call('Mem_Free', 'vi', tinfo)
