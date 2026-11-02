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

extern cGVar GVar_PoliceTauntRace;
extern cGVar GVar_UsrDefChar;

/* ---- AlarmStatus.cpp: the cAlarm of include/scrap/alarm.h ---- */

/* engine variables (Gvars) */
#define g_PoliceTauntRace (((cString *)&GVar_PoliceTauntRace.value))	/* the default at +4 */
#define g_UsrDefChar (((cString *)&GVar_UsrDefChar.value))
extern cGVar GVar_GenCharAttackActive;
#define g_GenCharAttackActive (GVar_GenCharAttackActive.value.i)

/* a taunt of the ship of that name ("" none), kind 1 or 2 */
void Taunts_Play(const char *ship, const char *race, int kind);

/* ---- the local logic of an entity ---- */
typedef struct cLocalLogic cLocalLogic;

/* the slots of cLocalLogic, the logic of a game mode and the base of the character logics; the variable slots take
 * the name of a script variable. All slots of the logic vtables take the logic as cLocalLogic. */
typedef struct cLocalLogicVtbl {
	cLocalLogic *(*Delete)(cLocalLogic *self, byte flags);			/* 0x00 */
	const char *(*GetName)(cLocalLogic *self);				/* 0x04 "BaseLogic" */
	byte(*Vf02)(cLocalLogic *self);						/* 0x08 */
	byte(*VarI)(cLocalLogic *self, const char *name, int *i, byte set);	/* 0x0c */
	byte(*VarF)(cLocalLogic *self, const char *name, double *d, byte set);	/* 0x10 */
	byte(*VarS)(cLocalLogic *self, const char *name, char **s, byte set);	/* 0x14 */
	byte(*VarV)(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set); /* 0x18 */
} cLocalLogicVtbl;

/* the slots of cBaseLocalLogic, the logic of a character */
typedef struct cBaseLocalLogicVtbl {
	cLocalLogicVtbl base;
	byte(*UpdateBusy)(cLocalLogic *self);						/* 0x1c */
	void(*Refresh)(cLocalLogic *self);						/* 0x20 */
	byte(*Listens)(cLocalLogic *self, cEntity *talker);				/* 0x24 vf09 the mayor */
	void(*OnHit)(cLocalLogic *self, cEntity *attacker);				/* 0x28 */
	void(*OnSeen)(cLocalLogic *self, cEntity *e);					/* 0x2c vf11 */
	void(*OnAttack)(cLocalLogic *self, cEntity *attacker, float value);		/* 0x30 vf12 */
	void(*OnShot)(cLocalLogic *self, cEntity *attacker);				/* 0x34 vf13 */
	void(*OnHammer)(cLocalLogic *self, cEntity *nurse);				/* 0x38 vf14 */
	byte(*GivesMoney)(cLocalLogic *self, cEntity *police);				/* 0x3c vf15 */
	void(*OnAlarm)(cLocalLogic *self, cEntity *e);				/* 0x40 vf16 */
	void(*Update)(cLocalLogic *self, float dt);					/* 0x44 */
	void(*SetOriginFrom)(cLocalLogic *self, cEntity *e);				/* 0x48 */
	byte(*SetState)(cLocalLogic *self, int state, cEntity *target);			/* 0x4c vf19 */
	void(*SetIdle)(cLocalLogic *self, byte idle);					/* 0x50 */
	void(*SetActionRadius)(cLocalLogic *self, float radius);				/* 0x54 */
	void(*SetOrigin)(cLocalLogic *self, float x, float y, float z);			/* 0x58 */
	void(*SetOriginAng)(cLocalLogic *self, float x, float y, float z);		/* 0x5c */
	void(*RestartState)(cLocalLogic *self);						/* 0x60 the return value of some is not used */
} cBaseLocalLogicVtbl;

static inline const cBaseLocalLogicVtbl *Logic_Vtbl(const void *l) {
	return *(const cBaseLocalLogicVtbl *const *)l;
}

