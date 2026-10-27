# engine3d leftovers 0x29c000..0x2b2000 in the level FZ (execfile'd by sdlevel.py, Python 1.5.2): the lens flares
# drawn through the D3D log with a view and a projection set here (the camera of gMundo for the near plane and the
# aspect, the floor of FZ for the traces), in the four patterns, at the edges of the screen and with the fade of the
# traces. The view, the projection, the time and the render list are put back.
def en3lri(a):
	return SDTest.Peek(a, '@i')
def en3lwi(a, v):
	SDTest.Poke(a - 0x400000, v)
def en3lwf(a, vals):
	SDTest.Call(0x3197d0, 'iiFi', a, vals, 4 * len(vals))
def en3lrf(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def en3llog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
en3lmundo = SDTest.Peek(0x4afde8, 'i')
en3lcam = en3lri(en3lmundo + 0xa4)
en3lmat = SDTest.Peek(0x4db7c4, 'i')
check('lens flare level', lambda: (en3lcam != 0, en3lmat != 0, SDTest.Peek(0x4db7d8, 'i'), SDTest.Peek(0x4db800, 'i')))
if en3lcam and en3lmat:
	en3lview = en3lrf(0x8ae4b8, 16)
	en3lproj = en3lrf(0x8ae3f8, 16)
	en3lsave = (SDTest.Peek(0x4a8ed0, 'i'), SDTest.Peek(0x4db7d8, 'i'), SDTest.Peek(0x4db800, 'i'))
	en3llist = []
	for en3li in range(8):
		en3llist.append(SDTest.Peek(0x4db7e0 + 4 * en3li, 'i'))
	SDTest.Call(0x24cde0, 'iF', (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 30.0, 0.0, 1.0))
	SDTest.Call(0x24c6a0, 'iF', (1.0, 0.0, 0.0, 0.0, 0.0, 1.333, 0.0, 0.0, 0.0, 0.0, 1.001, 1.0, 0.0, 0.0, -0.1, 0.0))
	en3lwf(0x8a8ed0, (1000.0,))
	SDTest.Poke(0x4db7d8, 1)
	SDTest.Poke(0x4db800, 0)
	en3lf = SDTest.Call(0x2a5dd0, 'tii', SDTest.Call(0x1334d0, 'ii', 0x50)[0])[0]
	SDTest.Call(0x2a5fa0, 'tvii', en3lf, 1)
	def en3lstate(f=en3lf):
		return (en3lrf(f + 0x24, 2), en3lrf(f + 0x30, 3), SDTest.Peek(f + 0x3c, '@b'), en3lri(f + 0x4c))
	def en3lrender(pos, tipo, rel, f=en3lf):
		en3lwf(f + 8, pos)
		en3lwi(f + 0x1c, tipo)
		SDTest.Call(0x2a7510, 'tvii', f, rel)
		return (en3llog(lambda: SDTest.Call(0x2a6160, 'v')), en3lstate())
	for en3lpos in ((0.0, -30.0, 10.0), (0.5, -29.7, 10.0), (1.1, -30.0, 10.0), (-1.15, -30.9, 10.0),
			(1.3, -30.0, 10.0), (0.0, -31.25, 10.0), (0.0, -30.0, -5.0), (0.25, -29.9, 0.001)):
		for en3ltipo in (0, 1, 2, 3, 4):
			check('LensFlare_RenderAll' + fmt((en3lpos, en3ltipo)), lambda p=en3lpos, t=en3ltipo:
				en3lrender(p, t, 0))
	check('LensFlare_RenderAll relativo', lambda: en3lrender((0.5, 0.25, 10.0), 1, 1))
	SDTest.Call(0x2a74e0, 'tvif', en3lf, 1.75)
	SDTest.Call(0x2a74a0, 'tvif', en3lf, 0.5)
	SDTest.Call(0x2a7460, 'tvif', en3lf, 0.6)
	check('LensFlare_RenderAll escala', lambda: en3lrender((1.2, -30.4, 10.0), 0, 0))
	# the traces against the floor of FZ, from the eye of the view at (0, -30, 0), over time
	SDTest.Call(0x2a7480, 'tvif', en3lf, 0.1)
	for en3lpos in ((0.0, -30.0, 10.0), (0.0, -10.0, 300.0), (5.0, -60.0, 40.0)):
		for en3lt in (1000.0, 1040.0, 1080.0, 1250.0, 1600.0, 5000.0):
			en3lwf(0x8a8ed0, (en3lt,))
			check('LensFlare_RenderAll trace' + fmt((en3lpos, en3lt)), lambda p=en3lpos: en3lrender(p, 2, 0))
	SDTest.Call(0x2a7460, 'tvif', en3lf, 0.0)
	check('LensFlare_RenderAll intens 0', lambda: en3lrender((0.0, -30.0, 10.0), 0, 0))
	SDTest.Poke(0x4db7d8, 0)
	check('LensFlare_RenderAll off', lambda: en3lrender((0.0, -30.0, 10.0), 0, 0))
	SDTest.Call(0x2a5e70, 'tvi', en3lf)
	SDTest.Call(0x1334e0, 'vi', en3lf)
	SDTest.Call(0x24c6a0, 'iF', en3lproj)
	SDTest.Call(0x24cde0, 'iF', en3lview)
	SDTest.Poke(0x4a8ed0, en3lsave[0])
	SDTest.Poke(0x4db7d8, en3lsave[1])
	SDTest.Poke(0x4db800, en3lsave[2])
	for en3li in range(8):
		SDTest.Poke(0x4db7e0 + 4 * en3li, en3llist[en3li])
	check('lens flare restored', lambda: (en3lrf(0x8ae4b8, 16) == en3lview, en3lrf(0x8ae3f8, 16) == en3lproj))
