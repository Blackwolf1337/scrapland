/*
 * The shared stubs: tiny methods (mostly empty vtable slots of the EmbeddedFX, CObjBase3D and CTriMesh classes,
 * constructors of empty classes) that have the same body, so one C function fills every slot that needs it. The
 * variants with arguments came from the callee-pops conventions of the original; the tests still name them. The
 * names are invented.
 */
#include "scrap/shared.h"

/* 33 c0 c3: xor eax, eax */
int Stub_Zero(void) {
	return 0;
}

/* 8b c1 c3: mov eax, ecx, the constructor of a class without fields */
void *Stub_ReturnSelf(void *self) {
	return self;
}
