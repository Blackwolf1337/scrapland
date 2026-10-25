/*
 * Functions of Scrap.exe the C code calls: SD_IMPORT(name, rva) for __cdecl, SD_IMPORT_STDCALL(name, argbytes, rva)
 * for __stdcall. src/core/imports.c turns each into a thunk with the name of the original, so the prototype comes
 * from the normal header. A function that gets implemented in C moves to src/hooks.inc: the linker reports a
 * function that is in both lists.
 */

/* engine: Global/ */
SD_IMPORT(CheckFailed, 0x0bcdb0)	/* CHECK macro, include/scrap/global.h */

SD_IMPORT(Con_Print, 0x0bca50)		/* include/scrap/console.h */
SD_IMPORT(Con_Errorf, 0x0bc580)
SD_IMPORT(Con_Debugf, 0x0bc710)

SD_IMPORT(HashTable_Hash, 0x065370)	/* Global/HashTable.h, the instance in Actions/ */
SD_IMPORT(HashTable_Find, 0x0652e0)

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
SD_IMPORT(operator_new, 0x1334d0)
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
SD_IMPORT(cAIVehicle_Setup, 0x08c040)
SD_IMPORT(cAIVehicle_GetParams1, 0x08bc20)
SD_IMPORT(cAIVehicle_GetParams2, 0x08bc40)
SD_IMPORT(cAIVehicle_GetParams3, 0x08bc60)
SD_IMPORT(cAIVehicle_GetNearestItemLife, 0x08a7d0)
SD_IMPORT(cAIVehicle_SetRotStatic, 0x08df70)
SD_IMPORT(cAIVehicle_SetInertia, 0x08df60)
SD_IMPORT(cVehicleEntity_GetOwnerSlot, 0x1085b0)
SD_IMPORT(cWalkCharEntity_GetOwnerSlot, 0x0d38b0)
SD_IMPORT(SetStateVehicle0, 0x1969b0)
SD_IMPORT(SetStateVehicle1, 0x196fb0)
SD_IMPORT(SetStateVehicle2, 0x196700)
SD_IMPORT(SetStateVehicle3, 0x197180)
SD_IMPORT(SetStateVehicle4, 0x197b70)
SD_IMPORT(SetStateVehicle5, 0x196c60)
SD_IMPORT(SetStateVehicle6, 0x1978d0)
SD_IMPORT(SetStateVehicle7, 0x1960e0)
SD_IMPORT(SetStateVehicle8, 0x196860)
SD_IMPORT(SetStateVehicle9, 0x197c90)
SD_IMPORT(SetStateChar0, 0x197a50)
SD_IMPORT(SetStateChar1, 0x1972f0)
SD_IMPORT(SetStateChar2, 0x196400)
SD_IMPORT(SetStateChar3, 0x196580)
SD_IMPORT(SetStateChar4, 0x196e10)
SD_IMPORT(SetStateChar5, 0x196ae0)
SD_IMPORT(SetStateChar6, 0x197750)
SD_IMPORT(SetStateChar7, 0x196280)
SD_IMPORT(SetStateChar8, 0x197450)
SD_IMPORT(SetStateChar9, 0x1975d0)

/* MSVC 2015 static CRT (cdecl) */
SD_IMPORT(crt_rand, 0x323c2f)

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
SD_IMPORT(Vec3_Rotate, 0x15e3b0)
SD_IMPORT(Vec3_GetRotAng, 0x15df10)
SD_IMPORT(NormAng, 0x15e2e0)

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
SD_IMPORT(PyErr_Clear, 0x2efd30)	/* errors.c */
SD_IMPORT(PyTuple_New, 0x2decb0)	/* tupleobject.c */
SD_IMPORT(PyTuple_SetItem, 0x2ded50)	/* tupleobject.c */
SD_IMPORT(PyFloat_FromDouble, 0x2dfd70)	/* floatobject.c */
SD_IMPORT(PyList_Size, 0x2dd2b0)	/* listobject.c */
SD_IMPORT(PyTuple_GetItem, 0x2dec30)	/* tupleobject.c */
SD_IMPORT(PyInt_AsLong, 0x2de3a0)	/* intobject.c */
SD_IMPORT(PyList_GetItem, 0x2dcf80)	/* listobject.c */
SD_IMPORT(PyString_AsString, 0x2dadd0)	/* stringobject.c */
