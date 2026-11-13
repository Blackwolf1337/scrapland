/*
 * The client's side of the PvP modes (include/scrap/arena.h): the join, the requests, the keys, the flag or mail bag
 * as local items, the names and team colours of the proxies and the original's HUD of the modes (the scores of
 * RacerScorer, the mission scorer, the texts of Ffa and the n*.py modes) from the instance's state. Not in the
 * original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/arena.h"
#include "scrap/camcontrol.h"
#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/texts.h"
#include "scrap/world.h"

#define AC_FLAG "ArenaFlag"				/* the local flag or bag item (the original's FlagToBeHunted) */
#define AC_ZONE "ArenaZone"				/* the local zone items of the postman modes, AC_ZONE<key> */
#define AC_CAM "ArenaCam"				/* the free camera of a spectator (Ffa.CameraSpectator) */
#define AC_ZONES 32
#define AC_MSG 256
#define AC_PROXY_SECS 0.5				/* the names and colours of the proxies set again this often */
#define AC_BOT_FIRE 1.5					/* s a headless player or a test bot waits before it asks for a ship */

static const int ac_usr[3] = { 255, 255, 0 }, ac_std[3] = { 255, 255, 255 };	/* Ffa.USRCOLOR, STDCOLOR */
static const int ac_red[4] = { 255, 0, 0, 128 }, ac_green[4] = { 0, 128, 0, 128 };	/* REDCOLOR, GREENCOLOR */

static struct {
	byte have;
	ArenaState st;
	byte joined;					/* the join request went */
	byte freeCam;					/* the free camera is on (Ac_SpectCam) */
	byte showing;					/* the scores are up */
	byte held;						/* the scores held with the Scorer key */
	byte dirty;						/* the state changed since the HUD was set */
	byte fire, keys[3];				/* the keys of the frame before */
	double lost;					/* local time the own ship went (0: it has one) */
	double proxyAt;
	double rejoinUntil, lastCount;	/* the count down of a team change */
	int yourPos, yourScore, winner;	/* Ffa.YourPosition, YourScore, Winner (the place labels) */
	char zones[AC_ZONES][ARENA_ZONE + 1];
	vec3_t zonePos[AC_ZONES];
	int nzones;
	byte zonesMade, flagMade;
	char lastWay[64];
	char deathTop[AC_MSG];			/* the text of the own death while it has no ship (Ffa.ShowPresFireMessage) */
	double botStart;				/* SCRAP_ARENA_REQ: local time of the first state */
	uint32_t botDone;
} ac = { .yourPos = 1 };

static double Ac_Now(void) {
	return (double)Plat_RealNs() / 1e9;
}

byte Arena_ClientOn(void) {
	return ac.have;
}

static int Ac_Flags(void) {
	return g_ArenaModes[ac.st.mode].flags;
}

static int Ac_You(void) {
	return ac.st.you != ARENA_NOBODY ? ac.st.you : -1;
}

static const char *Ac_Name(int k) {
	return k >= 0 && k < ARENA_MEMBERS && ac.st.m[k].used ? ac.st.m[k].name : "";
}

void Arena_ClientReset(void) {
	if (World_FindEntity(AC_CAM) && ac.freeCam) {
		cEntity_Del(World_FindEntity(AC_CAM));
	}
	memset(&ac, 0, sizeof(ac));
	ac.yourPos = 1;
}

/* ---- texts ---- */

static void Ac_Msg(const char *text, double secs, byte label) {
	nativeVal_t v;
	Con_Printf("arena: %s\n", text);
	if (g_HeadlessPlay || !text[0]) {
		return;
	}
	Native_SetStr(&v, text);
	if (label) {
		Mode_LabelText(&v, Mode_Time() + secs);
	} else {
		Mode_MsgText(&v, Mode_Time() + secs);
	}
	Native_Free(&v);
}

/* a language text of the original (or of ScrapC's table) with its %s, %d and %i filled in order with a and b (the
 * numbers as text: the original's texts are Python formats of one value or of a pair) */
static void Ac_Lang(char *out, int cap, const char *key, const char *a, const char *b) {
	char fmt[AC_MSG];
	Str_Copy(fmt, Script_Lang(key), sizeof(fmt));
	for (char *p = fmt; (p = strchr(p, '%')) != NULL; p += 2) {
		if (p[1] == 'd' || p[1] == 'i') {
			p[1] = 's';
		} else if (!p[1]) {
			break;
		}
	}
	Texts_Fill(out, cap, fmt, 0, a, b);
}

static const char *Ac_Num(char *buf, int cap, int n) {
	Str_PrintfQuiet(buf, cap, "%d", n);
	return buf;
}

