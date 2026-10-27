/*
 * Sound/Sound.cpp of the game: the sound system start and end, the base class CSoundSys (the listeners and the 3D
 * volume, pan and doppler of a sound, the music and voice calls of the interface) and the named sounds cSound, see
 * include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/scrap.h"
#include "scrap/sound.h"

/* the voice and music channels of ScrapEngMain.cpp (FMOD streams), byte results passed on in eax */
int Fmod_Init(void);						/* rva 0x232640 */
int Fmod_Close(void);						/* rva 0x232620 */
int Voice_GetString(int voice, char *buf, int size);		/* rva 0x2327d0 */
int Voice_Unused(int voice);					/* rva 0x232840, CHECK(0) */
int Voice_Open(const char *name, int voice, int arg);		/* rva 0x2328e0 */
int Voice_SetParam(int voice, float v);				/* rva 0x232a80 */
int Voice_Play(int voice);					/* rva 0x232b40 */
int Voice_Remain(int voice, float *remain);			/* rva 0x232c60 */
int Voice_SetString(int voice, const char *text);		/* rva 0x232d10 */
int Voice_Stop(int voice);					/* rva 0x232d80 */
int Voice_SetVolume(int voice, float volume);			/* rva 0x232dd0 */

/* the start and the end of the sound system */

void Sound_Init(void) {
	CHECK(!g_pSound);
	CSoundSys *s = (CSoundSys *)CSoundXBox_New();
	g_pSound = s;
	s->vftable->Init(s);
	Con_Printf("Sound system initialized\n");
}

void Sound_Done(void) {
	CHECK(g_pSound);
	g_pSound->vftable->Close(g_pSound);
	if (g_pSound) {
		g_pSound->vftable->Destroy(g_pSound, 1);
		g_pSound = NULL;
	}
	Con_Printf("Sound system closed\n");
}

/* volume 0..1: 600 per doubling (6 dB), SOUND_VOL_OFF below 1/65536 */
int Snd_Volume(float volume) {
	if (volume < 0x1p-16f) {
		return SOUND_VOL_OFF;
	}
	double v = Game_Log((double)Float_MinSS(1.0f, volume));
	return (int)(float)(v / Game_Log(2.0) * 600.0);
}

/* cSoundDesc */

void __thiscall cSoundDesc_Init(cSoundDesc *desc) {
	desc->volume = 0;
	desc->pan = 0;
	desc->freq = 1.0f;
	desc->loop = 0;
	desc->prio = 0x80;
	desc->param3 = 1.0f;
	desc->param1 = 0.0f;
	desc->param2 = 0.0f;
}

void __thiscall cSoundDesc_Copy(cSoundDesc *desc, const cSoundDesc *src) {
	*desc = *src;
}

void __thiscall cSoundDesc_Set(cSoundDesc *desc, int volume, int pan, float freq, byte loop, byte prio) {
	desc->volume = volume;
	desc->pan = pan;
	desc->loop = loop;
	desc->freq = freq;
	desc->prio = prio;
}

void __thiscall cSoundDesc_SetPos(cSoundDesc *desc, const vec3_t *pos) {
	vec3_t vel;

	CHECK(g_pSound);
	vel.x = vel.y = vel.z = 0.0f;
	g_pSound->vftable->Compute3D(g_pSound, desc, pos, &vel, 0);
}

/* cSound */

cSound *__fastcall cSound_Init(cSound *self) {
	cSoundDesc_Init(&self->desc);
	self->name[0] = 0;
	self->handle = 0;
	return self;
}

cSound *__thiscall cSound_ctor(cSound *self, const char *name, uint32_t flags) {
	cSound_Init(self);
	cSound_SetSound(self, name, flags);
	return self;
}

const char *__fastcall cSound_GetName(cSound *self) {
	return self->name;
}

void __fastcall cSound_Stop(cSound *self) {
	CHECK(g_pSound);
	if (self->handle) {
		g_pSound->vftable->StopSound(g_pSound, self->handle);
		self->handle = 0;
	}
}

void __thiscall cSound_SetSound(cSound *self, const char *name, uint32_t flags) {
	CHECK(g_pSound);
	cSound_Stop(self);
	self->name[0] = 0;
	self->handle = 0;
	if (!name) {
		return;
	}
	if (flags & 1) {
		g_pSound->vftable->GetSoundName(g_pSound, name, self->name, SOUND_NAME_MAX_LEN);
		g_pSound->vftable->LoadSound2(g_pSound, name);
	} else if (flags & 2) {
		g_pSound->vftable->GetSoundName(g_pSound, name, self->name, SOUND_NAME_MAX_LEN);
		g_pSound->vftable->LoadSound(g_pSound, name);
	} else {
		CHECK(strlen(name) < SOUND_NAME_MAX_LEN);
		Str_Copy(self->name, name, SOUND_NAME_MAX_LEN);
	}
	g_pSound->vftable->GetDesc(g_pSound, self->name, &self->desc);
	self->desc.volume = 0;
	self->desc.pan = 0;
	self->desc.freq = 1.0f;
}

