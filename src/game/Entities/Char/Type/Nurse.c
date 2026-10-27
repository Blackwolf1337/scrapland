/*
 * Entities/Char/Type/Nurse.cpp of the game: the nurse, whose hammer puts the characters to sleep (Dazed), wakes the
 * sleeping ones and kills a desktop (include/scrap/chartype.h). The natives keep the name of the nurse in a global
 * for the hit (g_NurseName), the sphere test finds the character hit (g_NurseTarget).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/primitive.h"

/* the AI of the nurse: whether it has a target and which */
typedef struct NurseAIVtbl {
	void *vf00_0d[14];
	int(__thiscall *HasTarget)(void *ai);			/* 0x38 */
	void *vf0f_11[3];
	cEntity *(__thiscall *GetTarget)(void *ai);		/* 0x48 */
} NurseAIVtbl;
SD_OFFSET(NurseAIVtbl, HasTarget, 0x38);
SD_OFFSET(NurseAIVtbl, GetTarget, 0x48);

/* the sphere test of the hit: params a character, not the nurse; the player is never the target */
void ChType_NurseSphereHit(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	if (!strcmp(g_NurseName, name)) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	CHECK(Entity_IsType(e, TYEN_WALKCHAR));
	if (e != cWorld_GetUsrEntity(g_pWorld, 0)) {
		g_NurseTarget = e;
	}
}

/* the hit of the hammer of the nurse g_NurseName at the end of ActionLoop: the character in front (the sphere test
 * for the player, the target of the AI when it sleeps near it) is blocked by a gatekeeper, a gear or another nurse,
 * else it falls asleep, wakes up (it slept) or dies (a desktop). The hit of the player raises the alarm. */
void ChType_NurseHit(void) {
	vec3_t t1, t2, t3, rot, off, hit, d, r;

	cEntity *v = World_FindEntity(g_NurseName);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	if (!cActCtrl_IsActive(ChType_ActCtrl(v), "ActionLoop")) {
		return;
	}
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)Game_Ptr(CHTYPE_NURSESPHEREHIT));
	cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Relax", 1);
	float ay = Obj_Vtbl(v)->GetAng(v, &t1)->y;
	rot.x = 0.0f;
	rot.y = ay;
	rot.z = 0.0f;
	off.x = 0.0f;
	off.y = 0.0f;
	off.z = -g_NurseDispDist;
	const vec3_t *rp = Vec3_Rotate(&t2, &off, &rot);
	const vec3_t *p = v->vftable->GetPos(v, &t3);
	g_NurseTarget = NULL;
	hit.x = rp->x + p->x;
	hit.y = p->y + rp->y;
	hit.z = p->z + rp->z;
	if (!v->vftable->GetAI(v)) {
		cWorld_SphereCallF(g_pWorld, 0x10, &hit, g_NurseDispRad, g_ChTypeSphereFunc, 0);
	} else {
		void *ai = v->vftable->GetAI(v);
		const NurseAIVtbl *vt = *(const NurseAIVtbl **)ai;
		if (vt->HasTarget(ai)) {
			g_NurseTarget = vt->GetTarget(ai);
			if (!g_NurseTarget) {
				goto fail;
			}
			if (Entity_IsType(g_NurseTarget, TYEN_WALKCHAR)) {
				if (!g_NurseTarget) {
					goto fail;
				}
				if (!crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(g_NurseTarget), 0), "Dazed", 5)) {
					const vec3_t *tp = g_NurseTarget->vftable->GetPos(g_NurseTarget, &t1);
					float dy = hit.y - tp->y;
					float dx = hit.x - tp->x;
					float dz = hit.z - tp->z;
					float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
					if (!(dist - Ent_Primitive(g_NurseTarget)->radius > g_NurseDispRad)) {
						goto found;
					}
				}
			}
			g_NurseTarget = NULL;
			goto fail;
		}
	}
