/*
 * Materials of the engine3d (original sources Engine/Material.cpp, engine3d/engine/Material.h): CMaterial with its
 * parameters and up to MAPA_TOTAL texture maps (CMapa), kept in lists (CListaMat) that can nest (a material may own
 * a list of sub materials). The textures themselves belong to the texture manager of the engine (CTextura,
 * decompiled), the maps hold references to them.
 */
#ifndef SCRAP_MATERIAL_H
#define SCRAP_MATERIAL_H

#include "scrap/archio.h"

#define MAPA_TOTAL 5
/* the maps of a material */
enum {
	MAPA_DIFFUSE = 0,
	MAPA_OPACITY = 1,
	MAPA_ENTORNO = 2,		/* the reflection */
	MAPA_BUMP = 3,
	MAPA_GLOW = 4,
};
#define MAX_RUTAS_TEXTURAS 10
#define MAX_EXTENS_TEXTURAS 10

#define CLISTAMAT_VFTABLE 0x3d6d48
#define CMATERIAL_VFTABLE 0x3d6dbc

/* params.atrib */
#define MAT_LOADTEX 0x40		/* textures loaded also with LoadTextures 2 */
#define MAT_MAPCOLOR 0x80		/* the diffuse color comes from the map (CMaterial_MapToColor) */
#define MAT_SHADER 0x200		/* the name holds "(+shader: name)" */
#define MAT_NAMED 0x300			/* materials only shared with the same name */
#define MAT_NOSHARE 0x8
#define MAT_SOMBRA 0x20			/* casts shadows in the light maps also when transparent (bsp2.cpp) */
#define MAT_AREALIGHT 0x100		/* the name holds "(+area_light: name)" of a radiosity light (radiosidad.cpp) */
#define MAT_1000 0x1000		/* transparent, but the alpha constant of the pixel shaders stays */

/* a texture of the texture manager: the reference (hint) and its device texture */
typedef struct CTextEst {
	void *tex;			/* 0x0 Textura_GetD3D */
	void *Hint;			/* 0x4 CTextura, referenced */
} CTextEst;
SD_SIZE(CTextEst, 0x8);

typedef struct CMapa {
	char *Nombre;			/* 0x00 as in the model */
	char *ruta;			/* 0x04 the file found (CMapa_ResolvePath) */
	byte b8;			/* 0x08 */
	byte lightmap;			/* 0x09 lightmap level, 0 for none */
	byte ba;			/* 0x0a */
	byte bb;			/* 0x0b */
	byte bc;			/* 0x0c */
	byte bd;			/* 0x0d */
	byte fx;			/* 0x0e kind of FXTextura, 0 for none */
	byte pad_f;
	float f10[2];			/* 0x10 */
	float f18[2];			/* 0x18 */
	float v20[3];			/* 0x20 */
	float f2c;			/* 0x2c */
	CTextEst *cur;			/* 0x30 est or a frame of TextAni */
	CTextEst est;			/* 0x34 */
	byte animated;			/* 0x3c rutaAni is set */
	byte pad_3d[3];
	int frames;			/* 0x40 */
	char *rutaAni;			/* 0x44 printf format of the frame files */
	CTextEst *TextAni;		/* 0x48 new[] of frames */
	void *AnimMat;			/* 0x4c 0x2c bytes */
	void *FXTextura;		/* 0x50 object with a virtual deleting destructor at [0] */
	byte hasAlpha;			/* 0x54 */
	byte b55;			/* 0x55 */
	byte pad_56[2];
	int state;			/* 0x58 MAPA_MISSING: the file was not found */
	void *MultiSprite;		/* 0x5c */
} CMapa;
SD_SIZE(CMapa, 0x60);
SD_OFFSET(CMapa, est, 0x34);
SD_OFFSET(CMapa, state, 0x58);

#define MAPA_MISSING 2

