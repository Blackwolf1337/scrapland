/*
 * Engine/LeeDatos.cpp of the engine3d: reader of the text and binary data files of the maps (radiosity, map
 * data). Files are pushed on a stack; text lines lose "#" comments and trailing blanks, empty lines are skipped.
 * g_ReadString is the read position in the line. Errors are fatal.
 */
#ifndef SCRAP_READDATA_H
#define SCRAP_READDATA_H

#include "scrap/game.h"

#define READ_LINE 0x400

typedef struct cReadStack {
	char name[MAX_FILEPATH];		/* 0x000 dir and name */
	void *file;			/* 0x104 FileIO_Open */
	char line[READ_LINE];		/* 0x108 */
	char *string;			/* 0x508 g_ReadString of the file below */
	int err;			/* 0x50c errno before the push */
	struct cReadStack *prev;		/* 0x510 */
} cReadStack;
SD_SIZE(cReadStack, 0x514);

#define g_ReadString GAME_GLOBAL(char *, g_ReadString)
#define g_ReadStack GAME_GLOBAL(cReadStack *, g_ReadStack)

/* dir (may be NULL) and name, binary "rb" else "rt" */
void Read_Open(const char *dir, const char *name, int binary);
/* errno after closing */
int Read_Close(void);
void Read_Pop(void);
/* the next line with text into g_ReadString: 0, -1 at the end */
int Read_NextLine(void);
/* the next line must start with tok and a blank, g_ReadString then is at the value */
void Read_Expect(const char *tok);
/* "text" at g_ReadString */
void Read_String(char *dest, int size);
/* with tok NULL the value of the current line */
void Read_Text(const char *tok, char *dest, int size);
void Read_QText(const char *tok, char *dest, int size);
void Read_Float(const char *tok, float *dest);
void Read_Int(const char *tok, int *dest);
void Read_Bytes(void *dest, int n);
int Read_Byte(void);
void Read_Error(const char *fmt, ...);

#endif
