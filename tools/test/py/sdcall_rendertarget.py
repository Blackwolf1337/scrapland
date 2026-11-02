# engine3d Engine/RenderTarget.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the pure helpers
# (pow2 size, the target names, the pixel compare), the bookkeeping of the scene targets on values set here, the
# effect switches, three materials made again and the Begin/End pairs of the NewsPanel, Credits and Scene targets
# with their D3D calls (vertices included) through the D3D log. The Credits trail draws without the time check
# (R_CreditsClear 1), motion blur and DuDv stay off (the clock and the models of the menu would change them).
# Not called: RenderTarget_VideoMemory (makes render targets until the device fails), InitD3D/ReleaseD3D and the
# caps checks (they remake the targets of the device).
def rti(a):
	return SDTest.Peek(a, '@i')
def rtpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def rtlog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r, SDTest.Out())
def rtsave(rvas):
	r = []
	for a in rvas:
		r.append(SDTest.Peek(a, 'i'))
	return r
def rtrestore(rvas, vals):
	for i in range(len(rvas)):
		SDTest.Poke(rvas[i], vals[i])

for rtn in (-1, 0, 1, 255, 256, 257, 511, 512, 513, 1024, 5000):
	check('RenderTarget_Pow2Size' + fmt((rtn,)), lambda rtn=rtn: SDTest.Call('RenderTarget_Pow2Size', 'ii', rtn)[0])
for rtname in ('Scene', 'NewsPanel', 'Credits'):
	check('RenderTarget_Mode' + fmt((rtname,)), lambda rtname=rtname: SDTest.Call('RenderTarget_Mode', 'iz', rtname)[0])
check('RenderTarget_Mode None', lambda: SDTest.Call('RenderTarget_Mode', 'iz', None)[0])

# two 16 x 16 images of 4 byte pixels (pitch 64 and 80), the second one changed in a few bytes
rta = SDTest.Call('Mem_Calloc', 'ii', 0x400)[0]
rtb = SDTest.Call('Mem_Calloc', 'ii', 0x500)[0]
for rti_ in range(0x100):
	rtpoke(rta + 4 * rti_, rti_ * 0x00010305 + 0x01020304)
for rty in range(16):
	for rtx in range(16):
		rtpoke(rtb + 80 * rty + 4 * rtx, SDTest.Peek(rta + 64 * rty + 4 * rtx, '@i'))
rtpoke(rtb + 80 * 3 + 4 * 5, SDTest.Peek(rta + 64 * 3 + 4 * 5, '@i') ^ 0x00000010)
rtpoke(rtb + 80 * 7 + 4 * 2, SDTest.Peek(rta + 64 * 7 + 4 * 2, '@i') ^ 0x00400000)
rtpoke(rtb + 80 * 9 + 4 * 9, SDTest.Peek(rta + 64 * 9 + 4 * 9, '@i') ^ 0x7f000000)
for rtxy in ((0, 0), (5, 3), (2, 7), (9, 9), (15, 15), (4, 1)):
	for rttol in (0, 1, 0x10, 0x20, 0x40, -1):
		check('RenderTarget_PixelEqual' + fmt(rtxy + (rttol,)), lambda rtxy=rtxy, rttol=rttol: SDTest.Call('RenderTarget_PixelEqual',
			'biiiiiiii', 64, rta, 80, rtb, rtxy[0], 4, rtxy[1], rttol)[0])

# the swap of source and target (texture and surface pointers set to numbers here, never used as objects)
rtscene = (sym('g_SceneSourceSurface'), sym('g_SceneTargetSurface'), sym('g_SceneSourceTexture'), sym('g_SceneTargetTexture'), sym('g_SceneRTargetSwap'))
rtsaved = tuple(rtsave(rtscene)) + (SDTest.PeekSpan('g_RTLinNonPow2', 0, 4),)
for rtswap in (0, 1):
	for rtnoswap in (0, 1):
		rtrestore(rtscene, (0x1111, 0x2222, 0x3333, 0x4444, rtswap))
		SDTest.PokeSpan('g_RTLinNonPow2', 0, (rtsaved[5] & ~0x00ff0000) | rtnoswap << 16, 4)
		check('Scene_Swap' + fmt((rtswap, rtnoswap)), lambda: (SDTest.Call('Scene_Swap', 'v')[0:0],
			rtsave(rtscene[:4]), SDTest.Peek('g_SceneNoSwap', 'b')))
# the target is the back buffer: a target surface equal to it is not released
rtback = rtsave((sym('g_RTBackBuffer'),))
rtrestore((sym('g_SceneTargetSurface'), sym('g_SceneTargetTexture')), (rtback[0], 0x5555))
check('Scene_TargetBackBuffer', lambda: (SDTest.Call('Scene_TargetBackBuffer', 'v')[0:0], SDTest.Peek('g_SceneTargetSurface', 'i') == rtback[0],
	SDTest.Peek('g_SceneTargetTexture', 'i')))
