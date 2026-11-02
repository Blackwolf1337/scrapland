/*
 * ScrapEngWin32.cpp of the game: the unhandled exceptions, the filter shows the exception, its address and the fault
 * as a fatal error (Con_FatalErrorf). The window procedure of the engine is gone: src/game/App/Platform.c handles the
 * events (its hotkeys are the full screen and tiled shot commands that were left of the menu).
 */
#include "scrap/rdev.h"
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/dxutil.h"
#include "scrap/scrapmain.h"
#include "scrap/e3dutil.h"
#include "scrap/video.h"
#include "scrap/win32.h"

#define APP_EXCEPTIONFILTER 0x233410


extern void *g_PrevExceptionFilter;

#define EXC_ACCESS_VIOLATION 0xc0000005u

static const char *Exception_Name(uint32_t code) {
	switch (code) {
	case EXC_ACCESS_VIOLATION:
		return "ACCESS_VIOLATION";
	case 0x80000002u:
		return "DATATYPE_MISALIGNMENT";
	case 0x80000003u:
		return "BREAKPOINT";
	case 0x80000004u:
		return "SINGLE_STEP";
	case 0xc0000006u:
		return "IN_PAGE_ERROR";
	case 0xc000001du:
		return "ILLEGAL_INSTRUCTION";
	case 0xc0000025u:
		return "NONCONTINUABLE_EXCEPTION";
	case 0xc0000026u:
		return "INVALID_DISPOSITION";
	case 0xc000008cu:
		return "ARRAY_BOUNDS_EXCEEDED";
	case 0xc000008du:
		return "FLT_DENORMAL_OPERAND";
	case 0xc000008eu:
		return "FLT_DIVIDE_BY_ZERO";
	case 0xc000008fu:
		return "FLT_INEXACT_RESULT";
	case 0xc0000090u:
		return "FLT_INVALID_OPERATION";
	case 0xc0000091u:
		return "FLT_OVERFLOW";
	case 0xc0000092u:
		return "FLT_STACK_CHECK";
	case 0xc0000093u:
		return "FLT_UNDERFLOW";
	case 0xc0000094u:
		return "INT_DIVIDE_BY_ZERO";
	case 0xc0000095u:
		return "INT_OVERFLOW";
	case 0xc0000096u:
		return "PRIV_INSTRUCTION";
	case 0xc00000fdu:
		return "STACK_OVERFLOW";
	}
	return NULL;
}

/* EXCEPTION_POINTERS: the record (code, flags, next, address, nparams, params) and the CONTEXT */
int __stdcall App_ExceptionFilter(void **ep) {
	const uint32_t *rec = ep[0];
	const uint32_t *ctx = ep[1];
	const char *name = Exception_Name(rec[0]);
	char info[0x40], regs[0x40];

	info[0] = '-';
	info[1] = 0;
	if (rec[0] == EXC_ACCESS_VIOLATION && rec[4] == 2) {
		Bprintf(info, sizeof(info), "%s fault on address 0x%08X", rec[5] ? "Write" : "Read", rec[6]);
	}
	regs[0] = '-';
	regs[1] = 0;
	/* CONTEXT_CONTROL: Esp at 0xc4, Ebp at 0xb4 */
	if (ctx && (ctx[0] & 0x10001)) {
		Bprintf(regs, sizeof(regs), "ESP: 0x%08X\nEBP: 0x%08X", ctx[0x31], ctx[0x2d]);
	}
	Con_FatalErrorf("\nUnhandled exception: %s\t\n\nAddress: 0x%08X\n\nInfo: %s\n\n%s\n", name, rec[3], info, regs);
	return -1;
}

void App_SetExceptionFilter(byte on) {
	if (on) {
		CHECK(!g_PrevExceptionFilter);
		g_PrevExceptionFilter = exe_SetUnhandledExceptionFilter((void *)App_ExceptionFilter);
		return;
	}
	exe_SetUnhandledExceptionFilter(g_PrevExceptionFilter);
	g_PrevExceptionFilter = NULL;
}
