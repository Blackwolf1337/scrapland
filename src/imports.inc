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

/* engine: World/FX/ and effects of Entities/ */

/* engine: Actions/ */

/* engine: AI/ (include/scrap/ai.h) */

/* MSVC 2015 static CRT (cdecl) */
SD_IMPORT(crt_rand, 0x323c2f)
SD_IMPORT(crt_malloc, 0x34c977)
SD_IMPORT(crt_free, 0x34c972)
SD_IMPORT(crt_realloc, 0x34c9e8)
SD_IMPORT(crt_msize, 0x34c9b5)
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

/* engine: Menu/ (include/scrap/scorer.h) */

/* engine: functions behind the Scrap module (include/scrap/scrap.h) */
SD_IMPORT(Steam_SetAchievement, 0x1a66a0)

/* engine: Input/ */
SD_IMPORT(ActionSet_IsNamed, 0x133a30)

/* engine: Math/ */

/* engine: World/ (weapon and ammo tables of cWorld) */

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

/* engine3d: Engine/ListaVert.cpp and Engine/render.cpp (include/scrap/listavert.h, mallatri.h) */

/* engine3d: called from Engine/luz.cpp (include/scrap/luz.h) */
/* engine3d: Engine/Material.c (include/scrap/material.h), the parts of the classes outside Material.cpp and the
 * texture manager */

/* src/game/engine3d/Engine/Escena.c (called from Engine/escena.cpp, include/scrap/escena.h) */

/* src/game/engine3d/Engine/Escena.c (called from Engine/escena.cpp, include/scrap/escena.h) */

/* src/game/engine3d/Engine/SistPart.c (called from Engine/SistPart.cpp, include/scrap/sistpart.h) */

/* src/game/engine3d/Engine/SueloPortal.c (called from Engine/SueloPortal.cpp, include/scrap/sueloportal.h) */

/* src/game/engine3d/Engine/Mundo.c (called from Engine/mundo.cpp, include/scrap/mundo.h) */

/* src/game/engine3d/Engine/Render.c (called from Engine/render.cpp, include/scrap/render.h) */
SD_IMPORT(ListaVert_InitD3D, 0x2404f0)
SD_IMPORT(ListaVert_ReleaseD3D, 0x2404e0)

/* src/game/engine3d/Engine/Grafico2D.c (called from Engine/grafico2d.cpp, include/scrap/grafico2d.h) */

/* src/game/engine3d/Engine/Texto.c (called from Engine/texto.cpp, include/scrap/texto.h) */

/* src/game/engine3d/Engine/UtilD3D.c (called from Engine/util_d3d.cpp, include/scrap/utild3d.h) */

/* src/game/engine3d/Engine/Grafico3D.c (called from Engine/grafico3d.cpp, include/scrap/grafico3d.h) */

/* src/game/engine3d/Engine/Portal.c (called from Engine/portal.cpp, include/scrap/portal.h) */

/* module agent: Engine/RenderTarget (imports below this line) */


/* module agent: Engine/MallaD3D (imports below this line) */

/* module agent: Engine/Suelo (imports below this line) */
SD_IMPORT(crt_ceil, 0x345010)

/* module agent: Engine/Trails (imports below this line) */
SD_IMPORT_STDCALL(D3DX_Vec3Normalize, 8, 0x006d3b)
SD_IMPORT_STDCALL(D3DX_Vec3Transform, 12, 0x007086)
SD_IMPORT_STDCALL(D3DX_MatrixRotationYPR, 16, 0x008701)
SD_IMPORT_STDCALL(D3DX_MatrixInverse, 12, 0x007cea)
SD_IMPORT_STDCALL(D3DX_AssembleShader, 24, 0x00b2c2)

/* module agent: Engine/Modelo (imports below this line) */
SD_IMPORT(CEscena_Shared, 0x24ecb0)

/* module agent: Engine/Nodo3D (imports below this line) */

/* module agent: Engine/Animacion (imports below this line) */

/* module agent: Engine/AnimGrupo (imports below this line) */
SD_IMPORT(CAnimCtrl_dtor, 0x250f10)

