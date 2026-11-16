/*
 * The network audit (src/game/Global/NetAudit.c, stage 0.1 of the second multiplayer design). Not in the original.
 *
 * Counters at the choke points a networked game has to replicate or route: the sound, voice and music sinks, the
 * effects, the debris, the animation events, the HUD operations and texts, the fade and the cinema bars, the camera of
 * view 0, the action set, the time speed, the entity variables the scripts set, the reads of "the player" on an
 * instance and the reads of the render flag and of the camera distance by a headless process. Each site has a short
 * key ("snd.play", "entvar.set:Life", "player.read.world:Parking_Update"). The counts are always on and change
 * nothing the game does or writes: a static table, no allocation, no output. The test channel verb "audit" answers
 * them since the last "audit reset" ("audit json" for tools).
 */
#ifndef SCRAP_NETAUDIT_H
#define SCRAP_NETAUDIT_H

#include <stdint.h>

typedef enum {
	NA_SOUND,			/* the mixer sinks of the sound system (every play) */
	NA_SOUNDSRC,		/* the callers of the sinks that a network has to know (script sounds, positional sets) */
	NA_VOICE,			/* the voices of the conversations */
	NA_MUSIC,			/* the music player */
	NA_SFX,				/* the effects of the SFX module and the named effects of entities */
	NA_DEBRIS,			/* the debris systems */
	NA_ANIMEVENT,		/* the events of the actions (sounds, effects, script calls) */
	NA_HUD,				/* the scorer: items added, set, shown, hidden; the texts */
	NA_FADE,			/* the fades of the screen */
	NA_CINEMA,			/* the cinema bars */
	NA_CAMERA,			/* the camera of a view */
	NA_ACTIONSET,		/* the action set of the input */
	NA_TIMESPEED,		/* the speed of the world clock or of an entity's clock */
	NA_ENTVAR,			/* the entity variables the scripts set (Native_EntStore) */
	NA_PLAYER,			/* reads of "the player" on an instance, split by a bound player or none */
	NA_VISIBLE,			/* reads of the render flag and the camera distance on a headless process */
	NA_CREATE,			/* the replication v2 (include/scrap/repent.h): entities a client made itself and deleted,
						   variables without a tag */
	NA_CENSUS,			/* the samples played and the debris systems dropped by name (the census of the tests) */
	NA_CATS
} netAuditCat_t;

/* one count at a site: key a static string */
void NetAudit_Site(netAuditCat_t cat, const char *key);
/* one count of prefix (static) and name, both static strings (a __func__): kept by their addresses */
void NetAudit_Func(netAuditCat_t cat, const char *prefix, const char *func);
/* NetAudit_Func on a headless process (App_Headless: an instance, a dedicated server, a headless player) only */
void NetAudit_Headless(netAuditCat_t cat, const char *prefix, const char *func);
/* one count of prefix (static) and a name that may live on (a script's variable name): the name is copied into a
 * fixed table the first time, an overflow key "<prefix>?" counts the names that no longer fit */
void NetAudit_Name(netAuditCat_t cat, const char *prefix, const char *name);
/* the counts start again at 0 */
void NetAudit_Reset(void);
/* seconds of the app clock since the last reset (since the start without one) */
double NetAudit_Since(void);
/* the name of a category */
const char *NetAudit_CatName(netAuditCat_t cat);

/* the counts since the last reset, one key at a time (keys of the same text summed), sorted by count down, then by
 * key: calls fn for each with the key text; returns the number of keys */
typedef void (*netAuditFn_t)(void *ctx, netAuditCat_t cat, const char *key, uint32_t count);
int NetAudit_Each(netAuditFn_t fn, void *ctx);
/* the count of the keys that did not fit into the table (0 in a healthy run) */
uint32_t NetAudit_Lost(void);

#endif
