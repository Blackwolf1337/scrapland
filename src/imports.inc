/*
 * Functions of Scrap.exe the C code calls: SD_IMPORT(name, rva) for __cdecl, SD_IMPORT_STDCALL(name, argbytes, rva)
 * for __stdcall. src/core/imports.c turns each into a thunk with the name of the original, so the prototype comes
 * from the normal header. A function that gets implemented in C moves to src/hooks.inc: the linker reports a
 * function that is in both lists.
 */

/* engine: Global/ */


SD_IMPORT(operator_delete_sized, 0x313634)


/* engine: Entities/ and the entity lists of World/ */
SD_IMPORT(cEntity_Del, 0x0ef0c0)
SD_IMPORT(cWorld_GetList, 0x209240)
SD_IMPORT(cEntityList_GetFirst, 0x1a7bc0)

/* engine: Logic/ and AI/ (include/scrap/logic.h) */
SD_IMPORT(Logic_GetLocalLogic, 0x141ac0)
SD_IMPORT(Logic_SetOnFloor, 0x151fd0)
SD_IMPORT(Logic_Flash, 0x0c9180)
SD_IMPORT(Taunts_UpdateEndTime, 0x151fb0)
SD_IMPORT(ShipRegen_Set, 0x1564c0)
SD_IMPORT(AICtrl_SendSentinelToWatch, 0x14f5f0)
SD_IMPORT(AICtrl_GearAttack, 0x14f140)
SD_IMPORT(Domination_AddZone, 0x144bf0)
SD_IMPORT(Domination_SetZoneState, 0x144db0)
SD_IMPORT(Domination_SetBatonState, 0x144d50)

/* engine: World/FX/ and effects of Entities/ */
SD_IMPORT(FX_ShipExplosion, 0x1f4d50)
SD_IMPORT(FX_CharacterConversor, 0x1b2b00)
SD_IMPORT(FX_CharacterConversion, 0x1b2920)
SD_IMPORT(FX_BishopSellLife, 0x0c4650)
SD_IMPORT(FX_MoneyTransfer, 0x1b3470)

/* engine: Actions/ */
SD_IMPORT(cActClassList_Create, 0x061f70)
SD_IMPORT(cActClassList_Get, 0x062250)
SD_IMPORT(cActClassList_Del, 0x062440)
SD_IMPORT(cActClass_CreateAction, 0x05fe90)
SD_IMPORT(cActClass_GetAction, 0x060590)
SD_IMPORT(cActClass_Var, 0x060770)
SD_IMPORT(cAction_Var, 0x064280)

/* engine: AI/ (include/scrap/ai.h) */
SD_IMPORT(CNodesWorld_ctor, 0x0a21b0)
SD_IMPORT(CNodesWorld2D_ctor, 0x0ad950)
SD_IMPORT(CNodesTrafficInDoor_ctor, 0x0a08a0)
SD_IMPORT(CNodesTraffic_ctor, 0x09b970)
SD_IMPORT(CAStar_ctor, 0x099330)
SD_IMPORT(CAStar2D_ctor, 0x09a2f0)
SD_IMPORT(cAIVehicleGroup_ctor, 0x096bf0)
SD_IMPORT(cAICharGroup_ctor, 0x080790)
SD_IMPORT(CNodesWorld_Load, 0x0ab650)
SD_IMPORT(CNodesWorld_Analize, 0x0a2da0)
SD_IMPORT(CNodesWorld_AnalizeGraph, 0x0a2840)
SD_IMPORT(CNodesWorld_GetRacePoint, 0x0a87c0)
SD_IMPORT(CNodesWorld2D_Load, 0x0b6200)
SD_IMPORT(CNodesWorld2D_AnalizeGraph, 0x0adc20)
SD_IMPORT(cAICharGroup_GetReposPos, 0x080d20)
SD_IMPORT(cAIVehicle_GetState, 0x08b740)
SD_IMPORT(cAIVehicle_SetState, 0x08dfb0)
SD_IMPORT(cAIVehicle_Setup, 0x08c040)
SD_IMPORT(cAIVehicle_GetParams1, 0x08bc20)
SD_IMPORT(cAIVehicle_GetParams2, 0x08bc40)
SD_IMPORT(cAIVehicle_GetParams3, 0x08bc60)
SD_IMPORT(cAIVehicle_GetNearestItemLife, 0x08a7d0)
SD_IMPORT(cAIVehicle_SetRotStatic, 0x08df70)
SD_IMPORT(cAIVehicle_SetInertia, 0x08df60)
SD_IMPORT(cVehicleEntity_GetOwnerSlot, 0x1085b0)
SD_IMPORT(cWalkCharEntity_GetOwnerSlot, 0x0d38b0)

