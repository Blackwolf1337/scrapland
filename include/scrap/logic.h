/*
 * Game logic (original sources: Logic/): the alarm of the world (AlarmStatus.cpp), the local logics of the
 * characters (BaseLogic.cpp, the character types after it) and of the ships of the multiplayer modes (CTFEnemies,
 * CTFFriends, DeathMatch, Domination). An entity list of the world makes the local logic of an entity that joins
 * it (cEntityListData.func) and keeps it in cEntity.lLogic. The logic control of the world (g_pWorld->logic,
 * cLogicControl in include/scrap/worldlogic.h) holds the mode objects.
 */
#ifndef SCRAP_LOGIC_H
#define SCRAP_LOGIC_H

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/alarm.h"
#include "scrap/collisiongrid.h"
#include "scrap/cstring.h"
#include "scrap/dlist.h"
#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/pyfunction.h"

#define LOGIC_EMPTY GAME_STR(str_Empty, 0x3a3228)	/* "" of the exe */

/* ---- AlarmStatus.cpp: the cAlarm of include/scrap/alarm.h ---- */

/* engine variables (Gvars) */
#define g_AlarmCallback (GAME_ARRAY_IN(cPy_Function, GVar_AlarmCallback, 16, 0x44cb1c))	/* AlarmCallback (on) */
#define g_AlarmFallDelta GAME_GLOBAL_IN(float, GVar_AlarmFallDelta, 16, 0x44caa4)
#define g_AlarmFallMission GAME_GLOBAL_IN(float, GVar_AlarmFallMission, 16, 0x44cb04)
#define g_AlarmGrowDelta GAME_GLOBAL_IN(float, GVar_AlarmGrowDelta, 16, 0x44cad4)
#define g_EasyAlarmFallFactor GAME_GLOBAL_IN(float, GVar_EasyAlarmFallFactor, 16, 0x44caec)
#define g_GuaranteedChars GAME_GLOBAL_IN(char *, GVar_GuaranteedChars, 16, 0x44cb58)		/* ",type1,type2," */
#define g_PoliceTauntRace (GAME_ARRAY_IN(cString, GVar_PoliceTauntRace, 16, 0x44cabc))	/* the default at +4 */
#define g_UsrDefChar (GAME_ARRAY_IN(cString, GVar_UsrDefChar, 16, 0x44cb40))
#define g_GenCharAttackActive GAME_GLOBAL_IN(int, GVar_GenCharAttackActive, 16, 0x44d1dc)

/* a taunt of the ship of that name ("" none), kind 1 or 2 (rva 0x155210) */
void Taunts_Play(const char *ship, const char *race, int kind);

/* ---- the local logic of an entity ---- */
typedef struct cLocalLogic cLocalLogic;

/* the slots of cLocalLogic (0x00 .. 0x18), cBaseLocalLogic (.. 0x60), cGenCharLocalLogic (.. 0x8c) and
 * cFunctionaryLocalLogic (.. 0xb0); the variable slots take the name of a script variable */
typedef struct cLocalLogicVtbl {
	void *(__thiscall *Delete)(void *self, byte flags);					/* 0x00 */
	const char *(__thiscall *GetName)(void *self);					/* 0x04 "BaseLogic" */
	byte(__thiscall *Vf02)(void *self);							/* 0x08 */
	byte(__thiscall *VarI)(void *self, const char *name, int *i, byte set);		/* 0x0c */
	byte(__thiscall *VarF)(void *self, const char *name, double *d, byte set);		/* 0x10 */
	byte(__thiscall *VarS)(void *self, const char *name, char **s, byte set);		/* 0x14 */
	byte(__thiscall *VarV)(void *self, const char *name, double *x, double *y, double *z, byte set); /* 0x18 */
	byte(__thiscall *UpdateBusy)(void *self);						/* 0x1c */
	void(__thiscall *Refresh)(void *self);						/* 0x20 */
	void *vf09;
	void(__thiscall *OnHit)(void *self, cEntity *attacker);				/* 0x28 */
	void(__thiscall *OnSeen)(void *self, cEntity *e);					/* 0x2c vf11 */
	void *vf12;
	void *vf13;
	void *vf14;
	void *vf15;
	void(__thiscall *OnAlarm)(void *self, cEntity *e);				/* 0x40 vf16 */
	void(__thiscall *Update)(void *self, float dt);					/* 0x44 */
	void(__thiscall *SetOriginFrom)(void *self, cEntity *e);				/* 0x48 */
	byte(__thiscall *SetState)(void *self, int state, cEntity *target);			/* 0x4c vf19 */
	void(__thiscall *SetIdle)(void *self, byte idle);					/* 0x50 */
	void(__thiscall *SetActionRadius)(void *self, float radius);				/* 0x54 */
	void(__thiscall *SetOrigin)(void *self, float x, float y, float z);			/* 0x58 */
	void(__thiscall *SetOriginAng)(void *self, float x, float y, float z);		/* 0x5c */
	byte(__thiscall *RestartState)(void *self);						/* 0x60 */
	byte(__thiscall *CanAttack)(void *self, cEntity *e);		/* 0x64 vf25 the special attack against e */
	void(__thiscall *UpdateView)(void *self);			/* 0x68 vf26 the view flag (+0x381) and the team */
	void(__thiscall *ToRelax)(void *self, void *ai);			/* 0x6c vf27 the AI to its zone, relaxed */
	void(__thiscall *ToAlert)(void *self, void *ai);			/* 0x70 vf28 the same in alert */
	void(__thiscall *Accuse)(void *self, cEntity *e);			/* 0x74 vf29 state 2 against e */
	cEntity *(__thiscall *FindTarget)(void *self);			/* 0x78 vf30 of the special attack */
	byte(__thiscall *TargetDone)(void *self);				/* 0x7c vf31 the special attack ends */
	void(__thiscall *Work)(void *self, float dt);				/* 0x80 vf32 functionaries */
	void(__thiscall *Vf33)(void *self, byte restart);			/* 0x84 */
	void(__thiscall *GoTo)(void *self, void *ai, cEntity *e);		/* 0x88 vf34 the AI walks to e */
	void(__thiscall *UpdateGetIt)(void *self, void *ai, float dt);	/* 0x8c vf35 state 10 */
	byte(__thiscall *CanTalk)(void *self);				/* 0x90 vf36 */
	void(__thiscall *StopWork)(void *self, byte restart);			/* 0x94 vf37 */
	vec3_t *(__thiscall *GetWorkAng)(void *self, vec3_t *out);		/* 0x98 vf38 */
	byte(__thiscall *FastEscape)(void *self);				/* 0x9c vf39 */
	void(__thiscall *AssignDesktop)(void *self, float maxDist);				/* 0xa0 vf40 */
	void(__thiscall *ReleaseDesktop)(void *self);					/* 0xa4 vf41 */
} cLocalLogicVtbl;
SD_OFFSET(cLocalLogicVtbl, UpdateView, 0x68);
SD_OFFSET(cLocalLogicVtbl, Work, 0x80);
SD_OFFSET(cLocalLogicVtbl, FastEscape, 0x9c);
SD_OFFSET(cLocalLogicVtbl, Update, 0x44);
SD_OFFSET(cLocalLogicVtbl, SetState, 0x4c);
SD_OFFSET(cLocalLogicVtbl, RestartState, 0x60);
SD_OFFSET(cLocalLogicVtbl, AssignDesktop, 0xa0);

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

