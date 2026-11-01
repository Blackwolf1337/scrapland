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

#include "scrap/gvar.h"
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
extern cGVar GVar_FXBankerStealFile;
#define g_FXBankerStealFile (GVar_FXBankerStealFile.value.s)
extern cGVar GVar_BankerStealAmount;
#define g_BankerStealAmount (GVar_BankerStealAmount.value.i)
extern cGVar GVar_FXBankMasterAttackFile;
#define g_FXBankMasterAttackFile (GVar_FXBankMasterAttackFile.value.s)
extern cGVar GVar_BankMasterVacumDist;
#define g_BankMasterVacumDist (GVar_BankMasterVacumDist.value.f)
extern cGVar GVar_BankMasterAmmoCost;
#define g_BankMasterAmmoCost (GVar_BankMasterAmmoCost.value.f)
extern cGVar GVar_FXBankMasterSuctionFile;
#define g_FXBankMasterSuctionFile (GVar_FXBankMasterSuctionFile.value.s)
extern cGVar GVar_FXBertoFlashFile;
#define g_FXBertoFlashFile (GVar_FXBertoFlashFile.value.s)
extern cGVar GVar_BertoFlashSound;
#define g_BertoFlashSound (GVar_BertoFlashSound.value.s)
extern cGVar GVar_BertoFlashDist;
#define g_BertoFlashDist (GVar_BertoFlashDist.value.f)
extern cGVar GVar_BertoFlashSoundVol;
#define g_BertoFlashSoundVol (GVar_BertoFlashSoundVol.value.f)
extern cGVar GVar_BertoAmmoCost;
#define g_BertoAmmoCost (GVar_BertoAmmoCost.value.f)
extern cGVar GVar_FXBertoAttackFile;
#define g_FXBertoAttackFile (GVar_FXBertoAttackFile.value.s)
extern cGVar GVar_BettyHitTimeFader;
#define g_BettyHitTimeFader (GVar_BettyHitTimeFader.value.f)
extern cGVar GVar_FXBettyAttackFile;
#define g_FXBettyAttackFile (GVar_FXBettyAttackFile.value.s)
extern cGVar GVar_BettyHSpeed;
#define g_BettyHSpeed (GVar_BettyHSpeed.value.f)
extern cGVar GVar_BettyHitPosY;
#define g_BettyHitPosY (GVar_BettyHitPosY.value.f)
extern cGVar GVar_BettyHitRadius;
#define g_BettyHitRadius (GVar_BettyHitRadius.value.f)
extern cGVar GVar_BettyVSpeed;
#define g_BettyVSpeed (GVar_BettyVSpeed.value.f)
extern cGVar GVar_FXBettyKillFile;
#define g_FXBettyKillFile (GVar_FXBettyKillFile.value.s)
extern cGVar GVar_BettyHitTopLimit;
#define g_BettyHitTopLimit (GVar_BettyHitTopLimit.value.f)
extern cGVar GVar_BettyHitPosX;
#define g_BettyHitPosX (GVar_BettyHitPosX.value.f)
extern cGVar GVar_BettyHitTimeSpeed;
#define g_BettyHitTimeSpeed (GVar_BettyHitTimeSpeed.value.f)
extern cGVar GVar_BettyHitPosZ;
#define g_BettyHitPosZ (GVar_BettyHitPosZ.value.f)
extern cGVar GVar_BishopHitSound;
#define g_BishopHitSound (GVar_BishopHitSound.value.s)
extern cGVar GVar_BishopAngleVariation;
#define g_BishopAngleVariation (GVar_BishopAngleVariation.value.f)
extern cGVar GVar_BishopHitSoundVol;
#define g_BishopHitSoundVol (GVar_BishopHitSoundVol.value.f)
extern cGVar GVar_BishopWeaponDamage;
#define g_BishopWeaponDamage (GVar_BishopWeaponDamage.value.i)
extern cGVar GVar_BishopAttackPosZ;
#define g_BishopAttackPosZ (GVar_BishopAttackPosZ.value.f)
extern cGVar GVar_DesktopJumpPrice;
#define g_DesktopJumpPrice (GVar_DesktopJumpPrice.value.i)
extern cGVar GVar_DesktopJumpSlideTime;
#define g_DesktopJumpSlideTime (GVar_DesktopJumpSlideTime.value.f)
extern cGVar GVar_DesktopJumpSpeed;
#define g_DesktopJumpSpeed (GVar_DesktopJumpSpeed.value.f)
extern cGVar GVar_FXDesktopFallEndDist;
#define g_FXDesktopFallEndDist (GVar_FXDesktopFallEndDist.value.f)
extern cGVar GVar_FXDesktopFallEndFile;
#define g_FXDesktopFallEndFile (GVar_FXDesktopFallEndFile.value.s)
extern cGVar GVar_FallSoundVel;
#define g_FallSoundVel (GVar_FallSoundVel.value.f)		/* FallSoundVel */
extern cGVar GVar_FXBishopSellLifeFile;
#define g_FXBishopSellLifeFile (GVar_FXBishopSellLifeFile.value.s)
extern cGVar GVar_FXBishopAttackFile;
#define g_FXBishopAttackFile (GVar_FXBishopAttackFile.value.s)
extern cGVar GVar_BishopAttackPosX;
#define g_BishopAttackPosX (GVar_BishopAttackPosX.value.f)
extern cGVar GVar_BishopAttackPosY;
#define g_BishopAttackPosY (GVar_BishopAttackPosY.value.f)
extern cGVar GVar_BishopAmmoCost;
#define g_BishopAmmoCost (GVar_BishopAmmoCost.value.f)
extern cGVar GVar_BishopAttackRandOff;
#define g_BishopAttackRandOff (GVar_BishopAttackRandOff.value.f)
extern cGVar GVar_BishopShootSoundVol;
#define g_BishopShootSoundVol (GVar_BishopShootSoundVol.value.f)
extern cGVar GVar_BishopShootSound;
#define g_BishopShootSound (GVar_BishopShootSound.value.s)
extern cGVar GVar_DTritusHitTimeFader;
#define g_DTritusHitTimeFader (GVar_DTritusHitTimeFader.value.f)
extern cGVar GVar_FXDTritusAttackFile;
#define g_FXDTritusAttackFile (GVar_FXDTritusAttackFile.value.s)
extern cGVar GVar_DTritusHitTimeSpeed;
#define g_DTritusHitTimeSpeed (GVar_DTritusHitTimeSpeed.value.f)
extern cGVar GVar_FXDTritusKillFile;
#define g_FXDTritusKillFile (GVar_FXDTritusKillFile.value.s)
extern cGVar GVar_GearMissileSoundVol;
#define g_GearMissileSoundVol (GVar_GearMissileSoundVol.value.f)
extern cGVar GVar_GearLoadWeaponSound;
#define g_GearLoadWeaponSound (GVar_GearLoadWeaponSound.value.s)
extern cGVar GVar_GearShootDamage;
#define g_GearShootDamage (GVar_GearShootDamage.value.i)
extern cGVar GVar_GearMissileSound;
#define g_GearMissileSound (GVar_GearMissileSound.value.s)
extern cGVar GVar_GearShootPosX;
#define g_GearShootPosX (GVar_GearShootPosX.value.f)
extern cGVar GVar_GearShootDistance;
#define g_GearShootDistance (GVar_GearShootDistance.value.f)
extern cGVar GVar_GearLoadWeaponSoundVol;
#define g_GearLoadWeaponSoundVol (GVar_GearLoadWeaponSoundVol.value.f)
extern cGVar GVar_GearHitSound;
#define g_GearHitSound (GVar_GearHitSound.value.s)
extern cGVar GVar_GearHitSoundVol;
#define g_GearHitSoundVol (GVar_GearHitSoundVol.value.f)
extern cGVar GVar_GearShootPosZ;
#define g_GearShootPosZ (GVar_GearShootPosZ.value.f)
extern cGVar GVar_GearShootFailAlarm;
#define g_GearShootFailAlarm (GVar_GearShootFailAlarm.value.f)
extern cGVar GVar_GearShootHitAlarm;
#define g_GearShootHitAlarm (GVar_GearShootHitAlarm.value.f)
extern cGVar GVar_GearMissileSpeed;
#define g_GearMissileSpeed (GVar_GearMissileSpeed.value.f)
extern cGVar GVar_GearShootSound;
#define g_GearShootSound (GVar_GearShootSound.value.s)
extern cGVar GVar_GearShootSoundVol;
#define g_GearShootSoundVol (GVar_GearShootSoundVol.value.f)
extern cGVar GVar_GearShootPosY;
#define g_GearShootPosY (GVar_GearShootPosY.value.f)
extern cGVar GVar_GearXplodeRadius;
#define g_GearXplodeRadius (GVar_GearXplodeRadius.value.f)
extern cGVar GVar_GearLowWeaponEnergy;
#define g_GearLowWeaponEnergy (GVar_GearLowWeaponEnergy.value.f)
extern cGVar GVar_FXMaintenanceWorkFile;
#define g_FXMaintenanceWorkFile (GVar_FXMaintenanceWorkFile.value.s)
extern cGVar GVar_GearTargetName;
#define g_GearTargetName (GVar_GearTargetName.value.s)	/* GearTargetName: the target of the missile of the player */

