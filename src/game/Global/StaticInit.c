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

/* the objects of the lines: SI_REF(name) its address,
 * through the assembler name, so a header macro of the same name does not get in the way */
/* an object by its name, without a declaration */
#define SI_REF(name) \
	({ \
		extern char sd_##name[] __asm__(SD_CPFX #name); \
		(void *)sd_##name; \
	})
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
		(void)(size), (void)(clear);	/* the 32 bit slot size and bytes of the original's call */ \
		t->slots = New_Array(count, sizeof(*t->slots)); \
		memset(t->slots, 0, (count) * sizeof(*t->slots)); \
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

/* a material the original constructs at the start and never uses */
CMaterial g_UnusedMaterial;

cGVar GVar_QuickMessageString4;

cGVar GVar_QuickMessageString2;

cGVar GVar_QuickMessageString6;

cGVar GVar_QuickMessageString3;

cGVar GVar_QuickMessageString5;

cGVar GVar_QuickMessageString1;

cGVar GVar_R_SkipModelFX;

cGVar GVar_NumRadarIcons;

cPy_NativeFunc NativeFunc_AnmSetAction;

cPy_NativeFunc NativeFunc_AnmCommand;

cPy_NativeFunc NativeFunc_AnmSetDefaultLoop;

cGVar GVar_MovieFramerate;

cGVar GVar_MovieJpegQuality;

cPy_NativeFunc NativeFunc_AnmChangeRandomAnm;

cGVar GVar_MovieUseMemory;

cPy_NativeFunc NativeFunc_AnmStopChan;

cGVar GVar_MovieCacheSize;

cPy_NativeFunc NativeFunc_AnmSetDisplAng;

cPy_NativeFunc NativeFunc_StreamingVoice;

cPy_NativeFunc NativeFunc_AnmSoundLoopEvent;

cGVar GVar_ScorerMarginV;

cPy_NativeFunc NativeFunc_WarningPolice;

cPy_NativeFunc NativeFunc_AnmSound2DEvent;

cPy_NativeFunc NativeFunc_AnmSoundEvent;

cPy_NativeFunc NativeFunc_StopStreamingVoice;

cGVar GVar_OptAnimPosMax;

cGVar GVar_MinTimeStoppedTurnPatrolCharAI;

cGVar GVar_MaxDistStopVisObjChaseWithOutActionAI;

cGVar GVar_MaxRotCharAI;

cGVar GVar_MaxDistStopVisObjChaseWithActionAI;

cGVar GVar_LimVelTurnCharAI;

cGVar GVar_MinDistStrafeCharAI;

cGVar GVar_FileArrowNextAlpha;

cPy_NativeFunc NativeFunc_EditText;

cGVar GVar_DimWindowReposYAI;

cGVar GVar_MaxMarginObjTrafficAI;

cPy_NativeFunc NativeFunc_ScorerSpriteGlow;

cPy_NativeFunc NativeFunc_ScorerSpriteFadeIn;

cGVar GVar_DistMinItemAI;

cPy_NativeFunc NativeFunc_ScorerMetroUsrHere;

cPy_NativeFunc NativeFunc_ScorerSpriteFadeOut;

cGVar GVar_MinDistBetweenTrafficAI;

cPy_NativeFunc NativeFunc_ScorerSpriteFadeOutFinal;

cPy_NativeFunc NativeFunc_ScorerSpriteFlash;

cPy_NativeFunc NativeFunc_BoostScorer;

cGVar GVar_DimWindowReposXAI;

cGVar GVar_ShootAI;

cGVar GVar_MaxAngleReposAI;

cGVar GVar_MaxRotAI;

cGVar GVar_MinRotAI;

cGVar GVar_ASEPublic;

/* two random generators the original seeds at the start and never uses */
cNetRandom g_UnusedNetRandom1;

cGVar GVar_ViewerModel;

cGVar GVar_ViewerModelOffsetY;

cGVar GVar_ViewerAnim;

cGVar GVar_BatchBuildGraph3DAI;

cGVar GVar_ViewerIniAI;

cGVar GVar_BatchBuildGraph2DAI;

cNetRandom g_UnusedNetRandom2;

cPy_NativeFunc NativeFunc_BankDirectorActionPress;

cPy_NativeFunc NativeFunc_BankDirectorActionRelease;

cPy_NativeFunc NativeFunc_BankerAttack;

