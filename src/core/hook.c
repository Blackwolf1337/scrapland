/*
 * Installs the hooks of src/hooks.inc: a 5 byte "jmp rel32" at the start of each original function. The C version
 * replaces the original completely, there is no trampoline back into it.
 *
 * With [general] wipe=1 the rest of each hooked original is filled with int3, so a jump into the middle of
 * replaced code stops the game with its rva in the log (a check for the standalone build, docs/standalone.md).
 * wipe=2 fills the whole game and engine code with int3 first, except the functions that stay in the exe.
 */
#include <string.h>
#include <windows.h>

/*
 * Only shared.h: the engine headers declare the hooked functions with their real prototypes, which would clash with
 * the plain declarations below. Fastcall and stdcall names get their decoration through --enable-stdcall-fixup.
 */
#include "scrap/shared.h"

extern uintptr_t game_base;

/* only the addresses are needed here, the real prototypes live in the implementing files */
#define SD_HOOK(name, rva, size) extern void name(void);
#include "hooks.inc"
#undef SD_HOOK

typedef struct {
	const char *name;
	uint32_t rva;
	uint32_t size;
	void *func;
} hook_t;

static const hook_t hooks[] = {
#define SD_HOOK(name, rva, size) { #name, (rva), (size), (void *)name },
#include "hooks.inc"
#undef SD_HOOK
};

int Hook_Enabled(const char *name);

static int Hook_Write(byte *at, const void *target, uint32_t wipe) {
	DWORD old;
	SIZE_T n = 5 + wipe;

	if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) {
		return 0;
	}
	at[0] = 0xe9;
	int32_t rel = (int32_t)((uintptr_t)target - ((uintptr_t)at + 5));
	memcpy(at + 1, &rel, sizeof(rel));
	memset(at + 5, 0xcc, wipe);
	VirtualProtect(at, n, old, &old);
	FlushInstructionCache(GetCurrentProcess(), at, n);
	return 1;
}

/* the game and engine code (below is Python 1.5.2, above the CRT and the other libraries) */
#define GAME_CODE_START 0x05f6d8
#define GAME_CODE_END 0x2d6010

/* still the exe's: Sleep and busy waits (docs/standalone.md) */
static const struct {
	uint32_t rva, size;
} keep[] = {
	{ 0x22f940, 1039 },	/* CMotorApp vf06, the frame with its limiter */
	{ 0x22fe60, 381 },	/* the device reset with Sleep(250) */
	{ 0x272f90, 171 },	/* Tsc_CpuFreq */
	{ 0x276ba0, 55 },	/* Timer_CountTicks */
	{ 0x276d00, 320 },	/* Timer_GetQPFreq */
	{ 0x276e40, 72 },	/* Timer_MeasureQP */
	{ 0x280d10, 70 },	/* Video_WaitKeysUp */
};

static void Hook_WipeGame(void) {
	byte *code = (byte *)(game_base + GAME_CODE_START);
	DWORD old;

	VirtualProtect(code, GAME_CODE_END - GAME_CODE_START, PAGE_EXECUTE_READWRITE, &old);
	uint32_t at = GAME_CODE_START;
	for (size_t i = 0; i <= sizeof(keep) / sizeof(keep[0]); i++) {
		uint32_t end = i < sizeof(keep) / sizeof(keep[0]) ? keep[i].rva : GAME_CODE_END;
		memset((byte *)game_base + at, 0xcc, end - at);
		if (i < sizeof(keep) / sizeof(keep[0])) {
			at = keep[i].rva + keep[i].size;
		}
	}
	VirtualProtect(code, GAME_CODE_END - GAME_CODE_START, old, &old);
	FlushInstructionCache(GetCurrentProcess(), code, GAME_CODE_END - GAME_CODE_START);
}

static LONG CALLBACK Hook_Trap(EXCEPTION_POINTERS *e) {
	uintptr_t at = (uintptr_t)e->ExceptionRecord->ExceptionAddress;

	if (e->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT && at - game_base < 0x400000) {
		Com_Printf("wipe: original code reached at rva %06x\n", (unsigned)(at - game_base));
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

void Hook_InstallAll(int wipe) {
	int installed = 0, off = 0;

	if (wipe >= 2) {
		Hook_WipeGame();
	}
	for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
		const hook_t *h = &hooks[i];
		if (h->size < 5) {
			Com_Printf("hook %s: function is only %u bytes, not hooked\n", h->name, (unsigned)h->size);
			continue;
		}
		if (!Hook_Enabled(h->name)) {
			off++;
			continue;
		}
		if (!Hook_Write((byte *)(game_base + h->rva), h->func, wipe ? h->size - 5 : 0)) {
			Com_Printf("hook %s: VirtualProtect failed (%lu)\n", h->name, GetLastError());
			continue;
		}
		installed++;
	}
	Com_Printf("%d of %d hooks installed, %d switched off in scrapdecomp.ini\n", installed,
		(int)(sizeof(hooks) / sizeof(hooks[0])), off);
	if (wipe) {
		AddVectoredExceptionHandler(1, Hook_Trap);
		Com_Printf(wipe >= 2 ? "wipe: the game code is int3 but for the hooks and %d functions\n"
			: "wipe: the rest of every hooked original is int3\n", (int)(sizeof(keep) / sizeof(keep[0])));
	}
}
