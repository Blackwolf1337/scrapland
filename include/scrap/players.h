/*
 * The players of the world (stage M2b of the multiplayer plan, src/game/World/Players.c). Not in the original.
 *
 * The original knows up to MAX_LOCAL_PLAYERS local players (split screen): the owner slots 0..3 ("UsrControl" of a
 * character or a ship), their entities in cWorld.owners, found every frame. The headless instance
 * (include/scrap/instance.h) has up to PLAYERS_MAX players of the network: player i drives the entity of owner slot
 * PLAYERS_NET_SLOT + i, found in the same scan into a table of its own, and owners stays empty there. The devices
 * never drive those slots (the code that reads the input of a slot checks slot < MAX_LOCAL_PLAYERS): the input comes
 * from the player's client (include/scrap/play.h). Code that asks "is it a human" with ownerSlot != -1 counts
 * them too.
 *
 * Code that asks for "the player" uses one of these: Players_Is(e) (e is a player's entity), Players_Nearest(pos)
 * (the player an NPC reacts to), Players_Any() (some player, NULL for none), Players_Local() (the player of this
 * machine: HUD, camera, sounds). Outside the instance all of them are cWorld_GetUsrEntity(g_pWorld, 0) of the
 * original, so the single player game and the old network modes do what they did.
 *
 * The player context (stage 1.1 of the second multiplayer design): the game state of one player (cPlayer below:
 * its entity, own ship, money, alarm, the alarm levels of the logic, its viewer pose, input, camera of view 0 and
 * action set). Code that acts for "the player" reads and changes the money and the alarm of the current context
 * (Players_CurMoney, Players_CurAlarm) and the module variables tagged PLAYER through its overlay
 * (include/scrap/modvars.h). The current context is a pointer, switched (Players_Enter, Players_Leave), never copied.
 * Single player and the old network modes have one context, the world's own: its money and alarm are the fields of
 * the world (g_pWorld->money, g_pWorld->alarm), so single player does what it did. The instance has one context per
 * player with its own money and alarm, and the world's own (PLAYERS_NONE) for the rest.
 *
 * Dispatch (stage 1.3): the instance enters a player's context only where the game knows the player: its input step
 * (src/game/App/Replicate.c), the update of its own entity or ship and so the action events and the arrow use of it
 * (World.c), a hit by or of a player (WL_RunFunc), an item it takes, the callbacks of its alarm and of the talk it
 * started, a call it scheduled (the scheduler keeps the player; Scheduler_Delete in a player's context leaves the
 * other players' calls alone), the coop, faction and travel code that acts for a player. Everything else runs in the
 * world's context. A read of "the player" there (Players_Any, Players_Script and the scripts' accessors, the money,
 * the alarm, a module variable tagged PLAYER, Players_ActingAt) goes through one resolver: the row of the site in
 * tools/script/netclass/playerread.tsv says NEAREST (to the entity that runs: Players_Run), ANY, PARTY or WORLD, and
 * the network audit counts each read by its rule (player.world.<rule>:<site>, player.world.norule for a site without
 * a row). Players_Any() and the scripts' player are the current player's entity in a player's context.
 */
#ifndef SCRAP_PLAYERS_H
#define SCRAP_PLAYERS_H

#include "scrap/alarm.h"
#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define PLAYERS_MAX 64
#define PLAYERS_NET_SLOT 4			/* the owner slot of the instance's player 0 */
#define PLAYERS_NONE PLAYERS_MAX	/* no player: the world's own context */
#define PLAYERS_SEE_DIST 12000.0f		/* units a player sees in front of it (Players_Sees) */
#define PLAYERS_SEE_NEAR 1500.0f		/* units around a player it sees whatever its heading (its camera is behind it) */
#define PLAYERS_SEE_COS 0.5f			/* cosine of the half angle of a player's view */
#define PLAYERS_EYE 120.0f				/* a player's eyes above its place */
#define PLAYERS_WANTED_REACH 3000.0f	/* units: an NPC reacts to a wanted player this near before a nearer innocent
									   one (the distance a character attacks what it sees, GenCharLogic) */

/* the sources of the alarm's level (Alarm_SetGrow of LogicControl_Update) kept per player */
enum { PLAYERS_LV_POLICE, PLAYERS_LV_AI, PLAYERS_LV_CHARS, PLAYERS_LEVELS };

struct cEntity;

/* the context of a player (the instance: one per player; single player: only the world's own, whose money and alarm
 * are the fields of the world) */
