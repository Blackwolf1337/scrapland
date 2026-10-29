/*
 * Installs the hooks of src/hooks.inc in Scrap.exe (the hook DLL only; ScrapC.exe has no exe to hook since stage 3c2
 * of docs/standalone.md): a 5 byte "jmp rel32" at the start of each original function. The C version replaces the
 * original completely, there is no trampoline back into it.
 *
 * With [general] wipe=1 the rest of each hooked original is filled with int3, so a jump into the middle of
 * replaced code stops the game with its rva in the log. wipe=2 fills the whole game and engine code with int3 first.
 * wipe=3 is the same with a probe: each trap logs its rva and caller, copies the piece of original code there back
 * and goes on, so one run lists every piece of game code without a C version that the run reaches.
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

/* the game and engine code (below it the dynamic initializers of the game and the libraries; above it the M3D math,
 * Python 1.5.2 and the CRT) */
#define GAME_CODE_START 0x05f6d8
#define GAME_CODE_END 0x2d6010

/* the code the probe covers */
static const uint32_t probe_start = GAME_CODE_START, probe_end = GAME_CODE_END;

static void Hook_Wipe(uint32_t start, uint32_t end) {
	byte *code = (byte *)(game_base + start);
	DWORD old;

	VirtualProtect(code, end - start, PAGE_EXECUTE_READWRITE, &old);
	memset(code, 0xcc, end - start);
	VirtualProtect(code, end - start, old, &old);
	FlushInstructionCache(GetCurrentProcess(), code, end - start);
}

/* wipe=3: a copy of the original code, and per byte whether it is in a hook (1, its jump 3) */
static byte *probe_orig, *probe_mark;

#define PROBE_HOOK 1
#define PROBE_JUMP 3

/* int3 padding of the original: up to the next 16 byte boundary or more of it */
static int Probe_Pad(uint32_t x) {
	return probe_orig[x - probe_start] == 0xcc
		&& ((x + 1) % 16 == 0 || x + 1 >= probe_end || probe_orig[x + 1 - probe_start] == 0xcc);
}

/* the piece of original code around the trap (up to padding or the edge of a hook) is copied back and run */
static LONG CALLBACK Hook_Probe(EXCEPTION_POINTERS *e) {
	uintptr_t at = (uintptr_t)e->ExceptionRecord->ExceptionAddress;
	uint32_t rva = (uint32_t)(at - game_base), lo, hi;
	DWORD old;

	if (e->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT || rva < probe_start || rva >= probe_end
		|| probe_orig[rva - probe_start] == 0xcc) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	byte m = probe_mark[rva - probe_start];
	for (lo = rva; lo > probe_start && probe_mark[lo - 1 - probe_start] == m && !Probe_Pad(lo - 1); lo--) {
	}
	for (hi = rva; hi < probe_end && probe_mark[hi - probe_start] == m && !Probe_Pad(hi); hi++) {
	}
	/* the return address when the trap is the first byte of a called function */
	uint32_t ret = *(uint32_t *)e->ContextRecord->Esp;
	Com_Printf("probe: rva %06x%s, piece %06x..%06x, [esp] %06x\n", (unsigned)rva, m ? " in a hook" : "",
		(unsigned)lo, (unsigned)hi, (unsigned)(ret - game_base));
	VirtualProtect((byte *)game_base + lo, hi - lo, PAGE_EXECUTE_READWRITE, &old);
	memcpy((byte *)game_base + lo, probe_orig + lo - probe_start, hi - lo);
	VirtualProtect((byte *)game_base + lo, hi - lo, old, &old);
	FlushInstructionCache(GetCurrentProcess(), (byte *)game_base + lo, hi - lo);
	e->ContextRecord->Eip = (DWORD)at;
	return EXCEPTION_CONTINUE_EXECUTION;
}

static void Hook_ProbeInit(void) {
	probe_orig = VirtualAlloc(NULL, probe_end - probe_start, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	probe_mark = VirtualAlloc(NULL, probe_end - probe_start, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!probe_orig || !probe_mark) {
		Com_Printf("probe: no memory\n");
		return;
	}
	memcpy(probe_orig, (byte *)game_base + probe_start, probe_end - probe_start);
	for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
		uint32_t r = hooks[i].rva, n = hooks[i].size;
		if (n >= 5 && r >= probe_start && r + n <= probe_end) {
			memset(probe_mark + r - probe_start, PROBE_HOOK, n);
			memset(probe_mark + r - probe_start, PROBE_JUMP, 5);
		}
	}
	AddVectoredExceptionHandler(1, Hook_Probe);
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

	if (wipe >= 3) {
		Hook_ProbeInit();
	}
	if (wipe >= 2) {
		Hook_Wipe(GAME_CODE_START, GAME_CODE_END);
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
		Com_Printf(wipe >= 2 ? "wipe: the game code is int3 but for the hooks\n"
			: "wipe: the rest of every hooked original is int3\n");
	}
}
