/*
 * Sound/PosSound.cpp of the game: the positional sounds by name (cSoundPS, the property sets of SSound), the
 * dynamic music of the levels (relax music, action music while enemies are on the radar or the alarm is on, with a
 * fade between them) and the timing of the talk voice. See include/scrap/sound.h.
 */
#include <stddef.h>
#include <string.h>

#include "scrap/gvar.h"
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
#include "scrap/staticinit.h"

static cGVar GVar_isActionMusic;

/* engine variables of the music and the voices (value, default) */
#define g_isActionMusic (GVar_isActionMusic.value.i)
#define g_isActionMusicDef (GVar_isActionMusic.def.i)
cGVar GVar_WarnVoiceVolume;

#define g_WarnVoiceVolume (GVar_WarnVoiceVolume.value.f)
#define g_WarnVoiceVolumeDef (GVar_WarnVoiceVolume.def.f)
static cGVar GVar_isActionIndoor;
#define g_isActionIndoor (GVar_isActionIndoor.value.i)
#define g_isActionIndoorDef (GVar_isActionIndoor.def.i)
cGVar GVar_MaxPoliceWarnDist;

#define g_MaxPoliceWarnDist (GVar_MaxPoliceWarnDist.value.f)
#define g_MaxPoliceWarnDistDef (GVar_MaxPoliceWarnDist.def.f)
static cGVar GVar_useDynamicMusic;
#define g_useDynamicMusic (GVar_useDynamicMusic.value.i)
#define g_useDynamicMusicDef (GVar_useDynamicMusic.def.i)
static cGVar GVar_ForceNoActionMusic;
#define g_ForceNoActionMusic (GVar_ForceNoActionMusic.value.i)
#define g_ForceNoActionMusicDef (GVar_ForceNoActionMusic.def.i)
static cGVar GVar_ActionMusicTimeFade;
#define g_ActionMusicTimeFade (GVar_ActionMusicTimeFade.value.f)
static cGVar GVar_OutActionMusicVolume;
#define g_OutActionMusicVolume (GVar_OutActionMusicVolume.value.f)
static cGVar GVar_OutRelaxMusicVolume;
#define g_OutRelaxMusicVolume (GVar_OutRelaxMusicVolume.value.f)
static cGVar GVar_iOutActionMusic;
#define g_iOutActionMusic (GVar_iOutActionMusic.value.i)
static cGVar GVar_iOutRelaxMusic;
#define g_iOutRelaxMusic (GVar_iOutRelaxMusic.value.i)
static cGVar GVar_OutActionMusic;
#define g_OutActionMusic (GVar_OutActionMusic.value.s)	/* format with the number */
static cGVar GVar_OutRelaxMusic;
#define g_OutRelaxMusic (GVar_OutRelaxMusic.value.s)
static cGVar GVar_MusicTimeChange;
#define g_MusicTimeChange (GVar_MusicTimeChange.value.f)
cGVar GVar_NumEnemiesInRadar;

#define g_NumEnemiesInRadar (GVar_NumEnemiesInRadar.value.i)
cGVar GVar_S_VoiceoverVolume;

#define g_S_VoiceoverVolume (GVar_S_VoiceoverVolume.value.f)
#define g_S_VoiceoverVolumeDef (GVar_S_VoiceoverVolume.def.f)

/* the state of the music and of the talk voice (voice 2) */

#define TALK_VOICE 2

static cGVar GVar_MinVoiceVolumeChat;
#define g_MinVoiceVolumeChat (GVar_MinVoiceVolumeChat.value.f)	/* MinVoiceVolumeChat */
#define VOICE_END 0.001f		/* the talk ends with less left */
#define VOICE_PAN -0.5f
#define VOICE_MIN_SPEED 0.8f		/* no voices in slow motion */
static cGVar GVar_VoicePlayDelay;
#define g_VoicePlayDelay (GVar_VoicePlayDelay.value.f)	/* VoicePlayDelay */
static cGVar GVar_StreamPath;
#define g_StreamPath (GVar_StreamPath.value.s)	/* StreamPath "Sounds/Stream/" */
static cGVar GVar_VoicePath;
#define g_VoicePath (GVar_VoicePath.value.s)	/* VoicePath "Sounds/Voices/" */

static void Sound_UpdateVoice(int voice);

static cSoundPS *g_SndPSList;

