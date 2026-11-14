/*
 * The place table of the sandbox and freeroam modes, see include/scrap/placedef.h. Pure: no engine. Not in the
 * original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coord.h"
#include "scrap/placedef.h"

#define PD_TOKENS 40
#define PD_LINE 512

static const char pd_text[] = {
#embed "../../data/places/places.def"
	, 0 };

static int Pd_Lower(int c) {
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int Pd_Eq(const char *a, const char *b) {
	while (*a && Pd_Lower((unsigned char)*a) == Pd_Lower((unsigned char)*b)) {
		a++;
		b++;
	}
	return !*a && !*b;
}

/* s into out (cap bytes): 0 when it does not fit */
static int Pd_Copy(char *out, const char *s, size_t cap) {
	size_t n = strlen(s);
	if (n >= cap) {
		return 0;
	}
	memcpy(out, s, n + 1);
	return 1;
}

static int Pd_Err(char *err, int cap, int line, const char *why, const char *what) {
	if (err && cap > 0) {
		snprintf(err, (size_t)cap, "line %d: %s%s%s", line, why, what ? " " : "", what ? what : "");
	}
	return 0;
}

/* a number token: 1 when all of it is a number */
static int Pd_Num(const char *s, float *out) {
	char *end;
	double d = strtod(s, &end);
	if (end == s || *end) {
		return 0;
	}
	*out = (float)d;
	return 1;
}

static int Pd_Int(const char *s, int *out) {
	float f;
	if (!Pd_Num(s, &f) || f < 0.0f || f > 100000.0f || f != (float)(int)f) {
		return 0;
	}
	*out = (int)f;
	return 1;
}

/* the five tokens of a position */
static int Pd_Pos(char **tok, PlacePos *p) {
	memset(p, 0, sizeof(*p));
	if (strcmp(tok[0], "-") && !Pd_Copy(p->dummy, tok[0], sizeof(p->dummy))) {
		return 0;
	}
	if (!Pd_Num(tok[1], &p->x) || !Pd_Num(tok[2], &p->y) || !Pd_Num(tok[3], &p->z)) {
		return 0;
	}
	if (!strcmp(tok[4], "*")) {
		p->dummyYaw = 1;
		return p->dummy[0] != 0;
	}
	return Pd_Num(tok[4], &p->yaw);
}

/* the three counts of the traffic of each weight */
static int Pd_Three(char **tok, int n, int out[3]) {
	return n == 4 && Pd_Int(tok[1], &out[0]) && Pd_Int(tok[2], &out[1]) && Pd_Int(tok[3], &out[2]);
}

