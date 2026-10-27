# Rest B (game leftovers from 0x240000, execfile'd by sdlevel.py, Python 1.5.2): the visibility test of the models
# of the render list in pass 1 (their last world matrix), and what the level frames drew with the C versions of
# CModelo_Render and the LOD choice (the flags they leave on the models).
def rsbli(a):
	return SDTest.Peek(a, '@i')
def rsblb(a):
	return SDTest.Peek(a, '@i') & 0xff
rsblpass = SDTest.Peek(0x4afdb8, 'i')
rsbln = SDTest.Peek(0x4af7c8, 'i')
check('models in the render list', lambda: rsbln > 0)
SDTest.Poke(0x4afdb8, 1)
rsblk = 0
while rsblk < rsbln and rsblk < 60:
	rsblm = rsbli(SDTest.Peek(0x4ae7c8 + rsblk * 4, 'i'))
	if rsblm:
		check('CModelo_Visibility' + fmt((rsblk, SDTest.Peek(rsbli(rsblm + 4), '@s'))), lambda: (
			SDTest.Call(0x254940, 'tbii', rsblm, rsblm + 0xb0)[0], rsblb(rsblm + 0x120), rsblb(rsblm + 0x121),
			rsbli(rsblm + 0x100)))
	rsblk = rsblk + 1
SDTest.Poke(0x4afdb8, rsblpass)
def rsblfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
SDTest.Poke(0x4afdb8, 1)
rsblk = 0
while rsblk < rsbln and rsblk < 12:
	rsblm = rsbli(SDTest.Peek(0x4ae7c8 + rsblk * 4, 'i'))
	if rsblm:
		check('CModelo_Render' + fmt((rsblk,)), lambda: (SDTest.Call(0x253610, 'tvi', rsblm), rsblb(rsblm + 0x121),
			rsblfl(rsblm + 0xb0, 16)))
		check('CModelo_UpdateLOD' + fmt((rsblk,)), lambda: SDTest.Call(0x2542c0, 'tbi', rsblm)[0])
	rsblk = rsblk + 1
SDTest.Poke(0x4afdb8, rsblpass)
