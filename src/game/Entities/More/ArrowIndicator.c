/*
 * Entities/More/ArrowIndicator.cpp: cArrowIndicator, the use and mission arrows of the entities with life, see
 * include/scrap/arrowindicator.h.
 */
#include <string.h>

#include "scrap/arrowindicator.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/withlifeentity.h"
#include "scrap/staticinit.h"

#define ARROW_POOL_MAX 5			/* more free models are deleted */
#define ARROW_FADE_SPEED 4.0f
#define ARROW_GLOW_TIME 0.66f
#define CSTRING_CTOR 0x133980
#define CSTRING_DTOR 0x133990

/* called after a new use or view function */
static inline void Arrow_FuncsChanged(cEntity *parent) {
	WithLife_Vtbl(parent)->UnlinkListen(parent);
}

float *Vec2_Normalize(float *out, const float *in) {
	float t[2];
	float l = (float)__builtin_sqrt((double)(in[0] * in[0] + in[1] * in[1]));

	if (l == 0.0f) {
		t[0] = in[0];
		t[1] = in[1];
	} else {
		float k = 1.0f / l;
		t[0] = in[0] * k;
		t[1] = in[1] * k;
	}
	out[0] = t[0];
	out[1] = t[1];
	return out;
}

cArrowIndicator *cArrowIndicator_ctor(cArrowIndicator *self) {
	crt_eh_vector_ctor(self->opts, sizeof(cString), ARROW_MAX_OPTS, (void *)cString_ctor, (void *)cString_dtor);
	cPy_Function_ctor(&self->onUse);
	cPy_Function_ctor(&self->onView);
	self->parent = NULL;
	self->arrow = NULL;
	self->mission = NULL;
	self->glow = NULL;
	self->missionOn = 0;
	self->alpha = 0.0f;
	self->shown = 0;
	self->selected = 0;
	self->canUse = 0;
	self->canView = 0;
	self->canMission = 0;
	self->alwaysUse = 0;
	self->visible = 0;
	self->tgType = 3;
	self->coneStatus = 0;
	self->coneFov = 0x1.0c1524p+0f;		/* 60 degrees */
	self->coneDist = 1000.0f;
	self->canPhoto = 0;
	self->useAngSel = 1;
	self->useAbsAngSel = 0;
	self->useMapCheck = 1;
	self->iOpt = 0;
	self->nOpt = 0;
	for (int i = 0; i < ARROW_MAX_OPTS; i++) {
		cString_Set(&self->opts[i], NULL);
	}
	return self;
}

cArrowModel *g_ArrowPool;

int g_ArrowPoolCount;

/* the model to the pool, or deleted when the pool is full */
static void Arrow_ToPool(cArrowModel *m) {
	if (g_ArrowPoolCount > ARROW_POOL_MAX) {
		CGameModel_Reset(&m->model);
		operator_delete_sized(m, sizeof(cArrowModel));
		return;
	}
	m->next = g_ArrowPool;
	g_ArrowPool = m;
	CGameModel_Disable(&m->model);
	g_ArrowPoolCount++;
}

void cArrowIndicator_dtor(cArrowIndicator *self) {
	if (self->arrow) {
		Arrow_ToPool(self->arrow);
	}
	self->arrow = NULL;
	cArrowIndicator_ReleaseMission(self);
	cPy_Function_dtor(&self->onView);
	cPy_Function_dtor(&self->onUse);
	crt_eh_vector_dtor(self->opts, sizeof(cString), ARROW_MAX_OPTS, (void *)cString_dtor);
}

byte cArrowIndicator_AddOpt(cArrowIndicator *self, const char *s) {
	if (!strchr(s, ':')) {
		Con_Verbosef("T.ERROR : '%s' can´t be added at the options list,\n '%c' is not included as separator\n", s, ':');
		return 0;
	}
	int n = self->nOpt;
	if (n >= ARROW_MAX_OPTS) {
		Con_Verbosef("T.ERROR : '%s' can´t be added at the options list,\n '%i' is the maximun number of options!\n", s, ARROW_MAX_OPTS);
		return 0;
	}
	self->nOpt = n + 1;
	cString_Set(&self->opts[n], s);
	return 1;
}