static int Pd_Line(PlaceTable *t, PlaceLevel *l, char **tok, int n, int ln, char *err, int cap) {
	const char *k = tok[0];
	if (Pd_Eq(k, "place")) {
		if (n < 2 || n > 3 || (n == 3 && !Pd_Eq(tok[2], "listed"))) {
			return Pd_Err(err, cap, ln, "place <Level> [listed]", NULL);
		}
		if (PlaceDef_Level(t, tok[1])) {
			return Pd_Err(err, cap, ln, "a second place", tok[1]);
		}
		if (t->n >= PLACE_LEVELS) {
			return Pd_Err(err, cap, ln, "too many places", NULL);
		}
		l = &t->lv[t->n++];
		memset(l, 0, sizeof(*l));
		l->traffic = l->police = -1;
		for (int i = 0; i < 3; i++) {
			l->rail[i] = l->intraffic[i] = -1;
		}
		l->listed = n == 3;
		return Pd_Copy(l->level, tok[1], sizeof(l->level)) ? 1 : Pd_Err(err, cap, ln, "a long name", tok[1]);
	}
	if (!l) {
		return Pd_Err(err, cap, ln, "a line before the first place:", k);
	}
	if (Pd_Eq(k, "link")) {
		if (n != 3 || l->nlinks >= PLACE_LINKS) {
			return Pd_Err(err, cap, ln, n != 3 ? "link <door> <Level>[,<Level>]" : "too many links", NULL);
		}
		PlaceLink *k2 = &l->link[l->nlinks++];
		if (!Pd_Copy(k2->door, tok[1], sizeof(k2->door)) || !Pd_Copy(k2->levels, tok[2], sizeof(k2->levels))) {
			return Pd_Err(err, cap, ln, "a long name", NULL);
		}
		return 1;
	}
	if (Pd_Eq(k, "gate")) {
		/* gate <name> <pos 5> to <Level>[:<gate>] ... */
		if (n < 9 || !Pd_Eq(tok[7], "to") || n - 8 > PLACE_OPTS || l->ngates >= PLACE_GATES) {
			return Pd_Err(err, cap, ln, l->ngates >= PLACE_GATES ? "too many gates" :
				"gate <name> <dummy|-> <x> <y> <z> <yaw|*> to <Level>[:<gate>] ...", NULL);
		}
		PlaceGate *g = &l->gate[l->ngates++];
		memset(g, 0, sizeof(*g));
		if (!Pd_Copy(g->name, tok[1], sizeof(g->name)) || !Pd_Pos(&tok[2], &g->pos)) {
			return Pd_Err(err, cap, ln, "a bad gate name or position", tok[1]);
		}
		for (int i = 8; i < n; i++) {
			char *c = strchr(tok[i], ':');
			if (c) {
				*c = 0;
			}
			if (!tok[i][0] || !Pd_Copy(g->opt[g->nopt].level, tok[i], PLACE_NAME) ||
				(c && !Pd_Copy(g->opt[g->nopt].gate, c + 1, PLACE_NAME))) {
				return Pd_Err(err, cap, ln, "a bad gate option", NULL);
			}
			g->nopt++;
		}
		return 1;
	}
	if (Pd_Eq(k, "npc")) {
		if ((n != 2 && n != 7 && n != 8) || (n == 8 && !Pd_Eq(tok[7], "fixed")) || l->nnpcs >= PLACE_NPCS) {
			return Pd_Err(err, cap, ln, l->nnpcs >= PLACE_NPCS ? "too many characters" :
				"npc <Name> | npc <Class>_<id> <dummy|-> <x> <y> <z> <yaw|*> [fixed]", NULL);
		}
		PlaceNpc *c = &l->npc[l->nnpcs++];
		memset(c, 0, sizeof(*c));
		c->mission = n == 2;
		c->fixed = n == 8;
		if (!Pd_Copy(c->name, tok[1], sizeof(c->name)) || !strchr(c->name, '_') ||
			(n > 2 && !Pd_Pos(&tok[2], &c->pos))) {
			return Pd_Err(err, cap, ln, "a character needs <Class>_<id> and a position:", tok[1]);
		}
		return 1;
	}
	if (Pd_Eq(k, "repos")) {
		if (n != 3 || l->nrepos >= PLACE_REPOS || !Pd_Copy(l->repos[l->nrepos].cls, tok[1], PLACE_NAME) ||
			!Pd_Int(tok[2], &l->repos[l->nrepos].n)) {
			return Pd_Err(err, cap, ln, "repos <Class> <n>", NULL);
		}
		l->nrepos++;
		return 1;
	}
	if (Pd_Eq(k, "traffic")) {
		if ((n != 2 && n != 3) || !Pd_Int(tok[1], &l->traffic) ||
			(n == 3 && !Pd_Copy(l->ships, tok[2], sizeof(l->ships)))) {
			return Pd_Err(err, cap, ln, "traffic <n> [<model>,...]", NULL);
		}
		return 1;
	}
	if (Pd_Eq(k, "rail") || Pd_Eq(k, "intraffic")) {
		return Pd_Three(tok, n, Pd_Eq(k, "rail") ? l->rail : l->intraffic) ? 1 :
			Pd_Err(err, cap, ln, k, "<light> <medium> <heavy>");
	}
	if (Pd_Eq(k, "police")) {
		if (n != 2 || (!Pd_Eq(tok[1], "on") && !Pd_Eq(tok[1], "off"))) {
			return Pd_Err(err, cap, ln, "police on|off", NULL);
		}
		l->police = Pd_Eq(tok[1], "on");
		return 1;
	}
	if (Pd_Eq(k, "spawn")) {
		if (n != 6 || l->nspawns >= PLACE_SPAWNS || !Pd_Pos(&tok[1], &l->spawn[l->nspawns])) {
			return Pd_Err(err, cap, ln, l->nspawns >= PLACE_SPAWNS ? "too many spawn points" :
				"spawn <dummy|-> <x> <y> <z> <yaw|*>", NULL);
		}
		l->nspawns++;
		return 1;
	}
	if (Pd_Eq(k, "nouse")) {
		if (n != 2 || l->nnouse >= PLACE_NOUSE || !Pd_Copy(l->nouse[l->nnouse], tok[1], PLACE_NAME)) {
			return Pd_Err(err, cap, ln, "nouse <entity>", NULL);
		}
		l->nnouse++;
		return 1;
	}
	if (Pd_Eq(k, "ai")) {
		return n == 2 && Pd_Num(tok[1], &l->aiStep) && l->aiStep >= 500.0f ? 1 :
			Pd_Err(err, cap, ln, "ai <step of 500 units or more>", NULL);
	}
	return Pd_Err(err, cap, ln, "an unknown line:", k);
}

