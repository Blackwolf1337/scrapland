# engine3d Engine/Material.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): materials, maps and
# material lists in heap memory, built by the constructors of the exe. Nothing is added to the material list of the
# engine and no material gets a renderer id (CMatId); textures are only looked up by name, never loaded. The MAT and
# MAP blocks are not tested: CArchIO_EndBlockWrite needs the pack file list being recorded, and only pack files can
# be read through CArchIO.
def mz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def mfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def mpoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def mstr(addr):
	return SDTest.Peek(addr, '@p')
def mfloats(addr, fl):
	SDTest.Call('crt_memcpy', 'iiFi', addr, fl, 4 * len(fl))
def setname(addr, s):
	SDTest.Call('Cad_Set', 'tviz', addr, s)
# a call on a heap object: its address stays out of the name
def ocall(name, rva, sig, obj, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, obj=obj, args=args: apply(SDTest.Call, (rva, sig, obj) + args))
def matnew(name):
	m = mz(0xb0)
	SDTest.Call('CMaterial_ctor', 'tii', m)
	if name:
		setname(m + 4, name)
	return m
def mapnew(name):
	p = mz(0x60)
	SDTest.Call('CMapa_ctor', 'tii', p)
	setname(p, name)
	return p
def who(p, objs):
	if not p:
		return 'NULL'
	for i in range(len(objs)):
		if objs[i] == p:
			return i
	return 'other'
# the bytes 8..0xe one by one: the padding after them is not set by the constructor
def mapget(p):
	cur = SDTest.Peek(p + 0x30, '@i')
	b = []
	for i in range(8, 15):
		b.append(SDTest.Peek(p + i, '@b'))
	return (mstr(p), mstr(p + 4), b, tpeek(p + 0x10, 8), cur == p + 0x34, cur == 0, tpeek(p + 0x34, 2),
		SDTest.Peek(p + 0x3c, '@b'), SDTest.Peek(p + 0x40, '@i'), mstr(p + 0x44), SDTest.Peek(p + 0x48, '@i') != 0,
		SDTest.Peek(p + 0x4c, '@i') != 0, SDTest.Peek(p + 0x54, '@b'), SDTest.Peek(p + 0x58, '@i'),
		SDTest.Peek(p + 0x5c, '@i'))
def mmaps(m):
	r = []
	for i in range(5):
		p = SDTest.Peek(m + 0x9c + 4 * i, '@i')
		if p:
			r.append(mapget(p))
		else:
			r.append(None)
	return r
def matget(m, objs=()):
	return (mstr(m + 4), SDTest.Peek(m + 8, '@i'), who(SDTest.Peek(m + 0xc, '@i'), objs), tpeek(m + 0x10, 13),
		who(SDTest.Peek(m + 0x44, '@i'), objs), who(SDTest.Peek(m + 0x48, '@i'), objs), tpeek(m + 0x4c, 17),
		SDTest.Peek(m + 0x90, '@i'), SDTest.Peek(m + 0x98, '@i'), mmaps(m))
def matdel(m):
	SDTest.Call('CMaterial_Delete', 'tiii', m, 1)
def ibyte():
	return int(rnd(0.0, 255.99))
def icolor():
	return ibyte() + (ibyte() << 8) + (ibyte() << 16) + ((ibyte() & 0x7f) << 24)
# params.atrib in the low word of 0x3c, w3e (0x100 from the constructor) above
def setatrib(m, a):
	mpoke(m + 0x3c, a + 0x1000000)

# the small compare helpers
for a, b in (('abcd', 'abcd'), ('abcd', 'abce'), ('abcd', 'xbcd'), ('abcd', 'abxd'), ('\001\002\003\004', '\001\002\003\004')):
	call('Color_Equal', sym('Color_Equal'), 'bss', a, b)
for a in specials:
	for b in (0.0, -0.0, 1.0, nan):
		call('Vec2_Differ', sym('Vec2_Differ'), 'iFF', (a, 1.0), (b, 1.0))
		call('Vec2_Differ y', sym('Vec2_Differ'), 'iFF', (2.0, a), (2.0, b))
pa, pb = mz(24), mz(24)
for i in range(6):
	mpoke(pa + 4 * i, i * 7 + 1)
	mpoke(pb + 4 * i, i * 7 + 1)
