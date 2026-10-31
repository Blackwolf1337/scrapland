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
#include "scrap/walkcharcontrol.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

/* the fields of the characters (cWalkCharEntity) the types use */
#define WChar_Cam(e) (WChar(e)->wl.obj.cam)		/* the follow camera of a player */
#define WChar_Sounds(e) (&WChar(e)->wl.obj.snd)
#define WChar_FX(e) ((void *)WChar(e)->wl.obj.fx)		/* the embedded effect */
#define WChar_PrevLife(e) (WChar(e)->wl.prevLife)
#define WChar_Life(e) (WChar(e)->wl.life)
#define WChar_God(e) (WChar(e)->wl.god)			/* no damage */
#define WChar_Attack(e) (&WChar(e)->wl.attack)		/* who hit it last (cVehicleAttack) */
#define WChar_Shadow(e) (&WChar(e)->wl.prim)		/* cPrimitive */
#define WChar_B381(e) (WChar(e)->wl.arrow.canUse)		/* "ArrowCanUse", cleared by the flash */
#define WChar_OwnerSlot(e) (WChar(e)->ownerSlot)		/* "UsrControl", -1 for an AI */
#define WChar_Ang(e) (&WChar(e)->view)
#define WChar_FallVel(e) (WChar(e)->fallVel)		/* the velocity of the fall */
#define WChar_SlideTime(e) (WChar(e)->slideTime)		/* "SlidingAndFallingTime" */
#define WChar_Falling(e) (WChar(e)->falling)
#define WChar_DisplAng(e) (WChar(e)->displAng)
#define WChar_EnergyUsed(e) (WChar(e)->energyUsed)		/* 1 minus the weapon energy */
#define WChar_ViewLock(e) (WChar(e)->viewLock)		/* the actions set the view angles */
#define WChar_EnergyBarDecr(e) (WChar(e)->energyBarDecr)	/* the energy an action needs */
#define WChar_Money(e) (WChar(e)->money)		/* "Money" */
#define WChar_MoneyReward(e) (WChar(e)->moneyReward)		/* "MoneyReward" */
#define WChar_Control(e) (&WChar(e)->control)	/* Entities/Char/WalkCharControl.cpp */
#define WChar_ControlF4f8(e) (WChar(e)->control.walk)	/* the walk of the control (x, z) */
#define WChar_ControlF4fc(e) (WChar(e)->control.strafe)
#define WChar_ActionPress(e) (WChar(e)->control.act)		/* the action button is down */
#define WChar_ListenNode(e) (&WChar(e)->listen)	/* to the character it listens to */
#define WChar_Listen(e) ((cEntity *)WChar(e)->listen.target)		/* its target: the mayor, the police */
#define WChar_ListenOn(e) (WChar(e)->listenOn)		/* the player still talks to it */
#define WChar_Victim(e) ((cEntity *)WChar(e)->victim.target)		/* the character the banker robs */

static inline void *ChType_ActCtrl(cEntity *e) {
	return WithLife_Vtbl(e)->GetActCtrl(e);
}

/* the action class of the controller */
static inline cActClass *ChType_ActClass(void *ctrl) {
	return ((cActCtrl *)ctrl)->cls;
}

/* the action of layer 0 of the controller has a priority (another one runs) */
static inline int ChType_ActPriority(void *ctrl) {
	return ((cActCtrl *)ctrl)->layers[0].action->priority;
}

