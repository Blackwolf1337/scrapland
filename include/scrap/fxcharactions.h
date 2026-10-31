/*
 * World/FX/FXCharActions.cpp of the game: the effects of the special actions of the characters, elements
 * (cObjElementFX) on the debris of a debris system like the ones of include/scrap/fxchar.h: the attacks and actions
 * of BankMaster, Banker, Berto, Betty, Bishop, D-Tritus, Functionary, Gear, Mercenary, Messenger, Police and
 * Sebastian. The element vtable: 0 Delete, 1 Update (debris, dt), 2 Alive (debris, sys), 3 Attach (debris),
 * 4 Reset. The native functions that start them (rva 0x4c1000..) set the entities with the setters below.
 */
#ifndef SCRAP_FXCHARACTIONS_H
#define SCRAP_FXCHARACTIONS_H

#include "scrap/fxchar.h"
#include "scrap/fxmisc.h"
#include "scrap/timer.h"

/* the vtables */
#define VT_FXBANKMASTERATTACK GAME_VFTABLE(cObjElementFXBankMasterAttack_vftable, 0x3cb72c)
#define VT_FXBANKMASTERSUCTION GAME_VFTABLE(cObjElementFXBankMasterSuction_vftable, 0x3cb744)
#define VT_FXBANKERSTEAL GAME_VFTABLE(cObjElementFXBankerSteal_vftable, 0x3cb5c4)
#define VT_FXBERTOATTACK GAME_VFTABLE(cObjElementFXBertoAttack_vftable, 0x3cb684)
#define VT_FXBERTOFLASH GAME_VFTABLE(cObjElementFXBertoFlash_vftable, 0x3cb69c)
#define VT_FXBETTYATTACK GAME_VFTABLE(cObjElementFXBettyAttack_vftable, 0x3cb6fc)
#define VT_FXBETTYKILL GAME_VFTABLE(cObjElementFXBettyKill_vftable, 0x3cb714)
#define VT_FXBISHOPATTACK GAME_VFTABLE(cObjElementFXBishopAttack_vftable, 0x3cb66c)
#define VT_FXBISHOPSELLLIFE GAME_VFTABLE(cObjElementFXBishopSellLife_vftable, 0x3cb654)
#define VT_FXDTRITUSATTACK GAME_VFTABLE(cObjElementFXDTritusAttack_vftable, 0x3cb624)
#define VT_FXDTRITUSKILL GAME_VFTABLE(cObjElementFXDTritusKill_vftable, 0x3cb63c)
#define VT_FXFUNCTIONARYSPECIALACTION GAME_VFTABLE(cObjElementFXFunctionarySpecialAction_vftable, 0x3cb5f4)
#define VT_FXGEARMISSILE GAME_VFTABLE(cObjElementFXGearMissile_vftable, 0x3cb5ac)
#define VT_FXMERCENARYDRINK GAME_VFTABLE(cObjElementFXMercenaryDrink_vftable, 0x3cb75c)
#define VT_FXMESSENGERATTACK GAME_VFTABLE(cObjElementFXMessengerAttack_vftable, 0x3cb6b4)
#define VT_FXMESSENGERTAKEDATAPACK GAME_VFTABLE(cObjElementFXMessengerTakeDataPack_vftable, 0x3cb6cc)
#define VT_FXPOLICEGEARCONVERSION GAME_VFTABLE(cObjElementFXPoliceGearConversion_vftable, 0x3cb60c)
#define VT_FXPOLICESTEAL GAME_VFTABLE(cObjElementFXPoliceSteal_vftable, 0x3cb5dc)
#define VT_FXSEBASTIANFLASH GAME_VFTABLE(cObjElementFXSebastianFlash_vftable, 0x3cb6e4)

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
#define g_FXFunctionaryInit GAME_GLOBAL(byte, g_FXFunctionaryInit, 0x466fc0)
#define g_FXFunctionaryVertex (GAME_ARRAY(int, g_FXFunctionaryVertex, 0x466fc4))		/* [14] */
#define g_FXFunctionaryColor (GAME_ARRAY(uint32_t, g_FXFunctionaryColor, 0x466ffc))		/* [14] */

