/*
 * Functions of Scrap.exe the C code calls: SD_IMPORT(name, rva) for __cdecl, SD_IMPORT_STDCALL(name, argbytes, rva)
 * for __stdcall. src/core/imports.c turns each into a thunk with the name of the original, so the prototype comes
 * from the normal header. A function that gets implemented in C moves to src/hooks.inc: the linker reports a
 * function that is in both lists.
 */

/* engine: Global/ */


SD_IMPORT(operator_delete_sized, 0x313634)


/* engine: Entities/ and the entity lists of World/ */

/* engine: Logic/ and AI/ (include/scrap/logic.h) */
SD_IMPORT(Logic_SetOnFloor, 0x151fd0)
SD_IMPORT(Taunts_UpdateEndTime, 0x151fb0)
SD_IMPORT(ShipRegen_Set, 0x1564c0)
SD_IMPORT(AICtrl_SendSentinelToWatch, 0x14f5f0)
SD_IMPORT(AICtrl_GearAttack, 0x14f140)

/* engine: World/FX/ and effects of Entities/ */
SD_IMPORT(FX_ShipExplosion, 0x1f4d50)

/* engine: Actions/ */
SD_IMPORT(cActClassList_Create, 0x061f70)
SD_IMPORT(cActClassList_Get, 0x062250)
SD_IMPORT(cActClassList_Del, 0x062440)
SD_IMPORT(cActClass_CreateAction, 0x05fe90)
SD_IMPORT(cActClass_GetAction, 0x060590)
SD_IMPORT(cActClass_Var, 0x060770)
SD_IMPORT(cAction_Var, 0x064280)

/* engine: AI/ (include/scrap/ai.h) */
SD_IMPORT(CAStar_ctor, 0x099330)
SD_IMPORT(CAStar2D_ctor, 0x09a2f0)
SD_IMPORT(cAICharGroup_ctor, 0x080790)
SD_IMPORT(cAICharGroup_GetReposPos, 0x080d20)

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

/* engine: functions behind the Scrap module (include/scrap/scrap.h) */
SD_IMPORT(Elements_Call, 0x202ea0)
SD_IMPORT(Elements_Create, 0x203000)
SD_IMPORT(Floor_Get, 0x203af0)
SD_IMPORT(Achievement_Unlock, 0x137470)
SD_IMPORT(Steam_SetAchievement, 0x1a66a0)
SD_IMPORT(Steam_GetID, 0x134720)
SD_IMPORT(Steam_GetTicket, 0x1345e0)
SD_IMPORT(Gog_GetID, 0x137160)
SD_IMPORT(Anm_Preload, 0x2051a0)
SD_IMPORT(Parking_AddZone, 0x155700)
SD_IMPORT(Parking_Find, 0x155bf0)
SD_IMPORT(Object3D_Preload, 0x205140)
SD_IMPORT(cDebrisSys_Init, 0x204d50)
SD_IMPORT(cDebrisSys_NewElement, 0x204bd0)
SD_IMPORT(cDebrisSys_Drop, 0x204a40)

/* engine: Input/ */
SD_IMPORT(ActionSet_IsNamed, 0x133a30)
SD_IMPORT(Input_GetActionSet, 0x139910)
SD_IMPORT(cPlayerInput_GetCursorChar, 0x139920)

/* engine: Math/ */

/* engine: World/ (weapon and ammo tables of cWorld) */
SD_IMPORT(WeapTable_Init, 0x19fd40)
SD_IMPORT(AmmoTable_Init, 0x19fc60)

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

/* engine3d: Engine/ListaVert.cpp and Engine/render.cpp (include/scrap/listavert.h, mallatri.h) */

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
SD_IMPORT(SistPart_Draw, 0x2b7150)
SD_IMPORT(SistPart_Draw2, 0x2b55d0)

/* src/game/engine3d/Engine/SueloPortal.c (called from Engine/SueloPortal.cpp, include/scrap/sueloportal.h) */
SD_IMPORT(CSuelo_RayoVert, 0x2ca360)
SD_IMPORT(Portales_DebugDraw, 0x26d0d0)

