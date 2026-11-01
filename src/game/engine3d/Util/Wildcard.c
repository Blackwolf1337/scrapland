/*
 * Paths and wildcards of the engine3d, original source Util/Comodin.cpp (include/scrap/e3dutil.h). Drive, chdir and
 * mkdir go through the CRT of the exe, so the drive and errno state is the one the exe sees.
 */
#include <string.h>

#include "scrap/e3dutil.h"

enum { CRT_ENOENT = 2, CRT_ENOMEM = 12, CRT_EACCES = 13 };
/* added to 2 for the errno of a path that does not exist */
#define g_PathErrBase GAME_GLOBAL(int, g_ErrPcxFormat)

#define IS_SEP(c) ((c) == '\\' || (c) == '/')

void Path_GetDrive(int *drive) {
	CHECK(drive);
	*drive = crt_getdrive();
}

int Path_MakeDir(char *path) {
	char tmp[MAX_FILEPATH];

	Str_Copy(tmp, path, sizeof(tmp));
	size_t len = strlen(tmp);
	if (len && IS_SEP(tmp[len - 1])) {
		if (len - 1 >= sizeof(tmp)) {
			crt_report_rangecheckfailure();
		}
		tmp[len - 1] = '\0';
	}
	if (crt_chdir(tmp) == 0) {
		return 0;
	}
	Path_Part(tmp, tmp, PATH_DRIVE | PATH_DIR);
	if (strcmp(path, tmp) == 0) {
		*crt_errno() = CRT_EACCES;
		return -1;
	}
	if (Path_MakeDir(tmp) != 0) {
		return -1;
	}
	len = strlen(path);
	if (IS_SEP(path[len - 1])) {
		path[len - 1] = '\0';
	}
	return crt_mkdir(path) != 0 ? -1 : 0;
}

/* the set at pattern ("[...]") against the character at s: the pattern after the set or NULL */
const char *Wild_MatchSet(const char *pattern, const char *s) {
	const char *p = pattern;
	int match = 0;

	if (*p != '[') {
		return NULL;
	}
	byte c = (byte)*++p;
	if (!c) {
		return NULL;
	}
	if (c == '!') {
		c = (byte)*++p;
		match = 1;
		if (!c) {
			return NULL;
		}
	}
	if (c != ']') {
		for (;;) {
			unsigned lo = c, hi = c;
			c = (byte)*++p;
			if (!c) {
				return NULL;
			}
			if (c == '-') {
				c = (byte)*++p;
				if (!c) {
					return NULL;
				}
				hi = 0xff;
				if (c != ']') {
					hi = c;
					c = (byte)*++p;
					if (!c) {
						return NULL;
					}
				}
			} else if (lo == '?') {
				lo = 0;
				hi = 0xff;
			}
			unsigned ch = (byte)*s;
			if (ch >= lo && ch <= hi) {
				match ^= 1;
				break;
			}
			if (c == ']') {
				break;
			}
		}
	}
	if (!match) {
		return NULL;
	}
	while (*p != ']') {
		if (!*++p) {
			return NULL;
		}
	}
	return p + 1;
}

int Wild_Match(const char *pattern, const char *s) {
	for (;;) {
		char c = *pattern;
		if (!c) {
			return *s == '\0';
		}
		if (c == '*') {
			while (!Wild_Match(pattern + 1, s)) {
				if (!*s) {
					return 0;
				}
				s++;
			}
			return 1;
		}
		if (c == '?') {
			pattern++;
			if (*s) {
				s++;
			}
			continue;
		}
		if (c == '[') {
			if (!*s) {
				return 0;
			}
			pattern = Wild_MatchSet(pattern, s);
			if (!pattern) {
				return 0;
			}
			s++;
			continue;
		}
		if (!*s || *s != c) {
			return 0;
		}
		pattern++;
		s++;
	}
}

