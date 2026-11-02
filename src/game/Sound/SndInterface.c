/*
 * Sound/SndInterface.cpp of the game: cSndInterface, the numbered sounds of an entity that its scripts set through
 * entity variables, and their update at the entity's position. See include/scrap/sound.h.
 */
#include <string.h>

#include "scrap/objentity.h"
#include "scrap/sound.h"
#include "scrap/world.h"

/* a field of a sound by the character of an "xxx%c" variable, which the original does not check */
#define SndItem_Field(it, type, c, base) ((type *)((byte *)(it) + (c) * 4 - (base)))

void cSndItem_Clear(cSndItem *self) {
	self->volume = 0.0f;
	self->pitch = 0.0f;
	self->time = 0.0f;
	for (int i = 0; i < 5; i++) {
		self->vols[i] = 0.0f;
		self->pitches[i] = 0.0f;
		self->times[i] = 0.0f;
		self->ints[i] = 0;
	}
	self->playing = 0;
}

cSndItem *cSndItem_ctorName(cSndItem *self, const char *name, uint32_t flags) {
	cSound_ctor(&self->sound, name, flags);
	cSndItem_Clear(self);
	return self;
}

cSndItem *cSndItem_ctor(cSndItem *self) {
	cSound_Init(&self->sound);
	cSndItem_Clear(self);
	return self;
}

/* at the entity's position with its velocity */
void cSndItem_UpdateAt(cSndItem *self, cEntity *e) {
	vec3_t vel, pos;

	const vec3_t *pv = Obj_Vtbl(e)->GetVel(e, &vel);
	const vec3_t *pp = Obj_Vtbl(e)->GetSndPos(e, &pos);
	cSound_SetPosVel(&self->sound, pp, pv);
	self->sound.desc.volume += Snd_Volume(self->volume);
	cSound_SetPitch(&self->sound, self->pitch);
	self->playing = cSound_Update(&self->sound);
}

byte cSndItem_Play(cSndItem *self, cEntity *e) {
	cSndItem_UpdateAt(self, e);
	self->playing = cSound_Play(&self->sound);
	return self->playing;
}

void cSndItem_Restart(cSndItem *self) {
	self->playing = cSound_Play(&self->sound);
}

/* 0 stops it, else it starts when it does not play */
void cSndItem_SetVolume(cSndItem *self, cEntity *e, float volume) {
	self->volume = volume;
	if (volume == 0.0f) {
		cSound_Stop(&self->sound);
		self->playing = 0;
		return;
	}
	if (!self->playing) {
		cSndItem_Play(self, e);
	}
}

void cSndItem_Tick(cSndItem *self, cEntity *e, float dt) {
	if (self->volume == 0.0f) {
		if (self->playing) {
			self->playing = 0;
			cSound_Stop(&self->sound);
		}
	} else if (self->playing || self->sound.desc.loop) {
		cSndItem_UpdateAt(self, e);
	}
	self->time = dt + self->time;
}

/* cSndInterface */

void cSndInterface_Alloc(cSndInterface *self, int n) {
	CHECK(n > 0);
	self->nsnd = n;
	self->snds = Mem_Alloc(n * sizeof(*self->snds));
	memset(self->snds, 0, self->nsnd * sizeof(*self->snds));
}

void cSndInterface_Free(cSndInterface *self) {
	for (int i = 0; i < self->nsnd; i++) {
		cSndItem *it = self->snds[i];
		if (it) {
			cSound_Stop(&it->sound);
			operator_delete_sized(it, sizeof(cSndItem));
		}
	}
	Mem_Free(self->snds);
}

cSndInterface *cSndInterface_ctor(cSndInterface *self, int n) {
	self->vftable = cSndInterface_vftable;
	if (g_SndInterfaces) {
		g_SndInterfaces->link = &self->next;
	}
	self->next = g_SndInterfaces;
	self->link = &g_SndInterfaces;
	g_SndInterfaces = self;
	cSndInterface_Alloc(self, n);
	memcpy(self->id, "snd", 4);
	return self;
}

