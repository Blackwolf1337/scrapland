/*
 * Game logic (original sources: Logic/): the alarm of the world (AlarmStatus.cpp), the local logics of the
 * characters (BaseLogic.cpp, the character types after it) and of the ships of the multiplayer modes (CTFEnemies,
 * CTFFriends, DeathMatch, Domination). An entity list of the world makes the local logic of an entity that joins
 * it (cEntityListData.func) and keeps it in cEntity.lLogic. The logic control of the world (g_pWorld->logic,
 * cLogicControl in include/scrap/worldlogic.h) holds the mode objects.
 */
#ifndef SCRAP_LOGIC_H
#define SCRAP_LOGIC_H

#include "scrap/gvar.h"
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aivehicle.h"
#include "scrap/alarm.h"
#include "scrap/collisiongrid.h"
#include "scrap/cstring.h"
#include "scrap/dlist.h"
#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/pyfunction.h"
#include "scrap/withlifeentity.h"

#define LOGIC_EMPTY GAME_STR(str_Empty)	/* the one "" the code compares by address */

/* ---- AlarmStatus.cpp: the cAlarm of include/scrap/alarm.h ---- */

/* engine variables (Gvars) */
#define g_AlarmCallback (((cPy_Function *)&GAME_GVARFUNC(GVar_AlarmCallback)->func))	/* AlarmCallback (on) */
extern cGVar GVar_AlarmFallDelta;
#define g_AlarmFallDelta (GVar_AlarmFallDelta.value.f)
extern cGVar GVar_AlarmFallMission;
#define g_AlarmFallMission (GVar_AlarmFallMission.value.f)
extern cGVar GVar_AlarmGrowDelta;
#define g_AlarmGrowDelta (GVar_AlarmGrowDelta.value.f)
extern cGVar GVar_EasyAlarmFallFactor;
#define g_EasyAlarmFallFactor (GVar_EasyAlarmFallFactor.value.f)
extern cGVar GVar_GuaranteedChars;
#define g_GuaranteedChars (GVar_GuaranteedChars.value.s)		/* ",type1,type2," */
#define g_PoliceTauntRace (((cString *)&GAME_GVAR(GVar_PoliceTauntRace)->value))	/* the default at +4 */
#define g_UsrDefChar (((cString *)&GAME_GVAR(GVar_UsrDefChar)->value))
extern cGVar GVar_GenCharAttackActive;
#define g_GenCharAttackActive (GVar_GenCharAttackActive.value.i)

/* a taunt of the ship of that name ("" none), kind 1 or 2 */
void Taunts_Play(const char *ship, const char *race, int kind);

/* ---- the local logic of an entity ---- */
typedef struct cLocalLogic cLocalLogic;

/* the slots of cLocalLogic (0x00 .. 0x18), cBaseLocalLogic (.. 0x60), cGenCharLocalLogic (.. 0x8c) and
 * cFunctionaryLocalLogic (.. 0xb0); the variable slots take the name of a script variable */
typedef struct cLocalLogicVtbl {
	void *(*Delete)(void *self, byte flags);					/* 0x00 */
	const char *(*GetName)(void *self);					/* 0x04 "BaseLogic" */
	byte(*Vf02)(void *self);							/* 0x08 */
	byte(*VarI)(void *self, const char *name, int *i, byte set);		/* 0x0c */
	byte(*VarF)(void *self, const char *name, double *d, byte set);		/* 0x10 */
	byte(*VarS)(void *self, const char *name, char **s, byte set);		/* 0x14 */
	byte(*VarV)(void *self, const char *name, double *x, double *y, double *z, byte set); /* 0x18 */
	byte(*UpdateBusy)(void *self);						/* 0x1c */
	void(*Refresh)(void *self);						/* 0x20 */
	byte(*Listens)(void *self, cEntity *talker);				/* 0x24 vf09 the mayor */
	void(*OnHit)(void *self, cEntity *attacker);				/* 0x28 */
	void(*OnSeen)(void *self, cEntity *e);					/* 0x2c vf11 */
	void(*OnAttack)(void *self, cEntity *attacker, float value);		/* 0x30 vf12 */
	void(*OnShot)(void *self, cEntity *attacker);				/* 0x34 vf13 */
	void(*OnHammer)(void *self, cEntity *nurse);				/* 0x38 vf14 */
	byte(*GivesMoney)(void *self, cEntity *police);				/* 0x3c vf15 */
	void(*OnAlarm)(void *self, cEntity *e);				/* 0x40 vf16 */
	void(*Update)(void *self, float dt);					/* 0x44 */
	void(*SetOriginFrom)(void *self, cEntity *e);				/* 0x48 */
	byte(*SetState)(void *self, int state, cEntity *target);			/* 0x4c vf19 */
	void(*SetIdle)(void *self, byte idle);					/* 0x50 */
	void(*SetActionRadius)(void *self, float radius);				/* 0x54 */
	void(*SetOrigin)(void *self, float x, float y, float z);			/* 0x58 */
	void(*SetOriginAng)(void *self, float x, float y, float z);		/* 0x5c */
	byte(*RestartState)(void *self);						/* 0x60 */
} cLocalLogicVtbl;

static inline const cLocalLogicVtbl *Logic_Vtbl(const void *l) {
	return *(const cLocalLogicVtbl *const *)l;
}

/* the slots of the logics of the generic characters (cGenCharLocalLogic and its children) */
typedef struct cGenCharLocalLogicVtbl {
	cLocalLogicVtbl base;
	byte(*CanAttack)(void *self, cEntity *e);		/* 0x64 vf25 the special attack against e */
	void(*UpdateView)(void *self);			/* 0x68 vf26 the view flag (+0x381) and the team */
	void(*ToRelax)(void *self, void *ai);			/* 0x6c vf27 the AI to its zone, relaxed */
	void(*ToAlert)(void *self, void *ai);			/* 0x70 vf28 the same in alert */
	void(*Accuse)(void *self, cEntity *e);			/* 0x74 vf29 state 2 against e */
	cEntity *(*FindTarget)(void *self);			/* 0x78 vf30 of the special attack */
	byte(*TargetDone)(void *self);				/* 0x7c vf31 the special attack ends */
	void(*Work)(void *self, float dt);				/* 0x80 vf32 functionaries */
	void(*Vf33)(void *self, byte restart);			/* 0x84 */
	void(*GoTo)(void *self, void *ai, cEntity *e);		/* 0x88 vf34 the AI walks to e */
	void(*UpdateGetIt)(void *self, void *ai, float dt);	/* 0x8c vf35 state 10 */
} cGenCharLocalLogicVtbl;
SD_OFFSET(cGenCharLocalLogicVtbl, CanAttack, 0x64);
SD_OFFSET(cGenCharLocalLogicVtbl, UpdateView, 0x68);
SD_OFFSET(cGenCharLocalLogicVtbl, Work, 0x80);

static inline const cGenCharLocalLogicVtbl *GenChar_Vtbl(const void *l) {
	return *(const cGenCharLocalLogicVtbl *const *)l;
}

/* the functionaries and the desk characters (cFunctionaryLocalLogic, Berto, Betty, Humphrey) */
typedef struct cFunctionaryLocalLogicVtbl {
	cGenCharLocalLogicVtbl base;
	byte(*CanTalk)(void *self);				/* 0x90 vf36 */
	void(*StopWork)(void *self, byte restart);			/* 0x94 vf37 */
	vec3_t *(*GetWorkAng)(void *self, vec3_t *out);		/* 0x98 vf38 */
	byte(*FastEscape)(void *self);				/* 0x9c vf39 */
	void(*AssignDesktop)(void *self, float maxDist);				/* 0xa0 vf40 */
	void(*ReleaseDesktop)(void *self);					/* 0xa4 vf41 */
} cFunctionaryLocalLogicVtbl;
SD_OFFSET(cFunctionaryLocalLogicVtbl, CanTalk, 0x90);
SD_OFFSET(cFunctionaryLocalLogicVtbl, FastEscape, 0x9c);
SD_OFFSET(cFunctionaryLocalLogicVtbl, AssignDesktop, 0xa0);

static inline const cFunctionaryLocalLogicVtbl *Functionary_Vtbl(const void *l) {
	return *(const cFunctionaryLocalLogicVtbl *const *)l;
}

/* the sentinels: the accuse when seen (no other sentinel accuses) and the think */
typedef struct cSentinelLocalLogicVtbl {
	cLocalLogicVtbl base;
	byte(*Seen)(void *self);					/* 0x64 */
	void(*Think)(void *self, float dt, byte other);		/* 0x68 */
} cSentinelLocalLogicVtbl;
SD_OFFSET(cSentinelLocalLogicVtbl, Seen, 0x64);

static inline const cSentinelLocalLogicVtbl *Sentinel_Vtbl(const void *l) {
	return *(const cSentinelLocalLogicVtbl *const *)l;
}
SD_OFFSET(cLocalLogicVtbl, Update, 0x44);
SD_OFFSET(cLocalLogicVtbl, SetState, 0x4c);
SD_OFFSET(cLocalLogicVtbl, RestartState, 0x60);

