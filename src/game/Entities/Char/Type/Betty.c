/*
 * Entities/Char/Type/Betty.cpp of the game: Betty, whose shot kills the characters in a sphere in front of her, in
 * slow motion with her camera turned (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/crt.h"
#include "scrap/primitive.h"
#include "scrap/scorer.h"
#include "scrap/vehicleattack.h"

static inline cFollowCamEntity *Betty_Cam(cEntity *v) {
	return (cFollowCamEntity *)WChar(v)->wl.obj.cam;
}

/* the point of the shot in front of v */
static void Betty_HitPos(cEntity *v, vec3_t *out) {
	vec3_t off, t1, t2, t3;

	off.x = g_BettyHitPosX;
	off.y = g_BettyHitPosY;
	off.z = g_BettyHitPosZ;
	const vec3_t *r = Vec3_Rotate(&t1, &off, Obj_Vtbl(v)->GetAng(v, &t2));
	const vec3_t *p = v->vftable->GetPos(v, &t3);
	out->x = p->x + r->x;
	out->y = p->y + r->y;
	out->z = p->z + r->z;
}

/* params: Betty. With 0.9 of energy or more: the shot action, the energy all used, the turn of the camera */
void ChType_BettyActionPress(cAbstractFile *params) {
	char name[0x80];
	vec3_t tmp, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *v = World_FindEntity(name);
	if (!v || !v->vftable->IsType(v, TYEN_WALKCHAR) || WChar(v)->falling || ChType_ActPriority(ChType_ActCtrl(v)) ||
		!(1.0f - WChar(v)->energyUsed >= 0.9f)) {
		return;
	}
	cActCtrl_Play(ChType_ActCtrl(v), "Action", 0, 1.0f);
	cWalkCharEntity_SetEnergy(v, (1.0f - WChar(v)->energyUsed) - 1.0f);
	cWalkCharControl_Reset(&WChar(v)->control, 1);
	cFollowCamEntity *cam = Betty_Cam(v);
	WChar(v)->displAng = 0.0f;
	if (cam) {
		float y = Obj_Vtbl(v)->GetAng(v, &tmp)->y;
		ang.x = 0.0f;
		ang.y = y;
		ang.z = 0.0f;
		WChar_Vtbl(v)->SetViewAng(v, &ang);
		cam->vSpeed = g_BettyVSpeed;
		cam->hSpeed = g_BettyHSpeed;
	}
}

/* params: Betty, "turn,pitch" (degrees), a float: the slow motion and the camera of the aim */
void ChType_BettyCameraSet(cAbstractFile *params) {
	char name[0x80], arg[0x80];
	vec3_t ang;
	float unused;

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, arg, 0x80);
	params->vftable->Read(params, &unused, 4);
	char *pitch = strchr(arg, ',');
	*pitch++ = 0;
	g_BettyEnt = World_FindEntity(name);
	CHECK(g_BettyEnt);
	CHECK(Entity_IsType(g_BettyEnt, TYEN_WALKCHAR));
	cFollowCamEntity *cam = Betty_Cam(g_BettyEnt);
	if (!cam) {
		return;
	}
	cTimer_SetSpeedChecked(&g_pWorld->timer, g_BettyHitTimeSpeed);
	cam->vSpeed = g_BettyVSpeed;
	cam->hSpeed = g_BettyHSpeed;
	cam->addTurn = Cam_Deg2Rad(crt_atof(arg));
	float y = WChar(g_BettyEnt)->view.y;
	ang.x = Cam_Deg2Rad(crt_atof(pitch));
	ang.y = y;
	ang.z = 0.0f;
	WChar_Vtbl(g_BettyEnt)->SetViewAng(g_BettyEnt, &ang);
}

void ChType_DirAngles(const vec3_t *dir, vec3_t *out) {
	vec3_t tmp;

	*out = *Vec3_GetRotAng(&tmp, dir);
}

/* the sphere test of the shot: params a character in the sphere, killed when it is not too far above or below
 * Betty, with the kill effect turned towards it from the point of the shot */
