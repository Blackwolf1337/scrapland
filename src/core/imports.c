/*
 * Thunks for the functions of src/imports.inc. Each is a global symbol with the original name that jumps through
 * a slot holding the RVA, Imports_Resolve adds the load address. The stack and the registers stay untouched, so
 * the calling convention of the caller's prototype goes through as it is. The Python functions (SD_IMPORT_PY) get
 * no thunk in ScrapC.exe (SD_STANDALONE), it links the real Python library; neither do the CRT functions
 * (SD_IMPORT_CRT), which are src/standalone/crt.c there, and the DLL functions (SD_IMPORT_DLL_STDCALL), which are
 * imports of ScrapC.exe itself.
 *
 * This file must not see the real prototypes (the thunks are plain labels), so it includes no game header.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef SD_STANDALONE
#define SD_IMPORT_PY(name, rva)
#define SD_IMPORT_CRT(name, rva)
#define SD_IMPORT_CRT_STDCALL(name, argbytes, rva)
#define SD_IMPORT_DLL_STDCALL(name, argbytes, rva)
#else
#define SD_IMPORT_PY(name, rva) SD_IMPORT(name, rva)
#define SD_IMPORT_CRT(name, rva) SD_IMPORT(name, rva)
#define SD_IMPORT_CRT_STDCALL(name, argbytes, rva) SD_IMPORT_STDCALL(name, argbytes, rva)
#define SD_IMPORT_DLL_STDCALL(name, argbytes, rva) SD_IMPORT_STDCALL(name, argbytes, rva)
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
	NULL	/* the end; ScrapC.exe has no thunks at all */
};

int Imports_Resolve(uintptr_t base);

int Imports_Resolve(uintptr_t base) {
	int n = 0;

	for (uint32_t *const *p = imp_slots; *p; p++, n++) {
		**p += (uint32_t)base;
	}
	return n;
}
