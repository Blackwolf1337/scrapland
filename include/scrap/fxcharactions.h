/*
 * World/FX/FXCharActions.cpp of the game: the effects of the special actions of the characters, elements
 * (cObjElementFX) on the debris of a debris system like the ones of include/scrap/fxchar.h: the attacks and actions
 * of BankMaster, Banker, Berto, Betty, Bishop, D-Tritus, Functionary, Gear, Mercenary, Messenger, Police and
 * Sebastian. The element vtable: 0 Delete, 1 Update (debris, dt), 2 Alive (debris, sys), 3 Attach (debris),
 * 4 Reset. The native functions that start them (..) set the entities with the setters below.
 */
#ifndef SCRAP_FXCHARACTIONS_H
#define SCRAP_FXCHARACTIONS_H

#include "scrap/gvar.h"
#include "scrap/fxchar.h"
#include "scrap/fxmisc.h"
#include "scrap/timer.h"

#define FXBMSUCTION_POINTS 14
#define FXBERTOFLASH_POINTS 60
#define FXBERTOFLASH_TRAILS 4
#define FXBETTYATTACK_TRAILS 4
#define FXKILL_POINTS 75		/* BettyKill and DTritusKill */
#define FXKILL_TRAILS 5
#define FXBISHOPATTACK_TRAILS 4
#define FXDTRITUSATTACK_POINTS 150
#define FXDTRITUSATTACK_TRAILS 10
#define FXFUNCTIONARY_TRAILS 14
#define FXGEARMISSILE_TRAILS 10
#define FXGEARMISSILE_SLOTS 10
#define FXGEARMISSILE_POINTS 6
#define FXMESSENGERATTACK_TRAILS 6
#define FXMESSENGER_POINTS 10		/* per line */
#define FXMESSENGER_LINES 5
#define FXDATAPACK_TRAILS 4
#define FXDATAPACK_POINTS 3		/* per trail */
#define FXGEAR_ARMORS 3
#define FXGEAR_PIECES 15		/* the node slots per armor */
#define FXGEAR_BUBBLES 60
#define FXGEAR_TRAILS 4
#define FXSEBASTIAN_POINTS 75
#define FXSEBASTIAN_TRAILS 5

/* the timer of a character (cWalkChar + 0x110) */
/* a float of the Police character: its center is this much above the feet */

typedef struct cObjElementFXBankMasterAttack {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	CGameTrail trail;
	CGameTrail trail2;
	int nodeBaton;			/* DF_Batton of the character */
	float t;			/* the time out of the action */
	byte started;
} cObjElementFXBankMasterAttack;

typedef struct cObjElementFXBankMasterSuction {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	cEntity *target;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	float phase[FXBMSUCTION_POINTS];	/* -0.9..0 */
	vec3_t dir[FXBMSUCTION_POINTS];	/* random directions of length 0.25..1 */
	float angle[FXBMSUCTION_POINTS];	/* 0..2 pi */
	CGameTrail trail;		/* only its Bezier helpers are used */
	int count;			/* the points born since the last sound */
	int lastSound;
	byte started;
} cObjElementFXBankMasterSuction;

typedef struct cObjElementFXBankerSteal {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	cEntity *target;		/* set by FXMisc_SetI8 */
	CGameParticles particles;	/* on Dummy01 */
	int node;			/* DF_BankerSteal of the character */
	float time;			/* since the last coin */
	int toggle;			/* a sound every second coin */
	int lastSound;			/* 0..2, -1 */
	byte started;
} cObjElementFXBankerSteal;

typedef struct cObjElementFXBertoAttack {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	int unused_c0;
	int nodeGlass;		/* R_Berto_TroncoInf-cristal */
	byte started;
} cObjElementFXBertoAttack;

typedef struct cObjElementFXBertoFlash {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	cEntity *target;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	vec3_t dir[FXBERTOFLASH_POINTS];	/* length 10..14 */
	vec3_t dir2[FXBERTOFLASH_POINTS];	/* length 0.65..0.85 of the target radius */
	CGameTrail trails[FXBERTOFLASH_TRAILS];
	byte started;
} cObjElementFXBertoFlash;

