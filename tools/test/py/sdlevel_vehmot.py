# game Entities/Vehicle/VehicleMotors.c, VehicleMotorsEffects.c, VehicleTraffic.c and the small cWeaponManager
# functions before rva 0x11a0f0 (execfile'd by sdlevel.py, Python 1.5.2). First on objects of our own in zeroed
# memory made by their constructors: the effects (colors, types, setters, the early returns), the motors without
# owner, a motor without models, the traffic lane (variables, the collision slide of the moves near the player)
# and the weapon manager. Then on a car of our own far away (Scrap.CreateEntity, found in the entity table): its
# motors (names, slots, exhausts, the script variables), a motor reloaded with its own file, the effects cleared
# and made again, the glow, smoke and Calima particles on the particles of its embedded effects, and
# cVehicleMotors_Update with a fixed frame time and the embedded effect values set by hand (restored). rand is
# seeded before every call that uses it. Pointers only as != 0 or ==.
# Not called: the Render functions with models (they draw now), StartAnimF/M that load (the animation files are not
# known), Stop with a dead owner (a death trail debris), Move with an AI in traffic (the AIs of the level move).
def vmoi(a):
	return SDTest.Peek(a, '@i')
def vmob(a):
	return SDTest.Peek(a, '@i') & 0xff
def vmoz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def vmofl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def vmoset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def vmopoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def vmosetbyte(a, v):
	w = vmoi(a & ~3)
	sh = 8 * (a & 3)
	vmopoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))
def vmostr(s):
	a = vmoz(len(s) + 4)
	SDTest.Call('String_Copy', 'iisi', a, s, len(s) + 1)
	return a
def vmosrand(s):
	SDTest.Call('crt_srand', 'vi', s)
def vmoints(a, n):
	r = []
	for vmok in range(n):
		r.append(vmoi(a + 4 * vmok))
	return tuple(r)
def vmonz(a, n):
	r = []
	for vmok in range(n):
		r.append(vmoi(a + 4 * vmok) != 0)
	return tuple(r)
def vmostrat(p):
	if not p:
		return None
	return SDTest.Peek(p, '@s')
# a cVehicleMotorsEffects: the owners as != 0, then every field
def vmoeff(e):
	return (vmonz(e, 3), vmob(e + 0xc), vmoi(e + 0x10), vmoints(e + 0x14, 4), vmonz(e + 0x24, 4), vmob(e + 0x34),
		vmonz(e + 0x38, 2), vmofl(e + 0x40, 10), vmob(e + 0x68), plain(vmoints(e + 0x6c, 2)), vmofl(e + 0x74, 2),
		vmoi(e + 0x7c), vmofl(e + 0x80, 6), vmoints(e + 0x98, 2), vmob(e + 0xa0), vmofl(e + 0xa4, 1))
# a cVehicleMotor: models loaded, nodes, name, animation fields, effects
def vmomotor(m):
	return (vmonz(m, 1), vmonz(m + 0x1c, 1), vmoints(m + 0x38, 2), SDTest.Peek(m + 0x40, '@s'), vmob(m + 0x60),
		vmofl(m + 0x64, 2), vmob(m + 0x6c), vmofl(m + 0x70, 1), vmoeff(m + 0x74))
# a cVehicleMotors: vtable, owner, the motors as != 0, flags, speed, effects
def vmomotors(s):
	return (vmoi(s) - 0x400000, vmonz(s + 4, 5), vmoi(s + 0x18) & 0xffff, vmofl(s + 0x1c, 1), vmoeff(s + 0x20))
# the parameters of a CGameParticles without its pointers
def vmopart(p):
	return (vmoints(p, 0x26), vmoi(p + 0x98) & 0xffff, vmoints(p + 0x9c, 2), vmob(p + 0xa4))
def vmoparticle(p, i):
	r = SDTest.Call('CGameParticles_GetParticle', 'tbiiFiipF', p, i, (9.0, 9.0, 9.0), 0, 0, 6, (5.0,))
	if r[0]:
		return r
	return 0
# a CGameTrail holder: the CTrail flags and counters and the points written
def vmotrail(h):
	t = vmoi(h)
	if not t:
		return None
	c = vmoi(t + 0x20)
	mx = c & 0xff
	num = (c >> 8) & 0xff
	head = (c >> 16) & 0xff
	r = (vmoints(t + 0xc, 5), c, vmoi(t + 0x24) & 0xffff, vmoints(t + 0x2c, 4), vmob(t + 0x3c), vmoints(t + 0x4c, 2))
	pts = vmoi(t + 0x28)
	if pts and mx:
		for vmok in range(num):
			i = (head - 1 - vmok) % mx
			r = r + (vmoints(pts + 0x14 * i, 5),)
	return r

vmoworld = SDTest.Peek('g_pWorld', 'i')

# ---- Rand_FloatRange ----
for vmolo, vmohi in ((0.0, 1.0), (-5.0, 5.0), (100.0, 25.0), (3.0, 3.0)):
	vmosrand(777)
	check('Rand_FloatRange' + fmt((vmolo, vmohi)), lambda lo=vmolo, hi=vmohi: (SDTest.Call('Rand_FloatRange', 'fff', lo, hi)[0],
		SDTest.Call('Rand_FloatRange', 'fff', lo, hi)[0]))

# ---- the effects in zeroed memory ----
vmoe = vmoz(0xa8)
check('cVehicleMotorsEffects_ctor', lambda: (SDTest.Call('cVehicleMotorsEffects_ctor', 'tii', vmoe)[0] == vmoe, vmoeff(vmoe)))
for vmoc in (0, 0xffffff, 0x123456, 0x80ff01, -0x1000000, 0x7f7f7f):
	check('cVehicleMotorsEffects_SetColor' + fmt((vmoc,)), lambda c=vmoc: (SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', vmoe, c),
		plain(vmoints(vmoe + 0x6c, 2))))
