# game Input/ActionSet.c, IDevice.c, PcKeyboard.c, PcMouse.c, PcPad.c (execfile'd by sdcall.py after its other checks,
# Python 1.5.2): the virtual keyboard, text input, rumble state and action states on objects in heap memory, the names
# and reserved entries of the real devices, the bindings by set and action name and the default bindings of the
# devices. The bindings of the devices are saved first and copied back at the end, the define state and the
# engine variables poked here are restored. Not called: anything that acquires or polls a device (the Update and
# Poll functions, Input_Update, Input_SelectActionSet, the Init and Create functions), the constructors of the
# devices (they register engine variables), cFuncGvarDevices Load and Save (files), the functions that call Python
# (cActionSet_Fire with a press, Input_OnTextDone, Input_OnDefined) and SetValue while a key is being defined.
inpb = 0x400000
def inpz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def inpfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def inpi(a):
	return SDTest.Peek(a, '@i')
def inpwi(a, v):
	SDTest.Poke(a - 0x400000, v)
def inpf(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def inpwf(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def inpcopy(dst, src, n):
	SDTest.Call('crt_memcpy', 'iiii', dst, src, n)
def inpstr(a):
	return SDTest.Peek(a, '@s')
def inppstr(a):
	return SDTest.Peek(a, '@p')

# the virtual keyboard grid (Standard first: Other keeps the columns)
inpvk = inpz(0x110)
for inpn in ('Standard', 'numbers', 'CONSOLE', 'Other', 'Numbers'):
	check('cVirtualKeyboard_Set' + fmt((inpn,)), lambda inpn=inpn: (SDTest.Call('cVirtualKeyboard_Set', 'tbis', inpvk, inpn)[0],
		inpi(inpvk + 0x100), inpi(inpvk + 0x104), inpi(inpvk + 0x108), inpstr(inpvk)))
inpfree(inpvk)
check('g_VirtualKeyboard', lambda: (SDTest.Peek(sym('g_VirtualKeyboard') + 0x100, 'i'), SDTest.Peek(sym('g_VirtualKeyboard') + 0x104, 'i'), SDTest.Peek(sym('g_VirtualKeyboard') + 0x108, 'i')))

# text input of a player
inppi = inpz(0xb4)
def inppist(p):
	return (inpi(p), inpi(p + 4), inpstr(p + 8), inpf(p + 0x8c, 2))
check('cPlayerInput_Clear', lambda: (SDTest.Call('cPlayerInput_Clear', 'tvi', inppi), inppist(inppi)))
for inpn in ('hello', '', 'x' * 0x80, 'ABC def'):
	check('cPlayerInput_SetString' + fmt((len(inpn),)), lambda inpn=inpn: (SDTest.Call('cPlayerInput_SetString', 'tvis', inppi, inpn),
		inppist(inppi)))
for inpv in ((0.5, 0.25), (20.0, 3.0), (-3.0, -0.75), (-100.0, 50.0), (0.0001, -0.0001), (12.4995, 5.4995), (1e30, -1e30)):
	inpwf(inppi + 0x8c, (0.5, 0.5))
	check('cPlayerInput_MoveCursor' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('cPlayerInput_MoveCursor', 'tviff', inppi, inpv[0],
		inpv[1]), inpf(inppi + 0x8c, 2)))
for inpc in (65, 122, 48, 10, 8, 126, 32, 0, 200):
	check('cPlayerInput_SetCursorChar' + fmt((inpc,)), lambda inpc=inpc: (SDTest.Call('cPlayerInput_SetCursorChar', 'tbii', inppi, inpc)[0],
		inpf(inppi + 0x8c, 2), SDTest.Call('cPlayerInput_GetCursorChar', 'tbi', inppi)[0]))
inpfree(inppi)
# SetInputFunc on player 4 with no function; onDone is put back
inppi = symva('g_PlayerInput') + 3 * 0xb4
inpold = inpi(inppi + 0x98)
check('cPlayerInput_SetInputFunc', lambda: (SDTest.Call('cPlayerInput_SetInputFunc', 'tvis', inppi, ''), isfn(inpi(inppi + 0x98), sym('Input_OnTextDone')),
	inpi(inppi + 0x9c + 8)))
inpwi(inppi + 0x98, inpold)

# rumble state of a pad
inppad = inpz(0x20)
def inppadst(p):
	return (inpi(p), inpf(p + 4, 6), inpi(p + 0x1c))
check('cPad_Init', lambda: (SDTest.Call('cPad_Init', 'tvii', inppad, 2), inppadst(inppad)))
for inpv in ((0.5, 0.75, 2.0), (0.05, 0.0500001, 10.0), (-1.0, 2.0, 100.0), (1.5, 0.0, -3.0), (0.06, 1.0, 30.0),
		(0.0, 0.0, 0.0), (1.0, 0.051, 29.999)):
	check('cPad_Rumble' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('cPad_Rumble', 'tvifff', inppad, inpv[0], inpv[1],
		inpv[2]), inppadst(inppad)))
