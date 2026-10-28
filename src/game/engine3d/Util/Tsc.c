/*
 * The rdtsc timers of the engine3d profiler, original source Util/tsc.cpp (include/scrap/prof.h): per section the
 * ticks of the frame, their average over n frames and the maximum, in milliseconds, and the measurement of the
 * processor clock (Tsc_CpuFreq, 200 ms of busy waiting).
 */
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/prof.h"
#include "scrap/win32.h"

#define PROF_SECTIONS 32

#define g_ProfAvg ((int64_t *)Game_Ptr(0x4b18e0))	/* the ticks of the last frame or the average */
#define g_ProfMax ((int64_t *)Game_Ptr(0x4b19e0))
#define g_ProfFrames ((byte *)Game_Ptr(0x4b1ae0))	/* the frames summed into g_ProfTicks */
#define g_ProfCounts ((int *)Game_Ptr(0x4b1b00))	/* Prof_Count */
#define g_ProfLastCounts ((int *)Game_Ptr(0x4b1b80))

/* the x87 division of the original (fdivr QWORD) in the precision the game set, stored as a float */
static float X87_DivToFloat(double num, float den) {
	long double r;

	__asm__("fdivrl %2" : "=t"(r) : "0"((long double)den), "m"(num));
	return (float)r;
}

int Prof_Count(int section, int n) {
	g_ProfCounts[section] += n;
	return g_ProfCounts[section];
}

int64_t Prof_Avg(uint32_t section, uint32_t frames) {
	g_ProfFrames[section]++;
	if (g_ProfFrames[section] == frames) {
		if (section >= PROF_SECTIONS) {
			crt_report_rangecheckfailure();
		}
		g_ProfFrames[section] = 0;
		g_ProfAvg[section] = (int64_t)g_ProfTicks[section] / (int64_t)(int32_t)frames;
		g_ProfTicks[section] = 0;
	}
	return g_ProfAvg[section];
}

int64_t Prof_Avg32(uint32_t section) {
	return Prof_Avg(section, 0x20);
}

int64_t Prof_Reset(uint32_t section) {
	if (section >= PROF_SECTIONS) {
		crt_report_rangecheckfailure();
	}
	int64_t last = (int64_t)g_ProfTicks[section];
	int64_t mx = g_ProfMax[section];
	g_ProfAvg[section] = last;
	g_ProfFrames[section] = 0;
	g_ProfTicks[section] = 0;
	if (mx <= last) {
		mx = last;
	}
	g_ProfMax[section] = mx;
	return last;
}

int Prof_GetCount(int section, int reset) {
	if (reset) {
		int n = g_ProfCounts[section];
		g_ProfLastCounts[section] = n;
		g_ProfCounts[section] = 0;
		return n;
	}
	return g_ProfLastCounts[section];
}

float Prof_GetTime(int section, int reset) {
	int64_t t = reset ? Prof_Reset(section) : Prof_Avg(section, 0xffffffff);
	double ms = Int64_ToDouble(t) * 1000.0;
	return X87_DivToFloat(ms, Tsc_CpuFreq(0));
}

float Prof_GetMaxTime(int section, int reset) {
	int64_t t = g_ProfMax[section];
	if (reset) {
		g_ProfMax[section] = 0;
	}
	double ms = Int64_ToDouble(t) * 1000.0;
	return X87_DivToFloat(ms, Tsc_CpuFreq(0));
}

/* the rdtsc ticks per second (those of 200 ms of busy waiting times 5), measured once or again with recalc */
#define g_CpuFreq GAME_DATA(float, 0x4b1c00)

float Tsc_CpuFreq(int recalc) {
	if (g_CpuFreq == 0.0f || recalc) {
		uint64_t t0 = __builtin_ia32_rdtsc();
		uint32_t ms = exe_timeGetTime();
		while (exe_timeGetTime() - ms < 200) {
		}
		uint64_t t1 = __builtin_ia32_rdtsc();
		float f = (float)Int64_ToDouble((int64_t)(t1 - t0)) * 5.0f;
		g_CpuFreq = f;
		CHECK(f != 0.0f);
	}
	return g_CpuFreq;
}
