/*
 * Sound/Sound.cpp of the game: the sound system start and end, the base class CSoundSys (the listeners and the 3D
 * volume, pan and doppler of a sound, the music and voice calls of the interface) and the named sounds cSound, see
 * include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/musicfmod.h"
#include "scrap/scrap.h"
#include "scrap/sound.h"
#include "scrap/staticinit.h"

CSoundSys *g_pSound;

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
	double v = log((double)Float_MinSS(1.0f, volume));
	return (int)(float)(v / log(2.0) * 600.0);
}

/* cSoundDesc */

void cSoundDesc_Init(cSoundDesc *desc) {
	desc->volume = 0;
	desc->pan = 0;
	desc->freq = 1.0f;
	desc->loop = 0;
	desc->prio = 0x80;
	desc->param3 = 1.0f;
	desc->param1 = 0.0f;
	desc->param2 = 0.0f;
}

void cSoundDesc_Copy(cSoundDesc *desc, const cSoundDesc *src) {
	*desc = *src;
}

void cSoundDesc_Set(cSoundDesc *desc, int volume, int pan, float freq, byte loop, byte prio) {
	desc->volume = volume;
	desc->pan = pan;
	desc->loop = loop;
	desc->freq = freq;
	desc->prio = prio;
}

void cSoundDesc_SetPos(cSoundDesc *desc, const vec3_t *pos) {
	vec3_t vel;

	CHECK(g_pSound);
	vel.x = vel.y = vel.z = 0.0f;
	g_pSound->vftable->Compute3D(g_pSound, desc, pos, &vel, 0);
}

/* cSound */

cSound *cSound_Init(cSound *self) {
	cSoundDesc_Init(&self->desc);
	self->name[0] = 0;
	self->handle = 0;
	return self;
}

cSound *cSound_ctor(cSound *self, const char *name, uint32_t flags) {
	cSound_Init(self);
	cSound_SetSound(self, name, flags);
	return self;
}

const char *cSound_GetName(cSound *self) {
	return self->name;
}

void cSound_Stop(cSound *self) {
	CHECK(g_pSound);
	if (self->handle) {
		g_pSound->vftable->StopSound(g_pSound, self->handle);
		self->handle = 0;
	}
}

void cSound_SetSound(cSound *self, const char *name, uint32_t flags) {
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

void cSound_SetPitch(cSound *self, float semitones) {
	if (semitones != 0.0f) {
		self->desc.freq = (float)pow(2.0, (double)semitones / 12.0) * self->desc.freq;
	}
}

byte cSound_Play(cSound *self) {
	cSound_Stop(self);
	return (byte)g_pSound->vftable->PlaySound(g_pSound, self->name, &self->desc, &self->handle);
}

byte cSound_PlayAt(cSound *self, const vec3_t *pos, int volume) {
	vec3_t vel;

	CHECK(g_pSound);
	vel.x = vel.y = vel.z = 0.0f;
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, &vel, 0);
	self->desc.volume += volume;
	return cSound_Play(self);
}

byte cSound_PlayVP(cSound *self, int volume, int pan) {
	self->desc.volume = volume;
	self->desc.pan = pan;
	return cSound_Play(self);
}

void cSound_SetPosVel(cSound *self, const vec3_t *pos, const vec3_t *vel) {
	CHECK(g_pSound);
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, vel, 1);
}

byte cSound_Update(cSound *self) {
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

byte cSound_UpdateAt(cSound *self, const vec3_t *pos, int volume) {
	vec3_t vel;

	CHECK(g_pSound);
	vel.x = vel.y = vel.z = 0.0f;
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, &vel, 0);
	self->desc.volume += volume;
	return cSound_Update(self);
}

byte cSound_UpdatePosVel(cSound *self, const vec3_t *pos, const vec3_t *vel, int volume) {
	CHECK(g_pSound);
	g_pSound->vftable->Compute3D(g_pSound, &self->desc, pos, vel, 1);
	self->desc.volume += volume;
	return cSound_Update(self);
}

byte cSound_UpdateVP(cSound *self, int volume, int pan) {
	self->desc.volume = volume;
	self->desc.pan = pan;
	return cSound_Update(self);
}

/* CSoundSys */

