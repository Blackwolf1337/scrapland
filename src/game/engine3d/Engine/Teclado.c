/*
 * The keyboard state of the engine3d, original source Engine/teclado.cpp (include/scrap/entrada.h), and the paused
 * time next to it.
 */
#include <string.h>

#include "scrap/entrada.h"
#include "scrap/win32.h"

uint32_t Tiempo_Get(void) {
	return exe_timeGetTime() - g_TiempoPausado;
}

void Tiempo_Pausa(int on) {
	if (on) {
		if (!g_TiempoPausa) {
			g_TiempoPausa = exe_timeGetTime();
		}
	} else if (g_TiempoPausa) {
		g_TiempoReanudado = 1;
		g_TiempoPausado += exe_timeGetTime() - g_TiempoPausa;
		g_TiempoPausa = 0;
	}
}

void Teclado_Clear(void) {
	memset(g_TecladoKeys, 0, 256);
}

int Teclado_GetChars(uint16_t *dest, int n) {
	CHECK(dest);
	if (n <= 0) {
		return 0;
	}
	if (n >= TECLADO_BUF) {
		n = TECLADO_BUF;
	}
	for (int i = 0; i < n; i++) {
		dest[i] = g_TecladoBuf[(g_TecladoBufCount - n + g_TecladoBufStart + i) & (TECLADO_BUF - 1)];
	}
	return n;
}

/* the scan code in bits 16..23 of lparam with the extended bit 24 as bit 23; the right shift (0x36) becomes the
 * left one, the extended Ctrl (0x9d) and Alt (0x9c) the normal ones */
static uint32_t Teclado_ScanCode(uint32_t lparam) {
	uint32_t ext = lparam >> 1 & 0x800000;
	uint32_t k = ext | lparam;

	if ((ext | (lparam & 0xff0000)) == 0x360000) {
		k = (lparam & 0xff2affff) | 0x2a0000;
	}
	if ((k & 0xff0000) == 0x9d0000) {
		k = (k & 0xff1dffff) | 0x1d0000;
	}
	if ((k & 0xff0000) == 0x9c0000) {
		k = (k & 0xff1cffff) | 0x1c0000;
	}
	return k;
}

void Teclado_KeyDown(uint16_t ch, uint32_t lparam) {
	uint32_t k = Teclado_ScanCode(lparam);
	byte *key = &g_TecladoKeys[k >> 16 & 0xff];

	if (!*key) {
		*key = 1;
	}
	/* bit 30: the key was down before (a repeat) */
	if (!(k & 0x40000000)) {
		int i = g_TecladoBufStart + g_TecladoBufCount;
		g_TecladoBufCount++;
		g_TecladoBuf[i & (TECLADO_BUF - 1)] = ch;
	}
}

void Teclado_KeyUp(uint32_t wparam, uint32_t lparam) {
	(void)wparam;
	g_TecladoKeys[Teclado_ScanCode(lparam) >> 16 & 0xff] = 0;
}