/* the slots of the local logic of a character the types call */
typedef struct cChTypeLogicVtbl {
	void *vf00_08[9];
	byte(__thiscall *Listens)(cLocalLogic *self, cEntity *talker);			/* 0x24 vf09 the mayor */
	void(__thiscall *OnHit)(cLocalLogic *self, cEntity *attacker);			/* 0x28 */
	void(__thiscall *OnSeen)(cLocalLogic *self, cEntity *sentinel);		/* 0x2c vf11 the sentinel */
	void(__thiscall *OnAttack)(cLocalLogic *self, cEntity *attacker, float value);	/* 0x30 */
	void(__thiscall *OnShot)(cLocalLogic *self, cEntity *attacker);		/* 0x34 */
	void(__thiscall *OnHammer)(cLocalLogic *self, cEntity *nurse);			/* 0x38 vf14 */
	byte(__thiscall *GivesMoney)(cLocalLogic *self, cEntity *police);		/* 0x3c vf15 */
	void *vf10_12[3];
	void(__thiscall *SetState)(cLocalLogic *self, int state, cEntity *target);	/* 0x4c vf19 */
	void(__thiscall *SetPaused)(cLocalLogic *self, byte on);			/* 0x50 vf20 */
	void *vf15_17[3];
	void(__thiscall *Resume)(cLocalLogic *self);					/* 0x60 vf24 the state again */
} cChTypeLogicVtbl;
SD_OFFSET(cChTypeLogicVtbl, Listens, 0x24);
SD_OFFSET(cChTypeLogicVtbl, OnHit, 0x28);
SD_OFFSET(cChTypeLogicVtbl, OnShot, 0x34);
SD_OFFSET(cChTypeLogicVtbl, GivesMoney, 0x3c);
SD_OFFSET(cChTypeLogicVtbl, SetState, 0x4c);
SD_OFFSET(cChTypeLogicVtbl, SetPaused, 0x50);
SD_OFFSET(cChTypeLogicVtbl, Resume, 0x60);

/* the fields of the local logic of a character (cBaseLocalLogic, Logic/BaseLogic.cpp) */
#define ChLogic_Target(l) ((cEntity *)((cBaseLocalLogic *)(l))->target.target)	/* of the state */
#define ChLogic_Idle(l) (((cBaseLocalLogic *)(l))->idle)
#define ChLogic_AlwaysLock(l) (((cBaseLocalLogic *)(l))->alwaysLock)
#define ChLogic_PoliceResult(l) (((cLittlePoliceLocalLogic *)(l))->policeResult)	/* of a police logic */

/* the action of layer 0 of the controller */
#define ChType_Action0(ctrl) ((void *)((cActCtrl *)(ctrl))->layers[0].action)

static inline const cChTypeLogicVtbl *ChLogic_Vtbl(cLocalLogic *l) {
	return (const cChTypeLogicVtbl *)l->vftable;
}

/* the C function of the sphere tests of the natives (also g_DoorSphereFunc, g_InfernoSphereFunc) */
#define g_ChTypeSphereFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0, 0x46679c))
#define g_OnFlash (GAME_ARRAY_IN(cPy_Function, GVar_OnFlash, 16, 0x446b04))		/* OnFlash, the C function of the flashes */

/* the natives one native calls through a sphere or flash test (rva, the same pointer as the original) */
#define CHTYPE_BANKMASTERSUCTION 0x0c1a70
#define CHTYPE_BERTOFLASHHIT 0x0c2480
#define CHTYPE_BETTYKILL 0x0c2c80
#define CHTYPE_DTRITUSKILL 0x0c5cd0
#define CHTYPE_GEARHIT 0x0c7020
#define CHTYPE_GEARSPHEREHIT 0x0c9130
#define CHTYPE_FLASHHIT 0x0c92f0