/* MSVC 2015 static CRT (cdecl) */
SD_IMPORT(crt_rand, 0x323c2f)
SD_IMPORT(crt_malloc, 0x34c977)
SD_IMPORT(crt_free, 0x34c972)
SD_IMPORT(crt_realloc, 0x34c9e8)
SD_IMPORT(crt_msize, 0x34c9b5)
SD_IMPORT(crt_fprintf, 0x0bd230)
SD_IMPORT(crt_fflush, 0x3463cc)
SD_IMPORT(crt_time32, 0x345d95)
SD_IMPORT(crt_gmtime32, 0x346bb1)
SD_IMPORT(crt_strftime, 0x346fbf)
SD_IMPORT(crt_exit_now, 0x346155)
SD_IMPORT(ConsoleWin_Add, 0x20d550)
SD_IMPORT(crt_report_rangecheckfailure, 0x313d15)
SD_IMPORT(crt_fopen, 0x344c5e)
SD_IMPORT(crt_fsopen, 0x344bea)
SD_IMPORT(crt_fclose, 0x34490b)
SD_IMPORT(crt_fread, 0x347845)
SD_IMPORT(crt_fwrite, 0x344fa9)
SD_IMPORT(crt_fputc, 0x350b30)
SD_IMPORT(crt_setlocale, 0x34ff2c)
SD_IMPORT(crt_remove, 0x3588ca)
SD_IMPORT(crt_rename, 0x350089)
SD_IMPORT(DbgWin_Printf, 0x237950)
SD_IMPORT(DbgStd_Printf, 0x237830)
SD_IMPORT_STDCALL(crt_eh_vector_ctor, 20, 0x313a76)
SD_IMPORT_STDCALL(crt_eh_vector_dtor, 16, 0x313ae4)
SD_IMPORT(crt_fseek, 0x347c87)
SD_IMPORT(crt_ftell, 0x3482af)
SD_IMPORT(crt_stricmp, 0x3585b4)
SD_IMPORT(crt_strncmp, 0x321f90)
SD_IMPORT(crt_asctime, 0x34dd93)
SD_IMPORT(crt_qsort, 0x34e010)
SD_IMPORT(crt_atoi, 0x323bb1)
SD_IMPORT(crt_atof, 0x329d91)
SD_IMPORT(crt_strncat, 0x34ccc0)
SD_IMPORT(crt_stdio_common_vsprintf, 0x33b8f2)
SD_IMPORT(crt_strnicmp, 0x358696)
SD_IMPORT(crt_strlwr, 0x34cbec)
SD_IMPORT(crt_fputs, 0x34653e)
SD_IMPORT(crt_feof, 0x3482d2)
SD_IMPORT(crt_fgets, 0x3484b1)
SD_IMPORT(crt_setvbuf, 0x348665)
SD_IMPORT(crt_access, 0x350971)
SD_IMPORT(crt_stat32, 0x34a426)
SD_IMPORT(crt_exit, 0x3461a9)
SD_IMPORT(crt_findfirst32, 0x34c422)
SD_IMPORT(crt_findnext32, 0x34c44e)
SD_IMPORT(crt_memicmp, 0x3587b9)
SD_IMPORT(crt_chdir, 0x34c95c)
SD_IMPORT(crt_splitpath, 0x34b1b2)
SD_IMPORT(crt_strtok_s, 0x34a6b1)
SD_IMPORT(crt_errno, 0x34ef39)
SD_IMPORT(crt_getdrive, 0x34fde0)
SD_IMPORT(crt_chdrive, 0x34fd60)
SD_IMPORT(crt_getcwd, 0x354661)
SD_IMPORT(crt_mkdir, 0x34f392)
SD_IMPORT(crt_isalpha, 0x34f784)
SD_IMPORT(crt_isdigit, 0x34f817)
SD_IMPORT(crt_toupper, 0x34fd03)
SD_IMPORT(crt_tolower, 0x34fcd3)
SD_IMPORT(crt_strupr, 0x34cf71)
SD_IMPORT(crt_sscanf, 0x081ac0)
SD_IMPORT(crt_sprintf, 0x06cb50)
SD_IMPORT(crt_fgetc, 0x350c9f)
SD_IMPORT(crt_ungetc, 0x350d9e)
SD_IMPORT(crt_isalnum, 0x34f755)
SD_IMPORT(crt_isxdigit, 0x34f956)
SD_IMPORT(crt_iob_func, 0x3507fa)
SD_IMPORT(crt_stdio_common_vfprintf, 0x33b5da)
SD_IMPORT(crt_strcspn, 0x3506a0)
SD_IMPORT(crt_fpclass, 0x34e765)
SD_IMPORT(crt_floor, 0x345130)