/* the slots of the logics of the generic characters (cGenCharLocalLogic and its children) */
typedef struct cGenCharLocalLogicVtbl {
	cBaseLocalLogicVtbl base;
	byte(*CanAttack)(cLocalLogic *self, cEntity *e);		/* 0x64 vf25 the special attack against e */
	void(*UpdateView)(cLocalLogic *self);			/* 0x68 vf26 the view flag (+0x381) and the team */
	void(*ToRelax)(cLocalLogic *self, struct cAIChar *ai);			/* 0x6c vf27 the AI to its zone, relaxed */
	void(*ToAlert)(cLocalLogic *self, struct cAIChar *ai);			/* 0x70 vf28 the same in alert */
	void(*Accuse)(cLocalLogic *self, cEntity *e);			/* 0x74 vf29 state 2 against e */
	cEntity *(*FindTarget)(cLocalLogic *self);			/* 0x78 vf30 of the special attack */
	byte(*TargetDone)(cLocalLogic *self);				/* 0x7c vf31 the special attack ends */
	void(*Work)(cLocalLogic *self, float dt);				/* 0x80 vf32 functionaries */
	void(*Vf33)(cLocalLogic *self, byte restart);			/* 0x84 */
	void(*GoTo)(cLocalLogic *self, struct cAIChar *ai, cEntity *e);		/* 0x88 vf34 the AI walks to e */
	void(*UpdateGetIt)(cLocalLogic *self, struct cAIChar *ai, float dt);	/* 0x8c vf35 state 10 */
} cGenCharLocalLogicVtbl;

static inline const cGenCharLocalLogicVtbl *GenChar_Vtbl(const void *l) {
	return *(const cGenCharLocalLogicVtbl *const *)l;
}

/* the functionaries and the desk characters (cFunctionaryLocalLogic, Berto, Betty, Humphrey) */
typedef struct cFunctionaryLocalLogicVtbl {
	cGenCharLocalLogicVtbl base;
	byte(*CanTalk)(cLocalLogic *self);				/* 0x90 vf36 */
	void(*StopWork)(cLocalLogic *self, byte restart);			/* 0x94 vf37 */
	vec3_t *(*GetWorkAng)(cLocalLogic *self, vec3_t *out);		/* 0x98 vf38 */
	byte(*FastEscape)(cLocalLogic *self);				/* 0x9c vf39 */
	void(*AssignDesktop)(cLocalLogic *self, float maxDist);				/* 0xa0 vf40 */
	void(*ReleaseDesktop)(cLocalLogic *self);					/* 0xa4 vf41 */
} cFunctionaryLocalLogicVtbl;

static inline const cFunctionaryLocalLogicVtbl *Functionary_Vtbl(const void *l) {
	return *(const cFunctionaryLocalLogicVtbl *const *)l;
}

/* Berto: three more slots, all empty */
typedef struct cBertoLocalLogicVtbl {
	cFunctionaryLocalLogicVtbl base;
	void(*Vf42)(cLocalLogic *self, int arg);				/* 0xa8 */
	void(*Vf43)(cLocalLogic *self, int arg);				/* 0xac */
	void(*Vf44)(cLocalLogic *self, int arg);				/* 0xb0 */
} cBertoLocalLogicVtbl;

/* the sentinels: the accuse when seen (no other sentinel accuses) and the think */
typedef struct cSentinelLocalLogicVtbl {
	cBaseLocalLogicVtbl base;
	byte(*Seen)(cLocalLogic *self);					/* 0x64 */
	void(*Think)(cLocalLogic *self, float dt, byte other);		/* 0x68 */
} cSentinelLocalLogicVtbl;

static inline const cSentinelLocalLogicVtbl *Sentinel_Vtbl(const void *l) {
	return *(const cSentinelLocalLogicVtbl *const *)l;
}

/* Logic/LocalLogic.h, 8 bytes: the logic of the CTFEnemies ships is this only */
struct cLocalLogic {
	const cLocalLogicVtbl *vftable;	/* 0x00 */
	cEntity *parent;		/* 0x04 */
};