/* engine variables (Gvars) */
#define g_FXBankerStealFile GAME_GLOBAL_IN(char *, GVar_FXBankerStealFile, 16, 0x446018)
#define g_BankerStealAmount GAME_GLOBAL_IN(int, GVar_BankerStealAmount, 16, 0x446048)
#define g_FXBankMasterAttackFile GAME_GLOBAL_IN(char *, GVar_FXBankMasterAttackFile, 16, 0x4460b4)
#define g_BankMasterVacumDist GAME_GLOBAL_IN(float, GVar_BankMasterVacumDist, 16, 0x4460dc)
#define g_BankMasterAmmoCost GAME_GLOBAL_IN(float, GVar_BankMasterAmmoCost, 16, 0x44617c)
#define g_FXBankMasterSuctionFile GAME_GLOBAL_IN(char *, GVar_FXBankMasterSuctionFile, 16, 0x4461a4)
#define g_FXBertoFlashFile GAME_GLOBAL_IN(char *, GVar_FXBertoFlashFile, 16, 0x4461e0)
#define g_BertoFlashSound GAME_GLOBAL_IN(char *, GVar_BertoFlashSound, 16, 0x4461f8)
#define g_BertoFlashDist GAME_GLOBAL_IN(float, GVar_BertoFlashDist, 16, 0x446220)
#define g_BertoFlashSoundVol GAME_GLOBAL_IN(float, GVar_BertoFlashSoundVol, 16, 0x446238)
#define g_BertoAmmoCost GAME_GLOBAL_IN(float, GVar_BertoAmmoCost, 16, 0x446270)
#define g_FXBertoAttackFile GAME_GLOBAL_IN(char *, GVar_FXBertoAttackFile, 16, 0x446298)
#define g_BettyHitTimeFader GAME_GLOBAL_IN(float, GVar_BettyHitTimeFader, 16, 0x4462d4)
#define g_FXBettyAttackFile GAME_GLOBAL_IN(char *, GVar_FXBettyAttackFile, 16, 0x44631c)
#define g_BettyHSpeed GAME_GLOBAL_IN(float, GVar_BettyHSpeed, 16, 0x446334)
#define g_BettyHitPosY GAME_GLOBAL_IN(float, GVar_BettyHitPosY, 16, 0x44634c)
#define g_BettyHitRadius GAME_GLOBAL_IN(float, GVar_BettyHitRadius, 16, 0x446364)
#define g_BettyVSpeed GAME_GLOBAL_IN(float, GVar_BettyVSpeed, 16, 0x44637c)
#define g_FXBettyKillFile GAME_GLOBAL_IN(char *, GVar_FXBettyKillFile, 16, 0x4463a4)
#define g_BettyHitTopLimit GAME_GLOBAL_IN(float, GVar_BettyHitTopLimit, 16, 0x4463bc)
#define g_BettyHitPosX GAME_GLOBAL_IN(float, GVar_BettyHitPosX, 16, 0x4463d4)
#define g_BettyHitTimeSpeed GAME_GLOBAL_IN(float, GVar_BettyHitTimeSpeed, 16, 0x4463ec)
#define g_BettyHitPosZ GAME_GLOBAL_IN(float, GVar_BettyHitPosZ, 16, 0x446404)
#define g_BishopHitSound GAME_GLOBAL_IN(char *, GVar_BishopHitSound, 16, 0x44641c)
#define g_BishopAngleVariation GAME_GLOBAL_IN(float, GVar_BishopAngleVariation, 16, 0x446434)
#define g_BishopHitSoundVol GAME_GLOBAL_IN(float, GVar_BishopHitSoundVol, 16, 0x44645c)
#define g_BishopWeaponDamage GAME_GLOBAL_IN(int, GVar_BishopWeaponDamage, 16, 0x446474)
#define g_BishopAttackPosZ GAME_GLOBAL_IN(float, GVar_BishopAttackPosZ, 16, 0x44648c)
#define g_DesktopJumpPrice GAME_GLOBAL_IN(int, GVar_DesktopJumpPrice, 16, 0x4465d4)
#define g_DesktopJumpSlideTime GAME_GLOBAL_IN(float, GVar_DesktopJumpSlideTime, 16, 0x4465ec)
#define g_DesktopJumpSpeed GAME_GLOBAL_IN(float, GVar_DesktopJumpSpeed, 16, 0x44663c)
#define g_FXDesktopFallEndDist GAME_GLOBAL_IN(float, GVar_FXDesktopFallEndDist, 16, 0x446614)
#define g_FXDesktopFallEndFile GAME_GLOBAL_IN(char *, GVar_FXDesktopFallEndFile, 16, 0x446654)
#define g_FallSoundVel GAME_GLOBAL_IN(float, GVar_FallSoundVel, 16, 0x4472e4)		/* FallSoundVel */
#define g_FXBishopSellLifeFile GAME_GLOBAL_IN(char *, GVar_FXBishopSellLifeFile, 16, 0x4464a4)
#define g_FXBishopAttackFile GAME_GLOBAL_IN(char *, GVar_FXBishopAttackFile, 16, 0x4464dc)
#define g_BishopAttackPosX GAME_GLOBAL_IN(float, GVar_BishopAttackPosX, 16, 0x4464f4)
#define g_BishopAttackPosY GAME_GLOBAL_IN(float, GVar_BishopAttackPosY, 16, 0x44650c)
#define g_BishopAmmoCost GAME_GLOBAL_IN(float, GVar_BishopAmmoCost, 16, 0x446554)
#define g_BishopAttackRandOff GAME_GLOBAL_IN(float, GVar_BishopAttackRandOff, 16, 0x44656c)
#define g_BishopShootSoundVol GAME_GLOBAL_IN(float, GVar_BishopShootSoundVol, 16, 0x446584)
#define g_BishopShootSound GAME_GLOBAL_IN(char *, GVar_BishopShootSound, 16, 0x4465ac)
#define g_DTritusHitTimeFader GAME_GLOBAL_IN(float, GVar_DTritusHitTimeFader, 16, 0x446670)
#define g_FXDTritusAttackFile GAME_GLOBAL_IN(char *, GVar_FXDTritusAttackFile, 16, 0x4466c8)
#define g_DTritusHitTimeSpeed GAME_GLOBAL_IN(float, GVar_DTritusHitTimeSpeed, 16, 0x4466e0)
#define g_FXDTritusKillFile GAME_GLOBAL_IN(char *, GVar_FXDTritusKillFile, 16, 0x4466f8)
#define g_GearMissileSoundVol GAME_GLOBAL_IN(float, GVar_GearMissileSoundVol, 16, 0x44686c)
#define g_GearLoadWeaponSound GAME_GLOBAL_IN(char *, GVar_GearLoadWeaponSound, 16, 0x446894)
#define g_GearShootDamage GAME_GLOBAL_IN(int, GVar_GearShootDamage, 16, 0x4468bc)
#define g_GearMissileSound GAME_GLOBAL_IN(char *, GVar_GearMissileSound, 16, 0x446904)
#define g_GearShootPosX GAME_GLOBAL_IN(float, GVar_GearShootPosX, 16, 0x44691c)
#define g_GearShootDistance GAME_GLOBAL_IN(float, GVar_GearShootDistance, 16, 0x446934)
#define g_GearLoadWeaponSoundVol GAME_GLOBAL_IN(float, GVar_GearLoadWeaponSoundVol, 16, 0x44694c)
#define g_GearHitSound GAME_GLOBAL_IN(char *, GVar_GearHitSound, 16, 0x446994)
#define g_GearHitSoundVol GAME_GLOBAL_IN(float, GVar_GearHitSoundVol, 16, 0x4469ac)
#define g_GearShootPosZ GAME_GLOBAL_IN(float, GVar_GearShootPosZ, 16, 0x4469c4)
#define g_GearShootFailAlarm GAME_GLOBAL_IN(float, GVar_GearShootFailAlarm, 16, 0x4469dc)
#define g_GearShootHitAlarm GAME_GLOBAL_IN(float, GVar_GearShootHitAlarm, 16, 0x4469f4)
#define g_GearMissileSpeed GAME_GLOBAL_IN(float, GVar_GearMissileSpeed, 16, 0x446a24)
#define g_GearShootSound GAME_GLOBAL_IN(char *, GVar_GearShootSound, 16, 0x446a54)
#define g_GearShootSoundVol GAME_GLOBAL_IN(float, GVar_GearShootSoundVol, 16, 0x446a6c)
#define g_GearShootPosY GAME_GLOBAL_IN(float, GVar_GearShootPosY, 16, 0x446a84)
#define g_GearXplodeRadius GAME_GLOBAL_IN(float, GVar_GearXplodeRadius, 16, 0x446a9c)
#define g_GearLowWeaponEnergy GAME_GLOBAL_IN(float, GVar_GearLowWeaponEnergy, 16, 0x446ac0)
#define g_FXMaintenanceWorkFile GAME_GLOBAL_IN(char *, GVar_FXMaintenanceWorkFile, 16, 0x446b8c)
#define g_GearTargetName GAME_GLOBAL_IN(char *, GVar_GearTargetName, 16, 0x460664)	/* GearTargetName: the target of the missile of the player */