typedef struct cPlayer {
	byte used;
	uint16_t joined;				/* joins of the slot so far */
	struct cEntity *ent;			/* its character or ship, found every frame (the owner scan) */
	struct cEntity *ship;			/* its own ship (Players_SetShip): compared, read while World_FindEntity(shipName)
									   is it */
	char shipName[64];
	int money;						/* not used by the world's own context: g_pWorld->money */
	cAlarm alarm;					/* not used by the world's own context: g_pWorld->alarm */
	int8_t level[PLAYERS_LEVELS];	/* the alarm level of each source (Players_LevelRaise), -1 */
	vec3_t viewPos;					/* where it looks from and the yaw it looks to: its entity's view (the client's
									   camera pose comes with stage 2.1) */
	float viewYaw;
	const struct NetRepInput *input;	/* its input of the tick while that runs, NULL */
	char camName[64];				/* the camera of view 0 the scripts set while it was current (cWorld_SetCam) */
	int actionSet;					/* the action set the scripts selected while it was current, -1 */
} cPlayer;

/* the instance's table is in use (the headless instance) */
byte Players_Net(void);
/* the entity of player i (the instance's players, else the owner slots 0..3), NULL */
struct cEntity *Players_Get(int i);
/* the player of the entity, -1 for an entity of no player */
int Players_Of(struct cEntity *e);
/* e is a player's entity (outside the instance: e == cWorld_GetUsrEntity(g_pWorld, 0), so also NULL == NULL) */
byte Players_Is(struct cEntity *e);
/* some player's entity, NULL when there is none (the instance: the current player's, in the world's context the one
 * of the site's rule); site names the caller for the network audit (include/scrap/netaudit.h) */
struct cEntity *Players_AnyAt(const char *site);
#define Players_Any() Players_AnyAt(__func__)
/* the player an NPC at pos reacts to (outside the instance: player 0), NULL: the nearest player whose alarm is on
 * within PLAYERS_WANTED_REACH, else the nearest */
struct cEntity *Players_Nearest(const vec3_t *pos);
/* the nearest player to pos whatever its alarm (outside the instance: player 0), NULL */
struct cEntity *Players_Closest(const vec3_t *pos);
/* the direction a player's entity looks into on the ground (a character's view, a ship's heading; the yaw 0 of an
 * entity looks to -z), length 1 */
void Players_Forward(struct cEntity *e, vec3_t *dir);
/* the instance: a player has p in view (nearer than PLAYERS_SEE_NEAR, or in front of it within PLAYERS_SEE_DIST),
 * with a line of sight from its eyes; outside the instance 0 */
byte Players_Sees(const vec3_t *p);
/* a line of sight from the eyes of a player standing at at to p */
byte Players_LineOfSight(const vec3_t *at, const vec3_t *p);
struct CGameModel;
/* the model is visible: outside the instance drawn in the last frame (CGameModel_IsVisible); the headless instance
 * draws nothing, so a model a player has in view (Players_Sees) counts, which the logic and the AI of the characters
 * read as single player reads its screen */
byte Players_SeesModelAt(const struct CGameModel *m, const char *site);
#define Players_SeesModel(m) Players_SeesModelAt(m, __func__)
/* the square of the distance of the nearest player of the instance to pos, FLT_MAX without one (outside the instance
 * always FLT_MAX) */
float Players_MinDistSq(const vec3_t *pos);
/* the scripts' player i (Scrap.UsrEntity(i), Mode_Usr): cWorld_GetUsrEntity(g_pWorld, i); on the instance the current
 * (or resolved) player's entity for 0 (Players_Any), NULL for others; site as for Players_AnyAt */
struct cEntity *Players_ScriptAt(int i, const char *site);
#define Players_Script(i) Players_ScriptAt(i, __func__)
/* the player of this machine (the HUD, the camera, the sounds): cWorld_GetUsrEntity(g_pWorld, 0) */
struct cEntity *Players_Local(void);

/* the owner scan of the world: Players_Clear, then Players_SetOwner for each active character or ship with a slot */
void Players_Clear(void);
void Players_SetOwner(int slot, struct cEntity *e);

/* ---- the game state per player (the instance; no-ops elsewhere) ---- */

/* player i joined (fresh state: money, alarm off) or left */
void Players_Join(int i, int money);
void Players_Part(int i);
/* the current context set to player i (PLAYERS_NONE or an unused one: the world's own); the previous one for
 * Players_Leave, which sets it back (no-ops outside the instance); site names the dispatch point for the trace of the
 * tests (SCRAP_PLAYER_TRACE=1: "pltrace" lines of each callback, money and alarm change with its player) */
