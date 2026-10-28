/*
 * Engine/fxtextura.cpp of the engine3d: the effects of animated maps, chosen by the map name: "(fxlava)" waves of a
 * 128x128 bitmap over itself, "(fxscroll)" scrolls it, "(+fx:NewsPanel)" shows the render target of the news panel.
 * The lava and scroll effects update the texture of the map every 50 ms.
 */
#ifndef SCRAP_FXTEXTURA_H
#define SCRAP_FXTEXTURA_H

#include "scrap/bmp.h"
#include "scrap/material.h"

#define IFXTEXTURA_VFTABLE GAME_VFTABLE(IFXTextura_vftable, 0x3e2c44)
#define CFXTEXLAVA_VFTABLE GAME_VFTABLE(CFXTexLava_vftable, 0x3e2c54)
#define CFXTEXSCROLL_VFTABLE GAME_VFTABLE(CFXTexScroll_vftable, 0x3e2c64)
#define CFXNEWSPANEL_VFTABLE GAME_VFTABLE(CFXNewsPanel_vftable, 0x3e2c74)
#define FXTEX_SIZE 128
#define FXTEX_PERIOD 50

/* the kinds of FXTextura_Create (CMapa fx) */
enum { FXTEX_LAVA = 1, FXTEX_SCROLL = 2, FXTEX_NEWSPANEL = 3 };

/* the vftable of IFXTextura: [0] the deleting destructor, [1] int Update(CMapa *), [2] void *GetTexture() */
typedef struct IFXTextura {
	void **vftable;			/* 0x0 */
} IFXTextura;

typedef struct CFXTexLava {
	IFXTextura base;
	CBmp *bmp;			/* 0x04 the file, changed in place and copied into the map */
	CBmp *orig;			/* 0x08 a copy of it */
	uint32_t lastTime;		/* 0x0c */
} CFXTexLava;
SD_SIZE(CFXTexLava, 0x10);

typedef struct CFXTexScroll {
	IFXTextura base;
	CBmp *bmp;			/* 0x04 */
	CBmp *orig;			/* 0x08 */
	uint32_t lastTime;		/* 0x0c */
	int offset;			/* 0x10 the pixels scrolled */
	int i14;			/* 0x14 */
} CFXTexScroll;
SD_SIZE(CFXTexScroll, 0x18);

typedef struct CFXNewsPanel {
	IFXTextura base;
	int time;			/* 0x4 of the last update */
} CFXNewsPanel;
SD_SIZE(CFXNewsPanel, 0x8);

/* the cos of the 256 angles of the lava, made by the first lava */
#define g_LavaCos (GAME_ARRAY(float, g_LavaCos, 0x4db900))
#define g_LavaCosInic GAME_GLOBAL(byte, g_LavaCosInic, 0x4dbd00)
#define g_LavaOffset (GAME_ARRAY(int, g_LavaOffset, 0x4dbd08))	/* FXTEX_SIZE, the row offset of each column */

void __fastcall IFXTextura_dtor(IFXTextura *self);
IFXTextura *__thiscall IFXTextura_Delete(IFXTextura *self, byte flags);
void *__fastcall IFXTextura_GetTexture(IFXTextura *self);
CFXTexLava *__thiscall CFXTexLava_ctor(CFXTexLava *self, const char *file);
void __fastcall CFXTexLava_dtor(CFXTexLava *self);
CFXTexLava *__thiscall CFXTexLava_Delete(CFXTexLava *self, byte flags);
int __thiscall CFXTexLava_Update(CFXTexLava *self, CMapa *mapa);
CFXTexScroll *__thiscall CFXTexScroll_ctor(CFXTexScroll *self, const char *file);
void __fastcall CFXTexScroll_dtor(CFXTexScroll *self);
CFXTexScroll *__thiscall CFXTexScroll_Delete(CFXTexScroll *self, byte flags);
int __thiscall CFXTexScroll_Update(CFXTexScroll *self, CMapa *mapa);
CFXNewsPanel *__fastcall CFXNewsPanel_ctor(CFXNewsPanel *self);
CFXNewsPanel *__thiscall CFXNewsPanel_Delete(CFXNewsPanel *self, byte flags);
int __thiscall CFXNewsPanel_Update(CFXNewsPanel *self, CMapa *mapa);
void *__fastcall CFXNewsPanel_GetTexture(CFXNewsPanel *self);
/* the effect of kind for the file of mapa, NULL for an unknown kind */
void *FXTextura_Create(byte kind, CMapa *mapa);
/* the kind of the first "(...)" tag of the name that is one, 0 for none */
byte FXTextura_Kind(const char *name);

#endif
