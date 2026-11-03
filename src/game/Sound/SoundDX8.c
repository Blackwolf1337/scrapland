/*
 * Sound/SoundDX8.cpp of the game: CSoundXBox, the backend of CSoundSys on the sound mixer (include/scrap/snd.h, FMOD 3
 * in the original). The samples are loaded from the packs as .wav into a table by sound name; every play takes a free
 * channel. See include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/snd.h"
#include "scrap/sound.h"
#include "scrap/staticinit.h"

#define CSOUNDXBOX_VFTABLE (&CSoundXBox_vftable)
#define g_SoundMainVolume (GVar_SoundMainVolume.value.f)
#define g_SoundSwapChannels (GVar_SoundSwapChannels.value.i)
#define g_S_SkipPlay (GVar_S_SkipPlay.value.i)
#define g_S_SkipUpdates (GVar_S_SkipUpdates.value.i)
static cGVar GVar_SoundSwapChannels;
static cGVar GVar_SoundMainVolume;
static cGVar GVar_S_SkipUpdates;
static cGVar GVar_S_SkipPlay;
static const CSoundSysVtbl CSoundXBox_vftable;
#define SOUND_TABLE_SLOTS 0x1c9
#define SOUND_VOL_MIN (-6400)	/* at or below nothing plays */

void Sound_Nop(void);

static CSoundXBox *CSoundXBox_ctor(CSoundXBox *self) {
	CSoundSys_ctor(&self->sys);
	self->sys.vftable = CSOUNDXBOX_VFTABLE;
	self->sounds.numSlots = SOUND_TABLE_SLOTS;
	self->sounds.slots = operator_new_array(SOUND_TABLE_SLOTS * sizeof(*self->sounds.slots));
	memset(self->sounds.slots, 0, SOUND_TABLE_SLOTS * sizeof(*self->sounds.slots));
	self->sounds.locked = 0;
	self->volume = 0;
	self->swapPan = 0;
	self->sounds.caseSensitive = 0;
	return self;
}

static void CSoundXBox_dtor(CSoundXBox *self) {
	self->sys.vftable = CSOUNDXBOX_VFTABLE;
	SoundStrTable_Clear(&self->sounds);
	CSoundSys_dtor(&self->sys);
}

static CSoundSys *CSoundXBox_Destroy(CSoundSys *base, byte flags) {
	CSoundXBox *self = (CSoundXBox *)base;
	CSoundXBox_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CSoundXBox));
	}
	return base;
}

CSoundXBox *CSoundXBox_New(void) {
	CSoundXBox *p = operator_new(sizeof(CSoundXBox));

	return p ? CSoundXBox_ctor(p) : NULL;
}

static cSoundStrItem *CSoundXBox_Find(CSoundXBox *self, const char *name) {
	return SoundStrTable_Find(&self->sounds, name, HashTable_Slot(&self->sounds, name));
}

/* the table value of name with the parameters of d applied: volumes and pans added, frequencies multiplied */
static byte CSoundXBox_GetPlayDesc(CSoundXBox *self, const char *name, const cSoundDesc *d, cSoundTableValue *out) {
	cSoundStrItem *it = CSoundXBox_Find(self, name);

	if (!it) {
		Con_Warnf("Sound not found: %s\n", name);
		return 0;
	}
	*out = it->value;
	out->desc.volume += self->volume + d->volume;
	int pan = d->pan + out->desc.pan;
	out->desc.pan = pan;
	out->desc.freq = d->freq * out->desc.freq;
	out->desc.loop = d->loop;
	out->desc.prio = d->prio;
	if (self->swapPan) {
		out->desc.pan = -pan;
	}
	return 1;
}

static void CSoundXBox_Init(CSoundSys *base) {
	CSoundXBox *self = (CSoundXBox *)base;
	if (g_SoundInit) {
		self->swapPan = g_SoundSwapChannels != 0;
		CSoundSys_InitFmod(&self->sys);
		SoundSys_Update(self);
	}
}

static void CSoundXBox_Close(CSoundSys *base) {
	CSoundXBox *self = (CSoundXBox *)base;
	CSoundSys_Close(&self->sys);
	SoundSys_DeleteAllSounds(self);
}

static float g_SndFreqScale = 1.0f;

/* the volume offset follows SoundMainVolume (at a change everything stops), the pitch the timer */
static void CSoundXBox_Update(CSoundSys *base) {
	CSoundXBox *self = (CSoundXBox *)base;
	CSoundSys_Nop(&self->sys);
	int mult = 1;
	if (g_TimerFreqMult > mult) {
		mult = g_TimerFreqMult;
	}
	g_SndFreqScale = g_S_PitchScale / (float)mult;
	int v = (int)((float)Snd_Volume(g_SoundMainVolume) / 0x1.333334p-1f);
	if (self->volume != v) {
		self->volume = v;
		SoundSys_StopAllSounds(self);
	}
}

