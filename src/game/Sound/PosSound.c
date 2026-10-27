/*
 * Sound/PosSound.cpp of the game: the positional sounds by name (cSoundPS, the property sets of SSound), the
 * dynamic music of the levels (relax music, action music while enemies are on the radar or the alarm is on, with a
 * fade between them) and the timing of the talk voice. See include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/cstring.h"
#include "scrap/hashtable.h"
#include "scrap/scrapfont.h"
#include "scrap/sound.h"
#include "scrap/world.h"

/* engine variables of the music and the voices (value, default) */
#define g_isActionMusic GAME_DATA(int, 0x466ab8)
#define g_isActionMusicDef GAME_DATA(int, 0x466abc)
#define g_WarnVoiceVolume GAME_DATA(float, 0x466b30)
#define g_WarnVoiceVolumeDef GAME_DATA(float, 0x466b34)
#define g_isActionIndoor GAME_DATA(int, 0x466b18)
#define g_isActionIndoorDef GAME_DATA(int, 0x466b1c)
#define g_MaxPoliceWarnDist GAME_DATA(float, 0x466a70)
#define g_MaxPoliceWarnDistDef GAME_DATA(float, 0x466a74)
#define g_useDynamicMusic GAME_DATA(int, 0x466ae8)
#define g_useDynamicMusicDef GAME_DATA(int, 0x466aec)
#define g_ForceNoActionMusic GAME_DATA(int, 0x466b48)
#define g_ForceNoActionMusicDef GAME_DATA(int, 0x466b4c)
#define g_ActionMusicTimeFade GAME_DATA(float, 0x466b60)
#define g_OutActionMusicVolume GAME_DATA(float, 0x466a40)
#define g_OutRelaxMusicVolume GAME_DATA(float, 0x466ad0)
#define g_iOutActionMusic GAME_DATA(int, 0x466b00)
#define g_iOutRelaxMusic GAME_DATA(int, 0x466aa0)
#define g_OutActionMusic GAME_DATA(const char *, 0x466a88)	/* format with the number */
#define g_OutRelaxMusic GAME_DATA(const char *, 0x466b90)
#define g_MusicTimeChange GAME_DATA(float, 0x466a28)
#define g_NumEnemiesInRadar GAME_DATA(int, 0x45fe30)
#define g_S_VoiceoverVolume GAME_DATA(float, 0x4aa1d0)
#define g_S_VoiceoverVolumeDef GAME_DATA(float, 0x4aa1d4)

/* the state of the music and of the talk voice (voice 2) */
#define g_MusicFade GAME_DATA(float, 0x430228)		/* the fade to the other music */
#define g_MusicWait GAME_DATA(float, 0x466b98)		/* no new change before it runs out */
#define g_MusicAction GAME_DATA(int, 0x466b9c)		/* the action music plays */
#define g_TalkDelay GAME_DATA(float, 0x466ba8)		/* until the talk voice starts */
#define g_TalkStop GAME_DATA(byte, 0x466bac)		/* stop instead of starting it */

#define TALK_VOICE 2

/* the voice volume by the distance to the speaker, of voice 0..2 (rva 0x1a0320) */
void Sound_UpdateVoice(int voice);
/* the debug sphere of DebugUtils.cpp */
void DebugUtils_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH);

cSoundPS *__fastcall cSoundPS_ctor(cSoundPS *self) {
	cSound_Init(&self->sound);
	cString_ctor(&self->name);
	self->volume = 1.0f;
	self->pitch = 0.0f;
	self->pos.x = 0.0f;
	self->pos.y = 0.0f;
	self->pos.z = 0.0f;
	self->next = NULL;
	self->loop = 1;
	int r = crt_rand() & 0x7fff;
	self->maxInterval = 3.0f;
	self->minInterval = 2.0f;
	self->next = g_SndPSList;
	g_SndPSList = self;
	self->timer = (float)((double)r * 0x1.0002000400080p-15 * 3.0 + 0.0);
	return self;
}

void __fastcall cSoundPS_dtor(cSoundPS *self) {
	cString_dtor(&self->name);
	cSound_Stop(&self->sound);
}

void __thiscall cSoundPS_Update(cSoundPS *self, float dt) {
	vec3_t vel;

	if (!self->loop) {
		float t = self->timer - dt;
		self->timer = t;
		if (t <= 0.0f) {
			float mn = self->minInterval;
			float mx = self->maxInterval;
			int r = crt_rand() & 0x7fff;
			self->timer = (float)((double)r * 0x1.0002000400080p-15 * (double)(mx - mn) + (double)mn);
			cSound_Play(&self->sound);
		}
	}
	self->sound.desc.loop = self->loop;
	vel.x = vel.y = vel.z = 0.0f;
	cSound_SetPosVel(&self->sound, &self->pos, &vel);
	self->sound.desc.volume += Snd_Volume(self->volume);
	cSound_SetPitch(&self->sound, self->pitch);
	cSound_Update(&self->sound);
}

