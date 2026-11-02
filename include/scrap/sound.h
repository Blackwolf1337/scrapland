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

/* the parameters of a sound (0x1c bytes) */
typedef struct cSoundDesc {
	int volume;			/* 0x00 */
	int pan;			/* 0x04 */
	float freq;			/* 0x08 frequency factor */
	byte loop;			/* 0x0c */
	byte prio;			/* 0x0d 0x80 */
	byte pad_0e[2];
	float param3;			/* 0x10 doppler factor (PlaySound p3, default 1) */
	float param1;			/* 0x14 distance where the attenuation starts, 0: the global rolloff (p1) */
	float param2;			/* 0x18 distance of silence, 0: no limit (p2) */
} cSoundDesc;

/* a sound by name with its parameters and the channel it plays on */
typedef struct cSound {
	cSoundDesc desc;		/* 0x00 */
	char name[SOUND_NAME_MAX_LEN];	/* 0x1c */
	int handle;			/* 0x3c mixer channel, 0 none */
} cSound;

/* a view: position, angles (radians) and velocity, with sin and cos of the negated angles */
typedef struct cSoundListener {
	vec3_t pos;			/* 0x00 */
	vec3_t ang;			/* 0x0c */
	vec3_t vel;			/* 0x18 */
	vec3_t sin;			/* 0x24 */
	vec3_t cos;			/* 0x30 */
} cSoundListener;

typedef struct CSoundSys CSoundSys;

/* the interface (CSoundSys at rva 0x3c9f3c, slots 0x10..0x40 pure, CSoundXBox at 0x3ca718) */
typedef struct CSoundSysVtbl {
	CSoundSys *(*Destroy)(CSoundSys *self, byte flags);			/* 0x00 */
	void(*Init)(CSoundSys *self);						/* 0x04 */
	void(*Close)(CSoundSys *self);					/* 0x08 */
	void(*Update)(CSoundSys *self);					/* 0x0c */
	void(*LoadSound2)(CSoundSys *self, const char *name);		/* 0x10 CHECK(0) */
	void(*LoadSound)(CSoundSys *self, const char *name);		/* 0x14 */
	void(*DeleteSound)(CSoundSys *self, const char *name);		/* 0x18 */
	void(*StopAllSounds)(CSoundSys *self);				/* 0x1c */
	void(*DeleteAllSounds)(CSoundSys *self);				/* 0x20 */
	byte(*GetDesc)(CSoundSys *self, const char *name, cSoundDesc *out);	/* 0x24 */
	byte(*SetDesc)(CSoundSys *self, const char *name, const cSoundDesc *d);	/* 0x28 CHECK(0) */
	int(*PlaySound)(CSoundSys *self, const char *name, const cSoundDesc *d, int *handle);	/* 0x2c */
	int(*PlayName)(CSoundSys *self, const char *name);			/* 0x30 */
	int(*Play)(CSoundSys *self, const char *name, int volume, int pan);	/* 0x34 */
	int(*PlayAt)(CSoundSys *self, const char *name, const vec3_t *pos, int volume);	/* 0x38 */
	byte(*UpdateSound)(CSoundSys *self, const char *name, const cSoundDesc *d, int handle);	/* 0x3c */
	int(*StopSound)(CSoundSys *self, int handle);			/* 0x40 */
	void(*SetVolume)(CSoundSys *self, const char *name, int volume);	/* 0x44 */
	void(*SetPan)(CSoundSys *self, const char *name, int pan);		/* 0x48 */
	void(*SetFreq)(CSoundSys *self, const char *name, float freq);	/* 0x4c */
	void(*SetLoop)(CSoundSys *self, const char *name, byte loop);	/* 0x50 */
	void(*SetPrio)(CSoundSys *self, const char *name, byte prio);	/* 0x54 */
	void(*ResetListeners)(CSoundSys *self);				/* 0x58 */
	void(*SetListener)(CSoundSys *self, int id, const vec3_t *pos, const vec3_t *ang,
		const vec3_t *vel);							/* 0x5c */
	/* volume, pan and (doppler set) frequency of a sound at pos moving with vel for the nearest listener */
	void(*Compute3D)(CSoundSys *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel,
		byte doppler);								/* 0x60 */
	void(*SetMusic)(CSoundSys *self, const char *name);			/* 0x64 */
	void(*vf1a)(CSoundSys *self);					/* 0x68 */
	void(*StopMusic)(CSoundSys *self);					/* 0x6c */
	void(*vf1c)(CSoundSys *self);					/* 0x70 */
	void(*SetMusicVolume)(CSoundSys *self, float volume);		/* 0x74 */
	/* the sound name of a file name: without directory and extension */
	void(*GetSoundName)(CSoundSys *self, const char *file, char *out, int size);	/* 0x78 */
	void(*DebugDraw)(CSoundSys *self, int x, int y);			/* 0x7c */
	int(*OpenVoice)(CSoundSys *self, const char *name, int voice, int arg);	/* 0x80 */
	int(*PlayVoice)(CSoundSys *self, int voice);				/* 0x84 */
	int(*StopVoice)(CSoundSys *self, int voice);				/* 0x88 */
	byte(*VoiceRemain)(CSoundSys *self, int voice, float *remain);	/* 0x8c */
	int(*SetVoiceVolume)(CSoundSys *self, int voice, float volume);	/* 0x90 */
	int(*SetVoiceParam)(CSoundSys *self, int voice, float v);		/* 0x94 */
	int(*SetVoiceString)(CSoundSys *self, int voice, const char *text);	/* 0x98 */
	int(*GetVoiceString)(CSoundSys *self, int voice, char *buf, int size);	/* 0x9c */
	int(*VoiceUnused)(CSoundSys *self, int voice);			/* 0xa0 CHECK(0) */
} CSoundSysVtbl;

