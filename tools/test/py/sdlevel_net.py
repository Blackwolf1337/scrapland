# game Net/ (execfile'd by sdlevel.py, Python 1.5.2): the parts of the network code that need no socket, in the
# single player level. The random numbers of the nonces (MT19937-64 with the shuffle table), Poly1305, ChaCha20, the
# byte buffer and the CRC16 on heap buffers; the message queue of a cNetClient (push, ack, receive of crafted
# packets, the time outs) with the transport in mode 0 (sends go nowhere); the resource names, the "net<n>" names and
# the descriptor streams through a cFileMem; the client table of the server, the transport address and ping tables and
# the early returns of everything that would open a socket, all on globals poked for the test and put back. Never
# called: the inits of server, client and browser (sockets), Transport_RecvFrom/SendTo, the frame of the server
# (CHECKs a server), the ASE queries and the parts that create ships.
def netz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def netnew(n):
	return SDTest.Call('operator_new', 'ii', n)[0]
def neti(a):
	return SDTest.Peek(a, '@i')
def netput(a, v):
	SDTest.Poke(a - 0x400000, v)
def netw(a, n):
	r = []
	for k in range(n):
		r.append(neti(a + 4 * k))
	return tuple(r)
def netb(a, n):
	r = []
	for k in range(n):
		r.append(SDTest.Peek(a + k, '@b'))
	return tuple(r)
def netword(b0, b1, b2, b3):
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)
# bytes into memory in whole words (up to 3 zero bytes after them)
def netsetb(a, l):
	l = list(l) + [0, 0, 0]
	k = 0
	while k + 4 <= len(l):
		netput(a + k, netword(l[k], l[k + 1], l[k + 2], l[k + 3]))
		k = k + 4
def netstr(s):
	r = []
	for ch in s:
		r.append(ord(ch))
	return r + [0]
def netpat(n, seed):
	r = []
	for k in range(n):
		r.append((seed * 37 + k * 13 + (k * k) % 7) & 0xff)
	return r
def netrange(n):
	r = []
	for k in range(n):
		r.append(k)
	return r

netapp = SDTest.Peek('g_pApp', 'i')
netflags = neti(netapp + 0x30)
netmode = SDTest.Peek('g_NetMode', 'i')
check('net state', lambda: (netflags, netmode, SDTest.Peek('g_NetMaxClients', 'i'), SDTest.Peek('g_NetNumClients', 'i'),
	SDTest.Peek('g_NetMasterOn', 'i'), SDTest.Peek('g_NetServerPort', 'i') & 0xffff, SDTest.Peek('g_NetUserFactory', 'i') == 0,
	SDTest.Peek('g_NetClients', 'i') == 0))

# the random numbers: a fresh twister seeds itself with 5489 on the first draw
netr = netz(0x11cc)
def netrstate(r):
	return (neti(r + 0x800), neti(r + 0x804), neti(r + 0x11c8), netw(r, 6), netw(r + 0x808, 4), netw(r + 0x11c0, 2))
def netnext(r, n, rva):
	l = []
	for k in range(n):
		l.append(SDTest.Call(rva, 'tii', r)[0])
	return l
check('cNetRandom_ctor', lambda: (SDTest.Call('cNetRandom_ctor', 'tii', netr)[0] == netr, neti(netr + 0x11c8)))
check('cNetRandom_Next unseeded', lambda: (netnext(netr, 6, sym('cNetRandom_Next')), netrstate(netr)))
for netseed in ((0x12345678, 0x9abc), (0, 0), (-1, -1), (5489, 0)):
	check('cNetRandom_Seed' + fmt(netseed), lambda s=netseed: (SDTest.Call('cNetRandom_Seed', 'tviii', netr, s[0], s[1]),
		netrstate(netr), netnext(netr, 400, sym('cNetRandom_Next')), netrstate(netr)))
check('cNetRandom_Next thunk', lambda: (netnext(netr, 5, sym('cNetRandom_Next@190360')), netrstate(netr)))
check('cNetRandom_Fill', lambda: (SDTest.Call('cNetRandom_Fill', 'tvi', netr), netrstate(netr), netnext(netr, 3, sym('cNetRandom_Next'))))
netkeys = netz(400 * 8)
for netk in range(800):
	netput(netkeys + 4 * netk, netword(netk & 0xff, (netk * 7) & 0xff, (netk * 13) & 0xff, (netk * 29) & 0xff))
for netlen in (1, 4, 312, 400):
	check('cNetRandom_InitByArray' + fmt((netlen,)), lambda n=netlen: (SDTest.Call('cNetRandom_InitByArray', 'tviii', netr, netkeys,
		n), netrstate(netr), netnext(netr, 20, sym('cNetRandom_Next'))))
# seeded from GetTickCount64: only the counters are the same in both runs
check('cNetRandom_ctorSeeded', lambda: (SDTest.Call('cNetRandom_ctorSeeded', 'tii', netr)[0] == netr, neti(netr + 0x800),
	neti(netr + 0x804), neti(netr + 0x11c8)))

# Poly1305 (the RFC 8439 vector first)
netkey = netz(0x40)
netmsg = netz(0x100)
netmac = netz(0x20)
def netpoly(key, msg, n):
	netsetb(netkey, key)
	netsetb(netmsg, msg)
	SDTest.Call('Poly1305_Auth', 'viiii', netmac, netmsg, n, netkey)
	return netb(netmac, 16)
