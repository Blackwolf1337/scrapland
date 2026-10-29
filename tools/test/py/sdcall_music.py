# game engine3d/mpeg/MusicFmod.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the FMOD state, the
# error texts of the copy in music_fmod.cpp, the voices without a stream (the menu opens none), the music with no
# stream and with a fake one (a stream pointer FMOD never sees, a channel that does not exist), the volume fades with
# a fixed frame time, the pause of the voices and the file callbacks (stdcall, called as thiscall with a dummy this).
# Not called: Voice_Open, Music_Open and the PlayFile of a new file (they open a stream that FMOD opens in the
# background), Voice_Unused (CHECK(0)), the screen debug info (S_Show 2). The music and voice globals, S_Show, the
# frame time and the fps are put back.
def mui(a):
	return SDTest.Peek(a, '@i')
def muf(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def muset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def mupoke(a, v):
	SDTest.Poke(a - 0x400000, v)

MUVOICES = symva('g_MusicVoices')
muvsize = 0x19c
# the music state: 0x831d04..0x831d1c, 0x8a94a8, 0x866c70, 0x8aa2f4..0x8aa30c, S_Show, frame time, fps
mustate = []
for a in (symva('g_MusicVolume'), symva('g_MusicVoiceover'), symva('g_MusicChannel'), symva('g_MusicVoiceoverTarget'), symva('g_MusicVoiceoverSpeed'), symva('g_MusicFadeTarget'), symva('g_MusicFadeSpeed'), symva('g_MusicCurMainVolume'), symva('g_MusicMainVolume'), symva('g_MusicStream'), symva('g_MusicPaused'),
		symva('g_MusicOpenTime'), symva('g_MusicWaitTime'), symva('g_NumVoices'), symva('g_NumVoiceovers'), symva('g_VoicesPaused'), symva('g_S_Show'), symva('g_RealFrameSecs'), symva('g_Fps')):
	mustate.append((a, mui(a)))
muname = SDTest.Peek('g_MusicName', 's')
muvoice = []
for i in range(8):
	v = MUVOICES + i * muvsize
	muvoice.append((mui(v + 0x104), mui(v + 0x108), mui(v + 0x18c), SDTest.Peek(v + 0x10c - 0x400000, 's')))
mupoke(symva('g_S_Show'), 0)

check('Fmod_State', lambda: SDTest.Peek('g_FmodState', 'i'))
check('Fmod_IsInit', lambda: SDTest.Call('Fmod_IsInit', 'i')[0])
if SDTest.Peek('g_FmodState', 'i') != 0:
	check('Fmod_Init again', lambda: SDTest.Call('Fmod_Init', 'b')[0])
for e in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100):
	check('Fmod_ErrorString music' + fmt((e,)), lambda e=e: SDTest.Call('Fmod_ErrorString@2311e0', 'si', e)[0])
fmodon = SDTest.Peek('g_FmodState', 'i') > 0

# the voices without a stream
for i in range(8):
	check('Voice_Get' + fmt((i,)), lambda i=i: SDTest.Call('Voice_Get', 'ii', i)[0] == (MUVOICES + i * muvsize) * fmodon)
for i in (0, 3, 7):
	v = MUVOICES + i * muvsize
	if muvoice[i][0] != 0:
		check('Voice busy' + fmt((i,)), lambda: 1)
		continue
	check('Voice_GetString' + fmt((i,)), lambda i=i: SDTest.Call('Voice_GetString', 'bibi', i, 'junk', 32))
	check('Voice_SetString' + fmt((i,)), lambda i=i, v=v: (SDTest.Call('Voice_SetString', 'bis', i, 'hello'),
		SDTest.Peek(v + 0x10c - 0x400000, 's')))
	check('Voice_SetParam' + fmt((i,)), lambda i=i: SDTest.Call('Voice_SetParam', 'bif', i, 0.5))
	check('Voice_SetVolume' + fmt((i,)), lambda i=i: SDTest.Call('Voice_SetVolume', 'bif', i, 0.5))
	check('Voice_Remain' + fmt((i,)), lambda i=i: SDTest.Call('Voice_Remain', 'bip', i, 12345))
	check('Voice_Play' + fmt((i,)), lambda i=i, v=v: (SDTest.Call('Voice_Play', 'bi', i), mui(v + 0x108)))
	mupoke(v + 0x18c, 1)
	SDTest.Call('String_Copy', 'iisi', v + 0x10c, 'text', 0x80)
	check('Voice_Stop' + fmt((i,)), lambda i=i, v=v: (SDTest.Call('Voice_Stop', 'bi', i), mui(v + 0x18c),
		SDTest.Peek(v + 0x10c - 0x400000, 's')))
	mupoke(v + 0x18c, 1)
	SDTest.Call('String_Copy', 'iisi', v + 0x10c, 'text', 0x80)
	check('Voice_Close' + fmt((i,)), lambda v=v: (SDTest.Call('Voice_Close', 'vi', v), mui(v + 0x18c),
		SDTest.Peek(v + 0x10c - 0x400000, 's')))
	check('Voice_ApplyVolume' + fmt((i,)), lambda v=v: SDTest.Call('Voice_ApplyVolume', 'vif', v, 0.25))