/* Logic/LocalLogic.h, 8 bytes: the logic of the CTFEnemies ships is this only */
struct cLocalLogic {
	const cLocalLogicVtbl *vftable;	/* 0x00 */
	cEntity *parent;		/* 0x04 */
};
SD_SIZE(cLocalLogic, 8);

/* BaseLogic.cpp, 0x48 bytes: the logic of a character ("WalkChar") */
typedef struct cBaseLocalLogic {
	const cLocalLogicVtbl *vftable;	/* 0x00 */
	cEntity *parent;		/* 0x04 the character */
	Nto1Node target;		/* 0x08 the target of the state, data the logic */
	int state;			/* 0x18 "State" */
	float rageTime;			/* 0x1c while the character has a link (Gear rage, GearRageMaxTime) */
	byte dazed;			/* 0x20 the action Dazed runs */
	byte pad_21[3];
	float dazedTime;		/* 0x24 LogicAwakeTime */
	byte idle;			/* 0x28 "Idle" */
	byte busy;			/* 0x29 "Lock": Crazy, Flashed or Dazed runs */
	byte alwaysLock;		/* 0x2a "AlwaysLock" */
	byte pad_2b;
	float actionRadius;		/* 0x2c "ActionRadius" */
	vec3_t origin;			/* 0x30 "Origin" */
	vec3_t originAng;		/* 0x3c "OriginAng" */
} cBaseLocalLogic;
SD_OFFSET(cBaseLocalLogic, target, 0x08);
SD_OFFSET(cBaseLocalLogic, actionRadius, 0x2c);
SD_SIZE(cBaseLocalLogic, 0x48);

/* GenCharLogic.cpp, 0x68 bytes */
typedef struct cGenCharLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float time;			/* 0x48 in the state: GenCharAcuseTime left of state 2, the time of state 3 */
	float accuseAgain;		/* 0x4c GenCharAcuseAgainTime after a hit, no new accuse before 0 */
	int defaultState;		/* 0x50 "DefaultState" (GENCHAR_STAY, RELAX, CHASE or GET_IT) */
	char *defaultTarget;		/* 0x54 cString "DefaultTarget" */
	byte canAttack;			/* 0x58 1: the special attack is allowed (CanAttack) */
	byte specialOn;			/* 0x59 the special action time runs */
	byte ignoreAlarm;		/* 0x5a "IgnoreAlarm", set by the bank logics */
	byte pad_5b;
	float specialTime;		/* 0x5c GenCharSpecialActionTime */
	float searchTime;		/* 0x60 the next target search when below 0 */
	int alarmState;			/* 0x64 Bishop: the state at an alarm (BishopAttackOnAlarm) */
} cGenCharLocalLogic;
SD_OFFSET(cGenCharLocalLogic, specialOn, 0x59);
SD_OFFSET(cGenCharLocalLogic, alarmState, 0x64);
SD_SIZE(cGenCharLocalLogic, 0x68);

/* FunctionaryLogic.cpp, 0xb8 bytes */
typedef struct cFunctionaryLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	Nto1Node desktop;		/* 0x68 the desktop of the type ("BertoDesktop"), data the logic */
	cEntity *forced;		/* 0x78 "ForcedDesktopAssigned", before the linked one */
	float sitTime;			/* 0x7c "TimeToSitDown" */
	float workTime;			/* 0x80 "TimeToStopWorking" */
	float relaxTime;		/* 0x84 half of the work time: the relax at the desktop */
	float playTime;			/* 0x88 three times the work time, the time when not working */
	float transTime;		/* 0x8c of the special action states */
	float specialTime;		/* 0x90 of the special action */
	float probWorking;		/* 0x94 "ProbTalkWhenWorking" */
	float probPlaying;		/* 0x98 "ProbTalkWhenPlaying" */
	float probWalking;		/* 0x9c "ProbTalkWhenWalking" */
	float escapeRadiusSqr;		/* 0xa0 "EscapeRadiusSqr" */
	float goToWorkSpeed;		/* 0xa4 "GoToWorkSpeed" */
	byte relaxed;			/* 0xa8 the relax at the desktop played */
	byte working;			/* 0xa9 always 1 */
	byte keepWorking;		/* 0xaa "KeepWorking" */
	byte special;			/* 0xab the special action runs */
	byte fast;			/* 0xac the fast part of the special action */
	byte fastEscape;		/* 0xad "FastEscape" */
	byte escapeInRadius;		/* 0xae "EscapeWithinRadius" */
	byte forceSpecial;		/* 0xaf "ForceSpecialBehaviour" */
	byte forceCancel;		/* 0xb0 "ForceCancelSpecialBehaviour" */
	byte ignoreBanker;		/* 0xb1 "IgnoreBankerAttack" */
	byte stopInRelax;		/* 0xb2 "StopInRelax" */
	byte pad_b3;
	int prevState;			/* 0xb4 the state before the special action */
} cFunctionaryLocalLogic;
SD_OFFSET(cFunctionaryLocalLogic, forced, 0x78);
SD_OFFSET(cFunctionaryLocalLogic, goToWorkSpeed, 0xa4);
SD_OFFSET(cFunctionaryLocalLogic, stopInRelax, 0xb2);
SD_OFFSET(cFunctionaryLocalLogic, prevState, 0xb4);
SD_OFFSET(cFunctionaryLocalLogic, desktop, 0x68);
SD_SIZE(cFunctionaryLocalLogic, 0xb8);

/* BertoLogic.cpp, BettyLogic.cpp and HumphreyLogic.cpp, 0xbc bytes each: a functionary that takes its desktop at the
 * first update when it is the default character ("Berto_Default", "Betty_Default", "Humphrey_Default") */
typedef struct cBertoLocalLogic {
	cFunctionaryLocalLogic base;	/* 0x00 */
	byte isDefault;			/* 0xb8 */
	byte pad_b9[3];
} cBertoLocalLogic;
SD_OFFSET(cBertoLocalLogic, isDefault, 0xb8);
SD_SIZE(cBertoLocalLogic, 0xbc);

typedef struct cBettyLocalLogic {
	cFunctionaryLocalLogic base;	/* 0x00 */
	byte isDefault;			/* 0xb8 */
	byte pad_b9[3];
} cBettyLocalLogic;
SD_OFFSET(cBettyLocalLogic, isDefault, 0xb8);
SD_SIZE(cBettyLocalLogic, 0xbc);

typedef struct cHumphreyLocalLogic {
	cFunctionaryLocalLogic base;	/* 0x00 */
	byte isDefault;			/* 0xb8 */
	byte pad_b9[3];
} cHumphreyLocalLogic;
SD_OFFSET(cHumphreyLocalLogic, isDefault, 0xb8);
SD_SIZE(cHumphreyLocalLogic, 0xbc);

/* BossLogic.cpp, 0x80 bytes */
typedef struct cBossLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	vec3_t workPos;			/* 0x68 the dummy BossWorkPlace */
	float workAng[2];		/* 0x74 */
	float time;			/* 0x7c in the walk or the work */
} cBossLocalLogic;
SD_OFFSET(cBossLocalLogic, time, 0x7c);
SD_SIZE(cBossLocalLogic, 0x80);

/* the GenChar logics that add no members (0x68 bytes in GenCharGroup_MakeLogic) */
typedef cGenCharLocalLogic cBankDirectorLocalLogic;
typedef cGenCharLocalLogic cBankMasterLocalLogic;
typedef cGenCharLocalLogic cBishopLocalLogic;
typedef cGenCharLocalLogic cCrazyDealerLocalLogic;
typedef cGenCharLocalLogic cDesktopLocalLogic;
typedef cGenCharLocalLogic cMercenaryLocalLogic;
typedef cGenCharLocalLogic cMessengerLocalLogic;
typedef cGenCharLocalLogic cPoliceBossLocalLogic;
typedef cGenCharLocalLogic cSputnikJunkyardLocalLogic;

/* the logics of the ships of CTFEnemies, Enemies and the scrapyard: a plain cLocalLogic (8 bytes) */
typedef cLocalLogic cCTFEnemiesLocalLogic;
typedef cLocalLogic cEnemiesLocalLogic;
typedef cLocalLogic cScrapyardLocalLogic;

