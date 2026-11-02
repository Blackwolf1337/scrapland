/*
 * Entities/Char/Type/Dtritus.cpp of the game: D-Tritus, whose punch kills the character in front of him in slow
 * motion (include/scrap/chartype.h). The native DtritusActionPress sits in the code of Desktop.cpp.
 */
#include "scrap/chartype.h"
#include "scrap/fxelements.h"
#include "scrap/primitive.h"
#include "scrap/vehicleattack.h"
#include "scrap/staticinit.h"

#define g_DTritusHitTimeFader (GVar_DTritusHitTimeFader.value.f)
#define g_FXDTritusAttackFile (GVar_FXDTritusAttackFile.value.s)
#define g_DTritusHitTimeSpeed (GVar_DTritusHitTimeSpeed.value.f)
#define g_FXDTritusKillFile (GVar_FXDTritusKillFile.value.s)
static cGVar GVar_FXDTritusKillFile;
static cGVar GVar_FXDTritusAttackFile;
static cGVar GVar_DTritusHitTimeSpeed;
static cGVar GVar_DTritusHitTimeFader;

static inline cFollowCamEntity *Dtritus_Cam(cEntity *v) {
	return (cFollowCamEntity *)WChar(v)->wl.obj.cam;
}

/* the relax action of the class: RelaxAlarm while the alarm runs, when the class has one */
static const char *Dtritus_Relax(cEntity *v) {
	if (ChType_ActCtrl(v) && ChType_ActClass(ChType_ActCtrl(v))->relaxAlarm &&
		(g_pWorld->alarm.hasGuaranteed || g_pWorld->alarm.on)) {
		return "RelaxAlarm";
	}
	return "Relax";
}

/* params: D-Tritus. With 0.9 of energy or more the punch, ActionFail as the loop, the energy all used */
static void ChType_DtritusActionPress(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *v = World_FindEntity(name);
	if (!v || !v->vftable->IsType(v, TYEN_WALKCHAR) || WChar(v)->falling || ChType_ActPriority(ChType_ActCtrl(v)) ||
		!(1.0f - WChar(v)->energyUsed >= 0.9f)) {
		return;
	}
	cActCtrl_SetDefLoop(ChType_ActCtrl(v), "ActionFail", 0);
	cActCtrl_Play(ChType_ActCtrl(v), "Action", 0, 1.0f);
	cWalkCharEntity_SetEnergy(v, (1.0f - WChar(v)->energyUsed) - 1.0f);
	cWalkCharControl_Reset(&WChar(v)->control, 1);
	if (WChar(v)->viewLock) {
		WChar(v)->displAng = 0.0f;
	}
}

static cEntity *g_DtritusEnt;

/* the sphere test of the punch: params the hit character, killed; D-Tritus turns by 45 degrees away from it, the
 * world in slow motion, the kill effect */