struct CSoundSys {
	const CSoundSysVtbl *vftable;			/* 0x00 */
	cSoundListener listeners[SOUND_MAX_LISTENERS];	/* 0x04 */
	int lastListener;				/* 0x7c the highest set, -1 none */
};

/* the mixer backend (include/scrap/snd.h; FMOD 3 in the original): the samples by name */
typedef struct CSoundXBox {
	CSoundSys sys;				/* 0x00 */
	HashTable sounds;			/* 0x80 457 slots, not case sensitive */
	int volume;				/* 0x8c added to every volume: SoundMainVolume */
	byte swapPan;				/* 0x90 SoundSwapChannels */
	byte pad_91[3];
} CSoundXBox;

/* a value of the sample table: the sample, its parameters and its default frequency */
typedef struct cSoundTableValue {
	void *sample;				/* 0x00 SndSample */
	cSoundDesc desc;			/* 0x04 */
	int freq;				/* 0x20 */
} cSoundTableValue;

typedef struct cSoundStrItem {
	cSoundTableValue value;			/* 0x00 */
	char *key;				/* 0x24 cString */
	struct cSoundStrItem *next;		/* 0x28 */
} cSoundStrItem;

extern const CSoundSysVtbl CSoundSys_vftable;
#define CSOUNDSYS_VFTABLE (&CSoundSys_vftable)
extern const CSoundSysVtbl CSoundXBox_vftable;
#define CSOUNDXBOX_VFTABLE (&CSoundXBox_vftable)

extern CSoundSys *g_pSound;