netrfckey = (0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33, 0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8, 0x01,
	0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd, 0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b)
check('Poly1305_Auth RFC 8439', lambda: netpoly(netrfckey, netstr('Cryptographic Forum Research Group'), 34))
for netn in (0, 1, 15, 16, 17, 31, 32, 47, 64, 80, 0xb0):
	for netseed in (1, 2):
		check('Poly1305_Auth' + fmt((netn, netseed)), lambda n=netn, s=netseed: netpoly(netpat(32, s),
			netpat(0xb0, s + 5), n))
check('Poly1305_Auth ff', lambda: netpoly([0xff] * 32, [0xff] * 0xb0, 0xb0))
check('Poly1305_Auth ff 33', lambda: netpoly([0xff] * 32, [0xff] * 0xb0, 33))
check('Poly1305_Auth 0 key', lambda: netpoly([0] * 32, [0xff] * 0xb0, 0x40))

# ChaCha20 (the RFC 8439 block: key 0..31, nonce 0:0:0:9 0:0:0:4a 0:0:0:0, counter 1)
netcc = netz(0x40)
netnonce = netz(0x10)
netout = netz(0x200)
netin = netz(0x200)
netx = netz(0x40)
netsetb(netkey, netrange(32))
netsetb(netnonce, (0, 0, 0, 9, 0, 0, 0, 0x4a, 0, 0, 0, 0))
netsetb(netin, netpat(0x180, 3))
check('cChaCha_ctor', lambda: SDTest.Call('cChaCha_ctor', 'tii', netcc)[0] == netcc)
check('cChaCha_Init', lambda: (SDTest.Call('cChaCha_Init', 'tviiii', netcc, netkey, 1, netnonce), netw(netcc, 16)))
for netrounds in (20, 1, 2, 8, 0, -3):
	check('cChaCha_Block' + fmt((netrounds,)), lambda n=netrounds: (SDTest.Call('cChaCha_Block', 'tviiii', netcc, netcc,
		netout, n), netw(netout, 16)))
for netlen in (0, 1, 63, 64, 100, 130):
	check('cChaCha_Crypt' + fmt((netlen,)), lambda n=netlen: (SDTest.Call('cChaCha_Crypt', 'tviiiiiii', netcc, netkey, 7,
		netnonce, netin, netout, n), netb(netout, n), netw(netcc, 16)))
check('cChaCha_Crypt in place', lambda: (SDTest.Call('cChaCha_Crypt', 'tviiiiiii', netcc, netkey, 0, netnonce, netin,
	netin, 70), netb(netin, 72)))
for netqr in ((0, 4, 8, 12), (3, 4, 9, 14), (0, 0, 1, 2), (5, 5, 5, 5), (1, 2, 1, 2)):
	netsetb(netx, netpat(64, netqr[1]))
	check('cChaCha_QuarterRound' + fmt(netqr), lambda q=netqr: (SDTest.Call('cChaCha_QuarterRound', 'tviiiiii', netcc, netx, q[0],
		q[1], q[2], q[3]), netw(netx, 16)))
for netrot in ((0x12345678, 0), (0x12345678, 1), (-0x7fffffff, 7), (0x80, 31), (0x1234, 32), (0x1234, 33),
	(0x1234, -1)):
	check('cChaCha_Rotl' + fmt(netrot), lambda r=netrot: SDTest.Call('cChaCha_Rotl', 'tiiii', netcc, r[0], r[1])[0])
check('cChaCha_Store', lambda: (SDTest.Call('cChaCha_Store', 'tviii', netcc, netx, netout), netb(netout, 64)))
check('cChaCha_StoreU32', lambda: (SDTest.Call('cChaCha_StoreU32', 'tviii', netcc, -0x12345679, netout + 1), netb(netout, 6)))
check('cChaCha_LoadU32', lambda: (SDTest.Call('cChaCha_LoadU32', 'tiii', netcc, netout + 1)[0], SDTest.Call('cChaCha_LoadU32', 'tiii',
	netcc, netin + 3)[0]))

# the byte buffer, its capacity grows by whole multiples of itself
netnb = netz(0x10)
netnb2 = netz(0x10)
def netnbs(nb):
	return (neti(nb), neti(nb + 4), netb(neti(nb + 8), neti(nb)))
check('cNetBuffer_ctor', lambda: (SDTest.Call('cNetBuffer_ctor', 'tiii', netnb, 4)[0] == netnb, netnbs(netnb)))
check('cNetBuffer_WriteU8', lambda: (SDTest.Call('cNetBuffer_WriteU8', 'tiii', netnb, 0x1ab)[0], netnbs(netnb)))
check('cNetBuffer_WriteU16', lambda: (SDTest.Call('cNetBuffer_WriteU16', 'tiii', netnb, 0x12345)[0], netnbs(netnb)))
check('cNetBuffer_WriteU32', lambda: (SDTest.Call('cNetBuffer_WriteU32', 'tiii', netnb, -0x789abcdf)[0], netnbs(netnb)))
check('cNetBuffer_WriteU64', lambda: (SDTest.Call('cNetBuffer_WriteU64', 'tiiii', netnb, 0x11223344, -0x55667788)[0],
	netnbs(netnb)))