typedef struct cObjElementFXBettyAttack {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	CGameParticles particles2;	/* on "Dummy02 (+dudv_pass)" if the model has it */
	float t2;			/* since the start (not read) */
	float time;			/* since the start */
	float lastTime;			/* time of the last Update */
	vec3_t pos;			/* the center of the attack (Emit) */
	CGameTrail trails[2];
	float ring;			/* the radius of the ring that grows */
	CGameTrail trails2[FXBETTYATTACK_TRAILS];
	CGameTrail trail;
	void *flare;			/* CGameLensFlare */
	byte started;
} cObjElementFXBettyAttack;

/* BettyKill and DTritusKill */
typedef struct cObjElementFXKill {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	int vertex[FXKILL_POINTS];	/* random vertices of the model */
	vec3_t point[FXKILL_POINTS];	/* the vertices in the model space (unrotated) */
	float scale[FXKILL_POINTS];	/* 15..75 times the radius / 90 */
	float len[FXKILL_POINTS];	/* of the beams, approaches lenTarget */
	float lenTarget[FXKILL_POINTS];	/* 525..625 */
	CGameTrail trails[FXKILL_TRAILS];
	vec3_t angles;			/* the turn of the beams */
	byte started;
} cObjElementFXKill;
typedef cObjElementFXKill cObjElementFXBettyKill;
typedef cObjElementFXKill cObjElementFXDTritusKill;

typedef struct cObjElementFXBishopAttack {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	byte pad_08[4];
	float emitTime;		/* the particles are emitted every 0.02 s of it */
	float time;			/* since the start */
	CGameParticles particles;	/* on Dummy01 */
	CGameTrail trails[FXBISHOPATTACK_TRAILS];
	vec3_t from;
	vec3_t to;
	vec3_t dir;			/* from -> to normalized */
	byte started;
} cObjElementFXBishopAttack;

typedef struct cObjElementFXBishopSellLife {
	const cObjElementFXVtbl *vftable;
	cEntity *attacker;
	cEntity *attacked;
	CGameParticles glow;		/* on Dummy01 */
	CGameParticles dataCodec;	/* on Dummy02 */
	float time;			/* since the start */
	float emitTime;		/* the particles are emitted every 0.02 s of it */
	int nodeHand[2];		/* R_Bishop_Mano_Izq, R_Bishop_Mano_Der */
	byte started;
} cObjElementFXBishopSellLife;

typedef struct cObjElementFXDTritusAttack {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	int nodeMaster;			/* R_Detritus_Tronco */
	int vertex[FXDTRITUSATTACK_POINTS];
	vec3_t point[FXDTRITUSATTACK_POINTS];	/* relative to the trunk, unrotated */
	float width[FXDTRITUSATTACK_POINTS];	/* of the beams, 40..55 */
	float len[FXDTRITUSATTACK_POINTS];	/* approaches lenTarget, starts at 25..35 */
	float lenTarget[FXDTRITUSATTACK_POINTS];	/* 375..425 */
	CGameTrail trails[FXDTRITUSATTACK_TRAILS];
	byte started;
} cObjElementFXDTritusAttack;

typedef struct cObjElementFXFunctionarySpecialAction {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;			/* a WalkChar */
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	byte pad_c0[4];
	CGameTrail trails[FXFUNCTIONARY_TRAILS];
	vec3_t pos[FXFUNCTIONARY_TRAILS];	/* the vertices of g_FXFunctionaryVertex */
	float alpha;			/* the model effect, up with the speed of the character */
	byte started;
} cObjElementFXFunctionarySpecialAction;

/* the electric ball of Gear: lightning bolts to the world or to entities near it, puffs, sparks and a tail */
typedef struct cObjElementFXGearMissile {
	const cObjElementFXVtbl *vftable;
	cEntity *entity;		/* the missile */
	int node;			/* Dummy01 */
	CGameParticles particles;	/* on Dummy01: the ball, 0..3 the glows */
	int nodeSparks;			/* Dummy02 */
	CGameParticles sparks;		/* on Dummy02: 3 i .. 3 i + 2 the end of bolt i */
	float emitTime;			/* since the last puffs */
	float fadeLife;			/* 0.5: the life of the debris from which it fades */
	float time;			/* since the start */
	vec3_t lastPos;			/* the position of the last Update with the missile on */
	CGameTrail trail;		/* the tail */
	float trailStep;		/* since the last point of the tail */
	float boltTime[FXGEARMISSILE_TRAILS];	/* the time left of bolt i, -1 */
	CGameTrail trails[FXGEARMISSILE_TRAILS];
	vec3_t pts[FXGEARMISSILE_TRAILS][FXGEARMISSILE_SLOTS];	/* the FXGEARMISSILE_POINTS points of bolt i */
	float boltScale[FXGEARMISSILE_TRAILS];	/* 0.5: the width of the trail and the end glows */
	byte started;
} cObjElementFXGearMissile;

