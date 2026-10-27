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
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def muset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def mupoke(a, v):
	SDTest.Poke(a - 0x400000, v)

MUVOICES = 0x8a94e0
muvsize = 0x19c
# the music state: 0x831d04..0x831d1c, 0x8a94a8, 0x866c70, 0x8aa2f4..0x8aa30c, S_Show, frame time, fps
mustate = []
for a in (0x831d04, 0x831d08, 0x831d0c, 0x831d10, 0x831d14, 0x831d18, 0x831d1c, 0x8a94a8, 0x866c70, 0x8aa2f4, 0x8aa2f8,
		0x8aa2fc, 0x8aa300, 0x8aa304, 0x8aa308, 0x8aa30c, 0x866d90, 0x8a8ee4, 0x831ba8):
	mustate.append((a, mui(a)))
muname = SDTest.Peek(0x4aa1f0, 's')
muvoice = []
for i in range(8):
	v = MUVOICES + i * muvsize
	muvoice.append((mui(v + 0x104), mui(v + 0x108), mui(v + 0x18c), SDTest.Peek(v + 0x10c - 0x400000, 's')))
mupoke(0x866d90, 0)

check('Fmod_State', lambda: SDTest.Peek(0x4a94dc, 'i'))
check('Fmod_IsInit', lambda: SDTest.Call(0x2325f0, 'i')[0])
if SDTest.Peek(0x4a94dc, 'i') != 0:
	check('Fmod_Init again', lambda: SDTest.Call(0x232640, 'b')[0])
	check('Fmod_Init body again', lambda: SDTest.Call(0x2312d0, 'b')[0])
for e in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100):
	check('Fmod_ErrorString music' + fmt((e,)), lambda e=e: SDTest.Call(0x2311e0, 'si', e)[0])
fmodon = SDTest.Peek(0x4a94dc, 'i') > 0

# the voices without a stream
for i in range(8):
	check('Voice_Get' + fmt((i,)), lambda i=i: SDTest.Call(0x2314c0, 'ii', i)[0] == (MUVOICES + i * muvsize) * fmodon)
for i in (0, 3, 7):
	v = MUVOICES + i * muvsize
	if muvoice[i][0] != 0:
		check('Voice busy' + fmt((i,)), lambda: 1)
		continue
	check('Voice_GetString' + fmt((i,)), lambda i=i: SDTest.Call(0x2327d0, 'iibi', i, 'junk', 32))
	check('Voice_SetString' + fmt((i,)), lambda i=i, v=v: (SDTest.Call(0x232d10, 'iis', i, 'hello'),
		SDTest.Peek(v + 0x10c - 0x400000, 's')))
	check('Voice_SetParam' + fmt((i,)), lambda i=i: SDTest.Call(0x232a80, 'iif', i, 0.5))
	check('Voice_SetVolume' + fmt((i,)), lambda i=i: SDTest.Call(0x232dd0, 'iif', i, 0.5))
	check('Voice_Remain' + fmt((i,)), lambda i=i: SDTest.Call(0x232c60, 'iip', i, 12345))
	check('Voice_Play' + fmt((i,)), lambda i=i, v=v: (SDTest.Call(0x232b40, 'ii', i), mui(v + 0x108)))
	mupoke(v + 0x18c, 1)
	SDTest.Call(0x22cf50, 'iisi', v + 0x10c, 'text', 0x80)
	check('Voice_Stop' + fmt((i,)), lambda i=i, v=v: (SDTest.Call(0x232d80, 'ii', i), mui(v + 0x18c),
		SDTest.Peek(v + 0x10c - 0x400000, 's')))
	mupoke(v + 0x18c, 1)
	SDTest.Call(0x22cf50, 'iisi', v + 0x10c, 'text', 0x80)
	check('Voice_Close' + fmt((i,)), lambda v=v: (SDTest.Call(0x231c50, 'vi', v), mui(v + 0x18c),
		SDTest.Peek(v + 0x10c - 0x400000, 's')))
	check('Voice_ApplyVolume' + fmt((i,)), lambda v=v: SDTest.Call(0x231d20, 'vif', v, 0.25))

# the pause of the voices: g_VoicesPaused flipped so that the update writes it
for p in (0, 1):
	mupoke(0x8aa30c, p)
	check('Voices_UpdatePause' + fmt((p,)), lambda: (SDTest.Call(0x231d80, 'v'), mui(0x8aa30c) & 0xff))
	mupoke(0x8aa30c, p)
	check('D3DApp_OnActivate' + fmt((p,)), lambda: (SDTest.Call(0x2325e0, 'v'), mui(0x8aa30c) & 0xff))

def mumusic():
	return (muf(0x831d04, 2), mui(0x831d0c), muf(0x831d10, 4), muf(0x8a94a8, 1), muf(0x866c70, 1), mui(0x8aa2f8))
def muinit(stream, channel):
	mupoke(0x8aa2f4, stream)
	mupoke(0x831d0c, channel)
	muset(0x831d04, (0.8, 0.5, 0.0, 0.3, 2.0, 0.1, 0.5))
	mupoke(0x831d0c, channel)
	muset(0x8a94a8, (0.7,))
	muset(0x866c70, (0.9,))
	mupoke(0x8aa2f8, 0)