static void Ac_Sound(byte mine) {
	if (!g_HeadlessPlay && Mode_Import("Sound")) {
		if (!(mine ? Mode_Sound("WaySound", "WayVolume") : Mode_Sound("EneWaySound", "EneWayVolume"))) {
			Native_ErrPrint();
		}
	}
}

/* the title of the mode (GetGameDescriptor): its name, the score of the teams in a team mode */
static void Ac_Title(char *out, int cap) {
	const ArenaState *s = &ac.st;
	char key[64];
	char a[16], b[16];
	if (Ac_Flags() & ARENA_F_TEAM) {
		/* Ffa.GetGameTeamDescriptor: tied, else the leading team with its total first */
		byte red = s->red > s->green;
		if (s->red == s->green) {
			Ac_Lang(out, cap, "Net_Title_Teams_Tied", Ac_Num(a, sizeof(a), s->green), NULL);
		} else {
			Ac_Lang(out, cap, red ? "Net_Title_Red_Wins" : "Net_Title_Green_Wins",
				Ac_Num(a, sizeof(a), red ? s->red : s->green), Ac_Num(b, sizeof(b), red ? s->green : s->red));
		}
		return;
	}
	Str_PrintfQuiet(key, sizeof(key), "Net_Title_%s", g_ArenaModes[s->mode == ARENA_DMBOTS ? ARENA_DM : s->mode].type);
	Str_Copy(out, Script_Lang(key), cap);
}

/* ---- the requests ---- */

static void Ac_Request(int rq, int team) {
	uint8_t buf[8];
	int n = Arena_WriteRequest(buf, sizeof(buf), rq, team);
	if (n > 0 && Watch_SendReliable(buf, n)) {
		Con_Printf("arena: request %d %d\n", rq, team);
	}
}

/* the join: the name, the team and the garage ship (Ffa.BuildCfgString: the engine variables of the ship) */
static void Ac_Join(void) {
	static const char *const cfg[10] = { "PlayerModel", "PlayerMaxLife", "PlayerName", "PilotModel", "Motor0Model",
		"Motor1Model", "Motor2Model", "Motor3Model", "WeaponBayList", "PlayerTeamID" };
	char ship[ARENA_SHIP_CFG];
	uint8_t buf[ARENA_SHIP_CFG + 64];
	int n = 0;
	for (int i = 0; i < 10; i++) {
		if (i == 1 || i == 9) {
			Str_PrintfQuiet(ship + n, (int)sizeof(ship) - n, "%s%d", i ? "\n" : "", (int)GVar_GetNum(cfg[i], 0));
		} else {
			Str_PrintfQuiet(ship + n, (int)sizeof(ship) - n, "%s%s", i ? "\n" : "", GVar_GetStr(cfg[i]));
		}
		n += (int)strlen(ship + n);
	}
	/* PlayerTeamID: 0 red, 2 green (4 no team) */
	int team = (int)GVar_GetNum("PlayerTeamID", 0) == 2 ? ARENA_TEAM_GREEN : ARENA_TEAM_RED;
	const char *env = getenv("SCRAP_ARENA_TEAM");
	if (env && *env) {
		team = *env == 'g' ? ARENA_TEAM_GREEN : ARENA_TEAM_RED;
	}
	/* the name of the garage (a coordinator's world names the player by its ticket); tests: SCRAP_PLAYER_NAME */
	const char *name = getenv("SCRAP_PLAYER_NAME");
	name = name && *name ? name : GVar_GetStr("PlayerName");
	int size = Arena_WriteJoin(buf, sizeof(buf), name, team, ship);
	if (size > 0 && Watch_SendReliable(buf, size)) {
		ac.joined = 1;
		Con_Printf("arena: join as %s, team %d\n", name, team);
	}
}

/* ---- the flag, the bag and the zones ---- */

static cEntity *Ac_Item(const char *name, const vec3_t *p) {
	cEntity *e = World_FindEntity(name);
	return e ? e : cWorld_CreateEntity(g_pWorld, name, p->x, p->y, p->z, "Item");
}

static void Ac_SetS(cEntity *e, const char *var, const char *v) {
	char *s = (char *)v;
	Entity_VarS(e, var, &s, 1);
}

static void Ac_SetI(cEntity *e, const char *var, int v) {
	Entity_VarI(e, var, &v, 1);
}

/* a model of the mode's module (FlagModel, FlagPilotModel, FlagNoUsrModel) */
static void Ac_Model(cEntity *e, const char *var) {
	char model[128];
	if (Arena_Module(ac.st.mode) && Mode_ModGetS(g_ArenaModes[ac.st.mode].module, var, model, sizeof(model)) &&
		model[0]) {
		Entity_SetModel(e, model, 1.0f, 1.0f, 1.0f, 1);
	}
	Native_ErrClear();
}

