/*
 * Engine/NodeFX.cpp of the engine3d: the node effects, see include/scrap/nodefx.h.
 */
#include "scrap/archio.h"
#include "scrap/fonts.h"
#include "scrap/global.h"
#include "scrap/nodefx.h"
#include "scrap/scrapmodel.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"
#include "scrap/e3dutil.h"
#include "scrap/staticinit.h"

#define CNODEFX_SPRING_VFTABLE (&CNodeFX_Spring_vftable)
#define CNODEFX_WHEEL_VFTABLE (&CNodeFX_Wheel_vftable)
#define INODEFX_VFTABLE (&INodeFX_vftable)
#define g_NodeFXSpringDamp1 (GVar_NodeFXSpringDamp1.value.f)	/* NodeFXSpringDamp1 */
#define g_NodeFXSpringDamp2 (GVar_NodeFXSpringDamp2.value.f)	/* NodeFXSpringDamp2 */
#define g_NodeFXSpringDebug (GVar_NodeFXSpringDebug.value.i)	/* NodeFXSpringDebug: the variables instead of the members */
#define g_NodeFXSpringMSec (GVar_NodeFXSpringMSec.value.u)	/* NodeFXSpringMSec, the step */
#define g_NodeFXSpringMidP (GVar_NodeFXSpringMidP.value.f)	/* NodeFXSpringMidP */
#define g_NodeFXSpringRot (GVar_NodeFXSpringRot.value.f)	/* NodeFXSpringRot */
#define g_NodeFXSpringRotX (GVar_NodeFXSpringRotX.value.f)	/* NodeFXSpringRotX, degrees */
#define g_NodeFXSpringShow (GVar_NodeFXSpringShow.value.i)	/* NodeFXSpringShow */
#define g_NodeFXSpringSprng (GVar_NodeFXSpringSprng.value.f)	/* NodeFXSpringSprng */
#define g_NodeFXWheelRadius (GVar_NodeFXWheelRadius.value.f)	/* NodeFXWheelRadius */
#define g_NodeFXWheelShow (GVar_NodeFXWheelShow.value.i)	/* NodeFXWheelShow */
#define g_NodeFXWheelSlide (GVar_NodeFXWheelSlide.value.f)	/* NodeFXWheelSlide */
static const INodeFXVtbl INodeFX_vftable;
static cGVar GVar_NodeFXWheelSlide;
static cGVar GVar_NodeFXWheelShow;
static cGVar GVar_NodeFXWheelRadius;
static cGVar GVar_NodeFXSpringSprng;
static cGVar GVar_NodeFXSpringShow;
static cGVar GVar_NodeFXSpringRotX;
static cGVar GVar_NodeFXSpringRot;
static cGVar GVar_NodeFXSpringMidP;
static cGVar GVar_NodeFXSpringMSec;
static cGVar GVar_NodeFXSpringDebug;
static cGVar GVar_NodeFXSpringDamp2;
static cGVar GVar_NodeFXSpringDamp1;
static const INodeFXVtbl CNodeFX_Wheel_vftable;
static const INodeFXVtbl CNodeFX_Spring_vftable;

/* the distance of a and b */
static float Vec3_Dist(const vec3_t *a, const vec3_t *b) {
	float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

static INodeFX *INodeFX_Delete(INodeFX *self, byte flags) {
	self->vftable = INODEFX_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(INodeFX));
	}
	return self;
}

static byte INodeFX_Type(INodeFX *self) {
	(void)self;
	return NODEFX_NONE;
}

uint32_t g_TimeMs;
static const char str_NodeEffect[];
/* the section "NodeEffect" of the user properties */
static const char *g_NodeEffectSect = (void *)str_NodeEffect;

/* ---- CNodeFX_Wheel ---- */

