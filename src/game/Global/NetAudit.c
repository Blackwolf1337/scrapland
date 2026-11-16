/*
 * The network audit: counts by site key, see include/scrap/netaudit.h. Not in the original.
 *
 * One open addressing table of NA_SLOTS entries. A site of static strings is found by the addresses of its strings
 * (the same text at two addresses gives two entries, summed when they are listed); a name that may not live on is
 * found by its text and copied into a fixed pool. Nothing is allocated and nothing is written; the game's output stays
 * what it was.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/game.h"
#include "scrap/global.h"

#include "scrap/netaudit.h"

#define NA_SLOTS 4096			/* a power of 2 */
#define NA_FILL (NA_SLOTS * 3 / 4)	/* entries at most, the rest keeps the probes short */
#define NA_POOL 0x10000			/* bytes of copied names */
#define NA_KEY 128				/* the longest key text listed */

typedef struct {
	const char *prefix;			/* NULL: a free slot */
	const char *name;			/* NULL for a plain site key */
	uint32_t count;
	uint8_t cat;
	uint8_t byText;				/* name was copied and is compared by its text */
} naEntry;

static naEntry na_table[NA_SLOTS];
static int na_used;
static char na_pool[NA_POOL];
static int na_poolUsed;
static uint32_t na_lost;
static double na_resetTime;		/* App_GetTime at the last reset */

static const char *const na_catNames[NA_CATS] = {
	"sound", "soundsrc", "voice", "music", "sfx", "debris", "animevent", "hud", "fade", "cinema", "camera",
	"actionset", "timespeed", "entvar", "player", "visible", "create", "predict",
};

const char *NetAudit_CatName(netAuditCat_t cat) {
	return (unsigned)cat < NA_CATS ? na_catNames[cat] : "?";
}

static uint32_t NA_HashPtr(const void *a, const void *b) {
	uint64_t h = (uint64_t)(uintptr_t)a * 0x9e3779b97f4a7c15ull ^ (uint64_t)(uintptr_t)b * 0xc2b2ae3d27d4eb4full;
	return (uint32_t)(h >> 32);
}

static uint32_t NA_HashText(const void *prefix, const char *s) {
	uint32_t h = 2166136261u;
	for (; *s; s++) {
		h = (h ^ (uint8_t)*s) * 16777619u;
	}
	return NA_HashPtr(prefix, (const void *)(uintptr_t)h);
}

/* the slot of the entry, a new one when it is not there (NULL: the table is full) */
static naEntry *NA_Find(netAuditCat_t cat, const char *prefix, const char *name, byte byText) {
	uint32_t h = byText ? NA_HashText(prefix, name) : NA_HashPtr(prefix, name);

	for (uint32_t k = 0; k < NA_SLOTS; k++) {
		naEntry *e = &na_table[(h + k) & (NA_SLOTS - 1)];
		if (!e->prefix) {
			if (na_used >= NA_FILL) {
				return NULL;
			}
			if (byText) {
				int n = (int)strlen(name) + 1;
				if (na_poolUsed + n > NA_POOL) {
					return NULL;
				}
				memcpy(na_pool + na_poolUsed, name, (size_t)n);
				name = na_pool + na_poolUsed;
				na_poolUsed += n;
			}
			e->prefix = prefix;
			e->name = name;
			e->cat = (uint8_t)cat;
			e->byText = byText;
			na_used++;
			return e;
		}
		if (e->prefix != prefix || e->byText != byText || e->cat != (uint8_t)cat) {
			continue;
		}
		if (byText ? !strcmp(e->name, name) : e->name == name) {
			return e;
		}
	}
	return NULL;
}

static void NA_Count(netAuditCat_t cat, const char *prefix, const char *name, byte byText) {
	naEntry *e = NA_Find(cat, prefix, name, byText);

	if (!e && name) {
		/* the names that no longer fit: "<prefix>?" */
		e = NA_Find(cat, prefix, "?", 0);
	}
	if (e) {
		e->count++;
	} else {
		na_lost++;
	}
}

void NetAudit_Site(netAuditCat_t cat, const char *key) {
	NA_Count(cat, key, NULL, 0);
}

void NetAudit_Func(netAuditCat_t cat, const char *prefix, const char *func) {
	NA_Count(cat, prefix, func ? func : "?", 0);
}

void NetAudit_Headless(netAuditCat_t cat, const char *prefix, const char *func) {
	if (App_Headless()) {
		NA_Count(cat, prefix, func ? func : "?", 0);
	}
}

void NetAudit_Name(netAuditCat_t cat, const char *prefix, const char *name) {
	NA_Count(cat, prefix, name ? name : "?", name != NULL);
}

void NetAudit_Reset(void) {
	for (int i = 0; i < NA_SLOTS; i++) {
		na_table[i].count = 0;
	}
	na_lost = 0;
	na_resetTime = g_pApp ? App_GetTime(g_pApp) : 0.0;
}

double NetAudit_Since(void) {
	return g_pApp ? App_GetTime(g_pApp) - na_resetTime : 0.0;
}

uint32_t NetAudit_Lost(void) {
	return na_lost;
}

/* ---- the list ---- */

typedef struct {
	char key[NA_KEY];
	uint32_t count;
	uint8_t cat;
} naRow;

static naRow na_rows[NA_FILL];

static int NA_ByKey(const void *a, const void *b) {
	const naRow *x = a, *y = b;
	int c = strcmp(x->key, y->key);
	return c ? c : (int)x->cat - (int)y->cat;
}

static int NA_ByCount(const void *a, const void *b) {
	const naRow *x = a, *y = b;
	if (x->count != y->count) {
		return x->count > y->count ? -1 : 1;
	}
	return NA_ByKey(a, b);
}

int NetAudit_Each(netAuditFn_t fn, void *ctx) {
	int n = 0;

	for (int i = 0; i < NA_SLOTS && n < NA_FILL; i++) {
		const naEntry *e = &na_table[i];
		if (!e->prefix || !e->count) {
			continue;
		}
		naRow *r = &na_rows[n++];
		Str_Copy(r->key, e->prefix, NA_KEY);
		if (e->name) {
			size_t len = strlen(r->key);
			Str_Copy(r->key + len, e->name, NA_KEY - (int)len);
		}
		r->count = e->count;
		r->cat = e->cat;
	}
	qsort(na_rows, (size_t)n, sizeof(*na_rows), NA_ByKey);
	/* the same text at two addresses: one row */
	int m = 0;
	for (int i = 0; i < n; i++) {
		if (m && na_rows[m - 1].cat == na_rows[i].cat && !strcmp(na_rows[m - 1].key, na_rows[i].key)) {
			na_rows[m - 1].count += na_rows[i].count;
		} else if (m != i) {
			na_rows[m++] = na_rows[i];
		} else {
			m++;
		}
	}
	qsort(na_rows, (size_t)m, sizeof(*na_rows), NA_ByCount);
	for (int i = 0; i < m; i++) {
		fn(ctx, (netAuditCat_t)na_rows[i].cat, na_rows[i].key, na_rows[i].count);
	}
	return m;
}
