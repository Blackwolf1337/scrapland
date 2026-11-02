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
#include "scrap/platform.h"

#ifdef _WIN32
#include <windows.h>

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

/* the exception, its address and the fault, the stack and frame pointers */
static LONG WINAPI App_ExceptionFilter(EXCEPTION_POINTERS *ep) {
	const EXCEPTION_RECORD *rec = ep->ExceptionRecord;
	const CONTEXT *ctx = ep->ContextRecord;
	const char *name = Exception_Name(rec->ExceptionCode);
	char info[0x60], regs[0x60];

	info[0] = '-';
	info[1] = 0;
	if (rec->ExceptionCode == EXC_ACCESS_VIOLATION && rec->NumberParameters == 2) {
		Bprintf(info, sizeof(info), "%s fault on address 0x%p", rec->ExceptionInformation[0] ? "Write" : "Read",
			(void *)rec->ExceptionInformation[1]);
	}
	regs[0] = '-';
	regs[1] = 0;
	if (ctx && (ctx->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
		Bprintf(regs, sizeof(regs), "RSP: 0x%p\nRBP: 0x%p", (void *)ctx->Rsp, (void *)ctx->Rbp);
	}
	Con_FatalErrorf("\nUnhandled exception: %s\t\n\nAddress: 0x%p\n\nInfo: %s\n\n%s\n", name, rec->ExceptionAddress, info,
		regs);
	return EXCEPTION_EXECUTE_HANDLER;
}

static LPTOP_LEVEL_EXCEPTION_FILTER g_PrevExceptionFilter;
#endif

/* the filter on Windows only; elsewhere a crash ends the process the normal way of the system */
void App_SetExceptionFilter(byte on) {
#ifdef _WIN32
	if (on) {
		CHECK(!g_PrevExceptionFilter);
		g_PrevExceptionFilter = SetUnhandledExceptionFilter(App_ExceptionFilter);
		return;
	}
	SetUnhandledExceptionFilter(g_PrevExceptionFilter);
	g_PrevExceptionFilter = NULL;
#else
	(void)on;
#endif
}
