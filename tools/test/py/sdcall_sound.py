# game Sound/Sound.c, SoundDX8.c, Wav.c, PosSound.c and SndInterface.c (execfile'd by sdcall.py after its other checks,
# Python 1.5.2): the volume curve, the FMOD error texts, cSoundDesc on heap memory, a private CSoundSys (constructor,
# listeners, the 3D volume, pan and doppler, the sound names of file names), a private CSoundXBox (two .wav of the packs
# loaded into its table, the descriptors, the play and update paths), cSound, a property set and the sounds of an
# entity (without entity) with g_pSound pointed at that CSoundXBox, and the wav header of pack files. Nothing plays: S_SkipPlay is on while the play paths run, the channel updates get a channel that does
# not exist. Not called: the init and close of the system, StopAllSounds and DeleteAllSounds (they stop every FMOD
# channel), the music and voice calls, SetDesc and LoadSound2 (CHECK(0)). g_pSound and the engine variables poked
# are put back.
def sndz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def sndi(a):
	return SDTest.Peek(a, '@i')
def sndf(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def sndset(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def sndpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def snddesc(d):
	return (sndi(d), sndi(d + 4), sndf(d + 8, 1)[0], sndi(d + 0xc) & 0xffff, sndf(d + 0x10, 3))

# the engine variables the tests change
sndgp = SDTest.Peek('g_pSound', 'i')
sndauto = SDTest.Peek('g_SoundAutoplayVolume', 'i')
sndskip = SDTest.Peek('g_S_SkipPlay', 'i')
sndskipup = SDTest.Peek('g_S_SkipUpdates', 'i')
sndfscale = SDTest.Peek('g_SndFreqScale', 'i')
sndshow = SDTest.Peek('g_SoundShowSetMode', 'i')
SDTest.Poke('g_SoundShowSetMode', 0)

for v in (0.0, 1.0, 0.5, 0.25, 0.125, 2.0, -1.0, 1.52587890625e-05, 1.5e-05, 0.1, 0.999, 0.7071, 1e-30, 1e30):
	check('Snd_Volume' + fmt((v,)), lambda v=v: SDTest.Call('Snd_Volume', 'if', v)[0])
for e in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 100):
	check('Fmod_ErrorString' + fmt((e,)), lambda e=e: SDTest.Call('Fmod_ErrorString', 'si', e)[0])

# cSoundDesc
sndd = sndz(0x20)
sndd2 = sndz(0x20)
sndv = sndz(0x28)
check('cSoundDesc_Init', lambda: (SDTest.Call('cSoundDesc_Init', 'tvi', sndd), snddesc(sndd)))
check('cSoundDesc_Set', lambda: (SDTest.Call('cSoundDesc_Set', 'tviiifii', sndd, -500, 300, 1.5, 1, 0x40), snddesc(sndd)))
check('cSoundDesc_Copy', lambda: (SDTest.Call('cSoundDesc_Copy', 'tvii', sndd2, sndd), snddesc(sndd2)))

# cSound basics
snds = sndz(0x40)
sndset(snds, (7.0,) * 16)
check('cSound_Init', lambda: (SDTest.Call('cSound_Init', 'tii', snds)[0] == snds, snddesc(snds), sndi(snds + 0x1c) & 0xff,
	sndi(snds + 0x3c)))
check('cSound_GetName', lambda: SDTest.Call('cSound_GetName', 'tii', snds)[0] == snds + 0x1c)
for p in (0.0, 1.0, -12.0, 12.0, 7.5, -0.5, 100.0, -100.0):
	sndset(snds + 8, (1.25,))
	check('cSound_SetPitch' + fmt((p,)), lambda p=p: (SDTest.Call('cSound_SetPitch', 'tvif', snds, p), sndf(snds + 8, 1)[0]))

# a private CSoundSys: constructor, listeners, Compute3D
sndss = sndz(0x80)
sndset(sndss, (7.0,) * 32)
def sndlist(ss):
	r = ()
	for k in range(1, 32):
		r = r + (sndi(ss + 4 * k),)
	return r
check('CSoundSys_ctor', lambda: (SDTest.Call('CSoundSys_ctor', 'tii', sndss)[0] == sndss, sndi(sndss) == symva('CSoundSys_vftable'),
	sndlist(sndss)))
sndpoke(sndss + 0x7c, 5)
check('CSoundSys_ResetListeners', lambda: (SDTest.Call('CSoundSys_ResetListeners', 'tvi', sndss), sndi(sndss + 0x7c)))
sndlisteners = (
	(0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
	(1, (100.0, 0.0, 50.0), (0.3, 1.2, -0.4), (1.0, 2.0, 3.0)),
	(0, (5.0, 1.0, -3.0), (0.0, 3.14159, 0.0), (0.0, 0.0, -20.0)),
	(1, (-40.0, 2.0, 7.0), (-1.5, 0.25, 2.5), (0.5, -0.5, 0.0)),
)
for id, pos, ang, vel in sndlisteners:
	check('CSoundSys_SetListener' + fmt((id, pos, ang, vel)), lambda id=id, pos=pos, ang=ang, vel=vel: (SDTest.Call(
		'CSoundSys_SetListener', 'tviiFFF', sndss, id, pos, ang, vel), sndf(sndss + 4 + 0x3c * id, 15), sndi(sndss + 0x7c)))

def snd3d(ss, params, pos, vel, dop):
	SDTest.Call('cSoundDesc_Init', 'tvi', sndd)
	sndset(sndd + 0x10, params)
	sndpoke(sndd, 777)
	sndpoke(sndd + 4, 777)
	SDTest.Call('CSoundSys_Compute3D', 'tviiFFi', ss, sndd, pos, vel, dop)
	return (sndi(sndd), sndi(sndd + 4), sndf(sndd + 8, 1)[0])
sndparams = ((1.0, 0.0, 0.0), (1.0, 5.0, 50.0), (1.0, 50.0, 5.0), (1.0, 0.0, 30.0), (2.0, 10.0, 0.0), (0.5, 0.0, 0.0))
sndpos = ((0.0, 0.0, 0.0), (3.0, 4.0, 0.0), (10.0, -2.0, 7.0), (60.0, 0.0, 40.0), (-200.0, 5.0, 1000.0),
	(-40.0, 2.0, 7.0), (0.5, 0.0, 0.0), (-38.0, 1.0, 9.0))
sndvels = ((0.0, 0.0, 0.0), (10.0, 0.0, -5.0), (-400.0, 30.0, 400.0))
for auto in (-60.0, -48.0, -10.0):
	sndset(symva('g_SoundAutoplayVolume'), (auto,))
	for params in sndparams:
		for pos in sndpos:
			for vel in sndvels:
				for dop in (0, 1):
					check('CSoundSys_Compute3D' + fmt((auto, params, pos, vel, dop)), lambda params=params, pos=pos,
						vel=vel, dop=dop: snd3d(sndss, params, pos, vel, dop))
SDTest.Poke('g_SoundAutoplayVolume', sndauto)
for last in (0, -1):
	sndpoke(sndss + 0x7c, last)
	check('CSoundSys_Compute3D last' + fmt((last,)), lambda: snd3d(sndss, (1.0, 0.0, 0.0), (3.0, 4.0, 0.0),
		(1.0, 0.0, 0.0), 1))
sndpoke(sndss + 0x7c, 1)

for f in ('Sounds/Menu/click.wav', 'a/b/c.d.e', 'noext', 'dir\\name.wav', 'c:\\x\\y\\z.mp3', '', '.wav',
		'1234567890123456789012345678901.wav'):
	for size in (32, 64):
		check('CSoundSys_GetSoundName' + fmt((f, size)), lambda f=f, size=size: SDTest.Call('CSoundSys_GetSoundName', 'tvisbi', sndss,
			f, 'junk', size)[1])

# the CSoundSys as g_pSound: the 3D setters go to its Compute3D
SDTest.Poke('g_pSound', sndss)
for pos in sndpos:
	check('cSoundDesc_SetPos' + fmt((pos,)), lambda pos=pos: (SDTest.Call('cSoundDesc_Init', 'tvi', sndd),
		SDTest.Call('cSoundDesc_SetPos', 'tviF', sndd, pos), snddesc(sndd)))
	for vel in sndvels:
		check('cSound_SetPosVel' + fmt((pos, vel)), lambda pos=pos, vel=vel: (SDTest.Call('cSoundDesc_Init', 'tvi', snds),
			SDTest.Call('cSound_SetPosVel', 'tviFF', snds, pos, vel), snddesc(snds)))
SDTest.Poke('g_pSound', sndgp)

# a private CSoundXBox
sndxb = sndz(0x94)
check('CSoundXBox_ctor', lambda: (SDTest.Call('CSoundXBox_ctor', 'tii', sndxb)[0] == sndxb, sndi(sndxb) == symva('CSoundXBox_vftable'),
	sndi(sndxb + 0x7c), sndi(sndxb + 0x80), sndi(sndxb + 0x84) != 0, sndi(sndxb + 0x88) & 0xffff, sndi(sndxb + 0x8c),
	sndi(sndxb + 0x90) & 0xff))
# the volume the game's own instance has, so that Update stops nothing
sndpoke(sndxb + 0x8c, sndi(sndgp + 0x8c))
check('CSoundXBox_Update', lambda: (SDTest.Call('CSoundXBox_Update', 'tvi', sndxb), sndi(sndxb + 0x8c) == sndi(sndgp + 0x8c),
	sndf(symva('g_SndFreqScale'), 1)[0]))
SDTest.Poke('g_SndFreqScale', sndfscale)
sndfiles = ('sounds/generic/menu/menuclik.wav', 'levels/bank/sounds/bank-loop.wav')
for f in sndfiles:
	check('CSoundXBox_LoadSound' + fmt((f,)), lambda f=f: SDTest.Call('CSoundXBox_LoadSound', 'tvis', sndxb, f))
check('CSoundXBox_Load again', lambda: SDTest.Call('CSoundXBox_Load', 'tvisi', sndxb, sndfiles[0], 1))
def sndfind(xb, name):
	slot = SDTest.Call('HashTable_Slot@1a51f0', 'tiis', xb + 0x80, name)[0]
	return SDTest.Call('SoundStrTable_Find', 'tiisi', xb + 0x80, name, slot)[0]
for name in ('menuclik', 'MenuClik', 'bank-loop', 'none'):
	check('CSoundXBox_GetDesc' + fmt((name,)), lambda name=name: (SDTest.Call('CSoundXBox_GetDesc', 'tbisi', sndxb, name, sndd)[0],
		snddesc(sndd)))
	it = sndfind(sndxb, name)
	if it:
		check('CSoundXBox item' + fmt((name,)), lambda it=it: (sndi(it) != 0, snddesc(it + 4), sndi(it + 0x20)))
	for swap in (0, 1):
		sndpoke(sndxb + 0x90, swap)
		SDTest.Call('cSoundDesc_Init', 'tvi', sndd2)
		SDTest.Call('cSoundDesc_Set', 'tviiifii', sndd2, -100, 50, 2.0, 1, 3)
		SDTest.Call('cSoundDesc_Init', 'tvi', sndv + 4)
		check('CSoundXBox_GetPlayDesc' + fmt((name, swap)), lambda name=name: (SDTest.Call('CSoundXBox_GetPlayDesc', 'tbisii', sndxb,
			name, sndd2, sndv)[0], sndi(sndv) != 0, snddesc(sndv + 4), sndi(sndv + 0x20)))
	sndpoke(sndxb + 0x90, 0)

# the play paths end in Fmod_Play, which S_SkipPlay stops before FMOD
SDTest.Poke('g_S_SkipPlay', 1)
for name in ('menuclik', 'bank-loop', 'none'):
	SDTest.Call('cSoundDesc_Init', 'tvi', sndd2)
	check('CSoundXBox_PlaySound' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_PlaySound', 'tiisip', sndxb, name, sndd2,
		5))
	check('CSoundXBox_PlayDesc' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_PlayDesc', 'tiisi', sndxb, name, sndd2))
	check('CSoundXBox_PlayName' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_PlayName', 'tiis', sndxb, name))
	check('CSoundXBox_Play' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_Play', 'tiisii', sndxb, name, -300, 20))
	check('CSoundXBox_PlayVPF' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_PlayVPF', 'tiisiif', sndxb, name, 0, 0,
		0.5))
	check('CSoundXBox_PlayVPF quiet' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_PlayVPF', 'tiisiif', sndxb, name,
		-7000, 0, 1.0))
	check('CSoundXBox_PlayAt' + fmt((name,)), lambda name=name: SDTest.Call('CSoundXBox_PlayAt', 'tiisFi', sndxb, name,
		(1.0, 2.0, 3.0), 0))
sndpoke(sndxb + 0x8c, -7000)
check('CSoundXBox_Play muted', lambda: SDTest.Call('CSoundXBox_Play', 'tiisii', sndxb, 'menuclik', 0, 0))
check('CSoundXBox_PlayDesc muted', lambda: SDTest.Call('CSoundXBox_PlayDesc', 'tiisi', sndxb, 'menuclik', sndd2))
sndpoke(sndxb + 0x8c, sndi(sndgp + 0x8c))
check('Fmod_Play no sample', lambda: SDTest.Call('Fmod_Play', 'iiis', 0, sndd2, 'x'))

# the channel updates of a channel that does not exist
for name in ('menuclik', 'none'):
	for skip in (0, 1):
		SDTest.Poke('g_S_SkipUpdates', skip)
		sndset(symva('g_SoundAutoplayVolume'), (-60.0,))
		check('CSoundXBox_UpdateSound' + fmt((name, skip)), lambda name=name: (SDTest.Call('CSoundXBox_UpdateSound', 'tbisii', sndxb,
			name, sndd2, -5)[0], sndf(symva('g_SoundAutoplayVolume'), 1)[0]))
SDTest.Poke('g_S_SkipUpdates', sndskipup)
sndset(symva('g_SoundAutoplayVolume'), (-60.0,))
check('Fmod_SetChannel', lambda: (SDTest.Call('Fmod_SetChannel', 'bii', -5, sndd2)[0], sndf(symva('g_SoundAutoplayVolume'), 1)[0]))
SDTest.Poke('g_SoundAutoplayVolume', sndauto)
check('CSoundSys_SetVolume none', lambda: SDTest.Call('CSoundSys_SetVolume', 'tvisi', sndxb, 'none', -100))
check('CSoundSys_SetPan none', lambda: SDTest.Call('CSoundSys_SetPan', 'tvisi', sndxb, 'none', 100))
check('CSoundSys_SetFreq none', lambda: SDTest.Call('CSoundSys_SetFreq', 'tvisf', sndxb, 'none', 2.0))
check('CSoundSys_SetLoop none', lambda: SDTest.Call('CSoundSys_SetLoop', 'tvisi', sndxb, 'none', 1))
check('CSoundSys_SetPrio none', lambda: SDTest.Call('CSoundSys_SetPrio', 'tvisi', sndxb, 'none', 1))

# cSound with the private CSoundXBox as g_pSound (no listener: every 3D volume is off)
SDTest.Poke('g_pSound', sndxb)
sndset(symva('g_SoundAutoplayVolume'), (-60.0,))
def sndsound(s):
	return (snddesc(s), SDTest.Call('cSound_GetName', 'tsi', s)[0], sndi(s + 0x3c))
check('cSound_SetSound name', lambda: (SDTest.Call('cSound_SetSound', 'tvisi', snds, 'menuclik', 0), sndsound(snds)))
check('cSound_SetSound none', lambda: (SDTest.Call('cSound_SetSound', 'tvisi', snds, 'none', 0), sndsound(snds)))
check('cSound_SetSound NULL', lambda: (SDTest.Call('cSound_SetSound', 'tvizi', snds, None, 0), sndsound(snds)))
check('cSound_SetSound file', lambda: (SDTest.Call('cSound_SetSound', 'tvisi', snds, 'sounds/generic/menu/menufocus.wav', 2),
	sndsound(snds)))
snds2 = sndz(0x40)
check('cSound_ctor', lambda: (SDTest.Call('cSound_ctor', 'tiisi', snds2, 'bank-loop', 0)[0] == snds2, sndsound(snds2)))
check('cSound_Play', lambda: (SDTest.Call('cSound_Play', 'tbi', snds)[0], sndsound(snds)))
check('cSound_PlayVP', lambda: (SDTest.Call('cSound_PlayVP', 'tbiii', snds, -200, 30)[0], sndsound(snds)))
check('cSound_PlayAt', lambda: (SDTest.Call('cSound_PlayAt', 'tbiFi', snds, (1.0, 2.0, 3.0), 5)[0], sndsound(snds)))
for loop in (0, 1):
	for vol in (0, -5000, -7000):
		sndpoke(snds + 0xc, 0x8000 + loop)
		check('cSound_UpdateVP' + fmt((loop, vol)), lambda vol=vol: (SDTest.Call('cSound_UpdateVP', 'tbiii', snds, vol, 10)[0],
			sndsound(snds)))
check('cSound_Update', lambda: (SDTest.Call('cSound_Update', 'tbi', snds)[0], sndsound(snds)))
check('cSound_UpdateAt', lambda: (SDTest.Call('cSound_UpdateAt', 'tbiFi', snds, (1.0, 2.0, 3.0), 5)[0], sndsound(snds)))
check('cSound_UpdatePosVel', lambda: (SDTest.Call('cSound_UpdatePosVel', 'tbiFFi', snds, (1.0, 2.0, 3.0), (0.0, 1.0, 0.0), 5)[0],
	sndsound(snds)))
check('cSound_Stop', lambda: (SDTest.Call('cSound_Stop@1a4390', 'tvi', snds), sndsound(snds)))
# with a listener the 3D volume is on
SDTest.Call('CSoundSys_SetListener', 'tviiFFF', sndxb, 0, (0.0, 0.0, 0.0), (0.0, 0.5, 0.0), (0.0, 0.0, 0.0))
for pos in sndpos:
	check('cSound_UpdatePosVel listener' + fmt((pos,)), lambda pos=pos: (SDTest.Call('cSound_UpdatePosVel', 'tbiFFi', snds, pos,
		(0.0, 1.0, 0.0), 5)[0], sndsound(snds)))
	check('cSound_PlayAt listener' + fmt((pos,)), lambda pos=pos: (SDTest.Call('cSound_PlayAt', 'tbiFi', snds, pos, -5)[0],
		sndsound(snds)))

# a property set on the heap, taken out of g_SndPSList again (its timer is random, it is not printed)
sndhead = SDTest.Peek('g_SndPSList', 'i')
sndps = sndz(0x6c)
def sndpsf(ps):
	return (sndf(ps + 0x44, 5), sndi(ps + 0x58) & 0xff, sndf(ps + 0x60, 2), snddesc(ps + 4))
check('cSoundPS_ctor', lambda: (SDTest.Call('cSoundPS_ctor', 'tii', sndps)[0] == sndps, sndi(sndps) == sndhead,
	SDTest.Peek('g_SndPSList', 'i') == sndps, sndpsf(sndps)))
SDTest.Poke('g_SndPSList', sndhead)
check('cSoundPS_SetSound', lambda: (SDTest.Call('cSoundPS_SetSound', 'tvis', sndps, 'menuclik'), sndsound(sndps + 4)))
for name in ('Sound', 'Loop', 'MaxInterval', 'MinInterval', 'AttenEnd', 'AttenIni', 'Doppler', 'Volume', 'Pitch', 'X',
		'y', 'Z', 'none'):
	check('cSoundPS_Var get' + fmt((name,)), lambda name=name: SDTest.Call('cSoundPS_Var', 'tiisipDp', sndps, name, 0, 0,
		-1.0, -1)[0:1] + SDTest.Call('cSoundPS_Var', 'tiisipDp', sndps, name, 0, 0, -1.0, -1)[2:4])
	if name != 'Sound':
		check('cSoundPS_Var set' + fmt((name,)), lambda name=name: (SDTest.Call('cSoundPS_Var', 'tiisipDp', sndps, name,
			0x100 + 1, 0, 2.5, 0)[0:1], sndpsf(sndps)))
check('cSoundPS_Var no pointers', lambda: SDTest.Call('cSoundPS_Var', 'tiisiiii', sndps, 'Volume', 0, 0, 0, 0)[0])
check('cSoundPS_Var Sound name', lambda: SDTest.Peek(SDTest.Call('cSoundPS_Var', 'tiisipDp', sndps, 'Sound', 0, 0, 0.0,
	0)[1], '@s'))
for loop in (1, 0):
	sndpoke(sndps + 0x58, loop)
	sndset(sndps + 0x5c, (100.0,))
	sndset(sndps + 0x4c, (1.0, 0.0, 2.0))
	check('cSoundPS_Update' + fmt((loop,)), lambda: (SDTest.Call('cSoundPS_Update', 'tvif', sndps, 0.5), sndf(sndps + 0x5c, 1),
		sndsound(sndps + 4)))
check('cSoundPS_dtor', lambda: (SDTest.Call('cSoundPS_dtor', 'tvi', sndps), sndi(sndps + 0x40)))
check('SndPS_Get none', lambda: SDTest.Call('SndPS_Get', 'is', 'sd no such set')[0])

# the sounds of an entity without the entity: nothing that needs it (no volume set, no play)
sndif = sndz(0x1c)
check('cSndInterface_ctor', lambda: (SDTest.Call('cSndInterface_ctor', 'tiii', sndif, 2)[0] == sndif, sndi(sndif) == symva('cSndInterface_vftable'),
	sndi(sndif + 4), sndi(sndif + 8) != 0, SDTest.Peek('g_SndInterfaces', 'i') == sndif, sndi(sndif + 0x10) == symva('g_SndInterfaces'),
	SDTest.Peek(sndif + 0x14, '@s')))
check('cSndInterface_SetId', lambda: (SDTest.Call('cSndInterface_SetId', 'tvis', sndif, 'abc'), SDTest.Peek(sndif + 0x14, '@s')))
for name in ('abc_n', 'abd_n', 'abc00_loop', 'abc01_loop', 'abc00_int0', 'abc00_int4', 'abc05_loop', 'abc0x_loop',
		'abc00x', 'abc00'):
	check('cSndInterface_IntVar get' + fmt((name,)), lambda name=name: SDTest.Call('cSndInterface_IntVar', 'tbispi', sndif, name, -1,
		0))
check('cSndInterface_IntVar set n', lambda: (SDTest.Call('cSndInterface_IntVar', 'tbispi', sndif, 'abc_n', 3, 1), sndi(sndif + 4)))
for i in (0, 1, 2, 3, -1):
	check('PtrArray_Get' + fmt((i,)), lambda i=i: SDTest.Call('PtrArray_Get', 'tiii', sndif, i)[0])
for name in ('abc00_x', 'abc12_name', 'ab', 'abc00name', 'xbc00_name', 'abc0a_x', 'abc99_name'):
	check('cSndInterface_ParseName' + fmt((name,)), lambda name=name: (lambda r: (r[0], r[1], r[2] != 0))(
		SDTest.Call('cSndInterface_ParseName', 'tbispp', sndif, name, 0, 0)))
def PtrArray_Get_ok(sif, i):
	it = SDTest.Call('PtrArray_Get', 'tiii', sif, i)[0]
	if not it:
		return 0
	return (sndsound(it), sndf(it + 0x40, 23), sndi(it + 0x9c) & 0xff)
check('cSndInterface_SetSound 0', lambda: (SDTest.Call('cSndInterface_SetSound', 'tviis', sndif, 0, 'menuclik'),
	PtrArray_Get_ok(sndif, 0)))
check('cSndInterface_SetSound empty', lambda: (SDTest.Call('cSndInterface_SetSound', 'tviis', sndif, 2, ''), PtrArray_Get_ok(sndif,
	2)))
# "menuclik" in heap memory for the string variable
sndstr = sndz(16)
sndpoke(sndstr, 0x756e656d)
sndpoke(sndstr + 4, 0x6b696c63)
check('cSndInterface_StrVar set', lambda: (SDTest.Call('cSndInterface_StrVar', 'tbispi', sndif, 'abc01_name', sndstr, 1)[0],
	PtrArray_Get_ok(sndif, 1)))
for name in ('abc00_name', 'abc01_name', 'abc02_name', 'abc00_nam', 'abd00_name'):
	check('cSndInterface_StrVar get' + fmt((name,)), lambda name=name: (lambda r: (r[0], r[1] and SDTest.Peek(r[1],
		'@s')))(SDTest.Call('cSndInterface_StrVar', 'tbispi', sndif, name, 0, 0)))
for name in ('abc00_ini', 'abc00_end', 'abc00_dopp', 'abc00_time', 'abc00_pitch', 'abc00_vol0', 'abc00_vol4',
		'abc00_pitch2', 'abc00_time4', 'abc00_vo', 'abc01_ini', 'abc00_int0'):
	check('cSndInterface_FloatVar set' + fmt((name,)), lambda name=name: (SDTest.Call('cSndInterface_FloatVar', 'tbisDi', sndif, name,
		0.75, 1)[0], PtrArray_Get_ok(sndif, 0)))
	check('cSndInterface_FloatVar get' + fmt((name,)), lambda name=name: SDTest.Call('cSndInterface_FloatVar', 'tbisDi', sndif, name,
		-1.0, 0))
check('cSndInterface_FloatVar vol get', lambda: SDTest.Call('cSndInterface_FloatVar', 'tbisDi', sndif, 'abc00_vol', -1.0, 0))
check('cSndInterface_FloatVar vol 0', lambda: (SDTest.Call('cSndInterface_FloatVar', 'tbisDi', sndif, 'abc00_vol', 0.0, 1)[0],
	PtrArray_Get_ok(sndif, 0)))
check('cSndInterface_IntVar set loop', lambda: (SDTest.Call('cSndInterface_IntVar', 'tbispi', sndif, 'abc00_loop', 5, 1),
	PtrArray_Get_ok(sndif, 0)))
check('cSndInterface_IntVar set int2', lambda: (SDTest.Call('cSndInterface_IntVar', 'tbispi', sndif, 'abc00_int2', 77, 1),
	PtrArray_Get_ok(sndif, 0)))
check('cSndInterface_Tick', lambda: (SDTest.Call('cSndInterface_Tick', 'tvif', sndif, 0.25), PtrArray_Get_ok(sndif, 0),
	PtrArray_Get_ok(sndif, 1)))
check('cSndInterface_StopAll', lambda: (SDTest.Call('cSndInterface_StopAll', 'tvii', sndif, 0), PtrArray_Get_ok(sndif, 0)))
check('cSndInterface_vf04', lambda: SDTest.Call('cSndInterface_Null', 'tbiiiiii', sndif, 1, 2, 3, 4, 5)[0])
check('cSndInterface_Resize same', lambda: (SDTest.Call('cSndInterface_Resize', 'tvii', sndif, 3), PtrArray_Get_ok(sndif, 0)))
sndit = sndz(0xa0)
sndset(sndit, (5.0,) * 40)
check('cSndItem_ctor', lambda: (SDTest.Call('cSndItem_ctor', 'tii', sndit)[0] == sndit, sndsound(sndit), sndf(sndit + 0x40,
	23), sndi(sndit + 0x9c) & 0xff))
check('cSndItem_ctorName', lambda: (SDTest.Call('cSndItem_ctorName', 'tiisi', sndit, 'bank-loop', 0)[0] == sndit,
	sndsound(sndit)))
check('cSndItem_Restart', lambda: (SDTest.Call('cSndItem_Restart', 'tvi', sndit), sndi(sndit + 0x9c) & 0xff, sndsound(sndit)))
check('cSndItem_Tick quiet', lambda: (SDTest.Call('cSndItem_Tick', 'tviif', sndit, 0, 1.5), sndf(sndit + 0x48, 1)))
check('cSndInterface_Destroy', lambda: (SDTest.Call('cSndInterface_Destroy', 'tiii', sndif, 0)[0] == sndif,
	SDTest.Peek('g_SndInterfaces', 'i') == sndif, sndi(sndif) == symva('cSndInterface_vftable')))
SDTest.Poke('g_pSound', sndgp)
SDTest.Poke('g_S_SkipPlay', sndskip)
SDTest.Poke('g_SoundAutoplayVolume', sndauto)

# the samples are FMOD's (NULL when it did not load them), the table goes with the destructor
for name in ('menufocus', 'menuclik'):
	it = sndfind(sndxb, name)
	if it and sndi(it):
		check('CSoundXBox_DeleteSound' + fmt((name,)), lambda name=name: (SDTest.Call('CSoundXBox_DeleteSound', 'tvis', sndxb, name),
			sndfind(sndxb, name) != 0))
	else:
		check('CSoundXBox_DeleteSound' + fmt((name,)), lambda: 'no sample')
check('CSoundXBox_Destroy', lambda: (SDTest.Call('CSoundXBox_Destroy', 'tiii', sndxb, 0)[0] == sndxb, sndi(sndxb) == symva('CSoundSys_vftable'),
	sndi(sndxb + 0x80), sndi(sndxb + 0x84)))

# the wav headers of pack files
sndfp = sndz(0x1c)
sndinfo = sndz(0x10)
for f in ('sounds/generic/menu/menuclik.wav', 'levels/bank/sounds/bank-loop.wav', 'm3d.ini',
		'sounds/generic/scorer/keyboard1.wav'):
	SDTest.Call('cFilePack_ctor', 'tii', sndfp)
	SDTest.Call('cFilePack_Open', 'tiisi', sndfp, f, 1)
	sndset(sndinfo, (0.0,) * 4)
	check('Wav_ReadInfo' + fmt((f,)), lambda: (SDTest.Call('Wav_ReadInfo', 'iii', sndfp, sndinfo)[0], sndi(sndinfo),
		sndi(sndinfo + 4), sndi(sndinfo + 8), sndi(sndinfo + 12)))
	SDTest.Call('cFilePack_Close', 'tii', sndfp)
	SDTest.Call('cFilePack_dtor', 'tvi', sndfp)
SDTest.Call('cFilePack_ctor', 'tii', sndfp)
SDTest.Call('cFilePack_Open', 'tiisi', sndfp, sndfiles[0], 1)
for id in (0x46464952, 0, 0x12345678, 0x20746d66):
	check('Wav_ReadChunk' + fmt((id,)), lambda id=id: SDTest.Call('Wav_ReadChunk', 'iipi', sndfp, 0, id))
SDTest.Call('cFilePack_Close', 'tii', sndfp)
SDTest.Call('cFilePack_dtor', 'tvi', sndfp)
SDTest.Poke('g_SoundShowSetMode', sndshow)
for p in (sndd, sndd2, sndv, snds, snds2, sndss, sndxb, sndfp, sndinfo, sndps, sndif, sndit, sndstr):
	SDTest.Call('Mem_Free', 'vi', p)
