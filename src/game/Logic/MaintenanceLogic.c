/*
 * Logic/MaintenanceLogic.cpp of the game: cMaintenanceLocalLogic (include/scrap/logic.h), the maintenance robot of
 * the ship editor (a GenChar logic). With the camera "EditCamera" and an active ship it builds the box of corners
 * around the ship on the platform "DM_Door_EditShip_00" (state 8) and walks from corner to corner (a neighbour of
 * the same level or the corner above or below, by chance), MaintenanceTimeToNextPos at each; the script reads the
 * point next to the ship it faces ("RepositionPoint").
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/scrapmodel.h"
#include "scrap/staticinit.h"

#define CMAINTENANCELOCALLOGIC_VFTABLE (&cMaintenanceLocalLogic_vftable.base)
#define g_MaintenanceTimeToNextPos (GVar_MaintenanceTimeToNextPos.value.f)
#define g_MaintenanceSphereRadiusFactor (GVar_MaintenanceSphereRadiusFactor.value.f)
static const cGenCharLocalLogicVtbl cMaintenanceLocalLogic_vftable;
static cGVar GVar_MaintenanceTimeToNextPos;
static cGVar GVar_MaintenanceSphereRadiusFactor;
#define MAINT_VT(self) ((self)->base.base.vftable)
#define MAINT_BASE(self) (&(self)->base.base)

static cEntity *g_MaintenanceShip;
static cEntity *g_MaintenanceFirstShip;

void *cMaintenanceLocalLogic_ctor(void *mem, cEntity *parent) {
	cMaintenanceLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(&self->base, parent, 1);
	MAINT_VT(self) = CMAINTENANCELOCALLOGIC_VFTABLE;
	self->timer = g_MaintenanceTimeToNextPos;
	self->point = -1;
	g_MaintenanceShip = NULL;
	g_MaintenanceFirstShip = NULL;
	if ((byte)cBaseLocalLogic_SetState(&(MAINT_BASE(self))->logic, 7, NULL)) {
		cEntity *p = MAINT_BASE(self)->parent;
		cAIChar *ai = p->vftable->GetAI(p);
		ai->vftable->SetState(&ai->base.base, 1, (double)g_GenCharRelaxSpeed, 0);
	}
	return self;
}

static cLocalLogic *cMaintenanceLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cMaintenanceLocalLogic *self = (cMaintenanceLocalLogic *)elem;
	MAINT_VT(self) = CMAINTENANCELOCALLOGIC_VFTABLE;
	g_MaintenanceShip = NULL;
	g_MaintenanceFirstShip = NULL;
	cGenCharLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* the corners of the box of half size radius * MaintenanceSphereRadiusFactor around (x, y, z); the lower ones
 * not below the platform (its y, the radius of the robot and 100) */
static void cMaintenanceLocalLogic_SetPlatform(cMaintenanceLocalLogic *self, float x, float y, float z,
	float radius) {
	vec3_t t;
	float r = g_MaintenanceSphereRadiusFactor * radius;
	float xp = x + r, yp = y + r, zp = z + r;
	float nr = -r;
	float zm = nr + z, xm = nr + x, ym = nr + y;
	cEntity *p = MAINT_BASE(self)->parent;
	vec3_t *pt = self->points;

	pt[0] = (vec3_t){ xp, yp, zp };
	pt[1] = (vec3_t){ xp, yp, zm };
	pt[2] = (vec3_t){ xm, yp, zp };
	pt[3] = (vec3_t){ xm, yp, zm };
	pt[4] = (vec3_t){ xp, ym, zp };
	pt[5] = (vec3_t){ xp, ym, zm };
	pt[6] = (vec3_t){ xm, ym, zp };
	pt[7] = (vec3_t){ xm, ym, zm };
	cEntity *platform = World_FindEntity("DM_Door_EditShip_00");
	CHECK(platform && "La plataforma no existe");
	float py = platform->vftable->GetPos(platform, &t)->y;
	float h = (float)((Obj_Vtbl(p)->GetRadius(p)) + py + 100.0f);
	for (int i = 4; i < 8; i++) {
		if (h > pt[i].y) {
			pt[i].y = h;
		}
	}
}

