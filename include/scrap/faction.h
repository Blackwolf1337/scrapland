/*
 * The factions of the freeroam mode (stage M6 of the multiplayer plan). Not in the original.
 *
 * The table (src/faction/FactionDef.c, pure: no engine, unit test src/faction/test/faction_test.c) comes from
 * data/factions/factions.def, built into the game: the six groups of the game's challenges (Police, Bishops, Bankers,
 * Bureaucracy, Press, Mercs) with their character classes, the class a member plays as, the ships, the office, the
 * ranks, the relations and the abilities with their cooldowns; and the rules of the wanted level of a player (points
 * of the crimes, the levels, the decay), of an arrest (fine, reward, reach) and of the siren and the backup.
 *
 * The game (src/game/App/Faction.c: membership, the state, the messages, crimes and wanted levels, the PvP rules,
 * the Police and the client; src/game/App/FactionDeeds.c: the abilities of the other five), headless instance:
 *   ScrapC -instance:<Level> ... -mode:freeroam [-policepvp:0|1] [-factionpvp:0|1]
 * -mode:freeroam turns the factions on (sandbox, the default, has none of this). A player joins a faction and leaves
 * it at the faction's office (the level of the office, within deskreach of its desk dummy) with the join key or the
 * use button on the desk (a use point at the dummy, on the radar); a member plays the class of its rank (the state key
 * char), flies the faction's ship of its rank with the faction's pilot, its rank rises with its deeds (arrests,
 * heals, loans, bounties, permits, photos, deliveries) and it starts at the faction's jail dummy after a death in the
 * office level. The state text of a player (include/scrap/coord.h) gets the keys faction, rank, deeds, char, wanted,
 * bounty (+ bountyends), debt, permit and job: they go with the travels and persist in the coordinator's store.
 * The abilities come from the client as requests (FACTION_MSG_REQ on the reliable channel), the instance checks them
 * (member, rank, place, cooldown) and answers with notices; the client gets its faction, rank, wanted level, keys and
 * ability state in FACTION_MSG_STATUS when they change, the effects of abilities (FACTION_ST_FX) and the headlines of
 * the players' deeds (FACTION_ST_NEWS).
 * Police: siren (NETREP_FL_SIREN: flashes and sound on every client, traffic gives way), armour (Gear body), arrest
 * of a wanted player within reach (fine, reward, jail travel), backup, the ship of the rank.
 * Bishops: heal another player (sell life: the healed pays the healer), the green ray (damage in front), revive a
 * dead player at its body (a dead player waits the downed time while a reviver is in the level), the sanctuaries.
 * Bankers: steal from the player in front, suck money from the players in front, a loan (money now, the debt paid by
 * a share of later income), bribe the police (a wanted level less; the money to the nearest officer), the vault of the
 * Bank (it opens now and then: a banker at it secures it, anyone else robs it).
 * Mercs: bounties (any player with a free key: on the last player that hurt it, else the nearest; a merc that kills
 * the target gets it), the contracts of the level, hired merc ships that follow and fight for it, the heavy loadout.
 * Bureaucracy: bullet time in a radius around the functionary (the entities in it run slower: NETREP_FL_SLOW, the
 * clients' prediction steps their own entity at its speed), bla-bla (the players around fall asleep: NETREP_FL_ASLEEP,
 * no input), permits (the faction ships of the higher ranks need one), fines.
 * Press: photos of players with a fresh crime (their wanted level rises), courier jobs to the desks of the routes.
 * The wanted level: crimes of a player (hurting or killing police or civilians, other players, theft) give points, the
 * level follows the points, a level that rises turns the player's alarm on, the points fall by themselves.
 * PvP between players: no one in a sanctuary; police and a wanted player (-policepvp:1, the default), a merc and a
 * player with a bounty, members of hostile factions (-factionpvp:1, the default) may hurt each other; members of the
 * same or friendly factions may not; else -pvp. Abilities on other players that harm follow the same rule.
 *
 * Client (-join, -play): the free actions QMsg1..QMsg6 of the Racer set (F2..F7 by default; read whatever action set
 * is on) are the keys of the player's faction (join first). Lines of text show the faction, the rank, the keys, the
 * wanted level, a bounty, a debt and a job; the notices of the instance show as messages, the headlines as a news
 * line. Every text a player sees is in the tables g_FactionTexts and g_FactionRanks of Faction.c.
 */
#ifndef SCRAP_FACTION_H
#define SCRAP_FACTION_H

#include "scrap/shared.h"