/* engine: Net/ (include/scrap/net.h) */
SD_IMPORT(Net_IsServer, 0x18b280)
SD_IMPORT(Net_ModifyUsrData, 0x190830)
SD_IMPORT(Net_ServerChangeLevel, 0x18b2d0)
SD_IMPORT(Net_AddResource, 0x18e780)
SD_IMPORT(Net_InitServer, 0x18b070)
SD_IMPORT(Net_CloseServer, 0x18af10)
SD_IMPORT(Net_GetServerName, 0x18b040)
SD_IMPORT(Net_GetServerMap, 0x18b050)
SD_IMPORT(Net_GetServerPort, 0x18b060)
SD_IMPORT(Net_GetBotName, 0x18e960)
SD_IMPORT(Net_GetObjName, 0x18ea10)
SD_IMPORT(Net_Kick, 0x18b290)
SD_IMPORT(Net_InitBrowser, 0x18f900)
SD_IMPORT(Net_DoneBrowser, 0x18f8e0)
SD_IMPORT(Net_IsClient, 0x18ae40)
SD_IMPORT(Net_GetClient, 0x18ac60)
SD_IMPORT(Net_InitClient, 0x18ac70)
SD_IMPORT(Net_CloseClientMsg, 0x18aba0)
SD_IMPORT(Net_CloseClient, 0x18a8f0)
SD_IMPORT(Net_GetClientName, 0x18ac40)
SD_IMPORT(Net_GetClientPort, 0x18ac50)
SD_IMPORT(Net_SendUsrString, 0x18f4d0)
SD_IMPORT(Net_SendChatString, 0x18f210)
SD_IMPORT(Net_SendCtrlString, 0x18f230)
SD_IMPORT(Net_SendMasterString, 0x191b80)
SD_IMPORT(Net_IsMaster, 0x191d00)
SD_IMPORT(Net_PingInetSvrs, 0x191600)
SD_IMPORT(Net_GetPing, 0x18eab0)

/* engine: Menu/ (include/scrap/scorer.h) */
SD_IMPORT(Scorer_Get, 0x170ea0)
SD_IMPORT(cScorer_FindItem, 0x170dc0)
SD_IMPORT(cScorer_AddItem, 0x171ad0)
SD_IMPORT(cScorer_Clear, 0x16fe20)
SD_IMPORT(cScorer_SetDefault, 0x1740a0)
SD_IMPORT(cScorer_SetMarkerSprite, 0x1740f0)
SD_IMPORT(cScorer_AddModel, 0x16fb50)
SD_IMPORT(ScorerFade_Start, 0x170cb0)
SD_IMPORT(ScorerCinema_Set, 0x173ff0)
SD_IMPORT(Scorer_AddChatMsg, 0x164ca0)
SD_IMPORT(Scorer_SetConsole, 0x1749d0)
SD_IMPORT(Scorer_SetSpeechText, 0x181420)
SD_IMPORT(Scorer_SetSpeechCallback, 0x181350)
SD_IMPORT(Scorer_CancelSpeech, 0x181330)
SD_IMPORT(Scorer_SetHeadMonitor, 0x16b3d0)
SD_IMPORT(Scorer_SetMsgText, 0x1603b0)
SD_IMPORT(Scorer_SetLabelText, 0x160380)
SD_IMPORT(Scorer_PreloadTexture, 0x172500)
SD_IMPORT(Engine_WorldToScreen, 0x216d60)
SD_IMPORT(Font_Get, 0x21ad30)
SD_IMPORT(Font_TextSize, 0x219f30)