/* BaseLogic.cpp, 0x48 bytes: the logic of a character ("WalkChar") */
typedef struct cBaseLocalLogic {
	union {
		cLocalLogic logic;	/* 0x00 */
		struct {
			const cBaseLocalLogicVtbl *vftable;	/* 0x00 */
			cEntity *parent;	/* 0x04 the character */
		};
	};
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

/* BertoLogic.cpp, BettyLogic.cpp and HumphreyLogic.cpp, 0xbc bytes each: a functionary that takes its desktop at the
 * first update when it is the default character ("Berto_Default", "Betty_Default", "Humphrey_Default") */
typedef struct cBertoLocalLogic {
	cFunctionaryLocalLogic base;	/* 0x00 */
	byte isDefault;			/* 0xb8 */
	byte pad_b9[3];
} cBertoLocalLogic;

typedef struct cBettyLocalLogic {
	cFunctionaryLocalLogic base;	/* 0x00 */
	byte isDefault;			/* 0xb8 */
	byte pad_b9[3];
} cBettyLocalLogic;

typedef struct cHumphreyLocalLogic {
	cFunctionaryLocalLogic base;	/* 0x00 */
	byte isDefault;			/* 0xb8 */
	byte pad_b9[3];
} cHumphreyLocalLogic;

/* BossLogic.cpp, 0x80 bytes */
typedef struct cBossLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	vec3_t workPos;			/* 0x68 the dummy BossWorkPlace */
	float workAng[2];		/* 0x74 */
	float time;			/* 0x7c in the walk or the work */
} cBossLocalLogic;

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