check('cNetBuffer_Append', lambda: (SDTest.Call('cNetBuffer_Append', 'tiiii', netnb, netin, 9)[0], netnbs(netnb)))
check('cNetBuffer_AppendPad16', lambda: (SDTest.Call('cNetBuffer_AppendPad16', 'tiiii', netnb, netin + 5, 21)[0], netnbs(netnb)))
check('cNetBuffer_AppendPad16 16', lambda: (SDTest.Call('cNetBuffer_AppendPad16', 'tiiii', netnb, netin, 16)[0], neti(netnb),
	neti(netnb + 4)))
check('cNetBuffer_ctor 2', lambda: (SDTest.Call('cNetBuffer_ctor', 'tiii', netnb2, 0x10)[0] == netnb2,
	SDTest.Call('cNetBuffer_Append', 'tiiii', netnb2, netin + 40, 7)[0], netnbs(netnb2)))
check('cNetBuffer_AppendBuffer', lambda: (SDTest.Call('cNetBuffer_AppendBuffer', 'tiii', netnb, netnb2)[0], netnbs(netnb)))
check('cNetBuffer_AppendBufferPad16', lambda: (SDTest.Call('cNetBuffer_AppendBufferPad16', 'tiii', netnb, netnb2)[0], netnbs(netnb)))
check('cNetBuffer_Reserve', lambda: (SDTest.Call('cNetBuffer_Reserve', 'tvii', netnb, 0x200), netnbs(netnb)))
check('cNetBuffer_Data Size', lambda: (SDTest.Call('cNetBuffer_Data', 'tii', netnb)[0] == neti(netnb + 8),
	SDTest.Call('cNetBuffer_Size', 'tii', netnb)[0]))
check('cNetBuffer_dtor', lambda: (SDTest.Call('cNetBuffer_dtor', 'tvi', netnb), SDTest.Call('cNetBuffer_dtor', 'tvi', netnb2),
	netw(netnb, 3), netw(netnb2, 3)))

# CRC16 of the transport (its table at rva 0x3c6c68)
for netn in (0, 1, 5, 64, 0x180):
	check('Transport_Crc16' + fmt((netn,)), lambda n=netn: SDTest.Call('Transport_Crc16', 'iii', netin, n)[0] & 0xffff)

# the message queue of one connection: no user manager (the factory is 0 in single player)
def netcs(c):
	return (neti(c) != 0, neti(c + 4) != 0, SDTest.Peek(c + 8, '@b'), SDTest.Peek(c + 9, '@b'), netw(c + 0xc, 6),
		SDTest.Peek(c + 0x24, '@b'), neti(c + 0x28))
def netq(c):
	r = []
	n = neti(c)
	while n and len(r) < 40:
		sz = neti(n) & 0xffff
		r.append((sz, netb(neti(n + 4), sz)))
		n = neti(n + 8)
	return r
netdat = netz(0x100)
netctl = netz(0x100)
netpkt = netz(0x100)
netsz = netz(0x10)
def netrecv(c, t, pkt, ds, cs):
	netsetb(netpkt, pkt)
	netsetb(netdat, [0xee] * 16)
	netsetb(netctl, [0xdd] * 16)
	r = SDTest.Call('cNetClient_Receive', 'tiiiiiipip', c, t, netpkt, len(pkt), netdat, ds, netctl, cs)
	return (r, netb(netdat, 8), netb(netctl, 8), netcs(c), netq(c))
if SDTest.Peek('g_NetUserFactory', 'i') == 0:
	netc = netnew(0x2c)
	check('cNetClient_ctor', lambda: (SDTest.Call('cNetClient_ctor', 'tiii', netc, -1)[0] == netc, netcs(netc)))
	netsetb(netmsg, netstr('abcdef'))
	check('cNetClient_Push', lambda: (SDTest.Call('cNetClient_Push', 'tviii', netc, netmsg, 3),
		SDTest.Call('cNetClient_Push', 'tviii', netc, netmsg + 2, 2), SDTest.Call('cNetClient_Push', 'tviii', netc, netmsg, 0x10003),
		netcs(netc), netq(netc)))
	check('cNetClient_GetSeq', lambda: SDTest.Call('cNetClient_GetSeq', 'tvipp', netc, 0, 0))
	netnode = SDTest.Call('cNetClient_NewMsg', 'tiiii', netc, netmsg + 1, 4)[0]
	check('cNetClient_NewMsg', lambda: (neti(netnode) & 0xffff, netb(neti(netnode + 4), 5), neti(netnode + 8),
		netcs(netc)))
	SDTest.Call('Mem_Free', 'ii', neti(netnode + 4))
	SDTest.Call('Mem_Free', 'ii', netnode)
	netsetb(netdat, netpat(16, 9))
	check('cNetClient_Send mode 0', lambda: (SDTest.Call('cNetClient_Send', 'tiiiiii', netc, 1000, -1, 4, netdat)[0],
		netcs(netc)))
	for nett in (1000, 4000, 6000, 8000):
		check('cNetClient_UpdateState' + fmt((nett,)), lambda t=nett: (SDTest.Call('cNetClient_UpdateState', 'tiii', netc, 1000 + t)[0],
			netcs(netc)))
	netput(netc + 0x10, 0)
	for nett in (1000, 1050, 1099, 1100, 1500):
		check('cNetClient_TimeToSend' + fmt((nett,)), lambda t=nett: SDTest.Call('cNetClient_TimeToSend', 'tiii', netc, t)[0])
	check('cNetClient_Flush', lambda: (SDTest.Call('cNetClient_Flush', 'tiiii', netc, 1050, 0)[0], netcs(netc),
		SDTest.Call('cNetClient_Flush', 'tiiii', netc, 1300, 0)[0], netcs(netc)))
	# acknowledges seq 0, brings message 0 (kind 9) and 3 bytes of data
	check('cNetClient_Receive ack', lambda: netrecv(netc, 1400, (0x00, 2, 0, 0, 9, 0x78, 0x79, 0x7a), 100, 100))
	check('cNetClient_Receive toggle', lambda: netrecv(netc, 1450, (0x81, 1, 0, 0x55, 0x56), 100, 100))
	check('cNetClient_Receive old seq', lambda: netrecv(netc, 1500, (0x02, 3, 0, 0, 4, 5, 6, 7), 2, 100))
	check('cNetClient_Receive next seq', lambda: netrecv(netc, 1550, (0x7f, 4, 0, 1, 4, 5, 6, 7, 8), 100, 2))
	check('cNetClient_Receive goodbye', lambda: netrecv(netc, 1600, (0x7f, 2, 0, 2, 2), 100, 100))
	check('cNetClient_Goodbye', lambda: (SDTest.Call('cNetClient_Goodbye', 'tvi', netc), netcs(netc), netq(netc)))
	check('cNetClient_UpdateState closing', lambda: (SDTest.Call('cNetClient_UpdateState', 'tiii', netc, 2000)[0],
		SDTest.Call('cNetClient_UpdateState', 'tiii', netc, 4000)[0]))
	# RequestFull CHECKs the client mode
	SDTest.Poke('g_NetMode', 2)
	check('cNetClient_RequestFull', lambda: (SDTest.Call('cNetClient_RequestFull', 'tvi', netc), netq(netc),
		SDTest.Call('cNetClient_RequestFull', 'tvi', netc), len(netq(netc))))
	SDTest.Poke('g_NetMode', netmode)
	check('cNetClient_ClientRecv mode 0', lambda: SDTest.Call('cNetClient_ClientRecv', 'tiiipipip', netc, 3000, 7, netdat, 100,
		netctl, 100))
	check('cNetClient_dtor', lambda: (SDTest.Call('cNetClient_dtor', 'tvi', netc), neti(netc), neti(netc + 4)))
