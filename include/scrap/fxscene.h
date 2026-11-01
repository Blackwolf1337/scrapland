/*
 * World/FX/FXScene.cpp of the game: the effects of the cutscene models (cObjElementFXScene*). A scene entity whose
 * model is one of Beating, BillArrives_A, BillArrives_C, Bishops, Decontamination, EnterMatrix, HumanCD, TheEndB or
 * Yakuzzi drops a debris of the matching engine variable file (FXScene_Create); its element follows the model of
 * the entity (the one with life the element holds) until the entity is gone and drives particles, trails and lens
 * flares by the time since the start. The element vtable is the one of World/FX/FXMisc.cpp: 0 deleting destructor,
 * 1 Update, 2 Alive, 3 Attach, 4 Reset. The constructors and destructors are in FXMisc.c.
 */
#ifndef SCRAP_FXSCENE_H
#define SCRAP_FXSCENE_H

#include "scrap/fxmisc.h"

/* the engine variables (Gvars): the debris system files of the scenes */
#define g_FXSceneBeatingFile GAME_GLOBAL_IN(const char *, GVar_FXSceneBeatPickusFile, 16)
#define g_FXSceneBillArrivesAFile GAME_GLOBAL_IN(const char *, GVar_FXSceneBillArrives_AFile, 16)
#define g_FXSceneDecontaminationFile GAME_GLOBAL_IN(const char *, GVar_FXSceneDecontaminationFile, 16)	/* also BillArrives_C */
#define g_FXSceneBishopsFile GAME_GLOBAL_IN(const char *, GVar_FXSceneBishopsFile, 16)
#define g_FXSceneEnterMatrixFile GAME_GLOBAL_IN(const char *, GVar_FXSceneEnterMatrixFile, 16)
#define g_FXSceneHumanCDFile GAME_GLOBAL_IN(const char *, GVar_FXSceneHumanCDFile, 16)
#define g_FXSceneTheEndBFile GAME_GLOBAL_IN(const char *, GVar_FXSceneTheEndBFile, 16)
#define g_FXSceneYakuzziFile GAME_GLOBAL_IN(const char *, GVar_FXSceneYakuzziFile, 16)

#define FXBISHOPS_TRAILS 12
#define FXDECON_RAYS 40
#define FXDECON_POINTS 3		/* the inner control points of the curve of a ray */
#define FXMATRIX_TRAILS 16

typedef struct cObjElementFXSceneBeatPickus {
	void *vftable;			/* 0x000 */
	float t;			/* 0x004 */
	float tPrev;			/* 0x008 */
	cEntity *ent;			/* 0x00c */
	vec3_t dummy;			/* 0x010 DM_Scene_Beating */
	CGameParticles dust;		/* 0x01c Dummy03: dust and smoke puffs */
	float dustT;			/* 0x0cc */
	CGameParticles debris;		/* 0x0d0 Dummy02: debris and the hits */
	float debrisT;			/* 0x180 */
	CGameParticles sparks;	/* 0x184 Dummy01: sparks */
	float sparkT;			/* 0x234 */
	float hitT;			/* 0x238 until the next hit */
	byte started;			/* 0x23c */
} cObjElementFXSceneBeatPickus;
SD_OFFSET(cObjElementFXSceneBeatPickus, sparks, 0x184);
SD_OFFSET(cObjElementFXSceneBeatPickus, started, 0x23c);

typedef struct cObjElementFXSceneBillArrives_A {
	void *vftable;			/* 0x00 */
	float t;			/* 0x04 */
	float tPrev;			/* 0x08 */
	cEntity *ent;			/* 0x0c */
	cEntity *capsule;		/* 0x10 BillArrives_ACapsule, its embedded effect holds the jets */
	void *flare;			/* 0x14 CGameLensFlare */
	float jetT;			/* 0x18 */
	CGameParticles particles;	/* 0x1c Dummy01: smoke and dust */
	float smokeT;			/* 0xcc */
	vec3_t dummy1;			/* 0xd0 DM_Effects_BillArrives_A_01 */
	vec3_t dummy2;			/* 0xdc DM_Effects_BillArrives_A_02 */
	byte started;			/* 0xe8 */
} cObjElementFXSceneBillArrives_A;
SD_OFFSET(cObjElementFXSceneBillArrives_A, dummy2, 0xdc);
SD_OFFSET(cObjElementFXSceneBillArrives_A, started, 0xe8);

