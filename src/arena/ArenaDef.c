/*
 * The pure part of the PvP modes on the new network (include/scrap/arena.h): the modes, the rules of the original's
 * scores, the next level, the order of the scorer, the texts and the wire. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "scrap/arena.h"
#include "scrap/netrep.h"

const ArenaModeDef g_ArenaModes[ARENA_MODES] = {
	{ "dm", "DeathMatch", "NDeathMatch", 0 },
	{ "dmbots", "DeathMatchBots", "NDeathMatchBots", ARENA_F_BOTS },
	{ "tdm", "TeamDM", "NTeamDM", ARENA_F_TEAM },
	{ "flaghunt", "FlagHunt", "NFlagHunt", ARENA_F_FLAG },
	{ "teamflag", "TeamFlag", "NTeamFlag", ARENA_F_TEAM | ARENA_F_FLAG },
	{ "postman", "Postman", "NPostman", ARENA_F_POST },
	{ "teampost", "TeamPost", "NTeamPost", ARENA_F_TEAM | ARENA_F_POST },
};

int Arena_ModeOf(const char *name) {
	for (int i = 0; name && i < ARENA_MODES; i++) {
		if (!strcasecmp(name, g_ArenaModes[i].arg) || !strcasecmp(name, g_ArenaModes[i].type)) {
			return i;
		}
	}
	return -1;
}

static int Ar_Flags(const ArenaState *s) {
	return s->mode < ARENA_MODES ? g_ArenaModes[s->mode].flags : 0;
}

static int32_t *Ar_Total(ArenaState *s, int k) {
	return s->m[k].team == ARENA_TEAM_RED ? &s->red : s->m[k].team == ARENA_TEAM_GREEN ? &s->green : NULL;
}

static byte Ar_Live(const ArenaState *s, int k) {
	return k >= 0 && k < ARENA_MEMBERS && s->m[k].used;
}

void ArenaRules_Kill(ArenaState *s, int victim, int killer) {
	int f = Ar_Flags(s);
	if (!Ar_Live(s, victim)) {
		return;
	}
	if (!Ar_Live(s, killer)) {
		killer = -1;
	}
	/* Ffa.OnDeathServer: the killer gets +1, a suicide (or no killer) and a team kill -1 for the one who did it */
	int x = killer >= 0 && killer != victim ? killer : victim;
	int add = x == victim || ((f & ARENA_F_TEAM) && s->m[x].team == s->m[victim].team) ? -1 : 1;
	int32_t *total;
	switch (s->mode) {
	case ARENA_DM:
	case ARENA_DMBOTS:
	case ARENA_TEAMPOST:
		s->m[x].score += add;
		break;
	case ARENA_TDM:
		s->m[x].score += add;
		if ((total = Ar_Total(s, x)) != NULL) {
			*total += add;
		}
		break;
	case ARENA_FLAGHUNT:
	case ARENA_TEAMFLAG:
		/* NFlagHunt.DeathFunc: the dead loses a point it has (NTeamFlag: and its team one) */
		if (s->m[victim].score > 0) {
			s->m[victim].score--;
		}
		if (s->mode == ARENA_TEAMFLAG && (total = Ar_Total(s, victim)) != NULL && *total > 0) {
			(*total)--;
		}
		break;
	default:
		break;
	}
}

void ArenaRules_Score(ArenaState *s, int k) {
	if (!Ar_Live(s, k)) {
		return;
	}
	int32_t *total = Ar_Total(s, k);
	/* NTeamPost.ServerTakeBase: 10 for the carrier and one for its team; the others one point */
	s->m[k].score += s->mode == ARENA_TEAMPOST ? 10 : 1;
	if ((Ar_Flags(s) & ARENA_F_TEAM) && total) {
		(*total)++;
	}
}