#define CLOCALLOGIC_VFTABLE GAME_VFTABLE(cLocalLogic_vftable)
#define CBASELOCALLOGIC_VFTABLE GAME_VFTABLE(cBaseLocalLogic_vftable)
#define CBANKDIRECTORLOCALLOGIC_VFTABLE GAME_VFTABLE(cBankDirectorLocalLogic_vftable)
#define CBANKMASTERLOCALLOGIC_VFTABLE GAME_VFTABLE(cBankMasterLocalLogic_vftable)
#define CBERTOLOCALLOGIC_VFTABLE GAME_VFTABLE(cBertoLocalLogic_vftable)
#define CBETTYLOCALLOGIC_VFTABLE GAME_VFTABLE(cBettyLocalLogic_vftable)
#define CBISHOPLOCALLOGIC_VFTABLE GAME_VFTABLE(cBishopLocalLogic_vftable)
#define CBOSSLOCALLOGIC_VFTABLE GAME_VFTABLE(cBossLocalLogic_vftable)
#define CCRAZYDEALERLOCALLOGIC_VFTABLE GAME_VFTABLE(cCrazyDealerLocalLogic_vftable)
#define CCTFENEMIESLOCALLOGIC_VFTABLE GAME_VFTABLE(cCTFEnemiesLocalLogic_vftable)
#define CDESKTOPLOCALLOGIC_VFTABLE GAME_VFTABLE(cDesktopLocalLogic_vftable)
#define CFUNCTIONARYLOCALLOGIC_VFTABLE GAME_VFTABLE(cFunctionaryLocalLogic_vftable)
#define CGATEKEEPERLOCALLOGIC_VFTABLE GAME_VFTABLE(cGateKeeperLocalLogic_vftable)
#define CGEARLOCALLOGIC_VFTABLE GAME_VFTABLE(cGearLocalLogic_vftable)
#define CGENCHARLOCALLOGIC_VFTABLE GAME_VFTABLE(cGenCharLocalLogic_vftable)


/* the type name of the action class of a character */
static inline const char *Logic_TypeName(cEntity *e) {
	cActCtrl *ctrl = WithLife_Vtbl(e)->GetActCtrl(e);
	return ctrl->cls->name;
}

/* the radar marker of an object (cObjEntity.radarShow, "RadarShow") */

/* the first and the next entity node of a list ring, NULL at the end */
static inline Nto1Node *Logic_ListFirst(cEntityListData *l) {
	Nto1Node *n = l->ring.next;
	return n->data ? n : NULL;
}

static inline Nto1Node *Logic_ListNext(Nto1Node *n) {
	n = n->next;
	return n->data ? n : NULL;
}

/* the count of a list from the numElems of a node or of the list, -1 without one */
static inline int Logic_Count(const int *numElems) {
	return numElems ? *numElems : -1;
}

/* the fields of the characters the logics use (chartype.h has more) */

/* engine variables of the logics */
extern cGVar GVar_DefActionRadius;
#define g_DefActionRadius (GVar_DefActionRadius.value.f)
extern cGVar GVar_GearRageMaxTime;
#define g_GearRageMaxTime (GVar_GearRageMaxTime.value.f)
extern cGVar GVar_LogicAwakeTime;
#define g_LogicAwakeTime (GVar_LogicAwakeTime.value.f)
extern cGVar GVar_NotCleanableByGear;
#define g_NotCleanableByGear (GVar_NotCleanableByGear.value.s)
extern cGVar GVar_BankDirectorAgressive;
#define g_BankDirectorAgressive (GVar_BankDirectorAgressive.value.i)
extern cGVar GVar_BishopAttackOnAlarm;
#define g_BishopAttackOnAlarm (GVar_BishopAttackOnAlarm.value.i)
extern cGVar GVar_BossWalkingTime;
#define g_BossWalkingTime (GVar_BossWalkingTime.value.f)
extern cGVar GVar_BossWorkPlace;
#define g_BossWorkPlace (GVar_BossWorkPlace.value.s)
extern cGVar GVar_BossWorkingTime;
#define g_BossWorkingTime (GVar_BossWorkingTime.value.f)
extern byte g_BossWasIdle;		/* the boss was idle while at work */
extern cGVar GVar_EnergyBarActive;
#define g_EnergyBarActive (GVar_EnergyBarActive.value.i)
extern cGVar GVar_SpecialActionActive;
#define g_SpecialActionActive (GVar_SpecialActionActive.value.i)
extern cGVar GVar_GenCharRelaxSpeed;
#define g_GenCharRelaxSpeed (GVar_GenCharRelaxSpeed.value.f)
extern cGVar GVar_GenCharSpecialActionTime;
#define g_GenCharSpecialActionTime (GVar_GenCharSpecialActionTime.value.f)
extern cGVar GVar_GenCharMinSpecialActionTime;
#define g_GenCharMinSpecialActionTime (GVar_GenCharMinSpecialActionTime.value.f)
extern cGVar GVar_GenCharStdFar;
#define g_GenCharStdFar (GVar_GenCharStdFar.value.f)
extern cGVar GVar_GenCharStdFovX;
#define g_GenCharStdFovX (GVar_GenCharStdFovX.value.f)
extern cGVar GVar_GenCharStdFovY;
#define g_GenCharStdFovY (GVar_GenCharStdFovY.value.f)

/* the logic of a character or NULL */
cLocalLogic *Logic_GetLocalLogic(cEntity *e);
/* puts the entity on the floor; 0 if not possible */
byte Logic_SetOnFloor(cEntity *e);
/* a character on the floor below it (floorDist above the hit), 0 and an error without a floor */
byte Logic_CharOnFloor(cEntity *e);
/* flash at pos with radius; the last argument is -1 from the script module */
int Logic_Flash(const vec3_t *pos, float radius, int arg);

void Taunts_UpdateEndTime(void *taunts);
void ShipRegen_Set(void *shipRegen, cEntity *ship, float speed, float endSpeed);
void AICtrl_SendSentinelToWatch(void *aiCtrl, const vec3_t *pos);
void AICtrl_GearAttack(void *aiCtrl, cEntity *target, int arg);
/* a character that saw the player goes to accuse it ("Sending %s to acuse the main character"); 1 if one went */
byte AICtrl_SendAccuser(void *aiCtrl, byte check);

/* BaseLogic.cpp */
cBaseLocalLogic *cBaseLocalLogic_ctor(cBaseLocalLogic *self, cEntity *parent);
void cBaseLocalLogic_dtor(cBaseLocalLogic *self);
void cLocalLogic_dtor(cLocalLogic *self);
/* pos is in the view of viewer (dist, the field of view, a free line from pos to it) */
byte Logic_InView(cEntity *viewer, const vec3_t *pos, float dist, float fovY, float fovX, byte testLine);
/* the first character of the list (not viewer) in the view of viewer that filter takes (NULL: all) */
cEntity *Logic_FindInView(cEntity *viewer, cEntityListData *list, float dist, float fovY, float fovX, byte testLine,
	byte (*filter)(cEntity *));
byte cBaseLocalLogic_UpdateBusy(cBaseLocalLogic *self);
/* the VarV of cLocalLogic: no variable */
byte cLocalLogic_VarVNone(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set);
int cBaseLocalLogic_SetState(cBaseLocalLogic *self, int state, cEntity *target);
byte cBaseLocalLogic_VarF(cBaseLocalLogic *self, const char *name, double *d, byte set);
byte cBaseLocalLogic_VarI(cBaseLocalLogic *self, const char *name, int *i, byte set);
byte cBaseLocalLogic_VarS(cBaseLocalLogic *self, const char *name, char **s, byte set);
byte cBaseLocalLogic_VarV(cBaseLocalLogic *self, const char *name, double *x, double *y, double *z,
	byte set);
void cBaseLocalLogic_Update(cBaseLocalLogic *self, float dt);
void cBaseLocalLogic_SetIdle(cBaseLocalLogic *self, byte idle);
/* the debug texts of ShowAI (a character node of a list with the state names, the ships) */
void Logic_ShowChar(Nto1Node *node, const char **names);
void Logic_ShowChars(Nto1Node *node, const char **names, byte visible);
void Logic_ShowShip(cEntity *e);
void Logic_ShowShips(void);
void Logic_ShowShipList(const char *name, byte visible);
void Logic_ShowAI(byte visible);
/* the slots without code shared by the logic classes */
byte Logic_True(void *self);
byte Logic_False(void *self);
cEntity *Logic_NoTarget(void *self);
void Logic_Nop1(void *self, int arg);
byte Logic_False1(void *self, int arg);
/* the bank logics: the player in view with money in the world, the first character in view in the lists */
cEntity *Logic_RichUsrInView(cGenCharLocalLogic *self, cEntity *usr);
cEntity *Logic_FindInCharLists(cEntity *p);
/* Berto and Betty: the desktop of that name, when there is none */
void Logic_LinkDesktop(cFunctionaryLocalLogic *self, const char *name);
/* GenCharLogic.cpp: the random chance in percent of a reaction to attacker (not while idle unless the player) */
byte Logic_Chance(cBaseLocalLogic *l, cEntity *attacker, float chance);