/* type 0 Wheel, 1 Roll: the defaults of the properties */
static CNodeFX_Wheel *CNodeFX_Wheel_ctor(CNodeFX_Wheel *self, CNode3D *node, int type) {
	static const vec3_t zero = { 0.0f, 0.0f, 0.0f }, one = { 1.0f, 1.0f, 1.0f };
	vec3_t tmp;

	self->vftable = CNODEFX_WHEEL_VFTABLE;
	CHECK(node && node->ini);
	if (type == 0 || type == 1) {
		CIniFile *ini = node->ini;
		/* Wheel turns by the movement, Roll by itself */
		const vec3_t *move = type == 0 ? &one : &zero;
		self->radius = CIniFile_GetFloat(ini, g_NodeEffectSect, "Radius", type == 0 ? 1.0f : 0.0f);
		self->rot = *CIniFile_GetVec3(ini, &tmp, g_NodeEffectSect, "Rotation", move);
		self->trans = *CIniFile_GetVec3(ini, &tmp, g_NodeEffectSect, "Translation", move);
		self->roll = *CIniFile_GetVec3(ini, &tmp, g_NodeEffectSect, "Roll", type == 0 ? &zero : &one);
		self->noiseVel = *CIniFile_GetVec3(ini, &tmp, g_NodeEffectSect, "NoiseVel", &zero);
		self->noiseAmt = *CIniFile_GetVec3(ini, &tmp, g_NodeEffectSect, "NoiseAmt", &zero);
	} else {
		CHECK(0);
	}
	E3D_LogWhite((&g_FileLog), "CNodeFX_Wheel:  Radius: %f", (double)self->radius);
	const char *trans = Vec3_Str(&self->trans, 1, 3);
	const char *rot = Vec3_Str(&self->rot, 0, 3);
	E3D_LogWhite((&g_FileLog), "  Rotation: %s  Translation: %s", rot, trans);
	const char *amt = Vec3_Str(&self->noiseAmt, 2, 3);
	const char *vel = Vec3_Str(&self->noiseVel, 1, 3);
	const char *roll = Vec3_Str(&self->roll, 0, 3);
	E3D_LogWhite((&g_FileLog), "  Roll: %s  NoiseVel: %s  NoiseAmt: %s", roll, vel, amt);
	self->ang = zero;
	self->noiseBase = (uint32_t)(crt_rand() % 20000) + g_TimeMs;
	return self;
}

static CNodeFX_Wheel *CNodeFX_Wheel_ctorCopy(CNodeFX_Wheel *self, const CNodeFX_Wheel *src) {
	self->vftable = CNODEFX_WHEEL_VFTABLE;
	self->radius = src->radius;
	self->rot = src->rot;
	self->trans = src->trans;
	self->roll = src->roll;
	self->noiseVel = src->noiseVel;
	self->noiseAmt = src->noiseAmt;
	self->ang = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->noiseBase = (uint32_t)(crt_rand() % 20000) + g_TimeMs;
	return self;
}

/* not to be used: CHECK(0) */
static INodeFX *CNodeFX_Wheel_Delete(INodeFX *base, byte flags) {
	CNodeFX_Wheel *self = (CNodeFX_Wheel *)base;
	self->vftable = INODEFX_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CNodeFX_Wheel));
	}
	return base;
}

static byte CNodeFX_Wheel_Type(INodeFX *base) {
	CNodeFX_Wheel *self = (CNodeFX_Wheel *)base;
	(void)self;
	return NODEFX_WHEEL;
}

