/*
 * Entities/Char/Type/BankMaster.cpp of the game: the bank master, whose baton sucks the money out of the characters
 * in front of it (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"

#define BANKMASTER_SUCTION_ANG 1.0471975511965976	/* rva 0x3ad5f8, 60 degrees */

void ChType_ActionSound(cEntity *v, byte loop, byte stop) {
	cSndItem *s = PtrArray_Get(&WChar(v)->wl.obj.snd, 0);

	if (s) {
		s->volume = 0.0f;
		s->sound.desc.loop = loop;
		if (stop) {
			cSound_Stop(&s->sound);
		}
	}
}

/* params: the bank master, an event, a float */
void ChType_BankMasterActionEnd(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 0, 1);
	if ((cWalkCharEntity_GetOwnerSlot(v) || !(WChar(v)->energyBarDecr > 1.0f - WChar(v)->energyUsed)) &&
		WChar(v)->control.act) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Action", 1);
	}
}

void ChType_BankMasterActionInit(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 1, 0);
}

void ChType_ActionPress(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *v = World_FindEntity(name);
	if (v && v->vftable->IsType(v, TYEN_WALKCHAR) && !WChar(v)->falling && !ChType_ActPriority(ChType_ActCtrl(v))) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Action", 0);
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 0, 1.0f);
	}
}

void ChType_Attack(cAbstractFile *params, float cost) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	float energy = 1.0f - WChar(v)->energyUsed;
	if ((!cWalkCharEntity_GetOwnerSlot(v) && g_GearLowWeaponEnergy > energy) || !WChar(v)->control.act) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Relax", 1);
	}
	cWalkCharEntity_SetEnergy(v, energy - cost);
}

void ChType_BankMasterAttack(cAbstractFile *params) {
	ChType_Attack(params, g_BankMasterAmmoCost);
}

/* the sphere test of the hit: params the character in the sphere. Within 60 degrees in front of the bank master its
 * money goes over, with the effect, and its logic is told */
void ChType_BankMasterSuction(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, t4, d, diff, n;
	cEntity *victim;

	cAbstractFile_ReadString(params, name, 0x80);
	CHECK(g_BankMasterEnt);
	CHECK(Entity_IsType(g_BankMasterEnt, TYEN_WALKCHAR));
	if (!name[0]) {
		return;
	}
	victim = World_FindEntity(name);
	if (!victim || g_BankMasterEnt == victim || !victim->vftable->IsType(victim, TYEN_WALKCHAR) ||
		WChar(victim)->wl.god || WChar(victim)->wl.life <= 0) {
		return;
	}
	const vec3_t *p = g_BankMasterEnt->vftable->GetPos(g_BankMasterEnt, &t1);
	const vec3_t *q = victim->vftable->GetPos(victim, &t2);
	d.x = q->x - p->x;
	d.y = q->y - p->y;
	d.z = q->z - p->z;
	float ax = WChar(g_BankMasterEnt)->view.x + 0.0f;
	float ay = Obj_Vtbl(g_BankMasterEnt)->GetAng(g_BankMasterEnt, &t3)->y + M_PI_F;
	const vec3_t *r = Vec3_GetRotAng(&t4, &d);
	diff.y = ay - r->y;
	diff.z = 0.0f - r->z;
	diff.x = ax - r->x;
	Vec3_NormAng(&n, &diff);
	float nx = (float)__builtin_fabs((double)n.x);
	float ny = (float)__builtin_fabs((double)n.y);
	if (!(BANKMASTER_SUCTION_ANG > (double)nx) || !(BANKMASTER_SUCTION_ANG > (double)ny)) {
		return;
	}
	if (ChType_Steal(&g_BankMasterEnt, &victim, g_BankerStealAmount, "Stats.BankMaster", 0.25f)) {
		cObjElementFXBankMasterSuction *el = FX_DropNew(g_FXBankMasterSuctionFile, 0.8f);
		if (el) {
			if (!el->started) {
				el->started = 1;
			}
			FXMisc_SetEntity(el, g_BankMasterEnt);
			FXCharActions_SetEntity8(el, victim);
			cObjElementFXBankMasterSuction_RandomPoints(el);
		}
	}
	cLocalLogic *l = Logic_GetLocalLogic(victim);
	if (l) {
		Logic_Vtbl(l)->OnAttack(l, g_BankMasterEnt, 100.0f);
	}
}

/* params: the bank master, an event, a float. The energy of a shot, then the suction in the sphere around it. */
void ChType_BankMasterHitEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	g_BankMasterEnt = World_FindEntity(name);
	CHECK(g_BankMasterEnt);
	CHECK(Entity_IsType(g_BankMasterEnt, TYEN_WALKCHAR));
	if (WChar(g_BankMasterEnt)->wl.obj.cam) {
		cPad_Rumble(&g_Pads[0], 0.0f, 0.5f, 0.35f);
	}
	cWalkCharEntity_SetEnergy(g_BankMasterEnt, (1.0f - WChar(g_BankMasterEnt)->energyUsed) - g_BankMasterAmmoCost);
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)GAME_FUNC(ChType_BankMasterSuction, CHTYPE_BANKMASTERSUCTION));
	float radius = g_BankMasterVacumDist;
	const vec3_t *pos = g_BankMasterEnt->vftable->GetPos(g_BankMasterEnt, &tmp);
	cWorld_SphereCallF(g_pWorld, 0x10, pos, radius, g_ChTypeSphereFunc, 0);
}

void ChType_FXBankMasterAttack(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	cObjElementFXBankMasterAttack *el = FX_DropNew(g_FXBankMasterAttackFile, 1.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXBankMasterAttack_Start(el, v);
	}
}