#define CLOCALLOGIC_VFTABLE GAME_VFTABLE(cLocalLogic_vftable, 0x3bd340)
#define CBASELOCALLOGIC_VFTABLE GAME_VFTABLE(cBaseLocalLogic_vftable, 0x3bd360)
#define CBANKDIRECTORLOCALLOGIC_VFTABLE GAME_VFTABLE(cBankDirectorLocalLogic_vftable, 0x3bcf2c)
#define CBANKMASTERLOCALLOGIC_VFTABLE GAME_VFTABLE(cBankMasterLocalLogic_vftable, 0x3bcfc0)
#define CBERTOLOCALLOGIC_VFTABLE GAME_VFTABLE(cBertoLocalLogic_vftable, 0x3bd578)
#define CBETTYLOCALLOGIC_VFTABLE GAME_VFTABLE(cBettyLocalLogic_vftable, 0x3bd680)
#define CBISHOPLOCALLOGIC_VFTABLE GAME_VFTABLE(cBishopLocalLogic_vftable, 0x3bd7e4)
#define CBOSSLOCALLOGIC_VFTABLE GAME_VFTABLE(cBossLocalLogic_vftable, 0x3bd8f8)
#define CCRAZYDEALERLOCALLOGIC_VFTABLE GAME_VFTABLE(cCrazyDealerLocalLogic_vftable, 0x3bda0c)
#define CCTFENEMIESLOCALLOGIC_VFTABLE GAME_VFTABLE(cCTFEnemiesLocalLogic_vftable, 0x3bdb54)
#define CDESKTOPLOCALLOGIC_VFTABLE GAME_VFTABLE(cDesktopLocalLogic_vftable, 0x3bdccc)
#define CFUNCTIONARYLOCALLOGIC_VFTABLE GAME_VFTABLE(cFunctionaryLocalLogic_vftable, 0x3be748)
#define CGATEKEEPERLOCALLOGIC_VFTABLE GAME_VFTABLE(cGateKeeperLocalLogic_vftable, 0x3bec3c)
#define CGEARLOCALLOGIC_VFTABLE GAME_VFTABLE(cGearLocalLogic_vftable, 0x3beeec)
#define CGENCHARLOCALLOGIC_VFTABLE GAME_VFTABLE(cGenCharLocalLogic_vftable, 0x3bf5ec)


/* the type name of the action class of a character */
static inline const char *Logic_TypeName(cEntity *e) {
	cActCtrl *ctrl = WithLife_Vtbl(e)->GetActCtrl(e);
	return ctrl->cls->name;
}

/* the radar marker of an object (cObjEntity.radarShow, "RadarShow") */
#define Logic_RadarShow(e) (((cObjEntity *)(e))->radarShow)

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
#define WChar_Link(e) (&WChar(e)->collide)
#define WChar_Radius(e) (WLife(e)->prim.radius)
#define WChar_ControlTurn(e) (WChar(e)->control.vRot)

/* engine variables of the logics */
#define g_DefActionRadius GAME_GLOBAL_IN(float, GVar_DefActionRadius, 16, 0x44cbb8)
#define g_GearRageMaxTime GAME_GLOBAL_IN(float, GVar_GearRageMaxTime, 16, 0x44cbd0)
#define g_LogicAwakeTime GAME_GLOBAL_IN(float, GVar_LogicAwakeTime, 16, 0x44cb88)
#define g_NotCleanableByGear GAME_GLOBAL_IN(char *, GVar_NotCleanableByGear, 16, 0x44cba0)
#define g_BankDirectorAgressive GAME_GLOBAL_IN(int, GVar_BankDirectorAgressive, 16, 0x44cb70)
#define g_BishopAttackOnAlarm GAME_GLOBAL_IN(int, GVar_BishopAttackOnAlarm, 16, 0x44cc00)
#define g_BossWalkingTime GAME_GLOBAL_IN(float, GVar_BossWalkingTime, 16, 0x44cc18)
#define g_BossWorkPlace GAME_GLOBAL_IN(char *, GVar_BossWorkPlace, 16, 0x44cc30)
#define g_BossWorkingTime GAME_GLOBAL_IN(float, GVar_BossWorkingTime, 16, 0x44cc48)
#define g_BossWasIdle GAME_GLOBAL(byte, g_BossWasIdle, 0x44cc50)		/* the boss was idle while at work */
#define g_EnergyBarActive GAME_GLOBAL_IN(int, GVar_EnergyBarActive, 16, 0x44726c)
#define g_SpecialActionActive GAME_GLOBAL_IN(int, GVar_SpecialActionActive, 16, 0x46067c)
#define g_GenCharRelaxSpeed GAME_GLOBAL_IN(float, GVar_GenCharRelaxSpeed, 16, 0x44d17c)
#define g_GenCharSpecialActionTime GAME_GLOBAL_IN(float, GVar_GenCharSpecialActionTime, 16, 0x44d254)
#define g_GenCharMinSpecialActionTime GAME_GLOBAL_IN(float, GVar_GenCharMinSpecialActionTime, 16, 0x44d2fc)
#define g_GenCharStdFar GAME_GLOBAL_IN(float, GVar_GenCharStdFar, 16, 0x44d2b4)
#define g_GenCharStdFovX GAME_GLOBAL_IN(float, GVar_GenCharStdFovX, 16, 0x44d20c)
#define g_GenCharStdFovY GAME_GLOBAL_IN(float, GVar_GenCharStdFovY, 16, 0x44d2cc)

/* the logic of a character or NULL */
cLocalLogic *Logic_GetLocalLogic(cEntity *e);
/* puts the entity on the floor; 0 if not possible */
byte Logic_SetOnFloor(cEntity *e);
/* a character on the floor below it (floorDist above the hit), 0 and an error without a floor */
byte Logic_CharOnFloor(cEntity *e);
/* flash at pos with radius; the last argument is -1 from the script module */
int Logic_Flash(const vec3_t *pos, float radius, int arg);

void __thiscall Taunts_UpdateEndTime(void *taunts);
void __thiscall ShipRegen_Set(void *shipRegen, cEntity *ship, float speed, float endSpeed);
void __thiscall AICtrl_SendSentinelToWatch(void *aiCtrl, const vec3_t *pos);
void __thiscall AICtrl_GearAttack(void *aiCtrl, cEntity *target, int arg);
/* a character that saw the player goes to accuse it ("Sending %s to acuse the main character"); 1 if one went */
byte __thiscall AICtrl_SendAccuser(void *aiCtrl, byte check);

/* BaseLogic.cpp */
cBaseLocalLogic *__thiscall cBaseLocalLogic_ctor(cBaseLocalLogic *self, cEntity *parent);
void __fastcall cBaseLocalLogic_dtor(cBaseLocalLogic *self);
void __fastcall cLocalLogic_dtor(cLocalLogic *self);
/* pos is in the view of viewer (dist, the field of view, a free line from pos to it) */
byte Logic_InView(cEntity *viewer, const vec3_t *pos, float dist, float fovY, float fovX, byte testLine);
/* the first character of the list (not viewer) in the view of viewer that filter takes (NULL: all) */
cEntity *Logic_FindInView(cEntity *viewer, cEntityListData *list, float dist, float fovY, float fovX, byte testLine,
	byte (*filter)(cEntity *));
byte __thiscall cBaseLocalLogic_UpdateBusy(cBaseLocalLogic *self);
/* the VarV of cLocalLogic: no variable */
byte __thiscall cLocalLogic_VarVNone(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set);
int __thiscall cBaseLocalLogic_SetState(cBaseLocalLogic *self, int state, cEntity *target);
byte __thiscall cBaseLocalLogic_VarF(cBaseLocalLogic *self, const char *name, double *d, byte set);
byte __thiscall cBaseLocalLogic_VarI(cBaseLocalLogic *self, const char *name, int *i, byte set);
byte __thiscall cBaseLocalLogic_VarS(cBaseLocalLogic *self, const char *name, char **s, byte set);
byte __thiscall cBaseLocalLogic_VarV(cBaseLocalLogic *self, const char *name, double *x, double *y, double *z,
	byte set);
