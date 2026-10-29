# Rest B (game leftovers from 0x240000, execfile'd by sdcall.py, Python 1.5.2): the texture animation counter
# (ControlAnimMat.c) on calloc memory, the CTable resizes, Vec3_Normalize, the stub CVertList_Optimize, the
# parsers of the command file (Cmd.c) on a file written here and Cmd_ReadFile on an empty one (an error in
# Cmd_ReadFile is fatal through Err_Printf, a model load reads files: not called).
def rsbz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def rsbi(a):
	return SDTest.Peek(a, '@i')
def rsbfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def rsbset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def rsbfree(a):
	SDTest.Call('Mem_Free', 'vi', a)

# CAnimMat: fps, frames, ciclo, dt, frame, framef, modo, fadeRate, fadeK (lastTime and lastFrame are the clock)
def rsbam(a):
	f = rsbfl(a, 9)
	return (f[0], rsbi(a + 4), rsbi(a + 8), f[3], rsbi(a + 0x10), f[5], rsbi(a + 0x18), f[7], f[8])
rsbnow = SDTest.Peek('g_TimeMs', 'i')
rsba = rsbz(0x2c)
rsbb = rsbz(0x2c)
check('CAnimMat_ctor', lambda: (SDTest.Call('CAnimMat_ctor', 'tii', rsba)[0] == rsba, rsbam(rsba), rsbi(rsba + 0x28)))
check('AnimMat_Set', lambda: (SDTest.Call('AnimMat_Set', 'bifi', rsba, 12.5, 2)[0], rsbam(rsba)))
check('AnimMat_Set NULL', lambda: SDTest.Call('AnimMat_Set', 'bifi', 0, 12.5, 2)[0])
SDTest.Poke(rsba + 4 - 0x400000, 6)
check('CAnimMat_ctorCopy', lambda: (SDTest.Call('CAnimMat_ctorCopy', 'tiii', rsbb, rsba)[0] == rsbb, rsbam(rsbb), rsbi(rsbb + 0x28)))
check('CAnimMat_Init', lambda: (SDTest.Call('CAnimMat_Init', 'tvi', rsbb), rsbam(rsbb)))
check('CAnimMat_dtor', lambda: (SDTest.Call('CAnimMat_dtor', 'tvi', rsbb), rsbam(rsbb)))
for rsbk in (0, 1, 2, 3):
	check('AnimMat_ModeName' + fmt((rsbk,)), lambda: SDTest.Call('AnimMat_ModeName', 'si', rsbk))
for rsbv in (0.0, 2.75, -1.5, 5.999):
	check('CAnimMat_SetFrameF' + fmt((rsbv,)), lambda: (SDTest.Call('CAnimMat_SetFrameF', 'tvif', rsba, rsbv), rsbam(rsba)))
check('CAnimMat_SetFrame', lambda: (SDTest.Call('CAnimMat_SetFrame', 'tvii', rsba, 3), rsbam(rsba)))
check('CAnimMat_Start', lambda: (SDTest.Call('CAnimMat_Start', 'tvi', rsba), rsbam(rsba), rsbi(rsba + 0x24) == rsbnow))
check('CAnimMat_Stop', lambda: (SDTest.Call('CAnimMat_Stop', 'tvi', rsba), rsbam(rsba)))
check('CAnimMat_Pause', lambda: (SDTest.Call('CAnimMat_Pause', 'tvi', rsba), rsbam(rsba)))
check('CAnimMat_FadePause not 2', lambda: (SDTest.Call('CAnimMat_FadePause', 'tvif', rsba, 500.0), rsbam(rsba)))
SDTest.Poke(rsba + 0x18 - 0x400000, 2)
check('CAnimMat_FadePause', lambda: (SDTest.Call('CAnimMat_FadePause', 'tvif', rsba, 500.0), rsbam(rsba)))
check('CAnimMat_FadePause 0', lambda: (SDTest.Call('CAnimMat_FadePause', 'tvif', rsba, 0.0), rsbam(rsba)))
check('CAnimMat_Play', lambda: (SDTest.Call('CAnimMat_Play', 'tvi', rsba), rsbam(rsba)))
# steps of ms milliseconds: lastTime set back from the clock (constant while the script runs)
def rsbstep(a, modo, ms, fps, frames, ciclo, framef, fund):
	rsbset(a, (fps,))
	SDTest.Poke(a + 4 - 0x400000, frames)
	SDTest.Poke(a + 8 - 0x400000, ciclo)
	rsbset(a + 0x14, (framef,))
	SDTest.Poke(a + 0x18 - 0x400000, modo)
	rsbset(a + 0x1c, fund)
	SDTest.Poke(a + 0x24 - 0x400000, rsbnow - ms)
	SDTest.Call('CAnimMat_Update', 'tvi', a)
	return rsbam(a), rsbi(a + 0x24) == rsbnow
