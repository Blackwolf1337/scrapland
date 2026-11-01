/*
 * Sound/PosSound.cpp of the game: the positional sounds by name (cSoundPS, the property sets of SSound), the
 * dynamic music of the levels (relax music, action music while enemies are on the radar or the alarm is on, with a
 * fade between them) and the timing of the talk voice. See include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/arrowindicator.h"
#include "scrap/console.h"
#include "scrap/cstring.h"
#include "scrap/hashtable.h"
#include "scrap/logic.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/sound.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

/* engine variables of the music and the voices (value, default) */
#define g_isActionMusic GAME_GLOBAL_IN(int, GVar_isActionMusic, 16)
#define g_isActionMusicDef GAME_GLOBAL_IN(int, GVar_isActionMusic, 20)
#define g_WarnVoiceVolume GAME_GLOBAL_IN(float, GVar_WarnVoiceVolume, 16)
#define g_WarnVoiceVolumeDef GAME_GLOBAL_IN(float, GVar_WarnVoiceVolume, 20)
#define g_isActionIndoor GAME_GLOBAL_IN(int, GVar_isActionIndoor, 16)
#define g_isActionIndoorDef GAME_GLOBAL_IN(int, GVar_isActionIndoor, 20)
#define g_MaxPoliceWarnDist GAME_GLOBAL_IN(float, GVar_MaxPoliceWarnDist, 16)
#define g_MaxPoliceWarnDistDef GAME_GLOBAL_IN(float, GVar_MaxPoliceWarnDist, 20)
#define g_useDynamicMusic GAME_GLOBAL_IN(int, GVar_useDynamicMusic, 16)
#define g_useDynamicMusicDef GAME_GLOBAL_IN(int, GVar_useDynamicMusic, 20)
#define g_ForceNoActionMusic GAME_GLOBAL_IN(int, GVar_ForceNoActionMusic, 16)
#define g_ForceNoActionMusicDef GAME_GLOBAL_IN(int, GVar_ForceNoActionMusic, 20)
#define g_ActionMusicTimeFade GAME_GLOBAL_IN(float, GVar_ActionMusicTimeFade, 16)
#define g_OutActionMusicVolume GAME_GLOBAL_IN(float, GVar_OutActionMusicVolume, 16)
#define g_OutRelaxMusicVolume GAME_GLOBAL_IN(float, GVar_OutRelaxMusicVolume, 16)
#define g_iOutActionMusic GAME_GLOBAL_IN(int, GVar_iOutActionMusic, 16)
#define g_iOutRelaxMusic GAME_GLOBAL_IN(int, GVar_iOutRelaxMusic, 16)
#define g_OutActionMusic GAME_GLOBAL_IN(const char *, GVar_OutActionMusic, 16)	/* format with the number */
#define g_OutRelaxMusic GAME_GLOBAL_IN(const char *, GVar_OutRelaxMusic, 16)
#define g_MusicTimeChange GAME_GLOBAL_IN(float, GVar_MusicTimeChange, 16)
#define g_NumEnemiesInRadar GAME_GLOBAL_IN(int, GVar_NumEnemiesInRadar, 16)
#define g_S_VoiceoverVolume GAME_GLOBAL_IN(float, GVar_S_VoiceoverVolume, 16)
#define g_S_VoiceoverVolumeDef GAME_GLOBAL_IN(float, GVar_S_VoiceoverVolume, 20)

/* the state of the music and of the talk voice (voice 2) */
#define g_MusicFade GAME_GLOBAL(float, g_MusicFade)		/* the fade to the other music */
#define g_MusicWait GAME_GLOBAL(float, g_MusicWait)		/* no new change before it runs out */
#define g_MusicAction GAME_GLOBAL(int, g_MusicAction)		/* the action music plays */
#define g_TalkDelay GAME_GLOBAL(float, g_TalkDelay)		/* until the talk voice starts */
#define g_TalkStop GAME_GLOBAL(byte, g_TalkStop)		/* stop instead of starting it */

#define TALK_VOICE 2

