/*
 * The PvP modes of the original on the new network (stage M8 of the multiplayer plan). Not in the original.
 *
 * The original's online modes (scripts/net/ffa.py and the n*.py modes, src/game/Script/Ffa.c and NetModes.c) ran on
 * the old protocol, whose server and clients traded scores as texts keyed by the names of the ships. Here the same
 * rules run on a headless instance (include/scrap/instance.h) of an arena level, each player in a ship of its own
 * (the garage ship of its client) with the instance's binding, prediction and lag compensation:
 *
 *   ScrapC -instance:FZ -mode:dm|dmbots|tdm|flaghunt|teamflag|postman|teampost [-fraglimit:<n>] [-timelimit:<min>]
 *          [-maps:<level>,<level>,...] [-endsecs:<s>]
 *
 * The modes (ARENA_*): DeathMatch, DeathMatch with bots (the BotsList of NDeathMatchBots fly as AI ships), TeamDM,
 * FlagHunt (a flag at a random DM_Multiplayer_Flag_ dummy: who touches it scores, it moves on), TeamFlag (the same for
 * two teams), Postman (the mail bag at DM_Domination_Flag: its carrier takes it to a random DM_Domination_Zone_ for a
 * point, a dead carrier drops it, a lone bag goes back after AUTO_RESPAWN_FLAG_TIME) and TeamPost. Scores as the
 * original counts them (Ffa.OnDeathServer and the DeathFunc of each mode: a kill +1, a suicide or a team kill -1, the
 * flag modes take a point from the dead, the postman modes do not count kills in Postman); the round counts once two
 * players (one of each team) played; it ends at the frag limit (the Fraglimit of the configuration, -fraglimit) or the
 * time limit (-timelimit, none by default; new); after the end screen (-endsecs, 10 s as the original) the next level
 * of -maps comes: in a coordinator's world all players travel there (include/scrap/travel.h), alone the round starts
 * again on the same level. Bots of the original's DeathMatch with bots are members as players are.
 *
 * The state (ArenaState: the members with their names, teams, scores, ships; the totals, the flag, the phase and the
 * time) goes to every player's client in full whenever it changed (at most every ARENA_SEND_MS), so a late joiner,
 * a player at the end screen or after a team change has all of it; events (kills, joins, the flag) go as they happen.
 * The races of the original are gone with it: a joining player waits for its first ship as a member, its ship comes
 * with the next spawn (never during the end screen), a team change removes its ship and counts as a new join, a
 * carrier that leaves or spectates drops the bag where it was.
 *
 * Client (src/game/App/ArenaClient.c): the original's HUD from that state: the scores (RacerScorer.ShowScorer, held
 * with the Scorer key, shown after a death and at the end), the mission scorer of the limit and the best and own
 * score or the team totals, the colours of the teams on the proxies (TgType) and their names (Descriptor), the flag or
 * bag and its zone as local items with the waypoint, the texts of the original's modes. Requests: the join (name,
 * team, the garage ship), fire after a death (a new ship), spectate, a team. Keys (Racer set): Fire respawns, QMsg1
 * spectate on or off, QMsg2 red team, QMsg3 green team (the original's team menu).
 *
 * Wire (the instance connection's reliable channel; strings u8 length + bytes):
 *   'D' instance -> client: kind u8, then
 *       ARENA_ST_STATE: mode u8, phase u8, started u8, round u8, fraglimit svar, time left svar (s, -1 none), end left
 *         svar (s), red svar, green svar, you u8 (the receiver's member, 255 none), flag u8 (ARENA_FL_*), flag position
 *         3 svar, carrier u8 (member + 1), zone str, next level str, members u8, per member: index u8, flags u8
 *         (ARENA_MF_*), team u8, score svar, slot u16 (the replication slot + 1 of its ship), name str
 *       ARENA_ST_EVENT: event u8 (ARENA_EV_*), member a u8 (255 none), member b u8, number svar
 *   'U' client -> instance: request u8 (ARENA_RQ_*), then for ARENA_RQ_JOIN: name str, team u8, ship u16 length + bytes
 *       (the ten lines of the garage ship, Ffa.BuildCfgString); ARENA_RQ_TEAM: team u8
 */
#ifndef SCRAP_ARENA_H
#define SCRAP_ARENA_H

#include <stdint.h>

#include "scrap/shared.h"

/* the modes */
enum { ARENA_DM, ARENA_DMBOTS, ARENA_TDM, ARENA_FLAGHUNT, ARENA_TEAMFLAG, ARENA_POSTMAN, ARENA_TEAMPOST, ARENA_MODES };

#define ARENA_F_TEAM 0x01			/* two teams, red and green */
#define ARENA_F_FLAG 0x02			/* FlagHunt's flag */
#define ARENA_F_POST 0x04			/* Postman's mail bag and zones */
#define ARENA_F_BOTS 0x08			/* the BotsList of the mode's module */