SDTest.Call('cPad_Rumble', 'tvifff', inppad, 0.5, 0.75, 2.0)
inpwf(inppad + 0x14, (1e30,))
check('cPad_Update later', lambda: (SDTest.Call('cPad_Update', 'tvi', inppad), inppadst(inppad)))
inpwf(inppad + 0x14, (0.0,))
check('cPad_Update menu', lambda: (SDTest.Call('cPad_Update', 'tvi', inppad), inpf(inppad + 4, 4), inpf(inppad + 0x18, 1),
	inpf(inppad + 0x14, 1)[0] != 0.0))
SDTest.Call('cPad_Rumble', 'tvifff', inppad, 0.5, 0.75, 2.0)
check('cPad_Stop', lambda: (SDTest.Call('cPad_Stop', 'tvi', inppad), inppadst(inppad)))
inpfree(inppad)

# an action set table of the exe's types in heap memory, freed by the exe (rva 0x1393b0)
inptbl = inpz(8)
check('ActionSetTable_Init', lambda: (SDTest.Call('ActionSetTable_Init', 'tvii', inptbl, 2), inpi(inptbl), inpi(inptbl + 4) != 0))
inpset = inpi(inptbl + 4)
check('cActionSet_Init', lambda: (SDTest.Call('cActionSet_Init', 'tviis', inpset, 3, 'InpSetA'), inppstr(inpset), inpi(inpset + 4),
	inpi(inpset + 8) != 0))
inpacts = inpi(inpset + 8)
def inpact(k):
	return inpacts + k * 0x7c
for inpk in range(3):
	check('cInputAction_Init' + fmt((inpk,)), lambda inpk=inpk: (SDTest.Call('cInputAction_Init', 'tvisi', inpact(inpk),
		'InpAct' + str(inpk), inpk & 1), inppstr(inpact(inpk) + 4), inpi(inpact(inpk) + 0x78) & 0xff, inpi(inpact(inpk))))
def inpst(a):
	return (inpi(a), inpf(a + 8, 8))
def inpsetst():
	return (inpst(inpact(0)), inpst(inpact(1)), inpst(inpact(2)))
