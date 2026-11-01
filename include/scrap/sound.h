/*
 * The sound system (original source: Sound/). g_pSound is the CSoundSys instance, made as its FMOD 3 backend
 * CSoundXBox (Sound/SoundDX8.cpp, the name is from the Xbox port); scripts reach it through the SSound module.
 * Sounds are loaded by name into a hash table of samples with their default parameters (cSoundDesc); a cSound
 * (Sound/Sound.cpp) is a named sound with its parameters and the FMOD channel it plays on. Volumes are in
 * hundredths of dB (0 full, -100000 off), pans in hundredths of dB too (-10000 left .. 10000 right).
 * Property sets ("PS") are the positional sounds of Sound/PosSound.cpp with the variable interface of the action
 * classes.
 */
#ifndef SCRAP_SOUND_H
#define SCRAP_SOUND_H

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
SD_SIZE(cSoundDesc, 0x1c);
SD_OFFSET(cSoundDesc, prio, 0xd);
SD_OFFSET(cSoundDesc, param3, 0x10);

/* a sound by name with its parameters and the channel it plays on */
typedef struct cSound {
	cSoundDesc desc;		/* 0x00 */
	char name[SOUND_NAME_MAX_LEN];	/* 0x1c */
	int handle;			/* 0x3c FMOD channel, 0 none */
} cSound;
SD_SIZE(cSound, 0x40);

/* a view: position, angles (radians) and velocity, with sin and cos of the negated angles */
typedef struct cSoundListener {
	vec3_t pos;			/* 0x00 */
	vec3_t ang;			/* 0x0c */
	vec3_t vel;			/* 0x18 */
	vec3_t sin;			/* 0x24 */
	vec3_t cos;			/* 0x30 */
} cSoundListener;
SD_SIZE(cSoundListener, 0x3c);

typedef struct CSoundSys CSoundSys;

