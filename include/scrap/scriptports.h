/*
 * The refs of the port functions that have a native entry (PORT_CALL and PORT_OWN, include/scrap/scriptnative.h),
 * one per table function: made by tools/script/ports.py from the function tables of src/game/Script, their
 * registrations in Script.c and Levels.c, the tables of the engine modules (src/game/PyInterface) and the
 * entries. Do not edit: run the tool after a table or an entry changes, and to resolve a merge. The number after
 * each ref is the count of arguments its entry takes.
 */
#ifndef SCRAP_SCRIPTPORTS_H
#define SCRAP_SCRIPTPORTS_H

/* Alarm.c */
PORT_DECL(Alarm, SetAlarm) /* 1 */
PORT_DECL(Alarm, ActivateAlarm) /* 0 */
PORT_DECL(Alarm, DeactivateAlarm) /* 0 */
PORT_DECL(Alarm, Init) /* 0 */
PORT_DECL(NewsPanel, GetNews) /* 0 */
PORT_DECL(NewsPanel, FillNews) /* 1 */
PORT_DECL(NewsPanel, Init) /* 0 */
PORT_DECL(NewsPanel, UseNewsPanel) /* 4 */
PORT_DECL(NewsPanel, EndOfNews) /* 0 */
PORT_DECL(NewsPanel, SwitchAlarm) /* 1 */
#define PORTS_Alarm \
	PORT_DEFN(Alarm, SetAlarm, Alarm_SetAlarm) \
	PORT_DEFN(Alarm, ActivateAlarm, Alarm_Nothing) \
	PORT_DEFN(Alarm, DeactivateAlarm, Alarm_DeactivateAlarm) \
	PORT_DEFN(Alarm, Init, Alarm_Nothing) \
	PORT_DEFN(NewsPanel, GetNews, NewsPanel_GetNews) \
	PORT_DEFN(NewsPanel, FillNews, NewsPanel_FillNews) \
	PORT_DEFN(NewsPanel, Init, NewsPanel_Init) \
	PORT_DEFN(NewsPanel, UseNewsPanel, NewsPanel_UseNewsPanel) \
	PORT_DEFN(NewsPanel, EndOfNews, NewsPanel_EndOfNews) \
	PORT_DEFN(NewsPanel, SwitchAlarm, NewsPanel_SwitchAlarm)

/* Bureaucracy.c */
PORT_DECL(Bureaucracy, CreateBureaucracyClass) /* 2 */
PORT_DECL(Bureaucracy, Init) /* ? */
#define PORTS_Bureaucracy \
	PORT_DEFN(Bureaucracy, CreateBureaucracyClass, Bureaucracy_CreateBureaucracyClass) \
	PORT_DEFNK(Bureaucracy, Init, Bureaucracy_InitPy)

/* Challenge.c */
PORT_DECL(Challenge, ComputeReward) /* 3 */
PORT_DECL(Challenge, Init) /* ? */
PORT_DECL(Challenge, SetChallengerSkills) /* 2 */
PORT_DECL(Challenge, OnChallengeOpt) /* ? */
PORT_DECL(Challenge, SetRevengeParams) /* 3 */
PORT_DECL(Challenge, SetRevenge) /* ? */
PORT_DECL(Challenge, LaunchRevenge) /* ? */
PORT_DECL(Challenge, LaunchChallenge) /* ? */
PORT_DECL(Challenge, CleanChallenge) /* ? */
PORT_DECL(Challenge, CleanRevenge) /* ? */
PORT_DECL(Challenge, CancelChallenge) /* 1 */
PORT_DECL(Challenge, SetChallengeOptions) /* 1 */
PORT_DECL(Challenge, AddChallenge) /* 3 */
#define PORTS_Challenge \
	PORT_DEFN(Challenge, ComputeReward, Challenge_ComputeReward) \
	PORT_DEFN(Challenge, Init, Challenge_Init) \
	PORT_DEFN(Challenge, SetChallengerSkills, Challenge_SetChallengerSkills) \
	PORT_DEFN(Challenge, OnChallengeOpt, Challenge_OnChallengeOpt) \
	PORT_DEFN(Challenge, SetRevengeParams, Challenge_SetRevengeParams) \
	PORT_DEFN(Challenge, SetRevenge, Challenge_SetRevenge) \
	PORT_DEFN(Challenge, LaunchRevenge, Challenge_LaunchRevenge) \
	PORT_DEFN(Challenge, LaunchChallenge, Challenge_LaunchChallenge) \
	PORT_DEFN(Challenge, CleanChallenge, Challenge_CleanChallenge) \
	PORT_DEFN(Challenge, CleanRevenge, Challenge_CleanRevenge) \
	PORT_DEFN(Challenge, CancelChallenge, Challenge_CancelChallenge) \
	PORT_DEFN(Challenge, SetChallengeOptions, Challenge_SetChallengeOptions) \
	PORT_DEFN(Challenge, AddChallenge, Challenge_AddChallenge)

/* CharAct.c */
PORT_DECL(CharAct, ActionPhoto) /* 1 */
PORT_DECL(CharAct, Photograph) /* 1 */
PORT_DECL(CharAct, OnDeath) /* 2 */
PORT_DECL(CharAct, NoFXDeath) /* ? */
PORT_DECL(CharAct, PlayerDeathCall) /* 1 */
PORT_DECL(CharAct, NPCDeathCall) /* 1 */
PORT_DECL(CharAct, OnCreate) /* 1 */
#define PORTS_CharAct \
	PORT_DEFN(CharAct, ActionPhoto, CharAct_ActionPhoto) \
	PORT_DEFN(CharAct, Photograph, CharAct_Photograph) \
	PORT_DEFN(CharAct, OnDeath, CharAct_OnDeath) \
	PORT_DEFN(CharAct, NoFXDeath, CharAct_NoFXDeath) \
	PORT_DEFN(CharAct, PlayerDeathCall, CharAct_PlayerDeathCall) \
	PORT_DEFN(CharAct, NPCDeathCall, CharAct_NPCDeathCall) \
	PORT_DEFN(CharAct, OnCreate, CharAct_OnCreate)

/* CharConversor.c */
PORT_DECL(CharConversor, GetNearestConversionPoint) /* 1 */
PORT_DECL(CharConversor, CreateConversorClass) /* 0 */
PORT_DECL(CharConversor, Init) /* 0 */
PORT_DECL(CharConversor, ConvertChar) /* 4 */
PORT_DECL(CharConversor, CPPos) /* 1 */
PORT_DECL(CharConversor, CPAng) /* 1 */
PORT_DECL(CharConversor, DrawBackground) /* ? */
PORT_DECL(CharConversor, AddInterfaceContent) /* 1 */
PORT_DECL(CharConversor, SpawnChar) /* 2 */
PORT_DECL(CharConversor, MarkSpawnWaypoint) /* 1 */
PORT_DECL(CharConversor, EnterCharConversor) /* 4 */
PORT_DECL(CharConversor, InitOfScene) /* 3 */
PORT_DECL(CharConversor, EndOfScene) /* 3 */
PORT_DECL(CharConversor, InitInterfaceEvent) /* 3 */
PORT_DECL(CharConversor, ExitCharConversor) /* 2 */
PORT_DECL(CharConversor, ShowCharDesc) /* 3 */
PORT_DECL(CharConversor, HideCharDesc) /* 3 */
PORT_DECL(CharConversor, Convert) /* 2 */
PORT_DECL(CharConversor, ConvertExecute) /* 0 */
PORT_DECL(CharConversor, Possession) /* 2 */
PORT_DECL(CharConversor, NewPlayer) /* 2 */
#define PORTS_CharConversor \
	PORT_DEFN(CharConversor, GetNearestConversionPoint, CharConversor_GetNearestConversionPoint) \
	PORT_DEFN(CharConversor, CreateConversorClass, CharConversor_CreateConversorClass) \
	PORT_DEFN(CharConversor, Init, CharConversor_Init) \
	PORT_DEFN(CharConversor, ConvertChar, CharConversor_ConvertChar) \
	PORT_DEFN(CharConversor, CPPos, CharConversor_CPPos) \
	PORT_DEFN(CharConversor, CPAng, CharConversor_CPAng) \
	PORT_DEFNK(CharConversor, DrawBackground, CharConversor_DrawBackground) \
	PORT_DEFN(CharConversor, AddInterfaceContent, CharConversor_AddInterfaceContent) \
	PORT_DEFN(CharConversor, SpawnChar, CharConversor_SpawnChar) \
	PORT_DEFN(CharConversor, MarkSpawnWaypoint, CharConversor_MarkSpawnWaypoint) \
	PORT_DEFN(CharConversor, EnterCharConversor, CharConversor_EnterCharConversor) \
	PORT_DEFN(CharConversor, InitOfScene, CharConversor_InitOfScene) \
	PORT_DEFN(CharConversor, EndOfScene, CharConversor_EndOfScene) \
	PORT_DEFN(CharConversor, InitInterfaceEvent, CharConversor_InitInterfaceEvent) \
	PORT_DEFN(CharConversor, ExitCharConversor, CharConversor_ExitCharConversor) \
	PORT_DEFN(CharConversor, ShowCharDesc, CharConversor_ShowCharDesc) \
	PORT_DEFN(CharConversor, HideCharDesc, CharConversor_HideCharDesc) \
	PORT_DEFN(CharConversor, Convert, CharConversor_Convert) \
	PORT_DEFN(CharConversor, ConvertExecute, CharConversor_ConvertExecute) \
	PORT_DEFN(CharConversor, Possession, CharConversor_Possession) \
	PORT_DEFN(CharConversor, NewPlayer, CharConversor_NewPlayer)

/* CharFuncs.c */
PORT_DECL(Boss, ActionPress) /* ? */
PORT_DECL(Boss, BossSpeech) /* ? */
PORT_DECL(CrazyGambler, CalculateStatus) /* ? */
PORT_DECL(CrazyGambler, CrazyGamblerSpeech) /* ? */
PORT_DECL(CrazyGambler, StartSuperDeal) /* ? */
PORT_DECL(Maintenance, MaintenanceTalk) /* 4 */
PORT_DECL(Maintenance, MaintenanceWorkFX) /* ? */
PORT_DECL(Mayor, MayorActionPress) /* ? */
PORT_DECL(Mayor, MayorReCheckAction) /* ? */
PORT_DECL(Rusty, ActionPress) /* ? */
PORT_DECL(Rusty, ActionRelease) /* ? */
PORT_DECL(SputnikJunkyard, SputnikJunkyardTalk) /* 4 */
PORT_DECL(Bishop, AddLifes) /* ? */
PORT_DECL(Bishop, BishopSpeech) /* ? */
PORT_DECL(Bishop, StartSellingLife) /* 1 */
PORT_DECL(Bishop, SellLife) /* 3 */
PORT_DECL(Bishop, FXBishopSellLife) /* ? */
PORT_DECL(Functionary, EnterFastEffect) /* ? */
PORT_DECL(Functionary, ExitFastEffect) /* ? */
PORT_DECL(Functionary, ActionPress) /* ? */
PORT_DECL(Functionary, ActionRelease) /* ? */
PORT_DECL(Gear, StartSirenLoop) /* ? */
PORT_DECL(Gear, AbortSiren) /* ? */
PORT_DECL(Police, AbortSiren) /* ? */
PORT_DECL(Police, GoToMission) /* ? */
PORT_DECL(Police, PoliceSpeech) /* ? */
PORT_DECL(Police, OnEndOfSpeech) /* ? */
PORT_DECL(Messenger, MakeDataPack) /* ? */
PORT_DECL(Messenger, DoorInitializator) /* ? */
PORT_DECL(Messenger, ObjInitializator) /* 2 */
#define PORTS_CharFuncs \
	PORT_DEFN(Boss, ActionPress, Boss_ActionPress) \
	PORT_DEFN(Boss, BossSpeech, Boss_BossSpeech) \
	PORT_DEFN(CrazyGambler, CalculateStatus, CrazyGambler_CalculateStatus) \
	PORT_DEFN(CrazyGambler, CrazyGamblerSpeech, CrazyGambler_CrazyGamblerSpeech) \
	PORT_DEFN(CrazyGambler, StartSuperDeal, CrazyGambler_StartSuperDeal) \
	PORT_DEFN(Maintenance, MaintenanceTalk, Char_CantTalk) \
	PORT_DEFN(Maintenance, MaintenanceWorkFX, Maintenance_MaintenanceWorkFX) \
	PORT_DEFN(Mayor, MayorActionPress, Mayor_MayorActionPress) \
	PORT_DEFN(Mayor, MayorReCheckAction, Mayor_MayorReCheckAction) \
	PORT_DEFN(Rusty, ActionPress, Mode_PyNone) \
	PORT_DEFN(Rusty, ActionRelease, Mode_PyNone) \
	PORT_DEFN(SputnikJunkyard, SputnikJunkyardTalk, Char_CantTalk) \
	PORT_DEFN(Bishop, AddLifes, Bishop_AddLifesPy) \
	PORT_DEFN(Bishop, BishopSpeech, Bishop_BishopSpeech) \
	PORT_DEFN(Bishop, StartSellingLife, Bishop_StartSellingLife) \
	PORT_DEFN(Bishop, SellLife, Bishop_SellLife) \
	PORT_DEFN(Bishop, FXBishopSellLife, Bishop_FXBishopSellLife) \
	PORT_DEFN(Functionary, EnterFastEffect, Functionary_EnterFastEffect) \
	PORT_DEFN(Functionary, ExitFastEffect, Functionary_ExitFastEffect) \
	PORT_DEFN(Functionary, ActionPress, Functionary_ActionPress) \
	PORT_DEFN(Functionary, ActionRelease, Functionary_ActionRelease) \
	PORT_DEFN(Gear, StartSirenLoop, Gear_StartSirenLoop) \
	PORT_DEFN(Gear, AbortSiren, Char_AbortSiren) \
	PORT_DEFN(Police, AbortSiren, Char_AbortSiren) \
	PORT_DEFN(Police, GoToMission, Police_GoToMission) \
	PORT_DEFN(Police, PoliceSpeech, Police_PoliceSpeech) \
	PORT_DEFN(Police, OnEndOfSpeech, Police_OnEndOfSpeech) \
	PORT_DEFN(Messenger, MakeDataPack, Messenger_MakeDataPack) \
	PORT_DEFN(Messenger, DoorInitializator, Messenger_DoorInitializator) \
	PORT_DEFN(Messenger, ObjInitializator, Messenger_ObjInitializator)

/* Characters.c */
PORT_DECL(Betty, Init) /* ? */
PORT_DECL(Betty, OnCreate) /* ? */
PORT_DECL(Betty, OnFreeResources) /* ? */
PORT_DECL(Betty, EnterSlowSpeed) /* ? */
PORT_DECL(Betty, ExitSlowSpeed) /* ? */
PORT_DECL(BankDirector, Init) /* ? */
PORT_DECL(BankDirector, OnCreate) /* ? */
PORT_DECL(BankDirector, OnFreeResources) /* ? */
PORT_DECL(Boss, Init) /* ? */
PORT_DECL(Boss, OnCreate) /* ? */
PORT_DECL(CrazyGambler, Init) /* ? */
PORT_DECL(CrazyGambler, OnCreate) /* ? */
PORT_DECL(Humphrey, Init) /* ? */
PORT_DECL(Humphrey, OnCreate) /* ? */
PORT_DECL(Maintenance, Init) /* ? */
PORT_DECL(Maintenance, OnCreate) /* ? */
PORT_DECL(Mayor, Init) /* ? */
PORT_DECL(Mayor, OnCreate) /* ? */
PORT_DECL(Rusty, Init) /* ? */
PORT_DECL(Rusty, OnCreate) /* ? */
PORT_DECL(Rusty, OnFreeResources) /* ? */
PORT_DECL(Sebastian, Init) /* ? */
PORT_DECL(Sebastian, OnCreate) /* ? */
PORT_DECL(Sebastian, OnFreeResources) /* ? */
PORT_DECL(Sentinel, Init) /* ? */
PORT_DECL(Sentinel, OnCreate) /* ? */
PORT_DECL(SputnikJunkyard, Init) /* ? */
PORT_DECL(SputnikJunkyard, OnCreate) /* ? */
PORT_DECL(Bishop, Init) /* ? */
PORT_DECL(Bishop, OnCreate) /* ? */
PORT_DECL(Bishop, OnFreeResources) /* ? */
PORT_DECL(Functionary, Init) /* ? */
PORT_DECL(Functionary, OnCreate) /* ? */
PORT_DECL(Functionary, OnFreeResources) /* ? */
PORT_DECL(Gear, Init) /* ? */
PORT_DECL(Gear, OnCreate) /* ? */
PORT_DECL(Gear, OnFreeResources) /* ? */
PORT_DECL(Gear, LoadSirenActs) /* ? */
PORT_DECL(Gear, DropMissileEvent) /* 2 */
PORT_DECL(BankMaster, Init) /* ? */
PORT_DECL(BankMaster, OnCreate) /* ? */
PORT_DECL(BankMaster, OnFreeResources) /* ? */
PORT_DECL(Berto, Init) /* ? */
PORT_DECL(Berto, OnCreate) /* ? */
PORT_DECL(Berto, OnFreeResources) /* ? */
PORT_DECL(Nurse, Init) /* ? */
PORT_DECL(Nurse, OnCreate) /* ? */
PORT_DECL(Nurse, OnFreeResources) /* ? */
PORT_DECL(Desktop, Init) /* ? */
PORT_DECL(Desktop, OnCreate) /* ? */
PORT_DECL(Desktop, OnFreeResources) /* ? */
PORT_DECL(Police, Init) /* ? */
PORT_DECL(Police, OnCreate) /* ? */
PORT_DECL(Police, OnFreeResources) /* ? */
PORT_DECL(Police, LoadSirenActs) /* ? */
PORT_DECL(GateKeeper, Init) /* ? */
PORT_DECL(Dtritus, Init) /* ? */
PORT_DECL(Dtritus, OnCreate) /* ? */
PORT_DECL(Dtritus, OnFreeResources) /* ? */
PORT_DECL(Sputnik, Init) /* ? */
PORT_DECL(Sputnik, OnCreate) /* ? */
PORT_DECL(PoliceBoss, Init) /* ? */
PORT_DECL(PoliceBoss, OnCreate) /* ? */
PORT_DECL(PoliceBoss, OnFreeResources) /* ? */
PORT_DECL(Messenger, Init) /* ? */
PORT_DECL(Messenger, OnCreate) /* ? */
PORT_DECL(Messenger, OnFreeResources) /* ? */
PORT_DECL(Human, Init) /* ? */
PORT_DECL(Human, OnCreate) /* ? */
PORT_DECL(MercenaryA, Init) /* ? */
PORT_DECL(MercenaryA, OnCreate) /* ? */
PORT_DECL(MercenaryB, Init) /* ? */
PORT_DECL(MercenaryC, Init) /* ? */
PORT_DECL(MercenaryL, Init) /* ? */
#define PORTS_Characters \
	PORT_DEFN(Betty, Init, Betty_Init) \
	PORT_DEFN(Betty, OnCreate, Betty_OnCreate) \
	PORT_DEFN(Betty, OnFreeResources, Betty_OnFreeResources) \
	PORT_DEFN(Betty, EnterSlowSpeed, Betty_EnterSlowSpeed) \
	PORT_DEFN(Betty, ExitSlowSpeed, Betty_ExitSlowSpeed) \
	PORT_DEFN(BankDirector, Init, BankDirector_Init) \
	PORT_DEFN(BankDirector, OnCreate, BankDirector_OnCreate) \
	PORT_DEFN(BankDirector, OnFreeResources, BankDirector_OnFreeResources) \
	PORT_DEFN(Boss, Init, Boss_Init) \
	PORT_DEFN(Boss, OnCreate, Boss_OnCreate) \
	PORT_DEFN(CrazyGambler, Init, CrazyGambler_Init) \
	PORT_DEFN(CrazyGambler, OnCreate, CrazyGambler_OnCreate) \
	PORT_DEFN(Humphrey, Init, Humphrey_Init) \
	PORT_DEFN(Humphrey, OnCreate, Humphrey_OnCreate) \
	PORT_DEFN(Maintenance, Init, Maintenance_Init) \
	PORT_DEFN(Maintenance, OnCreate, Maintenance_OnCreate) \
	PORT_DEFN(Mayor, Init, Mayor_Init) \
	PORT_DEFN(Mayor, OnCreate, Mayor_OnCreate) \
	PORT_DEFN(Rusty, Init, Rusty_Init) \
	PORT_DEFN(Rusty, OnCreate, Rusty_OnCreate) \
	PORT_DEFN(Rusty, OnFreeResources, Rusty_OnFreeResources) \
	PORT_DEFN(Sebastian, Init, Sebastian_Init) \
	PORT_DEFN(Sebastian, OnCreate, Sebastian_OnCreate) \
	PORT_DEFN(Sebastian, OnFreeResources, Sebastian_OnFreeResources) \
	PORT_DEFN(Sentinel, Init, Sentinel_Init) \
	PORT_DEFN(Sentinel, OnCreate, Sentinel_OnCreate) \
	PORT_DEFN(SputnikJunkyard, Init, SputnikJunkyard_Init) \
	PORT_DEFN(SputnikJunkyard, OnCreate, SputnikJunkyard_OnCreate) \
	PORT_DEFN(Bishop, Init, Bishop_Init) \
	PORT_DEFN(Bishop, OnCreate, Bishop_OnCreate) \
	PORT_DEFN(Bishop, OnFreeResources, Bishop_OnFreeResources) \
	PORT_DEFN(Functionary, Init, Functionary_Init) \
	PORT_DEFN(Functionary, OnCreate, Functionary_OnCreate) \
	PORT_DEFN(Functionary, OnFreeResources, Functionary_OnFreeResources) \
	PORT_DEFN(Gear, Init, Gear_Init) \
	PORT_DEFN(Gear, OnCreate, Gear_OnCreate) \
	PORT_DEFN(Gear, OnFreeResources, Gear_OnFreeResources) \
	PORT_DEFN(Gear, LoadSirenActs, Gear_LoadSirenActs) \
	PORT_DEFN(Gear, DropMissileEvent, Gear_DropMissileEvent) \
	PORT_DEFN(BankMaster, Init, BankMaster_Init) \
	PORT_DEFN(BankMaster, OnCreate, BankMaster_OnCreate) \
	PORT_DEFN(BankMaster, OnFreeResources, BankMaster_OnFreeResources) \
	PORT_DEFN(Berto, Init, Berto_Init) \
	PORT_DEFN(Berto, OnCreate, Berto_OnCreate) \
	PORT_DEFN(Berto, OnFreeResources, Berto_OnFreeResources) \
	PORT_DEFN(Nurse, Init, Nurse_Init) \
	PORT_DEFN(Nurse, OnCreate, Nurse_OnCreate) \
	PORT_DEFN(Nurse, OnFreeResources, Nurse_OnFreeResources) \
	PORT_DEFN(Desktop, Init, Desktop_Init) \
	PORT_DEFN(Desktop, OnCreate, Desktop_OnCreate) \
	PORT_DEFN(Desktop, OnFreeResources, Desktop_OnFreeResources) \
	PORT_DEFN(Police, Init, Police_Init) \
	PORT_DEFN(Police, OnCreate, Police_OnCreate) \
	PORT_DEFN(Police, OnFreeResources, Police_OnFreeResources) \
	PORT_DEFN(Police, LoadSirenActs, Police_LoadSirenActs) \
	PORT_DEFN(GateKeeper, Init, GateKeeper_Init) \
	PORT_DEFN(Dtritus, Init, Dtritus_Init) \
	PORT_DEFN(Dtritus, OnCreate, Dtritus_OnCreate) \
	PORT_DEFN(Dtritus, OnFreeResources, Dtritus_OnFreeResources) \
	PORT_DEFN(Sputnik, Init, Sputnik_Init) \
	PORT_DEFN(Sputnik, OnCreate, Sputnik_OnCreate) \
	PORT_DEFN(PoliceBoss, Init, PoliceBoss_Init) \
	PORT_DEFN(PoliceBoss, OnCreate, PoliceBoss_OnCreate) \
	PORT_DEFN(PoliceBoss, OnFreeResources, PoliceBoss_OnFreeResources) \
	PORT_DEFN(Messenger, Init, Messenger_Init) \
	PORT_DEFN(Messenger, OnCreate, Messenger_OnCreate) \
	PORT_DEFN(Messenger, OnFreeResources, Messenger_OnFreeResources) \
	PORT_DEFN(Human, Init, Human_Init) \
	PORT_DEFN(Human, OnCreate, Human_OnCreate) \
	PORT_DEFN(MercenaryA, Init, MercenaryA_Init) \
	PORT_DEFN(MercenaryA, OnCreate, MercenaryA_OnCreate) \
	PORT_DEFN(MercenaryB, Init, MercenaryB_Init) \
	PORT_DEFN(MercenaryC, Init, MercenaryC_Init) \
	PORT_DEFN(MercenaryL, Init, MercenaryL_Init)

/* Chars.c */
PORT_DECL(Chars, CanDrive) /* 1 */
PORT_DECL(Chars, CreateMainChar) /* 4 */
PORT_DECL(Chars, SetOnFloor) /* 1 */
PORT_DECL(Chars, IntSetChar) /* 2 */
PORT_DECL(Chars, ActSound) /* ? */
PORT_DECL(Chars, StrSound) /* ? */
PORT_DECL(Chars, StrSoundStop) /* 3 */
PORT_DECL(Chars, ActSoundLoop) /* ? */
PORT_DECL(Chars, DiscardRes) /* 1 */
PORT_DECL(Chars, TryToDiscardThis) /* 1 */
PORT_DECL(Chars, LoadRes) /* 1 */
PORT_DECL(Chars, AddNewCharRes) /* ? */
PORT_DECL(Chars, LocalLoadRes) /* 1 */
PORT_DECL(Chars, MainCharDeath) /* 2 */
PORT_DECL(Chars, CreatePlayersList) /* 0 */
PORT_DECL(Chars, Init) /* ? */
PORT_DECL(Chars, CreateStartupPlayer) /* 0 */
#define PORTS_Chars \
	PORT_DEFN(Chars, CanDrive, Chars_CanDrive) \
	PORT_DEFN(Chars, CreateMainChar, Chars_CreateMainChar) \
	PORT_DEFN(Chars, SetOnFloor, Chars_SetOnFloor) \
	PORT_DEFN(Chars, IntSetChar, Chars_IntSetChar) \
	PORT_DEFN(Chars, ActSound, Chars_ActSound) \
	PORT_DEFN(Chars, StrSound, Chars_StrSound) \
	PORT_DEFN(Chars, StrSoundStop, Chars_StrSoundStop) \
	PORT_DEFN(Chars, ActSoundLoop, Chars_ActSoundLoop) \
	PORT_DEFN(Chars, DiscardRes, Chars_DiscardRes) \
	PORT_DEFN(Chars, TryToDiscardThis, Chars_TryToDiscardThis) \
	PORT_DEFN(Chars, LoadRes, Chars_LoadRes) \
	PORT_DEFN(Chars, AddNewCharRes, Chars_AddNewCharRes) \
	PORT_DEFN(Chars, LocalLoadRes, Chars_LocalLoadRes) \
	PORT_DEFN(Chars, MainCharDeath, Chars_MainCharDeath) \
	PORT_DEFN(Chars, CreatePlayersList, Chars_CreatePlayersList) \
	PORT_DEFN(Chars, Init, Chars_Init) \
	PORT_DEFN(Chars, CreateStartupPlayer, Chars_CreateStartupPlayer)

/* CharsNPC.c */
PORT_DECL(CharsNPC, AddNPCPos) /* ? */
PORT_DECL(CharsNPC, CreateMissionChar) /* 1 */
PORT_DECL(CharsNPC, DeleteMissionChar) /* 1 */
PORT_DECL(CharsNPC, SetReposChars) /* 2 */
PORT_DECL(CharsNPC, CreateReposChars) /* ? */
PORT_DECL(CharsNPC, BrowseNPC) /* ? */
PORT_DECL(CharsNPC, AssignAttrib) /* 1 */
PORT_DECL(CharsNPC, CallCreateChar) /* 4 */
PORT_DECL(CharsNPC, CreateChar) /* ? */
PORT_DECL(CharsNPC, CharNPCInclude) /* 1 */
PORT_DECL(CharsNPC, Respawn) /* 1 */
PORT_DECL(CharsNPC, ActiveSpawned) /* 1 */
PORT_DECL(CharsNPC, Init) /* ? */
PORT_DECL(CharsNPC, CheckActualEntity) /* 1 */
PORT_DECL(CharsNPC, MercsHired) /* ? */
PORT_DECL(CharsNPC, IncludeSubOptions) /* 1 */
PORT_DECL(CharsNPC, Classify) /* 2 */
PORT_DECL(CharsNPC, ClassifyOptions) /* 1 */
PORT_DECL(CharsNPC, PlayOption) /* 5 */
PORT_DECL(CharsNPC, GenericNPCSpeech) /* 4 */
#define PORTS_CharsNPC \
	PORT_DEFN(CharsNPC, AddNPCPos, CharsNPC_AddNPCPos) \
	PORT_DEFN(CharsNPC, CreateMissionChar, CharsNPC_CreateMissionChar) \
	PORT_DEFN(CharsNPC, DeleteMissionChar, CharsNPC_DeleteMissionChar) \
	PORT_DEFN(CharsNPC, SetReposChars, CharsNPC_SetReposChars) \
	PORT_DEFN(CharsNPC, CreateReposChars, CharsNPC_CreateReposChars) \
	PORT_DEFN(CharsNPC, BrowseNPC, CharsNPC_BrowseNPC) \
	PORT_DEFN(CharsNPC, AssignAttrib, CharsNPC_AssignAttrib) \
	PORT_DEFN(CharsNPC, CallCreateChar, CharsNPC_CallCreateChar) \
	PORT_DEFN(CharsNPC, CreateChar, CharsNPC_CreateChar) \
	PORT_DEFN(CharsNPC, CharNPCInclude, CharsNPC_CharNPCInclude) \
	PORT_DEFN(CharsNPC, Respawn, CharsNPC_Respawn) \
	PORT_DEFN(CharsNPC, ActiveSpawned, CharsNPC_ActiveSpawned) \
	PORT_DEFN(CharsNPC, Init, CharsNPC_Init) \
	PORT_DEFN(CharsNPC, CheckActualEntity, CharsNPC_CheckActualEntity) \
	PORT_DEFN(CharsNPC, MercsHired, CharsNPC_MercsHired) \
	PORT_DEFN(CharsNPC, IncludeSubOptions, CharsNPC_IncludeSubOptions) \
	PORT_DEFN(CharsNPC, Classify, CharsNPC_Classify) \
	PORT_DEFN(CharsNPC, ClassifyOptions, CharsNPC_ClassifyOptions) \
	PORT_DEFN(CharsNPC, PlayOption, CharsNPC_PlayOption) \
	PORT_DEFN(CharsNPC, GenericNPCSpeech, CharsNPC_GenericNPCSpeech)

/* Chat.c */
PORT_DECL(Chat, GetID) /* ? */
PORT_DECL(Chat, CycleUp) /* ? */
PORT_DECL(Chat, CycleUpAndSet) /* ? */
PORT_DECL(Chat, CycleDown) /* ? */
PORT_DECL(Chat, CycleDownAndSet) /* ? */
PORT_DECL(Chat, ServerReSendChat) /* 2 */
PORT_DECL(Chat, PrintChatString) /* 2 */
PORT_DECL(Chat, SendChatString) /* ? */
PORT_DECL(Chat, EndChat) /* ? */
PORT_DECL(Chat, GetMyShip) /* 0 */
PORT_DECL(Chat, InitChat) /* ? */
PORT_DECL(Chat, ConsoleChatString) /* 1 */
PORT_DECL(Chat, qchat1) /* ? */
PORT_DECL(Chat, qchat2) /* ? */
PORT_DECL(Chat, qchat3) /* ? */
PORT_DECL(Chat, qchat4) /* ? */
PORT_DECL(Chat, qchat5) /* ? */
PORT_DECL(Chat, qchat6) /* ? */
PORT_DECL(Chat, SetChatProc) /* 0 */
#define PORTS_Chat \
	PORT_DEFN(Chat, GetID, Chat_GetID) \
	PORT_DEFN(Chat, CycleUp, Chat_CycleUp) \
	PORT_DEFN(Chat, CycleUpAndSet, Chat_CycleUpAndSet) \
	PORT_DEFN(Chat, CycleDown, Chat_CycleDown) \
	PORT_DEFN(Chat, CycleDownAndSet, Chat_CycleDownAndSet) \
	PORT_DEFN(Chat, ServerReSendChat, Chat_ServerReSendChat) \
	PORT_DEFN(Chat, PrintChatString, Chat_PrintChatString) \
	PORT_DEFN(Chat, SendChatString, Chat_SendChatString) \
	PORT_DEFN(Chat, EndChat, Chat_EndChat) \
	PORT_DEFN(Chat, GetMyShip, Chat_GetMyShip) \
	PORT_DEFN(Chat, InitChat, Chat_InitChat) \
	PORT_DEFN(Chat, ConsoleChatString, Chat_ConsoleChatString) \
	PORT_DEFN(Chat, qchat1, Chat_qchat1) \
	PORT_DEFN(Chat, qchat2, Chat_qchat2) \
	PORT_DEFN(Chat, qchat3, Chat_qchat3) \
	PORT_DEFN(Chat, qchat4, Chat_qchat4) \
	PORT_DEFN(Chat, qchat5, Chat_qchat5) \
	PORT_DEFN(Chat, qchat6, Chat_qchat6) \
	PORT_DEFN(Chat, SetChatProc, Chat_SetChatProc)

/* CreditsText.c */
PORT_DECL(CreditsText, Init) /* 0 */
PORT_DECL(CreditsText, CreditsScorer) /* ? */
PORT_DECL(CreditsText, PrintDataGuay) /* ? */
PORT_DECL(CreditsText, PrintData) /* 1 */
PORT_DECL(CreditsText, ShowUp) /* ? */
PORT_DECL(CreditsText, ShowDown) /* ? */
PORT_DECL(CreditsText, CleanCredits) /* 0 */
PORT_DECL(CreditsText, ShowCredit) /* 1 */
#define PORTS_CreditsText \
	PORT_DEFN(CreditsText, Init, CreditsText_Init) \
	PORT_DEFN(CreditsText, CreditsScorer, CreditsText_CreditsScorer) \
	PORT_DEFN(CreditsText, PrintDataGuay, CreditsText_PrintDataGuay) \
	PORT_DEFN(CreditsText, PrintData, CreditsText_PrintData) \
	PORT_DEFN(CreditsText, ShowUp, CreditsText_ShowUp) \
	PORT_DEFN(CreditsText, ShowDown, CreditsText_ShowDown) \
	PORT_DEFN(CreditsText, CleanCredits, CreditsText_CleanCredits) \
	PORT_DEFN(CreditsText, ShowCredit, CreditsText_ShowCredit)

/* Doors.c */
PORT_DECL(Doors, StdInitializator) /* 1 */
PORT_DECL(Doors, StdObjInitializator) /* 2 */
PORT_DECL(Doors, UsrObjInitializator) /* 2 */
PORT_DECL(Doors, CreateDoorClass) /* 1 */
PORT_DECL(Doors, InitDoorInfo) /* 0 */
PORT_DECL(Doors, Init) /* ? */
PORT_DECL(Elevator, isOutMap) /* 1 */
PORT_DECL(Elevator, ElevatorCamera) /* 1 */
PORT_DECL(Elevator, ElevatorBackCamera) /* 0 */
PORT_DECL(Elevator, ConnectElevators) /* ? */
PORT_DECL(Elevator, DoorInitializator) /* ? */
PORT_DECL(Elevator, ObjInitializator) /* 2 */
PORT_DECL(Elevator, ObjInitializatorNang) /* 2 */
PORT_DECL(Elevator, UseElevator) /* 4 */
PORT_DECL(Elevator, ElevatorActiveToGet) /* 1 */
PORT_DECL(Elevator, ElevatorCloseToGo) /* 1 */
PORT_DECL(Elevator, ElevatorHaveClose) /* 3 */
PORT_DECL(Elevator, ElevatorReachUp) /* 3 */
PORT_DECL(Elevator, ElevatorReachDown) /* 3 */
PORT_DECL(Elevator, ElevatorHaveOpen) /* 3 */
PORT_DECL(Elevator, LinkToElevator) /* ? */
PORT_DECL(Elevator, UnlinkFromElevator) /* 1 */
PORT_DECL(Elevator, Init) /* 0 */
PORT_DECL(Elevator, SetDef) /* 2 */
PORT_DECL(Elevator, TryToElevatorLaunchFirst) /* 0 */
PORT_DECL(Elevator, PlayerCreatedElevator) /* 0 */
PORT_DECL(Teleport, DoorInitializator) /* 1 */
PORT_DECL(Teleport, ObjInitializator) /* 2 */
PORT_DECL(Teleport, Link) /* 2 */
PORT_DECL(Teleport, FXAction) /* 3 */
#define PORTS_Doors \
	PORT_DEFN(Doors, StdInitializator, Doors_StdInitializator) \
	PORT_DEFN(Doors, StdObjInitializator, Doors_StdObjInitializator) \
	PORT_DEFN(Doors, UsrObjInitializator, Doors_UsrObjInitializator) \
	PORT_DEFN(Doors, CreateDoorClass, Doors_CreateDoorClass) \
	PORT_DEFN(Doors, InitDoorInfo, Doors_InitDoorInfo) \
	PORT_DEFN(Doors, Init, Doors_Init) \
	PORT_DEFN(Elevator, isOutMap, Elevator_isOutMap) \
	PORT_DEFN(Elevator, ElevatorCamera, Elevator_ElevatorCamera) \
	PORT_DEFN(Elevator, ElevatorBackCamera, Elevator_ElevatorBackCamera) \
	PORT_DEFN(Elevator, ConnectElevators, Elevator_ConnectElevators) \
	PORT_DEFN(Elevator, DoorInitializator, Elevator_DoorInitializator) \
	PORT_DEFN(Elevator, ObjInitializator, Elevator_ObjInitializator) \
	PORT_DEFN(Elevator, ObjInitializatorNang, Elevator_ObjInitializatorNang) \
	PORT_DEFN(Elevator, UseElevator, Elevator_UseElevator) \
	PORT_DEFN(Elevator, ElevatorActiveToGet, Elevator_ElevatorActiveToGet) \
	PORT_DEFN(Elevator, ElevatorCloseToGo, Elevator_ElevatorCloseToGo) \
	PORT_DEFN(Elevator, ElevatorHaveClose, Elevator_ElevatorHaveClose) \
	PORT_DEFN(Elevator, ElevatorReachUp, Elevator_ElevatorReachUp) \
	PORT_DEFN(Elevator, ElevatorReachDown, Elevator_ElevatorReachDown) \
	PORT_DEFN(Elevator, ElevatorHaveOpen, Elevator_ElevatorHaveOpen) \
	PORT_DEFN(Elevator, LinkToElevator, Elevator_LinkToElevator) \
	PORT_DEFN(Elevator, UnlinkFromElevator, Elevator_UnlinkFromElevator) \
	PORT_DEFN(Elevator, Init, Elevator_Init) \
	PORT_DEFN(Elevator, SetDef, Elevator_SetDef) \
	PORT_DEFN(Elevator, TryToElevatorLaunchFirst, Elevator_TryToElevatorLaunchFirst) \
	PORT_DEFN(Elevator, PlayerCreatedElevator, Elevator_PlayerCreatedElevator) \
	PORT_DEFN(Teleport, DoorInitializator, Teleport_DoorInitializator) \
	PORT_DEFN(Teleport, ObjInitializator, Teleport_ObjInitializator) \
	PORT_DEFN(Teleport, Link, Teleport_Link) \
	PORT_DEFN(Teleport, FXAction, Teleport_FXAction)

/* EnginesTab.c */
PORT_DECL(EnginesTab, HighlightCurrentMountedEngine) /* ? */
PORT_DECL(EnginesTab, ShowEngineDesc) /* 3 */
PORT_DECL(EnginesTab, HideEngineDesc) /* 3 */
PORT_DECL(EnginesTab, ChangeRecycleButtonState) /* 1 */
PORT_DECL(EnginesTab, UpdateEnginesList) /* 1 */
PORT_DECL(EnginesTab, PutEngine) /* 2 */
PORT_DECL(EnginesTab, RecycleEngine) /* 2 */
PORT_DECL(EnginesTab, ShowMoneySellingEngine) /* 3 */
PORT_DECL(EnginesTab, SelectEngineGroup) /* ? */
PORT_DECL(EnginesTab, SetEnginesPanel) /* 1 */
PORT_DECL(EnginesTab, SetPrevNextEngineVisible) /* ? */
PORT_DECL(EnginesTab, SetRecycleVisible) /* ? */
PORT_DECL(EnginesTab, ShowEnginesPanel) /* 1 */
PORT_DECL(EnginesTab, CreateLayouts) /* 1 */
PORT_DECL(EnginesTab, UpdateLayouts) /* 1 */
PORT_DECL(EnginesTab, HideLayouts) /* ? */
PORT_DECL(EnginesTab, UpdateLayoutsState) /* ? */
PORT_DECL(EnginesTab, UpdateLayoutsGlow) /* 2 */
PORT_DECL(EnginesTab, LayoutsGainFocus) /* 3 */
PORT_DECL(EnginesTab, LayoutsLostFocus) /* 3 */
PORT_DECL(EnginesTab, HideEnginesPanel) /* 1 */
PORT_DECL(EnginesTab, ScrollEngineListUp) /* 2 */
PORT_DECL(EnginesTab, ScrollEngineListDown) /* 2 */
#define PORTS_EnginesTab \
	PORT_DEFNK(EnginesTab, HighlightCurrentMountedEngine, ET_HighlightCurrentMountedEngine) \
	PORT_DEFN(EnginesTab, ShowEngineDesc, ET_ShowEngineDescPy) \
	PORT_DEFN(EnginesTab, HideEngineDesc, ET_HideEngineDesc) \
	PORT_DEFN(EnginesTab, ChangeRecycleButtonState, ET_ChangeRecycleButtonState) \
	PORT_DEFN(EnginesTab, UpdateEnginesList, ET_UpdateEnginesList) \
	PORT_DEFN(EnginesTab, PutEngine, ET_PutEnginePy) \
	PORT_DEFN(EnginesTab, RecycleEngine, ET_RecycleEnginePy) \
	PORT_DEFN(EnginesTab, ShowMoneySellingEngine, ET_ShowMoneySellingEnginePy) \
	PORT_DEFNK(EnginesTab, SelectEngineGroup, ET_SelectEngineGroupPy) \
	PORT_DEFN(EnginesTab, SetEnginesPanel, ET_SetEnginesPanelPy) \
	PORT_DEFN(EnginesTab, SetPrevNextEngineVisible, ET_SetPrevNextEngineVisible) \
	PORT_DEFN(EnginesTab, SetRecycleVisible, ET_SetRecycleVisible) \
	PORT_DEFN(EnginesTab, ShowEnginesPanel, ET_ShowEnginesPanelPy) \
	PORT_DEFN(EnginesTab, CreateLayouts, ET_CreateLayoutsPy) \
	PORT_DEFN(EnginesTab, UpdateLayouts, ET_UpdateLayoutsPy) \
	PORT_DEFN(EnginesTab, HideLayouts, ET_HideLayouts) \
	PORT_DEFN(EnginesTab, UpdateLayoutsState, ET_UpdateLayoutsStatePy) \
	PORT_DEFN(EnginesTab, UpdateLayoutsGlow, ET_UpdateLayoutsGlowPy) \
	PORT_DEFN(EnginesTab, LayoutsGainFocus, ET_LayoutsGainFocus) \
	PORT_DEFN(EnginesTab, LayoutsLostFocus, ET_LayoutsLostFocus) \
	PORT_DEFN(EnginesTab, HideEnginesPanel, ET_HideEnginesPanelPy) \
	PORT_DEFN(EnginesTab, ScrollEngineListUp, ET_ScrollEngineListUp) \
	PORT_DEFN(EnginesTab, ScrollEngineListDown, ET_ScrollEngineListDown)

/* Ffa.c */
PORT_DECL(Ffa, InitFfa) /* 0 */
PORT_DECL(Ffa, SetupUserShip) /* 1 */
PORT_DECL(Ffa, RestartOnPress) /* 1 */
PORT_DECL(Ffa, DelayedRestartOnPress) /* 1 */
PORT_DECL(Ffa, OnDeath) /* 2 */
PORT_DECL(Ffa, ShowPresFireMessage) /* 2 */
PORT_DECL(Ffa, DelayedRestartOnTime) /* 1 */
PORT_DECL(Ffa, EndOfGame) /* 0 */
PORT_DECL(Ffa, ServerSendPlayerStatus) /* 1 */
PORT_DECL(Ffa, ServerSendTeamData) /* 0 */
PORT_DECL(Ffa, ServerEndOfGame) /* 0 */
PORT_DECL(Ffa, OnDeathServer) /* 2 */
PORT_DECL(Ffa, OnDeathClient) /* 2 */
PORT_DECL(Ffa, Scorer2Str) /* 0 */
PORT_DECL(Ffa, Str2Scorer) /* 1 */
PORT_DECL(Ffa, ClientJoin) /* 11 */
PORT_DECL(Ffa, BuildCfgString) /* 0 */
PORT_DECL(Ffa, ModiTry) /* 0 */
PORT_DECL(Ffa, ModifyOnReyoin) /* 11 */
PORT_DECL(Ffa, ClientModify) /* 11 */
PORT_DECL(Ffa, TryToModify) /* 11 */
PORT_DECL(Ffa, PlayerMsgShow) /* 2 */
PORT_DECL(Ffa, PlayerDisconected) /* 1 */
PORT_DECL(Ffa, PlayerSpectator) /* 1 */
PORT_DECL(Ffa, PlayerJoinGame) /* 1 */
PORT_DECL(Ffa, ClientExit) /* 3 */
PORT_DECL(Ffa, ReSpawnClient) /* 1 */
PORT_DECL(Ffa, ServerRemoveSpectatorState) /* 1 */
PORT_DECL(Ffa, SetSpawnPos) /* 1 */
PORT_DECL(Ffa, InitServer) /* 1 */
PORT_DECL(Ffa, ClientSetConfig) /* 11 */
PORT_DECL(Ffa, ClientAfterCreate) /* 2 */
PORT_DECL(Ffa, ClientCreate) /* 2 */
PORT_DECL(Ffa, OnUsrTest) /* 2 */
PORT_DECL(Ffa, SvrShowMsg) /* 1 */
PORT_DECL(Ffa, OnUsrClient) /* 2 */
PORT_DECL(Ffa, OnUsrServer) /* 2 */
PORT_DECL(Ffa, Spectate) /* ? */
PORT_DECL(Ffa, JoinRed) /* 2 */
PORT_DECL(Ffa, JoinGreen) /* 2 */
PORT_DECL(Ffa, ReJoinGame) /* 2 */
PORT_DECL(Ffa, RejoinMsg) /* 3 */
PORT_DECL(Ffa, JoinGame) /* 2 */
PORT_DECL(Ffa, TeamMenu) /* 2 */
PORT_DECL(Ffa, GetGameTeamDescriptor) /* 0 */
PORT_DECL(Ffa, CameraSpectator) /* 0 */
PORT_DECL(Ffa, AbortSpectateMode) /* 0 */
PORT_DECL(Ffa, ServerSpectate) /* 1 */
PORT_DECL(Ffa, InitClient) /* 0 */
PORT_DECL(Ffa, StdInitFfa) /* 0 */
PORT_DECL(Ffa, GetScorerInfo) /* ? */
PORT_DECL(Ffa, OnPositionChange) /* 6 */
PORT_DECL(Ffa, isTied) /* 1 */
PORT_DECL(Ffa, CheckGameStart) /* 0 */
PORT_DECL(Ffa, UsrPos) /* 1 */
PORT_DECL(Ffa, ActualizeGameScorer) /* ? */
PORT_DECL(Ffa, ShowScorer) /* 2 */
PORT_DECL(Ffa, BackMenuFunc) /* 1 */
PORT_DECL(Ffa, ShowHelp) /* 1 */
PORT_DECL(Ffa, SetupGameRules) /* 1 */
PORT_DECL(Ffa, SendGameRules) /* 1 */
PORT_DECL(Ffa, SetScorerValues) /* 1 */
PORT_DECL(Ffa, FfaScorer) /* 1 */
#define PORTS_Ffa \
	PORT_DEFN(Ffa, InitFfa, Ffa_InitFfa) \
	PORT_DEFN(Ffa, SetupUserShip, Ffa_SetupUserShip) \
	PORT_DEFN(Ffa, RestartOnPress, Ffa_RestartOnPress) \
	PORT_DEFN(Ffa, DelayedRestartOnPress, Ffa_DelayedRestartOnPress) \
	PORT_DEFN(Ffa, OnDeath, Ffa_OnDeath) \
	PORT_DEFN(Ffa, ShowPresFireMessage, Ffa_ShowPresFireMessage) \
	PORT_DEFN(Ffa, DelayedRestartOnTime, Ffa_DelayedRestartOnTime) \
	PORT_DEFN(Ffa, EndOfGame, Ffa_EndOfGame) \
	PORT_DEFN(Ffa, ServerSendPlayerStatus, Ffa_ServerSendPlayerStatus) \
	PORT_DEFN(Ffa, ServerSendTeamData, Ffa_ServerSendTeamData) \
	PORT_DEFN(Ffa, ServerEndOfGame, Ffa_ServerEndOfGame) \
	PORT_DEFN(Ffa, OnDeathServer, Ffa_OnDeathServer) \
	PORT_DEFN(Ffa, OnDeathClient, Ffa_OnDeathClient) \
	PORT_DEFN(Ffa, Scorer2Str, Ffa_Scorer2Str) \
	PORT_DEFN(Ffa, Str2Scorer, Ffa_Str2Scorer) \
	PORT_DEFN(Ffa, ClientJoin, Ffa_ClientJoin) \
	PORT_DEFN(Ffa, BuildCfgString, Ffa_BuildCfgString) \
	PORT_DEFN(Ffa, ModiTry, Ffa_ModiTry) \
	PORT_DEFN(Ffa, ModifyOnReyoin, Ffa_ModifyOnReyoin) \
	PORT_DEFN(Ffa, ClientModify, Ffa_ClientModify) \
	PORT_DEFN(Ffa, TryToModify, Ffa_TryToModify) \
	PORT_DEFN(Ffa, PlayerMsgShow, Ffa_PlayerMsgShow) \
	PORT_DEFN(Ffa, PlayerDisconected, Ffa_PlayerDisconected) \
	PORT_DEFN(Ffa, PlayerSpectator, Ffa_PlayerSpectator) \
	PORT_DEFN(Ffa, PlayerJoinGame, Ffa_PlayerJoinGame) \
	PORT_DEFN(Ffa, ClientExit, Ffa_ClientExit) \
	PORT_DEFN(Ffa, ReSpawnClient, Ffa_ReSpawnClient) \
	PORT_DEFN(Ffa, ServerRemoveSpectatorState, Ffa_ServerRemoveSpectatorState) \
	PORT_DEFN(Ffa, SetSpawnPos, Ffa_SetSpawnPos) \
	PORT_DEFN(Ffa, InitServer, Ffa_InitServer) \
	PORT_DEFN(Ffa, ClientSetConfig, Ffa_ClientSetConfig) \
	PORT_DEFN(Ffa, ClientAfterCreate, Ffa_ClientAfterCreate) \
	PORT_DEFN(Ffa, ClientCreate, Ffa_ClientCreate) \
	PORT_DEFN(Ffa, OnUsrTest, Ffa_OnUsrTest) \
	PORT_DEFN(Ffa, SvrShowMsg, Ffa_SvrShowMsg) \
	PORT_DEFN(Ffa, OnUsrClient, Ffa_OnUsrClient) \
	PORT_DEFN(Ffa, OnUsrServer, Ffa_OnUsrServer) \
	PORT_DEFNK(Ffa, Spectate, Ffa_Spectate) \
	PORT_DEFN(Ffa, JoinRed, Ffa_JoinRed) \
	PORT_DEFN(Ffa, JoinGreen, Ffa_JoinGreen) \
	PORT_DEFN(Ffa, ReJoinGame, Ffa_ReJoinGame) \
	PORT_DEFN(Ffa, RejoinMsg, Ffa_RejoinMsg) \
	PORT_DEFN(Ffa, JoinGame, Ffa_JoinGame) \
	PORT_DEFN(Ffa, TeamMenu, Ffa_TeamMenu) \
	PORT_DEFN(Ffa, GetGameTeamDescriptor, Ffa_GetGameTeamDescriptor) \
	PORT_DEFN(Ffa, CameraSpectator, Ffa_CameraSpectator) \
	PORT_DEFN(Ffa, AbortSpectateMode, Ffa_AbortSpectateMode) \
	PORT_DEFN(Ffa, ServerSpectate, Ffa_ServerSpectate) \
	PORT_DEFN(Ffa, InitClient, Ffa_InitClient) \
	PORT_DEFN(Ffa, StdInitFfa, Ffa_StdInitFfa) \
	PORT_DEFNK(Ffa, GetScorerInfo, Ffa_GetScorerInfo) \
	PORT_DEFN(Ffa, OnPositionChange, Ffa_OnPositionChange) \
	PORT_DEFN(Ffa, isTied, Ffa_isTied) \
	PORT_DEFN(Ffa, CheckGameStart, Ffa_CheckGameStart) \
	PORT_DEFN(Ffa, UsrPos, Ffa_UsrPos) \
	PORT_DEFNK(Ffa, ActualizeGameScorer, Ffa_ActualizeGameScorer) \
	PORT_DEFN(Ffa, ShowScorer, Ffa_ShowScorer) \
	PORT_DEFN(Ffa, BackMenuFunc, Ffa_BackMenuFunc) \
	PORT_DEFN(Ffa, ShowHelp, Ffa_ShowHelp) \
	PORT_DEFN(Ffa, SetupGameRules, Ffa_SetupGameRules) \
	PORT_DEFN(Ffa, SendGameRules, Ffa_SendGameRules) \
	PORT_DEFN(Ffa, SetScorerValues, Ffa_SetScorerValues) \
	PORT_DEFN(Ffa, FfaScorer, Ffa_FfaScorer)

/* Fx.c */
PORT_DECL(Fx, Init) /* ? */
PORT_DECL(Fx, GetMotorClass) /* 1 */
PORT_DECL(Fx, SetUpShipMotors) /* 1 */
PORT_DECL(Fx, MoneyTransfer) /* 3 */
#define PORTS_Fx \
	PORT_DEFN(Fx, Init, Fx_InitPy) \
	PORT_DEFN(Fx, GetMotorClass, Fx_GetMotorClass) \
	PORT_DEFN(Fx, SetUpShipMotors, Fx_SetUpShipMotors) \
	PORT_DEFN(Fx, MoneyTransfer, Fx_MoneyTransferPy)

/* GameSounds.c */
PORT_DECL(Sound, SetStdDamageSounds) /* 1 */
PORT_DECL(Sound, SetStdDamageSoundLoops) /* 1 */
PORT_DECL(Sound, LoadDamageSounds) /* 0 */
PORT_DECL(Sound, LoadMenuSounds) /* 0 */
PORT_DECL(Sound, ScorerSounds) /* 0 */
PORT_DECL(Sound, WayPoints) /* 0 */
PORT_DECL(Sound, PhotoSounds) /* 0 */
PORT_DECL(Sound, CharConversionSounds) /* 0 */
PORT_DECL(Sound, LoadShipEditInterfaceSounds) /* 0 */
PORT_DECL(Sound, Init) /* 0 */
PORT_DECL(Sound, SetOutMusic) /* 0 */
PORT_DECL(Sound, SetPSLoop) /* ? */
PORT_DECL(Sound, SetPSInterval) /* ? */
PORT_DECL(OutSound, SetVehicleSounds) /* 1 */
PORT_DECL(OutSound, LoadEngines) /* 0 */
PORT_DECL(OutSound, LoadWeapons) /* 0 */
PORT_DECL(OutSound, LoadScorer) /* 0 */
PORT_DECL(OutSound, LoadMisc) /* 0 */
PORT_DECL(OutSound, LoadCrash) /* 0 */
PORT_DECL(OutSound, LoadVehicles) /* 0 */
PORT_DECL(OutSound, Init) /* 0 */
PORT_DECL(DoorSound, LoadSounds) /* 0 */
PORT_DECL(DoorSound, LoadSndList) /* 1 */
PORT_DECL(DoorSound, SetSlide) /* 1 */
PORT_DECL(DoorSound, SetInitDoorSound) /* ? */
PORT_DECL(DoorSound, ConversorSetSounds) /* 1 */
#define PORTS_GameSounds \
	PORT_DEFN(Sound, SetStdDamageSounds, Sound_SetStdDamageSounds) \
	PORT_DEFN(Sound, SetStdDamageSoundLoops, Sound_SetStdDamageSoundLoops) \
	PORT_DEFN(Sound, LoadDamageSounds, Sound_LoadDamageSounds) \
	PORT_DEFN(Sound, LoadMenuSounds, Sound_LoadMenuSounds) \
	PORT_DEFN(Sound, ScorerSounds, Sound_ScorerSounds) \
	PORT_DEFN(Sound, WayPoints, Sound_WayPoints) \
	PORT_DEFN(Sound, PhotoSounds, Sound_PhotoSounds) \
	PORT_DEFN(Sound, CharConversionSounds, Sound_CharConversionSounds) \
	PORT_DEFN(Sound, LoadShipEditInterfaceSounds, Sound_LoadShipEditInterfaceSounds) \
	PORT_DEFN(Sound, Init, Sound_Init) \
	PORT_DEFN(Sound, SetOutMusic, Sound_SetOutMusic) \
	PORT_DEFNK(Sound, SetPSLoop, Sound_SetPSLoop) \
	PORT_DEFNK(Sound, SetPSInterval, Sound_SetPSInterval) \
	PORT_DEFN(OutSound, SetVehicleSounds, OutSound_SetVehicleSounds) \
	PORT_DEFN(OutSound, LoadEngines, OutSound_LoadEngines) \
	PORT_DEFN(OutSound, LoadWeapons, OutSound_LoadWeapons) \
	PORT_DEFN(OutSound, LoadScorer, OutSound_LoadScorer) \
	PORT_DEFN(OutSound, LoadMisc, OutSound_LoadMisc) \
	PORT_DEFN(OutSound, LoadCrash, OutSound_LoadCrash) \
	PORT_DEFN(OutSound, LoadVehicles, OutSound_LoadVehicles) \
	PORT_DEFN(OutSound, Init, OutSound_Init) \
	PORT_DEFN(DoorSound, LoadSounds, DoorSound_LoadSounds) \
	PORT_DEFN(DoorSound, LoadSndList, DoorSound_LoadSndList) \
	PORT_DEFN(DoorSound, SetSlide, DoorSound_SetSlide) \
	PORT_DEFNK(DoorSound, SetInitDoorSound, DoorSound_SetInitDoorSound) \
	PORT_DEFN(DoorSound, ConversorSetSounds, DoorSound_ConversorSetSounds)

/* GateKeeper.c */
PORT_DECL(GateKeeper, UseDoor) /* 4 */
PORT_DECL(GateKeeper, OnCreate) /* 1 */
PORT_DECL(GateKeeper, DoNothingFunc) /* ? */
PORT_DECL(GateKeeper, SetDoor) /* ? */
PORT_DECL(GateKeeper, DoorFoe) /* 2 */
PORT_DECL(GateKeeper, GetGateKeeperId) /* 1 */
PORT_DECL(GateKeeper, RestoreState) /* 1 */
PORT_DECL(GateKeeper, EndOfSpeech) /* 0 */
PORT_DECL(GateKeeper, GateKeeperSpeech) /* 4 */
#define PORTS_GateKeeper \
	PORT_DEFN(GateKeeper, UseDoor, GateKeeper_UseDoor) \
	PORT_DEFN(GateKeeper, OnCreate, GateKeeper_OnCreate) \
	PORT_DEFN(GateKeeper, DoNothingFunc, Mode_PyNone2) \
	PORT_DEFNK(GateKeeper, SetDoor, GateKeeper_SetDoor) \
	PORT_DEFN(GateKeeper, DoorFoe, GateKeeper_DoorFoe) \
	PORT_DEFN(GateKeeper, GetGateKeeperId, GateKeeper_GetGateKeeperId) \
	PORT_DEFN(GateKeeper, RestoreState, GateKeeper_RestoreState) \
	PORT_DEFN(GateKeeper, EndOfSpeech, GateKeeper_EndOfSpeech) \
	PORT_DEFN(GateKeeper, GateKeeperSpeech, GateKeeper_GateKeeperSpeech)

/* HangarTab.c */
PORT_DECL(HangarTab, ShowBoostPrediction) /* 2 */
PORT_DECL(HangarTab, DisableChangePlatformButtons) /* 1 */
PORT_DECL(HangarTab, EnableChangePlatformButtons) /* 1 */
PORT_DECL(HangarTab, PrevPilot) /* 2 */
PORT_DECL(HangarTab, NextPilot) /* 2 */
PORT_DECL(HangarTab, DrawBackground) /* ? */
PORT_DECL(HangarTab, SetSellVisible) /* 2 */
PORT_DECL(HangarTab, SetSellActive) /* 2 */
PORT_DECL(HangarTab, SetExitVisible) /* 2 */
PORT_DECL(HangarTab, SetShipTypeVisible) /* 2 */
PORT_DECL(HangarTab, SetPilotVisible) /* 2 */
PORT_DECL(HangarTab, HideHangarPanel) /* 1 */
PORT_DECL(HangarTab, UpdateHangars) /* 1 */
PORT_DECL(HangarTab, ShowHangarPanel) /* 1 */
PORT_DECL(HangarTab, IsShipValid) /* 1 */
PORT_DECL(HangarTab, ComputeShipTotalCost) /* ? */
PORT_DECL(HangarTab, ShowRebuildShipMoney) /* 3 */
PORT_DECL(HangarTab, HideRebuildShipMoney) /* 3 */
PORT_DECL(HangarTab, AuxShowMoney) /* ? */
PORT_DECL(HangarTab, ShowMoneyBuyingShip) /* 3 */
PORT_DECL(HangarTab, ShowMoneySellingShip) /* 3 */
PORT_DECL(HangarTab, HideMoney) /* 3 */
PORT_DECL(HangarTab, ScaleSpeedRamp) /* ? */
PORT_DECL(HangarTab, GetSpeedRampOffsets) /* 1 */
PORT_DECL(HangarTab, GetMaxSpeedTime) /* 1 */
PORT_DECL(HangarTab, SetHangarPanel) /* 1 */
PORT_DECL(HangarTab, CmSec2Value) /* 1 */
PORT_DECL(HangarTab, ShowShipInfo) /* ? */
PORT_DECL(HangarTab, HideShipInfo) /* 1 */
PORT_DECL(HangarTab, PrevShipToEdit) /* 2 */
PORT_DECL(HangarTab, NextShipToEdit) /* 2 */
PORT_DECL(HangarTab, SetShipTypeButtonsState) /* 2 */
PORT_DECL(HangarTab, SelectShipToEdit) /* 1 */
PORT_DECL(HangarTab, SelectHangar) /* ? */
PORT_DECL(HangarTab, SelectItem) /* 2 */
PORT_DECL(HangarTab, CreateEmptyShip) /* 1 */
PORT_DECL(HangarTab, BuyShip) /* 2 */
PORT_DECL(HangarTab, SellShip) /* 2 */
PORT_DECL(HangarTab, AddReferenceShipControls) /* 1 */
PORT_DECL(HangarTab, ShowShipMiniInfo) /* 3 */
PORT_DECL(HangarTab, HideShipMiniInfo) /* 3 */
#define PORTS_HangarTab \
	PORT_DEFN(HangarTab, ShowBoostPrediction, HT_ShowBoostPrediction) \
	PORT_DEFN(HangarTab, DisableChangePlatformButtons, HT_DisableChangePlatformButtons) \
	PORT_DEFN(HangarTab, EnableChangePlatformButtons, HT_EnableChangePlatformButtons) \
	PORT_DEFN(HangarTab, PrevPilot, HT_PrevPilot) \
	PORT_DEFN(HangarTab, NextPilot, HT_NextPilot) \
	PORT_DEFNK(HangarTab, DrawBackground, HangarTab_DrawBackground) \
	PORT_DEFN(HangarTab, SetSellVisible, HT_SetSellVisible) \
	PORT_DEFN(HangarTab, SetSellActive, HT_SetSellActive) \
	PORT_DEFN(HangarTab, SetExitVisible, HT_SetExitVisible) \
	PORT_DEFN(HangarTab, SetShipTypeVisible, HT_SetShipTypeVisible) \
	PORT_DEFN(HangarTab, SetPilotVisible, HT_SetPilotVisible) \
	PORT_DEFN(HangarTab, HideHangarPanel, HT_HideHangarPanel) \
	PORT_DEFN(HangarTab, UpdateHangars, HT_UpdateHangars) \
	PORT_DEFN(HangarTab, ShowHangarPanel, HT_ShowHangarPanel) \
	PORT_DEFN(HangarTab, IsShipValid, HT_IsShipValid) \
	PORT_DEFNK(HangarTab, ComputeShipTotalCost, HT_ComputeShipTotalCost) \
	PORT_DEFN(HangarTab, ShowRebuildShipMoney, HT_ShowRebuildShipMoney) \
	PORT_DEFN(HangarTab, HideRebuildShipMoney, HT_HideRebuildShipMoney) \
	PORT_DEFN(HangarTab, AuxShowMoney, HT_AuxShowMoney) \
	PORT_DEFN(HangarTab, ShowMoneyBuyingShip, HT_ShowMoneyBuyingShip) \
	PORT_DEFN(HangarTab, ShowMoneySellingShip, HT_ShowMoneySellingShip) \
	PORT_DEFN(HangarTab, HideMoney, HT_HideMoney) \
	PORT_DEFNK(HangarTab, ScaleSpeedRamp, HT_ScaleSpeedRamp) \
	PORT_DEFN(HangarTab, GetSpeedRampOffsets, HT_GetSpeedRampOffsets) \
	PORT_DEFN(HangarTab, GetMaxSpeedTime, HT_GetMaxSpeedTime) \
	PORT_DEFN(HangarTab, SetHangarPanel, HT_SetHangarPanel) \
	PORT_DEFN(HangarTab, CmSec2Value, HT_CmSec2Value) \
	PORT_DEFNK(HangarTab, ShowShipInfo, HT_ShowShipInfo) \
	PORT_DEFN(HangarTab, HideShipInfo, HT_HideShipInfo) \
	PORT_DEFN(HangarTab, PrevShipToEdit, HT_PrevShipToEdit) \
	PORT_DEFN(HangarTab, NextShipToEdit, HT_NextShipToEdit) \
	PORT_DEFN(HangarTab, SetShipTypeButtonsState, HT_SetShipTypeButtonsState) \
	PORT_DEFN(HangarTab, SelectShipToEdit, HT_SelectShipToEdit) \
	PORT_DEFNK(HangarTab, SelectHangar, HT_SelectHangar) \
	PORT_DEFN(HangarTab, SelectItem, HT_SelectItem) \
	PORT_DEFN(HangarTab, CreateEmptyShip, HT_CreateEmptyShip) \
	PORT_DEFN(HangarTab, BuyShip, HT_BuyShip) \
	PORT_DEFN(HangarTab, SellShip, HT_SellShip) \
	PORT_DEFN(HangarTab, AddReferenceShipControls, HT_AddReferenceShipControls) \
	PORT_DEFN(HangarTab, ShowShipMiniInfo, HT_ShowShipMiniInfo) \
	PORT_DEFN(HangarTab, HideShipMiniInfo, HT_HideShipMiniInfo)

/* HullTab.c */
PORT_DECL(HullTab, ShowHullPredictionUp) /* 3 */
PORT_DECL(HullTab, HideHullPredictionUp) /* 3 */
PORT_DECL(HullTab, ShowHullPredictionDown) /* 3 */
PORT_DECL(HullTab, HideHullPredictionDown) /* 3 */
PORT_DECL(HullTab, TestArrows) /* ? */
PORT_DECL(HullTab, IncrementHull) /* 2 */
PORT_DECL(HullTab, DecrementHull) /* 2 */
PORT_DECL(HullTab, ChangeHull) /* 2 */
PORT_DECL(HullTab, SetHullState) /* 1 */
PORT_DECL(HullTab, HullSliderChange) /* 2 */
PORT_DECL(HullTab, SetHullPanel) /* 1 */
PORT_DECL(HullTab, ShowHullPanel) /* 1 */
PORT_DECL(HullTab, SetHullVisible) /* 2 */
PORT_DECL(HullTab, HideHullPanel) /* 1 */
#define PORTS_HullTab \
	PORT_DEFN(HullTab, ShowHullPredictionUp, HT_ShowHullPredictionUp) \
	PORT_DEFN(HullTab, HideHullPredictionUp, HT_HideHullPredictionUp) \
	PORT_DEFN(HullTab, ShowHullPredictionDown, HT_ShowHullPredictionDown) \
	PORT_DEFN(HullTab, HideHullPredictionDown, HT_HideHullPredictionDown) \
	PORT_DEFNK(HullTab, TestArrows, HT_TestArrows) \
	PORT_DEFN(HullTab, IncrementHull, HT_IncrementHull) \
	PORT_DEFN(HullTab, DecrementHull, HT_DecrementHull) \
	PORT_DEFN(HullTab, ChangeHull, HT_ChangeHullPy) \
	PORT_DEFN(HullTab, SetHullState, HT_SetHullStatePy) \
	PORT_DEFN(HullTab, HullSliderChange, HT_HullSliderChange) \
	PORT_DEFN(HullTab, SetHullPanel, HT_SetHullPanel) \
	PORT_DEFN(HullTab, ShowHullPanel, HT_ShowHullPanel) \
	PORT_DEFN(HullTab, SetHullVisible, HT_SetHullVisible) \
	PORT_DEFN(HullTab, HideHullPanel, HT_HideHullPanel)

/* InMap.c */
PORT_DECL(InMap, Init) /* 1 */
#define PORTS_InMap \
	PORT_DEFN(InMap, Init, InMap_InitPy)

/* InitLevel.c */
PORT_DECL(Init, LoadCharLib) /* 1 */
PORT_DECL(Init, AddMLib) /* 1 */
PORT_DECL(Init, MLibInitStatus) /* 1 */
PORT_DECL(Init, MLibInit) /* 1 */
PORT_DECL(Init, isMission) /* ? */
PORT_DECL(Init, PreloadMlibs) /* 1 */
PORT_DECL(Init, SetArrowSize) /* ? */
PORT_DECL(Init, Init) /* 1 */
PORT_DECL(Init, SetMapLabelText) /* ? */
PORT_DECL(Init, Close) /* ? */
#define PORTS_InitLevel \
	PORT_DEFN(Init, LoadCharLib, Init_LoadCharLib) \
	PORT_DEFN(Init, AddMLib, Init_AddMLib) \
	PORT_DEFN(Init, MLibInitStatus, Init_MLibInitStatus) \
	PORT_DEFN(Init, MLibInit, Init_MLibInit) \
	PORT_DEFN(Init, isMission, Init_isMission) \
	PORT_DEFN(Init, PreloadMlibs, Init_PreloadMlibs) \
	PORT_DEFN(Init, SetArrowSize, Init_SetArrowSize) \
	PORT_DEFN(Init, Init, Init_Init) \
	PORT_DEFN(Init, SetMapLabelText, Init_SetMapLabelText) \
	PORT_DEFN(Init, Close, Init_Close)

/* Items.c */
PORT_DECL(Items, ResetUpgrade) /* 1 */
PORT_DECL(Items, RestoreUpgrade) /* 1 */
PORT_DECL(Items, DropAll) /* 1 */
PORT_DECL(Items, DropUpgrade) /* 3 */
PORT_DECL(Items, BaseOnTakeUpgrade) /* 4 */
PORT_DECL(Items, OnTakeiXtr) /* 2 */
PORT_DECL(Items, OnTakeiSiz) /* 2 */
PORT_DECL(Items, OnTakeaBom) /* 2 */
PORT_DECL(Items, OnTakeaSpd) /* 2 */
PORT_DECL(Items, OnTakedAim) /* 2 */
PORT_DECL(Items, OnTakedBom) /* 2 */
PORT_DECL(Items, OnTaketAim) /* 2 */
PORT_DECL(Items, OnTaketTra) /* 2 */
PORT_DECL(Items, OnTakesRot) /* 2 */
PORT_DECL(Items, OnTakesRck) /* 2 */
PORT_DECL(Items, OnTakevAim) /* 2 */
PORT_DECL(Items, OnTakevSpd) /* 2 */
PORT_DECL(Items, SetSound) /* 2 */
PORT_DECL(Items, ClientCreate) /* 2 */
PORT_DECL(Items, CreateBox) /* 5 */
PORT_DECL(Items, BrowseItemPos) /* ? */
PORT_DECL(Items, InitResources) /* ? */
PORT_DECL(Items, Init) /* ? */
PORT_DECL(Items, ShowHint) /* 2 */
PORT_DECL(Items, Hint) /* ? */
PORT_DECL(Items, CreateInteractiveObj) /* 2 */
PORT_DECL(Items, SetInteractiveObjOnFloor) /* 1 */
#define PORTS_Items \
	PORT_DEFN(Items, ResetUpgrade, Items_ResetUpgrade) \
	PORT_DEFN(Items, RestoreUpgrade, Items_RestoreUpgrade) \
	PORT_DEFN(Items, DropAll, Items_DropAll) \
	PORT_DEFN(Items, DropUpgrade, Items_DropUpgrade) \
	PORT_DEFN(Items, BaseOnTakeUpgrade, Items_BaseOnTakeUpgrade) \
	PORT_DEFN(Items, OnTakeiXtr, Items_OnTakeiXtr) \
	PORT_DEFN(Items, OnTakeiSiz, Items_OnTakeiSiz) \
	PORT_DEFN(Items, OnTakeaBom, Items_OnTakeaBom) \
	PORT_DEFN(Items, OnTakeaSpd, Items_OnTakeaSpd) \
	PORT_DEFN(Items, OnTakedAim, Items_OnTakedAim) \
	PORT_DEFN(Items, OnTakedBom, Items_OnTakedBom) \
	PORT_DEFN(Items, OnTaketAim, Items_OnTaketAim) \
	PORT_DEFN(Items, OnTaketTra, Items_OnTaketTra) \
	PORT_DEFN(Items, OnTakesRot, Items_OnTakesRot) \
	PORT_DEFN(Items, OnTakesRck, Items_OnTakesRck) \
	PORT_DEFN(Items, OnTakevAim, Items_OnTakevAim) \
	PORT_DEFN(Items, OnTakevSpd, Items_OnTakevSpd) \
	PORT_DEFN(Items, SetSound, Items_SetSound) \
	PORT_DEFN(Items, ClientCreate, Items_ClientCreate) \
	PORT_DEFN(Items, CreateBox, Items_CreateBox) \
	PORT_DEFN(Items, BrowseItemPos, Items_BrowseItemPos) \
	PORT_DEFN(Items, InitResources, Items_InitResources) \
	PORT_DEFN(Items, Init, Items_Init) \
	PORT_DEFN(Items, ShowHint, Items_ShowHint) \
	PORT_DEFN(Items, Hint, Items_Hint) \
	PORT_DEFN(Items, CreateInteractiveObj, Items_CreateInteractiveObj) \
	PORT_DEFN(Items, SetInteractiveObjOnFloor, Items_SetInteractiveObjOnFloor)

/* MakeChar.c */
PORT_DECL(MakeChar, CreateRelaxAction) /* ? */
PORT_DECL(MakeChar, AddRelaxActions) /* ? */
PORT_DECL(MakeChar, AddDiscover) /* ? */
PORT_DECL(MakeChar, AddMove) /* ? */
PORT_DECL(MakeChar, AddWalkActions) /* ? */
PORT_DECL(MakeChar, AddInitWalkActions) /* ? */
PORT_DECL(MakeChar, AddEndWalkActions) /* ? */
PORT_DECL(MakeChar, AddFallAction) /* ? */
PORT_DECL(MakeChar, AddTalkAction) /* ? */
PORT_DECL(MakeChar, AddTalkMorph) /* 1 */
PORT_DECL(MakeChar, AddMorphAction) /* ? */
PORT_DECL(MakeChar, AddDefMorphActions) /* ? */
PORT_DECL(MakeChar, EventAnimCommand) /* ? */
PORT_DECL(MakeChar, AddTalkAct) /* ? */
PORT_DECL(MakeChar, AddFlashedAction) /* ? */
PORT_DECL(MakeChar, AddCrazyAction) /* ? */
PORT_DECL(MakeChar, AddDazeAction) /* ? */
PORT_DECL(MakeChar, ListenAction) /* ? */
PORT_DECL(MakeChar, AddBreakLoopAction) /* ? */
PORT_DECL(MakeChar, SetSitActions) /* ? */
PORT_DECL(MakeChar, InitChar) /* ? */
PORT_DECL(MakeChar, SetNodeRotation) /* 4 */
PORT_DECL(MakeChar, TurretBasicActions) /* ? */
PORT_DECL(MakeChar, AddTurretTalkAction) /* ? */
PORT_DECL(MakeChar, AddTurretTalkAct) /* ? */
#define PORTS_MakeChar \
	PORT_DEFNK(MakeChar, CreateRelaxAction, MakeChar_CreateRelaxAction) \
	PORT_DEFNK(MakeChar, AddRelaxActions, MakeChar_AddRelaxActions) \
	PORT_DEFN(MakeChar, AddDiscover, MakeChar_AddDiscover) \
	PORT_DEFN(MakeChar, AddMove, MakeChar_AddMove) \
	PORT_DEFNK(MakeChar, AddWalkActions, MakeChar_AddWalkActions) \
	PORT_DEFNK(MakeChar, AddInitWalkActions, MakeChar_AddInitWalkActions) \
	PORT_DEFNK(MakeChar, AddEndWalkActions, MakeChar_AddEndWalkActions) \
	PORT_DEFNK(MakeChar, AddFallAction, MakeChar_AddFallAction) \
	PORT_DEFNK(MakeChar, AddTalkAction, MakeChar_AddTalkAction) \
	PORT_DEFN(MakeChar, AddTalkMorph, MakeChar_AddTalkMorph) \
	PORT_DEFN(MakeChar, AddMorphAction, MakeChar_AddMorphAction) \
	PORT_DEFN(MakeChar, AddDefMorphActions, MakeChar_AddDefMorphActions) \
	PORT_DEFNK(MakeChar, EventAnimCommand, MakeChar_EventAnimCommand) \
	PORT_DEFNK(MakeChar, AddTalkAct, MakeChar_AddTalkAct) \
	PORT_DEFNK(MakeChar, AddFlashedAction, MakeChar_AddFlashedAction) \
	PORT_DEFN(MakeChar, AddCrazyAction, MakeChar_AddCrazyAction) \
	PORT_DEFNK(MakeChar, AddDazeAction, MakeChar_AddDazeAction) \
	PORT_DEFNK(MakeChar, ListenAction, MakeChar_ListenAction) \
	PORT_DEFN(MakeChar, AddBreakLoopAction, MakeChar_AddBreakLoopAction) \
	PORT_DEFN(MakeChar, SetSitActions, MakeChar_SetSitActions) \
	PORT_DEFN(MakeChar, InitChar, MakeChar_InitChar) \
	PORT_DEFN(MakeChar, SetNodeRotation, MakeChar_SetNodeRotation) \
	PORT_DEFNK(MakeChar, TurretBasicActions, MakeChar_TurretBasicActions) \
	PORT_DEFNK(MakeChar, AddTurretTalkAction, MakeChar_AddTurretTalkAction) \
	PORT_DEFNK(MakeChar, AddTurretTalkAct, MakeChar_AddTurretTalkAct)

/* MenuBase.c */
PORT_DECL(Menu, LinkLR) /* ? */
PORT_DECL(Menu, LinkUD) /* ? */
PORT_DECL(Menu, UnLinkLR) /* ? */
PORT_DECL(Menu, UnLinkUD) /* ? */
PORT_DECL(Menu, DrawBackMainMenu) /* ? */
PORT_DECL(Menu, DrawBackSubMenu) /* ? */
PORT_DECL(Menu, DrawBackOptionMenu) /* ? */
PORT_DECL(Menu, DrawCircuitMenu) /* ? */
PORT_DECL(Menu, DrawMenuTitleBar) /* ? */
PORT_DECL(Menu, StartNewMenu) /* ? */
PORT_DECL(Menu, AddTabs) /* ? */
PORT_DECL(Menu, DummyFunc) /* ? */
PORT_DECL(Menu, VerticalMenu) /* ? */
PORT_DECL(Menu, YesNoMenu) /* ? */
PORT_DECL(Menu, DisableMenuItem) /* ? */
PORT_DECL(Menu, ShowTextBox) /* ? */
PORT_DECL(Menu, ShowWarning) /* ? */
PORT_DECL(Menu, SetTabDefaults) /* ? */
PORT_DECL(Menu, SliderMenu) /* ? */
PORT_DECL(Menu, PreviousNextMenu) /* ? */
PORT_DECL(Menu, PreviousNextMenuSimple) /* ? */
#define PORTS_MenuBase \
	PORT_DEFN(Menu, LinkLR, Menu_LinkLR) \
	PORT_DEFN(Menu, LinkUD, Menu_LinkUD) \
	PORT_DEFN(Menu, UnLinkLR, Menu_UnLinkLR) \
	PORT_DEFN(Menu, UnLinkUD, Menu_UnLinkUD) \
	PORT_DEFN(Menu, DrawBackMainMenu, Menu_DrawBackMainMenu) \
	PORT_DEFN(Menu, DrawBackSubMenu, Menu_DrawBackSubMenu) \
	PORT_DEFN(Menu, DrawBackOptionMenu, Menu_DrawBackOptionMenu) \
	PORT_DEFN(Menu, DrawCircuitMenu, Menu_DrawCircuitMenu) \
	PORT_DEFN(Menu, DrawMenuTitleBar, Menu_DrawMenuTitleBar) \
	PORT_DEFN(Menu, StartNewMenu, Menu_StartNewMenu) \
	PORT_DEFNK(Menu, AddTabs, Menu_AddTabs) \
	PORT_DEFN(Menu, DummyFunc, Mode_PyNone) \
	PORT_DEFNK(Menu, VerticalMenu, Menu_VerticalMenu) \
	PORT_DEFN(Menu, YesNoMenu, Menu_YesNoMenu) \
	PORT_DEFN(Menu, DisableMenuItem, Menu_DisableMenuItem) \
	PORT_DEFN(Menu, ShowTextBox, Menu_ShowTextBox) \
	PORT_DEFN(Menu, ShowWarning, Menu_ShowWarning) \
	PORT_DEFN(Menu, SetTabDefaults, Menu_SetTabDefaults) \
	PORT_DEFN(Menu, SliderMenu, Menu_SliderMenu) \
	PORT_DEFN(Menu, PreviousNextMenu, Menu_PreviousNextMenu) \
	PORT_DEFN(Menu, PreviousNextMenuSimple, Menu_PreviousNextMenuSimple)

/* MenuControls.c */
PORT_DECL(PCMenu, OptionsMenu) /* ? */
PORT_DECL(PCMenu, ControlsMenu) /* ? */
PORT_DECL(PCMenu, MouseControlsMenu) /* ? */
PORT_DECL(PCMenu, RacerMoveControlsMenu) /* ? */
PORT_DECL(PCMenu, RacerAttackControlsMenu) /* ? */
PORT_DECL(PCMenu, WalkCharControlsMenu) /* ? */
PORT_DECL(PCMenu, UseCharControlsMenu) /* ? */
PORT_DECL(PCMenu, CreateControlMenu) /* ? */
PORT_DECL(PCMenu, RefreshControlMenu) /* ? */
PORT_DECL(PCMenu, ClearKeyEntry) /* ? */
PORT_DECL(PCMenu, WaitForKeyPress) /* ? */
PORT_DECL(PCMenu, AbortWaitRedefine) /* ? */
PORT_DECL(PCMenu, KetRedefineFunc) /* ? */
PORT_DECL(PCMenu, ResetEverything) /* ? */
PORT_DECL(PCMenu, ResetToDefault) /* ? */
PORT_DECL(PCMenu, OptionYesNo) /* ? */
PORT_DECL(PCMenu, InvertMouse) /* ? */
PORT_DECL(PCMenu, AlwaysRun) /* ? */
PORT_DECL(PCMenu, MouseSensitivityChange) /* ? */
PORT_DECL(PCMenu, MouseSensitivitySliderFocus_Sub) /* ? */
PORT_DECL(PCMenu, MouseSensitivitySliderFocus_Add) /* ? */
PORT_DECL(PCMenu, ControllerConfig) /* ? */
PORT_DECL(PCMenu, ControllerConfigCharMenu) /* ? */
PORT_DECL(PCMenu, ControllerConfigShipMenu) /* ? */
PORT_DECL(PCMenu, FillControllerMenu) /* ? */
PORT_DECL(PCMenu, PlaceMenuOption) /* ? */
PORT_DECL(PCMenu, SetOptionValue) /* ? */
PORT_DECL(PCMenu, ShowPadConfig) /* ? */
PORT_DECL(PCMenu, UpdatePadText) /* ? */
PORT_DECL(PCMenu, SwitchOption) /* ? */
#define PORTS_MenuControls \
	PORT_DEFN(PCMenu, OptionsMenu, PCMenu_OptionsMenu) \
	PORT_DEFN(PCMenu, ControlsMenu, PCMenu_ControlsMenu) \
	PORT_DEFN(PCMenu, MouseControlsMenu, PCMenu_MouseControlsMenu) \
	PORT_DEFN(PCMenu, RacerMoveControlsMenu, PCMenu_RacerMoveControlsMenu) \
	PORT_DEFN(PCMenu, RacerAttackControlsMenu, PCMenu_RacerAttackControlsMenu) \
	PORT_DEFN(PCMenu, WalkCharControlsMenu, PCMenu_WalkCharControlsMenu) \
	PORT_DEFN(PCMenu, UseCharControlsMenu, PCMenu_UseCharControlsMenu) \
	PORT_DEFN(PCMenu, CreateControlMenu, PCMenu_CreateControlMenu) \
	PORT_DEFN(PCMenu, RefreshControlMenu, PCMenu_RefreshControlMenu) \
	PORT_DEFN(PCMenu, ClearKeyEntry, PCMenu_ClearKeyEntry) \
	PORT_DEFN(PCMenu, WaitForKeyPress, PCMenu_WaitForKeyPress) \
	PORT_DEFN(PCMenu, AbortWaitRedefine, PCMenu_AbortWaitRedefine) \
	PORT_DEFN(PCMenu, KetRedefineFunc, PCMenu_AbortWaitRedefine) \
	PORT_DEFN(PCMenu, ResetEverything, PCMenu_ResetEverything) \
	PORT_DEFN(PCMenu, ResetToDefault, PCMenu_ResetToDefault) \
	PORT_DEFN(PCMenu, OptionYesNo, PCMenu_OptionYesNo) \
	PORT_DEFN(PCMenu, InvertMouse, PCMenu_InvertMouse) \
	PORT_DEFN(PCMenu, AlwaysRun, PCMenu_AlwaysRun) \
	PORT_DEFN(PCMenu, MouseSensitivityChange, PCMenu_MouseSensitivityChange) \
	PORT_DEFN(PCMenu, MouseSensitivitySliderFocus_Sub, PCMenu_MouseSensitivitySliderFocus_Sub) \
	PORT_DEFN(PCMenu, MouseSensitivitySliderFocus_Add, PCMenu_MouseSensitivitySliderFocus_Add) \
	PORT_DEFN(PCMenu, ControllerConfig, PCMenu_ControllerConfig) \
	PORT_DEFN(PCMenu, ControllerConfigCharMenu, PCMenu_ControllerConfigCharMenu) \
	PORT_DEFN(PCMenu, ControllerConfigShipMenu, PCMenu_ControllerConfigShipMenu) \
	PORT_DEFN(PCMenu, FillControllerMenu, PCMenu_FillControllerMenu) \
	PORT_DEFN(PCMenu, PlaceMenuOption, PCMenu_PlaceMenuOption) \
	PORT_DEFN(PCMenu, SetOptionValue, PCMenu_SetOptionValue) \
	PORT_DEFNK(PCMenu, ShowPadConfig, PCMenu_ShowPadConfig) \
	PORT_DEFN(PCMenu, UpdatePadText, PCMenu_UpdatePadText) \
	PORT_DEFN(PCMenu, SwitchOption, PCMenu_SwitchOption)

/* MenuHud.c */
PORT_DECL(Scorer, CreateStatusScorer) /* ? */
PORT_DECL(Scorer, CreateCopMeterScorer) /* 1 */
PORT_DECL(Scorer, CreateMonitorScorer) /* ? */
PORT_DECL(Scorer, CreateMsgScorer) /* 1 */
PORT_DECL(Scorer, CreateMissionScorer) /* ? */
PORT_DECL(Scorer, ShowMissionScorer) /* ? */
PORT_DECL(Scorer, CreateRadarScorer) /* ? */
PORT_DECL(Scorer, Blink) /* ? */
PORT_DECL(Scorer, MissionScorerBar_SetDefaults) /* ? */
PORT_DECL(CharScorer, Init) /* ? */
PORT_DECL(CharScorer, CreateRadarScorer) /* 1 */
PORT_DECL(CharScorer, CreateSpecialActionScorer) /* 1 */
PORT_DECL(CharScorer, Create) /* 1 */
PORT_DECL(RacerScorer, Init) /* ? */
PORT_DECL(RacerScorer, CreateWeaponScorer) /* 1 */
PORT_DECL(RacerScorer, CreateCannonScorer) /* 1 */
PORT_DECL(RacerScorer, CreateTargetScorer) /* 1 */
PORT_DECL(RacerScorer, CreateWaypointScorer) /* 1 */
PORT_DECL(RacerScorer, CreateRadarScorer) /* 1 */
PORT_DECL(RacerScorer, Create) /* 1 */
PORT_DECL(RacerScorer, CreateRacer) /* 1 */
PORT_DECL(RacerScorer, ShowScorer) /* ? */
PORT_DECL(RacerScorer, MultiplayerUpdatePing) /* ? */
PORT_DECL(RacerScorer, InitMissionScorer) /* ? */
PORT_DECL(RacerScorer, MissionScorer) /* 1 */
PORT_DECL(RacerScorer, MissionScorerTeam) /* 1 */
PORT_DECL(RacerScorer, UpdateMissionScorer) /* 3 */
#define PORTS_MenuHud \
	PORT_DEFN(Scorer, CreateStatusScorer, Scorer_CreateStatusScorer) \
	PORT_DEFN(Scorer, CreateCopMeterScorer, Scorer_CreateCopMeterScorer) \
	PORT_DEFN(Scorer, CreateMonitorScorer, Scorer_CreateMonitorScorer) \
	PORT_DEFN(Scorer, CreateMsgScorer, Scorer_CreateMsgScorer) \
	PORT_DEFNK(Scorer, CreateMissionScorer, Scorer_CreateMissionScorer) \
	PORT_DEFN(Scorer, ShowMissionScorer, Scorer_ShowMissionScorer) \
	PORT_DEFN(Scorer, CreateRadarScorer, Scorer_CreateRadarScorer) \
	PORT_DEFN(Scorer, Blink, Scorer_Blink) \
	PORT_DEFN(Scorer, MissionScorerBar_SetDefaults, Scorer_MissionScorerBar_SetDefaults) \
	PORT_DEFN(CharScorer, Init, Mode_PyNone) \
	PORT_DEFN(CharScorer, CreateRadarScorer, CharScorer_CreateRadarScorer) \
	PORT_DEFN(CharScorer, CreateSpecialActionScorer, CharScorer_CreateSpecialActionScorer) \
	PORT_DEFN(CharScorer, Create, CharScorer_Create) \
	PORT_DEFN(RacerScorer, Init, Mode_PyNone) \
	PORT_DEFN(RacerScorer, CreateWeaponScorer, Racer_CreateWeaponScorer) \
	PORT_DEFN(RacerScorer, CreateCannonScorer, Racer_CreateCannonScorer) \
	PORT_DEFN(RacerScorer, CreateTargetScorer, Racer_CreateTargetScorer) \
	PORT_DEFN(RacerScorer, CreateWaypointScorer, Racer_CreateWaypointScorer) \
	PORT_DEFN(RacerScorer, CreateRadarScorer, Racer_CreateRadarScorer) \
	PORT_DEFN(RacerScorer, Create, Racer_Create) \
	PORT_DEFN(RacerScorer, CreateRacer, Racer_CreateRacer) \
	PORT_DEFN(RacerScorer, ShowScorer, Racer_ShowScorer) \
	PORT_DEFN(RacerScorer, MultiplayerUpdatePing, Racer_MultiplayerUpdatePing) \
	PORT_DEFN(RacerScorer, InitMissionScorer, Racer_InitMissionScorer) \
	PORT_DEFN(RacerScorer, MissionScorer, Racer_MissionScorer) \
	PORT_DEFN(RacerScorer, MissionScorerTeam, Racer_MissionScorerTeam) \
	PORT_DEFN(RacerScorer, UpdateMissionScorer, Racer_UpdateMissionScorer)

/* MenuMain.c */
PORT_DECL(Menu, SinglePlayerMenu) /* ? */
PORT_DECL(Menu, AudioMenu) /* ? */
PORT_DECL(Menu, AudioVolumeSliderFocus_Sub) /* ? */
PORT_DECL(Menu, AudioVolumeSliderFocus_Add) /* ? */
PORT_DECL(Menu, AudioVolumeChange) /* ? */
PORT_DECL(Menu, MainMenu) /* ? */
PORT_DECL(Menu, CreateMainMenu) /* ? */
PORT_DECL(Menu, PlaySpacelords) /* ? */
PORT_DECL(Menu, BackToGame) /* ? */
PORT_DECL(Menu, DrawScorer) /* ? */
PORT_DECL(Menu, InitMenuSys) /* ? */
PORT_DECL(Menu, MovieScorer) /* ? */
PORT_DECL(Menu, Init) /* ? */
PORT_DECL(Menu, DeathInfo) /* 1 */
PORT_DECL(Menu, ExitGame) /* ? */
PORT_DECL(Menu, ByeMenu) /* ? */
PORT_DECL(Menu, SetSkillMenu) /* ? */
PORT_DECL(Menu, SetSkill) /* ? */
PORT_DECL(Menu, StartNewGame) /* ? */
PORT_DECL(Menu, NewGame) /* ? */
PORT_DECL(Menu, AuxAbortGame) /* ? */
PORT_DECL(Menu, AbortGame) /* ? */
PORT_DECL(Menu, AuxLoadGame) /* ? */
PORT_DECL(Menu, LoadGame) /* ? */
PORT_DECL(Menu, LoadLastSavedGame) /* ? */
PORT_DECL(Menu, AuxSaveGame) /* ? */
PORT_DECL(Menu, SaveGame) /* ? */
PORT_DECL(Menu, LoadGameMenu) /* ? */
PORT_DECL(Menu, SaveGameMenu) /* ? */
PORT_DECL(Menu, isAutoSave) /* ? */
PORT_DECL(Menu, ComposeSaveGameName) /* ? */
PORT_DECL(Menu, Initialize) /* ? */
PORT_DECL(Menu, CreditsMenu) /* ? */
PORT_DECL(Menu, GameOver) /* ? */
PORT_DECL(Menu, DebugMenu) /* ? */
PORT_DECL(Menu, DebugSet_Level) /* ? */
PORT_DECL(Menu, DebugSet_Console) /* ? */
PORT_DECL(Menu, DebugSet_RenderInfo) /* ? */
PORT_DECL(Menu, DebugSet_SoundInfo) /* ? */
PORT_DECL(Menu, DebugSet_WorldInfo) /* ? */
PORT_DECL(Menu, DebugSet_ObjectPool) /* ? */
PORT_DECL(Menu, DebugSet_SceneRenderTarget) /* ? */
PORT_DECL(Menu, DebugSet_StencilShadows) /* ? */
PORT_DECL(Menu, DebugSet_AI) /* ? */
PORT_DECL(Menu, DebugSet_MapCollision) /* ? */
PORT_DECL(Menu, DebugSet_XboxPyDebug) /* ? */
PORT_DECL(Menu, DebugSet_CycleVar) /* ? */
PORT_DECL(Menu, DebugSet_ChangeMapMenu) /* ? */
PORT_DECL(Menu, DebugSet_ChangeMapExec) /* ? */
PORT_DECL(Menu, TVOff) /* ? */
PORT_DECL(Menu, SetCam) /* 2 */
PORT_DECL(Menu, TVOffEnd) /* ? */
PORT_DECL(Menu, BlackTV) /* ? */
#define PORTS_MenuMain \
	PORT_DEFN(Menu, SinglePlayerMenu, Menu_SinglePlayerMenu) \
	PORT_DEFN(Menu, AudioMenu, Menu_AudioMenu) \
	PORT_DEFN(Menu, AudioVolumeSliderFocus_Sub, Menu_AudioVolumeSliderFocus_Sub) \
	PORT_DEFN(Menu, AudioVolumeSliderFocus_Add, Menu_AudioVolumeSliderFocus_Add) \
	PORT_DEFN(Menu, AudioVolumeChange, Menu_AudioVolumeChange) \
	PORT_DEFN(Menu, MainMenu, Menu_MainMenu) \
	PORT_DEFN(Menu, CreateMainMenu, Menu_CreateMainMenu) \
	PORT_DEFN(Menu, PlaySpacelords, Mode_PyNone) \
	PORT_DEFN(Menu, BackToGame, Menu_BackToGame) \
	PORT_DEFN(Menu, DrawScorer, Menu_DrawScorer) \
	PORT_DEFN(Menu, InitMenuSys, Menu_InitMenuSys) \
	PORT_DEFN(Menu, MovieScorer, Menu_MovieScorer) \
	PORT_DEFN(Menu, Init, Menu_Init) \
	PORT_DEFN(Menu, DeathInfo, Menu_DeathInfo) \
	PORT_DEFN(Menu, ExitGame, Menu_ExitGame) \
	PORT_DEFN(Menu, ByeMenu, Menu_ByeMenu) \
	PORT_DEFN(Menu, SetSkillMenu, Menu_SetSkillMenu) \
	PORT_DEFN(Menu, SetSkill, Menu_SetSkill) \
	PORT_DEFN(Menu, StartNewGame, Menu_StartNewGame) \
	PORT_DEFN(Menu, NewGame, Menu_NewGame) \
	PORT_DEFN(Menu, AuxAbortGame, Menu_AuxAbortGame) \
	PORT_DEFN(Menu, AbortGame, Menu_AbortGame) \
	PORT_DEFN(Menu, AuxLoadGame, Menu_AuxLoadGame) \
	PORT_DEFN(Menu, LoadGame, Menu_LoadGame) \
	PORT_DEFN(Menu, LoadLastSavedGame, Menu_LoadLastSavedGame) \
	PORT_DEFN(Menu, AuxSaveGame, Menu_AuxSaveGame) \
	PORT_DEFN(Menu, SaveGame, Menu_SaveGame) \
	PORT_DEFN(Menu, LoadGameMenu, Menu_LoadGameMenu) \
	PORT_DEFN(Menu, SaveGameMenu, Menu_SaveGameMenu) \
	PORT_DEFN(Menu, isAutoSave, Menu_isAutoSave) \
	PORT_DEFN(Menu, ComposeSaveGameName, Menu_ComposeSaveGameName) \
	PORT_DEFN(Menu, Initialize, Menu_Initialize) \
	PORT_DEFN(Menu, CreditsMenu, Menu_CreditsMenu) \
	PORT_DEFN(Menu, GameOver, Menu_GameOver) \
	PORT_DEFN(Menu, DebugMenu, Menu_DebugMenu) \
	PORT_DEFN(Menu, DebugSet_Level, Menu_DebugSet_Level) \
	PORT_DEFN(Menu, DebugSet_Console, Menu_DebugSet_Console) \
	PORT_DEFN(Menu, DebugSet_RenderInfo, Menu_DebugSet_RenderInfo) \
	PORT_DEFN(Menu, DebugSet_SoundInfo, Menu_DebugSet_SoundInfo) \
	PORT_DEFN(Menu, DebugSet_WorldInfo, Menu_DebugSet_WorldInfo) \
	PORT_DEFN(Menu, DebugSet_ObjectPool, Menu_DebugSet_ObjectPool) \
	PORT_DEFN(Menu, DebugSet_SceneRenderTarget, Menu_DebugSet_SceneRenderTarget) \
	PORT_DEFN(Menu, DebugSet_StencilShadows, Menu_DebugSet_StencilShadows) \
	PORT_DEFN(Menu, DebugSet_AI, Menu_DebugSet_AI) \
	PORT_DEFN(Menu, DebugSet_MapCollision, Menu_DebugSet_MapCollision) \
	PORT_DEFN(Menu, DebugSet_XboxPyDebug, Menu_DebugSet_XboxPyDebug) \
	PORT_DEFN(Menu, DebugSet_CycleVar, Menu_DebugSet_CycleVar) \
	PORT_DEFN(Menu, DebugSet_ChangeMapMenu, Menu_DebugSet_ChangeMapMenu) \
	PORT_DEFN(Menu, DebugSet_ChangeMapExec, Menu_DebugSet_ChangeMapExec) \
	PORT_DEFN(Menu, TVOff, Menu_TVOff) \
	PORT_DEFN(Menu, SetCam, Menu_SetCam) \
	PORT_DEFN(Menu, TVOffEnd, Menu_TVOffEnd) \
	PORT_DEFN(Menu, BlackTV, Menu_BlackTV)

/* MenuMission.c */
PORT_DECL(Menu, MenuMissionInfo) /* ? */
PORT_DECL(Menu, MenuMissionInfo_Left) /* ? */
PORT_DECL(Menu, MenuMissionInfo_Right) /* ? */
PORT_DECL(Menu, GoPrimaryMission) /* ? */
PORT_DECL(Menu, GoCrazyDeal) /* ? */
PORT_DECL(Menu, GoNotes) /* ? */
PORT_DECL(Menu, EnterMissionInfo) /* ? */
PORT_DECL(Menu, ExitMissionInfo) /* ? */
PORT_DECL(Menu, MenuObjetive) /* ? */
PORT_DECL(Menu, CrazyDealObjetive) /* ? */
PORT_DECL(Menu, MissionNotesText) /* ? */
#define PORTS_MenuMission \
	PORT_DEFN(Menu, MenuMissionInfo, Menu_MenuMissionInfo) \
	PORT_DEFN(Menu, MenuMissionInfo_Left, Menu_MenuMissionInfo_Left) \
	PORT_DEFN(Menu, MenuMissionInfo_Right, Menu_MenuMissionInfo_Right) \
	PORT_DEFN(Menu, GoPrimaryMission, Menu_GoPrimaryMission) \
	PORT_DEFN(Menu, GoCrazyDeal, Menu_GoCrazyDeal) \
	PORT_DEFN(Menu, GoNotes, Menu_GoNotes) \
	PORT_DEFN(Menu, EnterMissionInfo, Menu_EnterMissionInfo) \
	PORT_DEFN(Menu, ExitMissionInfo, Menu_ExitMissionInfo) \
	PORT_DEFN(Menu, MenuObjetive, Menu_MenuObjetive) \
	PORT_DEFN(Menu, CrazyDealObjetive, Menu_CrazyDealObjetive) \
	PORT_DEFN(Menu, MissionNotesText, Menu_MissionNotesText)

/* MenuNet.c */
PORT_DECL(PCMenu, MultiPlayerMenu) /* ? */
PORT_DECL(PCMenu, MenuRestartServer) /* ? */
PORT_DECL(PCMenu, MenuJoinGame) /* ? */
PORT_DECL(PCMenu, TryJoin) /* ? */
PORT_DECL(PCMenu, JoinPassword) /* ? */
PORT_DECL(PCMenu, AskPassword) /* ? */
PORT_DECL(PCMenu, ShowConectionStatus) /* ? */
PORT_DECL(PCMenu, AbortJoin) /* ? */
PORT_DECL(PCMenu, SpecifyIPMenu) /* ? */
PORT_DECL(PCMenu, AbortMultiplayerGame) /* ? */
PORT_DECL(PCMenu, NetDefaultContentMenu) /* ? */
PORT_DECL(PCMenu, NetJoinGameMenu) /* ? */
PORT_DECL(PCMenu, NetJoinInternetMenu) /* ? */
PORT_DECL(PCMenu, NetJoinLocalMenu) /* ? */
PORT_DECL(PCMenu, NetJoinIPMenu) /* ? */
PORT_DECL(PCMenu, OnNetBrowse) /* ? */
PORT_DECL(PCMenu, BrowserSortExecute) /* ? */
PORT_DECL(PCMenu, BrowseLocalMenu) /* ? */
PORT_DECL(PCMenu, MasterCommand) /* ? */
PORT_DECL(PCMenu, BrowseINetMenu) /* ? */
PORT_DECL(PCMenu, InetMasterBrowse) /* ? */
PORT_DECL(PCMenu, BrowserMenuBack) /* ? */
PORT_DECL(PCMenu, RefreshBrowseMenu) /* ? */
PORT_DECL(PCMenu, RefreshBrowseMenuButton) /* ? */
PORT_DECL(PCMenu, BrowseLocalMenuPrevious) /* ? */
PORT_DECL(PCMenu, BrowseLocalMenuNext) /* ? */
PORT_DECL(PCMenu, BrowseLocalMenuPgUp) /* ? */
PORT_DECL(PCMenu, BrowseLocalMenuPgDown) /* ? */
PORT_DECL(PCMenu, BrowseLocalJoin) /* ? */
PORT_DECL(PCMenu, UnicodeParse) /* ? */
PORT_DECL(PCMenu, CreateBrowseMenu) /* ? */
PORT_DECL(PCMenu, BrowserColumnBack_Click) /* ? */
PORT_DECL(PCMenu, BrowserSliderGainFocus) /* ? */
PORT_DECL(PCMenu, BrowserSliderLooseFocus) /* ? */
PORT_DECL(PCMenu, BrowserSliderOnRender) /* ? */
PORT_DECL(PCMenu, UpdateBrowseSlider) /* ? */
PORT_DECL(PCMenu, UpdateBrowseMenu) /* ? */
PORT_DECL(PCMenu, BrowseFormatServerLine) /* ? */
PORT_DECL(PCMenu, NetCreateGameMenu) /* ? */
PORT_DECL(PCMenu, MenuInitServer) /* ? */
PORT_DECL(PCMenu, AdvancedServer) /* ? */
PORT_DECL(PCMenu, BackToCreateServer) /* ? */
PORT_DECL(PCMenu, SwitchForceVehicle) /* ? */
PORT_DECL(PCMenu, SwitchUsePass) /* ? */
PORT_DECL(PCMenu, RunDedicatedServer) /* ? */
PORT_DECL(PCMenu, SetDataRate) /* ? */
PORT_DECL(PCMenu, GetDataRateText) /* ? */
PORT_DECL(PCMenu, ServerTypePrev) /* ? */
PORT_DECL(PCMenu, ServerTypeNext) /* ? */
PORT_DECL(PCMenu, UpdateServerType) /* ? */
PORT_DECL(PCMenu, ServerTypeInfo) /* ? */
PORT_DECL(PCMenu, MapUp) /* ? */
PORT_DECL(PCMenu, MapDown) /* ? */
PORT_DECL(PCMenu, MapSliderGainFocus) /* ? */
PORT_DECL(PCMenu, MapSliderLooseFocus) /* ? */
PORT_DECL(PCMenu, MapSliderOnRender) /* ? */
PORT_DECL(PCMenu, UpdateMapSlider) /* ? */
PORT_DECL(PCMenu, UpdateMapList) /* ? */
PORT_DECL(PCMenu, MapChoose) /* ? */
PORT_DECL(PCMenu, MapPreviewOn) /* ? */
PORT_DECL(PCMenu, MapPreviewOff) /* ? */
PORT_DECL(PCMenu, ServerNameGainFocus) /* ? */
PORT_DECL(PCMenu, ServerNameLooseFocus) /* ? */
PORT_DECL(PCMenu, ServerNameOnRender) /* ? */
PORT_DECL(PCMenu, PlayersNumberGainFocus) /* ? */
PORT_DECL(PCMenu, PlayersNumberLooseFocus) /* ? */
PORT_DECL(PCMenu, PlayersNumberOnRender) /* ? */
PORT_DECL(PCMenu, FragLimitGainFocus) /* ? */
PORT_DECL(PCMenu, FragLimitLooseFocus) /* ? */
PORT_DECL(PCMenu, FragLimitOnRender) /* ? */
#define PORTS_MenuNet \
	PORT_DEFN(PCMenu, MultiPlayerMenu, PCMenu_MultiPlayerMenu) \
	PORT_DEFN(PCMenu, MenuRestartServer, PCMenu_MenuRestartServer) \
	PORT_DEFN(PCMenu, MenuJoinGame, PCMenu_MenuJoinGame) \
	PORT_DEFN(PCMenu, TryJoin, PCMenu_TryJoin) \
	PORT_DEFN(PCMenu, JoinPassword, PCMenu_JoinPassword) \
	PORT_DEFN(PCMenu, AskPassword, PCMenu_AskPassword) \
	PORT_DEFN(PCMenu, ShowConectionStatus, PCMenu_ShowConectionStatus) \
	PORT_DEFN(PCMenu, AbortJoin, PCMenu_AbortJoin) \
	PORT_DEFN(PCMenu, SpecifyIPMenu, PCMenu_SpecifyIPMenu) \
	PORT_DEFN(PCMenu, AbortMultiplayerGame, PCMenu_AbortMultiplayerGame) \
	PORT_DEFN(PCMenu, NetDefaultContentMenu, PCMenu_NetDefaultContentMenu) \
	PORT_DEFN(PCMenu, NetJoinGameMenu, PCMenu_NetJoinGameMenu) \
	PORT_DEFN(PCMenu, NetJoinInternetMenu, PCMenu_NetJoinInternetMenu) \
	PORT_DEFN(PCMenu, NetJoinLocalMenu, PCMenu_NetJoinLocalMenu) \
	PORT_DEFN(PCMenu, NetJoinIPMenu, PCMenu_NetJoinIPMenu) \
	PORT_DEFN(PCMenu, OnNetBrowse, PCMenu_OnNetBrowse) \
	PORT_DEFN(PCMenu, BrowserSortExecute, PCMenu_BrowserSortExecute) \
	PORT_DEFN(PCMenu, BrowseLocalMenu, PCMenu_BrowseLocalMenu) \
	PORT_DEFN(PCMenu, MasterCommand, PCMenu_MasterCommand) \
	PORT_DEFN(PCMenu, BrowseINetMenu, PCMenu_BrowseINetMenu) \
	PORT_DEFN(PCMenu, InetMasterBrowse, PCMenu_InetMasterBrowse) \
	PORT_DEFN(PCMenu, BrowserMenuBack, PCMenu_BrowserMenuBack) \
	PORT_DEFN(PCMenu, RefreshBrowseMenu, PCMenu_RefreshBrowseMenu) \
	PORT_DEFN(PCMenu, RefreshBrowseMenuButton, PCMenu_RefreshBrowseMenuButton) \
	PORT_DEFN(PCMenu, BrowseLocalMenuPrevious, PCMenu_BrowseLocalMenuPrevious) \
	PORT_DEFN(PCMenu, BrowseLocalMenuNext, PCMenu_BrowseLocalMenuNext) \
	PORT_DEFN(PCMenu, BrowseLocalMenuPgUp, PCMenu_BrowseLocalMenuPgUp) \
	PORT_DEFN(PCMenu, BrowseLocalMenuPgDown, PCMenu_BrowseLocalMenuPgDown) \
	PORT_DEFN(PCMenu, BrowseLocalJoin, PCMenu_BrowseLocalJoin) \
	PORT_DEFN(PCMenu, UnicodeParse, PCMenu_UnicodeParse) \
	PORT_DEFN(PCMenu, CreateBrowseMenu, PCMenu_CreateBrowseMenu) \
	PORT_DEFN(PCMenu, BrowserColumnBack_Click, PCMenu_BrowserColumnBack_Click) \
	PORT_DEFN(PCMenu, BrowserSliderGainFocus, PCMenu_BrowserSliderGainFocus) \
	PORT_DEFN(PCMenu, BrowserSliderLooseFocus, PCMenu_BrowserSliderLooseFocus) \
	PORT_DEFN(PCMenu, BrowserSliderOnRender, PCMenu_BrowserSliderOnRender) \
	PORT_DEFN(PCMenu, UpdateBrowseSlider, PCMenu_UpdateBrowseSlider) \
	PORT_DEFN(PCMenu, UpdateBrowseMenu, PCMenu_UpdateBrowseMenu) \
	PORT_DEFN(PCMenu, BrowseFormatServerLine, PCMenu_BrowseFormatServerLine) \
	PORT_DEFN(PCMenu, NetCreateGameMenu, PCMenu_NetCreateGameMenu) \
	PORT_DEFN(PCMenu, MenuInitServer, PCMenu_MenuInitServer) \
	PORT_DEFN(PCMenu, AdvancedServer, PCMenu_AdvancedServer) \
	PORT_DEFN(PCMenu, BackToCreateServer, PCMenu_BackToCreateServer) \
	PORT_DEFN(PCMenu, SwitchForceVehicle, PCMenu_SwitchForceVehicle) \
	PORT_DEFN(PCMenu, SwitchUsePass, PCMenu_SwitchUsePass) \
	PORT_DEFN(PCMenu, RunDedicatedServer, PCMenu_RunDedicatedServer) \
	PORT_DEFN(PCMenu, SetDataRate, PCMenu_SetDataRate) \
	PORT_DEFN(PCMenu, GetDataRateText, PCMenu_GetDataRateText) \
	PORT_DEFN(PCMenu, ServerTypePrev, PCMenu_ServerTypePrev) \
	PORT_DEFN(PCMenu, ServerTypeNext, PCMenu_ServerTypeNext) \
	PORT_DEFN(PCMenu, UpdateServerType, PCMenu_UpdateServerType) \
	PORT_DEFN(PCMenu, ServerTypeInfo, PCMenu_ServerTypeInfo) \
	PORT_DEFN(PCMenu, MapUp, PCMenu_MapUp) \
	PORT_DEFN(PCMenu, MapDown, PCMenu_MapDown) \
	PORT_DEFN(PCMenu, MapSliderGainFocus, PCMenu_MapSliderGainFocus) \
	PORT_DEFN(PCMenu, MapSliderLooseFocus, PCMenu_MapSliderLooseFocus) \
	PORT_DEFN(PCMenu, MapSliderOnRender, PCMenu_MapSliderOnRender) \
	PORT_DEFN(PCMenu, UpdateMapSlider, PCMenu_UpdateMapSlider) \
	PORT_DEFN(PCMenu, UpdateMapList, PCMenu_UpdateMapList) \
	PORT_DEFN(PCMenu, MapChoose, PCMenu_MapChoose) \
	PORT_DEFN(PCMenu, MapPreviewOn, PCMenu_MapPreviewOn) \
	PORT_DEFN(PCMenu, MapPreviewOff, PCMenu_MapPreviewOff) \
	PORT_DEFN(PCMenu, ServerNameGainFocus, PCMenu_ServerNameGainFocus) \
	PORT_DEFN(PCMenu, ServerNameLooseFocus, PCMenu_EditLooseFocus) \
	PORT_DEFN(PCMenu, ServerNameOnRender, PCMenu_ServerNameOnRender) \
	PORT_DEFN(PCMenu, PlayersNumberGainFocus, PCMenu_PlayersNumberGainFocus) \
	PORT_DEFN(PCMenu, PlayersNumberLooseFocus, PCMenu_EditLooseFocus) \
	PORT_DEFN(PCMenu, PlayersNumberOnRender, PCMenu_PlayersNumberOnRender) \
	PORT_DEFN(PCMenu, FragLimitGainFocus, PCMenu_FragLimitGainFocus) \
	PORT_DEFN(PCMenu, FragLimitLooseFocus, PCMenu_EditLooseFocus) \
	PORT_DEFN(PCMenu, FragLimitOnRender, PCMenu_FragLimitOnRender)

/* MenuVideo.c */
PORT_DECL(PCMenu, VideoMenu) /* ? */
PORT_DECL(PCMenu, VideoMenuBack) /* ? */
PORT_DECL(PCMenu, VideoDefaults) /* ? */
PORT_DECL(PCMenu, ApplyChanges) /* ? */
PORT_DECL(PCMenu, ChangeVideoMode) /* ? */
PORT_DECL(PCMenu, PrevVideoMode) /* ? */
PORT_DECL(PCMenu, NextVideoMode) /* ? */
PORT_DECL(PCMenu, ChangeTextureFilter) /* ? */
PORT_DECL(PCMenu, ChangeTextureDetail) /* ? */
PORT_DECL(PCMenu, ChangeShadowsDetail) /* ? */
PORT_DECL(PCMenu, ChangeVSync) /* ? */
PORT_DECL(PCMenu, ChangeLock) /* ? */
PORT_DECL(PCMenu, ChangeFog) /* ? */
PORT_DECL(PCMenu, ChangeBloom) /* ? */
PORT_DECL(PCMenu, ChangeMotionBlur) /* ? */
PORT_DECL(PCMenu, ChangeDuDv) /* ? */
PORT_DECL(PCMenu, ChangeGamma) /* ? */
PORT_DECL(PCMenu, GammaSliderFocus_Sub) /* ? */
PORT_DECL(PCMenu, GammaSliderFocus_Add) /* ? */
PORT_DECL(PCMenu, ChangeDisplay) /* ? */
#define PORTS_MenuVideo \
	PORT_DEFN(PCMenu, VideoMenu, PCMenu_VideoMenu) \
	PORT_DEFN(PCMenu, VideoMenuBack, PCMenu_VideoMenuBack) \
	PORT_DEFN(PCMenu, VideoDefaults, PCMenu_VideoDefaults) \
	PORT_DEFN(PCMenu, ApplyChanges, PCMenu_ApplyChanges) \
	PORT_DEFN(PCMenu, ChangeVideoMode, PCMenu_ChangeVideoMode) \
	PORT_DEFN(PCMenu, PrevVideoMode, PCMenu_PrevVideoMode) \
	PORT_DEFN(PCMenu, NextVideoMode, PCMenu_NextVideoMode) \
	PORT_DEFN(PCMenu, ChangeTextureFilter, PCMenu_ChangeTextureFilter) \
	PORT_DEFN(PCMenu, ChangeTextureDetail, PCMenu_ChangeTextureDetail) \
	PORT_DEFN(PCMenu, ChangeShadowsDetail, PCMenu_ChangeShadowsDetail) \
	PORT_DEFN(PCMenu, ChangeVSync, PCMenu_ChangeVSync) \
	PORT_DEFN(PCMenu, ChangeLock, PCMenu_ChangeLock) \
	PORT_DEFN(PCMenu, ChangeFog, PCMenu_ChangeFog) \
	PORT_DEFN(PCMenu, ChangeBloom, PCMenu_ChangeBloom) \
	PORT_DEFN(PCMenu, ChangeMotionBlur, PCMenu_ChangeMotionBlur) \
	PORT_DEFN(PCMenu, ChangeDuDv, PCMenu_ChangeDuDv) \
	PORT_DEFN(PCMenu, ChangeGamma, PCMenu_ChangeGamma) \
	PORT_DEFN(PCMenu, GammaSliderFocus_Sub, PCMenu_GammaSliderFocus_Sub) \
	PORT_DEFN(PCMenu, GammaSliderFocus_Add, PCMenu_GammaSliderFocus_Add) \
	PORT_DEFN(PCMenu, ChangeDisplay, PCMenu_ChangeDisplay)

/* MenuVideoVars.c */
PORT_DECL(PCMenu, Apply_SetActive) /* 2 */
PORT_DECL(PCMenu, Apply_SetFog) /* 2 */
PORT_DECL(PCMenu, Apply_SetTextureFilter) /* 2 */
PORT_DECL(PCMenu, Apply_SetVideoLock) /* 2 */
PORT_DECL(PCMenu, ChangeBoolean) /* 2 */
PORT_DECL(PCMenu, DrawLine) /* ? */
PORT_DECL(PCMenu, FillShadowsDetail) /* 0 */
PORT_DECL(PCMenu, FillTextureDetail) /* 0 */
PORT_DECL(PCMenu, FillTextureFilter) /* 0 */
PORT_DECL(PCMenu, FillVideoModes) /* 0 */
PORT_DECL(PCMenu, GammaFormula) /* ? */
PORT_DECL(PCMenu, MouseSensitivityFormula) /* ? */
PORT_DECL(PCMenu, UpdateVideoOptions) /* 1 */
PORT_DECL(PCMenu, VideoGetLast) /* 1 */
#define PORTS_MenuVideoVars \
	PORT_DEFN(PCMenu, Apply_SetActive, PCMenu_Apply_SetActive) \
	PORT_DEFN(PCMenu, Apply_SetFog, PCMenu_Apply_SetFog) \
	PORT_DEFN(PCMenu, Apply_SetTextureFilter, PCMenu_Apply_SetTextureFilter) \
	PORT_DEFN(PCMenu, Apply_SetVideoLock, PCMenu_Apply_SetVideoLock) \
	PORT_DEFN(PCMenu, ChangeBoolean, PCMenu_ChangeBoolean) \
	PORT_DEFNK(PCMenu, DrawLine, PCMenu_DrawLine) \
	PORT_DEFN(PCMenu, FillShadowsDetail, PCMenu_FillShadowsDetail) \
	PORT_DEFN(PCMenu, FillTextureDetail, PCMenu_FillTextureDetail) \
	PORT_DEFN(PCMenu, FillTextureFilter, PCMenu_FillTextureFilter) \
	PORT_DEFN(PCMenu, FillVideoModes, PCMenu_FillVideoModes) \
	PORT_DEFNK(PCMenu, GammaFormula, PCMenu_GammaFormula) \
	PORT_DEFNK(PCMenu, MouseSensitivityFormula, PCMenu_MouseSensitivityFormula) \
	PORT_DEFN(PCMenu, UpdateVideoOptions, PCMenu_UpdateVideoOptions) \
	PORT_DEFN(PCMenu, VideoGetLast, PCMenu_VideoGetLast)

/* Metro.c */
PORT_DECL(Metro, CalculateMissionMap) /* 1 */
PORT_DECL(Metro, WrongOpt) /* ? */
PORT_DECL(Metro, RecoverUsr) /* 3 */
PORT_DECL(Metro, OutDoorInitializator) /* ? */
PORT_DECL(Metro, SetUsrParams) /* ? */
PORT_DECL(Metro, UseMetro) /* 2 */
PORT_DECL(Metro, CreateStation) /* ? */
PORT_DECL(Metro, DoNothing) /* ? */
PORT_DECL(Metro, ShowHint) /* 3 */
PORT_DECL(Metro, HideHint) /* 3 */
PORT_DECL(Metro, OnUse) /* 4 */
PORT_DECL(Metro, SetAsMetro) /* 1 */
PORT_DECL(Metro, DoorInitializator) /* 1 */
PORT_DECL(Metro, ObjInitializator) /* 2 */
#define PORTS_Metro \
	PORT_DEFN(Metro, CalculateMissionMap, Metro_CalculateMissionMap) \
	PORT_DEFN(Metro, WrongOpt, Mode_PyNone2) \
	PORT_DEFN(Metro, RecoverUsr, Metro_RecoverUsrPy) \
	PORT_DEFN(Metro, OutDoorInitializator, Metro_OutDoorInitializator) \
	PORT_DEFN(Metro, SetUsrParams, Metro_SetUsrParamsPy) \
	PORT_DEFN(Metro, UseMetro, Metro_UseMetroPy) \
	PORT_DEFN(Metro, CreateStation, Metro_CreateStation) \
	PORT_DEFN(Metro, DoNothing, Mode_PyNone2) \
	PORT_DEFN(Metro, ShowHint, Metro_ShowHintPy) \
	PORT_DEFN(Metro, HideHint, Metro_HideHintPy) \
	PORT_DEFN(Metro, OnUse, Metro_OnUsePy) \
	PORT_DEFN(Metro, SetAsMetro, Metro_SetAsMetroPy) \
	PORT_DEFN(Metro, DoorInitializator, Metro_DoorInitializator) \
	PORT_DEFN(Metro, ObjInitializator, Metro_ObjInitializator)

/* MisItems.c */
PORT_DECL(MisItems, Init) /* 0 */
PORT_DECL(MisItems, OnVehicleDeat) /* 1 */
PORT_DECL(MisItems, GetTargetReward) /* 1 */
PORT_DECL(MisItems, GiveMoney) /* 2 */
PORT_DECL(MisItems, GetMoneyModel) /* 1 */
PORT_DECL(MisItems, GetObjModel) /* 1 */
PORT_DECL(MisItems, CreateItem) /* 4 */
PORT_DECL(MisItems, CreateCoin) /* 2 */
PORT_DECL(MisItems, ActivateCoin) /* 1 */
PORT_DECL(MisItems, CreateBattery) /* ? */
PORT_DECL(MisItems, ActivateBattery) /* 1 */
PORT_DECL(MisItems, TakeBattery) /* 2 */
PORT_DECL(MisItems, DazedCoin) /* 3 */
PORT_DECL(CrazyWing, InitCrazyWing) /* 0 */
PORT_DECL(CrazyWing, CrazyWing) /* 0 */
PORT_DECL(CrazyWing, SetCrazy) /* 0 */
PORT_DECL(CrazyWing, DeCrazy) /* 2 */
PORT_DECL(CrazyWing, RestoreSpeech) /* 0 */
PORT_DECL(CrazyWing, AllInList) /* 0 */
PORT_DECL(CrazyWing, CrazyPhoto) /* 1 */
#define PORTS_MisItems \
	PORT_DEFN(MisItems, Init, MisItems_Init) \
	PORT_DEFN(MisItems, OnVehicleDeat, MisItems_OnVehicleDeat) \
	PORT_DEFN(MisItems, GetTargetReward, MisItems_GetTargetReward) \
	PORT_DEFN(MisItems, GiveMoney, MisItems_GiveMoney) \
	PORT_DEFN(MisItems, GetMoneyModel, MisItems_GetMoneyModel) \
	PORT_DEFN(MisItems, GetObjModel, MisItems_GetObjModel) \
	PORT_DEFN(MisItems, CreateItem, MisItems_CreateItem) \
	PORT_DEFN(MisItems, CreateCoin, MisItems_CreateCoin) \
	PORT_DEFN(MisItems, ActivateCoin, MisItems_ActivateCoin) \
	PORT_DEFN(MisItems, CreateBattery, MisItems_CreateBattery) \
	PORT_DEFN(MisItems, ActivateBattery, MisItems_ActivateBattery) \
	PORT_DEFN(MisItems, TakeBattery, MisItems_TakeBattery) \
	PORT_DEFN(MisItems, DazedCoin, MisItems_DazedCoin) \
	PORT_DEFN(CrazyWing, InitCrazyWing, CrazyWing_InitCrazyWing) \
	PORT_DEFN(CrazyWing, CrazyWing, CrazyWing_CrazyWing) \
	PORT_DEFN(CrazyWing, SetCrazy, CrazyWing_SetCrazy) \
	PORT_DEFN(CrazyWing, DeCrazy, CrazyWing_DeCrazy) \
	PORT_DEFN(CrazyWing, RestoreSpeech, CrazyWing_RestoreSpeech) \
	PORT_DEFN(CrazyWing, AllInList, CrazyWing_AllInList) \
	PORT_DEFN(CrazyWing, CrazyPhoto, CrazyWing_CrazyPhoto)

/* MissionsFuncs.c */
PORT_DECL(MissionsFuncs, Init) /* 0 */
PORT_DECL(MissionsFuncs, TryMsg) /* 1 */
PORT_DECL(MissionsFuncs, SetNextMission) /* 2 */
PORT_DECL(MissionsFuncs, SetFirstDemoMission) /* 0 */
PORT_DECL(MissionsFuncs, SetOutMissionArrow) /* 0 */
PORT_DECL(MissionsFuncs, ActivateTarget) /* ? */
PORT_DECL(MissionsFuncs, GetSecondary) /* 1 */
PORT_DECL(MissionsFuncs, SetSecondary) /* 1 */
PORT_DECL(MissionsFuncs, FinishGame) /* 1 */
PORT_DECL(MissionsFuncs, ShowMissionInfo) /* 1 */
PORT_DECL(MissionsFuncs, SetMissionTarget) /* 1 */
PORT_DECL(MissionsFuncs, SetMultipleMissionTargets) /* 1 */
PORT_DECL(MissionsFuncs, CleanAllTargets) /* 0 */
PORT_DECL(MissionsFuncs, DelMissionTarget) /* 1 */
PORT_DECL(MissionsFuncs, IsCharMissionObjective) /* 1 */
PORT_DECL(MissionsFuncs, CharMissionDialog) /* 4 */
PORT_DECL(MissionsFuncs, CancelSecondaryMission) /* 2 */
PORT_DECL(MissionsFuncs, SinglePlayerDeath) /* 0 */
PORT_DECL(MissionsFuncs, CreatePhotoObject) /* ? */
PORT_DECL(MissionsFuncs, UnsetPhotoObject) /* 1 */
PORT_DECL(MissionsFuncs, ReassignBertosPlayerTarget) /* 0 */
PORT_DECL(MissionsFuncs, CreateBerto) /* 1 */
PORT_DECL(MissionsFuncs, CreateBertos) /* 0 */
PORT_DECL(MissionsFuncs, OnBertoDeath) /* 2 */
PORT_DECL(MissionsFuncs, CrazyDealStatus) /* 1 */
PORT_DECL(MissionsFuncs, GetNumCrazyDealCompleted) /* 0 */
PORT_DECL(MissionsFuncs, OnCrazyDealFinishedMessage) /* 0 */
PORT_DECL(MissionsFuncs, OnCrazyDealTarget) /* 0 */
PORT_DECL(MissionsFuncs, NewCrazyDeal) /* 3 */
PORT_DECL(MissionsFuncs, SetNextSuperDeal) /* 3 */
PORT_DECL(MissionsFuncs, StartActualSuperDeal) /* 0 */
PORT_DECL(MissionsFuncs, EndOfSuperDeal) /* 1 */
PORT_DECL(MissionsFuncs, StartSuperDeal) /* 3 */
PORT_DECL(MissionsFuncs, RelaxShipsInList) /* 1 */
PORT_DECL(MissionsFuncs, SetInvulnerableShipsInList) /* ? */
PORT_DECL(MissionsFuncs, OnSuperDealFinish) /* ? */
PORT_DECL(MissionsFuncs, BuildMultipleEnemiesData) /* 2 */
PORT_DECL(MissionsFuncs, BuildTeamDeathMatchData) /* 1 */
PORT_DECL(MissionsFuncs, BuildVersusData) /* 1 */
PORT_DECL(MissionsFuncs, BuildDominationData) /* 1 */
PORT_DECL(MissionsFuncs, BuildCTFData) /* 1 */
PORT_DECL(MissionsFuncs, SuperDealOnDefeat) /* 2 */
PORT_DECL(MissionsFuncs, SuperDealOnVictory) /* 2 */
#define PORTS_MissionsFuncs \
	PORT_DEFN(MissionsFuncs, Init, MissionsFuncs_Init) \
	PORT_DEFN(MissionsFuncs, TryMsg, MissionsFuncs_TryMsg) \
	PORT_DEFN(MissionsFuncs, SetNextMission, MissionsFuncs_SetNextMission) \
	PORT_DEFN(MissionsFuncs, SetFirstDemoMission, MissionsFuncs_SetFirstDemoMission) \
	PORT_DEFN(MissionsFuncs, SetOutMissionArrow, MissionsFuncs_SetOutMissionArrow) \
	PORT_DEFN(MissionsFuncs, ActivateTarget, MissionsFuncs_ActivateTarget) \
	PORT_DEFN(MissionsFuncs, GetSecondary, MissionsFuncs_GetSecondary) \
	PORT_DEFN(MissionsFuncs, SetSecondary, MissionsFuncs_SetSecondary) \
	PORT_DEFN(MissionsFuncs, FinishGame, MissionsFuncs_FinishGame) \
	PORT_DEFN(MissionsFuncs, ShowMissionInfo, MissionsFuncs_ShowMissionInfo) \
	PORT_DEFN(MissionsFuncs, SetMissionTarget, MissionsFuncs_SetMissionTarget) \
	PORT_DEFN(MissionsFuncs, SetMultipleMissionTargets, MissionsFuncs_SetMultipleMissionTargets) \
	PORT_DEFN(MissionsFuncs, CleanAllTargets, MissionsFuncs_CleanAllTargets) \
	PORT_DEFN(MissionsFuncs, DelMissionTarget, MissionsFuncs_DelMissionTarget) \
	PORT_DEFN(MissionsFuncs, IsCharMissionObjective, MissionsFuncs_IsCharMissionObjective) \
	PORT_DEFN(MissionsFuncs, CharMissionDialog, MissionsFuncs_CharMissionDialog) \
	PORT_DEFN(MissionsFuncs, CancelSecondaryMission, MissionsFuncs_CancelSecondaryMission) \
	PORT_DEFN(MissionsFuncs, SinglePlayerDeath, MissionsFuncs_SinglePlayerDeath) \
	PORT_DEFN(MissionsFuncs, CreatePhotoObject, MissionsFuncs_CreatePhotoObject) \
	PORT_DEFN(MissionsFuncs, UnsetPhotoObject, MissionsFuncs_UnsetPhotoObject) \
	PORT_DEFN(MissionsFuncs, ReassignBertosPlayerTarget, MissionsFuncs_ReassignBertosPlayerTarget) \
	PORT_DEFN(MissionsFuncs, CreateBerto, MissionsFuncs_CreateBerto) \
	PORT_DEFN(MissionsFuncs, CreateBertos, MissionsFuncs_CreateBertos) \
	PORT_DEFN(MissionsFuncs, OnBertoDeath, MissionsFuncs_OnBertoDeath) \
	PORT_DEFN(MissionsFuncs, CrazyDealStatus, MissionsFuncs_CrazyDealStatus) \
	PORT_DEFN(MissionsFuncs, GetNumCrazyDealCompleted, MissionsFuncs_GetNumCrazyDealCompleted) \
	PORT_DEFN(MissionsFuncs, OnCrazyDealFinishedMessage, MissionsFuncs_OnCrazyDealFinishedMessage) \
	PORT_DEFN(MissionsFuncs, OnCrazyDealTarget, MissionsFuncs_OnCrazyDealTarget) \
	PORT_DEFN(MissionsFuncs, NewCrazyDeal, MissionsFuncs_NewCrazyDeal) \
	PORT_DEFN(MissionsFuncs, SetNextSuperDeal, MissionsFuncs_SetNextSuperDeal) \
	PORT_DEFN(MissionsFuncs, StartActualSuperDeal, MissionsFuncs_StartActualSuperDeal) \
	PORT_DEFN(MissionsFuncs, EndOfSuperDeal, MissionsFuncs_EndOfSuperDeal) \
	PORT_DEFN(MissionsFuncs, StartSuperDeal, MissionsFuncs_StartSuperDeal) \
	PORT_DEFN(MissionsFuncs, RelaxShipsInList, MissionsFuncs_RelaxShipsInList) \
	PORT_DEFN(MissionsFuncs, SetInvulnerableShipsInList, MissionsFuncs_SetInvulnerableShipsInList) \
	PORT_DEFN(MissionsFuncs, OnSuperDealFinish, MissionsFuncs_OnSuperDealFinish) \
	PORT_DEFN(MissionsFuncs, BuildMultipleEnemiesData, MissionsFuncs_BuildMultipleEnemiesData) \
	PORT_DEFN(MissionsFuncs, BuildTeamDeathMatchData, MissionsFuncs_BuildTeamDeathMatchData) \
	PORT_DEFN(MissionsFuncs, BuildVersusData, MissionsFuncs_BuildVersusData) \
	PORT_DEFN(MissionsFuncs, BuildDominationData, MissionsFuncs_BuildDominationData) \
	PORT_DEFN(MissionsFuncs, BuildCTFData, MissionsFuncs_BuildCTFData) \
	PORT_DEFN(MissionsFuncs, SuperDealOnDefeat, MissionsFuncs_SuperDealOnDefeat) \
	PORT_DEFN(MissionsFuncs, SuperDealOnVictory, MissionsFuncs_SuperDealOnVictory)

/* Net.c */
PORT_DECL(Net, PurgeServerMapList) /* ? */
PORT_DECL(Net, Spectate) /* 2 */
PORT_DECL(Net, ClientCreateDef) /* ? */
PORT_DECL(Net, ClientAfterCreateDef) /* ? */
PORT_DECL(Net, ClientJoinDef) /* 11 */
PORT_DECL(Net, LoadSinglePlayer) /* 1 */
#define PORTS_Net \
	PORT_DEFN(Net, PurgeServerMapList, Net_PurgeServerMapList) \
	PORT_DEFN(Net, Spectate, Net_Spectate) \
	PORT_DEFN(Net, ClientCreateDef, Mode_PyNone2) \
	PORT_DEFN(Net, ClientAfterCreateDef, Mode_PyNone2) \
	PORT_DEFN(Net, ClientJoinDef, Net_ClientJoinDef) \
	PORT_DEFN(Net, LoadSinglePlayer, Net_LoadSinglePlayer)

/* NetModes.c */
PORT_DECL(NDeathMatch, DeathFunc) /* 3 */
PORT_DECL(NDeathMatch, CheckEndOfGame) /* 0 */
PORT_DECL(NDeathMatch, GetGameDescriptor) /* 0 */
PORT_DECL(NDeathMatch, InitServerMod) /* 0 */
PORT_DECL(NDeathMatch, InitClientMod) /* 0 */
PORT_DECL(NDeathMatch, ShowHelp) /* 1 */
PORT_DECL(NDeathMatch, ServerHidePlayer) /* 1 */
PORT_DECL(NDeathMatchBots, DeathFunc) /* 3 */
PORT_DECL(NDeathMatchBots, Reborn) /* 2 */
PORT_DECL(NDeathMatchBots, CheckEndOfGame) /* 0 */
PORT_DECL(NDeathMatchBots, GetGameDescriptor) /* 0 */
PORT_DECL(NDeathMatchBots, TryGarage) /* 2 */
PORT_DECL(NDeathMatchBots, SetBotState) /* 1 */
PORT_DECL(NDeathMatchBots, CheckBotState) /* 1 */
PORT_DECL(NDeathMatchBots, InitServerMod) /* 0 */
PORT_DECL(NDeathMatchBots, InitClientMod) /* 0 */
PORT_DECL(NDeathMatchBots, ShowHelp) /* 1 */
PORT_DECL(NDeathMatchBots, ServerHidePlayer) /* 1 */
PORT_DECL(NTeamDM, DeathFunc) /* 3 */
PORT_DECL(NTeamDM, CheckEndOfGame) /* 0 */
PORT_DECL(NTeamDM, GetGameDescriptor) /* 0 */
PORT_DECL(NTeamDM, InitServerMod) /* 0 */
PORT_DECL(NTeamDM, InitClientMod) /* 0 */
PORT_DECL(NTeamDM, ShowHelp) /* 1 */
PORT_DECL(NTeamDM, ServerHidePlayer) /* 1 */
PORT_DECL(NTeamFlag, DeathFunc) /* 3 */
PORT_DECL(NTeamFlag, InitServerMod) /* 0 */
PORT_DECL(NTeamFlag, InitClientMod) /* 0 */
PORT_DECL(NTeamFlag, ShowHelp) /* 1 */
PORT_DECL(NTeamFlag, ServerHidePlayer) /* 1 */
PORT_DECL(NTeamFlag, GetRndFlag) /* 0 */
PORT_DECL(NTeamFlag, BrowseAreas) /* 0 */
PORT_DECL(NTeamFlag, CreateFlag) /* 1 */
PORT_DECL(NTeamFlag, TakeFlag) /* 1 */
PORT_DECL(NTeamFlag, OnUsrClient) /* 2 */
PORT_DECL(NTeamFlag, ClientJoin) /* 11 */
PORT_DECL(NTeamFlag, ServerTakeFlag) /* 2 */
PORT_DECL(NTeamFlag, CheckEndOfGame) /* 0 */
PORT_DECL(NTeamFlag, GetGameDescriptor) /* 0 */
PORT_DECL(NFlagHunt, DeathFunc) /* 3 */
PORT_DECL(NFlagHunt, InitServerMod) /* 0 */
PORT_DECL(NFlagHunt, InitClientMod) /* 0 */
PORT_DECL(NFlagHunt, ShowHelp) /* 1 */
PORT_DECL(NFlagHunt, ServerHidePlayer) /* 1 */
PORT_DECL(NFlagHunt, GetRndFlag) /* 0 */
PORT_DECL(NFlagHunt, BrowseAreas) /* 0 */
PORT_DECL(NFlagHunt, CreateFlag) /* 1 */
PORT_DECL(NFlagHunt, TakeFlag) /* 1 */
PORT_DECL(NFlagHunt, OnUsrClient) /* 2 */
PORT_DECL(NFlagHunt, ClientJoin) /* 11 */
PORT_DECL(NFlagHunt, ServerTakeFlag) /* 2 */
PORT_DECL(NFlagHunt, CheckEndOfGame) /* 0 */
PORT_DECL(NFlagHunt, GetGameDescriptor) /* 0 */
PORT_DECL(NPostman, DeathFunc) /* 3 */
PORT_DECL(NPostman, InitServerMod) /* 0 */
PORT_DECL(NPostman, InitClientMod) /* 0 */
PORT_DECL(NPostman, ShowHelp) /* 1 */
PORT_DECL(NPostman, ServerHidePlayer) /* 1 */
PORT_DECL(NPostman, GetRndZone) /* 0 */
PORT_DECL(NPostman, BrowseAreas) /* 0 */
PORT_DECL(NPostman, CreateFlag) /* 1 */
PORT_DECL(NPostman, LinkFlagTo) /* 2 */
PORT_DECL(NPostman, TryLinkFlagTo) /* 2 */
PORT_DECL(NPostman, TakeFlag) /* 1 */
PORT_DECL(NPostman, DropFlag) /* 1 */
PORT_DECL(NPostman, AbortZoneKeys) /* 0 */
PORT_DECL(NPostman, OnUsrClient) /* 2 */
PORT_DECL(NPostman, ClientJoin) /* 11 */
PORT_DECL(NPostman, ServerTakeBase) /* 2 */
PORT_DECL(NPostman, ServerDropFlag) /* 0 */
PORT_DECL(NPostman, ServerTakeFlag) /* 2 */
PORT_DECL(NPostman, RespawnFlag) /* 0 */
PORT_DECL(NPostman, CheckEndOfGame) /* 0 */
PORT_DECL(NPostman, GetGameDescriptor) /* 0 */
PORT_DECL(NTeamPost, DeathFunc) /* 3 */
PORT_DECL(NTeamPost, InitServerMod) /* 0 */
PORT_DECL(NTeamPost, InitClientMod) /* 0 */
PORT_DECL(NTeamPost, ShowHelp) /* 1 */
PORT_DECL(NTeamPost, ServerHidePlayer) /* 1 */
PORT_DECL(NTeamPost, GetRndZone) /* 0 */
PORT_DECL(NTeamPost, BrowseAreas) /* 0 */
PORT_DECL(NTeamPost, CreateFlag) /* 1 */
PORT_DECL(NTeamPost, LinkFlagTo) /* 2 */
PORT_DECL(NTeamPost, TryLinkFlagTo) /* 2 */
PORT_DECL(NTeamPost, TakeFlag) /* 1 */
PORT_DECL(NTeamPost, DropFlag) /* 1 */
PORT_DECL(NTeamPost, AbortZoneKeys) /* 0 */
PORT_DECL(NTeamPost, OnUsrClient) /* 2 */
PORT_DECL(NTeamPost, ClientJoin) /* 11 */
PORT_DECL(NTeamPost, ServerTakeBase) /* 2 */
PORT_DECL(NTeamPost, ServerDropFlag) /* 0 */
PORT_DECL(NTeamPost, ServerTakeFlag) /* 2 */
PORT_DECL(NTeamPost, RespawnFlag) /* 0 */
PORT_DECL(NTeamPost, CheckEndOfGame) /* 0 */
PORT_DECL(NTeamPost, GetGameDescriptor) /* 0 */
#define PORTS_NetModes \
	PORT_DEFN(NDeathMatch, DeathFunc, NDeathMatch_DeathFunc) \
	PORT_DEFN(NDeathMatch, CheckEndOfGame, NDeathMatch_CheckEndOfGame) \
	PORT_DEFN(NDeathMatch, GetGameDescriptor, NDeathMatch_GetGameDescriptor) \
	PORT_DEFN(NDeathMatch, InitServerMod, NDeathMatch_InitClientModPass) \
	PORT_DEFN(NDeathMatch, InitClientMod, NDeathMatch_InitClientModPass) \
	PORT_DEFN(NDeathMatch, ShowHelp, NDeathMatch_ShowHelp) \
	PORT_DEFN(NDeathMatch, ServerHidePlayer, NDeathMatch_ServerHidePlayerPass) \
	PORT_DEFN(NDeathMatchBots, DeathFunc, NDeathMatchBots_DeathFunc) \
	PORT_DEFN(NDeathMatchBots, Reborn, NDeathMatchBots_Reborn) \
	PORT_DEFN(NDeathMatchBots, CheckEndOfGame, NDeathMatchBots_CheckEndOfGame) \
	PORT_DEFN(NDeathMatchBots, GetGameDescriptor, NDeathMatchBots_GetGameDescriptor) \
	PORT_DEFN(NDeathMatchBots, TryGarage, NDeathMatchBots_TryGarage) \
	PORT_DEFN(NDeathMatchBots, SetBotState, NDeathMatchBots_SetBotState) \
	PORT_DEFN(NDeathMatchBots, CheckBotState, NDeathMatchBots_CheckBotState) \
	PORT_DEFN(NDeathMatchBots, InitServerMod, NDeathMatchBots_InitServerMod) \
	PORT_DEFN(NDeathMatchBots, InitClientMod, NDeathMatchBots_InitClientModPass) \
	PORT_DEFN(NDeathMatchBots, ShowHelp, NDeathMatchBots_ShowHelp) \
	PORT_DEFN(NDeathMatchBots, ServerHidePlayer, NDeathMatchBots_ServerHidePlayerPass) \
	PORT_DEFN(NTeamDM, DeathFunc, NTeamDM_DeathFunc) \
	PORT_DEFN(NTeamDM, CheckEndOfGame, NTeamDM_CheckEndOfTeamGame) \
	PORT_DEFN(NTeamDM, GetGameDescriptor, NTeamDM_TeamDescriptor) \
	PORT_DEFN(NTeamDM, InitServerMod, NTeamDM_InitClientModPass) \
	PORT_DEFN(NTeamDM, InitClientMod, NTeamDM_InitClientModPass) \
	PORT_DEFN(NTeamDM, ShowHelp, NTeamDM_ShowHelp) \
	PORT_DEFN(NTeamDM, ServerHidePlayer, NTeamDM_ServerHidePlayerPass) \
	PORT_DEFN(NTeamFlag, DeathFunc, NTeamFlag_DeathFunc) \
	PORT_DEFN(NTeamFlag, InitServerMod, NTeamFlag_InitServerMod) \
	PORT_DEFN(NTeamFlag, InitClientMod, NTeamFlag_InitClientMod) \
	PORT_DEFN(NTeamFlag, ShowHelp, NTeamFlag_ShowHelp) \
	PORT_DEFN(NTeamFlag, ServerHidePlayer, NTeamFlag_ServerHidePlayerPass) \
	PORT_DEFN(NTeamFlag, GetRndFlag, NTeamFlag_GetRndFlag) \
	PORT_DEFN(NTeamFlag, BrowseAreas, NTeamFlag_BrowseAreas) \
	PORT_DEFN(NTeamFlag, CreateFlag, NTeamFlag_CreateFlag) \
	PORT_DEFN(NTeamFlag, TakeFlag, NTeamFlag_TakeFlag) \
	PORT_DEFN(NTeamFlag, OnUsrClient, NTeamFlag_OnUsrClient) \
	PORT_DEFN(NTeamFlag, ClientJoin, NTeamFlag_ClientJoin) \
	PORT_DEFN(NTeamFlag, ServerTakeFlag, NTeamFlag_ServerTakeFlag) \
	PORT_DEFN(NTeamFlag, CheckEndOfGame, NTeamFlag_CheckEndOfTeamGame) \
	PORT_DEFN(NTeamFlag, GetGameDescriptor, NTeamFlag_TeamDescriptor) \
	PORT_DEFN(NFlagHunt, DeathFunc, NFlagHunt_DeathFunc) \
	PORT_DEFN(NFlagHunt, InitServerMod, NFlagHunt_InitServerMod) \
	PORT_DEFN(NFlagHunt, InitClientMod, NFlagHunt_InitClientMod) \
	PORT_DEFN(NFlagHunt, ShowHelp, NFlagHunt_ShowHelp) \
	PORT_DEFN(NFlagHunt, ServerHidePlayer, NFlagHunt_ServerHidePlayerPass) \
	PORT_DEFN(NFlagHunt, GetRndFlag, NFlagHunt_GetRndFlag) \
	PORT_DEFN(NFlagHunt, BrowseAreas, NFlagHunt_BrowseAreas) \
	PORT_DEFN(NFlagHunt, CreateFlag, NFlagHunt_CreateFlag) \
	PORT_DEFN(NFlagHunt, TakeFlag, NFlagHunt_TakeFlag) \
	PORT_DEFN(NFlagHunt, OnUsrClient, NFlagHunt_OnUsrClient) \
	PORT_DEFN(NFlagHunt, ClientJoin, NFlagHunt_ClientJoin) \
	PORT_DEFN(NFlagHunt, ServerTakeFlag, NFlagHunt_ServerTakeFlag) \
	PORT_DEFN(NFlagHunt, CheckEndOfGame, NFlagHunt_CheckEndOfGame) \
	PORT_DEFN(NFlagHunt, GetGameDescriptor, NFlagHunt_GetGameDescriptor) \
	PORT_DEFN(NPostman, DeathFunc, NPostman_DeathFunc) \
	PORT_DEFN(NPostman, InitServerMod, NPostman_InitServerMod) \
	PORT_DEFN(NPostman, InitClientMod, NPostman_InitClientMod) \
	PORT_DEFN(NPostman, ShowHelp, NPostman_ShowHelp) \
	PORT_DEFN(NPostman, ServerHidePlayer, NPostman_ServerHidePlayer) \
	PORT_DEFN(NPostman, GetRndZone, NPostman_GetRndZone) \
	PORT_DEFN(NPostman, BrowseAreas, NPostman_BrowseAreas) \
	PORT_DEFN(NPostman, CreateFlag, NPostman_CreateFlag) \
	PORT_DEFN(NPostman, LinkFlagTo, NPostman_LinkFlagTo) \
	PORT_DEFN(NPostman, TryLinkFlagTo, NPostman_TryLinkFlagTo) \
	PORT_DEFN(NPostman, TakeFlag, NPostman_TakeFlag) \
	PORT_DEFN(NPostman, DropFlag, NPostman_DropFlag) \
	PORT_DEFN(NPostman, AbortZoneKeys, NPostman_AbortZoneKeys) \
	PORT_DEFN(NPostman, OnUsrClient, NPostman_OnUsrClient) \
	PORT_DEFN(NPostman, ClientJoin, NPostman_ClientJoin) \
	PORT_DEFN(NPostman, ServerTakeBase, NPostman_ServerTakeBase) \
	PORT_DEFN(NPostman, ServerDropFlag, NPostman_ServerDropFlag) \
	PORT_DEFN(NPostman, ServerTakeFlag, NPostman_ServerTakeFlag) \
	PORT_DEFN(NPostman, RespawnFlag, NPostman_RespawnFlag) \
	PORT_DEFN(NPostman, CheckEndOfGame, NPostman_CheckEndOfGame) \
	PORT_DEFN(NPostman, GetGameDescriptor, NPostman_GetGameDescriptor) \
	PORT_DEFN(NTeamPost, DeathFunc, NTeamPost_DeathFunc) \
	PORT_DEFN(NTeamPost, InitServerMod, NTeamPost_InitServerMod) \
	PORT_DEFN(NTeamPost, InitClientMod, NTeamPost_InitClientMod) \
	PORT_DEFN(NTeamPost, ShowHelp, NTeamPost_ShowHelp) \
	PORT_DEFN(NTeamPost, ServerHidePlayer, NTeamPost_ServerHidePlayer) \
	PORT_DEFN(NTeamPost, GetRndZone, NTeamPost_GetRndZone) \
	PORT_DEFN(NTeamPost, BrowseAreas, NTeamPost_BrowseAreas) \
	PORT_DEFN(NTeamPost, CreateFlag, NTeamPost_CreateFlag) \
	PORT_DEFN(NTeamPost, LinkFlagTo, NTeamPost_LinkFlagTo) \
	PORT_DEFN(NTeamPost, TryLinkFlagTo, NTeamPost_TryLinkFlagTo) \
	PORT_DEFN(NTeamPost, TakeFlag, NTeamPost_TakeFlag) \
	PORT_DEFN(NTeamPost, DropFlag, NTeamPost_DropFlag) \
	PORT_DEFN(NTeamPost, AbortZoneKeys, NTeamPost_AbortZoneKeys) \
	PORT_DEFN(NTeamPost, OnUsrClient, NTeamPost_OnUsrClient) \
	PORT_DEFN(NTeamPost, ClientJoin, NTeamPost_ClientJoin) \
	PORT_DEFN(NTeamPost, ServerTakeBase, NTeamPost_ServerTakeBase) \
	PORT_DEFN(NTeamPost, ServerDropFlag, NTeamPost_ServerDropFlag) \
	PORT_DEFN(NTeamPost, ServerTakeFlag, NTeamPost_ServerTakeFlag) \
	PORT_DEFN(NTeamPost, RespawnFlag, NTeamPost_RespawnFlag) \
	PORT_DEFN(NTeamPost, CheckEndOfGame, NTeamPost_CheckEndOfTeamGame) \
	PORT_DEFN(NTeamPost, GetGameDescriptor, NTeamPost_TeamDescriptor)

/* OutMap.c */
PORT_DECL(OutMap, BrowseSpawnPos) /* 0 */
PORT_DECL(OutMap, GetSpawnPos) /* 0 */
PORT_DECL(OutMap, GetFreeSpawnPos) /* 0 */
PORT_DECL(OutMap, GetFreeSpawnPosReal) /* 0 */
PORT_DECL(OutMap, GetNearestFreeSpawnPosReal) /* 1 */
PORT_DECL(OutMap, GetFreeSpawn) /* 0 */
PORT_DECL(OutMap, GetNearestFreeSpawnPos) /* ? */
PORT_DECL(OutMap, GetFarthestFreeSpawnPos) /* ? */
PORT_DECL(OutMap, CreatePlayers) /* 0 */
PORT_DECL(OutMap, InitOutMap) /* 0 */
PORT_DECL(OutMap, BrowseAtmospherePos) /* 0 */
PORT_DECL(OutMap, Init) /* 1 */
PORT_DECL(InTraffic, GetNam) /* 1 */
PORT_DECL(InTraffic, FakeWeap) /* 0 */
PORT_DECL(InTraffic, LoadFakeTraffic) /* 0 */
PORT_DECL(InTraffic, CreateList) /* 3 */
PORT_DECL(InTraffic, FakeTraffic) /* 0 */
PORT_DECL(InTraffic, SetSound) /* 1 */
#define PORTS_OutMap \
	PORT_DEFN(OutMap, BrowseSpawnPos, OutMap_BrowseSpawnPos) \
	PORT_DEFN(OutMap, GetSpawnPos, OutMap_GetSpawnPos) \
	PORT_DEFN(OutMap, GetFreeSpawnPos, OutMap_GetFreeSpawnPos) \
	PORT_DEFN(OutMap, GetFreeSpawnPosReal, OutMap_GetFreeSpawnPosReal) \
	PORT_DEFN(OutMap, GetNearestFreeSpawnPosReal, OutMap_GetNearestFreeSpawnPosReal) \
	PORT_DEFN(OutMap, GetFreeSpawn, OutMap_GetFreeSpawn) \
	PORT_DEFNK(OutMap, GetNearestFreeSpawnPos, OutMap_GetNearestFreeSpawnPos) \
	PORT_DEFNK(OutMap, GetFarthestFreeSpawnPos, OutMap_GetFarthestFreeSpawnPos) \
	PORT_DEFN(OutMap, CreatePlayers, OutMap_CreatePlayers) \
	PORT_DEFN(OutMap, InitOutMap, OutMap_InitOutMap) \
	PORT_DEFN(OutMap, BrowseAtmospherePos, OutMap_BrowseAtmospherePos) \
	PORT_DEFN(OutMap, Init, OutMap_Init) \
	PORT_DEFN(InTraffic, GetNam, It_GetNamPy) \
	PORT_DEFN(InTraffic, FakeWeap, InTraffic_FakeWeap) \
	PORT_DEFN(InTraffic, LoadFakeTraffic, InTraffic_LoadFakeTraffic) \
	PORT_DEFN(InTraffic, CreateList, It_CreateListPy) \
	PORT_DEFN(InTraffic, FakeTraffic, InTraffic_FakeTraffic) \
	PORT_DEFN(InTraffic, SetSound, It_SetSoundPy)

/* Parking.c */
PORT_DECL(Parking, RestoreDeathPlayerShip) /* 0 */
PORT_DECL(Parking, SetInvunerable) /* 1 */
PORT_DECL(Parking, CharOnShip) /* 1 */
PORT_DECL(Parking, UseShip) /* ? */
PORT_DECL(Parking, PlayVoiceChannel) /* 1 */
PORT_DECL(Parking, ExecSwitchVehicleMissionCallback) /* 0 */
PORT_DECL(Parking, EndPathCam) /* 1 */
PORT_DECL(Parking, SetCameraStart) /* ? */
PORT_DECL(Parking, isValidToPark) /* 1 */
PORT_DECL(Parking, OnPark) /* 2 */
PORT_DECL(Parking, SetDtritusPlate) /* 2 */
PORT_DECL(Parking, CreateDtritusPlate) /* 0 */
PORT_DECL(Parking, CreatePlayerShip) /* 1 */
PORT_DECL(Parking, ShowChar) /* 2 */
PORT_DECL(Parking, EndJunkerPathCam) /* 1 */
PORT_DECL(Parking, JunkerPark) /* 2 */
PORT_DECL(Parking, OnParkingOccupied) /* 5 */
PORT_DECL(Parking, OnParkedOut) /* 5 */
PORT_DECL(Parking, CreateBox) /* 4 */
PORT_DECL(Parking, SetShipRadius) /* 2 */
PORT_DECL(Parking, SetOnFloor) /* 1 */
PORT_DECL(Parking, SetShipAsParked) /* 1 */
PORT_DECL(Parking, ParkedDeat) /* 2 */
PORT_DECL(Parking, BrowseItemPos) /* 0 */
PORT_DECL(Parking, CreateLonelyMercParkings) /* 0 */
PORT_DECL(Parking, CreateParkedShip) /* ? */
PORT_DECL(Parking, CheckWeapons) /* 1 */
PORT_DECL(Parking, BrowseParkedPos) /* 0 */
PORT_DECL(Parking, ChangeLevel) /* 2 */
PORT_DECL(Parking, BrowseParkingAreas) /* 0 */
PORT_DECL(Parking, BrowseSwitchLevel) /* 0 */
PORT_DECL(Parking, ShowNextMap) /* 3 */
PORT_DECL(Parking, DoorInitializator) /* 1 */
PORT_DECL(Parking, Init) /* 0 */
PORT_DECL(Accelerator, CreateAccelerator) /* ? */
PORT_DECL(Accelerator, Init) /* 0 */
#define PORTS_Parking \
	PORT_DEFN(Parking, RestoreDeathPlayerShip, Parking_RestoreDeathPlayerShip) \
	PORT_DEFN(Parking, SetInvunerable, Parking_SetInvunerable) \
	PORT_DEFN(Parking, CharOnShip, Parking_CharOnShip) \
	PORT_DEFNK(Parking, UseShip, Parking_UseShip) \
	PORT_DEFN(Parking, PlayVoiceChannel, Parking_PlayVoiceChannel) \
	PORT_DEFN(Parking, ExecSwitchVehicleMissionCallback, Parking_ExecSwitchVehicleMissionCallback) \
	PORT_DEFN(Parking, EndPathCam, Parking_EndPathCam) \
	PORT_DEFNK(Parking, SetCameraStart, Parking_SetCameraStart) \
	PORT_DEFN(Parking, isValidToPark, Parking_isValidToPark) \
	PORT_DEFN(Parking, OnPark, Parking_OnPark) \
	PORT_DEFN(Parking, SetDtritusPlate, Parking_SetDtritusPlate) \
	PORT_DEFN(Parking, CreateDtritusPlate, Parking_CreateDtritusPlate) \
	PORT_DEFN(Parking, CreatePlayerShip, Parking_CreatePlayerShip) \
	PORT_DEFN(Parking, ShowChar, Parking_ShowChar) \
	PORT_DEFN(Parking, EndJunkerPathCam, Parking_EndJunkerPathCam) \
	PORT_DEFN(Parking, JunkerPark, Parking_JunkerPark) \
	PORT_DEFN(Parking, OnParkingOccupied, Parking_OnParkingOccupied) \
	PORT_DEFN(Parking, OnParkedOut, Parking_OnParkedOut) \
	PORT_DEFN(Parking, CreateBox, Parking_CreateBox) \
	PORT_DEFN(Parking, SetShipRadius, Parking_SetShipRadius) \
	PORT_DEFN(Parking, SetOnFloor, Parking_SetOnFloor) \
	PORT_DEFN(Parking, SetShipAsParked, Parking_SetShipAsParked) \
	PORT_DEFN(Parking, ParkedDeat, Parking_ParkedDeat) \
	PORT_DEFN(Parking, BrowseItemPos, Parking_BrowseItemPos) \
	PORT_DEFN(Parking, CreateLonelyMercParkings, Parking_CreateLonelyMercParkings) \
	PORT_DEFNK(Parking, CreateParkedShip, Parking_CreateParkedShip) \
	PORT_DEFN(Parking, CheckWeapons, Parking_CheckWeapons) \
	PORT_DEFN(Parking, BrowseParkedPos, Parking_BrowseParkedPos) \
	PORT_DEFN(Parking, ChangeLevel, Parking_ChangeLevel) \
	PORT_DEFN(Parking, BrowseParkingAreas, Parking_BrowseParkingAreas) \
	PORT_DEFN(Parking, BrowseSwitchLevel, Parking_BrowseSwitchLevel) \
	PORT_DEFN(Parking, ShowNextMap, Parking_ShowNextMap) \
	PORT_DEFN(Parking, DoorInitializator, Parking_DoorInitializator) \
	PORT_DEFN(Parking, Init, Parking_Init) \
	PORT_DEFNK(Accelerator, CreateAccelerator, Accelerator_CreateAccelerator) \
	PORT_DEFN(Accelerator, Init, Accelerator_Init)

/* PhoneCab.c */
PORT_DECL(PhoneCab, ForceResetPhoneCab) /* ? */
PORT_DECL(PhoneCab, RevengeSpeech) /* 4 */
PORT_DECL(PhoneCab, SetRevengeOptions) /* 1 */
PORT_DECL(PhoneCab, GetNearestPhoneCab) /* 1 */
PORT_DECL(PhoneCab, CreatePhoneCab) /* 3 */
PORT_DECL(PhoneCab, CreateAudioCom) /* 3 */
PORT_DECL(PhoneCab, DefUseAudioCom) /* 4 */
PORT_DECL(PhoneCab, UseAudioCom) /* 4 */
PORT_DECL(PhoneCab, SpawnChar) /* 2 */
PORT_DECL(PhoneCab, CreatePhoneCabClass) /* ? */
PORT_DECL(PhoneCab, CreateAudioComClass) /* ? */
PORT_DECL(PhoneCab, Init) /* ? */
PORT_DECL(PhoneCab, GetFree) /* ? */
PORT_DECL(DTritusDesktop, ForceResetDTritusDesktop) /* ? */
PORT_DECL(DTritusDesktop, SpawnChar) /* 1 */
PORT_DECL(DTritusDesktop, CreateDTritusDesktopClass) /* ? */
PORT_DECL(DTritusDesktop, CreateDTritusDesktop) /* 3 */
PORT_DECL(DTritusDesktop, DefUseDTritusDesktop) /* 4 */
PORT_DECL(DTritusDesktop, UseDTritusDesktop) /* 4 */
PORT_DECL(DTritusDesktop, CreatePressDesktopClass) /* 1 */
PORT_DECL(DTritusDesktop, CreatePressDesktop) /* 1 */
PORT_DECL(DTritusDesktop, Init) /* ? */
PORT_DECL(DTritusDesktop, InitNPCS) /* ? */
#define PORTS_PhoneCab \
	PORT_DEFN(PhoneCab, ForceResetPhoneCab, PhoneCab_ForceResetPhoneCab) \
	PORT_DEFN(PhoneCab, RevengeSpeech, PhoneCab_RevengeSpeech) \
	PORT_DEFN(PhoneCab, SetRevengeOptions, PhoneCab_SetRevengeOptions) \
	PORT_DEFN(PhoneCab, GetNearestPhoneCab, PhoneCab_GetNearestPhoneCab) \
	PORT_DEFN(PhoneCab, CreatePhoneCab, PhoneCab_CreatePhoneCab) \
	PORT_DEFN(PhoneCab, CreateAudioCom, PhoneCab_CreateAudioCom) \
	PORT_DEFN(PhoneCab, DefUseAudioCom, PhoneCab_DefUseAudioCom) \
	PORT_DEFN(PhoneCab, UseAudioCom, PhoneCab_UseAudioCom) \
	PORT_DEFN(PhoneCab, SpawnChar, PhoneCab_SpawnChar) \
	PORT_DEFN(PhoneCab, CreatePhoneCabClass, PhoneCab_CreatePhoneCabClass) \
	PORT_DEFN(PhoneCab, CreateAudioComClass, PhoneCab_CreateAudioComClass) \
	PORT_DEFN(PhoneCab, Init, PhoneCab_Init) \
	PORT_DEFN(PhoneCab, GetFree, PhoneCab_GetFree) \
	PORT_DEFN(DTritusDesktop, ForceResetDTritusDesktop, DTritus_ForceResetDTritusDesktop) \
	PORT_DEFN(DTritusDesktop, SpawnChar, DTritus_SpawnChar) \
	PORT_DEFN(DTritusDesktop, CreateDTritusDesktopClass, DTritus_CreateDTritusDesktopClass) \
	PORT_DEFN(DTritusDesktop, CreateDTritusDesktop, DTritus_CreateDTritusDesktop) \
	PORT_DEFN(DTritusDesktop, DefUseDTritusDesktop, DTritus_DefUseDTritusDesktop) \
	PORT_DEFN(DTritusDesktop, UseDTritusDesktop, DTritus_UseDTritusDesktop) \
	PORT_DEFN(DTritusDesktop, CreatePressDesktopClass, DTritus_CreatePressDesktopClass) \
	PORT_DEFN(DTritusDesktop, CreatePressDesktop, DTritus_CreatePressDesktop) \
	PORT_DEFN(DTritusDesktop, Init, DTritus_Init) \
	PORT_DEFN(DTritusDesktop, InitNPCS, DTritus_InitNPCS)

/* PyInterface/SAIPy.c */
PORT_DECL(SAI, AnalizeMap) /* 1 */
PORT_DECL(SAI, AnalizeGraph) /* ? */
PORT_DECL(SAI, AnalizeGraph2D) /* ? */
PORT_DECL(SAI, AnalizeTraffic) /* 0 */
PORT_DECL(SAI, BuildGraph) /* 4 */
PORT_DECL(SAI, BuildGraph2D) /* 4 */
PORT_DECL(SAI, SetStateVehicle) /* ? */
PORT_DECL(SAI, SetStateChar) /* ? */
PORT_DECL(SAI, GetStateVehicle) /* 1 */
PORT_DECL(SAI, GetStateChar) /* 1 */
PORT_DECL(SAI, EnableAIVehicle) /* ? */
PORT_DECL(SAI, EnableAIChar) /* 3 */
PORT_DECL(SAI, IniAI) /* 2 */
PORT_DECL(SAI, GetNextRacePoint) /* 3 */
PORT_DECL(SAI, GetNearestItemLife) /* 1 */
PORT_DECL(SAI, GetRandomVisibilityPoint) /* ? */
PORT_DECL(SAI, GetReposCharPos) /* ? */
PORT_DECL(SAI, InitVehicleRace) /* ? */
PORT_DECL(SAI, AddVehicleRace) /* 1 */
PORT_DECL(SAI, SetRotStaticObj) /* 3 */
PORT_DECL(SAI, SetInertia) /* 2 */
PORT_DECL(SAI, GetOD) /* ? */
#define PORTS_Engine_SAIPy \
	PORT_DEFND(SAI, AnalizeMap, SAI_AnalyzeMap, str_PyDoc_SAI_AnalyzeMap) \
	PORT_DEFND(SAI, AnalizeGraph, SAI_AnalyzeGraph, str_PyDoc_SAI_AnalyzeGraph) \
	PORT_DEFND(SAI, AnalizeGraph2D, SAI_AnalyzeGraph2D, str_PyDoc_SAI_AnalyzeGraph2D) \
	PORT_DEFND(SAI, AnalizeTraffic, SAI_AnalyzeTraffic, str_PyDoc_SAI_AnalyzeTraffic) \
	PORT_DEFND(SAI, BuildGraph, SAI_BuildGraph, str_PyDoc_SAI_BuildGraph) \
	PORT_DEFND(SAI, BuildGraph2D, SAI_BuildGraph2D, str_PyDoc_SAI_BuildGraph2D) \
	PORT_DEFND(SAI, SetStateVehicle, SAI_SetStateVehicle, str_PyDoc_SAI_SetStateVehicle) \
	PORT_DEFND(SAI, SetStateChar, SAI_SetStateChar, str_PyDoc_SAI_SetStateChar) \
	PORT_DEFND(SAI, GetStateVehicle, SAI_GetStateVehicle, str_PyDoc_SAI_GetStateVehicle) \
	PORT_DEFND(SAI, GetStateChar, SAI_GetStateChar, str_PyDoc_SAI_GetStateChar) \
	PORT_DEFND(SAI, EnableAIVehicle, SAI_EnableAIVehicle, str_PyDoc_SAI_EnableAIVehicle) \
	PORT_DEFND(SAI, EnableAIChar, SAI_EnableAIChar, str_PyDoc_SAI_EnableAIChar) \
	PORT_DEFND(SAI, IniAI, SAI_IniAI, str_PyDoc_SAI_IniAI) \
	PORT_DEFND(SAI, GetNextRacePoint, SAI_GetNextRacePoint, str_PyDoc_SAI_GetNextRacePoint) \
	PORT_DEFND(SAI, GetNearestItemLife, SAI_GetNearestItemLife, str_PyDoc_SAI_GetNearestItemLife) \
	PORT_DEFND(SAI, GetRandomVisibilityPoint, SAI_GetRandomVisibilityPoint, str_PyDoc_SAI_GetRandomVisibilityPoint) \
	PORT_DEFND(SAI, GetReposCharPos, SAI_GetReposCharPos, str_PyDoc_SAI_GetReposCharPos) \
	PORT_DEFND(SAI, InitVehicleRace, SAI_InitVehicleRace, str_PyDoc_SAI_InitVehicleRace) \
	PORT_DEFND(SAI, AddVehicleRace, SAI_AddVehicleRace, str_PyDoc_SAI_AddVehicleRace) \
	PORT_DEFND(SAI, SetRotStaticObj, SAI_SetRotStaticObj, str_PyDoc_SAI_SetRotStaticObj) \
	PORT_DEFND(SAI, SetInertia, SAI_SetInertia, str_PyDoc_SAI_SetInertia) \
	PORT_DEFND(SAI, GetOD, SAI_GetOD, "")

/* PyInterface/SActPy.c */
PORT_DECL(SAct, CreateClass) /* 1 */
PORT_DECL(SAct, DelClass) /* 1 */
PORT_DECL(SAct, GetClass) /* 1 */
PORT_DECL(SAct, SetCls) /* ? */
PORT_DECL(SAct, GetCls) /* ? */
PORT_DECL(SAct, CreateAction) /* 1 */
PORT_DECL(SAct, GetAction) /* 1 */
PORT_DECL(SAct, SetAct) /* ? */
PORT_DECL(SAct, GetAct) /* ? */
PORT_DECL(SAct, DumpEvents) /* 1 */
#define PORTS_Engine_SActPy \
	PORT_DEFND(SAct, CreateClass, SAct_CreateClass, str_PyDoc_SAct_CreateClass) \
	PORT_DEFND(SAct, DelClass, SAct_DelClass, str_PyDoc_SAct_DelClass) \
	PORT_DEFND(SAct, GetClass, SAct_GetClass, str_PyDoc_SAct_GetClass) \
	PORT_DEFND(SAct, SetCls, SAct_SetCls, str_PyDoc_SAct_SetCls) \
	PORT_DEFND(SAct, GetCls, SAct_GetCls, str_PyDoc_SAct_GetCls) \
	PORT_DEFND(SAct, CreateAction, SAct_CreateAction, str_PyDoc_SAct_CreateAction) \
	PORT_DEFND(SAct, GetAction, SAct_GetAction, str_PyDoc_SAct_GetAction) \
	PORT_DEFND(SAct, SetAct, SAct_SetAct, str_PyDoc_SAct_SetAct) \
	PORT_DEFND(SAct, GetAct, SAct_GetAct, str_PyDoc_SAct_GetAct) \
	PORT_DEFND(SAct, DumpEvents, SAct_DumpEvents, "DumpEvents(file) : ScrapC only, every class, action and event into the file")

/* PyInterface/SFXPy.c */
PORT_DECL(SFX, ShipExplosion) /* 6 */
PORT_DECL(SFX, CharacterConversor) /* 3 */
PORT_DECL(SFX, CharacterConversion) /* 3 */
PORT_DECL(SFX, BishopSellLife) /* 2 */
PORT_DECL(SFX, MoneyTransfer) /* 3 */
PORT_DECL(SFX, EmbeddedSet) /* 2 */
#define PORTS_Engine_SFXPy \
	PORT_DEFND(SFX, ShipExplosion, SFX_ShipExplosion, str_PyDoc_SFX_ShipExplosion) \
	PORT_DEFND(SFX, CharacterConversor, SFX_CharacterConversor, str_PyDoc_SFX_CharacterConversor) \
	PORT_DEFND(SFX, CharacterConversion, SFX_CharacterConversion, str_PyDoc_SFX_CharacterConversion) \
	PORT_DEFND(SFX, BishopSellLife, SFX_BishopSellLife, str_PyDoc_SFX_BishopSellLife) \
	PORT_DEFND(SFX, MoneyTransfer, SFX_MoneyTransfer, str_PyDoc_SFX_MoneyTransfer) \
	PORT_DEFND(SFX, EmbeddedSet, SFX_EmbeddedSet, str_PyDoc_SFX_EmbeddedSet)

/* PyInterface/SInputPy.c */
PORT_DECL(SInput, SetActionSet) /* 1 */
PORT_DECL(SInput, GetActionSet) /* 0 */
PORT_DECL(SInput, AssingEntry) /* 5 */
PORT_DECL(SInput, GetEntry) /* 3 */
PORT_DECL(SInput, ListenToDefine) /* 4 */
PORT_DECL(SInput, AbortListenToDefine) /* 0 */
PORT_DECL(SInput, GetDefinedList) /* ? */
PORT_DECL(SInput, GetDefinedEntry) /* ? */
PORT_DECL(SInput, ClearDefinedList) /* ? */
PORT_DECL(SInput, ResetToDefault) /* 0 */
PORT_DECL(SInput, SetString) /* 2 */
PORT_DECL(SInput, GetString) /* 1 */
PORT_DECL(SInput, GetCursorChar) /* 1 */
PORT_DECL(SInput, GetVirtualKeyboard) /* 0 */
PORT_DECL(SInput, SetVirtualKeyboard) /* 1 */
PORT_DECL(SInput, SetInputFunc) /* 2 */
PORT_DECL(SInput, Bind) /* 4 */
PORT_DECL(SInput, Rumble) /* 4 */
PORT_DECL(SInput, ResetToSplit) /* 0 */
PORT_DECL(SInput, CheckPadButton) /* 2 */
#define PORTS_Engine_SInputPy \
	PORT_DEFND(SInput, SetActionSet, SInput_SetActionSet, str_PyDoc_SInput_SetActionSet) \
	PORT_DEFND(SInput, GetActionSet, SInput_GetActionSet, str_PyDoc_SInput_GetActionSet) \
	PORT_DEFND(SInput, AssingEntry, SInput_AssingEntry, str_PyDoc_SInput_AssingEntry) \
	PORT_DEFND(SInput, GetEntry, SInput_GetEntry, str_PyDoc_SInput_GetEntry) \
	PORT_DEFND(SInput, ListenToDefine, SInput_ListenToDefine, str_PyDoc_SInput_ListenToDefine) \
	PORT_DEFND(SInput, AbortListenToDefine, SInput_AbortListenToDefine, str_PyDoc_SInput_AbortListenToDefine) \
	PORT_DEFND(SInput, GetDefinedList, SInput_GetDefinedList, str_PyDoc_SInput_GetDefinedList) \
	PORT_DEFND(SInput, GetDefinedEntry, SInput_GetDefinedEntry, str_PyDoc_SInput_GetDefinedEntry) \
	PORT_DEFND(SInput, ClearDefinedList, SInput_ClearDefinedList, str_PyDoc_SInput_ClearDefinedList) \
	PORT_DEFND(SInput, ResetToDefault, SInput_ResetToDefault, str_PyDoc_SInput_ResetToDefault) \
	PORT_DEFND(SInput, SetString, SInput_SetString, str_PyDoc_SInput_SetString) \
	PORT_DEFND(SInput, GetString, SInput_GetString, str_PyDoc_SInput_GetString) \
	PORT_DEFND(SInput, GetCursorChar, SInput_GetCursorChar, str_PyDoc_SInput_GetCursorChar) \
	PORT_DEFND(SInput, GetVirtualKeyboard, SInput_GetVirtualKeyboard, str_PyDoc_SInput_GetVirtualKeyboard) \
	PORT_DEFND(SInput, SetVirtualKeyboard, SInput_SetVirtualKeyboard, str_PyDoc_SInput_SetVirtualKeyboard) \
	PORT_DEFND(SInput, SetInputFunc, SInput_SetInputFunc, str_PyDoc_SInput_SetInputFunc) \
	PORT_DEFND(SInput, Bind, SInput_Bind, str_PyDoc_SInput_Bind) \
	PORT_DEFND(SInput, Rumble, SInput_Rumble, str_PyDoc_SInput_Rumble) \
	PORT_DEFND(SInput, ResetToSplit, SInput_ResetToSplit, str_PyDoc_SInput_ResetToSplit) \
	PORT_DEFND(SInput, CheckPadButton, SInput_CheckPadButton, str_PyDoc_SInput_CheckPadButton)

/* PyInterface/SLogic.c */
PORT_DECL(SLogic, SendSentinelToWatch) /* 1 */
PORT_DECL(SLogic, GearAttack) /* 1 */
PORT_DECL(SLogic, SetCharState) /* 3 */
PORT_DECL(SLogic, IsEnemyActive) /* 0 */
PORT_DECL(SLogic, GetNearestShip) /* ? */
PORT_DECL(SLogic, SetOnFloor) /* 1 */
PORT_DECL(SLogic, Flash) /* 2 */
PORT_DECL(SLogic, SetShipToRegenerate) /* 3 */
PORT_DECL(SLogic, SetDominationZones) /* 1 */
PORT_DECL(SLogic, ChangeZoneState) /* 2 */
PORT_DECL(SLogic, ChangeBatonState) /* 2 */
PORT_DECL(SLogic, UpdateTauntEndTime) /* ? */
#define PORTS_Engine_SLogic \
	PORT_DEFND(SLogic, SendSentinelToWatch, SLogic_SendSentinelToWatch, str_PyDoc_SLogic_SendSentinelToWatch) \
	PORT_DEFND(SLogic, GearAttack, SLogic_GearAttack, str_PyDoc_SLogic_GearAttack) \
	PORT_DEFND(SLogic, SetCharState, SLogic_SetCharState, str_PyDoc_SLogic_SetCharState) \
	PORT_DEFND(SLogic, IsEnemyActive, SLogic_IsEnemyActive, str_PyDoc_SLogic_IsEnemyActive) \
	PORT_DEFND(SLogic, GetNearestShip, SLogic_GetNearestShip, str_PyDoc_SLogic_GetNearestShip) \
	PORT_DEFND(SLogic, SetOnFloor, SLogic_SetOnFloor, str_PyDoc_SLogic_SetOnFloor) \
	PORT_DEFND(SLogic, Flash, SLogic_Flash, str_PyDoc_SLogic_Flash) \
	PORT_DEFND(SLogic, SetShipToRegenerate, SLogic_SetShipToRegenerate, str_PyDoc_SLogic_SetShipToRegenerate) \
	PORT_DEFND(SLogic, SetDominationZones, SLogic_SetDominationZones, str_PyDoc_SLogic_SetDominationZones) \
	PORT_DEFND(SLogic, ChangeZoneState, SLogic_ChangeZoneState, str_PyDoc_SLogic_ChangeZoneState) \
	PORT_DEFND(SLogic, ChangeBatonState, SLogic_ChangeBatonState, str_PyDoc_SLogic_ChangeBatonState) \
	PORT_DEFND(SLogic, UpdateTauntEndTime, SLogic_UpdateTauntEndTime, str_PyDoc_SLogic_UpdateTauntEndTime)

/* PyInterface/SNetPy.c */
PORT_DECL(SNet, InitServer) /* 3 */
PORT_DECL(SNet, CloseServer) /* 1 */
PORT_DECL(SNet, ServerChangeLevel) /* 1 */
PORT_DECL(SNet, IsServer) /* 0 */
PORT_DECL(SNet, IsDedicated) /* 0 */
PORT_DECL(SNet, GetServerData) /* 0 */
PORT_DECL(SNet, AddResource) /* 1 */
PORT_DECL(SNet, ModifyUsrData) /* 1 */
PORT_DECL(SNet, GetBotName) /* 0 */
PORT_DECL(SNet, GetObjName) /* 0 */
PORT_DECL(SNet, Kick) /* 1 */
PORT_DECL(SNet, InitClient) /* 2 */
PORT_DECL(SNet, CloseClient) /* ? */
PORT_DECL(SNet, IsClient) /* 0 */
PORT_DECL(SNet, GetClientData) /* 0 */
PORT_DECL(SNet, GetMyClientShip) /* 0 */
PORT_DECL(SNet, InitBrowser) /* ? */
PORT_DECL(SNet, DoneBrowser) /* 0 */
PORT_DECL(SNet, SendUsrString) /* 2 */
PORT_DECL(SNet, SendChatString) /* 2 */
PORT_DECL(SNet, SendCtrlString) /* 1 */
PORT_DECL(SNet, SendMasterString) /* 1 */
PORT_DECL(SNet, IsMaster) /* 0 */
PORT_DECL(SNet, PingInetSvrs) /* 0 */
PORT_DECL(SNet, GetPing) /* 1 */
#define PORTS_Engine_SNetPy \
	PORT_DEFND(SNet, InitServer, SNet_InitServer, str_PyDoc_SNet_InitServer) \
	PORT_DEFND(SNet, CloseServer, SNet_CloseServer, str_PyDoc_SNet_CloseServer) \
	PORT_DEFND(SNet, ServerChangeLevel, SNet_ServerChangeLevel, str_PyDoc_SNet_ServerChangeLevel) \
	PORT_DEFND(SNet, IsServer, SNet_IsServer, str_PyDoc_SNet_IsServer) \
	PORT_DEFND(SNet, IsDedicated, SNet_IsDedicated, str_PyDoc_SNet_IsDedicated) \
	PORT_DEFND(SNet, GetServerData, SNet_GetServerData, str_PyDoc_SNet_GetServerData) \
	PORT_DEFND(SNet, AddResource, SNet_AddResource, str_PyDoc_SNet_AddResource) \
	PORT_DEFND(SNet, ModifyUsrData, SNet_ModifyUsrData, str_PyDoc_SNet_ModifyUsrData) \
	PORT_DEFND(SNet, GetBotName, SNet_GetBotName, str_PyDoc_SNet_GetBotName) \
	PORT_DEFND(SNet, GetObjName, SNet_GetObjName, str_PyDoc_SNet_GetObjName) \
	PORT_DEFND(SNet, Kick, SNet_Kick, str_PyDoc_SNet_Kick) \
	PORT_DEFND(SNet, InitClient, SNet_InitClient, str_PyDoc_SNet_InitClient) \
	PORT_DEFND(SNet, CloseClient, SNet_CloseClient, str_PyDoc_SNet_CloseClient) \
	PORT_DEFND(SNet, IsClient, SNet_IsClient, str_PyDoc_SNet_IsClient) \
	PORT_DEFND(SNet, GetClientData, SNet_GetClientData, str_PyDoc_SNet_GetClientData) \
	PORT_DEFND(SNet, GetMyClientShip, SNet_GetMyClientShip, str_PyDoc_SNet_GetMyClientShip) \
	PORT_DEFND(SNet, InitBrowser, SNet_InitBrowser, str_PyDoc_SNet_InitBrowser) \
	PORT_DEFND(SNet, DoneBrowser, SNet_DoneBrowser, str_PyDoc_SNet_DoneBrowser) \
	PORT_DEFND(SNet, SendUsrString, SNet_SendUsrString, str_PyDoc_SNet_SendUsrString) \
	PORT_DEFND(SNet, SendChatString, SNet_SendChatString, str_PyDoc_SNet_SendChatString) \
	PORT_DEFND(SNet, SendCtrlString, SNet_SendCtrlString, str_PyDoc_SNet_SendCtrlString) \
	PORT_DEFND(SNet, SendMasterString, SNet_SendMasterString, str_PyDoc_SNet_SendMasterString) \
	PORT_DEFND(SNet, IsMaster, SNet_IsMaster, str_PyDoc_SNet_IsMaster) \
	PORT_DEFND(SNet, PingInetSvrs, SNet_PingInetSvrs, str_PyDoc_SNet_PingInetSvrs) \
	PORT_DEFND(SNet, GetPing, SNet_GetPing, str_PyDoc_SNet_GetPing)

/* PyInterface/SScorerPy.c */
PORT_DECL(SScorer, Add) /* ? */
PORT_DECL(SScorer, Clear) /* 1 */
PORT_DECL(SScorer, Set) /* 4 */
PORT_DECL(SScorer, Get) /* 3 */
PORT_DECL(SScorer, Show) /* 1 */
PORT_DECL(SScorer, Hide) /* 1 */
PORT_DECL(SScorer, SetCursor) /* 2 */
PORT_DECL(SScorer, SetDefault) /* 2 */
PORT_DECL(SScorer, GetDefault) /* 1 */
PORT_DECL(SScorer, GetActual) /* 1 */
PORT_DECL(SScorer, SetOnCancel) /* 2 */
PORT_DECL(SScorer, SetOnNext) /* 2 */
PORT_DECL(SScorer, SetOnPrev) /* 2 */
PORT_DECL(SScorer, SetOnSpecialHint) /* 3 */
PORT_DECL(SScorer, AddModel) /* ? */
PORT_DECL(SScorer, SetMPFunc) /* 2 */
PORT_DECL(SScorer, PreloadTexture) /* ? */
PORT_DECL(SScorer, AddChatMsg) /* 4 */
PORT_DECL(SScorer, SetConsole) /* 1 */
PORT_DECL(SScorer, SetSpeechText) /* 5 */
PORT_DECL(SScorer, SetSpeechCallback) /* ? */
PORT_DECL(SScorer, CancelSpeech) /* 0 */
PORT_DECL(SScorer, SetHeadMonitor) /* 3 */
PORT_DECL(SScorer, Fade) /* 4 */
PORT_DECL(SScorer, SetCinema) /* 3 */
PORT_DECL(SScorer, SetMsgText) /* 2 */
PORT_DECL(SScorer, SetLabelText) /* 2 */
PORT_DECL(SScorer, Get2DPos) /* 1 */
PORT_DECL(SScorer, GetTextArea) /* 2 */
PORT_DECL(SScorer, GetMenuAccept) /* 1 */
PORT_DECL(SScorer, SetMarkerSprite) /* 2 */
PORT_DECL(SScorer, GetItem) /* 2 */
PORT_DECL(SScorer, New) /* ? */
PORT_DECL(SScorer, Items) /* 1 */
PORT_DECL(SScorer, Accept) /* 2 */
#define PORTS_Engine_SScorerPy \
	PORT_DEFND(SScorer, Add, SScorer_Add, str_PyDoc_SScorer_Add) \
	PORT_DEFND(SScorer, Clear, SScorer_Clear, str_PyDoc_SScorer_Clear) \
	PORT_DEFND(SScorer, Set, SScorer_Set, str_PyDoc_SScorer_Set) \
	PORT_DEFND(SScorer, Get, SScorer_Get, str_PyDoc_SScorer_Get) \
	PORT_DEFND(SScorer, Show, SScorer_Show, str_PyDoc_SScorer_Show) \
	PORT_DEFND(SScorer, Hide, SScorer_Hide, str_PyDoc_SScorer_Hide) \
	PORT_DEFND(SScorer, SetCursor, SScorer_SetCursor, str_PyDoc_SScorer_SetCursor) \
	PORT_DEFND(SScorer, SetDefault, SScorer_SetDefault, str_PyDoc_SScorer_SetDefault) \
	PORT_DEFND(SScorer, GetDefault, SScorer_GetDefault, str_PyDoc_SScorer_GetDefault) \
	PORT_DEFND(SScorer, GetActual, SScorer_GetCurrent, str_PyDoc_SScorer_GetCurrent) \
	PORT_DEFND(SScorer, SetOnCancel, SScorer_SetOnCancel, str_PyDoc_SScorer_SetOnCancel) \
	PORT_DEFND(SScorer, SetOnNext, SScorer_SetOnNext, str_PyDoc_SScorer_SetOnNext) \
	PORT_DEFND(SScorer, SetOnPrev, SScorer_SetOnPrev, str_PyDoc_SScorer_SetOnPrev) \
	PORT_DEFND(SScorer, SetOnSpecialHint, SScorer_SetOnSpecialHint, str_PyDoc_SScorer_SetOnSpecialHint) \
	PORT_DEFND(SScorer, AddModel, SScorer_AddModel, str_PyDoc_SScorer_AddModel) \
	PORT_DEFND(SScorer, SetMPFunc, SScorer_SetMPFunc, str_PyDoc_SScorer_SetMPFunc) \
	PORT_DEFND(SScorer, PreloadTexture, SScorer_PreloadTexture, str_PyDoc_SScorer_PreloadTexture) \
	PORT_DEFND(SScorer, AddChatMsg, SScorer_AddChatMsg, str_PyDoc_SScorer_AddChatMsg) \
	PORT_DEFND(SScorer, SetConsole, SScorer_SetConsole, str_PyDoc_SScorer_SetConsole) \
	PORT_DEFND(SScorer, SetSpeechText, SScorer_SetSpeechText, str_PyDoc_SScorer_SetSpeechText) \
	PORT_DEFND(SScorer, SetSpeechCallback, SScorer_SetSpeechCallback, str_PyDoc_SScorer_SetSpeechCallback) \
	PORT_DEFND(SScorer, CancelSpeech, SScorer_CancelSpeech, str_PyDoc_SScorer_CancelSpeech) \
	PORT_DEFND(SScorer, SetHeadMonitor, SScorer_SetHeadMonitor, str_PyDoc_SScorer_SetHeadMonitor) \
	PORT_DEFND(SScorer, Fade, SScorer_Fade, str_PyDoc_SScorer_Fade) \
	PORT_DEFND(SScorer, SetCinema, SScorer_SetCinema, str_PyDoc_SScorer_SetCinema) \
	PORT_DEFND(SScorer, SetMsgText, SScorer_SetMsgText, str_PyDoc_SScorer_SetMsgText) \
	PORT_DEFND(SScorer, SetLabelText, SScorer_SetLabelText, str_PyDoc_SScorer_SetLabelText) \
	PORT_DEFND(SScorer, Get2DPos, SScorer_Get2DPos, str_PyDoc_SScorer_Get2DPos) \
	PORT_DEFND(SScorer, GetTextArea, SScorer_GetTextArea, str_PyDoc_SScorer_GetTextArea) \
	PORT_DEFND(SScorer, GetMenuAccept, SScorer_GetMenuAccept, str_PyDoc_SScorer_GetMenuAccept) \
	PORT_DEFND(SScorer, SetMarkerSprite, SScorer_SetMarkerSprite, str_PyDoc_SScorer_SetMarkerSprite) \
	PORT_DEFND(SScorer, GetItem, SScorer_GetItem, str_PyDoc_SScorer_GetItem) \
	PORT_DEFND(SScorer, New, SScorer_New, str_PyDoc_SScorer_New) \
	PORT_DEFND(SScorer, Items, SScorer_Items, "Items(playernumber) : the item names in draw order (ScrapC)") \
	PORT_DEFND(SScorer, Accept, SScorer_Accept, "Accept(playernumber,ItemName) : runs the OnAccept of the item (ScrapC)")

/* PyInterface/SSoundPy.c */
PORT_DECL(SSound, LoadSound) /* 1 */
PORT_DECL(SSound, PlaySound) /* ? */
PORT_DECL(SSound, Play) /* ? */
PORT_DECL(SSound, StopAllSounds) /* 0 */
PORT_DECL(SSound, SetPS) /* 3 */
PORT_DECL(SSound, GetPS) /* 2 */
PORT_DECL(SSound, UsePS) /* 1 */
PORT_DECL(SSound, CreatePS) /* 1 */
PORT_DECL(SSound, SetMusic) /* ? */
PORT_DECL(SSound, SetMusicVolume) /* 1 */
PORT_DECL(SSound, OpenVoice) /* ? */
PORT_DECL(SSound, PlayVoice) /* ? */
PORT_DECL(SSound, StopVoice) /* 1 */
PORT_DECL(SSound, VoiceRemain) /* 2 */
PORT_DECL(SSound, SetVoiceVolume) /* 2 */
PORT_DECL(SSound, SetVoiceString) /* 2 */
PORT_DECL(SSound, DeleteAllSounds) /* 0 */
#define PORTS_Engine_SSoundPy \
	PORT_DEFND(SSound, LoadSound, SSound_LoadSound, str_PyDoc_SSound_LoadSound) \
	PORT_DEFND(SSound, PlaySound, SSound_PlaySound, str_PyDoc_SSound_PlaySound) \
	PORT_DEFND(SSound, Play, SSound_Play, str_PyDoc_SSound_Play) \
	PORT_DEFND(SSound, StopAllSounds, SSound_StopAllSounds, str_PyDoc_SSound_StopAllSounds) \
	PORT_DEFND(SSound, SetPS, SSound_SetPS, str_PyDoc_SSound_SetPS) \
	PORT_DEFND(SSound, GetPS, SSound_GetPS, str_PyDoc_SSound_GetPS) \
	PORT_DEFND(SSound, UsePS, SSound_UsePS, str_PyDoc_SSound_UsePS) \
	PORT_DEFND(SSound, CreatePS, SSound_CreatePS, str_PyDoc_SSound_CreatePS) \
	PORT_DEFND(SSound, SetMusic, SSound_SetMusic, str_PyDoc_SSound_SetMusic) \
	PORT_DEFND(SSound, SetMusicVolume, SSound_SetMusicVolume, str_PyDoc_SSound_SetMusicVolume) \
	PORT_DEFND(SSound, OpenVoice, SSound_OpenVoice, str_PyDoc_SSound_OpenVoice) \
	PORT_DEFND(SSound, PlayVoice, SSound_PlayVoice, str_PyDoc_SSound_PlayVoice) \
	PORT_DEFND(SSound, StopVoice, SSound_StopVoice, str_PyDoc_SSound_StopVoice) \
	PORT_DEFND(SSound, VoiceRemain, SSound_VoiceRemain, str_PyDoc_SSound_VoiceRemain) \
	PORT_DEFND(SSound, SetVoiceVolume, SSound_SetVoiceVolume, str_PyDoc_SSound_SetVoiceVolume) \
	PORT_DEFND(SSound, SetVoiceString, SSound_SetVoiceString, str_PyDoc_SSound_SetVoiceString) \
	PORT_DEFND(SSound, DeleteAllSounds, SSound_DeleteAllSounds, str_PyDoc_SSound_DeleteAllSounds)

/* PyInterface/SVecPy.c */
PORT_DECL(SVec, Add) /* 2 */
PORT_DECL(SVec, Sub) /* 2 */
PORT_DECL(SVec, Mod) /* 1 */
PORT_DECL(SVec, ModSqr) /* 1 */
PORT_DECL(SVec, Norm) /* 1 */
PORT_DECL(SVec, Prod) /* 2 */
PORT_DECL(SVec, DProd) /* 2 */
PORT_DECL(SVec, CProd) /* 2 */
PORT_DECL(SVec, Rotate3D) /* 2 */
PORT_DECL(SVec, GetAngle) /* 2 */
PORT_DECL(SVec, GetRotAng) /* ? */
PORT_DECL(SVec, NormAng) /* 1 */
#define PORTS_Engine_SVecPy \
	PORT_DEFND(SVec, Add, SVec_Add, str_PyDoc_SVec_Add) \
	PORT_DEFND(SVec, Sub, SVec_Sub, str_PyDoc_SVec_Sub) \
	PORT_DEFND(SVec, Mod, SVec_Mod, str_PyDoc_SVec_Mod) \
	PORT_DEFND(SVec, ModSqr, SVec_ModSqr, str_PyDoc_SVec_ModSqr) \
	PORT_DEFND(SVec, Norm, SVec_Norm, str_PyDoc_SVec_Norm) \
	PORT_DEFND(SVec, Prod, SVec_Prod, str_PyDoc_SVec_Prod) \
	PORT_DEFND(SVec, DProd, SVec_DProd, str_PyDoc_SVec_DProd) \
	PORT_DEFND(SVec, CProd, SVec_CProd, str_PyDoc_SVec_CProd) \
	PORT_DEFND(SVec, Rotate3D, SVec_Rotate3D, str_PyDoc_SVec_Rotate3D) \
	PORT_DEFND(SVec, GetAngle, SVec_GetAngle, str_PyDoc_SVec_GetAngle) \
	PORT_DEFND(SVec, GetRotAng, SVec_GetRotAng, str_PyDoc_SVec_GetRotAng) \
	PORT_DEFND(SVec, NormAng, SVec_NormAng, str_PyDoc_SVec_NormAng)

/* PyInterface/SWeapPy.c */
PORT_DECL(SWeap, GetNWeap) /* 0 */
PORT_DECL(SWeap, GetNAmmo) /* 0 */
PORT_DECL(SWeap, InitWeap) /* 1 */
PORT_DECL(SWeap, InitAmmo) /* 1 */
PORT_DECL(SWeap, GetFAmmo) /* 2 */
PORT_DECL(SWeap, GetSAmmo) /* 2 */
PORT_DECL(SWeap, SetFAmmo) /* 3 */
PORT_DECL(SWeap, SetSAmmo) /* 3 */
PORT_DECL(SWeap, GetFWeap) /* 2 */
PORT_DECL(SWeap, GetSWeap) /* 2 */
PORT_DECL(SWeap, SetFWeap) /* 3 */
PORT_DECL(SWeap, SetSWeap) /* 3 */
PORT_DECL(SWeap, FillPriority) /* 0 */
PORT_DECL(SWeap, GetFirstiWeap) /* 0 */
PORT_DECL(SWeap, NetExec) /* ? */
#define PORTS_Engine_SWeapPy \
	PORT_DEFND(SWeap, GetNWeap, SWeap_GetNWeap, str_PyDoc_SWeap_GetNWeap) \
	PORT_DEFND(SWeap, GetNAmmo, SWeap_GetNAmmo, str_PyDoc_SWeap_GetNAmmo) \
	PORT_DEFND(SWeap, InitWeap, SWeap_InitWeap, str_PyDoc_SWeap_InitWeap) \
	PORT_DEFND(SWeap, InitAmmo, SWeap_InitAmmo, str_PyDoc_SWeap_InitAmmo) \
	PORT_DEFND(SWeap, GetFAmmo, SWeap_GetFAmmo, str_PyDoc_SWeap_GetFAmmo) \
	PORT_DEFND(SWeap, GetSAmmo, SWeap_GetSAmmo, str_PyDoc_SWeap_GetSAmmo) \
	PORT_DEFND(SWeap, SetFAmmo, SWeap_SetFAmmo, str_PyDoc_SWeap_SetFAmmo) \
	PORT_DEFND(SWeap, SetSAmmo, SWeap_SetSAmmo, str_PyDoc_SWeap_SetSAmmo) \
	PORT_DEFND(SWeap, GetFWeap, SWeap_GetFWeap, str_PyDoc_SWeap_GetFWeap) \
	PORT_DEFND(SWeap, GetSWeap, SWeap_GetSWeap, str_PyDoc_SWeap_GetSWeap) \
	PORT_DEFND(SWeap, SetFWeap, SWeap_SetFWeap, str_PyDoc_SWeap_SetFWeap) \
	PORT_DEFND(SWeap, SetSWeap, SWeap_SetSWeap, str_PyDoc_SWeap_SetSWeap) \
	PORT_DEFND(SWeap, FillPriority, SWeap_FillPriority, str_PyDoc_SWeap_FillPriority) \
	PORT_DEFND(SWeap, GetFirstiWeap, SWeap_GetFirstiWeap, str_PyDoc_SWeap_GetFirstiWeap) \
	PORT_DEFND(SWeap, NetExec, SWeap_NetExec, str_PyDoc_SWeap_NetExec)

/* PyInterface/ScrapPy.c */
PORT_DECL(Scrap, Print) /* 1 */
PORT_DECL(Scrap, ConsoleOut) /* 1 */
PORT_DECL(Scrap, ConsoleError) /* 1 */
PORT_DECL(Scrap, DebugInput) /* 1 */
PORT_DECL(Scrap, Verbose) /* 1 */
PORT_DECL(Scrap, Ver) /* 0 */
PORT_DECL(Scrap, ExtractPack) /* 1 */
PORT_DECL(Scrap, OpenPack) /* 1 */
PORT_DECL(Scrap, ClosePack) /* 0 */
PORT_DECL(Scrap, PreloadLibrary) /* 2 */
PORT_DECL(Scrap, Preload3DObject) /* ? */
PORT_DECL(Scrap, PreloadAnm) /* 2 */
PORT_DECL(Scrap, PythonCompileAll) /* 0 */
PORT_DECL(Scrap, Get) /* 1 */
PORT_DECL(Scrap, Def) /* 1 */
PORT_DECL(Scrap, Des) /* 1 */
PORT_DECL(Scrap, Set) /* 2 */
PORT_DECL(Scrap, ShowGVars) /* ? */
PORT_DECL(Scrap, SwitchMissionArrows) /* 2 */
PORT_DECL(Scrap, GetTime) /* 0 */
PORT_DECL(Scrap, GetTimeSpeed) /* 0 */
PORT_DECL(Scrap, SetTime) /* 1 */
PORT_DECL(Scrap, SetTimeSpeed) /* 1 */
PORT_DECL(Scrap, AddScheduledFunc) /* ? */
PORT_DECL(Scrap, DeleteScheduledFuncs) /* 1 */
PORT_DECL(Scrap, SetCam) /* 2 */
PORT_DECL(Scrap, GetCam) /* 1 */
PORT_DECL(Scrap, GetMinCamDist) /* 3 */
PORT_DECL(Scrap, GetEntity) /* 1 */
PORT_DECL(Scrap, GetFirst) /* 0 */
PORT_DECL(Scrap, CreateEntity) /* 5 */
PORT_DECL(Scrap, UsrEntity) /* 1 */
PORT_DECL(Scrap, EntityListGet) /* 2 */
PORT_DECL(Scrap, EntityListSet) /* 3 */
PORT_DECL(Scrap, CreateEntityList) /* 1 */
PORT_DECL(Scrap, LoadLevel) /* 1 */
PORT_DECL(Scrap, GetLevelPath) /* 0 */
PORT_DECL(Scrap, GetNewLevelPath) /* 0 */
PORT_DECL(Scrap, SetGrid) /* 3 */
PORT_DECL(Scrap, GetFloor) /* 1 */
PORT_DECL(Scrap, CreateElements) /* 0 */
PORT_DECL(Scrap, CallElements) /* 1 */
PORT_DECL(Scrap, GetNetFlags) /* 0 */
PORT_DECL(Scrap, StartDummySearch) /* ? */
PORT_DECL(Scrap, NextDummySearch) /* 0 */
PORT_DECL(Scrap, Rand) /* 2 */
PORT_DECL(Scrap, Round) /* 1 */
PORT_DECL(Scrap, SetDebrisSys) /* ? */
PORT_DECL(Scrap, DropDebris) /* 2 */
PORT_DECL(Scrap, SetDebrisValue) /* 4 */
PORT_DECL(Scrap, ModelInfo) /* ? */
PORT_DECL(Scrap, ListModels) /* 0 */
PORT_DECL(Scrap, BuildFont) /* 2 */
PORT_DECL(Scrap, ScreenShot) /* 1 */
PORT_DECL(Scrap, ProcessDVF) /* 2 */
PORT_DECL(Scrap, VideoPlayer) /* ? */
PORT_DECL(Scrap, MusicPlayer) /* ? */
PORT_DECL(Scrap, GetVideoCurrentMode) /* ? */
PORT_DECL(Scrap, SetVideoCurrentMode) /* 1 */
PORT_DECL(Scrap, GetVideoModes) /* 1 */
PORT_DECL(Scrap, TestLine) /* 3 */
PORT_DECL(Scrap, TestSphere) /* ? */
PORT_DECL(Scrap, SphereCall) /* ? */
PORT_DECL(Scrap, SetCallFunc) /* 1 */
PORT_DECL(Scrap, AddParamf) /* 1 */
PORT_DECL(Scrap, AddParami) /* 1 */
PORT_DECL(Scrap, AddParams) /* 1 */
PORT_DECL(Scrap, Execute) /* 0 */
PORT_DECL(Scrap, DelSaveVars) /* 1 */
PORT_DECL(Scrap, CreateSaveVar) /* 2 */
PORT_DECL(Scrap, GetSaveVar) /* 1 */
PORT_DECL(Scrap, SetSaveVar) /* 2 */
PORT_DECL(Scrap, IncSaveVar) /* ? */
PORT_DECL(Scrap, LoadGameVars) /* 1 */
PORT_DECL(Scrap, CreateSaveProfile) /* 1 */
PORT_DECL(Scrap, DeleteProfile) /* 1 */
PORT_DECL(Scrap, SetSaveProfile) /* 2 */
PORT_DECL(Scrap, GetSavedProfilesList) /* 0 */
PORT_DECL(Scrap, GetFreeBlocks) /* 0 */
PORT_DECL(Scrap, LaunchDashboard) /* 1 */
PORT_DECL(Scrap, DeselectProfile) /* 0 */
PORT_DECL(Scrap, SaveGameVars) /* ? */
PORT_DECL(Scrap, SaveConfig) /* 0 */
PORT_DECL(Scrap, GetSaveGamesList) /* 1 */
PORT_DECL(Scrap, SetLanguage) /* ? */
PORT_DECL(Scrap, GetLanguage) /* 0 */
PORT_DECL(Scrap, GetLangStr) /* 1 */
PORT_DECL(Scrap, GetLangEntries) /* ? */
PORT_DECL(Scrap, GetAlarm) /* 0 */
PORT_DECL(Scrap, SetAlarm) /* 1 */
PORT_DECL(Scrap, SetAlarmGrow) /* 1 */
PORT_DECL(Scrap, GetAlarmChars) /* 0 */
PORT_DECL(Scrap, SetAlarmChar) /* 1 */
PORT_DECL(Scrap, GetLockAlarm) /* 0 */
PORT_DECL(Scrap, SetLockAlarm) /* 1 */
PORT_DECL(Scrap, GetMoney) /* 0 */
PORT_DECL(Scrap, SetMoney) /* 1 */
PORT_DECL(Scrap, AddParkingZone) /* 3 */
PORT_DECL(Scrap, GetFarestParked) /* ? */
PORT_DECL(Scrap, GetNearestParked) /* ? */
PORT_DECL(Scrap, AddItem) /* ? */
PORT_DECL(Scrap, InitLoading) /* 0 */
PORT_DECL(Scrap, EndLoading) /* 0 */
PORT_DECL(Scrap, Exit) /* 0 */
PORT_DECL(Scrap, FileExist) /* 1 */
PORT_DECL(Scrap, RunApp) /* 2 */
PORT_DECL(Scrap, Achievement) /* 1 */
PORT_DECL(Scrap, OpenStore) /* 1 */
PORT_DECL(Scrap, IsApp) /* 1 */
PORT_DECL(Scrap, GetSteamID) /* 0 */
PORT_DECL(Scrap, GetSteamTicket) /* 0 */
PORT_DECL(Scrap, IsSteam) /* 0 */
PORT_DECL(Scrap, GetGogID) /* 0 */
PORT_DECL(Scrap, ConsoleExec) /* 1 */
#define PORTS_Engine_ScrapPy \
	PORT_DEFND(Scrap, Print, Scrap_Print, str_PyDoc_Scrap_Print) \
	PORT_DEFND(Scrap, ConsoleOut, Scrap_ConsoleOut, str_PyDoc_Scrap_ConsoleOut) \
	PORT_DEFND(Scrap, ConsoleError, Scrap_ConsoleError, str_PyDoc_Scrap_ConsoleError) \
	PORT_DEFND(Scrap, DebugInput, Scrap_DebugInput, str_PyDoc_Scrap_DebugInput) \
	PORT_DEFND(Scrap, Verbose, Scrap_Verbose, str_PyDoc_Scrap_Verbose) \
	PORT_DEFND(Scrap, Ver, Scrap_Ver, str_PyDoc_Scrap_Ver) \
	PORT_DEFND(Scrap, ExtractPack, Scrap_ExtractPack, str_PyDoc_Scrap_ExtractPack) \
	PORT_DEFND(Scrap, OpenPack, Scrap_OpenPack, str_PyDoc_Scrap_OpenPack) \
	PORT_DEFND(Scrap, ClosePack, Scrap_ClosePack, str_PyDoc_Scrap_ClosePack) \
	PORT_DEFND(Scrap, PreloadLibrary, Scrap_PreloadLibrary, str_PyDoc_Scrap_PreloadLibrary) \
	PORT_DEFND(Scrap, Preload3DObject, Scrap_Preload3DObject, str_PyDoc_Scrap_Preload3DObject) \
	PORT_DEFND(Scrap, PreloadAnm, Scrap_PreloadAnm, str_PyDoc_Scrap_PreloadAnm) \
	PORT_DEFND(Scrap, PythonCompileAll, Scrap_PythonCompileAll, str_PyDoc_Scrap_PythonCompileAll) \
	PORT_DEFND(Scrap, Get, Scrap_Get, str_PyDoc_Scrap_Get) \
	PORT_DEFND(Scrap, Def, Scrap_Def, str_PyDoc_Scrap_Def) \
	PORT_DEFND(Scrap, Des, Scrap_Des, str_PyDoc_Scrap_Des) \
	PORT_DEFND(Scrap, Set, Scrap_Set, str_PyDoc_Scrap_Set) \
	PORT_DEFND(Scrap, ShowGVars, Scrap_ShowGVars, str_PyDoc_Scrap_ShowGVars) \
	PORT_DEFND(Scrap, SwitchMissionArrows, Scrap_SwitchMissionArrows, str_PyDoc_Scrap_SwitchMissionArrows) \
	PORT_DEFND(Scrap, GetTime, Scrap_GetTime, str_PyDoc_Scrap_GetTime) \
	PORT_DEFND(Scrap, GetTimeSpeed, Scrap_GetTimeSpeed, str_PyDoc_Scrap_GetTimeSpeed) \
	PORT_DEFND(Scrap, SetTime, Scrap_SetTime, str_PyDoc_Scrap_SetTime) \
	PORT_DEFND(Scrap, SetTimeSpeed, Scrap_SetTimeSpeed, str_PyDoc_Scrap_SetTimeSpeed) \
	PORT_DEFND(Scrap, AddScheduledFunc, Scrap_AddScheduledFunc, str_PyDoc_Scrap_AddScheduledFunc) \
	PORT_DEFND(Scrap, DeleteScheduledFuncs, Scrap_DeleteScheduledFuncs, str_PyDoc_Scrap_DeleteScheduledFuncs) \
	PORT_DEFND(Scrap, SetCam, Scrap_SetCam, str_PyDoc_Scrap_SetCam) \
	PORT_DEFND(Scrap, GetCam, Scrap_GetCam, str_PyDoc_Scrap_GetCam) \
	PORT_DEFND(Scrap, GetMinCamDist, Scrap_GetMinCamDist, str_PyDoc_Scrap_GetMinCamDist) \
	PORT_DEFND(Scrap, GetEntity, Scrap_GetEntity, str_PyDoc_Scrap_GetEntity) \
	PORT_DEFND(Scrap, GetFirst, Scrap_GetFirst, str_PyDoc_Scrap_GetFirst) \
	PORT_DEFND(Scrap, CreateEntity, Scrap_CreateEntity, str_PyDoc_Scrap_CreateEntity) \
	PORT_DEFND(Scrap, UsrEntity, Scrap_UsrEntity, str_PyDoc_Scrap_UsrEntity) \
	PORT_DEFND(Scrap, EntityListGet, Scrap_EntityListGet, str_PyDoc_Scrap_EntityListGet) \
	PORT_DEFND(Scrap, EntityListSet, Scrap_EntityListSet, str_PyDoc_Scrap_EntityListSet) \
	PORT_DEFND(Scrap, CreateEntityList, Scrap_CreateEntityList, str_PyDoc_Scrap_CreateEntityList) \
	PORT_DEFND(Scrap, LoadLevel, Scrap_LoadLevel, str_PyDoc_Scrap_LoadLevel) \
	PORT_DEFND(Scrap, GetLevelPath, Scrap_GetLevelPath, str_PyDoc_Scrap_GetLevelPath) \
	PORT_DEFND(Scrap, GetNewLevelPath, Scrap_GetNewLevelPath, str_PyDoc_Scrap_GetNewLevelPath) \
	PORT_DEFND(Scrap, SetGrid, Scrap_SetGrid, str_PyDoc_Scrap_SetGrid) \
	PORT_DEFND(Scrap, GetFloor, Scrap_GetFloor, str_PyDoc_Scrap_GetFloor) \
	PORT_DEFND(Scrap, CreateElements, Scrap_CreateElements, str_PyDoc_Scrap_CreateElements) \
	PORT_DEFND(Scrap, CallElements, Scrap_CallElements, str_PyDoc_Scrap_CallElements) \
	PORT_DEFND(Scrap, GetNetFlags, Scrap_GetNetFlags, str_PyDoc_Scrap_GetNetFlags) \
	PORT_DEFND(Scrap, StartDummySearch, Scrap_StartDummySearch, str_PyDoc_Scrap_StartDummySearch) \
	PORT_DEFND(Scrap, NextDummySearch, Scrap_NextDummySearch, str_PyDoc_Scrap_NextDummySearch) \
	PORT_DEFND(Scrap, Rand, Scrap_Rand, str_PyDoc_Scrap_Rand) \
	PORT_DEFND(Scrap, Round, Scrap_Round, str_PyDoc_Scrap_Round) \
	PORT_DEFND(Scrap, SetDebrisSys, Scrap_SetDebrisSys, str_PyDoc_Scrap_SetDebrisSys) \
	PORT_DEFND(Scrap, DropDebris, Scrap_DropDebris, str_PyDoc_Scrap_DropDebris) \
	PORT_DEFND(Scrap, SetDebrisValue, Scrap_SetDebrisValue, str_PyDoc_Scrap_SetDebrisValue) \
	PORT_DEFND(Scrap, ModelInfo, Scrap_ModelInfo, str_PyDoc_Scrap_ModelInfo) \
	PORT_DEFND(Scrap, ListModels, Scrap_ListModels, str_PyDoc_Scrap_ListModels) \
	PORT_DEFND(Scrap, BuildFont, Scrap_BuildFont, str_PyDoc_Scrap_BuildFont) \
	PORT_DEFND(Scrap, ScreenShot, Scrap_ScreenShot, str_PyDoc_Scrap_ScreenShot) \
	PORT_DEFND(Scrap, ProcessDVF, Scrap_ProcessDVF, str_PyDoc_Scrap_ProcessDVF) \
	PORT_DEFND(Scrap, VideoPlayer, Scrap_VideoPlayer, str_PyDoc_Scrap_VideoPlayer) \
	PORT_DEFND(Scrap, MusicPlayer, Scrap_MusicPlayer, str_PyDoc_Scrap_MusicPlayer) \
	PORT_DEFND(Scrap, GetVideoCurrentMode, Scrap_GetVideoCurrentMode, str_PyDoc_Scrap_GetVideoCurrentMode) \
	PORT_DEFND(Scrap, SetVideoCurrentMode, Scrap_SetVideoCurrentMode, str_PyDoc_Scrap_SetVideoCurrentMode) \
	PORT_DEFND(Scrap, GetVideoModes, Scrap_GetVideoModes, str_PyDoc_Scrap_GetVideoModes) \
	PORT_DEFND(Scrap, TestLine, Scrap_TestLine, str_PyDoc_Scrap_TestLine) \
	PORT_DEFND(Scrap, TestSphere, Scrap_TestSphere, str_PyDoc_Scrap_TestSphere) \
	PORT_DEFND(Scrap, SphereCall, Scrap_SphereCall, str_PyDoc_Scrap_SphereCall) \
	PORT_DEFND(Scrap, SetCallFunc, Scrap_SetCallFunc, str_PyDoc_Scrap_SetCallFunc) \
	PORT_DEFND(Scrap, AddParamf, Scrap_AddParamf, str_PyDoc_Scrap_AddParamf) \
	PORT_DEFND(Scrap, AddParami, Scrap_AddParami, str_PyDoc_Scrap_AddParami) \
	PORT_DEFND(Scrap, AddParams, Scrap_AddParams, str_PyDoc_Scrap_AddParams) \
	PORT_DEFND(Scrap, Execute, Scrap_Execute, str_PyDoc_Scrap_Execute) \
	PORT_DEFND(Scrap, DelSaveVars, Scrap_DelSaveVars, str_PyDoc_Scrap_DelSaveVars) \
	PORT_DEFND(Scrap, CreateSaveVar, Scrap_CreateSaveVar, str_PyDoc_Scrap_CreateSaveVar) \
	PORT_DEFND(Scrap, GetSaveVar, Scrap_GetSaveVar, str_PyDoc_Scrap_GetSaveVar) \
	PORT_DEFND(Scrap, SetSaveVar, Scrap_SetSaveVar, str_PyDoc_Scrap_SetSaveVar) \
	PORT_DEFND(Scrap, IncSaveVar, Scrap_IncSaveVar, str_PyDoc_Scrap_IncSaveVar) \
	PORT_DEFND(Scrap, LoadGameVars, Scrap_LoadGameVars, str_PyDoc_Scrap_LoadGameVars) \
	PORT_DEFND(Scrap, CreateSaveProfile, Scrap_CreateSaveProfile, str_PyDoc_Scrap_CreateSaveProfile) \
	PORT_DEFND(Scrap, DeleteProfile, Scrap_DeleteProfile, str_PyDoc_Scrap_DeleteProfile) \
	PORT_DEFND(Scrap, SetSaveProfile, Scrap_SetSaveProfile, str_PyDoc_Scrap_SetSaveProfile) \
	PORT_DEFND(Scrap, GetSavedProfilesList, Scrap_GetSavedProfilesList, str_PyDoc_Scrap_GetSavedProfilesList) \
	PORT_DEFND(Scrap, GetFreeBlocks, Scrap_GetFreeBlocks, str_PyDoc_Scrap_GetFreeBlocks) \
	PORT_DEFND(Scrap, LaunchDashboard, Scrap_LaunchDashboard, str_PyDoc_Scrap_LaunchDashboard) \
	PORT_DEFND(Scrap, DeselectProfile, Scrap_DeselectProfile, str_PyDoc_Scrap_DeselectProfile) \
	PORT_DEFND(Scrap, SaveGameVars, Scrap_SaveGameVars, str_PyDoc_Scrap_SaveGameVars) \
	PORT_DEFND(Scrap, SaveConfig, Scrap_SaveConfig, str_PyDoc_Scrap_SaveConfig) \
	PORT_DEFND(Scrap, GetSaveGamesList, Scrap_GetSaveGamesList, str_PyDoc_Scrap_GetSaveGamesList) \
	PORT_DEFND(Scrap, SetLanguage, Scrap_SetLanguage, str_PyDoc_Scrap_SetLanguage) \
	PORT_DEFND(Scrap, GetLanguage, Scrap_GetLanguage, str_PyDoc_Scrap_GetLanguage) \
	PORT_DEFND(Scrap, GetLangStr, Scrap_GetLangStr, str_PyDoc_Scrap_GetLangStr) \
	PORT_DEFND(Scrap, GetLangEntries, Scrap_GetLangEntries, str_PyDoc_Scrap_GetLangEntries) \
	PORT_DEFND(Scrap, GetAlarm, Scrap_GetAlarm, str_PyDoc_Scrap_GetAlarm) \
	PORT_DEFND(Scrap, SetAlarm, Scrap_SetAlarm, str_PyDoc_Scrap_SetAlarm) \
	PORT_DEFND(Scrap, SetAlarmGrow, Scrap_SetAlarmGrow, str_PyDoc_Scrap_SetAlarmGrow) \
	PORT_DEFND(Scrap, GetAlarmChars, Scrap_GetAlarmChars, str_PyDoc_Scrap_GetAlarmChars) \
	PORT_DEFND(Scrap, SetAlarmChar, Scrap_SetAlarmChar, str_PyDoc_Scrap_SetAlarmChar) \
	PORT_DEFND(Scrap, GetLockAlarm, Scrap_GetLockAlarm, str_PyDoc_Scrap_GetLockAlarm) \
	PORT_DEFND(Scrap, SetLockAlarm, Scrap_SetLockAlarm, str_PyDoc_Scrap_SetLockAlarm) \
	PORT_DEFND(Scrap, GetMoney, Scrap_GetMoney, str_PyDoc_Scrap_GetMoney) \
	PORT_DEFND(Scrap, SetMoney, Scrap_SetMoney, str_PyDoc_Scrap_SetMoney) \
	PORT_DEFND(Scrap, AddParkingZone, Scrap_AddParkingZone, str_PyDoc_Scrap_AddParkingZone) \
	PORT_DEFND(Scrap, GetFarestParked, Scrap_GetFarestParked, str_PyDoc_Scrap_GetFarestParked) \
	PORT_DEFND(Scrap, GetNearestParked, Scrap_GetNearestParked, str_PyDoc_Scrap_GetNearestParked) \
	PORT_DEFND(Scrap, AddItem, Scrap_AddItem, str_PyDoc_Scrap_AddItem) \
	PORT_DEFND(Scrap, InitLoading, Scrap_InitLoading, str_PyDoc_Scrap_InitLoading) \
	PORT_DEFND(Scrap, EndLoading, Scrap_EndLoading, str_PyDoc_Scrap_EndLoading) \
	PORT_DEFND(Scrap, Exit, Scrap_Exit, str_PyDoc_Scrap_Exit) \
	PORT_DEFND(Scrap, FileExist, Scrap_FileExist, str_PyDoc_Scrap_FileExist) \
	PORT_DEFND(Scrap, RunApp, Scrap_RunApp, str_PyDoc_Scrap_RunApp) \
	PORT_DEFND(Scrap, Achievement, Scrap_Achievement, str_PyDoc_Scrap_Achievement) \
	PORT_DEFND(Scrap, OpenStore, Scrap_OpenStore, str_PyDoc_Scrap_OpenStore) \
	PORT_DEFND(Scrap, IsApp, Scrap_IsApp, str_PyDoc_Scrap_IsApp) \
	PORT_DEFND(Scrap, GetSteamID, Scrap_GetSteamID, str_PyDoc_Scrap_GetSteamID) \
	PORT_DEFND(Scrap, GetSteamTicket, Scrap_GetSteamTicket, str_PyDoc_Scrap_GetSteamTicket) \
	PORT_DEFND(Scrap, IsSteam, Scrap_IsSteam, str_PyDoc_Scrap_IsSteam) \
	PORT_DEFND(Scrap, GetGogID, Scrap_GetGogID, str_PyDoc_Scrap_GetGogID) \
	PORT_DEFND(Scrap, ConsoleExec, Scrap_ConsoleExec, "ConsoleExec(line) : runs a line of the console")

/* RaceMaker.c */
PORT_DECL(RaceMaker, ShowSystemMessage) /* 1 */
PORT_DECL(RaceMaker, ShowSystemMessageDelay) /* 2 */
PORT_DECL(RaceMaker, NPCCombatKilled) /* 2 */
PORT_DECL(RaceMaker, DeleteCurrentWaypoint) /* 0 */
PORT_DECL(RaceMaker, MovePlayersToTraffic) /* 0 */
PORT_DECL(RaceMaker, EndOfRace) /* 2 */
PORT_DECL(RaceMaker, RaceStart) /* 2 */
PORT_DECL(RaceMaker, OnCounterMeasure) /* ? */
PORT_DECL(RaceMaker, RacerKilled) /* 2 */
PORT_DECL(RaceMaker, RacerInterruptedByAlarm) /* 0 */
PORT_DECL(RaceMaker, RacerDamaged) /* 3 */
PORT_DECL(RaceMaker, RacerLeft) /* 0 */
PORT_DECL(RaceMaker, AbortCameraTravelling) /* 0 */
PORT_DECL(RaceMaker, StartCameraTravelling) /* ? */
PORT_DECL(RaceMaker, OnRaceStart) /* 2 */
PORT_DECL(RaceMaker, RaceGo) /* 2 */
PORT_DECL(RaceMaker, CreateRaceCountDown) /* 0 */
PORT_DECL(RaceMaker, DestroyRaceCountDown) /* 0 */
PORT_DECL(RaceMaker, DestroyRaceCountDownEnd) /* 1 */
PORT_DECL(RaceMaker, Counter) /* ? */
PORT_DECL(RaceMaker, OnCheckPoint) /* 2 */
PORT_DECL(RaceMaker, OnNearCheckPoint) /* 2 */
PORT_DECL(RaceMaker, NameOponGoItem) /* 1 */
PORT_DECL(RaceMaker, LocalGetDealType) /* 0 */
PORT_DECL(RaceMaker, StopTimer) /* 0 */
PORT_DECL(RaceMaker, StartTimer) /* 0 */
PORT_DECL(RaceMaker, FirstTimeRace) /* 1 */
PORT_DECL(RaceMaker, FirstTimeCombat) /* 1 */
PORT_DECL(RaceMaker, PointObjectTo) /* 2 */
PORT_DECL(RaceMaker, PreloadCnDnResources) /* 0 */
PORT_DECL(RaceMaker, InitRace) /* ? */
PORT_DECL(RaceMaker, InitCombat) /* ? */
PORT_DECL(RaceMaker, MoveCombatNPCsToPos) /* 1 */
PORT_DECL(RaceMaker, PlaceNPCAtRacePos) /* 2 */
PORT_DECL(RaceMaker, SetScorerValues) /* 1 */
PORT_DECL(RaceMaker, MissionScorer2) /* 1 */
PORT_DECL(RaceMaker, MissionScorer) /* 1 */
PORT_DECL(RaceMaker, InitShipIAProfile) /* 2 */
PORT_DECL(RaceMaker, GetRandomShipIAProfile) /* 2 */
#define PORTS_RaceMaker \
	PORT_DEFN(RaceMaker, ShowSystemMessage, RaceMaker_ShowSystemMessage) \
	PORT_DEFN(RaceMaker, ShowSystemMessageDelay, RaceMaker_ShowSystemMessageDelay) \
	PORT_DEFN(RaceMaker, NPCCombatKilled, RaceMaker_NPCCombatKilled) \
	PORT_DEFN(RaceMaker, DeleteCurrentWaypoint, RaceMaker_DeleteCurrentWaypoint) \
	PORT_DEFN(RaceMaker, MovePlayersToTraffic, RaceMaker_MovePlayersToTraffic) \
	PORT_DEFN(RaceMaker, EndOfRace, RaceMaker_EndOfRace) \
	PORT_DEFN(RaceMaker, RaceStart, RaceMaker_RaceStart) \
	PORT_DEFNK(RaceMaker, OnCounterMeasure, RaceMaker_OnCounterMeasure) \
	PORT_DEFN(RaceMaker, RacerKilled, RaceMaker_RacerKilled) \
	PORT_DEFN(RaceMaker, RacerInterruptedByAlarm, RaceMaker_RacerInterruptedByAlarm) \
	PORT_DEFN(RaceMaker, RacerDamaged, RaceMaker_RacerDamaged) \
	PORT_DEFN(RaceMaker, RacerLeft, RaceMaker_RacerLeft) \
	PORT_DEFN(RaceMaker, AbortCameraTravelling, RaceMaker_AbortCameraTravelling) \
	PORT_DEFNK(RaceMaker, StartCameraTravelling, RaceMaker_StartCameraTravelling) \
	PORT_DEFN(RaceMaker, OnRaceStart, RaceMaker_OnRaceStart) \
	PORT_DEFN(RaceMaker, RaceGo, RaceMaker_RaceGo) \
	PORT_DEFN(RaceMaker, CreateRaceCountDown, RaceMaker_CreateRaceCountDown) \
	PORT_DEFN(RaceMaker, DestroyRaceCountDown, RaceMaker_DestroyRaceCountDown) \
	PORT_DEFN(RaceMaker, DestroyRaceCountDownEnd, RaceMaker_DestroyRaceCountDownEnd) \
	PORT_DEFNK(RaceMaker, Counter, RaceMaker_Counter) \
	PORT_DEFN(RaceMaker, OnCheckPoint, RaceMaker_OnCheckPoint) \
	PORT_DEFN(RaceMaker, OnNearCheckPoint, RaceMaker_OnNearCheckPoint) \
	PORT_DEFN(RaceMaker, NameOponGoItem, RaceMaker_NameOponGoItem) \
	PORT_DEFN(RaceMaker, LocalGetDealType, RaceMaker_LocalGetDealType) \
	PORT_DEFN(RaceMaker, StopTimer, RaceMaker_StopTimer) \
	PORT_DEFN(RaceMaker, StartTimer, RaceMaker_StartTimer) \
	PORT_DEFN(RaceMaker, FirstTimeRace, RaceMaker_FirstTimeRace) \
	PORT_DEFN(RaceMaker, FirstTimeCombat, RaceMaker_FirstTimeCombat) \
	PORT_DEFN(RaceMaker, PointObjectTo, RaceMaker_PointObjectTo) \
	PORT_DEFN(RaceMaker, PreloadCnDnResources, RaceMaker_PreloadCnDnResources) \
	PORT_DEFNK(RaceMaker, InitRace, RaceMaker_InitRace) \
	PORT_DEFNK(RaceMaker, InitCombat, RaceMaker_InitCombat) \
	PORT_DEFN(RaceMaker, MoveCombatNPCsToPos, RaceMaker_MoveCombatNPCsToPos) \
	PORT_DEFN(RaceMaker, PlaceNPCAtRacePos, RaceMaker_PlaceNPCAtRacePos) \
	PORT_DEFN(RaceMaker, SetScorerValues, RaceMaker_SetScorerValues) \
	PORT_DEFN(RaceMaker, MissionScorer2, RaceMaker_MissionScorer2) \
	PORT_DEFN(RaceMaker, MissionScorer, RaceMaker_MissionScorer) \
	PORT_DEFN(RaceMaker, InitShipIAProfile, RaceMaker_InitShipIAProfile) \
	PORT_DEFN(RaceMaker, GetRandomShipIAProfile, RaceMaker_GetRandomShipIAProfile)

/* SaveGame.c */
PORT_DECL(SaveGame, MMDesc) /* 2 */
PORT_DECL(SaveGame, SMDesc) /* 2 */
PORT_DECL(SaveGame, SaveGame) /* 1 */
PORT_DECL(SaveGame, InstantSaveGame) /* ? */
PORT_DECL(SaveGame, LoadGame) /* 1 */
PORT_DECL(SaveGame, NewGame) /* ? */
PORT_DECL(SaveGame, CheckAllBlueprints) /* ? */
PORT_DECL(SaveGame, MarkWayPoint) /* ? */
PORT_DECL(SaveGame, QWayPoint) /* ? */
PORT_DECL(SaveGame, SaveAlarm) /* ? */
PORT_DECL(SaveGame, LoadAlarm) /* ? */
PORT_DECL(SaveGame, SaveMoney) /* ? */
PORT_DECL(SaveGame, LoadMoney) /* ? */
PORT_DECL(SaveGame, QLoad) /* ? */
PORT_DECL(SaveGame, StartGame) /* ? */
PORT_DECL(SaveGame, Restart) /* ? */
PORT_DECL(SaveGame, QSaveReg) /* 1 */
PORT_DECL(SaveGame, SaveReg) /* 2 */
PORT_DECL(SaveGame, GetReg) /* 1 */
PORT_DECL(SaveGame, InitVars) /* ? */
#define PORTS_SaveGame \
	PORT_DEFN(SaveGame, MMDesc, SaveGame_MMDesc) \
	PORT_DEFN(SaveGame, SMDesc, SaveGame_SMDesc) \
	PORT_DEFN(SaveGame, SaveGame, SaveGame_SaveGame) \
	PORT_DEFNK(SaveGame, InstantSaveGame, SaveGame_InstantSaveGame) \
	PORT_DEFN(SaveGame, LoadGame, SaveGame_LoadGame) \
	PORT_DEFN(SaveGame, NewGame, SaveGame_NewGame) \
	PORT_DEFN(SaveGame, CheckAllBlueprints, SaveGame_CheckAllBlueprints) \
	PORT_DEFNK(SaveGame, MarkWayPoint, SaveGame_MarkWayPoint) \
	PORT_DEFN(SaveGame, QWayPoint, SaveGame_QWayPoint) \
	PORT_DEFN(SaveGame, SaveAlarm, SaveGame_SaveAlarm) \
	PORT_DEFN(SaveGame, LoadAlarm, SaveGame_LoadAlarm) \
	PORT_DEFN(SaveGame, SaveMoney, SaveGame_SaveMoney) \
	PORT_DEFN(SaveGame, LoadMoney, SaveGame_LoadMoney) \
	PORT_DEFNK(SaveGame, QLoad, SaveGame_QLoad) \
	PORT_DEFNK(SaveGame, StartGame, SaveGame_StartGame) \
	PORT_DEFN(SaveGame, Restart, SaveGame_Restart) \
	PORT_DEFN(SaveGame, QSaveReg, SaveGame_QSaveReg) \
	PORT_DEFN(SaveGame, SaveReg, SaveGame_SaveReg) \
	PORT_DEFN(SaveGame, GetReg, SaveGame_GetReg) \
	PORT_DEFN(SaveGame, InitVars, SaveGame_InitVars)

/* Scene.c */
PORT_DECL(Scene, BaseAddActor) /* 3 */
PORT_DECL(Scene, AddCharActor) /* 2 */
PORT_DECL(Scene, AddLocalActor) /* 2 */
PORT_DECL(Scene, ActorAction) /* ? */
PORT_DECL(Scene, ActorActEvent) /* 3 */
PORT_DECL(Scene, ActorBreakChannel) /* 3 */
PORT_DECL(Scene, ActorActBreakChanne) /* 3 */
PORT_DECL(Scene, ObjAction) /* ? */
PORT_DECL(Scene, ObjActEvent) /* 3 */
PORT_DECL(Scene, SetDefSounds) /* 2 */
PORT_DECL(Scene, CreateLocalAction) /* ? */
PORT_DECL(Scene, ExecGeneric) /* 3 */
PORT_DECL(Scene, AddGeneric) /* ? */
PORT_DECL(Scene, AddStream) /* ? */
PORT_DECL(Scene, PlayStream) /* 3 */
PORT_DECL(Scene, PlayVoiceVol) /* 2 */
PORT_DECL(Scene, PlayText) /* 3 */
PORT_DECL(Scene, SetText) /* 2 */
PORT_DECL(Scene, StopSpeech) /* ? */
PORT_DECL(Scene, AddText) /* ? */
PORT_DECL(Scene, AbortScene) /* 1 */
PORT_DECL(Scene, EndOfScene) /* 3 */
PORT_DECL(Scene, DelClass) /* 1 */
PORT_DECL(Scene, AddSound) /* ? */
PORT_DECL(Scene, Load) /* 1 */
PORT_DECL(Scene, UnhideChars) /* 0 */
PORT_DECL(Scene, HideThis) /* 1 */
PORT_DECL(Scene, HideChars) /* 0 */
PORT_DECL(Scene, HideVehiclesToo) /* 0 */
PORT_DECL(Scene, Play) /* 1 */
PORT_DECL(Scene, InitOutDoorScene) /* ? */
PORT_DECL(Scene, EndOutDoorScene) /* 0 */
#define PORTS_Scene \
	PORT_DEFN(Scene, BaseAddActor, Scene_BaseAddActor) \
	PORT_DEFN(Scene, AddCharActor, Scene_AddCharActor) \
	PORT_DEFN(Scene, AddLocalActor, Scene_AddLocalActor) \
	PORT_DEFNK(Scene, ActorAction, Scene_ActorAction) \
	PORT_DEFN(Scene, ActorActEvent, Scene_ActorActEvent) \
	PORT_DEFN(Scene, ActorBreakChannel, Scene_ActorBreakChannel) \
	PORT_DEFN(Scene, ActorActBreakChanne, Scene_ActorActBreakChanne) \
	PORT_DEFNK(Scene, ObjAction, Scene_ObjAction) \
	PORT_DEFN(Scene, ObjActEvent, Scene_ObjActEvent) \
	PORT_DEFN(Scene, SetDefSounds, Scene_SetDefSounds) \
	PORT_DEFNK(Scene, CreateLocalAction, Scene_CreateLocalAction) \
	PORT_DEFN(Scene, ExecGeneric, Scene_ExecGeneric) \
	PORT_DEFNK(Scene, AddGeneric, Scene_AddGeneric) \
	PORT_DEFNK(Scene, AddStream, Scene_AddStream) \
	PORT_DEFN(Scene, PlayStream, Scene_PlayStream) \
	PORT_DEFN(Scene, PlayVoiceVol, Scene_PlayVoiceVol) \
	PORT_DEFN(Scene, PlayText, Scene_PlayText) \
	PORT_DEFN(Scene, SetText, Scene_SetText) \
	PORT_DEFNK(Scene, StopSpeech, Scene_StopSpeech) \
	PORT_DEFNK(Scene, AddText, Scene_AddText) \
	PORT_DEFN(Scene, AbortScene, Scene_AbortScene) \
	PORT_DEFN(Scene, EndOfScene, Scene_EndOfScene) \
	PORT_DEFN(Scene, DelClass, Scene_DelClass) \
	PORT_DEFNK(Scene, AddSound, Scene_AddSound) \
	PORT_DEFN(Scene, Load, Scene_Load) \
	PORT_DEFN(Scene, UnhideChars, Scene_UnhideChars) \
	PORT_DEFN(Scene, HideThis, Scene_HideThis) \
	PORT_DEFN(Scene, HideChars, Scene_HideChars) \
	PORT_DEFN(Scene, HideVehiclesToo, Scene_HideVehiclesToo) \
	PORT_DEFN(Scene, Play, Scene_Play) \
	PORT_DEFNK(Scene, InitOutDoorScene, Scene_InitOutDoorScene) \
	PORT_DEFN(Scene, EndOutDoorScene, Scene_EndOutDoorScene)

/* ShipEdit.c */
PORT_DECL(ShipEdit, IsEditingMultiplayer) /* 0 */
PORT_DECL(ShipEdit, RustyOnPlatform) /* 1 */
PORT_DECL(ShipEdit, PointObjectTo) /* 2 */
PORT_DECL(ShipEdit, LaunchPlatformAnim) /* 0 */
PORT_DECL(ShipEdit, ChangeShip) /* 3 */
PORT_DECL(ShipEdit, VerifyChangeShip) /* 3 */
PORT_DECL(ShipEdit, DoorInitializator) /* 1 */
PORT_DECL(ShipEdit, ObjInitializator) /* 2 */
PORT_DECL(ShipEdit, EndMoveEngine) /* 1 */
PORT_DECL(ShipEdit, RotateShip) /* 1 */
PORT_DECL(ShipEdit, LoopRotateShip) /* 1 */
PORT_DECL(ShipEdit, StartRotateLoop) /* 0 */
PORT_DECL(ShipEdit, StopRotateLoop) /* 0 */
PORT_DECL(ShipEdit, DrawBackground) /* ? */
PORT_DECL(ShipEdit, AddShipEditTabs) /* 2 */
PORT_DECL(ShipEdit, AddDefaultPanelContent) /* 2 */
PORT_DECL(ShipEdit, ChangeTabLeft) /* 2 */
PORT_DECL(ShipEdit, ChangeTabRight) /* 2 */
PORT_DECL(ShipEdit, ChangeTab) /* ? */
PORT_DECL(ShipEdit, CleanEditedShips) /* 0 */
PORT_DECL(ShipEdit, CreatePlayerShip) /* 0 */
PORT_DECL(ShipEdit, SetupForEdit) /* 1 */
PORT_DECL(ShipEdit, SaveShipInfoInSaveVar) /* 2 */
PORT_DECL(ShipEdit, SaveShipInfoInGVar) /* 2 */
PORT_DECL(ShipEdit, CreateShipFromSaveVar) /* ? */
PORT_DECL(ShipEdit, AssignShip) /* 2 */
PORT_DECL(ShipEdit, SetupEdit) /* 1 */
PORT_DECL(ShipEdit, FreeEdit) /* 0 */
PORT_DECL(ShipEdit, EnterEditShipMenu) /* 2 */
PORT_DECL(ShipEdit, ExitEditShipMenu) /* 2 */
PORT_DECL(ShipEdit, SaveShip) /* 1 */
PORT_DECL(ShipEdit, ShowSpecialHint) /* 0 */
#define PORTS_ShipEdit \
	PORT_DEFN(ShipEdit, IsEditingMultiplayer, SE_IsEditingMultiplayer) \
	PORT_DEFN(ShipEdit, RustyOnPlatform, SE_RustyOnPlatform) \
	PORT_DEFN(ShipEdit, PointObjectTo, SE_PointObjectTo) \
	PORT_DEFN(ShipEdit, LaunchPlatformAnim, SE_LaunchPlatformAnim) \
	PORT_DEFN(ShipEdit, ChangeShip, SE_ChangeShip) \
	PORT_DEFN(ShipEdit, VerifyChangeShip, SE_VerifyChangeShip) \
	PORT_DEFN(ShipEdit, DoorInitializator, SE_DoorInitializator) \
	PORT_DEFN(ShipEdit, ObjInitializator, SE_ObjInitializator) \
	PORT_DEFN(ShipEdit, EndMoveEngine, SE_EndMoveEngine) \
	PORT_DEFN(ShipEdit, RotateShip, SE_RotateShip) \
	PORT_DEFN(ShipEdit, LoopRotateShip, SE_LoopRotateShip) \
	PORT_DEFN(ShipEdit, StartRotateLoop, SE_StartRotateLoop) \
	PORT_DEFN(ShipEdit, StopRotateLoop, SE_StopRotateLoop) \
	PORT_DEFNK(ShipEdit, DrawBackground, ShipEdit_DrawBackground) \
	PORT_DEFN(ShipEdit, AddShipEditTabs, SE_AddShipEditTabs) \
	PORT_DEFN(ShipEdit, AddDefaultPanelContent, SE_AddDefaultPanelContent) \
	PORT_DEFN(ShipEdit, ChangeTabLeft, SE_ChangeTabLeft) \
	PORT_DEFN(ShipEdit, ChangeTabRight, SE_ChangeTabRight) \
	PORT_DEFNK(ShipEdit, ChangeTab, SE_ChangeTab) \
	PORT_DEFN(ShipEdit, CleanEditedShips, SE_CleanEditedShips) \
	PORT_DEFN(ShipEdit, CreatePlayerShip, SE_CreatePlayerShip) \
	PORT_DEFN(ShipEdit, SetupForEdit, SE_SetupForEdit) \
	PORT_DEFN(ShipEdit, SaveShipInfoInSaveVar, SE_SaveShipInfoInSaveVar) \
	PORT_DEFN(ShipEdit, SaveShipInfoInGVar, SE_SaveShipInfoInGVar) \
	PORT_DEFNK(ShipEdit, CreateShipFromSaveVar, SE_CreateShipFromSaveVar) \
	PORT_DEFN(ShipEdit, AssignShip, SE_AssignShip) \
	PORT_DEFN(ShipEdit, SetupEdit, SE_SetupEdit) \
	PORT_DEFN(ShipEdit, FreeEdit, SE_FreeEdit) \
	PORT_DEFN(ShipEdit, EnterEditShipMenu, SE_EnterEditShipMenu) \
	PORT_DEFN(ShipEdit, ExitEditShipMenu, SE_ExitEditShipMenu) \
	PORT_DEFN(ShipEdit, SaveShip, SE_SaveShip) \
	PORT_DEFN(ShipEdit, ShowSpecialHint, SE_ShowSpecialHint)

/* ShipMaker.c */
PORT_DECL(ShipMaker, SetShipPrice) /* 2 */
PORT_DECL(ShipMaker, GetShipPrice) /* 1 */
PORT_DECL(ShipMaker, SetEnginePrice) /* 2 */
PORT_DECL(ShipMaker, GetEnginePrice) /* 1 */
PORT_DECL(ShipMaker, AddShipTon) /* 4 */
PORT_DECL(ShipMaker, GetShipTon) /* 1 */
PORT_DECL(ShipMaker, AddShipBalance) /* 2 */
PORT_DECL(ShipMaker, GetShipBalance) /* 1 */
PORT_DECL(ShipMaker, AddShip) /* ? */
PORT_DECL(ShipMaker, GetShip) /* 1 */
PORT_DECL(ShipMaker, AddMotor) /* 5 */
PORT_DECL(ShipMaker, GetMotor) /* 1 */
PORT_DECL(ShipMaker, IncludeMotor) /* 7 */
PORT_DECL(ShipMaker, AddWeapon) /* 3 */
PORT_DECL(ShipMaker, GetWeapon) /* 1 */
PORT_DECL(ShipMaker, CalculateVerticalBalance) /* 1 */
PORT_DECL(ShipMaker, SetUpChars) /* 1 */
PORT_DECL(ShipMaker, AddChar) /* 2 */
PORT_DECL(ShipMaker, GetBoostFactor) /* 2 */
PORT_DECL(ShipMaker, SetShipParams) /* 1 */
PORT_DECL(ShipMaker, CreateStdShips) /* 0 */
PORT_DECL(ShipMaker, Init) /* 0 */
PORT_DECL(ShipMaker, SetAnmShip) /* 2 */
PORT_DECL(ShipMaker, InitAnmShip) /* ? */
#define PORTS_ShipMaker \
	PORT_DEFN(ShipMaker, SetShipPrice, ShipMaker_SetShipPrice) \
	PORT_DEFN(ShipMaker, GetShipPrice, ShipMaker_GetShipPrice) \
	PORT_DEFN(ShipMaker, SetEnginePrice, ShipMaker_SetEnginePrice) \
	PORT_DEFN(ShipMaker, GetEnginePrice, ShipMaker_GetEnginePrice) \
	PORT_DEFN(ShipMaker, AddShipTon, ShipMaker_AddShipTon) \
	PORT_DEFN(ShipMaker, GetShipTon, ShipMaker_GetShipTon) \
	PORT_DEFN(ShipMaker, AddShipBalance, ShipMaker_AddShipBalance) \
	PORT_DEFN(ShipMaker, GetShipBalance, ShipMaker_GetShipBalance) \
	PORT_DEFNK(ShipMaker, AddShip, ShipMaker_AddShip) \
	PORT_DEFN(ShipMaker, GetShip, ShipMaker_GetShip) \
	PORT_DEFN(ShipMaker, AddMotor, ShipMaker_AddMotor) \
	PORT_DEFN(ShipMaker, GetMotor, ShipMaker_GetMotor) \
	PORT_DEFN(ShipMaker, IncludeMotor, ShipMaker_IncludeMotor) \
	PORT_DEFN(ShipMaker, AddWeapon, ShipMaker_AddWeapon) \
	PORT_DEFN(ShipMaker, GetWeapon, ShipMaker_GetWeapon) \
	PORT_DEFN(ShipMaker, CalculateVerticalBalance, ShipMaker_CalculateVerticalBalance) \
	PORT_DEFN(ShipMaker, SetUpChars, ShipMaker_SetUpChars) \
	PORT_DEFN(ShipMaker, AddChar, ShipMaker_AddChar) \
	PORT_DEFN(ShipMaker, GetBoostFactor, ShipMaker_GetBoostFactor) \
	PORT_DEFN(ShipMaker, SetShipParams, ShipMaker_SetShipParams) \
	PORT_DEFN(ShipMaker, CreateStdShips, ShipMaker_CreateStdShips) \
	PORT_DEFN(ShipMaker, Init, ShipMaker_Init) \
	PORT_DEFN(ShipMaker, SetAnmShip, ShipMaker_SetAnmShip) \
	PORT_DEFNK(ShipMaker, InitAnmShip, ShipMaker_InitAnmShip)

/* Sounds.c */
PORT_DECL(BankDirectorSound, LoadSounds) /* ? */
PORT_DECL(BankDirectorSound, SetSounds) /* ? */
PORT_DECL(BankDirectorSound, TalkSnd) /* ? */
PORT_DECL(BankDirectorSound, Talk0Snd) /* ? */
PORT_DECL(BankDirectorSound, Talk1Snd) /* ? */
PORT_DECL(BankDirectorSound, Talk2Snd) /* ? */
PORT_DECL(BankDirectorSound, InitLoops) /* 1 */
PORT_DECL(BankDirectorSound, Init) /* 1 */
PORT_DECL(BankMasterSound, LoadSounds) /* ? */
PORT_DECL(BankMasterSound, SetSounds) /* ? */
PORT_DECL(BankMasterSound, TalkSnd) /* ? */
PORT_DECL(BankMasterSound, Talk0Snd) /* ? */
PORT_DECL(BankMasterSound, Talk1Snd) /* ? */
PORT_DECL(BankMasterSound, InitLoops) /* 1 */
PORT_DECL(BankMasterSound, Init) /* 1 */
PORT_DECL(BertoSound, LoadSounds) /* ? */
PORT_DECL(BertoSound, SetSounds) /* ? */
PORT_DECL(BertoSound, TalkSnd) /* ? */
PORT_DECL(BertoSound, Talk0Snd) /* ? */
PORT_DECL(BertoSound, Talk1Snd) /* ? */
PORT_DECL(BertoSound, Talk2Snd) /* ? */
PORT_DECL(BertoSound, InitLoops) /* 1 */
PORT_DECL(BertoSound, Init) /* 1 */
PORT_DECL(BettySound, LoadSounds) /* ? */
PORT_DECL(BettySound, SetSounds) /* ? */
PORT_DECL(BettySound, TalkSnd) /* ? */
PORT_DECL(BettySound, Talk0Snd) /* ? */
PORT_DECL(BettySound, Talk1Snd) /* ? */
PORT_DECL(BettySound, Talk2Snd) /* ? */
PORT_DECL(BettySound, InitLoops) /* 1 */
PORT_DECL(BettySound, Init) /* 1 */
PORT_DECL(BishopSound, LoadSounds) /* ? */
PORT_DECL(BishopSound, SetSounds) /* ? */
PORT_DECL(BishopSound, TalkSnd) /* ? */
PORT_DECL(BishopSound, Talk0Snd) /* ? */
PORT_DECL(BishopSound, Talk1Snd) /* ? */
PORT_DECL(BishopSound, Talk2Snd) /* ? */
PORT_DECL(BishopSound, InitLoops) /* 1 */
PORT_DECL(BishopSound, Init) /* 1 */
PORT_DECL(BossSound, LoadSounds) /* ? */
PORT_DECL(BossSound, SetSounds) /* ? */
PORT_DECL(BossSound, TalkSnd) /* ? */
PORT_DECL(BossSound, Talk0Snd) /* ? */
PORT_DECL(BossSound, Talk1Snd) /* ? */
PORT_DECL(BossSound, Talk2Snd) /* ? */
PORT_DECL(BossSound, InitLoops) /* 1 */
PORT_DECL(BossSound, Init) /* 1 */
PORT_DECL(CrazyGamblerSound, LoadSounds) /* ? */
PORT_DECL(CrazyGamblerSound, SetSounds) /* ? */
PORT_DECL(CrazyGamblerSound, TalkSnd) /* ? */
PORT_DECL(CrazyGamblerSound, Talk0Snd) /* ? */
PORT_DECL(CrazyGamblerSound, Talk1Snd) /* ? */
PORT_DECL(CrazyGamblerSound, Talk2Snd) /* ? */
PORT_DECL(CrazyGamblerSound, InitLoops) /* 1 */
PORT_DECL(CrazyGamblerSound, Init) /* 1 */
PORT_DECL(DesktopSound, LoadSounds) /* ? */
PORT_DECL(DesktopSound, SetSounds) /* ? */
PORT_DECL(DesktopSound, TalkSnd) /* ? */
PORT_DECL(DesktopSound, Talk0Snd) /* ? */
PORT_DECL(DesktopSound, InitLoops) /* 1 */
PORT_DECL(DesktopSound, Init) /* 1 */
PORT_DECL(DtritusSound, LoadSounds) /* ? */
PORT_DECL(DtritusSound, SetSounds) /* ? */
PORT_DECL(DtritusSound, TalkSnd) /* ? */
PORT_DECL(DtritusSound, Talk0Snd) /* ? */
PORT_DECL(DtritusSound, Talk1Snd) /* ? */
PORT_DECL(DtritusSound, Talk2Snd) /* ? */
PORT_DECL(DtritusSound, Talk3Snd) /* ? */
PORT_DECL(DtritusSound, Talk4Snd) /* ? */
PORT_DECL(DtritusSound, Talk5Snd) /* ? */
PORT_DECL(DtritusSound, InitLoops) /* 1 */
PORT_DECL(DtritusSound, Init) /* 1 */
PORT_DECL(FunctionarySound, LoadSounds) /* ? */
PORT_DECL(FunctionarySound, SetSounds) /* ? */
PORT_DECL(FunctionarySound, TalkSnd) /* ? */
PORT_DECL(FunctionarySound, Talk0Snd) /* ? */
PORT_DECL(FunctionarySound, Talk2Snd) /* ? */
PORT_DECL(FunctionarySound, Talk1Snd) /* ? */
PORT_DECL(FunctionarySound, InitLoops) /* 1 */
PORT_DECL(FunctionarySound, Init) /* 1 */
PORT_DECL(GateKeeperSound, LoadSounds) /* ? */
PORT_DECL(GateKeeperSound, SetSounds) /* ? */
PORT_DECL(GateKeeperSound, TalkSnd) /* ? */
PORT_DECL(GateKeeperSound, TalkNoSnd) /* ? */
PORT_DECL(GateKeeperSound, InitLoops) /* 1 */
PORT_DECL(GateKeeperSound, Init) /* 1 */
PORT_DECL(GearSound, LoadSounds) /* ? */
PORT_DECL(GearSound, SetSounds) /* ? */
PORT_DECL(GearSound, TalkSnd) /* ? */
PORT_DECL(GearSound, Init) /* 1 */
PORT_DECL(GearSound, InitLoops) /* 1 */
PORT_DECL(HumanSound, LoadSounds) /* ? */
PORT_DECL(HumanSound, SetSounds) /* ? */
PORT_DECL(HumanSound, TalkSnd) /* ? */
PORT_DECL(HumanSound, Talk0Snd) /* ? */
PORT_DECL(HumanSound, Init) /* 1 */
PORT_DECL(HumphreySound, LoadSounds) /* ? */
PORT_DECL(HumphreySound, SetSounds) /* ? */
PORT_DECL(HumphreySound, TalkSnd) /* ? */
PORT_DECL(HumphreySound, Talk0Snd) /* ? */
PORT_DECL(HumphreySound, Talk1Snd) /* ? */
PORT_DECL(HumphreySound, Talk2Snd) /* ? */
PORT_DECL(HumphreySound, Init) /* 1 */
PORT_DECL(MaintenanceSound, LoadSounds) /* ? */
PORT_DECL(MaintenanceSound, SetSounds) /* ? */
PORT_DECL(MaintenanceSound, TalkSnd) /* ? */
PORT_DECL(MaintenanceSound, InitLoops) /* 1 */
PORT_DECL(MaintenanceSound, Init) /* 1 */
PORT_DECL(MayorSound, LoadSounds) /* ? */
PORT_DECL(MayorSound, SetSounds) /* ? */
PORT_DECL(MayorSound, TalkSnd) /* ? */
PORT_DECL(MayorSound, Talk0Snd) /* ? */
PORT_DECL(MayorSound, Talk1Snd) /* ? */
PORT_DECL(MayorSound, Talk2Snd) /* ? */
PORT_DECL(MayorSound, InitLoops) /* 1 */
PORT_DECL(MayorSound, Init) /* 1 */
PORT_DECL(MercenarySound, LoadSounds) /* ? */
PORT_DECL(MercenarySound, SetSounds) /* ? */
PORT_DECL(MercenarySound, Init) /* 2 */
PORT_DECL(MessengerSound, LoadSounds) /* ? */
PORT_DECL(MessengerSound, SetSounds) /* ? */
PORT_DECL(MessengerSound, TalkSnd) /* ? */
PORT_DECL(MessengerSound, Talk0Snd) /* ? */
PORT_DECL(MessengerSound, Talk1Snd) /* ? */
PORT_DECL(MessengerSound, InitLoops) /* 1 */
PORT_DECL(MessengerSound, Init) /* 1 */
PORT_DECL(NurseSound, LoadSounds) /* ? */
PORT_DECL(NurseSound, SetSounds) /* ? */
PORT_DECL(NurseSound, TalkSnd) /* ? */
PORT_DECL(NurseSound, Talk0Snd) /* ? */
PORT_DECL(NurseSound, Talk1Snd) /* ? */
PORT_DECL(NurseSound, InitLoops) /* 1 */
PORT_DECL(NurseSound, Init) /* 1 */
PORT_DECL(PoliceBossSound, LoadSounds) /* ? */
PORT_DECL(PoliceBossSound, SetSounds) /* ? */
PORT_DECL(PoliceBossSound, TalkSnd) /* ? */
PORT_DECL(PoliceBossSound, Talk0Snd) /* ? */
PORT_DECL(PoliceBossSound, Talk1Snd) /* ? */
PORT_DECL(PoliceBossSound, Talk2Snd) /* ? */
PORT_DECL(PoliceBossSound, Init) /* 1 */
PORT_DECL(PoliceSound, LoadSounds) /* ? */
PORT_DECL(PoliceSound, SetSounds) /* ? */
PORT_DECL(PoliceSound, TalkSnd) /* ? */
PORT_DECL(PoliceSound, Talk0Snd) /* ? */
PORT_DECL(PoliceSound, Talk1Snd) /* ? */
PORT_DECL(PoliceSound, Talk2Snd) /* ? */
PORT_DECL(PoliceSound, InitLoops) /* 1 */
PORT_DECL(PoliceSound, Init) /* 1 */
PORT_DECL(RustySound, LoadSounds) /* ? */
PORT_DECL(RustySound, SetSounds) /* ? */
PORT_DECL(RustySound, TalkSnd) /* ? */
PORT_DECL(RustySound, Talk0Snd) /* ? */
PORT_DECL(RustySound, Talk1Snd) /* ? */
PORT_DECL(RustySound, Talk2Snd) /* ? */
PORT_DECL(RustySound, InitLoops) /* 1 */
PORT_DECL(RustySound, Init) /* 1 */
PORT_DECL(SebastianSound, LoadSounds) /* ? */
PORT_DECL(SebastianSound, SetSounds) /* ? */
PORT_DECL(SebastianSound, TalkSnd) /* ? */
PORT_DECL(SebastianSound, Talk1Snd) /* ? */
PORT_DECL(SebastianSound, Talk2Snd) /* ? */
PORT_DECL(SebastianSound, Talk3Snd) /* ? */
PORT_DECL(SebastianSound, Talk4Snd) /* ? */
PORT_DECL(SebastianSound, InitLoops) /* 1 */
PORT_DECL(SebastianSound, Init) /* 1 */
PORT_DECL(SentinelSound, LoadSounds) /* ? */
PORT_DECL(SentinelSound, SetSounds) /* ? */
PORT_DECL(SentinelSound, TalkSnd) /* ? */
PORT_DECL(SentinelSound, Talk0Snd) /* ? */
PORT_DECL(SentinelSound, Talk1Snd) /* ? */
PORT_DECL(SentinelSound, InitLoops) /* 1 */
PORT_DECL(SentinelSound, Init) /* 1 */
PORT_DECL(SputnikJunkyardSound, LoadSounds) /* ? */
PORT_DECL(SputnikJunkyardSound, SetSounds) /* ? */
PORT_DECL(SputnikJunkyardSound, TalkSnd) /* ? */
PORT_DECL(SputnikJunkyardSound, InitLoops) /* 1 */
PORT_DECL(SputnikJunkyardSound, Init) /* 1 */
PORT_DECL(SputnikSound, LoadSounds) /* ? */
PORT_DECL(SputnikSound, SetSounds) /* ? */
PORT_DECL(SputnikSound, TalkSnd) /* ? */
PORT_DECL(SputnikSound, InitLoops) /* 1 */
PORT_DECL(SputnikSound, Init) /* 1 */
PORT_DECL(TestCSound, LoadSounds) /* ? */
PORT_DECL(TestCSound, SetSounds) /* ? */
PORT_DECL(TestCSound, TalkSnd) /* ? */
PORT_DECL(TestCSound, InitLoops) /* 1 */
PORT_DECL(TestCSound, Init) /* 1 */
#define PORTS_Sounds \
	PORT_DEFNK(BankDirectorSound, LoadSounds, BankDirectorSound_LoadSounds) \
	PORT_DEFNK(BankDirectorSound, SetSounds, BankDirectorSound_SetSounds) \
	PORT_DEFNK(BankDirectorSound, TalkSnd, BankDirectorSound_TalkSnd) \
	PORT_DEFNK(BankDirectorSound, Talk0Snd, BankDirectorSound_Talk0Snd) \
	PORT_DEFNK(BankDirectorSound, Talk1Snd, BankDirectorSound_Talk1Snd) \
	PORT_DEFNK(BankDirectorSound, Talk2Snd, BankDirectorSound_Talk2Snd) \
	PORT_DEFN(BankDirectorSound, InitLoops, BankDirectorSound_InitLoops) \
	PORT_DEFN(BankDirectorSound, Init, BankDirectorSound_Init) \
	PORT_DEFNK(BankMasterSound, LoadSounds, BankMasterSound_LoadSounds) \
	PORT_DEFNK(BankMasterSound, SetSounds, BankMasterSound_SetSounds) \
	PORT_DEFNK(BankMasterSound, TalkSnd, BankMasterSound_TalkSnd) \
	PORT_DEFNK(BankMasterSound, Talk0Snd, BankMasterSound_Talk0Snd) \
	PORT_DEFNK(BankMasterSound, Talk1Snd, BankMasterSound_Talk1Snd) \
	PORT_DEFN(BankMasterSound, InitLoops, BankMasterSound_InitLoops) \
	PORT_DEFN(BankMasterSound, Init, BankMasterSound_Init) \
	PORT_DEFNK(BertoSound, LoadSounds, BertoSound_LoadSounds) \
	PORT_DEFNK(BertoSound, SetSounds, BertoSound_SetSounds) \
	PORT_DEFNK(BertoSound, TalkSnd, BertoSound_TalkSnd) \
	PORT_DEFNK(BertoSound, Talk0Snd, BertoSound_Talk0Snd) \
	PORT_DEFNK(BertoSound, Talk1Snd, BertoSound_Talk1Snd) \
	PORT_DEFNK(BertoSound, Talk2Snd, BertoSound_Talk2Snd) \
	PORT_DEFN(BertoSound, InitLoops, BertoSound_InitLoops) \
	PORT_DEFN(BertoSound, Init, BertoSound_Init) \
	PORT_DEFNK(BettySound, LoadSounds, BettySound_LoadSounds) \
	PORT_DEFNK(BettySound, SetSounds, BettySound_SetSounds) \
	PORT_DEFNK(BettySound, TalkSnd, BettySound_TalkSnd) \
	PORT_DEFNK(BettySound, Talk0Snd, BettySound_Talk0Snd) \
	PORT_DEFNK(BettySound, Talk1Snd, BettySound_Talk1Snd) \
	PORT_DEFNK(BettySound, Talk2Snd, BettySound_Talk2Snd) \
	PORT_DEFN(BettySound, InitLoops, BettySound_InitLoops) \
	PORT_DEFN(BettySound, Init, BettySound_Init) \
	PORT_DEFNK(BishopSound, LoadSounds, BishopSound_LoadSounds) \
	PORT_DEFNK(BishopSound, SetSounds, BishopSound_SetSounds) \
	PORT_DEFNK(BishopSound, TalkSnd, BishopSound_TalkSnd) \
	PORT_DEFNK(BishopSound, Talk0Snd, BishopSound_Talk0Snd) \
	PORT_DEFNK(BishopSound, Talk1Snd, BishopSound_Talk1Snd) \
	PORT_DEFNK(BishopSound, Talk2Snd, BishopSound_Talk2Snd) \
	PORT_DEFN(BishopSound, InitLoops, BishopSound_InitLoops) \
	PORT_DEFN(BishopSound, Init, BishopSound_Init) \
	PORT_DEFNK(BossSound, LoadSounds, BossSound_LoadSounds) \
	PORT_DEFNK(BossSound, SetSounds, BossSound_SetSounds) \
	PORT_DEFNK(BossSound, TalkSnd, BossSound_TalkSnd) \
	PORT_DEFNK(BossSound, Talk0Snd, BossSound_Talk0Snd) \
	PORT_DEFNK(BossSound, Talk1Snd, BossSound_Talk1Snd) \
	PORT_DEFNK(BossSound, Talk2Snd, BossSound_Talk2Snd) \
	PORT_DEFN(BossSound, InitLoops, BossSound_InitLoops) \
	PORT_DEFN(BossSound, Init, BossSound_Init) \
	PORT_DEFNK(CrazyGamblerSound, LoadSounds, CrazyGamblerSound_LoadSounds) \
	PORT_DEFNK(CrazyGamblerSound, SetSounds, CrazyGamblerSound_SetSounds) \
	PORT_DEFNK(CrazyGamblerSound, TalkSnd, CrazyGamblerSound_TalkSnd) \
	PORT_DEFNK(CrazyGamblerSound, Talk0Snd, CrazyGamblerSound_Talk0Snd) \
	PORT_DEFNK(CrazyGamblerSound, Talk1Snd, CrazyGamblerSound_Talk1Snd) \
	PORT_DEFNK(CrazyGamblerSound, Talk2Snd, CrazyGamblerSound_Talk2Snd) \
	PORT_DEFN(CrazyGamblerSound, InitLoops, CrazyGamblerSound_InitLoops) \
	PORT_DEFN(CrazyGamblerSound, Init, CrazyGamblerSound_Init) \
	PORT_DEFNK(DesktopSound, LoadSounds, DesktopSound_LoadSounds) \
	PORT_DEFNK(DesktopSound, SetSounds, DesktopSound_SetSounds) \
	PORT_DEFNK(DesktopSound, TalkSnd, DesktopSound_TalkSnd) \
	PORT_DEFNK(DesktopSound, Talk0Snd, DesktopSound_Talk0Snd) \
	PORT_DEFN(DesktopSound, InitLoops, DesktopSound_InitLoops) \
	PORT_DEFN(DesktopSound, Init, DesktopSound_Init) \
	PORT_DEFNK(DtritusSound, LoadSounds, DtritusSound_LoadSounds) \
	PORT_DEFNK(DtritusSound, SetSounds, DtritusSound_SetSounds) \
	PORT_DEFNK(DtritusSound, TalkSnd, DtritusSound_TalkSnd) \
	PORT_DEFNK(DtritusSound, Talk0Snd, DtritusSound_Talk0Snd) \
	PORT_DEFNK(DtritusSound, Talk1Snd, DtritusSound_Talk1Snd) \
	PORT_DEFNK(DtritusSound, Talk2Snd, DtritusSound_Talk2Snd) \
	PORT_DEFNK(DtritusSound, Talk3Snd, DtritusSound_Talk3Snd) \
	PORT_DEFNK(DtritusSound, Talk4Snd, DtritusSound_Talk4Snd) \
	PORT_DEFNK(DtritusSound, Talk5Snd, DtritusSound_Talk5Snd) \
	PORT_DEFN(DtritusSound, InitLoops, DtritusSound_InitLoops) \
	PORT_DEFN(DtritusSound, Init, DtritusSound_Init) \
	PORT_DEFNK(FunctionarySound, LoadSounds, FunctionarySound_LoadSounds) \
	PORT_DEFNK(FunctionarySound, SetSounds, FunctionarySound_SetSounds) \
	PORT_DEFNK(FunctionarySound, TalkSnd, FunctionarySound_TalkSnd) \
	PORT_DEFNK(FunctionarySound, Talk0Snd, FunctionarySound_Talk0Snd) \
	PORT_DEFNK(FunctionarySound, Talk2Snd, FunctionarySound_Talk2Snd) \
	PORT_DEFNK(FunctionarySound, Talk1Snd, FunctionarySound_Talk1Snd) \
	PORT_DEFN(FunctionarySound, InitLoops, FunctionarySound_InitLoops) \
	PORT_DEFN(FunctionarySound, Init, FunctionarySound_Init) \
	PORT_DEFNK(GateKeeperSound, LoadSounds, GateKeeperSound_LoadSounds) \
	PORT_DEFNK(GateKeeperSound, SetSounds, GateKeeperSound_SetSounds) \
	PORT_DEFNK(GateKeeperSound, TalkSnd, GateKeeperSound_TalkSnd) \
	PORT_DEFNK(GateKeeperSound, TalkNoSnd, GateKeeperSound_TalkNoSnd) \
	PORT_DEFN(GateKeeperSound, InitLoops, GateKeeperSound_InitLoops) \
	PORT_DEFN(GateKeeperSound, Init, GateKeeperSound_Init) \
	PORT_DEFNK(GearSound, LoadSounds, GearSound_LoadSounds) \
	PORT_DEFNK(GearSound, SetSounds, GearSound_SetSounds) \
	PORT_DEFNK(GearSound, TalkSnd, GearSound_TalkSnd) \
	PORT_DEFN(GearSound, Init, GearSound_Init) \
	PORT_DEFN(GearSound, InitLoops, GearSound_InitLoops) \
	PORT_DEFNK(HumanSound, LoadSounds, HumanSound_LoadSounds) \
	PORT_DEFNK(HumanSound, SetSounds, HumanSound_SetSounds) \
	PORT_DEFNK(HumanSound, TalkSnd, HumanSound_TalkSnd) \
	PORT_DEFNK(HumanSound, Talk0Snd, HumanSound_Talk0Snd) \
	PORT_DEFN(HumanSound, Init, HumanSound_Init) \
	PORT_DEFNK(HumphreySound, LoadSounds, HumphreySound_LoadSounds) \
	PORT_DEFNK(HumphreySound, SetSounds, HumphreySound_SetSounds) \
	PORT_DEFNK(HumphreySound, TalkSnd, HumphreySound_TalkSnd) \
	PORT_DEFNK(HumphreySound, Talk0Snd, HumphreySound_Talk0Snd) \
	PORT_DEFNK(HumphreySound, Talk1Snd, HumphreySound_Talk1Snd) \
	PORT_DEFNK(HumphreySound, Talk2Snd, HumphreySound_Talk2Snd) \
	PORT_DEFN(HumphreySound, Init, HumphreySound_Init) \
	PORT_DEFNK(MaintenanceSound, LoadSounds, MaintenanceSound_LoadSounds) \
	PORT_DEFNK(MaintenanceSound, SetSounds, MaintenanceSound_SetSounds) \
	PORT_DEFNK(MaintenanceSound, TalkSnd, MaintenanceSound_TalkSnd) \
	PORT_DEFN(MaintenanceSound, InitLoops, MaintenanceSound_InitLoops) \
	PORT_DEFN(MaintenanceSound, Init, MaintenanceSound_Init) \
	PORT_DEFNK(MayorSound, LoadSounds, MayorSound_LoadSounds) \
	PORT_DEFNK(MayorSound, SetSounds, MayorSound_SetSounds) \
	PORT_DEFNK(MayorSound, TalkSnd, MayorSound_TalkSnd) \
	PORT_DEFNK(MayorSound, Talk0Snd, MayorSound_Talk0Snd) \
	PORT_DEFNK(MayorSound, Talk1Snd, MayorSound_Talk1Snd) \
	PORT_DEFNK(MayorSound, Talk2Snd, MayorSound_Talk2Snd) \
	PORT_DEFN(MayorSound, InitLoops, MayorSound_InitLoops) \
	PORT_DEFN(MayorSound, Init, MayorSound_Init) \
	PORT_DEFNK(MercenarySound, LoadSounds, MercenarySound_LoadSounds) \
	PORT_DEFNK(MercenarySound, SetSounds, MercenarySound_SetSounds) \
	PORT_DEFN(MercenarySound, Init, MercenarySound_Init) \
	PORT_DEFNK(MessengerSound, LoadSounds, MessengerSound_LoadSounds) \
	PORT_DEFNK(MessengerSound, SetSounds, MessengerSound_SetSounds) \
	PORT_DEFNK(MessengerSound, TalkSnd, MessengerSound_TalkSnd) \
	PORT_DEFNK(MessengerSound, Talk0Snd, MessengerSound_Talk0Snd) \
	PORT_DEFNK(MessengerSound, Talk1Snd, MessengerSound_Talk1Snd) \
	PORT_DEFN(MessengerSound, InitLoops, MessengerSound_InitLoops) \
	PORT_DEFN(MessengerSound, Init, MessengerSound_Init) \
	PORT_DEFNK(NurseSound, LoadSounds, NurseSound_LoadSounds) \
	PORT_DEFNK(NurseSound, SetSounds, NurseSound_SetSounds) \
	PORT_DEFNK(NurseSound, TalkSnd, NurseSound_TalkSnd) \
	PORT_DEFNK(NurseSound, Talk0Snd, NurseSound_Talk0Snd) \
	PORT_DEFNK(NurseSound, Talk1Snd, NurseSound_Talk1Snd) \
	PORT_DEFN(NurseSound, InitLoops, NurseSound_InitLoops) \
	PORT_DEFN(NurseSound, Init, NurseSound_Init) \
	PORT_DEFNK(PoliceBossSound, LoadSounds, PoliceBossSound_LoadSounds) \
	PORT_DEFNK(PoliceBossSound, SetSounds, PoliceBossSound_SetSounds) \
	PORT_DEFNK(PoliceBossSound, TalkSnd, PoliceBossSound_TalkSnd) \
	PORT_DEFNK(PoliceBossSound, Talk0Snd, PoliceBossSound_Talk0Snd) \
	PORT_DEFNK(PoliceBossSound, Talk1Snd, PoliceBossSound_Talk1Snd) \
	PORT_DEFNK(PoliceBossSound, Talk2Snd, PoliceBossSound_Talk2Snd) \
	PORT_DEFN(PoliceBossSound, Init, PoliceBossSound_Init) \
	PORT_DEFNK(PoliceSound, LoadSounds, PoliceSound_LoadSounds) \
	PORT_DEFNK(PoliceSound, SetSounds, PoliceSound_SetSounds) \
	PORT_DEFNK(PoliceSound, TalkSnd, PoliceSound_TalkSnd) \
	PORT_DEFNK(PoliceSound, Talk0Snd, PoliceSound_Talk0Snd) \
	PORT_DEFNK(PoliceSound, Talk1Snd, PoliceSound_Talk1Snd) \
	PORT_DEFNK(PoliceSound, Talk2Snd, PoliceSound_Talk2Snd) \
	PORT_DEFN(PoliceSound, InitLoops, PoliceSound_InitLoops) \
	PORT_DEFN(PoliceSound, Init, PoliceSound_Init) \
	PORT_DEFNK(RustySound, LoadSounds, RustySound_LoadSounds) \
	PORT_DEFNK(RustySound, SetSounds, RustySound_SetSounds) \
	PORT_DEFNK(RustySound, TalkSnd, RustySound_TalkSnd) \
	PORT_DEFNK(RustySound, Talk0Snd, RustySound_Talk0Snd) \
	PORT_DEFNK(RustySound, Talk1Snd, RustySound_Talk1Snd) \
	PORT_DEFNK(RustySound, Talk2Snd, RustySound_Talk2Snd) \
	PORT_DEFN(RustySound, InitLoops, RustySound_InitLoops) \
	PORT_DEFN(RustySound, Init, RustySound_Init) \
	PORT_DEFNK(SebastianSound, LoadSounds, SebastianSound_LoadSounds) \
	PORT_DEFNK(SebastianSound, SetSounds, SebastianSound_SetSounds) \
	PORT_DEFNK(SebastianSound, TalkSnd, SebastianSound_TalkSnd) \
	PORT_DEFNK(SebastianSound, Talk1Snd, SebastianSound_Talk1Snd) \
	PORT_DEFNK(SebastianSound, Talk2Snd, SebastianSound_Talk2Snd) \
	PORT_DEFNK(SebastianSound, Talk3Snd, SebastianSound_Talk3Snd) \
	PORT_DEFNK(SebastianSound, Talk4Snd, SebastianSound_Talk4Snd) \
	PORT_DEFN(SebastianSound, InitLoops, SebastianSound_InitLoops) \
	PORT_DEFN(SebastianSound, Init, SebastianSound_Init) \
	PORT_DEFNK(SentinelSound, LoadSounds, SentinelSound_LoadSounds) \
	PORT_DEFNK(SentinelSound, SetSounds, SentinelSound_SetSounds) \
	PORT_DEFNK(SentinelSound, TalkSnd, SentinelSound_TalkSnd) \
	PORT_DEFNK(SentinelSound, Talk0Snd, SentinelSound_Talk0Snd) \
	PORT_DEFNK(SentinelSound, Talk1Snd, SentinelSound_Talk1Snd) \
	PORT_DEFN(SentinelSound, InitLoops, SentinelSound_InitLoops) \
	PORT_DEFN(SentinelSound, Init, SentinelSound_Init) \
	PORT_DEFNK(SputnikJunkyardSound, LoadSounds, SputnikJunkyardSound_LoadSounds) \
	PORT_DEFNK(SputnikJunkyardSound, SetSounds, SputnikJunkyardSound_SetSounds) \
	PORT_DEFNK(SputnikJunkyardSound, TalkSnd, SputnikJunkyardSound_TalkSnd) \
	PORT_DEFN(SputnikJunkyardSound, InitLoops, SputnikJunkyardSound_InitLoops) \
	PORT_DEFN(SputnikJunkyardSound, Init, SputnikJunkyardSound_Init) \
	PORT_DEFNK(SputnikSound, LoadSounds, SputnikSound_LoadSounds) \
	PORT_DEFNK(SputnikSound, SetSounds, SputnikSound_SetSounds) \
	PORT_DEFNK(SputnikSound, TalkSnd, SputnikSound_TalkSnd) \
	PORT_DEFN(SputnikSound, InitLoops, SputnikSound_InitLoops) \
	PORT_DEFN(SputnikSound, Init, SputnikSound_Init) \
	PORT_DEFNK(TestCSound, LoadSounds, TestCSound_LoadSounds) \
	PORT_DEFNK(TestCSound, SetSounds, TestCSound_SetSounds) \
	PORT_DEFNK(TestCSound, TalkSnd, TestCSound_TalkSnd) \
	PORT_DEFN(TestCSound, InitLoops, TestCSound_InitLoops) \
	PORT_DEFN(TestCSound, Init, TestCSound_Init)

/* Speech.c */
PORT_DECL(Speech, ScheduledVoiceCallback) /* 2 */
PORT_DECL(Speech, PlayVoiceScheduled) /* 3 */
PORT_DECL(Speech, PerformAnswer) /* ? */
PORT_DECL(Speech, NormalAbortSpeech) /* ? */
PORT_DECL(Speech, CallEndOfSpeech) /* ? */
PORT_DECL(Speech, AbortSpeech) /* ? */
PORT_DECL(Speech, RestoreState) /* 1 */
PORT_DECL(Speech, AbortNPCSpeech) /* ? */
PORT_DECL(Speech, AutoAnswer) /* ? */
PORT_DECL(Speech, StopMenuEvents) /* ? */
PORT_DECL(Speech, RestartMenuEvents) /* ? */
PORT_DECL(Speech, RestoreMoveTime) /* 1 */
PORT_DECL(Speech, StdAnsLst) /* 1 */
PORT_DECL(Speech, StdOnSpch) /* ? */
PORT_DECL(Speech, CalculateSpeechTime) /* 1 */
PORT_DECL(Speech, OnSpch) /* ? */
PORT_DECL(Speech, OnChallengeSpch) /* ? */
PORT_DECL(Speech, OnSpeech) /* ? */
PORT_DECL(Speech, PlayerAnsEnd) /* 0 */
PORT_DECL(Speech, PlayerAns) /* 4 */
PORT_DECL(Speech, SetPlayerToTalk) /* 2 */
PORT_DECL(Speech, InspectChar) /* 4 */
PORT_DECL(Speech, AutoSpeech) /* 4 */
PORT_DECL(Speech, NISpeech) /* ? */
PORT_DECL(Speech, AutoAddNI) /* ? */
PORT_DECL(Speech, AddNI) /* ? */
PORT_DECL(Speech, RunNI) /* ? */
PORT_DECL(Speech, ObjNIEnd) /* ? */
PORT_DECL(Speech, LaunchTaunt) /* 3 */
PORT_DECL(Speech, RestoreSpeech) /* ? */
PORT_DECL(Speech, DoRemote) /* ? */
PORT_DECL(Speech, OffVoice) /* 1 */
PORT_DECL(Speech, RemoteComunication) /* ? */
PORT_DECL(Speech, ForceRemoteComInternal) /* ? */
PORT_DECL(Speech, ForceRemoteCom) /* ? */
PORT_DECL(Speech, ForceRemoteComunication) /* ? */
PORT_DECL(Speech, AddRemoteCom) /* ? */
PORT_DECL(Speech, ProcessRemoteCom) /* 1 */
PORT_DECL(Speech, CancelRemoteCom) /* ? */
PORT_DECL(Speech, TestInRange) /* 0 */
PORT_DECL(Speech, StartObjSpch) /* ? */
PORT_DECL(Speech, StartObjectSpeech) /* 6 */
PORT_DECL(Speech, StdObjSpch) /* ? */
PORT_DECL(Speech, OnObjSpch) /* ? */
PORT_DECL(Speech, OnChallengeObjSpch) /* ? */
PORT_DECL(Speech, OnObjectSpeech) /* ? */
PORT_DECL(Speech, ObjectAutoAnswer) /* ? */
PORT_DECL(Speech, ObjectAutoAnswer2) /* ? */
PORT_DECL(Speech, EndObjectAnswer) /* 0 */
#define PORTS_Speech \
	PORT_DEFN(Speech, ScheduledVoiceCallback, Speech_ScheduledVoiceCallback) \
	PORT_DEFN(Speech, PlayVoiceScheduled, Speech_PlayVoiceScheduled) \
	PORT_DEFNK(Speech, PerformAnswer, Speech_PerformAnswer) \
	PORT_DEFN(Speech, NormalAbortSpeech, Speech_NormalAbortSpeech) \
	PORT_DEFN(Speech, CallEndOfSpeech, Speech_CallEndOfSpeech) \
	PORT_DEFNK(Speech, AbortSpeech, Speech_AbortSpeech) \
	PORT_DEFN(Speech, RestoreState, Speech_RestoreState) \
	PORT_DEFNK(Speech, AbortNPCSpeech, Speech_AbortNPCSpeech) \
	PORT_DEFN(Speech, AutoAnswer, Speech_AutoAnswer) \
	PORT_DEFN(Speech, StopMenuEvents, Speech_StopMenuEvents) \
	PORT_DEFN(Speech, RestartMenuEvents, Speech_RestartMenuEvents) \
	PORT_DEFN(Speech, RestoreMoveTime, Speech_RestoreMoveTime) \
	PORT_DEFN(Speech, StdAnsLst, Speech_StdAnsLst) \
	PORT_DEFNK(Speech, StdOnSpch, Speech_StdOnSpch) \
	PORT_DEFN(Speech, CalculateSpeechTime, Speech_CalculateSpeechTime) \
	PORT_DEFNK(Speech, OnSpch, Speech_OnSpch) \
	PORT_DEFNK(Speech, OnChallengeSpch, Speech_OnChallengeSpch) \
	PORT_DEFNK(Speech, OnSpeech, Speech_OnSpeech) \
	PORT_DEFN(Speech, PlayerAnsEnd, Speech_PlayerAnsEnd) \
	PORT_DEFN(Speech, PlayerAns, Speech_PlayerAns) \
	PORT_DEFN(Speech, SetPlayerToTalk, Speech_SetPlayerToTalk) \
	PORT_DEFN(Speech, InspectChar, Speech_InspectChar) \
	PORT_DEFN(Speech, AutoSpeech, Speech_AutoSpeech) \
	PORT_DEFNK(Speech, NISpeech, Speech_NISpeech) \
	PORT_DEFNK(Speech, AutoAddNI, Speech_AutoAddNI) \
	PORT_DEFNK(Speech, AddNI, Speech_AddNI) \
	PORT_DEFNK(Speech, RunNI, Speech_RunNI) \
	PORT_DEFN(Speech, ObjNIEnd, Speech_ObjNIEnd) \
	PORT_DEFN(Speech, LaunchTaunt, Speech_LaunchTaunt) \
	PORT_DEFN(Speech, RestoreSpeech, Speech_RestoreSpeech) \
	PORT_DEFN(Speech, DoRemote, Speech_DoRemote) \
	PORT_DEFN(Speech, OffVoice, Speech_OffVoice) \
	PORT_DEFNK(Speech, RemoteComunication, Speech_RemoteComunication) \
	PORT_DEFNK(Speech, ForceRemoteComInternal, Speech_ForceRemoteComInternal) \
	PORT_DEFNK(Speech, ForceRemoteCom, Speech_ForceRemoteCom) \
	PORT_DEFNK(Speech, ForceRemoteComunication, Speech_ForceRemoteComunication) \
	PORT_DEFNK(Speech, AddRemoteCom, Speech_AddRemoteCom) \
	PORT_DEFN(Speech, ProcessRemoteCom, Speech_ProcessRemoteCom) \
	PORT_DEFN(Speech, CancelRemoteCom, Speech_CancelRemoteCom) \
	PORT_DEFN(Speech, TestInRange, Speech_TestInRange) \
	PORT_DEFNK(Speech, StartObjSpch, Speech_StartObjSpch) \
	PORT_DEFN(Speech, StartObjectSpeech, Speech_StartObjectSpeech) \
	PORT_DEFNK(Speech, StdObjSpch, Speech_StdObjSpch) \
	PORT_DEFNK(Speech, OnObjSpch, Speech_OnObjSpch) \
	PORT_DEFNK(Speech, OnChallengeObjSpch, Speech_OnChallengeObjSpch) \
	PORT_DEFNK(Speech, OnObjectSpeech, Speech_OnObjectSpeech) \
	PORT_DEFN(Speech, ObjectAutoAnswer, Speech_ObjectAutoAnswer) \
	PORT_DEFN(Speech, ObjectAutoAnswer2, Speech_ObjectAutoAnswer2) \
	PORT_DEFN(Speech, EndObjectAnswer, Speech_EndObjectAnswer)

/* Split.c */
PORT_DECL(Split, PurgeServerMapList) /* ? */
PORT_DECL(Split, GetSpawnPos) /* 1 */
PORT_DECL(Split, CreateUserShip) /* 1 */
PORT_DECL(Split, Init) /* ? */
PORT_DECL(Split, GetReSpawnPos) /* 1 */
PORT_DECL(Split, ReSpawnShip) /* 1 */
PORT_DECL(Split, OnDeathCallback) /* 2 */
PORT_DECL(Split, ChooseShips) /* ? */
PORT_DECL(Split, ChangeShip) /* ? */
PORT_DECL(Split, NextShip) /* 1 */
PORT_DECL(Split, PrevShip) /* 1 */
PORT_DECL(Split, StartGame) /* ? */
PORT_DECL(Split, DefaultSplitScorer) /* 1 */
PORT_DECL(Split, EndOfGame) /* 1 */
PORT_DECL(Split, ReloadNext) /* ? */
PORT_DECL(Split, ActualizeScorer) /* ? */
PORT_DECL(Split, MissionScorer) /* 1 */
PORT_DECL(Split, SeleShip) /* 1 */
PORT_DECL(Split, InitChooseScorer) /* 1 */
PORT_DECL(Split, ReadyToStartScorer) /* 1 */
PORT_DECL(Split, ConfirmAbortMain) /* 1 */
PORT_DECL(Split, ConfirmAbortPause) /* 1 */
PORT_DECL(Split, ConfirmRestartPause) /* 1 */
PORT_DECL(Split, ConfirmAbort) /* ? */
PORT_DECL(Split, Abort) /* 1 */
PORT_DECL(Split, Restart) /* 1 */
PORT_DECL(Split, PauseGame) /* 1 */
PORT_DECL(Split, PauseGameMenu) /* 1 */
PORT_DECL(Split, LittleText) /* 4 */
PORT_DECL(Split, BlackBack) /* 1 */
PORT_DECL(SDeathMatch, InitStatus) /* ? */
PORT_DECL(SDeathMatch, Init) /* ? */
PORT_DECL(SDeathMatch, OnDeathFunc) /* 2 */
PORT_DECL(SFlagHunt, BrowseAreas) /* ? */
PORT_DECL(SFlagHunt, GetRndFlag) /* ? */
PORT_DECL(SFlagHunt, TakeFlag) /* 2 */
PORT_DECL(SFlagHunt, CreateFlag) /* ? */
PORT_DECL(SFlagHunt, InitStatus) /* ? */
PORT_DECL(SFlagHunt, Init) /* ? */
PORT_DECL(SFlagHunt, OnDeathFunc) /* 2 */
PORT_DECL(SPostman, BrowseAreas) /* ? */
PORT_DECL(SPostman, GetRndFlag) /* ? */
PORT_DECL(SPostman, TakeArea) /* 2 */
PORT_DECL(SPostman, TakeFlag) /* 2 */
PORT_DECL(SPostman, CreateFlag) /* ? */
PORT_DECL(SPostman, InitStatus) /* ? */
PORT_DECL(SPostman, Init) /* ? */
PORT_DECL(SPostman, OnDeathFunc) /* 2 */
PORT_DECL(SPostman, BattonRepos) /* ? */
PORT_DECL(SRace, BrowseAreas) /* ? */
PORT_DECL(SRace, GetRndFlag) /* ? */
PORT_DECL(SRace, TakeFlag) /* 2 */
PORT_DECL(SRace, CreateFlag) /* ? */
PORT_DECL(SRace, InitRacePos) /* 1 */
PORT_DECL(SRace, GetReSpawnPos) /* 1 */
PORT_DECL(SRace, InitStatus) /* ? */
PORT_DECL(SRace, Init) /* ? */
PORT_DECL(SRace, ControlToRacers) /* ? */
PORT_DECL(SRace, OnDeathFunc) /* ? */
#define PORTS_Split \
	PORT_DEFN(Split, PurgeServerMapList, Split_PurgeServerMapList) \
	PORT_DEFN(Split, GetSpawnPos, Split_GetSpawnPos) \
	PORT_DEFN(Split, CreateUserShip, Split_CreateUserShip) \
	PORT_DEFN(Split, Init, Split_Init) \
	PORT_DEFN(Split, GetReSpawnPos, Split_GetSpawnPos) \
	PORT_DEFN(Split, ReSpawnShip, Split_ReSpawnShip) \
	PORT_DEFN(Split, OnDeathCallback, Split_OnDeathCallback) \
	PORT_DEFN(Split, ChooseShips, Split_ChooseShips) \
	PORT_DEFNK(Split, ChangeShip, Split_ChangeShip) \
	PORT_DEFN(Split, NextShip, Split_NextShip) \
	PORT_DEFN(Split, PrevShip, Split_PrevShip) \
	PORT_DEFNK(Split, StartGame, Split_StartGame) \
	PORT_DEFN(Split, DefaultSplitScorer, Split_DefaultSplitScorer) \
	PORT_DEFN(Split, EndOfGame, Split_EndOfGame) \
	PORT_DEFN(Split, ReloadNext, Split_ReloadNext) \
	PORT_DEFN(Split, ActualizeScorer, Split_ActualizeScorer) \
	PORT_DEFN(Split, MissionScorer, Split_MissionScorer) \
	PORT_DEFN(Split, SeleShip, Split_SeleShip) \
	PORT_DEFN(Split, InitChooseScorer, Split_InitChooseScorer) \
	PORT_DEFN(Split, ReadyToStartScorer, Split_ReadyToStartScorer) \
	PORT_DEFN(Split, ConfirmAbortMain, Split_ConfirmAbortMain) \
	PORT_DEFN(Split, ConfirmAbortPause, Split_ConfirmAbortPause) \
	PORT_DEFN(Split, ConfirmRestartPause, Split_ConfirmRestartPause) \
	PORT_DEFNK(Split, ConfirmAbort, Split_ConfirmAbort) \
	PORT_DEFN(Split, Abort, Split_Abort) \
	PORT_DEFN(Split, Restart, Split_Restart) \
	PORT_DEFN(Split, PauseGame, Split_PauseGame) \
	PORT_DEFN(Split, PauseGameMenu, Split_PauseGameMenu) \
	PORT_DEFN(Split, LittleText, Split_LittleText) \
	PORT_DEFN(Split, BlackBack, Split_BlackBack) \
	PORT_DEFN(SDeathMatch, InitStatus, Mode_PyNone) \
	PORT_DEFN(SDeathMatch, Init, SDeathMatch_Init) \
	PORT_DEFN(SDeathMatch, OnDeathFunc, SDeathMatch_OnDeathFunc) \
	PORT_DEFN(SFlagHunt, BrowseAreas, SFlagHunt_BrowseAreas) \
	PORT_DEFN(SFlagHunt, GetRndFlag, SFlagHunt_GetRndFlag) \
	PORT_DEFN(SFlagHunt, TakeFlag, SFlagHunt_TakeFlag) \
	PORT_DEFN(SFlagHunt, CreateFlag, SFlagHunt_CreateFlag) \
	PORT_DEFN(SFlagHunt, InitStatus, SFlagHunt_InitStatus) \
	PORT_DEFN(SFlagHunt, Init, SFlagHunt_Init) \
	PORT_DEFN(SFlagHunt, OnDeathFunc, SFlagHunt_OnDeathFunc) \
	PORT_DEFN(SPostman, BrowseAreas, SPostman_BrowseAreas) \
	PORT_DEFN(SPostman, GetRndFlag, SPostman_GetRndFlag) \
	PORT_DEFN(SPostman, TakeArea, SPostman_TakeArea) \
	PORT_DEFN(SPostman, TakeFlag, SPostman_TakeFlag) \
	PORT_DEFN(SPostman, CreateFlag, SPostman_CreateFlag) \
	PORT_DEFN(SPostman, InitStatus, SPostman_InitStatus) \
	PORT_DEFN(SPostman, Init, SPostman_Init) \
	PORT_DEFN(SPostman, OnDeathFunc, SPostman_OnDeathFunc) \
	PORT_DEFN(SPostman, BattonRepos, SPostman_BattonRepos) \
	PORT_DEFN(SRace, BrowseAreas, SRace_BrowseAreas) \
	PORT_DEFN(SRace, GetRndFlag, SRace_GetRndFlag) \
	PORT_DEFN(SRace, TakeFlag, SRace_TakeFlag) \
	PORT_DEFN(SRace, CreateFlag, SRace_CreateFlag) \
	PORT_DEFN(SRace, InitRacePos, SRace_InitRacePos) \
	PORT_DEFN(SRace, GetReSpawnPos, SRace_GetReSpawnPos) \
	PORT_DEFN(SRace, InitStatus, SRace_InitStatus) \
	PORT_DEFN(SRace, Init, SRace_Init) \
	PORT_DEFN(SRace, ControlToRacers, SRace_ControlToRacers) \
	PORT_DEFN(SRace, OnDeathFunc, Mode_PyNone2)

/* Sputnik.c */
PORT_DECL(Sputnik, Assign) /* ? */
PORT_DECL(Sputnik, ChangeRelax) /* 3 */
PORT_DECL(Sputnik, ByeBye) /* 3 */
PORT_DECL(Sputnik, BackCamera) /* 3 */
PORT_DECL(Sputnik, UseShip) /* ? */
PORT_DECL(Sputnik, ChangeShip) /* 4 */
PORT_DECL(Sputnik, CreateShip) /* 3 */
PORT_DECL(Sputnik, ShowInterface) /* 3 */
PORT_DECL(Sputnik, EnterInterface) /* 3 */
PORT_DECL(Sputnik, EndOfSelection) /* 0 */
PORT_DECL(Sputnik, BackControl) /* 3 */
PORT_DECL(Sputnik, SetSptAt) /* 3 */
PORT_DECL(Sputnik, InitSuck) /* 3 */
PORT_DECL(Sputnik, InitMake) /* 3 */
PORT_DECL(Sputnik, InitHello) /* 3 */
PORT_DECL(Sputnik, InitBye) /* 3 */
PORT_DECL(Sputnik, InitInterface) /* 3 */
#define PORTS_Sputnik \
	PORT_DEFNK(Sputnik, Assign, Sputnik_Assign) \
	PORT_DEFN(Sputnik, ChangeRelax, Sputnik_ChangeRelax) \
	PORT_DEFN(Sputnik, ByeBye, Sputnik_ByeBye) \
	PORT_DEFN(Sputnik, BackCamera, Sputnik_BackCamera) \
	PORT_DEFNK(Sputnik, UseShip, Sputnik_UseShip) \
	PORT_DEFN(Sputnik, ChangeShip, Sputnik_ChangeShip) \
	PORT_DEFN(Sputnik, CreateShip, Sputnik_CreateShip) \
	PORT_DEFN(Sputnik, ShowInterface, Sputnik_ShowInterface) \
	PORT_DEFN(Sputnik, EnterInterface, Sputnik_EnterInterface) \
	PORT_DEFN(Sputnik, EndOfSelection, Sputnik_EndOfSelection) \
	PORT_DEFN(Sputnik, BackControl, Sputnik_BackControl) \
	PORT_DEFN(Sputnik, SetSptAt, Sputnik_SetSptAt) \
	PORT_DEFN(Sputnik, InitSuck, Sputnik_InitSuck) \
	PORT_DEFN(Sputnik, InitMake, Sputnik_InitMake) \
	PORT_DEFN(Sputnik, InitHello, Sputnik_InitHello) \
	PORT_DEFN(Sputnik, InitBye, Sputnik_InitBye) \
	PORT_DEFN(Sputnik, InitInterface, Sputnik_InitInterface)

/* SputnikInterface.c */
PORT_DECL(SputnikInterface, DrawBackground) /* ? */
PORT_DECL(SputnikInterface, AddDefaultPanelContent) /* 1 */
PORT_DECL(SputnikInterface, EnterSputnikInterface) /* 1 */
PORT_DECL(SputnikInterface, ExitSputnikInterface) /* 2 */
PORT_DECL(SputnikInterface, SelectShip) /* 2 */
#define PORTS_SputnikInterface \
	PORT_DEFNK(SputnikInterface, DrawBackground, ShipEdit_DrawBackground) \
	PORT_DEFN(SputnikInterface, AddDefaultPanelContent, SI_AddDefaultPanelContent) \
	PORT_DEFN(SputnikInterface, EnterSputnikInterface, SI_EnterSputnikInterface) \
	PORT_DEFN(SputnikInterface, ExitSputnikInterface, SI_ExitSputnikInterface) \
	PORT_DEFN(SputnikInterface, SelectShip, SI_SelectShip)

/* String.c */
PORT_DECL(string, index) /* ? */
PORT_DECL(string, rindex) /* ? */
PORT_DECL(string, ljust) /* 2 */
PORT_DECL(string, rjust) /* 2 */
PORT_DECL(string, center) /* 2 */
PORT_DECL(string, zfill) /* 2 */
PORT_DECL(string, capwords) /* ? */
#define PORTS_String \
	PORT_DEFNK(string, index, String_index) \
	PORT_DEFNK(string, rindex, String_rindex) \
	PORT_DEFN(string, ljust, String_ljust) \
	PORT_DEFN(string, rjust, String_rjust) \
	PORT_DEFN(string, center, String_center) \
	PORT_DEFN(string, zfill, String_zfill) \
	PORT_DEFNK(string, capwords, String_capwords)

/* SuperDeals.c */
PORT_DECL(DeathMatch, GetMenuDescription) /* 0 */
PORT_DECL(DeathMatch, InitStatus) /* 1 */
PORT_DECL(DeathMatch, Init) /* 1 */
PORT_DECL(DeathMatch, SetInvulnerableStatus) /* 1 */
PORT_DECL(DeathMatch, SetProfile) /* 1 */
PORT_DECL(DeathMatch, CreateEnemies) /* 0 */
PORT_DECL(DeathMatch, AbortRemoteMessages) /* 0 */
PORT_DECL(DeathMatch, CrazyGamblerStartCombat) /* 0 */
PORT_DECL(DeathMatch, SetVulnerable) /* 0 */
PORT_DECL(DeathMatch, StartCombat) /* 0 */
PORT_DECL(DeathMatch, GetMaxFragsEnemies) /* 0 */
PORT_DECL(DeathMatch, OnEnemyDeath) /* 2 */
PORT_DECL(DeathMatch, RespawnEnemy) /* 1 */
PORT_DECL(DeathMatch, RespawnPlayer) /* 1 */
PORT_DECL(DeathMatch, OnPlayerDeath) /* 2 */
PORT_DECL(DeathMatch, RestartCombat) /* 2 */
PORT_DECL(TeamDeathMatch, GetMenuDescription) /* 0 */
PORT_DECL(TeamDeathMatch, InitStatus) /* 1 */
PORT_DECL(TeamDeathMatch, Init) /* 1 */
PORT_DECL(TeamDeathMatch, SetInvulnerableStatus) /* 1 */
PORT_DECL(TeamDeathMatch, SetEnemyProfile) /* 1 */
PORT_DECL(TeamDeathMatch, SetFriendProfile) /* 1 */
PORT_DECL(TeamDeathMatch, CreateFriends) /* 0 */
PORT_DECL(TeamDeathMatch, CreateEnemies) /* 0 */
PORT_DECL(TeamDeathMatch, AbortRemoteMessages) /* 0 */
PORT_DECL(TeamDeathMatch, CrazyGamblerStartCombat) /* 0 */
PORT_DECL(TeamDeathMatch, OnVehicle) /* 0 */
PORT_DECL(TeamDeathMatch, StartCombat) /* 0 */
PORT_DECL(TeamDeathMatch, OnEnemyDeath) /* 2 */
PORT_DECL(TeamDeathMatch, OnEnemyDeath2) /* 2 */
PORT_DECL(TeamDeathMatch, RespawnEnemy) /* 1 */
PORT_DECL(TeamDeathMatch, OnFriendDeath) /* 2 */
PORT_DECL(TeamDeathMatch, OnFriendDeath2) /* 2 */
PORT_DECL(TeamDeathMatch, RespawnFriend) /* 1 */
PORT_DECL(TeamDeathMatch, RespawnPlayer) /* 1 */
PORT_DECL(TeamDeathMatch, OnPlayerDeath) /* 2 */
PORT_DECL(TeamDeathMatch, OnPlayerDeath2) /* 2 */
PORT_DECL(TeamDeathMatch, RestartCombat) /* 2 */
PORT_DECL(Versus, GetMenuDescription) /* 0 */
PORT_DECL(Versus, InitStatus) /* 1 */
PORT_DECL(Versus, Init) /* 1 */
PORT_DECL(Versus, CreateEnemy) /* ? */
PORT_DECL(Versus, AbortRemoteMessages) /* 0 */
PORT_DECL(Versus, CrazyGamblerStartCombat) /* 0 */
PORT_DECL(Versus, StartCombat) /* 0 */
PORT_DECL(Versus, OnEnemyDeath) /* 2 */
PORT_DECL(Versus, RespawnEnemy) /* 0 */
PORT_DECL(Versus, OnPlayerDeath) /* 2 */
PORT_DECL(Versus, EnemyWins) /* 1 */
PORT_DECL(Versus, RespawnPlayer) /* 3 */
PORT_DECL(Versus, RestartCombat) /* 2 */
PORT_DECL(CTF, GetMenuDescription) /* 0 */
PORT_DECL(CTF, InitStatus) /* 1 */
PORT_DECL(CTF, Init) /* 1 */
PORT_DECL(CTF, SetInvulnerableStatus) /* 1 */
PORT_DECL(CTF, AbortRemoteMessages) /* 0 */
PORT_DECL(CTF, SetProfile) /* 2 */
PORT_DECL(CTF, CreateEnemies) /* 0 */
PORT_DECL(CTF, CreateFriends) /* 0 */
PORT_DECL(CTF, CrazyGamblerStartCTF) /* 0 */
PORT_DECL(CTF, SetVulnerable) /* 0 */
PORT_DECL(CTF, StartCTF) /* 0 */
PORT_DECL(CTF, OnVehicleDeathCallback) /* 2 */
PORT_DECL(CTF, OnPlayerDeath) /* 2 */
PORT_DECL(CTF, RemoveFlag) /* 1 */
PORT_DECL(CTF, EndOfCTF) /* 0 */
PORT_DECL(CTF, RespawnEnemy) /* ? */
PORT_DECL(CTF, RespawnFriend) /* 0 */
PORT_DECL(CTF, CreateFlag) /* ? */
PORT_DECL(CTF, ResetFlag) /* 1 */
PORT_DECL(CTF, TakeFlag) /* 2 */
PORT_DECL(CTF, SpawnFlags) /* 2 */
PORT_DECL(CTF, RespawnPlayer) /* 1 */
PORT_DECL(CTF, PreRestartMenu) /* 0 */
PORT_DECL(CTF, RestartCTF) /* 2 */
PORT_DECL(CTF, PreOnVictory) /* 0 */
PORT_DECL(Domination, GetMenuDescription) /* 0 */
PORT_DECL(Domination, InitStatus) /* 1 */
PORT_DECL(Domination, Init) /* 1 */
PORT_DECL(Domination, ReleaseBaton) /* 0 */
PORT_DECL(Domination, LostBatonMessage) /* 0 */
PORT_DECL(Domination, SetInvulnerableStatus) /* 1 */
PORT_DECL(Domination, AbortRemoteMessages) /* 0 */
PORT_DECL(Domination, SetProfile) /* 2 */
PORT_DECL(Domination, CreateEnemies) /* 0 */
PORT_DECL(Domination, CreateFriends) /* 0 */
PORT_DECL(Domination, CreateBaton) /* ? */
PORT_DECL(Domination, CrazyGamblerStartDomination) /* 0 */
PORT_DECL(Domination, OnVehicle) /* 0 */
PORT_DECL(Domination, StartDomination) /* 0 */
PORT_DECL(Domination, TakeBaton) /* 2 */
PORT_DECL(Domination, CreateZone) /* 2 */
PORT_DECL(Domination, TakeZone) /* 2 */
PORT_DECL(Domination, ActivateZone) /* 1 */
PORT_DECL(Domination, RestartDomination) /* 0 */
PORT_DECL(Domination, OnPlayerDeath) /* 2 */
PORT_DECL(Domination, OnEnemyDeath) /* 2 */
PORT_DECL(Domination, OnFriendDeath) /* 2 */
PORT_DECL(Domination, RespawnPlayer) /* 1 */
PORT_DECL(Domination, RespawnEnemy) /* 1 */
PORT_DECL(Domination, RespawnFriend) /* 0 */
PORT_DECL(Domination, EndOfDomination) /* 1 */
PORT_DECL(Domination, PreRestartMenu) /* 0 */
PORT_DECL(Domination, RestartDomin) /* 2 */
PORT_DECL(Domination, PreOnVictory) /* 0 */
PORT_DECL(Domination, MissionScorer) /* 1 */
PORT_DECL(MortalRace, GetMenuDescription) /* 0 */
PORT_DECL(MortalRace, InitStatus) /* 1 */
PORT_DECL(MortalRace, Init) /* 1 */
PORT_DECL(MortalRace, ReleaseBaton) /* 0 */
PORT_DECL(MortalRace, LostBatonMessage) /* 0 */
PORT_DECL(MortalRace, SetInvulnerableStatus) /* 1 */
PORT_DECL(MortalRace, AbortRemoteMessages) /* 0 */
PORT_DECL(MortalRace, CrazyGamblerStartMortalRace) /* 0 */
PORT_DECL(MortalRace, SetVulnerable) /* 0 */
PORT_DECL(MortalRace, CreateBaton) /* 1 */
PORT_DECL(MortalRace, TakeBaton) /* 2 */
PORT_DECL(MortalRace, InitRace) /* ? */
PORT_DECL(MortalRace, CreateFirstWaypoint) /* 0 */
PORT_DECL(MortalRace, OnRaceStart) /* 2 */
PORT_DECL(MortalRace, RaceGo) /* 2 */
PORT_DECL(MortalRace, CheckShipsState) /* 0 */
PORT_DECL(MortalRace, OnPositionReached) /* 1 */
PORT_DECL(MortalRace, OnCheckPoint) /* 2 */
PORT_DECL(MortalRace, Restart) /* 0 */
PORT_DECL(MortalRace, RestartRace) /* 2 */
PORT_DECL(MortalRace, MoveWaypoint) /* 0 */
PORT_DECL(MortalRace, EndOfMortalRace) /* 0 */
PORT_DECL(MortalRace, MoveRacersToTraffic) /* 0 */
PORT_DECL(MortalRace, MoveRacersToPos) /* 2 */
PORT_DECL(MortalRace, RacersAttackShip) /* 1 */
PORT_DECL(MortalRace, OnPlayerDeath) /* 2 */
PORT_DECL(MortalRace, RacerKilled) /* 2 */
PORT_DECL(MortalRace, RacerDamaged) /* 3 */
PORT_DECL(MortalRace, RespawnRacer) /* 2 */
PORT_DECL(MortalRace, RespawnPlayer) /* 3 */
PORT_DECL(MortalRace, PreOnVictory) /* 0 */
#define PORTS_SuperDeals \
	PORT_DEFN(DeathMatch, GetMenuDescription, Dm_GetMenuDescription) \
	PORT_DEFN(DeathMatch, InitStatus, Dm_InitStatus) \
	PORT_DEFN(DeathMatch, Init, Dm_Init) \
	PORT_DEFN(DeathMatch, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEFN(DeathMatch, SetProfile, Dm_SetProfile) \
	PORT_DEFN(DeathMatch, CreateEnemies, Dm_CreateEnemies) \
	PORT_DEFN(DeathMatch, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEFN(DeathMatch, CrazyGamblerStartCombat, Dm_CrazyGamblerStartCombat) \
	PORT_DEFN(DeathMatch, SetVulnerable, Sd_SetVulnerable) \
	PORT_DEFN(DeathMatch, StartCombat, Dm_StartCombat) \
	PORT_DEFN(DeathMatch, GetMaxFragsEnemies, Dm_GetMaxFragsEnemies) \
	PORT_DEFN(DeathMatch, OnEnemyDeath, Dm_OnEnemyDeath) \
	PORT_DEFN(DeathMatch, RespawnEnemy, Dm_RespawnEnemy) \
	PORT_DEFN(DeathMatch, RespawnPlayer, Sd_RespawnPlayer) \
	PORT_DEFN(DeathMatch, OnPlayerDeath, Dm_OnPlayerDeath) \
	PORT_DEFN(DeathMatch, RestartCombat, Dm_RestartCombat) \
	PORT_DEFN(TeamDeathMatch, GetMenuDescription, Tdm_GetMenuDescription) \
	PORT_DEFN(TeamDeathMatch, InitStatus, Tdm_InitStatus) \
	PORT_DEFN(TeamDeathMatch, Init, Tdm_Init) \
	PORT_DEFN(TeamDeathMatch, SetInvulnerableStatus, Tdm_SetInvulnerableStatus) \
	PORT_DEFN(TeamDeathMatch, SetEnemyProfile, Dm_SetProfile) \
	PORT_DEFN(TeamDeathMatch, SetFriendProfile, Tdm_SetFriendProfile) \
	PORT_DEFN(TeamDeathMatch, CreateFriends, Tdm_CreateFriends) \
	PORT_DEFN(TeamDeathMatch, CreateEnemies, Tdm_CreateEnemies) \
	PORT_DEFN(TeamDeathMatch, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEFN(TeamDeathMatch, CrazyGamblerStartCombat, Tdm_CrazyGamblerStartCombat) \
	PORT_DEFN(TeamDeathMatch, OnVehicle, Sd_SetVulnerable) \
	PORT_DEFN(TeamDeathMatch, StartCombat, Tdm_StartCombat) \
	PORT_DEFN(TeamDeathMatch, OnEnemyDeath, Tdm_OnEnemyDeath) \
	PORT_DEFN(TeamDeathMatch, OnEnemyDeath2, Tdm_OnEnemyDeath2) \
	PORT_DEFN(TeamDeathMatch, RespawnEnemy, Tdm_RespawnEnemy) \
	PORT_DEFN(TeamDeathMatch, OnFriendDeath, Tdm_OnFriendDeath) \
	PORT_DEFN(TeamDeathMatch, OnFriendDeath2, Tdm_OnFriendDeath2) \
	PORT_DEFN(TeamDeathMatch, RespawnFriend, Tdm_RespawnFriend) \
	PORT_DEFN(TeamDeathMatch, RespawnPlayer, Sd_RespawnPlayer) \
	PORT_DEFN(TeamDeathMatch, OnPlayerDeath, Tdm_OnPlayerDeath) \
	PORT_DEFN(TeamDeathMatch, OnPlayerDeath2, Tdm_OnPlayerDeath2) \
	PORT_DEFN(TeamDeathMatch, RestartCombat, Tdm_RestartCombat) \
	PORT_DEFN(Versus, GetMenuDescription, Vs_GetMenuDescription) \
	PORT_DEFN(Versus, InitStatus, Vs_InitStatus) \
	PORT_DEFN(Versus, Init, Vs_Init) \
	PORT_DEFNK(Versus, CreateEnemy, Vs_CreateEnemy) \
	PORT_DEFN(Versus, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEFN(Versus, CrazyGamblerStartCombat, Vs_CrazyGamblerStartCombat) \
	PORT_DEFN(Versus, StartCombat, Vs_StartCombat) \
	PORT_DEFN(Versus, OnEnemyDeath, Vs_OnEnemyDeath) \
	PORT_DEFN(Versus, RespawnEnemy, Vs_RespawnEnemy) \
	PORT_DEFN(Versus, OnPlayerDeath, Vs_OnPlayerDeath) \
	PORT_DEFN(Versus, EnemyWins, Vs_EnemyWins) \
	PORT_DEFN(Versus, RespawnPlayer, Vs_RespawnPlayer) \
	PORT_DEFN(Versus, RestartCombat, Vs_RestartCombat) \
	PORT_DEFN(CTF, GetMenuDescription, Ctf_GetMenuDescription) \
	PORT_DEFN(CTF, InitStatus, Ctf_InitStatus) \
	PORT_DEFN(CTF, Init, Ctf_Init) \
	PORT_DEFN(CTF, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEFN(CTF, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEFN(CTF, SetProfile, Ctf_SetProfile) \
	PORT_DEFN(CTF, CreateEnemies, Ctf_CreateEnemies) \
	PORT_DEFN(CTF, CreateFriends, Ctf_CreateFriends) \
	PORT_DEFN(CTF, CrazyGamblerStartCTF, Ctf_CrazyGamblerStartCTF) \
	PORT_DEFN(CTF, SetVulnerable, Sd_SetVulnerable) \
	PORT_DEFN(CTF, StartCTF, Ctf_StartCTF) \
	PORT_DEFN(CTF, OnVehicleDeathCallback, Ctf_OnVehicleDeathCallback) \
	PORT_DEFN(CTF, OnPlayerDeath, Ctf_OnPlayerDeath) \
	PORT_DEFN(CTF, RemoveFlag, Ctf_RemoveFlag) \
	PORT_DEFN(CTF, EndOfCTF, Ctf_EndOfCTF) \
	PORT_DEFNK(CTF, RespawnEnemy, Ctf_RespawnEnemy) \
	PORT_DEFN(CTF, RespawnFriend, Ctf_RespawnFriend) \
	PORT_DEFNK(CTF, CreateFlag, Ctf_CreateFlag) \
	PORT_DEFN(CTF, ResetFlag, Ctf_ResetFlag) \
	PORT_DEFN(CTF, TakeFlag, Ctf_TakeFlag) \
	PORT_DEFN(CTF, SpawnFlags, Ctf_SpawnFlags) \
	PORT_DEFN(CTF, RespawnPlayer, Ctf_RespawnPlayer) \
	PORT_DEFN(CTF, PreRestartMenu, Ctf_PreRestartMenu) \
	PORT_DEFN(CTF, RestartCTF, Sd_Restart) \
	PORT_DEFN(CTF, PreOnVictory, Ctf_PreOnVictory) \
	PORT_DEFN(Domination, GetMenuDescription, Dom_GetMenuDescription) \
	PORT_DEFN(Domination, InitStatus, Dom_InitStatus) \
	PORT_DEFN(Domination, Init, Dom_Init) \
	PORT_DEFN(Domination, ReleaseBaton, Dom_ReleaseBaton) \
	PORT_DEFN(Domination, LostBatonMessage, Dom_LostBatonMessage) \
	PORT_DEFN(Domination, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEFN(Domination, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEFN(Domination, SetProfile, Dom_SetProfile) \
	PORT_DEFN(Domination, CreateEnemies, Dom_CreateEnemies) \
	PORT_DEFN(Domination, CreateFriends, Dom_CreateFriends) \
	PORT_DEFNK(Domination, CreateBaton, Dom_CreateBaton) \
	PORT_DEFN(Domination, CrazyGamblerStartDomination, Dom_CrazyGamblerStartDomination) \
	PORT_DEFN(Domination, OnVehicle, Dom_OnVehicle) \
	PORT_DEFN(Domination, StartDomination, Dom_StartDomination) \
	PORT_DEFN(Domination, TakeBaton, Dom_TakeBaton) \
	PORT_DEFN(Domination, CreateZone, Dom_CreateZone) \
	PORT_DEFN(Domination, TakeZone, Dom_TakeZone) \
	PORT_DEFN(Domination, ActivateZone, Dom_ActivateZone) \
	PORT_DEFN(Domination, RestartDomination, Dom_RestartDomination) \
	PORT_DEFN(Domination, OnPlayerDeath, Dom_OnPlayerDeath) \
	PORT_DEFN(Domination, OnEnemyDeath, Dom_OnEnemyDeath) \
	PORT_DEFN(Domination, OnFriendDeath, Dom_OnFriendDeath) \
	PORT_DEFN(Domination, RespawnPlayer, Dom_RespawnPlayer) \
	PORT_DEFN(Domination, RespawnEnemy, Dom_RespawnEnemy) \
	PORT_DEFN(Domination, RespawnFriend, Dom_RespawnFriend) \
	PORT_DEFN(Domination, EndOfDomination, Dom_EndOfDomination) \
	PORT_DEFN(Domination, PreRestartMenu, Dom_PreRestartMenu) \
	PORT_DEFN(Domination, RestartDomin, Dom_RestartDomin) \
	PORT_DEFN(Domination, PreOnVictory, Dom_PreOnVictory) \
	PORT_DEFN(Domination, MissionScorer, Dom_MissionScorer) \
	PORT_DEFN(MortalRace, GetMenuDescription, Mr_GetMenuDescription) \
	PORT_DEFN(MortalRace, InitStatus, Mr_InitStatus) \
	PORT_DEFN(MortalRace, Init, Mr_Init) \
	PORT_DEFN(MortalRace, ReleaseBaton, Mr_ReleaseBaton) \
	PORT_DEFN(MortalRace, LostBatonMessage, Mr_LostBatonMessage) \
	PORT_DEFN(MortalRace, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEFN(MortalRace, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEFN(MortalRace, CrazyGamblerStartMortalRace, Mr_CrazyGamblerStartMortalRace) \
	PORT_DEFN(MortalRace, SetVulnerable, Sd_SetVulnerable) \
	PORT_DEFN(MortalRace, CreateBaton, Mr_CreateBaton) \
	PORT_DEFN(MortalRace, TakeBaton, Mr_TakeBaton) \
	PORT_DEFNK(MortalRace, InitRace, Mr_InitRace) \
	PORT_DEFN(MortalRace, CreateFirstWaypoint, Mr_CreateFirstWaypoint) \
	PORT_DEFN(MortalRace, OnRaceStart, Mr_OnRaceStart) \
	PORT_DEFN(MortalRace, RaceGo, Mr_RaceGo) \
	PORT_DEFN(MortalRace, CheckShipsState, Mr_CheckShipsState) \
	PORT_DEFN(MortalRace, OnPositionReached, Mr_OnPositionReached) \
	PORT_DEFN(MortalRace, OnCheckPoint, Mr_OnCheckPoint) \
	PORT_DEFN(MortalRace, Restart, Mr_Restart) \
	PORT_DEFN(MortalRace, RestartRace, Sd_Restart) \
	PORT_DEFN(MortalRace, MoveWaypoint, Mr_MoveWaypoint) \
	PORT_DEFN(MortalRace, EndOfMortalRace, Mr_EndOfMortalRace) \
	PORT_DEFN(MortalRace, MoveRacersToTraffic, Mr_MoveRacersToTraffic) \
	PORT_DEFN(MortalRace, MoveRacersToPos, Mr_MoveRacersToPos) \
	PORT_DEFN(MortalRace, RacersAttackShip, Mr_RacersAttackShip) \
	PORT_DEFN(MortalRace, OnPlayerDeath, Mr_OnPlayerDeath) \
	PORT_DEFN(MortalRace, RacerKilled, Mr_RacerKilled) \
	PORT_DEFN(MortalRace, RacerDamaged, Mr_RacerDamaged) \
	PORT_DEFN(MortalRace, RespawnRacer, Mr_RespawnRacer) \
	PORT_DEFN(MortalRace, RespawnPlayer, Mr_RespawnPlayer) \
	PORT_DEFN(MortalRace, PreOnVictory, Mr_PreOnVictory)

/* Traffic.c */
PORT_DECL(Traffic, SetPath) /* ? */
PORT_DECL(Traffic, ResetPath) /* ? */
PORT_DECL(Traffic, InitAIProfiles) /* ? */
PORT_DECL(Traffic, EmptyFunc) /* 1 */
PORT_DECL(Traffic, MakeTrafic) /* ? */
PORT_DECL(Traffic, InitTraffic) /* ? */
PORT_DECL(Traffic, OnNeedMoreTraffic) /* 1 */
PORT_DECL(Traffic, OnNeedMoreChasingTraffic) /* 1 */
PORT_DECL(Traffic, OnPlayerHelpedPolice) /* ? */
PORT_DECL(Traffic, RailTrafficDeath) /* 2 */
PORT_DECL(Traffic, HeavyTrafficDeath) /* 2 */
PORT_DECL(Traffic, NormalTrafficDeath) /* 2 */
PORT_DECL(Traffic, MediumTrafficDeath) /* 2 */
PORT_DECL(Traffic, InitCivilTraffic) /* ? */
PORT_DECL(Traffic, Init) /* ? */
PORT_DECL(OutPolice, MakePoli) /* 1 */
PORT_DECL(OutPolice, Init) /* ? */
PORT_DECL(OutPolice, NextWave) /* ? */
PORT_DECL(OutPolice, InitShips) /* ? */
PORT_DECL(OutPolice, InitWaves) /* ? */
PORT_DECL(OutPolice, InitAIProfiles) /* ? */
PORT_DECL(InPolice, Include) /* 1 */
PORT_DECL(InPolice, SetGVars) /* ? */
PORT_DECL(InPolice, Init) /* ? */
#define PORTS_Traffic \
	PORT_DEFN(Traffic, SetPath, Traffic_SetPath) \
	PORT_DEFN(Traffic, ResetPath, Traffic_ResetPath) \
	PORT_DEFN(Traffic, InitAIProfiles, Traffic_InitAIProfiles) \
	PORT_DEFN(Traffic, EmptyFunc, Traffic_EmptyFunc) \
	PORT_DEFN(Traffic, MakeTrafic, Traffic_MakeTrafic) \
	PORT_DEFN(Traffic, InitTraffic, Traffic_InitTraffic) \
	PORT_DEFN(Traffic, OnNeedMoreTraffic, Traffic_OnNeedMoreTraffic) \
	PORT_DEFN(Traffic, OnNeedMoreChasingTraffic, Traffic_OnNeedMoreChasingTraffic) \
	PORT_DEFN(Traffic, OnPlayerHelpedPolice, Traffic_OnPlayerHelpedPolice) \
	PORT_DEFN(Traffic, RailTrafficDeath, Traffic_RailTrafficDeath) \
	PORT_DEFN(Traffic, HeavyTrafficDeath, Traffic_HeavyTrafficDeath) \
	PORT_DEFN(Traffic, NormalTrafficDeath, Traffic_NormalTrafficDeath) \
	PORT_DEFN(Traffic, MediumTrafficDeath, Traffic_MediumTrafficDeath) \
	PORT_DEFN(Traffic, InitCivilTraffic, Traffic_InitCivilTraffic) \
	PORT_DEFN(Traffic, Init, Traffic_Init) \
	PORT_DEFN(OutPolice, MakePoli, OutPolice_MakePoli) \
	PORT_DEFN(OutPolice, Init, OutPolice_Init) \
	PORT_DEFN(OutPolice, NextWave, OutPolice_NextWave) \
	PORT_DEFN(OutPolice, InitShips, OutPolice_InitShips) \
	PORT_DEFN(OutPolice, InitWaves, OutPolice_InitWaves) \
	PORT_DEFN(OutPolice, InitAIProfiles, OutPolice_InitAIProfiles) \
	PORT_DEFN(InPolice, Include, InPolice_Include) \
	PORT_DEFN(InPolice, SetGVars, InPolice_SetGVars) \
	PORT_DEFN(InPolice, Init, InPolice_Init)

/* Vehicles.c */
PORT_DECL(Vehicles, ClientCreate) /* 2 */
PORT_DECL(Vehicles, ClientAfterCreate) /* 2 */
PORT_DECL(Vehicles, ServerMotorHit) /* 3 */
PORT_DECL(Vehicles, MotorHit) /* 3 */
PORT_DECL(Vehicles, CreateDumbMotor) /* 4 */
PORT_DECL(Vehicles, SetUpDumbMotor) /* 2 */
PORT_DECL(Vehicles, DumbShipHit) /* 3 */
PORT_DECL(Vehicles, OnClientDeath) /* 2 */
PORT_DECL(Vehicles, ActivateHitType) /* 2 */
PORT_DECL(Vehicles, GetCoin) /* 2 */
PORT_DECL(Vehicles, GetRandomMercenaryHead) /* ? */
PORT_DECL(Vehicles, OnDeath) /* 2 */
PORT_DECL(Vehicles, SwitchView) /* 2 */
PORT_DECL(Vehicles, SetActiveCam) /* 3 */
PORT_DECL(Vehicles, ReSpawnShip) /* 1 */
PORT_DECL(Vehicles, SetProperties) /* ? */
PORT_DECL(Vehicles, SetStdProperties) /* 3 */
PORT_DECL(Vehicles, Init) /* 0 */
#define PORTS_Vehicles \
	PORT_DEFN(Vehicles, ClientCreate, Vehicles_ClientCreate) \
	PORT_DEFN(Vehicles, ClientAfterCreate, Vehicles_ClientAfterCreate) \
	PORT_DEFN(Vehicles, ServerMotorHit, Vehicles_ServerMotorHit) \
	PORT_DEFN(Vehicles, MotorHit, Vehicles_MotorHit) \
	PORT_DEFN(Vehicles, CreateDumbMotor, Vehicles_CreateDumbMotor) \
	PORT_DEFN(Vehicles, SetUpDumbMotor, Vehicles_SetUpDumbMotor) \
	PORT_DEFN(Vehicles, DumbShipHit, Vehicles_DumbShipHit) \
	PORT_DEFN(Vehicles, OnClientDeath, Vehicles_OnClientDeath) \
	PORT_DEFN(Vehicles, ActivateHitType, Vehicles_ActivateHitType) \
	PORT_DEFN(Vehicles, GetCoin, Vehicles_GetCoin) \
	PORT_DEFN(Vehicles, GetRandomMercenaryHead, Vehicles_GetRandomMercenaryHead) \
	PORT_DEFN(Vehicles, OnDeath, Vehicles_OnDeath) \
	PORT_DEFN(Vehicles, SwitchView, Vehicles_SwitchView) \
	PORT_DEFN(Vehicles, SetActiveCam, Vehicles_SetActiveCam) \
	PORT_DEFN(Vehicles, ReSpawnShip, Vehicles_ReSpawnShip) \
	PORT_DEFNK(Vehicles, SetProperties, Vehicles_SetProperties) \
	PORT_DEFN(Vehicles, SetStdProperties, Vehicles_SetStdProperties) \
	PORT_DEFN(Vehicles, Init, Vehicles_Init)

/* VehiclesNPC.c */
PORT_DECL(VehiclesNPC, InitStdShips) /* 0 */
PORT_DECL(VehiclesNPC, MakeAI) /* ? */
PORT_DECL(VehiclesNPC, GetIAProf) /* 1 */
PORT_DECL(VehiclesNPC, ModAIProff) /* ? */
PORT_DECL(VehiclesNPC, SetIAProf) /* ? */
PORT_DECL(VehiclesNPC, SetAIProfile) /* 2 */
PORT_DECL(VehiclesNPC, SetStdProperties) /* 2 */
PORT_DECL(VehiclesNPC, MercOnDeath) /* 2 */
PORT_DECL(VehiclesNPC, InitMercs) /* 0 */
PORT_DECL(VehiclesNPC, Init) /* 0 */
#define PORTS_VehiclesNPC \
	PORT_DEFN(VehiclesNPC, InitStdShips, VehiclesNPC_InitStdShips) \
	PORT_DEFNK(VehiclesNPC, MakeAI, VehiclesNPC_MakeAI) \
	PORT_DEFN(VehiclesNPC, GetIAProf, VehiclesNPC_GetIAProf) \
	PORT_DEFNK(VehiclesNPC, ModAIProff, VehiclesNPC_ModAIProff) \
	PORT_DEFNK(VehiclesNPC, SetIAProf, VehiclesNPC_SetIAProf) \
	PORT_DEFN(VehiclesNPC, SetAIProfile, VehiclesNPC_SetAIProfile) \
	PORT_DEFN(VehiclesNPC, SetStdProperties, VehiclesNPC_SetStdProperties) \
	PORT_DEFN(VehiclesNPC, MercOnDeath, VehiclesNPC_MercOnDeath) \
	PORT_DEFN(VehiclesNPC, InitMercs, VehiclesNPC_InitMercs) \
	PORT_DEFN(VehiclesNPC, Init, VehiclesNPC_Init)

/* Weapons.c */
PORT_DECL(Weapons, InitAmmo) /* ? */
PORT_DECL(Weapons, InitWeapons) /* ? */
PORT_DECL(Weapons, Init) /* ? */
PORT_DECL(Vulcan, LoadResources) /* ? */
PORT_DECL(Vulcan, Init) /* 2 */
PORT_DECL(Devastator, LoadResources) /* ? */
PORT_DECL(Devastator, Init) /* 2 */
PORT_DECL(Tesla, LoadResources) /* ? */
PORT_DECL(Tesla, Init) /* 2 */
PORT_DECL(ATPC, LoadResources) /* ? */
PORT_DECL(ATPC, Init) /* 2 */
PORT_DECL(Laser, LoadResources) /* ? */
PORT_DECL(Laser, Init) /* 2 */
PORT_DECL(Swarm, ClientCreate) /* 2 */
PORT_DECL(Swarm, ClientAfterCreate) /* 2 */
PORT_DECL(Swarm, LoadResources) /* ? */
PORT_DECL(Swarm, Init) /* 2 */
PORT_DECL(Inferno, ClientCreate) /* 2 */
PORT_DECL(Inferno, ClientAfterCreate) /* 2 */
PORT_DECL(Inferno, LoadResources) /* ? */
PORT_DECL(Inferno, Init) /* 2 */
PORT_DECL(Cloud, ClientCreate) /* 2 */
PORT_DECL(Cloud, ClientAfterCreate) /* 2 */
PORT_DECL(Cloud, ActivateHitType) /* 2 */
PORT_DECL(Cloud, CreateCloud) /* 7 */
PORT_DECL(Cloud, SetUpCloud) /* 1 */
PORT_DECL(Cloud, LoadResources) /* ? */
PORT_DECL(Cloud, Init) /* 1 */
PORT_DECL(EMI, ClientCreate) /* 2 */
PORT_DECL(EMI, ClientAfterCreate) /* 2 */
PORT_DECL(EMI, ActivateHitType) /* 2 */
PORT_DECL(EMI, CreateEMI) /* 7 */
PORT_DECL(EMI, SetUpEMI) /* 1 */
PORT_DECL(EMI, LoadResources) /* ? */
PORT_DECL(EMI, Init) /* 1 */
PORT_DECL(Sonic, ClientCreate) /* 2 */
PORT_DECL(Sonic, ClientAfterCreate) /* 2 */
PORT_DECL(Sonic, SonicHit) /* 1 */
PORT_DECL(Sonic, OnHit) /* 3 */
PORT_DECL(Sonic, OnServerHit) /* 3 */
PORT_DECL(Sonic, OnDeath) /* 2 */
PORT_DECL(Sonic, SetUpToDetonate) /* 1 */
PORT_DECL(Sonic, CreateSonic) /* 7 */
PORT_DECL(Sonic, SetUpSonic) /* 1 */
PORT_DECL(Sonic, LoadResources) /* ? */
PORT_DECL(Sonic, Init) /* 1 */
#define PORTS_Weapons \
	PORT_DEFN(Weapons, InitAmmo, Weapons_InitAmmo) \
	PORT_DEFN(Weapons, InitWeapons, Weapons_InitWeapons) \
	PORT_DEFN(Weapons, Init, Weapons_Init) \
	PORT_DEFN(Vulcan, LoadResources, Vulcan_LoadResources) \
	PORT_DEFN(Vulcan, Init, Vulcan_Init) \
	PORT_DEFN(Devastator, LoadResources, Devastator_LoadResources) \
	PORT_DEFN(Devastator, Init, Devastator_Init) \
	PORT_DEFN(Tesla, LoadResources, Tesla_LoadResources) \
	PORT_DEFN(Tesla, Init, Tesla_Init) \
	PORT_DEFN(ATPC, LoadResources, ATPC_LoadResources) \
	PORT_DEFN(ATPC, Init, ATPC_Init) \
	PORT_DEFN(Laser, LoadResources, Laser_LoadResources) \
	PORT_DEFN(Laser, Init, Laser_Init) \
	PORT_DEFN(Swarm, ClientCreate, Swarm_ClientCreate) \
	PORT_DEFN(Swarm, ClientAfterCreate, Swarm_ClientAfterCreate) \
	PORT_DEFN(Swarm, LoadResources, Swarm_LoadResources) \
	PORT_DEFN(Swarm, Init, Swarm_Init) \
	PORT_DEFN(Inferno, ClientCreate, Inferno_ClientCreate) \
	PORT_DEFN(Inferno, ClientAfterCreate, Inferno_ClientAfterCreate) \
	PORT_DEFN(Inferno, LoadResources, Inferno_LoadResources) \
	PORT_DEFN(Inferno, Init, Inferno_Init) \
	PORT_DEFN(Cloud, ClientCreate, Cloud_ClientCreate) \
	PORT_DEFN(Cloud, ClientAfterCreate, Cloud_ClientAfterCreate) \
	PORT_DEFN(Cloud, ActivateHitType, Weap_ActivateHitType) \
	PORT_DEFN(Cloud, CreateCloud, Cloud_CreateCloud) \
	PORT_DEFN(Cloud, SetUpCloud, Cloud_SetUpCloud) \
	PORT_DEFN(Cloud, LoadResources, Cloud_LoadResources) \
	PORT_DEFN(Cloud, Init, Cloud_Init) \
	PORT_DEFN(EMI, ClientCreate, EMI_ClientCreate) \
	PORT_DEFN(EMI, ClientAfterCreate, EMI_ClientAfterCreate) \
	PORT_DEFN(EMI, ActivateHitType, Weap_ActivateHitType) \
	PORT_DEFN(EMI, CreateEMI, EMI_CreateEMI) \
	PORT_DEFN(EMI, SetUpEMI, EMI_SetUpEMI) \
	PORT_DEFN(EMI, LoadResources, EMI_LoadResources) \
	PORT_DEFN(EMI, Init, EMI_Init) \
	PORT_DEFN(Sonic, ClientCreate, Sonic_ClientCreate) \
	PORT_DEFN(Sonic, ClientAfterCreate, Sonic_ClientAfterCreate) \
	PORT_DEFN(Sonic, SonicHit, Sonic_SonicHit) \
	PORT_DEFN(Sonic, OnHit, Sonic_OnHit) \
	PORT_DEFN(Sonic, OnServerHit, Sonic_OnServerHit) \
	PORT_DEFN(Sonic, OnDeath, Sonic_OnDeath) \
	PORT_DEFN(Sonic, SetUpToDetonate, Sonic_SetUpToDetonate) \
	PORT_DEFN(Sonic, CreateSonic, Sonic_CreateSonic) \
	PORT_DEFN(Sonic, SetUpSonic, Sonic_SetUpSonic) \
	PORT_DEFN(Sonic, LoadResources, Sonic_LoadResources) \
	PORT_DEFN(Sonic, Init, Sonic_Init)

/* WeaponsTab.c */
PORT_DECL(WeaponsTab, DoNothing) /* ? */
PORT_DECL(WeaponsTab, ShowWeightPrediction) /* 3 */
PORT_DECL(WeaponsTab, HideWeightPrediction) /* 3 */
PORT_DECL(WeaponsTab, ShowWeaponDesc) /* 3 */
PORT_DECL(WeaponsTab, HideWeaponDesc) /* ? */
PORT_DECL(WeaponsTab, ShowWeaponUpgradeDesc) /* 3 */
PORT_DECL(WeaponsTab, HideWeaponUpgradeDesc) /* ? */
PORT_DECL(WeaponsTab, ShowSlotMarker) /* 3 */
PORT_DECL(WeaponsTab, HideSlotMarker) /* 3 */
PORT_DECL(WeaponsTab, LoadWeapon) /* 2 */
PORT_DECL(WeaponsTab, HideSlots) /* 2 */
PORT_DECL(WeaponsTab, FillSlots) /* 2 */
PORT_DECL(WeaponsTab, UpdateUpgrades) /* 2 */
PORT_DECL(WeaponsTab, SelectWeapon) /* 2 */
PORT_DECL(WeaponsTab, SelectUpgrade) /* 2 */
PORT_DECL(WeaponsTab, SetWeaponsPanel) /* 1 */
PORT_DECL(WeaponsTab, ShowWeaponsPanel) /* 1 */
PORT_DECL(WeaponsTab, HideWeaponsPanel) /* 1 */
#define PORTS_WeaponsTab \
	PORT_DEFN(WeaponsTab, DoNothing, Mode_PyNone2) \
	PORT_DEFN(WeaponsTab, ShowWeightPrediction, WT_ShowWeightPrediction) \
	PORT_DEFN(WeaponsTab, HideWeightPrediction, WT_HideWeightPrediction) \
	PORT_DEFN(WeaponsTab, ShowWeaponDesc, WT_ShowWeaponDescPy) \
	PORT_DEFN(WeaponsTab, HideWeaponDesc, WT_HideWeaponDesc) \
	PORT_DEFN(WeaponsTab, ShowWeaponUpgradeDesc, WT_ShowWeaponUpgradeDescPy) \
	PORT_DEFN(WeaponsTab, HideWeaponUpgradeDesc, WT_HideWeaponUpgradeDesc) \
	PORT_DEFN(WeaponsTab, ShowSlotMarker, WT_ShowSlotMarkerPy) \
	PORT_DEFN(WeaponsTab, HideSlotMarker, WT_HideSlotMarkerPy) \
	PORT_DEFN(WeaponsTab, LoadWeapon, WT_LoadWeaponPy) \
	PORT_DEFN(WeaponsTab, HideSlots, WT_HideSlotsPy) \
	PORT_DEFN(WeaponsTab, FillSlots, WT_FillSlotsPy) \
	PORT_DEFN(WeaponsTab, UpdateUpgrades, WT_UpdateUpgradesPy) \
	PORT_DEFN(WeaponsTab, SelectWeapon, WT_SelectWeaponPy) \
	PORT_DEFN(WeaponsTab, SelectUpgrade, WT_SelectUpgradePy) \
	PORT_DEFN(WeaponsTab, SetWeaponsPanel, WT_SetWeaponsPanelPy) \
	PORT_DEFN(WeaponsTab, ShowWeaponsPanel, WT_ShowWeaponsPanelPy) \
	PORT_DEFN(WeaponsTab, HideWeaponsPanel, WT_HideWeaponsPanelPy)

/* levels/AB.c */
PORT_DECLL(ab, MapFinalFight, GetBossProfile) /* 0 */
PORT_DECLL(ab, MapFinalFight, FakeShips) /* 0 */
PORT_DECLL(ab, MapFinalFight, InitStatus) /* 1 */
PORT_DECLL(ab, MapFinalFight, Init) /* 1 */
PORT_DECLL(ab, MapFinalFight, CreateBoss) /* 0 */
PORT_DECLL(ab, MapFinalFight, SetShieldPhase) /* 0 */
PORT_DECLL(ab, MapFinalFight, MissionScorer) /* 1 */
PORT_DECLL(ab, MapFinalFight, SetFase) /* 0 */
PORT_DECLL(ab, MapFinalFight, BossAttacks) /* 0 */
PORT_DECLL(ab, MapFinalFight, BossRelax) /* 0 */
PORT_DECLL(ab, MapFinalFight, OnPlayerDeath) /* 2 */
PORT_DECLL(ab, MapFinalFight, RespawnPlayer) /* 3 */
PORT_DECLL(ab, MapFinalFight, LaunchTaunt) /* 0 */
PORT_DECLL(ab, MapFinalFight, OnBossRecoversZero) /* 2 */
PORT_DECLL(ab, MapFinalFight, OnBossRecoversEnd) /* 2 */
PORT_DECLL(ab, MapFinalFight, OnBossDestroyed) /* 2 */
PORT_DECLL(ab, MapFinalFight, PreLoadFinal) /* 0 */
PORT_DECLL(ab, Map, onTrackCreate) /* 1 */
PORT_DECLL(ab, Map, InitStatus) /* 1 */
PORT_DECLL(ab, Map, Init) /* 1 */
PORT_DECLL(ab, Map, CreateAsteroid) /* 2 */
PORT_DECLL(ab, Map, detraffic) /* 1 */
PORT_DECLL(ab, Map, OnDeath) /* 2 */
PORT_DECLL(ab, Map, RealOnDeath) /* 2 */
#define PORTS_levels_AB \
	PORT_DEFLN(ab, MapFinalFight, GetBossProfile, Ff_GetBossProfilePy) \
	PORT_DEFLN(ab, MapFinalFight, FakeShips, Ff_FakeShipsPy) \
	PORT_DEFLN(ab, MapFinalFight, InitStatus, Ff_InitStatusPy) \
	PORT_DEFLN(ab, MapFinalFight, Init, Ff_InitPy) \
	PORT_DEFLN(ab, MapFinalFight, CreateBoss, Ff_CreateBossPy) \
	PORT_DEFLN(ab, MapFinalFight, SetShieldPhase, Ff_SetShieldPhasePy) \
	PORT_DEFLN(ab, MapFinalFight, MissionScorer, Ff_MissionScorerPy) \
	PORT_DEFLN(ab, MapFinalFight, SetFase, Ff_SetFasePy) \
	PORT_DEFLN(ab, MapFinalFight, BossAttacks, Ff_BossAttacksPy) \
	PORT_DEFLN(ab, MapFinalFight, BossRelax, Ff_BossRelaxPy) \
	PORT_DEFLN(ab, MapFinalFight, OnPlayerDeath, Ff_OnPlayerDeathPy) \
	PORT_DEFLN(ab, MapFinalFight, RespawnPlayer, Ff_RespawnPlayerPy) \
	PORT_DEFLN(ab, MapFinalFight, LaunchTaunt, Ff_LaunchTauntPy) \
	PORT_DEFLN(ab, MapFinalFight, OnBossRecoversZero, Ff_OnBossRecoversZeroPy) \
	PORT_DEFLN(ab, MapFinalFight, OnBossRecoversEnd, Ff_OnBossRecoversEndPy) \
	PORT_DEFLN(ab, MapFinalFight, OnBossDestroyed, Ff_OnBossDestroyedPy) \
	PORT_DEFLN(ab, MapFinalFight, PreLoadFinal, Ff_PreLoadFinalPy) \
	PORT_DEFLN(ab, Map, onTrackCreate, AbMap_onTrackCreatePy) \
	PORT_DEFLN(ab, Map, InitStatus, AbMap_InitStatusPy) \
	PORT_DEFLN(ab, Map, Init, AbMap_InitPy) \
	PORT_DEFLN(ab, Map, CreateAsteroid, AbMap_CreateAsteroidPy) \
	PORT_DEFLN(ab, Map, detraffic, AbMap_detrafficPy) \
	PORT_DEFLN(ab, Map, OnDeath, AbMap_OnDeathPy) \
	PORT_DEFLN(ab, Map, RealOnDeath, AbMap_RealOnDeathPy)

/* levels/ABIndoor.c */
PORT_DECLL(abindoor, Map, InitStatus) /* 1 */
PORT_DECLL(abindoor, Map, Init) /* 1 */
PORT_DECLL(abindoor, Map, Goodbye) /* ? */
PORT_DECLL(abindoor, Map, LaunchCinematic) /* 4 */
PORT_DECLL(abindoor, Map, ShowEndTitle) /* 1 */
PORT_DECLL(abindoor, Map, FadeEndTitle) /* 1 */
#define PORTS_levels_ABIndoor \
	PORT_DEFLN(abindoor, Map, InitStatus, Abi_InitStatusPy) \
	PORT_DEFLN(abindoor, Map, Init, Abi_InitPy) \
	PORT_DEFLNK(abindoor, Map, Goodbye, Abi_GoodbyePy) \
	PORT_DEFLN(abindoor, Map, LaunchCinematic, Abi_LaunchCinematicPy) \
	PORT_DEFLN(abindoor, Map, ShowEndTitle, Abi_ShowEndTitlePy) \
	PORT_DEFLN(abindoor, Map, FadeEndTitle, Abi_FadeEndTitlePy)

/* levels/Bank.c */
PORT_DECLL(bank, Map, InitStatus) /* 1 */
PORT_DECLL(bank, Map, Init) /* 1 */
PORT_DECLL(bank, MapTreasure, InitDeal) /* 2 */
PORT_DECLL(bank, MapTreasure, TakeMiniDeal) /* 2 */
PORT_DECLL(bank, MapTreasure, BlinkBigPrize) /* 1 */
PORT_DECLL(bank, MapTreasure, BigPrize) /* 2 */
PORT_DECLL(bank, MapTreasure, DeleteShent) /* 0 */
PORT_DECLL(bank, MapTreasure, RestartShents) /* 0 */
PORT_DECLL(bank, MapTreasure, InitStatus) /* 1 */
PORT_DECLL(bank, MapTreasure, MakeCoin) /* 3 */
PORT_DECLL(bank, MapTreasure, CreateCoins) /* 0 */
PORT_DECLL(bank, Mission_CatchRudolph, InitStatus) /* 1 */
PORT_DECLL(bank, Mission_CatchRudolph, CreateRudolph) /* 1 */
PORT_DECLL(bank, Mission_CatchRudolph, OnCatchRudolph) /* 4 */
PORT_DECLL(bank, Mission_CatchRudolph, SetNextMission) /* 0 */
PORT_DECLL(bank, Mission_GetDataPart2, ComputeAutoUpdateStopValue) /* 0 */
PORT_DECLL(bank, Mission_GetDataPart2, InitStatus) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, Init) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, TalkToBoss) /* 0 */
PORT_DECLL(bank, Mission_GetDataPart2, BossSpeech) /* 4 */
PORT_DECLL(bank, Mission_GetDataPart2, PreInitMission) /* 0 */
PORT_DECLL(bank, Mission_GetDataPart2, InitMission) /* 0 */
PORT_DECLL(bank, Mission_GetDataPart2, SetDesktopState) /* 2 */
PORT_DECLL(bank, Mission_GetDataPart2, CreateFuncInit) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, OnFunctionarySpeech) /* 4 */
PORT_DECLL(bank, Mission_GetDataPart2, OnFuncDefState) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, SetStopDownloadingState) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, SetNormalState) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, AbortDownload) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, OnDownloadAborted) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, OnBeginDownloadData) /* 4 */
PORT_DECLL(bank, Mission_GetDataPart2, OnFuncStartsWorking) /* 1 */
PORT_DECLL(bank, Mission_GetDataPart2, OnDownloadComplete) /* 0 */
PORT_DECLL(bank, Mission_GetDataPart2, MissionScorer) /* 1 */
PORT_DECLL(bank, Mission_InfiltrateBank, InitStatus) /* 1 */
PORT_DECLL(bank, Mission_InfiltrateBank, Init) /* 1 */
PORT_DECLL(bank, Mission_InfiltrateBank, UseDoor) /* 4 */
PORT_DECLL(bank, Mission_InfiltrateBank, TestEndOfElevator) /* 0 */
PORT_DECLL(bank, Mission_InfiltrateBank, ReAssignTarget) /* 0 */
PORT_DECLL(bank, Mission_InfiltrateBank, AfterDTMessage) /* 0 */
PORT_DECLL(bank, Mission_InfiltrateBank, ShowDeepThroatMessage) /* 0 */
PORT_DECLL(bank, Mission_InfiltrateBank, SetArrowToBankMaster) /* 0 */
PORT_DECLL(bank, Mission_InfiltrateBank, CreateBankMaster) /* 1 */
PORT_DECLL(bank, Mission_InfiltrateBank, LaunchScene) /* 1 */
PORT_DECLL(bank, Mission_InfiltrateBank, LaunchScene2) /* 1 */
PORT_DECLL(bank, Mission_InfiltrateBank, BankMasterSpeech) /* 4 */
PORT_DECLL(bank, Mission_InfiltrateBank, SetNextMission) /* 0 */
#define PORTS_levels_Bank \
	PORT_DEFLN(bank, Map, InitStatus, BankMap_InitStatusPy) \
	PORT_DEFLN(bank, Map, Init, BankMap_InitPy) \
	PORT_DEFLN(bank, MapTreasure, InitDeal, MapTreasure_InitDealPy) \
	PORT_DEFLN(bank, MapTreasure, TakeMiniDeal, MapTreasure_TakeMiniDealPy) \
	PORT_DEFLN(bank, MapTreasure, BlinkBigPrize, MapTreasure_BlinkBigPrizePy) \
	PORT_DEFLN(bank, MapTreasure, BigPrize, MapTreasure_BigPrizePy) \
	PORT_DEFLN(bank, MapTreasure, DeleteShent, MapTreasure_DeleteShentPy) \
	PORT_DEFLN(bank, MapTreasure, RestartShents, MapTreasure_RestartShentsPy) \
	PORT_DEFLN(bank, MapTreasure, InitStatus, MapTreasure_InitStatusPy) \
	PORT_DEFLN(bank, MapTreasure, MakeCoin, MapTreasure_MakeCoinPy) \
	PORT_DEFLN(bank, MapTreasure, CreateCoins, MapTreasure_CreateCoinsPy) \
	PORT_DEFLN(bank, Mission_CatchRudolph, InitStatus, CatchRudolph_InitStatusPy) \
	PORT_DEFLN(bank, Mission_CatchRudolph, CreateRudolph, CatchRudolph_CreateRudolphPy) \
	PORT_DEFLN(bank, Mission_CatchRudolph, OnCatchRudolph, CatchRudolph_OnCatchRudolphPy) \
	PORT_DEFLN(bank, Mission_CatchRudolph, SetNextMission, CatchRudolph_SetNextMissionPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, ComputeAutoUpdateStopValue, GetDataPart2_ComputeAutoUpdateStopValuePy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, InitStatus, GetDataPart2_InitStatusPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, Init, GetDataPart2_InitPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, TalkToBoss, GetDataPart2_TalkToBossPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, BossSpeech, GetDataPart2_BossSpeechPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, PreInitMission, GetDataPart2_PreInitMissionPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, InitMission, GetDataPart2_InitMissionPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, SetDesktopState, GetDataPart2_SetDesktopStatePy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, CreateFuncInit, GetDataPart2_CreateFuncInitPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, OnFunctionarySpeech, GetDataPart2_OnFunctionarySpeechPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, OnFuncDefState, GetDataPart2_OnFuncDefStatePy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, SetStopDownloadingState, GetDataPart2_SetStopDownloadingStatePy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, SetNormalState, GetDataPart2_SetNormalStatePy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, AbortDownload, GetDataPart2_AbortDownloadPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, OnDownloadAborted, GetDataPart2_OnDownloadAbortedPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, OnBeginDownloadData, GetDataPart2_OnBeginDownloadDataPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, OnFuncStartsWorking, GetDataPart2_OnFuncStartsWorkingPy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, OnDownloadComplete, GetDataPart2_OnDownloadCompletePy) \
	PORT_DEFLN(bank, Mission_GetDataPart2, MissionScorer, GetDataPart2_MissionScorerPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, InitStatus, InfiltrateBank_InitStatusPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, Init, InfiltrateBank_InitPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, UseDoor, InfiltrateBank_UseDoorPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, TestEndOfElevator, InfiltrateBank_TestEndOfElevatorPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, ReAssignTarget, InfiltrateBank_ReAssignTargetPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, AfterDTMessage, InfiltrateBank_AfterDTMessagePy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, ShowDeepThroatMessage, InfiltrateBank_ShowDeepThroatMessagePy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, SetArrowToBankMaster, InfiltrateBank_SetArrowToBankMasterPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, CreateBankMaster, InfiltrateBank_CreateBankMasterPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, LaunchScene, InfiltrateBank_LaunchScenePy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, LaunchScene2, InfiltrateBank_LaunchScene2Py) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, BankMasterSpeech, InfiltrateBank_BankMasterSpeechPy) \
	PORT_DEFLN(bank, Mission_InfiltrateBank, SetNextMission, InfiltrateBank_SetNextMissionPy)

/* levels/Commercial.c */
PORT_DECLL(commercial, Map, InitStatus) /* 1 */
PORT_DECLL(commercial, Map, Init) /* 1 */
PORT_DECLL(commercial, MapRace, InitStatus) /* 1 */
PORT_DECLL(commercial, MapRace, Init) /* 1 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial1) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial2) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial3) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial4) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial5) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial6) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial7) /* 0 */
PORT_DECLL(commercial, MapRace, EasyRaceCommercial8) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, GetFuncEnemiesProfile) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_DestroyMickus, Init) /* 1 */
PORT_DECLL(commercial, Mission_DestroyMickus, CreateTransports) /* 1 */
PORT_DECLL(commercial, Mission_DestroyMickus, InitMission) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, StopMission) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, OnMickusDamage) /* 3 */
PORT_DECLL(commercial, Mission_DestroyMickus, ScanShip) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, CheckContinueScanning) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, ResetScan) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, ResetScan2) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, OnFinishScan) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, MickusIdentified) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, ReassignTarget) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, TraitoriusFuncEnemiesCom) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, CheckMickusVisibility) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, CreateFuncEnemies) /* 1 */
PORT_DECLL(commercial, Mission_DestroyMickus, OnFuncEnemiesDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyMickus, OnTransportsDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyMickus, DropMoney) /* 1 */
PORT_DECLL(commercial, Mission_DestroyMickus, TakeMoney) /* 2 */
PORT_DECLL(commercial, Mission_DestroyMickus, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_DestroyMickus, MissionScorer) /* 1 */
PORT_DECLL(commercial, Mission_ProtectGus, CreateLonelyMercProfile) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, CreateBishopGusProfile) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_ProtectGus, Init) /* 1 */
PORT_DECLL(commercial, Mission_ProtectGus, InitMission) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, AttackEachOther) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, CreateLonelyMerc) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, CreateBishopGus) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, OnLonelyMercDeath) /* 2 */
PORT_DECLL(commercial, Mission_ProtectGus, OnBishopGusDeath) /* 2 */
PORT_DECLL(commercial, Mission_ProtectGus, PreCallDeepThroat) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, SetPhoneCab) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, UsePhoneCab) /* 4 */
PORT_DECLL(commercial, Mission_ProtectGus, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_ProtectGus, MissionScorer) /* 1 */
PORT_DECLL(commercial, Mission_DestroyStupidus, GetStupidusProfile) /* 0 */
PORT_DECLL(commercial, Mission_DestroyStupidus, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_DestroyStupidus, Init) /* 1 */
PORT_DECLL(commercial, Mission_DestroyStupidus, CreateStupidus) /* 0 */
PORT_DECLL(commercial, Mission_DestroyStupidus, OnStupidusDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyStupidus, OnStupidusDamage) /* 3 */
PORT_DECLL(commercial, Mission_DestroyStupidus, DropAmplifier) /* 1 */
PORT_DECLL(commercial, Mission_DestroyStupidus, TakeAmplifier) /* 2 */
PORT_DECLL(commercial, Mission_DestroyStupidus, BertoFound) /* 0 */
PORT_DECLL(commercial, Mission_DestroyStupidus, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_Win1stRace, Init) /* 1 */
PORT_DECLL(commercial, Mission_Win1stRace, InitTimeRace) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, OnRaceStart) /* 2 */
PORT_DECLL(commercial, Mission_Win1stRace, RaceGo) /* 2 */
PORT_DECLL(commercial, Mission_Win1stRace, OnCheckPoint) /* 2 */
PORT_DECLL(commercial, Mission_Win1stRace, OnAlarmOn) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, OnAlarmOff) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, TimeExpired) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, PreSetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_Win1stRace, MissionScorer) /* 1 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, Init) /* 1 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, AfterDeepThroatMessageShown) /* 0 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, AfterDeepThroatMessageShown2) /* 0 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, RestoreDeepThroatMessages) /* 0 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, CancelDeepThroatMessages) /* 0 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, SetPhoneCab) /* 0 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, UsePhoneCab) /* 4 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_FirstMeetDeepThroat, CGCalls) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, GetFunctionariesEnemiesProfile) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_GetDataPart3, Init) /* 1 */
PORT_DECLL(commercial, Mission_GetDataPart3, InitMission) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, StopMission) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, CreateWave) /* 1 */
PORT_DECLL(commercial, Mission_GetDataPart3, CreateFunctionary) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, ReinsertFunctionaryInWave) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, OnFunctionaryDeath) /* 2 */
PORT_DECLL(commercial, Mission_GetDataPart3, OnFunctionaryDamage) /* 3 */
PORT_DECLL(commercial, Mission_GetDataPart3, DropData) /* 1 */
PORT_DECLL(commercial, Mission_GetDataPart3, CheckAITakesData) /* 1 */
PORT_DECLL(commercial, Mission_GetDataPart3, CheckAI) /* 1 */
PORT_DECLL(commercial, Mission_GetDataPart3, TakeData) /* 2 */
PORT_DECLL(commercial, Mission_GetDataPart3, CallBoss) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, UsePhoneCab) /* 4 */
PORT_DECLL(commercial, Mission_GetDataPart3, PreGetData) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, GetData) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, PreCallDeepThroat) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, PreCallDeepThroat2) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, CallDeepThroat) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, UsePhoneCab2) /* 4 */
PORT_DECLL(commercial, Mission_GetDataPart3, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_GetDataPart3, MissionScorer) /* 1 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, GetCrazyGamblerProfile) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, Init) /* 1 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, LaunchFirstTaunt) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, LaunchTaunts) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, RefreshScorer) /* 1 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, CreateCrazyGambler) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, CreateCrazyGamblers) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, OnCrazyGamblerDeath) /* 2 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, AfterDeepThroatMessage) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, DropBossMatrix) /* 1 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, TakeMatrix) /* 2 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_DefeatCrazyGambler3, MissionScorer) /* 1 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, GetBishopsEnemiesProfile) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, Init) /* 1 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, InitMission) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, CreateMayorTransport) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, CreatePorkinsTransport) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, CreateBishop) /* ? */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, AttackMayorTransport) /* 1 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, OnPorkinsTransportDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, OnMayorTransportDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, CheckBishopWithoutTarget) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, OnBishopDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, OnBishopDamage) /* 3 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, GetMayorReward) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, PreCallDeepThroat) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, CallDeepThroat) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, UsePhoneCab) /* 4 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, SetNextMission) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, UpdateScorer) /* 0 */
PORT_DECLL(commercial, Mission_DestroyPorkinsTransports, MissionScorer) /* 1 */
#define PORTS_levels_Commercial \
	PORT_DEFLN(commercial, Map, InitStatus, CmMap_InitStatusA) \
	PORT_DEFLN(commercial, Map, Init, CmMap_InitA) \
	PORT_DEFLN(commercial, MapRace, InitStatus, MapRace_PassA) \
	PORT_DEFLN(commercial, MapRace, Init, MapRace_PassA) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial1, MapRace_Race1) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial2, MapRace_Race2) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial3, MapRace_Race3) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial4, MapRace_Race4) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial5, MapRace_Race5) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial6, MapRace_Race6) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial7, MapRace_Race7) \
	PORT_DEFLN(commercial, MapRace, EasyRaceCommercial8, MapRace_Race8) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, GetFuncEnemiesProfile, Mickus_GetFuncEnemiesProfileA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, InitStatus, Mickus_InitStatus) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, Init, Mickus_InitA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, CreateTransports, Mickus_CreateTransportsA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, InitMission, Mickus_InitMissionA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, StopMission, Mickus_StopMissionA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, OnMickusDamage, Mickus_OnMickusDamageA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, ScanShip, Mickus_ScanShipA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, CheckContinueScanning, Mickus_CheckContinueScanningA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, ResetScan, Mickus_ResetScanA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, ResetScan2, Mickus_ResetScan2A) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, OnFinishScan, Mickus_OnFinishScanA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, MickusIdentified, Mickus_MickusIdentifiedA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, ReassignTarget, Mickus_ReassignTargetA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, TraitoriusFuncEnemiesCom, Mickus_TraitoriusFuncEnemiesComA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, CheckMickusVisibility, Mickus_CheckMickusVisibilityA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, CreateFuncEnemies, Mickus_CreateFuncEnemiesA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, OnFuncEnemiesDeath, Mickus_OnFuncEnemiesDeathA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, OnTransportsDeath, Mickus_OnTransportsDeathA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, DropMoney, Mickus_DropMoneyA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, TakeMoney, Mickus_TakeMoneyA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, SetNextMission, Mickus_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_DestroyMickus, MissionScorer, Mickus_MissionScorerA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, CreateLonelyMercProfile, Gus_CreateLonelyMercProfileA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, CreateBishopGusProfile, Gus_CreateBishopGusProfileA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, InitStatus, Gus_InitStatus) \
	PORT_DEFLN(commercial, Mission_ProtectGus, Init, Gus_InitA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, InitMission, Gus_InitMissionA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, AttackEachOther, Gus_AttackEachOtherA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, CreateLonelyMerc, Gus_CreateLonelyMercA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, CreateBishopGus, Gus_CreateBishopGusA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, OnLonelyMercDeath, Gus_OnLonelyMercDeathA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, OnBishopGusDeath, Gus_OnBishopGusDeathA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, PreCallDeepThroat, Gus_PreCallDeepThroatA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, SetPhoneCab, Gus_SetPhoneCabA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, UsePhoneCab, Gus_UsePhoneCabA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, SetNextMission, Gus_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_ProtectGus, MissionScorer, Gus_MissionScorerA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, GetStupidusProfile, Stupidus_GetStupidusProfileA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, InitStatus, Stupidus_InitStatus) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, Init, Stupidus_InitA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, CreateStupidus, Stupidus_CreateStupidusA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, OnStupidusDeath, Stupidus_OnStupidusDeathA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, OnStupidusDamage, Stupidus_OnStupidusDamageA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, DropAmplifier, Stupidus_DropAmplifierA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, TakeAmplifier, Stupidus_TakeAmplifierA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, BertoFound, Stupidus_BertoFoundA) \
	PORT_DEFLN(commercial, Mission_DestroyStupidus, SetNextMission, Stupidus_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, InitStatus, Race_InitStatus) \
	PORT_DEFLN(commercial, Mission_Win1stRace, Init, Race_InitA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, InitTimeRace, Race_InitTimeRaceA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, OnRaceStart, Race_OnRaceStartA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, RaceGo, Race_RaceGoA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, OnCheckPoint, Race_OnCheckPointA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, OnAlarmOn, Race_OnAlarmOnA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, OnAlarmOff, Race_OnAlarmOffA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, TimeExpired, Race_TimeExpiredA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, PreSetNextMission, Race_PreSetNextMissionA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, SetNextMission, Race_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_Win1stRace, MissionScorer, Race_MissionScorerA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, InitStatus, Deep_InitStatus) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, Init, Deep_InitA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, AfterDeepThroatMessageShown, Deep_AfterDeepThroatMessageShownA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, AfterDeepThroatMessageShown2, Deep_AfterDeepThroatMessageShown2A) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, RestoreDeepThroatMessages, Deep_RestoreDeepThroatMessagesA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, CancelDeepThroatMessages, Deep_CancelDeepThroatMessagesA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, SetPhoneCab, Deep_SetPhoneCabA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, UsePhoneCab, Deep_UsePhoneCabA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, SetNextMission, Deep_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_FirstMeetDeepThroat, CGCalls, Deep_CGCallsA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, GetFunctionariesEnemiesProfile, Data_GetFunctionariesEnemiesProfileA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, InitStatus, Data_InitStatus) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, Init, Data_InitA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, InitMission, Data_InitMissionA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, StopMission, Data_StopMissionA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, CreateWave, Data_CreateWaveA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, CreateFunctionary, Data_CreateFunctionaryA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, ReinsertFunctionaryInWave, Data_ReinsertFunctionaryInWaveA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, OnFunctionaryDeath, Data_OnFunctionaryDeathA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, OnFunctionaryDamage, Data_OnFunctionaryDamageA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, DropData, Data_DropDataA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, CheckAITakesData, Data_CheckAITakesDataA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, CheckAI, Data_CheckAIA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, TakeData, Data_TakeDataA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, CallBoss, Data_CallBossA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, UsePhoneCab, Data_UsePhoneCabA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, PreGetData, Data_PreGetDataA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, GetData, Data_GetDataA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, PreCallDeepThroat, Data_PreCallDeepThroatA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, PreCallDeepThroat2, Data_PreCallDeepThroat2A) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, CallDeepThroat, Data_CallDeepThroatA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, UsePhoneCab2, Data_UsePhoneCab2A) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, SetNextMission, Data_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_GetDataPart3, MissionScorer, Data_MissionScorerA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, GetCrazyGamblerProfile, Gambler_GetCrazyGamblerProfileA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, InitStatus, Gambler_InitStatus) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, Init, Gambler_InitA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, LaunchFirstTaunt, Gambler_LaunchFirstTauntA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, LaunchTaunts, Gambler_LaunchTauntsA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, RefreshScorer, Gambler_RefreshScorerA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, CreateCrazyGambler, Gambler_CreateCrazyGamblerA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, CreateCrazyGamblers, Gambler_CreateCrazyGamblersA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, OnCrazyGamblerDeath, Gambler_OnCrazyGamblerDeathA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, AfterDeepThroatMessage, Gambler_AfterDeepThroatMessageA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, DropBossMatrix, Gambler_DropBossMatrixA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, TakeMatrix, Gambler_TakeMatrixA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, SetNextMission, Gambler_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_DefeatCrazyGambler3, MissionScorer, Gambler_MissionScorerA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, GetBishopsEnemiesProfile, Porkins_GetBishopsEnemiesProfileA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, InitStatus, Porkins_InitStatusA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, Init, Porkins_InitA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, InitMission, Porkins_InitMissionA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, CreateMayorTransport, Porkins_CreateMayorTransportA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, CreatePorkinsTransport, Porkins_CreatePorkinsTransportA) \
	PORT_DEFLNK(commercial, Mission_DestroyPorkinsTransports, CreateBishop, Porkins_CreateBishop) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, AttackMayorTransport, Porkins_AttackMayorTransportA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, OnPorkinsTransportDeath, Porkins_OnPorkinsTransportDeathA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, OnMayorTransportDeath, Porkins_OnMayorTransportDeathA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, CheckBishopWithoutTarget, Porkins_CheckBishopWithoutTargetA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, OnBishopDeath, Porkins_OnBishopDeathA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, OnBishopDamage, Porkins_OnBishopDamageA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, GetMayorReward, Porkins_GetMayorRewardA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, PreCallDeepThroat, Porkins_PreCallDeepThroatA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, CallDeepThroat, Porkins_CallDeepThroatA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, UsePhoneCab, Porkins_UsePhoneCabA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, SetNextMission, Porkins_SetNextMissionA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, UpdateScorer, Porkins_UpdateScorerA) \
	PORT_DEFLN(commercial, Mission_DestroyPorkinsTransports, MissionScorer, Porkins_MissionScorerA)

/* levels/DownTown.c */
PORT_DECLL(downtown, Map, InitStatus) /* 1 */
PORT_DECLL(downtown, Map, Init) /* 1 */
PORT_DECLL(downtown, MapRace, EasyRaceDownTown1) /* 0 */
PORT_DECLL(downtown, MapRace, EasyRaceDownTown2) /* 0 */
PORT_DECLL(downtown, MapRace, EasyRaceDownTown3) /* 0 */
PORT_DECLL(downtown, MapRace, EasyRaceDownTown4) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown1) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown2) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown3) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown4) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown5) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown6) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown7) /* 0 */
PORT_DECLL(downtown, MapRace, MediumRaceDownTown8) /* 0 */
PORT_DECLL(downtown, MapRace, HardRaceDownTown1) /* 0 */
PORT_DECLL(downtown, MapRace, HardRaceDownTown2) /* 0 */
PORT_DECLL(downtown, MapRace, HardRaceDownTown3) /* 0 */
PORT_DECLL(downtown, MapRace, HardRaceDownTown4) /* 0 */
PORT_DECLL(downtown, Mission_FirstContactPress, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_FirstContactPress, Init) /* 1 */
PORT_DECLL(downtown, Mission_BertoMassacre, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_BertoMassacre, StartMission) /* 0 */
PORT_DECLL(downtown, Mission_BertoMassacre, DeepThroatMessage) /* 0 */
PORT_DECLL(downtown, Mission_BertoMassacre, CheckBertoDeath) /* 3 */
PORT_DECLL(downtown, Mission_BertoMassacre, OnBertoFound) /* 0 */
PORT_DECLL(downtown, Mission_BertoMassacre, DropMatrix) /* 1 */
PORT_DECLL(downtown, Mission_BertoMassacre, TakeMatrix) /* 2 */
PORT_DECLL(downtown, Mission_BertoMassacre, GetBerto) /* 0 */
PORT_DECLL(downtown, Mission_BertoMassacre, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, GetCrazyGamblerProfile) /* 0 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, Init) /* 1 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, LaunchTaunts) /* 0 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, CreateCrazyGambler) /* 0 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, OnCrazyGamblerDeath) /* 2 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_FaceTheMayor, GetMayorProfile) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_FaceTheMayor, Init) /* 1 */
PORT_DECLL(downtown, Mission_FaceTheMayor, PreLaunchTaunt) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, LaunchMayorInitialTaunt) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, CreateCrazyMayor) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, MayorOnDeath) /* 2 */
PORT_DECLL(downtown, Mission_FaceTheMayor, SetPhoneCab) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, UsePhoneCab) /* 4 */
PORT_DECLL(downtown, Mission_FaceTheMayor, EndOfMission) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, Init) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, OnVehicleCallback) /* 2 */
PORT_DECLL(downtown, Mission_Massacre, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, TimeExpired) /* 0 */
PORT_DECLL(downtown, Mission_Massacre, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, GetHumphreyProfile) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_DistractHumphrey, Init) /* 1 */
PORT_DECLL(downtown, Mission_DistractHumphrey, CallHumphrey) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, UsePhoneCab) /* 4 */
PORT_DECLL(downtown, Mission_DistractHumphrey, CreateWaypoint) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, OnStartDistraction) /* 2 */
PORT_DECLL(downtown, Mission_DistractHumphrey, EnableMissionWaypoint) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, StartCameraTravelling) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, AbortCameraTravelling) /* 2 */
PORT_DECLL(downtown, Mission_DistractHumphrey, CreateHumphrey) /* 1 */
PORT_DECLL(downtown, Mission_DistractHumphrey, LaunchMission) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, AssignNewHumphreyTarget) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, OnHumphreyDeath) /* 2 */
PORT_DECLL(downtown, Mission_DistractHumphrey, OnHumphreyDamage) /* 3 */
PORT_DECLL(downtown, Mission_DistractHumphrey, OnGeneralDamage) /* 3 */
PORT_DECLL(downtown, Mission_DistractHumphrey, OnDeath) /* 2 */
PORT_DECLL(downtown, Mission_DistractHumphrey, StopMission) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, CallDeepThroat) /* 0 */
PORT_DECLL(downtown, Mission_DistractHumphrey, UsePhoneCab2) /* 4 */
PORT_DECLL(downtown, Mission_DistractHumphrey, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_GetDataPart1, GetFuncEnemiesProfile) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_GetDataPart1, Init) /* 1 */
PORT_DECLL(downtown, Mission_GetDataPart1, InitMission) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, CreateTransport) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, CreateFunctionary) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, AddTransportToList) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, SpawnFunctionaries) /* 1 */
PORT_DECLL(downtown, Mission_GetDataPart1, OnFunctionaryDeath) /* 2 */
PORT_DECLL(downtown, Mission_GetDataPart1, OnTransportsDeath) /* 2 */
PORT_DECLL(downtown, Mission_GetDataPart1, OnTransportsDamage) /* 3 */
PORT_DECLL(downtown, Mission_GetDataPart1, SpawnData) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, DropData) /* 1 */
PORT_DECLL(downtown, Mission_GetDataPart1, DelData) /* 1 */
PORT_DECLL(downtown, Mission_GetDataPart1, TakeData) /* 2 */
PORT_DECLL(downtown, Mission_GetDataPart1, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_GetDataPart1, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_HumphreyRace, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_HumphreyRace, ShowSystemMessage) /* 0 */
PORT_DECLL(downtown, Mission_HumphreyRace, Init) /* 1 */
PORT_DECLL(downtown, Mission_HumphreyRace, ShowBoostMessage) /* 0 */
PORT_DECLL(downtown, Mission_HumphreyRace, ClosePressHangar) /* 0 */
PORT_DECLL(downtown, Mission_HumphreyRace, DelRacers) /* 1 */
PORT_DECLL(downtown, Mission_HumphreyRace, CallCrazyGambler) /* 0 */
PORT_DECLL(downtown, Mission_HumphreyRace, UsePhoneCab) /* 4 */
PORT_DECLL(downtown, Mission_HumphreyRace, PreGivePlans2) /* 0 */
PORT_DECLL(downtown, Mission_HumphreyRace, GivePlans) /* 0 */
PORT_DECLL(downtown, Mission_HumphreyRace, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, GetFunctionaryProfile) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_MultiplusRace, Init) /* 1 */
PORT_DECLL(downtown, Mission_MultiplusRace, RaceList) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, GetDealType) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, EndOfRace) /* 2 */
PORT_DECLL(downtown, Mission_MultiplusRace, SetPhoneCab) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, UsePhoneCab) /* 4 */
PORT_DECLL(downtown, Mission_MultiplusRace, InitRace) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, AfterInitRaceTaunt) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, InitKill) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, SpawnStupidus) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, SpawnStupidusFriends) /* 1 */
PORT_DECLL(downtown, Mission_MultiplusRace, StupidusFriendOnDeath) /* 2 */
PORT_DECLL(downtown, Mission_MultiplusRace, RestoreEnemies) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, StupidusOnDeath) /* 2 */
PORT_DECLL(downtown, Mission_MultiplusRace, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_MultiplusRace, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_MultiplusRace, CGCalls) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, GetMercEnemiesProfile) /* 1 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, Init) /* 1 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, MissionInit) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, OnMissionStart) /* 2 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, OnMissionStart2) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, TransGoto) /* 1 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, CreateBadGuy) /* 1 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, AppearsBadGuys) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, AppearsBadGuys2) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, NextBadGuy) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, BackToTheTransport) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, BackToPlayer) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, MadGuyDeath) /* 2 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, PreLaunchTraitorius) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, MadGuyDamage) /* 3 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, MercTauntMessager) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, OnTravelComplete) /* 2 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, ActualizeScorer) /* 3 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, KillThem) /* 0 */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, InitKillThemMission) /* ? */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, InitKillBadGuys) /* ? */
PORT_DECLL(downtown, Mission_ProtectMayorTransport, RestoreTransport) /* 1 */
PORT_DECLL(downtown, Mission_StealMayorShip, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_StealMayorShip, Init) /* 1 */
PORT_DECLL(downtown, Mission_StealMayorShip, CreateMissionShip) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, TestForMissionShip) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, SetPlatformTarget) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, TestForCorrectPlatform) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, UseParkedMayorShip) /* 4 */
PORT_DECLL(downtown, Mission_StealMayorShip, UsePhoneCab) /* 4 */
PORT_DECLL(downtown, Mission_StealMayorShip, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, CGCalls) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, StartMission) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, ContinueMission) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, SetPoliceListTargetStatus) /* 1 */
PORT_DECLL(downtown, Mission_StealMayorShip, ComeBack) /* 0 */
PORT_DECLL(downtown, Mission_StealMayorShip, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_StealMayorShip, OnPoliceDeath) /* 2 */
PORT_DECLL(downtown, Mission_StealMayorShip, ParkMayorShip) /* 0 */
#define PORTS_levels_DownTown \
	PORT_DEFLN(downtown, Map, InitStatus, DownTown_InitStatusPy) \
	PORT_DEFLN(downtown, Map, Init, DownTown_InitPy) \
	PORT_DEFLN(downtown, MapRace, EasyRaceDownTown1, MapRace_Race1) \
	PORT_DEFLN(downtown, MapRace, EasyRaceDownTown2, MapRace_Race2) \
	PORT_DEFLN(downtown, MapRace, EasyRaceDownTown3, MapRace_Race3) \
	PORT_DEFLN(downtown, MapRace, EasyRaceDownTown4, MapRace_Race4) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown1, MapRace_Race5) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown2, MapRace_Race6) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown3, MapRace_Race7) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown4, MapRace_Race8) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown5, MapRace_Race9) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown6, MapRace_Race10) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown7, MapRace_Race11) \
	PORT_DEFLN(downtown, MapRace, MediumRaceDownTown8, MapRace_Race12) \
	PORT_DEFLN(downtown, MapRace, HardRaceDownTown1, MapRace_Race13) \
	PORT_DEFLN(downtown, MapRace, HardRaceDownTown2, MapRace_Race14) \
	PORT_DEFLN(downtown, MapRace, HardRaceDownTown3, MapRace_Race15) \
	PORT_DEFLN(downtown, MapRace, HardRaceDownTown4, MapRace_Race16) \
	PORT_DEFLN(downtown, Mission_FirstContactPress, InitStatus, Press_InitStatus) \
	PORT_DEFLN(downtown, Mission_FirstContactPress, Init, Press_InitPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, InitStatus, Berto_InitStatusPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, StartMission, Berto_StartMissionPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, DeepThroatMessage, Berto_DeepThroatMessagePy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, CheckBertoDeath, Berto_CheckBertoDeathPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, OnBertoFound, Berto_OnBertoFoundPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, DropMatrix, Berto_DropMatrixPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, TakeMatrix, Berto_TakeMatrixPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, GetBerto, Berto_GetBertoPy) \
	PORT_DEFLN(downtown, Mission_BertoMassacre, MissionScorer, Berto_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, GetCrazyGamblerProfile, Gambler_GetCrazyGamblerProfile) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, InitStatus, Gambler_InitStatus) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, Init, Gambler_InitPy) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, LaunchTaunts, Gambler_LaunchTauntsPy) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, CreateCrazyGambler, Gambler_CreateCrazyGamblerPy) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, OnCrazyGamblerDeath, Gambler_OnCrazyGamblerDeathPy) \
	PORT_DEFLN(downtown, Mission_DefeatCrazyGambler1, MissionScorer, Gambler_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, GetMayorProfile, Mayor_GetMayorProfile) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, InitStatus, Mayor_InitStatus) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, Init, Mayor_InitPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, PreLaunchTaunt, Mayor_PreLaunchTauntPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, LaunchMayorInitialTaunt, Mayor_LaunchMayorInitialTauntPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, CreateCrazyMayor, Mayor_CreateCrazyMayorPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, MayorOnDeath, Mayor_MayorOnDeathPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, SetPhoneCab, Mayor_SetPhoneCabPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, UsePhoneCab, Mayor_UsePhoneCabPy) \
	PORT_DEFLN(downtown, Mission_FaceTheMayor, EndOfMission, Mayor_EndOfMissionPy) \
	PORT_DEFLN(downtown, Mission_Massacre, InitStatus, Massacre_InitStatus) \
	PORT_DEFLN(downtown, Mission_Massacre, Init, Massacre_InitPy) \
	PORT_DEFLN(downtown, Mission_Massacre, OnVehicleCallback, Massacre_OnVehicleCallbackPy) \
	PORT_DEFLN(downtown, Mission_Massacre, MissionScorer, Massacre_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_Massacre, TimeExpired, Massacre_TimeExpiredPy) \
	PORT_DEFLN(downtown, Mission_Massacre, SetNextMission, Massacre_SetNextMissionPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, GetHumphreyProfile, Hump_GetHumphreyProfile) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, InitStatus, Hump_InitStatus) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, Init, Hump_InitPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, CallHumphrey, Hump_CallHumphreyPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, UsePhoneCab, Hump_UsePhoneCabPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, CreateWaypoint, Hump_CreateWaypointPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, OnStartDistraction, Hump_OnStartDistractionPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, EnableMissionWaypoint, Hump_EnableMissionWaypointPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, StartCameraTravelling, Hump_StartCameraTravellingPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, AbortCameraTravelling, Hump_AbortCameraTravellingPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, CreateHumphrey, Hump_CreateHumphreyPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, LaunchMission, Hump_LaunchMissionPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, AssignNewHumphreyTarget, Hump_AssignNewHumphreyTargetPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, OnHumphreyDeath, Hump_OnHumphreyDeathPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, OnHumphreyDamage, Hump_OnHumphreyDamagePy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, OnGeneralDamage, Hump_OnGeneralDamagePy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, OnDeath, Hump_OnDeathPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, StopMission, Hump_StopMissionPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, CallDeepThroat, Hump_CallDeepThroatPy) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, UsePhoneCab2, Hump_UsePhoneCab2Py) \
	PORT_DEFLN(downtown, Mission_DistractHumphrey, MissionScorer, Hump_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, GetFuncEnemiesProfile, Data_GetFuncEnemiesProfile) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, InitStatus, Data_InitStatus) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, Init, Data_InitPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, InitMission, Data_InitMissionPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, CreateTransport, Data_CreateTransportPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, CreateFunctionary, Data_CreateFunctionaryPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, AddTransportToList, Data_AddTransportToListPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, SpawnFunctionaries, Data_SpawnFunctionariesPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, OnFunctionaryDeath, Data_OnFunctionaryDeathPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, OnTransportsDeath, Data_OnTransportsDeathPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, OnTransportsDamage, Data_OnTransportsDamagePy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, SpawnData, Data_SpawnDataPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, DropData, Data_DropDataPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, DelData, Data_DelDataPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, TakeData, Data_TakeDataPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, SetNextMission, Data_SetNextMissionPy) \
	PORT_DEFLN(downtown, Mission_GetDataPart1, MissionScorer, Data_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, InitStatus, Bet_InitStatusPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, ShowSystemMessage, Bet_ShowSystemMessagePy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, Init, Bet_InitPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, ShowBoostMessage, Bet_ShowBoostMessagePy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, ClosePressHangar, Bet_ClosePressHangarPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, DelRacers, Bet_DelRacersPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, CallCrazyGambler, Bet_CallCrazyGamblerPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, UsePhoneCab, Bet_UsePhoneCabPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, PreGivePlans2, Bet_PreGivePlans2Py) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, GivePlans, Bet_GivePlansPy) \
	PORT_DEFLN(downtown, Mission_HumphreyRace, SetNextMission, Bet_SetNextMissionPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, GetFunctionaryProfile, Plus_GetFunctionaryProfile) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, InitStatus, Plus_InitStatusPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, Init, Plus_InitPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, RaceList, Plus_RaceList) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, GetDealType, Plus_GetDealType) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, EndOfRace, Plus_EndOfRacePy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, SetPhoneCab, Plus_SetPhoneCabPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, UsePhoneCab, Plus_UsePhoneCabPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, InitRace, Plus_InitRacePy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, AfterInitRaceTaunt, Plus_AfterInitRaceTauntPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, InitKill, Plus_InitKillPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, SpawnStupidus, Plus_SpawnStupidusPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, SpawnStupidusFriends, Plus_SpawnStupidusFriendsPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, StupidusFriendOnDeath, Plus_StupidusFriendOnDeathPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, RestoreEnemies, Plus_RestoreEnemiesPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, StupidusOnDeath, Plus_StupidusOnDeathPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, MissionScorer, Plus_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, SetNextMission, Plus_SetNextMissionPy) \
	PORT_DEFLN(downtown, Mission_MultiplusRace, CGCalls, Plus_CGCallsPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, GetMercEnemiesProfile, Escort_GetMercEnemiesProfilePy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, InitStatus, Escort_InitStatusPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, Init, Escort_InitPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, MissionInit, Escort_MissionInitPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, OnMissionStart, Escort_OnMissionStartPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, OnMissionStart2, Escort_OnMissionStart2Py) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, TransGoto, Escort_TransGotoPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, CreateBadGuy, Escort_CreateBadGuyPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, AppearsBadGuys, Escort_AppearsBadGuysPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, AppearsBadGuys2, Escort_AppearsBadGuys2Py) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, NextBadGuy, Escort_NextBadGuyPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, BackToTheTransport, Escort_BackToTheTransportPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, BackToPlayer, Escort_BackToPlayerPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, MadGuyDeath, Escort_MadGuyDeathPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, PreLaunchTraitorius, Escort_PreLaunchTraitoriusPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, SetNextMission, Escort_SetNextMissionPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, MadGuyDamage, Escort_MadGuyDamagePy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, MercTauntMessager, Escort_MercTauntMessagerPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, OnTravelComplete, Escort_OnTravelCompletePy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, ActualizeScorer, Escort_ActualizeScorerPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, MissionScorer, Escort_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, KillThem, Escort_KillThemPy) \
	PORT_DEFLNK(downtown, Mission_ProtectMayorTransport, InitKillThemMission, Escort_InitKillThemMissionPy) \
	PORT_DEFLNK(downtown, Mission_ProtectMayorTransport, InitKillBadGuys, Escort_InitKillBadGuysPy) \
	PORT_DEFLN(downtown, Mission_ProtectMayorTransport, RestoreTransport, Escort_RestoreTransportPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, InitStatus, Steal_InitStatus) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, Init, Steal_InitPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, CreateMissionShip, Steal_CreateMissionShipPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, TestForMissionShip, Steal_TestForMissionShipPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, SetPlatformTarget, Steal_SetPlatformTargetPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, TestForCorrectPlatform, Steal_TestForCorrectPlatformPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, UseParkedMayorShip, Steal_UseParkedMayorShipPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, UsePhoneCab, Steal_UsePhoneCabPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, SetNextMission, Steal_SetNextMissionPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, CGCalls, Steal_CGCallsPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, StartMission, Steal_StartMissionPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, ContinueMission, Steal_ContinueMissionPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, SetPoliceListTargetStatus, Steal_SetPoliceListTargetStatusPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, ComeBack, Steal_ComeBackPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, MissionScorer, Steal_MissionScorerPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, OnPoliceDeath, Steal_OnPoliceDeathPy) \
	PORT_DEFLN(downtown, Mission_StealMayorShip, ParkMayorShip, Steal_ParkMayorShipPy)

/* levels/Fake.c */
PORT_DECLL(fake, Map, Init) /* 1 */
PORT_DECLL(fake, Map, PlayFake2) /* ? */
PORT_DECLL(fake, Map, NextLevel) /* ? */
PORT_DECLL(fake, Map, CreateFakeBertoShip) /* 0 */
PORT_DECLL(fake, Map, ActivateBertoEngines) /* 3 */
PORT_DECLL(fake, Map, LoadFake2Scene) /* 0 */
#define PORTS_levels_Fake \
	PORT_DEFLN(fake, Map, Init, Fk_InitPy) \
	PORT_DEFLNK(fake, Map, PlayFake2, Fk_PlayFake2Py) \
	PORT_DEFLNK(fake, Map, NextLevel, Fk_NextLevelPy) \
	PORT_DEFLN(fake, Map, CreateFakeBertoShip, Fk_CreateFakeBertoShipPy) \
	PORT_DEFLN(fake, Map, ActivateBertoEngines, Fk_ActivateBertoEnginesPy) \
	PORT_DEFLN(fake, Map, LoadFake2Scene, Fk_LoadFake2ScenePy)

/* levels/GDB.c */
PORT_DECLL(gdb, Map, InitStatus) /* 1 */
PORT_DECLL(gdb, Map, Init) /* 1 */
PORT_DECLL(gdb, Map, InitPoliceBishops) /* 0 */
PORT_DECLL(gdb, Map, EndOfAppears) /* 1 */
PORT_DECLL(gdb, Map, GiveLife) /* 0 */
PORT_DECLL(gdb, Map, NextWave) /* 0 */
PORT_DECLL(gdb, MapMatrix, EndOFScene) /* ? */
PORT_DECLL(gdb, Mission_IntoTheGDB, GetBishopProfile) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, Init) /* 1 */
PORT_DECLL(gdb, Mission_IntoTheGDB, CreateBishop) /* 1 */
PORT_DECLL(gdb, Mission_IntoTheGDB, CreateBishops) /* 2 */
PORT_DECLL(gdb, Mission_IntoTheGDB, InitMission) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, ScanShip) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, CheckContinueScanning) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, ResetScan) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, OnFinishScan) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, OnBishopsDeath) /* 2 */
PORT_DECLL(gdb, Mission_IntoTheGDB, OnBishopsDamage) /* 3 */
PORT_DECLL(gdb, Mission_IntoTheGDB, DropMatrix) /* 1 */
PORT_DECLL(gdb, Mission_IntoTheGDB, ActivateMatrix) /* 1 */
PORT_DECLL(gdb, Mission_IntoTheGDB, TakeMatrix) /* 2 */
PORT_DECLL(gdb, Mission_IntoTheGDB, RestoreMatrix) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, LaunchRestoreMatrixAnim) /* 4 */
PORT_DECLL(gdb, Mission_IntoTheGDB, RelaxBishops) /* 0 */
PORT_DECLL(gdb, Mission_IntoTheGDB, MissionScorer) /* 1 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, GetBishopProfile) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, InitMission) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, BishopsAttackPlayer) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, CreateBishop) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, CreateBishops) /* 1 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, OnBishopsDeath) /* 2 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, PreSetNextMission) /* 4 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, GetBishopProfile) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, InitMission) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, BishopsAttackPlayer) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, CreateBishop) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, CreateBishops) /* 1 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, OnBishopsDeath) /* 2 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, PreSetNextMission) /* 4 */
#define PORTS_levels_GDB \
	PORT_DEFLN(gdb, Map, InitStatus, GDB_InitStatusPy) \
	PORT_DEFLN(gdb, Map, Init, GDB_InitPy) \
	PORT_DEFLN(gdb, Map, InitPoliceBishops, GDB_InitPoliceBishopsPy) \
	PORT_DEFLN(gdb, Map, EndOfAppears, GDB_EndOfAppearsPy) \
	PORT_DEFLN(gdb, Map, GiveLife, GDB_GiveLifePy) \
	PORT_DEFLN(gdb, Map, NextWave, GDB_NextWavePy) \
	PORT_DEFLNK(gdb, MapMatrix, EndOFScene, Matrix_EndOFScene) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, GetBishopProfile, Into_GetBishopProfile) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, Init, Into_InitPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, CreateBishop, Into_CreateBishopPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, CreateBishops, Into_CreateBishopsPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, InitMission, Into_InitMissionPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, ScanShip, Into_ScanShipPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, CheckContinueScanning, Into_CheckContinueScanningPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, ResetScan, Into_ResetScanPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, OnFinishScan, Into_OnFinishScanPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, OnBishopsDeath, Into_OnBishopsDeathPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, OnBishopsDamage, Into_OnBishopsDamagePy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, DropMatrix, Into_DropMatrixPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, ActivateMatrix, Into_ActivateMatrixPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, TakeMatrix, Into_TakeMatrixPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, RestoreMatrix, Into_RestoreMatrixPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, LaunchRestoreMatrixAnim, Into_LaunchRestoreMatrixAnimPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, RelaxBishops, Into_RelaxBishopsPy) \
	PORT_DEFLN(gdb, Mission_IntoTheGDB, MissionScorer, Into_MissionScorerPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, GetBishopProfile, Re1_GetBishopProfile) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, InitMission, Re1_InitMissionPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, BishopsAttackPlayer, Re1_BishopsAttackPlayerPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, CreateBishop, Re1_CreateBishopPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, CreateBishops, Re1_CreateBishopsPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, OnBishopsDeath, Re1_OnBishopsDeathPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix, PreSetNextMission, Re1_PreSetNextMissionPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, GetBishopProfile, Re2_GetBishopProfile) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, InitMission, Re2_InitMissionPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, BishopsAttackPlayer, Re2_BishopsAttackPlayerPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, CreateBishop, Re2_CreateBishopPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, CreateBishops, Re2_CreateBishopsPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, OnBishopsDeath, Re2_OnBishopsDeathPy) \
	PORT_DEFLN(gdb, Mission_ReinsertOwnMatrix2, PreSetNextMission, Re2_PreSetNextMissionPy)

/* levels/GamblinDen.c */
PORT_DECLL(gamblinden, Map, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Map, AfterBrowseNPC) /* 0 */
PORT_DECLL(gamblinden, Map, CreateFixedInit) /* 1 */
PORT_DECLL(gamblinden, Map, CreateFixedMercInit) /* 1 */
PORT_DECLL(gamblinden, Map, CreateLonelyMerc) /* 1 */
PORT_DECLL(gamblinden, Map, Init) /* 1 */
PORT_DECLL(gamblinden, Map, LonelyMercSpeech) /* 4 */
PORT_DECLL(gamblinden, Map, CharUsrAct) /* 2 */
PORT_DECLL(gamblinden, Map, CharUsrDance) /* 2 */
PORT_DECLL(gamblinden, Map, MercDrink) /* ? */
PORT_DECLL(gamblinden, Map, ChatNPCs) /* ? */
PORT_DECLL(gamblinden, Map, CharDance) /* 1 */
PORT_DECLL(gamblinden, Map, MercenarySpeech) /* 4 */
PORT_DECLL(gamblinden, Map, StartContract) /* 1 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, MercenariesSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, SetNextMission) /* ? */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, LoadScene) /* 0 */
PORT_DECLL(gamblinden, Mission_AfterFinalFight, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_AfterFinalFight, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, TalkToIgor) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, IgorSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, TalkToCrazyGambler) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeIgorCombat, LaunchIgorCombat) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, CreateTraitoriusInit) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, TraitoriusSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, TalkToCrazyGambler) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCTF, LaunchBankersCTF) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, LaunchIgorDefeatedSpeech) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, LaunchDeepThroatMessage) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, TalkToDeepThroat) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, DeepThroatSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, TalkToPoliceBoss) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, PoliceBossSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, PoliceBossSpeech2) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, TalkToCrazyGambler) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeBankersCombat, LaunchBankersCombat) /* 0 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, TalkToCrazyGambler) /* 0 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, TalkToIgor) /* 0 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, IgorSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, TalkToMercenaries) /* 0 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, MercenaryASpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, MercenaryCSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, MercenaryBSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_IgorFirstContact, SetNextMission) /* 0 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, AfterBrowseNPC) /* 0 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, CreateMercenaryInit) /* 1 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, MercenarySpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, HumanSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, LoadScene) /* 0 */
PORT_DECLL(gamblinden, Mission_GoToGamblinDen, SetNextMission) /* ? */
PORT_DECLL(gamblinden, Mission_TalkToTraitoriusAfterCTF, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_TalkToTraitoriusAfterCTF, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_TalkToTraitoriusAfterCTF, CreateTraitoriusInit) /* 1 */
PORT_DECLL(gamblinden, Mission_TalkToTraitoriusAfterCTF, LaunchTraitoriusSpeech) /* 0 */
PORT_DECLL(gamblinden, Mission_TalkToTraitoriusAfterCTF, TraitoriusSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_TalkToTraitoriusAfterCTF, SetNextMission) /* 0 */
PORT_DECLL(gamblinden, Mission_BackFromMortalRace, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_BackFromMortalRace, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_BackFromMortalRace, TalkToDeepThroat) /* 0 */
PORT_DECLL(gamblinden, Mission_BackFromMortalRace, AfterDeepThroatMessageShown) /* 0 */
PORT_DECLL(gamblinden, Mission_BackFromMortalRace, DeepThroatSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_BackFromMortalRace, SetNextMission) /* 0 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, OnEndOfScene) /* ? */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerFirstSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerSecondSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, EndOfTalk) /* ? */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, SetNextMission) /* 0 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, LoadScene) /* 0 */
PORT_DECLL(gamblinden, Mission_ArrangeMortalRace, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeMortalRace, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_ArrangeMortalRace, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ArrangeMortalRace, LaunchMortalRace) /* 0 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, AfterBrowseNPC) /* 0 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, CreateMercenaryAInit) /* 1 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, CreateMercenaryBInit) /* 1 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, CreateMercenaryCInit) /* 1 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, MercenaryASpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, MercenaryCSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, MercenaryBSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_ThreeStooges, SetNextMission) /* 1 */
#define PORTS_levels_GamblinDen \
	PORT_DEFLN(gamblinden, Map, InitStatus, Den_InitStatus) \
	PORT_DEFLN(gamblinden, Map, AfterBrowseNPC, Den_AfterBrowseNPC) \
	PORT_DEFLN(gamblinden, Map, CreateFixedInit, Den_CreateFixedInit) \
	PORT_DEFLN(gamblinden, Map, CreateFixedMercInit, Den_CreateFixedMercInit) \
	PORT_DEFLN(gamblinden, Map, CreateLonelyMerc, Den_CreateLonelyMerc) \
	PORT_DEFLN(gamblinden, Map, Init, Den_Init) \
	PORT_DEFLN(gamblinden, Map, LonelyMercSpeech, Den_LonelyMercSpeech) \
	PORT_DEFLN(gamblinden, Map, CharUsrAct, Den_CharUsrAct) \
	PORT_DEFLN(gamblinden, Map, CharUsrDance, Den_CharUsrDance) \
	PORT_DEFLNK(gamblinden, Map, MercDrink, Den_MercDrink) \
	PORT_DEFLNK(gamblinden, Map, ChatNPCs, Den_ChatNPCs) \
	PORT_DEFLN(gamblinden, Map, CharDance, Den_CharDance) \
	PORT_DEFLN(gamblinden, Map, MercenarySpeech, Den_MercenarySpeech) \
	PORT_DEFLN(gamblinden, Map, StartContract, Den_StartContract) \
	PORT_DEFLN(gamblinden, Mission_BeforeFinalFight, InitStatus, Before_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_BeforeFinalFight, Init, Before_Init) \
	PORT_DEFLN(gamblinden, Mission_BeforeFinalFight, CrazyGamblerSpeech, Before_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_BeforeFinalFight, MercenariesSpeech, Before_MercenariesSpeech) \
	PORT_DEFLNK(gamblinden, Mission_BeforeFinalFight, SetNextMission, Before_SetNextMission) \
	PORT_DEFLN(gamblinden, Mission_BeforeFinalFight, LoadScene, Before_LoadScene) \
	PORT_DEFLN(gamblinden, Mission_AfterFinalFight, InitStatus, After_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_AfterFinalFight, Init, After_Init) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, InitStatus, Igor_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, Init, Igor_Init) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, TalkToIgor, Igor_TalkToIgor) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, IgorSpeech, Igor_IgorSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, TalkToCrazyGambler, Igor_TalkToCrazyGambler) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, CrazyGamblerSpeech, Igor_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeIgorCombat, LaunchIgorCombat, Igor_LaunchIgorCombat) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, InitStatus, Ctf_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, Init, Ctf_Init) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, CreateTraitoriusInit, Ctf_CreateTraitoriusInit) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, TraitoriusSpeech, Ctf_TraitoriusSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, TalkToCrazyGambler, Ctf_TalkToCrazyGambler) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, CrazyGamblerSpeech, Ctf_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCTF, LaunchBankersCTF, Ctf_LaunchBankersCTF) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, InitStatus, Bankers_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, Init, Bankers_Init) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, LaunchIgorDefeatedSpeech, Bankers_LaunchIgorDefeatedSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, LaunchDeepThroatMessage, Bankers_LaunchDeepThroatMessage) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, TalkToDeepThroat, Bankers_TalkToDeepThroat) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, DeepThroatSpeech, Bankers_DeepThroatSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, TalkToPoliceBoss, Bankers_TalkToPoliceBoss) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, PoliceBossSpeech, Bankers_PoliceBossSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, PoliceBossSpeech2, Bankers_PoliceBossSpeech2) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, TalkToCrazyGambler, Bankers_TalkToCrazyGambler) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, CrazyGamblerSpeech, Bankers_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeBankersCombat, LaunchBankersCombat, Bankers_LaunchBankersCombat) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, InitStatus, First_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, Init, First_Init) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, TalkToCrazyGambler, First_TalkToCrazyGambler) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, CrazyGamblerSpeech, First_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, TalkToIgor, First_TalkToIgor) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, IgorSpeech, First_IgorSpeech) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, TalkToMercenaries, First_TalkToMercenaries) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, MercenaryASpeech, First_MercenaryASpeech) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, MercenaryCSpeech, First_MercenaryCSpeech) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, MercenaryBSpeech, First_MercenaryBSpeech) \
	PORT_DEFLN(gamblinden, Mission_IgorFirstContact, SetNextMission, First_SetNextMission) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, InitStatus, Go_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, AfterBrowseNPC, Go_AfterBrowseNPC) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, CreateMercenaryInit, Go_CreateMercenaryInit) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, MercenarySpeech, Go_MercenarySpeech) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, HumanSpeech, Go_HumanSpeech) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, CrazyGamblerSpeech, Go_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, Init, Go_Init) \
	PORT_DEFLN(gamblinden, Mission_GoToGamblinDen, LoadScene, Go_LoadScene) \
	PORT_DEFLNK(gamblinden, Mission_GoToGamblinDen, SetNextMission, Go_SetNextMission) \
	PORT_DEFLN(gamblinden, Mission_TalkToTraitoriusAfterCTF, InitStatus, Traitor_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_TalkToTraitoriusAfterCTF, Init, Traitor_Init) \
	PORT_DEFLN(gamblinden, Mission_TalkToTraitoriusAfterCTF, CreateTraitoriusInit, Traitor_CreateTraitoriusInit) \
	PORT_DEFLN(gamblinden, Mission_TalkToTraitoriusAfterCTF, LaunchTraitoriusSpeech, Traitor_LaunchTraitoriusSpeech) \
	PORT_DEFLN(gamblinden, Mission_TalkToTraitoriusAfterCTF, TraitoriusSpeech, Traitor_TraitoriusSpeech) \
	PORT_DEFLN(gamblinden, Mission_TalkToTraitoriusAfterCTF, SetNextMission, Traitor_SetNextMission) \
	PORT_DEFLN(gamblinden, Mission_BackFromMortalRace, InitStatus, Back_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_BackFromMortalRace, Init, Back_Init) \
	PORT_DEFLN(gamblinden, Mission_BackFromMortalRace, TalkToDeepThroat, Back_TalkToDeepThroat) \
	PORT_DEFLN(gamblinden, Mission_BackFromMortalRace, AfterDeepThroatMessageShown, Back_AfterDeepThroatMessageShown) \
	PORT_DEFLN(gamblinden, Mission_BackFromMortalRace, DeepThroatSpeech, Back_DeepThroatSpeech) \
	PORT_DEFLN(gamblinden, Mission_BackFromMortalRace, SetNextMission, Back_SetNextMission) \
	PORT_DEFLN(gamblinden, Mission_CrazyGamblerFirstContact, InitStatus, Meet_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_CrazyGamblerFirstContact, Init, Meet_Init) \
	PORT_DEFLNK(gamblinden, Mission_CrazyGamblerFirstContact, OnEndOfScene, Meet_OnEndOfScene) \
	PORT_DEFLN(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerFirstSpeech, Meet_CrazyGamblerFirstSpeech) \
	PORT_DEFLN(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerSecondSpeech, Meet_CrazyGamblerSecondSpeech) \
	PORT_DEFLNK(gamblinden, Mission_CrazyGamblerFirstContact, EndOfTalk, Meet_EndOfTalk) \
	PORT_DEFLN(gamblinden, Mission_CrazyGamblerFirstContact, SetNextMission, Meet_SetNextMission) \
	PORT_DEFLN(gamblinden, Mission_CrazyGamblerFirstContact, LoadScene, Meet_LoadScene) \
	PORT_DEFLN(gamblinden, Mission_ArrangeMortalRace, InitStatus, Mortal_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_ArrangeMortalRace, Init, Mortal_Init) \
	PORT_DEFLN(gamblinden, Mission_ArrangeMortalRace, CrazyGamblerSpeech, Mortal_CrazyGamblerSpeech) \
	PORT_DEFLN(gamblinden, Mission_ArrangeMortalRace, LaunchMortalRace, Mortal_LaunchMortalRace) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, InitStatus, Stooges_InitStatus) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, Init, Stooges_Init) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, AfterBrowseNPC, Stooges_AfterBrowseNPC) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, CreateMercenaryAInit, Stooges_CreateMercenaryAInit) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, CreateMercenaryBInit, Stooges_CreateMercenaryBInit) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, CreateMercenaryCInit, Stooges_CreateMercenaryCInit) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, MercenaryASpeech, Stooges_MercenaryASpeech) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, MercenaryCSpeech, Stooges_MercenaryCSpeech) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, MercenaryBSpeech, Stooges_MercenaryBSpeech) \
	PORT_DEFLN(gamblinden, Mission_ThreeStooges, SetNextMission, Stooges_SetNextMission)

/* levels/Junkyard.c */
PORT_DECLL(junkyard, Map, InitStatus) /* 1 */
PORT_DECLL(junkyard, Map, Init) /* 1 */
PORT_DECLL(junkyard, MapShipEdit, EnterEditShip) /* 4 */
PORT_DECLL(junkyard, MapShipEdit, SetInteract) /* 1 */
PORT_DECLL(junkyard, MapShipEdit, OnEndOfShipEdit) /* 2 */
PORT_DECLL(junkyard, MapShipEdit, CanUseElevator) /* 4 */
PORT_DECLL(junkyard, MapShipEdit, UseElevator) /* 4 */
PORT_DECLL(junkyard, MapShipEdit, Init) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, InitStatus) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, Init) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, CreateFakeBertoShip) /* 0 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, ShowExtras) /* 0 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, StartInitialScene) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, LoadMecanicFinalScene) /* 0 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, FXMecanicFinalSputnik) /* 3 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, BertoGoHome) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, BackDownBerto) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, EndOfBertoBye) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, StartInitBuildShip) /* 0 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, InitBuildShip) /* 0 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, EnterEditShipFirstTime) /* 4 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, NoTutorialEnterEditShip) /* 2 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, OnEndOfShipEditFirstTime) /* 2 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, UseElevatorFirstTime) /* 4 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, EndOfMission) /* 1 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, EnterEditShipTutorial) /* 2 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, TutorialMsgInit) /* 2 */
PORT_DECLL(junkyard, Mission_RustyFirstContact, EditShipTutorial_Step) /* 3 */
#define PORTS_levels_Junkyard \
	PORT_DEFLN(junkyard, Map, InitStatus, JunkyardMap_InitStatusPy) \
	PORT_DEFLN(junkyard, Map, Init, JunkyardMap_InitPy) \
	PORT_DEFLN(junkyard, MapShipEdit, EnterEditShip, MapShipEdit_EnterEditShipPy) \
	PORT_DEFLN(junkyard, MapShipEdit, SetInteract, MapShipEdit_SetInteractPy) \
	PORT_DEFLN(junkyard, MapShipEdit, OnEndOfShipEdit, MapShipEdit_OnEndOfShipEditPy) \
	PORT_DEFLN(junkyard, MapShipEdit, CanUseElevator, MapShipEdit_CanUseElevatorPy) \
	PORT_DEFLN(junkyard, MapShipEdit, UseElevator, MapShipEdit_UseElevatorPy) \
	PORT_DEFLN(junkyard, MapShipEdit, Init, MapShipEdit_InitPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, InitStatus, Rusty_InitStatusPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, Init, Rusty_InitPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, CreateFakeBertoShip, Rusty_CreateFakeBertoShipPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, ShowExtras, Rusty_ShowExtrasPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, StartInitialScene, Rusty_StartInitialScenePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, LoadMecanicFinalScene, Rusty_LoadMecanicFinalScenePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, FXMecanicFinalSputnik, Rusty_FXMecanicFinalSputnikPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, BertoGoHome, Rusty_BertoGoHomePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, BackDownBerto, Rusty_BackDownBertoPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, EndOfBertoBye, Rusty_EndOfBertoByePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, StartInitBuildShip, Rusty_StartInitBuildShipPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, InitBuildShip, Rusty_InitBuildShipPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, EnterEditShipFirstTime, Rusty_EnterEditShipFirstTimePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, NoTutorialEnterEditShip, Rusty_NoTutorialEnterEditShipPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, OnEndOfShipEditFirstTime, Rusty_OnEndOfShipEditFirstTimePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, UseElevatorFirstTime, Rusty_UseElevatorFirstTimePy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, EndOfMission, Rusty_EndOfMissionPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, EnterEditShipTutorial, Rusty_EnterEditShipTutorialPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, TutorialMsgInit, Rusty_TutorialMsgInitPy) \
	PORT_DEFLN(junkyard, Mission_RustyFirstContact, EditShipTutorial_Step, Rusty_EditShipTutorial_StepPy)

/* levels/LevelCommon.c */
PORT_DECLL(commercial, Mission_DestroyCommSats2, InitStatus) /* 1 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, Init) /* 1 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, IsValidPoint) /* 3 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, CreateProbes) /* 0 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, OnProbeDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, DestroyShipsInList) /* 1 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, DestroyEnemyShips) /* 0 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, MissionScorer) /* 1 */
PORT_DECLL(commercial, Mission_DestroyCommSats2, SetNextMission) /* 0 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, Init) /* 1 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, IsValidPoint) /* 3 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, CreateProbes) /* 0 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, OnProbeDeath) /* 2 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, DestroyShipsInList) /* 1 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, DestroyEnemyShips) /* 0 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_DestroyCommSats3, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, Init) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, IsValidPoint) /* 3 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, CreateProbes) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, OnProbeDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, DestroyShipsInList) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, DestroyEnemyShips) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyCommSats1, MissionScorer) /* 1 */
#define PORTS_levels_LevelCommon \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, InitStatus, Probes1_InitStatus) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, Init, Probes1_Init) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, IsValidPoint, Probes1_IsValidPoint) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, CreateProbes, Probes1_CreateProbes) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, OnProbeDeath, Probes1_OnProbeDeath) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, DestroyShipsInList, Probes1_DestroyShipsInList) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, DestroyEnemyShips, Probes1_DestroyEnemyShips) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, MissionScorer, Probes1_MissionScorer) \
	PORT_DEFLN(commercial, Mission_DestroyCommSats2, SetNextMission, Probes1_SetNextMission) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, InitStatus, Probes2_InitStatus) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, Init, Probes2_Init) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, IsValidPoint, Probes2_IsValidPoint) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, CreateProbes, Probes2_CreateProbes) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, OnProbeDeath, Probes2_OnProbeDeath) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, DestroyShipsInList, Probes2_DestroyShipsInList) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, DestroyEnemyShips, Probes2_DestroyEnemyShips) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, MissionScorer, Probes2_MissionScorer) \
	PORT_DEFLN(downtown, Mission_DestroyCommSats3, SetNextMission, Probes2_SetNextMission) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, InitStatus, Probes0_InitStatus) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, Init, Probes0_Init) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, IsValidPoint, Probes0_IsValidPoint) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, CreateProbes, Probes0_CreateProbes) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, OnProbeDeath, Probes0_OnProbeDeath) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, DestroyShipsInList, Probes0_DestroyShipsInList) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, DestroyEnemyShips, Probes0_DestroyEnemyShips) \
	PORT_DEFLN(outskirts, Mission_DestroyCommSats1, MissionScorer, Probes0_MissionScorer)

/* levels/Menu.c */
PORT_DECLL(menu, Map, InitStatus) /* 1 */
PORT_DECLL(menu, Map, EndCredits) /* ? */
PORT_DECLL(menu, Map, FadeOutCreditsEnd) /* 0 */
PORT_DECLL(menu, Map, FadeOutCredits) /* 0 */
PORT_DECLL(menu, Map, NextCredit) /* 0 */
PORT_DECLL(menu, Map, ShowCredits) /* 0 */
PORT_DECLL(menu, Map, SetMotionBlur) /* 0 */
PORT_DECLL(menu, Map, Init) /* 1 */
PORT_DECLL(menu, Map, drotorito) /* 0 */
PORT_DECLL(menu, Map, cremeteoro) /* ? */
#define PORTS_levels_Menu \
	PORT_DEFLN(menu, Map, InitStatus, Menu_InitStatusPy) \
	PORT_DEFLNK(menu, Map, EndCredits, Menu_EndCreditsPy) \
	PORT_DEFLN(menu, Map, FadeOutCreditsEnd, Menu_FadeOutCreditsEndPy) \
	PORT_DEFLN(menu, Map, FadeOutCredits, Menu_FadeOutCreditsPy) \
	PORT_DEFLN(menu, Map, NextCredit, Menu_NextCreditPy) \
	PORT_DEFLN(menu, Map, ShowCredits, Menu_ShowCreditsPy) \
	PORT_DEFLN(menu, Map, SetMotionBlur, Menu_SetMotionBlurPy) \
	PORT_DEFLN(menu, Map, Init, Menu_InitPy) \
	PORT_DEFLN(menu, Map, drotorito, Menu_drotoritoPy) \
	PORT_DEFLNK(menu, Map, cremeteoro, Menu_cremeteoroPy)

/* levels/OM.c */
PORT_DECLL(om, SuperDeal_Sixth, SetVars) /* 0 */
PORT_DECLL(om, SuperDeal_Tenth, SetVars) /* 0 */
#define PORTS_levels_OM \
	PORT_DEFLN(om, SuperDeal_Sixth, SetVars, Om_SixthSetVarsPy) \
	PORT_DEFLN(om, SuperDeal_Tenth, SetVars, Om_TenthSetVarsPy)

/* levels/Orbit.c */
PORT_DECLL(orbit, Map, InitStatus) /* 1 */
PORT_DECLL(orbit, Map, Init) /* 1 */
PORT_DECLL(orbit, MapDecont, InitStatus) /* 1 */
PORT_DECLL(orbit, MapDecont, Decontamination) /* 4 */
PORT_DECLL(orbit, MapDecont, EndOfScene) /* 1 */
PORT_DECLL(orbit, MapDecont, DecontaminatedWaypoint) /* 0 */
PORT_DECLL(orbit, MapDecont, SetWaypoint) /* 1 */
PORT_DECLL(orbit, MapDecont, LoadDecontamination) /* 0 */
PORT_DECLL(orbit, MapDecont, Init) /* 1 */
PORT_DECLL(orbit, MapDecont, MsgOffVoice) /* 0 */
PORT_DECLL(orbit, MapDecont, SetFirstInfoPointAsMission) /* 1 */
PORT_DECLL(orbit, MapDecont, CantUseElevator) /* 4 */
PORT_DECLL(orbit, MapDecont, CloseGateKeeperDoor) /* 2 */
PORT_DECLL(orbit, MapDecont, OpenGateKeeperDoor) /* 2 */
PORT_DECLL(orbit, MapDecont, AfterBrowseNPC) /* 0 */
PORT_DECLL(orbit, MapDecont, CreateFunctionaryInit) /* 1 */
PORT_DECLL(orbit, MapDecont, CreateBishopInit) /* 1 */
PORT_DECLL(orbit, MapDecont, CreateGateKeeperInit) /* 1 */
PORT_DECLL(orbit, MapDecont, CreateSentinelInit) /* 1 */
PORT_DECLL(orbit, MapDecont, ShowSkipConvsMsg) /* 0 */
PORT_DECLL(orbit, MapDecont, GateKeeperSpeech) /* 4 */
PORT_DECLL(orbit, MapDecont, SentinelSpeech) /* 4 */
PORT_DECLL(orbit, MapDecont, GaugeBlink) /* 0 */
PORT_DECLL(orbit, MapDecont, CheckShowNextHowto) /* 0 */
PORT_DECLL(orbit, MapDecont, CreateBishopsLauncher) /* 0 */
PORT_DECLL(orbit, MapDecont, EndOfSceneBishops) /* 1 */
PORT_DECLL(orbit, MapDecont, LoadBishopsScene) /* 0 */
PORT_DECLL(orbit, MapDecont, FXBishopsPray) /* 3 */
PORT_DECLL(orbit, MapDecont, PlayBishops) /* ? */
PORT_DECLL(orbit, MapDecont, LoadFunctionaryScene) /* 0 */
PORT_DECLL(orbit, MapDecont, AddDF) /* ? */
PORT_DECLL(orbit, MapDecont, SetDFAnm) /* 3 */
PORT_DECLL(orbit, MapDecont, EndOfSceneFunctionary) /* 1 */
PORT_DECLL(orbit, MapDecont, CreateDownKeyPad) /* 0 */
PORT_DECLL(orbit, MapDecont, PlayFalling) /* 4 */
PORT_DECLL(orbit, MapDecont, JumpNextMission) /* 1 */
PORT_DECLL(orbit, MapDecont, PlayFunctionary) /* 4 */
PORT_DECLL(orbit, MapDecont, LoadFallingScene) /* 0 */
PORT_DECLL(orbit, Mission_IntroHuman, InitStatus) /* 1 */
PORT_DECLL(orbit, Mission_IntroHuman, Init) /* 1 */
PORT_DECLL(orbit, Mission_IntroHuman, BillArrives_B) /* ? */
PORT_DECLL(orbit, Mission_IntroHuman, BillArrives_C) /* ? */
PORT_DECLL(orbit, Mission_IntroHuman, SetNextMission) /* ? */
PORT_DECLL(orbit, Mission_IntroHuman, LoadSceneA) /* 0 */
PORT_DECLL(orbit, Mission_IntroHuman, LoadSceneB) /* 0 */
PORT_DECLL(orbit, Mission_IntroHuman, LoadSceneC) /* 0 */
#define PORTS_levels_Orbit \
	PORT_DEFLN(orbit, Map, InitStatus, Orbit_InitStatus) \
	PORT_DEFLN(orbit, Map, Init, Orbit_Init) \
	PORT_DEFLN(orbit, MapDecont, InitStatus, Decont_InitStatus) \
	PORT_DEFLN(orbit, MapDecont, Decontamination, Decont_Decontamination) \
	PORT_DEFLN(orbit, MapDecont, EndOfScene, Decont_EndOfScene) \
	PORT_DEFLN(orbit, MapDecont, DecontaminatedWaypoint, Decont_DecontaminatedWaypoint) \
	PORT_DEFLN(orbit, MapDecont, SetWaypoint, Decont_SetWaypoint) \
	PORT_DEFLN(orbit, MapDecont, LoadDecontamination, Decont_LoadDecontamination) \
	PORT_DEFLN(orbit, MapDecont, Init, Decont_Init) \
	PORT_DEFLN(orbit, MapDecont, MsgOffVoice, Decont_MsgOffVoice) \
	PORT_DEFLN(orbit, MapDecont, SetFirstInfoPointAsMission, Decont_SetFirstInfoPointAsMission) \
	PORT_DEFLN(orbit, MapDecont, CantUseElevator, Decont_CantUseElevator) \
	PORT_DEFLN(orbit, MapDecont, CloseGateKeeperDoor, Decont_CloseGateKeeperDoor) \
	PORT_DEFLN(orbit, MapDecont, OpenGateKeeperDoor, Decont_OpenGateKeeperDoor) \
	PORT_DEFLN(orbit, MapDecont, AfterBrowseNPC, Decont_AfterBrowseNPC) \
	PORT_DEFLN(orbit, MapDecont, CreateFunctionaryInit, Decont_CreateFunctionaryInit) \
	PORT_DEFLN(orbit, MapDecont, CreateBishopInit, Decont_CreateBishopInit) \
	PORT_DEFLN(orbit, MapDecont, CreateGateKeeperInit, Decont_CreateGateKeeperInit) \
	PORT_DEFLN(orbit, MapDecont, CreateSentinelInit, Decont_CreateSentinelInit) \
	PORT_DEFLN(orbit, MapDecont, ShowSkipConvsMsg, Decont_ShowSkipConvsMsg) \
	PORT_DEFLN(orbit, MapDecont, GateKeeperSpeech, Decont_GateKeeperSpeech) \
	PORT_DEFLN(orbit, MapDecont, SentinelSpeech, Decont_SentinelSpeech) \
	PORT_DEFLN(orbit, MapDecont, GaugeBlink, Decont_GaugeBlink) \
	PORT_DEFLN(orbit, MapDecont, CheckShowNextHowto, Decont_CheckShowNextHowto) \
	PORT_DEFLN(orbit, MapDecont, CreateBishopsLauncher, Decont_CreateBishopsLauncher) \
	PORT_DEFLN(orbit, MapDecont, EndOfSceneBishops, Decont_EndOfSceneBishops) \
	PORT_DEFLN(orbit, MapDecont, LoadBishopsScene, Decont_LoadBishopsScene) \
	PORT_DEFLN(orbit, MapDecont, FXBishopsPray, Decont_FXBishopsPray) \
	PORT_DEFLNK(orbit, MapDecont, PlayBishops, Decont_PlayBishops) \
	PORT_DEFLN(orbit, MapDecont, LoadFunctionaryScene, Decont_LoadFunctionaryScene) \
	PORT_DEFLNK(orbit, MapDecont, AddDF, Decont_AddDF) \
	PORT_DEFLN(orbit, MapDecont, SetDFAnm, Decont_SetDFAnm) \
	PORT_DEFLN(orbit, MapDecont, EndOfSceneFunctionary, Decont_EndOfSceneFunctionary) \
	PORT_DEFLN(orbit, MapDecont, CreateDownKeyPad, Decont_CreateDownKeyPad) \
	PORT_DEFLN(orbit, MapDecont, PlayFalling, Decont_PlayFalling) \
	PORT_DEFLN(orbit, MapDecont, JumpNextMission, Decont_JumpNextMission) \
	PORT_DEFLN(orbit, MapDecont, PlayFunctionary, Decont_PlayFunctionary) \
	PORT_DEFLN(orbit, MapDecont, LoadFallingScene, Decont_LoadFallingScene) \
	PORT_DEFLN(orbit, Mission_IntroHuman, InitStatus, Intro_InitStatus) \
	PORT_DEFLN(orbit, Mission_IntroHuman, Init, Intro_Init) \
	PORT_DEFLNK(orbit, Mission_IntroHuman, BillArrives_B, Intro_BillArrives_B) \
	PORT_DEFLNK(orbit, Mission_IntroHuman, BillArrives_C, Intro_BillArrives_C) \
	PORT_DEFLNK(orbit, Mission_IntroHuman, SetNextMission, Intro_SetNextMission) \
	PORT_DEFLN(orbit, Mission_IntroHuman, LoadSceneA, Intro_LoadSceneA) \
	PORT_DEFLN(orbit, Mission_IntroHuman, LoadSceneB, Intro_LoadSceneB) \
	PORT_DEFLN(orbit, Mission_IntroHuman, LoadSceneC, Intro_LoadSceneC)

/* levels/Outskirts.c */
PORT_DECLL(outskirts, Map, InitStatus) /* 1 */
PORT_DECLL(outskirts, Map, Init) /* 1 */
PORT_DECLL(outskirts, MapRace, EasyRaceOurskirts1) /* 0 */
PORT_DECLL(outskirts, MapRace, EasyRaceOurskirts2) /* 0 */
PORT_DECLL(outskirts, MapRace, EasyRaceOurskirts3) /* 0 */
PORT_DECLL(outskirts, MapRace, EasyRaceOurskirts4) /* 0 */
PORT_DECLL(outskirts, MapRace, EasyRaceOurskirts5) /* 0 */
PORT_DECLL(outskirts, MapRace, MediumRaceOurskirts1) /* 0 */
PORT_DECLL(outskirts, MapRace, MediumRaceOurskirts2) /* 0 */
PORT_DECLL(outskirts, MapRace, MediumRaceOurskirts3) /* 0 */
PORT_DECLL(outskirts, MapRace, MediumRaceOurskirts4) /* 0 */
PORT_DECLL(outskirts, MapRace, MediumRaceOurskirts5) /* 0 */
PORT_DECLL(outskirts, MapRace, Mission_Win2ndRace_Race) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, GetTraitoriusProfile) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, Init) /* 1 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, ReassignTarget) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, CreateTraitorius) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, OnTraitoriusDamage) /* 3 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, OnTraitoriusDeath) /* 2 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, TraitoriusTakesLife) /* 1 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, TimeExpired) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, PreCallTraitorius) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, CallTraitorius) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, UsePhoneCab) /* 4 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_AttackTraitorius, MissionScorer) /* 1 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, ShowSystemMessage) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, Init) /* 1 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, ShowBoostMessage) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, ClosePressHangar) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, DelRacers) /* 1 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, OnPlayerWinsRace) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, CallCrazyGambler) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, ShowRustyMapsSystemMessage) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, UsePhoneCab) /* 4 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, EnableMetro) /* 0 */
PORT_DECLL(outskirts, Mission_Demo_HumphreyRace, OnMetroUsed) /* 4 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, GetMercenariesProfile) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, GetBankDirectorProfile) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, GetBankerProfile) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, CreateWaypoint) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, CreateShip) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnInitMission) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, ShowScorer) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, EnableMissionWaypoint) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, SelectNewMissionTarget) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnMissionTargetDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, BankMasterAttacksMissionTarget) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, DropTreasure) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, DelTreasure) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, SendShipToGetTreasure) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, GetPossibleMercTargets) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, CreateMercs) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, CreateBankMaster) /* ? */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, CreateBanker) /* ? */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, RespawnBankMaster) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnBankMasterDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, CheckMercsFreezed) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnBankerDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnMercDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnTreasureReached) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnShipReachedTreasurePos) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, OnCoinTaken) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, BankMasterSpeech) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, ShowDeepThroatMessage) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyBadDebtors, MissionScorer) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, GetHumphreyProfile) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, CreateHumphrey) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, HumphreyAttacks) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, LaunchTaunt) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, OnHumphreyDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, MissionScorer) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, GetFuncEnemiesProfile) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, GetPoliceFriendsProfile) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, Init) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, CreateWaypoint) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, EnableMissionWaypoint) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, MissionScorer) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, ComeBack) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, OnChangeVehicle) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, CreateNewPoliShip) /* 3 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, CreateNewFuncShip) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, CreateFriendsShips) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, CreateEnemiesShips) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, CGCalls) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, OnFuncDeath) /* 2 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, isPoliceShip) /* 1 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, OnMissionStart) /* 2 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, LaunchMission) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, StartCameraTravelling) /* 0 */
PORT_DECLL(outskirts, Mission_FunctionaryHunting, AbortCameraTravelling) /* 2 */
PORT_DECLL(outskirts, Mission_GoToOutskirts, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_GoToOutskirts, Init) /* 1 */
PORT_DECLL(outskirts, Mission_GoToOutskirts, LaunchCrazyGamblerComunication) /* 1 */
PORT_DECLL(outskirts, Mission_GoToOutskirts, PlayCrazyGamblerComunication) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CreateLonelyMercProfiles) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, Init) /* 1 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, InitIdentifyMerc) /* ? */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CreateLonelyMercInScene) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CheckTraitoriusDeath) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, EndOfScene) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, DelLonelyMerc) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CreateLonelyMerc) /* ? */
PORT_DECLL(outskirts, Mission_IdentifyMerc, InitMission) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, ScanShip) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, OnFinishScan) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, OnLonelyMercDeath) /* 2 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, OnLonelyMercDamage) /* 3 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, PreRespawnLonelyMerc) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, RespawnLonelyMerc) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, SetPhoneCab) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, UsePhoneCab) /* 4 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, MissionScorer) /* 1 */
PORT_DECLL(outskirts, Mission_KillPolice, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_KillPolice, Init) /* 1 */
PORT_DECLL(outskirts, Mission_KillPolice, SetPos) /* 2 */
PORT_DECLL(outskirts, Mission_KillPolice, InitMission) /* 0 */
PORT_DECLL(outskirts, Mission_KillPolice, OnAlarm) /* 1 */
PORT_DECLL(outskirts, Mission_KillPolice, OnPoliceDeath) /* 2 */
PORT_DECLL(outskirts, Mission_KillPolice, BertoAppears) /* 0 */
PORT_DECLL(outskirts, Mission_KillPolice, EnableMetro) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, GetRudolphProfile) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, RaceList) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_KillRudolph, Init) /* 1 */
PORT_DECLL(outskirts, Mission_KillRudolph, GetDealType) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, InitRace) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, initCreateRudolph) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, SetRudolphDef) /* 1 */
PORT_DECLL(outskirts, Mission_KillRudolph, OnDamage) /* 3 */
PORT_DECLL(outskirts, Mission_KillRudolph, EndOfRace) /* 2 */
PORT_DECLL(outskirts, Mission_KillRudolph, SetRudolphAsNone) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, RudolphKilled) /* 2 */
PORT_DECLL(outskirts, Mission_KillRudolph, CreateClue) /* 1 */
PORT_DECLL(outskirts, Mission_KillRudolph, ActivateClue) /* 1 */
PORT_DECLL(outskirts, Mission_KillRudolph, OnClueTaken) /* 2 */
PORT_DECLL(outskirts, Mission_KillRudolph, InitTalkToPoliceBoss) /* 0 */
PORT_DECLL(outskirts, Mission_KillRudolph, UsePhoneCab) /* 4 */
PORT_DECLL(outskirts, Mission_KillRudolph, SetNextMission2) /* 0 */
PORT_DECLL(outskirts, Mission_TransportKiller, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_TransportKiller, Init) /* 1 */
PORT_DECLL(outskirts, Mission_TransportKiller, InitTransport) /* 0 */
PORT_DECLL(outskirts, Mission_TransportKiller, isValidDamage) /* 1 */
PORT_DECLL(outskirts, Mission_TransportKiller, TryToDeath) /* 2 */
PORT_DECLL(outskirts, Mission_TransportKiller, TakeDataPack) /* 2 */
PORT_DECLL(outskirts, Mission_TransportKiller, TryToDamage) /* 3 */
PORT_DECLL(outskirts, Mission_TransportKiller, InitCall) /* 0 */
PORT_DECLL(outskirts, Mission_TransportKiller, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_Win2ndRace, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_Win2ndRace, Init) /* 1 */
PORT_DECLL(outskirts, Mission_Win2ndRace, InitRace) /* 0 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnCounterMeasure) /* ? */
PORT_DECLL(outskirts, Mission_Win2ndRace, GetRaceWaypointIndex) /* 0 */
PORT_DECLL(outskirts, Mission_Win2ndRace, RacerDamaged) /* 3 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnRaceStart) /* 2 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnCheckPoint) /* 2 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnNearCheckPoint) /* 2 */
PORT_DECLL(outskirts, Mission_Win2ndRace, NameOponGoItem) /* 1 */
PORT_DECLL(outskirts, Mission_Win2ndRace, CancelMission) /* 0 */
#define PORTS_levels_Outskirts \
	PORT_DEFLN(outskirts, Map, InitStatus, OsMap_InitStatusPy) \
	PORT_DEFLN(outskirts, Map, Init, OsMap_InitPy) \
	PORT_DEFLN(outskirts, MapRace, EasyRaceOurskirts1, MapRace_Easy1) \
	PORT_DEFLN(outskirts, MapRace, EasyRaceOurskirts2, MapRace_Easy2) \
	PORT_DEFLN(outskirts, MapRace, EasyRaceOurskirts3, MapRace_Easy3) \
	PORT_DEFLN(outskirts, MapRace, EasyRaceOurskirts4, MapRace_Easy4) \
	PORT_DEFLN(outskirts, MapRace, EasyRaceOurskirts5, MapRace_Easy5) \
	PORT_DEFLN(outskirts, MapRace, MediumRaceOurskirts1, MapRace_Medium1) \
	PORT_DEFLN(outskirts, MapRace, MediumRaceOurskirts2, MapRace_Medium2) \
	PORT_DEFLN(outskirts, MapRace, MediumRaceOurskirts3, MapRace_Medium3) \
	PORT_DEFLN(outskirts, MapRace, MediumRaceOurskirts4, MapRace_Medium4) \
	PORT_DEFLN(outskirts, MapRace, MediumRaceOurskirts5, MapRace_Medium5) \
	PORT_DEFLN(outskirts, MapRace, Mission_Win2ndRace_Race, MapRace_Win2nd) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, GetTraitoriusProfile, Traitorius_GetTraitoriusProfile) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, InitStatus, Traitorius_InitStatus) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, Init, Traitorius_InitPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, ReassignTarget, Traitorius_ReassignTargetPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, CreateTraitorius, Traitorius_CreateTraitoriusPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, OnTraitoriusDamage, Traitorius_OnTraitoriusDamagePy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, OnTraitoriusDeath, Traitorius_OnTraitoriusDeathPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, TraitoriusTakesLife, Traitorius_TraitoriusTakesLifePy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, TimeExpired, Traitorius_TimeExpiredPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, PreCallTraitorius, Traitorius_PreCallTraitoriusPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, CallTraitorius, Traitorius_CallTraitoriusPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, UsePhoneCab, Traitorius_UsePhoneCabPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, SetNextMission, Traitorius_SetNextMissionPy) \
	PORT_DEFLN(outskirts, Mission_AttackTraitorius, MissionScorer, Traitorius_MissionScorerPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, InitStatus, Demo_InitStatusPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, ShowSystemMessage, Demo_ShowSystemMessagePy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, Init, Demo_InitPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, ShowBoostMessage, Demo_ShowBoostMessagePy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, ClosePressHangar, Demo_ClosePressHangarPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, DelRacers, Demo_DelRacersPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, OnPlayerWinsRace, Demo_OnPlayerWinsRacePy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, CallCrazyGambler, Demo_CallCrazyGamblerPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, ShowRustyMapsSystemMessage, Demo_ShowRustyMapsSystemMessagePy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, UsePhoneCab, Demo_UsePhoneCabPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, EnableMetro, Demo_EnableMetroPy) \
	PORT_DEFLN(outskirts, Mission_Demo_HumphreyRace, OnMetroUsed, Demo_OnMetroUsedPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, GetMercenariesProfile, Debtors_GetMercenariesProfile) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, GetBankDirectorProfile, Debtors_GetBankDirectorProfile) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, GetBankerProfile, Debtors_GetBankerProfile) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, InitStatus, Debtors_InitStatusPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, CreateWaypoint, Debtors_CreateWaypointPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, CreateShip, Debtors_CreateShipPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnInitMission, Debtors_OnInitMissionPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, ShowScorer, Debtors_ShowScorerPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, EnableMissionWaypoint, Debtors_EnableMissionWaypointPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, SelectNewMissionTarget, Debtors_SelectNewMissionTargetPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnMissionTargetDeath, Debtors_OnMissionTargetDeathPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, BankMasterAttacksMissionTarget, Debtors_BankMasterAttacksMissionTargetPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, DropTreasure, Debtors_DropTreasurePy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, DelTreasure, Debtors_DelTreasurePy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, SendShipToGetTreasure, Debtors_SendShipToGetTreasurePy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, GetPossibleMercTargets, Debtors_GetPossibleMercTargetsPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, CreateMercs, Debtors_CreateMercsPy) \
	PORT_DEFLNK(outskirts, Mission_DestroyBadDebtors, CreateBankMaster, Debtors_CreateBankMasterPy) \
	PORT_DEFLNK(outskirts, Mission_DestroyBadDebtors, CreateBanker, Debtors_CreateBankerPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, RespawnBankMaster, Debtors_RespawnBankMasterPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnBankMasterDeath, Debtors_OnBankMasterDeathPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, CheckMercsFreezed, Debtors_CheckMercsFreezedPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnBankerDeath, Debtors_OnBankerDeathPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnMercDeath, Debtors_OnMercDeathPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnTreasureReached, Debtors_OnTreasureReachedPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnShipReachedTreasurePos, Debtors_OnShipReachedTreasurePosPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, OnCoinTaken, Debtors_OnCoinTakenPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, BankMasterSpeech, Debtors_BankMasterSpeechPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, ShowDeepThroatMessage, Debtors_ShowDeepThroatMessagePy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, SetNextMission, Debtors_SetNextMissionPy) \
	PORT_DEFLN(outskirts, Mission_DestroyBadDebtors, MissionScorer, Debtors_MissionScorerPy) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, GetHumphreyProfile, Humphrey_GetHumphreyProfile) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, InitStatus, Humphrey_InitStatus) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, CreateHumphrey, Humphrey_CreateHumphreyPy) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, HumphreyAttacks, Humphrey_HumphreyAttacksPy) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, LaunchTaunt, Humphrey_LaunchTauntPy) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, OnHumphreyDeath, Humphrey_OnHumphreyDeathPy) \
	PORT_DEFLN(outskirts, Mission_DestroyHumphrey, MissionScorer, Humphrey_MissionScorerPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, GetFuncEnemiesProfile, Hunt_GetFuncEnemiesProfile) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, GetPoliceFriendsProfile, Hunt_GetPoliceFriendsProfile) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, InitStatus, Hunt_InitStatus) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, Init, Hunt_InitPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, CreateWaypoint, Hunt_CreateWaypointPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, EnableMissionWaypoint, Hunt_EnableMissionWaypointPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, MissionScorer, Hunt_MissionScorerPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, ComeBack, Hunt_ComeBackPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, OnChangeVehicle, Hunt_OnChangeVehiclePy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, CreateNewPoliShip, Hunt_CreateNewPoliShipPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, CreateNewFuncShip, Hunt_CreateNewFuncShipPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, CreateFriendsShips, Hunt_CreateFriendsShipsPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, CreateEnemiesShips, Hunt_CreateEnemiesShipsPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, SetNextMission, Hunt_SetNextMissionPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, CGCalls, Hunt_CGCallsPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, OnFuncDeath, Hunt_OnFuncDeathPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, isPoliceShip, Hunt_isPoliceShipPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, OnMissionStart, Hunt_OnMissionStartPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, LaunchMission, Hunt_LaunchMissionPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, StartCameraTravelling, Hunt_StartCameraTravellingPy) \
	PORT_DEFLN(outskirts, Mission_FunctionaryHunting, AbortCameraTravelling, Hunt_AbortCameraTravellingPy) \
	PORT_DEFLN(outskirts, Mission_GoToOutskirts, InitStatus, GoTo_InitStatus) \
	PORT_DEFLN(outskirts, Mission_GoToOutskirts, Init, GoTo_InitPy) \
	PORT_DEFLN(outskirts, Mission_GoToOutskirts, LaunchCrazyGamblerComunication, GoTo_LaunchCrazyGamblerComunicationPy) \
	PORT_DEFLN(outskirts, Mission_GoToOutskirts, PlayCrazyGamblerComunication, GoTo_PlayCrazyGamblerComunicationPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, CreateLonelyMercProfiles, Merc_CreateLonelyMercProfilesPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, InitStatus, Merc_InitStatusPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, Init, Merc_InitPy) \
	PORT_DEFLNK(outskirts, Mission_IdentifyMerc, InitIdentifyMerc, Merc_InitIdentifyMercPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, CreateLonelyMercInScene, Merc_CreateLonelyMercInScenePy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, CheckTraitoriusDeath, Merc_CheckTraitoriusDeathPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, EndOfScene, Merc_EndOfScenePy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, DelLonelyMerc, Merc_DelLonelyMercPy) \
	PORT_DEFLNK(outskirts, Mission_IdentifyMerc, CreateLonelyMerc, Merc_CreateLonelyMercPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, InitMission, Merc_InitMissionPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, ScanShip, Merc_ScanShipPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, OnFinishScan, Merc_OnFinishScanPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, OnLonelyMercDeath, Merc_OnLonelyMercDeathPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, OnLonelyMercDamage, Merc_OnLonelyMercDamagePy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, PreRespawnLonelyMerc, Merc_PreRespawnLonelyMercPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, RespawnLonelyMerc, Merc_RespawnLonelyMercPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, SetPhoneCab, Merc_SetPhoneCabPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, UsePhoneCab, Merc_UsePhoneCabPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, SetNextMission, Merc_SetNextMissionPy) \
	PORT_DEFLN(outskirts, Mission_IdentifyMerc, MissionScorer, Merc_MissionScorerPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, InitStatus, Police_InitStatusPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, Init, Police_InitPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, SetPos, Police_SetPosPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, InitMission, Police_InitMissionPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, OnAlarm, Police_OnAlarmPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, OnPoliceDeath, Police_OnPoliceDeathPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, BertoAppears, Police_BertoAppearsPy) \
	PORT_DEFLN(outskirts, Mission_KillPolice, EnableMetro, Police_EnableMetroPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, GetRudolphProfile, Rudolph_GetRudolphProfile) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, RaceList, Rudolph_RaceList) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, InitStatus, Rudolph_InitStatusPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, Init, Rudolph_InitPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, GetDealType, Rudolph_GetDealType) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, InitRace, Rudolph_InitRacePy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, initCreateRudolph, Rudolph_initCreateRudolphPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, SetRudolphDef, Rudolph_SetRudolphDefPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, OnDamage, Rudolph_OnDamagePy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, EndOfRace, Rudolph_EndOfRacePy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, SetRudolphAsNone, Rudolph_SetRudolphAsNonePy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, RudolphKilled, Rudolph_RudolphKilledPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, CreateClue, Rudolph_CreateCluePy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, ActivateClue, Rudolph_ActivateCluePy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, OnClueTaken, Rudolph_OnClueTakenPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, InitTalkToPoliceBoss, Rudolph_InitTalkToPoliceBossPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, UsePhoneCab, Rudolph_UsePhoneCabPy) \
	PORT_DEFLN(outskirts, Mission_KillRudolph, SetNextMission2, Rudolph_SetNextMission2Py) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, InitStatus, Transport_InitStatus) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, Init, Transport_InitPy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, InitTransport, Transport_InitTransportPy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, isValidDamage, Transport_isValidDamagePy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, TryToDeath, Transport_TryToDeathPy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, TakeDataPack, Transport_TakeDataPackPy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, TryToDamage, Transport_TryToDamagePy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, InitCall, Transport_InitCallPy) \
	PORT_DEFLN(outskirts, Mission_TransportKiller, SetNextMission, Transport_SetNextMissionPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, InitStatus, Win2nd_InitStatus) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, Init, Win2nd_InitPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, InitRace, Win2nd_InitRacePy) \
	PORT_DEFLNK(outskirts, Mission_Win2ndRace, OnCounterMeasure, Win2nd_OnCounterMeasurePy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, GetRaceWaypointIndex, Win2nd_GetRaceWaypointIndexPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, RacerDamaged, Win2nd_RacerDamagedPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, OnRaceStart, Win2nd_OnRaceStartPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, OnCheckPoint, Win2nd_OnCheckPointPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, OnNearCheckPoint, Win2nd_OnNearCheckPointPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, NameOponGoItem, Win2nd_NameOponGoItemPy) \
	PORT_DEFLN(outskirts, Mission_Win2ndRace, CancelMission, Win2nd_CancelMissionPy)

/* levels/Police.c */
PORT_DECLL(police, Map, CreateFicusDeadClass) /* 0 */
PORT_DECLL(police, Map, CreateFicus) /* 0 */
PORT_DECLL(police, Map, InitStatus) /* 1 */
PORT_DECLL(police, Map, Init) /* 1 */
PORT_DECLL(police, MapSnd, SndElements) /* 6 */
PORT_DECLL(police, MapPlanes, Init) /* 1 */
PORT_DECLL(police, MapPlanes, TakenPhoto) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, InitStatus) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, Init) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, DisableIAFunctionary) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, OnTraitoriusRespawn) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, OnThugRespawn) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, SetThugsState) /* ? */
PORT_DECLL(police, Mission_BeatPickus, SetState) /* 3 */
PORT_DECLL(police, Mission_BeatPickus, TraitoriusSpeech) /* 4 */
PORT_DECLL(police, Mission_BeatPickus, HireThugs) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, ThugLeaderSpeech) /* 4 */
PORT_DECLL(police, Mission_BeatPickus, ThugNormalSpeech) /* 4 */
PORT_DECLL(police, Mission_BeatPickus, BeatPickus) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, OnAlarmActivated) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, CheckMaxDist) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, CheckThugsDist) /* 2 */
PORT_DECLL(police, Mission_BeatPickus, CreateLauncher) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, LaunchPickusFightScene) /* 2 */
PORT_DECLL(police, Mission_BeatPickus, CreatePickusBeatenClass) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, InitPickusBeaten) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, DelThug) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, AskPickus) /* ? */
PORT_DECLL(police, Mission_BeatPickus, OnTalkToPickus) /* 4 */
PORT_DECLL(police, Mission_BeatPickus, PreGetFuncShipPlans) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, GetFuncShipPlans) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, TakePlanPhoto) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, TalkToTraitoriusAgain) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, TraitoriusAgainSpeech) /* 4 */
PORT_DECLL(police, Mission_BeatPickus, LightingActivate) /* ? */
PORT_DECLL(police, Mission_FicusDead, InitFicusDead) /* 0 */
PORT_DECLL(police, Mission_FicusDead, Init) /* 1 */
PORT_DECLL(police, Mission_FicusDead, OnTalkToFicus) /* 4 */
PORT_DECLL(police, Mission_FicusDead, AfterDeepThroatMessageShown) /* 0 */
PORT_DECLL(police, Mission_FicusDead, DeepThroatSpeech) /* 4 */
PORT_DECLL(police, Mission_FicusDead, SetNextMission) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, InitStatus) /* 1 */
PORT_DECLL(police, Mission_GetAuthorization, CreateHuman) /* 1 */
PORT_DECLL(police, Mission_GetAuthorization, Init) /* 1 */
PORT_DECLL(police, Mission_GetAuthorization, AfterDeepThroatMessage) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, DeepThroatSpeech) /* 4 */
PORT_DECLL(police, Mission_GetAuthorization, PreGetAuthorization) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, GetAuthorization) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, ShowNearestConversor) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, ShowAfterConversionMessage) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, OnDataTaken) /* 1 */
PORT_DECLL(police, Mission_GetAuthorization, OnDataDropped) /* 2 */
PORT_DECLL(police, Mission_GetAuthorization, FreeHuman) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, TalkPepe) /* 4 */
PORT_DECLL(police, Mission_GetAuthorization, StartScene) /* 2 */
PORT_DECLL(police, Mission_GetAuthorization, OnTriggerActivated) /* 2 */
PORT_DECLL(police, Mission_GetAuthorization, GoToPress) /* 1 */
PORT_DECLL(police, Mission_GetAuthorization, GoToPress2) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, OnPossession) /* 2 */
PORT_DECLL(police, Mission_GetAuthorization, OnExitConversor) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, CheckMaxDist) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, CheckHumanDist) /* 0 */
PORT_DECLL(police, Mission_GetAuthorization, OnMetroUsed) /* 4 */
PORT_DECLL(police, Mission_InitEpilogue, InitStatus) /* 1 */
PORT_DECLL(police, Mission_InitEpilogue, CreateBettyJail) /* 1 */
PORT_DECLL(police, Mission_InitEpilogue, CreateBertoJail) /* 1 */
PORT_DECLL(police, Mission_InitEpilogue, Init) /* 1 */
PORT_DECLL(police, Mission_InitEpilogue, OnBettySpeech) /* 4 */
PORT_DECLL(police, Mission_InitEpilogue, OnBertoSpeech) /* 4 */
PORT_DECLL(police, Mission_InitEpilogue, SetNextMission) /* 0 */
PORT_DECLL(police, Mission_JailBreak, InitStatus) /* 1 */
PORT_DECLL(police, Mission_JailBreak, AfterBrowseNPC) /* 0 */
PORT_DECLL(police, Mission_JailBreak, CreateMCList) /* 1 */
PORT_DECLL(police, Mission_JailBreak, CreateGearStay) /* 1 */
PORT_DECLL(police, Mission_JailBreak, CreatePuff) /* 1 */
PORT_DECLL(police, Mission_JailBreak, InitPuffReborn) /* 1 */
PORT_DECLL(police, Mission_JailBreak, StaplerSpeech) /* 4 */
PORT_DECLL(police, Mission_JailBreak, CheckForJump) /* 1 */
PORT_DECLL(police, Mission_JailBreak, OnPossession) /* 2 */
PORT_DECLL(police, Mission_JailBreak, FirstTimeJail) /* 0 */
PORT_DECLL(police, Mission_JailBreak, Init) /* 1 */
PORT_DECLL(police, Mission_JailBreak, NextMission) /* 2 */
PORT_DECLL(police, Mission_JailBreak, LoadPolice) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, InitStatus) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, Init) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, CreatePoliceFicusPal) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, FicusPalMissionSpeech) /* 4 */
PORT_DECLL(police, Mission_PoliceBossCheats, CreateFakeChar) /* 3 */
PORT_DECLL(police, Mission_PoliceBossCheats, CheatingLoop) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, PlayAnm) /* 3 */
PORT_DECLL(police, Mission_PoliceBossCheats, WatchEvents) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, PlayDumb1) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, CheatPoliceBoss) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, PlayDumb2) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, BrupPoliceBoss) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, PlayPoliceBoss) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, PhotoOn) /* 3 */
PORT_DECLL(police, Mission_PoliceBossCheats, PhotoOff) /* 3 */
PORT_DECLL(police, Mission_PoliceBossCheats, OnPhoto) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, EndOfPhoto) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, InitAfterPhoto) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, InitAfterPhoto2) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, InitAfterPhoto3) /* 1 */
PORT_DECLL(police, Mission_PoliceBossCheats, InitAskCrazyGambler) /* 0 */
PORT_DECLL(police, Mission_PoliceBossCheats, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(police, Mission_PoliceBossCheats, SetNextMission) /* 0 */
#define PORTS_levels_Police \
	PORT_DEFLN(police, Map, CreateFicusDeadClass, PoMap_CreateFicusDeadClassA) \
	PORT_DEFLN(police, Map, CreateFicus, PoMap_CreateFicusA) \
	PORT_DEFLN(police, Map, InitStatus, PoMap_InitStatusA) \
	PORT_DEFLN(police, Map, Init, PoMap_InitA) \
	PORT_DEFLN(police, MapSnd, SndElements, PoliceMapSnd_SndElements) \
	PORT_DEFLN(police, MapPlanes, Init, MapPlanes_Init) \
	PORT_DEFLN(police, MapPlanes, TakenPhoto, MapPlanes_TakenPhoto) \
	PORT_DEFLN(police, Mission_BeatPickus, InitStatus, BeatPickus_InitStatusA) \
	PORT_DEFLN(police, Mission_BeatPickus, Init, BeatPickus_InitA) \
	PORT_DEFLN(police, Mission_BeatPickus, DisableIAFunctionary, BeatPickus_DisableIAFunctionaryA) \
	PORT_DEFLN(police, Mission_BeatPickus, OnTraitoriusRespawn, BeatPickus_OnTraitoriusRespawnA) \
	PORT_DEFLN(police, Mission_BeatPickus, OnThugRespawn, BeatPickus_OnThugRespawnA) \
	PORT_DEFLNK(police, Mission_BeatPickus, SetThugsState, BeatPickus_SetThugsState) \
	PORT_DEFLN(police, Mission_BeatPickus, SetState, BeatPickus_SetStateA) \
	PORT_DEFLN(police, Mission_BeatPickus, TraitoriusSpeech, BeatPickus_TraitoriusSpeechA) \
	PORT_DEFLN(police, Mission_BeatPickus, HireThugs, BeatPickus_HireThugsA) \
	PORT_DEFLN(police, Mission_BeatPickus, ThugLeaderSpeech, BeatPickus_ThugLeaderSpeechA) \
	PORT_DEFLN(police, Mission_BeatPickus, ThugNormalSpeech, BeatPickus_ThugNormalSpeechA) \
	PORT_DEFLN(police, Mission_BeatPickus, BeatPickus, BeatPickus_BeatPickusA) \
	PORT_DEFLN(police, Mission_BeatPickus, OnAlarmActivated, BeatPickus_OnAlarmActivatedA) \
	PORT_DEFLN(police, Mission_BeatPickus, CheckMaxDist, BeatPickus_CheckMaxDistA) \
	PORT_DEFLN(police, Mission_BeatPickus, CheckThugsDist, BeatPickus_CheckThugsDistA) \
	PORT_DEFLN(police, Mission_BeatPickus, CreateLauncher, BeatPickus_CreateLauncherA) \
	PORT_DEFLN(police, Mission_BeatPickus, LaunchPickusFightScene, BeatPickus_LaunchPickusFightSceneA) \
	PORT_DEFLN(police, Mission_BeatPickus, CreatePickusBeatenClass, BeatPickus_CreatePickusBeatenClassA) \
	PORT_DEFLN(police, Mission_BeatPickus, InitPickusBeaten, BeatPickus_InitPickusBeatenA) \
	PORT_DEFLN(police, Mission_BeatPickus, DelThug, BeatPickus_DelThugA) \
	PORT_DEFLNK(police, Mission_BeatPickus, AskPickus, BeatPickus_AskPickus) \
	PORT_DEFLN(police, Mission_BeatPickus, OnTalkToPickus, BeatPickus_OnTalkToPickusA) \
	PORT_DEFLN(police, Mission_BeatPickus, PreGetFuncShipPlans, BeatPickus_PreGetFuncShipPlansA) \
	PORT_DEFLN(police, Mission_BeatPickus, GetFuncShipPlans, BeatPickus_GetFuncShipPlansA) \
	PORT_DEFLN(police, Mission_BeatPickus, TakePlanPhoto, BeatPickus_TakePlanPhotoA) \
	PORT_DEFLN(police, Mission_BeatPickus, TalkToTraitoriusAgain, BeatPickus_TalkToTraitoriusAgainA) \
	PORT_DEFLN(police, Mission_BeatPickus, TraitoriusAgainSpeech, BeatPickus_TraitoriusAgainSpeechA) \
	PORT_DEFLNK(police, Mission_BeatPickus, LightingActivate, BeatPickus_LightingActivate) \
	PORT_DEFLN(police, Mission_FicusDead, InitFicusDead, FicusDead_InitFicusDeadA) \
	PORT_DEFLN(police, Mission_FicusDead, Init, FicusDead_InitA) \
	PORT_DEFLN(police, Mission_FicusDead, OnTalkToFicus, FicusDead_OnTalkToFicusA) \
	PORT_DEFLN(police, Mission_FicusDead, AfterDeepThroatMessageShown, FicusDead_AfterDeepThroatMessageShownA) \
	PORT_DEFLN(police, Mission_FicusDead, DeepThroatSpeech, FicusDead_DeepThroatSpeechA) \
	PORT_DEFLN(police, Mission_FicusDead, SetNextMission, FicusDead_SetNextMissionA) \
	PORT_DEFLN(police, Mission_GetAuthorization, InitStatus, GetAuthorization_InitStatusA) \
	PORT_DEFLN(police, Mission_GetAuthorization, CreateHuman, GetAuthorization_CreateHumanA) \
	PORT_DEFLN(police, Mission_GetAuthorization, Init, GetAuthorization_InitA) \
	PORT_DEFLN(police, Mission_GetAuthorization, AfterDeepThroatMessage, GetAuthorization_AfterDeepThroatMessageA) \
	PORT_DEFLN(police, Mission_GetAuthorization, DeepThroatSpeech, GetAuthorization_DeepThroatSpeechA) \
	PORT_DEFLN(police, Mission_GetAuthorization, PreGetAuthorization, GetAuthorization_PreGetAuthorizationA) \
	PORT_DEFLN(police, Mission_GetAuthorization, GetAuthorization, GetAuthorization_GetAuthorizationA) \
	PORT_DEFLN(police, Mission_GetAuthorization, ShowNearestConversor, GetAuthorization_ShowNearestConversorA) \
	PORT_DEFLN(police, Mission_GetAuthorization, ShowAfterConversionMessage, GetAuthorization_ShowAfterConversionMessageA) \
	PORT_DEFLN(police, Mission_GetAuthorization, OnDataTaken, GetAuthorization_OnDataTakenA) \
	PORT_DEFLN(police, Mission_GetAuthorization, OnDataDropped, GetAuthorization_OnDataDroppedA) \
	PORT_DEFLN(police, Mission_GetAuthorization, FreeHuman, GetAuthorization_FreeHumanA) \
	PORT_DEFLN(police, Mission_GetAuthorization, TalkPepe, GetAuthorization_TalkPepeA) \
	PORT_DEFLN(police, Mission_GetAuthorization, StartScene, GetAuthorization_StartSceneA) \
	PORT_DEFLN(police, Mission_GetAuthorization, OnTriggerActivated, GetAuthorization_OnTriggerActivatedA) \
	PORT_DEFLN(police, Mission_GetAuthorization, GoToPress, GetAuthorization_GoToPressA) \
	PORT_DEFLN(police, Mission_GetAuthorization, GoToPress2, GetAuthorization_GoToPress2A) \
	PORT_DEFLN(police, Mission_GetAuthorization, OnPossession, GetAuthorization_OnPossessionA) \
	PORT_DEFLN(police, Mission_GetAuthorization, OnExitConversor, GetAuthorization_OnExitConversorA) \
	PORT_DEFLN(police, Mission_GetAuthorization, CheckMaxDist, GetAuthorization_CheckMaxDistA) \
	PORT_DEFLN(police, Mission_GetAuthorization, CheckHumanDist, GetAuthorization_CheckHumanDistA) \
	PORT_DEFLN(police, Mission_GetAuthorization, OnMetroUsed, GetAuthorization_OnMetroUsedA) \
	PORT_DEFLN(police, Mission_InitEpilogue, InitStatus, InitEpilogue_InitStatusA) \
	PORT_DEFLN(police, Mission_InitEpilogue, CreateBettyJail, InitEpilogue_CreateBettyJailA) \
	PORT_DEFLN(police, Mission_InitEpilogue, CreateBertoJail, InitEpilogue_CreateBertoJailA) \
	PORT_DEFLN(police, Mission_InitEpilogue, Init, InitEpilogue_InitA) \
	PORT_DEFLN(police, Mission_InitEpilogue, OnBettySpeech, InitEpilogue_OnBettySpeechA) \
	PORT_DEFLN(police, Mission_InitEpilogue, OnBertoSpeech, InitEpilogue_OnBertoSpeechA) \
	PORT_DEFLN(police, Mission_InitEpilogue, SetNextMission, InitEpilogue_SetNextMissionA) \
	PORT_DEFLN(police, Mission_JailBreak, InitStatus, JailBreak_InitStatusA) \
	PORT_DEFLN(police, Mission_JailBreak, AfterBrowseNPC, JailBreak_AfterBrowseNPCA) \
	PORT_DEFLN(police, Mission_JailBreak, CreateMCList, JailBreak_CreateMCListA) \
	PORT_DEFLN(police, Mission_JailBreak, CreateGearStay, JailBreak_CreateGearStayA) \
	PORT_DEFLN(police, Mission_JailBreak, CreatePuff, JailBreak_CreatePuffA) \
	PORT_DEFLN(police, Mission_JailBreak, InitPuffReborn, JailBreak_InitPuffRebornA) \
	PORT_DEFLN(police, Mission_JailBreak, StaplerSpeech, JailBreak_StaplerSpeechA) \
	PORT_DEFLN(police, Mission_JailBreak, CheckForJump, JailBreak_CheckForJumpA) \
	PORT_DEFLN(police, Mission_JailBreak, OnPossession, JailBreak_OnPossessionA) \
	PORT_DEFLN(police, Mission_JailBreak, FirstTimeJail, JailBreak_FirstTimeJailA) \
	PORT_DEFLN(police, Mission_JailBreak, Init, JailBreak_InitA) \
	PORT_DEFLN(police, Mission_JailBreak, NextMission, JailBreak_NextMissionA) \
	PORT_DEFLN(police, Mission_JailBreak, LoadPolice, JailBreak_LoadPoliceA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, InitStatus, PoliceBossCheats_InitStatusA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, Init, PoliceBossCheats_InitA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, CreatePoliceFicusPal, PoliceBossCheats_CreatePoliceFicusPalA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, FicusPalMissionSpeech, PoliceBossCheats_FicusPalMissionSpeechA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, CreateFakeChar, PoliceBossCheats_CreateFakeCharA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, CheatingLoop, PoliceBossCheats_CheatingLoopA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, PlayAnm, PoliceBossCheats_PlayAnmA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, WatchEvents, PoliceBossCheats_WatchEventsA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, PlayDumb1, PoliceBossCheats_PlayDumb1A) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, CheatPoliceBoss, PoliceBossCheats_CheatPoliceBossA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, PlayDumb2, PoliceBossCheats_PlayDumb2A) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, BrupPoliceBoss, PoliceBossCheats_BrupPoliceBossA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, PlayPoliceBoss, PoliceBossCheats_PlayPoliceBossA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, PhotoOn, PoliceBossCheats_PhotoOnA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, PhotoOff, PoliceBossCheats_PhotoOffA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, OnPhoto, PoliceBossCheats_OnPhotoA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, EndOfPhoto, PoliceBossCheats_EndOfPhotoA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, InitAfterPhoto, PoliceBossCheats_InitAfterPhotoA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, InitAfterPhoto2, PoliceBossCheats_InitAfterPhoto2A) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, InitAfterPhoto3, PoliceBossCheats_InitAfterPhoto3A) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, InitAskCrazyGambler, PoliceBossCheats_InitAskCrazyGamblerA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, CrazyGamblerSpeech, PoliceBossCheats_CrazyGamblerSpeechA) \
	PORT_DEFLN(police, Mission_PoliceBossCheats, SetNextMission, PoliceBossCheats_SetNextMissionA)

/* levels/Press.c */
PORT_DECLL(press, Map, RemoteMsg) /* 1 */
PORT_DECLL(press, Map, ForbiddenMsg) /* 0 */
PORT_DECLL(press, Map, StartAlarmator) /* 0 */
PORT_DECLL(press, Map, ActivateAlarm) /* 2 */
PORT_DECLL(press, Map, CreateActivators) /* 0 */
PORT_DECLL(press, Map, CreateDeactivators) /* 0 */
PORT_DECLL(press, Map, DeactivateAlarm) /* 2 */
PORT_DECLL(press, Map, BrowseAlarmLaunchers) /* 0 */
PORT_DECLL(press, Map, InitStatus) /* 1 */
PORT_DECLL(press, Map, Init) /* 1 */
PORT_DECLL(press, Map, HideBureau) /* 0 */
PORT_DECLL(press, MapPlanes, Init) /* 1 */
PORT_DECLL(press, MapPlanes, TakenPhoto) /* 1 */
PORT_DECLL(press, MapDemoClose, Init) /* 1 */
PORT_DECLL(press, MapDemoClose, OnMetroUsed) /* 4 */
PORT_DECLL(press, MapSnd, LoadSounds) /* 0 */
PORT_DECLL(press, MapSnd, SetSounds) /* 0 */
PORT_DECLL(press, MapSnd, News1) /* 0 */
PORT_DECLL(press, Mission_FicusBanker, InitStatus) /* 1 */
PORT_DECLL(press, Mission_FicusBanker, Init) /* 1 */
PORT_DECLL(press, Mission_FicusBanker, CreatePoliceFicusInit) /* 1 */
PORT_DECLL(press, Mission_FicusBanker, FicusMissionSpeech) /* 4 */
PORT_DECLL(press, Mission_FicusBanker, CreateBankDirectorScrapusInit) /* 1 */
PORT_DECLL(press, Mission_FicusBanker, ScrapusMissionSpeech) /* 4 */
PORT_DECLL(press, Mission_FicusBanker, InitKillScrapus) /* 0 */
PORT_DECLL(press, Mission_FicusBanker, SpawnCheck) /* 2 */
PORT_DECLL(press, Mission_FicusBanker, TakeCheck) /* 2 */
PORT_DECLL(press, Mission_FicusBanker, SetNextMission) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, Init) /* 1 */
PORT_DECLL(press, Mission_GetCrazyData, GetCrazyData) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, OnAlarmActivated) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, OnSwitchPressed) /* 4 */
PORT_DECLL(press, Mission_GetCrazyData, StartTimer) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, TimeExpired) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, GoToPressRoom) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, OnFakeBureauTaken) /* 2 */
PORT_DECLL(press, Mission_GetCrazyData, FindHumphrey) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, HumphreySpeech) /* 4 */
PORT_DECLL(press, Mission_GetCrazyData, FindTrurl) /* 0 */
PORT_DECLL(press, Mission_GetCrazyData, TrurlSpeech) /* 4 */
PORT_DECLL(press, Mission_GetCrazyData, MissionScorer) /* 1 */
PORT_DECLL(press, Mission_GoToPress, Init) /* 1 */
PORT_DECLL(press, Mission_GoToPress, CrazySads) /* 1 */
PORT_DECLL(press, Mission_GoWithHuman, InitStatus) /* 1 */
PORT_DECLL(press, Mission_GoWithHuman, CreateHuman) /* 1 */
PORT_DECLL(press, Mission_GoWithHuman, Init) /* 1 */
PORT_DECLL(press, Mission_GoWithHuman, OnPossession) /* 2 */
PORT_DECLL(press, Mission_GoWithHuman, OnExitConversor) /* 0 */
PORT_DECLL(press, Mission_GoWithHuman, CheckMaxDist) /* 0 */
PORT_DECLL(press, Mission_GoWithHuman, CheckHumanDist) /* 0 */
PORT_DECLL(press, Mission_GoWithHuman, LoadNextMission) /* ? */
PORT_DECLL(press, Mission_GoWithHuman, LoadScene2) /* 0 */
PORT_DECLL(press, Mission_GoWithHuman, CreateFakeSpeech) /* 0 */
PORT_DECLL(press, Mission_GoWithHuman, NormCamera) /* 0 */
PORT_DECLL(press, Mission_GoWithHuman, SetTalk) /* 0 */
PORT_DECLL(press, Mission_HumphreyArchbishopKilled, SetNextMission) /* 1 */
PORT_DECLL(press, Mission_HumphreyArchbishopKilled, CGCalls) /* 0 */
PORT_DECLL(press, Mission_IntroHuman2, LaunchHumphreySpeech) /* 1 */
PORT_DECLL(press, Mission_IntroHuman2, LoadScene) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, InitStatus) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, AfterBrowseNPC) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, CreateHumphreyInit) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, CreateBertoInit) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, BertoSpeech) /* 4 */
PORT_DECLL(press, Mission_MeetBoss, BertoSpeech2) /* 4 */
PORT_DECLL(press, Mission_MeetBoss, DeactivateEmero) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, ActivateEmero) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, Init) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, PlayProlog0) /* ? */
PORT_DECLL(press, Mission_MeetBoss, PlayProlog1) /* ? */
PORT_DECLL(press, Mission_MeetBoss, PlayProlog2) /* ? */
PORT_DECLL(press, Mission_MeetBoss, CreateBattery) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, AssingOnTakeBattery) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, OnTakeBattery) /* 2 */
PORT_DECLL(press, Mission_MeetBoss, PreBertoPossession) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, BertoPossession) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, OnWarning) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, OnActionPressed) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, OnPossession) /* 2 */
PORT_DECLL(press, Mission_MeetBoss, ShowMessage) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, OnAlarmEnd) /* 1 */
PORT_DECLL(press, Mission_MeetBoss, UseConversor) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, OnConversorUsed) /* 4 */
PORT_DECLL(press, Mission_MeetBoss, CheckConversion) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, ChallengeHumphrey) /* 0 */
PORT_DECLL(press, Mission_MeetBoss, HumphreySpeech) /* 4 */
PORT_DECLL(press, Mission_MeetBoss, SetNextMission) /* 0 */
PORT_DECLL(press, Mission_PressMeeting, SetNextMission) /* 1 */
PORT_DECLL(press, Mission_TalkToBoss, InitStatus) /* 1 */
PORT_DECLL(press, Mission_TalkToBoss, AfterBrowseNPC) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, SetBossSpeech) /* 1 */
PORT_DECLL(press, Mission_TalkToBoss, CreateHuman) /* 1 */
PORT_DECLL(press, Mission_TalkToBoss, Init) /* 1 */
PORT_DECLL(press, Mission_TalkToBoss, HideHumanIntoBureau) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, CreateSceneLaunchers) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, LaunchScene) /* 2 */
PORT_DECLL(press, Mission_TalkToBoss, EndOfScene) /* ? */
PORT_DECLL(press, Mission_TalkToBoss, OnPossession) /* 2 */
PORT_DECLL(press, Mission_TalkToBoss, OnExitConversor) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, CheckMaxDist) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, CheckHumanDist) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, GoTalkToBoss) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, BossSpeech) /* 4 */
PORT_DECLL(press, Mission_TalkToBoss, SetNextMission) /* 1 */
PORT_DECLL(press, Mission_WhoIsDeepThroat, Init) /* 1 */
PORT_DECLL(press, Mission_WhoIsDeepThroat, UseTeleporter) /* 4 */
PORT_DECLL(press, Mission_WhoIsDeepThroat, LaunchScene) /* ? */
PORT_DECLL(press, Mission_WhoIsDeepThroat, PlayScene) /* 0 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, InitStatus) /* 1 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, Init) /* 1 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, GoForHuman) /* 0 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, OnBureauUsed) /* 4 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, PlayScene) /* 0 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, EndOfScene) /* 1 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, CreateFakeSpeech) /* 0 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, NormCamera) /* 0 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, SetTalk) /* 0 */
PORT_DECLL(press, Mission_WhoIsTheBaddie, LoadScene) /* 0 */
#define PORTS_levels_Press \
	PORT_DEFLN(press, Map, RemoteMsg, PressMap_RemoteMsgA) \
	PORT_DEFLN(press, Map, ForbiddenMsg, PressMap_ForbiddenMsgA) \
	PORT_DEFLN(press, Map, StartAlarmator, PressMap_StartAlarmatorA) \
	PORT_DEFLN(press, Map, ActivateAlarm, PressMap_ActivateAlarmA) \
	PORT_DEFLN(press, Map, CreateActivators, PressMap_CreateActivatorsA) \
	PORT_DEFLN(press, Map, CreateDeactivators, PressMap_CreateDeactivatorsA) \
	PORT_DEFLN(press, Map, DeactivateAlarm, PressMap_DeactivateAlarmA) \
	PORT_DEFLN(press, Map, BrowseAlarmLaunchers, PressMap_BrowseAlarmLaunchersA) \
	PORT_DEFLN(press, Map, InitStatus, PressMap_InitStatusA) \
	PORT_DEFLN(press, Map, Init, PressMap_InitA) \
	PORT_DEFLN(press, Map, HideBureau, PressMap_HideBureauA) \
	PORT_DEFLN(press, MapPlanes, Init, PressPlanes_Init) \
	PORT_DEFLN(press, MapPlanes, TakenPhoto, PressPlanes_TakenPhoto) \
	PORT_DEFLN(press, MapDemoClose, Init, DemoClose_InitA) \
	PORT_DEFLN(press, MapDemoClose, OnMetroUsed, DemoClose_OnMetroUsedA) \
	PORT_DEFLN(press, MapSnd, LoadSounds, PressSnd_LoadSoundsA) \
	PORT_DEFLN(press, MapSnd, SetSounds, PressSnd_SetSoundsA) \
	PORT_DEFLN(press, MapSnd, News1, PressSnd_News1A) \
	PORT_DEFLN(press, Mission_FicusBanker, InitStatus, FicusBanker_InitStatusA) \
	PORT_DEFLN(press, Mission_FicusBanker, Init, FicusBanker_InitA) \
	PORT_DEFLN(press, Mission_FicusBanker, CreatePoliceFicusInit, FicusBanker_CreatePoliceFicusInitA) \
	PORT_DEFLN(press, Mission_FicusBanker, FicusMissionSpeech, FicusBanker_FicusMissionSpeechA) \
	PORT_DEFLN(press, Mission_FicusBanker, CreateBankDirectorScrapusInit, FicusBanker_CreateBankDirectorScrapusInitA) \
	PORT_DEFLN(press, Mission_FicusBanker, ScrapusMissionSpeech, FicusBanker_ScrapusMissionSpeechA) \
	PORT_DEFLN(press, Mission_FicusBanker, InitKillScrapus, FicusBanker_InitKillScrapusA) \
	PORT_DEFLN(press, Mission_FicusBanker, SpawnCheck, FicusBanker_SpawnCheckA) \
	PORT_DEFLN(press, Mission_FicusBanker, TakeCheck, FicusBanker_TakeCheckA) \
	PORT_DEFLN(press, Mission_FicusBanker, SetNextMission, FicusBanker_SetNextMissionA) \
	PORT_DEFLN(press, Mission_GetCrazyData, Init, CrazyData_InitA) \
	PORT_DEFLN(press, Mission_GetCrazyData, GetCrazyData, CrazyData_GetCrazyDataA) \
	PORT_DEFLN(press, Mission_GetCrazyData, OnAlarmActivated, CrazyData_OnAlarmActivatedA) \
	PORT_DEFLN(press, Mission_GetCrazyData, OnSwitchPressed, CrazyData_OnSwitchPressedA) \
	PORT_DEFLN(press, Mission_GetCrazyData, StartTimer, CrazyData_StartTimerA) \
	PORT_DEFLN(press, Mission_GetCrazyData, TimeExpired, CrazyData_TimeExpiredA) \
	PORT_DEFLN(press, Mission_GetCrazyData, GoToPressRoom, CrazyData_GoToPressRoomA) \
	PORT_DEFLN(press, Mission_GetCrazyData, OnFakeBureauTaken, CrazyData_OnFakeBureauTakenA) \
	PORT_DEFLN(press, Mission_GetCrazyData, FindHumphrey, CrazyData_FindHumphreyA) \
	PORT_DEFLN(press, Mission_GetCrazyData, HumphreySpeech, CrazyData_HumphreySpeechA) \
	PORT_DEFLN(press, Mission_GetCrazyData, FindTrurl, CrazyData_FindTrurlA) \
	PORT_DEFLN(press, Mission_GetCrazyData, TrurlSpeech, CrazyData_TrurlSpeechA) \
	PORT_DEFLN(press, Mission_GetCrazyData, MissionScorer, CrazyData_MissionScorerA) \
	PORT_DEFLN(press, Mission_GoToPress, Init, GoToPress_InitA) \
	PORT_DEFLN(press, Mission_GoToPress, CrazySads, GoToPress_CrazySadsA) \
	PORT_DEFLN(press, Mission_GoWithHuman, InitStatus, GoWithHuman_InitStatusA) \
	PORT_DEFLN(press, Mission_GoWithHuman, CreateHuman, GoWithHuman_CreateHumanA) \
	PORT_DEFLN(press, Mission_GoWithHuman, Init, GoWithHuman_InitA) \
	PORT_DEFLN(press, Mission_GoWithHuman, OnPossession, GoWithHuman_OnPossessionA) \
	PORT_DEFLN(press, Mission_GoWithHuman, OnExitConversor, GoWithHuman_OnExitConversorA) \
	PORT_DEFLN(press, Mission_GoWithHuman, CheckMaxDist, GoWithHuman_CheckMaxDistA) \
	PORT_DEFLN(press, Mission_GoWithHuman, CheckHumanDist, GoWithHuman_CheckHumanDistA) \
	PORT_DEFLNK(press, Mission_GoWithHuman, LoadNextMission, GoWithHuman_LoadNextMission) \
	PORT_DEFLN(press, Mission_GoWithHuman, LoadScene2, GoWithHuman_LoadScene2A) \
	PORT_DEFLN(press, Mission_GoWithHuman, CreateFakeSpeech, GoWithHuman_CreateFakeSpeechA) \
	PORT_DEFLN(press, Mission_GoWithHuman, NormCamera, Pr_NormCameraA) \
	PORT_DEFLN(press, Mission_GoWithHuman, SetTalk, Pr_SetTalkA) \
	PORT_DEFLN(press, Mission_HumphreyArchbishopKilled, SetNextMission, ArchbishopKilled_SetNextMissionA) \
	PORT_DEFLN(press, Mission_HumphreyArchbishopKilled, CGCalls, ArchbishopKilled_CGCallsA) \
	PORT_DEFLN(press, Mission_IntroHuman2, LaunchHumphreySpeech, IntroHuman2_LaunchHumphreySpeechA) \
	PORT_DEFLN(press, Mission_IntroHuman2, LoadScene, IntroHuman2_LoadSceneA) \
	PORT_DEFLN(press, Mission_MeetBoss, InitStatus, MeetBoss_InitStatusA) \
	PORT_DEFLN(press, Mission_MeetBoss, AfterBrowseNPC, MeetBoss_AfterBrowseNPCA) \
	PORT_DEFLN(press, Mission_MeetBoss, CreateHumphreyInit, MeetBoss_CreateHumphreyInitA) \
	PORT_DEFLN(press, Mission_MeetBoss, CreateBertoInit, MeetBoss_CreateBertoInitA) \
	PORT_DEFLN(press, Mission_MeetBoss, BertoSpeech, MeetBoss_BertoSpeechA) \
	PORT_DEFLN(press, Mission_MeetBoss, BertoSpeech2, MeetBoss_BertoSpeech2A) \
	PORT_DEFLN(press, Mission_MeetBoss, DeactivateEmero, MeetBoss_DeactivateEmeroA) \
	PORT_DEFLN(press, Mission_MeetBoss, ActivateEmero, MeetBoss_ActivateEmeroA) \
	PORT_DEFLN(press, Mission_MeetBoss, Init, MeetBoss_InitA) \
	PORT_DEFLNK(press, Mission_MeetBoss, PlayProlog0, MeetBoss_PlayProlog0) \
	PORT_DEFLNK(press, Mission_MeetBoss, PlayProlog1, MeetBoss_PlayProlog1) \
	PORT_DEFLNK(press, Mission_MeetBoss, PlayProlog2, MeetBoss_PlayProlog2) \
	PORT_DEFLN(press, Mission_MeetBoss, CreateBattery, MeetBoss_CreateBatteryA) \
	PORT_DEFLN(press, Mission_MeetBoss, AssingOnTakeBattery, MeetBoss_AssingOnTakeBatteryA) \
	PORT_DEFLN(press, Mission_MeetBoss, OnTakeBattery, MeetBoss_OnTakeBatteryA) \
	PORT_DEFLN(press, Mission_MeetBoss, PreBertoPossession, MeetBoss_PreBertoPossessionA) \
	PORT_DEFLN(press, Mission_MeetBoss, BertoPossession, MeetBoss_BertoPossessionA) \
	PORT_DEFLN(press, Mission_MeetBoss, OnWarning, MeetBoss_OnWarningA) \
	PORT_DEFLN(press, Mission_MeetBoss, OnActionPressed, MeetBoss_OnActionPressedA) \
	PORT_DEFLN(press, Mission_MeetBoss, OnPossession, MeetBoss_OnPossessionA) \
	PORT_DEFLN(press, Mission_MeetBoss, ShowMessage, MeetBoss_ShowMessageA) \
	PORT_DEFLN(press, Mission_MeetBoss, OnAlarmEnd, MeetBoss_OnAlarmEndA) \
	PORT_DEFLN(press, Mission_MeetBoss, UseConversor, MeetBoss_UseConversorA) \
	PORT_DEFLN(press, Mission_MeetBoss, OnConversorUsed, MeetBoss_OnConversorUsedA) \
	PORT_DEFLN(press, Mission_MeetBoss, CheckConversion, MeetBoss_CheckConversionA) \
	PORT_DEFLN(press, Mission_MeetBoss, ChallengeHumphrey, MeetBoss_ChallengeHumphreyA) \
	PORT_DEFLN(press, Mission_MeetBoss, HumphreySpeech, MeetBoss_HumphreySpeechA) \
	PORT_DEFLN(press, Mission_MeetBoss, SetNextMission, MeetBoss_SetNextMissionA) \
	PORT_DEFLN(press, Mission_PressMeeting, SetNextMission, PressMeeting_SetNextMissionA) \
	PORT_DEFLN(press, Mission_TalkToBoss, InitStatus, TalkToBoss_InitStatusA) \
	PORT_DEFLN(press, Mission_TalkToBoss, AfterBrowseNPC, TalkToBoss_AfterBrowseNPCA) \
	PORT_DEFLN(press, Mission_TalkToBoss, SetBossSpeech, TalkToBoss_SetBossSpeechA) \
	PORT_DEFLN(press, Mission_TalkToBoss, CreateHuman, TalkToBoss_CreateHumanA) \
	PORT_DEFLN(press, Mission_TalkToBoss, Init, TalkToBoss_InitA) \
	PORT_DEFLN(press, Mission_TalkToBoss, HideHumanIntoBureau, TalkToBoss_HideHumanIntoBureauA) \
	PORT_DEFLN(press, Mission_TalkToBoss, CreateSceneLaunchers, TalkToBoss_CreateSceneLaunchersA) \
	PORT_DEFLN(press, Mission_TalkToBoss, LaunchScene, TalkToBoss_LaunchSceneA) \
	PORT_DEFLNK(press, Mission_TalkToBoss, EndOfScene, TalkToBoss_EndOfScene) \
	PORT_DEFLN(press, Mission_TalkToBoss, OnPossession, TalkToBoss_OnPossessionA) \
	PORT_DEFLN(press, Mission_TalkToBoss, OnExitConversor, TalkToBoss_OnExitConversorA) \
	PORT_DEFLN(press, Mission_TalkToBoss, CheckMaxDist, TalkToBoss_CheckMaxDistA) \
	PORT_DEFLN(press, Mission_TalkToBoss, CheckHumanDist, TalkToBoss_CheckHumanDistA) \
	PORT_DEFLN(press, Mission_TalkToBoss, GoTalkToBoss, TalkToBoss_GoTalkToBossA) \
	PORT_DEFLN(press, Mission_TalkToBoss, BossSpeech, TalkToBoss_BossSpeechA) \
	PORT_DEFLN(press, Mission_TalkToBoss, SetNextMission, TalkToBoss_SetNextMissionA) \
	PORT_DEFLN(press, Mission_WhoIsDeepThroat, Init, DeepThroat_InitA) \
	PORT_DEFLN(press, Mission_WhoIsDeepThroat, UseTeleporter, DeepThroat_UseTeleporterA) \
	PORT_DEFLNK(press, Mission_WhoIsDeepThroat, LaunchScene, DeepThroat_LaunchScene) \
	PORT_DEFLN(press, Mission_WhoIsDeepThroat, PlayScene, DeepThroat_PlaySceneA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, InitStatus, Baddie_InitStatusA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, Init, Baddie_InitA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, GoForHuman, Baddie_GoForHumanA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, OnBureauUsed, Baddie_OnBureauUsedA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, PlayScene, Baddie_PlaySceneA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, EndOfScene, Baddie_EndOfSceneA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, CreateFakeSpeech, Baddie_CreateFakeSpeechA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, NormCamera, Pr_NormCameraA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, SetTalk, Pr_SetTalkA) \
	PORT_DEFLN(press, Mission_WhoIsTheBaddie, LoadScene, Baddie_LoadSceneA)

/* levels/QF.c */
PORT_DECLL(qf, Mission_BankersCombat, CreatePlans) /* 0 */
PORT_DECLL(qf, Mission_BankersCombat, TakePlanPhoto) /* 1 */
PORT_DECLL(qf, Mission_BankersCombat, OnEndOfSuperCrazyDeal) /* 0 */
PORT_DECLL(qf, Mission_BankersCombat, BankersAttackPlayer) /* 0 */
PORT_DECLL(qf, Mission_BankersCombat, SetNextMission) /* 0 */
#define PORTS_levels_QF \
	PORT_DEFLN(qf, Mission_BankersCombat, CreatePlans, Bc_CreatePlansPy) \
	PORT_DEFLN(qf, Mission_BankersCombat, TakePlanPhoto, Bc_TakePlanPhotoPy) \
	PORT_DEFLN(qf, Mission_BankersCombat, OnEndOfSuperCrazyDeal, Bc_OnEndOfSuperCrazyDealPy) \
	PORT_DEFLN(qf, Mission_BankersCombat, BankersAttackPlayer, Bc_BankersAttackPlayerPy) \
	PORT_DEFLN(qf, Mission_BankersCombat, SetNextMission, Bc_SetNextMissionPy)

/* levels/Scrapyard.c */
PORT_DECLL(scrapyard, Map, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Map, Init) /* 1 */
PORT_DECLL(scrapyard, MapTriggers, GetMadHunterProfile) /* 0 */
PORT_DECLL(scrapyard, MapTriggers, GetMosquitoProfile) /* 0 */
PORT_DECLL(scrapyard, MapTriggers, Init) /* 1 */
PORT_DECLL(scrapyard, MapTriggers, CheckMosquitoCadenceShoot) /* 0 */
PORT_DECLL(scrapyard, MapTriggers, CreateMosquitosTriggers) /* 0 */
PORT_DECLL(scrapyard, MapTriggers, OnMosquitoTriggerTaken) /* 2 */
PORT_DECLL(scrapyard, MapTriggers, CreateMosquito) /* 1 */
PORT_DECLL(scrapyard, MapTriggers, CreateMadHuntersTriggers) /* 0 */
PORT_DECLL(scrapyard, MapTriggers, OnMadHunterTriggerTaken) /* 2 */
PORT_DECLL(scrapyard, MapTriggers, CreateMadHunter) /* 1 */
PORT_DECLL(scrapyard, Mission_Ambush, GetMadHuntersProfile) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_Ambush, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_Ambush, CallTraitorius) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, UsePhoneCab) /* 4 */
PORT_DECLL(scrapyard, Mission_Ambush, DeepThroatCom) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, IsValidPos) /* 2 */
PORT_DECLL(scrapyard, Mission_Ambush, CreateProbe) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, CreateMadHunter) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, CreateMadHunters) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, OnProbeDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_Ambush, PreCallDeepThroat) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, CallDeepThroat) /* 0 */
PORT_DECLL(scrapyard, Mission_Ambush, UsePhoneCab2) /* 4 */
PORT_DECLL(scrapyard, Mission_Ambush, SetNextMission) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, GetCrazyGamblerProfile) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, GetMosquitoProfile) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, LaunchTaunts) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, CreateMosquito) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, CreateMosquitos) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, CreateCrazyGambler) /* 0 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, OnCrazyGamblerDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_DefeatCrazyGambler2, MissionScorer) /* 1 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, SetLonelyMercProfile) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CreateLonelyMercShip) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, OnLonelyMercShipDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CheckLonelyMercShipExist) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, TestForLonelyMercShip) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CheckCreateBadGuy) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, OutOfLonelyMercShip) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, ActivateMissionObjects) /* 1 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CreateBadGuy) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, OnBadGuyDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CreatePlan) /* 2 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, TakePlanPhoto) /* 1 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CreateDtritusMatrix) /* 0 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, TakeMatrix) /* 2 */
PORT_DECLL(scrapyard, Mission_FindDtritusMatrix, CheckEndOfMission) /* 0 */
PORT_DECLL(scrapyard, Mission_MeetBerto, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_MeetBerto, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_MeetBerto, UsePhoneCab) /* 4 */
PORT_DECLL(scrapyard, Mission_MeetBerto, SetNextMission) /* 0 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, GetMosquitoProfile) /* 0 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, InitSwarm) /* 0 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, InitMosquito) /* 2 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, CreateMosquito) /* 2 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, MosquitoDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, TakeDataPack) /* 2 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, InitPoliChat) /* 0 */
PORT_DECLL(scrapyard, Mission_MosquitoClue, SetNextMission) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMerc, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMerc, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMerc, CreateMerc) /* ? */
PORT_DECLL(scrapyard, Mission_StupidMerc, OnStupidMercDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_StupidMerc, OnStupidMercDamage) /* 3 */
PORT_DECLL(scrapyard, Mission_StupidMerc, CheckAITakesLife) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMerc, CheckLife) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMerc, GotoNext) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMerc, CreateATPCPlans) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMerc, AddATPCPlans) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMerc, SetNextMission) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, GetMercsEnemiesProfile) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, InitFollow) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, CreateMerc) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, CreateMercs) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, SmartMerDeath) /* 2 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, GotoNext) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, CreateMercPlans) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, AddMercPlans) /* 1 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, StartEvasion) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, CheckEvasion) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, InitTalkBishop) /* 0 */
PORT_DECLL(scrapyard, Mission_StupidMercAgain, SetNextMission) /* 0 */
PORT_DECLL(scrapyard, Mission_TalkToDeepThroat, InitStatus) /* 1 */
PORT_DECLL(scrapyard, Mission_TalkToDeepThroat, Init) /* 1 */
PORT_DECLL(scrapyard, Mission_TalkToDeepThroat, UsePhoneCab) /* 4 */
PORT_DECLL(scrapyard, Mission_TalkToDeepThroat, SetNextMission) /* 0 */
#define PORTS_levels_Scrapyard \
	PORT_DEFLN(scrapyard, Map, InitStatus, Scrapyard_InitStatus) \
	PORT_DEFLN(scrapyard, Map, Init, Scrapyard_Init) \
	PORT_DEFLN(scrapyard, MapTriggers, GetMadHunterProfile, MT_GetMadHunterProfile) \
	PORT_DEFLN(scrapyard, MapTriggers, GetMosquitoProfile, MT_GetMosquitoProfile) \
	PORT_DEFLN(scrapyard, MapTriggers, Init, MT_Init) \
	PORT_DEFLN(scrapyard, MapTriggers, CheckMosquitoCadenceShoot, MT_CheckMosquitoCadenceShoot) \
	PORT_DEFLN(scrapyard, MapTriggers, CreateMosquitosTriggers, MT_CreateMosquitosTriggers) \
	PORT_DEFLN(scrapyard, MapTriggers, OnMosquitoTriggerTaken, MT_OnMosquitoTriggerTaken) \
	PORT_DEFLN(scrapyard, MapTriggers, CreateMosquito, MT_CreateMosquito) \
	PORT_DEFLN(scrapyard, MapTriggers, CreateMadHuntersTriggers, MT_CreateMadHuntersTriggers) \
	PORT_DEFLN(scrapyard, MapTriggers, OnMadHunterTriggerTaken, MT_OnMadHunterTriggerTaken) \
	PORT_DEFLN(scrapyard, MapTriggers, CreateMadHunter, MT_CreateMadHunter) \
	PORT_DEFLN(scrapyard, Mission_Ambush, GetMadHuntersProfile, Ambush_GetMadHuntersProfile) \
	PORT_DEFLN(scrapyard, Mission_Ambush, InitStatus, Ambush_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_Ambush, Init, Ambush_Init) \
	PORT_DEFLN(scrapyard, Mission_Ambush, CallTraitorius, Ambush_CallTraitorius) \
	PORT_DEFLN(scrapyard, Mission_Ambush, UsePhoneCab, Ambush_UsePhoneCab) \
	PORT_DEFLN(scrapyard, Mission_Ambush, DeepThroatCom, Ambush_DeepThroatCom) \
	PORT_DEFLN(scrapyard, Mission_Ambush, IsValidPos, Ambush_IsValidPos) \
	PORT_DEFLN(scrapyard, Mission_Ambush, CreateProbe, Ambush_CreateProbe) \
	PORT_DEFLN(scrapyard, Mission_Ambush, CreateMadHunter, Ambush_CreateMadHunter) \
	PORT_DEFLN(scrapyard, Mission_Ambush, CreateMadHunters, Ambush_CreateMadHunters) \
	PORT_DEFLN(scrapyard, Mission_Ambush, OnProbeDeath, Ambush_OnProbeDeath) \
	PORT_DEFLN(scrapyard, Mission_Ambush, PreCallDeepThroat, Ambush_PreCallDeepThroat) \
	PORT_DEFLN(scrapyard, Mission_Ambush, CallDeepThroat, Ambush_CallDeepThroat) \
	PORT_DEFLN(scrapyard, Mission_Ambush, UsePhoneCab2, Ambush_UsePhoneCab2) \
	PORT_DEFLN(scrapyard, Mission_Ambush, SetNextMission, Ambush_SetNextMission) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, GetCrazyGamblerProfile, Gambler_GetCrazyGamblerProfile) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, GetMosquitoProfile, Gambler_GetMosquitoProfile) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, InitStatus, Gambler_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, Init, Gambler_Init) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, LaunchTaunts, Gambler_LaunchTaunts) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, CreateMosquito, Gambler_CreateMosquito) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, CreateMosquitos, Gambler_CreateMosquitos) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, CreateCrazyGambler, Gambler_CreateCrazyGambler) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, OnCrazyGamblerDeath, Gambler_OnCrazyGamblerDeath) \
	PORT_DEFLN(scrapyard, Mission_DefeatCrazyGambler2, MissionScorer, Gambler_MissionScorer) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, SetLonelyMercProfile, Dtritus_SetLonelyMercProfile) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, InitStatus, Dtritus_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, Init, Dtritus_Init) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CreateLonelyMercShip, Dtritus_CreateLonelyMercShip) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, OnLonelyMercShipDeath, Dtritus_OnLonelyMercShipDeath) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CheckLonelyMercShipExist, Dtritus_CheckLonelyMercShipExist) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, TestForLonelyMercShip, Dtritus_TestForLonelyMercShip) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CheckCreateBadGuy, Dtritus_CheckCreateBadGuy) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, OutOfLonelyMercShip, Dtritus_OutOfLonelyMercShip) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, ActivateMissionObjects, Dtritus_ActivateMissionObjects) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CreateBadGuy, Dtritus_CreateBadGuy) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, OnBadGuyDeath, Dtritus_OnBadGuyDeath) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CreatePlan, Dtritus_CreatePlan) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, TakePlanPhoto, Dtritus_TakePlanPhoto) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CreateDtritusMatrix, Dtritus_CreateDtritusMatrix) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, TakeMatrix, Dtritus_TakeMatrix) \
	PORT_DEFLN(scrapyard, Mission_FindDtritusMatrix, CheckEndOfMission, Dtritus_CheckEndOfMission) \
	PORT_DEFLN(scrapyard, Mission_MeetBerto, InitStatus, Berto_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_MeetBerto, Init, Berto_Init) \
	PORT_DEFLN(scrapyard, Mission_MeetBerto, UsePhoneCab, Berto_UsePhoneCab) \
	PORT_DEFLN(scrapyard, Mission_MeetBerto, SetNextMission, Berto_SetNextMission) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, GetMosquitoProfile, Clue_GetMosquitoProfile) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, InitStatus, Clue_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, Init, Clue_Init) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, InitSwarm, Clue_InitSwarm) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, InitMosquito, Clue_InitMosquito) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, CreateMosquito, Clue_CreateMosquito) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, MosquitoDeath, Clue_MosquitoDeath) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, TakeDataPack, Clue_TakeDataPack) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, InitPoliChat, Clue_InitPoliChat) \
	PORT_DEFLN(scrapyard, Mission_MosquitoClue, SetNextMission, Clue_SetNextMission) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, InitStatus, Merc_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, Init, Merc_Init) \
	PORT_DEFLNK(scrapyard, Mission_StupidMerc, CreateMerc, Merc_CreateMerc) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, OnStupidMercDeath, Merc_OnStupidMercDeath) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, OnStupidMercDamage, Merc_OnStupidMercDamage) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, CheckAITakesLife, Merc_CheckAITakesLife) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, CheckLife, Merc_CheckLife) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, GotoNext, Merc_GotoNext) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, CreateATPCPlans, Merc_CreateATPCPlans) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, AddATPCPlans, Merc_AddATPCPlans) \
	PORT_DEFLN(scrapyard, Mission_StupidMerc, SetNextMission, Merc_SetNextMission) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, GetMercsEnemiesProfile, Again_GetMercsEnemiesProfile) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, InitStatus, Again_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, Init, Again_Init) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, InitFollow, Again_InitFollow) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, CreateMerc, Again_CreateMerc) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, CreateMercs, Again_CreateMercs) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, SmartMerDeath, Again_SmartMerDeath) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, GotoNext, Again_GotoNext) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, CreateMercPlans, Again_CreateMercPlans) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, AddMercPlans, Again_AddMercPlans) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, StartEvasion, Again_StartEvasion) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, CheckEvasion, Again_CheckEvasion) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, InitTalkBishop, Again_InitTalkBishop) \
	PORT_DEFLN(scrapyard, Mission_StupidMercAgain, SetNextMission, Again_SetNextMission) \
	PORT_DEFLN(scrapyard, Mission_TalkToDeepThroat, InitStatus, Throat_InitStatus) \
	PORT_DEFLN(scrapyard, Mission_TalkToDeepThroat, Init, Throat_Init) \
	PORT_DEFLN(scrapyard, Mission_TalkToDeepThroat, UsePhoneCab, Throat_UsePhoneCab) \
	PORT_DEFLN(scrapyard, Mission_TalkToDeepThroat, SetNextMission, Throat_SetNextMission)

/* levels/Small.c */
PORT_DECLL(tuczone, Map, Init) /* 1 */
PORT_DECLL(nano, Map, InitStatus) /* 1 */
PORT_DECLL(nano, Map, Init) /* 1 */
PORT_DECLL(shipedit, Map, Init) /* 1 */
PORT_DECLL(shipedit, MapSnd, Init) /* 1 */
#define PORTS_levels_Small \
	PORT_DEFLN(tuczone, Map, Init, Tuc_InitPy) \
	PORT_DEFLN(nano, Map, InitStatus, Nano_InitStatusPy) \
	PORT_DEFLN(nano, Map, Init, Nano_InitPy) \
	PORT_DEFLN(shipedit, Map, Init, Se_InitPy) \
	PORT_DEFLN(shipedit, MapSnd, Init, Se_SndInitPy)

/* levels/Temple.c */
PORT_DECLL(temple, Map, InitStatus) /* 1 */
PORT_DECLL(temple, Map, Init) /* 1 */
PORT_DECLL(temple, Map, EndOfAppears) /* 1 */
PORT_DECLL(temple, MapExtraLifes, TakeExtraLife) /* 2 */
PORT_DECLL(temple, MapExtraLifes, SetRespawnTime) /* 1 */
PORT_DECLL(temple, MapExtraLifes, MoveLifes) /* 1 */
PORT_DECLL(temple, MapExtraLifes, SwitchAlarm) /* 1 */
PORT_DECLL(temple, MapExtraLifes, Init) /* 1 */
PORT_DECLL(temple, MapExtraLifes, GetMoney) /* 1 */
PORT_DECLL(temple, Mission_ArchbishopDead, Init) /* 1 */
PORT_DECLL(temple, Mission_ArchbishopDead, ForbiddenMsg) /* 0 */
PORT_DECLL(temple, Mission_ArchbishopDead, CreateViscProofs) /* 0 */
PORT_DECLL(temple, Mission_ArchbishopDead, ViewProof) /* 4 */
PORT_DECLL(temple, Mission_ArchbishopDead, TakeProofPhoto) /* 1 */
PORT_DECLL(temple, Mission_ArchbishopDead, AfterDeepThroatMessageShown) /* 0 */
PORT_DECLL(temple, Mission_AskGus, InitStatus) /* 1 */
PORT_DECLL(temple, Mission_AskGus, AfterBrowseNPC) /* 0 */
PORT_DECLL(temple, Mission_AskGus, CreateGusInit) /* 1 */
PORT_DECLL(temple, Mission_AskGus, Init) /* 1 */
PORT_DECLL(temple, Mission_AskGus, DeepThroatAppears) /* 1 */
PORT_DECLL(temple, Mission_AskGus, PreCallDeepThroat) /* 0 */
PORT_DECLL(temple, Mission_AskGus, CallDeepThroat) /* 0 */
PORT_DECLL(temple, Mission_AskGus, DeepThroatSpeech) /* 4 */
PORT_DECLL(temple, Mission_MeetArchbishop, UseDoor) /* 4 */
PORT_DECLL(temple, Mission_MeetArchbishop, TestEndOfElevator) /* 0 */
PORT_DECLL(temple, Mission_MeetArchbishop, CreateFakeSpeech) /* 0 */
PORT_DECLL(temple, Mission_MeetArchbishop, Init) /* 1 */
PORT_DECLL(temple, Mission_TalkToDeepThroat, Init) /* 1 */
PORT_DECLL(temple, Mission_TalkToDeepThroat, DeepThroatSpeech) /* 4 */
#define PORTS_levels_Temple \
	PORT_DEFLN(temple, Map, InitStatus, TpMap_InitStatusPy) \
	PORT_DEFLN(temple, Map, Init, TpMap_InitPy) \
	PORT_DEFLN(temple, Map, EndOfAppears, TpMap_EndOfAppearsPy) \
	PORT_DEFLN(temple, MapExtraLifes, TakeExtraLife, Xl_TakeExtraLifePy) \
	PORT_DEFLN(temple, MapExtraLifes, SetRespawnTime, Xl_SetRespawnTimePy) \
	PORT_DEFLN(temple, MapExtraLifes, MoveLifes, Xl_MoveLifesPy) \
	PORT_DEFLN(temple, MapExtraLifes, SwitchAlarm, Xl_SwitchAlarmPy) \
	PORT_DEFLN(temple, MapExtraLifes, Init, Xl_InitPy) \
	PORT_DEFLN(temple, MapExtraLifes, GetMoney, Xl_GetMoneyPy) \
	PORT_DEFLN(temple, Mission_ArchbishopDead, Init, Ad_InitPy) \
	PORT_DEFLN(temple, Mission_ArchbishopDead, ForbiddenMsg, Ad_ForbiddenMsgPy) \
	PORT_DEFLN(temple, Mission_ArchbishopDead, CreateViscProofs, Ad_CreateViscProofsPy) \
	PORT_DEFLN(temple, Mission_ArchbishopDead, ViewProof, Ad_ViewProofPy) \
	PORT_DEFLN(temple, Mission_ArchbishopDead, TakeProofPhoto, Ad_TakeProofPhotoPy) \
	PORT_DEFLN(temple, Mission_ArchbishopDead, AfterDeepThroatMessageShown, Ad_AfterDeepThroatMessageShownPy) \
	PORT_DEFLN(temple, Mission_AskGus, InitStatus, Ag_InitStatusPy) \
	PORT_DEFLN(temple, Mission_AskGus, AfterBrowseNPC, Ag_AfterBrowseNPCPy) \
	PORT_DEFLN(temple, Mission_AskGus, CreateGusInit, Ag_CreateGusInitPy) \
	PORT_DEFLN(temple, Mission_AskGus, Init, Ag_InitPy) \
	PORT_DEFLN(temple, Mission_AskGus, DeepThroatAppears, Ag_DeepThroatAppearsPy) \
	PORT_DEFLN(temple, Mission_AskGus, PreCallDeepThroat, Ag_PreCallDeepThroatPy) \
	PORT_DEFLN(temple, Mission_AskGus, CallDeepThroat, Ag_CallDeepThroatPy) \
	PORT_DEFLN(temple, Mission_AskGus, DeepThroatSpeech, Ag_DeepThroatSpeechPy) \
	PORT_DEFLN(temple, Mission_MeetArchbishop, UseDoor, Ma_UseDoorPy) \
	PORT_DEFLN(temple, Mission_MeetArchbishop, TestEndOfElevator, Ma_TestEndOfElevatorPy) \
	PORT_DEFLN(temple, Mission_MeetArchbishop, CreateFakeSpeech, Ma_CreateFakeSpeechPy) \
	PORT_DEFLN(temple, Mission_MeetArchbishop, Init, Ma_InitPy) \
	PORT_DEFLN(temple, Mission_TalkToDeepThroat, Init, Td_InitPy) \
	PORT_DEFLN(temple, Mission_TalkToDeepThroat, DeepThroatSpeech, Td_DeepThroatSpeechPy)

/* levels/Townhall.c */
PORT_DECLL(townhall, Map, InitStatus) /* 1 */
PORT_DECLL(townhall, Map, CreateGearMayor) /* 1 */
PORT_DECLL(townhall, Map, Init) /* 1 */
PORT_DECLL(townhall, Map, OrbitLockMsg) /* 4 */
PORT_DECLL(townhall, MapSnd, SndElements) /* 6 */
PORT_DECLL(townhall, Mission_ApologizeMayor, InitStatus) /* 1 */
PORT_DECLL(townhall, Mission_ApologizeMayor, AfterBrowseNPC) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, CreateMayorInit) /* 1 */
PORT_DECLL(townhall, Mission_ApologizeMayor, Init) /* 1 */
PORT_DECLL(townhall, Mission_ApologizeMayor, ForbiddenMsg) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, MayorSpeech) /* 4 */
PORT_DECLL(townhall, Mission_ApologizeMayor, SetTransportMission) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, ChooseStapler) /* ? */
PORT_DECLL(townhall, Mission_ApologizeMayor, InitStaplerMission) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, StaplerSpeech) /* 4 */
PORT_DECLL(townhall, Mission_ApologizeMayor, StaplerDead) /* 2 */
PORT_DECLL(townhall, Mission_ApologizeMayor, MayorMessage) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, MissionScorer) /* 1 */
PORT_DECLL(townhall, Mission_CatchTrurl, InitStatus) /* 1 */
PORT_DECLL(townhall, Mission_CatchTrurl, Init) /* 1 */
PORT_DECLL(townhall, Mission_CatchTrurl, CreateTrurl) /* 1 */
PORT_DECLL(townhall, Mission_CatchTrurl, OnCatchTrurl) /* 4 */
PORT_DECLL(townhall, Mission_CatchTrurl, OnCatchTrurlAgain) /* 4 */
PORT_DECLL(townhall, Mission_CatchTrurl, FunctionaryMissionSpeech) /* 4 */
PORT_DECLL(townhall, Mission_CatchTrurl, SetCallbackFuncs) /* 1 */
PORT_DECLL(townhall, Mission_CatchTrurl, SpawnMap) /* 2 */
PORT_DECLL(townhall, Mission_CatchTrurl, CreateMap) /* 2 */
PORT_DECLL(townhall, Mission_CatchTrurl, TakeMapPhoto) /* 1 */
PORT_DECLL(townhall, Mission_CatchTrurl, KillFunctionaries) /* 0 */
PORT_DECLL(townhall, Mission_CatchTrurl, CatchTrurlAgain) /* 0 */
PORT_DECLL(townhall, Mission_CatchTrurl, SetNextMission) /* 0 */
PORT_DECLL(townhall, Mission_CatchTrurl, CGCalls) /* 0 */
PORT_DECLL(townhall, Mission_SheriffMayor, InitStatus) /* 1 */
PORT_DECLL(townhall, Mission_SheriffMayor, CreateMayor) /* 1 */
PORT_DECLL(townhall, Mission_SheriffMayor, CreatePoliceBoss) /* 1 */
PORT_DECLL(townhall, Mission_SheriffMayor, ReadyToTalk) /* 1 */
PORT_DECLL(townhall, Mission_SheriffMayor, Blablibla) /* ? */
PORT_DECLL(townhall, Mission_SheriffMayor, PoliceBossMissionSpeech) /* 4 */
PORT_DECLL(townhall, Mission_SheriffMayor, OnPay) /* 1 */
PORT_DECLL(townhall, Mission_SheriffMayor, SetNextMission) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, InitStatus) /* 1 */
PORT_DECLL(townhall, Mission_TalkToMayor, AfterBrowseNPC) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, CreateMayorInit) /* 1 */
PORT_DECLL(townhall, Mission_TalkToMayor, CreateBishopPorkins) /* 1 */
PORT_DECLL(townhall, Mission_TalkToMayor, CreateFunctionaryTamayo) /* 1 */
PORT_DECLL(townhall, Mission_TalkToMayor, Init) /* 1 */
PORT_DECLL(townhall, Mission_TalkToMayor, MayorSpeech) /* 4 */
PORT_DECLL(townhall, Mission_TalkToMayor, BishopSpeech) /* 4 */
PORT_DECLL(townhall, Mission_TalkToMayor, FunctionarySpeech) /* 4 */
PORT_DECLL(townhall, Mission_TalkToMayor, StealFunctionary) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, CheckTamayoMoney) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, RestoreTamayoState) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, OnBankerSteal) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, EndOfStealFunctionary) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, SetNextMission) /* 0 */
PORT_DECLL(townhall, Mission_TalkToMayor, MissionScorer) /* 1 */
#define PORTS_levels_Townhall \
	PORT_DEFLN(townhall, Map, InitStatus, TownhallMap_InitStatus) \
	PORT_DEFLN(townhall, Map, CreateGearMayor, TownhallMap_CreateGearMayor) \
	PORT_DEFLN(townhall, Map, Init, TownhallMap_Init) \
	PORT_DEFLN(townhall, Map, OrbitLockMsg, TownhallMap_OrbitLockMsg) \
	PORT_DEFLN(townhall, MapSnd, SndElements, TownhallMapSnd_SndElements) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, InitStatus, ApologizeMayor_InitStatus) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, AfterBrowseNPC, ApologizeMayor_AfterBrowseNPC) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, CreateMayorInit, ApologizeMayor_CreateMayorInit) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, Init, ApologizeMayor_Init) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, ForbiddenMsg, ApologizeMayor_ForbiddenMsg) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, MayorSpeech, ApologizeMayor_MayorSpeech) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, SetTransportMission, ApologizeMayor_SetTransportMission) \
	PORT_DEFLNK(townhall, Mission_ApologizeMayor, ChooseStapler, ApologizeMayor_ChooseStapler) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, InitStaplerMission, ApologizeMayor_InitStaplerMission) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, StaplerSpeech, ApologizeMayor_StaplerSpeech) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, StaplerDead, ApologizeMayor_StaplerDead) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, MayorMessage, ApologizeMayor_MayorMessage) \
	PORT_DEFLN(townhall, Mission_ApologizeMayor, MissionScorer, ApologizeMayor_MissionScorer) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, InitStatus, CatchTrurl_InitStatus) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, Init, CatchTrurl_Init) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, CreateTrurl, CatchTrurl_CreateTrurl) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, OnCatchTrurl, CatchTrurl_OnCatchTrurl) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, OnCatchTrurlAgain, CatchTrurl_OnCatchTrurlAgain) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, FunctionaryMissionSpeech, CatchTrurl_FunctionaryMissionSpeech) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, SetCallbackFuncs, CatchTrurl_SetCallbackFuncs) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, SpawnMap, CatchTrurl_SpawnMap) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, CreateMap, CatchTrurl_CreateMap) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, TakeMapPhoto, CatchTrurl_TakeMapPhoto) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, KillFunctionaries, CatchTrurl_KillFunctionaries) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, CatchTrurlAgain, CatchTrurl_CatchTrurlAgain) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, SetNextMission, CatchTrurl_SetNextMission) \
	PORT_DEFLN(townhall, Mission_CatchTrurl, CGCalls, CatchTrurl_CGCalls) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, InitStatus, SheriffMayor_InitStatus) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, CreateMayor, SheriffMayor_CreateMayor) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, CreatePoliceBoss, SheriffMayor_CreatePoliceBoss) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, ReadyToTalk, SheriffMayor_ReadyToTalk) \
	PORT_DEFLNK(townhall, Mission_SheriffMayor, Blablibla, SheriffMayor_Blablibla) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, PoliceBossMissionSpeech, SheriffMayor_PoliceBossMissionSpeech) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, OnPay, SheriffMayor_OnPay) \
	PORT_DEFLN(townhall, Mission_SheriffMayor, SetNextMission, SheriffMayor_SetNextMission) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, InitStatus, TalkToMayor_InitStatus) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, AfterBrowseNPC, TalkToMayor_AfterBrowseNPC) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, CreateMayorInit, TalkToMayor_CreateMayorInit) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, CreateBishopPorkins, TalkToMayor_CreateBishopPorkins) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, CreateFunctionaryTamayo, TalkToMayor_CreateFunctionaryTamayo) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, Init, TalkToMayor_Init) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, MayorSpeech, TalkToMayor_MayorSpeech) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, BishopSpeech, TalkToMayor_BishopSpeech) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, FunctionarySpeech, TalkToMayor_FunctionarySpeech) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, StealFunctionary, TalkToMayor_StealFunctionary) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, CheckTamayoMoney, TalkToMayor_CheckTamayoMoney) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, RestoreTamayoState, TalkToMayor_RestoreTamayoState) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, OnBankerSteal, TalkToMayor_OnBankerSteal) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, EndOfStealFunctionary, TalkToMayor_EndOfStealFunctionary) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, SetNextMission, TalkToMayor_SetNextMission) \
	PORT_DEFLN(townhall, Mission_TalkToMayor, MissionScorer, TalkToMayor_MissionScorer)

/* the number of refs: the size of the registry (g_PortTable, src/game/Script/PortTable.c) */
#define PORT_COUNT 3229

#endif
