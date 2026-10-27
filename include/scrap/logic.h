/*
 * Game logic (original sources: Logic/): the alarm of the world (AlarmStatus.cpp), the local logics of the
 * characters (BaseLogic.cpp, the character types after it) and of the ships of the multiplayer modes (CTFEnemies,
 * CTFFriends, DeathMatch, Domination). An entity list of the world makes the local logic of an entity that joins
 * it (cEntityListData.func) and keeps it in cEntity.lLogic. The objects at g_pWorld->taunts (cLogicControl, which
 * holds the mode objects), shipRegen and aiCtrl are only known by their methods so far.
 */
#ifndef SCRAP_LOGIC_H
#define SCRAP_LOGIC_H

#include "scrap/ai.h"
#include "scrap/collisiongrid.h"
#include "scrap/entity.h"
#include "scrap/filesys.h"
#include "scrap/pyfunction.h"

#define LOGIC_EMPTY ((char *)Game_Ptr(0x3a3228))	/* "" of the exe */

/* ---- AlarmStatus.cpp: g_pWorld->alarm (0x410 bytes at 0x1c68 of cWorld) ---- */
typedef struct cAlarm {
	float grow;			/* 0x000 change per second (Scrap.SetAlarmGrow) */
	float value;			/* 0x004 0 .. 1 (g_pWorld->alarmLevel) */
	byte on;			/* 0x008 set at 1, cleared at 0 (g_pWorld->alarmOn) */
	char chars[0x200];		/* 0x009 the type of the player character (Scrap.GetAlarmChars) */
	char lastChars[0x200];		/* 0x209 the type when the alarm went on */
	byte sameChar;			/* 0x409 the alarm is on and lastChars is the type of now (or not guaranteed) */
	byte notDefChar;		/* 0x40a the type is not UsrDefChar (or not guaranteed) */
	byte notGuaranteed;		/* 0x40b the type is not in GuaranteedChars */
	byte hasGuaranteed;		/* 0x40c GuaranteedChars is set */
	byte locked;			/* 0x40d Scrap.SetLockAlarm: no changes */
	byte tauntReady;		/* 0x40e the police taunt of the next full alarm */
	byte pad_40f;
} cAlarm;
SD_OFFSET(cAlarm, lastChars, 0x209);
SD_OFFSET(cAlarm, locked, 0x40d);
SD_SIZE(cAlarm, 0x410);

/* engine variables (Gvars) */
#define g_AlarmCallback ((cPy_Function *)Game_Ptr(0x44cb1c))	/* AlarmCallback (on) */
#define g_AlarmFallDelta GAME_DATA(float, 0x44caa4)
#define g_AlarmFallMission GAME_DATA(float, 0x44cb04)
#define g_AlarmGrowDelta GAME_DATA(float, 0x44cad4)
#define g_EasyAlarmFallFactor GAME_DATA(float, 0x44caec)
#define g_GuaranteedChars GAME_DATA(char *, 0x44cb58)		/* ",type1,type2," */
#define g_PoliceTauntRace ((cString *)Game_Ptr(0x44cabc))	/* the default at +4 */
#define g_UsrDefChar ((cString *)Game_Ptr(0x44cb40))
#define g_GenCharAttackActive GAME_DATA(int, 0x44d1dc)