/* the engine variables (Gvars) */
#define g_BankerSteal1Sound GAME_GLOBAL_IN(const char *, GVar_BankerSteal1Sound, 16, 0x446098)
#define g_BankerSteal2Sound GAME_GLOBAL_IN(const char *, GVar_BankerSteal2Sound, 16, 0x446000)
#define g_BankerSteal3Sound GAME_GLOBAL_IN(const char *, GVar_BankerSteal3Sound, 16, 0x446030)
#define g_BankerStealSoundVol GAME_GLOBAL_IN(float, GVar_BankerStealSoundVol, 16, 0x445f98)
#define g_BankerStealPosX GAME_GLOBAL_IN(float, GVar_BankerStealPosX, 16, 0x445fd8)
#define g_BankerStealPosY GAME_GLOBAL_IN(float, GVar_BankerStealPosY, 16, 0x446080)
#define g_BankerStealPosZ GAME_GLOBAL_IN(float, GVar_BankerStealPosZ, 16, 0x445fc0)
#define g_BankMasterBatonPosX GAME_GLOBAL_IN(float, GVar_BankMasterBatonPosX, 16, 0x446164)
#define g_BankMasterBatonPosY GAME_GLOBAL_IN(float, GVar_BankMasterBatonPosY, 16, 0x44614c)
#define g_BankMasterBatonPosZ GAME_GLOBAL_IN(float, GVar_BankMasterBatonPosZ, 16, 0x4460f4)

/* the timer of a character (cWalkChar + 0x110) */
#define Entity_Timer(e) (&((cObjEntity *)(e))->timer)
/* a float of the Police character: its center is this much above the feet */
#define Entity_F45C(e) (WChar(e)->floorDist)

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
void *__thiscall cObjElementFXBankMasterAttack_ctor(void *self);
void *__thiscall cObjElementFXBankMasterSuction_ctor(void *self);
void *__thiscall cObjElementFXBankerSteal_ctor(void *self);
void *__thiscall cObjElementFXBertoAttack_ctor(void *self);
void *__thiscall cObjElementFXBertoFlash_ctor(void *self);
void *__thiscall cObjElementFXBettyAttack_ctor(void *self);
cObjElementFXBettyKill *__thiscall cObjElementFXBettyKill_ctor(cObjElementFXBettyKill *self);
void *__thiscall cObjElementFXBishopAttack_ctor(void *self);
void *__thiscall cObjElementFXBishopSellLife_ctor(void *self);
void *__thiscall cObjElementFXDTritusAttack_ctor(void *self);
cObjElementFXDTritusKill *__thiscall cObjElementFXDTritusKill_ctor(cObjElementFXDTritusKill *self);
void *__thiscall cObjElementFXFunctionarySpecialAction_ctor(void *self);
void *__thiscall cObjElementFXGearMissile_ctor(void *self);
void *__thiscall cObjElementFXMercenaryDrink_ctor(void *self);
void *__thiscall cObjElementFXMessengerAttack_ctor(void *self);
void *__thiscall cObjElementFXMessengerTakeDataPack_ctor(void *self);
void *__thiscall cObjElementFXPoliceGearConversion_ctor(void *self);
void *__thiscall cObjElementFXPoliceSteal_ctor(void *self);
void *__thiscall cObjElementFXSebastianFlash_ctor(void *self);
void __thiscall cObjElementFXBankMasterAttack_dtor(void *self);
void __thiscall cObjElementFXBankMasterSuction_dtor(void *self);
void __thiscall cObjElementFXBankerSteal_dtor(void *self);
void __thiscall cObjElementFXBertoAttack_dtor(void *self);
void __thiscall cObjElementFXBertoFlash_dtor(void *self);
void __thiscall cObjElementFXBettyAttack_dtor(void *self);
void __thiscall cObjElementFXBettyKill_dtor(cObjElementFXBettyKill *self);
void __thiscall cObjElementFXBishopAttack_dtor(void *self);
void __thiscall cObjElementFXBishopSellLife_dtor(void *self);
void __thiscall cObjElementFXDTritusAttack_dtor(void *self);
void __thiscall cObjElementFXDTritusKill_dtor(cObjElementFXDTritusKill *self);
void __thiscall cObjElementFXFunctionarySpecialAction_dtor(void *self);
void __thiscall cObjElementFXGearMissile_dtor(void *self);
void __thiscall cObjElementFXMercenaryDrink_dtor(void *self);
void __thiscall cObjElementFXMessengerAttack_dtor(void *self);
void __thiscall cObjElementFXMessengerTakeDataPack_dtor(void *self);
void __thiscall cObjElementFXPoliceGearConversion_dtor(void *self);
void __thiscall cObjElementFXPoliceSteal_dtor(void *self);
void __thiscall cObjElementFXSebastianFlash_dtor(void *self);
void *__thiscall cObjElementFXBankMasterAttack_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXBankMasterSuction_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXBankerSteal_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXBertoAttack_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXBertoFlash_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXBettyAttack_Delete(void *self, uint32_t flags);
cObjElementFXBettyKill *__thiscall cObjElementFXBettyKill_Delete(cObjElementFXBettyKill *self, uint32_t flags);
void *__thiscall cObjElementFXBishopAttack_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXBishopSellLife_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXDTritusAttack_Delete(void *self, uint32_t flags);
cObjElementFXDTritusKill *__thiscall cObjElementFXDTritusKill_Delete(cObjElementFXDTritusKill *self, uint32_t flags);
void *__thiscall cObjElementFXFunctionarySpecialAction_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXGearMissile_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXMercenaryDrink_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXMessengerAttack_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXMessengerTakeDataPack_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXPoliceGearConversion_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXPoliceSteal_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXSebastianFlash_Delete(void *self, uint32_t flags);