void cArrowIndicator_Execute(cArrowIndicator *self, cEntity *user) {
	char buf[0x80];
	cPy_Function *f = self->canUse ? &self->onUse : &self->onView;

	cPy_Function_AddParamS(f, user->name);
	cPy_Function_AddParamS(f, self->parent->name);
	if (!self->nOpt) {
		cPy_Function_AddParamS(f, "");
		cPy_Function_AddParamS(f, "");
	} else {
		const char *opt = self->opts[self->iOpt];
		const char *colon = strchr(opt, ':');
		if (!colon) {
			CHECK(!"newcad");
		}
		uint32_t n = (uint32_t)(colon - opt);
		if (n >= sizeof(buf)) {
			crt_report_rangecheckfailure();
		}
		memcpy(buf, opt, n);
		buf[n] = 0;
		cPy_Function_AddParamS(f, buf);
		cPy_Function_AddParamS(f, colon + 1);
	}
	cPy_Function_Execute(f, 0);
}

byte ArrowIndicator_Visible(cEntity *self, cEntity *other, float ang, float maxDist, float *outDist, byte mark,
	byte useAng) {
	vec3_t ps, po, a;
	float v[2], r[2], n[2], s, c;

	if (self == other || !self->active || !other->active ||
		(!WLife(other)->arrow.canUse && !WLife(other)->arrow.canView)) {
		return 0;
	}
	const vec3_t *p = self->vftable->GetPos(self, &ps);
	const vec3_t *q = other->vftable->GetPos(other, &po);
	float dx = q->x - p->x;
	float dy = q->y - p->y;
	float dz = q->z - p->z;
	float l = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	float ro = WLife(other)->prim.radius;
	float d = l - (WLife(self)->prim.radius + ro);
	if (mark) {
		WLife(other)->arrow.shown = g_ArrowNearDist > d;
	}
	if (!(maxDist > d)) {
		return 0;
	}
	if (!(WLife(self)->prim.radius + ro + g_ArrowYMargin > (float)__builtin_fabs((double)dy))) {
		return 0;
	}
	c = 1.0f;
	if (useAng && WLife(self)->arrow.useAngSel) {
		if (WLife(other)->arrow.useAngSel) {
			v[0] = dx;
			v[1] = dz;
			const float *rv = Vec2_Rotate(r, v, ang + 0x1.921fb6p+0f);
			c = Vec2_Normalize(n, rv)[0];
		} else if (WLife(other)->arrow.useAbsAngSel) {
			const vec3_t *oa = Obj_Vtbl(other)->GetAng(other, &a);
			Float_SinCos(ang - oa->y + M_PI_F, &s, &c);
		}
	}
	if (!(c >= (float)cos(0.7853981633974483))) {
		return 0;
	}
	if (WLife(other)->arrow.useMapCheck && WLife(self)->arrow.useMapCheck) {
		const vec3_t *to = other->vftable->GetPos(other, &po);
		const vec3_t *from = self->vftable->GetPos(self, &ps);
		if (Engine_TestLine(from, to, NULL, NULL)) {
			return 0;
		}
	}
	if (outDist) {
		*outDist = d;
	}
	return 1;
}

void GameModelList_Free(void) {
	cArrowModel *m = g_ArrowPool;

	if (m) {
		do {
			cArrowModel *next = m->next;
			CGameModel_Reset(&m->model);
			operator_delete_sized(m, sizeof(cArrowModel));
			g_ArrowPoolCount--;
			m = next;
			g_ArrowPool = m;
		} while (m);
	}
	CHECK(g_ArrowPoolCount == 0);
}

void ArrowIndicator_ClearSel(void) {
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e->active && e->vftable->IsType(e, TYEN_WITH_LIFE)) {
			WLife(e)->arrow.selected = 0;
			WLife(e)->arrow.shown = 0;
		}
	}
}

