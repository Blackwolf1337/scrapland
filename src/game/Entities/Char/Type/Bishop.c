/*
 * Entities/Char/Type/Bishop.cpp of the game: the bishop, whose green ray hurts and who sells life
 * (include/scrap/chartype.h). BishopActionPress is ChType_ActionPress.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fx.h"
#include "scrap/fxelements.h"
#include "scrap/vehicleattack.h"

/* params: the bishop, an event, a float: the action again while the button is down and the energy lasts (always
 * for an AI) */
void ChType_EndActionEnd(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	cEntity *usr = ChType_Usr0();
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	if ((usr != v || !(WChar_EnergyBarDecr(v) > 1.0f - WChar_EnergyUsed(v))) && WChar_ActionPress(v)) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Action", 1);
	}
}

/* the characters whose hit raises the alarm at once */
static byte Bishop_AlarmClass(const char *cls) {
	return !strcmp(cls, "Police") || !strcmp(cls, "Gear") || !strcmp(cls, "PoliceBoss") || !strcmp(cls, "Sentinel");
}

/* params: the bishop. The shot: a random push back, the aim (ahead, at the GearTargetName of the player or the
 * targeting of the character), the damage, the alarm of a hit of the player, the green ray and the sounds. */
void ChType_BishopAttack(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, rot, imp, nv, aim, hit, d, mp, mr, rot2, up, q, w, aoff, from, to, to2, from2;
	cEntity *target = NULL;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	float energy = 1.0f - WChar_EnergyUsed(v);
	if (WChar_Life(v) <= 0) {
		return;
	}
	if ((!cWalkCharEntity_GetOwnerSlot(v) && g_GearLowWeaponEnergy > energy) || !WChar_ActionPress(v)) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Relax", 1);
	}
	cWalkCharEntity_SetEnergy(v, energy - g_BishopAmmoCost);
	/* the push back, turned by a random angle */
	float amax = (g_BishopAngleVariation * M_PI_F) / 180.0f;
	float amin = -amax;
	float r = FxElem_Rand((double)(amax - amin), (double)amin);
	const vec3_t *a = Obj_Vtbl(v)->GetAng(v, &t1);
	rot.x = a->x + 0.0f;
	rot.y = a->y + r;
	rot.z = a->z + 0.0f;
	imp.x = 0.0f;
	imp.y = 0.0f;
	imp.z = 1000.0f;
	const vec3_t *p = Vec3_Rotate(&t2, &imp, &rot);
	const vec3_t *vel = Obj_Vtbl(v)->GetVel(v, &t3);
	nv.x = p->x + vel->x;
	nv.y = vel->y + p->y;
	nv.z = vel->z + p->z;
	Obj_Vtbl(v)->SetVel(v, &nv);
	if (!cWalkCharEntity_GetOwnerSlot(v)) {
		const char *tn = g_GearTargetName;
		if (tn && *tn) {
			target = World_FindEntity(tn);
		}
	}
	float dist = 10000.0f;
	aim.x = 0.0f;
	aim.y = 0.0f;
	aim.z = 0.0f;
	if (!WChar_ViewLock(v)) {
		aim.x = Obj_Vtbl(v)->GetAng(v, &t1)->x;
		aim.y = Obj_Vtbl(v)->GetAng(v, &t1)->y;
		target = NULL;
	} else if (target && target->vftable->IsType(target, TYEN_WITH_LIFE)) {
		const vec3_t *tp = target->vftable->GetPos(target, &t1);
		const vec3_t *vp = v->vftable->GetPos(v, &t2);
		d.y = vp->y - tp->y;
		d.z = vp->z - tp->z;
		d.x = vp->x - tp->x;
		hit = *Vec3_GetRotAng(&t3, &d);
		aim.x = hit.x;
		aim.y = hit.y;
	} else {
		v->vftable->GetTargetAngle(v, &aim.x, dist, &target);
	}
	v->vftable->AimWeapon(v, aim.x, aim.y, &dist, &hit, &target, 0x11);
	if (target && target->vftable->IsType(target, TYEN_WITH_LIFE)) {
		if (WChar_Cam(target)) {
			cPad_Rumble(&g_Pads[0], 1.0f, 0.0f, 0.5f);
		}
		int damage = g_BishopWeaponDamage;
		if (v->vftable->GetAI(v) && World_IsEasySingle()) {
			damage = damage / 2;
		}
		if (!WChar_God(target)) {
			WChar_Life(target) -= damage;
		}
		cVehicleAttack_HitBy(WChar_Attack(target), v->name);
		if (!cWalkCharEntity_GetOwnerSlot(v)) {
			const char *cls = *(const char **)(ChType_ActClass(ChType_ActCtrl(target)) + 4);
			float level;
			if (WChar_Life(target) <= 0) {
				if (WChar_PrevLife(target) > 0) {
					SaveVar_Inc("Stats.Bishop", 1);
					SaveVar_Inc("Stats.Killer", 1);
				}
				level = 1.0f;
			} else if (Bishop_AlarmClass(cls)) {
				level = 1.0f;
			} else {
				level = g_pWorld->alarmLevel + 0.100000001f;
			}
			Alarm_Set(g_pWorld->alarm, level);
		}
		cLocalLogic *l = Logic_GetLocalLogic(target);
		if (l) {
			ChLogic_Vtbl(l)->OnShot(l, v);
		}
	} else {
		target = NULL;
	}
	cObjElementFXBishopAttack *el = FX_DropNew(g_FXBishopAttackFile, 0.400000006f);
	if (!el) {
		return;
	}
	if (!el->started) {
		el->started = 1;
	}
	CGameModel_GetPos(Obj_Vtbl(v)->GetModel(v), &mp);
	CGameModel_GetRot(Obj_Vtbl(v)->GetModel(v), &mr);
	/* the end of the ray off the point it hits by a random radius in a random direction */
	float off = g_BishopAttackRandOff;
	float rr = FxElem_Rand((double)off, 0.0);
	rot2.x = 0.0f;
	rot2.y = 0.0f;
	rot2.z = FxElem_Rand(6.2831854820251465, 0.0);
	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;
	const vec3_t *qq = Vec3_Rotate(&q, &up, &rot2);
	const vec3_t *ww = Vec3_Rotate(&w, qq, &mr);
	float rvx = rr * ww->x;
	float rvy = ww->y * rr;
	float rvz = ww->z * rr;
	aoff.x = g_BishopAttackPosX;
	aoff.y = g_BishopAttackPosY;
	aoff.z = g_BishopAttackPosZ;
	const vec3_t *s = Vec3_Rotate(&w, &aoff, &mr);
	from.x = mp.x + s->x;
	from.y = s->y + mp.y;
	from.z = s->z + mp.z;
	to = hit;
	if (target) {
		to = *WEnt_Vtbl(target)->GetCenter(target, &t1);
	}
	FXMisc_SetEntity(el, v);
	float k1 = FxElem_Rand(0.20000004768371582, 0.8999999761581421);
	to2.x = rvx * k1 + to.x;
	to2.y = rvy * k1 + to.y;
	to2.z = rvz * k1 + to.z;
	float k2 = FxElem_Rand(0.2999999523162842, 0.6000000238418579);
	from2.z = rvz * k2 + from.z;
	from2.x = rvx * k2 + from.x;
	from2.y = rvy * k2 + from.y;
	cObjElementFXBishopAttack_SetLine(el, from2.x, from2.y, from2.z, to2.x, to2.y, to2.z);
	const char *snd1 = g_BishopShootSound;
	CSoundSys *snd = g_pSound;
	int vol = Snd_Volume(g_BishopShootSoundVol);
	snd->vftable->PlayAt(g_pSound, snd1, &from, vol);
	const char *snd2 = g_BishopHitSound;
	snd = g_pSound;
	vol = Snd_Volume(g_BishopHitSoundVol);
	snd->vftable->PlayAt(g_pSound, snd2, &to, vol);
	if (WChar_Cam(v)) {
		cPad_Rumble(&g_Pads[0], 0.5f, 0.5f, 0.25f);
	}
}

/* params: the attacker (none: the life ring only) and the attacked */
void ChType_FXBishopSellLife(cAbstractFile *params) {
	char a[0x80], b[0x80];

	cAbstractFile_ReadString(params, a, 0x80);
	cAbstractFile_ReadString(params, b, 0x80);
	cEntity *attacker = World_FindEntity(a);
	cEntity *attacked = World_FindEntity(b);
	FX_BishopSellLife(attacker, attacked);
}

void FX_BishopSellLife(cEntity *attacker, cEntity *attacked) {
	if (attacker) {
		CHECK(Entity_IsType(attacker, TYEN_WITH_LIFE));
	}
	CHECK(attacked);
	CHECK(Entity_IsType(attacked, TYEN_WITH_LIFE));
	cObjElementFXBishopSellLife *el = FX_DropNew(g_FXBishopSellLifeFile, attacker ? 6.0f : 2.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXBishopSellLife_SetAttacker(el, attacker);
		cObjElementFXBishopSellLife_SetAttacked(el, attacked);
	}
}
