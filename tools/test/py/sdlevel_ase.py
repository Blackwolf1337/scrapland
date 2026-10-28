# The All-Seeing Eye server (src/lib/ase/Ase.c, execfile'd by sdlevel.py, Python 1.5.2): its answers to UDP queries
# on the loopback, sent and read through the Winsock thunks of the exe (stdcall: called as thiscall with a dummy
# this). The server starts without the internet flag (the original resolves the gone master servers then). The
# status comes from the game's callbacks in the level: the server name, type and map, the rules and the players.
# A second socket on the query port makes the server take the next port. The name of the server is set to a long
# text for one query (the strings are cut at 254 characters) and put back.
def asez(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def asefree(p):
	SDTest.Call(0x133550, 'vi', p)
def asepk(a):
	return SDTest.Peek(a, '@i')
def asepo(a, v):
	SDTest.Poke(a - 0x400000, v)
def asesock():
	s = SDTest.Call(0x20da08, 'tiiiii', 0, 2, 2, 0)[0]
	# non-blocking (FIONBIO), so a read without an answer returns at once
	SDTest.Call(0x20d9d8, 'tiiiip', 0, s, -2147195266, 1)
	return s
def aseclose(s):
	SDTest.Call(0x20d9d2, 'tiii', 0, s)
# a sockaddr_in of 127.0.0.1 and port
def aseaddr(port):
	a = asez(16)
	v = 2 | (port >> 8 & 0xff) << 16 | (port & 0xff) << 24
	asepo(a, v)
	asepo(a + 4, 0x0100007f)
	return a
def asebind(s, port):
	a = aseaddr(port)
	r = SDTest.Call(0x20d9cc, 'tiiiii', 0, s, a, 16)[0]
	asefree(a)
	return r
def asesend(s, port, text):
	a = aseaddr(port)
	r = SDTest.Call(0x20d9fc, 'tiiisiiii', 0, s, text, len(text), 0, a, 16)[0]
	asefree(a)
	return r
# the next packet on s: (length, bytes) or None
asebuf = asez(0x1000)
def aserecv(s):
	r = SDTest.Call(0x20d9f0, 'tiiiiiiii', 0, s, asebuf, 0x1000, 0, 0, 0)[0]
	if r <= 0:
		return None
	b = []
	for k in range(r):
		b.append(SDTest.Peek(asebuf + k, '@b'))
	return r, b
# the answer split into its strings where it has the format of an 's' answer
def asefields(b):
	if b[:4] != [69, 89, 69, 49]:
		return b
	r = ['EYE1']
	k = 4
	while k < len(b):
		n = b[k]
		if n == 0 or k + n > len(b):
			r.append(('rest', b[k:]))
			break
		t = ''
		for c in b[k + 1:k + n]:
			t = t + chr(c)
		r.append(t)
		k = k + n
	return r
# a query from s and the answer, if any, after one Ase_Check
def asequery(s, port, text):
	asesend(s, port, text)
	ok = SDTest.Call(0x0028f0, 'i')[0]
	a = aserecv(s)
	if a is None:
		return ok, None
	return ok, a[0], asefields(a[1])

aseport = 24444
aseq = aseport + 123
check('ase Ase_Check before init', lambda: (SDTest.Call(0x0028f0, 'i')[0], SDTest.Peek(0x43d030, 'p')))
check('ase Ase_Initialize', lambda: SDTest.Call(0x002410, 'iiii', aseport, 0, 0)[0])
ases = asesock()
check('ase client socket', lambda: ases != -1)
check('ase query s', lambda: asequery(ases, aseq, 's'))
check('ase query s again', lambda: asequery(ases, aseq, 's'))
check('ase query g', lambda: asequery(ases, aseq, 'g'))
check('ase query p', lambda: asequery(ases, aseq, 'p1234'))
check('ase query v', lambda: asequery(ases, aseq, 'v12345678'))
check('ase query x', lambda: asequery(ases, aseq, 'x'))
check('ase query sx', lambda: asequery(ases, aseq, 'sxyz'))
# two queries before one check
def asetwo():
	asesend(ases, aseq, 'g')
	asesend(ases, aseq, 's')
	ok = SDTest.Call(0x0028f0, 'i')[0]
	a = aserecv(ases)
	b = aserecv(ases)
	return ok, a is not None and a[0], b is not None and b[0], aserecv(ases)
check('ase two queries', asetwo)
# a long server name
asename = SDTest.Peek(0x463eec, 'i')
aselong = asez(400)
for k in range(0, 300, 4):
	asepo(aselong + k, 0x44434241 + (k / 4 % 20))
SDTest.Poke(0x463eec, aselong)
check('ase query s long name', lambda: asequery(ases, aseq, 's'))
SDTest.Poke(0x463eec, asename)
asefree(aselong)
check('ase Ase_Shutdown', lambda: (SDTest.Call(0x002070, 'v'), SDTest.Call(0x0028f0, 'i')[0],
	SDTest.Peek(0x43d030, 'p')))
check('ase query after shutdown', lambda: asequery(ases, aseq, 'g'))
# the query port taken: the next one
aseblock = asesock()
check('ase block the query port', lambda: asebind(aseblock, aseq))
check('ase Ase_Initialize taken', lambda: SDTest.Call(0x002410, 'iiii', aseport, 0, 0)[0])
check('ase query taken port', lambda: (asequery(ases, aseq, 'g'), aserecv(aseblock) is not None))
check('ase query next port', lambda: asequery(ases, aseq + 1, 'g'))
check('ase Ase_Initialize again', lambda: SDTest.Call(0x002410, 'iiii', aseport + 10, 0, 0)[0])
check('ase query new port', lambda: asequery(ases, aseq + 10, 'g'))
check('ase query old port', lambda: asequery(ases, aseq + 1, 'g'))
SDTest.Call(0x002070, 'v')
aseclose(aseblock)
aseclose(ases)
asefree(asebuf)