/* the void * arguments are g_pWorld->alarm (Alarm_ctor and Alarm_Update in world.h, the others in scrap.h) */
void __fastcall Alarm_Reset(void *alarm);
void __thiscall Alarm_SetOn(void *alarm, byte on);
void __thiscall Alarm_SetGrowRate(void *alarm, float grow);

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
	void *vf11;
	void *vf12;
	void *vf13;
	void *vf14;
	void *vf15;
	void *vf16;
	void(__thiscall *Update)(void *self, float dt);					/* 0x44 */
	void(__thiscall *SetOriginFrom)(void *self, cEntity *e);				/* 0x48 */
	byte(__thiscall *SetState)(void *self, int state, cEntity *target);			/* 0x4c vf19 */
	void(__thiscall *SetIdle)(void *self, byte idle);					/* 0x50 */
	void(__thiscall *SetActionRadius)(void *self, float radius);				/* 0x54 */
	void(__thiscall *SetOrigin)(void *self, float x, float y, float z);			/* 0x58 */
	void(__thiscall *SetOriginAng)(void *self, float x, float y, float z);		/* 0x5c */
	void(__thiscall *RestartState)(void *self);						/* 0x60 */
	void *vf25;
	void(__thiscall *UpdateView)(void *self);			/* 0x68 vf26 the view flag (+0x381) and the team */
	void(__thiscall *ToRelax)(void *self, void *ai);			/* 0x6c vf27 the AI to its zone, relaxed */
	void(__thiscall *ToAlert)(void *self, void *ai);			/* 0x70 vf28 the same in alert */
	void *vf29_31[3];
	void(__thiscall *Work)(void *self, float dt);				/* 0x80 vf32 functionaries */
	void(__thiscall *Vf33)(void *self, byte restart);			/* 0x84 */
	void *vf34_35[2];
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
	int i48;			/* 0x48 */
	int i4c;			/* 0x4c */
	int i50;			/* 0x50 */
	char *s54;			/* 0x54 cString */
	byte b58;			/* 0x58 1 */
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

/* FunctionaryLogic.cpp, 0xb8 bytes; Berto and Betty add isDefault (0xbc) */
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
	byte isDefault;			/* 0xb8 Berto and Betty: the character is "Berto_Default" / "Betty_Default" */
	byte pad_b9[3];
} cFunctionaryLocalLogic;
SD_OFFSET(cFunctionaryLocalLogic, forced, 0x78);
SD_OFFSET(cFunctionaryLocalLogic, goToWorkSpeed, 0xa4);
SD_OFFSET(cFunctionaryLocalLogic, stopInRelax, 0xb2);
SD_OFFSET(cFunctionaryLocalLogic, prevState, 0xb4);
SD_OFFSET(cFunctionaryLocalLogic, desktop, 0x68);
SD_OFFSET(cFunctionaryLocalLogic, isDefault, 0xb8);
SD_SIZE(cFunctionaryLocalLogic, 0xbc);

/* BossLogic.cpp, 0x80 bytes */
typedef struct cBossLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	vec3_t workPos;			/* 0x68 the dummy BossWorkPlace */
	float workAng[2];		/* 0x74 */
	float time;			/* 0x7c in the walk or the work */
} cBossLocalLogic;
SD_OFFSET(cBossLocalLogic, time, 0x7c);
SD_SIZE(cBossLocalLogic, 0x80);

#define CLOCALLOGIC_VFTABLE 0x3bd340
#define CBASELOCALLOGIC_VFTABLE 0x3bd360
#define CBANKDIRECTORLOCALLOGIC_VFTABLE 0x3bcf2c
#define CBANKMASTERLOCALLOGIC_VFTABLE 0x3bcfc0
#define CBERTOLOCALLOGIC_VFTABLE 0x3bd578
#define CBETTYLOCALLOGIC_VFTABLE 0x3bd680
#define CBISHOPLOCALLOGIC_VFTABLE 0x3bd7e4
#define CBOSSLOCALLOGIC_VFTABLE 0x3bd8f8
#define CCRAZYDEALERLOCALLOGIC_VFTABLE 0x3bda0c
#define CCTFENEMIESLOCALLOGIC_VFTABLE 0x3bdb54
#define CDESKTOPLOCALLOGIC_VFTABLE 0x3bdccc
#define CFUNCTIONARYLOCALLOGIC_VFTABLE 0x3be748
#define CGATEKEEPERLOCALLOGIC_VFTABLE 0x3bec3c
#define CGEARLOCALLOGIC_VFTABLE 0x3beeec
#define CGENCHARLOCALLOGIC_VFTABLE 0x3bf5ec