void cArrowIndicator_ClearOpts(cArrowIndicator *self) {
	self->iOpt = 0;
	self->nOpt = 0;
	for (int i = 0; i < ARROW_MAX_OPTS; i++) {
		cString_Set(&self->opts[i], NULL);
	}
}

cEntity *ArrowIndicator_FindSel(cEntity *self, float ang) {
	vec3_t pos;
	cEntity *sel = NULL;

	self->vftable->GetPos(self, &pos);
	WLife(self)->arrow.shown = 0;
	WLife(self)->arrow.selected = 0;
	float best = g_ArrowMaxDist;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->vftable->IsType(e, TYEN_WITH_LIFE)) {
			continue;
		}
		WLife(e)->arrow.selected = 0;
		if (ArrowIndicator_Visible(self, e, ang, best, &best, 1, 1)) {
			sel = e;
		}
	}
	if (sel) {
		WLife(sel)->arrow.selected = 1;
	}
	return sel;
}

byte cArrowIndicator_VarF(cArrowIndicator *self, const char *name, double *d, int set) {
	if (!strcmp(name, "ConeDist")) {
		if ((byte)set) {
			self->coneDist = (float)*d;
		} else {
			*d = self->coneDist;
		}
		return 1;
	}
	if (!strcmp(name, "ConeFOV")) {
		if ((byte)set) {
			self->coneFov = (float)((*d * M_PI_D) / 180.0);
		} else {
			*d = ((double)self->coneFov * 180.0) / M_PI_D;
		}
		return 1;
	}
	return 0;
}

cArrowModel *ArrowPool_Get(byte *fresh) {
	cArrowModel *m = g_ArrowPool;

	*fresh = m == NULL;
	if (m) {
		g_ArrowPoolCount--;
		g_ArrowPool = m->next;
		return m;
	}
	m = operator_new(sizeof(cArrowModel));
	if (m) {
		CGameModel_ctor(&m->model, NULL);
	}
	return m;
}

const char *cArrowIndicator_GetOpt(cArrowIndicator *self, int i) {
	if (i >= 0 && i < self->nOpt) {
		return self->opts[i];
	}
	return NULL;
}

uint32_t cArrowIndicator_GetColor(cArrowIndicator *self) {
	switch (self->tgType) {
	case 0:
		return 0xffc4c4;
	case 1:
		return 0xc4c4ff;
	case 2:
		return 0xc4ffc4;
	case 4:
		return 0xffa4a4;
	default:
		return 0xffffff;
	}
}

/* a flag of the arrow from the int variable */
static inline void Arrow_VarFlag(byte *flag, int *i, int set) {
	if ((byte)set) {
		*flag = *i != 0;
	} else {
		*i = *flag != 0;
	}
}

byte cArrowIndicator_VarI(cArrowIndicator *self, const char *name, int *i, int set) {
	if (!strcmp(name, "CanPhoto")) {
		Arrow_VarFlag(&self->canPhoto, i, set);
		return 1;
	}
	if (!strcmp(name, "AlwaysUse")) {
		Arrow_VarFlag(&self->alwaysUse, i, set);
		return 1;
	}
	if (!strcmp(name, "ArrowCanView")) {
		Arrow_VarFlag(&self->canView, i, set);
		return 1;
	}
	if (!strcmp(name, "ArrowCanMission")) {
		Arrow_VarFlag(&self->canMission, i, set);
		return 1;
	}
	if (!strcmp(name, "ArrowCanUse")) {
		Arrow_VarFlag(&self->canUse, i, set);
		return 1;
	}
	if (!strcmp(name, "ArrnOpt")) {
		if (!(byte)set) {
			*i = self->nOpt;
			return 1;
		}
		if (!*i) {
			cArrowIndicator_ClearOpts(self);
			return 1;
		}
		Con_Bluef("<Entities>.ArrnOpt only can be 0!");
		return 0;
	}
	if (!strcmp(name, "ArriOpt")) {
		if (!(byte)set) {
			*i = self->iOpt;
			return 1;
		}
		return cArrowIndicator_SetIOpt(self, *i);
	}
	if (!strcmp(name, "UseAngSel")) {
		Arrow_VarFlag(&self->useAngSel, i, set);
		return 1;
	}
	if (!strcmp(name, "UseMapCheck")) {
		Arrow_VarFlag(&self->useMapCheck, i, set);
		return 1;
	}
	if (!strcmp(name, "UseAbsAngSel")) {
		Arrow_VarFlag(&self->useAbsAngSel, i, set);
		return 1;
	}
	return 0;
}