/* the first active ship of the world, NULL while it has an effect */
static cEntity *Maintenance_FindShip(void) {
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);

	while (e && !(e->active && e->vftable->IsType(e, TYEN_CAR))) {
		e = cEntity_Next(e);
	}
	if (!e) {
		return NULL;
	}
	if (!e->vftable->IsType(e, TYEN_CAR)) {
		e = NULL;
	}
	if (CGameModel_GetEffect(&Obj(e)->model, NULL)) {
		return NULL;
	}
	return e;
}

static void cMaintenanceLocalLogic_NextPoint(cMaintenanceLocalLogic *self);

/* vf17: 7 waits for the editor and a ship, 8 works at the corners while it is the same ship */
static void cMaintenanceLocalLogic_Update(cLocalLogic *elem, float dt) {
	cMaintenanceLocalLogic *self = (cMaintenanceLocalLogic *)elem;
	vec3_t t;
	cBaseLocalLogic *b = MAINT_BASE(self);
	cEntity *p = b->parent;
	cAIChar *ai = p->vftable->GetAI(p);

	if (b->state == 7) {
		if (!World_FindEntity("EditCamera")) {
			return;
		}
		cEntity *s = Maintenance_FindShip();
		g_MaintenanceShip = s;
		g_MaintenanceFirstShip = s;
		if (!s) {
			return;
		}
		const vec3_t *sp = s->vftable->GetPos(s, &t);
		float r = Obj_Vtbl(g_MaintenanceShip)->GetRadius(g_MaintenanceShip);
		cMaintenanceLocalLogic_SetPlatform(self, sp->x, sp->y, sp->z, r);
		MAINT_VT(self)->SetState(&self->base.base.logic, 8, NULL);
		return;
	}
	if (b->state != 8) {
		cGenCharLocalLogic_Update(&self->base.base.logic, dt);
		return;
	}
	g_MaintenanceShip = Maintenance_FindShip();
	if (!g_MaintenanceShip || !World_FindEntity("EditCamera") || g_MaintenanceFirstShip != g_MaintenanceShip) {
		self->point = -1;
		MAINT_VT(self)->SetState(&self->base.base.logic, 7, NULL);
		return;
	}
	void *ctrl = ChType_ActCtrl(p);
	float tm = self->timer - dt;
	self->timer = tm;
	if (0.0f >= tm) {
		self->timer = g_MaintenanceTimeToNextPos;
		cMaintenanceLocalLogic_NextPoint(self);
		cActCtrl_Play(ctrl, "WorkEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ctrl, "Walk", 1);
		return;
	}
	if (strncmp(cActCtrl_GetActionName(ctrl, 0), "Work", 4) && ai->vftable->GetState(&ai->base.base) == 0) {
		cActCtrl_Play(ctrl, "WorkInit", 1, 1.0f);
		cActCtrl_SetDefLoop(ctrl, "Work", 1);
	}
}

/* the first time the nearest of the corners 0..6, then a neighbour: the next or the third next of the level, or
 * the same corner of the other level; the AI walks there faced to the ship, RepositionPoint the point of the ship
 * radii from it towards the corner */
static void cMaintenanceLocalLogic_NextPoint(cMaintenanceLocalLogic *self) {
	vec3_t t, sp;
	cEntity *p = MAINT_BASE(self)->parent;
	cAIChar *ai = p->vftable->GetAI(p);

	if (self->point == -1) {
		float best = 3.4028235e38f;
		for (int i = 0; i < 7; i++) {
			const vec3_t *pp = p->vftable->GetPos(p, &t);
			const vec3_t *c = &self->points[i];
			float dy = c->y - pp->y, dx = c->x - pp->x, dz = c->z - pp->z;
			float d = dy * dy + dx * dx + dz * dz;
			if (best > d) {
				best = d;
				self->point = i;
			}
		}
	} else {
		int r = crt_rand() % 3;
		int cur = self->point;
		int n = cur >= 4 ? 8 : 4;
		if (r == 0 || r == 1) {
			int k = (cur + (r == 0 ? 1 : 3)) % n;
			self->point = k;
			if (cur >= 4 && k < 4) {
				self->point = k + 4;
			}
		} else if (r == 2) {
			self->point = cur + (cur < 4 ? 4 : -4);
		}
	}
	g_MaintenanceShip->vftable->GetPos(g_MaintenanceShip, &sp);
	const vec3_t *c = &self->points[self->point];
	vec3_t dir;
	dir.z = sp.z - c->z;
	dir.x = sp.x - c->x;
	dir.y = sp.y - c->y;
	ai->vftable->SetState(&ai->base.base, 4, *c, dir, 100.0, 0.5);
	c = &self->points[self->point];
	sp.y = c->y;
	float sr = WLife(g_MaintenanceShip)->prim.radius;
	vec3_t d;
	d.x = c->x - sp.x;
	d.z = c->z - sp.z;
	d.y = c->y - sp.y;
	vec3_t v;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len == 0.0f) {
		v = d;
	} else {
		float inv = 1.0f / len;
		v.x = d.x * inv;
		v.y = d.y * inv;
		v.z = d.z * inv;
	}
	float rr = WLife(p)->prim.radius + sr;
	self->reposition.x = v.x * rr + sp.x;
	self->reposition.y = v.y * rr + sp.y;
	self->reposition.z = v.z * rr + sp.z;
}

