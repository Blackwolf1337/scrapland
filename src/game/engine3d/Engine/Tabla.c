/*
 * The instances of the array template of the engine3d, engine3d/engine/tabla.h (include/scrap/tabla.h).
 */
#include <string.h>

#include "scrap/tabla.h"

static void Tabla_New(CTabla *t, int num, uint32_t size) {
	CHECK(!t->p && num > 0);
	t->n = num;
	t->p = New_Array(num, size);
	CHECK(t->p);
}

static void Tabla_NewObjs(CTabla *t, int num, uint32_t size, uint32_t ctor, uint32_t dtor) {
	CHECK(!t->p && num > 0);
	t->n = num;
	t->p = New_Objects(num, size, ctor, dtor);
	CHECK(t->p);
}

#define TABLA_ALLOC(sz) \
	void __thiscall Tabla_Alloc##sz(CTabla *t, int num) { \
		Tabla_New(t, num, 0x##sz); \
	}
TABLA_ALLOC(1)
TABLA_ALLOC(2)
TABLA_ALLOC(4)
TABLA_ALLOC(8)
TABLA_ALLOC(C)
TABLA_ALLOC(10)
TABLA_ALLOC(14)
TABLA_ALLOC(18)
TABLA_ALLOC(1C)
TABLA_ALLOC(2C)
TABLA_ALLOC(38)

void __thiscall Tabla_Alloc10Z(CTabla *t, int num) {
	CHECK(!t->p && num > 0);
	t->n = num;
	byte *p = New_Array(num, 0x10);
	if (p) {
		for (int i = 0; i < num; i++) {
			memset(p + i * 0x10, 0, 14);
		}
	}
	t->p = p;
	CHECK(t->p);
}

#define TABLA_NEW(sz, ctor, dtor) \
	void __thiscall Tabla_New##sz(CTabla *t, int num) { \
		Tabla_NewObjs(t, num, 0x##sz, ctor, dtor); \
	}
TABLA_NEW(8, 0x252a70, 0x252d80)
TABLA_NEW(1C, 0x2bbcc0, 0x2bc160)
TABLA_NEW(24, 0x258720, 0x258ce0)
TABLA_NEW(A8, 0x23f3d0, 0x23f450)
TABLA_NEW(B4, 0x281c50, 0x2824b0)
TABLA_NEW(FC, 0x23cf40, 0x23d070)

#define TABLA_READ(sz) \
	void Tabla_Read##sz(CArchIO *f, CTabla *t) { \
		CHECK(!t->p); \
		int num = CArchIO_ReadInt(f); \
		int size = CArchIO_ReadInt(f); \
		CHECK(size == 0x##sz); \
		if (num) { \
			Tabla_Alloc##sz(t, num); \
			CArchIO_Read(f, t->p, t->n * 0x##sz); \
		} \
	}
TABLA_READ(2)
TABLA_READ(C)
TABLA_READ(1C)

static void Array_Read(CArchIO *f, void **p, uint32_t *n, int size) {
	*n = CArchIO_ReadInt(f);
	CHECK(!*p && CArchIO_ReadInt(f) == size);
	if (*n) {
		*p = New_Array(*n, size);
		CArchIO_Read(f, *p, *n * size);
	}
}

#define ARRAY_READ(sz) \
	void Array_Read##sz(CArchIO *f, void **p, uint32_t *n) { \
		Array_Read(f, p, n, 0x##sz); \
	}
ARRAY_READ(2)
ARRAY_READ(4)
ARRAY_READ(6)
ARRAY_READ(8)
ARRAY_READ(C)
ARRAY_READ(10)

static void Array_Write(CArchIO *f, const void *p, uint32_t n, int size) {
	if (!p) {
		n = 0;
	}
	CArchIO_WriteInt(f, n);
	CArchIO_WriteInt(f, size);
	CArchIO_Write(f, p, n * size);
}

#define ARRAY_WRITE(sz) \
	void Array_Write##sz(CArchIO *f, const void *p, uint32_t n) { \
		Array_Write(f, p, n, 0x##sz); \
	}
ARRAY_WRITE(2)
ARRAY_WRITE(4)
ARRAY_WRITE(6)
ARRAY_WRITE(8)
ARRAY_WRITE(C)
ARRAY_WRITE(10)