/* name is a file name; loads <name without its extension>.wav once under its sound name */
static void CSoundXBox_Load(CSoundXBox *self, const char *name, int unused) {
	char sndname[SOUND_NAME_MAX_LEN];
	char path[0x100 + 5];
	cSoundTableValue v;
	cFilePack f;
	uint32_t mem0, mem1, memMax;

	(void)unused;
	SoundSys_GetSoundName(self, name, sndname, sizeof(sndname));
	cSoundDesc_Init(&v.desc);
	if (CSoundXBox_Find(self, sndname)) {
		return;
	}
	Con_Debugf("Loading sound: %s\n", name);
	v.sample = NULL;
	v.freq = 44100;
	cSoundDesc_Init(&v.desc);
	cFilePack_ctor(&f);
	Str_Printf(path, 0x100, "%.*s", (int)strlen(name) - 4, name);
	memcpy(path + strlen(path), ".wav", 5);
	cFilePack_Open(&f.file.base, path, 1);
	int size = cFilePack_Size(&f.file.base);
	void *data = operator_new_array(size);
	CHECK(cFilePack_Read(&f.file.base, data, size) == size);
	CHECK(cFilePack_Close(&f.file.base));
	Snd_MemoryStats(&mem0, &memMax);
	v.sample = Snd_SampleLoad(data, size);
	if (!v.sample) {
		Con_Bluef("SOUND ERROR: %s: %s\n", Fmod_ErrorString(Snd_GetError()), path);
	}
	if (Snd_SampleStereo(v.sample)) {
		v.desc.volume = Snd_Volume(0.5f);
	}
	v.freq = Snd_SampleRate(v.sample);
	Snd_MemoryStats(&mem1, &memMax);
	Con_Verbosef("SOUND MEMORY: %d (%+d) %s, %d Hz\n", (int)(mem1 >> 10), (int)(mem1 - mem0) / 1024, path, v.freq);
	if (data) {
		operator_delete_sized(data, 0);
	}
	cFilePack_dtor(&f);
	CHECK(!self->sounds.locked);
	uint32_t slot = HashTable_Slot(&self->sounds, sndname);
	if (!SoundStrTable_Find(&self->sounds, sndname, slot)) {
		SoundStrTable_Insert(&self->sounds, sndname, slot, &v);
	} else {
		CheckFailed("SoundTable.Add( sndname, sound )", __FILE__, __TIMESTAMP__, __LINE__);
	}
}

/* vtable slot 0x10, not available here */
static void CSoundXBox_LoadSound2(CSoundSys *base, const char *name) {
	CSoundXBox *self = (CSoundXBox *)base;
	(void)self;
	(void)name;
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
}

static void CSoundXBox_LoadSound(CSoundSys *base, const char *name) {
	CSoundXBox *self = (CSoundXBox *)base;
	CSoundXBox_Load(self, name, 1);
}

static void CSoundXBox_DeleteSound(CSoundSys *base, const char *name) {
	CSoundXBox *self = (CSoundXBox *)base;
	cSoundDesc tmp;

	cSoundDesc_Init(&tmp);
	cSoundStrItem *it = CSoundXBox_Find(self, name);
	if (it) {
		Snd_SampleFree(it->value.sample);
		CHECK(SoundStrTable_Delete(&self->sounds, name));
	}
}

static void CSoundXBox_StopAllSounds(CSoundSys *base) {
	CSoundXBox *self = (CSoundXBox *)base;
	(void)self;
	Sound_Nop();
	for (int i = 0; i < SND_CHANNELS; i++) {
		if (!Snd_GetReserved(i)) {
			Snd_Stop(i);
		}
	}
}

static void CSoundXBox_DeleteAllSounds(CSoundSys *base) {
	CSoundXBox *self = (CSoundXBox *)base;
	HashTable *t = &self->sounds;

	SoundSys_StopAllSounds(self);
	CSoundSys_CallVf1c(&self->sys);
	for (;;) {
		int i = 0;
		if ((int)t->numSlots < 1) {
			return;
		}
		while (!t->slots[i]) {
			if (++i >= (int)t->numSlots) {
				return;
			}
		}
		cSoundStrItem *it = (cSoundStrItem *)t->slots[i];
		Snd_SampleFree(it->value.sample);
		CHECK(SoundStrTable_Delete(t, it->key));
	}
}

