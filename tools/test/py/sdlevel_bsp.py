# engine3d Engine/Bsp.c in the level FZ (execfile'd by sdlevel.py, Python 1.5.2): the parts that need the sectors of
# the level: the sector of a node and its name, the sector lists of a fake map. The light maps themselves are off
# (mapa_sombras usar), so the functions mundo.cpp calls only return.
def bspli(a):
	return SDTest.Peek(a, '@i')
def bsplz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def bsplpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def bsplidx(p, lst):
	for i in range(len(lst)):
		if lst[i] == p:
			return i
	if p:
		return 'ptr'
	return None
bspln = SDTest.Call('Sector_Num', 'i')[0]
check('bsp level', lambda: (SDTest.Peek('g_ShadowMapUse', 'i'), bspln, SDTest.Peek('g_Bsp', 'i') != 0))
bspltri = bsplz(0x224)
for bspl_z in (-1, 0, 1, 2, bspln - 1):
	if bspl_z < bspln:
		bsplpoke(bspltri + 0x1c8, bspl_z)
		check('BspTri_ZoneName' + fmt((bspl_z,)), lambda: SDTest.Call('BspTri_ZoneName', 'si', bspltri)[0])
# the nodes of the floor scene
bsplm = SDTest.Peek('g_World', 'i')
bsplnodo = bspli(bspli(bsplm + 0xac) + 0xc)
bspli0 = 0
while bsplnodo and bspli0 < 40:
	check('BspTri_Zone' + fmt((bspli0,)), lambda bspl_n=bsplnodo: (SDTest.Peek(bspl_n + 4, '@p'), SDTest.Call('BspTri_Zone', 'ii', bspl_n)[0]))
	bsplnodo = bspli(bsplnodo + 0xc)
	bspli0 = bspli0 + 1
# a map with lists in the sectors 0 and 1 and none
bsplmat = bsplz(0xb0)
SDTest.Call('CMaterial_ctor', 'tii', bsplmat)
bsplmap = bsplz(0x61aa8)
bspll = []
for bspl_z in (0, -1, 1, 0, -1):
	bspl_t = bsplz(0x224)
	SDTest.Call('BspTri_ctor', 'tii', bspl_t)
	bsplpoke(bspl_t + 0xac, bsplmat)
	if bspl_z < bspln:
		bsplpoke(bspl_t + 0x1c8, bspl_z)
	bsplpoke(bsplmap + 8 + 4 * len(bspll), bspl_t)
	bspll.append(bspl_t)
bsplpoke(bsplmap + 0x4e208, len(bspll))
def bsplchain(t):
	r = []
	while t:
		r.append(bsplidx(t, bspll))
		t = bspli(t + 0x1cc)
	return r
def bsplzonas():
	n, p = bspli(bsplmap + 0x61a98), bspli(bsplmap + 0x61a9c)
	r = [n]
	for i in range(min(n, 4)):
		r.append(bsplchain(bspli(p + 0xb4 * i)))
	return r
check('CBsp_CreateZones', lambda: (SDTest.Call('CBsp_CreateZones', 'tvi', bsplmap), bsplzonas()))
check('Table_FreeB4', lambda: (SDTest.Call('Table_FreeB4', 'tvi', bsplmap + 0x61a98), bspli(bsplmap + 0x61a98)))
# two regions near the player linked through the floor trace (and one through a wall of the level, maybe)
def bsplreg(c, n, col, other):
	r = bsplz(0x64)
	SDTest.Call('crt_memcpy', 'iiFi', r + 0x18, (2.0,) + c + n + (0.0, 0.0, 0.0) + col, 4 * 14)
	bsplpoke(r + 0x10, other)
	return r
def bsplacum(r):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * 4, r + 0x50, 16)[1]
bsplp = (pp[0], pp[1] + 50.0, pp[2])
for bspl_d in ((0.0, 0.0, 300.0), (0.0, 0.0, 3000.0), (0.0, -500.0, 10.0)):
	bspla = bsplreg(bsplp, (0.0, 0.0, 1.0), (0.5, 0.25, 1.0, 1.0), 1)
	bsplb = bsplreg((bsplp[0] + bspl_d[0], bsplp[1] + bspl_d[1], bsplp[2] + bspl_d[2]), (0.0, 0.0, -1.0),
		(1.0, 1.0, 0.5, 1.0), 1)
	check('BspRegion_Link' + fmt(bspl_d), lambda: (SDTest.Call('BspRegion_Link', 'tiiiiif', bsplmap, bspla, bsplb, 0, 0.001)[0],
		bsplacum(bspla), bsplacum(bsplb)))
# rays from a point above the floor to a light, the random part seeded
for bspl_l, bspl_r, bspl_n in (((0.0, 300.0, 0.0), 5.0, 16), ((0.0, -300.0, 0.0), 2.0, 8), ((200.0, 20.0, 0.0), 0.0, 4),
		((0.0, 0.0, 0.0), 1.0, 0)):
	SDTest.Call('crt_srand', 'vi', 0x1234)
	check('Bsp_Occlusion' + fmt((bspl_l, bspl_r, bspl_n)), lambda: SDTest.Call('Bsp_Occlusion', 'fFFfiFi', bsplp, (0.0, 1.0, 0.0),
		bspl_r, bspl_n, (bsplp[0] + bspl_l[0], bsplp[1] + bspl_l[1], bsplp[2] + bspl_l[2]),
		bspli(SDTest.Peek('g_World', 'i') + 0xb0))[0])
