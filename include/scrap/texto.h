/*
 * Engine/texto.cpp of the engine3d: debug text on the window through the GDI (yellow, at a cursor that the printf
 * functions move), and a stack of messages shown at the top of the screen, each one for its time, five at most.
 */
#ifndef SCRAP_TEXTO_H
#define SCRAP_TEXTO_H

#include "scrap/mathlib.h"

typedef struct TextoMsg {
	uint32_t inicio;		/* 0x000 timeGetTime when it was first shown, 0 before */
	uint32_t duracion;		/* 0x004 ms */
	char texto[0x100];		/* 0x008 */
	struct TextoMsg *sig;		/* 0x108 */
} TextoMsg;
SD_SIZE(TextoMsg, 0x10c);

#define g_TextoX GAME_DATA(int, 0x4b1cf0)
#define g_TextoY GAME_DATA(int, 0x4b1cf4)
#define g_TextoPila GAME_DATA(TextoMsg *, 0x4b1cf8)

void Texto_Draw(int x, int y, const char *s);
/* the first message for its time, then the next ones under it */
void Texto_DrawPila(void);
/* a new message at the end (Mem_Calloc) */
TextoMsg *Texto_Push(void);
/* the first message off */
void Texto_Pop(void);
/* at the screen position of pos, nothing when it is not seen */
void Texto_Printf3D(const vec3_t *pos, const char *fmt, ...);
/* the cursor moved by dx, dy, then the text there (none for NULL) */
void Texto_PrintfRel(int dx, int dy, const char *fmt, ...);
void Texto_Printf(int x, int y, const char *fmt, ...);
/* a message for ms (its absolute value); a time that is not 0 drops the first message before */
void Texto_Mensaje(uint32_t ms, const char *fmt, ...);

/* other modules (src/imports.inc) */
/* rva 0x2170d0 (ScrapEngMain.cpp): text in font fnt (1 or 2), align 3 centered */
void Eng_Printf(int fnt, float x, float y, int align, uint32_t color, const char *fmt, ...);

#endif
