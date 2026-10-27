# engine3d Engine/LeeM3D.c (the reader of the M3D files and the scene passes), BspQ3.c, RenderSuelo.c. Python 1.5.2,
# runs inside sdcall.py after the other checks: check, fmt come from there. The M3D reader reads binary streams built
# here in a cFileMem that is put on the Lee stack (no labels); the scene passes run on scenes built here.
# Not called: Escena_Load (needs an M3D file), the animation paths of the camera and light readers, the polygon
# faces, light maps, Build, Render and the device functions of the Q3 BSP loader, Batch_Flush with meshes.
def m3dz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def m3dnew(n):
	p = SDTest.Call(0x1334d0, 'ii', n)[0]
	SDTest.Call(0x319210, 'iiii', p, 0, n)
	return p
def m3di(a):
	return SDTest.Peek(a, '@i')
def m3dpk(a, v):
	SDTest.Poke(a - 0x400000, v)
def m3dfl(a, n):
	r = ()
	while n > 0:
		k = n
		if k > 60:
			k = 60
		r = r + SDTest.Call(0x3197d0, 'iFii', (0.0,) * k, a, 4 * k)[1]
		a = a + 4 * k
		n = n - k
	return r
def m3dwhich(l, p):
	for i in range(len(l)):
		if l[i] == p:
			return i
	return p != 0
def m3dsetf(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def m3dints(a, n):
	r = ()
	for i in range(n):
		r = r + (m3di(a + 4 * i),)
	return r
def m3dstr(a):
	return SDTest.Peek(a, '@p')
def m3dcad(a, s):
	SDTest.Call(0x2a1940, 'tvis', a, s)

# the stream: ints (variable length), floats, strings
m3dst = []
def m3dI(v):
	if v < 0:
		u = -1 - v
		first = 0x40
	else:
		u = v
		first = 0
	if u < 64:
		m3dst.append(first | u)
		return
	m3dst.append(0x80 | first | (u & 0x3f))
	u = u >> 6
	while 1:
		b = u & 0x7f
		u = u >> 7
		if u:
			m3dst.append(b | 0x80)
		else:
			m3dst.append(b)
			return
def m3dF(v):
	m3dst.append(('f', v))
def m3dV(x, y, z):
	m3dF(x)
	m3dF(y)
	m3dF(z)
def m3dS(s):
	m3dI(len(s))
	for c in s:
		m3dst.append(ord(c))
m3dsave = (SDTest.Peek(0x4e73bc, 'i'), SDTest.Peek(0x4e1fa4, 'i'), SDTest.Peek(0x4e1fa0, 'i'),
	SDTest.Peek(0x4e1fb0, 'i'), SDTest.Peek(0x4e27dc, 'i'), SDTest.Peek(0x4e25cc, 'i'), SDTest.Peek(0x4e24c4, 'i'),
	SDTest.Peek(0x4e1fa8, 'i'), SDTest.Peek(0x4e1fac, 'i'), SDTest.Peek(0x432a7c, 'i'))
# the stream of m3dst into a memory file on the Lee stack, binary without labels
def m3dopen():
	n = 0
	for x in m3dst:
		if type(x) == type(()):
			n = n + 4
		else:
			n = n + 1
	buf = m3dz(n + 16)
	o = 0
	for x in m3dst:
		if type(x) == type(()):
			m3dsetf(buf + o, (x[1],))
			o = o + 4
		else:
			SDTest.Call(0x319210, 'iiii', buf + o, x, 1)
			o = o + 1
	fm = m3dz(0x20)
	SDTest.Call(0x12e050, 'tiiiii', fm, buf, n, 0)
	SDTest.Call(0x12e1b0, 'tiiii', fm, 0, 1)
	pila = m3dz(0x514)
	m3dpk(pila + 0x104, fm)
	SDTest.Poke(0x4e73bc, pila)
	SDTest.Poke(0x4e1fa4, (m3dsave[1] & -65536) | 1)
	del m3dst[:]
	return fm
def m3dpos(fm):
	return m3di(fm + 0x18) - m3di(fm + 0xc)
SDTest.Poke(0x4e1fa0, 14)
SDTest.Poke(0x4e1fb0, 3)
SDTest.Poke(0x4e1fa8, 0)
SDTest.Poke(0x4e1fac, 2)

# the readers of single values
m3dV(1.5, -2.0, 3.25)
m3dV(4.0, 5.0, 6.0)
m3dF(7.5)
m3dF(-8.0)
m3dS('quoted name')
m3dV(0.5, 0.25, 0.125)
m3dF(1.0)
m3dF(0.5)
m3dF(0.0)
m3dF(0.6)
m3dF(0.8)
m3dF(0.0)
m3dF(0.0)
m3dF(1.0)
m3dF(0.3)
m3dV(1.0, 1.0, 1.0)
m3dfm = m3dopen()
check('M3D_ReadVec3Ret', lambda: SDTest.Call(0x2bce40, 'iFzi', (0.0, 0.0, 0.0), None, 1)[1])
check('M3D_ReadVec3Next', lambda: SDTest.Call(0x2bce60, 'vFi', (0.0, 0.0, 0.0), 0)[1])
check('M3D_ReadVec2Next', lambda: SDTest.Call(0x2bcd70, 'vF', (0.0, 0.0))[1])
check('M3D_ReadQString', lambda: SDTest.Call(0x2bd060, 'szii', None, 0, 0)[0])
check('M3D_ReadMatrix', lambda: SDTest.Call(0x2bca30, 'vsF', 'mt', (0.0,) * 16)[1])
check('M3D_NextLine', lambda m3dfm=m3dfm: (SDTest.Call(0x2c08a0, 'v'), m3dpos(m3dfm)))

# the animation tables
m3dtb = m3dz(0x30 * 6)
m3dS('sdint')
m3dI(1)
m3dI(2)
m3dI(10)
m3dI(-200)
m3dI(-1)
m3dI(7)
m3dS('sdflt')
m3dI(1)
m3dI(-3)
m3dF(1.25)
m3dS('A')
m3dI(1)
m3dI(3)
m3dF(1.0)
m3dF(2.0)
m3dF(3.0)
m3dS('B')
m3dI(1)
m3dI(1)
m3dF(4.0)
m3dI(-2)
m3dF(5.0)
m3dS('C')
m3dI(1)
m3dI(-3)
m3dF(-6.5)
m3dS('D')
m3dI(1)
m3dI(-3)
m3dF(0.5)
m3dfm = m3dopen()
def m3dtab(t):
	n = m3di(t)
	return n, m3dints(m3di(t + 4), n), m3di(t + 8) & 0xff
check('M3DTabla_ReadInt', lambda m3dtb=m3dtb: (SDTest.Call(0x2c59d0, 'tvis', m3dtb, 'sdint'), m3dtab(m3dtb)))
check('M3DTabla_ReadFloat', lambda m3dtb=m3dtb: (SDTest.Call(0x2c5c70, 'tvis', m3dtb + 0xc, 'sdflt'),
	m3dtab(m3dtb + 0xc), m3dfl(m3di(m3dtb + 0x10), 3)))
check('M3DTabla4_Read', lambda m3dtb=m3dtb: (SDTest.Call(0x2c5fb0, 'tvizzzzi', m3dtb + 0x30, 'A', 'B', 'C', 'D', 1),
	m3dfl(m3di(m3dtb + 0x34), 3), m3dfl(m3di(m3dtb + 0x40), 3), m3dfl(m3di(m3dtb + 0x4c), 3),
	m3dfl(m3di(m3dtb + 0x58), 3), m3di(m3dtb + 0x5c) & 0xff))
for i in (0, 1, 2):
	check('M3DTabla4_GetVec' + fmt((i,)), lambda i=i, m3dtb=m3dtb: SDTest.Call(0x2c6940, 'tiiFi', m3dtb + 0x30,
		(0.0, 0.0, 0.0), i)[1])
	check('M3DTabla4_GetQuat' + fmt((i,)), lambda i=i, m3dtb=m3dtb: SDTest.Call(0x2c6850, 'tiiFi', m3dtb + 0x30,
		(0.0, 0.0, 0.0, 0.0), i)[1])
check('M3DTabla4_SetVec', lambda m3dtb=m3dtb: (SDTest.Call(0x2c2a70, 'tviiF', m3dtb + 0x30, 1, (9.0, 8.0, 7.0)),
	SDTest.Call(0x2c6940, 'tiiFi', m3dtb + 0x30, (0.0, 0.0, 0.0), 1)[1]))
for i in (0, 2):
	check('M3DTabla4_RotToQuat' + fmt((i,)), lambda i=i, m3dtb=m3dtb: (SDTest.Call(0x2c1280, 'tvii', m3dtb + 0x30, i),
		SDTest.Call(0x2c6850, 'tiiFi', m3dtb + 0x30, (0.0, 0.0, 0.0, 0.0), i)[1]))

# the track of a node and its frame matrices
m3dnodos = m3dz(0x2ec * 4)
SDTest.Poke(0x4e27dc, m3dnodos)
for i in range(4):
	m3dsetf(m3dnodos + 0x2ec * i + 0x78, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))
