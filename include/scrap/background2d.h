/*
 * Engine/ListaGrf2D.cpp and Engine/fondo2d.cpp of the engine3d: a list of owned 2D sprites, and the backgrounds:
 * CBackground2D, a 512x512 image drawn over the whole viewport in four 256x256 pieces through the map of one material,
 * and the background sprite of the screens (Background2D_Set, drawn once per frame by Background2D_Render).
 */
#ifndef SCRAP_BACKGROUND2D_H
#define SCRAP_BACKGROUND2D_H

#include "scrap/bmp.h"
#include "scrap/graphic2d.h"
#include "scrap/node3d.h"

typedef struct NodeGraphic2D {
	struct NodeGraphic2D *next;		/* 0x0 */
	CGraphic2D *graphic;		/* 0x4 */
} NodeGraphic2D;
SD_SIZE(NodeGraphic2D, 0x8);

typedef struct CGraphic2DList {
	NodeGraphic2D *first;			/* 0x0 */
	NodeGraphic2D *last;			/* 0x4 */
} CGraphic2DList;
SD_SIZE(CGraphic2DList, 0x8);

CGraphic2DList *CGraphic2DList_ctor(CGraphic2DList *self);
/* the sprites deleted */
void CGraphic2DList_DeleteAll(CGraphic2DList *self);
void CGraphic2DList_Render(CGraphic2DList *self);
void CGraphic2DList_Add(CGraphic2DList *self, CGraphic2D *graphic);
/* out of the list (CHECK that it is in it) and deleted */
void CGraphic2DList_Delete(CGraphic2DList *self, CGraphic2D *graphic);
void CGraphic2DList_Remove(CGraphic2DList *self, CGraphic2D *graphic);

#define BACKGROUND2D_PIECE 0x100

typedef struct CBackground2D {
	CBmp bmp;			/* 0x00 */
	ColorIntens color;		/* 0x0c 0xffffff, 1 */
	CMaterial *Mat;			/* 0x14 shared, its map 0 gets the pieces */
} CBackground2D;
SD_SIZE(CBackground2D, 0x18);

CBackground2D *CBackground2D_ctorFile(CBackground2D *self, const char *name);
CBackground2D *CBackground2D_ctor(CBackground2D *self);
/* the color and the material "CFondo2D" */
void CBackground2D_Init(CBackground2D *self);
void CBackground2D_Render(CBackground2D *self);

/* the background sprite: the name set, a new sprite when it changes */
#define BACKGROUND2D_NAME 0x108
extern byte g_BackgroundPending;
extern int g_BackgroundFrame;		/* uframe3d, the frame of the last draw */
extern CGraphic2D *g_BackgroundGraphic;
#define g_BackgroundCurrent (GAME_ARRAY(char, g_BackgroundCurrent))	/* BACKGROUND2D_NAME */
#define g_BackgroundNew (GAME_ARRAY(char, g_BackgroundNew))	/* BACKGROUND2D_NAME */
extern uint32_t g_BackgroundColor;
extern uint32_t g_BackgroundColorNew;

void Background2D_Reset(void);
/* the sprite of the new name (once a frame, CHECK) */
void Background2D_Render(void);
void Background2D_Release(void);
/* name NULL for none; drawn at once unless deferred */
void Background2D_Set(const char *name, uint32_t color, byte deferred);

#endif
