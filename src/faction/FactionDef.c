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

static const char *const fd_abilities[FA_COUNT] = { "join", "siren", "armour", "arrest", "backup", "ship", "heal",
	"ray", "revive", "steal", "suck", "loan", "bribe", "bounty", "contracts", "hire", "heavy", "slow", "blabla", "permit",
	"fine", "photo", "courier" };
static const char *const fd_crimes[FCRIME_COUNT] = { "hurt_police", "hurt_civilian", "kill_police", "kill_civilian",
	"hurt_player", "kill_player", "theft" };

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
	t->decayOff = 0.1f;
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
	for (int a = 0; a < FA_COUNT; a++) {
		t->commonCd[a] = -1.0f;
	}
	t->healLife = 60;
	t->healPrice = 120;
	t->rayDamage = 30;
	t->rayReach = 2500.0f;
	t->downed = 15.0f;
	t->reviveReach = 900.0f;
	t->revivePrice = 100;
	t->steal = 150;
	t->suck = 60;
	t->stealReach = 900.0f;
	t->suckReach = 1200.0f;
	t->loan = 1000;
	t->loanOwed = 1.25f;
	t->loanShare = 0.5f;
	t->loanCommission = 0.1f;
	t->bribe = 400;
	t->bounty = 500;
	t->bountyMax = 5000;
	t->bountyTime = 3600;
	t->hireCount = 2;
	t->hirePrice = 500;
	t->hireTime = 120.0f;
	t->heavyLife = 1.5f;
	t->slowRadius = 2500.0f;
	t->slowTime = 8.0f;
	t->slowSpeed = 0.3f;
	t->blablaRadius = 1200.0f;
	t->blablaTime = 4.0f;
	t->permitPrice = 300;
	t->permitRank = 3;
	t->permitFine = 250;
	t->photoReach = 4000.0f;
	t->photoAge = 15.0f;
	t->photoPoints = 6.0f;
	t->photoReward = 100;
	t->courierTime = 300;
	t->courierReward = 400;
	t->courierBonus = 200;
}

