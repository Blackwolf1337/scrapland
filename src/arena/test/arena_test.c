/*
 * Unit test of the pure part of the PvP modes (include/scrap/arena.h): the modes, the scores of deaths and flags as
 * the original counts them, the start and the end of a round, the teams of joins, the next level, the order of the
 * scorer and the wire.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/arena.h"

static int checks, failed;

#define CHECK(c) Check((c), #c, __LINE__)

static void Check(int ok, const char *what, int line) {
	checks++;
	if (!ok) {
		failed++;
		printf("FAIL line %d: %s\n", line, what);
	}
}

static void Member(ArenaState *s, int k, int team, int score, const char *name) {
	s->m[k].used = 1;
	s->m[k].team = (uint8_t)team;
	s->m[k].score = score;
	s->m[k].flags = ARENA_MF_SHIP | ARENA_MF_JOINED;
	snprintf(s->m[k].name, sizeof(s->m[k].name), "%s", name);
}

static void Fresh(ArenaState *s, int mode) {
	memset(s, 0, sizeof(*s));
	s->mode = (uint8_t)mode;
	s->fraglimit = 20;
	s->you = ARENA_NOBODY;
}

static void Test_Modes(void) {
	CHECK(Arena_ModeOf("dm") == ARENA_DM);
	CHECK(Arena_ModeOf("DeathMatch") == ARENA_DM);
	CHECK(Arena_ModeOf("TEAMPOST") == ARENA_TEAMPOST);
	CHECK(Arena_ModeOf("teamdm") == ARENA_TDM);
	CHECK(Arena_ModeOf("tdm") == ARENA_TDM);
	CHECK(Arena_ModeOf("DeathMatchBots") == ARENA_DMBOTS);
	CHECK(Arena_ModeOf("sandbox") == -1);
	CHECK(Arena_ModeOf(NULL) == -1);
	CHECK(g_ArenaModes[ARENA_TEAMFLAG].flags == (ARENA_F_TEAM | ARENA_F_FLAG));
	CHECK(!strcmp(g_ArenaModes[ARENA_POSTMAN].module, "NPostman"));
}

static void Test_Kills(void) {
	ArenaState s;
	/* DeathMatch: +1 the killer, -1 a suicide */
	Fresh(&s, ARENA_DM);
	Member(&s, 0, ARENA_TEAM_NONE, 0, "a");
	Member(&s, 1, ARENA_TEAM_NONE, 0, "b");
	ArenaRules_Kill(&s, 1, 0);
	CHECK(s.m[0].score == 1 && s.m[1].score == 0);
	ArenaRules_Kill(&s, 1, -1);
	CHECK(s.m[1].score == -1);
	ArenaRules_Kill(&s, 0, 0);
	CHECK(s.m[0].score == 0);
	/* a killer that is no member: as a suicide */
	ArenaRules_Kill(&s, 0, 70);
	CHECK(s.m[0].score == -1);
	/* TeamDM: the team totals too, a team kill costs the killer and its team */
	Fresh(&s, ARENA_TDM);
	Member(&s, 0, ARENA_TEAM_RED, 0, "a");
	Member(&s, 1, ARENA_TEAM_GREEN, 0, "b");
	Member(&s, 2, ARENA_TEAM_RED, 0, "c");
	ArenaRules_Kill(&s, 1, 0);
	CHECK(s.m[0].score == 1 && s.red == 1 && s.green == 0);
	ArenaRules_Kill(&s, 2, 0);
	CHECK(s.m[0].score == 0 && s.red == 0);
	ArenaRules_Kill(&s, 1, -1);
	CHECK(s.m[1].score == -1 && s.green == -1);
	/* FlagHunt: the dead loses a point it has, the killer gets nothing */
	Fresh(&s, ARENA_FLAGHUNT);
	Member(&s, 0, ARENA_TEAM_NONE, 2, "a");
	Member(&s, 1, ARENA_TEAM_NONE, 0, "b");
	ArenaRules_Kill(&s, 0, 1);
	CHECK(s.m[0].score == 1 && s.m[1].score == 0);
	ArenaRules_Kill(&s, 1, 0);
	CHECK(s.m[1].score == 0 && s.m[0].score == 1);
	/* TeamFlag: and its team's point it has */
	Fresh(&s, ARENA_TEAMFLAG);
	Member(&s, 0, ARENA_TEAM_RED, 1, "a");
	Member(&s, 1, ARENA_TEAM_GREEN, 0, "b");
	s.red = 1;
	ArenaRules_Kill(&s, 0, 1);
	CHECK(s.m[0].score == 0 && s.red == 0);
	ArenaRules_Kill(&s, 0, 1);
	CHECK(s.m[0].score == 0 && s.red == 0);
	/* Postman counts no kills, TeamPost the killer's (no team total) */
	Fresh(&s, ARENA_POSTMAN);
	Member(&s, 0, ARENA_TEAM_NONE, 3, "a");
	Member(&s, 1, ARENA_TEAM_NONE, 0, "b");
	ArenaRules_Kill(&s, 0, 1);
	ArenaRules_Kill(&s, 1, -1);
	CHECK(s.m[0].score == 3 && s.m[1].score == 0);
	Fresh(&s, ARENA_TEAMPOST);
	Member(&s, 0, ARENA_TEAM_RED, 0, "a");
	Member(&s, 1, ARENA_TEAM_GREEN, 0, "b");
	ArenaRules_Kill(&s, 1, 0);
	CHECK(s.m[0].score == 1 && s.red == 0);
	/* a victim that is no member: nothing */
	ArenaRules_Kill(&s, 5, 0);
	CHECK(s.m[0].score == 1);
}