/* the characters of the running actions (the "MeEnt" CHECKs) */
#define g_BankMasterEnt GAME_GLOBAL(cEntity *, g_BankMasterEnt, 0x4460a0)
#define g_BertoEnt GAME_GLOBAL(cEntity *, g_BertoEnt, 0x4461ac)
#define g_BettyEnt GAME_GLOBAL(cEntity *, g_BettyEnt, 0x4462a0)
#define g_DtritusEnt GAME_GLOBAL(cEntity *, g_DtritusEnt, 0x44665c)
/* the gear natives: the name of the shooting gear (also the one the ball does not hurt), the missile, its point */
#define g_GearTarget (GAME_ARRAY(char, g_GearTarget, 0x446758))		/* [0x80] */
#define g_GearSwName (GAME_ARRAY(char, g_GearSwName, 0x4467d8))		/* [0x80] */
#define g_GearSw GAME_GLOBAL(cEntity *, g_GearSw, 0x446858)
#define g_GearXplodePos GAME_GLOBAL(vec3_t, g_GearXplodePos, 0x446aa4)
/* Logic_Flash: the point, the most characters to flash (-1 all), the count and the result */
#define g_FlashPos GAME_GLOBAL(vec3_t, g_FlashPos, 0x446ae8)
#define g_FlashResult GAME_GLOBAL(int, g_FlashResult, 0x446b18)
#define g_FlashMax GAME_GLOBAL(int, g_FlashMax, 0x446b1c)
#define g_FlashCount GAME_GLOBAL(int, g_FlashCount, 0x446b20)
#define FLASH_ANGLE 1.5707963705062866		/* rva 0x3aeb90 */