/* the interface (CSoundSys at rva 0x3c9f3c, slots 0x10..0x40 pure, CSoundXBox at 0x3ca718) */
typedef struct CSoundSysVtbl {
	CSoundSys *(__thiscall *Destroy)(CSoundSys *self, byte flags);			/* 0x00 */
	int(__thiscall *Init)(CSoundSys *self);						/* 0x04 */
	void(__thiscall *Close)(CSoundSys *self);					/* 0x08 */
	void(__thiscall *Update)(CSoundSys *self);					/* 0x0c */
	void(__thiscall *LoadSound2)(CSoundSys *self, const char *name);		/* 0x10 CHECK(0) */
	void(__thiscall *LoadSound)(CSoundSys *self, const char *name);		/* 0x14 */
	void(__thiscall *DeleteSound)(CSoundSys *self, const char *name);		/* 0x18 */
	void(__thiscall *StopAllSounds)(CSoundSys *self);				/* 0x1c */
	void(__thiscall *DeleteAllSounds)(CSoundSys *self);				/* 0x20 */
	byte(__thiscall *GetDesc)(CSoundSys *self, const char *name, cSoundDesc *out);	/* 0x24 */
	byte(__thiscall *SetDesc)(CSoundSys *self, const char *name, const cSoundDesc *d);	/* 0x28 CHECK(0) */
	int(__thiscall *PlaySound)(CSoundSys *self, const char *name, const cSoundDesc *d, int *handle);	/* 0x2c */
	int(__thiscall *PlayName)(CSoundSys *self, const char *name);			/* 0x30 */
	int(__thiscall *Play)(CSoundSys *self, const char *name, int volume, int pan);	/* 0x34 */
	int(__thiscall *PlayAt)(CSoundSys *self, const char *name, const vec3_t *pos, int volume);	/* 0x38 */
	byte(__thiscall *UpdateSound)(CSoundSys *self, const char *name, const cSoundDesc *d, int handle);	/* 0x3c */
	int(__thiscall *StopSound)(CSoundSys *self, int handle);			/* 0x40 */
	void(__thiscall *SetVolume)(CSoundSys *self, const char *name, int volume);	/* 0x44 */
	void(__thiscall *SetPan)(CSoundSys *self, const char *name, int pan);		/* 0x48 */
	void(__thiscall *SetFreq)(CSoundSys *self, const char *name, float freq);	/* 0x4c */
	void(__thiscall *SetLoop)(CSoundSys *self, const char *name, byte loop);	/* 0x50 */
	void(__thiscall *SetPrio)(CSoundSys *self, const char *name, byte prio);	/* 0x54 */
	void(__thiscall *ResetListeners)(CSoundSys *self);				/* 0x58 */
	void(__thiscall *SetListener)(CSoundSys *self, int id, const vec3_t *pos, const vec3_t *ang,
		const vec3_t *vel);							/* 0x5c */
	/* volume, pan and (doppler set) frequency of a sound at pos moving with vel for the nearest listener */
	void(__thiscall *Compute3D)(CSoundSys *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel,
		byte doppler);								/* 0x60 */
	void(__thiscall *SetMusic)(CSoundSys *self, const char *name);			/* 0x64 */
	void(__thiscall *vf1a)(CSoundSys *self);					/* 0x68 */
	void(__thiscall *StopMusic)(CSoundSys *self);					/* 0x6c */
	void(__thiscall *vf1c)(CSoundSys *self);					/* 0x70 */
	void(__thiscall *SetMusicVolume)(CSoundSys *self, float volume);		/* 0x74 */
	/* the sound name of a file name: without directory and extension */
	void(__thiscall *GetSoundName)(CSoundSys *self, const char *file, char *out, int size);	/* 0x78 */
	void(__thiscall *DebugDraw)(CSoundSys *self, int x, int y);			/* 0x7c */
	int(__thiscall *OpenVoice)(CSoundSys *self, const char *name, int voice, int arg);	/* 0x80 */
	int(__thiscall *PlayVoice)(CSoundSys *self, int voice);				/* 0x84 */
	int(__thiscall *StopVoice)(CSoundSys *self, int voice);				/* 0x88 */
	byte(__thiscall *VoiceRemain)(CSoundSys *self, int voice, float *remain);	/* 0x8c */
	int(__thiscall *SetVoiceVolume)(CSoundSys *self, int voice, float volume);	/* 0x90 */
	int(__thiscall *SetVoiceParam)(CSoundSys *self, int voice, float v);		/* 0x94 */
	int(__thiscall *SetVoiceString)(CSoundSys *self, int voice, const char *text);	/* 0x98 */
	int(__thiscall *GetVoiceString)(CSoundSys *self, int voice, char *buf, int size);	/* 0x9c */
	int(__thiscall *VoiceUnused)(CSoundSys *self, int voice);			/* 0xa0 CHECK(0) */
} CSoundSysVtbl;
SD_OFFSET(CSoundSysVtbl, LoadSound, 0x14);
SD_OFFSET(CSoundSysVtbl, PlaySound, 0x2c);
SD_OFFSET(CSoundSysVtbl, StopSound, 0x40);
SD_OFFSET(CSoundSysVtbl, SetListener, 0x5c);
SD_OFFSET(CSoundSysVtbl, SetMusic, 0x64);
SD_OFFSET(CSoundSysVtbl, GetSoundName, 0x78);
SD_OFFSET(CSoundSysVtbl, OpenVoice, 0x80);
SD_OFFSET(CSoundSysVtbl, VoiceUnused, 0xa0);

struct CSoundSys {
	const CSoundSysVtbl *vftable;			/* 0x00 */
	cSoundListener listeners[SOUND_MAX_LISTENERS];	/* 0x04 */
	int lastListener;				/* 0x7c the highest set, -1 none */
};
SD_SIZE(CSoundSys, 0x80);

/* the FMOD backend: the samples by name (SoundStrTable, cSoundTableValue) */
typedef struct CSoundXBox {
	CSoundSys sys;				/* 0x00 */
	HashTable sounds;			/* 0x80 457 slots, not case sensitive */
	int volume;				/* 0x8c added to every volume: SoundMainVolume */
	byte swapPan;				/* 0x90 SoundSwapChannels */
	byte pad_91[3];
} CSoundXBox;
SD_SIZE(CSoundXBox, 0x94);

/* a value of the sample table: the FMOD sample, its parameters and its default frequency */
typedef struct cSoundTableValue {
	void *sample;				/* 0x00 FSOUND_SAMPLE */
	cSoundDesc desc;			/* 0x04 */
	int freq;				/* 0x20 */
} cSoundTableValue;
SD_SIZE(cSoundTableValue, 0x24);

typedef struct cSoundStrItem {
	cSoundTableValue value;			/* 0x00 */
	char *key;				/* 0x24 cString */
	struct cSoundStrItem *next;		/* 0x28 */
} cSoundStrItem;
SD_SIZE(cSoundStrItem, 0x2c);

