# engine3d Engine/ParticleSystem.c: particle systems on a node made here, from inis made here (Python 1.5.2, runs inside
# sdcall.py after the other checks: check, fmt come from there). Floats are printed as their bits. Not tested here:
# CParticleSystem_Update and the D3D drawing (they depend on the time and the device), the bounce (needs the world floor).
def spnew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def spi(addr):
	return SDTest.Peek(addr, '@i')
def spb(addr):
	return spi(addr) & 0xff
def spints(addr, n):
	r = []
	for i in range(n):
		r.append(spi(addr + 4 * i))
	return r
def spini(sect, keys):
	ini = SDTest.Call('CIniFile_ctor', 'tii', spnew(4))[0]
	for k, v in keys:
		SDTest.Call('CIniFile_SetStr', 'tiisss', ini, sect, k, v)
	return ini
# a particle: the members both kinds have, then those of an active one
def sppart(p, active):
	r = (spb(p + 4), spints(p + 8, 11))
	if active:
		ramp = spi(p + 0x88)
		r = r + (spb(p + 0x34), spints(p + 0x38, 3), spb(p + 0x44), spints(p + 0x48, 10), spints(p + 0x74, 2),
			spb(p + 0x7c), spb(p + 0x7d), spb(p + 0x7e), spints(p + 0x80, 2), ramp != 0, spb(p + 0x8c))
	return r
def spparts(sp, active):
	r = []
	ps = spi(sp + 0x11c)
	for i in range(spi(sp + 0xc)):
		r.append(sppart(spi(ps + 4 * i), active))
	return r
# the system: the parameters (without the texture string), the state
def spget(sp):
	return (spints(sp + 0xc, 0x32), spb(sp + 0xd4), spb(sp + 0xd5), spb(sp + 0xd6), spb(sp + 0xdc), spb(sp + 0xdd), spints(sp + 0xe0, 3),
		spb(sp + 0xec), spints(sp + 0xf0, 3), spi(sp + 0xfc) != 0, spb(sp + 0x10c), spb(sp + 0x10d),
		SDTest.Peek(sp + 0x110, '@p'), spi(sp + 0x118) != 0, spb(sp + 0x140), spb(sp + 0x141), spb(sp + 0x142),
		spb(sp + 0x14c), spb(sp + 0x14d))

check('Graphic2D_NumPrims', lambda: map(lambda a: SDTest.Call('Graphic2D_NumPrims', 'iii', a[0], a[1])[0], ((4, 3), (4, 12), (5, 3),
	(5, 10), (6, 3), (6, 7))))
spsaved = (SDTest.Peek('g_Screen2DW', 'i'), SDTest.Peek('g_Screen2DH', 'i'))
check('Graphic2D_SetScreen', lambda: (SDTest.Call('Graphic2D_SetScreen', 'vii', 640, 480), SDTest.Call('Graphic2D_GetScreen', 'vpp', 0, 0),
	SDTest.Peek('g_Screen2DInvW', 'd'), SDTest.Peek('g_Screen2DInvH', 'd')))
SDTest.Call('Graphic2D_SetScreen', 'vii', spsaved[0], spsaved[1])
SDTest.Call('crt_srand', 'vi', 23)
for a, b in (((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)), ((-1.0, 2.0, 3.0), (1.0, 2.0, 5.0)), ((5.0, -5.0, 0.5), (-5.0, 5.0, 0.25))):
	check('ParticleSystem_RandVec' + fmt((a, b)), lambda a=a, b=b: SDTest.Call('ParticleSystem_RandVec', 'iFFF', (9.0, 9.0, 9.0), a, b)[1])

