/*
 * Entities/Char/Type/Messenger.cpp of the game: the messenger, whose lightning flashes the characters it hits and
 * knocks the data pack (MessengerDataPack) out of the hands of another messenger, and who catches it again
 * (include/scrap/chartype.h). MessengerActionPress is ChType_ActionPress, MessengerEndActionEnd ChType_EndActionEnd.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fxelements.h"
#include "scrap/itementity.h"
#include "scrap/modmove.h"
#include "scrap/staticinit.h"

#define g_FXMessengerAttackFile (GVar_FXMessengerAttackFile.value.s)
#define g_FXMessengerTakeDataPackFile (GVar_FXMessengerTakeDataPackFile.value.s)
#define g_MessengerAmmoCost (GVar_MessengerAmmoCost.value.f)
#define g_MessengerAttackPosX (GVar_MessengerAttackPosX.value.f)
#define g_MessengerAttackPosY (GVar_MessengerAttackPosY.value.f)
#define g_MessengerAttackPosZ (GVar_MessengerAttackPosZ.value.f)
#define g_MessengerShootSound (GVar_MessengerShootSound.value.s)
#define g_MessengerShootSoundVol (GVar_MessengerShootSoundVol.value.f)
#define g_MessengerTakeDataPackSound (GVar_MessengerTakeDataPackSound.value.s)
#define g_MessengerTakeDataPackSoundVol (GVar_MessengerTakeDataPackSoundVol.value.f)
#define g_MessengerTakenCallback (((cPy_Function *)&GVar_MessengerTakenCallback.func))	/* "MessengerTakenCallback" */
static cGVarFunction GVar_MessengerTakenCallback;
static cGVar GVar_MessengerTakeDataPackSoundVol;
static cGVar GVar_MessengerTakeDataPackSound;
static cGVar GVar_MessengerShootSoundVol;
static cGVar GVar_MessengerShootSound;
static cGVar GVar_MessengerAttackPosZ;
static cGVar GVar_MessengerAttackPosY;
static cGVar GVar_MessengerAttackPosX;
static cGVar GVar_MessengerAmmoCost;
static cGVar GVar_FXMessengerTakeDataPackFile;
static cGVar GVar_FXMessengerAttackFile;

#define ITEM_TYPE "item"

/* params: the messenger. The shot: the energy, the aim (ahead, at the GearTargetName of the player or the
 * targeting of the character), the flash of the character hit (a messenger drops the data pack it carries) and the
 * lightning with its sound */
static void ChType_MessengerAttack(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, ang, d, hit, p, a, off, from, to, nv, vel;
	cEntity *target = NULL;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	float energy = 1.0f - WChar(v)->energyUsed;
	if (WChar(v)->wl.life <= 0) {
		return;
	}
	if ((!cWalkCharEntity_GetOwnerSlot(v) && g_GearLowWeaponEnergy > energy) || !WChar(v)->control.act) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Relax", 1);
	}
	cWalkCharEntity_SetEnergy(v, energy - g_MessengerAmmoCost);
	if (!cWalkCharEntity_GetOwnerSlot(v)) {
		const char *tn = g_GearTargetName;
		if (tn && *tn) {
			target = World_FindEntity(tn);
		}
	}
	float dist = 2000.0f;
	ang.x = 0.0f;
	ang.y = 0.0f;
	ang.z = 0.0f;
	if (!WChar(v)->viewLock) {
		ang.x = WChar(v)->view.x;
		ang.y = Obj_Vtbl(v)->GetAng(v, &t1)->y;
		target = NULL;
	} else if (target && target->vftable->IsType(target, TYEN_WITH_LIFE)) {
		const vec3_t *tp = target->vftable->GetPos(target, &t1);
		const vec3_t *vp = v->vftable->GetPos(v, &t2);
		d.y = vp->y - tp->y;
		d.z = vp->z - tp->z;
		d.x = vp->x - tp->x;
		hit = *Vec3_GetRotAng(&t3, &d);
		ang.x = hit.x;
		ang.y = hit.y;
	} else {
		v->vftable->GetTargetAngle(v, &ang.x, dist, &target);
	}
	v->vftable->AimWeapon(v, ang.x, ang.y, &dist, &hit, &target, 0x11);
	if (!target || !target->vftable->IsType(target, TYEN_WITH_LIFE)) {
		target = NULL;
	}
	if (WChar(v)->wl.obj.cam) {
		cPad_Rumble(&g_Pads[0], 0.0f, 0.75f, 0.125f);
	}
	if (target) {
		const char *cls = ChType_ActClass(ChType_ActCtrl(target))->name;
		const char *dpn = g_MessengerDataPack;
		if (!strcmp(cls, "Messenger") && *dpn) {
			cEntity *dp = World_FindEntity(dpn);
			if (dp && dp->vftable->IsType(dp, ITEM_TYPE) && Obj(dp)->mod.link.target == target) {
				dp->vftable->SetPos(dp, CGameModel_GetPos(Obj_Vtbl(dp)->GetModel(dp), &t1));
				const vec3_t *n = Vec3_NormalizeTo(&nv, Obj_Vtbl(target)->GetVel(target, &t1));
				vel.x = n->x * -100.0f;
				vel.y = n->y * -100.0f;
				vel.z = n->z * -100.0f;
				Obj_Vtbl(dp)->SetVel(dp, &vel);
				cModMove_Link(&Obj(dp)->mod, "", "");
				cPy_Function_SetNative(&((cItemEntity *)dp)->onTake,
					(nativefunc_t)ChType_MessengerTakeDataPack);
			}
		}
		ChType_FlashChar(target);
		if (WChar(target)->wl.obj.cam) {
			cPad_Rumble(&g_Pads[0], 0.0f, 1.0f, 0.75f);
		}
	}
	cObjElementFXMessengerAttack *el = FX_DropNew(g_FXMessengerAttackFile, 0.400000006f);
	if (!el) {
		return;
	}
	if (!el->started) {
		el->started = 1;
	}
	Obj_Vtbl(v)->GetViewPos(v, &p);
	Obj_Vtbl(v)->GetViewAng(v, &a);
	off.x = g_MessengerAttackPosX;
	off.y = g_MessengerAttackPosY;
	off.z = g_MessengerAttackPosZ;
	const vec3_t *r = Vec3_Rotate(&t1, &off, &a);
	from.x = r->x + p.x;
	from.y = r->y + p.y;
	from.z = r->z + p.z;
	to = hit;
	if (target) {
		to = *Obj_Vtbl(target)->GetViewPos(target, &t1);
	}
	FXMisc_SetEntity(el, v);
	cObjElementFXMessengerAttack_SetLine(el, from.x, from.y, from.z, to.x, to.y, to.z);
	CSoundSys *snd = g_pSound;
	const char *s = g_MessengerShootSound;
	int vol = Snd_Volume(g_MessengerShootSoundVol);
	snd->vftable->PlayAt(g_pSound, s, &from, vol);
}