#define CSoundSys_vftable ((const CSoundSysVtbl *)GAME_VFTABLE(CSoundSys_vftable))
#define CSoundXBox_vftable ((const CSoundSysVtbl *)GAME_VFTABLE(CSoundXBox_vftable))

#define g_pSound GAME_GLOBAL(CSoundSys *, g_pSound)

/* engine variables of the sound system */
#define g_SoundAutoplayVolume GAME_GLOBAL_IN(float, GVar_SoundAutoplayVolume, 16)	/* dB, the volume below which nothing plays */
#define g_SoundDistAttenuation GAME_GLOBAL_IN(float, GVar_SoundDistAttenuation, 16)
#define g_SoundDopplerLevel GAME_GLOBAL_IN(float, GVar_SoundDopplerLevel, 16)
#define g_SoundPanningFactor GAME_GLOBAL_IN(float, GVar_SoundPanningFactor, 16)
#define g_SoundShowSetMode GAME_GLOBAL_IN(int, GVar_SoundShowSetMode, 16)		/* Compute3D and SetListener print their input */
#define g_SoundMainVolume GAME_GLOBAL_IN(float, GVar_SoundMainVolume, 16)
#define g_SoundInit GAME_GLOBAL_IN(int, GVar_SoundInit, 16)
#define g_SoundSwapChannels GAME_GLOBAL_IN(int, GVar_SoundSwapChannels, 16)
#define g_S_PitchScale GAME_GLOBAL_IN(float, GVar_S_PitchScale, 16)
#define g_S_SkipPlay GAME_GLOBAL_IN(int, GVar_S_SkipPlay, 16)
#define g_S_SkipUpdates GAME_GLOBAL_IN(int, GVar_S_SkipUpdates, 16)
#define g_isDolby GAME_GLOBAL_IN(int, GVar_isDolby, 16)			/* no pan */
#define g_SndFreqScale GAME_GLOBAL(float, g_SndFreqScale)		/* S_PitchScale over the timer multiplier */

/* volume 0..1 to the engine's volume value */
int Snd_Volume(float volume);
void __thiscall cSoundDesc_Init(cSoundDesc *desc);
void __thiscall cSoundDesc_Copy(cSoundDesc *desc, const cSoundDesc *src);
void __thiscall cSoundDesc_Set(cSoundDesc *desc, int volume, int pan, float freq, byte loop, byte prio);
/* the volume and pan of a sound at pos (without doppler) */
void __thiscall cSoundDesc_SetPos(cSoundDesc *desc, const vec3_t *pos);

/* Sound/Sound.cpp (src/game/Sound/Sound.c) */
void Sound_Init(void);
void Sound_Done(void);
cSound *__thiscall cSound_ctor(cSound *self, const char *name, uint32_t flags);
cSound *__fastcall cSound_Init(cSound *self);
/* stops it (also the destructor) */
void __fastcall cSound_Stop(cSound *self);
/* sets the name: flags 1 or 2 load the file (the name is the sound name of it), else name is a sound name */
void __thiscall cSound_SetSound(cSound *self, const char *name, uint32_t flags);
const char *__fastcall cSound_GetName(cSound *self);
/* restarts it; the result of PlaySound */
byte __fastcall cSound_Play(cSound *self);
/* keeps it playing or starts it again when it got loud enough, stops it when quiet; 1 while it plays */
byte __fastcall cSound_Update(cSound *self);
void __thiscall cSound_SetPitch(cSound *self, float semitones);
/* the 3D volume and pan with doppler */
void __thiscall cSound_SetPosVel(cSound *self, const vec3_t *pos, const vec3_t *vel);

CSoundSys *__fastcall CSoundSys_ctor(CSoundSys *self);
void __fastcall CSoundSys_dtor(CSoundSys *self);
int __fastcall CSoundSys_InitFmod(CSoundSys *self);
void __fastcall CSoundSys_Close(CSoundSys *self);
void __fastcall CSoundSys_Nop(CSoundSys *self);
void __fastcall CSoundSys_CallVf1c(CSoundSys *self);
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
	char *name;			/* 0x68 cString, the key in g_SndPSTable */
} cSoundPS;
SD_SIZE(cSoundPS, 0x6c);
SD_OFFSET(cSoundPS, loop, 0x58);