#define FACTION_MAX 8
#define FACTION_NAME 40
#define FACTION_LIST 8
#define FACTION_RANKS 8
#define FACTION_LEVELS 5			/* wanted levels 1..5 */
#define FACTION_KEYS 6				/* the client's keys QMsg1..QMsg6: the abilities of a faction in its order */
#define FACTION_ZONES 8				/* sanctuaries */
#define FACTION_ROUTES 16			/* courier targets */
#define FACTION_NONE (-1)

/* the abilities (FACTION_MSG_REQ); a faction's keys are join and its abilities in the order of the table, then the
 * common ones while keys are left */
enum {
	FA_JOIN, FA_SIREN, FA_ARMOUR, FA_ARREST, FA_BACKUP, FA_SHIP,
	FA_HEAL, FA_RAY, FA_REVIVE,					/* Bishops */
	FA_STEAL, FA_SUCK, FA_LOAN, FA_BRIBE,		/* Bankers */
	FA_BOUNTY, FA_CONTRACTS, FA_HIRE, FA_HEAVY,	/* Mercs (bounty also common) */
	FA_SLOW, FA_BLABLA, FA_PERMIT, FA_FINE,		/* Bureaucracy */
	FA_PHOTO, FA_COURIER,						/* Press */
	FA_COUNT
};
/* relations */
enum { FREL_NEUTRAL, FREL_FRIEND, FREL_HOSTILE };
/* crimes */
enum { FCRIME_HURT_POLICE, FCRIME_HURT_CIVILIAN, FCRIME_KILL_POLICE, FCRIME_KILL_CIVILIAN, FCRIME_HURT_PLAYER,
	FCRIME_KILL_PLAYER, FCRIME_THEFT, FCRIME_COUNT };

typedef struct FactionDef {
	char name[FACTION_NAME];
	char chars[FACTION_LIST][FACTION_NAME];
	int nchars;
	char play[FACTION_RANKS][FACTION_NAME];	/* the class a member of each rank plays as (the last for the ranks after) */
	int nplay;
	char pilot[FACTION_NAME];		/* the pilot of its ships ("P_Dtritus" by default) */
	char armour[FACTION_NAME];		/* the class of the armoured transform, "" none */
	int armourLife;
	char ships[FACTION_LIST][FACTION_NAME];	/* the ship of each rank (the last for the ranks after) */
	int nships;
	char office[FACTION_NAME];		/* the level of the office */
	char desk[FACTION_NAME];		/* an entity or dummy of the office level, "" the whole level */
	char jail[FACTION_NAME];		/* the dummy of the office level where an arrested player or a member that died
									   there starts, "" */
	int ranks[FACTION_RANKS];		/* the deeds from which rank k + 1 starts (ranks[0] = 0) */
	int nranks;
	float cooldown[FA_COUNT];		/* s; < 0: the faction has no such ability */
	uint8_t keys[FACTION_KEYS];		/* the abilities of the keys */
	int nkeys;
	char relName[FACTION_MAX][FACTION_NAME];
	uint8_t relKind[FACTION_MAX];
	int nrel;
} FactionDef;

typedef struct FactionZone {
	char level[FACTION_NAME];
	char dummy[FACTION_NAME];		/* "*": the whole level */
	float radius;
} FactionZone;

typedef struct FactionRoute {
	char level[FACTION_NAME];
	char dummy[FACTION_NAME];
} FactionRoute;

typedef struct FactionTable {
	FactionDef f[FACTION_MAX];
	int n;
	float wanted[FACTION_LEVELS];	/* points from which each level starts */
	float decayOn, decayOff;		/* points a second with the alarm on, off */
	float crime[FCRIME_COUNT];
	float crimeGap;
	int fine, reward;				/* per wanted level */
	float reachFoot, reachShip, deskReach;
	float sirenRadius, sirenAway, sirenTime;
	char backupShip[FACTION_NAME];
	int backupCount;
	/* common abilities (every player, after the faction's keys) */
	float commonCd[FA_COUNT];
	uint8_t common[FACTION_KEYS];
	int ncommon;
	/* Bishops */
	int healLife, healPrice;
	int rayDamage;
	float rayReach;
	float downed, reviveReach;		/* s a dead player waits while a reviver is in the level; units to its body */
	int revivePrice;
	FactionZone zones[FACTION_ZONES];
	int nzones;
	/* Bankers */
	int steal, suck;
	float stealReach, suckReach;
	int loan;
	float loanOwed, loanShare, loanCommission;	/* the debt per loaned, the share of an income that pays it, the
												   banker's part of the loan */
	int bribe;						/* per wanted level dropped */
	char vaultLevel[FACTION_NAME], vaultDummy[FACTION_NAME];	/* the bank vault event, "" none */
	float vaultEvery, vaultOpen;	/* s between two openings, s it stays open */
	int vaultMoney;
	/* Mercs */
	int bounty, bountyMax, bountyTime;	/* a placement, the most on a player, s until it ends */
	char hireShip[FACTION_NAME];
	int hireCount, hirePrice;
	float hireTime;
	float heavyLife;				/* the life factor of the ship */
	/* Bureaucracy */
	float slowRadius, slowTime, slowSpeed;
	float blablaRadius, blablaTime;
	int permitPrice, permitRank, permitFine;
	/* Press */
	float photoReach, photoAge, photoPoints;
	int photoReward;
	int courierTime, courierReward, courierBonus;
	FactionRoute routes[FACTION_ROUTES];
	int nroutes;
} FactionTable;

