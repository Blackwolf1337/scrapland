# game Scorer/SpriteScorer.c .. WindFxScorer.c, MenuDraw.c (execfile'd by sdlevel.py, Python 1.5.2): the items of
# the third scorer module built through SScorer.Add for player 0 (Sprite, Gradient, Status, Tab, Talk, Target, Text,
# TextTyping, Timer, Waypoint, Weapon, WindFx): their constructor fields, the script variables through SScorer.Set
# and Get, the drawing through the D3D log (viewport 640x480), the tab cells on the white image (frame 0) and the
# tab frame on the sheet 2D/Menu/Tabs.alpha.tga, the menu bar, frame and stretch helpers, the talk functions, the
# text edit start and end, the native OnRender functions of the menu items through a cPy_Function on the heap. No
# sprite files are set (a plain tga as a multisprite sheet is a CHECK); gets of HighRes and of the blending modes
# CHECK the int SScorer.Get leaves unset and are left out. The scorer times and the engine variables the functions
# change are saved and put back; the items are deleted at the end (Clear, then RemoveDeleted).
def sc3i(a):
	return SDTest.Peek(a, '@i')
def sc3b(a):
	return SDTest.Peek(a, '@b')
def sc3ints(a, offs):
	r = ()
	for o in offs:
		r = r + (sc3i(a + o),)
	return r
def sc3run(a, n):
	r = ()
	for k in range(n):
		r = r + (sc3i(a + 4 * k),)
	return r
# the same with the words at the indices ptrs (heap addresses) only as set or not
def sc3runp(a, n, ptrs):
	r = ()
	for k in range(n):
		v = sc3i(a + 4 * k)
		if k in ptrs:
			v = v != 0
		r = r + (v,)
	return r