/* src/game/engine3d/Engine/Mundo.c (called from Engine/mundo.cpp, include/scrap/mundo.h) */
SD_IMPORT(BspQ3_Free, 0x2c9d50)
SD_IMPORT(BspQ3_ReleaseD3D, 0x2c98a0)
SD_IMPORT(BspQ3_InitD3D, 0x2c98b0)
SD_IMPORT(BspQ3_Crear, 0x2c9d80)
SD_IMPORT(BspQ3_Render, 0x2c9d90)
SD_IMPORT(Sectores_Crear, 0x26d5a0)
SD_IMPORT(Portales_SetView, 0x26db80)
SD_IMPORT(CSuelo_FreeNodos, 0x261530)

/* src/game/engine3d/Engine/Render.c (called from Engine/render.cpp, include/scrap/render.h) */
SD_IMPORT(RenderLock_InitD3D, 0x2a48f0)
SD_IMPORT(ListaVert_InitD3D, 0x2404f0)
SD_IMPORT(RenderEVA_InitD3D, 0x2b4320)
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

/* src/game/engine3d/Engine/UtilD3D.c (called from Engine/util_d3d.cpp, include/scrap/utild3d.h) */

/* src/game/engine3d/Engine/Grafico3D.c (called from Engine/grafico3d.cpp, include/scrap/grafico3d.h) */
SD_IMPORT(CAnimMat_ctorCopy, 0x257f10)

/* src/game/engine3d/Engine/Portal.c (called from Engine/portal.cpp, include/scrap/portal.h) */
SD_IMPORT(CPortal_DrawDebug, 0x2ba2b0)
SD_IMPORT(CPortal_ClipRect, 0x2ba930)
SD_IMPORT(Debug_Line, 0x27cb80)

/* module agent: Engine/RenderTarget (imports below this line) */


/* module agent: Engine/MallaD3D (imports below this line) */
SD_IMPORT(MallaD3D_Render, 0x2b67a0)
SD_IMPORT(CMallaD3D_CalcSegmentos, 0x2d5350)

/* module agent: Engine/Suelo (imports below this line) */
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
SD_IMPORT(Matrix_PostScale, 0x239210)
SD_IMPORT(Matrix_PostRotate, 0x23a100)
SD_IMPORT(CNodoR_dtor, 0x23f450)
SD_IMPORT(CNodoR_SetNodo, 0x23f890)
SD_IMPORT(RenderCtx_Init, 0x23fac0)
SD_IMPORT(RenderCtx_SetCanal, 0x23fb10)
SD_IMPORT(CEscena_Shared, 0x24ecb0)
SD_IMPORT(CModelo_Render, 0x253610)
SD_IMPORT(Escena_Load, 0x2c2d60)
SD_IMPORT(CNodoR_SetLook, 0x23f7f0)

/* module agent: Engine/Nodo3D (imports below this line) */
SD_IMPORT(Matrix_MulD3D, 0x2391f0)
SD_IMPORT(Matrix_ToEuler, 0x2392d0)
SD_IMPORT(CSistPart_Cast, 0x23f310)
SD_IMPORT(CNodo3D_DebugDraw, 0x23d9d0)
SD_IMPORT(CNodoR_UpdateNodo, 0x23f5d0)
SD_IMPORT(CNodoR_ApplyFX, 0x23f540)

/* module agent: Engine/Animacion (imports below this line) */
SD_IMPORT(CEVA_ctor, 0x2d33e0)

/* module agent: Engine/AnimGrupo (imports below this line) */
SD_IMPORT(CAnimCtrl_dtor, 0x250f10)

/* module agent: Engine/Textura (imports below this line) */
SD_IMPORT(crt_mbsrchr, 0x3508c4)
SD_IMPORT(crt_spawnlp, 0x34f2e0)
SD_IMPORT_STDCALL(D3DX_CheckTextureRequirements, 28, 0x0037ea)
SD_IMPORT_STDCALL(D3DX_CreateTextureFromFileInMemoryEx, 60, 0x0062a7)
SD_IMPORT_STDCALL(D3DX_LoadSurfaceFromSurface, 32, 0x004dbf)

/* src/game/ScrapEng/ScrapEngCollision.c (called from suelo.cpp and checkbbox.cpp, include/scrap/suelo.h) */
SD_IMPORT(CSuelo_TestSeg, 0x26ed30)
SD_IMPORT(CSuelo_TraceSegPos, 0x26df80)
SD_IMPORT(CSuelo_TraceSeg, 0x26f120)
SD_IMPORT(CSuelo_CheckBBox, 0x26fb10)