static void Test_Score(void) {
	ArenaState s;
	Fresh(&s, ARENA_TEAMPOST);
	Member(&s, 0, ARENA_TEAM_GREEN, 0, "a");
	ArenaRules_Score(&s, 0);
	CHECK(s.m[0].score == 10 && s.green == 1 && s.red == 0);
	Fresh(&s, ARENA_FLAGHUNT);
	Member(&s, 0, ARENA_TEAM_NONE, 0, "a");
	ArenaRules_Score(&s, 0);
	CHECK(s.m[0].score == 1 && s.red == 0 && s.green == 0);
	Fresh(&s, ARENA_TEAMFLAG);
	Member(&s, 0, ARENA_TEAM_RED, 0, "a");
	ArenaRules_Score(&s, 0);
	CHECK(s.m[0].score == 1 && s.red == 1);
}

static void Test_Round(void) {
	ArenaState s;
	Fresh(&s, ARENA_DM);
	Member(&s, 0, ARENA_TEAM_NONE, 0, "a");
	CHECK(!ArenaRules_Started(&s));
	Member(&s, 1, ARENA_TEAM_NONE, 0, "b");
	s.m[1].flags = ARENA_MF_JOINED;
	CHECK(!ArenaRules_Started(&s));
	s.m[1].flags |= ARENA_MF_SHIP;
	CHECK(ArenaRules_Started(&s));
	/* it stays started */
	s.m[1].flags = 0;
	CHECK(ArenaRules_Started(&s));
	Fresh(&s, ARENA_TDM);
	Member(&s, 0, ARENA_TEAM_RED, 0, "a");
	Member(&s, 1, ARENA_TEAM_RED, 0, "b");
	CHECK(!ArenaRules_Started(&s));
	Member(&s, 2, ARENA_TEAM_GREEN, 0, "c");
	s.m[2].flags |= ARENA_MF_SPECT;
	CHECK(!ArenaRules_Started(&s));
	s.m[2].flags &= (uint8_t)~ARENA_MF_SPECT;
	CHECK(ArenaRules_Started(&s));
	/* the ends */
	Fresh(&s, ARENA_DM);
	s.fraglimit = 3;
	Member(&s, 0, ARENA_TEAM_NONE, 2, "a");
	CHECK(!ArenaRules_Ended(&s));
	s.m[0].score = 3;
	CHECK(ArenaRules_Ended(&s));
	s.m[0].flags |= ARENA_MF_SPECT;
	CHECK(!ArenaRules_Ended(&s));
	Fresh(&s, ARENA_TDM);
	s.fraglimit = 3;
	Member(&s, 0, ARENA_TEAM_RED, 5, "a");
	CHECK(!ArenaRules_Ended(&s));
	s.green = 3;
	CHECK(ArenaRules_Ended(&s));
}