void cArrowIndicator_NextOpt(cArrowIndicator *self) {
	if (self->nOpt > 1) {
		self->iOpt = (self->iOpt + 1) % self->nOpt;
	}
}

void cArrowIndicator_PrevOpt(cArrowIndicator *self) {
	int n = self->nOpt;

	if (n > 1) {
		self->iOpt = (self->iOpt - 1 + n) % n;
	}
}

void cArrowIndicator_ReleaseMission(cArrowIndicator *self) {
	if (self->glow) {
		if (self->glow->base.ParticleSystem) {
			CGameParticles_Release(&self->glow->base);
		}
		cArrowGlow *g = self->glow;
		if (g) {
			CGameParticles_Release(&g->base);
			operator_delete_sized(g, sizeof(cArrowGlow));
		}
		self->glow = NULL;
	}
	CGameModel *m = self->mission;
	if (m) {
		CGameModel_Reset(m);
		operator_delete_sized(m, sizeof(CGameModel));
	}
	self->mission = NULL;
}

void cArrowIndicator_ReleaseArrow(cArrowIndicator *self) {
	if (self->arrow) {
		Arrow_ToPool(self->arrow);
	}
	self->arrow = NULL;
}

/* the model of the "on" or "off" mission arrow */
static inline const char *Arrow_MissionName(const cArrowIndicator *self) {
	return self->missionOn ? *g_MissionArrowOn : *g_MissionArrowOff;
}

void cArrowIndicator_SetModels(cArrowIndicator *self, byte on) {
	if (!on || !(self->alpha > 0.0f)) {
		cArrowIndicator_ReleaseArrow(self);
		cArrowIndicator_ReleaseMission(self);
		return;
	}
	if (self->canMission) {
		if (!self->mission) {
			cArrowIndicator_LoadMission(self, Arrow_MissionName(self));
		}
		cArrowIndicator_ReleaseArrow(self);
		return;
	}
	if (!self->canUse && !self->canView) {
		return;
	}
	if (!self->arrow) {
		cArrowIndicator_LoadArrow(self, g_ArrowUseModel);
	}
	cArrowIndicator_ReleaseMission(self);
}

void cArrowIndicator_SetShown(cArrowIndicator *self, byte on) {
	self->shown = on;
}

void cArrowIndicator_LoadMission(cArrowIndicator *self, const char *name) {
	if (self->glow && self->glow->base.ParticleSystem) {
		CGameParticles_Release(&self->glow->base);
	}
	self->missionOn = !cString_ICmpStr(g_MissionArrowOn, name);
	if (!self->mission) {
		CGameModel *m = operator_new(sizeof(CGameModel));
		self->mission = m ? CGameModel_ctor(m, NULL) : NULL;
	}
	cArrowIndicator_LoadModel(self, self->mission, name);
	if (!self->glow) {
		cArrowGlow *g = operator_new(sizeof(cArrowGlow));
		if (g) {
			CGameParticles_ctor(&g->base);
		}
		self->glow = g;
	}
	self->glow->timer = 0.0f;
	cArrowGlow_Attach(self->glow, self->mission, name);
}

