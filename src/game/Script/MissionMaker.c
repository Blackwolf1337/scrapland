/*
 * The mission maker of the developers (scripts/missionmaker.py of Data.packed): the game never ran it, it loaded
 * every level and mission in turn to build the packs. Here its list of missions and its two loaders, mss() and
 * split(), for the console (map, mission, missions) and the debug menu.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/missionmaker.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptports.h"
#include "scrap/sound.h"

/* FileList of missionmaker.py; Mission_GetDataPart3 was in it twice */
const mmEntry g_MissionMakerList[] = {
	{ "ShipEdit", "", "" },
	{ "Orbit", "MapDecont", "" },
	{ "Orbit", "Mission_IntroHuman", "" },
	{ "Bank", "Mission_CatchRudolph", "" },
	{ "Bank", "Mission_GetDataPart2", "" },
	{ "Bank", "Mission_InfiltrateBank", "" },
	{ "Commercial", "Mission_DefeatCrazyGambler3", "" },
	{ "Commercial", "Mission_DestroyCommSats2", "" },
	{ "Commercial", "Mission_DestroyMickus", "" },
	{ "Commercial", "Mission_DestroyPorkinsTransports", "" },
	{ "Commercial", "Mission_DestroyStupidus", "" },
	{ "Commercial", "Mission_FirstMeetDeepThroat", "" },
	{ "Commercial", "Mission_GetDataPart3", "" },
	{ "Commercial", "Mission_ProtectGus", "" },
	{ "Commercial", "Mission_Win1stRace", "" },
	{ "DownTown", "Mission_BertoMassacre", "" },
	{ "DownTown", "Mission_DefeatCrazyGambler1", "" },
	{ "DownTown", "Mission_DestroyCommSats3", "" },
	{ "DownTown", "Mission_DistractHumphrey", "" },
	{ "DownTown", "Mission_FaceTheMayor", "" },
	{ "DownTown", "Mission_FirstContactPress", "" },
	{ "DownTown", "Mission_GetDataPart1", "" },
	{ "DownTown", "Mission_HumphreyRace", "" },
	{ "DownTown", "Mission_Massacre", "" },
	{ "DownTown", "Mission_MultiplusRace", "" },
	{ "DownTown", "Mission_ProtectMayorTransport", "" },
	{ "DownTown", "Mission_StealMayorShip", "" },
	{ "GamblinDen", "Mission_AfterFinalFight", "" },
	{ "GamblinDen", "Mission_ArrangeBankersCombat", "" },
	{ "GamblinDen", "Mission_ArrangeBankersCTF", "" },
	{ "GamblinDen", "Mission_ArrangeIgorCombat", "" },
	{ "GamblinDen", "Mission_ArrangeMortalRace", "" },
	{ "GamblinDen", "Mission_BackFromMortalRace", "" },
	{ "GamblinDen", "Mission_BeforeFinalFight", "" },
	{ "GamblinDen", "Mission_CrazyGamblerFirstContact", "" },
	{ "GamblinDen", "Mission_GoToGamblinDen", "" },
	{ "GamblinDen", "Mission_IgorFirstContact", "" },
	{ "GamblinDen", "Mission_TalkToTraitoriusAfterCTF", "" },
	{ "GamblinDen", "Mission_ThreeStooges", "" },
	{ "GDB", "Mission_IntoTheGDB", "" },
	{ "GDB", "Mission_ReinsertOwnMatrix", "" },
	{ "GDB", "Mission_ReinsertOwnMatrix2", "" },
	{ "Junkyard", "Mission_RustyFirstContact", "" },
	{ "Outskirts", "Mission_AttackTraitorius", "" },
	{ "Outskirts", "Mission_DestroyBadDebtors", "" },
	{ "Outskirts", "Mission_DestroyHumphrey", "" },
	{ "Outskirts", "Mission_FunctionaryHunting", "" },
	{ "Outskirts", "Mission_GoToOutskirts", "" },
	{ "Outskirts", "Mission_IdentifyMerc", "" },
	{ "Outskirts", "Mission_KillPolice", "" },
	{ "Outskirts", "Mission_KillRudolph", "" },
	{ "Outskirts", "Mission_TransportKiller", "" },
	{ "Outskirts", "Mission_Win2ndRace", "" },
	{ "Outskirts", "Mission_DestroyCommSats1", "" },
	{ "Press", "Mission_FicusBanker", "" },
	{ "Press", "Mission_GetCrazyData", "" },
	{ "Press", "Mission_GoToPress", "" },
	{ "Press", "Mission_GoWithHuman", "" },
	{ "Press", "Mission_HumphreyArchbishopKilled", "" },
	{ "Press", "Mission_IntroHuman2", "" },
	{ "Press", "Mission_MeetBoss", "" },
	{ "Press", "Mission_PressMeeting", "" },
	{ "Press", "Mission_TalkToBoss", "" },
	{ "Press", "Mission_WhoIsDeepThroat", "" },
	{ "Press", "Mission_WhoIsTheBaddie", "" },
	{ "Scrapyard", "Mission_Ambush", "" },
	{ "Scrapyard", "Mission_DefeatCrazyGambler2", "" },
	{ "Scrapyard", "Mission_FindDtritusMatrix", "" },
	{ "Scrapyard", "Mission_MeetBerto", "" },
	{ "Scrapyard", "Mission_MosquitoClue", "" },
	{ "Scrapyard", "Mission_StupidMerc", "" },
	{ "Scrapyard", "Mission_StupidMercAgain", "" },
	{ "Scrapyard", "Mission_TalkToDeepThroat", "" },
	{ "Temple", "Mission_ArchbishopDead", "" },
	{ "Temple", "Mission_AskGus", "" },
	{ "Temple", "Mission_MeetArchbishop", "" },
	{ "Temple", "Mission_TalkToDeepThroat", "" },
	{ "TownHall", "Mission_ApologizeMayor", "" },
	{ "TownHall", "Mission_CatchTrurl", "" },
	{ "TownHall", "Mission_SheriffMayor", "" },
	{ "TownHall", "Mission_TalkToMayor", "" },
	{ "QF", "Mission_BankersCombat", "TeamDeathMatch" },
	{ "QF", "SuperDeal_Eleventh", "TeamDeathMatch" },
	{ "QF", "SuperDeal_Fourth", "TeamDeathMatch" },
	{ "FZ", "SuperDeal_Fifth", "CTF" },
	{ "FZ", "SuperDeal_Ninth", "CTF" },
	{ "FZ", "SuperDeal_Second", "MortalRace" },
	{ "FZ", "SuperDeal_Seventh", "MortalRace" },
	{ "FZ", "SuperDeal_Twelfth", "MortalRace" },
	{ "FZ", "Mission_MortalRace", "MortalRace" },
	{ "CA", "Mission_IgorCombat", "Versus" },
	{ "CA", "SuperDeal_First", "Versus" },
	{ "CA", "SuperDeal_Third", "DeathMatch" },
	{ "CA", "SuperDeal_Eighth", "Versus" },
	{ "OM", "Mission_BankersCTF", "CTF" },
	{ "OM", "SuperDeal_Sixth", "Domination" },
	{ "OM", "SuperDeal_Tenth", "Domination" },
	{ "AB", "", "" },
	{ "Fake", "", "" },
	{ "ABIndoor", "", "" },
	{ "Police", "Mission_BeatPickus", "" },
	{ "Police", "Mission_FicusDead", "" },
	{ "Police", "Mission_GetAuthorization", "" },
	{ "Police", "Mission_InitEpilogue", "" },
	{ "Police", "Mission_PoliceBossCheats", "" },
	{ "Police", "Mission_JailBreak", "" },
};
const int g_MissionMakerCount = (int)(sizeof(g_MissionMakerList) / sizeof(g_MissionMakerList[0]));