CSoundSys *CSoundSys_ctor(CSoundSys *self) {
	self->vftable = CSOUNDSYS_VFTABLE;
	self->lastListener = -1;
	for (int i = 0; i < SOUND_MAX_LISTENERS; i++) {
		cSoundListener *l = &self->listeners[i];
		l->pos.x = l->pos.y = l->pos.z = 0.0f;
		l->ang.x = l->ang.y = l->ang.z = 0.0f;
		l->vel.x = l->vel.y = l->vel.z = 0.0f;
	}
	return self;
}

void CSoundSys_dtor(CSoundSys *self) {
	self->vftable = CSOUNDSYS_VFTABLE;
}

CSoundSys *CSoundSys_Destroy(CSoundSys *self, byte flags) {
	self->vftable = CSOUNDSYS_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CSoundSys));
	}
	return self;
}

/* the functions without code */
void Sound_Nop(void) {
}

void CSoundSys_Nop(CSoundSys *self) {
	(void)self;
}

void CSoundSys_Nop2(CSoundSys *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
}

/* vtable slot 0x70 called directly, a tail jump in the original */
void CSoundSys_CallVf1c(CSoundSys *self) {
	self->vftable->vf1c(self);
}

void CSoundSys_InitFmod(CSoundSys *self) {
	(void)self;
	Fmod_Init();
}

void CSoundSys_Close(CSoundSys *self) {
	self->vftable->vf1c(self);
	Fmod_Close();
}

void CSoundSys_ResetListeners(CSoundSys *self) {
	self->lastListener = -1;
}

void CSoundSys_SetListener(CSoundSys *self, int id, const vec3_t *pos, const vec3_t *ang,
	const vec3_t *vel) {
	CHECK(id >= 0 && id < SOUND_MAX_LISTENERS);
	self->lastListener = id;
	cSoundListener *l = &self->listeners[id];
	l->pos = *pos;
	l->ang = *ang;
	l->vel = *vel;
	Float_SinCos(-l->ang.x, &l->sin.x, &l->cos.x);
	Float_SinCos(-l->ang.y, &l->sin.y, &l->cos.y);
	Float_SinCos(-l->ang.z, &l->sin.z, &l->cos.z);
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

void CSoundSys_Compute3D(CSoundSys *self, cSoundDesc *d, const vec3_t *pos, const vec3_t *vel,
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
void CSoundSys_SetVolume(CSoundSys *self, const char *name, int volume) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.volume = volume;
		self->vftable->SetDesc(self, name, &d);
	}
}

void CSoundSys_SetPan(CSoundSys *self, const char *name, int pan) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.pan = pan;
		self->vftable->SetDesc(self, name, &d);
	}
}

void CSoundSys_SetFreq(CSoundSys *self, const char *name, float freq) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.freq = freq;
		self->vftable->SetDesc(self, name, &d);
	}
}

void CSoundSys_SetLoop(CSoundSys *self, const char *name, byte loop) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.loop = loop;
		self->vftable->SetDesc(self, name, &d);
	}
}

void CSoundSys_SetPrio(CSoundSys *self, const char *name, byte prio) {
	cSoundDesc d;

	cSoundDesc_Init(&d);
	if (self->vftable->GetDesc(self, name, &d)) {
		d.prio = prio;
		self->vftable->SetDesc(self, name, &d);
	}
}