static void Ac_Way(const char *name) {
	if (strcmp(ac.lastWay, name)) {
		Str_Copy(ac.lastWay, name, sizeof(ac.lastWay));
		Mode_GVarSetS("CurrentWaypointName", name);
	}
}

/* the zones of the postman modes: an item of radius 3000 at each DM_Domination_Zone_ dummy (NPostman.BrowseAreas) */
static void Ac_MakeZones(void) {
	char name[64], ent[96];
	vec3_t p, ang;
	ac.zonesMade = 1;
	ac.nzones = 0;
	DummySearch_Start("DM_Domination_Zone_*", 1);
	while (ac.nzones < AC_ZONES && DummySearch_NextEuler(name, sizeof(name), &p, &ang)) {
		Str_Copy(ac.zones[ac.nzones], (int)strlen(name) > 19 ? name + 19 : name, ARENA_ZONE + 1);
		ac.zonePos[ac.nzones] = p;
		Str_PrintfQuiet(ent, sizeof(ent), AC_ZONE "%s", ac.zones[ac.nzones]);
		cEntity *z = g_HeadlessPlay ? NULL : Ac_Item(ent, &p);
		if (z) {
			Ac_SetI(z, "Radius", (int)ARENA_ZONE_RADIUS);
			Ac_SetI(z, "RadarShow", 0);
		}
		ac.nzones++;
	}
}

static void Ac_Flag(void) {
	const ArenaState *s = &ac.st;
	vec3_t p = { (float)s->flagPos[0], (float)s->flagPos[1], (float)s->flagPos[2] };
	char ent[96], link[96];
	if (!(Ac_Flags() & (ARENA_F_FLAG | ARENA_F_POST)) || s->flag == ARENA_FL_NONE) {
		return;
	}
	cEntity *f = Ac_Item(AC_FLAG, &p);
	if (!f) {
		return;
	}
	if (!ac.flagMade) {
		ac.flagMade = 1;
		Ac_Model(f, "FlagModel");
		Ac_SetI(f, "Radius", (int)ARENA_FLAG_RADIUS);
		Ac_SetI(f, "RadarShow", 1);
	}
	if (Ac_Flags() & ARENA_F_POST) {
		if (!ac.zonesMade) {
			Ac_MakeZones();
		}
		for (int z = 0; z < ac.nzones; z++) {
			Str_PrintfQuiet(ent, sizeof(ent), AC_ZONE "%s", ac.zones[z]);
			cEntity *e = World_FindEntity(ent);
			if (e) {
				Ac_SetI(e, "RadarShow", s->flag == ARENA_FL_CARRIED && !strcmp(s->zone, ac.zones[z]) ? 2 : 0);
			}
		}
		int k = s->carrier - 1;
		cEntity *c = k >= 0 && s->m[k].slot ? Watch_SlotEntity(s->m[k].slot - 1) : NULL;
		if (s->flag == ARENA_FL_CARRIED && c) {
			/* NPostman.TakeFlag: on the carrier's DC_Flag_Player, the model without the pilot for the own one */
			Str_PrintfQuiet(link, sizeof(link), "%s,DC_Flag_Player", c->name);
			char *cur = NULL;
			if (!Entity_VarS(f, "LinkModel", &cur, 0) || !cur || strcmp(cur, link)) {
				Ac_Model(f, k == Ac_You() ? "FlagNoUsrModel" : "FlagPilotModel");
				Ac_SetS(f, "LinkModel", link);
				Ac_SetI(f, "DeltaOffMul", 4);
				Ac_SetI(f, "OffMul", 0);
				Ac_SetI(f, "UseLinkAng", 1);
			}
		} else {
			char *cur = NULL;
			if (Entity_VarS(f, "LinkModel", &cur, 0) && cur && strcmp(cur, ",")) {
				Ac_SetS(f, "LinkModel", ",");
				Ac_Model(f, "FlagModel");
			}
			f->pos = p;
		}
		/* the way point: the zone for the carrier (its team in TeamPost), else the bag */
		byte mine = k >= 0 && (k == Ac_You() || ((Ac_Flags() & ARENA_F_TEAM) && Ac_You() >= 0 &&
			s->m[k].team == s->m[Ac_You()].team));
		Str_PrintfQuiet(ent, sizeof(ent), AC_ZONE "%s", s->zone);
		Ac_Way(s->flag == ARENA_FL_CARRIED && mine && s->zone[0] ? ent : AC_FLAG);
		return;
	}
	f->pos = p;
	Ac_Way(AC_FLAG);
}

/* ---- the proxies ---- */