inpcases = ((0.0, 0.0, 1.0, 0.25, 0.75, 0.5, 0.0, 1.0), (1.0, 0.6, 0.0, 0.5, 0.0, 0.4, 0.9, 0.5),
	(0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5), (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
def inpfill(c):
	for k in range(3):
		inpwf(inpact(k) + 8, c)
for inpc in inpcases:
	inpfill(inpc)
	for inpp in range(4):
		check('cInputAction_Pressed' + fmt((inpc, inpp)), lambda inpp=inpp: (SDTest.Call('cInputAction_Pressed', 'tbii', inpact(0),
			inpp)[0], SDTest.Call('cInputAction_Released', 'tbii', inpact(0), inpp)[0]))
	check('cActionSet_AnyActive' + fmt(inpc), lambda: SDTest.Call('cActionSet_AnyActive', 'tbi', inpset)[0])
	check('cInputAction_Latch' + fmt(inpc), lambda: (SDTest.Call('cInputAction_Latch', 'tvi', inpact(1)), inpi(inpact(1)),
		SDTest.Call('cInputAction_Latch', 'tvi', inpact(0)), inpi(inpact(0))))
	inpwi(inpact(1), 0)
	check('cActionSet_Latch' + fmt(inpc), lambda: (SDTest.Call('cActionSet_Latch', 'tvi', inpset), inpsetst()))
	# Fire only without a press: a press would call the unbound functions
	inpj = 0
	for inpp in range(4):
		if 0.5 > inpc[inpp] and inpc[4 + inpp] > 0.5:
			inpj = 1
	if not inpj:
		check('cInputAction_Fire unpressed' + fmt(inpc), lambda: (SDTest.Call('cInputAction_Fire', 'tvi', inpact(2)),
			SDTest.Call('cActionSet_Fire', 'tvi', inpset), inpsetst()))
	for inpt in (('cInputAction_Shift', sym('cInputAction_Shift')), ('cInputAction_Keep', sym('cInputAction_Keep')), ('cInputAction_Clear', sym('cInputAction_Clear'))):
		inpfill(inpc)
		check(inpt[0] + fmt(inpc), lambda inpt=inpt: (SDTest.Call(inpt[1], 'tvi', inpact(1)), inpst(inpact(1))))
	for inpt in (('cActionSet_Shift', sym('cActionSet_Shift')), ('cActionSet_Keep', sym('cActionSet_Keep')), ('cActionSet_Clear', sym('cActionSet_Clear'))):
		inpfill(inpc)
		check(inpt[0] + fmt(inpc), lambda inpt=inpt: (SDTest.Call(inpt[1], 'tvi', inpset), inpsetst()))
check('ActionSetTable free', lambda: (SDTest.Call('ActionSetTable_Free', 'tvi', inptbl), inpi(inptbl), inpi(inptbl + 4)))
inpfree(inptbl)

# the action sets of the game
inpnsets = SDTest.Peek('g_ActionSets', 'i')
inpsitems = SDTest.Peek(sym('g_ActionSets') + 0x4, 'i')
check('g_ActionSets', lambda: (inpnsets, SDTest.Peek('g_CurActionSet', 'i')))
inpsetnames = []
inpactnames = []
inpk = 0
while inpk < inpnsets:
	inpa = inpsitems + inpk * 12
	inpsetnames.append(inppstr(inpa))
	inpl = []
	inpj = 0
	while inpj < inpi(inpa + 4):
		inpl.append(inppstr(inpi(inpa + 8) + inpj * 0x7c + 4))
		inpj = inpj + 1
	inpactnames.append(inpl)
	check('action set' + fmt((inpk,)), lambda inpk=inpk: (inpsetnames[inpk], tuple(inpactnames[inpk])))
	inpk = inpk + 1

# the devices: keyboard, mouse and the four pads (static objects of the exe)
inpkb = symva('g_Keyboard')
inpms = symva('g_Mouse')
inppads = (symva('g_PcPads'), symva('g_PcPads') + 0x2c, symva('g_PcPads') + 0x58, symva('g_PcPads') + 0x84)
inpdevs = (inpkb, inpms) + inppads
def inpdevlist():
	r = []
	h = SDTest.Peek('g_InputDevices', 'i')
	n = h
	while n:
		d = inpi(n + 8)
		if d in inpdevs:
			r.append(list(inpdevs).index(d))
		else:
			r.append(-1)
		n = inpi(n)
		if n == h:
			break
	return (tuple(r), SDTest.Peek(sym('g_InputDevices') + 0x8, 'i'))
check('device list', inpdevlist)
for inpd in inpdevs:
	check('device' + fmt((list(inpdevs).index(inpd),)), lambda inpd=inpd: (inpi(inpd) - inpb, inpi(inpd + 8),
		inpi(inpd + 0xc) != 0))
check('device names', lambda: (SDTest.Call('cPcKeyboard_GetName', 'tsi', inpkb)[0], SDTest.Call('cPcMouse_GetName', 'tsi', inpms)[0],
	SDTest.Call('cPcPad_GetName', 'tsi', inppads[0])[0], SDTest.Call('cPcPad_GetName', 'tsi', inppads[1])[0],
	SDTest.Call('cPcPad_GetName', 'tsi', inppads[2])[0], SDTest.Call('cPcPad_GetName', 'tsi', inppads[3])[0],
	SDTest.Call('cAbstractIDevice_IsReserved', 'tbii', inpkb, 1)[0]))
inpk = 0
while inpk < 0x100:
	check('cPcKeyboard names' + fmt((inpk,)), lambda inpk=inpk: (SDTest.Call('cPcKeyboard_GetEntryName', 'tsii', inpkb, inpk)[0],
		SDTest.Call('cPcKeyboard_IsReserved', 'tbii', inpkb, inpk)[0]))
	inpk = inpk + 1
for inpk in (-1, 0x100, 0x12345, -0x7fffffff):
	check('cPcKeyboard names' + fmt((inpk,)), lambda inpk=inpk: (SDTest.Call('cPcKeyboard_GetEntryName', 'tsii', inpkb, inpk)[0],
		SDTest.Call('cPcKeyboard_IsReserved', 'tbii', inpkb, inpk)[0]))
# the labels are the names of the keyboard layout: Wine's DirectInput ones in the reference, SDL's in the current
# build; only that there is one is printed, and the label of NUMPAD8 in the binding lists is masked
for inpk in (0x1e, 0x39, 0x01, 0xc8):
	check('cPcKeyboard_GetEntryLabel' + fmt((inpk,)), lambda inpk=inpk: SDTest.Call('cPcKeyboard_GetEntryLabel', 'tsii', inpkb, inpk)[0] != '')
inpkp8 = SDTest.Call('cPcKeyboard_GetEntryLabel', 'tsii', inpkb, 0x48)[0]
def inpmasklabel(v):
	import string
	return string.replace(repr(v), inpkp8, 'NUMPAD8')
for inpk in range(14):
	check('cPcMouse names' + fmt((inpk,)), lambda inpk=inpk: (SDTest.Call('cPcMouse_GetEntryName', 'tsii', inpms, inpk)[0],
		SDTest.Call('cPcMouse_GetEntryLabel', 'tsii', inpms, inpk)[0], SDTest.Call('cPcMouse_IsReserved', 'tiii', inpms, inpk)[0] & 0xff))
for inpk in range(0x3a):
	check('cPcPad names' + fmt((inpk,)), lambda inpk=inpk: (SDTest.Call('cPcPad_GetEntryLabel', 'tsii', inppads[0], inpk)[0],
		SDTest.Call('cPcPad_GetEntryName', 'tsii', inppads[1], inpk)[0], SDTest.Call('cPcPad_IsReserved', 'tbii', inppads[2], inpk)[0]))

# the bindings of a device as "entry:set:player:action" for the bound ones
def inpbinds(dev, sets):
	n = inpi(dev + 8)
	e = inpi(dev + 0xc)
	r = ''
	k = 0
	while k < n:
		for s in sets:
			p = inpi(e + k * 0x28 + 8 + s * 8)
			a = inpi(e + k * 0x28 + 12 + s * 8)
			if p != -1 or a != -1:
				r = r + '%d:%d:%d:%d ' % (k, s, p, a)
		k = k + 1
	return r
def inpallbinds():
	r = []
	for d in inpdevs:
		r.append(inpbinds(d, (0, 1, 2, 3)))
	return tuple(r)
inpsaved = []
for inpd in inpdevs:
	inpn = inpi(inpd + 8) * 0x28
	inpbuf = inpz(inpn)
	inpcopy(inpbuf, inpi(inpd + 0xc), inpn)
	inpsaved.append((inpd, inpbuf, inpn))
inpstart = inpallbinds()
check('bindings at start', lambda: inpstart)
inpdefsaved = (SDTest.Peek('g_ToDefinePlayer', 'i'), SDTest.Peek('g_ToDefineAction', 'i'), SDTest.Peek('g_ToDefineSet', 'i'),
	SDTest.Peek('g_OnDefined', 'i'))
check('define state at start', lambda: inpdefsaved[0:3])

# entries of single devices
def inpgetentry(d, e, s):
	return SDTest.Call('cAbstractIDevice_GetEntry', 'tviiipp', d, e, s, 7, 7)[1:]
for inpv in ((0x1e, 1, 2, 5), (0x1e, 3, -1, -1), (0x30, 0, 3, 0x7fff)):
	check('cAbstractIDevice_SetEntry' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('cAbstractIDevice_SetEntry', 'tbiiiiii', inpkb, inpv[0],
		inpv[1], inpv[2], inpv[3], 1)[0], inpgetentry(inpkb, inpv[0], inpv[1])))
for inpk in (0x01, 0x29, 0x3b, 0xdd, 0x1e):
	for inpc in (0, 1):
		check('cPcKeyboard_SetEntry' + fmt((inpk, inpc)), lambda inpk=inpk, inpc=inpc: (SDTest.Call('cPcKeyboard_SetEntry', 'tbiiiiii',
			inpkb, inpk, 2, 1, 4, inpc)[0], inpgetentry(inpkb, inpk, 2)))
for inpk in (0, 4, 7, 8):
	for inpc in (0, 1):
		check('cPcMouse_SetEntry' + fmt((inpk, inpc)), lambda inpk=inpk, inpc=inpc: (SDTest.Call('cPcMouse_SetEntry', 'tbiiiiii',
			inpms, inpk, 2, 1, 4, inpc)[0], inpgetentry(inpms, inpk, 2)))
for inpp in (0, 1, -1, 3):
	check('cPcPad_SetEntry' + fmt((inpp,)), lambda inpp=inpp: (SDTest.Call('cPcPad_SetEntry', 'tbiiiiii', inppads[1], 3, 2, inpp, 9,
		1)[0], inpgetentry(inppads[1], 3, 2)))

# the bindings by name
inps0 = inpsetnames[0]
inps1 = inpsetnames[min(1, len(inpsetnames) - 1)]
inpa0 = inpactnames[0][min(2, len(inpactnames[0]) - 1)]
inpa1 = inpactnames[min(1, len(inpsetnames) - 1)][min(3, len(inpactnames[min(1, len(inpsetnames) - 1)]) - 1)]
def inpgetentryn(d, e, s):
	r = SDTest.Call('Input_GetEntry', 'issspp', d, e, s, 77, 0)
	if r[0] == 0:
		return (r[0], r[1], SDTest.Peek(r[2], '@s'))
	return r
for inpv in (('Kb', 'Q', 0, inps0, inpa0), ('Kb', 'Q', 4, inps0, inpa0), ('Kb', 'Q', 0, 'NoSet', inpa0),
		('Kb', 'Q', 0, inps0, 'NoAct'), ('NoDev', 'Q', 0, inps0, inpa0), ('Kb', 'NOKEY', 0, inps0, inpa0),
		('Mouse', 'But2', 1, inps1, inpa1), ('Pad2', 'A', 1, inps1, inpa1), ('Pad2', 'B', 0, inps1, inpa1),
		('Kb', 'ESCAPE', 2, inps1, inpa1), ('kb', 'Q', 0, inps0, inpa0), ('Kb', 'q', 0, inps0, inpa0)):
	check('Input_AssignEntry' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('Input_AssignEntry', 'ississ', inpv[0], inpv[1], inpv[2],
		inpv[3], inpv[4])[0], inpgetentryn(inpv[0], inpv[1], inpv[3])))