for vmot in (0, 1, 9, 10, 11, 12, 13, 14, 15, 16, 20, 21, 25, 26, 27, 30, 35, 36, 40, 45, 50, 55, 60, 61, -3):
	SDTest.Call('cVehicleMotorsEffects_SetColor', 'tvii', vmoe, 0x010203)
	check('cVehicleMotorsEffects_SetType' + fmt((vmot,)), lambda t=vmot: (SDTest.Call('cVehicleMotorsEffects_SetType', 'tvii', vmoe, t),
		vmoi(vmoe + 0x10), plain(vmoints(vmoe + 0x6c, 2))))
check('cVehicleMotorsEffects_SetOffset', lambda: (SDTest.Call('cVehicleMotorsEffects_SetOffset', 'tvif', vmoe, -12.5), vmofl(vmoe + 0x74, 2)))
check('cVehicleMotorsEffects_SetScale', lambda: (SDTest.Call('cVehicleMotorsEffects_SetScale', 'tvif', vmoe, 2.25), vmofl(vmoe + 0x74, 2)))
vmofakem = vmoz(0x11c)
vmopoke(vmofakem + 0x38, -1)
vmopoke(vmofakem + 0x3c, 7)
check('cVehicleMotorsEffects_Set', lambda: (SDTest.Call('cVehicleMotorsEffects_Set', 'tviiii', vmoe, 0, 0, vmofakem), vmoeff(vmoe)))
check('cVehicleMotorsEffects_GetModel motor', lambda: (SDTest.Call('cVehicleMotorsEffects_GetModel', 'tiii', vmoe, 0)[0],
	SDTest.Call('cVehicleMotorsEffects_GetModel', 'tiii', vmoe, 1)[0] == vmofakem + 0x1c))
SDTest.Call('cVehicleMotorsEffects_Set', 'tviiii', vmoe, 0, 0, 0)
check('cVehicleMotorsEffects_GetModel none', lambda: SDTest.Call('cVehicleMotorsEffects_GetModel', 'tiii', vmoe, 1)[0])
# the early returns: StopGlow (off, no type, no exhausts), Glow without type or exhausts, HideTrails, SetVisible
for vmoon, vmot in ((0, 1), (1, 0), (1, 1)):
	vmosetbyte(vmoe + 0xa0, vmoon)
	vmopoke(vmoe + 0x10, vmot)
	check('cVehicleMotorsEffects_StopGlow' + fmt((vmoon, vmot)), lambda: (SDTest.Call('cVehicleMotorsEffects_StopGlow', 'tvipi', vmoe, 6, 0),
		vmob(vmoe + 0xa0)))
	check('cVehicleMotorsEffects_StopGlow NULL' + fmt((vmoon, vmot)), lambda: (SDTest.Call('cVehicleMotorsEffects_StopGlow', 'tviii', vmoe, 0,
		0), vmob(vmoe + 0xa0)))
for vmot in (0, 1):
	vmopoke(vmoe + 0x10, vmot)
	vmosetbyte(vmoe + 0xa0, 0)
	check('cVehicleMotorsEffects_Glow' + fmt((vmot,)), lambda: (SDTest.Call('cVehicleMotorsEffects_Glow', 'tvifffpi', vmoe, 0.5, 1.0,
		1000.0, 4, 0), vmob(vmoe + 0xa0)))
for vmoon in (0, 1):
	vmosetbyte(vmoe + 0x68, vmoon)
	check('cVehicleMotorsEffects_HideTrails' + fmt((vmoon,)), lambda: (SDTest.Call('cVehicleMotorsEffects_HideTrails', 'tvi', vmoe),
		vmob(vmoe + 0x68)))
for vmoon in (1, 1, 0):
	check('cVehicleMotorsEffects_SetVisible' + fmt((vmoon,)), lambda on=vmoon: (SDTest.Call('cVehicleMotorsEffects_SetVisible', 'tvii', vmoe,
		on), vmob(vmoe + 0x34)))
vmosetbyte(vmoe + 0x34, 1)
vmosetbyte(vmoe + 0x68, 1)
vmosetbyte(vmoe + 0xa0, 1)
check('cVehicleMotorsEffects_Stop', lambda: (SDTest.Call('cVehicleMotorsEffects_Stop', 'tvi', vmoe), vmoeff(vmoe)))
SDTest.Call('cVehicleMotorsEffects_Set', 'tviiii', vmoe, 1, 2, 3)
vmopoke(vmoe + 0x7c, 2)
check('cVehicleMotorsEffects_Clear', lambda: (SDTest.Call('cVehicleMotorsEffects_Clear', 'tvi', vmoe), vmoeff(vmoe)))
check('cVehicleMotorsEffects_dtor', lambda: (SDTest.Call('cVehicleMotorsEffects_dtor', 'tvi', vmoe), vmoeff(vmoe)))
vmov = vmoz(0x10)
check('Vec3_Assign', lambda: (SDTest.Call('Vec3_Assign', 'tiiF', vmov, (1.5, -2.0, 3.25))[0] == vmov, vmofl(vmov, 4)))

