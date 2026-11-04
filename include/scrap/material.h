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
	void *tex;			/* Texture_GetD3D */
	void *Hint;			/* CTexture, referenced */
} CTextStatic;

typedef struct CMap {
	char *Name;			/* as in the model */
	char *path;			/* the file found (CMap_ResolvePath) */
	byte filter;			/* 0 for "Filter: no" */
	byte lightmap;			/* lightmap level, 0 for none */
	byte ba;
	byte env2;			/* "(env2)" in the name of the reflection map */
	byte tile;			/* "TileU" (bit 0), "TileV" (bit 1) */
	byte mirror;			/* "EspejoU" (bit 0), "EspejoV" (bit 1) */
	byte fx;			/* kind of FXTexture, 0 for none */
	byte pad_f;
	float uvOffset[2];		/* "DesplazU", "DesplazV" */
	float uvScale[2];		/* "EscalaU", "EscalaV" */
	float uvRot[3];			/* the "Angulo" in z */
	float amount;			/* of the map in the material */
	CTextStatic *cur;			/* est or a frame of TextAni */
	CTextStatic staticTex;
	byte animated;			/* pathAni is set */
	byte pad_3d[3];
	int frames;
	char *pathAni;			/* printf format of the frame files */
	CTextStatic *TextAni;		/* new[] of frames */
	void *AnimMat;
	struct IFXTexture *FXTexture;
	byte hasAlpha;
	byte compressed;		/* the texture is compressed */
	byte pad_56[2];
	int state;			/* MAP_MISSING: the file was not found */
	void *MultiSprite;
} CMap;

#define MAP_MISSING 2

/* the colors are bytes R G B A; diffuse, ambient, specular and emissive of colors[] are D3DMATERIAL9 order */
typedef struct CMatParams {
	byte ambient[4];
	byte diffuse[4];		/* the alpha */
	byte specular[4];
	byte emissive[4];
	float power;
	float specLevel;
	byte color[4];
	int matClass;			/* 0 "Standard", 1 "Multi/Sub-Object" */
	int srcBlend;			/* 5 */
	int dstBlend;			/* 6 */
	byte twoSided;			/* "TwoSided" */
	byte lighting;			/* D3DRS_LIGHTING with the material colors */
	byte transp;
	byte reflection;		/* a reflection map is set */
	uint16_t attrib;			/* MAT_* */
	byte fog;			/* D3DRS_FOGENABLE when the fog is on */
	byte zWrite;			/* D3DRS_ZWRITEENABLE */
	int zFunc;			/* D3DRS_ZFUNC: 4 less or equal, 8 always */
} CMatParams;

/* the shader mode of a material for the renderer (Engine/RenderShader.cpp) */
typedef struct CMatId {
	int id;				/* the shader mode, -1 until registered */
	byte alphaTest;			/* the low byte of the flags of the mode */
	byte blend;			/* the next byte */
	byte alpha;			/* the diffuse map has an alpha (and the mask flag, see CMatId_Register) */
	byte pad_7;
	int gen;			/* the shader mode mask it was registered with */
} CMatId;

/* the bases without a vtable of the named and counted objects of objref.h: CScene, CNode3D and CMaterial have the
 * name at 0x4 and the count at 0x8, CMatList only the name */
typedef struct CObjName {
	char *name;
} CObjName;

typedef struct CObjRef {
	int Refs;
} CObjRef;

typedef struct CMatList CMatList;

typedef struct CMaterial {
	char *Name;
	int Refs;			/* the lists holding it */
	CMatList *sub;
	CMatParams params;
	struct CMaterial *next;
	struct CMaterial *prev;
	float colors[17];		/* diffuse, ambient, specular, emissive, power */
	CMatId id;
	CMap *map[MAP_TOTAL];
} CMaterial;


struct CMatList {
	char *Name;
	CMaterial *first;
	CMaterial *last;
};

/* the materials of the engine */
extern CMatList g_MatList;