/* GenCharLogic.cpp and FunctionaryLogic.cpp (imports) */
cGenCharLocalLogic *cGenCharLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent, int arg);
void cGenCharLocalLogic_dtor(cGenCharLocalLogic *self);
void cGenCharLocalLogic_Update(cGenCharLocalLogic *self, float dt);
byte cGenCharLocalLogic_SetState(cGenCharLocalLogic *self, int state, cEntity *target);
cFunctionaryLocalLogic *cFunctionaryLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent);
void cFunctionaryLocalLogic_dtor(cFunctionaryLocalLogic *self);
void cFunctionaryLocalLogic_Update(cFunctionaryLocalLogic *self, float dt);
/* the desktop of the functionary or NULL */
cEntity *cFunctionaryLocalLogic_GetDesktop(cFunctionaryLocalLogic *self);
/* the desktop leaves its work action */
void cFunctionaryLocalLogic_FreeDesktop(cFunctionaryLocalLogic *self);
void cFunctionaryLocalLogic_ReleaseDesktop(cFunctionaryLocalLogic *self);
void cFunctionaryLocalLogic_SetKeepWorking(cFunctionaryLocalLogic *self, byte on);
void cFunctionaryLocalLogic_NormalSpeed(cFunctionaryLocalLogic *self, cEntity *e);
void cFunctionaryLocalLogic_SlowEffect(cFunctionaryLocalLogic *self, cEntity *e);
void Functionary_FastEffect(cEntity *e);
void Functionary_WorkRelax(cEntity *e);
/* GenCharLogic.cpp (imports): the variables, vf12 (a robbery seen) */
byte cGenCharLocalLogic_VarI(cGenCharLocalLogic *self, const char *name, int *i, byte set);
byte cGenCharLocalLogic_VarS(cGenCharLocalLogic *self, const char *name, char **s, byte set);
void cGenCharLocalLogic_OnRobbery(cGenCharLocalLogic *self, cEntity *e, float amount);

/* ---- the multiplayer modes (of cLogicControl) ---- */
static inline cEntity *Logic_AttackTarget(cAIVehicle *ai) {
	return (cEntity *)ai->vftable->GetTargetNode(ai)->target;
}

/* the vehicle AI state constants the modes use */
#define AIV_STATE_ATTACK 2
#define AIV_STATE_FLAG 6
#define AIV_STATE_CHASE 7

/* CTFEnemies.cpp: g_pWorld->logic.ctfEnemies holds the pointer */
typedef struct cCTFEnemiesData {
	char *leader;			/* 0x00 cString, the name of the leader ship */
	cEntityListData *list;		/* 0x04 "CTFEnemies" */
	float timer;			/* 0x08 Think every 0.5 s */
	float bonus;			/* 0x0c the leader goes for the player while > 0 */
	int numAttack;			/* 0x10 the ships attacking the player */
} cCTFEnemiesData;
SD_SIZE(cCTFEnemiesData, 0x14);

/* CTFFriends.cpp: g_pWorld->logic.ctfFriends holds the pointer */
typedef struct cCTFFriendsData {
	cEntityListData *list;		/* 0x00 "CTFFriends" */
	float timer;			/* 0x04 */
	int numAttack;			/* 0x08 the ships attacking the leader */
} cCTFFriendsData;
SD_SIZE(cCTFFriendsData, 0xc);


extern cGVar GVar_LeaderFlag;
#define g_LeaderFlag (GVar_LeaderFlag.value.s)
extern cGVar GVar_PercEnemiesAttackPlayerWhenWinning;
#define g_PercEnemiesAttackPlayerWhenWinning (GVar_PercEnemiesAttackPlayerWhenWinning.value.f)
extern cGVar GVar_PlayerCTFTeamIsWinning;
#define g_PlayerCTFTeamIsWinning (GVar_PlayerCTFTeamIsWinning.value.i)
extern cGVar GVar_PercFriendsAttackLeaderWhenWinning;
#define g_PercFriendsAttackLeaderWhenWinning (GVar_PercFriendsAttackLeaderWhenWinning.value.f)
extern char g_CTFFlagName[128];		/* [0x80] the flag the leader goes for */

/* the ship e as a "Car" or NULL */
cEntity *Logic_AsCar(cEntity *e);
/* the "Car" of that name or NULL */
cEntity *Logic_FindCar(const char *name);

cCTFEnemies *cCTFEnemies_ctor(cCTFEnemies *self);
void cCTFEnemies_dtor(cCTFEnemies *self);
cEntityListData *cCTFEnemies_Init(cCTFEnemies *self);
cEntityListData *cCTFEnemies_GetList(cCTFEnemies *self);
char *cCTFEnemies_GetLeader(cCTFEnemies *self);
void cCTFEnemies_SetLeader(cCTFEnemies *self, const char *name);
void cCTFEnemies_SetBonus(cCTFEnemies *self, float t);
void cCTFEnemies_Update(cCTFEnemies *self, float dt);
void cCTFEnemiesData_Think(cCTFEnemiesData *self);
cCTFFriends *cCTFFriends_ctor(cCTFFriends *self);
void cCTFFriends_dtor(cCTFFriends *self);
void cCTFFriends_Init(cCTFFriends *self);
cEntityListData *cCTFFriends_GetList(cCTFFriends *self);
void cCTFFriends_Update(cCTFFriends *self, float dt);
void cCTFFriendsData_Think(cCTFFriendsData *self);
cDeathMatch *cDeathMatch_ctor(cDeathMatch *self);
void cDeathMatch_dtor(void);
void cDeathMatch_Init(cDeathMatch *self);
void cDeathMatch_Update(cDeathMatch *self, float dt);
void cDeathMatch_Think(cDeathMatch *self);

/* Domination.cpp: the zones of SLogic.SetDominationZones and the baton */
#define MAX_DOMIN_ZONES 4

typedef struct DominZone {
	cEntity *zone;			/* 0x0 */
	int state;			/* 0x4 0 free, 1 and 2 by a side, 3 and 4 a ship of a side on its way */
} DominZone;

extern int g_DominNumZones;
extern DominZone g_DominZones[4];		/* [MAX_DOMIN_ZONES] */
extern vec3_t g_DominBatonPos;
extern int g_DominBatonState;
extern cGVar GVar_DominFightingFreezeCadenceShoot;
#define g_DominFightingFreezeCadenceShoot (GVar_DominFightingFreezeCadenceShoot.value.i)
extern cGVar GVar_DominFightingFreezeControlAim;
#define g_DominFightingFreezeControlAim (GVar_DominFightingFreezeControlAim.value.i)
extern cGVar GVar_DominMaxRangeDist;
#define g_DominMaxRangeDist (GVar_DominMaxRangeDist.value.f)
extern cGVar GVar_DominMinRangeDist;
#define g_DominMinRangeDist (GVar_DominMinRangeDist.value.f)
extern cGVar GVar_DominWithinRangeDist;
#define g_DominWithinRangeDist (GVar_DominWithinRangeDist.value.f)
extern cGVar GVar_MaxDominDistChase;
#define g_MaxDominDistChase (GVar_MaxDominDistChase.value.f)
extern cGVar GVar_MinDominDistChase;
#define g_MinDominDistChase (GVar_MinDominDistChase.value.f)

typedef struct cDominationLogicVtbl {
	cDominationLogic *(*Delete)(cDominationLogic *self, byte flags);	/* 0x00 */
	void(*Init)(cDominationLogic *self);				/* 0x04 makes the list */
	void(*Update)(cDominationLogic *self, float dt);			/* 0x08 */
	cEntityListData *(*GetList)(cDominationLogic *self);		/* 0x0c the list of the side */
	int(*GetTeam)(cDominationLogic *self);				/* 0x10 0 enemies, 1 friends */
} cDominationLogicVtbl;

/* the data of a side: the enemies (0x10 bytes) keep the zone of their leader, the friends (0xc) only the timer */
typedef struct cDominationSide {
	cDominationLogic *owner;	/* 0x0 */
	cEntityListData *list;		/* 0x4 "DominationEnemies" */
	int zone;			/* 0x8 the zone the leader goes to, -1 none */
	float timer;			/* 0xc Think every 0.5 s */
} cDominationSide;
SD_SIZE(cDominationSide, 0x10);

typedef struct cDominationFriendsSide {
	cDominationLogic *owner;	/* 0x0 */
	cEntityListData *list;		/* 0x4 "DominationFriends" */
	float timer;			/* 0x8 */
} cDominationFriendsSide;
SD_SIZE(cDominationFriendsSide, 0xc);

#define CDOMINATIONLOGIC_VFTABLE GAME_VFTABLE(cDominationLogic_vftable)
#define CDOMINATIONENEMIESLOGIC_VFTABLE GAME_VFTABLE(cDominationEnemiesLogic_vftable)
#define CDOMINATIONFRIENDSLOGIC_VFTABLE GAME_VFTABLE(cDominationFriendsLogic_vftable)
/* the engine variable LeaderName (the leader ship of the enemies) and the death callbacks of the sides */
extern cGVar GVar_LeaderName;
#define g_DominLeaderName (GVar_LeaderName.value.s)
#define g_OnDominEnemyDeath (((cPy_Function *)&GAME_GVARFUNC(GVar_OnDominEnemyDeath)->func))
#define g_OnDominFriendDeath (((cPy_Function *)&GAME_GVARFUNC(GVar_OnDominFriendDeath)->func))

void Domination_AddZone(cEntity *zone);
void Domination_SetZoneState(int zone, int state);
void Domination_SetBatonState(const vec3_t *pos, int state);
cEntity *cDominationLogic_GetZone(cDominationLogic *self, cAIVehicle *ai);
cEntity *cDominationLogic_FindEnemy(cDominationLogic *self, const vec3_t *pos, cEntity *other,
	float maxDist);
