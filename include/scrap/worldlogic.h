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
typedef struct cEntityListData cEntityListData;
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
	char *name;			/* 0x0 string object (cString_ctor/dtor, cString_Set) */
	int value;			/* 0x4 */
} cItemDesc;

/* the item list of the world (InfItem.cpp, g_pWorld->logic.items) */
typedef struct cInfItem cInfItem;
typedef struct cInfItemVtbl {
	void *vf00;
	void(__thiscall *AddLife)(cInfItem *self, cItemDesc *item);			/* 0x04 */
	/* the walk over the life items: iter is the cursor (one pointer), NULL after the last */
	cItemDesc *(__thiscall *FirstLife)(cInfItem *self, void **iter);		/* 0x08 */
	cItemDesc *(__thiscall *NextLife)(cInfItem *self, void **iter);		/* 0x0c */
	void(__thiscall *AddAmmo)(cInfItem *self, int kind, cItemDesc *item);		/* 0x10 */
	cItemDesc *(__thiscall *FirstAmmo)(cInfItem *self, int kind, void **iter);	/* 0x14 */
	cItemDesc *(__thiscall *NextAmmo)(cInfItem *self, int kind, void **iter);	/* 0x18 */
} cInfItemVtbl;
SD_OFFSET(cInfItemVtbl, AddAmmo, 0x10);

typedef struct cCTFEnemies {
	cCTFEnemiesData *d;
} cCTFEnemies;

typedef struct cCTFFriends {
	cCTFFriendsData *d;
} cCTFFriends;

/* DeathMatch.cpp */
typedef struct cDeathMatch {
	float timer;			/* 0x0 */
	cEntityListData *list;		/* 0x4 "DeathMatch" */
} cDeathMatch;
SD_SIZE(cDeathMatch, 8);

/* Domination.cpp, 0x10 bytes */
struct cDominationLogic {
	const cDominationLogicVtbl *vftable;	/* 0x00 */
	HashTable assigned;		/* 0x04 of cNameItem: the zone (value) of a ship (name) */
};
SD_SIZE(cDominationLogic, 0x10);

/* DominationEnemies.cpp, 0x14 bytes */
typedef struct cDominationEnemiesLogic {
	cDominationLogic base;		/* 0x00 */
	cDominationSide *side;		/* 0x10 */
} cDominationEnemiesLogic;
SD_OFFSET(cDominationEnemiesLogic, side, 0x10);
SD_SIZE(cDominationEnemiesLogic, 0x14);

/* DominationFriends.cpp, 0x14 bytes */
typedef struct cDominationFriendsLogic {
	cDominationLogic base;		/* 0x00 */
	cDominationFriendsSide *side;	/* 0x10 */
} cDominationFriendsLogic;
SD_OFFSET(cDominationFriendsLogic, side, 0x10);
SD_SIZE(cDominationFriendsLogic, 0x14);

/* Enemies.cpp: g_pWorld->logic.enemies, the ships of the list "Enemies" attack the player */
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

/* ExtPolice.cpp: g_pWorld->logic.extPolice, the police cars of the outdoor maps ("OutCop") */
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

/* Friends.cpp: g_pWorld->logic.friends, the friend ships of the outdoor missions ("Friends") */
typedef struct cFriends {
	float timer;			/* 0x0 Think every 1.5 s */
	cEntityListData *list;		/* 0x4 "Friends" */
} cFriends;
SD_SIZE(cFriends, 8);

/* GateKeeperLogic.cpp: the gatekeepers ("GateKeeper", g_pWorld->logic.gateKeepers), every sqrt(2) s their logics update */
typedef struct cGateKeepers {
	float timer;			/* 0x0 */
	cEntityListData *list;		/* 0x4 "GateKeeper" */
} cGateKeepers;
SD_SIZE(cGateKeepers, 8);

/* GenCharGroup.cpp: g_pWorld->logic.genChars, the lists of the generic characters and the maker of their logics */
typedef struct cGenCharGroup {
	int attackers;			/* 0x00 counted from 0 every update, two may attack */
	int level;			/* 0x04 the highest level the logics report in a round, -1 */
	float timer;			/* 0x08 Think every 0.13 s */
	cEntityListData *genChars;	/* 0x0c "GenChars" */
	cEntityListData *police;	/* 0x10 "PoliceSmall" */
	cEntityListData *bishops;	/* 0x14 "Bishop" */
	cMessengers *messengers;	/* 0x18 MessengerLogic.cpp */
} cGenCharGroup;
SD_SIZE(cGenCharGroup, 0x1c);

typedef DLIST(cInfItemNode) cInfItemList;

typedef struct cInfItem {
	const cInfItemVtbl *vftable;	/* 0x00 */
	cInfItemList life;		/* 0x04 */
	cInfItemList ammo0;		/* 0x1c kind 0 */
	cInfItemList ammo2;		/* 0x34 kind 2 */
	cInfItemList ammo1;		/* 0x4c kind 1 */
} cInfItem;
SD_SIZE(cInfItem, 0x64);

/* IntPolice.cpp: the AI control of the indoor police (g_pWorld->logic.aiCtrl): the sentinels
 * search and accuse, the gears attack; two police of the second GenChar list fuse into gears that fight */
