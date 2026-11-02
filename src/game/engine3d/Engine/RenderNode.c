/*
 * Engine/NodoR.cpp of the engine3d: CRenderNode, the render instance of a node of a model (include/scrap/node3d.h): its
 * own copy of the camera or particle object and of the node effect, the look of the node and the update of a render
 * node of a LOD model through the node of the main model. Also here: the render context of the channels
 * (RenderCtx.h) and the class checks of objeto3d.h.
 */
#include "scrap/gvar.h"
#include "scrap/camera.h"
#include "scrap/e3dutil.h"
#include "scrap/model.h"
#include "scrap/nodefx.h"
#include "scrap/node3d.h"
#include "scrap/scrapmodel.h"
#include "scrap/particlesystem.h"
#include "scrap/misc.h"

#define RNODE_UNIQUE 2
#define RENDERNODE_ROT_NULL 0

extern cGVar GVar_R_RenderNode_Count;
#define g_RenderNodeCount (GVar_R_RenderNode_Count.value.i)

void *CParticleSystem_Cast(IObject3D *obj) {
	CHECK(obj->class == CLASS_OBJ3D_PARTICLESYSTEM);
	return obj;
}

static void Rotation_Init(CRotation *r) {
	r->flags = ROT_ZERO;
	r->mode = 0;
	r->ang.x = 0.0f;
	r->ang.y = 0.0f;
	r->ang.z = 0.0f;
	r->q.x = 0.0f;
	r->q.y = 0.0f;
	r->q.z = 0.0f;
	r->q.w = 1.0f;
}

/* both rotations zero, the type left */
NodeLook *NodeLook_ctor(NodeLook *self) {
	Rotation_Init(&self->r1);
	Rotation_Init(&self->r2);
	return self;
}

CRenderNode *CRenderNode_ctor(CRenderNode *self) {
	*(int *)self->pad_0 = 0;
	g_RenderNodeCount++;
	self->flags = 0;
	self->pad_5[0] = 0;
	self->owner = NULL;
	self->look = NULL;
	Matrix_LoadIdentity(&self->m);
	Matrix_LoadIdentity(&self->mObj);
	self->frame = 0;
	self->skin = NULL;
	self->slot = 0;
	self->skinFrame = 0;
	self->obj3D = NULL;
	self->obj3D2 = NULL;
	self->node = NULL;
	return self;
}

void CRenderNode_dtor(CRenderNode *self) {
	g_RenderNodeCount--;
	if (self->look) {
		operator_delete_sized(self->look, sizeof(NodeLook));
		self->look = NULL;
	}
	if (self->obj3D2) {
		((void *(*)(IObject3D *, int))self->obj3D2->vftable[0])(self->obj3D2, 1);
		self->obj3D2 = NULL;
	}
	if (self->obj3D) {
		((void *(*)(IObject3D *, int))self->obj3D->vftable[0])(self->obj3D, 1);
		self->obj3D = NULL;
	}
	CHECK(self->node->Refs > 1);
	if (self->node) {
		self->node->Refs--;
		self->node = NULL;
	}
	CHECK(!*(int *)self->pad_0);
}

/* the node effect on the world matrix m; -1 without one, 0 for a LOD model */
int CRenderNode_ApplyFX(CRenderNode *self, matrix_t *m) {
	CHECK(self->flags & RNODE_UNIQUE);
	if (((CModel *)self->owner)->mainLOD) {
		CHECK(!self->obj3D2);
		return 0;
	}
	if (self->obj3D2) {
		return ((int(*)(IObject3D *, matrix_t *, CNode3D *, CRenderNode *))self->obj3D2->vftable[NODEFX_VF_APPLY])(
			self->obj3D2, m, self->node, self);
	}
	return -1;
}

/* a render node of a LOD model: the node of the main model updated at the place of this node, the matrices taken */
void CRenderNode_UpdateNode(CRenderNode *self) {
	uint32_t i;
	CTable *tab;
	CRenderNode *main;
	CNode3D *mainNode;
	vec3_t pos;
	quat_t rot;

	CHECK(self->flags & RNODE_UNIQUE);
	i = (uint32_t)self->node->renderNode;
	tab = ((CModel *)self->owner)->mainLOD->renderNodes;
	if (!tab->p || i >= (uint32_t)tab->n) {
		CHECK(0);
	}
	main = (CRenderNode *)tab->p + i;
	mainNode = main->node;
	CHECK(!mainNode->scene->anim);
	mainNode->scene->anim = self->node->scene->anim;
	pos = mainNode->pos;
	rot = mainNode->rot;
	mainNode->pos = self->node->pos;
	mainNode->rot = self->node->rot;
	mainNode->parent->m = self->node->parent->m;
	CNode3D_Update(mainNode, main);
	mainNode->pos = pos;
	mainNode->rot = rot;
	CHECK(mainNode->scene->anim);
	mainNode->scene->anim = NULL;
	CHECK(!self->skin);
	self->node->m = main->m;
	self->m = main->m;
	self->mObj = main->mObj;
	self->frame = g_FrameCount;
}

