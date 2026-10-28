/*
 * Materials of the engine3d, original source Engine/Material.cpp (include/scrap/material.h): the maps and their
 * files, the material lists, reading and writing the "MAT" and "MAP" blocks, the D3D colors of a material.
 */
#include <string.h>

#include "scrap/bmp.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/fxtextura.h"
#include "scrap/material.h"
#include "scrap/textura.h"
#include "scrap/win32.h"

#define TEXTEST_CTOR 0x233990
#define TEXTEST_DTOR 0x233c70

int Vec3_Differ(const float *a, const float *b);

typedef void *(__thiscall *deleteObj_t)(void *self, int flags);

/* set while a material is deleted: the textures without references go too */
#define g_TexDeleteUnused GAME_DATA(int, 0x4aa318)
#define g_AnimCicloDefault GAME_DATA(int, 0x4aa31c)
/* engine variables DDS_MarkSource, LoadTextures, DDS_CheckMark, MatEmissive, DDS_UseTextures */
#define g_DDSMarkSource GAME_DATA(int, 0x4aa330)
#define g_LoadTextures GAME_DATA(int, 0x4aa348)
#define g_DDSCheckMark GAME_DATA(int, 0x4aa360)
#define g_MatEmissive GAME_DATA(float, 0x4aa378)
#define g_DDSUseTextures GAME_DATA(int, 0x4aa390)
#define g_RutasTexturas ((char (*)[MAX_RUTA])Game_Ptr(0x4aa398))
#define g_ExtensTexturas ((char (*)[MAX_RUTA])Game_Ptr(0x4aadc0))
#define g_MatStr ((char *)Game_Ptr(0x4ab7e8))
#define g_LightmapsHigh GAME_DATA(int, 0x4afdb0)
#define g_AnimFpsDefault GAME_DATA(float, 0x431d20)
#define g_BlendLegacy ((const uint32_t *)Game_Ptr(0x431d30))
#define g_BlendTable ((const uint32_t *)Game_Ptr(0x431d34))
#define g_MaxLightmap GAME_DATA(int, 0x432970)

#define BLEND_MODES 6
#define MAT_FILE_VERSION 3
#define MAPA_FILE_VERSION 3
#define FILE_HIDDEN 2
#define MAT_BYTE 0x1.010102p-8f		/* 1 / 255 */

static int Mat_IsShareable(const CMaterial *m) {
	return !(m->params.atrib & MAT_NOSHARE);
}

static void Mat_DeleteMapa(CMapa **slot) {
	CMapa *m = *slot;

	if (m) {
		CMapa_dtor(m);
		operator_delete_sized(m, sizeof(CMapa));
		*slot = NULL;
	}
}

static CMapa *Mat_NewMapa(void) {
	CMapa *m = operator_new(sizeof(CMapa));

	return m ? CMapa_ctor(m) : NULL;
}

static CMapa *Mat_SetMapCopy(CMaterial *self, uint32_t idmapa, const CMapa *orig) {
	CHECK(idmapa < MAPA_TOTAL);
	CHECK(orig != self->mapa[idmapa] || orig == NULL);
	Mat_DeleteMapa(&self->mapa[idmapa]);
	if (orig) {
		CMapa *m = Mat_NewMapa();
		self->mapa[idmapa] = m;
		CMapa_Assign(m, orig);
	}
	return self->mapa[idmapa];
}

static int Mat_StrSign(const char *a, const char *b) {
	int r = strcmp(a, b);

	return r < 0 ? -1 : r > 0;
}

void __fastcall CObjRef_dtor(int *Refs) {
	CHECK(!*Refs);
}

void __fastcall CTextEst_dtor(CTextEst *self) {
	if (!Textura_Release(self->Hint) && g_TexDeleteUnused) {
		Textura_Delete(self->Hint);
	}
	self->tex = NULL;
	self->Hint = NULL;
}

/* the vector deleting destructor (flags 2: an array of new[]) */
CTextEst *__thiscall CTextEst_Delete(CTextEst *self, byte flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(CTextEst), *mem, Game_Ptr(TEXTEST_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (CTextEst *)mem;
	}
	CTextEst_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CTextEst));
	}
	return self;
}

void __thiscall CMapa_Assign(CMapa *self, const CMapa *orig) {
	Cad_Set(&self->Nombre, orig->Nombre);
	self->b8 = orig->b8;
	self->lightmap = orig->lightmap;
	self->ba = orig->ba;
	self->bb = orig->bb;
	self->bc = orig->bc;
	self->bd = orig->bd;
	self->fx = orig->fx;
	memcpy(self->f10, orig->f10, sizeof(self->f10));
	memcpy(self->f18, orig->f18, sizeof(self->f18));
	memcpy(self->v20, orig->v20, sizeof(self->v20));
	self->f2c = orig->f2c;
}

void __thiscall CMaterial_Assign(CMaterial *self, const CMaterial *orig) {
	Cad_Set(&self->Nombre, orig->Nombre);
	self->params = orig->params;
	for (uint32_t i = 0; i < MAPA_TOTAL; i++) {
		Mat_SetMapCopy(self, i, orig->mapa[i]);
	}
}

byte Color_Equal(const byte *a, const byte *b) {
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

/* the 0x18 bytes from params.i2c */
byte __thiscall MatParams_Equal(const uint32_t *self, const uint32_t *other) {
	for (int i = 0; i < 6; i++) {
		if (self[i] != other[i]) {
			return 0;
		}
	}
	return 1;
}

int Vec2_Differ(const float *a, const float *b) {
	return !(a[0] == b[0] && a[1] == b[1]);
}

CMaterial *__thiscall CMaterial_Delete(CMaterial *self, byte flags) {
	CMaterial_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CMaterial));
	}
	return self;
}