netsaved = map(lambda a: SDTest.Peek(a, 'i'), (sym('g_NetCloseTime'), sym('g_NetUserFactory'), sym('g_NetSendDelay'), sym('g_NetWaitTime')))
check('msgman times', lambda: netsaved)
for netset in ((sym('MsgMan_SetCloseTime'), sym('g_NetCloseTime')), (sym('MsgMan_SetSendDelay'), sym('g_NetSendDelay')), (sym('MsgMan_SetWaitTime'), sym('g_NetWaitTime'))):
	check('MsgMan_Set' + fmt(netset), lambda s=netset: (SDTest.Call(s[0], 'vi', 1234), SDTest.Peek(s[1], 'i')))
SDTest.Call('MsgMan_SetCloseTime', 'vi', netsaved[0])
SDTest.Call('MsgMan_SetSendDelay', 'vi', netsaved[2])
SDTest.Call('MsgMan_SetWaitTime', 'vi', netsaved[3])
check('MsgMan_SetUserFactory', lambda: (SDTest.Call('MsgMan_SetUserFactory', 'vi', netsaved[1]), SDTest.Peek('g_NetUserFactory', 'i') ==
	netsaved[1]))

# the user manager base on fake objects
netu = netz(0x10)
netcl = netz(0x30)
check('cUserNetManager_ctor', lambda: (SDTest.Call('cUserNetManager_ctor', 'tiiii', netu, netcl, 5)[0] == netu,
	neti(netu) == symva('cUserNetManager_vftable'), neti(netu + 4), neti(netu + 8) == netcl))
netput(netcl + 0x28, netu)
check('cUserNetManager_dtor', lambda: (SDTest.Call('cUserNetManager_dtor', 'tvi', netu), neti(netcl + 0x28), neti(netu) == symva('cUserNetManager_vftable')))
netput(netcl + 0x28, netu)
check('cUserNetManager_Delete', lambda: (SDTest.Call('cUserNetManager_Delete', 'tiii', netu, 0)[0] == netu, neti(netcl + 0x28)))
check('cUserNetManager_Nop', lambda: SDTest.Call('cUserNetManager_Nop', 'tvi', netu))

# NetData: resources (a client flag poked for the table), names, the descriptor streams
netf = netz(0x20)
netfbuf = netz(0x400)
SDTest.Call('cFileMem_ctor', 'tiiiii', netf, netfbuf, 0x400, 0)
def netfbytes(f):
	return netb(netfbuf, SDTest.Call('cFileMem_Size', 'tii', f)[0])
def netres(n):
	r = []
	for k in range(n):
		r.append(SDTest.Call('Net_GetResourceName', 'si', k)[0])
	return r
