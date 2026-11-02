/*
 * The scene graph nodes of the engine3d, original source Engine/Nodo3D.cpp (include/scrap/node3d.h), with the inline
 * helpers of RenderCtx.h and objeto3d.h compiled into its range.
 */
#include <string.h>

#include "scrap/animgroup.h"
#include "scrap/meshd3d.h"
#include "scrap/gvar.h"
#include "scrap/camera.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/fonts.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/trimesh.h"
#include "scrap/material.h"
#include "scrap/node3d.h"
#include "scrap/render.h"
#include "scrap/particlesystem.h"
#include "scrap/utild3d.h"
#include "scrap/ground.h"
#include "scrap/misc.h"
#include "scrap/model.h"
#include "scrap/staticinit.h"

static const ObjRefVtbl CNode3D_vftable;
#define CNODO3D_VFTABLE (&CNode3D_vftable)

static cGVar GVar_R_Node3D_Count;

#define g_Node3DCount (GVar_R_Node3D_Count.value.i)	/* R_Nodo3D_Count */
extern byte g_RenderOff;
extern int g_RenderCounts[10];
#define g_NodesRendered g_RenderCounts[4]
#define g_NodesUpdated g_RenderCounts[5]
/* the pass of the renderer and its filters of the nodes */

static cGVar GVar_R_NodeSlerp2;

/* the engine variables of the nodes (Gvars) */
#define g_NodeSlerp2 (GVar_R_NodeSlerp2.value.i)	/* R_NodeSlerp2 0/1/2 */
static cGVar GVar_R_ShowLocalBox;

#define g_ShowLocalBox (GVar_R_ShowLocalBox.value.i)
static cGVar GVar_R_ShowLBoxMin;

#define g_ShowLBoxMin (GVar_R_ShowLBoxMin.value.i)	/* the nodes with an index from .. to (0: no limit) */
static cGVar GVar_R_ShowLBoxMax;

#define g_ShowLBoxMax (GVar_R_ShowLBoxMax.value.i)
static cGVar GVar_R_ShowLBoxName;

#define g_ShowLBoxName (GVar_R_ShowLBoxName.value.s)	/* a wildcard of the names, "" for all */
static cGVar GVar_R_SkipAnim;

#define g_SkipAnim (GVar_R_SkipAnim.value.i)
static cGVar GVar_R_SkipAnimVis;

#define g_SkipAnimVis (GVar_R_SkipAnimVis.value.i)
static cGVar GVar_R_SkipNodeFX;

#define g_SkipNodeFX (GVar_R_SkipNodeFX.value.i)
static cGVar GVar_R_SkipNodes;

#define g_SkipNodes (GVar_R_SkipNodes.value.i)
static cGVar GVar_R_SkipTrans2;

#define g_SkipTrans2 (GVar_R_SkipTrans2.value.i)	/* in the passes 2..9 and 12..19 */
static cGVar GVar_R_SkipTrans;

#define g_SkipTrans (GVar_R_SkipTrans.value.i)

/* the check of Nodo3D.cpp that ends the game, with the name of the node */
#define NODE_FATAL(self, expr) \
	Con_FatalErrorf("\nFATAL ERROR: Check failed!\n\nError: %s\n\nExpression: %s\n\nFile: '%s'\nDate: %s\nLine: %d", \
		(self)->name, expr, __FILE__, __TIMESTAMP__, __LINE__)

/* the casts of objeto3d.h: NULL after the failed check */
/* the track set of an animation of a channel */
static void *Anim_Set(void *anim) {
	return ((CAnimGroup *)anim)->Anim->AnimM3D;
}

/* channel 0 of the scene if it has an animation */
static NodeAnimChannel *CNode3D_Channel0(CNode3D *self) {
	NodeAnimCtx *a;

	if (!self->scene || !(a = self->scene->anim) || !a->chan[0].anim || !a->chan[0].anim->anim[0]) {
		return NULL;
	}
	return &a->chan[0];
}

static NodeAnimChannel *CNode3D_ChannelAt(CNode3D *self, uint32_t ch) {
	NodeAnimCtx *a;

	if (!self->scene || !(a = self->scene->anim)) {
		return NULL;
	}
	CHECK(ch < MAX_ANIM_CHANNEL);
	return &a->chan[ch];
}

static void Node_Unref(CNode3D **slot) {
	if (*slot) {
		(*slot)->Refs--;
		*slot = NULL;
	}
}