/* engine: functions behind the Scrap module (include/scrap/scrap.h) */
SD_IMPORT(Elements_Call, 0x202ea0)
SD_IMPORT(Elements_Create, 0x203000)
SD_IMPORT(Floor_Get, 0x203af0)
SD_IMPORT(Alarm_GetValue, 0x13fb30)
SD_IMPORT(Alarm_IsLocked, 0x13fb60)
SD_IMPORT(cWorld_NewEntityList, 0x209a00)
SD_IMPORT(cWorld_GetCamName, 0x209220)
SD_IMPORT(cWorld_GetFirstEntity, 0x2092a0)
SD_IMPORT(Achievement_Unlock, 0x137470)
SD_IMPORT(Steam_SetAchievement, 0x1a66a0)
SD_IMPORT(Steam_GetID, 0x134720)
SD_IMPORT(Steam_GetTicket, 0x1345e0)
SD_IMPORT(Gog_GetID, 0x137160)
SD_IMPORT(Font_Build, 0x21ab80)
SD_IMPORT(Loading_Init, 0x20b260)
SD_IMPORT(Loading_End, 0x20ade0)
SD_IMPORT(Video_GetCurrentMode, 0x215cb0)
SD_IMPORT(Models_List, 0x217f30)
SD_IMPORT(Anm_Preload, 0x2051a0)
SD_IMPORT(DVF_Process, 0x2175d0)
SD_IMPORT(Screenshot_Take, 0x2178b0)
SD_IMPORT(Video_SetCurrentMode, 0x217c60)
SD_IMPORT(Video_Play, 0x218010)
SD_IMPORT(Music_Play, 0x216ae0)
SD_IMPORT(Model_Info, 0x217e20)
SD_IMPORT(DummySearch_Start, 0x1a7ad0)
SD_IMPORT(MissionArrows_Switch, 0x0f82a0)
SD_IMPORT(CWinApp_LoadLevel, 0x20b8a0)
SD_IMPORT(cWorld_SetCam, 0x209b30)
SD_IMPORT(cWorld_GetUsrEntity, 0x0c1430)
SD_IMPORT(Alarm_Set, 0x13fe60)
SD_IMPORT(Alarm_SetChar, 0x13ff20)
SD_IMPORT(Alarm_SetGrow, 0x13fd50)
SD_IMPORT(Alarm_SetLock, 0x13fe50)
SD_IMPORT(App_GetPath, 0x20d100)
SD_IMPORT(App_GetConfigDir, 0x20d570)
SD_IMPORT(cWorld_CreateEntity, 0x208360)
SD_IMPORT(cWorld_GetMinCamDistSq, 0x209350)
SD_IMPORT(Grid_Set, 0x1a7710)
SD_IMPORT(Parking_AddZone, 0x155700)
SD_IMPORT(Parking_Find, 0x155bf0)
SD_IMPORT(cEntityList_Var, 0x1a7d70)
SD_IMPORT(Video_GetModes, 0x215d20)
SD_IMPORT(Object3D_Preload, 0x205140)
SD_IMPORT(cWorld_GetDebrisSys, 0x209590)
SD_IMPORT(cWorld_DeleteDebrisSys, 0x208c20)
SD_IMPORT(cDebrisSys_Init, 0x204d50)
SD_IMPORT(cDebrisSys_NewElement, 0x204bd0)
SD_IMPORT(cDebrisSys_Drop, 0x204a40)
SD_IMPORT(EntityLists_Get, 0x1a7170)
SD_IMPORT(Engine_TestLine, 0x213150)
SD_IMPORT(Engine_TestSphere, 0x213d60)
SD_IMPORT(cWorld_TestLineEntities, 0x2098f0)
SD_IMPORT(cWorld_TestSphereEntities, 0x20a320)
SD_IMPORT(cWorld_SphereCall, 0x20a290)
SD_IMPORT(DummySearch_Next, 0x1a7a00)

/* engine: Input/ */
SD_IMPORT(ActionSet_IsNamed, 0x133a30)
SD_IMPORT(Input_SelectActionSet, 0x13a560)
SD_IMPORT(Input_GetActionSet, 0x139910)
SD_IMPORT(Input_AssignEntry, 0x13abb0)
SD_IMPORT(Input_GetEntry, 0x13b800)
SD_IMPORT(Input_ListenToDefine, 0x13ba40)
SD_IMPORT(Input_AbortListenToDefine, 0x13ab10)
SD_IMPORT(Input_GetDefinedEntry, 0x13b420)
SD_IMPORT(Input_GetDefinedList, 0x13b680)
SD_IMPORT(Input_ClearDefinedList, 0x13b120)
SD_IMPORT(Input_ResetToDefault, 0x13c040)
SD_IMPORT(Input_Bind, 0x13adc0)
SD_IMPORT(cPlayerInput_SetString, 0x13a8a0)
SD_IMPORT(cPlayerInput_SetInputFunc, 0x13a800)
SD_IMPORT(cPlayerInput_GetCursorChar, 0x139920)
SD_IMPORT(cVirtualKeyboard_Set, 0x13a720)
SD_IMPORT(cPad_Rumble, 0x13a820)