#define g_SndPSTable (&GAME_GLOBAL(HashTable, g_SndPSTable))	/* cPtrItem of cSoundPS by name */
#define g_SndPSEnabled GAME_GLOBAL(byte, g_SndPSEnabled)
#define g_SndPSList GAME_GLOBAL(cSoundPS *, g_SndPSList)

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
int __thiscall cSoundPS_Var(cSoundPS *ps, const char *name, int set, char **s, double *d, int *i);
cSoundPS *__fastcall cSoundPS_ctor(cSoundPS *self);
void __fastcall cSoundPS_dtor(cSoundPS *self);
/* CShow 3: the spheres of the positional sounds */
void SndPS_DrawAll(void);
void __thiscall cSoundPS_Update(cSoundPS *self, float dt);
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
SD_SIZE(cSndItem, 0xa0);
SD_OFFSET(cSndItem, ints, 0x88);

typedef struct cEntity cEntity;
typedef struct cSndInterface {
	const void *vftable;		/* 0x00 */
	int nsnd;			/* 0x04 */
	cSndItem **snds;		/* 0x08 Mem_Alloc */
	struct cSndInterface *next;	/* 0x0c g_SndInterfaces */
	struct cSndInterface **link;	/* 0x10 the next field that points here */
	char id[4];			/* 0x14 three characters and 0 */
	cEntity *owner;			/* 0x18 */
} cSndInterface;
SD_SIZE(cSndInterface, 0x1c);

#define cSndInterface_vftable ((const void *)GAME_VFTABLE(cSndInterface_vftable))
#define g_SndInterfaces GAME_GLOBAL(cSndInterface *, g_SndInterfaces)

/* Sound/SndInterface.cpp (src/game/Sound/SndInterface.c) */
cSndInterface *__thiscall cSndInterface_ctor(cSndInterface *self, int n);
void __fastcall cSndInterface_dtor(cSndInterface *self);
/* the looping sounds stop */
void __thiscall cSndInterface_StopAll(cSndInterface *self, int unused);
void __thiscall cSndInterface_Tick(cSndInterface *self, float dt);
/* the three characters of the id ("sdd") */
void __thiscall cSndInterface_SetId(cSndInterface *self, const char *id);
/* 0 stops it, else it starts when it does not play */
void __thiscall cSndItem_SetVolume(cSndItem *self, cEntity *e, float volume);
byte __thiscall cSndItem_Play(cSndItem *self, cEntity *e);

/* the variable methods of the sounds (cSndInterface::vftable from slot 1) */
typedef struct cSndInterfaceVtbl {
	void *vf00;
	byte(__thiscall *VarI)(cSndInterface *self, const char *name, int *i, int set);
	byte(__thiscall *VarF)(cSndInterface *self, const char *name, double *d, int set);
	byte(__thiscall *VarS)(cSndInterface *self, const char *name, char **s, int set);
	byte(__thiscall *VarV)(cSndInterface *self, const char *name, double *x, double *y, double *z, int set);
} cSndInterfaceVtbl;

static inline const cSndInterfaceVtbl *Snd_Vtbl(cSndInterface *snd) {
	return (const cSndInterfaceVtbl *)snd->vftable;
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
cSoundStrItem *__thiscall SoundStrTable_Find(HashTable *self, const char *key, uint32_t slot);
cSoundStrItem *__thiscall SoundStrTable_Insert(HashTable *self, const char *key, uint32_t slot,
	const cSoundTableValue *v);
byte __thiscall SoundStrTable_Delete(HashTable *self, const char *key);
void __fastcall SoundStrTable_Clear(HashTable *self);

/* Sound/Wav.cpp (src/game/Sound/Wav.c): the format of a .wav file */
typedef struct cWavInfo {
	int rate;				/* 0x00 samples per second */
	int channels;				/* 0x04 1 or 2 */
	int bits16;				/* 0x08 1: 16 bit, 0: 8 bit */
	int samples;				/* 0x0c */
} cWavInfo;
SD_SIZE(cWavInfo, 0x10);

typedef struct cAbstractFile cAbstractFile;

/* reads the RIFF header up to the data chunk; 0, -1 with errno set (0x32 not a supported wav) */
int Wav_ReadInfo(cAbstractFile *file, cWavInfo *info);
/* reads a dword, which must be id unless id is 0; 0 or -1 */
int Wav_ReadChunk(cAbstractFile *file, uint32_t *out, uint32_t id);

#endif