static void Obj3D_Delete(IObject3D **slot) {
	if (*slot) {
		(*slot)->vftable->Delete(*slot, 1);
		*slot = NULL;
	}
}

static quat_t Quat_Conj(const quat_t *q) {
	return (quat_t){ -q->x, -q->y, -q->z, q->w };
}

CNode3D *CNode3D_ctor(CNode3D *self) {
	self->name = NULL;
	self->Refs = 0;
	self->vftable = CNODO3D_VFTABLE;
	self->flags = 0;
	g_Node3DCount++;
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->next = NULL;
	self->prev = NULL;
	self->parent = NULL;
	self->rot = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	self->obj3D = NULL;
	self->scene = NULL;
	self->index = -1;
	self->slot = -1;
	self->renderNode = -1;
	self->aniLocked = 0;
	self->obj58 = NULL;
	Matrix_LoadIdentity(&self->m);
	Matrix_LoadIdentity(&self->mObj);
	self->qLook = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	self->scale = (vec3_t){ 1.0f, 1.0f, 1.0f };
	self->ini = NULL;
	return self;
}

void CNode3D_dtor(CNode3D *self) {
	self->vftable = CNODO3D_VFTABLE;
	g_Node3DCount--;
	CHECK(!self->prev && !self->next);
	CNode3D_Free(self);
	CHECK(!self->Refs);
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

/* the object of the render node if it has one, else the node's */
static IObject3D *CNode3D_GetObj3D(CNode3D *self, CRenderNode *r) {
	if (r && r->obj3D) {
		CHECK(r->obj3D->class == self->obj3D->class);
		return r->obj3D;
	}
	return self->obj3D;
}

static box_t *CGround_GetBox(void *self, box_t *out);
byte CNode3D_GetBBox(CNode3D *self, matrix_t *m, box_t *bbox, byte recalc) {
	IObject3D *o;
	byte skin = 0;
	box_t b;
	matrix_t t;

	CHECK(bbox);
	o = self->obj3D;
	switch (o ? o->class : 0) {
	case CLASS_OBJ3D_MESHD3D:
		o = Obj3D_Cast(o, CLASS_OBJ3D_MESHD3D);
		skin = ((CMeshD3D *)o)->pSkin != NULL;
		CMeshD3D_GetBox(Obj3D_Cast(self->obj3D, CLASS_OBJ3D_MESHD3D), bbox, recalc);
		break;
	case CLASS_OBJ3D_TRIMESH:
		o = Obj3D_Cast(o, CLASS_OBJ3D_TRIMESH);
		skin = ((CTriMesh *)o)->pSkin != NULL;
		CTriMesh_GetBox(Obj3D_Cast(self->obj3D, CLASS_OBJ3D_TRIMESH), bbox, recalc);
		break;
	case CLASS_OBJ3D_GROUND:
		CHECK(!recalc);
		*bbox = *CGround_GetBox(Obj3D_Cast(self->obj3D, CLASS_OBJ3D_GROUND), &b);
		break;
	case CLASS_OBJ3D_PARTICLESYSTEM:
		CHECK(o->class == CLASS_OBJ3D_PARTICLESYSTEM);
		if (!CParticleSystem_GetBox((CParticleSystem *)o, bbox)) {
			return 0;
		}
		break;
	default:
		return 0;
	}
	if (m) {
		*m = skin ? *Matrix_MulD3D(&t, &self->mObj, &self->m) : self->m;
	}
	if (!(bbox->max.x >= bbox->min.x) || !(bbox->max.y >= bbox->min.y) || !(bbox->max.z >= bbox->min.z)) {
		NODE_FATAL(self, "(bbox->Min <= bbox->Max)");
	}
	if (bbox->min.x == bbox->max.x && bbox->min.y == bbox->max.y && bbox->min.z == bbox->max.z) {
		NODE_FATAL(self, "(bbox->Min != bbox->Max)");
	}
	return 1;
}

/* position and rotation of channel c (pos and rot blended with the second animation); 0 without a track */
static byte CNode3D_AnimChannel(CNode3D *self, NodeAnimChannel *c, vec3_t *pos, quat_t *rot) {
	void *track;
	vec3_t p2;
	quat_t q2, a, b, t, conj;
	float w, iw;

	track = CAnimation_GetNodeTable(Anim_Set(c->anim->anim[0]), self->index);
	if (!track || !CAnimNode_GetPosRot(track, pos, rot, c->frame, c->t)) {
		return 0;
	}
	w = c->blend;
	if (!c->anim->anim[1] || !(w > 0.0f)) {
		return 1;
	}
	track = CAnimation_GetNodeTable(Anim_Set(c->anim->anim[1]), self->index);
	if (!track || !CAnimNode_GetPosRot(track, &p2, &q2, c->frame2, c->t2)) {
		return 1;
	}
	iw = 1.0f - w;
	pos->x = pos->x * iw + p2.x * w;
	pos->y = pos->y * iw + p2.y * w;
	pos->z = pos->z * iw + p2.z * w;
	if (g_NodeSlerp2 == 2) {
		conj = Quat_Conj(&self->rot);
		Quat_Mul(&a, rot, &conj);
		conj = Quat_Conj(&self->rot);
		Quat_Mul(&b, &q2, &conj);
		if (Quat_SlerpFlip(rot, &a, &b, w)) {
			E3D_LogWhite((&g_FileLog), "node: %s", self->name);
		}
		*rot = *Quat_Mul(&t, rot, &self->rot);
		return 1;
	}
	if ((self->flags & NODE_SLERP2) && g_NodeSlerp2 == 1) {
		conj = Quat_Conj(&self->rot);
		Quat_Mul(&a, rot, &conj);
		conj = Quat_Conj(&self->rot);
		Quat_Mul(&b, &q2, &conj);
		Quat_SlerpFlip(rot, &a, &b, w);
		*rot = *Quat_Mul(&t, rot, &self->rot);
		return 1;
	}
	Quat_Slerp(rot, rot, &q2, w);
	return 1;
}

/* R_ShowLocalBox: the line to the parent and the axes; from 2 the box (blue with a skin, other colors for a render
 * node with its own object), 3 the name and class, 4 the name in the middle of the box, from 5 the kinds of the
 * position and rotation tracks (and the camera track) of the animation, from 6 the name too */
static byte CNode3D_DebugDraw(CNode3D *self, CRenderNode *r) {
	static const uint32_t kindColors[5] = { 0xff0000, 0xff, 0xffff00, 0x808080, 0x8000 };
	NodeAnimChannel *c;
	vec3_t a, b;
	matrix_t m;
	box_t box;
	void *track;
	uint32_t class;
	int kPos, kRot, kCam, k;
	float t;
	byte skin;

	if ((g_ShowLBoxMin && self->index < g_ShowLBoxMin) || (g_ShowLBoxMax && self->index > g_ShowLBoxMax)) {
		return 0;
	}
	if (*g_ShowLBoxName && !Wild_MatchLower(g_ShowLBoxName, self->name)) {
		return 0;
	}
	if (self->parent) {
		b = self->parent->m.pos;
		a = self->m.pos;
		Debug_DrawLine(&a, &b, self->parent->parent ? 0xffff00 : 0x808080);
	}
	if (r && r->look && *(int *)r->look) {
		Box_Zero(&box);
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		Debug_DrawAxes(&self->m, &a, 16.0f);
		a.x = 0.0f;
		a.y = 0.0f;
		a.z = 0.0f;
		Debug_DrawPointM(&self->m, &a, 8.0f, 0xff00ff);
		if (CNode3D_GetBBox(self, &m, &box, 0)) {
			Debug_DrawBoxM(&m, &box.min, &box.max, 0xff00);
		}
	}
	a.x = 0.0f;
	a.y = 0.0f;
	a.z = 0.0f;
	Debug_DrawAxes(&self->m, &a, 5.0f);
	if (g_ShowLocalBox == 3) {
		class = self->obj3D ? self->obj3D->class : 0;
		a = self->m.pos;
		Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &a, 0xffffff, "%s [%04X]", self->name, class & 0xffffff);
	}
	if (g_ShowLocalBox > 1) {
		memset(&box, 0, sizeof(box));
		if (CNode3D_GetBBox(self, &m, &box, 0)) {
			skin = self->obj3D && self->obj3D->class == CLASS_OBJ3D_MESHD3D &&
				((CMeshD3D *)self->obj3D)->pSkin != NULL;
			Debug_DrawBoxM(&m, &box.min, &box.max, g_DebugColors[(r && r->obj3D2 ? 8 : 0) + 2 + skin]);
			if (g_ShowLocalBox == 4) {
				class = self->obj3D ? self->obj3D->class : 0;
				a.x = (box.min.x + box.max.x) * 0.5f;
				a.y = (box.max.y + box.min.y) * 0.5f;
				a.z = (box.max.z + box.min.z) * 0.5f;
				Vec3_TransformH(&b, &a, &m);
				Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &b, class == CLASS_OBJ3D_MESHD3D ? 0xff0000 : 0xff00ff, "%s",
					self->name);
			}
		}
	}
	if (g_ShowLocalBox <= 4) {
		return 1;
	}
	kPos = 0;
	kRot = 0;
	kCam = 0;
	if ((c = CNode3D_Channel0(self)) &&
		(track = CAnimation_GetNodeTable(Anim_Set(c->anim->anim[0]), (uint32_t)self->index))) {
		/* the float t goes as the unused int */
		t = c->t;
		memcpy(&k, &t, sizeof(k));
		kPos = CAnimNode_TrackKind(track, c->frame, k, 1);
		kRot = CAnimNode_TrackKind(track, c->frame, k, 2);
		kCam = CAnimNode_TrackKind(track, c->frame, k, 4);
	}
	a = self->m.pos;
	Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &a, kindColors[kPos % 5], "%d", kPos);
	a = self->m.pos;
	Debug_Text3D(g_FntTerm8, 10.0f, 0.0f, &a, kindColors[kRot % 5], "%d", kRot);
	if (self->obj3D && self->obj3D->class == CLASS_OBJ3D_CAMERA) {
		a = self->m.pos;
		Debug_Text3D(g_FntTerm8, 20.0f, 0.0f, &a, kindColors[kCam % 5], "%d", kCam);
	}
	if (kPos < kRot) {
		kRot = kPos;
	}
	if (g_ShowLocalBox > 5) {
		a = self->m.pos;
		Debug_Text3D(g_FntTerm8, 30.0f, 0.0f, &a, kindColors[(uint32_t)kRot % 5], "%s", self->name);
	}
	return 1;
}