for rsbt in ((0, 40, 25.0, 10, -1, 0.0, (0.0, 0.0)), (0, 40, 25.0, 10, -1, 8.5, (0.0, 0.0)),
		(0, 40, 25.0, 10, 3, 9.5, (0.0, 0.0)), (0, 500, 25.0, 10, 0, 1.0, (0.0, 0.0)),
		(0, 0, 25.0, 10, -1, 9.99999, (0.0, 0.0)), (1, 40, 25.0, 10, -1, 4.0, (0.0, 0.0)),
		(2, 40, 25.0, 10, -1, 4.0, (2.0, 1.0)), (2, 100, 25.0, 10, -1, 4.0, (20.0, 0.5)),
		(3, 40, 25.0, 10, -1, 4.0, (0.0, 0.0)), (0, 33, 15.0, 0, -1, 0.0, (0.0, 0.0)),
		(0, 70, 7.5, 4, 2, 3.75, (0.0, 0.0)), (0, -5, 25.0, 10, -1, 1.0, (0.0, 0.0))):
	check('CAnimMat_Update' + fmt(rsbt), lambda: apply(rsbstep, (rsba,) + rsbt))
SDTest.Poke(rsba + 0x28 - 0x400000, SDTest.Peek('g_FrameCount', 'i'))
check('CAnimMat_GetFrame same frame', lambda: (SDTest.Call('CAnimMat_GetFrame', 'tii', rsba)[0], rsbam(rsba)))
SDTest.Poke(rsba + 0x18 - 0x400000, 0)
SDTest.Poke(rsba + 0x24 - 0x400000, rsbnow - 40)
SDTest.Poke(rsba + 0x28 - 0x400000, -1)
check('CAnimMat_GetFrame', lambda: (SDTest.Call('CAnimMat_GetFrame', 'tii', rsba)[0], rsbam(rsba),
	rsbi(rsba + 0x28) == SDTest.Peek('g_FrameCount', 'i')))
rsbfree(rsba)
rsbfree(rsbb)

# the CTable resizes of table.h: n elements, the old ones kept
def rsbtab(t, size):
	n = rsbi(t)
	p = rsbi(t + 4)
	if not p:
		return n, 0
	r = []
	for rsbj in range(min(n, rsbtabset[0]) * size / 4):
		r.append(rsbi(p + rsbj * 4))
	return n, r
for rsbr in ((sym('Table_Resize2'), 'Table_Resize2', 2), (sym('Table_ResizeC'), 'Table_ResizeC', 0xc), (sym('Table_Resize1C'), 'Table_Resize1C', 0x1c)):
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
	check('Vec3_Normalize' + fmt(rsbv), lambda: SDTest.Call('Vec3_Normalize', 'iFF', (9.0, 9.0, 9.0), rsbv)[1:])
check('CVertList_Optimize', lambda: SDTest.Call('CVertList_Optimize', 'tbiii', 0, 0, 0)[0])
check('BspQ3_Render', lambda: (SDTest.Peek('g_BspQ3', 'i'), SDTest.Call('BspQ3_Render', 'v')))