int Players_EnterAt(int i, const char *site);
#define Players_Enter(i) Players_EnterAt(i, __func__)
void Players_Leave(int prev);
/* Players_Enter for the player of the entity (or the owner of the ship: Players_ShipOwner), else the world's */
int Players_EnterEntityAt(struct cEntity *e, const char *site);
#define Players_EnterEntity(e) Players_EnterEntityAt(e, __func__)
/* Players_Enter for the player whose owner slot e has, also while e is not one of the players found (made inactive by
 * the scripts), else the owner of the ship e */
int Players_EnterOwnerAt(struct cEntity *e, const char *site);
#define Players_EnterOwner(e) Players_EnterOwnerAt(e, __func__)
/* the joins of player slot i so far (0 outside the instance); Players_Enter for player i when it is still the same
 * join (a scheduled call runs with the player that scheduled it), else the world's */
uint16_t Players_Joined(int i);
int Players_EnterJoinedAt(int i, uint16_t joined, const char *site);
#define Players_EnterJoined(i, joined) Players_EnterJoinedAt(i, joined, __func__)
/* the entity the world's context runs for (the NEAREST rule of the resolver): an entity's update, a logic's think of
 * an entity; the previous one */
struct cEntity *Players_Run(struct cEntity *e);
/* the current player, PLAYERS_NONE in the world's context */
int Players_Current(void);
/* the player the code acts for: the current one, in the world's context the player of the site's rule (PLAYERS_NONE
 * for none) */
int Players_ActingAt(const char *site);
#define Players_Acting() Players_ActingAt(__func__)
/* the own ship of player i (a ship it called): its Parking.PlayerShip of the scripts (an overlay per player of the
 * module variables, include/scrap/modvars.h); NULL for none */
void Players_SetShip(int i, struct cEntity *ship);
/* player i's own ship while it is in the world, NULL */
struct cEntity *Players_Ship(int i);
/* the player whose own ship e is, -1 */
int Players_ShipOwner(struct cEntity *e);
/* the current context and the context of player i (PLAYERS_NONE: the world's own); NULL for an unused player */
cPlayer *Players_Cur(void);
cPlayer *Players_Ctx(int i);
/* the player whose overlay holds the module variable module.name (tagged PLAYER, or PARTY outside coop: ModVars.c)
 * now: the current player, in the world's context of the instance the player of the rule of "module.name"; -1 for
 * the module (single player, the rule WORLD) */
int Players_VarOwnerAt(const char *module, const char *name);
/* the money and the alarm of the current context (single player: the world's fields; the world's context of the
 * instance: the player of the site's rule); site names the caller */
int *Players_CurMoneyAt(const char *site);
cAlarm *Players_CurAlarmAt(const char *site);
#define Players_CurMoney() Players_CurMoneyAt(__func__)
#define Players_CurAlarm() Players_CurAlarmAt(__func__)
/* the player whose alarm a is, PLAYERS_NONE for the world's own */
int Players_OfAlarm(const cAlarm *a);
/* the scripts set the camera of view 0 or selected an action set: kept in the current player's context (the
 * instance) */
void Players_NoteCam(const char *name);
void Players_NoteActionSet(int set);
/* the money and the alarm of player i whether it is current or not */
int Players_Money(int i);
void Players_SetMoney(int i, int money);
const struct cAlarm *Players_Alarm(int i);
/* a weapon of attacker (a ship or character, or the owner of a missile) may hurt victim: always but on an instance
 * between two players' entities, where the freeroam factions decide first (include/scrap/faction.h), else -pvp */
byte Players_MayHurt(struct cEntity *attacker, struct cEntity *victim);
/* the nearest player whose alarm is on, NULL */
struct cEntity *Players_NearestWanted(const vec3_t *pos);
/* the alarm level of a source for the current player (the world's context: the player of the site's rule;
 * Players_LevelReset at the start of the source's think: -1 for all players) */
void Players_LevelReset(int source);
void Players_LevelRaiseAt(int source, int level, const char *site);
#define Players_LevelRaise(source, level) Players_LevelRaiseAt(source, level, __func__)
/* the alarms' update of the frame (outside the instance Alarm_Update of the world's), their grow from the level of
 * the logic (outside the instance Alarm_SetGrow of the world's with level; the instance takes each player's own) */
void Players_UpdateAlarms(float dt);
void Players_SetGrow(int level);

#endif