void __thiscall cSoundPS_SetSound(cSoundPS *self, const char *name) {
	cSound_SetSound(&self->sound, name, 4);
}

int __thiscall cSoundPS_Var(cSoundPS *ps, const char *name, int set, char **s, double *d, int *i) {
	static const struct {
		const char *name;
		uint32_t off;
	} floats[] = {
		{ "MaxInterval", 0x60 }, { "MinInterval", 0x64 }, { "AttenEnd", 0x1c }, { "AttenIni", 0x18 },
		{ "Doppler", 0x14 }, { "Volume", 0x44 }, { "Pitch", 0x48 }, { "X", 0x4c }, { "Y", 0x50 }, { "Z", 0x54 },
	};

	if (s && !crt_stricmp(name, "Sound")) {
		if ((byte)set) {
			cSound_SetSound(&ps->sound, *s, 4);
		} else {
			*s = (char *)cSound_GetName(&ps->sound);
		}
		return 1;
	}
	if (i && !crt_stricmp(name, "Loop")) {
		if ((byte)set) {
			ps->loop = *i != 0;
		} else {
			*i = ps->loop != 0;
		}
		return 3;
	}
	if (!d) {
		return 0;
	}
	for (size_t k = 0; k < sizeof(floats) / sizeof(floats[0]); k++) {
		if (!crt_stricmp(name, floats[k].name)) {
			float *f = (float *)((byte *)ps + floats[k].off);
			if ((byte)set) {
				*f = (float)*d;
			} else {
				*d = (double)*f;
			}
			return 2;
		}
	}
	return 0;
}

/* the property sets */

cSoundPS *SndPS_Get(const char *name) {
	return PtrTable_Value(g_SndPSTable, name);
}

cSoundPS *SndPS_Create(const char *name) {
	if (PtrTable_Find(g_SndPSTable, name, HashTable_Slot(g_SndPSTable, name))) {
		return NULL;
	}
	cSoundPS *ps = operator_new(sizeof(cSoundPS));
	ps = ps ? cSoundPS_ctor(ps) : NULL;
	cString_Set(&ps->name, name);
	PtrTable_Add(g_SndPSTable, ps->name, (void *const *)&ps, 1);
	return ps;
}

void SndPS_StopAll(void) {
	for (cSoundPS *ps = g_SndPSList; ps; ps = ps->next) {
		if (ps->loop) {
			cSound_Stop(&ps->sound);
		}
	}
}

byte SndPS_Use(byte use) {
	SndPS_StopAll();
	g_SndPSEnabled = use;
	return 1;
}

void Sound_ResetVolumes(void) {
	HashTable *t = g_SndPSTable;

	g_isActionMusic = g_isActionMusicDef;
	g_WarnVoiceVolume = g_WarnVoiceVolumeDef;
	g_isActionIndoor = g_isActionIndoorDef;
	g_MaxPoliceWarnDist = g_MaxPoliceWarnDistDef;
	g_useDynamicMusic = g_useDynamicMusicDef;
	g_ForceNoActionMusic = g_ForceNoActionMusicDef;
	g_MusicFade = g_ActionMusicTimeFade;
	g_MusicWait = 0.0f;
	g_MusicAction = 0;
	g_SndPSList = NULL;
	g_SndPSEnabled = 1;
	for (;;) {
		int k = 0;
		if ((int)t->numSlots < 1) {
			return;
		}
		while (!t->slots[k]) {
			if (++k >= (int)t->numSlots) {
				return;
			}
		}
		cPtrItem *it = (cPtrItem *)t->slots[k];
		cSoundPS *ps = it->value;
		CHECK(PtrTable_Delete(t, it->key));
		if (ps) {
			cSoundPS_dtor(ps);
			operator_delete_sized(ps, sizeof(cSoundPS));
		}
	}
}

/* the debug view: the attenuation spheres in a color by the doppler factor and the names */
void __fastcall cSoundPS_Draw(cSoundPS *self) {
	int u = (int)(self->sound.desc.param3 * 255.0f);
	DebugUtils_DrawSphere(&self->pos, self->sound.desc.param1 + 20.0f, (255 - u) << 8 | u << 16 | u, 10, 0);
	if (self->sound.desc.param2 > 3.0f) {
		DebugUtils_DrawSphere(&self->pos, self->sound.desc.param2 + 20.0f, ((255 - u) / 2 | (u / 2) << 8) << 8 | u / 2,
			10, 0);
	}
	g_FontHoratioSmall->align = 4;
	CFont_Printf3D(g_FontHoratioSmall, &self->pos, 1, "%s (%s)", self->name, cSound_GetName(&self->sound));
}

void SndPS_DrawAll(void) {
	for (cSoundPS *ps = g_SndPSList; ps; ps = ps->next) {
		cSoundPS_Draw(ps);
	}
}

