/*
 * The place table of the sandbox and freeroam modes (stage M9 of the multiplayer plan): the levels single player keeps
 * closed (the Orbit, Fake, the Junkyard, AB and ABIndoor, the super deal arenas and the small multiplayer arenas) and
 * the ways to them from the city, read from data/places/places.def (built into the game and the coordinator). Pure:
 * no engine (src/coord/PlaceDef.c, unit test in coord_test). The game's side is src/game/App/Places.c
 * (include/scrap/places.h). Not in the original.
 *
 * A level's lines (all optional):
 *   place <Level> [listed]         the lines of a level start; listed: a place of the world lists (coordinator, menus)
 *   link <door> <Level>[,<Level>]  an elevator or teleporter of the level gets these levels as its options
 *   gate <name> <pos> to <Level>[:<gate>] ...   a door new to the level: a use point at the position whose options
 *                                  travel to those levels (at that gate of the level, at a dummy of it when the name
 *                                  starts with DM_, else at its players' spawn)
 *   npc <Name>                     a character at the level's dummy DM_Mission_<Name> (its class: Name to the first _)
 *   npc <Class>_<id> <pos> [fixed] a character of that class at the position (fixed: it stays where it is)
 *   repos <Class> <n>              n characters of the class walking between the DM_NPC_Repos_<Class>_ dummies
 *   traffic <n> [<model>,...]      ships of the free traffic (and their models)
 *   rail <light> <medium> <heavy>  ships of the rail traffic of each weight
 *   intraffic <light> <medium> <heavy>   the indoor traffic behind the windows
 *   police on|off                  the police waves outdoors
 *   spawn <pos>                    a spawn point of the players (the level's own when there is none)
 *   nouse <entity>                 an entity of the level nobody can use (the ship editor's keypad)
 *   ai <step>                      a level without a graph of the AI gets one made from its geometry when it starts:
 *                                  the free space around its DM_NodeIniAI dummies in a grid of that step (units)
 * A position <pos> is "<dummy|-> <x> <y> <z> <yaw|*>": with a dummy of the level the numbers are added to its
 * position and * is its yaw; with - they are the position and the yaw (degrees).
 */
#ifndef SCRAP_PLACEDEF_H
#define SCRAP_PLACEDEF_H

#define PLACE_NAME 48					/* bytes of a level, gate, dummy or entity name */
#define PLACE_LEVELS 40
#define PLACE_LINKS 4
#define PLACE_GATES 4
#define PLACE_OPTS 12					/* options of a gate */
#define PLACE_NPCS 24
#define PLACE_REPOS 6
#define PLACE_SPAWNS 16
#define PLACE_NOUSE 4
#define PLACE_LIST 256					/* bytes of a comma list */

typedef struct PlacePos {
	char dummy[PLACE_NAME];			/* "" absolute */
	float x, y, z;
	float yaw;						/* degrees */
	int dummyYaw;					/* the dummy's yaw (*) */
} PlacePos;

typedef struct PlaceLink {
	char door[PLACE_NAME];
	char levels[PLACE_LIST];		/* comma separated */
} PlaceLink;

typedef struct PlaceGate {
	char name[PLACE_NAME];
	PlacePos pos;
	int nopt;
	struct {
		char level[PLACE_NAME];
		char gate[PLACE_NAME];		/* "" the players' spawn of the level */
	} opt[PLACE_OPTS];
} PlaceGate;

typedef struct PlaceNpc {
	char name[PLACE_NAME];			/* <Name> of DM_Mission_<Name>, or <Class>_<id> */
	int mission;					/* at its DM_Mission_ dummy */
	int fixed;
	PlacePos pos;
} PlaceNpc;

typedef struct PlaceLevel {
	char level[PLACE_NAME];
	int listed;
	int nlinks, ngates, nnpcs, nrepos, nspawns, nnouse;
	PlaceLink link[PLACE_LINKS];
	PlaceGate gate[PLACE_GATES];
	PlaceNpc npc[PLACE_NPCS];
	struct {
		char cls[PLACE_NAME];
		int n;
	} repos[PLACE_REPOS];
	PlacePos spawn[PLACE_SPAWNS];
	char nouse[PLACE_NOUSE][PLACE_NAME];
	int traffic;					/* -1 the level's */
	char ships[PLACE_LIST];			/* "" the level's */
	int rail[3];					/* -1 the level's */
	int intraffic[3];				/* -1 none */
	int police;						/* -1 the level's, 0 off, 1 on */
	float aiStep;					/* the grid of the AI graph made at the start, 0 none */
} PlaceLevel;

typedef struct PlaceTable {
	int n;
	PlaceLevel lv[PLACE_LEVELS];
} PlaceTable;

/* text into t: 1, or 0 with the line and the reason in err */
int PlaceDef_Parse(const char *text, PlaceTable *t, char *err, int cap);
/* the built-in table (data/places/places.def), parsed at the first call; an error of it gives an empty table */
const PlaceTable *PlaceDef_Table(void);
/* the lines of a level (case-insensitive), NULL */
const PlaceLevel *PlaceDef_Level(const PlaceTable *t, const char *level);
/* a gate of a level, NULL */
const PlaceGate *PlaceDef_Gate(const PlaceLevel *l, const char *name);
/* the listed places, comma separated, into out ("" none); 1 when all fit */
int PlaceDef_Places(const PlaceTable *t, char *out, int cap);
/* the levels of a sandbox or freeroam world: the city (COORD_CITY) and the listed places of the built-in table */
const char *PlaceDef_WorldLevels(void);

#endif
