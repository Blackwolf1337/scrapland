# engine3d Engine/Texto.c: the message stack (with the game's own stack put aside) and the text cursor. Python 1.5.2,
# runs inside sdcall.py after the other checks: check, fmt come from there. The GDI output itself is not compared;
# Texto_DrawPila is not tested (its text needs the viewport of a frame being drawn, Python runs between frames).
def txi(a):
	return SDTest.Peek(a, '@i')
def txpila():
	r = []
	m = SDTest.Peek('g_TextoPila', 'i')
	while m:
		r.append((txi(m), txi(m + 4), SDTest.Peek(m + 8, '@s')))
		m = txi(m + 0x108)
	return r
def txcur():
	return (SDTest.Peek('g_TextoX', 'i'), SDTest.Peek('g_TextoY', 'i'))
txsaved = (SDTest.Peek('g_TextoPila', 'i'), txcur())
SDTest.Poke('g_TextoPila', 0)
for ms, s in ((0, 'uno'), (3000, 'dos %s'), (-2500, 'tres'), (0, 'cuatro'), (0, 'cinco'), (0, 'seis'), (-1, 'siete')):
	check('Texto_Mensaje' + fmt((ms, s)), lambda ms=ms, s=s: (SDTest.Call('Texto_Mensaje', 'viss', ms, s, 'x'), txpila()))
check('Texto_Push', lambda: (SDTest.Call('Texto_Push', 'i')[0] != 0, txpila()))
check('Texto_Pop', lambda: (SDTest.Call('Texto_Pop', 'v'), txpila()))
while SDTest.Peek('g_TextoPila', 'i'):
	SDTest.Call('Texto_Pop', 'v')
SDTest.Poke('g_TextoPila', txsaved[0])
check('Texto_Printf', lambda: (SDTest.Call('Texto_Printf', 'viis', 100, 50, 'sd %d'), txcur()))
check('Texto_Printf none', lambda: (SDTest.Call('Texto_Printf', 'viiz', 7, 9, None), txcur()))
check('Texto_PrintfRel', lambda: (SDTest.Call('Texto_PrintfRel', 'viis', 10, -20, 'rel'), txcur()))
check('Texto_PrintfRel none', lambda: (SDTest.Call('Texto_PrintfRel', 'viiz', -3, 4, None), txcur()))
for p in ((0.0, 0.0, 0.0), (10.0, 50.0, 300.0), (0.0, 0.0, -1000.0)):
	check('Texto_Printf3D' + fmt(p), lambda p=p: (SDTest.Call('Texto_Printf3D', 'vFs', p, '3d'), txcur()))
SDTest.Poke('g_TextoX', txsaved[1][0])
SDTest.Poke('g_TextoY', txsaved[1][1])