extern cGVar GVar_NotCleanableByGear;
#define g_NotCleanableByGear (GVar_NotCleanableByGear.value.s)
extern cGVar GVar_BankDirectorAgressive;
#define g_BankDirectorAgressive (GVar_BankDirectorAgressive.value.i)
extern cGVar GVar_BishopAttackOnAlarm;
#define g_BishopAttackOnAlarm (GVar_BishopAttackOnAlarm.value.i)
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
byte cBaseLocalLogic_UpdateBusy(cLocalLogic *base);
/* the VarV of cLocalLogic: no variable */
byte cLocalLogic_VarVNone(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set);
byte cLocalLogic_CanTalkTrue(cLocalLogic *self);
byte cLocalLogic_FastEscapeFalse(cLocalLogic *self);
cEntity *cLocalLogic_FindTargetNull(cLocalLogic *self);
byte cLocalLogic_ListensFalse(cLocalLogic *self, cEntity *talker);
byte cLocalLogic_ListensTrue(cLocalLogic *self, cEntity *talker);
void cLocalLogic_OnAlarmNop(cLocalLogic *self, cEntity *e);
void cLocalLogic_OnAttackNop(cLocalLogic *self, cEntity *attacker, float value);
void cLocalLogic_OnHammerNop(cLocalLogic *self, cEntity *nurse);
void cLocalLogic_OnHitNop(cLocalLogic *self, cEntity *attacker);
void cLocalLogic_OnSeenNop(cLocalLogic *self, cEntity *e);
void cLocalLogic_SetOriginFromNop(cLocalLogic *self, cEntity *e);
byte cLocalLogic_SetStateTrue(cLocalLogic *self, int state, cEntity *target);
byte cLocalLogic_TargetDoneFalse(cLocalLogic *self);
byte cLocalLogic_TargetDoneTrue(cLocalLogic *self);
void cLocalLogic_UpdateNop(cLocalLogic *self, float dt);
byte cLocalLogic_VarFFalse(cLocalLogic *self, const char *name, double *d, byte set);
byte cLocalLogic_VarIFalse(cLocalLogic *self, const char *name, int *i, byte set);
byte cLocalLogic_VarSFalse(cLocalLogic *self, const char *name, char **s, byte set);
byte cLocalLogic_Vf02True(cLocalLogic *self);
void cLocalLogic_Vf33Nop(cLocalLogic *self, byte restart);
void cLocalLogic_Vf42Nop(cLocalLogic *self, int arg);
void cLocalLogic_Vf43Nop(cLocalLogic *self, int arg);
void cLocalLogic_Vf44Nop(cLocalLogic *self, int arg);
void cLocalLogic_WorkNop(cLocalLogic *self, float dt);
const char *cLocalLogic_GetName(cLocalLogic *self);
byte cBaseLocalLogic_SetState(cLocalLogic *base, int state, cEntity *target);
byte cBaseLocalLogic_VarF(cLocalLogic *base, const char *name, double *d, byte set);
byte cBaseLocalLogic_VarI(cLocalLogic *base, const char *name, int *i, byte set);
byte cBaseLocalLogic_VarS(cLocalLogic *base, const char *name, char **s, byte set);
byte cBaseLocalLogic_VarV(cLocalLogic *base, const char *name, double *x, double *y, double *z, byte set);
void cBaseLocalLogic_Update(cLocalLogic *base, float dt);
void cBaseLocalLogic_SetIdle(cLocalLogic *base, byte idle);
const char *cBaseLocalLogic_GetName(cLocalLogic *base);
void cBaseLocalLogic_Refresh(cLocalLogic *base);
void cBaseLocalLogic_OnShot(cLocalLogic *base, cEntity *attacker);
void cBaseLocalLogic_SetOriginFrom(cLocalLogic *base, cEntity *e);
void cBaseLocalLogic_SetActionRadius(cLocalLogic *base, float radius);
void cBaseLocalLogic_SetOrigin(cLocalLogic *base, float x, float y, float z);
void cBaseLocalLogic_SetOriginAng(cLocalLogic *base, float x, float y, float z);
byte cBaseLocalLogic_Vf15(cLocalLogic *base, cEntity *arg);
void cBaseLocalLogic_RestartState(cLocalLogic *base);
void Logic_ShowShips(void);
void Logic_ShowAI(byte visible);
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
void cGenCharLocalLogic_Update(cLocalLogic *elem, float dt);
byte cGenCharLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target);
cFunctionaryLocalLogic *cFunctionaryLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent);
void cFunctionaryLocalLogic_dtor(cFunctionaryLocalLogic *self);
void cFunctionaryLocalLogic_Update(cLocalLogic *elem, float dt);
/* the desktop of the functionary or NULL */
cEntity *cFunctionaryLocalLogic_GetDesktop(cFunctionaryLocalLogic *self);
/* the desktop leaves its work action */
void cFunctionaryLocalLogic_FreeDesktop(cFunctionaryLocalLogic *self);
byte cFunctionaryLocalLogic_Vf02(cLocalLogic *elem);
byte cFunctionaryLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set);
byte cFunctionaryLocalLogic_VarF(cLocalLogic *elem, const char *name, double *d, byte set);
byte cFunctionaryLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set);
void cFunctionaryLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount);
byte cFunctionaryLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target);
void cFunctionaryLocalLogic_SetIdle(cLocalLogic *elem, byte idle);
void cFunctionaryLocalLogic_UpdateView(cLocalLogic *elem);
void cFunctionaryLocalLogic_Work(cLocalLogic *elem, float dt);
void cFunctionaryLocalLogic_Vf33(cLocalLogic *elem, byte restart);
void cFunctionaryLocalLogic_StopWork(cLocalLogic *base, byte restart);
/* GenCharLogic.cpp (imports): the variables, vf12 (a robbery seen) */
byte cGenCharLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set);
byte cGenCharLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set);
void cGenCharLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount);