# the pause of the voices: g_VoicesPaused flipped so that the update writes it
for p in (0, 1):
	mupoke(symva('g_VoicesPaused'), p)
	check('Voices_UpdatePause' + fmt((p,)), lambda: (SDTest.Call('Voices_UpdatePause', 'v'), mui(symva('g_VoicesPaused')) & 0xff))
	mupoke(symva('g_VoicesPaused'), p)
	check('D3DApp_OnActivate' + fmt((p,)), lambda: (SDTest.Call('D3DApp_OnActivate', 'v'), mui(symva('g_VoicesPaused')) & 0xff))

def mumusic():
	return (muf(symva('g_MusicVolume'), 1) + muf(symva('g_MusicVoiceover'), 1), mui(symva('g_MusicChannel')), muf(symva('g_MusicVoiceoverTarget'), 1) + muf(symva('g_MusicVoiceoverSpeed'), 1) + muf(symva('g_MusicFadeTarget'), 1) + muf(symva('g_MusicFadeSpeed'), 1), muf(symva('g_MusicCurMainVolume'), 1), muf(symva('g_MusicMainVolume'), 1), mui(symva('g_MusicPaused')))
def muinit(stream, channel):
	mupoke(symva('g_MusicStream'), stream)
	mupoke(symva('g_MusicChannel'), channel)
	muset(symva('g_MusicVolume'), (0.8,))
	muset(symva('g_MusicVoiceover'), (0.5,))
	muset(symva('g_MusicChannel'), (0.0,))
	muset(symva('g_MusicVoiceoverTarget'), (0.3,))
	muset(symva('g_MusicVoiceoverSpeed'), (2.0,))
	muset(symva('g_MusicFadeTarget'), (0.1,))
	muset(symva('g_MusicFadeSpeed'), (0.5,))
	mupoke(symva('g_MusicChannel'), channel)
	muset(symva('g_MusicCurMainVolume'), (0.7,))
	muset(symva('g_MusicMainVolume'), (0.9,))
	mupoke(symva('g_MusicPaused'), 0)

# the music without a stream and with a fake one
for stream, channel, name in ((0, -1, 'none'), (0x12345, 1000, 'fake')):
	muinit(stream, channel)
	check('Music_Ready ' + name, lambda: SDTest.Call('Music_Ready', 'b')[0])
	check('Music_ApplyVolume ' + name, lambda: (SDTest.Call('Music_ApplyVolume', 'v'), mumusic()))
	for p in (1, 0, 2):
		check('Music_SetPaused ' + name + fmt((p,)), lambda p=p: (SDTest.Call('Music_SetPaused', 'vi', p), mui(symva('g_MusicPaused'))))
	check('Music_Pause ' + name, lambda: (SDTest.Call('Music_Pause', 'b')[0], mui(symva('g_MusicPaused'))))
	check('Music_PlayFile unpause ' + name, lambda: (SDTest.Call('Music_PlayFile', 'bzz', None, None)[0], mui(symva('g_MusicPaused'))))
	mupoke(symva('g_MusicPaused'), 1)
	check('Music_PlayFile empty ' + name, lambda: (SDTest.Call('Music_PlayFile', 'bsz', '', 'music/')[0], mui(symva('g_MusicPaused'))))
	for v in (0.0, 0.25, 1.0, 1.5, -0.5):
		check('Music_SetVolume ' + name + fmt((v,)), lambda v=v: (SDTest.Call('Music_SetVolume', 'bf', v)[0], mumusic()))
		check('Music_SetMainVolume ' + name + fmt((v,)), lambda v=v: (SDTest.Call('Music_SetMainVolume', 'bf', v)[0], mumusic()))
	for a, b in ((0.0, 0.5), (1.0, 2.0), (0.3, 0.0)):
		check('Music_FadeOut ' + name + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('Music_FadeOut', 'bff', a, b)[0],
			mumusic()))
	for a, b in ((0.3, 4.0), (1.0, 2.0), (0.5, 0.0), (0.5, -1.0), (2.0, 0.0001)):
		check('Music_Voiceover ' + name + fmt((a, b)), lambda a=a, b=b: (SDTest.Call('Music_Voiceover', 'bff', a, b)[0],
			mumusic()))
	for dt, fps in ((0.02, 30.0), (0.5, 5.0), (0.0, 60.0), (0.1, 0.0), (1.0, 100.0)):
		muinit(stream, channel)
		muset(symva('g_RealFrameSecs'), (dt,))
		muset(symva('g_Fps'), (fps,))
		check('Music_UpdateVolumes ' + name + fmt((dt, fps)), lambda: (SDTest.Call('Music_UpdateVolumes', 'b')[0], mumusic()))
		check('Music_UpdateVolumes 2 ' + name + fmt((dt, fps)), lambda: (SDTest.Call('Music_UpdateVolumes', 'b')[0], mumusic()))
		check('Music_Update ' + name + fmt((dt, fps)), lambda: (SDTest.Call('Music_Update', 'b')[0], mumusic()))
	muinit(stream, channel)
	check('Voices_DrawDebugDefault ' + name, lambda: SDTest.Call('Voices_DrawDebugDefault', 'b')[0])
	check('Music_DrawDebug ' + name, lambda: SDTest.Call('Music_DrawDebug', 'vii', 10, 20))