/* engine: Sound/ */
SD_IMPORT(Snd_Volume, 0x1a45d0)
SD_IMPORT(cSoundDesc_Init, 0x1a3230)
SD_IMPORT(cSoundDesc_SetPos, 0x1a3f40)
SD_IMPORT(SndPS_Get, 0x1a0f70)
SD_IMPORT(SndPS_Create, 0x1a0a90)
SD_IMPORT(SndPS_Use, 0x1a1e60)
SD_IMPORT(cSoundPS_Var, 0x1a1660)

/* engine: Math/ */

/* engine: World/ (weapon and ammo tables of cWorld) */
SD_IMPORT(WeapTable_Init, 0x19fd40)
SD_IMPORT(AmmoTable_Init, 0x19fc60)
SD_IMPORT(cWorld_FillWeapPriority, 0x208d60)
SD_IMPORT(cWeapInfo_GetF, 0x206520)
SD_IMPORT(cWeapInfo_GetS, 0x206750)
SD_IMPORT(cWeapInfo_SetF, 0x2068f0)
SD_IMPORT(cWeapInfo_SetS, 0x206b00)
SD_IMPORT(cAmmoInfo_GetF, 0x2064c0)
SD_IMPORT(cAmmoInfo_GetS, 0x206680)
SD_IMPORT(cAmmoInfo_SetF, 0x206890)
SD_IMPORT(cAmmoInfo_SetS, 0x206a20)

/* engine: Entities/Vehicle/Weaps/ (network creation of missiles) */
SD_IMPORT(Swarm_NetCreate, 0x1261e0)
SD_IMPORT(Swarm_NetSetUp, 0x127970)
SD_IMPORT(Inferno_NetCreate, 0x1211f0)
SD_IMPORT(Inferno_NetSetUp, 0x122940)

/* Python 1.5.2 */
SD_IMPORT(PyArg_ParseTuple, 0x2f0560)	/* getargs.c */
SD_IMPORT(Py_BuildValue, 0x2f1b90)	/* modsupport.c */
SD_IMPORT(Py_InitModule4, 0x2f1bb0)	/* modsupport.c */
SD_IMPORT(PyErr_Clear, 0x2efd30)	/* errors.c */
SD_IMPORT(PyTuple_New, 0x2decb0)	/* tupleobject.c */
SD_IMPORT(PyTuple_SetItem, 0x2ded50)	/* tupleobject.c */
SD_IMPORT(PyFloat_FromDouble, 0x2dfd70)	/* floatobject.c */
SD_IMPORT(PyList_Size, 0x2dd2b0)	/* listobject.c */
SD_IMPORT(PyList_New, 0x2dd040)	/* listobject.c */
SD_IMPORT(PyTuple_GetItem, 0x2dec30)	/* tupleobject.c */
SD_IMPORT(PyInt_AsLong, 0x2de3a0)	/* intobject.c */
SD_IMPORT(PyList_GetItem, 0x2dcf80)	/* listobject.c */
SD_IMPORT(PyString_AsString, 0x2dadd0)	/* stringobject.c */
SD_IMPORT(PyArg_Parse, 0x2f0530)	/* getargs.c */
SD_IMPORT(Py_FindMethod, 0x2f29d0)	/* methodobject.c */
SD_IMPORT(_PyObject_New, 0x2ebc00)	/* object.c */
SD_IMPORT(PyList_AsTuple, 0x2dcf10)	/* listobject.c */
SD_IMPORT(PyList_Append, 0x2dcee0)	/* listobject.c */
SD_IMPORT(PyList_SetItem, 0x2dd1e0)	/* listobject.c */
SD_IMPORT(PyInt_FromLong, 0x2de630)	/* intobject.c */
SD_IMPORT(PyString_FromString, 0x2dbb80)	/* stringobject.c */
SD_IMPORT(PyEval_CallObject, 0x2ee850)	/* ceval.c */
SD_IMPORT(PyErr_Print, 0x2ebd00)	/* pythonrun.c */
SD_IMPORT(PyImport_AddModule, 0x2e1ba0)	/* import.c */
SD_IMPORT(PyModule_GetDict, 0x2e1240)	/* moduleobject.c */
SD_IMPORT(PyDict_GetItemString, 0x2e0930)	/* dictobject.c */
SD_IMPORT(PyCallable_Check, 0x2eb1f0)	/* object.c */
SD_IMPORT(Py_Initialize, 0x2ecde0)	/* pythonrun.c */
SD_IMPORT(Py_Finalize, 0x2ecd20)	/* pythonrun.c */
SD_IMPORT(PyRun_ScrapString, 0x2ec790)	/* pythonrun.c, changed by the engine */
SD_IMPORT(Py_FiniObjects, 0x2ebce0)
SD_IMPORT(PyImport_ImportModule, 0x2e25d0)	/* import.c */
SD_IMPORT(PyImport_GetModuleDict, 0x2e2300)	/* import.c */
SD_IMPORT(PyImport_ExecCodeModuleEx, 0x2e2100)	/* import.c */
SD_IMPORT(PyMarshal_ReadObjectFromString, 0x2e9c30)	/* marshal.c */
SD_IMPORT(PySys_GetObject, 0x2f4390)	/* sysmodule.c */
SD_IMPORT(PyFile_WriteString, 0x2f3ee0)	/* fileobject.c */

