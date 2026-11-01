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
#define WChar_FX(e) ((void *)WChar(e)->wl.obj.fx)		/* the embedded effect */
#define WChar_Listen(e) ((cEntity *)WChar(e)->listen.target)		/* its target: the mayor, the police */
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

/* the fields of the local logic of a character (cBaseLocalLogic, Logic/BaseLogic.cpp) */
#define ChLogic_Target(l) ((cEntity *)((cBaseLocalLogic *)(l))->target.target)	/* of the state */
#define ChLogic_Idle(l) (((cBaseLocalLogic *)(l))->idle)
#define ChLogic_AlwaysLock(l) (((cBaseLocalLogic *)(l))->alwaysLock)
#define ChLogic_PoliceResult(l) (((cLittlePoliceLocalLogic *)(l))->policeResult)	/* of a police logic */

/* the action of layer 0 of the controller */
#define ChType_Action0(ctrl) ((void *)((cActCtrl *)(ctrl))->layers[0].action)

/* the C function of the sphere tests of the natives (also g_DoorSphereFunc, g_InfernoSphereFunc) */
#define g_ChTypeSphereFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0))
#define g_OnFlash (GAME_ARRAY_IN(cPy_Function, GVar_OnFlash, 16))		/* OnFlash, the C function of the flashes */

/* the natives one native calls through a sphere or flash test (rva, the same pointer as the original) */
#define CHTYPE_BANKMASTERSUCTION 0x0c1a70
#define CHTYPE_BERTOFLASHHIT 0x0c2480
#define CHTYPE_BETTYKILL 0x0c2c80
#define CHTYPE_DTRITUSKILL 0x0c5cd0
#define CHTYPE_GEARHIT 0x0c7020
#define CHTYPE_GEARSPHEREHIT 0x0c9130
#define CHTYPE_FLASHHIT 0x0c92f0