typedef struct cObjElementFXMercenaryDrink {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	vec3_t offset;			/* from the mouth of the jar, in its frame */
	int nodeJar;			/* OB_JarraMercurio_MercA */
	byte started;
} cObjElementFXMercenaryDrink;

typedef struct cObjElementFXMessengerAttack {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	byte pad_08[4];
	float emitTime;		/* the particles are emitted every 0.02 s of it */
	float time;			/* since the start */
	CGameParticles particles;	/* on Dummy01 */
	vec3_t from;
	vec3_t to;
	vec3_t dir;
	CGameTrail trails[FXMESSENGERATTACK_TRAILS];
	/* the ray from .. to (the middle points moved at random), then the 5 branches off it */
	vec3_t lines[1 + FXMESSENGER_LINES][FXMESSENGER_POINTS];
	float width[1 + FXMESSENGER_LINES];
	byte started;
} cObjElementFXMessengerAttack;

typedef struct cObjElementFXMessengerTakeDataPack {
	const cObjElementFXVtbl *vftable;
	cEntity *owner;			/* a WalkChar */
	cEntity *item;			/* an Obj */
	float time;			/* since the start */
	CGameParticles particles;	/* on Dummy01 */
	CGameTrail trails[FXDATAPACK_TRAILS];
	vec3_t points[FXDATAPACK_TRAILS][FXDATAPACK_POINTS];
	float width[FXDATAPACK_TRAILS];	/* 0.6..1 */
	byte started;
} cObjElementFXMessengerTakeDataPack;

typedef struct cObjElementFXPoliceGearConversion {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	float t2;			/* since the start (not read) */
	float time;			/* since the start */
	float lastTime;			/* time of the last Update */
	byte pad_14[4];
	CGameParticles particles;	/* on Dummy01 */
	int nodeRoot;			/* R_Police-NoArmr_tronco-inferior */
	float smokeT;			/* since the last smoke ring */
	byte pad_d0[4];
	CGameParticles particles2;
	vec3_t bubblePos[FXGEAR_BUBBLES];	/* the bubbles of the fusion around the waist */
	float bubbleH[FXGEAR_BUBBLES];		/* their height */
	float bubbleVel[FXGEAR_BUBBLES];
	float bubbleAcc[FXGEAR_BUBBLES];
	float bubbleMax[FXGEAR_BUBBLES];
	CGameTrail trails[FXGEAR_TRAILS];
	int count;			/* the bubbles made */
	float emitT;			/* since the last smoke */
	CGameModel armor[FXGEAR_ARMORS];	/* Gear_Armor1..3 */
	int anim[FXGEAR_ARMORS];	/* Gear_ArmorFusion1..3 */
	int numPieces[FXGEAR_ARMORS];
	int piece[FXGEAR_ARMORS][FXGEAR_PIECES];	/* nodes OB_PiezasGear_nn of the armors, -1 */
	byte pieceOff[FXGEAR_ARMORS][FXGEAR_PIECES];
	byte pad_999[3];
	float pieceTime[FXGEAR_ARMORS][FXGEAR_PIECES];	/* the times the pieces fall off */
	byte started;
} cObjElementFXPoliceGearConversion;

typedef struct cObjElementFXPoliceSteal {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	cEntity *target;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	int unused_c0;
	int unused_c4;
	byte started;
} cObjElementFXPoliceSteal;

typedef struct cObjElementFXSebastianFlash {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	vec3_t dir[FXSEBASTIAN_POINTS];	/* length 0..3 */
	vec3_t dir2[FXSEBASTIAN_POINTS];	/* length 15..80 */
	CGameTrail trails[FXSEBASTIAN_TRAILS];
	byte started;
} cObjElementFXSebastianFlash;

