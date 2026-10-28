# The other Sleep and blocking wait rewrites (MusicFmod.c; execfile'd by sdcall.py, Python 1.5.2). A voice opens at
# once now (the original opened it in the background and waited for it in Voice_Remain and Voice_Play), Music_Stop
# closes a music stream FMOD still opens without the wait and the start (the original waited at least a second). The
# results are the same in both runs: the length of the voice and the state after the stop. No timing values are
# printed. Not called: the splash screen loop, the TXF cache of the light map compiler (file writes), CWinApp_Frame
# and CD3DApp_Run (the frame and the message loop), the keyboard Acquire (sdcall_eng3.py reads the keys).
def ns2i(a):
	return SDTest.Peek(a, '@i')

NS2VOICE = 7
ns2v = 0x8a94e0 + NS2VOICE * 0x19c
# the music and voice globals the calls change: 0x831d04..0x831d1c, the stream, paused, the counts of the voices
ns2state = []
for ns2a in (0x831d04, 0x831d08, 0x831d0c, 0x831d10, 0x831d14, 0x831d18, 0x831d1c, 0x8aa2f4, 0x8aa2f8, 0x8aa304,
		0x8aa308):
	ns2state.append((ns2a, ns2i(ns2a)))
ns2name = SDTest.Peek(0x4aa1f0, 's')
ns2fmod = SDTest.Peek(0x4a94dc, 'i') > 0

check('ns2 Voice_Wait none', lambda: SDTest.Call(0x232120, 'vii', ns2v, 0)[0])

# a voice of English.packed
ns2file = 'Sounds/Voices/challengecombatbertomsg1'
ns2pack = SDTest.Call(0x130240, 'bsbpp', ns2file + '.ogg', '', 0, 0)
check('ns2 Pack_GetFileInfo', lambda: (ns2pack[0] != 0, ns2pack[3]))
if ns2fmod and ns2pack[0] and ns2i(ns2v + 0x104) == 0:
	check('ns2 Voice_Open', lambda: (SDTest.Call(0x2328e0, 'isii', ns2file, NS2VOICE, 0)[0], ns2i(ns2v + 0x104) != 0,
		ns2i(ns2v + 0x108)))
	check('ns2 Voice_Remain', lambda: SDTest.Call(0x232c60, 'biF', NS2VOICE, (0.0,)))
	check('ns2 Voice_SetParam before', lambda: SDTest.Call(0x232a80, 'bif', NS2VOICE, 0.5)[0])
	check('ns2 Voice_Play', lambda: (SDTest.Call(0x232b40, 'bi', NS2VOICE)[0], ns2i(ns2v + 0x108) >= 0))
	check('ns2 Voice_SetVolume', lambda: SDTest.Call(0x232dd0, 'bif', NS2VOICE, 0.0)[0])
	check('ns2 Voice_Stop', lambda: (SDTest.Call(0x232d80, 'bi', NS2VOICE)[0], ns2i(ns2v + 0x104), ns2i(ns2v + 0x108)))
else:
	check('ns2 Voice skipped', lambda: (ns2fmod, ns2i(ns2v + 0x104) == 0))

# a music file of Data.packed, stopped right after the open; the music of the menu is stopped first and started
# again at the end
ns2music = ns2fmod and (ns2i(0x8aa2f4) == 0 or ns2name != '')
if ns2music:
	check('ns2 Music_Stop menu', lambda: (SDTest.Call(0x232360, 'b')[0], ns2i(0x8aa2f4), ns2i(0x831d0c)))
	check('ns2 Music_PlayFile', lambda: (SDTest.Call(0x232240, 'bss', 'epic.ogg', 'Music/')[0], ns2i(0x8aa2f4) != 0,
		SDTest.Peek(0x4aa1f0, 's')))
	check('ns2 Music_Stop', lambda: (SDTest.Call(0x232360, 'b')[0], ns2i(0x8aa2f4), ns2i(0x831d0c)))
	check('ns2 Music_Update', lambda: SDTest.Call(0x232600, 'b')[0])
else:
	check('ns2 Music skipped', lambda: (ns2fmod, ns2i(0x8aa2f4) != 0))

# back
for ns2a, ns2x in ns2state:
	if not ns2music or ns2a not in (0x8aa2f4, 0x831d0c):
		SDTest.Poke(ns2a - 0x400000, ns2x)
if ns2music and ns2state[7][1] != 0:
	check('ns2 Music_PlayFile menu', lambda: (SDTest.Call(0x232240, 'bss', ns2name, '')[0], ns2i(0x8aa2f4) != 0))
else:
	SDTest.Call(0x22cf50, 'iisi', 0x8aa1f0, ns2name, 0x104)