static byte CSoundXBox_GetDesc(CSoundSys *base, const char *name, cSoundDesc *out) {
	CSoundXBox *self = (CSoundXBox *)base;
	cSoundDesc tmp;

	cSoundDesc_Init(&tmp);
	cSoundStrItem *it = CSoundXBox_Find(self, name);
	if (it) {
		*out = it->value.desc;
		return 1;
	}
	Con_Warnf("Sound not found: %s\n", name);
	return 0;
}

/* vtable slot 0x28, not available here */
static byte CSoundXBox_SetDesc(CSoundSys *base, const char *name, const cSoundDesc *d) {
	CSoundXBox *self = (CSoundXBox *)base;
	(void)self;
	(void)name;
	(void)d;
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	return 1;
}

/* the channel or 0 */
static int Fmod_Play(void *sample, const cSoundDesc *d, const char *name);
static int CSoundXBox_PlayDesc(CSoundXBox *self, const char *name, const cSoundDesc *d) {
	cSoundTableValue v;

	if (self->volume <= SOUND_VOL_MIN || d->volume <= SOUND_VOL_MIN) {
		return 0;
	}
	cSoundDesc_Init(&v.desc);
	if (!CSoundXBox_GetPlayDesc(self, name, d, &v)) {
		return 0;
	}
	return Fmod_Play(v.sample, &v.desc, name);
}

static int CSoundXBox_PlayVPF(CSoundXBox *self, const char *name, int volume, int pan, float freq) {
	cSoundTableValue v;
	cSoundDesc d;

	if (self->volume <= SOUND_VOL_MIN || volume <= SOUND_VOL_MIN) {
		return 0;
	}
	cSoundDesc_Init(&v.desc);
	cSoundStrItem *it = CSoundXBox_Find(self, name);
	if (!it) {
		Con_Warnf("Sound not found: %s\n", name);
		return 0;
	}
	v = it->value;
	cSoundDesc_Init(&d);
	d.volume = v.desc.volume + volume + self->volume;
	d.freq = v.desc.freq * freq;
	d.pan = pan + v.desc.pan;
	d.loop = v.desc.loop;
	d.prio = v.desc.prio;
	if (self->swapPan) {
		d.pan = -d.pan;
	}
	return Fmod_Play(v.sample, &d, name);
}

static int CSoundXBox_PlaySound(CSoundSys *base, const char *name, const cSoundDesc *d, int *handle) {
	CSoundXBox *self = (CSoundXBox *)base;
	int ch = CSoundXBox_PlayDesc(self, name, d);

	if (handle) {
		*handle = ch;
	}
	return ch != 0;
}

static int CSoundXBox_PlayName(CSoundSys *base, const char *name) {
	CSoundXBox *self = (CSoundXBox *)base;
	return CSoundXBox_PlayVPF(self, name, 0, 0, 1.0f);
}

static int CSoundXBox_Play(CSoundSys *base, const char *name, int volume, int pan) {
	CSoundXBox *self = (CSoundXBox *)base;
	return CSoundXBox_PlayVPF(self, name, volume, pan, 1.0f);
}

static int CSoundXBox_PlayAt(CSoundSys *base, const char *name, const vec3_t *pos, int volume) {
	CSoundXBox *self = (CSoundXBox *)base;
	cSoundDesc d;

	cSoundDesc_Init(&d);
	cSoundDesc_SetPos(&d, pos);
	d.volume += volume;
	return CSoundXBox_PlayVPF(self, name, d.volume, d.pan, 1.0f);
}

static byte Fmod_SetChannel(int channel, const cSoundDesc *d);
static byte CSoundXBox_UpdateSound(CSoundSys *base, const char *name, const cSoundDesc *d, int handle) {
	CSoundXBox *self = (CSoundXBox *)base;
	cSoundTableValue v;

	if (g_S_SkipUpdates) {
		return 1;
	}
	cSoundDesc_Init(&v.desc);
	if (!CSoundXBox_GetPlayDesc(self, name, d, &v)) {
		return 0;
	}
	return Fmod_SetChannel(handle, &v.desc);
}

static int CSoundXBox_StopSound(CSoundSys *base, int handle) {
	CSoundXBox *self = (CSoundXBox *)base;
	(void)self;
	return Snd_Stop(handle);
}

/* the channel functions (FMOD's in the original; the error texts are FMOD 3's, Snd_GetError numbers them alike) */

