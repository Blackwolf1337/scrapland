/*
 * The data probe of ScrapC.exe, [general] dataprobe (docs/standalone.md, stage 3b2): which data of the original the
 * run reads and writes. The pages of the original's data in the room (ROOM_PROBE of room.h: no code, no resources,
 * not the page of the import address table) are PAGE_NOACCESS. A vectored handler logs the first read and the first
 * write of each 4 byte unit with the eip, then lets the access through: the page open again, one single step with the
 * trap flag, the page closed again. For an eip outside ScrapC.exe (the UCRT, a system DLL) it also logs the first
 * return address into the code of ScrapC.exe on the stack, the C function that called it.
 *
 * dataprobe=1: per page. The first access of a page is logged and the page stays open.
 * dataprobe=2: per unit. A page that faulted dataprobebudget times (default 20000) stays open from then on and is
 * logged as retired, so the hot pages do not make the run too slow. Its later accesses are not seen.
 *
 * Another thread that reaches a page while it is open for one step is not seen either, and Wine reports the access
 * of a system call (unix side) to a closed page as an error of the call, not as an exception.
 *
 * The log is dataprobe.log next to the exe, written new at every start, one WriteFile per line, so a run stopped with
 * wineserver -k keeps it. tools/standalone/datamap.py reads it (all numbers hex):
 *   start <mode> <budget>
 *   m <base> <path>            a module that an eip lies in
 *   u <rva> <r|w> <eip> <ret>  the first read or write of a unit, ret 0 when the eip is in ScrapC.exe
 *   p <rva> <faults>           a page retired (dataprobe=2) or opened (dataprobe=1)
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "scrap/game.h"
#include "room.h"

void DataProbe_Start(const char *dir);
int Game_Option(const char *key);
int Game_OptionOr(const char *key, int def);

#define PROBE_PAGES (GAME_SIZEOFIMAGE >> 12)
#define PROBE_TF 0x100

enum {
	PAGE_FREE,	/* not watched */
	PAGE_WATCHED,
	PAGE_OPEN	/* retired, or dataprobe=1 after its first access */
};

static int probe_mode, probe_budget;
static HANDLE probe_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION probe_lock;
static DWORD probe_tls;	/* per thread: the pages open for its single step, each page + 1 in 16 bits */
static byte probe_page[PROBE_PAGES];
static uint32_t probe_faults[PROBE_PAGES];
static byte *probe_seen;	/* per unit: 1 read logged, 2 write logged */
static uintptr_t own_text, own_text_end;
static uintptr_t probe_mods[64];
static int probe_nmods;

static char *Probe_Hex(char *p, uint32_t v) {
	char tmp[8];
	int n = 0;

	do {
		tmp[n++] = "0123456789abcdef"[v & 15];
		v >>= 4;
	} while (v);
	while (n) {
		*p++ = tmp[--n];
	}
	return p;
}

static void Probe_Write(const char *s, size_t n) {
	DWORD written;

	if (probe_file != INVALID_HANDLE_VALUE) {
		WriteFile(probe_file, s, (DWORD)n, &written, NULL);
	}
}

static void Probe_Page(uint32_t page, uint32_t faults) {
	char line[32], *p = line;

	*p++ = 'p';
	*p++ = ' ';
	p = Probe_Hex(p, page << 12);
	*p++ = ' ';
	p = Probe_Hex(p, faults);
	*p++ = '\n';
	Probe_Write(line, (size_t)(p - line));
}