/* Alive (vf02); the 18 byte ones of the other classes are FXMisc_Alive */
/* BankMasterAttack and BertoAttack: the debris lives while the action of the character runs */
byte __thiscall FXCharActions_AliveAction(void *self, cDebris *d, cDebrisSys *sys);
byte __thiscall cObjElementFXFunctionarySpecialAction_Alive(cObjElementFXFunctionarySpecialAction *self, cDebris *d,
	cDebrisSys *sys);
byte __thiscall cObjElementFXPoliceGearConversion_Alive(cObjElementFXPoliceGearConversion *self, cDebris *d,
	cDebrisSys *sys);

/* Attach (vf03) */
void __thiscall cObjElementFXBankMasterAttack_Attach(cObjElementFXBankMasterAttack *self, cDebris *d);
void __thiscall cObjElementFXBankMasterSuction_Attach(cObjElementFXBankMasterSuction *self, cDebris *d);
/* BankerSteal and PoliceSteal: only the particles at 0xc */
void __thiscall FXCharActions_AttachC(void *self, cDebris *d);
/* BertoAttack and MercenaryDrink: the particles at 0x8 */
void __thiscall FXCharActions_Attach8(void *self, cDebris *d);
void __thiscall cObjElementFXBertoFlash_Attach(cObjElementFXBertoFlash *self, cDebris *d);
void __thiscall cObjElementFXBettyAttack_Attach(cObjElementFXBettyAttack *self, cDebris *d);
void __thiscall cObjElementFXKill_Attach(cObjElementFXKill *self, cDebris *d);
void __thiscall cObjElementFXBishopAttack_Attach(cObjElementFXBishopAttack *self, cDebris *d);
void __thiscall cObjElementFXBishopSellLife_Attach(cObjElementFXBishopSellLife *self, cDebris *d);
void __thiscall cObjElementFXDTritusAttack_Attach(cObjElementFXDTritusAttack *self, cDebris *d);
void __thiscall cObjElementFXFunctionarySpecialAction_Attach(cObjElementFXFunctionarySpecialAction *self, cDebris *d);
void __thiscall cObjElementFXGearMissile_Attach(cObjElementFXGearMissile *self, cDebris *d);
void __thiscall cObjElementFXMessengerAttack_Attach(cObjElementFXMessengerAttack *self, cDebris *d);
void __thiscall cObjElementFXMessengerTakeDataPack_Attach(cObjElementFXMessengerTakeDataPack *self, cDebris *d);
void __thiscall cObjElementFXPoliceGearConversion_Attach(cObjElementFXPoliceGearConversion *self, cDebris *d);
void __thiscall cObjElementFXSebastianFlash_Attach(cObjElementFXSebastianFlash *self, cDebris *d);

