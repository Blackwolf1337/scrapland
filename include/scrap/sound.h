/*
 * The sound system (original source: Sound/). g_pSound is the CSoundSys instance, made as its mixer backend
 * CSoundXBox (Sound/SoundDX8.cpp, the name is from the Xbox port); scripts reach it through the SSound module.
 * Sounds are loaded by name into a hash table of samples with their default parameters (cSoundDesc); a cSound
 * (Sound/Sound.cpp) is a named sound with its parameters and the mixer channel it plays on. Volumes are in
 * hundredths of dB (0 full, -100000 off), pans in hundredths of dB too (-10000 left .. 10000 right).
 * Property sets ("PS") are the positional sounds of Sound/PosSound.cpp with the variable interface of the action
 * classes.
 */
#ifndef SCRAP_SOUND_H
#define SCRAP_SOUND_H

#include "scrap/gvar.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"

#define SOUND_NAME_MAX_LEN 0x20
#define SOUND_MAX_LISTENERS 2
#define SOUND_VOL_OFF (-100000)

/* the parameters of a sound */
typedef struct cSoundDesc {
	int volume;
	int pan;
	float freq;			/* frequency factor */
	byte loop;
	byte prio;			/* 0x80 */
	byte pad_0e[2];
	float param3;			/* doppler factor (PlaySound p3, default 1) */
	float param1;			/* distance where the attenuation starts, 0: the global rolloff (p1) */
	float param2;			/* distance of silence, 0: no limit (p2) */
} cSoundDesc;

/* a sound by name with its parameters and the channel it plays on */
typedef struct cSound {
	cSoundDesc desc;
	char name[SOUND_NAME_MAX_LEN];
	int handle;			/* mixer channel, 0 none */
} cSound;

/* a view: position, angles (radians) and velocity, with sin and cos of the negated angles */
typedef struct cSoundListener {
	vec3_t pos;
	vec3_t ang;
	vec3_t vel;
	vec3_t sin;
	vec3_t cos;
} cSoundListener;

typedef struct CSoundSys CSoundSys;

/* the interface (CSoundSys slots 0x10..0x40 pure, CSoundXBox at 0x3ca718) */
typedef struct CSoundSysVtbl {
	CSoundSys *(*Destroy)(CSoundSys *self, byte flags);
	void(*Init)(CSoundSys *self);
	void(*Close)(CSoundSys *self);
	void(*Update)(CSoundSys *self);
	void(*LoadSound2)(CSoundSys *self, const char *name);		/* CHECK(0) */
	void(*LoadSound)(CSoundSys *self, const char *name);
	void(*DeleteSound)(CSoundSys *self, const char *name);
	void(*StopAllSounds)(CSoundSys *self);
	void(*DeleteAllSounds)(CSoundSys *self);
	byte(*GetDesc)(CSoundSys *self, const char *name, cSoundDesc *out);
	byte(*SetDesc)(CSoundSys *self, const char *name, const cSoundDesc *d);	/* CHECK(0) */
	int(*PlaySound)(CSoundSys *self, const char *name, const cSoundDesc *d, int *handle);
	int(*PlayName)(CSoundSys *self, const char *name);
	int(*Play)(CSoundSys *self, const char *name, int volume, int pan);
	int(*PlayAt)(CSoundSys *self, const char *name, const vec3_t *pos, int volume);
	byte(*UpdateSound)(CSoundSys *self, const char *name, const cSoundDesc *d, int handle);
	int(*StopSound)(CSoundSys *self, int handle);
	void(*SetVolume)(CSoundSys *self, const char *name, int volume);
	void(*SetPan)(CSoundSys *self, const char *name, int pan);
	void(*SetFreq)(CSoundSys *self, const char *name, float freq);
	void(*SetLoop)(CSoundSys *self, const char *name, byte loop);
	void(*SetPrio)(CSoundSys *self, const char *name, byte prio);
	void(*ResetListeners)(CSoundSys *self);
	void(*SetListener)(CSoundSys *self, int id, const vec3_t *pos, const vec3_t *ang,
		const vec3_t *vel);
	/* volume, pan and (doppler set) frequency of a sound at pos moving with vel for the nearest listener */
	void(*Compute3D)(CSoundSys *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel,
		byte doppler);
	void(*SetMusic)(CSoundSys *self, const char *name);
	void(*Unused1a)(CSoundSys *self);					/* nothing, never called */
	void(*StopMusic)(CSoundSys *self);
	void(*CloseHook)(CSoundSys *self);					/* nothing in every class, called by Close */
	void(*SetMusicVolume)(CSoundSys *self, float volume);
	/* the sound name of a file name: without directory and extension */
	void(*GetSoundName)(CSoundSys *self, const char *file, char *out, int size);
	void(*DebugDraw)(CSoundSys *self, int x, int y);
	int(*OpenVoice)(CSoundSys *self, const char *name, int voice, int arg);
	int(*PlayVoice)(CSoundSys *self, int voice);
	int(*StopVoice)(CSoundSys *self, int voice);
	byte(*VoiceRemain)(CSoundSys *self, int voice, float *remain);
	int(*SetVoiceVolume)(CSoundSys *self, int voice, float volume);
	int(*SetVoiceParam)(CSoundSys *self, int voice, float v);
	int(*SetVoiceString)(CSoundSys *self, int voice, const char *text);
	int(*GetVoiceString)(CSoundSys *self, int voice, char *buf, int size);
	int(*VoiceUnused)(CSoundSys *self, int voice);			/* CHECK(0) */
} CSoundSysVtbl;

