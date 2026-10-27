# engine3d Engine/Portal.c CPortal_ClipRect (execfile'd by sdlevel.py, Python 1.5.2): the screen box of portals made
# here, with the near and far planes of the camera of the world of the level and a view and projection set here (the
# ones of the frame are put back after).
def rl2new(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def rl2i(a):
	return SDTest.Peek(a, '@i')
def rl2ints(a, n):
	r = []
	for i in range(n):
		r.append(rl2i(a + 4 * i))
	return r
def rl2pf(a, t):
	SDTest.Call(0x3197d0, 'iiFi', a, t, 4 * len(t))
rl2saved = rl2ints(0x4ae3f8 + 0x400000, 16) + rl2ints(0x4ae4b8 + 0x400000, 16)
rl2pf(0x4ae3f8 + 0x400000, (1.2, 0.0, 0.0, 0.0, 0.0, 1.6, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, -1.0, 0.0))
rl2pf(0x4ae4b8 + 0x400000, (0.8, 0.0, 0.6, 0.0, 0.0, 1.0, 0.0, 0.0, -0.6, 0.0, 0.8, 0.0, 2.0, -1.0, 3.0, 1.0))
rl2rect = rl2new(24)
rl2mundo = SDTest.Peek(0x4afde8, 'i')
check('mundo cam', lambda: (rl2mundo != 0, rl2mundo and rl2i(rl2mundo + 0xa4) != 0))
rl2cases = ()
if rl2mundo and rl2i(rl2mundo + 0xa4) and rl2i(rl2i(rl2mundo + 0xa4) + 0x2c) > 0:
	rl2cases = (((0.0, 0.0, 20.0), (4.0, 2.0)), ((3.0, 4.0, 50.0), (10.0, 6.0)), ((-2.0, 1.0, -3.0), (4.0, 20.0)),
		((1.0, -1.0, -40.0), (4.0, 2.0)), ((0.0, 0.0, 1000000.0), (4.0, 2.0)), ((0.0, 3.0, 0.5), (30.0, 3.0)))
	check('camera planes', lambda: rl2ints(rl2i(rl2mundo + 0xa4) + 0x2c, 2))
for rl2pos, rl2size in rl2cases:
	rl2n = SDTest.Call(0x23cf40, 'tii', rl2new(0xfc))[0]
	SDTest.Poke(rl2n + 8 - 0x400000, 1)
	rl2pf(rl2n + 0x5c, (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0) + rl2pos + (1.0,))
	rl2po = SDTest.Call(0x2ba0e0, 'tii', rl2new(0x68))[0]
	SDTest.Call(0x2cad70, 'tvii', rl2po, rl2n)
	rl2pf(rl2po + 0xc, rl2size)
	rl2pf(rl2rect, (9.0, 9.0, 9.0, 9.0, 9.0, 9.0))
	check('CPortal_ClipRect' + fmt((rl2pos, rl2size)), lambda: (SDTest.Call(0x2ba930, 'tbii', rl2po, rl2rect)[0],
		rl2ints(rl2rect, 6)))
	check('CPortal_ClipRect NULL' + fmt((rl2pos, rl2size)), lambda: SDTest.Call(0x2ba930, 'tbii', rl2po, 0)[0])
for rl2k in range(16):
	SDTest.Poke(0x4ae3f8 + 4 * rl2k, rl2saved[rl2k])
	SDTest.Poke(0x4ae4b8 + 4 * rl2k, rl2saved[16 + rl2k])