check('MatParams_Equal', lambda: SDTest.Call('MatParams_Equal', 'tbii', pa, pb)[0])
for i in range(6):
	mpoke(pb + 4 * i, 99)
	check('MatParams_Equal differ' + fmt((i,)), lambda: SDTest.Call('MatParams_Equal', 'tbii', pa, pb)[0])
	mpoke(pb + 4 * i, i * 7 + 1)
mfree(pa)
mfree(pb)
for mode in range(1, 7):
	call('Mat_ConvertBlend legacy', sym('Mat_ConvertBlend'), 'vpii', mode, 0, 1)
	call('Mat_ConvertBlend both', sym('Mat_ConvertBlend'), 'vpii', mode, 1, 1)
	call('Mat_ConvertBlend toFile', sym('Mat_ConvertBlend'), 'vpii', mode, 1, 0)
for mode in range(6):
	call('Mat_ConvertBlend', sym('Mat_ConvertBlend'), 'vpii', mode, 0, 0)

# the constructor of the exe ends in CMaterial_CalcColors
m1 = matnew('sdmat1')
check('CMaterial_ctor', lambda: matget(m1))
call('Mat_Str NULL', sym('Mat_Str'), 'si', 0)
check('Mat_Str', lambda: SDTest.Call('Mat_Str', 'si', m1)[0])
emsaved = SDTest.Peek('g_MatEmissive', 'i')
cout = mz(0x44)
fake = mapnew('sdfake.bmp')
for i in range(16):
	for off in (0x10, 0x14, 0x18, 0x1c, 0x28):
		mpoke(m1 + off, icolor())
	mfloats(m1 + 0x20, (rnd(0.0, 2.0), rnd(0.0, 2.0)))
	setatrib(m1, (i & 1) * 0x80)
	if i & 2:
		mpoke(m1 + 0x9c, fake)
	else:
		mpoke(m1 + 0x9c, 0)
	mfloats(sym('g_MatEmissive') + 0x400000, (rnd(0.0, 1.0),))
	check('CMaterial_CalcColors' + fmt((i,)), lambda: (SDTest.Call('CMaterial_CalcColors', 'tvii', m1, cout), tpeek(cout, 17), tpeek(m1 + 0x10, 7)))
mfloats(m1 + 0x20, (inf, -inf))
check('CMaterial_CalcColors inf', lambda: (SDTest.Call('CMaterial_CalcColors', 'tvii', m1, cout), tpeek(cout, 17)))
SDTest.Poke('g_MatEmissive', emsaved)
mpoke(m1 + 0x9c, 0)
mfree(cout)

# shader names (with MAT_SHADER the name must hold one, else the original fails a CHECK)
for name, atrib in (('plain', 0), (None, 0x200), ('x(+shader:water)', 0), ('x(+shader:water)', 0x200),
		('a (+shader:  glass ) b', 0x200), ('(+shader:fire)', 0x280)):
	setname(m1 + 4, name)
	setatrib(m1, atrib)
	check('CMaterial_GetShaderName' + fmt((name, atrib)), lambda: SDTest.Call('CMaterial_GetShaderName', 'tsibi', m1, '', 64)[0])
setatrib(m1, 0)
setname(m1 + 4, 'sdmat1')

# maps: set, copy, compare
check('CMaterial_SetMap', lambda: (SDTest.Call('CMaterial_SetMap', 'tiiiz', m1, 0, 'sdnone/a.bmp')[0] != 0, matget(m1)))
check('CMaterial_SetMap NULL', lambda: (SDTest.Call('CMaterial_SetMap', 'tiiiz', m1, 1, None)[0], mmaps(m1)))
check('CMaterial_SetMap 3', lambda: SDTest.Call('CMaterial_SetMap', 'tiiiz', m1, 3, 'sdnone/c.tga')[0] != 0)
for i in range(5):
	ocall('CMaterial_GetMapName', sym('CMaterial_GetMapName'), 'tsii', m1, i)
check('Mat_Str map', lambda: SDTest.Call('Mat_Str', 'si', m1)[0])
map0 = SDTest.Peek(m1 + 0x9c, '@i')
for i in range(8, 15):
	SDTest.Call('crt_memcpy', 'iisi', map0 + i, chr(i * 3), 1)
