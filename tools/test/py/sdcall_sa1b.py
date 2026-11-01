# Standalone 1b (execfile'd by sdcall.py, Python 1.5.2): the shared stubs of src/game/Global/Stubs.c by some of
# their rvas, the constructors Ghidra missed, the GVars of the static initializers (read only: a second run would
# link them into g_GVars twice). Not called: the atexit initializers, the Winsock thunks (stdcall, SDTest calls cdecl).
def sabz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def sabfree(p):
	SDTest.Call('Mem_Free', 'vi', p)
def sabi(a):
	return SDTest.Peek(a, '@i')
def sabset(a, s):
	SDTest.Call('crt_memcpy', 'iisi', a, s, len(s))

sabobj = sabz(0x40)
for sabr in ('Stub_Nop', 'Stub_Nop@e0f70', 'CFog_dtor'):
	check('Stub_Nop' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tvi', sabobj))
for sabr in ('Stub_True', 'Stub_True@e2710', 'Stub_True@dfc10'):
	check('Stub_True' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tbi', sabobj)[0])
for sabr in ('Stub_Nop1', 'Stub_Nop1@e71d0', 'Stub_Nop1@2627e0', 'Stub_Nop1@27c8b0'):
	check('Stub_Nop1' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tvii', sabobj, 5))
check('Stub_Nop2', lambda: SDTest.Call('Stub_Nop2', 'tviii', sabobj, 5, 6))
check('Stub_Nop3', lambda: SDTest.Call('Stub_Nop3', 'tviiii', sabobj, 5, 6, 7))
check('Stub_Zero', lambda: SDTest.Call('Stub_Zero', 'i')[0])
for sabr in ('Stub_ReturnSelf', 'Stub_ReturnSelf@249490', 'Stub_ReturnSelf@258940'):
	check('Stub_ReturnSelf' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tii', sabobj)[0] == sabobj)

check('InputMaps_Done', lambda: SDTest.Call('InputMaps_Done', 'v'))
check('VertList_ReleaseD3D', lambda: SDTest.Call('VertList_ReleaseD3D', 'v'))
check('VertList_InitD3D', lambda: SDTest.Call('VertList_InitD3D', 'v'))
check('CScene_Shared', lambda: SDTest.Call('CScene_Shared', 'tvi', sabobj))
check('CAnimCtrl_dtor', lambda: SDTest.Call('CAnimCtrl_dtor', 'tvi', sabobj))
check('BspQ3_InitVertG', lambda: SDTest.Call('BspQ3_InitVertG', 'v'))

# constructors
sabset(sabobj, 'abcdefgh')
check('CTextStatic_ctor', lambda: (SDTest.Call('CTextStatic_ctor', 'tii', sabobj)[0] == sabobj, sabi(sabobj), sabi(sabobj + 4)))
sabset(sabobj, 'abcdefgh')
check('CLod_ctor', lambda: (SDTest.Call('CLod_ctor', 'tii', sabobj)[0] == sabobj, sabi(sabobj), sabi(sabobj + 4)))
sabvk = sabz(0x110)
check('cVirtualKeyboard_ctor', lambda: (SDTest.Call('cVirtualKeyboard_ctor', 'tii', sabvk)[0] == sabvk, SDTest.Peek(sabvk, '@s'),
	sabi(sabvk + 0x100), sabi(sabvk + 0x104), sabi(sabvk + 0x108)))
sabfree(sabvk)
sabgm = sabz(0x1c)
check('CGameModel_ctorDefault', lambda: (SDTest.Call('CGameModel_ctorDefault', 'tii', sabgm)[0] == sabgm, sabi(sabgm) == 0,
	sabi(sabgm + 0x18)))
SDTest.Call('CGameModel_Reset', 'tvi', sabgm)
sabfree(sabgm)
check('MemPools_PrintAll thunk', lambda: SDTest.Call('MemPools_PrintAll@193e10', 'v'))

# the int GVars of the static initializers
for sabv in (sym('g_ShowNodesTreeVar'), sym('g_UseWSMVar'), sym('g_FastGetYSectorVar')):
	check('GVar' + fmt((sabv,)), lambda v=sabv: (SDTest.Peek(v + 4, 'p'), SDTest.Peek(v + 8, 'p'),
		SDTest.Peek(v + 0xc, 'i') & 0xffff, SDTest.Peek(v + 0x10, 'i'), SDTest.Peek(v + 0x14, 'i')))

sabfree(sabobj)
