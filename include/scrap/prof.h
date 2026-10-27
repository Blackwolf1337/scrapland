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

int Prof_Count(int section, int n);	/* rva 0x273040, a profiler counter, the new value */

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