int PlaceDef_Parse(const char *text, PlaceTable *t, char *err, int cap) {
	char line[PD_LINE];
	char *tok[PD_TOKENS];
	int ln = 0;

	memset(t, 0, sizeof(*t));
	if (err && cap > 0) {
		err[0] = 0;
	}
	while (*text) {
		size_t len = strcspn(text, "\n");
		ln++;
		if (len >= sizeof(line)) {
			return Pd_Err(err, cap, ln, "a long line", NULL);
		}
		memcpy(line, text, len);
		line[len] = 0;
		text += len + (text[len] == '\n');
		char *hash = strchr(line, '#');
		if (hash) {
			*hash = 0;
		}
		int n = 0;
		for (char *p = strtok(line, " \t\r"); p; p = strtok(NULL, " \t\r")) {
			if (n == PD_TOKENS) {
				return Pd_Err(err, cap, ln, "too many words", NULL);
			}
			tok[n++] = p;
		}
		if (n && !Pd_Line(t, t->n ? &t->lv[t->n - 1] : NULL, tok, n, ln, err, cap)) {
			return 0;
		}
	}
	return 1;
}

const PlaceTable *PlaceDef_Table(void) {
	static PlaceTable table;
	static int parsed;
	char err[128];
	if (!parsed) {
		parsed = 1;
		if (!PlaceDef_Parse(pd_text, &table, err, sizeof(err))) {
			fprintf(stderr, "places: data/places/places.def: %s\n", err);
			memset(&table, 0, sizeof(table));
		}
	}
	return &table;
}

const PlaceLevel *PlaceDef_Level(const PlaceTable *t, const char *level) {
	for (int i = 0; t && level && i < t->n; i++) {
		if (Pd_Eq(t->lv[i].level, level)) {
			return &t->lv[i];
		}
	}
	return NULL;
}

const PlaceGate *PlaceDef_Gate(const PlaceLevel *l, const char *name) {
	for (int i = 0; l && name && i < l->ngates; i++) {
		if (Pd_Eq(l->gate[i].name, name)) {
			return &l->gate[i];
		}
	}
	return NULL;
}

int PlaceDef_Places(const PlaceTable *t, char *out, int cap) {
	int len = 0, ok = 1;
	if (cap > 0) {
		out[0] = 0;
	}
	for (int i = 0; i < t->n; i++) {
		if (!t->lv[i].listed) {
			continue;
		}
		int need = (int)strlen(t->lv[i].level) + (len > 0);
		if (len + need >= cap) {
			ok = 0;
			continue;
		}
		len += snprintf(out + len, (size_t)(cap - len), "%s%s", len ? "," : "", t->lv[i].level);
	}
	return ok;
}

const char *PlaceDef_WorldLevels(void) {
	static char levels[sizeof(COORD_CITY) + PLACE_LEVELS * (PLACE_NAME + 1)];
	if (!levels[0]) {
		char places[PLACE_LEVELS * (PLACE_NAME + 1)];
		PlaceDef_Places(PlaceDef_Table(), places, sizeof(places));
		snprintf(levels, sizeof(levels), "%s%s%s", COORD_CITY, places[0] ? "," : "", places);
	}
	return levels;
}
