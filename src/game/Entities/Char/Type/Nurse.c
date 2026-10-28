/*
 * Entities/Char/Type/Nurse.cpp of the game: the nurse, whose hammer puts the characters to sleep (Dazed), wakes the
 * sleeping ones and kills a desktop (include/scrap/chartype.h). The natives keep the name of the nurse in a global
 * for the hit (g_NurseName), the sphere test finds the character hit (g_NurseTarget).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fxelements.h"
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

#define NURSE_RAMP_GLOW (GAME_ARRAY(uint32_t, NURSE_RAMP_GLOW, 0x428198))	/* [9] */
#define NURSE_RAMPS_STARS (GAME_ARRAY(uint32_t, NURSE_RAMPS_STARS, 0x428108))	/* [2 * 9], two ramps */
#define NURSE_RAMPS_SPARKS (GAME_ARRAY(uint32_t, NURSE_RAMPS_SPARKS, 0x428150))	/* [2 * 9], two ramps */
#define NURSE_TWO_PI 6.2831854820251465

static void Nurse_PosMin(CGameParticles *p, float x, float y, float z) {
	p->initPosMin.x = x;
	p->initPosMin.y = y;
	p->initPosMin.z = z;
	p->initPosMax = p->initPosMin;
}

/* the start of a ring particle of the glow and of the stars (rotation, sprites, the ramp on) */
static void Nurse_Start(CGameParticles *p, float rotVar, uint16_t sprites) {
	float r = FxElem_Rand(NURSE_TWO_PI, 0.0);

	p->rotVar = rotVar;
	*(uint16_t *)&p->spriteIni = sprites;
	p->rotIni = r;
	p->rotEnd = r;
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
}