void __thiscall cSound_SetPitch(cSound *self, float semitones) {
	if (semitones != 0.0f) {
		self->desc.freq = (float)Game_Pow(2.0, (double)semitones / 12.0) * self->desc.freq;
	}
}

byte __fastcall cSound_Play(cSound *self) {
	cSound_Stop(self);
	return (byte)g_pSound->vftable->PlaySound(g_pSound, self->name, &self->desc, &self->handle);
}

byte __thiscall cSound_PlayAt(cSound *self, const vec3_t *pos, int volume) {
	vec3_t vel;

	CHECK(g_pSound);
	vel.x = vel.y = vel.z = 0.0f;
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, &vel, 0);
	self->desc.volume += volume;
	return cSound_Play(self);
}

byte __thiscall cSound_PlayVP(cSound *self, int volume, int pan) {
	self->desc.volume = volume;
	self->desc.pan = pan;
	return cSound_Play(self);
}

void __thiscall cSound_SetPosVel(cSound *self, const vec3_t *pos, const vec3_t *vel) {
	CHECK(g_pSound);
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, vel, 1);
}

byte __fastcall cSound_Update(cSound *self) {
	CHECK(g_pSound);
	if (self->desc.loop) {
		if (self->desc.volume > (int)(g_SoundAutoplayVolume * 100.0f)) {
			if (self->desc.volume >= 300 - (int)(g_SoundAutoplayVolume * -100.0f) && !self->handle) {
				g_pSound->vftable->PlaySound(g_pSound, self->name, &self->desc, &self->handle);
			}
		} else if (self->handle) {
			cSound_Stop(self);
		}
	}
	if (self->handle) {
		if (g_pSound->vftable->UpdateSound(g_pSound, self->name, &self->desc, self->handle)) {
			return 1;
		}
		cSound_Stop(self);
	}
	return 0;
}

byte __thiscall cSound_UpdateAt(cSound *self, const vec3_t *pos, int volume) {
	vec3_t vel;

	CHECK(g_pSound);
	vel.x = vel.y = vel.z = 0.0f;
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, &vel, 0);
	self->desc.volume += volume;
	return cSound_Update(self);
}

byte __thiscall cSound_UpdatePosVel(cSound *self, const vec3_t *pos, const vec3_t *vel, int volume) {
	CHECK(g_pSound);
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, vel, 1);
	self->desc.volume += volume;
	return cSound_Update(self);
}

byte __thiscall cSound_UpdateVP(cSound *self, int volume, int pan) {
	self->desc.volume = volume;
	self->desc.pan = pan;
	return cSound_Update(self);
}

/* CSoundSys */

CSoundSys *__fastcall CSoundSys_ctor(CSoundSys *self) {
	self->vftable = CSoundSys_vftable;
	self->lastListener = -1;
	for (int i = 0; i < SOUND_MAX_LISTENERS; i++) {
		cSoundListener *l = &self->listeners[i];
		l->pos.x = l->pos.y = l->pos.z = 0.0f;
		l->ang.x = l->ang.y = l->ang.z = 0.0f;
		l->vel.x = l->vel.y = l->vel.z = 0.0f;
	}
	return self;
}

void __fastcall CSoundSys_dtor(CSoundSys *self) {
	self->vftable = CSoundSys_vftable;
}

CSoundSys *__thiscall CSoundSys_Destroy(CSoundSys *self, byte flags) {
	self->vftable = CSoundSys_vftable;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CSoundSys));
	}
	return self;
}

/* the functions without code */
void Sound_Nop(void) {
}

void __fastcall CSoundSys_Nop(CSoundSys *self) {
	(void)self;
}

void __thiscall CSoundSys_Nop2(CSoundSys *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
}

/* vtable slot 0x70 called directly, a tail jump in the original */
void __fastcall CSoundSys_CallVf1c(CSoundSys *self) {
	self->vftable->vf1c(self);
}

int __fastcall CSoundSys_InitFmod(CSoundSys *self) {
	(void)self;
	return Fmod_Init();
}

void __fastcall CSoundSys_Close(CSoundSys *self) {
	self->vftable->vf1c(self);
	Fmod_Close();
}

void __fastcall CSoundSys_ResetListeners(CSoundSys *self) {
	self->lastListener = -1;
}

