/*
 * Entities/Char/Type/Maintenance.cpp of the game, its first part: the work effect of the maintenance robot on a
 * ship (include/scrap/chartype.h). The rest of the file comes with part 2 of the character types.
 */
#include "scrap/chartype.h"
#include "scrap/fxchar.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

/* params: the robot, the ship, the time of the effect */
void ChType_FXMaintenanceWork(cAbstractFile *params) {
	char name[0x80], shipName[0x80];
	float time;
	vec3_t tmp;

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, shipName, 0x80);
	params->vftable->Read(params, &time, 4);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	cEntity *ship = World_FindEntity(shipName);
	CHECK(ship);
	CHECK(Entity_IsType(ship, TYEN_VEHICLE));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMaintenanceWorkFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), v->vftable->GetPos(v, &tmp));
	d->life = time;
	cDebrisSys_Drop(sys, d);
	cObjElementFXMaintenanceWork *el = Debris_Element(d);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXMaintenanceWork_SetMaintenance(el, v);
		cObjElementFXMaintenanceWork_SetShip(el, ship);
		cObjElementFXMaintenanceWork_Start(el, d);
	}
}

byte cAIChar_TestFlags(const void *self) {
	return ((const byte *)self)[0x28] || ((const byte *)self)[0x2a];
}

cGVar GVar_FXMaintenanceWorkFile;

cPy_NativeFunc NativeFunc_FXMaintenanceWork;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FXMaintenanceWorkFile, &GVar_FXMaintenanceWorkFile)
INIT_GVAR_STRING(Init_GVar_FXMaintenanceWorkFile, Fini_GVar_FXMaintenanceWorkFile, &GVar_FXMaintenanceWorkFile, "FXMaintenanceWorkFile", 0, "Default Model for Maintenance Robot Work Effects", "Models/GFX/Maintenance/Work.M3D")
INIT_NATIVE_FUNC(Init_NativeFunc_FXMaintenanceWork, &NativeFunc_FXMaintenanceWork, "FXMaintenanceWork", ChType_FXMaintenanceWork)
