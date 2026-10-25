/*
 * Entities of the world (original source: Entities/). The world keeps every entity by name in
 * g_pWorld->entities.
 */
#ifndef SCRAP_ENTITY_H
#define SCRAP_ENTITY_H

#include "scrap/game.h"

/* entity type names for IsType (the TYEN_ macros of the original) */
#define TYEN_WALKCHAR "WalkChar"
#define TYEN_WITH_LIFE "WithLife"

typedef struct cEntity cEntity;

/* the first slots of cEntity::vftable; Ghidra names them cEntity::vfNN */
typedef struct cEntityVtbl {
	void *vf00_04[5];
	byte(__thiscall *IsType)(cEntity *self, const char *type);		/* 0x14 vf05 */
	void *vf06_14[9];
	void(__thiscall *SetEmbeddedFX)(cEntity *self, const char *fxType);	/* 0x3c vf15 */
} cEntityVtbl;
SD_OFFSET(cEntityVtbl, IsType, 0x14);
SD_OFFSET(cEntityVtbl, SetEmbeddedFX, 0x3c);

struct cEntity {
	const cEntityVtbl *vftable;
};

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