static byte CNode3D_AnimAlpha(CNode3D *self, byte *out);
void CNode3D_Render(CNode3D *self, void *arg, CRenderNode *r) {
	IObject3D *o;
	uint32_t flags;
	int pass;
	byte alpha;

	if (g_RenderOff) {
		return;
	}
	if (g_ShowLocalBox && !CNode3D_DebugDraw(self, r)) {
		return;
	}
	if (g_SkipNodes || (self->flags & NODE_HIDE)) {
		return;
	}
	g_NodesRendered++;
	flags = self->flags;
	pass = g_RenderPass;
	if (((flags >> 30) & 1) != (uint32_t)(pass == 100)) {
		return;
	}
	if (pass == 3 || pass == 13) {
		if (!(flags & NODE_PASS3) && !(CNode3D_Class(self) & 0xe0)) {
			return;
		}
	} else {
		if (CNode3D_Class(self) & g_RenderClassMask & 0xffffff) {
			return;
		}
		if ((int)((flags & g_RenderFlagMask) == 0) == (int)g_RenderFlagNeg) {
			return;
		}
	}
	if (CNode3D_AnimAlpha(self, &alpha) && !g_SkipAnimVis) {
		o = self->obj3D;
		if (o && o->class == CLASS_OBJ3D_FLARE) {
			if (alpha <= 2) {
				return;
			}
			o = Obj3D_Cast(o, CLASS_OBJ3D_FLARE);
			((CFlare *)o)->intens = (float)((double)alpha * (1.0 / 255.0));
		} else if (o && o->class == CLASS_OBJ3D_PARTICLESYSTEM) {
			if (alpha <= 2) {
				return;
			}
			((CParticleSystem *)CParticleSystem_Cast(CNode3D_GetObj3D(self, r)))->enabled = alpha >= 0x80;
		} else if (alpha < 0x80) {
			return;
		}
	}
	Render_SetWorldMatrix(&self->m, 1);
	o = CNode3D_GetObj3D(self, r);
	if (o) {
		o->vftable->Render(o, arg, r);
	}
}

