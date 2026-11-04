/*
 * Materials of the engine3d, original source Engine/Material.cpp (include/scrap/material.h): the maps and their
 * files, the material lists, reading and writing the "MAT" and "MAP" blocks, the D3D colors of a material.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/bmp.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fxtexture.h"
#include "scrap/material.h"
#include "scrap/texture.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"


int Vec3_Differ(const float *a, const float *b);

/* set while a material is deleted: the textures without references go too */
static cGVar GVar_DDS_MarkSource;

/* engine variables DDS_MarkSource, LoadTextures, DDS_CheckMark, MatEmissive, DDS_UseTextures */
#define g_DDSMarkSource (GVar_DDS_MarkSource.value.i)
cGVar GVar_LoadTextures;

#define g_LoadTextures (GVar_LoadTextures.value.i)
static cGVar GVar_DDS_CheckMark;
#define g_DDSCheckMark (GVar_DDS_CheckMark.value.i)
static cGVar GVar_MatEmissive;
#define g_MatEmissive (GVar_MatEmissive.value.f)
static cGVar GVar_DDS_UseTextures;
#define g_DDSUseTextures (GVar_DDS_UseTextures.value.i)
extern int g_ShadowsStencil;
#define g_LightmapsHigh g_ShadowsStencil
extern int g_MaxLightmap;

#define BLEND_MODES 6
#define MAT_FILE_VERSION 3
#define MAP_FILE_VERSION 3
#define FILE_HIDDEN 2
#define MAT_BYTE 0x1.010102p-8f		/* 1 / 255 */

static int Mat_IsShareable(const CMaterial *m) {
	return !(m->params.attrib & MAT_NOSHARE);
}

static void CMap_dtor(CMap *self);
static void Mat_DeleteMap(CMap **slot) {
	CMap *m = *slot;

	if (m) {
		CMap_dtor(m);
		operator_delete(m);
		*slot = NULL;
	}
}

static CMap *CMap_ctor(CMap *self);
static CMap *Mat_NewMap(void) {
	CMap *m = operator_new(sizeof(CMap));

	return m ? CMap_ctor(m) : NULL;
}

static void CMap_Assign(CMap *self, const CMap *orig);
static CMap *Mat_SetMapCopy(CMaterial *self, uint32_t mapid, const CMap *orig) {
	CHECK(mapid < MAP_TOTAL);
	CHECK(orig != self->map[mapid] || orig == NULL);
	Mat_DeleteMap(&self->map[mapid]);
	if (orig) {
		CMap *m = Mat_NewMap();
		self->map[mapid] = m;
		CMap_Assign(m, orig);
	}
	return self->map[mapid];
}

static int Mat_StrSign(const char *a, const char *b) {
	int r = strcmp(a, b);

	return r < 0 ? -1 : r > 0;
}

static void CObjRef_dtor(CObjRef *self) {
	CHECK(!self->Refs);
}

static CTextStatic *CTextStatic_ctor(CTextStatic *self) {
	self->tex = NULL;
	self->Hint = NULL;
	return self;
}

static int g_TexDeleteUnused;

static void CTextStatic_dtor(CTextStatic *self) {
	if (!Texture_Release(self->Hint) && g_TexDeleteUnused) {
		Texture_Delete(self->Hint);
	}
	self->tex = NULL;
	self->Hint = NULL;
}

/* name and parameters, not the textures */
static void CMap_Assign(CMap *self, const CMap *orig) {
	String_Set(&self->Name, orig->Name);
	self->filter = orig->filter;
	self->lightmap = orig->lightmap;
	self->ba = orig->ba;
	self->env2 = orig->env2;
	self->tile = orig->tile;
	self->mirror = orig->mirror;
	self->fx = orig->fx;
	memcpy(self->uvOffset, orig->uvOffset, sizeof(self->uvOffset));
	memcpy(self->uvScale, orig->uvScale, sizeof(self->uvScale));
	memcpy(self->uvRot, orig->uvRot, sizeof(self->uvRot));
	self->amount = orig->amount;
}

void CMaterial_Assign(CMaterial *self, const CMaterial *orig) {
	String_Set(&self->Name, orig->Name);
	self->params = orig->params;
	for (uint32_t i = 0; i < MAP_TOTAL; i++) {
		Mat_SetMapCopy(self, i, orig->map[i]);
	}
}