typedef struct ArenaModeDef {
	const char *arg;				/* of -mode ("dm") */
	const char *type;				/* the original's ServerType ("DeathMatch"): Net_Type_<type>, Net_Help_<type> */
	const char *module;				/* the original's module of the mode ("NDeathMatch"): its data */
	int flags;						/* ARENA_F_ */
} ArenaModeDef;

extern const ArenaModeDef g_ArenaModes[ARENA_MODES];

#define ARENA_PLAYERS 64			/* PLAYERS_MAX: member i is player i */
#define ARENA_BOTS 8
#define ARENA_MEMBERS (ARENA_PLAYERS + ARENA_BOTS)	/* the bots after the players */
#define ARENA_NAME 31
#define ARENA_ZONE 31
#define ARENA_LEVEL 48
#define ARENA_NOBODY 255

enum { ARENA_TEAM_NONE, ARENA_TEAM_RED, ARENA_TEAM_GREEN };	/* "killer", "red", "green" of the original */
enum { ARENA_PH_WAIT, ARENA_PH_PLAY, ARENA_PH_END };
enum { ARENA_FL_NONE, ARENA_FL_LIES, ARENA_FL_CARRIED };

#define ARENA_MF_BOT 0x01
#define ARENA_MF_SPECT 0x02			/* a spectator (no ship, its score shows SPECT) */
#define ARENA_MF_SHIP 0x04			/* has a ship now */
#define ARENA_MF_JOINED 0x08		/* sent its join (a player) */

typedef struct ArenaMember {
	uint8_t used, flags, team;
	int32_t score;
	uint16_t slot;					/* the replication slot + 1 of its ship, 0 none */
	char name[ARENA_NAME + 1];
} ArenaMember;

typedef struct ArenaState {
	uint8_t mode, phase, started, round;
	int32_t fraglimit;
	int32_t timeLeft;				/* s, -1 no time limit */
	int32_t endLeft;				/* s of the end screen left */
	int32_t red, green;				/* the team totals */
	uint8_t you;					/* the receiver's member (written per client), ARENA_NOBODY */
	uint8_t flag;					/* ARENA_FL_ */
	int32_t flagPos[3];
	uint8_t carrier;				/* member + 1, 0 none */
	char zone[ARENA_ZONE + 1];		/* the key of the flag's dummy (FlagHunt) or of the bag's target zone (Postman) */
	char next[ARENA_LEVEL + 1];		/* the next level */
	ArenaMember m[ARENA_MEMBERS];
} ArenaState;

/* the events */
enum {
	ARENA_EV_KILL,					/* b killed by a (ARENA_NOBODY: a suicide or the world) */
	ARENA_EV_JOIN,					/* a joined (its team in the state) */
	ARENA_EV_LEAVE,					/* a left */
	ARENA_EV_SPECT,					/* a became a spectator */
	ARENA_EV_FLAG,					/* a took the flag (FlagHunt: scored) or the bag */
	ARENA_EV_DROP,					/* the bag dropped (a its carrier) */
	ARENA_EV_BACK,					/* the bag went back to its start */
	ARENA_EV_DELIVER,				/* a delivered the bag */
	ARENA_EV_WAIT,					/* a touched the flag before the round counts */
	ARENA_EV_END,					/* the round ended */
	ARENA_EV_START,					/* a new round */
	ARENA_EV_COUNT
};

/* the requests */
enum { ARENA_RQ_JOIN, ARENA_RQ_RESPAWN, ARENA_RQ_SPECT, ARENA_RQ_TEAM };

#define ARENA_MSG 'D'				/* instance -> client */
#define ARENA_MSG_REQ 'U'			/* client -> instance */
#define ARENA_ST_STATE 0
#define ARENA_ST_EVENT 1
#define ARENA_SHIP_CFG 512			/* bytes of the ship text of a join */
#define ARENA_SEND_MS 250			/* a changed state goes out at most this often */
#define ARENA_RESPAWN_SECS 1.0		/* after a death fire brings a new ship (Ffa.ShowPresFireMessage) */
#define ARENA_AUTO_SECS 10.0		/* and without fire it comes after this (Ffa.DelayedRestartOnTime) */
#define ARENA_REJOIN_SECS 10.0		/* a team change waits this long (Ffa.RejoinMsg) */
#define ARENA_BOT_SECS 3.0			/* a dead bot is reborn after this (NDeathMatchBots.DeathFunc) */
#define ARENA_END_SECS 10			/* the end screen (Ffa.ServerEndOfGame) */
#define ARENA_FLAG_RADIUS 2000.0f	/* the flag's take radius (NFlagHunt.CreateFlag) */
#define ARENA_ZONE_RADIUS 3000.0f	/* a postman zone's (NPostman.BrowseAreas) */
#define ARENA_INTEREST 1000000.0f	/* the interest radius of a client: the whole arena (the original sent every ship to
									   every client) */
#define ARENA_SPECTATOR_POINTS (-999999)	/* RacerScorer.SPECTATOR_POINTS: a spectator's score in the scorer */

