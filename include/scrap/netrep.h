/*
 * NetRep: the replication of the world from a headless instance to its clients (stage M3a of the multiplayer plan),
 * the pure part without the engine (src/netrep, unit tests in src/netrep/test). The instance (src/game/App/Replicate.c)
 * fills one snapshot of the replicated entities per NETREP_SNAP_HZ, this code picks per client what to send and
 * encodes it; the watcher (src/game/App/Watch.c) decodes it into its proxies. Not in the original.
 *
 * Entities have a slot (the net id, 0..NETREP_MAX_ENTS-1) and a look (a string of how the client builds its proxy,
 * sent once per client). Every entity state is quantised (NetRepState) and kept for the last NETREP_HIST snapshots.
 * Per client and slot the server keeps the state the client acked (the base): a record is a delta against it, or
 * the full state when there is none (a create, with the look string while the client has not acked it) or the base
 * is too old. A client acks every message it took (its newest message number and a bit for each of the 32 before)
 * in its ack message (unreliable); an entity that leaves the client's interest or dies gets a remove record until a
 * remove newer than everything else sent of it is acked, only then the slot may be reused.
 *
 * Snapshot messages fit one unreliable Net2 message (NETREP_MSG_MAX); a snapshot is split into several messages, each
 * of them complete on its own, so a lost one costs only its records. Per client the interest is the entities in grid
 * cells within a radius of its view point; the records are ordered by priority (distance, size of the change, how
 * long they waited) and written until the byte budget of the snapshot (the client's rate) is used.
 *
 * Players (stage M3b): a client that says so in its hello gets a player (an entity of its own, include/scrap/players.h)
 * and is told its slot with a player message whenever it changes (a ship taken, left). Its ack messages carry its
 * input at NETREP_TICK_HZ: the inputs of the last NETREP_INPUT_REDUNDANCY ticks (a lost message costs nothing while
 * the next one comes in time), each for the server tick it is meant for, and the server time the client shows the
 * other entities at (for the lag compensation of the hits). The server keeps the inputs by tick (NetRepInBuf) and
 * runs each on its tick; late ones are dropped, a missing one repeats the input before. The client predicts its own
 * entity with the same movement code, keeps its predicted states by tick (NetRepPredHist) and goes back to the
 * server's state when a snapshot shows it went elsewhere, with the inputs after that tick run again. Its clock
 * (NetRepClock) follows the server ticks plus half the round trip.
 *
 * Wire format: little endian, varints are LEB128 (7 bits a byte), signed ones zigzag. Messages start with a type byte:
 *   'H' hello, client to server, reliable: version u16, rate u32 (bytes per second the client takes), flags u8
 *       (NETREP_HELLO_PLAY)
 *   'W' welcome, server to client, reliable: version u16, tick Hz u8, snapshot Hz u8, tick u32, view point 3 x i32,
 *       level (u8 length + text)
 *   'P' player, server to client, reliable: player u8, slot u16 (NETREP_NO_SLOT none), kind u8, tick u32 (the first
 *       tick of the slot as the player's), the movement constants of the entity the client's copy needs for the same
 *       movement (a ship's thrust, speed, friction, mass, radius; set by the scripts that made it): count u8, floats
 *       (u32 bits)
 *   'A' ack, client to server, unreliable: newest message u16, bits u32, view point 3 x i32 (whole units); a player
 *       adds: inputs u8, then with inputs the tick of the newest u32, the shown server time u32 (ms) and the inputs,
 *       newest first, each: ticks back from the one before u8 (0 for the newest), move 3 x i8, rot 2 x i16,
 *       buttons u16, weapon u8
 *   'S' snapshot, server to client, unreliable: message u16, snapshot u32, tick u32, records u16, records
 * A record: slot varint, op u8 (NETREP_OP_* in bits 0-1, NETREP_REC_LOOK bit 2), then for a remove nothing; else for a
 * delta the age of its base in snapshots (varint), for a create kind u8 and look varint (+ u8 length and the text with
 * NETREP_REC_LOOK), then the field mask (varint) and the fields of the mask (delta: against the base, full: against
 * zero): pos 3 svarints, ang 3 svarints (16 bit wrap), vel 3 svarints, dt varint, life svarint, flags u8,
 * ctrl 2 x i8, act0 and act1 varint action + varint time, view 2 svarints (16 bit wrap).
 */
#ifndef SCRAP_NETREP_H
#define SCRAP_NETREP_H

#include <stdint.h>

