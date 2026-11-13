/*
 * Shared between the test channel (TestChannel.c: the files, the commands) and its expressions (TestExpr.c). Not in
 * the original, test tools only.
 */
#ifndef SCRAP_TESTCHANNEL_H
#define SCRAP_TESTCHANNEL_H

#include "scrap/nativeobj.h"
#include "scrap/pyfunction.h"

/* the answer of a command file: a growing text (Mem_Realloc) */
typedef struct tcBuf {
	char *p;
	int len, cap;
} tcBuf;

void TC_Add(tcBuf *b, const char *s, int n);
void TC_Addf(tcBuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
/* OBJLIB 7: the pending error as "<type>: <value>" (normalized, as sys.exc_type and sys.exc_value in an except
 * clause), cleared */
void TC_AddError(tcBuf *b);

/* a name as a channel expression finds it at the top level: the globals of __main__, the builtins, the channel's own
 * functions, the loaded modules, else the module imported: 1 with *out (Native_Free it), 0 with the error set */
byte TcX_Name(const char *name, nativeVal_t *out);
/* a whole line as one expression in the globals of __main__: 1 with the value in *out (Native_Free it), 0 with the
 * error set */
byte TcX_Eval(const char *line, nativeVal_t *out);
/* an expression, or "target = expr" (the value None), in the globals of __main__: 1 with *out, 0 with the error set.
 * Attributes that fail without an error raise as the getattr and setattr builtins do (golden's check lines) */
byte TcX_Statement(const char *line, nativeVal_t *out);
/* the statements of line as TcX_Statement runs them, the name name giving v (as a global of __main__ would, without
 * one): 0 with the error set */
byte TcX_Bind(const char *line, const char *name, const nativeVal_t *v);
/* __main__.name = v (a variable of __main__ in the registry): 0 with the error set */
byte TcX_SetMain(const char *name, const nativeVal_t *v);
/* the statements of a line ("a = f(); b(); result = (a, b)") in the globals of a module dict (NULL: __main__),
 * answered as the game side of the MCP server (the old tools/mcp/mcpgame.py) answered a line: a single expression its
 * repr, statements the repr of the global "result" or ok, an error "error: <type>: <value>" (the statements after it
 * are not run); quiet: only an error answers */
void TcX_Run(tcBuf *b, const char *line, const nativeVal_t *globals, byte quiet);
/* a native function of the channel's verbs (the recorder): call(ctx, a, n, out) runs it on the arguments, drop(ctx)
 * (NULL: nothing) frees the context with the function. The evaluator calls it natively; a call with keywords raises
 * the TypeError of a C function. OBJLIB 9: its object is a C function object of Python named name on the self
 * (record, self[0], self[1], self[2]), the form of a lambda of the channel (repr "<built-in method name of tuple
 * object at ...>"; the module dump shows the 4-tuple as the data of a script function, "self=?" when self[1] is no
 * sequence), so the game calls it as any callable; OBJLIB 7: callers through Python (the game calling a module
 * variable) reach it through an adapter; the seams of the engine functions call its C function (Native_HookFinder). 1 with *out (Native_Free it), 0 with the error set (ctx
 * dropped) */
byte TcX_NewFunc(nativeVal_t *out, const char *name, nativeCall_t call, void *ctx, void (*drop)(void *ctx),
	const nativeVal_t self[3]);
/* the channel's functions (recorders, lambdas) as the C hooks of the seams they are put at (Native_HookFinder) */
void TcX_Init(void);
#endif
