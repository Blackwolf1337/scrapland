/*
 * Unit test of the network classification lookup (src/game/Script/NetClass.c and the generated NetClassTable.c,
 * include/scrap/netclass.h): every table sorted, every row found by its own name, no engine API function without a
 * tag, and the families of the entity variables. ctest netclass_test.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/netclass.h"

static int fails;

#define EXPECT(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
	for (int t = 0; t < NCT_TABLES; t++) {
		const netClassTable_t *tab = &g_NetClassTables[t];
		EXPECT(tab->count > 0);
		for (int i = 0; i < tab->count; i++) {
			if (i) {
				EXPECT(strcmp(tab->rows[i - 1].name, tab->rows[i].name) < 0);
			}
			EXPECT(NetClass_Row((netTable_t)t, tab->rows[i].name) == &tab->rows[i]);
			EXPECT(tab->rows[i].tag < NC_TAGS);
		}
		printf("%-8s %5d rows, %4d untagged\n", NetClass_TableName((netTable_t)t), tab->count,
			NetClass_Untagged((netTable_t)t));
	}
	EXPECT(NetClass_Untagged(NCT_API) == 0);
	EXPECT(NetClass_Find(NCT_API, "Scrap.CreateEntity") == NC_SIM);
	EXPECT(NetClass_Find(NCT_API, "SVec.Add") == NC_QUERY);
	EXPECT(NetClass_Find(NCT_API, "SNet.InitServer") == NC_NET_OLD);
	EXPECT(NetClass_Find(NCT_API, "SFX.ShipExplosion") == NC_PRES_EVENT);
	EXPECT(NetClass_Find(NCT_API, "Scrap.NoSuchFunction") == NC_ABSENT);
	EXPECT(NetClass_Find(NCT_API, "") == NC_ABSENT);
	EXPECT(NetClass_Find(NCT_TABLES, "Scrap.CreateEntity") == NC_ABSENT);
	/* the families: digit runs, starts with '*' */
	EXPECT(NetClass_Find(NCT_ENTVAR, "snd03_vol1") == NetClass_Find(NCT_ENTVAR, "snd#_vol#"));
	EXPECT(NetClass_Find(NCT_ENTVAR, "snd03_vol1") != NC_ABSENT);
	EXPECT(NetClass_Find(NCT_ENTVAR, "NodePos_DC_Platform") == NetClass_Find(NCT_ENTVAR, "NodePos_*"));
	EXPECT(NetClass_Find(NCT_ENTVAR, "AI_SomethingNew") == NC_SERVER_ONLY);
	EXPECT(NetClass_Find(NCT_ENTVAR, "LLogicWhatever") == NC_SERVER_ONLY);
	EXPECT(NetClass_Find(NCT_ENTVAR, "Life") == NC_STATE);
	EXPECT(NetClass_Find(NCT_ENTVAR, "ForceAction") == NC_EVENT);
	EXPECT(NetClass_Find(NCT_MODVAR, "Parking.PlayerShip") == NC_PLAYER);
	EXPECT(NetClass_Find(NCT_SAVEVAR, "Mission") == NC_STORY);
	EXPECT(NetClass_Find(NCT_SAVEVAR, "Hangar") == NC_PLAYER);
	EXPECT(NetClass_Find(NCT_MODULE, "Scene") == NC_SCENE);
	EXPECT(!strcmp(NetClass_TagName(NC_ABSENT), "absent"));
	EXPECT(!strcmp(NetClass_TagName(NC_UNTAGGED), "untagged"));
	EXPECT(!strcmp(NetClass_TagName(NC_PRES_STATE), "PRES_STATE"));
	/* a long name is looked up as it is */
	char longName[300];
	memset(longName, 'A', sizeof(longName) - 1);
	longName[sizeof(longName) - 1] = 0;
	EXPECT(NetClass_Find(NCT_ENTVAR, longName) == NC_ABSENT);
	printf(fails ? "netclass_test: %d failures\n" : "netclass_test: ok\n", fails);
	return fails != 0;
}