/* engine variables of the sound system */
extern cGVar GVar_SoundAutoplayVolume;
#define g_SoundAutoplayVolume (GVar_SoundAutoplayVolume.value.f)	/* dB, the volume below which nothing plays */
extern cGVar GVar_SoundDistAttenuation;
#define g_SoundDistAttenuation (GVar_SoundDistAttenuation.value.f)
extern cGVar GVar_SoundDopplerLevel;
#define g_SoundDopplerLevel (GVar_SoundDopplerLevel.value.f)
extern cGVar GVar_SoundPanningFactor;
#define g_SoundPanningFactor (GVar_SoundPanningFactor.value.f)
extern cGVar GVar_SoundShowSetMode;
#define g_SoundShowSetMode (GVar_SoundShowSetMode.value.i)		/* Compute3D and SetListener print their input */
extern cGVar GVar_SoundMainVolume;
#define g_SoundMainVolume (GVar_SoundMainVolume.value.f)
extern cGVar GVar_SoundInit;
#define g_SoundInit (GVar_SoundInit.value.i)
extern cGVar GVar_SoundSwapChannels;
#define g_SoundSwapChannels (GVar_SoundSwapChannels.value.i)
extern cGVar GVar_S_PitchScale;
#define g_S_PitchScale (GVar_S_PitchScale.value.f)
extern cGVar GVar_S_SkipPlay;
#define g_S_SkipPlay (GVar_S_SkipPlay.value.i)
extern cGVar GVar_S_SkipUpdates;
#define g_S_SkipUpdates (GVar_S_SkipUpdates.value.i)
extern cGVar GVar_isDolby;
#define g_isDolby (GVar_isDolby.value.i)			/* no pan */
extern float g_SndFreqScale;		/* S_PitchScale over the timer multiplier */

/* volume 0..1 to the engine's volume value */
int Snd_Volume(float volume);
void cSoundDesc_Init(cSoundDesc *desc);
void cSoundDesc_Copy(cSoundDesc *desc, const cSoundDesc *src);
void cSoundDesc_Set(cSoundDesc *desc, int volume, int pan, float freq, byte loop, byte prio);
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
void CSoundSys_CallVf1c(CSoundSys *self);
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
	struct cSoundPS *next;		/* 0x00 g_SndPSList */
	cSound sound;			/* 0x04 */
	float volume;			/* 0x44 "Volume" 0..1 */
	float pitch;			/* 0x48 "Pitch" semitones */
	vec3_t pos;			/* 0x4c "X" "Y" "Z" */
	byte loop;			/* 0x58 "Loop", else played again after a random interval */
	byte pad_59[3];
	float timer;			/* 0x5c until the next play */
	float maxInterval;		/* 0x60 "MaxInterval" */
	float minInterval;		/* 0x64 "MinInterval" */
	char *name;			/* 0x68 cString, the key in (&g_SndPSTable) */
} cSoundPS;

extern HashTable g_SndPSTable;
extern byte g_SndPSEnabled;
extern cSoundPS *g_SndPSList;

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
cSoundPS *cSoundPS_ctor(cSoundPS *self);
void cSoundPS_dtor(cSoundPS *self);
/* CShow 3: the spheres of the positional sounds */
void SndPS_DrawAll(void);
void cSoundPS_Update(cSoundPS *self, float dt);
/* the music, the property sets and the voices of a frame */
void Sound_Update(float dt);
void Sound_UpdateMusic(float dt);
void Sound_StopTalk(void);
/* the talk of the character of that name ended (StopStreamingVoice) */
void Sound_TalkDone(const char *name);

/*
 * Sound/SndInterface.cpp (src/game/Sound/SndInterface.c): the sounds of an entity, set through its variables
 * "<id>_n" (count) and "<id><nn>_<var>" (two digits, the sound) with the vars name, loop, vol, pitch, time, ini, end,
 * dopp and vol0..4, pitch0..4, time0..4, int0..4 (the digit is not checked).
 */
typedef struct cSndItem {
	cSound sound;			/* 0x00 */
	float volume;			/* 0x40 "vol", 0 stops it */
	float pitch;			/* 0x44 "pitch" */
	float time;			/* 0x48 "time", the play time */
	float vols[5];			/* 0x4c */
	float pitches[5];		/* 0x60 */
	float times[5];			/* 0x74 */
	int ints[5];			/* 0x88 */
	byte playing;			/* 0x9c */
	byte pad_9d[3];
} cSndItem;

typedef struct cEntity cEntity;
typedef struct cSndInterface cSndInterface;

