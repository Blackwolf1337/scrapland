/*
 * The refs of the port functions that have a native entry (PORT_CALL and PORT_OWN, include/scrap/scriptnative.h),
 * one per table function: made by tools/script/ports.py from the PyMethodDef tables of src/game/Script, their
 * registrations in Script.c and Levels.c and the entries. Do not edit: run the tool after a table or an entry
 * changes, and to resolve a merge. The number after each ref is the count of arguments its entry takes.
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
	PORT_DEF(Alarm, SetAlarm, Alarm_SetAlarm) \
	PORT_DEF(Alarm, ActivateAlarm, Alarm_Nothing) \
	PORT_DEF(Alarm, DeactivateAlarm, Alarm_DeactivateAlarm) \
	PORT_DEF(Alarm, Init, Alarm_Nothing) \
	PORT_DEF(NewsPanel, GetNews, NewsPanel_GetNews) \
	PORT_DEF(NewsPanel, FillNews, NewsPanel_FillNews) \
	PORT_DEF(NewsPanel, Init, NewsPanel_Init) \
	PORT_DEF(NewsPanel, UseNewsPanel, NewsPanel_UseNewsPanel) \
	PORT_DEF(NewsPanel, EndOfNews, NewsPanel_EndOfNews) \
	PORT_DEF(NewsPanel, SwitchAlarm, NewsPanel_SwitchAlarm)

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
	PORT_DEF(Challenge, ComputeReward, Challenge_ComputeReward) \
	PORT_DEF(Challenge, Init, Challenge_Init) \
	PORT_DEF(Challenge, SetChallengerSkills, Challenge_SetChallengerSkills) \
	PORT_DEF(Challenge, OnChallengeOpt, Challenge_OnChallengeOpt) \
	PORT_DEF(Challenge, SetRevengeParams, Challenge_SetRevengeParams) \
	PORT_DEF(Challenge, SetRevenge, Challenge_SetRevenge) \
	PORT_DEF(Challenge, LaunchRevenge, Challenge_LaunchRevenge) \
	PORT_DEF(Challenge, LaunchChallenge, Challenge_LaunchChallenge) \
	PORT_DEF(Challenge, CleanChallenge, Challenge_CleanChallenge) \
	PORT_DEF(Challenge, CleanRevenge, Challenge_CleanRevenge) \
	PORT_DEF(Challenge, CancelChallenge, Challenge_CancelChallenge) \
	PORT_DEF(Challenge, SetChallengeOptions, Challenge_SetChallengeOptions) \
	PORT_DEF(Challenge, AddChallenge, Challenge_AddChallenge)

/* CharAct.c */
PORT_DECL(CharAct, ActionPhoto) /* 1 */
PORT_DECL(CharAct, Photograph) /* 1 */
PORT_DECL(CharAct, OnDeath) /* 2 */
PORT_DECL(CharAct, NoFXDeath) /* ? */
PORT_DECL(CharAct, PlayerDeathCall) /* 1 */
PORT_DECL(CharAct, NPCDeathCall) /* 1 */
PORT_DECL(CharAct, OnCreate) /* 1 */
#define PORTS_CharAct \
	PORT_DEF(CharAct, ActionPhoto, CharAct_ActionPhoto) \
	PORT_DEF(CharAct, Photograph, CharAct_Photograph) \
	PORT_DEF(CharAct, OnDeath, CharAct_OnDeath) \
	PORT_DEF(CharAct, NoFXDeath, CharAct_NoFXDeath) \
	PORT_DEF(CharAct, PlayerDeathCall, CharAct_PlayerDeathCall) \
	PORT_DEF(CharAct, NPCDeathCall, CharAct_NPCDeathCall) \
	PORT_DEF(CharAct, OnCreate, CharAct_OnCreate)

/* CharConversor.c */
PORT_DECL(CharConversor, GetNearestConversionPoint) /* 1 */
PORT_DECL(CharConversor, CreateConversorClass) /* 0 */
PORT_DECL(CharConversor, Init) /* 0 */
PORT_DECL(CharConversor, ConvertChar) /* 4 */
PORT_DECL(CharConversor, CPPos) /* 1 */
PORT_DECL(CharConversor, CPAng) /* 1 */
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
	PORT_DEF(CharConversor, GetNearestConversionPoint, CharConversor_GetNearestConversionPoint) \
	PORT_DEF(CharConversor, CreateConversorClass, CharConversor_CreateConversorClass) \
	PORT_DEF(CharConversor, Init, CharConversor_Init) \
	PORT_DEF(CharConversor, ConvertChar, CharConversor_ConvertChar) \
	PORT_DEF(CharConversor, CPPos, CharConversor_CPPos) \
	PORT_DEF(CharConversor, CPAng, CharConversor_CPAng) \
	PORT_DEF(CharConversor, AddInterfaceContent, CharConversor_AddInterfaceContent) \
	PORT_DEF(CharConversor, SpawnChar, CharConversor_SpawnChar) \
	PORT_DEF(CharConversor, MarkSpawnWaypoint, CharConversor_MarkSpawnWaypoint) \
	PORT_DEF(CharConversor, EnterCharConversor, CharConversor_EnterCharConversor) \
	PORT_DEF(CharConversor, InitOfScene, CharConversor_InitOfScene) \
	PORT_DEF(CharConversor, EndOfScene, CharConversor_EndOfScene) \
	PORT_DEF(CharConversor, InitInterfaceEvent, CharConversor_InitInterfaceEvent) \
	PORT_DEF(CharConversor, ExitCharConversor, CharConversor_ExitCharConversor) \
	PORT_DEF(CharConversor, ShowCharDesc, CharConversor_ShowCharDesc) \
	PORT_DEF(CharConversor, HideCharDesc, CharConversor_HideCharDesc) \
	PORT_DEF(CharConversor, Convert, CharConversor_Convert) \
	PORT_DEF(CharConversor, ConvertExecute, CharConversor_ConvertExecute) \
	PORT_DEF(CharConversor, Possession, CharConversor_Possession) \
	PORT_DEF(CharConversor, NewPlayer, CharConversor_NewPlayer)

/* CharFuncs.c */
PORT_DECL(Maintenance, MaintenanceTalk) /* 4 */
PORT_DECL(SputnikJunkyard, SputnikJunkyardTalk) /* 4 */
PORT_DECL(Bishop, StartSellingLife) /* 1 */
PORT_DECL(Bishop, SellLife) /* 3 */
PORT_DECL(Messenger, ObjInitializator) /* 2 */
#define PORTS_CharFuncs \
	PORT_DEF(Maintenance, MaintenanceTalk, Char_CantTalk) \
	PORT_DEF(SputnikJunkyard, SputnikJunkyardTalk, Char_CantTalk) \
	PORT_DEF(Bishop, StartSellingLife, Bishop_StartSellingLife) \
	PORT_DEF(Bishop, SellLife, Bishop_SellLife) \
	PORT_DEF(Messenger, ObjInitializator, Messenger_ObjInitializator)

/* Chars.c */
PORT_DECL(Chars, CanDrive) /* 1 */
PORT_DECL(Chars, CreateMainChar) /* 4 */
PORT_DECL(Chars, SetOnFloor) /* 1 */
PORT_DECL(Chars, IntSetChar) /* 2 */
PORT_DECL(Chars, StrSoundStop) /* 3 */
PORT_DECL(Chars, DiscardRes) /* 1 */
PORT_DECL(Chars, TryToDiscardThis) /* 1 */
PORT_DECL(Chars, LoadRes) /* 1 */
PORT_DECL(Chars, AddNewCharRes) /* ? */
PORT_DECL(Chars, LocalLoadRes) /* 1 */
PORT_DECL(Chars, MainCharDeath) /* 2 */
PORT_DECL(Chars, CreatePlayersList) /* 0 */
PORT_DECL(Chars, CreateStartupPlayer) /* 0 */
#define PORTS_Chars \
	PORT_DEF(Chars, CanDrive, Chars_CanDrive) \
	PORT_DEF(Chars, CreateMainChar, Chars_CreateMainChar) \
	PORT_DEF(Chars, SetOnFloor, Chars_SetOnFloor) \
	PORT_DEF(Chars, IntSetChar, Chars_IntSetChar) \
	PORT_DEF(Chars, StrSoundStop, Chars_StrSoundStop) \
	PORT_DEF(Chars, DiscardRes, Chars_DiscardRes) \
	PORT_DEF(Chars, TryToDiscardThis, Chars_TryToDiscardThis) \
	PORT_DEF(Chars, LoadRes, Chars_LoadRes) \
	PORT_DEF(Chars, AddNewCharRes, Chars_AddNewCharRes) \
	PORT_DEF(Chars, LocalLoadRes, Chars_LocalLoadRes) \
	PORT_DEF(Chars, MainCharDeath, Chars_MainCharDeath) \
	PORT_DEF(Chars, CreatePlayersList, Chars_CreatePlayersList) \
	PORT_DEF(Chars, CreateStartupPlayer, Chars_CreateStartupPlayer)

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
	PORT_DEF(CharsNPC, AddNPCPos, CharsNPC_AddNPCPos) \
	PORT_DEF(CharsNPC, CreateMissionChar, CharsNPC_CreateMissionChar) \
	PORT_DEF(CharsNPC, DeleteMissionChar, CharsNPC_DeleteMissionChar) \
	PORT_DEF(CharsNPC, SetReposChars, CharsNPC_SetReposChars) \
	PORT_DEF(CharsNPC, CreateReposChars, CharsNPC_CreateReposChars) \
	PORT_DEF(CharsNPC, BrowseNPC, CharsNPC_BrowseNPC) \
	PORT_DEF(CharsNPC, AssignAttrib, CharsNPC_AssignAttrib) \
	PORT_DEF(CharsNPC, CallCreateChar, CharsNPC_CallCreateChar) \
	PORT_DEF(CharsNPC, CreateChar, CharsNPC_CreateChar) \
	PORT_DEF(CharsNPC, CharNPCInclude, CharsNPC_CharNPCInclude) \
	PORT_DEF(CharsNPC, Respawn, CharsNPC_Respawn) \
	PORT_DEF(CharsNPC, ActiveSpawned, CharsNPC_ActiveSpawned) \
	PORT_DEF(CharsNPC, Init, CharsNPC_Init) \
	PORT_DEF(CharsNPC, CheckActualEntity, CharsNPC_CheckActualEntity) \
	PORT_DEF(CharsNPC, MercsHired, CharsNPC_MercsHired) \
	PORT_DEF(CharsNPC, IncludeSubOptions, CharsNPC_IncludeSubOptions) \
	PORT_DEF(CharsNPC, Classify, CharsNPC_Classify) \
	PORT_DEF(CharsNPC, ClassifyOptions, CharsNPC_ClassifyOptions) \
	PORT_DEF(CharsNPC, PlayOption, CharsNPC_PlayOption) \
	PORT_DEF(CharsNPC, GenericNPCSpeech, CharsNPC_GenericNPCSpeech)

/* CreditsText.c */
PORT_DECL(CreditsText, Init) /* 0 */
PORT_DECL(CreditsText, ShowCredit) /* 1 */
#define PORTS_CreditsText \
	PORT_DEF(CreditsText, Init, CreditsText_Init) \
	PORT_DEF(CreditsText, ShowCredit, CreditsText_ShowCredit)

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
	PORT_DEF(Doors, StdInitializator, Doors_StdInitializator) \
	PORT_DEF(Doors, StdObjInitializator, Doors_StdObjInitializator) \
	PORT_DEF(Doors, UsrObjInitializator, Doors_UsrObjInitializator) \
	PORT_DEF(Doors, CreateDoorClass, Doors_CreateDoorClass) \
	PORT_DEF(Doors, InitDoorInfo, Doors_InitDoorInfo) \
	PORT_DEF(Doors, Init, Doors_Init) \
	PORT_DEF(Elevator, isOutMap, Elevator_isOutMap) \
	PORT_DEF(Elevator, ElevatorCamera, Elevator_ElevatorCamera) \
	PORT_DEF(Elevator, ElevatorBackCamera, Elevator_ElevatorBackCamera) \
	PORT_DEF(Elevator, ConnectElevators, Elevator_ConnectElevators) \
	PORT_DEF(Elevator, DoorInitializator, Elevator_DoorInitializator) \
	PORT_DEF(Elevator, ObjInitializator, Elevator_ObjInitializator) \
	PORT_DEF(Elevator, ObjInitializatorNang, Elevator_ObjInitializatorNang) \
	PORT_DEF(Elevator, UseElevator, Elevator_UseElevator) \
	PORT_DEF(Elevator, ElevatorActiveToGet, Elevator_ElevatorActiveToGet) \
	PORT_DEF(Elevator, ElevatorCloseToGo, Elevator_ElevatorCloseToGo) \
	PORT_DEF(Elevator, ElevatorHaveClose, Elevator_ElevatorHaveClose) \
	PORT_DEF(Elevator, ElevatorReachUp, Elevator_ElevatorReachUp) \
	PORT_DEF(Elevator, ElevatorReachDown, Elevator_ElevatorReachDown) \
	PORT_DEF(Elevator, ElevatorHaveOpen, Elevator_ElevatorHaveOpen) \
	PORT_DEF(Elevator, LinkToElevator, Elevator_LinkToElevator) \
	PORT_DEF(Elevator, UnlinkFromElevator, Elevator_UnlinkFromElevator) \
	PORT_DEF(Elevator, Init, Elevator_Init) \
	PORT_DEF(Elevator, SetDef, Elevator_SetDef) \
	PORT_DEF(Elevator, TryToElevatorLaunchFirst, Elevator_TryToElevatorLaunchFirst) \
	PORT_DEF(Elevator, PlayerCreatedElevator, Elevator_PlayerCreatedElevator) \
	PORT_DEF(Teleport, DoorInitializator, Teleport_DoorInitializator) \
	PORT_DEF(Teleport, ObjInitializator, Teleport_ObjInitializator) \
	PORT_DEF(Teleport, Link, Teleport_Link) \
	PORT_DEF(Teleport, FXAction, Teleport_FXAction)

/* EnginesTab.c */
PORT_DECL(EnginesTab, ShowEngineDesc) /* 3 */
PORT_DECL(EnginesTab, HideEngineDesc) /* 3 */
PORT_DECL(EnginesTab, ChangeRecycleButtonState) /* 1 */
PORT_DECL(EnginesTab, UpdateEnginesList) /* 1 */
PORT_DECL(EnginesTab, PutEngine) /* 2 */
PORT_DECL(EnginesTab, RecycleEngine) /* 2 */
PORT_DECL(EnginesTab, ShowMoneySellingEngine) /* 3 */
PORT_DECL(EnginesTab, SelectEngineGroup) /* ? */
PORT_DECL(EnginesTab, SetEnginesPanel) /* 1 */
PORT_DECL(EnginesTab, ShowEnginesPanel) /* 1 */
PORT_DECL(EnginesTab, CreateLayouts) /* 1 */
PORT_DECL(EnginesTab, UpdateLayouts) /* 1 */
PORT_DECL(EnginesTab, UpdateLayoutsGlow) /* 2 */
PORT_DECL(EnginesTab, LayoutsGainFocus) /* 3 */
PORT_DECL(EnginesTab, LayoutsLostFocus) /* 3 */
PORT_DECL(EnginesTab, HideEnginesPanel) /* 1 */
PORT_DECL(EnginesTab, ScrollEngineListUp) /* 2 */
PORT_DECL(EnginesTab, ScrollEngineListDown) /* 2 */
#define PORTS_EnginesTab \
	PORT_DEF(EnginesTab, ShowEngineDesc, ET_ShowEngineDescPy) \
	PORT_DEF(EnginesTab, HideEngineDesc, ET_HideEngineDesc) \
	PORT_DEF(EnginesTab, ChangeRecycleButtonState, ET_ChangeRecycleButtonState) \
	PORT_DEF(EnginesTab, UpdateEnginesList, ET_UpdateEnginesList) \
	PORT_DEF(EnginesTab, PutEngine, ET_PutEnginePy) \
	PORT_DEF(EnginesTab, RecycleEngine, ET_RecycleEnginePy) \
	PORT_DEF(EnginesTab, ShowMoneySellingEngine, ET_ShowMoneySellingEnginePy) \
	PORT_DEF(EnginesTab, SelectEngineGroup, ET_SelectEngineGroupPy) \
	PORT_DEF(EnginesTab, SetEnginesPanel, ET_SetEnginesPanelPy) \
	PORT_DEF(EnginesTab, ShowEnginesPanel, ET_ShowEnginesPanelPy) \
	PORT_DEF(EnginesTab, CreateLayouts, ET_CreateLayoutsPy) \
	PORT_DEF(EnginesTab, UpdateLayouts, ET_UpdateLayoutsPy) \
	PORT_DEF(EnginesTab, UpdateLayoutsGlow, ET_UpdateLayoutsGlowPy) \
	PORT_DEF(EnginesTab, LayoutsGainFocus, ET_LayoutsGainFocus) \
	PORT_DEF(EnginesTab, LayoutsLostFocus, ET_LayoutsLostFocus) \
	PORT_DEF(EnginesTab, HideEnginesPanel, ET_HideEnginesPanelPy) \
	PORT_DEF(EnginesTab, ScrollEngineListUp, ET_ScrollEngineListUp) \
	PORT_DEF(EnginesTab, ScrollEngineListDown, ET_ScrollEngineListDown)

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
	PORT_DEF(Ffa, InitFfa, Ffa_InitFfa) \
	PORT_DEF(Ffa, SetupUserShip, Ffa_SetupUserShip) \
	PORT_DEF(Ffa, RestartOnPress, Ffa_RestartOnPress) \
	PORT_DEF(Ffa, DelayedRestartOnPress, Ffa_DelayedRestartOnPress) \
	PORT_DEF(Ffa, OnDeath, Ffa_OnDeath) \
	PORT_DEF(Ffa, ShowPresFireMessage, Ffa_ShowPresFireMessage) \
	PORT_DEF(Ffa, DelayedRestartOnTime, Ffa_DelayedRestartOnTime) \
	PORT_DEF(Ffa, EndOfGame, Ffa_EndOfGame) \
	PORT_DEF(Ffa, ServerSendPlayerStatus, Ffa_ServerSendPlayerStatus) \
	PORT_DEF(Ffa, ServerSendTeamData, Ffa_ServerSendTeamData) \
	PORT_DEF(Ffa, ServerEndOfGame, Ffa_ServerEndOfGame) \
	PORT_DEF(Ffa, OnDeathServer, Ffa_OnDeathServer) \
	PORT_DEF(Ffa, OnDeathClient, Ffa_OnDeathClient) \
	PORT_DEF(Ffa, Scorer2Str, Ffa_Scorer2Str) \
	PORT_DEF(Ffa, Str2Scorer, Ffa_Str2Scorer) \
	PORT_DEF(Ffa, ClientJoin, Ffa_ClientJoin) \
	PORT_DEF(Ffa, BuildCfgString, Ffa_BuildCfgString) \
	PORT_DEF(Ffa, ModiTry, Ffa_ModiTry) \
	PORT_DEF(Ffa, ModifyOnReyoin, Ffa_ModifyOnReyoin) \
	PORT_DEF(Ffa, ClientModify, Ffa_ClientModify) \
	PORT_DEF(Ffa, TryToModify, Ffa_TryToModify) \
	PORT_DEF(Ffa, PlayerMsgShow, Ffa_PlayerMsgShow) \
	PORT_DEF(Ffa, PlayerDisconected, Ffa_PlayerDisconected) \
	PORT_DEF(Ffa, PlayerSpectator, Ffa_PlayerSpectator) \
	PORT_DEF(Ffa, PlayerJoinGame, Ffa_PlayerJoinGame) \
	PORT_DEF(Ffa, ClientExit, Ffa_ClientExit) \
	PORT_DEF(Ffa, ReSpawnClient, Ffa_ReSpawnClient) \
	PORT_DEF(Ffa, ServerRemoveSpectatorState, Ffa_ServerRemoveSpectatorState) \
	PORT_DEF(Ffa, SetSpawnPos, Ffa_SetSpawnPos) \
	PORT_DEF(Ffa, InitServer, Ffa_InitServer) \
	PORT_DEF(Ffa, ClientSetConfig, Ffa_ClientSetConfig) \
	PORT_DEF(Ffa, ClientAfterCreate, Ffa_ClientAfterCreate) \
	PORT_DEF(Ffa, ClientCreate, Ffa_ClientCreate) \
	PORT_DEF(Ffa, OnUsrTest, Ffa_OnUsrTest) \
	PORT_DEF(Ffa, SvrShowMsg, Ffa_SvrShowMsg) \
	PORT_DEF(Ffa, OnUsrClient, Ffa_OnUsrClient) \
	PORT_DEF(Ffa, OnUsrServer, Ffa_OnUsrServer) \
	PORT_DEF(Ffa, JoinRed, Ffa_JoinRed) \
	PORT_DEF(Ffa, JoinGreen, Ffa_JoinGreen) \
	PORT_DEF(Ffa, ReJoinGame, Ffa_ReJoinGame) \
	PORT_DEF(Ffa, RejoinMsg, Ffa_RejoinMsg) \
	PORT_DEF(Ffa, JoinGame, Ffa_JoinGame) \
	PORT_DEF(Ffa, TeamMenu, Ffa_TeamMenu) \
	PORT_DEF(Ffa, GetGameTeamDescriptor, Ffa_GetGameTeamDescriptor) \
	PORT_DEF(Ffa, CameraSpectator, Ffa_CameraSpectator) \
	PORT_DEF(Ffa, AbortSpectateMode, Ffa_AbortSpectateMode) \
	PORT_DEF(Ffa, ServerSpectate, Ffa_ServerSpectate) \
	PORT_DEF(Ffa, InitClient, Ffa_InitClient) \
	PORT_DEF(Ffa, StdInitFfa, Ffa_StdInitFfa) \
	PORT_DEF(Ffa, OnPositionChange, Ffa_OnPositionChange) \
	PORT_DEF(Ffa, isTied, Ffa_isTied) \
	PORT_DEF(Ffa, CheckGameStart, Ffa_CheckGameStart) \
	PORT_DEF(Ffa, UsrPos, Ffa_UsrPos) \
	PORT_DEFK(Ffa, ActualizeGameScorer, Ffa_ActualizeGameScorer) \
	PORT_DEF(Ffa, ShowScorer, Ffa_ShowScorer) \
	PORT_DEF(Ffa, BackMenuFunc, Ffa_BackMenuFunc) \
	PORT_DEF(Ffa, ShowHelp, Ffa_ShowHelp) \
	PORT_DEF(Ffa, SetupGameRules, Ffa_SetupGameRules) \
	PORT_DEF(Ffa, SendGameRules, Ffa_SendGameRules) \
	PORT_DEF(Ffa, SetScorerValues, Ffa_SetScorerValues) \
	PORT_DEF(Ffa, FfaScorer, Ffa_FfaScorer)

/* Fx.c */
PORT_DECL(Fx, GetMotorClass) /* 1 */
PORT_DECL(Fx, SetUpShipMotors) /* 1 */
PORT_DECL(Fx, MoneyTransfer) /* 3 */
#define PORTS_Fx \
	PORT_DEF(Fx, GetMotorClass, Fx_GetMotorClass) \
	PORT_DEF(Fx, SetUpShipMotors, Fx_SetUpShipMotors) \
	PORT_DEF(Fx, MoneyTransfer, Fx_MoneyTransferPy)

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
PORT_DECL(DoorSound, SetSlide) /* 1 */
PORT_DECL(DoorSound, ConversorSetSounds) /* 1 */
#define PORTS_GameSounds \
	PORT_DEF(Sound, SetStdDamageSounds, Sound_SetStdDamageSounds) \
	PORT_DEF(Sound, SetStdDamageSoundLoops, Sound_SetStdDamageSoundLoops) \
	PORT_DEF(Sound, LoadDamageSounds, Sound_LoadDamageSounds) \
	PORT_DEF(Sound, LoadMenuSounds, Sound_LoadMenuSounds) \
	PORT_DEF(Sound, ScorerSounds, Sound_ScorerSounds) \
	PORT_DEF(Sound, WayPoints, Sound_WayPoints) \
	PORT_DEF(Sound, PhotoSounds, Sound_PhotoSounds) \
	PORT_DEF(Sound, CharConversionSounds, Sound_CharConversionSounds) \
	PORT_DEF(Sound, LoadShipEditInterfaceSounds, Sound_LoadShipEditInterfaceSounds) \
	PORT_DEF(Sound, Init, Sound_Init) \
	PORT_DEF(Sound, SetOutMusic, Sound_SetOutMusic) \
	PORT_DEF(Sound, SetPSLoop, Sound_SetPSLoop) \
	PORT_DEF(Sound, SetPSInterval, Sound_SetPSInterval) \
	PORT_DEF(OutSound, SetVehicleSounds, OutSound_SetVehicleSounds) \
	PORT_DEF(OutSound, LoadEngines, OutSound_LoadEngines) \
	PORT_DEF(OutSound, LoadWeapons, OutSound_LoadWeapons) \
	PORT_DEF(OutSound, LoadScorer, OutSound_LoadScorer) \
	PORT_DEF(OutSound, LoadMisc, OutSound_LoadMisc) \
	PORT_DEF(OutSound, LoadCrash, OutSound_LoadCrash) \
	PORT_DEF(OutSound, LoadVehicles, OutSound_LoadVehicles) \
	PORT_DEF(OutSound, Init, OutSound_Init) \
	PORT_DEF(DoorSound, LoadSounds, DoorSound_LoadSounds) \
	PORT_DEF(DoorSound, SetSlide, DoorSound_SetSlide) \
	PORT_DEF(DoorSound, ConversorSetSounds, DoorSound_ConversorSetSounds)

/* GateKeeper.c */
PORT_DECL(GateKeeper, UseDoor) /* ? */
PORT_DECL(GateKeeper, SetDoor) /* ? */
PORT_DECL(GateKeeper, DoorFoe) /* ? */
#define PORTS_GateKeeper \
	PORT_DEFP(GateKeeper, UseDoor, GateKeeper_UseDoor) \
	PORT_DEFP(GateKeeper, SetDoor, GateKeeper_SetDoor) \
	PORT_DEFP(GateKeeper, DoorFoe, GateKeeper_DoorFoe)