void __thiscall CSoundSys_SetListener(CSoundSys *self, int id, const vec3_t *pos, const vec3_t *ang,
	const vec3_t *vel) {
	CHECK(id >= 0 && id < SOUND_MAX_LISTENERS);
	self->lastListener = id;
	cSoundListener *l = &self->listeners[id];
	l->pos = *pos;
	l->ang = *ang;
	l->vel = *vel;
	X87_SinCos(-l->ang.x, &l->sin.x, &l->cos.x);
	X87_SinCos(-l->ang.y, &l->sin.y, &l->cos.y);
	X87_SinCos(-l->ang.z, &l->sin.z, &l->cos.z);
	if (g_SoundShowSetMode) {
		Con_Printf("Listener:  Pos: %f %f %f  Vel: %f %f %f\n", (double)pos->x, (double)pos->y, (double)pos->z,
			(double)vel->x, (double)vel->y, (double)vel->z);
	}
}

/* the direction from the listener to pos, of length 1 unless it is 0 */
static void Snd_Dir(vec3_t *out, const vec3_t *pos, const cSoundListener *l) {
	float dx = pos->x - l->pos.x;
	float dy = pos->y - l->pos.y;
	float dz = pos->z - l->pos.z;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));

	if (len == 0.0f) {
		out->x = dx;
		out->y = dy;
		out->z = dz;
		return;
	}
	float inv = 1.0f / len;
	out->x = dx * inv;
	out->y = dy * inv;
	out->z = dz * inv;
}

void __thiscall CSoundSys_Compute3D(CSoundSys *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel,
	byte doppler) {
	CHECK(d);
	int n = self->lastListener;
	int best = 0;
	if (n < 0) {
		d->volume = SOUND_VOL_OFF;
		return;
	}
	if (n > 0) {
		float bestd = 0x1.fffffep127f;
		for (int i = 0; i <= n; i++) {
			const cSoundListener *l = &self->listeners[i];
			float dy = pos->y - l->pos.y;
			float dx = pos->x - l->pos.x;
			float dz = pos->z - l->pos.z;
			float dist = dy * dy + dx * dx + dz * dz;
			if (bestd > dist) {
				best = i;
				bestd = dist;
			}
		}
	}
	const cSoundListener *l = &self->listeners[best];
	float end = d->param2;
	vec3_t rel, r;
	rel.x = pos->x - l->pos.x;
	rel.y = pos->y - l->pos.y;
	rel.z = pos->z - l->pos.z;
	if (end != 0.0f && rel.y * rel.y + rel.x * rel.x + rel.z * rel.z > end * end) {
		d->volume = SOUND_VOL_OFF;
		return;
	}
	/* in the view of the listener: x to the side, y up */
	const vec3_t *pr = Vec3_RotateSC(&r, &rel, &l->cos, &l->sin);
	float rx = pr->x, ry = pr->y, rz = pr->z;
	float zz = rz * rz;
	float xx = rx * rx;
	float h = (float)__builtin_sqrt((double)(zz + xx));
	float side = h == 0.0f ? rx : 1.0f / h * rx;
	float ini = d->param1;
	if (ini == 0.0f && d->param2 == 0.0f) {
		int vmin = (int)(g_SoundAutoplayVolume * 100.0f);
		float v = Float_MaxSS(-((ry * ry + xx + zz) * g_SoundDistAttenuation * 0x1.a36e2ep-14f), -100000.0f);
		if ((float)vmin >= v) {
			v = -100000.0f;
		}
		d->volume = (int)v;
		double s = (double)side;
		d->pan = (int)(__builtin_fabs(s) * s * (double)g_SoundPanningFactor * 100.0);
	} else {
		float dist = (float)__builtin_sqrt((double)(ry * ry + xx + zz));
		end = d->param2;
		ini = d->param1;
		if (ini >= end) {
			int vmin = (int)(g_SoundAutoplayVolume * -100.0f);
			end = (float)__builtin_sqrt((double)((float)vmin / (g_SoundDistAttenuation * 0x1.a36e2ep-14f))) + ini;
		}
		float f = 1.0f;
		if (dist > ini) {
			float t = (end - dist) / (end - ini);
			float x = 0.0f > t ? 0.0f : Float_MinSS(1.0f, t);
			f = (3.0f - (x + x)) * (x * x);
		}
		int vmin = (int)(g_SoundAutoplayVolume * 100.0f);
		float g = 1.0f - f;
		d->volume = (int)((float)vmin * (g * g));
		double s = (double)side;
		d->pan = (int)(__builtin_fabs(s) * s * (double)g * (double)g_SoundPanningFactor * 100.0);
		if (dist > end) {
			d->volume = SOUND_VOL_OFF;
		}
	}
	if (doppler) {
		vec3_t dir;
		float vx = l->vel.x - vel->x;
		float vy = l->vel.y - vel->y;
		float vz = l->vel.z - vel->z;
		Snd_Dir(&dir, pos, l);
		float s = (dir.y * vy + dir.x * vx + dir.z * vz) * 0x1.47ae14p-7f * (d->param3 * g_SoundDopplerLevel) / 340.0f;
		d->freq = 0.0f > s ? 1.0f / (1.0f - s) : 2.0f - 1.0f / (s + 1.0f);
	}
	if (g_SoundShowSetMode && d->volume > -1800) {
		vec3_t dir;
		int vol = d->volume;
		Snd_Dir(&dir, pos, l);
		float fvel = (dir.y * (l->vel.y - vel->y) + dir.x * (l->vel.x - vel->x) + dir.z * (l->vel.z - vel->z)) *
			0x1.47ae14p-7f;
		Con_Printf("  Object:  Pos: %f %f %f  Vel: %f %f %f  vol: %d  fvel: %f\n", (double)pos->x, (double)pos->y,
			(double)pos->z, (double)vel->x, (double)vel->y, (double)vel->z, vol, (double)fvel);
	}
}