static byte Color_Equal(const byte *a, const byte *b) {
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

/* the from params.i2c */
static byte MatParams_Equal(const uint32_t *self, const uint32_t *other) {
	for (int i = 0; i < 6; i++) {
		if (self[i] != other[i]) {
			return 0;
		}
	}
	return 1;
}

static int Vec2_Differ(const float *a, const float *b) {
	return !(a[0] == b[0] && a[1] == b[1]);
}

void CMaterial_Free(CMaterial *self) {
	CMaterial_dtor(self);
	operator_delete(self);
}

CMatList g_MatList;

byte CMap_BuildMultiSprite(CMap *self) {
	if (!self->MultiSprite) {
		CHECK(self->staticTex.Hint);
		self->MultiSprite = Texture_GetMultiSprite(self->staticTex.Hint, self->Name);
		CHECK(self->MultiSprite);
	}
	return 1;
}

void *CMaterial_GetMultiSprite(CMaterial *self, uint32_t mapid) {
	CHECK(mapid < MAP_TOTAL);
	CMap *m = self->map[mapid];
	if (!m) {
		E3D_LogWhite((&g_FileLog), "BuildMultiSpriteTable: Map not found in Material: %s", self->Name);
		return NULL;
	}
	if (CMap_BuildMultiSprite(m)) {
		return m->MultiSprite;
	}
	return NULL;
}

static int CMap_Read(CMap *self, CFileIO *f) {
	char buf[MAX_FILEPATH];

	CFileIO_BeginBlock(f, "MAP");
	int version = CFileIO_ReadInt(f);
	CHECK(version >= 2 && version <= 3);
	CFileIO_ReadString(f, buf, MAX_FILEPATH);
	String_Set(&self->Name, buf);
	CFileIO_Read(f, &self->filter, 1);
	CFileIO_Read(f, &self->lightmap, 1);
	CFileIO_Read(f, &self->ba, 1);
	CFileIO_Read(f, &self->env2, 1);
	CFileIO_Read(f, &self->tile, 1);
	CFileIO_Read(f, &self->mirror, 1);
	CFileIO_Read(f, &self->fx, 1);
	CFileIO_Read(f, self->uvOffset, 8);
	CFileIO_Read(f, self->uvScale, 8);
	CFileIO_Read(f, &self->amount, 4);
	if (version > 2) {
		CFileIO_Read(f, self->uvRot, 0xc);
	}
	return CFileIO_Error(f, "MAP");
}

int CMaterial_Read(CMaterial *self, CFileIO *f) {
	char buf[0x100];

	CFileIO_BeginBlock(f, "MAT");
	int version = CFileIO_ReadInt(f);
	CHECK(version >= 1 && version <= 3);
	if (version > 1) {
		CFileIO_ReadString(f, buf, sizeof(buf));
		String_Set(&self->Name, buf);
	}
	CFileIO_Read(f, self->params.ambient, 4);
	CFileIO_Read(f, self->params.diffuse, 4);
	CFileIO_Read(f, self->params.color, 4);
	CFileIO_Read(f, self->params.specular, 4);
	CFileIO_Read(f, self->params.emissive, 4);
	CFileIO_Read(f, &self->params.power, 4);
	CFileIO_Read(f, &self->params.specLevel, 4);
	CFileIO_Read(f, &self->params.matClass, 0x18);
	for (uint32_t i = 0; i < MAP_TOTAL; i++) {
		CHECK(i < MAP_TOTAL);
		Mat_DeleteMap(&self->map[i]);
		/* version 2 has 3 maps */
		if (version <= 2 && i > 2) {
			break;
		}
		if (CFileIO_ReadInt(f)) {
			CMap *m = Mat_NewMap();
			self->map[i] = m;
			CMap_Read(m, f);
		}
	}
	return CFileIO_Error(f, "MAT");
}

/* SetFileAttributes with exactly attr, if the file has none of its bits */
static void DDS_Hide(const char *name, uint32_t attr) {
	uint32_t a = Plat_FileAttributes(name);

	if (a == 0xffffffff || (attr & a)) {
		return;
	}
	if (!Plat_SetFileAttributes(name, attr)) {
		E3D_LogBlue((&g_FileLog), "DDS: Could not hide: %s", name);
		return;
	}
	E3D_LogWhite((&g_FileLog), "DDS: Hide: %s", name);
}

static void DDS_Unhide(const char *name, uint32_t attr) {
	uint32_t a = Plat_FileAttributes(name);

	if (a == 0xffffffff || !(attr & a)) {
		return;
	}
	E3D_LogBlue((&g_FileLog), "DDS: Hidden file: %s", name);
	if (!Plat_SetFileAttributes(name, ~attr & a)) {
		E3D_LogBlue((&g_FileLog), "DDS: Could not unhide: %s", name);
		return;
	}
	E3D_LogWhite((&g_FileLog), "DDS: Unhide: %s", name);
}

CMaterial *Mat_New(const CMaterial *orig, byte share) {
	if (share) {
		CMaterial *m = CMatList_Find((&g_MatList), orig);
		if (m) {
			return m;
		}
	}
	CMaterial *m = operator_new(sizeof(CMaterial));
	m = m ? CMaterial_ctor(m) : NULL;
	CMaterial_Assign(m, orig);
	CMaterial_Prepare(m, g_Engine->device);
	CMatList_Add((&g_MatList), m);
	return m;
}

CMaterial *Mat_AddCopy(const CMaterial *orig) {
	CMaterial *m = operator_new(sizeof(CMaterial));
	m = m ? CMaterial_ctor(m) : NULL;
	CMaterial_Assign(m, orig);
	CMatList_Add((&g_MatList), m);
	return m;
}

void AnimMat_Free(CAnimMat **slot) {
	if (*slot) {
		CAnimMat *a = *slot;
		CAnimMat_dtor(a);
		operator_delete(a);
		*slot = NULL;
	}
}

void AnimMat_FromMap(CAnimMat **slot, const CMap *map) {
	if (map && map->AnimMat) {
		void *p = operator_new(sizeof(CAnimMat));
		*slot = p ? CAnimMat_ctorCopy(p, map->AnimMat) : NULL;
		CAnimMat_Start(*slot);
	}
}

void Render_SetMaterialAnim(CMaterial *m, CAnimMat *anim) {
	if (!anim) {
		Render_SetMaterial(m, 0);
		return;
	}
	CMap *map = m->map[0];
	void *old = map->AnimMat;
	map->AnimMat = anim;
	Render_SetMaterial(m, 0);
	map->AnimMat = old;
}

int CMaterial_GetId(CMaterial *self, CMatId **out) {
	CMatId_Check(&self->id, self);
	if (out) {
		*out = &self->id;
	}
	return self->id.id;
}

char *CMaterial_GetShaderName(CMaterial *self, char *str, uint32_t nbytes) {
	CHECK(str);
	if (!self->Name || !(self->params.attrib & MAT_SHADER)) {
		return NULL;
	}
	const char *name = strstr(self->Name, "(+shader:");
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

static int CMap_Write(CMap *self, CFileIO *f) {
	char buf[MAX_FILEPATH];

	CFileIO_BeginBlockWrite(f, "MAP");
	CFileIO_WriteInt(f, MAP_FILE_VERSION);
	CHECK(self->Name);
	Path_Part(buf, self->Name, PATH_DIR | PATH_NAME | PATH_EXT);
	CFileIO_WriteString(f, buf);
	CFileIO_Write(f, &self->filter, 1);
	CFileIO_Write(f, &self->lightmap, 1);
	CFileIO_Write(f, &self->ba, 1);
	CFileIO_Write(f, &self->env2, 1);
	CFileIO_Write(f, &self->tile, 1);
	CFileIO_Write(f, &self->mirror, 1);
	CFileIO_Write(f, &self->fx, 1);
	CFileIO_Write(f, self->uvOffset, 8);
	CFileIO_Write(f, self->uvScale, 8);
	CFileIO_Write(f, &self->amount, 4);
	CFileIO_Write(f, self->uvRot, 0xc);
	return CFileIO_EndBlockWrite(f, "MAP");
}

int CMaterial_Write(CMaterial *self, CFileIO *f) {
	CFileIO_BeginBlockWrite(f, "MAT");
	CFileIO_WriteInt(f, MAT_FILE_VERSION);
	CFileIO_WriteString(f, self->Name);
	CFileIO_Write(f, self->params.ambient, 4);
	CFileIO_Write(f, self->params.diffuse, 4);
	CFileIO_Write(f, self->params.color, 4);
	CFileIO_Write(f, self->params.specular, 4);
	CFileIO_Write(f, self->params.emissive, 4);
	CFileIO_Write(f, &self->params.power, 4);
	CFileIO_Write(f, &self->params.specLevel, 4);
	CFileIO_Write(f, &self->params.matClass, 0x18);
	for (int i = 0; i < MAP_TOTAL; i++) {
		if (self->map[i]) {
			CFileIO_WriteInt(f, 1);
			CMap_Write(self->map[i], f);
		} else {
			CFileIO_WriteInt(f, 0);
		}
	}
	return CFileIO_EndBlockWrite(f, "MAT");
}

static byte Mat_CompareNames(const CMaterial *a, const CMaterial *b) {
	CHECK((a->params.attrib & MAT_NAMED) == (b->params.attrib & MAT_NAMED));
	if ((a->params.attrib & MAT_NAMED) || (b->params.attrib & MAT_NAMED)) {
		CHECK(a->Name && b->Name);
		if (strcmp(a->Name, b->Name)) {
			E3D_LogInfo((&g_FileLog), "MatCompareNames: [%s] [%s]", a->Name, b->Name);
			return 0;
		}
	}
	return 1;
}

void CMatList_Add(CMatList *self, CMaterial *mat) {
	CHECK(mat);
	CHECK(!mat->prev && !mat->next);
	if (!self->last) {
		self->last = self->first;
	}
	if (self->last) {
		while (self->last->next) {
			self->last = self->last->next;
		}
		self->last->next = mat;
		mat->prev = self->last;
		self->last = mat;
		mat->Refs++;
		return;
	}
	self->first = mat;
	self->last = mat;
	mat->Refs++;
}

static char g_TextureExts[10][MAX_FILEPATH];

void Tex_AddExtension(const char *ext) {
	char tmp[0x114];
	int i;

	if (!ext || !*ext) {
		return;
	}
	for (i = 0; i < MAX_TEXTURE_EXTS; i++) {
		if (!g_TextureExts[i][0]) {
			break;
		}
	}
	CHECK(i < MAX_TEXTURE_EXTS);
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
	CHECK(j + 1 <= MAX_FILEPATH);
	char *d = g_TextureExts[i];
	if (prefix) {
		Bprintf(d, MAX_FILEPATH, "&u&d&n.%s", tmp);
	} else {
		String_Copy(d, tmp, MAX_FILEPATH);
	}
	E3D_LogDebug((&g_FileLog), "DBG: ExtensTexturas%d: '%s'", i + 1, d);
}

static char g_TexturePaths[10][MAX_FILEPATH];

void Tex_AddPath(const char *path) {
	int i;

	if (!path || !*path) {
		return;
	}
	for (i = 0; i < MAX_TEXTURE_PATHS; i++) {
		if (!g_TexturePaths[i][0]) {
			break;
		}
	}
	CHECK(i < MAX_TEXTURE_PATHS);
	Path_Copy(g_TexturePaths[i], path, MAX_FILEPATH);
	E3D_LogDebug((&g_FileLog), "DBG: RutaTextura%d: '%s'", i + 1, g_TexturePaths[i]);
}

void Tex_ClearExtensions(void) {
	for (int i = 0; i < MAX_TEXTURE_EXTS; i++) {
		g_TextureExts[i][0] = 0;
	}
}

void Tex_ClearPaths(void) {
	for (int i = 0; i < MAX_TEXTURE_PATHS; i++) {
		g_TexturePaths[i][0] = 0;
	}
}

static byte CMap_Equal(const CMap *a, const CMap *b);
CMaterial *CMatList_Find(CMatList *self, const CMaterial *mat) {
	CHECK(!self->first || !self->first->prev);
	CHECK(Mat_IsShareable(mat));
	for (CMaterial *m = self->first; m; m = m->next) {
		const CMatParams *a = &m->params, *b = &mat->params;
		/* the four colors up to power as bytes */
		if (!memcmp(a->ambient, b->ambient, 0x10) && a->power == b->power && a->specLevel == b->specLevel
			&& Color_Equal(a->color, b->color) && MatParams_Equal((const uint32_t *)&a->matClass, (const uint32_t *)&b->matClass)) {
			int i;
			for (i = 0; i < MAP_TOTAL; i++) {
				if (!CMap_Equal(m->map[i], mat->map[i])) {
					break;
				}
			}
			if (i == MAP_TOTAL && Mat_CompareNames(m, mat)) {
				CHECK(Mat_IsShareable(m));
				return m;
			}
		}
		if (m->sub) {
			CMaterial *r = CMatList_Find(m->sub, mat);
			if (r) {
				return r;
			}
		}
	}
	return NULL;
}

static byte Tex_TryFormat(const char *frmt, const char *name, char *out, int alpha) {
	char tmp[MAX_FILEPATH];

	Path_Format(tmp, name, frmt);
	if (alpha) {
		Path_Format(tmp, tmp, "&U&D&N.alpha&E");
	}
	if (File_Exists(tmp)) {
		String_Copy(out, tmp, MAX_FILEPATH);
		return 1;
	}
	return 0;
}

/* 0 with the file in out, -1 */
static int Tex_FindDDS(const char *name, char *out, int alpha) {
	char ext[MAX_FILEPATH];

	Path_SplitDrive(name, NULL, NULL, NULL, ext);
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
	for (int i = 0; i < MAX_TEXTURE_EXTS && g_TextureExts[i][0]; i++) {
		if (Tex_TryFormat(g_TextureExts[i], name, out, alpha)) {
			return 0;
		}
	}
	return -1;
}

static int Tex_FindInPaths(const char *name, char *out) {
	char file[MAX_FILEPATH];
	char tmp[MAX_FILEPATH];

	if (!g_TexturePaths[0][0]) {
		return -1;
	}
	Path_Part(file, name, PATH_NAME | PATH_EXT);
	for (int i = 0; i < MAX_TEXTURE_PATHS && g_TexturePaths[i][0]; i++) {
		Path_Merge(tmp, g_TexturePaths[i], file);
		if (File_Exists(tmp)) {
			String_Copy(out, tmp, MAX_FILEPATH);
			return 0;
		}
	}
	return -1;
}

static void Mat_LogOlder(const char *dds, const char *orig) {
	if (File_TimeDiff(dds, orig, 0) < 0) {
		E3D_LogVerbose((&g_FileLog), "WARN: [%s] is older than [%s]", Path_FileName(dds), Path_FileName(orig));
	}
}

/* the file of the map: a DDS version, the bmp/ directory, the texture paths, numbered frames; path or NULL */
static char *CMap_ResolvePath(CMap *self, byte load, CMap *alpha) {
	char path[MAX_FILEPATH];
	char found[MAX_FILEPATH];
	char drive[4], dir[0x100], name[0x100], ext[0x100];

	CHECK(self->Name);
	self->state = 0;
	String_Set(&self->path, NULL);
	self->animated = 0;
	self->frames = 0;
	Path_Copy(path, self->Name, MAX_FILEPATH);
	/* the alpha map as a flag: the DDS with both */
	if (load && !Tex_FindDDS(path, found, alpha != NULL)) {
		Mat_LogOlder(found, path);
		if (alpha && alpha->path) {
			Mat_LogOlder(found, alpha->path);
		}
		if (g_DDSMarkSource) {
			DDS_Hide(path, FILE_HIDDEN);
			if (alpha && alpha->path) {
				DDS_Hide(alpha->path, FILE_HIDDEN);
			}
		}
		memcpy(path, found, strlen(found) + 1);
	} else if (!File_Exists(path)) {
		Path_SplitDrive(path, drive, dir, name, ext);
		Bprintf(path, MAX_FILEPATH, "%s%sbmp/%s%s", drive, dir, name, ext);
		if (!File_Exists(path) && Tex_FindInPaths(self->Name, path)) {
			/* numbered frames from 0000 */
			Bprintf(path, MAX_FILEPATH, "%s%s%s%s%s", drive, dir, name, "%04d", ext);
			Bprintf(found, MAX_FILEPATH, path, 0);
			if (!File_Exists(found)) {
				Bprintf(path, MAX_FILEPATH, "%s%sbmp/%s%s%s", drive, dir, name, "%04d", ext);
				Bprintf(found, MAX_FILEPATH, path, 0);
				if (!File_Exists(found)) {
					if (load) {
						Con_Warnf("WARN: Texture not found: '%s'\n", self->Name);
					}
					self->state = MAP_MISSING;
					return NULL;
				}
			}
			String_Set(&self->pathAni, path);
			memcpy(path, found, strlen(found) + 1);
			self->animated = 1;
		}
	}
	if (!self->animated) {
		/* name.000.ext: frames name.%03d.ext */
		char *p = strstr(Path_FileName(path), ".000.");
		if (p) {
			Bprintf(found, MAX_FILEPATH, "%.*s.%s.%s", (int)(p - path), path, "%03d", p + 5);
			String_Set(&self->pathAni, found);
			self->animated = 1;
		}
	}
	String_Set(&self->path, path);
	if (g_DDSCheckMark && load) {
		DDS_Unhide(path, FILE_HIDDEN);
	}
	return self->path;
}

static int g_AnimCycleDefault;
static float g_AnimFpsDefault = 25.0f;

static char *CMap_FramePath(CMap *self, int frame, char *buf, int size);
static void CMap_Release(CMap *self);
static void CMap_LoadTexture(CMap *self, void *dev, CMap *alpha, int kind) {
	char buf[MAX_FILEPATH];
	char abuf[MAX_FILEPATH];
	char ini_name[MAX_FILEPATH];
	int info[5];
	CIniFile ini;

	if (self->staticTex.tex || self->state == MAP_MISSING) {
		return;
	}
	const char *alphaPath = NULL;
	if (self->staticTex.Hint) {
		self->staticTex.tex = Texture_GetD3D(self->staticTex.Hint, dev);
		self->cur = &self->staticTex;
		if (!self->staticTex.tex || (!self->animated && !self->fx)) {
			return;
		}
	}
	CMap_Release(self);
	if (alpha && alpha->state != MAP_MISSING) {
		CMap_ResolvePath(alpha, 0, NULL);
		alphaPath = alpha->path;
	}
	if (!CMap_ResolvePath(self, 1, alpha)) {
		return;
	}
	byte lm = self->lightmap;
	if (lm) {
		if (!dev) {
			return;
		}
		if ((int)lm > g_MaxLightmap || (lm > 1 && !g_LightmapsHigh)) {
			Con_Verbosef("INFO: Skipping lightmap texture: '%s'\n", self->Name);
			self->state = MAP_MISSING;
			return;
		}
	}
	int flags, flagsAlt;
	if (self->filter && !self->fx) {
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
		CMap_FramePath(self, i, buf, MAX_FILEPATH);
		while (File_Exists(buf)) {
			i++;
			CMap_FramePath(self, i, buf, MAX_FILEPATH);
		}
		self->frames = i;
		if (i < 2) {
			E3D_LogVerbose((&g_FileLog), "WARN: Animated texture with %d frame: %s", i, self->path);
		}
		CHECK(!self->TextAni);
		CHECK(!self->AnimMat);
		self->TextAni = New_Objects(self->frames, sizeof(CTextStatic), (void *)CTextStatic_ctor, (void *)CTextStatic_dtor);
		if (self->frames > 0) {
			char *aname = alphaPath ? abuf : NULL;
			for (i = 0; i < self->frames; i++) {
				CMap_FramePath(self, i, buf, MAX_FILEPATH);
				if (alphaPath) {
					CMap_FramePath(alpha, i, abuf, MAX_FILEPATH);
				}
				Texture_Add(buf, aname, kind, flags);
				self->TextAni[i].Hint = Texture_Find(buf, aname);
				self->TextAni[i].tex = Texture_GetD3D(self->TextAni[i].Hint, dev);
			}
		}
		/* name.txa: [anim] fps, cycle */
		CIniFile_ctor(&ini);
		int cycle = g_AnimCycleDefault;
		float fps = g_AnimFpsDefault;
		Path_Copy(ini_name, self->Name, MAX_FILEPATH);
		CIniFile_Load(&ini, ini_name, ".txa", 0);
		if (CIniFile_FindSection(&ini, "anim", 1)) {
			CIniFile_ReadFloat(&ini, "anim", "fps", &fps);
			CIniFile_ReadInt(&ini, "anim", "ciclo", &cycle);
		}
		CAnimMat *am = operator_new(sizeof(CAnimMat));
		am = am ? CAnimMat_ctor(am) : NULL;
		self->AnimMat = am;
		am->fps = fps;
		((CAnimMat *)self->AnimMat)->frames = self->frames;
		((CAnimMat *)self->AnimMat)->cycle = cycle;
		CAnimMat_Start(self->AnimMat);
		CIniFile_dtor(&ini);
	}
	Texture_Add(self->path, alphaPath, kind, flags);
	self->staticTex.Hint = Texture_Find(self->path, alphaPath);
	self->staticTex.tex = Texture_GetD3D(self->staticTex.Hint, dev);
	self->cur = &self->staticTex;
	if (self->staticTex.tex && self->fx) {
		CHECK(!self->FXTexture);
		self->FXTexture = FXTexture_Create(self->fx, self);
	}
	if (!Texture_GetInfo(self->staticTex.Hint, info)) {
		self->hasAlpha = info[3] != 0;
		self->compressed = info[4] != 0;
	}
	if (alpha && alpha->state == MAP_MISSING && !self->hasAlpha) {
		Con_Warnf("WARN: Opacity map not found: '%s'\n", alpha->Name);
	}
}

void CMaterial_Prepare(CMaterial *self, void *dev) {
	CMaterial_CalcColors(self, self->colors);
	if (self->sub) {
		CMatList_Prepare(self->sub, dev);
	}
	if (g_LoadTextures != 2 || (self->params.attrib & MAT_LOADTEX)) {
		if (self->map[0]) {
			CMap_LoadTexture(self->map[0], dev, self->map[1], 0);
		}
		if (self->map[2] && self->params.reflection) {
			CMap_LoadTexture(self->map[2], dev, NULL, 1);
		}
		if (self->map[3]) {
			CMap_LoadTexture(self->map[3], dev, NULL, 0);
		}
		if (self->map[4]) {
			CMap_LoadTexture(self->map[4], dev, NULL, 0);
		}
	}
	if (self->id.id == -1) {
		CMatId_Register(&self->id, self);
	}
}

void CMatList_Prepare(CMatList *self, void *dev) {
	CHECK(!self->first || !self->first->prev);
	for (CMaterial *m = self->first; m; m = m->next) {
		CMaterial_Prepare(m, dev);
	}
}

static byte CMap_Equal(const CMap *a, const CMap *b) {
	if (a == b) {
		return 1;
	}
	if (!a || !b) {
		return 0;
	}
	if (strcmp(a->Name, b->Name) || a->filter != b->filter || a->lightmap != b->lightmap || a->ba != b->ba || a->env2 != b->env2
		|| a->tile != b->tile || a->mirror != b->mirror || a->fx != b->fx) {
		return 0;
	}
	if ((byte)Vec2_Differ(a->uvOffset, b->uvOffset) || (byte)Vec2_Differ(a->uvScale, b->uvScale) || (byte)Vec3_Differ(a->uvRot, b->uvRot)) {
		return 0;
	}
	return a->amount == b->amount;
}

int CMatList_Count(CMatList *self, byte sub) {
	int n = 0;

	CHECK(!self->first || !self->first->prev);
	for (CMaterial *m = self->first; m; m = m->next) {
		if (sub && m->sub) {
			n += CMatList_Count(m->sub, 0);
		}
		n++;
	}
	return n;
}

CMap *CMaterial_SetMap(CMaterial *self, uint32_t mapid, const char *name) {
	CHECK(mapid < MAP_TOTAL);
	Mat_DeleteMap(&self->map[mapid]);
	if (name) {
		CMap *m = Mat_NewMap();
		self->map[mapid] = m;
		String_Set(&m->Name, name);
	}
	return self->map[mapid];
}

CMap *CMaterial_SetMapCopy(CMaterial *self, uint32_t mapid, const CMap *orig) {
	return Mat_SetMapCopy(self, mapid, orig);
}

CMaterial *CMatList_Extract(CMatList *self, CMaterial *mat, byte sub) {
	CMaterial *m;

	CHECK(mat);
	for (m = self->first; m; m = m->next) {
		if (m == mat) {
			break;
		}
		if (m->sub && sub) {
			CMaterial *r = CMatList_Extract(m->sub, mat, 1);
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
	if (self->first == m) {
		self->first = m->next;
	}
	if (self->last == m) {
		self->last = m->prev;
	}
	if (m->prev) {
		m->prev->next = m->next;
	}
	if (m->next) {
		m->next->prev = m->prev;
	}
	m->prev = NULL;
	m->next = NULL;
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
	if (a->params.reflection != b->params.reflection) {
		return a->params.reflection ? 1 : -1;
	}
	if (!a->map[3] != !b->map[3]) {
		return a->map[3] ? 1 : -1;
	}
	if (!a->map[4] != !b->map[4]) {
		return a->map[4] ? 1 : -1;
	}
	const char *na = a->map[0] ? a->map[0]->Name : NULL;
	const char *nb = b->map[0] ? b->map[0]->Name : NULL;
	int r;
	if (na && nb) {
		r = Mat_StrSign(na, nb);
	} else {
		r = (nb == NULL) - (na == NULL);
	}
	return r ? r : (int)((uintptr_t)a - (uintptr_t)b);
}

void CMaterial_CalcColors(CMaterial *self, float *out) {
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
	if (!self->map[0]) {
		out[0] = ((float)p->diffuse[0] / 255.0f) * out[0];
		out[1] = ((float)p->diffuse[1] / 255.0f) * out[1];
		out[2] = ((float)p->diffuse[2] / 255.0f) * out[2];
		if (p->attrib & MAT_MAPCOLOR) {
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

static void CMap_Release(CMap *self) {
	self->cur = NULL;
	CTextStatic_dtor(&self->staticTex);
	self->state = 0;
	self->animated = 0;
	self->frames = 0;
	if (self->TextAni) {
		Delete_Objects(self->TextAni, sizeof(CTextStatic), (void *)CTextStatic_dtor);
		self->TextAni = NULL;
	}
	if (self->AnimMat) {
		CAnimMat_dtor(self->AnimMat);
		operator_delete(self->AnimMat);
		self->AnimMat = NULL;
	}
	if (self->FXTexture) {
		FXTexture_Free(self->FXTexture);
		self->FXTexture = NULL;
	}
}

static void CMaterial_Release(CMaterial *self) {
	if (self->sub) {
		CMatList_Release(self->sub);
	}
	for (int i = 0; i < MAP_TOTAL; i++) {
		if (self->map[i]) {
			CMap_Release(self->map[i]);
		}
	}
}

void CMatList_Release(CMatList *self) {
	CHECK(!self->first || !self->first->prev);
	for (CMaterial *m = self->first; m; m = m->next) {
		CMaterial_Release(m);
	}
}

void CMatList_DeleteAll(CMatList *self) {
	int n = 0;

	CHECK(!self->first || !self->first->prev);
	while (self->first) {
		CMaterial *next = self->first->next;
		self->first->Refs--;
		if (self->first) {
			CMaterial_Free(self->first);
		}
		n++;
		self->first = next;
	}
	E3D_LogDebug2((&g_FileLog), "ListaMat: %d materials deleted", n);
	self->last = NULL;
	self->first = NULL;
}

void CMaterial_MapToColor(CMaterial *self) {
	CMap *m = self->map[0];

	if (!m) {
		return;
	}
	self->params.attrib |= MAT_MAPCOLOR;
	CMap_ResolvePath(m, 0, NULL);
	if (m->path) {
		CBmp img = { 0, 0, NULL };
		CBmp avg = { 1, 1, NULL };
		avg.img = New_Array(1, 4);
		CBmp_Load(&img, m->path, 0);
		CBmp_Resample(&avg, &img, 0xf);
		uint32_t c = *CBmp_Pixel(&avg, 0, 0);
		self->params.diffuse[0] = c;
		self->params.diffuse[2] = c >> 16;
		self->params.diffuse[1] = c >> 8;
		avg.width = 0;
		avg.height = 0;
		if (avg.img) {
			operator_delete(avg.img);
			avg.img = NULL;
		}
		img.width = 0;
		img.height = 0;
		if (img.img) {
			operator_delete(img.img);
		}
	}
	Mat_DeleteMap(&self->map[0]);
}

static byte CMap_GetSize(CMap *self, int *w, int *h) {
	int info[5];

	if (self->cur && !Texture_GetInfo(self->cur->Hint, info)) {
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

void *CMap_GetTexture(CMap *self, byte load) {
	CTextStatic *e = self->cur;

	if (!e) {
		return NULL;
	}
	if (e->tex || !e->Hint) {
		return e->tex;
	}
	if (!load) {
		return NULL;
	}
	return Texture_Load(e->Hint, g_pd3dDevice);
}

char *CMaterial_GetMapName(CMaterial *self, uint32_t mapid) {
	CHECK(mapid < MAP_TOTAL);
	return self->map[mapid] ? self->map[mapid]->Name : NULL;
}

byte CMaterial_GetMapSize(CMaterial *self, uint32_t mapid, int *w, int *h) {
	CHECK(mapid < MAP_TOTAL);
	return self->map[mapid] ? CMap_GetSize(self->map[mapid], w, h) : 0;
}

static char *CMap_FramePath(CMap *self, int frame, char *buf, int size) {
	if (self->animated) {
		CHECK(self->pathAni);
		Bprintf(buf, size, self->pathAni, frame);
		return buf;
	}
	CHECK(self->path);
	String_Copy(buf, self->path, size);
	return buf;
}

void CMatList_Delete(CMatList *self, CMaterial *mat) {
	g_TexDeleteUnused = 1;
	CHECK(!Mat_IsShareable(mat));
	CMaterial *m = CMatList_Extract(self, mat, 0);
	CHECK(m);
	CMaterial_Free(mat);
	g_TexDeleteUnused = 0;
}

/* ---- the constructors and destructors of material.h and objref.h (..) ---- */

/* the constructors and destructors of material.h */
static void CMatList_dtor(CMatList *self);
void CMatList_Free(CMatList *self) {
	CMatList_dtor(self);
	operator_delete(self);
}

static CMap *CMap_ctor(CMap *self) {
	self->Name = NULL;
	self->path = NULL;
	self->staticTex.tex = NULL;
	self->staticTex.Hint = NULL;
	self->pathAni = NULL;
	self->uvOffset[0] = 0.0f;
	self->uvOffset[1] = 0.0f;
	self->uvScale[0] = 1.0f;
	self->uvScale[1] = 1.0f;
	self->uvRot[0] = 0.0f;
	self->uvRot[1] = 0.0f;
	self->uvRot[2] = 0.0f;
	self->filter = 1;
	self->lightmap = 0;
	self->ba = 0;
	self->env2 = 0;
	self->tile = 3;
	self->mirror = 0;
	self->fx = 0;
	self->amount = 1.0f;
	self->cur = NULL;
	self->animated = 0;
	self->frames = 0;
	self->TextAni = NULL;
	self->AnimMat = NULL;
	self->FXTexture = NULL;
	self->hasAlpha = 0;
	self->compressed = 0;
	self->state = 0;
	self->MultiSprite = NULL;
	return self;
}

CMaterial *CMaterial_ctor(CMaterial *self) {
	CMatParams *p = &self->params;
	int i;

	self->Name = NULL;
	self->Refs = 0;
	CMatId_ctor(&self->id);
	p->ambient[0] = p->ambient[1] = p->ambient[2] = 0xff;
	p->ambient[3] = 0;
	p->diffuse[0] = p->diffuse[1] = p->diffuse[2] = 0xff;
	p->diffuse[3] = 0;
	p->color[0] = p->color[1] = p->color[2] = 0xff;
	p->color[3] = 0;
	memset(p->specular, 0, sizeof(p->specular));
	self->next = NULL;
	self->prev = NULL;
	self->sub = NULL;
	memset(p->emissive, 0, sizeof(p->emissive));
	p->power = 0.0f;
	p->specLevel = 0.0f;
	p->matClass = 0;
	p->srcBlend = 5;
	p->dstBlend = 6;
	p->twoSided = 0;
	p->lighting = 1;
	p->transp = 0;
	p->reflection = 0;
	p->attrib = 0;
	p->fog = 0;
	p->zWrite = 1;
	p->zFunc = 4;
	for (i = 0; i < MAP_TOTAL; i++) {
		self->map[i] = NULL;
	}
	CMaterial_CalcColors(self, self->colors);
	return self;
}

/* a CBmp of the stack freed (the pixels with the sized delete of one pixel) */
static void CMatList_dtor(CMatList *self) {
	CMatList_DeleteAll(self);
	if (self->Name) {
		String_Set(&self->Name, NULL);
	}
}

/* the atexit destructor of (&g_MatList) (the destructor inlined) */
static void Material_FiniMatList(void) {
	CMatList_dtor((&g_MatList));
}

static void CMap_dtor(CMap *self) {
	CMap_Release(self);
	if (self->pathAni) {
		String_Set(&self->pathAni, NULL);
	}
	CTextStatic_dtor(&self->staticTex);
	if (self->path) {
		String_Set(&self->path, NULL);
	}
	if (self->Name) {
		String_Set(&self->Name, NULL);
	}
}

void CMaterial_dtor(CMaterial *self) {
	int i;

	if (self->prev) {
		self->prev->next = self->next;
	}
	if (self->next) {
		self->next->prev = self->prev;
	}
	for (i = 0; i < MAP_TOTAL; i++) {
		Mat_DeleteMap(&self->map[i]);
	}
	if (self->sub) {
		CMatList_Free(self->sub);
		self->sub = NULL;
	}
	CMatId_dtor(&self->id);
	CObjRef_dtor((CObjRef *)&self->Refs);
	if (self->Name) {
		String_Set(&self->Name, NULL);
	}
}

void String_dtor(char **self) {
	if (*self) {
		String_Set(self, NULL);
	}
}

void CMaterial_ScaleColors(CMaterial *self, float k) {
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

/* only managed textures in R5G6B5, A8R8G8B8 or X8R8G8B8; the 32 bit formats get an alpha of 0 */
byte CMap_CopyBmp(CMap *self, int dx, int dy, const CImage *bmp, int sx, int sy, int w, int h) {
	void *tex = self->staticTex.tex;
	d3dSurfaceDesc_t desc;
	d3dLockedRect_t lr;
	const byte *src;
	int x, y;

	if (!tex || !bmp || !bmp->img || sx < 0 || sy < 0 || dx < 0 || dy < 0) {
		return 0;
	}
	if (Rd_TexGetLevelDesc(tex, 0, &desc) < 0 || desc.Pool != D3DPOOL_MANAGED) {
		return 0;
	}
	/* D3DFMT_R5G6B5, D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8 */
	if (desc.Format != 0x17 && desc.Format != 0x15 && desc.Format != 0x16) {
		return 0;
	}
	if (w >= bmp->width - sx) {
		w = bmp->width - sx;
	}
	if (h >= bmp->height - sy) {
		h = bmp->height - sy;
	}
	if (w >= (int)desc.Width - dx) {
		w = (int)desc.Width - dx;
	}
	if (h >= (int)desc.Height - dy) {
		h = (int)desc.Height - dy;
	}
	if (w <= 0 || h <= 0 || Rd_TexLockRect(tex, 0, &lr, NULL, 0) < 0) {
		return 0;
	}
	if (desc.Format == 0x17) {
		for (y = 0; y < h; y++) {
			uint16_t *dst = (uint16_t *)((byte *)lr.pBits + (dy + y) * lr.Pitch + dx * 2);

			src = (const byte *)CBmp_Pixel(bmp, (uint32_t)sx, (uint32_t)(y + sy));
			for (x = 0; x < w; x++, src += 4) {
				dst[x] = (uint16_t)(((src[0] & 0xf8) << 8) | ((src[1] & 0xfc) << 3) | (src[2] >> 3));
			}
		}
	} else {
		for (y = 0; y < h; y++) {
			uint32_t *dst = (uint32_t *)((byte *)lr.pBits + (dy + y) * lr.Pitch + dx * 4);

			src = (const byte *)CBmp_Pixel(bmp, (uint32_t)sx, (uint32_t)(y + sy));
			for (x = 0; x < w; x++, src += 4) {
				dst[x] = ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
			}
		}
	}
	Rd_TexUnlockRect(tex, 0);
	return 1;
}

/* the vtable of CMatList */

/* the vtable of CMaterial */

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_ATEXIT(Init_MatList, Material_FiniMatList)
INIT_GVAR_INT(Init_GVar_DDS_UseTextures, &GVar_DDS_UseTextures, "DDS_UseTextures", 0, "Read DDS Textures (0/1)", 1)
INIT_GVAR_INT(Init_GVar_DDS_MarkSource, &GVar_DDS_MarkSource, "DDS_MarkSource", 0, "Mark original texture (0/1)", 0)
INIT_GVAR_INT(Init_GVar_DDS_CheckMark, &GVar_DDS_CheckMark, "DDS_CheckMark", 0, "Check that used textures are not marked (0/1)", 0)
INIT_GVAR_FLOAT(Init_GVar_MatEmissive, &GVar_MatEmissive, "MatEmissive", 0, "Extra amount of emissive lighting", 0.0f)
INIT_GVAR_INT(Init_GVar_LoadTextures, &GVar_LoadTextures, "LoadTextures", 0, "Load textures (except Sprite2D ones) (0/1)", 1)
