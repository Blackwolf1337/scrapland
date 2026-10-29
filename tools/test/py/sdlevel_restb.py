# Rest B (game leftovers from 0x240000, execfile'd by sdlevel.py, Python 1.5.2): the visibility test of the models
# of the render list in pass 1 (their last world matrix), and what the level frames drew with the C versions of
# CModel_Render and the LOD choice (the flags they leave on the models).
def rsbli(a):
	return SDTest.Peek(a, '@i')
def rsblb(a):
	return SDTest.Peek(a, '@i') & 0xff
rsblpass = SDTest.Peek('g_RenderPass', 'i')
rsbln = SDTest.Peek('g_NumModelsRender', 'i')
check('models in the render list', lambda: rsbln > 0)
SDTest.Poke('g_RenderPass', 1)
rsblk = 0
while rsblk < rsbln and rsblk < 60:
	rsblm = rsbli(SDTest.Peek(sym('g_ModelsRender') + rsblk * 4, 'i'))
	if rsblm:
		check('CModel_Visibility' + fmt((rsblk, SDTest.Peek(rsbli(rsblm + 4), '@s'))), lambda: (
			SDTest.Call('CModel_Visibility', 'tbii', rsblm, rsblm + 0xb0)[0], rsblb(rsblm + 0x120), rsblb(rsblm + 0x121),
			rsbli(rsblm + 0x100)))
	rsblk = rsblk + 1
SDTest.Poke('g_RenderPass', rsblpass)
def rsblfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
SDTest.Poke('g_RenderPass', 1)
rsblk = 0
while rsblk < rsbln and rsblk < 12:
	rsblm = rsbli(SDTest.Peek(sym('g_ModelsRender') + rsblk * 4, 'i'))
	if rsblm:
		check('CModel_Render' + fmt((rsblk,)), lambda: (SDTest.Call('CModel_Render', 'tvi', rsblm), rsblb(rsblm + 0x121),
			rsblfl(rsblm + 0xb0, 16)))
		check('CModel_UpdateLOD' + fmt((rsblk,)), lambda: SDTest.Call('CModel_UpdateLOD', 'tbi', rsblm)[0])
	rsblk = rsblk + 1
SDTest.Poke('g_RenderPass', rsblpass)

# Stencil_Mesh on the device meshes of the first models that have their shadow data: a light set here, the
# distance multiplier raised so the meshes count as near, the counters of the volumes cleared and put back after
def rsblmeshes():
	r = []
	k = 0
	while k < rsbln and len(r) < 3:
		m = rsbli(SDTest.Peek(sym('g_ModelsRender') + k * 4, 'i'))
		k = k + 1
		if not m or not rsbli(m + 0xac):
			continue
		t = rsbli(m + 0xac)
		n = rsbli(t)
		p = rsbli(t + 4)
		j = 0
		while j < n and len(r) < 3:
			nodo = rsbli(p + j * 0xa8 + 0xa4)
			j = j + 1
			if not nodo:
				continue
			o = rsbli(nodo + 0x3c)
			if not o or rsbli(o + 4) != -1593835518:
				continue
			if rsbli(o + 0x3c) and rsbli(o + 0x40) and rsbli(o + 0x44) and rsbli(o + 0xb4) and not (rsbli(rsbli(o + 0xb4) + 0x18) & 0x20):
				r.append(o)
	return r
rsblvars = (sym('g_ShadowsStencil'), sym('g_StencilCapture'), sym('g_ShadowLight'), sym('g_StencilMultFrame'), sym('g_StencilSegs'), sym('g_StencilInv'), sym('g_StencilGroups'), sym('g_StencilGroupsMax'), sym('g_ShadowLightType'), sym('g_ShadowRange'),
	sym('g_ShadowCos'), sym('g_ShadowLightPos'), sym('g_ShadowLightPos') + 0x4, sym('g_ShadowLightPos') + 0x8, sym('g_ShadowLightDir'), sym('g_ShadowLightDir') + 0x4, sym('g_ShadowLightDir') + 0x8, sym('g_ShowStencil'))
rsblsave = map(lambda a: SDTest.Peek(a, 'i'), rsblvars)
rsblms = rsblmeshes()
check('stencil meshes', lambda: (len(rsblms), SDTest.Peek('g_StencilBlock', 'i') != 0, SDTest.Peek('g_StencilSegments', 'i') != 0,
	SDTest.Peek('g_StencilTriInv', 'i') != 0))