/* engine: FileSys/ (include/scrap/filesys.h) */
SD_IMPORT(cStringList_NewNode, 0x0605f0)
SD_IMPORT(Loading_Step, 0x2157a0)
SD_IMPORT(Console_Execute, 0x20aaf0)

/* engine3d: Engine/ListaVert.cpp and Engine/render.cpp (include/scrap/listavert.h, mallatri.h) */
SD_IMPORT(CListaVert_GetPos, 0x212570)

/* engine3d: called from Engine/luz.cpp (include/scrap/luz.h) */
SD_IMPORT(E3D_SetShadowLight, 0x2682d0)
SD_IMPORT(Prof_Count, 0x273040)
SD_IMPORT(CSuelo_Trace, 0x26e490)
/* engine3d: Engine/Material.c (include/scrap/material.h), the parts of the classes outside Material.cpp and the
 * texture manager */
SD_IMPORT(CMapa_ctor, 0x233760)
SD_IMPORT(CMapa_dtor, 0x233a30)
SD_IMPORT(CMaterial_ctor, 0x233830)
SD_IMPORT(CMaterial_dtor, 0x233af0)
SD_IMPORT(Cad_Set, 0x2a1940)
SD_IMPORT(FXTextura_Create, 0x2a81d0)
SD_IMPORT(CMatId_Check, 0x2a7a00)
SD_IMPORT(CMatId_Register, 0x2a7ae0)
SD_IMPORT(CAnimMat_ctor, 0x257f70)
SD_IMPORT(CAnimMat_dtor, 0x257fb0)
SD_IMPORT(CAnimMat_Start, 0x258110)
SD_IMPORT(Vec3_Differ, 0x067380)

/* src/game/engine3d/Engine/Escena.c (called from Engine/escena.cpp, include/scrap/escena.h) */
SD_IMPORT(Box_Transform, 0x239880)
SD_IMPORT(Cad_Read, 0x2a18a0)
SD_IMPORT(Cad_Write, 0x2a1920)
SD_IMPORT(CAnimacion_ctor, 0x2ab6f0)
SD_IMPORT(CAnimacion_dtor, 0x2ab940)
SD_IMPORT(CMallaD3D_CalcPesos, 0x2b8900)
SD_IMPORT(CMallaD3D_CalcIlum, 0x2b8e80)
SD_IMPORT(CSuelo_CalcIlum, 0x261630)
SD_IMPORT(NodeFX_New, 0x2b2cc0)

/* src/game/engine3d/Engine/SistPart.c (called from Engine/SistPart.cpp, include/scrap/sistpart.h) */
SD_IMPORT(CObjBase3D_SetNodo, 0x2cad70)
SD_IMPORT(CMultiSprite_GetUV, 0x29d800)
SD_IMPORT(CSuelo_TraceSeg, 0x26f120)
SD_IMPORT(Debug_Text3D, 0x299d50)
SD_IMPORT(SistPart_Draw, 0x2b7150)
SD_IMPORT(SistPart_Draw2, 0x2b55d0)

