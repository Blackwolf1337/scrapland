/*
 * Engine/LeeDatos.cpp of the engine3d: reader of the text and binary data files of the maps (radiosity, map
 * data). Files are pushed on a stack; text lines lose "#" comments and trailing blanks, empty lines are skipped.
 * g_LeeCad is the read position in the line. Errors are fatal.
 */
#ifndef SCRAP_LEEDATOS_H
#define SCRAP_LEEDATOS_H

#include "scrap/game.h"

#define LEE_LINE 0x400

typedef struct cLeePila {
	char name[MAX_RUTA];		/* 0x000 dir and name */
	void *arch;			/* 0x104 ArchIO_Open */
	char line[LEE_LINE];		/* 0x108 */
	char *cad;			/* 0x508 g_LeeCad of the file below */
	int err;			/* 0x50c errno before the push */
	struct cLeePila *prev;		/* 0x510 */
} cLeePila;
SD_SIZE(cLeePila, 0x514);

#define g_LeeCad GAME_GLOBAL(char *, g_LeeCad, 0x4e73b8)
#define g_LeePila GAME_GLOBAL(cLeePila *, g_LeePila, 0x4e73bc)

/* dir (may be NULL) and name, binary "rb" else "rt" */
void Lee_Open(const char *dir, const char *name, int binary);
/* errno after closing */
int Lee_Close(void);
void Lee_Pop(void);
/* the next line with text into g_LeeCad: 0, -1 at the end */
int Lee_NextLine(void);
/* the next line must start with tok and a blank, g_LeeCad then is at the value */
void Lee_Expect(const char *tok);
/* "text" at g_LeeCad */
void Lee_String(char *dest, int size);
/* with tok NULL the value of the current line */
void Lee_Text(const char *tok, char *dest, int size);
void Lee_QText(const char *tok, char *dest, int size);
void Lee_Float(const char *tok, float *dest);
void Lee_Int(const char *tok, int *dest);
void Lee_Bytes(void *dest, int n);
int Lee_Byte(void);
void Lee_Error(const char *fmt, ...);

#endif
