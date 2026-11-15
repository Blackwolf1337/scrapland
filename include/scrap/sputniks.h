/*
 * The players' Sputniks of the sandbox and freeroam worlds (stage M12 of the multiplayer plan). Not in the original.
 *
 * Single player's Sputnik stands beside the player's own ship (Parking.PlayerShip) and is the way into its hangar
 * (SputnikInterface: the ships of Hangar.HangarShip1-9 and HangarShipAux, a destroyed one rebuilt for money). On an
 * instance of a sandbox or freeroam world (include/scrap/places.h) every player has its own: while the player's own
 * ship (include/scrap/players.h, Players_Ship) stands parked, the script's Sputnik.Assign puts that player's Sputnik
 * beside it (the module variable Sputnik.spt is kept per player and bound with the rest, World/Players.c); while the
 * player flies it, Sputnik is away. The player's use button on its own Sputnik opens the hangar screen on its client
 * (the same screen with the same save variables as the coop story's, include/scrap/coop.h); its answer comes back to
 * the instance: the money (never more than the player has), the hangar into the player's state (the keys
 * Hangar.HangarShip1-9, Hangar.HangarShipAux, Hangar.iHangarShip and Hangar.DestroyedShips of the state text,
 * include/scrap/coord.h, kept by the coordinator's store) and, for another hangar, that ship in place of the parked one
 * (it becomes the player's ship, the state key ship). The hangar fills with the ships the player boards (a new one
 * into the first empty hangar, else into the one it flew). A player that arrives on foot in an outdoor level finds its
 * own ship beside it, as single player's ship waits in the level's parking (Replicate.c).
 *
 * Wire format (the instance connection's reliable channel, beside the replication's messages):
 *   'J' SPUTNIKS_MSG, kind u8: SPUTNIKS_OPEN (instance -> client): money u32, the hangar's save variables (count u16,
 *       then name str, value u16 length + bytes each); SPUTNIKS_DONE (client -> instance): the same after the screen
 */
#ifndef SCRAP_SPUTNIKS_H
#define SCRAP_SPUTNIKS_H

#include "scrap/shared.h"

struct cEntity;
struct Net2Host;

#define SPUTNIKS_MSG 'J'
#define SPUTNIKS_OPEN 1
#define SPUTNIKS_DONE 2

/* ---- instance ---- */

/* an outdoor level of the city (Init.Outdoor: ships, parkings; not the indoor levels, whatever weapons they load) */
byte Sputniks_Outdoor(void);
/* once a tick after the players' inputs: each player's Sputnik beside its parked own ship, away while it flies */
void Sputniks_Frame(void);
/* player i used the arrow of t: 1 when t is a player's Sputnik (its own opens the hangar on its client) */
byte Sputniks_Use(int i, struct cEntity *t);
/* the coop story's one Sputnik (the scripts' Sputnik.spt, single player's) is used by the member that used it last:
 * Sputniks_Use makes the parked ship beside it that member's own ship (its Parking.PlayerShip), the scripts' Sputnik.
 * ChangeShip runs for it, and while Sputnik acts (its interface, the new ship) it acts for that member: its index for e
 * that Sputnik, else -1 */
int Sputniks_CoopUser(struct cEntity *e);
/* player i used that Sputnik a moment ago (its character, hidden by Sputnik's interface, is no lost one) */
byte Sputniks_CoopHolds(int i);
/* player i leaves the instance: its Sputnik goes */
void Sputniks_Left(int i);
/* player boarded a ship (its text as a hangar keeps it): into its hangar */
void Sputniks_Boarded(int player, const char *text);
/* a client's message (SPUTNIKS_MSG): 1 when taken */
byte Sputniks_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);

/* ---- client ---- */

/* the instance's message (SPUTNIKS_MSG): the hangar screen opens (test: SCRAP_PLAYBOT_SPUTNIK=<control> picks Exit,
 * HangarAux or Hangar1-9 of it at once) */
byte Sputniks_ClientMessage(const uint8_t *data, int size);
/* the hangar screen closed (SputnikInterface.ExitSputnikInterface): the hangar and the money to the instance */
void Sputniks_ClientDone(void);

#endif
