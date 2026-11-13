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
	CHECK(!strcmp(Faction_PlayClass(p, 1), "Police") && !strcmp(Faction_PlayClass(p, 5), "PoliceBoss"));
	CHECK(!strcmp(Faction_PlayClass(p, 9), "PoliceBoss") && !strcmp(Faction_PlayClass(p, 0), "Police"));
	CHECK(!strcmp(p->pilot, "P_Police") && !strcmp(t.f[1].pilot, "P_Bishop"));
	CHECK(!strcmp(p->armour, "Gear") && p->armourLife == 300);
	CHECK(!strcmp(p->office, "Police"));
	CHECK(p->nships == 6 && !strcmp(p->ships[4], "SPoli5"));
	CHECK(p->nranks == 5 && p->ranks[4] == 20);
	for (int a = 0; a <= FA_SHIP; a++) {
		CHECK(p->cooldown[a] >= 0.0f && p->keys[a] == a);
	}
	CHECK(p->nkeys == 6 && p->cooldown[FA_HEAL] < 0.0f);
	/* every group can be joined at a desk of its office and has a jail and ships */
	for (int i = 0; i < 6; i++) {
		CHECK(t.f[i].keys[0] == FA_JOIN && t.f[i].cooldown[FA_SHIP] >= 0.0f);
		CHECK(t.f[i].desk[0] && t.f[i].jail[0] && t.f[i].nships > 0 && t.f[i].nplay > 0);
	}
	CHECK(!strcmp(Faction_RankShip(p, 1), "SPoli1") && !strcmp(Faction_RankShip(p, 5), "SPoli5"));
	CHECK(!strcmp(Faction_RankShip(&t.f[1], 5), "SArchbishop1") && !strcmp(Faction_RankShip(&t.f[5], 3), "SMerc2"));
	CHECK(!strcmp(Faction_PlayClass(&t.f[5], 1), "MercenaryC") && !strcmp(Faction_PlayClass(&t.f[5], 5), "MercenaryA"));
	CHECK(!strcmp(t.f[3].office, "TownHall") && !strcmp(t.f[5].office, "GamblinDen"));
	CHECK(!strcmp(t.f[5].desk, "DM_NPC_Spawn_MercenaryA_Smartie") && !strcmp(t.f[1].jail, "DM_Mission_Bishop_Prayer00"));
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
	CHECK(Near(t.crime[FCRIME_KILL_PLAYER], 6.0f) && Near(t.crime[FCRIME_THEFT], 1.5f));
	CHECK(t.healLife == 60 && t.healPrice == 120 && t.rayDamage == 30 && Near(t.downed, 15.0f));
	CHECK(t.steal == 150 && t.loan == 1000 && Near(t.loanOwed, 1.25f) && t.bribe == 400);
	CHECK(!strcmp(t.vaultLevel, "Bank") && Near(t.vaultEvery, 600.0f) && Near(t.vaultOpen, 60.0f) && t.vaultMoney == 1500);
	CHECK(t.bounty == 500 && t.bountyMax == 5000 && !strcmp(t.hireShip, "SMerc1") && t.hireCount == 2);
	CHECK(Near(t.slowSpeed, 0.3f) && Near(t.blablaTime, 4.0f) && t.permitRank == 3);
	CHECK(t.nroutes == 6 && t.nzones == 2 && !strcmp(t.zones[0].dummy, "*") && Near(t.zones[1].radius, 3000.0f));
	CHECK(t.ncommon == 1 && t.common[0] == FA_BOUNTY);
}

