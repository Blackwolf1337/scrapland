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

/* the vtables */
extern const cObjElementFXVtbl cObjElementFXBankMasterAttack_vftable;
#define VT_FXBANKMASTERATTACK (&cObjElementFXBankMasterAttack_vftable)
extern const cObjElementFXVtbl cObjElementFXBankMasterSuction_vftable;
#define VT_FXBANKMASTERSUCTION (&cObjElementFXBankMasterSuction_vftable)
extern const cObjElementFXVtbl cObjElementFXBankerSteal_vftable;
#define VT_FXBANKERSTEAL (&cObjElementFXBankerSteal_vftable)
extern const cObjElementFXVtbl cObjElementFXBertoAttack_vftable;
#define VT_FXBERTOATTACK (&cObjElementFXBertoAttack_vftable)
extern const cObjElementFXVtbl cObjElementFXBertoFlash_vftable;
#define VT_FXBERTOFLASH (&cObjElementFXBertoFlash_vftable)
extern const cObjElementFXVtbl cObjElementFXBettyAttack_vftable;
#define VT_FXBETTYATTACK (&cObjElementFXBettyAttack_vftable)
extern const cObjElementFXVtbl cObjElementFXBettyKill_vftable;
#define VT_FXBETTYKILL (&cObjElementFXBettyKill_vftable)
extern const cObjElementFXVtbl cObjElementFXBishopAttack_vftable;
#define VT_FXBISHOPATTACK (&cObjElementFXBishopAttack_vftable)
extern const cObjElementFXVtbl cObjElementFXBishopSellLife_vftable;
#define VT_FXBISHOPSELLLIFE (&cObjElementFXBishopSellLife_vftable)
extern const cObjElementFXVtbl cObjElementFXDTritusAttack_vftable;
#define VT_FXDTRITUSATTACK (&cObjElementFXDTritusAttack_vftable)
extern const cObjElementFXVtbl cObjElementFXDTritusKill_vftable;
#define VT_FXDTRITUSKILL (&cObjElementFXDTritusKill_vftable)
extern const cObjElementFXVtbl cObjElementFXFunctionarySpecialAction_vftable;
#define VT_FXFUNCTIONARYSPECIALACTION (&cObjElementFXFunctionarySpecialAction_vftable)
extern const cObjElementFXVtbl cObjElementFXGearMissile_vftable;
#define VT_FXGEARMISSILE (&cObjElementFXGearMissile_vftable)
extern const cObjElementFXVtbl cObjElementFXMercenaryDrink_vftable;
#define VT_FXMERCENARYDRINK (&cObjElementFXMercenaryDrink_vftable)
extern const cObjElementFXVtbl cObjElementFXMessengerAttack_vftable;
#define VT_FXMESSENGERATTACK (&cObjElementFXMessengerAttack_vftable)
extern const cObjElementFXVtbl cObjElementFXMessengerTakeDataPack_vftable;
#define VT_FXMESSENGERTAKEDATAPACK (&cObjElementFXMessengerTakeDataPack_vftable)
extern const cObjElementFXVtbl cObjElementFXPoliceGearConversion_vftable;
#define VT_FXPOLICEGEARCONVERSION (&cObjElementFXPoliceGearConversion_vftable)
extern const cObjElementFXVtbl cObjElementFXPoliceSteal_vftable;
#define VT_FXPOLICESTEAL (&cObjElementFXPoliceSteal_vftable)
extern const cObjElementFXVtbl cObjElementFXSebastianFlash_vftable;
#define VT_FXSEBASTIANFLASH (&cObjElementFXSebastianFlash_vftable)

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

/* the vertices of the Functionary model the special action starts from, found once (rva 0x466fc0..) */
extern byte g_FXFunctionaryInit;
extern int g_FXFunctionaryVertex[14];		/* [14] */
extern uint32_t g_FXFunctionaryColor[14];		/* [14] */

