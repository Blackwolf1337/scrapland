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

/* engine: Actions/ */
SD_IMPORT(cActClassList_Create, 0x061f70)
SD_IMPORT(cActClassList_Get, 0x062250)
SD_IMPORT(cActClassList_Del, 0x062440)
SD_IMPORT(cActClass_CreateAction, 0x05fe90)
SD_IMPORT(cActClass_GetAction, 0x060590)
SD_IMPORT(cActClass_Var, 0x060770)
SD_IMPORT(cAction_Var, 0x064280)

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