# ---- the motors without owner ----
vmos = vmoz(0xc8)
check('cVehicleMotors_ctor', lambda: (SDTest.Call('cVehicleMotors_ctor', 'tii', vmos)[0] == vmos, vmomotors(vmos)))
check('cVehicleMotors_GetOwner', lambda: SDTest.Call('cVehicleMotors_GetOwner', 'tii', vmos)[0])
for vmon in ('ShipMotorScale', 'ShipMotorOffset', 'ShipMotorScaleX', 'ShipMotorOffsetX', 'ShipMotorScal', 'Bogus'):
	check('cVehicleMotors_VarF get ' + vmon, lambda n=vmon: SDTest.Call('cVehicleMotors_VarF', 'tbisDi', vmos, n, 9.0, 0))
	check('cVehicleMotors_VarF set ' + vmon, lambda n=vmon: (SDTest.Call('cVehicleMotors_VarF', 'tbisDi', vmos, n, 0.375, 1),
		vmofl(vmos + 0x94, 2)))
for vmon, vmov2 in (('ShipMotorColorR', 0x1ab), ('ShipMotorColorG', 0x33), ('ShipMotorColorB', 0x7f), ('FxClass', 13),
		('FxClass', 99), ('EngFxClass0', 10), ('EngFxClass3', 10), ('EngFxClass4', 10), ('ShipMotorColorX', 1),
		('NEng0', 0), ('nEng9', 0), ('Bogus', 0)):
	check('cVehicleMotors_VarI set ' + vmon, lambda n=vmon, v=vmov2: (SDTest.Call('cVehicleMotors_VarI', 'tbispi', vmos, n, v, 1),
		vmoi(vmos + 0x30), vmoints(vmos + 0x8c, 2)))
for vmon in ('NEng0', 'NEng3', 'NEng4', 'NEng/', 'NEng', 'FxClass', 'ShipMotorColorR'):
	check('cVehicleMotors_VarI get ' + vmon, lambda n=vmon: SDTest.Call('cVehicleMotors_VarI', 'tbispi', vmos, n, 55, 0))
for vmon in ('MEng0', 'MEng3', 'MEng4', 'EngAnmM0', 'Bogus'):
	check('cVehicleMotors_VarS get ' + vmon, lambda n=vmon: (lambda r: (r[0], vmostrat(r[1])))(
		SDTest.Call('cVehicleMotors_VarS', 'tbispi', vmos, n, 0, 0)))
vmoanim = vmostr('SDVmoAnim')
for vmon in ('EngAnmM0', 'EngAnmF1', 'EngAnmX0', 'EngAnmM9', 'EngAnmMM'):
	check('cVehicleMotors_VarS set ' + vmon, lambda n=vmon: SDTest.Call('cVehicleMotors_VarS', 'tbispi', vmos, n, vmoanim, 1)[0])
for vmon in ('PEng0A', 'AEng1B', 'PEng4A', 'PEng0C', 'XEng0A', 'PEngSlot0Q', 'PEng0'):
	check('cVehicleMotors_VarV ' + vmon, lambda n=vmon: SDTest.Call('cVehicleMotors_VarV', 'tbisDDDi', vmos, n, 1.0, 2.0, 3.0, 0))
check('cVehicleMotors_VarV set', lambda: SDTest.Call('cVehicleMotors_VarV', 'tbisDDDi', vmos, 'PEng0A', 1.0, 2.0, 3.0, 1))
for vmom in (0, 3, 4, 100):
	check('cVehicleMotors_GetMotorName' + fmt((vmom,)), lambda m=vmom: (lambda r: (vmostrat(r[0]), r[1]))(
		SDTest.Call('cVehicleMotors_GetMotorName', 'tiiip', vmos, m, 77)))
check('cVehicleMotors_GetMotorName NULL', lambda: vmostrat(SDTest.Call('cVehicleMotors_GetMotorName', 'tiiii', vmos, 2, 0)[0]))
check('cVehicleMotors_GetExhaust none', lambda: SDTest.Call('cVehicleMotors_GetExhaust', 'tbiiiFF', vmos, 0, 0, (1.0, 2.0, 3.0),
	(4.0, 5.0, 6.0)))
check('cVehicleMotors_GetExhaust bad', lambda: (SDTest.Call('cVehicleMotors_GetExhaust', 'tbiiiFF', vmos, 4, 0, (1.0, 2.0, 3.0),
	(4.0, 5.0, 6.0)), SDTest.Call('cVehicleMotors_GetSlot', 'tbiiiFF', vmos, 0, 2, (1.0, 2.0, 3.0), (4.0, 5.0, 6.0))))
for vmoon in (1, 0):
	vmoset(vmos + 0x1c, (5.0,))
	check('cVehicleMotors_SetEnabled' + fmt((vmoon,)), lambda on=vmoon: (SDTest.Call('cVehicleMotors_SetEnabled', 'tvii', vmos, on),
		vmomotors(vmos)))
check('cVehicleMotors_SetEffect', lambda: (SDTest.Call('cVehicleMotors_SetEffect', 'tvii', vmos, 3), vmomotors(vmos)))
check('cVehicleMotors_SetEffectParam', lambda: (SDTest.Call('cVehicleMotors_SetEffectParam', 'tviif', vmos, 0, 0.5), vmomotors(vmos)))
check('cVehicleMotors_StartAnim none', lambda: (SDTest.Call('cVehicleMotors_StartAnimF', 'tviis', vmos, 1, 'x'),
	SDTest.Call('cVehicleMotors_StartAnimM', 'tviis', vmos, 7, 'x'), vmomotors(vmos)))
check('cVehicleMotors_Render none', lambda: (SDTest.Call('cVehicleMotors_Render', 'tvi', vmos), vmomotors(vmos)))
vmosetbyte(vmos + 0x18, 1)
vmosetbyte(vmos + 0x19, 1)
check('cVehicleMotors_Reset', lambda: (SDTest.Call('cVehicleMotors_Reset', 'tvi', vmos), vmomotors(vmos)))
check('cVehicleMotors_ClearEffects', lambda: (SDTest.Call('cVehicleMotors_ClearEffects', 'tvi', vmos), vmomotors(vmos)))
vmos2 = vmoz(0xc8)
SDTest.Call('cVehicleMotors_ctor', 'tii', vmos2)
check('cVehicleMotors_SetOwner', lambda: (SDTest.Call('cVehicleMotors_SetOwner', 'tvii', vmos2, 0x1234), vmoi(vmos2 + 4)))