node = SDTest.Call('CNode3D_ctor', 'tii', spnew(0xfc))[0]
SDTest.Call('String_Set', 'tvis', node + 4, 'sdpart')
# held by its scene, as the systems expect (they only add their own reference)
SDTest.Poke(node + 8 - 0x400000, 1)
SDTest.Poke(node + 0xf8 - 0x400000, spini('Particles', (('Type', 'sdpa'), ('Scale', '2.0'), ('Alpha', '0.5'))))
pini = spini('sdpa', (('Particles', '6'), ('BirthRate', '30'), ('LifeTime', '0.4'), ('Scale', '1.5'), ('SizeIni', '0.5'),
	('SizeEnd', '2'), ('SizeVar', '0.3'), ('SizeLife', '3'), ('ColorIni', '255 0 0 255'), ('ColorEnd', '0 0 255 0'),
	('InitPosMin', '-1 -1 -1'), ('InitPosMax', '1 2 1'), ('InitVelMin', '-2 4 -2'), ('InitVelMax', '2 8 2'),
	('InitAcc', '0 -9.8 0'), ('RotIni', '10'), ('RotEnd', '90'), ('RotVar', '15'), ('SpriteIni', '1'), ('SpriteEnd', '4'),
	('SpriteRand', '1'), ('Sparks', '1'), ('AdditiveAlpha', '0'), ('AlphaDist', '2'), ('Bounce', '1'), ('ZBias', '0.25')))
SDTest.Call('CIniFile_SetStr', 'tiisss', pini, 'sdpr', 'Particles', '4')
SDTest.Call('CIniFile_SetStr', 'tiisss', pini, 'sdpr', 'ColorRamp', '1')
for i in range(1, 6):
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, 'sdpr', 'ColorRamp_%02d' % i, '%d %d %d %d' % (50 * i, 255 - 40 * i, 20 * i, 200))
SDTest.Call('crt_srand', 'vi', 31)
sp = SDTest.Call('ParticleSystem_FromNode', 'iii', node, pini)[0]
check('ParticleSystem_FromNode', lambda: (sp != 0, spget(sp), spparts(sp, 1)))
check('CParticleSystem_GetScale', lambda: (SDTest.Call('CParticleSystem_GetScale', 'tviF', sp, (9.0, 9.0, 9.0))[1],
	SDTest.Call('CParticleSystem_GetScale2', 'tviF', sp, (9.0, 9.0))[1]))
check('CParticleSystem_SetAlpha', lambda: (SDTest.Call('CParticleSystem_SetAlpha', 'tvif', sp, 0.75), spi(sp + 0xf8)))
for n, force, dt in ((3, 0, 0.05), (2, 0, 0.0), (4, 0, 0.1), (3, 1, 0.02)):
	check('CParticleSystem_EmitDt' + fmt((n, force, dt)), lambda n=n, force=force, dt=dt: (SDTest.Call('CParticleSystem_EmitDt', 'tviiif', sp, n,
		force, dt), spget(sp)[10:12], spparts(sp, 1)))
# the life of the particles in fixed steps, without and with a multi sprite (only the sprite index then)
for step in range(8):
	ps = spi(sp + 0x11c)
	for i in range(spi(sp + 0xc)):
		p = spi(ps + 4 * i)
		if spb(p + 4):
			SDTest.Call('CParticleActive_Update', 'tviifi', p, sp, 0.06, 0)
	check('CParticleActive_Update' + fmt((step,)), lambda: (spget(sp)[10:12], spparts(sp, 1)))
emit = spnew(24)
SDTest.Call('crt_memcpy', 'iiFi', emit, (1.0, 2.0, 3.0, -4.0, -5.0, -6.0), 24)
check('CParticleSystem_SetEmitPos', lambda: (SDTest.Call('CParticleSystem_SetEmitPos', 'tviii', sp, emit, 2), spget(sp)[17:]))
check('CParticleSystem_Emit pos', lambda: (SDTest.Call('CParticleSystem_EmitDt', 'tviiif', sp, 3, 1, 0.0), spget(sp)[17:], spparts(sp, 1)))
check('CParticleSystem_SetEmitPos none', lambda: (SDTest.Call('CParticleSystem_SetEmitPos', 'tviii', sp, 0, 5), spget(sp)[17:]))
mat = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 10.0, 20.0, 30.0, 1.0)
check('CParticleSystem_EmitM', lambda: (SDTest.Call('CParticleSystem_EmitM', 'tviiiF', sp, 2, 1, mat)[0], spparts(sp, 1)))
p0 = spi(spi(sp + 0x11c))
check('CParticleSystem_SetActive', lambda: (SDTest.Call('CParticleSystem_SetActive', 'tviii', sp, p0, 0), spb(sp + 0x10c),
	SDTest.Call('CParticleSystem_SetActive', 'tviii', sp, p0, 0), spb(sp + 0x10c), SDTest.Call('CParticleSystem_SetActive', 'tviii', sp, p0, 1), spb(sp + 0x10c)))
