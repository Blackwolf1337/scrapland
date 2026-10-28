# D3DX8 shader assembler (D3DX_AssembleShader, rva 0x00b2c2; execfile'd by sdcall.py, Python 1.5.2): every pixel shader
# of the game data assembled, the tokens of the code buffer and the text of the error buffer, then texts with errors.
# The buffers are read through their ID3DXBuffer vtable (GetBufferPointer +0xc, GetBufferSize +0x10, Release +8).
def asmvt(obj, off):
	return SDTest.Peek(SDTest.Peek(obj, '@i') + off, '@i') - 0x400000
def asmbuf(obj, text):
	if not obj:
		return None
	p = SDTest.Call(asmvt(obj, 0xc), 'tiii', 0, obj)[0]
	n = SDTest.Call(asmvt(obj, 0x10), 'tiii', 0, obj)[0]
	if text:
		r = SDTest.Peek(p, '@s')
	else:
		r = ()
		for i in range(0, n - n % 4, 4):
			r = r + (SDTest.Peek(p + i, '@i'),)
	SDTest.Call(asmvt(obj, 8), 'tiii', 0, obj)
	return n, r
def asmrun(data, size):
	r = SDTest.Call(0x00b2c2, 'tiiiiiipp', 0, data, size, 0, 0, 0, 0)
	return r[0], asmbuf(r[1], 0), asmbuf(r[2], 1)
def asmfile(name):
	r = SDTest.Call(0x21b330, 'vspp', name, 0, 0)
	res = asmrun(r[1], r[2])
	SDTest.Call(0x21b300, 'vp', r[1])
	return res
def asmtext(t):
	b = SDTest.Call(0x133650, 'ii', len(t) + 1)[0]
	SDTest.Call(0x3197d0, 'iisi', b, t, len(t) + 1)
	res = asmrun(b, len(t))
	SDTest.Call(0x133550, 'vi', b)
	return res
for asmname in ('lightmap', 'specular', 'GlowmapMaskEnvBump', 'MaskEnvBump', 'MaskEnvmap', 'GlowmapMaskEnvmap',
		'GlowmapMaskEnvmapLightmap', 'MaskEnvBumpLightmap', 'MaskEnvmapLightmap', 'CloudTest', 'Envmap',
		'EnvmapLightmap', 'Glowmap', 'GlowmapLightmap', 'hologram', 'GlowFlickLightmap', 'electric', 'fire', 'glass',
		'waves', 'noise', 'BloomFilter', 'BloomBlur', 'BloomTarget', 'MotionBlurAdd', 'MotionBlurTarget', 'BlurTarget',
		'DUDVFilter', 'DUDVTarget', 'RadialBlurTarget'):
	check('D3DX_AssembleShader ' + asmname, lambda n=asmname: asmfile('bmp/' + n + '.psh'))