/* the engine variables (Gvars) */
extern cGVar GVar_BankerSteal1Sound;
#define g_BankerSteal1Sound (GVar_BankerSteal1Sound.value.s)
extern cGVar GVar_BankerSteal2Sound;
#define g_BankerSteal2Sound (GVar_BankerSteal2Sound.value.s)
extern cGVar GVar_BankerSteal3Sound;
#define g_BankerSteal3Sound (GVar_BankerSteal3Sound.value.s)
extern cGVar GVar_BankerStealSoundVol;
#define g_BankerStealSoundVol (GVar_BankerStealSoundVol.value.f)
extern cGVar GVar_BankerStealPosX;
#define g_BankerStealPosX (GVar_BankerStealPosX.value.f)
extern cGVar GVar_BankerStealPosY;
#define g_BankerStealPosY (GVar_BankerStealPosY.value.f)
extern cGVar GVar_BankerStealPosZ;
#define g_BankerStealPosZ (GVar_BankerStealPosZ.value.f)
extern cGVar GVar_BankMasterBatonPosX;
#define g_BankMasterBatonPosX (GVar_BankMasterBatonPosX.value.f)
extern cGVar GVar_BankMasterBatonPosY;
#define g_BankMasterBatonPosY (GVar_BankMasterBatonPosY.value.f)
extern cGVar GVar_BankMasterBatonPosZ;
#define g_BankMasterBatonPosZ (GVar_BankMasterBatonPosZ.value.f)

/* the timer of a character (cWalkChar + 0x110) */
/* a float of the Police character: its center is this much above the feet */