static cSoundPS *cSoundPS_ctor(cSoundPS *self) {
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

static void cSoundPS_dtor(cSoundPS *self) {
	cString_dtor(&self->name);
	cSound_Stop(&self->sound);
}

static void cSoundPS_Update(cSoundPS *self, float dt) {
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

int cSoundPS_Var(cSoundPS *ps, const char *name, int set, char **s, double *d, int *i) {
	static const struct {
		const char *name;
		uint32_t off;
	} floats[] = {
		{ "MaxInterval", offsetof(cSoundPS, maxInterval) },
		{ "MinInterval", offsetof(cSoundPS, minInterval) },
		{ "AttenEnd", offsetof(cSoundPS, sound.desc.param2) },
		{ "AttenIni", offsetof(cSoundPS, sound.desc.param1) },
		{ "Doppler", offsetof(cSoundPS, sound.desc.param3) },
		{ "Volume", offsetof(cSoundPS, volume) },
		{ "Pitch", offsetof(cSoundPS, pitch) },
		{ "X", offsetof(cSoundPS, pos.x) },
		{ "Y", offsetof(cSoundPS, pos.y) },
		{ "Z", offsetof(cSoundPS, pos.z) },
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

static HashTable g_SndPSTable = { .numSlots = 457 };

/* the property sets */

cSoundPS *SndPS_Get(const char *name) {
	return PtrTable_Value((&g_SndPSTable), name);
}

cSoundPS *SndPS_Create(const char *name) {
	if (PtrTable_Find((&g_SndPSTable), name, HashTable_Slot((&g_SndPSTable), name))) {
		return NULL;
	}
	cSoundPS *ps = operator_new(sizeof(cSoundPS));
	ps = ps ? cSoundPS_ctor(ps) : NULL;
	cString_Set(&ps->name, name);
	PtrTable_Add((&g_SndPSTable), ps->name, (void *const *)&ps, 1);
	return ps;
}

void SndPS_StopAll(void) {
	for (cSoundPS *ps = g_SndPSList; ps; ps = ps->next) {
		if (ps->loop) {
			cSound_Stop(&ps->sound);
		}
	}
}

static byte g_SndPSEnabled = 1;

byte SndPS_Use(byte use) {
	SndPS_StopAll();
	g_SndPSEnabled = use;
	return 1;
}

static float g_MusicWait;
static int g_MusicAction;
static float g_MusicFade = 0.00001f;

void Sound_ResetVolumes(void) {
	HashTable *t = (&g_SndPSTable);

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
			operator_delete(ps);
		}
	}
}

/* the debug view: the attenuation spheres in a color by the doppler factor and the names */
static void cSoundPS_Draw(cSoundPS *self) {
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

static void Sound_UpdateMusic(float dt) {
	char name[256];
	int want, cur;

	if (App_Headless()) {
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
			SoundSys_SetMusicVolume(g_pSound, vol * f);
			return;
		}
		if (want != cur) {
			SoundSys_StopMusic(g_pSound);
			g_MusicWait = 0.0f;
			return;
		}
		const char *fmt;
		int n;
		if (cur) {
			SoundSys_StopMusic(g_pSound);
			SoundSys_SetMusicVolume(g_pSound, g_OutActionMusicVolume);
			n = SaveVar_Inc("OutMusicAction", 1) % g_iOutActionMusic + 1;
			fmt = g_OutActionMusic;
		} else {
			SoundSys_StopMusic(g_pSound);
			SoundSys_SetMusicVolume(g_pSound, g_OutRelaxMusicVolume);
			n = SaveVar_Inc("OutMusicRelax", 1) % g_iOutRelaxMusic + 1;
			fmt = g_OutRelaxMusic;
		}
		Str_PrintfQuiet(name, sizeof(name), fmt, n);
		SoundSys_SetMusic(g_pSound, name);
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

static void Sound_StopTalk(void) {
	SoundSys_StopVoice(g_pSound, TALK_VOICE);
	g_MaxPoliceWarnDist = g_MaxPoliceWarnDistDef;
	g_WarnVoiceVolume = g_WarnVoiceVolumeDef;
}

static float g_TalkDelay;
static byte g_TalkStop;

/* the talk voice name has ended: stops it, or later when it has not started yet */
void Sound_TalkDone(const char *name) {
	char cur[0x80];

	cur[0] = 0;
	SoundSys_GetVoiceString(g_pSound, TALK_VOICE, cur, sizeof(cur));
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
static void Sound_UpdateVoice(int voice) {
	char name[0x80];
	vec3_t tmp1, tmp2, d, rot;
	float remain, vol;

	SoundSys_GetVoiceString(g_pSound, voice, name, sizeof(name));
	if (!name[0]) {
		return;
	}
	cEntity *user = g_pWorld->owners[0];
	if (user) {
		CHECK(Entity_IsType(user, TYEN_WITH_LIFE));
	}
	user = g_pWorld->owners[0];
	if (!user || !Entity_IsType(user, TYEN_WALKCHAR)) {
		return;
	}
	cEntity *sp = World_FindEntity(name);
	if (!sp || !Entity_IsType(sp, TYEN_WALKCHAR) || !((cWithLifeEntity *)sp)->life) {
		if (voice == TALK_VOICE && g_TalkDelay > 0.0f) {
			return;
		}
		SoundSys_StopVoice(g_pSound, voice);
		return;
	}
	vol = 1.0f;
	if (voice != TALK_VOICE) {
		const vec3_t *pu = Obj_GetViewPos(user, &tmp1);
		const vec3_t *ps = Obj_GetViewPos(sp, &tmp2);
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
		const vec3_t *pu = Obj_GetViewPos(user, &tmp1);
		const vec3_t *ps = Obj_GetViewPos(sp, &tmp2);
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
		SoundSys_VoiceRemain(g_pSound, TALK_VOICE, &remain);
		if (VOICE_END >= remain || vol == 0.0f) {
			SoundSys_StopVoice(g_pSound, TALK_VOICE);
			return;
		}
		rot = ((cWalkCharEntity *)user)->view;
		vec3_t r = *Vec3_RotateInv(&tmp1, &d, &rot);
		float xz[2] = { r.x, r.z };
		float n[2];
		Vec2_Normalize(n, xz);
		SoundSys_SetVoiceParam(g_pSound, TALK_VOICE, n[0] * VOICE_PAN);
	}
	SoundSys_SetVoiceVolume(g_pSound, voice, (1.0f - g_MinVoiceVolumeChat) * vol + g_MinVoiceVolumeChat);
}

static uint32_t g_VoiceCount;

/* the talk voice of the character name for event (in an alarm, or "AnmStr" and "Abort" always): the file arg (in
 * VoicePath with force, else StreamPath) or a warning of its type (Gear, Police, the generic ones) picked by the count;
 * not while another one talks, in slow motion, for the player itself (but "AnmStr") or too far away. 1 if it opens */
byte Sound_PlayVoice(const char *name, const char *event, const char *arg, int force) {
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
	SoundSys_GetVoiceString(g_pSound, TALK_VOICE, buf, sizeof(buf));
	if (buf[0]) {
		return 0;
	}
	cEntity *user = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!user || !Entity_IsType(user, TYEN_WALKCHAR)) {
		return 0;
	}
	cEntity *sp = World_FindEntity(name);
	if (!sp || (!anm && sp == cWorld_GetUsrEntity(g_pWorld, 0)) || !Entity_IsType(sp, TYEN_WALKCHAR)) {
		return 0;
	}
	float r = (float)((double)(crt_rand() & 0x7fff) * 0x1.0002000400080p-15 * 4.0 - 3.0);
	if (0.0f > r && !strcmp(event, "Acuse")) {
		return 0;
	}
	const vec3_t *pu = Obj_GetViewPos(user, &t1);
	const vec3_t *ps = Obj_GetViewPos(sp, &t2);
	float dx = ps->x - pu->x;
	float dy = ps->y - pu->y;
	float dz = ps->z - pu->z;
	if ((float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) >= g_MaxPoliceWarnDist) {
		return 0;
	}
	g_VoiceCount++;
	const char *type = Logic_TypeName(sp);
	if (arg) {
		crt_sprintf(file, "%s%s.mp3", (byte)force ? g_VoicePath : g_StreamPath, arg);
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
	SoundSys_OpenVoice(g_pSound, file, TALK_VOICE, (byte)force || !arg ? 1 : 0);
	SoundSys_SetVoiceString(g_pSound, TALK_VOICE, sp->name);
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
		SoundSys_GetVoiceString(g_pSound, TALK_VOICE, cur, sizeof(cur));
		if (!cur[0]) {
			g_TalkStop = 1;
		} else if (g_TalkDelay <= 0.0f) {
			if (g_TalkStop) {
				Sound_StopTalk();
			} else {
				SoundSys_PlayVoice(g_pSound, TALK_VOICE);
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		Sound_UpdateVoice(i);
	}
}

cGVar GVar_ArrowDeactivationRadius;
cGVar GVar_ArrowActiveRadius;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_ArrowActiveRadius, &GVar_ArrowActiveRadius, "ArrowActiveRadius", 0, "Indicates the radius between entities to start actions", 200.0f)
INIT_GVAR_FLOAT(Init_GVar_ArrowDeactivationRadius, &GVar_ArrowDeactivationRadius, "ArrowDeactivationRadius", 0, "Indicates the radius between entities to start actions", 400.0f)
INIT_GVAR_INT(Init_GVar_NumEnemiesInRadar, &GVar_NumEnemiesInRadar, "NumEnemiesInRadar", 1, "Number of enemies in the radar", 0)
INIT_GVAR_FLOAT(Init_GVar_VoicePlayDelay, &GVar_VoicePlayDelay, "VoicePlayDelay", 0, "Delay to play an specific voice.\n", 0.5f)
FINI_GVAR_STRING(Fini_GVar_OutRelaxMusic, &GVar_OutRelaxMusic)
INIT_GVAR_STRING(Init_GVar_OutRelaxMusic, Fini_GVar_OutRelaxMusic, &GVar_OutRelaxMusic, "OutRelaxMusic", 0, "Relax Music path for outdoors.\n", "Music/Track%d.ogg")
INIT_GVAR_INT(Init_GVar_iOutRelaxMusic, &GVar_iOutRelaxMusic, "iOutRelaxMusic", 0, "Num of Relax Music path for outdoors.\n", 3)
INIT_GVAR_FLOAT(Init_GVar_OutRelaxMusicVolume, &GVar_OutRelaxMusicVolume, "OutRelaxMusicVolume", 0, "Volume of Relax Music path for outdoors.\n", 1.0f)
FINI_GVAR_STRING(Fini_GVar_OutActionMusic, &GVar_OutActionMusic)
INIT_GVAR_STRING(Init_GVar_OutActionMusic, Fini_GVar_OutActionMusic, &GVar_OutActionMusic, "OutActionMusic", 0, "Action Music path for outdoors.\n", "Music/Outdoor%d.ogg")
INIT_GVAR_INT(Init_GVar_iOutActionMusic, &GVar_iOutActionMusic, "iOutActionMusic", 0, "Num of Action Music path for outdoors.\n", 7)
INIT_GVAR_FLOAT(Init_GVar_OutActionMusicVolume, &GVar_OutActionMusicVolume, "OutActionMusicVolume", 0, "Volume of Action Music path for outdoors.\n", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_ActionMusicTimeFade, &GVar_ActionMusicTimeFade, "ActionMusicTimeFade", 0, "Time to fade off the action musics.\n", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_MusicTimeChange, &GVar_MusicTimeChange, "MusicTimeChange", 0, "minimun time of musicplay after the fade.\n", 15.0f)
INIT_GVAR_INT(Init_GVar_isActionMusic, &GVar_isActionMusic, "isActionMusic", 0, "Modify this value to start/end the action!\n", 0)
INIT_GVAR_INT(Init_GVar_isActionIndoor, &GVar_isActionIndoor, "isActionIndoor", 0, "True if the indoormap uses DinaMusic.\n", 0)
INIT_GVAR_INT(Init_GVar_useDynamicMusic, &GVar_useDynamicMusic, "useDynamicMusic", 0, "Uses dynamic music when set to true.\n", 1)
INIT_GVAR_INT(Init_GVar_ForceNoActionMusic, &GVar_ForceNoActionMusic, "ForceNoActionMusic", 0, "Deactivates the action music.\n", 0)
FINI_CALL(Fini_SndPSTable, &g_SndPSTable, PtrTable_Clear)
INIT_HASHTABLE(Init_SndPSTable, Fini_SndPSTable, &g_SndPSTable, 0x1c9, 4, 0x724, 0, 1)
INIT_GVAR_FLOAT(Init_GVar_MinVoiceVolumeChat, &GVar_MinVoiceVolumeChat, "MinVoiceVolumeChat", 0, "voice minimal volume of thrd person speacher.\n", 0.2f)
INIT_GVAR_FLOAT(Init_GVar_MaxPoliceWarnDist, &GVar_MaxPoliceWarnDist, "MaxPoliceWarnDist", 0, "Distance of warnning police messages.\n", 3000.0f)
INIT_GVAR_FLOAT(Init_GVar_WarnVoiceVolume, &GVar_WarnVoiceVolume, "WarnVoiceVolume", 0, "Voice maximal volume of police messages.\n", 1.0f)
FINI_GVAR_STRING(Fini_GVar_VoicePath, &GVar_VoicePath)
INIT_GVAR_STRING(Init_GVar_VoicePath, Fini_GVar_VoicePath, &GVar_VoicePath, "VoicePath", 0, "Voice default path", "Sounds/Voices/")
FINI_GVAR_STRING(Fini_GVar_StreamPath, &GVar_StreamPath)
INIT_GVAR_STRING(Init_GVar_StreamPath, Fini_GVar_StreamPath, &GVar_StreamPath, "StreamPath", 0, "Stream default path", "Sounds/Stream/")
INIT_GVAR_FLOAT(Init_GVar_S_VoiceoverVolume, &GVar_S_VoiceoverVolume, "S_VoiceoverVolume", 0, "Voiceover relative volume", 0.6f)