for asmt in ('', 'ps.1.1', 'ps.1.1\n', 'ps.1.0\nmov r0, v0\n', 'ps.1.2\ntex t0\nmov r0, t0\n', 'ps.1.3\nmov r0, v0',
		'ps.1.4\nmov r0, v0\n', 'vs.1.1\nmov oPos, v0\n', 'vs.1.0\ndp4 oPos.x, v0, c0\n', 'ps.1.1\nmov r0, v0 // c\n',
		'ps.1.1\n; comment\nmov r0, v0\n', 'ps.1.1\n/* block\ncomment */ mov r0, v0\n', 'ps.1.1\nmov r0, v9\n',
		'ps.1.1\nfoo r0, v0\n', 'ps.1.1\nmov r0\n', 'ps.1.1\nmov r0, v0,\n', 'mov r0, v0\n', 'ps.1.1\ndef c0, 1, 2, 3\n',
		'ps.1.1\nmov r0, v0\ndef c0, 1, 2, 3, 4\n', 'ps.1.1\ndef c0, 1.5f, -2, 0.25e1, .5\nmov r0, c0\n',
		'ps.1.1\nmov r0.x, v0\n', 'ps.1.1\nmov r0.rgba, v0.r\n', 'ps.1.1\nmov r0, -v0_bx2\n', 'ps.1.1\nmov r0, 1-v0_bias\n',
		'ps.1.1\nmul_x2_sat r0, v0, v1\n', 'ps.1.1\nmul_sat r0, v0, v1\n', 'ps.1.1\nmul_d2 r0.rgb, v0, v1\n+add r0.a, v0, v1\n',
		'ps.1.1\n+mov r0.a, v0\n', 'ps.1.1\ntex t0\ntex t0\n', 'ps.1.1\ntexbem t1, t0\n', 'ps.1.1\ncnd r0, r0.a, v0, v1\n',
		'ps.1.1\nlrp r0, c0, v0, v1\n', 'ps.1.1\ndp4 r0, v0, v1\n', 'ps.1.1\nnop\nmov r0, v0\n', 'PS.1.1\nMOV R0, V0\n',
		'ps.1.1\r\nmov r0, v0\r\n', 'ps.1.1\n\tmov\tr0 ,  v0\n', 'ps.1.1\nmov r0, v0\nmov r1, r0\nmov r0, r1\n',
		'ps.1.1\nmov r0, v0 mov r1, v1\n', 'ps.1.1\nmov r9, v0\n', 'ps.1.1\nmov r0, c8\n', 'ps.1.1\n#define x\n',
		'ps.1.1\nmov r0, v0\n// last', 'ps.1.1\nsub r0, t0, t0.a\nadd r0, r0, c0\n', 'ps.1.1\nmad r0, t1, 1-r1.a, t0\n',
		'ps.1.1\ndp3_d2 r1, r0, c1\n', 'ps.1.1\ntexm3x2pad t1, t0_bx2\ntexm3x2tex t2, t0_bx2\n', 'ps.1.1\nmov r0, t2_bias.b\n'):
	check('D3DX_AssembleShader text' + fmt((asmt,)), lambda t=asmt: asmtext(t))
# flags (1 debug comments, 2 no validation) and the constant buffer of vertex shaders
def asmrunf(data, size, flags):
	r = SDTest.Call(0x00b2c2, 'tiiiiippp', 0, data, size, flags, 0, 0, 0)
	return r[0], asmbuf(r[1], 0), asmbuf(r[2], 0), asmbuf(r[3], 1)
def asmtextf(t, flags):
	b = SDTest.Call(0x133650, 'ii', len(t) + 1)[0]
	SDTest.Call(0x3197d0, 'iisi', b, t, len(t) + 1)
	res = asmrunf(b, len(t), flags)
	SDTest.Call(0x133550, 'vi', b)
	return res
for asmt in ('vs.1.1\ndef c3, 1.0, 2.5, -3, 4\nmov oPos, c[a0.x + 3]\n', 'vs.1.1\nm4x4 oPos, v0, c0\nmov oT0.xy, v7\n',
		'vs.1.1\nmov r0, c[2]\nmov r1, c[a0.x]\nmov r2, -c[1 + a0.x]\nsub r3, r0, r1\n', 'vs.1.1\nmov oD0, v0.wzyx\n',
		'vs.1.1\nmov r0, v0\n+mov r1, v1\n', 'vs.1.1\nmov r0, 1-v0\n', 'vs.0.0\nmov r0, v0\n', 'ps.2.0\nmov r0, v0\n',
		'ps.1.4\ntexld r0, t0\nphase\ntexcrd r1.rgb, t1\nbem r2.rg, c0, r1\nmov r0, -c0_bx2\n', 'ps.1.1\nmov r0.ag, v0\n',
		'ps.1.1\nmov r0, v0.rgbaa\n', 'ps.1.1\nmov r0, v0.q\n', 'ps.1.1\n#line 7 "a\\\\b.psh"\nmov r0, v9x\n',
		'ps.1.1\nmov r0, v0\n/* open', 'ps.1.1\nmov_x8 r0, v0\nmov_d8_sat r0, v0\nmov_foo r0, v0\n',
		'ps.1.1\ndef c1, -1.5, +2, -3, 4.e1\nmov r0, c1\n', 'ps.1.1\ndef r1, 1.0, 2.0, 3.0, 4.0\nmov r0, v0\n',
		'ps.1.1\nmov r0, -v0_dz\nmov r1, -v1_x2\nmov r2, --v2\n', 'ps.1.1\nmov r0, c[1]\n', 'ps.1.1 mov r0, v0 mov r1, v1'):
	for asmf in (0, 1, 2):
		check('D3DX_AssembleShader flags' + fmt((asmt, asmf)), lambda t=asmt, f=asmf: asmtextf(t, f))
