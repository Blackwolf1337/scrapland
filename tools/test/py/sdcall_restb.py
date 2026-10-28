# Rest B (game leftovers from 0x240000, execfile'd by sdcall.py, Python 1.5.2): the texture animation counter
# (ControlAnimMat.c) on calloc memory, the CTabla resizes, Vec3_Normalize, the stub CListaVert_Optimize, the
# parsers of the command file (Cmd.c) on a file written here and Cmd_ReadFile on an empty one (an error in
# Cmd_ReadFile is fatal through Err_Printf, a model load reads files: not called).
def rsbz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def rsbi(a):
	return SDTest.Peek(a, '@i')
def rsbfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def rsbset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def rsbfree(a):
	SDTest.Call(0x133550, 'vi', a)

# CAnimMat: fps, frames, ciclo, dt, frame, framef, modo, fundRate, fundK (lastTime and lastFrame are the clock)
def rsbam(a):
	f = rsbfl(a, 9)
	return (f[0], rsbi(a + 4), rsbi(a + 8), f[3], rsbi(a + 0x10), f[5], rsbi(a + 0x18), f[7], f[8])
rsbnow = SDTest.Peek(0x4a8ef0, 'i')
rsba = rsbz(0x2c)
rsbb = rsbz(0x2c)
check('CAnimMat_ctor', lambda: (SDTest.Call(0x257f70, 'tii', rsba)[0] == rsba, rsbam(rsba), rsbi(rsba + 0x28)))
check('AnimMat_Set', lambda: (SDTest.Call(0x257fc0, 'bifi', rsba, 12.5, 2)[0], rsbam(rsba)))
check('AnimMat_Set NULL', lambda: SDTest.Call(0x257fc0, 'bifi', 0, 12.5, 2)[0])
SDTest.Poke(rsba + 4 - 0x400000, 6)
check('CAnimMat_ctorCopy', lambda: (SDTest.Call(0x257f10, 'tiii', rsbb, rsba)[0] == rsbb, rsbam(rsbb), rsbi(rsbb + 0x28)))
check('CAnimMat_Init', lambda: (SDTest.Call(0x2580d0, 'tvi', rsbb), rsbam(rsbb)))
check('CAnimMat_dtor', lambda: (SDTest.Call(0x257fb0, 'tvi', rsbb), rsbam(rsbb)))
for rsbk in (0, 1, 2, 3):
	check('AnimMat_ModoName' + fmt((rsbk,)), lambda: SDTest.Call(0x258000, 'si', rsbk))
for rsbv in (0.0, 2.75, -1.5, 5.999):
	check('CAnimMat_SetFrameF' + fmt((rsbv,)), lambda: (SDTest.Call(0x2580b0, 'tvif', rsba, rsbv), rsbam(rsba)))
check('CAnimMat_SetFrame', lambda: (SDTest.Call(0x258090, 'tvii', rsba, 3), rsbam(rsba)))
check('CAnimMat_Start', lambda: (SDTest.Call(0x258110, 'tvi', rsba), rsbam(rsba), rsbi(rsba + 0x24) == rsbnow))
check('CAnimMat_Stop', lambda: (SDTest.Call(0x258140, 'tvi', rsba), rsbam(rsba)))
check('CAnimMat_Pause', lambda: (SDTest.Call(0x258150, 'tvi', rsba), rsbam(rsba)))
check('CAnimMat_FundPausa not 2', lambda: (SDTest.Call(0x258160, 'tvif', rsba, 500.0), rsbam(rsba)))
SDTest.Poke(rsba + 0x18 - 0x400000, 2)
check('CAnimMat_FundPausa', lambda: (SDTest.Call(0x258160, 'tvif', rsba, 500.0), rsbam(rsba)))
check('CAnimMat_FundPausa 0', lambda: (SDTest.Call(0x258160, 'tvif', rsba, 0.0), rsbam(rsba)))
check('CAnimMat_Play', lambda: (SDTest.Call(0x258080, 'tvi', rsba), rsbam(rsba)))
# steps of ms milliseconds: lastTime set back from the clock (constant while the script runs)
def rsbstep(a, modo, ms, fps, frames, ciclo, framef, fund):
	rsbset(a, (fps,))
	SDTest.Poke(a + 4 - 0x400000, frames)
	SDTest.Poke(a + 8 - 0x400000, ciclo)
	rsbset(a + 0x14, (framef,))
	SDTest.Poke(a + 0x18 - 0x400000, modo)
	rsbset(a + 0x1c, fund)
	SDTest.Poke(a + 0x24 - 0x400000, rsbnow - ms)
	SDTest.Call(0x2581a0, 'tvi', a)
	return rsbam(a), rsbi(a + 0x24) == rsbnow
