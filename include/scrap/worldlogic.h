/*
 * The logic control of the world (Logic/Logic.cpp, cLogicControl, embedded in cWorld at 0x2094 as logic) and the
 * mode and group objects it holds. Here and not in logic.h because cWorld (game.h) embeds them; their functions and
 * the data they point to are in include/scrap/logic.h.
 */
#ifndef SCRAP_WORLDLOGIC_H
#define SCRAP_WORLDLOGIC_H

#include "scrap/dlist.h"
#include "scrap/global.h"
#include "scrap/nto1links.h"

typedef struct cEntity cEntity;
typedef struct cEntityList cEntityListData;
typedef struct cCTFEnemiesData cCTFEnemiesData;
typedef struct cCTFFriendsData cCTFFriendsData;
typedef struct cDominationLogic cDominationLogic;
typedef struct cDominationLogicVtbl cDominationLogicVtbl;
typedef struct cDominationSide cDominationSide;
typedef struct cDominationFriendsSide cDominationFriendsSide;
typedef struct cInfItemNode cInfItemNode;
typedef struct cParkingNode cParkingNode;
typedef struct cShipRegenShip cShipRegenShip;
typedef struct cShipGroupData cShipGroupData;
typedef struct cMessengers cMessengers;

/* an item description for the item list (Scrap.AddItem): a string object and a value */
typedef struct cItemDesc {
	char *name;			/* string object (cString_ctor/dtor, cString_Set) */
	int value;
} cItemDesc;

/* the item list of the world (InfItem.cpp, g_pWorld->logic.items) */
typedef struct cInfItem cInfItem;
typedef struct cCTFEnemies {
	cCTFEnemiesData *d;
} cCTFEnemies;

typedef struct cCTFFriends {
	cCTFFriendsData *d;
} cCTFFriends;

/* DeathMatch.cpp */
typedef struct cDeathMatch {
	float timer;
	cEntityListData *list;		/* "DeathMatch" */
} cDeathMatch;

/* Domination.cpp */
struct cDominationLogic {
	const cDominationLogicVtbl *vftable;
	HashTable assigned;		/* of cNameItem: the zone (value) of a ship (name) */
};

/* DominationEnemies.cpp */
typedef struct cDominationEnemiesLogic {
	cDominationLogic base;
	cDominationSide *side;
} cDominationEnemiesLogic;

/* DominationFriends.cpp */
typedef struct cDominationFriendsLogic {
	cDominationLogic base;
	cDominationFriendsSide *side;
} cDominationFriendsLogic;

/* Enemies.cpp: g_pWorld->logic.enemies, the ships of the list "Enemies" attack the player */
typedef struct cEnemies {
	cEntity *usr;			/* the car of the player they went for */
	byte ignoreFriends;		/* "IgnoreFriends" of the logic of a ship */
	byte attacking;			/* since the player had a car */
	byte relax;			/* 1 */
	byte pad_7;
	float timer;			/* Think every 0.5 s */
	cEntityListData *list;		/* "Enemies" */
} cEnemies;

/* ExtPolice.cpp: g_pWorld->logic.extPolice, the police cars of the outdoor maps ("OutCop") */
typedef struct cExtPolice {
	float retarget;			/* 5 s: while the alarm stays on, the cars go for the player again */
	byte alarm;			/* the alarm at the last think */
	byte pad_5[3];
	cEntity *usr;			/* the player at the last think */
	float timer;			/* Think every 0.5 s */
	int raised;			/* 1 when the last think set the alarm, else -1 */
	cEntityListData *list;		/* "OutCop" */
} cExtPolice;

/* Friends.cpp: g_pWorld->logic.friends, the friend ships of the outdoor missions ("Friends") */
typedef struct cFriends {
	float timer;			/* Think every 1.5 s */
	cEntityListData *list;		/* "Friends" */
} cFriends;

/* GateKeeperLogic.cpp: the gatekeepers ("GateKeeper", g_pWorld->logic.gateKeepers), every sqrt(2) s their logics update */
typedef struct cGateKeepers {
	float timer;
	cEntityListData *list;		/* "GateKeeper" */
} cGateKeepers;

/* GenCharGroup.cpp: g_pWorld->logic.genChars, the lists of the generic characters and the maker of their logics */
typedef struct cGenCharGroup {
	int attackers;			/* counted from 0 every update, two may attack */
	int level;			/* the highest level the logics report in a round, -1 */
	float timer;			/* Think every 0.13 s */
	cEntityListData *genChars;	/* "GenChars" */
	cEntityListData *police;	/* "PoliceSmall" */
	cEntityListData *bishops;	/* "Bishop" */
	cMessengers *messengers;	/* MessengerLogic.cpp */
} cGenCharGroup;

typedef DLIST(cInfItemNode) cInfItemList;