# Cmd.c on the lexer of cmd_lex
def rsbcl():
	return (SDTest.Peek(sym('g_Cl') + 0x4, 'i'), SDTest.Peek('g_Cl', 'p'), SDTest.Peek(sym('g_Cl') + 0x8, 'i'), SDTest.Out())
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
check('Lex_SetOut(out)', lambda: SDTest.Call('Lex_SetOut', 'vi', SDTest.OutFunc()))
call('Lex_Open', sym('Lex_Open'), 'isi', 'sdrsb.txt', 1)
SDTest.Poke(sym('g_Cl') + 0x4, 0)
rsbst('CmdLex_Next', sym('CmdLex_Next'), 'v')
check('Cmd_GetVec', lambda: (SDTest.Call('Cmd_GetVec', 'iiii', 0, rsbv, 0)[0], rsbvec(), rsbcl()))
check('Cmd_GetVec bad', lambda: (SDTest.Call('Cmd_GetVec', 'iiii', 0, rsbv, 0)[0], rsbvec(), rsbcl()))
rsbst('Cmd_GetBool bad', sym('Cmd_GetBool'), 'bi', 9)
rsbst('Cmd_SkipToIdent', sym('Cmd_SkipToIdent'), 'v')
rsbst('Cmd_GetBool', sym('Cmd_GetBool'), 'bi', 5)
rsbst('Cmd_GetBool', sym('Cmd_GetBool'), 'bi', 5)
rsbst('Cmd_GetBool maybe', sym('Cmd_GetBool'), 'bi', 7)
check('Cmd_GetFloat ;', lambda: (SDTest.Call('Cmd_GetFloat', 'iiii', 0, rsbv, ord(';'))[0], rsbvec(), rsbcl()))
check('Cmd_GetFloat ,', lambda: (SDTest.Call('Cmd_GetFloat', 'iiii', 0, rsbv + 4, ord(','))[0], rsbvec(), rsbcl()))
check('Cmd_GetFloat 0', lambda: (SDTest.Call('Cmd_GetFloat', 'iiii', 0, rsbv + 8, 0)[0], rsbvec(), rsbcl()))
check('Cmd_GetFloat next', lambda: (SDTest.Call('Cmd_GetFloat', 'iiii', 1, rsbv, 0)[0], rsbvec(), rsbcl()))
check('Cmd_GetModel', lambda: (SDTest.Call('Cmd_GetModel', 'i')[0], rsbcl()))
check('Cmd_GetModel string', lambda: (SDTest.Call('Cmd_GetModel', 'i')[0], rsbcl()))
rsbst('Cmd_NextInt', sym('Cmd_NextInt'), 'i')
rsbst('CmdLex_Next', sym('CmdLex_Next'), 'v')
rsbst('Cmd_AddAnim', sym('Cmd_AddAnim'), 'v')
rsbst('Cmd_PlayAnim', sym('Cmd_PlayAnim'), 'v')
rsbst('CmdLex_Next', sym('CmdLex_Next'), 'v')
rsbst('Cmd_PivotPos', sym('Cmd_PivotPos'), 'v')
rsbst('Cmd_MeshEffect', sym('Cmd_MeshEffect'), 'v')
rsbst('Cmd_MeshEffect', sym('Cmd_MeshEffect'), 'v')
rsbst('Cmd_MergeNodes', sym('Cmd_MergeNodes'), 'v')
rsbst('Cmd_SkipToIdent ident', sym('Cmd_SkipToIdent'), 'v')
rsbst('CmdLex_Next end', sym('CmdLex_Next'), 'v')
rsbst('Cmd_SkipToIdent end', sym('Cmd_SkipToIdent'), 'v')
call('Lex_Close', sym('Lex_Close'), 'vi', 1)
check('Lex out', SDTest.Out)
call('Cmd_Out', sym('Cmd_Out'), 'vs', 'rsb out text')
for rsbn in ('sdrsb2.txt', 'sdrsb3.txt', 'sdnofile.txt'):
	check('Cmd_ReadFile' + fmt((rsbn,)), lambda: (SDTest.Call('Cmd_ReadFile', 'vs', rsbn), rsbcl(),
		isfn(SDTest.Peek('g_LexOut', 'i'), sym('Cmd_Out')), SDTest.Peek('g_LexErrors', 'i')))
check('Lex_SetOut(out)', lambda: SDTest.Call('Lex_SetOut', 'vi', SDTest.OutFunc()))
rsbfree(rsbv)