void __thiscall cBaseLocalLogic_Update(cBaseLocalLogic *self, float dt);
void __thiscall cBaseLocalLogic_SetIdle(cBaseLocalLogic *self, byte idle);
/* the debug texts of ShowAI (a character node of a list with the state names, the ships) */
void Logic_ShowChar(Nto1Node *node, const char **names);
void Logic_ShowChars(Nto1Node *node, const char **names, byte visible);
void Logic_ShowShip(cEntity *e);
void Logic_ShowShips(void);
void Logic_ShowShipList(const char *name, byte visible);
void Logic_ShowAI(byte visible);
/* the slots of the AI of a character the logics call (the debug texts, the walks) */
typedef struct cLogicAICharVtbl {
	void *vf00_12[19];
	byte(__thiscall *Stopped)(void *self);				/* 0x4c cAIMove vf19: the byte at 0x85 */
	void *vf14_1e[11];
	int(__thiscall *GetState)(void *self);				/* 0x7c */
	void *vf20_21[2];
	void(__thiscall *GetStateName)(void *self, char *out);		/* 0x88 */
	void *vf23;
	void(__thiscall *GetSubStateName)(void *self, char *out);	/* 0x90 */
	byte(__thiscall *Vf94)(void *self);				/* 0x94 slot 0x28 of its control (0x19c) */
	void *vf26_28[3];
	void(__thiscall *GetGoal)(void *self, void *out);		/* 0xa4 a point at +8 of out */
} cLogicAICharVtbl;
SD_OFFSET(cLogicAICharVtbl, Stopped, 0x4c);
SD_OFFSET(cLogicAICharVtbl, GetState, 0x7c);
SD_OFFSET(cLogicAICharVtbl, GetStateName, 0x88);
SD_OFFSET(cLogicAICharVtbl, Vf94, 0x94);
SD_OFFSET(cLogicAICharVtbl, GetGoal, 0xa4);

static inline const cLogicAICharVtbl *Logic_CharAIVtbl(void *ai) {
	return *(const cLogicAICharVtbl **)ai;
}

/* the slots without code shared by the logic classes */
byte __thiscall Logic_True(void *self);
byte __thiscall Logic_False(void *self);
cEntity *__thiscall Logic_NoTarget(void *self);
void __thiscall Logic_Nop1(void *self, int arg);
byte __thiscall Logic_False1(void *self, int arg);
/* the bank logics: the player in view with money in the world, the first character in view in the lists */
cEntity *Logic_RichUsrInView(cGenCharLocalLogic *self, cEntity *usr);
cEntity *Logic_FindInCharLists(cEntity *p);
/* Berto and Betty: the desktop of that name, when there is none */
void Logic_LinkDesktop(cFunctionaryLocalLogic *self, const char *name);
/* GenCharLogic.cpp: the random chance in percent of a reaction to attacker (not while idle unless the player) */
byte Logic_Chance(cBaseLocalLogic *l, cEntity *attacker, float chance);

/* GenCharLogic.cpp and FunctionaryLogic.cpp (imports) */
cGenCharLocalLogic *__thiscall cGenCharLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent, int arg);
void __fastcall cGenCharLocalLogic_dtor(cGenCharLocalLogic *self);
void __thiscall cGenCharLocalLogic_Update(cGenCharLocalLogic *self, float dt);
byte __thiscall cGenCharLocalLogic_SetState(cGenCharLocalLogic *self, int state, cEntity *target);
cFunctionaryLocalLogic *__thiscall cFunctionaryLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent);
void __fastcall cFunctionaryLocalLogic_dtor(cFunctionaryLocalLogic *self);
void __thiscall cFunctionaryLocalLogic_Update(cFunctionaryLocalLogic *self, float dt);
/* the desktop of the functionary or NULL */
cEntity *__fastcall cFunctionaryLocalLogic_GetDesktop(cFunctionaryLocalLogic *self);
/* the desktop leaves its work action */
void __fastcall cFunctionaryLocalLogic_FreeDesktop(cFunctionaryLocalLogic *self);
void __thiscall cFunctionaryLocalLogic_ReleaseDesktop(cFunctionaryLocalLogic *self);
void __thiscall cFunctionaryLocalLogic_SetKeepWorking(cFunctionaryLocalLogic *self, byte on);
void __thiscall cFunctionaryLocalLogic_NormalSpeed(cFunctionaryLocalLogic *self, cEntity *e);
void __thiscall cFunctionaryLocalLogic_SlowEffect(cFunctionaryLocalLogic *self, cEntity *e);
void Functionary_FastEffect(cEntity *e);
void Functionary_WorkRelax(cEntity *e);
/* GenCharLogic.cpp (imports): the variables, vf12 (a robbery seen) */
byte __thiscall cGenCharLocalLogic_VarI(cGenCharLocalLogic *self, const char *name, int *i, byte set);
byte __thiscall cGenCharLocalLogic_VarS(cGenCharLocalLogic *self, const char *name, char **s, byte set);
void __thiscall cGenCharLocalLogic_OnRobbery(cGenCharLocalLogic *self, cEntity *e, float amount);
/* the name of the default loop of the action controller, "" without one (rva 0x62c80) */
const char *__thiscall cActCtrl_GetDefLoopName(void *ctrl);

/* ---- the multiplayer modes (of cLogicControl) ---- */
/* the AI vehicle slots the modes use */
typedef struct cLogicAIVehicleVtbl {
	void *vf00_0d[14];
	void *(__thiscall *GetAttack)(cAIVehicle *self);	/* 0x38 the attack data, the target at +8 */
	void *vf0f_11[3];
	cEntity *(__thiscall *GetTarget)(cAIVehicle *self);	/* 0x48 the same target */
} cLogicAIVehicleVtbl;
SD_OFFSET(cLogicAIVehicleVtbl, GetAttack, 0x38);
SD_OFFSET(cLogicAIVehicleVtbl, GetTarget, 0x48);

static inline const cLogicAIVehicleVtbl *Logic_AIVtbl(cAIVehicle *ai) {
	return *(const cLogicAIVehicleVtbl **)ai;
}