/* engine variables (Gvars) */
#define g_FXBankerStealFile GAME_GLOBAL_IN(char *, GVar_FXBankerStealFile, 16)
#define g_BankerStealAmount GAME_GLOBAL_IN(int, GVar_BankerStealAmount, 16)
#define g_FXBankMasterAttackFile GAME_GLOBAL_IN(char *, GVar_FXBankMasterAttackFile, 16)
#define g_BankMasterVacumDist GAME_GLOBAL_IN(float, GVar_BankMasterVacumDist, 16)
#define g_BankMasterAmmoCost GAME_GLOBAL_IN(float, GVar_BankMasterAmmoCost, 16)
#define g_FXBankMasterSuctionFile GAME_GLOBAL_IN(char *, GVar_FXBankMasterSuctionFile, 16)
#define g_FXBertoFlashFile GAME_GLOBAL_IN(char *, GVar_FXBertoFlashFile, 16)
#define g_BertoFlashSound GAME_GLOBAL_IN(char *, GVar_BertoFlashSound, 16)
#define g_BertoFlashDist GAME_GLOBAL_IN(float, GVar_BertoFlashDist, 16)
#define g_BertoFlashSoundVol GAME_GLOBAL_IN(float, GVar_BertoFlashSoundVol, 16)
#define g_BertoAmmoCost GAME_GLOBAL_IN(float, GVar_BertoAmmoCost, 16)
#define g_FXBertoAttackFile GAME_GLOBAL_IN(char *, GVar_FXBertoAttackFile, 16)
#define g_BettyHitTimeFader GAME_GLOBAL_IN(float, GVar_BettyHitTimeFader, 16)
#define g_FXBettyAttackFile GAME_GLOBAL_IN(char *, GVar_FXBettyAttackFile, 16)
#define g_BettyHSpeed GAME_GLOBAL_IN(float, GVar_BettyHSpeed, 16)
#define g_BettyHitPosY GAME_GLOBAL_IN(float, GVar_BettyHitPosY, 16)
#define g_BettyHitRadius GAME_GLOBAL_IN(float, GVar_BettyHitRadius, 16)
#define g_BettyVSpeed GAME_GLOBAL_IN(float, GVar_BettyVSpeed, 16)
#define g_FXBettyKillFile GAME_GLOBAL_IN(char *, GVar_FXBettyKillFile, 16)
#define g_BettyHitTopLimit GAME_GLOBAL_IN(float, GVar_BettyHitTopLimit, 16)
#define g_BettyHitPosX GAME_GLOBAL_IN(float, GVar_BettyHitPosX, 16)
#define g_BettyHitTimeSpeed GAME_GLOBAL_IN(float, GVar_BettyHitTimeSpeed, 16)
#define g_BettyHitPosZ GAME_GLOBAL_IN(float, GVar_BettyHitPosZ, 16)
#define g_BishopHitSound GAME_GLOBAL_IN(char *, GVar_BishopHitSound, 16)
#define g_BishopAngleVariation GAME_GLOBAL_IN(float, GVar_BishopAngleVariation, 16)
#define g_BishopHitSoundVol GAME_GLOBAL_IN(float, GVar_BishopHitSoundVol, 16)
#define g_BishopWeaponDamage GAME_GLOBAL_IN(int, GVar_BishopWeaponDamage, 16)
#define g_BishopAttackPosZ GAME_GLOBAL_IN(float, GVar_BishopAttackPosZ, 16)
#define g_DesktopJumpPrice GAME_GLOBAL_IN(int, GVar_DesktopJumpPrice, 16)
#define g_DesktopJumpSlideTime GAME_GLOBAL_IN(float, GVar_DesktopJumpSlideTime, 16)
#define g_DesktopJumpSpeed GAME_GLOBAL_IN(float, GVar_DesktopJumpSpeed, 16)
#define g_FXDesktopFallEndDist GAME_GLOBAL_IN(float, GVar_FXDesktopFallEndDist, 16)
#define g_FXDesktopFallEndFile GAME_GLOBAL_IN(char *, GVar_FXDesktopFallEndFile, 16)
#define g_FallSoundVel GAME_GLOBAL_IN(float, GVar_FallSoundVel, 16)		/* FallSoundVel */
#define g_FXBishopSellLifeFile GAME_GLOBAL_IN(char *, GVar_FXBishopSellLifeFile, 16)
#define g_FXBishopAttackFile GAME_GLOBAL_IN(char *, GVar_FXBishopAttackFile, 16)
#define g_BishopAttackPosX GAME_GLOBAL_IN(float, GVar_BishopAttackPosX, 16)
#define g_BishopAttackPosY GAME_GLOBAL_IN(float, GVar_BishopAttackPosY, 16)
#define g_BishopAmmoCost GAME_GLOBAL_IN(float, GVar_BishopAmmoCost, 16)
#define g_BishopAttackRandOff GAME_GLOBAL_IN(float, GVar_BishopAttackRandOff, 16)
#define g_BishopShootSoundVol GAME_GLOBAL_IN(float, GVar_BishopShootSoundVol, 16)
#define g_BishopShootSound GAME_GLOBAL_IN(char *, GVar_BishopShootSound, 16)
#define g_DTritusHitTimeFader GAME_GLOBAL_IN(float, GVar_DTritusHitTimeFader, 16)
#define g_FXDTritusAttackFile GAME_GLOBAL_IN(char *, GVar_FXDTritusAttackFile, 16)
#define g_DTritusHitTimeSpeed GAME_GLOBAL_IN(float, GVar_DTritusHitTimeSpeed, 16)
#define g_FXDTritusKillFile GAME_GLOBAL_IN(char *, GVar_FXDTritusKillFile, 16)
#define g_GearMissileSoundVol GAME_GLOBAL_IN(float, GVar_GearMissileSoundVol, 16)
#define g_GearLoadWeaponSound GAME_GLOBAL_IN(char *, GVar_GearLoadWeaponSound, 16)
#define g_GearShootDamage GAME_GLOBAL_IN(int, GVar_GearShootDamage, 16)
#define g_GearMissileSound GAME_GLOBAL_IN(char *, GVar_GearMissileSound, 16)
#define g_GearShootPosX GAME_GLOBAL_IN(float, GVar_GearShootPosX, 16)
#define g_GearShootDistance GAME_GLOBAL_IN(float, GVar_GearShootDistance, 16)
#define g_GearLoadWeaponSoundVol GAME_GLOBAL_IN(float, GVar_GearLoadWeaponSoundVol, 16)
#define g_GearHitSound GAME_GLOBAL_IN(char *, GVar_GearHitSound, 16)
#define g_GearHitSoundVol GAME_GLOBAL_IN(float, GVar_GearHitSoundVol, 16)
#define g_GearShootPosZ GAME_GLOBAL_IN(float, GVar_GearShootPosZ, 16)
#define g_GearShootFailAlarm GAME_GLOBAL_IN(float, GVar_GearShootFailAlarm, 16)
#define g_GearShootHitAlarm GAME_GLOBAL_IN(float, GVar_GearShootHitAlarm, 16)
#define g_GearMissileSpeed GAME_GLOBAL_IN(float, GVar_GearMissileSpeed, 16)
#define g_GearShootSound GAME_GLOBAL_IN(char *, GVar_GearShootSound, 16)
#define g_GearShootSoundVol GAME_GLOBAL_IN(float, GVar_GearShootSoundVol, 16)
#define g_GearShootPosY GAME_GLOBAL_IN(float, GVar_GearShootPosY, 16)
#define g_GearXplodeRadius GAME_GLOBAL_IN(float, GVar_GearXplodeRadius, 16)
#define g_GearLowWeaponEnergy GAME_GLOBAL_IN(float, GVar_GearLowWeaponEnergy, 16)
#define g_FXMaintenanceWorkFile GAME_GLOBAL_IN(char *, GVar_FXMaintenanceWorkFile, 16)
#define g_GearTargetName GAME_GLOBAL_IN(char *, GVar_GearTargetName, 16)	/* GearTargetName: the target of the missile of the player */

