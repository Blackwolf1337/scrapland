# engine3d Engine/Fuentes.c: the two debug fonts measured and drawn through the D3D log (the viewport set), the
# unicode font loaded into a table made here and its escapes, a grid font and the digits, the border rows of a bitmap.
# Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt and rdlog come from there.
def fni(a):
	return SDTest.Peek(a, '@i')
def fnget(f):
	r = []
	for o in range(0, 0x1c, 4):
		r.append(fni(f + o))
	r = r + [SDTest.Peek(f + 0x1c, '@b'), SDTest.Peek(f + 0x1d, '@b'), fni(f + 0x30) != 0, fni(f + 0x34),
		SDTest.Peek(f + 0x3c, '@b'), SDTest.Peek(f + 0x3d, '@b'), fni(f + 0x40), fni(f + 0x44)]
	p = fni(f + 0x38)
	for i in (0, 1, fni(f + 0x34) - 1):
		r.append((fni(p + 16 * i), fni(p + 16 * i + 4), fni(p + 16 * i + 8), fni(p + 16 * i + 12)))
	g = fni(f + 0x30)
	for o in (0, 4, 0x10, 0x14, 0x18, 0x1c, 0x28, 0x2c, 0x30, 0x34, 0x38):
		r.append(fni(g + o))
	return r
