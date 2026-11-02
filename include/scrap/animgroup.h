/*
 * Engine/AnimGrupo.cpp of the engine3d: animation groups. A group (CAnimGroup) plays the animation of a scene
 * (CScene, Engine/escena.cpp) loaded as an animation ("Anim", its CAnimation in AnimM3D) on another scene: the
 * nodes are matched by name, the table at AnimM3D+0x18 maps the iObjeto of each scene node to the animation node.
 * A group has an event list (the end event at the last frame), the play state (CAnimCtrl) and for vertex
 * animations up to MAX_MORPHDATA morph steps read from the "Morphing:<name>" section of the scene's ini. A scene
 * keeps its groups in a CAnimGroups of MAX_ANIMGROUPS slots.
 *
 * CAnimation and CAnimNode belong to Engine/animacion.cpp, CScene to Engine/escena.cpp: only the members read
 * here are named.
 */
#ifndef SCRAP_ANIMGROUP_H
#define SCRAP_ANIMGROUP_H

#include "scrap/animation.h"
#include "scrap/archio.h"
#include "scrap/controlanim.h"
#include "scrap/scene.h"
#include "scrap/light.h"
#include "scrap/mathlib.h"
#include "scrap/table.h"

#define MAX_ANIMGROUPS 44
#define MAX_MORPHDATA 10
#define ANIM_VERT 0x1000		/* CAnimNode_GetFlags: vertex animation */
#define ANIMEVENT_END 0xd		/* the event a group puts at its last frame */





typedef struct MorphData {
	char Tag[2];			/* 0x0 */
	byte Target;			/* 0x2 */
	byte param3;			/* 0x3 0 by default */
	byte Interp;			/* 0x4 */
	byte NextMin;			/* 0x5 */
	byte NextMax;			/* 0x6 */
	byte param7;			/* 0x7 100 by default */
} MorphData;

typedef struct CAnimGroup {
	char *name;			/* 0x00 String_Set */
	CAnimEvent *events;		/* 0x04 */
	CAnimCtrl ctrl;			/* 0x08 */
	MorphData *morph;		/* 0x8c new[] of MAX_MORPHDATA */
	union {
		uint16_t morphAct;	/* 0x90 */
		struct {
			byte iMorph;	/* 0x90 the step played */
			byte morphCount; /* 0x91 its repetitions left */
		};
	};
	uint16_t pad_92;
	CScene *Anim;			/* 0x94 a reference */
} CAnimGroup;

typedef struct CAnimGroups {
	CAnimGroup *g[MAX_ANIMGROUPS];
} CAnimGroups;

/* of other modules, called in the exe */
/* Engine/Nodo3D.cpp: the node named name from self on (self itself with incl), by next */
/* Engine/escena.cpp: the node with that iObjeto */

/* the groups destroyed and their slots cleared */
CAnimGroups *CAnimGroups_ctor(CAnimGroups *self);
void CAnimGroups_Clear(CAnimGroups *self);
void CAnimGroup_dtor(CAnimGroup *self);
/* the events of the list deleted */
void AnimEvents_Free(CAnimEvent **list);
/* the step with the 2 character tag, -1 none */
int MorphData_Find(const MorphData *md, const char *tag);
/* "<i>_Data" (6 bytes from Target on) and "<i>_Tag" of the section; 0 without the data */
byte MorphData_Read(CIniFile *ini, const char *sect, int i, MorphData *d, int targets);
/* into the first free slot, its index */
int CAnimGroups_Add(CAnimGroups *self, CAnimGroup *g);
CAnimGroup *CAnimGroups_Get(CAnimGroups *self, uint32_t i);
/* by name (no case), its slot into *idx */
CAnimGroup *CAnimGroups_Find(CAnimGroups *self, const char *name, int *idx);
int CAnimGroups_Count(CAnimGroups *self);
/* the animation node of the scene node named node moved to where that node is in scene; without it in anim the
 * animation is marked a morph (vertex) animation */
byte AnimGroup_AlignNode(CScene *scene, CScene *anim, const char *node);
/* a new group playing anim on scene, named name or after the animation */
CAnimGroup *AnimGroup_New(CScene *scene, CScene *anim, const char *name);
/* the table AnimM3D->objetos of anim for the nodes of scene */
void AnimGroup_MapNodes(CScene *scene, CScene *anim);
/* the animated nodes moved to the front of the animation (iObjeto of the scene nodes swapped with them) */
void AnimGroup_Compact(CScene *anim);

#endif
