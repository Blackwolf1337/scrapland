/*
 * Installs the hooks of src/hooks.inc: a 5 byte "jmp rel32" at the start of each original function. The C version
 * replaces the original completely, there is no trampoline back into it.
 *
 * With [general] wipe=1 the rest of each hooked original is filled with int3, so a jump into the middle of
 * replaced code stops the game with its rva in the log (a check for the standalone build, docs/standalone.md).
 * wipe=2 fills the whole game and engine code with int3 first. wipe=3 is the same with a probe: each trap logs its
 * rva and caller, copies the piece of original code there back and goes on, so one run lists every piece of game
 * code without a C version that the run reaches. wipepy=1 (ScrapC.exe only, it runs the real Python of src/python)
 * also fills the code of the original's Python with int3, the probe of wipe=3 then covers it too. wipelow=1 (ScrapC.exe
 * only) does the same for the libraries below the game code (ASE, D3DX8) and the M3D math above it. wipeinit=1
 * (ScrapC.exe only) fills the dynamic initializers of the game and the atexit destructors they register with int3.
 *
 * ScrapC.exe also points the entries of the original's C runtime to the C of src/standalone (crthooks.inc), and
 * wipecrt=1 fills the rest of that runtime with int3 (the probe covers it too).
 */
#include <string.h>
#include <windows.h>

/*
 * Only shared.h: the engine headers declare the hooked functions with their real prototypes, which would clash with
 * the plain declarations below. Fastcall and stdcall names get their decoration through --enable-stdcall-fixup.
 */
#include "scrap/shared.h"
#include "scrap/origcrt.h"
#include "scrap/origpython.h"

extern uintptr_t game_base;

/* only the addresses are needed here, the real prototypes live in the implementing files */
#define SD_HOOK(name, rva, size) extern void name(void);
#include "hooks.inc"
#ifdef SD_STANDALONE
#include "standalone/crthooks.inc"
#endif
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
#ifdef SD_STANDALONE
#include "standalone/crthooks.inc"
#endif
#undef SD_HOOK
};

int Game_Option(const char *key);

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

/* wipelow: the libraries at the start of the code (the ASE SDK, D3DX8 with its zlib, libpng and libjpeg; before them
 * the data formats of dinput8.lib, after them the dynamic initializers of the game from rva 0x03e800 up to the game
 * code) and the M3D math between the game code and the Python, which ScrapC.exe has in C */
#define LIB_CODE_START 0x002070
#define LIB_CODE_END 0x03e800
#define M3D_CODE_END 0x2da770

/* wipeinit: the dynamic initializers of the game's globals (between the libraries and the game code) and the atexit
 * destructors at the end of the code, src/game/Global/StaticInit.c */
#define INIT_CODE_START 0x03e800
#define INIT_CODE_END GAME_CODE_START
#define DTOR_CODE_START 0x38c720
#define DTOR_CODE_END 0x38f097

/* the code the probe covers: the game code, with wipelow from the libraries up to the end of the M3D math, with
 * wipepy up to the end of the Python, with wipecrt up to the end of the CRT, with wipeinit from the initializers up to
 * the end of the destructors */
