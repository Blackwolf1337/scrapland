/*
 * The performance counter of the engine (next to the helpers of the DX8 SDK framework in dxutil.cpp of the original,
 * include/scrap/dxutil.h).
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/dxutil.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrapmain.h"
#include "scrap/platform.h"

/* the performance counter of the engine: since g_TimerBase, or the fixed time while one is set */

static int64_t g_TimerFixed;
static int64_t g_TimerBase;

int Timer_GetFreq(int64_t *out) {
	if (g_TimerFixed) {
		*out = g_TimerFixed;
		return 1;
	}
	int r = Plat_Counter(out);
	*out -= g_TimerBase;
	return r;
}

int Timer_FixedStep(float fps) {
	int64_t t;

	if (fps == 0.0f) {
		/* back to the counter, from the fixed time on */
		if (g_TimerFixed) {
			Plat_Counter(&t);
			g_TimerBase = t - g_TimerFixed;
		}
		g_TimerFixed = 0;
		return 1;
	}
	if (!g_TimerFixed) {
		Plat_Counter(&t);
		t -= g_TimerBase;
		g_TimerFixed = t;
	}
	Timer_GetQPFreq(&t);
	g_TimerFixed += (int)((float)Int64_ToDouble(t) / fps);
	return 1;
}

static int64_t g_TimerQPFreq = -1LL;

/* the frequency of the performance counter times g_TimerQPMult, taken once (-1 until then), 0 without a counter */

int Timer_GetQPFreq(int64_t *out) {
	if (((uint32_t)g_TimerQPFreq & (uint32_t)((uint64_t)g_TimerQPFreq >> 32)) == 0xffffffff) {
		/* differs from the original on purpose (no busy waits): the original also measured the counter over 100 ms
		 * of timeGetTime, up to 10 times, and took 0 (timeGetTime) when it was not 1/10 of the frequency */
		g_TimerQPFreq = Plat_Frequency(out) ? *out : 0;
		g_TimerQPFreq = (int64_t)((uint64_t)(uint32_t)g_TimerQPMult * (uint64_t)g_TimerQPFreq);
	}
	*out = g_TimerQPFreq;
	return g_TimerQPFreq != 0;
}

/* the state of DXUtil_Timer: the counter path in ticks, the timeGetTime path in seconds */

static float DXTimer_Seconds(int64_t t, int64_t freq) {
	double n = Int64_ToDouble(t);
	return (float)(n / Int64_ToDouble(freq));
}

static int g_DXTimerInit;
static int g_DXTimerQP;
static int64_t g_DXTimerFreq;
static int64_t g_DXTimerStop;
static int64_t g_DXTimerLast;
static int64_t g_DXTimerBase;
static double g_DXTimerLastD;
static double g_DXTimerBaseD;
static double g_DXTimerStopD;
static int g_DXTimerStopped = 1;

float DXUtil_Timer(int cmd) {
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
		Plat_Counter(&t);
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
		time = (double)Plat_Ticks() * 0.001;
		stop = g_DXTimerStopD;
	} else {
		time = stop;
	}
	switch (cmd) {
	case DXUTIL_TIMER_GETELAPSEDTIME: {
		float r = (float)(time - g_DXTimerLastD);
		g_DXTimerLastD = time;
		return r;
	}
	case DXUTIL_TIMER_GETAPPTIME:
		return (float)(time - g_DXTimerBaseD);
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
