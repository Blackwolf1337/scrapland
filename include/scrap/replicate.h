/*
 * The replication of the instance's world to watching clients (stage M3a of the multiplayer plan, the engine side of
 * include/scrap/netrep.h): src/game/App/Replicate.c on the instance, src/game/App/Watch.c in the watcher. Not in the
 * original.
 *
 * Instance: the ships ("Car": traffic, police, parked, the startup player's) and the characters ("WalkChar": NPCs and
 * the player) get a slot; at NETREP_SNAP_HZ their states go into a snapshot and each watcher gets its messages. A
 * connection becomes a watcher with a hello on the reliable channel 0 and gets the welcome (level, tick, a view
 * point); its acks and view point come unreliable (channel 2), the snapshots go unreliable (channel 2).
 *
 * Watcher: ScrapC -watch:<IPv4> [-port:<n>] (or -watch:"<address>:<port>") [-watchrate:<bytes a second>], optional
 * -imap:Levels/<level> to load the
 * server's level at once (else the menu loads first and the level of the welcome after it). The level runs without
 * the single player setup (no player, traffic, police, startup characters; InitOutMap of OutMap.c); characters and
 * ships the level scripts still make are deleted, the replicated ones are proxies (cObjEntity.proxy) that the
 * snapshots place, behind the server's time by the interpolation delay (NetRepDelay), interpolated. A free camera
 * ("WatchCam", an FCam with FreeCam on) flies with the input, starts above the view point of the welcome and takes
 * its camPos and camAng variables (the test channel: e = Scrap.GetEntity("WatchCam"); e.camPos = (x, y, z)). Its
 * position is the view point.
 *
 * Test hooks: SCRAP_REPLOG=<file> writes the positions of the replicated entities as CSV, on the instance every tick
 * (time of the sample, slot, x, y, z, the count of entities the slot had), in the watcher every frame from the first
 * sample of a proxy on (server time shown, slot, x, y, z).
 */
#ifndef SCRAP_REPLICATE_H
#define SCRAP_REPLICATE_H

#include "scrap/mathlib.h"
#include "scrap/net2.h"
#include "scrap/netrep.h"
#include "scrap/shared.h"

#define REP_CH_RELIABLE 0
#define REP_CH_UNRELIABLE 2
/* client -> instance, reliable, no fields: the client's own rail ship (Rep_LocalAmbient) died by its player's hand */
#define REP_MSG_RAILKILL 'R'
/* instance -> client, reliable: the level's things the instance made at its start (items, machines, doors, places;
 * not replicated otherwise) once the client's level runs, so it can make the ones its own start did not; then their
 * changes. REP_MSG_STATICS: u8 count, per record name, class, model, action class (strings), position (QPos, signed
 * varints), angles (QAng, u16 each), u8 active. REP_MSG_STATICSET: u8 count, per record name, u8 REP_STATIC_* */
#define REP_MSG_STATICS 'Z'
#define REP_RECORD_NAME 64			/* the names in them, with the end */
#define REP_MSG_STATICSET 'X'
#define REP_STATIC_OFF 0
#define REP_STATIC_ON 1
#define REP_STATIC_GONE 2
/* the flags of the welcome (NetRep_WriteWelcome): the world has none of the single player setup (the PvP modes) */
#define REP_WELCOME_ARENA 0x01

/* ---- both ---- */

/* an entity every game makes for itself around its own camera, which the instance does not replicate and a watcher
 * keeps (the rail traffic: the lists NormalTraffic, MediumTraffic and HeavyTraffic place their ships around the
 * camera, so the headless instance has them all off) */
byte Rep_LocalAmbient(struct cEntity *e);

/* ---- instance (Replicate.c) ---- */

