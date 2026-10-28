/*
 * The string and GUID helpers of the DX8 SDK framework (dxutil.cpp, include/scrap/dxutil.h; the generic strings
 * are ANSI) and the performance counter of the engine next to them.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/dxutil.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrapmain.h"
#include "scrap/win32.h"

/* the original copies without a bound for cch -1 */
void DXUtil_AnsiToGeneric(char *dst, const char *src, int cch) {
	if (!dst || !src || !cch) {
		return;
	}
	if (cch == -1) {
		do {
			*dst++ = *src;
		} while (*src++);
		return;
	}
	strncpy(dst, src, (size_t)cch);
	dst[cch - 1] = 0;
}

void DXUtil_AnsiToWide(uint16_t *dst, const char *src, int cch) {
	if (!dst || !src) {
		return;
	}
	if (cch == -1) {
		cch = (int)strlen(src) + 1;
	}
	exe_MultiByteToWideChar(0, 0, src, -1, dst, cch - 1);
	dst[cch - 1] = 0;
}

void DXUtil_GUIDToString(const D3DGuid *g, char *buf) {
	Str_PrintfQuiet(buf, 39, "{%0.8X-%0.4X-%0.4X-%0.2X%0.2X-%0.2X%0.2X%0.2X%0.2X%0.2X%0.2X}", g->Data1, g->Data2,
		g->Data3, g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6],
		g->Data4[7]);
}

void DXUtil_GenericToWide(uint16_t *dst, const char *src, int cch) {
	if (dst && src && cch) {
		DXUtil_AnsiToWide(dst, src, cch);
	}
}

int DXUtil_StringToGUID(const char *s, D3DGuid *g) {
	uint32_t d2, d3, b[8];

	if (crt_sscanf(s, "{%8X-%4X-%4X-%2X%2X-%2X%2X%2X%2X%2X%2X}", &g->Data1, &d2, &d3, &b[0], &b[1], &b[2], &b[3],
		&b[4], &b[5], &b[6], &b[7]) != 11) {
		memset(g, 0, sizeof(*g));
		return 0;
	}
	g->Data2 = (uint16_t)d2;
	g->Data3 = (uint16_t)d3;
	for (int i = 0; i < 8; i++) {
		g->Data4[i] = (byte)b[i];
	}
	return 1;
}

void DXUtil_WideToAnsi(char *dst, const uint16_t *src, int cch) {
	if (!dst || !src) {
		return;
	}
	if (cch == -1) {
		const uint16_t *p = src;
		while (*p) {
			p++;
		}
		cch = (int)(p - src) + 1;
	}
	exe_WideCharToMultiByte(0, 0, src, -1, dst, cch - 1, NULL, NULL);
	dst[cch - 1] = 0;
}

void DXUtil_WideToGeneric(char *dst, const uint16_t *src, int cch) {
	if (dst && src && cch) {
		DXUtil_WideToAnsi(dst, src, cch);
	}
}

/* the performance counter of the engine: since g_TimerBase, or the fixed time while one is set */
#define g_TimerFixed GAME_GLOBAL(int64_t, g_TimerFixed, 0x4b1c80)
#define g_TimerBase GAME_GLOBAL(int64_t, g_TimerBase, 0x4b1c88)

/* differs from the original on purpose (no busy waits): the original counted the distinct values of timeGetTime
 * during ms, which the 1 ms period of CWinApp_Init (timeBeginPeriod) makes ms; no caller in the exe */
int Timer_CountTicks(int ms) {
	return ms > 0 ? ms : 0;
}

int Timer_GetFreq(int64_t *out) {
	if (g_TimerFixed) {
		*out = g_TimerFixed;
		return 1;
	}
	int r = exe_QueryPerformanceCounter(out);
	*out -= g_TimerBase;
	return r;
}

int Timer_FixedStep(float fps) {
	int64_t t;

	if (fps == 0.0f) {
		/* back to the counter, from the fixed time on */
		if (g_TimerFixed) {
			exe_QueryPerformanceCounter(&t);
			g_TimerBase = t - g_TimerFixed;
		}
		g_TimerFixed = 0;
		return 1;
	}
	if (!g_TimerFixed) {
		exe_QueryPerformanceCounter(&t);
		t -= g_TimerBase;
		g_TimerFixed = t;
	}
	Timer_GetQPFreq(&t);
	g_TimerFixed += (int)((float)Int64_ToDouble(t) / fps);
	return 1;
}

/* the frequency of the performance counter times g_TimerQPMult, taken once (-1 until then), 0 without a counter */
#define g_TimerQPFreq GAME_GLOBAL(int64_t, g_TimerQPFreq, 0x432590)

int Timer_GetQPFreq(int64_t *out) {
	if (((uint32_t)g_TimerQPFreq & (uint32_t)((uint64_t)g_TimerQPFreq >> 32)) == 0xffffffff) {
		/* differs from the original on purpose (no busy waits): the original also measured the counter over 100 ms
		 * of timeGetTime, up to 10 times, and took 0 (timeGetTime) when it was not 1/10 of the frequency */
		g_TimerQPFreq = exe_QueryPerformanceFrequency(out) ? *out : 0;
		g_TimerQPFreq = (int64_t)((uint64_t)(uint32_t)g_TimerQPMult * (uint64_t)g_TimerQPFreq);
	}
	*out = g_TimerQPFreq;
	return g_TimerQPFreq != 0;
}

/* differs from the original on purpose (no busy waits): the counter ticks of ms from its frequency, not measured
 * (no caller in the exe) */
int64_t Timer_MeasureQP(int ms) {
	int64_t freq;

	if (ms <= 0 || !exe_QueryPerformanceFrequency(&freq)) {
		return 0;
	}
	return freq * ms / 1000;
}

