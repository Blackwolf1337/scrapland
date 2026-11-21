/*
 * The coop party as "the player" of the scripts (stage 5 of the second multiplayer design, src/game/App/Party.c).
 * Not in the original.
 *
 * In coop the world's context of an instance is the party: a script or engine site that asks for "the player" there
 * gets its answer from the party's rule of the site (Players_ResolveAt, include/scrap/players.h, asks Party_Tag):
 *   PARTY  a getter: the acting member (the current player, else the mission's member, Coop_MissionPlayer), else the
 *          member nearest to the entity that runs
 *   MENU   a menu or a dialog: the acting member, never a nearest one
 *   DEAD   "is the player dead": a living member first (the acting one when it lives), so "dead" means every member
 *          dead
 *   ALL    a setter: the site runs once for every member, each in its context (Party_All); a read outside that gives
 *          the acting member
 *   NEAREST, ANY, WORLD as outside coop (the netclass table playerread)
 * The rule of a site is its row of tools/script/netclass/playerread.tsv, unless data/coop/rules.def has a line
 * "read <site> <rule> [<mission>]" for it (with a mission: only while the story's Mission.Library is that one). A
 * site without either is counted as player.world.norule (the gate of stage 5: none in the coop chapters) and taken as
 * PARTY. Outside coop PARTY, MENU, DEAD and ALL rows are NEAREST and Party_All runs its function once.
 *
 * data/coop/rules.def (embedded) holds the other rules of the coop story too, one place: SceneSkip (NetScene.c), the
 * lists of the story's variables that are a member's own or the clients' (local, hud, clientnot, entry), the pilot of
 * the boarding callbacks, the restart time of a failed mission, the time of a super deal's scores and the gather
 * distance after a scene.
 */
#ifndef SCRAP_PARTY_H
#define SCRAP_PARTY_H

#include "scrap/netclass.h"
#include "scrap/shared.h"

/* the value of the rule line "<key> <value>" of rules.def, def without one */
const char *Party_Rule(const char *key, const char *def);
double Party_RuleNum(const char *key, double def);
/* a rule line "<key> ... <word> ..." has word after its key (the lists: local Map Spawn ...) */
byte Party_RuleHas(const char *key, const char *word);
/* the words of the first rule line of key (a list), their count into *n; NULL without one */
const char *const *Party_RuleList(const char *key, int *n);

/* the rule of a read of "the player" at site in coop's world context: rules.def's read line (the story's mission
 * first), else tag (the playerread row, NC_ABSENT without one) */
netTag_t Party_Tag(const char *site, netTag_t tag);
/* the acting member (the current player, else the mission's member), else -1 */
int Party_Acting(void);
/* a living member (prefer first when it lives), else prefer */
int Party_Living(int prefer);

/* fn(ctx) once for every member of the party in its context when coop's world context runs it (the rule ALL);
 * else once in the current context; the AND of the results */
typedef byte (*partyFn_t)(void *ctx);
byte Party_AllAt(partyFn_t fn, void *ctx, const char *site);
#define Party_All(fn, ctx) Party_AllAt(fn, ctx, __func__)

#endif
