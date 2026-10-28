# D3DX8 math (src/lib/d3dx8/D3dxMath.c, execfile'd by sdcall.py, Python 1.5.2): the public stubs of the exe called as
# thiscall with a dummy this (they are stdcall), in and out through heap buffers so the aliasing cases run too.
def dxz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def dxget(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def dxset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
dxa = dxz(64)
dxb = dxz(64)
dxo = dxz(64)
dxm = (
	(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0),
	(0.5, 1.25, -2.0, 0.0, 3.0, 0.75, 0.125, 0.0, -1.5, 2.5, 4.0, 0.0, 10.0, -20.0, 30.5, 1.0),
	(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7),
	(1.0, 2.0, 3.0, 4.0, 2.0, 4.0, 6.0, 8.0, 0.5, 0.25, 0.125, 1.0, 7.0, 6.0, 5.0, 4.0),
	(1e-20, 3.0, -7.0, 0.001, 1e20, -0.333, 0.7071, 2.0, 5.5, 1e-3, -1e5, 0.0, 0.0, 1.0, 0.0, 0.0),
	(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
)
dxv = ((1.0, 2.0, 3.0), (0.0, 0.0, 0.0), (1e-8, 0.0, 0.0), (1e-7, 1e-7, 1e-7), (-5.0, 0.25, 1e5), (3e-30, 4e-30, 0.0),
	(1e-40, 0.0, 0.0), (0.577, -0.577, 0.577), (1e19, 1e19, 1e19))
dxang = (0.0, 0.5, -1.0, 3.14159265, 1.5707963, 100.0, -1e6, 1e7, 1e-30, 6.2831853, -0.7853982, 1e-5)

for v in dxv:
	dxset(dxa, v)
	check('D3DX_Vec3Normalize' + fmt(v), lambda: (SDTest.Call('D3DX_Vec3Normalize', 'tiiii', 0, dxo, dxa)[0] == dxo,
		dxget(dxo, 3)))
	check('D3DX_Vec3Normalize in place' + fmt(v), lambda: (SDTest.Call('D3DX_Vec3Normalize', 'tiiii', 0, dxa, dxa)[0] == dxa,
		dxget(dxa, 3)))
	for m in dxm:
		dxset(dxa, v)
		dxset(dxb, m)
		check('D3DX_Vec3Transform' + fmt(v + m), lambda: (SDTest.Call('D3DX_Vec3Transform', 'tiiiii', 0, dxo, dxa, dxb)[0] == dxo,
			dxget(dxo, 3)))
for m in dxm:
	for m2 in dxm:
		dxset(dxa, m)
		dxset(dxb, m2)
		check('D3DX_MatrixMultiply' + fmt(m + m2), lambda: (SDTest.Call('D3DX_MatrixMultiply', 'tiiiii', 0, dxo, dxa, dxb)[0] ==
			dxo, dxget(dxo, 16)))
	dxset(dxa, m)
	dxset(dxb, dxm[1])
	check('D3DX_MatrixMultiply out=a' + fmt(m), lambda: (SDTest.Call('D3DX_MatrixMultiply', 'tiiiii', 0, dxa, dxa, dxb)[0] == dxa,
		dxget(dxa, 16)))
	dxset(dxa, m)
	check('D3DX_MatrixMultiply out=b' + fmt(m), lambda: (SDTest.Call('D3DX_MatrixMultiply', 'tiiiii', 0, dxb, dxa, dxb)[0] == dxb,
		dxget(dxb, 16)))
	dxset(dxa, m)
	check('D3DX_MatrixTranspose' + fmt(m), lambda: (SDTest.Call('D3DX_MatrixTranspose', 'tiiii', 0, dxo, dxa)[0] == dxo,
		dxget(dxo, 16)))
	check('D3DX_MatrixTranspose in place' + fmt(m), lambda: (SDTest.Call('D3DX_MatrixTranspose', 'tiiii', 0, dxa, dxa)[0] == dxa,
		dxget(dxa, 16)))
	dxset(dxa, m)
	dxset(dxo, (9.0,) * 16)
	check('D3DX_MatrixInverse' + fmt(m), lambda: (SDTest.Call('D3DX_MatrixInverse', 'tiiiii', 0, dxo, 0, dxa)[0] == dxo,
		dxget(dxo, 16)))
	dxset(dxa, m)
	check('D3DX_MatrixInverse det' + fmt(m), lambda: (SDTest.Call('D3DX_MatrixInverse', 'tiiiFi', 0, dxa, (7.0,), dxa)[0] == dxa,
		SDTest.Call('D3DX_MatrixInverse', 'tiiiFi', 0, dxo, (7.0,), dxa)[1], dxget(dxa, 16)))
for a in dxang:
	for rva, name in ((sym('D3DX_MatrixRotationX'), 'X'), (sym('D3DX_MatrixRotationY'), 'Y'), (sym('D3DX_MatrixRotationZ'), 'Z')):
		dxset(dxo, (9.0,) * 16)
		check('D3DX_MatrixRotation' + name + fmt((a,)), lambda rva=rva, a=a: (SDTest.Call(rva, 'tiiif', 0, dxo,
			a)[0] == dxo, dxget(dxo, 16)))
	for b in (0.0, 0.3, -2.0):
		for c in (0.0, 1.0, 1e6):
			dxset(dxo, (9.0,) * 16)
			check('D3DX_MatrixRotationYPR' + fmt((a, b, c)), lambda a=a, b=b, c=c: (SDTest.Call('D3DX_MatrixRotationYPR', 'tiiifff', 0,
				dxo, a, b, c)[0] == dxo, dxget(dxo, 16)))
			check('D3DX_QuaternionRotationYPR' + fmt((a, b, c)), lambda a=a, b=b, c=c: (SDTest.Call('D3DX_QuaternionRotationYPR',
				'tiiifff', 0, dxo, a, b, c)[0] == dxo, dxget(dxo, 4)))
for q in ((0.0, 0.0, 0.0, 1.0), (0.5, 0.5, 0.5, 0.5), (0.1, -0.2, 0.3, 0.927), (1e-20, 3.0, -1e5, 2.0),
		(1e20, 1e20, 0.0, 0.0)):
	dxset(dxa, q)
	check('D3DX_MatrixRotationQuaternion' + fmt(q), lambda: (SDTest.Call('D3DX_MatrixRotationQuaternion', 'tiiii', 0, dxo, dxa)[0] == dxo,
		dxget(dxo, 16)))
SDTest.Call('Mem_Free', 'vi', dxa)
SDTest.Call('Mem_Free', 'vi', dxb)
SDTest.Call('Mem_Free', 'vi', dxo)
