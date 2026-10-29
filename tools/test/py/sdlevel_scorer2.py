import strop
# game Scorer/Scorer.c, ScraplandScorer.c, SliderScorer.c, SpecialActionScorer.c (execfile'd by sdlevel.py, Python
# 1.5.2): the color lerp, the fade, cinema and noise objects on a heap buffer, a cScorer and a cScorerItem built and
# deleted on the heap, the items Slider, SpecialAction, ScraplandMainMenu, ScraplandSubMenu and Sprite of scorer 0
# through SScorer.Add, Set and Get, the slider draw and the cursor update through the D3D log, the cursor moves, the
# Xbox texts and the clouds. Few items and few images; every engine variable the functions write is saved and put
# back; the items are deleted at the end (Clear, then RemoveDeleted).
def sc2i(a):
	return SDTest.Peek(a, '@i')
def sc2b(a):
	return SDTest.Peek(a, '@b')
def sc2ints(a, offs):
	r = ()
	for o in offs:
		r = r + (sc2i(a + o),)
	return r
def sc2fl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def sc2setf(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def sc2log(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
def sc2srand(s):
	SDTest.Call('crt_srand', 'vi', s)
def sc2new(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def sc2free(p):
	SDTest.Call('operator_delete', 'vi', p)
def sc2get(name, var):
	return SScorer.Get(0, name, var)
def sc2set(name, var, v):
	return SScorer.Set(0, name, var, v)
def sc2find(name):
	return SDTest.Call('cScorer_FindItem', 'tiis', sc2sc, name)[0]
# the rvas of the engine variables as ints, saved and put back
def sc2save(rvas):
	return map(lambda a: SDTest.Peek(a, 'i'), rvas)
def sc2restore(rvas, vals):
	for sc2k in range(len(rvas)):
		SDTest.Poke(rvas[sc2k], vals[sc2k])

sc2sc = SDTest.Call('Scorer_Get', 'ii', 0)[0]
sc2all = SDTest.Peek('g_Scorers', 'i')
check('SC2 Scorer_Get', lambda: map(lambda p: SDTest.Call('Scorer_Get', 'ii', p)[0] == sc2all + 0x104 * p,
	(-1, 0, 1, 3, 4)))
sc2gvars = (sym('g_RadarMMSelect'), sym('g_BarValue'), sym('g_BarStopValue'), sym('g_BarSpeedFactor'), sym('g_CinemaBlackBandHeight'), sym('g_CameraScene'), sym('g_BarAutoUpdate'), sym('g_NumEnemiesInRadar'), sym('g_MissionSolidTime'), sym('g_NoiseFactor'),
	sym('g_NoiseFactorBegin'), sym('g_NoiseTime'), sym('g_NoiseEndTime'), sym('g_MotionBlurFactorBegin'), sym('g_MotionBlurFactorEnd'), sym('g_MotionBlurTime'), sym('g_MotionBlurEndTime'), sym('g_MotionBlurOffVar'), sym('g_SceneMotionBlurValue'), sym('g_SceneMotionBlurOff'), sym('g_PhysicalAspectRatio'),
	sym('g_ViewportW'), sym('g_ViewportH'), sym('g_XboxSavingMsg'), sym('g_XboxNoPadMsg'), sym('g_XboxNoPadMsgP'), sym('g_SpecialActionActive'), sym('g_EnergyBarActive'), sym('g_SpecialActionState'), sym('g_SpecialActionFade'))
sc2gsaved = sc2save(sc2gvars)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
if sc2sc:
	sc2scsaved = sc2ints(sc2sc, (0x20, 0x24, 0x28, 0x2c, 0x100))
	sc2setf(sc2sc + 0x20, (12.5, 12.25, 0.05))
	SDTest.Poke(sc2sc + 0x100 - 0x400000, 0)

# ---- the color lerp ----
for sc2a, sc2c, sc2t in ((0x11223344, 0x55667788, 0.25), (-0x1000000, 0xffffff, 0.5), (0x7f808080, 0x10203040,
	0.75), (0x12345678, -0x789abcdf, -1.0), (0x12345678, -0x789abcdf, 0.0), (0x12345678, -0x789abcdf, 0.5),
	(0x12345678, -0x789abcdf, 1.0), (0x12345678, -0x789abcdf, 3.0), (0, -1, 0.999), (-1, 0, 0.3)):
	check('SC2 Scorer_ColorLerp' + fmt((sc2a, sc2c, sc2t)), lambda a=sc2a, c=sc2c, t=sc2t: SDTest.Call('Scorer_ColorLerp',
		'iiif', a, c, t)[0])

# ---- cinema, fade and noise objects on a buffer ----
sc2buf = sc2new(0x40)
check('SC2 ScorerCinema_ctor', lambda b=sc2buf: (SDTest.Call('ScorerCinema_ctor', 'tii', b)[0] == b, sc2fl(b, 2), sc2b(b + 8),
	sc2fl(b + 12, 1)))
for sc2on, sc2t in ((1, 0.0), (0, 0.5), (0, 0.25), (1, 2.0), (1, 1.0)):
	check('SC2 ScorerCinema_Set' + fmt((sc2on, sc2t)), lambda b=sc2buf, o=sc2on, t=sc2t: (SDTest.Call('ScorerCinema_Set',
		'tviif', b, o, t), sc2fl(b, 2), sc2b(b + 8)))
for sc2on, sc2scene, sc2ar, sc2dt in ((1, 0, 1.25, 0.5), (1, 1, 0.75, 0.25), (1, 1, 1.5, 0.25), (0, 0, 1.25, 0.25),
	(0, 0, 1.25, 4.0)):
	SDTest.Call('ScorerCinema_Set', 'tviif', sc2buf, sc2on, 0.0)
	sc2setf(sc2buf, (0.25, 1.0))
	SDTest.Poke('g_CameraScene', sc2scene)
	sc2setf(sym('g_PhysicalAspectRatio') + 0x400000, (sc2ar,))
	check('SC2 ScorerCinema_Draw' + fmt((sc2on, sc2scene, sc2ar, sc2dt)), lambda b=sc2buf, dt=sc2dt: (sc2log(
		lambda b=b, dt=dt: SDTest.Call('ScorerCinema_Draw', 'tviif', b, 0, dt)), sc2fl(b, 2), sc2fl(b + 12, 1)))
sc2restore(sc2gvars, sc2gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
check('SC2 ScorerFade_ctor', lambda b=sc2buf: (SDTest.Call('ScorerFade_ctor', 'tii', b)[0] == b, sc2fl(b, 10)))
check('SC2 ScorerFade_Start', lambda b=sc2buf: (SDTest.Call('ScorerFade_Start', 'tviiif', b, 0x11223344, 0x55667788, 2.0),
	sc2fl(b, 10)))
for sc2dt in (0.5, 1.0, 3.0):
	check('SC2 ScorerFade_Draw' + fmt((sc2dt,)), lambda b=sc2buf, dt=sc2dt: (sc2log(lambda b=b, dt=dt:
		SDTest.Call('ScorerFade_Draw', 'tviif', b, 0, dt)), sc2fl(b + 0x20, 2)))
SDTest.Call('ScorerFade_Start', 'tviiif', sc2buf, 0x112233, 0x445566, 1.0)
check('SC2 ScorerFade_Draw no alpha', lambda b=sc2buf: sc2log(lambda b=b: SDTest.Call('ScorerFade_Draw', 'tviif', b, 0, 0.5)))
check('SC2 ScorerFx_ctor', lambda b=sc2buf: (SDTest.Call('ScorerFx_ctor', 'tii', b)[0] == b, sc2save((sym('g_NoiseFactor'), sym('g_NoiseFactorBegin'),
	sym('g_NoiseTime'), sym('g_NoiseEndTime'), sym('g_MotionBlurFactorBegin'), sym('g_MotionBlurFactorEnd'), sym('g_MotionBlurTime'), sym('g_MotionBlurEndTime'), sym('g_MotionBlurOffVar')))))
sc2noise = SDTest.Peek('g_ScorerNoise', 'i')
for sc2f, sc2fb, sc2nt, sc2ne, sc2mb, sc2me, sc2mt, sc2mend, sc2off in ((200, 0, 0.5, 2.0, 0.2, 0.8, 0.25, 1.0, 0.0),
	(100, 50, 1.5, 2.0, 0.9, 0.3, 0.5, 2.0, 0.5), (0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0), (90, 0, 1.9, 2.0, 0.0,
	0.6, 0.9, 1.0, 1.0)):
	SDTest.Poke('g_NoiseFactor', sc2f)
	SDTest.Poke('g_NoiseFactorBegin', sc2fb)
	sc2setf(sym('g_NoiseTime') + 0x400000, (sc2nt,))
	sc2setf(sym('g_NoiseEndTime') + 0x400000, (sc2ne,))
	sc2setf(sym('g_MotionBlurFactorBegin') + 0x400000, (sc2mb,))
	sc2setf(sym('g_MotionBlurFactorEnd') + 0x400000, (sc2me,))
	sc2setf(sym('g_MotionBlurTime') + 0x400000, (sc2mt,))
	sc2setf(sym('g_MotionBlurEndTime') + 0x400000, (sc2mend,))
	sc2setf(sym('g_MotionBlurOffVar') + 0x400000, (sc2off,))
	sc2srand(77)
	check('SC2 ScorerFx_Draw' + fmt((sc2f, sc2fb, sc2nt, sc2ne, sc2mb, sc2me, sc2mt, sc2mend, sc2off)),
		lambda b=sc2buf: (sc2log(lambda b=b: SDTest.Call('ScorerFx_Draw', 'tviif', b, 0, 0.25)), sc2fl(sym('g_NoiseTime') + 0x400000,
		1), sc2fl(sym('g_NoiseEndTime') + 0x400000, 1), sc2fl(sym('g_MotionBlurTime') + 0x400000, 1), sc2fl(sym('g_MotionBlurEndTime') + 0x400000, 1),
		sc2fl(sym('g_SceneMotionBlurValue') + 0x400000, 1), sc2fl(sym('g_SceneMotionBlurOff') + 0x400000, 1), sc2noise and sc2fl(sc2noise + 0x24, 4)))
sc2restore(sc2gvars, sc2gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)
sc2free(sc2buf)

# ---- a scorer and an item on the heap ----
sc2item = sc2new(0xac)
check('SC2 cScorerItem_ctor', lambda a=sc2item: (SDTest.Call('cScorerItem_ctor', 'tii', a)[0] == a, sc2i(a) - 0x400000,
	sc2ints(a, (0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x2c, 0x30)), sc2b(a + 0x28), sc2b(a + 0x29), sc2b(a + 0xa8),
	map(lambda o, a=a: SDTest.Peek(a + o, '@p'), (4, 8, 0x34, 0x38, 0x3c, 0x40))))
for sc2v, sc2d, sc2n in (('X', 3.5, 0), ('FocusPivotY', -2.0, 0), ('MaxHintLineWidth', 0.0, 40), ('Visible', 0.0, 0),
	('MultiPress', 0.0, 7), ('DelMe', 0.0, 2), ('DelMe', 0.0, 1), ('Hint', 0.0, 0), ('Missing', 1.0, 1)):
	check('SC2 cScorerItem_Var set ' + sc2v, lambda a=sc2item, v=sc2v, d=sc2d, n=sc2n: SDTest.Call('cScorerItem_Var',
		'tiisiiDp', a, v, 1, 0, d, n))
	check('SC2 cScorerItem_Var get ' + sc2v, lambda a=sc2item, v=sc2v: SDTest.Call('cScorerItem_Var', 'tiisiiDp', a, v, 0, 0,
		0.0, 0))
check('SC2 cScorerItem_Delete', lambda a=sc2item: SDTest.Call('cScorerItem_Delete', 'tiii', a, 1)[0] == a)
sc2scb = sc2new(0x104)
check('SC2 cScorer_ctor', lambda a=sc2scb: (SDTest.Call('cScorer_ctor', 'tii', a)[0] == a, sc2i(a) - 0x400000,
	sc2ints(a, (4, 0x2c, 0x38, 0x3c, 0x40, 0x50, 0xa4, 0xa8, 0xb0, 0xbc, 0xc0, 0xcc, 0xd0)), sc2fl(a + 0xc, 5),
	sc2fl(a + 0x20, 2), sc2fl(a + 0xd4, 3), sc2fl(a + 0xe4, 1), sc2b(a + 0x8), sc2b(a + 0x44), sc2b(a + 0xe0),
	sc2b(a + 0x100), map(lambda o, a=a: SDTest.Peek(a + o, '@p'), (0x30, 0x34, 0x48, 0x4c)),
	sc2save((sym('g_RadarMMSelect'), sym('g_BarValue'), sym('g_BarStopValue'), sym('g_BarSpeedFactor'), sym('g_BarAutoUpdate'), sym('g_NumEnemiesInRadar'), sym('g_MissionSolidTime'), sym('g_NoiseFactor'), sym('g_MotionBlurOffVar'))),
	SDTest.Peek('g_CinemaBlackBandHeight', 'i') == SDTest.Peek(sym('GVar_CinemaBlackBandHeight') + 0x14, 'i'), SDTest.Peek('g_CameraScene', 'i') == SDTest.Peek(sym('GVar_CameraScene') + 0x14,
	'i')))
check('SC2 cScorer_Delete', lambda a=sc2scb: SDTest.Call('cScorer_Delete', 'tiii', a, 1)[0] == a)
sc2restore(sc2gvars, sc2gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)

# ---- the items of scorer 0 ----
# the menu animation state (the first main and sub menu constructors set it up with rand)
sc2animsize = 0x224
sc2anim = sc2new(sc2animsize)
SDTest.Call('crt_memcpy', 'iiii', sc2anim, sym('g_MainMenuAnimInit') + 0x400000, sc2animsize)
SDTest.Poke('g_MainMenuAnimInit', SDTest.Peek('g_MainMenuAnimInit', 'i') & ~0xffff)
sc2types = (('sc2Slider', 'Slider', 0), ('sc2Special', 'SpecialAction', 0), ('sc2Main', 'ScraplandMainMenu', 1),
	('sc2Sub', 'ScraplandSubMenu', 1), ('sc2Sprite', 'Sprite', 1), ('sc2Slider', 'Slider', 0), ('sc2Bad',
	'NoType', 0))
sc2items = {}
for sc2name, sc2type, sc2app in sc2types:
	sc2srand(1234)
	check('SC2 Add ' + sc2name + ' ' + sc2type, lambda n=sc2name, t=sc2type, p=sc2app: SScorer.Add(0, n, t, p))
	sc2a = 0
	if sc2sc:
		sc2a = sc2find(sc2name)
	sc2items[sc2name] = sc2a
	check('SC2 item ' + sc2name, lambda a=sc2a: a and (sc2i(a) - 0x400000, sc2i(a + 0x30)))
def sc2names():
	r = ()
	sc2it = sc2i(sc2sc + 0x40)
	while sc2it:
		r = r + ((SDTest.Peek(sc2it + 4, '@p'), sc2b(sc2it + 0x28)),)
		sc2it = sc2i(sc2it + 0x2c)
	return r
if sc2sc:
	check('SC2 item list', sc2names)
check('SC2 menu anim', lambda: (sc2b(sym('g_MainMenuAnimInit') + 0x400000), sc2b(sym('g_SubMenuAnimInit') + 0x400000), sc2fl(sym('g_MainMenuWait') + 0x400000, 9),
	sc2fl(sym('g_MainMenuTime') + 0x400000, 9), sc2fl(sym('g_MainMenuLength') + 0x400000, 9), sc2fl(sym('g_MainMenuSparkWait') + 0x400000, 19),
	sc2fl(sym('g_MainMenuSparkLength') + 0x400000, 19), map(sc2i, range(sym('g_MainMenuSparkFrame') + 0x400000, sym('g_MainMenuSparkFrame') + 0x400000 + 76, 4)),
	map(sc2b, range(sym('g_MainMenuSparkDir') + 0x400000, sym('g_MainMenuSparkDir') + 0x400000 + 19)), sc2fl(sym('g_MainMenuNeonWait') + 0x400000, 21),
	sc2fl(sym('g_MainMenuNeonGroup') + 0x400000, 3), sc2fl(sym('g_MainMenuNeonNext') + 0x400000, 1), sc2fl(sym('g_SubMenuWait') + 0x400000, 1),
	sc2fl(sym('g_SubMenuRingTime') + 0x400000, 1) + sc2fl(sym('g_SubMenuRingWait') + 0x400000, 1)))
sc2a = sc2items['sc2Slider']
if sc2a:
	check('SC2 ctor Slider', lambda a=sc2a: (sc2ints(a, (0xac, 0xb0, 0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0, 0xd4, 0xd8)),
		sc2fl(a + 0x10, 2), sc2b(a + 0xdc), sc2b(a + 0xdd), sc2b(a + 0xde), sc2i(a + 0xf4) != 0,
		sc2i(sc2i(a + 0xf4) + 0x34)))
sc2a = sc2items['sc2Special']
if sc2a:
	check('SC2 ctor SpecialAction', lambda a=sc2a: (sc2ints(a, (0xac, 0x11c, 0x120, 0x130, 0x134)),
		sc2i(a + 0xf0) == SDTest.Peek('g_FontHoratio', 'i'), map(lambda o, a=a: sc2i(a + o) != 0, (0xbc, 0xf4, 0x100,
		0x104, 0x108, 0x10c))))
sc2a = sc2items['sc2Main']
if sc2a:
	check('SC2 ctor MainMenu', lambda a=sc2a: (sc2i(a + 0xb4), map(lambda o, a=a: sc2i(a + o) != 0, (0xb8, 0xbc,
		0xc0, 0xc4)), map(lambda o, a=a: sc2i(sc2i(a + o) + 0x34), (0xb8, 0xbc, 0xc0, 0xc4))))
sc2a = sc2items['sc2Sub']
if sc2a:
	check('SC2 ctor SubMenu', lambda a=sc2a: (sc2i(a + 0xb8), sc2fl(a + 0xd4, 1), sc2fl(a + 0x10, 2),
		SDTest.Peek(a + 0xb4, '@p'), map(lambda o, a=a: sc2i(a + o) != 0, (0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0))))

# ---- the script variables ----
sc2basevars = (('X', 100.0), ('Y', 50.0), ('W', 200.0), ('H', 20.0), ('FocusPivotX', 1.0), ('Hint', 'sc2 hint'),
	('MaxHintLineWidth', 80), ('Left', 'sc2Special'), ('Up', 'sc2Main'), ('MultiPress', 1), ('Visible', 1),
	('Name', 'x'), ('OnAccept', ''))
sc2baseget = ('X', 'Y', 'W', 'H', 'FocusPivotX', 'Hint', 'MaxHintLineWidth', 'Left', 'Right', 'Up', 'Down',
	'MultiPress', 'Visible', 'Name', 'Missing')
sc2vars = (
	('sc2Slider', sc2basevars + (('File', '2D/white.tga'), ('HighRes', 0), ('BlendingMode', 2),
		('File', '2D/white.tga'), ('BackSpriteIndex', 0), ('SliderSpriteIndex', 0), ('FocusSpriteIndex', 0),
		('Red', 10), ('Green', 20), ('Blue', 30), ('Alpha', 255), ('Unit', 5), ('MinValue', 1.0), ('MaxValue', 5.0),
		('Value', 7.5), ('Value', 0.5), ('Value', 2.5), ('ValueStep', 0.25), ('MarginSize', 4.0),
		('MaxValueForced', 4.0), ('OnChange', '')),
		sc2baseget + ('HighRes', 'BackSpriteIndex', 'SliderSpriteIndex', 'FocusSpriteIndex', 'Red', 'Green', 'Blue',
		'Alpha', 'Unit', 'Value', 'ValueStep', 'MinValue', 'MaxValue', 'MarginSize', 'MaxValueForced', 'File')),
	('sc2Special', sc2basevars + (('CharAction', 3), ('CannonSprite', '2D/white.tga'), ('CannonIndex', 2),
		('CrossHairIndex', 3), ('Sprite', '2D/white.tga'), ('EnergyBackIndex', 4), ('EnergyBackOffX', 1.5),
		('EnergyBackOffY', 2.5), ('EnergyBarIndex', 5), ('EnergyBarOffX', 3.5), ('EnergyBarOffY', 4.5),
		('BatteryBarIndex', 6), ('BatteryBarOffX', 5.5), ('BatteryBarOffY', 6.5), ('BackIndex', 0), ('BackOffX', 7.5),
		('BackOffY', 8.5), ('IconSprite1', '2D/white.tga'), ('IconOffX', 9.5), ('IconOffY', 10.5),
		('IconIndexSprite0', 7), ('IconIndex', 8), ('GlowIconIndex', 9), ('InactiveColorRed', 1),
		('InactiveColorGreen', 2), ('InactiveColorBlue', 3), ('InactiveColorAlpha', 4), ('ActiveColorRed', 5),
		('ActiveColorAlpha', 6), ('GlowColorGreen', 7), ('GlowColorBlue', 300), ('TextOffX', 11.5), ('TextOffY', 12.5),
		('TextColorRed', 8), ('TextColorGreen', 9), ('TextColorBlue', 10), ('TextColorAlpha', 11),
		('TextFont', 'Horatio'), ('TextFont', 'sc2NoFont')),
		sc2baseget + ('CharAction', 'CannonIndex', 'CrossHairIndex', 'EnergyBackIndex', 'EnergyBackOffX',
		'EnergyBackOffY', 'EnergyBarIndex', 'EnergyBarOffX', 'EnergyBarOffY', 'BatteryBarIndex', 'BatteryBarOffX',
		'BatteryBarOffY', 'BackIndex', 'BackOffX', 'BackOffY', 'IconOffX', 'IconOffY', 'IconIndexSprite0',
		'IconIndex', 'GlowIconIndex', 'InactiveColorRed', 'InactiveColorGreen', 'InactiveColorBlue',
		'InactiveColorAlpha', 'ActiveColorRed', 'ActiveColorGreen', 'ActiveColorBlue', 'ActiveColorAlpha',
		'GlowColorRed', 'GlowColorGreen', 'GlowColorBlue', 'GlowColorAlpha', 'TextOffX', 'TextOffY', 'TextColorRed',
		'TextColorGreen', 'TextColorBlue', 'TextColorAlpha', 'Sprite', 'TextFont')),
	('sc2Main', (('FileLogo', '2D/white.tga'), ('FileGlow', '2D/white.tga'), ('HighRes', 0),
		('FileNeon', '2D/white.tga'), ('FileLighting', '2D/white.tga'), ('FileLighting', '2D/Noise.tga'),
		('X', 10.0)), ('HighRes', 'FileLogo', 'X')),
	('sc2Sub', (('FileLogo', '2D/white.tga'), ('FileGlow', '2D/white.tga'), ('HighRes', 0),
		('FileLighting', '2D/Noise.tga'), ('FileRingMask', '2D/white.tga'), ('FileRingReflection', '2D/white.tga'),
		('FileRingBorder', '2D/white.tga'), ('FileLighting', '2D/white.tga'), ('Scale', 2.5), ('Y', 3.0)),
		('HighRes', 'Scale', 'FileLogo', 'Y')))
for sc2name, sc2sets, sc2gets in sc2vars:
	for sc2v, sc2val in sc2sets:
		# the sprite files are loaded as multisprite sheets (flag 0x10000): a plain tga is a CHECK
		if type(sc2val) == type('') and (strop.find(sc2v, 'File') >= 0 or strop.find(sc2v, 'Sprite') >= 0):
			continue
		check('SC2 Set %s %s %s' % (sc2name, sc2v, repr(sc2val)), lambda n=sc2name, v=sc2v, x=sc2val: sc2set(n, v,
			x))
	for sc2v in sc2gets:
		# a get of HighRes CHECKs the int buffer it is handed (0 or 1), which SScorer.Get leaves unset
		if sc2v == 'HighRes':
			continue
		check('SC2 Get %s %s' % (sc2name, sc2v), lambda n=sc2name, v=sc2v: sc2get(n, v))
sc2a = sc2items['sc2Sub']
if sc2a:
	check('SC2 SubMenu file', lambda a=sc2a: (SDTest.Peek(a + 0xb4, '@p'), map(lambda o, a=a: sc2i(sc2i(a + o) +
		0x34), (0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0))))
sc2a = sc2items['sc2Special']
if sc2a:
	check('SC2 SpecialAction fields', lambda a=sc2a: (sc2i(a + 0xf0) == SDTest.Peek('g_FontHoratio', 'i'),
		sc2ints(a, (0xec, 0x124, 0x128, 0x12c)), map(lambda o, a=a: sc2i(sc2i(a + o) + 0x34), (0xbc, 0xf4, 0x100,
		0x108))))
	SDTest.Poke('g_SpecialActionActive', 0)
	check('SC2 SpecialAction Draw off', lambda a=sc2a: sc2log(lambda a=a: SDTest.Call('cSpecialActionScorer_Draw', 'tvii', a, 0)))
	# only frame 0 of the white sheets: every index back to 0, no icon (DrawIcon returns at once)
	for sc2v in ('CharAction', 'CannonIndex', 'CrossHairIndex', 'EnergyBackIndex', 'EnergyBarIndex',
		'BatteryBarIndex', 'BackIndex', 'IconIndex', 'GlowIconIndex'):
		sc2set('sc2Special', sc2v, 0)
	check('SC2 SpecialAction DrawIcon', lambda a=sc2a: (SDTest.Call('cSpecialActionScorer_DrawIcon', 'tviii', a, 0, 1),
		SDTest.Peek('g_SpecialActionState', 'i'), sc2fl(sym('g_SpecialActionFade') + 0x400000, 1)))
	check('SC2 SpecialAction DrawCrossHair', lambda a=sc2a: sc2log(lambda a=a: SDTest.Call('cSpecialActionScorer_DrawCrossHair', 'tvii', a,
		0)))
	sc2user = Scrap.UsrEntity(0)
	if sc2user and sc2user.IsType('WalkChar'):
		# with the battery bar on (0x44726c) DrawEnergy reads through a NULL in both runs: only off
		for sc2bar, sc2act, sc2t in ((0, 2, 12.5), (0, 1, 12.625)):
			SDTest.Poke('g_EnergyBarActive', sc2bar)
			SDTest.Poke('g_SpecialActionActive', sc2act)
			sc2setf(sc2sc + 0x20, (sc2t,))
			check('SC2 SpecialAction DrawEnergy' + fmt((sc2bar, sc2act, sc2t)), lambda a=sc2a: sc2log(lambda a=a:
				SDTest.Call('cSpecialActionScorer_DrawEnergy', 'tvi', a)))
			check('SC2 SpecialAction Draw' + fmt((sc2bar, sc2act, sc2t)), lambda a=sc2a: sc2log(lambda a=a:
				SDTest.Call('cSpecialActionScorer_Draw', 'tvii', a, 0)))
		# the handlers without the banker (its money label needs the frames 0x22, 0x23) and the gear aims
		SDTest.Poke('g_EnergyBarActive', 0)
		for sc2h in ('SpecialAction_DrawDefault', 'SpecialAction_DrawTarget', 'SpecialAction_DrawVictim', 'SpecialAction_DrawOn', 'SpecialAction_DrawLowEnergy', 'SpecialAction_DrawFunctionary', 'SpecialAction_DrawEnergyOnly', 'SpecialAction_DrawOn@178770'):
			check('SC2 SpecialAction handler %s' % sc2h, lambda a=sc2a, h=sc2h: SDTest.Call(h, 'tvii', a, 0))
		sc2setf(sc2sc + 0x20, (12.5,))
sc2restore(sc2gvars, sc2gsaved)
SDTest.Poke('g_ViewportW', 640)
SDTest.Poke('g_ViewportH', 480)

# ---- the slider draw, the cursor update ----
sc2a = sc2items['sc2Slider']
if sc2a and sc2sc:
	check('SC2 Slider Draw', lambda a=sc2a: (sc2log(lambda a=a: SDTest.Call('cSliderScorer_Draw', 'tvii', a, 0)),
		sc2fl(a + 0xc0, 2), sc2b(a + 0xdc), sc2b(a + 0xdd), sc2b(a + 0xde)))
	SScorer.SetCursor(0, 'sc2Slider')
	sc2setf(sc2sc + 0x2c, (-10.0,))
	check('SC2 cScorer_UpdateCursor', lambda a=sc2a: (sc2log(lambda: SDTest.Call('cScorer_UpdateCursor', 'tvii', sc2sc, 0)),
		sc2fl(a + 0x10, 2), sc2b(a + 0x29), sc2i(sc2i(a + 0xf4) + 0x34), sc2fl(sc2sc + 0x2c, 1),
		SDTest.Peek(sc2sc + 0x48, '@p'), SDTest.Peek(sc2sc + 0x30, '@p'), sc2b(sc2sc + 0x100)))
	sc2setf(sc2a + 0x10, (330.0, 245.0))
	check('SC2 cScorer_UpdateCursor hit', lambda a=sc2a: (sc2log(lambda: SDTest.Call('cScorer_UpdateCursor', 'tvii', sc2sc, 0)),
		sc2fl(a + 0x10, 2), sc2b(a + 0x29), sc2fl(sc2sc + 0x2c, 1), SDTest.Peek(sc2sc + 0x48, '@p'),
		SDTest.Peek(sc2sc + 0x30, '@p'), sc2i(sc2sc + 0x3c)))
	SDTest.Poke(sc2sc + 0x50 - 0x400000, 0)
	check('SC2 cScorer_UpdateMP', lambda: SDTest.Call('cScorer_UpdateMP', 'tvif', sc2sc, 0.0))

# ---- the cursor moves and the other scorer functions ----
if sc2sc:
	SDTest.Poke(sc2sc + 0x100 - 0x400000, 0)
	check('SC2 cScorer_FindItem', lambda: (sc2find('sc2Special') == sc2items['sc2Special'], sc2find('sc2NoItem'),
		sc2i(sc2sc + 4) == 0))
	check('SC2 cScorer_FindModel', lambda: (SDTest.Call('cScorer_FindModel', 'tiis', sc2sc, 'sc2NoModel')[0],
		SDTest.Call('cScorer_FindModelModel', 'tiis', sc2sc, 'sc2NoModel')[0]))
	check('SC2 cScorer_SetParam', lambda: SDTest.Call('cScorer_SetParam', 'tviss', sc2sc, 'sc2Param', 'value'))
	for sc2from, sc2to in ((None, None), ('sc2Slider', 'sc2Special'), (None, 'sc2Slider'), ('sc2Special',
		'sc2NoItem'), ('sc2NoItem', None)):
		check('SC2 cScorer_SetCursorItem' + fmt((sc2from, sc2to)), lambda f=sc2from, t=sc2to: (SDTest.Call('cScorer_SetCursorItem',
			'tviizz', sc2sc, 0, f, t), SDTest.Peek(sc2sc + 0x30, '@p'), sc2fl(sc2sc + 0x2c, 1), sc2fl(sc2sc + 0x38,
			1), sc2i(sc2sc + 0x3c)))
	for sc2d in ('sc2Main', 'sc2Main', 'sc2Sub'):
		check('SC2 cScorer_SetDefault ' + sc2d, lambda d=sc2d: (SDTest.Call('cScorer_SetDefault', 'tvis', sc2sc, d),
			SDTest.Peek(sc2sc + 0x48, '@p'), SDTest.Peek(sc2sc + 0x4c, '@p'), sc2fl(sc2sc + 0x2c, 1)))
	sc2skips = map(lambda k: SDTest.Peek(sc2all + 0x104 * k + 0xa4, '@i'), range(4))
	check('SC2 Scorer_SetInputState', lambda: (SDTest.Call('Scorer_SetInputState', 'vi', 5), map(lambda k: SDTest.Peek(sc2all +
		0x104 * k + 0xa4, '@i'), range(4))))
	for sc2k in range(4):
		SDTest.Poke(sc2all + 0x104 * sc2k + 0xa4 - 0x400000, sc2skips[sc2k])
	check('SC2 Scorer images', lambda: (SDTest.Call('Scorer_GetWhite', 'i')[0] == SDTest.Peek('g_ScorerWhite', 'i'),
		SDTest.Call('Scorer_GetMarker', 'i')[0] == SDTest.Peek('g_ScorerMarker', 'i'), SDTest.Call('Scorer_GetNoise', 'i')[0] ==
		SDTest.Peek('g_ScorerNoise', 'i'), SDTest.Peek('g_ScorerWhite', 'i') != 0))
	check('SC2 cScorer_SetMarkerSprite', lambda: (SDTest.Call('cScorer_SetMarkerSprite', 'tviz', sc2sc, None),
		SDTest.Peek('g_ScorerMarker', 'i') != 0, sc2i(SDTest.Peek('g_ScorerMarker', 'i') + 0x34)))
	# Scorer_PreloadTexture loads a multisprite sheet: a plain tga is a CHECK
	for sc2v in ('Life', 'sc2NoVar', ''):
		check('SC2 Scorer_GetCamVarStr ' + sc2v, lambda v=sc2v: SDTest.Call('Scorer_GetCamVarStr', 'vibsi', 0, 'none', v, 0x40)[1])
	if SDTest.Call('Scorer_GetCamVarStr', 'vibsi', 0, 'none', 'Life', 0x40)[1] != 'none':
		for sc2v in ('Life', 'sc2NoVar', ''):
			check('SC2 Scorer_GetCamVarInt ' + sc2v, lambda v=sc2v: SDTest.Call('Scorer_GetCamVarInt', 'iis', 0, v)[0])
	check('SC2 cScorer_DrawXboxMsgs off', lambda: sc2log(lambda: SDTest.Call('cScorer_DrawXboxMsgs', 'tvii', sc2sc, 0)))
	check('SC2 cScorer_DrawNoPadMsg big', lambda: sc2log(lambda: SDTest.Call('cScorer_DrawNoPadMsg', 'tviifii', sc2sc, 0, 300.0,
		1, 0)))
	check('SC2 cScorer_DrawNoPadMsg', lambda: sc2log(lambda: SDTest.Call('cScorer_DrawNoPadMsg', 'tviifii', sc2sc, 0, 300.0, 0,
		2)))
	SDTest.Poke('g_XboxNoPadMsg', 1)
	check('SC2 cScorer_DrawXboxMsgs pad', lambda: sc2log(lambda: SDTest.Call('cScorer_DrawXboxMsgs', 'tvii', sc2sc, 0)))
	SDTest.Poke('g_XboxNoPadMsg', 0)
	check('SC2 cScorer_DrawClouds', lambda: sc2log(lambda: SDTest.Call('cScorer_DrawClouds', 'tvii', sc2sc, 0)))
	sc2restore(sc2gvars, sc2gsaved)

# ---- the items deleted: Clear marks them, RemoveDeleted runs the deleting destructors ----
if sc2sc:
	SDTest.Poke(sc2sc + 0x100 - 0x400000, 0)
	check('SC2 SScorer.Clear(0)', lambda: (SScorer.Clear(0), sc2b(sc2sc + 8), sc2fl(sc2sc + 0x2c, 1)))
	check('SC2 cScorer_RemoveDeleted', lambda: (SDTest.Call('cScorer_RemoveDeleted', 'tvii', sc2sc, 0), sc2i(sc2sc + 0x40) != 0,
		sc2i(sc2sc + 4) != 0, sc2i(sc2sc + 0x50) != 0))
	check('SC2 items gone', lambda: map(lambda t: SScorer.GetItem(0, t[0]), sc2types))
	SDTest.Call('crt_memcpy', 'iiii', sym('g_MainMenuAnimInit') + 0x400000, sc2anim, sc2animsize)
	sc2free(sc2anim)
	for sc2k in range(5):
		SDTest.Poke(sc2sc + [0x20, 0x24, 0x28, 0x2c, 0x100][sc2k] - 0x400000, sc2scsaved[sc2k])
sc2restore(sc2gvars, sc2gsaved)
check('SC2 heap', lambda: SDTest.HeapOk())
