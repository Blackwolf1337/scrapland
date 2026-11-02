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

#define SOUND_TABLE_SLOTS 0x1c9
#define SOUND_VOL_MIN (-6400)	/* at or below nothing plays */

void Sound_Nop(void);

CSoundXBox *CSoundXBox_ctor(CSoundXBox *self) {
	CSoundSys_ctor(&self->sys);
	self->sys.vftable = CSoundXBox_vftable;
	self->sounds.numSlots = SOUND_TABLE_SLOTS;
	self->sounds.slots = operator_new_array(SOUND_TABLE_SLOTS * 4);
	memset(self->sounds.slots, 0, SOUND_TABLE_SLOTS * 4);
	self->sounds.locked = 0;
	self->volume = 0;
	self->swapPan = 0;
	self->sounds.caseSensitive = 0;
	return self;
}

void CSoundXBox_dtor(CSoundXBox *self) {
	self->sys.vftable = CSoundXBox_vftable;
	SoundStrTable_Clear(&self->sounds);
	CSoundSys_dtor(&self->sys);
}

CSoundXBox *CSoundXBox_Destroy(CSoundXBox *self, byte flags) {
	CSoundXBox_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CSoundXBox));
	}
	return self;
}

CSoundXBox *CSoundXBox_New(void) {
	CSoundXBox *p = operator_new(sizeof(CSoundXBox));

	return p ? CSoundXBox_ctor(p) : NULL;
}

static cSoundStrItem *CSoundXBox_Find(CSoundXBox *self, const char *name) {
	return SoundStrTable_Find(&self->sounds, name, HashTable_Slot(&self->sounds, name));
}

/* the table value of name with the parameters of d applied: volumes and pans added, frequencies multiplied */
byte CSoundXBox_GetPlayDesc(CSoundXBox *self, const char *name, const cSoundDesc *d, cSoundTableValue *out) {
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

void CSoundXBox_Init(CSoundXBox *self) {
	if (g_SoundInit) {
		self->swapPan = g_SoundSwapChannels != 0;
		CSoundSys_InitFmod(&self->sys);
		self->sys.vftable->Update(&self->sys);
	}
}

void CSoundXBox_Close(CSoundXBox *self) {
	CSoundSys_Close(&self->sys);
	self->sys.vftable->DeleteAllSounds(&self->sys);
}

/* the volume offset follows SoundMainVolume (at a change everything stops), the pitch the timer */
void CSoundXBox_Update(CSoundXBox *self) {
	CSoundSys_Nop(&self->sys);
	int mult = 1;
	if (g_TimerFreqMult > mult) {
		mult = g_TimerFreqMult;
	}
	g_SndFreqScale = g_S_PitchScale / (float)mult;
	int v = (int)((float)Snd_Volume(g_SoundMainVolume) / 0x1.333334p-1f);
	if (self->volume != v) {
		self->volume = v;
		self->sys.vftable->StopAllSounds(&self->sys);
	}
}

/* name is a file name; loads <name without its extension>.wav once under its sound name */
void CSoundXBox_Load(CSoundXBox *self, const char *name, int unused) {
	char sndname[SOUND_NAME_MAX_LEN];
	char path[0x100 + 5];
	cSoundTableValue v;
	cFilePack f;
	uint32_t mem0, mem1, memMax;

	(void)unused;
	self->sys.vftable->GetSoundName(&self->sys, name, sndname, sizeof(sndname));
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
	cFilePack_Open(&f, path, 1);
	int size = cFilePack_Size(&f);
	void *data = operator_new_array(size);
	CHECK(cFilePack_Read(&f, data, size) == size);
	CHECK(cFilePack_Close(&f));
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
void CSoundXBox_LoadSound2(CSoundXBox *self, const char *name) {
	(void)self;
	(void)name;
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
}

void CSoundXBox_LoadSound(CSoundXBox *self, const char *name) {
	CSoundXBox_Load(self, name, 1);
}

void CSoundXBox_DeleteSound(CSoundXBox *self, const char *name) {
	cSoundDesc tmp;

	cSoundDesc_Init(&tmp);
	cSoundStrItem *it = CSoundXBox_Find(self, name);
	if (it) {
		Snd_SampleFree(it->value.sample);
		CHECK(SoundStrTable_Delete(&self->sounds, name));
	}
}

void CSoundXBox_StopAllSounds(CSoundXBox *self) {
	(void)self;
	Sound_Nop();
	for (int i = 0; i < SND_CHANNELS; i++) {
		if (!Snd_GetReserved(i)) {
			Snd_Stop(i);
		}
	}
}

void CSoundXBox_DeleteAllSounds(CSoundXBox *self) {
	HashTable *t = &self->sounds;

	self->sys.vftable->StopAllSounds(&self->sys);
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

byte CSoundXBox_GetDesc(CSoundXBox *self, const char *name, cSoundDesc *out) {
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
byte CSoundXBox_SetDesc(CSoundXBox *self, const char *name, const cSoundDesc *d) {
	(void)self;
	(void)name;
	(void)d;
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	return 1;
}

/* the channel or 0 */
int CSoundXBox_PlayDesc(CSoundXBox *self, const char *name, const cSoundDesc *d) {
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

int CSoundXBox_PlayVPF(CSoundXBox *self, const char *name, int volume, int pan, float freq) {
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

int CSoundXBox_PlaySound(CSoundXBox *self, const char *name, const cSoundDesc *d, int *handle) {
	int ch = CSoundXBox_PlayDesc(self, name, d);

	if (handle) {
		*handle = ch;
	}
	return ch != 0;
}

int CSoundXBox_PlayName(CSoundXBox *self, const char *name) {
	return CSoundXBox_PlayVPF(self, name, 0, 0, 1.0f);
}

int CSoundXBox_Play(CSoundXBox *self, const char *name, int volume, int pan) {
	return CSoundXBox_PlayVPF(self, name, volume, pan, 1.0f);
}

int CSoundXBox_PlayAt(CSoundXBox *self, const char *name, const vec3_t *pos, int volume) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	cSoundDesc_SetPos(&d, pos);
	d.volume += volume;
	return CSoundXBox_PlayVPF(self, name, d.volume, d.pan, 1.0f);
}

byte CSoundXBox_UpdateSound(CSoundXBox *self, const char *name, const cSoundDesc *d, int handle) {
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

int CSoundXBox_StopSound(CSoundXBox *self, int handle) {
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

byte Fmod_SetChannel(int channel, const cSoundDesc *d) {
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

int Fmod_Play(void *sample, const cSoundDesc *d, const char *name) {
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

extern cGVar GVar_S_Show;
#define g_S_Show (GVar_S_Show.value.i)		/* S_Show: 1 the channels, 3 the memory */

/* vf31 (DebugDraw): S_Show 3 the memory of the samples, 1 the channels with their state and the volume, pan and
 * frequency of each in two columns (the original also listed FMOD's hardware channels) */
void CSoundXBox_DebugDraw(CSoundXBox *self, int xi, int yi) {
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
