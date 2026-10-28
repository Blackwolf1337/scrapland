# Engine/descomp_afin.cpp (src/game/engine3d/Engine/DescompAfin.c, execfile'd by sdcall.py, Python 1.5.2): matrices
# built from rotations, scales (equal, unequal, mirrored), shears and translations taken apart by Matrix_ToTransform,
# the transforms turned back by M3DTransform_ToMatrix and _ToMatrixAnim; also matrices of rank 2, 1 and 0.
def dcz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def dcget(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def dcset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
dcm = dcz(64)
dcr = dcz(64)
dcs = dcz(64)
dct = dcz(64)
def dcmul(a, b):
	dcset(dcr, a)
	dcset(dcs, b)
	SDTest.Call(0x0077b8, 'tiiiii', 0, dcm, dcr, dcs)
	return dcget(dcm, 16)
def dcrot(y, p, r):
	SDTest.Call(0x008701, 'tiiifff', 0, dcm, y, p, r)
	return dcget(dcm, 16)
def dcscale(x, y, z, tx, ty, tz):
	return (x, 0.0, 0.0, 0.0, 0.0, y, 0.0, 0.0, 0.0, 0.0, z, 0.0, tx, ty, tz, 1.0)
dcmats = []
for dcang in ((0.0, 0.0, 0.0), (0.3, -0.7, 1.1), (2.5, 0.1, -3.0), (1.5707963, 0.0, 0.0), (0.0, 3.14159265, 0.0)):
	dcrm = dcrot(dcang[0], dcang[1], dcang[2])
	for dcsc in ((1.0, 1.0, 1.0), (2.0, 2.0, 2.0), (1.0, 2.0, 3.0), (0.5, 0.5, 4.0), (3.0, 1.5, 3.0),
			(-1.0, 1.0, 1.0), (1.0, 1.0, -2.0), (-0.25, 7.0, 7.0)):
		dcmats.append(dcmul(dcscale(dcsc[0], dcsc[1], dcsc[2], 1.5, -20.0, 300.25), dcrm))
		dcmats.append(dcmul(dcrm, dcscale(dcsc[0], dcsc[1], dcsc[2], 0.0, 0.0, 0.0)))
dcmats = dcmats + [
	(1.0, 0.5, 0.0, 0.0, 0.0, 1.0, 0.25, 0.0, 0.3, 0.0, 1.0, 0.0, 5.0, 6.0, 7.0, 1.0),
	(0.1, 0.2, 0.3, 0.0, 0.4, 0.5, 0.6, 0.0, 0.7, 0.8, 0.95, 0.0, 0.0, 0.0, 0.0, 1.0),
	(1.0, 2.0, 3.0, 0.0, 2.0, 4.0, 6.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0),
	(1.0, 2.0, 3.0, 0.0, 2.0, 4.0, 6.0, 0.0, 3.0, 6.0, 9.0, 0.0, 1.0, 1.0, 1.0, 1.0),
	(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0),
	(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 2.0),
	(1e-3, 0.0, 0.0, 0.0, 0.0, 1e3, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0),
	(0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0),
	(-1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0),
]
for dck in range(len(dcmats)):
	dcset(dcm, dcmats[dck])
	check('Matrix_ToTransform' + fmt(dcmats[dck]), lambda: (SDTest.Call(0x2d6c50, 'vii', dcm, dct), dcget(dct, 15)))
	dcok = 1
	for dcx in dcget(dct, 15):
		if dcx != dcx:
			dcok = 0
	if not dcok:
		continue
	check('M3DTransform_ToMatrix ' + fmt(dck), lambda: (SDTest.Call(0x2d6750, 'tvii', dct, dcr), dcget(dcr, 16)))
	check('M3DTransform_ToMatrixAnim ' + fmt(dck), lambda: (SDTest.Call(0x2d65d0, 'tiii', dct, dcr)[0] != 0,
		dcget(dcr, 16)))
for dcv in ((1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 2.0, 2.0, 2.0, 1.0),
		(0.0, 0.0, 0.0, 0.1, 0.2, 0.3, 0.927, 0.5, -0.5, 0.5, 0.5, 1.0, 3.0, 0.5, -1.0)):
	dcset(dct, dcv)
	check('M3DTransform_ToMatrix' + fmt(dcv), lambda: (SDTest.Call(0x2d6750, 'tvii', dct, dcr), dcget(dcr, 16)))
SDTest.Call(0x133550, 'vi', dcm)
SDTest.Call(0x133550, 'vi', dcr)
SDTest.Call(0x133550, 'vi', dcs)
SDTest.Call(0x133550, 'vi', dct)