static inline cEntity *Logic_AttackTarget(cAIVehicle *ai) {
	return *(cEntity **)((byte *)Logic_AIVtbl(ai)->GetAttack(ai) + 8);
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


#define g_LeaderFlag GAME_GLOBAL_IN(char *, GVar_LeaderFlag, 16, 0x44cc64)
#define g_PercEnemiesAttackPlayerWhenWinning GAME_GLOBAL_IN(float, GVar_PercEnemiesAttackPlayerWhenWinning, 16, 0x44cc94)
#define g_PlayerCTFTeamIsWinning GAME_GLOBAL_IN(int, GVar_PlayerCTFTeamIsWinning, 16, 0x44cc7c)
#define g_PercFriendsAttackLeaderWhenWinning GAME_GLOBAL_IN(float, GVar_PercFriendsAttackLeaderWhenWinning, 16, 0x44cd30)
#define g_CTFFlagName (GAME_ARRAY(char, g_CTFFlagName, 0x44cca0))		/* [0x80] the flag the leader goes for */

/* the ship e as a "Car" or NULL */
cEntity *Logic_AsCar(cEntity *e);
/* the "Car" of that name or NULL */
cEntity *Logic_FindCar(const char *name);

cCTFEnemies *__fastcall cCTFEnemies_ctor(cCTFEnemies *self);
void __fastcall cCTFEnemies_dtor(cCTFEnemies *self);
cEntityListData *__fastcall cCTFEnemies_Init(cCTFEnemies *self);
cEntityListData *__fastcall cCTFEnemies_GetList(cCTFEnemies *self);
char *__fastcall cCTFEnemies_GetLeader(cCTFEnemies *self);
void __thiscall cCTFEnemies_SetLeader(cCTFEnemies *self, const char *name);
void __thiscall cCTFEnemies_SetBonus(cCTFEnemies *self, float t);
void __thiscall cCTFEnemies_Update(cCTFEnemies *self, float dt);
void __fastcall cCTFEnemiesData_Think(cCTFEnemiesData *self);
cCTFFriends *__fastcall cCTFFriends_ctor(cCTFFriends *self);
void __fastcall cCTFFriends_dtor(cCTFFriends *self);
void __fastcall cCTFFriends_Init(cCTFFriends *self);
cEntityListData *__fastcall cCTFFriends_GetList(cCTFFriends *self);
void __thiscall cCTFFriends_Update(cCTFFriends *self, float dt);
void __fastcall cCTFFriendsData_Think(cCTFFriendsData *self);
cDeathMatch *__fastcall cDeathMatch_ctor(cDeathMatch *self);
void cDeathMatch_dtor(void);
void __fastcall cDeathMatch_Init(cDeathMatch *self);
void __thiscall cDeathMatch_Update(cDeathMatch *self, float dt);
void __fastcall cDeathMatch_Think(cDeathMatch *self);

/* Domination.cpp: the zones of SLogic.SetDominationZones and the baton */
#define MAX_DOMIN_ZONES 4

typedef struct DominZone {
	cEntity *zone;			/* 0x0 */
	int state;			/* 0x4 0 free, 1 and 2 by a side, 3 and 4 a ship of a side on its way */
} DominZone;

#define g_DominNumZones GAME_GLOBAL(int, g_DominNumZones, 0x44cd38)
#define g_DominZones (GAME_ARRAY(DominZone, g_DominZones, 0x44cd3c))		/* [MAX_DOMIN_ZONES] */
#define g_DominBatonPos GAME_GLOBAL(vec3_t, g_DominBatonPos, 0x44cda4)
#define g_DominBatonState GAME_GLOBAL(int, g_DominBatonState, 0x44cdb0)
#define g_DominFightingFreezeCadenceShoot GAME_GLOBAL_IN(int, GVar_DominFightingFreezeCadenceShoot, 16, 0x44cdf4)
#define g_DominFightingFreezeControlAim GAME_GLOBAL_IN(int, GVar_DominFightingFreezeControlAim, 16, 0x44cdc4)
#define g_DominMaxRangeDist GAME_GLOBAL_IN(float, GVar_DominMaxRangeDist, 16, 0x44cd84)
#define g_DominMinRangeDist GAME_GLOBAL_IN(float, GVar_DominMinRangeDist, 16, 0x44ce0c)
#define g_DominWithinRangeDist GAME_GLOBAL_IN(float, GVar_DominWithinRangeDist, 16, 0x44cddc)
#define g_MaxDominDistChase GAME_GLOBAL_IN(float, GVar_MaxDominDistChase, 16, 0x44cd6c)
#define g_MinDominDistChase GAME_GLOBAL_IN(float, GVar_MinDominDistChase, 16, 0x44cd9c)

typedef struct cDominationLogicVtbl {
	cDominationLogic *(__thiscall *Delete)(cDominationLogic *self, byte flags);	/* 0x00 */
	void(__thiscall *Init)(cDominationLogic *self);				/* 0x04 makes the list */
	void(__thiscall *Update)(cDominationLogic *self, float dt);			/* 0x08 */
	cEntityListData *(__thiscall *GetList)(cDominationLogic *self);		/* 0x0c the list of the side */
	int(__thiscall *GetTeam)(cDominationLogic *self);				/* 0x10 0 enemies, 1 friends */
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

#define CDOMINATIONLOGIC_VFTABLE GAME_VFTABLE(cDominationLogic_vftable, 0x3be14c)
#define CDOMINATIONENEMIESLOGIC_VFTABLE GAME_VFTABLE(cDominationEnemiesLogic_vftable, 0x3be164)
#define CDOMINATIONFRIENDSLOGIC_VFTABLE GAME_VFTABLE(cDominationFriendsLogic_vftable, 0x3be1ec)
/* the engine variable LeaderName (the leader ship of the enemies) and the death callbacks of the sides */
#define g_DominLeaderName GAME_GLOBAL_IN(char *, GVar_LeaderName, 16, 0x44ce48)
#define g_OnDominEnemyDeath (GAME_ARRAY_IN(cPy_Function, GVar_OnDominEnemyDeath, 16, 0x44ce24))
#define g_OnDominFriendDeath (GAME_ARRAY_IN(cPy_Function, GVar_OnDominFriendDeath, 16, 0x44ce60))

void Domination_AddZone(cEntity *zone);
void Domination_SetZoneState(int zone, int state);
void Domination_SetBatonState(const vec3_t *pos, int state);
cEntity *__thiscall cDominationLogic_GetZone(cDominationLogic *self, cAIVehicle *ai);
cEntity *__thiscall cDominationLogic_FindEnemy(cDominationLogic *self, const vec3_t *pos, cEntity *other,
	float maxDist);
void __thiscall cDominationLogic_UpdateShip(cDominationLogic *self, cEntity *v, cEntity *target, cEntity *baton);
int __thiscall cDominationLogic_NearestZone(cDominationLogic *self, const vec3_t *pos);
void cDominationLogic_ctor(cDominationLogic *self);
cDominationEnemiesLogic *__fastcall cDominationEnemiesLogic_ctor(cDominationEnemiesLogic *self);
void __fastcall cDominationEnemiesLogic_dtor(cDominationEnemiesLogic *self);
void __fastcall cDominationLogic_dtor(cDominationLogic *self);
/* DominationEnemies.cpp and DominationFriends.cpp */
cEntity *__thiscall cDominationEnemiesLogic_GetLeader(cDominationEnemiesLogic *self);
void __fastcall cDominationSide_Think(cDominationSide *self);
void __fastcall cDominationFriendsSide_Think(cDominationFriendsSide *self);
/* the zone of the ship of that name taken from the table (a zone on the way of the side is free again) */
cEntity *__thiscall cDominationEnemiesLogic_ReleaseZone(cDominationLogic *self, const char *name);
cEntity *__thiscall cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name);
cEntity *Domination_ReleaseZone(cDominationLogic *self, const char *name, int going, int held);
void Domination_OnDeath(cAbstractFile *params, cDominationLogic *side,
	cEntity *(__thiscall *release)(cDominationLogic *, const char *), cPy_Function *cb);
/* the ship of the side that goes to zone, NULL for none */
cEntity *__thiscall cDominationFriendsLogic_FindShipOfZone(cDominationFriendsLogic *self, cEntity *zone);

/* ---- the ships of the outdoor missions (of cLogicControl) ---- */
/* the empty destructors of the mode objects and the empty natives of their lists */
void __fastcall Logic_NopDtor(void *self);
void Logic_NativeNop(cAbstractFile *params);
/* a random car of the list (rva 0x154b20), NULL for none */
cEntity *Logic_RandomCar(cEntityListData *list);

#define CENEMIESLOCALLOGIC_VFTABLE GAME_VFTABLE(cEnemiesLocalLogic_vftable, 0x3be240)

cEnemies *__fastcall cEnemies_ctor(cEnemies *self);
void __thiscall cEnemies_Attack(cEnemies *self, cEntity *usr);
void __fastcall cEnemies_Think(cEnemies *self);
void __fastcall cEnemies_Relax(cEnemies *self);

#define g_ExtPoliceChasing GAME_GLOBAL_IN(int, GVar_DoResetOutPolice, 16, 0x44cea8)		/* the cars were sent after the player */
#define g_PoliceWaveEliminated (GAME_ARRAY_IN(cPy_Function, GVar_PoliceWaveEliminated, 16, 0x44ce84))	/* no police car left */
#define g_TrafficFightingMaxDist GAME_GLOBAL_IN(float, GVar_TrafficFightingMaxDist, 16, 0x44dc40)
#define Entity_ArrowCanMission(e) (WLife(e)->arrow.canMission)

/* the AI parameters of a police car back to the defaults or random (rva 0x15b2f0, 0x15b3c0) */
void PoliceAI_RestoreParams(cAIVehicle *ai);
void PoliceAI_RandomParams(cAIVehicle *ai);
/* a flag of the attack part of the AI (+0x194, its +0x174), rva 0x8bef0 */
byte __thiscall cAIVehicle_ControlFlag(cAIVehicle *ai);

cExtPolice *__fastcall cExtPolice_ctor(cExtPolice *self);
cEntity *ExtPolice_CallCop(cEntity *e);
void ExtPolice_CopDamage(const char *name, const char *attacker, byte died);
void __fastcall cExtPolice_Think(cExtPolice *self);
byte ExtPolice_IsChased(cEntity *e);


cFriends *__fastcall cFriends_ctor(cFriends *self);
void __thiscall cFriends_Attack(cFriends *self, cEntity *usr);
void __fastcall cFriends_Think(cFriends *self);

/* the logic of a gatekeeper (0x4c bytes): states 0 relaxed, 1 hidden in its shell */
typedef struct cGateKeeperLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float hiddenTime;		/* 0x48 GateKeeperHiddenTime when it hides */
} cGateKeeperLocalLogic;
SD_SIZE(cGateKeeperLocalLogic, 0x4c);