/* ---- the pure part (src/arena/ArenaDef.c, unit test arena_test) ---- */

/* the mode of a -mode argument or a ServerType (case does not matter), -1 */
int Arena_ModeOf(const char *name);
/* the changes of a death: victim killed by killer (-1: a suicide or the world), as Ffa.OnDeathServer and the mode's
 * DeathFunc count them; the scores and totals of s changed */
void ArenaRules_Kill(ArenaState *s, int victim, int killer);
/* a point of the flag (FlagHunt, TeamFlag) or a delivery (Postman, TeamPost) for member k */
void ArenaRules_Score(ArenaState *s, int k);
/* the round counts: two members with ships (one per team in a team mode); s->started is set and stays */
byte ArenaRules_Started(ArenaState *s);
/* the frag limit is reached (a member, or a team in a team mode) */
byte ArenaRules_Ended(const ArenaState *s);
/* the team a joining member gets: want (ARENA_TEAM_RED, ARENA_TEAM_GREEN) unless that team has more members than the
 * other, then the other; ARENA_TEAM_NONE outside the team modes; skip: a member not counted (the one joining) */
int ArenaRules_JoinTeam(const ArenaState *s, int want, int skip);
/* the level after cur in the comma separated list (the first when cur is not in it, cur when the list is empty) */
void Arena_NextLevel(const char *list, const char *cur, char *out, int cap);
/* the rows of the scorer (Ffa.GetScorerInfo): the members in order (the leading team, the other team, the players of
 * no team, the spectators; each by score, the member you before an equal score); their count */
int Arena_Rows(const ArenaState *s, int you, int *rows, int max);

/* the wire: the state for the member you (ARENA_NOBODY: a watcher), an event, the requests; the size, 0 when it does
 * not fit. The readers: 1 when the message was whole */
int Arena_WriteState(uint8_t *buf, int cap, const ArenaState *s, int you);
byte Arena_ReadState(const uint8_t *buf, int size, ArenaState *s);
int Arena_WriteEvent(uint8_t *buf, int cap, int ev, int a, int b, int num);
byte Arena_ReadEvent(const uint8_t *buf, int size, int *ev, int *a, int *b, int *num);
int Arena_WriteJoin(uint8_t *buf, int cap, const char *name, int team, const char *ship);
int Arena_WriteRequest(uint8_t *buf, int cap, int rq, int team);
/* a request: *rq, and for a join the name, team and ship text, for a team the team */
byte Arena_ReadRequest(const uint8_t *buf, int size, int *rq, char *name, int ncap, int *team, char *ship, int scap);

/* ---- the instance (src/game/App/Arena.c) ---- */

struct cEntity;
struct Net2Host;

/* the original's module of the mode loaded (Scrap.PreloadLibrary, import: its data, both sides): 1 */
byte Arena_Module(int mode);

/* -mode (the arena modes; the others are left to the factions), -fraglimit, -timelimit, -maps, -endsecs */
byte Arena_ParseParam(const char *param, const char *ext);
/* an arena mode runs on this instance */
byte Arena_On(void);
/* the mode's -mode name ("dm"), NULL outside the arena modes; -maps ("" none) */
const char *Arena_ModeName(void);
const char *Arena_Maps(void);
/* player i connected (no ship yet: it waits for its join and the spawn), left */
void Arena_Joined(int i);
void Arena_Left(int i);
/* the ship of player i, named name, at a free spawn point of the level (OutMap.GetFreeSpawn), NULL */
struct cEntity *Arena_MakeShip(int i, const char *name);
/* player i died in e (the killer is the one whose weapon hit it last) */
void Arena_Died(int i, struct cEntity *e);
/* player i, without a ship for ticks, gets one now */
byte Arena_SpawnDue(int i, uint64_t ticks);
/* a client's request on the reliable channel; 1 when it was one */
byte Arena_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* each tick after the frame: the bots, the flag, the limits, the round, the state to the clients */
void Arena_Tick(struct Net2Host *host, uint64_t tick);
/* a test line of the instance: "arena status", "arena score <member> <n>", "arena end", "arena kill <member>
 * <killer>" (as a weapon of the killer, -1 none), "arena flag <member> [<dist>]" (its ship to the flag, the bag or
 * the bag's zone, or dist units east of it looking at it); 1 when it was one */
byte Arena_Command(const char *line);

/* ---- the client (src/game/App/ArenaClient.c) ---- */

void Arena_ClientMessage(const uint8_t *data, int size);
/* each frame of a player: the requests, the keys, the flag, the proxies, the HUD */
void Arena_ClientFrame(void);
void Arena_ClientReset(void);
/* the client plays an arena mode (it got a state) */
byte Arena_ClientOn(void);
/* the test bot of the client (SCRAP_PLAYBOT, include/scrap/play.h) in the flag modes: where to go (the flag, the bag,
 * the bag's zone when its own team carries it); 0 for none */
byte Arena_BotGoal(float *at);

#endif