void Nurse_HammerFX(cEntity *v) {
	vec3_t off, mr, t, pos, up, one, rot, dir, x1, t2, cam;

	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	CGameModel *model = Obj_Vtbl(v)->GetModel(v);
	if (!model || !CGameModel_IsLoaded(model) || !ChType_ActCtrl(v)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte block = cActCtrl_IsActive(ChType_ActCtrl(v), "ActionBlock");
	off.x = 0.0f;
	float ang = 0.0f;
	if (block) {
		/* the hammer held up: the point higher, the ring turned with the model */
		off.y = -20.0f;
		off.z = -140.0f;
		const vec3_t *r = Vec3_Rotate(&t, &off, CGameModel_GetRot(model, &mr));
		const vec3_t *p = CGameModel_GetPos(model, &pos);
		float px = p->x + r->x;
		float py = p->y + r->y;
		pos.z = p->z + r->z;
		off.x = 0.5f;
		off.y = 1.0f;
		pos.x = px;
		pos.y = py;
		off.z = 0.5f;
		up = *Vec3_Rotate(&t, &off, CGameModel_GetRot(model, &mr));
	} else {
		off.y = -60.0f;
		off.z = -110.0f;
		const vec3_t *r = Vec3_Rotate(&t, &off, CGameModel_GetRot(model, &mr));
		const vec3_t *p = CGameModel_GetPos(model, &pos);
		float px = r->x + p->x;
		float py = p->y + r->y;
		float pz = p->z + r->z;
		up.x = 0.0f;
		up.y = 1.0f;
		up.z = 0.0f;
		pos.x = px;
		pos.y = py;
		pos.z = pz;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXHammerHitFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	cam = g_pWorld->camPos[g_pWorld->curCam];
	CGameModel_SetPos(Debris_Model(d), &cam);
	Debris_Life(d) = 1.5f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXSistPart *el = Debris_Element(d);
	if (!el) {
		return;
	}
	CGameParticles *p = &el->particles;
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	CGameParticles_SetSparksOn(p, 0);
	p->birthRate = 0;
	/* the glow: 6 particles around the point */
	for (int i = 6; i; i--) {
		float az = FxElem_Rand(NURSE_TWO_PI, 0.0);
		float ay = FxElem_Rand(NURSE_TWO_PI, 0.0);
		one.x = 1.0f;
		one.y = 1.0f;
		one.z = 1.0f;
		rot.x = FxElem_Rand(NURSE_TWO_PI, 0.0);
		rot.y = ay;
		rot.z = az;
		Vec3_Rotate(&dir, &one, &rot);
		float s = FxElem_Rand(10.0, 10.0);
		Nurse_PosMin(p, dir.x * s + pos.x, dir.y * s + pos.y, dir.z * s + pos.z);
		p->initVelMin.x = 0.0f;
		p->initVelMin.y = 0.0f;
		p->initVelMin.z = 0.0f;
		p->initVelMax = p->initVelMin;
		p->initAccMin.x = 0.0f;
		p->initAccMin.y = 0.0f;
		p->initAccMin.z = 0.0f;
		p->initAccMax = p->initAccMin;
		p->sizeIni = FxElem_Rand(50.0, 100.0);
		p->sizeEnd = FxElem_Rand(50.0, 350.0);
		p->sizeLife = 0.200000003f;
		p->lifeTime = FxElem_Rand(0.04999998211860657, 0.4000000059604645);
		Nurse_Start(p, 0.0f, 0x202);
		CGameParticles_SetColorRamp(p, NURSE_RAMP_GLOW);
		CGameParticles_EmitN(p, 1, 1);
	}
	/* the ring around up, every second step a fast one too */
	byte odd = 0;
	do {
		rot.x = 0.0f;
		rot.y = ang;
		rot.z = 0.0f;
		x1.x = 1.0f;
		x1.y = 0.0f;
		x1.z = 0.0f;
		dir = *Vec3_Rotate(&dir, Vec3_Rotate(&t2, &x1, &rot), &up);
		odd ^= 1;
		if (odd) {
			Nurse_PosMin(p, dir.x * 4.0f + pos.x, dir.y * 4.0f + pos.y, dir.z * 4.0f + pos.z);
			float s = FxElem_Rand(250.0, 1250.0);
			p->initVelMin.y = dir.y * s;
			p->initVelMin.z = dir.z * s;
			p->initVelMin.x = dir.x * s;
			p->initVelMax = p->initVelMin;
			float vx = -p->initVelMin.x;
			float vy = -p->initVelMin.y;
			float vz = -p->initVelMin.z;
			p->initAccMin.x = vx * 0.699999988f;
			p->initAccMin.y = vy * 0.699999988f;
			p->initAccMin.z = vz * 0.699999988f;
			p->initAccMax = p->initAccMin;
			p->sizeIni = FxElem_Rand(25.0, 150.0);
			p->sizeEnd = FxElem_Rand(50.0, 400.0);
			p->sizeLife = 1.0f;
			p->lifeTime = FxElem_Rand(0.09999999403953552, 0.30000001192092896);
			Nurse_Start(p, 0.0f, 0x202);
			CGameParticles_SetColorRamp(p, NURSE_RAMP_GLOW);
			CGameParticles_EmitDt(p, 1, 1, FxElem_Rand(0.03999999910593033, 0.009999999776482582));
		}
		float s = FxElem_Rand(90.0, 30.0);
		Nurse_PosMin(p, s * dir.x + pos.x, s * dir.y + pos.y, s * dir.z + pos.z);
		float sv = FxElem_Rand(75.0, 300.0);
		p->initVelMin.x = sv * dir.x;
		p->initVelMin.y = sv * dir.y;
		p->initVelMin.z = sv * dir.z;
		p->initVelMax = p->initVelMin;
		float vy = -p->initVelMin.y;
		float vz = -p->initVelMin.z;
		float k = FxElem_Rand(0.10000002384185791, 0.5);
		float vx = -p->initVelMin.x;
		p->initAccMin.x = k * vx;
		p->initAccMin.y = k * vy;
		p->initAccMin.z = k * vz;
		p->initAccMax = p->initAccMin;
		p->sizeIni = FxElem_Rand(25.0, 125.0);
		p->sizeEnd = FxElem_Rand(50.0, 275.0);
		p->sizeLife = 1.0f;
		p->lifeTime = FxElem_Rand(0.40000003576278687, 0.800000011920929);
		Nurse_Start(p, 0.2617993950843811f, 0x1212);
		CGameParticles_SetColorRamp(p, NURSE_RAMPS_STARS + (crt_rand() & 1) * 9);
		CGameParticles_EmitDt(p, 1, 1, FxElem_Rand(0.20000000298023224, 0.0));
		ang = ang + 0.10471975803375244f;
	} while ((float)NURSE_TWO_PI > ang);
	/* the sparks: 32 falling ones */
	Nurse_PosMin(p, pos.x, pos.y, pos.z);
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = -750.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax.x = 0.0f;
	p->initAccMax.y = -750.0f;
	p->initAccMax.z = 0.0f;
	CGameParticles_SetSparksOn(p, 1);
	CGameParticles_SetColorRampOn(p, 1);
	p->sparkMaxSize = 1.0f;
	p->sparkVelScale = 1.0f;
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
	for (int i = 32; i; i--) {
		float az = FxElem_Rand(NURSE_TWO_PI, 0.0);
		float ay = FxElem_Rand(NURSE_TWO_PI, 0.0);
		one.x = 1.0f;
		one.y = 1.0f;
		one.z = 1.0f;
		rot.x = FxElem_Rand(NURSE_TWO_PI, 0.0);
		rot.y = ay;
		rot.z = az;
		Vec3_Rotate(&dir, &one, &rot);
		float s = FxElem_Rand(300.0, 250.0);
		p->initVelMin.x = s * dir.x;
		p->initVelMin.y = s * dir.y;
		p->initVelMin.z = s * dir.z;
		p->initVelMax = p->initVelMin;
		float e = FxElem_Rand(0.009999999776482582, 0.009999999776482582);
		float lt = FxElem_Rand(0.19999998807907104, 0.5);
		CGameParticles_SetColorRamp(p, NURSE_RAMPS_SPARKS + (crt_rand() & 1) * 9);
		p->lifeTime = lt;
		p->sizeLife = 1.0f;
		float sz = FxElem_Rand(25.0, 35.0);
		p->sizeIni = sz;
		p->sizeEnd = sz;
		int r = crt_rand();
		p->spriteEnd = 0x44;
		p->spriteIni = (byte)((r & 1) + 0x41);
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 0, e);
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
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)GAME_FUNC(ChType_NurseSphereHit, CHTYPE_NURSESPHEREHIT));
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
