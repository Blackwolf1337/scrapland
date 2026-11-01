/*
 * Sound/SoundDX8.cpp of the game: CSoundXBox, the FMOD 3 backend of CSoundSys. The samples are loaded from the
 * packs as .wav into a table by sound name; every play takes a free FMOD channel. See include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/fmod.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/sound.h"

#define SOUND_TABLE_SLOTS 0x1c9
#define SOUND_VOL_MIN (-6400)	/* at or below nothing plays */

void Sound_Nop(void);

CSoundXBox *__fastcall CSoundXBox_ctor(CSoundXBox *self) {
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

void __fastcall CSoundXBox_dtor(CSoundXBox *self) {
	self->sys.vftable = CSoundXBox_vftable;
	SoundStrTable_Clear(&self->sounds);
	CSoundSys_dtor(&self->sys);
}

CSoundXBox *__thiscall CSoundXBox_Destroy(CSoundXBox *self, byte flags) {
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
byte __thiscall CSoundXBox_GetPlayDesc(CSoundXBox *self, const char *name, const cSoundDesc *d, cSoundTableValue *out) {
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

void __fastcall CSoundXBox_Init(CSoundXBox *self) {
	if (g_SoundInit) {
		self->swapPan = g_SoundSwapChannels != 0;
		CSoundSys_InitFmod(&self->sys);
		self->sys.vftable->Update(&self->sys);
	}
}

void __fastcall CSoundXBox_Close(CSoundXBox *self) {
	CSoundSys_Close(&self->sys);
	self->sys.vftable->DeleteAllSounds(&self->sys);
}

/* the volume offset follows SoundMainVolume (at a change everything stops), the pitch the timer */
void __fastcall CSoundXBox_Update(CSoundXBox *self) {
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
void __thiscall CSoundXBox_Load(CSoundXBox *self, const char *name, int unused) {
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
	FSOUND_GetMemoryStats(&mem0, &memMax);
	v.sample = FSOUND_Sample_Load(FSOUND_UNMANAGED, data, 0x88000, 0, size);
	if (!v.sample) {
		Con_Bluef("SOUND ERROR: %s: %s\n", Fmod_ErrorString(FSOUND_GetError()), path);
	}
	if (FSOUND_Sample_GetMode(v.sample) & FSOUND_STEREO) {
		v.desc.volume = Snd_Volume(0.5f);
	}
	FSOUND_Sample_GetDefaults(v.sample, &v.freq, NULL, NULL, NULL);
	FSOUND_GetMemoryStats(&mem1, &memMax);
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
void __thiscall CSoundXBox_LoadSound2(CSoundXBox *self, const char *name) {
	(void)self;
	(void)name;
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
}

void __thiscall CSoundXBox_LoadSound(CSoundXBox *self, const char *name) {
	CSoundXBox_Load(self, name, 1);
}

void __thiscall CSoundXBox_DeleteSound(CSoundXBox *self, const char *name) {
	cSoundDesc tmp;

	cSoundDesc_Init(&tmp);
	cSoundStrItem *it = CSoundXBox_Find(self, name);
	if (it) {
		FSOUND_Sample_Free(it->value.sample);
		CHECK(SoundStrTable_Delete(&self->sounds, name));
	}
}

void __fastcall CSoundXBox_StopAllSounds(CSoundXBox *self) {
	(void)self;
	Sound_Nop();
	int n = FSOUND_GetMaxChannels();
	for (int i = 0; i < n; i++) {
		if (!FSOUND_GetReserved(i)) {
			FSOUND_StopSound(i);
		}
	}
}

void __fastcall CSoundXBox_DeleteAllSounds(CSoundXBox *self) {
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
		FSOUND_Sample_Free(it->value.sample);
		CHECK(SoundStrTable_Delete(t, it->key));
	}
}

byte __thiscall CSoundXBox_GetDesc(CSoundXBox *self, const char *name, cSoundDesc *out) {
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
byte __thiscall CSoundXBox_SetDesc(CSoundXBox *self, const char *name, const cSoundDesc *d) {
	(void)self;
	(void)name;
	(void)d;
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	return 1;
}

/* the channel or 0 */
int __thiscall CSoundXBox_PlayDesc(CSoundXBox *self, const char *name, const cSoundDesc *d) {
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

int __thiscall CSoundXBox_PlayVPF(CSoundXBox *self, const char *name, int volume, int pan, float freq) {
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

int __thiscall CSoundXBox_PlaySound(CSoundXBox *self, const char *name, const cSoundDesc *d, int *handle) {
	int ch = CSoundXBox_PlayDesc(self, name, d);

	if (handle) {
		*handle = ch;
	}
	return ch != 0;
}

int __thiscall CSoundXBox_PlayName(CSoundXBox *self, const char *name) {
	return CSoundXBox_PlayVPF(self, name, 0, 0, 1.0f);
}

int __thiscall CSoundXBox_Play(CSoundXBox *self, const char *name, int volume, int pan) {
	return CSoundXBox_PlayVPF(self, name, volume, pan, 1.0f);
}

int __thiscall CSoundXBox_PlayAt(CSoundXBox *self, const char *name, const vec3_t *pos, int volume) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	cSoundDesc_SetPos(&d, pos);
	d.volume += volume;
	return CSoundXBox_PlayVPF(self, name, d.volume, d.pan, 1.0f);
}

byte __thiscall CSoundXBox_UpdateSound(CSoundXBox *self, const char *name, const cSoundDesc *d, int handle) {
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

/* a tail jump to FSOUND_StopSound in the original */
int __thiscall CSoundXBox_StopSound(CSoundXBox *self, int handle) {
	(void)self;
	return FSOUND_StopSound(handle);
}

/* the FMOD channel functions */

const char *Fmod_ErrorString(int err) {
	const char *const texts[] = {
		GAME_STR(str_3ca3c8), GAME_STR(str_3ca3d8), GAME_STR(str_3ca420),
		GAME_STR(str_3ca474), GAME_STR(str_3ca498), GAME_STR(str_3ca458),
		GAME_STR(str_3ca4d8), GAME_STR(str_3ca534), GAME_STR(str_3ca564),
		GAME_STR(str_3ca58c), GAME_STR(str_3ca59c), GAME_STR(str_3ca5b0),
		GAME_STR(str_3ca5c4), GAME_STR(str_3ca5d8), GAME_STR(str_3ca610),
		GAME_STR(str_3ca648), GAME_STR(str_3ca68c), GAME_STR(str_3ca6b0),
		GAME_STR(str_3ca6d8),
	};

	if ((uint32_t)err >= sizeof(texts) / sizeof(texts[0])) {
		return GAME_STR(str_3ca704);	/* "Unknown error" */
	}
	return texts[err];
}

byte Fmod_SetChannel(int channel, const cSoundDesc *d) {
	int deffreq;

	g_SoundAutoplayVolume = -48.0f;
	if (!FSOUND_IsPlaying(channel)) {
		return 0;
	}
	FSOUND_Sample_GetDefaults(FSOUND_GetCurrentSample(channel), &deffreq, NULL, NULL, NULL);
	float f = d->freq * g_SndFreqScale;
	int freq = deffreq;
	if (f != 1.0f) {
		freq = (int)((double)((float)deffreq * f) + 0.5);
	}
	float v = (float)Game_Pow(2.0, (double)d->volume * 0.6 / 600.0);
	float vol = 0.0f >= v ? 0.0f : Float_MinSS(1.0f, v);
	FSOUND_SetVolume(channel, (int)(vol * 255.0f));
	int pan = d->pan / 20 + 128;
	if (pan <= 0) {
		pan = 0;
	} else if (pan >= 255) {
		pan = 255;
	}
	if (!g_isDolby) {
		FSOUND_SetPan(channel, pan);
	}
	if (freq <= 200) {
		freq = 200;
	} else if (freq >= 100000) {
		freq = 100000;
	}
	FSOUND_SetFrequency(channel, freq);
	return 1;
}

int Fmod_Play(void *sample, const cSoundDesc *d, const char *name) {
	(void)name;
	if (!sample || g_S_SkipPlay) {
		return 0;
	}
	int ch = FSOUND_PlaySoundEx(FSOUND_FREE, sample, NULL, 1);
	if (ch == -1) {
		return 0;
	}
	Fmod_SetChannel(ch, d);
	FSOUND_SetLoopMode(ch, (d->loop != 0) + 1);
	FSOUND_SetPaused(ch, 0);
	return ch;
}

#define g_S_Show GAME_GLOBAL_IN(int, GVar_S_Show, 16)		/* S_Show: 1 the channels, 3 the memory */

/* vf31 (DebugDraw): S_Show 3 the memory of FMOD, 1 the software and hardware channels with their state and the
 * sample, volume, pan and frequency of each hardware channel in two columns */
void __thiscall CSoundXBox_DebugDraw(CSoundXBox *self, int xi, int yi) {
	int hw2D, hw3D, hwTotal;
	uint32_t cur, max;

	(void)self;
	float x = (float)xi + 20.0f;
	if (g_S_Show == 3) {
		FSOUND_GetMemoryStats(&cur, &max);
		CFont_PrintfColorShadow(g_FontHoratioSmall, x, (float)yi + 20.0f, 0xffff00, "memory: %d %d", cur >> 10,
			max >> 10);
		return;
	}
	if (g_S_Show != 1) {
		return;
	}
	CFont *f = g_FontTerminal8;
	FSOUND_GetNumHWChannels(&hw2D, &hw3D, &hwTotal);
	int maxc = FSOUND_GetMaxChannels();
	int playing = FSOUND_GetChannelsPlaying();
	int sw = maxc - hwTotal;
	CFont_PrintfColorShadow(g_FontTerminal8, x, (float)yi, 0xffff00, "SoundXBox:");
	CFont_PrintfColorShadow(g_FontTerminal8, x, (float)yi + 10.0f, 0xff00, "NumSWChannels: %d", sw);
	CFont_PrintfColorShadow(g_FontTerminal8, x, (float)yi + 20.0f, 0xff00, "NumHWChannels: %d %d %d", hw2D, hw3D,
		hwTotal);
	CFont_PrintfColorShadow(g_FontTerminal8, x, (float)yi + 30.0f, 0xff00, "SFXMasterVolume: %d",
		FSOUND_GetSFXMasterVolume());
	float y = (float)yi + 40.0f;
	int n = 0;
	for (int i = 0; i < sw; i++) {
		if (FSOUND_IsPlaying(i)) {
			n++;
		}
	}
	CFont_PrintfColorShadow(g_FontTerminal8, x, y, n ? 0xff0000 : 0xff00, "Channels playing (SW): %d", n);
	int i = hwTotal ? sw : 0;
	if (i < maxc) {
		int k = 0;
		float y1 = y + 10.0f;
		float y2 = y + 20.0f;
		do {
			float cx = (float)k + x + 30.0f;
			signed char on = (signed char)FSOUND_IsPlaying(i);
			signed char res = (signed char)FSOUND_GetReserved(i);
			CFont_PrintfColorShadow(g_FontTerminal8, cx, y1, 0xff00, "%s", on ? "*" : ".");
			CFont_PrintfColorShadow(g_FontTerminal8, cx, y2, 0xff00, "%s", res ? "R" : "");
			i++;
			k += 5;
		} while (i < maxc);
	}
	y = y + 20.0f;
	float top = y;
	float cx = x + 10.0f;
	i = hwTotal ? maxc - hw2D : 0;
	if (i < maxc) {
		int mid = (i + maxc) / 2;
		do {
			void *smp = FSOUND_GetCurrentSample(i);
			if (i == mid) {
				cx = cx + 300.0f;
				y = top;
			}
			y = y + 10.0f;
			CFont_PrintfColorShadow(g_FontTerminal8, cx, y, 0xff00, "%2d", i);
			if (FSOUND_Sample_GetName(smp)) {
				uint32_t col = FSOUND_GetPriority(i) == 0x100 ? 0xff : 0xffff00;
				CFont_PrintfColorShadow(f, f->x, y, col, "  [%s]", FSOUND_Sample_GetName(smp));
			}
			if (smp) {
				int pri = FSOUND_GetPriority(i);
				int vol = FSOUND_GetVolume(i);
				int pan = FSOUND_GetPan(i);
				int freq = FSOUND_GetFrequency(i);
				signed char on = (signed char)FSOUND_IsPlaying(i);
				uint32_t loop = FSOUND_GetLoopMode(i);
				FSOUND_GetReserved(i);
				uint32_t col = on ? 0xff00 : 0x8000;
				CFont_PrintfColorShadow(f, f->x, y, col, "  %s", loop == FSOUND_LOOP_OFF ? " " : "L");
				CFont_PrintfColorShadow(f, f->x, y, col, "  vpf: %3d %3d %5d %3d", vol, pan, freq, pri);
			} else {
				CFont_PrintfColorShadow(f, f->x, y, 0x8000, "  ---");
			}
			i++;
		} while (i < maxc);
	}
	double cpu = FSOUND_GetCPUUsage();
	CFont_PrintfColorShadow(g_FontTerminal8, x, y + 10.0f, 0xffff00, "channels: %d/%d  cpu %5.02f%%", playing, maxc,
		cpu);
	FSOUND_GetMemoryStats(&cur, &max);
	CFont_PrintfColorShadow(g_FontTerminal8, x, y + 20.0f, 0xffff00, "memory: %d %d", cur >> 10, max >> 10);
}