void Mat_ConvertBlend(uint32_t *mode, byte toFile, byte legacy) {
	if (legacy) {
		CHECK(*mode - 1 < BLEND_MODES);
		*mode = g_BlendLegacy[*mode];
		return;
	}
	if (toFile) {
		uint32_t i;
		for (i = 0; i < BLEND_MODES; i++) {
			if (g_BlendTable[i] == *mode) {
				break;
			}
		}
		*mode = i;
		CHECK(i < BLEND_MODES);
		return;
	}
	CHECK(*mode < BLEND_MODES);
	*mode = g_BlendTable[*mode];
}

byte __fastcall CMapa_BuildMultiSprite(CMapa *self) {
	if (!self->MultiSprite) {
		CHECK(self->est.Hint);
		self->MultiSprite = Textura_GetMultiSprite(self->est.Hint, self->Nombre);
		CHECK(self->MultiSprite);
	}
	return 1;
}

void *__thiscall CMaterial_GetMultiSprite(CMaterial *self, uint32_t idmapa) {
	CHECK(idmapa < MAPA_TOTAL);
	CMapa *m = self->mapa[idmapa];
	if (!m) {
		E3D_LogWhite(g_FileLog, "BuildMultiSpriteTable: Map not found in Material: %s", self->Nombre);
		return NULL;
	}
	if (CMapa_BuildMultiSprite(m)) {
		return m->MultiSprite;
	}
	return NULL;
}

int __thiscall CMapa_Read(CMapa *self, CArchIO *f) {
	char buf[MAX_RUTA];

	CArchIO_BeginBlock(f, "MAP");
	int version = CArchIO_ReadInt(f);
	CHECK(version >= 2 && version <= 3);
	CArchIO_ReadString(f, buf, MAX_RUTA);
	Cad_Set(&self->Nombre, buf);
	CArchIO_Read(f, &self->b8, 1);
	CArchIO_Read(f, &self->lightmap, 1);
	CArchIO_Read(f, &self->ba, 1);
	CArchIO_Read(f, &self->bb, 1);
	CArchIO_Read(f, &self->bc, 1);
	CArchIO_Read(f, &self->bd, 1);
	CArchIO_Read(f, &self->fx, 1);
	CArchIO_Read(f, self->f10, 8);
	CArchIO_Read(f, self->f18, 8);
	CArchIO_Read(f, &self->f2c, 4);
	if (version > 2) {
		CArchIO_Read(f, self->v20, 0xc);
	}
	return CArchIO_Error(f, "MAP");
}

int __thiscall CMaterial_Read(CMaterial *self, CArchIO *f) {
	char buf[0x100];

	CArchIO_BeginBlock(f, "MAT");
	int version = CArchIO_ReadInt(f);
	CHECK(version >= 1 && version <= 3);
	if (version > 1) {
		CArchIO_ReadString(f, buf, sizeof(buf));
		Cad_Set(&self->Nombre, buf);
	}
	CArchIO_Read(f, self->params.ambient, 4);
	CArchIO_Read(f, self->params.diffuse, 4);
	CArchIO_Read(f, self->params.color, 4);
	CArchIO_Read(f, self->params.specular, 4);
	CArchIO_Read(f, self->params.emissive, 4);
	CArchIO_Read(f, &self->params.power, 4);
	CArchIO_Read(f, &self->params.specLevel, 4);
	CArchIO_Read(f, &self->params.i2c, 0x18);
	for (uint32_t i = 0; i < MAPA_TOTAL; i++) {
		CHECK(i < MAPA_TOTAL);
		Mat_DeleteMapa(&self->mapa[i]);
		/* version 2 has 3 maps */
		if (version <= 2 && i > 2) {
			break;
		}
		if (CArchIO_ReadInt(f)) {
			CMapa *m = Mat_NewMapa();
			self->mapa[i] = m;
			CMapa_Read(m, f);
		}
	}
	return CArchIO_Error(f, "MAT");
}

/* SetFileAttributes with exactly attr, if the file has none of its bits */
void DDS_Hide(const char *name, uint32_t attr) {
	uint32_t a = exe_GetFileAttributesA(name);

	if (a == 0xffffffff || (attr & a)) {
		return;
	}
	if (!exe_SetFileAttributesA(name, attr)) {
		E3D_LogBlue(g_FileLog, "DDS: Could not hide: %s", name);
		return;
	}
	E3D_LogWhite(g_FileLog, "DDS: Hide: %s", name);
}

void DDS_Unhide(const char *name, uint32_t attr) {
	uint32_t a = exe_GetFileAttributesA(name);

	if (a == 0xffffffff || !(attr & a)) {
		return;
	}
	E3D_LogBlue(g_FileLog, "DDS: Hidden file: %s", name);
	if (!exe_SetFileAttributesA(name, ~attr & a)) {
		E3D_LogBlue(g_FileLog, "DDS: Could not unhide: %s", name);
		return;
	}
	E3D_LogWhite(g_FileLog, "DDS: Unhide: %s", name);
}

CMaterial *Mat_New(const CMaterial *orig, byte share) {
	if (share) {
		CMaterial *m = CListaMat_Find(g_ListaMat, orig);
		if (m) {
			return m;
		}
	}
	CMaterial *m = operator_new(sizeof(CMaterial));
	m = m ? CMaterial_ctor(m) : NULL;
	CMaterial_Assign(m, orig);
	CMaterial_Prepare(m, g_MotorDev);
	CListaMat_Add(g_ListaMat, m);
	return m;
}

CMaterial *Mat_AddCopy(const CMaterial *orig) {
	CMaterial *m = operator_new(sizeof(CMaterial));
	m = m ? CMaterial_ctor(m) : NULL;
	CMaterial_Assign(m, orig);
	CListaMat_Add(g_ListaMat, m);
	return m;
}

void AnimMat_Free(CAnimMat **slot) {
	if (*slot) {
		CAnimMat *a = *slot;
		CAnimMat_dtor(a);
		operator_delete_sized(a, sizeof(CAnimMat));
		*slot = NULL;
	}
}

void AnimMat_FromMapa(CAnimMat **slot, const CMapa *mapa) {
	if (mapa && mapa->AnimMat) {
		void *p = operator_new(sizeof(CAnimMat));
		*slot = p ? CAnimMat_ctorCopy(p, mapa->AnimMat) : NULL;
		CAnimMat_Start(*slot);
	}
}