static int CNodeFX_Wheel_Apply(INodeFX *base, matrix_t *m, CNode3D *node, CRenderNode *r) {
	CNodeFX_Wheel *self = (CNodeFX_Wheel *)base;
	CHECK(r && node && node->parent);
	if (r->frame == g_FrameCount) {
		return 0;
	}
	if (r->frame + 1 < g_FrameCount) {
		self->ang = (vec3_t){ 0.0f, 0.0f, 0.0f };
		self->noiseBase = (uint32_t)(crt_rand() % 20000) + g_TimeMs;
	}
	CRenderNode_SetLook(r, 1);
	float dt = g_FrameSec;
	vec3_t cur = r->look->r1.ang;
	vec3_t a = { cur.x + self->roll.x * dt, cur.y + self->roll.y * dt, cur.z + self->roll.z * dt };
	float rad = self->radius * g_NodeFXWheelRadius;
	vec3_t pos = { m->m[12], m->m[13], m->m[14] };
	vec3_t t = { pos.x - r->m.m[12], pos.y - r->m.m[13], pos.z - r->m.m[14] };
	if (rad != 0.0f) {
		matrix_t tr;
		/* the movement in the space of the parent */
		Matrix_Transpose(&tr, &node->parent->m);
		Vec3_TransformDir0(&t, &t, &tr);
		vec3_t s = { self->trans.x * t.x, self->trans.y * t.y, self->trans.z * t.z };
		float slide = g_NodeFXWheelSlide;
		vec3_t k = { (1.0f - self->trans.x) * t.x * slide, (1.0f - self->trans.y) * t.y * slide,
			(1.0f - self->trans.z) * t.z * slide };
		float l1 = (float)__builtin_sqrt((double)(s.y * s.y + s.x * s.x + s.z * s.z));
		float l2 = (float)__builtin_sqrt((double)(k.y * k.y + k.x * k.x + k.z * k.z));
		float dist = l1 + l2;
		if (!(s.y + s.x + s.z > 0.0f)) {
			dist = -dist;
		}
		dist = dist / rad;
		a.x = a.x + self->rot.x * dist;
		a.y = a.y + self->rot.y * dist;
		a.z = a.z + self->rot.z * dist;
	}
	self->ang.x = Ang_Wrap(a.x);
	self->ang.y = Ang_Wrap(a.y);
	self->ang.z = Ang_Wrap(a.z);
	CRotation *r1 = &r->look->r1;
	r1->ang = self->ang;
	r1->flags |= ROT_DIRTY;
	if (self->ang.x == 0.0f && self->ang.y == 0.0f && self->ang.z == 0.0f) {
		r1->flags |= ROT_ZERO;
	} else {
		r1->flags &= ~ROT_ZERO;
	}
	if (self->noiseVel.x != 0.0f || self->noiseVel.y != 0.0f || self->noiseVel.z != 0.0f) {
		float tm = (float)(double)(g_TimeMs - self->noiseBase + 20000);
		float nx = tm * self->noiseVel.x * 0.001f;
		float ny = self->noiseVel.y * tm * 0.001f;
		float nz = self->noiseVel.z * tm * 0.001f;
		float sx = (float)sin((double)nx);
		float sy = (float)sin((double)ny);
		float sz = (float)sin((double)nz);
		vec3_t v = { self->noiseAmt.x * sx, self->noiseAmt.y * sy, self->noiseAmt.z * sz };
		Vec3_TransformDir0(&v, &v, &node->m);
		Matrix_AddPos(m, &v);
	}
	if (g_NodeFXWheelShow) {
		vec3_t e, r2;
		Matrix_ToEuler(&e, m);
		r2 = r->look->r1.ang;
		Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &pos, 0xff00, "r1: %s", Vec3_StrDeg(&e, 0, 2));
		Debug_Text3D(g_FntTerm8, 0.0f, 12.0f, &pos, 0xff00, "r2: %s", Vec3_StrDeg(&r2, 0, 2));
		Debug_Text3D(g_FntTerm8, 0.0f, 24.0f, &pos, 0xff00, "t: %s", Vec3_Str(&t, 0, 6));
		Debug_Text3D(g_FntTerm8, 0.0f, 36.0f, &pos, 0xff00, "b: %d", self->noiseBase);
		vec3_t zero = { 0.0f, 0.0f, 0.0f };
		Debug_DrawSphereM(m, &zero, rad, 0xff, 12, 12);
	}
	return 0;
}

/* ---- CNodeFX_Spring ---- */

/* the positions and the times cleared */
static void Spring_Clear(CNodeFX_Spring *self) {
	self->prev = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->next = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->prevMSec = 0;
	self->nextMSec = 0;
}

/* type 0 Spring, 1 Cloth, 2 Hair, 3 Bust */
static CNodeFX_Spring *CNodeFX_Spring_ctor(CNodeFX_Spring *self, CNode3D *node, int type) {
	self->vftable = CNODEFX_SPRING_VFTABLE;
	CHECK(node && node->ini);
	self->damp2 = 1.0f;
	switch (type) {
	case 0:
		self->damping = -0.85f;
		self->spring = -0.02f;
		self->torsion = 0.8f;
		break;
	case 1:
		self->damping = 0.7f;
		self->spring = 0.5f;
		self->torsion = 0.5f;
		break;
	case 2:
		self->damping = 0.0f;
		self->spring = 0.6f;
		self->torsion = 0.1f;
		self->damp2 = 0.3f;
		break;
	case 3:
		self->damping = 0.0f;
		self->spring = 0.5f;
		self->torsion = 0.2f;
		self->damp2 = 0.6f;
		break;
	default:
		CHECK(0);
		break;
	}
	self->damping = CIniFile_GetFloat(node->ini, g_NodeEffectSect, "Damping", self->damping);
	self->spring = CIniFile_GetFloat(node->ini, g_NodeEffectSect, "Spring", self->spring);
	self->torsion = CIniFile_GetFloat(node->ini, g_NodeEffectSect, "Torsion", self->torsion);
	self->damp2 = CIniFile_GetFloat(node->ini, g_NodeEffectSect, "Damp2", self->damp2);
	E3D_LogWhite((&g_FileLog), "CNodeFX_Spring:  Damping: %f  Spring: %f  Torsion: %f  Damp2: %f", (double)self->damping,
		(double)self->spring, (double)self->torsion, (double)self->damp2);
	Spring_Clear(self);
	return self;
}

