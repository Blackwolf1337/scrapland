/*
 * The shared stubs: tiny methods (mostly empty vtable slots of the EmbeddedFX, CObjBase3D and CTriMesh classes,
 * constructors of empty classes) that have the same body, so one C function fills every slot that needs it. The
 * variants with arguments came from the callee-pops conventions of the original; the tests still name them. The
 * names are invented.
 */
#include "scrap/shared.h"

/* c3: ret */
void Stub_Nop(void) {
}

/* c2 04 00: ret 4, a method with one argument that does nothing */
void Stub_Nop1(void *self, int a) {
	(void)self;
	(void)a;
}

/* c2 08 00: ret 8 */
void Stub_Nop2(void *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
}

/* c2 0c 00: ret 0xc */
void Stub_Nop3(void *self, int a, int b, int c) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
}

/* b0 01 c3: mov al, 1 */
byte Stub_True(void) {
	return 1;
}

/* 33 c0 c3: xor eax, eax */
int Stub_Zero(void) {
	return 0;
}

/* 8b c1 c3: mov eax, ecx, the constructor of a class without fields */
void *Stub_ReturnSelf(void *self) {
	return self;
}