/* calls through the slots of CSoundSysVtbl */
static inline void SoundSys_Close(void *self) {
	VT(CSoundSysVtbl, self)->Close(self);
}

static inline void SoundSys_Compute3D(void *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel, byte doppler) {
	VT(CSoundSysVtbl, self)->Compute3D(self, d, pos, vel, doppler);
}

static inline void SoundSys_DebugDraw(void *self, int x, int y) {
	VT(CSoundSysVtbl, self)->DebugDraw(self, x, y);
}

static inline void SoundSys_DeleteAllSounds(void *self) {
	VT(CSoundSysVtbl, self)->DeleteAllSounds(self);
}

static inline void SoundSys_DeleteSound(void *self, const char *name) {
	VT(CSoundSysVtbl, self)->DeleteSound(self, name);
}

static inline CSoundSys *SoundSys_Destroy(void *self, byte flags) {
	return VT(CSoundSysVtbl, self)->Destroy(self, flags);
}

static inline byte SoundSys_GetDesc(void *self, const char *name, cSoundDesc *out) {
	return VT(CSoundSysVtbl, self)->GetDesc(self, name, out);
}

static inline void SoundSys_GetSoundName(void *self, const char *file, char *out, int size) {
	VT(CSoundSysVtbl, self)->GetSoundName(self, file, out, size);
}

static inline int SoundSys_GetVoiceString(void *self, int voice, char *buf, int size) {
	return VT(CSoundSysVtbl, self)->GetVoiceString(self, voice, buf, size);
}

static inline void SoundSys_Init(void *self) {
	VT(CSoundSysVtbl, self)->Init(self);
}

static inline void SoundSys_LoadSound(void *self, const char *name) {
	VT(CSoundSysVtbl, self)->LoadSound(self, name);
}

static inline void SoundSys_LoadSound2(void *self, const char *name) {
	VT(CSoundSysVtbl, self)->LoadSound2(self, name);
}

static inline int SoundSys_OpenVoice(void *self, const char *name, int voice, int arg) {
	return VT(CSoundSysVtbl, self)->OpenVoice(self, name, voice, arg);
}

static inline int SoundSys_Play(void *self, const char *name, int volume, int pan) {
	return VT(CSoundSysVtbl, self)->Play(self, name, volume, pan);
}

static inline int SoundSys_PlayAt(void *self, const char *name, const vec3_t *pos, int volume) {
	return VT(CSoundSysVtbl, self)->PlayAt(self, name, pos, volume);
}

static inline int SoundSys_PlaySound(void *self, const char *name, const cSoundDesc *d, int *handle) {
	return VT(CSoundSysVtbl, self)->PlaySound(self, name, d, handle);
}