/* module agent: World/World.cpp (imports below this line) */
SD_IMPORT(cWithLifeEntity_ctor, 0x12b580)
SD_IMPORT(Logic_ctor, 0x151900)
SD_IMPORT(Logic_dtor, 0x151a90)
SD_IMPORT(Logic_Reset, 0x151d00)
SD_IMPORT(Logic_Update, 0x151dc0)
SD_IMPORT(Unk2078_ctor, 0x1540f0)
SD_IMPORT(Unk2078_Update, 0x154170)
SD_IMPORT(Elements_ctor, 0x202e10)
SD_IMPORT(Elements_dtor, 0x202e40)
SD_IMPORT(Elements_Update, 0x203550)
SD_IMPORT(Floor_ctor, 0x203580)
SD_IMPORT(Floor_dtor, 0x203a00)
SD_IMPORT(cDebrisSys_ctor, 0x2048d0)
SD_IMPORT(cDebrisSys_dtor, 0x204950)
SD_IMPORT(cDebrisSys_FreeActive, 0x204b30)
SD_IMPORT(cDebrisSys_FreeIdle, 0x204b80)
SD_IMPORT(cDebrisSys_ShowInfo, 0x205210)
SD_IMPORT(cDebrisSys_SetName, 0x205450)
SD_IMPORT(cDebrisSys_Update, 0x205490)
SD_IMPORT(cScheduledFuncStack_dtor, 0x2056f0)

/* module agent: ScrapEng/ScrapEngWorld.cpp (imports below this line) */

/* module agent: ScrapEng/ScrapEngMain.cpp (imports below this line) */
SD_IMPORT(Engine3D_Create, 0x22da30)
SD_IMPORT(Engine3D_Shutdown, 0x22dc00)
SD_IMPORT(CD3DApp_Cleanup, 0x273d90)
SD_IMPORT(CD3DApp_Render, 0x275120)
SD_IMPORT(CD3DApp_ResetDevice, 0x275470)
SD_IMPORT(CD3DApp_Reset, 0x2754b0)
SD_IMPORT(Timer_GetFreq, 0x276cb0)
SD_IMPORT(Timer_GetQPFreq, 0x276d00)
SD_IMPORT(Video_PlayFile, 0x281660)
SD_IMPORT(Avi_Save, 0x27fa20)
SD_IMPORT(Avi_Frame, 0x27f9f0)
SD_IMPORT(Avi_Load, 0x27f9b0)
SD_IMPORT(SombrasStencil_Set, 0x267460)
SD_IMPORT(CPsaImage_ctor, 0x22d120)
SD_IMPORT(CPsaImage_dtor, 0x22d160)
SD_IMPORT(CPsaImage_Draw, 0x22d3c0)
SD_IMPORT(ViewerCamera_Move, 0x218080)
SD_IMPORT(Cmd_ReadFile, 0x280b10)

/* module agent: ScrapEng/ScrapEngCamera+Particles+Trails.cpp (imports below this line) */
SD_IMPORT(Matrix_RotationX, 0x23a270)

/* module agent: Sound/Sound+SoundDX8+Wav.cpp (imports below this line) */
SD_IMPORT(Sound_UpdateVoice, 0x1a0320)
SD_IMPORT(DebugUtils_DrawSphere, 0x219d20)
SD_IMPORT_STDCALL(FSOUND_GetError, 0, 0x3179c6)
SD_IMPORT_STDCALL(FSOUND_GetMaxChannels, 0, 0x3179cc)
SD_IMPORT_STDCALL(FSOUND_GetMemoryStats, 8, 0x3179ea)
SD_IMPORT_STDCALL(FSOUND_Sample_Load, 20, 0x3179f0)
SD_IMPORT_STDCALL(FSOUND_Sample_Free, 4, 0x3179f6)
SD_IMPORT_STDCALL(FSOUND_Sample_GetDefaults, 20, 0x317a02)
SD_IMPORT_STDCALL(FSOUND_Sample_GetMode, 4, 0x317a08)
SD_IMPORT_STDCALL(FSOUND_PlaySoundEx, 16, 0x317a0e)
SD_IMPORT_STDCALL(FSOUND_StopSound, 4, 0x317a14)
SD_IMPORT_STDCALL(FSOUND_SetFrequency, 8, 0x317a1a)
SD_IMPORT_STDCALL(FSOUND_SetVolume, 8, 0x317a20)
SD_IMPORT_STDCALL(FSOUND_SetPan, 8, 0x317a26)
SD_IMPORT_STDCALL(FSOUND_SetPaused, 8, 0x317a2c)
SD_IMPORT_STDCALL(FSOUND_SetLoopMode, 8, 0x317a32)
SD_IMPORT_STDCALL(FSOUND_IsPlaying, 4, 0x317a38)
SD_IMPORT_STDCALL(FSOUND_GetReserved, 4, 0x317a56)
SD_IMPORT_STDCALL(FSOUND_GetCurrentSample, 4, 0x317a62)