/* Reset (vf04) */
void __thiscall cObjElementFXBankMasterAttack_Reset(cObjElementFXBankMasterAttack *self, int arg);
void __thiscall cObjElementFXBankMasterSuction_Reset(cObjElementFXBankMasterSuction *self, int arg);
void __thiscall cObjElementFXBankerSteal_Reset(cObjElementFXBankerSteal *self, int arg);
void __thiscall cObjElementFXBertoAttack_Reset(cObjElementFXBertoAttack *self, int arg);
void __thiscall cObjElementFXBertoFlash_Reset(cObjElementFXBertoFlash *self, int arg);
void __thiscall cObjElementFXBettyAttack_Reset(cObjElementFXBettyAttack *self, int arg);
void __thiscall cObjElementFXKill_Reset(cObjElementFXKill *self, int arg);
void __thiscall cObjElementFXBishopAttack_Reset(cObjElementFXBishopAttack *self, int arg);
void __thiscall cObjElementFXBishopSellLife_Reset(cObjElementFXBishopSellLife *self, int arg);
void __thiscall cObjElementFXDTritusAttack_Reset(cObjElementFXDTritusAttack *self, int arg);
void __thiscall cObjElementFXFunctionarySpecialAction_Reset(cObjElementFXFunctionarySpecialAction *self, int arg);
void __thiscall cObjElementFXGearMissile_Reset(cObjElementFXGearMissile *self, int arg);
/* vf01: the bolts, the tail, the glows, puffs and sparks of the ball and its fade at the end of the debris */
void __thiscall cObjElementFXGearMissile_Update(cObjElementFXGearMissile *self, cDebris *debris, float dt);
void __thiscall cObjElementFXMercenaryDrink_Reset(cObjElementFXMercenaryDrink *self, int arg);
void __thiscall cObjElementFXMessengerAttack_Reset(cObjElementFXMessengerAttack *self, int arg);
void __thiscall cObjElementFXMessengerTakeDataPack_Reset(cObjElementFXMessengerTakeDataPack *self, int arg);
void __thiscall cObjElementFXPoliceGearConversion_Reset(cObjElementFXPoliceGearConversion *self, int arg);
void __thiscall cObjElementFXPoliceSteal_Reset(cObjElementFXPoliceSteal *self, int arg);
void __thiscall cObjElementFXSebastianFlash_Reset(cObjElementFXSebastianFlash *self, int arg);

/* the setters and starts; the entity at 0x4 of PoliceSteal, BankMasterSuction, BertoFlash, BettyAttack,
 * BishopAttack, MessengerAttack and SebastianFlash is FXMisc_SetEntity, the int at 0x8 of BankerSteal FXMisc_SetI8,
 * the one at 0x4 of GearMissile FXChar_SetI4 */
/* the entity at 0x8 (PoliceSteal, BankMasterSuction, BertoFlash), its model a CHECK */
void __thiscall FXCharActions_SetEntity8(void *self, cEntity *e);
void __thiscall cObjElementFXBankerSteal_SetEntity(cObjElementFXBankerSteal *self, cEntity *e);
/* cObjElementFXFunctionarySpecialAction_Start (the trails from 14 vertices of the WalkChar) is in fxchar.h */
void __thiscall cObjElementFXBishopSellLife_SetAttacked(cObjElementFXBishopSellLife *self, cEntity *e);
void __thiscall cObjElementFXBishopSellLife_SetAttacker(cObjElementFXBishopSellLife *self, cEntity *e);
void __thiscall cObjElementFXMessengerTakeDataPack_SetItem(cObjElementFXMessengerTakeDataPack *self, cEntity *e);
void __thiscall cObjElementFXMessengerTakeDataPack_SetOwner(cObjElementFXMessengerTakeDataPack *self, cEntity *e);
void __thiscall cObjElementFXMercenaryDrink_SetC0(cObjElementFXMercenaryDrink *self, float a, float b, float c);
void __thiscall cObjElementFXMercenaryDrink_Start(cObjElementFXMercenaryDrink *self, cEntity *e);
void __thiscall cObjElementFXBankMasterAttack_Start(cObjElementFXBankMasterAttack *self, cEntity *e);
void __thiscall cObjElementFXBertoAttack_Start(cObjElementFXBertoAttack *self, cEntity *e);
/* BettyKill and DTritusKill: 75 random vertices of the model of e with random factors */
void __thiscall cObjElementFXKill_Start(cObjElementFXKill *self, cEntity *e);
void __thiscall cObjElementFXKill_SetAngles(cObjElementFXKill *self, float x, float y, float z);
/* the beams from the vertices of the character, fading in, and the particles at their roots */
/* the swing of the baton while the action runs, fading out in 0.5 s after it */
void __thiscall cObjElementFXBankMasterAttack_Update(cObjElementFXBankMasterAttack *self, cDebris *d, float dt);
/* the particles on 32 vertices of the crystal of Berto */
void __thiscall cObjElementFXBertoAttack_Update(cObjElementFXBertoAttack *self, cDebris *d, float dt);
void __thiscall cObjElementFXDTritusAttack_Update(cObjElementFXDTritusAttack *self, cDebris *d, float dt);
/* the coins (particles) from the target to the police every 0.04 s */
void __thiscall cObjElementFXPoliceSteal_Update(cObjElementFXPoliceSteal *self, cDebris *d, float dt);
/* the flash: random trails up from the character, in, out after 1.3 s, gone at 1.5 s */
void __thiscall cObjElementFXSebastianFlash_Update(cObjElementFXSebastianFlash *self, cDebris *d, float dt);
/* the flash of Berto: trails from Berto to the target, in 0.2 s, out until 0.7 s */
void __thiscall cObjElementFXBertoFlash_Update(cObjElementFXBertoFlash *self, cDebris *d, float dt);
/* the coins from the target to the banker, faster when near, a steal sound every second coin */
void __thiscall cObjElementFXBankerSteal_Update(cObjElementFXBankerSteal *self, cDebris *d, float dt);
/* the money sucked from the target to the baton along a Bezier curve, a steal sound every 8 points */
void __thiscall cObjElementFXBankMasterSuction_Update(cObjElementFXBankMasterSuction *self, cDebris *d, float dt);
/* the trails of the Functionary while it runs fast (its speed above 1) */
void __thiscall cObjElementFXFunctionarySpecialAction_Update(cObjElementFXFunctionarySpecialAction *self, cDebris *d,
	float dt);