/* module agent: Engine/Textura (imports below this line) */
SD_IMPORT(crt_mbsrchr, 0x3508c4)
SD_IMPORT(crt_spawnlp, 0x34f2e0)
SD_IMPORT_STDCALL(D3DX_CheckTextureRequirements, 28, 0x0037ea)
SD_IMPORT_STDCALL(D3DX_CreateTextureFromFileInMemoryEx, 60, 0x0062a7)
SD_IMPORT_STDCALL(D3DX_LoadSurfaceFromSurface, 32, 0x004dbf)

/* src/game/ScrapEng/ScrapEngCollision.c (called from suelo.cpp and checkbbox.cpp, include/scrap/suelo.h) */

/* module agent: World/World.cpp (imports below this line) */

/* module agent: ScrapEng/ScrapEngWorld.cpp (imports below this line) */

/* module agent: ScrapEng/ScrapEngMain.cpp (imports below this line) */

/* module agent: ScrapEng/ScrapEngCamera+Particles+Trails.cpp (imports below this line) */

/* module agent: Sound/Sound+SoundDX8+Wav.cpp (imports below this line) */
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

/* module agent: App/CApp+CWinApp (imports below this line) */
SD_IMPORT(Gog_Init, 0x137250)
SD_IMPORT(Gog_Done, 0x137430)
SD_IMPORT(InputMaps_Done, 0x139900)
SD_IMPORT(Steam_Done, 0x1a64d0)	/* Sound agent range */
SD_IMPORT(Steam_GetLanguage, 0x1a65f0)	/* Sound agent range */
SD_IMPORT(Steam_Init, 0x1a6620)	/* Sound agent range */
SD_IMPORT(Steam_RunCallbacks, 0x1a66e0)	/* Sound agent range */
SD_IMPORT(crt_srand, 0x323c50)

/* module agent: World/FX/FXCountermeasures+FXElementsOrbit+FXElementsPress.cpp (imports below this line) */

/* module agent: World/FX/FXElements+FXElementsAB.cpp (imports below this line) */

/* module agent: Entities/EmbeddedFX/EmbeddedFXChar+CharHead.cpp (imports below this line) */

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

/* module agent: Entities/EmbeddedFX/EmbeddedFXMisc.cpp (imports below this line) */

/* module agent: Entities/Vehicle/WeaponManager+Weaps/ATPC+Devastator.cpp (imports below this line) */

/* module agent: Entities/Vehicle/Weaps/Inferno+Laser+Swarm.cpp (imports below this line) */

/* module agent: Entities/Entity+Game/Doors.cpp (imports below this line) */

/* module agent: World/FX/FXChar.cpp (imports below this line) */

/* module agent: World/FX/FXScene.cpp (imports below this line) */

/* module agent: World/FX/FXVehicle.cpp (imports below this line) */

/* module agent: Engine/bsp.cpp (imports below this line) */

/* module agent: World/FX/FXCharActions.cpp (imports below this line) */

/* module agent: Entities/More+ObjEntity (imports below this line) */

/* module agent: Entities/Vehicle/VehicleEffects+VehicleEntity.cpp (imports below this line) */

/* module agent: Entities/Vehicle/VehicleMotors+MotorsEffects+Traffic.cpp (imports below this line) */

/* module agent: Engine/RenderTarget.cpp (imports below this line) */

/* module agent: AI/NodesTraffic+NodesTrafficInDoor.cpp (imports below this line) */

/* module agent: Scorer/AbstractText+Button+Circuit+Head+Mission+Monitor+Radar (imports below this line) */

/* module agent: Entities/Char/Type part 1 (imports below this line) */

/* module agent: AI/NodesWorld.cpp (imports below this line) */

/* module agent: Input/ActionSet+IDevice+PcKeyboard+PcMouse+PcPad.cpp (imports below this line) */
SD_IMPORT(crt_chmod, 0x345287)

/* module agent: AI/NodesWorld2D+NodesWorldSup.cpp (imports below this line) */

/* module agent: Entities/Char/Type part 2+WalkCharControl (imports below this line) */

/* module agent: Logic part 1 (AlarmStatus..Domination) (imports below this line) */

