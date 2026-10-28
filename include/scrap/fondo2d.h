/*
 * Engine/ListaGrf2D.cpp and Engine/fondo2d.cpp of the engine3d: a list of owned 2D sprites, and the backgrounds:
 * CFondo2D, a 512x512 image drawn over the whole viewport in four 256x256 pieces through the map of one material,
 * and the background sprite of the screens (Fondo2D_Set, drawn once per frame by Fondo2D_Render).
 */
#ifndef SCRAP_FONDO2D_H
#define SCRAP_FONDO2D_H

#include "scrap/bmp.h"
#include "scrap/grafico2d.h"
#include "scrap/nodo3d.h"

typedef struct NodoGrf2D {
	struct NodoGrf2D *sig;		/* 0x0 */
	CGrafico2D *grf;		/* 0x4 */
} NodoGrf2D;
SD_SIZE(NodoGrf2D, 0x8);

typedef struct CListaGrf2D {
	NodoGrf2D *pri;			/* 0x0 */
	NodoGrf2D *ult;			/* 0x4 */
} CListaGrf2D;
SD_SIZE(CListaGrf2D, 0x8);

CListaGrf2D *__fastcall CListaGrf2D_ctor(CListaGrf2D *self);
/* the sprites deleted */
void __fastcall CListaGrf2D_DeleteAll(CListaGrf2D *self);
void __fastcall CListaGrf2D_Render(CListaGrf2D *self);
void __thiscall CListaGrf2D_Add(CListaGrf2D *self, CGrafico2D *grf);
/* out of the list (CHECK that it is in it) and deleted */
void __thiscall CListaGrf2D_Delete(CListaGrf2D *self, CGrafico2D *grf);
void __thiscall CListaGrf2D_Remove(CListaGrf2D *self, CGrafico2D *grf);

#define FONDO2D_PIECE 0x100

typedef struct CFondo2D {
	CBmp bmp;			/* 0x00 */
	ColorIntens color;		/* 0x0c 0xffffff, 1 */
	CMaterial *Mat;			/* 0x14 shared, its map 0 gets the pieces */
} CFondo2D;
SD_SIZE(CFondo2D, 0x18);

CFondo2D *__thiscall CFondo2D_ctorFile(CFondo2D *self, const char *name);
CFondo2D *__fastcall CFondo2D_ctor(CFondo2D *self);
/* the color and the material "CFondo2D" */
void __fastcall CFondo2D_Init(CFondo2D *self);
void __fastcall CFondo2D_Render(CFondo2D *self);

/* the background sprite: the name set, a new sprite when it changes */
#define FONDO2D_NAME 0x108
#define g_FondoPendiente GAME_GLOBAL(byte, g_FondoPendiente, 0x4db490)
#define g_FondoFrame GAME_GLOBAL(int, g_FondoFrame, 0x4db494)		/* uframe3d, the frame of the last draw */
#define g_FondoGrf GAME_GLOBAL(CGrafico2D *, g_FondoGrf, 0x4db498)
#define g_FondoActual (GAME_ARRAY(char, g_FondoActual, 0x4db4a0))	/* FONDO2D_NAME */
#define g_FondoNuevo (GAME_ARRAY(char, g_FondoNuevo, 0x4db5a8))	/* FONDO2D_NAME */
#define g_FondoColor GAME_GLOBAL(uint32_t, g_FondoColor, 0x432a10)
#define g_FondoColorNuevo GAME_GLOBAL(uint32_t, g_FondoColorNuevo, 0x432a14)

void Fondo2D_Reset(void);
/* the sprite of the new name (once a frame, CHECK) */
void Fondo2D_Render(void);
void Fondo2D_Release(void);
/* name NULL for none; drawn at once unless deferred */
void Fondo2D_Set(const char *name, uint32_t color, byte deferred);

#endif
