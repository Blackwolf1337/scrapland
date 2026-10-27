/*
 * Entities/Char/Type/Desktop.cpp of the game: the desktop, who jumps (forward, back or to a side) and can land on
 * a character for money (include/scrap/chartype.h). FXDesktopFallEnd (the dust of the landing) is not in C yet.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/weapons.h"

/* the characters a landing pays twice for */
static byte Desktop_BonusClass(cEntity *t) {
	const char *cls = *(const char **)(ChType_ActClass(ChType_ActCtrl(t)) + 4);

	if (!strcmp(cls, "Gear")) {
		return 1;
	}
	cls = *(const char **)(ChType_ActClass(ChType_ActCtrl(t)) + 4);
	if (!strcmp(cls, "Police")) {
		return 1;
	}
	cls = *(const char **)(ChType_ActClass(ChType_ActCtrl(t)) + 4);
	return !strcmp(cls, "Sentinel");
}

/* params: the desktop. On the ground the jump of the direction it walks (ActUpB/F/L/R) with the speed of its
 * walk; in the air from a jump the fall (ActMoreB/F, the view turned for a side), and a landing on a character
 * pays DesktopJumpPrice. */
void ChType_DesktopActionPress(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, v3, c, up;
	cEntity *target;
	const char *jump;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	const char *act = cActCtrl_GetActionName(ChType_ActCtrl(v), 0);
	if (!WChar_Falling(v) || WChar_SlideTime(v) > g_DesktopJumpSlideTime) {
		if (!strcmp(act, "Back")) {
			jump = "ActUpB";
		} else if (!strcmp(act, "StrLeft")) {
			jump = "ActUpL";
		} else if (strcmp(act, "StrRight")) {
			jump = "ActUpF";
		} else {
			jump = "ActUpR";
		}
		cActCtrl_Play(ChType_ActCtrl(v), jump, 0, 1.0f);
		if (!cActCtrl_IsActive(ChType_ActCtrl(v), jump)) {
			return;
		}
		cWalkCharEntity_SetFalling(v, 1);
		float ay = WChar_Ang(v)->y;
		up.x = 0.0f;
		up.y = g_DesktopJumpSpeed;
		up.z = 0.0f;
		/* the walk direction (the control at 0x4f8, 0x4fc) turns the jump, its length tilts it */
		double a = Game_Atan2((double)-WChar_ControlF4fc(v), (double)-WChar_ControlF4f8(v));
		float yaw = (float)a + ay;
		float len = (float)__builtin_sqrt((double)(WChar_ControlF4f8(v) * WChar_ControlF4f8(v) +
			WChar_ControlF4fc(v) * WChar_ControlF4fc(v)));
		const vec3_t *vel = Vec3_RotateXY(&t1, &up, len * 0.25f, yaw);
		Obj_Vtbl(v)->SetVel(v, vel);
		cActCtrl_Play(ChType_ActCtrl(v), jump, 0, 1.0f);
		return;
	}
	if (strncmp(act, "ActUp", 5)) {
		return;
	}
	float vx = Obj_Vtbl(v)->GetVel(v, &t1)->x;
	float speed = g_DesktopJumpSpeed;
	v3.z = Obj_Vtbl(v)->GetVel(v, &t2)->z;
	v3.x = vx;
	v3.y = speed;
	Obj_Vtbl(v)->SetVel(v, &v3);
	if (!strcmp(act, "ActUpB")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreB", 0, 1.0f);
	} else if (!strcmp(act, "ActUpF")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
	} else if (!strcmp(act, "ActUpL")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
		WChar_DisplAng(v) = -1.04719758f;
	} else if (!strcmp(act, "ActUpR")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
		WChar_DisplAng(v) = 1.04719758f;
	}
	const vec3_t *p = v->vftable->GetPos(v, &t3);
	c.x = p->x + 0.0f;
	c.y = p->y - 400.0f;
	c.z = p->z + 0.0f;
	target = NULL;
	float radius = Entity_Radius(v);
	p = v->vftable->GetPos(v, &t3);
	if (!cWorld_TestSphereEntities(g_pWorld, v, (void *)0x10, p, radius, &target, 1, (int)&c) || !target ||
		!target->vftable->IsType(target, TYEN_WALKCHAR) || !ChType_ActCtrl(target)) {
		return;
	}
	g_pWorld->money += g_DesktopJumpPrice;
	SaveVar_Inc("Stats.Jump", 1);
	if (Desktop_BonusClass(target)) {
		g_pWorld->money += g_DesktopJumpPrice;
		SaveVar_Inc("Stats.Jump.Police", 1);
	}
}
