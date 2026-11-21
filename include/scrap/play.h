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
 * -mp2 (stage 3.5 of the second design, prediction v2): the client runs the buttons of its own entity too but the
 * flows of the scripts (use, possession: the server starts them, the client sees them start when the server's state
 * comes) and the countermeasure: walking, jumping, a character's action, the boost, the full thrust, the lock, the
 * fire of the instant-hit and missile weapons (NETPL_MP2_LOCAL). With each snapshot the server sends the rollback
 * state of the player's entity after its step of the tick (NETREP_MSG_OWN, NetPl_OwnOf: position, velocity, angles,
 * life, a character's view, fall, walk, energy, its action layers and default loop with their times, a ship's
 * speed, thrust, turns, boost, push, cloud and EMI, the scripted move; NETPL_O_*). The client compares it with its own
 * state of that tick (NetPl_OwnDiff, NETPL_TOL_*); on a difference it goes back to its saved state of that tick
 * (NetPl_Save keeps the layers, the animation channels, the timed states and the move, NetPlMotion), takes the
 * server's (NetPl_OwnApply) and runs the inputs after it again, actions and slots alike, the presentation off
 * (g_PlayReplaying: the sound sinks, the presenting events of the layers, the boost and crash sounds and shakes are
 * muted, Play_Mute; the audit counts pred.muted:<what>, and pred.twice:<site> for a presentation sink a replay
 * reached all the same). The events of the own entity's layers: the presenting ones play at the prediction, the ones
 * that only set its layers run in both, the rest (script calls, the classes' logic, voices) is the server's
 * (Play_OwnEvent, pred.server:<event>). A move of the entity by the scripts (Pos, Ang, ViewAng or MoveTime set:
 * Rep_OwnMoved) counts in the state's moved: the client takes the next state as a jump, not smoothed and not a
 * correction (the hand-over of a flow: boarding, parking, possession, an elevator); a move the client made the same
 * itself changes nothing drawn. The first state of a new own entity hands it over. A difference that comes with one
 * of the life (the server's damage: a hit and its push) is taken, not counted as a correction (pred.took:life|hit), as
 * is a state after a collision with another ship (NETPL_OF_TOUCHED: the rail traffic, a ship of no player or another
 * player, whose motion the server alone knows; pred.took:touch), and one that differs in a cloud or EMI (the timed
 * states a shot's hit sets: the server's, the client's replica of the shot leaves the own entity, Play_IsOwn;
 * pred.took:effect). A state within the tolerances that is not the same is taken too, not counted (settled: the
 * prediction stays equal after it). The own entity that leaves the snapshot (a death) stays a moment, inactive, for the
 * records that name it (Play_Wreck).
 * Corrections by what differed: pred.corr:<pos|vel|ang|act|state|move>.
 *
 * Stage 3.5b: a player's ship touches the parking boxes in its step (every third slot, the 0.05 s of the box's own
 * slot; the box's own touch leaves such a ship alone): the server runs the box's OnTake (Parking.OnPark), the client
 * predicts what it does with the boxes it has as replicas: the brake of a fast ship (0.6 of its velocity) or the
 * start of the parking's move (Parking_PredictPark), so a take-off and a landing on a box cost no correction. The
 * prediction collides with the ships of no player near it where they are at the predicted tick (position and
 * velocity of their samples; the fights), the players' as before. The use pressed with the arrow on a parked ship
 * starts the boarding's camera path at once (Play_FlowStart: Parking.SetCameraStart's path to the camera behind the
 * ship, pred.flow:board) on the path entity of the server's camera paths (include/scrap/netscene.h), whose path of the
 * boarding goes on from there when it comes; the camera stays on the ship until the server's hand-over (the own ship
 * of the player message), without one in PLAY_FLOW_SECS the client gives it up (pred.flow:cancel). The controls of the own entity are read after the frame's
 * input update, before its step (Watch_FrameInput, Play_PreFrame), not in the entity's frame after the step: a press
 * steps in the frame that saw it, the frame after it draws it (single player draws it a frame later).
 *
 * Controls: those of single player. The switch: Use (Walk set) on foot, Mission info (Racer set) in a ship.
 * SCRAP_PLAYLOG=<file>: a CSV line per server state of the own entity (tick, error, correction, ...). SCRAP_PLAYBOT=1:
 * the client plays itself (walks, takes a ship, flies, fires at the nearest other player) for tests; =npc: it fires
 * at ships of no player; =target: it takes a ship and stays in the air; =walk: it walks around on foot; =idle: no
 * input (the tests drive it with the instance's test lines); =use: it stands and presses use every 3 s after 10 s (doors);
 * SCRAP_PLAYBOT_WEAPONS=1 (or weapon names): it fires each weapon for 6 s of aiming in turn and stays in the ship;
 * SCRAP_PLAYBOT_CM=1: it also holds the countermeasure of the weapon now and then;
 * SCRAP_PLAYBOT_BOOST=1: in a ship it also holds the boost for 0.4 s every 12 s;
 * SCRAP_PLAYAHEAD=0: the prediction collides with the others where they are drawn, =players: only the players at the
 * predicted tick (the ships of no player where they are drawn), =all: the characters near it at the predicted tick too
 * (tests). SCRAP_NET2_LOSS (percent) and SCRAP_NET2_DELAY (ms, each way, +
 * SCRAP_NET2_JITTER ms) on either side: a test link.
 */
#ifndef SCRAP_PLAY_H
#define SCRAP_PLAY_H

#include "scrap/actions.h"
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
	/* the rest of the rollback state (stage 3.5, -mp2) */
	byte full;						/* the fields below were saved */
	float cloud, emi, noFire, histTime, histSpeed, histAccLen, hitTime;	/* ships */
	vec3_t histVel, histAcc;
	byte cloudOn;
	float energyUsed, relaxTime, hitFlash, viewLockF, centerTime, floorDist;	/* characters */
	byte viewLock, prevAct, prevAct2, prevAct3, lockStart, lock;
	vec3_t moveStartPos, moveStartAng, moveLastPos, moveLastAng, movePos, moveAng;	/* the scripted move */
	float moveTime, moveLen;
	int moveFade;
	byte moveOn, moveView;
	byte acts;						/* the action controller below was saved */
	cActCtrlState act;
} NetPlMotion;

