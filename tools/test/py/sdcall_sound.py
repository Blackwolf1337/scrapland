# game Sound/Sound.c, SoundDX8.c and Wav.c (execfile'd by sdcall.py after its other checks, Python 1.5.2): the volume
# curve, the FMOD error texts, cSoundDesc on heap memory, a private CSoundSys (constructor, listeners, the 3D volume,
# pan and doppler, the sound names of file names), a private CSoundXBox (two .wav of the packs loaded into its table,
# the descriptors, the play and update paths), cSound with g_pSound pointed at that CSoundXBox, and the wav header of
# pack files. Nothing plays: S_SkipPlay is on while the play paths run, the channel updates get a channel that does
# not exist. Not called: the init and close of the system, StopAllSounds and DeleteAllSounds (they stop every FMOD
# channel), the music and voice calls, SetDesc and LoadSound2 (CHECK(0)). g_pSound and the engine variables poked
# are put back.
def sndz(n):
	return SDTest.Call(0x133650, 'ii', n)[0]
def sndi(a):
	return SDTest.Peek(a, '@i')
def sndf(a, n):
	return SDTest.Call(0x3197d0, 'iFii', (0.0,) * n, a, 4 * n)[1]
def sndset(a, v):
	SDTest.Call(0x3197d0, 'iiFi', a, v, 4 * len(v))
def sndpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def snddesc(d):
	return (sndi(d), sndi(d + 4), sndf(d + 8, 1)[0], sndi(d + 0xc) & 0xffff, sndf(d + 0x10, 3))

# the engine variables the tests change
sndgp = SDTest.Peek(0x466bb4, 'i')
sndauto = SDTest.Peek(0x466ca0, 'i')
sndskip = SDTest.Peek(0x466dc0, 'i')
sndskipup = SDTest.Peek(0x466ce8, 'i')
sndfscale = SDTest.Peek(0x43023c, 'i')
sndshow = SDTest.Peek(0x466c40, 'i')
SDTest.Poke(0x466c40, 0)

for v in (0.0, 1.0, 0.5, 0.25, 0.125, 2.0, -1.0, 1.52587890625e-05, 1.5e-05, 0.1, 0.999, 0.7071, 1e-30, 1e30):
	check('Snd_Volume' + fmt((v,)), lambda v=v: SDTest.Call(0x1a45d0, 'if', v)[0])
for e in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100):
	check('Fmod_ErrorString' + fmt((e,)), lambda e=e: SDTest.Call(0x1a4f00, 'si', e)[0])

# cSoundDesc
sndd = sndz(0x20)
sndd2 = sndz(0x20)
sndv = sndz(0x28)
check('cSoundDesc_Init', lambda: (SDTest.Call(0x1a3230, 'tvi', sndd), snddesc(sndd)))
check('cSoundDesc_Set', lambda: (SDTest.Call(0x1a3e70, 'tviiifii', sndd, -500, 300, 1.5, 1, 0x40), snddesc(sndd)))
check('cSoundDesc_Copy', lambda: (SDTest.Call(0x1a3210, 'tvii', sndd2, sndd), snddesc(sndd2)))

# cSound basics
snds = sndz(0x40)
sndset(snds, (7.0,) * 16)
check('cSound_Init', lambda: (SDTest.Call(0x1a2f30, 'tii', snds)[0] == snds, snddesc(snds), sndi(snds + 0x1c) & 0xff,
	sndi(snds + 0x3c)))
check('cSound_GetName', lambda: SDTest.Call(0x1a31b0, 'tii', snds)[0] == snds + 0x1c)
for p in (0.0, 1.0, -12.0, 12.0, 7.5, -0.5, 100.0, -100.0):
	sndset(snds + 8, (1.25,))
	check('cSound_SetPitch' + fmt((p,)), lambda p=p: (SDTest.Call(0x1a30d0, 'tvif', snds, p), sndf(snds + 8, 1)[0]))

# a private CSoundSys: constructor, listeners, Compute3D
sndss = sndz(0x80)
sndset(sndss, (7.0,) * 32)
def sndlist(ss):
	r = ()
	for k in range(1, 32):
		r = r + (sndi(ss + 4 * k),)
	return r
