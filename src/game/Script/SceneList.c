/*
 * The cutscenes of the game, see include/scrap/scenelist.h: the table (the scenes of the packs'
 * levels/<level>/map/<scene>/<scene>anm with the function of the level's scripts that loads each), the import of a
 * mission's module and the load and play of a scene by its name. Not in the original.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/scenelist.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptports.h"

static const sceneInfo_t sl_scenes[] = {
	{ "Prolog0", "Press", "Mission_MeetBoss", "Mission_MeetBoss", "LoadProlog0Scene", 0, "Dtritus, Berto" },
	{ "Prolog1", "Press", "Mission_MeetBoss", "Mission_MeetBoss", "LoadProlog1Scene", 0, "Dtritus, Berto, Betty, Boss" },
	{ "Prolog2", "Press", "Mission_MeetBoss", "Mission_MeetBoss", "LoadProlog2Scene", 0, "Dtritus, Betty, Gatekeeper" },
	{ "Mecanic", "Junkyard", "Mission_RustyFirstContact", "Mission_RustyFirstContact", "LoadMecanicScene", 0, "Dtritus, Berto, Rusty, Sputnik" },
	{ "MecanicFinal", "Junkyard", "Mission_RustyFirstContact", "Mission_RustyFirstContact", "LoadMecanicFinalScene", 0, "Dtritus, Rusty, Sputnik" },
	{ "News1", "Press", "Mission_HumphreyArchbishopKilled", "Mission_HumphreyArchbishopKilled", "LoadScene", 1, "Betty, Dtritus, Humphrey" },
	{ "Confesion", "Press", "Mission_GoToPress", "Mission_GoToPress", "LoadScene", 0, "Dtritus, Humphrey, Betty, Berto, Boss" },
	{ "News2", "Press", "Mission_PressMeeting", "Mission_PressMeeting", "LoadScene", 1, "Betty, Dtritus, Berto, Boss" },
	{ "News3", "Press", "Mission_IntroHuman2", "Mission_IntroHuman2", "LoadScene", 1, "Betty" },
	{ "HideHuman", "Press", "Mission_TalkToBoss", "Mission_TalkToBoss", "LoadScene", 0, "Dtritus, Human" },
	{ "HumanCD", "Press", "Mission_GoWithHuman", "Mission_GoWithHuman", "LoadScene", 0, "Human" },
	{ "HumanCD2", "Press", "Mission_GoWithHuman", "Mission_GoWithHuman", "LoadScene2", 0, "Human, Dtritus" },
	{ "DiscoverDeep", "Press", "Mission_WhoIsDeepThroat", "Mission_WhoIsDeepThroat", "LoadScene", 0, "Dtritus, Betty, Humphrey, Boss" },
	{ "BossDiscovered", "Press", "Mission_WhoIsTheBaddie", "Mission_WhoIsTheBaddie", "LoadScene", 0, "Dtritus" },
	{ "MeetHuman", "Police", "Mission_GetAuthorization", "Mission_GetAuthorization", "LoadScene", 0, "Dtritus, Human" },
	{ "Beating", "Police", "Mission_BeatPickus", "Mission_BeatPickus", "LoadScene", 0, "Dtritus, PoliceJefe, Functionary, 5 thugs" },
	{ "PoliFight", "Police", "Mission_PoliceBossCheats", "Mission_PoliceBossCheats", "LoadPoliFightScene", 0, "Police, PoliceBoss" },
	{ "BillArrives_A", "Orbit", "Mission_IntroHuman", "Mission_IntroHuman", "LoadSceneA", 1, "Human" },
	{ "BillArrives_B", "Orbit", "Mission_IntroHuman", "Mission_IntroHuman", "LoadSceneB", 0, "Human" },
	{ "BillArrives_C", "Orbit", "Mission_IntroHuman", "Mission_IntroHuman", "LoadSceneC", 0, "Human, 2 gears" },
	{ "Decontamination", "Orbit", "MapDecont", "MapDecont", "LoadDecontamination", 0, "Dtritus" },
	{ "Bishops", "Orbit", "MapDecont", "MapDecont", "LoadBishopsScene", 0, "Dtritus, 2 bishops" },
	{ "Functionary", "Orbit", "MapDecont", "MapDecont", "LoadFunctionaryScene", 0, "Dtritus, Functionary" },
	{ "Falling", "Orbit", "MapDecont", "MapDecont", "LoadFallingScene", 0, "Dtritus" },
	{ "DirectorDiscovered", "Bank", "Mission_InfiltrateBank", "Mission_InfiltrateBank", "LoadScene", 0, "Dtritus, BankMaster" },
	{ "MeetGambler", "GamblinDen", "Mission_CrazyGamblerFirstContact", "Mission_CrazyGamblerFirstContact", "LoadScene", 1, "CrazyGambler" },
	{ "Plan", "GamblinDen", "Mission_GoToGamblinDen", "Mission_GoToGamblinDen", "LoadScene", 1, "Dtritus, Human, CrazyGambler, 3 mercenaries" },
	{ "Plan2", "GamblinDen", "Mission_BeforeFinalFight", "Mission_BeforeFinalFight", "LoadScene", 1, "Dtritus, CrazyGambler" },
	{ "Yakuzzi", "Temple", "Mission_MeetArchbishop", "Mission_MeetArchbishop", "LoadYakuzziScene", 0, "Dtritus" },
	{ "EnterMatrix", "GDB", "Mission_IntoTheGDB", "MapMatrix", "Load", 0, "Bishop" },
	{ "Fake0", "Fake", "", "Map", "LoadFake0Scene", 0, "Dtritus, Berto, Functionary" },
	{ "Fake1", "Fake", "", "Map", "LoadFake1Scene", 0, "Dtritus, Berto, a crowd" },
	{ "Fake2", "Fake", "", "Map", "LoadFake2Scene", 0, "Dtritus, Berto" },
	{ "TheEndA", "ABIndoor", "", "Map", "LoadSceneA", 0, "Dtritus, CrazyGambler, Human, Betty, Berto" },
	{ "TheEndB", "ABIndoor", "", "Map", "LoadSceneB", 0, "Dtritus, CrazyGambler, Human, Betty, Berto" },
};

#define SL_COUNT ((int)(sizeof(sl_scenes) / sizeof(sl_scenes[0])))

int SceneList_Count(void) {
	return SL_COUNT;
}

const sceneInfo_t *SceneList_At(int i) {
	return i >= 0 && i < SL_COUNT ? &sl_scenes[i] : NULL;
}

const sceneInfo_t *SceneList_Find(const char *scene) {
	for (int i = 0; scene && i < SL_COUNT; i++) {
		if (!crt_stricmp(sl_scenes[i].scene, scene)) {
			return &sl_scenes[i];
		}
	}
	return NULL;
}

byte SceneList_Loaded(const char *scene) {
	char name[0x60];
	Str_PrintfQuiet(name, sizeof(name), "Scene%s", scene);
	return g_pActClassList && cActClassList_Get(g_pActClassList, name) != NULL;
}

byte SceneList_Import(const char *module) {
	char dir[0x200], pyc[0x60];
	nativeVal_t path;
	if (Mode_Import(module)) {
		return 1;
	}
	Native_ErrClear();
	if (!g_pApp->levelPath) {
		return 0;
	}
	Str_PrintfQuiet(dir, sizeof(dir), "%s/Scripts", g_pApp->levelPath);
	Str_PrintfQuiet(pyc, sizeof(pyc), "%s.pyc", module);
	Native_SetStr(&path, dir);
	byte ok = Mode_Pack(&path) && PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", module, pyc);
	Native_Free(&path);
	Mode_Pack(NULL);
	return ok && Mode_Import(module);
}

byte SceneList_Load(const sceneInfo_t *s) {
	char code[0x100];
	nativeVal_t text = { 0 };
	if (!s) {
		return 0;
	}
	if (SceneList_Loaded(s->scene)) {
		return 1;
	}
	if (!SceneList_Import(s->module)) {
		Native_ErrClear();
		return 0;
	}
	/* the line the debug menu ran: the function as the module holds it (a data section's replay or a port) */
	execScope_t x = { .module = "Scene" };
	Str_PrintfQuiet(code, sizeof(code), "import %s;%s.%s()", s->module, s->module, s->load);
	Native_SetText(&text, code, (int)strlen(code));
	byte ok = Exec_Text(&x, &text);
	Native_Free(&text);
	if (!ok) {
		Native_ErrClear();
	}
	return ok && SceneList_Loaded(s->scene);
}

byte SceneList_Play(const char *scene) {
	const sceneInfo_t *s = SceneList_Find(scene);
	if (!s || !SceneList_Load(s)) {
		return 0;
	}
	if (!Mode_Import("Scene") || !PORT_CALL(Scene, Play, NULL, "s", s->scene)) {
		Native_ErrClear();
		return 0;
	}
	return 1;
}