/* module agent: World/CollisionGrid+FX/FXMisc.cpp (imports below this line) */
SD_IMPORT(cObjElement_dtor, 0x204940)
SD_IMPORT(cDebrisSys_GetIdle, 0x204d40)
SD_IMPORT(cDebrisSys_IsIdle, 0x204dd0)
SD_IMPORT(cDebris_Move, 0x204de0)
SD_IMPORT(CGameLensFlare_ctor, 0x232e80)
SD_IMPORT(CGameLensFlare_dtor, 0x232f10)
SD_IMPORT(Vec3_Angle, 0x066ad0)
SD_IMPORT(Vec3_NormalizeTo, 0x072340)

/* module agent: App/CApp+CWinApp (imports below this line) */
SD_IMPORT(ActClassList_Done, 0x062140)
SD_IMPORT(ActClassList_Init, 0x0623a0)
SD_IMPORT(GogListeners_ctor, 0x136f10)
SD_IMPORT(Gog_GetLanguage, 0x137140)
SD_IMPORT(Gog_Init, 0x137250)
SD_IMPORT(Gog_Done, 0x137430)
SD_IMPORT(Gog_ProcessData, 0x137570)
SD_IMPORT(PtrVector_Deallocate, 0x137c30)
SD_IMPORT(InputMaps_Done, 0x139900)
SD_IMPORT(Net_ClientFrame, 0x18aa50)
SD_IMPORT(Net_ClientReady, 0x18ae50)
SD_IMPORT(Net_ServerFrame, 0x18b330)
SD_IMPORT(Net_Done, 0x18e920)
SD_IMPORT(Net_Init, 0x18eb60)
SD_IMPORT(Net_Update, 0x18f950)
SD_IMPORT(Steam_Done, 0x1a64d0)	/* Sound agent range */
SD_IMPORT(Steam_GetLanguage, 0x1a65f0)	/* Sound agent range */
SD_IMPORT(Steam_Init, 0x1a6620)	/* Sound agent range */
SD_IMPORT(Steam_RunCallbacks, 0x1a66e0)	/* Sound agent range */
SD_IMPORT(Font_SetLanguage, 0x21b1e0)
SD_IMPORT(CPsaImage_ctorTex, 0x22d0c0)
SD_IMPORT(CPsaImage_ctorFlags, 0x22d0f0)
SD_IMPORT(CPsaImage_DrawPoly, 0x22d520)
SD_IMPORT(D3DApp_MsgProc, 0x233030)
SD_IMPORT(App_SetExceptionFilter, 0x2333b0)
SD_IMPORT(crt_srand, 0x323c50)

/* module agent: World/FX/FXCountermeasures+FXElementsOrbit+FXElementsPress.cpp (imports below this line) */
SD_IMPORT(cDebris_Update, 0x204ea0)
SD_IMPORT(cMapElement_ctor, 0x202ca0)
SD_IMPORT(cMapElement_dtor, 0x202e20)
SD_IMPORT(cMapElement_Init, 0x203310)
SD_IMPORT(cMapElement_Update, 0x203520)

/* module agent: World/FX/FXElements+FXElementsAB.cpp (imports below this line) */

/* module agent: Entities/EmbeddedFX/EmbeddedFXChar+CharHead.cpp (imports below this line) */
SD_IMPORT(cEmbeddedFXBankDirector_ctor, 0xd6930)
SD_IMPORT(cEmbeddedFXBankMaster_ctor, 0xd69a0)
SD_IMPORT(cEmbeddedFXCrazyGambler_ctor, 0xd6a30)
SD_IMPORT(cEmbeddedFXGear_ctor, 0xd6af0)
SD_IMPORT(cEmbeddedFXMaintenance_ctor, 0xd6b90)
SD_IMPORT(cEmbeddedFXMercenary_ctor, 0xd6c10)
SD_IMPORT(cEmbeddedFXMessenger_ctor, 0xd6cb0)
SD_IMPORT(cEmbeddedFXPolice_ctor, 0xd6d40)
SD_IMPORT(cEmbeddedFXRusty_ctor, 0xd6df0)
SD_IMPORT(cEmbeddedFXSentinel_ctor, 0xd6ed0)