/* the module of an eip outside ScrapC.exe, logged once per module */
static void Probe_Module(uintptr_t eip) {
	MEMORY_BASIC_INFORMATION mbi;
	char line[MAX_PATH + 16], *p = line;

	if (!VirtualQuery((void *)eip, &mbi, sizeof(mbi))) {
		return;
	}
	uintptr_t base = (uintptr_t)mbi.AllocationBase;
	for (int i = 0; i < probe_nmods; i++) {
		if (probe_mods[i] == base) {
			return;
		}
	}
	if (probe_nmods == (int)(sizeof(probe_mods) / sizeof(probe_mods[0]))) {
		return;
	}
	probe_mods[probe_nmods++] = base;
	*p++ = 'm';
	*p++ = ' ';
	p = Probe_Hex(p, (uint32_t)base);
	*p++ = ' ';
	DWORD n = GetModuleFileNameA((HMODULE)base, p, MAX_PATH);
	p += n < MAX_PATH ? n : 0;
	*p++ = '\n';
	Probe_Write(line, (size_t)(p - line));
}

/* a return address: right after a call (rel32, or through a register or memory operand) */
static int Probe_IsReturn(uint32_t ret) {
	const byte *p = (const byte *)(uintptr_t)ret;

	if (ret < own_text + 8 || ret > own_text_end) {
		return 0;
	}
	return p[-5] == 0xe8 || (p[-2] == 0xff && (p[-1] & 0x38) == 0x10) || (p[-3] == 0xff && (p[-2] & 0x38) == 0x10)
		|| (p[-6] == 0xff && (p[-5] & 0x38) == 0x10) || (p[-7] == 0xff && (p[-6] & 0x38) == 0x10);
}

/* the first return address into ScrapC.exe on the stack of a call from it into another module (a stale one in an
 * uninitialized local can come first) */
static uint32_t Probe_Caller(const CONTEXT *c) {
	const uint32_t *sp = (const uint32_t *)(uintptr_t)c->Esp;
	const uint32_t *top = (const uint32_t *)((NT_TIB *)NtCurrentTeb())->StackBase;

	for (int i = 0; i < 256 && sp + i < top; i++) {
		if (Probe_IsReturn(sp[i])) {
			return sp[i];
		}
	}
	return 0;
}

static void Probe_Unit(uint32_t rva, int write, const CONTEXT *c) {
	char line[64], *p = line;
	uintptr_t eip = c->Eip;
	uint32_t ret = 0;

	if (eip < own_text || eip >= own_text_end) {
		Probe_Module(eip);
		ret = Probe_Caller(c);
	}
	*p++ = 'u';
	*p++ = ' ';
	p = Probe_Hex(p, rva);
	*p++ = ' ';
	*p++ = write ? 'w' : 'r';
	*p++ = ' ';
	p = Probe_Hex(p, (uint32_t)eip);
	*p++ = ' ';
	p = Probe_Hex(p, ret);
	*p++ = '\n';
	Probe_Write(line, (size_t)(p - line));
}

static void Probe_Protect(uint32_t page, DWORD prot) {
	DWORD old;

	VirtualProtect((byte *)game_base + (page << 12), 0x1000, prot, &old);
}