char *Path_Make(char *path, const char *drive, const char *dir, const char *name, const char *ext) {
	char u[8];

	CHECK(path);
	u[0] = '\0';
	if (drive && *drive) {
		u[0] = *drive;
		u[1] = ':';
		u[2] = '\0';
	}
	if (!name) {
		name = "";
	}
	if (!ext) {
		ext = "";
	}
	if (!dir || !*dir) {
		Bprintf(path, MAX_FILEPATH, "%s%s%s", u, name, ext);
	} else if (!IS_SEP(dir[strlen(dir) - 1])) {
		Bprintf(path, MAX_FILEPATH, "%s%s\\%s%s", u, dir, name, ext);
	} else {
		Bprintf(path, MAX_FILEPATH, "%s%s%s%s", u, dir, name, ext);
	}
	return path;
}

char *Path_Format(char *dest, const char *orig, const char *frmt) {
	char drive[4];
	char dir[256];
	char name[256];
	char ext[256];
	char tmp[520];
	char *p = tmp;

	CHECK(dest && orig && frmt);
	Path_SplitDrive(orig, drive, dir, name, ext);
	for (; *frmt; frmt++) {
		if (*frmt != '&') {
			*p++ = *frmt;
			continue;
		}
		const char *cp = NULL;
		frmt++;
		if (*frmt == '&') {
			cp = "&";
		} else if (crt_tolower(*frmt) == 'u') {
			cp = drive;
		} else if (crt_tolower(*frmt) == 'd') {
			cp = dir;
		} else if (crt_tolower(*frmt) == 'n') {
			cp = name;
		} else if (crt_tolower(*frmt) == 'e') {
			cp = ext;
		} else {
			CHECK(cp);
		}
		int size = (int)strlen(cp);
		CHECK((p - tmp) + size < MAX_FILEPATH);
		memcpy(p, cp, size);
		p += size;
	}
	*p = '\0';
	CHECK(p - tmp + 1 <= MAX_FILEPATH);
	memcpy(dest, tmp, p - tmp + 1);
	return dest;
}

char *Path_IsDir(char *path) {
	char cwd[MAX_FILEPATH];
	char *res = NULL;
	int hadSep = 0;

	CHECK(path);
	int drive = Path_DriveNum(path);
	if (Path_GetCwd(drive, cwd, sizeof(cwd)) != 0) {
		return NULL;
	}
	size_t len = strlen(path);
	if (IS_SEP(path[len - 1])) {
		path[--len] = '\0';
		hadSep = 1;
	}
	if ((len == 2 && drive) || crt_chdir(path) == 0) {
		res = path;
	}
	if (res || hadSep) {
		path[len] = '\\';
		path[len + 1] = '\0';
	}
	crt_chdir(cwd);
	if (!res && *crt_errno() == CRT_ENOENT) {
		*crt_errno() = g_PathErrBase + 2;
	}
	return res;
}

int Path_MakePath(const char *fullPath) {
	char path[MAX_FILEPATH];
	char cwd[MAX_FILEPATH];

	Path_Part(path, fullPath, PATH_DRIVE | PATH_DIR);
	if (!Path_Full(path, path, MAX_FILEPATH, NULL) || Path_GetCwd(Path_DriveNum(path), cwd, MAX_FILEPATH) != 0) {
		return -1;
	}
	int r = Path_MakeDir(path);
	crt_chdir(cwd);
	return r;
}

/* the characters Path_SplitDrive collected backwards ending at buf[MAX_FILEPATH], k + 2 is the first */
static void Path_CopyPart(char *dst, const char *buf, int k, unsigned max) {
	unsigned len = (unsigned)(MAX_FILEPATH - 1 - k);
	unsigned n = len < max ? len : max;
	memcpy(dst, buf + k + 2, n);
	dst[n] = '\0';
}