/* module agent: Entities/EmbeddedFX/EmbeddedFXVehicle.cpp+mpeg/music_fmod.cpp (imports below this line) */
SD_IMPORT_STDCALL(FSOUND_Stream_GetSample, 4, 0x317ada)
SD_IMPORT_STDCALL(FSOUND_Stream_GetOpenState, 4, 0x317ad4)
SD_IMPORT_STDCALL(FSOUND_Stream_GetLengthMs, 4, 0x317ace)
SD_IMPORT_STDCALL(FSOUND_Stream_GetTime, 4, 0x317ac8)
SD_IMPORT_STDCALL(FSOUND_Stream_PlayEx, 16, 0x317ac2)
SD_IMPORT_STDCALL(FSOUND_Stream_Close, 4, 0x317abc)
SD_IMPORT_STDCALL(FSOUND_Stream_Open, 16, 0x317ab6)
SD_IMPORT_STDCALL(FSOUND_Stream_SetBufferSize, 4, 0x317ab0)
SD_IMPORT_STDCALL(FSOUND_GetNumSubChannels, 4, 0x317aaa)
SD_IMPORT_STDCALL(FSOUND_SetReserved, 8, 0x317aa4)
SD_IMPORT_STDCALL(FSOUND_GetDriverCaps, 8, 0x317a9e)
SD_IMPORT_STDCALL(FSOUND_GetDriverName, 4, 0x317a98)
SD_IMPORT_STDCALL(FSOUND_GetNumDrivers, 0, 0x317a92)
SD_IMPORT_STDCALL(FSOUND_GetMixer, 0, 0x317a8c)
SD_IMPORT_STDCALL(FSOUND_GetOutput, 0, 0x317a86)
SD_IMPORT_STDCALL(FSOUND_GetVersion, 0, 0x317a80)
SD_IMPORT_STDCALL(FSOUND_Close, 0, 0x317a7a)
SD_IMPORT_STDCALL(FSOUND_Init, 12, 0x317a74)
SD_IMPORT_STDCALL(FSOUND_SetDriver, 4, 0x317a6e)
SD_IMPORT_STDCALL(FSOUND_SetOutput, 4, 0x317a68)
SD_IMPORT_STDCALL(FSOUND_GetVolume, 4, 0x317a44)
SD_IMPORT_STDCALL(FSOUND_GetPan, 4, 0x317a4a)
SD_IMPORT_STDCALL(FSOUND_GetFrequency, 4, 0x317a3e)
SD_IMPORT_STDCALL(FSOUND_Sample_GetName, 4, 0x3179fc)
SD_IMPORT_STDCALL(FSOUND_GetCPUUsage, 0, 0x3179e4)

/* module agent: World/FX/FXWeapons.cpp (imports below this line) */
SD_IMPORT(CGameLensFlare_SetOn, 0x232f30)
SD_IMPORT(CGameLensFlare_SetIntensity, 0x232f80)
SD_IMPORT(CGameLensFlare_SetF30, 0x232fa0)
SD_IMPORT(CGameLensFlare_SetPos, 0x232fc0)
SD_IMPORT(CGameLensFlare_SetI1c, 0x233010)

/* module agent: Entities/EmbeddedFX/EmbeddedFXMisc.cpp (imports below this line) */

/* module agent: Entities/Vehicle/WeaponManager+Weaps/ATPC+Devastator.cpp (imports below this line) */
SD_IMPORT(cVehicleEffects_Shoot, 0x103500)
SD_IMPORT(cObjElementFXATPC_Shoot, 0x1fde50)
SD_IMPORT(Net_IsNetEntity, 0x18f840)

/* module agent: Entities/Vehicle/Weaps/Inferno+Laser+Swarm.cpp (imports below this line) */
SD_IMPORT(cWithLifeEntity_SetBurner, 0x12c860)
SD_IMPORT(Net_GetPlayerIndex, 0x18e9d0)
SD_IMPORT(FXSwarm_BeeDeath, 0x1ff190)
SD_IMPORT(FXSwarm_Hit, 0x1ffea0)
SD_IMPORT(cObjElementFXInfernoExplosion_Explode, 0x201eb0)

