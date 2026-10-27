/*
 * The profiler of the engine3d: per section a call counter (Prof_Count) and, with R_Profiler mode 5, the rdtsc ticks
 * spent in it. The original code times a section with a local object whose destructor adds the end ticks (only on
 * exceptions, the normal path is inlined).
 */
#ifndef SCRAP_PROF_H
#define SCRAP_PROF_H

#include "scrap/shared.h"

#define g_ProfMode GAME_DATA(int, 0x4a9018)	/* 5: timing with rdtsc */
#define g_ProfTicks ((uint64_t *)Game_Ptr(0x4b17e0))	/* per section */

/* Util/tsc.cpp (src/game/engine3d/Util/Tsc.c) */
/* the rdtsc ticks per millisecond, measured once (or again with recalc) over 200 ms of timeGetTime (rva 0x272f90,
 * src/imports.inc) */
float Tsc_CpuFreq(int recalc);
int Prof_Count(int section, int n);	/* a profiler counter, the new value */
/* the ticks of the section summed over frames calls, then their average (the last one in between) */
int64_t Prof_Avg(uint32_t section, uint32_t frames);
int64_t Prof_Avg32(uint32_t section);
/* the ticks of the frame, also into the maximum; the sum cleared */
int64_t Prof_Reset(uint32_t section);
/* the counter of the frame (reset: taken and cleared) */
int Prof_GetCount(int section, int reset);
/* the average (reset: the frame) and the maximum in milliseconds */
float Prof_GetTime(int section, int reset);
float Prof_GetMaxTime(int section, int reset);

static inline void Prof_Begin(int section) {
	if (g_ProfMode == 5) {
		g_ProfTicks[section] -= __builtin_ia32_rdtsc();
		Prof_Count(section, 1);
	}
}

static inline void Prof_End(int section) {
	if (g_ProfMode == 5) {
		g_ProfTicks[section] += __builtin_ia32_rdtsc();
	}
}

/* rva 0x212770 (ScrapEngCollision.cpp): the destructor of the timing object, the end ticks of its section */
void __fastcall ProfScope_End(const int *section);

#endif
