# The Sleep and busy wait rewrites (Motor.c, DXUtil.c, Tsc.c, Video.c; execfile'd by sdcall.py, Python 1.5.2). They
# differ from the original on purpose, so the checks of the new behavior run only in the hooked run (the first byte
# of the frame function is the jmp of the hook) and print the constant they expect in the original run. No timing
# values are printed. Not called: the frame with the limiter off (a render from inside the frame callback), the reset
# with g_ResetDev on, the end of the hold of Video_WaitKeysUp (no keys held in the test setup).
nslHooked = SDTest.Peek(0x22f940, 'b') == 0xe9
def nsli(a):
	return SDTest.Peek(a, '@i')

check('nsl Timer_CountTicks', lambda: (SDTest.Call(0x276ba0, 'ii', 0)[0], SDTest.Call(0x276ba0, 'ii', -5)[0]))

# Timer_GetQPFreq: the cached frequency (0x432590, -1 until the first call) times g_TimerQPMult (0x432588)
nslbuf = SDTest.Call(0x133650, 'ii', 8)[0]
nslmult = SDTest.Peek(0x432588, 'i')
nslcache = (SDTest.Peek(0x432590, 'i'), SDTest.Peek(0x432594, 'i'))
def nslqp(buf=nslbuf):
	r = SDTest.Call(0x276d00, 'ii', buf)[0]
	return (r, SDTest.Peek(buf, '@i'), SDTest.Peek(buf + 4, '@i'))
def nslqpreset():
	SDTest.Poke(0x432590, -1)
	SDTest.Poke(0x432594, -1)
nslq0 = nslqp()
check('nsl Timer_GetQPFreq', lambda: (nslq0[0], (nslq0[1], nslq0[2]) == nslcache, nslq0[1] != 0))
nslqpreset()
check('nsl Timer_GetQPFreq again', lambda: nslqp()[0:3] == nslq0)
nslqpreset()
SDTest.Poke(0x432588, 3 * nslmult)
check('nsl Timer_GetQPFreq mult', lambda: nslqp()[0:3] == (1, 3 * nslq0[1], 3 * nslq0[2]))
SDTest.Poke(0x432588, nslmult)
SDTest.Poke(0x432590, nslcache[0])
SDTest.Poke(0x432594, nslcache[1])
SDTest.Call(0x133550, 'vi', nslbuf)
nslqpf = 0
if nslmult:
	nslqpf = nslq0[1] / nslmult

# no measuring: the counter ticks of ms from QueryPerformanceFrequency
if nslHooked:
	check('nsl Timer_MeasureQP', lambda: (SDTest.Call(0x276e40, 'ii', 100)[0] == nslqpf / 10,
		SDTest.Call(0x276e40, 'ii', 0)[0], SDTest.Call(0x276e40, 'ii', -3)[0]))
else:
	check('nsl Timer_MeasureQP', lambda: (1, 0, 0))

# Tsc_CpuFreq: recalc starts the measurement and gives the frequency of the counter until 200 ms have passed
nslcpu = SDTest.Peek(0x4b1c00, 'i')
if nslHooked:
	check('nsl Tsc_CpuFreq', lambda: (SDTest.Call(0x272f90, 'fi', 1)[0] == float(nslqpf),
		SDTest.Call(0x272f90, 'fi', 0)[0] == float(nslqpf)))
else:
	check('nsl Tsc_CpuFreq', lambda: (1, 1))
SDTest.Poke(0x4b1c00, nslcpu)

# Video_WaitKeysUp: no key is held, the original returns at once and the hold of the C ends at once
check('nsl Video_WaitKeysUp', lambda: (SDTest.Call(0x280d10, 'vi', 1000)[0], SDTest.Call(0x280d10, 'vi', 0)[0]))

# CMotorApp_ResetDevice without a pending reset (g_ResetDev 0x4a8efe)
nslmotor = SDTest.Peek(0x4b1c04, 'i')
if SDTest.Peek(0x4a8efe, 'b') == 0:
	check('nsl CMotorApp_ResetDevice off', lambda: SDTest.Call(0x22fe60, 'tii', nslmotor)[0])

# CMotorApp_Render while the limiter holds the frame back (limitaFPS 30, the next frame at timeGetTime * 3 =
# 0xfffffff0): at once 0, no frame counted. Only without vsync (D3DPRESENT_PARAMETERS at 0x2a398 of the motor: 5
# SwapEffect, 7 Windowed, 12 FullScreen_PresentationInterval), else Render would draw a frame.
nslpp = nslmotor + 0x2a398
if nsli(nslpp + 28):
	nslNoVsync = nsli(nslpp + 20) != 4
else:
	nslNoVsync = nsli(nslpp + 48) == -0x7fffffff - 1
nslsave = (SDTest.Peek(0x4a8f8c, 'i'), SDTest.Peek(0x4a9048, 'i'), SDTest.Peek(0x4a8ec8, 'i'))
if nslHooked and nslNoVsync:
	SDTest.Poke(0x4a8f8c, 0x41f00000)
	SDTest.Poke(0x4a9048, -16)
	check('nsl CMotorApp_Render held back', lambda: (SDTest.Call(0x22f940, 'tii', nslmotor)[0],
		SDTest.Peek(0x4a8ec8, 'i') == nslsave[2], SDTest.Peek(0x4a9048, 'i')))
	SDTest.Poke(0x4a8f8c, nslsave[0])
	SDTest.Poke(0x4a9048, nslsave[1])
else:
	check('nsl CMotorApp_Render held back', lambda: (0, 1, -16))