/* the look of the render node on the world matrix m */
static quat_t *CRotation_GetQuat(CRotation *self, quat_t *out);
static quat_t *Rot_Combine2(quat_t *out, CRotation *a, CRotation *b);
static void CNode3D_Look(CNode3D *self, CRenderNode *r, NodeLook *l, matrix_t *m) {
	matrix_t mt, mr;
	quat_t q1, q2, q3, conj;
	vec3_t v, e, *pe;
	float nx;

	switch (l->type) {
	case 2:
		v = (vec3_t){ -r->owner->rot.x, -r->owner->rot.y, -r->owner->rot.z };
		CRotation_SetEuler(&l->r1, &v);
		conj = Quat_Conj(CRotation_GetQuat(&l->r1, &q1));
		Quat_ToMatrixD3D(&mt, Quat_Mul(&q3, Rot_Combine2(&q2, &l->r1, &l->r2), &conj));
		v = (vec3_t){ m->m[12], m->m[13], m->m[14] };
		Matrix_AddPos(&mt, &v);
		m->m[12] = 0.0f;
		m->m[13] = 0.0f;
		m->m[14] = 0.0f;
		*m = *Matrix_MulD3D(&mr, m, &mt);
		break;
	case 3:
		Quat_ToMatrixD3D(&mt, Quat_Mul(&q2, &self->qLook, CRotation_GetQuat(&l->r2, &q1)));
		v = (vec3_t){ m->m[12], m->m[13], m->m[14] };
		Matrix_AddPos(&mt, &v);
		*m = mt;
		break;
	case 4:
		conj = Quat_Conj(&self->qLook);
		Quat_Mul(&q3, &self->qLook, CRotation_GetQuat(&l->r1, &q2));
		Quat_ToMatrixD3D(&mt, Quat_Mul(&q1, &q3, &conj));
		*m = *Matrix_MulD3D(&mr, &mt, m);
		break;
	case 5:
		v = l->r1.ang;
		nx = -v.x;
		pe = Matrix_ToEuler(&e, m);
		Quat_FromEuler(&q2, -(pe->x + nx), -(pe->y + v.y), -(pe->z + v.z));
		Quat_ToMatrixD3D(&mt, &q2);
		v = (vec3_t){ m->m[12], m->m[13], m->m[14] };
		Matrix_AddPos(&mt, &v);
		*m = mt;
		break;
	}
}