/* the state of DXUtil_Timer: the counter path in ticks, the timeGetTime path in seconds */
#define g_DXTimerInit GAME_GLOBAL(int, g_DXTimerInit, 0x4b1c40)
#define g_DXTimerQP GAME_GLOBAL(int, g_DXTimerQP, 0x4b1c44)
#define g_DXTimerFreq GAME_GLOBAL(int64_t, g_DXTimerFreq, 0x4b1c48)
#define g_DXTimerStop GAME_GLOBAL(int64_t, g_DXTimerStop, 0x4b1c50)
#define g_DXTimerLast GAME_GLOBAL(int64_t, g_DXTimerLast, 0x4b1c58)
#define g_DXTimerBase GAME_GLOBAL(int64_t, g_DXTimerBase, 0x4b1c60)
#define g_DXTimerLastD GAME_GLOBAL(double, g_DXTimerLastD, 0x4b1c68)
#define g_DXTimerBaseD GAME_GLOBAL(double, g_DXTimerBaseD, 0x4b1c70)
#define g_DXTimerStopD GAME_GLOBAL(double, g_DXTimerStopD, 0x4b1c78)
#define g_DXTimerStopped GAME_GLOBAL(int, g_DXTimerStopped, 0x43258c)

/* the x87 a - b of the original (fsub QWORD) in the precision the game set, stored as a float */
static float X87_SubToFloat(double a, double b) {
	long double r;

	__asm__("fsubl %2" : "=t"(r) : "0"((long double)a), "m"(b));
	return (float)r;
}

static float DXTimer_Seconds(int64_t t, int64_t freq) {
	double n = Int64_ToDouble(t);
	return (float)(n / Int64_ToDouble(freq));
}

float __stdcall DXUtil_Timer(int cmd) {
	int64_t freq;

	if (!g_DXTimerInit) {
		g_DXTimerInit = 1;
		g_DXTimerQP = Timer_GetQPFreq(&freq);
		if (!g_DXTimerQP) {
			goto noqp;
		}
		g_DXTimerFreq = freq;
	}
	freq = g_DXTimerFreq;
	if (!g_DXTimerQP) {
		goto noqp;
	}
	int64_t t;
	if (g_DXTimerStop && cmd != DXUTIL_TIMER_START && cmd != DXUTIL_TIMER_GETABSOLUTETIME) {
		t = g_DXTimerStop;
	} else if (g_TimerFixed) {
		t = g_TimerFixed;
	} else {
		exe_QueryPerformanceCounter(&t);
		t -= g_TimerBase;
	}
	switch (cmd) {
	case DXUTIL_TIMER_GETELAPSEDTIME: {
		float r = DXTimer_Seconds(t - g_DXTimerLast, freq);
		g_DXTimerLast = t;
		return r;
	}
	case DXUTIL_TIMER_GETAPPTIME:
		return DXTimer_Seconds(t - g_DXTimerBase, freq);
	case DXUTIL_TIMER_RESET:
		g_DXTimerBase = t;
		g_DXTimerLast = t;
		g_DXTimerStop = 0;
		g_DXTimerStopped = 0;
		return 0.0f;
	case DXUTIL_TIMER_START:
		if (g_DXTimerStopped) {
			g_DXTimerBase += t - g_DXTimerStop;
		}
		g_DXTimerLast = t;
		g_DXTimerStop = 0;
		g_DXTimerStopped = 0;
		return 0.0f;
	case DXUTIL_TIMER_STOP:
		g_DXTimerStop = t;
		g_DXTimerLast = t;
		g_DXTimerStopped = 1;
		return 0.0f;
	case DXUTIL_TIMER_ADVANCE:
		g_DXTimerStop += freq / 10;
		return 0.0f;
	case DXUTIL_TIMER_GETABSOLUTETIME:
		return DXTimer_Seconds(t, freq);
	}
	return -1.0f;

noqp:;
	double stop = g_DXTimerStopD;
	double time;
	if (stop == 0.0 || cmd == DXUTIL_TIMER_START || cmd == DXUTIL_TIMER_GETABSOLUTETIME) {
		time = (double)exe_timeGetTime() * 0.001;
		stop = g_DXTimerStopD;
	} else {
		time = stop;
	}
	switch (cmd) {
	case DXUTIL_TIMER_GETELAPSEDTIME: {
		float r = X87_SubToFloat(time, g_DXTimerLastD);
		g_DXTimerLastD = time;
		return r;
	}
	case DXUTIL_TIMER_GETAPPTIME:
		return X87_SubToFloat(time, g_DXTimerBaseD);
	case DXUTIL_TIMER_RESET:
		g_DXTimerStopped = 0;
		g_DXTimerBaseD = time;
		g_DXTimerLastD = time;
		g_DXTimerStopD = 0.0;
		return 0.0f;
	case DXUTIL_TIMER_START:
		if (g_DXTimerStopped) {
			g_DXTimerBaseD = (time - stop) + g_DXTimerBaseD;
		}
		g_DXTimerStopD = 0.0;
		g_DXTimerLastD = time;
		g_DXTimerStopped = 0;
		return 0.0f;
	case DXUTIL_TIMER_STOP:
		g_DXTimerStopD = time;
		g_DXTimerLastD = time;
		g_DXTimerStopped = 1;
		return 0.0f;
	case DXUTIL_TIMER_ADVANCE:
		g_DXTimerStopD = stop + (double)0.1f;
		return 0.0f;
	case DXUTIL_TIMER_GETABSOLUTETIME:
		return (float)time;
	}
	return -1.0f;
}