static void ChType_DtritusKill(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, t4, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	CHECK(g_DtritusEnt);
	CHECK(Entity_IsType(g_DtritusEnt, TYEN_WALKCHAR));
	if (!name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e || g_DtritusEnt == e || !e->vftable->IsType(e, TYEN_WALKCHAR) || WChar(e)->wl.god) {
		return;
	}
	SaveVar_Inc("Stats.Dtritus", 1);
	SaveVar_Inc("Stats.Killer", 1);
	cVehicleAttack_HitBy(&WChar(e)->wl.attack, g_DtritusEnt->name);
	if (!WChar(e)->wl.god) {
		WChar(e)->wl.life = 0;
	}
	cEntity_CollisionOff(e);
	cPrimitive_SetSize(&WChar(e)->wl.prim, 0.0f);
	Alarm_Set(&g_pWorld->alarm, 1.0f);
	cTimer_SetSpeedChecked(&g_pWorld->timer, g_DTritusHitTimeSpeed);
	cTimer_SetSpeedTo(&g_pWorld->timer, 1.0f);
	cTimer_SetBlend(&g_pWorld->timer, g_DTritusHitTimeFader);
	const vec3_t *pe = e->vftable->GetPos(e, &t1);
	const vec3_t *pm = g_DtritusEnt->vftable->GetPos(g_DtritusEnt, &t2);
	const vec3_t *pe2 = e->vftable->GetPos(e, &t3);
	const vec3_t *pm2 = g_DtritusEnt->vftable->GetPos(g_DtritusEnt, &t4);
	double a = atan2((double)(pe->x - pm->x), (double)(pe2->z - pm2->z));
	float n = NormAng(((float)a - Obj_Vtbl(g_DtritusEnt)->GetAng(g_DtritusEnt, &t1)->y) - M_PI_F);
	double turn = (n > 0.0f ? M_PI_D : -M_PI_D) * 0.25;
	float y = Obj_Vtbl(g_DtritusEnt)->GetAng(g_DtritusEnt, &t1)->y + (float)turn;
	ang.x = 0.0f;
	ang.y = y;
	ang.z = 0.0f;
	WChar_Vtbl(g_DtritusEnt)->SetViewAng(g_DtritusEnt, &ang);
	WChar(g_DtritusEnt)->displAng = -(float)turn;
	const char *act = FxElem_Rand(1.0, 0.0) > 0.5f ? "ActionOK1" : "ActionOK";
	cActCtrl_SetDefLoop(ChType_ActCtrl(g_DtritusEnt), act, 0);
	cFollowCamEntity *cam = Dtritus_Cam(g_DtritusEnt);
	if (cam) {
		cam->vSpeed = 1.5f;
		cam->hSpeed = 1.5f;
		cam->quakeFactor = 0.0199999996f;
		cam->quakeTime = 0.0500000007f;
		cPad_Rumble(&g_Pads[0], 1.0f, 0.5f, 0.5f);
	}
	cObjElementFXKill *el = FX_DropNew(g_FXDTritusKillFile, 1.0f);
	if (!el) {
		return;
	}
	if (!el->started) {
		el->started = 1;
	}
	cObjElementFXKill_Start(el, e);
	const vec3_t *ma = Obj_Vtbl(g_DtritusEnt)->GetAng(g_DtritusEnt, &t1);
	cObjElementFXKill_SetAngles(el, ma->x + 0.0f, ma->y + M_PI_F, ma->z + 0.0f);
}

/* params: D-Tritus, an event, a float: he stops, the world speed and his view back */
static void ChType_DtritusEndEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t v;

	ChType_ReadParams(params, name, event);
	g_DtritusEnt = World_FindEntity(name);
	CHECK(g_DtritusEnt);
	CHECK(Entity_IsType(g_DtritusEnt, TYEN_WALKCHAR));
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = 0.0f;
	Obj_Vtbl(g_DtritusEnt)->SetVel(g_DtritusEnt, &v);
	cTimer_SetSpeedChecked(&g_pWorld->timer, 1.0f);
	if (WChar(g_DtritusEnt)->viewLock) {
		WChar_Vtbl(g_DtritusEnt)->SetViewAng(g_DtritusEnt, Obj_Vtbl(g_DtritusEnt)->GetAng(g_DtritusEnt, &v));
		WChar(g_DtritusEnt)->displAng = 0.0f;
	}
	cWalkCharControl_Reset(&WChar(g_DtritusEnt)->control, 0);
	if (WChar(g_DtritusEnt)->ownerSlot >= 0 && WChar(g_DtritusEnt)->ownerSlot < 4) {
		cWalkCharControl_Update(&WChar(g_DtritusEnt)->control, g_DtritusEnt, 0.0f);
	}
	if (WChar(g_DtritusEnt)->control.walk > 0.5f) {
		const char *relax = Dtritus_Relax(g_DtritusEnt);
		cActCtrl_SetDefLoop(ChType_ActCtrl(g_DtritusEnt), relax, 1);
	}
	cFollowCamEntity *cam = Dtritus_Cam(g_DtritusEnt);
	if (cam) {
		cam->vSpeed = 1.10000002f;
		cam->hSpeed = 1.10000002f;
	}
}