static byte CNode3D_SampleAnim(CNode3D *self, vec3_t *pos, quat_t *rot);
static quat_t *Rot_Combine(quat_t *out, CRotation *a, const quat_t *q, CRotation *b);
void CNode3D_Update(CNode3D *self, CRenderNode *r) {
	matrix_t *m = &self->m;
	const matrix_t *pm = NULL;
	matrix_t t;
	vec3_t pos;
	quat_t rot, q;
	IObject3D *o;
	int pass;
	byte ok;

	if (self->flags & NODE_NOUPDATE) {
		return;
	}
	pass = g_RenderPass;
	if ((((pass > 1 && pass < 10) || (uint32_t)(pass - 12) <= 7) && g_SkipTrans2) || g_SkipTrans) {
		if (r) {
			self->m = r->m;
		}
		return;
	}
	if (r && self->renderNode >= 0) {
		CRenderNode_UpdateNode(r);
		return;
	}
	g_NodesUpdated++;
	ok = CNode3D_SampleAnim(self, &pos, &rot);
	if (!r) {
		if (ok) {
			Quat_ToMatrixD3D(m, &rot);
			if (self->flags & NODE_POSTFIX) {
				pos = self->pos;
			}
			Matrix_AddPos(m, &pos);
		} else {
			Quat_ToMatrixD3D(m, &self->rot);
			Matrix_AddPos(m, &self->pos);
		}
	} else {
		if (!ok) {
			rot = self->rot;
			pos = self->pos;
		} else if (self->flags & NODE_POSTFIX) {
			pos = self->pos;
		}
		if (r->look && r->look->type == 1) {
			rot = *Rot_Combine(&q, &r->look->r1, &rot, &r->look->r2);
		}
		Quat_ToMatrixD3D(m, &rot);
		Matrix_AddPos(m, &pos);
	}
	if (self->parent) {
		pm = &self->parent->m;
	} else if (self->scene && self->scene->anim) {
		pm = &self->scene->anim->m;
	}
	if (pm) {
		*m = *Matrix_MulD3D(&t, m, pm);
	}
	if (r) {
		if (r->look && r->look->type) {
			CNode3D_Look(self, r, r->look, m);
		}
		if ((self->flags & NODE_FX) && !g_SkipNodeFX) {
			CRenderNode_ApplyFX(r, m);
		}
		r->m = *m;
		r->mObj = *Matrix_MulD3D(&t, &self->mObj, m);
	}
	o = CNode3D_GetObj3D(self, r);
	if (o) {
		o->vftable->Update(o, r);
	}
	if (r) {
		r->frame = g_FrameCount;
	}
}