/* HangarTab.c */
PORT_DECL(HangarTab, ShowBoostPrediction) /* 2 */
PORT_DECL(HangarTab, DisableChangePlatformButtons) /* 1 */
PORT_DECL(HangarTab, EnableChangePlatformButtons) /* 1 */
PORT_DECL(HangarTab, PrevPilot) /* 2 */
PORT_DECL(HangarTab, NextPilot) /* 2 */
PORT_DECL(HangarTab, SetSellVisible) /* 2 */
PORT_DECL(HangarTab, SetSellActive) /* 2 */
PORT_DECL(HangarTab, SetExitVisible) /* 2 */
PORT_DECL(HangarTab, SetShipTypeVisible) /* 2 */
PORT_DECL(HangarTab, SetPilotVisible) /* 2 */
PORT_DECL(HangarTab, HideHangarPanel) /* 1 */
PORT_DECL(HangarTab, UpdateHangars) /* 1 */
PORT_DECL(HangarTab, ShowHangarPanel) /* 1 */
PORT_DECL(HangarTab, IsShipValid) /* 1 */
PORT_DECL(HangarTab, ShowRebuildShipMoney) /* 3 */
PORT_DECL(HangarTab, HideRebuildShipMoney) /* 3 */
PORT_DECL(HangarTab, ShowMoneyBuyingShip) /* 3 */
PORT_DECL(HangarTab, ShowMoneySellingShip) /* 3 */
PORT_DECL(HangarTab, HideMoney) /* 3 */
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
PORT_DECL(HangarTab, SelectItem) /* 2 */
PORT_DECL(HangarTab, BuyShip) /* 2 */
PORT_DECL(HangarTab, SellShip) /* 2 */
PORT_DECL(HangarTab, AddReferenceShipControls) /* 1 */
PORT_DECL(HangarTab, ShowShipMiniInfo) /* 3 */
PORT_DECL(HangarTab, HideShipMiniInfo) /* 3 */
#define PORTS_HangarTab \
	PORT_DEF(HangarTab, ShowBoostPrediction, HT_ShowBoostPrediction) \
	PORT_DEF(HangarTab, DisableChangePlatformButtons, HT_DisableChangePlatformButtons) \
	PORT_DEF(HangarTab, EnableChangePlatformButtons, HT_EnableChangePlatformButtons) \
	PORT_DEF(HangarTab, PrevPilot, HT_PrevPilot) \
	PORT_DEF(HangarTab, NextPilot, HT_NextPilot) \
	PORT_DEF(HangarTab, SetSellVisible, HT_SetSellVisible) \
	PORT_DEF(HangarTab, SetSellActive, HT_SetSellActive) \
	PORT_DEF(HangarTab, SetExitVisible, HT_SetExitVisible) \
	PORT_DEF(HangarTab, SetShipTypeVisible, HT_SetShipTypeVisible) \
	PORT_DEF(HangarTab, SetPilotVisible, HT_SetPilotVisible) \
	PORT_DEF(HangarTab, HideHangarPanel, HT_HideHangarPanel) \
	PORT_DEF(HangarTab, UpdateHangars, HT_UpdateHangars) \
	PORT_DEF(HangarTab, ShowHangarPanel, HT_ShowHangarPanel) \
	PORT_DEF(HangarTab, IsShipValid, HT_IsShipValid) \
	PORT_DEF(HangarTab, ShowRebuildShipMoney, HT_ShowRebuildShipMoney) \
	PORT_DEF(HangarTab, HideRebuildShipMoney, HT_HideRebuildShipMoney) \
	PORT_DEF(HangarTab, ShowMoneyBuyingShip, HT_ShowMoneyBuyingShip) \
	PORT_DEF(HangarTab, ShowMoneySellingShip, HT_ShowMoneySellingShip) \
	PORT_DEF(HangarTab, HideMoney, HT_HideMoney) \
	PORT_DEF(HangarTab, GetSpeedRampOffsets, HT_GetSpeedRampOffsets) \
	PORT_DEF(HangarTab, GetMaxSpeedTime, HT_GetMaxSpeedTime) \
	PORT_DEF(HangarTab, SetHangarPanel, HT_SetHangarPanel) \
	PORT_DEF(HangarTab, CmSec2Value, HT_CmSec2Value) \
	PORT_DEF(HangarTab, ShowShipInfo, HT_ShowShipInfo) \
	PORT_DEF(HangarTab, HideShipInfo, HT_HideShipInfo) \
	PORT_DEF(HangarTab, PrevShipToEdit, HT_PrevShipToEdit) \
	PORT_DEF(HangarTab, NextShipToEdit, HT_NextShipToEdit) \
	PORT_DEF(HangarTab, SetShipTypeButtonsState, HT_SetShipTypeButtonsState) \
	PORT_DEF(HangarTab, SelectShipToEdit, HT_SelectShipToEdit) \
	PORT_DEF(HangarTab, SelectItem, HT_SelectItem) \
	PORT_DEF(HangarTab, BuyShip, HT_BuyShip) \
	PORT_DEF(HangarTab, SellShip, HT_SellShip) \
	PORT_DEF(HangarTab, AddReferenceShipControls, HT_AddReferenceShipControls) \
	PORT_DEF(HangarTab, ShowShipMiniInfo, HT_ShowShipMiniInfo) \
	PORT_DEF(HangarTab, HideShipMiniInfo, HT_HideShipMiniInfo)

/* HullTab.c */
PORT_DECL(HullTab, ShowHullPredictionUp) /* 3 */
PORT_DECL(HullTab, HideHullPredictionUp) /* 3 */
PORT_DECL(HullTab, ShowHullPredictionDown) /* 3 */
PORT_DECL(HullTab, HideHullPredictionDown) /* 3 */
PORT_DECL(HullTab, IncrementHull) /* 2 */
PORT_DECL(HullTab, DecrementHull) /* 2 */
PORT_DECL(HullTab, SetHullState) /* 1 */
PORT_DECL(HullTab, HullSliderChange) /* 2 */
PORT_DECL(HullTab, SetHullPanel) /* 1 */
PORT_DECL(HullTab, ShowHullPanel) /* 1 */
PORT_DECL(HullTab, HideHullPanel) /* 1 */
#define PORTS_HullTab \
	PORT_DEF(HullTab, ShowHullPredictionUp, HT_ShowHullPredictionUp) \
	PORT_DEF(HullTab, HideHullPredictionUp, HT_HideHullPredictionUp) \
	PORT_DEF(HullTab, ShowHullPredictionDown, HT_ShowHullPredictionDown) \
	PORT_DEF(HullTab, HideHullPredictionDown, HT_HideHullPredictionDown) \
	PORT_DEF(HullTab, IncrementHull, HT_IncrementHull) \
	PORT_DEF(HullTab, DecrementHull, HT_DecrementHull) \
	PORT_DEF(HullTab, SetHullState, HT_SetHullStatePy) \
	PORT_DEF(HullTab, HullSliderChange, HT_HullSliderChange) \
	PORT_DEF(HullTab, SetHullPanel, HT_SetHullPanel) \
	PORT_DEF(HullTab, ShowHullPanel, HT_ShowHullPanel) \
	PORT_DEF(HullTab, HideHullPanel, HT_HideHullPanel)

/* InitLevel.c */
PORT_DECL(Init, LoadCharLib) /* 1 */
PORT_DECL(Init, AddMLib) /* 1 */
PORT_DECL(Init, MLibInitStatus) /* 1 */
PORT_DECL(Init, MLibInit) /* 1 */
PORT_DECL(Init, isMission) /* ? */
PORT_DECL(Init, PreloadMlibs) /* 1 */
PORT_DECL(Init, Init) /* 1 */
#define PORTS_InitLevel \
	PORT_DEF(Init, LoadCharLib, Init_LoadCharLib) \
	PORT_DEF(Init, AddMLib, Init_AddMLib) \
	PORT_DEF(Init, MLibInitStatus, Init_MLibInitStatus) \
	PORT_DEF(Init, MLibInit, Init_MLibInit) \
	PORT_DEF(Init, isMission, Init_isMission) \
	PORT_DEF(Init, PreloadMlibs, Init_PreloadMlibs) \
	PORT_DEF(Init, Init, Init_Init)

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
PORT_DECL(Items, ShowHint) /* 2 */
PORT_DECL(Items, CreateInteractiveObj) /* 2 */
PORT_DECL(Items, SetInteractiveObjOnFloor) /* 1 */
#define PORTS_Items \
	PORT_DEF(Items, ResetUpgrade, Items_ResetUpgrade) \
	PORT_DEF(Items, RestoreUpgrade, Items_RestoreUpgrade) \
	PORT_DEF(Items, DropAll, Items_DropAll) \
	PORT_DEF(Items, DropUpgrade, Items_DropUpgrade) \
	PORT_DEF(Items, BaseOnTakeUpgrade, Items_BaseOnTakeUpgrade) \
	PORT_DEF(Items, OnTakeiXtr, Items_OnTakeiXtr) \
	PORT_DEF(Items, OnTakeiSiz, Items_OnTakeiSiz) \
	PORT_DEF(Items, OnTakeaBom, Items_OnTakeaBom) \
	PORT_DEF(Items, OnTakeaSpd, Items_OnTakeaSpd) \
	PORT_DEF(Items, OnTakedAim, Items_OnTakedAim) \
	PORT_DEF(Items, OnTakedBom, Items_OnTakedBom) \
	PORT_DEF(Items, OnTaketAim, Items_OnTaketAim) \
	PORT_DEF(Items, OnTaketTra, Items_OnTaketTra) \
	PORT_DEF(Items, OnTakesRot, Items_OnTakesRot) \
	PORT_DEF(Items, OnTakesRck, Items_OnTakesRck) \
	PORT_DEF(Items, OnTakevAim, Items_OnTakevAim) \
	PORT_DEF(Items, OnTakevSpd, Items_OnTakevSpd) \
	PORT_DEF(Items, SetSound, Items_SetSound) \
	PORT_DEF(Items, ClientCreate, Items_ClientCreate) \
	PORT_DEF(Items, CreateBox, Items_CreateBox) \
	PORT_DEF(Items, ShowHint, Items_ShowHint) \
	PORT_DEF(Items, CreateInteractiveObj, Items_CreateInteractiveObj) \
	PORT_DEF(Items, SetInteractiveObjOnFloor, Items_SetInteractiveObjOnFloor)

/* MakeChar.c */
PORT_DECL(MakeChar, CreateRelaxAction) /* ? */
PORT_DECL(MakeChar, AddDefMorphActions) /* ? */
PORT_DECL(MakeChar, SetNodeRotation) /* 4 */
#define PORTS_MakeChar \
	PORT_DEFP(MakeChar, CreateRelaxAction, MakeChar_CreateRelaxAction) \
	PORT_DEFP(MakeChar, AddDefMorphActions, MakeChar_AddDefMorphActions) \
	PORT_DEF(MakeChar, SetNodeRotation, MakeChar_SetNodeRotation)

/* MenuBase.c */
PORT_DECL(Menu, DrawBackMainMenu) /* ? */
PORT_DECL(Menu, DrawBackSubMenu) /* ? */
PORT_DECL(Menu, DrawBackOptionMenu) /* ? */
PORT_DECL(Menu, DrawMenuTitleBar) /* ? */
PORT_DECL(Menu, StartNewMenu) /* ? */
PORT_DECL(Menu, YesNoMenu) /* ? */
PORT_DECL(Menu, DisableMenuItem) /* ? */
PORT_DECL(Menu, ShowTextBox) /* ? */
PORT_DECL(Menu, ShowWarning) /* ? */
PORT_DECL(Menu, SetTabDefaults) /* ? */
#define PORTS_MenuBase \
	PORT_DEF(Menu, DrawBackMainMenu, Menu_DrawBackMainMenu) \
	PORT_DEF(Menu, DrawBackSubMenu, Menu_DrawBackSubMenu) \
	PORT_DEF(Menu, DrawBackOptionMenu, Menu_DrawBackOptionMenu) \
	PORT_DEF(Menu, DrawMenuTitleBar, Menu_DrawMenuTitleBar) \
	PORT_DEF(Menu, StartNewMenu, Menu_StartNewMenu) \
	PORT_DEF(Menu, YesNoMenu, Menu_YesNoMenu) \
	PORT_DEF(Menu, DisableMenuItem, Menu_DisableMenuItem) \
	PORT_DEF(Menu, ShowTextBox, Menu_ShowTextBox) \
	PORT_DEF(Menu, ShowWarning, Menu_ShowWarning) \
	PORT_DEF(Menu, SetTabDefaults, Menu_SetTabDefaults)

/* MenuHud.c */
PORT_DECL(Scorer, ShowMissionScorer) /* ? */
PORT_DECL(Scorer, Blink) /* ? */
PORT_DECL(Scorer, MissionScorerBar_SetDefaults) /* ? */
PORT_DECL(CharScorer, Create) /* 1 */
PORT_DECL(RacerScorer, CreateWeaponScorer) /* 1 */
PORT_DECL(RacerScorer, CreateCannonScorer) /* 1 */
PORT_DECL(RacerScorer, CreateTargetScorer) /* 1 */
PORT_DECL(RacerScorer, CreateWaypointScorer) /* 1 */
PORT_DECL(RacerScorer, CreateRadarScorer) /* 1 */
PORT_DECL(RacerScorer, Create) /* 1 */
PORT_DECL(RacerScorer, ShowScorer) /* ? */
PORT_DECL(RacerScorer, InitMissionScorer) /* ? */
PORT_DECL(RacerScorer, MissionScorer) /* 1 */
PORT_DECL(RacerScorer, MissionScorerTeam) /* 1 */
PORT_DECL(RacerScorer, UpdateMissionScorer) /* 3 */
#define PORTS_MenuHud \
	PORT_DEF(Scorer, ShowMissionScorer, Scorer_ShowMissionScorer) \
	PORT_DEF(Scorer, Blink, Scorer_Blink) \
	PORT_DEF(Scorer, MissionScorerBar_SetDefaults, Scorer_MissionScorerBar_SetDefaults) \
	PORT_DEF(CharScorer, Create, CharScorer_Create) \
	PORT_DEF(RacerScorer, CreateWeaponScorer, Racer_CreateWeaponScorer) \
	PORT_DEF(RacerScorer, CreateCannonScorer, Racer_CreateCannonScorer) \
	PORT_DEF(RacerScorer, CreateTargetScorer, Racer_CreateTargetScorer) \
	PORT_DEF(RacerScorer, CreateWaypointScorer, Racer_CreateWaypointScorer) \
	PORT_DEF(RacerScorer, CreateRadarScorer, Racer_CreateRadarScorer) \
	PORT_DEF(RacerScorer, Create, Racer_Create) \
	PORT_DEF(RacerScorer, ShowScorer, Racer_ShowScorer) \
	PORT_DEF(RacerScorer, InitMissionScorer, Racer_InitMissionScorer) \
	PORT_DEF(RacerScorer, MissionScorer, Racer_MissionScorer) \
	PORT_DEF(RacerScorer, MissionScorerTeam, Racer_MissionScorerTeam) \
	PORT_DEF(RacerScorer, UpdateMissionScorer, Racer_UpdateMissionScorer)

/* MenuMain.c */
PORT_DECL(Menu, MainMenu) /* ? */
PORT_DECL(Menu, BackToGame) /* ? */
PORT_DECL(Menu, DrawScorer) /* ? */
PORT_DECL(Menu, InitMenuSys) /* ? */
PORT_DECL(Menu, MovieScorer) /* ? */
PORT_DECL(Menu, GameOver) /* ? */
#define PORTS_MenuMain \
	PORT_DEF(Menu, MainMenu, Menu_MainMenu) \
	PORT_DEF(Menu, BackToGame, Menu_BackToGame) \
	PORT_DEF(Menu, DrawScorer, Menu_DrawScorer) \
	PORT_DEF(Menu, InitMenuSys, Menu_InitMenuSys) \
	PORT_DEF(Menu, MovieScorer, Menu_MovieScorer) \
	PORT_DEF(Menu, GameOver, Menu_GameOver)

/* MenuMission.c */
PORT_DECL(Menu, EnterMissionInfo) /* ? */
#define PORTS_MenuMission \
	PORT_DEF(Menu, EnterMissionInfo, Menu_EnterMissionInfo)

/* MenuNet.c */
PORT_DECL(PCMenu, TryJoin) /* ? */
#define PORTS_MenuNet \
	PORT_DEFP(PCMenu, TryJoin, PCMenu_TryJoin)

/* Metro.c */
PORT_DECL(Metro, SetUsrParams) /* ? */
PORT_DECL(Metro, SetAsMetro) /* 1 */
#define PORTS_Metro \
	PORT_DEF(Metro, SetUsrParams, Metro_SetUsrParamsPy) \
	PORT_DEF(Metro, SetAsMetro, Metro_SetAsMetroPy)

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
PORT_DECL(CrazyWing, CrazyPhoto) /* 1 */
#define PORTS_MisItems \
	PORT_DEF(MisItems, Init, MisItems_Init) \
	PORT_DEF(MisItems, OnVehicleDeat, MisItems_OnVehicleDeat) \
	PORT_DEF(MisItems, GetTargetReward, MisItems_GetTargetReward) \
	PORT_DEF(MisItems, GiveMoney, MisItems_GiveMoney) \
	PORT_DEF(MisItems, GetMoneyModel, MisItems_GetMoneyModel) \
	PORT_DEF(MisItems, GetObjModel, MisItems_GetObjModel) \
	PORT_DEF(MisItems, CreateItem, MisItems_CreateItem) \
	PORT_DEF(MisItems, CreateCoin, MisItems_CreateCoin) \
	PORT_DEF(MisItems, ActivateCoin, MisItems_ActivateCoin) \
	PORT_DEF(MisItems, CreateBattery, MisItems_CreateBattery) \
	PORT_DEF(MisItems, ActivateBattery, MisItems_ActivateBattery) \
	PORT_DEF(MisItems, TakeBattery, MisItems_TakeBattery) \
	PORT_DEF(MisItems, DazedCoin, MisItems_DazedCoin) \
	PORT_DEF(CrazyWing, InitCrazyWing, CrazyWing_InitCrazyWing) \
	PORT_DEF(CrazyWing, CrazyWing, CrazyWing_CrazyWing) \
	PORT_DEF(CrazyWing, SetCrazy, CrazyWing_SetCrazy) \
	PORT_DEF(CrazyWing, DeCrazy, CrazyWing_DeCrazy) \
	PORT_DEF(CrazyWing, RestoreSpeech, CrazyWing_RestoreSpeech) \
	PORT_DEF(CrazyWing, CrazyPhoto, CrazyWing_CrazyPhoto)

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
#define PORTS_MissionsFuncs \
	PORT_DEF(MissionsFuncs, Init, MissionsFuncs_Init) \
	PORT_DEF(MissionsFuncs, TryMsg, MissionsFuncs_TryMsg) \
	PORT_DEF(MissionsFuncs, SetNextMission, MissionsFuncs_SetNextMission) \
	PORT_DEF(MissionsFuncs, SetFirstDemoMission, MissionsFuncs_SetFirstDemoMission) \
	PORT_DEF(MissionsFuncs, SetOutMissionArrow, MissionsFuncs_SetOutMissionArrow) \
	PORT_DEF(MissionsFuncs, ActivateTarget, MissionsFuncs_ActivateTarget) \
	PORT_DEF(MissionsFuncs, GetSecondary, MissionsFuncs_GetSecondary) \
	PORT_DEF(MissionsFuncs, SetSecondary, MissionsFuncs_SetSecondary) \
	PORT_DEF(MissionsFuncs, FinishGame, MissionsFuncs_FinishGame) \
	PORT_DEF(MissionsFuncs, ShowMissionInfo, MissionsFuncs_ShowMissionInfo) \
	PORT_DEF(MissionsFuncs, SetMissionTarget, MissionsFuncs_SetMissionTarget) \
	PORT_DEF(MissionsFuncs, SetMultipleMissionTargets, MissionsFuncs_SetMultipleMissionTargets) \
	PORT_DEF(MissionsFuncs, CleanAllTargets, MissionsFuncs_CleanAllTargets) \
	PORT_DEF(MissionsFuncs, DelMissionTarget, MissionsFuncs_DelMissionTarget) \
	PORT_DEF(MissionsFuncs, IsCharMissionObjective, MissionsFuncs_IsCharMissionObjective) \
	PORT_DEF(MissionsFuncs, CharMissionDialog, MissionsFuncs_CharMissionDialog) \
	PORT_DEF(MissionsFuncs, CancelSecondaryMission, MissionsFuncs_CancelSecondaryMission) \
	PORT_DEF(MissionsFuncs, SinglePlayerDeath, MissionsFuncs_SinglePlayerDeath) \
	PORT_DEF(MissionsFuncs, CreatePhotoObject, MissionsFuncs_CreatePhotoObject) \
	PORT_DEF(MissionsFuncs, UnsetPhotoObject, MissionsFuncs_UnsetPhotoObject) \
	PORT_DEF(MissionsFuncs, ReassignBertosPlayerTarget, MissionsFuncs_ReassignBertosPlayerTarget) \
	PORT_DEF(MissionsFuncs, CreateBerto, MissionsFuncs_CreateBerto) \
	PORT_DEF(MissionsFuncs, CreateBertos, MissionsFuncs_CreateBertos) \
	PORT_DEF(MissionsFuncs, OnBertoDeath, MissionsFuncs_OnBertoDeath) \
	PORT_DEF(MissionsFuncs, CrazyDealStatus, MissionsFuncs_CrazyDealStatus) \
	PORT_DEF(MissionsFuncs, GetNumCrazyDealCompleted, MissionsFuncs_GetNumCrazyDealCompleted) \
	PORT_DEF(MissionsFuncs, OnCrazyDealFinishedMessage, MissionsFuncs_OnCrazyDealFinishedMessage) \
	PORT_DEF(MissionsFuncs, OnCrazyDealTarget, MissionsFuncs_OnCrazyDealTarget) \
	PORT_DEF(MissionsFuncs, NewCrazyDeal, MissionsFuncs_NewCrazyDeal) \
	PORT_DEF(MissionsFuncs, SetNextSuperDeal, MissionsFuncs_SetNextSuperDeal) \
	PORT_DEF(MissionsFuncs, StartActualSuperDeal, MissionsFuncs_StartActualSuperDeal) \
	PORT_DEF(MissionsFuncs, EndOfSuperDeal, MissionsFuncs_EndOfSuperDeal) \
	PORT_DEF(MissionsFuncs, StartSuperDeal, MissionsFuncs_StartSuperDeal) \
	PORT_DEF(MissionsFuncs, RelaxShipsInList, MissionsFuncs_RelaxShipsInList) \
	PORT_DEF(MissionsFuncs, SetInvulnerableShipsInList, MissionsFuncs_SetInvulnerableShipsInList) \
	PORT_DEF(MissionsFuncs, OnSuperDealFinish, MissionsFuncs_OnSuperDealFinish) \
	PORT_DEF(MissionsFuncs, BuildMultipleEnemiesData, MissionsFuncs_BuildMultipleEnemiesData) \
	PORT_DEF(MissionsFuncs, BuildTeamDeathMatchData, MissionsFuncs_BuildTeamDeathMatchData) \
	PORT_DEF(MissionsFuncs, BuildVersusData, MissionsFuncs_BuildVersusData) \
	PORT_DEF(MissionsFuncs, BuildDominationData, MissionsFuncs_BuildDominationData) \
	PORT_DEF(MissionsFuncs, BuildCTFData, MissionsFuncs_BuildCTFData)

