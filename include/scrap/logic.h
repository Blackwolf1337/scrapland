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
	void (*Free)(cLocalLogic *self);
	const char *(*GetName)(cLocalLogic *self);				/* "BaseLogic" */
	byte(*AcceptsArrow)(cLocalLogic *self);						/* the arrow (talk) of the player may run */
	byte(*VarI)(cLocalLogic *self, const char *name, int *i, byte set);
	byte(*VarF)(cLocalLogic *self, const char *name, double *d, byte set);
	byte(*VarS)(cLocalLogic *self, const char *name, char **s, byte set);
	byte(*VarV)(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set);
} cLocalLogicVtbl;

/* calls through the slots of cLocalLogicVtbl */
static inline void LocalLogic_Free(void *self) {
	VT(cLocalLogicVtbl, self)->Free(self);
}

static inline byte LocalLogic_VarV(void *self, const char *name, double *x, double *y, double *z, byte set) {
	return VT(cLocalLogicVtbl, self)->VarV(self, name, x, y, z, set);
}

static inline byte LocalLogic_VarS(void *self, const char *name, char **s, byte set) {
	return VT(cLocalLogicVtbl, self)->VarS(self, name, s, set);
}

static inline byte LocalLogic_VarF(void *self, const char *name, double *d, byte set) {
	return VT(cLocalLogicVtbl, self)->VarF(self, name, d, set);
}

static inline byte LocalLogic_VarI(void *self, const char *name, int *i, byte set) {
	return VT(cLocalLogicVtbl, self)->VarI(self, name, i, set);
}

static inline const char *LocalLogic_GetName(void *self) {
	return VT(cLocalLogicVtbl, self)->GetName(self);
}

static inline byte LocalLogic_AcceptsArrow(void *self) {
	return VT(cLocalLogicVtbl, self)->AcceptsArrow(self);
}

/* the slots of cBaseLocalLogic, the logic of a character */
typedef struct cBaseLocalLogicVtbl {
	cLocalLogicVtbl base;
	byte(*UpdateBusy)(cLocalLogic *self);
	void(*Refresh)(cLocalLogic *self);
	byte(*Listens)(cLocalLogic *self, cEntity *talker);				/* the mayor */
	void(*OnHit)(cLocalLogic *self, cEntity *attacker);
	void(*OnSeen)(cLocalLogic *self, cEntity *e);
	void(*OnAttack)(cLocalLogic *self, cEntity *attacker, float value);
	void(*OnShot)(cLocalLogic *self, cEntity *attacker);
	void(*OnHammer)(cLocalLogic *self, cEntity *nurse);
	byte(*GivesMoney)(cLocalLogic *self, cEntity *police);
	void(*OnAlarm)(cLocalLogic *self, cEntity *e);
	void(*Update)(cLocalLogic *self, float dt);
	void(*SetOriginFrom)(cLocalLogic *self, cEntity *e);
	byte(*SetState)(cLocalLogic *self, int state, cEntity *target);
	void(*SetIdle)(cLocalLogic *self, byte idle);
	void(*SetActionRadius)(cLocalLogic *self, float radius);
	void(*SetOrigin)(cLocalLogic *self, float x, float y, float z);
	void(*SetOriginAng)(cLocalLogic *self, float x, float y, float z);
	void(*RestartState)(cLocalLogic *self);						/* the return value of some is not used */
} cBaseLocalLogicVtbl;

/* calls through the slots of cBaseLocalLogicVtbl */
static inline byte Logic_GivesMoney(void *self, cEntity *police) {
	return VT(cBaseLocalLogicVtbl, self)->GivesMoney(self, police);
}

static inline byte Logic_Listens(void *self, cEntity *talker) {
	return VT(cBaseLocalLogicVtbl, self)->Listens(self, talker);
}

static inline void Logic_OnAlarm(void *self, cEntity *e) {
	VT(cBaseLocalLogicVtbl, self)->OnAlarm(self, e);
}

static inline void Logic_OnAttack(void *self, cEntity *attacker, float value) {
	VT(cBaseLocalLogicVtbl, self)->OnAttack(self, attacker, value);
}