/* the first node from n on with an object of class class named name (NULL: any) */
static IObject3D *Node_FindObj(CNode3D *n, const char *name, uint32_t class) {
	for (; n; n = n->next) {
		if (n->obj3D && n->obj3D->class == class && (!name || !strcmp(name, n->name))) {
			return Obj3D_Cast(n->obj3D, class);
		}
	}
	return NULL;
}

IObject3D *CNode3D_FindCamera(CNode3D *self, const char *name) {
	return Node_FindObj(self, name, CLASS_OBJ3D_CAMERA);
}

CNode3D *CNode3D_Find(CNode3D *self, const char *name, byte incl) {
	CNode3D *n;

	if (!name) {
		return NULL;
	}
	for (n = incl ? self : self->next; n; n = n->next) {
		if (!strcmp(name, n->name)) {
			return n;
		}
	}
	return NULL;
}

CNode3D *CNode3D_FindPrev(CNode3D *self, const char *name) {
	CNode3D *n;

	if (!name) {
		return NULL;
	}
	for (n = self->prev; n; n = n->prev) {
		if (!strcmp(name, n->name)) {
			return n;
		}
	}
	return NULL;
}

CNode3D *CNode3D_FindWild(CNode3D *self, const char *pattern, byte incl) {
	CNode3D *n;

	if (!pattern) {
		return NULL;
	}
	for (n = incl ? self : self->next; n; n = n->next) {
		if (Wild_Match(pattern, n->name)) {
			return n;
		}
	}
	return NULL;
}

uint32_t CNode3D_Class(CNode3D *self) {
	return self->obj3D ? self->obj3D->class : 0;
}

byte CNode3D_BuildD3D(CNode3D *self, uint32_t flags) {
	void *p, *obj;
	uint32_t class = CNode3D_Class(self);

	if (class == CLASS_OBJ3D_TRIMESH) {
		p = operator_new(sizeof(CMeshD3D));
		obj = p ? CMeshD3D_ctorMesh(p, (CTriMesh *)self->obj3D, flags, self) : NULL;
		CHECK((void *)self->obj3D == obj);
		return 1;
	}
	if (class != CLASS_OBJ3D_GROUND) {
		return 0;
	}
	CGround_NodesToD3D(Obj3D_Cast(self->obj3D, CLASS_OBJ3D_GROUND), flags);
	return 1;
}

/* the link in *slot set to n (counted), the old one released */
static void Node_Link(CNode3D **slot, CNode3D *n) {
	Node_Unref(slot);
	if (n) {
		n->Refs++;
	}
	*slot = n;
}

void CNode3D_Unlink(CNode3D *self) {
	CHECK(!self->scene);
	CHECK(!self->prev || self->prev->next == self);
	CHECK(!self->next || self->next->prev == self);
	if (self->prev) {
		Node_Link(&self->prev->next, self->next);
	}
	if (self->next) {
		Node_Link(&self->next->prev, self->prev);
	}
	Node_Unref(&self->prev);
	Node_Unref(&self->next);
}

void CNode3D_FreeObj3D(CNode3D *self) {
	self->Refs++;
	Obj3D_Delete(&self->obj3D);
	self->Refs--;
}

void CNode3D_SetPrev(CNode3D *self, CNode3D *n) {
	Node_Link(&self->prev, n);
}

void CNode3D_SetObj3D(CNode3D *self, IObject3D *obj) {
	CHECK(!self->obj3D);
	if (obj) {
		CHECK(obj->class != 0);
		self->obj3D = obj;
	}
}

void CNode3D_SetParent(CNode3D *self, CNode3D *n) {
	if (n) {
		CHECK(self->scene && n->scene == self->scene);
		n->Refs++;
	}
	Node_Unref(&self->parent);
	self->parent = n;
}

void CNode3D_SetParentName(CNode3D *self, const char *name) {
	CNode3D_SetParent(self, CNode3D_FindPrev(self, name));
}

void CNode3D_SetNext(CNode3D *self, CNode3D *n) {
	Node_Link(&self->next, n);
}