cPy_NativeFunc NativeFunc_BankDirectorEndActionInit;

cPy_NativeFunc NativeFunc_BankMasterAttack;

cPy_NativeFunc NativeFunc_BankMasterActionInit;

cPy_NativeFunc NativeFunc_PrintMemBlockData;

cPy_NativeFunc NativeFunc_BankMasterActionEnd;

cPy_NativeFunc NativeFunc_BankMasterHitEvent;

cPy_NativeFunc NativeFunc_FXBankMasterAttack;

cPy_NativeFunc NativeFunc_BankMasterActionPress;

cPy_NativeFunc NativeFunc_BertoAttack;

cPy_NativeFunc NativeFunc_FXBertoAttack;

cPy_NativeFunc NativeFunc_BertoActionInit;

cPy_NativeFunc NativeFunc_BertoActionPress;

cPy_NativeFunc NativeFunc_BertoActionEnd;

cPy_NativeFunc NativeFunc_BertoHitEvent;

cPy_NativeFunc NativeFunc_BettyRestoreTime;

cPy_NativeFunc NativeFunc_BettyCameraSet;

cPy_NativeFunc NativeFunc_BettyActionPress;

cPy_NativeFunc NativeFunc_FXBettyAttack;

cPy_NativeFunc NativeFunc_BettyHitEvent;

cGVar GVar_SoundFrequency;

cGVar GVar_SoundChannels;

cGVar GVar_S_PriorityMode;

cPy_NativeFunc NativeFunc_BettyEndEvent;

cGVar GVar_SoundBits;

cGVar GVar_SoundStereo;

cGVar GVar_SoundUseHardware;

cGVar GVar_S_PlayMute;

cPy_NativeFunc NativeFunc_BishopEndActionEnd;

cPy_NativeFunc NativeFunc_FXCharacterTeleport;

cPy_NativeFunc NativeFunc_BishopActionPress;

cPy_NativeFunc NativeFunc_FXBishopSellLife;

cGVar GVar_MaxPlayerLives;

cGVar GVar_BishopLivePrice;

cPy_NativeFunc NativeFunc_FXCharacterDazed;

cPy_NativeFunc NativeFunc_FXCharacterBlaBliBla;

cPy_NativeFunc NativeFunc_BishopAttack;

cPy_NativeFunc NativeFunc_FXCrazyGamblerSetHeadModel;

cPy_NativeFunc NativeFunc_FXDesktopFallEnd;

cPy_NativeFunc NativeFunc_DesktopActionPress;

cGVar GVar_FXCharacterPoliceGearConversionFile;

cPy_NativeFunc NativeFunc_FXDTritusAttack;

cPy_NativeFunc NativeFunc_DtritusEndEvent;

cPy_NativeFunc NativeFunc_DtritusRestartEvent;

cGVar GVar_ShowTSCMode;

cPy_NativeFunc NativeFunc_DtritusActionPress;

cPy_NativeFunc NativeFunc_DtritusHitEvent;

cPy_NativeFunc NativeFunc_GearActionPress;

cPy_NativeFunc NativeFunc_GearShoot;

cPy_NativeFunc NativeFunc_GearHit;

cPy_NativeFunc NativeFunc_GearEndActionEnd;

cPy_NativeFunc NativeFunc_GearShootLoad;

cGVar GVar_FXVehicleAcceleratorFile;

cPy_NativeFunc NativeFunc_MapElementsActivate;

cPy_NativeFunc NativeFunc_GearInitAction;

cPy_NativeFunc NativeFunc_GearEndAction;

cGVar GVar_XboxSavePath;

cPy_NativeFunc NativeFunc_FXMaintenanceWork;

cGVar GVar_XboxSaveName6;

cGVar GVar_XboxSaveName3;

cPy_NativeFunc NativeFunc_ListeningMayor;

cPy_NativeFunc NativeFunc_MayorEndAction;

cGVar GVar_XboxSaveName0;

cGVar GVar_Fraglimit;

cPy_NativeFunc NativeFunc_FXMercenaryDrink;

cGVar GVar_XboxProfileName;

cGVar GVar_XboxSaveName1;

cPy_NativeFunc NativeFunc_MessengerActionPress;

cGVar GVar_XboxSaveName5;

cGVar GVar_XBRumblePadPad;