/* engine side */
/* the name of the action of layer i, "" when it does not run (rva 0x62c40) */
const char *__thiscall cActCtrl_GetActionName(void *ctrl, int i);
/* the weapon energy of a character, 0 or more (rva 0xd4bc0) */
void __thiscall cWalkCharEntity_SetEnergy(cEntity *self, float energy);
/* the action, unless it runs already in its layer; 0 then or without it (rva 0x631c0) */
byte __thiscall cActCtrl_PlayNew(void *ctrl, const char *action, int arg, float speed);
/* the falling state of a character (0x464), with what goes with it (rva 0xd4c60) */
void __thiscall cWalkCharEntity_SetFalling(cEntity *self, byte on);

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
void ChType_BishopAttack(cAbstractFile *params);

/* CrazyGambler.c */
void ChType_FXCrazyGamblerSetHeadModel(cAbstractFile *params);

/* Desktop.c */
void ChType_DesktopActionPress(cAbstractFile *params);
void ChType_FXDesktopFallEnd(cAbstractFile *params);

/* Dtritus.c */
void ChType_DtritusActionPress(cAbstractFile *params);
void ChType_DtritusKill(cAbstractFile *params);
void ChType_DtritusEndEvent(cAbstractFile *params);
void ChType_DtritusHitEvent(cAbstractFile *params);
void ChType_DtritusRestartEvent(cAbstractFile *params);
void ChType_FXDTritusAttack(cAbstractFile *params);

/* Gear.c */
void ChType_GearHitChar(const char *name, byte noLogic);
void ChType_GearActionPress(cAbstractFile *params);
void ChType_GearEndAction(cAbstractFile *params);
void ChType_GearSphereHit(cAbstractFile *params);
void ChType_GearHit(cAbstractFile *params);
void ChType_GearInitAction(cAbstractFile *params);
void ChType_GearShoot(cAbstractFile *params);
/* "!GearShootLoad": params the gear, an event and the delay of the end flashes; the charge on its shot particles */
void ChType_GearShootLoad(cAbstractFile *params);
int ChType_FlashChar(cEntity *e);
void ChType_FlashHit(cAbstractFile *params);

