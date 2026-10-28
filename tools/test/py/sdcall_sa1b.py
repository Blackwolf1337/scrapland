# Standalone 1b (execfile'd by sdcall.py, Python 1.5.2): the shared stubs of src/game/Global/Stubs.c by some of
# their rvas, the Steam stubs, the constructors Ghidra missed, the GVars of the static initializers (read only: a
# second run would link them into g_GVars twice), the string and vector reallocation of the GOG code. Not called:
# Gog_Init (Galaxy is set up once), the atexit initializers, the Winsock thunks (stdcall, SDTest calls cdecl).
def sabz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def sabfree(p):
	SDTest.Call(0x133550, 'vi', p)
def sabi(a):
	return SDTest.Peek(a, '@i')
def sabset(a, s):
	SDTest.Call(0x3197d0, 'iisi', a, s, len(s))
def sabpoke(a, v):
	SDTest.Poke(a - 0x400000, v)

sabobj = sabz(0x40)
for sabr in (0x0d7650, 0x0e0f70, 0x1373f0, 0x278390):
	check('Stub_Nop' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tvi', sabobj))
for sabr in (0x0d81b0, 0x0e2710, 0x0dfc10):
	check('Stub_True' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tbi', sabobj)[0])
for sabr in (0x0d8670, 0x0e71d0, 0x2627e0, 0x27c8b0):
	check('Stub_Nop1' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tvii', sabobj, 5))
check('Stub_Nop2', lambda: SDTest.Call(0x27bff0, 'tviii', sabobj, 5, 6))
check('Stub_Nop3', lambda: SDTest.Call(0x27c8a0, 'tviiii', sabobj, 5, 6, 7))
check('Stub_Zero', lambda: SDTest.Call(0x1a7f10, 'i')[0])
for sabr in (0x144ae0, 0x249490, 0x258940):
	check('Stub_ReturnSelf' + fmt((sabr,)), lambda r=sabr: SDTest.Call(r, 'tii', sabobj)[0] == sabobj)

check('Steam_Done', lambda: SDTest.Call(0x1a64d0, 'v'))
check('Steam_GetLanguage', lambda: SDTest.Call(0x1a65f0, 's'))
check('Steam_Init', lambda: SDTest.Call(0x1a6620, 'bi', 0)[0])
check('Steam_SetAchievement', lambda: SDTest.Call(0x1a66a0, 'bii', 3, 1)[0])
check('Steam_RunCallbacks', lambda: SDTest.Call(0x1a66e0, 'v'))
check('Gog_Done', lambda: SDTest.Call(0x137430, 'tvi', sabobj))
check('InputMaps_Done', lambda: SDTest.Call(0x139900, 'v'))
check('ListaVert_ReleaseD3D', lambda: SDTest.Call(0x2404e0, 'v'))
check('ListaVert_InitD3D', lambda: SDTest.Call(0x2404f0, 'v'))
check('CEscena_Shared', lambda: SDTest.Call(0x24ecb0, 'tvi', sabobj))
check('CAnimCtrl_dtor', lambda: SDTest.Call(0x250f10, 'tvi', sabobj))
check('BspQ3_InitVertG', lambda: SDTest.Call(0x05f770, 'v'))

# constructors
sabset(sabobj, 'abcdefgh')
check('CTextEst_ctor', lambda: (SDTest.Call(0x233990, 'tii', sabobj)[0] == sabobj, sabi(sabobj), sabi(sabobj + 4)))
sabset(sabobj, 'abcdefgh')
check('CLod_ctor', lambda: (SDTest.Call(0x252a70, 'tii', sabobj)[0] == sabobj, sabi(sabobj), sabi(sabobj + 4)))
sabvk = sabz(0x110)
check('cVirtualKeyboard_ctor', lambda: (SDTest.Call(0x1392e0, 'tii', sabvk)[0] == sabvk, SDTest.Peek(sabvk, '@s'),
	sabi(sabvk + 0x100), sabi(sabvk + 0x104), sabi(sabvk + 0x108)))
sabfree(sabvk)
sabgm = sabz(0x1c)
check('CGameModel_ctorDefault', lambda: (SDTest.Call(0x105e60, 'tii', sabgm)[0] == sabgm, sabi(sabgm) == 0,
	sabi(sabgm + 0x18)))
SDTest.Call(0x20ec10, 'tvi', sabgm)
sabfree(sabgm)
check('MemPools_PrintAll thunk', lambda: SDTest.Call(0x193e10, 'v'))

# the int GVars of the static initializers
for sabv in (0x4e1ee0, 0x4e1f70, 0x4e6e00):
	check('GVar' + fmt((sabv,)), lambda v=sabv: (SDTest.Peek(v + 4, 'p'), SDTest.Peek(v + 8, 'p'),
		SDTest.Peek(v + 0xc, 'i') & 0xffff, SDTest.Peek(v + 0x10, 'i'), SDTest.Peek(v + 0x14, 'i')))

# std::string _Reallocate_for: from the small buffer, then grown by half
sabs = sabz(0x18)
sabset(sabs, 'abc')
sabpoke(sabs + 0x10, 3)
sabpoke(sabs + 0x14, 15)
def sabstr(s):
	cap = sabi(s + 0x14)
	if cap >= 16:
		return (sabi(s + 0x10), cap, SDTest.Peek(s, '@p'))
	return (sabi(s + 0x10), cap, SDTest.Peek(s, '@s'))
check('MsvcString small', lambda: sabstr(sabs))
check('MsvcString_ReallocFor(40, 3)', lambda: (SDTest.Call(0x1375f0, 'tviii', sabs, 40, 3), sabstr(sabs)))
check('MsvcString_ReallocFor(50, 2)', lambda: (SDTest.Call(0x1375f0, 'tviii', sabs, 50, 2), sabstr(sabs)))
check('MsvcString_ReallocFor(10, 0)', lambda: (SDTest.Call(0x1375f0, 'tviii', sabs, 10, 0), sabstr(sabs)))
check('MsvcString_Tidy', lambda: (SDTest.Call(0x136f40, 'tvi', sabs), sabi(sabs + 0x10), sabi(sabs + 0x14)))
sabfree(sabs)

# vector<unique_ptr> _Reallocate: an empty one, then with two (fake, never deleted: the old slots are NULL after
# the move) pointers
sabv = sabz(0x0c)
def sabvec(v):
	b = sabi(v)
	if not b:
		return (0,)
	n = (sabi(v + 4) - b) / 4
	r = [n, (sabi(v + 8) - b) / 4]
	for i in range(n):
		r.append(sabi(b + 4 * i))
	return tuple(r)
check('PtrVector_Reallocate(4) empty', lambda: (SDTest.Call(0x137790, 'tvii', sabv, 4), sabvec(sabv)))
sabpoke(sabi(sabv), 0x11)
sabpoke(sabi(sabv) + 4, 0x22)
sabpoke(sabv + 4, sabi(sabv) + 8)
check('PtrVector_Reallocate(9)', lambda: (SDTest.Call(0x137790, 'tvii', sabv, 9), sabvec(sabv)))
SDTest.Call(0x137c30, 'tviii', sabv, sabi(sabv), (sabi(sabv + 8) - sabi(sabv)) / 4)
sabfree(sabv)
sabfree(sabobj)