#define g_GateKeeperStdFar GAME_GLOBAL_IN(float, GVar_GateKeeperStdFar, 16, 0x44cfc0)
#define g_GateKeeperStdFovY GAME_GLOBAL_IN(float, GVar_GateKeeperStdFovY, 16, 0x44cfd8)
#define g_GateKeeperStdFovX GAME_GLOBAL_IN(float, GVar_GateKeeperStdFovX, 16, 0x44d008)
#define g_GateKeeperHiddenTime GAME_GLOBAL_IN(float, GVar_GateKeeperHiddenTime, 16, 0x44cff0)

/* the slot of the logics with one argument that returns 1 */
byte __thiscall Logic_True1(void *self, int arg);
cGateKeeperLocalLogic *__thiscall cGateKeeperLocalLogic_ctor(cGateKeeperLocalLogic *self, cEntity *parent);
byte __thiscall cGateKeeperLocalLogic_SetState(cGateKeeperLocalLogic *self, int state, cEntity *target);

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
#define WChar_ConeStatus(e) (WLife(e)->arrow.coneStatus)
#define WChar_ConeFov(e) (WLife(e)->arrow.coneFov)
#define WChar_ConeDist(e) (WLife(e)->arrow.coneDist)

/* the AI control: level to at least level (rva 0x14fb00), the gear becomes a police (rva 0x14eb50) */
void __thiscall AICtrl_RaiseLevel(void *aiCtrl, int level);
byte __thiscall AICtrl_GearToPolice(void *aiCtrl, cEntity *gear);

byte __thiscall cGearLocalLogic_SetState(cGearLocalLogic *self, int state, cEntity *target);


cGenCharGroup *__fastcall cGenCharGroup_ctor(cGenCharGroup *self);
void __fastcall cGenCharGroup_dtor(cGenCharGroup *self);
void __fastcall cGenCharGroup_Init(cGenCharGroup *self);
void __thiscall cGenCharGroup_Update(cGenCharGroup *self, float dt);
int __fastcall cGenCharGroup_GetLevel(cGenCharGroup *self);
void __thiscall cGenCharGroup_RaiseLevel(cGenCharGroup *self, int level);
byte __fastcall cGenCharGroup_TakeAttack(cGenCharGroup *self);
/* the maker of the logics of the GenChar lists (cEntityListData.func, rva 0x14b860) */
#define GENCHARGROUP_MAKELOGIC 0x14b860

/* the Init and Update of the mode objects of part 2 */
void __fastcall cExtPolice_Init(cExtPolice *self);
void __thiscall cExtPolice_Update(cExtPolice *self, float dt);
void __fastcall cEnemies_Init(cEnemies *self);
void __thiscall cEnemies_Update(cEnemies *self, float dt);
void __fastcall cFriends_Init(cFriends *self);
void __thiscall cFriends_Update(cFriends *self, float dt);
cGateKeepers *__fastcall cGateKeepers_ctor(cGateKeepers *self);
void __fastcall cGateKeepers_Init(cGateKeepers *self);
void __thiscall cGateKeepers_Update(cGateKeepers *self, float dt);
cDominationFriendsLogic *__fastcall cDominationFriendsLogic_ctor(cDominationFriendsLogic *self);
void __fastcall cDominationFriendsLogic_dtor(cDominationFriendsLogic *self);

/* MessengerLogic.cpp */
void *__thiscall cMessengers_ctor(void *self);
void __fastcall cMessengers_dtor(void *self);
void __fastcall cMessengers_Init(void *self);
void __fastcall cMessengers_Think(void *self);
/* the messenger m with the pack at the desk: 1 while the drop goes on */
byte Messenger_Deliver(cEntity *m, cEntity *desk, cEntity *pack);

/* the constructors of the logics of the other types (later sources of Logic/, imports) */
void *__thiscall cLittlePoliceLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cMayorLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cRustyLocalLogic_ctor(void *self, cEntity *parent);
cSputnikJunkyardLocalLogic *__thiscall cSputnikJunkyardLocalLogic_ctor(cSputnikJunkyardLocalLogic *self,
	cEntity *parent);
void *__thiscall cMaintenanceLocalLogic_ctor(void *self, cEntity *parent);
cPoliceBossLocalLogic *__thiscall cPoliceBossLocalLogic_ctor(cPoliceBossLocalLogic *self, cEntity *parent);
cMessengerLocalLogic *__thiscall cMessengerLocalLogic_ctor(cMessengerLocalLogic *self, cEntity *parent);
cMercenaryLocalLogic *__thiscall cMercenaryLocalLogic_ctor(cMercenaryLocalLogic *self, cEntity *parent);
cHumphreyLocalLogic *__thiscall cHumphreyLocalLogic_ctor(cHumphreyLocalLogic *self, cEntity *parent);
void *__thiscall cHumanLocalLogic_ctor(void *self, cEntity *parent);
/* the logics of part 1 */
cBankDirectorLocalLogic *__thiscall cBankDirectorLocalLogic_ctor(cBankDirectorLocalLogic *self, cEntity *parent);
cBankMasterLocalLogic *__thiscall cBankMasterLocalLogic_ctor(cBankMasterLocalLogic *self, cEntity *parent);
cDesktopLocalLogic *__thiscall cDesktopLocalLogic_ctor(cDesktopLocalLogic *self, cEntity *parent);
cBertoLocalLogic *__thiscall cBertoLocalLogic_ctor(cBertoLocalLogic *self, cEntity *parent);
cBettyLocalLogic *__thiscall cBettyLocalLogic_ctor(cBettyLocalLogic *self, cEntity *parent);
cBishopLocalLogic *__thiscall cBishopLocalLogic_ctor(cBishopLocalLogic *self, cEntity *parent);
cCrazyDealerLocalLogic *__thiscall cCrazyDealerLocalLogic_ctor(cCrazyDealerLocalLogic *self, cEntity *parent);
cBossLocalLogic *__thiscall cBossLocalLogic_ctor(cBossLocalLogic *self, cEntity *parent);

/* GenCharLogic.cpp: the states (GENCHAR_ of the CHECK of DefaultState); 7 and 8 are of the police */
#define GENCHAR_RELAX 0
#define GENCHAR_ALERT 1
#define GENCHAR_ACCUSE 2
#define GENCHAR_GETAWAY 3
#define GENCHAR_ATTACK 4		/* the special attack against the target */
#define GENCHAR_CHASE 5
#define GENCHAR_STAY 6
#define GENCHAR_GET_IT 10		/* walk to the target */