mfloats(map0 + 0x10, (0.5, -1.0, 2.0, 3.0, 0.25, 0.0, -0.0, 7.0))
m2 = matnew('sdmat2')
check('CMaterial_SetMapCopy', lambda: (SDTest.Call('CMaterial_SetMapCopy', 'tiiii', m2, 2, map0)[0] != 0, mmaps(m2)))
mapc = SDTest.Peek(m2 + 0xa4, '@i')
check('CMapa_Equal same', lambda: SDTest.Call('CMapa_Equal', 'bii', map0, map0)[0])
check('CMapa_Equal NULL', lambda: (SDTest.Call('CMapa_Equal', 'bii', map0, 0)[0], SDTest.Call('CMapa_Equal', 'bii', 0, map0)[0],
	SDTest.Call('CMapa_Equal', 'bii', 0, 0)[0]))
check('CMapa_Equal copy', lambda: SDTest.Call('CMapa_Equal', 'bii', map0, mapc)[0])
def mapdiff(name, off, data):
	saved = SDTest.Peek(mapc + off, '@i')
	if type(data) == type(''):
		SDTest.Call('crt_memcpy', 'iisi', mapc + off, data, 1)
	else:
		mfloats(mapc + off, data)
	check('CMapa_Equal ' + name, lambda: SDTest.Call('CMapa_Equal', 'bii', map0, mapc)[0])
	mpoke(mapc + off, saved)
for i in range(8, 15):
	mapdiff('byte %d' % i, i, '\377')
for i in range(8):
	mapdiff('float %d' % i, 0x10 + 4 * i, (99.0,))
mapdiff('-0', 0x28, (0.0,))
mapdiff('nan', 0x2c, (nan,))
setname(mapc, 'sdnone/other.bmp')
check('CMapa_Equal name', lambda: SDTest.Call('CMapa_Equal', 'bii', map0, mapc)[0])
check('CMapa_Assign', lambda: (SDTest.Call('CMapa_Assign', 'tvii', mapc, map0), mapget(mapc)))
for off in (0x14, 0x18, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x40):
	mpoke(m1 + off, icolor())
mfloats(m1 + 0x20, (1.5, 0.75))
# deletes mapc: m1 has no map 2
check('CMaterial_Assign', lambda: (SDTest.Call('CMaterial_Assign', 'tvii', m2, m1), matget(m2)))
check('CMaterial_SetMapCopy NULL', lambda: (SDTest.Call('CMaterial_SetMapCopy', 'tiiii', m2, 0, 0)[0], mmaps(m2)))
check('CMaterial_Assign again', lambda: (SDTest.Call('CMaterial_Assign', 'tvii', m2, m1), matget(m2)))

# the map getters on maps without textures, then with fake values
ocall('CMapa_GetSize', sym('CMapa_GetSize'), 'tbipp', map0, 0, 0)
ocall('CMaterial_GetMapSize', sym('CMaterial_GetMapSize'), 'tbiipp', m1, 0, 0, 0)
ocall('CMaterial_GetMapSize none', sym('CMaterial_GetMapSize'), 'tbiipp', m1, 1, 0, 0)
ocall('CMapa_GetTexture', sym('CMapa_GetTexture'), 'tiii', map0, 0)
mpoke(map0 + 0x30, map0 + 0x34)
mpoke(map0 + 0x34, 1234)
ocall('CMapa_GetTexture tex', sym('CMapa_GetTexture'), 'tiii', map0, 0)
mpoke(map0 + 0x34, 0)
mpoke(map0 + 0x38, 77)
ocall('CMapa_GetTexture hint', sym('CMapa_GetTexture'), 'tiii', map0, 0)
mpoke(map0 + 0x38, 0)
ocall('CMapa_GetTexture empty', sym('CMapa_GetTexture'), 'tiii', map0, 1)
mpoke(map0 + 0x30, 0)
ocall('CMaterial_GetMultiSprite none', sym('CMaterial_GetMultiSprite'), 'tiii', m1, 1)
mpoke(map0 + 0x5c, 4321)
ocall('CMaterial_GetMultiSprite', sym('CMaterial_GetMultiSprite'), 'tiii', m1, 0)
ocall('CMapa_BuildMultiSprite', sym('CMapa_BuildMultiSprite'), 'tbi', map0)
mpoke(map0 + 0x5c, 0)
mpoke(m1 + 0x90, 17)
ocall('CMaterial_Id', sym('CMaterial_Id'), 'tii', m1)
mpoke(m1 + 0x90, -1)

