/*
 * The scene graph nodes of the engine3d, original source Engine/Nodo3D.cpp (include/scrap/nodo3d.h), with the inline
 * helpers of RenderCtx.h and objeto3d.h compiled into its range. The debug drawing of R_ShowLocalBox
 * (CNodo3D_DebugDraw, rva 0x23d9d0) is not here: it goes through the render path of the debug lines and texts.
 */
#include <string.h>

#include "scrap/camara.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/luz.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/nodo3d.h"
#include "scrap/sistpart.h"
#include "scrap/suelo.h"
#include "scrap/varios.h"

#define CNODO3D_VFTABLE 0x3d7e78
#define MALLAD3D_SIZE 200
#define MALLAD3D_SKIN 0x94		/* the CMallaD3D member that puts the object into mObj */
#define A6000080_ALPHA 0x28
#define SISTPART_VISIBLE 0x140

#define g_Nodo3DCount GAME_DATA(int, 0x4ad548)	/* R_Nodo3D_Count */
#define g_RenderOff GAME_DATA(byte, 0x4a8ef8)
#define g_NodesRendered GAME_DATA(int, 0x4a8f10)
#define g_NodesUpdated GAME_DATA(int, 0x4a8f14)
/* the pass of the renderer and its filters of the nodes */
#define g_RenderClaseMask GAME_DATA(uint32_t, 0x4ad568)
#define g_RenderFlagMask GAME_DATA(uint32_t, 0x4ad5fc)
#define g_RenderFlagNeg GAME_DATA(byte, 0x4ad4ec)	/* drawn if (flags & mask) == 0 differs from it */

/* the engine variables of the nodes (Gvars) */
#define g_NodeSlerp2 GAME_DATA(int, 0x4ad560)	/* R_NodeSlerp2 0/1/2 */
#define g_ShowLocalBox GAME_DATA(int, 0x4ad530)
#define g_SkipAnim GAME_DATA(int, 0x4ad500)
#define g_SkipAnimVis GAME_DATA(int, 0x4ad5c4)
#define g_SkipNodeFX GAME_DATA(int, 0x4ad57c)
#define g_SkipNodes GAME_DATA(int, 0x4ad594)
#define g_SkipTrans2 GAME_DATA(int, 0x4ad5f4)	/* in the passes 2..9 and 12..19 */
#define g_SkipTrans GAME_DATA(int, 0x4ad5ac)

/* the check of Nodo3D.cpp that ends the game, with the name of the node */
#define NODO_FATAL(self, expr) \
	Con_FatalErrorf("\nFATAL ERROR: Check failed!\n\nError: %s\n\nExpression: %s\n\nFile: '%s'\nDate: %s\nLine: %d", \
		(self)->name, expr, __FILE__, __TIMESTAMP__, __LINE__)

typedef void(__thiscall *objUpdate_t)(IObjeto3D *self, CNodoR *r);
typedef void(__thiscall *objRender_t)(IObjeto3D *self, void *arg, CNodoR *r);

/* the casts of objeto3d.h: NULL after the failed check */
/* the track set of an animation of a channel */
static void *Anim_Set(void *anim) {
	return *(void **)(*(byte **)((byte *)anim + 0x94) + 0x3c);
}

/* channel 0 of the scene if it has an animation */
static NodoAnimChannel *CNodo3D_Channel0(CNodo3D *self) {
	NodoAnimCtx *a;

	if (!self->escena || !(a = self->escena->anim) || !a->chan[0].anim || !a->chan[0].anim->anim[0]) {
		return NULL;
	}
	return &a->chan[0];
}

static NodoAnimChannel *CNodo3D_ChannelAt(CNodo3D *self, uint32_t ch) {
	NodoAnimCtx *a;

	if (!self->escena || !(a = self->escena->anim)) {
		return NULL;
	}
	CHECK(ch < MAX_ANIM_CHANNEL);
	return &a->chan[ch];
}

static void Nodo_Unref(CNodo3D **slot) {
	if (*slot) {
		(*slot)->Refs--;
		*slot = NULL;
	}
}

static void Obj3D_Delete(IObjeto3D **slot) {
	if (*slot) {
		((objDelete_t)(*slot)->vftable[0])(*slot, 1);
		*slot = NULL;
	}
}

