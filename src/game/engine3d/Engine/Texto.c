/*
 * Engine/texto.cpp of the engine3d: debug text, see include/scrap/texto.h.
 */
#include <stdarg.h>

#include "scrap/e3dutil.h"
#include "scrap/global.h"
#include "scrap/sistpart.h"
#include "scrap/texto.h"
#include "scrap/win32.h"

#define TEXTO_BUF 0x400
#define TEXTO_PILA_MAX 5

void Texto_Draw(int x, int y, const char *s) {
	void *dc = exe_GetDC(MOTOR_HWND(g_Motor));
	if (dc) {
		exe_SetTextColor(dc, 0xffff);
		exe_SetBkMode(dc, 1);
		exe_ExtTextOutA(dc, x, y, 0, NULL, s, (unsigned)exe_lstrlenA(s), NULL);
		exe_ReleaseDC(MOTOR_HWND(g_Motor), dc);
	}
}

void Texto_DrawPila(void) {
	if (!g_TextoPila) {
		return;
	}
	if (!g_TextoPila->inicio) {
		g_TextoPila->inicio = exe_timeGetTime();
	}
	if (exe_timeGetTime() > g_TextoPila->duracion + g_TextoPila->inicio) {
		Texto_Pop();
	}
	const TextoMsg *m = g_TextoPila;
	if (!m) {
		return;
	}
	float y = 40.0f;
	int w, h;
	Grafico2D_GetPantalla(&w, &h);
	for (int i = 0; m && i < TEXTO_PILA_MAX; i++) {
		Eng_Printf(2, (float)w * 0.5f, y, 3, 0xffffff, Game_Ptr(0x3bae88), m->texto);	/* "%s" */
		m = m->sig;
		y = (float)(int)((float)h * 0.05f) + y;
	}
}

TextoMsg *Texto_Push(void) {
	TextoMsg **slot = &g_TextoPila;
	while (*slot) {
		slot = &(*slot)->sig;
	}
	*slot = Mem_Calloc(sizeof(TextoMsg));
	return *slot;
}

void Texto_Pop(void) {
	CHECK(g_TextoPila);
	TextoMsg *m = g_TextoPila;
	g_TextoPila = m->sig;
	Mem_Free(m);
}

void Texto_Printf3D(const vec3_t *pos, const char *fmt, ...) {
	char buf[TEXTO_BUF];
	float xy[2];

	if (!fmt || Render_ToScreen(pos, xy)) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	g_TextoY = (int)xy[1];
	g_TextoX = (int)xy[0];
	Texto_Draw(g_TextoX, g_TextoY, buf);
}

/* the text at the cursor */
static void Texto_VDraw(const char *fmt, va_list ap) {
	char buf[TEXTO_BUF];

	VBprintf(buf, sizeof(buf), fmt, ap);
	Texto_Draw(g_TextoX, g_TextoY, buf);
}

void Texto_PrintfRel(int dx, int dy, const char *fmt, ...) {
	g_TextoX += dx;
	g_TextoY += dy;
	if (fmt) {
		va_list ap;
		va_start(ap, fmt);
		Texto_VDraw(fmt, ap);
		va_end(ap);
	}
}

void Texto_Printf(int x, int y, const char *fmt, ...) {
	g_TextoX = x;
	g_TextoY = y;
	if (fmt) {
		va_list ap;
		va_start(ap, fmt);
		Texto_VDraw(fmt, ap);
		va_end(ap);
	}
}

void Texto_Mensaje(uint32_t ms, const char *fmt, ...) {
	if (g_TextoPila && ms) {
		Texto_Pop();
	}
	TextoMsg *m = Texto_Push();
	va_list ap;
	va_start(ap, fmt);
	VBprintf(m->texto, sizeof(m->texto), fmt, ap);
	va_end(ap);
	m->inicio = 0;
	m->duracion = (int)ms < 0 ? 0u - ms : ms;
}
