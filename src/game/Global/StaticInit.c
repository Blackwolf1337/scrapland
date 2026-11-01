/*
 * The dynamic initializers of the game's globals: the functions the CRT of the original called from its __xc table
 * before WinMain, and the atexit destructors they register. Crt_Main walks the table of the original (crtxc.inc), so
 * they run in its order, and the destructors are registered with crt_atexit in the same order as the original did.
 *
 * Nearly all of them follow a few patterns: one macro per pattern below, one line per function in StaticInit.inc,
 * the destructor an initializer registers right before it. The few that fit no pattern are in the files of their
 * classes. Names and descriptions are the strings of the exe.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/archio.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/fonts.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/hashtable.h"
#include "scrap/input.h"
#include "scrap/material.h"
#include "scrap/musicfmod.h"
#include "scrap/net.h"
#include "scrap/pyfunction.h"
#include "scrap/scoreritems.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapparticles.h"
#include "scrap/groundportal.h"
#include "scrap/table.h"
#include "scrap/misc.h"
#include "scrap/chartype.h"
#include "scrap/controls.h"
#include "scrap/doors.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/embeddedfxmisc.h"
#include "scrap/fxchar.h"
#include "scrap/logic.h"
#include "scrap/scorerhud.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

/* the functions the lines name that no header declares */
void AICtrl_Police2GearEnd(cAbstractFile *params);
void ATPC_Create(cAbstractFile *params);
void ATPC_Fire(cAbstractFile *params);
void ATPC_FX(cAbstractFile *params);
void ATPC_OnEnd(cAbstractFile *params);
void ATPC_OnStart(cAbstractFile *params);
void ATPC_XplosionCall(cAbstractFile *params);
void ChType_ActionPress(cAbstractFile *params);
void ChType_BankDirectorActionPress(cAbstractFile *params);
void ChType_BankDirectorActionRelease(cAbstractFile *params);
void ChType_BankDirectorEndActionInit(cAbstractFile *params);
void ChType_BankerAttack(cAbstractFile *params);
void ChType_BankMasterActionEnd(cAbstractFile *params);
void ChType_BankMasterActionInit(cAbstractFile *params);
void ChType_BankMasterAttack(cAbstractFile *params);
void ChType_BankMasterHitEvent(cAbstractFile *params);
void ChType_BertoActionEnd(cAbstractFile *params);
void ChType_BertoActionInit(cAbstractFile *params);
void ChType_BertoAttack(cAbstractFile *params);
void ChType_BertoHitEvent(cAbstractFile *params);
void ChType_BettyActionPress(cAbstractFile *params);
void ChType_BettyCameraSet(cAbstractFile *params);
void ChType_BettyEndEvent(cAbstractFile *params);
void ChType_BettyHitEvent(cAbstractFile *params);
void ChType_BettyRestoreTime(cAbstractFile *params);
void ChType_BishopAttack(cAbstractFile *params);
void ChType_DesktopActionPress(cAbstractFile *params);
void ChType_DtritusActionPress(cAbstractFile *params);
void ChType_DtritusEndEvent(cAbstractFile *params);
void ChType_DtritusHitEvent(cAbstractFile *params);
void ChType_DtritusRestartEvent(cAbstractFile *params);
void ChType_EndActionEnd(cAbstractFile *params);
void ChType_FXBankMasterAttack(cAbstractFile *params);
void ChType_FXBertoAttack(cAbstractFile *params);
void ChType_FXBettyAttack(cAbstractFile *params);
void ChType_FXBishopSellLife(cAbstractFile *params);
void ChType_FXCrazyGamblerSetHeadModel(cAbstractFile *params);
void ChType_FXDesktopFallEnd(cAbstractFile *params);
void ChType_FXDTritusAttack(cAbstractFile *params);
void ChType_FXMaintenanceWork(cAbstractFile *params);
void ChType_FXMercenaryDrink(cAbstractFile *params);
void ChType_FXPoliceBossBrup(cAbstractFile *params);
void ChType_FXPoliceGearConversion(cAbstractFile *params);
void ChType_FXPoliceSteal(cAbstractFile *params);
void ChType_FXRustyHit(cAbstractFile *params);
void ChType_FXSebastianFlash(cAbstractFile *params);
void ChType_GearActionPress(cAbstractFile *params);
void ChType_GearEndAction(cAbstractFile *params);
void ChType_GearHit(cAbstractFile *params);
void ChType_GearInitAction(cAbstractFile *params);
void ChType_GearShoot(cAbstractFile *params);
void ChType_GearShootLoad(cAbstractFile *params);
void ChType_ListeningMayor(cAbstractFile *params);
void ChType_ListeningPolice(cAbstractFile *params);
void ChType_MayorEndAction(cAbstractFile *params);
void ChType_MessengerAttack(cAbstractFile *params);
void ChType_MessengerTakeDataPack(cAbstractFile *params);
void ChType_NurseActionPress(cAbstractFile *params);
void ChType_NurseActionRelease(cAbstractFile *params);
void ChType_NurseBackHitControl(cAbstractFile *params);
void ChType_NurseDoHammerHit(cAbstractFile *params);
void ChType_NurseInitHammerLoop(cAbstractFile *params);
void ChType_NurseTryHammerAgain(cAbstractFile *params);
void ChType_PoliceActionPress(cAbstractFile *params);
void ChType_PoliceBossAction(cAbstractFile *params);
void ChType_PoliceBossBrup(cAbstractFile *params);
void ChType_PoliceEndActionMoney(cAbstractFile *params);
void ChType_PoliceRestoreFreeTurn(cAbstractFile *params);
void ChType_SentinelActionPress(cAbstractFile *params);
void ChType_SentinelActionRelease(cAbstractFile *params);
void ChType_SentinelPoint(cAbstractFile *params);
void Cloud_Fire(cAbstractFile *params);
void Cloud_OnHit(cAbstractFile *params);
void Controls_TakeEnerNative(cAbstractFile *f);
void Controls_TakeLifeNative(cAbstractFile *f);
void Controls_TakeMissNative(cAbstractFile *f);
void Controls_TakePartNative(cAbstractFile *f);
void Devastator_Create(cAbstractFile *params);
void Devastator_Fire(cAbstractFile *params);
void Devastator_FX(cAbstractFile *params);
void Devastator_Press(cAbstractFile *params);
void Devastator_Release(cAbstractFile *params);
void Devastator_XplosionCall(cAbstractFile *params);
void Doors_AcceleratorOnEnter(cAbstractFile *params);
void Doors_AutoDoorSlot(cAbstractFile *params);
void Doors_AutoDoorUsrSlot(cAbstractFile *params);
void Doors_BackDoorSlot(cAbstractFile *params);
void Doors_ChangeDoorState(cAbstractFile *params);
void Doors_CloseDoorSlot(cAbstractFile *params);
void Doors_FrontDoorSlot(cAbstractFile *params);
void Doors_Usr0AutoDoorSlot(cAbstractFile *params);
void Elements_Load(cAbstractFile *f);
void EmbeddedFXChar_AlarmOff(cAbstractFile *f);
void EmbeddedFXChar_AlarmOn(cAbstractFile *f);
void EmbeddedFXMisc_BossShieldHit(void);
void EmbeddedFXMisc_BossShieldMsg(cAbstractFile *f);
void EmbeddedFXMisc_MeetingPointMsg(cAbstractFile *f);
void EMI_Fire(cAbstractFile *params);
void EMI_OnHit(cAbstractFile *params);
void FX_CharacterBlaBliBla(cAbstractFile *params);
void FX_CharacterDazed(cAbstractFile *params);
void FX_CharacterTeleport(cAbstractFile *params);
void Hook_AimNative(cAbstractFile *f);
void Hook_OffNative(cAbstractFile *f);
void Hook_StartNative(cAbstractFile *f);
void Hook_UpdateNative(cAbstractFile *f);
void Inferno_AimMode2Native(cAbstractFile *f);
void Inferno_AimMode3Native(cAbstractFile *f);
void Inferno_AimOffNative(cAbstractFile *f);
void Inferno_AimOnNative(cAbstractFile *f);
void Inferno_ClientFireNet(cAbstractFile *params);
void Inferno_FireNative(cAbstractFile *f);
void Inferno_HitNative(cAbstractFile *f);
void Inferno_HudNative(cAbstractFile *f);
void Inferno_NetCreateNative(cAbstractFile *f);
void Inferno_NetSetUpNative(cAbstractFile *f);
void Inferno_ServerHitNative(cAbstractFile *f);
void Inferno_SlowNative(cAbstractFile *f);
void Inferno_SphereHit(cAbstractFile *f);
void Laser_AimOffNative(cAbstractFile *f);
void Laser_AimOnNative(cAbstractFile *f);
void Laser_CreateNative(cAbstractFile *f);
void Laser_FireNative(cAbstractFile *f);
void Laser_HudNative(cAbstractFile *f);
void MemPools_PrintAll(void);
void Rusty_LoopDone(cAbstractFile *params);
void Scorer_BoostScorer(cAbstractFile *params);
void Scorer_MetroUsrHere(cAbstractFile *params);
void Scorer_SpriteFadeIn(cAbstractFile *params);
void Scorer_SpriteFadeOut(cAbstractFile *params);
void Scorer_SpriteFadeOutFinal(cAbstractFile *params);
void Scorer_SpriteFlash(cAbstractFile *params);
void Scorer_SpriteGlow(cAbstractFile *params);
void Sonic_ExplodeNative(cAbstractFile *f);
void Sonic_FireNative(cAbstractFile *f);
void Sputnik_LaughDone(cAbstractFile *params);
void Swarm_AimMode2Native(cAbstractFile *f);
void Swarm_AimMode3Native(cAbstractFile *f);
void Swarm_AimOffNative(cAbstractFile *f);
void Swarm_AimOnNative(cAbstractFile *f);
void Swarm_FireNative(cAbstractFile *f);
void Swarm_HitFXNative(cAbstractFile *f);
void Swarm_HudNative(cAbstractFile *f);
void Swarm_NetCreateNative(cAbstractFile *f);
void Swarm_NetSetUpNative(cAbstractFile *f);
void Swarm_ServerHitNative(cAbstractFile *f);
void Swarm_ShootNative(cAbstractFile *f);
void Tesla_AimOffNative(cAbstractFile *f);
void Tesla_AimOnNative(cAbstractFile *f);
void Tesla_CreateNative(cAbstractFile *f);
void Tesla_FireNative(cAbstractFile *f);
void Tesla_HudNative(cAbstractFile *f);
void TextEdit_Start(cAbstractFile *params);
void Vulcan_AimOffNative(cAbstractFile *f);
void Vulcan_AimOnNative(cAbstractFile *f);
void Vulcan_CreateNative(cAbstractFile *f);
void Vulcan_FireNative(cAbstractFile *f);
void Vulcan_HudNative(cAbstractFile *f);
void AICtrl_Police2GearEnd(cAbstractFile *params);
void MemPools_PrintAll(void);