/* the characters of the running actions (the "MeEnt" CHECKs) */
#define g_BankMasterEnt GAME_GLOBAL(cEntity *, g_BankMasterEnt)
#define g_BertoEnt GAME_GLOBAL(cEntity *, g_BertoEnt)
#define g_BettyEnt GAME_GLOBAL(cEntity *, g_BettyEnt)
#define g_DtritusEnt GAME_GLOBAL(cEntity *, g_DtritusEnt)
/* the gear natives: the name of the shooting gear (also the one the ball does not hurt), the missile, its point */
#define g_GearTarget (GAME_ARRAY(char, g_GearTarget))		/* [0x80] */
#define g_GearSwName (GAME_ARRAY(char, g_GearSwName))		/* [0x80] */
#define g_GearSw GAME_GLOBAL(cEntity *, g_GearSw)
#define g_GearXplodePos GAME_GLOBAL(vec3_t, g_GearXplodePos)
/* Logic_Flash: the point, the most characters to flash (-1 all), the count and the result */
#define g_FlashPos GAME_GLOBAL(vec3_t, g_FlashPos)
#define g_FlashResult GAME_GLOBAL(int, g_FlashResult)
#define g_FlashMax GAME_GLOBAL(int, g_FlashMax)
#define g_FlashCount GAME_GLOBAL(int, g_FlashCount)
#define FLASH_ANGLE 1.5707963705062866

/* engine side */

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
/* an inline of the local logic of a character: it is paused (+0x28) or +0x2a is set */
byte cAIChar_TestFlags(const void *self);

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

/* a flag of the AI controller of the world (aiCtrl + 0x31): with the alarm on the sentinels call no others */
#define World_AlarmChase(w) ((w)->logic.aiCtrl.accusing)

/* the natives one native binds to a sphere test or an OnTake (rva, the same pointer as the original) */
#define CHTYPE_MAYORSPHEREHIT 0x0c9c20
#define CHTYPE_MESSENGERTAKEDATAPACK 0x0caa80
#define CHTYPE_NURSESPHEREHIT 0x0caec0
#define CHTYPE_POLICEBOSSBRUPBOUNCE 0x0ce470
#define CHTYPE_POLICEBOSSSPHEREHIT 0x0ce820