static void Ac_Proxies(void) {
	static const char *const tg[3] = { "killer", "red", "green" };
	double now = Ac_Now();
	if (now < ac.proxyAt && !ac.dirty) {
		return;
	}
	ac.proxyAt = now + AC_PROXY_SECS;
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		const ArenaMember *m = &ac.st.m[k];
		cEntity *e = m->used && m->slot ? Watch_SlotEntity(m->slot - 1) : NULL;
		if (!e) {
			continue;
		}
		char *cur = NULL;
		if (!Entity_VarS(e, "Descriptor", &cur, 0) || !cur || strcmp(cur, m->name)) {
			Ac_SetS(e, "Descriptor", m->name);
		}
		cur = NULL;
		if (!Entity_VarS(e, "TgType", &cur, 0) || !cur || strcmp(cur, tg[m->team])) {
			Ac_SetS(e, "TgType", tg[m->team]);
		}
	}
}

/* ---- the HUD ---- */

static void Ac_Tuple(nativeVal_t *t, const int *v, int n) {
	Native_SetEmpty(t, NV_TUPLE);
	for (int i = 0; i < n; i++) {
		Native_SetInt(Native_Append(t), v[i]);
	}
}

/* the rows of RacerScorer.ShowScorer (Ffa.GetScorerInfo): score, the bag's mark and the name, the colour, the back,
 * the player; the own place and score, the lead (Ffa.isTied: 1 alone, 0 tied, -1 not on top) */
static int Ac_Rows(nativeVal_t *list, int *pos, int *score, int *winner) {
	static const int none[4] = { 0, 0, 0, 0 };
	int rows[ARENA_MEMBERS], you = Ac_You();
	int n = Arena_Rows(&ac.st, you, rows, ARENA_MEMBERS);
	Native_SetEmpty(list, NV_LIST);
	*pos = 1;
	*score = 0;
	*winner = -1;
	for (int r = 0; r < n; r++) {
		const ArenaMember *m = &ac.st.m[rows[r]];
		char name[ARENA_NAME + 3];
		nativeVal_t *row = Native_Append(list);
		Native_SetEmpty(row, NV_TUPLE);
		Native_SetInt(Native_Append(row), (m->flags & ARENA_MF_SPECT) ? ARENA_SPECTATOR_POINTS : m->score);
		Str_PrintfQuiet(name, sizeof(name), "%s%s", ac.st.carrier == rows[r] + 1 ? "*" : " ", m->name);
		Native_SetStr(Native_Append(row), name);
		Ac_Tuple(Native_Append(row), rows[r] == you ? ac_usr : ac_std, 3);
		Ac_Tuple(Native_Append(row), m->team == ARENA_TEAM_RED ? ac_red : m->team == ARENA_TEAM_GREEN ? ac_green : none,
			4);
		Native_SetStr(Native_Append(row), m->name);
		if (rows[r] == you) {
			*pos = r + 1;
			*score = m->score;
		}
	}
	if (n && you >= 0) {
		int top = ac.st.m[rows[0]].score, same = 0;
		for (int r = 0; r < n; r++) {
			same += ac.st.m[rows[r]].score == top && !(ac.st.m[rows[r]].flags & ARENA_MF_SPECT);
		}
		*winner = *score != top ? -1 : same == 1 ? 1 : 0;
	}
	return n;
}

/* the scores on screen (RacerScorer.ShowScorer) with the mode's title and the bottom text */
static void Ac_ShowScores(const char *top, const char *bottom) {
	nativeVal_t list, t, b;
	int pos, score, winner;
	if (g_HeadlessPlay || Mode_ActionSetIs("Menu") || !Mode_Import("RacerScorer")) {
		return;
	}
	Ac_Rows(&list, &pos, &score, &winner);
	Native_SetStr(&t, top);
	Native_SetStr(&b, bottom);
	if (!Mode_SetModVar("RacerScorer", "ScorerTopMessage", &t) || !Mode_SetModVar("RacerScorer", "ScorerBotMessage", &b) ||
		!PORT_CALL(RacerScorer, ShowScorer, NULL, "iv", 0L, &list)) {
		Native_ErrPrint();
	}
	Native_Free(&list);
	Native_Free(&t);
	Native_Free(&b);
	ac.showing = 1;
}

static void Ac_Set(const char *item, const char *var, double v) {
	Script_SetNum(0, item, var, v);
}

/* the mission scorer of the racer HUD (Ffa.FfaScorer, SetScorerValues): the limit, the team totals or the best and
 * the own score */