/* Net.c */
PORT_DECL(Net, PurgeServerMapList) /* ? */
#define PORTS_Net \
	PORT_DEF(Net, PurgeServerMapList, Net_PurgeServerMapList)

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
	PORT_DEF(OutMap, BrowseSpawnPos, OutMap_BrowseSpawnPos) \
	PORT_DEF(OutMap, GetSpawnPos, OutMap_GetSpawnPos) \
	PORT_DEF(OutMap, GetFreeSpawnPos, OutMap_GetFreeSpawnPos) \
	PORT_DEF(OutMap, GetFreeSpawnPosReal, OutMap_GetFreeSpawnPosReal) \
	PORT_DEF(OutMap, GetNearestFreeSpawnPosReal, OutMap_GetNearestFreeSpawnPosReal) \
	PORT_DEF(OutMap, GetFreeSpawn, OutMap_GetFreeSpawn) \
	PORT_DEFK(OutMap, GetNearestFreeSpawnPos, OutMap_GetNearestFreeSpawnPos) \
	PORT_DEFK(OutMap, GetFarthestFreeSpawnPos, OutMap_GetFarthestFreeSpawnPos) \
	PORT_DEF(OutMap, CreatePlayers, OutMap_CreatePlayers) \
	PORT_DEF(OutMap, InitOutMap, OutMap_InitOutMap) \
	PORT_DEF(OutMap, BrowseAtmospherePos, OutMap_BrowseAtmospherePos) \
	PORT_DEF(OutMap, Init, OutMap_Init) \
	PORT_DEF(InTraffic, GetNam, It_GetNamPy) \
	PORT_DEF(InTraffic, FakeWeap, InTraffic_FakeWeap) \
	PORT_DEF(InTraffic, LoadFakeTraffic, InTraffic_LoadFakeTraffic) \
	PORT_DEF(InTraffic, CreateList, It_CreateListPy) \
	PORT_DEF(InTraffic, FakeTraffic, InTraffic_FakeTraffic) \
	PORT_DEF(InTraffic, SetSound, It_SetSoundPy)

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
	PORT_DEF(Parking, RestoreDeathPlayerShip, Parking_RestoreDeathPlayerShip) \
	PORT_DEF(Parking, SetInvunerable, Parking_SetInvunerable) \
	PORT_DEF(Parking, CharOnShip, Parking_CharOnShip) \
	PORT_DEFK(Parking, UseShip, Parking_UseShip) \
	PORT_DEF(Parking, PlayVoiceChannel, Parking_PlayVoiceChannel) \
	PORT_DEF(Parking, ExecSwitchVehicleMissionCallback, Parking_ExecSwitchVehicleMissionCallback) \
	PORT_DEF(Parking, EndPathCam, Parking_EndPathCam) \
	PORT_DEFK(Parking, SetCameraStart, Parking_SetCameraStart) \
	PORT_DEF(Parking, isValidToPark, Parking_isValidToPark) \
	PORT_DEF(Parking, OnPark, Parking_OnPark) \
	PORT_DEF(Parking, SetDtritusPlate, Parking_SetDtritusPlate) \
	PORT_DEF(Parking, CreateDtritusPlate, Parking_CreateDtritusPlate) \
	PORT_DEF(Parking, CreatePlayerShip, Parking_CreatePlayerShip) \
	PORT_DEF(Parking, ShowChar, Parking_ShowChar) \
	PORT_DEF(Parking, EndJunkerPathCam, Parking_EndJunkerPathCam) \
	PORT_DEF(Parking, JunkerPark, Parking_JunkerPark) \
	PORT_DEF(Parking, OnParkingOccupied, Parking_OnParkingOccupied) \
	PORT_DEF(Parking, OnParkedOut, Parking_OnParkedOut) \
	PORT_DEF(Parking, CreateBox, Parking_CreateBox) \
	PORT_DEF(Parking, SetShipRadius, Parking_SetShipRadius) \
	PORT_DEF(Parking, SetOnFloor, Parking_SetOnFloor) \
	PORT_DEF(Parking, SetShipAsParked, Parking_SetShipAsParked) \
	PORT_DEF(Parking, ParkedDeat, Parking_ParkedDeat) \
	PORT_DEF(Parking, BrowseItemPos, Parking_BrowseItemPos) \
	PORT_DEF(Parking, CreateLonelyMercParkings, Parking_CreateLonelyMercParkings) \
	PORT_DEFK(Parking, CreateParkedShip, Parking_CreateParkedShip) \
	PORT_DEF(Parking, CheckWeapons, Parking_CheckWeapons) \
	PORT_DEF(Parking, BrowseParkedPos, Parking_BrowseParkedPos) \
	PORT_DEF(Parking, ChangeLevel, Parking_ChangeLevel) \
	PORT_DEF(Parking, BrowseParkingAreas, Parking_BrowseParkingAreas) \
	PORT_DEF(Parking, BrowseSwitchLevel, Parking_BrowseSwitchLevel) \
	PORT_DEF(Parking, ShowNextMap, Parking_ShowNextMap) \
	PORT_DEF(Parking, DoorInitializator, Parking_DoorInitializator) \
	PORT_DEF(Parking, Init, Parking_Init) \
	PORT_DEFK(Accelerator, CreateAccelerator, Accelerator_CreateAccelerator) \
	PORT_DEF(Accelerator, Init, Accelerator_Init)

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
	PORT_DEF(PhoneCab, ForceResetPhoneCab, PhoneCab_ForceResetPhoneCab) \
	PORT_DEF(PhoneCab, RevengeSpeech, PhoneCab_RevengeSpeech) \
	PORT_DEF(PhoneCab, SetRevengeOptions, PhoneCab_SetRevengeOptions) \
	PORT_DEF(PhoneCab, GetNearestPhoneCab, PhoneCab_GetNearestPhoneCab) \
	PORT_DEF(PhoneCab, CreatePhoneCab, PhoneCab_CreatePhoneCab) \
	PORT_DEF(PhoneCab, CreateAudioCom, PhoneCab_CreateAudioCom) \
	PORT_DEF(PhoneCab, DefUseAudioCom, PhoneCab_DefUseAudioCom) \
	PORT_DEF(PhoneCab, UseAudioCom, PhoneCab_UseAudioCom) \
	PORT_DEF(PhoneCab, SpawnChar, PhoneCab_SpawnChar) \
	PORT_DEF(PhoneCab, CreatePhoneCabClass, PhoneCab_CreatePhoneCabClass) \
	PORT_DEF(PhoneCab, CreateAudioComClass, PhoneCab_CreateAudioComClass) \
	PORT_DEF(PhoneCab, Init, PhoneCab_Init) \
	PORT_DEF(PhoneCab, GetFree, PhoneCab_GetFree) \
	PORT_DEF(DTritusDesktop, ForceResetDTritusDesktop, DTritus_ForceResetDTritusDesktop) \
	PORT_DEF(DTritusDesktop, SpawnChar, DTritus_SpawnChar) \
	PORT_DEF(DTritusDesktop, CreateDTritusDesktopClass, DTritus_CreateDTritusDesktopClass) \
	PORT_DEF(DTritusDesktop, CreateDTritusDesktop, DTritus_CreateDTritusDesktop) \
	PORT_DEF(DTritusDesktop, DefUseDTritusDesktop, DTritus_DefUseDTritusDesktop) \
	PORT_DEF(DTritusDesktop, UseDTritusDesktop, DTritus_UseDTritusDesktop) \
	PORT_DEF(DTritusDesktop, CreatePressDesktopClass, DTritus_CreatePressDesktopClass) \
	PORT_DEF(DTritusDesktop, CreatePressDesktop, DTritus_CreatePressDesktop) \
	PORT_DEF(DTritusDesktop, Init, DTritus_Init) \
	PORT_DEF(DTritusDesktop, InitNPCS, DTritus_InitNPCS)

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
	PORT_DEF(RaceMaker, ShowSystemMessage, RaceMaker_ShowSystemMessage) \
	PORT_DEF(RaceMaker, ShowSystemMessageDelay, RaceMaker_ShowSystemMessageDelay) \
	PORT_DEF(RaceMaker, NPCCombatKilled, RaceMaker_NPCCombatKilled) \
	PORT_DEF(RaceMaker, DeleteCurrentWaypoint, RaceMaker_DeleteCurrentWaypoint) \
	PORT_DEF(RaceMaker, MovePlayersToTraffic, RaceMaker_MovePlayersToTraffic) \
	PORT_DEF(RaceMaker, EndOfRace, RaceMaker_EndOfRace) \
	PORT_DEF(RaceMaker, RaceStart, RaceMaker_RaceStart) \
	PORT_DEFK(RaceMaker, OnCounterMeasure, RaceMaker_OnCounterMeasure) \
	PORT_DEF(RaceMaker, RacerKilled, RaceMaker_RacerKilled) \
	PORT_DEF(RaceMaker, RacerInterruptedByAlarm, RaceMaker_RacerInterruptedByAlarm) \
	PORT_DEF(RaceMaker, RacerDamaged, RaceMaker_RacerDamaged) \
	PORT_DEF(RaceMaker, RacerLeft, RaceMaker_RacerLeft) \
	PORT_DEF(RaceMaker, AbortCameraTravelling, RaceMaker_AbortCameraTravelling) \
	PORT_DEFK(RaceMaker, StartCameraTravelling, RaceMaker_StartCameraTravelling) \
	PORT_DEF(RaceMaker, OnRaceStart, RaceMaker_OnRaceStart) \
	PORT_DEF(RaceMaker, RaceGo, RaceMaker_RaceGo) \
	PORT_DEF(RaceMaker, CreateRaceCountDown, RaceMaker_CreateRaceCountDown) \
	PORT_DEF(RaceMaker, DestroyRaceCountDown, RaceMaker_DestroyRaceCountDown) \
	PORT_DEF(RaceMaker, DestroyRaceCountDownEnd, RaceMaker_DestroyRaceCountDownEnd) \
	PORT_DEFK(RaceMaker, Counter, RaceMaker_Counter) \
	PORT_DEF(RaceMaker, OnCheckPoint, RaceMaker_OnCheckPoint) \
	PORT_DEF(RaceMaker, OnNearCheckPoint, RaceMaker_OnNearCheckPoint) \
	PORT_DEF(RaceMaker, NameOponGoItem, RaceMaker_NameOponGoItem) \
	PORT_DEF(RaceMaker, LocalGetDealType, RaceMaker_LocalGetDealType) \
	PORT_DEF(RaceMaker, StopTimer, RaceMaker_StopTimer) \
	PORT_DEF(RaceMaker, StartTimer, RaceMaker_StartTimer) \
	PORT_DEF(RaceMaker, FirstTimeRace, RaceMaker_FirstTimeRace) \
	PORT_DEF(RaceMaker, FirstTimeCombat, RaceMaker_FirstTimeCombat) \
	PORT_DEF(RaceMaker, PointObjectTo, RaceMaker_PointObjectTo) \
	PORT_DEF(RaceMaker, PreloadCnDnResources, RaceMaker_PreloadCnDnResources) \
	PORT_DEFK(RaceMaker, InitRace, RaceMaker_InitRace) \
	PORT_DEFK(RaceMaker, InitCombat, RaceMaker_InitCombat) \
	PORT_DEF(RaceMaker, MoveCombatNPCsToPos, RaceMaker_MoveCombatNPCsToPos) \
	PORT_DEF(RaceMaker, PlaceNPCAtRacePos, RaceMaker_PlaceNPCAtRacePos) \
	PORT_DEF(RaceMaker, SetScorerValues, RaceMaker_SetScorerValues) \
	PORT_DEF(RaceMaker, MissionScorer2, RaceMaker_MissionScorer2) \
	PORT_DEF(RaceMaker, MissionScorer, RaceMaker_MissionScorer) \
	PORT_DEF(RaceMaker, InitShipIAProfile, RaceMaker_InitShipIAProfile) \
	PORT_DEF(RaceMaker, GetRandomShipIAProfile, RaceMaker_GetRandomShipIAProfile)

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
PORT_DECL(SaveGame, InitVars) /* ? */
#define PORTS_SaveGame \
	PORT_DEF(SaveGame, MMDesc, SaveGame_MMDesc) \
	PORT_DEF(SaveGame, SMDesc, SaveGame_SMDesc) \
	PORT_DEF(SaveGame, SaveGame, SaveGame_SaveGame) \
	PORT_DEFK(SaveGame, InstantSaveGame, SaveGame_InstantSaveGame) \
	PORT_DEF(SaveGame, LoadGame, SaveGame_LoadGame) \
	PORT_DEF(SaveGame, NewGame, SaveGame_NewGame) \
	PORT_DEF(SaveGame, CheckAllBlueprints, SaveGame_CheckAllBlueprints) \
	PORT_DEFK(SaveGame, MarkWayPoint, SaveGame_MarkWayPoint) \
	PORT_DEF(SaveGame, QWayPoint, SaveGame_QWayPoint) \
	PORT_DEF(SaveGame, SaveAlarm, SaveGame_SaveAlarm) \
	PORT_DEF(SaveGame, LoadAlarm, SaveGame_LoadAlarm) \
	PORT_DEF(SaveGame, SaveMoney, SaveGame_SaveMoney) \
	PORT_DEF(SaveGame, LoadMoney, SaveGame_LoadMoney) \
	PORT_DEFK(SaveGame, QLoad, SaveGame_QLoad) \
	PORT_DEFK(SaveGame, StartGame, SaveGame_StartGame) \
	PORT_DEF(SaveGame, Restart, SaveGame_Restart) \
	PORT_DEF(SaveGame, QSaveReg, SaveGame_QSaveReg) \
	PORT_DEF(SaveGame, SaveReg, SaveGame_SaveReg) \
	PORT_DEF(SaveGame, InitVars, SaveGame_InitVars)

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
	PORT_DEF(Scene, BaseAddActor, Scene_BaseAddActor) \
	PORT_DEF(Scene, AddCharActor, Scene_AddCharActor) \
	PORT_DEF(Scene, AddLocalActor, Scene_AddLocalActor) \
	PORT_DEFK(Scene, ActorAction, Scene_ActorAction) \
	PORT_DEF(Scene, ActorActEvent, Scene_ActorActEvent) \
	PORT_DEF(Scene, ActorBreakChannel, Scene_ActorBreakChannel) \
	PORT_DEF(Scene, ActorActBreakChanne, Scene_ActorActBreakChanne) \
	PORT_DEFK(Scene, ObjAction, Scene_ObjAction) \
	PORT_DEF(Scene, ObjActEvent, Scene_ObjActEvent) \
	PORT_DEF(Scene, SetDefSounds, Scene_SetDefSounds) \
	PORT_DEFK(Scene, CreateLocalAction, Scene_CreateLocalAction) \
	PORT_DEF(Scene, ExecGeneric, Scene_ExecGeneric) \
	PORT_DEFK(Scene, AddGeneric, Scene_AddGeneric) \
	PORT_DEFK(Scene, AddStream, Scene_AddStream) \
	PORT_DEF(Scene, PlayStream, Scene_PlayStream) \
	PORT_DEF(Scene, PlayVoiceVol, Scene_PlayVoiceVol) \
	PORT_DEF(Scene, PlayText, Scene_PlayText) \
	PORT_DEF(Scene, SetText, Scene_SetText) \
	PORT_DEFK(Scene, StopSpeech, Scene_StopSpeech) \
	PORT_DEFK(Scene, AddText, Scene_AddText) \
	PORT_DEF(Scene, AbortScene, Scene_AbortScene) \
	PORT_DEF(Scene, EndOfScene, Scene_EndOfScene) \
	PORT_DEF(Scene, DelClass, Scene_DelClass) \
	PORT_DEFK(Scene, AddSound, Scene_AddSound) \
	PORT_DEF(Scene, Load, Scene_Load) \
	PORT_DEF(Scene, UnhideChars, Scene_UnhideChars) \
	PORT_DEF(Scene, HideThis, Scene_HideThis) \
	PORT_DEF(Scene, HideChars, Scene_HideChars) \
	PORT_DEF(Scene, HideVehiclesToo, Scene_HideVehiclesToo) \
	PORT_DEF(Scene, Play, Scene_Play) \
	PORT_DEFK(Scene, InitOutDoorScene, Scene_InitOutDoorScene) \
	PORT_DEF(Scene, EndOutDoorScene, Scene_EndOutDoorScene)

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
	PORT_DEF(ShipEdit, IsEditingMultiplayer, SE_IsEditingMultiplayer) \
	PORT_DEF(ShipEdit, RustyOnPlatform, SE_RustyOnPlatform) \
	PORT_DEF(ShipEdit, PointObjectTo, SE_PointObjectTo) \
	PORT_DEF(ShipEdit, LaunchPlatformAnim, SE_LaunchPlatformAnim) \
	PORT_DEF(ShipEdit, ChangeShip, SE_ChangeShip) \
	PORT_DEF(ShipEdit, VerifyChangeShip, SE_VerifyChangeShip) \
	PORT_DEF(ShipEdit, DoorInitializator, SE_DoorInitializator) \
	PORT_DEF(ShipEdit, ObjInitializator, SE_ObjInitializator) \
	PORT_DEF(ShipEdit, EndMoveEngine, SE_EndMoveEngine) \
	PORT_DEF(ShipEdit, RotateShip, SE_RotateShip) \
	PORT_DEF(ShipEdit, LoopRotateShip, SE_LoopRotateShip) \
	PORT_DEF(ShipEdit, StartRotateLoop, SE_StartRotateLoop) \
	PORT_DEF(ShipEdit, StopRotateLoop, SE_StopRotateLoop) \
	PORT_DEF(ShipEdit, AddShipEditTabs, SE_AddShipEditTabs) \
	PORT_DEF(ShipEdit, AddDefaultPanelContent, SE_AddDefaultPanelContent) \
	PORT_DEF(ShipEdit, ChangeTabLeft, SE_ChangeTabLeft) \
	PORT_DEF(ShipEdit, ChangeTabRight, SE_ChangeTabRight) \
	PORT_DEF(ShipEdit, ChangeTab, SE_ChangeTab) \
	PORT_DEF(ShipEdit, CleanEditedShips, SE_CleanEditedShips) \
	PORT_DEF(ShipEdit, CreatePlayerShip, SE_CreatePlayerShip) \
	PORT_DEF(ShipEdit, SetupForEdit, SE_SetupForEdit) \
	PORT_DEF(ShipEdit, SaveShipInfoInSaveVar, SE_SaveShipInfoInSaveVar) \
	PORT_DEF(ShipEdit, SaveShipInfoInGVar, SE_SaveShipInfoInGVar) \
	PORT_DEFK(ShipEdit, CreateShipFromSaveVar, SE_CreateShipFromSaveVar) \
	PORT_DEF(ShipEdit, AssignShip, SE_AssignShip) \
	PORT_DEF(ShipEdit, SetupEdit, SE_SetupEdit) \
	PORT_DEF(ShipEdit, FreeEdit, SE_FreeEdit) \
	PORT_DEF(ShipEdit, EnterEditShipMenu, SE_EnterEditShipMenu) \
	PORT_DEF(ShipEdit, ExitEditShipMenu, SE_ExitEditShipMenu) \
	PORT_DEF(ShipEdit, SaveShip, SE_SaveShip) \
	PORT_DEF(ShipEdit, ShowSpecialHint, SE_ShowSpecialHint)

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
	PORT_DEF(ShipMaker, SetShipPrice, ShipMaker_SetShipPrice) \
	PORT_DEF(ShipMaker, GetShipPrice, ShipMaker_GetShipPrice) \
	PORT_DEF(ShipMaker, SetEnginePrice, ShipMaker_SetEnginePrice) \
	PORT_DEF(ShipMaker, GetEnginePrice, ShipMaker_GetEnginePrice) \
	PORT_DEF(ShipMaker, AddShipTon, ShipMaker_AddShipTon) \
	PORT_DEF(ShipMaker, GetShipTon, ShipMaker_GetShipTon) \
	PORT_DEF(ShipMaker, AddShipBalance, ShipMaker_AddShipBalance) \
	PORT_DEF(ShipMaker, GetShipBalance, ShipMaker_GetShipBalance) \
	PORT_DEFK(ShipMaker, AddShip, ShipMaker_AddShip) \
	PORT_DEF(ShipMaker, GetShip, ShipMaker_GetShip) \
	PORT_DEF(ShipMaker, AddMotor, ShipMaker_AddMotor) \
	PORT_DEF(ShipMaker, GetMotor, ShipMaker_GetMotor) \
	PORT_DEF(ShipMaker, IncludeMotor, ShipMaker_IncludeMotor) \
	PORT_DEF(ShipMaker, AddWeapon, ShipMaker_AddWeapon) \
	PORT_DEF(ShipMaker, GetWeapon, ShipMaker_GetWeapon) \
	PORT_DEF(ShipMaker, CalculateVerticalBalance, ShipMaker_CalculateVerticalBalance) \
	PORT_DEF(ShipMaker, SetUpChars, ShipMaker_SetUpChars) \
	PORT_DEF(ShipMaker, AddChar, ShipMaker_AddChar) \
	PORT_DEF(ShipMaker, GetBoostFactor, ShipMaker_GetBoostFactor) \
	PORT_DEF(ShipMaker, SetShipParams, ShipMaker_SetShipParams) \
	PORT_DEF(ShipMaker, CreateStdShips, ShipMaker_CreateStdShips) \
	PORT_DEF(ShipMaker, Init, ShipMaker_Init) \
	PORT_DEF(ShipMaker, SetAnmShip, ShipMaker_SetAnmShip) \
	PORT_DEFK(ShipMaker, InitAnmShip, ShipMaker_InitAnmShip)

/* Speech.c */
PORT_DECL(Speech, ScheduledVoiceCallback) /* 2 */
PORT_DECL(Speech, PlayVoiceScheduled) /* 3 */
PORT_DECL(Speech, PerformAnswer) /* ? */
PORT_DECL(Speech, NormalAbortSpeech) /* ? */
PORT_DECL(Speech, CallEndOfSpeech) /* ? */
PORT_DECL(Speech, AbortSpeech) /* ? */
PORT_DECL(Speech, RestoreState) /* 1 */
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
	PORT_DEF(Speech, ScheduledVoiceCallback, Speech_ScheduledVoiceCallback) \
	PORT_DEF(Speech, PlayVoiceScheduled, Speech_PlayVoiceScheduled) \
	PORT_DEFK(Speech, PerformAnswer, Speech_PerformAnswer) \
	PORT_DEF(Speech, NormalAbortSpeech, Speech_NormalAbortSpeech) \
	PORT_DEF(Speech, CallEndOfSpeech, Speech_CallEndOfSpeech) \
	PORT_DEFK(Speech, AbortSpeech, Speech_AbortSpeech) \
	PORT_DEF(Speech, RestoreState, Speech_RestoreState) \
	PORT_DEF(Speech, AutoAnswer, Speech_AutoAnswer) \
	PORT_DEF(Speech, StopMenuEvents, Speech_StopMenuEvents) \
	PORT_DEF(Speech, RestartMenuEvents, Speech_RestartMenuEvents) \
	PORT_DEF(Speech, RestoreMoveTime, Speech_RestoreMoveTime) \
	PORT_DEF(Speech, StdAnsLst, Speech_StdAnsLst) \
	PORT_DEFK(Speech, StdOnSpch, Speech_StdOnSpch) \
	PORT_DEF(Speech, CalculateSpeechTime, Speech_CalculateSpeechTime) \
	PORT_DEFK(Speech, OnSpch, Speech_OnSpch) \
	PORT_DEFK(Speech, OnChallengeSpch, Speech_OnChallengeSpch) \
	PORT_DEFK(Speech, OnSpeech, Speech_OnSpeech) \
	PORT_DEF(Speech, PlayerAnsEnd, Speech_PlayerAnsEnd) \
	PORT_DEF(Speech, PlayerAns, Speech_PlayerAns) \
	PORT_DEF(Speech, SetPlayerToTalk, Speech_SetPlayerToTalk) \
	PORT_DEF(Speech, InspectChar, Speech_InspectChar) \
	PORT_DEF(Speech, AutoSpeech, Speech_AutoSpeech) \
	PORT_DEFK(Speech, NISpeech, Speech_NISpeech) \
	PORT_DEFK(Speech, AutoAddNI, Speech_AutoAddNI) \
	PORT_DEFK(Speech, AddNI, Speech_AddNI) \
	PORT_DEFK(Speech, RunNI, Speech_RunNI) \
	PORT_DEF(Speech, ObjNIEnd, Speech_ObjNIEnd) \
	PORT_DEF(Speech, LaunchTaunt, Speech_LaunchTaunt) \
	PORT_DEF(Speech, RestoreSpeech, Speech_RestoreSpeech) \
	PORT_DEF(Speech, OffVoice, Speech_OffVoice) \
	PORT_DEFK(Speech, RemoteComunication, Speech_RemoteComunication) \
	PORT_DEFK(Speech, ForceRemoteComInternal, Speech_ForceRemoteComInternal) \
	PORT_DEFK(Speech, ForceRemoteCom, Speech_ForceRemoteCom) \
	PORT_DEFK(Speech, ForceRemoteComunication, Speech_ForceRemoteComunication) \
	PORT_DEFK(Speech, AddRemoteCom, Speech_AddRemoteCom) \
	PORT_DEF(Speech, ProcessRemoteCom, Speech_ProcessRemoteCom) \
	PORT_DEF(Speech, CancelRemoteCom, Speech_CancelRemoteCom) \
	PORT_DEF(Speech, TestInRange, Speech_TestInRange) \
	PORT_DEFK(Speech, StartObjSpch, Speech_StartObjSpch) \
	PORT_DEF(Speech, StartObjectSpeech, Speech_StartObjectSpeech) \
	PORT_DEFK(Speech, StdObjSpch, Speech_StdObjSpch) \
	PORT_DEFK(Speech, OnObjSpch, Speech_OnObjSpch) \
	PORT_DEFK(Speech, OnChallengeObjSpch, Speech_OnChallengeObjSpch) \
	PORT_DEFK(Speech, OnObjectSpeech, Speech_OnObjectSpeech) \
	PORT_DEF(Speech, ObjectAutoAnswer, Speech_ObjectAutoAnswer) \
	PORT_DEF(Speech, ObjectAutoAnswer2, Speech_ObjectAutoAnswer2) \
	PORT_DEF(Speech, EndObjectAnswer, Speech_EndObjectAnswer)

/* Split.c */
PORT_DECL(Split, GetSpawnPos) /* 1 */
PORT_DECL(Split, CreateUserShip) /* 1 */
PORT_DECL(Split, GetReSpawnPos) /* 1 */
PORT_DECL(Split, ReSpawnShip) /* 1 */
PORT_DECL(Split, OnDeathCallback) /* 2 */
PORT_DECL(Split, NextShip) /* 1 */
PORT_DECL(Split, PrevShip) /* 1 */
PORT_DECL(Split, EndOfGame) /* 1 */
PORT_DECL(Split, SeleShip) /* 1 */
PORT_DECL(Split, InitChooseScorer) /* 1 */
PORT_DECL(Split, ConfirmAbortMain) /* 1 */
PORT_DECL(Split, ConfirmAbortPause) /* 1 */
PORT_DECL(Split, ConfirmRestartPause) /* 1 */
PORT_DECL(Split, Abort) /* 1 */
PORT_DECL(Split, Restart) /* 1 */
PORT_DECL(Split, PauseGame) /* 1 */
PORT_DECL(Split, PauseGameMenu) /* 1 */
PORT_DECL(Split, LittleText) /* 4 */
PORT_DECL(Split, BlackBack) /* 1 */
PORT_DECL(SDeathMatch, OnDeathFunc) /* 2 */
PORT_DECL(SFlagHunt, TakeFlag) /* 2 */
PORT_DECL(SFlagHunt, OnDeathFunc) /* 2 */
PORT_DECL(SPostman, TakeArea) /* 2 */
PORT_DECL(SPostman, TakeFlag) /* 2 */
PORT_DECL(SPostman, OnDeathFunc) /* 2 */
PORT_DECL(SRace, TakeFlag) /* 2 */
PORT_DECL(SRace, InitRacePos) /* 1 */
PORT_DECL(SRace, GetReSpawnPos) /* 1 */
#define PORTS_Split \
	PORT_DEF(Split, GetSpawnPos, Split_GetSpawnPos) \
	PORT_DEF(Split, CreateUserShip, Split_CreateUserShip) \
	PORT_DEF(Split, GetReSpawnPos, Split_GetSpawnPos) \
	PORT_DEF(Split, ReSpawnShip, Split_ReSpawnShip) \
	PORT_DEF(Split, OnDeathCallback, Split_OnDeathCallback) \
	PORT_DEF(Split, NextShip, Split_NextShip) \
	PORT_DEF(Split, PrevShip, Split_PrevShip) \
	PORT_DEF(Split, EndOfGame, Split_EndOfGame) \
	PORT_DEF(Split, SeleShip, Split_SeleShip) \
	PORT_DEF(Split, InitChooseScorer, Split_InitChooseScorer) \
	PORT_DEF(Split, ConfirmAbortMain, Split_ConfirmAbortMain) \
	PORT_DEF(Split, ConfirmAbortPause, Split_ConfirmAbortPause) \
	PORT_DEF(Split, ConfirmRestartPause, Split_ConfirmRestartPause) \
	PORT_DEF(Split, Abort, Split_Abort) \
	PORT_DEF(Split, Restart, Split_Restart) \
	PORT_DEF(Split, PauseGame, Split_PauseGame) \
	PORT_DEF(Split, PauseGameMenu, Split_PauseGameMenu) \
	PORT_DEF(Split, LittleText, Split_LittleText) \
	PORT_DEF(Split, BlackBack, Split_BlackBack) \
	PORT_DEF(SDeathMatch, OnDeathFunc, SDeathMatch_OnDeathFunc) \
	PORT_DEF(SFlagHunt, TakeFlag, SFlagHunt_TakeFlag) \
	PORT_DEF(SFlagHunt, OnDeathFunc, SFlagHunt_OnDeathFunc) \
	PORT_DEF(SPostman, TakeArea, SPostman_TakeArea) \
	PORT_DEF(SPostman, TakeFlag, SPostman_TakeFlag) \
	PORT_DEF(SPostman, OnDeathFunc, SPostman_OnDeathFunc) \
	PORT_DEF(SRace, TakeFlag, SRace_TakeFlag) \
	PORT_DEF(SRace, InitRacePos, SRace_InitRacePos) \
	PORT_DEF(SRace, GetReSpawnPos, SRace_GetReSpawnPos)

/* Sputnik.c */
PORT_DECL(Sputnik, Assign) /* ? */
PORT_DECL(Sputnik, SetSptAt) /* ? */
#define PORTS_Sputnik \
	PORT_DEFP(Sputnik, Assign, Sputnik_Assign) \
	PORT_DEFP(Sputnik, SetSptAt, Sputnik_SetSptAt)