for inpv in (('Kb', 'W', inps1), ('Kb', 'W', 'NoSet'), ('Mouse', 'Left', inps0), ('Mouse', 'Nope', inps0),
		('Pad1', 'UP', inps1), ('NoDev', 'A', inps0)):
	check('Input_GetEntry' + fmt(inpv), lambda inpv=inpv: inpgetentryn(inpv[0], inpv[1], inpv[2]))
for inpp in (0, 1, 3, 4, -1):
	for inpv in ((inps0, inpa0), (inps1, inpa1), ('NoSet', inpa0), (inps0, 'NoAct')):
		for inpc in (0, 1):
			check('Input_GetDefinedEntry' + fmt((inpp,) + inpv + (inpc,)), lambda inpp=inpp, inpv=inpv, inpc=inpc: inpmasklabel((
				SDTest.Call('Input_GetDefinedEntry', 'sisssi', inpp, inpv[0], inpv[1], 'Kb', inpc)[0],
				SDTest.Call('Input_GetDefinedEntry', 'sisssi', inpp, inpv[0], inpv[1], 'Pad2', inpc)[0],
				SDTest.Call('Input_GetDefinedEntry', 'sisssi', inpp, inpv[0], inpv[1], 'None', inpc)[0],
				SDTest.Call('Input_GetDefinedList', 'iissbii', inpp, inpv[0], inpv[1], '', 0x200, inpc))))
