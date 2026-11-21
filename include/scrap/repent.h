/*
 * The replication v2 of the second multiplayer design (stage 3.2), behind the switch -mp2 on the instance and the
 * client (the coordinator of a -mp2 host passes it to its instances). Not in the original.
 *
 * Every entity of every class but the cameras gets a slot (src/game/App/RepEnt.c on the instance): the moving ones
 * (ships, characters, missiles) in the interest grid, the others (objects, things with life, items, plain entities)
 * in the level scope that every client gets. The snapshot state of src/netrep carries a class block
 * (NetRepState.x, the REPX_ values below): the four action layers, the walk speed, the head's view lock, maximum life,
 * burn, boost, cloud, EMI, the fire of the weapon, the scripted move, radar, render effect, the item's state, a
 * missile's target and level. The look of the create record tells how the client builds the entity (the first
 * replication's looks for ships, characters, missiles and items, "O|model|action class" for the others).
 *
 * Entity variables: the inline Entity_Var functions (include/scrap/entity.h) call a tap on every set that took, the
 * scripts' sets (Native_EntStore) and the C code's alike. By the netclass table (tools/script/netclass/entvar.tsv) a
 * STATE variable goes into the entity's journal (the last values in the order of the last sets, NetRepJournal), an
 * EVENT variable is sent as an event, the rest stays on the server. The journal is the create message: a client
 * gets it on the reliable channel with the name and class when the snapshots create the slot for it, then the
 * changes (once per snapshot, the changed entries in journal order) and the events.
 *
 * The client (src/game/App/ReplicaEnt.c) creates a replica (cObjEntity.replica: only the Present half runs) with the
 * server's name and class once it has both the create record and the create message of the same generation
 * (REPX_GEN), applies the journal through the same Var functions, places it from the snapshots (Watch.c) and the
 * class block. Its level start runs only the DEF steps (include/scrap/initsteps.h, stage 3.3) and no simulation
 * script runs on it: the audit counts what it still makes itself ("client.script.create:"; "client.rail.create:" for
 * the rail traffic's modules, 0 since stage 2.2) and every call of a SIM module ("client.sim.call:"). On a level without a client part yet
 * it runs the whole start and deletes what that made ("client.purge:"). The own player's entity keeps the
 * prediction path (Play.c) and gets the journal too.
 *
 * Wire (reliable, channel 0, server to client): REPENT_MSG, a kind u8, then
 *   REPENT_K_CREATE  records until the end: slot varint, generation varint, class, name (u8 length + text), count
 *                    varint, the journal values (NetRep_PutJVal)
 *   REPENT_K_VARS    records: slot varint, generation varint, count varint, the changed values in journal order
 *   REPENT_K_EVENT   records: slot varint, generation varint, a value
 *
 * Test verb "mirror json" (both sides): every replicated entity with slot, generation, class, name, the declared
 * state (the state the server would send of it now, made by the same function on both sides), its journal, and the
 * value each journal variable reads back from the entity; the client adds its view point and own slot.
 * tools/test/parity/mirror.py diffs a server's mirror against a client's.
 */
#ifndef SCRAP_REPENT_H
#define SCRAP_REPENT_H

#include "scrap/net2.h"
#include "scrap/netrep.h"
#include "scrap/shared.h"

struct cEntity;
struct tcBuf;

/* the kinds of the v2 slots beyond the first replication's (NETREP_SHIP, NETREP_CHAR, NETREP_MISSILE, NETREP_ITEM) */
#define REPENT_OBJECT 5				/* an object (Obj) */
#define REPENT_LIFE 6				/* a thing with life that is no ship, character or missile (doors, machines) */
#define REPENT_BASE 7				/* a plain entity */

/* the class block */
enum {
	REPX_GEN,						/* the generation of the slot's entity: create message and record must match */
	REPX_ACT2, REPX_ACT2T,			/* action layer 2: action + 1, ms in it */
	REPX_ACT3, REPX_ACT3T,			/* layer 3 */
	REPX_WALK,						/* the walk speed of the actions, 1/1000 */
	REPX_HEAD,						/* character: the view lock its head turn blends to (the blend is Present) */
	REPX_MAXLIFE,
	REPX_PREVLIFE,					/* the life of the last check (a replica's hit flash) */
	REPX_BURN,						/* fire left, ms */
	REPX_RADAR,						/* RadarShow */
	REPX_FX,						/* the render effect of the model */
	REPX_FXP,						/* its parameter 0 (the float's bits) */
	REPX_BOOST,						/* ship: boost left, 1/1000 */
	REPX_CLOUD,						/* ship: the cloud counter, ms, and its switch << 30 */
	REPX_EMI,						/* ship: EMI left, ms */
	REPX_FIREW,						/* ship: bit w for the weapon w firing, bit 30 the Hook firing, bit 31 the
									 * weapons' lock (hook plug) */
	REPX_MOVE,						/* the scripted move: on << 30 | ms done */
	REPX_MOVET,						/* its length, ms */
	REPX_ITEM,						/* item: taken, ms left (0 there); ship: REPX_WEAPSND */
	REPX_BLINK,						/* item: blink */
	REPX_TARGET,					/* missile: the slot of its target + 1 */
	REPX_LEVEL,						/* missile: its upgrade level */
	REPX_ARROW,						/* the arrow: can use, view, mission (bits 0..2), the view cone's status (off,
									 * relax, alert, danger: bits 3..4, the radar's cones and a sentinel's light)
									 * and the target type << 8 */
	REPX_COUNT
};
/* ship: the sound item of the firing loop + 1, 0 none (cWeaponManager.weapSnd: a replica runs no weapon update) */
#define REPX_WEAPSND REPX_ITEM

