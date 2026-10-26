# game ScrapEng/ScrapEngModel.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): CGameModel objects in
# heap memory loaded with two characters of the packs (the .sm3 caches and their .lod1), their animations from the
# .cm3 caches, the getters, the anim channels and the render nodes. The models stay loaded (Reset only drops the
# reference). Not called: the collision functions (they need the floor of a world), RenderNow (the render lists),
# the vertex functions (R_GetModelVertex is off in the test setup, they return the position), SetAnimName
# (CModelo_SetAnimName is a CHECK(0)).
def gmz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def gmi(a):
	return SDTest.Peek(a, '@i')
def gmf(a):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,), a, 4)[1][0]
def gmv(a):
	return SDTest.Call(0x3197d0, 'iFii', (0.0, 0.0, 0.0), a, 12)[1]
def gmset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
gmbuf = gmz(0x100)
gmvec = gmz(0x40)
def gmstate(gm):
	return (SDTest.Call(0x210da0, 'tbi', gm)[0], SDTest.Call(0x20f7a0, 'tsi', gm)[0],
		SDTest.Call(0x20f730, 'tsi', gm)[0], SDTest.Call(0x210f30, 'tbi', gm)[0], SDTest.Call(0x20f5d0, 'tfi', gm)[0],
		gmi(gm + 0x10) & 0xff, gmf(gm + 0x14), gmi(gm + 0x18))
gmnames = ('Models/Vehicles/Characters/P_Dtritus/P_Dtritus.M3D',
	'Models/Vehicles/Characters/P_CrazyGambler/P_CrazyGambler.M3D')
gms = []
for name in gmnames:
	gm = gmz(0x1c)
	check('CGameModel_ctor' + fmt((name,)), lambda gm=gm, name=name: (SDTest.Call(0x20eb10, 'tiis', gm, name)[0] == gm,
		gmstate(gm)))
	gms.append(gm)
gm = gms[0]
check('CGameModel_Disable', lambda gm=gm: (SDTest.Call(0x20f3f0, 'tvi', gm), SDTest.Call(0x210f30, 'tbi', gm)[0]))
check('CGameModel_Enable', lambda gm=gm: (SDTest.Call(0x20f420, 'tvi', gm), SDTest.Call(0x210f30, 'tbi', gm)[0]))
check('CGameModel_IsVisible', lambda gm=gm: SDTest.Call(0x210f80, 'tbi', gm)[0])
check('CGameModel_CollideOn', lambda gm=gm: SDTest.Call(0x210f60, 'tbi', gm)[0])
for v in ((1.0, 2.0, 3.0), (-50.5, 0.25, 1e5)):
	gmset(gmvec, v)
	check('CGameModel_SetPos' + fmt(v), lambda gm=gm: (SDTest.Call(0x211d20, 'tvii', gm, gmvec),
		SDTest.Call(0x20fa30, 'tiii', gm, gmvec + 0x10)[0] == gmvec + 0x10, gmv(gmvec + 0x10)))
	check('CGameModel_SetRot' + fmt(v), lambda gm=gm: (SDTest.Call(0x211e50, 'tvii', gm, gmvec),
		SDTest.Call(0x210830, 'tiii', gm, gmvec + 0x10)[0] == gmvec + 0x10, gmv(gmvec + 0x10)))
	check('CGameModel_SetRot2' + fmt(v), lambda gm=gm: (SDTest.Call(0x211cd0, 'tvii', gm, gmvec),
		SDTest.Call(0x210830, 'tiii', gm, gmvec + 0x10)[0] == gmvec + 0x10, gmv(gmvec + 0x10)))
for d in (0.0, 1.5, -10.0):
	check('CGameModel_MoveForward' + fmt((d,)), lambda gm=gm, d=d: (SDTest.Call(0x20ed10, 'tvif', gm, d),
		SDTest.Call(0x20fa30, 'tiii', gm, gmvec)[0] == gmvec, gmv(gmvec)))
for fx in (0, 1, 2):
	check('CGameModel_SetEffect' + fmt((fx,)), lambda gm=gm, fx=fx: (SDTest.Call(0x211d70, 'tvii', gm, fx),
		SDTest.Call(0x210760, 'tiii', gm, gmvec)[0], gmi(gmvec)))
for i in (0,):
	check('CGameModel_SetEffectParam' + fmt((i,)), lambda gm=gm, i=i: (SDTest.Call(0x211de0, 'tviii', gm, i, 7 + i),
		SDTest.Call(0x2107c0, 'tviii', gm, i, gmvec), gmi(gmvec)))
check('CGameModel_SetSortZ', lambda gm=gm: SDTest.Call(0x211a40, 'tviff', gm, 2.5, 0.5))