static void Test_Keys(void) {
	int bishops = Faction_Find(&t, "Bishops"), mercs = Faction_Find(&t, "Mercs"), press = Faction_Find(&t, "Press");
	/* no member: join, then the common bounty */
	CHECK(Faction_KeyAbility(&t, FACTION_NONE, 0) == FA_JOIN);
	CHECK(Faction_KeyAbility(&t, FACTION_NONE, 1) == FA_BOUNTY);
	CHECK(Faction_KeyAbility(&t, FACTION_NONE, 2) == -1);
	CHECK(Faction_KeyAbility(&t, FACTION_NONE, 6) == -1 && Faction_KeyAbility(&t, 0, -1) == -1);
	/* the police has six of its own: no room for the common one */
	CHECK(Faction_KeyAbility(&t, 0, 5) == FA_SHIP && Faction_Cooldown(&t, 0, FA_BOUNTY) < 0.0f);
	CHECK(Faction_KeyAbility(&t, bishops, 1) == FA_HEAL && Faction_KeyAbility(&t, bishops, 3) == FA_REVIVE);
	CHECK(Faction_KeyAbility(&t, bishops, 5) == FA_BOUNTY && Near(Faction_Cooldown(&t, bishops, FA_BOUNTY), 30.0f));
	CHECK(Faction_KeyAbility(&t, press, 4) == FA_BOUNTY && Faction_KeyAbility(&t, press, 5) == -1);
	/* the mercs have the bounty of their own (not twice) */
	CHECK(Faction_KeyAbility(&t, mercs, 5) == FA_BOUNTY && Faction_KeyAbility(&t, mercs, 1) == FA_CONTRACTS);
	CHECK(Near(Faction_Cooldown(&t, mercs, FA_BOUNTY), 30.0f) && Near(Faction_Cooldown(&t, FACTION_NONE, FA_BOUNTY), 30.0f));
	CHECK(Faction_Cooldown(&t, FACTION_NONE, FA_HEAL) < 0.0f && Faction_Cooldown(&t, bishops, FA_STEAL) < 0.0f);
	CHECK(Near(Faction_Cooldown(&t, FACTION_NONE, FA_JOIN), 0.0f));
	CHECK(Faction_AbilityOf("courier") == FA_COURIER && Faction_AbilityOf("fly") == -1 && Faction_AbilityOf(NULL) == -1);
	CHECK(!strcmp(Faction_AbilityName(FA_BLABLA), "blabla"));
}

static void Test_PvP(void) {
	int police = 0, bishops = 1, bankers = 2, bur = 3, mercs = 5;
	FactionSide a = { FACTION_NONE, 0, 0, 0 }, b = { FACTION_NONE, 0, 0, 0 };
	CHECK(Faction_PvPRule(&t, &a, &b, 1, 1) == -1);
	/* police and a wanted player, both ways, only with policePvp */
	a.faction = police;
	b.wanted = 2;
	CHECK(Faction_PvPRule(&t, &a, &b, 1, 1) == 1 && Faction_PvPRule(&t, &b, &a, 1, 1) == 1);
	CHECK(Faction_PvPRule(&t, &a, &b, 0, 0) == -1);
	/* a wanted officer is no officer for this */
	a.wanted = 1;
	CHECK(Faction_PvPRule(&t, &a, &b, 1, 0) == -1);
	/* the sanctuary wins over all */
	a.wanted = 0;
	b.sanctuary = 1;
	CHECK(Faction_PvPRule(&t, &a, &b, 1, 1) == 0 && Faction_PvPRule(&t, &b, &a, 1, 1) == 0);
	/* hostile factions with factionPvp, friends never, the same faction never */
	FactionSide c = { bishops, 0, 0, 0 }, d = { bankers, 0, 0, 0 };
	CHECK(Faction_PvPRule(&t, &c, &d, 1, 1) == 1 && Faction_PvPRule(&t, &d, &c, 1, 1) == 1);
	CHECK(Faction_PvPRule(&t, &c, &d, 1, 0) == -1);
	d.faction = bishops;
	CHECK(Faction_PvPRule(&t, &c, &d, 1, 1) == 0);
	FactionSide e = { police, 0, 0, 0 }, f = { bur, 0, 0, 0 };
	CHECK(Faction_PvPRule(&t, &e, &f, 1, 1) == 0 && Faction_PvPRule(&t, &f, &e, 1, 1) == 0);
	/* police against a wanted friend: the law first */
	f.wanted = 3;
	CHECK(Faction_PvPRule(&t, &e, &f, 1, 1) == 1);
	/* a merc and a player with a bounty, both ways; mercs and police are hostile */
	FactionSide m = { mercs, 0, 0, 0 }, v = { FACTION_NONE, 0, 700, 0 };
	CHECK(Faction_PvPRule(&t, &m, &v, 0, 0) == 1 && Faction_PvPRule(&t, &v, &m, 0, 0) == 1);
	v.bounty = 0;
	CHECK(Faction_PvPRule(&t, &m, &v, 1, 1) == -1);
	CHECK(Faction_PvPRule(&t, &m, &e, 1, 1) == 1 && Faction_PvPRule(&t, &m, &e, 1, 0) == -1);
}