def sc3fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def sc3setf(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def sc3copy(dst, src, n):
	SDTest.Call('crt_memcpy', 'iiii', dst, src, n)
def sc3log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
def sc3srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def sc3new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def sc3free(p):
	SDTest.Call('operator_delete', 'vi', p)
def sc3get(name, var):
	return SScorer.Get(0, name, var)
def sc3set(name, var, v):
	return SScorer.Set(0, name, var, v)
def sc3find(name):
	return SDTest.Call('cScorer_FindItem', 'tiis', sc3sc, name)[0]
def sc3save(rvas):
	return map(lambda a: SDTest.Peek(a, 'i'), rvas)
def sc3restore(rvas, vals):
	for sc3k in range(len(rvas)):
		SDTest.Poke(rvas[sc3k], vals[sc3k])
# a heap copy of n bytes at the address a, put back with sc3putback
def sc3keep(a, n):
	b = sc3new(n)
	sc3copy(b, a, n)
	return (a, b, n)
def sc3putback(k):
	sc3copy(k[0], k[1], k[2])
	sc3free(k[1])

sc3sc = SDTest.Call('Scorer_Get', 'ii', 0)[0]
sc3world = SDTest.Peek('g_pWorld', 'i')
check('SC3 scorer', lambda: (sc3sc != 0, sc3world != 0))
# engine variables: the talk, the timers, the menu natives, the viewport
sc3gvars = (sym('g_RemainTalkTime'), sym('g_TalkTime'), sym('g_TalkNumLines'), sym('g_TalkColor'), sym('g_TalkColor') + 0x4, sym('g_TalkColor') + 0x8, sym('g_TimeToCancel'), sym('g_TimeToFinish'), sym('g_TimeInactive'), sym('g_TimeSpecialUse'),
	sym('g_TimerMode'), sym('g_TimerWarning'), sym('GVar_TimeWarning') + 0xc, sym('g_MetroRing'), sym('g_FadeOutFinalAcc'), sym('g_FadeOutFinalTime'), sym('g_ViewportW'), sym('g_ViewportH'), sym('g_TextEditItem'), sym('g_TextEditCursor'))
sc3gsaved = sc3save(sc3gvars)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
if sc3sc:
	sc3scsaved = sc3ints(sc3sc, (0x20, 0x24, 0x28, 0x50, 0xa4, 0x100))
	sc3setf(sc3sc + 0x20, (12.5, 12.25, 0.05))
	SDTest.Poke(sc3sc + 0x100 - 0x400000, 0)
sc3horatio = SDTest.Call('Font_Get', 'is', 'Horatio')[0]
sc3tabs = SDTest.Peek('g_ScorerTabs', 'i')

# ---- the items: the vtable the constructor set and its fields ----
sc3types = (('sc3Sprite', 'Sprite'), ('sc3Grad', 'Gradient'), ('sc3Status', 'Status'), ('sc3Tab', 'Tab'),
	('sc3Talk', 'Talk'), ('sc3Target', 'Target'), ('sc3Text', 'Text'), ('sc3Typing', 'TextTyping'),
	('sc3Timer', 'Timer'), ('sc3Way', 'Waypoint'), ('sc3Weapon', 'Weapon'), ('sc3Wind', 'WindFx'),
	('sc3Ring0', 'Sprite'), ('sc3Ring1', 'Sprite'), ('sc3Ring2', 'Sprite'), ('sc3Ring3', 'Sprite'))
sc3item = {}
for sc3name, sc3type in sc3types:
	sc3srand(4321)
	check('SC3 Add ' + sc3name, lambda n=sc3name, t=sc3type: SScorer.Add(0, n, t))
	sc3a = 0
	if sc3sc:
		sc3a = sc3find(sc3name)
	sc3item[sc3name] = sc3a
	check('SC3 vftable ' + sc3name, lambda a=sc3a: a and sc3i(a) - 0x400000)
# int fields, byte fields and pointer fields (printed != 0) the constructors write
sc3ctor = (
	('sc3Sprite', (0x10, 0x14, 0xac, 0xb0, 0xb4), (0xb8,), (0xbc,)),
	('sc3Grad', (0xac, 0xb0, 0xb4, 0xc4, 0xc8, 0xcc, 0xd0, 0xd4), (0xb8,), (0xbc,)),
	('sc3Status', (0x10, 0x14, 0x15c, 0x160, 0x170, 0x14c, 0x188, 0x198, 0x1a8, 0x1ac, 0x1b0, 0x1bc, 0x1d4, 0x1e0,
		0x1e4, 0x1f4, 0x1f8), (0xb8, 0xf8, 0x12c), (0x130, 0x134, 0x1e8)),
	('sc3Tab', (0x10, 0x14, 0xc8, 0xcc, 0xd4, 0xd8, 0xe0, 0xe4, 0xe8), (), (0xec,)),
	('sc3Talk', (0xac, 0xb0, 0xb4, 0xb8, 0x1a5c, 0x1a60, 0x1a68, 0x1a6c, 0x1a7c, 0x1a78), (0xbc, 0x1a70), (0x1a64,)),
	('sc3Target', (0xac, 0xb0, 0x144), (0xb4,), (0x134,)),
	('sc3Text', (0xc8, 0xcc), (0xd0, 0xd1, 0xd8), (0xc4,)),
	('sc3Typing', (0xc8, 0xcc, 0xdc, 0xe0, 0xe4, 0xec), (0xd0, 0xd1, 0xd8, 0xe8, 0xe9, 0xf0), (0xc4,)),
	('sc3Timer', (0xc4,), (0xd8, 0xec), ()),
	('sc3Way', (0xac, 0xbc), (0xb0,), (0xc0,)),
	('sc3Weapon', (0x10, 0x14, 0xb8, 0x1a0, 0x1a4, 0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1bc, 0x1c0), (), (0xac, 0xb0,
		0x1b8)),
	('sc3Wind', (0xb0, 0xb4, 0xb8, 0xbc), (0xcc,), (0xac,)))
for sc3name, sc3offs, sc3boffs, sc3poffs in sc3ctor:
	sc3a = sc3item[sc3name]
	if sc3a:
		check('SC3 ctor ' + sc3name, lambda a=sc3a, o=sc3offs, b=sc3boffs, p=sc3poffs: (sc3ints(a, o),
			map(lambda x, a=a: sc3b(a + x), b), map(lambda x, a=a: sc3i(a + x) != 0, p)))
for sc3name, sc3off in (('sc3Sprite', 0xbc), ('sc3Status', 0x130), ('sc3Status', 0x134), ('sc3Status', 0x1e8),
	('sc3Tab', 0xec), ('sc3Talk', 0x1a64), ('sc3Target', 0x134), ('sc3Text', 0xc4), ('sc3Way', 0xc0),
	('sc3Weapon', 0xac), ('sc3Weapon', 0xb0), ('sc3Weapon', 0x1b8), ('sc3Wind', 0xac)):
	sc3a = sc3item[sc3name]
	if sc3a:
		check('SC3 ctor image %s %x' % (sc3name, sc3off), lambda a=sc3a, o=sc3off: sc3i(sc3i(a + o) + 0x34))
for sc3name, sc3off in (('sc3Status', 0x158), ('sc3Status', 0x16c), ('sc3Status', 0x194), ('sc3Talk', 0x1a74),
	('sc3Weapon', 0xb4)):
	sc3a = sc3item[sc3name]
	if sc3a:
		check('SC3 ctor font %s %x' % (sc3name, sc3off), lambda a=sc3a, o=sc3off: sc3i(a + o) == sc3horatio)
for sc3name, sc3off in (('sc3Status', 0xac), ('sc3Status', 0xec), ('sc3Wind', 0xc0)):
	sc3a = sc3item[sc3name]
	if sc3a:
		check('SC3 ctor sound %s %x' % (sc3name, sc3off), lambda a=sc3a, o=sc3off: (SDTest.Peek(a + o + 0x1c,
			'@s'), sc3b(a + o + 0xc), sc3i(a + o + 0x3c) != 0))
check('SC3 tab back text width', lambda: sc3fl(sym('g_TabBackTextWidth') + 0x400000, 1))

# ---- the script variables: set, then read back ----
sc3colors = (('Red', 11), ('Green', 22), ('Blue', 33), ('Alpha', 200))
sc3vars = (
	('sc3Sprite', (('FixPosition', 1), ('JumpFX', 2.5), ('ScaleX', 1.5), ('ScaleY', 0.5), ('PivotX', 0.25),
		('PivotY', 0.75), ('IsMultiSprite', 0), ('SpriteIndex', 1), ('HighRes', 0), ('BlendingMode', 2),
		('Unit', 3), ('Mirror', 1), ('Rotate', 2), ('Discardable', 5), ('Discardable', -1), ('X', 100.0),
		('Y', 50.0)) + sc3colors, ('FixPosition', 'JumpFX', 'ScaleX', 'ScaleY', 'PivotX', 'PivotY',
		'IsMultiSprite', 'SpriteIndex', 'Unit', 'Mirror', 'Rotate', 'Discardable', 'File', 'Red', 'Green', 'Blue',
		'Alpha', 'X', 'Missing')),
	('sc3Grad', (('ColorSelected', 2), ('Red', 1), ('Green', 2), ('Blue', 3), ('Alpha', 4), ('ColorSelected', 0),
		('Red', 5), ('Alpha', 6), ('JumpFX', 1.0), ('X', 30.0), ('Y', 40.0), ('W', 50.0), ('H', 60.0)),
		('ColorSelected', 'Red', 'Green', 'Blue', 'Alpha', 'JumpFX', 'ScaleX')),
	('sc3Status', (('BackIndex', 1), ('BackOffX', 1.5), ('BackOffY', 2.5), ('LifeIconIndex', 2), ('LifeIconOffX', 3.5),
		('LifeIconOffY', 4.5), ('LifeTextOffX', 5.5), ('LifeTextOffY', 6.5), ('LifeFont', 'Horatio'),
		('LifeFont', 'sc3NoFont'), ('LifeTextRed', 1), ('LifeTextGreen', 2), ('LifeTextBlue', 3),
		('LifeTextAlpha', 4), ('LivesTextOffX', 7.5), ('LivesTextOffY', 8.5), ('LivesFont', 'ScrapMedium'),
		('LivesTextRed', 5), ('LivesTextGreen', 6), ('LivesTextBlue', 7), ('LivesTextAlpha', 8),
		('MoneyBackIndex', 3), ('MoneyBackOffX', 9.5), ('MoneyBackOffY', 10.5), ('MoneyIconIndex', 4),
		('MoneyIconOffX', 11.5), ('MoneyIconOffY', 12.5), ('MoneyTextOffX', 13.5), ('MoneyTextOffY', 14.5),
		('MoneyFont', 'sc3NoFont'), ('MoneyTextRed', 9), ('MoneyTextGreen', 10), ('MoneyTextBlue', 300),
		('MoneyTextAlpha', 12), ('BoostStatus', 0x80), ('BoostRemaining', 0.25), ('BoostOnRender', ''),
		('BoostOffX', 15.5), ('BoostOffY', 16.5), ('BoostIconIndex', 5), ('BoostTurboIndex', 6),
		('HullIconOffX', 17.5), ('HullIconOffY', 18.5), ('HullIconIndex', 7), ('FontBackIndex', 8),
		('FontBackOffX', 19.5), ('FontBackOffY', 20.5), ('LivesBackIndex', 9), ('LivesBackOffX', 21.5),
		('LivesBackOffY', 22.5), ('FlagOffX', 23.5), ('FlagOffY', 24.5), ('X', 10.0)),
		('BackIndex', 'BackOffX', 'BackOffY', 'LifeIconIndex', 'LifeIconOffX', 'LifeIconOffY', 'LifeTextOffX',
		'LifeTextOffY', 'LifeTextRed', 'LifeTextGreen', 'LifeTextBlue', 'LifeTextAlpha', 'LivesTextOffX',
		'LivesTextOffY', 'LivesTextRed', 'LivesTextGreen', 'LivesTextBlue', 'LivesTextAlpha', 'MoneyBackIndex',
		'MoneyBackOffX', 'MoneyBackOffY', 'MoneyIconIndex', 'MoneyIconOffX', 'MoneyIconOffY', 'MoneyTextOffX',
		'MoneyTextOffY', 'MoneyTextRed', 'MoneyTextGreen', 'MoneyTextBlue', 'MoneyTextAlpha', 'BoostStatus',
		'BoostRemaining', 'BoostOffX', 'BoostOffY', 'BoostIconIndex', 'BoostTurboIndex', 'HullIconOffX',
		'HullIconOffY', 'HullIconIndex', 'FontBackIndex', 'FontBackOffX', 'FontBackOffY', 'LivesBackIndex',
		'LivesBackOffX', 'LivesBackOffY', 'FlagOffX', 'FlagOffY', 'LifeFont', 'X', 'Missing')),
	('sc3Tab', (('ScaleX', 1.0), ('ScaleY', 1.0), ('HighRes', 0), ('BlendingMode', 3), ('TabSpriteIndex', 0),
		('Unit', 0), ('SizeX', 100.0), ('SizeY', 75.0), ('TabInit', 1), ('TabEnd', 4), ('TabMax', 5),
		('SizeTabQuad', 2), ('Type', 0), ('Filled', 1), ('X', 100.0), ('Y', 120.0)) + sc3colors[:3] +
		(('Alpha', 255),), ('ScaleX', 'ScaleY', 'TabSpriteIndex', 'Unit', 'SizeX', 'SizeY', 'TabInit', 'TabEnd',
		'TabMax', 'SizeTabQuad', 'Type', 'Filled', 'Red', 'Green', 'Blue', 'Alpha', 'Missing')),
	('sc3Talk', (('NextIndex', 0), ('NextBackIndex', 0), ('UseIndex', 0), ('TabBlendingMode', 4),
		('TabSpriteIndex', 0), ('TabRed', 1), ('TabGreen', 2), ('TabBlue', 3), ('TabAlpha', 4), ('TabVisible', 2),
		('X', 320.0), ('Y', 300.0)), ('NextIndex', 'NextBackIndex', 'UseIndex', 'TabSpriteIndex', 'TabRed',
		'TabGreen', 'TabBlue', 'TabAlpha', 'TabVisible', 'Missing')),
	('sc3Target', (('TargetRectIndex', 1), ('TargetArrowIndex', 2), ('EnemyFireIndex', 3)),
		('TargetRectIndex', 'TargetArrowIndex', 'EnemyFireIndex', 'X')),
	('sc3Text', (('Text', 'sc3 text'), ('EditHint', 'sc3 edit hint'), ('CenterEdit', 4), ('IsNumeric', 0),
		('MaxInput', 6), ('Font', 'Horatio'), ('X', 100.0), ('Y', 200.0)), ('Text', 'EditHint', 'CenterEdit',
		'IsNumeric', 'MaxInput', 'W', 'H', 'Missing')),
	('sc3Typing', (('Text', 'sc3 typing text, typed in'), ('TypingSpeed', 0.125), ('TypingTime', 0.75),
		('CursorAtInit', 0), ('CursorAtEnd', 1), ('TypingSound', 1), ('MaxInput', 4), ('X', 50.0), ('Y', 60.0)),
		('CursorAtInit', 'CursorAtEnd', 'TypingSound', 'MaxInput', 'Text',
		'Missing')),
	('sc3Timer', (('OnTimeExpired', ''), ('OnTimeWarning', ''), ('TimerType', 'TimeToFinish'), ('Text', 'sc3'),
		('X', 300.0), ('Y', 20.0)), ('Text', 'X', 'OnTimeExpired', 'Missing')),
	('sc3Way', (('WaypointRectIndex', 1), ('WaypointArrowIndex', 2), ('WaypointDotIndex', 3)),
		('WaypointRectIndex', 'WaypointArrowIndex', 'WaypointDotIndex', 'Y')),
	('sc3Weapon', (('BackIndex', 1), ('BackOffX', 1.5), ('BackOffY', 2.5), ('WeaponIconOffX', 3.5),
		('WeaponIconOffY', 4.5), ('WeaponIconShadowOffX', 5.5), ('WeaponIconShadowOffY', 6.5), ('WeaponIconAc', 3),
		('WeaponIconIndex', 7), ('UpgradeIconAc', 12), ('UpgradeIconIndex', 8), ('FontBackIndex', 9),
		('FontBackOffX', 7.5), ('FontBackOffY', 8.5), ('AmmoBackIndex', 10), ('AmmoBackOffX', 9.5),
		('AmmoBackOffY', 10.5), ('AmmoIconAc', 4), ('AmmoIconIndex', 11), ('AmmoIconOffX', 11.5),
		('AmmoIconOffY', 12.5), ('AmmoBarIndex', 12), ('AmmoBarOffX', 13.5), ('AmmoBarOffY', 14.5),
		('AmmoBarRed', 1), ('AmmoBarGreen', 2), ('AmmoBarBlue', 3), ('AmmoBarAlpha', 4), ('AmmoTextOffX', 15.5),
		('AmmoTextOffY', 16.5), ('AmmoFont', 'sc3NoFont'), ('AmmoFont', 'Horatio'), ('AmmoTextRed', 5),
		('AmmoTextGreen', 6), ('AmmoTextBlue', 7), ('AmmoTextAlpha', 8), ('UpgradeBarIndex', 13),
		('UpgradeBarOffX', 17.5), ('UpgradeBarOffY', 18.5), ('UpgradeBarRed', 9), ('UpgradeBarGreen', 10),
		('UpgradeBarBlue', 11), ('UpgradeBarAlpha', 12), ('SlotsOffX', 19.5), ('SlotsOffY', 20.5),
		('SlotsIndex0', 14), ('SlotsIndex1', 15), ('TabBlendingMode', 5), ('TabSpriteIndex', 0), ('TabRed', 13),
		('TabGreen', 14), ('TabBlue', 15), ('TabAlpha', 16), ('WeaponIconAc', 0), ('UpgradeIconAc', 0),
		('AmmoIconAc', 0)), ('BackIndex', 'BackOffX', 'BackOffY', 'WeaponIconOffX', 'WeaponIconOffY',
		'WeaponIconShadowOffX', 'WeaponIconShadowOffY', 'WeaponIconAc', 'WeaponIconIndex', 'UpgradeIconAc',
		'UpgradeIconIndex', 'FontBackIndex', 'FontBackOffX', 'FontBackOffY', 'AmmoBackIndex', 'AmmoBackOffX',
		'AmmoBackOffY', 'AmmoIconAc', 'AmmoIconIndex', 'AmmoIconOffX', 'AmmoIconOffY', 'AmmoBarIndex',
		'AmmoBarOffX', 'AmmoBarOffY', 'AmmoBarRed', 'AmmoBarGreen', 'AmmoBarBlue', 'AmmoBarAlpha', 'AmmoTextOffX',
		'AmmoTextOffY', 'AmmoTextRed', 'AmmoTextGreen', 'AmmoTextBlue', 'AmmoTextAlpha', 'UpgradeBarIndex',
		'UpgradeBarOffX', 'UpgradeBarOffY', 'UpgradeBarRed', 'UpgradeBarGreen', 'UpgradeBarBlue',
		'UpgradeBarAlpha', 'SlotsOffX', 'SlotsOffY', 'SlotsIndex0', 'SlotsIndex1', 'TabSpriteIndex', 'TabRed',
		'TabGreen', 'TabBlue', 'TabAlpha', 'Missing')),
	('sc3Wind', (('X', 5.0), ('Visible', 0)), ('X', 'Visible', 'Missing')))
for sc3name, sc3sets, sc3gets in sc3vars:
	for sc3v, sc3val in sc3sets:
		check('SC3 Set' + fmt((sc3name, sc3v, sc3val)), lambda n=sc3name, v=sc3v, x=sc3val: sc3set(n, v, x))
	for sc3v in sc3gets:
		check('SC3 Get' + fmt((sc3name, sc3v)), lambda n=sc3name, v=sc3v: sc3get(n, v))
# the fields behind the variables
sc3a = sc3item['sc3Grad']
if sc3a:
	check('SC3 Gradient colors', lambda a=sc3a: sc3ints(a, (0xc4, 0xc8, 0xcc, 0xd0, 0xd4)))
sc3a = sc3item['sc3Status']
if sc3a:
	check('SC3 Status fields', lambda a=sc3a: (sc3runp(a + 0x138, 0x2f, (8, 13, 23, 44)), sc3i(a + 0x158) == sc3horatio,
		sc3i(a + 0x16c) == SDTest.Call('Font_Get', 'is', 'ScrapMedium')[0], sc3i(a + 0x194) == sc3horatio))
sc3a = sc3item['sc3Weapon']
if sc3a:
	check('SC3 Weapon fields', lambda a=sc3a: (sc3run(a + 0xb8, 0x3d), sc3i(a + 0xb4) == sc3horatio,
		sc3i(sc3i(a + 0x1b8) + 0x34)))
sc3a = sc3item['sc3Tab']
if sc3a:
	check('SC3 Tab fields', lambda a=sc3a: (sc3fl(a + 0xac, 4), sc3run(a + 0xbc, 12)))
	# the size to whole cells, the tab over the width when none is set
	for sc3hr, sc3w, sc3h, sc3ti, sc3te in ((0, 100.0, 75.0, 0, 0), (1, 49.0, 22.0, 0, 0), (0, 13.0, 300.0, 2, 0)):
		check('SC3 cTabScorer_UpdateSize' + fmt((sc3hr, sc3w, sc3h, sc3ti, sc3te)), lambda a=sc3a, hr=sc3hr,
			w=sc3w, h=sc3h, ti=sc3ti, te=sc3te: (SDTest.Poke(a + 0xc8 - 0x400000, hr), sc3setf(a + 0xb4, (w, h)),
			SDTest.Poke(a + 0xd4 - 0x400000, ti), SDTest.Poke(a + 0xd8 - 0x400000, te), SDTest.Call('cTabScorer_UpdateSize', 'tvi',
			a), sc3fl(a + 0xac, 4), sc3run(a + 0xbc, 9)))
sc3a = sc3item['sc3Text']
if sc3a:
	check('SC3 Text fields', lambda a=sc3a: (sc3fl(a + 0x18, 2), sc3i(a + 0xcc), sc3b(a + 0xd1), SDTest.Peek(a +
		0xd4, '@p')))
	for sc3n in (0, 255, 256, -1):
		if sc3n < 0:
			continue
		check('SC3 cTextScorer_Var MaxInput %d' % sc3n, lambda a=sc3a, n=sc3n: (SDTest.Call('cTextScorer_Var', 'tiisiiDp',
			a, 'MaxInput', 1, 0, 0.0, n)[0], sc3i(a + 0xcc), sc3fl(a + 0x18, 2)))
	sc3set('sc3Text', 'MaxInput', 6)
sc3a = sc3item['sc3Typing']
if sc3a:
	check('SC3 TextTyping fields', lambda a=sc3a: (sc3fl(a + 0xdc, 3), sc3ints(a, (0xcc, 0xec)), sc3b(a + 0xe8),
		sc3b(a + 0xe9), sc3b(a + 0xf0)))
sc3a = sc3item['sc3Timer']
if sc3a:
	for sc3v in ('TimeToCancel', 'TimeInactive', 'TimeSpecialUse', 'TimeToFinish'):
		check('SC3 Timer TimerType ' + sc3v, lambda a=sc3a, v=sc3v: (sc3set('sc3Timer', 'TimerType', v), sc3i(a +
			0xc4)))
sc3a = sc3item['sc3Talk']
if sc3a:
	check('SC3 Talk fields', lambda a=sc3a: (sc3ints(a, (0xb0, 0xb4, 0xb8, 0x1a68, 0x1a6c)), sc3b(a + 0x1a70),
		sc3i(sc3i(a + 0x1a64) + 0x34)))
sc3a = sc3item['sc3Wind']
if sc3a:
	check('SC3 WindFx Var', lambda a=sc3a: (SDTest.Call('cScorerItem_Var@188e90', 'tiisiiDp', a, 'Y', 1, 0, 7.5, 0),
		SDTest.Call('cScorerItem_Var@188e90', 'tiisiiDp', a, 'Y', 0, 0, 0.0, 0), SDTest.Call('cScorerItem_Var@188e90', 'tiisiiDp', a, 'sc3No', 0, 0,
		0.0, 0)))

# ---- Sprite and Gradient: the draws, the file, the multisprite slot ----
sc3a = sc3item['sc3Sprite']
if sc3a and sc3sc:
	sc3set('sc3Sprite', 'Alpha', 255)
	for sc3fix, sc3jump, sc3t in ((0, 0.0, 12.5), (1, 0.0, 12.5), (0, 2.5, 12.5), (1, 3.0, 7.25), (0, -1.5, 0.3)):
		sc3setf(sc3sc + 0x20, (sc3t,))
		check('SC3 cSpriteScorer_Draw' + fmt((sc3fix, sc3jump, sc3t)), lambda a=sc3a, f=sc3fix, j=sc3jump:
			(sc3set('sc3Sprite', 'FixPosition', f), sc3set('sc3Sprite', 'JumpFX', j), sc3log(lambda a=a:
			SDTest.Call('cSpriteScorer_Draw', 'tvii', a, 0))))
	sc3setf(sc3sc + 0x20, (12.5,))
	check('SC3 cSpriteScorer_SetMultiSprite', lambda a=sc3a: (SDTest.Call('cSpriteScorer_SetMultiSprite', 'tvii', a, 1), sc3b(a + 0xb2),
		SDTest.Call('cSpriteScorer_SetMultiSprite', 'tvii', a, 0), sc3b(a + 0xb2)))
	check('SC3 cSpriteScorer_SetFile', lambda a=sc3a: (SDTest.Call('cSpriteScorer_SetFile', 'tviis', a, 0x20, '2D/white.tga'),
		sc3i(a + 0xbc) != 0, sc3i(sc3i(a + 0xbc) + 0x34), sc3fl(sc3i(a + 0xbc) + 0x10, 2)))
	check('SC3 Sprite Discardable SetFile', lambda a=sc3a: (sc3set('sc3Sprite', 'Discardable', 1),
		SDTest.Call('cSpriteScorer_SetFile', 'tviis', a, 0x20, '2D/white.tga'), sc3i(a + 0xbc) != 0, sc3set('sc3Sprite',
		'Discardable', 0)))
sc3a = sc3item['sc3Grad']
if sc3a:
	check('SC3 cGradientScorer_Draw', lambda a=sc3a: (sc3log(lambda a=a: SDTest.Call('cGradientScorer_Draw', 'tvii', a, 0)),
		sc3fl(sc3i(a + 0xbc), 2), sc3i(sc3i(a + 0xbc) + 0x34)))

# ---- the tab: cells and rectangles on the white image (frame 0), the frame on the tab sheet ----
sc3a = sc3item['sc3Tab']
if sc3a:
	sc3img = sc3i(sc3a + 0xec)
	sc3set('sc3Tab', 'SizeX', 100.0)
	sc3set('sc3Tab', 'SizeY', 75.0)
	# rotations 0..4 only: a higher one reads past the corner table (the stack of the original)
	for sc3x, sc3y, sc3rot, sc3flip in ((0, 0, 0, 0), (2, 1, 1, 0), (3, 2, 2, 0), (1, 4, 3, 0), (5, 0, 4, 0),
		(0, 3, 2, 1), (2, 2, 0, 1), (4, 1, 3, 1), (6, 5, 1, 1)):
		check('SC3 cTabScorer_DrawCell' + fmt((sc3x, sc3y, sc3rot, sc3flip)), lambda a=sc3a, m=sc3img, x=sc3x,
			y=sc3y, r=sc3rot, f=sc3flip: (sc3log(lambda a=a, m=m, x=x, y=y, r=r, f=f: SDTest.Call('cTabScorer_DrawCell',
			'tviiiiiii', a, m, x, y, 0, r, f)), sc3fl(m, 2)))
	for sc3r in ((0, 0, 1, 1), (1, 2, 5, 4), (3, 1, 2, 6), (2, 2, 2, 2)):
		check('SC3 cTabScorer_DrawRect' + fmt(sc3r), lambda a=sc3a, m=sc3img, r=sc3r: (sc3log(lambda a=a, m=m, r=r:
			SDTest.Call('cTabScorer_DrawRect', 'tviiiiiii', a, m, r[0], r[1], r[2], r[3], 0)), sc3fl(m, 2)))
	# the frame needs the frames of a sheet: the tab sheet of the scorer, the types whose frames it has
	sc3set('sc3Tab', 'File', '2D/Menu/Tabs.alpha.tga')
	sc3img = sc3i(sc3a + 0xec)
	sc3nf = 0
	if sc3img:
		sc3nf = SDTest.Call('CPsaImage_GetNumFrames', 'tii', sc3img)[0]
	check('SC3 Tab sheet frames', lambda n=sc3nf: n)
	for sc3type, sc3need in ((4, 3), (0, 6), (1, 9), (2, 6), (3, 15)):
		if sc3need >= sc3nf:
			continue
		for sc3sq, sc3ti, sc3te, sc3fill in ((1, 0, 0, 1), (2, 1, 4, 0), (0, 0, 0, 1), (3, 2, 5, 1)):
			check('SC3 cTabScorer_Draw' + fmt((sc3type, sc3sq, sc3ti, sc3te, sc3fill)), lambda a=sc3a, t=sc3type,
				q=sc3sq, ti=sc3ti, te=sc3te, f=sc3fill: (sc3set('sc3Tab', 'Type', t), sc3set('sc3Tab', 'SizeTabQuad',
				q), sc3set('sc3Tab', 'Filled', f), SDTest.Poke(a + 0xd4 - 0x400000, ti), SDTest.Poke(a + 0xd8 -
				0x400000, te), sc3log(lambda a=a: SDTest.Call('cTabScorer_Draw', 'tvii', a, 0)), sc3run(a + 0xd4, 2)))
	sc3set('sc3Tab', 'Type', 0)

# ---- the menu helpers: the stretch and the frame on the tab sheet, the bar on a white image ----
if sc3tabs:
	sc3k = sc3keep(sc3tabs, 0x44)
	check('SC3 Menu_DrawStretch', lambda m=sc3tabs: (sc3log(lambda m=m: SDTest.Call('Menu_DrawStretch', 'viiffff', m, 1,
		100.0, 200.0, 150.0, 1.5)), sc3fl(m, 6)))
	for sc3r in ((52.0, 300.0, 536.0, 120.0), (10.5, 20.25, 14.0, 14.0), (0.0, 0.0, 3.0, 640.0)):
		check('SC3 Menu_DrawFrame' + fmt(sc3r), lambda m=sc3tabs, r=sc3r: (sc3log(lambda m=m, r=r: SDTest.Call(
			'Menu_DrawFrame', 'viiffff', m, 1, r[0], r[1], r[2], r[3])), sc3fl(m, 6)))
	sc3putback(sc3k)
sc3a = sc3item['sc3Ring3']
if sc3a:
	sc3img = sc3i(sc3a + 0xbc)
	for sc3mode, sc3frac, sc3f, sc3sx in ((0, 0.5, 0.0, 1.0), (1, 0.25, 0.1, 2.0), (2, 0.75, 0.2, 0.5),
		(3, 0.4, 0.3, 1.0), (4, 0.5, 0.0, 1.0), (5, 0.6, 0.15, 1.5), (6, 0.1, 0.25, 1.0), (7, 0.9, 0.05, 3.0),
		(0, 1.0, 0.0, 1.0), (4, 1.0, 0.1, 1.0), (1, 1.5, 0.0, 1.0), (0, -0.5, 0.0, 1.0)):
		check('SC3 Menu_DrawBar' + fmt((sc3mode, sc3frac, sc3f, sc3sx)), lambda m=sc3img, md=sc3mode, fr=sc3frac,
			f=sc3f, sx=sc3sx: (sc3setf(m + 0x10, (sx, 1.25)), SDTest.Poke(m + 0x34 - 0x400000, 0x11223344),
			sc3log(lambda m=m, md=md, fr=fr, f=f: SDTest.Call('Menu_DrawBar', 'viffffiffffffffii', m, 30.0, 40.0, fr,
			md, f, 0.0, 0.0, 0.5, 0.25, 0.125, 0.5, 0.75, 1.0, -0x7f00ff01, 0x40404040)), sc3fl(m, 2), sc3i(m +
			0x34)))
	check('SC3 Menu_DrawBar no colors', lambda m=sc3img: (sc3log(lambda m=m: SDTest.Call('Menu_DrawBar',
		'viffffiffffffffii', m, 30.0, 40.0, 0.5, 0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0, 0)), sc3i(m +
		0x34)))
	sc3setf(sc3img + 0x10, (1.0, 1.0))

# ---- the talk ----
sc3a = sc3item['sc3Talk']
sc3text = sc3keep(sym('g_TalkText') + 0x400000, 0x1900)
sc3lines = sc3keep(sym('g_TalkLines') + 0x400000, 0xa0)
check('SC3 Scorer_SetSpeechText', lambda: (SDTest.Call('Scorer_SetSpeechText', 'vsfiii', 'sc3 talk text with some words to wrap ' +
	'into a few lines of the talk, the width of the talk is the engine variable', 3.0, 0x11, 0x22, 0x33),
	sc3fl(sym('g_RemainTalkTime') + 0x400000, 1), sc3fl(sym('g_TalkTime') + 0x400000, 1), SDTest.Peek('g_TalkNumLines', 'i'),
	sc3run(sym('g_TalkColor') + 0x400000, 3), SDTest.Peek('g_TalkText', 's')))
check('SC3 Talk_IsShown', lambda: SDTest.Call('Talk_IsShown', 'i')[0])
sc3sets = SDTest.Peek('g_ActionSets', 'i')
sc3setok = 0
if sc3sets > 3:
	sc3set3 = SDTest.Peek(sym('g_ActionSets') + 0x4, 'i') + 3 * 0xc
	sc3setok = sc3i(sc3set3 + 4) > 0x11 and sc3i(sc3set3 + 8) != 0
check('SC3 talk action set', lambda: sc3setok)
if sc3a and sc3sc and sc3setok:
	SScorer.SetSpeechCallback('')
	sc3set('sc3Talk', 'TabVisible', 0)
	# with TabVisible 1 the tab sprite draw reads through a NULL in both runs: only 0
	for sc3t, sc3vis, sc3ml in ((3.0, 0, 3), (2.5, 0, 9), (0.005, 0, 3)):
		check('SC3 cTalkScorer_DrawTalk' + fmt((sc3t, sc3vis, sc3ml)), lambda a=sc3a, t=sc3t, v=sc3vis, ml=sc3ml:
			(sc3setf(sym('g_RemainTalkTime') + 0x400000, (t,)), sc3set('sc3Talk', 'TabVisible', v), SDTest.Poke(a + 0x1a7c -
			0x400000, ml), sc3log(lambda a=a: SDTest.Call('cTalkScorer_DrawTalk', 'tviiF', a, 0, (320.0, 300.0))),
			sc3fl(sym('g_RemainTalkTime') + 0x400000, 1), sc3fl(a + 0x1a60, 1)))
	SDTest.Poke(sc3a + 0x1a7c - 0x400000, 3)
	sc3setf(sym('g_RemainTalkTime') + 0x400000, (2.0,))
	check('SC3 cTalkScorer_Draw', lambda a=sc3a: (sc3log(lambda a=a: SDTest.Call('cTalkScorer_Draw', 'tvii', a, 0)),
		sc3fl(sym('g_RemainTalkTime') + 0x400000, 1)))
	check('SC3 cTalkScorer_DrawUse', lambda a=sc3a: SDTest.Call('cTalkScorer_DrawUse', 'tviiF', a, 0, (320.0, 300.0)))
check('SC3 Scorer_CancelSpeech', lambda: (SDTest.Call('Scorer_CancelSpeech', 'vf', 0.75), sc3fl(sym('g_RemainTalkTime') + 0x400000, 1),
	SDTest.Call('Talk_IsShown', 'i')[0], SDTest.Call('Scorer_CancelSpeech', 'vf', 0.0), SDTest.Call('Talk_IsShown', 'i')[0]))
# the callback with a list of parameters: a native one, run by hand on the alpha of a sprite item
sc3a = sc3item['sc3Ring0']
if sc3a and sc3sc:
	check('SC3 Scorer_SetSpeechCallback', lambda a=sc3a: (SScorer.SetSpeechCallback('!ScorerSpriteGlow', [0,
		'sc3Ring0', 2.5, None]), SDTest.Call('cPy_Function_IsValid', 'tbi', sym('g_TalkCallback') + 0x400000)[0], sc3i(sym('g_TalkCallback') + 0x400000 +
		0xc) != 0, SDTest.Call('cPy_Function_Execute', 'tiii', sym('g_TalkCallback') + 0x400000, 0)[0], sc3i(sc3i(a + 0xbc) + 0x34)))
	check('SC3 Scorer_SetSpeechCallback none', lambda: (SScorer.SetSpeechCallback(''), SDTest.Call('cPy_Function_IsValid',
		'tbi', sym('g_TalkCallback') + 0x400000)[0]))
sc3putback(sc3text)
sc3putback(sc3lines)
sc3restore(sc3gvars, sc3gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)

# ---- the native OnRender functions of the menu items: a cPy_Function on the heap, params player, item name ----
sc3fn = sc3new(0x14)
SDTest.Call('cPy_Function_ctor', 'tii', sc3fn)
def sc3native(name, item):
	SDTest.Call('cPy_Function_SetFunc', 'tbis', sc3fn, '!' + name)
	SDTest.Call('cPy_Function_AddParamI', 'tvii', sc3fn, 0)
	SDTest.Call('cPy_Function_AddParamS', 'tvis', sc3fn, item)
	return SDTest.Call('cPy_Function_Execute', 'tiii', sc3fn, 0)[0]
def sc3alpha(name):
	a = sc3item[name]
	return (sc3i(sc3i(a + 0xbc) + 0x34), sc3b(a + 0x29), sc3i(a + 0x6c + 8) != 0)
if sc3sc and sc3item['sc3Ring0']:
	for sc3t, sc3last, sc3a0 in ((12.5, 12.49, 0), (12.5, 12.2, 100), (12.5, 12.0, 250), (12.5, 12.5, 254)):
		sc3setf(sc3sc + 0x20, (sc3t, sc3last))
		check('SC3 ScorerSpriteFadeIn' + fmt((sc3t, sc3last, sc3a0)), lambda a0=sc3a0: (sc3set('sc3Ring0', 'Alpha',
			a0), sc3set('sc3Ring0', 'OnRender', '!ScorerSpriteGlow'), sc3native('ScorerSpriteFadeIn', 'sc3Ring0'),
			sc3alpha('sc3Ring0')))
		check('SC3 ScorerSpriteFadeOut' + fmt((sc3t, sc3last, sc3a0)), lambda a0=sc3a0: (sc3set('sc3Ring1', 'Alpha',
			a0), sc3native('ScorerSpriteFadeOut', 'sc3Ring1'), sc3alpha('sc3Ring1')))
	for sc3t, sc3last, sc3acc, sc3ft in ((12.5, 12.4, 0.0, 2.0), (12.5, 12.0, 1.5, 2.0), (12.5, 12.4, 0.0, 0.0)):
		sc3setf(sc3sc + 0x20, (sc3t, sc3last))
		sc3setf(sym('g_FadeOutFinalAcc') + 0x400000, (sc3acc,))
		sc3setf(sym('g_FadeOutFinalTime') + 0x400000, (sc3ft,))
		check('SC3 ScorerSpriteFadeOutFinal' + fmt((sc3t, sc3last, sc3acc, sc3ft)), lambda: (sc3native(
			'ScorerSpriteFadeOutFinal', 'sc3Ring2'), sc3alpha('sc3Ring2'), sc3fl(sym('g_FadeOutFinalAcc') + 0x400000, 1)))
	for sc3t in (12.5, 0.01, 0.3, 1.0, 3.75):
		sc3setf(sc3sc + 0x20, (sc3t,))
		check('SC3 ScorerSpriteFlash' + fmt((sc3t,)), lambda: (sc3native('ScorerSpriteFlash', 'sc3Ring3'),
			sc3alpha('sc3Ring3')))
		check('SC3 ScorerSpriteGlow' + fmt((sc3t,)), lambda: (sc3native('ScorerSpriteGlow', 'sc3Ring3'),
			sc3alpha('sc3Ring3')))
	# the rings: the last letter of the name is the ring
	for sc3ring, sc3dt in ((0.0, 0.25), (0.9, 0.25), (1.5, 0.75), (-0.5, 0.25), (0.5, 0.0)):
		sc3setf(sym('g_MetroRing') + 0x400000, (sc3ring,))
		sc3setf(sc3sc + 0x28, (sc3dt,))
		check('SC3 ScorerMetroUsrHere' + fmt((sc3ring, sc3dt)), lambda: (sc3native('ScorerMetroUsrHere',
			'sc3RingX'), sc3fl(sym('g_MetroRing') + 0x400000, 1), map(lambda n: (sc3alpha(n)[0], sc3fl(sc3i(sc3item[n] +
			0xbc) + 0x10, 2)), ('sc3Ring0', 'sc3Ring1', 'sc3Ring2', 'sc3Ring3'))))
	check('SC3 natives no item', lambda: (sc3native('ScorerSpriteFadeIn', 'sc3NoItem'), sc3native(
		'ScorerSpriteGlow', 'sc3NoItem'), sc3native('ScorerMetroUsrHere', 'sc3NoItemX')))
	# BoostScorer reads the ship of the camera in the ship action set only
	check('SC3 action set', lambda: SDTest.Call('Input_GetActionSet', 'i')[0])
	if SDTest.Call('Input_GetActionSet', 'i')[0] != 1:
		check('SC3 BoostScorer', lambda: (sc3native('BoostScorer', 'sc3Status'), sc3ints(sc3item['sc3Status'],
			(0x1cc, 0x1d0))))
	sc3setf(sc3sc + 0x20, (12.5, 12.25, 0.05))
SDTest.Call('cPy_Function_dtor', 'tvi', sc3fn)
sc3free(sc3fn)
sc3restore(sc3gvars, sc3gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)

# ---- the texts: the draw as the cursor item or not, the edit through its native start, the typing ----
sc3a = sc3item['sc3Text']
if sc3a and sc3sc:
	check('SC3 cTextScorer_Draw', lambda a=sc3a: (sc3log(lambda a=a: SDTest.Call('cTextScorer_Draw', 'tvii', a, 0)),
		sc3ints(sc3i(a + 0xb8), (0x10, 0x14, 0x20))))
	SDTest.Call('cScorer_SetDefault', 'tvis', sc3sc, 'sc3Text')
	for sc3t in (12.5, 13.0):
		sc3setf(sc3sc + 0x20, (sc3t,))
		check('SC3 cTextScorer_Draw cursor' + fmt((sc3t,)), lambda a=sc3a: (sc3log(lambda a=a: SDTest.Call(
			'cTextScorer_Draw', 'tvii', a, 0)), sc3ints(sc3i(a + 0xb8), (0x10, 0x14, 0x20))))
	sc3tabsk = sc3keep(sc3tabs, 0x44)
	sc3set('sc3Text', 'OnAccept', '!EditText')
	for sc3ce in (0, 1):
		check('SC3 cTextScorer_Draw edit cursor' + fmt((sc3ce,)), lambda a=sc3a, c=sc3ce: (sc3set('sc3Text',
			'CenterEdit', c), sc3log(lambda a=a: SDTest.Call('cTextScorer_Draw', 'tvii', a, 0)), sc3i(sc3tabs + 0x34)))
	SDTest.Call('cScorer_SetDefault', 'tvis', sc3sc, 'sc3None')
	check('SC3 cTextScorer_Draw edit', lambda a=sc3a: sc3log(lambda a=a: SDTest.Call('cTextScorer_Draw', 'tvii', a, 0)))
	sc3putback(sc3tabsk)
	sc3setf(sc3sc + 0x20, (12.5,))
	# the edit: the player input, the virtual keyboard and the edit globals are put back after it
	sc3pin = sc3keep(sym('g_PlayerInput') + 0x400000, 0xb4)
	sc3kb = sc3keep(sym('g_VirtualKeyboard') + 0x400000, 0x10c)
	sc3fn = sc3new(0x14)
	SDTest.Call('cPy_Function_ctor', 'tii', sc3fn)
	check('SC3 EditText', lambda a=sc3a: (sc3native('EditText', 'sc3Text'), sc3b(a + 0xd0), sc3i(sym('g_TextEditItem') +
		0x400000) == a, sc3i(sc3sc + 0x50), SDTest.Peek(sym('g_PlayerInput') + 0x8, 's'), SDTest.Peek('g_PlayerInput', 'i'),
		SDTest.Peek(sym('g_PlayerInput') + 0x98, 'i') - 0x400000, sc3fl(a + 0xc8, 1), SDTest.Peek(sc3sc + 0x30, '@p'),
		SDTest.Peek('g_VirtualKeyboard', 's')))
	SDTest.Call('cPy_Function_dtor', 'tvi', sc3fn)
	sc3free(sc3fn)
	check('SC3 TextEdit_Done', lambda a=sc3a: (SDTest.Call('TextEdit_Done', 'vii', 0, 0), sc3b(a + 0xd0),
		SDTest.Peek(a + 0xc0, '@p'), sc3i(sc3sc + 0x50) == SDTest.Peek('g_TextEditCursor', 'i'), sc3i(sc3sc + 0xa4),
		SDTest.Peek(sym('g_PlayerInput') + 0x98, 'i'), SDTest.Peek(sc3sc + 0x30, '@p')))
	sc3putback(sc3pin)
	sc3putback(sc3kb)
	sc3set('sc3Text', 'OnAccept', '')
	SDTest.Poke(sc3sc + 0xa4 - 0x400000, sc3scsaved[4])
sc3a = sc3item['sc3Typing']
if sc3a and sc3sc:
	for sc3t, sc3last, sc3tt, sc3init, sc3end in ((12.5, 12.4, 0.0, 1, 1), (12.5, 12.0, 0.75, 0, 1), (13.0, 12.5,
		0.5, 1, 0), (12.5, 0.0, 9.0, 1, 1), (12.5, 12.4, -1.0, 1, 1), (12.75, 12.5, 2.0, 0, 0)):
		sc3setf(sc3sc + 0x20, (sc3t,))
		sc3srand(99)
		check('SC3 cTextTypingScorer_Draw' + fmt((sc3t, sc3last, sc3tt, sc3init, sc3end)), lambda a=sc3a, l=sc3last,
			tt=sc3tt, i=sc3init, e=sc3end: (sc3setf(a + 0xdc, (l, tt)), sc3set('sc3Typing', 'CursorAtInit', i),
			sc3set('sc3Typing', 'CursorAtEnd', e), sc3log(lambda a=a: SDTest.Call('cTextTypingScorer_Draw', 'tvii', a, 0)),
			sc3fl(a + 0xdc, 2), sc3i(a + 0xec)))
	sc3setf(sc3sc + 0x20, (12.5,))

# ---- the timer ----
sc3a = sc3item['sc3Timer']
# the delta of the world timer (the frame time, different in every run) fixed for the count
sc3delta = sc3world + 0x2c8 + 0xc
if sc3a and sc3world:
	sc3deltasaved = sc3i(sc3delta)
	sc3setf(sc3delta, (0.125,))
	for sc3type, sc3mode, sc3time, sc3warn, sc3flags in ((1, 0, 75.5, -1.0, 0), (0, 1, 3.25, 10.0, 0),
		(2, 0, 5.0, -1.0, 0), (3, 2, 600.0, -1.0, 0), (1, 0, 0.0, -1.0, 0), (1, 0, 0.01, 1.0, 0),
		(0, 0, 125.0, 200.0, 1)):
		check('SC3 cTimerScorer_Draw' + fmt((sc3type, sc3mode, sc3time, sc3warn, sc3flags)), lambda a=sc3a,
			t=sc3type, m=sc3mode, tm=sc3time, w=sc3warn, f=sc3flags: (SDTest.Poke(a + 0xc4 - 0x400000, t),
			SDTest.Poke('g_TimerMode', m), sc3setf((sym('g_TimeToCancel'), sym('g_TimeToFinish'), sym('g_TimeInactive'), sym('g_TimeSpecialUse'))[t] + 0x400000, (tm,)),
			sc3setf(sym('g_TimerWarning') + 0x400000, (w,)), SDTest.Poke(sym('GVar_TimeWarning') + 0xc, (SDTest.Peek(sym('GVar_TimeWarning') + 0xc, 'i') & ~1) | f),
			sc3log(lambda a=a: SDTest.Call('cTimerScorer_Draw', 'tvii', a, 0)), sc3fl((sym('g_TimeToCancel'), sym('g_TimeToFinish'), sym('g_TimeInactive'),
			sym('g_TimeSpecialUse'))[t] + 0x400000, 1), sc3fl(sym('g_TimerWarning') + 0x400000, 1)))
	SDTest.Poke(sc3delta - 0x400000, sc3deltasaved)
sc3restore(sc3gvars, sc3gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)

# ---- the target: only with a ship that is no Car (the draw returns before it reads the ship) ----
sc3a = sc3item['sc3Target']
sc3cam = SDTest.Call('cWorld_GetCamName', 'tsii', sc3world, 0)[0]
check('SC3 camera', lambda: sc3cam)
if sc3a and sc3sc and sc3cam:
	sc3ce = Scrap.GetEntity(sc3cam)
	if sc3ce and sc3ce.IsType('FCam'):
		check('SC3 cTargetScorer_Draw', lambda a=sc3a: (SDTest.Call('cTargetScorer_Draw', 'tvii', a, 0), sc3fl(a + 0xac, 2)))
# the waypoint: the current one or the nearest object that is one (the mark itself is the exe's, its blink and the
# positions of the level are not printed)
sc3a = sc3item['sc3Way']
if sc3a and sc3sc:
	check('SC3 waypoint names', lambda: (SDTest.Peek('g_CurrentWaypointName', 'p'), SDTest.Peek('g_CurrentWaypointName2', 'p')))
	for sc3p in (0, 1):
		check('SC3 cWaypointScorer_Draw %d' % sc3p, lambda a=sc3a, p=sc3p: SDTest.Call('cWaypointScorer_Draw', 'tvii', a, p))

# ---- the items deleted: Clear marks them, RemoveDeleted runs the deleting destructors ----
if sc3sc:
	SDTest.Poke(sc3sc + 0x100 - 0x400000, 0)
	check('SC3 SScorer.Clear(0)', lambda: SScorer.Clear(0))
	check('SC3 cScorer_RemoveDeleted', lambda: (SDTest.Call('cScorer_RemoveDeleted', 'tvii', sc3sc, 0), sc3i(sc3sc + 0x40) != 0))
	check('SC3 items gone', lambda: map(lambda t: SScorer.GetItem(0, t[0]), sc3types))
	for sc3k in range(6):
		SDTest.Poke(sc3sc + [0x20, 0x24, 0x28, 0x50, 0xa4, 0x100][sc3k] - 0x400000, sc3scsaved[sc3k])
sc3restore(sc3gvars, sc3gsaved)
check('SC3 heap', lambda: SDTest.HeapOk())