static CNodeFX_Spring *CNodeFX_Spring_ctorCopy(CNodeFX_Spring *self, const CNodeFX_Spring *src) {
	self->vftable = CNODEFX_SPRING_VFTABLE;
	self->damping = src->damping;
	self->spring = src->spring;
	self->torsion = src->torsion;
	self->damp2 = src->damp2;
	Spring_Clear(self);
	return self;
}

static INodeFX *CNodeFX_Spring_Delete(INodeFX *base, byte flags) {
	CNodeFX_Spring *self = (CNodeFX_Spring *)base;
	self->vftable = INODEFX_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CNodeFX_Spring));
	}
	return base;
}

static byte CNodeFX_Spring_Type(INodeFX *base) {
	CNodeFX_Spring *self = (CNodeFX_Spring *)base;
	(void)self;
	return NODEFX_SPRING;
}

static int CNodeFX_Spring_Apply(INodeFX *base, matrix_t *m, CNode3D *node, CRenderNode *r) {
	CNodeFX_Spring *self = (CNodeFX_Spring *)base;
	CNode3D *parent = node->parent;
	float damping, spring, torsion, damp2;
	vec3_t p;

	if (g_NodeFXSpringDebug) {
		spring = g_NodeFXSpringSprng;
		torsion = g_NodeFXSpringRot;
		damping = g_NodeFXSpringDamp1;
		damp2 = g_NodeFXSpringDamp2;
	} else {
		spring = self->spring;
		damping = self->damping;
		torsion = self->torsion;
		damp2 = self->damp2;
	}
	if (r->frame + 1 < g_FrameCount) {
		self->prevMSec = 0;
		self->nextMSec = 0;
		self->next = (vec3_t){ m->m[12], m->m[13], m->m[14] };
	}
	if (g_TimeMs < self->prevMSec || g_TimeMs >= self->nextMSec) {
		/* a step: the spring pulls the last position to the node */
		vec3_t mp = { m->m[12], m->m[13], m->m[14] };
		vec3_t n = self->next;
		vec3_t q;
		q.x = mp.x + (n.x - mp.x) * damping + self->vel.x;
		q.y = self->vel.y + (mp.y + (n.y - mp.y) * damping);
		q.z = self->vel.z + (mp.z + (n.z - mp.z) * damping);
		self->vel.x = (q.x - n.x) * spring;
		self->vel.y = (q.y - n.y) * spring;
		self->vel.z = (q.z - n.z) * spring;
		if (parent) {
			/* kept at the distance of the node from its parent */
			vec3_t *o = (vec3_t *)&parent->m.m[12];
			float d = Vec3_Dist(&mp, o);
			vec3_t v = { q.x - o->x, q.y - o->y, q.z - o->z };
			Vec3_Unit(&v);
			q.x = d * v.x + o->x;
			q.y = d * v.y + o->y;
			q.z = d * v.z + o->z;
		}
		self->prev = self->next;
		self->next = q;
		self->prevMSec = g_TimeMs;
		self->nextMSec = g_NodeFXSpringMSec + g_TimeMs;
		p = self->prev;
	} else {
		CHECK(self->nextMSec != self->prevMSec);
		float t = (float)(double)(g_TimeMs - self->prevMSec) / (float)(double)(self->nextMSec - self->prevMSec);
		float u = 1.0f - t;
		p.x = self->next.x * t + self->prev.x * u;
		p.y = self->next.y * t + self->prev.y * u;
		p.z = self->next.z * t + self->prev.z * u;
	}
	vec3_t mp = { m->m[12], m->m[13], m->m[14] };
	vec3_t o = parent ? parent->m.pos : mp;
	if (parent) {
		vec3_t a = { mp.x + (p.x - mp.x) * damp2, mp.y + (p.y - mp.y) * damp2, mp.z + (p.z - mp.z) * damp2 };
		float d = Vec3_Dist(&mp, &o);
		vec3_t v = { a.x - o.x, a.y - o.y, a.z - o.z };
		Vec3_Unit(&v);
		vec3_t b = { v.x * d + o.x, v.y * d + o.y, v.z * d + o.z };
		/* the node turned from its place to the new one around the mid point to the parent */
		float k = g_NodeFXSpringMidP, ok = 1.0f - k;
		vec3_t c = { m->m[12] * k + o.x * ok, m->m[13] * k + o.y * ok, m->m[14] * k + o.z * ok };
		vec3_t e = { m->m[12] - c.x, m->m[13] - c.y, m->m[14] - c.z };
		Vec3_Unit(&e);
		vec3_t f = { b.x - c.x, b.y - c.y, b.z - c.z };
		Vec3_Unit(&f);
		float dot = f.y * e.y + f.x * e.x + f.z * e.z;
		if (0.9999f > dot) {
			vec3_t axis = { f.z * e.y - f.y * e.z, f.x * e.z - f.z * e.x, f.y * e.x - f.x * e.y };
			Vec3_Unit(&axis);
			float c2 = -1.0f >= dot ? -1.0f : Float_MinSS(1.0f, dot);
			float ang = (float)acos((double)c2);
			quat_t q;
			matrix_t rm, tmp;
			Quat_ToMatrixD3D(&rm, Quat_FromAxisAngle(&q, &axis, -(ang * torsion)));
			*m = *Matrix_MulD3D(&tmp, m, &rm);
		}
		m->m[12] = b.x;
		m->m[13] = b.y;
		m->m[14] = b.z;
	}
	if (g_NodeFXSpringRotX != 0.0f) {
		matrix_t rx, tmp;
		Matrix_RotationX(&rx, (float)((double)g_NodeFXSpringRotX * M_PI_D / 180.0));
		*m = *Matrix_MulD3D(&tmp, &rx, m);
	}
	if (g_NodeFXSpringShow) {
		vec3_t zero = { 0.0f, 0.0f, 0.0f };
		Debug_DrawSphereM(m, &zero, 2.0f, 0xff, 12, 12);
		vec3_t v = { m->m[12] + self->vel.x * 4.0f, m->m[13] + self->vel.y * 4.0f, m->m[14] + self->vel.z * 4.0f };
		Debug_DrawSphere(&v, 1.0f, 0xffffff, 6, 6);
		Debug_DrawSphere(&self->prev, 1.0f, 0xff00, 6, 6);
		Debug_DrawSphere(&o, 1.0f, 0xff0000, 10, -1);
	}
	return 0;
}

