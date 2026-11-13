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
 * Wire format: little endian, varints are LEB128 (7 bits a byte), signed ones zigzag. Messages start with a type byte:
 *   'H' hello, client to server, reliable: version u16, rate u32 (bytes per second the client takes)
 *   'W' welcome, server to client, reliable: version u16, tick Hz u8, snapshot Hz u8, tick u32, view point 3 x i32,
 *       level (u8 length + text)
 *   'A' ack, client to server, unreliable: newest message u16, bits u32, view point 3 x i32 (whole units)
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

#define NETREP_VERSION 1
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

/* message types */
#define NETREP_MSG_HELLO 'H'
#define NETREP_MSG_WELCOME 'W'
#define NETREP_MSG_ACK 'A'
#define NETREP_MSG_SNAP 'S'

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

NetRepClient *NetRepSrv_AddClient(NetRepServer *s, uint32_t rate);
void NetRepSrv_RemoveClient(NetRepServer *s, NetRepClient *c);
void NetRepSrv_SetView(NetRepClient *c, float x, float y, float z, float radius);
void NetRepSrv_Ack(NetRepServer *s, NetRepClient *c, uint16_t newest, uint32_t bits);
/* the messages of the snapshot for the client into msgs (each NETREP_MSG_MAX), their sizes in sizes; the count */
int NetRepSrv_Build(NetRepServer *s, NetRepClient *c, uint8_t msgs[][NETREP_MSG_MAX], int *sizes, int max);
const NetRepClientStats *NetRepSrv_Stats(const NetRepClient *c);
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
int NetRep_WriteHello(uint8_t *buf, int cap, uint32_t rate);
int NetRep_ReadHello(const uint8_t *data, int size, uint32_t *rate);	/* 0: not a hello of this version */
int NetRep_WriteWelcome(uint8_t *buf, int cap, uint32_t tick, const float *view, const char *level);
int NetRep_ReadWelcome(const uint8_t *data, int size, uint32_t *tick, float *view, char *level, int cap);
int NetRep_WriteAck(uint8_t *buf, int cap, uint16_t newest, uint32_t bits, const float *view);
int NetRep_ReadAck(const uint8_t *data, int size, uint16_t *newest, uint32_t *bits, float *view);

#endif
