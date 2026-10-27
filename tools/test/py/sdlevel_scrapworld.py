# game ScrapEng/ScrapEngWorld.c (execfile'd by sdlevel.py, Python 1.5.2): the boxes of the floor, the sector of the
# player, the world node info over the dummies and the nodes of the level with name patterns and the user properties
# of the dummies. WorldInfo_Next only with DM_ patterns: after the dummies a matching node is a CHECK. The walk state
# is put back to an empty pattern at the end.
def swz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def swfl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
# a WorldNodeInfo: name, pos, matrix, dummy byte
def swinfo(a):
	return (SDTest.Peek(a, '@s'), swfl(a + 0x40, 3), swfl(a + 0x4c, 16), SDTest.Peek(a + 0x8c, '@i') & 0xff)
def swstate():
	return (SDTest.Peek(0x4a81f4, 'i') != 0, SDTest.Peek(0x4a81f8, 'i') != 0, SDTest.Peek(0x4a81fc, 'b'),
		SDTest.Peek(0x4a8200, 's'))

check('World_GetBox', lambda: SDTest.Call(0x214380, 'iF', (0.0,) * 6)[1])
check('World_GetCollideBox', lambda: SDTest.Call(0x2143e0, 'iF', (0.0,) * 6)[1])
swsec = SDTest.Call(0x2146e0, 'iF', (pp[0], pp[1], pp[2]))[0]
check('Sector_At player', lambda: swsec)
if swsec >= 0:
	check('Sector_Visible', lambda: SDTest.Call(0x214450, 'bi', swsec)[0])

swi = swz(0x90)
# the dummies one by one with their user properties, and the nodes, for each pattern (at most 40 of each)
swnodes = {}
for pat, wild in (('', 0), ('DM_*', 1), ('dm_*', 1), ('*spawn*', 1), ('DM_2dMap1', 0), ('dm_2dmap1', 0),
	('NoSuchName', 0), ('[a-d]*', 1), ('dm_nosuch*', 1)):
	check('WorldInfo_Begin' + fmt((pat, wild)), lambda pat=pat, wild=wild: (SDTest.Call(0x214460, 'vsi', pat, wild),
		swstate()))
	for k in range(40):
		r = SDTest.Call(0x2147f0, 'bi', swi)[0]
		hp = 0
		if r:
			hp = SDTest.Call(0x214510, 'b')[0]
		check('WorldInfo_NextDummy' + fmt((pat, wild, k)), lambda r=r, hp=hp: (r, hp, swinfo(swi), swstate()))
		if not r:
			break
		if hp:
			check('WorldInfo user props' + fmt((pat, wild, k)), lambda: (SDTest.Call(0x214550, 'bssi', 'info', 'loop',
				7)[0], SDTest.Call(0x2145a0, 'fssf', 'info', 'density', 1.5)[0], SDTest.Call(0x2145a0, 'fssf', 'info',
				'radius', -2.0)[0], SDTest.Call(0x2145f0, 'issi', 'info', 'type', 12345)[0], SDTest.Call(0x2145f0,
				'issi', 'nosect', 'nokey', -9)[0], SDTest.Call(0x214640, 'issbis', 'info', 'type', '', 64, 'def')[1],
				SDTest.Call(0x214640, 'issbis', 'nosect', 'x', '', 8, 'default text')[1], SDTest.Call(0x214690, 'iFssF',
				(0.0,) * 3, 'info', 'vel', (1.0, 2.0, 3.0))[1:]))
	SDTest.Call(0x214460, 'vsi', pat, wild)
	swnodes[pat] = 0
	for k in range(40):
		r = SDTest.Call(0x2148f0, 'bi', swi)[0]
		check('WorldInfo_NextNode' + fmt((pat, wild, k)), lambda r=r: (r, swinfo(swi), swstate()))
		if not r:
			break
		swnodes[pat] = 1
# WorldInfo_Next only where no node matches (a matching node after the dummies is a CHECK)
for pat, wild in (('DM_*', 1), ('DM_2dMap1', 0), ('dm_nosuch*', 1)):
	if swnodes[pat]:
		continue
	SDTest.Call(0x214460, 'vsi', pat, wild)
	for k in range(40):
		r = SDTest.Call(0x2144c0, 'bi', swi)[0]
		check('WorldInfo_Next' + fmt((pat, wild, k)), lambda r=r: (r, swinfo(swi), swstate()))
		if not r:
			break
# a dummy and a node into the info directly, NULLs do nothing
SDTest.Call(0x214460, 'vsi', '', 0)
if SDTest.Call(0x2147f0, 'bi', swi)[0]:
	swd = SDTest.Peek(0x4a81f8, 'i')
	check('WorldInfo_FromDummy', lambda swd=swd: (SDTest.Call(0x214200, 'vii', swi, swd), swinfo(swi)))
	check('WorldInfo_FromDummy NULL', lambda swd=swd: (SDTest.Call(0x214200, 'vii', 0, swd), SDTest.Call(0x214200, 'vii',
		swi, 0), swinfo(swi)))
if SDTest.Call(0x2148f0, 'bi', swi)[0]:
	swn = SDTest.Peek(0x4a81f4, 'i')
	check('WorldInfo_FromNode', lambda swn=swn: (SDTest.Call(0x2142d0, 'vii', swi, swn), swinfo(swi)))
	check('WorldInfo_FromNode NULL', lambda swn=swn: (SDTest.Call(0x2142d0, 'vii', 0, swn), SDTest.Call(0x2142d0, 'vii',
		swi, 0), swinfo(swi)))
SDTest.Call(0x214460, 'vsi', '', 0)
check('WorldInfo state end', swstate)