/* ---- the factory ---- */

void *NodeFX_New(const char *type, CNode3D *node) {
	char tmp[0x20];
	void *p;

	if (!type) {
		CHECK(CIniFile_GetText(node->ini, "NodeEffect", "Type", tmp, sizeof(tmp), NULL));
		type = tmp;
	}
	CHECK(type && node);
	E3D_LogWhite((&g_FileLog), "NodeFX: [%s] in node: %s", type, node->name);
	if (!crt_stricmp(type, "Wheel")) {
		p = operator_new(sizeof(CNodeFX_Wheel));
		return p ? CNodeFX_Wheel_ctor(p, node, 0) : NULL;
	}
	if (!crt_stricmp(type, "Roll")) {
		p = operator_new(sizeof(CNodeFX_Wheel));
		return p ? CNodeFX_Wheel_ctor(p, node, 1) : NULL;
	}
	if (!crt_stricmp(type, "Spring")) {
		p = operator_new(sizeof(CNodeFX_Spring));
		return p ? CNodeFX_Spring_ctor(p, node, 0) : NULL;
	}
	if (!crt_stricmp(type, "Cloth")) {
		p = operator_new(sizeof(CNodeFX_Spring));
		return p ? CNodeFX_Spring_ctor(p, node, 1) : NULL;
	}
	if (!crt_stricmp(type, "Hair")) {
		p = operator_new(sizeof(CNodeFX_Spring));
		return p ? CNodeFX_Spring_ctor(p, node, 2) : NULL;
	}
	if (!crt_stricmp(type, "Bust")) {
		p = operator_new(sizeof(CNodeFX_Spring));
		return p ? CNodeFX_Spring_ctor(p, node, 3) : NULL;
	}
	E3D_LogBlue((&g_FileLog), "Unknown Node Effect: %s", type);
	return NULL;
}

#define NODEFX_TYPE(fx) ((fx)->vftable->Type(fx))

