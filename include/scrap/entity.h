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
	void(__thiscall *Update)(cEntity *self);				/* 0x00 vf00 */
	void(__thiscall *Render)(cEntity *self, void *arg);			/* 0x04 vf01 */
	void(__thiscall *UpdateOff)(cEntity *self);				/* 0x08 vf02 */
	cEntity *(__thiscall *Delete)(cEntity *self, uint32_t flags);		/* 0x0c vf03 deleting destructor */
	const char *(__thiscall *GetClassName)(cEntity *self);		/* 0x10 vf04 "Entity", "Car", ... */
	byte(__thiscall *IsType)(cEntity *self, const char *type);		/* 0x14 vf05 */
	vec3_t *(__thiscall *GetPos)(cEntity *self, vec3_t *out);		/* 0x18 vf06, returns out */
	void(__thiscall *SetPos)(cEntity *self, const vec3_t *pos);		/* 0x1c vf07 */
	void(__thiscall *SetActive)(cEntity *self, byte on);			/* 0x20 vf08 active */
	/* the tests of the collision sphere (no hit in cEntity) and the touch of another entity */
	byte(__thiscall *TestLine)(cEntity *self, const vec3_t *from, vec3_t *hit);		/* 0x24 vf09 */
	byte(__thiscall *TestSphere)(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
		const vec3_t *prev);							/* 0x28 vf10 */
	void(__thiscall *Touch)(cEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal);	/* 0x2c vf11 */
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

SD_OFFSET(cEntityVtbl, GetClassName, 0x10);
SD_OFFSET(cEntityVtbl, IsType, 0x14);
SD_OFFSET(cEntityVtbl, SetActive, 0x20);
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

/* Nto1Links.h with a shared count: the head (data NULL) owns the int, every linked node points to it */
typedef struct EntityDNode {
	Nto1Node node;			/* 0x00 */
	int *numElems;			/* 0x10 */
} EntityDNode;
SD_SIZE(EntityDNode, 0x14);

/* Entities/Entity.cpp (src/game/Entities/Entity.c), 0x7c bytes */
struct cEntity {
	const cEntityVtbl *vftable;	/* 0x00 */
	char *name;			/* 0x04 string object, unique in g_pWorld->entities */
	Nto1Node worldNode;		/* 0x08 in g_pWorld->entityList, data the entity */
	Nto1Node deadNode;		/* 0x18 in g_pWorld->deadList once deleted (cEntity_Del) */
	vec3_t pos;			/* 0x28 */
	Nto1Node gridNode;		/* 0x34 cell of the collision grid, data the entity */
	uint32_t savedMask;		/* 0x44 the mask while the collision is off (ActiveCMask 0) */
	uint32_t mask;			/* 0x48 the entity lists it is in, a bit each */
	byte active;			/* 0x4c "Active"; checked by the SAI functions before they touch the AI */
	byte pad_4d[3];
	EntityDNode listNode;		/* 0x50 in the ring of its cEntityListData ("EntityList"), target the list */
	Nto1Node links;			/* 0x64 head of the script handles (cPyEntity), target the entity */
	void *lLogic;			/* 0x74 local logic made by the list ("LLogic" variables) */
	void *ai;			/* 0x78 the AI (GetAI, "AI_" variables) */
};
SD_OFFSET(cEntity, name, 0x4);
SD_OFFSET(cEntity, pos, 0x28);
SD_OFFSET(cEntity, mask, 0x48);
SD_OFFSET(cEntity, active, 0x4c);
SD_OFFSET(cEntity, listNode, 0x50);
SD_OFFSET(cEntity, links, 0x64);
SD_OFFSET(cEntity, ai, 0x78);
SD_SIZE(cEntity, 0x7c);

#define CENTITY_VFTABLE GAME_VFTABLE(cEntity_vftable, 0x3b25f8)

/* a vehicle ("Car"): include/scrap/vehicleentity.h */
typedef struct cVehicleEntity cVehicleEntity;

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

/* Entities/Entity.cpp; cEntity_ctor, cEntity_Next and EntityDList_dtor are declared in world.h */
void __fastcall Nto1Node_dtor(Nto1Node *node);
void __fastcall cEntity_dtor(cEntity *self);
/* node joins the ring of head and counts in its numElems */
void __thiscall EntityDList_Add(void *head, EntityDNode *node);
/* the mask to savedMask (no collisions) and back */
void __fastcall cEntity_CollisionOff(cEntity *self);
void __fastcall cEntity_CollisionOn(cEntity *self);
/* node linked to the entity src is linked to, or unlinked */
void Nto1Node_LinkAs(Nto1Node *node, const Nto1Node *src);
/* node linked to the entity of that name, or unlinked */
void Nto1Node_LinkByName(Nto1Node *node, const char *name);
/* the name of the entity node is linked to, "" for none */
const char *Nto1Node_TargetName(const Nto1Node *node);
/* the name of the entity list, NULL for none */
char *__fastcall cEntity_GetListName(cEntity *self);
cEntity *__fastcall cEntity_NextInList(cEntity *self);
/* into the entity list of that name (NULL or "": none), which also makes the local logic; 0 if no such list */
byte __thiscall cEntity_SetList(cEntity *self, const char *list);
/* the cEntity vtable slots */
void __thiscall cEntity_Update(cEntity *self);
void __thiscall cEntity_Render(cEntity *self, void *arg);
void __thiscall cEntity_UpdateOff(cEntity *self);
cEntity *__thiscall cEntity_Delete(cEntity *self, uint32_t flags);
const char *__thiscall cEntity_GetClassName(cEntity *self);
int __thiscall cEntity_IsType(cEntity *self, const char *type);
vec3_t *__thiscall cEntity_GetPos(cEntity *self, vec3_t *out);
void __thiscall cEntity_SetPos(cEntity *self, const vec3_t *pos);
void __thiscall cEntity_SetActive(cEntity *self, byte on);
void __thiscall cEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag);
void __thiscall cEntity_GetTargetAngle(cEntity *self, float *ang, float dist, cEntity **target);
void __thiscall cEntity_AimWeapon(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
	int flags);
void __thiscall cEntity_SetEmbeddedFX(cEntity *self, const char *fxType);
byte __thiscall cEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte __thiscall cEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte __thiscall cEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte __thiscall cEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void *__thiscall cEntity_GetAI(cEntity *self);
/* the inline virtual functions of Entity.h: vf09 and vf10 (no hit), vf11, vf20 (nothing) and vf21 (0) */
byte __thiscall cEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit);
byte __thiscall cEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius, int arg);
void __thiscall cEntity_Vf11(cEntity *self, int a, int b, int c);
void __thiscall cEntity_Vf20(cEntity *self);
int __thiscall cEntity_Vf21(cEntity *self, int a);

/* the network game: an entity named "net..." of a client is the copy of a server one */
byte Net_IsNetEntity(cEntity *e);
int Net_GetSendRate(void);

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