void Render_SetMaterialAnim(CMaterial *m, CAnimMat *anim) {
	if (!anim) {
		Render_SetMaterial(m, 0);
		return;
	}
	CMapa *mapa = m->mapa[0];
	void *old = mapa->AnimMat;
	mapa->AnimMat = anim;
	Render_SetMaterial(m, 0);
	mapa->AnimMat = old;
}

int __thiscall CMaterial_GetId(CMaterial *self, CMatId **out) {
	CMatId_Check(&self->id, self);
	if (out) {
		*out = &self->id;
	}
	return self->id.id;
}

int __fastcall CMaterial_Id(CMaterial *self) {
	if (self->id.id == -1) {
		CMatId_Register(&self->id, self);
	}
	return self->id.id;
}

char *__thiscall CMaterial_GetShaderName(CMaterial *self, char *str, uint32_t nbytes) {
	CHECK(str);
	if (!self->Nombre || !(self->params.atrib & MAT_SHADER)) {
		return NULL;
	}
	const char *name = strstr(self->Nombre, "(+shader:");
	if (!name) {
		CHECK(name);
		return NULL;
	}
	name += 9;
	while (*name == ' ') {
		name++;
	}
	CHECK(*name);
	const char *end = name;
	while (*end && *end != ' ' && *end != ')') {
		end++;
	}
	CHECK(*end);
	uint32_t cnt = end - name;
	CHECK(cnt < nbytes);
	memcpy(str, name, cnt);
	str[cnt] = 0;
	return str;
}

int __thiscall CMapa_Write(CMapa *self, CArchIO *f) {
	char buf[MAX_RUTA];

	CArchIO_BeginBlockWrite(f, "MAP");
	CArchIO_WriteInt(f, MAPA_FILE_VERSION);
	CHECK(self->Nombre);
	Ruta_Part(buf, self->Nombre, RUTA_DIR | RUTA_NAME | RUTA_EXT);
	CArchIO_WriteString(f, buf);
	CArchIO_Write(f, &self->b8, 1);
	CArchIO_Write(f, &self->lightmap, 1);
	CArchIO_Write(f, &self->ba, 1);
	CArchIO_Write(f, &self->bb, 1);
	CArchIO_Write(f, &self->bc, 1);
	CArchIO_Write(f, &self->bd, 1);
	CArchIO_Write(f, &self->fx, 1);
	CArchIO_Write(f, self->f10, 8);
	CArchIO_Write(f, self->f18, 8);
	CArchIO_Write(f, &self->f2c, 4);
	CArchIO_Write(f, self->v20, 0xc);
	return CArchIO_EndBlockWrite(f, "MAP");
}

int __thiscall CMaterial_Write(CMaterial *self, CArchIO *f) {
	CArchIO_BeginBlockWrite(f, "MAT");
	CArchIO_WriteInt(f, MAT_FILE_VERSION);
	CArchIO_WriteString(f, self->Nombre);
	CArchIO_Write(f, self->params.ambient, 4);
	CArchIO_Write(f, self->params.diffuse, 4);
	CArchIO_Write(f, self->params.color, 4);
	CArchIO_Write(f, self->params.specular, 4);
	CArchIO_Write(f, self->params.emissive, 4);
	CArchIO_Write(f, &self->params.power, 4);
	CArchIO_Write(f, &self->params.specLevel, 4);
	CArchIO_Write(f, &self->params.i2c, 0x18);
	for (int i = 0; i < MAPA_TOTAL; i++) {
		if (self->mapa[i]) {
			CArchIO_WriteInt(f, 1);
			CMapa_Write(self->mapa[i], f);
		} else {
			CArchIO_WriteInt(f, 0);
		}
	}
	return CArchIO_EndBlockWrite(f, "MAT");
}

byte Mat_CompareNames(const CMaterial *a, const CMaterial *b) {
	CHECK((a->params.atrib & MAT_NAMED) == (b->params.atrib & MAT_NAMED));
	if ((a->params.atrib & MAT_NAMED) || (b->params.atrib & MAT_NAMED)) {
		CHECK(a->Nombre && b->Nombre);
		if (strcmp(a->Nombre, b->Nombre)) {
			E3D_LogInfo(g_FileLog, "MatCompareNames: [%s] [%s]", a->Nombre, b->Nombre);
			return 0;
		}
	}
	return 1;
}

const char *Mat_Str(const CMaterial *m) {
	if (m) {
		Bprintf(g_MatStr, 0x100, "%s [%s]", m->Nombre, m->mapa[0] ? m->mapa[0]->Nombre : NULL);
		return g_MatStr;
	}
	Bprintf(g_MatStr, 0x100, "(null material)");
	return g_MatStr;
}

void __thiscall CListaMat_Add(CListaMat *self, CMaterial *mat) {
	CHECK(mat);
	CHECK(!mat->ant && !mat->sig);
	if (!self->ult) {
		self->ult = self->pri;
	}
	if (self->ult) {
		while (self->ult->sig) {
			self->ult = self->ult->sig;
		}
		self->ult->sig = mat;
		mat->ant = self->ult;
		self->ult = mat;
		mat->Refs++;
		return;
	}
	self->pri = mat;
	self->ult = mat;
	mat->Refs++;
}

void Tex_AddExtension(const char *ext) {
	char tmp[0x114];
	int i;

	if (!ext || !*ext) {
		return;
	}
	for (i = 0; i < MAX_EXTENS_TEXTURAS; i++) {
		if (!g_ExtensTexturas[i][0]) {
			break;
		}
	}
	CHECK(i < MAX_EXTENS_TEXTURAS);
	uint32_t j = 0;
	byte prefix = 1;
	do {
		CHECK(j < sizeof(tmp) - 2);
		char c = *ext;
		if (c == '&') {
			tmp[j++] = '&';
			tmp[j] = '&';
		} else {
			if (c == '<') {
				const char *x = ext + 1;
				CHECK(ext[1] != 0);
				ext += 2;
				CHECK(*ext == '>');
				tmp[j++] = '&';
				prefix = 0;
				c = *x;
			}
			tmp[j] = c;
		}
		ext++;
		j++;
	} while (*ext);
	if (j >= sizeof(tmp)) {
		crt_report_rangecheckfailure();
	}
	tmp[j] = 0;
	CHECK(j + 1 <= MAX_RUTA);
	char *d = g_ExtensTexturas[i];
	if (prefix) {
		Bprintf(d, MAX_RUTA, "&u&d&n.%s", tmp);
	} else {
		Cad_Copy(d, tmp, MAX_RUTA);
	}
	E3D_LogDebug(g_FileLog, "DBG: ExtensTexturas%d: '%s'", i + 1, d);
}