/* the character lists of the world the logics search (of cLogicControl at g_pWorld->taunts) */
#define World_CharList(off) (*(cEntityListData **)((byte *)g_pWorld + (off)))
#define WORLD_SENTINELS 0x21fc
#define WORLD_GEARS 0x2200
#define WORLD_NURSES 0x2234
#define WORLD_GENCHARS1 0x2244
#define WORLD_GENCHARS2 0x2248
#define WORLD_GENCHARS3 0x224c
#define WORLD_GENCHARS4 0x2250		/* an object with the list at +0x10 */

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
#define WChar_Link(e) ((Nto1Node *)((byte *)(e) + 0x3e8))		/* the first field of cWalkCharEntity */
#define WChar_Radius(e) (*(float *)((byte *)(e) + 0x3a0))
#define WChar_ControlTurn(e) (*(float *)((byte *)(e) + 0x4f0))	/* of WalkCharControl */

/* engine variables of the logics */
#define g_DefActionRadius GAME_DATA(float, 0x44cbb8)
#define g_GearRageMaxTime GAME_DATA(float, 0x44cbd0)
#define g_LogicAwakeTime GAME_DATA(float, 0x44cb88)
#define g_NotCleanableByGear GAME_DATA(char *, 0x44cba0)
#define g_BankDirectorAgressive GAME_DATA(int, 0x44cb70)
#define g_BishopAttackOnAlarm GAME_DATA(int, 0x44cc00)
#define g_BossWalkingTime GAME_DATA(float, 0x44cc18)
#define g_BossWorkPlace GAME_DATA(char *, 0x44cc30)
#define g_BossWorkingTime GAME_DATA(float, 0x44cc48)
#define g_BossWasIdle GAME_DATA(byte, 0x44cc50)		/* the boss was idle while at work */
#define g_EnergyBarActive GAME_DATA(int, 0x44726c)
#define g_SpecialActionActive GAME_DATA(int, 0x46067c)
#define g_GenCharRelaxSpeed GAME_DATA(float, 0x44d17c)
#define g_GenCharSpecialActionTime GAME_DATA(float, 0x44d254)
#define g_GenCharMinSpecialActionTime GAME_DATA(float, 0x44d2fc)
#define g_GenCharStdFar GAME_DATA(float, 0x44d2b4)
#define g_GenCharStdFovX GAME_DATA(float, 0x44d20c)
#define g_GenCharStdFovY GAME_DATA(float, 0x44d2cc)

/* the logic of a character or NULL */
cLocalLogic *Logic_GetLocalLogic(cEntity *e);
/* puts the entity on the floor; 0 if not possible */
byte Logic_SetOnFloor(cEntity *e);
/* flash at pos with radius; the last argument is -1 from the script module */
int Logic_Flash(const vec3_t *pos, float radius, int arg);

void __thiscall Taunts_UpdateEndTime(void *taunts);
void __thiscall ShipRegen_Set(void *shipRegen, cEntity *ship, float speed, float endSpeed);
void __thiscall AICtrl_SendSentinelToWatch(void *aiCtrl, const vec3_t *pos);
void __thiscall AICtrl_GearAttack(void *aiCtrl, cEntity *target, int arg);
/* a character that saw the player goes to accuse it ("Sending %s to acuse the main character"); 1 if one went */
byte __thiscall AICtrl_SendAccuser(void *aiCtrl, byte check);
/* rva 0x14f960, for the target of the gear */
void __thiscall AICtrl_CallGears(void *aiCtrl, cEntity *e);

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
int __thiscall cBaseLocalLogic_SetState(cBaseLocalLogic *self, int state, cEntity *target);
byte __thiscall cBaseLocalLogic_VarF(cBaseLocalLogic *self, const char *name, double *d, byte set);
byte __thiscall cBaseLocalLogic_VarI(cBaseLocalLogic *self, const char *name, int *i, byte set);
byte __thiscall cBaseLocalLogic_VarS(cBaseLocalLogic *self, const char *name, char **s, byte set);
void __thiscall cBaseLocalLogic_Update(cBaseLocalLogic *self, float dt);
void __thiscall cBaseLocalLogic_SetIdle(cBaseLocalLogic *self, byte idle);
/* the debug texts of ShowAI (a character node of a list with the state names, the ships) */
void Logic_ShowChar(Nto1Node *node, const char **names);
void Logic_ShowChars(Nto1Node *node, const char **names, byte visible);
void Logic_ShowShip(cEntity *e);
void Logic_ShowShips(void);
void Logic_ShowShipList(const char *name, byte visible);
void Logic_ShowAI(byte visible);
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
int __thiscall cGenCharLocalLogic_SetState(cGenCharLocalLogic *self, int state, cEntity *target);
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