# ---- a motor without models ----
vmom = vmoz(0x11c)
check('cVehicleMotor_ctor', lambda: (SDTest.Call('cVehicleMotor_ctor', 'tii', vmom)[0] == vmom, vmomotor(vmom)))
vmosetbyte(vmom + 0x40, 0x41)
vmosetbyte(vmom + 0x60, 1)
vmosetbyte(vmom + 0x6c, 1)
check('cVehicleMotor_Reset', lambda: (SDTest.Call('cVehicleMotor_Reset', 'tvi', vmom), vmomotor(vmom)))
for vmoon in (1, 0):
	check('cVehicleMotor_SetEnabled' + fmt((vmoon,)), lambda on=vmoon: (SDTest.Call('cVehicleMotor_SetEnabled', 'tvii', vmom, on),
		vmomotor(vmom)))
check('cVehicleMotor_SetEffect', lambda: (SDTest.Call('cVehicleMotor_SetEffect', 'tvii', vmom, 2), SDTest.Call('cVehicleMotor_SetEffectParam', 'tviif',
	vmom, 0, 1.5), vmomotor(vmom)))
check('cVehicleMotor_Render', lambda: (SDTest.Call('cVehicleMotor_Render', 'tvi', vmom), vmomotor(vmom)))
# the animations with the flags and lengths poked: the models have no animation in their channels
for vmoon, vmodur, vmot, vmok in ((0, 2.0, 0.5, 0.5), (1, 2.0, 0.5, 0.5), (1, 2.0, 1.75, -1.0), (1, 0.7, 3.1, 1.0),
		(1, 0.0, 0.5, 0.0)):
	vmosetbyte(vmom + 0x60, vmoon)
	vmosetbyte(vmom + 0x6c, vmoon)
	vmoset(vmom + 0x64, (vmodur, 0.25))
	vmoset(vmom + 0x70, (vmodur,))
	check('cVehicleMotor_UpdateAnimM' + fmt((vmoon, vmodur, vmot)), lambda t=vmot: (SDTest.Call('cVehicleMotor_UpdateAnimM', 'tvif',
		vmom, t), vmofl(vmom + 0x64, 2)))
	check('cVehicleMotor_SetAnimF' + fmt((vmoon, vmodur, vmok)), lambda k=vmok: (SDTest.Call('cVehicleMotor_SetAnimF', 'tvif', vmom,
		k), vmofl(vmom + 0x70, 1)))
# the animation starts with the flags set do nothing
vmosetbyte(vmom + 0x60, 1)
vmosetbyte(vmom + 0x6c, 1)
check('cVehicleMotor_StartAnim on', lambda: (SDTest.Call('cVehicleMotor_StartAnimF', 'tvis', vmom, 'x'), SDTest.Call('cVehicleMotor_StartAnimM', 'tvis',
	vmom, 'x'), vmomotor(vmom)))
check('cVehicleMotor_Load empty', lambda: (SDTest.Call('cVehicleMotor_Load', 'tiiiisF', vmom, 0x1234, 1, '', (1.0, 1.0, 1.0))[0],
	vmomotor(vmom)))

# ---- the traffic lane ----
vmotr = vmoz(0x2c)
def vmotraffic(t):
	return (vmoi(t) != 0, vmofl(t + 4, 6), vmoi(t + 0x1c) & 0xffff, vmofl(t + 0x20, 1), vmoi(t + 0x24), vmob(t + 0x28))
check('cVehicleTraffic_ctor', lambda: (SDTest.Call('cVehicleTraffic_ctor', 'tii', vmotr)[0] == vmotr, vmotraffic(vmotr)))
for vmon in ('Speed', 'Speed2', 'Spee', 'Sp', 'Active', ''):
	check('cVehicleTraffic_VarF ' + vmon, lambda n=vmon: (SDTest.Call('cVehicleTraffic_VarF', 'tbisDi', vmotr, n, 0.0, 0),
		SDTest.Call('cVehicleTraffic_VarF', 'tbisDi', vmotr, n, 1234.5, 1), vmotraffic(vmotr)))
for vmon in ('Active', 'Activ', 'ActiveX', 'Speed'):
	for vmov2 in (5, 0, -1):
		check('cVehicleTraffic_VarI ' + vmon + fmt((vmov2,)), lambda n=vmon, v=vmov2: (SDTest.Call('cVehicleTraffic_VarI', 'tbispi',
			vmotr, n, v, 1), SDTest.Call('cVehicleTraffic_VarI', 'tbispi', vmotr, n, 9, 0), vmotraffic(vmotr)))
for vmon in ('heavy', 'MEDIUM', 'Light', 'Huge', 'Heavy'):
	check('cVehicleTraffic_VarS set ' + vmon, lambda n=vmon: (SDTest.Call('cVehicleTraffic_VarS', 'tbispi', vmotr, 'Type',
		vmostr(n), 1)[0], vmotraffic(vmotr)))
for vmot in (0, 1, 2, 3, -1):
	vmopoke(vmotr + 0x24, vmot)
	check('cVehicleTraffic_VarS get' + fmt((vmot,)), lambda: (lambda r: (r[0], vmostrat(r[1])))(
		SDTest.Call('cVehicleTraffic_VarS', 'tbispi', vmotr, 'Type', 0, 0)))
