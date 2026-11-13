/*
 * Unit test of the faction table and the wanted level rules (include/scrap/faction.h): the built-in table, its
 * errors, the lookups, the ranks and the wanted level of crimes and decay.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "scrap/faction.h"

static int checks, failed;

#define CHECK(c) Check((c), #c, __LINE__)

static void Check(int ok, const char *what, int line) {
	checks++;
	if (!ok) {
		failed++;
		printf("FAIL line %d: %s\n", line, what);
	}
}

static int Near(float a, float b) {
	return fabsf(a - b) < 1e-4f;
}

static FactionTable t;

static void Test_BuiltIn(void) {
	char err[128];
	static const char *const groups[] = { "Police", "Bishops", "Bankers", "Bureaucracy", "Press", "Mercs" };

	int n = FactionTable_Parse(&t, g_FactionTableText, err, sizeof(err));
	if (n < 0) {
		printf("built-in table: %s\n", err);
	}
	CHECK(n == 6);
	for (int i = 0; i < 6; i++) {
		CHECK(Faction_Find(&t, groups[i]) == i);
	}
	CHECK(Faction_Find(&t, "police") == 0);
	CHECK(Faction_Find(&t, "Nobody") == FACTION_NONE);
	const FactionDef *p = &t.f[0];
	CHECK(!strcmp(p->play, "Police"));
	CHECK(!strcmp(p->armour, "Gear") && p->armourLife == 300);
	CHECK(!strcmp(p->office, "Police"));
	CHECK(p->nships == 6 && !strcmp(p->ships[4], "SPoli5"));
	CHECK(p->nranks == 5 && p->ranks[4] == 20);
	for (int a = 0; a < FA_COUNT; a++) {
		CHECK(p->cooldown[a] >= 0.0f);
	}
	/* the other groups have no abilities in this stage */
	for (int i = 1; i < 6; i++) {
		for (int a = 0; a < FA_COUNT; a++) {
			CHECK(t.f[i].cooldown[a] < 0.0f);
		}
	}
	CHECK(Faction_OfClass(&t, "Gear") == 0);
	CHECK(Faction_OfClass(&t, "Sentinel") == 0);
	CHECK(Faction_OfClass(&t, "BankMaster") == 2);
	CHECK(Faction_OfClass(&t, "MercenaryB") == 5);
	CHECK(Faction_OfClass(&t, "Dtritus") == FACTION_NONE);
	CHECK(Faction_OfClass(&t, "") == FACTION_NONE);
	CHECK(Faction_OfShip(&t, "SPOLI3") == 0);
	CHECK(Faction_OfShip(&t, "spoliboss1") == 0);
	CHECK(Faction_OfShip(&t, "SHump1") == 4);
	CHECK(Faction_OfShip(&t, "SDtritus") == FACTION_NONE);
	CHECK(Faction_Relation(&t, 0, 5) == FREL_HOSTILE);
	CHECK(Faction_Relation(&t, 5, 0) == FREL_HOSTILE);
	CHECK(Faction_Relation(&t, 0, 3) == FREL_FRIEND);
	CHECK(Faction_Relation(&t, 0, 4) == FREL_NEUTRAL);
	CHECK(Faction_Relation(&t, 0, 0) == FREL_NEUTRAL);
	CHECK(Faction_Relation(&t, 0, FACTION_NONE) == FREL_NEUTRAL);
	CHECK(Near(t.wanted[0], 1.0f) && Near(t.wanted[4], 45.0f));
	CHECK(t.fine == 300 && t.reward == 150);
	CHECK(!strcmp(t.backupShip, "SPoli2_Rookie") && t.backupCount == 2);
	CHECK(Near(t.crime[FCRIME_KILL_PLAYER], 6.0f));
}

static void Test_Ranks(void) {
	const FactionDef *p = &t.f[0];
	CHECK(Faction_Rank(p, 0) == 1);
	CHECK(Faction_Rank(p, 1) == 1);
	CHECK(Faction_Rank(p, 2) == 2);
	CHECK(Faction_Rank(p, 9) == 3);
	CHECK(Faction_Rank(p, 10) == 4);
	CHECK(Faction_Rank(p, 20) == 5);
	CHECK(Faction_Rank(p, 1000) == 5);
}

