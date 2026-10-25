/*
 * Entities of the world (original source: Entities/). The world keeps every entity by name in
 * g_pWorld->entities.
 */
#ifndef SCRAP_ENTITY_H
#define SCRAP_ENTITY_H

#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/nto1links.h"

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
	/* script variables of the entity by name (cPyEntity attributes): set 0 reads into the pointers, 1 writes */
	byte(__thiscall *VarI)(cEntity *self, const char *name, int *i, int set);			/* 0x40 vf16 */
	byte(__thiscall *VarF)(cEntity *self, const char *name, double *d, int set);			/* 0x44 vf17 */
	byte(__thiscall *VarS)(cEntity *self, const char *name, char **s, int set);			/* 0x48 vf18 */
	byte(__thiscall *VarV)(cEntity *self, const char *name, double *x, double *y, double *z, int set); /* 0x4c */
	void *vf20_21[2];
	void *(__thiscall *GetAI)(cEntity *self);				/* 0x58 vf22 */
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
SD_OFFSET(cEntityVtbl, VarI, 0x40);
SD_OFFSET(cEntityVtbl, VarF, 0x44);
SD_OFFSET(cEntityVtbl, VarS, 0x48);
SD_OFFSET(cEntityVtbl, VarV, 0x4c);
SD_OFFSET(cEntityVtbl, GetAI, 0x58);

struct cEntity {
	const cEntityVtbl *vftable;	/* 0x00 */
	char *name;			/* 0x04 */
	byte pad_08[0x4c - 0x08];
	byte flag_4c;			/* 0x4c checked by the SAI functions before they touch the AI */
	byte pad_4d[0x64 - 0x4d];
	Nto1Node links;			/* 0x64 head of the script handles (cPyEntity), target the entity */
};
SD_OFFSET(cEntity, name, 0x4);
SD_OFFSET(cEntity, flag_4c, 0x4c);
SD_OFFSET(cEntity, links, 0x64);
SD_SIZE(cEntity, 0x74);

/* a vehicle ("Car"); only the fields used so far */
typedef struct cVehicleEntity {
	cEntity base;
	byte pad_74[0x238 - 0x74];
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

/* links node to the entity (rva 0xc9a20); it leaves any ring it was in first */
void __thiscall cEntity_AddLink(cEntity *self, Nto1Node *node);

/* removes the entity; 0 if it cannot (rva 0xef0c0) */
byte __thiscall cEntity_Del(cEntity *self);

/* vehicles ("Car") and characters ("WalkChar") continue the vtable of cWithLifeEntity */
typedef struct cVehicleEntityVtbl {
	cWithLifeEntityVtbl base;
	void *vf54_57[4];
	void(__thiscall *EnableAI)(cEntity *self, byte enable, void *p1, void *p2, void *p3);	/* 0xe8 vf58 */
} cVehicleEntityVtbl;
SD_OFFSET(cVehicleEntityVtbl, EnableAI, 0xe8);

typedef struct cWalkCharEntityVtbl {
	cWithLifeEntityVtbl base;
	void *vf54_55[2];
	void(__thiscall *EnableAI)(cEntity *self, byte enable, int *arg);	/* 0xe0 vf56 */
} cWalkCharEntityVtbl;
SD_OFFSET(cWalkCharEntityVtbl, EnableAI, 0xe0);

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