void CNode3D_Free(CNode3D *self) {
	CIniFile *ini;

	self->Refs++;
	Obj3D_Delete(&self->obj58);
	Obj3D_Delete(&self->obj3D);
	Node_Unref(&self->parent);
	ini = self->ini;
	if (ini) {
		CIniFile_dtor(ini);
		operator_delete_sized(ini, sizeof(*ini));
		self->ini = NULL;
	}
	self->Refs--;
}

/* a, q and b combined (b around the result); the quaternions of a and b are made into one buffer, so with both
 * not zero b's is lost and a's is used twice, as in the original */
static quat_t *Rot_Combine(quat_t *out, CRotation *a, const quat_t *q, CRotation *b) {
	quat_t t, t2, *bq, *aq;

	if (a->flags & ROT_ZERO) {
		if (b->flags & ROT_ZERO) {
			*out = *q;
		} else {
			Quat_Mul(out, q, CRotation_GetQuat(b, &t));
		}
		return out;
	}
	if (b->flags & ROT_ZERO) {
		Quat_Mul(out, CRotation_GetQuat(a, &t), q);
		return out;
	}
	bq = CRotation_GetQuat(b, &t);
	aq = CRotation_GetQuat(a, &t);
	Quat_Mul(out, Quat_Mul(&t2, aq, q), bq);
	return out;
}

static quat_t *Rot_Combine2(quat_t *out, CRotation *a, CRotation *b) {
	quat_t t, *bq, *aq;

	if (a->flags & ROT_ZERO) {
		if (b->flags & ROT_ZERO) {
			*out = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
		} else {
			CRotation_GetQuat(b, out);
		}
		return out;
	}
	if (b->flags & ROT_ZERO) {
		CRotation_GetQuat(a, out);
		return out;
	}
	bq = CRotation_GetQuat(b, &t);
	aq = CRotation_GetQuat(a, &t);
	Quat_Mul(out, aq, bq);
	return out;
}

void *CNode3D_Track(CNode3D *self, uint32_t ch) {
	NodeAnimChannel *c = CNode3D_ChannelAt(self, ch);

	if (!c || !c->anim || !c->anim->anim[0]) {
		return NULL;
	}
	return CAnimation_GetNodeTable(Anim_Set(c->anim->anim[0]), self->index);
}

byte CNode_LightColor(CNode3D *self, vec3_t *color, float *mult) {
	NodeAnimChannel *c = CNode3D_Channel0(self);
	void *track;

	if (!c || !(track = CAnimation_GetNodeTable(Anim_Set(c->anim->anim[0]), self->index))) {
		return 0;
	}
	return CAnimNode_GetColor(track, color, mult, c->frame, c->t) ? 1 : 0;
}

CEVA *CNode3D_TrackData(CNode3D *self, uint32_t ch) {
	byte *track = CNode3D_Track(self, ch);

	return track ? ((CAnimNode *)track)->EVA : NULL;
}

byte CNode3D_AnimFloat(CNode3D *self, float *out) {
	NodeAnimChannel *c = CNode3D_Channel0(self);
	void *track;

	if (!c || !(track = CAnimation_GetNodeTable(Anim_Set(c->anim->anim[0]), self->index))) {
		return 0;
	}
	return CAnimNode_GetFOV(track, out, c->frame, c->t) ? 1 : 0;
}

/* position and rotation from the first channel (3 down to 0) that has the node */
static byte CNode3D_SampleAnim(CNode3D *self, vec3_t *pos, quat_t *rot) {
	NodeAnimChannel *c;

	if (g_SkipAnim) {
		return 0;
	}
	CHECK(self->scene);
	if (self->index < 0 || !self->scene || !self->scene->anim) {
		return 0;
	}
	for (int ch = MAX_ANIM_CHANNEL - 1; ch >= 0; ch--) {
		c = CNode3D_ChannelAt(self, ch);
		if (c && c->anim && c->anim->anim[0] && CNode3D_AnimChannel(self, c, pos, rot)) {
			return 1;
		}
	}
	return 0;
}

/* the animated visibility (0..255) from channel 0 */
static byte CNode3D_AnimAlpha(CNode3D *self, byte *out) {
	NodeAnimChannel *c = CNode3D_Channel0(self);
	void *track;

	if (!c || !(track = CAnimation_GetNodeTable(Anim_Set(c->anim->anim[0]), self->index))) {
		return 0;
	}
	return CAnimNode_GetVisible(track, out, c->frame, c->t) ? 1 : 0;
}

