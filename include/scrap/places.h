/*
 * The places of the sandbox and freeroam modes that single player keeps closed (stage M9 of the multiplayer plan,
 * src/game/App/Places.c): the levels of the place table (include/scrap/placedef.h, data/places/places.def) run in
 * the instances of a sandbox or freeroam world with their characters and traffic and without their story, and the
 * city leads to them. Not in the original.
 *
 * On an instance of such a world (not a PvP mode, not a coop story) and on the clients of one (the coordinator's
 * ticket names the world's mode): no mission library of the level loads (Init.isMission is 0) and the intro scenes
 * of Fake and ABIndoor do not play. The instance adds the characters, the traffic and the police of the table before
 * the level makes them (Places_LevelStatus, after the InitStatus of the level's modules); both sides give the doors of
 * the table their levels and make the gates (Places_LevelInit, after the Init of the level's modules). A gate is a use
 * point (PlaceGate_<name>): its arrow's options are the levels it leads to; the use button on it hands the player to
 * the coordinator for that level, where it arrives at the gate named in the option (Places_Entrance). The players'
 * spawn points of a level can come from the table (Places_Spawns).
 */
#ifndef SCRAP_PLACES_H
#define SCRAP_PLACES_H

#include "scrap/mathlib.h"
#include "scrap/shared.h"

struct cEntity;
struct NetRepInput;

/* -mode of the instance's command line (looked at, never taken: 0) */
byte Places_ParseParam(const char *param, const char *ext);
/* a client: the mode of the world its ticket came from ("" sandbox) */
void Places_WorldMode(const char *mode);
/* an instance of a sandbox or freeroam world, or a client in one */
byte Places_On(void);
/* the level's start: the characters, the traffic and the police of the table (an instance; 0 with the error of the
 * script), then the doors and the gates (both sides) */
byte Places_LevelStatus(void);
byte Places_LevelInit(void);
/* instance: the use button of player i on a gate (taken from the input) */
void Places_Input(int i, struct cEntity *e, struct NetRepInput *in);
/* tests: player uses the gate t with an option: 1 when t is a gate */
byte Places_TestUse(int player, struct cEntity *t, int option);
/* the arrival at a gate's entrance (PlaceGate_<name>): 1 with the position */
byte Places_Entrance(const char *entrance, vec3_t *pos, float *yaw);
/* the players' spawn points of the table for this level (at most max): their number, 0 for the level's own */
int Places_Spawns(vec3_t *pos, float *yaw, int max);
/* tests: "dummies [<pattern>]", "ents [<pattern>]", "census", "nearest <player> char|car" (the player next to it) */
byte Places_Command(const char *line);

#endif