for inpv in ((0, 0, 9), (0, 1, 2), (1, 1, 5), (5, 0, 0), (0, 3, 2)):
	check('Input_FindBinding' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('Input_FindBinding', 'siiisi', inpv[0], inpv[1], inpv[2],
		'Mouse', 0)[0], SDTest.Call('Input_ListBindings', 'iiiibii', inpv[0], inpv[1], inpv[2], '', 0x100, 1)))
for inpv in ((0, inps0, inpa0, 1), (0, inps0, inpa0, 0), (1, inps1, inpa1, 0), (4, inps0, inpa0, 0), (0, 'NoSet', inpa0, 0),
		(0, inps0, 'NoAct', 0)):
	check('Input_ClearDefinedList' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('Input_ClearDefinedList', 'iissi', inpv[0], inpv[1],
		inpv[2], inpv[3])[0], SDTest.Call('Input_GetDefinedList', 'iissbii', 0, inpv[1], inpv[2], '', 0x200, 0)))
for inpv in ((0, 1, 2, 0), (1, 1, 5, 1), (7, 0, 0, 0)):
	check('Input_ClearBinding' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('Input_ClearBinding', 'iiiii', inpv[0], inpv[1], inpv[2],
		inpv[3])[0], inpbinds(inpkb, (1,))))