/* src/game/engine3d/Engine/SueloPortal.c (called from Engine/SueloPortal.cpp, include/scrap/sueloportal.h) */
SD_IMPORT(CSuelo_RayoVert, 0x2ca360)
SD_IMPORT(Portales_DebugDraw, 0x26d0d0)

/* src/game/engine3d/Engine/Mundo.c (called from Engine/mundo.cpp, include/scrap/mundo.h) */
SD_IMPORT(Color_ToD3D, 0x214f70)
SD_IMPORT(Bsp_Free, 0x290520)
SD_IMPORT(Bsp_ReleaseD3D, 0x286c80)
SD_IMPORT(Bsp_InitD3D, 0x286cb0)
SD_IMPORT(Bsp_Crear, 0x291000)
SD_IMPORT(Bsp_Render, 0x296cd0)
SD_IMPORT(BspQ3_Free, 0x2c9d50)
SD_IMPORT(BspQ3_ReleaseD3D, 0x2c98a0)
SD_IMPORT(BspQ3_InitD3D, 0x2c98b0)
SD_IMPORT(BspQ3_Crear, 0x2c9d80)
SD_IMPORT(BspQ3_Render, 0x2c9d90)
SD_IMPORT(Sectores_Crear, 0x26d5a0)
SD_IMPORT(Portales_SetView, 0x26db80)
SD_IMPORT(CSuelo_FreeNodos, 0x261530)
SD_IMPORT(Font_PrintfXY, 0x299ce0)

/* src/game/engine3d/Engine/Render.c (called from Engine/render.cpp, include/scrap/render.h) */
SD_IMPORT(RenderLock_InitD3D, 0x2a48f0)
SD_IMPORT(ListaVert_InitD3D, 0x2404f0)
SD_IMPORT(RenderEVA_InitD3D, 0x2b4320)
SD_IMPORT(RenderTarget_InitD3D, 0x220d20)
SD_IMPORT(ModelFX_InitD3D, 0x2b8570)
SD_IMPORT(RenderEVA_DrawSkin, 0x2b5d90)
SD_IMPORT(RenderEVA_DrawSkinNodo, 0x2b4660)
SD_IMPORT(Stencil_Malla, 0x267530)
SD_IMPORT(CListaVert_EnvMapUV, 0x2418c0)
SD_IMPORT(CAnimMat_GetFrame, 0x258060)
SD_IMPORT(Matrix_Invert, 0x239560)
SD_IMPORT(RenderLock_ReleaseD3D, 0x2a4860)
SD_IMPORT(ListaVert_ReleaseD3D, 0x2404e0)
SD_IMPORT(RenderEVA_ReleaseD3D, 0x2b4550)
SD_IMPORT(RenderTarget_ReleaseD3D, 0x220c60)
SD_IMPORT(RenderGamma_ReleaseD3D, 0x2a7980)
SD_IMPORT(RenderGamma_Begin, 0x2a7520)
SD_IMPORT(RenderGamma_LogShaderMask, 0x2a7cd0)
SD_IMPORT(Vec3_ToAngles, 0x239150)
SD_IMPORT(Matrix_RotationZ, 0x23a2b0)
SD_IMPORT(RenderMallaD3D_Flush, 0x2b6af0)
SD_IMPORT(RenderMallaD3D_Flush2, 0x2b6e60)
SD_IMPORT(RenderObjBase3D_Flush, 0x2b7260)

/* src/game/engine3d/Engine/Grafico2D.c (called from Engine/grafico2d.cpp, include/scrap/grafico2d.h) */

/* src/game/engine3d/Engine/Texto.c (called from Engine/texto.cpp, include/scrap/texto.h) */
SD_IMPORT(Eng_Printf, 0x2170d0)

/* src/game/engine3d/Engine/UtilD3D.c (called from Engine/util_d3d.cpp, include/scrap/utild3d.h) */

/* src/game/engine3d/Engine/Grafico3D.c (called from Engine/grafico3d.cpp, include/scrap/grafico3d.h) */
SD_IMPORT(CAnimMat_ctorCopy, 0x257f10)

/* src/game/engine3d/Engine/Portal.c (called from Engine/portal.cpp, include/scrap/portal.h) */
SD_IMPORT(CPortal_DrawDebug, 0x2ba2b0)
SD_IMPORT(CPortal_ClipRect, 0x2ba930)
SD_IMPORT(Debug_Line, 0x27cb80)