for rsbt in ((0, 40, 25.0, 10, -1, 0.0, (0.0, 0.0)), (0, 40, 25.0, 10, -1, 8.5, (0.0, 0.0)),
		(0, 40, 25.0, 10, 3, 9.5, (0.0, 0.0)), (0, 500, 25.0, 10, 0, 1.0, (0.0, 0.0)),
		(0, 0, 25.0, 10, -1, 9.99999, (0.0, 0.0)), (1, 40, 25.0, 10, -1, 4.0, (0.0, 0.0)),
		(2, 40, 25.0, 10, -1, 4.0, (2.0, 1.0)), (2, 100, 25.0, 10, -1, 4.0, (20.0, 0.5)),
		(3, 40, 25.0, 10, -1, 4.0, (0.0, 0.0)), (0, 33, 15.0, 0, -1, 0.0, (0.0, 0.0)),
		(0, 70, 7.5, 4, 2, 3.75, (0.0, 0.0)), (0, -5, 25.0, 10, -1, 1.0, (0.0, 0.0))):
	check('CAnimMat_Update' + fmt(rsbt), lambda: apply(rsbstep, (rsba,) + rsbt))
SDTest.Poke(rsba + 0x28 - 0x400000, SDTest.Peek(0x4a8ec8, 'i'))
check('CAnimMat_GetFrame same frame', lambda: (SDTest.Call(0x258060, 'tii', rsba)[0], rsbam(rsba)))
SDTest.Poke(rsba + 0x18 - 0x400000, 0)
SDTest.Poke(rsba + 0x24 - 0x400000, rsbnow - 40)
SDTest.Poke(rsba + 0x28 - 0x400000, -1)
check('CAnimMat_GetFrame', lambda: (SDTest.Call(0x258060, 'tii', rsba)[0], rsbam(rsba),
	rsbi(rsba + 0x28) == SDTest.Peek(0x4a8ec8, 'i')))
rsbfree(rsba)
rsbfree(rsbb)

# the CTabla resizes of tabla.h: n elements, the old ones kept
def rsbtab(t, size):
	n = rsbi(t)
	p = rsbi(t + 4)
	if not p:
		return n, 0
	r = []
	for rsbj in range(min(n, rsbtabset[0]) * size / 4):
		r.append(rsbi(p + rsbj * 4))
	return n, r
for rsbr in ((0x2616f0, 'Tabla_Resize2', 2), (0x261830, 'Tabla_ResizeC', 0xc), (0x261780, 'Tabla_Resize1C', 0x1c)):
	rsbt = rsbz(8)
	rsbtabset = [0]
	for rsbn in (4, 4, 6, 2, 0, 0, 3, -1):
		rsbrva, rsbname, rsbsize = rsbr
		check(rsbname + fmt((rsbn,)), lambda: (SDTest.Call(rsbrva, 'tvii', rsbt, rsbn), rsbtab(rsbt, rsbsize)))
		rsbp = rsbi(rsbt + 4)
		if rsbp:
			# the elements this code wrote (the new ones are not cleared)
			for rsbj in range(rsbtabset[0] * rsbsize / 4, rsbn * rsbsize / 4):
				SDTest.Poke(rsbp + rsbj * 4 - 0x400000, rsbj * 7 + 1)
			rsbtabset[0] = rsbn
		else:
			rsbtabset[0] = 0
	rsbfree(rsbt)

