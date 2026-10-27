/*
 * Engine/NodoR.cpp of the engine3d: CNodoR, the render instance of a node of a model (include/scrap/nodo3d.h): its
 * own copy of the camera or particle object and of the node effect, the look of the node and the update of a render
 * node of a LOD model through the node of the main model. Also here: the render context of the channels
 * (RenderCtx.h) and the class checks of objeto3d.h.
 */
#include "scrap/camara.h"
#include "scrap/e3dutil.h"
#include "scrap/modelo.h"
#include "scrap/nodefx.h"
#include "scrap/nodo3d.h"
#include "scrap/scrapmodel.h"
#include "scrap/sistpart.h"
#include "scrap/varios.h"

#define RNODE_UNIQUE 2
#define NODOR_ROT_NULO 0

#define g_NodoRCount GAME_DATA(int, 0x4ad628)

void *CSistPart_Cast(IObjeto3D *obj) {
	CHECK(obj->clase == CLASE_OBJ3D_SISTPART);
	return obj;
}

static void Rotacion_Init(CRotacion *r) {
	r->flags = ROT_ZERO;
	r->modo = 0;
	r->ang.x = 0.0f;
	r->ang.y = 0.0f;
	r->ang.z = 0.0f;
	r->q.x = 0.0f;
	r->q.y = 0.0f;
	r->q.z = 0.0f;
	r->q.w = 1.0f;
}

/* both rotations zero, the type left */
NodoLook *__fastcall NodoLook_ctor(NodoLook *self) {
	Rotacion_Init(&self->r1);
	Rotacion_Init(&self->r2);
	return self;
}

CNodoR *__fastcall CNodoR_ctor(CNodoR *self) {
	*(int *)self->pad_0 = 0;
	g_NodoRCount++;
	self->flags = 0;
	self->pad_5[0] = 0;
	self->owner = NULL;
	self->look = NULL;
	Matrix_LoadIdentity(&self->m);
	Matrix_LoadIdentity(&self->mObj);
	self->frame = 0;
	self->skin = NULL;
	self->slot = 0;
	self->pad_9a[0] = 0;
	self->pad_9a[1] = 0;
	self->obj3D = NULL;
	self->obj3D2 = NULL;
	self->nodo = NULL;
	return self;
}

void __fastcall CNodoR_dtor(CNodoR *self) {
	g_NodoRCount--;
	if (self->look) {
		operator_delete_sized(self->look, sizeof(NodoLook));
		self->look = NULL;
	}
	if (self->obj3D2) {
		((void *(__thiscall *)(IObjeto3D *, int))self->obj3D2->vftable[0])(self->obj3D2, 1);
		self->obj3D2 = NULL;
	}
	if (self->obj3D) {
		((void *(__thiscall *)(IObjeto3D *, int))self->obj3D->vftable[0])(self->obj3D, 1);
		self->obj3D = NULL;
	}
	CHECK(self->nodo->Refs > 1);
	if (self->nodo) {
		self->nodo->Refs--;
		self->nodo = NULL;
	}
	CHECK(!*(int *)self->pad_0);
}

/* the node effect on the world matrix m; -1 without one, 0 for a LOD model */
int __thiscall CNodoR_ApplyFX(CNodoR *self, matrix_t *m) {
	CHECK(self->flags & RNODE_UNIQUE);
	if (((CModelo *)self->owner)->mainLOD) {
		CHECK(!self->obj3D2);
		return 0;
	}
	if (self->obj3D2) {
		return ((int(__thiscall *)(IObjeto3D *, matrix_t *, CNodo3D *, CNodoR *))self->obj3D2->vftable[NODEFX_VF_APPLY])(
			self->obj3D2, m, self->nodo, self);
	}
	return -1;
}

