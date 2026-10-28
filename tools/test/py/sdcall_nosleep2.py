# The other Sleep and blocking wait rewrites (MusicFmod.c; execfile'd by sdcall.py, Python 1.5.2). A voice opens at
# once now (the original opened it in the background and waited for it in Voice_Remain and Voice_Play), Music_Stop
# closes a music stream FMOD still opens without the wait and the start (the original waited at least a second). The
# results are the same in both runs: the length of the voice and the state after the stop. No timing values are
# printed. Not called: the splash screen loop, the TXF cache of the light map compiler (file writes), CWinApp_Frame
# and CD3DApp_Run (the frame and the message loop), the keyboard Acquire (sdcall_eng3.py reads the keys).
def ns2i(a):
	return SDTest.Peek(a, '@i')

NS2VOICE = 7
ns2v = symva('g_MusicVoices') + NS2VOICE * 0x19c
# the music and voice globals the calls change: 0x831d04..0x831d1c, the stream, paused, the counts of the voices
ns2state = []
for ns2a in (symva('g_MusicVolume'), symva('g_MusicVoiceover'), symva('g_MusicChannel'), symva('g_MusicVoiceoverTarget'), symva('g_MusicVoiceoverSpeed'), symva('g_MusicFadeTarget'), symva('g_MusicFadeSpeed'), symva('g_MusicStream'), symva('g_MusicPaused'), symva('g_NumVoices'),
		symva('g_NumVoiceovers')):
	ns2state.append((ns2a, ns2i(ns2a)))
ns2name = SDTest.Peek('g_MusicName', 's')
ns2fmod = SDTest.Peek('g_FmodState', 'i') > 0

check('ns2 Voice_Wait none', lambda: SDTest.Call('Voice_Wait', 'vii', ns2v, 0)[0])

# a voice of English.packed
ns2file = 'Sounds/Voices/challengecombatbertomsg1'
ns2pack = SDTest.Call('Pack_GetFileInfo', 'bsbpp', ns2file + '.ogg', '', 0, 0)
check('ns2 Pack_GetFileInfo', lambda: (ns2pack[0] != 0, ns2pack[3]))
if ns2fmod and ns2pack[0] and ns2i(ns2v + 0x104) == 0:
	check('ns2 Voice_Open', lambda: (SDTest.Call('Voice_Open', 'isii', ns2file, NS2VOICE, 0)[0], ns2i(ns2v + 0x104) != 0,
		ns2i(ns2v + 0x108)))
	check('ns2 Voice_Remain', lambda: SDTest.Call('Voice_Remain', 'biF', NS2VOICE, (0.0,)))
	check('ns2 Voice_SetParam before', lambda: SDTest.Call('Voice_SetParam', 'bif', NS2VOICE, 0.5)[0])
	check('ns2 Voice_Play', lambda: (SDTest.Call('Voice_Play', 'bi', NS2VOICE)[0], ns2i(ns2v + 0x108) >= 0))
	check('ns2 Voice_SetVolume', lambda: SDTest.Call('Voice_SetVolume', 'bif', NS2VOICE, 0.0)[0])
	check('ns2 Voice_Stop', lambda: (SDTest.Call('Voice_Stop', 'bi', NS2VOICE)[0], ns2i(ns2v + 0x104), ns2i(ns2v + 0x108)))
else:
	check('ns2 Voice skipped', lambda: (ns2fmod, ns2i(ns2v + 0x104) == 0))

# a music file of Data.packed, stopped right after the open; the music of the menu is stopped first and started
# again at the end
ns2music = ns2fmod and (ns2i(symva('g_MusicStream')) == 0 or ns2name != '')
if ns2music:
	check('ns2 Music_Stop menu', lambda: (SDTest.Call('Music_Stop', 'b')[0], ns2i(symva('g_MusicStream')), ns2i(symva('g_MusicChannel'))))
	check('ns2 Music_PlayFile', lambda: (SDTest.Call('Music_PlayFile', 'bss', 'epic.ogg', 'Music/')[0], ns2i(symva('g_MusicStream')) != 0,
		SDTest.Peek('g_MusicName', 's')))
	check('ns2 Music_Stop', lambda: (SDTest.Call('Music_Stop', 'b')[0], ns2i(symva('g_MusicStream')), ns2i(symva('g_MusicChannel'))))
	check('ns2 Music_Update', lambda: SDTest.Call('Music_Update', 'b')[0])
else:
	check('ns2 Music skipped', lambda: (ns2fmod, ns2i(symva('g_MusicStream')) != 0))

# back
for ns2a, ns2x in ns2state:
	if not ns2music or ns2a not in (symva('g_MusicStream'), symva('g_MusicChannel')):
		SDTest.Poke(ns2a - 0x400000, ns2x)
if ns2music and ns2state[7][1] != 0:
	check('ns2 Music_PlayFile menu', lambda: (SDTest.Call('Music_PlayFile', 'bss', ns2name, '')[0], ns2i(symva('g_MusicStream')) != 0))
else:
	SDTest.Call('Cad_Copy', 'iisi', symva('g_MusicName'), ns2name, 0x104)