static inline void Logic_OnHammer(void *self, cEntity *nurse) {
	VT(cBaseLocalLogicVtbl, self)->OnHammer(self, nurse);
}

static inline void Logic_OnHit(void *self, cEntity *attacker) {
	VT(cBaseLocalLogicVtbl, self)->OnHit(self, attacker);
}

static inline void Logic_OnSeen(void *self, cEntity *e) {
	VT(cBaseLocalLogicVtbl, self)->OnSeen(self, e);
}

static inline void Logic_OnShot(void *self, cEntity *attacker) {
	VT(cBaseLocalLogicVtbl, self)->OnShot(self, attacker);
}

static inline void Logic_Refresh(void *self) {
	VT(cBaseLocalLogicVtbl, self)->Refresh(self);
}

static inline void Logic_RestartState(void *self) {
	VT(cBaseLocalLogicVtbl, self)->RestartState(self);
}

static inline void Logic_SetActionRadius(void *self, float radius) {
	VT(cBaseLocalLogicVtbl, self)->SetActionRadius(self, radius);
}

static inline void Logic_SetIdle(void *self, byte idle) {
	VT(cBaseLocalLogicVtbl, self)->SetIdle(self, idle);
}

static inline void Logic_SetOrigin(void *self, float x, float y, float z) {
	VT(cBaseLocalLogicVtbl, self)->SetOrigin(self, x, y, z);
}

static inline void Logic_SetOriginAng(void *self, float x, float y, float z) {
	VT(cBaseLocalLogicVtbl, self)->SetOriginAng(self, x, y, z);
}

static inline void Logic_SetOriginFrom(void *self, cEntity *e) {
	VT(cBaseLocalLogicVtbl, self)->SetOriginFrom(self, e);
}

static inline byte Logic_SetState(void *self, int state, cEntity *target) {
	return VT(cBaseLocalLogicVtbl, self)->SetState(self, state, target);
}

static inline void Logic_Update(void *self, float dt) {
	VT(cBaseLocalLogicVtbl, self)->Update(self, dt);
}

static inline byte Logic_UpdateBusy(void *self) {
	return VT(cBaseLocalLogicVtbl, self)->UpdateBusy(self);
}

/* the slots of the logics of the generic characters (cGenCharLocalLogic and its children) */
typedef struct cGenCharLocalLogicVtbl {
	cBaseLocalLogicVtbl base;
	byte(*CanAttack)(cLocalLogic *self, cEntity *e);		/* the special attack against e */
	void(*UpdateView)(cLocalLogic *self);			/* the view flag (+0x381) and the team */
	void(*ToRelax)(cLocalLogic *self, struct cAIChar *ai);			/* the AI to its zone, relaxed */
	void(*ToAlert)(cLocalLogic *self, struct cAIChar *ai);			/* the same in alert */
	void(*Accuse)(cLocalLogic *self, cEntity *e);			/* state 2 against e */
	cEntity *(*FindTarget)(cLocalLogic *self);			/* of the special attack */
	byte(*TargetDone)(cLocalLogic *self);				/* the special attack ends */
	void(*Work)(cLocalLogic *self, float dt);				/* functionaries */
	void(*BreakWork)(cLocalLogic *self, byte restart);			/* busy or moved: stop the work */
	void(*GoTo)(cLocalLogic *self, struct cAIChar *ai, cEntity *e);		/* the AI walks to e */
	void(*UpdateGetIt)(cLocalLogic *self, struct cAIChar *ai, float dt);	/* state 10 */
} cGenCharLocalLogicVtbl;

/* calls through the slots of cGenCharLocalLogicVtbl */
static inline void GenChar_BreakWork(void *self, byte restart) {
	VT(cGenCharLocalLogicVtbl, self)->BreakWork(self, restart);
}

static inline void GenChar_UpdateGetIt(void *self, struct cAIChar *ai, float dt) {
	VT(cGenCharLocalLogicVtbl, self)->UpdateGetIt(self, ai, dt);
}

static inline void GenChar_ToRelax(void *self, struct cAIChar *ai) {
	VT(cGenCharLocalLogicVtbl, self)->ToRelax(self, ai);
}