# animations from the .cm3 caches next to the model
for k in range(len(gms)):
	gm = gms[k]
	gmdir = gmnames[k][:strop.rfind(gmnames[k], '/') + 1]
	for an in ('Relax.M3D', 'Go.M3D', 'Left.M3D'):
		check('CGameModel_AddAnim' + fmt((gmdir + an,)), lambda gm=gm, an=gmdir + an: SDTest.Call(0x20ec70, 'tiis', gm,
			an)[0])
	for an in ('Relax', 'Go', 'go', 'Left.M3D', 'none', ''):
		check('CGameModel_FindAnim' + fmt((an,)), lambda gm=gm, an=an: SDTest.Call(0x20efe0, 'tiis', gm, an)[0])
	for i in range(4):
		check('CGameModel_AnimName' + fmt((i,)), lambda gm=gm, i=i: SDTest.Call(0x20f0b0, 'tsii', gm, i)[0])
	for i in range(3):
		check('CGameModel_AnimInfo' + fmt((i,)), lambda gm=gm, i=i: (SDTest.Call(0x20f020, 'tbiiii', gm, i, gmvec,
			gmvec + 4)[0], gmf(gmvec), gmf(gmvec + 4)))
		for fps in (0.0, 15.0, 30.0):
			SDTest.Call(0x3197d0, 'iiFi', gm + 0x14, (fps,), 4)
			for fr in (0.0, 1.0, 2.5):
				check('CGameModel_AnimTime' + fmt((i, fps, fr)), lambda gm=gm, i=i, fr=fr: (SDTest.Call(0x20ee70,
					'tbiifi', gm, i, fr, gmvec)[0], gmf(gmvec)))
		SDTest.Call(0x3197d0, 'iiFi', gm + 0x14, (0.0,), 4)
		check('CGameModel_SetAnimVel' + fmt((i,)), lambda gm=gm, i=i: SDTest.Call(0x20f0f0, 'tbiif', gm, i, 12.0)[0])
		check('CGameModel_SetAnimSpeed' + fmt((i,)), lambda gm=gm, i=i: SDTest.Call(0x20f160, 'tbiif', gm, i, 1.5)[0])
		check('CGameModel_SetAnimFactor' + fmt((i,)), lambda gm=gm, i=i: SDTest.Call(0x20f250, 'tbiif', gm, i, 0.5)[0])
		check('CGameModel_SetAnimLoop' + fmt((i,)), lambda gm=gm, i=i: SDTest.Call(0x20f1e0, 'tbiii', gm, i, i & 1)[0])
		check('CGameModel_AnimInfo after' + fmt((i,)), lambda gm=gm, i=i: (SDTest.Call(0x20f020, 'tbiiii', gm, i,
			gmvec, gmvec + 4)[0], gmf(gmvec), gmf(gmvec + 4)))
	for chan, i in ((0, 0), (1, 2), (2, 1)):
		check('CGameModel_SetAnim' + fmt((chan, i)), lambda gm=gm, chan=chan, i=i: (SDTest.Call(0x210fc0, 'tviiii',
			gm, i, chan, 0), SDTest.Call(0x20f490, 'tiii', gm, chan)[0] != 0,
			SDTest.Call(0x20f450, 'tbii', gm, chan)[0], SDTest.Call(0x20f500, 'tbiiii', gm, chan, gmvec, gmvec + 4)[0],
			gmi(gmvec), gmi(gmvec + 4), SDTest.Call(0x20f570, 'tbiii', gm, chan, gmvec + 8)[0], gmf(gmvec + 8)))
	for chan in (0, 1):
		for t in (0.0, 0.1, 0.5):
			check('CGameModel_AnimDelta' + fmt((chan, t)), lambda gm=gm, chan=chan, t=t: (SDTest.Call(0x211250,
				'tbiifi', gm, chan, t, gmvec)[0], gmv(gmvec)))
	check('CGameModel_StopAnim', lambda gm=gm: (SDTest.Call(0x2124d0, 'tvii', gm, 1),
		SDTest.Call(0x20f490, 'tiii', gm, 1)[0], SDTest.Call(0x20f490, 'tiii', gm, 2)[0] != 0))
	check('CGameModel_StopAnims', lambda gm=gm: (SDTest.Call(0x212490, 'tvi', gm),
		SDTest.Call(0x20f490, 'tiii', gm, 0)[0] != 0, SDTest.Call(0x20f490, 'tiii', gm, 2)[0] != 0))
	# with animations playing, the update fires the events of the animations, which CHECK an entity
	SDTest.Poke(gmvec + 0x20 - 0x400000, 5)
	check('CGameModel_RootMotion', lambda gm=gm: (SDTest.Call(0x210db0, 'tvifii', gm, 0.05, gmvec, gmvec + 0x20),
		gmv(gmvec), gmi(gmvec + 0x20)))
	for cmd in ('Morph,1', 'bad'):
		check('CGameModel_AnimCmd' + fmt((cmd,)), lambda gm=gm, cmd=cmd: SDTest.Call(0x20edb0, 'tvis', gm, cmd))

