# ScrapEng/ScrapEngFont.c and Fonts.c: the fonts of the game by name, their sizes of texts and the printf variants
# through the D3D log (the viewport set), a font loaded again from Fonts.fnt. Python 1.5.2, runs inside sdcall.py
# after sdcall_fuentes.py: check, fmt, rdlog, fni and fnget come from there.
def cfget(c):
	r = []
	for o in range(0, 0x30, 4):
		r.append(fni(c + o))
	return r + [fni(c + 0x30) != 0]
rdvp = (SDTest.Peek('g_ViewportW', 'i'), SDTest.Peek('g_ViewportH', 'i'))
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
cfout = SDTest.Call('operator_new', 'ii', 16)[0]
names = ('Terminal8', 'Terminal8Fixed', 'BigNumbers', 'ScrapBig', 'ScrapMedium', 'Horatio', 'HoratioSmall', 'terminal8',
	'None')
for name in names:
	check('Font_Get' + fmt((name,)), lambda name=name: SDTest.Call('Font_Get', 'is', name)[0] == SDTest.Call('Font_Get', 'is',
		name)[0] and SDTest.Call('Font_Get', 'is', name)[0] != 0)
check('Fonts_Unifont', lambda: SDTest.Call('Fonts_Unifont', 'b')[0])
def cfsave(c):
	return cfget(c)
def cfrestore(c, v):
	for i in range(12):
		SDTest.Poke(c + 4 * i - 0x400000, v[i])
for name in ('Terminal8', 'Terminal8Fixed', 'ScrapBig', 'Horatio'):
	c = SDTest.Call('Font_Get', 'is', name)[0]
	saved = cfsave(c)
	check('CFont ' + name, lambda c=c: (cfget(c), fnget(fni(c + 0x30))))
	for s in ('Score 120', 'A', ''):
		for x, y in ((0.0, 0.0), (100.0, 50.0)):
			check('Font_TextSize' + fmt((name, s, x, y)), lambda c=c, s=s, x=x, y=y: (SDTest.Call('Font_TextSize',
				'viiiffss', c, cfout, cfout + 8, x, y, '%s', s), fni(cfout), fni(cfout + 4), fni(cfout + 8),
				fni(cfout + 12)))
		SDTest.Poke(c + 0x10 - 0x400000, 4)
		check('Font_TextSizeAlign' + fmt((name, s)), lambda c=c, s=s: (SDTest.Call('Font_TextSizeAlign', 'viiiffiss', c, cfout,
			cfout + 8, 30.0, 40.0, 1, '%s', s), fni(cfout), fni(cfout + 4)))
	for ef in (0, 0x10000, 0x20000, 0x40000, 0x70000):
		SDTest.Poke(c + 0x20 - 0x400000, ef)
		SDTest.Call('crt_memcpy', 'iiFi', c + 0x24, (1.2, 2.0, 3.0), 12)
		for flags in (0, 1, 2, 8, 11):
			check('CFont_PrintfFlags' + fmt((name, ef, flags)), lambda c=c, flags=flags: (rdlog(lambda c=c,
				flags=flags: SDTest.Call('CFont_PrintfFlags', 'viffisi', c, 20.0, 30.0, flags, 'n %d', 7)), cfget(c)))
	cfrestore(c, saved)
	calls = (('CFont_Printf', sym('CFont_Printf'), 'viss', (c, '%s!', 'hola')),
		('CFont_PrintfAlign', sym('CFont_PrintfAlign'), 'viffiss', (c, 10.0, 20.0, 4, '%s', 'al'))	,
		('CFont_PrintfAlignColor', sym('CFont_PrintfAlignColor'), 'viffiiss', (c, 10.0, 20.0, 1, 0xff00ff, '%s', 'ac')),
		('CFont_PrintfAlignColorShadow', sym('CFont_PrintfAlignColorShadow'), 'viffiiss', (c, 10.0, 20.0, 2, 0xff0000, '%s', 'acs')),
		('CFont_PrintfColor', sym('CFont_PrintfColor'), 'viffiss', (c, 10.0, 20.0, 0xff00, '%s', 'co')),
		('CFont_PrintfColorShadow', sym('CFont_PrintfColorShadow'), 'viffiss', (c, 10.0, 20.0, 0xff, '%s', 'cs')),
		('CFont_PrintfShadow', sym('CFont_PrintfShadow'), 'viffss', (c, 10.0, 20.0, '%s', 'so')),
		('CFont_PrintfXY', sym('CFont_PrintfXY'), 'viffss', (c, 5.0, 6.0, '%s', 'xy')),
		('CFont_PrintXY', sym('CFont_PrintXY'), 'tviffs', (c, 7.0, 8.0, 'pxy')),
		('CFont_Print', sym('CFont_Print'), 'tvis', (c, 'p')))
	for cname, rva, sig, args in calls:
		check(cname + ' ' + name, lambda c=c, rva=rva, sig=sig, args=args: (rdlog(lambda rva=rva, sig=sig, args=args:
			apply(SDTest.Call, (rva, sig) + args)), cfget(c)))
		cfrestore(c, saved)
	pos = SDTest.Call('operator_new', 'ii', 12)[0]
	SDTest.Call('crt_memcpy', 'iiFi', pos, (0.0, 0.0, 10.0), 12)
	check('CFont_Printf3D ' + name, lambda c=c, pos=pos: (rdlog(lambda c=c, pos=pos: SDTest.Call('CFont_Printf3D', 'viiiss', c,
		pos, 3, '%s', '3d')), cfget(c)))
	cfrestore(c, saved)
	check('CFont_ResetEffects/ResetX ' + name, lambda c=c: (SDTest.Call('CFont_ResetEffects', 'tvi', c), SDTest.Call('CFont_ResetX', 'tvi',
		c), cfget(c)))
	cfrestore(c, saved)

# a font made again from Fonts.fnt and freed
ini = SDTest.Call('CIniFile_ctor', 'tii', SDTest.Call('operator_new', 'ii', 4)[0])[0]
check('CIniFile_Load Fonts.fnt', lambda ini=ini: SDTest.Call('CIniFile_Load', 'tiiszi', ini, '2d/fonts-drm/Fonts.fnt', None, 0)[0])
for name in ('Horatio', 'BigNumbers', 'Terminal8Fixed'):
	nc = SDTest.Call('operator_new', 'ii', 0x34)[0]
	check('CFont_ctor/Load' + fmt((name,)), lambda nc=nc, name=name, ini=ini: (SDTest.Call('CFont_ctor', 'tiizi', nc, None,
		0)[0] == nc, SDTest.Call('CFont_Load', 'tvisii', nc, name, 0, ini), cfget(nc), fnget(fni(nc + 0x30))))
	SDTest.Call('CFont_dtor', 'tvi', nc)
	check('CFont_dtor' + fmt((name,)), lambda nc=nc: fni(nc + 0x30))
SDTest.Call('CIniFile_dtor', 'tvi', ini)
SDTest.Poke('g_ViewportW', rdvp[0])
SDTest.Poke('g_ViewportH', rdvp[1])
