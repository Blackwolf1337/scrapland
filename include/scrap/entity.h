/*
 * Entities of the world (original source: Entities/). The world keeps every entity by name in
 * g_pWorld->entities.
 */
#ifndef SCRAP_ENTITY_H
#define SCRAP_ENTITY_H

#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/nto1links.h"
#include "scrap/pyfunction.h"

/* entity type names for IsType (the TYEN_ macros of the original) */
#define TYEN_WALKCHAR "WalkChar"
#define TYEN_WITH_LIFE "WithLife"
#define TYEN_CAR "Car"

typedef struct cEntity cEntity;

/* the first slots of cEntity::vftable; Ghidra names the rest cEntity::vfNN */
typedef struct cEntityVtbl {
	void(*Update)(cEntity *self);				/* 0x00 vf00 */
	void(*Render)(cEntity *self, void *arg);			/* 0x04 vf01 */
	void(*UpdateOff)(cEntity *self);				/* 0x08 vf02 */
	cEntity *(*Delete)(cEntity *self, uint32_t flags);		/* 0x0c vf03 deleting destructor */
	const char *(*GetClassName)(cEntity *self);		/* 0x10 vf04 "Entity", "Car", ... */
	byte(*IsType)(cEntity *self, const char *type);		/* 0x14 vf05 */
	vec3_t *(*GetPos)(cEntity *self, vec3_t *out);		/* 0x18 vf06, returns out */
	void(*SetPos)(cEntity *self, const vec3_t *pos);		/* 0x1c vf07 */
	void(*SetActive)(cEntity *self, byte on);			/* 0x20 vf08 active */
	/* the tests of the collision sphere (no hit in cEntity) and the touch of another entity */
	byte(*TestLine)(cEntity *self, const vec3_t *from, vec3_t *hit);		/* 0x24 vf09 */
	byte(*TestSphere)(cEntity *self, cEntity *exclude, const vec3_t *center, float radius,
		const vec3_t *prev);							/* 0x28 vf10 */
	void(*Touch)(cEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal);	/* 0x2c vf11 */
	void(*SetModel)(cEntity *self, const char *file, float sx, float sy, float sz, int flag); /* 0x30 */
	/* angle to the target in ang (in: x, y; out: x, y), target found in *target */
	void(*GetTargetAngle)(cEntity *self, float *ang, float dist, cEntity **target);	/* 0x34 vf13 */
	/* aim of the weapon: *dist in/out, out = aim point, *target the entity hit; last argument 8 from SDK */
	void(*AimWeapon)(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
		int flags);							/* 0x38 vf14 */
	void(*SetEmbeddedFX)(cEntity *self, const char *fxType);	/* 0x3c vf15 */
	/* script variables of the entity by name (cPyEntity attributes): set 0 reads into the pointers, 1 writes */
	byte(*VarI)(cEntity *self, const char *name, int *i, int set);			/* 0x40 vf16 */
	byte(*VarF)(cEntity *self, const char *name, double *d, int set);			/* 0x44 vf17 */
	byte(*VarS)(cEntity *self, const char *name, char **s, int set);			/* 0x48 vf18 */
	byte(*VarV)(cEntity *self, const char *name, double *x, double *y, double *z, int set); /* 0x4c */
	void(*Unused20)(cEntity *self);				/* 0x50 vf20, nothing and never called */
	int(*Unused21)(cEntity *self, int a);			/* 0x54 vf21, 0 and never called */
	void *(*GetAI)(cEntity *self);				/* 0x58 vf22 */
} cEntityVtbl;

/* calls through the slots of cEntityVtbl */
static inline void Entity_AimWeapon(void *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target, int flags) {
	VT(cEntityVtbl, self)->AimWeapon(self, ax, ay, dist, out, target, flags);
}

static inline cEntity *Entity_Delete(void *self, uint32_t flags) {
	return VT(cEntityVtbl, self)->Delete(self, flags);
}

static inline void *Entity_GetAI(void *self) {
	return VT(cEntityVtbl, self)->GetAI(self);
}

static inline const char *Entity_GetClassName(void *self) {
	return VT(cEntityVtbl, self)->GetClassName(self);
}

static inline vec3_t *Entity_GetPos(void *self, vec3_t *out) {
	return VT(cEntityVtbl, self)->GetPos(self, out);
}

static inline void Entity_GetTargetAngle(void *self, float *ang, float dist, cEntity **target) {
	VT(cEntityVtbl, self)->GetTargetAngle(self, ang, dist, target);
}

static inline byte Entity_IsType(void *self, const char *type) {
	return VT(cEntityVtbl, self)->IsType(self, type);
}

static inline void Entity_SetActive(void *self, byte on) {
	VT(cEntityVtbl, self)->SetActive(self, on);
}

static inline void Entity_SetEmbeddedFX(void *self, const char *fxType) {
	VT(cEntityVtbl, self)->SetEmbeddedFX(self, fxType);
}

static inline void Entity_SetModel(void *self, const char *file, float sx, float sy, float sz, int flag) {
	VT(cEntityVtbl, self)->SetModel(self, file, sx, sy, sz, flag);
}

static inline void Entity_SetPos(void *self, const vec3_t *pos) {
	VT(cEntityVtbl, self)->SetPos(self, pos);
}

static inline byte Entity_TestLine(void *self, const vec3_t *from, vec3_t *hit) {
	return VT(cEntityVtbl, self)->TestLine(self, from, hit);
}

static inline byte Entity_TestSphere(void *self, cEntity *exclude, const vec3_t *center, float radius, const vec3_t *prev) {
	return VT(cEntityVtbl, self)->TestSphere(self, exclude, center, radius, prev);
}