static inline void GenChar_ToAlert(void *self, struct cAIChar *ai) {
	VT(cGenCharLocalLogicVtbl, self)->ToAlert(self, ai);
}

static inline byte GenChar_TargetDone(void *self) {
	return VT(cGenCharLocalLogicVtbl, self)->TargetDone(self);
}

static inline void GenChar_GoTo(void *self, struct cAIChar *ai, cEntity *e) {
	VT(cGenCharLocalLogicVtbl, self)->GoTo(self, ai, e);
}

static inline cEntity *GenChar_FindTarget(void *self) {
	return VT(cGenCharLocalLogicVtbl, self)->FindTarget(self);
}

static inline byte GenChar_CanAttack(void *self, cEntity *e) {
	return VT(cGenCharLocalLogicVtbl, self)->CanAttack(self, e);
}

static inline void GenChar_Accuse(void *self, cEntity *e) {
	VT(cGenCharLocalLogicVtbl, self)->Accuse(self, e);
}

static inline void GenChar_UpdateView(void *self) {
	VT(cGenCharLocalLogicVtbl, self)->UpdateView(self);
}

static inline void GenChar_Work(void *self, float dt) {
	VT(cGenCharLocalLogicVtbl, self)->Work(self, dt);
}

/* the functionaries and the desk characters (cFunctionaryLocalLogic, Berto, Betty, Humphrey) */
typedef struct cFunctionaryLocalLogicVtbl {
	cGenCharLocalLogicVtbl base;
	byte(*CanTalk)(cLocalLogic *self);
	void(*StopWork)(cLocalLogic *self, byte restart);
	vec3_t *(*GetWorkAng)(cLocalLogic *self, vec3_t *out);
	byte(*FastEscape)(cLocalLogic *self);
	void(*AssignDesktop)(cLocalLogic *self, float maxDist);
	void(*ReleaseDesktop)(cLocalLogic *self);
} cFunctionaryLocalLogicVtbl;

/* calls through the slots of cFunctionaryLocalLogicVtbl */
static inline void Functionary_StopWork(void *self, byte restart) {
	VT(cFunctionaryLocalLogicVtbl, self)->StopWork(self, restart);
}

static inline void Functionary_ReleaseDesktop(void *self) {
	VT(cFunctionaryLocalLogicVtbl, self)->ReleaseDesktop(self);
}

static inline vec3_t *Functionary_GetWorkAng(void *self, vec3_t *out) {
	return VT(cFunctionaryLocalLogicVtbl, self)->GetWorkAng(self, out);
}

static inline byte Functionary_FastEscape(void *self) {
	return VT(cFunctionaryLocalLogicVtbl, self)->FastEscape(self);
}

static inline byte Functionary_CanTalk(void *self) {
	return VT(cFunctionaryLocalLogicVtbl, self)->CanTalk(self);
}

static inline void Functionary_AssignDesktop(void *self, float maxDist) {
	VT(cFunctionaryLocalLogicVtbl, self)->AssignDesktop(self, maxDist);
}

/* Logic/LocalLogic.h, 8 bytes: the logic of the CTFEnemies ships is this only */
struct cLocalLogic {
	const cLocalLogicVtbl *vftable;
	cEntity *parent;
};

/* BaseLogic.cpp: the logic of a character ("WalkChar") */
typedef struct cBaseLocalLogic {
	union {
		cLocalLogic logic;
		struct {
			const cBaseLocalLogicVtbl *vftable;
			cEntity *parent;	/* the character */
		};
	};
	Nto1Node target;		/* the target of the state, data the logic */
	int state;			/* "State" */
	float rageTime;			/* while the character has a link (Gear rage, GearRageMaxTime) */
	byte dazed;			/* the action Dazed runs */
	byte pad_21[3];
	float dazedTime;		/* LogicAwakeTime */
	byte idle;			/* "Idle" */
	byte busy;			/* "Lock": Crazy, Flashed or Dazed runs */
	byte alwaysLock;		/* "AlwaysLock" */
	byte pad_2b;
	float actionRadius;		/* "ActionRadius" */
	vec3_t origin;			/* "Origin" */
	vec3_t originAng;		/* "OriginAng" */
} cBaseLocalLogic;