/* the string freed if set (the destructor of the string members) */
void String_dtor(char **self);
CMatId *CMatId_ctor(CMatId *self);		/* id -1 */
void CMatId_dtor(CMatId *self);
CMaterial *CMaterial_ctor(CMaterial *self);
/* the D3D colors (CMaterial_CalcColors) times k unless k is 1, the alpha of colors from diffuse[3] */
void CMaterial_ScaleColors(CMaterial *self, float k);
void CMaterial_dtor(CMaterial *self);
/* free the old string, a Mem_Alloc copy of s (NULL stays NULL) */
/* the animation of an animated texture (Engine/ControlAnimMat.cpp) */
typedef struct CAnimMat {
	float fps;
	int frames;
	int cycle;			/* the frame it loops back to, -1: stops at the end */
	float dt;			/* the seconds of the last step (at most 0.1) */
	int frame;
	float framef;
	int mode;			/* ANIMMAT_REPR .. ANIMMAT_END */
	float fadeRate;			/* FUND_PAUSA: the slow down per second */
	float fadeK;			/* FUND_PAUSA: the speed factor, down to 0.2 */
	uint32_t lastTime;		/* g_TimeMs of the last step */
	int lastFrame;			/* g_FrameCount of the last step */
} CAnimMat;
#define ANIMMAT_REPR 0			/* playing */
#define ANIMMAT_PAUSE 1
#define ANIMMAT_FADE_PAUSE 2		/* slowing down */
#define ANIMMAT_END 3
CAnimMat *CAnimMat_ctor(CAnimMat *self);
/* a copy of fps, frames and cycle of src */
CAnimMat *CAnimMat_ctorCopy(CAnimMat *self, const CAnimMat *src);
void CAnimMat_dtor(CAnimMat *self);
/* from frame 0 at the time of now */
void CAnimMat_Start(CAnimMat *self);
/* the frame of now (updated once each frame) */
int CAnimMat_GetFrame(CAnimMat *self);
void String_Set(char **self, const char *s);
/* *self a String_Dup of the printf of fmt, NULL (freed) for fmt NULL */
void String_Printf(char **self, const char *fmt, ...);
/* and 0x2a1920: the string as a CFileIO string (empty is NULL) */
void String_Read(char **self, CFileIO *f);
void String_Write(char **self, CFileIO *f);

byte CMap_BuildMultiSprite(CMap *self);
void *CMap_GetTexture(CMap *self, byte load);
struct CImage;
/* the w x h pixels of bmp at (sx, sy) into the texture at (dx, dy); 0 when not done */
byte CMap_CopyBmp(CMap *self, int dx, int dy, const struct CImage *bmp, int sx, int sy, int w, int h);
/* FXTexture_Kind (the kind of FXTexture of a map name): fxtexture.h */

/* name, parameters and copies of the maps */
void CMaterial_Assign(CMaterial *self, const CMaterial *orig);
void *CMaterial_GetMultiSprite(CMaterial *self, uint32_t mapid);
int CMaterial_Read(CMaterial *self, CFileIO *f);
int CMaterial_Write(CMaterial *self, CFileIO *f);
int CMaterial_GetId(CMaterial *self, CMatId **out);
/* 1 when the mode is up to date, else registered again (deferred for an unregistered one) and 0 */
byte CMatId_Check(CMatId *self, CMaterial *mat);
/* the mode of the maps and flags of mat, CHECK(id == -1) */
void CMatId_Register(CMatId *self, CMaterial *mat);
char *CMaterial_GetShaderName(CMaterial *self, char *str, uint32_t nbytes);
void CMaterial_Prepare(CMaterial *self, void *dev);
CMap *CMaterial_SetMap(CMaterial *self, uint32_t mapid, const char *name);
CMap *CMaterial_SetMapCopy(CMaterial *self, uint32_t mapid, const CMap *orig);
/* colors as a D3DMATERIAL9 */
void CMaterial_CalcColors(CMaterial *self, float *out);
/* the average color of map 0 as the diffuse color, the map deleted */
void CMaterial_MapToColor(CMaterial *self);
char *CMaterial_GetMapName(CMaterial *self, uint32_t mapid);
byte CMaterial_GetMapSize(CMaterial *self, uint32_t mapid, int *w, int *h);

void CMatList_Add(CMatList *self, CMaterial *mat);
/* an equal shareable material, also in the sub lists */
CMaterial *CMatList_Find(CMatList *self, const CMaterial *mat);
void CMatList_Prepare(CMatList *self, void *dev);
int CMatList_Count(CMatList *self, byte sub);
CMaterial *CMatList_Extract(CMatList *self, CMaterial *mat, byte sub);
void CMatList_Release(CMatList *self);
void CMatList_DeleteAll(CMatList *self);
void CMatList_Delete(CMatList *self, CMaterial *mat);

/* a copy of orig in (&g_MatList), prepared; with share an equal material of the list instead */
CMaterial *Mat_New(const CMaterial *orig, byte share);
/* a copy of orig added to (&g_MatList), not prepared */
CMaterial *Mat_AddCopy(const CMaterial *orig);
/* the own animation of a sprite: freed; a running copy of the one of map (if map has one) */
void AnimMat_Free(CAnimMat **slot);
void AnimMat_FromMap(CAnimMat **slot, const CMap *map);
/* Render_SetMaterial of pass 0 with anim (if set) in place of the animation of the first map */
void Render_SetMaterialAnim(CMaterial *m, CAnimMat *anim);
/* sort order for drawing */
int Mat_Compare(const CMaterial *a, const CMaterial *b);
/* with an opacity: transparent flag or map 1 without b3b (inline in the exe) */
static inline byte Mat_IsTransp(const CMaterial *m) {
	if (m->params.attrib & 0x20) {
		return 0;
	}
	return m->params.transp || (m->map[1] && !m->params.reflection);
}
/* an extension: a Path_Format format, "<x>" for &x; without one "&u&d&n." is put before it */
void Tex_AddExtension(const char *ext);
void Tex_AddPath(const char *path);
void Tex_ClearExtensions(void);
void Tex_ClearPaths(void);
void CMaterial_Free(CMaterial *self);
void CMatList_Free(CMatList *self);

#endif