netgvflags = SDTest.Peek(sym('g_GVarServerType') + 0xc, 'i')
check('Net_GetResourceName single player', lambda: netres(2))
if netflags == 0:
	netput(netapp + 0x30, 1)
	check('Net_GetResourceName client', lambda: netres(3))
	check('Net_AddResource', lambda: (SDTest.Call('Net_AddResource', 'is', 'NetTestA')[0], SDTest.Call('Net_AddResource', 'is',
		'NetTestB')[0], SDTest.Call('Net_AddResource', 'is', 'nettesta')[0], SDTest.Call('Net_AddResource', 'is', 'x' * 31)[0], netres(5)))
	netput(netapp + 0x30, 3)
	check('Net_AddResource server', lambda: (SDTest.Call('Net_AddResource', 'is', 'NetTestC')[0], netres(5)))
	netput(netapp + 0x30, 1)
	SDTest.Call('cFileMem_Open', 'tiisi', netf, '', 6)
	check('NetData_WriteResources', lambda: (SDTest.Call('NetData_WriteResources', 'viii', netf, 0, 6), netfbytes(netf)))
	if not SDTest.Peek(sym('g_ClientAddRes') + 0x10, 'b') & 3 and not SDTest.Peek(sym('g_ClientDelRes') + 0x10, 'b') & 3:
		netsetb(netfbuf, [1, 0, 3, 5, 0, 0, 0, 0x52, 0x65, 0x73, 0x31, 0x32, 0, 0, 0, 0, 2, 0, 0, 0, 0x5a, 0x7a])
		SDTest.Call('cFileMem_SetBuffer', 'tviiiii', netf, netfbuf, 0x400, 0, 0)
		SDTest.Call('cFileMem_Open', 'tiisi', netf, '', 1)
		check('NetData_ReadResources', lambda: (SDTest.Call('NetData_ReadResources', 'vi', netf), netres(6)))
	check('Net_Done', lambda: (SDTest.Call('Net_Done', 'v'), netres(6)))
	SDTest.Poke(sym('g_GVarServerType') + 0xc, netgvflags)
	netput(netapp + 0x30, netflags)

netbot = SDTest.Peek('g_NetBotCounter', 'i')
netobj = SDTest.Peek('g_NetObjCounter', 'i')
check('net name counters', lambda: (netbot, netobj))
check('Net_GetBotName', lambda: (SDTest.Call('Net_GetBotName', 'vb', ''), SDTest.Call('Net_GetBotName', 'vb', ''),
	SDTest.Peek('g_NetBotCounter', 'i')))
SDTest.Poke('g_NetBotCounter', 0x201)
check('Net_GetBotName wrap', lambda: (SDTest.Call('Net_GetBotName', 'vb', ''), SDTest.Peek('g_NetBotCounter', 'i')))
check('Net_GetObjName', lambda: (SDTest.Call('Net_GetObjName', 'vb', ''), SDTest.Peek('g_NetObjCounter', 'i')))
SDTest.Poke('g_NetObjCounter', 32001)
check('Net_GetObjName wrap', lambda: (SDTest.Call('Net_GetObjName', 'vb', ''), SDTest.Peek('g_NetObjCounter', 'i')))
SDTest.Poke('g_NetBotCounter', netbot)
SDTest.Poke('g_NetObjCounter', netobj)
for netname in ('net0', 'net12', 'net300', 'net', 'nex5', 'Net5', 'netabc', 'net-3', 'net00042x', ''):
	check('Net names' + fmt((netname,)), lambda n=netname: (SDTest.Call('Net_GetPlayerIndex', 'is', n)[0],
		SDTest.Call('Net_GetNetIndex', 'bsp', n, 99), SDTest.Call('Net_IsPlayerName', 'bs', n)[0]))
# a kick of "net0" would read the empty client table of single player
for netname in ('x', 'net12', 'net300', 'net-3'):
	check('Net_Kick' + fmt((netname,)), lambda n=netname: SDTest.Call('Net_Kick', 'bs', n)[0])
nete = netz(0x80)
netename = netz(0x20)
netput(nete + 4, netename)
for netname in ('net7', 'nec7', 'net'):
	netsetb(netename, netstr(netname))
	check('Net_IsNetEntity' + fmt((netname,)), lambda: SDTest.Call('Net_IsNetEntity', 'bi', nete)[0])
	if netflags == 0:
		netput(netapp + 0x30, 1)
		check('Net_IsNetEntity client' + fmt((netname,)), lambda: SDTest.Call('Net_IsNetEntity', 'bi', nete)[0])
		netput(netapp + 0x30, netflags)
netpings = SDTest.Peek('g_NetClientPings', 'i')
SDTest.Poke('g_NetClientPings', netword(0, 24, 120, 255))
for netname in ('net0', 'net1', 'net2', 'net3', 'net600', 'x'):
	check('Net_GetPing' + fmt((netname,)), lambda n=netname: SDTest.Call('Net_GetPing', 'is', n)[0])
for netn in (0, 1, 2, 3, 0x100, -1):
	check('Net_GetClientPing' + fmt((netn,)), lambda n=netn: SDTest.Call('Net_GetClientPing', 'ii', n)[0])
SDTest.Poke('g_NetClientPings', netpings)
check('Net_Send strings mode 0', lambda: (SDTest.Call('Net_SendChatString', 'bsi', 'hi', -1)[0], SDTest.Call('Net_SendUsrString', 'bsi',
	'hi', 3)[0], SDTest.Call('Net_SendString', 'bsii', 'yo', 0, 1)[0], SDTest.Call('Net_SendCtrlString', 'bs', 'cmd')[0]))
