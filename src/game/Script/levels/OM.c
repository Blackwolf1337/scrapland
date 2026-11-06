/*
 * The scripts of the level OM in C (levels/om/scripts of data01.packed, issue #2): SetVars of the two domination super
 * deals (SuperDeal_Sixth, SuperDeal_Tenth). Their other functions, Map, MapSnd and Mission_BankersCTF are data
 * (the files in data/levels/om).
 */
#include "scrap/script.h"

typedef struct {
	const char *deal;	/* Sixth, Tenth: the names of the texts and profiles */
	const char *rounds;
	const char *enemy;
	byte textsFirst;	/* SuperDeal_Tenth sets the texts before the profiles */
} omDomination_t;

static const omDomination_t om_sixth = { "Sixth", "2", "Police", 0 };
static const omDomination_t om_tenth = { "Tenth", "4", "Bishop", 1 };

static void Om_Texts(scriptFrame_t *f, const omDomination_t *d) {
	static const char *const vars[] = { "DominationStartTextCad", "DominationFinishPlayerWinsTextCad",
		"DominationFinishPlayerLoosesTextCad" };
	char text[0x40];
	Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", "DominationOnDeathSpawnTime", "5.0");
	for (int i = 0; i < 3; i++) {
		Str_Printf(text, sizeof(text), "SuperDeal_%s_RemoteMessage%d", d->deal, i + 1);
		Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", vars[i], text);
	}
}

/*
 * SetVars(): the zones of the level into Domination.zones (1 to 4 of them), the teams, the profiles by the bad
 * reputation, the texts and the calls on abort and on victory
 */
static PyObject *Om_DominationSetVars(PyObject *args, const omDomination_t *d) {
	static const char *const levels[] = { "Pilot", "Elite", "MegaAce" };
	scriptFrame_t f = { 0 };
	char text[0x80];
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Domination");
	Sf_Run(&f, "Scrap", "StartDummySearch", "(si)", "DM_Domination_Zone_*", 1);
	int mark = f.n;
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	while (Sf_True(&f, dummy) > 0) {
		Sf_Method(&f, Sf_Mod(&f, "Domination", "zones"), "append", "(O)", Sf_Item(&f, dummy, 1));
		dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		Sf_ReleaseKeep(&f, mark, &dummy, 1);
	}
	int l = Sf_Len(&f, Sf_Mod(&f, "Domination", "zones"));
	if (!f.failed && (l == 0 || l > 4)) {
		Sf_RaiseText(&f, "N\xfamero inv\xe1lido de zonas de dominaci\xf3n !!!");
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "DominationZones", Sf_Repr(&f, Sf_Mod(&f, "Domination", "zones")));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "DominationRounds", d->rounds);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "DominationNumEnemies", Sf_ReprI(&f, l));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "DominationEnemyType", d->enemy);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "DominationNumFriends", Sf_ReprI(&f, l - 1));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "DominationFriendType", "BankDirector");
	if (d->textsFirst) {
		Om_Texts(&f, d);
	}
	PyObject *rep = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "BadReputation"));
	if (!f.failed) {
		long r = PyInt_AsLong(rep);
		const char *level = levels[r < 2 ? 0 : r < 4 ? 1 : 2];
		Str_Printf(text, sizeof(text), "SuperDeal%sFriends%s", d->deal, level);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "DominationFriendProfile", text);
		Str_Printf(text, sizeof(text), "SuperDeal%sEnemies%s", d->deal, level);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "DominationEnemyProfile", text);
		Sf_Run(&f, "Scrap", "Set", "(si)", "DominFightingFreezeCadenceShoot", 2);
		Sf_Run(&f, "Scrap", "Set", "(si)", "DominFightingFreezeControlAim", 6);
	}
	if (!d->textsFirst) {
		Om_Texts(&f, d);
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "OnAbort",
		"import MissionsFuncs; MissionsFuncs.EndOfSuperDeal('SuperDeal_Faliure_SystemMessage1')");
	Str_Printf(text, sizeof(text), "import SuperDeal_%s;SuperDeal_%s.Victory()", d->deal, d->deal);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "OnVictory", text);
	return Sf_None(&f);
}

static PyObject *Om_SixthSetVars(PyObject *self, PyObject *args) {
	(void)self;
	return Om_DominationSetVars(args, &om_sixth);
}

static PyObject *Om_TenthSetVars(PyObject *self, PyObject *args) {
	(void)self;
	return Om_DominationSetVars(args, &om_tenth);
}

PyMethodDef g_OmSixthMethods[] = {
	{ "SetVars", Om_SixthSetVars, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_OmTenthMethods[] = {
	{ "SetVars", Om_TenthSetVars, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