static void Ac_Mission(void) {
	char t[32];
	const ArenaState *s = &ac.st;
	if (!Script_HasItem(0, "MissionLimit")) {
		if (!Mode_Import("RacerScorer") || !PORT_CALL(RacerScorer, MissionScorer, NULL, "i", 0L) ||
			((Ac_Flags() & ARENA_F_TEAM) && !PORT_CALL(RacerScorer, MissionScorerTeam, NULL, "i", 0L))) {
			Native_ErrPrint();
			return;
		}
	}
	Str_PrintfQuiet(t, sizeof(t), "%d", s->fraglimit);
	Script_SetStr(0, "MissionLimit", "Text", t);
	if (Ac_Flags() & ARENA_F_TEAM) {
		byte r = s->red > s->green;
		const char *rc[2] = { r ? "MissionSLimit" : "MissionYou", r ? "MissionBest" : "MissionBad" };
		const char *gc[2] = { r ? "MissionYou" : "MissionSLimit", r ? "MissionBad" : "MissionBest" };
		Ac_Set(rc[0], "Green", 0);
		Ac_Set(rc[0], "Red", 255);
		Str_PrintfQuiet(t, sizeof(t), "%d", s->red);
		Script_SetStr(0, rc[1], "Text", t);
		Ac_Set(gc[0], "Green", 255);
		Ac_Set(gc[0], "Red", 0);
		Str_PrintfQuiet(t, sizeof(t), "%d", s->green);
		Script_SetStr(0, gc[1], "Text", t);
		return;
	}
	int rows[ARENA_MEMBERS], you = Ac_You();
	int n = Arena_Rows(s, you, rows, ARENA_MEMBERS);
	if (!n || you < 0) {
		Script_SetStr(0, "MissionBest", "Text", "-");
		Script_SetStr(0, "MissionBad", "Text", "-");
		Ac_Set("MissionYou", "Blue", 0);
		Ac_Set("MissionYou", "Red", 0);
		return;
	}
	int mine = s->m[you].score, top = s->m[rows[0]].score;
	if (mine == top) {
		int second = n > 1 && !(s->m[rows[1]].flags & ARENA_MF_SPECT) ? s->m[rows[1]].score : 0;
		Str_PrintfQuiet(t, sizeof(t), "%d", mine);
		Script_SetStr(0, "MissionBest", "Text", t);
		Str_PrintfQuiet(t, sizeof(t), "%d", second);
		Script_SetStr(0, "MissionBad", "Text", t);
		Ac_Set("MissionYou", "Y", 180);
		Ac_Set("MissionYou", "Blue", 255);
		Ac_Set("MissionYou", "Red", 0);
	} else {
		Str_PrintfQuiet(t, sizeof(t), "%d", top);
		Script_SetStr(0, "MissionBest", "Text", t);
		Str_PrintfQuiet(t, sizeof(t), "%d", mine);
		Script_SetStr(0, "MissionBad", "Text", t);
		Ac_Set("MissionYou", "Y", 200);
		Ac_Set("MissionYou", "Blue", 0);
		Ac_Set("MissionYou", "Red", 255);
	}
}

/* Ffa.OnPositionChange: the labels of a change of the lead or of the place (the modes without teams) */
static void Ac_Places(void) {
	nativeVal_t list;
	char text[AC_MSG], place[16];
	int pos, score, winner;
	if (Ac_You() < 0 || (ac.st.m[Ac_You()].flags & ARENA_MF_SPECT)) {
		return;
	}
	Ac_Rows(&list, &pos, &score, &winner);
	Native_Free(&list);
	if (pos == ac.yourPos && score == ac.yourScore && winner == ac.winner) {
		return;
	}
	if (!(Ac_Flags() & ARENA_F_TEAM)) {
		if (winner != ac.winner || (score != ac.yourScore && ac.winner != -1)) {
			Ac_Msg(Script_Lang(winner == -1 ? "Net_Label_Lost_Lead" : winner == 0 ? "Net_Label_Tied_Lead" :
				"Net_Label_Tied_Leading"), 5.0, 1);
		} else {
			Str_PrintfQuiet(place, sizeof(place), pos == 1 ? "1st" : pos == 2 ? "2nd" : "%dth", pos);
			Ac_Lang(text, sizeof(text), pos < ac.yourPos ? "Net_Label_Up_to" : pos > ac.yourPos ? "Net_Label_Down_to" :
				"Net_Label_Still_to", place, NULL);
			Ac_Msg(text, 5.0, 1);
		}
	}
	ac.yourPos = pos;
	ac.yourScore = score;
	ac.winner = winner;
}

/* the free camera of a spectator from the first spawn point (Ffa.CameraSpectator) */
static void Ac_SpectCam(byte on) {
	if (on == ac.freeCam) {
		return;
	}
	ac.freeCam = on;
	if (!on) {
		cEntity *c = World_FindEntity(AC_CAM);
		if (c) {
			cEntity_Del(c);
		}
		return;
	}
	char name[64];
	vec3_t p = { 0.0f, 0.0f, 0.0f }, ang = { 0.0f, 0.0f, 0.0f };
	DummySearch_Start("DM_Spawn*", 1);
	DummySearch_NextEuler(name, sizeof(name), &p, &ang);
	cEntity *c = World_FindEntity(AC_CAM);
	if (!c) {
		c = cWorld_CreateEntity(g_pWorld, AC_CAM, p.x, p.y + 500.0f, p.z, TYEN_FCAM);
	}
	if (c) {
		Ac_SetI(c, "FreeCam", 1);
		cWorld_SetCam(g_pWorld, 0, AC_CAM);
		Input_SelectActionSetNamed("Racer");
	}
}