# no stream: start, wait and stop return at once
muinit(0, -1)
check('Music_Start none', lambda: SDTest.Call('Music_Start', 'b')[0])
check('Music_Wait none', lambda: SDTest.Call('Music_Wait', 'b')[0])
check('Music_Stop none', lambda: (SDTest.Call('Music_Stop', 'b')[0], mui(symva('g_MusicStream')), mui(symva('g_MusicChannel'))))
# the fake stream with the same name: "already started", unpaused
muinit(0x12345, 1000)
SDTest.Call('String_Copy', 'iisi', symva('g_MusicName'), 'music/Test.ogg', 0x104)
mupoke(symva('g_MusicPaused'), 1)
check('Music_PlayFile same', lambda: (SDTest.Call('Music_PlayFile', 'bss', 'test.OGG', 'MUSIC/')[0], mui(symva('g_MusicPaused')),
	SDTest.Peek('g_MusicName', 's')))

# the file callbacks
check('Music_FileOpen missing', lambda: SDTest.Call('Music_FileOpen', 'tiis', 0, 'no/such/file.ogg')[0])
muf1 = SDTest.Call('Music_FileOpen', 'tiis', 0, 'sdcall.py')[0]
check('Music_FileOpen', lambda: muf1 != 0)
if muf1:
	check('Music_FileTell', lambda: SDTest.Call('Music_FileTell', 'tiii', 0, muf1)[0])
	check('Music_FileRead', lambda: SDTest.Call('Music_FileRead', 'tiibii', 0, '', 8, muf1)[0:2])
	check('Music_FileSeek', lambda: SDTest.Call('Music_FileSeek', 'tiiiii', 0, muf1, 3, 0x100)[0])
	check('Music_FileTell 2', lambda: SDTest.Call('Music_FileTell', 'tiii', 0, muf1)[0])
	check('Music_FileRead 2', lambda: SDTest.Call('Music_FileRead', 'tiibii', 0, '', 4, muf1)[0:2])
	check('Music_FileClose', lambda: SDTest.Call('Music_FileClose', 'tiii', 0, muf1)[0])
check('Music_True16a', lambda: SDTest.Call('Music_True16a', 'tbiiiii', 0, 1, 2, 3, 4)[0])
check('Music_True16b', lambda: SDTest.Call('Music_True16b', 'tbiiiii', 0, 1, 2, 3, 4)[0])
check('Music_Nop', lambda: SDTest.Call('Music_Nop', 'v')[0])

# back
for a, v in mustate:
	mupoke(a, v)
SDTest.Call('String_Copy', 'iisi', symva('g_MusicName'), muname, 0x104)
for i in range(8):
	v = MUVOICES + i * muvsize
	mupoke(v + 0x104, muvoice[i][0])
	mupoke(v + 0x108, muvoice[i][1])
	mupoke(v + 0x18c, muvoice[i][2])
	SDTest.Call('String_Copy', 'iisi', v + 0x10c, muvoice[i][3], 0x80)