void Path_SplitDrive(const char *path, char *drive, char *dir, char *name, char *ext) {
	char buf[MAX_FILEPATH + 1];
	/* the part read next, from the end: 3 name or extension, 2 name, 1 directory, 0 drive */
	int state = 3;
	int k;

	if (drive) {
		*drive = '\0';
	}
	if (dir) {
		*dir = '\0';
	}
	if (name) {
		*name = '\0';
	}
	if (ext) {
		*ext = '\0';
	}
	if (!path) {
		return;
	}
	int i = (int)strlen(path) - 1;
	if (i < 0) {
		return;
	}
	while (path[i] == ' ') {
		if (--i < 0) {
			return;
		}
	}
	/* ".", "..", "x\\." and "C:." are directories */
	if (i >= 1 && path[i] == '.') {
		if (path[i - 1] == '.' || path[i - 1] == ':' || Path_IsSep(path[i - 1])) {
			state = 1;
		}
	} else if (i == 0 && path[0] == '.') {
		state = 1;
	}

	k = MAX_FILEPATH - 1;
	while (state == 3 && k >= 0) {
		char c = path[i];
		if (c == '.') {
			buf[k-- + 1] = c;
			i--;
			if (ext) {
				Path_CopyPart(ext, buf, k, 0xff);
			}
			state = 2;
		} else if (IS_SEP(c) || c == ':') {
			if (name) {
				Path_CopyPart(name, buf, k, 0xff);
			}
			state = c == ':' ? 0 : 1;
		} else {
			buf[k-- + 1] = c;
			i--;
		}
		if (i < 0) {
			if (state == 3 && name) {
				Path_CopyPart(name, buf, k, 0xff);
			}
			break;
		}
	}

	if (i >= 0) {
		k = MAX_FILEPATH - 1;
		while (state == 2 && k >= 0) {
			char c = path[i];
			if (IS_SEP(c) || c == ':') {
				if (name) {
					Path_CopyPart(name, buf, k, 0xff);
				}
				state = c == ':' ? 0 : 1;
			} else {
				buf[k-- + 1] = c;
				i--;
			}
			if (i < 0) {
				if (state == 2 && name) {
					Path_CopyPart(name, buf, k, 0xff);
				}
				break;
			}
		}
	}

	if (i >= 0) {
		k = MAX_FILEPATH - 1;
		while (state == 1 && k >= 0) {
			char c = path[i];
			if (c == ':') {
				if (dir) {
					Path_CopyPart(dir, buf, k, 0xff);
				}
				state = 0;
			} else {
				buf[k-- + 1] = c;
				i--;
			}
			if (i < 0) {
				if (state == 1 && dir) {
					Path_CopyPart(dir, buf, k, 0xff);
				}
				break;
			}
		}
	}

	if (i >= 0) {
		k = MAX_FILEPATH - 1;
		while (state == 0 && k >= 0) {
			buf[k-- + 1] = path[i--];
			if (i < 0) {
				if (drive) {
					Path_CopyPart(drive, buf, k, 3);
				}
				break;
			}
		}
	}
}

int Path_IsSep(char c) {
	return IS_SEP(c);
}

char *Path_Merge(char *dest, const char *path1, const char *path2) {
	char drive1[4], drive2[4];
	char dir1[256], dir2[256];
	char name1[256], name2[256];
	char ext1[256], ext2[256];

	CHECK(dest && (path1 || path2));
	/* the original copies a single path with strcpy, here at most MAX_FILEPATH bytes; a copy onto itself (the .EMI
	 * reader merges "" with dest) keeps it, Str_Copy would clear it */
	if (!path1 || !*path1) {
		if (dest != path2) {
			Str_Copy(dest, path2, MAX_FILEPATH);
		}
		return dest;
	}
	if (!path2 || !*path2) {
		if (dest != path1) {
			Str_Copy(dest, path1, MAX_FILEPATH);
		}
		return dest;
	}
	Path_SplitDrive(path1, drive1, dir1, name1, ext1);
	Path_SplitDrive(path2, drive2, dir2, name2, ext2);
	return Path_Make(dest, *drive1 ? drive1 : drive2, *dir1 ? dir1 : dir2, *name1 ? name1 : name2,
		*ext1 ? ext1 : ext2);
}

int Path_Match(const char *pattern, const char *path) {
	char name[256], ext[256];
	char pname[256], pext[256];

	Path_SplitDrive(path, NULL, NULL, name, ext);
	Path_SplitDrive(pattern, NULL, NULL, pname, pext);
	crt_strupr(name);
	crt_strupr(pname);
	crt_strupr(ext);
	crt_strupr(pext);
	if (!pname[0]) {
		Str_Copy(pname, "*", sizeof(pname));
	}
	if (!pext[0]) {
		Str_Copy(pext, ".*", sizeof(pext));
	}
	const char *e = ext[0] == '.' ? ext + 1 : ext;
	const char *pe = pext[0] == '.' ? pext + 1 : pext;
	return Wild_Match(pname, name) && Wild_Match(pe, e);
}