/* engine variables (Gvars) */
#define g_MayorBlabliblaRadius GAME_GLOBAL_IN(float, GVar_MayorBlabliblaRadius, 16)
#define g_MayorListenPercent GAME_GLOBAL_IN(float, GVar_MayorListenPercent, 16)
#define g_MayorSleepPercent GAME_GLOBAL_IN(float, GVar_MayorSleepPercent, 16)
#define g_FXMercenaryDrinkFile GAME_GLOBAL_IN(char *, GVar_FXMercenaryDrinkFile, 16)
#define g_MercenaryDrinkPosX GAME_GLOBAL_IN(float, GVar_MercenaryDrinkPosX, 16)
#define g_MercenaryDrinkPosY GAME_GLOBAL_IN(float, GVar_MercenaryDrinkPosY, 16)
#define g_MercenaryDrinkPosZ GAME_GLOBAL_IN(float, GVar_MercenaryDrinkPosZ, 16)
#define g_FXMessengerAttackFile GAME_GLOBAL_IN(char *, GVar_FXMessengerAttackFile, 16)
#define g_FXMessengerTakeDataPackFile GAME_GLOBAL_IN(char *, GVar_FXMessengerTakeDataPackFile, 16)
#define g_MessengerAmmoCost GAME_GLOBAL_IN(float, GVar_MessengerAmmoCost, 16)
#define g_MessengerAttackPosX GAME_GLOBAL_IN(float, GVar_MessengerAttackPosX, 16)
#define g_MessengerAttackPosY GAME_GLOBAL_IN(float, GVar_MessengerAttackPosY, 16)
#define g_MessengerAttackPosZ GAME_GLOBAL_IN(float, GVar_MessengerAttackPosZ, 16)
#define g_MessengerShootSound GAME_GLOBAL_IN(char *, GVar_MessengerShootSound, 16)
#define g_MessengerShootSoundVol GAME_GLOBAL_IN(float, GVar_MessengerShootSoundVol, 16)
#define g_MessengerTakeDataPackSound GAME_GLOBAL_IN(char *, GVar_MessengerTakeDataPackSound, 16)
#define g_MessengerTakeDataPackSoundVol GAME_GLOBAL_IN(float, GVar_MessengerTakeDataPackSoundVol, 16)
#define g_MessengerTakenCallback (GAME_ARRAY_IN(cPy_Function, GVar_MessengerTakenCallback, 16))	/* "MessengerTakenCallback" */
#define g_MessengerDataPack GAME_GLOBAL_IN(char *, GVar_MessengerDataPack, 16)	/* "MessengerDataPack": the name of the data pack */
#define g_FXHammerHitFile GAME_GLOBAL_IN(char *, GVar_FXHammerHitFile, 16)
#define g_NurseAlarmHit GAME_GLOBAL_IN(float, GVar_NurseAlarmHit, 16)
#define g_NurseDispDist GAME_GLOBAL_IN(float, GVar_NurseDispDist, 16)
#define g_NurseDispRad GAME_GLOBAL_IN(float, GVar_NurseDispRad, 16)
#define g_OnSleep (GAME_ARRAY_IN(cPy_Function, GVar_OnSleep, 16))		/* "OnSleep" */
#define g_FXPoliceGearConversionFile GAME_GLOBAL_IN(char *, GVar_FXPoliceGearConversionFile, 16)
#define g_FXPoliceStealFile GAME_GLOBAL_IN(char *, GVar_FXPoliceStealFile, 16)
#define g_PoliceAlarmStepSTM GAME_GLOBAL_IN(float, GVar_PoliceAlarmStepSTM, 16)
#define g_PoliceStealAmount GAME_GLOBAL_IN(int, GVar_PoliceStealAmount, 16)
#define g_PoliceBossActionRadius GAME_GLOBAL_IN(float, GVar_PoliceBossActionRadius, 16)
#define g_PoliceBossBrupBaseFile GAME_GLOBAL_IN(char *, GVar_PoliceBossBrupBaseFile, 16)
#define g_PoliceBossBrupBounce1Sound GAME_GLOBAL_IN(char *, GVar_PoliceBossBrupBounce1Sound, 16)
#define g_PoliceBossBrupBounce2Sound GAME_GLOBAL_IN(char *, GVar_PoliceBossBrupBounce2Sound, 16)
#define g_PoliceBossBrupBounceSoundVol GAME_GLOBAL_IN(float, GVar_PoliceBossBrupBounceSoundVol, 16)
#define g_PoliceBossBrupDebrisMax GAME_GLOBAL_IN(int, GVar_PoliceBossBrupDebrisMax, 16)
#define g_PoliceBossBrupDebrisMin GAME_GLOBAL_IN(int, GVar_PoliceBossBrupDebrisMin, 16)
#define g_PoliceBossBrupDirRand GAME_GLOBAL_IN(float, GVar_PoliceBossBrupDirRand, 16)
#define g_PoliceBossBrupPosX GAME_GLOBAL_IN(float, GVar_PoliceBossBrupPosX, 16)
#define g_PoliceBossBrupPosY GAME_GLOBAL_IN(float, GVar_PoliceBossBrupPosY, 16)
#define g_PoliceBossBrupPosZ GAME_GLOBAL_IN(float, GVar_PoliceBossBrupPosZ, 16)
#define g_FXRustyHitFile GAME_GLOBAL_IN(char *, GVar_FXRustyHitFile, 16)
#define g_RustyHitPosX GAME_GLOBAL_IN(float, GVar_RustyHitPosX, 16)
#define g_RustyHitPosY GAME_GLOBAL_IN(float, GVar_RustyHitPosY, 16)
#define g_RustyHitPosZ GAME_GLOBAL_IN(float, GVar_RustyHitPosZ, 16)
#define g_FXSebastianFlashFile GAME_GLOBAL_IN(char *, GVar_FXSebastianFlashFile, 16)

/* the characters of the running actions (the "MeEnt" CHECKs), the nurse by name and the character it hits */
#define g_MayorEnt GAME_GLOBAL(cEntity *, g_MayorEnt)
#define g_PoliceBossEnt GAME_GLOBAL(cEntity *, g_PoliceBossEnt)
#define g_NurseName (GAME_ARRAY(char, g_NurseName))		/* [0x80] */
#define g_NurseTarget GAME_GLOBAL(cEntity *, g_NurseTarget)

/* engine side */
/* the name of the listener into the mayor logic (NULL clears it, rva 0x153010) */
void cMayorLocalLogic_SetListener(cLocalLogic *self, cEntity *e);

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
