/*
 * The parts of the freeroam factions that Faction.c (membership, state, messages, crimes, PvP, the Police, the client)
 * and FactionDeeds.c (the abilities of the other five factions) share, see include/scrap/faction.h. Not in the
 * original.
 */
#ifndef GAME_APP_FACTIONINT_H
#define GAME_APP_FACTIONINT_H

#include "scrap/entity.h"
#include "scrap/faction.h"
#include "scrap/mathlib.h"
#include "scrap/players.h"

#define FR_NAME 64
#define FR_TEXT 160

/* every text a player sees (g_FactionTexts of Faction.c; a writing pass comes later): notices, headlines (FN_), the
 * lines of the client (FT_HUD_) */
enum {
	FT_JOINED, FT_LEFT, FT_NOT_OFFICE, FT_LEAVE_FIRST, FT_WANTED_NO_JOIN, FT_ON_FOOT, FT_MEMBERS_ONLY, FT_NOT_READY,
	FT_NEED_SHIP, FT_SIREN_ON, FT_SIREN_OFF, FT_ARMOUR_ON, FT_ARMOUR_OFF, FT_NO_SUSPECT, FT_ARRESTED, FT_ARREST_MADE,
	FT_RANK_UP, FT_BACKUP, FT_SHIP, FT_NO_SHIP, FT_WANTED, FT_NOT_FREEROAM, FT_YOU_ARE_WANTED,
	FT_NO_ONE_NEAR, FT_ONLY_SHIP, FT_ONLY_OUTDOORS, FT_NO_MONEY, FT_PROTECTED, FT_NEED_PERMIT,
	FT_HEAL_GIVE, FT_HEAL_GOT, FT_NO_WOUNDED, FT_RAY_MISS, FT_RAY_HIT, FT_NO_BODY, FT_REVIVE_GIVE, FT_REVIVE_GOT,
	FT_STEAL_GIVE, FT_STEAL_GOT, FT_SUCK_GIVE, FT_LOAN_GIVE, FT_LOAN_GOT, FT_HAS_DEBT, FT_DEBT_PAID, FT_BRIBE,
	FT_BRIBE_GOT, FT_NOT_WANTED,
	FT_BOUNTY_SET, FT_BOUNTY_ON_YOU, FT_BOUNTY_FULL, FT_BOUNTY_CLAIM, FT_CONTRACT, FT_NO_CONTRACTS, FT_HIRED,
	FT_HIRE_GONE, FT_HEAVY,
	FT_SLOW_ON, FT_SLOWED, FT_ASLEEP, FT_BLABLA, FT_PERMIT_GIVE, FT_PERMIT_GOT, FT_HAS_PERMIT, FT_FINE_GIVE,
	FT_FINE_GOT, FT_NO_OFFENDER,
	FT_PHOTO_GIVE, FT_PHOTO_GOT, FT_NO_PHOTO, FT_JOB, FT_JOB_DONE, FT_JOB_LATE, FT_JOB_AT_DESK, FT_JOB_HAVE,
	FT_VAULT_GUARD, FT_VAULT_ROB,
	FN_ARREST, FN_BOUNTY, FN_PHOTO, FN_REVIVE, FN_STEAL, FN_DELIVERY, FN_JOINED, FN_VAULT_OPEN, FN_VAULT_ROBBED,
	FT_HUD_MEMBER, FT_HUD_WANTED, FT_HUD_RANK, FT_HUD_BOUNTY, FT_HUD_DEBT, FT_HUD_JOB, FT_HUD_PERMIT, FT_HUD_NEWS,
	FT_HUD_KEYS, FT_DESK, FT_COUNT
};

