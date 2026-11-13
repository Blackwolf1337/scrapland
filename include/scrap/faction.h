/*
 * The factions of the freeroam mode (stage M6 of the multiplayer plan). Not in the original.
 *
 * The table (src/faction/FactionDef.c, pure: no engine, unit test src/faction/test/faction_test.c) comes from
 * data/factions/factions.def, built into the game: the six groups of the game's challenges (Police, Bishops, Bankers,
 * Bureaucracy, Press, Mercs) with their character classes, the class a member plays as, the ships, the office, the
 * ranks, the relations and the abilities with their cooldowns; and the rules of the wanted level of a player (points
 * of the crimes, the levels, the decay), of an arrest (fine, reward, reach) and of the siren and the backup.
 *
 * The game (src/game/App/Faction.c), headless instance:
 *   ScrapC -instance:<Level> ... -mode:freeroam [-policepvp:0|1]
 * -mode:freeroam turns the factions on (sandbox, the default, has none of this). A player joins a faction and leaves
 * it at the faction's office (the level of the office, within deskreach of its desk entity or dummy) with the join
 * ability; a member plays the faction's class (the state key char), its rank rises with its arrests. The state text of
 * a player (include/scrap/coord.h) gets the keys faction, rank, arrests, char and wanted: they go with the travels and
 * persist in the coordinator's store; the stored char is the class a player spawns as (any mode).
 * The abilities come from the client as requests (FACTION_MSG_REQ on the reliable channel), the instance checks them
 * (member, rank, place, cooldown) and answers with notices; the client gets its faction, rank, wanted level and
 * ability state in FACTION_MSG_STATUS when they change. Police: the siren of a police ship (its state flag
 * NETREP_FL_SIREN: the flashes and the sound on every client; the traffic near it moves away), the armoured cop (the
 * armour class: slow, more life, its weapons; and back), the arrest of a wanted player within reach (the fine, the
 * officer's reward, its wanted level cleared, the travel to the jail of the office level), the backup (police ships
 * outdoors, the level's gears indoors, after the nearest wanted player) and a police ship of the rank (SPoli<rank>).
 * The wanted level: crimes of a player (hurting or killing police or civilians, other players) give points, the level
 * follows the points (wanted), a level that rises turns the player's alarm on (the police of the level hunts it), the
 * points fall by themselves (decay; faster while the alarm is off). Police against a wanted player (both ways) may
 * hurt each other even with -pvp:0 when -policepvp:1 (the default); other players follow -pvp.
 *
 * Client (-join, -play): the free actions QMsg1..QMsg6 of the Racer set (F2..F7 by default; read whatever action set
 * is on): join/leave, siren, armour, arrest, backup, police ship. A line of text shows the faction, the rank and the
 * wanted level; the notices of the instance show as messages. Every text a player sees is in the table g_FactionTexts
 * of Faction.c.
 */
#ifndef SCRAP_FACTION_H
#define SCRAP_FACTION_H

#include "scrap/shared.h"

#define FACTION_MAX 8
#define FACTION_NAME 24
#define FACTION_LIST 8
#define FACTION_RANKS 8
#define FACTION_LEVELS 5			/* wanted levels 1..5 */
#define FACTION_NONE (-1)

/* the abilities (FACTION_MSG_REQ), in the order of the client's keys QMsg1..QMsg6 */
enum { FA_JOIN, FA_SIREN, FA_ARMOUR, FA_ARREST, FA_BACKUP, FA_SHIP, FA_COUNT };
/* relations */
enum { FREL_NEUTRAL, FREL_FRIEND, FREL_HOSTILE };
/* crimes */
enum { FCRIME_HURT_POLICE, FCRIME_HURT_CIVILIAN, FCRIME_KILL_POLICE, FCRIME_KILL_CIVILIAN, FCRIME_HURT_PLAYER,
	FCRIME_KILL_PLAYER, FCRIME_COUNT };