#define NETREP_VERSION 2
#define NETREP_TICK_HZ 30
#define NETREP_SNAP_HZ 20
#define NETREP_MAX_ENTS 2048		/* slots */
#define NETREP_MAX_LOOKS 1024
#define NETREP_LOOK_MAX 200			/* bytes of a look string without its zero */
#define NETREP_HIST 32				/* snapshots of states kept (server) and of bases (client) */
#define NETREP_FULL_AGE 24			/* a base this many snapshots old: full state instead of a delta */
#define NETREP_SENT 128				/* messages per client remembered for their acks */
#define NETREP_MSG_MAX 1159			/* NET2_MAX_UNRELIABLE */
#define NETREP_MSG_HEADER 13
#define NETREP_MAX_MSGS 8			/* per client and snapshot */
#define NETREP_MSG_OVERHEAD 40		/* bytes of a Net2 packet around a message: header, tag, chunk, acks */
#define NETREP_LAYERS 2				/* action layers of a character */

#define NETREP_POS_SCALE 4.0f		/* steps per unit (cm) */
#define NETREP_ANG_SCALE 10430.378f	/* steps per radian: 65536 a turn */

#define NETREP_RATE_MIN 4096
#define NETREP_RATE_MAX (1 << 20)
#define NETREP_RATE_DEFAULT 49152
#define NETREP_RADIUS 60000.0f		/* default interest radius (units) */
#define NETREP_CELL 8000.0f			/* grid cell */
#define NETREP_KEEP 1.15f			/* an entity stays in the interest up to this times the radius */

/* entity kinds */
#define NETREP_SHIP 1
#define NETREP_CHAR 2
#define NETREP_MISSILE 3			/* the projectiles of the weapons */

/* record ops */
#define NETREP_OP_DELTA 0
#define NETREP_OP_FULL 1
#define NETREP_OP_CREATE 2
#define NETREP_OP_REMOVE 3
#define NETREP_REC_LOOK 4

/* fields of the mask */
#define NETREP_F_POS 0x001
#define NETREP_F_ANG 0x002
#define NETREP_F_VEL 0x004
#define NETREP_F_DT 0x008
#define NETREP_F_LIFE 0x010
#define NETREP_F_FLAGS 0x020
#define NETREP_F_CTRL 0x040
#define NETREP_F_ACT0 0x080
#define NETREP_F_ACT1 0x100
#define NETREP_F_VIEW 0x200
#define NETREP_F_ALL 0x3ff

/* bits of NetRepState.flags */
#define NETREP_FL_ACTIVE 0x01
#define NETREP_FL_FALLING 0x02
#define NETREP_FL_WALKING 0x04
#define NETREP_FL_PLAYER 0x08		/* a player's entity */

/* message types */
#define NETREP_MSG_HELLO 'H'
#define NETREP_MSG_WELCOME 'W'
#define NETREP_MSG_ACK 'A'
#define NETREP_MSG_SNAP 'S'
#define NETREP_MSG_PLAYER 'P'

#define NETREP_HELLO_PLAY 0x01		/* the client wants a player */
#define NETREP_NO_SLOT 0xffff
#define NETREP_PHYS_MAX 16			/* movement constants of a player message */

/* the quantised state of an entity */
typedef struct NetRepState {
	int32_t pos[3];					/* 1 / NETREP_POS_SCALE units */
	int32_t vel[3];					/* units per second */
	uint16_t ang[3];				/* euler angles, 65536 a turn */
	uint16_t view[2];				/* character: the view angles x, y */
	int16_t life;
	uint8_t flags;					/* NETREP_FL_ */
	int8_t ctrl[2];					/* ship: thrust and turn input, -127..127 */
	uint16_t dt;					/* ms of the sample after the tick of its snapshot */
	uint16_t act[NETREP_LAYERS];	/* character: action of the layer + 1, 0 none */
	uint32_t actTime[NETREP_LAYERS];	/* ms in the action */
} NetRepState;

/* byte writer and reader; a writer that ran out of room or a reader that ran past the end sets bad */
typedef struct NetRepBuf {
	uint8_t *p;
	int size, cap;
	int bad;
} NetRepBuf;

typedef struct NetRepReader {
	const uint8_t *p;
	int size, pos;
	int bad;
} NetRepReader;