static inline int SoundSys_PlayVoice(void *self, int voice) {
	return VT(CSoundSysVtbl, self)->PlayVoice(self, voice);
}

static inline void SoundSys_ResetListeners(void *self) {
	VT(CSoundSysVtbl, self)->ResetListeners(self);
}

static inline byte SoundSys_SetDesc(void *self, const char *name, const cSoundDesc *d) {
	return VT(CSoundSysVtbl, self)->SetDesc(self, name, d);
}

static inline void SoundSys_SetMusic(void *self, const char *name) {
	VT(CSoundSysVtbl, self)->SetMusic(self, name);
}

static inline void SoundSys_SetMusicVolume(void *self, float volume) {
	VT(CSoundSysVtbl, self)->SetMusicVolume(self, volume);
}

static inline int SoundSys_SetVoiceParam(void *self, int voice, float v) {
	return VT(CSoundSysVtbl, self)->SetVoiceParam(self, voice, v);
}

static inline int SoundSys_SetVoiceString(void *self, int voice, const char *text) {
	return VT(CSoundSysVtbl, self)->SetVoiceString(self, voice, text);
}

static inline int SoundSys_SetVoiceVolume(void *self, int voice, float volume) {
	return VT(CSoundSysVtbl, self)->SetVoiceVolume(self, voice, volume);
}

static inline void SoundSys_StopAllSounds(void *self) {
	VT(CSoundSysVtbl, self)->StopAllSounds(self);
}

static inline void SoundSys_StopMusic(void *self) {
	VT(CSoundSysVtbl, self)->StopMusic(self);
}

static inline int SoundSys_StopSound(void *self, int handle) {
	return VT(CSoundSysVtbl, self)->StopSound(self, handle);
}

static inline int SoundSys_StopVoice(void *self, int voice) {
	return VT(CSoundSysVtbl, self)->StopVoice(self, voice);
}

static inline void SoundSys_Update(void *self) {
	VT(CSoundSysVtbl, self)->Update(self);
}

static inline byte SoundSys_UpdateSound(void *self, const char *name, const cSoundDesc *d, int handle) {
	return VT(CSoundSysVtbl, self)->UpdateSound(self, name, d, handle);
}

static inline byte SoundSys_VoiceRemain(void *self, int voice, float *remain) {
	return VT(CSoundSysVtbl, self)->VoiceRemain(self, voice, remain);
}

static inline void SoundSys_CloseHook(void *self) {
	VT(CSoundSysVtbl, self)->CloseHook(self);
}

struct CSoundSys {
	const CSoundSysVtbl *vftable;
	cSoundListener listeners[SOUND_MAX_LISTENERS];
	int lastListener;				/* the highest set, -1 none */
};

/* the mixer backend (include/scrap/snd.h; FMOD 3 in the original): the samples by name */
typedef struct CSoundXBox {
	CSoundSys sys;
	HashTable sounds;			/* 457 slots, not case sensitive */
	int volume;				/* added to every volume: SoundMainVolume */
	byte swapPan;				/* SoundSwapChannels */
	byte pad_91[3];
} CSoundXBox;

/* a value of the sample table: the sample, its parameters and its default frequency */
typedef struct cSoundTableValue {
	void *sample;				/* SndSample */
	cSoundDesc desc;
	int freq;
} cSoundTableValue;

typedef struct cSoundStrItem {
	cSoundTableValue value;
	char *key;				/* cString */
	struct cSoundStrItem *next;
} cSoundStrItem;

extern CSoundSys *g_pSound;

/* engine variables of the sound system */
extern cGVar GVar_SoundAutoplayVolume;
#define g_SoundAutoplayVolume (GVar_SoundAutoplayVolume.value.f)	/* dB, the volume below which nothing plays */
extern cGVar GVar_SoundInit;
#define g_SoundInit (GVar_SoundInit.value.i)
extern cGVar GVar_S_PitchScale;
#define g_S_PitchScale (GVar_S_PitchScale.value.f)
extern cGVar GVar_isDolby;
#define g_isDolby (GVar_isDolby.value.i)			/* no pan */