# CTextEst and CObjRef on heap memory; the hints are NULL, so Textura_Release does nothing
est = mz(12)
mpoke(est, 55)
check('CTextEst_dtor', lambda: (SDTest.Call('CTextEst_dtor', 'tvi', est), tpeek(est, 3)))
mpoke(est, 56)
check('CTextEst_dtor 2', lambda: (SDTest.Call('CTextEst_dtor@234c20', 'tvi', est), tpeek(est, 3)))
mpoke(est, 57)
check('CTextEst_Delete', lambda: (SDTest.Call('CTextEst_Delete', 'tiii', est, 0)[0] == est, tpeek(est, 3)))
ocall('CObjRef_dtor', sym('CObjRef_dtor'), 'tvi', est)
mfree(est)
arr = mz(4 + 8 * 3)
mpoke(arr, 3)
for i in range(3):
	mpoke(arr + 4 + 8 * i, 100 + i)
check('CTextEst_Delete array', lambda: (SDTest.Call('CTextEst_Delete', 'tiii', arr + 4, 2)[0] == arr, tpeek(arr, 7)))
check('CTextEst_Delete array free', lambda: SDTest.Call('CTextEst_Delete', 'tiii', arr + 4, 3)[0] == arr)

# CMapa_Release with frames and an AnimMat (empty destructor)
map2 = mapnew('sdmap2')
arr = mz(4 + 8 * 2)
mpoke(arr, 2)
mpoke(map2 + 0x48, arr + 4)
mpoke(map2 + 0x4c, mz(0x2c))
mpoke(map2 + 0x30, map2 + 0x34)
mpoke(map2 + 0x3c, 1)
mpoke(map2 + 0x40, 2)
mpoke(map2 + 0x58, 2)
check('CMapa_Release', lambda: (SDTest.Call('CMapa_Release', 'tvi', map2), mapget(map2)))
mpoke(map0 + 0x30, map0 + 0x34)
mpoke(map0 + 0x58, 2)
check('CMaterial_Release', lambda: (SDTest.Call('CMaterial_Release', 'tvi', m1), mmaps(m1)))

# compare and sort order; the address difference of the materials decides last
mpoke(m1 + 0x38, 0)
mpoke(m2 + 0x38, 0)
setatrib(m1, 0)
setatrib(m2, 0)
def cmpres(r):
	if r < -1 or r > 1:
		return 'address'
	return r
def mcmp(name, a, b):
	check('Mat_Compare ' + name, lambda a=a, b=b: cmpres(SDTest.Call('Mat_Compare', 'iii', a, b)[0]))
	check('Mat_Compare ' + name + ' swapped', lambda a=a, b=b: cmpres(SDTest.Call('Mat_Compare', 'iii', b, a)[0]))
mcmp('self', m1, m1)
mcmp('maps', m1, m2)
for off, v in ((0x38, 0x10000), (0x38, 0x1000000), (0x3c, 0x20)):
	mpoke(m2 + off, v)
	mcmp('%x %x' % (off, v), m1, m2)
	mpoke(m2 + 0x38, 0)
	setatrib(m2, 0)
mpoke(m2 + 0xa0, map2)
mcmp('map1', m1, m2)
mpoke(m2 + 0x38, 0x1000000)
mcmp('map1 b3b', m1, m2)
mpoke(m2 + 0x38, 0)
mpoke(m2 + 0xa0, 0)
saved3 = SDTest.Peek(m2 + 0xa8, '@i')
mpoke(m2 + 0xa8, 0)
mcmp('map3', m1, m2)
mpoke(m2 + 0xa8, saved3)
mpoke(m1 + 0xac, map2)
mcmp('map4', m1, m2)
mpoke(m1 + 0xac, 0)
saved0 = SDTest.Peek(m2 + 0x9c, '@i')
setname(saved0, 'sdnone/b.bmp')
mcmp('map0 name', m1, m2)
setname(saved0, 'sdnone/a.bmp')
mpoke(m2 + 0x9c, 0)
mcmp('map0', m1, m2)
mpoke(m1 + 0x9c, 0)
mcmp('no map0', m1, m2)
mpoke(m1 + 0x9c, map0)
mpoke(m2 + 0x9c, saved0)
setatrib(m1, 0x100)
setatrib(m2, 0x100)
check('Mat_CompareNames', lambda: SDTest.Call('Mat_CompareNames', 'bii', m1, m2)[0])
setname(m2 + 4, 'sdmat1')
check('Mat_CompareNames same', lambda: SDTest.Call('Mat_CompareNames', 'bii', m1, m2)[0])
setatrib(m1, 0)
setatrib(m2, 0)
check('Mat_CompareNames unnamed', lambda: SDTest.Call('Mat_CompareNames', 'bii', m1, m2)[0])
setname(m2 + 4, 'sdmat2')