/* the objects of the lines: SI_REF(name) its address, SI_AT(name, off) an address inside it (the value of a cGVar at
 * 16); through the assembler name, so a header macro of the same name does not get in the way */
#define SI_REF(name) SD_REF_(sd_##name, "_" #name)
#define SI_AT(name, off) ((void *)((char *)SI_REF(name) + (off)))
#define SI_OBJ(p) (p)
#define SI_GVAR(p) ((cGVar *)(p))
#define SI_STR(s) (s)
#define SI_FLOAT(p) (*(float *)(p))

/* ---- engine variables (cGVar): name, flags, description and the value ---- */

#define INIT_GVAR_INT(fn, obj, name, flags, desc, value) \
	void fn(void) { \
		GVar_InitInt(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), value); \
	}
#define INIT_GVAR_FLOAT(fn, obj, name, flags, desc, value) \
	void fn(void) { \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), value); \
	}
/* the value from a float of the data, read before the constructor */
#define INIT_GVAR_FLOAT_DATA(fn, obj, name, flags, desc, src) \
	void fn(void) { \
		float v = SI_FLOAT(src); \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), v); \
	}
#define INIT_GVAR_FLOAT_MULDIV(fn, obj, name, flags, desc, src, mul, div) \
	void fn(void) { \
		float v = SI_FLOAT(src) * SI_FLOAT(mul) / SI_FLOAT(div); \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), v); \
	}