/* Maintenance.c */
void ChType_FXMaintenanceWork(cAbstractFile *params);
/* an inline of the local logic of a character (rva 0xc9750): it is paused (+0x28) or +0x2a is set */
byte __fastcall cAIChar_TestFlags(const void *self);

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

/* ---- part 2: Mayor .. Sentinel ---- */

/* the class name of the action class of a character */
static inline const char *ChType_ClassName(cEntity *e) {
	return ChType_ActClass(ChType_ActCtrl(e))->name;
}

#define WChar_DisplOn(e) (WChar(e)->disableControl)		/* "DisableControl": the slot only turns the body */
/* a flag of the AI controller of the world (aiCtrl + 0x31): with the alarm on the sentinels call no others */
#define World_AlarmChase(w) ((w)->logic.aiCtrl.accusing)

/* the natives one native binds to a sphere test or an OnTake (rva, the same pointer as the original) */
#define CHTYPE_MAYORSPHEREHIT 0x0c9c20
#define CHTYPE_MESSENGERTAKEDATAPACK 0x0caa80
#define CHTYPE_NURSESPHEREHIT 0x0caec0
#define CHTYPE_POLICEBOSSBRUPBOUNCE 0x0ce470
#define CHTYPE_POLICEBOSSSPHEREHIT 0x0ce820