check('cVehicleTraffic_VarS bad', lambda: (SDTest.Call('cVehicleTraffic_VarS', 'tbispi', vmotr, 'Typ', 0, 0)[0],
	SDTest.Call('cVehicleTraffic_VarS', 'tbispi', vmotr, 'Types', 0, 0)[0]))
for vmon in ('Begin', 'End', 'Beg', 'Endx', 'Bogus'):
	check('cVehicleTraffic_VarV ' + vmon, lambda n=vmon: (SDTest.Call('cVehicleTraffic_VarV', 'tbisDDDi', vmotr, n, 1.5, -2.5, 1e6,
		1), SDTest.Call('cVehicleTraffic_VarV', 'tbisDDDi', vmotr, n, 0.0, 0.0, 0.0, 0), vmotraffic(vmotr)))
check('cVehicleTraffic_SetOff', lambda: (SDTest.Call('cVehicleTraffic_SetOff', 'tvii', vmotr, 0x101), vmotraffic(vmotr)))
# the parent: a fake vehicle with the GetAI slot of the vehicles (its AI at +0x78 is NULL)
vmovt = vmoz(0x100)
vmopoke(vmovt + 0x58, symva('cEntity_GetAI'))
vmofake = vmoz(0x100)
vmopoke(vmofake, vmovt)
check('cVehicleTraffic_SetParent', lambda: (SDTest.Call('cVehicleTraffic_SetParent', 'tvii', vmotr, vmofake), vmoi(vmotr) == vmofake))
vmoskip = SDTest.Peek('g_UseTrafficCollisionSkip', 'i')
for vmoact, vmosk in ((0, 1), (1, 1), (1, 0)):
	vmosetbyte(vmotr + 0x1c, vmoact)
	SDTest.Poke('g_UseTrafficCollisionSkip', vmosk)
	check('cVehicleTraffic_CollideEntities' + fmt((vmoact, vmosk)), lambda: SDTest.Call('cVehicleTraffic_CollideEntities', 'tbi', vmotr)[0])
SDTest.Poke('g_UseTrafficCollisionSkip', vmoskip)
def vmoadd(p, d):
	return (p[0] + d[0], p[1] + d[1], p[2] + d[2])
vmop0 = (pp[0], pp[1], pp[2])
vmoset(vmotr + 4, vmoadd(vmop0, (0.0, 200.0, 0.0)) + vmoadd(vmop0, (3000.0, 200.0, 0.0)))
vmoset(vmotr + 0x20, (500.0,))
vmomoves = ((vmoadd(vmop0, (0.0, 300.0, 0.0)), vmoadd(vmop0, (0.0, -300.0, 0.0)), 300.0),
	(vmoadd(vmop0, (100.0, 250.0, 0.0)), vmoadd(vmop0, (200.0, 250.0, 50.0)), 100.0),
	(vmoadd(vmop0, (100.0, 250.0, 0.0)), vmoadd(vmop0, (900.0, 250.0, 5000.0)), 900.0),
	(vmoadd(vmop0, (0.0, 200.0, 0.0)), vmoadd(vmop0, (0.0, 200.0, 0.0)), 0.0))
for vmooff, vmoact, vmot in ((1, 1, 0), (0, 0, 0), (0, 1, 0), (0, 1, 1), (0, 1, 2), (0, 1, 5)):
	vmosetbyte(vmotr + 0x28, vmooff)
	vmosetbyte(vmotr + 0x1c, vmoact)
	vmopoke(vmotr + 0x24, vmot)
	for vmoa, vmob2, vmolen in vmomoves:
		for vmor in (20.0, 2000.0):
			check('cVehicleTraffic_Move' + fmt((vmooff, vmoact, vmot, vmoa, vmob2, vmor, vmolen)),
				lambda a=vmoa, b=vmob2, r=vmor, l=vmolen: (SDTest.Call('cVehicleTraffic_Move', 'tbiFFfFfi', vmotr, a, b, r,
				(7.0, 7.0, 7.0), l, 0), vmob(vmotr + 0x1d)))

# ---- the weapon manager ----
def vmowm(w):
	return (vmoi(w) - 0x400000, vmoi(w + 4), vmoi(w + 0x28) == w + 0x28, vmoi(w + 0x2c) == w + 0x28, vmoi(w + 0x30),
		vmoi(w + 0x34) == w, vmoi(w + 0x44) & 0xffffff)
vmow = vmoz(0x48)
check('cWeaponManager_ctor', lambda: (SDTest.Call('cWeaponManager_ctor', 'tii', vmow)[0] == vmow, vmowm(vmow)))
for vmostamp, vmot in ((1.0, 0.5), (0.25, 0.5), (0.5, 0.5), (-1.0, 0.25), (2.0, -1.0)):
	vmoset(vmow + 0x10, (vmostamp, vmostamp * 3.0))
	check('cWeaponManager_DelayFire' + fmt((vmostamp, vmot)), lambda t=vmot: (SDTest.Call('cWeaponManager_DelayFire', 'tvif', vmow, t),
		vmofl(vmow + 0x10, 2)))
	vmoset(vmow + 0x10, (vmostamp, vmostamp * 3.0))
	check('cTargetLock_SetHookDelay' + fmt((vmostamp, vmot)), lambda t=vmot: (SDTest.Call('cTargetLock_SetHookDelay', 'tvif', vmow, t),
		vmofl(vmow + 0x10, 2)))
vmonw = vmoi(vmoworld + 0x1bb8)
vmona = vmoi(vmoworld + 0x1bc4)
check('weapon tables', lambda: (vmonw, vmona))
for vmok in range(vmonw):
	check('WeapTable_At' + fmt((vmok,)), lambda k=vmok: SDTest.Call('WeapTable_At', 'tiii', vmoworld + 0x1bb8, k)[0] ==
		vmoi(vmoworld + 0x1bbc) + 0xb0 * k)
