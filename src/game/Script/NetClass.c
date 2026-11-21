/*
 * The lookup of the network classification tables (include/scrap/netclass.h, made by tools/script/netclass.py): a
 * binary search in the sorted rows of a table. A name that has no row of its own is looked up again with its digit
 * runs as '#' ("snd03_vol0" as "snd#_vol#"), and each form also as the longest start that has a '*' row ("AI_*").
 * Plain C with no engine calls, so the unit test (tools/script/netclass_test.c) links it alone.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/netclass.h"

#define NC_NAME 128			/* longer names are only looked up as they are */

static const char *const nc_tagNames[NC_TAGS] = {
	"untagged", "SIM", "DEF", "PRES_STATE", "PRES_EVENT", "UI", "QUERY", "CLIENT", "NET_OLD", "STATE", "EVENT",
	"SERVER_ONLY", "CLIENT_ONLY", "WORLD_SETTING", "SCENE", "WORLD", "PLAYER", "PARTY", "STORY", "PLAYER_ENTER",
	"NEAREST", "ANY", "PLAYER_SHARED", "PARTY_SHARED", "GROUP", "MENU", "DEAD", "ALL",
};

static const netClassRow_t *NC_Search(const netClassTable_t *t, const char *name) {
	int lo = 0, hi = t->count - 1;

	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		int c = strcmp(t->rows[mid].name, name);
		if (!c) {
			return &t->rows[mid];
		}
		if (c < 0) {
			lo = mid + 1;
		} else {
			hi = mid - 1;
		}
	}
	return NULL;
}

/* the row of name itself, else of its longest start with a '*' row */
static const netClassRow_t *NC_Family(const netClassTable_t *t, const char *name) {
	char key[NC_NAME + 1];
	const netClassRow_t *r = NC_Search(t, name);
	size_t n = strlen(name);

	if (r || n >= NC_NAME) {
		return r;
	}
	memcpy(key, name, n);
	for (size_t k = n; k > 0; k--) {
		key[k] = '*';
		key[k + 1] = 0;
		if ((r = NC_Search(t, key))) {
			return r;
		}
	}
	return NULL;
}

const netClassRow_t *NetClass_Row(netTable_t t, const char *name) {
	char norm[NC_NAME];
	int n = 0, digits = 0, run = 0;

	if ((unsigned)t >= NCT_TABLES || !name) {
		return NULL;
	}
	const netClassTable_t *tab = &g_NetClassTables[t];
	const netClassRow_t *r = NC_Family(tab, name);
	if (r) {
		return r;
	}
	/* the digit runs as '#' */
	for (const char *s = name; *s && n < NC_NAME - 1; s++) {
		int digit = *s >= '0' && *s <= '9';
		if (!digit) {
			norm[n++] = *s;
		} else if (!run) {
			norm[n++] = '#';
		}
		run = digit;
		digits |= digit;
	}
	norm[n] = 0;
	return digits && strlen(name) < NC_NAME ? NC_Family(tab, norm) : NULL;
}

netTag_t NetClass_Find(netTable_t t, const char *name) {
	const netClassRow_t *r = NetClass_Row(t, name);
	return r ? (netTag_t)r->tag : NC_ABSENT;
}

netTag_t NetClass_ModuleTag(const char *module, const char *func, const char *level) {
	char key[4][NC_NAME];
	int n = 2;

	snprintf(key[0], NC_NAME, "%s.%s", module, func);
	snprintf(key[1], NC_NAME, "%s", module);
	if (level && level[0]) {
		snprintf(key[2], NC_NAME, "%s/%s.%s", level, module, func);
		snprintf(key[3], NC_NAME, "%s/%s", level, module);
		n = 4;
	}
	for (int i = 0; i < n; i++) {
		netTag_t tag = NetClass_Find(NCT_MODULE, key[i]);
		if (tag != NC_ABSENT) {
			return tag;
		}
	}
	return NC_ABSENT;
}

const char *NetClass_TagName(netTag_t tag) {
	if (tag == NC_ABSENT) {
		return "absent";
	}
	return (unsigned)tag < NC_TAGS ? nc_tagNames[tag] : "?";
}

const char *NetClass_TableName(netTable_t t) {
	return (unsigned)t < NCT_TABLES ? g_NetClassTables[t].name : "?";
}

int NetClass_Untagged(netTable_t t) {
	int n = 0;

	if ((unsigned)t >= NCT_TABLES) {
		return 0;
	}
	for (int i = 0; i < g_NetClassTables[t].count; i++) {
		n += g_NetClassTables[t].rows[i].tag == NC_UNTAGGED;
	}
	return n;
}
