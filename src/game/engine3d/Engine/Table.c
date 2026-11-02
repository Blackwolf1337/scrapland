/*
 * The instances of the array template of the engine3d, engine3d/engine/table.h (include/scrap/table.h).
 */
#include <string.h>

#include "scrap/table.h"
#include "scrap/bsp.h"
#include "scrap/ground.h"
#include "scrap/model.h"
#include "scrap/node3d.h"
#include "scrap/readm3d.h"

static void Table_New(CTable *t, int num, uint32_t size) {
	CHECK(!t->p && num > 0);
	t->n = num;
	t->p = New_Array(num, size);
	CHECK(t->p);
}

static void Table_NewObjs(CTable *t, int num, uint32_t size, void *ctor, void *dtor) {
	CHECK(!t->p && num > 0);
	t->n = num;
	t->p = New_Objects(num, size, ctor, dtor);
	CHECK(t->p);
}

#define TABLE_ALLOC(sz) \
	void Table_Alloc##sz(CTable *t, int num) { \
		Table_New(t, num, 0x##sz); \
	}
TABLE_ALLOC(1)
TABLE_ALLOC(2)
TABLE_ALLOC(4)
TABLE_ALLOC(8)
TABLE_ALLOC(C)
TABLE_ALLOC(10)
TABLE_ALLOC(14)
TABLE_ALLOC(18)
TABLE_ALLOC(1C)
TABLE_ALLOC(2C)
TABLE_ALLOC(38)

/* the object tables: the element size of the type (0x<sz> on 32 bit builds) */
#define TABLE_NEW(sz, type, ctor, dtor) \
	static_assert(!SD_32BIT || sizeof(type) == 0x##sz, #type); \
	void Table_New##sz(CTable *t, int num) { \
		Table_NewObjs(t, num, sizeof(type), ctor, dtor); \
	}
TABLE_NEW(8, CLod, (void *)CLod_ctor, (void *)CLod_dtor)
TABLE_NEW(1C, M3DSkinVert, (void *)M3DSkinVert_ctor, (void *)M3DSkinVert_dtor)
TABLE_NEW(24, CCollideQuad, (void *)CCollideQuad_ctor, (void *)CCollideQuad_dtor)
TABLE_NEW(A8, CRenderNode, (void *)CRenderNode_ctor, (void *)CRenderNode_dtor)
TABLE_NEW(B4, BspZone, (void *)BspZone_ctor, (void *)BspZone_dtor)
TABLE_NEW(FC, CNode3D, (void *)CNode3D_ctor, (void *)CNode3D_dtor)

#define TABLE_READ(sz) \
	void Table_Read##sz(CFileIO *f, CTable *t) { \
		CHECK(!t->p); \
		int num = CFileIO_ReadInt(f); \
		int size = CFileIO_ReadInt(f); \
		CHECK(size == 0x##sz); \
		if (num) { \
			Table_Alloc##sz(t, num); \
			CFileIO_Read(f, t->p, t->n * 0x##sz); \
		} \
	}
TABLE_READ(2)
TABLE_READ(C)
TABLE_READ(1C)

static void Array_Read(CFileIO *f, void **p, uint32_t *n, int size) {
	*n = CFileIO_ReadInt(f);
	CHECK(!*p && CFileIO_ReadInt(f) == size);
	if (*n) {
		*p = New_Array(*n, size);
		CFileIO_Read(f, *p, *n * size);
	}
}

#define ARRAY_READ(sz) \
	void Array_Read##sz(CFileIO *f, void **p, uint32_t *n) { \
		Array_Read(f, p, n, 0x##sz); \
	}
ARRAY_READ(2)
ARRAY_READ(4)
ARRAY_READ(6)
ARRAY_READ(8)
ARRAY_READ(C)
ARRAY_READ(10)

static void Array_Write(CFileIO *f, const void *p, uint32_t n, int size) {
	if (!p) {
		n = 0;
	}
	CFileIO_WriteInt(f, n);
	CFileIO_WriteInt(f, size);
	CFileIO_Write(f, p, n * size);
}

#define ARRAY_WRITE(sz) \
	void Array_Write##sz(CFileIO *f, const void *p, uint32_t n) { \
		Array_Write(f, p, n, 0x##sz); \
	}
ARRAY_WRITE(2)
ARRAY_WRITE(4)
ARRAY_WRITE(6)
ARRAY_WRITE(8)
ARRAY_WRITE(C)
ARRAY_WRITE(10)

void *Table_GetN(CTable *t, uint32_t i, uint32_t size) {
	TABLE_CHECK(t->p, i, t->n);
	return (byte *)t->p + i * size;
}

void Table_Free(CTable *t) {
	if (t->p) {
		operator_delete_array(t->p);
		t->p = NULL;
		t->n = 0;
	}
}

/* the resize of table.h: n elements of size, the old ones kept as far as they fit, n < 1 frees */
static void Table_Resize(CTable *t, int n, uint32_t size, void(*alloc)(CTable *, int)) {
	if (n < 1) {
		if (t->p) {
			operator_delete_array(t->p);
			t->p = NULL;
			t->n = 0;
		}
		return;
	}
	if (n == t->n) {
		return;
	}
	void *old = t->p;
	if (!old) {
		alloc(t, n);
		return;
	}
	t->p = New_Array((uint32_t)n, size);
	memcpy(t->p, old, (uint32_t)(t->n < n ? t->n : n) * size);
	operator_delete_array(old);
	t->n = n;
}

/* n elements of size bytes, the sizeof of the element type at the call: the tables whose elements hold pointers */
void Table_AllocN(CTable *t, int num, uint32_t size) {
	Table_New(t, num, size);
}

void Table_ResizeN(CTable *t, int n, uint32_t size) {
	if (n < 1) {
		Table_Free(t);
		return;
	}
	if (n == t->n) {
		return;
	}
	void *old = t->p;
	if (!old) {
		Table_New(t, n, size);
		return;
	}
	t->p = New_Array((uint32_t)n, size);
	memcpy(t->p, old, (uint32_t)(t->n < n ? t->n : n) * size);
	operator_delete_array(old);
	t->n = n;
}

void Table_Resize2(CTable *t, int n) {
	Table_Resize(t, n, 2, Table_Alloc2);
}

void Table_Resize4(CTable *t, int n) {
	Table_Resize(t, n, 4, Table_Alloc4);
}

void Table_ResizeC(CTable *t, int n) {
	Table_Resize(t, n, 0xc, Table_AllocC);
}

void Table_Resize14(CTable *t, int n) {
	Table_Resize(t, n, 0x14, Table_Alloc14);
}

void Table_Resize1C(CTable *t, int n) {
	Table_Resize(t, n, 0x1c, Table_Alloc1C);
}

void Table_Resize2C(CTable *t, int n) {
	Table_Resize(t, n, 0x2c, Table_Alloc2C);
}

void Table_Resize38(CTable *t, int n) {
	Table_Resize(t, n, 0x38, Table_Alloc38);
}