void Tex_AddPath(const char *ruta) {
	int i;

	if (!ruta || !*ruta) {
		return;
	}
	for (i = 0; i < MAX_RUTAS_TEXTURAS; i++) {
		if (!g_RutasTexturas[i][0]) {
			break;
		}
	}
	CHECK(i < MAX_RUTAS_TEXTURAS);
	Ruta_Copy(g_RutasTexturas[i], ruta, MAX_RUTA);
	E3D_LogDebug(g_FileLog, "DBG: RutaTextura%d: '%s'", i + 1, g_RutasTexturas[i]);
}

void Tex_ClearExtensions(void) {
	for (int i = 0; i < MAX_EXTENS_TEXTURAS; i++) {
		g_ExtensTexturas[i][0] = 0;
	}
}

void Tex_ClearPaths(void) {
	for (int i = 0; i < MAX_RUTAS_TEXTURAS; i++) {
		g_RutasTexturas[i][0] = 0;
	}
}

CMaterial *__thiscall CListaMat_Find(CListaMat *self, const CMaterial *mat) {
	CHECK(!self->pri || !self->pri->ant);
	CHECK(Mat_IsShareable(mat));
	for (CMaterial *m = self->pri; m; m = m->sig) {
		const CMatParams *a = &m->params, *b = &mat->params;
		/* the four colors up to power as bytes */
		if (!memcmp(a->ambient, b->ambient, 0x10) && a->power == b->power && a->specLevel == b->specLevel
			&& Color_Equal(a->color, b->color) && MatParams_Equal((const uint32_t *)&a->i2c, (const uint32_t *)&b->i2c)) {
			int i;
			for (i = 0; i < MAPA_TOTAL; i++) {
				if (!CMapa_Equal(m->mapa[i], mat->mapa[i])) {
					break;
				}
			}
			if (i == MAPA_TOTAL && Mat_CompareNames(m, mat)) {
				CHECK(Mat_IsShareable(m));
				return m;
			}
		}
		if (m->sub) {
			CMaterial *r = CListaMat_Find(m->sub, mat);
			if (r) {
				return r;
			}
		}
	}
	return NULL;
}

byte Tex_TryFormat(const char *frmt, const char *name, char *out, int alpha) {
	char tmp[MAX_RUTA];

	Ruta_Format(tmp, name, frmt);
	if (alpha) {
		Ruta_Format(tmp, tmp, "&U&D&N.alpha&E");
	}
	if (File_Exists(tmp)) {
		Cad_Copy(out, tmp, MAX_RUTA);
		return 1;
	}
	return 0;
}

int Tex_FindDDS(const char *name, char *out, int alpha) {
	char ext[MAX_RUTA];

	Ruta_Split(name, NULL, NULL, NULL, ext);
	crt_strlwr(ext);
	if (g_DDSUseTextures) {
		if (!strcmp(ext, ".bmp") || !strcmp(ext, ".tga")) {
			if (Tex_TryFormat("&U&D&N.dds", name, out, alpha)) {
				return 0;
			}
		}
		if (Tex_TryFormat("&U&Ddds/&N.dds", name, out, alpha)) {
			return 0;
		}
	}
	for (int i = 0; i < MAX_EXTENS_TEXTURAS && g_ExtensTexturas[i][0]; i++) {
		if (Tex_TryFormat(g_ExtensTexturas[i], name, out, alpha)) {
			return 0;
		}
	}
	return -1;
}

int Tex_FindInPaths(const char *name, char *out) {
	char file[MAX_RUTA];
	char tmp[MAX_RUTA];

	if (!g_RutasTexturas[0][0]) {
		return -1;
	}
	Ruta_Part(file, name, RUTA_NAME | RUTA_EXT);
	for (int i = 0; i < MAX_RUTAS_TEXTURAS && g_RutasTexturas[i][0]; i++) {
		Ruta_Merge(tmp, g_RutasTexturas[i], file);
		if (File_Exists(tmp)) {
			Cad_Copy(out, tmp, MAX_RUTA);
			return 0;
		}
	}
	return -1;
}

static void Mat_LogOlder(const char *dds, const char *orig) {
	if (File_TimeDiff(dds, orig, 0) < 0) {
		E3D_LogVerbose(g_FileLog, "WARN: [%s] is older than [%s]", Ruta_FileName(dds), Ruta_FileName(orig));
	}
}