/* module agent: Entities/Entity+Game/Doors.cpp (imports below this line) */
SD_IMPORT(cActCtrl_Play, 0x063150)
SD_IMPORT(cActCtrl_SetDefLoop, 0x063210)
SD_IMPORT(cActCtrl_IsActive, 0x063dc0)
SD_IMPORT(Net_GetSendRate, 0x190d60)
SD_IMPORT(cWithLifeEntity_dtor, 0x12b780)
SD_IMPORT(cWithLifeEntity_VarF, 0x12bb30)
SD_IMPORT(cWithLifeEntity_UpdateOff, 0x12bf10)
SD_IMPORT(cWithLifeEntity_VarI, 0x12bfb0)
SD_IMPORT(cWithLifeEntity_IsType, 0x12c230)
SD_IMPORT(cWithLifeEntity_OnDeath, 0x12c530)
SD_IMPORT(cWithLifeEntity_SetActive, 0x12c770)
SD_IMPORT(cWithLifeEntity_Update, 0x12c8d0)
SD_IMPORT(cWithLifeEntity_VarS, 0x12c960)
SD_IMPORT(cWithLifeEntity_VarV, 0x12cd80)
SD_IMPORT(cDebris_Step, 0x204970)
SD_IMPORT(cDebrisSys_Release, 0x2053d0)

/* module agent: World/FX/FXChar.cpp (imports below this line) */

/* module agent: World/FX/FXScene.cpp (imports below this line) */
SD_IMPORT(CGameLensFlare_SetF18, 0x232ff0)
SD_IMPORT(cActCtrl_GetLayer, 0x062c70)

/* module agent: World/FX/FXVehicle.cpp (imports below this line) */

/* module agent: Engine/bsp.cpp (imports below this line) */
SD_IMPORT(CBsp_InitD3D, 0x28f710)
SD_IMPORT(BspRegion_ctor, 0x2d2920)
SD_IMPORT(BspRegion_dtor, 0x2d2ac0)
SD_IMPORT(BspLuces_dtor, 0x2d1290)
SD_IMPORT(BspLuces_Render, 0x2d1860)
SD_IMPORT(CBsp_LeerEMI, 0x28c650)
SD_IMPORT(CBsp_Calcular, 0x28a860)
SD_IMPORT(CBsp_Preparar, 0x28efe0)
SD_IMPORT(Dlg_SetFloat, 0x2a5d20)
SD_IMPORT(Dlg_GetFloat, 0x2a5d80)

/* module agent: World/FX/FXCharActions.cpp (imports below this line) */

/* module agent: Entities/More+ObjEntity (imports below this line) */
SD_IMPORT(Net_GetResourceName, 0x18ea80)

/* module agent: Entities/Vehicle/VehicleEffects+VehicleEntity.cpp (imports below this line) */
SD_IMPORT(cVehicleEffects_dtor, 0x100b60)
SD_IMPORT(cVehicleEffects_WindSmoke, 0x102940)
SD_IMPORT(cVehicleEntity_Step, 0x10a730)
SD_IMPORT(cVehicleMotors_dtor, 0x10bd10)
SD_IMPORT(cVehicleO648_dtor, 0x0ff010)
SD_IMPORT(cVehicleO648_SetSlot, 0x0ff3f0)
SD_IMPORT(cVehicleO648_Update, 0x0ff6a0)
SD_IMPORT(cVehicleControls_Tick, 0x0ffcc0)
SD_IMPORT(cWithLifeEntity_SetSndTime, 0x12b9e0)
SD_IMPORT(cWithLifeEntity_Tick, 0x12c440)
SD_IMPORT(cWithLifeEntity_RenderNow, 0x12c5a0)
SD_IMPORT(cNetVController_ctor, 0x1186b0)
SD_IMPORT(cVehicleO648_ctor, 0x0fefb0)
SD_IMPORT(cVehicleMotors_ctor, 0x10bc20)
SD_IMPORT(cVehicleEffects_ctor, 0x100ab0)

/* module agent: Entities/Vehicle/VehicleMotors+MotorsEffects+Traffic.cpp (imports below this line) */

/* module agent: Engine/RenderTarget.cpp (imports below this line) */
SD_IMPORT(Surface_SaveShot, 0x27fe90)

/* module agent: AI/NodesTraffic+NodesTrafficInDoor.cpp (imports below this line) */
SD_IMPORT(cVehicleMotor_ctor, 0x10bb90)
SD_IMPORT(cVehicleMotorsEffects_Exhaust, 0x10f210)