/* SputnikInterface.c */
PORT_DECL(SputnikInterface, AddDefaultPanelContent) /* 1 */
PORT_DECL(SputnikInterface, EnterSputnikInterface) /* 1 */
PORT_DECL(SputnikInterface, ExitSputnikInterface) /* 2 */
PORT_DECL(SputnikInterface, SelectShip) /* 2 */
#define PORTS_SputnikInterface \
	PORT_DEF(SputnikInterface, AddDefaultPanelContent, SI_AddDefaultPanelContent) \
	PORT_DEF(SputnikInterface, EnterSputnikInterface, SI_EnterSputnikInterface) \
	PORT_DEF(SputnikInterface, ExitSputnikInterface, SI_ExitSputnikInterface) \
	PORT_DEF(SputnikInterface, SelectShip, SI_SelectShip)

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
PORT_DECL(CTF, RespawnFriend) /* 0 */
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
	PORT_DEF(DeathMatch, GetMenuDescription, Dm_GetMenuDescription) \
	PORT_DEF(DeathMatch, InitStatus, Dm_InitStatus) \
	PORT_DEF(DeathMatch, Init, Dm_Init) \
	PORT_DEF(DeathMatch, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEF(DeathMatch, SetProfile, Dm_SetProfile) \
	PORT_DEF(DeathMatch, CreateEnemies, Dm_CreateEnemies) \
	PORT_DEF(DeathMatch, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEF(DeathMatch, CrazyGamblerStartCombat, Dm_CrazyGamblerStartCombat) \
	PORT_DEF(DeathMatch, SetVulnerable, Sd_SetVulnerable) \
	PORT_DEF(DeathMatch, StartCombat, Dm_StartCombat) \
	PORT_DEF(DeathMatch, GetMaxFragsEnemies, Dm_GetMaxFragsEnemies) \
	PORT_DEF(DeathMatch, OnEnemyDeath, Dm_OnEnemyDeath) \
	PORT_DEF(DeathMatch, RespawnEnemy, Dm_RespawnEnemy) \
	PORT_DEF(DeathMatch, RespawnPlayer, Sd_RespawnPlayer) \
	PORT_DEF(DeathMatch, OnPlayerDeath, Dm_OnPlayerDeath) \
	PORT_DEF(DeathMatch, RestartCombat, Dm_RestartCombat) \
	PORT_DEF(TeamDeathMatch, GetMenuDescription, Tdm_GetMenuDescription) \
	PORT_DEF(TeamDeathMatch, InitStatus, Tdm_InitStatus) \
	PORT_DEF(TeamDeathMatch, Init, Tdm_Init) \
	PORT_DEF(TeamDeathMatch, SetInvulnerableStatus, Tdm_SetInvulnerableStatus) \
	PORT_DEF(TeamDeathMatch, SetEnemyProfile, Dm_SetProfile) \
	PORT_DEF(TeamDeathMatch, SetFriendProfile, Tdm_SetFriendProfile) \
	PORT_DEF(TeamDeathMatch, CreateFriends, Tdm_CreateFriends) \
	PORT_DEF(TeamDeathMatch, CreateEnemies, Tdm_CreateEnemies) \
	PORT_DEF(TeamDeathMatch, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEF(TeamDeathMatch, CrazyGamblerStartCombat, Tdm_CrazyGamblerStartCombat) \
	PORT_DEF(TeamDeathMatch, OnVehicle, Sd_SetVulnerable) \
	PORT_DEF(TeamDeathMatch, StartCombat, Tdm_StartCombat) \
	PORT_DEF(TeamDeathMatch, OnEnemyDeath, Tdm_OnEnemyDeath) \
	PORT_DEF(TeamDeathMatch, OnEnemyDeath2, Tdm_OnEnemyDeath2) \
	PORT_DEF(TeamDeathMatch, RespawnEnemy, Tdm_RespawnEnemy) \
	PORT_DEF(TeamDeathMatch, OnFriendDeath, Tdm_OnFriendDeath) \
	PORT_DEF(TeamDeathMatch, OnFriendDeath2, Tdm_OnFriendDeath2) \
	PORT_DEF(TeamDeathMatch, RespawnFriend, Tdm_RespawnFriend) \
	PORT_DEF(TeamDeathMatch, RespawnPlayer, Sd_RespawnPlayer) \
	PORT_DEF(TeamDeathMatch, OnPlayerDeath, Tdm_OnPlayerDeath) \
	PORT_DEF(TeamDeathMatch, OnPlayerDeath2, Tdm_OnPlayerDeath2) \
	PORT_DEF(TeamDeathMatch, RestartCombat, Tdm_RestartCombat) \
	PORT_DEF(Versus, GetMenuDescription, Vs_GetMenuDescription) \
	PORT_DEF(Versus, InitStatus, Vs_InitStatus) \
	PORT_DEF(Versus, Init, Vs_Init) \
	PORT_DEF(Versus, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEF(Versus, CrazyGamblerStartCombat, Vs_CrazyGamblerStartCombat) \
	PORT_DEF(Versus, StartCombat, Vs_StartCombat) \
	PORT_DEF(Versus, OnEnemyDeath, Vs_OnEnemyDeath) \
	PORT_DEF(Versus, RespawnEnemy, Vs_RespawnEnemy) \
	PORT_DEF(Versus, OnPlayerDeath, Vs_OnPlayerDeath) \
	PORT_DEF(Versus, EnemyWins, Vs_EnemyWins) \
	PORT_DEF(Versus, RespawnPlayer, Vs_RespawnPlayer) \
	PORT_DEF(Versus, RestartCombat, Vs_RestartCombat) \
	PORT_DEF(CTF, GetMenuDescription, Ctf_GetMenuDescription) \
	PORT_DEF(CTF, InitStatus, Ctf_InitStatus) \
	PORT_DEF(CTF, Init, Ctf_Init) \
	PORT_DEF(CTF, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEF(CTF, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEF(CTF, SetProfile, Ctf_SetProfile) \
	PORT_DEF(CTF, CreateEnemies, Ctf_CreateEnemies) \
	PORT_DEF(CTF, CreateFriends, Ctf_CreateFriends) \
	PORT_DEF(CTF, CrazyGamblerStartCTF, Ctf_CrazyGamblerStartCTF) \
	PORT_DEF(CTF, SetVulnerable, Sd_SetVulnerable) \
	PORT_DEF(CTF, StartCTF, Ctf_StartCTF) \
	PORT_DEF(CTF, OnVehicleDeathCallback, Ctf_OnVehicleDeathCallback) \
	PORT_DEF(CTF, OnPlayerDeath, Ctf_OnPlayerDeath) \
	PORT_DEF(CTF, RemoveFlag, Ctf_RemoveFlag) \
	PORT_DEF(CTF, EndOfCTF, Ctf_EndOfCTF) \
	PORT_DEF(CTF, RespawnFriend, Ctf_RespawnFriend) \
	PORT_DEF(CTF, ResetFlag, Ctf_ResetFlag) \
	PORT_DEF(CTF, TakeFlag, Ctf_TakeFlag) \
	PORT_DEF(CTF, SpawnFlags, Ctf_SpawnFlags) \
	PORT_DEF(CTF, RespawnPlayer, Ctf_RespawnPlayer) \
	PORT_DEF(CTF, PreRestartMenu, Ctf_PreRestartMenu) \
	PORT_DEF(CTF, RestartCTF, Sd_Restart) \
	PORT_DEF(CTF, PreOnVictory, Ctf_PreOnVictory) \
	PORT_DEF(Domination, GetMenuDescription, Dom_GetMenuDescription) \
	PORT_DEF(Domination, InitStatus, Dom_InitStatus) \
	PORT_DEF(Domination, Init, Dom_Init) \
	PORT_DEF(Domination, ReleaseBaton, Dom_ReleaseBaton) \
	PORT_DEF(Domination, LostBatonMessage, Dom_LostBatonMessage) \
	PORT_DEF(Domination, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEF(Domination, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEF(Domination, SetProfile, Dom_SetProfile) \
	PORT_DEF(Domination, CreateEnemies, Dom_CreateEnemies) \
	PORT_DEF(Domination, CreateFriends, Dom_CreateFriends) \
	PORT_DEF(Domination, CrazyGamblerStartDomination, Dom_CrazyGamblerStartDomination) \
	PORT_DEF(Domination, OnVehicle, Dom_OnVehicle) \
	PORT_DEF(Domination, StartDomination, Dom_StartDomination) \
	PORT_DEF(Domination, TakeBaton, Dom_TakeBaton) \
	PORT_DEF(Domination, CreateZone, Dom_CreateZone) \
	PORT_DEF(Domination, TakeZone, Dom_TakeZone) \
	PORT_DEF(Domination, ActivateZone, Dom_ActivateZone) \
	PORT_DEF(Domination, RestartDomination, Dom_RestartDomination) \
	PORT_DEF(Domination, OnPlayerDeath, Dom_OnPlayerDeath) \
	PORT_DEF(Domination, OnEnemyDeath, Dom_OnEnemyDeath) \
	PORT_DEF(Domination, OnFriendDeath, Dom_OnFriendDeath) \
	PORT_DEF(Domination, RespawnPlayer, Dom_RespawnPlayer) \
	PORT_DEF(Domination, RespawnEnemy, Dom_RespawnEnemy) \
	PORT_DEF(Domination, RespawnFriend, Dom_RespawnFriend) \
	PORT_DEF(Domination, EndOfDomination, Dom_EndOfDomination) \
	PORT_DEF(Domination, PreRestartMenu, Dom_PreRestartMenu) \
	PORT_DEF(Domination, RestartDomin, Dom_RestartDomin) \
	PORT_DEF(Domination, PreOnVictory, Dom_PreOnVictory) \
	PORT_DEF(Domination, MissionScorer, Dom_MissionScorer) \
	PORT_DEF(MortalRace, GetMenuDescription, Mr_GetMenuDescription) \
	PORT_DEF(MortalRace, InitStatus, Mr_InitStatus) \
	PORT_DEF(MortalRace, Init, Mr_Init) \
	PORT_DEF(MortalRace, ReleaseBaton, Mr_ReleaseBaton) \
	PORT_DEF(MortalRace, LostBatonMessage, Mr_LostBatonMessage) \
	PORT_DEF(MortalRace, SetInvulnerableStatus, Sd_SetInvulnerable) \
	PORT_DEF(MortalRace, AbortRemoteMessages, Sd_AbortRemoteMessages) \
	PORT_DEF(MortalRace, CrazyGamblerStartMortalRace, Mr_CrazyGamblerStartMortalRace) \
	PORT_DEF(MortalRace, SetVulnerable, Sd_SetVulnerable) \
	PORT_DEF(MortalRace, CreateBaton, Mr_CreateBaton) \
	PORT_DEF(MortalRace, TakeBaton, Mr_TakeBaton) \
	PORT_DEF(MortalRace, CreateFirstWaypoint, Mr_CreateFirstWaypoint) \
	PORT_DEF(MortalRace, OnRaceStart, Mr_OnRaceStart) \
	PORT_DEF(MortalRace, RaceGo, Mr_RaceGo) \
	PORT_DEF(MortalRace, CheckShipsState, Mr_CheckShipsState) \
	PORT_DEF(MortalRace, OnPositionReached, Mr_OnPositionReached) \
	PORT_DEF(MortalRace, OnCheckPoint, Mr_OnCheckPoint) \
	PORT_DEF(MortalRace, Restart, Mr_Restart) \
	PORT_DEF(MortalRace, RestartRace, Sd_Restart) \
	PORT_DEF(MortalRace, MoveWaypoint, Mr_MoveWaypoint) \
	PORT_DEF(MortalRace, EndOfMortalRace, Mr_EndOfMortalRace) \
	PORT_DEF(MortalRace, MoveRacersToTraffic, Mr_MoveRacersToTraffic) \
	PORT_DEF(MortalRace, MoveRacersToPos, Mr_MoveRacersToPos) \
	PORT_DEF(MortalRace, RacersAttackShip, Mr_RacersAttackShip) \
	PORT_DEF(MortalRace, OnPlayerDeath, Mr_OnPlayerDeath) \
	PORT_DEF(MortalRace, RacerKilled, Mr_RacerKilled) \
	PORT_DEF(MortalRace, RacerDamaged, Mr_RacerDamaged) \
	PORT_DEF(MortalRace, RespawnRacer, Mr_RespawnRacer) \
	PORT_DEF(MortalRace, RespawnPlayer, Mr_RespawnPlayer) \
	PORT_DEF(MortalRace, PreOnVictory, Mr_PreOnVictory)

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
	PORT_DEF(Traffic, SetPath, Traffic_SetPath) \
	PORT_DEF(Traffic, ResetPath, Traffic_ResetPath) \
	PORT_DEF(Traffic, InitAIProfiles, Traffic_InitAIProfiles) \
	PORT_DEF(Traffic, EmptyFunc, Traffic_EmptyFunc) \
	PORT_DEF(Traffic, MakeTrafic, Traffic_MakeTrafic) \
	PORT_DEF(Traffic, InitTraffic, Traffic_InitTraffic) \
	PORT_DEF(Traffic, OnNeedMoreTraffic, Traffic_OnNeedMoreTraffic) \
	PORT_DEF(Traffic, OnNeedMoreChasingTraffic, Traffic_OnNeedMoreChasingTraffic) \
	PORT_DEF(Traffic, OnPlayerHelpedPolice, Traffic_OnPlayerHelpedPolice) \
	PORT_DEF(Traffic, RailTrafficDeath, Traffic_RailTrafficDeath) \
	PORT_DEF(Traffic, HeavyTrafficDeath, Traffic_HeavyTrafficDeath) \
	PORT_DEF(Traffic, NormalTrafficDeath, Traffic_NormalTrafficDeath) \
	PORT_DEF(Traffic, MediumTrafficDeath, Traffic_MediumTrafficDeath) \
	PORT_DEF(Traffic, InitCivilTraffic, Traffic_InitCivilTraffic) \
	PORT_DEF(Traffic, Init, Traffic_Init) \
	PORT_DEF(OutPolice, MakePoli, OutPolice_MakePoli) \
	PORT_DEF(OutPolice, Init, OutPolice_Init) \
	PORT_DEF(OutPolice, NextWave, OutPolice_NextWave) \
	PORT_DEF(OutPolice, InitShips, OutPolice_InitShips) \
	PORT_DEF(OutPolice, InitWaves, OutPolice_InitWaves) \
	PORT_DEF(OutPolice, InitAIProfiles, OutPolice_InitAIProfiles) \
	PORT_DEF(InPolice, Include, InPolice_Include) \
	PORT_DEF(InPolice, SetGVars, InPolice_SetGVars) \
	PORT_DEF(InPolice, Init, InPolice_Init)

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
PORT_DECL(Vehicles, OnDeath) /* 2 */
PORT_DECL(Vehicles, SwitchView) /* 2 */
PORT_DECL(Vehicles, SetActiveCam) /* 3 */
PORT_DECL(Vehicles, ReSpawnShip) /* 1 */
PORT_DECL(Vehicles, SetProperties) /* ? */
PORT_DECL(Vehicles, SetStdProperties) /* 3 */
PORT_DECL(Vehicles, Init) /* 0 */
#define PORTS_Vehicles \
	PORT_DEF(Vehicles, ClientCreate, Vehicles_ClientCreate) \
	PORT_DEF(Vehicles, ClientAfterCreate, Vehicles_ClientAfterCreate) \
	PORT_DEF(Vehicles, ServerMotorHit, Vehicles_ServerMotorHit) \
	PORT_DEF(Vehicles, MotorHit, Vehicles_MotorHit) \
	PORT_DEF(Vehicles, CreateDumbMotor, Vehicles_CreateDumbMotor) \
	PORT_DEF(Vehicles, SetUpDumbMotor, Vehicles_SetUpDumbMotor) \
	PORT_DEF(Vehicles, DumbShipHit, Vehicles_DumbShipHit) \
	PORT_DEF(Vehicles, OnClientDeath, Vehicles_OnClientDeath) \
	PORT_DEF(Vehicles, ActivateHitType, Vehicles_ActivateHitType) \
	PORT_DEF(Vehicles, GetCoin, Vehicles_GetCoin) \
	PORT_DEF(Vehicles, OnDeath, Vehicles_OnDeath) \
	PORT_DEF(Vehicles, SwitchView, Vehicles_SwitchView) \
	PORT_DEF(Vehicles, SetActiveCam, Vehicles_SetActiveCam) \
	PORT_DEF(Vehicles, ReSpawnShip, Vehicles_ReSpawnShip) \
	PORT_DEFK(Vehicles, SetProperties, Vehicles_SetProperties) \
	PORT_DEF(Vehicles, SetStdProperties, Vehicles_SetStdProperties) \
	PORT_DEF(Vehicles, Init, Vehicles_Init)

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
	PORT_DEF(VehiclesNPC, InitStdShips, VehiclesNPC_InitStdShips) \
	PORT_DEFK(VehiclesNPC, MakeAI, VehiclesNPC_MakeAI) \
	PORT_DEF(VehiclesNPC, GetIAProf, VehiclesNPC_GetIAProf) \
	PORT_DEFK(VehiclesNPC, ModAIProff, VehiclesNPC_ModAIProff) \
	PORT_DEFK(VehiclesNPC, SetIAProf, VehiclesNPC_SetIAProf) \
	PORT_DEF(VehiclesNPC, SetAIProfile, VehiclesNPC_SetAIProfile) \
	PORT_DEF(VehiclesNPC, SetStdProperties, VehiclesNPC_SetStdProperties) \
	PORT_DEF(VehiclesNPC, MercOnDeath, VehiclesNPC_MercOnDeath) \
	PORT_DEF(VehiclesNPC, InitMercs, VehiclesNPC_InitMercs) \
	PORT_DEF(VehiclesNPC, Init, VehiclesNPC_Init)

/* Weapons.c */
PORT_DECL(Vulcan, Init) /* 2 */
PORT_DECL(Devastator, Init) /* 2 */
PORT_DECL(Tesla, Init) /* 2 */
PORT_DECL(ATPC, Init) /* 2 */
PORT_DECL(Laser, Init) /* 2 */
PORT_DECL(Swarm, ClientCreate) /* 2 */
PORT_DECL(Swarm, ClientAfterCreate) /* 2 */
PORT_DECL(Swarm, Init) /* 2 */
PORT_DECL(Inferno, ClientCreate) /* 2 */
PORT_DECL(Inferno, ClientAfterCreate) /* 2 */
PORT_DECL(Inferno, Init) /* 2 */
PORT_DECL(Cloud, ClientCreate) /* 2 */
PORT_DECL(Cloud, ClientAfterCreate) /* 2 */
PORT_DECL(Cloud, ActivateHitType) /* 2 */
PORT_DECL(Cloud, CreateCloud) /* 7 */
PORT_DECL(Cloud, SetUpCloud) /* 1 */
PORT_DECL(Cloud, Init) /* 1 */
PORT_DECL(EMI, ClientCreate) /* 2 */
PORT_DECL(EMI, ClientAfterCreate) /* 2 */
PORT_DECL(EMI, ActivateHitType) /* 2 */
PORT_DECL(EMI, CreateEMI) /* 7 */
PORT_DECL(EMI, SetUpEMI) /* 1 */
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
PORT_DECL(Sonic, Init) /* 1 */
#define PORTS_Weapons \
	PORT_DEF(Vulcan, Init, Vulcan_Init) \
	PORT_DEF(Devastator, Init, Devastator_Init) \
	PORT_DEF(Tesla, Init, Tesla_Init) \
	PORT_DEF(ATPC, Init, ATPC_Init) \
	PORT_DEF(Laser, Init, Laser_Init) \
	PORT_DEF(Swarm, ClientCreate, Swarm_ClientCreate) \
	PORT_DEF(Swarm, ClientAfterCreate, Swarm_ClientAfterCreate) \
	PORT_DEF(Swarm, Init, Swarm_Init) \
	PORT_DEF(Inferno, ClientCreate, Inferno_ClientCreate) \
	PORT_DEF(Inferno, ClientAfterCreate, Inferno_ClientAfterCreate) \
	PORT_DEF(Inferno, Init, Inferno_Init) \
	PORT_DEF(Cloud, ClientCreate, Cloud_ClientCreate) \
	PORT_DEF(Cloud, ClientAfterCreate, Cloud_ClientAfterCreate) \
	PORT_DEF(Cloud, ActivateHitType, Weap_ActivateHitType) \
	PORT_DEF(Cloud, CreateCloud, Cloud_CreateCloud) \
	PORT_DEF(Cloud, SetUpCloud, Cloud_SetUpCloud) \
	PORT_DEF(Cloud, Init, Cloud_Init) \
	PORT_DEF(EMI, ClientCreate, EMI_ClientCreate) \
	PORT_DEF(EMI, ClientAfterCreate, EMI_ClientAfterCreate) \
	PORT_DEF(EMI, ActivateHitType, Weap_ActivateHitType) \
	PORT_DEF(EMI, CreateEMI, EMI_CreateEMI) \
	PORT_DEF(EMI, SetUpEMI, EMI_SetUpEMI) \
	PORT_DEF(EMI, Init, EMI_Init) \
	PORT_DEF(Sonic, ClientCreate, Sonic_ClientCreate) \
	PORT_DEF(Sonic, ClientAfterCreate, Sonic_ClientAfterCreate) \
	PORT_DEF(Sonic, SonicHit, Sonic_SonicHit) \
	PORT_DEF(Sonic, OnHit, Sonic_OnHit) \
	PORT_DEF(Sonic, OnServerHit, Sonic_OnServerHit) \
	PORT_DEF(Sonic, OnDeath, Sonic_OnDeath) \
	PORT_DEF(Sonic, SetUpToDetonate, Sonic_SetUpToDetonate) \
	PORT_DEF(Sonic, CreateSonic, Sonic_CreateSonic) \
	PORT_DEF(Sonic, SetUpSonic, Sonic_SetUpSonic) \
	PORT_DEF(Sonic, Init, Sonic_Init)

/* WeaponsTab.c */
PORT_DECL(WeaponsTab, ShowWeightPrediction) /* 3 */
PORT_DECL(WeaponsTab, HideWeightPrediction) /* 3 */
PORT_DECL(WeaponsTab, ShowWeaponDesc) /* 3 */
PORT_DECL(WeaponsTab, ShowWeaponUpgradeDesc) /* 3 */
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
	PORT_DEF(WeaponsTab, ShowWeightPrediction, WT_ShowWeightPrediction) \
	PORT_DEF(WeaponsTab, HideWeightPrediction, WT_HideWeightPrediction) \
	PORT_DEF(WeaponsTab, ShowWeaponDesc, WT_ShowWeaponDescPy) \
	PORT_DEF(WeaponsTab, ShowWeaponUpgradeDesc, WT_ShowWeaponUpgradeDescPy) \
	PORT_DEF(WeaponsTab, ShowSlotMarker, WT_ShowSlotMarkerPy) \
	PORT_DEF(WeaponsTab, HideSlotMarker, WT_HideSlotMarkerPy) \
	PORT_DEF(WeaponsTab, LoadWeapon, WT_LoadWeaponPy) \
	PORT_DEF(WeaponsTab, HideSlots, WT_HideSlotsPy) \
	PORT_DEF(WeaponsTab, FillSlots, WT_FillSlotsPy) \
	PORT_DEF(WeaponsTab, UpdateUpgrades, WT_UpdateUpgradesPy) \
	PORT_DEF(WeaponsTab, SelectWeapon, WT_SelectWeaponPy) \
	PORT_DEF(WeaponsTab, SelectUpgrade, WT_SelectUpgradePy) \
	PORT_DEF(WeaponsTab, SetWeaponsPanel, WT_SetWeaponsPanelPy) \
	PORT_DEF(WeaponsTab, ShowWeaponsPanel, WT_ShowWeaponsPanelPy) \
	PORT_DEF(WeaponsTab, HideWeaponsPanel, WT_HideWeaponsPanelPy)

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
	PORT_DEFL(ab, MapFinalFight, GetBossProfile, Ff_GetBossProfilePy) \
	PORT_DEFL(ab, MapFinalFight, FakeShips, Ff_FakeShipsPy) \
	PORT_DEFL(ab, MapFinalFight, InitStatus, Ff_InitStatusPy) \
	PORT_DEFL(ab, MapFinalFight, Init, Ff_InitPy) \
	PORT_DEFL(ab, MapFinalFight, CreateBoss, Ff_CreateBossPy) \
	PORT_DEFL(ab, MapFinalFight, SetShieldPhase, Ff_SetShieldPhasePy) \
	PORT_DEFL(ab, MapFinalFight, MissionScorer, Ff_MissionScorerPy) \
	PORT_DEFL(ab, MapFinalFight, SetFase, Ff_SetFasePy) \
	PORT_DEFL(ab, MapFinalFight, BossAttacks, Ff_BossAttacksPy) \
	PORT_DEFL(ab, MapFinalFight, BossRelax, Ff_BossRelaxPy) \
	PORT_DEFL(ab, MapFinalFight, OnPlayerDeath, Ff_OnPlayerDeathPy) \
	PORT_DEFL(ab, MapFinalFight, RespawnPlayer, Ff_RespawnPlayerPy) \
	PORT_DEFL(ab, MapFinalFight, LaunchTaunt, Ff_LaunchTauntPy) \
	PORT_DEFL(ab, MapFinalFight, OnBossRecoversZero, Ff_OnBossRecoversZeroPy) \
	PORT_DEFL(ab, MapFinalFight, OnBossRecoversEnd, Ff_OnBossRecoversEndPy) \
	PORT_DEFL(ab, MapFinalFight, OnBossDestroyed, Ff_OnBossDestroyedPy) \
	PORT_DEFL(ab, MapFinalFight, PreLoadFinal, Ff_PreLoadFinalPy) \
	PORT_DEFL(ab, Map, onTrackCreate, AbMap_onTrackCreatePy) \
	PORT_DEFL(ab, Map, InitStatus, AbMap_InitStatusPy) \
	PORT_DEFL(ab, Map, Init, AbMap_InitPy) \
	PORT_DEFL(ab, Map, CreateAsteroid, AbMap_CreateAsteroidPy) \
	PORT_DEFL(ab, Map, detraffic, AbMap_detrafficPy) \
	PORT_DEFL(ab, Map, OnDeath, AbMap_OnDeathPy) \
	PORT_DEFL(ab, Map, RealOnDeath, AbMap_RealOnDeathPy)

/* levels/ABIndoor.c */
PORT_DECLL(abindoor, Map, InitStatus) /* 1 */
PORT_DECLL(abindoor, Map, Init) /* 1 */
PORT_DECLL(abindoor, Map, LaunchCinematic) /* 4 */
PORT_DECLL(abindoor, Map, ShowEndTitle) /* 1 */
PORT_DECLL(abindoor, Map, FadeEndTitle) /* 1 */
#define PORTS_levels_ABIndoor \
	PORT_DEFL(abindoor, Map, InitStatus, Abi_InitStatusPy) \
	PORT_DEFL(abindoor, Map, Init, Abi_InitPy) \
	PORT_DEFL(abindoor, Map, LaunchCinematic, Abi_LaunchCinematicPy) \
	PORT_DEFL(abindoor, Map, ShowEndTitle, Abi_ShowEndTitlePy) \
	PORT_DEFL(abindoor, Map, FadeEndTitle, Abi_FadeEndTitlePy)

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
	PORT_DEFL(bank, Map, InitStatus, BankMap_InitStatusPy) \
	PORT_DEFL(bank, Map, Init, BankMap_InitPy) \
	PORT_DEFL(bank, MapTreasure, InitDeal, MapTreasure_InitDealPy) \
	PORT_DEFL(bank, MapTreasure, TakeMiniDeal, MapTreasure_TakeMiniDealPy) \
	PORT_DEFL(bank, MapTreasure, BlinkBigPrize, MapTreasure_BlinkBigPrizePy) \
	PORT_DEFL(bank, MapTreasure, BigPrize, MapTreasure_BigPrizePy) \
	PORT_DEFL(bank, MapTreasure, DeleteShent, MapTreasure_DeleteShentPy) \
	PORT_DEFL(bank, MapTreasure, RestartShents, MapTreasure_RestartShentsPy) \
	PORT_DEFL(bank, MapTreasure, InitStatus, MapTreasure_InitStatusPy) \
	PORT_DEFL(bank, MapTreasure, MakeCoin, MapTreasure_MakeCoinPy) \
	PORT_DEFL(bank, MapTreasure, CreateCoins, MapTreasure_CreateCoinsPy) \
	PORT_DEFL(bank, Mission_CatchRudolph, InitStatus, CatchRudolph_InitStatusPy) \
	PORT_DEFL(bank, Mission_CatchRudolph, CreateRudolph, CatchRudolph_CreateRudolphPy) \
	PORT_DEFL(bank, Mission_CatchRudolph, OnCatchRudolph, CatchRudolph_OnCatchRudolphPy) \
	PORT_DEFL(bank, Mission_CatchRudolph, SetNextMission, CatchRudolph_SetNextMissionPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, ComputeAutoUpdateStopValue, GetDataPart2_ComputeAutoUpdateStopValuePy) \
	PORT_DEFL(bank, Mission_GetDataPart2, InitStatus, GetDataPart2_InitStatusPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, Init, GetDataPart2_InitPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, TalkToBoss, GetDataPart2_TalkToBossPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, BossSpeech, GetDataPart2_BossSpeechPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, PreInitMission, GetDataPart2_PreInitMissionPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, InitMission, GetDataPart2_InitMissionPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, SetDesktopState, GetDataPart2_SetDesktopStatePy) \
	PORT_DEFL(bank, Mission_GetDataPart2, CreateFuncInit, GetDataPart2_CreateFuncInitPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, OnFunctionarySpeech, GetDataPart2_OnFunctionarySpeechPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, OnFuncDefState, GetDataPart2_OnFuncDefStatePy) \
	PORT_DEFL(bank, Mission_GetDataPart2, SetStopDownloadingState, GetDataPart2_SetStopDownloadingStatePy) \
	PORT_DEFL(bank, Mission_GetDataPart2, SetNormalState, GetDataPart2_SetNormalStatePy) \
	PORT_DEFL(bank, Mission_GetDataPart2, AbortDownload, GetDataPart2_AbortDownloadPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, OnDownloadAborted, GetDataPart2_OnDownloadAbortedPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, OnBeginDownloadData, GetDataPart2_OnBeginDownloadDataPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, OnFuncStartsWorking, GetDataPart2_OnFuncStartsWorkingPy) \
	PORT_DEFL(bank, Mission_GetDataPart2, OnDownloadComplete, GetDataPart2_OnDownloadCompletePy) \
	PORT_DEFL(bank, Mission_GetDataPart2, MissionScorer, GetDataPart2_MissionScorerPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, InitStatus, InfiltrateBank_InitStatusPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, Init, InfiltrateBank_InitPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, UseDoor, InfiltrateBank_UseDoorPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, TestEndOfElevator, InfiltrateBank_TestEndOfElevatorPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, ReAssignTarget, InfiltrateBank_ReAssignTargetPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, AfterDTMessage, InfiltrateBank_AfterDTMessagePy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, ShowDeepThroatMessage, InfiltrateBank_ShowDeepThroatMessagePy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, SetArrowToBankMaster, InfiltrateBank_SetArrowToBankMasterPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, CreateBankMaster, InfiltrateBank_CreateBankMasterPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, LaunchScene, InfiltrateBank_LaunchScenePy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, LaunchScene2, InfiltrateBank_LaunchScene2Py) \
	PORT_DEFL(bank, Mission_InfiltrateBank, BankMasterSpeech, InfiltrateBank_BankMasterSpeechPy) \
	PORT_DEFL(bank, Mission_InfiltrateBank, SetNextMission, InfiltrateBank_SetNextMissionPy)

/* levels/Commercial.c */
PORT_DECLL(commercial, Map, InitStatus) /* 1 */
PORT_DECLL(commercial, Map, Init) /* 1 */
PORT_DECLL(commercial, MapRace, InitStatus) /* 1 */
PORT_DECLL(commercial, MapRace, Init) /* 1 */
PORT_DECLL(commercial, Mission_DestroyMickus, GetFuncEnemiesProfile) /* 0 */
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
PORT_DECLL(commercial, Mission_DestroyStupidus, Init) /* 1 */
PORT_DECLL(commercial, Mission_DestroyStupidus, CreateStupidus) /* 0 */
PORT_DECLL(commercial, Mission_DestroyStupidus, OnStupidusDeath) /* 2 */
PORT_DECLL(commercial, Mission_DestroyStupidus, OnStupidusDamage) /* 3 */
PORT_DECLL(commercial, Mission_DestroyStupidus, DropAmplifier) /* 1 */
PORT_DECLL(commercial, Mission_DestroyStupidus, TakeAmplifier) /* 2 */
PORT_DECLL(commercial, Mission_DestroyStupidus, BertoFound) /* 0 */
PORT_DECLL(commercial, Mission_DestroyStupidus, SetNextMission) /* 0 */
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
	PORT_DEFL(commercial, Map, InitStatus, CmMap_InitStatusA) \
	PORT_DEFL(commercial, Map, Init, CmMap_InitA) \
	PORT_DEFL(commercial, MapRace, InitStatus, MapRace_PassA) \
	PORT_DEFL(commercial, MapRace, Init, MapRace_PassA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, GetFuncEnemiesProfile, Mickus_GetFuncEnemiesProfileA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, Init, Mickus_InitA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, CreateTransports, Mickus_CreateTransportsA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, InitMission, Mickus_InitMissionA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, StopMission, Mickus_StopMissionA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, OnMickusDamage, Mickus_OnMickusDamageA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, ScanShip, Mickus_ScanShipA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, CheckContinueScanning, Mickus_CheckContinueScanningA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, ResetScan, Mickus_ResetScanA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, ResetScan2, Mickus_ResetScan2A) \
	PORT_DEFL(commercial, Mission_DestroyMickus, OnFinishScan, Mickus_OnFinishScanA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, MickusIdentified, Mickus_MickusIdentifiedA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, ReassignTarget, Mickus_ReassignTargetA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, TraitoriusFuncEnemiesCom, Mickus_TraitoriusFuncEnemiesComA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, CheckMickusVisibility, Mickus_CheckMickusVisibilityA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, CreateFuncEnemies, Mickus_CreateFuncEnemiesA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, OnFuncEnemiesDeath, Mickus_OnFuncEnemiesDeathA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, OnTransportsDeath, Mickus_OnTransportsDeathA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, DropMoney, Mickus_DropMoneyA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, TakeMoney, Mickus_TakeMoneyA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, SetNextMission, Mickus_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_DestroyMickus, MissionScorer, Mickus_MissionScorerA) \
	PORT_DEFL(commercial, Mission_ProtectGus, CreateLonelyMercProfile, Gus_CreateLonelyMercProfileA) \
	PORT_DEFL(commercial, Mission_ProtectGus, CreateBishopGusProfile, Gus_CreateBishopGusProfileA) \
	PORT_DEFL(commercial, Mission_ProtectGus, Init, Gus_InitA) \
	PORT_DEFL(commercial, Mission_ProtectGus, InitMission, Gus_InitMissionA) \
	PORT_DEFL(commercial, Mission_ProtectGus, AttackEachOther, Gus_AttackEachOtherA) \
	PORT_DEFL(commercial, Mission_ProtectGus, CreateLonelyMerc, Gus_CreateLonelyMercA) \
	PORT_DEFL(commercial, Mission_ProtectGus, CreateBishopGus, Gus_CreateBishopGusA) \
	PORT_DEFL(commercial, Mission_ProtectGus, OnLonelyMercDeath, Gus_OnLonelyMercDeathA) \
	PORT_DEFL(commercial, Mission_ProtectGus, OnBishopGusDeath, Gus_OnBishopGusDeathA) \
	PORT_DEFL(commercial, Mission_ProtectGus, PreCallDeepThroat, Gus_PreCallDeepThroatA) \
	PORT_DEFL(commercial, Mission_ProtectGus, SetPhoneCab, Gus_SetPhoneCabA) \
	PORT_DEFL(commercial, Mission_ProtectGus, UsePhoneCab, Gus_UsePhoneCabA) \
	PORT_DEFL(commercial, Mission_ProtectGus, SetNextMission, Gus_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_ProtectGus, MissionScorer, Gus_MissionScorerA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, GetStupidusProfile, Stupidus_GetStupidusProfileA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, Init, Stupidus_InitA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, CreateStupidus, Stupidus_CreateStupidusA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, OnStupidusDeath, Stupidus_OnStupidusDeathA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, OnStupidusDamage, Stupidus_OnStupidusDamageA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, DropAmplifier, Stupidus_DropAmplifierA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, TakeAmplifier, Stupidus_TakeAmplifierA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, BertoFound, Stupidus_BertoFoundA) \
	PORT_DEFL(commercial, Mission_DestroyStupidus, SetNextMission, Stupidus_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_Win1stRace, Init, Race_InitA) \
	PORT_DEFL(commercial, Mission_Win1stRace, InitTimeRace, Race_InitTimeRaceA) \
	PORT_DEFL(commercial, Mission_Win1stRace, OnRaceStart, Race_OnRaceStartA) \
	PORT_DEFL(commercial, Mission_Win1stRace, RaceGo, Race_RaceGoA) \
	PORT_DEFL(commercial, Mission_Win1stRace, OnCheckPoint, Race_OnCheckPointA) \
	PORT_DEFL(commercial, Mission_Win1stRace, OnAlarmOn, Race_OnAlarmOnA) \
	PORT_DEFL(commercial, Mission_Win1stRace, OnAlarmOff, Race_OnAlarmOffA) \
	PORT_DEFL(commercial, Mission_Win1stRace, TimeExpired, Race_TimeExpiredA) \
	PORT_DEFL(commercial, Mission_Win1stRace, PreSetNextMission, Race_PreSetNextMissionA) \
	PORT_DEFL(commercial, Mission_Win1stRace, SetNextMission, Race_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_Win1stRace, MissionScorer, Race_MissionScorerA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, Init, Deep_InitA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, AfterDeepThroatMessageShown, Deep_AfterDeepThroatMessageShownA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, AfterDeepThroatMessageShown2, Deep_AfterDeepThroatMessageShown2A) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, RestoreDeepThroatMessages, Deep_RestoreDeepThroatMessagesA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, CancelDeepThroatMessages, Deep_CancelDeepThroatMessagesA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, SetPhoneCab, Deep_SetPhoneCabA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, UsePhoneCab, Deep_UsePhoneCabA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, SetNextMission, Deep_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_FirstMeetDeepThroat, CGCalls, Deep_CGCallsA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, GetFunctionariesEnemiesProfile, Data_GetFunctionariesEnemiesProfileA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, Init, Data_InitA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, InitMission, Data_InitMissionA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, StopMission, Data_StopMissionA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, CreateWave, Data_CreateWaveA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, CreateFunctionary, Data_CreateFunctionaryA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, ReinsertFunctionaryInWave, Data_ReinsertFunctionaryInWaveA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, OnFunctionaryDeath, Data_OnFunctionaryDeathA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, OnFunctionaryDamage, Data_OnFunctionaryDamageA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, DropData, Data_DropDataA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, CheckAITakesData, Data_CheckAITakesDataA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, CheckAI, Data_CheckAIA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, TakeData, Data_TakeDataA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, CallBoss, Data_CallBossA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, UsePhoneCab, Data_UsePhoneCabA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, PreGetData, Data_PreGetDataA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, GetData, Data_GetDataA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, PreCallDeepThroat, Data_PreCallDeepThroatA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, PreCallDeepThroat2, Data_PreCallDeepThroat2A) \
	PORT_DEFL(commercial, Mission_GetDataPart3, CallDeepThroat, Data_CallDeepThroatA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, UsePhoneCab2, Data_UsePhoneCab2A) \
	PORT_DEFL(commercial, Mission_GetDataPart3, SetNextMission, Data_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_GetDataPart3, MissionScorer, Data_MissionScorerA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, GetCrazyGamblerProfile, Gambler_GetCrazyGamblerProfileA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, Init, Gambler_InitA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, LaunchFirstTaunt, Gambler_LaunchFirstTauntA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, LaunchTaunts, Gambler_LaunchTauntsA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, RefreshScorer, Gambler_RefreshScorerA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, CreateCrazyGambler, Gambler_CreateCrazyGamblerA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, CreateCrazyGamblers, Gambler_CreateCrazyGamblersA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, OnCrazyGamblerDeath, Gambler_OnCrazyGamblerDeathA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, AfterDeepThroatMessage, Gambler_AfterDeepThroatMessageA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, DropBossMatrix, Gambler_DropBossMatrixA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, TakeMatrix, Gambler_TakeMatrixA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, SetNextMission, Gambler_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_DefeatCrazyGambler3, MissionScorer, Gambler_MissionScorerA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, GetBishopsEnemiesProfile, Porkins_GetBishopsEnemiesProfileA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, InitStatus, Porkins_InitStatusA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, Init, Porkins_InitA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, InitMission, Porkins_InitMissionA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, CreateMayorTransport, Porkins_CreateMayorTransportA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, CreatePorkinsTransport, Porkins_CreatePorkinsTransportA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, AttackMayorTransport, Porkins_AttackMayorTransportA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, OnPorkinsTransportDeath, Porkins_OnPorkinsTransportDeathA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, OnMayorTransportDeath, Porkins_OnMayorTransportDeathA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, CheckBishopWithoutTarget, Porkins_CheckBishopWithoutTargetA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, OnBishopDeath, Porkins_OnBishopDeathA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, OnBishopDamage, Porkins_OnBishopDamageA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, GetMayorReward, Porkins_GetMayorRewardA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, PreCallDeepThroat, Porkins_PreCallDeepThroatA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, CallDeepThroat, Porkins_CallDeepThroatA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, UsePhoneCab, Porkins_UsePhoneCabA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, SetNextMission, Porkins_SetNextMissionA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, UpdateScorer, Porkins_UpdateScorerA) \
	PORT_DEFL(commercial, Mission_DestroyPorkinsTransports, MissionScorer, Porkins_MissionScorerA)

/* levels/DownTown.c */
PORT_DECLL(downtown, Map, InitStatus) /* 1 */
PORT_DECLL(downtown, Map, Init) /* 1 */
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
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, Init) /* 1 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, LaunchTaunts) /* 0 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, CreateCrazyGambler) /* 0 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, OnCrazyGamblerDeath) /* 2 */
PORT_DECLL(downtown, Mission_DefeatCrazyGambler1, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_FaceTheMayor, Init) /* 1 */
PORT_DECLL(downtown, Mission_FaceTheMayor, PreLaunchTaunt) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, LaunchMayorInitialTaunt) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, CreateCrazyMayor) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, MayorOnDeath) /* 2 */
PORT_DECLL(downtown, Mission_FaceTheMayor, SetPhoneCab) /* 0 */
PORT_DECLL(downtown, Mission_FaceTheMayor, UsePhoneCab) /* 4 */
PORT_DECLL(downtown, Mission_FaceTheMayor, EndOfMission) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, Init) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, OnVehicleCallback) /* 2 */
PORT_DECLL(downtown, Mission_Massacre, MissionScorer) /* 1 */
PORT_DECLL(downtown, Mission_Massacre, TimeExpired) /* 0 */
PORT_DECLL(downtown, Mission_Massacre, SetNextMission) /* 0 */
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
PORT_DECLL(downtown, Mission_MultiplusRace, InitStatus) /* 1 */
PORT_DECLL(downtown, Mission_MultiplusRace, Init) /* 1 */
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
PORT_DECLL(downtown, Mission_ProtectMayorTransport, RestoreTransport) /* 1 */
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
	PORT_DEFL(downtown, Map, InitStatus, DownTown_InitStatusPy) \
	PORT_DEFL(downtown, Map, Init, DownTown_InitPy) \
	PORT_DEFL(downtown, Mission_FirstContactPress, Init, Press_InitPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, InitStatus, Berto_InitStatusPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, StartMission, Berto_StartMissionPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, DeepThroatMessage, Berto_DeepThroatMessagePy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, CheckBertoDeath, Berto_CheckBertoDeathPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, OnBertoFound, Berto_OnBertoFoundPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, DropMatrix, Berto_DropMatrixPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, TakeMatrix, Berto_TakeMatrixPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, GetBerto, Berto_GetBertoPy) \
	PORT_DEFL(downtown, Mission_BertoMassacre, MissionScorer, Berto_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_DefeatCrazyGambler1, Init, Gambler_InitPy) \
	PORT_DEFL(downtown, Mission_DefeatCrazyGambler1, LaunchTaunts, Gambler_LaunchTauntsPy) \
	PORT_DEFL(downtown, Mission_DefeatCrazyGambler1, CreateCrazyGambler, Gambler_CreateCrazyGamblerPy) \
	PORT_DEFL(downtown, Mission_DefeatCrazyGambler1, OnCrazyGamblerDeath, Gambler_OnCrazyGamblerDeathPy) \
	PORT_DEFL(downtown, Mission_DefeatCrazyGambler1, MissionScorer, Gambler_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, Init, Mayor_InitPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, PreLaunchTaunt, Mayor_PreLaunchTauntPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, LaunchMayorInitialTaunt, Mayor_LaunchMayorInitialTauntPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, CreateCrazyMayor, Mayor_CreateCrazyMayorPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, MayorOnDeath, Mayor_MayorOnDeathPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, SetPhoneCab, Mayor_SetPhoneCabPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, UsePhoneCab, Mayor_UsePhoneCabPy) \
	PORT_DEFL(downtown, Mission_FaceTheMayor, EndOfMission, Mayor_EndOfMissionPy) \
	PORT_DEFL(downtown, Mission_Massacre, Init, Massacre_InitPy) \
	PORT_DEFL(downtown, Mission_Massacre, OnVehicleCallback, Massacre_OnVehicleCallbackPy) \
	PORT_DEFL(downtown, Mission_Massacre, MissionScorer, Massacre_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_Massacre, TimeExpired, Massacre_TimeExpiredPy) \
	PORT_DEFL(downtown, Mission_Massacre, SetNextMission, Massacre_SetNextMissionPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, Init, Hump_InitPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, CallHumphrey, Hump_CallHumphreyPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, UsePhoneCab, Hump_UsePhoneCabPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, CreateWaypoint, Hump_CreateWaypointPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, OnStartDistraction, Hump_OnStartDistractionPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, EnableMissionWaypoint, Hump_EnableMissionWaypointPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, StartCameraTravelling, Hump_StartCameraTravellingPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, AbortCameraTravelling, Hump_AbortCameraTravellingPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, CreateHumphrey, Hump_CreateHumphreyPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, LaunchMission, Hump_LaunchMissionPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, AssignNewHumphreyTarget, Hump_AssignNewHumphreyTargetPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, OnHumphreyDeath, Hump_OnHumphreyDeathPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, OnHumphreyDamage, Hump_OnHumphreyDamagePy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, OnGeneralDamage, Hump_OnGeneralDamagePy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, OnDeath, Hump_OnDeathPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, StopMission, Hump_StopMissionPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, CallDeepThroat, Hump_CallDeepThroatPy) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, UsePhoneCab2, Hump_UsePhoneCab2Py) \
	PORT_DEFL(downtown, Mission_DistractHumphrey, MissionScorer, Hump_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, Init, Data_InitPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, InitMission, Data_InitMissionPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, CreateTransport, Data_CreateTransportPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, CreateFunctionary, Data_CreateFunctionaryPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, AddTransportToList, Data_AddTransportToListPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, SpawnFunctionaries, Data_SpawnFunctionariesPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, OnFunctionaryDeath, Data_OnFunctionaryDeathPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, OnTransportsDeath, Data_OnTransportsDeathPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, OnTransportsDamage, Data_OnTransportsDamagePy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, SpawnData, Data_SpawnDataPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, DropData, Data_DropDataPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, DelData, Data_DelDataPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, TakeData, Data_TakeDataPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, SetNextMission, Data_SetNextMissionPy) \
	PORT_DEFL(downtown, Mission_GetDataPart1, MissionScorer, Data_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, InitStatus, Bet_InitStatusPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, ShowSystemMessage, Bet_ShowSystemMessagePy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, Init, Bet_InitPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, ShowBoostMessage, Bet_ShowBoostMessagePy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, ClosePressHangar, Bet_ClosePressHangarPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, DelRacers, Bet_DelRacersPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, CallCrazyGambler, Bet_CallCrazyGamblerPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, UsePhoneCab, Bet_UsePhoneCabPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, PreGivePlans2, Bet_PreGivePlans2Py) \
	PORT_DEFL(downtown, Mission_HumphreyRace, GivePlans, Bet_GivePlansPy) \
	PORT_DEFL(downtown, Mission_HumphreyRace, SetNextMission, Bet_SetNextMissionPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, InitStatus, Plus_InitStatusPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, Init, Plus_InitPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, EndOfRace, Plus_EndOfRacePy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, SetPhoneCab, Plus_SetPhoneCabPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, UsePhoneCab, Plus_UsePhoneCabPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, InitRace, Plus_InitRacePy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, AfterInitRaceTaunt, Plus_AfterInitRaceTauntPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, InitKill, Plus_InitKillPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, SpawnStupidus, Plus_SpawnStupidusPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, SpawnStupidusFriends, Plus_SpawnStupidusFriendsPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, StupidusFriendOnDeath, Plus_StupidusFriendOnDeathPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, RestoreEnemies, Plus_RestoreEnemiesPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, StupidusOnDeath, Plus_StupidusOnDeathPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, MissionScorer, Plus_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, SetNextMission, Plus_SetNextMissionPy) \
	PORT_DEFL(downtown, Mission_MultiplusRace, CGCalls, Plus_CGCallsPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, GetMercEnemiesProfile, Escort_GetMercEnemiesProfilePy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, InitStatus, Escort_InitStatusPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, Init, Escort_InitPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, MissionInit, Escort_MissionInitPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, OnMissionStart, Escort_OnMissionStartPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, OnMissionStart2, Escort_OnMissionStart2Py) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, TransGoto, Escort_TransGotoPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, CreateBadGuy, Escort_CreateBadGuyPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, AppearsBadGuys, Escort_AppearsBadGuysPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, AppearsBadGuys2, Escort_AppearsBadGuys2Py) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, NextBadGuy, Escort_NextBadGuyPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, BackToTheTransport, Escort_BackToTheTransportPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, BackToPlayer, Escort_BackToPlayerPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, MadGuyDeath, Escort_MadGuyDeathPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, PreLaunchTraitorius, Escort_PreLaunchTraitoriusPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, SetNextMission, Escort_SetNextMissionPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, MadGuyDamage, Escort_MadGuyDamagePy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, MercTauntMessager, Escort_MercTauntMessagerPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, OnTravelComplete, Escort_OnTravelCompletePy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, ActualizeScorer, Escort_ActualizeScorerPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, MissionScorer, Escort_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, KillThem, Escort_KillThemPy) \
	PORT_DEFL(downtown, Mission_ProtectMayorTransport, RestoreTransport, Escort_RestoreTransportPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, Init, Steal_InitPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, CreateMissionShip, Steal_CreateMissionShipPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, TestForMissionShip, Steal_TestForMissionShipPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, SetPlatformTarget, Steal_SetPlatformTargetPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, TestForCorrectPlatform, Steal_TestForCorrectPlatformPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, UseParkedMayorShip, Steal_UseParkedMayorShipPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, UsePhoneCab, Steal_UsePhoneCabPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, SetNextMission, Steal_SetNextMissionPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, CGCalls, Steal_CGCallsPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, StartMission, Steal_StartMissionPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, ContinueMission, Steal_ContinueMissionPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, SetPoliceListTargetStatus, Steal_SetPoliceListTargetStatusPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, ComeBack, Steal_ComeBackPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, MissionScorer, Steal_MissionScorerPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, OnPoliceDeath, Steal_OnPoliceDeathPy) \
	PORT_DEFL(downtown, Mission_StealMayorShip, ParkMayorShip, Steal_ParkMayorShipPy)