void CSoundSys_GetSoundName(CSoundSys *self, const char *file, char *out, int size) {
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
void CSoundSys_SetMusic(CSoundSys *self, const char *name) {
	(void)self;
	Music_Play(name, "play", 0.0f);
}

void CSoundSys_SetMusicVolume(CSoundSys *self, float volume) {
	(void)self;
	Music_Play(NULL, "volume", volume);
}

void CSoundSys_StopMusic(CSoundSys *self) {
	(void)self;
	Music_Play(NULL, "stop", 0.0f);
}

/* the voices, also ScrapEngMain.cpp */
int CSoundSys_OpenVoice(CSoundSys *self, const char *name, int voice, int arg) {
	(void)self;
	return Voice_Open(name, voice, arg);
}

int CSoundSys_PlayVoice(CSoundSys *self, int voice) {
	(void)self;
	return Voice_Play(voice);
}

int CSoundSys_StopVoice(CSoundSys *self, int voice) {
	(void)self;
	return Voice_Stop(voice);
}

byte CSoundSys_VoiceRemain(CSoundSys *self, int voice, float *remain) {
	(void)self;
	return Voice_Remain(voice, remain);
}

int CSoundSys_SetVoiceVolume(CSoundSys *self, int voice, float volume) {
	(void)self;
	return Voice_SetVolume(voice, volume);
}

int CSoundSys_SetVoiceParam(CSoundSys *self, int voice, float v) {
	(void)self;
	return Voice_SetParam(voice, v);
}

int CSoundSys_SetVoiceString(CSoundSys *self, int voice, const char *text) {
	(void)self;
	return Voice_SetString(voice, text);
}

int CSoundSys_GetVoiceString(CSoundSys *self, int voice, char *buf, int size) {
	(void)self;
	return Voice_GetString(voice, buf, size);
}

int CSoundSys_VoiceUnused(CSoundSys *self, int voice) {
	(void)self;
	return Voice_Unused(voice);
}

/* the vtable of CSoundSys */
const CSoundSysVtbl CSoundSys_vftable = {
	.Destroy = CSoundSys_Destroy,
	.Init = CSoundSys_InitFmod,
	.Close = CSoundSys_Close,
	.Update = CSoundSys_Nop,
	.LoadSound2 = (void *)crt_purecall,
	.LoadSound = (void *)crt_purecall,
	.DeleteSound = (void *)crt_purecall,
	.StopAllSounds = (void *)crt_purecall,
	.DeleteAllSounds = (void *)crt_purecall,
	.GetDesc = (void *)crt_purecall,
	.SetDesc = (void *)crt_purecall,
	.PlaySound = (void *)crt_purecall,
	.PlayName = (void *)crt_purecall,
	.Play = (void *)crt_purecall,
	.PlayAt = (void *)crt_purecall,
	.UpdateSound = (void *)crt_purecall,
	.StopSound = (void *)crt_purecall,
	.SetVolume = CSoundSys_SetVolume,
	.SetPan = CSoundSys_SetPan,
	.SetFreq = CSoundSys_SetFreq,
	.SetLoop = CSoundSys_SetLoop,
	.SetPrio = CSoundSys_SetPrio,
	.ResetListeners = CSoundSys_ResetListeners,
	.SetListener = CSoundSys_SetListener,
	.Compute3D = CSoundSys_Compute3D,
	.SetMusic = CSoundSys_SetMusic,
	.vf1a = CSoundSys_Nop,
	.StopMusic = CSoundSys_StopMusic,
	.vf1c = CSoundSys_Nop,
	.SetMusicVolume = CSoundSys_SetMusicVolume,
	.GetSoundName = CSoundSys_GetSoundName,
	.DebugDraw = CSoundSys_Nop2,
	.OpenVoice = CSoundSys_OpenVoice,
	.PlayVoice = CSoundSys_PlayVoice,
	.StopVoice = CSoundSys_StopVoice,
	.VoiceRemain = CSoundSys_VoiceRemain,
	.SetVoiceVolume = CSoundSys_SetVoiceVolume,
	.SetVoiceParam = CSoundSys_SetVoiceParam,
	.SetVoiceString = CSoundSys_SetVoiceString,
	.GetVoiceString = CSoundSys_GetVoiceString,
	.VoiceUnused = CSoundSys_VoiceUnused,
};

cGVar GVar_SoundDopplerLevel;
cGVar GVar_SoundShowSetMode;
cGVar GVar_SoundPanningFactor;
cGVar GVar_SoundDistAttenuation;
cGVar GVar_SoundAutoplayVolume;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_SoundAutoplayVolume, &GVar_SoundAutoplayVolume, "SoundAutoplayVolume", 0, "Sound autoplay volume (dB)\n", -60.0f)
INIT_GVAR_FLOAT(Init_GVar_SoundDopplerLevel, &GVar_SoundDopplerLevel, "SoundDopplerLevel", 0, "Sound doppler effect level", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_SoundDistAttenuation, &GVar_SoundDistAttenuation, "SoundDistAttenuation", 0, "Sound distance attenuation factor", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_SoundPanningFactor, &GVar_SoundPanningFactor, "SoundPanningFactor", 0, "Sound panning factor (dB)", 12.0f)
INIT_GVAR_INT(Init_GVar_SoundShowSetMode, &GVar_SoundShowSetMode, "SoundShowSetMode", 0, "Sound show SetMode info", 0)
