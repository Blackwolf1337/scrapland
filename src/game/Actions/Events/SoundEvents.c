/*
 * Actions/Events/SoundEvents.cpp of the game: the C functions the events of the actions call to play sounds and
 * voices. See include/scrap/actions.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/logic.h"
#include "scrap/sound.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define EVENT_STR 0x80

/* the engine variables WarnVoiceVolume and MaxPoliceWarnDist (Sound/PosSound.cpp) */
extern cGVar GVar_WarnVoiceVolume;
#define g_WarnVoiceVolume (GVar_WarnVoiceVolume.value.f)
extern cGVar GVar_MaxPoliceWarnDist;
#define g_MaxPoliceWarnDist (GVar_MaxPoliceWarnDist.value.f)

/* the entity name, the value string and the time of the event */
static void Event_Read(cAbstractFile *params, char *name, char *str, float *t) {
	cAbstractFile_ReadString(params, name, EVENT_STR);
	cAbstractFile_ReadString(params, str, EVENT_STR);
	params->vftable->Read(params, t, sizeof(*t));
}

/* the part after the next ':' (cut there), NULL without one */
static char *Event_Next(char *s) {
	char *p = strchr(s, ':');

	if (p) {
		*p++ = 0;
	}
	return p;
}

static void Event_AnmSound2DEvent(cAbstractFile *params) {
	char name[EVENT_STR], snd[EVENT_STR];
	float t;
	cSound s;

	Event_Read(params, name, snd, &t);
	cEntity *ent = World_FindEntity(name);
	if (!ent) {
		Con_Bluef("ERROR!:AnmSound2DEvent('%s','%s',%f):\n\tEntity Not found!\n", name, snd, (double)t);
		return;
	}
	CHECK(ent->vftable->IsType(ent, TYEN_WITH_LIFE));
	char *volu = strchr(snd, ':');
	CHECK(volu);
	*volu++ = 0;
	int volume = Snd_Volume((float)crt_atof(volu));
	char *p = strchr(volu, ':');
	int pan = 0;
	float pitch = 0.0f;
	if (p) {
		*p++ = 0;
		pitch = (float)crt_atof(p);
		p = strchr(p, ':');
		if (p) {
			*p++ = 0;
			pan = (int)((crt_atof(p)) * 10000.0);
		}
	}
	cSoundDesc_Init(&s.desc);
	s.desc.loop = 0;
	s.desc.volume = volume;
	s.desc.pan = pan;
	s.desc.freq = 1.0f;
	cSound_SetPitch(&s, pitch);
	g_pSound->vftable->PlaySound(g_pSound, snd, &s.desc, NULL);
}

static void Event_AnmSoundEvent(cAbstractFile *params) {
	char name[EVENT_STR], snd[EVENT_STR];
	float t;
	cSound s;
	vec3_t pos;

	Event_Read(params, name, snd, &t);
	cEntity *ent = World_FindEntity(name);
	if (!ent) {
		Con_Bluef("ERROR!:AnmSoundEvent('%s','%s',%f):\n\tEntity Not found!\n", name, snd, (double)t);
		return;
	}
	CHECK(ent->vftable->IsType(ent, TYEN_WITH_LIFE));
	char *volu = strchr(snd, ':');
	CHECK(volu);
	*volu++ = 0;
	char *pitch = Event_Next(volu);
	cSoundDesc_Init(&s.desc);
	s.desc.loop = 0;
	cSoundDesc_SetPos(&s.desc, Obj_Vtbl(ent)->GetSndPos(ent, &pos));
	s.desc.volume += Snd_Volume((float)crt_atof(volu));
	if (pitch) {
		cSound_SetPitch(&s, (float)crt_atof(pitch));
	}
	g_pSound->vftable->PlaySound(g_pSound, snd, &s.desc, NULL);
}

static void Event_AnmSoundLoopEvent(cAbstractFile *params) {
	char name[EVENT_STR], snd[EVENT_STR];
	float t;

	Event_Read(params, name, snd, &t);
	cEntity *ent = World_FindEntity(name);
	if (!ent) {
		Con_Bluef("ERROR!:AnmSoundLoopEvent('%s','%s',%f):\n\tEntity Not found!\n", name, snd, (double)t);
		return;
	}
	CHECK(ent->vftable->IsType(ent, TYEN_WITH_LIFE));
	char *volu = strchr(snd, ':');
	CHECK(volu);
	*volu++ = 0;
	char *pitch = Event_Next(volu);
	cSndItem *it = PtrArray_Get(&Obj(ent)->snd, crt_atoi(snd));
	if (it) {
		it->volume = (float)crt_atof(volu);
		/* the original reads the pitch also when there is none */
		it->pitch = (float)crt_atof(pitch);
		if (!it->sound.desc.loop) {
			cSndItem_Play(it, ent);
		}
	}
}

static void Event_StopStreamingVoice(cAbstractFile *params) {
	char name[EVENT_STR];

	cAbstractFile_ReadString(params, name, EVENT_STR);
	Sound_TalkDone(name);
}

static void Event_StreamingVoice(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];
	float t;

	Event_Read(params, name, str, &t);
	char *volu = strchr(str, ':');
	CHECK(volu);
	*volu++ = 0;
	float volume = (float)crt_atof(volu);
	char *p = Event_Next(volu);
	float dist = 0.0f;
	if (p) {
		dist = (float)crt_atof(p);
	}
	/* the text goes where the voice functions of the logic pass an int */
	if (Sound_PlayVoice(name, "AnmStr", str, 0)) {
		g_WarnVoiceVolume = volume;
		g_MaxPoliceWarnDist = dist;
	}
}

static void Event_WarningPolice(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];
	float t;

	Event_Read(params, name, str, &t);
	Sound_PlayVoice(name, str, NULL, 1);
}

static cPy_NativeFunc NativeFunc_AnmSoundLoopEvent;
static cPy_NativeFunc NativeFunc_AnmSoundEvent;
static cPy_NativeFunc NativeFunc_AnmSound2DEvent;
static cPy_NativeFunc NativeFunc_WarningPolice;
static cPy_NativeFunc NativeFunc_StreamingVoice;
static cPy_NativeFunc NativeFunc_StopStreamingVoice;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_NATIVE_FUNC(Init_NativeFunc_AnmSoundLoopEvent, &NativeFunc_AnmSoundLoopEvent, "AnmSoundLoopEvent", Event_AnmSoundLoopEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmSoundEvent, &NativeFunc_AnmSoundEvent, "AnmSoundEvent", Event_AnmSoundEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmSound2DEvent, &NativeFunc_AnmSound2DEvent, "AnmSound2DEvent", Event_AnmSound2DEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_WarningPolice, &NativeFunc_WarningPolice, "WarningPolice", Event_WarningPolice)
INIT_NATIVE_FUNC(Init_NativeFunc_StreamingVoice, &NativeFunc_StreamingVoice, "StreamingVoice", Event_StreamingVoice)
INIT_NATIVE_FUNC(Init_NativeFunc_StopStreamingVoice, &NativeFunc_StopStreamingVoice, "StopStreamingVoice", Event_StopStreamingVoice)