static inline void Entity_Touch(void *self, cEntity *e, const vec3_t *point, const vec3_t *normal) {
	VT(cEntityVtbl, self)->Touch(self, e, point, normal);
}

static inline void Entity_Update(void *self) {
	VT(cEntityVtbl, self)->Update(self);
}

static inline void Entity_UpdateOff(void *self) {
	VT(cEntityVtbl, self)->UpdateOff(self);
}

static inline byte Entity_VarF(void *self, const char *name, double *d, int set) {
	return VT(cEntityVtbl, self)->VarF(self, name, d, set);
}

static inline byte Entity_VarI(void *self, const char *name, int *i, int set) {
	return VT(cEntityVtbl, self)->VarI(self, name, i, set);
}

static inline byte Entity_VarS(void *self, const char *name, char **s, int set) {
	return VT(cEntityVtbl, self)->VarS(self, name, s, set);
}

static inline byte Entity_VarV(void *self, const char *name, double *x, double *y, double *z, int set) {
	return VT(cEntityVtbl, self)->VarV(self, name, x, y, z, set);
}

/* Nto1Links.h with a shared count: the head (data NULL) owns the int, every linked node points to it */
typedef struct EntityDNode {
	Nto1Node node;			/* 0x00 */
	int *numElems;			/* 0x10 */
} EntityDNode;

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

/* a vehicle ("Car"): include/scrap/vehicleentity.h */
typedef struct cVehicleEntity cVehicleEntity;

/* Global/DList.h: node of the entity lists of the world; the list ends at a node without data */
typedef struct DListNode {
	struct DListNode *next;		/* 0x0 */
	struct DListNode *prev;		/* 0x4 */
	void *target;			/* 0x8 */
	cEntity *data;			/* 0xc */
} DListNode;

/* an entity list of g_pWorld->lists (CollisionGrid.c makes them) */
typedef struct cEntityList {
	char *name;			/* 0x00 string object */
	cPy_Function onDeath;		/* 0x04 */
	cPy_Function onDamage;		/* 0x18 */
	int (*func)(void);		/* 0x2c returns 0 */
	union {
		Nto1Node ring;		/* 0x30 head of the entities (data), target the list */
		DListNode *head;	/* 0x30 ring.next, the first node */
	};
	int *numElems;			/* 0x40 */
} cEntityList;

/* list of the world by name ("Traffic", "Enemies", ...), rva 0x209240 */
cEntityList *cWorld_GetList(cWorld *world, const char *name);
/* first entity of the list or NULL, rva 0x1a7bc0 */
cEntity *cEntityList_GetFirst(cEntityList *list);

/* links node to the entity; it leaves any ring it was in first */
void cEntity_AddLink(cEntity *self, Nto1Node *node);

/* removes the entity; 0 if it cannot */
byte cEntity_Del(cEntity *self);

/* Entities/Entity.cpp; cEntity_ctor, cEntity_Next and EntityDList_dtor are declared in world.h */
void Nto1Node_dtor(Nto1Node *node);
void cEntity_dtor(cEntity *self);
/* the mask to savedMask (no collisions) and back */
void cEntity_CollisionOff(cEntity *self);
/* node linked to the entity src is linked to, or unlinked */
void Nto1Node_LinkAs(Nto1Node *node, const Nto1Node *src);
/* node linked to the entity of that name, or unlinked */
void Nto1Node_LinkByName(Nto1Node *node, const char *name);
/* the name of the entity node is linked to, "" for none */
const char *Nto1Node_TargetName(const Nto1Node *node);
/* the name of the entity list, NULL for none */
char *cEntity_GetListName(cEntity *self);
/* into the entity list of that name (NULL or "": none), which also makes the local logic; 0 if no such list */
byte cEntity_SetList(cEntity *self, const char *list);
/* the cEntity vtable slots */
void cEntity_Update(cEntity *self);
void cEntity_Render(cEntity *self, void *arg);
void cEntity_UpdateOff(cEntity *self);
byte cEntity_IsType(cEntity *self, const char *type);
vec3_t *cEntity_GetPos(cEntity *self, vec3_t *out);
void cEntity_SetPos(cEntity *self, const vec3_t *pos);
void cEntity_SetActive(cEntity *self, byte on);
void cEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag);
void cEntity_GetTargetAngle(cEntity *self, float *ang, float dist, cEntity **target);
void cEntity_AimWeapon(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
	int flags);
void cEntity_SetEmbeddedFX(cEntity *self, const char *fxType);
byte cEntity_VarI(cEntity *self, const char *name, int *i, int set);
byte cEntity_VarF(cEntity *self, const char *name, double *d, int set);
byte cEntity_VarS(cEntity *self, const char *name, char **s, int set);
byte cEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set);
void *cEntity_GetAI(cEntity *self);
/* the inline virtual functions of Entity.h: vf09 and vf10 (no hit), vf11, vf20 (nothing) and vf21 (0) */
byte cEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit);
void cEntity_Unused20(cEntity *self);
int cEntity_Unused21(cEntity *self, int a);
byte cEntity_NetNeedsSendTrue(cEntity *self, float dist2);
void cEntity_NetReadNop(cEntity *self, void *msg, int isNew);
void cEntity_NetWriteNop(cEntity *self, float dist2, void *msg, int full);
byte cEntity_TestSphereFalse(cEntity *self, cEntity *exclude, const vec3_t *center, float radius, const vec3_t *prev);
void cEntity_TouchNop(cEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal);

/* the network game: an entity named "net..." of a client is the copy of a server one */
byte Net_IsNetEntity(cEntity *e);

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