const char *const g_MissionMakerSplitMaps[] = { "QF", "FZ", "OM", "CA", "GDB", "MCA", "MCB", "MDA", "MDB", "MOA", "MOB",
	"MSA", "MSB" };
const int g_MissionMakerSplitCount = (int)(sizeof(g_MissionMakerSplitMaps) / sizeof(g_MissionMakerSplitMaps[0]));

/* the save variables of a fresh game that mss() creates, in its order (Char and SuperDealType come from the
 * arguments) */
static const char *const mm_defaults[] = {
	"GameplayTime", "0",
	"Spawn", "DM_Player_Spawn_Beginning",
	"Char", NULL,
	"ComeFrom", "",
	"BadReputation", "0",
	"StdShipAIProfile", "Ace",
	"NumCrazyGamblerLifes", "10",
	"MortalRaceFightingFreezeControlAim", "0",
	"Hangar.iHangarShip", "8",
	"Hangar.availableEnginesList", "['MPOLI4', 'MPOLI5', 'MPOLIBOSS1', 'MPOLI2', 'MBERTO1', 'MBETTY1', 'MPOLI1', "
		"'MMERC1','MMERC2', 'MMERC3', 'MPOLI3', 'MMAYOR1', 'MFUNC1', 'MBANKER1','MBANKMASTER1', 'MBISHOP1',"
		"'MARCHBISHOP1', 'MHUMP1', 'MBOSS1']",
	"Hangar.availableUpgradesList", "[\"VulcanUpgrade1\", \"VulcanUpgrade2\", \"DevastatorUpgrade1\", "
		"\"DevastatorUpgrade2\", \"SwarmUpgrade1\", \"SwarmUpgrade2\", \"InfernoUpgrade1\", \"InfernoUpgrade2\", "
		"\"TeslaUpgrade1\", \"TeslaUpgrade2\", \"ATPCUpgrade1\", \"ATPCUpgrade2\"]",
	"Hangar.availableWeaponsList", "[\"Vulcan\", \"Devastator\", \"Swarm\",\"Inferno\", \"Tesla\", \"ATPC\"]",
	"Hangar.shipsToEditList", "[\"SPoli1\", \"SPoli2\", \"SPoli3\", \"SPoli4\", \"SPoli5\", \"SPoliBoss1\", "
		"\"SMerc1\",\"SMerc2\",\"SMerc3\",\"SMayor1\",\"SBanker1\",\"SBankMaster1\",\"SBishop1\",\"SArchbishop1\","
		"\"SFunc1\",\"SBerto1\",\"SBetty1\",\"SHump1\",\"SBoss1\"]",
	"Hangar.DestroyedShips", "[0,0,0,0,0,0,0,0,0,0]",
	"Hangar.HangarShipAux", "SLifeBoat<-<-<-<-<-0,0,0,0,0,0,1<-50<-0,0,0,0,0,0",
	"Hangar.HangarShip9", "SPoli1<-MPoli1<-<-<-<-1,1,1,1,1,0,1<-50<-0,0,0,0,0,0",
	"Hangar.HangarShip8", "",
	"Hangar.HangarShip7", "",
	"Hangar.HangarShip6", "",
	"Hangar.HangarShip5", "",
	"Hangar.HangarShip4", "",
	"Hangar.HangarShip3", "",
	"Hangar.HangarShip2", "",
	"Hangar.HangarShip1", "",
	"Parked.Police", "['SPOLI1', 'SPOLI2', 'SPOLI3', 'SPOLI4', 'SPOLI5', 'SPOLIBOSS1']",
	"Parked.Mercs", "['SMERC1', 'SMERC2', 'SMERC3']",
	"Parked.TownHall", "['SMAYOR1','SFUNC1']",
	"Parked.Bank", "['SBANKER1', 'SBANKMASTER1']",
	"Parked.Press", "['SBERTO1', 'SBETTY1','SHUMP1','SBOSS1']",
	"Parked.Temple", "['SARCHBISHOP1', 'SBISHOP1']",
	"FirstJailBreak", "No",
	"SuperDealType", NULL,
	"PlayerHasBeenKilled", "1",
	NULL,
};

