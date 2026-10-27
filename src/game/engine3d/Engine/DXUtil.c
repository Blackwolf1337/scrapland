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
#define g_TimerFixed GAME_DATA(int64_t, 0x4b1c80)
#define g_TimerBase GAME_DATA(int64_t, 0x4b1c88)

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
