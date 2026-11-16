/*
 * The players of the network (stages M2b and M3b of the multiplayer plan). Not in the original.
 *
 * Instance (src/game/App/Replicate.c): a connection whose hello asks for a player (NETREP_HELLO_PLAY of
 * include/scrap/netrep.h) gets a player slot of include/scrap/players.h and a Dtritus on foot, named NetPlayer<i>, at
 * the spawn point of the level ("DM_Player_Spawn*", else "DM_Spawn*") with the fewest players near it, on a free place
 * there (a spawn point a new character falls from is not used again); it is told the replication slot of its entity
 * in a player message. Its inputs come in its ack messages; every tick, before the frame, each player's input of that
 * tick drives its entity (NetPl_Step). The switch button gets the character into the nearest parked ship ("Parked"
 * of the parkings, no one in it, owned by no other player) within PLAY_BOARD_RADIUS (with none there it calls a new
 * parked ship to a free place beside it: the sandbox has no shop yet; the player owns it, -ships of them at once), or
 * out of the ship (the ship stays parked there). A taken ship gets every weapon and full ammo. A leave removes the
 * character (a ship the player was in stays parked); a dead player's entity is left to the game, the death costs of
 * single player apply (-lives, -rebuild; the alarm goes off) and the player comes back on foot after -respawn s (the
 * player message without a slot has the kind NETREP_DEAD: the client's death camera). Hits of the instant-hit weapons
 * (Vulcan, Laser, Tesla, ATPC, Devastator) of a
 * player are checked with the other entities where that player saw them (the lag compensation): the server time the
 * client showed came with its input, and the replication's history of samples gives the positions then. The money,
 * the alarm and the ammo of a player go to its client in a status message; the hits of every ship in shots messages
 * (the effects on the proxies); missiles are replicated entities (their proxies are the game's network copies with
 * their effects). The client fires its own instant-hit weapons itself (the effects at once) and launches its missile
 * weapons (their launch effects); its prediction collides with the other players where they are at the predicted
 * tick (Watch_ProxiesAt).
 *
 * Client: ScrapC -play:<IPv4> [-port:<n>] [-imap:Levels/<level>] [-headless]: a watcher (include/scrap/replicate.h)
 * with its own player (-headless: no window, render device, sound or input, the frames at PLAY_HEADLESS_HZ: the bots
 * of the load tests) and its own
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
 * the client plays itself (walks, takes a ship, flies, fires at the nearest other player) for tests; =npc: it fires
 * at ships of no player; =target: it takes a ship and stays in the air; =walk: it walks around on foot; =idle: no
 * input (the tests drive it with the instance's test lines); =use: it stands and presses use every 3 s after 10 s (doors);
 * SCRAP_PLAYBOT_WEAPONS=1 (or weapon names): it fires each weapon for 6 s of aiming in turn and stays in the ship;
 * SCRAP_PLAYBOT_CM=1: it also holds the countermeasure of the weapon now and then;
 * SCRAP_PLAYAHEAD=0: the prediction collides with the others where they are drawn, =all: the characters near it at
 * the predicted tick too (tests). SCRAP_NET2_LOSS (percent) and SCRAP_NET2_DELAY (ms, each way, +
 * SCRAP_NET2_JITTER ms) on either side: a test link.
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
#define PLAY_SMOOTH_SNAP 8000.0f		/* units: a larger correction is a jump */
#define PLAY_LEAD 0.075					/* s the input should be at the server before its tick */
#define PLAY_LOSS_LEAD 0.07				/* s more with loss: a later copy of a lost input comes in time */
#define PLAY_STEPS 2					/* slot ticks per tick: 1/60 s each, as a player's of single player */
#define PLAY_SLOT 0.0166666675f			/* 1/60: s of a slot of a player's character and ship (SetUsrControl) */
#define PLAY_SHIPS_MAX 8					/* called ships a player may own (-ships) */
#define PLAY_RESPAWN_SECS 3				/* s from a death to the new character (-respawn) */
#define PLAY_HEADLESS_HZ 60				/* frames a second of a headless player */

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
	byte collision[0x20];			/* cVehicleEntity.collision: the state of the world collision */
} NetPlMotion;

