/*
 * Materials of the engine3d (original sources Engine/Material.cpp, engine3d/engine/Material.h): CMaterial with its
 * parameters and up to MAP_TOTAL texture maps (CMap), kept in lists (CMatList) that can nest (a material may own
 * a list of sub materials). The textures themselves belong to the texture manager of the engine (CTexture,
 * decompiled), the maps hold references to them.
 */
#ifndef SCRAP_MATERIAL_H
#define SCRAP_MATERIAL_H

#include "scrap/archio.h"

#define MAP_TOTAL 5
/* the maps of a material */
enum {
	MAP_DIFFUSE = 0,
	MAP_OPACITY = 1,
	MAP_ENVIRONMENT = 2,		/* the reflection */
	MAP_BUMP = 3,
	MAP_GLOW = 4,
};
#define MAX_TEXTURE_PATHS 10
#define MAX_TEXTURE_EXTS 10

#define CMATLIST_VFTABLE GAME_VFTABLE(CMatList_vftable, 0x3d6d48)
#define CMATERIAL_VFTABLE GAME_VFTABLE(CMaterial_vftable, 0x3d6dbc)

/* params.attrib */
#define MAT_NOCOLLIDE 0x1		/* "(nc)", "(+no_collide)": the triangles get TRI_ALT_NO_COLLISIONS */
#define MAT_LOADTEX 0x40		/* textures loaded also with LoadTextures 2 */
#define MAT_MAPCOLOR 0x80		/* the diffuse color comes from the map (CMaterial_MapToColor) */
#define MAT_SHADER 0x200		/* the name holds "(+shader: name)" */
#define MAT_NAMED 0x300			/* materials only shared with the same name */
#define MAT_NOSHARE 0x8
#define MAT_SHADOW 0x20			/* casts shadows in the light maps also when transparent (bsp2.cpp) */
#define MAT_AREALIGHT 0x100		/* the name holds "(+area_light: name)" of a radiosity light (radiosidad.cpp) */
#define MAT_1000 0x1000		/* transparent, but the alpha constant of the pixel shaders stays */

/* a texture of the texture manager: the reference (hint) and its device texture */
typedef struct CTextStatic {
	void *tex;			/* 0x0 Texture_GetD3D */
	void *Hint;			/* 0x4 CTexture, referenced */
} CTextStatic;
SD_SIZE(CTextStatic, 0x8);

typedef struct CMap {
	char *Name;			/* 0x00 as in the model */
	char *path;			/* 0x04 the file found (CMap_ResolvePath) */
	byte filter;			/* 0x08 0 for "Filter: no" */
	byte lightmap;			/* 0x09 lightmap level, 0 for none */
	byte ba;			/* 0x0a */
	byte env2;			/* 0x0b "(env2)" in the name of the reflection map */
	byte tile;			/* 0x0c "TileU" (bit 0), "TileV" (bit 1) */
	byte mirror;			/* 0x0d "EspejoU" (bit 0), "EspejoV" (bit 1) */
	byte fx;			/* 0x0e kind of FXTexture, 0 for none */
	byte pad_f;
	float uvOffset[2];		/* 0x10 "DesplazU", "DesplazV" */
	float uvScale[2];		/* 0x18 "EscalaU", "EscalaV" */
	float uvRot[3];			/* 0x20 the "Angulo" in z */
	float amount;			/* 0x2c of the map in the material */
	CTextStatic *cur;			/* 0x30 est or a frame of TextAni */
	CTextStatic staticTex;			/* 0x34 */
	byte animated;			/* 0x3c pathAni is set */
	byte pad_3d[3];
	int frames;			/* 0x40 */
	char *pathAni;			/* 0x44 printf format of the frame files */
	CTextStatic *TextAni;		/* 0x48 new[] of frames */
	void *AnimMat;			/* 0x4c 0x2c bytes */
	void *FXTexture;		/* 0x50 object with a virtual deleting destructor at [0] */
	byte hasAlpha;			/* 0x54 */
	byte compressed;		/* 0x55 the texture is compressed */
	byte pad_56[2];
	int state;			/* 0x58 MAP_MISSING: the file was not found */
	void *MultiSprite;		/* 0x5c */
} CMap;
SD_SIZE(CMap, 0x60);
SD_OFFSET(CMap, staticTex, 0x34);
SD_OFFSET(CMap, state, 0x58);

#define MAP_MISSING 2

