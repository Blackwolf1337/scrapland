# game App/App.c + WinApp.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the getters of the
# application in the main menu: the paths, the command line switches, the registry, the handles, the dedicated
# flags, the console window without a window (it is closed in the test setup) and the console commands on one engine
# variable. Not called: everything that makes windows, loads levels or runs the frame (the message loop runs them
# all the time anyway).
app = SDTest.Peek(0x467ba0, 'i')
check('App_GetPath', lambda: SDTest.Call(0x20d100, 'vbi', '', 0x104)[1])
check('App_GetConfigDir', lambda: SDTest.Call(0x20d570, 'vbi', '', 0x200)[1])
for name in ('pythonexecute', 'console', 'dedicated', 'widewindow', 'txf_helper', 'python', 'execute', ''):
	check('CWinApp_HasArg' + fmt((name,)), lambda name=name: SDTest.Call(0x20d030, 'tbis', app, name)[0])
for name in ('LANGUAGE', 'SDTestNone'):
	check('App_RegGetString' + fmt((name,)), lambda name=name: SDTest.Call(0x20d120, 'tsis', app, name)[0])
check('App_GetHInstance', lambda: SDTest.Call(0x20d1a0, 'i')[0] == SDTest.Peek(app + 0x48, '@i'))
check('App_GetHWnd', lambda: SDTest.Call(0x20d290, 'i')[0] == SDTest.Peek(app + 0x7c, '@i'))
check('CWinApp_IsDedicatedWin', lambda: SDTest.Call(0x20d8b0, 'tbi', app)[0])
check('CApp_IsDedicatedWin', lambda: SDTest.Call(0x20c800, 'tbi', app)[0])
check('App_CheckBlock', lambda: SDTest.Call(0x20acd0, 'b')[0])
check('World_TimeScale', lambda: SDTest.Call(0x20aea0, 'f')[0])
check('app flags', lambda: (SDTest.Peek(app + 0x28, '@i'), SDTest.Peek(app + 0x2d, '@b'), SDTest.Peek(app + 0x30, '@i'),
	SDTest.Peek(app + 0x80, '@b'), SDTest.Peek(app + 0x81, '@b')))
for p in (('sdtest_none', '1'), ('SDTEST_NONE', '')):
	check('CApp_ParseParam' + fmt(p), lambda p=p: SDTest.Call(0x20b8d0, 'tbiss', app, p[0], p[1])[0])
	check('CWinApp_ParseParam' + fmt(p), lambda p=p: SDTest.Call(0x20d680, 'tbiss', app, p[0], p[1])[0])
if SDTest.Peek(0x467450, 'i'):
	check('App_GetObjData', lambda: SDTest.Call(0x20ae60, 'is', 'SDTestNone')[0] != 0)
# the console window is closed: no window, nothing printed, 0
check('ConsoleWin state', lambda: (SDTest.Peek(0x4a801c, 'i'), SDTest.Peek(0x467ff0, 'i') == 0))
if SDTest.Peek(0x4a801c, 'i') == 0 and SDTest.Peek(0x467ff0, 'i') == 0:
	check('ConsoleWin_GetHwnd', lambda: SDTest.Call(0x20e1d0, 'i')[0])
	check('ConsoleWin_Print', lambda: SDTest.Call(0x20de10, 'isi', 'sdtest\n', 0xff)[0])
	check('ConsoleWin_Show', lambda: SDTest.Call(0x20e5d0, 'ii', 1)[0])
	check('ConsoleWin_Close', lambda: SDTest.Call(0x20dd80, 'i')[0])
# the console commands on iLoadMsgString (rva 0x4679c0, the next loading tip), saved and restored
msg = SDTest.Peek(0x4679c0, 'i')
for cmd in (':iLoadMsgString 7', ':iLoadMsgString', '?iLoadMsg', ':iLoadMsgString  "12" ',
		"Scrap.Set('iLoadMsgString',3)", ':iLoadMsgString 5'):
	check('Console_Execute' + fmt((cmd,)), lambda cmd=cmd: (SDTest.Call(0x20aaf0, 'vs', cmd), SDTest.Peek(0x4679c0, 'i')))
SDTest.Poke(0x4679c0, msg)