/* CTFEnemies.cpp: g_pWorld + 0x219c holds the pointer */
typedef struct cCTFEnemiesData {
	char *leader;			/* 0x00 cString, the name of the leader ship */
	cEntityListData *list;		/* 0x04 "CTFEnemies" */
	float timer;			/* 0x08 Think every 0.5 s */
	float bonus;			/* 0x0c the leader goes for the player while > 0 */
	int numAttack;			/* 0x10 the ships attacking the player */
} cCTFEnemiesData;
SD_SIZE(cCTFEnemiesData, 0x14);

typedef struct cCTFEnemies {
	cCTFEnemiesData *d;
} cCTFEnemies;

/* CTFFriends.cpp: g_pWorld + 0x2198 */
typedef struct cCTFFriendsData {
	cEntityListData *list;		/* 0x00 "CTFFriends" */
	float timer;			/* 0x04 */
	int numAttack;			/* 0x08 the ships attacking the leader */
} cCTFFriendsData;
SD_SIZE(cCTFFriendsData, 0xc);

typedef struct cCTFFriends {
	cCTFFriendsData *d;
} cCTFFriends;

#define World_CTFFriends() ((cCTFFriends *)((byte *)g_pWorld + 0x2198))
#define World_CTFEnemies() ((cCTFEnemies *)((byte *)g_pWorld + 0x219c))

/* DeathMatch.cpp */
typedef struct cDeathMatch {
	float timer;			/* 0x0 */
	cEntityListData *list;		/* 0x4 "DeathMatch" */
} cDeathMatch;
SD_SIZE(cDeathMatch, 8);

#define g_LeaderFlag GAME_DATA(char *, 0x44cc64)
#define g_PercEnemiesAttackPlayerWhenWinning GAME_DATA(float, 0x44cc94)
#define g_PlayerCTFTeamIsWinning GAME_DATA(int, 0x44cc7c)
#define g_PercFriendsAttackLeaderWhenWinning GAME_DATA(float, 0x44cd30)
#define g_CTFFlagName ((char *)Game_Ptr(0x44cca0))		/* [0x80] the flag the leader goes for */

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

#define g_DominNumZones GAME_DATA(int, 0x44cd38)
#define g_DominZones ((DominZone *)Game_Ptr(0x44cd3c))		/* [MAX_DOMIN_ZONES] */
#define g_DominBatonPos GAME_DATA(vec3_t, 0x44cda4)
#define g_DominBatonState GAME_DATA(int, 0x44cdb0)
#define g_DominFightingFreezeCadenceShoot GAME_DATA(int, 0x44cdf4)
#define g_DominFightingFreezeControlAim GAME_DATA(int, 0x44cdc4)
#define g_DominMaxRangeDist GAME_DATA(float, 0x44cd84)
#define g_DominMinRangeDist GAME_DATA(float, 0x44ce0c)
#define g_DominWithinRangeDist GAME_DATA(float, 0x44cddc)
#define g_MaxDominDistChase GAME_DATA(float, 0x44cd6c)
#define g_MinDominDistChase GAME_DATA(float, 0x44cd9c)

typedef struct cDominationLogic cDominationLogic;

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