void *NodeFX_Clone(INodeFX *fx) {
	void *p;

	CHECK(fx);
	if (NODEFX_TYPE(fx) == NODEFX_WHEEL) {
		p = operator_new(sizeof(CNodeFX_Wheel));
		return p ? CNodeFX_Wheel_ctorCopy(p, (CNodeFX_Wheel *)fx) : NULL;
	}
	if (NODEFX_TYPE(fx) == NODEFX_SPRING) {
		p = operator_new(sizeof(CNodeFX_Spring));
		return p ? CNodeFX_Spring_ctorCopy(p, (CNodeFX_Spring *)fx) : NULL;
	}
	E3D_LogBlue((&g_FileLog), "Unknown Node Effect: %d", NODEFX_TYPE(fx));
	return NULL;
}

/* the vtable of INodeFX */
static const INodeFXVtbl INodeFX_vftable = {
	.Delete = INodeFX_Delete,
	.Type = INodeFX_Type,
	.Apply = (void *)crt_purecall,
};

/* the vtable of CNodeFX_Wheel */
static const INodeFXVtbl CNodeFX_Wheel_vftable = {
	.Delete = CNodeFX_Wheel_Delete,
	.Type = CNodeFX_Wheel_Type,
	.Apply = CNodeFX_Wheel_Apply,
};

/* the vtable of CNodeFX_Spring */
static const INodeFXVtbl CNodeFX_Spring_vftable = {
	.Delete = CNodeFX_Spring_Delete,
	.Type = CNodeFX_Spring_Type,
	.Apply = CNodeFX_Spring_Apply,
};

static const char str_NodeEffect[] = "NodeEffect";
static cGVar GVar_NodeFXSpringMidP;
static cGVar GVar_NodeFXSpringDamp2;
static cGVar GVar_NodeFXWheelShow;
static cGVar GVar_NodeFXSpringMSec;
static cGVar GVar_NodeFXWheelRadius;
static cGVar GVar_NodeFXSpringDebug;
static cGVar GVar_NodeFXSpringRotX;
static cGVar GVar_NodeFXSpringShow;
/* the engine variables of the effects */
static cGVar GVar_NodeFXSpringDamp1;
static cGVar GVar_NodeFXSpringRot;
static cGVar GVar_NodeFXWheelSlide;
static cGVar GVar_NodeFXSpringSprng;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_NodeFXWheelRadius, &GVar_NodeFXWheelRadius, "NodeFXWheelRadius", 0, "NodeFX Wheel radius multiplier", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_NodeFXWheelSlide, &GVar_NodeFXWheelSlide, "NodeFXWheelSlide", 0, "NodeFX Wheel sliding factor", 1.0f)
INIT_GVAR_INT(Init_GVar_NodeFXWheelShow, &GVar_NodeFXWheelShow, "NodeFXWheelShow", 0, "NodeFX Wheel debug info", 0)
INIT_GVAR_FLOAT(Init_GVar_NodeFXSpringDamp2, &GVar_NodeFXSpringDamp2, "NodeFXSpringDamp2", 0, "NodeFX Spring damp 2 (global damp)", 0.6f)
INIT_GVAR_FLOAT(Init_GVar_NodeFXSpringDamp1, &GVar_NodeFXSpringDamp1, "NodeFXSpringDamp1", 0, "NodeFX Spring damp 1 (spring damp)", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_NodeFXSpringSprng, &GVar_NodeFXSpringSprng, "NodeFXSpringSprng", 0, "NodeFX Spring coef 1 (spring coef)", 0.3f)
INIT_GVAR_FLOAT(Init_GVar_NodeFXSpringRot, &GVar_NodeFXSpringRot, "NodeFXSpringRot", 0, "NodeFX Spring rotation coef.", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_NodeFXSpringRotX, &GVar_NodeFXSpringRotX, "NodeFXSpringRotX", 0, "NodeFX Spring rotation x (deg)", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_NodeFXSpringMidP, &GVar_NodeFXSpringMidP, "NodeFXSpringMidP", 0, "NodeFX Spring mid-point", 0.5f)
INIT_GVAR_INT(Init_GVar_NodeFXSpringMSec, &GVar_NodeFXSpringMSec, "NodeFXSpringMSec", 0, "NodeFX Spring calc. period (msec)", 50)
INIT_GVAR_INT(Init_GVar_NodeFXSpringShow, &GVar_NodeFXSpringShow, "NodeFXSpringShow", 0, "NodeFX Spring debug info", 0)
INIT_GVAR_INT(Init_GVar_NodeFXSpringDebug, &GVar_NodeFXSpringDebug, "NodeFXSpringDebug", 0, "NodeFX Spring override (0/1)", 0)