void cArrowIndicator_LoadArrow(cArrowIndicator *self, const char *name) {
	byte fresh;

	if (self->arrow) {
		return;
	}
	cArrowModel *m = ArrowPool_Get(&fresh);
	self->arrow = m;
	if (fresh) {
		cArrowIndicator_LoadModel(self, &m->model, name);
		return;
	}
	CGameModel_Enable(&m->model);
}

void cArrowIndicator_LoadModel(cArrowIndicator *self, CGameModel *model, const char *name) {
	char buf[0x200];

	(void)self;
	Str_PrintfQuiet(buf, sizeof(buf), "%s.M3D", name);
	CGameModel_LoadName(model, buf);
	CGameModel_SetPivotNode(model, "DC_Root");
	Str_PrintfQuiet(buf, sizeof(buf), "%s.anim.M3D", name);
	int a = CGameModel_AddAnim(model, buf);
	CGameModel_SetAnim(model, a, 0, 0);
}

void cArrowIndicator_SetUseFunc(cArrowIndicator *self, const char *name) {
	cPy_Function_SetFunc(&self->onUse, name);
	Arrow_FuncsChanged(self->parent);
}

void cArrowIndicator_SetViewFunc(cArrowIndicator *self, const char *name) {
	cPy_Function_SetFunc(&self->onView, name);
	Arrow_FuncsChanged(self->parent);
}

byte cArrowIndicator_SetOpt(cArrowIndicator *self, int i, const char *s) {
	if (i == self->nOpt) {
		return cArrowIndicator_AddOpt(self, s);
	}
	if (i >= 0 && i < self->nOpt) {
		cString_Set(&self->opts[i], s);
		return 1;
	}
	return 0;
}

void cArrowIndicator_SetParent(cArrowIndicator *self, cEntity *parent) {
	CHECK(self->parent == NULL);
	self->parent = parent;
}

void cArrowGlow_SetVisible(cArrowGlow *self, byte on) {
	if (self->base.ParticleSystem) {
		CGameParticles_SetVisible(&self->base, on);
	}
}

void cArrowGlow_Attach(cArrowGlow *self, CGameModel *model, const char *name) {
	char node[20];
	const char *big = "";
	const char *kind;

	if (!model || !CGameModel_IsLoaded(model)) {
		CHECK(!"model && model->HasModel()");
	}
	CHECK(name);
	self->scale = 1.0f;
	if (!strstr(name, "/B")) {
		self->scale = 0.1f;
	} else {
		big = "B";
	}
	self->ramp = 0;
	if (strstr(name, "/SM")) {
		self->ramp = 1;
		kind = "S";
	} else {
		kind = "M";
	}
	memcpy(node, "DOB_", 5);
	crt_strncat(node, big, sizeof(node) - strlen(node) - 1);
	crt_strncat(node, kind, sizeof(node) - strlen(node) - 1);
	crt_strncat(node, "Mission_Glow", sizeof(node) - strlen(node) - 1);
	int i = CGameModel_FindRenderNode(model, node);
	CHECK(i > -1);
	CGameParticles_Attach(&self->base, model, (uint32_t)i);
}

byte cArrowIndicator_SetIOpt(cArrowIndicator *self, int i) {
	if (i >= 0 && i < self->nOpt) {
		self->iOpt = i;
		return 1;
	}
	return 0;
}