#define g_GenCharMaxSpecialActionTime GAME_GLOBAL_IN(float, GVar_GenCharMaxSpecialActionTime, 16, 0x44d2e4)
#define g_GenCharAlertSpeed GAME_GLOBAL_IN(float, GVar_GenCharAlertSpeed, 16, 0x44d194)
#define g_OnDefStateCallback (GAME_ARRAY_IN(cPy_Function, GVar_OnDefStateCallback, 16, 0x44d128))	/* (name) after RestartState */
#define g_GenCharAcuseAgainTime GAME_GLOBAL_IN(float, GVar_GenCharAcuseAgainTime, 16, 0x44d14c)
#define g_GenCharRobberyDetection GAME_GLOBAL_IN(float, GVar_GenCharRobberyDetection, 16, 0x44d164)
#define g_GenCharAcuseTime GAME_GLOBAL_IN(float, GVar_GenCharAcuseTime, 16, 0x44d1ac)
#define g_GenCharSpecialChaseSpeed GAME_GLOBAL_IN(float, GVar_GenCharSpecialChaseSpeed, 16, 0x44d1c4)
#define g_GenCharAttackSpeed GAME_GLOBAL_IN(float, GVar_GenCharAttackSpeed, 16, 0x44d224)
#define g_GenCharGetAwayTime GAME_GLOBAL_IN(float, GVar_GenCharGetAwayTime, 16, 0x44d23c)
#define g_GenCharAttackLimit GAME_GLOBAL_IN(float, GVar_GenCharAttackLimit, 16, 0x44d26c)
#define g_GenCharPoliceRefuse GAME_GLOBAL_IN(float, GVar_GenCharPoliceRefuse, 16, 0x44d284)
#define g_GenCharEscapeSpeed GAME_GLOBAL_IN(float, GVar_GenCharEscapeSpeed, 16, 0x44d29c)
/* the type of the player draws the attention of a generic character (rva 0x14c5a0) */
byte Logic_IsSuspectType(const char *type);
byte __thiscall cGenCharLocalLogic_RestartState(cGenCharLocalLogic *self);
void __thiscall cGenCharLocalLogic_OnHit(cGenCharLocalLogic *self, cEntity *attacker);
void __thiscall cGenCharLocalLogic_GoTo(cGenCharLocalLogic *self, cAIChar *ai, cEntity *e);
byte __thiscall cGenCharLocalLogic_CanAttack(cGenCharLocalLogic *self, cEntity *e);
/* the special action over, the next target search in a random time */
void __fastcall cGenCharLocalLogic_ResetSpecial(cGenCharLocalLogic *self);
/* the action Discover of a character that stands */
void __fastcall cGenCharLocalLogic_Discover(cGenCharLocalLogic *self);
/* SetState(2, e): the vf29 of the GenChars and the vf16 of the nurses */
void __thiscall Logic_SetState2(cBaseLocalLogic *self, cEntity *e);
/* the empty slots with two arguments */
void __thiscall Logic_Nop2(void *self, int a, int b);
byte __thiscall Logic_True2(void *self, int a, int b);

/* HumanLogic.cpp, 0x5c bytes: the characters near the human flee from it */
typedef struct cHumanLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	cPy_Function func;		/* 0x48 HumanLogic_Escape */
} cHumanLocalLogic;
SD_SIZE(cHumanLocalLogic, 0x5c);

#define CHUMANLOCALLOGIC_VFTABLE GAME_VFTABLE(cHumanLocalLogic_vftable, 0x3bf75c)
#define g_HumanEnt GAME_GLOBAL(cEntity *, g_HumanEnt, 0x44d304)	/* the parent of the last human logic made */

/* HumphreyLogic.cpp: a cFunctionaryLocalLogic (isDefault: "Humphrey_Default") at the desktop "HumphreyDesktop" */
#define CHUMPHREYLOCALLOGIC_VFTABLE GAME_VFTABLE(cHumphreyLocalLogic_vftable, 0x3bf7e0)

/* InfItem.cpp: the item list of the world (cInfItem of worldlogic.h, g_pWorld->logic.items, 100 bytes), DLists of
 * cItemDesc nodes: the life items and the ammo items of three kinds */
typedef struct cInfItemNode {
	struct cInfItemNode *next;	/* 0x0 */
	struct cInfItemNode *prev;	/* 0x4 */
	cItemDesc item;			/* 0x8 */
} cInfItemNode;
SD_SIZE(cInfItemNode, 0x10);

#define CINFITEM_VFTABLE GAME_VFTABLE(cInfItem_vftable, 0x3bf8d0)

cInfItem *__fastcall cInfItem_ctor(cInfItem *self);
void __fastcall cInfItem_dtor(cInfItem *self);

#define g_CreatePolice (GAME_ARRAY_IN(cPy_Function, GVar_CreatePolice, 16, 0x44d330))	/* (name, x, y, z) */
#define g_CreateGear (GAME_ARRAY_IN(cPy_Function, GVar_CreateGear, 16, 0x44d37c))	/* (name, x, y, z) */
#define g_PoliceLogicBelligerant GAME_GLOBAL_IN(int, GVar_PoliceLogicBelligerant, 16, 0x44d354)
#define g_SentinelNextGotoTime GAME_GLOBAL_IN(float, GVar_SentinelNextGotoTime, 16, 0x44daa8)
#define g_SentinelAbortyNPCAttDist GAME_GLOBAL_IN(float, GVar_SentinelAbortyNPCAttDist, 16, 0x44db38)

void *__fastcall AICtrl_ctor(void *aiCtrl);
void __fastcall AICtrl_dtor(void *aiCtrl);
void __fastcall AICtrl_Init(void *aiCtrl);
void __thiscall AICtrl_Update(void *aiCtrl, float dt);
/* the police e may become a gear (the gear "Gear" + the name after "Police" does not exist) */
byte __thiscall AICtrl_CanFuse(void *aiCtrl, cEntity *e);
/* the police becomes a gear (into *gear, may be NULL); 0 if it may not */
byte __thiscall AICtrl_Fuse(void *aiCtrl, cEntity *police, cEntity **gear);
/* the sentinel nearest to e goes for it (state 6), only before the alarm (rva 0x14f960) */
void __thiscall AICtrl_SendSentinelToNPC(void *aiCtrl, cEntity *e);
/* the gears that chase the player go for e, without one the gear attack on e (rva 0x14fba0) */
void __thiscall AICtrl_GearsChase(void *aiCtrl, cEntity *e);

/* the sentinels (SentinelLogic.cpp, the source after this part, imports) */
/* the logic of a sentinel, 0x50 bytes */
typedef struct cSentinelLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float time;			/* 0x48 of the state (the follow, the watch) */
	byte seen;			/* 0x4c the player was in view at the last think */
	byte pad_4d[3];
} cSentinelLocalLogic;
SD_SIZE(cSentinelLocalLogic, 0x50);

void *__thiscall cSentinelLocalLogic_ctor(void *self, cEntity *parent);
/* to the place pos to watch it */
void __thiscall cSentinelLocalLogic_Watch(void *self, const vec3_t *pos);
/* back to state 0 (1 with the alarm), unless idle */
void __fastcall cSentinelLocalLogic_Return(void *self);
void __fastcall cSentinelLocalLogic_dtor(cSentinelLocalLogic *self);
cSentinelLocalLogic *__thiscall cSentinelLocalLogic_Delete(cSentinelLocalLogic *self, byte flags);
void __thiscall cSentinelLocalLogic_OnAlarm(cSentinelLocalLogic *self, cEntity *e);
byte __thiscall cSentinelLocalLogic_Seen(cSentinelLocalLogic *self);
void __thiscall cSentinelLocalLogic_Think(cSentinelLocalLogic *self, float dt, byte quiet);
void __thiscall cSentinelLocalLogic_OnRobbery(cSentinelLocalLogic *self, cEntity *e, float amount);
void __thiscall cSentinelLocalLogic_OnAttack(cSentinelLocalLogic *self, cEntity *e);
void __thiscall cSentinelLocalLogic_OnUsr(cSentinelLocalLogic *self, cEntity *e);
byte __thiscall cSentinelLocalLogic_GivesMoney(cSentinelLocalLogic *self, cEntity *e);
void __thiscall cSentinelLocalLogic_UpdateState(cSentinelLocalLogic *self, cEntity *p, float dt);
byte __thiscall cSentinelLocalLogic_RestartState(cSentinelLocalLogic *self);
byte __thiscall cSentinelLocalLogic_SetState(cSentinelLocalLogic *self, int state, cEntity *target);
/* the voice event of the character of that name (Sound/, rva 0x1a1090) */
byte Sound_PlayVoice(const char *name, const char *event, int arg, int force);
/* GearLogic.cpp */
cGearLocalLogic *__thiscall cGearLocalLogic_ctor(cGearLocalLogic *self, cEntity *parent);
byte __thiscall cGearLocalLogic_Call(cGearLocalLogic *self, cEntity *e);

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