/* a player of the instance */
typedef struct FrPlayer {
	int faction;					/* FACTION_NONE */
	int deeds;
	char name[FR_NAME];
	char charCls[FACTION_NAME];		/* the class it spawns as when it is no member, "" Dtritus */
	byte armoured;
	byte siren;
	byte callShip;					/* the next ship it calls is its faction's ship */
	cEntity *ship;					/* its ship of the siren (compared, read while World_FindEntity(shipName) is it) */
	char shipName[FR_NAME];
	FactionWanted wanted;
	int level;						/* the wanted level of the last tick */
	int jailFine;					/* arrested and sent to the jail of another level with this fine: its notice
									   there (state key jailfine), -1 */
	double ready[FA_COUNT];			/* the instance time the ability is ready again */
	uint8_t sent[96];				/* the last status sent */
	int sentLen;
	uint32_t requests, done;
	/* the deeds of the other factions */
	int bounty;						/* money on it */
	int64_t bountyEnds;				/* s of the Unix epoch */
	int debt;
	byte permit;
	int lastMoney;					/* the money of the last tick (a loan is paid from the income since) */
	byte moneyKnown;
	char jobLevel[FACTION_NAME], jobDummy[FACTION_NAME];	/* a courier job, "" none */
	int64_t jobStart, jobEnds;		/* s of the Unix epoch */
	double lastCrime;				/* the instance time of its last crime, < 0 none */
	int lastBy;						/* the player that hurt it last, -1 */
	uint16_t lastByJoin;
	double lastByTime;
	byte dead;						/* its body lies at deadPos (a revive brings it back there) */
	vec3_t deadPos;
	float deadYaw;
	byte home;						/* its next character starts at the jail dummy of its faction */
	byte revived;
	int fullLife;					/* the life of its character when it was new */
	byte useHeld;					/* the use button down on a desk */
	double slowUntil;				/* its bullet time runs until then */
	double asleepUntil;
	byte slowed, asleep;			/* in a bullet time, asleep: the flags of its entity */
} FrPlayer;

/* Faction.c */
FrPlayer *Fr_Player(int i);
double Fr_Now(void);
int64_t Fr_Unix(void);
int Fr_Rank(const FrPlayer *p);
/* a notice to player i, a headline to every player of the instance, an effect on every client (a and b: entities of
 * the replication, NULL none) */
void Fr_Notice(int i, int text, int num, const char *name, const char *name2);
void Fr_News(int text, int num, const char *name, const char *name2);
void Fr_Fx(int fx, cEntity *a, cEntity *b);
float Fr_DistSq(const cEntity *a, const vec3_t *b);
/* the money of player i, more by delta (less when negative) */
int Fr_Money(int i);
void Fr_AddMoney(int i, int delta);
/* the nearest other player within reach of e (in front within cone radians of its view when cone > 0) that test
 * accepts (NULL: any), -1 */
int Fr_Nearest(int i, cEntity *e, float reach, float cone, byte (*test)(int i, int j));
/* player i may act against player j (the PvP rules); its notice when it may not and tell */
byte Fr_MayAffect(int i, int j, byte tell);
/* e is at the desk of faction f's office (in the office level, within deskreach) */
byte Fr_AtDesk(int f, cEntity *e);
/* a deed of player i: its rank may rise; the store gets it */
void Fr_Deed(int i);
/* player i's character again where it stands (another class) */
void Fr_Rebody(int i, const vec3_t *pos, float yaw);
/* the position of a dummy of the level: 1 found */
byte Fr_Dummy(const char *name, vec3_t *pos, float *yaw);
/* a crime of player i */
void Fr_AddCrime(int i, int crime);
/* the character of player i is outdoors (the police cars' list) */
byte Fr_Outdoors(void);

/* FactionDeeds.c: an ability of the five factions besides the Police (-1: not one of them, else 1 done, 0 refused),
 * the tick of their running things, a kill of player j by player i (1: a bounty claimed, the kill is lawful), the
 * player left or died (its bullet time and its hired ships end), an entity in a sanctuary, the NETREP_FL_ flags of a
 * player's entity */
int FrD_Request(int i, cEntity *e, int a);
void FrD_Tick(uint64_t tick);
byte FrD_Kill(int i, int j);
void FrD_Gone(int i);
byte FrD_InSanctuary(cEntity *e);
uint8_t FrD_Flags(int i);
/* the bank vault of this level opens now (tests) */
void FrD_OpenVault(void);

#endif