check('Net_Update mode 0', lambda: SDTest.Call('Net_Update', 'v'))
netinfo = netz(0x60)
netsetb(netinfo, (0xba, 0xce, 2, 1))
check('NetData_OnBrowse other version', lambda: SDTest.Call('NetData_OnBrowse', 'vsiii', '1.2.3.4', 99, netinfo, 7))
# the descriptors: no "net" vehicles in the level, so the counts stay 0
networld = SDTest.Peek('g_pWorld', 'i')
netfirst = SDTest.Call('cWorld_GetFirstEntity', 'tii', networld)[0]
netmaxsend = SDTest.Peek('g_NetMaxPlayersInitSend', 'i')
SDTest.Call('cFileMem_SetBuffer', 'tviiiii', netf, netfbuf, 0x400, 0, 0)
SDTest.Call('cFileMem_Open', 'tiisi', netf, '', 6)
check('NetData_WriteUsrDescriptors none', lambda: (SDTest.Call('NetData_WriteUsrDescriptors', 'iii', netf, 0)[0], netfbytes(netf)))
check('NetData_WriteUsrDescriptors world', lambda: (netmaxsend, SDTest.Call('NetData_WriteUsrDescriptors', 'iii', netf, netfirst)[0] in
	(0, netfirst), netfbytes(netf)))
netsetb(netfbuf, (1, 0, 9, 3, 0, 0, 0, 0))
SDTest.Call('cFileMem_SetBuffer', 'tviiiii', netf, netfbuf, 0x400, 0, 0)
SDTest.Call('cFileMem_Open', 'tiisi', netf, '', 1)
check('NetData_ReadUsrDescriptors unknown', lambda: (SDTest.Call('NetData_ReadUsrDescriptors', 'vi', netf), SDTest.Call('cFileMem_Tell', 'tii',
	netf)[0]))
netcb = SDTest.Peek('g_MasterCommandCallback', 'i')
check('Net_Init', lambda: (SDTest.Call('Net_Init', 'v'), SDTest.Peek('g_NetBotCounter', 'i'), SDTest.Peek('g_NetObjCounter', 'i'),
	isfn(SDTest.Peek('g_MasterCommandCallback', 'i'), sym('NetData_OnMasterCommand')), SDTest.Peek('g_NetResInit', 'b'), netres(2)))
SDTest.Poke('g_NetBotCounter', netbot)
SDTest.Poke('g_NetObjCounter', netobj)
SDTest.Poke('g_MasterCommandCallback', netcb)
SDTest.Poke(sym('g_GVarServerType') + 0xc, netgvflags)

# the client and server functions without a connection
check('Net client state', lambda: (SDTest.Call('Net_IsServer', 'b')[0], SDTest.Call('Net_IsClient', 'b')[0],
	SDTest.Call('Net_GetClient', 'i')[0], SDTest.Call('Net_GetClientName', 's')[0], SDTest.Call('Net_GetClientPort', 'i')[0] & 0xffff,
	SDTest.Call('Net_GetServerName', 's')[0], SDTest.Call('Net_GetServerMap', 's')[0], SDTest.Call('Net_GetServerPort', 'i')[0] & 0xffff))
check('Net closes', lambda: (SDTest.Call('Net_ClientReady', 'b')[0], SDTest.Call('Net_CloseClient', 'b')[0], SDTest.Call('Net_CloseClientMsg',
	'bs', 'Levels/Menu')[0], SDTest.Call('Net_CloseServer', 'bs', 'Levels/Menu')[0], SDTest.Call('Net_ServerChangeLevel', 'bs', 'x')[0],
	SDTest.Call('Net_ModifyUsrData', 'bi', 0)[0]))
if SDTest.Peek('g_AseInit', 'i') == 0:
	check('Net ASE off', lambda: (SDTest.Call('Net_AseCheck', 'v'), SDTest.Call('Net_AseInit', 'vi', 1234),
		SDTest.Call('Net_AseShutdown', 'v')))
check('Net_HexDump', lambda: (SDTest.Call('Net_HexDump', 'vii', netin, 40), SDTest.Call('Net_HexDump', 'vii', netin, 16),
	SDTest.Call('Net_HexDump', 'vii', netin, 0), SDTest.Call('Net_DumpData', 'vsii', 'title', netin, 21)))
netfuncs = (sym('g_ClientAddRes'), sym('g_ClientDelRes'), sym('g_ClientCreate'), sym('g_ClientSetPlayer'), sym('g_OnUsrString'), sym('g_OnChatString'), sym('g_OnClientJoin'), sym('g_OnClientExit'), sym('g_OnNetBrowseCallback'))
def netfuncset():
	return map(lambda a: SDTest.Peek(a + 8, 'i') or SDTest.Peek(a + 0x10, 'b') & 3, netfuncs)
if netfuncset() == [0] * len(netfuncs):
	check('Net reset funcs', lambda: (SDTest.Call('Net_ClientResetFuncs', 'v'), SDTest.Call('Net_ServerResetFuncs', 'v'), netfuncset()))

# the client table of the server and the transport tables on poked globals
netglobs = (sym('g_NetClients'), sym('g_NetMaxClients'), sym('g_NetNumClients'), sym('g_NetClientAddrs'), sym('g_NetNextSend'), sym('g_ServerDataRateLimit'), sym('g_NetMode'), sym('g_NetMasterOn'), sym('g_NetPingServers'), sym('g_NetStatRecv'),
	sym('g_NetStatRecvSlot'), sym('g_NetStatSentSlot'), sym('g_NetStatSent'))
netgsaved = map(lambda a: SDTest.Peek(a, 'i'), netglobs)
nettab = netz(4 * 6)
netaddrs = netz(16 * 6)
netcls = []
for netk in range(4):
	netc = netnew(0x2c)
	SDTest.Call('cNetClient_ctor', 'tiii', netc, netk)
	netcls.append(netc)
for netk in (0, 2, 3):
	netput(nettab + 4 * netk, netcls[netk])
