# game Entities/EmbeddedFX/EmbeddedFXVehicle.c (execfile'd by sdlevel.py, Python 1.5.2): the vehicle effects in the
# level FZ on fake vehicles in calloc memory: a vtable of the WithLife slots the effects call (GetAI, GetModel,
# GetRadius, SetFX, SetFXParam and the pilot name of the vehicles), a model of the packs (SPoli1, TProbe,
# TVoteMayor) made visible by hand, the type name at +0x568 and "P_Police" as pilot. The cEmbeddedFXVehicle is
# made by its constructor, inited, given the fake vehicles as owner (sirens, Probe, Vote), updated with a fixed
# frame time (0.04), faded and hidden; the sirens flash at times set by hand, with and without the alarm. Every
# model is disabled and freed again; the world timer, the alarm and MinDistReposAI / MaxDistReposAI are restored.
# Not called: the Render functions (they draw now), the Probe update when the TProbe model has no animation in
# channel 0 (its ChanTime CHECKs), the real vehicles of the level (their models move).
def ei(a):
	return SDTest.Peek(a, '@i')
def eb(a):
	return SDTest.Peek(a, '@b')
def ez(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def efree(p):
	SDTest.Call(0x133550, 'ii', p)
def efl(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def eset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def epoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def estr(a, s):
	SDTest.Call(0x22cf50, 'iisi', a, s, len(s) + 1)
def ebytes(a, offs):
	return map(lambda o, a=a: SDTest.Peek(a + o, '@b'), offs)
def esetbyte(a, v):
	w = ei(a & ~3)
	sh = 8 * (a & 3)
	epoke(a & ~3, (w & ~(0xff << sh)) | (v << sh))

eworld = SDTest.Peek(0x467450, 'i')
edelta = ei(eworld + 0x2d4)
ealarm = ei(eworld + 0x1c70)
erepos = (ei(0x844900), ei(0x844858))
eset(eworld + 0x2d4, (0.04,))
esirenrefs = ei(0x84750c)
check('siren refs', lambda: esirenrefs)

# the fake vehicles
evt = ez(0x100)
for off, fn in ((0x58, 0x4ef2d0), (0x98, 0x4fb380), (0xa0, 0x5085c0), (0xb0, 0x4fc9d0), (0xb4, 0x4fc9e0),
		(0xf4, 0x5083b0)):
	epoke(evt + off, fn)
ecars = []
def ecar(model, typename):
	c = ez(0x700)
	n = ez(0x20)
	ecars.append((c, n))
	epoke(c, evt)
	estr(n, 'SDFakeCar')
	epoke(c + 4, n)
	SDTest.Call(0x20eb10, 'tiis', c + 0xb8, model)
	eset(c + 0x458, (250.0,))
	epoke(c + 0x4c, 1)
	epoke(c + 0x238, 100)
	eset(c + 0x4c0, (5.0,))
	estr(c + 0x568, typename)
	epoke(c + 0x51c, 0)
	estr(c + 0x520, 'P_Police')
	SDTest.Call(0x211d20, 'tviF', c + 0xb8, (pp[0] + 300.0, pp[1] + 50.0, pp[2] - 200.0))
	return c
def eshow(c, vis):
	if vis:
		SDTest.Call(0x20f420, 'tvi', c + 0xb8)
	else:
		SDTest.Call(0x20f3f0, 'tvi', c + 0xb8)
	esetbyte(ei(c + 0xb8) + 0x121, vis)
def ecarfx(c):
	m = c + 0xb8
	p = SDTest.Call(0x2107c0, 'tviip', m, 0, 0)[1]
	return (SDTest.Call(0x210760, 'tiii', m, 0)[0], p, efl(ei(m) + 0x114, 2))
ecpol = ecar('Models/Vehicles/Ships/SPoli1/SPoli1.M3D', 'SPoli1')
ecprobe = ecar('Models/Vehicles/OutTraffic/TProbe.M3D', 'TProbe')
ecvote = ecar('Models/Vehicles/OutTraffic/TVoteMayor.M3D', 'TVoteMayor')
for c in (ecpol, ecprobe, ecvote):
	check('fake car model', lambda c=c: (SDTest.Call(0x210da0, 'tbi', c + 0xb8)[0], SDTest.Call(0x20f7d0, 'tiis',
		c + 0xb8, 'DC_Root')[0] != -1))

# the effect: constructor, flags, init (vehicle and traffic model), done
efx = ez(0x1b0)
def efxstate(f):
	return (ei(f) - 0x400000, ei(f + 4) != 0, efl(f + 8, 1), ebytes(f, (0x188, 0x189, 0x18a, 0x18b, 0x18c)),
		efl(f + 0x190, 5), ei(f + 0x1a4), ei(f + 0x1a8) != 0, ei(f + 0x1ac) != 0)
def efxmodel(f):
	m = f + 0xc
	if not SDTest.Call(0x210da0, 'tbi', m)[0]:
		return 0
	return (SDTest.Call(0x210f30, 'tbi', m)[0], ei(f + 0x28 + 0xac) != 0, ei(f + 0xd8 + 0xac) != 0,
		SDTest.Call(0x20fa30, 'tiiF', m, (0.0, 0.0, 0.0))[1], SDTest.Call(0x210830, 'tiiF', m, (0.0, 0.0, 0.0))[1],
		efl(ei(m) + 0x114, 2))
check('cEmbeddedFXVehicle_ctor', lambda: (SDTest.Call(0x0eb0d0, 'tii', efx)[0] == efx, efxstate(efx)))
for rva, name in ((0x0ee700, 'SetTraffic'), (0x0ee770, 'SetAIRange')):
	for v in (1, 0x101, 0):
		check('cEmbeddedFXVehicle_%s' % name + fmt((v,)), lambda rva=rva, v=v: (SDTest.Call(rva, 'tvii', efx, v),
			efxstate(efx)))
check('cEmbeddedFXVehicle_SetAttached', lambda: (SDTest.Call(0x0ee380, 'tvii', efx, 0x1234), ei(efx + 0x1a4)))
SDTest.Call(0x0ee380, 'tvii', efx, 0)
# Init only after Done: a second Init would load the model again under the attached particles
for traffic in (0, 1):
	SDTest.Call(0x0ee700, 'tvii', efx, traffic)
	check('cEmbeddedFXVehicle_Init' + fmt((traffic,)), lambda: (SDTest.Call(0x0ed7f0, 'tbi', efx)[0],
		efxstate(efx), efxmodel(efx)))
	check('cEmbeddedFXVehicle_Done' + fmt((traffic,)), lambda: (SDTest.Call(0x0ed5f0, 'tvi', efx), efxstate(efx),
		efxmodel(efx)))
SDTest.Call(0x0ee700, 'tvii', efx, 0)
check('cEmbeddedFXVehicle_Init again', lambda: (SDTest.Call(0x0ed7f0, 'tbi', efx)[0], efxstate(efx), efxmodel(efx)))

# the police vehicle: the sirens
def esirens(f):
	s = ei(f + 0x1a8)
	if not s:
		return 0
	return (ei(s) == f, efl(s + 4, 8), ei(0x84750c), SDTest.Call(0x210da0, 'tbi', 0x847478)[0],
		ei(0x847510 + 0xac) != 0)
def eparts(p):
	return (map(lambda o, p=p: ei(p + o), (0, 4, 0x18)), efl(p + 8, 4), efl(p + 0x1c, 1), efl(p + 0x2c, 6),
		efl(p + 0x50, 6), efl(p + 0x74, 6), efl(p + 0x8c, 3), ebytes(p, (0x98, 0x99)))
check('cEmbeddedFXVehicle_SetOwner police', lambda: (SDTest.Call(0x0ee390, 'tvii', efx, ecpol), efxstate(efx),
	efl(efx + 0x198, 3), esirens(efx), efxmodel(efx)))
esetbyte(ecpol + 0x5e0, 1)
check('cEmbeddedFXVehicle_Configure', lambda: (SDTest.Call(0x0ee780, 'tvii', efx, 0), efxstate(efx)))
esetbyte(ecpol + 0x5e0, 0)
SDTest.Call(0x0ee780, 'tvii', efx, 0)
eshow(ecpol, 1)
check('cEmbeddedFXVehicle_Update police', lambda: (SDTest.Call(0x0eb670, 'tvi', efx), efxstate(efx),
	efxmodel(efx), esirens(efx), ecarfx(ecpol)))
check('cEmbeddedFXVehicle_Update police 2', lambda: (SDTest.Call(0x0eb670, 'tvi', efx), efxstate(efx),
	esirens(efx)))
es = ei(efx + 0x1a8)
for t, last, alarm, followed in ((0.08, 0.08, 0, 0), (0.18, 0.18, 1, 0), (0.58, 0.5, 0, 1), (0.69, 0.69, 1, 1),
		(0.88, 0.88, 0, 0), (0.3, 0.3, 0, 1)):
	eset(es + 0x1c, (t, last))
	esetbyte(eworld + 0x1c70, alarm)
	esetbyte(efx + 0x189, followed)
	check('cFXVehicleSirens_Update' + fmt((t, last, alarm, followed)), lambda: (SDTest.Call(0x0ec2a0, 'tvi', es),
		efl(es + 0x1c, 2), eparts(efx + 0x28), eparts(0x847510)))
esetbyte(efx + 0x189, 0)
epoke(eworld + 0x1c70, ealarm)
# the early returns of the sirens: hidden, too slow, faded, driven by a player
for vis, speed, alpha, player in ((0, 5.0, 1.0, 0), (1, 0.0, 1.0, 0), (1, 5.0, 0.5, 0), (1, 5.0, 1.0, 1)):
	eset(es + 0x1c, (0.08, 0.08))
	esetbyte(efx + 0x188, vis)
	eset(ecpol + 0x4c0, (speed,))
	eset(efx + 0x190, (alpha,))
	epoke(ecpol + 0xf0, player)
	check('cFXVehicleSirens_Update off' + fmt((vis, speed, alpha, player)), lambda: (SDTest.Call(0x0ec2a0, 'tvi', es),
		efl(es + 0x1c, 2)))
esetbyte(efx + 0x188, 1)
eset(ecpol + 0x4c0, (5.0,))
eset(efx + 0x190, (1.0,))
epoke(ecpol + 0xf0, 0)

# the fade: StartFade, the AI distances (the defaults, set around the camera distance), the alpha reached
check('cEmbeddedFXVehicle_StartFade', lambda: (SDTest.Call(0x0ee720, 'tvi', efx), efxstate(efx), ecarfx(ecpol)))
check('cEmbeddedFXVehicle_Update fade', lambda: (SDTest.Call(0x0eb670, 'tvi', efx), efxstate(efx), ecarfx(ecpol)))
SDTest.Call(0x0ee770, 'tvii', efx, 1)
d = efl(efx + 8, 1)[0]
for near, far in ((0, 0), (d * 0.5, d * 2.0), (d * 0.25, d * 0.5), (d * 2.0, d * 4.0), (d * 0.9, d)):
	if near:
		eset(0x844900, (near,))
		eset(0x844858, (far,))
	check('cEmbeddedFXVehicle_Update fade AI' + fmt((near != 0,)), lambda: (SDTest.Call(0x0eb670, 'tvi', efx),
		efxstate(efx), ecarfx(ecpol)))
epoke(0x844900, erepos[0])
epoke(0x844858, erepos[1])
SDTest.Call(0x0ee770, 'tvii', efx, 0)
eset(efx + 0x190, (1.0,))
check('cEmbeddedFXVehicle_Update opaque', lambda: (SDTest.Call(0x0eb670, 'tvi', efx), efxstate(efx), ecarfx(ecpol)))
check('cEmbeddedFXVehicle_SetActive(1)', lambda: (SDTest.Call(0x0ee300, 'tvii', efx, 1), efxstate(efx)))
SDTest.Call(0x0ee720, 'tvi', efx)
check('cEmbeddedFXVehicle_SetActive(0)', lambda: (SDTest.Call(0x0ee300, 'tvii', efx, 0), efxstate(efx),
	efxmodel(efx), ecarfx(ecpol)))
# the vehicle hidden: off after a second
eshow(ecpol, 0)
for i in range(3):
	eset(efx + 0x194, (0.98 * i,))
	check('cEmbeddedFXVehicle_Update hidden %d' % i, lambda: (SDTest.Call(0x0eb670, 'tvi', efx), efxstate(efx),
		efxmodel(efx)))

# the Probe
def eprobe(p):
	return (ei(p) - 0x400000, ei(p + 4) != 0, eb(p + 8), ei(p + 0x37c), efl(p + 0x380, 2), efl(p + 0x38c, 1),
		map(lambda o, p=p: ei(p + o + 0xac) != 0, (0xc, 0xbc, 0x16c, 0x21c, 0x2cc)))
# only the particles the effect has set (the others are whatever the system holds)
def eprobeparts(p, n0, n3):
	r = []
	for o, n in ((0xc, n0), (0x21c, n3)):
		for i in range(n):
			x = SDTest.Call(0x21d4e0, 'tbiiFiipp', p + o, i, (0.0, 0.0, 0.0), 0, 0, 0, 0)
			r.append((x[0], x[1], x[2], x[3]))
	return (r, eparts(p + 0x2cc))
check('cEmbeddedFXVehicle_SetOwner probe', lambda: (SDTest.Call(0x0ee390, 'tvii', efx, ecprobe), efxstate(efx),
	efl(efx + 0x198, 3), esirens(efx), ei(0x84750c)))
ep = ei(efx + 0x1ac)
check('cEmbeddedFXVehicleProbe_Init', lambda: (eprobe(ep), eprobeparts(ep, 5, 4)))
em = ei(ecprobe + 0xb8)
eanim = 0
if ei(em + 0xa4) > 0 and SDTest.Call(0x20f0b0, 'tsii', ecprobe + 0xb8, 0)[0]:
	SDTest.Call(0x210fc0, 'tviiii', ecprobe + 0xb8, 0, 0, 0)
	eanim = ei(ei(em + 0xa8)) != 0
check('probe anim', lambda: eanim)
if eanim:
	eshow(ecprobe, 1)
	for lt, tt, rt, dist, n3 in ((0.1, 0.5, 0.2, 500.0, 4), (0.7, 0.98, 0.98, 5000.0, 6), (1.79, 0.2, 0.5, 100000.0, 6)):
		eset(ep + 0x380, (lt, tt))
		eset(ep + 0x38c, (rt,))
		check('cEmbeddedFXVehicle_Update probe' + fmt((lt, tt, rt)), lambda n3=n3: (SDTest.Call(0x0eb670, 'tvi', efx),
			efxstate(efx), eprobe(ep), eprobeparts(ep, 7, n3)))
		eset(efx + 8, (dist,))
		check('cEmbeddedFXVehicleProbe_Update' + fmt((dist,)), lambda: (SDTest.Call(0x0eba30, 'tvi', ep), eprobe(ep),
			eprobeparts(ep, 7, 6)))
eshow(ecprobe, 0)
esetbyte(ep + 8, 1)
check('cEmbeddedFXVehicleProbe_Update hidden', lambda: (SDTest.Call(0x0eba30, 'tvi', ep), eprobe(ep)))
check('cEmbeddedFXVehicleProbe_SetActive', lambda: (SDTest.Call(0x0ee330, 'tvii', ep, 1),
	SDTest.Call(0x0ee330, 'tvii', ep, 0), eprobe(ep)))
check('cEmbeddedFXVehicleProbe_ClearTrail/Done/Configure', lambda: (SDTest.Call(0x0ed7b0, 'tvi', ep),
	SDTest.Call(0x0ed6b0, 'tvi', ep), SDTest.Call(0x0ee8c0, 'tvii', ep, 0), eprobe(ep)))

# the Vote: mayor, then bishop
def evote(v):
	return (ei(v) - 0x400000, ei(v + 4) != 0, eb(v + 8), ei(v + 0xc), ei(v + 0xc0), ei(v + 0x2dc),
		map(lambda o, v=v: ei(v + o + 0xac) != 0, (0x10, 0xcc, 0x17c, 0x22c)))
def evoteparts(v):
	r = []
	for i in range(26):
		x = SDTest.Call(0x21d4e0, 'tbiiFiipp', v + 0x10, i, (0.0, 0.0, 0.0), 0, 0, 0, 0)
		r.append((x[0], x[1], x[2], x[3]))
	for i in range(3):
		for j in range(2):
			x = SDTest.Call(0x21d4e0, 'tbiiFiipp', v + 0xcc + 0xb0 * i, j, (0.0, 0.0, 0.0), 0, 0, 0, 0)
			r.append((x[0], x[1], x[2], x[3]))
		r.append(eparts(v + 0xcc + 0xb0 * i))
	return r
check('cEmbeddedFXVehicle_SetOwner vote', lambda: (SDTest.Call(0x0ee390, 'tvii', efx, ecvote), efxstate(efx)))
ev = ei(efx + 0x1ac)
check('cEmbeddedFXVehicleVote_Init', lambda: evote(ev))
eshow(ecvote, 1)
for t in (1, 2, 0):
	SDTest.Call(0x0ee710, 'tvii', ev, t)
	check('cEmbeddedFXVehicle_Update vote' + fmt((t,)), lambda: (SDTest.Call(0x0eb670, 'tvi', efx), efxstate(efx),
		evote(ev), evoteparts(ev)))
	check('cEmbeddedFXVehicleVote_Update' + fmt((t,)), lambda: (SDTest.Call(0x0ecaa0, 'tvi', ev), evote(ev),
		evoteparts(ev)))
eshow(ecvote, 0)
check('cEmbeddedFXVehicleVote_Update hidden', lambda: (SDTest.Call(0x0ecaa0, 'tvi', ev), evote(ev)))
check('cEmbeddedFXVehicleVote_SetActive/ClearTrails/Configure', lambda: (SDTest.Call(0x0ee350, 'tvii', ev, 1),
	SDTest.Call(0x0ee350, 'tvii', ev, 0), SDTest.Call(0x0ed7c0, 'tvi', ev), SDTest.Call(0x0ee8e0, 'tvii', ev, 0),
	evote(ev)))
check('cEmbeddedFXVehicleVote_Done', lambda: (SDTest.Call(0x0ed700, 'tvi', ev), evote(ev)))
check('cEmbeddedFXVehicle_Done', lambda: (SDTest.Call(0x0ed5f0, 'tvi', efx), efxstate(efx), efxmodel(efx)))
check('cEmbeddedFXVehicle_Delete(0)', lambda: (SDTest.Call(0x0eb570, 'tiii', efx, 0)[0] == efx, ei(efx) - 0x400000))
efree(efx)
check('siren refs after', lambda: (ei(0x84750c), SDTest.Call(0x210da0, 'tbi', 0x847478)[0]))

# the sirens, Probe and Vote on their own: constructors, destructors, deleting destructors
efx = ez(0x1b0)
SDTest.Call(0x0eb0d0, 'tii', efx)
epoke(efx + 4, ecpol)
s = ez(0x24)
check('cFXVehicleSirens_ctor', lambda s=s: (SDTest.Call(0x0eb270, 'tiii', s, efx)[0] == s, ei(s) == efx))
check('cFXVehicleSirens_Init', lambda s=s: (SDTest.Call(0x0ede00, 'tbi', s)[0], efl(s + 4, 8), ei(0x84750c),
	SDTest.Call(0x210da0, 'tbi', 0x847478)[0]))
check('cFXVehicleSirens_dtor', lambda s=s: (SDTest.Call(0x0ed6c0, 'tvi', s), ei(0x84750c),
	SDTest.Call(0x210da0, 'tbi', 0x847478)[0]))
efree(s)
for name, ctor, dtor, delete, size in (('Probe', 0x0eb170, 0x0eb430, 0x0eb5a0, 0x390),
		('Vote', 0x0eb2b0, 0x0eb4f0, 0x0eb5d0, 0x2e0)):
	p = ez(size)
	check('cEmbeddedFXVehicle%s_ctor' % name, lambda p=p, ctor=ctor: (SDTest.Call(ctor, 'tiii', p, efx)[0] == p,
		ei(p) - 0x400000, ei(p + 4) == efx, eb(p + 8), ei(p + size - 4)))
	check('cEmbeddedFXVehicle%s_dtor' % name, lambda p=p, dtor=dtor: (SDTest.Call(dtor, 'tvi', p), ei(p) - 0x400000))
	SDTest.Call(ctor, 'tiii', p, efx)
	check('cEmbeddedFXVehicle%s_Delete(0)' % name, lambda p=p, delete=delete: (SDTest.Call(delete, 'tiii', p, 0)[0]
		== p, ei(p) - 0x400000))
	efree(p)
	p = SDTest.Call(0x1334d0, 'ii', size)[0]
	SDTest.Call(ctor, 'tiii', p, efx)
	check('cEmbeddedFXVehicle%s_Delete(1)' % name, lambda p=p, delete=delete: SDTest.Call(delete, 'tiii', p, 1)[0]
		== p)
epoke(efx + 4, 0)
check('cEmbeddedFXVehicle_dtor', lambda: (SDTest.Call(0x0eb380, 'tvi', efx), ei(efx) - 0x400000))
efree(efx)
efx = SDTest.Call(0x1334d0, 'ii', 0x1b0)[0]
SDTest.Call(0x0eb0d0, 'tii', efx)
check('cEmbeddedFXVehicle_Delete(1)', lambda: SDTest.Call(0x0eb570, 'tiii', efx, 1)[0] == efx)

# back
for c, n in ecars:
	SDTest.Call(0x20f3f0, 'tvi', c + 0xb8)
	SDTest.Call(0x20ec10, 'tvi', c + 0xb8)
	efree(n)
	efree(c)
efree(evt)
epoke(eworld + 0x2d4, edelta)
check('siren refs end', lambda: ei(0x84750c))