void cDominationLogic_UpdateShip(cDominationLogic *self, cEntity *v, cEntity *target, cEntity *baton);
int cDominationLogic_NearestZone(cDominationLogic *self, const vec3_t *pos);
void cDominationLogic_ctor(cDominationLogic *self);
cDominationEnemiesLogic *cDominationEnemiesLogic_ctor(cDominationEnemiesLogic *self);
void cDominationEnemiesLogic_dtor(cDominationEnemiesLogic *self);
void cDominationLogic_dtor(cDominationLogic *self);
/* DominationEnemies.cpp and DominationFriends.cpp */
cEntity *cDominationEnemiesLogic_GetLeader(cDominationEnemiesLogic *self);
void cDominationSide_Think(cDominationSide *self);
void cDominationFriendsSide_Think(cDominationFriendsSide *self);
/* the zone of the ship of that name taken from the table (a zone on the way of the side is free again) */
cEntity *cDominationEnemiesLogic_ReleaseZone(cDominationLogic *self, const char *name);
cEntity *cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name);
cEntity *Domination_ReleaseZone(cDominationLogic *self, const char *name, int going, int held);
void Domination_OnDeath(cAbstractFile *params, cDominationLogic *side,
	cEntity *(*release)(cDominationLogic *, const char *), cPy_Function *cb);
/* the ship of the side that goes to zone, NULL for none */
cEntity *cDominationFriendsLogic_FindShipOfZone(cDominationFriendsLogic *self, cEntity *zone);

/* ---- the ships of the outdoor missions (of cLogicControl) ---- */
/* the empty destructors of the mode objects and the empty natives of their lists */
void Logic_NopDtor(void *self);
void Logic_NativeNop(cAbstractFile *params);
/* a random car of the list, NULL for none */
cEntity *Logic_RandomCar(cEntityListData *list);

#define CENEMIESLOCALLOGIC_VFTABLE GAME_VFTABLE(cEnemiesLocalLogic_vftable)

cEnemies *cEnemies_ctor(cEnemies *self);
void cEnemies_Attack(cEnemies *self, cEntity *usr);
void cEnemies_Think(cEnemies *self);
void cEnemies_Relax(cEnemies *self);

extern cGVar GVar_DoResetOutPolice;
#define g_ExtPoliceChasing (GVar_DoResetOutPolice.value.i)		/* the cars were sent after the player */
#define g_PoliceWaveEliminated (((cPy_Function *)&GAME_GVARFUNC(GVar_PoliceWaveEliminated)->func))	/* no police car left */
extern cGVar GVar_TrafficFightingMaxDist;
#define g_TrafficFightingMaxDist (GVar_TrafficFightingMaxDist.value.f)

/* the AI parameters of a police car back to the defaults or random */
void PoliceAI_RestoreParams(cAIVehicle *ai);
void PoliceAI_RandomParams(cAIVehicle *ai);

cExtPolice *cExtPolice_ctor(cExtPolice *self);
cEntity *ExtPolice_CallCop(cEntity *e);
void ExtPolice_CopDamage(const char *name, const char *attacker, byte died);
void cExtPolice_Think(cExtPolice *self);
byte ExtPolice_IsChased(cEntity *e);


cFriends *cFriends_ctor(cFriends *self);
void cFriends_Attack(cFriends *self, cEntity *usr);
void cFriends_Think(cFriends *self);

/* the logic of a gatekeeper (0x4c bytes): states 0 relaxed, 1 hidden in its shell */
typedef struct cGateKeeperLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float hiddenTime;		/* 0x48 GateKeeperHiddenTime when it hides */
} cGateKeeperLocalLogic;
SD_SIZE(cGateKeeperLocalLogic, 0x4c);

extern cGVar GVar_GateKeeperStdFar;
#define g_GateKeeperStdFar (GVar_GateKeeperStdFar.value.f)
extern cGVar GVar_GateKeeperStdFovY;
#define g_GateKeeperStdFovY (GVar_GateKeeperStdFovY.value.f)
extern cGVar GVar_GateKeeperStdFovX;
#define g_GateKeeperStdFovX (GVar_GateKeeperStdFovX.value.f)
extern cGVar GVar_GateKeeperHiddenTime;
#define g_GateKeeperHiddenTime (GVar_GateKeeperHiddenTime.value.f)

/* the slot of the logics with one argument that returns 1 */
byte Logic_True1(void *self, int arg);
cGateKeeperLocalLogic *cGateKeeperLocalLogic_ctor(cGateKeeperLocalLogic *self, cEntity *parent);
byte cGateKeeperLocalLogic_SetState(cGateKeeperLocalLogic *self, int state, cEntity *target);

/* GearLogic.cpp: the gears (0x5c bytes); states 0 relax (patrol), 1 chase (called), 2 attack, 3 stay in place,
 * 4 escort */
#define GEAR_RELAX 0
#define GEAR_STAY_IN_PLACE 3
#define GEAR_SCORT 4

typedef struct cGearLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float sirenTime;		/* 0x48 with the alarm: the next siren */
	float lostTime;			/* 0x4c since the target was lost */
	byte called;			/* 0x50 sent by the AI control */
	byte pad_51[3];
	int defaultState;		/* 0x54 "DefaultState" */
	char *defaultTarget;		/* 0x58 cString "DefaultTarget" */
} cGearLocalLogic;
SD_SIZE(cGearLocalLogic, 0x5c);

/* the view cone of the arrow of a character (its AI sees in it) */

/* the AI control: level to at least level, the gear becomes a police */
void AICtrl_RaiseLevel(void *aiCtrl, int level);
byte AICtrl_GearToPolice(void *aiCtrl, cEntity *gear);

byte cGearLocalLogic_SetState(cGearLocalLogic *self, int state, cEntity *target);


cGenCharGroup *cGenCharGroup_ctor(cGenCharGroup *self);
void cGenCharGroup_dtor(cGenCharGroup *self);
void cGenCharGroup_Init(cGenCharGroup *self);
void cGenCharGroup_Update(cGenCharGroup *self, float dt);
int cGenCharGroup_GetLevel(cGenCharGroup *self);
void cGenCharGroup_RaiseLevel(cGenCharGroup *self, int level);
byte cGenCharGroup_TakeAttack(cGenCharGroup *self);
/* the maker of the logics of the GenChar lists (cEntityListData.func, rva 0x14b860) */
#define GENCHARGROUP_MAKELOGIC 0x14b860

/* the Init and Update of the mode objects of part 2 */
void cExtPolice_Init(cExtPolice *self);
void cExtPolice_Update(cExtPolice *self, float dt);
void cEnemies_Init(cEnemies *self);
void cEnemies_Update(cEnemies *self, float dt);
void cFriends_Init(cFriends *self);
void cFriends_Update(cFriends *self, float dt);
cGateKeepers *cGateKeepers_ctor(cGateKeepers *self);
void cGateKeepers_Init(cGateKeepers *self);
void cGateKeepers_Update(cGateKeepers *self, float dt);
cDominationFriendsLogic *cDominationFriendsLogic_ctor(cDominationFriendsLogic *self);
void cDominationFriendsLogic_dtor(cDominationFriendsLogic *self);

/* MessengerLogic.cpp */
void *cMessengers_ctor(void *self);
void cMessengers_dtor(void *self);
void cMessengers_Init(void *self);
void cMessengers_Think(void *self);
/* the messenger m with the pack at the desk: 1 while the drop goes on */
byte Messenger_Deliver(cEntity *m, cEntity *desk, cEntity *pack);

/* the constructors of the logics of the other types (later sources of Logic/, imports) */
void *cLittlePoliceLocalLogic_ctor(void *self, cEntity *parent);
void *cMayorLocalLogic_ctor(void *self, cEntity *parent);
void *cRustyLocalLogic_ctor(void *self, cEntity *parent);
cSputnikJunkyardLocalLogic *cSputnikJunkyardLocalLogic_ctor(cSputnikJunkyardLocalLogic *self,
	cEntity *parent);
void *cMaintenanceLocalLogic_ctor(void *self, cEntity *parent);
cPoliceBossLocalLogic *cPoliceBossLocalLogic_ctor(cPoliceBossLocalLogic *self, cEntity *parent);
cMessengerLocalLogic *cMessengerLocalLogic_ctor(cMessengerLocalLogic *self, cEntity *parent);
cMercenaryLocalLogic *cMercenaryLocalLogic_ctor(cMercenaryLocalLogic *self, cEntity *parent);
cHumphreyLocalLogic *cHumphreyLocalLogic_ctor(cHumphreyLocalLogic *self, cEntity *parent);
void *cHumanLocalLogic_ctor(void *self, cEntity *parent);
/* the logics of part 1 */
cBankDirectorLocalLogic *cBankDirectorLocalLogic_ctor(cBankDirectorLocalLogic *self, cEntity *parent);
cBankMasterLocalLogic *cBankMasterLocalLogic_ctor(cBankMasterLocalLogic *self, cEntity *parent);
cDesktopLocalLogic *cDesktopLocalLogic_ctor(cDesktopLocalLogic *self, cEntity *parent);
cBertoLocalLogic *cBertoLocalLogic_ctor(cBertoLocalLogic *self, cEntity *parent);
cBettyLocalLogic *cBettyLocalLogic_ctor(cBettyLocalLogic *self, cEntity *parent);
cBishopLocalLogic *cBishopLocalLogic_ctor(cBishopLocalLogic *self, cEntity *parent);
cCrazyDealerLocalLogic *cCrazyDealerLocalLogic_ctor(cCrazyDealerLocalLogic *self, cEntity *parent);
cBossLocalLogic *cBossLocalLogic_ctor(cBossLocalLogic *self, cEntity *parent);

