# game World/FX/FXElements.c, FXElementsAB.c and the Orbit constructors of FXElementsOrbit.c (execfile'd by
# sdlevel.py, Python 1.5.2): the map elements made from their real models in Models/Elements (not linked into the
# world): the fire drops (FireAFXFireDrop), the lightnings of FZ (FZFXLighting00), the fake limit of AB and the
# meteors (both on the model ABFakeLimit, it has the nodes Dummy01 and Dummy02), then their Updates with rand seeded
# by srand of the exe. Models count as visible with R_SplitScreen 1 (restored), the fire drops count as near with
# FXElementsFireDropMaxDist 1e30 (restored). Printed: the fields as ints, the particle parameters, the trail points
# (only the ones written since the last Reset) and the positions of the particles (not their age: it may come from
# the clock). Not called: the fire drop sound (FXElementsFireDropSoundAttEnd is 0 in FZ), the car of the player
# near the fake limit (the player is no car in FZ), the lens flare destructors (no constructor of CGameLensFlare
# here), the AB loader with dummies (FZ has none, it runs without).
def fei(a):
	return SDTest.Peek(a, '@i')
def feb(a):
	return SDTest.Peek(a, '@b')
def fez(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def feset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def feints(a, n):
	r = []
	for i in range(n):
		r.append(fei(a + 4 * i))
	return tuple(r)
def fepoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def fesrand(s):
	SDTest.Call('crt_srand', 'vi', s)
# the parameters of a CGameParticles without its padding and pointers
def fepart(p):
	return (feints(p, 0x26), fei(p + 0x98) & 0xffff, feints(p + 0x9c, 2), feb(p + 0xa4), fei(p + 0xac) != 0)
# the active flags and positions of particles first .. first + n - 1
def feparts(p, first, n):
	r = []
	if not fei(p + 0xac):
		return r
	r.append(SDTest.Call('CGameParticles_NumActive', 'tii', p)[0])
	for i in range(first, first + n):
		x = SDTest.Call('CGameParticles_GetParticle', 'tbiiFiiii', p, i, (0.0, 0.0, 0.0), 0, 0, 0, 0)
		r.append((x[0], x[1]))
	return r
# the state of a CGameTrail and its points written since the last Reset
def fetrail(t):
	tr = fei(t)
	if not tr:
		return None
	mx = feb(tr + 0x20)
	num = feb(tr + 0x21)
	head = feb(tr + 0x22)
	n = head
	if head == 0 and num > 0:
		n = mx
	pts = fei(tr + 0x28)
	r = (feb(tr), fei(tr + 8), feints(tr + 0xc, 5), fei(tr + 0x20), fei(tr + 0x24) & 0xffff, feints(tr + 0x2c, 4),
		feb(tr + 0x3c), feints(tr + 0x40, 5))
	if pts:
		r = r + (feints(pts, 5 * n),)
	return r
feworld = SDTest.Peek('g_pWorld', 'i')
fesplit = sym('g_SplitScreen') + 0x400000
fesavedsplit = fei(fesplit)
fepos = fez(0x18)
feset(fepos, (100.0, 50.0, -20.0, 0.0, 0.5, 0.0))
ferot = fepos + 12

# ---- the fire drops ----
femaxdist = sym('g_FireDropMaxDist') + 0x400000
fesavedmax = fei(femaxdist)
fd = fez(0x118)
check('cFXElementsFireDrop_ctor', lambda fd=fd: (SDTest.Call('cFXElementsFireDrop_ctor', 'tiisii', fd, 'FireAFXFireDrop', fepos,
	ferot)[0] == fd, fei(fd) == 0x400000 + sym('cFXElementsFireDrop_vftable'), fei(fd + 4) != 0))
check('cFXElementsFireDrop_Init', lambda fd=fd: (SDTest.Call('cFXElementsFireDrop_Init', 'tbi', fd)[0], fei(fd + 0x2c),
	feints(fd + 0x30, 3), fei(fd + 0xe8) != 0, feints(fd + 0xec, 11)))
fepoke(fesplit, 1)
fepoke(femaxdist, 0x7149f2ca)
# the first update starts a burst (it was 0), then the burst goes on, then it is set by hand around the limits
fesrand(321)
for dt, burst in ((0.1, None), (0.05, None), (0.3, None), (0.02, 0.085), (0.01, 0.11), (0.5, 1.7), (0.04, 0.05),
		(0.2, -0.5)):
	if burst != None:
		feset(fd + 0xf0, (burst,))
	check('cFXElementsFireDrop_Update' + fmt((dt, burst)), lambda fd=fd, dt=dt: (SDTest.Call('cFXElementsFireDrop_Update', 'tvif', fd,
		dt), feb(fd + 0xec), feints(fd + 0xf0, 5), fepart(fd + 0x3c), feparts(fd + 0x3c, 0, 6)))
check('rand after the fire drops', lambda: SDTest.Call('crt_rand', 'i')[0])
fepoke(femaxdist, 0)
check('cFXElementsFireDrop_Update far', lambda fd=fd: (SDTest.Call('cFXElementsFireDrop_Update', 'tvif', fd, 0.1), feb(fd + 0xec),
	feints(fd + 0xf0, 5)))
fepoke(femaxdist, fesavedmax)
check('cFXElementsFireDrop_Release', lambda fd=fd: (SDTest.Call('cFXElementsFireDrop_Release', 'tvi', fd), fei(fd + 0xe4), fei(fd + 0xe8)))
check('cFXElementsFireDrop_Delete', lambda fd=fd: (SDTest.Call('cFXElementsFireDrop_Delete', 'tiii', fd, 0)[0] == fd, fei(fd + 0xe8)))

# ---- the lens flare: nothing to do ----
lf = fez(0x30)
check('cFXElementsLensFlare_Init', lambda lf=lf: SDTest.Call('cFXElementsLensFlare_Init', 'tbi', lf)[0])
check('cFXElementsLensFlare_Update', lambda lf=lf: (SDTest.Call('cFXElementsLensFlare_Update', 'tvif', lf, 0.5), feints(lf, 12)))
check('cFXElementsLensFlare_Release', lambda lf=lf: (SDTest.Call('cFXElementsLensFlare_Release', 'tvi', lf), feints(lf, 12)))

# ---- the lightnings ----
lt = fez(0xe88)
check('cFXElementsLighting_ctor', lambda lt=lt: (SDTest.Call('cFXElementsLighting_ctor', 'tiisii', lt, 'FZFXLighting00', fepos,
	ferot)[0] == lt, fei(lt) == 0x400000 + sym('cFXElementsLighting_vftable'), fei(lt + 0x9ac) != 0))
check('cFXElementsLighting_Init', lambda lt=lt: (SDTest.Call('cFXElementsLighting_Init', 'tbi', lt)[0], fei(lt + 0x2c),
	feints(lt + 0x30, 3), fei(lt + 0xe8) != 0, feb(lt + 0xec), feints(lt + 0xf0, 4)))
check('Lighting links', lambda lt=lt: (feints(lt + 0x100, 0x48), feints(lt + 0x218, 10)))
check('Lighting colours and sizes', lambda lt=lt: feints(lt + 0x240, 0xf0))
check('Lighting bolt parameters', lambda lt=lt: (feints(lt + 0x600, 0x8c), feints(lt + 0x830, 5), feints(lt + 0x844,
	0x3c), fei(lt + 0xe84)))
check('Lighting trails', lambda lt=lt: (fetrail(lt + 0x9ac), fetrail(lt + 0x9b0), fetrail(lt + 0x9d0)))
fesrand(77)
for dt in (0.1, 0.01, 0.02, 0.03, 0.2, 0.01, 0.6, 0.05, 0.05):
	check('cFXElementsLighting_Update' + fmt((dt,)), lambda lt=lt, dt=dt: (SDTest.Call('cFXElementsLighting_Update', 'tvif', lt, dt),
		feb(lt + 0xec), feints(lt + 0xf8, 2), feints(lt + 0x1c8, 20), feints(lt + 0x86c, 0x50),
		feints(lt + 0x9d4, 300)))
	check('Lighting trails after' + fmt((dt,)), lambda lt=lt: map(lambda i, lt=lt: fetrail(lt + 0x9ac + 4 * i),
		range(10)))
	check('Lighting glows after' + fmt((dt,)), lambda lt=lt: (fepart(lt + 0x3c), feparts(lt + 0x3c, 0, 40)))
check('rand after the lightnings', lambda: SDTest.Call('crt_rand', 'i')[0])
fepoke(fesplit, 0)
check('cFXElementsLighting_Update hidden', lambda lt=lt: (SDTest.Call('cFXElementsLighting_Update', 'tvif', lt, 0.1), feb(lt + 0xec),
	fetrail(lt + 0x9ac)))
fepoke(fesplit, 1)
check('cFXElementsLighting_Release', lambda lt=lt: (SDTest.Call('cFXElementsLighting_Release', 'tvi', lt), fei(lt + 0xe8),
	fetrail(lt + 0x9ac)))
check('cFXElementsLighting_Delete', lambda lt=lt: (SDTest.Call('cFXElementsLighting_Delete', 'tiii', lt, 0)[0] == lt, fei(lt + 0xe8)))

# ---- the fake limit of AB ----
fl = fez(0x498)
check('cFXElementsABFakeLimit_ctor', lambda fl=fl: (SDTest.Call('cFXElementsABFakeLimit_ctor', 'tiisii', fl, 'ABFakeLimit', fepos,
	ferot)[0] == fl, fei(fl) == 0x400000 + sym('cFXElementsABFakeLimit_vftable'), fei(fl + 0x110) != 0, fei(fl + 0x3b8) != 0))
fesrand(55)
check('cFXElementsABFakeLimit_Init', lambda fl=fl: (SDTest.Call('cFXElementsABFakeLimit_Init', 'tbi', fl)[0], feints(fl + 0x2c, 3),
	fei(fl + 0xe4) != 0, fei(fl + 0x374) != 0, feints(fl + 0xe8, 10), feints(fl + 0x378, 16), fetrail(fl + 0x110),
	fetrail(fl + 0x114), fetrail(fl + 0x3b8), fetrail(fl + 0x494)))
check('rand after the limit Init', lambda: SDTest.Call('crt_rand', 'i')[0])
for dt in (0.0, 0.1, 2.5, 0.04):
	check('cFXElementsABFakeLimit_Update' + fmt((dt,)), lambda fl=fl, dt=dt: (SDTest.Call('cFXElementsABFakeLimit_Update', 'tvif', fl, dt),
		feints(fl + 0x378, 4), feints(fl + 0xe8, 10), SDTest.Call('CGameParticles_IsVisible', 'tbi', fl + 0x38)[0]))
	check('ABFakeLimit rings after' + fmt((dt,)), lambda fl=fl: (fetrail(fl + 0x3b8), fetrail(fl + 0x3bc),
		fetrail(fl + 0x3b8 + 0x38 * 2), fetrail(fl + 0x3b8 + 0x38 * 3 + 0x34)))
check('cFXElementsABFakeLimit_Clear', lambda fl=fl: (SDTest.Call('cFXElementsABFakeLimit_Clear', 'tvi', fl), feints(fl + 0xe8, 10),
	fetrail(fl + 0x3b8)))
check('cFXElementsABFakeLimit_Delete', lambda fl=fl: SDTest.Call('cFXElementsABFakeLimit_Delete', 'tiii', fl, 0)[0] == fl)

# ---- the meteors of AB ----
fm = fez(0x5780)
check('cFXElementsABFakeMeteors_ctor', lambda fm=fm: (SDTest.Call('cFXElementsABFakeMeteors_ctor', 'tiisii', fm, 'ABFakeLimit', fepos,
	ferot)[0] == fm, fei(fm) == 0x400000 + sym('cFXElementsABFakeMeteors_vftable'), feints(fm + 0x2c, 4), fei(fm + 0x577c) != 0))
check('cFXElementsABFakeMeteors_Init', lambda fm=fm: (SDTest.Call('cFXElementsABFakeMeteors_Init', 'tbi', fm)[0], fei(fm + 0x56c8) != 0,
	fei(fm + 0x5778) != 0))
# the fields of meteor i
def femeteor(m, i):
	return (fei(m + 0x2c + 4 * i), fei(m + 0x7fc + 4 * i), feints(m + 0xfcc + 12 * i, 3), fei(m + 0x273c + 4 * i),
		fei(m + 0x2f0c + 4 * i), fei(m + 0x36dc + 4 * i), fei(m + 0x3eac + 4 * i), fei(m + 0x467c + 4 * i),
		fei(m + 0x4e4c + 4 * i))
# the update sets particles 0..250 of the first system, 1..249 of the second, sprites up to 20: only with systems
# that big (else its CHECKs end the game)
def fesprites(p):
	sp = fei(p + 0xac)
	ms = fei(sp + 0x118)
	if ms:
		return fei(ms + 8)
	return 1000
fesizes = (SDTest.Call('CGameParticles_NumParticles', 'tii', fm + 0x561c)[0], SDTest.Call('CGameParticles_NumParticles', 'tii', fm + 0x56cc)[0],
	fesprites(fm + 0x561c), fesprites(fm + 0x56cc))
check('ABFakeMeteors systems', lambda: fesizes)
fesrand(99)
fedts = ()
if fesizes[0] > 250 and fesizes[1] > 249 and fesizes[2] > 20 and fesizes[3] > 20:
	fedts = (0.1, 0.5, 3.0, 25.0)
for dt in fedts:
	check('cFXElementsABFakeMeteors_Update' + fmt((dt,)), lambda fm=fm, dt=dt: (SDTest.Call('cFXElementsABFakeMeteors_Update', 'tvif', fm,
		dt), map(lambda i, fm=fm: femeteor(fm, i), (0, 1, 2, 3, 100, 250, 251, 252, 400, 499))))
	check('ABFakeMeteors particles after' + fmt((dt,)), lambda fm=fm: (feparts(fm + 0x561c, 0, 4),
		feparts(fm + 0x561c, 249, 2), feparts(fm + 0x56cc, 0, 3), feparts(fm + 0x56cc, 248, 2)))
check('rand after the meteors', lambda: SDTest.Call('crt_rand', 'i')[0])
check('cFXElementsABFakeMeteors_Delete', lambda fm=fm: SDTest.Call('cFXElementsABFakeMeteors_Delete', 'tiii', fm, 0)[0] == fm)
fepoke(fesplit, fesavedsplit)

# the loader: FZ has no AB dummies, the element list stays
fehead = fei(feworld + 0x1bb4)
check('FXElementsAB_Create', lambda fehead=fehead: (SDTest.Call('FXElementsAB_Create', 'v'), fei(feworld + 0x1bb4) == fehead))

# ---- the orbit buoys: constructors and destructors ----
fb = fez(0xf0)
check('cFXElementsOrbitBuoy_ctor', lambda fb=fb: (SDTest.Call('cFXElementsOrbitBuoy_ctor', 'tiisii', fb, 'OrbitBoya0', fepos,
	ferot)[0] == fb, fei(fb) == 0x400000 + sym('cFXElementsOrbitBuoy_vftable'), fei(fb + 4) != 0, fepart(fb + 0x40)))
check('cFXElementsOrbitBuoy_Init', lambda fb=fb: (SDTest.Call('cFXElementsOrbitBuoy_Init', 'tbi', fb)[0], fei(fb + 0xec) != 0))
check('cFXElementsOrbitBuoy_ReleaseLight', lambda fb=fb: (SDTest.Call('cFXElementsOrbitBuoy_ReleaseLight', 'tvi', fb), fei(fb + 0xe8),
	fei(fb + 0xec)))
check('cFXElementsOrbitBuoy_Delete', lambda fb=fb: SDTest.Call('cFXElementsOrbitBuoy_Delete', 'tiii', fb, 0)[0] == fb)
fb = fez(0xf0)
SDTest.Call('cFXElementsOrbitBuoy_ctor', 'tiisii', fb, 'OrbitBoya0', fepos, ferot)
check('cFXElementsOrbitBuoy_dtor', lambda fb=fb: (SDTest.Call('cFXElementsOrbitBuoy_dtor', 'tvi', fb), fei(fb + 0xec)))
fc = fez(0x1ac)
check('cFXElementsOrbitBuoyController_ctor', lambda fc=fc: (SDTest.Call('cFXElementsOrbitBuoyController_ctor', 'tiisii', fc,
	'OrbitBoyaController', fepos, ferot)[0] == fc, fei(fc) == 0x400000 + sym('cFXElementsOrbitBuoyController_vftable'), feints(fc + 0x2c, 0x60)))
check('cFXElementsOrbitBuoyController_Release', lambda fc=fc: (SDTest.Call('cFXElementsOrbitBuoyController_Release', 'tvi', fc), fei(fc + 0x12c)))
check('cFXElementsOrbitBuoyController_Delete', lambda fc=fc: SDTest.Call('cFXElementsOrbitBuoyController_Delete', 'tiii', fc, 0)[0] == fc)
fc = fez(0x1ac)
SDTest.Call('cFXElementsOrbitBuoyController_ctor', 'tiisii', fc, 'OrbitBoyaController', fepos, ferot)
check('cFXElementsOrbitBuoyController_dtor', lambda fc=fc: SDTest.Call('cFXElementsOrbitBuoyController_dtor', 'tvi', fc))