/* the input into the controls of the entity (local: the client's own, its action buttons stay off but the fire of
 * a NetPl_LocalWeapon outside a replay) */
void NetPl_Apply(struct cEntity *e, const NetRepInput *in, byte local, byte replay);
/* The step of a player's entity, the same functions on the server and in a client's prediction: single player's
 * frame of the entity at 60 frames a second, its actions (cActCtrl_Tick) and then its slot (Obj_Tick), each of 1/60 s
 * (times the entity's time speed).
 * one tick of a player's entity with the input: PLAY_STEPS slot parts */
void NetPl_Step(struct cEntity *e, const NetRepInput *in, byte local, byte replay);
/* slot part (0 .. PLAY_STEPS - 1) of such a tick, the moves of the input's slot: its actions (not when the input runs
 * again for a correction), then its slot; the client steps a tick in parts as the frames pass its slots */
void NetPl_StepPart(struct cEntity *e, const NetRepInput *in, byte local, byte replay, int part);
/* the two halves of a part: the actions of a frame of dt, and a slot of length iv with the input (the solo instance
 * runs them in single player's own frame pattern: a frame of the tape without a slot, one, or two) */
void NetPl_Actions(struct cEntity *e, float dt);
void NetPl_Slot(struct cEntity *e, const NetRepInput *in, byte local, byte replay, int part, float iv);
/* the controls of the client's own entity as the input of a frame, added with weight dt into sum (the averages of
 * the tick come from NetPl_Input) */
typedef struct NetPlAccum {
	float move[3], rot[2], time;
	uint16_t buttons;
	uint8_t weapon;
} NetPlAccum;
void NetPl_Accumulate(struct cEntity *e, NetPlAccum *a, float dt);
void NetPl_Input(NetPlAccum *a, NetRepInput *out);
/* the moves of the frame before the tick's second slot into out->move2 (none since the first slot: its moves) */
void NetPl_InputSlot(const NetPlAccum *a, NetRepInput *out);
void NetPl_Save(struct cEntity *e, NetPlMotion *m);
void NetPl_Restore(struct cEntity *e, const NetPlMotion *m);
/* the client fires weapon w of its own ship itself for the effects at once (the instant-hit weapons; the hits it
 * makes stay local, the server's count): Vulcan, Laser, Tesla, ATPC, Devastator */
byte NetPl_LocalWeapon(int w);
/* the client launches missile weapon w of its own ship itself for the launch effects at once (Swarm, Inferno: their
 * client fire of the game's network mode, Play_Start); the missiles come replicated */
byte NetPl_LaunchWeapon(int w);
/* the replication kind of a player's entity: NETREP_CHAR, NETREP_SHIP, 0 */
int NetPl_Kind(struct cEntity *e);
/* the turn towards at as the test bots steer (rot[0] by the yaw, rot[1] by the pitch of at, or with rel by the pitch
 * still to turn, -1..1; a character's yaw is its view's): the yaw still to turn */
float NetPl_Aim(struct cEntity *e, const vec3_t *at, float rot[2], byte rel);
/* the movement constants the scripts set on a ship (the player message): into phys (NETREP_PHYS_MAX), the count;
 * and back into the client's copy */
int NetPl_GetPhys(struct cEntity *e, float *phys);
void NetPl_SetPhys(struct cEntity *e, const float *phys, int n);

/* ---- the instance's players (Replicate.c) ---- */

/* the sandbox options of the instance's command line (Instance_ParseParam): -ships:<n> called ships a player owns
 * at once (1; a call past it removes the oldest), -respawn:<s> (PLAY_RESPAWN_SECS), -lives:<n> (0: infinite; at
 * none left the player starts again with the start money), -rebuild:0|1 (1: a player's own ship that died with it
 * costs its price at the next call, as single player's rebuild; 0 by default: the sandbox has no income yet), -spawn:<n> (all players at spawn point n, tests;
 * without it they spread over the spawn points); 1 when param is one of them */
byte Rep_ParseParam(const char *param, const char *ext);
/* a test line of the instance's standard input: "life <player> <n>" sets the life of the player's entity (its
 * damage and death run as for a hit); 1 when it was one */
