/*
 * Ini files of the engine3d, original source Engine/ArchINI.cpp (include/scrap/archio.h): a CArchINI holds a
 * cArchDatos, the getters take a default for a missing or bad value. In model files the lines are stored as an
 * "INI" block of a CArchIO.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/e3dutil.h"

#define INI_LOADSAVE_MAXLINE 0x100

/* name, below dir if there is one */
static void ArchINI_Path(char *path, const char *name, const char *dir) {
	if (dir) {
		Ruta_Merge(path, dir, name);
	} else {
		Cad_Copy(path, name, MAX_RUTA);
	}
}

CArchINI *__thiscall CArchINI_ctorLoad(CArchINI *self, const char *name, const char *dir) {
	char path[MAX_RUTA];

	self->dat = Arch_New();
	ArchINI_Path(path, name, dir);
	int r = Arch_Load(self->dat, path, Arch_Count(self->dat));
	E3D_LogInfo(g_FileLog, "INI: %s%s", path, r == 0 ? " <ok>" : " <not found>");
	return self;
}

CArchINI *__thiscall CArchINI_ctor(CArchINI *self) {
	self->dat = Arch_New();
	return self;
}

void __thiscall CArchINI_dtor(CArchINI *self) {
	Arch_Free(self->dat);
}

/* a flag, then the block if it is set */
void ArchINI_Read(CArchIO *f, CArchINI **ini) {
	CHECK(ini && !*ini);
	if (CArchIO_ReadInt(f) == 0) {
		return;
	}
	CArchINI *p = operator_new(sizeof(*p));
	if (p) {
		p->dat = Arch_New();
	}
	*ini = p;
	CArchINI_ReadBlock(p, f);
}

void ArchINI_Write(CArchIO *f, CArchINI *ini) {
	CArchIO_WriteInt(f, ini != NULL);
	if (ini) {
		CArchINI_WriteBlock(ini, f);
	}
}

int __thiscall CArchINI_FindKey(CArchINI *self, const char *key, char *value, int size, int from) {
	return Arch_FindKey(self->dat, key, from, value, size);
}

int __thiscall CArchINI_FindSection(CArchINI *self, const char *name, int from) {
	return Arch_FindSection(self->dat, name, from);
}

byte __thiscall CArchINI_GetBool(CArchINI *self, const char *sect, const char *key, byte def) {
	int v;

	if (Dat_GetBool(self->dat, sect, key, &v) != 0) {
		return def;
	}
	return v != 0;
}

float __thiscall CArchINI_GetFloat(CArchINI *self, const char *sect, const char *key, float def) {
	float v;

	return Dat_GetFloat(self->dat, sect, key, &v) != 0 ? def : v;
}

int __thiscall CArchINI_GetInt(CArchINI *self, const char *sect, const char *key, int def) {
	int v;

	return Dat_GetLong(self->dat, sect, key, &v) != 0 ? def : v;
}

int __thiscall CArchINI_Count(CArchINI *self) {
	return Arch_Count(self->dat);
}

static void ArchINI_Invalid(const char *sect, const char *key, const char *text) {
	E3D_LogBlue(g_FileLog, "Invalid parameter: [%s] %s = %s", sect, key, text);
}

