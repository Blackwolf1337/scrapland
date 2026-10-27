/*
 * Entities/Char/Type/ of the game, part 1 (src/game/Entities/Char/Type/Banker.c, BankMaster.c, Berto.c, Betty.c,
 * Bishop.c, CrazyGambler.c, Dtritus.c, Gear.c, Maintenance.c): the special actions of the character types. They
 * are C functions of the action scripts (cPy_NativeFunc, "!BankerAttack" and so on): the parameters come in a
 * memory file (the character name first, often an event name and a float after it). They play the actions of the
 * character, take its weapon energy, steal money, start the effects of include/scrap/fxcharactions.h and call
 * other natives of the same type through a sphere or flash test (the character of the action is kept in a global
 * "MeEnt" for them).
 */
#ifndef SCRAP_CHARTYPE_H
#define SCRAP_CHARTYPE_H

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/doors.h"
#include "scrap/fxcharactions.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/input.h"
#include "scrap/logic.h"
#include "scrap/sound.h"
#include "scrap/world.h"

/* the fields of the characters (cWalkCharEntity) the types use */
#define WChar_Cam(e) (*(cEntity **)((byte *)(e) + 0xf0))		/* the follow camera of a player */
#define WChar_Sounds(e) ((cSndInterface *)((byte *)(e) + 0x144))
#define WChar_FX(e) (*(void **)((byte *)(e) + 0x224))			/* the embedded effect */
#define WChar_MaxLife(e) (*(int *)((byte *)(e) + 0x22c))
#define WChar_Life(e) (*(int *)((byte *)(e) + 0x238))
#define WChar_God(e) (*(byte *)((byte *)(e) + 0x23c))			/* no damage */
#define WChar_Attack(e) ((void *)((byte *)(e) + 0x298))		/* who hit it last (cVehicleAttack) */
#define WChar_Shadow(e) ((void *)((byte *)(e) + 0x39c))		/* cPrimitive */
#define WChar_OwnerSlot(e) (*(int *)((byte *)(e) + 0x424))		/* "UsrControl", -1 for an AI */
#define WChar_Ang(e) ((vec3_t *)((byte *)(e) + 0x428))
#define WChar_Falling(e) (*(byte *)((byte *)(e) + 0x464))
#define WChar_DisplAng(e) (*(float *)((byte *)(e) + 0x468))
#define WChar_EnergyUsed(e) (*(float *)((byte *)(e) + 0x498))		/* 1 minus the weapon energy */
#define WChar_ViewLock(e) (*(byte *)((byte *)(e) + 0x4d4))		/* the actions set the view angles */
#define WChar_EnergyBarDecr(e) (*(float *)((byte *)(e) + 0x4b0))	/* the energy an action needs */
#define WChar_Money(e) (*(int *)((byte *)(e) + 0x4cc))		/* "Money" */
#define WChar_MoneyReward(e) (*(int *)((byte *)(e) + 0x4d0))		/* "MoneyReward" */
#define WChar_Control(e) ((void *)((byte *)(e) + 0x4ec))		/* Entities/Char/WalkCharControl.cpp */
#define WChar_ControlF4f8(e) (*(float *)((byte *)(e) + 0x4f8))
#define WChar_ActionPress(e) (*(byte *)((byte *)(e) + 0x504))		/* the action button is down */
#define WChar_Victim(e) (*(cEntity **)((byte *)(e) + 0x53c))		/* the character the banker robs */

/* the slots of the character vtables the types call (after the ones of cObjEntityVtbl) */
typedef struct cChTypeVtbl {
	void *vf00_42[67];
	void(__thiscall *SetViewAng)(cEntity *self, const vec3_t *ang);	/* 0x10c */
} cChTypeVtbl;
SD_OFFSET(cChTypeVtbl, SetViewAng, 0x10c);

static inline const cChTypeVtbl *ChType_Vtbl(cEntity *e) {
	return (const cChTypeVtbl *)e->vftable;
}

static inline void *ChType_ActCtrl(cEntity *e) {
	return WithLife_Vtbl(e)->GetActCtrl(e);
}

/* the action class of the controller */
static inline byte *ChType_ActClass(void *ctrl) {
	return *(byte **)((byte *)ctrl + 8);
}

/* the action of layer 0 of the controller has a priority (another one runs) */
static inline int ChType_ActPriority(void *ctrl) {
	return *(int *)(*(byte **)((byte *)ctrl + 0x14) + 0x64);
}