static void Test_Errors(void) {
	FactionTable x;
	char err[128];
	CHECK(FactionTable_Parse(&x, "", err, sizeof(err)) == 0);
	CHECK(FactionTable_Parse(&x, "# only a comment\n\n", err, sizeof(err)) == 0);
	CHECK(FactionTable_Parse(&x, "chars Police\n", err, sizeof(err)) < 0 && strstr(err, "line 1"));
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\tability fly 3\n", err, sizeof(err)) < 0 &&
		strstr(err, "line 4") && strstr(err, "fly"));
	CHECK(FactionTable_Parse(&x, "faction A\n\toffice L\n", err, sizeof(err)) < 0 && strstr(err, "play"));
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\trelation B hostile\n", err, sizeof(err)) < 0 &&
		strstr(err, "unknown faction"));
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\nfaction a\n", err, sizeof(err)) < 0 &&
		strstr(err, "twice"));
	CHECK(FactionTable_Parse(&x, "wanted 1 2 3 4\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "wanted 1 2 2 4 5\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "crime steal 3\n", err, sizeof(err)) < 0 && strstr(err, "steal"));
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\tranks 1 2\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\tranks 0 5 3\n", err, sizeof(err)) < 0);
	/* a minimal faction, defaults and comments after statements, CR LF lines */
	int n = FactionTable_Parse(&x, "fine 10 # each\r\nfaction A\r\n\tplay X\r\n\toffice L D\r\n\tability ship 2\r\n",
		err, sizeof(err));
	CHECK(n == 1);
	CHECK(x.fine == 10 && x.reward == 150);
	CHECK(!strcmp(x.f[0].desk, "D") && !strcmp(x.f[0].office, "L"));
	CHECK(Near(x.f[0].cooldown[FA_SHIP], 2.0f) && x.f[0].cooldown[FA_SIREN] < 0.0f);
	CHECK(Faction_Rank(&x.f[0], 50) == 1);
	CHECK(!strcmp(Faction_AbilityName(FA_ARREST), "arrest") && !Faction_AbilityName(FA_COUNT));
	CHECK(!strcmp(Faction_CrimeName(FCRIME_KILL_PLAYER), "kill_player") && !Faction_CrimeName(-1));
}

static void Test_Wanted(void) {
	FactionWanted w;
	FactionWanted_Init(&w);
	CHECK(FactionWanted_Level(&t, &w) == 0);
	/* hurting a civilian: half a point, again within the gap does not count */
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_HURT_CIVILIAN, 10.0) == 1);
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_HURT_CIVILIAN, 10.5) == 0);
	CHECK(Near(w.points, 0.5f) && FactionWanted_Level(&t, &w) == 0);
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_HURT_CIVILIAN, 11.0) == 1);
	CHECK(FactionWanted_Level(&t, &w) == 1);
	/* another kind in the same second counts */
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_HURT_POLICE, 11.2) == 1);
	CHECK(Near(w.points, 3.0f) && FactionWanted_Level(&t, &w) == 1);
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_KILL_POLICE, 12.0) == 1);
	CHECK(FactionWanted_Level(&t, &w) == 2);
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_KILL_PLAYER, 12.0) == 1);
	CHECK(Near(w.points, 17.0f) && FactionWanted_Level(&t, &w) == 3);
	/* a crime of no points does not count; unknown ones neither */
	FactionTable z = t;
	z.crime[FCRIME_HURT_PLAYER] = 0.0f;
	CHECK(FactionWanted_Crime(&z, &w, FCRIME_HURT_PLAYER, 20.0) == 0);
	CHECK(FactionWanted_Crime(&t, &w, FCRIME_COUNT, 20.0) == 0);
	CHECK(FactionWanted_Crime(&t, &w, -1, 20.0) == 0);
	/* the cap: 1.5 times the top level */
	for (int i = 0; i < 40; i++) {
		FactionWanted_Crime(&t, &w, FCRIME_KILL_POLICE, 30.0 + i * 2.0);
	}
	CHECK(Near(w.points, 67.5f) && FactionWanted_Level(&t, &w) == 5);
	/* decay: slow with the alarm on, faster with it off, never below 0 */
	FactionWanted_Decay(&t, &w, 100.0f, 1);
	CHECK(Near(w.points, 65.5f));
	FactionWanted_Decay(&t, &w, 100.0f, 0);
	CHECK(Near(w.points, 55.5f) && FactionWanted_Level(&t, &w) == 5);
	FactionWanted_Decay(&t, &w, 150.0f, 0);
	CHECK(Near(w.points, 40.5f) && FactionWanted_Level(&t, &w) == 4);
	FactionWanted_Decay(&t, &w, 1000.0f, 0);
	CHECK(Near(w.points, 0.0f) && FactionWanted_Level(&t, &w) == 0);
	CHECK(Near(FactionWanted_Points(&t, 0), 0.0f) && Near(FactionWanted_Points(&t, 3), 14.0f));
	CHECK(Near(FactionWanted_Points(&t, 9), 45.0f));
	/* exactly at a level's start */
	w.points = 6.0f;
	CHECK(FactionWanted_Level(&t, &w) == 2);
	w.points = 5.999f;
	CHECK(FactionWanted_Level(&t, &w) == 1);
}

int main(void) {
	Test_BuiltIn();
	Test_Ranks();
	Test_Errors();
	Test_Wanted();
	printf("faction_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