typedef struct cInfItem {
	cInfItemList life;
	cInfItemList ammo0;		/* kind 0 */
	cInfItemList ammo2;		/* kind 2 */
	cInfItemList ammo1;		/* kind 1 */
} cInfItem;

/* IntPolice.cpp: the AI control of the indoor police (g_pWorld->logic.aiCtrl): the sentinels
 * search and accuse, the gears attack; two police of the second GenChar list fuse into gears that fight */
typedef struct cAICtrl {
	int level;			/* the highest alarm level of the round (RaiseLevel), -1 */
	int attacking;			/* the gears in state 1 or 2 */
	byte alarm;			/* the alarm at the last think */
	byte pad_9[3];
	cEntity *lastUsr;		/* the player at the last think */
	float timer;			/* Think every 0.5 s */
	byte sameChar;			/* the alarm is on for the type of now */
	byte notDefChar;
	byte alarmOn;
	byte pad_17;
	cEntity *usr;
	Nto1Node watch;			/* to the sentinel in state 5, data the control */
	float watchTime;		/* no sentinel is sent to watch before 0 */
	byte alerted;			/* a sentinel is in state 6 */
	byte accusing;			/* a sentinel accuses */
	byte chased;			/* a gear played the voice "Chase", "Abort" follows */
	byte pad_33;
	cEntityListData *sentinels;	/* "Sentinel" */
	cEntityListData *gears;		/* "Gear" */
} cAICtrl;

typedef DLIST(cParkingNode) cParkingList;

typedef struct cParking {
	float timer;			/* the next check of one zone of each ring */
	cParkingList ships;		/* flag 0, iter the zone checked last */
	cParkingList items;		/* flag 1, checked only while the player is in a ship */
} cParking;

typedef struct cShipRegen {
	byte unused_00;			/* 0, not used */
	byte pad_1[3];
	cShipRegenShip *ship;		/* made by the first ShipRegen_Set */
	float time;			/* to the next check of the player */
	byte noLoss;			/* no life lost since the last check */
	byte pad_d[3];
	int life;			/* of the player at the last check */
} cShipRegen;

typedef struct cShipGroup {
	cShipGroupData *d;
} cShipGroup;

/* Traffic.cpp: the ships of the traffic turn against the player when it hits them (cEnemiesTraffic) */
typedef struct cEnemiesTraffic {
	HashTable idle;			/* of cNameItem: the time (float bits) a traffic ship stood */
	int bossRobots;			/* "RobotsControlledByBoss" is "1", -1 before the first think */
	cEntity *usr;			/* the player at the last think */
	float timer;			/* think every 0.5 s */
	float attackTime;		/* left of the attack of the traffic enemies */
	cEntityListData *traffic;	/* "Traffic" */
	cEntityListData *fighting;	/* "FightingTraffic" */
	cEntityListData *chasing;	/* "ChasingTraffic" */
	cEntityListData *enemies;	/* "EnemiesTraffic" */
} cEnemiesTraffic;

/* Logic.cpp: the nurses of the logic control (g_pWorld->logic.nurses) */
typedef struct cNurses {
	float timer;			/* Think every 0.73 s */
	cEntityListData *list;		/* "Nurse" */
} cNurses;

typedef struct cLogicControl {
	byte warned;			/* the last warning think played one */
	byte pad_5[3];
	float warnTime;
	float timer;			/* the taunts and warnings every 0.5 s */
	float tauntTime;
	float talkEnd;			/* the world time the talk of RemainTalkTime ends (Taunts_UpdateEndTime) */
	cParking parking;		/* the parking zones */
	cInfItem items;
	cShipRegen shipRegen;
	cExtPolice extPolice;
	cEnemies enemies;
	cFriends friends;
	cShipGroup mosquitos;		/* "Mosquitos" */
	cShipGroup madHunters;		/* "MadHunters" */
	cDeathMatch deathMatch;
	cCTFFriends ctfFriends;
	cCTFEnemies ctfEnemies;
	cDominationFriendsLogic dominFriends;
	cDominationEnemiesLogic dominEnemies;
	cAICtrl aiCtrl;
	cEnemiesTraffic enemiesTraffic;
	cNurses nurses;
	cGenCharGroup genChars;
	cGateKeepers gateKeepers;
} cLogicControl;

cItemDesc *cInfItem_FirstLife(cInfItem *self, void **iter);
cItemDesc *cInfItem_FirstAmmo(cInfItem *self, int kind, void **iter);
cItemDesc *cInfItem_NextLife(cInfItem *self, void **iter);
cItemDesc *cInfItem_NextAmmo(cInfItem *self, int kind, void **iter);
void cInfItem_AddLife(cInfItem *self, cItemDesc *item);
void cInfItem_AddAmmo(cInfItem *self, int kind, cItemDesc *item);

#endif