static quat_t Quat_Conj(const quat_t *q) {
	return (quat_t){ -q->x, -q->y, -q->z, q->w };
}

CNodo3D *__fastcall CNodo3D_ctor(CNodo3D *self) {
	self->name = NULL;
	self->Refs = 0;
	self->vftable = Game_Ptr(CNODO3D_VFTABLE);
	self->flags = 0;
	g_Nodo3DCount++;
	self->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->sig = NULL;
	self->ant = NULL;
	self->padre = NULL;
	self->rot = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	self->obj3D = NULL;
	self->escena = NULL;
	self->indice = -1;
	self->i48 = -1;
	self->nodoR = -1;
	self->i50 = 0;
	self->obj58 = NULL;
	Matrix_LoadIdentity(&self->m);
	Matrix_LoadIdentity(&self->mObj);
	self->qLook = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	self->escala = (vec3_t){ 1.0f, 1.0f, 1.0f };
	self->ini = NULL;
	return self;
}

void __fastcall CNodo3D_dtor(CNodo3D *self) {
	self->vftable = Game_Ptr(CNODO3D_VFTABLE);
	g_Nodo3DCount--;
	CHECK(!self->ant && !self->sig);
	CNodo3D_Free(self);
	CHECK(!self->Refs);
	if (self->name) {
		Cad_Set(&self->name, NULL);
	}
}