cPy_NativeFunc NativeFunc_MessengerAttack;

cGVar GVar_XboxCanSave;

cGVar GVar_ShowCredits;

cGVar GVar_XboxSaveName2;

cGVar GVar_NumProfiles;

cPy_NativeFunc NativeFunc_MessengerTakeDataPack;

cGVar GVar_XboxSaveName4;

cPy_NativeFunc NativeFunc_MessengerEndActionEnd;

cGVar GVar_XBNumControllers;

cPy_NativeFunc NativeFunc_NurseDoHammerHit;

cGVar GVar_isXboxDebug;

cGVar GVar_XboxFileCacheSize;

cPy_NativeFunc NativeFunc_NurseTryHammerAgain;

cPy_NativeFunc NativeFunc_NurseBackHitControl;

cPy_NativeFunc NativeFunc_NurseActionPress;

cGVar GVar_DefaultServerAddress;

cPy_NativeFunc NativeFunc_NurseInitHammerLoop;

cGVar GVar_MakeDedicated;

cPy_NativeFunc NativeFunc_NurseActionRelease;

cPy_NativeFunc NativeFunc_ListeningPolice;

cPy_NativeFunc NativeFunc_FXPoliceGearConversion;

cPy_NativeFunc NativeFunc_PoliceEndActionMoney;

cGVar GVar_JoinServerOnStartup;

cPy_NativeFunc NativeFunc_FXPoliceSteal;

cPy_NativeFunc NativeFunc_PoliceRestoreFreeTurn;

cPy_NativeFunc NativeFunc_PoliceActionPress;

cPy_NativeFunc NativeFunc_FXPoliceBossBrup;

cPy_NativeFunc NativeFunc_PoliceBossBrup;

cPy_NativeFunc NativeFunc_PoliceBossAction;

cPy_NativeFunc NativeFunc_FXRustyHit;

cPy_NativeFunc NativeFunc_FXSebastianFlash;

cPy_NativeFunc NativeFunc_SentinelActionRelease;

cPy_NativeFunc NativeFunc_SentinelActionPress;

cPy_NativeFunc NativeFunc_SentinelPoint;

cPy_NativeFunc NativeFunc_FXPoliceAlarmEnd;

cPy_NativeFunc NativeFunc_FXPoliceAlarmInit;

cPy_NativeFunc NativeFunc_FXMeetingPointCountDown;

cPy_NativeFunc NativeFunc_FXBossShieldSet;

cPy_NativeFunc NativeFunc_FXBossOnDamage;

cPy_NativeFunc NativeFunc_AcceleratorOnEnter;

cGVar GVar_MapTriCacheRecalc;

cPy_NativeFunc NativeFunc_ChangeDoorState;

cPy_NativeFunc NativeFunc_AutoDoorSlot;

cPy_NativeFunc NativeFunc_BackDoorSlot;

cPy_NativeFunc NativeFunc_AutoDoorUsrSlot;

cPy_NativeFunc NativeFunc_FrontDoorSlot;

cPy_NativeFunc NativeFunc_Usr0AutoDoorSlot;

cPy_NativeFunc NativeFunc_CloseDoorSlot;

cPy_NativeFunc NativeFunc_TakeMiss;

cPy_NativeFunc NativeFunc_TakeLife;

cPy_NativeFunc NativeFunc_TakeEner;

cPy_NativeFunc NativeFunc_TakePart;

cGVar GVar_FXVehicleSpeedTrailFollowFactor;

cGVar GVar_HangarShip5;

cGVar GVar_HangarShip3;

cGVar GVar_HangarShip9;

cGVar GVar_HangarShip8;

cGVar GVar_HangarShip2;

cGVar GVar_iHangarShip;

cGVar GVar_HangarShip4;

cGVar GVar_HangarShip7;

cGVar GVar_HangarShip1;

cGVar GVar_HangarShip6;

cGVar GVar_STrafficCapsule;

cPy_NativeFunc NativeFunc_ATPCCreate;

cPy_NativeFunc NativeFunc_ATPCOnStart;

cPy_NativeFunc NativeFunc_ATPCOnEnd;

cPy_NativeFunc NativeFunc_ATPCFire;

cPy_NativeFunc NativeFunc_ATPCFX;

cPy_NativeFunc NativeFunc_ATPCXplosionCall;

cPy_NativeFunc NativeFunc_CloudOnHit;