void cArrowIndicator_Update(cArrowIndicator *self) {
	cObjEntity *p = (cObjEntity *)self->parent;
	vec3_t off, a, r, rp, pos, rot;

	if (!self->parent->active) {
		cArrowIndicator_ReleaseArrow(self);
		cArrowIndicator_ReleaseMission(self);
		return;
	}
	if (self->canMission) {
		self->shown = 1;
	} else if (!self->canUse && !self->canView) {
		self->shown = 0;
	}
	float t = Timer_GetTime(&p->timer);
	float dt = cTimer_GetDelta(&p->timer);
	if (1e-6f >= dt) {
		return;
	}
	if (!self->shown) {
		float al = self->alpha;
		if (0.0f >= al) {
			cArrowIndicator_ReleaseArrow(self);
			cArrowIndicator_ReleaseMission(self);
			self->alpha = 0.0f;
			return;
		}
		self->alpha = al - dt * ARROW_FADE_SPEED;
	} else {
		float al = dt * ARROW_FADE_SPEED + self->alpha;
		self->alpha = al;
		if (al > 1.0f) {
			self->alpha = 1.0f;
		}
	}
	if (!(self->alpha > 0.0f)) {
		return;
	}
	byte mission = self->canMission;
	if (!mission && !self->canUse && !self->canView) {
		return;
	}
	cEntity *e = self->parent;
	off = Obj(e)->camOff;
	const vec3_t *rr = Vec3_Rotate(&r, &off, Obj_Vtbl(e)->GetAng(e, &a));
	const vec3_t *q = Obj_Vtbl(self->parent)->GetRenderPos(self->parent, &rp);
	pos.x = q->x + rr->x;
	float y = q->y + rr->y;
	pos.z = q->z + rr->z;
	if (1.0f > self->alpha) {
		float g = 1.0f - self->alpha;
		float h = g * g * (2.0f - g);
		y = y + h * h * (2.0f - h) * 300.0f;
	}
	pos.y = (float)(sin((double)(t * 3.0f)) * 5.0) + y;
	cArrowIndicator_SetModels(self, 1);
	/* the model of the kind chosen before SetModels made it */
	CGameModel *m = mission ? self->mission : (CGameModel *)self->arrow;
	CGameModel_SetPos(m, &pos);
	if (self->canMission && CGameModel_IsVisible(m) && self->glow) {
		cArrowGlow_Update(self->glow, m);
	}
	const vec3_t *cam = &g_pWorld->camPos[g_pWorld->curCam];
	float cz = cam->z;
	float cx = cam->x;
	rot.x = 0.0f;
	rot.y = (float)atan2((double)(pos.x - cx), (double)(pos.z - cz));
	rot.z = 0.0f;
	CGameModel_SetRot(m, &rot);
	float st;
	cEntity *usr;
	if (self->selected || (self->missionOn && cWorld_GetUsrEntity(g_pWorld, 0) &&
		(usr = cWorld_GetUsrEntity(g_pWorld, 0), usr->vftable->IsType(usr, TYEN_CAR)))) {
		st = dt;
	} else {
		st = dt * 0.125f;
	}
	CGameModel_RootMotion(m, st, NULL, NULL);
}

uint32_t g_ArrowGlowRamps[2][9] = {
	{ 0x80ffff00, 0x84ffff00, 0x94ffff00, 0xa4ffff00, 0xb9ffff00, 0xcdffff00, 0xe8ffff00, 0xffffff00, 0xffffff00 },
	{ 0x8000ffff, 0x8400ffff, 0x9400ffff, 0xa400ffff, 0xb900ffff, 0xcd00ffff, 0xe800ffff, 0xff00ffff, 0xff00ffff },
};