void ChType_BettyKill(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, hp, dir, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	CHECK(g_BettyEnt);
	CHECK(Entity_IsType(g_BettyEnt, TYEN_WALKCHAR));
	if (!name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e || g_BettyEnt == e || !e->vftable->IsType(e, TYEN_WALKCHAR) || WChar(e)->wl.god ||
		WChar(e)->wl.life <= 0) {
		return;
	}
	const vec3_t *p = g_BettyEnt->vftable->GetPos(g_BettyEnt, &t1);
	const vec3_t *q = e->vftable->GetPos(e, &t2);
	float dy = (float)__builtin_fabs((double)(q->y - p->y));
	if (!(g_BettyHitTopLimit > dy)) {
		return;
	}
	SaveVar_Inc("Stats.Betty", 1);
	SaveVar_Inc("Stats.Killer", 1);
	cVehicleAttack_HitBy(&WChar(e)->wl.attack, g_BettyEnt->name);
	if (!WChar(e)->wl.god) {
		WChar(e)->wl.life = 0;
	}
	cEntity_CollisionOff(e);
	cPrimitive_SetSize(&WChar(e)->wl.prim, 0.0f);
	Alarm_Set(&g_pWorld->alarm, 1.0f);
	cObjElementFXKill *el = FX_DropNew(g_FXBettyKillFile, 1.0f);
	if (!el) {
		return;
	}
	if (!el->started) {
		el->started = 1;
	}
	cObjElementFXKill_Start(el, e);
	Betty_HitPos(g_BettyEnt, &hp);
	const vec3_t *ep = e->vftable->GetPos(e, &t3);
	dir.y = ep->y - hp.y;
	dir.z = ep->z - hp.z;
	dir.x = ep->x - hp.x;
	ChType_DirAngles(&dir, &ang);
	cObjElementFXKill_SetAngles(el, ang.x, ang.y, ang.z);
}

/* params: Betty, the turn of the camera (degrees), a float: she stops and looks ahead again */
void ChType_BettyEndEvent(cAbstractFile *params) {
	char name[0x80], arg[0x80];
	vec3_t v;
	float unused;

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, arg, 0x80);
	params->vftable->Read(params, &unused, 4);
	g_BettyEnt = World_FindEntity(name);
	CHECK(g_BettyEnt);
	CHECK(Entity_IsType(g_BettyEnt, TYEN_WALKCHAR));
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = 0.0f;
	Obj_Vtbl(g_BettyEnt)->SetVel(g_BettyEnt, &v);
	cWalkCharControl_Reset(&WChar(g_BettyEnt)->control, 0);
	WChar_Vtbl(g_BettyEnt)->SetViewAng(g_BettyEnt, Obj_Vtbl(g_BettyEnt)->GetAng(g_BettyEnt, &v));
	cFollowCamEntity *cam = Betty_Cam(g_BettyEnt);
	WChar(g_BettyEnt)->displAng = 0.0f;
	if (cam) {
		cam->addTurn = Cam_Deg2Rad(crt_atof(arg));
	}
}

/* params: Betty, the charge (0 to 16), a float: the shot, its sphere smaller for a stronger charge */
void ChType_BettyHitEvent(cAbstractFile *params) {
	char name[0x80], arg[0x80];
	vec3_t hp, tmp;

	ChType_ReadParams(params, name, arg);
	g_BettyEnt = World_FindEntity(name);
	CHECK(g_BettyEnt);
	CHECK(Entity_IsType(g_BettyEnt, TYEN_WALKCHAR));
	/* the point of the shot is computed and not used */
	Betty_HitPos(g_BettyEnt, &hp);
	float charge = (float)crt_atof(arg);
	/* x87: 1 - charge / 16 */
	float k = (float)(1.0L - (long double)charge * 0.0625L);
	float radius = (1.0f - k * k * (2.0f - k)) * g_BettyHitRadius;
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)GAME_FUNC(ChType_BettyKill));
	const vec3_t *pos = g_BettyEnt->vftable->GetPos(g_BettyEnt, &tmp);
	cWorld_SphereCallF(g_pWorld, 0x10, pos, radius, g_ChTypeSphereFunc, 0);
	if (WChar(g_BettyEnt)->wl.obj.cam) {
		cPad_Rumble(&g_Pads[0], k, 0.3f, 0.35f);
	}
}

/* params: Betty, an event, a float: the world speed back */
void ChType_BettyRestoreTime(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	g_BettyEnt = World_FindEntity(name);
	CHECK(g_BettyEnt);
	CHECK(Entity_IsType(g_BettyEnt, TYEN_WALKCHAR));
	cTimer_SetSpeedChecked(&g_pWorld->timer, g_BettyHitTimeSpeed);
	cTimer_SetSpeedTo(&g_pWorld->timer, 1.0f);
	cTimer_SetBlend(&g_pWorld->timer, g_BettyHitTimeFader);
}

/* params: Betty, an event, a float: the shot effect, a shake of the camera and the white fade of the player */
void ChType_FXBettyAttack(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t hp;

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	Betty_HitPos(v, &hp);
	cFollowCamEntity *cam = Betty_Cam(v);
	if (cam) {
		cam->quakeFactor = 0.0244346093f;
		cam->quakeTime = 0.150000006f;
		cScorer *sc = Scorer_Get(0);
		if (sc) {
			ScorerFade_Start(&sc->fade, 0xffffffff, 0x00ffffff, 0.5f);
		}
	}
	cObjElementFXBettyAttack *el = FX_DropNew(g_FXBettyAttackFile, 1.89999998f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		FXMisc_SetEntity(el, v);
		cObjElementFXBettyAttack_Emit(el, hp.x, hp.y, hp.z);
	}
}