typedef struct FactionDef {
	char name[FACTION_NAME];
	char chars[FACTION_LIST][FACTION_NAME];
	int nchars;
	char play[FACTION_NAME];		/* the class a member plays as */
	char armour[FACTION_NAME];		/* the class of the armoured transform, "" none */
	int armourLife;
	char ships[FACTION_LIST][FACTION_NAME];
	int nships;
	char office[FACTION_NAME];		/* the level of the office */
	char desk[FACTION_NAME];		/* an entity or dummy of the office level, "" the whole level */
	char jail[FACTION_NAME];		/* the dummy of the office level where an arrested player starts, "" */
	int ranks[FACTION_RANKS];		/* the arrests from which rank k + 1 starts (ranks[0] = 0) */
	int nranks;
	float cooldown[FA_COUNT];		/* s; < 0: the faction has no such ability */
	char relName[FACTION_MAX][FACTION_NAME];
	uint8_t relKind[FACTION_MAX];
	int nrel;
} FactionDef;

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
} FactionTable;

/* the text of a table into t (defaults first); the number of factions, -1 with the line and the reason in err */
int FactionTable_Parse(FactionTable *t, const char *text, char *err, int cap);
/* the faction of the name, of a member class (chars), of a ship model (case does not matter); FACTION_NONE */
int Faction_Find(const FactionTable *t, const char *name);
int Faction_OfClass(const FactionTable *t, const char *cls);
int Faction_OfShip(const FactionTable *t, const char *model);
/* the relation of faction a to faction b (FREL_NEUTRAL for none or the same) */
int Faction_Relation(const FactionTable *t, int a, int b);
/* the rank (1..nranks) of so many arrests */
int Faction_Rank(const FactionDef *f, int arrests);
/* the ability's name (join, siren, ...), the crime's name; NULL past the end */
const char *Faction_AbilityName(int a);
const char *Faction_CrimeName(int c);

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
enum { FACTION_ST_STATE, FACTION_ST_NOTICE };

struct cEntity;
struct Net2Host;

/* the table (loaded once, both sides); NULL when the built-in text is bad */
const FactionTable *Faction_Table(void);

/* instance: -mode, -policepvp; 1 when param is one of them */
byte Faction_ParseParam(const char *param, const char *ext);
/* -mode:freeroam */
byte Faction_Freeroam(void);
/* the mode's name for the server info */
const char *Faction_ModeName(void);
/* the state of player i from its ticket's text (NULL: a new player); its keys into the text */
void Faction_Admit(int i, const char *state, int len);
void Faction_State(int i, char *state, int *len, int cap);
/* the class player i spawns as; the pilot of the ships it takes (P_Police for the police, else P_Dtritus) */
const char *Faction_CharClass(int i);
const char *Faction_Pilot(int i);
/* the player's character is new (a spawn, a transform, a ship left) or the player left */
void Faction_Spawned(int i, struct cEntity *e);
void Faction_Left(int i);
/* a client's message on the reliable channel; 1 when it was a faction request */
byte Faction_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* each tick after the frame: the requests, the crimes, the siren, the wanted levels, the status to the clients */
void Faction_Tick(struct Net2Host *host, uint64_t tick);
/* victim was hurt (died: killed) by attacker (an OnDamage or OnDeath of the instance) */
void Faction_Crime(struct cEntity *victim, struct cEntity *attacker, byte died);
/* police against a wanted player with -policepvp:1 */
byte Faction_MayHurt(struct cEntity *attacker, struct cEntity *victim);
/* the ship the player calls (its police ship after the ship ability), NULL for the others */
struct cEntity *Faction_CallShip(int i, const void *pos, float yaw);
/* NETREP_FL_ flags of a replicated entity (the siren of a player's ship) */
uint8_t Faction_Flags(struct cEntity *e);
/* a test line of the instance: "faction <player> join|leave|<ability>|wanted <points>|arrests <n>|dump <pattern>"; 1
 * when it was one */
byte Faction_Command(const char *line);

/* client: a status message of the instance; each frame (the keys, the text line); a proxy's or the own entity's state
 * flags; the siren of a vehicle for its flashes (-1: not a player's ship of the network, 0 off, 1 on); the class of a
 * character proxy loaded when it is new to the level */
void Faction_ClientMessage(const uint8_t *data, int size);
void Faction_ClientFrame(void);
void Faction_EntityFlags(struct cEntity *e, uint8_t flags);
int Faction_SirenOf(struct cEntity *v);
void Faction_EnsureClass(const char *cls);
void Faction_ClientReset(void);

#endif
