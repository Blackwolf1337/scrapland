/*
 * engine3d/engine/tabla.h: CTabla<T>, the array template of the engine3d (n elements at p), and the readers of
 * arrays stored in CArchIO files (count, sizeof(T), the elements). The instances differ in the element size and in
 * the constructor of the elements; the C versions are named after the size.
 */
#ifndef SCRAP_TABLA_H
#define SCRAP_TABLA_H

#include "scrap/archio.h"

/* the index check of the CTabla accessors of tabla.h: "p && (dword)i<(dword)n" */
#define TABLA_CHECK(p, i, n) CHECK((p) && (uint32_t)(i) < (uint32_t)(n))

typedef struct CTabla {
	int n;				/* 0x0 */
	void *p;			/* 0x4 */
} CTabla;
SD_SIZE(CTabla, 0x8);

/* new elements, not initialized; CHECK(!p && num > 0) */
void __thiscall Tabla_Alloc1(CTabla *t, int num);
void __thiscall Tabla_Alloc2(CTabla *t, int num);
void __thiscall Tabla_Alloc4(CTabla *t, int num);
void __thiscall Tabla_Alloc8(CTabla *t, int num);
void __thiscall Tabla_AllocC(CTabla *t, int num);
void __thiscall Tabla_Alloc10(CTabla *t, int num);
void __thiscall Tabla_Alloc14(CTabla *t, int num);
void __thiscall Tabla_Alloc18(CTabla *t, int num);
void __thiscall Tabla_Alloc1C(CTabla *t, int num);
void __thiscall Tabla_Alloc2C(CTabla *t, int num);
void __thiscall Tabla_Alloc38(CTabla *t, int num);
/* 16 byte elements whose first 14 bytes are cleared */
void __thiscall Tabla_Alloc10Z(CTabla *t, int num);
/* objects: new[] with the count before them and the constructor of the exe on each */
void __thiscall Tabla_New8(CTabla *t, int num);
void __thiscall Tabla_New1C(CTabla *t, int num);
void __thiscall Tabla_New24(CTabla *t, int num);
void __thiscall Tabla_NewA8(CTabla *t, int num);
void __thiscall Tabla_NewB4(CTabla *t, int num);
void __thiscall Tabla_NewFC(CTabla *t, int num);
/* count and size from f, then the elements; t must be empty */
void Tabla_Read2(CArchIO *f, CTabla *t);
void Tabla_ReadC(CArchIO *f, CTabla *t);
void Tabla_Read1C(CArchIO *f, CTabla *t);
/* the same into a plain array and its count */
void Array_Read2(CArchIO *f, void **p, uint32_t *n);
void Array_Read4(CArchIO *f, void **p, uint32_t *n);
void Array_Read6(CArchIO *f, void **p, uint32_t *n);
void Array_Read8(CArchIO *f, void **p, uint32_t *n);
void Array_ReadC(CArchIO *f, void **p, uint32_t *n);
void Array_Read10(CArchIO *f, void **p, uint32_t *n);
/* the count (0 without p), sizeof(T) and the elements, instanced in MallaD3D.cpp */
void Array_Write2(CArchIO *f, const void *p, uint32_t n);
void Array_Write4(CArchIO *f, const void *p, uint32_t n);
void Array_Write6(CArchIO *f, const void *p, uint32_t n);
void Array_Write8(CArchIO *f, const void *p, uint32_t n);
void Array_WriteC(CArchIO *f, const void *p, uint32_t n);
void Array_Write10(CArchIO *f, const void *p, uint32_t n);
/* element i, TABLA_CHECK */
void *__thiscall Tabla_Get8(CTabla *t, uint32_t i);
void *__thiscall Tabla_Get50(CTabla *t, uint32_t i);
/* the delete[] of the elements, empty after (three copies in the exe) */
void __fastcall Tabla_Free(CTabla *t);
/* n elements, the old ones kept as far as they fit; n < 1 frees */
void __thiscall Tabla_Resize2(CTabla *t, int n);
void __thiscall Tabla_Resize4(CTabla *t, int n);
void __thiscall Tabla_ResizeC(CTabla *t, int n);
void __thiscall Tabla_Resize14(CTabla *t, int n);
void __thiscall Tabla_Resize1C(CTabla *t, int n);
void __thiscall Tabla_Resize2C(CTabla *t, int n);
void __thiscall Tabla_Resize38(CTabla *t, int n);

#endif