/* ---- the multiplayer modes (of cLogicControl) ---- */
static inline cEntity *Logic_AttackTarget(cAIVehicle *ai) {
	return (cEntity *)ai->vftable->GetTarget(&ai->base.base)->target;
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

/* CTFFriends.cpp: g_pWorld->logic.ctfFriends holds the pointer */
typedef struct cCTFFriendsData {
	cEntityListData *list;		/* 0x00 "CTFFriends" */
	float timer;			/* 0x04 */
	int numAttack;			/* 0x08 the ships attacking the leader */
} cCTFFriendsData;

extern cGVar GVar_PlayerCTFTeamIsWinning;
#define g_PlayerCTFTeamIsWinning (GVar_PlayerCTFTeamIsWinning.value.i)

/* the ship e as a "Car" or NULL */
cEntity *Logic_AsCar(cEntity *e);
/* the "Car" of that name or NULL */
cEntity *Logic_FindCar(const char *name);

cCTFEnemies *cCTFEnemies_ctor(cCTFEnemies *self);
void cCTFEnemies_dtor(cCTFEnemies *self);
cEntityListData *cCTFEnemies_Init(cCTFEnemies *self);
cEntityListData *cCTFEnemies_GetList(cCTFEnemies *self);
char *cCTFEnemies_GetLeader(cCTFEnemies *self);
void cCTFEnemies_Update(cCTFEnemies *self, float dt);
cCTFFriends *cCTFFriends_ctor(cCTFFriends *self);
void cCTFFriends_dtor(cCTFFriends *self);
void cCTFFriends_Init(cCTFFriends *self);
cEntityListData *cCTFFriends_GetList(cCTFFriends *self);
void cCTFFriends_Update(cCTFFriends *self, float dt);
cDeathMatch *cDeathMatch_ctor(cDeathMatch *self);
void cDeathMatch_dtor(void);
void cDeathMatch_Init(cDeathMatch *self);
void cDeathMatch_Update(cDeathMatch *self, float dt);

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

typedef struct cDominationFriendsSide {
	cDominationLogic *owner;	/* 0x0 */
	cEntityListData *list;		/* 0x4 "DominationFriends" */
	float timer;			/* 0x8 */
} cDominationFriendsSide;

void Domination_AddZone(cEntity *zone);
void Domination_SetZoneState(int zone, int state);
void Domination_SetBatonState(const vec3_t *pos, int state);
void cDominationLogic_UpdateShip(cDominationLogic *self, cEntity *v, cEntity *target, cEntity *baton);
int cDominationLogic_NearestZone(cDominationLogic *self, const vec3_t *pos);
void cDominationLogic_ctor(cDominationLogic *self);
cDominationEnemiesLogic *cDominationEnemiesLogic_ctor(cDominationEnemiesLogic *self);
void cDominationEnemiesLogic_dtor(cDominationEnemiesLogic *self);
void cDominationLogic_dtor(cDominationLogic *self);
int cDominationLogic_GetTeamTrue(cDominationLogic *self);
/* DominationEnemies.cpp and DominationFriends.cpp */
cEntity *cDominationEnemiesLogic_GetLeader(cDominationEnemiesLogic *self);
void cDominationEnemiesLogic_Init(cDominationLogic *base);
void cDominationEnemiesLogic_Update(cDominationLogic *base, float dt);
cEntityListData *cDominationEnemiesLogic_GetList(cDominationLogic *base);
cEntity *Domination_ReleaseZone(cDominationLogic *self, const char *name, int going, int held);
void Domination_OnDeath(cAbstractFile *params, cDominationLogic *side,
	cEntity *(*release)(cDominationLogic *, const char *), cPy_Function *cb);

/* ---- the ships of the outdoor missions (of cLogicControl) ---- */
/* the empty destructors of the mode objects and the empty natives of their lists */
void Logic_NopDtor(void *self);
void Logic_NativeNop(cAbstractFile *params);
/* a random car of the list, NULL for none */
cEntity *Logic_RandomCar(cEntityListData *list);

cEnemies *cEnemies_ctor(cEnemies *self);

extern cGVar GVar_TrafficFightingMaxDist;
#define g_TrafficFightingMaxDist (GVar_TrafficFightingMaxDist.value.f)

/* the AI parameters of a police car back to the defaults or random */
void PoliceAI_RestoreParams(cAIVehicle *ai);
void PoliceAI_RandomParams(cAIVehicle *ai);

cExtPolice *cExtPolice_ctor(cExtPolice *self);
cEntity *ExtPolice_CallCop(cEntity *e);
byte ExtPolice_IsChased(cEntity *e);

cFriends *cFriends_ctor(cFriends *self);

/* the logic of a gatekeeper (0x4c bytes): states 0 relaxed, 1 hidden in its shell */
typedef struct cGateKeeperLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float hiddenTime;		/* 0x48 GateKeeperHiddenTime when it hides */
} cGateKeeperLocalLogic;

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