vmoweaps = vmoz(0xc * vmonw + 8)
vmoammo = vmoz(4 * vmona + 8)
for vmok in range(3 * vmonw + 2):
	vmopoke(vmoweaps + 4 * vmok, 0x11110000 + vmok)
for vmok in range(vmona + 2):
	vmopoke(vmoammo + 4 * vmok, 0x22220000 + vmok)
vmopoke(vmow + 0x18, vmoweaps)
vmopoke(vmow + 0x20, vmoammo)
check('cWeaponManager_ClearWeaps', lambda: (SDTest.Call('cWeaponManager_ClearWeaps', 'tvi', vmow), vmoints(vmoweaps, 3 * vmonw + 2)))
check('cWeaponManager_ClearAmmo', lambda: (SDTest.Call('cWeaponManager_ClearAmmo', 'tvi', vmow), vmoints(vmoammo, vmona + 2)))
vmopoke(vmow + 0x18, vmoz(8))
vmopoke(vmow + 0x1c, vmoz(8))
vmopoke(vmow + 0x20, vmoz(8))
check('cWeaponManager_dtor', lambda: (SDTest.Call('cWeaponManager_dtor', 'tvi', vmow), vmowm(vmow)))
vmow = vmoz(0x48)
SDTest.Call('cWeaponManager_ctor', 'tii', vmow)
check('cWeaponManager_Delete(0)', lambda: (SDTest.Call('cWeaponManager_Delete', 'tiii', vmow, 0)[0] == vmow, vmowm(vmow)))
vmow = SDTest.Call('operator_new', 'ii', 0x48)[0]
SDTest.Call('cWeaponManager_ctor', 'tii', vmow)
vmopoke(vmow + 0x18, 0)
vmopoke(vmow + 0x1c, 0)
vmopoke(vmow + 0x20, 0)
check('cWeaponManager_Delete(1)', lambda: SDTest.Call('cWeaponManager_Delete', 'tiii', vmow, 1)[0] == vmow)

# ---- a car of our own ----
vmocarobj = Scrap.CreateEntity('SDVmoCar', 54000.0, 50000.0, 50000.0, 'Car')
vmoslot = SDTest.Call('HashTable_Slot', 'tiis', vmoworld + 4, 'SDVmoCar')[0]
vmonode = SDTest.Call('PtrTable_Find', 'tiisi', vmoworld + 4, 'SDVmoCar', vmoslot)[0]
vmocar = 0
if vmonode:
	vmocar = vmoi(vmonode)
vmoiscar = 0
if vmocar:
	vmoiscar = SDTest.Call(vmoi(vmoi(vmocar) + 0x14) - 0x400000, 'tbis', vmocar, 'Car')[0]
vmohasmodel = 0
if vmoiscar:
	vmohasmodel = SDTest.Call('CGameModel_IsLoaded', 'tbi', vmocar + 0xb8)[0]
if not vmohasmodel:
	vmocar = 0
