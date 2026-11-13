/*
 * Travel between the instances of a world and the client's side of a coordinator (stage M5 of the multiplayer plan,
 * include/scrap/coord.h). Not in the original.
 *
 * Instance (src/game/App/Travel.c), started by a coordinator:
 *   ScrapC -instance:<Level> ... -coord:<IPv4> -coordport:<n> -coordkey:<64 hex digits>   (token: COORD_TOKEN_ENV)
 * It keeps a Net2 connection to the coordinator (again and again while it is away; it ends after COORD_ORPHAN_SECS
 * without one), takes players only with a ticket of that coordinator (the message before the replication's hello),
 * sets up each from the ticket's state (money, alarm, owned ship, the player's save variables) at the ticket's
 * entrance (the spawn dummy of the level it comes from, the ride down in the elevator of the door it came through),
 * and reports the states to the coordinator (join, leave, every COORD_REPORT_SECS). A level change of a player (the
 * elevators' and the metro's MoveLevel, through Elevator.ElevatorReachUp) loads no level: Travel_Request hands the
 * player to the coordinator, which sends its client the ticket for the target instance. Other level changes of the
 * scripts are refused on an instance. The use of a metro door sends the player's client the station map
 * (COORD_MSG_METRO); the station it picks comes back (COORD_MSG_PICK) and the ride starts on the instance. The ship a
 * player takes becomes its owned ship (the text of a hangar save variable, key "ship"): a ship it calls in another
 * level is that one.
 *
 * Client (src/game/App/Join.c):
 *   ScrapC -join:<IPv4|IPv6> [-port:<coordinator port>] [-name:<name>] [-headless]     a world of a coordinator
 *   ScrapC -host[:<start level>] [-port:<n>] [-name:<name>] [-hostbind:<IPv4>]         hosts a world itself
 * Its identity key is player.key in the config folder (made on the first start; SCRAP_PLAYER_KEY=<file> another
 * one). It joins with a signature of the coordinator's challenge, gets a ticket and connects to that instance as a
 * player (include/scrap/play.h); a later ticket (a travel) closes the instance connection, loads the new level with
 * the loading screen and connects to the new instance. When the coordinator goes away the client keeps playing and
 * joins again (resume) when it is back. -host runs the coordinator in this game (its data in the config folder's
 * "host" folder, the instances as headless child processes of this game on loopback, -hostbind for players of other
 * machines) and joins it on 127.0.0.1; the levels start on demand and the start level is kept up (-hostkeep:<list>).
 * Tests: SCRAP_PLAYBOT_METRO=<station,...> makes a client pick the first station of the list the metro map offers.
 * The menus of the worlds (include/scrap/worldmenu.h) start the same join or host at run time (Join_Start,
 * Join_StartHost); Join_Stop ends either and goes back to the main menu, also when the instance and the coordinator
 * are both gone (or the coordinator refused the player) once the game played in the world.
 */
#ifndef SCRAP_TRAVEL_H
#define SCRAP_TRAVEL_H

#include "scrap/coord.h"
#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define TRAVEL_RETRY_MS 2000			/* the instance connects to its coordinator again this often */
#define TRAVEL_GIVEBACK_SECS 60			/* a player whose travel got no ticket in this time is given back */
#define TRAVEL_PENDING 16				/* tickets waiting for their hello */
#define TRAVEL_NONCES 512				/* tickets taken, kept until they expire */

struct cEntity;
struct Net2Host;

/* ---- instance ---- */

byte Travel_ParseParam(const char *param, const char *ext);
/* the instance has a coordinator (its players need tickets) */
byte Travel_Coordinated(void);
/* each tick: the coordinator connection, the reports, the players given back */
void Travel_InstancePass(uint64_t ms);
/* the players' leave states to the coordinator while the world still runs (the shutdown, before the world closes) */
void Travel_InstanceLeave(void);
void Travel_InstanceClose(void);
/* a message of a client on the instance's reliable channel: tickets, metro picks; 1 when taken */
byte Travel_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* the connection closed: its ticket goes */
void Travel_Drop(int conn);
/* the connection may say hello (it gave a valid ticket, or the instance has no coordinator) */
byte Travel_HelloOk(int conn);
/* where and how player (on conn) starts: its money, the spawn point (*have 0: the players' spawn ring); 0 refused */
typedef struct TravelSpawn {
	int money;
	byte have;
	vec3_t pos;
	float yaw;
} TravelSpawn;
byte Travel_Admit(int conn, int player, TravelSpawn *sp);
/* the player's entity is there: its alarm, the ride down of its entrance, the report */
void Travel_Joined(int player);
/* the player left (its connection closed): the report, unless it travelled */
void Travel_Left(int player);
/* the level change of the rider (an entity name) to level (a folder of Levels) with the spawn dummy and the door
 * class the original would use: 1 when it was a player's (handed to the coordinator) */
