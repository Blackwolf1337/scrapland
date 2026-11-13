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

#include "scrap/net2.h"
#include "scrap/netrep.h"
#include "scrap/shared.h"

#define REP_CH_RELIABLE 0
#define REP_CH_UNRELIABLE 2

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
/* the players of the instance for the travel (include/scrap/travel.h): the connection of player i (-1), the player
 * of a connection (-1), player i back at the players' spawn ring */
int Rep_PlayerConn(int i);
int Rep_ConnPlayer(int conn);
void Rep_Respawn(int i);
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
/* the other players' proxies at server time t for the own prediction of a tick, then back where they are drawn */
void Watch_PlayersAt(double t);
void Watch_PlayersBack(void);
/* the proxy is a player's entity */
byte Watch_IsPlayer(struct cEntity *e);
/* the clock of the server (include/scrap/netrep.h, NetRepClock) runs; the tick of an input of now; the server time
 * the proxies are drawn at (s) */
byte Watch_ClockReady(void);
uint32_t Watch_InputTick(double now, double rtt, double lead);
double Watch_ShownTime(void);

#endif