# material lists of their own (the vftable of a list is not used by these functions)
l1 = mz(0x10)
l2 = mz(0x10)
m3 = matnew('sdmat3')
m4 = matnew('sdsub')
m7 = matnew('sdmat7')
objs = (m1, m2, m3, m4, m7, l1, l2)
def lget(l):
	r = []
	p = SDTest.Peek(l + 8, '@i')
	while p and len(r) < 10:
		r.append(who(p, objs))
		p = SDTest.Peek(p + 0x44, '@i')
	return r, who(SDTest.Peek(l + 0xc, '@i'), objs)
def lstep(name, m, sub):
	check(name, lambda m=m, sub=sub: (who(SDTest.Call('CListaMat_Extract', 'tiiii', l1, m, sub)[0], objs), lget(l1), lget(l2)))
def lfind(name, m):
	check(name, lambda m=m: who(SDTest.Call('CListaMat_Find', 'tiii', l1, m)[0], objs))
for m in (m1, m2, m3):
	check('CListaMat_Add', lambda m=m: (SDTest.Call('CListaMat_Add', 'tvii', l1, m), lget(l1), matget(m, objs)[0:6]))
check('CListaMat_Add sub', lambda: (SDTest.Call('CListaMat_Add', 'tvii', l2, m4), lget(l2)))
mpoke(m3 + 0xc, l2)
for s in (0, 1):
	check('CListaMat_Count' + fmt((s,)), lambda s=s: SDTest.Call('CListaMat_Count', 'tiii', l1, s)[0])
probe = matnew('sdprobe')
lfind('CListaMat_Find', probe)
SDTest.Call('CMaterial_Assign', 'tvii', probe, m1)
lfind('CListaMat_Find m1', probe)
mfloats(probe + 0x24, (nan,))
lfind('CListaMat_Find nan', probe)
mfloats(m1 + 0x24, (nan,))
lfind('CListaMat_Find nan both', probe)
mfloats(m1 + 0x24, (0.75,))
mfloats(probe + 0x24, (0.75,))
setatrib(m1, 0x100)
setatrib(probe, 0x100)
setname(probe + 4, 'sdprobe')
lfind('CListaMat_Find named', probe)
setname(probe + 4, 'sdmat1')
lfind('CListaMat_Find named same', probe)
setatrib(m1, 0)
setatrib(probe, 0)
mpoke(probe + 0x10, 0x01020304)
lfind('CListaMat_Find none', probe)
SDTest.Call('CMaterial_SetMap', 'tiiiz', probe, 0, None)
SDTest.Call('CMaterial_SetMap', 'tiiiz', probe, 3, None)
matdel(probe)
check('CListaMat_Release', lambda: (SDTest.Call('CListaMat_Release', 'tvi', l1), mmaps(m1), mmaps(m2)))
lstep('CListaMat_Extract none', m4, 0)
lstep('CListaMat_Extract sub', m4, 1)
check('CListaMat_Count after', lambda: SDTest.Call('CListaMat_Count', 'tiii', l1, 1)[0])
lstep('CListaMat_Extract middle', m2, 0)
check('CListaMat_Add back', lambda: (SDTest.Call('CListaMat_Add', 'tvii', l1, m2), lget(l1)))
lstep('CListaMat_Extract first', m1, 0)
lstep('CListaMat_Extract last', m2, 0)
check('CListaMat_Add m4', lambda: (SDTest.Call('CListaMat_Add', 'tvii', l1, m4), lget(l1)))
mpoke(m3 + 0xc, 0)
setatrib(m4, 0x8)
check('CListaMat_Delete', lambda: (SDTest.Call('CListaMat_Delete', 'tvii', l1, m4), lget(l1), SDTest.Peek('g_TexDeleteUnused', 'i')))
check('CListaMat_Add m7', lambda: (SDTest.Call('CListaMat_Add', 'tvii', l1, m7), lget(l1)))
check('CListaMat_DeleteAll', lambda: (SDTest.Call('CListaMat_DeleteAll', 'tvi', l1), lget(l1)))
check('CListaMat_Count empty', lambda: SDTest.Call('CListaMat_Count', 'tiii', l1, 1)[0])