check('Input_Bind errors', lambda: (SDTest.Call('Input_Bind', 'iisss', 0, 'NoSet', inpa0, '')[0],
	SDTest.Call('Input_Bind', 'iisss', 0, inps0, 'NoAct', '')[0]))

# the key definition state (no function: the callback stays NULL)
def inpdefst():
	return (SDTest.Peek('g_ToDefinePlayer', 'i'), SDTest.Peek('g_ToDefineAction', 'i'), SDTest.Peek('g_ToDefineSet', 'i'), SDTest.Peek('g_OnDefined', 'i'))
for inpv in ((0, inps0, inpa0), (3, inps1, inpa1), (4, inps0, inpa0), (-1, inps0, inpa0), (0, 'NoSet', inpa0),
		(0, inps0, 'NoAct')):
	check('Input_ListenToDefine' + fmt(inpv), lambda inpv=inpv: (SDTest.Call('Input_ListenToDefine', 'iisss', inpv[0], inpv[1], inpv[2],
		'')[0], inpdefst(), SDTest.Call('Input_AbortListenToDefine', 'v'), inpdefst()))
check('Input_SetToDefine', lambda: (SDTest.Call('Input_SetToDefine', 'viii', 2, 1, 1), inpdefst(), SDTest.Call('Input_AbortListenToDefine', 'v'),
	inpdefst()))
check('Input_SetDefineCallback', lambda: (SDTest.Call('Input_SetDefineCallback', 'vi', 0x1234560), inpdefst(),
	SDTest.Call('Input_SetDefineFunc', 'vs', ''), inpdefst()))
inpwi(0x400000 + sym('g_ToDefinePlayer'), inpdefsaved[0])
inpwi(0x400000 + sym('g_ToDefineAction'), inpdefsaved[1])
inpwi(0x400000 + sym('g_ToDefineSet'), inpdefsaved[2])
inpwi(0x400000 + sym('g_OnDefined'), inpdefsaved[3])

# entry values: the actions of the current set (0 in the menu) take the largest value; values put back after
def inpactaddr(s, a):
	return inpi(inpsitems + s * 12 + 8) + a * 0x7c
inpsavef = []
for inpv in ((inpkb, 0xcd), (inpkb, 0xcb), (inpms, 4), (inpms, 5)):
	inpsavef.append((inpi(inpv[0] + 0xc) + inpv[1] * 0x28, inpf(inpi(inpv[0] + 0xc) + inpv[1] * 0x28, 2)))
for inpk in (2, 3, 4, 5, 8, 9):
	inpsavef.append((inpactaddr(0, inpk) + 8, inpf(inpactaddr(0, inpk) + 8, 8)))
SDTest.Call('cAbstractIDevice_SetEntry', 'tbiiiiii', inpkb, 0xcd, 0, 0, 9, 0)
SDTest.Call('cAbstractIDevice_SetEntry', 'tbiiiiii', inpms, 4, 0, 1, 5, 0)
SDTest.Call('cAbstractIDevice_SetEntry', 'tbiiiiii', inpms, 5, 0, 0, 4, 0)
SDTest.Call('cAbstractIDevice_SetEntry', 'tbiiiiii', inpkb, 0xcb, 0, -1, 8, 0)
for inpk in (2, 3, 4, 5, 8, 9):
	inpwf(inpactaddr(0, inpk) + 0x18, (0.0, 0.0, 0.0, 0.0))
for inpv in (0.75, 0.25, 1.0, -0.5, 0.0):
	check('cAbstractIDevice_SetValue' + fmt((inpv,)), lambda inpv=inpv: (SDTest.Call('cAbstractIDevice_SetValue', 'tviif', inpkb, 0xcd, inpv),
		inpf(inpi(inpkb + 0xc) + 0xcd * 0x28, 2), inpf(inpactaddr(0, 9) + 0x18, 4)))
	check('cAbstractIDevice_SetValue player -1' + fmt((inpv,)), lambda inpv=inpv: (SDTest.Call('cAbstractIDevice_SetValue', 'tviif', inpkb,
		0xcb, inpv), inpf(inpactaddr(0, 8) + 8, 8)))
	check('cAbstractIDevice_SetAxis' + fmt((inpv,)), lambda inpv=inpv: (SDTest.Call('cAbstractIDevice_SetAxis', 'tviif', inpms, 4, inpv),
		inpf(inpi(inpms + 0xc) + 4 * 0x28, 2), inpf(inpi(inpms + 0xc) + 5 * 0x28, 2), inpf(inpactaddr(0, 5) + 0x18, 4),
		inpf(inpactaddr(0, 4) + 0x18, 4)))