/* params: D-Tritus, an event, a float: the punch, ChType_DtritusKill in a sphere 50 ahead along his velocity */
static void ChType_DtritusHitEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp, n, c;

	ChType_ReadParams(params, name, event);
	g_DtritusEnt = World_FindEntity(name);
	CHECK(g_DtritusEnt);
	CHECK(Entity_IsType(g_DtritusEnt, TYEN_WALKCHAR));
	const vec3_t *vel = Obj_Vtbl(g_DtritusEnt)->GetVel(g_DtritusEnt, &tmp);
	float len = (float)__builtin_sqrt((double)(vel->x * vel->x + vel->y * vel->y + vel->z * vel->z));
	if (len == 0.0f) {
		n = *vel;
	} else {
		float k = 1.0f / len;
		n.x = vel->x * k;
		n.y = vel->y * k;
		n.z = vel->z * k;
	}
	float ox = n.x * 50.0f;
	float oy = n.y * 50.0f;
	float oz = n.z * 50.0f;
	const vec3_t *p = g_DtritusEnt->vftable->GetPos(g_DtritusEnt, &tmp);
	c.x = p->x + ox;
	c.y = p->y + oy;
	c.z = p->z + oz;
	cPy_Function_SetNative(&g_ChTypeSphereFunc, (nativefunc_t)ChType_DtritusKill);
	cWorld_SphereCallF(g_pWorld, 0x10, &c, 100.0f, &g_ChTypeSphereFunc, 0);
}

/* params: D-Tritus, an event, a float: the relax loop again */
static void ChType_DtritusRestartEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	g_DtritusEnt = World_FindEntity(name);
	CHECK(g_DtritusEnt);
	CHECK(Entity_IsType(g_DtritusEnt, TYEN_WALKCHAR));
	const char *relax = Dtritus_Relax(g_DtritusEnt);
	cActCtrl_SetDefLoop(ChType_ActCtrl(g_DtritusEnt), relax, 1);
}

static void ChType_FXDTritusAttack(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	cObjElementFXDTritusAttack *el = FX_DropNew(g_FXDTritusAttackFile, 0.8f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cPad_Rumble(&g_Pads[0], 0.0f, 0.5f, 1.0f);
		cObjElementFXDTritusAttack_Start(el, v);
	}
}

static cGVar GVar_DTritusHitTimeFader;
static cGVar GVar_FXDTritusAttackFile;
static cGVar GVar_DTritusHitTimeSpeed;
static cGVar GVar_FXDTritusKillFile;
static cPy_NativeFunc NativeFunc_DtritusActionPress;
static cPy_NativeFunc NativeFunc_DtritusHitEvent;
static cPy_NativeFunc NativeFunc_DtritusEndEvent;
static cPy_NativeFunc NativeFunc_DtritusRestartEvent;
static cPy_NativeFunc NativeFunc_FXDTritusAttack;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FXDTritusAttackFile, &GVar_FXDTritusAttackFile)
INIT_GVAR_STRING(Init_GVar_FXDTritusAttackFile, Fini_GVar_FXDTritusAttackFile, &GVar_FXDTritusAttackFile, "FXDTritusAttackFile", 0, "Default Model for DTritus Attack Effect", "Models/GFX/DTritus/Attack.M3D")
FINI_GVAR_STRING(Fini_GVar_FXDTritusKillFile, &GVar_FXDTritusKillFile)
INIT_GVAR_STRING(Init_GVar_FXDTritusKillFile, Fini_GVar_FXDTritusKillFile, &GVar_FXDTritusKillFile, "FXDTritusKillFile", 0, "Default Model for DTritus Kill Effect", "Models/GFX/DTritus/Kill.M3D")
INIT_GVAR_FLOAT(Init_GVar_DTritusHitTimeSpeed, &GVar_DTritusHitTimeSpeed, "DTritusHitTimeSpeed", 0, "DTritus Effect Time", 0.05f)
INIT_GVAR_FLOAT(Init_GVar_DTritusHitTimeFader, &GVar_DTritusHitTimeFader, "DTritusHitTimeFader", 0, "DTritus Effect Time", 0.2f)
INIT_NATIVE_FUNC(Init_NativeFunc_DtritusActionPress, &NativeFunc_DtritusActionPress, "DtritusActionPress", ChType_DtritusActionPress)
INIT_NATIVE_FUNC(Init_NativeFunc_DtritusHitEvent, &NativeFunc_DtritusHitEvent, "DtritusHitEvent", ChType_DtritusHitEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_DtritusEndEvent, &NativeFunc_DtritusEndEvent, "DtritusEndEvent", ChType_DtritusEndEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_DtritusRestartEvent, &NativeFunc_DtritusRestartEvent, "DtritusRestartEvent", ChType_DtritusRestartEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_FXDTritusAttack, &NativeFunc_FXDTritusAttack, "FXDTritusAttack", ChType_FXDTritusAttack)
