# game Scorer/AbstractTextScorer.c .. RadarScorer.c (execfile'd by sdlevel.py, Python 1.5.2): the HUD items of the
# first scorer module built through SScorer.Add for player 0 (the HUD of FZ is gone after SScorer.Clear(0) in
# sdlevel.py): their constructor fields, the script variables through SScorer.Set/Get, the text and frame-free
# drawing through the D3D log (viewport 640x480), the pure helpers (Button lines, credits lines, radar floors, view
# cones), the message, label, chat and head monitor texts of the scripts, the news panel object. Only frame 0 or
# no frames are drawn (an image without a sheet CHECKs any other frame). The scorer time and every engine variable
# the items change are saved and put back; SScorer.Clear(0) at the end runs the deleting destructors.
def sc1i(a):
	return SDTest.Peek(a, '@i')
def sc1b(a):
	return SDTest.Peek(a, '@b')
def sc1ints(a, offs):
	r = ()
	for o in offs:
		r = r + (sc1i(a + o),)
	return r
def sc1run(a, n):
	r = ()
	for k in range(n):
		r = r + (sc1i(a + 4 * k),)
	return r
def sc1poke(a, v):
	SDTest.Poke(a - 0x400000, v)
def sc1setf(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def sc1fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def sc1log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
def sc1srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def sc1get(name, var):
	return SScorer.Get(0, name, var)
def sc1set(name, var, v):
	return SScorer.Set(0, name, var, v)

sc1sc = SDTest.Call('Scorer_Get', 'ii', 0)[0]
sc1world = SDTest.Peek('g_pWorld', 'i')
check('SC1 scorer', lambda: (sc1sc != 0, sc1world != 0))
# engine variables the items read or change: scorer-independent globals as rvas
sc1gvars = (sym('g_CreditsAlpha'), sym('g_CreditsClear'), sym('g_CreditsPhase'), sym('g_LabelTime'), sym('g_MsgTime'), sym('g_MsgNumLines'), sym('g_BarValue'), sym('g_BarStopValue'), sym('g_BarSpeedFactor'), sym('g_BarAutoUpdate'),
	sym('g_ChatTime'), sym('g_HeadMonitorAnim'), sym('g_ShowClientPing'), sym('g_ShowNetStatus'), sym('g_NetStatSent'), sym('g_NetStatSentSlot'), sym('g_NetStatRecv'), sym('g_NetStatRecvSlot'), sym('g_RadarScale'), sym('g_RadarAngle'), sym('g_RadarCenterX'),
	sym('g_RadarCenterZ'), sym('g_NewsPanelActive'), sym('g_NewsPanelShowHead'), sym('g_ViewportW'), sym('g_ViewportH'))
sc1gsaved = map(lambda a: SDTest.Peek(a, 'i'), sc1gvars)
# the mission globals 0x45fa80..0x45faa8, the chat lines, the circuit flares
# the words of the mission scorer, separate C objects in ScrapC.exe (stage 3c2): the blink, the solid time, Status0..2
# and the four status times
sc1mwords = [sym('g_MissionBlinkOn'), sym('g_MissionBlinkTime'), sym('g_MissionBlinkLast'), sym('g_MissionSolidTime'),
	sym('g_MissionStatus'), sym('g_MissionStatus') + 4, sym('g_MissionStatus') + 8, sym('g_MissionStatusTime'),
	sym('g_MissionStatusTime') + 4, sym('g_MissionStatusTime') + 8, sym('g_MissionStatusTime') + 12]
def sc1mission(lo, n):
	r = ()
	for k in range(lo, lo + n):
		r = r + (sc1i(sc1mwords[k] + 0x400000),)
	return r
sc1msaved = sc1mission(0, 11)
sc1csaved = sc1run(sym('g_ChatMsgs') + 0x400000, 16 * 0x54 / 4)
sc1fsaved = sc1run(sym('g_CircuitFlares') + 0x400000, 24)
sc1label = SDTest.Peek('g_LabelText', 's')
sc1msg = SDTest.Peek('g_MsgText', 's')
sc1hmname = SDTest.Peek('g_HeadMonitorName', 's')
sc1hmtext = SDTest.Peek('g_HeadMonitorText', 's')
if sc1sc:
	sc1scsaved = sc1ints(sc1sc, (0x20, 0x28))
	sc1setf(sc1sc + 0x20, (12.5,))
	sc1setf(sc1sc + 0x28, (0.2,))
sc1horatio = SDTest.Call('Font_Get', 'is', 'Horatio')[0]
sc1buf = SDTest.Call('operator_new', 'ii', 0x40)[0]

# ---- the items: the vtable the constructor set and its fields ----
sc1types = (('sc1Hint', 'Hint'), ('sc1Label', 'Label'), ('sc1Msg', 'Msg'), ('sc1Credits', 'Credits'),
	('sc1Bar', 'Bar'), ('sc1Button', 'Button'), ('sc1Cannon', 'Cannon'), ('sc1Chat', 'Chat'),
	('sc1Circuit', 'Circuit'), ('sc1CopMeter', 'CopMeter'), ('sc1Hangar', 'HangarShip'), ('sc1Head', 'Head'),
	('sc1Mission', 'Mission'), ('sc1Monitor', 'Monitor'), ('sc1NetStats', 'NetStats'), ('sc1Radar', 'Radar'))
sc1item = {}
for sc1name, sc1type in sc1types:
	sc1srand(4321)
	check('SC1 Add ' + sc1type, lambda n=sc1name, t=sc1type: SScorer.Add(0, n, t))
	sc1a = 0
	if sc1sc:
		sc1a = SDTest.Call('cScorer_FindItem', 'tiis', sc1sc, sc1name)[0]
	sc1item[sc1type] = sc1a
	check('SC1 vftable ' + sc1type, lambda a=sc1a: a and sc1i(a) - 0x400000)
# int fields, byte fields and pointer fields (printed != 0) the constructors write
sc1ctor = (
	('Hint', (0xac, 0xb0, 0xb4, 0xc8, 0xcc), (), (0xc4,)),
	('Label', (0xac, 0xb0, 0xb4, 0xc8, 0xcc), (), (0xc4,)),
	('Msg', (0xac, 0xb0, 0xb4, 0xc4), (), ()),
	('Credits', (0xac, 0xb0, 0xb4, 0xcc), (0xdc, 0xdc + 0x280 * 8, 0xdc + 0x280 * 19), ()),
	('Bar', (0xc4, 0xc8, 0xd0, 0xd4, 0xd8, 0xdc, 0xe0, 0xe4, 0xe8, 0xec, 0xf0, 0xf4, 0xf8, 0xfc, 0x100, 0x104),
		(0xcc, 0xcd), ()),
	('Button', (0xc4, 0xc8, 0xcc, 0xd0, 0xd8, 0xdc, 0xe0, 0xe4, 0xe8, 0xec, 0xf0, 0xf4, 0xf8, 0xfc, 0x100, 0x104,
		0x108, 0x10c, 0x110, 0x11c, 0x120, 0x124, 0x128, 0x12c, 0x130, 0x134, 0x138, 0x13c, 0x140, 0x144, 0x148,
		0x14c, 0x150, 0x154, 0x158, 0x164), (0xb2,), (0x15c, 0x160)),
	('Cannon', (0xb0, 0xb4, 0xb8, 0xbc, 0xc0, 0x18, 0x1c), (0xac,), (0xc4,)),
	('Chat', (0x10, 0x14, 0xb0, 0xb8), (), (0xac,)),
	('Circuit', (0x10, 0x14, 0xac, 0xb0, 0xb8, 0xbc), (), (0xc0, 0xc4)),
	('CopMeter', (0x10, 0x14, 0xd8, 0xdc, 0xe8, 0xec), (0xe0,), (0xac, 0xb0)),
	('HangarShip', (0xc8, 0xe0, 0xe4, 0xe8, 0xec), (0xd0,), (0xc4,)),
	('Head', (0xac, 0xb0, 0xb4, 0xbc, 0xc0), (), (0xb8,)),
	('Mission', (0xe4, 0xe8), (), (0xe0,)),
	('Monitor', (0xac, 0xb0, 0xb8, 0x1fc, 0x208, 0x20c, 0x214, 0x218, 0x21c, 0x220, 0x2a4, 0x344, 0x348),
		(0xbc, 0x210, 0x224, 0x2a8), (0xb4, 0x2ac, 0x310, 0x318, 0x320, 0x328, 0x330, 0x34c, 0x350)),
	('NetStats', (), (), (0x22c,)),
	('Radar', (0x8cc, 0x8d0, 0x8d4, 0x8d8, 0x91c, 0x920, 0x924, 0x928, 0x92c, 0x930, 0x938, 0x93c, 0x940, 0x944,
		0x948), (0x934,), (0x8c4, 0x8c8, 0xbc, 0xc4, 0xcc)))
for sc1type, sc1offs, sc1boffs, sc1poffs in sc1ctor:
	sc1a = sc1item[sc1type]
	if sc1a:
		check('SC1 ctor ' + sc1type, lambda a=sc1a, o=sc1offs, b=sc1boffs, p=sc1poffs: (sc1ints(a, o),
			map(lambda x, a=a: sc1b(a + x), b), map(lambda x, a=a: sc1i(a + x) != 0, p)))
sc1a = sc1item['NetStats']
if sc1a:
	check('SC1 ctor NetStats arrays', lambda a=sc1a: sc1run(a + 0xac, 96))
for sc1type in ('Hint', 'Msg', 'Credits'):
	sc1a = sc1item[sc1type]
	if sc1a:
		check('SC1 ctor strings ' + sc1type, lambda a=sc1a: (SDTest.Peek(a + 0xbc, '@p'), SDTest.Peek(a + 0xc0,
			'@p'), sc1i(a + 0xb8) == sc1horatio))
sc1a = sc1item['Credits']
if sc1a:
	check('SC1 ctor Credits fonts', lambda a=sc1a: (sc1i(a + 0xc4) == SDTest.Call('Font_Get', 'is', 'ScrapMedium')[0],
		sc1i(a + 0xc8) == sc1horatio, SDTest.Peek('g_CreditsAlpha', 'i')))
sc1a = sc1item['Mission']
if sc1a:
	check('SC1 ctor Mission strings', lambda a=sc1a: (SDTest.Peek(a + 0xc4, '@p'), SDTest.Peek(a + 0xcc, '@p')))
sc1a = sc1item['HangarShip']
if sc1a:
	check('SC1 ctor HangarShip name', lambda a=sc1a: SDTest.Peek(a + 0xcc, '@p'))
sc1a = sc1item['Circuit']
if sc1a:
	check('SC1 ctor Circuit file', lambda a=sc1a: SDTest.Peek(a + 0xb4, '@p'))
	# the paths of the board (CircuitScorer_InitPaths, once per run): points, lengths and their fractions
	for sc1k in range(9):
		sc1p = sym('g_CircuitPaths') + 0x400000 + sc1k * 0x308
		check('SC1 circuit path %d' % sc1k, lambda p=sc1p: (sc1i(p), sc1i(p + 0x304), sc1run(p + 4,
			2 * min(32, max(0, sc1i(p)))), sc1run(p + 0x104, min(32, max(0, sc1i(p)))), sc1run(p + 0x184, min(32,
			max(0, sc1i(p)))), sc1run(p + 0x204, min(32, max(0, sc1i(p)))), sc1run(p + 0x284, min(32, max(0,
			sc1i(p))))))

# ---- the script variables: set, then read back ----
sc1textvars = (('Text', 'sc1 text one'), ('Red', 10), ('Green', 20), ('Blue', 30), ('Alpha', 200),
	('Align', 'Right'), ('Align', 'Up'), ('Effect', 'Shadow'), ('Effect', 'Bad'), ('Font', 'ScrapMedium'),
	('Font', 'sc1NoFont'), ('X', 100.0), ('Y', 50.0))
sc1textget = ('Text', 'Red', 'Green', 'Blue', 'Alpha', 'Align', 'Effect', 'X', 'Y', 'Missing')
sc1vars = (
	('sc1Hint', sc1textvars + (('TabSpriteIndex', 0), ('TabBlendingMode', 2), ('TabRed', 1), ('TabGreen', 2),
		('TabBlue', 3), ('TabAlpha', 4)), sc1textget + ('TabSpriteIndex', 'TabRed', 'TabGreen', 'TabBlue', 'TabAlpha')),
	('sc1Label', sc1textvars + (('Align', 'Center'), ('Effect', 'Outline'), ('Font', 'Horatio'), ('TabAlpha', 99)),
		sc1textget + ('TabAlpha', 'TabSpriteIndex')),
	('sc1Msg', sc1textvars + (('Align', 'Left'), ('Effect', 'Oscile'), ('Font', 'Horatio')), sc1textget),
	('sc1Credits', sc1textvars[1:] + (('Align', 'Left'), ('Effect', 'None'), ('Time', 0.0), ('Life', 3.5),
		('FadeIn', 1.25), ('FadeOut', 0.75), ('Text0', 'Credits title'), ('Text1', 'First line'),
		('Text12', 'Line twelve'), ('Text19', 'Line nineteen')), sc1textget + ('Time', 'Life', 'FadeIn',
		'FadeOut')),
	('sc1Bar', (('MaxValue', 100.0), ('Value', 40.0), ('AutoUpdateStopValue', 150.0), ('AutoUpdateSpeedFactor',
		2.5), ('IsAutoUpdatable', 0), ('Blink', 3), ('BackIndex', 0), ('BackRed', 11), ('BackAlpha', 12),
		('MainGreen', 13), ('MainBlue', 14), ('BlackRed', 15), ('BlackAlpha', 16), ('X', 40.0), ('Y', 300.0)),
		('MaxValue', 'Value', 'AutoUpdateStopValue', 'AutoUpdateSpeedFactor', 'IsAutoUpdatable', 'Blink',
		'BackIndex', 'BackRed', 'BackGreen', 'BackAlpha', 'MainGreen', 'MainBlue', 'MainAlpha', 'BlackRed',
		'BlackBlue', 'BlackAlpha', 'MainBarIndex', 'BlackBarIndex', 'Missing')),
	('sc1Cannon', (('CrossHairIndex', 0), ('AimAc', 2), ('AimIndex', 5), ('AimAc', 0), ('AimIndex', 0)),
		('CrossHairIndex', 'AimAc', 'AimIndex', 'X')),
	('sc1Chat', (('NumMsg', 5),), ('NumMsg', 'X', 'Y')),
	('sc1Circuit', (('ScaleX', 2.0), ('ScaleY', 0.5), ('HighRes', 1), ('HighRes', 0), ('BlendingMode', 4),
		('AlphaCircuit', 77), ('AlphaFlare', 66)), ('ScaleX', 'ScaleY', 'AlphaCircuit', 'AlphaFlare', 'X', 'Y')),
	('sc1CopMeter', (('Percent', 50.0), ('Target', 25.0), ('BackIndex', 0), ('BackOffX', 1.5), ('BackOffY', -2.5),
		('LightIndex', 0), ('LightOffX', 3.0), ('LightOffY', 4.0), ('BarOffX', 5.0), ('BarOffY', 6.0),
		('BarIndex', 0)), ('Percent', 'Target', 'BackIndex', 'BackOffX', 'BackOffY', 'LightIndex', 'LightOffX',
		'LightOffY', 'BarOffX', 'BarOffY', 'BarIndex')),
	('sc1Hangar', (('GridColorRed', 21), ('GridColorGreen', 22), ('GridColorBlue', 23), ('GridColorAlpha', 24),
		('TargetName', 'sc1NoShip'), ('CamDist', 150.0), ('CamFov', 30.0)), ('GridColorRed', 'GridColorGreen',
		'GridColorBlue', 'GridColorAlpha', 'GridSpriteIndex', 'TargetName', 'CamDist', 'CamFov')),
	('sc1Head', (('HeadModel', 'sc1NoModel'), ('HeadAnimIndex', 3), ('VW', 128.0), ('VH', 96.0)),
		('HeadAnimIndex', 'VW', 'VH', 'X')),
	('sc1Mission', (('ForceMap', 'Orbit'), ('BackIndex', 0), ('PrimaryIndex', 0), ('SecondaryIndex', 0),
		('SolidTime', 0.0), ('BlinkTime', 0.0), ('TabSpriteIndex', 0), ('TabBlendingMode', 1), ('TabRed', 31),
		('TabAlpha', 32), ('TextRed', 33), ('TextGreen', 34), ('TextBlue', 35)), ('BackIndex', 'PrimaryIndex',
		'SecondaryIndex', 'SolidTime', 'BlinkTime', 'TabSpriteIndex', 'TabRed', 'TabAlpha', 'TextRed', 'TextGreen',
		'TextBlue')),
	('sc1Monitor', (('PanelIndex', 1), ('MaskIndex', 2), ('FrameIndex', 3), ('ObjetiveIndex', 4),
		('ObjetiveChargeIndex', 5), ('ObjetiveBlackIndex', 6), ('ObjetiveWhiteIndex', 7), ('StaticIndex', 8),
		('BigFont', 'Horatio'), ('SmallFont', 'sc1NoFont'), ('HeadModel', 'sc1NoModel'), ('HeadAnimIndex', 2),
		('Noise', 2.5), ('Text', 'sc1 monitor text'), ('TargetView', 5), ('WeaponOffX', 7.5), ('WeaponOffY', 8.5),
		('WeaponIconScale', 1.5), ('WeaponIconAc', 3), ('WeaponIconIndex', 9), ('UpgradeIconAc', 12),
		('UpgradeIconIndex', 10), ('CanPossessIndex', 11), ('CanPossessGlowIndex', 12)), ('PanelIndex',
		'MaskIndex', 'FrameIndex', 'ObjetiveIndex', 'ObjetiveChargeIndex', 'ObjetiveBlackIndex',
		'ObjetiveWhiteIndex', 'StaticIndex', 'HeadAnimIndex', 'Noise', 'Text', 'TargetView', 'WeaponOffX',
		'WeaponOffY', 'WeaponIconScale', 'WeaponIconAc', 'WeaponIconIndex', 'UpgradeIconAc', 'UpgradeIconIndex',
		'CanPossessIndex', 'CanPossessGlowIndex')),
	('sc1NetStats', (('X', 200.0), ('Y', 300.0)), ('X', 'Y', 'Missing')),
	('sc1Radar', (('MenuScaleSpeed', 5.0), ('MenuMaxScale', 600.0), ('MenuMinScale', 400.0), ('MenuMap', 2),
		('OutAlphaFactor', 80), ('Map2dScale', 256.0), ('Map2dScaleFactor', 0.02), ('RadarIndex', 1),
		('MaskIndex', 2), ('IconAc', 4), ('IconIndex', 6), ('MenuMap', 0), ('X', 500.0),
		('Y', 20.0)), ('MenuScaleSpeed', 'MenuMaxScale', 'MenuMinScale', 'MenuMap', 'OutAlphaFactor',
		'Map2dScale', 'Map2dScaleFactor', 'RadarIndex', 'MaskIndex', 'IconAc', 'IconIndex', 'X', 'Y')))
for sc1name, sc1sets, sc1gets in sc1vars:
	for sc1v, sc1val in sc1sets:
		check('SC1 Set' + fmt((sc1name, sc1v, sc1val)), lambda n=sc1name, v=sc1v, x=sc1val: sc1set(n, v, x))
	for sc1v in sc1gets:
		check('SC1 Get' + fmt((sc1name, sc1v)), lambda n=sc1name, v=sc1v: sc1get(n, v))
sc1a = sc1item['Bar']
if sc1a:
	for sc1v in (-5.0, 30.0):
		check('SC1 Bar AutoUpdateStopValue' + fmt((sc1v,)), lambda v=sc1v: (sc1set('sc1Bar',
			'AutoUpdateStopValue', v), sc1get('sc1Bar', 'AutoUpdateStopValue')))
	check('SC1 Bar Blink 0', lambda a=sc1a: (sc1set('sc1Bar', 'Blink', 0), sc1b(a + 0xcc)))
sc1a = sc1item['Credits']
if sc1a:
	check('SC1 Credits lines', lambda a=sc1a: (SDTest.Peek(a + 0xdc, '@s'), SDTest.Peek(a + 0xdc + 0x280,
		'@s'), SDTest.Peek(a + 0xdc + 0x280 * 12, '@s'), SDTest.Peek(a + 0xdc + 0x280 * 19, '@s')))
sc1a = sc1item['Hint']
if sc1a:
	sc1set('sc1Hint', 'Text', 'sc1 hint extent')
	for sc1v in ('GetExtens', 'CentralText'):
		check('SC1 Hint ' + sc1v, lambda a=sc1a, v=sc1v: (sc1set('sc1Hint', 'X', 100.0), sc1set('sc1Hint', 'Y',
			50.0), sc1set('sc1Hint', v, 1), sc1run(a + 0x10, 4), sc1set('sc1Hint', v, 0), sc1run(a + 0x10, 4)))
	check('SC1 cScorerItem_SetParam', lambda a=sc1a: SDTest.Call('cScorerItem_SetParam', 'tviss', a, 'X', '1'))
sc1a = sc1item['Mission']
if sc1a:
	# the status lamps: the scorer time at the set, the station text of the map
	sc1setf(sc1sc + 0x20, (20.0,))
	check('SC1 Mission Status1', lambda: (sc1set('sc1Mission', 'Status1', 1), sc1get('sc1Mission', 'LastStatus')))
	sc1setf(sc1sc + 0x20, (21.0,))
	check('SC1 Mission Status2', lambda: (sc1set('sc1Mission', 'Status2', 2), sc1get('sc1Mission', 'LastStatus')))
	sc1setf(sc1sc + 0x20, (22.0,))
	for sc1v in ('Orbit', 'sc1Map'):
		check('SC1 Mission Status0' + fmt((sc1v,)), lambda v=sc1v: (sc1set('sc1Mission', 'ForceMap', v),
			sc1set('sc1Mission', 'Status0', 1), SDTest.Peek('g_MissionMapName', 'p'), sc1get('sc1Mission', 'LastStatus'),
			sc1get('sc1Mission', 'Status0')))
	check('SC1 Mission Status0 0', lambda: (sc1set('sc1Mission', 'Status0', 0), SDTest.Peek('g_MissionMapName', 'i'),
		sc1get('sc1Mission', 'LastStatus')))
	check('SC1 Mission globals', lambda: sc1mission(0, 10))
	check('SC1 Mission StatusClear', lambda: (sc1set('sc1Mission', 'StatusClear', 1), sc1mission(4, 6), sc1get('sc1Mission', 'LastStatus')))
	check('SC1 Mission BlinkTime', lambda: (sc1set('sc1Mission', 'BlinkTime', 1.0), sc1mission(0, 3)))
	sc1setf(sc1sc + 0x20, (12.5,))
sc1a = sc1item['Monitor']
if sc1a:
	for sc1v in ('1', '0', 'x'):
		check('SC1 Monitor SetParam' + fmt((sc1v,)), lambda a=sc1a, v=sc1v: (SDTest.Call('cMonitorScorer_SetParam', 'tviss', a,
			'TargetView', v), sc1b(a + 0x210)))
	check('SC1 Monitor SetModel', lambda a=sc1a: (SDTest.Call('cMonitorScorer_SetModel', 'tbis', a, 'sc1NoModel')[0],
		sc1ints(a, (0xac, 0xb8, 0x1fc, 0x208))))
	check('SC1 Monitor SetAnim', lambda a=sc1a: (SDTest.Call('cMonitorScorer_SetAnim', 'tvii', a, 1), sc1i(a + 0x20c)))
	check('SC1 Monitor fonts', lambda a=sc1a: (sc1i(a + 0x204) == sc1horatio, sc1i(a + 0x200) ==
		SDTest.Call('Font_Get', 'is', 'Terminal8')[0]))
sc1a = sc1item['Head']
if sc1a:
	check('SC1 Head SetModel', lambda a=sc1a: (SDTest.Call('cHeadScorer_SetModel', 'tbis', a, 'sc1NoModel')[0], sc1i(a + 0xb0),
		sc1i(a + 0xb4)))
	check('SC1 Head SetAnim', lambda a=sc1a: (SDTest.Call('cHeadScorer_SetAnim', 'tvii', a, 0), sc1i(a + 0xb4)))
	check('SC1 Head Draw', lambda a=sc1a: (SDTest.Call('cHeadScorer_Draw', 'tvii', a, 0), sc1i(a + 0xac)))

# ---- Button: the lines of a text, the box size, the word wrap ----
sc1a = sc1item['Button']
def sc1width(font, s):
	SDTest.Call('Font_TextSize', 'viiiffss', font, sc1buf, sc1buf + 8, 100.0, 50.0, '%s', s)
	return sc1fl(sc1buf + 8, 1)[0]
# the wrap of cButtonScorer_WrapLines reads past the end of the text when the last word does not fit on its line:
# only texts where it fits
def sc1wrapok(font, s, lim):
	start = 0
	cur = 0
	while 1:
		sp = strop.find(s, ' ', cur)
		if sp < 0:
			sp = len(s)
		w = sc1width(font, s[start:sp])
		if sp == len(s):
			return float(lim) >= w
		if w > float(lim):
			start = cur
		cur = sp + 1
sc1btnf = (0x10, 0x14, 0x18, 0x1c, 0xc8, 0xd0, 0xf4, 0xf8, 0x100, 0x114, 0x118, 0x124, 0x144, 0x148)
def sc1lines(a):
	r = ()
	for k in range(sc1i(a + 0xc8)):
		r = r + ((SDTest.Peek(sc1i(a + 0xc4) + 4 * k, '@p'), sc1b(sc1i(a + 0xcc) + k)),)
	return r
if sc1a:
	sc1set('sc1Button', 'X', 100.0)
	sc1set('sc1Button', 'Y', 50.0)
	for sc1pair, sc1max, sc1text in ((1, -1, 'Play game'), (1, -1, 'Name:Value'), (1, -1, 'Lives:3\nScore:120'),
		(0, -1, 'Line one\nLine two\nLine three'), (0, -1, 'A:B'), (1, -1, ''),
		(1, 40, 'This is a longer text that wraps into a few lines of the box'),
		(0, 200, 'Short words wrap late in this one'), (1, 10, 'Single')):
		sc1lim = sc1max + 75
		if sc1max > -1 and not sc1wrapok(sc1i(sc1a + 0xd4), sc1text, sc1lim):
			check('SC1 Button Text' + fmt((sc1pair, sc1max, sc1text)), lambda: 'wrap past the end, skipped')
			continue
		check('SC1 Button Text' + fmt((sc1pair, sc1max, sc1text)), lambda a=sc1a, p=sc1pair, m=sc1max, t=sc1text: (
			sc1set('sc1Button', 'IsNameValuePair', p), sc1set('sc1Button', 'MaxLineSize', m), sc1set('sc1Button',
			'Text', t), sc1ints(a, sc1btnf), sc1lines(a), sc1get('sc1Button', 'SizeX'), sc1get('sc1Button',
			'SizeY')))
	for sc1v in ('XAlignLeft', 'XAlignRight', 'XAlignCenter'):
		check('SC1 Button ' + sc1v, lambda a=sc1a, v=sc1v: (sc1set('sc1Button', v, 300.0), sc1get('sc1Button', v),
			sc1i(a + 0x10)))
	sc1set('sc1Button', 'X', 100.0)
	for sc1v, sc1val in (('Highlight', 1), ('Glow', 1), ('Highlight', 0), ('Glow', 0), ('HighlightOffset', 2.5),
		('Marker', 1), ('Marker', 0), ('MarkerOffset', 3.0), ('ScaleSmoothTime', 0.5), ('TextColorRed', 41),
		('TextColorAlpha', 42), ('BorderColorGreen', 43), ('HighLightColorBlue', 44), ('FocusSpriteIndex', 0),
		('RenderAlphaChannel', 0), ('BorderSpriteIndex', -1), ('Complex', 0), ('MinX', -1.0), ('MaxX', -1.0),
		('HighLightBlendingMode', 2), ('Font', 'sc1NoFont'), ('ForceWidth', 85), ('ForceHeight', 47)):
		check('SC1 Button Set' + fmt((sc1v, sc1val)), lambda v=sc1v, x=sc1val: sc1set('sc1Button', v, x))
	check('SC1 Button fields', lambda a=sc1a: sc1ints(a, (0xe4, 0xe8, 0xf4, 0xf8, 0x108, 0x11c, 0x120, 0x130,
		0x134, 0x138, 0x14c, 0x150, 0x154, 0x158, 0x164)))
	for sc1v in ('Highlight', 'Glow', 'HighlightOffset', 'Marker', 'MarkerOffset', 'ScaleSmoothTime',
		'ScaleSmoothWidth', 'ScaleSmoothHeight', 'TextColorRed', 'TextColorGreen', 'TextColorAlpha',
		'BorderColorRed', 'BorderColorGreen', 'HighLightColorBlue', 'FocusSpriteIndex', 'RenderAlphaChannel',
		'BorderSpriteIndex', 'Complex', 'MinX', 'MaxX', 'IsNameValuePair', 'MaxLineSize', 'SizeX', 'SizeY'):
		check('SC1 Button Get' + fmt((sc1v,)), lambda v=sc1v: sc1get('sc1Button', v))
	# cButtonScorer_WrapLines on its own: the lines appended to a buffer
	sc1out = SDTest.Call('operator_new', 'ii', 0x1400)[0]
	for sc1max, sc1text in ((40, 'Wrap these words into short lines of text'), (500, 'One line only'),
		(0, 'a b c')):
		if not sc1wrapok(sc1i(sc1a + 0xd4), sc1text, sc1max + 75):
			check('SC1 Button_WrapLines' + fmt((sc1max, sc1text)), lambda: 'wrap past the end, skipped')
			continue
		sc1poke(sc1out, 0)
		sc1poke(sc1buf + 0x20, sc1out)
		sc1set('sc1Button', 'MaxLineSize', sc1max)
		check('SC1 Button_WrapLines' + fmt((sc1max, sc1text)), lambda a=sc1a, t=sc1text, o=sc1out: (
			SDTest.Call('cButtonScorer_WrapLines', 'tiiis', a, sc1buf + 0x20, t)[0], SDTest.Peek(o, '@s'), sc1i(sc1buf + 0x20) == o))
	sc1set('sc1Button', 'MaxLineSize', -1)

# the viewport of the draws from here on (put back with the engine variables at the end)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)

# ---- Radar: the floors of the world and the view cones ----
sc1a = sc1item['Radar']
if sc1a and sc1world:
	sc1nf = sc1i(sc1world + 0x2f0)
	check('SC1 world floors', lambda: (sc1nf, map(lambda k: sc1i(sc1world + 0x2f4 + k * 0x18c), range(min(16,
		max(0, sc1nf))))))
	if sc1nf > 0:
		for sc1y in (-10000.0, -100.0, 0.0, 5.0, 50.0, 100.0, 300.0, 1000.0, 10000.0, -10000.0):
			check('SC1 Radar UpdateFloor' + fmt((sc1y,)), lambda a=sc1a, y=sc1y: (SDTest.Call('cRadarScorer_UpdateFloor', 'tvif', a,
				y), sc1run(a + 0x8cc, 3), SDTest.Call('cRadarScorer_FloorSide', 'tiif', a, y)[0], SDTest.Call('cRadarScorer_OnPrevFloor', 'tbif', a,
				y)[0], SDTest.Call('cRadarScorer_FloorSide', 'tiif', a, -y)[0], SDTest.Call('cRadarScorer_OnPrevFloor', 'tbif', a, -y)[0]))
		sc1pos = SDTest.Call('operator_new', 'ii', 12)[0]
		sc1setf(sc1a + 0xac, (55.0, 55.0, 56.25, 56.25))
		sc1setf(sym('g_RadarScale') + 0x400000, (2.0,))
		sc1setf(sym('g_RadarAngle') + 0x400000, (0.5,))
		sc1setf(sym('g_RadarCenterX') + 0x400000, (30.0,))
		sc1setf(sym('g_RadarCenterZ') + 0x400000, (-20.0,))
		for sc1fade in (0.25, 1.0):
			sc1setf(sc1a + 0x8d4, (sc1fade,))
			for sc1y in (-10000.0, 0.0, 50.0, 10000.0):
				for sc1mode in (1, 2, 3):
					sc1setf(sc1pos, (10.0, sc1y, 20.0))
					check('SC1 Radar DrawCone' + fmt((sc1fade, sc1y, sc1mode)), lambda a=sc1a, m=sc1mode: sc1log(
						lambda a=a, m=m: SDTest.Call('cRadarScorer_DrawCone', 'tviifffi', a, sc1pos, 0.75, 0.5, 120.0, m)))
		check('SC1 Radar floor state', lambda a=sc1a: (sc1run(a + 0x8cc, 3), sc1i(a + 0x934) & 0xff))

# ---- drawing the texts ----
sc1a = sc1item['Label']
if sc1a:
	check('SC1 Scorer_SetLabelText', lambda: (SDTest.Call('Scorer_SetLabelText', 'vsf', 'sc1 label text', 0.0),
		SDTest.Peek('g_LabelText', 's'), SDTest.Peek('g_LabelTime', 'i')))
	for sc1v in ('Left', 'Center', 'Right'):
		sc1set('sc1Label', 'Align', sc1v)
		check('SC1 Label Draw ' + sc1v, lambda a=sc1a: sc1log(lambda a=a: SDTest.Call('cLabelScorer_Draw', 'tvii', a, 0)))
	check('SC1 SScorer.SetLabelText', lambda: (SScorer.SetLabelText('sc1 second label', 0.0),
		SDTest.Peek('g_LabelText', 's')))
sc1a = sc1item['Msg']
if sc1a:
	sc1mtext = 'sc1 a message long enough to wrap into more than one line of the message area on the screen'
	check('SC1 Scorer_SetMsgText', lambda t=sc1mtext: (SDTest.Call('Scorer_SetMsgText', 'vsfs', t, 0.0, 'Horatio'),
		SDTest.Peek('g_MsgNumLines', 'i'), SDTest.Peek('g_MsgTime', 'i'), map(lambda k: SDTest.Peek(sym('g_MsgLines') + 4 * k, 'p'),
		range(min(8, SDTest.Peek('g_MsgNumLines', 'i'))))))
	check('SC1 Msg Draw', lambda a=sc1a: sc1log(lambda a=a: SDTest.Call('cMsgScorer_Draw', 'tvii', a, 0)))
	check('SC1 SScorer.SetMsgText', lambda: (SScorer.SetMsgText('sc1 short', 0.0), SDTest.Peek('g_MsgNumLines', 'i')))
	check('SC1 Msg Draw short', lambda a=sc1a: sc1log(lambda a=a: SDTest.Call('cMsgScorer_Draw', 'tvii', a, 0)))
	# shown until a world time past now: nothing drawn once it is over (the time 1e-30 is in the past)
	sc1setf(sym('g_MsgTime') + 0x400000, (1e-30,))
	check('SC1 Msg Draw over', lambda a=sc1a: sc1log(lambda a=a: SDTest.Call('cMsgScorer_Draw', 'tvii', a, 0)))
	sc1setf(sym('g_MsgTime') + 0x400000, (1e30,))
	check('SC1 Monitor_RepeatPhotoMessage', lambda: (SDTest.Call('Monitor_RepeatPhotoMessage', 'v'), SDTest.Peek('g_MsgText', 's')))
sc1a = sc1item['Hint']
if sc1a:
	check('SC1 Hint Draw', lambda a=sc1a: (sc1log(lambda a=a: SDTest.Call('cHintScorer_Draw', 'tvii', a, 0)),
		SDTest.Peek(sc1sc + 0x30, '@p')))
sc1a = sc1item['Credits']
if sc1a:
	sc1set('sc1Credits', 'Font', 'Horatio')
	for sc1fade in (0, 1):
		check('SC1 Credits DrawLines' + fmt((sc1fade,)), lambda a=sc1a, f=sc1fade: sc1log(lambda a=a, f=f:
			SDTest.Call('cCreditsScorer_DrawLines', 'tviii', a, f, 0x40ff8000)))
	sc1set('sc1Credits', 'Text8', 'Columns')
	check('SC1 Credits Draw columns', lambda a=sc1a: (sc1log(lambda a=a: SDTest.Call('cCreditsScorer_Draw', 'tvii', a, 0)),
		sc1i(a + 0xcc)))
sc1a = sc1item['Chat']
if sc1a:
	for sc1k in range(18):
		SScorer.AddChatMsg('sc1 chat %d' % sc1k, sc1k * 10, 255 - sc1k * 10, sc1k)
	check('SC1 chat lines', lambda: map(lambda k: (SDTest.Peek(sym('g_ChatMsgs') + k * 0x54, 's'), SDTest.Peek(sym('g_ChatMsgs') +
		k * 0x54 + 0x50, 'i')), range(16)))
	for sc1v in (4, 20, -3):
		check('SC1 Chat Draw' + fmt((sc1v,)), lambda a=sc1a, v=sc1v: (sc1set('sc1Chat', 'NumMsg', v),
			sc1log(lambda a=a: SDTest.Call('cChatScorer_Draw', 'tvii', a, 0)), sc1i(a + 0xb0)))
sc1a = sc1item['Button']
if sc1a:
	sc1set('sc1Button', 'IsNameValuePair', 1)
	sc1set('sc1Button', 'Text', 'Lives:3\nScore:120')
	# isMultiSprite 0: only the text, no frames of the sprite
	sc1poke(sc1a + 0xb0, sc1i(sc1a + 0xb0) & ~0xff0000)
	for sc1v in ('Left', 'Center', 'Right'):
		for sc1mm in ((-1.0, -1.0), (120.0, 260.0)):
			check('SC1 Button Draw' + fmt((sc1v, sc1mm)), lambda a=sc1a, v=sc1v, mm=sc1mm: (sc1set('sc1Button',
				'TextAlign', v), sc1set('sc1Button', 'MinX', mm[0]), sc1set('sc1Button', 'MaxX', mm[1]),
				sc1log(lambda a=a: SDTest.Call('cButtonScorer_Draw', 'tvii', a, 0))))
	sc1set('sc1Button', 'IsNameValuePair', 0)
	sc1set('sc1Button', 'Text', 'Plain one\nPlain two')
	check('SC1 Button Draw plain', lambda a=sc1a: sc1log(lambda a=a: SDTest.Call('cButtonScorer_Draw', 'tvii', a, 0)))
sc1a = sc1item['Bar']
if sc1a:
	for sc1v in (0.0, 40.0, 100.0, 150.0):
		check('SC1 Bar Draw' + fmt((sc1v,)), lambda a=sc1a, v=sc1v: (sc1set('sc1Bar', 'Value', v), sc1log(
			lambda a=a: SDTest.Call('cBarScorer_Draw', 'tvii', a, 0)), sc1get('sc1Bar', 'Value')))
sc1a = sc1item['Mission']
if sc1a:
	# blinking: the icons are off in this frame, nothing drawn
	check('SC1 Mission Draw blink', lambda a=sc1a: (sc1set('sc1Mission', 'BlinkTime', 1.0), sc1set('sc1Mission',
		'SolidTime', 2.0), sc1log(lambda a=a: SDTest.Call('cMissionScorer_Draw', 'tvii', a, 0)), sc1mission(0, 4),
		sc1i(sc1i(a + 0xbc) + 0x34)))
sc1a = sc1item['Circuit']
if sc1a:
	# the flares all at their end: new paths, pauses and durations from rand, nothing drawn in this frame
	for sc1k in range(24):
		sc1poke(sym('g_CircuitFlares') + 0x400000 + 4 * sc1k, 0)
	sc1poke(sc1a + 0xb0, 0)
	sc1srand(99)
	check('SC1 Circuit Draw', lambda a=sc1a: (sc1log(lambda a=a: SDTest.Call('cCircuitScorer_Draw', 'tvii', a, 0)),
		sc1run(sym('g_CircuitFlares') + 0x400000, 24), sc1i(a + 0xac) == sc1i(a + 0xb0)))
sc1a = sc1item['NetStats']
if sc1a:
	sc1setf(sc1a + 0xac, (10.0, 0.0, 5.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0))
	sc1setf(sc1a + 0xec, (0.0, 0.0, 6.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 9.0))
	sc1setf(sc1a + 0x12c, (1.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
	sc1setf(sc1a + 0x16c, (1.1, 3.0, 0.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.25))
	for sc1k in range(16):
		sc1poke(sc1a + 0x1ac + 4 * sc1k, sc1k * 3)
		sc1poke(sc1a + 0x1ec + 4 * sc1k, sc1k * 5)
	SDTest.Poke('g_ShowNetStatus', 1)
	SDTest.Poke('g_NetStatSent', 0)
	SDTest.Poke('g_NetStatRecv', 40)
	SDTest.Poke('g_NetStatSentSlot', 3)
	SDTest.Poke('g_NetStatRecvSlot', 5)
	check('SC1 NetStats Draw', lambda a=sc1a: (sc1log(lambda a=a: SDTest.Call('cNetStatsScorer_Draw', 'tvii', a, 0)),
		sc1run(a + 0xac, 96), SDTest.Peek('g_NetStatRecv', 'i')))
	SDTest.Poke('g_ShowNetStatus', 0)
	check('SC1 NetStats Draw off', lambda a=sc1a: sc1log(lambda a=a: SDTest.Call('cNetStatsScorer_Draw', 'tvii', a, 0)))
sc1a = sc1item['CopMeter']
if sc1a and sc1world and sc1i(sc1world + 0x1c6c) == 0:
	# no alarm and faded out: nothing drawn
	check('SC1 CopMeter Draw', lambda a=sc1a: (sc1log(lambda a=a: SDTest.Call('cCopMeterScorer_Draw', 'tvii', a, 0)),
		sc1ints(a, (0xd8, 0xdc, 0xe8)), sc1b(a + 0xe0)))

# ---- the head monitor text of the scripts ----
check('SC1 SScorer.SetHeadMonitor', lambda: (SScorer.SetHeadMonitor('sc1Head', 'sc1 head text', -2),
	SDTest.Peek('g_HeadMonitorName', 's'), SDTest.Peek('g_HeadMonitorText', 's'), SDTest.Peek('g_HeadMonitorAnim', 'i')))

# ---- the news panel object on its own ----
sc1np = SDTest.Call('operator_new', 'ii', 0x38)[0]
check('SC1 cNewsPanel_ctor', lambda p=sc1np: (SDTest.Call('cNewsPanel_ctor', 'tii', p)[0] == p, sc1ints(p, (0x24, 0x28,
	0x2c, 0x30)), sc1b(p + 0x34), sc1i(p) == SDTest.Peek('g_EngineInits', 'i'), sc1i(p + 0x10) != 0))
check('SC1 cNewsPanel_Reset', lambda p=sc1np: (SDTest.Call('cNewsPanel_Reset', 'tvi', p), sc1run(p + 4, 3), sc1ints(p,
	(0x20, 0x24, 0x28, 0x2c, 0x30)), sc1b(p + 0x34), SDTest.Peek(p + 0x14, '@p'), SDTest.Peek(p + 0x18, '@p'),
	SDTest.Peek(p + 0x1c, '@p'), SDTest.Peek('g_NewsPanelActive', 'i') == SDTest.Peek('g_NewsPanelActiveDef', 'i'), SDTest.Peek('g_NewsPanelShowHead',
	'i') == SDTest.Peek('g_NewsPanelShowHeadDef', 'i')))
for sc1k in range(4):
	check('SC1 cNewsPanel_GetWatched(%d)' % sc1k, lambda p=sc1np, k=sc1k: SDTest.Call('cNewsPanel_GetWatched', 'tiii', p, k)[0] !=
		0)
check('SC1 cNewsPanel_dtor', lambda p=sc1np: SDTest.Call('cNewsPanel_dtor', 'tvi', p))

# ---- everything back; Clear(0) deletes the items ----
# Clear only marks them; until the next frame deletes them, every frame index is 0 again: their sheets are
# white.tga, which CHECKs any other frame
for sc1name, sc1v, sc1val in (('sc1Cannon', 'AimAc', 2), ('sc1Radar', 'IconAc', 4), ('sc1Monitor',
	'WeaponIconAc', 3), ('sc1Monitor', 'UpgradeIconAc', 12)):
	sc1set(sc1name, sc1v, sc1val)
for sc1name, sc1v in (('sc1Cannon', 'AimIndex'), ('sc1Radar', 'IconIndex'), ('sc1Monitor', 'WeaponIconIndex'),
	('sc1Monitor', 'UpgradeIconIndex')):
	sc1set(sc1name, sc1v, 0)
for sc1name, sc1v in (('sc1Cannon', 'AimAc'), ('sc1Radar', 'IconAc'), ('sc1Monitor', 'WeaponIconAc'),
	('sc1Monitor', 'UpgradeIconAc'), ('sc1Radar', 'RadarIndex'), ('sc1Radar', 'MaskIndex'), ('sc1Monitor',
	'PanelIndex'), ('sc1Monitor', 'MaskIndex'), ('sc1Monitor', 'FrameIndex'), ('sc1Monitor', 'ObjetiveIndex'),
	('sc1Monitor', 'ObjetiveChargeIndex'), ('sc1Monitor', 'ObjetiveBlackIndex'), ('sc1Monitor', 'ObjetiveWhiteIndex'),
	('sc1Monitor', 'StaticIndex'), ('sc1Monitor', 'CanPossessIndex'), ('sc1Monitor', 'CanPossessGlowIndex')):
	sc1set(sc1name, sc1v, 0)
check('SC1 SScorer.Clear(0)', lambda: SScorer.Clear(0))
check('SC1 items gone', lambda: map(lambda t: SScorer.GetItem(0, t[0]), sc1types))
SScorer.SetLabelText(sc1label, 0.0)
SScorer.SetMsgText(sc1msg, 0.0)
SScorer.SetHeadMonitor(sc1hmname, sc1hmtext, 0)
for sc1k in range(len(sc1gvars)):
	SDTest.Poke(sc1gvars[sc1k], sc1gsaved[sc1k])
for sc1k in range(11):
	sc1poke(sc1mwords[sc1k] + 0x400000, sc1msaved[sc1k])
for sc1k in range(len(sc1csaved)):
	sc1poke(sym('g_ChatMsgs') + 0x400000 + 4 * sc1k, sc1csaved[sc1k])
for sc1k in range(24):
	sc1poke(sym('g_CircuitFlares') + 0x400000 + 4 * sc1k, sc1fsaved[sc1k])
if sc1sc:
	sc1poke(sc1sc + 0x20, sc1scsaved[0])
	sc1poke(sc1sc + 0x28, sc1scsaved[1])