/* the text of a table into t (defaults first); the number of factions, -1 with the line and the reason in err */
int FactionTable_Parse(FactionTable *t, const char *text, char *err, int cap);
/* the faction of the name, of a member class (chars), of a ship model (case does not matter); FACTION_NONE */
int Faction_Find(const FactionTable *t, const char *name);
int Faction_OfClass(const FactionTable *t, const char *cls);
int Faction_OfShip(const FactionTable *t, const char *model);
/* the relation of faction a to faction b (FREL_NEUTRAL for none or the same) */
int Faction_Relation(const FactionTable *t, int a, int b);
/* the rank (1..nranks) of so many deeds */
int Faction_Rank(const FactionDef *f, int deeds);
/* the class a member of the rank plays as, the ship of the rank */
const char *Faction_PlayClass(const FactionDef *f, int rank);
const char *Faction_RankShip(const FactionDef *f, int rank);
/* the ability of key k (0..FACTION_KEYS-1) of a member of faction f (FACTION_NONE: no member), -1 none */
int Faction_KeyAbility(const FactionTable *t, int f, int k);
/* the cooldown of ability a for faction f (its own, else the common one), < 0: not for it */
float Faction_Cooldown(const FactionTable *t, int f, int a);
/* the ability's name (join, siren, ...), the crime's name; NULL past the end; the ability of a name, -1 */
const char *Faction_AbilityName(int a);
const char *Faction_CrimeName(int c);
int Faction_AbilityOf(const char *name);

/* who may hurt whom between two players of the freeroam mode */
typedef struct FactionSide {
	int faction;
	int wanted;						/* the wanted level */
	int bounty;						/* money on the player */
	byte sanctuary;					/* in a sanctuary */
} FactionSide;
/* 1 a may hurt b, 0 it may not, -1 the server's -pvp decides: no one in a sanctuary; police against a wanted player
 * and both ways (policePvp); a merc against a player with a bounty and both ways; not the same faction or friends;
 * hostile factions (factionPvp) */
int Faction_PvPRule(const FactionTable *t, const FactionSide *a, const FactionSide *b, int policePvp, int factionPvp);

/* a loan: the part of an income (> 0) that pays the debt, the debt less; 0 without debt */
int FactionLoan_Repay(const FactionTable *t, int *debt, int income);
/* a bounty: amount more on the player whose bounty ends at *ends (s of the Unix epoch), up to bountyMax, the end
 * bountyTime from now; the amount taken (0 when it is at the most) */
int FactionBounty_Add(const FactionTable *t, int *bounty, int64_t *ends, int amount, int64_t now);
/* the bounty that is left at now (0 and the end cleared when it ended) */
int FactionBounty_Live(int *bounty, int64_t *ends, int64_t now);
/* a courier target for a job given in level from, seed picks among the others; -1 none */
int Faction_Route(const FactionTable *t, const char *from, uint32_t seed);
/* the reward of a delivery that took secs of the job's time */
int Faction_CourierReward(const FactionTable *t, int secs);
/* a member of rank needs a permit for its faction's ship */
byte Faction_NeedsPermit(const FactionTable *t, int rank);

/* the wanted level of a player */
typedef struct FactionWanted {
	float points;
	double last[FCRIME_COUNT];		/* the time of the last crime counted of each kind, < 0 none */
} FactionWanted;

void FactionWanted_Init(FactionWanted *w);
/* a crime at the time now (s): 1 when it counted (not within crimeGap of the same one) */
int FactionWanted_Crime(const FactionTable *t, FactionWanted *w, int crime, double now);
/* the points fall for dt seconds */
void FactionWanted_Decay(const FactionTable *t, FactionWanted *w, float dt, int alarmOn);
/* the level 0..FACTION_LEVELS of the points */
int FactionWanted_Level(const FactionTable *t, const FactionWanted *w);
/* the points of the start of a level (0 for level 0) */
float FactionWanted_Points(const FactionTable *t, int level);

/* the built-in table (data/factions/factions.def) */
extern const char g_FactionTableText[];