static void Fd_NewFaction(FactionDef *f, const char *name) {
	memset(f, 0, sizeof(*f));
	Fd_Copy(f->name, name, sizeof(f->name));
	for (int a = 0; a < FA_COUNT; a++) {
		f->cooldown[a] = -1.0f;
	}
	f->nranks = 1;
	Fd_Copy(f->pilot, "P_Dtritus", sizeof(f->pilot));
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

/* the numbers of a rule into floats (count of them); 1 when the key and the count match */
static int Fd_Nums(char **tok, int n, const char *key, int count, float *out) {
	if (!Fd_Eq(tok[0], key) || n != count + 1) {
		return 0;
	}
	for (int i = 0; i < count; i++) {
		out[i] = (float)atof(tok[i + 1]);
	}
	return 1;
}

/* the rules of the abilities of the factions (outside a faction): 0 done, -1 an error, 1 not one of them */
static int Fd_Rules(FactionTable *t, char **tok, int n, int ln, char *err, int cap) {
	float v[4];
	if (Fd_Nums(tok, n, "heal", 2, v)) {
		t->healLife = (int)v[0];
		t->healPrice = (int)v[1];
	} else if (Fd_Nums(tok, n, "ray", 2, v)) {
		t->rayDamage = (int)v[0];
		t->rayReach = v[1];
	} else if (Fd_Nums(tok, n, "revive", 3, v)) {
		t->downed = v[0];
		t->reviveReach = v[1];
		t->revivePrice = (int)v[2];
	} else if (Fd_Nums(tok, n, "steal", 2, v)) {
		t->steal = (int)v[0];
		t->stealReach = v[1];
	} else if (Fd_Nums(tok, n, "suck", 2, v)) {
		t->suck = (int)v[0];
		t->suckReach = v[1];
	} else if (Fd_Nums(tok, n, "loan", 4, v)) {
		t->loan = (int)v[0];
		t->loanOwed = v[1];
		t->loanShare = v[2];
		t->loanCommission = v[3];
		if (t->loanOwed < 1.0f || t->loanShare <= 0.0f || t->loanShare > 1.0f) {
			return Fd_Err(err, cap, ln, "loan: owed >= 1, share in (0, 1]", NULL);
		}
	} else if (Fd_Nums(tok, n, "bribe", 1, v)) {
		t->bribe = (int)v[0];
	} else if (Fd_Nums(tok, n, "bounty", 3, v)) {
		t->bounty = (int)v[0];
		t->bountyMax = (int)v[1];
		t->bountyTime = (int)v[2];
	} else if (Fd_Eq(tok[0], "hire") && n == 5) {
		Fd_Copy(t->hireShip, tok[1], sizeof(t->hireShip));
		t->hireCount = atoi(tok[2]);
		t->hireTime = (float)atof(tok[3]);
		t->hirePrice = atoi(tok[4]);
	} else if (Fd_Nums(tok, n, "heavy", 1, v)) {
		t->heavyLife = v[0];
	} else if (Fd_Nums(tok, n, "slow", 3, v)) {
		t->slowRadius = v[0];
		t->slowTime = v[1];
		t->slowSpeed = v[2];
		if (t->slowSpeed <= 0.0f || t->slowSpeed > 1.0f) {
			return Fd_Err(err, cap, ln, "slow: the speed is in (0, 1]", NULL);
		}
	} else if (Fd_Nums(tok, n, "blabla", 2, v)) {
		t->blablaRadius = v[0];
		t->blablaTime = v[1];
	} else if (Fd_Nums(tok, n, "permit", 3, v)) {
		t->permitPrice = (int)v[0];
		t->permitRank = (int)v[1];
		t->permitFine = (int)v[2];
	} else if (Fd_Nums(tok, n, "photo", 4, v)) {
		t->photoReach = v[0];
		t->photoAge = v[1];
		t->photoPoints = v[2];
		t->photoReward = (int)v[3];
	} else if (Fd_Nums(tok, n, "courier", 3, v)) {
		t->courierTime = (int)v[0];
		t->courierReward = (int)v[1];
		t->courierBonus = (int)v[2];
	} else if (Fd_Eq(tok[0], "route") && n == 3) {
		if (t->nroutes >= FACTION_ROUTES) {
			return Fd_Err(err, cap, ln, "too many routes", NULL);
		}
		Fd_Copy(t->routes[t->nroutes].level, tok[1], FACTION_NAME);
		Fd_Copy(t->routes[t->nroutes++].dummy, tok[2], FACTION_NAME);
	} else if (Fd_Eq(tok[0], "sanctuary") && n == 4) {
		if (t->nzones >= FACTION_ZONES) {
			return Fd_Err(err, cap, ln, "too many sanctuaries", NULL);
		}
		FactionZone *z = &t->zones[t->nzones++];
		Fd_Copy(z->level, tok[1], FACTION_NAME);
		Fd_Copy(z->dummy, tok[2], FACTION_NAME);
		z->radius = (float)atof(tok[3]);
	} else if (Fd_Eq(tok[0], "common") && n == 3) {
		int a = Fd_Index(fd_abilities, FA_COUNT, tok[1]);
		if (a <= FA_JOIN || t->ncommon >= FACTION_KEYS || t->commonCd[a] >= 0.0f) {
			return Fd_Err(err, cap, ln, "bad common ability", tok[1]);
		}
		float cd = (float)atof(tok[2]);
		t->commonCd[a] = cd < 0.0f ? 0.0f : cd;
		t->common[t->ncommon++] = (uint8_t)a;
	} else {
		return 1;
	}
	return 0;
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
	int r = Fd_Rules(t, tok, n, ln, err, cap);
	if (r <= 0) {
		return r;
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
	if (Fd_Eq(k, "play") && n > 1) {
		f->nplay = 0;
		for (int i = 1; i < n && f->nplay < FACTION_RANKS; i++) {
			Fd_Copy(f->play[f->nplay++], tok[i], FACTION_NAME);
		}
		return 0;
	}
	if (Fd_Eq(k, "pilot") && n == 2) {
		Fd_Copy(f->pilot, tok[1], sizeof(f->pilot));
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
		if (f->cooldown[a] >= 0.0f) {
			return Fd_Err(err, cap, ln, "ability twice", tok[1]);
		}
		if (a == FA_JOIN && f->nkeys) {
			return Fd_Err(err, cap, ln, "join must be the first ability", NULL);
		}
		if (f->nkeys >= FACTION_KEYS) {
			return Fd_Err(err, cap, ln, "too many abilities for the keys:", tok[1]);
		}
		float cd = (float)atof(tok[2]);
		f->cooldown[a] = cd < 0.0f ? 0.0f : cd;
		f->keys[f->nkeys++] = (uint8_t)a;
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
		for (int k = 0; k < n; k++) {
			if (strlen(tok[k]) >= FACTION_NAME) {
				return Fd_Err(err, cap, ln, "name too long:", tok[k]);
			}
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
		if (!f->nplay || !f->office[0]) {
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

const char *Faction_PlayClass(const FactionDef *f, int rank) {
	int k = rank < 1 ? 0 : rank > f->nplay ? f->nplay - 1 : rank - 1;
	return f->nplay ? f->play[k] : "";
}

const char *Faction_RankShip(const FactionDef *f, int rank) {
	int k = rank < 1 ? 0 : rank > f->nships ? f->nships - 1 : rank - 1;
	return f->nships ? f->ships[k] : "";
}

int Faction_KeyAbility(const FactionTable *t, int f, int k) {
	int own = 1;
	const FactionDef *d = f >= 0 && f < t->n ? &t->f[f] : NULL;
	if (k < 0 || k >= FACTION_KEYS) {
		return -1;
	}
	if (d) {
		/* a member's keys: its join first, its abilities, then the common ones it has not */
		own = d->nkeys;
		if (k < own) {
			return d->keys[k];
		}
	} else if (k == 0) {
		return FA_JOIN;
	}
	for (int c = 0, at = own; c < t->ncommon; c++) {
		if (d && d->cooldown[t->common[c]] >= 0.0f) {
			continue;
		}
		if (at++ == k) {
			return t->common[c];
		}
	}
	return -1;
}

float Faction_Cooldown(const FactionTable *t, int f, int a) {
	if (a < 0 || a >= FA_COUNT) {
		return -1.0f;
	}
	if (f >= 0 && f < t->n && t->f[f].cooldown[a] >= 0.0f) {
		return t->f[f].cooldown[a];
	}
	if (a == FA_JOIN) {
		return 0.0f;
	}
	/* a common ability only while it has a key */
	for (int k = 0; k < FACTION_KEYS; k++) {
		if (Faction_KeyAbility(t, f, k) == a) {
			return t->commonCd[a];
		}
	}
	return -1.0f;
}

int Faction_AbilityOf(const char *name) {
	return Fd_Index(fd_abilities, FA_COUNT, name ? name : "");
}

int Faction_PvPRule(const FactionTable *t, const FactionSide *a, const FactionSide *b, int policePvp, int factionPvp) {
	int police = Faction_Find(t, "Police"), mercs = Faction_Find(t, "Mercs");
	if (a->sanctuary || b->sanctuary) {
		return 0;
	}
	if (policePvp && police >= 0 && ((a->faction == police && !a->wanted && b->wanted) ||
		(b->faction == police && !b->wanted && a->wanted))) {
		return 1;
	}
	if (mercs >= 0 && ((a->faction == mercs && b->bounty > 0) || (b->faction == mercs && a->bounty > 0))) {
		return 1;
	}
	if (a->faction >= 0 && (a->faction == b->faction || Faction_Relation(t, a->faction, b->faction) == FREL_FRIEND ||
		Faction_Relation(t, b->faction, a->faction) == FREL_FRIEND)) {
		return 0;
	}
	if (factionPvp && (Faction_Relation(t, a->faction, b->faction) == FREL_HOSTILE ||
		Faction_Relation(t, b->faction, a->faction) == FREL_HOSTILE)) {
		return 1;
	}
	return -1;
}

int FactionLoan_Repay(const FactionTable *t, int *debt, int income) {
	if (*debt <= 0 || income <= 0) {
		return 0;
	}
	int pay = (int)((float)income * t->loanShare + 0.5f);
	pay = pay < 1 ? 1 : pay > *debt ? *debt : pay;
	*debt -= pay;
	return pay;
}

int FactionBounty_Add(const FactionTable *t, int *bounty, int64_t *ends, int amount, int64_t now) {
	FactionBounty_Live(bounty, ends, now);
	int room = t->bountyMax - *bounty;
	int take = amount > room ? room : amount;
	if (take <= 0) {
		return 0;
	}
	*bounty += take;
	*ends = now + t->bountyTime;
	return take;
}

int FactionBounty_Live(int *bounty, int64_t *ends, int64_t now) {
	if (*bounty > 0 && *ends > 0 && now >= *ends) {
		*bounty = 0;
	}
	if (*bounty <= 0) {
		*bounty = 0;
		*ends = 0;
	}
	return *bounty;
}

int Faction_Route(const FactionTable *t, const char *from, uint32_t seed) {
	int n = 0;
	for (int r = 0; r < t->nroutes; r++) {
		n += !Fd_Eq(t->routes[r].level, from ? from : "");
	}
	if (!n) {
		return -1;
	}
	int pick = (int)(seed % (uint32_t)n);
	for (int r = 0; r < t->nroutes; r++) {
		if (!Fd_Eq(t->routes[r].level, from ? from : "") && pick-- == 0) {
			return r;
		}
	}
	return -1;
}

int Faction_CourierReward(const FactionTable *t, int secs) {
	if (secs < 0 || secs > t->courierTime) {
		return 0;
	}
	return t->courierReward + (secs * 2 <= t->courierTime ? t->courierBonus : 0);
}

byte Faction_NeedsPermit(const FactionTable *t, int rank) {
	return t->permitRank > 0 && rank >= t->permitRank;
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