/* the slots of the local logic of a character the types call */
typedef struct cChTypeLogicVtbl {
	void *vf00_09[10];
	void(__thiscall *OnHit)(cLocalLogic *self, cEntity *attacker);			/* 0x28 */
	void *vf0b;
	void(__thiscall *OnAttack)(cLocalLogic *self, cEntity *attacker, float value);	/* 0x30 */
	void(__thiscall *OnShot)(cLocalLogic *self, cEntity *attacker);		/* 0x34 */
} cChTypeLogicVtbl;
SD_OFFSET(cChTypeLogicVtbl, OnHit, 0x28);
SD_OFFSET(cChTypeLogicVtbl, OnShot, 0x34);

static inline const cChTypeLogicVtbl *ChLogic_Vtbl(cLocalLogic *l) {
	return (const cChTypeLogicVtbl *)l->vftable;
}

/* the C function of the sphere tests of the natives (also g_DoorSphereFunc, g_InfernoSphereFunc) */
#define g_ChTypeSphereFunc ((cPy_Function *)Game_Ptr(0x46679c))
#define g_OnFlash ((cPy_Function *)Game_Ptr(0x446b04))		/* OnFlash, the C function of the flashes */

/* the natives one native calls through a sphere or flash test (rva, the same pointer as the original) */
#define CHTYPE_BANKMASTERSUCTION 0x0c1a70
#define CHTYPE_BERTOFLASHHIT 0x0c2480
#define CHTYPE_BETTYKILL 0x0c2c80
#define CHTYPE_DTRITUSKILL 0x0c5cd0

/* engine variables (Gvars) */
#define g_FXBankerStealFile GAME_DATA(char *, 0x446018)
#define g_BankerStealQuanto GAME_DATA(int, 0x446048)
#define g_FXBankMasterAttackFile GAME_DATA(char *, 0x4460b4)
#define g_BankMasterVacumDist GAME_DATA(float, 0x4460dc)
#define g_BankMasterAmmoCost GAME_DATA(float, 0x44617c)
#define g_FXBankMasterSuctionFile GAME_DATA(char *, 0x4461a4)
#define g_FXBertoFlashFile GAME_DATA(char *, 0x4461e0)
#define g_BertoFlashSound GAME_DATA(char *, 0x4461f8)
#define g_BertoFlashDist GAME_DATA(float, 0x446220)
#define g_BertoFlashSoundVol GAME_DATA(float, 0x446238)
#define g_BertoAmmoCost GAME_DATA(float, 0x446270)
#define g_FXBertoAttackFile GAME_DATA(char *, 0x446298)
#define g_BettyHitTimeFader GAME_DATA(float, 0x4462d4)
#define g_FXBettyAttackFile GAME_DATA(char *, 0x44631c)
#define g_BettyHSpeed GAME_DATA(float, 0x446334)
#define g_BettyHitPosY GAME_DATA(float, 0x44634c)
#define g_BettyHitRadius GAME_DATA(float, 0x446364)
#define g_BettyVSpeed GAME_DATA(float, 0x44637c)
#define g_FXBettyKillFile GAME_DATA(char *, 0x4463a4)
#define g_BettyHitTopLimit GAME_DATA(float, 0x4463bc)
#define g_BettyHitPosX GAME_DATA(float, 0x4463d4)
#define g_BettyHitTimeSpeed GAME_DATA(float, 0x4463ec)
#define g_BettyHitPosZ GAME_DATA(float, 0x446404)
#define g_FXBishopSellLifeFile GAME_DATA(char *, 0x4464a4)
#define g_DTritusHitTimeFader GAME_DATA(float, 0x446670)
#define g_FXDTritusAttackFile GAME_DATA(char *, 0x4466c8)
#define g_DTritusHitTimeSpeed GAME_DATA(float, 0x4466e0)
#define g_FXDTritusKillFile GAME_DATA(char *, 0x4466f8)
#define g_GearLowWeaponEnergy GAME_DATA(float, 0x446ac0)

/* the characters of the running actions (the "MeEnt" CHECKs) */
#define g_BankMasterEnt GAME_DATA(cEntity *, 0x4460a0)
#define g_BertoEnt GAME_DATA(cEntity *, 0x4461ac)
#define g_BettyEnt GAME_DATA(cEntity *, 0x4462a0)
#define g_DtritusEnt GAME_DATA(cEntity *, 0x44665c)

/* engine side */
/* the name of the action of layer i, "" when it does not run (rva 0x62c40) */
const char *__thiscall cActCtrl_GetActionName(void *ctrl, int i);
/* the weapon energy of a character, 0 or more (rva 0xd4bc0) */
void __thiscall cWalkCharEntity_SetEnergy(cEntity *self, float energy);
/* the control of a character (rva 0xd0390, 0xd0410) */
void __thiscall cWalkCharControl_Reset(void *ctrl, byte on);
void __thiscall cWalkCharControl_Update(void *ctrl, cEntity *wle, float dt);