cPy_NativeFunc NativeFunc_CloudFire;

cPy_NativeFunc NativeFunc_DevastatorCreate;

cPy_NativeFunc NativeFunc_DevastatorRelease;

cPy_NativeFunc NativeFunc_DevastatorFire;

cPy_NativeFunc NativeFunc_DevastatorPress;

cPy_NativeFunc NativeFunc_DevastatorFX;

cPy_NativeFunc NativeFunc_EMIOnHit;

cGVar GVar_EMIRadius;

cPy_NativeFunc NativeFunc_EMIFire;

cPy_NativeFunc NativeFunc_InfernoFX;

cPy_NativeFunc NativeFunc_InfernoRelease;

cPy_NativeFunc NativeFunc_InfernoHit;

cPy_NativeFunc NativeFunc_InfernoPress;

cPy_NativeFunc NativeFunc_InfernoServerHit;

cPy_NativeFunc NativeFunc_InfernoOnStart;

cPy_NativeFunc NativeFunc_InfernoCreate;

cPy_NativeFunc NativeFunc_InfernoOnEnd;

cPy_NativeFunc NativeFunc_InfernoOnObjSlot;

cPy_NativeFunc NativeFunc_InfernoSetUp;

cPy_NativeFunc NativeFunc_InfernoClientFire;

cGVar GVar_InfernoFastSpeed;

cPy_NativeFunc NativeFunc_InfernoFire;

cPy_NativeFunc NativeFunc_HookPlugFX;

cPy_NativeFunc NativeFunc_HookDeActivate;

cPy_NativeFunc NativeFunc_LaserPress;

cPy_NativeFunc NativeFunc_LaserFX;

cPy_NativeFunc NativeFunc_HookActivate;

cPy_NativeFunc NativeFunc_LaserFire;

cPy_NativeFunc NativeFunc_HookFire;

cPy_NativeFunc NativeFunc_LaserRelease;

cPy_NativeFunc NativeFunc_LaserCreate;

cPy_NativeFunc NativeFunc_SonicXplde;

cPy_NativeFunc NativeFunc_SonicFire;

cPy_NativeFunc NativeFunc_SwarmHit;

cPy_NativeFunc NativeFunc_SwarmOnStart;

cPy_NativeFunc NativeFunc_SwarmPress;

cGVar GVar_SwarmDamage;

cGVar GVar_SwarmRangeUPG;

cPy_NativeFunc NativeFunc_SwarmCreate;

cGVar GVar_SwarmNumber3;

cPy_NativeFunc NativeFunc_SwarmSetUp;

cPy_NativeFunc NativeFunc_SwarmOnEnd;

cPy_NativeFunc NativeFunc_SwarmFire;

cGVar GVar_SwarmRange;

cGVar GVar_SwarmNumber4;

cPy_NativeFunc NativeFunc_SwarmRelease;

cPy_NativeFunc NativeFunc_SwarmServerHit;

cPy_NativeFunc NativeFunc_SwarmClientFire;

cPy_NativeFunc NativeFunc_SwarmFX;

cGVar GVar_SwarmNumber2;

cPy_NativeFunc NativeFunc_TeslaCreate;

cPy_NativeFunc NativeFunc_TeslaFire;

cPy_NativeFunc NativeFunc_TeslaPress;

cPy_NativeFunc NativeFunc_TeslaRelease;

cPy_NativeFunc NativeFunc_TeslaFX;

cPy_NativeFunc NativeFunc_VulcanFX;

cPy_NativeFunc NativeFunc_VulcanFire;

cPy_NativeFunc NativeFunc_VulcanRelease;

cPy_NativeFunc NativeFunc_VulcanCreate;

cPy_NativeFunc NativeFunc_VulcanPress;

cGVar GVar_BishopSpecialActionTime;

cGVar GVar_RelaxSeatsFunctionaryListName;

cGVar GVar_GenCharSpecialActionRadius;

cGVar GVar_GetAwayHumanDist;

cPy_NativeFunc NativeFunc_Police2GearEnd;

cGVar GVar_SecondaryMissionDesc;

cGVar GVar_PrimaryMissionDesc;

cPy_NativeFunc NativeFunc_IncrementHits;

cPy_NativeFunc NativeFunc_EndOfLaugh;

cGVar GVar_PlayerHelpedPoliceReward;