def fnpokef(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
fnout = SDTest.Call('operator_new', 'ii', 16)[0]
def fnmedir(f, s, align, pos, dim):
	SDTest.Call('crt_memcpy', 'iiFi', fnout, (-1.0, -1.0, -1.0, -1.0), 16)
	p = pos and fnout or 0
	d = dim and fnout + 8 or 0
	SDTest.Call('CFont3_Measure', 'tviffiiii', f, 100.0, 50.0, align, s, p, d)
	return (fni(fnout), fni(fnout + 4), fni(fnout + 8), fni(fnout + 12))
rdvp = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
# '\001' + '0041' is not in the list: its color code reads past the end of the string (heap memory)
strs = ('', 'Hola', 'Scrapland 1.6 ~', '\001' + '00e9x', '\001' + 'zz12y', '\005ab\037', '\001' + '04')
for name, a in (('Comic14', sym('g_FntComic14')), ('Term8', sym('g_FntTerm8'))):
	f = SDTest.Peek(a, 'i')
	check('CFont3 ' + name, lambda f=f: fnget(f))
	for s in strs:
		sb = SDTest.Call('operator_new', 'ii', len(s) + 1)[0]
		for i in range(len(s)):
			SDTest.Poke(sb + i - 0x400000, ord(s[i]) | (SDTest.Peek(sb + i, '@i') & ~0xff))
		SDTest.Poke(sb + len(s) - 0x400000, SDTest.Peek(sb + len(s), '@i') & ~0xff)
		for align in (0, 1, 2, 3, 4, 5):
			check('CFont3_Measure' + fmt((name, s, align)), lambda f=f, sb=sb, align=align: (fnmedir(f, sb, align, 1, 1),
				fnmedir(f, sb, align, 1, 0), fnmedir(f, sb, align, 0, 1)))
		for x, y in ((10.0, 20.0), (10.4, 20.6)):
			check('CFont3_Print' + fmt((name, s, x, y)), lambda f=f, sb=sb, x=x, y=y: (rdlog(lambda f=f, sb=sb,
				x=x, y=y: SDTest.Call('CFont3_Print', 'tviffi', f, x, y, sb)), fni(f), fni(f + 4)))
		for onda in (1.0, 1.25):
			check('CFont3_PrintWave' + fmt((name, s, onda)), lambda f=f, sb=sb, onda=onda: rdlog(lambda f=f,
				sb=sb, onda=onda: SDTest.Call('CFont3_PrintWave', 'tviffifi', f, 30.0, 40.0, 0x80ff00ff, onda, sb)))
		for flags in (0, 1, 2, 3, 4, 0x10004, 0x40004, 0x50000):
			check('CFont3_PrintAlign' + fmt((name, s, flags)), lambda f=f, sb=sb, flags=flags: rdlog(lambda f=f,
				sb=sb, flags=flags: SDTest.Call('CFont3_PrintAlign', 'tviffiii', f, 200.0, 100.0, flags, 0xff00ff00, sb)))
		check('CFont3_PrintColor' + fmt((name, s)), lambda f=f, sb=sb: rdlog(lambda f=f, sb=sb:
			SDTest.Call('CFont3_PrintColor', 'tviffii', f, 5.0, 6.0, 0x12345678, sb)))
	g = SDTest.Peek(f + 0x38, '@i')
	check('CFont3_GlyphWidth/Alto ' + name, lambda f=f, g=g: (SDTest.Call('CFont3_GlyphWidth', 'tfii', f, g)[0],
		SDTest.Call('CFont3_GlyphHeight', 'tfii', f, g + 16)[0]))
	check('Font_SetColor/SetFlags ' + name, lambda f=f: (SDTest.Call('Font_SetColor', 'tiii', f, 0x00ff00)[0],
		SDTest.Call('Font_SetFlags', 'tiii', f, 0x20000)[0], fni(f + 0x10), fni(f + 0x14)))
	for flags in (0, 2, 8, 10):
		check('Font_Printf' + fmt((name, flags)), lambda f=f, flags=flags: rdlog(lambda f=f, flags=flags:
			SDTest.Call('Font_Printf', 'viffiss', f, 12.0, 34.0, flags, 'v%s', 'abc')))
	check('Font_PrintfXY ' + name, lambda f=f: rdlog(lambda f=f: SDTest.Call('Font_PrintfXY', 'viffsi', f, 7.0, 8.0, 'n=%d', 42)))
	check('Font_PrintfCenter ' + name, lambda f=f: rdlog(lambda f=f: SDTest.Call('Font_PrintfCenter', 'viffs', f, 320.0, 240.0, 'mid')))
	pos = SDTest.Call('operator_new', 'ii', 12)[0]
	fnpokef(pos, (0.0, 0.0, 10.0))
	check('Debug_Text3D ' + name, lambda f=f, pos=pos: rdlog(lambda f=f, pos=pos: SDTest.Call('Debug_Text3D', 'viffiis', f,
		4.0, -12.0, pos, 0x8000, 'dbg')))
	SDTest.Call('Font_SetColor', 'tiii', f, 0xffffff)
	SDTest.Call('Font_SetFlags', 'tiii', f, 0)
	g = fni(f + 0x30)
	rdesc = (fni(g + 0x18), fni(g + 0x1c))
	check('CFont3_SetScale ' + name, lambda f=f, g=g: (SDTest.Call('CFont3_SetScale', 'tviff', f, 0.5, 2.0) and 0, fni(g + 0x18),
		fni(g + 0x1c)))
	SDTest.Poke(g + 0x18 - 0x400000, rdesc[0])
	SDTest.Poke(g + 0x1c - 0x400000, rdesc[1])

# the unicode font into a table of its own
u = SDTest.Call('operator_new', 'ii', 16)[0]
for i in range(4):
	SDTest.Poke(u + 4 * i - 0x400000, 0)
def unget(u):
	r = [fni(u), fni(u + 4) != 0, fni(u + 8) != 0, fni(u + 12) != 0]
	ms = fni(u + 12)
	if ms:
		r = r + [fni(ms), fni(ms + 4), fni(ms + 8)]
	return r
check('CUnifont_Load', lambda u=u: (SDTest.Call('CUnifont_Load', 'tvi', u), unget(u)))
outw = SDTest.Call('operator_new', 'ii', 4)[0]
for s in ('\001' + '00e9', '\001' + '0041', '\001' + 'FFFF', '\001' + '00g1', 'x0041', '\001' + '20ac', '\001' + '0410', '\001' + '044f'):
	sb = SDTest.Call('operator_new', 'ii', 8)[0]
	SDTest.Call('crt_memcpy', 'iisi', sb, s, len(s) + 1)
	check('CUnifont_Parse' + fmt((s,)), lambda u=u, sb=sb: (SDTest.Call('CUnifont_Parse', 'tbiii', u, sb, outw)[0],
		fni(outw) & 0xffff))
if fni(u + 12):
	n = fni(fni(u + 12) + 8)
	for i in (0, 1, n / 2, n - 1, n, n + 5):
		check('CUnifont_Width/SetSprite' + fmt((i,)), lambda u=u, i=i: (SDTest.Call('CUnifont_Width', 'tfiif', u, i, 12.0)[0],
			SDTest.Call('CUnifont_SetSprite', 'tbiiff', u, i, 8.0, 12.0)[0], fni(fni(u + 8) + 0x10), fni(fni(u + 8) + 0x14),
			fni(fni(u + 8) + 0x28), fni(fni(u + 8) + 0x34)))
check('CUnifont_Free', lambda u=u: (SDTest.Call('CUnifont_Free', 'tvi', u), unget(u)))

# a grid font of 16 by 16 pixel cells and the digits
fg = SDTest.Call('CFontGrid_ctor', 'tiissiiiif', SDTest.Call('operator_new', 'ii', 0x5c)[0], 'bmp/rgb_white.tga', 'bmp/rgb_grey.tga',
	0x20, 0x7f, 16, 32, 9.5)[0]
check('CFontGrid_ctor', lambda fg=fg: (fni(fg + 0x20) & 0xffff, fni(fg + 0x44) & 0xffff, fni(fg + 0x48), fni(fg + 0x4c),
	fni(fg + 0x50), fni(fg + 0x54), fni(fg + 0x58)))
check('CFontGrid_Print', lambda fg=fg: (rdlog(lambda fg=fg: SDTest.Call('CFontGrid_Print', 'tviffs', fg, 3.0, 4.0, 'AZ az~\177')),
	fni(fg), fni(fg + 0x28), fni(fg + 0x2c), fni(fg + 0x30), fni(fg + 0x34)))
SDTest.Call('CGraphic2D_dtor', 'tvi', fg)
dg = SDTest.Call('CDigits_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0x2b4)[0])[0]
check('CDigits_ctor', lambda dg=dg: (fni(dg + 0x20) & 0xffff, fni(dg + 0x44 * 9 + 0x20) & 0xffff, fni(dg + 0x2a8),
	fni(dg + 0x2ac), fni(dg + 0x2b0), fni(dg + 0x3c) != 0))
SDTest.Call('CDigits_dtor', 'tvi', dg)

# the border rows: 5 pixels, the border color with other alphas
bm = SDTest.Call('operator_new', 'ii', 12)[0]
px = SDTest.Call('operator_new', 'ii', 40)[0]
for i, v in ((0, 0x11223344), (1, 0xff223344), (2, 0x00223344), (3, 0x11223345), (4, 0x11223344), (5, 0x00223344),
	(6, 0x00223344), (7, 0x00223344), (8, 0x00223344), (9, 0x00223344)):
	SDTest.Poke(px + 4 * i - 0x400000, v)
for o, v in ((0, 5), (4, 2), (8, px)):
	SDTest.Poke(bm + o - 0x400000, v)
for y in (0, 1):
	for c in (0x223344, 0x99223344, 0x223345):
		check('Bmp_RowBorder' + fmt((y, c)), lambda bm=bm, y=y, c=c: SDTest.Call('Bmp_RowBorder', 'iiii', bm, y, c)[0])
SDTest.Poke('g_ViewportW', rdvp[0])
SDTest.Poke('g_ViewportH', rdvp[1])

# the two debug fonts made again from their .MST files and freed
for tex, opac, flags in (('bmp/fnt/Comic14.tga', 'bmp/fnt/A_Comic14.tga', 0), ('bmp/fnt/Term8.alpha.tga', None, 8)):
	nf = SDTest.Call('operator_new', 'ii', 0x48)[0]
	check('CFont3_ctor' + fmt((tex, flags)), lambda nf=nf, tex=tex, opac=opac, flags=flags: (SDTest.Call('CFont3_ctor',
		'tiisziifiif', nf, tex, opac, 0x20, 0xff, 0.0, 0, flags, 1.0)[0] == nf, fnget(nf)))
	SDTest.Call('CFont3_dtor', 'tvi', nf)
	check('CFont3_dtor' + fmt((tex,)), lambda nf=nf: (fni(nf + 0x30), fni(nf + 0x34), fni(nf + 0x38)))