/* the constructors, destructors and the Free slots */
void *cObjElementFXBankMasterAttack_ctor(void *self);
void *cObjElementFXBankMasterSuction_ctor(void *self);
void *cObjElementFXBankerSteal_ctor(void *self);
void *cObjElementFXBertoAttack_ctor(void *self);
void *cObjElementFXBertoFlash_ctor(void *self);
void *cObjElementFXBettyAttack_ctor(void *self);
cObjElementFXBettyKill *cObjElementFXBettyKill_ctor(cObjElementFXBettyKill *self);
void *cObjElementFXBishopAttack_ctor(void *self);
void *cObjElementFXBishopSellLife_ctor(void *self);
void *cObjElementFXDTritusAttack_ctor(void *self);
cObjElementFXDTritusKill *cObjElementFXDTritusKill_ctor(cObjElementFXDTritusKill *self);
void *cObjElementFXFunctionarySpecialAction_ctor(void *self);
void *cObjElementFXGearMissile_ctor(void *self);
void *cObjElementFXMercenaryDrink_ctor(void *self);
void *cObjElementFXMessengerAttack_ctor(void *self);
void *cObjElementFXMessengerTakeDataPack_ctor(void *self);
void *cObjElementFXPoliceGearConversion_ctor(void *self);
void *cObjElementFXPoliceSteal_ctor(void *self);
void *cObjElementFXSebastianFlash_ctor(void *self);

/* the setters and starts; the entity at 0x4 of PoliceSteal, BankMasterSuction, BertoFlash, BettyAttack,
 * BishopAttack, MessengerAttack and SebastianFlash is FXMisc_SetEntity, the int at 0x8 of BankerSteal FXMisc_SetI8 */
/* the entity at 0x8 (PoliceSteal, BankMasterSuction, BertoFlash), its model a CHECK */
void FXCharActions_SetEntity8(void *self, cEntity *e);
void cObjElementFXBankerSteal_SetEntity(cObjElementFXBankerSteal *self, cEntity *e);
/* cObjElementFXFunctionarySpecialAction_Start (the trails from 14 vertices of the WalkChar) is in fxchar.h */
void cObjElementFXBishopSellLife_SetAttacked(cObjElementFXBishopSellLife *self, cEntity *e);
void cObjElementFXBishopSellLife_SetAttacker(cObjElementFXBishopSellLife *self, cEntity *e);
void cObjElementFXMessengerTakeDataPack_SetItem(cObjElementFXMessengerTakeDataPack *self, cEntity *e);
void cObjElementFXMessengerTakeDataPack_SetOwner(cObjElementFXMessengerTakeDataPack *self, cEntity *e);
void cObjElementFXMercenaryDrink_SetC0(cObjElementFXMercenaryDrink *self, float a, float b, float c);
void cObjElementFXMercenaryDrink_Start(cObjElementFXMercenaryDrink *self, cEntity *e);
void cObjElementFXBankMasterAttack_Start(cObjElementFXBankMasterAttack *self, cEntity *e);
void cObjElementFXBertoAttack_Start(cObjElementFXBertoAttack *self, cEntity *e);
/* BettyKill and DTritusKill: 75 random vertices of the model of e with random factors */
void cObjElementFXKill_Start(cObjElementFXKill *self, cEntity *e);
void cObjElementFXKill_SetAngles(cObjElementFXKill *self, float x, float y, float z);
void cObjElementFXDTritusAttack_Start(cObjElementFXDTritusAttack *self, cEntity *e);
void cObjElementFXBishopAttack_SetLine(cObjElementFXBishopAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz);
/* the shot of the Betty attack at x y z: a sphere of particles */
void cObjElementFXBettyAttack_Emit(cObjElementFXBettyAttack *self, float x, float y, float z);
/* the trails from the owner to the item, 3 points each */
void cObjElementFXMessengerTakeDataPack_Start(cObjElementFXMessengerTakeDataPack *self);
/* the lightning from .. to and 5 branches off its Bezier curve (stopped by the floor) */
void cObjElementFXMessengerAttack_SetLine(cObjElementFXMessengerAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz);
/* the armors at the Police character e and the smoke and sparks at its feet if it is visible */
void cObjElementFXPoliceGearConversion_Start(cObjElementFXPoliceGearConversion *self, cEntity *e);
/* GearMissile: 8 Updates of 0 seconds through the vtable */
void cObjElementFXGearMissile_Run(void *self, cDebris *d);
/* the random directions of the start */
void cObjElementFXBankMasterSuction_RandomPoints(cObjElementFXBankMasterSuction *self);
void cObjElementFXBertoFlash_RandomPoints(cObjElementFXBertoFlash *self);
void cObjElementFXSebastianFlash_RandomPoints(cObjElementFXSebastianFlash *self);

#endif