/* journal entries of no entity variable (a name no Var function knows): the embedded effect type the scripts set
 * (SFX.EmbeddedSet, SetFX, a character's named effect; the client applies it with Entity_SetEmbeddedFX) */
#define REPENT_J_FX "@FX"

#define REPENT_MSG 'N'
#define REPENT_K_CREATE 'c'
#define REPENT_K_VARS 'v'
#define REPENT_K_EVENT 'e'

/* -mp2 on the command line */
extern byte g_Mp2;

/* ---- both (RepEnt.c) ---- */

/* -mp2: 1 when param is it */
byte RepEnt_ParseParam(const char *param, const char *ext);
/* the kind of an entity's slot, 0 for an entity that is not replicated (a camera, a deleted one) */
int RepEnt_Kind(struct cEntity *e);
/* the declared state of e of that kind (the snapshot's), its generation in x[REPX_GEN] left 0 */
void RepEnt_State(struct cEntity *e, int kind, NetRepState *s);
/* the test verb "mirror [json]" */
void RepEnt_Mirror(struct tcBuf *b, const char *arg);
/* one entity of the mirror (n before it): its declared state, the journal of owner in j, read back */
void RepEnt_MirrorEnt(struct tcBuf *b, int n, int slot, uint32_t gen, struct cEntity *e, const NetRepJournal *j,
	int owner);

/* ---- instance (RepEnt.c) ---- */

/* the snapshot: every entity into the server (NetRepSrv_Begin and End around it) */
void RepEnt_Gather(NetRepServer *srv, uint32_t tick);
/* the slot of an entity, -1 */
int RepEnt_Slot(const struct cEntity *e);
/* a journal entry of e that is no entity variable (REPENT_J_FX) set to text */
void RepEnt_Note(struct cEntity *e, const char *name, const char *text);
/* the destructor of an entity with a slot */
void RepEnt_Gone(struct cEntity *e);
/* before the snapshot messages: the changed journal entries and the events to the clients that have the entities */
void RepEnt_Flush(Net2Host *host, int conns);
/* after the snapshot messages of a connection: the create messages of the slots its records created */
void RepEnt_Sent(Net2Host *host, int conn, NetRepClient *c);
/* a connection is gone */
void RepEnt_Drop(int conn);
void RepEnt_Status(void);

/* ---- client (ReplicaEnt.c) ---- */

/* a REPENT_MSG message */
void ReplicaEnt_Message(const uint8_t *data, int size);
/* the snapshots created (or recreated) a slot / removed it; an update of a slot (a new generation recreates it) */
void ReplicaEnt_Want(int slot);
void ReplicaEnt_Unwant(int slot);
void ReplicaEnt_Update(int slot, const NetRepState *st);
/* each frame before the proxies are placed: what the level and the scripts made away, the replicas whose create
 * record and message are both there made */
void ReplicaEnt_Frame(void);
/* a replica placed from its samples a and b (span s apart, drawn at part f of the way), dt s after the frame before:
 * the class block, the action layers */
void ReplicaEnt_Present(struct cEntity *e, int kind, const NetRepState *a, const NetRepState *b, float span, float f,
	float dt);
/* the journal of a slot into an entity (the own entity of the prediction) */
void ReplicaEnt_Journal(int slot, struct cEntity *e);
/* the server gave the client the replica of slot as its own entity (Play_SetOwn): the own entity made from it with the
 * slot's journal, as the replication's creation */
void ReplicaEnt_OwnCreate(int slot, int kind, const char *look, uint32_t tick, const NetRepState *st);
/* cWorld_CreateEntity on a -mp2 client: counted unless the replication made it */
void ReplicaEnt_Created(const char *name, const char *type);
/* Port_Entry on a -mp2 client: a call of module.func counted when the module table tags it SIM
 * ("client.sim.call:Module.func"; the client runs no simulation script), as "client.rail.call:" when it is a call of
 * the rail traffic's modules (Traffic, VehiclesNPC and InTraffic, and what they call: the server's since stage 2.2,
 * so a count shows a call the client should not make);
 * ReplicaEnt_ModLeave when the call returns */
void ReplicaEnt_ModCall(const char *module, const char *func);
void ReplicaEnt_ModLeave(const char *module);
/* the replication makes an entity or sets a journal value into one now (a replica or the own entity) */
byte ReplicaEnt_Busy(void);
/* the connection is gone (a travel): everything of it */
void ReplicaEnt_Reset(void);
/* the entity of the instance's name: its replica, or the own entity for the name of the own slot; NULL */
struct cEntity *ReplicaEnt_Find(const char *name);
/* the slot of a replica, -1 */
int ReplicaEnt_SlotOf(const struct cEntity *e);
/* the client's side of "mirror json" */
void ReplicaEnt_Mirror(struct tcBuf *b);

#endif