# the music without a stream and with a fake one
for stream, channel, name in ((0, -1, 'none'), (0x12345, 1000, 'fake')):
	muinit(stream, channel)
	check('Music_Ready ' + name, lambda: SDTest.Call(0x231550, 'b')[0])
	check('Music_ApplyVolume ' + name, lambda: (SDTest.Call(0x231ce0, 'v'), mumusic()))
	for p in (1, 0, 2):
		check('Music_SetPaused ' + name + fmt((p,)), lambda p=p: (SDTest.Call(0x231af0, 'vi', p), mui(0x8aa2f8)))
	check('Music_Pause ' + name, lambda: (SDTest.Call(0x232210, 'b')[0], mui(0x8aa2f8)))
	check('Music_PlayFile unpause ' + name, lambda: (SDTest.Call(0x232240, 'bzz', None, None)[0], mui(0x8aa2f8)))
	mupoke(0x8aa2f8, 1)
	check('Music_PlayFile empty ' + name, lambda: (SDTest.Call(0x232240, 'bsz', '', 'music/')[0], mui(0x8aa2f8)))
	for v in (0.0, 0.25, 1.0, 1.5, -0.5):
		check('Music_SetVolume ' + name + fmt((v,)), lambda v=v: (SDTest.Call(0x2324e0, 'bf', v)[0], mumusic()))
		check('Music_SetMainVolume ' + name + fmt((v,)), lambda v=v: (SDTest.Call(0x2321c0, 'bf', v)[0], mumusic()))
	for a, b in ((0.0, 0.5), (1.0, 2.0), (0.3, 0.0)):
		check('Music_FadeOut ' + name + fmt((a, b)), lambda a=a, b=b: (SDTest.Call(0x232480, 'bff', a, b)[0],
			mumusic()))
	for a, b in ((0.3, 4.0), (1.0, 2.0), (0.5, 0.0), (0.5, -1.0), (2.0, 0.0001)):
		check('Music_Voiceover ' + name + fmt((a, b)), lambda a=a, b=b: (SDTest.Call(0x2323c0, 'bff', a, b)[0],
			mumusic()))
	for dt, fps in ((0.02, 30.0), (0.5, 5.0), (0.0, 60.0), (0.1, 0.0), (1.0, 100.0)):
		muinit(stream, channel)
		muset(0x8a8ee4, (dt,))
		muset(0x831ba8, (fps,))
		check('Music_UpdateVolumes ' + name + fmt((dt, fps)), lambda: (SDTest.Call(0x2315d0, 'b')[0], mumusic()))
		check('Music_UpdateVolumes 2 ' + name + fmt((dt, fps)), lambda: (SDTest.Call(0x2315d0, 'b')[0], mumusic()))
		check('Music_Update ' + name + fmt((dt, fps)), lambda: (SDTest.Call(0x232600, 'b')[0], mumusic()))
	muinit(stream, channel)
	check('Voices_DrawDebugDefault ' + name, lambda: SDTest.Call(0x231e30, 'b')[0])
	check('Music_DrawDebug ' + name, lambda: SDTest.Call(0x231770, 'vii', 10, 20))

# no stream: start, wait and stop return at once
muinit(0, -1)
check('Music_Start none', lambda: SDTest.Call(0x231b20, 'b')[0])
check('Music_Wait none', lambda: SDTest.Call(0x231950, 'b')[0])
check('Music_Stop none', lambda: (SDTest.Call(0x232360, 'b')[0], mui(0x8aa2f4), mui(0x831d0c)))
# the fake stream with the same name: "already started", unpaused
muinit(0x12345, 1000)
SDTest.Call(0x22cf50, 'iisi', 0x8aa1f0, 'music/Test.ogg', 0x104)
mupoke(0x8aa2f8, 1)
check('Music_PlayFile same', lambda: (SDTest.Call(0x232240, 'bss', 'test.OGG', 'MUSIC/')[0], mui(0x8aa2f8),
	SDTest.Peek(0x4aa1f0, 's')))

# the file callbacks
check('Music_FileOpen missing', lambda: SDTest.Call(0x232560, 'tiis', 0, 'no/such/file.ogg')[0])
muf1 = SDTest.Call(0x232560, 'tiis', 0, 'sdcall.py')[0]
check('Music_FileOpen', lambda: muf1 != 0)
if muf1:
	check('Music_FileTell', lambda: SDTest.Call(0x2325c0, 'tiii', 0, muf1)[0])
	check('Music_FileRead', lambda: SDTest.Call(0x232580, 'tiibii', 0, '', 8, muf1)[0:2])
	check('Music_FileSeek', lambda: SDTest.Call(0x2325a0, 'tiiiii', 0, muf1, 3, 0x100)[0])
	check('Music_FileTell 2', lambda: SDTest.Call(0x2325c0, 'tiii', 0, muf1)[0])
	check('Music_FileRead 2', lambda: SDTest.Call(0x232580, 'tiibii', 0, '', 4, muf1)[0:2])
	check('Music_FileClose', lambda: SDTest.Call(0x232540, 'tiii', 0, muf1)[0])
check('Music_True16a', lambda: SDTest.Call(0x2321a0, 'tbiiiii', 0, 1, 2, 3, 4)[0])
check('Music_True16b', lambda: SDTest.Call(0x2321b0, 'tbiiiii', 0, 1, 2, 3, 4)[0])
check('Music_Nop', lambda: SDTest.Call(0x232190, 'v')[0])

# back
for a, v in mustate:
	mupoke(a, v)
SDTest.Call(0x22cf50, 'iisi', 0x8aa1f0, muname, 0x104)
for i in range(8):
	v = MUVOICES + i * muvsize
	mupoke(v + 0x104, muvoice[i][0])
	mupoke(v + 0x108, muvoice[i][1])
	mupoke(v + 0x18c, muvoice[i][2])
	SDTest.Call(0x22cf50, 'iisi', v + 0x10c, muvoice[i][3], 0x80)