/* module agent: Scorer part 2 (Scorer+ScraplandScorer+Slider+SpecialAction) (imports below this line) */
SD_IMPORT(crt_ltoa, 0x34d525)

/* module agent: AI/IVehicle.cpp (imports below this line) */

/* module agent: Logic part 2 (Enemies..GenCharGroup) (imports below this line) */

/* module agent: Entities/Char/WalkCharEntity.cpp (imports below this line) */

/* module agent: Scorer part 3 (Gradient+Sprite+Tab) (imports below this line) */
SD_IMPORT(crt_strtime, 0x33bb73)
SD_IMPORT(PyFloat_AsDouble, 0x2dfa30)	/* floatobject.c */

/* module agent: Logic part 3 (GenCharLogic..Messenger) (imports below this line) */

/* module agent: AI/ICharControl+2D+3D.cpp (imports below this line) */

/* module agent: AI/IVehicleControl+IVehicleShootControl.cpp (imports below this line) */

/* module agent: net (GameServer..transport) (imports below this line) */
/* the All-Seeing Eye SDK */
SD_IMPORT(Ase_Shutdown, 0x002070)
SD_IMPORT(Ase_Initialize, 0x002410)
SD_IMPORT(Ase_ServerStatus, 0x002480)
SD_IMPORT(Ase_AddRule, 0x002700)
SD_IMPORT(Ase_AddPlayer, 0x002770)
SD_IMPORT(Ase_Check, 0x0028f0)
/* wsock32 by ordinal through the jump thunks of the exe */
SD_IMPORT_STDCALL(Wsock_bind, 12, 0x20d9cc)
SD_IMPORT_STDCALL(Wsock_closesocket, 4, 0x20d9d2)
SD_IMPORT_STDCALL(Wsock_ioctlsocket, 12, 0x20d9d8)
SD_IMPORT_STDCALL(Wsock_htonl, 4, 0x20d9de)
SD_IMPORT_STDCALL(Wsock_htons, 4, 0x20d9e4)
SD_IMPORT_STDCALL(Wsock_ntohs, 4, 0x20d9ea)
SD_IMPORT_STDCALL(Wsock_recvfrom, 24, 0x20d9f0)
SD_IMPORT_STDCALL(Wsock_select, 20, 0x20d9f6)
SD_IMPORT_STDCALL(Wsock_sendto, 24, 0x20d9fc)
SD_IMPORT_STDCALL(Wsock_setsockopt, 20, 0x20da02)
SD_IMPORT_STDCALL(Wsock_socket, 12, 0x20da08)
SD_IMPORT_STDCALL(Wsock_gethostbyaddr, 12, 0x20da0e)
SD_IMPORT_STDCALL(Wsock_gethostbyname, 4, 0x20da14)
SD_IMPORT_STDCALL(Wsock_WSAStartup, 8, 0x20da1a)
SD_IMPORT_STDCALL(Wsock_WSACleanup, 0, 0x20da20)

/* module agent: Logic part 4 (Nurse..Traffic) (imports below this line) */
SD_IMPORT(crt_atexit, 0x313a5c)
SD_IMPORT(crt_Init_thread_header, 0x31409d)
SD_IMPORT(crt_Init_thread_footer, 0x31405e)

/* module agent: Actions and Events (ActClass..AIBishop) (imports below this line) */

/* module agent: AI/AIChar.cpp (imports below this line) */

/* module agent: AI/AICharControl.cpp (imports below this line) */

/* module agent: AI/AICharGroup and the AI subclasses (imports below this line) */
SD_IMPORT(crt_strtok, 0x33bbc1)

/* module agent: Vehicle weapons Tesla+Vulcan and WithLifeEntity (imports below this line) */

/* module agent: Engine/lee_M3D.cpp and bsp_q3.cpp (imports below this line) */
SD_IMPORT(crt_fileno, 0x3541cc)
SD_IMPORT(crt_filelength, 0x358bef)
SD_IMPORT(M3DTransform_ToMatrix, 0x2d6750)
SD_IMPORT(M3DTransform_ToMatrixAnim, 0x2d65d0)
SD_IMPORT(Matrix_ToTransform, 0x2d6c50)