/* the lightning: the 6 trails along the Bezier curves of the lines, flashes at their ends */
void __thiscall cObjElementFXMessengerAttack_Update(cObjElementFXMessengerAttack *self, cDebris *d, float dt);
/* the data pack trails from the owner to the item, the middle points following, flashes at the ends */
void __thiscall cObjElementFXMessengerTakeDataPack_Update(cObjElementFXMessengerTakeDataPack *self, cDebris *d,
	float dt);
/* the mercury drops out of the jar of the Mercenary from 0.5 to 1.3 s */
void __thiscall cObjElementFXMercenaryDrink_Update(cObjElementFXMercenaryDrink *self, cDebris *d, float dt);
/* the green ray of the Bishop from .. to with sparks at both ends */
void __thiscall cObjElementFXBishopAttack_Update(cObjElementFXBishopAttack *self, cDebris *d, float dt);
/* the life sold by the Bishop: glows and data codes from the hands of the attacker into the attacked and a ring
 * of them at the attacked facing the camera */
void __thiscall cObjElementFXBishopSellLife_Update(cObjElementFXBishopSellLife *self, cDebris *d, float dt);
void __thiscall cObjElementFXBettyKill_Update(cObjElementFXBettyKill *self, cDebris *d, float dt);
void __thiscall cObjElementFXDTritusKill_Update(cObjElementFXDTritusKill *self, cDebris *d, float dt);
void __thiscall cObjElementFXDTritusAttack_Start(cObjElementFXDTritusAttack *self, cEntity *e);
void __thiscall cObjElementFXBishopAttack_SetLine(cObjElementFXBishopAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz);
/* the shot of the Betty attack at x y z: a sphere of particles */
void __thiscall cObjElementFXBettyAttack_Emit(cObjElementFXBettyAttack *self, float x, float y, float z);
/* the attack of Betty: bolts out of the center, a ring that grows with bolts and dust on the side away from the
 * camera, the ring line, a wave in the dudv pass and a flash */
void __thiscall cObjElementFXBettyAttack_Update(cObjElementFXBettyAttack *self, cDebris *d, float dt);
/* the conversion of the police into Gear: the armors fade in, bubbles with bolts rise around the waist, smoke, the
 * pieces of the armors fall off with sparks */
void __thiscall cObjElementFXPoliceGearConversion_Update(cObjElementFXPoliceGearConversion *self, cDebris *d,
	float dt);
/* the trails from the owner to the item, 3 points each */
void __fastcall cObjElementFXMessengerTakeDataPack_Start(cObjElementFXMessengerTakeDataPack *self);
/* the lightning from .. to and 5 branches off its Bezier curve (stopped by the floor) */
void __thiscall cObjElementFXMessengerAttack_SetLine(cObjElementFXMessengerAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz);
/* the armors at the Police character e and the smoke and sparks at its feet if it is visible */
void __thiscall cObjElementFXPoliceGearConversion_Start(cObjElementFXPoliceGearConversion *self, cEntity *e);
/* GearMissile: 8 Updates of 0 seconds through the vtable */
void __thiscall cObjElementFXGearMissile_Run(void *self, cDebris *d);
/* the random directions of the start */
void __fastcall cObjElementFXBankMasterSuction_RandomPoints(cObjElementFXBankMasterSuction *self);
void __fastcall cObjElementFXBertoFlash_RandomPoints(cObjElementFXBertoFlash *self);
void __fastcall cObjElementFXSebastianFlash_RandomPoints(cObjElementFXSebastianFlash *self);

#endif
