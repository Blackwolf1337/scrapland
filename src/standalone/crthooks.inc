/*
 * ScrapC.exe: the entries of the original's C runtime that jump to the C of src/standalone/crt.c and crtinit.c,
 * included by src/core/hook.c with its SD_HOOK. First the SD_IMPORT_CRT functions of src/imports.inc, then the ones
 * only the code of the original that still runs from the copy reaches (the dynamic initializers and atexit
 * destructors of the game's globals, D3DX8 with libpng, libjpeg and zlib, the ASE SDK, the M3D library) and the
 * tests (tools/test/py call memcpy and memset by rva). A size of 5: only the jump is written, wipecrt=1 fills the
 * rest of the runtime.
 */
#define SD_IMPORT(name, rva)
#define SD_IMPORT_STDCALL(name, argbytes, rva)
#define SD_IMPORT_PY(name, rva)
#define SD_IMPORT_CRT(name, rva) SD_HOOK(name, rva, 5)
#define SD_IMPORT_CRT_STDCALL(name, argbytes, rva) SD_HOOK(name, rva, 5)
#include "imports.inc"
#undef SD_IMPORT
#undef SD_IMPORT_STDCALL
#undef SD_IMPORT_PY
#undef SD_IMPORT_CRT
#undef SD_IMPORT_CRT_STDCALL

SD_HOOK(crt_eh_vector_ctor, 0x313a71, 5)	/* jmp to 0x313a76 */
SD_HOOK(crt_eh_vector_dtor, 0x313adf, 5)	/* jmp to 0x313ae4 */
SD_HOOK(crt_operator_delete_array_sized, 0x313be4, 5)
SD_HOOK(crt_security_check_cookie, 0x313650, 5)
SD_HOOK(crt_alloca_probe, 0x3141b0, 5)
SD_HOOK(crt_memset, 0x319210, 5)
SD_HOOK(crt_memcpy, 0x3197d0, 5)
SD_HOOK(crt_purecall, 0x31951a, 5)
SD_HOOK(crt_strncpy, 0x34de00, 5)
SD_HOOK(crt_finite, 0x34e74b, 5)
SD_HOOK(crt_calloc, 0x368b3b, 5)
SD_HOOK(crt_aullshr, 0x3769f0, 5)
SD_HOOK(crt_EH_prolog, 0x376c04, 5)
SD_HOOK(crt_longjmp, 0x379db0, 5)
SD_HOOK(crt_setjmp3, 0x379dd0, 5)
SD_HOOK(crt_ftol, 0x379f50, 5)
SD_HOOK(crt_vsnprintf, 0x37ae2c, 5)
SD_HOOK(crt_snprintf, 0x37b2fe, 5)
