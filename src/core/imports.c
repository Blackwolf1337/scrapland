/*
 * Thunks for the functions of src/imports.inc. Each is a global symbol with the original name that jumps through
 * a slot holding the RVA, Imports_Resolve adds the load address. The stack and the registers stay untouched, so
 * the calling convention of the caller's prototype goes through as it is. The Python functions (SD_IMPORT_PY) get
 * no thunk in ScrapC.exe (SD_STANDALONE), it links the real Python library; neither do the CRT functions
 * (SD_IMPORT_CRT), which are src/standalone/crt.c there.
 *
 * This file must not see the real prototypes (the thunks are plain labels), so it includes no game header.
 */
#include <stdint.h>

#ifdef SD_STANDALONE
#define SD_IMPORT_PY(name, rva)
#define SD_IMPORT_CRT(name, rva)
#define SD_IMPORT_CRT_STDCALL(name, argbytes, rva)
#else
#define SD_IMPORT_PY(name, rva) SD_IMPORT(name, rva)
#define SD_IMPORT_CRT(name, rva) SD_IMPORT(name, rva)
#define SD_IMPORT_CRT_STDCALL(name, argbytes, rva) SD_IMPORT_STDCALL(name, argbytes, rva)
#endif

#define SD_THUNK(sym, name, rva) \
	uint32_t imp_##name = (rva); \
	__asm__(".text\n\t.globl " sym "\n\t.p2align 4\n" sym ":\n\tjmp *_imp_" #name "\n");
#define SD_IMPORT(name, rva) SD_THUNK("_" #name, name, rva)
#define SD_IMPORT_STDCALL(name, argbytes, rva) SD_THUNK("_" #name "@" #argbytes, name, rva)
#include "imports.inc"
#undef SD_IMPORT
#undef SD_IMPORT_STDCALL

static uint32_t *const imp_slots[] = {
#define SD_IMPORT(name, rva) &imp_##name,
#define SD_IMPORT_STDCALL(name, argbytes, rva) &imp_##name,
#include "imports.inc"
#undef SD_IMPORT
#undef SD_IMPORT_STDCALL
};

int Imports_Resolve(uintptr_t base);

int Imports_Resolve(uintptr_t base) {
	for (size_t i = 0; i < sizeof(imp_slots) / sizeof(imp_slots[0]); i++) {
		*imp_slots[i] += (uint32_t)base;
	}
	return (int)(sizeof(imp_slots) / sizeof(imp_slots[0]));
}