static uint32_t probe_start = GAME_CODE_START, probe_end = GAME_CODE_END;

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
	Com_Printf("probe: rva %06x%s, piece %06x..%06x, [esp] %06x\n", (unsigned)rva,
		m ? " in a hook" : rva >= DTOR_CODE_START ? " in an atexit destructor" : rva >= ORIG_CRT_CODE_START ? " in the CRT"
		: rva >= ORIG_PY_CODE_START ? " in the Python" : rva >= INIT_CODE_START && rva < INIT_CODE_END ? " in an initializer"
		: rva < GAME_CODE_START || rva >= GAME_CODE_END ? " in a library" : "",
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

/* wipecrt=1: the code of the original's C runtime int3 but for the import thunks of the game in it */
static int Hook_WipeCrt(void) {
#ifdef SD_STANDALONE
	static const struct {
		uint32_t lo, hi;
	} keep[] = ORIG_CRT_THUNKS;
	uint32_t at = ORIG_CRT_CODE_START;

	if (!Game_Option("wipecrt")) {
		return 0;
	}
	for (size_t i = 0; i < sizeof(keep) / sizeof(keep[0]); i++) {
		Hook_Wipe(at, keep[i].lo);
		at = keep[i].hi;
	}
	Hook_Wipe(at, ORIG_CRT_CODE_END);
	Com_Printf("wipecrt: the code of the original's CRT (rva %06x..%06x) is int3 but for the game's import thunks\n",
		ORIG_CRT_CODE_START, ORIG_CRT_CODE_END);
	return 1;
#else
	if (Game_Option("wipecrt")) {
		Com_Printf("wipecrt: only in ScrapC.exe, the hook DLL runs the CRT of the exe\n");
	}
	return 0;
#endif
}

/* wipeinit=1: ScrapC.exe only; in the hook DLL the exe ran its initializers before the hooks */
static int Hook_WipeInitRange(void) {
	int wipeinit = Game_Option("wipeinit");

#ifdef SD_STANDALONE
	if (wipeinit) {
		if (probe_start > INIT_CODE_START) {
			probe_start = INIT_CODE_START;
		}
		if (probe_end < DTOR_CODE_END) {
			probe_end = DTOR_CODE_END;
		}
	}
	return wipeinit;
#else
	if (wipeinit) {
		Com_Printf("wipeinit: only in ScrapC.exe, the exe of the hook DLL ran its initializers before the hooks\n");
	}
	return 0;
#endif
}

/* wipelow=1: ScrapC.exe only; the hook DLL has the same C for what the game calls, but its exe still runs the rest */
static int Hook_WipeLowRange(int wipelow) {
#ifdef SD_STANDALONE
	if (wipelow) {
		probe_start = LIB_CODE_START;
		if (probe_end < M3D_CODE_END) {
			probe_end = M3D_CODE_END;
		}
	}
	return wipelow;
#else
	if (wipelow) {
		Com_Printf("wipelow: only in ScrapC.exe, the hook DLL runs the D3DX8 of the exe\n");
	}
	return 0;
#endif
}

void Hook_InstallAll(int wipe, int wipepy, int wipelow) {
	int installed = 0, off = 0, wipecrt, wipeinit;

#ifdef SD_STANDALONE
	if (wipepy) {
		probe_end = ORIG_PY_CODE_END;
	}
	if (Game_Option("wipecrt")) {
		probe_end = ORIG_CRT_CODE_END;
	}
#else
	if (wipepy) {
		Com_Printf("wipepy: only in ScrapC.exe, the hook DLL runs the Python of the exe\n");
		wipepy = 0;
	}
#endif
	wipelow = Hook_WipeLowRange(wipelow);
	wipeinit = Hook_WipeInitRange();
	if (wipe >= 3) {
		Hook_ProbeInit();
	}
	if (wipe >= 2) {
		Hook_Wipe(GAME_CODE_START, GAME_CODE_END);
	}
	if (wipepy) {
		Hook_Wipe(ORIG_PY_CODE_START, ORIG_PY_CODE_END);
	}
	wipecrt = Hook_WipeCrt();
	if (wipelow) {
		Hook_Wipe(LIB_CODE_START, LIB_CODE_END);
		Hook_Wipe(GAME_CODE_END, M3D_CODE_END);
	}
	if (wipeinit) {
		Hook_Wipe(INIT_CODE_START, INIT_CODE_END);
		Hook_Wipe(DTOR_CODE_START, DTOR_CODE_END);
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
	if (wipe || wipepy || wipecrt || wipelow || wipeinit) {
		AddVectoredExceptionHandler(1, Hook_Trap);
	}
	if (wipe) {
		Com_Printf(wipe >= 2 ? "wipe: the game code is int3 but for the hooks\n"
			: "wipe: the rest of every hooked original is int3\n");
	}
	if (wipepy) {
		Com_Printf("wipepy: the code of the original's Python (rva %06x..%06x) is int3\n", ORIG_PY_CODE_START,
			ORIG_PY_CODE_END);
	}
	if (wipelow) {
		Com_Printf("wipelow: the code of the libraries (rva %06x..%06x) and the M3D math (rva %06x..%06x) is int3\n",
			LIB_CODE_START, LIB_CODE_END, GAME_CODE_END, M3D_CODE_END);
	}
	if (wipeinit) {
		Com_Printf("wipeinit: the initializers (rva %06x..%06x) and the atexit destructors (rva %06x..%06x) are int3\n",
			INIT_CODE_START, INIT_CODE_END, DTOR_CODE_START, DTOR_CODE_END);
	}
}