/* GenCharLogic.cpp */
typedef struct cGenCharLocalLogic {
	cBaseLocalLogic base;
	float time;			/* in the state: GenCharAcuseTime left of state 2, the time of state 3 */
	float accuseAgain;		/* GenCharAcuseAgainTime after a hit, no new accuse before 0 */
	int defaultState;		/* "DefaultState" (GENCHAR_STAY, RELAX, CHASE or GET_IT) */
	char *defaultTarget;		/* cString "DefaultTarget" */
	byte canAttack;			/* 1: the special attack is allowed (CanAttack) */
	byte specialOn;			/* the special action time runs */
	byte ignoreAlarm;		/* "IgnoreAlarm", set by the bank logics */
	byte pad_5b;
	float specialTime;		/* GenCharSpecialActionTime */
	float searchTime;		/* the next target search when below 0 */
	int alarmState;			/* Bishop: the state at an alarm (BishopAttackOnAlarm) */
} cGenCharLocalLogic;

/* FunctionaryLogic.cpp */
typedef struct cFunctionaryLocalLogic {
	cGenCharLocalLogic base;
	Nto1Node desktop;		/* the desktop of the type ("BertoDesktop"), data the logic */
	cEntity *forced;		/* "ForcedDesktopAssigned", before the linked one */
	float sitTime;			/* "TimeToSitDown" */
	float workTime;			/* "TimeToStopWorking" */
	float relaxTime;		/* half of the work time: the relax at the desktop */
	float playTime;			/* three times the work time, the time when not working */
	float transTime;		/* of the special action states */
	float specialTime;		/* of the special action */
	float probWorking;		/* "ProbTalkWhenWorking" */
	float probPlaying;		/* "ProbTalkWhenPlaying" */
	float probWalking;		/* "ProbTalkWhenWalking" */
	float escapeRadiusSqr;		/* "EscapeRadiusSqr" */
	float goToWorkSpeed;		/* "GoToWorkSpeed" */
	byte relaxed;			/* the relax at the desktop played */
	byte working;			/* always 1 */
	byte keepWorking;		/* "KeepWorking" */
	byte special;			/* the special action runs */
	byte fast;			/* the fast part of the special action */
	byte fastEscape;		/* "FastEscape" */
	byte escapeInRadius;		/* "EscapeWithinRadius" */
	byte forceSpecial;		/* "ForceSpecialBehaviour" */
	byte forceCancel;		/* "ForceCancelSpecialBehaviour" */
	byte ignoreBanker;		/* "IgnoreBankerAttack" */
	byte stopInRelax;		/* "StopInRelax" */
	byte pad_b3;
	int prevState;			/* the state before the special action */
} cFunctionaryLocalLogic;

/* BertoLogic.cpp, BettyLogic.cpp and HumphreyLogic.cpp, 0xbc bytes each: a functionary that takes its desktop at the
 * first update when it is the default character ("Berto_Default", "Betty_Default", "Humphrey_Default") */
typedef struct cBertoLocalLogic {
	cFunctionaryLocalLogic base;
	byte isDefault;
	byte pad_b9[3];
} cBertoLocalLogic;

typedef struct cBettyLocalLogic {
	cFunctionaryLocalLogic base;
	byte isDefault;
	byte pad_b9[3];
} cBettyLocalLogic;

typedef struct cHumphreyLocalLogic {
	cFunctionaryLocalLogic base;
	byte isDefault;
	byte pad_b9[3];
} cHumphreyLocalLogic;

/* BossLogic.cpp */
typedef struct cBossLocalLogic {
	cGenCharLocalLogic base;
	vec3_t workPos;			/* the dummy BossWorkPlace */
	float workAng[2];
	float time;			/* in the walk or the work */
} cBossLocalLogic;

