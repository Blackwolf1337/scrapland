/*
 * World/FX/FXCharActions.cpp of the game: the effects of the special actions of the characters, elements
 * (cObjElementFX) on the debris of a debris system like the ones of include/scrap/fxchar.h: the attacks and actions
 * of BankMaster, Banker, Berto, Betty, Bishop, D-Tritus, Functionary, Gear, Mercenary, Messenger, Police and
 * Sebastian. The element vtable: 0 Delete, 1 Update (debris, dt), 2 Alive (debris, sys), 3 Attach (debris),
 * 4 Reset. The native functions that start them (rva 0x4c1000..) set the entities with the setters below.
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
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	float emitTime;		/* 0xb8 the particles are emitted every 0.04 s of it */
	float time;			/* 0xbc since the start */
	CGameTrail trail;		/* 0xc0 */
	CGameTrail trail2;		/* 0xc4 */
	int nodeBaton;			/* 0xc8 DF_Batton of the character */
	float t;			/* 0xcc the time out of the action */
	byte started;			/* 0xd0 */
} cObjElementFXBankMasterAttack;

typedef struct cObjElementFXBankMasterSuction {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	cEntity *target;		/* 0x008 */
	CGameParticles particles;	/* 0x00c on Dummy01 */
	float emitTime;		/* 0x0bc the particles are emitted every 0.04 s of it */
	float time;			/* 0x0c0 since the start */
	float phase[FXBMSUCTION_POINTS];	/* 0x0c4 -0.9..0 */
	vec3_t dir[FXBMSUCTION_POINTS];	/* 0x0fc random directions of length 0.25..1 */
	float angle[FXBMSUCTION_POINTS];	/* 0x1a4 0..2 pi */
	CGameTrail trail;		/* 0x1dc only its Bezier helpers are used */
	int count;			/* 0x1e0 the points born since the last sound */
	int lastSound;			/* 0x1e4 */
	byte started;			/* 0x1e8 */
} cObjElementFXBankMasterSuction;

typedef struct cObjElementFXBankerSteal {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	cEntity *target;		/* 0x08 set by FXMisc_SetI8 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	int node;			/* 0xbc DF_BankerSteal of the character */
	float time;			/* 0xc0 since the last coin */
	int toggle;			/* 0xc4 a sound every second coin */
	int lastSound;			/* 0xc8 0..2, -1 */
	byte started;			/* 0xcc */
} cObjElementFXBankerSteal;

typedef struct cObjElementFXBertoAttack {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	float emitTime;		/* 0xb8 the particles are emitted every 0.04 s of it */
	float time;			/* 0xbc since the start */
	int unused_c0;			/* 0xc0 */
	int nodeGlass;		/* 0xc4 R_Berto_TroncoInf-cristal */
	byte started;			/* 0xc8 */
} cObjElementFXBertoAttack;

typedef struct cObjElementFXBertoFlash {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	cEntity *target;		/* 0x008 */
	CGameParticles particles;	/* 0x00c on Dummy01 */
	float emitTime;		/* 0x0bc the particles are emitted every 0.04 s of it */
	float time;			/* 0x0c0 since the start */
	vec3_t dir[FXBERTOFLASH_POINTS];	/* 0x0c4 length 10..14 */
	vec3_t dir2[FXBERTOFLASH_POINTS];	/* 0x394 length 0.65..0.85 of the target radius */
	CGameTrail trails[FXBERTOFLASH_TRAILS];	/* 0x664 */
	byte started;			/* 0x674 */
} cObjElementFXBertoFlash;

typedef struct cObjElementFXBettyAttack {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	CGameParticles particles2;	/* 0x0b8 on "Dummy02 (+dudv_pass)" if the model has it */
	float t2;			/* 0x168 since the start (not read) */
	float time;			/* 0x16c since the start */
	float lastTime;			/* 0x170 time of the last Update */
	vec3_t pos;			/* 0x174 the center of the attack (Emit) */
	CGameTrail trails[2];		/* 0x180 */
	float ring;			/* 0x188 the radius of the ring that grows */
	CGameTrail trails2[FXBETTYATTACK_TRAILS];	/* 0x18c */
	CGameTrail trail;		/* 0x19c */
	void *flare;			/* 0x1a0 CGameLensFlare */
	byte started;			/* 0x1a4 */
} cObjElementFXBettyAttack;

/* BettyKill and DTritusKill */
typedef struct cObjElementFXKill {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	float emitTime;		/* 0x0b8 the particles are emitted every 0.04 s of it */
	float time;			/* 0x0bc since the start */
	int vertex[FXKILL_POINTS];	/* 0x0c0 random vertices of the model */
	vec3_t point[FXKILL_POINTS];	/* 0x1ec the vertices in the model space (unrotated) */
	float scale[FXKILL_POINTS];	/* 0x570 15..75 times the radius / 90 */
	float len[FXKILL_POINTS];	/* 0x69c of the beams, approaches lenTarget */
	float lenTarget[FXKILL_POINTS];	/* 0x7c8 525..625 */
	CGameTrail trails[FXKILL_TRAILS];	/* 0x8f4 */
	vec3_t angles;			/* 0x908 the turn of the beams */
	byte started;			/* 0x914 */
} cObjElementFXKill;
typedef cObjElementFXKill cObjElementFXBettyKill;
typedef cObjElementFXKill cObjElementFXDTritusKill;

