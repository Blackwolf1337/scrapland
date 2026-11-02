/*
 * Dynamic arrays of the engine3d, original source Util/Listadin.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

/* the inlined check at the start of every function */
static void DynList_Check(const cDynList *dyn) {
	CHECK(dyn);
	CHECK(dyn->id == DYNLIST_ID);
}

#define LDIN_ELEM(dyn, i) ((dyn)->mem + (dyn)->elemSize * (i))

void DynList_Free(cDynList *dyn) {
	DynList_Clear(dyn);
	dyn->id = 0;
	Mem_Free(dyn);
}

static cDynList *DynList_Alloc(uint16_t elemSize, uint16_t blockSize) {
	cDynList *dyn = Mem_Alloc(sizeof(*dyn));
	dyn->id = DYNLIST_ID;
	dyn->mem = NULL;
	dyn->total = 0;
	dyn->used = 0;
	dyn->elemSize = elemSize;
	dyn->blockSize = blockSize;
	dyn->changed = 0;
	dyn->unused17 = 0;
	return dyn;
}

cDynList *DynList_New(uint16_t elemSize) {
	return DynList_Alloc(elemSize, 0x100);
}

cDynList *DynList_NewBlock(uint16_t elemSize, uint16_t blockSize) {
	CHECK(blockSize > 0);
	return DynList_Alloc(elemSize, blockSize);
}

void *DynList_Get(cDynList *dyn, uint32_t i) {
	DynList_Check(dyn);
	CHECK(i < dyn->used);
	CHECK(dyn->mem);
	return LDIN_ELEM(dyn, i);
}

uint32_t DynList_Count(cDynList *dyn) {
	DynList_Check(dyn);
	return dyn->used;
}

void *DynList_Last(cDynList *dyn) {
	DynList_Check(dyn);
	CHECK(dyn->used > 0);
	CHECK(dyn->mem);
	return LDIN_ELEM(dyn, dyn->used - 1);
}

void *DynList_AddOne(cDynList *dyn) {
	return DynList_Add(dyn, 1);
}

void *DynList_Add(cDynList *dyn, int num) {
	DynList_Check(dyn);
	CHECK(num > 0);
	if (dyn->total < dyn->used + num) {
		uint32_t total = num - 1 + dyn->blockSize + dyn->used;
		dyn->total = total - total % dyn->blockSize;
		dyn->mem = Mem_Realloc(dyn->mem, dyn->elemSize * dyn->total);
	}
	uint32_t first = dyn->used;
	dyn->changed = 1;
	dyn->used = first + num;
	CHECK(dyn->used <= dyn->total);
	return LDIN_ELEM(dyn, first);
}

void DynList_Copy(cDynList *dyn, uint32_t orig, uint32_t dest, int num) {
	DynList_Check(dyn);
	CHECK(num >= 0);
	CHECK(orig < dyn->used && orig + num <= dyn->used);
	CHECK(dest < dyn->used && dest + num <= dyn->used);
	if (orig != dest && num) {
		dyn->changed = 1;
		memmove(LDIN_ELEM(dyn, dest), LDIN_ELEM(dyn, orig), dyn->elemSize * num);
	}
}

void DynList_Delete(cDynList *dyn, uint32_t i) {
	DynList_Check(dyn);
	CHECK(i < dyn->used);
	if (i != dyn->used - 1) {
		memmove(LDIN_ELEM(dyn, i), LDIN_ELEM(dyn, i + 1), (dyn->used - 1 - i) * dyn->elemSize);
	}
	DynList_RemoveLast(dyn);
}

/* frees whole blocks that are no longer used */
void DynList_RemoveLast(cDynList *dyn) {
	DynList_Check(dyn);
	if (!dyn->used) {
		return;
	}
	dyn->used--;
	dyn->changed = 1;
	uint32_t limit = dyn->blockSize + dyn->used;
	if (limit > dyn->total) {
		return;
	}
	uint32_t total = dyn->total;
	do {
		total -= dyn->blockSize;
	} while (limit <= total);
	dyn->total = total;
	if (dyn->elemSize * total != 0) {
		dyn->mem = Mem_Realloc(dyn->mem, dyn->elemSize * total);
		return;
	}
	Mem_Free(dyn->mem);
	dyn->mem = NULL;
	CHECK(!dyn->total && !dyn->used);
}

/* i == used fails the CHECK of DynList_Copy, in the original too */
void *DynList_Insert(cDynList *dyn, uint32_t i) {
	DynList_Check(dyn);
	CHECK(i <= dyn->used);
	DynList_Add(dyn, 1);
	DynList_Copy(dyn, i, i + 1, dyn->used - i - 1);
	return LDIN_ELEM(dyn, i);
}

void DynList_Move(cDynList *dyn, int orig, int dest) {
	DynList_Check(dyn);
	CHECK(dyn->used);
	if (orig < 0) {
		orig = dyn->used - 1;
	}
	if (dest < 0) {
		dest = dyn->used - 1;
	}
	CHECK((uint32_t)orig < dyn->used);
	CHECK((uint32_t)dest < dyn->used);
	if (orig == dest) {
		return;
	}
	dyn->changed = 1;
	void *tmp = Mem_Alloc(dyn->elemSize);
	memcpy(tmp, LDIN_ELEM(dyn, orig), dyn->elemSize);
	if (orig < dest) {
		memmove(LDIN_ELEM(dyn, orig), LDIN_ELEM(dyn, orig + 1), (dest - orig) * dyn->elemSize);
	} else {
		memmove(LDIN_ELEM(dyn, dest + 1), LDIN_ELEM(dyn, dest), (orig - dest) * dyn->elemSize);
	}
	memcpy(LDIN_ELEM(dyn, dest), tmp, dyn->elemSize);
	Mem_Free(tmp);
}

void DynList_Clear(cDynList *dyn) {
	DynList_Check(dyn);
	if (dyn->mem) {
		Mem_Free(dyn->mem);
		dyn->mem = NULL;
	}
	dyn->total = 0;
	dyn->used = 0;
	dyn->changed = 1;
}
