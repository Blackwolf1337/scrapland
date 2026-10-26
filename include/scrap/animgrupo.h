/*
 * Engine/AnimGrupo.cpp of the engine3d: animation groups. A group (CAnimGrupo) plays the animation of a scene
 * (CEscena, Engine/escena.cpp) loaded as an animation ("Anim", its CAnimacion in AnimM3D) on another scene: the
 * nodes are matched by name, the table at AnimM3D+0x18 maps the iObjeto of each scene node to the animation node.
 * A group has an event list (the end event at the last frame), the play state (CAnimCtrl) and for vertex
 * animations up to MAX_MORPHDATA morph steps read from the "Morphing:<name>" section of the scene's ini. A scene
 * keeps its groups in a CAnimGrupos of MAX_ANIMGRUPOS slots.
 *
 * CAnimacion and CAnimNodo belong to Engine/animacion.cpp, CEscena to Engine/escena.cpp: only the members read
 * here are named.
 */
#ifndef SCRAP_ANIMGRUPO_H
#define SCRAP_ANIMGRUPO_H

#include "scrap/archio.h"
#include "scrap/luz.h"
#include "scrap/mathlib.h"
#include "scrap/tabla.h"

#define MAX_ANIMGRUPOS 44
#define MAX_MORPHDATA 10
#define ANIM_VERT 0x1000		/* CAnimNodo_GetFlags: vertex animation */
#define ANIMEVENT_FIN 0xd		/* the event a group puts at its last frame */

/* 0x24 bytes, the first frame as a word at 0x0 */
typedef struct CAnimNodo CAnimNodo;

typedef struct CAnimacion {
	const char *name;		/* 0x00 */
	int pad_4;
	float vel;			/* 0x08 copied to CAnimCtrl.vel */
	int primero;			/* 0x0c the first frame */
	int frames;			/* 0x10 */
	byte morph;			/* 0x14 all nodes are ANIM_VERT */
	byte pad_15[3];
	CTabla objetos;			/* 0x18 bytes: iObjeto of a scene node to animation node + 1, 0 none */
	CAnimNodo *AnimNodo;		/* 0x20 */
	int nAnimNodo;			/* 0x24 */
	void *MemBlock;			/* 0x28 AnimBlock.MemBlock */
} CAnimacion;
SD_OFFSET(CAnimacion, objetos, 0x18);
SD_OFFSET(CAnimacion, nAnimNodo, 0x24);


/* an entry of the sorted event list of a group (constructor rva 0x2bb8e0) */
typedef struct CAnimEvent {
	int id;				/* 0x00 */
	int arg;			/* 0x04 */
	struct CAnimEvent *sig;		/* 0x08 */
	float t;			/* 0x0c the frame */
	uint32_t flags;			/* 0x10 1: in a list */
} CAnimEvent;
SD_SIZE(CAnimEvent, 0x14);

/* the play state of a group (constructor rva 0x250e50, empty destructor 0x250f10) */
typedef struct CAnimCtrl {
	float vel;			/* 0x00 */
	byte pad_4[0x2c];
	CAnimNodo *nodo;		/* 0x30 */
	int primero;			/* 0x34 */
	int frames;			/* 0x38 */
	CAnimEvent **eventos;		/* 0x3c */
	byte pad_40[0x44];
} CAnimCtrl;
SD_OFFSET(CAnimCtrl, nodo, 0x30);
SD_OFFSET(CAnimCtrl, eventos, 0x3c);
SD_SIZE(CAnimCtrl, 0x84);

typedef struct MorphData {
	char Tag[2];			/* 0x0 */
	byte Target;			/* 0x2 */
	byte param3;			/* 0x3 0 by default */
	byte Interp;			/* 0x4 */
	byte NextMin;			/* 0x5 */
	byte NextMax;			/* 0x6 */
	byte param7;			/* 0x7 100 by default */
} MorphData;
SD_SIZE(MorphData, 8);