/* a render node of a LOD model: the node of the main model updated at the place of this node, the matrices taken */
void __thiscall CNodoR_UpdateNodo(CNodoR *self) {
	uint32_t i;
	CTabla *tab;
	CNodoR *main;
	CNodo3D *mainNode;
	vec3_t pos;
	quat_t rot;

	CHECK(self->flags & RNODE_UNIQUE);
	i = (uint32_t)self->nodo->nodoR;
	tab = ((CModelo *)self->owner)->mainLOD->nodosR;
	if (!tab->p || i >= (uint32_t)tab->n) {
		CHECK(0);
	}
	main = (CNodoR *)tab->p + i;
	mainNode = main->nodo;
	CHECK(!mainNode->escena->anim);
	mainNode->escena->anim = self->nodo->escena->anim;
	pos = mainNode->pos;
	rot = mainNode->rot;
	mainNode->pos = self->nodo->pos;
	mainNode->rot = self->nodo->rot;
	mainNode->padre->m = self->nodo->padre->m;
	CNodo3D_Update(mainNode, main);
	mainNode->pos = pos;
	mainNode->rot = rot;
	CHECK(mainNode->escena->anim);
	mainNode->escena->anim = NULL;
	CHECK(!self->skin);
	self->nodo->m = main->m;
	self->m = main->m;
	self->mObj = main->mObj;
	self->frame = g_FrameCount;
}

/* the look type (NODOR_ROT_*) of the node, its look made when missing */
void __thiscall CNodoR_SetLook(CNodoR *self, int tipo) {
	CHECK(self->flags & RNODE_UNIQUE);
	if (!self->look) {
		void *mem;
		NodoLook *look;

		E3D_LogWhite(g_FileLog, "WARN: SetNodeRotation: Deferred creation: %s", self->nodo->name);
		mem = operator_new(sizeof(NodoLook));
		look = mem ? NodoLook_ctor(mem) : NULL;
		self->look = look;
		look->tipo = 0;
	}
	CHECK(tipo != NODOR_ROT_NULO);
	self->look->tipo = tipo;
}

/* the node of the render node (a reference) and the own copies of its camera, particles and node effect */
void __thiscall CNodoR_SetNodo(CNodoR *self, CNodo3D *nodo) {
	uint32_t clase;
	void *mem;

	CHECK(nodo && !self->nodo);
	CHECK(nodo->Refs > 0);
	nodo->Refs++;
	if (self->nodo) {
		self->nodo->Refs--;
		self->nodo = NULL;
	}
	self->nodo = nodo;
	if (((CModelo *)self->owner)->mainLOD) {
		/* a LOD: the main model has the objects */
		CHECK(CNodo3D_Clase(nodo) != CLASE_OBJ3D_SISTPART);
		CHECK(!self->obj3D);
		CHECK(!self->obj3D2);
		CHECK(!self->look);
		return;
	}
	CHECK(!self->obj3D);
	clase = CNodo3D_Clase(nodo);
	if (clase == CLASE_OBJ3D_CAMARA) {
		CCamara *cam = Obj3D_Cast(nodo->obj3D, CLASE_OBJ3D_CAMARA);

		mem = operator_new(sizeof(CCamara));
		self->obj3D = mem ? (IObjeto3D *)CCamara_ctorCopy(mem, cam) : NULL;
	} else if (clase == CLASE_OBJ3D_SISTPART) {
		CSistPart *sp = CSistPart_Cast(nodo->obj3D);

		mem = operator_new(sizeof(CSistPart));
		self->obj3D = mem ? (IObjeto3D *)CSistPart_ctorCopy(mem, sp) : NULL;
	}
	if (nodo->obj58) {
		self->obj3D2 = NodeFX_Clone((INodeFX *)nodo->obj58);
	}
}

/* ---- RenderCtx.h ---- */

void __thiscall RenderCtx_Init(NodoAnimCtx *self, CModelo *m, const vec3_t *scale) {
	(void)scale;
	Matrix_LoadIdentity(&self->m);
	self->modelo = m;
	self->nodosR = m ? m->nodosR : NULL;
	self->chan[0].anim = NULL;
	self->chan[1].anim = NULL;
	self->chan[2].anim = NULL;
	self->chan[3].anim = NULL;
}

/* the frame of an animation group (its control) */
#define AnimGrupo_Frame(g) ((g)->ctrl.frame)

void __thiscall RenderCtx_SetCanal(NodoAnimChannel *self, CAnimSlot *slot) {
	float f1, f2, b;

	self->anim = (NodoAnimPair *)slot;
	if (!slot) {
		self->frame2 = 0;
		self->frame = 0;
		self->t2 = 0.0f;
		self->t = 0.0f;
		self->blend = 0.0f;
		return;
	}
	f1 = slot->anim ? AnimGrupo_Frame(slot->anim) : 0.0f;
	f2 = slot->prev ? AnimGrupo_Frame(slot->prev) : 0.0f;
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
