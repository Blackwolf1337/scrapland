# engine3d Engine/Bsp.c in the level FZ (execfile'd by sdlevel.py, Python 1.5.2): the parts that need the sectors of
# the level: the sector of a node and its name, the sector lists of a fake map. The light maps themselves are off
# (mapa_sombras usar), so the functions mundo.cpp calls only return.
def bspli(a):
	return SDTest.Peek(a, '@i')
def bsplz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def bsplpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def bsplidx(p, lst):
	for i in range(len(lst)):
		if lst[i] == p:
			return i
	if p:
		return 'ptr'
	return None
bspln = SDTest.Call(0x26d0a0, 'i')[0]
check('bsp level', lambda: (SDTest.Peek(0x4b2af8, 'i'), bspln, SDTest.Peek(0x4b2b38, 'i') != 0))
bspltri = bsplz(0x224)
for bspl_z in (-1, 0, 1, 2, bspln - 1):
	if bspl_z < bspln:
		bsplpoke(bspltri + 0x1c8, bspl_z)
		check('BspTri_ZonaName' + fmt((bspl_z,)), lambda: SDTest.Call(0x290680, 'si', bspltri)[0])
# the nodes of the floor scene
bsplm = SDTest.Peek(0x4afde8, 'i')
bsplnodo = bspli(bspli(bsplm + 0xac) + 0xc)
bspli0 = 0
while bsplnodo and bspli0 < 40:
	check('BspTri_Zona' + fmt((bspli0,)), lambda bspl_n=bsplnodo: (SDTest.Peek(bspl_n + 4, '@p'), SDTest.Call(0x290650, 'ii', bspl_n)[0]))
	bsplnodo = bspli(bsplnodo + 0xc)
	bspli0 = bspli0 + 1
# a map with lists in the sectors 0 and 1 and none
bsplmat = bsplz(0xb0)
SDTest.Call(0x233830, 'tii', bsplmat)
bsplmap = bsplz(0x61aa8)
bspll = []
for bspl_z in (0, -1, 1, 0, -1):
	bspl_t = bsplz(0x224)
	SDTest.Call(0x281d40, 'tii', bspl_t)
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
check('CBsp_CrearZonas', lambda: (SDTest.Call(0x285be0, 'tvi', bsplmap), bsplzonas()))
check('Tabla_FreeB4', lambda: (SDTest.Call(0x281fe0, 'tvi', bsplmap + 0x61a98), bspli(bsplmap + 0x61a98)))