typedef struct cObjElementFXSceneBishops {
	void *vftable;			/* 0x000 */
	cEntity *ent;			/* 0x004 */
	cEntity *detritus;		/* 0x008 BishopsDtritus */
	CGameParticles dataCodec;	/* 0x00c Dummy01 */
	CGameParticles glow;		/* 0x0bc Dummy02 */
	float t;			/* 0x16c */
	int node;			/* 0x170 R_Detritus_Tronco of the detritus */
	vec3_t master;			/* 0x174 the position of the node */
	float glowT;			/* 0x180 */
	CGameTrail trails[FXBISHOPS_TRAILS];	/* 0x184 */
	byte started;			/* 0x1b4 */
} cObjElementFXSceneBishops;
SD_OFFSET(cObjElementFXSceneBishops, glow, 0xbc);
SD_OFFSET(cObjElementFXSceneBishops, trails, 0x184);
SD_OFFSET(cObjElementFXSceneBishops, started, 0x1b4);

typedef struct cObjElementFXSceneDecontamination {
	void *vftable;			/* 0x0000 */
	CGameParticles particles;	/* 0x0004 Dummy01 */
	cEntity *ent;			/* 0x00b4 */
	cEntity *target;		/* 0x00b8 the entity of targetName */
	char targetName[0x20];		/* 0x00bc */
	char action[0x20];		/* 0x00dc the action of the target that shows the rays */
	float height;			/* 0x00fc 145 Decontamination, 80 BillArrives_C */
	float t;			/* 0x0100 */
	float t2;			/* 0x0104 */
	float life[FXDECON_RAYS];	/* 0x0108 of the ray, 0 none */
	float wait[FXDECON_RAYS];	/* 0x01a8 until the next ray */
	CGameTrail trails[FXDECON_RAYS];	/* 0x0248 */
	float width[FXDECON_RAYS];	/* 0x02e8 */
	uint32_t color[FXDECON_RAYS];	/* 0x0388 */
	int vertex[FXDECON_RAYS];	/* 0x0428 the vertex of the target nearest to the height of the end */
	vec3_t end[FXDECON_RAYS];	/* 0x04c8 */
	vec3_t pts[FXDECON_RAYS][FXDECON_POINTS];	/* 0x06a8 the offsets of the inner points */
	vec3_t vel[FXDECON_RAYS][FXDECON_POINTS];	/* 0x0c48 their velocities */
	byte started;			/* 0x11e8 */
} cObjElementFXSceneDecontamination;
SD_OFFSET(cObjElementFXSceneDecontamination, height, 0xfc);
SD_OFFSET(cObjElementFXSceneDecontamination, trails, 0x248);
SD_OFFSET(cObjElementFXSceneDecontamination, end, 0x4c8);
SD_OFFSET(cObjElementFXSceneDecontamination, vel, 0xc48);
SD_OFFSET(cObjElementFXSceneDecontamination, started, 0x11e8);

