/*
 * Ini files of the engine3d, original source Engine/ArchINI.cpp (include/scrap/archio.h): a CIniFile holds a
 * cDataFile, the getters take a default for a missing or bad value. In model files the lines are stored as an
 * "INI" block of a CFileIO.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/e3dutil.h"

#define INI_LOADSAVE_MAXLINE 0x100

/* name, below dir if there is one */
static void IniFile_Path(char *path, const char *name, const char *dir) {
	if (dir) {
		Path_Merge(path, dir, name);
	} else {
		String_Copy(path, name, MAX_FILEPATH);
	}
}

CIniFile *CIniFile_ctorLoad(CIniFile *self, const char *name, const char *dir) {
	char path[MAX_FILEPATH];

	self->dat = DataFile_New();
	IniFile_Path(path, name, dir);
	int r = DataFile_Load(self->dat, path, DataFile_Count(self->dat));
	E3D_LogInfo((&g_FileLog), "INI: %s%s", path, r == 0 ? " <ok>" : " <not found>");
	return self;
}

CIniFile *CIniFile_ctor(CIniFile *self) {
	self->dat = DataFile_New();
	return self;
}

void CIniFile_dtor(CIniFile *self) {
	DataFile_Free(self->dat);
}

/* a flag, then the block if it is set */
void IniFile_Read(CFileIO *f, CIniFile **ini) {
	CHECK(ini && !*ini);
	if (CFileIO_ReadInt(f) == 0) {
		return;
	}
	CIniFile *p = operator_new(sizeof(*p));
	if (p) {
		p->dat = DataFile_New();
	}
	*ini = p;
	CIniFile_ReadBlock(p, f);
}

void IniFile_Write(CFileIO *f, CIniFile *ini) {
	CFileIO_WriteInt(f, ini != NULL);
	if (ini) {
		CIniFile_WriteBlock(ini, f);
	}
}

int CIniFile_FindKey(CIniFile *self, const char *key, char *value, int size, int from) {
	return DataFile_FindKey(self->dat, key, from, value, size);
}

int CIniFile_FindSection(CIniFile *self, const char *name, int from) {
	return DataFile_FindSection(self->dat, name, from);
}

byte CIniFile_GetBool(CIniFile *self, const char *sect, const char *key, byte def) {
	int v;

	if (Dat_GetBool(self->dat, sect, key, &v) != 0) {
		return def;
	}
	return v != 0;
}

float CIniFile_GetFloat(CIniFile *self, const char *sect, const char *key, float def) {
	float v;

	return Dat_GetFloat(self->dat, sect, key, &v) != 0 ? def : v;
}

int CIniFile_GetInt(CIniFile *self, const char *sect, const char *key, int def) {
	int v;

	return Dat_GetLong(self->dat, sect, key, &v) != 0 ? def : v;
}

int CIniFile_Count(CIniFile *self) {
	return DataFile_Count(self->dat);
}

static void IniFile_Invalid(const char *sect, const char *key, const char *text) {
	E3D_LogBlue((&g_FileLog), "Invalid parameter: [%s] %s = %s", sect, key, text);
}

