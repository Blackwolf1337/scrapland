# engine3d Engine/gety.cpp (execfile'd by sdlevel.py, Python 1.5.2): the height of the floor of FZ on a grid of
# points around the player (pp) with the query functions of the floor, their flags, the sector mode of the vertical
# ray (g_RayoVertSectors, put back after) and the cell accessor of the grid. The hit triangle is only compared.
def en2li(a):
	return SDTest.Peek(a, '@i')
def en2lz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def en2lfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def en2lset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, tuple(v), 4 * len(v))
en2lsuelo = en2li(mundo + 0xb0)
check('gety suelo', lambda: en2lsuelo != 0)
en2lalt = en2lsuelo + 0x70
en2lpts = []
for dx in (-300.0, -40.0, 0.0, 25.0, 500.0):
	for dz in (-250.0, 0.0, 60.0, 900.0):
		en2lpts.append((pp[0] + dx, pp[2] + dz))
en2lpts.append((-1e6, 1e6))
en2ly = pp[1]
en2linfo = en2lz(0x20)
# SueloRayo: the fields in, the hit out
def en2lrayo(rva, x, z, flags, miny, y, minb, maxb):
	SDTest.Poke(en2linfo - 0x400000, flags)
	en2lset(en2linfo + 4, (miny, y, minb, maxb, 7.0))
	SDTest.Poke(en2linfo + 0x18 - 0x400000, 0)
	SDTest.Poke(en2linfo + 0x1c - 0x400000, 1234)
	r = SDTest.Call(rva, 'tbiffi', en2lsuelo, x, z, en2linfo)[0]
	return (r, en2lfl(en2linfo + 0x14, 1), en2li(en2linfo + 0x18) != 0, en2li(en2linfo + 0x1c))
if en2lsuelo:
	for x, z in en2lpts:
		for maxy in (en2ly + 50.0, en2ly - 20.0, 1e5):
			check('CSuelo_GetY' + fmt((x, z, maxy)), lambda x=x, z=z, maxy=maxy: (SDTest.Call(0x2c9da0, 'tfifffi',
				en2lsuelo, x, z, maxy, 0)[0], SDTest.Call(0x2c9da0, 'tfifffi', en2lsuelo, x, z, maxy, 1)[0]))
			check('CSuelo_GetYRange' + fmt((x, z, maxy)), lambda x=x, z=z, maxy=maxy: (SDTest.Call(0x2ca7f0, 'tfiffffi',
				en2lsuelo, x, z, maxy, maxy - 200.0, 0)[0], SDTest.Call(0x2ca7f0, 'tfiffffi', en2lsuelo, x, z, maxy,
				-1e5, 1)[0]))
			check('CSuelo_GetYBelow' + fmt((x, z, maxy)), lambda x=x, z=z, maxy=maxy: (SDTest.Call(0x2caab0, 'tbifffi',
				en2lsuelo, x, z, maxy, 0)[0], SDTest.Call(0x2caab0, 'tbifffF', en2lsuelo, x, z, maxy, (5.0,))[0:2]))
			for flags, miny, minb, maxb in ((2, 0.0, 0.0, 0.0), (3, en2ly - 100.0, 0.0, 0.0), (0xe, 0.0, 0.5, 0.9),
					(0x10a, 0.0, 0.0, 0.95), (0x302, 0.0, 0.0, 0.0), (0x206, 0.0, 0.7, 0.0), (0, 0.0, 0.0, 0.0)):
				check('CSuelo_GetYInfo' + fmt((x, z, maxy, flags)), lambda x=x, z=z, maxy=maxy, flags=flags, miny=miny,
					minb=minb, maxb=maxb: en2lrayo(0x2c9ff0, x, z, flags, miny, maxy, minb, maxb))
		for sect in (0, 1):
			SDTest.Poke(0x4e6e10, sect)
			for maxy in (en2ly + 50.0, en2ly - 20.0, 1e5):
				check('CSuelo_RayoVert' + fmt((x, z, maxy, sect)), lambda x=x, z=z, maxy=maxy: en2lrayo(0x2ca360, x, z,
					0x202, 0.0, maxy, 0.0, 0.0))
	SDTest.Poke(0x4e6e10, 0)
	en2lnx, en2lnz = en2li(en2lalt + 0x3498), en2li(en2lalt + 0x349c)
	check('CAlturas grid', lambda: (en2lnx, en2lnz, en2li(en2lalt + 0x3490)))
	for cx, cz in ((0, 0), (en2lnx - 1, en2lnz - 1), (en2lnx, 0), (0, en2lnz), (-1, 3), (en2lnx / 2, en2lnz / 3)):
		check('CAlturas_GetCell' + fmt((cx, cz)), lambda cx=cx, cz=cz: (lambda r: (r != 0, r and (r - en2li(en2lalt +
			0x3494)) / 0x24))(SDTest.Call(0x2cad00, 'tiiii', en2lalt, cx, cz)[0]))
SDTest.Call(0x133550, 'vi', en2linfo)