typedef struct cObjElementFXBishopAttack {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	byte pad_08[4];
	float emitTime;		/* 0x0c the particles are emitted every 0.02 s of it */
	float time;			/* 0x10 since the start */
	CGameParticles particles;	/* 0x14 on Dummy01 */
	CGameTrail trails[FXBISHOPATTACK_TRAILS];	/* 0xc4 */
	vec3_t from;			/* 0xd4 */
	vec3_t to;			/* 0xe0 */
	vec3_t dir;			/* 0xec from -> to normalized */
	byte started;			/* 0xf8 */
} cObjElementFXBishopAttack;

typedef struct cObjElementFXBishopSellLife {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *attacker;		/* 0x004 */
	cEntity *attacked;		/* 0x008 */
	CGameParticles glow;		/* 0x00c on Dummy01 */
	CGameParticles dataCodec;	/* 0x0bc on Dummy02 */
	float time;			/* 0x16c since the start */
	float emitTime;		/* 0x170 the particles are emitted every 0.02 s of it */
	int nodeHand[2];		/* 0x174 R_Bishop_Mano_Izq, R_Bishop_Mano_Der */
	byte started;			/* 0x17c */
} cObjElementFXBishopSellLife;

typedef struct cObjElementFXDTritusAttack {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x0004 */
	CGameParticles particles;	/* 0x0008 on Dummy01 */
	float emitTime;		/* 0x00b8 the particles are emitted every 0.04 s of it */
	float time;			/* 0x00bc since the start */
	int nodeMaster;			/* 0x00c0 R_Detritus_Tronco */
	int vertex[FXDTRITUSATTACK_POINTS];	/* 0x00c4 */
	vec3_t point[FXDTRITUSATTACK_POINTS];	/* 0x031c relative to the trunk, unrotated */
	float width[FXDTRITUSATTACK_POINTS];	/* 0x0a24 of the beams, 40..55 */
	float len[FXDTRITUSATTACK_POINTS];	/* 0x0c7c approaches lenTarget, starts at 25..35 */
	float lenTarget[FXDTRITUSATTACK_POINTS];	/* 0x0ed4 375..425 */
	CGameTrail trails[FXDTRITUSATTACK_TRAILS];	/* 0x112c */
	byte started;			/* 0x1154 */
} cObjElementFXDTritusAttack;

typedef struct cObjElementFXFunctionarySpecialAction {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 a WalkChar */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	float emitTime;		/* 0x0b8 the particles are emitted every 0.04 s of it */
	float time;			/* 0x0bc since the start */
	byte pad_c0[4];
	CGameTrail trails[FXFUNCTIONARY_TRAILS];	/* 0x0c4 */
	vec3_t pos[FXFUNCTIONARY_TRAILS];	/* 0x0fc the vertices of g_FXFunctionaryVertex */
	float alpha;			/* 0x1a4 the model effect, up with the speed of the character */
	byte started;			/* 0x1a8 */
} cObjElementFXFunctionarySpecialAction;

/* the electric ball of Gear: lightning bolts to the world or to entities near it, puffs, sparks and a tail */
typedef struct cObjElementFXGearMissile {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *entity;		/* 0x004 the missile */
	int node;			/* 0x008 Dummy01 */
	CGameParticles particles;	/* 0x00c on Dummy01: the ball, 0..3 the glows */
	int nodeSparks;			/* 0x0bc Dummy02 */
	CGameParticles sparks;		/* 0x0c0 on Dummy02: 3 i .. 3 i + 2 the end of bolt i */
	float emitTime;			/* 0x170 since the last puffs */
	float fadeLife;			/* 0x174 0.5: the life of the debris from which it fades */
	float time;			/* 0x178 since the start */
	vec3_t lastPos;			/* 0x17c the position of the last Update with the missile on */
	CGameTrail trail;		/* 0x188 the tail */
	float trailStep;		/* 0x18c since the last point of the tail */
	float boltTime[FXGEARMISSILE_TRAILS];	/* 0x190 the time left of bolt i, -1 */
	CGameTrail trails[FXGEARMISSILE_TRAILS];	/* 0x1b8 */
	vec3_t pts[FXGEARMISSILE_TRAILS][FXGEARMISSILE_SLOTS];	/* 0x1e0 the FXGEARMISSILE_POINTS points of bolt i */
	float boltScale[FXGEARMISSILE_TRAILS];	/* 0x690 0.5: the width of the trail and the end glows */
	byte started;			/* 0x6b8 */
} cObjElementFXGearMissile;

