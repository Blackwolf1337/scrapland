/*
 * ScrapEngWin32.cpp of the game: the window procedure of the engine (the menu commands of the D3D window, the
 * cursor, the paint while paused, the power messages) and the unhandled exceptions: the filter shows the exception,
 * its address and the fault as a fatal error (Con_FatalErrorf).
 */
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/dxutil.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/e3dutil.h"
#include "scrap/video.h"
#include "scrap/win32.h"

#define APP_EXCEPTIONFILTER 0x233410

/* the state of g_Motor the window procedure reads */
#define MOTOR_SHOWCURSOR(m) (*(int *)((m) + 0x2a5c4))
#define MOTOR_WINDOWRECT(m) ((exe_rect *)((m) + 0x2a4f8))
#define MOTOR_CLIENTRECT(m) ((exe_rect *)((m) + 0x2a508))
#define MOTOR_VT_PAUSE 0x34
#define DEV_VT_SHOWCURSOR 0x30
#define DEV_VT_PRESENT 0x3c

#define WM_PAINT 0x0f
#define WM_SETCURSOR 0x20
#define WM_COMMAND 0x111
#define WM_POWERBROADCAST 0x218
#define WM_EXITSIZEMOVE 0x232
#define PBT_APMQUERYSUSPEND 0
#define PBT_APMRESUMESUSPEND 7
#define BROADCAST_QUERY_DENY 0x424d5144
/* the commands of the menu of the D3D window */
#define IDM_CHANGEDEVICE 0x9c42
#define IDM_TOGGLEFULLSCREEN 0x9c43
#define IDM_SPECULAR 0x9c54
#define IDM_RENDERMODE100 0x9c56
#define IDM_PROFILER 0x9ca4
#define IDM_TILE 0x9ca5
#define IDM_TEXFILTER 0x9ca7
#define IDM_SHADOWS 0x9ca8
#define IDM_PROFMODE 0x9ca9
#define IDM_PROFMODE2 0x9caa

static void Motor_Pause(byte *m, int on) {
	((void(__thiscall *)(byte *, int))(*(void ***)m)[MOTOR_VT_PAUSE / 4])(m, on);
}

intptr_t D3DApp_MsgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	byte *m = g_Motor;

	switch (msg) {
	case WM_SETCURSOR:
		if (m && MOTOR_ACTIVE(m) && MOTOR_READY(m) && !MOTOR_WINDOWED(m)) {
			exe_SetCursor(NULL);
			if (MOTOR_SHOWCURSOR(g_Motor)) {
				void *dev = MOTOR_DEV(g_Motor);

				((int(__stdcall *)(void *, int))(*(void ***)dev)[DEV_VT_SHOWCURSOR / 4])(dev, 1);
			}
			return 1;
		}
		break;
	case WM_PAINT:
		/* differs from the original on purpose (no blocking waits): no Present over a video, which plays from the
		 * message loop now (the original handled no message while it played) */
		if (m && MOTOR_DEV(m) && !MOTOR_READY(m) && MOTOR_WINDOWED(m) && !Video_Busy()) {
			void *dev = MOTOR_DEV(m);

			((int(__stdcall *)(void *, void *, void *, void *, void *))(*(void ***)dev)[DEV_VT_PRESENT / 4])(dev,
				NULL, NULL, NULL, NULL);
			return exe_DefWindowProcA(hwnd, msg, wParam, lParam);
		}
		break;
	case WM_COMMAND:
		switch ((uint16_t)wParam) {
		case IDM_CHANGEDEVICE:
			if (g_Motor && MOTOR_ACTIVE(g_Motor) && MOTOR_READY(g_Motor)) {
				Motor_Pause(g_Motor, 1);
				if (CD3DApp_UserSelectNewDevice(g_Motor) >= 0) {
					Motor_Pause(g_Motor, 0);
				}
			}
			return 0;
		case IDM_TOGGLEFULLSCREEN: {
			void *fg = exe_GetForegroundWindow();

			if (fg == App_GetHWnd() && MOTOR_ACTIVE(g_Motor) && MOTOR_READY(g_Motor)) {
				ConsoleWin_Show(MOTOR_WINDOWED(g_Motor) == 0);
				if (CD3DApp_ToggleFullscreen(g_Motor) < 0) {
					Con_FatalErrorf("Could not reset the Direct3D device.");
				}
			}
			return 0;
		}
		case IDM_PROFILER:
			Engine_ToggleOption(0x10);
			return 0;
		case IDM_TILE:
			Engine_ToggleOption(0x20);
			return 0;
		case IDM_RENDERMODE100:
			Engine_ToggleOption(0x100);
			return 0;
		case IDM_SPECULAR:
			Engine_ToggleOption(0x101);
			return 0;
		case IDM_TEXFILTER:
			Engine_ToggleOption(0x102);
			return 0;
		case IDM_SHADOWS:
			Engine_ToggleOption(0x103);
			return 0;
		case IDM_PROFMODE:
			g_ProfMode = (g_ProfMode + 1) % 6;
			return 0;
		case IDM_PROFMODE2:
			g_ProfMode = (g_ProfMode + 1) % 2;
			return 0;
		}
		break;
	case WM_POWERBROADCAST:
		if (wParam == PBT_APMQUERYSUSPEND) {
			return BROADCAST_QUERY_DENY;
		}
		if (wParam == PBT_APMRESUMESUSPEND) {
			return 1;
		}
		break;
	case WM_EXITSIZEMOVE:
		if (m) {
			exe_GetWindowRect(hwnd, MOTOR_WINDOWRECT(m));
			exe_GetClientRect(hwnd, MOTOR_CLIENTRECT(g_Motor));
			return exe_DefWindowProcA(hwnd, msg, wParam, lParam);
		}
		break;
	}
	return exe_DefWindowProcA(hwnd, msg, wParam, lParam);
}
#define g_PrevExceptionFilter GAME_GLOBAL(void *, g_PrevExceptionFilter, 0x4aa310)

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
		g_PrevExceptionFilter = exe_SetUnhandledExceptionFilter(GAME_FUNC(App_ExceptionFilter, APP_EXCEPTIONFILTER));
		return;
	}
	exe_SetUnhandledExceptionFilter(g_PrevExceptionFilter);
	g_PrevExceptionFilter = NULL;
}