/* the scores and the mission scorer as the state and the keys say */
static void Ac_Hud(void) {
	char top[AC_MSG], bottom[AC_MSG];
	const ArenaState *s = &ac.st;
	int you = Ac_You();
	byte spect = you >= 0 && (s->m[you].flags & ARENA_MF_SPECT);
	byte own = Play_Entity() != NULL;
	byte want = ac.held || s->phase == ARENA_PH_END || !own;
	Ac_Title(top, sizeof(top));
	bottom[0] = 0;
	if (s->phase == ARENA_PH_END) {
		if (!(Ac_Flags() & ARENA_F_TEAM)) {
			Str_Copy(top, Script_Lang("Net_Title_Game_Results"), sizeof(top));
		}
		Str_Copy(bottom, Script_Lang("Net_Bott_Wait_Next_Match"), sizeof(bottom));
	} else if (spect) {
		Str_Copy(bottom, "Spectator mode", sizeof(bottom));
	} else if (!own && ac.lost > 0.0) {
		Str_Copy(bottom, Script_Lang("Net_Bott_Press_Fire"), sizeof(bottom));
		if (ac.deathTop[0]) {
			Str_Copy(top, ac.deathTop, sizeof(top));
		}
	}
	/* again when the state changed or something else cleared the scorer (the death camera, a menu) */
	if (want && (ac.dirty || !ac.showing || !Script_HasItem(0, "PlyMsg"))) {
		Ac_ShowScores(top, bottom);
	} else if (!want && ac.showing && own) {
		ac.showing = 0;
		if (Mode_Import("RacerScorer") && !PORT_CALL(RacerScorer, Create, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
	}
	if (own && !ac.showing && (ac.dirty || !Script_HasItem(0, "MissionLimit"))) {
		Ac_Mission();
	}
}

/* ---- messages ---- */

static void Ac_Event(int ev, int a, int b, int num) {
	char text[AC_MSG];
	int you = Ac_You();
	const ArenaState *s = &ac.st;
	byte team = (Ac_Flags() & ARENA_F_TEAM) != 0;
	byte ours = a >= 0 && you >= 0 && (a == you || (team && s->m[a].team == s->m[you].team));
	switch (ev) {
	case ARENA_EV_KILL:
		if (b == you && you >= 0) {
			/* Ffa.OnDeath of the dead player of this machine */
			if (a >= 0 && a != b) {
				Ac_Lang(text, sizeof(text), "Net_Title_Death_Kill", Ac_Name(a), NULL);
			} else {
				Str_Copy(text, Script_Lang("Net_Title_Death_Suicide"), sizeof(text));
			}
			Str_Copy(ac.deathTop, text, sizeof(ac.deathTop));
			Ac_ShowScores(text, Script_Lang("Net_Bott_Press_Fire"));
			Con_Printf("arena: %s\n", text);
		} else if (a == you && you >= 0 && b >= 0) {
			Ac_Lang(text, sizeof(text), "Net_Msg_You_Kill", Ac_Name(b), NULL);
			Ac_Msg(text, 5.0, 0);
		} else {
			Con_Printf("arena: %s killed by %s\n", Ac_Name(b), a >= 0 ? Ac_Name(a) : "-");
		}
		break;
	case ARENA_EV_JOIN:
		if (a == you && num > 0) {
			ac.rejoinUntil = Ac_Now() + num;
			ac.lastCount = 0.0;
		}
		/* Ffa.PlayerJoinGame (not in the first seconds of the level, Ffa.PlayerMsgShow) */
		Texts_Fill(text, sizeof(text), s->m[a].team == ARENA_TEAM_RED ? "%s joins red team." : s->m[a].team ==
			ARENA_TEAM_GREEN ? "%s joins green team." : "%s joins the game.", 0, Ac_Name(a), NULL);
		Ac_Msg(text, 2.5, 0);
		break;
	case ARENA_EV_LEAVE:
		Texts_Fill(text, sizeof(text), "%s leaves the match.", 0, Ac_Name(a), NULL);
		Ac_Msg(text, 2.5, 0);
		break;
	case ARENA_EV_SPECT:
		Texts_Fill(text, sizeof(text), "%s becomes spectator.", 0, Ac_Name(a), NULL);
		Ac_Msg(text, 2.5, 0);
		break;
	case ARENA_EV_FLAG:
		Ac_Sound(ours);
		if (Ac_Flags() & ARENA_F_POST) {
			/* NPostman.TakeFlag: the label of the carrier, of its team, of the enemies (team modes) */
			if (a == you) {
				Ac_Msg(Script_Lang("Net_Msg_You_HaveFlag"), 5.0, 0);
			} else if (ours) {
				Ac_Msg(Script_Lang("Net_Msg_Your_TeamFlag"), 5.0, 0);
			} else if (team) {
				Ac_Msg(Script_Lang("Net_Msg_Enemy_TeamFlag"), 5.0, 0);
			} else {
				Con_Printf("arena: %s took the bag\n", Ac_Name(a));
			}
		} else {
			Con_Printf("arena: %s took the flag\n", Ac_Name(a));
		}
		break;
	case ARENA_EV_DROP:
		Ac_Sound(0);
		Con_Printf("arena: the bag dropped (%s)\n", Ac_Name(a));
		break;
	case ARENA_EV_BACK:
		Ac_Msg(Script_Lang("MP_Arena_BagBack"), 3.0, 0);
		break;
	case ARENA_EV_DELIVER:
		Ac_Sound(ours);
		Ac_Lang(text, sizeof(text), "MP_Arena_Delivered", Ac_Name(a), NULL);
		Ac_Msg(text, 3.0, 0);
		break;
	case ARENA_EV_WAIT:
		if (!g_HeadlessPlay) {
			Mode_Label(0, "Net_Label_Waiting_Challenge", 1);
		}
		Con_Printf("arena: waiting for a challenger\n");
		break;
	case ARENA_EV_END:
		/* Ffa.EndOfGame: the screen fades to noise */
		Con_Printf("arena: the round ended, next %s in %d s\n", s->next, num);
		if (!g_HeadlessPlay) {
			static const int clear[4] = { 0, 0, 0, 0 }, dark[4] = { 0, 0, 0, 128 };
			Mode_GVarSetN("NoiseFactorBegin", 0);
			Mode_GVarSetN("NoiseFactor", 32);
			Mode_GVarSetN("NoiseTime", 0);
			Mode_GVarSetN("NoiseEndTime", 1.0);
			Mode_Fade(0, clear, dark, 1.0);
		}
		break;
	case ARENA_EV_START:
		Con_Printf("arena: round %d\n", num);
		if (!g_HeadlessPlay) {
			static const int dark[4] = { 0, 0, 0, 128 }, clear[4] = { 0, 0, 0, 0 };
			Mode_GVarSetN("NoiseFactor", 0);
			Mode_Fade(0, dark, clear, 0.5);
		}
		break;
	default:
		break;
	}
}

byte Arena_BotGoal(float *at) {
	const ArenaState *s = &ac.st;
	int k = s->carrier - 1, you = Ac_You();
	if (!ac.have || !(Ac_Flags() & (ARENA_F_FLAG | ARENA_F_POST)) || s->flag == ARENA_FL_NONE ||
		s->phase == ARENA_PH_END) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		at[i] = (float)s->flagPos[i];
	}
	if (s->flag != ARENA_FL_CARRIED || you < 0 || k < 0) {
		return 1;
	}
	cEntity *c = s->m[k].slot ? Watch_SlotEntity(s->m[k].slot - 1) : NULL;
	if (k != you && c) {
		/* after the carrier */
		at[0] = c->pos.x;
		at[1] = c->pos.y;
		at[2] = c->pos.z;
	}
	if (k == you || ((Ac_Flags() & ARENA_F_TEAM) && s->m[k].team == s->m[you].team)) {
		if (!ac.zonesMade) {
			Ac_MakeZones();
		}
		for (int z = 0; z < ac.nzones; z++) {
			if (!strcmp(ac.zones[z], s->zone)) {
				at[0] = ac.zonePos[z].x;
				at[1] = ac.zonePos[z].y;
				at[2] = ac.zonePos[z].z;
			}
		}
	}
	return 1;
}

void Arena_ClientMessage(const uint8_t *data, int size) {
	int ev, a, b, num;
	if (size >= 2 && data[1] == ARENA_ST_STATE) {
		ArenaState s;
		if (!Arena_ReadState(data, size, &s)) {
			Con_Printf("arena: a bad state\n");
			return;
		}
		if (!ac.have || s.phase != ac.st.phase || s.round != ac.st.round) {
			Con_Printf("arena: %s round %d phase %d, %d to win, you %d\n", g_ArenaModes[s.mode].type, s.round, s.phase,
				s.fraglimit, s.you == ARENA_NOBODY ? -1 : s.you);
		}
		ac.st = s;
		ac.have = 1;
		ac.dirty = 1;
		return;
	}
	if (ac.have && Arena_ReadEvent(data, size, &ev, &a, &b, &num)) {
		Ac_Event(ev, a, b, num);
	}
}

/* the keys: fire for a new ship, QMsg1 spectate, QMsg2 and QMsg3 the teams (the original's team menu) */
static void Ac_Keys(byte own, byte spect) {
	uint32_t set = Input_GetActionSet();
	byte fire = Input_BoundValue(ACTSET_RACER, RACER_FIRE, 0) > 0.5f && set == ACTSET_RACER;
	byte keys[3];
	for (int k = 0; k < 3; k++) {
		keys[k] = Input_BoundValue(ACTSET_RACER, RACER_QMSG1 + k, 0) > 0.5f && set == ACTSET_RACER;
	}
	ac.held = Input_BoundValue(ACTSET_RACER, RACER_SCORER, 0) > 0.5f && set == ACTSET_RACER;
	double now = Ac_Now();
	/* a headless player and the test bots ask for their ship by themselves */
	const char *bot = getenv("SCRAP_PLAYBOT");
	byte autoFire = g_HeadlessPlay || (bot && *bot && *bot != '0');
	if (!own && !spect && ac.st.phase != ARENA_PH_END && ((fire && !ac.fire) ||
		(autoFire && ac.lost > 0.0 && now - ac.lost >= AC_BOT_FIRE))) {
		Ac_Request(ARENA_RQ_RESPAWN, 0);
		ac.lost = now;
	}
	if (keys[0] && !ac.keys[0]) {
		Ac_Request(ARENA_RQ_SPECT, 0);
	}
	if ((Ac_Flags() & ARENA_F_TEAM) && keys[1] && !ac.keys[1]) {
		Ac_Request(ARENA_RQ_TEAM, ARENA_TEAM_RED);
	}
	if ((Ac_Flags() & ARENA_F_TEAM) && keys[2] && !ac.keys[2]) {
		Ac_Request(ARENA_RQ_TEAM, ARENA_TEAM_GREEN);
	}
	ac.fire = fire;
	memcpy(ac.keys, keys, sizeof(keys));
}

/* tests: SCRAP_ARENA_REQ=<s>:spect|red|green|fire,... sends each request once, that many seconds after the first
 * state */
static void Ac_Bot(void) {
	const char *env = getenv("SCRAP_ARENA_REQ");
	char list[256];
	uint32_t k = 0;
	if (!env || !*env) {
		return;
	}
	double now = Ac_Now();
	if (ac.botStart == 0.0) {
		ac.botStart = now;
	}
	Str_Copy(list, env, sizeof(list));
	for (char *tok = strtok(list, ","); tok && k < 32; tok = strtok(NULL, ","), k++) {
		char *c = strchr(tok, ':');
		if (!c || (ac.botDone >> k & 1) || now - ac.botStart < crt_atof(tok)) {
			continue;
		}
		ac.botDone |= 1u << k;
		c++;
		if (!strcmp(c, "spect")) {
			Ac_Request(ARENA_RQ_SPECT, 0);
		} else if (!strcmp(c, "red") || !strcmp(c, "green")) {
			Ac_Request(ARENA_RQ_TEAM, *c == 'r' ? ARENA_TEAM_RED : ARENA_TEAM_GREEN);
		} else if (!strcmp(c, "fire")) {
			Ac_Request(ARENA_RQ_RESPAWN, 0);
		}
	}
}

void Arena_ClientFrame(void) {
	if (!g_Play || !ac.have || !Watch_ClockReady()) {
		return;
	}
	if (!ac.joined) {
		Ac_Join();
	}
	int you = Ac_You();
	byte spect = you >= 0 && (ac.st.m[you].flags & ARENA_MF_SPECT);
	byte own = Play_Entity() != NULL;
	double now = Ac_Now();
	if (own) {
		ac.lost = 0.0;
		ac.deathTop[0] = 0;
	} else if (ac.lost == 0.0) {
		ac.lost = now;
	}
	Ac_Keys(own, spect);
	Ac_Bot();
	/* the free camera while there is no own ship and no death camera: a spectator, a player before its first ship
	 * or at the end of a round */
	Ac_SpectCam(!own && !Play_DeathCamOn());
	if (ac.rejoinUntil > 0.0 && now - ac.lastCount >= 1.0) {
		/* Ffa.RejoinMsg: the count down of a team change */
		char text[AC_MSG];
		int left = (int)ceil(ac.rejoinUntil - now);
		ac.lastCount = now;
		if (left > 0) {
			char n[16];
			Ac_Lang(text, sizeof(text), "Net_Msg_Join_Time", Ac_Num(n, sizeof(n), left), NULL);
			Ac_Msg(text, 1.0, 0);
		} else {
			ac.rejoinUntil = 0.0;
		}
	}
	if (g_HeadlessPlay) {
		ac.dirty = 0;
		return;
	}
	Ac_Flag();
	Ac_Proxies();
	Ac_Places();
	Ac_Hud();
	ac.dirty = 0;
}
