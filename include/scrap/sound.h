/*
 * The sound system (original source: Sound/). g_pSound is the CSoundSys instance behind the interface class
 * cSndInterface; scripts reach it through the SSound module. Property sets ("PS") are named groups of sound
 * settings with the same variable interface as the action classes.
 */
#ifndef SCRAP_SOUND_H
#define SCRAP_SOUND_H

#include "scrap/game.h"
#include "scrap/mathlib.h"

typedef struct cSndInterface cSndInterface;

/* sound start descriptor (0x28 bytes on the stack of SSound.PlaySound) */
typedef struct cSoundDesc {
	int volume;			/* 0x00 the result of Snd_Volume is added */
	byte pad_04[0x10 - 0x04];
	float param3;			/* 0x10 third optional argument of PlaySound (default 1) */
	float param1;			/* 0x14 first optional argument (default 0) */
	float param2;			/* 0x18 second optional argument (default 0) */
	vec3_t pos;			/* 0x1c */
} cSoundDesc;
SD_SIZE(cSoundDesc, 0x28);
SD_OFFSET(cSoundDesc, param3, 0x10);
SD_OFFSET(cSoundDesc, pos, 0x1c);

typedef struct cSndInterfaceVtbl {
	void *vf00_04[5];
	void(__thiscall *LoadSound)(cSndInterface *self, const char *name);			/* 0x14 */
	void *vf06;
	void(__thiscall *StopAllSounds)(cSndInterface *self);					/* 0x1c */
	void(__thiscall *DeleteAllSounds)(cSndInterface *self);				/* 0x20 */
	void *vf09_0a[2];
	void(__thiscall *PlaySound)(cSndInterface *self, const char *name, cSoundDesc *desc, int arg);	/* 0x2c */
	void *vf0c;
	void(__thiscall *Play)(cSndInterface *self, const char *name, int volume, int pan);	/* 0x34 */
	void *vf0e_18[11];
	void(__thiscall *SetMusic)(cSndInterface *self, const char *name);			/* 0x64 */
	void *vf1a_1c[3];
	void(__thiscall *SetMusicVolume)(cSndInterface *self, float volume);			/* 0x74 */
	void *vf1e_1f[2];
	void(__thiscall *OpenVoice)(cSndInterface *self, const char *name, int voice, int arg);	/* 0x80 */
	void(__thiscall *PlayVoice)(cSndInterface *self, int voice);				/* 0x84 */
	void(__thiscall *StopVoice)(cSndInterface *self, int voice);				/* 0x88 */
	byte(__thiscall *VoiceRemain)(cSndInterface *self, int voice, float *remain);		/* 0x8c */
	void(__thiscall *SetVoiceVolume)(cSndInterface *self, int voice, float volume);	/* 0x90 */
	void *vf25;
	void(__thiscall *SetVoiceString)(cSndInterface *self, int voice, const char *text);	/* 0x98 */
} cSndInterfaceVtbl;
SD_OFFSET(cSndInterfaceVtbl, LoadSound, 0x14);
SD_OFFSET(cSndInterfaceVtbl, PlaySound, 0x2c);
SD_OFFSET(cSndInterfaceVtbl, Play, 0x34);
SD_OFFSET(cSndInterfaceVtbl, SetMusic, 0x64);
SD_OFFSET(cSndInterfaceVtbl, SetMusicVolume, 0x74);
SD_OFFSET(cSndInterfaceVtbl, OpenVoice, 0x80);
SD_OFFSET(cSndInterfaceVtbl, SetVoiceString, 0x98);

struct cSndInterface {
	const cSndInterfaceVtbl *vftable;
};

#define g_pSound GAME_DATA(cSndInterface *, 0x466bb4)

typedef struct cSoundPS cSoundPS;

/* volume 0..1 to the engine's volume value */
int Snd_Volume(float volume);
void __thiscall cSoundDesc_Init(cSoundDesc *desc);
void __thiscall cSoundDesc_SetPos(cSoundDesc *desc, const vec3_t *pos);
/* the property set of that name or NULL */
cSoundPS *SndPS_Get(const char *name);
cSoundPS *SndPS_Create(const char *name);
byte SndPS_Use(byte use);
/* like cActClass_Var: set != 0 writes *s or *d / *i, else reads all; returns 1 string, 2 double, 3 int, 0 */
int __thiscall cSoundPS_Var(cSoundPS *ps, const char *name, int set, char **s, double *d, int *i);

#endif