/* GenCharLogic.cpp: the states (GENCHAR_ of the CHECK of DefaultState); 7 and 8 are of the police */
#define GENCHAR_RELAX 0
#define GENCHAR_ALERT 1
#define GENCHAR_ACCUSE 2
#define GENCHAR_GETAWAY 3
#define GENCHAR_ATTACK 4		/* the special attack against the target */
#define GENCHAR_CHASE 5
#define GENCHAR_STAY 6
#define GENCHAR_GET_IT 10		/* walk to the target */

extern cGVar GVar_GenCharMaxSpecialActionTime;
#define g_GenCharMaxSpecialActionTime (GVar_GenCharMaxSpecialActionTime.value.f)
extern cGVar GVar_GenCharAlertSpeed;
#define g_GenCharAlertSpeed (GVar_GenCharAlertSpeed.value.f)
#define g_OnDefStateCallback (((cPy_Function *)&GAME_GVARFUNC(GVar_OnDefStateCallback)->func))	/* (name) after RestartState */
extern cGVar GVar_GenCharAcuseAgainTime;
#define g_GenCharAcuseAgainTime (GVar_GenCharAcuseAgainTime.value.f)
extern cGVar GVar_GenCharRobberyDetection;
#define g_GenCharRobberyDetection (GVar_GenCharRobberyDetection.value.f)
extern cGVar GVar_GenCharAcuseTime;
#define g_GenCharAcuseTime (GVar_GenCharAcuseTime.value.f)
extern cGVar GVar_GenCharSpecialChaseSpeed;
#define g_GenCharSpecialChaseSpeed (GVar_GenCharSpecialChaseSpeed.value.f)
extern cGVar GVar_GenCharAttackSpeed;
#define g_GenCharAttackSpeed (GVar_GenCharAttackSpeed.value.f)
extern cGVar GVar_GenCharGetAwayTime;
#define g_GenCharGetAwayTime (GVar_GenCharGetAwayTime.value.f)
extern cGVar GVar_GenCharAttackLimit;
#define g_GenCharAttackLimit (GVar_GenCharAttackLimit.value.f)
extern cGVar GVar_GenCharPoliceRefuse;
#define g_GenCharPoliceRefuse (GVar_GenCharPoliceRefuse.value.f)
extern cGVar GVar_GenCharEscapeSpeed;
#define g_GenCharEscapeSpeed (GVar_GenCharEscapeSpeed.value.f)
/* the type of the player draws the attention of a generic character */
byte Logic_IsSuspectType(const char *type);
byte cGenCharLocalLogic_RestartState(cGenCharLocalLogic *self);
void cGenCharLocalLogic_OnHit(cGenCharLocalLogic *self, cEntity *attacker);
void cGenCharLocalLogic_GoTo(cGenCharLocalLogic *self, cAIChar *ai, cEntity *e);
byte cGenCharLocalLogic_CanAttack(cGenCharLocalLogic *self, cEntity *e);
/* the special action over, the next target search in a random time */
void cGenCharLocalLogic_ResetSpecial(cGenCharLocalLogic *self);
/* the action Discover of a character that stands */
void cGenCharLocalLogic_Discover(cGenCharLocalLogic *self);
/* SetState(2, e): the vf29 of the GenChars and the vf16 of the nurses */
void Logic_SetState2(cBaseLocalLogic *self, cEntity *e);
/* the empty slots with two arguments */
void Logic_Nop2(void *self, int a, int b);
byte Logic_True2(void *self, int a, int b);

/* HumanLogic.cpp, 0x5c bytes: the characters near the human flee from it */
typedef struct cHumanLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	cPy_Function func;		/* 0x48 HumanLogic_Escape */
} cHumanLocalLogic;
SD_SIZE(cHumanLocalLogic, 0x5c);

#define CHUMANLOCALLOGIC_VFTABLE GAME_VFTABLE(cHumanLocalLogic_vftable)
extern cEntity *g_HumanEnt;	/* the parent of the last human logic made */

/* HumphreyLogic.cpp: a cFunctionaryLocalLogic (isDefault: "Humphrey_Default") at the desktop "HumphreyDesktop" */
#define CHUMPHREYLOCALLOGIC_VFTABLE GAME_VFTABLE(cHumphreyLocalLogic_vftable)

/* InfItem.cpp: the item list of the world (cInfItem of worldlogic.h, g_pWorld->logic.items, 100 bytes), DLists of
 * cItemDesc nodes: the life items and the ammo items of three kinds */
typedef struct cInfItemNode {
	struct cInfItemNode *next;	/* 0x0 */
	struct cInfItemNode *prev;	/* 0x4 */
	cItemDesc item;			/* 0x8 */
} cInfItemNode;
SD_SIZE(cInfItemNode, 0x10);

#define CINFITEM_VFTABLE GAME_VFTABLE(cInfItem_vftable)

cInfItem *cInfItem_ctor(cInfItem *self);
void cInfItem_dtor(cInfItem *self);

#define g_CreatePolice (((cPy_Function *)&GAME_GVARFUNC(GVar_CreatePolice)->func))	/* (name, x, y, z) */
#define g_CreateGear (((cPy_Function *)&GAME_GVARFUNC(GVar_CreateGear)->func))	/* (name, x, y, z) */
extern cGVar GVar_PoliceLogicBelligerant;
#define g_PoliceLogicBelligerant (GVar_PoliceLogicBelligerant.value.i)
extern cGVar GVar_SentinelNextGotoTime;
#define g_SentinelNextGotoTime (GVar_SentinelNextGotoTime.value.f)
extern cGVar GVar_SentinelAbortyNPCAttDist;
#define g_SentinelAbortyNPCAttDist (GVar_SentinelAbortyNPCAttDist.value.f)

void *AICtrl_ctor(void *aiCtrl);
void AICtrl_dtor(void *aiCtrl);
void AICtrl_Init(void *aiCtrl);
void AICtrl_Update(void *aiCtrl, float dt);
/* the police e may become a gear (the gear "Gear" + the name after "Police" does not exist) */
byte AICtrl_CanFuse(void *aiCtrl, cEntity *e);
/* the police becomes a gear (into *gear, may be NULL); 0 if it may not */
byte AICtrl_Fuse(void *aiCtrl, cEntity *police, cEntity **gear);
/* the sentinel nearest to e goes for it (state 6), only before the alarm */
void AICtrl_SendSentinelToNPC(void *aiCtrl, cEntity *e);
/* the gears that chase the player go for e, without one the gear attack on e */
void AICtrl_GearsChase(void *aiCtrl, cEntity *e);

/* the sentinels (SentinelLogic.cpp, the source after this part, imports) */
/* the logic of a sentinel, 0x50 bytes */
typedef struct cSentinelLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float time;			/* 0x48 of the state (the follow, the watch) */
	byte seen;			/* 0x4c the player was in view at the last think */
	byte pad_4d[3];
} cSentinelLocalLogic;
SD_SIZE(cSentinelLocalLogic, 0x50);

void *cSentinelLocalLogic_ctor(void *self, cEntity *parent);
/* to the place pos to watch it */
void cSentinelLocalLogic_Watch(void *self, const vec3_t *pos);
/* back to state 0 (1 with the alarm), unless idle */
void cSentinelLocalLogic_Return(void *self);
cSentinelLocalLogic *cSentinelLocalLogic_Delete(cSentinelLocalLogic *self, byte flags);
void cSentinelLocalLogic_OnAlarm(cSentinelLocalLogic *self, cEntity *e);
byte cSentinelLocalLogic_Seen(cSentinelLocalLogic *self);
void cSentinelLocalLogic_Think(cSentinelLocalLogic *self, float dt, byte quiet);
void cSentinelLocalLogic_OnRobbery(cSentinelLocalLogic *self, cEntity *e, float amount);
void cSentinelLocalLogic_OnAttack(cSentinelLocalLogic *self, cEntity *e);
void cSentinelLocalLogic_OnUsr(cSentinelLocalLogic *self, cEntity *e);
byte cSentinelLocalLogic_GivesMoney(cSentinelLocalLogic *self, cEntity *e);
void cSentinelLocalLogic_UpdateState(cSentinelLocalLogic *self, cEntity *p, float dt);
byte cSentinelLocalLogic_RestartState(cSentinelLocalLogic *self);
byte cSentinelLocalLogic_SetState(cSentinelLocalLogic *self, int state, cEntity *target);
/* the voice event of the character of that name (Sound/, rva 0x1a1090) */
byte Sound_PlayVoice(const char *name, const char *event, int arg, int force);
/* GearLogic.cpp */
cGearLocalLogic *cGearLocalLogic_ctor(cGearLocalLogic *self, cEntity *parent);
byte cGearLocalLogic_Call(cGearLocalLogic *self, cEntity *e);