/* the box of a CGround (objeto3d.h) */
static box_t *CGround_GetBox(void *self, box_t *out) {
	*out = ((CGround *)self)->Ext;
	return out;
}

NodeAnimChannel *CNode3D_Channel(CNode3D *self, uint32_t ch) {
	NodeAnimChannel *c = CNode3D_ChannelAt(self, ch);

	if (!c || !c->anim || !c->anim->anim[0]) {
		return NULL;
	}
	return c;
}

static quat_t *CRotation_GetQuat(CRotation *self, quat_t *out) {
	quat_t qx, qy, qz, t, t2;
	float s, c;

	if (self->flags & ROT_DIRTY) {
		self->flags &= ~ROT_DIRTY;
		if (self->flags & ROT_ZERO) {
			self->q = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
		} else if (!self->mode) {
			Quat_FromEuler(&self->q, -self->ang.x, -self->ang.y, -self->ang.z);
		} else {
			Float_SinCos(-self->ang.y * 0.5f, &s, &c);
			qy = (quat_t){ s * 0.0f, s, s * 0.0f, c };
			Float_SinCos(-self->ang.z * 0.5f, &s, &c);
			qz = (quat_t){ s * 0.0f, s * 0.0f, s, c };
			Float_SinCos(-self->ang.x * 0.5f, &s, &c);
			qx = (quat_t){ s, s * 0.0f, s * 0.0f, c };
			self->q = *Quat_Mul(&t2, Quat_Mul(&t, &qx, &qz), &qy);
		}
	}
	*out = self->q;
	return out;
}

/* the vtable of CNode3D */
static const ObjRefVtbl CNode3D_vftable = {
	.Delete = CNode3D_VecDelete,
};

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_ShowLocalBox, &GVar_R_ShowLocalBox, "R_ShowLocalBox", 0, "Show nodes hierarchy and bbox (0/1)\n", 0)
INIT_GVAR_INT(Init_GVar_R_ShowLBoxMin, &GVar_R_ShowLBoxMin, "R_ShowLBoxMin", 0, "Show nodes hierarchy and bbox (min. node)", 0)
INIT_GVAR_INT(Init_GVar_R_ShowLBoxMax, &GVar_R_ShowLBoxMax, "R_ShowLBoxMax", 0, "Show nodes hierarchy and bbox (max. node)", 0)
FINI_GVAR_STRING(Fini_GVar_R_ShowLBoxName, &GVar_R_ShowLBoxName)
INIT_GVAR_STRING(Init_GVar_R_ShowLBoxName, Fini_GVar_R_ShowLBoxName, &GVar_R_ShowLBoxName, "R_ShowLBoxName", 0, "Show nodes hierarchy and bbox (filter)", "")
INIT_GVAR_INT(Init_GVar_R_SkipNodes, &GVar_R_SkipNodes, "R_SkipNodes", 0, "Disable node rendering (0/1)", 0)
INIT_GVAR_INT(Init_GVar_R_SkipTrans, &GVar_R_SkipTrans, "R_SkipTrans", 0, "Disable node transform (0/1)", 0)
INIT_GVAR_INT(Init_GVar_R_SkipTrans2, &GVar_R_SkipTrans2, "R_SkipTrans2", 0, "Disable stencil node transform (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_SkipAnim, &GVar_R_SkipAnim, "R_SkipAnim", 0, "Disable node animation (0/1)", 0)
INIT_GVAR_INT(Init_GVar_R_SkipAnimVis, &GVar_R_SkipAnimVis, "R_SkipAnimVis", 0, "Disable node visibility anim (0/1)", 0)
INIT_GVAR_INT(Init_GVar_R_SkipNodeFX, &GVar_R_SkipNodeFX, "R_SkipNodeFX", 0, "Disable node effects (0/1)", 0)
INIT_GVAR_INT(Init_GVar_R_NodeSlerp2, &GVar_R_NodeSlerp2, "R_NodeSlerp2", 0, "Slerp2 mode for nodes (0/1/2)", 1)
INIT_GVAR_INT(Init_GVar_R_Node3D_Count, &GVar_R_Node3D_Count, "R_Nodo3D_Count", 0, "Node3D counter", 0)