const char *Fmod_ErrorString(int err) {
	const char *const texts[] = {
		"No errors", "Cannot call this command after FSOUND_Init.  Call FSOUND_Close first.", "This command failed because FSOUND_Init was not called",
		"Error initializing output device.", "The output device is already in use and cannot be reused.", "Playing the sound failed.",
		"Soundcard does not support the features needed for this soundsystem (16bit stereo output)", "Error setting cooperative level for hardware.", "Error creating hardware sound buffer.",
		"File not found", "Unknown file format", "Error loading file",
		"Not enough memory ", "The version number of this file format is not supported", "An invalid parameter was passed to this function",
		"Tried to use an EAX command on a non EAX enabled channel or output.", "Failed to allocate a new channel", "Recording not supported on this device",
		"Required Mediaplayer codec is not installed",
	};

	if ((uint32_t)err >= sizeof(texts) / sizeof(texts[0])) {
		return "Unknown error";
	}
	return texts[err];
}

/* volume, pan and frequency of a playing channel; 0 when it does not play */
static byte Fmod_SetChannel(int channel, const cSoundDesc *d) {
	g_SoundAutoplayVolume = -48.0f;
	if (!Snd_IsPlaying(channel)) {
		return 0;
	}
	int deffreq = Snd_SampleRate(Snd_ChannelSample(channel));
	float f = d->freq * g_SndFreqScale;
	int freq = deffreq;
	if (f != 1.0f) {
		freq = (int)((double)((float)deffreq * f) + 0.5);
	}
	float v = (float)pow(2.0, (double)d->volume * 0.6 / 600.0);
	float vol = 0.0f >= v ? 0.0f : Float_MinSS(1.0f, v);
	Snd_SetVolume(channel, (int)(vol * 255.0f));
	int pan = d->pan / 20 + 128;
	if (pan <= 0) {
		pan = 0;
	} else if (pan >= 255) {
		pan = 255;
	}
	if (!g_isDolby) {
		Snd_SetPan(channel, pan);
	}
	if (freq <= 200) {
		freq = 200;
	} else if (freq >= 100000) {
		freq = 100000;
	}
	Snd_SetFrequency(channel, freq);
	return 1;
}

/* plays a sample with the parameters, the channel or 0 */
static int Fmod_Play(void *sample, const cSoundDesc *d, const char *name) {
	(void)name;
	if (!sample || g_S_SkipPlay) {
		return 0;
	}
	int ch = Snd_Play(sample, 1);
	if (ch == -1) {
		return 0;
	}
	Fmod_SetChannel(ch, d);
	Snd_SetLoop(ch, d->loop != 0);
	Snd_SetPaused(ch, 0);
	return ch;
}

cGVar GVar_S_Show;

#define g_S_Show (GVar_S_Show.value.i)		/* S_Show: 1 the channels, 3 the memory */

/* vf31 (DebugDraw): S_Show 3 the memory of the samples, 1 the channels with their state and the volume, pan and
 * frequency of each in two columns (the original also listed FMOD's hardware channels) */
static void CSoundXBox_DebugDraw(CSoundSys *base, int xi, int yi) {
	CSoundXBox *self = (CSoundXBox *)base;
	uint32_t cur, max;

	(void)self;
	float x = (float)xi + 20.0f;
	if (g_S_Show == 3) {
		Snd_MemoryStats(&cur, &max);
		CFont_PrintfColorShadow(g_FontHoratioSmall, x, (float)yi + 20.0f, 0xffff00, "memory: %d %d", cur >> 10,
			max >> 10);
		return;
	}
	if (g_S_Show != 1) {
		return;
	}
	CFont *f = g_FontTerminal8;
	int playing = Snd_ChannelsPlaying();
	CFont_PrintfColorShadow(f, x, (float)yi, 0xffff00, "SoundXBox: %s", Snd_DeviceName());
	float y = (float)yi + 10.0f;
	CFont_PrintfColorShadow(f, x, y, playing ? 0xff0000 : 0xff00, "Channels playing: %d", playing);
	for (int i = 0, k = 0; i < SND_CHANNELS; i++, k += 5) {
		float cx = (float)k + x + 30.0f;
		CFont_PrintfColorShadow(f, cx, y + 10.0f, 0xff00, "%s", Snd_IsPlaying(i) ? "*" : ".");
		CFont_PrintfColorShadow(f, cx, y + 20.0f, 0xff00, "%s", Snd_GetReserved(i) ? "R" : "");
	}
	y = y + 20.0f;
	float top = y;
	float cx = x + 10.0f;
	for (int i = 0; i < SND_CHANNELS; i++) {
		if (i == SND_CHANNELS / 2) {
			cx = cx + 300.0f;
			y = top;
		}
		y = y + 10.0f;
		CFont_PrintfColorShadow(f, cx, y, 0xff00, "%2d", i);
		if (Snd_IsPlaying(i)) {
			CFont_PrintfColorShadow(f, f->x, y, 0xff00, "  %s", Snd_GetLoop(i) ? "L" : " ");
			CFont_PrintfColorShadow(f, f->x, y, 0xff00, "  vpf: %3d %3d %5d %3d", Snd_GetVolume(i), Snd_GetPan(i),
				Snd_GetFrequency(i), SND_PRIORITY);
		} else {
			CFont_PrintfColorShadow(f, f->x, y, 0x8000, "  ---");
		}
	}
	Snd_MemoryStats(&cur, &max);
	CFont_PrintfColorShadow(f, x, y + 10.0f, 0xffff00, "channels: %d/%d", playing, SND_CHANNELS);
	CFont_PrintfColorShadow(f, x, y + 20.0f, 0xffff00, "memory: %d %d", cur >> 10, max >> 10);
}

