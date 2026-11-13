/*
 * The faction table and the wanted level rules of the freeroam mode, see include/scrap/faction.h. Pure: no engine.
 * Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/faction.h"

#define FD_TOKENS 16
#define FD_LINE 256

const char g_FactionTableText[] = {
#embed "../../data/factions/factions.def"
	, 0 };

static const char *const fd_abilities[FA_COUNT] = { "join", "siren", "armour", "arrest", "backup", "ship" };
static const char *const fd_crimes[FCRIME_COUNT] = { "hurt_police", "hurt_civilian", "kill_police", "kill_civilian",
	"hurt_player", "kill_player" };

static int Fd_Lower(int c) {
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int Fd_Eq(const char *a, const char *b) {
	while (*a && Fd_Lower((unsigned char)*a) == Fd_Lower((unsigned char)*b)) {
		a++;
		b++;
	}
	return !*a && !*b;
}

static void Fd_Copy(char *out, const char *s, size_t cap) {
	size_t n = strlen(s);
	n = n < cap - 1 ? n : cap - 1;
	memcpy(out, s, n);
	out[n] = 0;
}

static int Fd_Index(const char *const *names, int n, const char *s) {
	for (int i = 0; i < n; i++) {
		if (Fd_Eq(names[i], s)) {
			return i;
		}
	}
	return -1;
}

static void Fd_Defaults(FactionTable *t) {
	static const float wanted[FACTION_LEVELS] = { 1.0f, 6.0f, 14.0f, 26.0f, 45.0f };
	memset(t, 0, sizeof(*t));
	memcpy(t->wanted, wanted, sizeof(wanted));
	t->decayOn = 0.02f;
	t->decayOff = 0.25f;
	t->crimeGap = 1.0f;
	t->fine = 300;
	t->reward = 150;
	t->reachFoot = 900.0f;
	t->reachShip = 2000.0f;
	t->deskReach = 1200.0f;
	t->sirenRadius = 3500.0f;
	t->sirenAway = 2500.0f;
	t->sirenTime = 5.0f;
	t->backupCount = 2;
}

static void Fd_NewFaction(FactionDef *f, const char *name) {
	memset(f, 0, sizeof(*f));
	Fd_Copy(f->name, name, sizeof(f->name));
	for (int a = 0; a < FA_COUNT; a++) {
		f->cooldown[a] = -1.0f;
	}
	f->nranks = 1;
}

/* a list of names into list (at most FACTION_LIST) */
static int Fd_Names(char list[][FACTION_NAME], char **tok, int n) {
	int k = 0;
	for (int i = 0; i < n && k < FACTION_LIST; i++) {
		Fd_Copy(list[k++], tok[i], FACTION_NAME);
	}
	return k;
}

static int Fd_Err(char *err, int cap, int line, const char *why, const char *what) {
	if (err && cap > 0) {
		snprintf(err, (size_t)cap, "line %d: %s%s%s", line, why, what ? " " : "", what ? what : "");
	}
	return -1;
}