static void Test_Teams(void) {
	ArenaState s;
	Fresh(&s, ARENA_DM);
	CHECK(ArenaRules_JoinTeam(&s, ARENA_TEAM_GREEN, -1) == ARENA_TEAM_NONE);
	Fresh(&s, ARENA_TDM);
	CHECK(ArenaRules_JoinTeam(&s, ARENA_TEAM_RED, -1) == ARENA_TEAM_RED);
	CHECK(ArenaRules_JoinTeam(&s, 7, -1) == ARENA_TEAM_RED);
	Member(&s, 0, ARENA_TEAM_RED, 0, "a");
	CHECK(ArenaRules_JoinTeam(&s, ARENA_TEAM_RED, -1) == ARENA_TEAM_GREEN);
	CHECK(ArenaRules_JoinTeam(&s, ARENA_TEAM_RED, 0) == ARENA_TEAM_RED);
	Member(&s, 1, ARENA_TEAM_GREEN, 0, "b");
	CHECK(ArenaRules_JoinTeam(&s, ARENA_TEAM_GREEN, -1) == ARENA_TEAM_GREEN);
}

static void Test_Next(void) {
	char out[ARENA_LEVEL + 1];
	Arena_NextLevel("FZ,OM", "FZ", out, sizeof(out));
	CHECK(!strcmp(out, "OM"));
	Arena_NextLevel("FZ,OM", "om", out, sizeof(out));
	CHECK(!strcmp(out, "FZ"));
	Arena_NextLevel("FZ,OM", "MCA", out, sizeof(out));
	CHECK(!strcmp(out, "FZ"));
	Arena_NextLevel("", "MCA", out, sizeof(out));
	CHECK(!strcmp(out, "MCA"));
	Arena_NextLevel("MCA", "MCA", out, sizeof(out));
	CHECK(!strcmp(out, "MCA"));
	Arena_NextLevel(",QF,,CA,", "CA", out, sizeof(out));
	CHECK(!strcmp(out, "QF"));
}

static void Test_Rows(void) {
	ArenaState s;
	int rows[ARENA_MEMBERS];
	Fresh(&s, ARENA_TDM);
	Member(&s, 0, ARENA_TEAM_RED, 2, "a");
	Member(&s, 1, ARENA_TEAM_GREEN, 5, "b");
	Member(&s, 2, ARENA_TEAM_GREEN, 5, "c");
	Member(&s, 3, ARENA_TEAM_RED, 4, "d");
	Member(&s, 4, ARENA_TEAM_RED, 9, "e");
	s.m[4].flags |= ARENA_MF_SPECT;
	s.red = 6;
	s.green = 10;
	int n = Arena_Rows(&s, 1, rows, ARENA_MEMBERS);
	CHECK(n == 5);
	/* green leads: b (you) before c at an equal score, then red d, a, then the spectator */
	CHECK(rows[0] == 1 && rows[1] == 2 && rows[2] == 3 && rows[3] == 0 && rows[4] == 4);
	n = Arena_Rows(&s, -1, rows, ARENA_MEMBERS);
	CHECK(rows[0] == 2 && rows[1] == 1);
	s.red = 10;
	n = Arena_Rows(&s, -1, rows, ARENA_MEMBERS);
	CHECK(rows[0] == 3 && rows[1] == 0 && rows[2] == 2);
	CHECK(Arena_Rows(&s, -1, rows, 2) == 2);
}