# Prepare on maps whose files are missing (no texture is loaded), the id set so that none is registered
m5 = matnew('sdmat5')
mpoke(m5 + 0x90, 3)
SDTest.Call('CMaterial_SetMap', 'tiiiz', m5, 0, 'sdnone/p0.bmp')
SDTest.Call('CMaterial_SetMap', 'tiiiz', m5, 1, 'sdnone/p1.tga')
SDTest.Call('CMaterial_SetMap', 'tiiiz', m5, 3, 'sdnone/p3.tga')
check('CMaterial_Prepare', lambda: (SDTest.Call('CMaterial_Prepare', 'tvii', m5, 0), matget(m5)))
check('CMaterial_Prepare again', lambda: (SDTest.Call('CMaterial_Prepare', 'tvii', m5, 0), matget(m5)))
mpoke(l1 + 8, m5)
mpoke(l1 + 0xc, m5)
mpoke(m2 + 0x90, 4)
mpoke(m5 + 0xc, l2)
mpoke(l2 + 8, m2)
mpoke(l2 + 0xc, m2)
check('CListaMat_Prepare', lambda: (SDTest.Call('CListaMat_Prepare', 'tvii', l1, 0), mmaps(m5), matget(m2)))
mpoke(m5 + 0xc, 0)
mpoke(l1 + 8, 0)
mpoke(l1 + 0xc, 0)
mpoke(l2 + 8, 0)
mpoke(l2 + 0xc, 0)

# the files of the maps: pack files, missing files, the texture paths and extensions (saved and put back)
img = 'models/elements/templeyakuzzi/aceite-r.bmp'
def slots(rva):
	r = []
	for i in range(10):
		r.append(SDTest.Peek(rva + i * 0x104, 's'))
	return r
def restore(rva, saved):
	for i in range(10):
		SDTest.Call('Str_Copy', 'sisi', rva + 0x400000 + i * 0x104, saved[i], 0x104)
paths = slots(sym('g_RutasTexturas'))
extens = slots(sym('g_ExtensTexturas'))
call('Tex_ClearPaths', sym('Tex_ClearPaths'), 'v')
call('Tex_ClearExtensions', sym('Tex_ClearExtensions'), 'v')
check('Tex cleared', lambda: (slots(sym('g_RutasTexturas')), slots(sym('g_ExtensTexturas'))))
for p in (None, '', 'sdnone/', 'models/elements/templeyakuzzi/'):
	check('Tex_AddPath' + fmt((p,)), lambda p=p: (SDTest.Call('Tex_AddPath', 'vz', p), slots(sym('g_RutasTexturas'))))
for e in (None, '', 'tga', 'a&b', '<U><D>sdx/<N>.dds', '<D>&<N>'):
	check('Tex_AddExtension' + fmt((e,)), lambda e=e: (SDTest.Call('Tex_AddExtension', 'vz', e), slots(sym('g_ExtensTexturas'))))
for n in (img, 'aceite-r.bmp', 'templeyakuzzi/aceite-r.bmp', 'sdnone.bmp', 'x/2d/white.tga'):
	call('Tex_FindInPaths', sym('Tex_FindInPaths'), 'isb', n, '')
	for a in (0, 1):
		call('Tex_FindDDS', sym('Tex_FindDDS'), 'isbi', n, '', a)
		for f in ('&U&D&N&E', '&N&E', '2d/&N&E', 'sdx/&N.dds'):
			call('Tex_TryFormat', sym('Tex_TryFormat'), 'bssbi', f, n, '', a)