CNodo3D *__thiscall CNodo3D_Delete(CNodo3D *self, byte flags) {
	CNodo3D_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

IObjeto3D *__thiscall CNodo3D_GetObj3D(CNodo3D *self, CNodoR *r) {
	if (r && r->obj3D) {
		CHECK(r->obj3D->clase == self->obj3D->clase);
		return r->obj3D;
	}
	return self->obj3D;
}

byte __thiscall CNodo3D_GetBBox(CNodo3D *self, matrix_t *m, box_t *bbox, byte recalc) {
	IObjeto3D *o;
	byte skin = 0;
	box_t b;
	matrix_t t;

	CHECK(bbox);
	o = self->obj3D;
	switch (o ? o->clase : 0) {
	case CLASE_OBJ3D_MALLAD3D:
		o = Obj3D_Cast(o, CLASE_OBJ3D_MALLAD3D);
		skin = *(int *)((byte *)o + MALLAD3D_SKIN) != 0;
		CMallaD3D_GetBox(Obj3D_Cast(self->obj3D, CLASE_OBJ3D_MALLAD3D), bbox, recalc);
		break;
	case CLASE_OBJ3D_MALLATRI:
		o = Obj3D_Cast(o, CLASE_OBJ3D_MALLATRI);
		skin = ((CMallaTri *)o)->pSkin != NULL;
		CMallaTri_GetBox(Obj3D_Cast(self->obj3D, CLASE_OBJ3D_MALLATRI), bbox, recalc);
		break;
	case CLASE_OBJ3D_SUELO:
		CHECK(!recalc);
		*bbox = *CSuelo_GetBox(Obj3D_Cast(self->obj3D, CLASE_OBJ3D_SUELO), &b);
		break;
	case CLASE_OBJ3D_SISTPART:
		CHECK(o->clase == CLASE_OBJ3D_SISTPART);
		if (!CSistPart_GetBox((CSistPart *)o, bbox)) {
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
		NODO_FATAL(self, "(bbox->Min <= bbox->Max)");
	}
	if (bbox->min.x == bbox->max.x && bbox->min.y == bbox->max.y && bbox->min.z == bbox->max.z) {
		NODO_FATAL(self, "(bbox->Min != bbox->Max)");
	}
	return 1;
}

byte __thiscall CNodo3D_AnimChannel(CNodo3D *self, NodoAnimChannel *c, vec3_t *pos, quat_t *rot) {
	void *track;
	vec3_t p2;
	quat_t q2, a, b, t, conj;
	float w, iw;

	track = CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[0]), self->indice);
	if (!track || !CAnimNodo_GetPosRot(track, pos, rot, c->frame, c->t)) {
		return 0;
	}
	w = c->blend;
	if (!c->anim->anim[1] || !(w > 0.0f)) {
		return 1;
	}
	track = CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[1]), self->indice);
	if (!track || !CAnimNodo_GetPosRot(track, &p2, &q2, c->frame2, c->t2)) {
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
			E3D_LogWhite(g_FileLog, "node: %s", self->name);
		}
		*rot = *Quat_Mul(&t, rot, &self->rot);
		return 1;
	}
	if ((self->flags & NODO_SLERP2) && g_NodeSlerp2 == 1) {
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

void __thiscall CNodo3D_Render(CNodo3D *self, void *arg, CNodoR *r) {
	IObjeto3D *o;
	uint32_t flags;
	int pass;
	byte alpha;

	if (g_RenderOff) {
		return;
	}
	if (g_ShowLocalBox && !CNodo3D_DebugDraw(self, r)) {
		return;
	}
	if (g_SkipNodes || (self->flags & NODO_HIDE)) {
		return;
	}
	g_NodesRendered++;
	flags = self->flags;
	pass = g_RenderPass;
	if (((flags >> 30) & 1) != (uint32_t)(pass == 100)) {
		return;
	}
	if (pass == 3 || pass == 13) {
		if (!(flags & NODO_PASS3) && !(CNodo3D_Clase(self) & 0xe0)) {
			return;
		}
	} else {
		if (CNodo3D_Clase(self) & g_RenderClaseMask & 0xffffff) {
			return;
		}
		if ((int)((flags & g_RenderFlagMask) == 0) == (int)g_RenderFlagNeg) {
			return;
		}
	}
	if (CNodo3D_AnimAlpha(self, &alpha) && !g_SkipAnimVis) {
		o = self->obj3D;
		if (o && o->clase == CLASE_OBJ3D_FLARE) {
			if (alpha <= 2) {
				return;
			}
			o = Obj3D_Cast(o, CLASE_OBJ3D_FLARE);
			*(float *)((byte *)o + A6000080_ALPHA) = (float)((double)alpha * (1.0 / 255.0));
		} else if (o && o->clase == CLASE_OBJ3D_SISTPART) {
			if (alpha <= 2) {
				return;
			}
			*((byte *)CSistPart_Cast(CNodo3D_GetObj3D(self, r)) + SISTPART_VISIBLE) = alpha >= 0x80;
		} else if (alpha < 0x80) {
			return;
		}
	}
	Render_SetWorldMatrix(&self->m, 1);
	o = CNodo3D_GetObj3D(self, r);
	if (o) {
		((objRender_t)o->vftable[2])(o, arg, r);
	}
}

/* the look of the render node on the world matrix m */
static void CNodo3D_Look(CNodo3D *self, CNodoR *r, NodoLook *l, matrix_t *m) {
	matrix_t mt, mr;
	quat_t q1, q2, q3, conj;
	vec3_t v, e, *pe;
	float nx;

	switch (l->tipo) {
	case 2:
		v = (vec3_t){ -((vec3_t *)(r->owner + 0x14))->x, -((vec3_t *)(r->owner + 0x14))->y,
			-((vec3_t *)(r->owner + 0x14))->z };
		CRotacion_SetEuler(&l->r1, &v);
		conj = Quat_Conj(CRotacion_GetQuat(&l->r1, &q1));
		Quat_ToMatrixD3D(&mt, Quat_Mul(&q3, Rot_Combine2(&q2, &l->r1, &l->r2), &conj));
		v = (vec3_t){ m->m[12], m->m[13], m->m[14] };
		Matrix_AddPos(&mt, &v);
		m->m[12] = 0.0f;
		m->m[13] = 0.0f;
		m->m[14] = 0.0f;
		*m = *Matrix_MulD3D(&mr, m, &mt);
		break;
	case 3:
		Quat_ToMatrixD3D(&mt, Quat_Mul(&q2, &self->qLook, CRotacion_GetQuat(&l->r2, &q1)));
		v = (vec3_t){ m->m[12], m->m[13], m->m[14] };
		Matrix_AddPos(&mt, &v);
		*m = mt;
		break;
	case 4:
		conj = Quat_Conj(&self->qLook);
		Quat_Mul(&q3, &self->qLook, CRotacion_GetQuat(&l->r1, &q2));
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

void __thiscall CNodo3D_Update(CNodo3D *self, CNodoR *r) {
	matrix_t *m = &self->m;
	const matrix_t *pm = NULL;
	matrix_t t;
	vec3_t pos;
	quat_t rot, q;
	IObjeto3D *o;
	int pass;
	byte ok;

	if (self->flags & NODO_NOUPDATE) {
		return;
	}
	pass = g_RenderPass;
	if ((((pass > 1 && pass < 10) || (uint32_t)(pass - 12) <= 7) && g_SkipTrans2) || g_SkipTrans) {
		if (r) {
			self->m = r->m;
		}
		return;
	}
	if (r && self->nodoR >= 0) {
		CNodoR_UpdateNodo(r);
		return;
	}
	g_NodesUpdated++;
	ok = CNodo3D_SampleAnim(self, &pos, &rot);
	if (!r) {
		if (ok) {
			Quat_ToMatrixD3D(m, &rot);
			if (self->flags & NODO_POSFIJA) {
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
		} else if (self->flags & NODO_POSFIJA) {
			pos = self->pos;
		}
		if (r->look && r->look->tipo == 1) {
			rot = *Rot_Combine(&q, &r->look->r1, &rot, &r->look->r2);
		}
		Quat_ToMatrixD3D(m, &rot);
		Matrix_AddPos(m, &pos);
	}
	if (self->padre) {
		pm = &self->padre->m;
	} else if (self->escena && self->escena->anim) {
		pm = &self->escena->anim->m;
	}
	if (pm) {
		*m = *Matrix_MulD3D(&t, m, pm);
	}
	if (r) {
		if (r->look && r->look->tipo) {
			CNodo3D_Look(self, r, r->look, m);
		}
		if ((self->flags & NODO_FX) && !g_SkipNodeFX) {
			CNodoR_ApplyFX(r, m);
		}
		r->m = *m;
		r->mObj = *Matrix_MulD3D(&t, &self->mObj, m);
	}
	o = CNodo3D_GetObj3D(self, r);
	if (o) {
		((objUpdate_t)o->vftable[1])(o, r);
	}
	if (r) {
		r->frame = g_FrameCount;
	}
}

/* the first node from n on with an object of class clase named name (NULL: any) */
static IObjeto3D *Nodo_FindObj(CNodo3D *n, const char *name, uint32_t clase) {
	for (; n; n = n->sig) {
		if (n->obj3D && n->obj3D->clase == clase && (!name || !strcmp(name, n->name))) {
			return Obj3D_Cast(n->obj3D, clase);
		}
	}
	return NULL;
}

IObjeto3D *__thiscall CNodo3D_FindCamara(CNodo3D *self, const char *name) {
	return Nodo_FindObj(self, name, CLASE_OBJ3D_CAMARA);
}

IObjeto3D *__thiscall CNodo3D_FindLuz(CNodo3D *self, const char *name) {
	return Nodo_FindObj(self, name, CLASE_OBJ3D_LUZ);
}

CNodo3D *__thiscall CNodo3D_Find(CNodo3D *self, const char *name, byte incl) {
	CNodo3D *n;

	if (!name) {
		return NULL;
	}
	for (n = incl ? self : self->sig; n; n = n->sig) {
		if (!strcmp(name, n->name)) {
			return n;
		}
	}
	return NULL;
}

CNodo3D *__thiscall CNodo3D_FindPrev(CNodo3D *self, const char *name) {
	CNodo3D *n;

	if (!name) {
		return NULL;
	}
	for (n = self->ant; n; n = n->ant) {
		if (!strcmp(name, n->name)) {
			return n;
		}
	}
	return NULL;
}

CNodo3D *__thiscall CNodo3D_FindWild(CNodo3D *self, const char *pattern, byte incl) {
	CNodo3D *n;

	if (!pattern) {
		return NULL;
	}
	for (n = incl ? self : self->sig; n; n = n->sig) {
		if (Wild_Match(pattern, n->name)) {
			return n;
		}
	}
	return NULL;
}

uint32_t __fastcall CNodo3D_Clase(CNodo3D *self) {
	return self->obj3D ? self->obj3D->clase : 0;
}

byte __thiscall CNodo3D_BuildD3D(CNodo3D *self, uint32_t flags) {
	void *p, *obj;
	uint32_t clase = CNodo3D_Clase(self);

	if (clase == CLASE_OBJ3D_MALLATRI) {
		p = operator_new(MALLAD3D_SIZE);
		obj = p ? CMallaD3D_ctorMalla(p, (CMallaTri *)self->obj3D, flags, self) : NULL;
		CHECK((void *)self->obj3D == obj);
		return 1;
	}
	if (clase != CLASE_OBJ3D_SUELO) {
		return 0;
	}
	CSuelo_NodosToD3D(Obj3D_Cast(self->obj3D, CLASE_OBJ3D_SUELO), flags);
	return 1;
}

/* the link in *slot set to n (counted), the old one released */
static void Nodo_Link(CNodo3D **slot, CNodo3D *n) {
	Nodo_Unref(slot);
	if (n) {
		n->Refs++;
	}
	*slot = n;
}

void __fastcall CNodo3D_Unlink(CNodo3D *self) {
	CHECK(!self->escena);
	CHECK(!self->ant || self->ant->sig == self);
	CHECK(!self->sig || self->sig->ant == self);
	if (self->ant) {
		Nodo_Link(&self->ant->sig, self->sig);
	}
	if (self->sig) {
		Nodo_Link(&self->sig->ant, self->ant);
	}
	Nodo_Unref(&self->ant);
	Nodo_Unref(&self->sig);
}

void __fastcall CNodo3D_FreeObj3D(CNodo3D *self) {
	self->Refs++;
	Obj3D_Delete(&self->obj3D);
	self->Refs--;
}

void __thiscall CNodo3D_SetAnt(CNodo3D *self, CNodo3D *n) {
	Nodo_Link(&self->ant, n);
}

void __thiscall CNodo3D_SetObj3D(CNodo3D *self, IObjeto3D *obj) {
	CHECK(!self->obj3D);
	if (obj) {
		CHECK(obj->clase != 0);
		self->obj3D = obj;
	}
}

void __thiscall CNodo3D_SetPadre(CNodo3D *self, CNodo3D *n) {
	if (n) {
		CHECK(self->escena && n->escena == self->escena);
		n->Refs++;
	}
	Nodo_Unref(&self->padre);
	self->padre = n;
}

void __thiscall CNodo3D_SetPadreName(CNodo3D *self, const char *name) {
	CNodo3D_SetPadre(self, CNodo3D_FindPrev(self, name));
}

void __thiscall CNodo3D_SetSig(CNodo3D *self, CNodo3D *n) {
	Nodo_Link(&self->sig, n);
}

void __fastcall CNodo3D_Free(CNodo3D *self) {
	CArchINI *ini;

	self->Refs++;
	Obj3D_Delete(&self->obj58);
	Obj3D_Delete(&self->obj3D);
	Nodo_Unref(&self->padre);
	ini = self->ini;
	if (ini) {
		CArchINI_dtor(ini);
		operator_delete_sized(ini, sizeof(*ini));
		self->ini = NULL;
	}
	self->Refs--;
}

quat_t *Rot_Combine(quat_t *out, CRotacion *a, const quat_t *q, CRotacion *b) {
	quat_t t, t2, *bq, *aq;

	if (a->flags & ROT_ZERO) {
		if (b->flags & ROT_ZERO) {
			*out = *q;
		} else {
			Quat_Mul(out, q, CRotacion_GetQuat(b, &t));
		}
		return out;
	}
	if (b->flags & ROT_ZERO) {
		Quat_Mul(out, CRotacion_GetQuat(a, &t), q);
		return out;
	}
	bq = CRotacion_GetQuat(b, &t);
	aq = CRotacion_GetQuat(a, &t);
	Quat_Mul(out, Quat_Mul(&t2, aq, q), bq);
	return out;
}

quat_t *Rot_Combine2(quat_t *out, CRotacion *a, CRotacion *b) {
	quat_t t, *bq, *aq;

	if (a->flags & ROT_ZERO) {
		if (b->flags & ROT_ZERO) {
			*out = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
		} else {
			CRotacion_GetQuat(b, out);
		}
		return out;
	}
	if (b->flags & ROT_ZERO) {
		CRotacion_GetQuat(a, out);
		return out;
	}
	bq = CRotacion_GetQuat(b, &t);
	aq = CRotacion_GetQuat(a, &t);
	Quat_Mul(out, aq, bq);
	return out;
}

void *__thiscall CNodo3D_Track(CNodo3D *self, uint32_t ch) {
	NodoAnimChannel *c = CNodo3D_ChannelAt(self, ch);

	if (!c || !c->anim || !c->anim->anim[0]) {
		return NULL;
	}
	return CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[0]), self->indice);
}

void *__thiscall CNodo3D_Track2(CNodo3D *self, uint32_t ch) {
	NodoAnimChannel *c = CNodo3D_ChannelAt(self, ch);

	if (!c || !c->anim || !c->anim->anim[1]) {
		return NULL;
	}
	return CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[1]), self->indice);
}