#define CLITTLEPOLICELOCALLOGIC_VFTABLE GAME_VFTABLE(cLittlePoliceLocalLogic_vftable, 0x3bfc50)
#define g_LittlePoliceStopAcusingMaxDistFromUser GAME_GLOBAL_IN(float, GVar_LittlePoliceStopAcusingMaxDistFromUser, 16, 0x44d3a0)
#define g_LittlePoliceMaxDistFar GAME_GLOBAL_IN(float, GVar_LittlePoliceMaxDistFar, 16, 0x44d3b8)
#define g_LittlePoliceAsksForMoney GAME_GLOBAL_IN(int, GVar_LittlePoliceAsksForMoney, 16, 0x44d3d0)
#define g_LittlePoliceChasingTime GAME_GLOBAL_IN(float, GVar_LittlePoliceChasingTime, 16, 0x44d3e8)
#define g_MainMissionStopDist GAME_GLOBAL_IN(float, GVar_MainMissionStopDist, 16, 0x44d400)
#define g_LittlePoliceArrived GAME_GLOBAL(byte, g_LittlePoliceArrived, 0x44d408)	/* GoTo left the AI in state 4 */
#define g_PoliceStealAmount GAME_GLOBAL_IN(int, GVar_PoliceStealAmount, 16, 0x446ff0)

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

cShipGroupData *__thiscall cShipGroupData_ctor(cShipGroupData *self, const char *list);
void __thiscall cShipGroupData_SetLair(cShipGroupData *self, const char *name, const vec3_t *pos);
void __fastcall cShipGroupData_Think(cShipGroupData *self);

#define CLOGICCONTROL_VFTABLE GAME_VFTABLE(cLogicControl_vftable, 0x3bfe20)
#define g_RemainTalkTime GAME_GLOBAL_IN(float, GVar_RemainTalkTime, 16, 0x462148)
#define g_TimeBetweeenTaunts GAME_GLOBAL_IN(float, GVar_TimeBetweeenTaunts, 16, 0x44d648)
#define g_TimeBetweeenWarnings GAME_GLOBAL_IN(float, GVar_TimeBetweeenWarnings, 16, 0x44d6b4)
/* the callback the world puts a new entity at its origin with (set by Logic_ctor) */
#define g_SetOriginFunc GAME_GLOBAL(void *, g_SetOriginFunc, 0x444730)

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

#define CMAINTENANCELOCALLOGIC_VFTABLE GAME_VFTABLE(cMaintenanceLocalLogic_vftable, 0x3bff4c)
#define g_MaintenanceShip GAME_GLOBAL(cEntity *, g_MaintenanceShip, 0x44d43c)	/* the ship on the platform */
#define g_MaintenanceFirstShip GAME_GLOBAL(cEntity *, g_MaintenanceFirstShip, 0x44d440)	/* the one the work began with */
#define g_MaintenanceTimeToNextPos GAME_GLOBAL_IN(float, GVar_MaintenanceTimeToNextPos, 16, 0x44d454)
#define g_MaintenanceSphereRadiusFactor GAME_GLOBAL_IN(float, GVar_MaintenanceSphereRadiusFactor, 16, 0x44d46c)

/* MayorLogic.cpp, 0xe8 bytes */
typedef struct cMayorLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	char listener[0x80];		/* 0x68 the character that talks to the mayor, "" none */
} cMayorLocalLogic;
SD_SIZE(cMayorLocalLogic, 0xe8);

#define CMAYORLOCALLOGIC_VFTABLE GAME_VFTABLE(cMayorLocalLogic_vftable, 0x3c0074)
#define g_MayorBlabliblaRadius GAME_GLOBAL_IN(float, GVar_MayorBlabliblaRadius, 16, 0x446c10)
/* the search of the sphere native: the mayor, the nearest dazed character in its view and the square distance */
#define g_MayorSearchEnt GAME_GLOBAL(cEntity *, g_MayorSearchEnt, 0x44d478)
#define g_MayorTarget GAME_GLOBAL(cEntity *, g_MayorTarget, 0x44d474)
#define g_MayorTargetDist GAME_GLOBAL(float, g_MayorTargetDist, 0x428ba4)

#define CMERCENARYLOCALLOGIC_VFTABLE GAME_VFTABLE(cMercenaryLocalLogic_vftable, 0x3c0108)

/* MessengerLogic.cpp: the messengers (cMessengers, 0x14 bytes, of cGenCharGroup) */
typedef struct cMessengers {
	cEntity *desk;			/* 0x00 the desk of the data pack of the player */
	cEntity *target;		/* 0x04 the desk of the messenger with the pack */
	float timer;			/* 0x08 5 s */
	int count;			/* 0x0c the most desks of the type seen, 5 */
	cEntityListData *list;		/* 0x10 "Messenger" */
} cMessengers;
SD_SIZE(cMessengers, 0x14);

#define CMESSENGERLOCALLOGIC_VFTABLE GAME_VFTABLE(cMessengerLocalLogic_vftable, 0x3c0440)
#define g_MessengerTubeState GAME_GLOBAL_IN(int, GVar_MessengerTubeState, 16, 0x44d48c)
#define g_MessengerTubeStateDef GAME_GLOBAL_IN(int, GVar_MessengerTubeState, 20, 0x44d490)
#define g_MessengerDropPrice GAME_GLOBAL_IN(int, GVar_MessengerDropPrice, 16, 0x44d4e0)
#define g_MessengerDropPriceDef GAME_GLOBAL_IN(int, GVar_MessengerDropPrice, 20, 0x44d4e4)
#define g_MessengerDropCallback (GAME_ARRAY_IN(cPy_Function, GVar_MessengerDropCallback, 16, 0x44d4f8))
#define g_MessengerDataPackMissionArrow GAME_GLOBAL_IN(int, GVar_MessengerDataPackMissionArrow, 16, 0x44d534)
#define g_MessengerDataPackMissionArrowDef GAME_GLOBAL_IN(int, GVar_MessengerDataPackMissionArrow, 20, 0x44d538)
#define g_MessengerGotoSpeed GAME_GLOBAL_IN(float, GVar_MessengerGotoSpeed, 16, 0x44d564)

/* a random desk of the type of the tube state; count grows to the desks seen */
cEntity *__fastcall cMessengers_FindDesk(cMessengers *self);
/* the state of every messenger but e (and not on the radar), -1: back to its default state */
void __thiscall cMessengers_SetStateAll(cMessengers *self, int state, cEntity *e);

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
const char *__fastcall cStrMoney_GetStr(cStrMoney *self);

#define g_OnMoneyReachCallback (GAME_ARRAY_IN(cPy_Function, GVar_OnMoneyReachCallback, 16, 0x44d57c))
#define g_OnMoneyReachValue GAME_GLOBAL_IN(int, GVar_OnMoneyReachValue, 16, 0x44d5a0)
#define g_StrMoneyUpdate GAME_GLOBAL_IN(float, GVar_StrMoneyUpdate, 16, 0x44d5b8)

/* NurseLogic.cpp: the nurses (cLogicControl.nurses) take the dazed characters away; the logic is 0x4c bytes */
typedef struct cNurseLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float time;			/* 0x48 of the escape (state 2) or the pick up (3) */
} cNurseLocalLogic;
SD_SIZE(cNurseLocalLogic, 0x4c);

#define CNURSELOCALLOGIC_VFTABLE GAME_VFTABLE(cNurseLocalLogic_vftable, 0x3c06d4)
#define g_NurseBankerDetect GAME_GLOBAL_IN(float, GVar_NurseBankerDetect, 16, 0x44d5d0)
#define g_NursePoliceRefuse GAME_GLOBAL_IN(float, GVar_NursePoliceRefuse, 16, 0x44d5e8)
#define g_NurseEscapeTime GAME_GLOBAL_IN(float, GVar_NurseEscapeTime, 16, 0x44d600)