typedef struct cObjElementFXSceneEnterMatrix {
	void *vftable;			/* 0x000 */
	float t;			/* 0x004 */
	float tPrev;			/* 0x008 */
	cEntity *ent;			/* 0x00c */
	CGameParticles particles;	/* 0x010 Dummy01: the glows at the ends of the rays, 6 a ray */
	byte pad_c0[0x180 - 0xc0];
	vec3_t center;			/* 0x180 of EnterMatrixMatrix */
	float life[FXMATRIX_TRAILS];	/* 0x18c of the ray, -1 none */
	float wait[FXMATRIX_TRAILS];	/* 0x1cc until the next ray */
	float segLen[FXMATRIX_TRAILS];	/* 0x20c the length of a segment of the trail */
	float uvOff[FXMATRIX_TRAILS];	/* 0x24c the scroll of the texture */
	CGameTrail trails[FXMATRIX_TRAILS];	/* 0x28c */
	vec3_t pts[FXMATRIX_TRAILS][5];	/* 0x2cc the control points of the curve of the ray */
	int kind[FXMATRIX_TRAILS];	/* 0x68c the path of the second part */
	float prog[FXMATRIX_TRAILS];	/* 0x6cc along the path, 1 at its end */
	float speed[FXMATRIX_TRAILS];	/* 0x70c */
	float spread[FXMATRIX_TRAILS];	/* 0x74c the end ahead of the start */
	int lastKind;			/* 0x78c -1, no path is taken twice in a row */
	void *flare;			/* 0x790 CGameLensFlare */
	byte started;			/* 0x794 */
} cObjElementFXSceneEnterMatrix;
SD_OFFSET(cObjElementFXSceneEnterMatrix, center, 0x180);
SD_OFFSET(cObjElementFXSceneEnterMatrix, trails, 0x28c);
SD_OFFSET(cObjElementFXSceneEnterMatrix, kind, 0x68c);
SD_OFFSET(cObjElementFXSceneEnterMatrix, lastKind, 0x78c);
SD_OFFSET(cObjElementFXSceneEnterMatrix, started, 0x794);

typedef struct cObjElementFXSceneHumanCD {
	void *vftable;			/* 0x000 */
	float t;			/* 0x004 */
	float tPrev;			/* 0x008 */
	cEntity *ent;			/* 0x00c */
	CGameParticles particles;		/* 0x010 Dummy01 */
	CGameParticles sparks;		/* 0x0c0 Dummy03 */
	vec3_t dummy;			/* 0x170 DM_Element_MentalControllerMainFX */
	float sparkT;			/* 0x17c */
	byte started;			/* 0x180 */
} cObjElementFXSceneHumanCD;
SD_OFFSET(cObjElementFXSceneHumanCD, sparks, 0xc0);
SD_OFFSET(cObjElementFXSceneHumanCD, started, 0x180);

typedef struct cObjElementFXSceneTheEndB {
	void *vftable;			/* 0x00 */
	float t;			/* 0x04 */
	float tPrev;			/* 0x08 */
	cEntity *ent;			/* 0x0c */
	cEntity *bike;			/* 0x10 TheEndBMoto */
	CGameParticles particles;	/* 0x14 Dummy01 */
	void *flare;			/* 0xc4 CGameLensFlare */
	byte started;			/* 0xc8 */
} cObjElementFXSceneTheEndB;
SD_OFFSET(cObjElementFXSceneTheEndB, flare, 0xc4);
SD_OFFSET(cObjElementFXSceneTheEndB, started, 0xc8);

typedef struct cObjElementFXSceneYakuzzi {
	void *vftable;			/* 0x00 */
	float t;			/* 0x04 */
	float tPrev;			/* 0x08 */
	cEntity *ent;			/* 0x0c */
	CGameParticles particles;	/* 0x10 Dummy01 */
	float emitT;			/* 0xc0 */
	vec3_t dummy;			/* 0xc4 DM_Element_TempleYakuzzi_00 */
	byte started;			/* 0xd0 */
} cObjElementFXSceneYakuzzi;
SD_OFFSET(cObjElementFXSceneYakuzzi, dummy, 0xc4);
SD_OFFSET(cObjElementFXSceneYakuzzi, started, 0xd0);

/* at the start of the FXVehicle.cpp code: the target entity name and its action of the
 * rays (31 characters at most), the height */
void cObjElementFXSceneDecontamination_SetNames(cObjElementFXSceneDecontamination *self,
	const char *target, const char *action);
void cObjElementFXSceneDecontamination_SetFC(cObjElementFXSceneDecontamination *self, float v);

/* the scene effect of the model of e (none for other models) */
void FXScene_Create(cEntity *e, const char *model);

