# engine3d Engine/UtilD3D.c: the color conversion, the class names and the debug lines, the lines through the D3D log
# of SDTest (the device calls with their data). Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt
# come from there.
def udlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r[0], SDTest.Out())

for c in ((0.0, 0.0, 0.0, 0.0), (1.0, 0.5, 0.25, 1.0), (0.2, 0.4, 0.6, 0.8), (-0.5, 2.0, 1.002, 0.998),
	(0.501 / 255, 1.5 / 255, 2.5 / 255, 254.5 / 255), (-0.0, 300.0, -300.0, 0.5)):
	check('Color_FromFloat' + fmt(c), lambda c=c: SDTest.Call('Color_FromFloat', 'iF', c)[0])
for k in (0, 0xa1000001, 0xa1000002, 0xa2000004, 0xa3000008, 0xa4000010, 0xa5000020, 0xa6000040, 0xa6000080,
	0xa7000100, 0xa1000003, 5, -1):
	check('Obj3D_ClassName' + fmt((k,)), lambda k=k: SDTest.Call('Obj3D_ClassName', 'si', k)[0])

udm = (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 10.0, 20.0, 30.0, 1.0)
uda = (0.0, 1.0, 2.0)
udb = (3.0, -4.0, 5.5)
udz = (-0.0, 0.0, -0.0)
check('Debug_DrawLine', lambda: udlog(lambda: SDTest.Call('Debug_DrawLine', 'vFFi', uda, udb, 0xff00ff)))
check('Debug_DrawLineM', lambda: udlog(lambda: SDTest.Call('Debug_DrawLineM', 'vFFFi', udm, uda, udb, 0x123456)))
check('Debug_DrawBox', lambda: udlog(lambda: SDTest.Call('Debug_DrawBox', 'vFFi', (-1.0, -2.0, -3.0), (1.0, 2.0, 3.0), 0xff)))
check('Debug_DrawBoxM', lambda: udlog(lambda: SDTest.Call('Debug_DrawBoxM', 'vFFFi', udm, udz, udb, 0xff0000)))
check('Debug_DrawTri', lambda: udlog(lambda: SDTest.Call('Debug_DrawTri', 'vFFFi', uda, udb, udz, 0x8080)))
check('Debug_DrawTriM', lambda: udlog(lambda: SDTest.Call('Debug_DrawTriM', 'vFFFFi', udm, uda, udb, udz, 0x8080)))
for p, s in ((uda, 4.0), (udz, 1.5), (udb, -2.0)):
	check('Debug_DrawAxes' + fmt((p, s)), lambda p=p, s=s: udlog(lambda p=p, s=s: SDTest.Call('Debug_DrawAxes', 'vFFf', udm, p, s)))
	check('Debug_DrawPointM' + fmt((p, s)), lambda p=p, s=s: udlog(lambda p=p, s=s: SDTest.Call('Debug_DrawPointM', 'vFFfi', udm,
		p, s, 0xabcdef)))
	check('Debug_DrawPoint' + fmt((p, s)), lambda p=p, s=s: udlog(lambda p=p, s=s: SDTest.Call('Debug_DrawPoint', 'vFfi', p, s, 7)))
for l, a in ((5.0, 0.7), (1.0, 0.0), (-3.0, 3.1), (100.0, 1.5)):
	check('Debug_DrawConeM' + fmt((l, a)), lambda l=l, a=a: udlog(lambda l=l, a=a: SDTest.Call('Debug_DrawConeM', 'vFff', udm, l, a)))
for r, v, h in ((2.0, 8, -1), (1.0, 6, 5), (0.5, 4, 4), (3.0, 3, 2), (1.0, 2, 7), (0.0, 4, 4), (-1.0, 4, 4)):
	check('Debug_DrawSphereM' + fmt((r, v, h)), lambda r=r, v=v, h=h: udlog(lambda r=r, v=v, h=h: SDTest.Call('Debug_DrawSphereM',
		'vFFfiii', udm, udb, r, 0x404040, v, h)))
check('Debug_DrawSphere', lambda: udlog(lambda: SDTest.Call('Debug_DrawSphere', 'vFfiii', uda, 1.25, 0xffffff, 5, -1)))