/* volume 0..1 to the engine's volume value */
int Snd_Volume(float volume);
void cSoundDesc_Init(cSoundDesc *desc);
/* the volume and pan of a sound at pos (without doppler) */
void cSoundDesc_SetPos(cSoundDesc *desc, const vec3_t *pos);

/* Sound/Sound.cpp (src/game/Sound/Sound.c) */
void Sound_Init(void);
void Sound_Done(void);
cSound *cSound_ctor(cSound *self, const char *name, uint32_t flags);
cSound *cSound_Init(cSound *self);
/* stops it (also the destructor) */
void cSound_Stop(cSound *self);
/* sets the name: flags 1 or 2 load the file (the name is the sound name of it), else name is a sound name */
void cSound_SetSound(cSound *self, const char *name, uint32_t flags);
const char *cSound_GetName(cSound *self);
/* restarts it; the result of PlaySound */
byte cSound_Play(cSound *self);
/* keeps it playing or starts it again when it got loud enough, stops it when quiet; 1 while it plays */
byte cSound_Update(cSound *self);
void cSound_SetPitch(cSound *self, float semitones);
/* the 3D volume and pan with doppler */
void cSound_SetPosVel(cSound *self, const vec3_t *pos, const vec3_t *vel);

CSoundSys *CSoundSys_ctor(CSoundSys *self);
void CSoundSys_dtor(CSoundSys *self);
void CSoundSys_InitFmod(CSoundSys *self);
void CSoundSys_Close(CSoundSys *self);
void CSoundSys_Nop(CSoundSys *self);
void CSoundSys_CallCloseHook(CSoundSys *self);
void CSoundSys_SetVolume(CSoundSys *self, const char *name, int volume);
void CSoundSys_SetPan(CSoundSys *self, const char *name, int pan);
void CSoundSys_SetFreq(CSoundSys *self, const char *name, float freq);
void CSoundSys_SetLoop(CSoundSys *self, const char *name, byte loop);
void CSoundSys_SetPrio(CSoundSys *self, const char *name, byte prio);
void CSoundSys_ResetListeners(CSoundSys *self);
void CSoundSys_SetListener(CSoundSys *self, int id, const vec3_t *pos, const vec3_t *ang, const vec3_t *vel);
void CSoundSys_Compute3D(CSoundSys *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel, byte doppler);
void CSoundSys_SetMusic(CSoundSys *self, const char *name);
void CSoundSys_StopMusic(CSoundSys *self);
void CSoundSys_SetMusicVolume(CSoundSys *self, float volume);
void CSoundSys_GetSoundName(CSoundSys *self, const char *file, char *out, int size);
int CSoundSys_OpenVoice(CSoundSys *self, const char *name, int voice, int arg);
int CSoundSys_PlayVoice(CSoundSys *self, int voice);
int CSoundSys_StopVoice(CSoundSys *self, int voice);
byte CSoundSys_VoiceRemain(CSoundSys *self, int voice, float *remain);
int CSoundSys_SetVoiceVolume(CSoundSys *self, int voice, float volume);
int CSoundSys_SetVoiceParam(CSoundSys *self, int voice, float v);
int CSoundSys_SetVoiceString(CSoundSys *self, int voice, const char *text);
int CSoundSys_GetVoiceString(CSoundSys *self, int voice, char *buf, int size);
int CSoundSys_VoiceUnused(CSoundSys *self, int voice);
void Sound_Nop(void);

/*
 * Sound/PosSound.cpp (src/game/Sound/PosSound.c): the positional sounds by name (the "property sets" of SSound),
 * the dynamic music of the levels (relax and action music) and the voice of the talks.
 */
typedef struct cSoundPS {
	struct cSoundPS *next;		/* g_SndPSList */
	cSound sound;
	float volume;			/* "Volume" 0..1 */
	float pitch;			/* "Pitch" semitones */
	vec3_t pos;			/* "X" "Y" "Z" */
	byte loop;			/* "Loop", else played again after a random interval */
	byte pad_59[3];
	float timer;			/* until the next play */
	float maxInterval;		/* "MaxInterval" */
	float minInterval;		/* "MinInterval" */
	char *name;			/* cString, the key in (&g_SndPSTable) */
} cSoundPS;