#define INIT_GVAR_FLOAT_ADD(fn, obj, name, flags, desc, src, add) \
	void fn(void) { \
		float v = SI_FLOAT(src) + SI_FLOAT(add); \
		GVar_InitFloat(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), v); \
	}
#define INIT_GVAR_STRING(fn, fini, obj, name, flags, desc, value) \
	void fini(void); \
	void fn(void) { \
		GVar_InitString(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), SI_STR(value)); \
		crt_atexit(fini); \
	}
#define FINI_GVAR_STRING(fn, obj) \
	void fn(void) { \
		GVar_FiniString(SI_GVAR(obj)); \
	}
/* func NULL: no function */
#define INIT_GVAR_FUNCTION(fn, fini, obj, name, flags, desc, func) \
	void fini(void); \
	void fn(void) { \
		GVar_InitFunction(SI_GVAR(obj), SI_STR(name), flags, SI_STR(desc), SI_STR(func)); \
		crt_atexit(fini); \
	}

/* ---- the registry of C functions for the scripts ---- */

#define INIT_NATIVE_FUNC(fn, obj, name, cfunc) \
	void fn(void) { \
		cPy_NativeFunc_ctor(SI_OBJ(obj), SI_STR(name), (nativefunc_t)cfunc); \
	}

/* ---- objects with a constructor of the C ---- */

