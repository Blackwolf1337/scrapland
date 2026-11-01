# The Sleep and busy wait rewrites (Engine.c, DXUtil.c, Tsc.c, Video.c; execfile'd by sdcall.py, Python 1.5.2). They
# differ from the original on purpose, so the checks of the new behavior run only in the hooked run (the first byte
# of the frame function is the jmp of the hook) and print the constant they expect in the original run. No timing
# values are printed. Not called: the frame with the limiter off (a render from inside the frame callback), the reset
# with g_ResetDev on, the end of the hold of Video_WaitKeysUp (no keys held in the test setup).
nslHooked = SDTest.Peek('CEngineApp_Render', 'b') == 0xe9
def nsli(a):
	return SDTest.Peek(a, '@i')

check('nsl Timer_CountTicks', lambda: (SDTest.Call('Timer_CountTicks', 'ii', 0)[0], SDTest.Call('Timer_CountTicks', 'ii', -5)[0]))

# Timer_GetQPFreq: the cached frequency (0x432590, -1 until the first call) times g_TimerQPMult (0x432588)
nslbuf = SDTest.Call('Mem_Calloc', 'ii', 8)[0]
nslmult = SDTest.Peek('g_TimerQPMult', 'i')
nslcache = (SDTest.Peek('g_TimerQPFreq', 'i'), SDTest.Peek(sym('g_TimerQPFreq') + 0x4, 'i'))
def nslqp(buf=nslbuf):
	r = SDTest.Call('Timer_GetQPFreq', 'ii', buf)[0]
	return (r, SDTest.Peek(buf, '@i'), SDTest.Peek(buf + 4, '@i'))
def nslqpreset():
	SDTest.Poke('g_TimerQPFreq', -1)
	SDTest.Poke(sym('g_TimerQPFreq') + 0x4, -1)
nslq0 = nslqp()
check('nsl Timer_GetQPFreq', lambda: (nslq0[0], (nslq0[1], nslq0[2]) == nslcache, nslq0[1] != 0))
nslqpreset()
check('nsl Timer_GetQPFreq again', lambda: nslqp()[0:3] == nslq0)
nslqpreset()
SDTest.Poke('g_TimerQPMult', 3 * nslmult)
check('nsl Timer_GetQPFreq mult', lambda: nslqp()[0:3] == (1, 3 * nslq0[1], 3 * nslq0[2]))
SDTest.Poke('g_TimerQPMult', nslmult)
SDTest.Poke('g_TimerQPFreq', nslcache[0])
SDTest.Poke(sym('g_TimerQPFreq') + 0x4, nslcache[1])
SDTest.Call('Mem_Free', 'vi', nslbuf)
nslqpf = 0
if nslmult:
	nslqpf = nslq0[1] / nslmult

# no measuring: the counter ticks of ms from QueryPerformanceFrequency
if nslHooked:
	check('nsl Timer_MeasureQP', lambda: (SDTest.Call('Timer_MeasureQP', 'ii', 100)[0] == nslqpf / 10,
		SDTest.Call('Timer_MeasureQP', 'ii', 0)[0], SDTest.Call('Timer_MeasureQP', 'ii', -3)[0]))
else:
	check('nsl Timer_MeasureQP', lambda: (1, 0, 0))


# Video_WaitKeysUp: no key is held, the original returns at once and the hold of the C ends at once
check('nsl Video_WaitKeysUp', lambda: (SDTest.Call('Video_WaitKeysUp', 'vi', 1000)[0], SDTest.Call('Video_WaitKeysUp', 'vi', 0)[0]))

# CEngineApp_ResetDevice without a pending reset (g_ResetDev 0x4a8efe)
nslmotor = SDTest.Peek('g_Engine', 'i')
if SDTest.Peek('g_ResetDev', 'b') == 0:
	check('nsl CEngineApp_ResetDevice off', lambda: SDTest.Call('CEngineApp_ResetDevice', 'tii', nslmotor)[0])

# CEngineApp_Render while the limiter holds the frame back (limitaFPS 30, the next frame at timeGetTime * 3 =
# 0xfffffff0): at once 0, no frame counted. Only without vsync (D3DPRESENT_PARAMETERS at 0x2a398 of the motor: 5
# SwapEffect, 7 Windowed, 12 FullScreen_PresentationInterval), else Render would draw a frame.
nslpp = nslmotor + 0x2a398
if nsli(nslpp + 28):
	nslNoVsync = nsli(nslpp + 20) != 4
else:
	nslNoVsync = nsli(nslpp + 48) == -0x7fffffff - 1
nslsave = (SDTest.Peek('g_LimitFps', 'i'), SDTest.Peek('g_LimitNext', 'i'), SDTest.Peek('g_FrameCount', 'i'))
if nslHooked and nslNoVsync:
	SDTest.Poke('g_LimitFps', 0x41f00000)
	SDTest.Poke('g_LimitNext', -16)
	check('nsl CEngineApp_Render held back', lambda: (SDTest.Call('CEngineApp_Render', 'tii', nslmotor)[0],
		SDTest.Peek('g_FrameCount', 'i') == nslsave[2], SDTest.Peek('g_LimitNext', 'i')))
	SDTest.Poke('g_LimitFps', nslsave[0])
	SDTest.Poke('g_LimitNext', nslsave[1])
else:
	check('nsl CEngineApp_Render held back', lambda: (0, 1, -16))