/* levels/Fake.c */
PORT_DECLL(fake, Map, Init) /* 1 */
PORT_DECLL(fake, Map, CreateFakeBertoShip) /* 0 */
PORT_DECLL(fake, Map, ActivateBertoEngines) /* 3 */
PORT_DECLL(fake, Map, LoadFake2Scene) /* 0 */
#define PORTS_levels_Fake \
	PORT_DEFL(fake, Map, Init, Fk_InitPy) \
	PORT_DEFL(fake, Map, CreateFakeBertoShip, Fk_CreateFakeBertoShipPy) \
	PORT_DEFL(fake, Map, ActivateBertoEngines, Fk_ActivateBertoEnginesPy) \
	PORT_DEFL(fake, Map, LoadFake2Scene, Fk_LoadFake2ScenePy)

/* levels/GDB.c */
PORT_DECLL(gdb, Map, InitStatus) /* 1 */
PORT_DECLL(gdb, Map, Init) /* 1 */
PORT_DECLL(gdb, Map, InitPoliceBishops) /* 0 */
PORT_DECLL(gdb, Map, EndOfAppears) /* 1 */
PORT_DECLL(gdb, Map, GiveLife) /* 0 */
PORT_DECLL(gdb, Map, NextWave) /* 0 */
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
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, InitMission) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, BishopsAttackPlayer) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, CreateBishop) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, CreateBishops) /* 1 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, OnBishopsDeath) /* 2 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix, PreSetNextMission) /* 4 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, InitMission) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, BishopsAttackPlayer) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, CreateBishop) /* 0 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, CreateBishops) /* 1 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, OnBishopsDeath) /* 2 */
PORT_DECLL(gdb, Mission_ReinsertOwnMatrix2, PreSetNextMission) /* 4 */
#define PORTS_levels_GDB \
	PORT_DEFL(gdb, Map, InitStatus, GDB_InitStatusPy) \
	PORT_DEFL(gdb, Map, Init, GDB_InitPy) \
	PORT_DEFL(gdb, Map, InitPoliceBishops, GDB_InitPoliceBishopsPy) \
	PORT_DEFL(gdb, Map, EndOfAppears, GDB_EndOfAppearsPy) \
	PORT_DEFL(gdb, Map, GiveLife, GDB_GiveLifePy) \
	PORT_DEFL(gdb, Map, NextWave, GDB_NextWavePy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, Init, Into_InitPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, CreateBishop, Into_CreateBishopPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, CreateBishops, Into_CreateBishopsPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, InitMission, Into_InitMissionPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, ScanShip, Into_ScanShipPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, CheckContinueScanning, Into_CheckContinueScanningPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, ResetScan, Into_ResetScanPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, OnFinishScan, Into_OnFinishScanPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, OnBishopsDeath, Into_OnBishopsDeathPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, OnBishopsDamage, Into_OnBishopsDamagePy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, DropMatrix, Into_DropMatrixPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, ActivateMatrix, Into_ActivateMatrixPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, TakeMatrix, Into_TakeMatrixPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, RestoreMatrix, Into_RestoreMatrixPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, LaunchRestoreMatrixAnim, Into_LaunchRestoreMatrixAnimPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, RelaxBishops, Into_RelaxBishopsPy) \
	PORT_DEFL(gdb, Mission_IntoTheGDB, MissionScorer, Into_MissionScorerPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix, InitMission, Re1_InitMissionPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix, BishopsAttackPlayer, Re1_BishopsAttackPlayerPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix, CreateBishop, Re1_CreateBishopPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix, CreateBishops, Re1_CreateBishopsPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix, OnBishopsDeath, Re1_OnBishopsDeathPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix, PreSetNextMission, Re1_PreSetNextMissionPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix2, InitMission, Re2_InitMissionPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix2, BishopsAttackPlayer, Re2_BishopsAttackPlayerPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix2, CreateBishop, Re2_CreateBishopPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix2, CreateBishops, Re2_CreateBishopsPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix2, OnBishopsDeath, Re2_OnBishopsDeathPy) \
	PORT_DEFL(gdb, Mission_ReinsertOwnMatrix2, PreSetNextMission, Re2_PreSetNextMissionPy)