byte Rep_Command(const char *line);
/* before the frame of tick: each player's input of the tick into its entity */
void Rep_PreFrame(uint64_t tick);
/* the players of the instance use single player's own flows (stage 3.6 of the second multiplayer design): their own
 * ship and Sputnik at the level's parking (Parking.PlayerShip, Sputnik.spt per player), boarding a parked ship by its
 * use arrow (Sputnik.UseShip, Parking.UseShip), parking on a box (Parking.OnPark), with the scripts' UsrControl 0 as
 * the player's slot (Coop_UsrSlot, Coop_UsrRead) and the scripts' new character or ship as the player's (Coop_Adopts):
 * the solo instance and -mp2, not in coop (its own rules) nor the PvP modes; the old path keeps the instance's stand-ins
 * (the switch into the nearest parked ship, a called ship, Sputniks.c) */
byte Rep_SpFlows(void);
/* the frame of a player's own entity (cWithLifeEntity_Update, in the world's entity loop where single player steps
 * its player; dt its frame time): the step Rep_PreFrame gave it for this frame, NetPl_Step of the tick's input; the
 * solo instance on a tape of single player's frames: the actions of the frame and the frame's slots */
void Rep_OwnFrame(struct cEntity *e, float dt);
/* the lag compensation around the aim and hit check of an instant-hit weapon of the vehicle v (only for a
 * player's): the other replicated entities where v's client saw them, and back */
void Rep_RewindBegin(struct cEntity *v);
void Rep_RewindEnd(struct cEntity *v);
/* the damage of attacker's weapon may go to victim (outside the instance always): the server's PvP setting
 * (Players_MayHurt); a hit of a player is counted and logged ("hit by player") */
byte Rep_Hurt(struct cEntity *attacker, struct cEntity *victim, const char *weapon);
/* the hits of a ship's weapon its cVehicleAttack hands to the weapon's OnHit (the effects): count shots of weapon
 * from the point from to the point to (at target, NULL for a point), sent to the clients that see the ship */
void Rep_Shot(struct cEntity *shooter, int weapon, int count, const vec3_t *from, const vec3_t *to,
	struct cEntity *target);

/* ---- the client (Play.c) ---- */

extern byte g_Play;
/* a correction runs the own entity's ticks again */
extern byte g_PlayReplaying;

/* -headless: the frames of a headless player at PLAY_HEADLESS_HZ; 0 after a wait for an event up to the next one */
byte Play_HeadlessDue(void);

byte Play_ParseParam(const char *param, const char *ext);
/* the replication's events of the own slot (Watch.c) */
int Play_OwnSlot(void);
void Play_SetOwn(int player, int slot, int kind, uint32_t tick, const float *phys, int nphys);
void Play_OwnCreate(int kind, const char *look, uint32_t tick, const NetRepState *st);
void Play_OwnUpdate(uint32_t tick, const NetRepState *st);
void Play_OwnRemove(void);
/* the status message: the money, the alarm and the ammo of the own player into the world and the own ship (the
 * HUD of single player shows them) */
void Play_OwnStatus(const NetRepStatus *st);
struct cEntity *Play_Entity(void);
/* the death camera looks at the place of the own death (no own entity yet) */
byte Play_DeathCamOn(void);
/* the level runs: the camera; each frame before and after App_Frame (Watch.c) */
void Play_Start(void);
/* the player's camera of view 0, made when missing (at x y z) */
struct cEntity *Play_ViewCamera(float x, float y, float z);
void Play_PreFrame(double now, double rtt, double rttVar, float loss);
/* 1 when an ack with the inputs is due now: its inputs (newest first) and the shown server time */
int Play_TakeInputs(NetRepInput *in, int *n, uint32_t *shownMs);
void Play_PostFrame(float dt);
void Play_Status(void);
/* the mission info key of the own player: the sandbox's switch (into the nearest parked or a called ship, out of
 * the ship) with the next tick's input */
void Play_Switch(void);
/* the corrections of the own entity's prediction since the start */
uint32_t Play_Corrections(void);
/* the own player gone (a travel to another instance) */
void Play_Reset(void);
void Play_Close(void);

#endif