/* the colors are bytes R G B A; diffuse, ambient, specular and emissive of colors[] are D3DMATERIAL9 order */
typedef struct CMatParams {
	byte ambient[4];		/* 0x10 */
	byte diffuse[4];		/* 0x14 [3] the alpha */
	byte specular[4];		/* 0x18 */
	byte emissive[4];		/* 0x1c */
	float power;			/* 0x20 */
	float specLevel;		/* 0x24 */
	byte color[4];			/* 0x28 */
	int matClass;			/* 0x2c 0 "Standard", 1 "Multi/Sub-Object" */
	int srcBlend;			/* 0x30 5 */
	int dstBlend;			/* 0x34 6 */
	byte twoSided;			/* 0x38 "TwoSided" */
	byte lighting;			/* 0x39 D3DRS_LIGHTING with the material colors */
	byte transp;			/* 0x3a */
	byte reflection;		/* 0x3b a reflection map is set */
	uint16_t attrib;			/* 0x3c MAT_* */
	byte fog;			/* 0x3e D3DRS_FOGENABLE when the fog is on */
	byte zWrite;			/* 0x3f D3DRS_ZWRITEENABLE */
	int zFunc;			/* 0x40 D3DRS_ZFUNC: 4 less or equal, 8 always */
} CMatParams;
SD_SIZE(CMatParams, 0x34);

/* the shader mode of a material for the renderer (Engine/RenderShader.cpp) */
typedef struct CMatId {
	int id;				/* 0x0 the shader mode, -1 until registered */
	byte alphaTest;			/* 0x4 the low byte of the flags of the mode */
	byte blend;			/* 0x5 the next byte */
	byte alpha;			/* 0x6 the diffuse map has an alpha (and the mask flag, see CMatId_Register) */
	byte pad_7;
	int gen;			/* 0x8 the shader mode mask it was registered with */
} CMatId;
SD_SIZE(CMatId, 0xc);

/* the bases without a vtable of the named and counted objects of objref.h: CScene, CNode3D and CMaterial have the
 * name at 0x4 and the count at 0x8, CMatList only the name */
typedef struct CObjName {
	char *name;			/* 0x0 */
} CObjName;
SD_SIZE(CObjName, 4);

typedef struct CObjRef {
	int Refs;			/* 0x0 */
} CObjRef;
SD_SIZE(CObjRef, 4);

typedef struct CMatList CMatList;

typedef struct CMaterial {
	void **vftable;			/* 0x00 [0] the deleting destructor */
	char *Name;			/* 0x04 */
	int Refs;			/* 0x08 the lists holding it */
	CMatList *sub;			/* 0x0c */
	CMatParams params;		/* 0x10 */
	struct CMaterial *next;		/* 0x44 */
	struct CMaterial *prev;		/* 0x48 */
	float colors[17];		/* 0x4c diffuse, ambient, specular, emissive, power */
	CMatId id;			/* 0x90 */
	CMap *map[MAP_TOTAL];	/* 0x9c */
} CMaterial;
SD_SIZE(CMaterial, 0xb0);
SD_OFFSET(CMaterial, next, 0x44);
SD_OFFSET(CMaterial, map, 0x9c);

struct CMatList {
	void **vftable;			/* 0x0 */
	char *Name;			/* 0x4 */
	CMaterial *first;			/* 0x8 */
	CMaterial *last;			/* 0xc */
};
SD_SIZE(CMatList, 0x10);

/* the materials of the engine */
#define g_MatList (GAME_ARRAY(CMatList, g_MatList, 0x431d24))

