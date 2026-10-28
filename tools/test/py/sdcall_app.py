# game App/App.c + WinApp.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the getters of the
# application in the main menu: the paths, the command line switches, the registry, the handles, the dedicated
# flags, the console window without a window (it is closed in the test setup) and the console commands on one engine
# variable. Not called: everything that makes windows, loads levels or runs the frame (the message loop runs them
# all the time anyway).
app = SDTest.Peek('g_pApp', 'i')
# the folder only: the standalone run is ScrapC.exe
def appdir(p):
	i = len(p) - 1
	while i >= 0 and p[i] != '\\':
		i = i - 1
	return p[:i]
check('App_GetPath', lambda: appdir(SDTest.Call('App_GetPath', 'vbi', '', 0x104)[1]))
check('App_GetConfigDir', lambda: SDTest.Call('App_GetConfigDir', 'vbi', '', 0x200)[1])
for name in ('pythonexecute', 'console', 'dedicated', 'widewindow', 'txf_helper', 'python', 'execute', ''):
	check('CWinApp_HasArg' + fmt((name,)), lambda name=name: SDTest.Call('CWinApp_HasArg', 'tbis', app, name)[0])
for name in ('LANGUAGE', 'SDTestNone'):
	check('App_RegGetString' + fmt((name,)), lambda name=name: SDTest.Call('App_RegGetString', 'tsis', app, name)[0])
check('App_GetHInstance', lambda: SDTest.Call('App_GetHInstance', 'i')[0] == SDTest.Peek(app + 0x48, '@i'))
check('App_GetHWnd', lambda: SDTest.Call('App_GetHWnd', 'i')[0] == SDTest.Peek(app + 0x7c, '@i'))
check('CWinApp_IsDedicatedWin', lambda: SDTest.Call('CWinApp_IsDedicatedWin', 'tbi', app)[0])
check('CApp_IsDedicatedWin', lambda: SDTest.Call('CApp_IsDedicatedWin', 'tbi', app)[0])
check('App_CheckBlock', lambda: SDTest.Call('App_CheckBlock', 'b')[0])
check('World_TimeScale', lambda: SDTest.Call('World_TimeScale', 'f')[0])
check('app flags', lambda: (SDTest.Peek(app + 0x28, '@i'), SDTest.Peek(app + 0x2d, '@b'), SDTest.Peek(app + 0x30, '@i'),
	SDTest.Peek(app + 0x80, '@b'), SDTest.Peek(app + 0x81, '@b')))
for p in (('sdtest_none', '1'), ('SDTEST_NONE', '')):
	check('CApp_ParseParam' + fmt(p), lambda p=p: SDTest.Call('CApp_ParseParam', 'tbiss', app, p[0], p[1])[0])
	check('CWinApp_ParseParam' + fmt(p), lambda p=p: SDTest.Call('CWinApp_ParseParam', 'tbiss', app, p[0], p[1])[0])
if SDTest.Peek('g_pWorld', 'i'):
	check('App_GetObjData', lambda: SDTest.Call('App_GetObjData', 'is', 'SDTestNone')[0] != 0)
# the console window is closed: no window, nothing printed, 0
check('ConsoleWin state', lambda: (SDTest.Peek('g_ConOpen', 'i'), SDTest.Peek('g_ConHwnd', 'i') == 0))
if SDTest.Peek('g_ConOpen', 'i') == 0 and SDTest.Peek('g_ConHwnd', 'i') == 0:
	check('ConsoleWin_GetHwnd', lambda: SDTest.Call('ConsoleWin_GetHwnd', 'i')[0])
	check('ConsoleWin_Print', lambda: SDTest.Call('ConsoleWin_Print', 'isi', 'sdtest\n', 0xff)[0])
	check('ConsoleWin_Show', lambda: SDTest.Call('ConsoleWin_Show', 'ii', 1)[0])
	check('ConsoleWin_Close', lambda: SDTest.Call('ConsoleWin_Close', 'i')[0])
# the console commands on iLoadMsgString (rva 0x4679c0, the next loading tip), saved and restored
msg = SDTest.Peek('g_LoadMsgIndex', 'i')
for cmd in (':iLoadMsgString 7', ':iLoadMsgString', '?iLoadMsg', ':iLoadMsgString  "12" ',
		"Scrap.Set('iLoadMsgString',3)", ':iLoadMsgString 5'):
	check('Console_Execute' + fmt((cmd,)), lambda cmd=cmd: (SDTest.Call('Console_Execute', 'vs', cmd), SDTest.Peek('g_LoadMsgIndex', 'i')))
SDTest.Poke('g_LoadMsgIndex', msg)