/* module agent: Scorer/AbstractText+Button+Circuit+Head+Mission+Monitor+Radar (imports below this line) */

/* module agent: Entities/Char/Type part 1 (imports below this line) */

/* module agent: AI/NodesWorld.cpp (imports below this line) */

/* module agent: Input/ActionSet+IDevice+PcKeyboard+PcMouse+PcPad.cpp (imports below this line) */
SD_IMPORT(CNodesWorldSup_SaveInfPath, 0x0bba70)
SD_IMPORT(DListVec3_Clear, 0x06d8a0)
SD_IMPORT(DListVec3_Add, 0x06d720)
SD_IMPORT(CNodesWorld_AddExtraPoints, 0x0a93d0)
SD_IMPORT(CNodesWorld_Generate, 0x0a7a00)
SD_IMPORT(CNodesWorld_AnalizeMap, 0x0a2e80)
SD_IMPORT(crt_chmod, 0x345287)

/* module agent: AI/NodesWorld2D+NodesWorldSup.cpp (imports below this line) */
SD_IMPORT(CPsaImage_GetFrameSize, 0x22d2e0)
SD_IMPORT(CPsaImage_GetFrameUV, 0x22d320)
SD_IMPORT(CPsaImage_DrawFrame, 0x22d440)
SD_IMPORT(CPsaImage_DrawFrameFlip, 0x22d480)
SD_IMPORT(cMonitorScorer_DrawMonitor, 0x168c60)
SD_IMPORT(cRadarScorer_DrawRadar, 0x16d570)
SD_IMPORT(cPlayerInput_PressKey, 0x1396b0)
SD_IMPORT(cActionSet_HoldPressed, 0x139810)
SD_IMPORT(cActCtrl_GetActionName, 0x062c40)
SD_IMPORT(cActCtrl_PlayNew, 0x0631c0)
SD_IMPORT(CNodesWorld2D_Generate, 0x0b2f60)

/* module agent: Entities/Char/Type part 2+WalkCharControl (imports below this line) */
SD_IMPORT(cActCtrl_PlayAction, 0x063030)
SD_IMPORT(cMayorLocalLogic_SetListener, 0x153010)
SD_IMPORT(AICtrl_CallGears, 0x14f960)
SD_IMPORT(AICtrl_SentinelsChase, 0x14fba0)
SD_IMPORT(cActCtrl_TurnHead, 0x062dc0)
SD_IMPORT(cActClassCam_Update, 0x0616e0)

/* module agent: Logic part 1 (AlarmStatus..Domination) (imports below this line) */
SD_IMPORT(AICtrl_SendAccuser, 0x14f840)
SD_IMPORT(Taunts_Play, 0x155210)
SD_IMPORT(cGenCharLocalLogic_Update, 0x14c6e0)
SD_IMPORT(cGenCharLocalLogic_SetState, 0x14d390)

/* module agent: Scorer part 2 (Scorer+ScraplandScorer+Slider+SpecialAction) (imports below this line) */
SD_IMPORT(Scorer_DrawDebug, 0x173450)
SD_IMPORT(Scorer_DrawConsole, 0x1742f0)
SD_IMPORT(cSpecialActionScorer_DrawGearAim, 0x17ac90)
SD_IMPORT(crt_ltoa, 0x34d525)

/* module agent: AI/IVehicle.cpp (imports below this line) */

/* module agent: Logic part 2 (Enemies..GenCharGroup) (imports below this line) */
SD_IMPORT(Logic_RandomCar, 0x154b20)
SD_IMPORT(PoliceAI_RestoreParams, 0x15b2f0)
SD_IMPORT(PoliceAI_RandomParams, 0x15b3c0)
SD_IMPORT(cGenCharLocalLogic_VarI, 0x14c4f0)
SD_IMPORT(cGenCharLocalLogic_VarS, 0x14d6e0)
SD_IMPORT(cGenCharLocalLogic_OnRobbery, 0x14cd20)
SD_IMPORT(cActCtrl_GetDefLoopName, 0x062c80)
SD_IMPORT(AICtrl_RaiseLevel, 0x14fb00)
SD_IMPORT(AICtrl_GearToPolice, 0x14eb50)
SD_IMPORT(cMessengers_ctor, 0x1530f0)
SD_IMPORT(cMessengers_dtor, 0x153100)
SD_IMPORT(cMessengers_Init, 0x153260)
SD_IMPORT(cMessengers_Think, 0x153350)
SD_IMPORT(cPoliceLocalLogic_ctor, 0x14fe50)
SD_IMPORT(cMayorLocalLogic_ctor, 0x152c60)
SD_IMPORT(cRustyLocalLogic_ctor, 0x156730)
SD_IMPORT(cSputnikJunkyardLocalLogic_ctor, 0x159220)
SD_IMPORT(cMaintenanceLocalLogic_ctor, 0x152230)
SD_IMPORT(cPoliceBossLocalLogic_ctor, 0x156010)
SD_IMPORT(cMessengerLocalLogic_ctor, 0x153d10)
SD_IMPORT(cMercenaryLocalLogic_ctor, 0x153040)
SD_IMPORT(cHumphreyLocalLogic_ctor, 0x14db90)
SD_IMPORT(cHumanLocalLogic_ctor, 0x14d830)
SD_IMPORT(Logic_IsSuspectType, 0x14c5a0)
SD_IMPORT(cGenCharLocalLogic_RestartState, 0x14d130)

