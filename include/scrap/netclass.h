/*
 * The network classification tables (stage 0.2 of the second multiplayer design): the tag of each engine API
 * function, entity variable, engine variable, script module, module variable, save variable prefix and level
 * init step. Made by tools/script/netclass.py from tools/script/netclass/<table>.tsv (the tags and their meaning are
 * in the head of each file), with src/game/Script/NetClassTable.c. Do not edit: run the tool after a .tsv
 * changes. The lookup is src/game/Script/NetClass.c.
 */
#ifndef SCRAP_NETCLASS_H
#define SCRAP_NETCLASS_H

#include <stdint.h>

/* the tags; NC_UNTAGGED: a row without one, NC_ABSENT: no row */
typedef enum {
	NC_ABSENT = -1,
	NC_UNTAGGED = 0,
	NC_SIM = 1,
	NC_DEF = 2,
	NC_PRES_STATE = 3,
	NC_PRES_EVENT = 4,
	NC_UI = 5,
	NC_QUERY = 6,
	NC_CLIENT = 7,
	NC_NET_OLD = 8,
	NC_STATE = 9,
	NC_EVENT = 10,
	NC_SERVER_ONLY = 11,
	NC_CLIENT_ONLY = 12,
	NC_WORLD_SETTING = 13,
	NC_SCENE = 14,
	NC_WORLD = 15,
	NC_PLAYER = 16,
	NC_PARTY = 17,
	NC_STORY = 18,
	NC_PLAYER_ENTER = 19,
	NC_NEAREST = 20,
	NC_ANY = 21,
	NC_PLAYER_SHARED = 22,
	NC_PARTY_SHARED = 23,
	NC_GROUP = 24,
	NC_TAGS
} netTag_t;

/* the tables */
typedef enum {
	NCT_API,			/* an engine API function: Module.Function */
	NCT_ENTVAR,			/* an entity variable */
	NCT_GVAR,			/* an engine variable scripts or data set */
	NCT_MODULE,			/* a script module: Module, or level/Module for a module of a level; Module.func for a function of a mixed module */
	NCT_MODVAR,			/* a script module variable: Module.name */
	NCT_SAVEVAR,			/* a save variable prefix (the name up to its first dot) */
	NCT_INITSTEP,			/* a step of the level start: File:call */
	NCT_PLAYERREAD,			/* a read of "the player" in the world context: C function or Module.variable */
	NCT_TABLES
} netTable_t;

typedef struct {
	const char *name;
	uint8_t tag;			/* netTag_t */
} netClassRow_t;

typedef struct {
	const char *name;
	const netClassRow_t *rows;	/* sorted as strcmp orders the names */
	int count;
} netClassTable_t;

extern const netClassTable_t g_NetClassTables[NCT_TABLES];

/* the tag of name in table t: the row of the name, else of the name with its digit runs as '#', each also as
 * the longest start with a '*' row ("AI_*"); NC_ABSENT without one */
netTag_t NetClass_Find(netTable_t t, const char *name);
/* the row of name in table t as NetClass_Find finds it, NULL */
const netClassRow_t *NetClass_Row(netTable_t t, const char *name);
/* the tag of a function of a script module in the module table: the row "Module.func", else "Module", else the
 * same forms of a level's module "level/Module.func", "level/Module" (level: the lower case name, NULL) */
netTag_t NetClass_ModuleTag(const char *module, const char *func, const char *level);
/* the name of a tag ("SIM"; "untagged", "absent") and of a table ("api") */
const char *NetClass_TagName(netTag_t tag);
const char *NetClass_TableName(netTable_t t);
/* the rows of table t without a tag */
int NetClass_Untagged(netTable_t t);

#endif