void cArrowGlow_Update(cArrowGlow *self, CGameModel *model) {
	CGameParticles *pt = &self->base;
	vec3_t mp;

	float t = cTimer_GetDelta(&g_pWorld->timer) + self->timer;
	self->timer = t;
	if (ARROW_GLOW_TIME > t) {
		return;
	}
	self->timer = 0.0f;
	const vec3_t *cam = &g_pWorld->camPos[g_pWorld->curCam];
	vec3_t c = *cam;
	const vec3_t *p = CGameModel_GetPos(model, &mp);
	float dy = c.y - p->y;
	float dx = c.x - p->x;
	float dz = c.z - p->z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) / (self->scale * g_ArrowGlowDist);
	float k;
	if (g_ArrowGlowMin > d) {
		k = g_ArrowGlowMin;
	} else if (g_ArrowGlowMax > d) {
		k = d;
	} else {
		k = g_ArrowGlowMax;
	}
	pt->birthRate = 0;
	CGameParticles_SetColorRampOn(pt, 1);
	CGameParticles_SetColorRamp(pt, g_ArrowGlowRamps[self->ramp]);
	pt->lifeTime = 0.18f;
	pt->sizeIni = k * 170.0f * self->scale;
	float h = k * 0.5f;
	pt->sizeLife = 0.3f;
	pt->spriteIni = 5;
	pt->spriteEnd = 5;
	h = h * 145.0f;
	pt->sizeEnd = k * 650.0f * self->scale;
	memset(&pt->initVelMin, 0, sizeof(pt->initVelMin));
	pt->initVelMax = pt->initVelMin;
	memset(&pt->initAccMin, 0, sizeof(pt->initAccMin));
	pt->initAccMax = pt->initAccMin;
	float s = self->scale;
	h = h * s;
	float y0 = s * 600.0f - h * 1.5f;
	float ys[4];
	ys[0] = y0;
	ys[1] = y0 + h;
	ys[2] = h + h + y0;
	ys[3] = h * 3.0f + y0;
	for (int i = 0; i < 4; i++) {
		pt->initPosMin.x = 0.0f;
		pt->initPosMin.z = 0.0f;
		pt->initPosMin.y = ys[i];
		pt->initPosMax = pt->initPosMin;
		CGameParticles_Apply(pt);
		CGameParticles_EmitN(pt, 1, 0);
	}
}

byte cArrowIndicator_VarS(cArrowIndicator *self, const char *name, char **s, int set) {
	if ((byte)set && !strcmp(name, "AddArrOpt")) {
		cArrowIndicator_AddOpt(self, *s);
		return 1;
	}
	if (!strcmp(name, "ConeStatus")) {
		if ((byte)set) {
			if (!crt_stricmp(*s, "off")) {
				self->coneStatus = 0;
			} else if (!crt_stricmp(*s, "relax")) {
				self->coneStatus = 1;
			} else if (!crt_stricmp(*s, "alert")) {
				self->coneStatus = 2;
			} else if (!crt_stricmp(*s, "danger")) {
				self->coneStatus = 3;
			} else {
				Con_Errorf("ERROR : '%s' invalid value for <Entity>.ConeStatus \n", *s);
			}
			return 1;
		}
		switch (self->coneStatus) {
		case 0:
			*s = "off";
			break;
		case 1:
			*s = "relax";
			break;
		case 2:
			*s = "alert";
			break;
		case 3:
			*s = "danger";
			break;
		default:
			*s = "<unknow>";
			break;
		}
		return 1;
	}
	if (!strcmp(name, "TgType")) {
		if ((byte)set) {
			if (!crt_stricmp(*s, "killer")) {
				self->tgType = 4;
			} else if (!crt_stricmp(*s, "green")) {
				self->tgType = 2;
			} else if (!crt_stricmp(*s, "red")) {
				self->tgType = 0;
			} else if (!crt_stricmp(*s, "blue")) {
				self->tgType = 1;
			} else if (!crt_stricmp(*s, "none")) {
				self->tgType = 3;
			} else {
				Con_Errorf("ERROR : '%s' invalid value for <Entity>.TgType \n", *s);
			}
			return 1;
		}
		switch (self->tgType) {
		case 0:
			*s = "red";
			break;
		case 1:
			*s = "blue";
			break;
		case 2:
			*s = "green";
			break;
		case 3:
			*s = "none";
			break;
		case 4:
			*s = "killer";
			break;
		default:
			*s = "<unknow>";
			break;
		}
		return 1;
	}
	if (!crt_strncmp(name, "ArrOpt", 6)) {
		uint32_t n = (uint32_t)crt_atoi(name + 6);
		if (n <= ARROW_MAX_OPTS) {
			if (!(byte)set) {
				*s = (char *)cArrowIndicator_GetOpt(self, (int)n);
				return 1;
			}
			return cArrowIndicator_SetOpt(self, (int)n, *s);
		}
	}
	if (!(byte)set) {
		return 0;
	}
	if (!strcmp(name, "UseFunc")) {
		cArrowIndicator_SetUseFunc(self, *s);
		return 1;
	}
	if (!strcmp(name, "ViewFunc")) {
		cArrowIndicator_SetViewFunc(self, *s);
		return 1;
	}
	if (!strcmp(name, "ArrowModUse")) {
		cArrowIndicator_LoadArrow(self, *s);
		return 1;
	}
	if (!strcmp(name, "ArrowModMission")) {
		cArrowIndicator_LoadMission(self, *s);
		return 1;
	}
	return 0;
}

