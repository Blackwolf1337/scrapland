/*
 * Arguments for the game's own command line parser (CApp_ParseCmdLine of src/game/App/App.c: a '-' starts a
 * parameter, a ':' its value, a blank ends it, a '"' or '\'' quotes): "-param:value" with the value quoted when it
 * has a blank, '-' or ':' (a name "a-b c", an IPv6 address). Used where a program builds the line the game parses:
 * the Linux main (src/standalone/main.c) from argv, the coordinator for its instances on Windows (src/coord/Proc.c).
 * Not in the original.
 */
#ifndef SCRAP_CMDQUOTE_H
#define SCRAP_CMDQUOTE_H

#include <string.h>

/* arg as the game's parser takes it into out (zero ended): its length, -1 when it does not fit. An argument with a
 * quote in its value is left as it is (quoted by the user already); one that is no "-param:value" with a blank is
 * quoted whole */
static inline int CmdQuote_Arg(const char *arg, char *out, int cap) {
	const char *colon = arg[0] == '-' ? strchr(arg, ':') : NULL;
	int n = (int)strlen(arg);
	if (colon && !strpbrk(colon + 1, "\"'") && strpbrk(colon + 1, " \t-:")) {
		int head = (int)(colon - arg) + 1;
		if (n + 3 > cap) {
			return -1;
		}
		memcpy(out, arg, (size_t)head);
		out[head] = '"';
		memcpy(out + head + 1, colon + 1, (size_t)(n - head));
		out[n + 1] = '"';
		out[n + 2] = 0;
		return n + 2;
	}
	if (!colon && strpbrk(arg, " \t")) {
		if (n + 3 > cap) {
			return -1;
		}
		out[0] = '"';
		memcpy(out + 1, arg, (size_t)n);
		out[n + 1] = '"';
		out[n + 2] = 0;
		return n + 2;
	}
	if (n + 1 > cap) {
		return -1;
	}
	memcpy(out, arg, (size_t)n + 1);
	return n;
}

#endif