/* module agent: Entities/Char/WalkCharEntity.cpp (imports below this line) */
SD_IMPORT(cWithLifeEntity_SetEmbeddedFX, 0x12c7f0)
SD_IMPORT(cAIBankDirector_ctor, 0x066120)
SD_IMPORT(cAIBankMaster_ctor, 0x066560)
SD_IMPORT(cAIBishop_ctor, 0x0669c0)
SD_IMPORT(cAIChar_ctor, 0x0670b0)
SD_IMPORT(cAIDesktop_ctor, 0x084310)
SD_IMPORT(cAIGear_ctor, 0x084790)
SD_IMPORT(cAIHuman_ctor, 0x084d50)
SD_IMPORT(cAIMayor_ctor, 0x084df0)
SD_IMPORT(cAIMessenger_ctor, 0x084f80)
SD_IMPORT(cAINurse_ctor, 0x085b30)
SD_IMPORT(cAIPolice_ctor, 0x085f20)
SD_IMPORT(cAISentinel_ctor, 0x086100)

/* module agent: Scorer part 3 (Gradient+Sprite+Tab) (imports below this line) */
SD_IMPORT(Nto1Head_Link, 0x067450)
SD_IMPORT(Nto1Node_UnlinkChecked, 0x06af70)
SD_IMPORT(cAIVehicle_GraphTraffic, 0x089070)
SD_IMPORT(cAIVehicle_Repos, 0x089b00)
SD_IMPORT(cAIVehicle_TrackPoint, 0x08a850)
SD_IMPORT(cAIVehicle_TestHookPoint, 0x08ab20)
SD_IMPORT(cAIVehicle_TrackPointRandom, 0x08b120)
SD_IMPORT(cAIVehicle_ReposTraffic, 0x08d3c0)
SD_IMPORT(cAIMove_ctor, 0x085340)
SD_IMPORT(cAIMove_dtor, 0x0854c0)
SD_IMPORT(cAIMove_Draw, 0x085690)
SD_IMPORT(cAIMove_Update, 0x0856a0)
SD_IMPORT(cAIMove_UpdateOff, 0x0856d0)
SD_IMPORT(cAIMove_VarF, 0x0856f0)
SD_IMPORT(cAIMove_VarI, 0x0859d0)
SD_IMPORT(cAIMove_VarS, 0x085a70)
SD_IMPORT(cAIMove_VarV, 0x085b10)
SD_IMPORT(crt_strtime, 0x33bb73)
SD_IMPORT(CPsaImage_GetNumFrames, 0x22d280)
SD_IMPORT(PyFloat_AsDouble, 0x2dfa30)	/* floatobject.c */
SD_IMPORT(cTargetScorer_DrawTarget, 0x181600)
SD_IMPORT(cTargetScorer_DrawEnemies, 0x182430)
SD_IMPORT(cWaypointScorer_DrawTarget, 0x185e90)

/* module agent: Logic part 3 (GenCharLogic..Messenger) (imports below this line) */

/* module agent: AI/ICharControl+2D+3D.cpp (imports below this line) */

/* module agent: AI/IVehicleControl+IVehicleShootControl.cpp (imports below this line) */
SD_IMPORT(cAIEntityGroup_ctor, 0x084620)
SD_IMPORT(cAIEntityGroup_dtor, 0x084650)
SD_IMPORT(CAStar_FindPath, 0x099b30)

/* module agent: net (GameServer..transport) (imports below this line) */