# with the engine variables DDS_MarkSource or DDS_CheckMark set a load changes file attributes: then only load 0
mkok = not SDTest.Peek('g_DDSMarkSource', 'i') and not SDTest.Peek('g_DDSCheckMark', 'i')
rp = mapnew('x')
for n in (img, '2d/white.tga', 'aceite-r.bmp', 'sdnone.bmp', 'sdnone/q.tga', 'levels/menu/map/map3d.0001.lmap1.tga'):
	for load in (0, 1):
		if load and not mkok:
			continue
		setname(rp, n)
		check('CMapa_ResolvePath' + fmt((n, load)), lambda load=load: (SDTest.Call('CMapa_ResolvePath', 'tsiii', rp, load, 0)[0], mapget(rp)))
		check('CMapa_ResolvePath alpha' + fmt((n, load)), lambda load=load: (SDTest.Call('CMapa_ResolvePath', 'tsiii', rp, load, map0)[0], mapget(rp)))
		if SDTest.Peek(rp + 4, '@i'):
			check('CMapa_FramePath' + fmt((n, load)), lambda: SDTest.Call('CMapa_FramePath', 'tsiibi', rp, 3, '', 20)[0:2])
setname(rp + 0x44, 'sdani%04d.tga')
mpoke(rp + 0x3c, 1)
for fr in (0, 7, 12345):
	ocall('CMapa_FramePath frame', sym('CMapa_FramePath'), 'tsiibi', rp, fr, '', 260)
ocall('CMapa_FramePath short', sym('CMapa_FramePath'), 'tsiibi', rp, 1, '', 5)
mpoke(rp + 0x3c, 0)
# LoadTexture stops after the file with a lightmap and no device, or skips a lightmap level above the limit
if mkok:
	mpoke(rp + 8, 0x100)
	for n in (img, 'sdnone.bmp'):
		setname(rp, n)
		mpoke(rp + 0x58, 0)
		check('CMapa_LoadTexture lightmap' + fmt((n,)), lambda: (SDTest.Call('CMapa_LoadTexture', 'tviiii', rp, 0, 0, 0), mapget(rp)))
	if SDTest.Peek('g_MaxLightmap', 'i') < 255:
		mpoke(rp + 8, 0xff00)
		setname(rp, img)
		mpoke(rp + 0x58, 0)
		check('CMapa_LoadTexture skip', lambda: (SDTest.Call('CMapa_LoadTexture', 'tviiii', rp, 1, 0, 0), mapget(rp)))
	check('CMapa_LoadTexture missing', lambda: (SDTest.Call('CMapa_LoadTexture', 'tviiii', rp, 1, 0, 0), mapget(rp)))
	mpoke(rp + 8, 0)
SDTest.Call('CMapa_dtor', 'tvi', rp)
mfree(rp)
restore(sym('g_RutasTexturas'), paths)
restore(sym('g_ExtensTexturas'), extens)
check('Tex restored', lambda: (slots(sym('g_RutasTexturas')) == paths, slots(sym('g_ExtensTexturas')) == extens))

# the average color of map 0 as the diffuse color
m6 = matnew('sdmat6')
SDTest.Call('CMaterial_SetMap', 'tiiiz', m6, 0, img)
check('CMaterial_MapToColor', lambda: (SDTest.Call('CMaterial_MapToColor', 'tvi', m6), matget(m6)))
check('CMaterial_MapToColor none', lambda: (SDTest.Call('CMaterial_MapToColor', 'tvi', m6), matget(m6)))

# DDS_Hide and DDS_Unhide on a file of the test (the results only go to the log), unhidden at the end
writefile('sdmat.txt', 'x')
for rva, n in ((sym('DDS_Hide'), 'DDS_Hide'), (sym('DDS_Unhide'), 'DDS_Unhide'), (sym('DDS_Unhide'), 'DDS_Unhide'), (sym('DDS_Hide'), 'DDS_Hide')):
	call(n, rva, 'vsi', 'sdmat.txt', 2)
	call(n + ' missing', rva, 'vsi', 'sdnofile.txt', 2)
call('DDS_Unhide', sym('DDS_Unhide'), 'vsi', 'sdmat.txt', 2)

for m in (m1, m2, m5, m6):
	mpoke(m + 8, 0)
	mpoke(m + 0xc, 0)
	mpoke(m + 0x44, 0)
	mpoke(m + 0x48, 0)
	matdel(m)
for p in (fake, map2):
	SDTest.Call('CMapa_dtor', 'tvi', p)
	mfree(p)
mfree(l1)
mfree(l2)
