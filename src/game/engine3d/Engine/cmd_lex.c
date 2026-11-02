/*
 * The token layer of the engine3d command files on top of Util/Lex.cpp, original source Engine/cmd_lex.cpp
 * (include/scrap/lex.h). The current token is g_Cl, with its value converted.
 */
#include <string.h>

#include "scrap/lex.h"

cClex g_Cl;

int CmdLex_IsChar(int c) {
	return g_Cl.type == CLEX_CHAR && g_Cl.value.i == c;
}

int CmdLex_NeedChar(int c) {
	if (g_Cl.type == CLEX_CHAR && g_Cl.value.i == c) {
		CmdLex_Next();
		return 0;
	}
	Lex_Error(0, "missing a %c character", c);
	if (g_Cl.type == CLEX_CHAR) {
		CmdLex_Next();
	}
	return -1;
}

/* a '+' or '-' before the number is its own token */
static int CmdLex_Sign(void) {
	int c = g_Cl.value.i;

	if (g_Cl.type == CLEX_CHAR && (c == '-' || c == '+')) {
		CmdLex_Next();
		return c;
	}
	return '+';
}

int CmdLex_GetDouble(double *out) {
	int sign = CmdLex_Sign();
	double d;

	switch (g_Cl.type) {
	case CLEX_IDENT:
	case CLEX_STRING:
		Lex_Error(0, "Invalid operand type");
		return -1;
	case CLEX_INT:
		d = g_Cl.value.i;
		break;
	case CLEX_FLOAT:
		d = g_Cl.value.d;
		break;
	default:
		Lex_Error(0, "Missing operand");
		return -1;
	}
	if (sign == '-') {
		d = -d;
	}
	d += 0.0;	/* in the original too: -0.0 becomes 0.0 */
	CmdLex_Next();
	if (out) {
		*out = d;
	}
	return 0;
}

/* depth 0: first up to the next '{' */
void CmdLex_Next(void) {
	int n;

	CHECK(g_Cl.type != CLEX_ERR);
	g_Cl.type = Lex_Next();
	g_Cl.text = g_LexToken;
	if (g_Cl.type == CLEX_INT) {
		const char *kind = "integer";
		const char *fmt = "%ld%n";
		g_Cl.value.i = 0;
		if (g_LexToken[0] == '0') {
			if (g_LexToken[1] == 'x' || g_LexToken[1] == 'X') {
				kind = "hexadecimal";
				fmt = "%lx%n";
			} else {
				kind = "octal";
				fmt = "%lo%n";
			}
		}
		if (crt_sscanf(g_LexToken, fmt, &g_Cl.value.i, &n) != 1 || g_Cl.value.i < 0 || g_Cl.text[n] != '\0') {
			Lex_Error(0, "Invalid %s constant: '%s'", kind, g_Cl.text);
		}
	} else if (g_Cl.type == CLEX_FLOAT) {
		g_Cl.value.d = 0.0;
		if (crt_sscanf(g_LexToken, "%lf%n", &g_Cl.value.d, &n) != 1) {
			Lex_Error(0, "Invalid %s constant: '%s'", "floating point", g_Cl.text);
		}
	} else if (g_Cl.type == CLEX_CHAR) {
		g_Cl.value.i = (byte)g_LexToken[0];
	}
}
