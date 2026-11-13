/*
 * The system part of the coordinator (include/scrap/coord.h): the wall clock of the tickets, hex, folders and files
 * written atomically, the seed files of identity keys.
 */
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "scrap/coord.h"

uint64_t Coord_UnixMs(void) {
#ifdef _WIN32
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	return t / 10000 - 11644473600000ull;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

void Coord_Hex(const uint8_t *b, int n, char *out) {
	static const char digits[] = "0123456789abcdef";
	for (int i = 0; i < n; i++) {
		out[2 * i] = digits[b[i] >> 4];
		out[2 * i + 1] = digits[b[i] & 15];
	}
	out[2 * n] = 0;
}

static int Coord_Digit(char c) {
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

int Coord_Unhex(const char *s, uint8_t *b, int n) {
	for (int i = 0; i < n; i++) {
		int hi = Coord_Digit(s[2 * i]), lo = hi < 0 ? -1 : Coord_Digit(s[2 * i + 1]);
		if (lo < 0) {
			return 0;
		}
		b[i] = (uint8_t)(hi << 4 | lo);
	}
	return s[2 * n] == 0;
}

int Coord_MkDir(const char *path) {
#ifdef _WIN32
	if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
		return 1;
	}
	return 0;
#else
	return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

int Coord_ReadFile(const char *path, char *buf, int cap) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return errno == ENOENT ? -1 : -2;
	}
	size_t n = fread(buf, 1, (size_t)cap, f);
	int err = ferror(f);
	fclose(f);
	if (err || n >= (size_t)cap) {
		return -2;
	}
	buf[n] = 0;
	return (int)n;
}

/* the data on the disk before the rename (a crash after the rename must not leave an empty file) */
static int Coord_Flush(FILE *f) {
	if (fflush(f) != 0) {
		return 0;
	}
#ifdef _WIN32
	return _commit(_fileno(f)) == 0;
#else
	return fsync(fileno(f)) == 0;
#endif
}

static int Coord_Rename(const char *from, const char *to) {
#ifdef _WIN32
	return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(from, to) == 0;
#endif
}

int Coord_WriteAtomic(const char *path, const void *data, int size, const char *bak) {
	char tmp[COORD_PATH + 8];
	if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
		return 0;
	}
	FILE *f = fopen(tmp, "wb");
	if (!f) {
		return 0;
	}
	int ok = fwrite(data, 1, (size_t)size, f) == (size_t)size;
	ok = Coord_Flush(f) && ok;
	ok = fclose(f) == 0 && ok;
	if (!ok) {
		remove(tmp);
		return 0;
	}
	if (bak) {
		FILE *old = fopen(path, "rb");
		if (old) {
			fclose(old);
			if (!Coord_Rename(path, bak)) {
				remove(tmp);
				return 0;
			}
		}
	}
	if (!Coord_Rename(tmp, path)) {
		remove(tmp);
		return 0;
	}
	return 1;
}

int Coord_LoadSeed(const char *path, uint8_t seed[NET2_SEED_SIZE], int *made) {
	char text[2 * NET2_SEED_SIZE + 4];
	*made = 0;
	int n = Coord_ReadFile(path, text, sizeof(text));
	if (n >= 2 * NET2_SEED_SIZE) {
		text[2 * NET2_SEED_SIZE] = 0;
		return Coord_Unhex(text, seed, NET2_SEED_SIZE);
	}
	if (n != -1 || !Net2_Random(seed, NET2_SEED_SIZE)) {
		return 0;
	}
	Coord_Hex(seed, NET2_SEED_SIZE, text);
	text[2 * NET2_SEED_SIZE] = '\n';
#ifdef _WIN32
	int ok = Coord_WriteAtomic(path, text, 2 * NET2_SEED_SIZE + 1, NULL);
#else
	/* only the owner reads it */
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
	int ok = fd >= 0 && write(fd, text, 2 * NET2_SEED_SIZE + 1) == 2 * NET2_SEED_SIZE + 1;
	ok = fd >= 0 && fsync(fd) == 0 && ok;
	if (fd >= 0) {
		close(fd);
	}
#endif
	Net2_Wipe(text, sizeof(text));
	*made = ok;
	return ok;
}