/* the per name parameter setters: GetDesc, one field, SetDesc */
void __thiscall CSoundSys_SetVolume(CSoundSys *self, const char *name, int volume) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.volume = volume;
		self->vftable->SetDesc(self, name, &d);
	}
}

void __thiscall CSoundSys_SetPan(CSoundSys *self, const char *name, int pan) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.pan = pan;
		self->vftable->SetDesc(self, name, &d);
	}
}

void __thiscall CSoundSys_SetFreq(CSoundSys *self, const char *name, float freq) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.freq = freq;
		self->vftable->SetDesc(self, name, &d);
	}
}

void __thiscall CSoundSys_SetLoop(CSoundSys *self, const char *name, byte loop) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.loop = loop;
		self->vftable->SetDesc(self, name, &d);
	}
}

void __thiscall CSoundSys_SetPrio(CSoundSys *self, const char *name, byte prio) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.prio = prio;
		self->vftable->SetDesc(self, name, &d);
	}
}

void __thiscall CSoundSys_GetSoundName(CSoundSys *self, const char *file, char *out, int size) {
	char name[256];

	(void)self;
	CHECK(file && out);
	CHECK(strlen(file) < sizeof(name));
	crt_splitpath(file, NULL, NULL, name, NULL);
	CHECK(strlen(name) < SOUND_NAME_MAX_LEN);
	CHECK(SOUND_NAME_MAX_LEN <= size);
	Str_Copy(out, name, size);
}

/* the music player of ScrapEngMain.cpp */
void __thiscall CSoundSys_SetMusic(CSoundSys *self, const char *name) {
	(void)self;
	Music_Play(name, "play", 0.0f);
}

void __thiscall CSoundSys_SetMusicVolume(CSoundSys *self, float volume) {
	(void)self;
	Music_Play(NULL, "volume", volume);
}

void __fastcall CSoundSys_StopMusic(CSoundSys *self) {
	(void)self;
	Music_Play(NULL, "stop", 0.0f);
}

/* the voices, also ScrapEngMain.cpp */
int __thiscall CSoundSys_OpenVoice(CSoundSys *self, const char *name, int voice, int arg) {
	(void)self;
	return Voice_Open(name, voice, arg);
}

int __thiscall CSoundSys_PlayVoice(CSoundSys *self, int voice) {
	(void)self;
	return Voice_Play(voice);
}

int __thiscall CSoundSys_StopVoice(CSoundSys *self, int voice) {
	(void)self;
	return Voice_Stop(voice);
}

int __thiscall CSoundSys_VoiceRemain(CSoundSys *self, int voice, float *remain) {
	(void)self;
	return Voice_Remain(voice, remain);
}

int __thiscall CSoundSys_SetVoiceVolume(CSoundSys *self, int voice, float volume) {
	(void)self;
	return Voice_SetVolume(voice, volume);
}

int __thiscall CSoundSys_SetVoiceParam(CSoundSys *self, int voice, float v) {
	(void)self;
	return Voice_SetParam(voice, v);
}

int __thiscall CSoundSys_SetVoiceString(CSoundSys *self, int voice, const char *text) {
	(void)self;
	return Voice_SetString(voice, text);
}

int __thiscall CSoundSys_GetVoiceString(CSoundSys *self, int voice, char *buf, int size) {
	(void)self;
	return Voice_GetString(voice, buf, size);
}

int __thiscall CSoundSys_VoiceUnused(CSoundSys *self, int voice) {
	(void)self;
	return Voice_Unused(voice);
}