m3dk = 0
for nm in ('PosX', 'PosY', 'PosZ', 'Sgn', 'QRotX', 'QRotY', 'QRotZ', 'QRotA', 'URotX', 'URotY', 'URotZ', 'URotA', 'EscX',
		'EscY', 'EscZ'):
	m3dS(nm)
	m3dI(1)
	if nm == 'Sgn' or nm[:3] == 'Esc':
		m3dI(-3)
		m3dF(1.0)
	elif nm[-1] == 'A':
		m3dI(3)
		m3dF(0.5)
		m3dF(1.0)
		m3dF(-0.25)
	else:
		m3dI(3)
		m3dF(m3dk * 0.5)
		m3dF(1.0 - m3dk * 0.25)
		m3dF(m3dk + 0.125)
	m3dk = m3dk + 1
m3dfm = m3dopen()
check('M3DPista_Read', lambda m3dnodos=m3dnodos, m3dfm=m3dfm: (SDTest.Call(0x2c6050, 'tvi', m3dnodos + 0x10c),
	m3dpos(m3dfm), m3dfl(m3di(m3dnodos + 0x10c + 0x40), 3), m3dfl(m3di(m3dnodos + 0x10c + 0x58), 3),
	m3dfl(m3di(m3dnodos + 0x10c + 0x70), 3)))
for i in (0, 1, 2):
	check('M3DNodo_FrameMatrix' + fmt((i,)), lambda i=i, m3dnodos=m3dnodos: SDTest.Call(0x2c13c0, 'tiiFi', m3dnodos,
		(0.0,) * 16, i)[1])
m3dsetf(m3dnodos, (1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0))
check('M3DNodo_CalcMatrix', lambda m3dnodos=m3dnodos: (SDTest.Call(0x2c1490, 'tii', m3dnodos)[0] == m3dnodos + 0x78,
	m3dfl(m3dnodos + 0x78, 16)))
m3dsetf(m3dnodos + 0x78, (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0))