const mmEntry *MissionMaker_Find(const char *mission) {
	for (int i = 0; i < g_MissionMakerCount; i++) {
		if (*g_MissionMakerList[i].mission && !crt_stricmp(g_MissionMakerList[i].mission, mission)) {
			return &g_MissionMakerList[i];
		}
	}
	return NULL;
}

const char *MissionMaker_Level(const char *level) {
	for (int i = 0; i < g_MissionMakerCount; i++) {
		if (!crt_stricmp(g_MissionMakerList[i].level, level)) {
			return g_MissionMakerList[i].level;
		}
	}
	for (int i = 0; i < g_MissionMakerSplitCount; i++) {
		if (!crt_stricmp(g_MissionMakerSplitMaps[i], level)) {
			return g_MissionMakerSplitMaps[i];
		}
	}
	return NULL;
}

/* SaveGame.QLoad("Levels/" + map) */
static byte MM_QLoad(const char *map) {
	char path[0x80];
	Str_PrintfQuiet(path, sizeof(path), "Levels/%s", map);
	return Mode_Import("SaveGame") && PORT_CALL(SaveGame, QLoad, NULL, "s", path);
}

const char *MissionMaker_Char(const char *level) {
	/* Init preloads the module of the character of the save variable only in the outdoor levels, before the map's
	 * InitStatus makes the player the bishop of the GDB: with Dtritus the import of Bishop fails there */
	return !crt_stricmp(level, "GDB") ? "Bishop" : "Dtritus";
}