/* the constructors and destructors of material.h (rva 0x2336f0..0x233c20) */
CMatList *__thiscall CMatList_DeleteObj(CMatList *self, byte flags);
void __fastcall CMatList_dtor(CMatList *self);
/* the string freed if set (the destructor of the string members) */
void __fastcall String_dtor(char **self);
CMatId *__fastcall CMatId_ctor(CMatId *self);		/* rva 0x2a79e0 */
void __fastcall CMatId_dtor(CMatId *self);		/* rva 0x2a79f0 */
CMap *__thiscall CMap_ctor(CMap *self);
void __thiscall CMap_dtor(CMap *self);
CMaterial *__thiscall CMaterial_ctor(CMaterial *self);
/* rva 0x237020: the D3D colors (CMaterial_CalcColors) times k unless k is 1, the alpha of colors from diffuse[3] */
void __thiscall CMaterial_ScaleColors(CMaterial *self, float k);
void __thiscall CMaterial_dtor(CMaterial *self);
/* free the old string, a Mem_Alloc copy of s (NULL stays NULL) */
/* the animation of an animated texture (Engine/ControlAnimMat.cpp, rva 0x257f10..0x258284), 0x2c bytes */
typedef struct CAnimMat {
	float fps;			/* 0x0 */
	int frames;			/* 0x4 */
	int cycle;			/* 0x8 the frame it loops back to, -1: stops at the end */
	float dt;			/* 0xc the seconds of the last step (at most 0.1) */
	int frame;			/* 0x10 */
	float framef;			/* 0x14 */
	int mode;			/* 0x18 ANIMMAT_REPR .. ANIMMAT_END */
	float fadeRate;			/* 0x1c FUND_PAUSA: the slow down per second */
	float fadeK;			/* 0x20 FUND_PAUSA: the speed factor, down to 0.2 */
	uint32_t lastTime;		/* 0x24 g_TimeMs of the last step */
	int lastFrame;			/* 0x28 g_FrameCount of the last step */
} CAnimMat;
SD_SIZE(CAnimMat, 0x2c);
#define ANIMMAT_REPR 0			/* playing */
#define ANIMMAT_PAUSE 1
#define ANIMMAT_FADE_PAUSE 2		/* slowing down */
#define ANIMMAT_END 3
CAnimMat *__thiscall CAnimMat_ctor(CAnimMat *self);
/* rva 0x257f10, a copy of fps, frames and cycle of src */
CAnimMat *__thiscall CAnimMat_ctorCopy(CAnimMat *self, const CAnimMat *src);
void __thiscall CAnimMat_dtor(CAnimMat *self);
/* from frame 0 at the time of now */
void __thiscall CAnimMat_Start(CAnimMat *self);
/* rva 0x258060: the frame of now (updated once each frame) */
int __fastcall CAnimMat_GetFrame(CAnimMat *self);
/* the step to g_TimeMs */
void __fastcall CAnimMat_Update(CAnimMat *self);
void __thiscall CAnimMat_SetFrameF(CAnimMat *self, float frame);
/* only while already FUND_PAUSA: the slow down restarted over ms */
void __thiscall CAnimMat_FadePause(CAnimMat *self, float ms);
/* fps and cycle of a, 0 for a NULL a (al only) */
byte AnimMat_Set(CAnimMat *a, float fps, int cycle);
/* "REPR", "PAUSA", "FUND_PAUSA", "FIN" (strings of the exe), CHECK and NULL for others */
const char *AnimMat_ModeName(int mode);
void __thiscall String_Set(char **self, const char *s);
/* rva 0x2a1970: *self a String_Dup of the printf of fmt, NULL (freed) for fmt NULL */
void String_Printf(char **self, const char *fmt, ...);
/* rva 0x2a18a0 and 0x2a1920: the string as a CFileIO string (empty is NULL) */
void __thiscall String_Read(char **self, CFileIO *f);
void __thiscall String_Write(char **self, CFileIO *f);
/* rva 0x2a1970: a VBprintf (1 KB) copy, NULL for a NULL fmt */
void String_Printf(char **self, const char *fmt, ...);

void __fastcall CObjRef_dtor(CObjRef *self);
CTextStatic *__fastcall CTextStatic_ctor(CTextStatic *self);
void __fastcall CTextStatic_dtor(CTextStatic *self);
CTextStatic *__thiscall CTextStatic_Delete(CTextStatic *self, byte flags);
/* name and parameters, not the textures */
void __thiscall CMap_Assign(CMap *self, const CMap *orig);
byte CMap_Equal(const CMap *a, const CMap *b);
int __thiscall CMap_Read(CMap *self, CFileIO *f);
int __thiscall CMap_Write(CMap *self, CFileIO *f);
byte __fastcall CMap_BuildMultiSprite(CMap *self);
/* the file of the map: a DDS version, the bmp/ directory, the texture paths, numbered frames; path or NULL */
char *__thiscall CMap_ResolvePath(CMap *self, byte load, CMap *alpha);
void __thiscall CMap_LoadTexture(CMap *self, void *dev, CMap *alpha, int flags);
void __fastcall CMap_Release(CMap *self);
byte __thiscall CMap_GetSize(CMap *self, int *w, int *h);
byte __fastcall CMap_HasAlpha(const CMap *self);
void *__thiscall CMap_GetTexture(CMap *self, byte load);
struct CImage;
/* the w x h pixels of bmp at (sx, sy) into the texture at (dx, dy); 0 when not done */
byte __thiscall CMap_CopyBmp(CMap *self, int dx, int dy, const struct CImage *bmp, int sx, int sy, int w, int h);
char *__thiscall CMap_FramePath(CMap *self, int frame, char *buf, int size);
/* FXTexture_Kind (the kind of FXTexture of a map name): fxtexture.h */