# the materials of a file
m3dlista = m3dnew(0x10)
m3dpk(m3dlista, 0x7d6d48)
SDTest.Poke(0x4e25cc, 0)
m3dI(0)
m3dI(1)
m3dS('SDMat(+alpha_test)(TF)')
m3dS('Standard')
m3dV(0.1, 0.2, 0.3)
m3dV(0.5, 0.6, 1.0)
m3dV(1.0, 0.0, 0.25)
m3dF(12.0)
m3dF(0.75)
m3dF(0.25)
m3dS('Blinn')
m3dF(0.5)
m3dI(1)
m3dI(0)
m3dS('Additive')
m3dI(2)
m3dS('diffuse')
m3dS('mapa (env2)')
m3dS('Bitmap')
m3dF(0.8)
m3dS('sdtex.tga')
m3dI(1)
m3dI(0)
m3dI(1)
m3dI(1)
m3dI(1)
m3dS('Explicit')
m3dF(0.25)
m3dF(-0.5)
m3dF(2.0)
m3dF(3.0)
m3dF(0.1)
m3dI(0)
m3dS('no')
m3dI(0)
m3dS('nulo')
m3dI(0)
m3dI(1)
m3dI(1)
m3dS('SDMulti')
m3dS('Multi/Sub-Object')
m3dV(0.0, 0.0, 0.0)
m3dV(1.0, 1.0, 1.0)
m3dV(0.5, 0.5, 0.5)
m3dF(0.0)
m3dF(0.0)
m3dF(0.0)
m3dI(0)
m3dI(1)
m3dI(0)
m3dI(1)
m3dS('SDSub(+transp:premult)')
m3dS('Standard')
m3dV(0.25, 0.25, 0.25)
m3dV(0.75, 0.5, 0.25)
m3dV(0.0, 0.0, 0.0)
m3dF(1.0)
m3dF(1.0)
m3dF(0.5)
m3dS('Phong')
m3dF(0.0)
m3dI(0)
m3dI(1)
m3dS('Filter')
m3dI(0)
m3dI(0)
m3dfm = m3dopen()
def m3dmat(m):
	if not m:
		return None
	r = (m3dstr(m + 4), m3dints(m + 0x10, 12), m3di(m + 0xc) != 0)
	for i in range(5):
		mp = m3di(m + 0x9c + 4 * i)
		if mp:
			r = r + (i, m3dstr(mp), m3di(mp + 8), m3di(mp + 0xc) & 0xffffff, m3dfl(mp + 0x10, 8))
	return r
check('M3D_LeeMaterial', lambda m3dlista=m3dlista, m3dfm=m3dfm: (SDTest.Call(0x2bfc50, 'viii', m3dlista, 0, -1),
	SDTest.Peek(0x4e25cc, 'i'), m3dmat(SDTest.Peek(0x4e28e8, 'i'))))
check('M3D_LeeMaterial multi', lambda m3dlista=m3dlista, m3dfm=m3dfm: (SDTest.Call(0x2bfc50, 'viii', m3dlista, 1, -1),
	SDTest.Peek(0x4e25cc, 'i'), m3dmat(SDTest.Peek(0x4e28ec, 'i')), m3dmat(SDTest.Peek(0x4e28f0, 'i')),
	(SDTest.Peek(0x4e32ec, 'i'), SDTest.Peek(0x4e32f0, 'i'), SDTest.Peek(0x4e3aec, 'i'), SDTest.Peek(0x4e3af0, 'i')),
	m3dpos(m3dfm)))
m3dmats = [SDTest.Peek(0x4e28e8, 'i'), SDTest.Peek(0x4e28ec, 'i'), SDTest.Peek(0x4e28f0, 'i')]
for a, b in ((0, -1), (1, -1), (1, 0), (1, 1), (1, 5), (0, 0), (0, 3), (9, -1), (9, 0)):
	check('M3D_FindMat' + fmt((a, b)), lambda a=a, b=b, m3dmats=m3dmats: m3dwhich(m3dmats, SDTest.Call(0x2bc920, 'iii', a, b)[0]))
check('M3D_MatUsado', lambda m3dmats=m3dmats: (SDTest.Call(0x2c6730, 'vi', m3dmats[2]), SDTest.Call(0x2c6730, 'vi', 0),
	SDTest.Peek(0x4e30e8, 'i') & 0xffffff))
check('M3D_CompartirMats', lambda m3dlista=m3dlista, m3dmats=m3dmats: (SDTest.Call(0x2c6a60, 'vi', m3dlista),
	[SDTest.Peek(0x4e28e8, 'i'), SDTest.Peek(0x4e28ec, 'i'), SDTest.Peek(0x4e28f0, 'i')] == m3dmats))
m3dV(0.0, 0.5, 1.0)
m3dV(0.2, 0.4, 0.6)
m3dV(1.0, 1.0, 1.0)
m3dF(2.0)
m3dF(0.0)
m3dF(1.0)
m3dfm = m3dopen()
m3dm = SDTest.Call(0x233830, 'tii', m3dnew(0xb0))[0]
check('M3D_ReadColores', lambda m3dm=m3dm: (SDTest.Call(0x2c06d0, 'vi', m3dm), m3dints(m3dm + 0x10, 12)))

# the objects: camera, light, portal, mesh
def m3dnodo(name, ind):
	n = SDTest.Call(0x23cf40, 'tii', m3dnew(0xfc))[0]
	m3dcad(n + 4, name)
	m3dpk(n + 0x44, ind)
	return n
m3dcam = m3dnodo('sdcam', 0)
check('M3D_NodoDat', lambda m3dcam=m3dcam, m3dnodos=m3dnodos: SDTest.Call(0x2c69f0, 'ii', m3dcam)[0] == m3dnodos)
m3dsetf(m3dcam + 0x1c, (1.0, 2.0, 3.0, 0.0, 0.6, 0.0, 0.8))
for near, far in ((1.0, 500.0), (0.001, 5000.0)):
	m3dI(1)
	m3dS('target')
	m3dF(0.7)
	m3dI(1)
	m3dF(near)
	m3dF(far)
	m3dF(10.0)
	m3dF(20.0)
	m3dF(30.0)
m3dfm = m3dopen()
for i in (0, 1):
	if i:
		m3dcam = m3dnodo('sdcam2', 0)
		m3dsetf(m3dcam + 0x1c, (1.0, 2.0, 3.0, 0.0, 0.6, 0.0, 0.8))
	check('M3D_LeeCamara' + fmt((i,)), lambda m3dcam=m3dcam, m3dnodos=m3dnodos: (SDTest.Call(0x2bd150, 'viii', m3dcam,
		0, 0), m3dfl(m3di(m3dcam + 0x3c) + 8, 14), m3dstr(m3dnodos + 0xf8)))