def rsblsetf(rva, v):
	SDTest.Call('crt_memcpy', 'iiFi', rva + 0x400000, (v,), 4)
def rsblblock(i):
	b = SDTest.Peek('g_StencilBlock', 'i') + i * 0x80
	return (rsbli(b) != 0, rsblfl(b + 4, 4), rsblb(b + 0x18), rsblb(b + 0x19), rsblfl(b + 0x1c, 6),
		rsblfl(b + 0x34, 16), rsbli(b + 0x74), rsbli(b + 0x7c))
def rsblstate():
	segs = SDTest.Peek('g_StencilSegs', 'i')
	tris = SDTest.Peek('g_StencilInv', 'i')
	sp = SDTest.Peek('g_StencilSegments', 'i')
	tp = SDTest.Peek('g_StencilTriInv', 'i')
	f = []
	for j in range(min(48, 0x10000)):
		f.append(SDTest.Peek(sym('g_TriFacing') + j, 'b'))
	r = (segs, tris, SDTest.Peek('g_StencilGroups', 'i'), SDTest.Peek('g_StencilGroupsMax', 'i'), f)
	if segs:
		r = r + (rsblfl(sp, 6 * min(segs, 4)), rsblfl(sp + (segs - 1) * 0x18, 6))
	if tris:
		r = r + (rsblfl(tp, 9 * min(tris, 3)), rsblfl(tp + (tris - 1) * 0x24, 9))
	return r
if len(rsblms) and SDTest.Peek('g_StencilBlock', 'i') and SDTest.Peek('g_StencilSegments', 'i') and SDTest.Peek('g_StencilTriInv', 'i'):
	rsblluz = SDTest.Call('Mem_Calloc', 'ii', 0xcc)[0]
	for rsblcase in ((0, 1, 1, 0x1388), (1, 0, 1, 0x1388), (1, 1, 0, 0x1388), (1, 1, 1, 0x1388), (1, 1, 1, 0x138a),
			(1, 1, 1, 0x1389)):
		SDTest.Poke('g_StencilSegs', 0)
		SDTest.Poke('g_StencilInv', 0)
		SDTest.Poke('g_StencilGroups', 0)
		SDTest.Poke('g_StencilGroupsMax', 0)
		SDTest.Poke('g_ShadowsStencil', rsblcase[0])
		SDTest.Poke('g_StencilCapture', rsblcase[1])
		SDTest.Poke('g_ShadowLight', rsblcase[2] and rsblluz)
		SDTest.Poke('g_ShadowLightType', rsblcase[3])
		rsblsetf(sym('g_StencilMultFrame'), 1e10)
		rsblsetf(sym('g_ShadowRange'), 3000.0)
		rsblsetf(sym('g_ShadowCos'), 0.5)
		SDTest.Call('crt_memcpy', 'iiFi', sym('g_ShadowLightPos') + 0x400000, (pp[0] + 50.0, pp[1] + 900.0, pp[2] - 30.0), 12)
		SDTest.Call('crt_memcpy', 'iiFi', sym('g_ShadowLightDir') + 0x400000, (0.0, -1.0, 0.0), 12)
		for rsblm in rsblms:
			check('Stencil_Mesh' + fmt(rsblcase), lambda: (SDTest.Call('Stencil_Mesh', 'viiiii', 0, 0, 0, rsblm, 0),
				rsblstate()))
		if SDTest.Peek('g_StencilGroups', 'i'):
			check('Stencil_Mesh block', lambda: rsblblock(0))
	SDTest.Call('Mem_Free', 'vi', rsblluz)
for rsblk in range(len(rsblvars)):
	SDTest.Poke(rsblvars[rsblk], rsblsave[rsblk])