/* engine variables (Gvars) */
#define g_MayorBlabliblaRadius GAME_GLOBAL_IN(float, GVar_MayorBlabliblaRadius, 16, 0x446c10)
#define g_MayorListenPercent GAME_GLOBAL_IN(float, GVar_MayorListenPercent, 16, 0x446be0)
#define g_MayorSleepPercent GAME_GLOBAL_IN(float, GVar_MayorSleepPercent, 16, 0x446bf8)
#define g_FXMercenaryDrinkFile GAME_GLOBAL_IN(char *, GVar_FXMercenaryDrinkFile, 16, 0x446c58)
#define g_MercenaryDrinkPosX GAME_GLOBAL_IN(float, GVar_MercenaryDrinkPosX, 16, 0x446c80)
#define g_MercenaryDrinkPosY GAME_GLOBAL_IN(float, GVar_MercenaryDrinkPosY, 16, 0x446c40)
#define g_MercenaryDrinkPosZ GAME_GLOBAL_IN(float, GVar_MercenaryDrinkPosZ, 16, 0x446c98)
#define g_FXMessengerAttackFile GAME_GLOBAL_IN(char *, GVar_FXMessengerAttackFile, 16, 0x446d08)
#define g_FXMessengerTakeDataPackFile GAME_GLOBAL_IN(char *, GVar_FXMessengerTakeDataPackFile, 16, 0x446d48)
#define g_MessengerAmmoCost GAME_GLOBAL_IN(float, GVar_MessengerAmmoCost, 16, 0x446cd8)
#define g_MessengerAttackPosX GAME_GLOBAL_IN(float, GVar_MessengerAttackPosX, 16, 0x446cf0)
#define g_MessengerAttackPosY GAME_GLOBAL_IN(float, GVar_MessengerAttackPosY, 16, 0x446d60)
#define g_MessengerAttackPosZ GAME_GLOBAL_IN(float, GVar_MessengerAttackPosZ, 16, 0x446cb0)
#define g_MessengerShootSound GAME_GLOBAL_IN(char *, GVar_MessengerShootSound, 16, 0x446d90)
#define g_MessengerShootSoundVol GAME_GLOBAL_IN(float, GVar_MessengerShootSoundVol, 16, 0x446d78)
#define g_MessengerTakeDataPackSound GAME_GLOBAL_IN(char *, GVar_MessengerTakeDataPackSound, 16, 0x446d30)
#define g_MessengerTakeDataPackSoundVol GAME_GLOBAL_IN(float, GVar_MessengerTakeDataPackSoundVol, 16, 0x446da8)
#define g_MessengerTakenCallback (GAME_ARRAY_IN(cPy_Function, GVar_MessengerTakenCallback, 16, 0x44d4bc))	/* "MessengerTakenCallback" */
#define g_MessengerDataPack GAME_GLOBAL_IN(char *, GVar_MessengerDataPack, 16, 0x44d54c)	/* "MessengerDataPack": the name of the data pack */
#define g_FXHammerHitFile GAME_GLOBAL_IN(char *, GVar_FXHammerHitFile, 16, 0x446f18)
#define g_NurseAlarmHit GAME_GLOBAL_IN(float, GVar_NurseAlarmHit, 16, 0x446e98)
#define g_NurseDispDist GAME_GLOBAL_IN(float, GVar_NurseDispDist, 16, 0x446eb0)
#define g_NurseDispRad GAME_GLOBAL_IN(float, GVar_NurseDispRad, 16, 0x446f30)
#define g_OnSleep (GAME_ARRAY_IN(cPy_Function, GVar_OnSleep, 16, 0x446e74))		/* "OnSleep" */
#define g_FXPoliceGearConversionFile GAME_GLOBAL_IN(char *, GVar_FXPoliceGearConversionFile, 16, 0x446fa0)
#define g_FXPoliceStealFile GAME_GLOBAL_IN(char *, GVar_FXPoliceStealFile, 16, 0x446f88)
#define g_PoliceAlarmStepSTM GAME_GLOBAL_IN(float, GVar_PoliceAlarmStepSTM, 16, 0x446fc8)
#define g_PoliceStealAmount GAME_GLOBAL_IN(int, GVar_PoliceStealAmount, 16, 0x446ff0)
#define g_PoliceBossActionRadius GAME_GLOBAL_IN(float, GVar_PoliceBossActionRadius, 16, 0x44708c)
#define g_PoliceBossBrupBaseFile GAME_GLOBAL_IN(char *, GVar_PoliceBossBrupBaseFile, 16, 0x44700c)
#define g_PoliceBossBrupBounce1Sound GAME_GLOBAL_IN(char *, GVar_PoliceBossBrupBounce1Sound, 16, 0x44705c)
#define g_PoliceBossBrupBounce2Sound GAME_GLOBAL_IN(char *, GVar_PoliceBossBrupBounce2Sound, 16, 0x4470ec)
#define g_PoliceBossBrupBounceSoundVol GAME_GLOBAL_IN(float, GVar_PoliceBossBrupBounceSoundVol, 16, 0x4470d4)
#define g_PoliceBossBrupDebrisMax GAME_GLOBAL_IN(int, GVar_PoliceBossBrupDebrisMax, 16, 0x447024)
#define g_PoliceBossBrupDebrisMin GAME_GLOBAL_IN(int, GVar_PoliceBossBrupDebrisMin, 16, 0x44711c)
#define g_PoliceBossBrupDirRand GAME_GLOBAL_IN(float, GVar_PoliceBossBrupDirRand, 16, 0x447074)
#define g_PoliceBossBrupPosX GAME_GLOBAL_IN(float, GVar_PoliceBossBrupPosX, 16, 0x4470bc)
#define g_PoliceBossBrupPosY GAME_GLOBAL_IN(float, GVar_PoliceBossBrupPosY, 16, 0x4470a4)
#define g_PoliceBossBrupPosZ GAME_GLOBAL_IN(float, GVar_PoliceBossBrupPosZ, 16, 0x447104)
#define g_FXRustyHitFile GAME_GLOBAL_IN(char *, GVar_FXRustyHitFile, 16, 0x44719c)
#define g_RustyHitPosX GAME_GLOBAL_IN(float, GVar_RustyHitPosX, 16, 0x447144)
#define g_RustyHitPosY GAME_GLOBAL_IN(float, GVar_RustyHitPosY, 16, 0x447184)
#define g_RustyHitPosZ GAME_GLOBAL_IN(float, GVar_RustyHitPosZ, 16, 0x44715c)
#define g_FXSebastianFlashFile GAME_GLOBAL_IN(char *, GVar_FXSebastianFlashFile, 16, 0x4471c4)