m3dluz = m3dnodo('sdluz(mult=50)(radcoef=20)', 1)
m3dsetf(m3dluz + 0x1c, (4.0, 5.0, 6.0, 0.0, 0.0, 0.0, 1.0))
m3dsetf(m3dnodos + 0x2ec + 0x2c, (1.0, 2.0, 4.0))
m3dS('tspot')
m3dI(0)
m3dS('Off')
m3dI(0)
m3dI(1)
m3dV(1.0, 0.5, 0.25)
m3dF(2.0)
m3dF(30.0)
m3dF(45.0)
m3dI(1)
m3dF(10.0)
m3dF(100.0)
m3dS('omni')
m3dI(1)
m3dS('luztarget')
m3dS('On')
m3dI(0)
m3dI(0)
m3dV(0.0, 1.0, 0.0)
m3dF(0.5)
m3dI(0)
m3dfm = m3dopen()
for i in (0, 1):
	if i:
		m3dluz = m3dnodo('sdluz2', 1)
	check('M3D_LeeLuz' + fmt((i,)), lambda m3dluz=m3dluz: (SDTest.Call(0x2beb20, 'viii', m3dluz, 0, 0),
		m3di(m3di(m3dluz + 0x3c) + 8), m3di(m3di(m3dluz + 0x3c) + 0xc) & 0xffff, m3dfl(m3di(m3dluz + 0x3c) + 0x10, 6),
		m3di(m3di(m3dluz + 0x3c) + 0x28), m3dfl(m3di(m3dluz + 0x3c) + 0x2c, 7)))
m3dpor = m3dnodo('sdportal', 2)
m3dsetf(m3dnodos + 0x2ec * 2 + 0x78, (2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 1.0))
m3dI(1)
m3dF(4.0)
m3dF(5.0)
m3dS('sector\3511 ')
m3dS('sector2')
m3dI(1)
m3dF(0.0)
m3dF(5.0)
m3dS('a')
m3dS('b')
m3dfm = m3dopen()
for i in (0, 1):
	if i:
		m3dpor = m3dnodo('sdportal2', 2)
	check('M3D_LeePortal' + fmt((i,)), lambda m3dpor=m3dpor, m3dnodos=m3dnodos: (SDTest.Call(0x2c08b0, 'vii', m3dpor, 0),
		m3dfl(m3di(m3dpor + 0x3c) + 0xc, 2), m3dstr(m3dnodos + 0x2ec * 2 + 0xf8),
		m3dstr(m3dnodos + 0x2ec * 2 + 0xfc)))
check('M3D_LeePortal anim', lambda m3dpor=m3dpor: SDTest.Call(0x2c08b0, 'vii', m3dpor, 1))

# a mesh: 4 vertices, 2 faces, Physique weights (one small weight spread)
m3dmal = m3dnodo('sdmalla', 3)
def m3dmesh(phys, imat):
	m3dS('Malla')
	m3dI(4)
	m3dI(3)
	m3dI(2)
	m3dI(1)
	m3dV(1.0, 0.0, 0.0)
	m3dF(1.0)
	m3dF(0.0)
	m3dF(0.0)
	m3dF(1.0)
	m3dF(0.5)
	m3dF(0.0)
	m3dF(0.0)
	m3dF(1.0)
	m3dF(0.0)
	m3dV(1.0, 2.0, 1.0)
	m3dI(imat)
	m3dV(0.5, 0.25, 1.0)
	for i in range(4):
		m3dI(i)
		m3dV(i * 1.0, i * 0.5 + 1.0, -i * 2.0)
	m3dI(phys)
	if phys:
		m3dI(0)
		m3dI(1)
		m3dI(4)
		for i in range(4):
			m3dI(i)
			m3dI(3)
			m3dS('Bip01 Spine')
			m3dF(0.6)
			m3dS('Bip01 Pelvis')
			m3dF(0.395)
			if i == 2:
				m3dS('Bip01 Spine')
				m3dF(0.1)
			else:
				m3dS('Bip01 Head')
				m3dF(0.005)
	for f in ((0, 1, 2, 0), (1, 3, 1, 1)):
		m3dI(f[3])
		m3dI(f[0])
		m3dI(f[1])
		m3dI(f[2])
		m3dI(f[3])
	for i in range(3):
		m3dI(i)
		m3dF(i * 0.5)
		m3dF(1.0 - i * 20.0)
	for i in range(2):
		m3dI(i)
		m3dI(0)
		m3dI(1)
		m3dI(2)
	for f in ((0, 1, 2), (1, 3, 1)):
		for v in f:
			m3dI(v)
			m3dV(0.0, v * 1.0, 1.0)
def m3dmalla(n):
	m = m3di(n + 0x3c)
	if not m:
		return None
	nv, nt = m3di(m + 8), m3di(m + 0x10)
	r = (nv, m3di(m + 0xc), nt, m3dfl(m3di(m + 0x20), 3 * nv), m3dfl(m3di(m + 0x24), 2 * m3di(m + 0xc)))
	for i in range(nt):
		t = m3di(m + 0x28) + 0x44 * i
		r = r + (m3dints(t, 6), m3dfl(t + 0x18, 9), m3dstr(m3di(t + 0x3c) + 4), m3di(t + 0x40))
	return r
def m3dskin(d):
	r = ()
	n = m3di(d + 0x104)
	for i in range(n):
		v = m3di(d + 0x108) + 0x1c * i
		nb = m3di(v)
		r = r + (nb,)
		for k in range(nb):
			r = r + (m3dstr(v + 4 + 4 * k),)
		r = r + (m3dfl(v + 0x10, nb),)
	return r