/* the view cone of the arrow of a character (its AI sees in it) */

/* the AI control: level to at least level, the gear becomes a police */
void AICtrl_RaiseLevel(void *aiCtrl, int level);
byte AICtrl_GearToPolice(void *aiCtrl, cEntity *gear);

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
void cBankDirectorLocalLogic_Update(cLocalLogic *base, float dt);
byte cBankDirectorLocalLogic_LoseTarget(cLocalLogic *elem);
cBankMasterLocalLogic *cBankMasterLocalLogic_ctor(cBankMasterLocalLogic *self, cEntity *parent);
cDesktopLocalLogic *cDesktopLocalLogic_ctor(cDesktopLocalLogic *self, cEntity *parent);
cBertoLocalLogic *cBertoLocalLogic_ctor(cBertoLocalLogic *self, cEntity *parent);
void cBertoLocalLogic_Update(cLocalLogic *elem, float dt);
void cBertoLocalLogic_ReleaseDesktop(cLocalLogic *elem);
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
extern cGVar GVar_GenCharEscapeSpeed;
#define g_GenCharEscapeSpeed (GVar_GenCharEscapeSpeed.value.f)
void cGenCharLocalLogic_RestartState(cLocalLogic *elem);
void cGenCharLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker);
void cGenCharLocalLogic_GoTo(cLocalLogic *elem, cAIChar *ai, cEntity *e);
byte cGenCharLocalLogic_CanAttack(cLocalLogic *base, cEntity *e);
/* the action Discover of a character that stands */
void cGenCharLocalLogic_Discover(cGenCharLocalLogic *self);
void cGenCharLocalLogic_OnSeen(cLocalLogic *elem, cEntity *e);
byte cGenCharLocalLogic_GivesMoney(cLocalLogic *elem, cEntity *police);
void cGenCharLocalLogic_OnAlarm(cLocalLogic *elem, cEntity *e);
void cGenCharLocalLogic_UpdateView(cLocalLogic *elem);
void cGenCharLocalLogic_ToRelax(cLocalLogic *elem, cAIChar *ai);
void cGenCharLocalLogic_ToAlert(cLocalLogic *elem, cAIChar *ai);
void cGenCharLocalLogic_UpdateGetIt(cLocalLogic *base, cAIChar *ai, float dt);
/* SetState(2, e): the vf29 of the GenChars and the vf16 of the nurses */
void Logic_SetState2(cLocalLogic *base, cEntity *e);

/* HumanLogic.cpp, 0x5c bytes: the characters near the human flee from it */
typedef struct cHumanLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	cPy_Function func;		/* 0x48 HumanLogic_Escape */
} cHumanLocalLogic;

/* InfItem.cpp: the item list of the world (cInfItem of worldlogic.h, g_pWorld->logic.items, 100 bytes), DLists of
 * cItemDesc nodes: the life items and the ammo items of three kinds */
typedef struct cInfItemNode {
	struct cInfItemNode *next;	/* 0x0 */
	struct cInfItemNode *prev;	/* 0x4 */
	cItemDesc item;			/* 0x8 */
} cInfItemNode;

cInfItem *cInfItem_ctor(cInfItem *self);
void cInfItem_dtor(cInfItem *self);

