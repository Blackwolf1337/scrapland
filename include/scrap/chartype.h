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

extern cGVarFunction GVar_OnFlash;
extern cGVarFunction GVar_OnSleep;

/* the fields of the characters (cWalkCharEntity) the types use */
#define WChar_FX(e) ((void *)WChar(e)->wl.obj.fx)		/* the embedded effect */
#define WChar_Listen(e) ((cEntity *)WChar(e)->listen.target)		/* its target: the mayor, the police */
#define WChar_Victim(e) ((cEntity *)WChar(e)->victim.target)		/* the character the banker robs */

static inline void *ChType_ActCtrl(cEntity *e) {
	return WithLife_GetActCtrl(e);
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
extern cPy_Function g_ChTypeSphereFunc;
#define g_OnFlash (((cPy_Function *)&GVar_OnFlash.func))		/* OnFlash, the C function of the flashes */

/* the natives one native calls through a sphere or flash test (rva, the same pointer as the original) */
#define CHTYPE_BANKMASTERSUCTION 0x0c1a70
#define CHTYPE_BERTOFLASHHIT 0x0c2480
#define CHTYPE_BETTYKILL 0x0c2c80
#define CHTYPE_DTRITUSKILL 0x0c5cd0
#define CHTYPE_GEARHIT 0x0c7020
#define CHTYPE_GEARSPHEREHIT 0x0c9130
#define CHTYPE_FLASHHIT 0x0c92f0

extern cGVar GVar_BankerStealAmount;
#define g_BankerStealAmount (GVar_BankerStealAmount.value.i)
extern cGVar GVar_BankMasterVacumDist;
#define g_BankMasterVacumDist (GVar_BankMasterVacumDist.value.f)
extern cGVar GVar_FallSoundVel;
#define g_FallSoundVel (GVar_FallSoundVel.value.f)		/* FallSoundVel */
extern cGVar GVar_GearXplodeRadius;
#define g_GearXplodeRadius (GVar_GearXplodeRadius.value.f)
extern cGVar GVar_GearLowWeaponEnergy;
#define g_GearLowWeaponEnergy (GVar_GearLowWeaponEnergy.value.f)
extern cGVar GVar_GearTargetName;
#define g_GearTargetName (GVar_GearTargetName.value.s)	/* GearTargetName: the target of the missile of the player */

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
/* money from *victim to *thief: the player takes a part (frac) of the MoneyReward of the victim (stat counts it);
 * a character takes amount from the player or the MoneyReward of another one (its victim from its logic) */
byte ChType_Steal(cEntity **thief, cEntity **victim, int amount, const char *stat, float frac);

void ChType_BankMasterActionPress(cAbstractFile *params);

void ChType_BertoActionPress(cAbstractFile *params);

int ChType_FlashChar(cEntity *e);

/* an inline of the local logic of a character: it is paused (+0x28) or +0x2a is set */
byte cAIChar_TestFlags(const void *self);

/* the shared steps of the natives (Banker.c) */
/* the parameters: the name, an event name and a float (unused) */
void ChType_ReadParams(cAbstractFile *f, char *name, char *event);
/* the character of that name with the CHECKs v and v->IsType(TYEN_WALKCHAR) */
cEntity *ChType_CheckChar(const char *name);
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
extern cGVar GVar_MessengerDataPack;
#define g_MessengerDataPack (GVar_MessengerDataPack.value.s)	/* "MessengerDataPack": the name of the data pack */
#define g_OnSleep (((cPy_Function *)&GVar_OnSleep.func))		/* "OnSleep" */
extern cGVar GVar_PoliceStealAmount;
#define g_PoliceStealAmount (GVar_PoliceStealAmount.value.i)

/* engine side */
/* the name of the listener into the mayor logic (NULL clears it, rva 0x153010) */
void cMayorLocalLogic_SetListener(cLocalLogic *self, cEntity *e);

void ChType_MessengerTakeDataPack(cAbstractFile *params);

byte ChType_PoliceFindListener(cEntity *v, cEntity **victim, byte *usr, void **action);

void ChType_PoliceBossTryAction(cEntity *v);

cEntity *ChType_SentinelTarget(cEntity *e);

#endif