/* "R G B A" as 0xAARRGGBB */
uint32_t CIniFile_GetColor(CIniFile *self, const char *sect, const char *key, uint32_t def) {
	char text[0x100];
	byte c[4];

	if (Dat_GetText(self->dat, sect, key, text, sizeof(text)) != 0) {
		return def;
	}
	if (Proc_Scan(text, "&b&b&b&b", &c[0], &c[1], &c[2], &c[3]) != 4) {
		IniFile_Invalid(sect, key, text);
		return def;
	}
	return (uint32_t)c[3] << 24 | (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
}

byte CIniFile_GetSectionName(CIniFile *self, int idx, char *dest, int size) {
	if (DataFile_LineType(self->dat, idx) != LINE_SECTION) {
		return 0;
	}
	DataFile_SectionName(dest, DataFile_Line(self->dat, idx)->text, size);
	return 1;
}

/* dest, or NULL if the key is missing and there is no default */
char *CIniFile_GetText(CIniFile *self, const char *sect, const char *key, char *dest, int size,
	const char *def) {
	if (Dat_GetText(self->dat, sect, key, dest, size) != 0) {
		if (!def) {
			return NULL;
		}
		String_Copy(dest, def, size);
	}
	return dest;
}

float *CIniFile_GetVec2(CIniFile *self, float *out, const char *sect, const char *key, const float *def) {
	char text[0x100];
	float v[2];

	if (Dat_GetText(self->dat, sect, key, text, sizeof(text)) != 0) {
		out[0] = def[0];
		out[1] = def[1];
		return out;
	}
	if (Proc_Scan(text, "&f&f", &v[0], &v[1]) != 2) {
		IniFile_Invalid(sect, key, text);
		out[0] = def[0];
		out[1] = def[1];
		return out;
	}
	out[0] = v[0];
	out[1] = v[1];
	return out;
}

vec3_t *CIniFile_GetVec3(CIniFile *self, vec3_t *out, const char *sect, const char *key, const vec3_t *def) {
	char text[0x100];
	vec3_t v;

	if (Dat_GetText(self->dat, sect, key, text, sizeof(text)) != 0) {
		*out = *def;
		return out;
	}
	if (Proc_Scan(text, "&f&f&f", &v.x, &v.y, &v.z) != 3) {
		IniFile_Invalid(sect, key, text);
		*out = *def;
		return out;
	}
	*out = v;
	return out;
}

void CIniFile_ReadBlock(CIniFile *self, CFileIO *f) {
	char line[INI_LOADSAVE_MAXLINE];

	CHECK(!DataFile_Count(self->dat));
	CFileIO_BeginBlock(f, "INI");
	int version = CFileIO_ReadInt(f);
	CHECK(version == 1);
	for (int n = CFileIO_ReadInt(f); n > 0; n--) {
		CFileIO_ReadString(f, line, sizeof(line));
		DataFile_InsertAt(self->dat, line, DataFile_Count(self->dat));
	}
	CFileIO_Error(f, "INI");
}

void CIniFile_Clear(CIniFile *self) {
	DataFile_Free(self->dat);
	self->dat = DataFile_New();
}

void CIniFile_WriteBlock(CIniFile *self, CFileIO *f) {
	CHECK(DataFile_Count(self->dat));
	CFileIO_BeginBlockWrite(f, "INI");
	CFileIO_WriteInt(f, 1);
	int n = DataFile_Count(self->dat);
	CFileIO_WriteInt(f, n);
	for (int i = 1; i <= n; i++) {
		cDatLine *lin = DataFile_Line(self->dat, i);
		const char *line = lin ? lin->text : NULL;
		CHECK(strlen(line) < INI_LOADSAVE_MAXLINE);
		CFileIO_WriteString(f, line);
	}
	CFileIO_EndBlockWrite(f, "INI");
}

int CIniFile_SetYesNo(CIniFile *self, const char *sect, const char *key, byte v) {
	return Dat_SetStr(self->dat, sect, key, v ? "YES" : "NO");
}

/* prec digits after the point */
int CIniFile_SetFloat(CIniFile *self, const char *sect, const char *key, float v, int prec) {
	char text[0x40];

	Bprintf(text, sizeof(text), "%.*f", prec, (double)v);
	return Dat_SetStr(self->dat, sect, key, text);
}

int CIniFile_SetInt(CIniFile *self, const char *sect, const char *key, int v) {
	return Dat_SetInt(self->dat, sect, key, v);
}

int CIniFile_SetStr(CIniFile *self, const char *sect, const char *key, const char *v) {
	return Dat_SetStr(self->dat, sect, key, v);
}

/* the lines of a file after the ones there are; verbose logs a missing file once more */
int CIniFile_Load(CIniFile *self, const char *name, const char *dir, byte verbose) {
	char path[MAX_FILEPATH];

	IniFile_Path(path, name, dir);
	int r = DataFile_Load(self->dat, path, DataFile_Count(self->dat));
	if (verbose) {
		E3D_LogDebug((&g_FileLog), "INI: %s%s", path, r == 0 ? "" : " <not found>");
	}
	E3D_LogInfo((&g_FileLog), "INI: %s%s", path, r == 0 ? " <ok>" : " <not found>");
	return r;
}

/* idx < 0: at the end */
void CIniFile_AddLine(CIniFile *self, const char *text, int idx) {
	if (idx < 0) {
		idx = DataFile_Count(self->dat);
	}
	DataFile_InsertAt(self->dat, text, idx);
}

void CIniFile_DeleteLine(CIniFile *self, int idx) {
	DataFile_Delete(self->dat, idx);
}

/* "SI" / "NO" */
int CIniFile_SetBool(CIniFile *self, const char *sect, const char *key, int v) {
	return Dat_SetBool(self->dat, sect, key, v);
}

/* 0 or -1; the third argument is not used */
int CIniFile_Save(CIniFile *self, const char *name, const char *dir, int unused) {
	char path[MAX_FILEPATH];

	(void)unused;
	IniFile_Path(path, name, dir);
	void *f = File_OpenRel(path, "wt");
	if (!f) {
		return -1;
	}
	int r = DataFile_Write(self->dat, f);
	crt_fclose(f);
	return r;
}

int CIniFile_ReadBool(CIniFile *self, const char *sect, const char *key, int *out) {
	return Dat_GetBool(self->dat, sect, key, out);
}

int CIniFile_ReadFloat(CIniFile *self, const char *sect, const char *key, float *out) {
	return Dat_GetFloat(self->dat, sect, key, out);
}

int CIniFile_ReadInt(CIniFile *self, const char *sect, const char *key, int *out) {
	return Dat_GetLong(self->dat, sect, key, out);
}

int CIniFile_ReadText(CIniFile *self, const char *sect, const char *key, char *dest, int size) {
	return Dat_GetText(self->dat, sect, key, dest, size);
}

const char *CIniFile_LineText(CIniFile *self, int idx) {
	cDatLine *lin = DataFile_Line(self->dat, idx);
	return lin ? lin->text : NULL;
}

int CIniFile_LineType(CIniFile *self, int idx) {
	return DataFile_LineType(self->dat, idx);
}
