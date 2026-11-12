/*
 * The scripts of the level OM in C (levels/om/scripts of data01.packed, issue #2): SetVars of the two domination super
 * deals (SuperDeal_Sixth, SuperDeal_Tenth). Their other functions, Map, MapSnd and Mission_BankersCTF are data
 * (the files in data/levels/om).
 */
#include "scrap/scrap.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL om

typedef struct {
	const char *deal;	/* Sixth, Tenth: the names of the texts and profiles */
	const char *rounds;
	const char *enemy;
	byte textsFirst;	/* SuperDeal_Tenth sets the texts before the profiles */
} omDomination_t;

static const omDomination_t om_sixth = { "Sixth", "2", "Police", 0 };
static const omDomination_t om_tenth = { "Tenth", "4", "Bishop", 1 };

static void Om_Texts(const omDomination_t *d) {
	static const char *const vars[] = { "DominationStartTextCad", "DominationFinishPlayerWinsTextCad",
		"DominationFinishPlayerLoosesTextCad" };
	char text[0x40];
	SaveVar_Set("DominationOnDeathSpawnTime", "5.0");
	for (int i = 0; i < 3; i++) {
		Str_Printf(text, sizeof(text), "SuperDeal_%s_RemoteMessage%d", d->deal, i + 1);
		SaveVar_Set(vars[i], text);
	}
}

/* Scrap.SetSaveVar(name, `v`) of an int */
static void Om_SaveInt(const char *name, long v) {
	char text[0x20];
	Str_Printf(text, sizeof(text), "%ld", v);
	SaveVar_Set(name, text);
}

/*
 * SetVars(): the zones of the level into Domination.zones (1 to 4 of them), the teams, the profiles by the bad
 * reputation, the texts and the calls on abort and on victory
 */
static byte Om_DominationSetVars(const omDomination_t *d) {
	static const char *const levels[] = { "Pilot", "Elite", "MegaAce" };
	char text[0x80];
	nativeVal_t dummy, pos, zones, rep, r;
	if (!Mode_Import("Domination")) {
		return 0;
	}
	Mode_DummyStart("DM_Domination_Zone_*", 1);
	Mode_DummyNext(&dummy);
	byte ok = 1;
	while (ok && Native_True(&dummy)) {
		/* Domination.zones.append(dummy[1]) (the list of Domination, changed in place) */
		ok = LvN_Item(&dummy, 1, &pos) && LvN_ModAppend("Domination", "zones", &pos);
		Native_Free(&pos);
		Native_Free(&dummy);
		if (ok) {
			Mode_DummyNext(&dummy);
		}
	}
	Native_Free(&dummy);
	if (!ok || !LvN_ModGet("Domination", "zones", &zones)) {
		return 0;
	}
	int l = Native_Size(&zones);
	if (l < 0 || l == 0 || l > 4) {
		Native_Free(&zones);
		return l >= 0 && LvN_RaiseText("N\xfamero inv\xe1lido de zonas de dominaci\xf3n !!!");
	}
	LvN_SaveRepr("DominationZones", &zones);
	Native_Free(&zones);
	SaveVar_Set("DominationRounds", d->rounds);
	Om_SaveInt("DominationNumEnemies", l);
	SaveVar_Set("DominationEnemyType", d->enemy);
	Om_SaveInt("DominationNumFriends", l - 1);
	SaveVar_Set("DominationFriendType", "BankDirector");
	if (d->textsFirst) {
		Om_Texts(d);
	}
	Mode_SaveVar("BadReputation", &rep);
	Native_SetNone(&r);
	ok = Native_ToInt(&rep, &r);
	Native_Free(&rep);
	if (!ok) {
		return 0;
	}
	/* int() of a text is an int (a larger one is its ValueError) */
	long n = r.i;
	const char *level = levels[n < 2 ? 0 : n < 4 ? 1 : 2];
	Str_Printf(text, sizeof(text), "SuperDeal%sFriends%s", d->deal, level);
	SaveVar_Set("DominationFriendProfile", text);
	Str_Printf(text, sizeof(text), "SuperDeal%sEnemies%s", d->deal, level);
	SaveVar_Set("DominationEnemyProfile", text);
	nativeVal_t two, six;
	Native_SetInt(&two, 2);
	Native_SetInt(&six, 6);
	if (!Mode_GVarSet("DominFightingFreezeCadenceShoot", &two) || !Mode_GVarSet("DominFightingFreezeControlAim", &six)) {
		return 0;
	}
	if (!d->textsFirst) {
		Om_Texts(d);
	}
	SaveVar_Set("OnAbort", "import MissionsFuncs; MissionsFuncs.EndOfSuperDeal('SuperDeal_Faliure_SystemMessage1')");
	Str_Printf(text, sizeof(text), "import SuperDeal_%s;SuperDeal_%s.Victory()", d->deal, d->deal);
	SaveVar_Set("OnVictory", text);
	return 1;
}

static byte Om_SixthSetVars(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Om_DominationSetVars(&om_sixth);
}

static byte Om_TenthSetVars(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Om_DominationSetVars(&om_tenth);
}

LV_FN(Om, SixthSetVarsPy, 0, Om_SixthSetVars)
LV_FN(Om, TenthSetVarsPy, 0, Om_TenthSetVars)

PyMethodDef g_OmSixthMethods[] = {
	{ "SetVars", Om_SixthSetVarsPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_OmTenthMethods[] = {
	{ "SetVars", Om_TenthSetVarsPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_OM)
