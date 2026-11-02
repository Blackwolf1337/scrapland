/*
 * Engine/texto.cpp of the engine3d: debug text, see include/scrap/text.h.
 */
#include <stdarg.h>

#include "scrap/e3dutil.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/text.h"
#include "scrap/platform.h"

#define TEXT_BUF 0x400
#define TEXT_STACK_MAX 5

#define TEXT_QUEUE 64
#define TEXT_QUEUE_LEN 120

/* the texts of Text_Draw until the frame draws them */
static struct {
	int x, y;
	char text[TEXT_QUEUE_LEN];
} text_queue[TEXT_QUEUE];
static int text_queued;

/* the original wrote the text with GDI on the window at once; it waits for the 2D pass of the frame now */
void Text_Draw(int x, int y, const char *s) {
	if (text_queued < TEXT_QUEUE) {
		text_queue[text_queued].x = x;
		text_queue[text_queued].y = y;
		Str_Copy(text_queue[text_queued].text, s, TEXT_QUEUE_LEN);
		text_queued++;
	}
}

/* the texts of Text_Draw (yellow with a shadow in the font of the messages), then the message stack */
void Text_DrawStack(void) {
	for (int i = 0; i < text_queued; i++) {
		Eng_Printf(2, (float)text_queue[i].x, (float)text_queue[i].y, 1, 0xffff00, "%s", text_queue[i].text);
	}
	text_queued = 0;
	if (!g_TextStack) {
		return;
	}
	if (!g_TextStack->start) {
		g_TextStack->start = Plat_Ticks();
	}
	if (Plat_Ticks() > g_TextStack->duration + g_TextStack->start) {
		Text_Pop();
	}
	const TextMsg *m = g_TextStack;
	if (!m) {
		return;
	}
	float y = 40.0f;
	int w, h;
	Graphic2D_GetScreen(&w, &h);
	for (int i = 0; m && i < TEXT_STACK_MAX; i++) {
		Eng_Printf(2, (float)w * 0.5f, y, 3, 0xffffff, "%s", m->text);	/* "%s" */
		m = m->next;
		y = (float)(int)((float)h * 0.05f) + y;
	}
}

TextMsg *Text_Push(void) {
	TextMsg **slot = &g_TextStack;
	while (*slot) {
		slot = &(*slot)->next;
	}
	*slot = Mem_Calloc(sizeof(TextMsg));
	return *slot;
}

void Text_Pop(void) {
	CHECK(g_TextStack);
	TextMsg *m = g_TextStack;
	g_TextStack = m->next;
	Mem_Free(m);
}

void Text_Printf3D(const vec3_t *pos, const char *fmt, ...) {
	char buf[TEXT_BUF];
	float xy[2];

	if (!fmt || Render_ToScreen(pos, xy)) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	g_TextY = (int)xy[1];
	g_TextX = (int)xy[0];
	Text_Draw(g_TextX, g_TextY, buf);
}

/* the text at the cursor */
static void Text_VDraw(const char *fmt, va_list ap) {
	char buf[TEXT_BUF];

	VBprintf(buf, sizeof(buf), fmt, ap);
	Text_Draw(g_TextX, g_TextY, buf);
}

void Text_PrintfRel(int dx, int dy, const char *fmt, ...) {
	g_TextX += dx;
	g_TextY += dy;
	if (fmt) {
		va_list ap;
		va_start(ap, fmt);
		Text_VDraw(fmt, ap);
		va_end(ap);
	}
}

void Text_Printf(int x, int y, const char *fmt, ...) {
	g_TextX = x;
	g_TextY = y;
	if (fmt) {
		va_list ap;
		va_start(ap, fmt);
		Text_VDraw(fmt, ap);
		va_end(ap);
	}
}

void Text_Message(uint32_t ms, const char *fmt, ...) {
	if (g_TextStack && ms) {
		Text_Pop();
	}
	TextMsg *m = Text_Push();
	va_list ap;
	va_start(ap, fmt);
	VBprintf(m->text, sizeof(m->text), fmt, ap);
	va_end(ap);
	m->start = 0;
	m->duration = (int)ms < 0 ? 0u - ms : ms;
}