/* The rollback state the server sends (NetRepOwn of include/scrap/netrep.h, stage 3.5): the floats by index, the
 * first NETPL_O_COMMON of every kind, then the kind's own */
enum {
	NETPL_O_POS = 0, NETPL_O_VEL = 3, NETPL_O_ANG = 6, NETPL_O_ROTVEL = 9,
	NETPL_O_LIFE = 12,				/* the life (a ship's armour weighs: its acceleration follows it) */
	NETPL_O_COMMON = 13,
	/* characters */
	NETPL_O_VIEW = NETPL_O_COMMON, NETPL_O_VIEWVEL = NETPL_O_VIEW + 3, NETPL_O_FALLVEL = NETPL_O_VIEWVEL + 3,
	NETPL_O_SLIDE = NETPL_O_FALLVEL + 3, NETPL_O_DISPL, NETPL_O_LASTMOVE,
	NETPL_O_LASTDISPL, NETPL_O_WALKTIME, NETPL_O_MOVE, NETPL_O_MOVED2, NETPL_O_ENERGY, NETPL_O_WALKSPEED,
	NETPL_O_RELAX,					/* the time to the next relax action */
	NETPL_O_ACTTIME,				/* 4: the times of the layers */
	NETPL_O_ACTFRAME = NETPL_O_ACTTIME + 4,	/* 4: the frames of their channels (exact) */
	NETPL_O_CHAR = NETPL_O_ACTFRAME + 4,
	/* ships */
	NETPL_O_SPEED = NETPL_O_COMMON, NETPL_O_THRUST, NETPL_O_TURNX, NETPL_O_TURNY, NETPL_O_ROLL, NETPL_O_BOOST, NETPL_O_PUSHTIME,
	NETPL_O_PUSH, NETPL_O_CLOUD = NETPL_O_PUSH + 3, NETPL_O_EMI,
	NETPL_O_SHIP,
	/* the scripted move (both, after the kind's) */
	NETPL_O_MOVETIME = 0, NETPL_O_MOVELEN, NETPL_O_MOVEPOS, NETPL_O_MOVEANG = NETPL_O_MOVEPOS + 3,
	NETPL_O_MOVESTART = NETPL_O_MOVEANG + 3, NETPL_O_MOVESTARTANG = NETPL_O_MOVESTART + 3,
	NETPL_O_MOVEN = NETPL_O_MOVESTARTANG + 3
};
/* NetRepOwn.flags */
#define NETPL_OF_FALLING 0x01
#define NETPL_OF_WALKING 0x02
#define NETPL_OF_CLOUD 0x04			/* ship: cloudOn */
#define NETPL_OF_MOVE 0x08			/* the scripted move runs (its values follow the kind's) */
#define NETPL_OF_NOCTRL 0x10		/* character: the scripts took its control (DisableControl, the control off) */
#define NETPL_OF_ENERGYBAR 0x20		/* the world's EnergyBarActive (the character's energy and action presses read it) */
#define NETPL_OF_TOUCHED 0x40		/* the server's state: a collision with another ship moved it since the last state
									   or the one or two before (the rail traffic, a ship of no player, another player:
									   their motion is the server's, the client takes its result, not compared) */
/* what differs between two rollback states (NetPl_OwnDiff) */
#define NETPL_D_POS 0x01
#define NETPL_D_VEL 0x02
#define NETPL_D_ANG 0x04
#define NETPL_D_ACT 0x08			/* the actions of the layers, their times */
#define NETPL_D_STATE 0x10			/* the timed states: boost, cloud, EMI, energy, falling, walking, push */
#define NETPL_D_MOVE 0x20			/* the scripted move */
#define NETPL_D_LIFE 0x40			/* the life (the server's damage): taken, not counted as a correction */
#define NETPL_D_BITS 7

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
/* the moves of the frame before the tick's second slot into out->move2 (none since the first slot: its moves); -mp2:
 * the buttons it runs itself first pressed since the first slot into the late bits of out->buttons (NETREP_IN_LATES) */
void NetPl_InputSlot(const NetPlAccum *a, NetRepInput *out);
void NetPl_Save(struct cEntity *e, NetPlMotion *m);
void NetPl_Restore(struct cEntity *e, const NetPlMotion *m);
/* the rollback state of e as the server sends it (moved: the count of the moves outside its step) */
void NetPl_OwnOf(struct cEntity *e, uint32_t tick, uint8_t moved, NetRepOwn *o);
/* NETPL_D_* of what differs between the client's state a and the server's b beyond the tolerances */
int NetPl_OwnDiff(const NetRepOwn *a, const NetRepOwn *b);
/* a and b are the same state (every value equal, the touch flag aside) */
byte NetPl_OwnSame(const NetRepOwn *a, const NetRepOwn *b);
/* the ship states a (the client's) and b (the server's) differ in a timed state a shot's hit sets (a countermeasure's
 * cloud, an EMI: the server's alone on a -mp2 client, Play_IsOwn) */
byte NetPl_OwnEffect(const NetRepOwn *a, const NetRepOwn *b);
/* the server's state into e (after NetPl_Restore of the client's own state of that tick) */
void NetPl_OwnApply(struct cEntity *e, const NetRepOwn *o);
/* -mp2: a random draw (0 to 0x7fff) of e's actions or slot while NetPl_StepPart steps it, the same on the server and
 * its client (from the tick and part of the step and the draws before: a random relax or idle of the own character is
 * no correction); 0 when e is not stepped there now (the game's random then) */
byte NetPl_StepRand(const struct cEntity *e, int *r);
/* -mp2: e is a player's ship whose step touches the parking boxes (Parking.OnPark at fixed slots of its step, the
 * brake and the start of a parking predicted by its client; the box's own touch leaves it alone) */
byte NetPl_StepBrakes(const struct cEntity *e);
/* a parking box: an item with the parking model (on the server and as a replica alike) */
byte NetPl_IsParkBox(struct cEntity *e);
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
/* single player's flows: player i's own entity (its own ship) dies and the scripts delete it in its death
 * (Vehicles.OnDeath losing the hangar's ship): the instance's death of the player now, while the entity is there */
void Rep_OwnDied(int i);
/* the frame of a player's own entity (cWithLifeEntity_Update, in the world's entity loop where single player steps
 * its player; dt its frame time): the step Rep_PreFrame gave it for this frame, NetPl_Step of the tick's input; the
 * solo instance on a tape of single player's frames: the actions of the frame and the frame's slots */
void Rep_OwnFrame(struct cEntity *e, float dt);
/* -mp2: the ship e and another ship collided (cVehicleEntity_Touch): when e is a player's entity, its next rollback
 * state says so (NETPL_OF_TOUCHED) */
void Rep_OwnTouched(struct cEntity *e);
/* -mp2: the scripts set the place or the angles of e (RepEnt's variable tap): when it is a player's entity, its
 * client takes the next rollback state as a jump (a velocity they set is a world effect its prediction corrects) */
void Rep_OwnMoved(struct cEntity *e);
/* the lag compensation around the aim and hit check of an instant-hit weapon of the vehicle v (only for a
 * player's): the other replicated entities where v's client saw them, and back */
void Rep_RewindBegin(struct cEntity *v);
void Rep_RewindEnd(struct cEntity *v);
/* the damage of attacker's weapon may go to victim (outside the instance always, on a -mp2 client never but for
 * the own character's punch: counted "client.damage:<weapon>"): the server's PvP setting (Players_MayHurt); a hit of a
 * player is counted and logged ("hit by player") */
byte Rep_Hurt(struct cEntity *attacker, struct cEntity *victim, const char *weapon);
/* the hits of a ship's weapon its cVehicleAttack hands to the weapon's OnHit (the effects): count shots of weapon
 * from the point from to the point to (at target, NULL for a point), sent to the clients that see the ship */
void Rep_Shot(struct cEntity *shooter, int weapon, int count, const vec3_t *from, const vec3_t *to,
	struct cEntity *target);

/* ---- the client (Play.c) ---- */

extern byte g_Play;
/* a correction runs the own entity's ticks again */
extern byte g_PlayReplaying;
/* -mp2: a presentation sink reached in such a replay: counted ("pred.muted:<what>:<site>"), 1 when it is to be skipped
 * (the presentation of the own entity played at the prediction already) */
byte Play_Mute(const char *what, const char *site);

/* -headless: the frames of a headless player at PLAY_HEADLESS_HZ; 0 after a wait for an event up to the next one */
byte Play_HeadlessDue(void);

byte Play_ParseParam(const char *param, const char *ext);
/* the replication's events of the own slot (Watch.c) */
int Play_OwnSlot(void);
/* the client's player index on its instance, -1 */
int Play_Player(void);
void Play_SetOwn(int player, int slot, int kind, uint32_t tick, const float *phys, int nphys);
void Play_OwnCreate(int kind, const char *look, uint32_t tick, const NetRepState *st);
void Play_OwnUpdate(uint32_t tick, const NetRepState *st);
/* -mp2: the server's rollback state of the own entity (NETREP_MSG_OWN): compared with the prediction of its tick, a
 * difference beyond the tolerances goes back to it and runs the inputs after it again */
void Play_OwnState(const uint8_t *data, int size);
void Play_OwnRemove(void);
/* the server's own entity left the snapshot (a death, the scripts deleted it); -mp2: kept a moment (Play_Wreck) */
void Play_OwnGone(void);
/* the status message: the money, the alarm and the ammo of the own player into the world and the own ship (the
 * HUD of single player shows them) */
void Play_OwnStatus(const NetRepStatus *st);
struct cEntity *Play_Entity(void);
/* -mp2: e is the client's own predicted entity; the timed states a shot's hit sets on it (a countermeasure's cloud,
 * an EMI) are the server's there (its rollback state), a replica's hit leaves them */
byte Play_IsOwn(const struct cEntity *e);
/* -mp2: the own entity that died a moment ago, for the records of its death by the instance's name (the player message
 * of the death comes before them; the server's entity goes inactive, no replica of it may come), NULL */
struct cEntity *Play_Wreck(const char *name);
/* the server time (s) the own entity is predicted for now, 0 without one */
double Play_OwnTime(void);
/* the death camera looks at the place of the own death (no own entity yet) */
byte Play_DeathCamOn(void);
/* the level runs: the camera; each frame before and after App_Frame (Watch.c) */
void Play_Start(void);
/* the player's camera of view 0, made when missing (at x y z) */
struct cEntity *Play_ViewCamera(float x, float y, float z);
/* a scene of the server ended here (include/scrap/netscene.h): the camera, the HUD and the input of the own entity
 * again */
void Play_SceneEnd(void);
/* the own player's step of the frame after its input update (Watch_FrameInput): its controls read, the ticks the
 * clock passed stepped, the drawn place set */
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

/* stage 6b (src/game/App/PlayFlow.c): the flows of the scripts a -mp2 client starts for the eye at the press that
 * starts them on the server (possession: the button of the possession with a victim the own character can take; the
 * use with the arrow on a conversor's option Convert, an elevator's MoveUp/MoveDown/MoveTele or a metro door's
 * Travel): the possession's flash, the camera linked to the machine or elevator, the metro's map, the HUD of a movie
 * and no controls, as single player's scripts show them. The server's hand-over ends it (PlayFlow_Took: the server's
 * flash, the conversor's interface, the elevator's camera link, the metro's map message, a new own entity); a refusal
 * or none in 3 s gives it up (the camera, the controls and the HUD on foot again). Audit pred.flow:<kind>,
 * pred.flow.took:<kind>, pred.flow.cancel:<kind>. */
enum { PLAYFLOW_NONE, PLAYFLOW_POSS, PLAYFLOW_CONV, PLAYFLOW_ELEV, PLAYFLOW_METRO };
void PlayFlow_Press(struct cEntity *own, uint16_t pressed, double now);
void PlayFlow_Frame(double now);
int PlayFlow_Pending(void);
/* the machine, elevator or door of the started flow ("" none) */
const char *PlayFlow_Ent(void);
/* a fade record of the server: 1 when it is the started possession's flash already on the screen (not started again) */
byte PlayFlow_FadeShown(uint32_t from, uint32_t to);
/* the server's hand-over of a flow of the kind (ent: its machine, NULL any) */
void PlayFlow_Took(int kind, const char *ent);
void PlayFlow_Refused(void);
void PlayFlow_Reset(void);
/* the test verb "press <act|act2|act3|use|boost> [ticks]" of a client: the button held that many ticks (6), as a key */
byte Play_TestPress(const char *name, int ticks);
void Play_Close(void);

#endif
