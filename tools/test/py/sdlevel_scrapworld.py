# game ScrapEng/ScrapEngWorld.c (execfile'd by sdlevel.py, Python 1.5.2): the boxes of the floor, the sector of the
# player, the world node info over the dummies and the nodes of the level with name patterns and the user properties
# of the dummies. WorldInfo_Next only with DM_ patterns: after the dummies a matching node is a CHECK. The walk state
# is put back to an empty pattern at the end.
def swz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def swfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
# a WorldNodeInfo: name, pos, matrix, dummy byte
def swinfo(a):
	return (SDTest.Peek(a, '@s'), swfl(a + 0x40, 3), swfl(a + 0x4c, 16), SDTest.Peek(a + 0x8c, '@i') & 0xff)
def swstate():
	return (SDTest.Peek('g_WorldInfoNode', 'i') != 0, SDTest.Peek('g_WorldInfoDummy', 'i') != 0, SDTest.Peek('g_WorldInfoWild', 'b'),
		SDTest.Peek('g_WorldInfoPattern', 's'))

check('World_GetBox', lambda: SDTest.Call('World_GetBox', 'iF', (0.0,) * 6)[1])
check('World_GetCollideBox', lambda: SDTest.Call('World_GetCollideBox', 'iF', (0.0,) * 6)[1])
swsec = SDTest.Call('Sector_At', 'iF', (pp[0], pp[1], pp[2]))[0]
check('Sector_At player', lambda: swsec)
if swsec >= 0:
	check('Sector_Visible', lambda: SDTest.Call('Sector_Visible', 'bi', swsec)[0])

swi = swz(0x90)
# the dummies one by one with their user properties, and the nodes, for each pattern (at most 40 of each)
swnodes = {}
for pat, wild in (('', 0), ('DM_*', 1), ('dm_*', 1), ('*spawn*', 1), ('DM_2dMap1', 0), ('dm_2dmap1', 0),
	('NoSuchName', 0), ('[a-d]*', 1), ('dm_nosuch*', 1)):
	check('WorldInfo_Begin' + fmt((pat, wild)), lambda pat=pat, wild=wild: (SDTest.Call('WorldInfo_Begin', 'vsi', pat, wild),
		swstate()))
	for k in range(40):
		r = SDTest.Call('WorldInfo_NextDummy', 'bi', swi)[0]
		hp = 0
		if r:
			hp = SDTest.Call('WorldInfo_HasUserProp', 'b')[0]
		check('WorldInfo_NextDummy' + fmt((pat, wild, k)), lambda r=r, hp=hp: (r, hp, swinfo(swi), swstate()))
		if not r:
			break
		if hp:
			check('WorldInfo user props' + fmt((pat, wild, k)), lambda: (SDTest.Call('WorldInfo_GetBool', 'bssi', 'info', 'loop',
				7)[0], SDTest.Call('WorldInfo_GetFloat', 'fssf', 'info', 'density', 1.5)[0], SDTest.Call('WorldInfo_GetFloat', 'fssf', 'info',
				'radius', -2.0)[0], SDTest.Call('WorldInfo_GetInt', 'issi', 'info', 'type', 12345)[0], SDTest.Call('WorldInfo_GetInt',
				'issi', 'nosect', 'nokey', -9)[0], SDTest.Call('WorldInfo_GetText', 'issbis', 'info', 'type', '', 64, 'def')[1],
				SDTest.Call('WorldInfo_GetText', 'issbis', 'nosect', 'x', '', 8, 'default text')[1], SDTest.Call('WorldInfo_GetVec3', 'iFssF',
				(0.0,) * 3, 'info', 'vel', (1.0, 2.0, 3.0))[1:]))
	SDTest.Call('WorldInfo_Begin', 'vsi', pat, wild)
	swnodes[pat] = 0
	for k in range(40):
		r = SDTest.Call('WorldInfo_NextNode', 'bi', swi)[0]
		check('WorldInfo_NextNode' + fmt((pat, wild, k)), lambda r=r: (r, swinfo(swi), swstate()))
		if not r:
			break
		swnodes[pat] = 1
# WorldInfo_Next only where no node matches (a matching node after the dummies is a CHECK)
for pat, wild in (('DM_*', 1), ('DM_2dMap1', 0), ('dm_nosuch*', 1)):
	if swnodes[pat]:
		continue
	SDTest.Call('WorldInfo_Begin', 'vsi', pat, wild)
	for k in range(40):
		r = SDTest.Call('WorldInfo_Next', 'bi', swi)[0]
		check('WorldInfo_Next' + fmt((pat, wild, k)), lambda r=r: (r, swinfo(swi), swstate()))
		if not r:
			break
# a dummy and a node into the info directly, NULLs do nothing
SDTest.Call('WorldInfo_Begin', 'vsi', '', 0)
if SDTest.Call('WorldInfo_NextDummy', 'bi', swi)[0]:
	swd = SDTest.Peek('g_WorldInfoDummy', 'i')
	check('WorldInfo_FromDummy', lambda swd=swd: (SDTest.Call('WorldInfo_FromDummy', 'vii', swi, swd), swinfo(swi)))
	check('WorldInfo_FromDummy NULL', lambda swd=swd: (SDTest.Call('WorldInfo_FromDummy', 'vii', 0, swd), SDTest.Call('WorldInfo_FromDummy', 'vii',
		swi, 0), swinfo(swi)))
if SDTest.Call('WorldInfo_NextNode', 'bi', swi)[0]:
	swn = SDTest.Peek('g_WorldInfoNode', 'i')
	check('WorldInfo_FromNode', lambda swn=swn: (SDTest.Call('WorldInfo_FromNode', 'vii', swi, swn), swinfo(swi)))
	check('WorldInfo_FromNode NULL', lambda swn=swn: (SDTest.Call('WorldInfo_FromNode', 'vii', 0, swn), SDTest.Call('WorldInfo_FromNode', 'vii',
		swi, 0), swinfo(swi)))
SDTest.Call('WorldInfo_Begin', 'vsi', '', 0)
check('WorldInfo state end', swstate)