char *__thiscall CMapa_ResolvePath(CMapa *self, byte load, CMapa *alpha) {
	char path[MAX_RUTA];
	char found[MAX_RUTA];
	char drive[4], dir[0x100], name[0x100], ext[0x100];

	CHECK(self->Nombre);
	self->state = 0;
	Cad_Set(&self->ruta, NULL);
	self->animated = 0;
	self->frames = 0;
	Ruta_Copy(path, self->Nombre, MAX_RUTA);
	/* the alpha map as a flag: the DDS with both */
	if (load && !Tex_FindDDS(path, found, (int)(intptr_t)alpha)) {
		Mat_LogOlder(found, path);
		if (alpha && alpha->ruta) {
			Mat_LogOlder(found, alpha->ruta);
		}
		if (g_DDSMarkSource) {
			DDS_Hide(path, FILE_HIDDEN);
			if (alpha && alpha->ruta) {
				DDS_Hide(alpha->ruta, FILE_HIDDEN);
			}
		}
		memcpy(path, found, strlen(found) + 1);
	} else if (!File_Exists(path)) {
		Ruta_Split(path, drive, dir, name, ext);
		Bprintf(path, MAX_RUTA, "%s%sbmp/%s%s", drive, dir, name, ext);
		if (!File_Exists(path) && Tex_FindInPaths(self->Nombre, path)) {
			/* numbered frames from 0000 */
			Bprintf(path, MAX_RUTA, "%s%s%s%s%s", drive, dir, name, "%04d", ext);
			Bprintf(found, MAX_RUTA, path, 0);
			if (!File_Exists(found)) {
				Bprintf(path, MAX_RUTA, "%s%sbmp/%s%s%s", drive, dir, name, "%04d", ext);
				Bprintf(found, MAX_RUTA, path, 0);
				if (!File_Exists(found)) {
					if (load) {
						Con_Warnf("WARN: Texture not found: '%s'\n", self->Nombre);
					}
					self->state = MAPA_MISSING;
					return NULL;
				}
			}
			Cad_Set(&self->rutaAni, path);
			memcpy(path, found, strlen(found) + 1);
			self->animated = 1;
		}
	}
	if (!self->animated) {
		/* name.000.ext: frames name.%03d.ext */
		char *p = strstr(Ruta_FileName(path), ".000.");
		if (p) {
			Bprintf(found, MAX_RUTA, "%.*s.%s.%s", (int)(p - path), path, "%03d", p + 5);
			Cad_Set(&self->rutaAni, found);
			self->animated = 1;
		}
	}
	Cad_Set(&self->ruta, path);
	if (g_DDSCheckMark && load) {
		DDS_Unhide(path, FILE_HIDDEN);
	}
	return self->ruta;
}

void __thiscall CMapa_LoadTexture(CMapa *self, void *dev, CMapa *alpha, int kind) {
	char buf[MAX_RUTA];
	char abuf[MAX_RUTA];
	char ini_name[MAX_RUTA];
	int info[5];
	CArchINI ini;

	if (self->est.tex || self->state == MAPA_MISSING) {
		return;
	}
	const char *alphaPath = NULL;
	if (self->est.Hint) {
		self->est.tex = Textura_GetD3D(self->est.Hint, dev);
		self->cur = &self->est;
		if (!self->est.tex || (!self->animated && !self->fx)) {
			return;
		}
	}
	CMapa_Release(self);
	if (alpha && alpha->state != MAPA_MISSING) {
		CMapa_ResolvePath(alpha, 0, NULL);
		alphaPath = alpha->ruta;
	}
	if (!CMapa_ResolvePath(self, 1, alpha)) {
		return;
	}
	byte lm = self->lightmap;
	if (lm) {
		if (!dev) {
			return;
		}
		if ((int)lm > g_MaxLightmap || (lm > 1 && !g_LightmapsHigh)) {
			Con_Verbosef("INFO: Skipping lightmap texture: '%s'\n", self->Nombre);
			self->state = MAPA_MISSING;
			return;
		}
	}
	int flags, flagsAlt;
	if (self->b8 && !self->fx) {
		flags = 0x100;
		flagsAlt = 0x180;
	} else {
		flags = 0x120;
		flagsAlt = 0x1a0;
	}
	if (self->ba || self->fx) {
		flags = flagsAlt;
	}
	flags |= lm ? 0x40 : 0;
	if (self->animated) {
		int i = 0;
		self->frames = 0;
		CMapa_FramePath(self, i, buf, MAX_RUTA);
		while (File_Exists(buf)) {
			i++;
			CMapa_FramePath(self, i, buf, MAX_RUTA);
		}
		self->frames = i;
		if (i < 2) {
			E3D_LogVerbose(g_FileLog, "WARN: Animated texture with %d frame: %s", i, self->ruta);
		}
		CHECK(!self->TextAni);
		CHECK(!self->AnimMat);
		self->TextAni = New_Objects(self->frames, sizeof(CTextEst), TEXTEST_CTOR, TEXTEST_DTOR);
		if (self->frames > 0) {
			char *aname = alphaPath ? abuf : NULL;
			for (i = 0; i < self->frames; i++) {
				CMapa_FramePath(self, i, buf, MAX_RUTA);
				if (alphaPath) {
					CMapa_FramePath(alpha, i, abuf, MAX_RUTA);
				}
				Textura_Add(buf, aname, kind, flags);
				self->TextAni[i].Hint = Textura_Find(buf, aname);
				self->TextAni[i].tex = Textura_GetD3D(self->TextAni[i].Hint, dev);
			}
		}
		/* name.txa: [anim] fps, ciclo */
		CArchINI_ctor(&ini);
		int ciclo = g_AnimCicloDefault;
		float fps = g_AnimFpsDefault;
		Ruta_Copy(ini_name, self->Nombre, MAX_RUTA);
		CArchINI_Load(&ini, ini_name, ".txa", 0);
		if (CArchINI_FindSection(&ini, "anim", 1)) {
			CArchINI_ReadFloat(&ini, "anim", "fps", &fps);
			CArchINI_ReadInt(&ini, "anim", "ciclo", &ciclo);
		}
		CAnimMat *am = operator_new(sizeof(CAnimMat));
		am = am ? CAnimMat_ctor(am) : NULL;
		self->AnimMat = am;
		am->fps = fps;
		((CAnimMat *)self->AnimMat)->frames = self->frames;
		((CAnimMat *)self->AnimMat)->ciclo = ciclo;
		CAnimMat_Start(self->AnimMat);
		CArchINI_dtor(&ini);
	}
	Textura_Add(self->ruta, alphaPath, kind, flags);
	self->est.Hint = Textura_Find(self->ruta, alphaPath);
	self->est.tex = Textura_GetD3D(self->est.Hint, dev);
	self->cur = &self->est;
	if (self->est.tex && self->fx) {
		CHECK(!self->FXTextura);
		self->FXTextura = FXTextura_Create(self->fx, self);
	}
	if (!Textura_GetInfo(self->est.Hint, info)) {
		self->hasAlpha = info[3] != 0;
		self->b55 = info[4] != 0;
	}
	if (alpha && alpha->state == MAPA_MISSING && !self->hasAlpha) {
		Con_Warnf("WARN: Opacity map not found: '%s'\n", alpha->Nombre);
	}
}