static void Test_Wire(void) {
	ArenaState s, t;
	uint8_t buf[8192];
	Fresh(&s, ARENA_TEAMPOST);
	s.phase = ARENA_PH_PLAY;
	s.started = 1;
	s.round = 3;
	s.timeLeft = -1;
	s.endLeft = 7;
	s.red = 4;
	s.green = -2;
	s.flag = ARENA_FL_CARRIED;
	s.flagPos[0] = -300000;
	s.flagPos[1] = 12;
	s.flagPos[2] = 700000;
	s.carrier = 3;
	strcpy(s.zone, "07");
	strcpy(s.next, "OM");
	Member(&s, 2, ARENA_TEAM_RED, 31, "Ann-Lee K");
	Member(&s, 70, ARENA_TEAM_GREEN, -4, "[Rouco]");
	s.m[70].flags |= ARENA_MF_BOT;
	s.m[2].slot = 1234;
	int n = Arena_WriteState(buf, sizeof(buf), &s, 2);
	CHECK(n > 0);
	CHECK(Arena_ReadState(buf, n, &t));
	CHECK(t.mode == ARENA_TEAMPOST && t.phase == ARENA_PH_PLAY && t.started && t.round == 3 && t.you == 2);
	CHECK(t.timeLeft == -1 && t.endLeft == 7 && t.red == 4 && t.green == -2 && t.fraglimit == 20);
	CHECK(t.flag == ARENA_FL_CARRIED && t.flagPos[0] == -300000 && t.flagPos[2] == 700000 && t.carrier == 3);
	CHECK(!strcmp(t.zone, "07") && !strcmp(t.next, "OM"));
	CHECK(t.m[2].used && t.m[2].score == 31 && t.m[2].slot == 1234 && !strcmp(t.m[2].name, "Ann-Lee K"));
	CHECK(t.m[70].used && (t.m[70].flags & ARENA_MF_BOT) && t.m[70].team == ARENA_TEAM_GREEN && t.m[70].score == -4);
	CHECK(!t.m[0].used && !t.m[3].used);
	/* cut short or too small: refused, the state kept */
	CHECK(!Arena_ReadState(buf, n - 1, &t) && t.m[2].used);
	CHECK(Arena_WriteState(buf, 20, &s, 2) == 0);
	/* all members */
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		Member(&s, k, k & 1 ? ARENA_TEAM_RED : ARENA_TEAM_GREEN, k, "a name of thirty one characters");
	}
	n = Arena_WriteState(buf, sizeof(buf), &s, ARENA_NOBODY);
	CHECK(n > 0 && n < 4096 && Arena_ReadState(buf, n, &t) && t.you == ARENA_NOBODY && t.m[71].score == 71);
	/* events */
	int ev, a, b, num;
	n = Arena_WriteEvent(buf, sizeof(buf), ARENA_EV_KILL, 4, -1, 0);
	CHECK(n > 0 && Arena_ReadEvent(buf, n, &ev, &a, &b, &num) && ev == ARENA_EV_KILL && a == 4 && b == -1);
	n = Arena_WriteEvent(buf, sizeof(buf), 200, 4, 5, 0);
	CHECK(!Arena_ReadEvent(buf, n, &ev, &a, &b, &num));
	/* requests */
	char name[ARENA_NAME + 1], ship[ARENA_SHIP_CFG];
	int rq, team;
	n = Arena_WriteJoin(buf, sizeof(buf), "pa", ARENA_TEAM_GREEN, "SBetty1\n80\npa\nP_Betty\nMBetty1\n\n\n\n1,1,0\n2");
	CHECK(n > 0 && Arena_ReadRequest(buf, n, &rq, name, sizeof(name), &team, ship, sizeof(ship)));
	CHECK(rq == ARENA_RQ_JOIN && team == ARENA_TEAM_GREEN && !strcmp(name, "pa") && !strncmp(ship, "SBetty1\n80", 10));
	CHECK(!Arena_ReadRequest(buf, n, &rq, name, sizeof(name), &team, ship, 8));
	n = Arena_WriteRequest(buf, sizeof(buf), ARENA_RQ_TEAM, ARENA_TEAM_RED);
	CHECK(n == 3 && Arena_ReadRequest(buf, n, &rq, name, sizeof(name), &team, ship, sizeof(ship)) && rq == ARENA_RQ_TEAM &&
		team == ARENA_TEAM_RED);
	n = Arena_WriteRequest(buf, sizeof(buf), ARENA_RQ_SPECT, 0);
	CHECK(n == 2 && Arena_ReadRequest(buf, n, &rq, name, sizeof(name), &team, ship, sizeof(ship)) && rq == ARENA_RQ_SPECT);
	buf[1] = 9;
	CHECK(!Arena_ReadRequest(buf, n, &rq, name, sizeof(name), &team, ship, sizeof(ship)));
}

int main(void) {
	Test_Modes();
	Test_Kills();
	Test_Score();
	Test_Round();
	Test_Teams();
	Test_Next();
	Test_Rows();
	Test_Wire();
	printf("arena_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