void NetRepBuf_Init(NetRepBuf *w, uint8_t *p, int cap);
void NetRep_PutU8(NetRepBuf *w, uint32_t v);
void NetRep_PutU16(NetRepBuf *w, uint32_t v);
void NetRep_PutU32(NetRepBuf *w, uint32_t v);
void NetRep_PutVar(NetRepBuf *w, uint32_t v);
void NetRep_PutSVar(NetRepBuf *w, int32_t v);
void NetRep_PutStr(NetRepBuf *w, const char *s);	/* u8 length (<= 255) and the bytes */
void NetRepReader_Init(NetRepReader *r, const uint8_t *p, int size);
uint32_t NetRep_GetU8(NetRepReader *r);
uint32_t NetRep_GetU16(NetRepReader *r);
uint32_t NetRep_GetU32(NetRepReader *r);
uint32_t NetRep_GetVar(NetRepReader *r);
int32_t NetRep_GetSVar(NetRepReader *r);
void NetRep_GetStr(NetRepReader *r, char *out, int cap);	/* zero terminated, cut to cap */

/* quantisation */
int32_t NetRep_QPos(float v);
float NetRep_UPos(int32_t q);
uint16_t NetRep_QAng(float rad);
float NetRep_UAng(uint16_t q);		/* -pi..pi */
int32_t NetRep_QVel(float v);
int8_t NetRep_QCtrl(float v);		/* -1..1 */

/* the fields that differ between two states */
uint32_t NetRep_Diff(const NetRepState *a, const NetRepState *b);
/* the fields of mask of s against base */
void NetRep_WriteFields(NetRepBuf *w, const NetRepState *base, const NetRepState *s, uint32_t mask);
/* out = base with the fields of mask read; 0 for a bad record */
int NetRep_ReadFields(NetRepReader *r, const NetRepState *base, NetRepState *out, uint32_t mask);

/* the interest grid: the slots by cell of x, z, buckets of a hash of the cell */
#define NETREP_GRID_BUCKETS 1024
typedef struct NetRepGrid {
	float cell;
	int16_t head[NETREP_GRID_BUCKETS];
	int16_t next[NETREP_MAX_ENTS];
	int32_t cx[NETREP_MAX_ENTS], cz[NETREP_MAX_ENTS];
} NetRepGrid;

void NetRepGrid_Clear(NetRepGrid *g, float cell);
void NetRepGrid_Add(NetRepGrid *g, int slot, float x, float z);
/* the slots in the cells the square around the circle touches (each once), up to max; returns the count */
int NetRepGrid_Query(const NetRepGrid *g, float x, float z, float radius, uint16_t *out, int max);

/* priority of a record: d distance to the view point, err the change against the base (units), op NETREP_OP_ */
float NetRep_Priority(float d, float radius, float err, int op);

typedef struct NetRepCand {
	float prio;
	uint16_t slot;
	uint8_t op;
} NetRepCand;
/* by priority, highest first, then by slot */
void NetRep_SortCands(NetRepCand *c, int n);

/* ---- server ---- */

typedef struct NetRepServer NetRepServer;
typedef struct NetRepClient NetRepClient;

typedef struct NetRepClientStats {
	uint64_t bytes;					/* snapshot messages incl. NETREP_MSG_OVERHEAD each */
	uint32_t snaps, msgs;
	uint32_t records, creates, removes, fulls;
	uint32_t maxSnapBytes, maxRecords;
	uint32_t skipped;				/* records that had to wait for the budget */
	uint32_t acked;					/* messages acked */
	uint32_t interest;				/* entities in the interest at the last snapshot */
} NetRepClientStats;

NetRepServer *NetRepSrv_Create(void);
void NetRepSrv_Destroy(NetRepServer *s);
/* the id of a look string (added when new), -1 when the table is full */
int NetRepSrv_Look(NetRepServer *s, const char *look);
/* a slot that no entity and no client holds, -1 when there is none */
int NetRepSrv_FreeSlot(const NetRepServer *s);
/* a new snapshot of tick: then NetRepSrv_Set for each live entity, then NetRepSrv_End */
uint32_t NetRepSrv_Begin(NetRepServer *s, uint32_t tick);
void NetRepSrv_Set(NetRepServer *s, int slot, int kind, int look, const NetRepState *st);
void NetRepSrv_End(NetRepServer *s);
int NetRepSrv_Live(const NetRepServer *s);	/* entities of the last snapshot */
int NetRepSrv_NumLooks(const NetRepServer *s);

NetRepClient *NetRepSrv_AddClient(NetRepServer *s, uint32_t rate);
void NetRepSrv_RemoveClient(NetRepServer *s, NetRepClient *c);
void NetRepSrv_SetView(NetRepClient *c, float x, float y, float z, float radius);
void NetRepSrv_Ack(NetRepServer *s, NetRepClient *c, uint16_t newest, uint32_t bits);
/* the messages of the snapshot for the client into msgs (each NETREP_MSG_MAX), their sizes in sizes; the count */
int NetRepSrv_Build(NetRepServer *s, NetRepClient *c, uint8_t msgs[][NETREP_MSG_MAX], int *sizes, int max);
const NetRepClientStats *NetRepSrv_Stats(const NetRepClient *c);
/* the client's own entity (a player's): sent first in every snapshot it changed in; NETREP_NO_SLOT none */
void NetRepSrv_SetOwn(NetRepClient *c, int slot);
/* the position of slot at server time t (s) from the snapshots kept, between the two samples around t as the
 * clients draw it (the oldest or newest sample outside them); 0 when the slot has no sample */