/* the music */

void Sound_UpdateMusic(float dt) {
	char name[256];
	int want, cur;

	if ((g_pApp->flags >> 2) & 1) {
		return;
	}
	if (g_ForceNoActionMusic) {
		want = 0;
		cur = g_MusicAction;
		if (cur) {
			cur = 0;
			g_MusicWait = 0.0f;
			g_MusicFade = dt;
			g_MusicAction = cur;
		}
	} else {
		if (g_NumEnemiesInRadar > 0) {
			want = 1;
		} else {
			want = g_isActionMusic;
			if (g_pWorld->alarmOn) {
				want = 1;
			}
		}
		cur = g_MusicAction;
	}
	float fade = g_MusicFade;
	if (fade > 0.0f) {
		fade -= dt;
		g_MusicFade = fade;
		if (!(fade <= 0.0f)) {
			float f = fade / g_ActionMusicTimeFade;
			int changed = 0;
			if (f > 1.0f) {
				f = 1.0f;
				if (want != cur) {
					cur = want;
					g_MusicFade = 0.0f;
					g_MusicAction = cur;
					changed = 1;
				}
			}
			float vol = (cur != 0) == changed ? g_OutActionMusicVolume : g_OutRelaxMusicVolume;
			g_pSound->vftable->SetMusicVolume(g_pSound, vol * f);
			return;
		}
		if (want != cur) {
			g_pSound->vftable->StopMusic(g_pSound);
			g_MusicWait = 0.0f;
			return;
		}
		const char *fmt;
		int n;
		if (cur) {
			g_pSound->vftable->StopMusic(g_pSound);
			g_pSound->vftable->SetMusicVolume(g_pSound, g_OutActionMusicVolume);
			n = SaveVar_Inc("OutMusicAction", 1) % g_iOutActionMusic + 1;
			fmt = g_OutActionMusic;
		} else {
			g_pSound->vftable->StopMusic(g_pSound);
			g_pSound->vftable->SetMusicVolume(g_pSound, g_OutRelaxMusicVolume);
			n = SaveVar_Inc("OutMusicRelax", 1) % g_iOutRelaxMusic + 1;
			fmt = g_OutRelaxMusic;
		}
		Str_PrintfQuiet(name, sizeof(name), fmt, n);
		g_pSound->vftable->SetMusic(g_pSound, name);
		return;
	}
	float wait = g_MusicWait;
	if (wait > 0.0f) {
		g_MusicWait = wait - dt;
	} else if (want != cur) {
		float t = g_MusicTimeChange;
		g_MusicAction = want;
		if (want) {
			g_MusicFade = 0x1.4f8b58p-17f;
		} else {
			g_MusicFade = t;
		}
		g_MusicWait = t;
	}
}

/* the talk voice */

void Sound_StopTalk(void) {
	g_pSound->vftable->StopVoice(g_pSound, TALK_VOICE);
	g_MaxPoliceWarnDist = g_MaxPoliceWarnDistDef;
	g_WarnVoiceVolume = g_WarnVoiceVolumeDef;
}

/* the talk voice name has ended: stops it, or later when it has not started yet */
void Sound_TalkDone(const char *name) {
	char cur[0x80];

	cur[0] = 0;
	g_pSound->vftable->GetVoiceString(g_pSound, TALK_VOICE, cur, sizeof(cur));
	if (strcmp(name, cur)) {
		return;
	}
	if (g_TalkDelay > 0.0f) {
		g_TalkStop = 1;
		return;
	}
	Sound_StopTalk();
}

void Sound_Update(float dt) {
	char cur[0x80];

	if (g_useDynamicMusic && (g_IsMapOutDoor || g_isActionIndoor)) {
		Sound_UpdateMusic(dt);
	}
	if (g_SndPSEnabled) {
		for (cSoundPS *ps = g_SndPSList; ps; ps = ps->next) {
			cSoundPS_Update(ps, dt);
		}
	}
	if (!g_ForceNoActionMusic && !g_IsMapOutDoor && g_isActionIndoor && g_pWorld->alarmOn &&
		g_RemainTalkTime <= 0.0f) {
		g_S_VoiceoverVolume = 1.0f;
	} else {
		g_S_VoiceoverVolume = g_S_VoiceoverVolumeDef;
	}
	if (g_TalkDelay > 0.0f) {
		g_TalkDelay = g_TalkDelay - dt;
		g_pSound->vftable->GetVoiceString(g_pSound, TALK_VOICE, cur, sizeof(cur));
		if (!cur[0]) {
			g_TalkStop = 1;
		} else if (g_TalkDelay <= 0.0f) {
			if (g_TalkStop) {
				Sound_StopTalk();
			} else {
				g_pSound->vftable->PlayVoice(g_pSound, TALK_VOICE);
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		Sound_UpdateVoice(i);
	}
}