static LONG CALLBACK Probe_Handler(EXCEPTION_POINTERS *e) {
	const EXCEPTION_RECORD *r = e->ExceptionRecord;
	CONTEXT *c = e->ContextRecord;

	if (r->ExceptionCode == EXCEPTION_SINGLE_STEP) {
		uintptr_t pend = (uintptr_t)TlsGetValue(probe_tls);
		if (!pend) {
			return EXCEPTION_CONTINUE_SEARCH;
		}
		TlsSetValue(probe_tls, NULL);
		EnterCriticalSection(&probe_lock);
		for (; pend; pend >>= 16) {
			uint32_t page = (uint32_t)(pend & 0xffff) - 1;
			if (probe_page[page] == PAGE_WATCHED) {
				Probe_Protect(page, PAGE_NOACCESS);
			}
		}
		LeaveCriticalSection(&probe_lock);
		c->EFlags &= ~PROBE_TF;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	uint32_t rva = (uint32_t)(r->ExceptionInformation[1] - game_base);
	if (rva >= GAME_SIZEOFIMAGE || probe_page[rva >> 12] == PAGE_FREE) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	uint32_t page = rva >> 12;
	EnterCriticalSection(&probe_lock);
	if (probe_page[page] == PAGE_OPEN) {
		/* opened by another thread after this access faulted */
		LeaveCriticalSection(&probe_lock);
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	byte bit = r->ExceptionInformation[0] == 1 ? 2 : 1;
	if (!(probe_seen[rva >> 2] & bit)) {
		probe_seen[rva >> 2] |= bit;
		Probe_Unit(rva, bit == 2, c);
	}
	uintptr_t pend = (uintptr_t)TlsGetValue(probe_tls);
	if (probe_mode == 1 || ++probe_faults[page] >= (uint32_t)probe_budget || pend >> 16) {
		/* a third page for one instruction (a string move across pages) retires it too */
		probe_page[page] = PAGE_OPEN;
		Probe_Protect(page, PAGE_EXECUTE_READWRITE);
		Probe_Page(page, probe_faults[page]);
		LeaveCriticalSection(&probe_lock);
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	Probe_Protect(page, PAGE_EXECUTE_READWRITE);
	LeaveCriticalSection(&probe_lock);
	TlsSetValue(probe_tls, (void *)(pend << 16 | (page + 1)));
	c->EFlags |= PROBE_TF;
	return EXCEPTION_CONTINUE_EXECUTION;
}

/* the code section of ScrapC.exe, to tell its eips from those of other modules */
static void Probe_OwnText(void) {
	const byte *own = (const byte *)GetModuleHandleA(NULL);
	const IMAGE_NT_HEADERS32 *nt = (const void *)(own + ((const IMAGE_DOS_HEADER *)own)->e_lfanew);
	const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);

	for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
		if (!memcmp(s->Name, ".text", 6)) {
			own_text = (uintptr_t)own + s->VirtualAddress;
			own_text_end = own_text + s->Misc.VirtualSize;
		}
	}
}

void DataProbe_Start(const char *dir) {
	static const struct {
		uint32_t lo, hi;
	} watch[] = ROOM_PROBE;
	char path[MAX_PATH], line[64];
	int pages = 0;

	probe_mode = Game_Option("dataprobe");
	if (!probe_mode) {
		return;
	}
	probe_budget = Game_OptionOr("dataprobebudget", 20000);
	if (snprintf(path, sizeof(path), "%sdataprobe.log", dir) >= (int)sizeof(path)) {
		return;
	}
	probe_seen = VirtualAlloc(NULL, GAME_SIZEOFIMAGE >> 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	probe_tls = TlsAlloc();
	probe_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!probe_seen || probe_tls == TLS_OUT_OF_INDEXES || probe_file == INVALID_HANDLE_VALUE) {
		Com_Printf("dataprobe: cannot start (%lu)\n", GetLastError());
		return;
	}
	Probe_OwnText();
	InitializeCriticalSection(&probe_lock);
	snprintf(line, sizeof(line), "start %x %x\n", (unsigned)probe_mode, (unsigned)probe_budget);
	Probe_Write(line, strlen(line));
	AddVectoredExceptionHandler(1, Probe_Handler);
	for (size_t i = 0; i < sizeof(watch) / sizeof(watch[0]); i++) {
		for (uint32_t page = watch[i].lo >> 12; page << 12 < watch[i].hi; page++) {
			if (page == ROOM_IAT_RVA >> 12 || page == (ROOM_IAT_RVA + ROOM_IAT_SIZE - 1) >> 12
				|| probe_page[page] != PAGE_FREE) {
				continue;
			}
			probe_page[page] = PAGE_WATCHED;
			pages++;
		}
	}
	for (uint32_t page = 0; page < PROBE_PAGES; page++) {
		if (probe_page[page] == PAGE_WATCHED) {
			Probe_Protect(page, PAGE_NOACCESS);
		}
	}
	Com_Printf("dataprobe: %d pages of data watched, %s, log in %s\n", pages,
		probe_mode == 1 ? "per page" : "per unit", path);
}