/* the deleting destructor and the variables of the sounds */
typedef struct cSndInterfaceVtbl {
	cSndInterface *(*Delete)(cSndInterface *self, byte flags);					/* 0x00 */
	byte(*VarI)(cSndInterface *self, const char *name, int *i, int set);				/* 0x04 */
	byte(*VarF)(cSndInterface *self, const char *name, double *d, int set);			/* 0x08 */
	byte(*VarS)(cSndInterface *self, const char *name, char **s, int set);			/* 0x0c */
	byte(*VarV)(cSndInterface *self, const char *name, double *x, double *y, double *z, int set);	/* 0x10 */
} cSndInterfaceVtbl;

struct cSndInterface {
	const cSndInterfaceVtbl *vftable;	/* 0x00 */
	int nsnd;			/* 0x04 */
	cSndItem **snds;		/* 0x08 Mem_Alloc */
	struct cSndInterface *next;	/* 0x0c g_SndInterfaces */
	struct cSndInterface **link;	/* 0x10 the next field that points here */
	char id[4];			/* 0x14 three characters and 0 */
	cEntity *owner;			/* 0x18 */
};

extern const cSndInterfaceVtbl cSndInterface_vftable;
#define CSNDINTERFACE_VFTABLE (&cSndInterface_vftable)
extern cSndInterface *g_SndInterfaces;

/* Sound/SndInterface.cpp (src/game/Sound/SndInterface.c) */
cSndInterface *cSndInterface_ctor(cSndInterface *self, int n);
void cSndInterface_dtor(cSndInterface *self);
/* the looping sounds stop */
void cSndInterface_StopAll(cSndInterface *self, int unused);
void cSndInterface_Tick(cSndInterface *self, float dt);
/* the three characters of the id ("sdd") */
void cSndInterface_SetId(cSndInterface *self, const char *id);
byte cSndInterface_VarVFalse(cSndInterface *self, const char *name, double *x, double *y, double *z, int set);
/* 0 stops it, else it starts when it does not play */
void cSndItem_SetVolume(cSndItem *self, cEntity *e, float volume);
byte cSndItem_Play(cSndItem *self, cEntity *e);

/* the variable methods of the sounds (cSndInterface::vftable from slot 1) */
static inline const cSndInterfaceVtbl *Snd_Vtbl(cSndInterface *snd) {
	return snd->vftable;
}

void Sound_UpdatePaused(void);

/* Sound/SoundDX8.cpp (src/game/Sound/SoundDX8.c) */
CSoundXBox *CSoundXBox_New(void);
/* the text of an FMOD error code */
const char *Fmod_ErrorString(int err);
/* volume, pan and frequency of a playing channel; 0 when it does not play */
byte Fmod_SetChannel(int channel, const cSoundDesc *d);
/* plays a sample with the parameters, the channel or 0 */
int Fmod_Play(void *sample, const cSoundDesc *d, const char *name);

/* the instance of Global/HashTable.h in src/game/Global/HashTables.c */
cSoundStrItem *SoundStrTable_Find(HashTable *self, const char *key, uint32_t slot);
cSoundStrItem *SoundStrTable_Insert(HashTable *self, const char *key, uint32_t slot,
	const cSoundTableValue *v);
byte SoundStrTable_Delete(HashTable *self, const char *key);
void SoundStrTable_Clear(HashTable *self);

/* Sound/Wav.cpp (src/game/Sound/Wav.c): the format of a .wav file */
typedef struct cWavInfo {
	int rate;				/* 0x00 samples per second */
	int channels;				/* 0x04 1 or 2 */
	int bits16;				/* 0x08 1: 16 bit, 0: 8 bit */
	int samples;				/* 0x0c */
} cWavInfo;

typedef struct cAbstractFile cAbstractFile;

/* reads the RIFF header up to the data chunk; 0, -1 with errno set (0x32 not a supported wav) */
int Wav_ReadInfo(cAbstractFile *file, cWavInfo *info);
/* reads a dword, which must be id unless id is 0; 0 or -1 */
int Wav_ReadChunk(cAbstractFile *file, uint32_t *out, uint32_t id);

#endif