check('VMO car', lambda: (vmocar != 0, vmoiscar, vmohasmodel))
if vmocar:
	vmoms = vmocar + 0x720
	check('car motors', lambda: (vmoi(vmoms) - 0x400000, vmoi(vmoms + 4) == vmocar))
	check('car type', lambda: SDTest.Peek(vmocar + 0x568, '@s'))
	# a dead owner would leave a death trail debris on Stop
	vmoalive = vmoi(vmocar + 0x238) > 0
	check('car alive', lambda: vmoalive)
	vmonames = []
	for vmok in range(4):
		vmonames.append(vmostrat(SDTest.Call('cVehicleMotors_GetMotorName', 'tiiip', vmoms, vmok, 0)[0]))
	check('car motor names', lambda: vmonames)
	for vmok in range(4):
		vmomk = vmoi(vmoms + 8 + 4 * vmok)
		if vmomk:
			check('car motor %d' % vmok, lambda m=vmomk: vmomotor(m))
	check('car ship effects', lambda: vmoeff(vmoms + 0x20))
	for vmok in range(5):
		check('car NumSlots' + fmt((vmok,)), lambda k=vmok: (SDTest.Call('cVehicleMotors_NumSlots', 'tiii', vmoms, k)[0],
			SDTest.Call('cVehicleMotors_VarI', 'tbispi', vmoms, 'nEng%d' % k, 0, 0), SDTest.Call('cVehicleMotors_VarI', 'tbispi', vmoms,
			'NEng%d' % k, 0, 0)))
	for vmok in range(4):
		for vmoside in (0, 1):
			check('car GetExhaust' + fmt((vmok, vmoside)), lambda k=vmok, s=vmoside: (SDTest.Call('cVehicleMotors_GetExhaust',
				'tbiiiFF', vmoms, k, s, (1.0, 2.0, 3.0), (4.0, 5.0, 6.0)), SDTest.Call('cVehicleMotors_GetSlot', 'tbiiiFF', vmoms, k,
				s, (1.0, 2.0, 3.0), (4.0, 5.0, 6.0))))
			for vmon in ('PEng%d%s', 'AEng%d%s', 'PEngSlot%d%s'):
				vmonm = vmon % (vmok, chr(65 + vmoside))
				check('car VarV ' + vmonm, lambda n=vmonm: SDTest.Call('cVehicleMotors_VarV', 'tbisDDDi', vmoms, n, 0.0, 0.0, 0.0,
					0))
		check('car VarS get MEng%d' % vmok, lambda k=vmok: (lambda r: (r[0], vmostrat(r[1])))(
			SDTest.Call('cVehicleMotors_VarS', 'tbispi', vmoms, 'MEng%d' % k, 0, 0)))
	check('car GetExhaust NULL', lambda: (SDTest.Call('cVehicleMotors_GetExhaust', 'tbiiiii', vmoms, 0, 0, 0, 0)[0],
		SDTest.Call('cVehicleMotors_GetSlot', 'tbiiiii', vmoms, 0, 0, 0, 0)[0]))
	# a motor reloaded with its own file (VarS MEng0 at scale 1, then Load at another scale)
	for vmok in range(4):
		if vmonames[vmok]:
			vmofile = vmostr(vmonames[vmok])
			check('car VarS set MEng%d' % vmok, lambda k=vmok, f=vmofile: (SDTest.Call('cVehicleMotors_VarS', 'tbispi', vmoms,
				'MEng%d' % k, f, 1)[0], vmomotor(vmoi(vmoms + 8 + 4 * k))))
			check('car SetMotor %d' % vmok, lambda k=vmok, n=vmonames[vmok]: (SDTest.Call('cVehicleMotors_SetMotor', 'tiiisfff', vmoms,
				k, n, 1.5, 1.0, 0.5)[0], vmomotor(vmoi(vmoms + 8 + 4 * k))))
			check('car SetMotor %d again' % vmok, lambda k=vmok, n=vmonames[vmok]: (SDTest.Call('cVehicleMotors_SetMotor', 'tiiisfff',
				vmoms, k, n, 1.0, 1.0, 1.0)[0], vmomotor(vmoi(vmoms + 8 + 4 * k))))
	check('car SetMotor bad', lambda: SDTest.Call('cVehicleMotors_SetMotor', 'tiiisfff', vmoms, 4, 'x', 1.0, 1.0, 1.0)[0])
	# the ship effects made again
	check('car ClearEffects', lambda: (SDTest.Call('cVehicleMotors_ClearEffects', 'tvi', vmoms), vmoeff(vmoms + 0x20)))
	check('car InitEffects', lambda: (SDTest.Call('cVehicleMotors_InitEffects', 'tvi', vmoms), vmoeff(vmoms + 0x20)))
	for vmon, vmov2 in (('FxClass', 30), ('EngFxClass0', 21), ('EngFxClass1', 0), ('ShipMotorColorG', 0x40)):
		check('car VarI set ' + vmon, lambda n=vmon, v=vmov2: (SDTest.Call('cVehicleMotors_VarI', 'tbispi', vmoms, n, v, 1),
			vmoints(vmoms + 0x30, 1), vmoints(vmoms + 0x8c, 2)))
	vmoeffs = [vmoms + 0x20]
	for vmok in range(4):
		vmomk = vmoi(vmoms + 8 + 4 * vmok)
		if vmomk:
			vmoeffs.append(vmomk + 0x74)
	for vmoon in (1, 0, 1):
		if not vmoalive:
			break
		check('car SetEnabled' + fmt((vmoon,)), lambda on=vmoon: (SDTest.Call('cVehicleMotors_SetEnabled', 'tvii', vmoms, on),
			map(vmoeff, vmoeffs)))
	for vmoon in (0, 1):
		for vmoef in vmoeffs:
			check('car SetVisible' + fmt((vmoon,)), lambda e=vmoef, on=vmoon: (SDTest.Call('cVehicleMotorsEffects_SetVisible', 'tvii', e, on),
				vmoeff(e)))
	vmofx = vmoi(vmocar + 0x224)
	vmops = 0
	vmonp = 0
	if vmofx and vmoi(vmofx + 0x28 + 0xac):
		vmops = vmofx + 0x28
		vmonp = SDTest.Call('CGameParticles_NumParticles', 'tii', vmops)[0]
	check('car embedded fx', lambda: (vmofx != 0, vmops != 0, vmonp))
	# the glow into the particles of the embedded effects, while they hold all exhausts
	vmoneed = 0
	for vmoef in vmoeffs:
		vmoneed = vmoneed + 2 * vmoi(vmoef + 0x7c)
	if vmops and vmonp >= vmoneed:
		for vmothr, vmokk, vmodist in ((1.0, 1.0, 100.0), (0.5, 0.25, 5000.0), (0.8, 2.0, 7000.0), (1.0, 1.0, 9000.0),
				(0.3, 0.7, 20000.0)):
			vmofirst = 0
			for vmoef in vmoeffs:
				vmor2 = SDTest.Call('cVehicleMotorsEffects_Glow', 'tvifffpi', vmoef, vmothr, vmokk, vmodist, vmofirst, vmops)
				vmofirst = vmor2[1]
				check('car Glow' + fmt((vmothr, vmokk, vmodist)), lambda e=vmoef, f=vmofirst: (f, vmob(e + 0xa0)))
			for vmok in range(vmofirst):
				check('car Glow particle %d' % vmok, lambda k=vmok: vmoparticle(vmops, k))
		vmofirst = 0
		for vmoef in vmoeffs:
			vmor2 = SDTest.Call('cVehicleMotorsEffects_StopGlow', 'tvipi', vmoef, vmofirst, vmops)
			vmofirst = vmor2[1]
			check('car StopGlow', lambda e=vmoef, f=vmofirst: (f, vmob(e + 0xa0)))
		for vmok in range(vmoneed):
			check('car StopGlow particle %d' % vmok, lambda k=vmok: vmoparticle(vmops, k))
	# smoke and Calima into the same particles, rand seeded
	if vmops:
		for vmoef in vmoeffs:
			for vmot in (0.001, 0.02):
				vmoset(vmoef + 0xa4, (0.004,))
				vmosrand(4321)
				check('car Smoke' + fmt((vmot,)), lambda e=vmoef, t=vmot: (SDTest.Call('cVehicleMotorsEffects_Smoke', 'tvifi', e, t, vmops),
					vmofl(e + 0xa4, 1), vmopart(vmops)))
		for vmothr, vmosp, vmon in ((0.1, 0.0, 1), (0.9, 0.5, 2), (-1.0, 2.0, 3)):
			vmosrand(97)
			check('VMEffects_SetupHaze' + fmt((vmothr, vmosp, vmon)), lambda t=vmothr, s=vmosp, n=vmon:
				(SDTest.Call('VMEffects_SetupHaze', 'vffFii', t, s, (10.0, 20.0, 30.0, -5.0, 0.0, 5.0, 1.0, 2.0, 3.0)[:3 * n],
				n, vmops), vmopart(vmops)))
	# the update: a fixed frame time and the values of the embedded effects set by hand
	if vmofx and vmops:
		vmotimer = vmocar + 0x110 + 0xc
		vmosaved = (vmoi(vmotimer), vmoints(vmofx + 8, 1), vmoints(vmofx + 0x188, 1), vmoints(vmofx + 0x190, 2),
			vmoints(vmocar + 0x4ac, 8), vmoints(vmocar + 0x4e4, 3), vmoints(vmocar + 0x490, 2), vmoi(vmocar + 0xf0))
		vmoset(vmotimer, (0.04,))
		vmoset(vmocar + 0x490, (0.0, 1.0))
		def vmoupd(name, thr, speed, vis, dist, alpha, time):
			vmoset(vmocar + 0x4b0, (thr,))
			vmoset(vmocar + 0x4c0, (speed, 0.5, 0.75))
			vmoset(vmocar + 0x4bc, (0.25,))
			vmoset(vmocar + 0x4e4, (0.0, 0.0, speed))
			vmoset(vmofx + 8, (dist,))
			vmosetbyte(vmofx + 0x188, vis)
			vmoset(vmofx + 0x190, (alpha, time))
			vmosrand(1357)
			SDTest.Call('cVehicleMotors_Update', 'tvi', vmocar + 0x720)
			r = [vmomotors(vmocar + 0x720)]
			for vmok in range(4):
				m = vmoi(vmocar + 0x728 + 4 * vmok)
				if m:
					r.append(vmomotor(m))
					r.append(SDTest.Call('CGameModel_GetPos', 'tiiF', m, (0.0, 0.0, 0.0))[1])
					r.append(SDTest.Call('CGameModel_GetRot', 'tiiF', m, (0.0, 0.0, 0.0))[1])
			for e in vmoeffs:
				r.append((vmotrail(e + 0x38), vmotrail(e + 0x3c)))
			check('cVehicleMotors_Update ' + name, lambda r=r: r)
		vmoupd('still', 0.0, 0.0, 1, 1000.0, 1.0, 0.0)
		vmoupd('still 2', 0.0, 0.0, 1, 1000.0, 1.0, 0.0)
		vmoupd('start', 0.5, 300.0, 1, 1000.0, 1.0, 0.0)
		vmoupd('fast', 1.0, 3000.0, 1, 5000.0, 0.5, 0.0)
		vmoupd('fast 2', 1.0, 3500.0, 1, 5000.0, 0.5, 0.0)
		vmoupd('far', 0.8, 2000.0, 1, 9000.0, 1.0, 0.0)
		vmoupd('back', -1.0, 500.0, 1, 2000.0, 1.0, 0.0)
		vmoupd('slow', 0.005, 10.0, 1, 2000.0, 1.0, 0.0)
		vmoupd('hidden', 0.5, 300.0, 0, 2000.0, 1.0, 1.0)
		vmoupd('hidden late', 0.5, 300.0, 0, 2000.0, 1.0, 10.0)
		vmoupd('visible', 0.5, 300.0, 1, 2000.0, 1.0, 0.0)
		vmopoke(vmocar + 0xf0, 1)
		vmoupd('player', 0.5, 300.0, 1, 2000.0, 1.0, 0.0)
		vmopoke(vmocar + 0xf0, vmosaved[7])
		vmoset(vmotimer, (0.0,))
		vmoupd('dt 0', 0.0, 0.0, 1, 1000.0, 1.0, 0.0)
		vmopoke(vmotimer, vmosaved[0])
		vmopoke(vmofx + 8, vmosaved[1][0])
		vmopoke(vmofx + 0x188, vmosaved[2][0])
		vmopoke(vmofx + 0x190, vmosaved[3][0])
		vmopoke(vmofx + 0x194, vmosaved[3][1])
		for vmok in range(8):
			vmopoke(vmocar + 0x4ac + 4 * vmok, vmosaved[4][vmok])
		for vmok in range(3):
			vmopoke(vmocar + 0x4e4 + 4 * vmok, vmosaved[5][vmok])
		for vmok in range(2):
			vmopoke(vmocar + 0x490 + 4 * vmok, vmosaved[6][vmok])
	for vmoef in vmoeffs:
		check('car HideTrails', lambda e=vmoef: (SDTest.Call('cVehicleMotorsEffects_HideTrails', 'tvi', e), vmotrail(e + 0x38), vmob(e + 0x68)))
	if vmoalive:
		check('car Stop', lambda: (SDTest.Call('cVehicleMotors_SetEnabled', 'tvii', vmoms, 0), map(vmoeff, vmoeffs)))
	check('car SetEffect', lambda: (SDTest.Call('cVehicleMotors_SetEffect', 'tvii', vmoms, 0), SDTest.Call('cVehicleMotors_SetEffectParam', 'tviif', vmoms, 0,
		0.0), vmomotors(vmoms)))
	check('car Reset', lambda: (SDTest.Call('cVehicleMotors_Reset', 'tvi', vmoms), vmomotors(vmoms)))
