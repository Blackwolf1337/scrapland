/*
 * The rdtsc timers of the engine3d profiler, original source Util/tsc.cpp (include/scrap/prof.h): per section the
 * ticks of the frame, their average over n frames and the maximum, in milliseconds, and the measurement of the
 * processor clock (Tsc_CpuFreq, against the performance counter without the 200 ms busy wait of the original).
 */
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/prof.h"
#include "scrap/win32.h"

#define PROF_SECTIONS 32

#define g_ProfAvg (GAME_ARRAY(int64_t, g_ProfAvg, 0x4b18e0))	/* the ticks of the last frame or the average */
#define g_ProfMax (GAME_ARRAY(int64_t, g_ProfMax, 0x4b19e0))
#define g_ProfFrames (GAME_ARRAY(byte, g_ProfFrames, 0x4b1ae0))	/* the frames summed into g_ProfTicks */
#define g_ProfCounts (GAME_ARRAY(int, g_ProfCounts, 0x4b1b00))	/* Prof_Count */
#define g_ProfLastCounts (GAME_ARRAY(int, g_ProfLastCounts, 0x4b1b80))

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

/* the rdtsc ticks per second, measured once or again with recalc */
#define g_CpuFreq GAME_GLOBAL(float, g_CpuFreq, 0x4b1c00)

/* the measurement under way: rdtsc and the performance counter at its start, the value given until it ends */
static uint64_t tsc_start;
static int64_t tsc_qpcStart;
static float tsc_interim;	/* 0: no measurement under way */

/*
 * Differs from the original on purpose (no busy waits): the original counted rdtsc over 200 ms of busy waiting on
 * timeGetTime. Here the first call (and each recalc) starts the measurement and answers with the frequency of the
 * performance counter, the first call 200 ms or more later sets the rdtsc ticks per second of that time measured
 * with the performance counter. QPF alone is not the rate of rdtsc (10 MHz on current Windows and Wine), and a value
 * read once like the ~MHz of the registry is the nominal or (Wine) the boost clock; the measurement between two
 * calls needs no wait and is as exact as the original. The value only feeds the profiler statistics.
 */
float Tsc_CpuFreq(int recalc) {
	int64_t freq, now;

	if (g_CpuFreq == 0.0f || recalc) {
		tsc_start = __builtin_ia32_rdtsc();
		exe_QueryPerformanceCounter(&tsc_qpcStart);
		exe_QueryPerformanceFrequency(&freq);
		g_CpuFreq = (float)freq;
		tsc_interim = g_CpuFreq;
		CHECK(g_CpuFreq != 0.0f);
	} else if (tsc_interim != 0.0f) {
		if (g_CpuFreq != tsc_interim) {
			/* set by someone else in between */
			tsc_interim = 0.0f;
		} else {
			uint64_t ticks = __builtin_ia32_rdtsc() - tsc_start;
			exe_QueryPerformanceCounter(&now);
			exe_QueryPerformanceFrequency(&freq);
			now -= tsc_qpcStart;
			if (now >= freq / 5) {
				g_CpuFreq = (float)((double)ticks * (double)freq / (double)now);
				tsc_interim = 0.0f;
				CHECK(g_CpuFreq != 0.0f);
			}
		}
	}
	return g_CpuFreq;
}

