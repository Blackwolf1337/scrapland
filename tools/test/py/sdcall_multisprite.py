# engine3d Engine/MultiSprite.c: the sprite tables of .mst files of the packs, their sizes and texture coordinates,
# a table by hand and the build without the index bitmap. Python 1.5.2, runs inside sdcall.py: check, fmt from there.
def msi(a):
	return SDTest.Peek(a, '@i')
def msnew():
	return SDTest.Call('CMultiSprite_ctor', 'tii', SDTest.Call('operator_new', 'ii', 0x10)[0])[0]
def msget(m):
	r = [msi(m), msi(m + 4), msi(m + 8)]
	p = msi(m + 0xc)
	for i in range(msi(m + 8)):
		r.append((msi(p + 8 * i), msi(p + 8 * i + 4)))
	return r
def msinfo(m, i):
	out = SDTest.Call('operator_new', 'ii', 16)[0]
	a = SDTest.Call('CMultiSprite_GetSize', 'tbiiii', m, i, out, out + 4)[0]
	b = SDTest.Call('CMultiSprite_GetUV', 'tbiiii', m, i, out + 8, out + 12)[0]
	return (a, msi(out), msi(out + 4), b, msi(out + 8), msi(out + 12))
for name in ('2d/gfx/motor.mst', '2d/fonts-drm/terminal8.alpha.mst', '2d/racer/target.alpha.mst', 'sdnone/x.mst'):
	m = msnew()
	check('CMultiSprite_Load' + fmt((name,)), lambda m=m, name=name: (SDTest.Call('CMultiSprite_Load', 'tbis', m, name)[0], msget(m)))
	n = msi(m + 8)
	for i in (0, 1, n / 2, n - 1, n):
		if i < n or i == 0:
			check('CMultiSprite_GetSize/GetUV' + fmt((name, i)), lambda m=m, i=i: msinfo(m, i))
	SDTest.Call('CMultiSprite_dtor', 'tvi', m)
	check('CMultiSprite_dtor' + fmt((name,)), lambda m=m: msget(m))
m = msnew()
check('CMultiSprite_BuildTable none', lambda m=m: (SDTest.Call('CMultiSprite_BuildTable', 'tbis', m, 'sdnone/x.tga')[0], msget(m)))
SDTest.Call('CMultiSprite_dtor', 'tvi', m)