/* the colors are bytes R G B A; diffuse, ambient, specular and emissive of colors[] are D3DMATERIAL9 order */
typedef struct CMatParams {
	byte ambient[4];		/* 0x10 */
	byte diffuse[4];		/* 0x14 [3] the alpha */
	byte specular[4];		/* 0x18 */
	byte emissive[4];		/* 0x1c */
	float power;			/* 0x20 */
	float specLevel;		/* 0x24 */
	byte color[4];			/* 0x28 */
	int i2c;			/* 0x2c */
	int srcBlend;			/* 0x30 5 */
	int dstBlend;			/* 0x34 6 */
	byte b38;			/* 0x38 */
	byte b39;			/* 0x39 */
	byte transp;			/* 0x3a */
	byte b3b;			/* 0x3b */
	uint16_t atrib;			/* 0x3c MAT_* */
	uint16_t w3e;			/* 0x3e */
	int i40;			/* 0x40 */
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

typedef struct CListaMat CListaMat;

typedef struct CMaterial {
	void **vftable;			/* 0x00 [0] the deleting destructor */
	char *Nombre;			/* 0x04 */
	int Refs;			/* 0x08 the lists holding it */
	CListaMat *sub;			/* 0x0c */
	CMatParams params;		/* 0x10 */
	struct CMaterial *sig;		/* 0x44 */
	struct CMaterial *ant;		/* 0x48 */
	float colors[17];		/* 0x4c diffuse, ambient, specular, emissive, power */
	CMatId id;			/* 0x90 */
	CMapa *mapa[MAPA_TOTAL];	/* 0x9c */
} CMaterial;
SD_SIZE(CMaterial, 0xb0);
SD_OFFSET(CMaterial, sig, 0x44);
SD_OFFSET(CMaterial, mapa, 0x9c);

struct CListaMat {
	void **vftable;			/* 0x0 */
	char *Nombre;			/* 0x4 */
	CMaterial *pri;			/* 0x8 */
	CMaterial *ult;			/* 0xc */
};
SD_SIZE(CListaMat, 0x10);

/* the materials of the engine */
#define g_ListaMat ((CListaMat *)Game_Ptr(0x431d24))

/* the parts of the classes outside Material.cpp (rva 0x233760.., not decompiled) */
CMapa *__thiscall CMapa_ctor(CMapa *self);
void __thiscall CMapa_dtor(CMapa *self);
CMaterial *__thiscall CMaterial_ctor(CMaterial *self);
/* rva 0x237020: the D3D colors (CMaterial_CalcColors) times k unless k is 1, the alpha of colors from diffuse[3] */
void __thiscall CMaterial_ScaleColors(CMaterial *self, float k);
void __thiscall CMaterial_dtor(CMaterial *self);
/* free the old string, a Mem_Alloc copy of s (NULL stays NULL) */
/* the animation of an animated texture (rva 0x257f70.., not decompiled yet), 0x2c bytes */
typedef struct CAnimMat {
	float fps;			/* 0x0 */
	int frames;			/* 0x4 */
	int ciclo;			/* 0x8 */
	byte rest[0x20];
} CAnimMat;
SD_SIZE(CAnimMat, 0x2c);
CAnimMat *__thiscall CAnimMat_ctor(CAnimMat *self);
/* rva 0x257f10, a copy of src */
CAnimMat *__thiscall CAnimMat_ctorCopy(CAnimMat *self, const CAnimMat *src);
void __thiscall CAnimMat_dtor(CAnimMat *self);
void __thiscall CAnimMat_Start(CAnimMat *self);
/* rva 0x258060: the frame of now (updated once each frame) */
int __fastcall CAnimMat_GetFrame(CAnimMat *self);
void __thiscall Cad_Set(char **self, const char *s);
/* rva 0x2a1970: *self a Cad_Dup of the printf of fmt, NULL (freed) for fmt NULL */
void Cad_Printf(char **self, const char *fmt, ...);
/* rva 0x2a18a0 and 0x2a1920: the string as a CArchIO string (empty is NULL) */
void __thiscall Cad_Read(char **self, CArchIO *f);
void __thiscall Cad_Write(char **self, CArchIO *f);
/* rva 0x2a1970: a VBprintf (1 KB) copy, NULL for a NULL fmt */
void Cad_Printf(char **self, const char *fmt, ...);

void __fastcall CObjRef_dtor(int *Refs);
void __fastcall CTextEst_dtor(CTextEst *self);
CTextEst *__thiscall CTextEst_Delete(CTextEst *self, byte flags);
/* name and parameters, not the textures */
void __thiscall CMapa_Assign(CMapa *self, const CMapa *orig);
byte CMapa_Equal(const CMapa *a, const CMapa *b);
int __thiscall CMapa_Read(CMapa *self, CArchIO *f);
int __thiscall CMapa_Write(CMapa *self, CArchIO *f);
byte __fastcall CMapa_BuildMultiSprite(CMapa *self);
/* the file of the map: a DDS version, the bmp/ directory, the texture paths, numbered frames; ruta or NULL */
char *__thiscall CMapa_ResolvePath(CMapa *self, byte load, CMapa *alpha);
void __thiscall CMapa_LoadTexture(CMapa *self, void *dev, CMapa *alpha, int flags);
void __fastcall CMapa_Release(CMapa *self);
byte __thiscall CMapa_GetSize(CMapa *self, int *w, int *h);
void *__thiscall CMapa_GetTexture(CMapa *self, byte load);
struct CImagen;
/* rva 0x2370a0, not in C: the w x h pixels of bmp at (sx, sy) into the texture at (dx, dy); 0 when not done */
byte __thiscall CMapa_CopyBmp(CMapa *self, int dx, int dy, const struct CImagen *bmp, int sx, int sy, int w, int h);
char *__thiscall CMapa_FramePath(CMapa *self, int frame, char *buf, int size);
/* rva 0x2a82f0: the kind of FXTextura of a map named name ("(...)" parts), 0 for none */
byte FXTextura_Tipo(const char *name);

CMaterial *__thiscall CMaterial_Delete(CMaterial *self, byte flags);
/* name, parameters and copies of the maps */
void __thiscall CMaterial_Assign(CMaterial *self, const CMaterial *orig);
void *__thiscall CMaterial_GetMultiSprite(CMaterial *self, uint32_t idmapa);
int __thiscall CMaterial_Read(CMaterial *self, CArchIO *f);
int __thiscall CMaterial_Write(CMaterial *self, CArchIO *f);
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
CMapa *__thiscall CMaterial_SetMap(CMaterial *self, uint32_t idmapa, const char *name);
CMapa *__thiscall CMaterial_SetMapCopy(CMaterial *self, uint32_t idmapa, const CMapa *orig);
/* colors as a D3DMATERIAL9 */
void __thiscall CMaterial_CalcColors(CMaterial *self, float *out);
void __fastcall CMaterial_Release(CMaterial *self);
/* the average color of map 0 as the diffuse color, the map deleted */
void __fastcall CMaterial_MapToColor(CMaterial *self);
char *__thiscall CMaterial_GetMapName(CMaterial *self, uint32_t idmapa);
byte __thiscall CMaterial_GetMapSize(CMaterial *self, uint32_t idmapa, int *w, int *h);

void __thiscall CListaMat_Add(CListaMat *self, CMaterial *mat);
/* an equal shareable material, also in the sub lists */
CMaterial *__thiscall CListaMat_Find(CListaMat *self, const CMaterial *mat);
void __thiscall CListaMat_Prepare(CListaMat *self, void *dev);
int __thiscall CListaMat_Count(CListaMat *self, byte sub);
CMaterial *__thiscall CListaMat_Extract(CListaMat *self, CMaterial *mat, byte sub);
void __fastcall CListaMat_Release(CListaMat *self);
void __fastcall CListaMat_DeleteAll(CListaMat *self);
void __thiscall CListaMat_Delete(CListaMat *self, CMaterial *mat);

byte Color_Equal(const byte *a, const byte *b);
byte __thiscall MatParams_Equal(const uint32_t *self, const uint32_t *other);
int Vec2_Differ(const float *a, const float *b);
/* blend modes between the file values and the engine (legacy: the files of version 1) */
void Mat_ConvertBlend(uint32_t *mode, byte toFile, byte legacy);
/* a copy of orig in g_ListaMat, prepared; with share an equal material of the list instead */
CMaterial *Mat_New(const CMaterial *orig, byte share);
/* a copy of orig added to g_ListaMat, not prepared */
CMaterial *Mat_AddCopy(const CMaterial *orig);
/* the own animation of a sprite: freed; a running copy of the one of mapa (if mapa has one) */
void AnimMat_Free(CAnimMat **slot);
void AnimMat_FromMapa(CAnimMat **slot, const CMapa *mapa);
/* Render_SetMaterial of pass 0 with anim (if set) in place of the animation of the first map */
void Render_SetMaterialAnim(CMaterial *m, CAnimMat *anim);
byte Mat_CompareNames(const CMaterial *a, const CMaterial *b);
const char *Mat_Str(const CMaterial *m);
/* sort order for drawing */
int Mat_Compare(const CMaterial *a, const CMaterial *b);
/* with an opacity: transparent flag or map 1 without b3b (inline in the exe) */
static inline byte Mat_IsTransp(const CMaterial *m) {
	if (m->params.atrib & 0x20) {
		return 0;
	}
	return m->params.transp || (m->mapa[1] && !m->params.b3b);
}
void DDS_Hide(const char *name, uint32_t attr);
void DDS_Unhide(const char *name, uint32_t attr);
/* an extension: a Ruta_Format format, "<x>" for &x; without one "&u&d&n." is put before it */
void Tex_AddExtension(const char *ext);
void Tex_AddPath(const char *ruta);
void Tex_ClearExtensions(void);
void Tex_ClearPaths(void);
/* 0 with the file in out, -1 */
int Tex_FindDDS(const char *name, char *out, int alpha);
int Tex_FindInPaths(const char *name, char *out);
byte Tex_TryFormat(const char *frmt, const char *name, char *out, int alpha);

#endif