/* the Net2 events of a connection; Rep_Message returns 1 when the message was the replication's */
void Rep_Connect(Net2Host *host, int conn);
void Rep_Disconnect(int conn);
byte Rep_Message(Net2Host *host, int conn, int channel, const uint8_t *data, int size);
byte Rep_IsWatcher(int conn);
/* after the frame of tick: the snapshot when it is due, the messages to the watchers */
void Rep_Tick(Net2Host *host, uint64_t tick);
/* the status lines: per watcher bytes per second, snapshot sizes, records, interest; time spent */
void Rep_Status(double secs);
void Rep_Close(void);
/* the level of the instance has started (its first pass): the items it made then are every game's, the ones made
 * later are replicated (NETREP_ITEM) */
void Rep_Open(void);
/* the players of the instance for the travel (include/scrap/travel.h): the connection of player i (-1), the player
 * of a connection (-1), player i back at the players' spawn ring */
int Rep_PlayerConn(int i);
int Rep_ConnPlayer(int conn);
void Rep_Respawn(int i);
/* player i's character was put somewhere else (the test line goto): a drop from there is no fall from its spawn point
 * (REP_SPAWN_FALL) */
void Rep_Placed(int i);
/* player i's entity gone without a death (a ship of the PvP modes is deleted, include/scrap/arena.h): no new one
 * before the mode or the respawn time gives it */
void Rep_Remove(int i);
/* player i's character again (its class of now, include/scrap/faction.h) where it stands, or at pos (out of its ship
 * then; a dead player comes back there: a revive); pos NULL and the player in a ship or dead: nothing */
void Rep_Rebody(int i, const vec3_t *pos, float yaw);
/* the names of the players' characters on an instance */
#define REP_PLAYER_PREFIX "NetPlayer"
/* the replication slot of an entity of the last snapshot, -1 */
int Rep_SlotOf(const struct cEntity *e);
/* a free place for a character around at (the place k of the rings around it on, 0 at itself): 1 with *out */
byte Rep_FreeSpot(const vec3_t *at, int first, vec3_t *out);
/* tests: the switch of player i (into the nearest parked ship or out of its ship), or a ship called beside it */
void Rep_TestSwitch(int i, byte call);

/* ---- watcher (Watch.c) ---- */

extern byte g_Watch;

/* -watch and -watchrate of the command line; 1 when param is one of them */
byte Watch_ParseParam(const char *param, const char *ext);
/* before and after App_Frame in the main loop of a watcher */
void Watch_PreFrame(void);
void Watch_PostFrame(void);
void Watch_Close(void);
/* an entity of a look string (S|model|pilot|motor 0..3|class or C|class) at the position of st: a proxy, or the
 * player's own entity (include/scrap/play.h) */
struct cEntity *Watch_Build(const char *name, int kind, const char *look, const NetRepState *st, byte proxy);
/* the proxies at server time t for the own prediction of a tick (the players' and the other characters within
 * WATCH_AHEAD_RADIUS of the own entity, or only the players' with players), then back where they are drawn */
void Watch_ProxiesAt(double t, byte players);
void Watch_ProxiesBack(void);
/* the client's own entity of a slot it no longer drives becomes the proxy of that slot (a dead player's wreck stays
 * in view: the death camera follows it) */
void Watch_Adopt(int slot, struct cEntity *e, int kind);
/* the proxy is a player's entity */
byte Watch_IsPlayer(struct cEntity *e);
/* the entity of a replication slot (the own entity or a proxy), NULL */
struct cEntity *Watch_SlotEntity(int slot);
/* a message to the instance on the reliable channel while the level runs: 1 sent */
byte Watch_SendReliable(const uint8_t *data, int size);
/* the clock of the server (include/scrap/netrep.h, NetRepClock) runs; the time (s) of the tick an input of now is
 * for; the server time the proxies are drawn at (s) */
byte Watch_ClockReady(void);
double Watch_InputTime(double now, double rtt, double lead);
double Watch_ShownTime(void);
/* the welcome said the world has no single player setup (REP_WELCOME_ARENA); 0 before the welcome */
byte Watch_ArenaWorld(void);
/* the own player killed a rail ship of this game (Traffic.RailTrafficDeath): told to the instance (REP_MSG_RAILKILL) */
void Watch_RailKill(void);

#endif