byte ArenaRules_Started(ArenaState *s) {
	int red = 0, green = 0, all = 0;
	if (s->started) {
		return 1;
	}
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		const ArenaMember *m = &s->m[k];
		if (!m->used || !(m->flags & ARENA_MF_SHIP) || (m->flags & ARENA_MF_SPECT)) {
			continue;
		}
		all++;
		red += m->team == ARENA_TEAM_RED;
		green += m->team == ARENA_TEAM_GREEN;
	}
	s->started = (Ar_Flags(s) & ARENA_F_TEAM) ? red > 0 && green > 0 : all > 1;
	return s->started;
}

byte ArenaRules_Ended(const ArenaState *s) {
	if (Ar_Flags(s) & ARENA_F_TEAM) {
		return s->red >= s->fraglimit || s->green >= s->fraglimit;
	}
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		if (s->m[k].used && !(s->m[k].flags & ARENA_MF_SPECT) && s->m[k].score >= s->fraglimit) {
			return 1;
		}
	}
	return 0;
}

int ArenaRules_JoinTeam(const ArenaState *s, int want, int skip) {
	int n[3] = { 0, 0, 0 };
	if (!(Ar_Flags(s) & ARENA_F_TEAM)) {
		return ARENA_TEAM_NONE;
	}
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		if (k != skip && s->m[k].used && s->m[k].team <= ARENA_TEAM_GREEN) {
			n[s->m[k].team]++;
		}
	}
	if (want != ARENA_TEAM_RED && want != ARENA_TEAM_GREEN) {
		want = ARENA_TEAM_RED;
	}
	int other = want == ARENA_TEAM_RED ? ARENA_TEAM_GREEN : ARENA_TEAM_RED;
	return n[want] > n[other] ? other : want;
}

void Arena_NextLevel(const char *list, const char *cur, char *out, int cap) {
	char first[ARENA_LEVEL + 1] = "", item[ARENA_LEVEL + 1];
	int found = 0;
	snprintf(out, (size_t)cap, "%s", cur);
	for (const char *p = list; p && *p;) {
		const char *e = strchr(p, ',');
		int n = e ? (int)(e - p) : (int)strlen(p);
		snprintf(item, sizeof(item), "%.*s", n > ARENA_LEVEL ? ARENA_LEVEL : n, p);
		p = e ? e + 1 : NULL;
		if (!item[0]) {
			continue;
		}
		if (found) {
			snprintf(out, (size_t)cap, "%s", item);
			return;
		}
		if (!first[0]) {
			snprintf(first, sizeof(first), "%s", item);
		}
		found = !strcasecmp(item, cur);
	}
	if (first[0]) {
		snprintf(out, (size_t)cap, "%s", first);
	}
}

static const ArenaState *ar_sort;
static int ar_you;

static int Ar_Group(const ArenaState *s, int k) {
	const ArenaMember *m = &s->m[k];
	if (m->flags & ARENA_MF_SPECT) {
		return 3;
	}
	if (m->team == ARENA_TEAM_NONE) {
		return 2;
	}
	byte greenFirst = s->green > s->red;
	return (m->team == ARENA_TEAM_GREEN) == greenFirst ? 0 : 1;
}

static int Ar_RowCmp(const void *pa, const void *pb) {
	int a = *(const int *)pa, b = *(const int *)pb;
	const ArenaState *s = ar_sort;
	int ga = Ar_Group(s, a), gb = Ar_Group(s, b);
	if (ga != gb) {
		return ga - gb;
	}
	if (s->m[a].score != s->m[b].score) {
		return s->m[a].score > s->m[b].score ? -1 : 1;
	}
	if ((a == ar_you) != (b == ar_you)) {
		return a == ar_you ? -1 : 1;
	}
	/* the original sorted the rows down: an equal score by the name, down */
	int c = strcmp(s->m[b].name, s->m[a].name);
	return c ? c : a - b;
}

int Arena_Rows(const ArenaState *s, int you, int *rows, int max) {
	int n = 0;
	for (int k = 0; k < ARENA_MEMBERS && n < max; k++) {
		if (s->m[k].used) {
			rows[n++] = k;
		}
	}
	ar_sort = s;
	ar_you = you;
	qsort(rows, (size_t)n, sizeof(rows[0]), Ar_RowCmp);
	return n;
}