/* subdirs: the directory of pattern may also be a parent of the one of path */
int Path_MatchPath(const char *pattern, const char *path, int subdirs) {
	char pdir[256], dir[256];
	int i = 0;

	Path_Part(pdir, pattern, PATH_DRIVE | PATH_DIR);
	Path_Part(dir, path, PATH_DRIVE | PATH_DIR);
	crt_strupr(pdir);
	crt_strupr(dir);
	while (pdir[i] && dir[i] && pdir[i] == dir[i]) {
		i++;
	}
	if (pdir[i] || (!subdirs && dir[i])) {
		return 0;
	}
	return Path_Match(pattern, path);
}

void Path_Rename(char *dest, const char *src, const char *pattern) {
	char name[MAX_FILEPATH];
	char frmt[MAX_FILEPATH];
	char ext[256];

	CHECK(dest && src && pattern);
	Path_SplitDrive(src, NULL, NULL, name, ext);
	Path_Part(frmt, pattern, PATH_NAME | PATH_EXT);
	Path_Part(dest, pattern, PATH_DRIVE | PATH_DIR);
	char *out = dest + strlen(dest);
	if (!ext[0]) {
		Str_Copy(ext, ".", sizeof(ext));
	}
	int nameLen = (int)strlen(name);
	/* kept from the last number like the stack slots of the original, which reads them also after a failed scan */
	int v = 0, n = 1;
	int extLen = (int)strlen(ext);
	/* '*' and '?' copy from the same position of the name, after the first '.' of the extension */
	const char *cur = name;
	int curLen = nameLen;
	int inExt = 0;
	int o = 0;

	for (const char *p = frmt; *p;) {
		switch (*p) {
		case '(':
			if (p[1] == '(') {
				out[o++] = '(';
				p += 2;
				continue;
			}
			for (p++; *p != ')';) {
				if (crt_sscanf(p, "%d%n", &v, &n) != 1 || v == 0) {
					Err_Printf(1, "Formato no v\xa0lido (n>0 nombre, n<0 exten.) (%s)", frmt);
				}
				p += n;
				while (*p == ' ') {
					p++;
				}
				if (v > 0) {
					if (v - 1 < nameLen) {
						out[o++] = name[v - 1];
					}
				} else if (-v < extLen) {
					out[o++] = ext[-v];
				}
			}
			break;
		case '*':
			if (o < curLen) {
				memcpy(out + o, cur + o, curLen - o);
				o = curLen;
			}
			break;
		case '?':
			if (o < curLen) {
				out[o] = cur[o];
				o++;
			}
			break;
		case '.':
			if (!inExt) {
				out += o;
				o = 0;
				inExt = 1;
				cur = ext;
				curLen = extLen;
			}
			out[o++] = *p;
			break;
		default:
			out[o++] = *p;
			break;
		}
		p++;
	}
	out[o] = '\0';
	if (out[0] == '.' && out[1] == '\0') {
		out[0] = '\0';
	}
}