/* cDominationLogic is 0x10 bytes, cDominationEnemiesLogic and cDominationFriendsLogic 0x14 */
struct cDominationLogic {
	const cDominationLogicVtbl *vftable;	/* 0x00 */
	HashTable assigned;		/* 0x04 of cNameItem: the zone (value) of a ship (name) */
	cDominationSide *side;		/* 0x10 */
};
SD_OFFSET(cDominationLogic, side, 0x10);

#define CDOMINATIONLOGIC_VFTABLE 0x3be14c
#define CDOMINATIONENEMIESLOGIC_VFTABLE 0x3be164
#define CDOMINATIONFRIENDSLOGIC_VFTABLE 0x3be1ec
/* the engine variable LeaderName (the leader ship of the enemies) and the death callbacks of the sides */
#define g_DominLeaderName GAME_DATA(char *, 0x44ce48)
#define g_OnDominEnemyDeath ((cPy_Function *)Game_Ptr(0x44ce24))
#define g_OnDominFriendDeath ((cPy_Function *)Game_Ptr(0x44ce60))
#define World_DominFriends() ((cDominationLogic *)((byte *)g_pWorld + 0x21a0))
#define World_DominEnemies() ((cDominationLogic *)((byte *)g_pWorld + 0x21b4))

void Domination_AddZone(cEntity *zone);
void Domination_SetZoneState(int zone, int state);
void Domination_SetBatonState(const vec3_t *pos, int state);
cEntity *__thiscall cDominationLogic_GetZone(cDominationLogic *self, cAIVehicle *ai);
cEntity *__thiscall cDominationLogic_FindEnemy(cDominationLogic *self, const vec3_t *pos, cEntity *other,
	float maxDist);
void __thiscall cDominationLogic_UpdateShip(cDominationLogic *self, cEntity *v, cEntity *target, cEntity *baton);
int __thiscall cDominationLogic_NearestZone(cDominationLogic *self, const vec3_t *pos);
void cDominationLogic_ctor(cDominationLogic *self);
cDominationLogic *__fastcall cDominationEnemiesLogic_ctor(cDominationLogic *self);
void __fastcall cDominationEnemiesLogic_dtor(cDominationLogic *self);
void __fastcall cDominationLogic_dtor(cDominationLogic *self);
/* DominationEnemies.cpp and DominationFriends.cpp */
cEntity *__thiscall cDominationEnemiesLogic_GetLeader(cDominationLogic *self);
void __fastcall cDominationSide_Think(cDominationSide *self);
void __fastcall cDominationFriendsSide_Think(cDominationFriendsSide *self);
/* the zone of the ship of that name taken from the table (a zone on the way of the side is free again) */
cEntity *__thiscall cDominationEnemiesLogic_ReleaseZone(cDominationLogic *self, const char *name);
cEntity *__thiscall cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name);
cEntity *Domination_ReleaseZone(cDominationLogic *self, const char *name, int going, int held);
void Domination_OnDeath(cAbstractFile *params, cDominationLogic *side,
	cEntity *(__thiscall *release)(cDominationLogic *, const char *), cPy_Function *cb);
/* the ship of the side that goes to zone, NULL for none */
cEntity *__thiscall cDominationFriendsLogic_FindShipOfZone(cDominationLogic *self, cEntity *zone);

/* ---- the ships of the outdoor missions (of cLogicControl) ---- */
/* the empty destructors of the mode objects and the empty natives of their lists */
void __fastcall Logic_NopDtor(void *self);
void Logic_NativeNop(cAbstractFile *params);
/* a random car of the list (rva 0x154b20), NULL for none */
cEntity *Logic_RandomCar(cEntityListData *list);

/* Enemies.cpp: g_pWorld + 0x2170, the ships of the list "Enemies" attack the player */
typedef struct cEnemies {
	cEntity *usr;			/* 0x0 the car of the player they went for */
	byte ignoreFriends;		/* 0x4 "IgnoreFriends" of the logic of a ship */
	byte attacking;			/* 0x5 since the player had a car */
	byte relax;			/* 0x6 1 */
	byte pad_7;
	float timer;			/* 0x8 Think every 0.5 s */
	cEntityListData *list;		/* 0xc "Enemies" */
} cEnemies;
SD_SIZE(cEnemies, 0x10);

