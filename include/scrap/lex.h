/*
 * The lexer of the engine3d text files (original source Util/Lex.cpp) and the token layer on top of it
 * (Engine/cmd_lex.cpp, used by the reader of engine.txt). One file at a time is read, "#inc" lines push included
 * files; all state is global in the exe.
 */
#ifndef SCRAP_LEX_H
#define SCRAP_LEX_H

#include "scrap/game.h"

#define MAX_LAST_CLEX 0x400

/* Lex_Next results, also cClex.type */
enum {
	CLEX_ERR = -1,			/* end of the file */
	CLEX_IDENT = 0,
	CLEX_INT = 1,
	CLEX_FLOAT = 2,
	CLEX_STRING = 3,		/* without the quotes */
	CLEX_CHAR = 4,			/* any other single character */
};

typedef struct cLexFile {
	char *name;			/* 0x00 as given to Lex_Open (String_Dup) */
	char *path;			/* 0x04 full path */
	void *f;			/* 0x08 FILE of the exe's CRT */
	int line;			/* 0x0c */
	int tokens;			/* 0x10 tokens on this line so far: '#' only starts a directive at 0 */
	int included;			/* 0x14 lines of the files included from this one */
	int savedCh;			/* 0x18 the character to go on with when an included file ends */
	struct cLexFile *parent;	/* 0x1c the including file */
	int escapes;			/* 0x20 '\\' escapes in strings */
} cLexFile;

/* the current token of cmd_lex */
typedef struct cClex {
	char *text;			/* 0x0 g_LexToken */
	int type;			/* 0x4 CLEX_ */
	union {
		int i;			/* CLEX_INT, the character for CLEX_CHAR */
		double d;		/* CLEX_FLOAT */
	} value;			/* 0x8 */
} cClex;

extern cLexFile *g_Lex;
extern int g_LexCh;	/* the next character, -1 at the end */
extern char g_LexToken[1024];	/* MAX_LAST_CLEX */
extern int g_LexTokenLen;
extern int g_LexErrors;
extern int g_LexWarnings;
extern char g_LexDir[260];	/* MAX_FILEPATH, where Lex_Open looks second */
extern cClex g_Cl;

typedef void (*lexout_t)(const char *text);
extern lexout_t g_LexOut;

/* Util/Lex.cpp */
void Lex_String(void);
void Lex_Directive(void);
void Lex_Hex(void);
void Lex_Ident(void);
void Lex_ReadUntil(int end);
int Lex_Number(void);	/* CLEX_INT or CLEX_FLOAT */
void Lex_Octal(void);
/* printf to g_LexOut; without one the original calls itself until the stack ends */
void Lex_Print(const char *fmt, ...);
void Lex_VPrint(const char *fmt, va_list ap);
void Lex_NewLine(void);
void Lex_Comment(void);	/* nested */
void Lex_SkipBlanks(void);
void Lex_SkipLine(void);
void Lex_NextCh(void);
void Lex_Add(int ch);
int Lex_Peek(void);
int Lex_Open(const char *name, int escapes);	/* 0 or -1 */
/* 2 always, 1 if there were errors or warnings: the summary line */
void Lex_Close(int summary);
/* level 0 error, 1 warning, 2 info; too many errors or warnings end the process */
void Lex_Error(int level, const char *fmt, ...);
void Lex_Summary(void);
void Lex_SetOut(lexout_t out);
void Lex_SetDir(const char *name);
int Lex_Next(void);

/* Engine/cmd_lex.cpp */
int CmdLex_Expect(int type);	/* 0 or -1 with an error */
int CmdLex_IsChar(int c);
int CmdLex_NeedChar(int c);
int CmdLex_GetInt(int *out);
int CmdLex_GetDouble(double *out);
void CmdLex_SkipBlock(int depth);
void CmdLex_SkipTo(int c);	/* past the character c */
void CmdLex_SkipToAny(const char *chars);	/* up to one of chars */
void CmdLex_Next(void);

#endif