/* "R G B A" as 0xAARRGGBB */
uint32_t __thiscall CArchINI_GetColor(CArchINI *self, const char *sect, const char *key, uint32_t def) {
	char text[0x100];
	byte c[4];

	if (Dat_GetText(self->dat, sect, key, text, sizeof(text)) != 0) {
		return def;
	}
	if (Proc_Scan(text, "&b&b&b&b", &c[0], &c[1], &c[2], &c[3]) != 4) {
		ArchINI_Invalid(sect, key, text);
		return def;
	}
	return (uint32_t)c[3] << 24 | (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
}

byte __thiscall CArchINI_GetSectionName(CArchINI *self, int idx, char *dest, int size) {
	if (Arch_LineType(self->dat, idx) != LINEA_SECTION) {
		return 0;
	}
	Arch_SectionName(dest, Arch_Line(self->dat, idx)->text, size);
	return 1;
}

/* dest, or NULL if the key is missing and there is no default */
char *__thiscall CArchINI_GetText(CArchINI *self, const char *sect, const char *key, char *dest, int size,
	const char *def) {
	if (Dat_GetText(self->dat, sect, key, dest, size) != 0) {
		if (!def) {
			return NULL;
		}
		Cad_Copy(dest, def, size);
	}
	return dest;
}

float *__thiscall CArchINI_GetVec2(CArchINI *self, float *out, const char *sect, const char *key, const float *def) {
	char text[0x100];
	float v[2];

	if (Dat_GetText(self->dat, sect, key, text, sizeof(text)) != 0) {
		out[0] = def[0];
		out[1] = def[1];
		return out;
	}
	if (Proc_Scan(text, "&f&f", &v[0], &v[1]) != 2) {
		ArchINI_Invalid(sect, key, text);
		out[0] = def[0];
		out[1] = def[1];
		return out;
	}
	out[0] = v[0];
	out[1] = v[1];
	return out;
}

vec3_t *__thiscall CArchINI_GetVec3(CArchINI *self, vec3_t *out, const char *sect, const char *key, const vec3_t *def) {
	char text[0x100];
	vec3_t v;

	if (Dat_GetText(self->dat, sect, key, text, sizeof(text)) != 0) {
		*out = *def;
		return out;
	}
	if (Proc_Scan(text, "&f&f&f", &v.x, &v.y, &v.z) != 3) {
		ArchINI_Invalid(sect, key, text);
		*out = *def;
		return out;
	}
	*out = v;
	return out;
}

void __thiscall CArchINI_ReadBlock(CArchINI *self, CArchIO *f) {
	char line[INI_LOADSAVE_MAXLINE];

	CHECK(!Arch_Count(self->dat));
	CArchIO_BeginBlock(f, "INI");
	int version = CArchIO_ReadInt(f);
	CHECK(version == 1);
	for (int n = CArchIO_ReadInt(f); n > 0; n--) {
		CArchIO_ReadString(f, line, sizeof(line));
		Arch_InsertAt(self->dat, line, Arch_Count(self->dat));
	}
	CArchIO_Error(f, "INI");
}

void __thiscall CArchINI_Clear(CArchINI *self) {
	Arch_Free(self->dat);
	self->dat = Arch_New();
}

void __thiscall CArchINI_WriteBlock(CArchINI *self, CArchIO *f) {
	CHECK(Arch_Count(self->dat));
	CArchIO_BeginBlockWrite(f, "INI");
	CArchIO_WriteInt(f, 1);
	int n = Arch_Count(self->dat);
	CArchIO_WriteInt(f, n);
	for (int i = 1; i <= n; i++) {
		cLineaDat *lin = Arch_Line(self->dat, i);
		const char *line = lin ? lin->text : NULL;
		CHECK(strlen(line) < INI_LOADSAVE_MAXLINE);
		CArchIO_WriteString(f, line);
	}
	CArchIO_EndBlockWrite(f, "INI");
}

int __thiscall CArchINI_SetYesNo(CArchINI *self, const char *sect, const char *key, byte v) {
	return Dat_SetStr(self->dat, sect, key, v ? "YES" : "NO");
}

/* prec digits after the point */
int __thiscall CArchINI_SetFloat(CArchINI *self, const char *sect, const char *key, float v, int prec) {
	char text[0x40];

	Bprintf(text, sizeof(text), "%.*f", prec, (double)v);
	return Dat_SetStr(self->dat, sect, key, text);
}

int __thiscall CArchINI_SetInt(CArchINI *self, const char *sect, const char *key, int v) {
	return Dat_SetInt(self->dat, sect, key, v);
}

int __thiscall CArchINI_SetStr(CArchINI *self, const char *sect, const char *key, const char *v) {
	return Dat_SetStr(self->dat, sect, key, v);
}

/* the lines of a file after the ones there are; verbose logs a missing file once more */
int __thiscall CArchINI_Load(CArchINI *self, const char *name, const char *dir, byte verbose) {
	char path[MAX_RUTA];

	ArchINI_Path(path, name, dir);
	int r = Arch_Load(self->dat, path, Arch_Count(self->dat));
	if (verbose) {
		E3D_LogDebug(g_FileLog, "INI: %s%s", path, r == 0 ? "" : " <not found>");
	}
	E3D_LogInfo(g_FileLog, "INI: %s%s", path, r == 0 ? " <ok>" : " <not found>");
	return r;
}

/* idx < 0: at the end */
void __thiscall CArchINI_AddLine(CArchINI *self, const char *text, int idx) {
	if (idx < 0) {
		idx = Arch_Count(self->dat);
	}
	Arch_InsertAt(self->dat, text, idx);
}

void __thiscall CArchINI_DeleteLine(CArchINI *self, int idx) {
	Arch_Delete(self->dat, idx);
}

/* "SI" / "NO" */
int __thiscall CArchINI_SetBool(CArchINI *self, const char *sect, const char *key, int v) {
	return Dat_SetBool(self->dat, sect, key, v);
}

/* 0 or -1; the third argument is not used */
int __thiscall CArchINI_Save(CArchINI *self, const char *name, const char *dir, int unused) {
	char path[MAX_RUTA];

	(void)unused;
	ArchINI_Path(path, name, dir);
	void *f = File_OpenRel(path, "wt");
	if (!f) {
		return -1;
	}
	int r = Arch_Write(self->dat, f);
	crt_fclose(f);
	return r;
}

int __thiscall CArchINI_ReadBool(CArchINI *self, const char *sect, const char *key, int *out) {
	return Dat_GetBool(self->dat, sect, key, out);
}

int __thiscall CArchINI_ReadFloat(CArchINI *self, const char *sect, const char *key, float *out) {
	return Dat_GetFloat(self->dat, sect, key, out);
}

int __thiscall CArchINI_ReadInt(CArchINI *self, const char *sect, const char *key, int *out) {
	return Dat_GetLong(self->dat, sect, key, out);
}

int __thiscall CArchINI_ReadText(CArchINI *self, const char *sect, const char *key, char *dest, int size) {
	return Dat_GetText(self->dat, sect, key, dest, size);
}

const char *__thiscall CArchINI_LineText(CArchINI *self, int idx) {
	cLineaDat *lin = Arch_Line(self->dat, idx);
	return lin ? lin->text : NULL;
}

int __thiscall CArchINI_LineType(CArchINI *self, int idx) {
	return Arch_LineType(self->dat, idx);
}