m3dmesh(1, 0)
m3dmesh(0, 9)
m3dfm = m3dopen()
check('M3D_LeeMalla', lambda m3dmal=m3dmal, m3dnodos=m3dnodos, m3dfm=m3dfm: (SDTest.Call(0x2bd4b0, 'vii', m3dmal, 0),
	m3dpos(m3dfm), m3dmalla(m3dmal), m3dskin(m3dnodos + 0x2ec * 3), m3dfl(m3dnodos + 0x2ec * 3 + 0xb8, 16),
	SDTest.Peek(0x4e30e8, 'i') & 0xffffff))
m3dmal2 = m3dnodo('sdmalla2', 3)
check('M3D_LeeMalla 2', lambda m3dmal2=m3dmal2, m3dfm=m3dfm: (SDTest.Call(0x2bd4b0, 'vii', m3dmal2, 0), m3dpos(m3dfm),
	m3dmalla(m3dmal2)))
check('M3D_LeeMalla anim', lambda m3dmal2=m3dmal2: SDTest.Call(0x2bd4b0, 'vii', m3dmal2, 1))
SDTest.Poke(0x4e73bc, m3dsave[0])

# the constructors and destructors of the reader data
def m3dgarb(n):
	p = SDTest.Call(0x1334d0, 'ii', n)[0]
	SDTest.Call(0x319210, 'iiii', p, 0x55, n)
	return p
m3dno = m3dgarb(0x2ec)
check('M3DNodo_ctor', lambda m3dno=m3dno: (SDTest.Call(0x2bbb90, 'tii', m3dno)[0] == m3dno, m3dints(m3dno + 0xf8, 6),
	m3dints(m3dno + 0x10c, 0x33), m3dints(m3dno + 0x1d8, 0x33), m3dints(m3dno + 0x2a4, 18)))
m3dcad(m3dno + 0xf8, 'obj')
check('M3DNodo_Delete', lambda m3dno=m3dno: SDTest.Call(0x2bc1f0, 'tiii', m3dno, 1)[0] == m3dno)
m3dsv = m3dgarb(0x1c)
check('M3DSkinVert_ctor', lambda m3dsv=m3dsv: (SDTest.Call(0x2bbcc0, 'tii', m3dsv)[0] == m3dsv, m3dints(m3dsv, 7)))
m3dcad(m3dsv + 8, 'bone')
check('M3DSkinVert_dtor', lambda m3dsv=m3dsv: (SDTest.Call(0x2bc160, 'tvi', m3dsv), m3dints(m3dsv, 7)))
check('M3DSkinVert_Delete', lambda m3dsv=m3dsv: SDTest.Call(0x2bc2a0, 'tiii', m3dsv, 1)[0] == m3dsv)
m3dt1 = m3dgarb(0xc)
check('M3DTabla_ctor', lambda m3dt1=m3dt1: (SDTest.Call(0x2bba90, 'tii', m3dt1)[0] == m3dt1, m3dints(m3dt1, 3)))
m3dc1 = m3dgarb(4)
check('Cad_ctor', lambda m3dc1=m3dc1: (SDTest.Call(0x2bbab0, 'tii', m3dc1)[0] == m3dc1, m3di(m3dc1)))
m3dpi = m3dgarb(0xcc)
check('M3DPista_ctor', lambda m3dpi=m3dpi: (SDTest.Call(0x2bbac0, 'tii', m3dpi)[0] == m3dpi, m3dints(m3dpi, 0x33)))
check('M3DTabla4_dtor', lambda m3dpi=m3dpi: (SDTest.Call(0x2bbd50, 'tvi', m3dpi), m3dints(m3dpi, 12)))
check('M3DPista_dtor', lambda m3dnodos=m3dnodos: (SDTest.Call(0x2bbd90, 'tvi', m3dnodos + 0x10c),
	m3dints(m3dnodos + 0x10c, 0x33)))
check('M3DTabla6_dtor', lambda m3dtb=m3dtb: (SDTest.Call(0x2bbe50, 'tvi', m3dtb), m3dints(m3dtb, 18)))
check('Tabla_Free', lambda m3dtb=m3dtb: (SDTest.Call(0x2bbcf0, 'tvi', m3dtb + 0x48), SDTest.Call(0x2bbd20, 'tvi',
	m3dtb + 0x54), m3dints(m3dtb + 0x48, 6)))
check('M3DSkin_Free', lambda m3dnodos=m3dnodos: (SDTest.Call(0x2bc0d0, 'tvi', m3dnodos + 0x2ec * 3 + 0x104),
	m3dints(m3dnodos + 0x2ec * 3 + 0x104, 2)))
SDTest.Poke(0x4e1fa4, m3dsave[1])

# the scene passes on scenes built here
def m3descena(name, nodes):
	e = SDTest.Call(0x24e7d0, 'tii', m3dnew(0x64))[0]
	m3dcad(e + 4, name)
	r = []
	for nm, padre, flags, ind, pos in nodes:
		n = m3dnodo(nm, ind)
		m3dpk(n + 0x18, flags)
		m3dsetf(n + 0x1c, pos)
		r.append(n)
	for i in range(len(nodes)):
		if nodes[i][1] >= 0:
			SDTest.Call(0x23eac0, 'tvii', r[i], r[nodes[i][1]])
		SDTest.Call(0x24fa10, 'tvii', e, r[i])
	return e, r
def m3dlist(e):
	r = ()
	n = m3di(e + 0xc)
	while n and len(r) < 100:
		p = m3di(n + 0x14)
		if p:
			p = m3dstr(p + 4)
		r = r + ((m3dstr(n + 4), p, m3di(n + 0x18), m3di(n + 0x44), m3di(n + 0x48), m3di(n + 0x4c), m3di(n + 0x50),
			m3di(n + 0x3c) != 0),)
		n = m3di(n + 0xc)
	return r
m3dq0 = (0.0, 0.0, 0.0, 1.0)
m3de, m3dn = m3descena('sdesc', (('Bip01', -1, 0x200000, 0, (0.0, 1.0, 0.0) + m3dq0),
	('grand (-rt_shadows)', 2, 0, 1, (1.0, 1.0, 1.0, 0.0, 0.6, 0.0, 0.8)),
	('child (+collide)(+no_sector)', 0, 0x4000000, 2, (0.0, 2.0, 0.0, 0.6, 0.0, 0.0, 0.8)),
	('mesh', 0, 0, -1, (0.5, 0.0, 0.0) + m3dq0),
	('other(+ilum_dinam)', -1, 0x400000, -1, (3.0, 0.0, 0.0) + m3dq0)))