#define World_Enemies() ((cEnemies *)((byte *)g_pWorld + 0x2170))
#define CENEMIESLOCALLOGIC_VFTABLE 0x3be240

cEnemies *__fastcall cEnemies_ctor(cEnemies *self);
void __thiscall cEnemies_Attack(cEnemies *self, cEntity *usr);
void __fastcall cEnemies_Think(cEnemies *self);
void __fastcall cEnemies_Relax(cEnemies *self);

/* ExtPolice.cpp: g_pWorld + 0x2158, the police cars of the outdoor maps ("OutCop") */
typedef struct cExtPolice {
	float retarget;			/* 0x00 5 s: while the alarm stays on, the cars go for the player again */
	byte alarm;			/* 0x04 the alarm at the last think */
	byte pad_5[3];
	cEntity *usr;			/* 0x08 the player at the last think */
	float timer;			/* 0x0c Think every 0.5 s */
	int raised;			/* 0x10 1 when the last think set the alarm, else -1 */
	cEntityListData *list;		/* 0x14 "OutCop" */
} cExtPolice;
SD_SIZE(cExtPolice, 0x18);

#define World_ExtPolice() ((cExtPolice *)((byte *)g_pWorld + 0x2158))
#define g_ExtPoliceChasing GAME_DATA(int, 0x44cea8)		/* the cars were sent after the player */
#define g_PoliceWaveEliminated ((cPy_Function *)Game_Ptr(0x44ce84))	/* no police car left */
#define g_TrafficFightingMaxDist GAME_DATA(float, 0x44dc40)
#define Vehicle_B383(e) (*(byte *)((byte *)(e) + 0x383))

/* the AI parameters of a police car back to the defaults or random (rva 0x15b2f0, 0x15b3c0) */
void PoliceAI_RestoreParams(cAIVehicle *ai);
void PoliceAI_RandomParams(cAIVehicle *ai);
/* a flag of the attack part of the AI (+0x194, its +0x174), rva 0x8bef0 */
byte __thiscall cAIVehicle_TargetSeen(cAIVehicle *ai);

cExtPolice *__fastcall cExtPolice_ctor(cExtPolice *self);
cEntity *ExtPolice_CallCop(cEntity *e);
void ExtPolice_CopDamage(const char *name, const char *attacker, byte died);
void __fastcall cExtPolice_Think(cExtPolice *self);
byte ExtPolice_IsChased(cEntity *e);

/* Friends.cpp: g_pWorld + 0x2180, the friend ships of the outdoor missions ("Friends") */
typedef struct cFriends {
	float timer;			/* 0x0 Think every 1.5 s */
	cEntityListData *list;		/* 0x4 "Friends" */
} cFriends;
SD_SIZE(cFriends, 8);

#define World_Friends() ((cFriends *)((byte *)g_pWorld + 0x2180))

cFriends *__fastcall cFriends_ctor(cFriends *self);
void __thiscall cFriends_Attack(cFriends *self, cEntity *usr);
void __fastcall cFriends_Think(cFriends *self);

/* GateKeeperLogic.cpp: the gatekeepers ("GateKeeper", g_pWorld + 0x2254), every sqrt(2) s their logics update */
typedef struct cGateKeepers {
	float timer;			/* 0x0 */
	cEntityListData *list;		/* 0x4 "GateKeeper" */
} cGateKeepers;
SD_SIZE(cGateKeepers, 8);

/* the logic of a gatekeeper (0x4c bytes): states 0 relaxed, 1 hidden in its shell */
typedef struct cGateKeeperLocalLogic {
	cBaseLocalLogic base;		/* 0x00 */
	float hiddenTime;		/* 0x48 GateKeeperHiddenTime when it hides */
} cGateKeeperLocalLogic;
SD_SIZE(cGateKeeperLocalLogic, 0x4c);

