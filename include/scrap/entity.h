/*
 * Entities of the world (original source: Entities/). The world keeps every entity by name in
 * g_pWorld->entities.
 */
#ifndef SCRAP_ENTITY_H
#define SCRAP_ENTITY_H

#include "scrap/game.h"
#include "scrap/mathlib.h"

/* entity type names for IsType (the TYEN_ macros of the original) */
#define TYEN_WALKCHAR "WalkChar"
#define TYEN_WITH_LIFE "WithLife"

typedef struct cEntity cEntity;

/* the first slots of cEntity::vftable; Ghidra names the rest cEntity::vfNN */
typedef struct cEntityVtbl {
	void *vf00_04[5];
	byte(__thiscall *IsType)(cEntity *self, const char *type);		/* 0x14 vf05 */
	void *vf06_11[6];
	void(__thiscall *SetModel)(cEntity *self, const char *file, float sx, float sy, float sz, int flag); /* 0x30 */
	/* angle to the target in ang (in: x, y; out: x, y), target found in *target */
	void(__thiscall *GetTargetAngle)(cEntity *self, float *ang, float dist, cEntity **target);	/* 0x34 vf13 */
	/* aim of the weapon: *dist in/out, out = aim point, *target the entity hit; last argument 8 from SDK */
	void(__thiscall *AimWeapon)(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
		int flags);							/* 0x38 vf14 */
	void(__thiscall *SetEmbeddedFX)(cEntity *self, const char *fxType);	/* 0x3c vf15 */
} cEntityVtbl;
SD_OFFSET(cEntityVtbl, IsType, 0x14);
SD_OFFSET(cEntityVtbl, SetModel, 0x30);
SD_OFFSET(cEntityVtbl, GetTargetAngle, 0x34);
SD_OFFSET(cEntityVtbl, AimWeapon, 0x38);
SD_OFFSET(cEntityVtbl, SetEmbeddedFX, 0x3c);

struct cEntity {
	const cEntityVtbl *vftable;	/* 0x0 */
	char *name;			/* 0x4 */
};
SD_OFFSET(cEntity, name, 0x4);

/* removes the entity; 0 if it cannot (rva 0xef0c0) */
byte __thiscall cEntity_Del(cEntity *self);

static inline int Entity_IsType(cEntity *e, const char *type) {
	return e->vftable->IsType(e, type) != 0;
}

/* the entity with that name or NULL; inlined everywhere in the original */
static inline cEntity *World_FindEntity(const char *name) {
	if (!*name) {
		return NULL;
	}
	HashTable *t = &g_pWorld->entities;
	HashNode *node = HashTable_Find(t, name, HashTable_Hash(t, name));
	return node ? node->value : NULL;
}

#endif
