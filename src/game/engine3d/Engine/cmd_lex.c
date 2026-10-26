/*
 * The token layer of the engine3d command files on top of Util/Lex.cpp, original source Engine/cmd_lex.cpp
 * (include/scrap/lex.h). The current token is g_Cl, with its value converted.
 */
#include <string.h>

#include "scrap/lex.h"

int crt_sscanf(const char *s, const char *fmt, ...);

int CmdLex_Expect(int type) {
	const char *name;

	if (g_Cl.type == type) {
		return 0;
	}
	switch (type) {
	case CLEX_IDENT:
		name = "an identifier";
		break;
	case CLEX_INT:
		name = "an integer";
		break;
	case CLEX_FLOAT:
		name = "a floating point number";
		break;
	case CLEX_STRING:
		name = "a string";
		break;
	default:
		name = (const char *)(intptr_t)type;
		CHECK(0);
		break;
	}
	Lex_Error(0, "%s %s: '%s'", "It is not", name, g_Cl.text);
	return -1;
}

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

int CmdLex_GetInt(int *out) {
	int sign = CmdLex_Sign();
	int v = g_Cl.value.i;

	switch (g_Cl.type) {
	case CLEX_IDENT:
		return -1;
	case CLEX_INT:
		break;
	case CLEX_FLOAT:
		Lex_Error(1, "%s: '%s'", "Double to integer conversion", g_Cl.text);
		v = (int)g_Cl.value.d;
		break;
	case CLEX_STRING:
		Lex_Error(0, "Invalid operand type");
		return -1;
	default:
		Lex_Error(0, "Missing operand");
		return -1;
	}
	if (sign == '-') {
		v = -v;
	}
	CmdLex_Next();
	if (out) {
		*out = v;
	}
	return 0;
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
void CmdLex_SkipBlock(int depth) {
	if (depth == 0) {
		CmdLex_SkipTo('{');
		depth = 1;
	} else if (depth < 1) {
		return;
	}
	do {
		CmdLex_SkipToAny("{}");
		if (g_Cl.type == CLEX_ERR) {
			return;
		}
		depth += g_Cl.value.i == '{' ? 1 : -1;
		CmdLex_Next();
	} while (depth > 0);
}

void CmdLex_SkipTo(int c) {
	for (;;) {
		if (g_Cl.type == CLEX_CHAR) {
			if (g_Cl.value.i == c) {
				CmdLex_Next();
				return;
			}
		} else if (g_Cl.type == CLEX_ERR) {
			return;
		}
		CmdLex_Next();
	}
}

void CmdLex_SkipToAny(const char *chars) {
	while (!(g_Cl.type == CLEX_CHAR && strchr(chars, g_Cl.value.i)) && g_Cl.type != CLEX_ERR) {
		CmdLex_Next();
	}
}

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