/* ---- the wire ---- */

int Arena_WriteState(uint8_t *buf, int cap, const ArenaState *s, int you) {
	NetRepBuf w;
	int count = 0;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, ARENA_MSG);
	NetRep_PutU8(&w, ARENA_ST_STATE);
	NetRep_PutU8(&w, s->mode);
	NetRep_PutU8(&w, s->phase);
	NetRep_PutU8(&w, s->started);
	NetRep_PutU8(&w, s->round);
	NetRep_PutSVar(&w, s->fraglimit);
	NetRep_PutSVar(&w, s->timeLeft);
	NetRep_PutSVar(&w, s->endLeft);
	NetRep_PutSVar(&w, s->red);
	NetRep_PutSVar(&w, s->green);
	NetRep_PutU8(&w, (uint32_t)(you >= 0 && you < ARENA_MEMBERS ? you : ARENA_NOBODY));
	NetRep_PutU8(&w, s->flag);
	for (int k = 0; k < 3; k++) {
		NetRep_PutSVar(&w, s->flagPos[k]);
	}
	NetRep_PutU8(&w, s->carrier);
	NetRep_PutStr(&w, s->zone);
	NetRep_PutStr(&w, s->next);
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		count += s->m[k].used != 0;
	}
	NetRep_PutU8(&w, (uint32_t)count);
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		const ArenaMember *m = &s->m[k];
		if (!m->used) {
			continue;
		}
		NetRep_PutU8(&w, (uint32_t)k);
		NetRep_PutU8(&w, m->flags);
		NetRep_PutU8(&w, m->team);
		NetRep_PutSVar(&w, m->score);
		NetRep_PutU16(&w, m->slot);
		NetRep_PutStr(&w, m->name);
	}
	return w.bad ? 0 : w.size;
}

byte Arena_ReadState(const uint8_t *buf, int size, ArenaState *s) {
	NetRepReader r;
	ArenaState t;
	memset(&t, 0, sizeof(t));
	NetRepReader_Init(&r, buf, size);
	if (NetRep_GetU8(&r) != ARENA_MSG || NetRep_GetU8(&r) != ARENA_ST_STATE) {
		return 0;
	}
	t.mode = (uint8_t)NetRep_GetU8(&r);
	t.phase = (uint8_t)NetRep_GetU8(&r);
	t.started = (uint8_t)NetRep_GetU8(&r);
	t.round = (uint8_t)NetRep_GetU8(&r);
	t.fraglimit = NetRep_GetSVar(&r);
	t.timeLeft = NetRep_GetSVar(&r);
	t.endLeft = NetRep_GetSVar(&r);
	t.red = NetRep_GetSVar(&r);
	t.green = NetRep_GetSVar(&r);
	t.you = (uint8_t)NetRep_GetU8(&r);
	t.flag = (uint8_t)NetRep_GetU8(&r);
	for (int k = 0; k < 3; k++) {
		t.flagPos[k] = NetRep_GetSVar(&r);
	}
	t.carrier = (uint8_t)NetRep_GetU8(&r);
	NetRep_GetStr(&r, t.zone, sizeof(t.zone));
	NetRep_GetStr(&r, t.next, sizeof(t.next));
	int count = (int)NetRep_GetU8(&r);
	for (int i = 0; i < count && !r.bad; i++) {
		uint32_t k = NetRep_GetU8(&r);
		ArenaMember m;
		memset(&m, 0, sizeof(m));
		m.used = 1;
		m.flags = (uint8_t)NetRep_GetU8(&r);
		m.team = (uint8_t)NetRep_GetU8(&r);
		m.score = NetRep_GetSVar(&r);
		m.slot = (uint16_t)NetRep_GetU16(&r);
		NetRep_GetStr(&r, m.name, sizeof(m.name));
		if (k >= ARENA_MEMBERS || m.team > ARENA_TEAM_GREEN) {
			return 0;
		}
		t.m[k] = m;
	}
	if (r.bad || t.mode >= ARENA_MODES || t.phase > ARENA_PH_END || t.flag > ARENA_FL_CARRIED ||
		t.carrier > ARENA_MEMBERS || (t.you != ARENA_NOBODY && t.you >= ARENA_MEMBERS)) {
		return 0;
	}
	*s = t;
	return 1;
}