for inpv in inpsavef:
	inpwf(inpv[0], inpv[1])

# the default bindings, the pad ones with the pad options
inpdefs = ((inpkb, sym('cPcKeyboard_SetDefaults'), sym('cPcKeyboard_SetGameDefaults')), (inpms, sym('cPcMouse_SetDefaults'), sym('cPcMouse_SetGameDefaults')), (inppads[0], sym('cPcPad_SetDefaults'), sym('cPcPad_SetGameDefaults')),
	(inppads[3], sym('cPcPad_SetDefaults'), sym('cPcPad_SetGameDefaults')))
for inpv in inpdefs:
	check('defaults' + fmt((list(inpdevs).index(inpv[0]),)), lambda inpv=inpv: (SDTest.Call('cAbstractIDevice_ClearEntries', 'tvi', inpv[0]),
		inpbinds(inpv[0], (0, 1, 2, 3)), SDTest.Call(inpv[1], 'tvi', inpv[0]), inpbinds(inpv[0], (0, 1, 2, 3)),
		SDTest.Call(inpv[2], 'tvi', inpv[0]), inpbinds(inpv[0], (0, 1, 2, 3))))
inpxb = (sym('g_XBCDigitalAsAnalog'), sym('g_XBCameraAutoPad'), sym('g_XBInvertCharYPad'), sym('g_XBInvertLRChar'), sym('g_XBInvertLRVehicle'), sym('g_XBInvertVehicleYPad'), sym('g_XBVDigitalAsAnalog'), sym('g_XBVehicleAsFPS'))
inpxbsaved = []
for inpk in inpxb:
	inpxbsaved.append(SDTest.Peek(inpk, 'i'))
for inpv in ((0, 0, 0, 0, 0, 0, 0, 0), (1, 1, 1, 1, 1, 1, 1, 1), (1, 0, 1, 0, 1, 0, 1, 0), (0, 1, 0, 1, 0, 1, 0, 1),
		(0, 0, 1, 1, 0, 0, 1, 1)):
	for inpk in range(8):
		inpwi(inpb + inpxb[inpk], inpv[inpk])
	check('pad defaults' + fmt(inpv), lambda: (SDTest.Call('cAbstractIDevice_ClearEntries', 'tvi', inppads[2]), SDTest.Call('cPcPad_SetVehicleDefaults', 'tvi',
		inppads[2]), inpbinds(inppads[2], (3,)), SDTest.Call('cPcPad_SetCharDefaults', 'tvi', inppads[2]), inpbinds(inppads[2], (1,))))
for inpk in range(8):
	inpwi(inpb + inpxb[inpk], inpxbsaved[inpk])
check('Input_ResetToDefault', lambda: (SDTest.Call('Input_ResetToDefault', 'v'), inpallbinds(), inpdevlist()))

# the bindings back
for inpv in inpsaved:
	inpcopy(inpi(inpv[0] + 0xc), inpv[1], inpv[2])
	inpfree(inpv[1])
check('bindings restored', lambda: inpallbinds() == inpstart)

# the inactivity timer, with the time far away so that no function runs
inptimer = inpf(0x400000 + sym('g_InactivityTimer'), 1)
inppause = SDTest.Peek('g_PauseInactivity', 'i')
check('CheckInactivityTime', lambda: (inpf(0x400000 + sym('g_InactivityTime'), 1), inpf(0x400000 + sym('g_InactivityTimeDef'), 1)))
inpwf(0x400000 + sym('g_InactivityTimer'), (5.0,))
check('Input_CheckInactivity', lambda: (SDTest.Call('Input_CheckInactivity', 'vf', 1.5), inpf(0x400000 + sym('g_InactivityTimer'), 1)))
inpwi(0x400000 + sym('g_PauseInactivity'), 1)
check('Input_CheckInactivity paused', lambda: (SDTest.Call('Input_CheckInactivity', 'vf', 1.5), inpf(0x400000 + sym('g_InactivityTimer'), 1)))
inpwi(0x400000 + sym('g_PauseInactivity'), inppause)
inpwf(0x400000 + sym('g_InactivityTimer'), inptimer)