/* the GenChar logics that add no members (in GenCharGroup_MakeLogic) */
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
	cActCtrl *ctrl = WithLife_GetActCtrl(e);
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
byte cLocalLogic_AcceptsArrowTrue(cLocalLogic *self);
void cLocalLogic_BreakWorkNop(cLocalLogic *self, byte restart);
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
byte cFunctionaryLocalLogic_AcceptsArrow(cLocalLogic *elem);
byte cFunctionaryLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set);
byte cFunctionaryLocalLogic_VarF(cLocalLogic *elem, const char *name, double *d, byte set);
byte cFunctionaryLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set);
void cFunctionaryLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount);
byte cFunctionaryLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target);
void cFunctionaryLocalLogic_SetIdle(cLocalLogic *elem, byte idle);
void cFunctionaryLocalLogic_UpdateView(cLocalLogic *elem);
void cFunctionaryLocalLogic_Work(cLocalLogic *elem, float dt);
void cFunctionaryLocalLogic_BreakWork(cLocalLogic *elem, byte restart);
void cFunctionaryLocalLogic_StopWork(cLocalLogic *base, byte restart);
/* GenCharLogic.cpp (imports): the variables (a robbery seen) */
byte cGenCharLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set);
byte cGenCharLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set);
void cGenCharLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount);

/* ---- the multiplayer modes (of cLogicControl) ---- */
static inline cEntity *Logic_AttackTarget(cAIVehicle *ai) {
	return (cEntity *)AIMove_GetTarget(ai)->target;
}

/* the vehicle AI state constants the modes use */
#define AIV_STATE_ATTACK 2
#define AIV_STATE_FLAG 6
#define AIV_STATE_CHASE 7

/* CTFEnemies.cpp: g_pWorld->logic.ctfEnemies holds the pointer */
typedef struct cCTFEnemiesData {
	char *leader;			/* cString, the name of the leader ship */
	cEntityListData *list;		/* "CTFEnemies" */
	float timer;			/* Think every 0.5 s */
	float bonus;			/* the leader goes for the player while > 0 */
	int numAttack;			/* the ships attacking the player */
} cCTFEnemiesData;

/* CTFFriends.cpp: g_pWorld->logic.ctfFriends holds the pointer */
typedef struct cCTFFriendsData {
	cEntityListData *list;		/* "CTFFriends" */
	float timer;
	int numAttack;			/* the ships attacking the leader */
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
	cEntity *zone;
	int state;			/* 0 free, 1 and 2 by a side, 3 and 4 a ship of a side on its way */
} DominZone;

extern int g_DominNumZones;
extern DominZone g_DominZones[4];		/* [MAX_DOMIN_ZONES] */
extern vec3_t g_DominBatonPos;
extern int g_DominBatonState;

typedef struct cDominationLogicVtbl {
	void (*Free)(cDominationLogic *self);
	void(*Init)(cDominationLogic *self);				/* makes the list */
	void(*Update)(cDominationLogic *self, float dt);
	cEntityListData *(*GetList)(cDominationLogic *self);		/* the list of the side */
	int(*GetTeam)(cDominationLogic *self);				/* 0 enemies, 1 friends */
} cDominationLogicVtbl;

/* calls through the slots of cDominationLogicVtbl */
static inline cEntityListData *Domination_GetList(void *self) {
	return VT(cDominationLogicVtbl, self)->GetList(self);
}

static inline int Domination_GetTeam(void *self) {
	return VT(cDominationLogicVtbl, self)->GetTeam(self);
}

static inline void Domination_Init(void *self) {
	VT(cDominationLogicVtbl, self)->Init(self);
}

static inline void Domination_Update(void *self, float dt) {
	VT(cDominationLogicVtbl, self)->Update(self, dt);
}

/* the data of a side: the enemies keep the zone of their leader, the friends (0xc) only the timer */
typedef struct cDominationSide {
	cDominationLogic *owner;
	cEntityListData *list;		/* "DominationEnemies" */
	int zone;			/* the zone the leader goes to, -1 none */
	float timer;			/* Think every 0.5 s */
} cDominationSide;