/* the characters of the running actions (the "MeEnt" CHECKs) */
extern cEntity *g_BankMasterEnt;
extern cEntity *g_BertoEnt;
extern cEntity *g_BettyEnt;
extern cEntity *g_DtritusEnt;
/* the gear natives: the name of the shooting gear (also the one the ball does not hurt), the missile, its point */
extern char g_GearTarget[128];		/* [0x80] */
extern char g_GearSwName[128];		/* [0x80] */
extern cEntity *g_GearSw;
extern vec3_t g_GearXplodePos;
/* Logic_Flash: the point, the most characters to flash (-1 all), the count and the result */
extern vec3_t g_FlashPos;
extern int g_FlashResult;
extern int g_FlashMax;
extern int g_FlashCount;
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
extern cGVar GVar_MayorBlabliblaRadius;
#define g_MayorBlabliblaRadius (GVar_MayorBlabliblaRadius.value.f)
extern cGVar GVar_MayorListenPercent;
#define g_MayorListenPercent (GVar_MayorListenPercent.value.f)
extern cGVar GVar_MayorSleepPercent;
#define g_MayorSleepPercent (GVar_MayorSleepPercent.value.f)
extern cGVar GVar_FXMercenaryDrinkFile;
#define g_FXMercenaryDrinkFile (GVar_FXMercenaryDrinkFile.value.s)
extern cGVar GVar_MercenaryDrinkPosX;
#define g_MercenaryDrinkPosX (GVar_MercenaryDrinkPosX.value.f)
extern cGVar GVar_MercenaryDrinkPosY;
#define g_MercenaryDrinkPosY (GVar_MercenaryDrinkPosY.value.f)
extern cGVar GVar_MercenaryDrinkPosZ;
#define g_MercenaryDrinkPosZ (GVar_MercenaryDrinkPosZ.value.f)
extern cGVar GVar_FXMessengerAttackFile;
#define g_FXMessengerAttackFile (GVar_FXMessengerAttackFile.value.s)
extern cGVar GVar_FXMessengerTakeDataPackFile;
#define g_FXMessengerTakeDataPackFile (GVar_FXMessengerTakeDataPackFile.value.s)
extern cGVar GVar_MessengerAmmoCost;
#define g_MessengerAmmoCost (GVar_MessengerAmmoCost.value.f)
extern cGVar GVar_MessengerAttackPosX;
#define g_MessengerAttackPosX (GVar_MessengerAttackPosX.value.f)
extern cGVar GVar_MessengerAttackPosY;
#define g_MessengerAttackPosY (GVar_MessengerAttackPosY.value.f)
extern cGVar GVar_MessengerAttackPosZ;
#define g_MessengerAttackPosZ (GVar_MessengerAttackPosZ.value.f)
extern cGVar GVar_MessengerShootSound;
#define g_MessengerShootSound (GVar_MessengerShootSound.value.s)
extern cGVar GVar_MessengerShootSoundVol;
#define g_MessengerShootSoundVol (GVar_MessengerShootSoundVol.value.f)
extern cGVar GVar_MessengerTakeDataPackSound;
#define g_MessengerTakeDataPackSound (GVar_MessengerTakeDataPackSound.value.s)
extern cGVar GVar_MessengerTakeDataPackSoundVol;
#define g_MessengerTakeDataPackSoundVol (GVar_MessengerTakeDataPackSoundVol.value.f)
#define g_MessengerTakenCallback (GAME_ARRAY_IN(cPy_Function, GVar_MessengerTakenCallback, 16))	/* "MessengerTakenCallback" */
extern cGVar GVar_MessengerDataPack;
#define g_MessengerDataPack (GVar_MessengerDataPack.value.s)	/* "MessengerDataPack": the name of the data pack */
extern cGVar GVar_FXHammerHitFile;
#define g_FXHammerHitFile (GVar_FXHammerHitFile.value.s)
extern cGVar GVar_NurseAlarmHit;
#define g_NurseAlarmHit (GVar_NurseAlarmHit.value.f)
extern cGVar GVar_NurseDispDist;
#define g_NurseDispDist (GVar_NurseDispDist.value.f)
extern cGVar GVar_NurseDispRad;
#define g_NurseDispRad (GVar_NurseDispRad.value.f)
#define g_OnSleep (GAME_ARRAY_IN(cPy_Function, GVar_OnSleep, 16))		/* "OnSleep" */
extern cGVar GVar_FXPoliceGearConversionFile;
#define g_FXPoliceGearConversionFile (GVar_FXPoliceGearConversionFile.value.s)
extern cGVar GVar_FXPoliceStealFile;
#define g_FXPoliceStealFile (GVar_FXPoliceStealFile.value.s)
extern cGVar GVar_PoliceAlarmStepSTM;
#define g_PoliceAlarmStepSTM (GVar_PoliceAlarmStepSTM.value.f)
extern cGVar GVar_PoliceStealAmount;
#define g_PoliceStealAmount (GVar_PoliceStealAmount.value.i)
extern cGVar GVar_PoliceBossActionRadius;
#define g_PoliceBossActionRadius (GVar_PoliceBossActionRadius.value.f)
extern cGVar GVar_PoliceBossBrupBaseFile;
#define g_PoliceBossBrupBaseFile (GVar_PoliceBossBrupBaseFile.value.s)
extern cGVar GVar_PoliceBossBrupBounce1Sound;
#define g_PoliceBossBrupBounce1Sound (GVar_PoliceBossBrupBounce1Sound.value.s)
extern cGVar GVar_PoliceBossBrupBounce2Sound;
#define g_PoliceBossBrupBounce2Sound (GVar_PoliceBossBrupBounce2Sound.value.s)
extern cGVar GVar_PoliceBossBrupBounceSoundVol;
#define g_PoliceBossBrupBounceSoundVol (GVar_PoliceBossBrupBounceSoundVol.value.f)
extern cGVar GVar_PoliceBossBrupDebrisMax;
#define g_PoliceBossBrupDebrisMax (GVar_PoliceBossBrupDebrisMax.value.i)
extern cGVar GVar_PoliceBossBrupDebrisMin;
#define g_PoliceBossBrupDebrisMin (GVar_PoliceBossBrupDebrisMin.value.i)
extern cGVar GVar_PoliceBossBrupDirRand;
#define g_PoliceBossBrupDirRand (GVar_PoliceBossBrupDirRand.value.f)
extern cGVar GVar_PoliceBossBrupPosX;
#define g_PoliceBossBrupPosX (GVar_PoliceBossBrupPosX.value.f)
extern cGVar GVar_PoliceBossBrupPosY;
#define g_PoliceBossBrupPosY (GVar_PoliceBossBrupPosY.value.f)
extern cGVar GVar_PoliceBossBrupPosZ;
#define g_PoliceBossBrupPosZ (GVar_PoliceBossBrupPosZ.value.f)
extern cGVar GVar_FXRustyHitFile;
#define g_FXRustyHitFile (GVar_FXRustyHitFile.value.s)
extern cGVar GVar_RustyHitPosX;
#define g_RustyHitPosX (GVar_RustyHitPosX.value.f)
extern cGVar GVar_RustyHitPosY;
#define g_RustyHitPosY (GVar_RustyHitPosY.value.f)
extern cGVar GVar_RustyHitPosZ;
#define g_RustyHitPosZ (GVar_RustyHitPosZ.value.f)
extern cGVar GVar_FXSebastianFlashFile;
#define g_FXSebastianFlashFile (GVar_FXSebastianFlashFile.value.s)

/* the characters of the running actions (the "MeEnt" CHECKs), the nurse by name and the character it hits */
extern cEntity *g_MayorEnt;
extern cEntity *g_PoliceBossEnt;
extern char g_NurseName[128];		/* [0x80] */
extern cEntity *g_NurseTarget;

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