rtrestore(rtscene, rtsaved[:5])
SDTest.PokeSpan('g_RTLinNonPow2', 0, rtsaved[5], 4)

# the effects on as the device has them, and the targets of the device (only whether they were made)
for rtfx, rtrva in (('DuDv', sym('DuDv_IsOn')), ('Bloom', sym('Bloom_IsOn')), ('MotionBlur', sym('MotionBlur_IsOn')), ('RadialBlur', sym('RadialBlur_IsOn')),
		('Blur', sym('Blur_IsOn'))):
	check(rtfx + '_IsOn', lambda rtrva=rtrva: SDTest.Call(rtrva, 'b')[0])
check('RenderTarget targets', lambda: (SDTest.Peek('g_SceneOn', 'b'), SDTest.Peek('g_SceneRTarget', 'i'),
	SDTest.Call('NewsPanel_GetTarget', 'i')[0] != 0, SDTest.Peek('g_SceneRT1', 'i') != 0, SDTest.Peek('g_BloomTarget1', 'i') != 0,
	SDTest.Peek('g_CreditsTarget', 'i') != 0, SDTest.Peek('g_PSOn', 'i') != 0))

# three materials made again (the made one stays in the material list; SceneCopy is shared: the same material)
def rtmat(rva, glob):
	old = SDTest.Peek(glob, 'i')
	SDTest.Poke(glob, 0)
	SDTest.Call(rva, 'v')
	m = SDTest.Peek(glob, 'i')
	SDTest.Poke(glob, old)
	if not m:
		return None
	maps = []
	for i in (0, 3):
		mp = rti(m + 0x9c + 4 * i)
		if mp:
			maps.append((i, SDTest.Peek(mp, '@p'), SDTest.Peek(mp + 8, '@b'), SDTest.Peek(mp + 0xc, '@b')))
	return (m == old, SDTest.Peek(m + 4, '@p'), rti(m + 0x30), rti(m + 0x34), rti(m + 0x38), rti(m + 0x3c) & ~0x00ff0000,
		rti(m + 0x40), maps)
for rtname, rtrva, rtglob in (('CreditsMat', sym('RenderTarget_MakeCreditsMat'), sym('g_CreditsMat')), ('DuDvMatRTarget', sym('RenderTarget_MakeDuDvMatRTarget'), sym('g_SceneDuDvMatRTarget')),
		('SceneMatCopy', sym('RenderTarget_MakeSceneMatCopy'), sym('g_SceneMatCopy'))):
	check('RenderTarget_Make' + rtname, lambda rtrva=rtrva, rtglob=rtglob: rtmat(rtrva, rtglob))

# the Begin/End pairs, each only when no target is on
def rtpair(name):
	if SDTest.Peek('g_RTMode', 'i'):
		return 'busy'
	r = rtlog(lambda name=name: SDTest.Call('RenderTarget_Begin', 'bs', name)[0])
	return (r, rtlog(lambda name=name: SDTest.Call('RenderTarget_End', 'bs', name)[0]), SDTest.Peek('g_RTMode', 'i'))
check('RenderTarget NewsPanel', lambda: rtpair('NewsPanel'))
# Credits: the target made by Begin with R_CreditsRTarget 1, released again by Credits_Release (rva 0x220120)
rtcred = (sym('g_CreditsRTarget'), sym('g_CreditsAlpha'), sym('g_CreditsClear'), sym('g_CreditsPhase'))
rtcsaved = rtsave(rtcred)
for rtphase in (0, 1):
	rtrestore(rtcred, (1, 200, 1, rtphase))
	check('RenderTarget Credits' + fmt((rtphase,)), lambda: rtpair('Credits'))
SDTest.Call('Credits_Release', 'v')
rtrestore(rtcred, rtcsaved)
# Scene: bloom, radial blur and blur; DuDv off, motion blur value 0
rtfxv = (sym('g_SceneDuDvRTarget'), sym('g_SceneMotionBlurValue'), sym('g_SceneRadialBlurValue'), sym('g_SceneBlurValue'))
rtfsaved = rtsave(rtfxv)
for rtvals in ((0, 0, 0, 0), (0, 0, 0x3f000000, 0), (0, 0, 0, 0x3e800000), (0, 0, 0x3f400000, 0x3f800000)):
	rtrestore(rtfxv, rtvals)
	check('RenderTarget Scene' + fmt(tuple(rtvals)), lambda: rtpair('Scene'))
rtrestore(rtfxv, rtfsaved)
SDTest.Call('Mem_Free', 'vi', rta)
SDTest.Call('Mem_Free', 'vi', rtb)