for rsbv in ((3.0, 4.0, 0.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (-2.0, 1e-20, 5.0), (1e20, 1e20, 1e20),
		(-0.0, 0.0, -0.0), (1e-30, 0.0, 0.0)):
	check('Vec3_Normalize' + fmt(rsbv), lambda: SDTest.Call(0x261580, 'iFF', (9.0, 9.0, 9.0), rsbv)[1:])
check('CListaVert_Optimize', lambda: SDTest.Call(0x243090, 'tbiii', 0, 0, 0)[0])
check('BspQ3_Render', lambda: (SDTest.Peek(0x4e5bf8, 'i'), SDTest.Call(0x2c9d90, 'v')))

# Cmd.c on the lexer of cmd_lex
def rsbcl():
	return (SDTest.Peek(0x4e733c, 'i'), SDTest.Peek(0x4e7338, 'p'), SDTest.Peek(0x4e7340, 'i'), SDTest.Out())
def rsbst(name, rva, sig, *args):
	check(name + fmt(args), lambda rva=rva, sig=sig, args=args: (apply(SDTest.Call, (rva, sig) + args), rsbcl()))
writefile('sdrsb.txt', '( 1 , 2.5 , -3 ) ( 4 , x ) true false maybe 7.5 ; 8 , 9 ) nomodel "nomodel2" 11 12\n'
	'AddAnim nomodel anim2 PlayAnim nomodel a 3 PivotPos nomodel n EFEC_MALLA modelo nomodel '
	'EFEC_MALLA pantalla nomodel FUNDIR_NODOS nomodel end\n')
writefile('sdrsb2.txt', '')
writefile('sdrsb3.txt', '/* only a comment */\n')
rsbv = rsbz(0x10)
def rsbvec():
	return rsbfl(rsbv, 3)
call('Lex_SetOut', 0x2cda80, 'vi', SDTest.OutFunc())
call('Lex_Open', 0x2cd5f0, 'isi', 'sdrsb.txt', 1)
SDTest.Poke(0x4e733c, 0)
rsbst('CmdLex_Next', 0x2ce270, 'v')
check('Cmd_GetVec', lambda: (SDTest.Call(0x280780, 'iiii', 0, rsbv, 0)[0], rsbvec(), rsbcl()))
check('Cmd_GetVec bad', lambda: (SDTest.Call(0x280780, 'iiii', 0, rsbv, 0)[0], rsbvec(), rsbcl()))
rsbst('Cmd_GetBool bad', 0x280970, 'bi', 9)
rsbst('Cmd_SkipToIdent', 0x280a80, 'v')
rsbst('Cmd_GetBool', 0x280970, 'bi', 5)
rsbst('Cmd_GetBool', 0x280970, 'bi', 5)
rsbst('Cmd_GetBool maybe', 0x280970, 'bi', 7)
check('Cmd_GetFloat ;', lambda: (SDTest.Call(0x280a00, 'iiii', 0, rsbv, ord(';'))[0], rsbvec(), rsbcl()))
check('Cmd_GetFloat ,', lambda: (SDTest.Call(0x280a00, 'iiii', 0, rsbv + 4, ord(','))[0], rsbvec(), rsbcl()))
check('Cmd_GetFloat 0', lambda: (SDTest.Call(0x280a00, 'iiii', 0, rsbv + 8, 0)[0], rsbvec(), rsbcl()))
check('Cmd_GetFloat next', lambda: (SDTest.Call(0x280a00, 'iiii', 1, rsbv, 0)[0], rsbvec(), rsbcl()))
check('Cmd_GetModelo', lambda: (SDTest.Call(0x280720, 'i')[0], rsbcl()))
check('Cmd_GetModelo string', lambda: (SDTest.Call(0x280720, 'i')[0], rsbcl()))
rsbst('Cmd_NextInt', 0x280ac0, 'i')
rsbst('CmdLex_Next', 0x2ce270, 'v')
rsbst('Cmd_AddAnim', 0x2803d0, 'v')
rsbst('Cmd_PlayAnim', 0x2805d0, 'v')
rsbst('CmdLex_Next', 0x2ce270, 'v')
rsbst('Cmd_PivotPos', 0x2805b0, 'v')
rsbst('Cmd_EfecMalla', 0x280630, 'v')
rsbst('Cmd_EfecMalla', 0x280630, 'v')
rsbst('Cmd_FundirNodos', 0x280700, 'v')
rsbst('Cmd_SkipToIdent ident', 0x280a80, 'v')
rsbst('CmdLex_Next end', 0x2ce270, 'v')
rsbst('Cmd_SkipToIdent end', 0x280a80, 'v')
call('Lex_Close', 0x2cd780, 'vi', 1)
check('Lex out', SDTest.Out)
call('Cmd_Out', 0x280af0, 'vs', 'rsb out text')
for rsbn in ('sdrsb2.txt', 'sdrsb3.txt', 'sdnofile.txt'):
	check('Cmd_ReadFile' + fmt((rsbn,)), lambda: (SDTest.Call(0x280b10, 'vs', rsbn), rsbcl(),
		SDTest.Peek(0x4e7334, 'i'), SDTest.Peek(0x4e6e24, 'i')))
call('Lex_SetOut', 0x2cda80, 'vi', SDTest.OutFunc())
rsbfree(rsbv)

# timers (only what does not depend on the clock), the capture without a surface or a file, the video player on
# missing files, the GDI line on the window
check('Timer_GetQPFreq', lambda: SDTest.Call(0x276d00, 'iD', 0.0)[0])
check('Timer_CountTicks 0', lambda: SDTest.Call(0x276ba0, 'ii', 0)[0])
check('Timer_MeasureQP 0', lambda: SDTest.Call(0x276e40, 'ii', 0)[0] >= 0)
check('Tsc_CpuFreq', lambda: SDTest.Call(0x272f90, 'fi', 0)[0] != 0.0)
check('Debug_Line', lambda: SDTest.Call(0x27cb80, 'viiiiii', 1, 2, 30, 40, 0x123456, 1))
check('Avi_Frame', lambda: (SDTest.Peek(0x4b29a0, 'i'), SDTest.Call(0x27f9f0, 'bsi', 'sdrsb.avi', 5)[0]))
check('Avi_Load', lambda: SDTest.Call(0x27f9b0, 'bs', 'sdrsb.avi')[0])
check('Avi_Save', lambda: SDTest.Call(0x27fa20, 'bs', 'sdrsb.avi')[0])
check('Avi_Write close', lambda: SDTest.Call(0x27fa70, 'viiiii', 0, 0, 0, 0, 0))
check('Surface_SaveShot NULL', lambda: SDTest.Call(0x27fe90, 'bi', 0)[0])
check('Surface_ShotCopy NULL', lambda: SDTest.Call(0x280310, 'bi', 0)[0])
check('Video_WaitKeysUp 0', lambda: SDTest.Call(0x280d10, 'vi', 0))
check('Video_PadPressed', lambda: SDTest.Call(0x280d60, 'b')[0])
for rsbn in ('sdnovideo.avi', 'sdnoa.avi|  sdnob.avi', 'Video/sdno.avi'):
	check('Video_PlayFile' + fmt((rsbn,)), lambda: SDTest.Call(0x281660, 'isz', rsbn, None)[0])
	check('Video_PlayPath' + fmt((rsbn,)), lambda: SDTest.Call(0x281740, 'iszi', rsbn, 'Bin/', 0)[0])

# the debug view of the collision meshes: the vertices of one triangle (wireframe and solid view), a mesh made here
# drawn in the modes of c_show (outside a scene the draws fail, the marks of the triangles are cleared)
def rsbcv(a):
	r = []
	for rsbj in range(3):
		f = rsbfl(a + rsbj * 0x18, 6)
		r.append((f[0:3], rsbi(a + rsbj * 0x18 + 0xc), f[4:6]))
	return r
rsbsolid = SDTest.Peek(0x4af850, 'i')
rsbshow = SDTest.Peek(0x4a8f70, 'i')
rsbo = rsbz(0x48)
for rsbs in (0, 1, 2):
	SDTest.Poke(0x4af850, rsbs)
	for rsbt in (((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 0.0, 10.0), 0x123456),
			((1.0, 2.0, 3.0), (4.0, 8.0, -3.0), (-5.0, 2.5, 7.0), 0xffffffff),
			((0.0, 0.0, 0.0), (0.0, 10.0, 0.0), (10.0, 0.0, 0.0), 0x80ff00),
			((0.0, 0.0, 0.0), (0.0, 0.0, 10.0), (0.0, 10.0, 0.0), 0),
			((1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), 0x10203),
			((100.0, -50.0, 3.0), (100.5, -49.0, 3.5), (99.0, -50.0, 2.0), 0x7f7f7f)):
		check('Collide_TriVerts' + fmt((rsbs,) + rsbt), lambda: (SDTest.Call(0x258fc0, 'viFFFi', rsbo,
			rsbt[0], rsbt[1], rsbt[2], rsbt[3]), rsbcv(rsbo)))
SDTest.Poke(0x4af850, rsbsolid)
rsbmesh = rsbz(0x34)
rsbvt = rsbz(12 * 6)
rsbset(rsbvt, (0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, 0.0, 10.0, 5.0, 5.0, 5.0, -3.0, 2.0, 1.0, 4.0, 4.0, -4.0))
rsbtri = rsbz(0x1c * 6)
rsbtris = ((0, 1, 2, 0x0000), (1, 2, 3, 0x1004), (2, 3, 4, 0x2000), (3, 4, 5, 0x2005), (0, 2, 4, 0x0001),
	(1, 3, 5, 0xf004))
def rsbmeshset(flags, sector):
	SDTest.Poke(rsbmesh + 4 - 0x400000, flags | sector << 16)
	SDTest.Poke(rsbmesh + 0x28 - 0x400000, rsbvt)
	SDTest.Poke(rsbmesh + 0x2c - 0x400000, len(rsbtris))
	SDTest.Poke(rsbmesh + 0x30 - 0x400000, rsbtri)
	for rsbj in range(len(rsbtris)):
		rsbta, rsbtb, rsbtc, rsbtf = rsbtris[rsbj]
		SDTest.Poke(rsbtri + rsbj * 0x1c + 0x14 - 0x400000, rsbta | rsbtb << 16)
		SDTest.Poke(rsbtri + rsbj * 0x1c + 0x18 - 0x400000, rsbtc | rsbtf << 16)
def rsbmeshflags():
	r = []
	for rsbj in range(len(rsbtris)):
		r.append(rsbi(rsbtri + rsbj * 0x1c + 0x18) >> 16)
	return r
for rsbmode, rsbs, rsbmf, rsbsec in ((0, 0, 0, 0), (1, 0, 0, 0), (3, 0, 0, 0), (4, 1, 1, 0), (5, 0, 0, 0),
		(5, 2, 0, 3), (5, 0, 0, 16)):
	rsbmeshset(rsbmf, rsbsec)
	SDTest.Poke(0x4a8f70, rsbmode)
	SDTest.Poke(0x4af850, rsbs)
	check('CCollideMesh_DrawDebug' + fmt((rsbmode, rsbs, rsbmf, rsbsec)), lambda: (SDTest.Call(0x25a030, 'vii',
		rsbmesh, 0), rsbmeshflags(), rsbcv(0x4af8b0 + 0x400000), rsbfl(0x432420 + 0x400000, 4)))
SDTest.Poke(0x4a8f70, rsbshow)
SDTest.Poke(0x4af850, rsbsolid)
rsbfree(rsbo)
rsbfree(rsbmesh)
rsbfree(rsbvt)
rsbfree(rsbtri)