typedef struct cObjElementFXMercenaryDrink {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	float emitTime;		/* 0xb8 the particles are emitted every 0.04 s of it */
	float time;			/* 0xbc since the start */
	vec3_t offset;			/* 0xc0 from the mouth of the jar, in its frame */
	int nodeJar;			/* 0xcc OB_JarraMercurio_MercA */
	byte started;			/* 0xd0 */
} cObjElementFXMercenaryDrink;

typedef struct cObjElementFXMessengerAttack {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	byte pad_08[4];
	float emitTime;		/* 0x00c the particles are emitted every 0.02 s of it */
	float time;			/* 0x010 since the start */
	CGameParticles particles;	/* 0x014 on Dummy01 */
	vec3_t from;			/* 0x0c4 */
	vec3_t to;			/* 0x0d0 */
	vec3_t dir;			/* 0x0dc */
	CGameTrail trails[FXMESSENGERATTACK_TRAILS];	/* 0x0e8 */
	/* 0x100 the ray from .. to (the middle points moved at random), then the 5 branches off it */
	vec3_t lines[1 + FXMESSENGER_LINES][FXMESSENGER_POINTS];
	float width[1 + FXMESSENGER_LINES];	/* 0x3d0 */
	byte started;			/* 0x3e8 */
} cObjElementFXMessengerAttack;

typedef struct cObjElementFXMessengerTakeDataPack {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *owner;			/* 0x004 a WalkChar */
	cEntity *item;			/* 0x008 an Obj */
	float time;			/* 0x00c since the start */
	CGameParticles particles;	/* 0x010 on Dummy01 */
	CGameTrail trails[FXDATAPACK_TRAILS];	/* 0x0c0 */
	vec3_t points[FXDATAPACK_TRAILS][FXDATAPACK_POINTS];	/* 0x0d0 */
	float width[FXDATAPACK_TRAILS];	/* 0x160 0.6..1 */
	byte started;			/* 0x170 */
} cObjElementFXMessengerTakeDataPack;

typedef struct cObjElementFXPoliceGearConversion {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	float t2;			/* 0x008 since the start (not read) */
	float time;			/* 0x00c since the start */
	float lastTime;			/* 0x010 time of the last Update */
	byte pad_14[4];
	CGameParticles particles;	/* 0x018 on Dummy01 */
	int nodeRoot;			/* 0x0c8 R_Police-NoArmr_tronco-inferior */
	float smokeT;			/* 0x0cc since the last smoke ring */
	byte pad_d0[4];
	CGameParticles particles2;	/* 0x0d4 */
	byte pad_184[0x1a4 - 0x184];
	vec3_t bubblePos[FXGEAR_BUBBLES];	/* 0x1a4 the bubbles of the fusion around the waist */
	float bubbleH[FXGEAR_BUBBLES];		/* 0x474 their height */
	float bubbleVel[FXGEAR_BUBBLES];	/* 0x564 */
	float bubbleAcc[FXGEAR_BUBBLES];	/* 0x654 */
	float bubbleMax[FXGEAR_BUBBLES];	/* 0x744 */
	CGameTrail trails[FXGEAR_TRAILS];	/* 0x834 */
	int count;			/* 0x844 the bubbles made */
	float emitT;			/* 0x848 since the last smoke */
	CGameModel armor[FXGEAR_ARMORS];	/* 0x84c Gear_Armor1..3 */
	int anim[FXGEAR_ARMORS];	/* 0x8a0 Gear_ArmorFusion1..3 */
	int numPieces[FXGEAR_ARMORS];	/* 0x8ac */
	int piece[FXGEAR_ARMORS][FXGEAR_PIECES];	/* 0x8b8 nodes OB_PiezasGear_nn of the armors, -1 */
	byte pieceOff[FXGEAR_ARMORS][FXGEAR_PIECES];	/* 0x96c */
	byte pad_999[3];
	float pieceTime[FXGEAR_ARMORS][FXGEAR_PIECES];	/* 0x99c the times the pieces fall off */
	byte started;			/* 0xa50 */
} cObjElementFXPoliceGearConversion;

typedef struct cObjElementFXPoliceSteal {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	cEntity *target;		/* 0x08 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	float emitTime;		/* 0xbc the particles are emitted every 0.04 s of it */
	int unused_c0;			/* 0xc0 */
	int unused_c4;			/* 0xc4 */
	byte started;			/* 0xc8 */
} cObjElementFXPoliceSteal;

typedef struct cObjElementFXSebastianFlash {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	float emitTime;		/* 0x0b8 the particles are emitted every 0.04 s of it */
	float time;			/* 0x0bc since the start */
	vec3_t dir[FXSEBASTIAN_POINTS];	/* 0x0c0 length 0..3 */
	vec3_t dir2[FXSEBASTIAN_POINTS];	/* 0x444 length 15..80 */
	CGameTrail trails[FXSEBASTIAN_TRAILS];	/* 0x7c8 */
	byte started;			/* 0x7dc */
} cObjElementFXSebastianFlash;

/* the constructors, destructors and deleting destructors (vf00) */
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