/* the OnTake of the data pack a messenger dropped: params the data pack, the one that touches it. A messenger that
 * is not flashed takes it back on its node DC_DataPack (crab close in front, else the drop end) and the
 * MessengerTakenCallback hears its name */
void ChType_MessengerTakeDataPack(cAbstractFile *params) {
	char item[0x80], name[0x80];
	vec3_t t1, t2, t3, t4;

	cAbstractFile_ReadString(params, item, 0x80);
	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *m = World_FindEntity(name);
	if (!m || !m->vftable->IsType(m, TYEN_WALKCHAR) ||
		strcmp(ChType_ActClass(ChType_ActCtrl(m))->name, "Messenger") ||
		strcmp(g_MessengerDataPack, item) ||
		!crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(m), 0), "Flashed", 7)) {
		return;
	}
	cEntity *dp = World_FindEntity(item);
	if (!dp || !dp->vftable->IsType(dp, ITEM_TYPE)) {
		return;
	}
	cModMove *mod = &Obj(dp)->mod;
	cModMove_Link(mod, "", "");
	cModMove_Link(mod, name, "DC_DataPack");
	mod->offMul = 0.0f;
	mod->deltaOffMul = 4.0f;
	mod->useLinkAng = 1;
	cPy_Function_Clear(&((cItemEntity *)dp)->onTake);
	const vec3_t *a = dp->vftable->GetPos(dp, &t1);
	const vec3_t *b = m->vftable->GetPos(m, &t2);
	const vec3_t *c = dp->vftable->GetPos(dp, &t3);
	const vec3_t *e = m->vftable->GetPos(m, &t4);
	float at = (float)atan2((double)(a->x - b->x), (double)(c->z - e->z));
	float n = NormAng(at - Obj_Vtbl(m)->GetAng(m, &t4)->y);
	if (__builtin_fabs((double)n) > 1.5707963267948966) {
		if (cActCtrl_Play(ChType_ActCtrl(m), "DropDataEnd", 0, 1.0f)) {
			cActCtrl_SetDefLoop(ChType_ActCtrl(m), "Relax", 1);
		}
	} else {
		cActCtrl_Play(ChType_ActCtrl(m), "CrabClose", 1, 1.0f);
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMessengerTakeDataPackFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	d->life = 0.699999988f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXMessengerTakeDataPack *el = Debris_Element(d);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXMessengerTakeDataPack_SetOwner(el, m);
		cObjElementFXMessengerTakeDataPack_SetItem(el, dp);
		cObjElementFXMessengerTakeDataPack_Start(el);
		CSoundSys *snd = g_pSound;
		const char *s = g_MessengerTakeDataPackSound;
		int vol = Snd_Volume(g_MessengerTakeDataPackSoundVol);
		snd->vftable->PlayAt(g_pSound, s, m->vftable->GetPos(m, &t4), vol);
	}
	if (cPy_Function_IsValid(g_MessengerTakenCallback)) {
		cPy_Function_AddParamS(g_MessengerTakenCallback, name);
		cPy_Function_Execute(g_MessengerTakenCallback, 0);
	}
}