SDTest.Call(0x23ea60, 'tvii', m3dn[3], SDTest.Call(0x2626d0, 'tii', m3dnew(0x4c))[0])
check('Escena built', lambda m3de=m3de: m3dlist(m3de))
check('Nodo_FlagNombre', lambda m3dn=m3dn: (SDTest.Call(0x2c1d00, 'vizzi', m3dn[1], '(+x)', '(-rt_shadows)', 0x4000000),
	SDTest.Call(0x2c1d00, 'vizzi', m3dn[1], None, None, 0x4000000), SDTest.Call(0x2c1d00, 'vizzi', m3dn[2], 'child',
	None, 0x10), m3di(m3dn[1] + 0x18), m3di(m3dn[2] + 0x18)))
check('Escena_FlagsNombres', lambda m3de=m3de: (SDTest.Call(0x2c6150, 'vi', m3de), m3dlist(m3de)))
check('Nodo_BuscarDescendiente', lambda m3dn=m3dn: (m3dwhich(m3dn, SDTest.Call(0x2c7ca0, 'iii', m3dn[0], m3dn[0])[0]),
	m3dwhich(m3dn, SDTest.Call(0x2c7ca0, 'iii', m3dn[3], m3dn[2])[0]), m3dwhich(m3dn, SDTest.Call(0x2c7ca0, 'iii',
	m3dn[0], m3dn[2])[0])))
check('Nodo_BuscarDescendienteAnt', lambda m3dn=m3dn: (m3dwhich(m3dn, SDTest.Call(0x2c7d10, 'iii', m3dn[1], m3dn[0])[0]),
	m3dwhich(m3dn, SDTest.Call(0x2c7d10, 'iii', m3dn[4], m3dn[0])[0]), m3dwhich(m3dn, SDTest.Call(0x2c7d10, 'iii', m3dn[2],
	m3dn[4])[0])))
check('Escena_BuscarColapsable', lambda m3dn=m3dn: (m3dwhich(m3dn, SDTest.Call(0x2c7550, 'iii', m3dn[0], m3dn[0])[0]),
	m3dwhich(m3dn, SDTest.Call(0x2c7550, 'iii', m3dn[0], m3dn[2])[0])))
check('Escena_Ordenar', lambda m3de=m3de: (SDTest.Call(0x2c7930, 'vi', m3de), m3dlist(m3de)))
check('Escena_Ordenar again', lambda m3de=m3de: (SDTest.Call(0x2c7930, 'vi', m3de), m3dlist(m3de)))
def m3dmats(n):
	return m3dfl(n + 0x1c, 7), m3dfl(n + 0x5c, 16), m3dfl(n + 0x9c, 16), m3dfl(n + 0xdc, 4)
check('Escena_MatricesObj', lambda m3de=m3de, m3dn=m3dn: (SDTest.Call(0x2c14a0, 'vi', m3de), m3dmats(m3dn[1]),
	m3dmats(m3dn[2])))
check('Escena_AnimLocal', lambda m3de=m3de, m3dn=m3dn: (SDTest.Call(0x2c15f0, 'vii', m3de, 0), m3dmats(m3dn[1]),
	m3dmats(m3dn[3])))
check('Escena_TablaNodos', lambda m3de=m3de: (SDTest.Call(0x2c6b40, 'vi', m3de), m3dlist(m3de)))
check('Escena_FlagsIni', lambda m3de=m3de: (SDTest.Call(0x2c64a0, 'vi', m3de), m3dlist(m3de)))
check('Escena_SkinMesh', lambda m3de=m3de: (SDTest.Call(0x2c1f50, 'vi', m3de), m3dlist(m3de)))
check('Escena_PhysiqueSkin', lambda m3de=m3de: (SDTest.Call(0x2c7300, 'vi', m3de), m3dlist(m3de)))
check('Escena_ColapsarBip', lambda m3de=m3de, m3dn=m3dn: (SDTest.Call(0x2c1d60, 'vi', m3de), m3dlist(m3de),
	m3dmalla(m3dn[0])))
m3de2, m3dn2 = m3descena('sdesc2', (('other(+ilum_dinam)', -1, 0, -1, (3.0, 0.0, 0.0) + m3dq0),
	('mesh', 0, 0, -1, (0.5, 0.0, 0.0) + m3dq0), ('child (+collide)(+no_sector)', 1, 0, -1, (0.0, 2.0, 0.0) + m3dq0),
	('extra', 1, 0, -1, (0.0, 0.0, 0.0) + m3dq0)))
check('Escena_PadresAnim', lambda m3de2=m3de2, m3de=m3de: (SDTest.Call(0x2c7da0, 'vii', m3de2, m3de), m3dlist(m3de2)))
m3de3, m3dn3 = m3descena('models/sd/sdesc.lod1', (('Bip01', -1, 0, 5, (0.0, 1.0, 0.0) + m3dq0),
	('child (+collide)(+no_sector)', 0, 0, 5, (0.0, 2.5, 0.0, 0.6, 0.0, 0.0, 0.8)), ('mesh', 0, 0x1000, 5,
	(0.5, 0.0, 0.0) + m3dq0), ('nolod', 0, 0, 5, (0.0, 0.0, 0.0) + m3dq0), ('xLOD1y', 0, 0, 5, m3dq0[:3] + m3dq0)))