check('CSoundSys_ctor', lambda: (SDTest.Call(0x1a2f70, 'tii', sndss)[0] == sndss, sndi(sndss) == 0x7c9f3c,
	sndlist(sndss)))
sndpoke(sndss + 0x7c, 5)
check('CSoundSys_ResetListeners', lambda: (SDTest.Call(0x1a32a0, 'tvi', sndss), sndi(sndss + 0x7c)))
sndlisteners = (
	(0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
	(1, (100.0, 0.0, 50.0), (0.3, 1.2, -0.4), (1.0, 2.0, 3.0)),
	(0, (5.0, 1.0, -3.0), (0.0, 3.14159, 0.0), (0.0, 0.0, -20.0)),
	(1, (-40.0, 2.0, 7.0), (-1.5, 0.25, 2.5), (0.5, -0.5, 0.0)),
)
for id, pos, ang, vel in sndlisteners:
	check('CSoundSys_SetListener' + fmt((id, pos, ang, vel)), lambda id=id, pos=pos, ang=ang, vel=vel: (SDTest.Call(
		0x1a3ce0, 'tviiFFF', sndss, id, pos, ang, vel), sndf(sndss + 4 + 0x3c * id, 15), sndi(sndss + 0x7c)))

def snd3d(ss, params, pos, vel, dop):
	SDTest.Call(0x1a3230, 'tvi', sndd)
	sndset(sndd + 0x10, params)
	sndpoke(sndd, 777)
	sndpoke(sndd + 4, 777)
	SDTest.Call(0x1a32b0, 'tviiFFi', ss, sndd, pos, vel, dop)
	return (sndi(sndd), sndi(sndd + 4), sndf(sndd + 8, 1)[0])
sndparams = ((1.0, 0.0, 0.0), (1.0, 5.0, 50.0), (1.0, 50.0, 5.0), (1.0, 0.0, 30.0), (2.0, 10.0, 0.0), (0.5, 0.0, 0.0))
sndpos = ((0.0, 0.0, 0.0), (3.0, 4.0, 0.0), (10.0, -2.0, 7.0), (60.0, 0.0, 40.0), (-200.0, 5.0, 1000.0),
	(-40.0, 2.0, 7.0), (0.5, 0.0, 0.0), (-38.0, 1.0, 9.0))
sndvels = ((0.0, 0.0, 0.0), (10.0, 0.0, -5.0), (-400.0, 30.0, 400.0))
for auto in (-60.0, -48.0, -10.0):
	sndset(0x866ca0, (auto,))
	for params in sndparams:
		for pos in sndpos:
			for vel in sndvels:
				for dop in (0, 1):
					check('CSoundSys_Compute3D' + fmt((auto, params, pos, vel, dop)), lambda params=params, pos=pos,
						vel=vel, dop=dop: snd3d(sndss, params, pos, vel, dop))
SDTest.Poke(0x466ca0, sndauto)
for last in (0, -1):
	sndpoke(sndss + 0x7c, last)
	check('CSoundSys_Compute3D last' + fmt((last,)), lambda: snd3d(sndss, (1.0, 0.0, 0.0), (3.0, 4.0, 0.0),
		(1.0, 0.0, 0.0), 1))
sndpoke(sndss + 0x7c, 1)

for f in ('Sounds/Menu/click.wav', 'a/b/c.d.e', 'noext', 'dir\\name.wav', 'c:\\x\\y\\z.mp3', '', '.wav',
		'1234567890123456789012345678901.wav'):
	for size in (32, 64):
		check('CSoundSys_GetSoundName' + fmt((f, size)), lambda f=f, size=size: SDTest.Call(0x1a3a90, 'tvisbi', sndss,
			f, 'junk', size)[1])

# the CSoundSys as g_pSound: the 3D setters go to its Compute3D
SDTest.Poke(0x466bb4, sndss)
for pos in sndpos:
	check('cSoundDesc_SetPos' + fmt((pos,)), lambda pos=pos: (SDTest.Call(0x1a3230, 'tvi', sndd),
		SDTest.Call(0x1a3f40, 'tviF', sndd, pos), snddesc(sndd)))
	for vel in sndvels:
		check('cSound_SetPosVel' + fmt((pos, vel)), lambda pos=pos, vel=vel: (SDTest.Call(0x1a3230, 'tvi', snds),
			SDTest.Call(0x1a3ef0, 'tviFF', snds, pos, vel), snddesc(snds)))
SDTest.Poke(0x466bb4, sndgp)

# a private CSoundXBox
sndxb = sndz(0x94)
check('CSoundXBox_ctor', lambda: (SDTest.Call(0x1a46b0, 'tii', sndxb)[0] == sndxb, sndi(sndxb) == 0x7ca718,
	sndi(sndxb + 0x7c), sndi(sndxb + 0x80), sndi(sndxb + 0x84) != 0, sndi(sndxb + 0x88) & 0xffff, sndi(sndxb + 0x8c),
	sndi(sndxb + 0x90) & 0xff))
# the volume the game's own instance has, so that Update stops nothing
sndpoke(sndxb + 0x8c, sndi(sndgp + 0x8c))
check('CSoundXBox_Update', lambda: (SDTest.Call(0x1a5e50, 'tvi', sndxb), sndi(sndxb + 0x8c) == sndi(sndgp + 0x8c),
	sndf(0x83023c, 1)[0]))
SDTest.Poke(0x43023c, sndfscale)
sndfiles = ('sounds/generic/menu/menuclik.wav', 'levels/bank/sounds/bank-loop.wav')
for f in sndfiles:
	check('CSoundXBox_LoadSound' + fmt((f,)), lambda f=f: SDTest.Call(0x1a5640, 'tvis', sndxb, f))
check('CSoundXBox_Load again', lambda: SDTest.Call(0x1a5310, 'tvisi', sndxb, sndfiles[0], 1))
def sndfind(xb, name):
	slot = SDTest.Call(0x1a51f0, 'tiis', xb + 0x80, name)[0]
	return SDTest.Call(0x1a50a0, 'tiisi', xb + 0x80, name, slot)[0]
for name in ('menuclik', 'MenuClik', 'bank-loop', 'none'):
	check('CSoundXBox_GetDesc' + fmt((name,)), lambda name=name: (SDTest.Call(0x1a5160, 'tbisi', sndxb, name, sndd)[0],
		snddesc(sndd)))
	it = sndfind(sndxb, name)
	if it:
		check('CSoundXBox item' + fmt((name,)), lambda it=it: (sndi(it) != 0, snddesc(it + 4), sndi(it + 0x20)))
	for swap in (0, 1):
		sndpoke(sndxb + 0x90, swap)
		SDTest.Call(0x1a3230, 'tvi', sndd2)
		SDTest.Call(0x1a3e70, 'tviiifii', sndd2, -100, 50, 2.0, 1, 3)
		SDTest.Call(0x1a3230, 'tvi', sndv + 4)
		check('CSoundXBox_GetPlayDesc' + fmt((name, swap)), lambda name=name: (SDTest.Call(0x1a4a90, 'tbisii', sndxb,
			name, sndd2, sndv)[0], sndi(sndv) != 0, snddesc(sndv + 4), sndi(sndv + 0x20)))
	sndpoke(sndxb + 0x90, 0)

# the play paths end in Fmod_Play, which S_SkipPlay stops before FMOD
SDTest.Poke(0x466dc0, 1)
for name in ('menuclik', 'bank-loop', 'none'):
	SDTest.Call(0x1a3230, 'tvi', sndd2)
	check('CSoundXBox_PlaySound' + fmt((name,)), lambda name=name: SDTest.Call(0x1a57e0, 'tiisip', sndxb, name, sndd2,
		5))
	check('CSoundXBox_PlayDesc' + fmt((name,)), lambda name=name: SDTest.Call(0x1a5660, 'tiisi', sndxb, name, sndd2))
	check('CSoundXBox_PlayName' + fmt((name,)), lambda name=name: SDTest.Call(0x1a57c0, 'tiis', sndxb, name))
	check('CSoundXBox_Play' + fmt((name,)), lambda name=name: SDTest.Call(0x1a5860, 'tiisii', sndxb, name, -300, 20))
	check('CSoundXBox_PlayVPF' + fmt((name,)), lambda name=name: SDTest.Call(0x1a56d0, 'tiisiif', sndxb, name, 0, 0,
		0.5))
	check('CSoundXBox_PlayVPF quiet' + fmt((name,)), lambda name=name: SDTest.Call(0x1a56d0, 'tiisiif', sndxb, name,
		-7000, 0, 1.0))
	check('CSoundXBox_PlayAt' + fmt((name,)), lambda name=name: SDTest.Call(0x1a5810, 'tiisFi', sndxb, name,
		(1.0, 2.0, 3.0), 0))
sndpoke(sndxb + 0x8c, -7000)
check('CSoundXBox_Play muted', lambda: SDTest.Call(0x1a5860, 'tiisii', sndxb, 'menuclik', 0, 0))
check('CSoundXBox_PlayDesc muted', lambda: SDTest.Call(0x1a5660, 'tiisi', sndxb, 'menuclik', sndd2))
sndpoke(sndxb + 0x8c, sndi(sndgp + 0x8c))
check('Fmod_Play no sample', lambda: SDTest.Call(0x1a61a0, 'iiis', 0, sndd2, 'x'))

# the channel updates of a channel that does not exist
for name in ('menuclik', 'none'):
	for skip in (0, 1):
		SDTest.Poke(0x466ce8, skip)
		sndset(0x866ca0, (-60.0,))
		check('CSoundXBox_UpdateSound' + fmt((name, skip)), lambda name=name: (SDTest.Call(0x1a6000, 'tbisii', sndxb,
			name, sndd2, -5)[0], sndf(0x866ca0, 1)[0]))
SDTest.Poke(0x466ce8, sndskipup)
sndset(0x866ca0, (-60.0,))
check('Fmod_SetChannel', lambda: (SDTest.Call(0x1a6060, 'bii', -5, sndd2)[0], sndf(0x866ca0, 1)[0]))
SDTest.Poke(0x466ca0, sndauto)
check('CSoundSys_SetVolume none', lambda: SDTest.Call(0x1a4290, 'tvisi', sndxb, 'none', -100))
check('CSoundSys_SetPan none', lambda: SDTest.Call(0x1a41b0, 'tvisi', sndxb, 'none', 100))
check('CSoundSys_SetFreq none', lambda: SDTest.Call(0x1a40d0, 'tvisf', sndxb, 'none', 2.0))
check('CSoundSys_SetLoop none', lambda: SDTest.Call(0x1a4140, 'tvisi', sndxb, 'none', 1))
check('CSoundSys_SetPrio none', lambda: SDTest.Call(0x1a4220, 'tvisi', sndxb, 'none', 1))

# cSound with the private CSoundXBox as g_pSound (no listener: every 3D volume is off)
SDTest.Poke(0x466bb4, sndxb)
sndset(0x866ca0, (-60.0,))
def sndsound(s):
	return (snddesc(s), SDTest.Call(0x1a31b0, 'tsi', s)[0], sndi(s + 0x3c))
check('cSound_SetSound name', lambda: (SDTest.Call(0x1a3fa0, 'tvisi', snds, 'menuclik', 0), sndsound(snds)))
check('cSound_SetSound none', lambda: (SDTest.Call(0x1a3fa0, 'tvisi', snds, 'none', 0), sndsound(snds)))
check('cSound_SetSound NULL', lambda: (SDTest.Call(0x1a3fa0, 'tvizi', snds, None, 0), sndsound(snds)))
check('cSound_SetSound file', lambda: (SDTest.Call(0x1a3fa0, 'tvisi', snds, 'sounds/generic/menu/menufocus.wav', 2),
	sndsound(snds)))
snds2 = sndz(0x40)
check('cSound_ctor', lambda: (SDTest.Call(0x1a2ed0, 'tiisi', snds2, 'bank-loop', 0)[0] == snds2, sndsound(snds2)))
check('cSound_Play', lambda: (SDTest.Call(0x1a3bb0, 'tbi', snds)[0], sndsound(snds)))
check('cSound_PlayVP', lambda: (SDTest.Call(0x1a3c80, 'tbiii', snds, -200, 30)[0], sndsound(snds)))
check('cSound_PlayAt', lambda: (SDTest.Call(0x1a3c10, 'tbiFi', snds, (1.0, 2.0, 3.0), 5)[0], sndsound(snds)))
for loop in (0, 1):
	for vol in (0, -5000, -7000):
		sndpoke(snds + 0xc, 0x8000 + loop)
		check('cSound_UpdateVP' + fmt((loop, vol)), lambda vol=vol: (SDTest.Call(0x1a4590, 'tbiii', snds, vol, 10)[0],
			sndsound(snds)))
check('cSound_Update', lambda: (SDTest.Call(0x1a4400, 'tbi', snds)[0], sndsound(snds)))
check('cSound_UpdateAt', lambda: (SDTest.Call(0x1a44c0, 'tbiFi', snds, (1.0, 2.0, 3.0), 5)[0], sndsound(snds)))
check('cSound_UpdatePosVel', lambda: (SDTest.Call(0x1a4530, 'tbiFFi', snds, (1.0, 2.0, 3.0), (0.0, 1.0, 0.0), 5)[0],
	sndsound(snds)))
check('cSound_Stop', lambda: (SDTest.Call(0x1a4390, 'tvi', snds), sndsound(snds)))
# with a listener the 3D volume is on
SDTest.Call(0x1a3ce0, 'tviiFFF', sndxb, 0, (0.0, 0.0, 0.0), (0.0, 0.5, 0.0), (0.0, 0.0, 0.0))
for pos in sndpos:
	check('cSound_UpdatePosVel listener' + fmt((pos,)), lambda pos=pos: (SDTest.Call(0x1a4530, 'tbiFFi', snds, pos,
		(0.0, 1.0, 0.0), 5)[0], sndsound(snds)))
	check('cSound_PlayAt listener' + fmt((pos,)), lambda pos=pos: (SDTest.Call(0x1a3c10, 'tbiFi', snds, pos, -5)[0],
		sndsound(snds)))
SDTest.Poke(0x466bb4, sndgp)
SDTest.Poke(0x466dc0, sndskip)
SDTest.Poke(0x466ca0, sndauto)

# the samples are FMOD's (NULL when it did not load them), the table goes with the destructor
for name in ('menufocus', 'menuclik'):
	it = sndfind(sndxb, name)
	if it and sndi(it):
		check('CSoundXBox_DeleteSound' + fmt((name,)), lambda name=name: (SDTest.Call(0x1a4e70, 'tvis', sndxb, name),
			sndfind(sndxb, name) != 0))
	else:
		check('CSoundXBox_DeleteSound' + fmt((name,)), lambda: 'no sample')
check('CSoundXBox_Destroy', lambda: (SDTest.Call(0x1a4880, 'tiii', sndxb, 0)[0] == sndxb, sndi(sndxb) == 0x7c9f3c,
	sndi(sndxb + 0x80), sndi(sndxb + 0x84)))

# the wav headers of pack files
sndfp = sndz(0x1c)
sndinfo = sndz(0x10)
for f in ('sounds/generic/menu/menuclik.wav', 'levels/bank/sounds/bank-loop.wav', 'm3d.ini',
		'sounds/generic/scorer/keyboard1.wav'):
	SDTest.Call(0x12e510, 'tii', sndfp)
	SDTest.Call(0x1307f0, 'tiisi', sndfp, f, 1)
	sndset(sndinfo, (0.0,) * 4)
	check('Wav_ReadInfo' + fmt((f,)), lambda: (SDTest.Call(0x1a6200, 'iii', sndfp, sndinfo)[0], sndi(sndinfo),
		sndi(sndinfo + 4), sndi(sndinfo + 8), sndi(sndinfo + 12)))
	SDTest.Call(0x12f940, 'tii', sndfp)
	SDTest.Call(0x12e5d0, 'tvi', sndfp)
SDTest.Call(0x12e510, 'tii', sndfp)
SDTest.Call(0x1307f0, 'tiisi', sndfp, sndfiles[0], 1)
for id in (0x46464952, 0, 0x12345678, 0x20746d66):
	check('Wav_ReadChunk' + fmt((id,)), lambda id=id: SDTest.Call(0x1a6420, 'iipi', sndfp, 0, id))
SDTest.Call(0x12f940, 'tii', sndfp)
SDTest.Call(0x12e5d0, 'tvi', sndfp)
SDTest.Poke(0x466c40, sndshow)
for p in (sndd, sndd2, sndv, snds, snds2, sndss, sndxb, sndfp, sndinfo):
	SDTest.Call(0x133550, 'vi', p)