/* levels/GamblinDen.c */
PORT_DECLL(gamblinden, Map, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Map, AfterBrowseNPC) /* 0 */
PORT_DECLL(gamblinden, Map, CreateFixedMercInit) /* 1 */
PORT_DECLL(gamblinden, Map, CreateLonelyMerc) /* 1 */
PORT_DECLL(gamblinden, Map, Init) /* 1 */
PORT_DECLL(gamblinden, Map, LonelyMercSpeech) /* 4 */
PORT_DECLL(gamblinden, Map, CharUsrAct) /* 2 */
PORT_DECLL(gamblinden, Map, CharUsrDance) /* 2 */
PORT_DECLL(gamblinden, Map, CharDance) /* 1 */
PORT_DECLL(gamblinden, Map, MercenarySpeech) /* 4 */
PORT_DECLL(gamblinden, Map, StartContract) /* 1 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, InitStatus) /* 1 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, Init) /* 1 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, CrazyGamblerSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_BeforeFinalFight, MercenariesSpeech) /* 4 */
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
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerFirstSpeech) /* 4 */
PORT_DECLL(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerSecondSpeech) /* 4 */
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
	PORT_DEFL(gamblinden, Map, InitStatus, Den_InitStatus) \
	PORT_DEFL(gamblinden, Map, AfterBrowseNPC, Den_AfterBrowseNPC) \
	PORT_DEFL(gamblinden, Map, CreateFixedMercInit, Den_CreateFixedMercInit) \
	PORT_DEFL(gamblinden, Map, CreateLonelyMerc, Den_CreateLonelyMerc) \
	PORT_DEFL(gamblinden, Map, Init, Den_Init) \
	PORT_DEFL(gamblinden, Map, LonelyMercSpeech, Den_LonelyMercSpeech) \
	PORT_DEFL(gamblinden, Map, CharUsrAct, Den_CharUsrAct) \
	PORT_DEFL(gamblinden, Map, CharUsrDance, Den_CharUsrDance) \
	PORT_DEFL(gamblinden, Map, CharDance, Den_CharDance) \
	PORT_DEFL(gamblinden, Map, MercenarySpeech, Den_MercenarySpeech) \
	PORT_DEFL(gamblinden, Map, StartContract, Den_StartContract) \
	PORT_DEFL(gamblinden, Mission_BeforeFinalFight, InitStatus, Before_InitStatus) \
	PORT_DEFL(gamblinden, Mission_BeforeFinalFight, Init, Before_Init) \
	PORT_DEFL(gamblinden, Mission_BeforeFinalFight, CrazyGamblerSpeech, Before_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_BeforeFinalFight, MercenariesSpeech, Before_MercenariesSpeech) \
	PORT_DEFL(gamblinden, Mission_BeforeFinalFight, LoadScene, Before_LoadScene) \
	PORT_DEFL(gamblinden, Mission_AfterFinalFight, InitStatus, After_InitStatus) \
	PORT_DEFL(gamblinden, Mission_AfterFinalFight, Init, After_Init) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, InitStatus, Igor_InitStatus) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, Init, Igor_Init) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, TalkToIgor, Igor_TalkToIgor) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, IgorSpeech, Igor_IgorSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, TalkToCrazyGambler, Igor_TalkToCrazyGambler) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, CrazyGamblerSpeech, Igor_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeIgorCombat, LaunchIgorCombat, Igor_LaunchIgorCombat) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, InitStatus, Ctf_InitStatus) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, Init, Ctf_Init) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, CreateTraitoriusInit, Ctf_CreateTraitoriusInit) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, TraitoriusSpeech, Ctf_TraitoriusSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, TalkToCrazyGambler, Ctf_TalkToCrazyGambler) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, CrazyGamblerSpeech, Ctf_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCTF, LaunchBankersCTF, Ctf_LaunchBankersCTF) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, InitStatus, Bankers_InitStatus) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, Init, Bankers_Init) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, LaunchIgorDefeatedSpeech, Bankers_LaunchIgorDefeatedSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, LaunchDeepThroatMessage, Bankers_LaunchDeepThroatMessage) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, TalkToDeepThroat, Bankers_TalkToDeepThroat) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, DeepThroatSpeech, Bankers_DeepThroatSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, TalkToPoliceBoss, Bankers_TalkToPoliceBoss) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, PoliceBossSpeech, Bankers_PoliceBossSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, PoliceBossSpeech2, Bankers_PoliceBossSpeech2) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, TalkToCrazyGambler, Bankers_TalkToCrazyGambler) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, CrazyGamblerSpeech, Bankers_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeBankersCombat, LaunchBankersCombat, Bankers_LaunchBankersCombat) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, InitStatus, First_InitStatus) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, Init, First_Init) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, TalkToCrazyGambler, First_TalkToCrazyGambler) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, CrazyGamblerSpeech, First_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, TalkToIgor, First_TalkToIgor) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, IgorSpeech, First_IgorSpeech) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, TalkToMercenaries, First_TalkToMercenaries) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, MercenaryASpeech, First_MercenaryASpeech) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, MercenaryCSpeech, First_MercenaryCSpeech) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, MercenaryBSpeech, First_MercenaryBSpeech) \
	PORT_DEFL(gamblinden, Mission_IgorFirstContact, SetNextMission, First_SetNextMission) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, InitStatus, Go_InitStatus) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, AfterBrowseNPC, Go_AfterBrowseNPC) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, CreateMercenaryInit, Go_CreateMercenaryInit) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, MercenarySpeech, Go_MercenarySpeech) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, HumanSpeech, Go_HumanSpeech) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, CrazyGamblerSpeech, Go_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, Init, Go_Init) \
	PORT_DEFL(gamblinden, Mission_GoToGamblinDen, LoadScene, Go_LoadScene) \
	PORT_DEFL(gamblinden, Mission_TalkToTraitoriusAfterCTF, InitStatus, Traitor_InitStatus) \
	PORT_DEFL(gamblinden, Mission_TalkToTraitoriusAfterCTF, Init, Traitor_Init) \
	PORT_DEFL(gamblinden, Mission_TalkToTraitoriusAfterCTF, CreateTraitoriusInit, Traitor_CreateTraitoriusInit) \
	PORT_DEFL(gamblinden, Mission_TalkToTraitoriusAfterCTF, LaunchTraitoriusSpeech, Traitor_LaunchTraitoriusSpeech) \
	PORT_DEFL(gamblinden, Mission_TalkToTraitoriusAfterCTF, TraitoriusSpeech, Traitor_TraitoriusSpeech) \
	PORT_DEFL(gamblinden, Mission_TalkToTraitoriusAfterCTF, SetNextMission, Traitor_SetNextMission) \
	PORT_DEFL(gamblinden, Mission_BackFromMortalRace, InitStatus, Back_InitStatus) \
	PORT_DEFL(gamblinden, Mission_BackFromMortalRace, Init, Back_Init) \
	PORT_DEFL(gamblinden, Mission_BackFromMortalRace, TalkToDeepThroat, Back_TalkToDeepThroat) \
	PORT_DEFL(gamblinden, Mission_BackFromMortalRace, AfterDeepThroatMessageShown, Back_AfterDeepThroatMessageShown) \
	PORT_DEFL(gamblinden, Mission_BackFromMortalRace, DeepThroatSpeech, Back_DeepThroatSpeech) \
	PORT_DEFL(gamblinden, Mission_BackFromMortalRace, SetNextMission, Back_SetNextMission) \
	PORT_DEFL(gamblinden, Mission_CrazyGamblerFirstContact, InitStatus, Meet_InitStatus) \
	PORT_DEFL(gamblinden, Mission_CrazyGamblerFirstContact, Init, Meet_Init) \
	PORT_DEFL(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerFirstSpeech, Meet_CrazyGamblerFirstSpeech) \
	PORT_DEFL(gamblinden, Mission_CrazyGamblerFirstContact, CrazyGamblerSecondSpeech, Meet_CrazyGamblerSecondSpeech) \
	PORT_DEFL(gamblinden, Mission_CrazyGamblerFirstContact, SetNextMission, Meet_SetNextMission) \
	PORT_DEFL(gamblinden, Mission_CrazyGamblerFirstContact, LoadScene, Meet_LoadScene) \
	PORT_DEFL(gamblinden, Mission_ArrangeMortalRace, InitStatus, Mortal_InitStatus) \
	PORT_DEFL(gamblinden, Mission_ArrangeMortalRace, Init, Mortal_Init) \
	PORT_DEFL(gamblinden, Mission_ArrangeMortalRace, CrazyGamblerSpeech, Mortal_CrazyGamblerSpeech) \
	PORT_DEFL(gamblinden, Mission_ArrangeMortalRace, LaunchMortalRace, Mortal_LaunchMortalRace) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, InitStatus, Stooges_InitStatus) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, Init, Stooges_Init) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, AfterBrowseNPC, Stooges_AfterBrowseNPC) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, CreateMercenaryAInit, Stooges_CreateMercenaryAInit) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, CreateMercenaryBInit, Stooges_CreateMercenaryBInit) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, CreateMercenaryCInit, Stooges_CreateMercenaryCInit) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, MercenaryASpeech, Stooges_MercenaryASpeech) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, MercenaryCSpeech, Stooges_MercenaryCSpeech) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, MercenaryBSpeech, Stooges_MercenaryBSpeech) \
	PORT_DEFL(gamblinden, Mission_ThreeStooges, SetNextMission, Stooges_SetNextMission)

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
	PORT_DEFL(junkyard, Map, InitStatus, JunkyardMap_InitStatusPy) \
	PORT_DEFL(junkyard, Map, Init, JunkyardMap_InitPy) \
	PORT_DEFL(junkyard, MapShipEdit, EnterEditShip, MapShipEdit_EnterEditShipPy) \
	PORT_DEFL(junkyard, MapShipEdit, SetInteract, MapShipEdit_SetInteractPy) \
	PORT_DEFL(junkyard, MapShipEdit, OnEndOfShipEdit, MapShipEdit_OnEndOfShipEditPy) \
	PORT_DEFL(junkyard, MapShipEdit, CanUseElevator, MapShipEdit_CanUseElevatorPy) \
	PORT_DEFL(junkyard, MapShipEdit, UseElevator, MapShipEdit_UseElevatorPy) \
	PORT_DEFL(junkyard, MapShipEdit, Init, MapShipEdit_InitPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, InitStatus, Rusty_InitStatusPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, Init, Rusty_InitPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, CreateFakeBertoShip, Rusty_CreateFakeBertoShipPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, ShowExtras, Rusty_ShowExtrasPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, StartInitialScene, Rusty_StartInitialScenePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, LoadMecanicFinalScene, Rusty_LoadMecanicFinalScenePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, FXMecanicFinalSputnik, Rusty_FXMecanicFinalSputnikPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, BertoGoHome, Rusty_BertoGoHomePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, BackDownBerto, Rusty_BackDownBertoPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, EndOfBertoBye, Rusty_EndOfBertoByePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, StartInitBuildShip, Rusty_StartInitBuildShipPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, InitBuildShip, Rusty_InitBuildShipPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, EnterEditShipFirstTime, Rusty_EnterEditShipFirstTimePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, NoTutorialEnterEditShip, Rusty_NoTutorialEnterEditShipPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, OnEndOfShipEditFirstTime, Rusty_OnEndOfShipEditFirstTimePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, UseElevatorFirstTime, Rusty_UseElevatorFirstTimePy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, EndOfMission, Rusty_EndOfMissionPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, EnterEditShipTutorial, Rusty_EnterEditShipTutorialPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, TutorialMsgInit, Rusty_TutorialMsgInitPy) \
	PORT_DEFL(junkyard, Mission_RustyFirstContact, EditShipTutorial_Step, Rusty_EditShipTutorial_StepPy)

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
	PORT_DEFL(commercial, Mission_DestroyCommSats2, InitStatus, Probes1_InitStatus) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, Init, Probes1_Init) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, IsValidPoint, Probes1_IsValidPoint) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, CreateProbes, Probes1_CreateProbes) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, OnProbeDeath, Probes1_OnProbeDeath) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, DestroyShipsInList, Probes1_DestroyShipsInList) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, DestroyEnemyShips, Probes1_DestroyEnemyShips) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, MissionScorer, Probes1_MissionScorer) \
	PORT_DEFL(commercial, Mission_DestroyCommSats2, SetNextMission, Probes1_SetNextMission) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, InitStatus, Probes2_InitStatus) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, Init, Probes2_Init) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, IsValidPoint, Probes2_IsValidPoint) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, CreateProbes, Probes2_CreateProbes) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, OnProbeDeath, Probes2_OnProbeDeath) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, DestroyShipsInList, Probes2_DestroyShipsInList) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, DestroyEnemyShips, Probes2_DestroyEnemyShips) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, MissionScorer, Probes2_MissionScorer) \
	PORT_DEFL(downtown, Mission_DestroyCommSats3, SetNextMission, Probes2_SetNextMission) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, InitStatus, Probes0_InitStatus) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, Init, Probes0_Init) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, IsValidPoint, Probes0_IsValidPoint) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, CreateProbes, Probes0_CreateProbes) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, OnProbeDeath, Probes0_OnProbeDeath) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, DestroyShipsInList, Probes0_DestroyShipsInList) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, DestroyEnemyShips, Probes0_DestroyEnemyShips) \
	PORT_DEFL(outskirts, Mission_DestroyCommSats1, MissionScorer, Probes0_MissionScorer)

/* levels/Menu.c */
PORT_DECLL(menu, Map, InitStatus) /* 1 */
PORT_DECLL(menu, Map, FadeOutCreditsEnd) /* 0 */
PORT_DECLL(menu, Map, FadeOutCredits) /* 0 */
PORT_DECLL(menu, Map, NextCredit) /* 0 */
PORT_DECLL(menu, Map, ShowCredits) /* 0 */
PORT_DECLL(menu, Map, SetMotionBlur) /* 0 */
PORT_DECLL(menu, Map, Init) /* 1 */
PORT_DECLL(menu, Map, drotorito) /* 0 */
#define PORTS_levels_Menu \
	PORT_DEFL(menu, Map, InitStatus, Menu_InitStatusPy) \
	PORT_DEFL(menu, Map, FadeOutCreditsEnd, Menu_FadeOutCreditsEndPy) \
	PORT_DEFL(menu, Map, FadeOutCredits, Menu_FadeOutCreditsPy) \
	PORT_DEFL(menu, Map, NextCredit, Menu_NextCreditPy) \
	PORT_DEFL(menu, Map, ShowCredits, Menu_ShowCreditsPy) \
	PORT_DEFL(menu, Map, SetMotionBlur, Menu_SetMotionBlurPy) \
	PORT_DEFL(menu, Map, Init, Menu_InitPy) \
	PORT_DEFL(menu, Map, drotorito, Menu_drotoritoPy)

/* levels/OM.c */
PORT_DECLL(om, SuperDeal_Sixth, SetVars) /* 0 */
PORT_DECLL(om, SuperDeal_Tenth, SetVars) /* 0 */
#define PORTS_levels_OM \
	PORT_DEFL(om, SuperDeal_Sixth, SetVars, Om_SixthSetVarsPy) \
	PORT_DEFL(om, SuperDeal_Tenth, SetVars, Om_TenthSetVarsPy)

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
PORT_DECLL(orbit, MapDecont, LoadFunctionaryScene) /* 0 */
PORT_DECLL(orbit, MapDecont, SetDFAnm) /* 3 */
PORT_DECLL(orbit, MapDecont, EndOfSceneFunctionary) /* 1 */
PORT_DECLL(orbit, MapDecont, CreateDownKeyPad) /* 0 */
PORT_DECLL(orbit, MapDecont, PlayFalling) /* 4 */
PORT_DECLL(orbit, MapDecont, JumpNextMission) /* 1 */
PORT_DECLL(orbit, MapDecont, PlayFunctionary) /* 4 */
PORT_DECLL(orbit, MapDecont, LoadFallingScene) /* 0 */
PORT_DECLL(orbit, Mission_IntroHuman, InitStatus) /* 1 */
PORT_DECLL(orbit, Mission_IntroHuman, Init) /* 1 */
PORT_DECLL(orbit, Mission_IntroHuman, LoadSceneA) /* 0 */
PORT_DECLL(orbit, Mission_IntroHuman, LoadSceneB) /* 0 */
PORT_DECLL(orbit, Mission_IntroHuman, LoadSceneC) /* 0 */
#define PORTS_levels_Orbit \
	PORT_DEFL(orbit, Map, InitStatus, Orbit_InitStatus) \
	PORT_DEFL(orbit, Map, Init, Orbit_Init) \
	PORT_DEFL(orbit, MapDecont, InitStatus, Decont_InitStatus) \
	PORT_DEFL(orbit, MapDecont, Decontamination, Decont_Decontamination) \
	PORT_DEFL(orbit, MapDecont, EndOfScene, Decont_EndOfScene) \
	PORT_DEFL(orbit, MapDecont, DecontaminatedWaypoint, Decont_DecontaminatedWaypoint) \
	PORT_DEFL(orbit, MapDecont, SetWaypoint, Decont_SetWaypoint) \
	PORT_DEFL(orbit, MapDecont, LoadDecontamination, Decont_LoadDecontamination) \
	PORT_DEFL(orbit, MapDecont, Init, Decont_Init) \
	PORT_DEFL(orbit, MapDecont, MsgOffVoice, Decont_MsgOffVoice) \
	PORT_DEFL(orbit, MapDecont, SetFirstInfoPointAsMission, Decont_SetFirstInfoPointAsMission) \
	PORT_DEFL(orbit, MapDecont, CantUseElevator, Decont_CantUseElevator) \
	PORT_DEFL(orbit, MapDecont, CloseGateKeeperDoor, Decont_CloseGateKeeperDoor) \
	PORT_DEFL(orbit, MapDecont, OpenGateKeeperDoor, Decont_OpenGateKeeperDoor) \
	PORT_DEFL(orbit, MapDecont, AfterBrowseNPC, Decont_AfterBrowseNPC) \
	PORT_DEFL(orbit, MapDecont, CreateFunctionaryInit, Decont_CreateFunctionaryInit) \
	PORT_DEFL(orbit, MapDecont, CreateGateKeeperInit, Decont_CreateGateKeeperInit) \
	PORT_DEFL(orbit, MapDecont, CreateSentinelInit, Decont_CreateSentinelInit) \
	PORT_DEFL(orbit, MapDecont, ShowSkipConvsMsg, Decont_ShowSkipConvsMsg) \
	PORT_DEFL(orbit, MapDecont, GateKeeperSpeech, Decont_GateKeeperSpeech) \
	PORT_DEFL(orbit, MapDecont, SentinelSpeech, Decont_SentinelSpeech) \
	PORT_DEFL(orbit, MapDecont, GaugeBlink, Decont_GaugeBlink) \
	PORT_DEFL(orbit, MapDecont, CheckShowNextHowto, Decont_CheckShowNextHowto) \
	PORT_DEFL(orbit, MapDecont, CreateBishopsLauncher, Decont_CreateBishopsLauncher) \
	PORT_DEFL(orbit, MapDecont, EndOfSceneBishops, Decont_EndOfSceneBishops) \
	PORT_DEFL(orbit, MapDecont, LoadBishopsScene, Decont_LoadBishopsScene) \
	PORT_DEFL(orbit, MapDecont, FXBishopsPray, Decont_FXBishopsPray) \
	PORT_DEFL(orbit, MapDecont, LoadFunctionaryScene, Decont_LoadFunctionaryScene) \
	PORT_DEFL(orbit, MapDecont, SetDFAnm, Decont_SetDFAnm) \
	PORT_DEFL(orbit, MapDecont, EndOfSceneFunctionary, Decont_EndOfSceneFunctionary) \
	PORT_DEFL(orbit, MapDecont, CreateDownKeyPad, Decont_CreateDownKeyPad) \
	PORT_DEFL(orbit, MapDecont, PlayFalling, Decont_PlayFalling) \
	PORT_DEFL(orbit, MapDecont, JumpNextMission, Decont_JumpNextMission) \
	PORT_DEFL(orbit, MapDecont, PlayFunctionary, Decont_PlayFunctionary) \
	PORT_DEFL(orbit, MapDecont, LoadFallingScene, Decont_LoadFallingScene) \
	PORT_DEFL(orbit, Mission_IntroHuman, InitStatus, Intro_InitStatus) \
	PORT_DEFL(orbit, Mission_IntroHuman, Init, Intro_Init) \
	PORT_DEFL(orbit, Mission_IntroHuman, LoadSceneA, Intro_LoadSceneA) \
	PORT_DEFL(orbit, Mission_IntroHuman, LoadSceneB, Intro_LoadSceneB) \
	PORT_DEFL(orbit, Mission_IntroHuman, LoadSceneC, Intro_LoadSceneC)