SDTest.Poke(0x4e1ec4, m3de)
check('Escena_SetupLOD', lambda m3de3=m3de3: (SDTest.Call(0x2c6e90, 'vi', m3de3), m3dstr(m3de3 + 0x44), m3dlist(m3de3)))
SDTest.Poke(0x4e1ec4, 0)
m3dpk(m3dn[1] + 0x18, m3di(m3dn[1] + 0x18) | 8)
check('Escena_Jerarquia', lambda m3de=m3de: (SDTest.Call(0x2c7610, 'vi', m3de), m3dlist(m3de)))
SDTest.Poke(0x4e24c4, m3dlista)
check('Escena_MatsUsados', lambda m3de2=m3de2: (SDTest.Call(0x2c10a0, 'vi', m3de2), m3di(m3de2 + 0x58),
	m3dmat(m3di(m3di(m3de2 + 0x5c))), SDTest.Peek(0x4e24c4, 'i')))
SDTest.Poke(0x4e24c4, m3dsave[6])
SDTest.Poke(0x4e25cc, m3dsave[5])
SDTest.Poke(0x4e27dc, m3dsave[4])
SDTest.Poke(0x4e1fa0, m3dsave[2])
SDTest.Poke(0x4e1fb0, m3dsave[3])
SDTest.Poke(0x4e1fa8, m3dsave[7])
SDTest.Poke(0x4e1fac, m3dsave[8])
SDTest.Poke(0x432a7c, m3dsave[9])

# BspQ3.c: a map in memory without polygons, the mesh lists, the dot product
check('Vec3_DotX87', lambda: SDTest.Call(0x2c8920, 'fFF', (1.5, -2.0, 3.0), (0.25, 4.0, 1e-3))[0])
m3dq3 = m3dnew(0x6c)
m3dlump = m3dz(0x200)
m3dpk(m3dlump, 0x50534249)
m3dpk(m3dlump + 4, 0x2e)
m3dpk(m3dlump + 8 + 8 * 1, 0x90)
m3dpk(m3dlump + 8 + 8 * 1 + 4, 0x90)
m3dpk(m3dlump + 8 + 8 * 10, 0x120)
m3dpk(m3dlump + 8 + 8 * 10 + 4, 0x58)
m3dpk(m3dlump + 8 + 8 * 13, 0x178)
m3dpk(m3dlump + 8 + 8 * 13 + 4, 0x68)
SDTest.Call(0x3197d0, 'iisi', m3dlump + 0x90, 'textures/sd/wall', 17)
m3dpk(m3dlump + 0x90 + 0x40, 0x20)
SDTest.Call(0x3197d0, 'iisi', m3dlump + 0xd8, 'textures/sd/floor', 18)
m3dsetf(m3dlump + 0x120, (1.0, 2.0, 3.0, 0.5, 0.25, 0.1, 0.2, 0.0, 0.0, 1.0))
m3dsetf(m3dlump + 0x14c, (4.0, 5.0, 6.0, 0.5, 0.75, 0.3, 0.4, 0.0, 1.0, 0.0))
m3dpk(m3dlump + 0x178, 1)
m3dpk(m3dlump + 0x178 + 8, 2)
m3dpk(m3dlump + 0x178 + 0xc, 1)
m3dpk(m3dlump + 0x178 + 0x10, 1)
m3dpk(m3dlump + 0x178 + 0x18, 6)
m3dpk(m3dlump + 0x178 + 0x1c, 3)
m3dsetf(m3dlump + 0x178 + 0x3c, (0.1, 0.2, 0.3, 0.4, 0.5, 0.6))
m3dpk(m3dq3 + 4, m3dlump)
m3dpk(m3dq3 + 8, m3dlump)
m3dcad(m3dq3, 'maps/sd/sdq3.bsp')
check('CBspQ3_ReadShaders', lambda m3dq3=m3dq3: (SDTest.Call(0x2c92b0, 'tvi', m3dq3), m3di(m3dq3 + 0x20),
	SDTest.Peek(m3di(m3dq3 + 0x44) + 0x48, '@s'), m3di(m3di(m3dq3 + 0x44) + 0x40)))
check('CBspQ3_ReadVerts', lambda m3dq3=m3dq3: (SDTest.Call(0x2c9350, 'tvi', m3dq3), m3di(m3dq3 + 0x10),
	m3dfl(m3di(m3dq3 + 0x40), 22)))
def m3dfaces(q):
	r = ()
	for i in range(m3di(q + 0x24)):
		f = m3di(q + 0x50) + 0x58 * i
		r = r + (m3di(f + 4), m3di(f + 0xc), m3di(f + 0x10), m3di(f + 0x18), m3di(f + 0x20), m3di(f + 0x24),
			m3dfl(f + 0x40, 6), m3di(m3di(q + 0x54) + 8 * i) == f, m3di(f + 0x14) - m3di(q + 0x40))
	return r
check('CBspQ3_ReadFaces', lambda m3dq3=m3dq3: (SDTest.Call(0x2c8a60, 'tvi', m3dq3), m3di(m3dq3 + 0x24),
	m3di(m3dq3 + 0x4c), m3dfaces(m3dq3)))
for id, el in ((1, 0x48), (10, 0x2c), (13, 0x68), (2, 4)):
	check('CBspQ3_ReadLump' + fmt((id, el)), lambda id=id, el=el, m3dq3=m3dq3: (lambda r: (r[0], r[1] != 0))(
		SDTest.Call(0x2c93f0, 'tiiipi', m3dq3, id, 0, el)))
m3dms = SDTest.Call(0x2c86f0, 'tii', m3dnew(0x404))[0]
m3dfa = m3dz(0x58 * 3)
m3dpk(m3dfa + 0xc, 0x1234)
m3dpk(m3dfa + 0x10, 2)
m3dpk(m3dfa + 0x58 + 0xc, 0x1234)
m3dpk(m3dfa + 0x58 + 0x10, 3)
m3dpk(m3dfa + 0xb0 + 0xc, 0x1234)
m3dpk(m3dfa + 0xb0 + 0x10, 2)
def m3dmallas(ms, fa):
	r = (m3di(ms + 0x400),)
	for i in range(m3di(ms + 0x400)):
		m = m3di(ms + 4 * i)
		r = r + ((m3di(m) - fa) / 0x58, m3di(m + 4), m3di(m + 8), m3di(m + 0x14), m3di(m + 0x1c))
	return r