/* vf19: 7 walks relaxed, 8 begins the work at the next corner */
static byte cMaintenanceLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cMaintenanceLocalLogic *self = (cMaintenanceLocalLogic *)elem;
	cBaseLocalLogic *b = MAINT_BASE(self);

	if (state == 7) {
		if ((byte)cBaseLocalLogic_SetState(&b->logic, 7, target)) {
			cAIChar *ai = b->parent->vftable->GetAI(b->parent);
			ai->vftable->SetState(&ai->base.base, 1, (double)g_GenCharRelaxSpeed, 0);
		}
		return 1;
	}
	if (state != 8) {
		return cGenCharLocalLogic_SetState(&self->base.base.logic, state, target);
	}
	cBaseLocalLogic_SetState(&b->logic, 8, target);
	void *ctrl = ChType_ActCtrl(b->parent);
	cMaintenanceLocalLogic_NextPoint(self);
	cActCtrl_Play(ctrl, "WorkEnd", 1, 1.0f);
	cActCtrl_SetDefLoop(ctrl, "Walk", 1);
	return 1;
}

/* vf06: "RepositionPoint" (read only, y positive) */
static byte cMaintenanceLocalLogic_VarV(cLocalLogic *base, const char *name, double *x, double *y, double *z, byte set) {
	cMaintenanceLocalLogic *self = (cMaintenanceLocalLogic *)base;
	if (!set && !strcmp(name, "RepositionPoint")) {
		*x = (double)self->reposition.x;
		*y = __builtin_fabs((double)self->reposition.y);
		*z = (double)self->reposition.z;
		return 1;
	}
	return cBaseLocalLogic_VarV(&(MAINT_BASE(self))->logic, name, x, y, z, set);
}

/* the vtable of cMaintenanceLocalLogic */
static const cGenCharLocalLogicVtbl cMaintenanceLocalLogic_vftable = {
	.base.base.Delete = cMaintenanceLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cGenCharLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cMaintenanceLocalLogic_VarV,
	.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.OnAttack = cGenCharLocalLogic_OnRobbery,
	.base.OnShot = cBaseLocalLogic_OnShot,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.Update = cMaintenanceLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cMaintenanceLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cGenCharLocalLogic_UpdateView,
	.ToRelax = cGenCharLocalLogic_ToRelax,
	.ToAlert = cGenCharLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cLocalLogic_FindTargetNull,
	.TargetDone = cLocalLogic_TargetDoneTrue,
	.Work = cLocalLogic_WorkNop,
	.Vf33 = cLocalLogic_Vf33Nop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};

static cGVar GVar_MaintenanceTimeToNextPos;
static cGVar GVar_MaintenanceSphereRadiusFactor;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MaintenanceTimeToNextPos, &GVar_MaintenanceTimeToNextPos, "MaintenanceTimeToNextPos", 0, "Maintenance time to next pos in cube", 8.0f)
INIT_GVAR_FLOAT(Init_GVar_MaintenanceSphereRadiusFactor, &GVar_MaintenanceSphereRadiusFactor, "MaintenanceSphereRadiusFactor", 0, "Maintenance sphere radius factor", 1.0f)
