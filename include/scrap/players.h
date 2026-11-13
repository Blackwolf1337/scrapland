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
 * The game state of a player (stage M4a): the world keeps one money (g_pWorld->money) and one alarm
 * (g_pWorld->alarm) that the game code and the scripts read and change wherever they act for "the player". The
 * instance keeps them per player and binds one player's into the world at a time (Players_Bind): the player whose
 * input runs (its actions, hits, pickups), the player nearest to an entity while that entity updates (the NPC that
 * sees a crime raises that player's alarm and reacts to it), each player in turn for the alarm's own update and
 * the alarm level of the logic, and between those the player of the frame's turn (a different one every frame, so
 * a script that polls "the player" every frame sees all of them in turn). Players_Any() and the scripts' player
 * (Mode_Usr, Scrap.UsrEntity(0)) are the bound player's entity there. Outside the instance nothing is bound.
 */
#ifndef SCRAP_PLAYERS_H
#define SCRAP_PLAYERS_H

#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define PLAYERS_MAX 64
#define PLAYERS_NET_SLOT 4			/* the owner slot of the instance's player 0 */
#define PLAYERS_NONE PLAYERS_MAX	/* Players_Bind: no player (the world's own state) */
#define PLAYERS_WANTED_REACH 3000.0f	/* units: an NPC reacts to a wanted player this near before a nearer innocent
									   one (the distance a character attacks what it sees, GenCharLogic) */

/* the sources of the alarm's level (Alarm_SetGrow of LogicControl_Update) kept per player */
enum { PLAYERS_LV_POLICE, PLAYERS_LV_AI, PLAYERS_LV_CHARS, PLAYERS_LEVELS };

struct cEntity;

/* the instance's table is in use (the headless instance) */
byte Players_Net(void);
/* the entity of player i (the instance's players, else the owner slots 0..3), NULL */
struct cEntity *Players_Get(int i);
/* the player of the entity, -1 for an entity of no player */
int Players_Of(struct cEntity *e);
/* e is a player's entity (outside the instance: e == cWorld_GetUsrEntity(g_pWorld, 0), so also NULL == NULL) */
byte Players_Is(struct cEntity *e);
/* some player's entity, NULL when there is none (the instance: the bound player's when it has one) */
struct cEntity *Players_Any(void);
/* the player an NPC at pos reacts to (outside the instance: player 0), NULL: the nearest player whose alarm is on
 * within PLAYERS_WANTED_REACH, else the nearest */
struct cEntity *Players_Nearest(const vec3_t *pos);
/* the nearest player to pos whatever its alarm (outside the instance: player 0), NULL */
struct cEntity *Players_Closest(const vec3_t *pos);
/* the square of the distance of the nearest player of the instance to pos, FLT_MAX without one (outside the instance
 * always FLT_MAX) */
float Players_MinDistSq(const vec3_t *pos);
/* the scripts' player i (Scrap.UsrEntity(i), Mode_Usr): cWorld_GetUsrEntity(g_pWorld, i); on the instance the bound
 * player's entity for 0 (Players_Any), NULL for others */
struct cEntity *Players_Script(int i);
/* the player of this machine (the HUD, the camera, the sounds): cWorld_GetUsrEntity(g_pWorld, 0) */
struct cEntity *Players_Local(void);

/* the owner scan of the world: Players_Clear, then Players_SetOwner for each active character or ship with a slot */
void Players_Clear(void);
void Players_SetOwner(int slot, struct cEntity *e);

/* ---- the game state per player (the instance; no-ops elsewhere) ---- */

/* player i joined (fresh state: money, alarm off) or left */
void Players_Join(int i, int money);
void Players_Part(int i);
/* the state of player i (PLAYERS_NONE or an unused one: the world's own) into the world */
void Players_Bind(int i);
/* the joins of player slot i so far (0 outside the instance); player i bound when it is still the same join (a
 * scheduled call runs with the player that scheduled it), else the binding stays */
uint16_t Players_Joined(int i);
void Players_BindJoined(int i, uint16_t joined);
/* the player of the entity (or the owner of the ship: Players_ShipOwner), the one an NPC at pos reacts to
 * (Players_Nearest), the one of this frame's turn */
void Players_BindEntity(struct cEntity *e);
void Players_BindNearest(const vec3_t *pos);
void Players_BindTurn(void);
/* the own ship of player i (a ship it called): its Parking.PlayerShip of the scripts (that variable and
 * Parking.LocalShip are kept per player and bound with the rest); NULL for none */
void Players_SetShip(int i, struct cEntity *ship);
/* player i's own ship while it is in the world, NULL */
struct cEntity *Players_Ship(int i);
/* the player whose own ship e is, -1 */
int Players_ShipOwner(struct cEntity *e);
/* the bound player, PLAYERS_NONE */
int Players_Bound(void);
/* the money and the alarm of player i whether it is bound or not */
int Players_Money(int i);
const struct cAlarm *Players_Alarm(int i);
/* a weapon of attacker (a ship or character, or the owner of a missile) may hurt victim: always but on an instance
 * with -pvp:0 between two players' entities */
byte Players_MayHurt(struct cEntity *attacker, struct cEntity *victim);
/* the nearest player whose alarm is on, NULL */
struct cEntity *Players_NearestWanted(const vec3_t *pos);
/* the alarm level of a source for the bound player (Players_LevelReset at the start of the source's think: -1 for
 * all players) */
void Players_LevelReset(int source);
void Players_LevelRaise(int source, int level);
/* the alarms' update of the frame (outside the instance Alarm_Update of the world's), their grow from the level of
 * the logic (outside the instance Alarm_SetGrow of the world's with level; the instance takes each player's own) */
void Players_UpdateAlarms(float dt);
void Players_SetGrow(int level);

#endif