int NetRepSrv_PosAt(const NetRepServer *s, int slot, double t, float *pos);
/* the state the client acked of slot (NULL when it has none): tests */
const NetRepState *NetRepSrv_Base(const NetRepClient *c, int slot);

/* ---- client ---- */

typedef struct NetRepView NetRepView;

/* what a snapshot message does to the client's entities; tick and dt give the time of the sample */
typedef struct NetRepEvents {
	void *ctx;
	void (*create)(void *ctx, int slot, int kind, const char *look, uint32_t tick, const NetRepState *st);
	void (*update)(void *ctx, int slot, uint32_t tick, const NetRepState *st);
	void (*remove)(void *ctx, int slot);
} NetRepEvents;

NetRepView *NetRepCli_Create(void);
void NetRepCli_Destroy(NetRepView *v);
/* a snapshot message: 1 taken (acked), 0 bad or a record without its base (not acked) */
int NetRepCli_Message(NetRepView *v, const uint8_t *data, int size, const NetRepEvents *ev);
/* the ack to send: newest message taken and the bits of the 32 before it; 0 when nothing was taken yet */
int NetRepCli_Ack(const NetRepView *v, uint16_t *newest, uint32_t *bits);
/* the newest state of slot, NULL when the client has no such entity */
const NetRepState *NetRepCli_State(const NetRepView *v, int slot, int *kind, int *look);
const char *NetRepCli_LookString(const NetRepView *v, int look);
uint32_t NetRepCli_Tick(const NetRepView *v);	/* newest tick taken */

/* the small messages */
int NetRep_WriteHello(uint8_t *buf, int cap, uint32_t rate, int flags);
/* 0: not a hello of this version; flags NETREP_HELLO_ */
int NetRep_ReadHello(const uint8_t *data, int size, uint32_t *rate, int *flags);
int NetRep_WritePlayer(uint8_t *buf, int cap, int player, int slot, int kind, uint32_t tick, const float *phys,
	int nphys);
/* phys takes up to NETREP_PHYS_MAX constants, their count into *nphys */
int NetRep_ReadPlayer(const uint8_t *data, int size, int *player, int *slot, int *kind, uint32_t *tick, float *phys,
	int *nphys);
int NetRep_WriteWelcome(uint8_t *buf, int cap, uint32_t tick, const float *view, const char *level);
int NetRep_ReadWelcome(const uint8_t *data, int size, uint32_t *tick, float *view, char *level, int cap);
int NetRep_WriteAck(uint8_t *buf, int cap, uint16_t newest, uint32_t bits, const float *view);
int NetRep_ReadAck(const uint8_t *data, int size, uint16_t *newest, uint32_t *bits, float *view);

/* ---- input and prediction (stage M3b) ---- */

#define NETREP_INPUT_REDUNDANCY 4	/* inputs per ack message: the newest and the ones of the ticks before */
#define NETREP_INBUF 64				/* ticks of input the server keeps per player */
#define NETREP_INPUT_AHEAD 32		/* input more ticks ahead of the server than this is dropped */
#define NETREP_PRED 64				/* ticks of predicted states the client keeps */
#define NETREP_MOVE_SCALE 127.0f	/* steps of move for 1 */
#define NETREP_ROT_SCALE 1024.0f	/* steps of rot for 1 */

/* buttons of NetRepInput */
#define NETREP_IN_ACT 0x0001		/* character: the action; ship: fire */
#define NETREP_IN_ACT2 0x0002		/* character: possession; ship: countermeasure */
#define NETREP_IN_ACT3 0x0004		/* character: use */
#define NETREP_IN_LOCK 0x0008		/* character: the lock on the selection */
#define NETREP_IN_BOOST 0x0010		/* ship: boost */
#define NETREP_IN_SWITCH 0x0020		/* get into the nearest parked ship, out of the ship (on its press) */
#define NETREP_IN_FULL 0x0040		/* ship: the full thrust (its second accelerate key) */

/* the input of one tick: the controls of a character (walk, strafe, jump; the view turn) or a ship (acceleration,
 * strafe, vertical strafe; the turn) */