/* levels/Outskirts.c */
PORT_DECLL(outskirts, Map, InitStatus) /* 1 */
PORT_DECLL(outskirts, Map, Init) /* 1 */
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
PORT_DECLL(outskirts, Mission_DestroyHumphrey, CreateHumphrey) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, HumphreyAttacks) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, LaunchTaunt) /* 0 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, OnHumphreyDeath) /* 2 */
PORT_DECLL(outskirts, Mission_DestroyHumphrey, MissionScorer) /* 1 */
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
PORT_DECLL(outskirts, Mission_GoToOutskirts, Init) /* 1 */
PORT_DECLL(outskirts, Mission_GoToOutskirts, LaunchCrazyGamblerComunication) /* 1 */
PORT_DECLL(outskirts, Mission_GoToOutskirts, PlayCrazyGamblerComunication) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CreateLonelyMercProfiles) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, Init) /* 1 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CreateLonelyMercInScene) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, CheckTraitoriusDeath) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, EndOfScene) /* 0 */
PORT_DECLL(outskirts, Mission_IdentifyMerc, DelLonelyMerc) /* 0 */
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
PORT_DECLL(outskirts, Mission_KillRudolph, InitStatus) /* 1 */
PORT_DECLL(outskirts, Mission_KillRudolph, Init) /* 1 */
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
PORT_DECLL(outskirts, Mission_TransportKiller, Init) /* 1 */
PORT_DECLL(outskirts, Mission_TransportKiller, InitTransport) /* 0 */
PORT_DECLL(outskirts, Mission_TransportKiller, isValidDamage) /* 1 */
PORT_DECLL(outskirts, Mission_TransportKiller, TryToDeath) /* 2 */
PORT_DECLL(outskirts, Mission_TransportKiller, TakeDataPack) /* 2 */
PORT_DECLL(outskirts, Mission_TransportKiller, TryToDamage) /* 3 */
PORT_DECLL(outskirts, Mission_TransportKiller, InitCall) /* 0 */
PORT_DECLL(outskirts, Mission_TransportKiller, SetNextMission) /* 0 */
PORT_DECLL(outskirts, Mission_Win2ndRace, Init) /* 1 */
PORT_DECLL(outskirts, Mission_Win2ndRace, InitRace) /* 0 */
PORT_DECLL(outskirts, Mission_Win2ndRace, GetRaceWaypointIndex) /* 0 */
PORT_DECLL(outskirts, Mission_Win2ndRace, RacerDamaged) /* 3 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnRaceStart) /* 2 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnCheckPoint) /* 2 */
PORT_DECLL(outskirts, Mission_Win2ndRace, OnNearCheckPoint) /* 2 */
PORT_DECLL(outskirts, Mission_Win2ndRace, NameOponGoItem) /* 1 */
PORT_DECLL(outskirts, Mission_Win2ndRace, CancelMission) /* 0 */
#define PORTS_levels_Outskirts \
	PORT_DEFL(outskirts, Map, InitStatus, OsMap_InitStatusPy) \
	PORT_DEFL(outskirts, Map, Init, OsMap_InitPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, Init, Traitorius_InitPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, ReassignTarget, Traitorius_ReassignTargetPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, CreateTraitorius, Traitorius_CreateTraitoriusPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, OnTraitoriusDamage, Traitorius_OnTraitoriusDamagePy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, OnTraitoriusDeath, Traitorius_OnTraitoriusDeathPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, TraitoriusTakesLife, Traitorius_TraitoriusTakesLifePy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, TimeExpired, Traitorius_TimeExpiredPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, PreCallTraitorius, Traitorius_PreCallTraitoriusPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, CallTraitorius, Traitorius_CallTraitoriusPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, UsePhoneCab, Traitorius_UsePhoneCabPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, SetNextMission, Traitorius_SetNextMissionPy) \
	PORT_DEFL(outskirts, Mission_AttackTraitorius, MissionScorer, Traitorius_MissionScorerPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, InitStatus, Demo_InitStatusPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, ShowSystemMessage, Demo_ShowSystemMessagePy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, Init, Demo_InitPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, ShowBoostMessage, Demo_ShowBoostMessagePy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, ClosePressHangar, Demo_ClosePressHangarPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, DelRacers, Demo_DelRacersPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, OnPlayerWinsRace, Demo_OnPlayerWinsRacePy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, CallCrazyGambler, Demo_CallCrazyGamblerPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, ShowRustyMapsSystemMessage, Demo_ShowRustyMapsSystemMessagePy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, UsePhoneCab, Demo_UsePhoneCabPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, EnableMetro, Demo_EnableMetroPy) \
	PORT_DEFL(outskirts, Mission_Demo_HumphreyRace, OnMetroUsed, Demo_OnMetroUsedPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, InitStatus, Debtors_InitStatusPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, CreateWaypoint, Debtors_CreateWaypointPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, CreateShip, Debtors_CreateShipPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnInitMission, Debtors_OnInitMissionPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, ShowScorer, Debtors_ShowScorerPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, EnableMissionWaypoint, Debtors_EnableMissionWaypointPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, SelectNewMissionTarget, Debtors_SelectNewMissionTargetPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnMissionTargetDeath, Debtors_OnMissionTargetDeathPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, BankMasterAttacksMissionTarget, Debtors_BankMasterAttacksMissionTargetPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, DropTreasure, Debtors_DropTreasurePy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, DelTreasure, Debtors_DelTreasurePy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, SendShipToGetTreasure, Debtors_SendShipToGetTreasurePy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, GetPossibleMercTargets, Debtors_GetPossibleMercTargetsPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, CreateMercs, Debtors_CreateMercsPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, RespawnBankMaster, Debtors_RespawnBankMasterPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnBankMasterDeath, Debtors_OnBankMasterDeathPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, CheckMercsFreezed, Debtors_CheckMercsFreezedPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnBankerDeath, Debtors_OnBankerDeathPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnMercDeath, Debtors_OnMercDeathPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnTreasureReached, Debtors_OnTreasureReachedPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnShipReachedTreasurePos, Debtors_OnShipReachedTreasurePosPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, OnCoinTaken, Debtors_OnCoinTakenPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, BankMasterSpeech, Debtors_BankMasterSpeechPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, ShowDeepThroatMessage, Debtors_ShowDeepThroatMessagePy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, SetNextMission, Debtors_SetNextMissionPy) \
	PORT_DEFL(outskirts, Mission_DestroyBadDebtors, MissionScorer, Debtors_MissionScorerPy) \
	PORT_DEFL(outskirts, Mission_DestroyHumphrey, CreateHumphrey, Humphrey_CreateHumphreyPy) \
	PORT_DEFL(outskirts, Mission_DestroyHumphrey, HumphreyAttacks, Humphrey_HumphreyAttacksPy) \
	PORT_DEFL(outskirts, Mission_DestroyHumphrey, LaunchTaunt, Humphrey_LaunchTauntPy) \
	PORT_DEFL(outskirts, Mission_DestroyHumphrey, OnHumphreyDeath, Humphrey_OnHumphreyDeathPy) \
	PORT_DEFL(outskirts, Mission_DestroyHumphrey, MissionScorer, Humphrey_MissionScorerPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, Init, Hunt_InitPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, CreateWaypoint, Hunt_CreateWaypointPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, EnableMissionWaypoint, Hunt_EnableMissionWaypointPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, MissionScorer, Hunt_MissionScorerPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, ComeBack, Hunt_ComeBackPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, OnChangeVehicle, Hunt_OnChangeVehiclePy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, CreateNewPoliShip, Hunt_CreateNewPoliShipPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, CreateNewFuncShip, Hunt_CreateNewFuncShipPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, CreateFriendsShips, Hunt_CreateFriendsShipsPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, CreateEnemiesShips, Hunt_CreateEnemiesShipsPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, SetNextMission, Hunt_SetNextMissionPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, CGCalls, Hunt_CGCallsPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, OnFuncDeath, Hunt_OnFuncDeathPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, isPoliceShip, Hunt_isPoliceShipPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, OnMissionStart, Hunt_OnMissionStartPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, LaunchMission, Hunt_LaunchMissionPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, StartCameraTravelling, Hunt_StartCameraTravellingPy) \
	PORT_DEFL(outskirts, Mission_FunctionaryHunting, AbortCameraTravelling, Hunt_AbortCameraTravellingPy) \
	PORT_DEFL(outskirts, Mission_GoToOutskirts, Init, GoTo_InitPy) \
	PORT_DEFL(outskirts, Mission_GoToOutskirts, LaunchCrazyGamblerComunication, GoTo_LaunchCrazyGamblerComunicationPy) \
	PORT_DEFL(outskirts, Mission_GoToOutskirts, PlayCrazyGamblerComunication, GoTo_PlayCrazyGamblerComunicationPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, CreateLonelyMercProfiles, Merc_CreateLonelyMercProfilesPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, InitStatus, Merc_InitStatusPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, Init, Merc_InitPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, CreateLonelyMercInScene, Merc_CreateLonelyMercInScenePy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, CheckTraitoriusDeath, Merc_CheckTraitoriusDeathPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, EndOfScene, Merc_EndOfScenePy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, DelLonelyMerc, Merc_DelLonelyMercPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, InitMission, Merc_InitMissionPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, ScanShip, Merc_ScanShipPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, OnFinishScan, Merc_OnFinishScanPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, OnLonelyMercDeath, Merc_OnLonelyMercDeathPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, OnLonelyMercDamage, Merc_OnLonelyMercDamagePy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, PreRespawnLonelyMerc, Merc_PreRespawnLonelyMercPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, RespawnLonelyMerc, Merc_RespawnLonelyMercPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, SetPhoneCab, Merc_SetPhoneCabPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, UsePhoneCab, Merc_UsePhoneCabPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, SetNextMission, Merc_SetNextMissionPy) \
	PORT_DEFL(outskirts, Mission_IdentifyMerc, MissionScorer, Merc_MissionScorerPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, InitStatus, Police_InitStatusPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, Init, Police_InitPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, SetPos, Police_SetPosPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, InitMission, Police_InitMissionPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, OnAlarm, Police_OnAlarmPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, OnPoliceDeath, Police_OnPoliceDeathPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, BertoAppears, Police_BertoAppearsPy) \
	PORT_DEFL(outskirts, Mission_KillPolice, EnableMetro, Police_EnableMetroPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, InitStatus, Rudolph_InitStatusPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, Init, Rudolph_InitPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, InitRace, Rudolph_InitRacePy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, initCreateRudolph, Rudolph_initCreateRudolphPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, SetRudolphDef, Rudolph_SetRudolphDefPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, OnDamage, Rudolph_OnDamagePy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, EndOfRace, Rudolph_EndOfRacePy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, SetRudolphAsNone, Rudolph_SetRudolphAsNonePy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, RudolphKilled, Rudolph_RudolphKilledPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, CreateClue, Rudolph_CreateCluePy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, ActivateClue, Rudolph_ActivateCluePy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, OnClueTaken, Rudolph_OnClueTakenPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, InitTalkToPoliceBoss, Rudolph_InitTalkToPoliceBossPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, UsePhoneCab, Rudolph_UsePhoneCabPy) \
	PORT_DEFL(outskirts, Mission_KillRudolph, SetNextMission2, Rudolph_SetNextMission2Py) \
	PORT_DEFL(outskirts, Mission_TransportKiller, Init, Transport_InitPy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, InitTransport, Transport_InitTransportPy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, isValidDamage, Transport_isValidDamagePy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, TryToDeath, Transport_TryToDeathPy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, TakeDataPack, Transport_TakeDataPackPy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, TryToDamage, Transport_TryToDamagePy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, InitCall, Transport_InitCallPy) \
	PORT_DEFL(outskirts, Mission_TransportKiller, SetNextMission, Transport_SetNextMissionPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, Init, Win2nd_InitPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, InitRace, Win2nd_InitRacePy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, GetRaceWaypointIndex, Win2nd_GetRaceWaypointIndexPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, RacerDamaged, Win2nd_RacerDamagedPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, OnRaceStart, Win2nd_OnRaceStartPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, OnCheckPoint, Win2nd_OnCheckPointPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, OnNearCheckPoint, Win2nd_OnNearCheckPointPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, NameOponGoItem, Win2nd_NameOponGoItemPy) \
	PORT_DEFL(outskirts, Mission_Win2ndRace, CancelMission, Win2nd_CancelMissionPy)

/* levels/Police.c */
PORT_DECLL(police, Map, CreateFicusDeadClass) /* 0 */
PORT_DECLL(police, Map, CreateFicus) /* 0 */
PORT_DECLL(police, Map, InitStatus) /* 1 */
PORT_DECLL(police, Map, Init) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, InitStatus) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, Init) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, DisableIAFunctionary) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, OnTraitoriusRespawn) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, OnThugRespawn) /* 1 */
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
PORT_DECLL(police, Mission_BeatPickus, OnTalkToPickus) /* 4 */
PORT_DECLL(police, Mission_BeatPickus, PreGetFuncShipPlans) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, GetFuncShipPlans) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, TakePlanPhoto) /* 1 */
PORT_DECLL(police, Mission_BeatPickus, TalkToTraitoriusAgain) /* 0 */
PORT_DECLL(police, Mission_BeatPickus, TraitoriusAgainSpeech) /* 4 */
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
	PORT_DEFL(police, Map, CreateFicusDeadClass, PoMap_CreateFicusDeadClassA) \
	PORT_DEFL(police, Map, CreateFicus, PoMap_CreateFicusA) \
	PORT_DEFL(police, Map, InitStatus, PoMap_InitStatusA) \
	PORT_DEFL(police, Map, Init, PoMap_InitA) \
	PORT_DEFL(police, Mission_BeatPickus, InitStatus, BeatPickus_InitStatusA) \
	PORT_DEFL(police, Mission_BeatPickus, Init, BeatPickus_InitA) \
	PORT_DEFL(police, Mission_BeatPickus, DisableIAFunctionary, BeatPickus_DisableIAFunctionaryA) \
	PORT_DEFL(police, Mission_BeatPickus, OnTraitoriusRespawn, BeatPickus_OnTraitoriusRespawnA) \
	PORT_DEFL(police, Mission_BeatPickus, OnThugRespawn, BeatPickus_OnThugRespawnA) \
	PORT_DEFL(police, Mission_BeatPickus, SetState, BeatPickus_SetStateA) \
	PORT_DEFL(police, Mission_BeatPickus, TraitoriusSpeech, BeatPickus_TraitoriusSpeechA) \
	PORT_DEFL(police, Mission_BeatPickus, HireThugs, BeatPickus_HireThugsA) \
	PORT_DEFL(police, Mission_BeatPickus, ThugLeaderSpeech, BeatPickus_ThugLeaderSpeechA) \
	PORT_DEFL(police, Mission_BeatPickus, ThugNormalSpeech, BeatPickus_ThugNormalSpeechA) \
	PORT_DEFL(police, Mission_BeatPickus, BeatPickus, BeatPickus_BeatPickusA) \
	PORT_DEFL(police, Mission_BeatPickus, OnAlarmActivated, BeatPickus_OnAlarmActivatedA) \
	PORT_DEFL(police, Mission_BeatPickus, CheckMaxDist, BeatPickus_CheckMaxDistA) \
	PORT_DEFL(police, Mission_BeatPickus, CheckThugsDist, BeatPickus_CheckThugsDistA) \
	PORT_DEFL(police, Mission_BeatPickus, CreateLauncher, BeatPickus_CreateLauncherA) \
	PORT_DEFL(police, Mission_BeatPickus, LaunchPickusFightScene, BeatPickus_LaunchPickusFightSceneA) \
	PORT_DEFL(police, Mission_BeatPickus, CreatePickusBeatenClass, BeatPickus_CreatePickusBeatenClassA) \
	PORT_DEFL(police, Mission_BeatPickus, InitPickusBeaten, BeatPickus_InitPickusBeatenA) \
	PORT_DEFL(police, Mission_BeatPickus, DelThug, BeatPickus_DelThugA) \
	PORT_DEFL(police, Mission_BeatPickus, OnTalkToPickus, BeatPickus_OnTalkToPickusA) \
	PORT_DEFL(police, Mission_BeatPickus, PreGetFuncShipPlans, BeatPickus_PreGetFuncShipPlansA) \
	PORT_DEFL(police, Mission_BeatPickus, GetFuncShipPlans, BeatPickus_GetFuncShipPlansA) \
	PORT_DEFL(police, Mission_BeatPickus, TakePlanPhoto, BeatPickus_TakePlanPhotoA) \
	PORT_DEFL(police, Mission_BeatPickus, TalkToTraitoriusAgain, BeatPickus_TalkToTraitoriusAgainA) \
	PORT_DEFL(police, Mission_BeatPickus, TraitoriusAgainSpeech, BeatPickus_TraitoriusAgainSpeechA) \
	PORT_DEFL(police, Mission_FicusDead, InitFicusDead, FicusDead_InitFicusDeadA) \
	PORT_DEFL(police, Mission_FicusDead, Init, FicusDead_InitA) \
	PORT_DEFL(police, Mission_FicusDead, OnTalkToFicus, FicusDead_OnTalkToFicusA) \
	PORT_DEFL(police, Mission_FicusDead, AfterDeepThroatMessageShown, FicusDead_AfterDeepThroatMessageShownA) \
	PORT_DEFL(police, Mission_FicusDead, DeepThroatSpeech, FicusDead_DeepThroatSpeechA) \
	PORT_DEFL(police, Mission_FicusDead, SetNextMission, FicusDead_SetNextMissionA) \
	PORT_DEFL(police, Mission_GetAuthorization, InitStatus, GetAuthorization_InitStatusA) \
	PORT_DEFL(police, Mission_GetAuthorization, CreateHuman, GetAuthorization_CreateHumanA) \
	PORT_DEFL(police, Mission_GetAuthorization, Init, GetAuthorization_InitA) \
	PORT_DEFL(police, Mission_GetAuthorization, AfterDeepThroatMessage, GetAuthorization_AfterDeepThroatMessageA) \
	PORT_DEFL(police, Mission_GetAuthorization, DeepThroatSpeech, GetAuthorization_DeepThroatSpeechA) \
	PORT_DEFL(police, Mission_GetAuthorization, PreGetAuthorization, GetAuthorization_PreGetAuthorizationA) \
	PORT_DEFL(police, Mission_GetAuthorization, GetAuthorization, GetAuthorization_GetAuthorizationA) \
	PORT_DEFL(police, Mission_GetAuthorization, ShowNearestConversor, GetAuthorization_ShowNearestConversorA) \
	PORT_DEFL(police, Mission_GetAuthorization, ShowAfterConversionMessage, GetAuthorization_ShowAfterConversionMessageA) \
	PORT_DEFL(police, Mission_GetAuthorization, OnDataTaken, GetAuthorization_OnDataTakenA) \
	PORT_DEFL(police, Mission_GetAuthorization, OnDataDropped, GetAuthorization_OnDataDroppedA) \
	PORT_DEFL(police, Mission_GetAuthorization, FreeHuman, GetAuthorization_FreeHumanA) \
	PORT_DEFL(police, Mission_GetAuthorization, TalkPepe, GetAuthorization_TalkPepeA) \
	PORT_DEFL(police, Mission_GetAuthorization, StartScene, GetAuthorization_StartSceneA) \
	PORT_DEFL(police, Mission_GetAuthorization, OnTriggerActivated, GetAuthorization_OnTriggerActivatedA) \
	PORT_DEFL(police, Mission_GetAuthorization, GoToPress, GetAuthorization_GoToPressA) \
	PORT_DEFL(police, Mission_GetAuthorization, GoToPress2, GetAuthorization_GoToPress2A) \
	PORT_DEFL(police, Mission_GetAuthorization, OnPossession, GetAuthorization_OnPossessionA) \
	PORT_DEFL(police, Mission_GetAuthorization, OnExitConversor, GetAuthorization_OnExitConversorA) \
	PORT_DEFL(police, Mission_GetAuthorization, CheckMaxDist, GetAuthorization_CheckMaxDistA) \
	PORT_DEFL(police, Mission_GetAuthorization, CheckHumanDist, GetAuthorization_CheckHumanDistA) \
	PORT_DEFL(police, Mission_GetAuthorization, OnMetroUsed, GetAuthorization_OnMetroUsedA) \
	PORT_DEFL(police, Mission_InitEpilogue, InitStatus, InitEpilogue_InitStatusA) \
	PORT_DEFL(police, Mission_InitEpilogue, CreateBettyJail, InitEpilogue_CreateBettyJailA) \
	PORT_DEFL(police, Mission_InitEpilogue, CreateBertoJail, InitEpilogue_CreateBertoJailA) \
	PORT_DEFL(police, Mission_InitEpilogue, Init, InitEpilogue_InitA) \
	PORT_DEFL(police, Mission_InitEpilogue, OnBettySpeech, InitEpilogue_OnBettySpeechA) \
	PORT_DEFL(police, Mission_InitEpilogue, OnBertoSpeech, InitEpilogue_OnBertoSpeechA) \
	PORT_DEFL(police, Mission_InitEpilogue, SetNextMission, InitEpilogue_SetNextMissionA) \
	PORT_DEFL(police, Mission_JailBreak, InitStatus, JailBreak_InitStatusA) \
	PORT_DEFL(police, Mission_JailBreak, AfterBrowseNPC, JailBreak_AfterBrowseNPCA) \
	PORT_DEFL(police, Mission_JailBreak, CreateMCList, JailBreak_CreateMCListA) \
	PORT_DEFL(police, Mission_JailBreak, CreateGearStay, JailBreak_CreateGearStayA) \
	PORT_DEFL(police, Mission_JailBreak, CreatePuff, JailBreak_CreatePuffA) \
	PORT_DEFL(police, Mission_JailBreak, InitPuffReborn, JailBreak_InitPuffRebornA) \
	PORT_DEFL(police, Mission_JailBreak, StaplerSpeech, JailBreak_StaplerSpeechA) \
	PORT_DEFL(police, Mission_JailBreak, CheckForJump, JailBreak_CheckForJumpA) \
	PORT_DEFL(police, Mission_JailBreak, OnPossession, JailBreak_OnPossessionA) \
	PORT_DEFL(police, Mission_JailBreak, FirstTimeJail, JailBreak_FirstTimeJailA) \
	PORT_DEFL(police, Mission_JailBreak, Init, JailBreak_InitA) \
	PORT_DEFL(police, Mission_JailBreak, NextMission, JailBreak_NextMissionA) \
	PORT_DEFL(police, Mission_JailBreak, LoadPolice, JailBreak_LoadPoliceA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, InitStatus, PoliceBossCheats_InitStatusA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, Init, PoliceBossCheats_InitA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, CreatePoliceFicusPal, PoliceBossCheats_CreatePoliceFicusPalA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, FicusPalMissionSpeech, PoliceBossCheats_FicusPalMissionSpeechA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, CreateFakeChar, PoliceBossCheats_CreateFakeCharA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, CheatingLoop, PoliceBossCheats_CheatingLoopA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, PlayAnm, PoliceBossCheats_PlayAnmA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, WatchEvents, PoliceBossCheats_WatchEventsA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, PlayDumb1, PoliceBossCheats_PlayDumb1A) \
	PORT_DEFL(police, Mission_PoliceBossCheats, CheatPoliceBoss, PoliceBossCheats_CheatPoliceBossA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, PlayDumb2, PoliceBossCheats_PlayDumb2A) \
	PORT_DEFL(police, Mission_PoliceBossCheats, BrupPoliceBoss, PoliceBossCheats_BrupPoliceBossA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, PlayPoliceBoss, PoliceBossCheats_PlayPoliceBossA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, PhotoOn, PoliceBossCheats_PhotoOnA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, PhotoOff, PoliceBossCheats_PhotoOffA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, OnPhoto, PoliceBossCheats_OnPhotoA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, EndOfPhoto, PoliceBossCheats_EndOfPhotoA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, InitAfterPhoto, PoliceBossCheats_InitAfterPhotoA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, InitAfterPhoto2, PoliceBossCheats_InitAfterPhoto2A) \
	PORT_DEFL(police, Mission_PoliceBossCheats, InitAfterPhoto3, PoliceBossCheats_InitAfterPhoto3A) \
	PORT_DEFL(police, Mission_PoliceBossCheats, InitAskCrazyGambler, PoliceBossCheats_InitAskCrazyGamblerA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, CrazyGamblerSpeech, PoliceBossCheats_CrazyGamblerSpeechA) \
	PORT_DEFL(police, Mission_PoliceBossCheats, SetNextMission, PoliceBossCheats_SetNextMissionA)

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
PORT_DECLL(press, Mission_TalkToBoss, OnPossession) /* 2 */
PORT_DECLL(press, Mission_TalkToBoss, OnExitConversor) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, CheckMaxDist) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, CheckHumanDist) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, GoTalkToBoss) /* 0 */
PORT_DECLL(press, Mission_TalkToBoss, BossSpeech) /* 4 */
PORT_DECLL(press, Mission_TalkToBoss, SetNextMission) /* 1 */
PORT_DECLL(press, Mission_WhoIsDeepThroat, Init) /* 1 */
PORT_DECLL(press, Mission_WhoIsDeepThroat, UseTeleporter) /* 4 */
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
	PORT_DEFL(press, Map, RemoteMsg, PressMap_RemoteMsgA) \
	PORT_DEFL(press, Map, ForbiddenMsg, PressMap_ForbiddenMsgA) \
	PORT_DEFL(press, Map, StartAlarmator, PressMap_StartAlarmatorA) \
	PORT_DEFL(press, Map, ActivateAlarm, PressMap_ActivateAlarmA) \
	PORT_DEFL(press, Map, CreateActivators, PressMap_CreateActivatorsA) \
	PORT_DEFL(press, Map, CreateDeactivators, PressMap_CreateDeactivatorsA) \
	PORT_DEFL(press, Map, DeactivateAlarm, PressMap_DeactivateAlarmA) \
	PORT_DEFL(press, Map, BrowseAlarmLaunchers, PressMap_BrowseAlarmLaunchersA) \
	PORT_DEFL(press, Map, InitStatus, PressMap_InitStatusA) \
	PORT_DEFL(press, Map, Init, PressMap_InitA) \
	PORT_DEFL(press, Map, HideBureau, PressMap_HideBureauA) \
	PORT_DEFL(press, MapDemoClose, Init, DemoClose_InitA) \
	PORT_DEFL(press, MapDemoClose, OnMetroUsed, DemoClose_OnMetroUsedA) \
	PORT_DEFL(press, MapSnd, LoadSounds, PressSnd_LoadSoundsA) \
	PORT_DEFL(press, MapSnd, SetSounds, PressSnd_SetSoundsA) \
	PORT_DEFL(press, MapSnd, News1, PressSnd_News1A) \
	PORT_DEFL(press, Mission_FicusBanker, InitStatus, FicusBanker_InitStatusA) \
	PORT_DEFL(press, Mission_FicusBanker, Init, FicusBanker_InitA) \
	PORT_DEFL(press, Mission_FicusBanker, CreatePoliceFicusInit, FicusBanker_CreatePoliceFicusInitA) \
	PORT_DEFL(press, Mission_FicusBanker, FicusMissionSpeech, FicusBanker_FicusMissionSpeechA) \
	PORT_DEFL(press, Mission_FicusBanker, CreateBankDirectorScrapusInit, FicusBanker_CreateBankDirectorScrapusInitA) \
	PORT_DEFL(press, Mission_FicusBanker, ScrapusMissionSpeech, FicusBanker_ScrapusMissionSpeechA) \
	PORT_DEFL(press, Mission_FicusBanker, InitKillScrapus, FicusBanker_InitKillScrapusA) \
	PORT_DEFL(press, Mission_FicusBanker, SpawnCheck, FicusBanker_SpawnCheckA) \
	PORT_DEFL(press, Mission_FicusBanker, TakeCheck, FicusBanker_TakeCheckA) \
	PORT_DEFL(press, Mission_FicusBanker, SetNextMission, FicusBanker_SetNextMissionA) \
	PORT_DEFL(press, Mission_GetCrazyData, Init, CrazyData_InitA) \
	PORT_DEFL(press, Mission_GetCrazyData, GetCrazyData, CrazyData_GetCrazyDataA) \
	PORT_DEFL(press, Mission_GetCrazyData, OnAlarmActivated, CrazyData_OnAlarmActivatedA) \
	PORT_DEFL(press, Mission_GetCrazyData, OnSwitchPressed, CrazyData_OnSwitchPressedA) \
	PORT_DEFL(press, Mission_GetCrazyData, StartTimer, CrazyData_StartTimerA) \
	PORT_DEFL(press, Mission_GetCrazyData, TimeExpired, CrazyData_TimeExpiredA) \
	PORT_DEFL(press, Mission_GetCrazyData, GoToPressRoom, CrazyData_GoToPressRoomA) \
	PORT_DEFL(press, Mission_GetCrazyData, OnFakeBureauTaken, CrazyData_OnFakeBureauTakenA) \
	PORT_DEFL(press, Mission_GetCrazyData, FindHumphrey, CrazyData_FindHumphreyA) \
	PORT_DEFL(press, Mission_GetCrazyData, HumphreySpeech, CrazyData_HumphreySpeechA) \
	PORT_DEFL(press, Mission_GetCrazyData, FindTrurl, CrazyData_FindTrurlA) \
	PORT_DEFL(press, Mission_GetCrazyData, TrurlSpeech, CrazyData_TrurlSpeechA) \
	PORT_DEFL(press, Mission_GetCrazyData, MissionScorer, CrazyData_MissionScorerA) \
	PORT_DEFL(press, Mission_GoToPress, Init, GoToPress_InitA) \
	PORT_DEFL(press, Mission_GoToPress, CrazySads, GoToPress_CrazySadsA) \
	PORT_DEFL(press, Mission_GoWithHuman, InitStatus, GoWithHuman_InitStatusA) \
	PORT_DEFL(press, Mission_GoWithHuman, CreateHuman, GoWithHuman_CreateHumanA) \
	PORT_DEFL(press, Mission_GoWithHuman, Init, GoWithHuman_InitA) \
	PORT_DEFL(press, Mission_GoWithHuman, OnPossession, GoWithHuman_OnPossessionA) \
	PORT_DEFL(press, Mission_GoWithHuman, OnExitConversor, GoWithHuman_OnExitConversorA) \
	PORT_DEFL(press, Mission_GoWithHuman, CheckMaxDist, GoWithHuman_CheckMaxDistA) \
	PORT_DEFL(press, Mission_GoWithHuman, CheckHumanDist, GoWithHuman_CheckHumanDistA) \
	PORT_DEFL(press, Mission_GoWithHuman, LoadScene2, GoWithHuman_LoadScene2A) \
	PORT_DEFL(press, Mission_GoWithHuman, CreateFakeSpeech, GoWithHuman_CreateFakeSpeechA) \
	PORT_DEFL(press, Mission_GoWithHuman, NormCamera, Pr_NormCameraA) \
	PORT_DEFL(press, Mission_GoWithHuman, SetTalk, Pr_SetTalkA) \
	PORT_DEFL(press, Mission_HumphreyArchbishopKilled, SetNextMission, ArchbishopKilled_SetNextMissionA) \
	PORT_DEFL(press, Mission_HumphreyArchbishopKilled, CGCalls, ArchbishopKilled_CGCallsA) \
	PORT_DEFL(press, Mission_IntroHuman2, LaunchHumphreySpeech, IntroHuman2_LaunchHumphreySpeechA) \
	PORT_DEFL(press, Mission_IntroHuman2, LoadScene, IntroHuman2_LoadSceneA) \
	PORT_DEFL(press, Mission_MeetBoss, InitStatus, MeetBoss_InitStatusA) \
	PORT_DEFL(press, Mission_MeetBoss, AfterBrowseNPC, MeetBoss_AfterBrowseNPCA) \
	PORT_DEFL(press, Mission_MeetBoss, CreateHumphreyInit, MeetBoss_CreateHumphreyInitA) \
	PORT_DEFL(press, Mission_MeetBoss, CreateBertoInit, MeetBoss_CreateBertoInitA) \
	PORT_DEFL(press, Mission_MeetBoss, BertoSpeech, MeetBoss_BertoSpeechA) \
	PORT_DEFL(press, Mission_MeetBoss, BertoSpeech2, MeetBoss_BertoSpeech2A) \
	PORT_DEFL(press, Mission_MeetBoss, DeactivateEmero, MeetBoss_DeactivateEmeroA) \
	PORT_DEFL(press, Mission_MeetBoss, ActivateEmero, MeetBoss_ActivateEmeroA) \
	PORT_DEFL(press, Mission_MeetBoss, Init, MeetBoss_InitA) \
	PORT_DEFL(press, Mission_MeetBoss, CreateBattery, MeetBoss_CreateBatteryA) \
	PORT_DEFL(press, Mission_MeetBoss, AssingOnTakeBattery, MeetBoss_AssingOnTakeBatteryA) \
	PORT_DEFL(press, Mission_MeetBoss, OnTakeBattery, MeetBoss_OnTakeBatteryA) \
	PORT_DEFL(press, Mission_MeetBoss, PreBertoPossession, MeetBoss_PreBertoPossessionA) \
	PORT_DEFL(press, Mission_MeetBoss, BertoPossession, MeetBoss_BertoPossessionA) \
	PORT_DEFL(press, Mission_MeetBoss, OnWarning, MeetBoss_OnWarningA) \
	PORT_DEFL(press, Mission_MeetBoss, OnActionPressed, MeetBoss_OnActionPressedA) \
	PORT_DEFL(press, Mission_MeetBoss, OnPossession, MeetBoss_OnPossessionA) \
	PORT_DEFL(press, Mission_MeetBoss, ShowMessage, MeetBoss_ShowMessageA) \
	PORT_DEFL(press, Mission_MeetBoss, OnAlarmEnd, MeetBoss_OnAlarmEndA) \
	PORT_DEFL(press, Mission_MeetBoss, UseConversor, MeetBoss_UseConversorA) \
	PORT_DEFL(press, Mission_MeetBoss, OnConversorUsed, MeetBoss_OnConversorUsedA) \
	PORT_DEFL(press, Mission_MeetBoss, CheckConversion, MeetBoss_CheckConversionA) \
	PORT_DEFL(press, Mission_MeetBoss, ChallengeHumphrey, MeetBoss_ChallengeHumphreyA) \
	PORT_DEFL(press, Mission_MeetBoss, HumphreySpeech, MeetBoss_HumphreySpeechA) \
	PORT_DEFL(press, Mission_MeetBoss, SetNextMission, MeetBoss_SetNextMissionA) \
	PORT_DEFL(press, Mission_PressMeeting, SetNextMission, PressMeeting_SetNextMissionA) \
	PORT_DEFL(press, Mission_TalkToBoss, InitStatus, TalkToBoss_InitStatusA) \
	PORT_DEFL(press, Mission_TalkToBoss, AfterBrowseNPC, TalkToBoss_AfterBrowseNPCA) \
	PORT_DEFL(press, Mission_TalkToBoss, SetBossSpeech, TalkToBoss_SetBossSpeechA) \
	PORT_DEFL(press, Mission_TalkToBoss, CreateHuman, TalkToBoss_CreateHumanA) \
	PORT_DEFL(press, Mission_TalkToBoss, Init, TalkToBoss_InitA) \
	PORT_DEFL(press, Mission_TalkToBoss, HideHumanIntoBureau, TalkToBoss_HideHumanIntoBureauA) \
	PORT_DEFL(press, Mission_TalkToBoss, CreateSceneLaunchers, TalkToBoss_CreateSceneLaunchersA) \
	PORT_DEFL(press, Mission_TalkToBoss, LaunchScene, TalkToBoss_LaunchSceneA) \
	PORT_DEFL(press, Mission_TalkToBoss, OnPossession, TalkToBoss_OnPossessionA) \
	PORT_DEFL(press, Mission_TalkToBoss, OnExitConversor, TalkToBoss_OnExitConversorA) \
	PORT_DEFL(press, Mission_TalkToBoss, CheckMaxDist, TalkToBoss_CheckMaxDistA) \
	PORT_DEFL(press, Mission_TalkToBoss, CheckHumanDist, TalkToBoss_CheckHumanDistA) \
	PORT_DEFL(press, Mission_TalkToBoss, GoTalkToBoss, TalkToBoss_GoTalkToBossA) \
	PORT_DEFL(press, Mission_TalkToBoss, BossSpeech, TalkToBoss_BossSpeechA) \
	PORT_DEFL(press, Mission_TalkToBoss, SetNextMission, TalkToBoss_SetNextMissionA) \
	PORT_DEFL(press, Mission_WhoIsDeepThroat, Init, DeepThroat_InitA) \
	PORT_DEFL(press, Mission_WhoIsDeepThroat, UseTeleporter, DeepThroat_UseTeleporterA) \
	PORT_DEFL(press, Mission_WhoIsDeepThroat, PlayScene, DeepThroat_PlaySceneA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, InitStatus, Baddie_InitStatusA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, Init, Baddie_InitA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, GoForHuman, Baddie_GoForHumanA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, OnBureauUsed, Baddie_OnBureauUsedA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, PlayScene, Baddie_PlaySceneA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, EndOfScene, Baddie_EndOfSceneA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, CreateFakeSpeech, Baddie_CreateFakeSpeechA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, NormCamera, Pr_NormCameraA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, SetTalk, Pr_SetTalkA) \
	PORT_DEFL(press, Mission_WhoIsTheBaddie, LoadScene, Baddie_LoadSceneA)

