# engine3d Engine/Texto.c: the message stack (with the game's own stack put aside) and the text cursor. Python 1.5.2,
# runs inside sdcall.py after the other checks: check, fmt come from there. The GDI output itself is not compared;
# Text_DrawStack is not tested (its text needs the viewport of a frame being drawn, Python runs between frames).
def txi(a):
	return SDTest.Peek(a, '@i')
def txpila():
	r = []
	m = SDTest.Peek('g_TextStack', 'i')
	while m:
		r.append((txi(m), txi(m + 4), SDTest.Peek(m + 8, '@s')))
		m = txi(m + 0x108)
	return r
def txcur():
	return (SDTest.Peek('g_TextX', 'i'), SDTest.Peek('g_TextY', 'i'))
txsaved = (SDTest.Peek('g_TextStack', 'i'), txcur())
SDTest.Poke('g_TextStack', 0)
for ms, s in ((0, 'one'), (3000, 'dos %s'), (-2500, 'tres'), (0, 'cuatro'), (0, 'cinco'), (0, 'seis'), (-1, 'siete')):
	check('Text_Message' + fmt((ms, s)), lambda ms=ms, s=s: (SDTest.Call('Text_Message', 'viss', ms, s, 'x'), txpila()))
check('Text_Push', lambda: (SDTest.Call('Text_Push', 'i')[0] != 0, txpila()))
check('Text_Pop', lambda: (SDTest.Call('Text_Pop', 'v'), txpila()))
while SDTest.Peek('g_TextStack', 'i'):
	SDTest.Call('Text_Pop', 'v')
SDTest.Poke('g_TextStack', txsaved[0])
check('Text_Printf', lambda: (SDTest.Call('Text_Printf', 'viis', 100, 50, 'sd %d'), txcur()))
check('Text_Printf none', lambda: (SDTest.Call('Text_Printf', 'viiz', 7, 9, None), txcur()))
check('Text_PrintfRel', lambda: (SDTest.Call('Text_PrintfRel', 'viis', 10, -20, 'rel'), txcur()))
check('Text_PrintfRel none', lambda: (SDTest.Call('Text_PrintfRel', 'viiz', -3, 4, None), txcur()))
for p in ((0.0, 0.0, 0.0), (10.0, 50.0, 300.0), (0.0, 0.0, -1000.0)):
	check('Text_Printf3D' + fmt(p), lambda p=p: (SDTest.Call('Text_Printf3D', 'vFs', p, '3d'), txcur()))
SDTest.Poke('g_TextX', txsaved[1][0])
SDTest.Poke('g_TextY', txsaved[1][1])