/* the property set of that name or NULL */
cSoundPS *SndPS_Get(const char *name);
/* NULL if the name is taken */
cSoundPS *SndPS_Create(const char *name);
/* stops the looping ones, then switches all on or off; 1 */
byte SndPS_Use(byte use);
void SndPS_StopAll(void);
/* the engine variables back to their defaults, all property sets deleted */
void Sound_ResetVolumes(void);
/* like cActClass_Var: set != 0 writes *s or *d / *i, else reads all; returns 1 string, 2 double, 3 int, 0 */
int cSoundPS_Var(cSoundPS *ps, const char *name, int set, char **s, double *d, int *i);
/* CShow 3: the spheres of the positional sounds */
void SndPS_DrawAll(void);
/* the music, the property sets and the voices of a frame */
void Sound_Update(float dt);
/* the talk of the character of that name ended (StopStreamingVoice) */
void Sound_TalkDone(const char *name);

/*
 * Sound/SndInterface.cpp (src/game/Sound/SndInterface.c): the sounds of an entity, set through its variables
 * "<id>_n" (count) and "<id><nn>_<var>" (two digits, the sound) with the vars name, loop, vol, pitch, time, ini, end,
 * dopp and vol0..4, pitch0..4, time0..4, int0..4 (the digit is not checked).
 */
typedef struct cSndItem {
	cSound sound;
	float volume;			/* "vol", 0 stops it */
	float pitch;			/* "pitch" */
	float time;			/* "time", the play time */
	float vols[5];
	float pitches[5];
	float times[5];
	int ints[5];
	byte playing;
	byte pad_9d[3];
} cSndItem;

typedef struct cEntity cEntity;
typedef struct cSndInterface cSndInterface;

struct cSndInterface {
	int nsnd;
	cSndItem **snds;		/* Mem_Alloc */
	struct cSndInterface *next;	/* g_SndInterfaces */
	struct cSndInterface **link;	/* the next field that points here */
	char id[4];			/* three characters and 0 */
	cEntity *owner;
};

/* Sound/SndInterface.cpp (src/game/Sound/SndInterface.c) */
cSndInterface *cSndInterface_ctor(cSndInterface *self, int n);
void cSndInterface_dtor(cSndInterface *self);
/* the looping sounds stop */
void cSndInterface_StopAll(cSndInterface *self, int unused);
void cSndInterface_Tick(cSndInterface *self, float dt);
/* the three characters of the id ("sdd") */
void cSndInterface_SetId(cSndInterface *self, const char *id);
/* 0 stops it, else it starts when it does not play */
void cSndItem_SetVolume(cSndItem *self, cEntity *e, float volume);
byte cSndItem_Play(cSndItem *self, cEntity *e);

void Sound_UpdatePaused(void);

/* Sound/SoundDX8.cpp (src/game/Sound/SoundDX8.c) */
CSoundXBox *CSoundXBox_New(void);
/* the text of an FMOD error code */
const char *Fmod_ErrorString(int err);

/* the instance of Global/HashTable.h in src/game/Global/HashTables.c */
cSoundStrItem *SoundStrTable_Find(HashTable *self, const char *key, uint32_t slot);
cSoundStrItem *SoundStrTable_Insert(HashTable *self, const char *key, uint32_t slot,
	const cSoundTableValue *v);
byte SoundStrTable_Delete(HashTable *self, const char *key);
void SoundStrTable_Clear(HashTable *self);

/* Sound/Wav.cpp (src/game/Sound/Wav.c): the format of a .wav file */
typedef struct cWavInfo {
	int rate;				/* samples per second */
	int channels;				/* 1 or 2 */
	int bits16;				/* 1: 16 bit, 0: 8 bit */
	int samples;
} cWavInfo;

typedef struct cAbstractFile cAbstractFile;

byte cSndInterface_FloatVar(cSndInterface *self, const char *name, double *v, int set);
byte cSndInterface_IntVar(cSndInterface *self, const char *name, int *v, int set);
byte cSndInterface_StrVar(cSndInterface *self, const char *name, char **v, int set);

#endif
