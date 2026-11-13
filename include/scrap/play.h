/*
 * The players of the network (stages M2b and M3b of the multiplayer plan). Not in the original.
 *
 * Instance (src/game/App/Replicate.c): a connection whose hello asks for a player (NETREP_HELLO_PLAY of
 * include/scrap/netrep.h) gets a player slot of include/scrap/players.h and a Dtritus on foot at a spawn point of the
 * level ("DM_Player_Spawn*", else "DM_Spawn*"), named NetPlayer<i>; it is told the replication slot of its entity in
 * a player message. Its inputs come in its ack messages; every tick, before the frame, each player's input of that
 * tick drives its entity (NetPl_Step). The switch button gets the character into the nearest parked ship ("Parked"
 * of the parkings, no one in it) within PLAY_BOARD_RADIUS, or out of the ship (the ship stays parked there). A leave
 * removes the character (a ship the player was in stays parked). Hits of the instant-hit weapons (the Vulcan) of a
 * player are checked with the other entities where that player saw them (the lag compensation): the server time the
 * client showed came with its input, and the replication's history of samples gives the positions then.
 *
 * Client: ScrapC -play:<IPv4> [-port:<n>] [-imap:Levels/<level>]: a watcher (include/scrap/replicate.h) with its own
 * player. Its entity is a real local one (owner slot 0: the camera, the HUD and the controls of single player work
 * on it) that it steps itself once per tick with the input of that tick (NetPl_Step, the same code as the server),
 * NETREP_TICK_HZ times a second ahead of the server by half the round trip plus a lead. The inputs of the last ticks
 * go in every ack message. A snapshot record of the own entity is the server's state after the input of its tick:
 * when the predicted state of that tick is further off than PLAY_CORRECT_MIN the entity goes back to the server's
 * state and the inputs after that tick run again (actions and animations are not run again), and the change of the
 * drawn position fades out (NetRepSmooth). The action buttons are not run on the client (the server's hits and
 * projectiles come back replicated).
 *
 * Controls: those of single player. The switch: Use (Walk set) on foot, Mission info (Racer set) in a ship.
 * SCRAP_PLAYLOG=<file>: a CSV line per server state of the own entity (tick, error, correction, ...). SCRAP_PLAYBOT=1:
 * the client plays itself (walks, takes a ship, flies, fires at the nearest other player) for tests. SCRAP_NET2_LOSS
 * (percent) and SCRAP_NET2_DELAY (ms, each way, + SCRAP_NET2_JITTER ms) on either side: a test link.
 */
#ifndef SCRAP_PLAY_H
#define SCRAP_PLAY_H

#include "scrap/mathlib.h"
#include "scrap/netrep.h"
#include "scrap/shared.h"

#define PLAY_BOARD_RADIUS 4000.0f		/* units from a parked ship */
#define PLAY_LEAVE_SPEED 3000.0f		/* a ship gets left only below this speed (units/s) */
#define PLAY_CORRECT_MIN 2.0f			/* units of prediction error that make a correction */
#define PLAY_SMOOTH_HALF 0.08f			/* s: half life of the drawn offset of a correction */
#define PLAY_SMOOTH_SNAP 3000.0f		/* units: a larger correction is a jump */
#define PLAY_LEAD 0.075					/* s the input should be at the server before its tick */
#define PLAY_LOSS_LEAD 0.07				/* s more with loss: a later copy of a lost input comes in time */
#define PLAY_STEPS 2					/* slot ticks per tick: 1/60 s each, as a player's of single player */
#define PLAY_SPAWN_RING 400.0f			/* units between the players at a spawn point */

struct cEntity;

/* ---- the entity of a player (src/game/App/NetPlayer.c), both sides ---- */

/* the motion state the client goes back to for a correction */
typedef struct NetPlMotion {
	vec3_t pos, vel, ang, rotVel;
	vec3_t view, viewVel;			/* characters */
	float slideTime, displAng, lastMove, lastDisplAng, walkTime, move, moved2;
	vec3_t fallVel;
	byte falling, walking;
	float speed, smoothSpeed;		/* ships */
	float thrust, turnSpeedX, turnSpeedY, roll, boost, pushTime;
	vec3_t push;
} NetPlMotion;

/* the input into the controls of the entity (local: the client's own, its action buttons stay off) */
void NetPl_Apply(struct cEntity *e, const NetRepInput *in, byte local);
/* one tick of a player's entity with the input: its actions (not when the input runs again for a correction) and
 * PLAY_STEPS slot ticks */
void NetPl_Step(struct cEntity *e, const NetRepInput *in, byte local, byte replay);
/* the controls of the client's own entity as the input of a frame, added with weight dt into sum (the averages of
 * the tick come from NetPl_Input) */
typedef struct NetPlAccum {
	float move[3], rot[2], time;
	uint16_t buttons;
	uint8_t weapon;
} NetPlAccum;
void NetPl_Accumulate(struct cEntity *e, NetPlAccum *a, float dt);
void NetPl_Input(NetPlAccum *a, NetRepInput *out);
void NetPl_Save(struct cEntity *e, NetPlMotion *m);
void NetPl_Restore(struct cEntity *e, const NetPlMotion *m);
/* the replication kind of a player's entity: NETREP_CHAR, NETREP_SHIP, 0 */
int NetPl_Kind(struct cEntity *e);
/* the movement constants the scripts set on a ship (the player message): into phys (NETREP_PHYS_MAX), the count;
 * and back into the client's copy */
int NetPl_GetPhys(struct cEntity *e, float *phys);
void NetPl_SetPhys(struct cEntity *e, const float *phys, int n);

/* ---- the instance's players (Replicate.c) ---- */

/* before the frame of tick: each player's input of the tick into its entity */
void Rep_PreFrame(uint64_t tick);
/* the lag compensation around a hit check of the vehicle v (only for a player's): the other replicated entities
 * where v's client saw them, and back */
void Rep_RewindBegin(struct cEntity *v);
void Rep_RewindEnd(struct cEntity *v, struct cEntity *target, int shots);

/* ---- the client (Play.c) ---- */

extern byte g_Play;

byte Play_ParseParam(const char *param, const char *ext);
/* the replication's events of the own slot (Watch.c) */
int Play_OwnSlot(void);
void Play_SetOwn(int player, int slot, int kind, uint32_t tick, const float *phys, int nphys);
void Play_OwnCreate(int kind, const char *look, uint32_t tick, const NetRepState *st);
void Play_OwnUpdate(uint32_t tick, const NetRepState *st);
void Play_OwnRemove(void);
struct cEntity *Play_Entity(void);
/* the level runs: the camera; each frame before and after App_Frame (Watch.c) */
void Play_Start(void);
void Play_PreFrame(double now, double rtt, double rttVar, float loss);
/* 1 when an ack with the inputs is due now: its inputs (newest first) and the shown server time */
int Play_TakeInputs(NetRepInput *in, int *n, uint32_t *shownMs);
void Play_PostFrame(float dt);
void Play_Status(void);
void Play_Close(void);

#endif