/* LittlePoliceLogic.cpp, 0x78 bytes: the police of the indoor maps (the type "Police") */
typedef struct cLittlePoliceLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 specialTime: the time of the chase in state 4 */
	float sirenTime;		/* 0x68 with the alarm: the next siren */
	float lostTime;			/* 0x6c of the player, 5 when the AI walks to it */
	byte bribe;			/* 0x70 the arrow shows the options of the bribe */
	byte pad_71[3];
	int policeResult;		/* 0x74 "PoliceResult": 1 when it goes to the main mission */
} cLittlePoliceLocalLogic;
SD_OFFSET(cLittlePoliceLocalLogic, policeResult, 0x74);
SD_SIZE(cLittlePoliceLocalLogic, 0x78);

#define CLITTLEPOLICELOCALLOGIC_VFTABLE GAME_VFTABLE(cLittlePoliceLocalLogic_vftable)
extern cGVar GVar_LittlePoliceStopAcusingMaxDistFromUser;
#define g_LittlePoliceStopAcusingMaxDistFromUser (GVar_LittlePoliceStopAcusingMaxDistFromUser.value.f)
extern cGVar GVar_LittlePoliceMaxDistFar;
#define g_LittlePoliceMaxDistFar (GVar_LittlePoliceMaxDistFar.value.f)
extern cGVar GVar_LittlePoliceAsksForMoney;
#define g_LittlePoliceAsksForMoney (GVar_LittlePoliceAsksForMoney.value.i)
extern cGVar GVar_LittlePoliceChasingTime;
#define g_LittlePoliceChasingTime (GVar_LittlePoliceChasingTime.value.f)
extern cGVar GVar_MainMissionStopDist;
#define g_MainMissionStopDist (GVar_MainMissionStopDist.value.f)
extern byte g_LittlePoliceArrived;	/* GoTo left the AI in state 4 */
extern cGVar GVar_PoliceStealAmount;
#define g_PoliceStealAmount (GVar_PoliceStealAmount.value.i)

/* Parking.cpp: the parking zones of Scrap.AddParkingZone (flag 0: of the ships, 1: of the items), a ring each */
typedef struct cParkingZone {
	vec3_t pos;			/* 0x0 */
	cString name;			/* 0xc "_Junkyard" is not found in ring 0 */
} cParkingZone;
SD_SIZE(cParkingZone, 0x10);

typedef struct cParkingNode {
	struct cParkingNode *next;	/* 0x00 */
	struct cParkingNode *prev;	/* 0x04 */
	cParkingZone zone;		/* 0x08 */
} cParkingNode;
SD_SIZE(cParkingNode, 0x18);

/* ShipRegen.cpp: the life regeneration of the player, and the ship of SLogic.SetShipToRegenerate */
typedef struct cShipRegenShip {
	char name[0x80];		/* 0x00 */
	float time;			/* 0x80 to the next point */
	float rate;			/* 0x84 1 / speed: the time per point at no life */
	float endRate;			/* 0x88 1 / endSpeed: at full life */
	int life;			/* 0x8c at the last update */
} cShipRegenShip;
SD_SIZE(cShipRegenShip, 0x90);

/* Scrapyard.cpp: a group of ships that guard their lairs ("Mosquitos", "MadHunters"), behind a pointer */
typedef struct cShipGroupData {
	cPy_Function onSphere;		/* 0x00 native: the ships in the sphere of the intruder */
	HashTable lairs;		/* 0x14 of cWorldStrItem: the lair (vec3) of a ship (name) */
	const char *listName;		/* 0x20 */
	cEntityListData *list;		/* 0x24 */
	float timer;			/* 0x28 think every 0.5 s */
} cShipGroupData;
SD_SIZE(cShipGroupData, 0x2c);

cShipGroupData *cShipGroupData_ctor(cShipGroupData *self, const char *list);
void cShipGroupData_SetLair(cShipGroupData *self, const char *name, const vec3_t *pos);
void cShipGroupData_Think(cShipGroupData *self);

#define CLOGICCONTROL_VFTABLE GAME_VFTABLE(cLogicControl_vftable)
extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)
extern cGVar GVar_TimeBetweeenTaunts;
#define g_TimeBetweeenTaunts (GVar_TimeBetweeenTaunts.value.f)
extern cGVar GVar_TimeBetweeenWarnings;
#define g_TimeBetweeenWarnings (GVar_TimeBetweeenWarnings.value.f)
/* the callback the world puts a new entity at its origin with (set by Logic_ctor) */
extern void *g_SetOriginFunc;

/* MaintenanceLogic.cpp, 0xdc bytes: the maintenance robot of the ship editor walks around the ship on the
 * platform (states 7: wait for the ship, 8: work) */
typedef struct cMaintenanceLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	vec3_t points[8];		/* 0x68 the corners of the box around the ship */
	vec3_t reposition;		/* 0xc8 "RepositionPoint" */
	int point;			/* 0xd4 the corner it walks to, -1 */
	float timer;			/* 0xd8 MaintenanceTimeToNextPos */
} cMaintenanceLocalLogic;
SD_OFFSET(cMaintenanceLocalLogic, reposition, 0xc8);
SD_SIZE(cMaintenanceLocalLogic, 0xdc);

#define CMAINTENANCELOCALLOGIC_VFTABLE GAME_VFTABLE(cMaintenanceLocalLogic_vftable)
extern cEntity *g_MaintenanceShip;	/* the ship on the platform */
extern cEntity *g_MaintenanceFirstShip;	/* the one the work began with */
extern cGVar GVar_MaintenanceTimeToNextPos;
#define g_MaintenanceTimeToNextPos (GVar_MaintenanceTimeToNextPos.value.f)
extern cGVar GVar_MaintenanceSphereRadiusFactor;
#define g_MaintenanceSphereRadiusFactor (GVar_MaintenanceSphereRadiusFactor.value.f)

/* MayorLogic.cpp, 0xe8 bytes */
typedef struct cMayorLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	char listener[0x80];		/* 0x68 the character that talks to the mayor, "" none */
} cMayorLocalLogic;
SD_SIZE(cMayorLocalLogic, 0xe8);

#define CMAYORLOCALLOGIC_VFTABLE GAME_VFTABLE(cMayorLocalLogic_vftable)
extern cGVar GVar_MayorBlabliblaRadius;
#define g_MayorBlabliblaRadius (GVar_MayorBlabliblaRadius.value.f)
/* the search of the sphere native: the mayor, the nearest dazed character in its view and the square distance */
extern cEntity *g_MayorSearchEnt;
extern cEntity *g_MayorTarget;
extern float g_MayorTargetDist;

#define CMERCENARYLOCALLOGIC_VFTABLE GAME_VFTABLE(cMercenaryLocalLogic_vftable)

/* MessengerLogic.cpp: the messengers (cMessengers, 0x14 bytes, of cGenCharGroup) */
typedef struct cMessengers {
	cEntity *desk;			/* 0x00 the desk of the data pack of the player */
	cEntity *target;		/* 0x04 the desk of the messenger with the pack */
	float timer;			/* 0x08 5 s */
	int count;			/* 0x0c the most desks of the type seen, 5 */
	cEntityListData *list;		/* 0x10 "Messenger" */
} cMessengers;
SD_SIZE(cMessengers, 0x14);

#define CMESSENGERLOCALLOGIC_VFTABLE GAME_VFTABLE(cMessengerLocalLogic_vftable)
extern cGVar GVar_MessengerTubeState;
#define g_MessengerTubeState (GVar_MessengerTubeState.value.i)
#define g_MessengerTubeStateDef (GVar_MessengerTubeState.def.i)
extern cGVar GVar_MessengerDropPrice;
#define g_MessengerDropPrice (GVar_MessengerDropPrice.value.i)
#define g_MessengerDropPriceDef (GVar_MessengerDropPrice.def.i)
#define g_MessengerDropCallback (((cPy_Function *)&GAME_GVARFUNC(GVar_MessengerDropCallback)->func))
extern cGVar GVar_MessengerDataPackMissionArrow;
#define g_MessengerDataPackMissionArrow (GVar_MessengerDataPackMissionArrow.value.i)
#define g_MessengerDataPackMissionArrowDef (GVar_MessengerDataPackMissionArrow.def.i)
extern cGVar GVar_MessengerGotoSpeed;
#define g_MessengerGotoSpeed (GVar_MessengerGotoSpeed.value.f)

/* a random desk of the type of the tube state; count grows to the desks seen */
cEntity *cMessengers_FindDesk(cMessengers *self);
/* the state of every messenger but e (and not on the radar), -1: back to its default state */
void cMessengers_SetStateAll(cMessengers *self, int state, cEntity *e);