void __thiscall CMaterial_Prepare(CMaterial *self, void *dev) {
	CMaterial_CalcColors(self, self->colors);
	if (self->sub) {
		CListaMat_Prepare(self->sub, dev);
	}
	if (g_LoadTextures != 2 || (self->params.atrib & MAT_LOADTEX)) {
		if (self->mapa[0]) {
			CMapa_LoadTexture(self->mapa[0], dev, self->mapa[1], 0);
		}
		if (self->mapa[2] && self->params.b3b) {
			CMapa_LoadTexture(self->mapa[2], dev, NULL, 1);
		}
		if (self->mapa[3]) {
			CMapa_LoadTexture(self->mapa[3], dev, NULL, 0);
		}
		if (self->mapa[4]) {
			CMapa_LoadTexture(self->mapa[4], dev, NULL, 0);
		}
	}
	if (self->id.id == -1) {
		CMatId_Register(&self->id, self);
	}
}

void __thiscall CListaMat_Prepare(CListaMat *self, void *dev) {
	CHECK(!self->pri || !self->pri->ant);
	for (CMaterial *m = self->pri; m; m = m->sig) {
		CMaterial_Prepare(m, dev);
	}
}

byte CMapa_Equal(const CMapa *a, const CMapa *b) {
	if (a == b) {
		return 1;
	}
	if (!a || !b) {
		return 0;
	}
	if (strcmp(a->Nombre, b->Nombre) || a->b8 != b->b8 || a->lightmap != b->lightmap || a->ba != b->ba || a->bb != b->bb
		|| a->bc != b->bc || a->bd != b->bd || a->fx != b->fx) {
		return 0;
	}
	if ((byte)Vec2_Differ(a->f10, b->f10) || (byte)Vec2_Differ(a->f18, b->f18) || (byte)Vec3_Differ(a->v20, b->v20)) {
		return 0;
	}
	return a->f2c == b->f2c;
}

int __thiscall CListaMat_Count(CListaMat *self, byte sub) {
	int n = 0;

	CHECK(!self->pri || !self->pri->ant);
	for (CMaterial *m = self->pri; m; m = m->sig) {
		if (sub && m->sub) {
			n += CListaMat_Count(m->sub, 0);
		}
		n++;
	}
	return n;
}

CMapa *__thiscall CMaterial_SetMap(CMaterial *self, uint32_t idmapa, const char *name) {
	CHECK(idmapa < MAPA_TOTAL);
	Mat_DeleteMapa(&self->mapa[idmapa]);
	if (name) {
		CMapa *m = Mat_NewMapa();
		self->mapa[idmapa] = m;
		Cad_Set(&m->Nombre, name);
	}
	return self->mapa[idmapa];
}

CMapa *__thiscall CMaterial_SetMapCopy(CMaterial *self, uint32_t idmapa, const CMapa *orig) {
	return Mat_SetMapCopy(self, idmapa, orig);
}

CMaterial *__thiscall CListaMat_Extract(CListaMat *self, CMaterial *mat, byte sub) {
	CMaterial *m;

	CHECK(mat);
	for (m = self->pri; m; m = m->sig) {
		if (m == mat) {
			break;
		}
		if (m->sub && sub) {
			CMaterial *r = CListaMat_Extract(m->sub, mat, 1);
			if (r) {
				return r;
			}
		}
	}
	if (!m) {
		return NULL;
	}
	m->Refs--;
	CHECK(m->Refs == 0);
	if (self->pri == m) {
		self->pri = m->sig;
	}
	if (self->ult == m) {
		self->ult = m->ant;
	}
	if (m->ant) {
		m->ant->sig = m->sig;
	}
	if (m->sig) {
		m->sig->ant = m->ant;
	}
	m->ant = NULL;
	m->sig = NULL;
	return m;
}

int Mat_Compare(const CMaterial *a, const CMaterial *b) {
	CHECK(a && b);
	if (a == b) {
		return 0;
	}
	byte ta = Mat_IsTransp(a);
	if (ta != Mat_IsTransp(b)) {
		return ta ? 1 : -1;
	}
	if (a->params.b3b != b->params.b3b) {
		return a->params.b3b ? 1 : -1;
	}
	if (!a->mapa[3] != !b->mapa[3]) {
		return a->mapa[3] ? 1 : -1;
	}
	if (!a->mapa[4] != !b->mapa[4]) {
		return a->mapa[4] ? 1 : -1;
	}
	const char *na = a->mapa[0] ? a->mapa[0]->Nombre : NULL;
	const char *nb = b->mapa[0] ? b->mapa[0]->Nombre : NULL;
	int r;
	if (na && nb) {
		r = Mat_StrSign(na, nb);
	} else {
		r = (nb == NULL) - (na == NULL);
	}
	return r ? r : (int)((uintptr_t)a - (uintptr_t)b);
}