/* the entity the element follows: an entity with life with a model (CHECKs) */
void cObjElementFXSceneBeatPickus_SetEntity(cObjElementFXSceneBeatPickus *self, cEntity *e);
void cObjElementFXSceneBillArrives_A_SetEntity(cObjElementFXSceneBillArrives_A *self, cEntity *e);
void cObjElementFXSceneBishops_SetEntity(cObjElementFXSceneBishops *self, cEntity *e);
void cObjElementFXSceneDecontamination_SetEntity(cObjElementFXSceneDecontamination *self, cEntity *e);
void cObjElementFXSceneEnterMatrix_SetEntity(cObjElementFXSceneEnterMatrix *self, cEntity *e);
/* also the main mental controller effect to its start (scale 1, no last controller) */
void cObjElementFXSceneHumanCD_SetEntity(cObjElementFXSceneHumanCD *self, cEntity *e);
void cObjElementFXSceneTheEndB_SetEntity(cObjElementFXSceneTheEndB *self, cEntity *e);
void cObjElementFXSceneYakuzzi_SetEntity(cObjElementFXSceneYakuzzi *self, cEntity *e);

/* Attach (vf03): the particles to the dummies of the debris model, the positions of the scene dummies */
void cObjElementFXSceneBeatPickus_Attach(cObjElementFXSceneBeatPickus *self, cDebris *d);
void cObjElementFXSceneBillArrives_A_Attach(cObjElementFXSceneBillArrives_A *self, cDebris *d);
void cObjElementFXSceneBishops_Attach(cObjElementFXSceneBishops *self, cDebris *d);
void cObjElementFXSceneDecontamination_Attach(cObjElementFXSceneDecontamination *self, cDebris *d);
void cObjElementFXSceneEnterMatrix_Attach(cObjElementFXSceneEnterMatrix *self, cDebris *d);
void cObjElementFXSceneHumanCD_Attach(cObjElementFXSceneHumanCD *self, cDebris *d);
void cObjElementFXSceneTheEndB_Attach(cObjElementFXSceneTheEndB *self, cDebris *d);
void cObjElementFXSceneYakuzzi_Attach(cObjElementFXSceneYakuzzi *self, cDebris *d);

/* Reset (vf04) */
void cObjElementFXSceneBeatPickus_Reset(cObjElementFXSceneBeatPickus *self, int arg);
void cObjElementFXSceneBillArrives_A_Reset(cObjElementFXSceneBillArrives_A *self, int arg);
void cObjElementFXSceneBishops_Reset(cObjElementFXSceneBishops *self, int arg);
void cObjElementFXSceneDecontamination_Reset(cObjElementFXSceneDecontamination *self, int arg);
void cObjElementFXSceneEnterMatrix_Reset(cObjElementFXSceneEnterMatrix *self, int arg);
void cObjElementFXSceneHumanCD_Reset(cObjElementFXSceneHumanCD *self, int arg);
void cObjElementFXSceneTheEndB_Reset(cObjElementFXSceneTheEndB *self, int arg);
void cObjElementFXSceneYakuzzi_Reset(cObjElementFXSceneYakuzzi *self, int arg);

/* Update (vf01): the life of the debris to 0 once the entity is gone */
void cObjElementFXSceneBeatPickus_Update(cObjElementFXSceneBeatPickus *self, cDebris *d, float dt);
void cObjElementFXSceneBillArrives_A_Update(cObjElementFXSceneBillArrives_A *self, cDebris *d,
	float dt);
void cObjElementFXSceneBishops_Update(cObjElementFXSceneBishops *self, cDebris *d, float dt);
void cObjElementFXSceneDecontamination_Update(cObjElementFXSceneDecontamination *self, cDebris *d,
	float dt);
void cObjElementFXSceneEnterMatrix_Update(cObjElementFXSceneEnterMatrix *self, cDebris *d, float dt);
void cObjElementFXSceneHumanCD_Update(cObjElementFXSceneHumanCD *self, cDebris *d, float dt);
void cObjElementFXSceneTheEndB_Update(cObjElementFXSceneTheEndB *self, cDebris *d, float dt);
void cObjElementFXSceneYakuzzi_Update(cObjElementFXSceneYakuzzi *self, cDebris *d, float dt);

#endif