/* the vtable of CSoundXBox */
static const CSoundSysVtbl CSoundXBox_vftable = {
	.Destroy = CSoundXBox_Destroy,
	.Init = CSoundXBox_Init,
	.Close = CSoundXBox_Close,
	.Update = CSoundXBox_Update,
	.LoadSound2 = CSoundXBox_LoadSound2,
	.LoadSound = CSoundXBox_LoadSound,
	.DeleteSound = CSoundXBox_DeleteSound,
	.StopAllSounds = CSoundXBox_StopAllSounds,
	.DeleteAllSounds = CSoundXBox_DeleteAllSounds,
	.GetDesc = CSoundXBox_GetDesc,
	.SetDesc = CSoundXBox_SetDesc,
	.PlaySound = CSoundXBox_PlaySound,
	.PlayName = CSoundXBox_PlayName,
	.Play = CSoundXBox_Play,
	.PlayAt = CSoundXBox_PlayAt,
	.UpdateSound = CSoundXBox_UpdateSound,
	.StopSound = CSoundXBox_StopSound,
	.SetVolume = CSoundSys_SetVolume,
	.SetPan = CSoundSys_SetPan,
	.SetFreq = CSoundSys_SetFreq,
	.SetLoop = CSoundSys_SetLoop,
	.SetPrio = CSoundSys_SetPrio,
	.ResetListeners = CSoundSys_ResetListeners,
	.SetListener = CSoundSys_SetListener,
	.Compute3D = CSoundSys_Compute3D,
	.SetMusic = CSoundSys_SetMusic,
	.vf1a = CSoundSys_Nop,
	.StopMusic = CSoundSys_StopMusic,
	.vf1c = CSoundSys_Nop,
	.SetMusicVolume = CSoundSys_SetMusicVolume,
	.GetSoundName = CSoundSys_GetSoundName,
	.DebugDraw = CSoundXBox_DebugDraw,
	.OpenVoice = CSoundSys_OpenVoice,
	.PlayVoice = CSoundSys_PlayVoice,
	.StopVoice = CSoundSys_StopVoice,
	.VoiceRemain = CSoundSys_VoiceRemain,
	.SetVoiceVolume = CSoundSys_SetVoiceVolume,
	.SetVoiceParam = CSoundSys_SetVoiceParam,
	.SetVoiceString = CSoundSys_SetVoiceString,
	.GetVoiceString = CSoundSys_GetVoiceString,
	.VoiceUnused = CSoundSys_VoiceUnused,
};

static cGVar GVar_SoundMainVolume;
static cGVar GVar_S_SkipUpdates;
static cGVar GVar_SoundSwapChannels;
cGVar GVar_S_PitchScale;
static cGVar GVar_S_SkipPlay;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_SoundMainVolume, &GVar_SoundMainVolume, "SoundMainVolume", 10, "Sound main volume (0..1)\n", 1.0f)
INIT_GVAR_INT(Init_GVar_SoundSwapChannels, &GVar_SoundSwapChannels, "SoundSwapChannels", 2, "0 = Normal, 1 = Swap left/right channels", 0)
INIT_GVAR_INT(Init_GVar_S_Show, &GVar_S_Show, "S_Show", 0, "Show sound debug info on screen (0/1/2)", 0)
INIT_GVAR_INT(Init_GVar_S_SkipPlay, &GVar_S_SkipPlay, "S_SkipPlay", 0, "Disable sound playing (0/1)", 0)
INIT_GVAR_INT(Init_GVar_S_SkipUpdates, &GVar_S_SkipUpdates, "S_SkipUpdates", 0, "Disable sound updates (0/1)", 0)
INIT_GVAR_FLOAT(Init_GVar_S_PitchScale, &GVar_S_PitchScale, "S_PitchScale", 0, "Pitch scale (def. 1.0)", 1.0f)