void __thiscall CMaterial_CalcColors(CMaterial *self, float *out) {
	const CMatParams *p = &self->params;

	memset(out, 0, sizeof(self->colors));
	out[0] = (float)p->color[0] * MAT_BYTE;
	out[1] = (float)p->color[1] * MAT_BYTE;
	out[2] = (float)p->color[2] * MAT_BYTE;
	out[3] = 1.0f;
	out[4] = 1.0f;
	out[5] = 1.0f;
	out[6] = 1.0f;
	out[7] = 1.0f;
	out[8] = 0.0f;
	out[9] = 0.0f;
	out[10] = 0.0f;
	out[11] = 1.0f;
	out[12] = (float)p->emissive[0] * MAT_BYTE;
	out[13] = (float)p->emissive[1] * MAT_BYTE;
	out[14] = (float)p->emissive[2] * MAT_BYTE;
	out[15] = 1.0f;
	if (!self->mapa[0]) {
		out[0] = ((float)p->diffuse[0] / 255.0f) * out[0];
		out[1] = ((float)p->diffuse[1] / 255.0f) * out[1];
		out[2] = ((float)p->diffuse[2] / 255.0f) * out[2];
		if (p->atrib & MAT_MAPCOLOR) {
			out[4] = out[0] * 0.8f;
			out[5] = out[1] * 0.8f;
			out[6] = out[2] * 0.8f;
		} else {
			out[4] = (float)p->ambient[0] / 255.0f;
			out[5] = (float)p->ambient[1] / 255.0f;
			out[6] = (float)p->ambient[2] / 255.0f;
		}
	}
	out[8] = ((float)p->specular[0] * p->specLevel) / 255.0f;
	out[9] = ((float)p->specular[1] * p->specLevel) / 255.0f;
	out[10] = ((float)p->specular[2] * p->specLevel) / 255.0f;
	out[16] = p->power * 255.0f;
	/* the emissive part of the diffuse and ambient colors moves to emissive */
	float r = (float)p->emissive[0] * MAT_BYTE;
	float g = (float)p->emissive[1] * MAT_BYTE;
	float b = (float)p->emissive[2] * MAT_BYTE;
	float ir = 1.0f - r, ig = 1.0f - g, ib = 1.0f - b;
	out[12] = r * out[0];
	out[13] = out[1] * g;
	out[14] = out[2] * b;
	out[15] = out[3];
	out[0] = out[0] * ir;
	out[1] = out[1] * ig;
	out[2] = out[2] * ib;
	out[3] = out[3] * 0.0f;
	out[4] = out[4] * ir;
	out[5] = out[5] * ig;
	out[6] = out[6] * ib;
	out[7] = out[7] * 0.0f;
	float e = g_MatEmissive;
	out[12] = out[0] * e + out[12];
	out[13] = out[13] + out[1] * e;
	out[14] = out[14] + out[2] * e;
	out[15] = out[15] + out[3] * e;
	out[3] = (float)p->diffuse[3] / 255.0f;
}

void __fastcall CMapa_Release(CMapa *self) {
	self->cur = NULL;
	CTextEst_dtor(&self->est);
	self->state = 0;
	self->animated = 0;
	self->frames = 0;
	if (self->TextAni) {
		Delete_Objects(self->TextAni, sizeof(CTextEst), TEXTEST_DTOR);
		self->TextAni = NULL;
	}
	if (self->AnimMat) {
		CAnimMat_dtor(self->AnimMat);
		operator_delete_sized(self->AnimMat, sizeof(CAnimMat));
		self->AnimMat = NULL;
	}
	if (self->FXTextura) {
		((deleteObj_t)(*(void ***)self->FXTextura)[0])(self->FXTextura, 1);
		self->FXTextura = NULL;
	}
}

void __fastcall CMaterial_Release(CMaterial *self) {
	if (self->sub) {
		CListaMat_Release(self->sub);
	}
	for (int i = 0; i < MAPA_TOTAL; i++) {
		if (self->mapa[i]) {
			CMapa_Release(self->mapa[i]);
		}
	}
}

void __fastcall CListaMat_Release(CListaMat *self) {
	CHECK(!self->pri || !self->pri->ant);
	for (CMaterial *m = self->pri; m; m = m->sig) {
		CMaterial_Release(m);
	}
}

void __fastcall CListaMat_DeleteAll(CListaMat *self) {
	int n = 0;

	CHECK(!self->pri || !self->pri->ant);
	while (self->pri) {
		CMaterial *next = self->pri->sig;
		self->pri->Refs--;
		if (self->pri) {
			((deleteObj_t)self->pri->vftable[0])(self->pri, 1);
		}
		n++;
		self->pri = next;
	}
	E3D_LogDebug2(g_FileLog, "ListaMat: %d materials deleted", n);
	self->ult = NULL;
	self->pri = NULL;
}

void __fastcall CMaterial_MapToColor(CMaterial *self) {
	CMapa *m = self->mapa[0];

	if (!m) {
		return;
	}
	self->params.atrib |= MAT_MAPCOLOR;
	CMapa_ResolvePath(m, 0, NULL);
	if (m->ruta) {
		CBmp img = { 0, 0, NULL };
		CBmp avg = { 1, 1, NULL };
		avg.img = New_Array(1, 4);
		CBmp_Load(&img, m->ruta, 0);
		CBmp_Resample(&avg, &img, 0xf);
		uint32_t c = *CBmp_Pixel(&avg, 0, 0);
		self->params.diffuse[0] = c;
		self->params.diffuse[2] = c >> 16;
		self->params.diffuse[1] = c >> 8;
		avg.ancho = 0;
		avg.alto = 0;
		if (avg.img) {
			operator_delete_sized(avg.img, 4);
			avg.img = NULL;
		}
		img.ancho = 0;
		img.alto = 0;
		if (img.img) {
			operator_delete_sized(img.img, 4);
		}
	}
	Mat_DeleteMapa(&self->mapa[0]);
}

byte __thiscall CMapa_GetSize(CMapa *self, int *w, int *h) {
	int info[5];

	if (self->cur && !Textura_GetInfo(self->cur->Hint, info)) {
		if (w) {
			*w = info[0];
		}
		if (h) {
			*h = info[1];
		}
		return 1;
	}
	return 0;
}

void *__thiscall CMapa_GetTexture(CMapa *self, byte load) {
	CTextEst *e = self->cur;

	if (!e) {
		return NULL;
	}
	if (e->tex || !e->Hint) {
		return e->tex;
	}
	if (!load) {
		return NULL;
	}
	return Textura_Load(e->Hint, g_pd3dDevice);
}

char *__thiscall CMaterial_GetMapName(CMaterial *self, uint32_t idmapa) {
	CHECK(idmapa < MAPA_TOTAL);
	return self->mapa[idmapa] ? self->mapa[idmapa]->Nombre : NULL;
}

