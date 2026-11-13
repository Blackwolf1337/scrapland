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
 */
#ifndef SCRAP_PLAYERS_H
#define SCRAP_PLAYERS_H

#include "scrap/shared.h"

#define PLAYERS_MAX 64
#define PLAYERS_NET_SLOT 4			/* the owner slot of the instance's player 0 */

struct cEntity;

/* the instance's table is in use (the headless instance) */
byte Players_Net(void);
/* the entity of player i (the instance's players, else the owner slots 0..3), NULL */
struct cEntity *Players_Get(int i);
/* the player of the entity, -1 for an entity of no player */
int Players_Of(struct cEntity *e);
/* e is a player's entity (outside the instance: e == cWorld_GetUsrEntity(g_pWorld, 0), so also NULL == NULL) */
byte Players_Is(struct cEntity *e);
/* some player's entity, NULL when there is none */
struct cEntity *Players_Any(void);
/* the player nearest to pos (outside the instance: player 0), NULL */
struct cEntity *Players_Nearest(const vec3_t *pos);
/* the square of the distance of the nearest player of the instance to pos, FLT_MAX without one (outside the instance
 * always FLT_MAX) */
float Players_MinDistSq(const vec3_t *pos);
/* the player of this machine (the HUD, the camera, the sounds): cWorld_GetUsrEntity(g_pWorld, 0) */
struct cEntity *Players_Local(void);

/* the owner scan of the world: Players_Clear, then Players_SetOwner for each active character or ship with a slot */
void Players_Clear(void);
void Players_SetOwner(int slot, struct cEntity *e);

#endif