# the capture without a surface or a file, the video player on
# missing files, the GDI line on the window
check('Debug_Line', lambda: SDTest.Call('Debug_Line', 'viiiiii', 1, 2, 30, 40, 0x123456, 1))
check('Avi_Frame', lambda: (SDTest.Peek('g_AviFrames', 'i'), SDTest.Call('Avi_Frame', 'bsi', 'sdrsb.avi', 5)[0]))
check('Avi_Load', lambda: SDTest.Call('Avi_Load', 'bs', 'sdrsb.avi')[0])
check('Avi_Save', lambda: SDTest.Call('Avi_Save', 'bs', 'sdrsb.avi')[0])
check('Avi_Write close', lambda: SDTest.Call('Avi_Write', 'viiiii', 0, 0, 0, 0, 0))
check('Surface_SaveShot NULL', lambda: SDTest.Call('Surface_SaveShot', 'bi', 0)[0])
check('Surface_ShotCopy NULL', lambda: SDTest.Call('Surface_ShotCopy', 'bi', 0)[0])
check('Video_PadPressed', lambda: SDTest.Call('Video_PadPressed', 'b')[0])
for rsbn in ('sdnovideo.avi', 'sdnoa.avi|  sdnob.avi', 'Video/sdno.avi'):
	check('Video_PlayFile' + fmt((rsbn,)), lambda: SDTest.Call('Video_PlayFile', 'isz', rsbn, None)[0])
	check('Video_PlayPath' + fmt((rsbn,)), lambda: SDTest.Call('Video_PlayPath', 'iszi', rsbn, 'Bin/', 0)[0])

# the debug view of the collision meshes: the vertices of one triangle (wireframe and solid view), a mesh made here
# drawn in the modes of c_show (outside a scene the draws fail, the marks of the triangles are cleared)
def rsbcv(a):
	r = []
	for rsbj in range(3):
		f = rsbfl(a + rsbj * 0x18, 6)
		r.append((f[0:3], rsbi(a + rsbj * 0x18 + 0xc), f[4:6]))
	return r
rsbsolid = SDTest.Peek('g_ShowMapSolid', 'i')
rsbshow = SDTest.Peek('g_ShowCollision', 'i')
rsbo = rsbz(0x48)
for rsbs in (0, 1, 2):
	SDTest.Poke('g_ShowMapSolid', rsbs)
	for rsbt in (((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 0.0, 10.0), 0x123456),
			((1.0, 2.0, 3.0), (4.0, 8.0, -3.0), (-5.0, 2.5, 7.0), 0xffffffff),
			((0.0, 0.0, 0.0), (0.0, 10.0, 0.0), (10.0, 0.0, 0.0), 0x80ff00),
			((0.0, 0.0, 0.0), (0.0, 0.0, 10.0), (0.0, 10.0, 0.0), 0),
			((1.0, 1.0, 1.0), (1.0, 1.0, 1.0), (1.0, 1.0, 1.0), 0x10203),
			((100.0, -50.0, 3.0), (100.5, -49.0, 3.5), (99.0, -50.0, 2.0), 0x7f7f7f)):
		check('Collide_TriVerts' + fmt((rsbs,) + rsbt), lambda: (SDTest.Call('Collide_TriVerts', 'viFFFi', rsbo,
			rsbt[0], rsbt[1], rsbt[2], rsbt[3]), rsbcv(rsbo)))
SDTest.Poke('g_ShowMapSolid', rsbsolid)
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
	SDTest.Poke('g_ShowCollision', rsbmode)
	SDTest.Poke('g_ShowMapSolid', rsbs)
	check('CCollideMesh_DrawDebug' + fmt((rsbmode, rsbs, rsbmf, rsbsec)), lambda: (SDTest.Call('CCollideMesh_DrawDebug', 'vii',
		rsbmesh, 0), rsbmeshflags(), rsbcv(sym('g_CollideVerts') + 0x400000), rsbfl(sym('g_CollideAddColor') + 0x400000, 4)))
SDTest.Poke('g_ShowCollision', rsbshow)
SDTest.Poke('g_ShowMapSolid', rsbsolid)
rsbfree(rsbo)
rsbfree(rsbmesh)
rsbfree(rsbvt)
rsbfree(rsbtri)