/* ---- the game (src/game/App/Faction.c) ---- */

#define FACTION_MSG_REQ 'F'			/* client -> instance: u8 ability */
#define FACTION_MSG_STATUS 'G'		/* instance -> client: u8 kind (FACTION_ST_), then its fields */
enum { FACTION_ST_STATE, FACTION_ST_NOTICE, FACTION_ST_FX, FACTION_ST_NEWS };
/* the effects of FACTION_ST_FX (u8 kind, u16 slot + 1 of the one that acts, of the other; 0 none) */
enum { FFX_HEAL, FFX_RAY, FFX_REVIVE, FFX_STEAL, FFX_SUCK, FFX_MONEY, FFX_PHOTO, FFX_SLOW, FFX_COUNT };

struct cEntity;
struct Net2Host;
struct NetRepInput;

/* the table (loaded once, both sides); NULL when the built-in text is bad */
const FactionTable *Faction_Table(void);

/* instance: -mode, -policepvp, -factionpvp; 1 when param is one of them */
byte Faction_ParseParam(const char *param, const char *ext);
/* -mode:freeroam */
byte Faction_Freeroam(void);
/* the mode's name for the server info */
const char *Faction_ModeName(void);
/* the state of player i from its ticket's text (NULL: a new player); its keys into the text */
void Faction_Admit(int i, const char *state, int len);
void Faction_State(int i, char *state, int *len, int cap);
/* the class player i spawns as; the pilot of the ships it takes (its faction's, else P_Dtritus) */
const char *Faction_CharClass(int i);
const char *Faction_Pilot(int i);
/* the player's character is new (a spawn, a transform, a ship left) or the player left */
void Faction_Spawned(int i, struct cEntity *e);
void Faction_Left(int i);
/* the player died: the armour and the siren off, the wanted level cleared (as the alarm of a death), its body's place
 * for a revive */
void Faction_Died(int i);
/* the place of a new character of player i that has no place: the jail dummy of its faction after a death in the
 * office level (pos a vec3_t); 1 when it has one */
byte Faction_SpawnPlace(int i, void *pos, float *yaw);
/* the ticks a dead player waits more than -respawn (a reviver is in the level) */
uint64_t Faction_DownedTicks(int i);
/* player i's input of a tick before it steps: none while asleep; the use button on a desk joins or leaves */
void Faction_Input(int i, struct cEntity *e, struct NetRepInput *in);
/* a client's message on the reliable channel; 1 when it was a faction request */
byte Faction_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* each tick after the frame: the crimes, the siren, the wanted levels, bullet times, sleepers, loans, jobs, hired
 * ships, the status to the clients */
void Faction_Tick(struct Net2Host *host, uint64_t tick);
/* victim was hurt (died: killed) by attacker (an OnDamage or OnDeath of the instance) */
void Faction_Crime(struct cEntity *victim, struct cEntity *attacker, byte died);
/* between two players' entities: 1 attacker may hurt victim, 0 not, -1 the server's -pvp decides (not freeroam) */
int Faction_PvP(struct cEntity *attacker, struct cEntity *victim);
/* the ship the player calls (its faction's ship after the ship ability), NULL for the others */
struct cEntity *Faction_CallShip(int i, const void *pos, float yaw);
/* cls is the class of the faction of the bound player (a member is no stranger in it: the alarm's UsrDefChar test) */
byte Faction_Uniform(const char *cls);
/* NETREP_FL_ flags of a replicated entity (the siren of a player's ship, a slowed or sleeping player) */
uint8_t Faction_Flags(struct cEntity *e);
/* a test line of the instance: "faction <player> join|leave|<ability>|status|set <faction>|wanted <points>|deeds
 * <n>|crime <crime>|setbounty <n>|setdebt <n>|setpermit <0|1>|setjob <level>|vault|hit <player> [<damage>]|dump
 * <pattern>" (abilities as the client's requests; hit: as a weapon of the player, 0 kills; vault: the bank vault opens;
 * dump: the dummies of the pattern and the entities near the player); 1 when it was one */
byte Faction_Command(const char *line);

/* client: a status message of the instance; each frame (the keys, the text lines); a proxy's or the own entity's
 * state flags; the siren of a vehicle for its flashes (-1: not a player's ship of the network, 0 off, 1 on); the class
 * of a character proxy loaded when it is new to the level; the own player asleep */
void Faction_ClientMessage(const uint8_t *data, int size);
void Faction_ClientFrame(void);
void Faction_EntityFlags(struct cEntity *e, uint8_t flags);
int Faction_SirenOf(struct cEntity *v);
void Faction_EnsureClass(const char *cls);
void Faction_ClientReset(void);
byte Faction_OwnAsleep(void);

#endif
