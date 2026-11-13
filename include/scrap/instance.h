/*
 * The headless instance (stage M2a of the multiplayer plan, src/game/App/Instance.c): one level's world, scripts, AI,
 * traffic, police and characters run as in single player, with no window, no render device, no sound device, no
 * input and no menu, at a fixed server tick, with a Net2 server (include/scrap/net2.h) on its port. Not in the
 * original.
 *
 *   ScrapC -instance:DownTown [-port:28100] [-id:dt1] [-bind:127.0.0.1] [-maxplayers:64]
 *
 * The level is a folder of Levels (or a path "Levels/X"). The ticks follow the real clock; the app time (the frame
 * time of CApp_Frame) goes on by exactly one tick per tick, so the world always steps 1/INSTANCE_TICK_HZ s. A tick
 * that is late runs at once; more than INSTANCE_MAX_BEHIND ticks late (a level load, a stop of the process) the
 * backlog is dropped. Between ticks the main loop waits for an SDL event (a quit) up to the next tick.
 *
 * The Net2 server answers server queries (name = id, map = level, mode "instance", players = open connections). A
 * connection that sends the hello of the replication is a watcher (include/scrap/replicate.h); messages of other
 * (test) connections come back on their channel (an echo), and they are dropped after INSTANCE_TEST_HOLD s with
 * reason INSTANCE_DR_NOGAME.
 *
 * The end: the line "quit" on the standard input (a pipe or a terminal; other lines go to the console as typed
 * there, "status" prints the state), the file instance_<id>.quit in the game folder (checked once a second, removed
 * when seen), or SIGINT/SIGTERM through SDL's quit event. The configuration is the game folder's Scrap.cfg (as the
 * dedicated server reads it) and is never saved, and no saved game is written, so several instances share a game
 * folder.
 */
#ifndef SCRAP_INSTANCE_H
#define SCRAP_INSTANCE_H

#include "scrap/shared.h"

#define INSTANCE_TICK_HZ 30
#define INSTANCE_MAX_BEHIND 8			/* ticks */
#define INSTANCE_APP_VERSION 1			/* Net2 appVersion of the game's multiplayer */
#define INSTANCE_MAX_PLAYERS 64
#define INSTANCE_TEST_HOLD 30			/* s a test connection stays */
#define INSTANCE_DR_NOGAME 64			/* NET2_DR_APP: the instance has no game for the client yet */
#define INSTANCE_STATUS_SECS 60			/* s between the status lines in the output */
#define INSTANCE_POLL_MS 4				/* the sockets are read this often between the ticks */
#define INSTANCE_CONN_BUFFER (16 * 1024)	/* Net2 inbox and outbox per connection */

/* -port, -id, -bind and -maxplayers of the command line; 1 when param is one of them */
byte Instance_ParseParam(const char *param, const char *ext);
/* one pass of the main loop after the init: the tick when it is due, else a wait for an event up to it */
void Instance_Pass(void);
/* -port of the command line (the watcher's server port too), NET2_DEFAULT_PORT without one */
uint16_t Instance_Port(void);
/* the app time: seconds of the ticks run */
double Instance_Time(void);
/* the Net2 server closed (App shutdown) */
void Instance_Close(void);
/* tests: SCRAP_NET2_LOSS (percent), SCRAP_NET2_DELAY and SCRAP_NET2_JITTER (ms) make the host's sends a test link
 * (Net2_SetTestLink); the instance's and a watcher's or player's host */
struct Net2Host;
void Instance_TestLink(struct Net2Host *host, const char *who);

#endif