void MissionArrows_Switch(const char *on, const char *off) {
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);

	cString_Set(g_MissionArrowOn, on);
	cString_Set(g_MissionArrowOff, off);
	for (; e; e = cEntity_Next(e)) {
		if (e->vftable->IsType(e, TYEN_WITH_LIFE) && WLife(e)->arrow.mission &&
			CGameModel_IsLoaded(WLife(e)->arrow.mission)) {
			cArrowIndicator_LoadMission(&WLife(e)->arrow, Arrow_MissionName(&WLife(e)->arrow));
		}
	}
}

cGVar GVar_ArrowViewRadius;

cGVar GVar_ArrowActiveHorizontal;

cGVar GVar_ArrowMissionFile;

cGVar GVar_ArrowUseFile;

cGVar GVar_ArrowTargetFile;

cGVar GVar_FXArrowIndicatorFlash_SizeMax;

cGVar GVar_FXArrowIndicatorFlash_DistGrow;

cGVar GVar_FXArrowIndicatorFlash_SizeMin;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_ArrowUseFile, &GVar_ArrowUseFile)
INIT_GVAR_STRING(Init_GVar_ArrowUseFile, Fini_GVar_ArrowUseFile, &GVar_ArrowUseFile, "ArrowUseFile", 0, "This file is the floating use arrow.", "Models/Misc/Action/Action")
FINI_GVAR_STRING(Fini_GVar_ArrowMissionFile, &GVar_ArrowMissionFile)
INIT_GVAR_STRING(Init_GVar_ArrowMissionFile, Fini_GVar_ArrowMissionFile, &GVar_ArrowMissionFile, "ArrowMissionFile", 0, "This file is the floating arrow.", "Models/Misc/MMission/MMission")
FINI_GVAR_STRING(Fini_GVar_ArrowTargetFile, &GVar_ArrowTargetFile)
INIT_GVAR_STRING(Init_GVar_ArrowTargetFile, Fini_GVar_ArrowTargetFile, &GVar_ArrowTargetFile, "ArrowTargetFile", 0, "This file is the floating arrow.", "Models/Misc/SMission/SMission")
INIT_GVAR_FLOAT(Init_GVar_ArrowViewRadius, &GVar_ArrowViewRadius, "ArrowViewRadius", 0, "Indicates the radius between entities to view the indicator", 500.0f)
INIT_GVAR_FLOAT(Init_GVar_ArrowActiveHorizontal, &GVar_ArrowActiveHorizontal, "ArrowActiveHorizontal", 0, "The Horizontal distance betweem the both spheres", 100.0f)
INIT_GVAR_FLOAT(Init_GVar_FXArrowIndicatorFlash_DistGrow, &GVar_FXArrowIndicatorFlash_DistGrow, "FXArrowIndicatorFlash_DistGrow", 0, "Arrow Indicator Flash FX Distance for Grow", 6500.0f)
INIT_GVAR_FLOAT(Init_GVar_FXArrowIndicatorFlash_SizeMin, &GVar_FXArrowIndicatorFlash_SizeMin, "FXArrowIndicatorFlash_SizeMin", 0, "Arrow Indicator Flash FX Min Size (Mult)", 2.5f)
INIT_GVAR_FLOAT(Init_GVar_FXArrowIndicatorFlash_SizeMax, &GVar_FXArrowIndicatorFlash_SizeMax, "FXArrowIndicatorFlash_SizeMax", 0, "Arrow Indicator Flash FX Max Size (Mult)", 6.0f)