/* StrMoney.c (g_pWorld + 0x2078): the money of the world (cWorld.money) as digits, one changes per update */
typedef struct cStrMoney {
	char digits[16];		/* 0x00 "0000000000000000", 1..15 change */
	char end;			/* 0x10 0 */
	char len;			/* 0x11 the digits from the first one not "0" */
	byte pad_12[2];
	float timer;			/* 0x14 StrMoneyUpdate */
	int money;			/* 0x18 cWorld.money */
} cStrMoney;
SD_OFFSET(cStrMoney, money, 0x18);
/* the digits from the first one not "0", "0" for none */
const char *cStrMoney_GetStr(cStrMoney *self);

#define g_OnMoneyReachCallback (((cPy_Function *)&GAME_GVARFUNC(GVar_OnMoneyReachCallback)->func))
extern cGVar GVar_OnMoneyReachValue;
#define g_OnMoneyReachValue (GVar_OnMoneyReachValue.value.i)
extern cGVar GVar_StrMoneyUpdate;
#define g_StrMoneyUpdate (GVar_StrMoneyUpdate.value.f)

/* NurseLogic.cpp: the nurses (cLogicControl.nurses) take the dazed characters away; the logic is 0x4c bytes */
typedef struct cNurseLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float time;			/* 0x48 of the escape (state 2) or the pick up (3) */
} cNurseLocalLogic;
SD_SIZE(cNurseLocalLogic, 0x4c);

#define CNURSELOCALLOGIC_VFTABLE GAME_VFTABLE(cNurseLocalLogic_vftable)
extern cGVar GVar_NurseBankerDetect;
#define g_NurseBankerDetect (GVar_NurseBankerDetect.value.f)
extern cGVar GVar_NursePoliceRefuse;
#define g_NursePoliceRefuse (GVar_NursePoliceRefuse.value.f)
extern cGVar GVar_NurseEscapeTime;
#define g_NurseEscapeTime (GVar_NurseEscapeTime.value.f)

void *cNurses_ctor(cNurses *self);
void cNurses_Init(cNurses *self);
void cNurses_Update(cNurses *self, float dt);
void *cNurseLocalLogic_ctor(void *self, cEntity *parent);
/* the state of the nurse with its AI */
byte cNurseLocalLogic_SetState(cNurseLocalLogic *self, int state, cEntity *target);
extern cGVar GVar_NurseRelaxSpeed;
#define g_NurseRelaxSpeed (GVar_NurseRelaxSpeed.value.f)
extern cGVar GVar_NurseAlertSpeed;
#define g_NurseAlertSpeed (GVar_NurseAlertSpeed.value.f)

/* ---- part 4: the taunts, parking zones, ship regeneration, Rusty, the scrapyards, sentinels, Sputnik and the
 * traffic ---- */
/* the native function of the sphere tests (cWorld_SphereCallF) of the logics */
#define g_LogicSphereFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0))

/* Taunts (between NurseLogic.cpp and Parking.cpp): OnTaunt(ship, race, kind) */
#define g_OnTaunt (((cPy_Function *)&GAME_GVARFUNC(GVar_OnTaunt)->func))
extern cGVar GVar_PhoneCabWarningDist;
#define g_PhoneCabWarningDist (GVar_PhoneCabWarningDist.value.f)
extern cGVar GVar_TimeBetweeenTaunts2;
#define g_TimeBetweeenTaunts2 (GVar_TimeBetweeenTaunts2.value.f)	/* while the boss controls the robots */
/* the kind of the police taunt of the ship while the boss controls the robots (3, 4, 6), -1 for none now */
int Taunts_PoliceKind(cEntity *ship);
/* the taunt of the pilot of the ship (only kind 8 for ships without LaunchTaunts, and that one every second time) */
void Taunts_Ship(cEntity *ship, int kind);
/* the ship that attacks usr in its car taunts it */
void Taunts_Attacker(cEntity *usr, cEntity *ship);
cParking *Parking_ctor(cParking *self);
cParkingNode *Parking_NewNode(cParkingList *list);
void Parking_ClearList(cParkingList *list);
void Parking_FreePool(cParkingList *list);
void cParkingZone_dtor(cParkingZone *self);
void Parking_ListAdd(cParkingList *list, const cParkingZone *zone);
/* one zone of each ring checked; 1 when the player is not in a ship */
int Parking_Think(cParking *self);
/* the comparisons of Parking_Find (farest: d > best, else d <= best) */
byte Parking_Farther(float d, float best);
byte Parking_Nearer(float d, float best);
void Parking_Native(cAbstractFile *params);
void Parking_dtor(cParking *self);
void Parking_Update(cParking *self, float dt);
cShipRegen *ShipRegen_ctor(cShipRegen *self);
void ShipRegen_dtor(cShipRegen *self);
void ShipRegen_Update(cShipRegen *self, float dt);
/* the points of the ship of SetShipToRegenerate (LifeReachedCallback at LifeToLaunchCallback) and its bar */
void ShipRegen_UpdateShip(cShipRegen *self, float dt);
/* the group of the ships of the list of that name ("Mosquitos", "MadHunters") */
cShipGroup *cShipGroup_ctor(cShipGroup *self, const char *list);
void cShipGroup_dtor(cShipGroup *self);
void cShipGroup_Init(cShipGroup *self);
void cShipGroup_Update(cShipGroup *self, float dt);
void cShipGroup_SetLair(cShipGroup *self, const char *name, const vec3_t *pos);
byte cShipGroup_DeleteLair(cShipGroup *self, const char *name);
void cShipGroup_GetLair(cShipGroup *self, const char *name, vec3_t *out);
cScrapyardLocalLogic *cScrapyardLocalLogic_Delete(cScrapyardLocalLogic *self, byte flags);
byte cScrapyardLocalLogic_VarV(cScrapyardLocalLogic *self, const char *name, double *x, double *y, double *z,
	byte set);
cLocalLogic *ShipGroup_MakeLogic(cEntity *e);
byte ShipGroup_OnAIEnd(cAIVehicle *ai);
void ShipGroup_SphereNative(cAbstractFile *params);
void ShipGroup_DeathNative(cAbstractFile *params);
cEnemiesTraffic *cEnemiesTraffic_ctor(cEnemiesTraffic *self);
void cEnemiesTraffic_dtor(cEnemiesTraffic *self);
void cEnemiesTraffic_Init(cEnemiesTraffic *self);
void cEnemiesTraffic_Update(cEnemiesTraffic *self, float dt);
void cEnemiesTraffic_Think(cEnemiesTraffic *self);
void cEnemiesTraffic_RemoveFarthest(cEnemiesTraffic *self);
byte Traffic_FightChance(cEntity *a, cEntity *b);
cEntity *Traffic_FindOther(cEntity *e);
byte Traffic_OnAIEnd(cAIVehicle *ai);
void Traffic_DamageNative(cAbstractFile *params);
void Traffic_DeathNative(cAbstractFile *params);

/* RustyLogic.cpp, 0x84 bytes: states 7 the walk to a junk point, 8 the work at it, 9 pissed at the platform */
typedef struct cRustyLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	cEntity *junk;			/* 0x68 the junk point */
	byte pad_6c[0x84 - 0x6c];
} cRustyLocalLogic;
SD_SIZE(cRustyLocalLogic, 0x84);

cRustyLocalLogic *cRustyLocalLogic_Delete(cRustyLocalLogic *self, byte flags);
void Rusty_LoopDone(cAbstractFile *params);
byte cRustyLocalLogic_VarI(cRustyLocalLogic *self, const char *name, int *i, byte set);
byte cRustyLocalLogic_VarS(cRustyLocalLogic *self, const char *name, char **s, byte set);
void cRustyLocalLogic_Update(cRustyLocalLogic *self, float dt);
void cRustyLocalLogic_NextJunk(cRustyLocalLogic *self);
byte cRustyLocalLogic_SetState(cRustyLocalLogic *self, int state, cEntity *target);

/* SputnikJunkyardLogic.cpp: a cGenCharLocalLogic, states 7 the walk, 8 the laugh in front of the camera */
cSputnikJunkyardLocalLogic *cSputnikJunkyardLocalLogic_Delete(cSputnikJunkyardLocalLogic *self, byte flags);
void Sputnik_LaughDone(cAbstractFile *params);
void cSputnikJunkyardLocalLogic_Update(cSputnikJunkyardLocalLogic *self, float dt);
byte cSputnikJunkyardLocalLogic_SetState(cSputnikJunkyardLocalLogic *self, int state, cEntity *target);

/* PoliceBossLogic.cpp: a cGenCharLocalLogic */
cPoliceBossLocalLogic *cPoliceBossLocalLogic_Delete(cPoliceBossLocalLogic *self, byte flags);
cEntity *cPoliceBossLocalLogic_FindTarget(cPoliceBossLocalLogic *self);
void cPoliceBossLocalLogic_ToAlert(cPoliceBossLocalLogic *self, cAIChar *ai);
void cPoliceBossLocalLogic_ToRelax(cPoliceBossLocalLogic *self, cAIChar *ai);
void cPoliceBossLocalLogic_Update(cPoliceBossLocalLogic *self, float dt);
/* every TimeBetweeenTaunts (outdoor maps), dt is not used */
void Taunts_Think(float dt);
/* the phone cab warning with the alarm; 1 when the player is near one */
byte Taunts_Warnings(float dt);

#endif