/* module agent: Engine/RenderTarget (imports below this line) */


/* module agent: Engine/MallaD3D (imports below this line) */
SD_IMPORT(Plane_Calc, 0x286fb0)
SD_IMPORT(MallaD3D_Render, 0x2b67a0)
SD_IMPORT(CMallaD3D_CalcSegmentos, 0x2d5350)

/* module agent: Engine/Suelo (imports below this line) */
SD_IMPORT(CCollideMesh_dtor, 0x20eb70)
SD_IMPORT(Vec3_Normalize, 0x261580)
SD_IMPORT(crt_ceil, 0x345010)
SD_IMPORT(BoxTri, 0x271d60)
SD_IMPORT(Tabla_ResizeC, 0x261830)
SD_IMPORT(Tabla_Resize1C, 0x261780)
SD_IMPORT(Batch_AddMalla, 0x2c81d0)
SD_IMPORT(Batch_Flush, 0x2c83c0)
SD_IMPORT(CSuelo_DrawDebug, 0x25a260)

/* module agent: Engine/Trails (imports below this line) */
SD_IMPORT_STDCALL(D3DX_Vec3Normalize, 8, 0x006d3b)
SD_IMPORT_STDCALL(D3DX_Vec3Transform, 12, 0x007086)
SD_IMPORT_STDCALL(D3DX_MatrixRotationYPR, 16, 0x008701)
SD_IMPORT_STDCALL(D3DX_MatrixInverse, 12, 0x007cea)
SD_IMPORT_STDCALL(D3DX_AssembleShader, 24, 0x00b2c2)

/* module agent: Engine/Modelo (imports below this line) */
SD_IMPORT(World_TimeScale, 0x20aea0)
SD_IMPORT(Matrix_PostScale, 0x239210)
SD_IMPORT(Matrix_PostRotate, 0x23a100)
SD_IMPORT(CNodoR_dtor, 0x23f450)
SD_IMPORT(CNodoR_SetNodo, 0x23f890)
SD_IMPORT(RenderCtx_Init, 0x23fac0)
SD_IMPORT(RenderCtx_SetCanal, 0x23fb10)
SD_IMPORT(CEscena_Shared, 0x24ecb0)
SD_IMPORT(CModelo_Render, 0x253610)
SD_IMPORT(Font_SetColor, 0x299980)
SD_IMPORT(Font_Printf, 0x29b850)
SD_IMPORT(SM3_Load, 0x2bc5c0)
SD_IMPORT(Escena_PostLoad, 0x2c0e40)
SD_IMPORT(Escena_Load, 0x2c2d60)

/* module agent: Engine/Nodo3D (imports below this line) */
SD_IMPORT(Matrix_LoadIdentity, 0x214710)
SD_IMPORT(Matrix_MulD3D, 0x2391f0)
SD_IMPORT(Matrix_ToEuler, 0x2392d0)
SD_IMPORT(CRotacion_SetEuler, 0x212510)
SD_IMPORT(CSistPart_Cast, 0x23f310)
SD_IMPORT(CNodo3D_DebugDraw, 0x23d9d0)
SD_IMPORT(CNodoR_UpdateNodo, 0x23f5d0)
SD_IMPORT(CNodoR_ApplyFX, 0x23f540)

/* module agent: Engine/Animacion (imports below this line) */
SD_IMPORT(CEVA_ctor, 0x2d33e0)

/* module agent: Engine/AnimGrupo (imports below this line) */
SD_IMPORT(CAnimCtrl_dtor, 0x250f10)

/* module agent: Engine/Textura (imports below this line) */
SD_IMPORT(CMultiSprite_ctor, 0x29cf80)
SD_IMPORT(CMultiSprite_dtor, 0x29cfb0)
SD_IMPORT(CMultiSprite_Build, 0x29cfe0)
SD_IMPORT(crt_mbsrchr, 0x3508c4)
SD_IMPORT(crt_spawnlp, 0x34f2e0)
SD_IMPORT_STDCALL(D3DX_CheckTextureRequirements, 28, 0x0037ea)
SD_IMPORT_STDCALL(D3DX_CreateTextureFromFileInMemoryEx, 60, 0x0062a7)
SD_IMPORT_STDCALL(D3DX_LoadSurfaceFromSurface, 32, 0x004dbf)