typedef struct cDominationFriendsSide {
	cDominationLogic *owner;
	cEntityListData *list;		/* "DominationFriends" */
	float timer;
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

/* the logic of a gatekeeper: states 0 relaxed, 1 hidden in its shell */
typedef struct cGateKeeperLocalLogic {
	cBaseLocalLogic base;
	float hiddenTime;		/* GateKeeperHiddenTime when it hides */
} cGateKeeperLocalLogic;

/* GearLogic.cpp: the gears */
enum {
	GEAR_RELAX,		/* patrols around its origin */
	GEAR_CHASE,		/* called */
	GEAR_ATTACK,
	GEAR_STAY_IN_PLACE,	/* goes back to its origin */
	GEAR_ESCORT
};

typedef struct cGearLocalLogic {
	cBaseLocalLogic base;
	float sirenTime;		/* with the alarm: the next siren */
	float lostTime;			/* since the target was lost */
	byte called;			/* sent by the AI control */
	byte pad_51[3];
	int defaultState;		/* "DefaultState" */
	char *defaultTarget;		/* cString "DefaultTarget" */
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
/* the maker of the logics of the GenChar lists (cEntityListData.func) */

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

/* the states the little police adds to the GENCHAR_ ones */
enum {
	LPOLICE_DISCOVER = 7,	/* turns to the target and accuses it */
	LPOLICE_CHASE = 8	/* runs after the target */
};

/* the states of the sentinel */
enum {
	SENTINEL_RELAX, SENTINEL_SEARCH, SENTINEL_ACCUSE, SENTINEL_INSPECT, SENTINEL_SUSPECT, SENTINEL_WATCH,
	SENTINEL_ACCUSE_NPC	/* runs to a character that robbed a banker */
};

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
/* SetState(2, e): the of the GenChars and the of the nurses */
void Logic_SetState2(cLocalLogic *base, cEntity *e);

/* HumanLogic.cpp: the characters near the human flee from it */
typedef struct cHumanLocalLogic {
	cBaseLocalLogic base;
	cPy_Function func;		/* HumanLogic_Escape */
} cHumanLocalLogic;

/* InfItem.cpp: the item list of the world (cInfItem of worldlogic.h, g_pWorld->logic.items, 100 bytes), DLists of
 * cItemDesc nodes: the life items and the ammo items of three kinds */
typedef struct cInfItemNode {
	struct cInfItemNode *next;
	struct cInfItemNode *prev;
	cItemDesc item;
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
/* the logic of a sentinel */
typedef struct cSentinelLocalLogic {
	cBaseLocalLogic base;
	float time;			/* of the state (the follow, the watch) */
	byte seen;			/* the player was in view at the last think */
	byte pad_4d[3];
} cSentinelLocalLogic;

void *cSentinelLocalLogic_ctor(void *self, cEntity *parent);
/* to the place pos to watch it */
void cSentinelLocalLogic_Watch(void *self, const vec3_t *pos);
/* back to state 0 (1 with the alarm), unless idle */
void cSentinelLocalLogic_Return(void *self);
/* the voice event of the character of that name (Sound/) */
byte Sound_PlayVoice(const char *name, const char *event, const char *arg, int force);
/* GearLogic.cpp */
cGearLocalLogic *cGearLocalLogic_ctor(cGearLocalLogic *self, cEntity *parent);
byte cGearLocalLogic_Call(cGearLocalLogic *self, cEntity *e);

/* LittlePoliceLogic.cpp: the police of the indoor maps (the type "Police") */
typedef struct cLittlePoliceLocalLogic {
	cGenCharLocalLogic base;	/* specialTime: the time of the chase in state 4 */
	float sirenTime;		/* with the alarm: the next siren */
	float lostTime;			/* of the player, 5 when the AI walks to it */
	byte bribe;			/* the arrow shows the options of the bribe */
	byte pad_71[3];
	int policeResult;		/* "PoliceResult": 1 when it goes to the main mission */
} cLittlePoliceLocalLogic;

extern cGVar GVar_PoliceStealAmount;
#define g_PoliceStealAmount (GVar_PoliceStealAmount.value.i)

/* Parking.cpp: the parking zones of Scrap.AddParkingZone (flag 0: of the ships, 1: of the items), a ring each */
typedef struct cParkingZone {
	vec3_t pos;
	cString name;			/* "_Junkyard" is not found in ring 0 */
} cParkingZone;

typedef struct cParkingNode {
	struct cParkingNode *next;
	struct cParkingNode *prev;
	cParkingZone zone;
} cParkingNode;

/* ShipRegen.cpp: the life regeneration of the player, and the ship of SLogic.SetShipToRegenerate */
typedef struct cShipRegenShip {
	char name[0x80];
	float time;			/* to the next point */
	float rate;			/* 1 / speed: the time per point at no life */
	float endRate;			/* 1 / endSpeed: at full life */
	int life;			/* at the last update */
} cShipRegenShip;

/* Scrapyard.cpp: a group of ships that guard their lairs ("Mosquitos", "MadHunters"), behind a pointer */
typedef struct cShipGroupData {
	cPy_Function onSphere;		/* native: the ships in the sphere of the intruder */
	HashTable lairs;		/* of cWorldStrItem: the lair (vec3) of a ship (name) */
	const char *listName;
	cEntityListData *list;
	float timer;			/* think every 0.5 s */
} cShipGroupData;

extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)
/* the callback the world puts a new entity at its origin with (set by Logic_ctor) */
extern void *g_SetOriginFunc;

/* MaintenanceLogic.cpp, 0xdc bytes: the maintenance robot of the ship editor walks around the ship on the
 * platform (states 7: wait for the ship, 8: work) */
typedef struct cMaintenanceLocalLogic {
	cGenCharLocalLogic base;
	vec3_t points[8];		/* the corners of the box around the ship */
	vec3_t reposition;		/* "RepositionPoint" */
	int point;			/* the corner it walks to, -1 */
	float timer;			/* MaintenanceTimeToNextPos */
} cMaintenanceLocalLogic;

/* MayorLogic.cpp */
typedef struct cMayorLocalLogic {
	cGenCharLocalLogic base;
	char listener[0x80];		/* the character that talks to the mayor, "" none */
} cMayorLocalLogic;

extern cGVar GVar_MayorBlabliblaRadius;
#define g_MayorBlabliblaRadius (GVar_MayorBlabliblaRadius.value.f)

/* MessengerLogic.cpp: the messengers (cMessengers of cGenCharGroup) */
typedef struct cMessengers {
	cEntity *desk;			/* the desk of the data pack of the player */
	cEntity *target;		/* the desk of the messenger with the pack */
	float timer;			/* 5 s */
	int count;			/* the most desks of the type seen, 5 */
	cEntityListData *list;		/* "Messenger" */
} cMessengers;

extern cGVar GVar_MessengerTubeState;
#define g_MessengerTubeState (GVar_MessengerTubeState.value.i)
#define g_MessengerTubeStateDef (GVar_MessengerTubeState.def.i)

/* StrMoney.c (g_pWorld + 0x2078): the money of the world (cWorld.money) as digits, one changes per update */
typedef struct cStrMoney {
	char digits[16];		/* "0000000000000000", 1..15 change */
	char end;			/* 0 */
	char len;			/* the digits from the first one not "0" */
	byte pad_12[2];
	float timer;			/* StrMoneyUpdate */
	int money;			/* cWorld.money */
} cStrMoney;
/* the digits from the first one not "0", "0" for none */
const char *cStrMoney_GetStr(cStrMoney *self);

/* NurseLogic.cpp: the nurses (cLogicControl.nurses) take the dazed characters away; the logic is */
typedef struct cNurseLocalLogic {
	cBaseLocalLogic base;
	float time;			/* of the escape (state 2) or the pick up (3) */
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

/* RustyLogic.cpp: states 7 the walk to a junk point, 8 the work at it, 9 pissed at the platform */
typedef struct cRustyLocalLogic {
	cGenCharLocalLogic base;
	cEntity *junk;			/* the junk point */
} cRustyLocalLogic;

/* every TimeBetweeenTaunts (outdoor maps), dt is not used */
void Taunts_Think(float dt);
/* the phone cab warning with the alarm; 1 when the player is near one */
byte Taunts_Warnings(float dt);
byte cSentinelLocalLogic_Seen(cSentinelLocalLogic *self);
void cSentinelLocalLogic_Think(cSentinelLocalLogic *self, float dt, byte quiet);

#endif
