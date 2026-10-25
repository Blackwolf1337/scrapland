/*
 * Engine basics of the original Global/ folder: the CHECK macro and the console.
 */
#ifndef SCRAP_GLOBAL_H
#define SCRAP_GLOBAL_H

#include "scrap/shared.h"

/* "FATAL ERROR: Check failed!" report of the engine (rva 0xbcdb0), logs expression, file, date and line */
void CheckFailed(const char *expr, const char *file, const char *date, int line);

/* the original macro passes __FILE__ and __TIMESTAMP__ of the .cpp file, ours those of the .c file */
#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			CheckFailed(#expr, __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

#endif