/* one statement (tok[0] the key) of line ln into t (f: the faction of the lines, NULL before the first) */
static int Fd_Statement(FactionTable *t, FactionDef *f, char **tok, int n, int ln, char *err, int cap) {
	const char *k = tok[0];
	if (Fd_Eq(k, "wanted")) {
		if (n != FACTION_LEVELS + 1) {
			return Fd_Err(err, cap, ln, "wanted needs 5 levels", NULL);
		}
		for (int i = 0; i < FACTION_LEVELS; i++) {
			t->wanted[i] = (float)atof(tok[i + 1]);
			if (t->wanted[i] <= 0.0f || (i && t->wanted[i] <= t->wanted[i - 1])) {
				return Fd_Err(err, cap, ln, "wanted levels must rise from above 0", NULL);
			}
		}
		return 0;
	}
	if (Fd_Eq(k, "decay") && n == 3) {
		t->decayOn = (float)atof(tok[1]);
		t->decayOff = (float)atof(tok[2]);
		return 0;
	}
	if (Fd_Eq(k, "crime") && n == 3) {
		int c = Fd_Index(fd_crimes, FCRIME_COUNT, tok[1]);
		if (c < 0) {
			return Fd_Err(err, cap, ln, "unknown crime", tok[1]);
		}
		t->crime[c] = (float)atof(tok[2]);
		return 0;
	}
	if (Fd_Eq(k, "crimegap") && n == 2) {
		t->crimeGap = (float)atof(tok[1]);
		return 0;
	}
	if (Fd_Eq(k, "fine") && n == 2) {
		t->fine = atoi(tok[1]);
		return 0;
	}
	if (Fd_Eq(k, "reward") && n == 2) {
		t->reward = atoi(tok[1]);
		return 0;
	}
	if (Fd_Eq(k, "reach") && n == 3) {
		t->reachFoot = (float)atof(tok[1]);
		t->reachShip = (float)atof(tok[2]);
		return 0;
	}
	if (Fd_Eq(k, "deskreach") && n == 2) {
		t->deskReach = (float)atof(tok[1]);
		return 0;
	}
	if (Fd_Eq(k, "siren") && n == 4) {
		t->sirenRadius = (float)atof(tok[1]);
		t->sirenAway = (float)atof(tok[2]);
		t->sirenTime = (float)atof(tok[3]);
		return 0;
	}
	if (Fd_Eq(k, "backup") && n == 3) {
		Fd_Copy(t->backupShip, tok[1], sizeof(t->backupShip));
		t->backupCount = atoi(tok[2]);
		return 0;
	}
	if (!f) {
		return Fd_Err(err, cap, ln, "unknown statement or wrong count", k);
	}
	if (Fd_Eq(k, "chars") && n > 1) {
		f->nchars = Fd_Names(f->chars, tok + 1, n - 1);
		return 0;
	}
	if (Fd_Eq(k, "ships") && n > 1) {
		f->nships = Fd_Names(f->ships, tok + 1, n - 1);
		return 0;
	}
	if (Fd_Eq(k, "play") && n == 2) {
		Fd_Copy(f->play, tok[1], sizeof(f->play));
		return 0;
	}
	if (Fd_Eq(k, "armour") && n == 3) {
		Fd_Copy(f->armour, tok[1], sizeof(f->armour));
		f->armourLife = atoi(tok[2]);
		return 0;
	}
	if (Fd_Eq(k, "office") && (n == 2 || n == 3)) {
		Fd_Copy(f->office, tok[1], sizeof(f->office));
		Fd_Copy(f->desk, n == 3 ? tok[2] : "", sizeof(f->desk));
		return 0;
	}
	if (Fd_Eq(k, "jail") && n == 2) {
		Fd_Copy(f->jail, tok[1], sizeof(f->jail));
		return 0;
	}
	if (Fd_Eq(k, "ranks") && n > 1) {
		f->nranks = 0;
		for (int i = 1; i < n && f->nranks < FACTION_RANKS; i++) {
			int a = atoi(tok[i]);
			if ((f->nranks == 0 && a != 0) || (f->nranks && a <= f->ranks[f->nranks - 1])) {
				return Fd_Err(err, cap, ln, "ranks must start at 0 and rise", NULL);
			}
			f->ranks[f->nranks++] = a;
		}
		return 0;
	}
	if (Fd_Eq(k, "relation") && n == 3) {
		static const char *const kinds[] = { "neutral", "friend", "hostile" };
		int r = Fd_Index(kinds, 3, tok[2]);
		if (r < 0 || f->nrel >= FACTION_MAX) {
			return Fd_Err(err, cap, ln, "bad relation", tok[2]);
		}
		Fd_Copy(f->relName[f->nrel], tok[1], FACTION_NAME);
		f->relKind[f->nrel++] = (uint8_t)r;
		return 0;
	}
	if (Fd_Eq(k, "ability") && n == 3) {
		int a = Fd_Index(fd_abilities, FA_COUNT, tok[1]);
		if (a < 0) {
			return Fd_Err(err, cap, ln, "unknown ability", tok[1]);
		}
		float cd = (float)atof(tok[2]);
		f->cooldown[a] = cd < 0.0f ? 0.0f : cd;
		return 0;
	}
	return Fd_Err(err, cap, ln, "unknown statement or wrong count", k);
}