SDTest.Poke('g_NetClients', nettab)
SDTest.Poke('g_NetMaxClients', 4)
SDTest.Poke('g_NetNumClients', 3)
SDTest.Poke('g_NetClientAddrs', netaddrs)
def netwhich(c):
	if c == 0:
		return -1
	for k in range(4):
		if netcls[k] == c:
			return k
	return 'other'
check('Transport counts', lambda: (SDTest.Call('Transport_GetMaxClients', 'i')[0], SDTest.Call('Net_GetSendRate', 'i')[0],
	SDTest.Call('Transport_GetMode', 'i')[0], SDTest.Call('Net_IsMaster', 'i')[0]))
for netn in (-1, 0, 1, 2, 3, 4):
	check('NetServer slots' + fmt((netn,)), lambda n=netn: (SDTest.Call('NetServer_GetSlotByIndex', 'ii', n)[0],
		netwhich(SDTest.Call('NetServer_GetClient', 'ii', n)[0]), SDTest.Call('NetServer_GetIndexOfSlot', 'ii', n)[0]))
if SDTest.Peek('g_ClientPPS', 'i') != 0:
	for netk, nett in ((0, 100), (2, 50), (3, 300)):
		netput(netcls[netk] + 0x20, nett)
	for nett in (0, 200, 400, -1000):
		check('NetServer_NextToSend' + fmt((nett,)), lambda t=nett: SDTest.Call('NetServer_NextToSend', 'ii', t)[0])
netsetb(netmsg, netstr('zyxw'))
check('NetServer_SendAll', lambda: (SDTest.Call('NetServer_SendAll', 'vii', netmsg, 3), map(lambda c: len(netq(c)), netcls)))
netus = []
for netk in range(4):
	netu = netz(0x10)
	netput(netu + 4, netk)
	netput(netcls[netk] + 0x28, netu)
	netus.append(netu)
check('NetServer_SendAllBut', lambda: (SDTest.Call('NetServer_SendAllBut', 'viii', netmsg, 2, 2), map(lambda c: netq(c), netcls)))
for netk in range(4):
	netput(netcls[netk] + 0x28, 0)
SDTest.Poke('g_NetNextSend', 0)
check('NetServer_SendTo', lambda: (SDTest.Call('NetServer_SendTo', 'iiiii', 1000, 0, 4, netdat)[0], SDTest.Peek('g_NetNextSend', 'i'),
	SDTest.Peek('g_ServerDataRateLimit', 'i'), SDTest.Call('NetServer_SendTo', 'iiiii', 500, 0, 4, netdat)[0], SDTest.Call('NetServer_SendTo', 'iiiii',
	2000, 1, 4, netdat)[0], SDTest.Call('NetServer_SendTo', 'iiiii', 2000, 9, 4, netdat)[0], netcs(netcls[0])))
SDTest.Poke('g_ServerDataRateLimit', netgsaved[5])
check('NetServer_Goodbye', lambda: (SDTest.Call('NetServer_Goodbye', 'v'), map(lambda c: neti(c + 0x10), netcls)))
netput(netcls[3] + 0x10, 3)
check('NetServer_Flush', lambda: (SDTest.Call('NetServer_Flush', 'ii', 5000)[0], map(lambda k: neti(nettab + 4 * k) != 0,
	range(4))))
check('NetServer_DropClient empty addr', lambda: (SDTest.Call('NetServer_DropClient', 'ii', 2)[0], neti(nettab + 8) != 0))
netput(netaddrs + 2 * 16 + 4, 0x0100007f)
check('NetServer_DropClient', lambda: (SDTest.Call('NetServer_DropClient', 'ii', 2)[0], neti(nettab + 8), SDTest.Peek('g_NetNumClients', 'i'),
	netw(netaddrs + 2 * 16, 4)))
check('NetServer_Receive mode 0', lambda: SDTest.Call('NetServer_Receive', 'iiipip', 100, netdat, 100, netctl, 100))
if SDTest.Peek('g_ServerType', 'i') and SDTest.Peek('g_ServerName', 'i'):
	check('NetServer_GetInfo', lambda: (SDTest.Call('NetServer_GetInfo', 'vi', netinfo), netw(netinfo, 2), netb(netinfo + 4, 7),
		SDTest.Peek(netinfo + 0xb, '@s'), SDTest.Peek(netinfo + 0x2b, '@s'), SDTest.Peek(netinfo + 0x3b, '@s')))
# the address table: the first free slot takes a new address
netsetb(netaddrs, [0] * 96)
SDTest.Poke('g_NetNumClients', 0)
netad = netz(0x20)
for netaddr in ((2, 0x1234, 0x0100007f), (2, 0x1234, 0x0100007f), (2, 0x1235, 0x0100007f), (2, 0x1234, 0x0200007f),
	(3, 0x1234, 0x0100007f), (2, 1, 1), (2, 2, 2)):
	check('Transport_FindClient' + fmt(netaddr), lambda a=netaddr: (netput(netad, a[0] + a[1] * 65536),
		netput(netad + 4, a[2]), SDTest.Call('Transport_FindClient', 'ii', netad)[0], SDTest.Peek('g_NetNumClients', 'i'))[2:])
check('Transport addrs', lambda: netw(netaddrs, 20))
check('Transport_RemoveClient', lambda: (SDTest.Call('Transport_RemoveClient', 'ii', 1)[0], SDTest.Call('Transport_RemoveClient', 'ii', 1)[0],
	SDTest.Peek('g_NetNumClients', 'i'), netw(netaddrs + 16, 4)))
