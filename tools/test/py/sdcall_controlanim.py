# engine3d Engine/ControlAnim.c: a CAnimCtrl in heap memory walking event lists made here, with and without a root
# node with a position track (Python 1.5.2, runs inside sdcall.py after the other checks: check, fmt come from
# there). Floats are printed as their bits. Not tested: the callback (no exe function fits it).
def cacalloc(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def cai(addr):
	return SDTest.Peek(addr, '@i')
def capoke(addr, v):
	SDTest.Poke(addr - 0x400000, v)
def caints(addr, n):
	r = []
	for i in range(n):
		r.append(cai(addr + 4 * i))
	return r
def cafloat(addr, v):
	SDTest.Call(0x3197d0, 'iiFi', addr, (v,), 4)
# the bits of a float
cabuf = cacalloc(4)
def cabits(v):
	cafloat(cabuf, v)
	return cai(cabuf)
# an event list from (t, id, arg) in t order, the head at the start; the events, the head
def caevents(evs):
	head = cacalloc(4)
	r = []
	for t, id, arg in evs:
		e = cacalloc(0x14)
		capoke(e, id)
		capoke(e + 4, arg)
		cafloat(e + 0xc, t)
		if r:
			capoke(r[-1] + 8, e)
		r.append(e)
	capoke(head, r[0])
	return r, head
def cawho(evs, p):
	for i in range(len(evs)):
		if evs[i] == p:
			return i
	if p == 0:
		return None
	return '?'
# the state that changes: move, ofs, saltos, paso, frame, state, relativo, escala, fade, lastPos, lastFrame, event
def caget(c, evs):
	return (caints(c + 0x14, 7), caints(c + 0x4c, 3), cai(c + 0x5c) & 0xff, caints(c + 0x60, 3), caints(c + 0x6c, 3),
		cai(c + 0x7c), cawho(evs, cai(c + 0x80)))

c = SDTest.Call(0x250e50, 'tii', cacalloc(0x84))[0]
check('CAnimCtrl_ctor', lambda: (caints(c, 0x10), caints(c + 0x4c, 6), caints(c + 0x80, 1)))
check('CAnimCtrl_Play', lambda: (SDTest.Call(0x250f20, 'tvi', c), cai(c + 0x54)))
check('CAnimCtrl_Pause', lambda: (SDTest.Call(0x2511b0, 'tvi', c), cai(c + 0x54)))
check('CAnimCtrl_Stop', lambda: (SDTest.Call(0x2511a0, 'tvi', c), cai(c + 0x54)))
check('CAnimCtrl_SetCallback', lambda: (SDTest.Call(0x250f30, 'tviiii', c, 0, 5, 6), caints(c + 0x40, 3)))
check('CAnimCtrl_SetFrames', lambda: (SDTest.Call(0x250fb0, 'tviii', c, 3, 40), caints(c + 0x34, 2)))
for s in range(4):
	check('AnimCtrl_StateName' + fmt((s,)), lambda s=s: SDTest.Call(0x250fd0, 'si', s)[0])
check('CAnimCtrl_Reset', lambda: (SDTest.Call(0x250f50, 'tiif', c, 7.5)[0], caget(c, [])))
for st in (0, 2, 2):
	capoke(c + 0x54, st)
	check('CAnimCtrl_Fade' + fmt((st,)), lambda: (SDTest.Call(0x2511c0, 'tvif', c, 0.5), caints(c + 0x54, 1), caints(c + 0x64, 2)))
check('CAnimCtrl_GetPosRot none', lambda: SDTest.Call(0x2510f0, 'tbifFF', c, 2.5, (9.0, 9.0, 9.0), (9.0, 9.0, 9.0, 9.0)))

# the events: frames 3..40, a motion and a scale event, a frame counter, a not taken conditional jump, a jump to
# frame 6 and the end
cond = cacalloc(4)
dest = cacalloc(0x14)
capoke(dest + 4, cond)
evs, head = caevents(((3.0, 0, 0), (5.0, 1, 1), (8.0, 4, cabits(2.0)), (10.0, 6, 0), (12.0, 11, dest), (14.0, 1, 0),
	(15.0, 10, cabits(6.0)), (40.0, 13, 0)))
capoke(c + 0x3c, head)
check('CAnimCtrl_Start', lambda: (SDTest.Call(0x251030, 'tvi', c), caget(c, evs)))
for i in range(30):
	check('CAnimCtrl_Update' + fmt((i,)), lambda: (SDTest.Call(0x251530, 'tbiif', c, 0, 0.1)[0], caget(c, evs)))
	if cai(c + 0x54) == 3:
		break
check('CAnimCtrl_Update stopped', lambda: (SDTest.Call(0x251530, 'tbiif', c, 0, 0.1)[0], caget(c, evs)))

# a root node with positions (and identity rotations) for 50 frames: the root motion, a taken jump, the fade and a rotation
n = cacalloc(0x24)
SDTest.Call(0x2ae2c0, 'tviiii', n, 50, 0, 3)
for i in range(0, 50, 16):
	k = min(16, 50 - i)
	SDTest.Call(0x3197d0, 'iiFi', cai(n + 0xc) + 16 * i, (0.0, 0.0, 0.0, 1.0) * k, 16 * k)
pos = []
for i in range(50):
	pos = pos + [0.5 * i, 0.25 * i * (i % 3), -0.125 * i]
SDTest.Call(0x3197d0, 'iiFi', cai(n + 8), tuple(pos[:60]), 240)
SDTest.Call(0x3197d0, 'iiFi', cai(n + 8) + 240, tuple(pos[60:120]), 240)
SDTest.Call(0x3197d0, 'iiFi', cai(n + 8) + 480, tuple(pos[120:]), 120)
capoke(c + 0x30, n)
SDTest.Call(0x250fb0, 'tviii', c, 0, 50)
for f in (0.0, 2.5, 7.25, 49.0):
	check('CAnimCtrl_GetPosRot' + fmt((f,)), lambda f=f: SDTest.Call(0x2510f0, 'tbifFF', c, f, (9.0, 9.0, 9.0), (9.0, 9.0, 9.0, 9.0)))
evs, head = caevents(((0.0, 0, 0), (4.0, 0, 0), (6.0, 1, 1), (9.0, 1, 0), (11.0, 12, 0), (30.0, 13, 0)))
capoke(evs[4] + 4, evs[1])
capoke(c + 0x3c, head)
SDTest.Call(0x251030, 'tvi', c)
for i in range(12):
	if i == 8:
		capoke(c + 0x54, 2)
		SDTest.Call(0x2511c0, 'tvif', c, 0.25)
	if i == 10:
		capoke(c + 0x10, 1)
	check('CAnimCtrl_Update root' + fmt((i,)), lambda i=i: (SDTest.Call(0x251530, 'tbiFf', c, (0.0, 0.5 * (i % 2), 0.0), 0.15)[0], caget(c, evs)))
check('CAnimCtrl_Update NULL rot', lambda: (SDTest.Call(0x251530, 'tbiif', c, 0, 0.05)[0], caget(c, evs)))