#define g_MinVoiceVolumeChat GAME_GLOBAL_IN(float, GVar_MinVoiceVolumeChat, 16)	/* MinVoiceVolumeChat */
#define VOICE_END 0.001f		/* rva 0x3a3d58: the talk ends with less left */
#define VOICE_PAN -0.5f
#define VOICE_MIN_SPEED 0.8f		/* rva 0x3b0844: no voices in slow motion */
#define g_VoicePlayDelay GAME_GLOBAL_IN(float, GVar_VoicePlayDelay, 16)	/* VoicePlayDelay */
#define g_StreamPath GAME_GLOBAL_IN(const char *, GVar_StreamPath, 16)	/* StreamPath "Sounds/Stream/" */
#define g_VoicePath GAME_GLOBAL_IN(const char *, GVar_VoicePath, 16)	/* VoicePath "Sounds/Voices/" */
#define g_VoiceCount GAME_GLOBAL(uint32_t, g_VoiceCount)	/* the voices played, picks the variant */

void Sound_UpdateVoice(int voice);

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
			if (g_pWorld->alarm.on) {
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

/* the volume of voice 0..2 by the distance of its speaker to the player (a "WalkChar"): the chat voices fade out
 * between ArrowActiveRadius and ArrowDeactivationRadius of the gap, the talk voice (2) fades over
 * MaxPoliceWarnDist, is panned by the side the speaker is on and stops at its end; MinVoiceVolumeChat is always
 * heard. Without the speaker the voice stops. */
void Sound_UpdateVoice(int voice) {
	char name[0x80];
	vec3_t tmp1, tmp2, d, rot;
	float remain, vol;

	g_pSound->vftable->GetVoiceString(g_pSound, voice, name, sizeof(name));
	if (!name[0]) {
		return;
	}
	cEntity *user = g_pWorld->owners[0];
	if (user) {
		CHECK(Entity_IsType(user, TYEN_WITH_LIFE));
	}
	user = g_pWorld->owners[0];
	if (!user || !user->vftable->IsType(user, TYEN_WALKCHAR)) {
		return;
	}
	cEntity *sp = World_FindEntity(name);
	if (!sp || !sp->vftable->IsType(sp, TYEN_WALKCHAR) || !((cWithLifeEntity *)sp)->life) {
		if (voice == TALK_VOICE && g_TalkDelay > 0.0f) {
			return;
		}
		g_pSound->vftable->StopVoice(g_pSound, voice);
		return;
	}
	vol = 1.0f;
	const cObjEntityVtbl *uv = (const cObjEntityVtbl *)user->vftable;
	const cObjEntityVtbl *sv = (const cObjEntityVtbl *)sp->vftable;
	if (voice != TALK_VOICE) {
		const vec3_t *pu = uv->GetViewPos(user, &tmp1);
		const vec3_t *ps = sv->GetViewPos(sp, &tmp2);
		float dx = ps->x - pu->x;
		float dy = ps->y - pu->y;
		float dz = ps->z - pu->z;
		float gap = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) - ((cWithLifeEntity *)user)->prim.radius -
			((cWithLifeEntity *)sp)->prim.radius;
		if (gap > g_ArrowMaxDist) {
			if (gap > g_ArrowDeactivationRadius) {
				vol = 0.0f;
			} else {
				vol = 1.0f - (gap - g_ArrowMaxDist) / (g_ArrowDeactivationRadius - g_ArrowMaxDist);
			}
		}
	} else {
		if (g_TalkDelay > 0.0f) {
			return;
		}
		const vec3_t *pu = uv->GetViewPos(user, &tmp1);
		const vec3_t *ps = sv->GetViewPos(sp, &tmp2);
		d.x = ps->x - pu->x;
		d.y = ps->y - pu->y;
		d.z = ps->z - pu->z;
		float f = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z)) / g_MaxPoliceWarnDist;
		float k = 0.0f;
		if (!(f > 1.0f)) {
			k = 1.0f;
			if (!(0.0f > f)) {
				k = 1.0f - f;
			}
		}
		vol = g_WarnVoiceVolume * k;
		g_pSound->vftable->VoiceRemain(g_pSound, TALK_VOICE, &remain);
		if (VOICE_END >= remain || vol == 0.0f) {
			g_pSound->vftable->StopVoice(g_pSound, TALK_VOICE);
			return;
		}
		rot = ((cWalkCharEntity *)user)->view;
		vec3_t r = *Vec3_RotateInv(&tmp1, &d, &rot);
		float xz[2] = { r.x, r.z };
		float n[2];
		Vec2_Normalize(n, xz);
		g_pSound->vftable->SetVoiceParam(g_pSound, TALK_VOICE, n[0] * VOICE_PAN);
	}
	g_pSound->vftable->SetVoiceVolume(g_pSound, voice, (1.0f - g_MinVoiceVolumeChat) * vol + g_MinVoiceVolumeChat);
}