CMaterial *__thiscall CMaterial_Delete(CMaterial *self, byte flags);
/* name, parameters and copies of the maps */
void __thiscall CMaterial_Assign(CMaterial *self, const CMaterial *orig);
void *__thiscall CMaterial_GetMultiSprite(CMaterial *self, uint32_t mapid);
int __thiscall CMaterial_Read(CMaterial *self, CFileIO *f);
int __thiscall CMaterial_Write(CMaterial *self, CFileIO *f);
int __thiscall CMaterial_GetId(CMaterial *self, CMatId **out);
/* src/game/engine3d/Engine/RenderShader.c: id -1 */
CMatId *__fastcall CMatId_ctor(CMatId *self);
void __fastcall CMatId_dtor(CMatId *self);
/* 1 when the mode is up to date, else registered again (deferred for an unregistered one) and 0 */
byte __thiscall CMatId_Check(CMatId *self, CMaterial *mat);
/* the mode of the maps and flags of mat, CHECK(id == -1) */
void __thiscall CMatId_Register(CMatId *self, CMaterial *mat);
int __fastcall CMaterial_Id(CMaterial *self);
char *__thiscall CMaterial_GetShaderName(CMaterial *self, char *str, uint32_t nbytes);
void __thiscall CMaterial_Prepare(CMaterial *self, void *dev);
CMap *__thiscall CMaterial_SetMap(CMaterial *self, uint32_t mapid, const char *name);
CMap *__thiscall CMaterial_SetMapCopy(CMaterial *self, uint32_t mapid, const CMap *orig);
/* colors as a D3DMATERIAL9 */
void __thiscall CMaterial_CalcColors(CMaterial *self, float *out);
void __fastcall CMaterial_Release(CMaterial *self);
/* the average color of map 0 as the diffuse color, the map deleted */
void __fastcall CMaterial_MapToColor(CMaterial *self);
char *__thiscall CMaterial_GetMapName(CMaterial *self, uint32_t mapid);
byte __thiscall CMaterial_GetMapSize(CMaterial *self, uint32_t mapid, int *w, int *h);

void __thiscall CMatList_Add(CMatList *self, CMaterial *mat);
/* an equal shareable material, also in the sub lists */
CMaterial *__thiscall CMatList_Find(CMatList *self, const CMaterial *mat);
void __thiscall CMatList_Prepare(CMatList *self, void *dev);
int __thiscall CMatList_Count(CMatList *self, byte sub);
CMaterial *__thiscall CMatList_Extract(CMatList *self, CMaterial *mat, byte sub);
void __fastcall CMatList_Release(CMatList *self);
void __fastcall CMatList_DeleteAll(CMatList *self);
void __thiscall CMatList_Delete(CMatList *self, CMaterial *mat);

byte Color_Equal(const byte *a, const byte *b);
byte __thiscall MatParams_Equal(const uint32_t *self, const uint32_t *other);
int Vec2_Differ(const float *a, const float *b);
/* blend modes between the file values and the engine (legacy: the files of version 1) */
void Mat_ConvertBlend(uint32_t *mode, byte toFile, byte legacy);
/* a copy of orig in g_MatList, prepared; with share an equal material of the list instead */
CMaterial *Mat_New(const CMaterial *orig, byte share);
/* a copy of orig added to g_MatList, not prepared */
CMaterial *Mat_AddCopy(const CMaterial *orig);
/* the own animation of a sprite: freed; a running copy of the one of map (if map has one) */
void AnimMat_Free(CAnimMat **slot);
void AnimMat_FromMap(CAnimMat **slot, const CMap *map);
/* Render_SetMaterial of pass 0 with anim (if set) in place of the animation of the first map */
void Render_SetMaterialAnim(CMaterial *m, CAnimMat *anim);
byte Mat_CompareNames(const CMaterial *a, const CMaterial *b);
const char *Mat_Str(const CMaterial *m);
/* sort order for drawing */
int Mat_Compare(const CMaterial *a, const CMaterial *b);
/* with an opacity: transparent flag or map 1 without b3b (inline in the exe) */
static inline byte Mat_IsTransp(const CMaterial *m) {
	if (m->params.attrib & 0x20) {
		return 0;
	}
	return m->params.transp || (m->map[1] && !m->params.reflection);
}
void DDS_Hide(const char *name, uint32_t attr);
void DDS_Unhide(const char *name, uint32_t attr);
/* an extension: a Path_Format format, "<x>" for &x; without one "&u&d&n." is put before it */
void Tex_AddExtension(const char *ext);
void Tex_AddPath(const char *path);
void Tex_ClearExtensions(void);
void Tex_ClearPaths(void);
/* 0 with the file in out, -1 */
int Tex_FindDDS(const char *name, char *out, int alpha);
int Tex_FindInPaths(const char *name, char *out);
byte Tex_TryFormat(const char *frmt, const char *name, char *out, int alpha);

#endif