typedef struct cObjElementFXBankMasterAttack {
	void *vftable;			/* 0x00 */
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
SD_OFFSET(cObjElementFXBankMasterAttack, trail, 0xc0);
SD_OFFSET(cObjElementFXBankMasterAttack, started, 0xd0);
SD_SIZE(cObjElementFXBankMasterAttack, 0xd4);

typedef struct cObjElementFXBankMasterSuction {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXBankMasterSuction, dir, 0xfc);
SD_OFFSET(cObjElementFXBankMasterSuction, trail, 0x1dc);
SD_SIZE(cObjElementFXBankMasterSuction, 0x1ec);

typedef struct cObjElementFXBankerSteal {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	cEntity *target;		/* 0x08 set by FXMisc_SetI8 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	int node;			/* 0xbc DF_BankerSteal of the character */
	float time;			/* 0xc0 since the last coin */
	int toggle;			/* 0xc4 a sound every second coin */
	int lastSound;			/* 0xc8 0..2, -1 */
	byte started;			/* 0xcc */
} cObjElementFXBankerSteal;
SD_OFFSET(cObjElementFXBankerSteal, node, 0xbc);
SD_SIZE(cObjElementFXBankerSteal, 0xd0);

typedef struct cObjElementFXBertoAttack {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	float emitTime;		/* 0xb8 the particles are emitted every 0.04 s of it */
	float time;			/* 0xbc since the start */
	int unused_c0;			/* 0xc0 */
	int nodeGlass;		/* 0xc4 R_Berto_TroncoInf-cristal */
	byte started;			/* 0xc8 */
} cObjElementFXBertoAttack;
SD_OFFSET(cObjElementFXBertoAttack, nodeGlass, 0xc4);
SD_SIZE(cObjElementFXBertoAttack, 0xcc);

typedef struct cObjElementFXBertoFlash {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXBertoFlash, dir2, 0x394);
SD_OFFSET(cObjElementFXBertoFlash, trails, 0x664);
SD_SIZE(cObjElementFXBertoFlash, 0x678);

typedef struct cObjElementFXBettyAttack {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXBettyAttack, trails, 0x180);
SD_OFFSET(cObjElementFXBettyAttack, flare, 0x1a0);
SD_SIZE(cObjElementFXBettyAttack, 0x1a8);

/* BettyKill and DTritusKill */
typedef struct cObjElementFXKill {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXKill, point, 0x1ec);
SD_OFFSET(cObjElementFXKill, lenTarget, 0x7c8);
SD_OFFSET(cObjElementFXKill, trails, 0x8f4);
SD_SIZE(cObjElementFXKill, 0x918);
typedef cObjElementFXKill cObjElementFXBettyKill;
typedef cObjElementFXKill cObjElementFXDTritusKill;

typedef struct cObjElementFXBishopAttack {
	void *vftable;			/* 0x00 */
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
SD_OFFSET(cObjElementFXBishopAttack, trails, 0xc4);
SD_OFFSET(cObjElementFXBishopAttack, dir, 0xec);
SD_SIZE(cObjElementFXBishopAttack, 0xfc);

typedef struct cObjElementFXBishopSellLife {
	void *vftable;			/* 0x000 */
	cEntity *attacker;		/* 0x004 */
	cEntity *attacked;		/* 0x008 */
	CGameParticles glow;		/* 0x00c on Dummy01 */
	CGameParticles dataCodec;	/* 0x0bc on Dummy02 */
	float time;			/* 0x16c since the start */
	float emitTime;		/* 0x170 the particles are emitted every 0.02 s of it */
	int nodeHand[2];		/* 0x174 R_Bishop_Mano_Izq, R_Bishop_Mano_Der */
	byte started;			/* 0x17c */
} cObjElementFXBishopSellLife;
SD_OFFSET(cObjElementFXBishopSellLife, nodeHand, 0x174);
SD_SIZE(cObjElementFXBishopSellLife, 0x180);

typedef struct cObjElementFXDTritusAttack {
	void *vftable;			/* 0x0000 */
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
SD_OFFSET(cObjElementFXDTritusAttack, point, 0x31c);
SD_OFFSET(cObjElementFXDTritusAttack, lenTarget, 0xed4);
SD_OFFSET(cObjElementFXDTritusAttack, trails, 0x112c);
SD_SIZE(cObjElementFXDTritusAttack, 0x1158);

typedef struct cObjElementFXFunctionarySpecialAction {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXFunctionarySpecialAction, trails, 0xc4);
SD_OFFSET(cObjElementFXFunctionarySpecialAction, pos, 0xfc);
SD_SIZE(cObjElementFXFunctionarySpecialAction, 0x1ac);

/* the electric ball of Gear: lightning bolts to the world or to entities near it, puffs, sparks and a tail */
typedef struct cObjElementFXGearMissile {
	void *vftable;			/* 0x000 */
	cEntity *entity;		/* 0x004 the missile (FXChar_SetI4) */
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
SD_OFFSET(cObjElementFXGearMissile, sparks, 0xc0);
SD_OFFSET(cObjElementFXGearMissile, lastPos, 0x17c);
SD_OFFSET(cObjElementFXGearMissile, trails, 0x1b8);
SD_OFFSET(cObjElementFXGearMissile, pts, 0x1e0);
SD_OFFSET(cObjElementFXGearMissile, boltScale, 0x690);
SD_SIZE(cObjElementFXGearMissile, 0x6bc);

typedef struct cObjElementFXMercenaryDrink {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	float emitTime;		/* 0xb8 the particles are emitted every 0.04 s of it */
	float time;			/* 0xbc since the start */
	vec3_t offset;			/* 0xc0 from the mouth of the jar, in its frame */
	int nodeJar;			/* 0xcc OB_JarraMercurio_MercA */
	byte started;			/* 0xd0 */
} cObjElementFXMercenaryDrink;
SD_OFFSET(cObjElementFXMercenaryDrink, nodeJar, 0xcc);
SD_SIZE(cObjElementFXMercenaryDrink, 0xd4);

typedef struct cObjElementFXMessengerAttack {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXMessengerAttack, trails, 0xe8);
SD_OFFSET(cObjElementFXMessengerAttack, lines, 0x100);
SD_OFFSET(cObjElementFXMessengerAttack, width, 0x3d0);
SD_SIZE(cObjElementFXMessengerAttack, 0x3ec);

typedef struct cObjElementFXMessengerTakeDataPack {
	void *vftable;			/* 0x000 */
	cEntity *owner;			/* 0x004 a WalkChar */
	cEntity *item;			/* 0x008 an Obj */
	float time;			/* 0x00c since the start */
	CGameParticles particles;	/* 0x010 on Dummy01 */
	CGameTrail trails[FXDATAPACK_TRAILS];	/* 0x0c0 */
	vec3_t points[FXDATAPACK_TRAILS][FXDATAPACK_POINTS];	/* 0x0d0 */
	float width[FXDATAPACK_TRAILS];	/* 0x160 0.6..1 */
	byte started;			/* 0x170 */
} cObjElementFXMessengerTakeDataPack;
SD_OFFSET(cObjElementFXMessengerTakeDataPack, points, 0xd0);
SD_OFFSET(cObjElementFXMessengerTakeDataPack, width, 0x160);
SD_SIZE(cObjElementFXMessengerTakeDataPack, 0x174);

typedef struct cObjElementFXPoliceGearConversion {
	void *vftable;			/* 0x000 */
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
SD_OFFSET(cObjElementFXPoliceGearConversion, bubbleH, 0x474);
SD_OFFSET(cObjElementFXPoliceGearConversion, bubbleMax, 0x744);
SD_OFFSET(cObjElementFXPoliceGearConversion, pieceTime, 0x99c);
SD_OFFSET(cObjElementFXPoliceGearConversion, particles2, 0xd4);
SD_OFFSET(cObjElementFXPoliceGearConversion, trails, 0x834);
SD_OFFSET(cObjElementFXPoliceGearConversion, armor, 0x84c);
SD_OFFSET(cObjElementFXPoliceGearConversion, piece, 0x8b8);
SD_OFFSET(cObjElementFXPoliceGearConversion, pieceOff, 0x96c);
SD_SIZE(cObjElementFXPoliceGearConversion, 0xa54);

typedef struct cObjElementFXPoliceSteal {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	cEntity *target;		/* 0x08 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	float emitTime;		/* 0xbc the particles are emitted every 0.04 s of it */
	int unused_c0;			/* 0xc0 */
	int unused_c4;			/* 0xc4 */
	byte started;			/* 0xc8 */
} cObjElementFXPoliceSteal;
SD_SIZE(cObjElementFXPoliceSteal, 0xcc);

typedef struct cObjElementFXSebastianFlash {
	void *vftable;			/* 0x000 */
	cEntity *ent;			/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	float emitTime;		/* 0x0b8 the particles are emitted every 0.04 s of it */
	float time;			/* 0x0bc since the start */
	vec3_t dir[FXSEBASTIAN_POINTS];	/* 0x0c0 length 0..3 */
	vec3_t dir2[FXSEBASTIAN_POINTS];	/* 0x444 length 15..80 */
	CGameTrail trails[FXSEBASTIAN_TRAILS];	/* 0x7c8 */
	byte started;			/* 0x7dc */
} cObjElementFXSebastianFlash;
SD_OFFSET(cObjElementFXSebastianFlash, dir2, 0x444);
SD_OFFSET(cObjElementFXSebastianFlash, trails, 0x7c8);
SD_SIZE(cObjElementFXSebastianFlash, 0x7e0);

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
void cObjElementFXBankMasterAttack_dtor(void *self);
void cObjElementFXBankMasterSuction_dtor(void *self);
void cObjElementFXBankerSteal_dtor(void *self);
void cObjElementFXBertoAttack_dtor(void *self);
void cObjElementFXBertoFlash_dtor(void *self);
void cObjElementFXBettyAttack_dtor(void *self);
void cObjElementFXBettyKill_dtor(cObjElementFXBettyKill *self);
void cObjElementFXBishopAttack_dtor(void *self);
void cObjElementFXBishopSellLife_dtor(void *self);
void cObjElementFXDTritusAttack_dtor(void *self);
void cObjElementFXDTritusKill_dtor(cObjElementFXDTritusKill *self);
void cObjElementFXFunctionarySpecialAction_dtor(void *self);
void cObjElementFXGearMissile_dtor(void *self);
void cObjElementFXMercenaryDrink_dtor(void *self);
void cObjElementFXMessengerAttack_dtor(void *self);
void cObjElementFXMessengerTakeDataPack_dtor(void *self);
void cObjElementFXPoliceGearConversion_dtor(void *self);
void cObjElementFXPoliceSteal_dtor(void *self);
void cObjElementFXSebastianFlash_dtor(void *self);
cObjElementFX *cObjElementFXBankMasterAttack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBankMasterSuction_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBankerSteal_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBertoAttack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBertoFlash_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBettyAttack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBettyKill_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBishopAttack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXBishopSellLife_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXDTritusAttack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXDTritusKill_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXFunctionarySpecialAction_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXGearMissile_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXMercenaryDrink_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXMessengerAttack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXMessengerTakeDataPack_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXPoliceGearConversion_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXPoliceSteal_Delete(cObjElementFX *base, byte flags);
cObjElementFX *cObjElementFXSebastianFlash_Delete(cObjElementFX *base, byte flags);

/* Alive (vf02); the 18 byte ones of the other classes are FXMisc_Alive */
/* BankMasterAttack and BertoAttack: the debris lives while the action of the character runs */
byte FXCharActions_AliveAction(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
byte cObjElementFXFunctionarySpecialAction_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
byte cObjElementFXPoliceGearConversion_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);

/* Attach (vf03) */
void cObjElementFXBankMasterAttack_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXBankMasterSuction_Attach(cObjElementFX *base, cDebris *d);
/* BankerSteal and PoliceSteal: only the particles at 0xc */
void FXCharActions_AttachC(cObjElementFX *base, cDebris *d);
/* BertoAttack and MercenaryDrink: the particles at 0x8 */
void FXCharActions_Attach8(cObjElementFX *base, cDebris *d);
void cObjElementFXBertoFlash_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXBettyAttack_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXKill_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXBishopAttack_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXBishopSellLife_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXDTritusAttack_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXFunctionarySpecialAction_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXGearMissile_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXMessengerAttack_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXMessengerTakeDataPack_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXPoliceGearConversion_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSebastianFlash_Attach(cObjElementFX *base, cDebris *d);

/* Reset (vf04) */
void cObjElementFXBankMasterAttack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBankMasterSuction_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBankerSteal_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBertoAttack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBertoFlash_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBettyAttack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXKill_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBishopAttack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXBishopSellLife_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXDTritusAttack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXFunctionarySpecialAction_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXGearMissile_Reset(cObjElementFX *base, cDebris *arg);
/* vf01: the bolts, the tail, the glows, puffs and sparks of the ball and its fade at the end of the debris */
void cObjElementFXGearMissile_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXMercenaryDrink_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXMessengerAttack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXMessengerTakeDataPack_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXPoliceGearConversion_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXPoliceSteal_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSebastianFlash_Reset(cObjElementFX *base, cDebris *arg);

/* the setters and starts; the entity at 0x4 of PoliceSteal, BankMasterSuction, BertoFlash, BettyAttack,
 * BishopAttack, MessengerAttack and SebastianFlash is FXMisc_SetEntity, the int at 0x8 of BankerSteal FXMisc_SetI8,
 * the one at 0x4 of GearMissile FXChar_SetI4 */
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
/* the beams from the vertices of the character, fading in, and the particles at their roots */
/* the swing of the baton while the action runs, fading out in 0.5 s after it */
void cObjElementFXBankMasterAttack_Update(cObjElementFX *base, cDebris *d, float dt);
/* the particles on 32 vertices of the crystal of Berto */
void cObjElementFXBertoAttack_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXDTritusAttack_Update(cObjElementFX *base, cDebris *d, float dt);
/* the coins (particles) from the target to the police every 0.04 s */
void cObjElementFXPoliceSteal_Update(cObjElementFX *base, cDebris *d, float dt);
/* the flash: random trails up from the character, in, out after 1.3 s, gone at 1.5 s */
void cObjElementFXSebastianFlash_Update(cObjElementFX *base, cDebris *d, float dt);
/* the flash of Berto: trails from Berto to the target, in 0.2 s, out until 0.7 s */
void cObjElementFXBertoFlash_Update(cObjElementFX *base, cDebris *d, float dt);
/* the coins from the target to the banker, faster when near, a steal sound every second coin */
void cObjElementFXBankerSteal_Update(cObjElementFX *base, cDebris *d, float dt);
/* the money sucked from the target to the baton along a Bezier curve, a steal sound every 8 points */
void cObjElementFXBankMasterSuction_Update(cObjElementFX *base, cDebris *d, float dt);
/* the trails of the Functionary while it runs fast (its speed above 1) */
void cObjElementFXFunctionarySpecialAction_Update(cObjElementFX *base, cDebris *d, float dt);
/* the lightning: the 6 trails along the Bezier curves of the lines, flashes at their ends */
void cObjElementFXMessengerAttack_Update(cObjElementFX *base, cDebris *d, float dt);
/* the data pack trails from the owner to the item, the middle points following, flashes at the ends */
void cObjElementFXMessengerTakeDataPack_Update(cObjElementFX *base, cDebris *d, float dt);
/* the mercury drops out of the jar of the Mercenary from 0.5 to 1.3 s */
void cObjElementFXMercenaryDrink_Update(cObjElementFX *base, cDebris *d, float dt);
/* the green ray of the Bishop from .. to with sparks at both ends */
void cObjElementFXBishopAttack_Update(cObjElementFX *base, cDebris *d, float dt);
/* the life sold by the Bishop: glows and data codes from the hands of the attacker into the attacked and a ring
 * of them at the attacked facing the camera */
void cObjElementFXBishopSellLife_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXBettyKill_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXDTritusKill_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXDTritusAttack_Start(cObjElementFXDTritusAttack *self, cEntity *e);
void cObjElementFXBishopAttack_SetLine(cObjElementFXBishopAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz);
/* the shot of the Betty attack at x y z: a sphere of particles */
void cObjElementFXBettyAttack_Emit(cObjElementFXBettyAttack *self, float x, float y, float z);
/* the attack of Betty: bolts out of the center, a ring that grows with bolts and dust on the side away from the
 * camera, the ring line, a wave in the dudv pass and a flash */
void cObjElementFXBettyAttack_Update(cObjElementFX *base, cDebris *d, float dt);
/* the conversion of the police into Gear: the armors fade in, bubbles with bolts rise around the waist, smoke, the
 * pieces of the armors fall off with sparks */
void cObjElementFXPoliceGearConversion_Update(cObjElementFX *base, cDebris *d, float dt);
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