# the stick dead zone
for inpv in ((0.1, 0.1, 0.2), (0.5, 0.5, 0.2), (1.0, 0.0, 0.2), (0.7, -0.7, 0.24), (-1.0, -1.0, 0.2), (0.3, 0.4, 0.5),
		(2.0, 0.0, 0.2), (0.0, -0.9, 0.0), (0.6, 0.8, 1.0), (-0.25, 0.75, 0.1), (0.0, 0.0, 0.0), (1e-20, 3e-20, 0.0)):
	check('Pad_DeadZone' + fmt(inpv), lambda inpv=inpv: SDTest.Call('Pad_DeadZone', 'vFFf', (inpv[0],), (inpv[1],), inpv[2])[1:])

# the device list functions and a bare device on heap memory
inplist = inpz(0x18)
inppp = inpz(4)
inpwi(inppp, 0x1111110)
check('InputDevices_Add', lambda: (SDTest.Call('InputDevices_Add', 'tvii', inplist, inppp), inpi(inplist + 8),
	inpi(inpi(inplist) + 8), inpi(inpi(inplist)) == inpi(inplist)))
inpwi(inppp, 0x2222220)
check('InputDevices_Add 2', lambda: (SDTest.Call('InputDevices_Add', 'tvii', inplist, inppp), inpi(inplist + 8),
	inpi(inpi(inplist) + 8), inpi(inpi(inpi(inplist)) + 8), inpi(inplist + 4)))
check('InputDevices_Clear', lambda: (SDTest.Call('InputDevices_Clear', 'tvi', inplist), inpi(inplist), inpi(inplist + 8),
	inpi(inplist + 0xc)))
inpfree(inplist)
inpfree(inppp)
inpdev = inpz(0x10)
def inpwords(a, n):
	r = []
	for k in range(n):
		r.append(inpi(a + 4 * k))
	return tuple(r)
check('cAbstractIDevice_ctor', lambda: (SDTest.Call('cAbstractIDevice_ctor', 'tii', inpdev)[0] == inpdev, inpi(inpdev) - inpb,
	inpi(inpdev + 4), inpi(inpdev + 8), inpi(inpdev + 0xc)))
check('cAbstractIDevice_InitEntries', lambda: (SDTest.Call('cAbstractIDevice_InitEntries', 'tvii', inpdev, 3), inpi(inpdev + 8),
	inpi(inpdev + 0xc) != 0, inpwords(inpi(inpdev + 0xc), 30)))
check('cAbstractIDevice_SetValue unbound', lambda: (SDTest.Call('cAbstractIDevice_SetValue', 'tviif', inpdev, 1, 0.8),
	SDTest.Call('cAbstractIDevice_SetValue', 'tviif', inpdev, 1, 0.3), inpf(inpi(inpdev + 0xc) + 0x28, 2)))
check('cAbstractIDevice_SetAxis unbound', lambda: (SDTest.Call('cAbstractIDevice_SetAxis', 'tviif', inpdev, 0, -0.4),
	inpf(inpi(inpdev + 0xc), 2), inpf(inpi(inpdev + 0xc) + 0x28, 2)))
check('cAbstractIDevice_dtor', lambda: (SDTest.Call('cAbstractIDevice_dtor', 'tvi', inpdev), inpi(inpdev) - inpb))
check('cAbstractIDevice_Delete', lambda: (SDTest.Call('cAbstractIDevice_ctor', 'tii', inpdev)[0] == inpdev,
	SDTest.Call('cAbstractIDevice_InitEntries', 'tvii', inpdev, 2), SDTest.Call('cAbstractIDevice_Delete', 'tiii', inpdev, 0)[0] == inpdev, inpi(inpdev) - inpb))
inpfree(inpdev)

# the states of the actions of the game (no keys held in the test)
check('Input_ResetStates', lambda: (SDTest.Call('Input_ResetStates', 'v'), inpf(inpactaddr(0, 0) + 8, 8)))
check('Input_ClearActions', lambda: (SDTest.Call('Input_ClearActions', 'v'), inpf(inpactaddr(0, 0) + 8, 8),
	SDTest.Call('cActionSet_AnyActive', 'tbi', inpsitems)[0]))