check('BspQ3Mallas_Count', lambda m3dms=m3dms, m3dfa=m3dfa: (SDTest.Call(0x2c9940, 'tviii', m3dms, m3dfa, 2),
	SDTest.Call(0x2c9940, 'tviii', m3dms, m3dfa + 0x58, 1), SDTest.Call(0x2c9940, 'tviii', m3dms, m3dfa + 0xb0, 1),
	m3dmallas(m3dms, m3dfa)))
check('BspQ3Mallas_Find', lambda m3dms=m3dms, m3dfa=m3dfa: (SDTest.Call(0x2c98c0, 'tiii', m3dms, m3dfa + 0xb0)[0] ==
	m3di(m3dms), SDTest.Call(0x2c98c0, 'tiii', m3dms, m3dfa + 0x58)[0] == m3di(m3dms + 4)))
m3dvl = m3dz(0x28 * 3)
m3dvals = ()
for i in range(30):
	m3dvals = m3dvals + (i * 0.5 - 3.0,)
m3dsetf(m3dvl, m3dvals)
def m3dmalla3(m):
	nt = m3di(m + 4)
	p = m3di(m + 0xc)
	r = (nt, m3di(m + 8), m3di(m + 0x14), m3di(m + 0x1c))
	for i in range(nt):
		r = r + (m3di(p + 6 * i) & 0xffff, m3di(p + 6 * i + 2) & 0xffff, m3di(p + 6 * i + 4) & 0xffff)
	return r + (m3dfl(m3di(m + 0x10), 10 * 3 * nt),)
for k in (0, 1, 2):
	check('BspQ3Mallas_AddTri' + fmt((k,)), lambda k=k, m3dms=m3dms, m3dfa=m3dfa, m3dvl=m3dvl: (SDTest.Call(0x2c9740,
		'tviiiii', m3dms, m3dvl + 0x28 * k, m3dvl + 0x28 * ((k + 1) % 3), m3dvl + 0x28 * ((k + 2) % 3), m3dfa),
		m3dmalla3(m3di(m3dms))))
check('BspQ3Mallas_Free', lambda m3dms=m3dms: (SDTest.Call(0x2c8840, 'tvi', m3dms), m3dints(m3dms, 2),
	m3di(m3dms + 0x400)))
check('BspQ3Malla', lambda: (lambda m: (SDTest.Call(0x2c8880, 'tvi', m), m3dints(m, 11)))(SDTest.Call(0x2c8720, 'tii',
	m3dnew(0x2c))[0]))
check('BspQ3_Identidad', lambda m3dms=m3dms: SDTest.Call(0x2c8770, 'tii', m3dms)[0] == m3dms)
check('BspQ3 globals', lambda: (SDTest.Call(0x2c98a0, 'v'), SDTest.Call(0x2c98b0, 'v'), SDTest.Call(0x2c9d50, 'v'),
	SDTest.Call(0x2c9d80, 'v'), SDTest.Peek(0x4e5bf8, 'i')))
m3dpk(m3dq3 + 4, 0)
check('CBspQ3_dtor', lambda m3dq3=m3dq3: (SDTest.Call(0x2c8780, 'tvi', m3dq3), m3dints(m3dq3, 3), m3dints(m3dq3 + 0x60,
	3)))

# RenderSuelo.c: the order of the batches, a mesh added and taken out again
m3dbm = m3dz(0xc8 * 2)
m3dpk(m3dbm + 0x4c, 0x100)
m3dpk(m3dbm + 0xc8 + 0x4c, 0x200)
m3dbe = m3dz(0x30)
for i, v in ((0, (m3dbm, 5, 2.0)), (1, (m3dbm, 5, 3.0)), (2, (m3dbm + 0xc8, 5, 1.0)), (3, (m3dbm, 7, 0.0))):
	m3dpk(m3dbe + 0xc * i, v[0])
	m3dpk(m3dbe + 0xc * i + 4, v[1])
	m3dsetf(m3dbe + 0xc * i + 8, (v[2],))
m3dbp = m3dz(0x10)
for i in range(4):
	m3dpk(m3dbp + 4 * i, m3dbe + 0xc * i)
for a, b in ((0, 1), (1, 0), (0, 0), (0, 2), (2, 0), (3, 0), (0, 3)):
	check('Batch_Compare' + fmt((a, b)), lambda a=a, b=b, m3dbp=m3dbp: SDTest.Call(0x2c8520, 'iii', m3dbp + 4 * a,
		m3dbp + 4 * b)[0])
m3dold = SDTest.Peek(0x4e4af0, 'i')
check('Batch_AddMalla empty', lambda m3dbm=m3dbm: (SDTest.Call(0x2c81d0, 'vii', m3dbm, 0), SDTest.Peek(0x4e4af0, 'i'),
	SDTest.Peek(0x4e42e8, 'i'), SDTest.Peek(0x4e42ec, 'i') != 0))
m3dpk(m3dbm + 0x18, 3)
m3dpk(m3dbm + 0x4c, 0)
def m3dbatch(m3dbm=m3dbm, m3dold=m3dold):
	SDTest.Call(0x2c81d0, 'vii', m3dbm, 0)
	n = SDTest.Peek(0x4e4af0, 'i')
	e = SDTest.Peek(0x4e42f0 + 4 * m3dold, 'i')
	r = (n - m3dold, m3di(e) == m3dbm, m3di(e + 4), e == SDTest.Peek(0x4e42ec, 'i') + 0xc * m3dold)
	SDTest.Poke(0x4e4af0, m3dold)
	return r
check('Batch_AddMalla', m3dbatch)
check('Batch_Flush empty', lambda m3dold=m3dold: m3dold or (SDTest.Call(0x2c83c0, 'vi', 0), SDTest.Peek(0x4e4af0, 'i')))
