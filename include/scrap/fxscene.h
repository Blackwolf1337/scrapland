/*
 * World/FX/FXScene.cpp of the game: the effects of the cutscene models (cObjElementFXScene*). A scene entity whose
 * model is one of Beating, BillArrives_A, BillArrives_C, Bishops, Decontamination, EnterMatrix, HumanCD, TheEndB or
 * Yakuzzi drops a debris of the matching engine variable file (FXScene_Create); its element follows the model of
 * the entity (the one with life the element holds) until the entity is gone and drives particles, trails and lens
 * flares by the time since the start. The element vtable is the one of World/FX/FXMisc.cpp: 0 Free,
 * 1 Update, 2 Alive, 3 Attach, 4 Reset. The constructors and destructors are in FXMisc.c.
 */
#ifndef SCRAP_FXSCENE_H
#define SCRAP_FXSCENE_H

#include "scrap/gvar.h"
#include "scrap/fxmisc.h"

#define FXBISHOPS_TRAILS 12
#define FXDECON_RAYS 40
#define FXDECON_POINTS 3		/* the inner control points of the curve of a ray */
#define FXMATRIX_TRAILS 16

typedef struct cObjElementFXSceneBeatPickus {
	const cObjElementFXVtbl *vftable;
	float t;
	float tPrev;
	cEntity *ent;
	vec3_t dummy;			/* DM_Scene_Beating */
	CGameParticles dust;		/* Dummy03: dust and smoke puffs */
	float dustT;
	CGameParticles debris;		/* Dummy02: debris and the hits */
	float debrisT;
	CGameParticles sparks;	/* Dummy01: sparks */
	float sparkT;
	float hitT;			/* until the next hit */
	byte started;
} cObjElementFXSceneBeatPickus;

typedef struct cObjElementFXSceneBillArrives_A {
	const cObjElementFXVtbl *vftable;
	float t;
	float tPrev;
	cEntity *ent;
	cEntity *capsule;		/* BillArrives_ACapsule, its embedded effect holds the jets */
	void *flare;			/* CGameLensFlare */
	float jetT;
	CGameParticles particles;	/* Dummy01: smoke and dust */
	float smokeT;
	vec3_t dummy1;			/* DM_Effects_BillArrives_A_01 */
	vec3_t dummy2;			/* DM_Effects_BillArrives_A_02 */
	byte started;
} cObjElementFXSceneBillArrives_A;

typedef struct cObjElementFXSceneBishops {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	cEntity *detritus;		/* BishopsDtritus */
	CGameParticles dataCodec;	/* Dummy01 */
	CGameParticles glow;		/* Dummy02 */
	float t;
	int node;			/* R_Detritus_Tronco of the detritus */
	vec3_t master;			/* the position of the node */
	float glowT;
	CGameTrail trails[FXBISHOPS_TRAILS];
	byte started;
} cObjElementFXSceneBishops;

typedef struct cObjElementFXSceneDecontamination {
	const cObjElementFXVtbl *vftable;
	CGameParticles particles;	/* Dummy01 */
	cEntity *ent;
	cEntity *target;		/* the entity of targetName */
	char targetName[0x20];
	char action[0x20];		/* the action of the target that shows the rays */
	float height;			/* 145 Decontamination, 80 BillArrives_C */
	float t;
	float t2;
	float life[FXDECON_RAYS];	/* of the ray, 0 none */
	float wait[FXDECON_RAYS];	/* until the next ray */
	CGameTrail trails[FXDECON_RAYS];
	float width[FXDECON_RAYS];
	uint32_t color[FXDECON_RAYS];
	int vertex[FXDECON_RAYS];	/* the vertex of the target nearest to the height of the end */
	vec3_t end[FXDECON_RAYS];
	vec3_t pts[FXDECON_RAYS][FXDECON_POINTS];	/* the offsets of the inner points */
	vec3_t vel[FXDECON_RAYS][FXDECON_POINTS];	/* their velocities */
	byte started;
} cObjElementFXSceneDecontamination;

typedef struct cObjElementFXSceneEnterMatrix {
	const cObjElementFXVtbl *vftable;
	float t;
	float tPrev;
	cEntity *ent;
	CGameParticles particles;	/* Dummy01: the glows at the ends of the rays, 6 a ray */
	vec3_t center;			/* of EnterMatrixMatrix */
	float life[FXMATRIX_TRAILS];	/* of the ray, -1 none */
	float wait[FXMATRIX_TRAILS];	/* until the next ray */
	float segLen[FXMATRIX_TRAILS];	/* the length of a segment of the trail */
	float uvOff[FXMATRIX_TRAILS];	/* the scroll of the texture */
	CGameTrail trails[FXMATRIX_TRAILS];
	vec3_t pts[FXMATRIX_TRAILS][5];	/* the control points of the curve of the ray */
	int kind[FXMATRIX_TRAILS];	/* the path of the second part */
	float prog[FXMATRIX_TRAILS];	/* along the path, 1 at its end */
	float speed[FXMATRIX_TRAILS];
	float spread[FXMATRIX_TRAILS];	/* the end ahead of the start */
	int lastKind;			/* -1, no path is taken twice in a row */
	void *flare;			/* CGameLensFlare */
	byte started;
} cObjElementFXSceneEnterMatrix;

typedef struct cObjElementFXSceneHumanCD {
	const cObjElementFXVtbl *vftable;
	float t;
	float tPrev;
	cEntity *ent;
	CGameParticles particles;		/* Dummy01 */
	CGameParticles sparks;		/* Dummy03 */
	vec3_t dummy;			/* DM_Element_MentalControllerMainFX */
	float sparkT;
	byte started;
} cObjElementFXSceneHumanCD;

typedef struct cObjElementFXSceneTheEndB {
	const cObjElementFXVtbl *vftable;
	float t;
	float tPrev;
	cEntity *ent;
	cEntity *bike;			/* TheEndBMoto */
	CGameParticles particles;	/* Dummy01 */
	void *flare;			/* CGameLensFlare */
	byte started;
} cObjElementFXSceneTheEndB;

typedef struct cObjElementFXSceneYakuzzi {
	const cObjElementFXVtbl *vftable;
	float t;
	float tPrev;
	cEntity *ent;
	CGameParticles particles;	/* Dummy01 */
	float emitT;
	vec3_t dummy;			/* DM_Element_TempleYakuzzi_00 */
	byte started;
} cObjElementFXSceneYakuzzi;

/* the scene effect of the model of e (none for other models) */
void FXScene_Create(cEntity *e, const char *model);

/* Attach: the particles to the dummies of the debris model, the positions of the scene dummies */
void cObjElementFXSceneBeatPickus_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneBillArrives_A_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneBishops_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneDecontamination_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneEnterMatrix_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneHumanCD_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneTheEndB_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSceneYakuzzi_Attach(cObjElementFX *base, cDebris *d);

/* Reset */
void cObjElementFXSceneBeatPickus_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneBillArrives_A_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneBishops_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneDecontamination_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneEnterMatrix_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneHumanCD_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneTheEndB_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSceneYakuzzi_Reset(cObjElementFX *base, cDebris *arg);

/* Update: the life of the debris to 0 once the entity is gone */
void cObjElementFXSceneBeatPickus_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneBillArrives_A_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneBishops_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneDecontamination_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneEnterMatrix_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneHumanCD_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneTheEndB_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXSceneYakuzzi_Update(cObjElementFX *base, cDebris *d, float dt);

#endif