/* only the destructor: the object is all data */
#define INIT_ATEXIT(fn, fini) \
	void fini(void); \
	void fn(void) { \
		crt_atexit(fini); \
	}
#define INIT_OBJECT(fn, fini, obj, ctor) \
	void fini(void); \
	void fn(void) { \
		ctor(SI_OBJ(obj)); \
		crt_atexit(fini); \
	}
#define INIT_OBJECT_ARGS(fn, fini, obj, ctor, ...) \
	void fini(void); \
	void fn(void) { \
		ctor(SI_OBJ(obj), __VA_ARGS__); \
		crt_atexit(fini); \
	}
/* without destructor */
#define INIT_CALL(fn, obj, ctor) \
	void fn(void) { \
		ctor(SI_OBJ(obj)); \
	}
#define INIT_CALL_ARGS(fn, obj, ctor, ...) \
	void fn(void) { \
		ctor(SI_OBJ(obj), __VA_ARGS__); \
	}
#define INIT_JUMP(fn, func) \
	void fn(void) { \
		func(); \
	}
/* an array through the vector constructor iterator of the CRT */
#define INIT_VECTOR(fn, fini, obj, size, count, ctor, dtor) \
	void fini(void); \
	void fn(void) { \
		crt_eh_vector_ctor(SI_OBJ(obj), size, count, (void *)ctor, (void *)dtor); \
		crt_atexit(fini); \
	}
/* an array, the constructor in a loop of the initializer */
#define INIT_ARRAY(fn, obj, size, count, ctor) \
	void fn(void) { \
		for (int i = 0; i < (count); i++) { \
			ctor((void *)((byte *)SI_OBJ(obj) + i * (size))); \
		} \
	}
#define INIT_VECTOR3(fn, obj, vx, vy, vz) \
	void fn(void) { \
		vec3_t *v = SI_OBJ(obj); \
		v->x = vx; \
		v->y = vy; \
		v->z = vz; \
	}
/* the slots of a HashTable whose numSlots is data: new[], cleared, then locked and caseSensitive */
#define INIT_HASHTABLE(fn, fini, table, count, size, clear, lock, cs) \
	void fini(void); \
	void fn(void) { \
		HashTable *t = SI_OBJ(table); \
		t->slots = New_Array(count, size); \
		memset(t->slots, 0, clear); \
		t->locked = lock; \
		t->caseSensitive = cs; \
		crt_atexit(fini); \
	}

/* ---- destructors ---- */

#define FINI_CALL(fn, obj, dtor) \
	void fn(void) { \
		dtor(SI_OBJ(obj)); \
	}
#define FINI_CALL2(fn, obj1, dtor1, obj2, dtor2) \
	void fn(void) { \
		dtor1(SI_OBJ(obj1)); \
		dtor2(SI_OBJ(obj2)); \
	}
#define FINI_VECTOR(fn, obj, size, count, dtor) \
	void fn(void) { \
		crt_eh_vector_dtor(SI_OBJ(obj), size, count, (void *)dtor); \
	}
/* a DList: its nodes by its own function, then the free list */
#define FINI_DLIST(fn, obj, clear, size) \
	void fn(void) { \
		clear(SI_OBJ(obj)); \
		DList_FreePool(SI_OBJ(obj), size); \
	}
#define FINI_TABLE(fn, obj) \
	void fn(void) { \
		Table_Free(SI_OBJ(obj)); \
	}

#include "StaticInit.inc"