# render nodes
	for nm in ('Bip01', 'Bip01 Head', 'Pivot', 'none', '*Bip*', 'Bip01*', '*'):
		check('CGameModel_FindNodoR' + fmt((nm,)), lambda gm=gm, nm=nm: (SDTest.Call(0x20f7d0, 'tiis', gm, nm)[0],
			SDTest.Call(0x20f820, 'tiis', gm, nm)[0]))
	for i in (0, 1, 5):
		# the reference is dropped again (CNodoR.Refs 0), a reload CHECKs it
		gmr = SDTest.Call(0x20fad0, 'tiii', gm, i)[0]
		check('CGameModel_NodoRRef' + fmt((i,)), lambda gmr=gmr: (gmi(gmr), SDTest.Peek(gmi(gmr + 0xa4) + 4, '@p')))
		SDTest.Poke(gmr - 0x400000, gmi(gmr) - 1)
		# animated 1 is a CHECK
		check('CGameModel_NodoPos' + fmt((i,)), lambda gm=gm, i=i: (SDTest.Call(0x20f870, 'tiiiii', gm, gmvec, i,
			0)[0] == gmvec, gmv(gmvec)))
		for an in (0, 1):
			check('CGameModel_NodoMatrix' + fmt((i, an)), lambda gm=gm, i=i, an=an: (SDTest.Call(0x211060,
				'tiiiiiii', gm, gmbuf, i, an, gmvec, 0)[0] == gmbuf, SDTest.Call(0x3197d0, 'iFii', (0.0,) * 16,
				gmbuf, 64)[1], gmi(gmvec) & 0xff))
		# mode 0 is a CHECK in CNodoR_SetLook
		for mode in (1, 2, 3, 4, 5, 6):
			gmset(gmvec, (0.5, -0.25, 1.0))
			check('CGameModel_NodoLook' + fmt((i, mode)), lambda gm=gm, i=i, mode=mode: SDTest.Call(0x211a90,
				'tbiiii', gm, i, mode, gmvec)[0])
		# CNodoR.nodo 0xa4, CNodo3D.ini 0xf8: a node without an ini is a NULL read
		gmini = gmi(gmi(gmr + 0xa4) + 0xf8)
		check('CGameModel_NodoIni' + fmt((i, gmini != 0)), lambda gm=gm, i=i, gmini=gmini: gmini and (
			SDTest.Call(0x20f660, 'tfiissf', gm, i, 'General', 'Scale', 2.5)[0],
			SDTest.Call(0x20f6d0, 'tiiissi', gm, i, 'General', 'Num', -3)[0],
			SDTest.Call(0x20fa70, 'tiiissi', gm, i, 'General', 'Color', 0x11223344)[0]) or None)
	for nm in ('Bip01', 'none'):
		check('CGameModel_SetPivotNodo' + fmt((nm,)), lambda gm=gm, nm=nm: (SDTest.Call(0x211c90, 'tvis', gm, nm),
			SDTest.Call(0x20fa30, 'tiii', gm, gmvec)[0] == gmvec, gmv(gmvec)))
	for rel in (0, 1):
		gmset(gmvec + 0x20, (0.0, 1.0, -2.0))
		check('CGameModel_SetPivot' + fmt((rel,)), lambda gm=gm, rel=rel: (SDTest.Call(0x211c30, 'tviiii', gm,
			gmvec + 0x20, rel, 0), SDTest.Call(0x20fa30, 'tiii', gm, gmvec)[0] == gmvec, gmv(gmvec)))
	for tx, fr in (('CabinaDetritus-A_D', 1), ('none', 0)):
		check('CGameModel_SetTexFrame' + fmt((tx, fr)), lambda gm=gm, tx=tx, fr=fr: SDTest.Call(0x211470, 'tbisi',
			gm, tx, fr)[0])
	check('CGameModel_VertexCounts', lambda gm=gm: (SDTest.Call(0x20f910, 'tiiii', gm, gmbuf, 64)[0],
		map(lambda k: gmi(gmbuf + 4 * k), range(16))))

# load and reset
gm = gms[1]
gmset(gmvec, (2.0, 2.0, 2.0))
check('CGameModel_LoadScaled', lambda gm=gm: (SDTest.Call(0x211a20, 'tvisi', gm, gmnames[0], gmvec), gmstate(gm)))
check('CGameModel_LoadName', lambda gm=gm: (SDTest.Call(0x2119f0, 'tvis', gm, gmnames[1]), gmstate(gm)))
check('CGameModel_Reset', lambda gm=gm: (SDTest.Call(0x20ec10, 'tvi', gm), gmi(gm)))
gm = gmz(0x1c)
check('CGameModel_ctor none', lambda gm=gm: (SDTest.Call(0x20eb10, 'tiii', gm, 0)[0] == gm,
	map(lambda k, gm=gm: gmi(gm + 4 * k),
	range(7)), SDTest.Call(0x210da0, 'tbi', gm)[0]))
