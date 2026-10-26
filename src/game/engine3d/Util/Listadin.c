/*
 * Dynamic arrays of the engine3d, original source Util/Listadin.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

/* the inlined check at the start of every function */
static void LDin_Check(const cListaDin *din) {
	CHECK(din);
	CHECK(din->id == LISTADIN_ID);
}

#define LDIN_ELEM(din, i) ((din)->mem + (din)->elemSize * (i))

void LDin_Free(cListaDin *din) {
	LDin_Clear(din);
	din->id = 0;
	Mem_Free(din);
}

static cListaDin *LDin_Alloc(uint16_t elemSize, uint16_t blockSize) {
	cListaDin *din = Mem_Alloc(sizeof(*din));
	din->id = LISTADIN_ID;
	din->mem = NULL;
	din->total = 0;
	din->used = 0;
	din->elemSize = elemSize;
	din->blockSize = blockSize;
	din->changed = 0;
	din->pad_17 = 0;
	return din;
}

cListaDin *LDin_New(uint16_t elemSize) {
	return LDin_Alloc(elemSize, 0x100);
}

cListaDin *LDin_NewBlock(uint16_t elemSize, uint16_t blockSize) {
	CHECK(blockSize > 0);
	return LDin_Alloc(elemSize, blockSize);
}

void *LDin_Get(cListaDin *din, uint32_t i) {
	LDin_Check(din);
	CHECK(i < din->used);
	CHECK(din->mem);
	return LDIN_ELEM(din, i);
}

uint32_t LDin_Count(cListaDin *din) {
	LDin_Check(din);
	return din->used;
}

void *LDin_Last(cListaDin *din) {
	LDin_Check(din);
	CHECK(din->used > 0);
	CHECK(din->mem);
	return LDIN_ELEM(din, din->used - 1);
}

void *LDin_AddOne(cListaDin *din) {
	return LDin_Add(din, 1);
}

void *LDin_Add(cListaDin *din, int num) {
	LDin_Check(din);
	CHECK(num > 0);
	if (din->total < din->used + num) {
		uint32_t total = num - 1 + din->blockSize + din->used;
		din->total = total - total % din->blockSize;
		din->mem = Mem_Realloc(din->mem, din->elemSize * din->total);
	}
	uint32_t first = din->used;
	din->changed = 1;
	din->used = first + num;
	CHECK(din->used <= din->total);
	return LDIN_ELEM(din, first);
}

void LDin_Copy(cListaDin *din, uint32_t orig, uint32_t dest, int num) {
	LDin_Check(din);
	CHECK(num >= 0);
	CHECK(orig < din->used && orig + num <= din->used);
	CHECK(dest < din->used && dest + num <= din->used);
	if (orig != dest && num) {
		din->changed = 1;
		memmove(LDIN_ELEM(din, dest), LDIN_ELEM(din, orig), din->elemSize * num);
	}
}

void LDin_Delete(cListaDin *din, uint32_t i) {
	LDin_Check(din);
	CHECK(i < din->used);
	if (i != din->used - 1) {
		memmove(LDIN_ELEM(din, i), LDIN_ELEM(din, i + 1), (din->used - 1 - i) * din->elemSize);
	}
	LDin_RemoveLast(din);
}

/* frees whole blocks that are no longer used */
void LDin_RemoveLast(cListaDin *din) {
	LDin_Check(din);
	if (!din->used) {
		return;
	}
	din->used--;
	din->changed = 1;
	uint32_t limit = din->blockSize + din->used;
	if (limit > din->total) {
		return;
	}
	uint32_t total = din->total;
	do {
		total -= din->blockSize;
	} while (limit <= total);
	din->total = total;
	if (din->elemSize * total != 0) {
		din->mem = Mem_Realloc(din->mem, din->elemSize * total);
		return;
	}
	Mem_Free(din->mem);
	din->mem = NULL;
	CHECK(!din->total && !din->used);
}

/* i == used fails the CHECK of LDin_Copy, in the original too */
void *LDin_Insert(cListaDin *din, uint32_t i) {
	LDin_Check(din);
	CHECK(i <= din->used);
	LDin_Add(din, 1);
	LDin_Copy(din, i, i + 1, din->used - i - 1);
	return LDIN_ELEM(din, i);
}

void LDin_Move(cListaDin *din, int orig, int dest) {
	LDin_Check(din);
	CHECK(din->used);
	if (orig < 0) {
		orig = din->used - 1;
	}
	if (dest < 0) {
		dest = din->used - 1;
	}
	CHECK((uint32_t)orig < din->used);
	CHECK((uint32_t)dest < din->used);
	if (orig == dest) {
		return;
	}
	din->changed = 1;
	void *tmp = Mem_Alloc(din->elemSize);
	memcpy(tmp, LDIN_ELEM(din, orig), din->elemSize);
	if (orig < dest) {
		memmove(LDIN_ELEM(din, orig), LDIN_ELEM(din, orig + 1), (dest - orig) * din->elemSize);
	} else {
		memmove(LDIN_ELEM(din, dest + 1), LDIN_ELEM(din, dest), (orig - dest) * din->elemSize);
	}
	memcpy(LDIN_ELEM(din, dest), tmp, din->elemSize);
	Mem_Free(tmp);
}

void LDin_Clear(cListaDin *din) {
	LDin_Check(din);
	if (din->mem) {
		Mem_Free(din->mem);
		din->mem = NULL;
	}
	din->total = 0;
	din->used = 0;
	din->changed = 1;
}