/* levels/QF.c */
PORT_DECLL(qf, Mission_BankersCombat, CreatePlans) /* 0 */
PORT_DECLL(qf, Mission_BankersCombat, TakePlanPhoto) /* 1 */
PORT_DECLL(qf, Mission_BankersCombat, OnEndOfSuperCrazyDeal) /* 0 */
PORT_DECLL(qf, Mission_BankersCombat, BankersAttackPlayer) /* 0 */
PORT_DECLL(qf, Mission_BankersCombat, SetNextMission) /* 0 */
#define PORTS_levels_QF \
	PORT_DEFL(qf, Mission_BankersCombat, CreatePlans, Bc_CreatePlansPy) \
	PORT_DEFL(qf, Mission_BankersCombat, TakePlanPhoto, Bc_TakePlanPhotoPy) \
	PORT_DEFL(qf, Mission_BankersCombat, OnEndOfSuperCrazyDeal, Bc_OnEndOfSuperCrazyDealPy) \
	PORT_DEFL(qf, Mission_BankersCombat, BankersAttackPlayer, Bc_BankersAttackPlayerPy) \
	PORT_DEFL(qf, Mission_BankersCombat, SetNextMission, Bc_SetNextMissionPy)

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
	PORT_DEFL(scrapyard, Map, InitStatus, Scrapyard_InitStatus) \
	PORT_DEFL(scrapyard, Map, Init, Scrapyard_Init) \
	PORT_DEFL(scrapyard, MapTriggers, GetMadHunterProfile, MT_GetMadHunterProfile) \
	PORT_DEFL(scrapyard, MapTriggers, GetMosquitoProfile, MT_GetMosquitoProfile) \
	PORT_DEFL(scrapyard, MapTriggers, Init, MT_Init) \
	PORT_DEFL(scrapyard, MapTriggers, CheckMosquitoCadenceShoot, MT_CheckMosquitoCadenceShoot) \
	PORT_DEFL(scrapyard, MapTriggers, CreateMosquitosTriggers, MT_CreateMosquitosTriggers) \
	PORT_DEFL(scrapyard, MapTriggers, OnMosquitoTriggerTaken, MT_OnMosquitoTriggerTaken) \
	PORT_DEFL(scrapyard, MapTriggers, CreateMosquito, MT_CreateMosquito) \
	PORT_DEFL(scrapyard, MapTriggers, CreateMadHuntersTriggers, MT_CreateMadHuntersTriggers) \
	PORT_DEFL(scrapyard, MapTriggers, OnMadHunterTriggerTaken, MT_OnMadHunterTriggerTaken) \
	PORT_DEFL(scrapyard, MapTriggers, CreateMadHunter, MT_CreateMadHunter) \
	PORT_DEFL(scrapyard, Mission_Ambush, GetMadHuntersProfile, Ambush_GetMadHuntersProfile) \
	PORT_DEFL(scrapyard, Mission_Ambush, InitStatus, Ambush_InitStatus) \
	PORT_DEFL(scrapyard, Mission_Ambush, Init, Ambush_Init) \
	PORT_DEFL(scrapyard, Mission_Ambush, CallTraitorius, Ambush_CallTraitorius) \
	PORT_DEFL(scrapyard, Mission_Ambush, UsePhoneCab, Ambush_UsePhoneCab) \
	PORT_DEFL(scrapyard, Mission_Ambush, DeepThroatCom, Ambush_DeepThroatCom) \
	PORT_DEFL(scrapyard, Mission_Ambush, IsValidPos, Ambush_IsValidPos) \
	PORT_DEFL(scrapyard, Mission_Ambush, CreateProbe, Ambush_CreateProbe) \
	PORT_DEFL(scrapyard, Mission_Ambush, CreateMadHunter, Ambush_CreateMadHunter) \
	PORT_DEFL(scrapyard, Mission_Ambush, CreateMadHunters, Ambush_CreateMadHunters) \
	PORT_DEFL(scrapyard, Mission_Ambush, OnProbeDeath, Ambush_OnProbeDeath) \
	PORT_DEFL(scrapyard, Mission_Ambush, PreCallDeepThroat, Ambush_PreCallDeepThroat) \
	PORT_DEFL(scrapyard, Mission_Ambush, CallDeepThroat, Ambush_CallDeepThroat) \
	PORT_DEFL(scrapyard, Mission_Ambush, UsePhoneCab2, Ambush_UsePhoneCab2) \
	PORT_DEFL(scrapyard, Mission_Ambush, SetNextMission, Ambush_SetNextMission) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, GetCrazyGamblerProfile, Gambler_GetCrazyGamblerProfile) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, GetMosquitoProfile, Gambler_GetMosquitoProfile) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, InitStatus, Gambler_InitStatus) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, Init, Gambler_Init) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, LaunchTaunts, Gambler_LaunchTaunts) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, CreateMosquito, Gambler_CreateMosquito) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, CreateMosquitos, Gambler_CreateMosquitos) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, CreateCrazyGambler, Gambler_CreateCrazyGambler) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, OnCrazyGamblerDeath, Gambler_OnCrazyGamblerDeath) \
	PORT_DEFL(scrapyard, Mission_DefeatCrazyGambler2, MissionScorer, Gambler_MissionScorer) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, SetLonelyMercProfile, Dtritus_SetLonelyMercProfile) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, InitStatus, Dtritus_InitStatus) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, Init, Dtritus_Init) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CreateLonelyMercShip, Dtritus_CreateLonelyMercShip) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, OnLonelyMercShipDeath, Dtritus_OnLonelyMercShipDeath) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CheckLonelyMercShipExist, Dtritus_CheckLonelyMercShipExist) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, TestForLonelyMercShip, Dtritus_TestForLonelyMercShip) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CheckCreateBadGuy, Dtritus_CheckCreateBadGuy) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, OutOfLonelyMercShip, Dtritus_OutOfLonelyMercShip) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, ActivateMissionObjects, Dtritus_ActivateMissionObjects) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CreateBadGuy, Dtritus_CreateBadGuy) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, OnBadGuyDeath, Dtritus_OnBadGuyDeath) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CreatePlan, Dtritus_CreatePlan) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, TakePlanPhoto, Dtritus_TakePlanPhoto) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CreateDtritusMatrix, Dtritus_CreateDtritusMatrix) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, TakeMatrix, Dtritus_TakeMatrix) \
	PORT_DEFL(scrapyard, Mission_FindDtritusMatrix, CheckEndOfMission, Dtritus_CheckEndOfMission) \
	PORT_DEFL(scrapyard, Mission_MeetBerto, InitStatus, Berto_InitStatus) \
	PORT_DEFL(scrapyard, Mission_MeetBerto, Init, Berto_Init) \
	PORT_DEFL(scrapyard, Mission_MeetBerto, UsePhoneCab, Berto_UsePhoneCab) \
	PORT_DEFL(scrapyard, Mission_MeetBerto, SetNextMission, Berto_SetNextMission) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, GetMosquitoProfile, Clue_GetMosquitoProfile) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, InitStatus, Clue_InitStatus) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, Init, Clue_Init) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, InitSwarm, Clue_InitSwarm) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, InitMosquito, Clue_InitMosquito) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, CreateMosquito, Clue_CreateMosquito) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, MosquitoDeath, Clue_MosquitoDeath) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, TakeDataPack, Clue_TakeDataPack) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, InitPoliChat, Clue_InitPoliChat) \
	PORT_DEFL(scrapyard, Mission_MosquitoClue, SetNextMission, Clue_SetNextMission) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, InitStatus, Merc_InitStatus) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, Init, Merc_Init) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, OnStupidMercDeath, Merc_OnStupidMercDeath) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, OnStupidMercDamage, Merc_OnStupidMercDamage) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, CheckAITakesLife, Merc_CheckAITakesLife) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, CheckLife, Merc_CheckLife) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, GotoNext, Merc_GotoNext) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, CreateATPCPlans, Merc_CreateATPCPlans) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, AddATPCPlans, Merc_AddATPCPlans) \
	PORT_DEFL(scrapyard, Mission_StupidMerc, SetNextMission, Merc_SetNextMission) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, GetMercsEnemiesProfile, Again_GetMercsEnemiesProfile) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, InitStatus, Again_InitStatus) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, Init, Again_Init) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, InitFollow, Again_InitFollow) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, CreateMerc, Again_CreateMerc) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, CreateMercs, Again_CreateMercs) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, SmartMerDeath, Again_SmartMerDeath) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, GotoNext, Again_GotoNext) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, CreateMercPlans, Again_CreateMercPlans) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, AddMercPlans, Again_AddMercPlans) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, StartEvasion, Again_StartEvasion) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, CheckEvasion, Again_CheckEvasion) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, InitTalkBishop, Again_InitTalkBishop) \
	PORT_DEFL(scrapyard, Mission_StupidMercAgain, SetNextMission, Again_SetNextMission) \
	PORT_DEFL(scrapyard, Mission_TalkToDeepThroat, InitStatus, Throat_InitStatus) \
	PORT_DEFL(scrapyard, Mission_TalkToDeepThroat, Init, Throat_Init) \
	PORT_DEFL(scrapyard, Mission_TalkToDeepThroat, UsePhoneCab, Throat_UsePhoneCab) \
	PORT_DEFL(scrapyard, Mission_TalkToDeepThroat, SetNextMission, Throat_SetNextMission)

/* levels/Small.c */
PORT_DECLL(tuczone, Map, Init) /* 1 */
PORT_DECLL(nano, Map, InitStatus) /* 1 */
PORT_DECLL(nano, Map, Init) /* 1 */
PORT_DECLL(shipedit, Map, Init) /* 1 */
PORT_DECLL(shipedit, MapSnd, Init) /* 1 */
#define PORTS_levels_Small \
	PORT_DEFL(tuczone, Map, Init, Tuc_InitPy) \
	PORT_DEFL(nano, Map, InitStatus, Nano_InitStatusPy) \
	PORT_DEFL(nano, Map, Init, Nano_InitPy) \
	PORT_DEFL(shipedit, Map, Init, Se_InitPy) \
	PORT_DEFL(shipedit, MapSnd, Init, Se_SndInitPy)

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
	PORT_DEFL(temple, Map, InitStatus, TpMap_InitStatusPy) \
	PORT_DEFL(temple, Map, Init, TpMap_InitPy) \
	PORT_DEFL(temple, Map, EndOfAppears, TpMap_EndOfAppearsPy) \
	PORT_DEFL(temple, MapExtraLifes, TakeExtraLife, Xl_TakeExtraLifePy) \
	PORT_DEFL(temple, MapExtraLifes, SetRespawnTime, Xl_SetRespawnTimePy) \
	PORT_DEFL(temple, MapExtraLifes, MoveLifes, Xl_MoveLifesPy) \
	PORT_DEFL(temple, MapExtraLifes, SwitchAlarm, Xl_SwitchAlarmPy) \
	PORT_DEFL(temple, MapExtraLifes, Init, Xl_InitPy) \
	PORT_DEFL(temple, MapExtraLifes, GetMoney, Xl_GetMoneyPy) \
	PORT_DEFL(temple, Mission_ArchbishopDead, Init, Ad_InitPy) \
	PORT_DEFL(temple, Mission_ArchbishopDead, ForbiddenMsg, Ad_ForbiddenMsgPy) \
	PORT_DEFL(temple, Mission_ArchbishopDead, CreateViscProofs, Ad_CreateViscProofsPy) \
	PORT_DEFL(temple, Mission_ArchbishopDead, ViewProof, Ad_ViewProofPy) \
	PORT_DEFL(temple, Mission_ArchbishopDead, TakeProofPhoto, Ad_TakeProofPhotoPy) \
	PORT_DEFL(temple, Mission_ArchbishopDead, AfterDeepThroatMessageShown, Ad_AfterDeepThroatMessageShownPy) \
	PORT_DEFL(temple, Mission_AskGus, InitStatus, Ag_InitStatusPy) \
	PORT_DEFL(temple, Mission_AskGus, AfterBrowseNPC, Ag_AfterBrowseNPCPy) \
	PORT_DEFL(temple, Mission_AskGus, CreateGusInit, Ag_CreateGusInitPy) \
	PORT_DEFL(temple, Mission_AskGus, Init, Ag_InitPy) \
	PORT_DEFL(temple, Mission_AskGus, DeepThroatAppears, Ag_DeepThroatAppearsPy) \
	PORT_DEFL(temple, Mission_AskGus, PreCallDeepThroat, Ag_PreCallDeepThroatPy) \
	PORT_DEFL(temple, Mission_AskGus, CallDeepThroat, Ag_CallDeepThroatPy) \
	PORT_DEFL(temple, Mission_AskGus, DeepThroatSpeech, Ag_DeepThroatSpeechPy) \
	PORT_DEFL(temple, Mission_MeetArchbishop, UseDoor, Ma_UseDoorPy) \
	PORT_DEFL(temple, Mission_MeetArchbishop, TestEndOfElevator, Ma_TestEndOfElevatorPy) \
	PORT_DEFL(temple, Mission_MeetArchbishop, CreateFakeSpeech, Ma_CreateFakeSpeechPy) \
	PORT_DEFL(temple, Mission_MeetArchbishop, Init, Ma_InitPy) \
	PORT_DEFL(temple, Mission_TalkToDeepThroat, Init, Td_InitPy) \
	PORT_DEFL(temple, Mission_TalkToDeepThroat, DeepThroatSpeech, Td_DeepThroatSpeechPy)

/* levels/Townhall.c */
PORT_DECLL(townhall, Map, InitStatus) /* 1 */
PORT_DECLL(townhall, Map, CreateGearMayor) /* 1 */
PORT_DECLL(townhall, Map, Init) /* 1 */
PORT_DECLL(townhall, Map, OrbitLockMsg) /* 4 */
PORT_DECLL(townhall, Mission_ApologizeMayor, InitStatus) /* 1 */
PORT_DECLL(townhall, Mission_ApologizeMayor, AfterBrowseNPC) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, CreateMayorInit) /* 1 */
PORT_DECLL(townhall, Mission_ApologizeMayor, Init) /* 1 */
PORT_DECLL(townhall, Mission_ApologizeMayor, ForbiddenMsg) /* 0 */
PORT_DECLL(townhall, Mission_ApologizeMayor, MayorSpeech) /* 4 */
PORT_DECLL(townhall, Mission_ApologizeMayor, SetTransportMission) /* 0 */
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
	PORT_DEFL(townhall, Map, InitStatus, TownhallMap_InitStatus) \
	PORT_DEFL(townhall, Map, CreateGearMayor, TownhallMap_CreateGearMayor) \
	PORT_DEFL(townhall, Map, Init, TownhallMap_Init) \
	PORT_DEFL(townhall, Map, OrbitLockMsg, TownhallMap_OrbitLockMsg) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, InitStatus, ApologizeMayor_InitStatus) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, AfterBrowseNPC, ApologizeMayor_AfterBrowseNPC) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, CreateMayorInit, ApologizeMayor_CreateMayorInit) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, Init, ApologizeMayor_Init) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, ForbiddenMsg, ApologizeMayor_ForbiddenMsg) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, MayorSpeech, ApologizeMayor_MayorSpeech) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, SetTransportMission, ApologizeMayor_SetTransportMission) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, InitStaplerMission, ApologizeMayor_InitStaplerMission) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, StaplerSpeech, ApologizeMayor_StaplerSpeech) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, StaplerDead, ApologizeMayor_StaplerDead) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, MayorMessage, ApologizeMayor_MayorMessage) \
	PORT_DEFL(townhall, Mission_ApologizeMayor, MissionScorer, ApologizeMayor_MissionScorer) \
	PORT_DEFL(townhall, Mission_CatchTrurl, InitStatus, CatchTrurl_InitStatus) \
	PORT_DEFL(townhall, Mission_CatchTrurl, Init, CatchTrurl_Init) \
	PORT_DEFL(townhall, Mission_CatchTrurl, CreateTrurl, CatchTrurl_CreateTrurl) \
	PORT_DEFL(townhall, Mission_CatchTrurl, OnCatchTrurl, CatchTrurl_OnCatchTrurl) \
	PORT_DEFL(townhall, Mission_CatchTrurl, OnCatchTrurlAgain, CatchTrurl_OnCatchTrurlAgain) \
	PORT_DEFL(townhall, Mission_CatchTrurl, FunctionaryMissionSpeech, CatchTrurl_FunctionaryMissionSpeech) \
	PORT_DEFL(townhall, Mission_CatchTrurl, SetCallbackFuncs, CatchTrurl_SetCallbackFuncs) \
	PORT_DEFL(townhall, Mission_CatchTrurl, SpawnMap, CatchTrurl_SpawnMap) \
	PORT_DEFL(townhall, Mission_CatchTrurl, CreateMap, CatchTrurl_CreateMap) \
	PORT_DEFL(townhall, Mission_CatchTrurl, TakeMapPhoto, CatchTrurl_TakeMapPhoto) \
	PORT_DEFL(townhall, Mission_CatchTrurl, KillFunctionaries, CatchTrurl_KillFunctionaries) \
	PORT_DEFL(townhall, Mission_CatchTrurl, CatchTrurlAgain, CatchTrurl_CatchTrurlAgain) \
	PORT_DEFL(townhall, Mission_CatchTrurl, SetNextMission, CatchTrurl_SetNextMission) \
	PORT_DEFL(townhall, Mission_CatchTrurl, CGCalls, CatchTrurl_CGCalls) \
	PORT_DEFL(townhall, Mission_SheriffMayor, InitStatus, SheriffMayor_InitStatus) \
	PORT_DEFL(townhall, Mission_SheriffMayor, CreateMayor, SheriffMayor_CreateMayor) \
	PORT_DEFL(townhall, Mission_SheriffMayor, CreatePoliceBoss, SheriffMayor_CreatePoliceBoss) \
	PORT_DEFL(townhall, Mission_SheriffMayor, ReadyToTalk, SheriffMayor_ReadyToTalk) \
	PORT_DEFL(townhall, Mission_SheriffMayor, PoliceBossMissionSpeech, SheriffMayor_PoliceBossMissionSpeech) \
	PORT_DEFL(townhall, Mission_SheriffMayor, OnPay, SheriffMayor_OnPay) \
	PORT_DEFL(townhall, Mission_SheriffMayor, SetNextMission, SheriffMayor_SetNextMission) \
	PORT_DEFL(townhall, Mission_TalkToMayor, InitStatus, TalkToMayor_InitStatus) \
	PORT_DEFL(townhall, Mission_TalkToMayor, AfterBrowseNPC, TalkToMayor_AfterBrowseNPC) \
	PORT_DEFL(townhall, Mission_TalkToMayor, CreateMayorInit, TalkToMayor_CreateMayorInit) \
	PORT_DEFL(townhall, Mission_TalkToMayor, CreateBishopPorkins, TalkToMayor_CreateBishopPorkins) \
	PORT_DEFL(townhall, Mission_TalkToMayor, CreateFunctionaryTamayo, TalkToMayor_CreateFunctionaryTamayo) \
	PORT_DEFL(townhall, Mission_TalkToMayor, Init, TalkToMayor_Init) \
	PORT_DEFL(townhall, Mission_TalkToMayor, MayorSpeech, TalkToMayor_MayorSpeech) \
	PORT_DEFL(townhall, Mission_TalkToMayor, BishopSpeech, TalkToMayor_BishopSpeech) \
	PORT_DEFL(townhall, Mission_TalkToMayor, FunctionarySpeech, TalkToMayor_FunctionarySpeech) \
	PORT_DEFL(townhall, Mission_TalkToMayor, StealFunctionary, TalkToMayor_StealFunctionary) \
	PORT_DEFL(townhall, Mission_TalkToMayor, CheckTamayoMoney, TalkToMayor_CheckTamayoMoney) \
	PORT_DEFL(townhall, Mission_TalkToMayor, RestoreTamayoState, TalkToMayor_RestoreTamayoState) \
	PORT_DEFL(townhall, Mission_TalkToMayor, OnBankerSteal, TalkToMayor_OnBankerSteal) \
	PORT_DEFL(townhall, Mission_TalkToMayor, EndOfStealFunctionary, TalkToMayor_EndOfStealFunctionary) \
	PORT_DEFL(townhall, Mission_TalkToMayor, SetNextMission, TalkToMayor_SetNextMission) \
	PORT_DEFL(townhall, Mission_TalkToMayor, MissionScorer, TalkToMayor_MissionScorer)

#endif