/* module agent: Engine NodeFX..ModelFX (imports below this line) */

/* module agent: Engine leftovers 0x265000..0x278000 (imports below this line) */

/* module agent: Engine leftovers 0x2c9d90..0x2d6010 (imports below this line) */
SD_IMPORT(crt_open, 0x359882)
SD_IMPORT(crt_read, 0x35a234)
SD_IMPORT(crt_close, 0x358d98)

/* module agent: Engine leftovers 0x29c000..0x2b2000 (imports below this line) */

/* module agent: Big leftovers 0x0f0000..0x110000 (imports below this line) */

/* module agent: Big FX leftovers 0x1a8000..0x1eab00 (imports below this line) */

/* module agent: Big FX leftovers 0x1eab00..0x200000 (imports below this line) */

/* module agent: Engine leftovers 0x200000..0x240000 (imports below this line) */
SD_IMPORT_STDCALL(D3DX_MatrixMultiply, 12, 0x0077b8)
SD_IMPORT_STDCALL(D3DX_MatrixTranspose, 8, 0x007a01)
SD_IMPORT_STDCALL(D3DX_MatrixRotationX, 8, 0x0082fb)
SD_IMPORT_STDCALL(D3DX_MatrixRotationY, 8, 0x008397)
SD_IMPORT_STDCALL(D3DX_MatrixRotationZ, 8, 0x008434)

/* module agent: AI leftovers 0x098000..0x0b8000 (imports below this line) */
SD_IMPORT(crt_difftime32, 0x345bb1)
SD_IMPORT(crt_ctime32, 0x345734)

/* module agent: Rest A, game leftovers below 0x1a8000 (imports below this line) */
SD_IMPORT(crt_ui64toa, 0x34d6d5)
SD_IMPORT(crt_stdio_common_vsscanf, 0x344793)
SD_IMPORT_STDCALL(FSOUND_GetNumHWChannels, 12, 0x3179d8)
SD_IMPORT_STDCALL(FSOUND_GetChannelsPlaying, 0, 0x3179de)
SD_IMPORT_STDCALL(FSOUND_GetSFXMasterVolume, 0, 0x3179d2)
SD_IMPORT_STDCALL(FSOUND_GetPriority, 4, 0x317a50)
SD_IMPORT_STDCALL(FSOUND_GetLoopMode, 4, 0x317a5c)
SD_IMPORT(crt_Xlength_error, 0x317912)
SD_IMPORT(crt_Xout_of_range, 0x317932)
SD_IMPORT(crt_invalid_parameter_noinfo_noreturn, 0x34e614)
SD_IMPORT(crt_Throw_bad_array_new_length, 0x3178b8)
SD_IMPORT(MsvcString_ReallocFor, 0x1375f0)
SD_IMPORT(PtrVector_Reallocate, 0x137790)

/* module agent: Rest B, game leftovers from 0x240000 (imports below this line) */
/* busy waits of the original (the project forbids blocking waits in C): left to the exe */
SD_IMPORT(Video_WaitKeysUp, 0x280d10)
SD_IMPORT(Timer_CountTicks, 0x276ba0)
SD_IMPORT(Timer_GetQPFreq, 0x276d00)
SD_IMPORT(Timer_MeasureQP, 0x276e40)
SD_IMPORT(Tsc_CpuFreq, 0x272f90)

/* module agent: Final FX Starts (ShipExplosion, VehicleDamage, GearMissile, EMI, MentalController) (imports below this line) */

/* module agent: BSP light map compiler (imports below this line) */

/* module agent: Exhaust (imports below this line) */

/* module agent: Engine leftovers (Escena_Load, stencil draws, debug and dialogs) (imports below this line) */

/* module agent: Leftovers C, EmbeddedFX vf03, AICharControl3D, Gear, Devastator (imports below this line) */

/* module agent: Scorer draws (Monitor, Radar, MainMenu, Status, Target, Weapon, WindFx) (imports below this line) */

/* module agent: Final untested (DevastatorCreate, BSP compiler, EMI writer, radiosity) (imports below this line) */