byte Travel_Request(const char *rider, const char *level, const char *entrance, const char *door);
/* the use of a metro door by an entity: 1 when it was a player's (its client gets the station map) */
byte Travel_MetroUse(const char *usr, const char *door);
/* the player took ship: its owned ship */
void Travel_Boarded(int player, struct cEntity *ship);
/* a parked ship of the player's owned ship at pos, NULL when it has none */
struct cEntity *Travel_OwnShip(int player, const vec3_t *pos, float yaw);
/* tests (the instance's standard input): a player uses an entity with an option of its arrow, goes next to it, or
 * goes next to it (dx, dz units away, facing it) */
void Travel_TestUse(int player, const char *entity, const char *option);
void Travel_TestGoto(int player, const char *entity, float dx, float dz);

/* the parts of the scripts: the metro's stations (Metro.c), the text of a ship as a hangar save variable keeps it
 * (ShipEdit.c), the ride down of a player of the instance in an elevator (Doors.c) */
int Metro_Stations(const char **names, int max);
byte ShipEdit_ShipText(struct cEntity *ship, char *out, int cap);
byte Elevator_Arrive(const char *usr, const char *elevator);

/* ---- client ---- */

byte Join_ParseParam(const char *param, const char *ext);
/* -join or -host: the watcher waits for a ticket */
byte Join_Active(void);
/* each frame before the watcher's; during level loads (the connections kept alive) */
void Join_PreFrame(void);
void Join_Idle(void);
void Join_Close(void);

/* the menus of the worlds (src/game/Script/MenuWorld.c): a join of the world at addr (the coordinator's port; the
 * world's identity key when the list server gave it, else NULL), a world hosted by this game, and the end of either:
 * the connections closed, the hosted world's instances stopped, the watcher and player gone, back to the main menu
 * when a level of the world ran (why: the text the menus show, NULL none; a headless player ends the game). The
 * state for the menus: JOIN_UI_*, why the text of a failure */
#define JOIN_UI_IDLE 0
#define JOIN_UI_WAIT 1					/* connecting or starting, no ticket yet */
#define JOIN_UI_PLAYING 2				/* a ticket came: the level of the world loads or runs */
#define JOIN_UI_FAILED 3
typedef struct JoinHostOpts {
	char level[COORD_LEVEL + 1];		/* the start level, kept up */
	char mode[16];						/* "" or "sandbox", "freeroam" (passed to the instances as -mode) */
	int pvp;							/* 0 or 1 */
	int maxPlayers;
	uint16_t port;
	byte listed;						/* registered with the list server of scrapdecomp.ini (listserver=) */
} JoinHostOpts;
byte Join_Start(const char *addr, uint16_t port, const char *name, const uint8_t *serverKey);
byte Join_StartHost(const JoinHostOpts *o, const char *name);
void Join_Stop(const char *why);
int Join_UiState(const char **why);
/* the console line "/coord <line>": a line for the coordinator of the world this game hosts (status, inst <level>
 * <line>, ...: Coord_Command) */
void Join_Command(const char *line);
void Join_UiClear(void);

/* the watcher's side (Watch.c): the instance of a ticket (a travel when one runs already), the station picked on
 * the metro map, the map of the instance's metro message; the end of everything of the instance (Join_Stop), whether
 * an instance connection is up, being made or about to be made */
void Watch_Go(const char *addr, uint16_t port, const uint8_t *ticket, int size, const char *level);
void Watch_Stop(void);
byte Watch_Alive(void);
byte Watch_MetroPick(const char *station);
void Watch_Idle(void);

#endif