/* Banker.c: the shared parts */
/* World.h, inlined: the entity of player slot 0 with the CHECK of a set one (cWorld_GetUsrEntity of scrap.h is the
 * same function, its copy sits in Banker.cpp) */
static inline cEntity *ChType_Usr0(void) {
	cWorld *w = g_pWorld;

	if (w->owners[0]) {
		CHECK(Entity_IsType(w->owners[0], TYEN_WITH_LIFE));
	}
	return w->owners[0];
}
/* the victim of the banker v, NULL when it has no money to give */
cEntity *ChType_BankerVictim(cEntity *v);
/* thief may rob v (unused first argument): v is a character with money */
byte ChType_CanRob(cEntity *thief, cEntity *v);
/* money from *victim to *thief: the player takes a part (frac) of the MoneyReward of the victim (stat counts it);
 * a character takes amount from the player or the MoneyReward of another one (its victim from its logic) */
byte ChType_Steal(cEntity **thief, cEntity **victim, int amount, const char *stat, float frac);
void ChType_BankDirectorActionPress(cAbstractFile *params);
void ChType_BankDirectorActionRelease(cAbstractFile *params);
void ChType_BankDirectorEndActionInit(cAbstractFile *params);
void ChType_BankerAttack(cAbstractFile *params);

/* BankMaster.c */
void ChType_BankMasterActionEnd(cAbstractFile *params);
void ChType_BankMasterActionInit(cAbstractFile *params);
void ChType_BankMasterActionPress(cAbstractFile *params);
void ChType_BankMasterAttack(cAbstractFile *params);
void ChType_BankMasterSuction(cAbstractFile *params);
void ChType_BankMasterHitEvent(cAbstractFile *params);
void ChType_FXBankMasterAttack(cAbstractFile *params);

/* Berto.c */
void ChType_BertoActionEnd(cAbstractFile *params);
void ChType_BertoActionInit(cAbstractFile *params);
void ChType_BertoActionPress(cAbstractFile *params);
void ChType_BertoAttack(cAbstractFile *params);
void ChType_BertoFlashHit(cAbstractFile *params);
void ChType_BertoHitEvent(cAbstractFile *params);
void ChType_FXBertoAttack(cAbstractFile *params);

/* Betty.c */
void ChType_BettyActionPress(cAbstractFile *params);
void ChType_BettyCameraSet(cAbstractFile *params);
/* out the angles that turn the z axis into dir (cdecl, rva 0xc3780) */
void ChType_DirAngles(const vec3_t *dir, vec3_t *out);
void ChType_BettyKill(cAbstractFile *params);
void ChType_BettyEndEvent(cAbstractFile *params);
void ChType_BettyHitEvent(cAbstractFile *params);
void ChType_BettyRestoreTime(cAbstractFile *params);
void ChType_FXBettyAttack(cAbstractFile *params);

/* Bishop.c (BishopEndActionEnd and GearEndActionEnd): the action again while the button is down and the energy
 * lasts, always for an AI */
void ChType_EndActionEnd(cAbstractFile *params);
void ChType_FXBishopSellLife(cAbstractFile *params);

/* CrazyGambler.c */
void ChType_FXCrazyGamblerSetHeadModel(cAbstractFile *params);

/* Dtritus.c */
void ChType_DtritusActionPress(cAbstractFile *params);
void ChType_DtritusKill(cAbstractFile *params);
void ChType_DtritusEndEvent(cAbstractFile *params);
void ChType_DtritusHitEvent(cAbstractFile *params);
void ChType_DtritusRestartEvent(cAbstractFile *params);
void ChType_FXDTritusAttack(cAbstractFile *params);

/* the shared steps of the natives (Banker.c) */
/* the parameters: the name, an event name and a float (unused) */
void ChType_ReadParams(cAbstractFile *f, char *name, char *event);
/* the character of that name with the CHECKs v and v->IsType(TYEN_WALKCHAR) */
cEntity *ChType_CheckChar(const char *name);
/* BankMasterActionPress, BertoActionPress and BishopActionPress: for a character that does not fall and runs no
 * other action, Action as the loop and ActionInit */
void ChType_ActionPress(cAbstractFile *params);
/* BankMasterAttack and BertoAttack: ActionEnd and Relax when the energy of the player is low or the button is up;
 * the energy minus cost */
void ChType_Attack(cAbstractFile *params, float cost);
/* the action sound (sound 0) silent, its loop flag; stopped too */
void ChType_ActionSound(cEntity *v, byte loop, byte stop);

#endif