typedef struct CAnimGrupo {
	char *name;			/* 0x00 Cad_Set */
	CAnimEvent *eventos;		/* 0x04 */
	CAnimCtrl ctrl;			/* 0x08 */
	MorphData *morph;		/* 0x8c new[] of MAX_MORPHDATA */
	uint16_t morphAct;		/* 0x90 */
	uint16_t pad_92;
	CEscena *Anim;			/* 0x94 a reference */
} CAnimGrupo;
SD_OFFSET(CAnimGrupo, morph, 0x8c);
SD_OFFSET(CAnimGrupo, Anim, 0x94);
SD_SIZE(CAnimGrupo, 0x98);

typedef struct CAnimGrupos {
	CAnimGrupo *g[MAX_ANIMGRUPOS];
} CAnimGrupos;

/* of other modules, called in the exe */
/* Engine/Nodo3D.cpp: the node named name from self on (self itself with incl), by sig */
/* Engine/escena.cpp: the node with that iObjeto */
CNodo3D *__thiscall CEscena_NodoObjeto(CEscena *self, int iObjeto);
CAnimCtrl *__thiscall CAnimCtrl_ctor(CAnimCtrl *self);
void __thiscall CAnimCtrl_dtor(CAnimCtrl *self);
void __thiscall CAnimCtrl_SetFrames(CAnimCtrl *self, int primero, int frames);
CAnimEvent *__thiscall CAnimEvent_ctor(CAnimEvent *self, float t, int id, int arg);
/* the rest of the chain deleted */
void __thiscall CAnimEvent_dtor(CAnimEvent *self);
/* ev into the list sorted by t, then id */
CAnimEvent *__thiscall AnimEventos_Add(CAnimEvent **list, CAnimEvent *ev);
/* Engine/animacion.cpp */
CAnimNodo *__thiscall CAnimacion_GetNodo(CAnimacion *self, uint32_t i);	/* obtAnimNodo */
CAnimNodo *__thiscall CAnimacion_NodoObjeto(CAnimacion *self, uint32_t iObjeto);	/* through objetos */
void __thiscall CAnimacion_SwapNodos(CAnimacion *self, uint32_t i, uint32_t j);
/* the empty nodes at the end removed */
void __thiscall CAnimacion_Trim(CAnimacion *self);
uint32_t __thiscall CAnimNodo_GetFlags(CAnimNodo *self);	/* obtFlags */
byte __thiscall CAnimNodo_GetPosRot(CAnimNodo *self, vec3_t *pos, float *rot, int frame, float subframe);
/* d added to the positions */
int __thiscall CAnimNodo_Translate(CAnimNodo *self, const vec3_t *d);

/* the groups destroyed and their slots cleared */
void __fastcall CAnimGrupos_Clear(CAnimGrupos *self);
void __fastcall CAnimGrupo_dtor(CAnimGrupo *self);
/* the events of the list deleted */
void __fastcall AnimEventos_Free(CAnimEvent **list);
/* the step with the 2 character tag, -1 none */
int MorphData_Find(const MorphData *md, const char *tag);
/* "<i>_Data" (6 bytes from Target on) and "<i>_Tag" of the section; 0 without the data */
byte MorphData_Read(CArchINI *ini, const char *sect, int i, MorphData *d, int targets);
/* into the first free slot, its index */
int __thiscall CAnimGrupos_Add(CAnimGrupos *self, CAnimGrupo *g);
CAnimGrupo *__thiscall CAnimGrupos_Get(CAnimGrupos *self, uint32_t i);
/* by name (no case), its slot into *idx */
CAnimGrupo *__thiscall CAnimGrupos_Find(CAnimGrupos *self, const char *name, int *idx);
int __fastcall CAnimGrupos_Count(CAnimGrupos *self);
/* the animation node of the scene node named nodo moved to where that node is in escena; without it in anim the
 * animation is marked a morph (vertex) animation */
byte AnimGrupo_AlignNode(CEscena *escena, CEscena *anim, const char *nodo);
/* a new group playing anim on escena, named nombre or after the animation */
CAnimGrupo *AnimGrupo_New(CEscena *escena, CEscena *anim, const char *nombre);
/* the table AnimM3D->objetos of anim for the nodes of escena */
void AnimGrupo_MapNodes(CEscena *escena, CEscena *anim);
/* the animated nodes moved to the front of the animation (iObjeto of the scene nodes swapped with them) */
void AnimGrupo_Compact(CEscena *anim);

#endif