#define g_GateKeeperStdFar GAME_DATA(float, 0x44cfc0)
#define g_GateKeeperStdFovY GAME_DATA(float, 0x44cfd8)
#define g_GateKeeperStdFovX GAME_DATA(float, 0x44d008)
#define g_GateKeeperHiddenTime GAME_DATA(float, 0x44cff0)

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

#define World_Alarm() ((cAlarm *)g_pWorld->alarm)
#define World_AICtrlI4() (*(int *)((byte *)g_pWorld + 0x21cc))	/* of g_pWorld->aiCtrl */
/* the view of the AI of a character */
#define WChar_ViewMode(e) (*(int *)((byte *)(e) + 0x38c))
#define WChar_ViewFovY(e) (*(float *)((byte *)(e) + 0x390))
#define WChar_ViewFar(e) (*(float *)((byte *)(e) + 0x394))

/* the AI control: level to at least level (rva 0x14fb00), the gear becomes a police (rva 0x14eb50) */
void __thiscall AICtrl_RaiseLevel(void *aiCtrl, int level);
void __thiscall AICtrl_GearToPolice(void *aiCtrl, cEntity *gear);

byte __thiscall cGearLocalLogic_SetState(cGearLocalLogic *self, int state, cEntity *target);

/* GenCharGroup.cpp: g_pWorld + 0x2238, the lists of the generic characters and the maker of their logics */
typedef struct cGenCharGroup {
	int attackers;			/* 0x00 counted from 0 every update, two may attack */
	int level;			/* 0x04 the highest level the logics report in a round, -1 */
	float timer;			/* 0x08 Think every 0.13 s */
	cEntityListData *genChars;	/* 0x0c "GenChars" (WORLD_GENCHARS1) */
	cEntityListData *police;	/* 0x10 "PoliceSmall" */
	cEntityListData *bishops;	/* 0x14 "Bishop" */
	void *messengers;		/* 0x18 cMessengers (MessengerLogic.cpp, 0x14 bytes), its list at +0x10 */
} cGenCharGroup;
SD_SIZE(cGenCharGroup, 0x1c);

#define World_GenCharGroup() ((cGenCharGroup *)((byte *)g_pWorld + 0x2238))

/* MessengerLogic.cpp (imports) */
void *__thiscall cMessengers_ctor(void *self);
void __fastcall cMessengers_dtor(void *self);
void __fastcall cMessengers_Init(void *self);
void __fastcall cMessengers_Think(void *self);

/* the constructors of the logics of the other types (later sources of Logic/, imports) */
void *__thiscall cPoliceLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cMayorLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cRustyLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cSputnikJunkyardLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cMaintenanceLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cPoliceBossLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cMessengerLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cMercenaryLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cHumphreyLocalLogic_ctor(void *self, cEntity *parent);
void *__thiscall cHumanLocalLogic_ctor(void *self, cEntity *parent);
/* the logics of part 1 */
cGenCharLocalLogic *__thiscall cBankDirectorLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent);
cGenCharLocalLogic *__thiscall cBankMasterLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent);
cGenCharLocalLogic *__thiscall cDesktopLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent);
cFunctionaryLocalLogic *__thiscall cBertoLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent);
cFunctionaryLocalLogic *__thiscall cBettyLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent);
cGenCharLocalLogic *__thiscall cBishopLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent);
cGenCharLocalLogic *__thiscall cCrazyDealerLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent);
cBossLocalLogic *__thiscall cBossLocalLogic_ctor(cBossLocalLogic *self, cEntity *parent);

/* GenCharLogic.cpp */
#define g_GenCharMaxSpecialActionTime GAME_DATA(float, 0x44d2e4)
#define g_GenCharAlertSpeed GAME_DATA(float, 0x44d194)
/* the type of the player draws the attention of a generic character (rva 0x14c5a0) */
byte Logic_IsSuspectType(const char *type);
void __thiscall cGenCharLocalLogic_RestartState(cGenCharLocalLogic *self);

#endif