typedef struct cAICtrl {
	int level;			/* 0x00 the highest alarm level of the round (RaiseLevel), -1 */
	int attacking;			/* 0x04 the gears in state 1 or 2 */
	byte alarm;			/* 0x08 the alarm at the last think */
	byte pad_9[3];
	cEntity *lastUsr;		/* 0x0c the player at the last think */
	float timer;			/* 0x10 Think every 0.5 s */
	byte sameChar;			/* 0x14 the alarm is on for the type of now */
	byte notDefChar;		/* 0x15 */
	byte alarmOn;			/* 0x16 */
	byte pad_17;
	cEntity *usr;			/* 0x18 */
	Nto1Node watch;			/* 0x1c to the sentinel in state 5, data the control */
	float watchTime;		/* 0x2c no sentinel is sent to watch before 0 */
	byte alerted;			/* 0x30 a sentinel is in state 6 */
	byte accusing;			/* 0x31 a sentinel accuses */
	byte chased;			/* 0x32 a gear played the voice "Chase", "Abort" follows */
	byte pad_33;
	cEntityListData *sentinels;	/* 0x34 "Sentinel" */
	cEntityListData *gears;		/* 0x38 "Gear" */
} cAICtrl;
SD_OFFSET(cAICtrl, watch, 0x1c);
SD_OFFSET(cAICtrl, gears, 0x38);
SD_SIZE(cAICtrl, 0x3c);

typedef DLIST(cParkingNode) cParkingList;

typedef struct cParking {
	float timer;			/* 0x00 the next check of one zone of each ring */
	cParkingList ships;		/* 0x04 flag 0, iter the zone checked last */
	cParkingList items;		/* 0x1c flag 1, checked only while the player is in a ship */
} cParking;
SD_OFFSET(cParking, items, 0x1c);
SD_SIZE(cParking, 0x34);

typedef struct cShipRegen {
	byte unused_00;			/* 0x00 0, not used */
	byte pad_1[3];
	cShipRegenShip *ship;		/* 0x04 made by the first ShipRegen_Set */
	float time;			/* 0x08 to the next check of the player */
	byte noLoss;			/* 0x0c no life lost since the last check */
	byte pad_d[3];
	int life;			/* 0x10 of the player at the last check */
} cShipRegen;
SD_SIZE(cShipRegen, 0x14);

typedef struct cShipGroup {
	cShipGroupData *d;
} cShipGroup;

/* Traffic.cpp: the ships of the traffic turn against the player when it hits them (cEnemiesTraffic) */
typedef struct cEnemiesTraffic {
	HashTable idle;			/* 0x00 of cNameItem: the time (float bits) a traffic ship stood */
	int bossRobots;			/* 0x0c "RobotsControlledByBoss" is "1", -1 before the first think */
	cEntity *usr;			/* 0x10 the player at the last think */
	float timer;			/* 0x14 think every 0.5 s */
	float attackTime;		/* 0x18 left of the attack of the traffic enemies */
	cEntityListData *traffic;	/* 0x1c "Traffic" */
	cEntityListData *fighting;	/* 0x20 "FightingTraffic" */
	cEntityListData *chasing;	/* 0x24 "ChasingTraffic" */
	cEntityListData *enemies;	/* 0x28 "EnemiesTraffic" */
} cEnemiesTraffic;
SD_OFFSET(cEnemiesTraffic, enemies, 0x28);
SD_SIZE(cEnemiesTraffic, 0x2c);

/* Logic.cpp: the nurses of the logic control (g_pWorld->logic.nurses) */
typedef struct cNurses {
	float timer;			/* 0x0 Think every 0.73 s */
	cEntityListData *list;		/* 0x4 "Nurse" */
} cNurses;
SD_SIZE(cNurses, 8);

typedef struct cLogicControl {
	void *vftable;			/* 0x000 */
	byte warned;			/* 0x004 the last warning think played one */
	byte pad_5[3];
	float warnTime;			/* 0x008 */
	float timer;			/* 0x00c the taunts and warnings every 0.5 s */
	float tauntTime;		/* 0x010 */
	float talkEnd;			/* 0x014 the world time the talk of RemainTalkTime ends (Taunts_UpdateEndTime) */
	cParking parking;		/* 0x018 the parking zones */
	cInfItem items;			/* 0x04c */
	cShipRegen shipRegen;		/* 0x0b0 */
	cExtPolice extPolice;		/* 0x0c4 */
	cEnemies enemies;		/* 0x0dc */
	cFriends friends;		/* 0x0ec */
	cShipGroup mosquitos;		/* 0x0f4 "Mosquitos" */
	cShipGroup madHunters;		/* 0x0f8 "MadHunters" */
	cDeathMatch deathMatch;		/* 0x0fc */
	cCTFFriends ctfFriends;		/* 0x104 */
	cCTFEnemies ctfEnemies;		/* 0x108 */
	cDominationFriendsLogic dominFriends;	/* 0x10c */
	cDominationEnemiesLogic dominEnemies;	/* 0x120 */
	cAICtrl aiCtrl;			/* 0x134 */
	cEnemiesTraffic enemiesTraffic;	/* 0x170 */
	cNurses nurses;			/* 0x19c */
	cGenCharGroup genChars;		/* 0x1a4 */
	cGateKeepers gateKeepers;	/* 0x1c0 */
} cLogicControl;
SD_OFFSET(cLogicControl, items, 0x4c);
SD_OFFSET(cLogicControl, extPolice, 0xc4);
SD_OFFSET(cLogicControl, mosquitos, 0xf4);
SD_OFFSET(cLogicControl, dominFriends, 0x10c);
SD_OFFSET(cLogicControl, dominEnemies, 0x120);
SD_OFFSET(cLogicControl, aiCtrl, 0x134);
SD_OFFSET(cLogicControl, nurses, 0x19c);
SD_OFFSET(cLogicControl, genChars, 0x1a4);
SD_OFFSET(cLogicControl, gateKeepers, 0x1c0);
SD_SIZE(cLogicControl, 0x1c8);

#endif
