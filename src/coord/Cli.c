/*
 * The console of the dedicated programs ScrapCoord and ScrapList (include/scrap/coord.h): their clock, the lines of
 * the standard input without blocking, the -name:value arguments, the short wait between their passes.
 */
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <time.h>
#include <unistd.h>
#endif
#include <string.h>

#include "scrap/coord.h"

uint64_t CoordCli_Ms(void) {
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

int CoordCli_Line(char *line, int cap, int ms) {
	static char buf[512];
	static int len, eof;
	for (;;) {
		char *nl = memchr(buf, '\n', (size_t)len);
		if (nl) {
			int n = (int)(nl - buf);
			int k = n < cap - 1 ? n : cap - 1;
			memcpy(line, buf, (size_t)k);
			line[k] = 0;
			if (k && line[k - 1] == '\r') {
				line[k - 1] = 0;
			}
			memmove(buf, nl + 1, (size_t)(len - n - 1));
			len -= n + 1;
			return 1;
		}
		if (eof) {
			return -1;
		}
		if (len == (int)sizeof(buf)) {
			len = 0;
		}
#ifdef _WIN32
		HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
		DWORD got = 0, avail = 0;
		if (GetFileType(in) == FILE_TYPE_PIPE) {
			if (!PeekNamedPipe(in, NULL, 0, NULL, &avail, NULL)) {
				eof = 1;
				continue;
			}
			if (!avail) {
				Sleep((DWORD)ms);
				return 0;
			}
		} else if (WaitForSingleObject(in, (DWORD)ms) != WAIT_OBJECT_0) {
			return 0;
		}
		if (!ReadFile(in, buf + len, (DWORD)(sizeof(buf) - (size_t)len), &got, NULL) || !got) {
			eof = 1;
			continue;
		}
		len += (int)got;
#else
		struct pollfd p = { 0, POLLIN, 0 };
		if (poll(&p, 1, ms) <= 0) {
			return 0;
		}
		ssize_t got = read(0, buf + len, sizeof(buf) - (size_t)len);
		if (got <= 0) {
			eof = 1;
			continue;
		}
		len += (int)got;
#endif
		ms = 0;
	}
}

const char *CoordCli_Arg(const char *a, const char *name) {
	size_t n = strlen(name);
	return a[0] == '-' && !strncmp(a + 1, name, n) && a[n + 1] == ':' ? a + n + 2 : NULL;
}

void CoordCli_Wait(int ms) {
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	poll(NULL, 0, ms);
#endif
}
