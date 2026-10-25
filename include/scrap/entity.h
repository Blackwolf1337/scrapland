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
#define TYEN_CAR "Car"

typedef struct cEntity cEntity;

/* the first slots of cEntity::vftable; Ghidra names the rest cEntity::vfNN */
typedef struct cEntityVtbl {
	void *vf00_04[5];
	byte(__thiscall *IsType)(cEntity *self, const char *type);		/* 0x14 vf05 */
	vec3_t *(__thiscall *GetPos)(cEntity *self, vec3_t *out);		/* 0x18 vf06, returns out */
	void *vf07_11[5];
	void(__thiscall *SetModel)(cEntity *self, const char *file, float sx, float sy, float sz, int flag); /* 0x30 */
	/* angle to the target in ang (in: x, y; out: x, y), target found in *target */
	void(__thiscall *GetTargetAngle)(cEntity *self, float *ang, float dist, cEntity **target);	/* 0x34 vf13 */
	/* aim of the weapon: *dist in/out, out = aim point, *target the entity hit; last argument 8 from SDK */
	void(__thiscall *AimWeapon)(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
		int flags);							/* 0x38 vf14 */
	void(__thiscall *SetEmbeddedFX)(cEntity *self, const char *fxType);	/* 0x3c vf15 */
	void *vf16_22[7];
} cEntityVtbl;
SD_SIZE(cEntityVtbl, 23 * 4);

/* entities with life (characters derive from it): cWithLifeEntity::vftable continues the one of cEntity */
typedef struct cWithLifeEntityVtbl {
	cEntityVtbl base;
	void *vf23_52[30];
	void *(__thiscall *GetActCtrl)(cEntity *self);				/* 0xd4 vf53 */
} cWithLifeEntityVtbl;
SD_OFFSET(cWithLifeEntityVtbl, GetActCtrl, 0xd4);
SD_OFFSET(cEntityVtbl, IsType, 0x14);
SD_OFFSET(cEntityVtbl, GetPos, 0x18);
SD_OFFSET(cEntityVtbl, SetModel, 0x30);
SD_OFFSET(cEntityVtbl, GetTargetAngle, 0x34);
SD_OFFSET(cEntityVtbl, AimWeapon, 0x38);
SD_OFFSET(cEntityVtbl, SetEmbeddedFX, 0x3c);

struct cEntity {
	const cEntityVtbl *vftable;	/* 0x0 */
	char *name;			/* 0x4 */
};
SD_OFFSET(cEntity, name, 0x4);

/* a vehicle ("Car"); only the fields used so far */
typedef struct cVehicleEntity {
	cEntity base;
	byte pad_8[0x238 - 0x8];
	int life;			/* 0x238 > 0 while the ship is alive (SLogic.GetNearestShip) */
} cVehicleEntity;
SD_OFFSET(cVehicleEntity, life, 0x238);

/* Global/DList.h: node of the entity lists of the world; the list ends at a node without data */
typedef struct DListNode {
	struct DListNode *next;		/* 0x0 */
	byte pad_4[0x8];
	cEntity *data;			/* 0xc */
} DListNode;

typedef struct cEntityList {
	byte pad_0[0x30];
	DListNode *head;		/* 0x30 */
} cEntityList;
SD_OFFSET(cEntityList, head, 0x30);

/* list of the world by name ("Traffic", "Enemies", ...), rva 0x209240 */
cEntityList *__thiscall cWorld_GetList(cWorld *world, const char *name);
/* first entity of the list or NULL, rva 0x1a7bc0 */
cEntity *__thiscall cEntityList_GetFirst(cEntityList *list);

/* removes the entity; 0 if it cannot (rva 0xef0c0) */
byte __thiscall cEntity_Del(cEntity *self);

static inline const cWithLifeEntityVtbl *WithLife_Vtbl(cEntity *e) {
	return (const cWithLifeEntityVtbl *)e->vftable;
}

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