for force in (0, 1):
	check('CParticleSystem_GetFree' + fmt((force,)), lambda force=force: SDTest.Call('CParticleSystem_GetFree', 'tiii', sp, force)[0] != 0)

# a color ramp system on the same node
SDTest.Call('CIniFile_SetStr', 'tiisss', spi(node + 0xf8), 'Particles', 'Type', 'sdpr')
SDTest.Call('crt_srand', 'vi', 41)
sr = SDTest.Call('ParticleSystem_FromNode', 'iii', node, pini)[0]
check('ParticleSystem_FromNode ramp', lambda: (spget(sr), spparts(sr, 1)))
SDTest.Call('CParticleSystem_EmitDt', 'tviiif', sr, 4, 0, 0.0)
for step in range(4):
	ps = spi(sr + 0x11c)
	for i in range(spi(sr + 0xc)):
		p = spi(ps + 4 * i)
		if spb(p + 4):
			SDTest.Call('CParticleActive_Update', 'tviifi', p, sr, 0.15, 0)
	check('CParticleActive_Update ramp' + fmt((step,)), lambda: spparts(sr, 1))

# static systems: local ones get a box, the others not
for name, local in (('sdst', '1'), ('sdsw', '0')):
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, name, 'Static', '1')
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, name, 'Local', local)
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, name, 'Particles', '5')
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, name, 'BirthRate', '5')
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, name, 'InitVel', '1 2 0.5')
	SDTest.Call('CIniFile_SetStr', 'tiisss', pini, name, 'SizeIni', '2')
	SDTest.Call('CIniFile_SetStr', 'tiisss', spi(node + 0xf8), 'Particles', 'Type', name)
	SDTest.Call('crt_srand', 'vi', 53)
	st = SDTest.Call('ParticleSystem_FromNode', 'iii', node, pini)[0]
	check('ParticleSystem_FromNode static' + fmt((local,)), lambda st=st: (spget(st), spparts(st, 0)))
	check('CParticleSystem_GetBox' + fmt((local,)), lambda st=st: SDTest.Call('CParticleSystem_GetBox', 'tbiF', st, (9.0, 9.0, 9.0, 9.0, 9.0, 9.0)))
	check('CParticleSystem_Delete static' + fmt((local,)), lambda st=st: SDTest.Call('CParticleSystem_Delete', 'tiii', st, 1)[0] == st)
check('CParticleSystem_GetBox not static', lambda: SDTest.Call('CParticleSystem_GetBox', 'tbiF', sp, (9.0, 9.0, 9.0, 9.0, 9.0, 9.0)))

# a copy, then everything deleted
SDTest.Call('crt_srand', 'vi', 61)
cp = SDTest.Call('CParticleSystem_ctorCopy', 'tiii', spnew(0x150), sp)[0]
check('CParticleSystem_ctorCopy', lambda: (spget(cp), spparts(cp, 1)))
cnt = SDTest.Peek('g_ParticleSystemCount', 'i')
for s in (cp, sr, sp):
	check('CParticleSystem_Delete', lambda s=s: (SDTest.Call('CParticleSystem_Delete', 'tiii', s, 1)[0] == s, SDTest.Peek('g_ParticleSystemCount', 'i') - cnt))
check('CParticleSystem node refs', lambda: spi(node + 8))