int Arena_WriteEvent(uint8_t *buf, int cap, int ev, int a, int b, int num) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, ARENA_MSG);
	NetRep_PutU8(&w, ARENA_ST_EVENT);
	NetRep_PutU8(&w, (uint32_t)ev);
	NetRep_PutU8(&w, (uint32_t)(a >= 0 && a < ARENA_MEMBERS ? a : ARENA_NOBODY));
	NetRep_PutU8(&w, (uint32_t)(b >= 0 && b < ARENA_MEMBERS ? b : ARENA_NOBODY));
	NetRep_PutSVar(&w, num);
	return w.bad ? 0 : w.size;
}

byte Arena_ReadEvent(const uint8_t *buf, int size, int *ev, int *a, int *b, int *num) {
	NetRepReader r;
	NetRepReader_Init(&r, buf, size);
	if (NetRep_GetU8(&r) != ARENA_MSG || NetRep_GetU8(&r) != ARENA_ST_EVENT) {
		return 0;
	}
	*ev = (int)NetRep_GetU8(&r);
	*a = (int)NetRep_GetU8(&r);
	*b = (int)NetRep_GetU8(&r);
	*num = NetRep_GetSVar(&r);
	*a = *a < ARENA_MEMBERS ? *a : -1;
	*b = *b < ARENA_MEMBERS ? *b : -1;
	return !r.bad && *ev >= 0 && *ev < ARENA_EV_COUNT;
}

int Arena_WriteJoin(uint8_t *buf, int cap, const char *name, int team, const char *ship) {
	NetRepBuf w;
	int len = (int)strlen(ship);
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, ARENA_MSG_REQ);
	NetRep_PutU8(&w, ARENA_RQ_JOIN);
	NetRep_PutStr(&w, name);
	NetRep_PutU8(&w, (uint32_t)team);
	/* the ship text may pass 255 bytes: u16 length */
	len = len > ARENA_SHIP_CFG - 1 ? ARENA_SHIP_CFG - 1 : len;
	NetRep_PutU16(&w, (uint32_t)len);
	for (int i = 0; i < len; i++) {
		NetRep_PutU8(&w, (uint8_t)ship[i]);
	}
	return w.bad ? 0 : w.size;
}

int Arena_WriteRequest(uint8_t *buf, int cap, int rq, int team) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, ARENA_MSG_REQ);
	NetRep_PutU8(&w, (uint32_t)rq);
	if (rq == ARENA_RQ_TEAM) {
		NetRep_PutU8(&w, (uint32_t)team);
	}
	return w.bad ? 0 : w.size;
}

byte Arena_ReadRequest(const uint8_t *buf, int size, int *rq, char *name, int ncap, int *team, char *ship, int scap) {
	NetRepReader r;
	NetRepReader_Init(&r, buf, size);
	if (NetRep_GetU8(&r) != ARENA_MSG_REQ) {
		return 0;
	}
	*rq = (int)NetRep_GetU8(&r);
	*team = ARENA_TEAM_NONE;
	if (*rq == ARENA_RQ_JOIN) {
		NetRep_GetStr(&r, name, ncap);
		*team = (int)NetRep_GetU8(&r);
		int len = (int)NetRep_GetU16(&r);
		if (len >= scap || len >= ARENA_SHIP_CFG) {
			return 0;
		}
		for (int i = 0; i < len; i++) {
			ship[i] = (char)NetRep_GetU8(&r);
		}
		ship[len] = 0;
	} else if (*rq == ARENA_RQ_TEAM) {
		*team = (int)NetRep_GetU8(&r);
	}
	return !r.bad && *rq >= ARENA_RQ_JOIN && *rq <= ARENA_RQ_TEAM;
}