byte __thiscall CNodo_LightColor(CNodo3D *self, vec3_t *color, float *mult) {
	NodoAnimChannel *c = CNodo3D_Channel0(self);
	void *track;

	if (!c || !(track = CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[0]), self->indice))) {
		return 0;
	}
	return CAnimNodo_GetColor(track, color, mult, c->frame, c->t) ? 1 : 0;
}

CEVA *__thiscall CNodo3D_TrackData(CNodo3D *self, uint32_t ch) {
	byte *track = CNodo3D_Track(self, ch);

	return track ? ((CAnimNodo *)track)->EVA : NULL;
}

byte __thiscall CNodo3D_AnimFloat(CNodo3D *self, float *out) {
	NodoAnimChannel *c = CNodo3D_Channel0(self);
	void *track;

	if (!c || !(track = CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[0]), self->indice))) {
		return 0;
	}
	return CAnimNodo_GetFOV(track, out, c->frame, c->t) ? 1 : 0;
}

byte __thiscall CNodo3D_SampleAnim(CNodo3D *self, vec3_t *pos, quat_t *rot) {
	NodoAnimChannel *c;

	if (g_SkipAnim) {
		return 0;
	}
	CHECK(self->escena);
	if (self->indice < 0 || !self->escena || !self->escena->anim) {
		return 0;
	}
	for (int ch = MAX_ANIM_CHANNEL - 1; ch >= 0; ch--) {
		c = CNodo3D_ChannelAt(self, ch);
		if (c && c->anim && c->anim->anim[0] && CNodo3D_AnimChannel(self, c, pos, rot)) {
			return 1;
		}
	}
	return 0;
}