static void Test_Economy(void) {
	/* a loan of 1000 owes 1250, half of each income pays it */
	int debt = (int)((float)t.loan * t.loanOwed);
	CHECK(debt == 1250);
	CHECK(FactionLoan_Repay(&t, &debt, 0) == 0 && FactionLoan_Repay(&t, &debt, -50) == 0);
	CHECK(FactionLoan_Repay(&t, &debt, 300) == 150 && debt == 1100);
	CHECK(FactionLoan_Repay(&t, &debt, 1) == 1 && debt == 1099);
	CHECK(FactionLoan_Repay(&t, &debt, 5000) == 1099 && debt == 0);
	CHECK(FactionLoan_Repay(&t, &debt, 100) == 0);
	/* bounties add up to the most, end after their time and start again */
	int b = 0;
	int64_t ends = 0;
	CHECK(FactionBounty_Add(&t, &b, &ends, 500, 1000) == 500 && b == 500 && ends == 1000 + 3600);
	CHECK(FactionBounty_Add(&t, &b, &ends, 4800, 2000) == 4500 && b == 5000 && ends == 2000 + 3600);
	CHECK(FactionBounty_Add(&t, &b, &ends, 500, 2001) == 0 && b == 5000);
	CHECK(FactionBounty_Live(&b, &ends, 5599) == 5000);
	CHECK(FactionBounty_Live(&b, &ends, 5600) == 0 && ends == 0);
	CHECK(FactionBounty_Add(&t, &b, &ends, 500, 9000) == 500);
	/* courier routes: never the level of the job's start; the reward with the bonus within half the time */
	for (uint32_t s = 0; s < 20; s++) {
		int r = Faction_Route(&t, "Press", s);
		CHECK(r >= 0 && strcmp(t.routes[r].level, "Press"));
	}
	CHECK(Faction_Route(&t, "Press", 0) != Faction_Route(&t, "Press", 1));
	CHECK(Faction_CourierReward(&t, 100) == 600 && Faction_CourierReward(&t, 150) == 600);
	CHECK(Faction_CourierReward(&t, 151) == 400 && Faction_CourierReward(&t, 301) == 0);
	CHECK(!Faction_NeedsPermit(&t, 2) && Faction_NeedsPermit(&t, 3) && Faction_NeedsPermit(&t, 5));
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
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\tability ship 2\n\tability join 2\n", err,
		sizeof(err)) < 0 && strstr(err, "first"));
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\tability ship 2\n\tability ship 3\n", err,
		sizeof(err)) < 0 && strstr(err, "twice"));
	CHECK(FactionTable_Parse(&x, "faction A\n\tplay X\n\toffice L\n\tability join 1\n\tability heal 1\n\tability ray 1\n"
		"\tability steal 1\n\tability suck 1\n\tability loan 1\n\tability bribe 1\n", err, sizeof(err)) < 0 &&
		strstr(err, "line 10") && strstr(err, "keys"));
	CHECK(FactionTable_Parse(&x, "common join 3\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "loan 100 0.5 0.5 0\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "slow 100 3 0\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "vault Bank D 60 60 100\n", err, sizeof(err)) < 0 && strstr(err, "vault"));
	CHECK(FactionTable_Parse(&x, "route A\n", err, sizeof(err)) < 0);
	CHECK(FactionTable_Parse(&x, "route A DM_A_Name_Much_Longer_Than_The_Room_Of_The_Field\n", err, sizeof(err)) < 0 &&
		strstr(err, "too long"));
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
	Test_Keys();
	Test_PvP();
	Test_Economy();
	printf("faction_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