/* rel made absolute against base (NULL: the current directory of its drive), "." and ".." resolved */
char *Path_Full(char *dest, const char *rel, int size, const char *base) {
	char tmp[0x20c];
	char baseDrive = '\0';
	char drive;

	CHECK(dest && rel);
	if (base) {
		if (Path_DriveNum(base)) {
			baseDrive = *base;
			base += 2;
		}
		if (IS_SEP(*base)) {
			base++;
		}
	}
	if (Path_DriveNum(rel)) {
		drive = *rel;
		if (drive != baseDrive) {
			base = NULL;
		}
		rel += 2;
	} else if (base && baseDrive) {
		drive = baseDrive;
	} else {
		drive = (char)(crt_getdrive() + '@');
	}

	tmp[0] = drive;
	tmp[1] = ':';
	char *p = tmp + 2;
	if (!IS_SEP(*rel)) {
		tmp[2] = '\\';
		p = tmp + 3;
		if (!base) {
			if (Path_GetCwd(crt_toupper(drive) - '@', p, 0x206) != 0) {
				return NULL;
			}
			/* the cwd without its drive and first separator, moved to p */
			const char *q = Path_DriveNum(p) ? tmp + 5 : p;
			if (IS_SEP(*q)) {
				q++;
			}
			while (*q) {
				*p++ = *q++;
			}
			*p = '\0';
		} else {
			Str_Copy(tmp + 3, base, sizeof(tmp) - 3);
			p = tmp + strlen(tmp);
		}
		if (!IS_SEP(p[-1])) {
			*p++ = '\\';
		}
	}
	Str_Copy(p, rel, (int)(tmp + sizeof(tmp) - p));

	char *d = tmp;
	for (const char *s = tmp;;) {
		char c = *s++;
		if (c == '\0' || IS_SEP(c)) {
			if (d[-1] == '.') {
				if (IS_SEP(d[-2])) {
					d -= 2;
				} else if (d[-2] == '.' && IS_SEP(d[-3])) {
					d -= 3;
					if (d[-1] == ':') {
						*crt_errno() = g_PathErrBase + 2;
						return NULL;
					}
					do {
						d--;
					} while (!IS_SEP(*d));
				}
			}
			if (c == '\0') {
				break;
			}
		}
		*d++ = c;
	}
	if (d[-1] == ':') {
		*d++ = '\\';
	}
	*d = '\0';
	if ((int)strlen(tmp) + 1 > size) {
		*crt_errno() = CRT_ENOMEM;
		return NULL;
	}
	memcpy(dest, tmp, strlen(tmp) + 1);
	return dest;
}

char *Path_Part(char *dest, const char *orig, uint32_t mask) {
	char drive[4];
	char dir[256];
	char name[256];
	char ext[256];

	CHECK(dest && orig);
	drive[0] = '\0';
	dir[0] = '\0';
	name[0] = '\0';
	ext[0] = '\0';
	Path_SplitDrive(orig, mask & PATH_DRIVE ? drive : NULL, mask & PATH_DIR ? dir : NULL, mask & PATH_NAME ? name : NULL,
		mask & PATH_EXT ? ext : NULL);
	return Path_Make(dest, drive, dir, name, ext);
}

char *Path_PartN(char *dest, int size, const char *orig, uint32_t mask) {
	char tmp[MAX_FILEPATH];

	CHECK(size > 0);
	Path_Part(tmp, orig, mask);
	return String_Copy(dest, tmp, size);
}

char *Path_FileName(const char *path) {
	CHECK(path);
	for (int i = (int)strlen(path) - 1; i >= 0; i--) {
		if (IS_SEP(path[i]) || path[i] == ':') {
			return (char *)path + i + 1;
		}
	}
	return (char *)path;
}

char *Path_FirstDir(char *dest, const char *path) {
	char dir[256];

	CHECK(dest && path);
	Path_SplitDrive(path, NULL, dir, NULL, NULL);
	const char *s = IS_SEP(dir[0]) ? dir + 1 : dir;
	char *d = dest;
	while (*s && !IS_SEP(*s)) {
		*d++ = *s++;
	}
	*d = '\0';
	return dest;
}

char *Path_LastDir(char *dest, const char *path) {
	char dir[256];

	CHECK(dest && path);
	*dest = '\0';
	Path_SplitDrive(path, NULL, dir, NULL, NULL);
	int i = (int)strlen(dir) - 1;
	if (i >= 0) {
		if (IS_SEP(dir[i])) {
			if ((unsigned)i >= sizeof(dir)) {
				crt_report_rangecheckfailure();
			}
			dir[i] = '\0';
		}
		Str_Copy(dest, Path_FileName(dir), sizeof(dir));
	}
	return dest;
}

int Path_DriveNum(const char *path) {
	CHECK(path);
	if (crt_isalpha(*path) && path[1] == ':') {
		return crt_toupper(*path) - '@';
	}
	return 0;
}

int Path_GetCwd(int drive, char *dest, int size) {
	int old = 0;
	int r = 0;

	CHECK(dest && size > 0);
	CHECK(drive >= 0);
	if (drive) {
		old = crt_getdrive();
		crt_chdrive(drive);
		if (crt_getdrive() != drive) {
			*crt_errno() = CRT_EACCES;
			return -1;
		}
	}
	if (!crt_getcwd(dest, size)) {
		r = -1;
	}
	if (drive) {
		crt_chdrive(old);
	}
	return r;
}