byte __thiscall CMaterial_GetMapSize(CMaterial *self, uint32_t idmapa, int *w, int *h) {
	CHECK(idmapa < MAPA_TOTAL);
	return self->mapa[idmapa] ? CMapa_GetSize(self->mapa[idmapa], w, h) : 0;
}

char *__thiscall CMapa_FramePath(CMapa *self, int frame, char *buf, int size) {
	if (self->animated) {
		CHECK(self->rutaAni);
		Bprintf(buf, size, self->rutaAni, frame);
		return buf;
	}
	CHECK(self->ruta);
	Cad_Copy(buf, self->ruta, size);
	return buf;
}

void __thiscall CListaMat_Delete(CListaMat *self, CMaterial *mat) {
	g_TexDeleteUnused = 1;
	CHECK(!Mat_IsShareable(mat));
	CMaterial *m = CListaMat_Extract(self, mat, 0);
	CHECK(m);
	((deleteObj_t)mat->vftable[0])(mat, 1);
	g_TexDeleteUnused = 0;
}

/* ---- the constructors and destructors of material.h and objref.h (rva 0x2336f0..) ---- */

CListaMat *__thiscall CListaMat_DeleteObj(CListaMat *self, byte flags) {
	CListaMat_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

CMapa *__thiscall CMapa_ctor(CMapa *self) {
	self->Nombre = NULL;
	self->ruta = NULL;
	self->est.tex = NULL;
	self->est.Hint = NULL;
	self->rutaAni = NULL;
	self->f10[0] = 0.0f;
	self->f10[1] = 0.0f;
	self->f18[0] = 1.0f;
	self->f18[1] = 1.0f;
	self->v20[0] = 0.0f;
	self->v20[1] = 0.0f;
	self->v20[2] = 0.0f;
	self->b8 = 1;
	self->lightmap = 0;
	self->ba = 0;
	self->bb = 0;
	self->bc = 3;
	self->bd = 0;
	self->fx = 0;
	self->f2c = 1.0f;
	self->cur = NULL;
	self->animated = 0;
	self->frames = 0;
	self->TextAni = NULL;
	self->AnimMat = NULL;
	self->FXTextura = NULL;
	self->hasAlpha = 0;
	self->b55 = 0;
	self->state = 0;
	self->MultiSprite = NULL;
	return self;
}

CMaterial *__thiscall CMaterial_ctor(CMaterial *self) {
	CMatParams *p = &self->params;
	int i;

	self->Nombre = NULL;
	self->Refs = 0;
	self->vftable = Game_Ptr(CMATERIAL_VFTABLE);
	CMatId_ctor(&self->id);
	p->ambient[0] = p->ambient[1] = p->ambient[2] = 0xff;
	p->ambient[3] = 0;
	p->diffuse[0] = p->diffuse[1] = p->diffuse[2] = 0xff;
	p->diffuse[3] = 0;
	p->color[0] = p->color[1] = p->color[2] = 0xff;
	p->color[3] = 0;
	memset(p->specular, 0, sizeof(p->specular));
	self->sig = NULL;
	self->ant = NULL;
	self->sub = NULL;
	memset(p->emissive, 0, sizeof(p->emissive));
	p->power = 0.0f;
	p->specLevel = 0.0f;
	p->i2c = 0;
	p->srcBlend = 5;
	p->dstBlend = 6;
	p->b38 = 0;
	p->b39 = 1;
	p->transp = 0;
	p->b3b = 0;
	p->atrib = 0;
	p->w3e = 0x100;
	p->i40 = 4;
	for (i = 0; i < MAPA_TOTAL; i++) {
		self->mapa[i] = NULL;
	}
	CMaterial_CalcColors(self, self->colors);
	return self;
}

/* a CBmp of the stack freed (the pixels with the sized delete of one pixel) */
void __fastcall CBmp_dtor(CBmp *self) {
	Img_Free(self, sizeof(uint32_t));
}

void __fastcall CListaMat_dtor(CListaMat *self) {
	self->vftable = Game_Ptr(CLISTAMAT_VFTABLE);
	CListaMat_DeleteAll(self);
	if (self->Nombre) {
		Cad_Set(&self->Nombre, NULL);
	}
}

void __thiscall CMapa_dtor(CMapa *self) {
	CMapa_Release(self);
	if (self->rutaAni) {
		Cad_Set(&self->rutaAni, NULL);
	}
	CTextEst_dtor(&self->est);
	if (self->ruta) {
		Cad_Set(&self->ruta, NULL);
	}
	if (self->Nombre) {
		Cad_Set(&self->Nombre, NULL);
	}
}

void __thiscall CMaterial_dtor(CMaterial *self) {
	int i;

	self->vftable = Game_Ptr(CMATERIAL_VFTABLE);
	if (self->ant) {
		self->ant->sig = self->sig;
	}
	if (self->sig) {
		self->sig->ant = self->ant;
	}
	for (i = 0; i < MAPA_TOTAL; i++) {
		Mat_DeleteMapa(&self->mapa[i]);
	}
	if (self->sub) {
		((deleteObj_t)self->sub->vftable[0])(self->sub, 1);
		self->sub = NULL;
	}
	CMatId_dtor(&self->id);
	CObjRef_dtor(&self->Refs);
	if (self->Nombre) {
		Cad_Set(&self->Nombre, NULL);
	}
}

void __fastcall Cad_dtor(char **self) {
	if (*self) {
		Cad_Set(self, NULL);
	}
}

byte __fastcall CMapa_HasAlpha(const CMapa *self) {
	return self->hasAlpha;
}

void __thiscall CMaterial_ScaleColors(CMaterial *self, float k) {
	int i;

	CMaterial_CalcColors(self, self->colors);
	if (k != 1.0f) {
		/* diffuse, ambient and emissive; the specular and the power stay */
		for (i = 0; i < 8; i++) {
			self->colors[i] = self->colors[i] * k;
		}
		for (i = 12; i < 16; i++) {
			self->colors[i] = self->colors[i] * k;
		}
		self->colors[3] = (float)self->params.diffuse[3] / 255.0f;
	}
}
