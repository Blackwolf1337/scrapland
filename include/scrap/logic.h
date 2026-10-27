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
	void *vf25_39[15];
	void(__thiscall *AssignDesktop)(void *self, int arg);					/* 0xa0 vf40 */
	void(__thiscall *ReleaseDesktop)(void *self);					/* 0xa4 vf41 */
} cLocalLogicVtbl;
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
	byte pad_48[0x59 - 0x48];
	byte specialOn;			/* 0x59 the special action time runs */
	byte b5a;			/* 0x5a set by the bank logics */
	byte pad_5b;
	float specialTime;		/* 0x5c GenCharSpecialActionTime */
	float searchTime;		/* 0x60 the next target search when below 0 */
	int alarmState;			/* 0x64 Bishop: the state at an alarm (BishopAttackOnAlarm) */
} cGenCharLocalLogic;
SD_OFFSET(cGenCharLocalLogic, specialOn, 0x59);
SD_OFFSET(cGenCharLocalLogic, alarmState, 0x64);
SD_SIZE(cGenCharLocalLogic, 0x68);

/* FunctionaryLogic.cpp, 0xbc bytes (Berto, Betty) */
typedef struct cFunctionaryLocalLogic {
	cGenCharLocalLogic base;	/* 0x00 */
	Nto1Node desktop;		/* 0x68 the desktop of the type ("BertoDesktop"), data the logic */
	byte pad_78[0xb8 - 0x78];
	byte isDefault;			/* 0xb8 the character is "Berto_Default" / "Betty_Default" */
	byte pad_b9[3];
} cFunctionaryLocalLogic;
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
void __thiscall AICtrl_Accuse(void *aiCtrl, cEntity *e);

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
	void *vf01;
	void *vf02;
	cEntityListData *(__thiscall *GetList)(cDominationLogic *self);		/* 0x0c the list of the side */
	int(__thiscall *GetTeam)(cDominationLogic *self);				/* 0x10 0 enemies, 1 friends */
} cDominationLogicVtbl;

/* the data of a side (0x10 bytes) */
typedef struct cDominationSide {
	cDominationLogic *owner;	/* 0x0 */
	cEntityListData *list;		/* 0x4 */
	int i8;				/* 0x8 -1 */
	int ic;				/* 0xc */
} cDominationSide;
SD_SIZE(cDominationSide, 0x10);

/* cDominationLogic is 0x10 bytes, cDominationEnemiesLogic and cDominationFriendsLogic 0x14 */
struct cDominationLogic {
	const cDominationLogicVtbl *vftable;	/* 0x00 */
	HashTable assigned;		/* 0x04 of cNameItem: the zone (value) of a ship (name) */
	cDominationSide *side;		/* 0x10 */
};
SD_OFFSET(cDominationLogic, side, 0x10);

#define CDOMINATIONLOGIC_VFTABLE 0x3be14c
#define CDOMINATIONENEMIESLOGIC_VFTABLE 0x3be164
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
cDominationLogic *__fastcall cDominationEnemiesLogic_ctor(cDominationLogic *self);
void __fastcall cDominationEnemiesLogic_dtor(cDominationLogic *self);
void __fastcall cDominationLogic_dtor(cDominationLogic *self);

#endif