# CGround_DrawDebug with the device calls logged: the quad boxes of the floor near the camera (ShowMapQDist), the
# quad analysis, and the mesh view on an empty floor made here (the floor of the level has too many triangles)
def rsbllog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
rsbldvars = (sym('g_ShowCollision'), sym('g_ShowMapSolid'), sym('g_ShowMapQuads'), sym('g_ShowMapQDist'), sym('g_AnalyzeQuads'))
rsbldsave = map(lambda a: SDTest.Peek(a, 'i'), rsbldvars)
rsblsuelo = rsbli(SDTest.Peek('g_World', 'i') + 0xb0)
check('ground', lambda: rsblsuelo != 0)
# a 2 x 2 grid of quads (one cell with a second quad) next to the camera on a floor made here
rsblcam = rsblfl(rsbli(SDTest.Peek('g_World', 'i') + 0xa4) + 0x14, 3)
rsblgrid = SDTest.Call('Mem_Calloc', 'ii', 0x3524)[0]
rsblq = SDTest.Call('Mem_Calloc', 'ii', 0x24 * 5)[0]
SDTest.Poke(rsblgrid + 0x3500 - 0x400000, 4)
SDTest.Poke(rsblgrid + 0x3504 - 0x400000, rsblq)
SDTest.Poke(rsblgrid + 0x3508 - 0x400000, 2)
SDTest.Poke(rsblgrid + 0x350c - 0x400000, 2)
SDTest.Call('crt_memcpy', 'iiFi', rsblgrid + 0x3510, (100.0, 80.0), 8)
SDTest.Call('crt_memcpy', 'iiFi', rsblgrid + 0x34dc, (rsblcam[0] - 100.0, rsblcam[1] - 50.0, rsblcam[2] - 30.0), 12)
for rsblk in range(5):
	SDTest.Call('crt_memcpy', 'iiFi', rsblq + rsblk * 0x24, (rsblcam[1] + 10.0 * rsblk, rsblcam[1] - 5.0 * rsblk), 8)
	SDTest.Call('crt_memcpy', 'iiFi', rsblq + rsblk * 0x24 + 0x14, (rsblcam[1] + 20.0 + rsblk, rsblcam[1] - 20.0), 8)
	SDTest.Poke(rsblq + rsblk * 0x24 + 0xc - 0x400000, (0x50, 0x150, 0x3ff, 0x400, 0x777)[rsblk])
SDTest.Poke(rsblq + 0x20 - 0x400000, rsblq + 4 * 0x24)
for rsblqd in (0.0, 120.0):
	for rsblqm in (1, 2, 3, 4, 5, 6, 7):
		SDTest.Poke('g_ShowCollision', 0)
		SDTest.Poke('g_ShowMapQuads', rsblqm)
		rsblsetf(sym('g_ShowMapQDist'), rsblqd)
		check('CGround_DrawDebug quads' + fmt((rsblqm, rsblqd)), lambda: rsbllog(lambda: SDTest.Call('CGround_DrawDebug', 'tvi',
			rsblgrid)))
SDTest.Poke('g_ShowMapQuads', 0)
SDTest.Poke('g_AnalyzeQuads', 1)
for rsblk in range(5):
	SDTest.Poke(rsblq + rsblk * 0x24 + 0xc - 0x400000, 0)
check('CGround_DrawDebug analize grid', lambda: rsbllog(lambda: (SDTest.Call('CGround_DrawDebug', 'tvi', rsblgrid),
	SDTest.Peek('g_AnalyzeQuads', 'i'))))
SDTest.Call('Mem_Free', 'vi', rsblq)
SDTest.Call('Mem_Free', 'vi', rsblgrid)
SDTest.Poke('g_ShowMapQuads', 0)
SDTest.Poke('g_AnalyzeQuads', 1)
check('CGround_DrawDebug analize', lambda: rsbllog(lambda: (SDTest.Call('CGround_DrawDebug', 'tvi', rsblsuelo),
	SDTest.Peek('g_AnalyzeQuads', 'i'))))
rsblfake = SDTest.Call('Mem_Calloc', 'ii', 0x3524)[0]
for rsblshow in (0, 1, 6, 3):
	SDTest.Poke('g_ShowCollision', rsblshow)
	SDTest.Poke('g_ShowMapSolid', 0)
	check('CGround_DrawDebug empty' + fmt((rsblshow,)), lambda: rsbllog(lambda: (SDTest.Call('CGround_DrawDebug', 'tvi',
		rsblfake), SDTest.Peek('g_ShowMapSolid', 'i'))))
SDTest.Call('Mem_Free', 'vi', rsblfake)
for rsblk in range(len(rsbldvars)):
	SDTest.Poke(rsbldvars[rsblk], rsbldsave[rsblk])