void *__fastcall cNurses_ctor(cNurses *self);
void __fastcall cNurses_Init(cNurses *self);
void __thiscall cNurses_Update(cNurses *self, float dt);
void *__thiscall cNurseLocalLogic_ctor(void *self, cEntity *parent);
/* the state of the nurse with its AI */
byte __thiscall cNurseLocalLogic_SetState(cNurseLocalLogic *self, int state, cEntity *target);
#define g_NurseRelaxSpeed GAME_GLOBAL_IN(float, GVar_NurseRelaxSpeed, 16, 0x44d630)
#define g_NurseAlertSpeed GAME_GLOBAL_IN(float, GVar_NurseAlertSpeed, 16, 0x44d618)

/* ---- part 4: the taunts, parking zones, ship regeneration, Rusty, the scrapyards, sentinels, Sputnik and the
 * traffic ---- */
/* the native function of the sphere tests (cWorld_SphereCallF) of the logics */
#define g_LogicSphereFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0, 0x46679c))

/* Taunts (between NurseLogic.cpp and Parking.cpp): OnTaunt(ship, race, kind) */
#define g_OnTaunt (GAME_ARRAY_IN(cPy_Function, GVar_OnTaunt, 16, 0x44d690))
#define g_PhoneCabWarningDist GAME_GLOBAL_IN(float, GVar_PhoneCabWarningDist, 16, 0x44d660)
#define g_TimeBetweeenTaunts2 GAME_GLOBAL_IN(float, GVar_TimeBetweeenTaunts2, 16, 0x44d678)	/* while the boss controls the robots */
/* the kind of the police taunt of the ship while the boss controls the robots (3, 4, 6), -1 for none now */
int Taunts_PoliceKind(cEntity *ship);
/* the taunt of the pilot of the ship (only kind 8 for ships without LaunchTaunts, and that one every second time) */
void Taunts_Ship(cEntity *ship, int kind);
/* the ship that attacks usr in its car taunts it */
void Taunts_Attacker(cEntity *usr, cEntity *ship);
cParking *__fastcall Parking_ctor(cParking *self);
cParkingNode *__fastcall Parking_NewNode(cParkingList *list);
void __fastcall Parking_ClearList(cParkingList *list);
void __fastcall Parking_FreePool(cParkingList *list);
void __fastcall cParkingZone_dtor(cParkingZone *self);
void __thiscall Parking_ListAdd(cParkingList *list, const cParkingZone *zone);
/* one zone of each ring checked; 1 when the player is not in a ship */
int __fastcall Parking_Think(cParking *self);
/* the comparisons of Parking_Find (farest: d > best, else d <= best) */
byte Parking_Farther(float d, float best);
byte Parking_Nearer(float d, float best);
void Parking_Native(cAbstractFile *params);
void __fastcall Parking_dtor(cParking *self);
void __thiscall Parking_Update(cParking *self, float dt);
cShipRegen *__fastcall ShipRegen_ctor(cShipRegen *self);
void __fastcall ShipRegen_dtor(cShipRegen *self);
void __thiscall ShipRegen_Update(cShipRegen *self, float dt);
/* the points of the ship of SetShipToRegenerate (LifeReachedCallback at LifeToLaunchCallback) and its bar */
void __thiscall ShipRegen_UpdateShip(cShipRegen *self, float dt);
/* the group of the ships of the list of that name ("Mosquitos", "MadHunters") */
cShipGroup *__thiscall cShipGroup_ctor(cShipGroup *self, const char *list);
void __fastcall cShipGroup_dtor(cShipGroup *self);
void __fastcall cShipGroup_Init(cShipGroup *self);
void __thiscall cShipGroup_Update(cShipGroup *self, float dt);
void __thiscall cShipGroup_SetLair(cShipGroup *self, const char *name, const vec3_t *pos);
byte __thiscall cShipGroup_DeleteLair(cShipGroup *self, const char *name);
void __thiscall cShipGroup_GetLair(cShipGroup *self, const char *name, vec3_t *out);
cScrapyardLocalLogic *__thiscall cScrapyardLocalLogic_Delete(cScrapyardLocalLogic *self, byte flags);
byte __thiscall cScrapyardLocalLogic_VarV(cScrapyardLocalLogic *self, const char *name, double *x, double *y, double *z,
	byte set);
cLocalLogic *ShipGroup_MakeLogic(cEntity *e);
byte ShipGroup_OnAIEnd(cAIVehicle *ai);
void ShipGroup_SphereNative(cAbstractFile *params);
void ShipGroup_DeathNative(cAbstractFile *params);
cEnemiesTraffic *__fastcall cEnemiesTraffic_ctor(cEnemiesTraffic *self);
void __fastcall cEnemiesTraffic_dtor(cEnemiesTraffic *self);
void __fastcall cEnemiesTraffic_Init(cEnemiesTraffic *self);
void __thiscall cEnemiesTraffic_Update(cEnemiesTraffic *self, float dt);
void __fastcall cEnemiesTraffic_Think(cEnemiesTraffic *self);
void __fastcall cEnemiesTraffic_RemoveFarthest(cEnemiesTraffic *self);
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

void __fastcall cRustyLocalLogic_dtor(cRustyLocalLogic *self);
cRustyLocalLogic *__thiscall cRustyLocalLogic_Delete(cRustyLocalLogic *self, byte flags);
void Rusty_LoopDone(cAbstractFile *params);
byte __thiscall cRustyLocalLogic_VarI(cRustyLocalLogic *self, const char *name, int *i, byte set);
byte __thiscall cRustyLocalLogic_VarS(cRustyLocalLogic *self, const char *name, char **s, byte set);
void __thiscall cRustyLocalLogic_Update(cRustyLocalLogic *self, float dt);
void __fastcall cRustyLocalLogic_NextJunk(cRustyLocalLogic *self);
byte __thiscall cRustyLocalLogic_SetState(cRustyLocalLogic *self, int state, cEntity *target);

/* SputnikJunkyardLogic.cpp: a cGenCharLocalLogic, states 7 the walk, 8 the laugh in front of the camera */
void __fastcall cSputnikJunkyardLocalLogic_dtor(cSputnikJunkyardLocalLogic *self);
cSputnikJunkyardLocalLogic *__thiscall cSputnikJunkyardLocalLogic_Delete(cSputnikJunkyardLocalLogic *self, byte flags);
void Sputnik_LaughDone(cAbstractFile *params);
void __thiscall cSputnikJunkyardLocalLogic_Update(cSputnikJunkyardLocalLogic *self, float dt);
byte __thiscall cSputnikJunkyardLocalLogic_SetState(cSputnikJunkyardLocalLogic *self, int state, cEntity *target);

/* PoliceBossLogic.cpp: a cGenCharLocalLogic */
void __fastcall cPoliceBossLocalLogic_dtor(cPoliceBossLocalLogic *self);
cPoliceBossLocalLogic *__thiscall cPoliceBossLocalLogic_Delete(cPoliceBossLocalLogic *self, byte flags);
cEntity *__thiscall cPoliceBossLocalLogic_FindTarget(cPoliceBossLocalLogic *self);
void __thiscall cPoliceBossLocalLogic_ToAlert(cPoliceBossLocalLogic *self, cAIChar *ai);
void __thiscall cPoliceBossLocalLogic_ToRelax(cPoliceBossLocalLogic *self, cAIChar *ai);
void __thiscall cPoliceBossLocalLogic_Update(cPoliceBossLocalLogic *self, float dt);
/* every TimeBetweeenTaunts (outdoor maps), dt is not used */
void Taunts_Think(float dt);
/* the phone cab warning with the alarm; 1 when the player is near one */
byte Taunts_Warnings(float dt);

#endif
