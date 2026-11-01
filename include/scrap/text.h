/*
 * Engine/texto.cpp of the engine3d: debug text on the window through the GDI (yellow, at a cursor that the printf
 * functions move), and a stack of messages shown at the top of the screen, each one for its time, five at most.
 */
#ifndef SCRAP_TEXT_H
#define SCRAP_TEXT_H

#include "scrap/mathlib.h"

typedef struct TextMsg {
	uint32_t start;		/* 0x000 timeGetTime when it was first shown, 0 before */
	uint32_t duration;		/* 0x004 ms */
	char text[0x100];		/* 0x008 */
	struct TextMsg *next;		/* 0x108 */
} TextMsg;
SD_SIZE(TextMsg, 0x10c);

extern int g_TextX;
extern int g_TextY;
extern TextMsg *g_TextStack;

void Text_Draw(int x, int y, const char *s);
/* the first message for its time, then the next ones under it */
void Text_DrawStack(void);
/* a new message at the end (Mem_Calloc) */
TextMsg *Text_Push(void);
/* the first message off */
void Text_Pop(void);
/* at the screen position of pos, nothing when it is not seen */
void Text_Printf3D(const vec3_t *pos, const char *fmt, ...);
/* the cursor moved by dx, dy, then the text there (none for NULL) */
void Text_PrintfRel(int dx, int dy, const char *fmt, ...);
void Text_Printf(int x, int y, const char *fmt, ...);
/* a message for ms (its absolute value); a time that is not 0 drops the first message before */
void Text_Message(uint32_t ms, const char *fmt, ...);

/* other modules (src/imports.inc) */
/* rva 0x2170d0 (ScrapEngMain.cpp): text in font fnt (1 or 2), align 3 centered */
void Eng_Printf(int fnt, float x, float y, int align, uint32_t color, const char *fmt, ...);

#endif