extern cGVar GVar_SentinelAbortyNPCAttDist;
#define g_SentinelAbortyNPCAttDist (GVar_SentinelAbortyNPCAttDist.value.f)

void *AICtrl_ctor(void *aiCtrl);
void AICtrl_dtor(void *aiCtrl);
void AICtrl_Init(void *aiCtrl);
void AICtrl_Update(void *aiCtrl, float dt);
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

void *cSentinelLocalLogic_ctor(void *self, cEntity *parent);
/* to the place pos to watch it */
void cSentinelLocalLogic_Watch(void *self, const vec3_t *pos);
/* back to state 0 (1 with the alarm), unless idle */
void cSentinelLocalLogic_Return(void *self);
/* the voice event of the character of that name (Sound/, rva 0x1a1090) */
byte Sound_PlayVoice(const char *name, const char *event, const char *arg, int force);
/* GearLogic.cpp */
cGearLocalLogic *cGearLocalLogic_ctor(cGearLocalLogic *self, cEntity *parent);
byte cGearLocalLogic_Call(cGearLocalLogic *self, cEntity *e);

/* LittlePoliceLogic.cpp, 0x78 bytes: the police of the indoor maps (the type "Police") */
/* the vtable of the little police: the GenChar slots and one of its own */
typedef struct cLittlePoliceLocalLogicVtbl {
	cGenCharLocalLogicVtbl base;
	void(*UpdateLook)(cLocalLogic *self);			/* 0x90 vf36 */
} cLittlePoliceLocalLogicVtbl;

typedef struct cLittlePoliceLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 specialTime: the time of the chase in state 4 */
	float sirenTime;		/* 0x68 with the alarm: the next siren */
	float lostTime;			/* 0x6c of the player, 5 when the AI walks to it */
	byte bribe;			/* 0x70 the arrow shows the options of the bribe */
	byte pad_71[3];
	int policeResult;		/* 0x74 "PoliceResult": 1 when it goes to the main mission */
} cLittlePoliceLocalLogic;

extern cGVar GVar_PoliceStealAmount;
#define g_PoliceStealAmount (GVar_PoliceStealAmount.value.i)

/* Parking.cpp: the parking zones of Scrap.AddParkingZone (flag 0: of the ships, 1: of the items), a ring each */
typedef struct cParkingZone {
	vec3_t pos;			/* 0x0 */
	cString name;			/* 0xc "_Junkyard" is not found in ring 0 */
} cParkingZone;

typedef struct cParkingNode {
	struct cParkingNode *next;	/* 0x00 */
	struct cParkingNode *prev;	/* 0x04 */
	cParkingZone zone;		/* 0x08 */
} cParkingNode;

/* ShipRegen.cpp: the life regeneration of the player, and the ship of SLogic.SetShipToRegenerate */
typedef struct cShipRegenShip {
	char name[0x80];		/* 0x00 */
	float time;			/* 0x80 to the next point */
	float rate;			/* 0x84 1 / speed: the time per point at no life */
	float endRate;			/* 0x88 1 / endSpeed: at full life */
	int life;			/* 0x8c at the last update */
} cShipRegenShip;

/* Scrapyard.cpp: a group of ships that guard their lairs ("Mosquitos", "MadHunters"), behind a pointer */
typedef struct cShipGroupData {
	cPy_Function onSphere;		/* 0x00 native: the ships in the sphere of the intruder */
	HashTable lairs;		/* 0x14 of cWorldStrItem: the lair (vec3) of a ship (name) */
	const char *listName;		/* 0x20 */
	cEntityListData *list;		/* 0x24 */
	float timer;			/* 0x28 think every 0.5 s */
} cShipGroupData;

extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)
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

/* MayorLogic.cpp, 0xe8 bytes */
typedef struct cMayorLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	char listener[0x80];		/* 0x68 the character that talks to the mayor, "" none */
} cMayorLocalLogic;

