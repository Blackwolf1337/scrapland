/*
 * The coop party as "the player" of the scripts and the rules of the coop story (data/coop/rules.def), see
 * include/scrap/party.h. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coop.h"
#include "scrap/crt.h"
#include "scrap/party.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"

#define PARTY_LINES 96				/* rule lines */
#define PARTY_WORDS 16				/* words of a line after its key */
#define PARTY_TEXT 4096				/* bytes of the words of all lines */

static const char party_def[] = {
#embed "../../../data/coop/rules.def"
	, 0 };

static struct {
	byte read;						/* the lines were read */
	int nlines;
	struct {
		const char *key;
		const char *word[PARTY_WORDS];
		int nwords;
	} line[PARTY_LINES];
	char text[PARTY_TEXT];
	int used;
} party;

static const char *Party_Word(const char *s, int len) {
	if (party.used + len + 1 > PARTY_TEXT) {
		return NULL;
	}
	char *w = party.text + party.used;
	memcpy(w, s, (size_t)len);
	w[len] = 0;
	party.used += len + 1;
	return w;
}

/* the lines of rules.def into words once; a word after '#' ends the line */
static void Party_Read(void) {
	if (party.read) {
		return;
	}
	party.read = 1;
	for (const char *p = party_def; *p && party.nlines < PARTY_LINES;) {
		const char *eol = strchr(p, '\n');
		const char *end = eol ? eol : p + strlen(p);
		int n = -1;
		while (p < end) {
			while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) {
				p++;
			}
			const char *w = p;
			while (p < end && *p != ' ' && *p != '\t' && *p != '\r') {
				p++;
			}
			if (p == w || *w == '#') {
				break;
			}
			const char *word = Party_Word(w, (int)(p - w));
			if (!word) {
				break;
			}
			if (n < 0) {
				party.line[party.nlines].key = word;
				party.line[party.nlines].nwords = 0;
				n = 0;
			} else if (n < PARTY_WORDS) {
				party.line[party.nlines].word[n++] = word;
				party.line[party.nlines].nwords = n;
			}
		}
		if (n >= 0) {
			party.nlines++;
		}
		p = eol ? eol + 1 : end;
	}
}

const char *Party_Rule(const char *key, const char *def) {
	const char *any = NULL, *mission = NULL;
	Party_Read();
	for (int i = party.nlines - 1; i >= 0; i--) {
		if (strcmp(party.line[i].key, key) || !party.line[i].nwords) {
			continue;
		}
		if (party.line[i].nwords < 2) {
			any = any ? any : party.line[i].word[0];
			continue;
		}
		/* "<key> <value> <mission>": the value while the story's Mission.Library is that mission */
		if (!mission) {
			mission = SaveVar_Get("Mission.Library");
			mission = mission ? mission : "";
		}
		if (!strcmp(party.line[i].word[1], mission)) {
			return party.line[i].word[0];
		}
	}
	return any ? any : def;
}

double Party_RuleNum(const char *key, double def) {
	const char *v = Party_Rule(key, NULL);
	return v ? atof(v) : def;
}

byte Party_RuleHas(const char *key, const char *word) {
	Party_Read();
	for (int i = 0; i < party.nlines; i++) {
		if (!strcmp(party.line[i].key, key)) {
			for (int k = 0; k < party.line[i].nwords; k++) {
				if (!strcmp(party.line[i].word[k], word)) {
					return 1;
				}
			}
		}
	}
	return 0;
}

const char *const *Party_RuleList(const char *key, int *n) {
	Party_Read();
	for (int i = 0; i < party.nlines; i++) {
		if (!strcmp(party.line[i].key, key)) {
			*n = party.line[i].nwords;
			return party.line[i].word;
		}
	}
	*n = 0;
	return NULL;
}

static netTag_t Party_TagName(const char *name) {
	static const struct {
		const char *name;
		netTag_t tag;
	} tags[] = { { "PARTY", NC_PARTY }, { "MENU", NC_MENU }, { "DEAD", NC_DEAD }, { "ALL", NC_ALL },
		{ "NEAREST", NC_NEAREST }, { "ANY", NC_ANY }, { "WORLD", NC_WORLD } };
	for (size_t k = 0; k < sizeof(tags) / sizeof(tags[0]); k++) {
		if (!crt_stricmp(tags[k].name, name)) {
			return tags[k].tag;
		}
	}
	return NC_ABSENT;
}

netTag_t Party_Tag(const char *site, netTag_t tag) {
	Party_Read();
	const char *mission = NULL;
	netTag_t any = NC_ABSENT;
	for (int i = 0; i < party.nlines; i++) {
		if (strcmp(party.line[i].key, "read") || party.line[i].nwords < 2 || strcmp(party.line[i].word[0], site)) {
			continue;
		}
		if (party.line[i].nwords < 3) {
			any = Party_TagName(party.line[i].word[1]);
			continue;
		}
		if (!mission) {
			mission = SaveVar_Get("Mission.Library");
			mission = mission ? mission : "";
		}
		if (!strcmp(party.line[i].word[2], mission)) {
			return Party_TagName(party.line[i].word[1]);
		}
	}
	return any != NC_ABSENT ? any : tag;
}

int Party_Acting(void) {
	int i = Players_Current();
	if (i != PLAYERS_NONE) {
		return i;
	}
	return Coop_MissionPlayer();
}

static byte Party_Lives(int i) {
	return i >= 0 && i < PLAYERS_MAX && Rep_PlayerConn(i) >= 0 && !Rep_PlayerDead(i) && Players_Get(i);
}

int Party_Living(int prefer) {
	if (Party_Lives(prefer)) {
		return prefer;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Party_Lives(i)) {
			return i;
		}
	}
	return prefer;
}

byte Party_AllAt(partyFn_t fn, void *ctx, const char *site) {
	if (!Coop_On() || !Players_Net() || Players_Current() != PLAYERS_NONE) {
		return fn(ctx);
	}
	byte ok = 1;
	int n = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) < 0 || !Players_Get(i)) {
			continue;
		}
		int was = Players_EnterAt(i, site);
		ok = fn(ctx) && ok;
		Players_Leave(was);
		n++;
	}
	/* no member here (the level's start before the first one comes): once in the world's context */
	return n ? ok : fn(ctx);
}