static cGVar GVar_MessengerAttackPosZ;
static cGVar GVar_MessengerAmmoCost;
static cGVar GVar_MessengerAttackPosX;
static cGVar GVar_FXMessengerAttackFile;
static cGVar GVar_MessengerTakeDataPackSound;
static cGVar GVar_FXMessengerTakeDataPackFile;
static cGVar GVar_MessengerAttackPosY;
static cGVar GVar_MessengerShootSoundVol;
static cGVar GVar_MessengerShootSound;
static cGVar GVar_MessengerTakeDataPackSoundVol;
static cGVarFunction GVar_MessengerTakenCallback;
cGVar GVar_MessengerDataPack;
static cPy_NativeFunc NativeFunc_MessengerTakeDataPack;
static cPy_NativeFunc NativeFunc_MessengerAttack;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MessengerAmmoCost, &GVar_MessengerAmmoCost, "MessengerAmmoCost", 0, "Messenger AmmoCost Per shoot", 0.05f)
FINI_GVAR_STRING(Fini_GVar_FXMessengerAttackFile, &GVar_FXMessengerAttackFile)
INIT_GVAR_STRING(Init_GVar_FXMessengerAttackFile, Fini_GVar_FXMessengerAttackFile, &GVar_FXMessengerAttackFile, "FXMessengerAttackFile", 0, "Default Model for Messenger Attack Effects", "Models/GFX/Messenger/Attack.M3D")
FINI_GVAR_STRING(Fini_GVar_FXMessengerTakeDataPackFile, &GVar_FXMessengerTakeDataPackFile)
INIT_GVAR_STRING(Init_GVar_FXMessengerTakeDataPackFile, Fini_GVar_FXMessengerTakeDataPackFile, &GVar_FXMessengerTakeDataPackFile, "FXMessengerTakeDataPackFile", 0, "Default Model for Messenger Take DataPack Effects", "Models/GFX/Messenger/TakeDataPack.M3D")
INIT_GVAR_FLOAT(Init_GVar_MessengerAttackPosX, &GVar_MessengerAttackPosX, "MessengerAttackPosX", 0, "Messenger Attack Displacement from center", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_MessengerAttackPosY, &GVar_MessengerAttackPosY, "MessengerAttackPosY", 0, "Messenger Attack Displacement from center", -25.0f)
INIT_GVAR_FLOAT(Init_GVar_MessengerAttackPosZ, &GVar_MessengerAttackPosZ, "MessengerAttackPosZ", 0, "Messenger Attack Displacement from center", -40.0f)
FINI_GVAR_STRING(Fini_GVar_MessengerShootSound, &GVar_MessengerShootSound)
INIT_GVAR_STRING(Init_GVar_MessengerShootSound, Fini_GVar_MessengerShootSound, &GVar_MessengerShootSound, "MessengerShootSound", 0, "Messenger shoot impact sound name", "MessengerShoot")
INIT_GVAR_FLOAT(Init_GVar_MessengerShootSoundVol, &GVar_MessengerShootSoundVol, "MessengerShootSoundVol", 0, "Messenger shoot impact sound volume", 1.0f)
FINI_GVAR_STRING(Fini_GVar_MessengerTakeDataPackSound, &GVar_MessengerTakeDataPackSound)
INIT_GVAR_STRING(Init_GVar_MessengerTakeDataPackSound, Fini_GVar_MessengerTakeDataPackSound, &GVar_MessengerTakeDataPackSound, "MessengerTakeDataPackSound", 0, "Messenger Take DataPack sound name", "MessengerTakeDataPack")
INIT_GVAR_FLOAT(Init_GVar_MessengerTakeDataPackSoundVol, &GVar_MessengerTakeDataPackSoundVol, "MessengerTakeDataPackSoundVol", 0, "Messenger Take DataPack sound volume", 1.0f)
FINI_CALL(Fini_GVar_MessengerTakenCallback, &GVar_MessengerTakenCallback.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_MessengerTakenCallback, Fini_GVar_MessengerTakenCallback, &GVar_MessengerTakenCallback, "MessengerTakenCallback", 0, "Called if a messager takes a datapack", "")
FINI_GVAR_STRING(Fini_GVar_MessengerDataPack, &GVar_MessengerDataPack)
INIT_GVAR_STRING(Init_GVar_MessengerDataPack, Fini_GVar_MessengerDataPack, &GVar_MessengerDataPack, "MessengerDataPack", 0, "Desktops functionary list name", "MessengerDataPack")
INIT_NATIVE_FUNC(Init_NativeFunc_MessengerTakeDataPack, &NativeFunc_MessengerTakeDataPack, "MessengerTakeDataPack", ChType_MessengerTakeDataPack)
INIT_NATIVE_FUNC(Init_NativeFunc_MessengerAttack, &NativeFunc_MessengerAttack, "MessengerAttack", ChType_MessengerAttack)
