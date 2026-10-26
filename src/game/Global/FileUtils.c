/*
 * Helpers of Global/ linked between FileSys/FilePak.cpp and Global/HashTable.h: line trimming, directory
 * creation and walking, and pattern lists for the pack builder.
 */
#include <string.h>

#include "scrap/filesys.h"

/* _finddata32_t of the exe's CRT */
typedef struct crt_finddata32 {
	uint32_t attrib;		/* 0x00 */
	int32_t timeCreate;		/* 0x04 */
	int32_t timeAccess;		/* 0x08 */
	int32_t timeWrite;		/* 0x0c */
	uint32_t size;			/* 0x10 */
	char name[260];			/* 0x14 */
} crt_finddata32;
SD_SIZE(crt_finddata32, 0x118);

enum { CRT_A_HIDDEN = 0x2, CRT_A_SYSTEM = 0x4, CRT_A_SUBDIR = 0x10 };

intptr_t crt_findfirst32(const char *spec, crt_finddata32 *fd);
int crt_findnext32(intptr_t h, crt_finddata32 *fd);
int crt_memicmp(const void *a, const void *b, size_t n);
int crt_chdir(const char *dir);
void crt_splitpath(const char *path, char *drive, char *dir, char *name, char *ext);
char *crt_strtok_s(char *s, const char *sep, char **ctx);
char *crt_strlwr(char *s);

/* drops CR and LF everywhere, blanks at the start and the end */
void Str_Trim(char *s) {
	int len = 0;
	int end = 0;

	for (const char *p = s; *p; p++) {
		switch (*p) {
		case '\t':
		case '\v':
		case ' ':
			if (end) {
				s[len++] = *p;
			}
			break;
		case '\n':
		case '\r':
			break;
		default:
			s[len++] = *p;
			end = len;
			break;
		}
	}
	s[end] = '\0';
}

/* creates every directory of path up to the last separator */
void Path_MakeDirs(const char *path) {
	char buf[256];

	memset(buf, 0, sizeof(buf));
	Str_Copy(buf, path, sizeof(buf));
	for (char *p = buf;;) {
		char *sep = strstr(p, "/");
		char *bs = strstr(p, "\\");
		if (bs && (uintptr_t)bs < (uintptr_t)sep) {
			sep = bs;
		}
		if (!sep) {
			break;
		}
		*sep = '\0';
		File_MkDirRel(buf);
		*sep = '/';
		p = sep + 1;
	}
}

/*
 * calls cb with every file below dir (dir ends with '/') whose name ends in "." ext, all files for "".
 * Hidden and system entries are skipped. The search handles are never closed, as in the original.
 */
void Dir_Walk(const char *dir, const char *ext, void (*cb)(const char *path)) {
	size_t extLen = strlen(ext);
	crt_finddata32 fd;
	char buf[512];

	Str_Printf(buf, sizeof(buf), "%s/*.*", dir);
	intptr_t h = crt_findfirst32(buf, &fd);
	if (h == -1) {
		return;
	}
	do {
		if (fd.attrib & CRT_A_SUBDIR) {
			if (fd.attrib & (CRT_A_HIDDEN | CRT_A_SYSTEM)) {
				continue;
			}
			if (fd.name[0] == '.' && (fd.name[1] == '.' ? fd.name[2] : fd.name[1]) == '\0') {
				continue;
			}
			Str_Printf(buf, sizeof(buf), "%s%s/", dir, fd.name);
			Dir_Walk(buf, ext, cb);
		} else if (!(fd.attrib & (CRT_A_HIDDEN | CRT_A_SYSTEM | CRT_A_SUBDIR))) {
			size_t len = strlen(fd.name);
			if (len <= extLen) {
				continue;
			}
			if (extLen && (crt_memicmp(fd.name + len - extLen, ext, extLen) || fd.name[len - extLen - 1] != '.')) {
				continue;
			}
			Str_Printf(buf, sizeof(buf), "%s%s", dir, fd.name);
			cb(buf);
		}
	} while (crt_findnext32(h, &fd) == 0);
}

/* changes to the directory of path, then to sub (NULL for none); 1 if the last chdir worked */
byte Dir_Change(const char *path, const char *sub) {
	char drive[4];
	char dir[256];
	char buf[260];

	crt_splitpath(path, drive, dir, NULL, NULL);
	size_t len = strlen(dir);
	if (len == 0) {
		return 0;
	}
	int slash = dir[len - 1] == '\\' || dir[len - 1] == '/';
	Str_Printf(buf, sizeof(buf), "%s%s%s%s", drive, dir, slash ? "" : "/", sub ? sub : "");
	return crt_chdir(buf) == 0;
}

/* case-insensitive wildcard match of name against pattern ('*', '?', "[...]") */
byte Str_MatchNoCase(const char *pattern, const char *name) {
	char n[520];
	char p[520];

	Cad_Copy(n, name, sizeof(n));
	Cad_Copy(p, pattern, sizeof(p));
	crt_strlwr(n);
	crt_strlwr(p);
	return Wild_Match(p, n) != 0;
}

/* 1 if name matches one of the sep separated patterns of list; blanks around a pattern do not count */
byte StrList_Match(const char *list, const char *name, const char *sep) {
	char buf[1024];
	char *ctx = NULL;

	Str_Copy(buf, list, sizeof(buf));
	for (char *tok = crt_strtok_s(buf, sep, &ctx); tok; tok = crt_strtok_s(NULL, sep, &ctx)) {
		while (*tok == '\t' || *tok == ' ') {
			tok++;
		}
		for (char *end = tok + strlen(tok) - 1; end >= tok && (*end == '\t' || *end == ' '); end--) {
			*end = '\0';
		}
		if (Str_MatchNoCase(tok, name)) {
			return 1;
		}
	}
	return 0;
}