/* the look type (NODOR_ROT_*) of the node, its look made when missing */
void CRenderNode_SetLook(CRenderNode *self, int type) {
	CHECK(self->flags & RNODE_UNIQUE);
	if (!self->look) {
		void *mem;
		NodeLook *look;

		E3D_LogWhite(g_FileLog, "WARN: SetNodeRotation: Deferred creation: %s", self->node->name);
		mem = operator_new(sizeof(NodeLook));
		look = mem ? NodeLook_ctor(mem) : NULL;
		self->look = look;
		look->type = 0;
	}
	CHECK(type != RENDERNODE_ROT_NULL);
	self->look->type = type;
}

/* the node of the render node (a reference) and the own copies of its camera, particles and node effect */
void CRenderNode_SetNode(CRenderNode *self, CNode3D *node) {
	uint32_t class;
	void *mem;

	CHECK(node && !self->node);
	CHECK(node->Refs > 0);
	node->Refs++;
	if (self->node) {
		self->node->Refs--;
		self->node = NULL;
	}
	self->node = node;
	if (((CModel *)self->owner)->mainLOD) {
		/* a LOD: the main model has the objects */
		CHECK(CNode3D_Class(node) != CLASS_OBJ3D_PARTICLESYSTEM);
		CHECK(!self->obj3D);
		CHECK(!self->obj3D2);
		CHECK(!self->look);
		return;
	}
	CHECK(!self->obj3D);
	class = CNode3D_Class(node);
	if (class == CLASS_OBJ3D_CAMERA) {
		CCamera *cam = Obj3D_Cast(node->obj3D, CLASS_OBJ3D_CAMERA);

		mem = operator_new(sizeof(CCamera));
		self->obj3D = mem ? (IObject3D *)CCamera_ctorCopy(mem, cam) : NULL;
	} else if (class == CLASS_OBJ3D_PARTICLESYSTEM) {
		CParticleSystem *sp = CParticleSystem_Cast(node->obj3D);

		mem = operator_new(sizeof(CParticleSystem));
		self->obj3D = mem ? (IObject3D *)CParticleSystem_ctorCopy(mem, sp) : NULL;
	}
	if (node->obj58) {
		self->obj3D2 = NodeFX_Clone((INodeFX *)node->obj58);
	}
}

/* ---- RenderCtx.h ---- */

void RenderCtx_Init(NodeAnimCtx *self, CModel *m, const vec3_t *scale) {
	(void)scale;
	Matrix_LoadIdentity(&self->m);
	self->model = m;
	self->renderNodes = m ? m->renderNodes : NULL;
	self->chan[0].anim = NULL;
	self->chan[1].anim = NULL;
	self->chan[2].anim = NULL;
	self->chan[3].anim = NULL;
}

/* the frame of an animation group (its control) */
#define AnimGroup_Frame(g) ((g)->ctrl.frame)

void RenderCtx_SetChannel(NodeAnimChannel *self, CAnimSlot *slot) {
	float f1, f2, b;

	self->anim = (NodeAnimPair *)slot;
	if (!slot) {
		self->frame2 = 0;
		self->frame = 0;
		self->t2 = 0.0f;
		self->t = 0.0f;
		self->blend = 0.0f;
		return;
	}
	f1 = slot->anim ? AnimGroup_Frame(slot->anim) : 0.0f;
	f2 = slot->prev ? AnimGroup_Frame(slot->prev) : 0.0f;
	self->frame = (int)f1;
	self->frame2 = (int)f2;
	self->t = f1 - (float)self->frame;
	self->t2 = f2 - (float)self->frame2;
	b = 0.0f;
	if (!(0.0f >= slot->blend)) {
		b = Float_MinSS(1.0f, slot->blend);
	}
	/* smoothstep */
	self->blend = (3.0f - (b + b)) * (b * b);
}
