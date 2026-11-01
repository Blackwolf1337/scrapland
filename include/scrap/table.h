/*
 * engine3d/engine/table.h: CTable<T>, the array template of the engine3d (n elements at p), and the readers of
 * arrays stored in CFileIO files (count, sizeof(T), the elements). The instances differ in the element size and in
 * the constructor of the elements; the C versions are named after the size.
 */
#ifndef SCRAP_TABLE_H
#define SCRAP_TABLE_H

#include "scrap/archio.h"

/* the index check of the CTable accessors of table.h: "p && (dword)i<(dword)n" */
#define TABLE_CHECK(p, i, n) CHECK((p) && (uint32_t)(i) < (uint32_t)(n))

typedef struct CTable {
	int n;				/* 0x0 */
	void *p;			/* 0x4 */
} CTable;
SD_SIZE(CTable, 0x8);

/* new elements, not initialized; CHECK(!p && num > 0) */
void Table_Alloc1(CTable *t, int num);
void Table_Alloc2(CTable *t, int num);
void Table_Alloc4(CTable *t, int num);
void Table_Alloc8(CTable *t, int num);
void Table_AllocC(CTable *t, int num);
void Table_Alloc10(CTable *t, int num);
void Table_Alloc14(CTable *t, int num);
void Table_Alloc18(CTable *t, int num);
void Table_Alloc1C(CTable *t, int num);
void Table_Alloc2C(CTable *t, int num);
void Table_Alloc38(CTable *t, int num);
/* 16 byte elements whose first 14 bytes are cleared */
void Table_Alloc10Z(CTable *t, int num);
/* objects: new[] with the count before them and the constructor of the exe on each */
void Table_New8(CTable *t, int num);
void Table_New1C(CTable *t, int num);
void Table_New24(CTable *t, int num);
void Table_NewA8(CTable *t, int num);
void Table_NewB4(CTable *t, int num);
void Table_NewFC(CTable *t, int num);
/* count and size from f, then the elements; t must be empty */
void Table_Read2(CFileIO *f, CTable *t);
void Table_ReadC(CFileIO *f, CTable *t);
void Table_Read1C(CFileIO *f, CTable *t);
/* the same into a plain array and its count */
void Array_Read2(CFileIO *f, void **p, uint32_t *n);
void Array_Read4(CFileIO *f, void **p, uint32_t *n);
void Array_Read6(CFileIO *f, void **p, uint32_t *n);
void Array_Read8(CFileIO *f, void **p, uint32_t *n);
void Array_ReadC(CFileIO *f, void **p, uint32_t *n);
void Array_Read10(CFileIO *f, void **p, uint32_t *n);
/* the count (0 without p), sizeof(T) and the elements, instanced in MallaD3D.cpp */
void Array_Write2(CFileIO *f, const void *p, uint32_t n);
void Array_Write4(CFileIO *f, const void *p, uint32_t n);
void Array_Write6(CFileIO *f, const void *p, uint32_t n);
void Array_Write8(CFileIO *f, const void *p, uint32_t n);
void Array_WriteC(CFileIO *f, const void *p, uint32_t n);
void Array_Write10(CFileIO *f, const void *p, uint32_t n);
/* element i, TABLE_CHECK */
void *Table_Get8(CTable *t, uint32_t i);
void *Table_Get50(CTable *t, uint32_t i);
/* the delete[] of the elements, empty after (three copies in the exe) */
void Table_Free(CTable *t);
/* n elements, the old ones kept as far as they fit; n < 1 frees */
void Table_Resize2(CTable *t, int n);
void Table_Resize4(CTable *t, int n);
void Table_ResizeC(CTable *t, int n);
void Table_Resize14(CTable *t, int n);
void Table_Resize1C(CTable *t, int n);
void Table_Resize2C(CTable *t, int n);
void Table_Resize38(CTable *t, int n);

#endif