/* the talk voice of the character name for event (in an alarm, or "AnmStr" and "Abort" always): the file arg (in
 * VoicePath with force, else StreamPath) or a warning of its type (Gear, Police, the generic ones) picked by the count;
 * not while another one talks, in slow motion, for the player itself (but "AnmStr") or too far away. 1 if it opens */
byte Sound_PlayVoice(const char *name, const char *event, int arg, int force) {
	char file[0x80], buf[0x80];
	vec3_t t1, t2;

	if (g_RemainTalkTime > 0.0f || g_TalkDelay > 0.0f) {
		return 0;
	}
	if (VOICE_MIN_SPEED > cTimer_GetSpeed(&g_pWorld->timer)) {
		return 0;
	}
	byte anm = !strcmp(event, "AnmStr");
	byte abort = !strcmp(event, "Abort");
	if (!abort && !anm && !g_pWorld->alarm.on) {
		return 0;
	}
	g_pSound->vftable->GetVoiceString(g_pSound, TALK_VOICE, buf, sizeof(buf));
	if (buf[0]) {
		return 0;
	}
	cEntity *user = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!user || !user->vftable->IsType(user, TYEN_WALKCHAR)) {
		return 0;
	}
	cEntity *sp = World_FindEntity(name);
	if (!sp || (!anm && sp == cWorld_GetUsrEntity(g_pWorld, 0)) || !sp->vftable->IsType(sp, TYEN_WALKCHAR)) {
		return 0;
	}
	float r = (float)((double)(crt_rand() & 0x7fff) * 0x1.0002000400080p-15 * 4.0 - 3.0);
	if (0.0f > r && !strcmp(event, "Acuse")) {
		return 0;
	}
	const vec3_t *pu = ((const cObjEntityVtbl *)user->vftable)->GetViewPos(user, &t1);
	const vec3_t *ps = ((const cObjEntityVtbl *)sp->vftable)->GetViewPos(sp, &t2);
	float dx = ps->x - pu->x;
	float dy = ps->y - pu->y;
	float dz = ps->z - pu->z;
	if ((float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) >= g_MaxPoliceWarnDist) {
		return 0;
	}
	g_VoiceCount++;
	const char *type = Logic_TypeName(sp);
	if (arg) {
		crt_sprintf(file, "%s%s.mp3", (byte)force ? g_VoicePath : g_StreamPath, (const char *)(uintptr_t)arg);
	} else {
		if (!strcmp(type, "Boss")) {
			return 0;
		}
		uint32_t n = g_VoiceCount;
		if (abort) {
			crt_sprintf(file, "%sGenerics_GearWarning_%d.mp3", g_VoicePath, (n & 1) + 7);
		} else if (!strcmp(type, "Gear")) {
			if (!strcmp(event, "Fire")) {
				crt_sprintf(file, "%sGenerics_GearShooting_%d.mp3", g_VoicePath, n % 5 + 1);
			} else {
				uint32_t m = n % 9;
				crt_sprintf(file, "%sGenerics_GearWarning_%d.mp3", g_VoicePath, m <= 5 ? m + 1 : m + 3);
			}
		} else if (!strcmp(type, "Police") || !strcmp(type, "PoliceBoss")) {
			if (n % 3 == 2) {
				crt_sprintf(file, "%sGenerics_GearWarning_9.mp3", g_VoicePath);
			} else {
				crt_sprintf(file, "%sGenerics_PoliceWarning_%d.mp3", g_VoicePath, n % 3 + 12);
			}
		} else {
			crt_sprintf(file, "%sGenerics_%s_Warning_%d.mp3", g_VoicePath, type, n % 3 + 1);
		}
	}
	g_pSound->vftable->OpenVoice(g_pSound, file, TALK_VOICE, (byte)force || !arg ? 1 : 0);
	g_pSound->vftable->SetVoiceString(g_pSound, TALK_VOICE, sp->name);
	g_TalkDelay = g_VoicePlayDelay;
	g_TalkStop = 0;
	return 1;
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
	if (!g_ForceNoActionMusic && !g_IsMapOutDoor && g_isActionIndoor && g_pWorld->alarm.on &&
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