/* the characters of the running actions (the "MeEnt" CHECKs), the nurse by name and the character it hits */
#define g_MayorEnt GAME_GLOBAL(cEntity *, g_MayorEnt, 0x446bac)
#define g_PoliceBossEnt GAME_GLOBAL(cEntity *, g_PoliceBossEnt, 0x446ff8)
#define g_NurseName (GAME_ARRAY(char, g_NurseName, 0x446dd0))		/* [0x80] */
#define g_NurseTarget GAME_GLOBAL(cEntity *, g_NurseTarget, 0x446e50)

/* engine side */
/* the action of the controller (not when it runs already in its layer and one of priority runs); 0 if not */
byte __thiscall cActCtrl_PlayAction(void *ctrl, void *action, int arg, int set, float speed);
/* the name of the listener into the mayor logic (NULL clears it, rva 0x153010) */
void __thiscall cMayorLocalLogic_SetListener(cLocalLogic *self, cEntity *e);
/* the view of a character set to the direction from pos to it (rva 0xd5240) */
void __thiscall cWalkCharEntity_SetViewFrom(cEntity *self, const vec3_t *pos);

/* Mayor.c */
void ChType_ListeningMayor(cAbstractFile *params);
void ChType_MayorEndAction(cAbstractFile *params);
void ChType_MayorSphereHit(cAbstractFile *params);

/* Mercenary.c */
void ChType_FXMercenaryDrink(cAbstractFile *params);

/* Messenger.c (MessengerActionPress is ChType_ActionPress, MessengerEndActionEnd ChType_EndActionEnd) */
void ChType_MessengerAttack(cAbstractFile *params);
void ChType_MessengerTakeDataPack(cAbstractFile *params);

/* Nurse.c */
void ChType_NurseSphereHit(cAbstractFile *params);
/* the hammer hit effect at the nurse v: a glow, a ring of stars and sparks (the debris at the camera) */
void Nurse_HammerFX(cEntity *v);
void ChType_NurseActionPress(cAbstractFile *params);
void ChType_NurseActionRelease(cAbstractFile *params);
void ChType_NurseHit(void);
void ChType_NurseBackHitControl(cAbstractFile *params);
void ChType_NurseDoHammerHit(cAbstractFile *params);
void ChType_NurseInitHammerLoop(cAbstractFile *params);
void ChType_NurseTryHammerAgain(cAbstractFile *params);

/* Police.c */
void ChType_FXPoliceGearConversion(cAbstractFile *params);
void ChType_FXPoliceSteal(cAbstractFile *params);
void FX_PoliceSteal(cEntity *attacker, cEntity *attacked);
void ChType_ListeningPolice(cAbstractFile *params);
byte ChType_PoliceFindListener(cEntity *v, cEntity **victim, byte *usr, void **action);
void ChType_PoliceActionPress(cAbstractFile *params);
void ChType_PoliceEndActionMoney(cAbstractFile *params);
void ChType_PoliceRestoreFreeTurn(cAbstractFile *params);

/* PoliceBoss.c */
void ChType_FXPoliceBossBrup(cAbstractFile *params);
void ChType_PoliceBossBrupBounce(cDebris *d, const byte *hit);
void ChType_PoliceBossAction(cAbstractFile *params);
void ChType_PoliceBossTryAction(cEntity *v);
void ChType_PoliceBossBrup(cAbstractFile *params);
void ChType_PoliceBossSphereHit(cAbstractFile *params);

/* Rusty.c */
void ChType_FXRustyHit(cAbstractFile *params);

/* Sebastian.c */
void ChType_FXSebastianFlash(cAbstractFile *params);

/* Sentinel.c */
void ChType_SentinelActionPress(cAbstractFile *params);
void ChType_SentinelActionRelease(cAbstractFile *params);
cEntity *ChType_SentinelTarget(cEntity *e);
byte ChType_SentinelCanCall(void);
void ChType_SentinelPoint(cAbstractFile *params);

#endif