byte __thiscall CNodo3D_AnimAlpha(CNodo3D *self, byte *out) {
	NodoAnimChannel *c = CNodo3D_Channel0(self);
	void *track;

	if (!c || !(track = CAnimacion_GetNodoTabla(Anim_Set(c->anim->anim[0]), self->indice))) {
		return 0;
	}
	return CAnimNodo_GetVisible(track, out, c->frame, c->t) ? 1 : 0;
}

box_t *__thiscall CSuelo_GetBox(void *self, box_t *out) {
	*out = *(box_t *)((byte *)self + 0x30);
	return out;
}

NodoAnimChannel *__thiscall CNodo3D_Channel(CNodo3D *self, uint32_t ch) {
	NodoAnimChannel *c = CNodo3D_ChannelAt(self, ch);

	if (!c || !c->anim || !c->anim->anim[0]) {
		return NULL;
	}
	return c;
}

quat_t *__thiscall CRotacion_GetQuat(CRotacion *self, quat_t *out) {
	quat_t qx, qy, qz, t, t2;
	float s, c;

	if (self->flags & ROT_DIRTY) {
		self->flags &= ~ROT_DIRTY;
		if (self->flags & ROT_ZERO) {
			self->q = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
		} else if (!self->modo) {
			Quat_FromEuler(&self->q, -self->ang.x, -self->ang.y, -self->ang.z);
		} else {
			X87_SinCos(-self->ang.y * 0.5f, &s, &c);
			qy = (quat_t){ s * 0.0f, s, s * 0.0f, c };
			X87_SinCos(-self->ang.z * 0.5f, &s, &c);
			qz = (quat_t){ s * 0.0f, s * 0.0f, s, c };
			X87_SinCos(-self->ang.x * 0.5f, &s, &c);
			qx = (quat_t){ s, s * 0.0f, s * 0.0f, c };
			self->q = *Quat_Mul(&t2, Quat_Mul(&t, &qx, &qz), &qy);
		}
	}
	*out = self->q;
	return out;
}
