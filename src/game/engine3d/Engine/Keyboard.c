/*
 * The keyboard state of the engine3d, original source Engine/teclado.cpp (include/scrap/e3dinput.h), and the paused
 * time next to it.
 */
#include <string.h>

#include "scrap/e3dinput.h"
#include "scrap/platform.h"

/* the time of the engine without the pauses (next to teclado.cpp, its source is not known) */
static uint32_t g_TimePaused;

uint32_t Time_Get(void) {
	return Plat_Ticks() - g_TimePaused;
}

byte g_KeyboardKeys[256];

void Keyboard_Clear(void) {
	memset(g_KeyboardKeys, 0, 256);
}

static uint16_t g_KeyboardBuf[32];
static int g_KeyboardBufStart;
static int g_KeyboardBufCount;

/* the scan code in bits 16..23 of lparam with the extended bit 24 as bit 23; the right shift (0x36) becomes the
 * left one, the extended Ctrl (0x9d) and Alt (0x9c) the normal ones */
static uint32_t Keyboard_ScanCode(uint32_t lparam) {
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

void Keyboard_KeyDown(uint16_t ch, uint32_t lparam) {
	uint32_t k = Keyboard_ScanCode(lparam);
	byte *key = &g_KeyboardKeys[k >> 16 & 0xff];

	if (!*key) {
		*key = 1;
	}
	/* bit 30: the key was down before (a repeat) */
	if (!(k & 0x40000000)) {
		int i = g_KeyboardBufStart + g_KeyboardBufCount;
		g_KeyboardBufCount++;
		g_KeyboardBuf[i & (KEYBOARD_BUF - 1)] = ch;
	}
}

void Keyboard_KeyUp(uint32_t wparam, uint32_t lparam) {
	(void)wparam;
	g_KeyboardKeys[Keyboard_ScanCode(lparam) >> 16 & 0xff] = 0;
}