void cSndInterface_dtor(cSndInterface *self) {
	self->vftable = cSndInterface_vftable;
	if (self->next) {
		self->next->link = self->link;
	}
	if (self->link) {
		*self->link = self->next;
	}
	cSndInterface_Free(self);
}

cSndInterface *cSndInterface_Destroy(cSndInterface *self, byte flags) {
	cSndInterface_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cSndInterface));
	}
	return self;
}

void cSndInterface_Resize(cSndInterface *self, int n) {
	if (n != self->nsnd) {
		cSndInterface_Free(self);
		cSndInterface_Alloc(self, n);
	}
}

/* item i of a pointer array { int pad; int n; void **items } (shared with other classes), NULL outside */
void *PtrArray_Get(void *arr, int i) {
	cSndInterface *self = arr;

	if (i >= 0 && i < self->nsnd) {
		return self->snds[i];
	}
	return NULL;
}

void cSndInterface_StopAll(cSndInterface *self, int unused) {
	(void)unused;
	for (int i = 0; i < self->nsnd; i++) {
		cSndItem *it = PtrArray_Get(self, i);
		if (it && it->sound.desc.loop) {
			cSound_Stop(&it->sound);
		}
	}
}

void cSndInterface_SetId(cSndInterface *self, const char *id) {
	CHECK(strlen(id) == 3);
	memcpy(self->id, id, 4);
}

/* sound i plays name (a sound name), none for NULL or "" */
void cSndInterface_SetSound(cSndInterface *self, int i, const char *name) {
	CHECK(i >= 0);
	CHECK(i < self->nsnd);
	cSndItem *it = self->snds[i];
	if (it) {
		cSound_Stop(&it->sound);
		operator_delete_sized(it, sizeof(cSndItem));
	}
	if (name && *name) {
		it = operator_new(sizeof(cSndItem));
		self->snds[i] = it ? cSndItem_ctorName(it, name, 4) : NULL;
		return;
	}
	self->snds[i] = NULL;
}

void cSndInterface_Tick(cSndInterface *self, float dt) {
	for (int i = 0; i < self->nsnd; i++) {
		cSndItem *it = PtrArray_Get(self, i);
		if (it) {
			cSndItem_Tick(it, self->owner, dt);
		}
	}
}

/* "<id><nn>_<rest>": the sound number and the rest */
byte cSndInterface_ParseName(cSndInterface *self, const char *name, int *idx, const char **rest) {
	char num[3];

	if ((int)strlen(name) < 6 || name[5] != '_' || memcmp(self->id, name, 2) || self->id[2] != name[2]) {
		return 0;
	}
	num[0] = name[3];
	num[1] = name[4];
	num[2] = 0;
	if ((byte)(num[0] - '0') > 9 || (byte)(num[1] - '0') > 9) {
		return 0;
	}
	int n = crt_atoi(num);
	*idx = n;
	if (n >= self->nsnd) {
		return 0;
	}
	*rest = name + 6;
	return 1;
}

/* "<id>_n" the count, "loop", "int0".."int4" */
byte cSndInterface_IntVar(cSndInterface *self, const char *name, uint32_t *v, byte set) {
	char count[6];
	const char *rest;
	int idx;

	count[0] = self->id[0];
	count[1] = self->id[1];
	count[2] = self->id[2];
	count[3] = '_';
	count[4] = 'n';
	count[5] = 0;
	if (!strcmp(name, count)) {
		if (set) {
			cSndInterface_Resize(self, (int)*v);
		} else {
			*v = (uint32_t)self->nsnd;
		}
		return 1;
	}
	cSndItem *it;
	if (!cSndInterface_ParseName(self, name, &idx, &rest) || !(it = PtrArray_Get(self, idx))) {
		return 0;
	}
	if (!strcmp(rest, "loop")) {
		if (set) {
			it->sound.desc.loop = *v != 0;
		} else {
			*v = it->sound.desc.loop != 0;
		}
		return 1;
	}
	if (strlen(rest) == 4 && rest[0] == 'i' && rest[1] == 'n' && rest[2] == 't') {
		uint32_t *f = SndItem_Field(it, uint32_t, (signed char)rest[3], 0x38);
		if (set) {
			*f = *v;
		} else {
			*v = *f;
		}
		return 1;
	}
	return 0;
}