found:
	if (!g_NurseTarget) {
		goto fail;
	}
	byte usr = cWorld_GetUsrEntity(g_pWorld, 0) == v;
	const vec3_t *tp = g_NurseTarget->vftable->GetPos(g_NurseTarget, &t1);
	const vec3_t *vp = v->vftable->GetPos(v, &t2);
	d.x = vp->x - tp->x;
	d.y = vp->y - tp->y;
	d.z = vp->z - tp->z;
	Vec3_GetRotAng(&r, &d);
	vec3_t a = *WChar_Ang(v);
	WChar_DisplAng(v) = NormAng(r.y - a.y);
	WChar_DisplOn(v) = 1;
	const char *cls = *(const char **)(ChType_ActClass(ChType_ActCtrl(g_NurseTarget)) + 4);
	const char *act = cActCtrl_GetActionName(ChType_ActCtrl(g_NurseTarget), 0);
	if (usr && crt_strncmp(act, "Dazed", 5)) {
		AICtrl_SendSentinelToWatch(g_pWorld->aiCtrl, v->vftable->GetPos(v, &t3));
		if (!strcmp(cls, "Gear") || !strcmp(cls, "Sentinel") || !strcmp(cls, "Police") || !strcmp(cls, "Desktop")) {
			Alarm_Set(g_pWorld->alarm, 1.0f);
		} else {
			Alarm_Set(g_pWorld->alarm, g_pWorld->alarmLevel + g_NurseAlarmHit);
		}
		cLocalLogic *l = Logic_GetLocalLogic(g_NurseTarget);
		if (l) {
			ChLogic_Vtbl(l)->OnHammer(l, v);
		}
	}
	if (!strcmp(cls, "GateKeeper")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionBlock", 0, 1.0f);
		return;
	}
	if (!strcmp(cls, "Gear")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionBlock", 0, 1.0f);
		if (WChar_Cam(v)) {
			cPad_Rumble(&g_Pads[0], 1.0f, 0.0f, 0.25f);
		}
		return;
	}
	if (!strcmp(cls, "Nurse")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionBlock", 0, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(g_NurseTarget), "Relax", 1);
		cActCtrl_Play(ChType_ActCtrl(g_NurseTarget), "BlockHit", 1, 1.0f);
		cWalkCharEntity_SetViewFrom(g_NurseTarget, v->vftable->GetPos(v, &t3));
		if (WChar_Cam(v)) {
			cPad_Rumble(&g_Pads[0], 0.75f, 0.5f, 0.25f);
		}
		return;
	}
	cActCtrl_Play(ChType_ActCtrl(v), "ActionOK", 0, 1.0f);
	if (WChar_Cam(v)) {
		cPad_Rumble(&g_Pads[0], 0.75f, 1.0f, 0.25f);
	}
	if (!strcmp(act, "Dazed")) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(g_NurseTarget), "Relax", 1);
		cActCtrl_Play(ChType_ActCtrl(g_NurseTarget), "DazedEnd", 1, 1.0f);
		return;
	}
	if (!strcmp(cls, "Desktop")) {
		if (usr) {
			SaveVar_Inc("Stats.Nurse.Kill", 1);
			SaveVar_Inc("Stats.Killer", 1);
		}
		if (!WChar_God(g_NurseTarget)) {
			WChar_Life(g_NurseTarget) = 0;
		}
		return;
	}
	cActCtrl_SetDefLoop(ChType_ActCtrl(g_NurseTarget), "Dazed", 1);
	cActCtrl_Play(ChType_ActCtrl(g_NurseTarget), "DazedInit", 1, 1.0f);
	if (usr) {
		SaveVar_Inc("Stats.Nurse.Dazed", 1);
	}
	if (cPy_Function_IsValid(g_OnSleep)) {
		cPy_Function_AddParamS(g_OnSleep, g_NurseTarget->name);
		cPy_Function_Execute(g_OnSleep, 0);
	}
	return;
fail:
	cActCtrl_Play(ChType_ActCtrl(v), "ActionFail", 0, 1.0f);
}

/* params: the nurse. ActionInit unless an Action runs */
void ChType_NurseActionPress(cAbstractFile *params) {
	cAbstractFile_ReadString(params, g_NurseName, 0x80);
	cEntity *v = World_FindEntity(g_NurseName);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	if (crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(v), 0), "Action", 6)) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 1, 1.0f);
	}
}

/* params: the nurse: the hit if the hammer is up */
void ChType_NurseActionRelease(cAbstractFile *params) {
	cAbstractFile_ReadString(params, g_NurseName, 0x80);
	ChType_NurseHit();
}

/* params: the nurse, an event, a float: its displacement angle off */
void ChType_NurseBackHitControl(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	WChar_DisplOn(v) = 0;
}

/* params: the nurse, an event, a float: the hammer effect and a shake of the camera */
void ChType_NurseDoHammerHit(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	WChar_DisplOn(v) = 0;
	Nurse_HammerFX(v);
	cFollowCamEntity *cam = (cFollowCamEntity *)WChar_Cam(v);
	if (cam) {
		cam->quakeFactor = 0.05235987901687622f;
		cam->quakeTime = 0.200000003f;
	}
}

/* params: the nurse, an event, a float: ActionLoop, the hit at once when the button is up */
void ChType_NurseInitHammerLoop(cAbstractFile *params) {
	char event[0x80];
	float unused;

	cAbstractFile_ReadString(params, g_NurseName, 0x80);
	cAbstractFile_ReadString(params, event, 0x80);
	params->vftable->Read(params, &unused, 4);
	cEntity *v = ChType_CheckChar(g_NurseName);
	cActCtrl_Play(ChType_ActCtrl(v), "ActionLoop", 0, 1.0f);
	if (!WChar_ActionPress(v)) {
		ChType_NurseHit();
	}
}

/* params: the nurse, an event, a float: ActionInit again while the button is down */
void ChType_NurseTryHammerAgain(cAbstractFile *params) {
	char event[0x80];
	float unused;

	cAbstractFile_ReadString(params, g_NurseName, 0x80);
	cAbstractFile_ReadString(params, event, 0x80);
	params->vftable->Read(params, &unused, 4);
	cEntity *v = ChType_CheckChar(g_NurseName);
	if (WChar_ActionPress(v)) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 1, 1.0f);
	}
}