byte MissionMaker_Load(const char *level, const char *mission, const char *dealType, const char *ch) {
	if (!ch) {
		ch = MissionMaker_Char(level);
	}
	/* Init() of missionmaker.py cleared the save variables first; the module set Police.FicusDeath when it loaded */
	SaveVars_Delete("");
	SaveVar_Set("Police.FicusDeath", "1");
	if (*dealType || (!crt_stricmp(level, "GamblinDen") && crt_stricmp(mission, "Mission_ArrangeMortalRace"))) {
		SaveVar_Set("LonelyMercInGamblinDen", "1");
	}
	if (!crt_stricmp(level, "Outskirts") && !crt_stricmp(mission, "Mission_IdentifyMerc")) {
		SaveVar_Set("LonelyMercDataActive", "1");
	}
	for (int i = 0; mm_defaults[i]; i += 2) {
		const char *v = mm_defaults[i + 1];
		if (!v) {
			v = !strcmp(mm_defaults[i], "Char") ? ch : dealType;
		}
		SaveVar_Create(mm_defaults[i], v);
	}
	if (*mission && !(Mode_Import("MissionsFuncs") && PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", mission, level))) {
		return 0;
	}
	/* differs from missionmaker.py: no auto save into slot 0 at the load, so a jump keeps the player's auto save */
	SaveVar_Set("AutoSaveGameOnLoad", "0");
	GVar_SetNum("ForceReload", 0.0);
	SoundSys_DeleteAllSounds(g_pSound);
	return MM_QLoad(level);
}

byte MissionMaker_Split(const char *map) {
	char path[0x80];
	GVar_SetStr("ServerType", "DeathMatch");
	GVar_SetNum("r_SplitScreen", 1.0);
	GVar_SetNum("SplitScreen", 1.0);
	GVar_SetNum("CameraDefViewAspect", 2.6666);
	GVar_SetNum("R_SceneRTargetSwap", 0.0);
	Str_PrintfQuiet(path, sizeof(path), "Levels/%s/2D/Map2D.tga", map);
	return Mode_Import("SScorer") && PORT_CALL(SScorer, PreloadTexture, NULL, "s", path) && MM_QLoad(map);
}