static byte SndItem_Float(float *f, double *v, byte set) {
	if (set) {
		*f = (float)*v;
	} else {
		*v = (double)*f;
	}
	return 1;
}

/* "ini", "end", "dopp", "vol", "time", "pitch" and "vol0".."vol4", "pitch0".."pitch4", "time0".."time4" */
byte cSndInterface_FloatVar(cSndInterface *self, const char *name, double *v, byte set) {
	const char *rest;
	int idx;

	cSndItem *it;
	if (!cSndInterface_ParseName(self, name, &idx, &rest) || !(it = PtrArray_Get(self, idx))) {
		return 0;
	}
	size_t len = strlen(rest);
	if (!strcmp(rest, "ini")) {
		return SndItem_Float(&it->sound.desc.param1, v, set);
	}
	if (!strcmp(rest, "end")) {
		return SndItem_Float(&it->sound.desc.param2, v, set);
	}
	if (!strcmp(rest, "dopp")) {
		return SndItem_Float(&it->sound.desc.param3, v, set);
	}
	if (!strcmp(rest, "vol")) {
		if (set) {
			cSndItem_SetVolume(it, self->owner, (float)*v);
		} else {
			*v = (double)it->volume;
		}
		return 1;
	}
	if (!strcmp(rest, "time")) {
		return SndItem_Float(&it->time, v, set);
	}
	if (!strcmp(rest, "pitch")) {
		return SndItem_Float(&it->pitch, v, set);
	}
	if (len == 4) {
		if (rest[0] != 'v' || rest[1] != 'o' || rest[2] != 'l') {
			return 0;
		}
		return SndItem_Float(SndItem_Field(it, float, (signed char)rest[3], 0x74), v, set);
	}
	if (len == 6) {
		if (memcmp(rest, "pitc", 4) || rest[4] != 'h') {
			return 0;
		}
		return SndItem_Float(SndItem_Field(it, float, (signed char)rest[5], 0x60), v, set);
	}
	if (len == 5 && !memcmp(rest, "time", 4)) {
		return SndItem_Float(SndItem_Field(it, float, (signed char)rest[4], 0x4c), v, set);
	}
	return 0;
}

/* "name": the sound name */
byte cSndInterface_StrVar(cSndInterface *self, const char *name, const char **v, byte set) {
	const char *rest;
	int idx;

	if (!cSndInterface_ParseName(self, name, &idx, &rest) || strcmp(rest, "name")) {
		return 0;
	}
	if (set) {
		cSndInterface_SetSound(self, idx, *v);
		return 1;
	}
	cSndItem *it = PtrArray_Get(self, idx);
	*v = it ? cSound_GetName(&it->sound) : GAME_STR(str_Empty);
	return 1;
}

/* vtable slot 0x10 */
byte cSndInterface_Null(cSndInterface *self, int a, int b, int c, int d, int e) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
	return 0;
}

/* while the game is paused: the looping sounds of all entities silent, the looping property sets stopped */
void Sound_UpdatePaused(void) {
	for (cSndInterface *s = g_SndInterfaces; s; s = s->next) {
		for (int i = 0; i < s->nsnd; i++) {
			cSndItem *it = PtrArray_Get(s, i);
			if (it && it->sound.desc.loop) {
				it->sound.desc.volume = Snd_Volume(0.0f);
				cSound_Update(&it->sound);
			}
		}
	}
	SndPS_StopAll();
}
