/*
 * Shared between the test channel (TestChannel.c: the files, the commands) and its expressions (TestExpr.c). Not in
 * the original, test tools only.
 */
#ifndef SCRAP_TESTCHANNEL_H
#define SCRAP_TESTCHANNEL_H

#include "scrap/pyfunction.h"
#include "scrap/scriptnative.h"

/* the answer of a command file: a growing text (Mem_Realloc) */
typedef struct tcBuf {
	char *p;
	int len, cap;
} tcBuf;

void TC_Add(tcBuf *b, const char *s, int n);
void TC_Addf(tcBuf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
/* str() or repr() of o; a failing one gives "?" */
void TC_AddObj(tcBuf *b, PyObject *o, byte repr);
/* the pending error as "<type>: <value>" (normalized, as sys.exc_type and sys.exc_value in an except clause), cleared */
void TC_AddError(tcBuf *b);

/* the dict of __main__ */
PyObject *TcX_Main(void);
/* a name as a channel expression finds it at the top level: the globals of __main__, the builtins, the channel's own
 * functions, the loaded modules, else the module imported */
PyObject *TcX_Name(const char *name);
/* a whole line as one expression: the value, NULL with the error set */
PyObject *TcX_Eval(const char *line);
/* an expression, or "target = expr" (the value None), in the globals of __main__: NULL with the error set. Attributes
 * that fail without an error raise as the getattr and setattr builtins do (golden's check lines) */
PyObject *TcX_Statement(const char *line);
/* the statements of a line ("a = f(); b(); result = (a, b)") in the globals of a module dict, answered as the game
 * side of the MCP server (the old tools/mcp/mcpgame.py) answered a line: a single expression its repr, statements the
 * repr of the global "result" or ok, an error "error: <type>: <value>" (the statements after it are not run); quiet:
 * only an error answers */
void TcX_Run(tcBuf *b, const char *line, PyObject *globals, byte quiet);
/* what the expressions sent through Python since the start (OBJLIB 9), one line "channel <what> <count>" each added
 * to out: "builtin <name>" a builtin function that is not native here or was called in a form the native one does not
 * take, "operator <op>" an operator on a Python object or one the native values do not have, "keywords" a call with
 * keyword arguments of a callable that is no lambda; reset: the counts back to 0, nothing added */
void TcX_Counts(nativeBuf_t *out, byte reset);

#endif
