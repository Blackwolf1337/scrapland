/*
 * The entries of the original's C runtime that C functions of ScrapC.exe stand for, as SD_HOOK(name, rva, size) lines
 * (until stage 3c2 src/core/hook.c pointed the entries of the copy here). No C includes this file any more: the tools
 * read it to name an rva of the original's CRT (tools/standalone/sdexe.py c_names for datagen.py --check and
 * rvacheck.py, tools/test/testsyms.py for the SD_TEST_CRT names, initpat.py). First the SD_IMPORT_CRT functions of
 * src/imports.inc, then the entries the tests and the vtables reach and the MSVC math with conventions of its own.
 */
#define SD_IMPORT(name, rva)
#define SD_IMPORT_STDCALL(name, argbytes, rva)
#define SD_IMPORT_PY(name, rva)
#define SD_IMPORT_CRT(name, rva) SD_HOOK(name, rva, 5)
#define SD_IMPORT_CRT_STDCALL(name, argbytes, rva) SD_HOOK(name, rva, 5)
#define SD_IMPORT_DLL_STDCALL(name, argbytes, rva)
#include "imports.inc"
#undef SD_IMPORT
#undef SD_IMPORT_STDCALL
#undef SD_IMPORT_PY
#undef SD_IMPORT_CRT
#undef SD_IMPORT_CRT_STDCALL
#undef SD_IMPORT_DLL_STDCALL

SD_HOOK(crt_eh_vector_ctor, 0x313a71, 5)	/* jmp to 0x313a76 */
SD_HOOK(crt_eh_vector_dtor, 0x313adf, 5)	/* jmp to 0x313ae4 */
SD_HOOK(crt_memset, 0x319210, 5)
SD_HOOK(crt_memcpy, 0x3197d0, 5)
SD_HOOK(crt_purecall, 0x31951a, 5)
/* the MSVC math with conventions of its own: Microsoft's ucrtbase (src/standalone/crtmath.c) and the helpers
 * without an export (src/standalone/crtvec.c) */
SD_HOOK(crt_libm_sse2_pow_precise, 0x321440, 5)
SD_HOOK(crt_CIfmod, 0x32201a, 5)
SD_HOOK(crt_CIatan2, 0x329f3a, 5)
SD_HOOK(crt_libm_sse2_acos_precise, 0x329f50, 5)
SD_HOOK(crt_libm_sse2_cos_precise, 0x346fe0, 5)
SD_HOOK(crt_libm_sse2_sin_precise, 0x3471a0, 5)
SD_HOOK(crt_libm_sse2_atan_precise, 0x347370, 5)
SD_HOOK(crt_libm_sse2_exp_precise, 0x34e950, 5)
SD_HOOK(crt_libm_sse2_log_precise, 0x34ec30, 5)
SD_HOOK(crt_libm_sse2_asin_precise, 0x350150, 5)
SD_HOOK(crt_ftoui3, 0x314310, 5)
SD_HOOK(crt_dtoui3, 0x3144f1, 5)
SD_HOOK(crt_vdecl_cos2, 0x314930, 5)
SD_HOOK(crt_vdecl_sin2, 0x314940, 5)
SD_HOOK(crt_vdecl_exp2, 0x314a00, 5)