int FactionTable_Parse(FactionTable *t, const char *text, char *err, int cap) {
	char line[FD_LINE];
	char *tok[FD_TOKENS];
	int ln = 0;

	Fd_Defaults(t);
	if (err && cap > 0) {
		err[0] = 0;
	}
	for (const char *p = text; p && *p;) {
		const char *end = strchr(p, '\n');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		ln++;
		if (len >= sizeof(line)) {
			return Fd_Err(err, cap, ln, "line too long", NULL);
		}
		memcpy(line, p, len);
		line[len] = 0;
		p = end ? end + 1 : NULL;
		char *hash = strchr(line, '#');
		if (hash) {
			*hash = 0;
		}
		int n = 0;
		for (char *s = line; *s && n < FD_TOKENS;) {
			while (*s == ' ' || *s == '\t' || *s == '\r') {
				*s++ = 0;
			}
			if (!*s) {
				break;
			}
			tok[n++] = s;
			while (*s && *s != ' ' && *s != '\t' && *s != '\r') {
				s++;
			}
		}
		if (!n) {
			continue;
		}
		if (Fd_Eq(tok[0], "faction")) {
			if (n != 2) {
				return Fd_Err(err, cap, ln, "faction needs a name", NULL);
			}
			if (t->n >= FACTION_MAX) {
				return Fd_Err(err, cap, ln, "too many factions", NULL);
			}
			if (Faction_Find(t, tok[1]) != FACTION_NONE) {
				return Fd_Err(err, cap, ln, "faction twice", tok[1]);
			}
			Fd_NewFaction(&t->f[t->n++], tok[1]);
			continue;
		}
		if (Fd_Statement(t, t->n ? &t->f[t->n - 1] : NULL, tok, n, ln, err, cap) < 0) {
			return -1;
		}
	}
	for (int i = 0; i < t->n; i++) {
		FactionDef *f = &t->f[i];
		if (!f->play[0] || !f->office[0]) {
			return Fd_Err(err, cap, ln, "a faction needs play and office:", f->name);
		}
		for (int r = 0; r < f->nrel; r++) {
			if (Faction_Find(t, f->relName[r]) == FACTION_NONE) {
				return Fd_Err(err, cap, ln, "relation to an unknown faction", f->relName[r]);
			}
		}
	}
	return t->n;
}

int Faction_Find(const FactionTable *t, const char *name) {
	for (int i = 0; name && i < t->n; i++) {
		if (Fd_Eq(t->f[i].name, name)) {
			return i;
		}
	}
	return FACTION_NONE;
}

int Faction_OfClass(const FactionTable *t, const char *cls) {
	for (int i = 0; cls && *cls && i < t->n; i++) {
		for (int k = 0; k < t->f[i].nchars; k++) {
			if (Fd_Eq(t->f[i].chars[k], cls)) {
				return i;
			}
		}
	}
	return FACTION_NONE;
}

int Faction_OfShip(const FactionTable *t, const char *model) {
	for (int i = 0; model && *model && i < t->n; i++) {
		for (int k = 0; k < t->f[i].nships; k++) {
			if (Fd_Eq(t->f[i].ships[k], model)) {
				return i;
			}
		}
	}
	return FACTION_NONE;
}

int Faction_Relation(const FactionTable *t, int a, int b) {
	if (a < 0 || b < 0 || a >= t->n || b >= t->n || a == b) {
		return FREL_NEUTRAL;
	}
	const FactionDef *f = &t->f[a];
	for (int r = 0; r < f->nrel; r++) {
		if (Fd_Eq(f->relName[r], t->f[b].name)) {
			return f->relKind[r];
		}
	}
	return FREL_NEUTRAL;
}

int Faction_Rank(const FactionDef *f, int arrests) {
	int r = 1;
	for (int k = 1; k < f->nranks; k++) {
		if (arrests >= f->ranks[k]) {
			r = k + 1;
		}
	}
	return r;
}

const char *Faction_AbilityName(int a) {
	return a >= 0 && a < FA_COUNT ? fd_abilities[a] : NULL;
}

const char *Faction_CrimeName(int c) {
	return c >= 0 && c < FCRIME_COUNT ? fd_crimes[c] : NULL;
}

void FactionWanted_Init(FactionWanted *w) {
	w->points = 0.0f;
	for (int c = 0; c < FCRIME_COUNT; c++) {
		w->last[c] = -1.0;
	}
}

int FactionWanted_Crime(const FactionTable *t, FactionWanted *w, int crime, double now) {
	if (crime < 0 || crime >= FCRIME_COUNT || t->crime[crime] <= 0.0f) {
		return 0;
	}
	if (w->last[crime] >= 0.0 && now - w->last[crime] < t->crimeGap) {
		return 0;
	}
	w->last[crime] = now;
	w->points += t->crime[crime];
	/* the points stay a little above the top level: a few seconds of decay do not drop it */
	float top = t->wanted[FACTION_LEVELS - 1] * 1.5f;
	if (w->points > top) {
		w->points = top;
	}
	return 1;
}

void FactionWanted_Decay(const FactionTable *t, FactionWanted *w, float dt, int alarmOn) {
	w->points -= (alarmOn ? t->decayOn : t->decayOff) * dt;
	if (w->points < 0.0f) {
		w->points = 0.0f;
	}
}

int FactionWanted_Level(const FactionTable *t, const FactionWanted *w) {
	int l = 0;
	while (l < FACTION_LEVELS && w->points >= t->wanted[l]) {
		l++;
	}
	return l;
}

float FactionWanted_Points(const FactionTable *t, int level) {
	return level <= 0 ? 0.0f : t->wanted[(level > FACTION_LEVELS ? FACTION_LEVELS : level) - 1];
}