typedef struct NetRepInput {
	uint32_t tick;					/* the server tick it is meant for */
	int8_t move[3];
	int16_t rot[2];
	uint16_t buttons;
	uint8_t weapon;
} NetRepInput;

int8_t NetRep_QMove(float v);		/* -1..1 */
int16_t NetRep_QRot(float v);
/* an ack with the client's inputs, newest first (n <= NETREP_INPUT_REDUNDANCY), and the server time it shows the
 * other entities at (ms) */
int NetRep_WriteAckInput(uint8_t *buf, int cap, uint16_t newest, uint32_t bits, const float *view,
	const NetRepInput *in, int n, uint32_t shownMs);
/* the inputs of an ack (0 in *n for an ack without), up to max; 0 for a bad message */
int NetRep_ReadAckInput(const uint8_t *data, int size, uint16_t *newest, uint32_t *bits, float *view,
	NetRepInput *in, int max, int *n, uint32_t *shownMs);

/* the server's input of a player by tick */
typedef struct NetRepInBuf {
	NetRepInput in[NETREP_INBUF];	/* by tick % NETREP_INBUF */
	uint8_t have[NETREP_INBUF];		/* 1 waiting, 2 run with it */
	NetRepInput last;				/* the input of the last tick run */
	uint32_t ran;					/* the last tick run, 0 none */
	uint32_t got, late, dup, ahead, missed;	/* counters: taken, after their tick (that ran without it), again (a copy
											   of one taken), too far ahead, ticks run with the input before */
	int32_t leadSum;				/* ticks the inputs came early (sum and count of the taken ones) */
	uint32_t leadN;
} NetRepInBuf;

void NetRepInBuf_Init(NetRepInBuf *b);
/* an input that came while the server is at tick now (the last tick run): 1 taken, 0 late, again or too far ahead */
int NetRepInBuf_Put(NetRepInBuf *b, const NetRepInput *in, uint32_t now);
/* the input for tick: the one of that tick (1), else the last one again with its tick set (0); it is the last
 * input then */
int NetRepInBuf_Take(NetRepInBuf *b, uint32_t tick, NetRepInput *out);

/* the client's estimate of the server time: the ticks of the snapshots plus half the round trip */
typedef struct NetRepClock {
	double offset;					/* server time minus local time, s */
	int have;
	double jump;					/* a sample further off than this is taken at once, s */
	double gain;					/* the part of the difference taken per sample */
} NetRepClock;

void NetRepClock_Init(NetRepClock *c);
/* a snapshot of tick came at local time now (s) with the round trip rtt (s) */
void NetRepClock_Sample(NetRepClock *c, uint32_t tick, double now, double rtt);
double NetRepClock_Server(const NetRepClock *c, double now);
/* the tick the client's input of now is for: the server time when it gets there (half the round trip) plus lead s */
uint32_t NetRepClock_InputTick(const NetRepClock *c, double now, double rtt, double lead);

/* the predicted states of the client's own entity by tick: position, velocity and the view (euler angles of the
 * view, characters) or the angles (ships) */
typedef struct NetRepPred {
	uint32_t tick;
	float pos[3];
	float vel[3];
	float ang[3];
} NetRepPred;

typedef struct NetRepPredHist {
	NetRepPred p[NETREP_PRED];		/* by tick % NETREP_PRED */
	NetRepInput in[NETREP_PRED];	/* the input run on that tick */
} NetRepPredHist;

void NetRepPredHist_Init(NetRepPredHist *h);
void NetRepPredHist_Put(NetRepPredHist *h, const NetRepPred *p, const NetRepInput *in);
const NetRepPred *NetRepPredHist_Get(const NetRepPredHist *h, uint32_t tick);
const NetRepInput *NetRepPredHist_Input(const NetRepPredHist *h, uint32_t tick);
/* the distance between the server's position of tick and the predicted one; -1 when that tick is not kept */
float NetRepPredHist_Error(const NetRepPredHist *h, uint32_t tick, const float *serverPos);

/* the visual offset after a correction: added to the drawn position, it fades by half every halfLife s; jumps
 * larger than snap are not smoothed */
typedef struct NetRepSmooth {
	float off[3];
	float halfLife, snap;
} NetRepSmooth;

void NetRepSmooth_Init(NetRepSmooth *s, float halfLife, float snap);
/* the predicted position went from before to after: the offset keeps the drawn position where it was */
void NetRepSmooth_Correct(NetRepSmooth *s, const float *before, const float *after);
void NetRepSmooth_Step(NetRepSmooth *s, float dt);

#endif