check('Transport sends to empty slots', lambda: (SDTest.Call('Transport_SendToClient', 'iiii', netdat, 4, 1)[0],
	SDTest.Call('Transport_SendNeedPassword', 'ii', 1)[0]))
netping = netz(0x2800)
SDTest.Poke('g_NetPingServers', netping)
check('Transport_PingFreeSlot empty', lambda: SDTest.Call('Transport_PingFreeSlot', 'i')[0])
for netk in range(3):
	netput(netping + netk * 0x14 + 0x10, 500 - netk)
check('Transport_PingFreeSlot 3', lambda: SDTest.Call('Transport_PingFreeSlot', 'i')[0])
for netk in range(0x200):
	netput(netping + netk * 0x14 + 0x10, 1000 + (netk * 37) % 0x200)
check('Transport_PingFreeSlot full', lambda: SDTest.Call('Transport_PingFreeSlot', 'i')[0])
check('Transport mode 0 returns', lambda: (SDTest.Call('Transport_GetLocalPort', 'i')[0] & 0xffff, SDTest.Call('Transport_Recv', 'iip',
	netdat, 0x40), SDTest.Call('Transport_PingServer', 'ii', netad)[0], SDTest.Call('Transport_Wait', 'vi', 1000),
	SDTest.Call('Transport_CloseBrowser', 'i')[0]))
# without the master server nothing goes out
if SDTest.Peek('g_NetMasterOn', 'i') == 0:
	check('Transport master off', lambda: (SDTest.Call('Net_SendMasterString', 'is', 'x')[0], SDTest.Call('Net_PingInetSvrs', 'i')[0]))
netsetb(netpkt, (0, 0, 0, 0, 0x7e, 0x41, 0x42, 0x43, 0))
check('Transport_MasterPacket off', lambda: SDTest.Call('Transport_MasterPacket', 'iii', netpkt, 9)[0])
SDTest.Poke('g_NetMasterOn', 1)
check('Transport_MasterPacket mode 0', lambda: (SDTest.Call('Transport_MasterPacket', 'iii', netpkt, 9)[0], SDTest.Call('Transport_MasterPacket',
	'iii', netpkt, 6)[0]))
for netk in range(len(netglobs)):
	SDTest.Poke(netglobs[netk], netgsaved[netk])

# the gvar string setter, the cString length and the unlock of a static buffer
netgv = netz(0x20)
check('cGVar_SetString', lambda: (SDTest.Call('cGVar_SetString', 'tbis', netgv, 'abc')[0], SDTest.Peek(netgv + 0x10, '@p'),
	netput(netgv + 0xc, 1), SDTest.Call('cGVar_SetString', 'tbis', netgv, 'zz')[0], SDTest.Peek(netgv + 0x10, '@p')))
netcstr = netz(8)
check('cString_Length', lambda: (SDTest.Call('cString_Length', 'tii', netcstr)[0], netput(netcstr, netename),
	SDTest.Call('cString_Length', 'tii', netcstr)[0]))
netsb = netz(0x10)
netput(netsb + 8, 1)
check('cStaticBuffer_Unlock', lambda: (SDTest.Call('cStaticBuffer_Unlock', 'tvi', netsb), netw(netsb, 3)))

# the managers: only the messages that need no entity and no callback
netsm = netz(0x90)
netcm = netz(0x4a0)
netsetb(netctl, (2, 0, 0, 0))
check('cServerNetManager_OnReceive goodbye', lambda: (SDTest.Call('cServerNetManager_OnReceive', 'tviiiii', netsm, netdat, 0, netctl, 1),
	SDTest.Call('cServerNetManager_OnReceive', 'tviiiii', netsm, netdat, 0, netctl, 0)))
check('cClientNetManager_OnReceive no ship', lambda: (SDTest.Call('cClientNetManager_OnReceive', 'tviiiii', netcm, netdat, 4, netctl, 1),
	netput(netctl, 3), SDTest.Call('cClientNetManager_OnReceive', 'tviiiii', netcm, netdat, 4, netctl, 2)))
if SDTest.Peek('g_NetUserFactory', 'i') == 0:
	netc = netnew(0x2c)
	SDTest.Call('cNetClient_ctor', 'tiii', netc, -1)
	netput(netcm + 8, netc)
	netput(netsm + 8, netc)
	netrs = SDTest.Peek('g_NetMaxResourcesSend', 'i')
	if netrs > 0 and netrs < 0x100 and netflags == 0:
		check('cClientNetManager_SendResources', lambda r=netrs: (r, SDTest.Call('cClientNetManager_SendResources', 'tvi', netcm), netq(netc)))
	SDTest.Call('cNetClient_dtor', 'tvi', netc)
	netgs = map(lambda a: SDTest.Peek(a, 'i'), (sym('g_ClientPassword'), sym('g_PlayerName'), sym('g_PlayerModel'), sym('g_PilotModel'), sym('g_Motor0Model'), sym('g_Motor1Model'), sym('g_Motor2Model'),
		sym('g_Motor3Model'), sym('g_WeaponBayList')))
	if not 0 in netgs:
		check('cServerNetManager_SendHello', lambda: (SDTest.Call('cServerNetManager_SendHello', 'tvii', netsm, 6), netq(netc)))
	SDTest.Call('cNetClient_dtor', 'tvi', netc)