extern cGVar GVar_MayorBlabliblaRadius;
#define g_MayorBlabliblaRadius (GVar_MayorBlabliblaRadius.value.f)

/* MessengerLogic.cpp: the messengers (cMessengers, 0x14 bytes, of cGenCharGroup) */
typedef struct cMessengers {
	cEntity *desk;			/* 0x00 the desk of the data pack of the player */
	cEntity *target;		/* 0x04 the desk of the messenger with the pack */
	float timer;			/* 0x08 5 s */
	int count;			/* 0x0c the most desks of the type seen, 5 */
	cEntityListData *list;		/* 0x10 "Messenger" */
} cMessengers;

extern cGVar GVar_MessengerTubeState;
#define g_MessengerTubeState (GVar_MessengerTubeState.value.i)
#define g_MessengerTubeStateDef (GVar_MessengerTubeState.def.i)

/* StrMoney.c (g_pWorld + 0x2078): the money of the world (cWorld.money) as digits, one changes per update */
typedef struct cStrMoney {
	char digits[16];		/* 0x00 "0000000000000000", 1..15 change */
	char end;			/* 0x10 0 */
	char len;			/* 0x11 the digits from the first one not "0" */
	byte pad_12[2];
	float timer;			/* 0x14 StrMoneyUpdate */
	int money;			/* 0x18 cWorld.money */
} cStrMoney;
/* the digits from the first one not "0", "0" for none */
const char *cStrMoney_GetStr(cStrMoney *self);

/* NurseLogic.cpp: the nurses (cLogicControl.nurses) take the dazed characters away; the logic is 0x4c bytes */
typedef struct cNurseLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float time;			/* 0x48 of the escape (state 2) or the pick up (3) */
} cNurseLocalLogic;

void *cNurses_ctor(cNurses *self);
void cNurses_Init(cNurses *self);
void cNurses_Update(cNurses *self, float dt);

/* ---- part 4: the taunts, parking zones, ship regeneration, Rusty, the scrapyards, sentinels, Sputnik and the
 * traffic ---- */
/* the native function of the sphere tests (cWorld_SphereCallF) of the logics */
#define g_LogicSphereFunc ((&g_ChTypeSphereFunc))

/* Taunts (between NurseLogic.cpp and Parking.cpp): OnTaunt(ship, race, kind) */
/* the taunt of the pilot of the ship (only kind 8 for ships without LaunchTaunts, and that one every second time) */
void Taunts_Ship(cEntity *ship, int kind);
cParking *Parking_ctor(cParking *self);
void Parking_dtor(cParking *self);
void Parking_Update(cParking *self, float dt);
cShipRegen *ShipRegen_ctor(cShipRegen *self);
void ShipRegen_dtor(cShipRegen *self);
void ShipRegen_Update(cShipRegen *self, float dt);
/* the group of the ships of the list of that name ("Mosquitos", "MadHunters") */
cShipGroup *cShipGroup_ctor(cShipGroup *self, const char *list);
void cShipGroup_dtor(cShipGroup *self);
void cShipGroup_Init(cShipGroup *self);
void cShipGroup_Update(cShipGroup *self, float dt);
cEnemiesTraffic *cEnemiesTraffic_ctor(cEnemiesTraffic *self);
void cEnemiesTraffic_dtor(cEnemiesTraffic *self);
void cEnemiesTraffic_Init(cEnemiesTraffic *self);
void cEnemiesTraffic_Update(cEnemiesTraffic *self, float dt);

/* RustyLogic.cpp, 0x84 bytes: states 7 the walk to a junk point, 8 the work at it, 9 pissed at the platform */
typedef struct cRustyLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	cEntity *junk;			/* 0x68 the junk point */
	byte pad_6c[0x84 - 0x6c];
} cRustyLocalLogic;

/* every TimeBetweeenTaunts (outdoor maps), dt is not used */
void Taunts_Think(float dt);
/* the phone cab warning with the alarm; 1 when the player is near one */
byte Taunts_Warnings(float dt);

#endif
