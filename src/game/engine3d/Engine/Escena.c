/*
 * Engine/escena.cpp of the engine3d: CEscena, see include/scrap/escena.h.
 */
#include <string.h>

#include "scrap/animacion.h"
#include "scrap/camara.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/grafico3d.h"
#include "scrap/luz.h"
#include "scrap/mallad3d.h"
#include "scrap/mallatri.h"
#include "scrap/nodefx.h"
#include "scrap/portal.h"
#include "scrap/material.h"
#include "scrap/modelo.h"
#include "scrap/suelo.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

/* the time since t0 in seconds, for the debug output */
static double Escena_Seconds(uint32_t t0) {
	return (double)(uint32_t)(exe_timeGetTime() - t0) / 1000.0;
}

CEscena *__fastcall CEscena_ctor(CEscena *self) {
	self->name = NULL;
	self->Refs = 0;
	self->vftable = Game_Ptr(CESCENA_VFTABLE);
	self->box = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	self->nodo = NULL;
	self->mainLOD = NULL;
	self->mats.p = NULL;
	self->mats.n = 0;
	self->ambiente.rgba = 0xb0b0b;
	self->ambiente.intens = 1.0f;
	self->niebla.rgba = 0;
	self->niebla.intens = 1.0f;
	self->Nodo3D = NULL;
	self->anim = NULL;
	self->AnimM3D = NULL;
	self->umbralUnirVert = -1.0f;
	self->ini = NULL;
	self->userProp = NULL;
	self->CollideMesh = NULL;
	self->fromAmc = 0;
	return self;
}

static void Escena_DeleteIni(CArchINI **ini) {
	if (*ini) {
		CArchINI_dtor(*ini);
		operator_delete_sized(*ini, 4);
		*ini = NULL;
	}
}

void __fastcall CEscena_dtor(CEscena *self) {
	self->vftable = Game_Ptr(CESCENA_VFTABLE);
	E3D_DebugPrintf("\n> Escena::~Escena(): '%s' (%08Xh)", self->name, self);
	self->anim = NULL;
	E3D_LogDebug2(g_FileLog, " Scene deleted: %s", self->name);
	void *dev = g_pd3dDevice;
	if (dev) {
		CEscena_ReleaseD3D(self, dev);
	}
	for (int i = 0; i < self->mats.n; i++) {
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		CListaMat_Delete(g_ListaMat, ((CMaterial **)self->mats.p)[i]);
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		((CMaterial **)self->mats.p)[i] = NULL;
	}
	CEscena_DeleteNodes(self);
	if (self->AnimM3D) {
		CAnimacion *a = self->AnimM3D;
		CAnimacion_dtor(a);
		operator_delete_sized(a, sizeof(CAnimacion));
		self->AnimM3D = NULL;
	}
	Escena_DeleteIni(&self->ini);
	Escena_DeleteIni(&self->userProp);
	if (self->CollideMesh) {
		OBJ3D_CALL(self->CollideMesh, OBJ3D_VF_DELETE, objDelete_t, 1);
		self->CollideMesh = NULL;
	}
	E3D_DebugPrintf("\n< Escena::~Escena(): '%s' (%08Xh)", self->name, self);
	Tabla_Free(&self->mats);
	if (self->mainLOD) {
		Cad_Set(&self->mainLOD, NULL);
	}
	if (self->nodo) {
		Cad_Set(&self->nodo, NULL);
	}
	CHECK(!self->Refs);
	if (self->name) {
		Cad_Set(&self->name, NULL);
	}
}

void __thiscall CEscena_Shared(CEscena *self) {
	(void)self;
}

CEscena *__thiscall CEscena_Delete(CEscena *self, byte flags) {
	CEscena_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CEscena));
	}
	return self;
}

void __thiscall CEscena_Msg(CEscena *self, int msg, int a, int b, int c, int d) {
	(void)c;
	(void)d;
	switch (msg) {
	case ESCENA_MSG_JOIN:
		if (a || b) {
			CEscena_Join(self, b != 0, NULL);
		}
		break;
	case ESCENA_MSG_CONVERTD3D:
		CEscena_ConvertD3D(self, (0.0f > self->umbralUnirVert ? 0 : 8) | (uint32_t)a | 0x12);
		break;
	case ESCENA_MSG_CALCPESOS:
		CEscena_CalcPesos(self);
		break;
	case ESCENA_MSG_CAMARAS: {
		float fa, fb;
		memcpy(&fa, &a, 4);
		memcpy(&fb, &b, 4);
		for (CNodo3D *n = NULL;;) {
			n = CEscena_FindClase(self, n, CLASE_OBJ3D_CAMARA);
			if (!n) {
				break;
			}
			if (fa != 0.0f) {
				CCamara *cam = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_CAMARA);
				cam->zNear = fa;
			}
			if (fb != 0.0f) {
				CCamara *cam = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_CAMARA);
				cam->zFar = fb;
			}
		}
		break;
	}
	}
}

void __thiscall CEscena_ReleaseD3D(CEscena *self, void *dev) {
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		if (n->obj3D) {
			OBJ3D_CALL(n->obj3D, OBJ3D_VF_RELEASED3D, objDev_t, dev);
		}
	}
}

void __thiscall CEscena_InitD3D(CEscena *self, void *dev) {
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		if (n->obj3D) {
			OBJ3D_CALL(n->obj3D, OBJ3D_VF_INITD3D, objDev_t, dev);
		}
	}
}

/* the object of a node of the class read from a "SCN" block, NULL for none (CHECK on an unknown class) */
static IObjeto3D *Escena_NewObj(CNodo3D *nodo, uint32_t clase, byte *set) {
	void *p;

	*set = 1;
	switch (clase) {
	case CLASE_OBJ3D_SUELO:
		p = operator_new(sizeof(CSuelo));
		return p ? (IObjeto3D *)CSuelo_ctor(p) : NULL;
	case CLASE_OBJ3D_CAMARA:
		p = operator_new(sizeof(CCamara));
		return p ? (IObjeto3D *)CCamara_ctor(p) : NULL;
	case CLASE_OBJ3D_MALLATRI:
		p = operator_new(sizeof(CMallaTri));
		return p ? (IObjeto3D *)CMallaTri_ctor(p) : NULL;
	case CLASE_OBJ3D_MALLAD3D:
		p = operator_new(sizeof(CMallaD3D));
		return p ? (IObjeto3D *)CMallaD3D_ctor(p) : NULL;
	case CLASE_OBJ3D_LUZ:
		p = operator_new(sizeof(CLuz));
		return p ? (IObjeto3D *)CLuz_ctor(p) : NULL;
	case CLASE_OBJ3D_GRAFICO3D:
		p = operator_new(sizeof(CGrafico3D));
		return p ? (IObjeto3D *)CGrafico3D_ctor(p) : NULL;
	case CLASE_OBJ3D_PORTAL:
		p = operator_new(sizeof(CPortal));
		return p ? (IObjeto3D *)CPortal_ctor(p) : NULL;
	case CLASE_OBJ3D_SISTPART:
		CHECK(CArchINI_FindSection(nodo->ini, "Particles", 1));
		break;
	default:
		CHECK(0);
	}
	*set = 0;
	return NULL;
}

void __thiscall CEscena_Read(CEscena *self, CArchIO *f, byte bAnim) {
	char buf[0x100];

	CHECK(!self->Nodo3D);
	CArchIO_BeginBlock(f, "SCN");
	CHECK(CArchIO_ReadInt(f) == 1);
	Cad_Read(&self->mainLOD, f);
	Cad_Read(&self->nodo, f);
	ArchINI_Read(f, &self->ini);
	CArchIO_Read(f, &self->ambiente, sizeof(self->ambiente));
	CArchIO_Read(f, &self->niebla, sizeof(self->niebla));
	CArchIO_Read(f, &self->box, sizeof(box_t));
	CArchIO_Read(f, &self->umbralUnirVert, 4);
	ArchINI_Read(f, &self->userProp);
	CHECK(!self->CollideMesh);
	Tabla_Resize4(&self->mats, CArchIO_ReadInt(f));
	for (int i = 0; i < self->mats.n; i++) {
		void *p = operator_new(sizeof(CMaterial));
		CMaterial *mat = p ? CMaterial_ctor(p) : NULL;
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		((CMaterial **)self->mats.p)[i] = mat;
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		CMaterial_Read(((CMaterial **)self->mats.p)[i], f);
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		CHECK(((CMaterial **)self->mats.p)[i]->params.atrib & MAT_NOSHARE);
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		CListaMat_Add(g_ListaMat, ((CMaterial **)self->mats.p)[i]);
	}
	CHECK(CArchIO_ReadInt(f) == 1);
	int count = CArchIO_ReadInt(f);
	if (count > 0) {
		do {
			void *p = operator_new(sizeof(CNodo3D));
			CNodo3D *nodo = p ? CNodo3D_ctor(p) : NULL;
			CEscena_AddNodo(self, nodo);
			CArchIO_Read(f, &nodo->indice, 4);
			CArchIO_Read(f, &nodo->i48, 4);
			CArchIO_Read(f, &nodo->nodoR, 4);
			CArchIO_Read(f, &nodo->flags, 4);
			CArchIO_Read(f, &nodo->i50, 4);
			Cad_Read(&nodo->name, f);
			CArchIO_ReadString(f, buf, sizeof(buf));
			CNodo3D_SetPadreName(nodo, buf[0] ? buf : NULL);
			CArchIO_Read(f, &nodo->pos, 0xc);
			CArchIO_Read(f, &nodo->rot, 0x10);
			CArchIO_Read(f, &nodo->visible, 4);
			CArchIO_Read(f, &nodo->m, 0x40);
			CArchIO_Read(f, &nodo->mObj, 0x40);
			CArchIO_Read(f, &nodo->qLook, 0x10);
			CArchIO_Read(f, &nodo->escala, 0xc);
			ArchINI_Read(f, &nodo->ini);
			if ((nodo->flags & NODO_FX) && nodo->nodoR == -1) {
				nodo->obj58 = NodeFX_New(0, nodo);
			}
			MallaD3D_FileCtx ctx = { &self->mats, nodo };
			if (CArchIO_ReadInt(f)) {
				byte set;
				IObjeto3D *obj = Escena_NewObj(nodo, (uint32_t)CArchIO_ReadInt(f), &set);
				if (set) {
					CNodo3D_SetObj3D(nodo, obj);
				}
				if (nodo->obj3D) {
					CHECK(OBJ3D_CALL(nodo->obj3D, OBJ3D_VF_READ, objFile_t, f, &ctx));
				}
			}
		} while (--count);
	}
	if (CArchIO_ReadInt(f)) {
		Ruta_Part(buf, self->name, 4);
		void *p = operator_new(sizeof(CAnimacion));
		self->AnimM3D = p ? CAnimacion_ctor(p) : NULL;
		Cad_Set((char **)&self->AnimM3D->name, buf);
		CAnimacion_Read(self->AnimM3D, f);
	}
	CHECK(self->AnimM3D || !bAnim);
	CArchIO_Error(f, "SCN");
}

void __thiscall CEscena_Write(CEscena *self, CArchIO *f, byte bAnim) {
	if (bAnim) {
		CHECK(self->AnimM3D);
		CHECK(!self->ini);
		CHECK(!self->Nodo3D);
		CHECK(!self->mats.n);
	}
	CArchIO_BeginBlockWrite(f, "SCN");
	CArchIO_WriteInt(f, 1);
	Cad_Write(&self->mainLOD, f);
	Cad_Write(&self->nodo, f);
	ArchINI_Write(f, self->ini);
	CArchIO_Write(f, &self->ambiente, sizeof(self->ambiente));
	CArchIO_Write(f, &self->niebla, sizeof(self->niebla));
	CArchIO_Write(f, &self->box, sizeof(box_t));
	CArchIO_Write(f, &self->umbralUnirVert, 4);
	ArchINI_Write(f, self->userProp);
	CHECK(!self->CollideMesh);
	CArchIO_WriteInt(f, self->mats.n);
	for (int i = 0; i < self->mats.n; i++) {
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		CMaterial_Write(((CMaterial **)self->mats.p)[i], f);
	}
	CArchIO_WriteInt(f, 1);
	CArchIO_WriteInt(f, CEscena_NumNodos(self));
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		CArchIO_Write(f, &n->indice, 4);
		CArchIO_Write(f, &n->i48, 4);
		CArchIO_Write(f, &n->nodoR, 4);
		CArchIO_Write(f, &n->flags, 4);
		CArchIO_Write(f, &n->i50, 4);
		Cad_Write(&n->name, f);
		CArchIO_WriteString(f, n->padre ? n->padre->name : NULL);
		CArchIO_Write(f, &n->pos, 0xc);
		CArchIO_Write(f, &n->rot, 0x10);
		CArchIO_Write(f, &n->visible, 4);
		CArchIO_Write(f, &n->m, 0x40);
		CArchIO_Write(f, &n->mObj, 0x40);
		CArchIO_Write(f, &n->qLook, 0x10);
		CArchIO_Write(f, &n->escala, 0xc);
		ArchINI_Write(f, n->ini);
		MallaD3D_FileCtx ctx = { &self->mats, n };
		CArchIO_WriteInt(f, n->obj3D != NULL);
		if (n->obj3D) {
			CArchIO_WriteInt(f, (int)CNodo3D_Clase(n));
			CHECK(OBJ3D_CALL(n->obj3D, OBJ3D_VF_WRITE, objFile_t, f, &ctx));
		}
	}
	CArchIO_WriteInt(f, self->AnimM3D != NULL);
	if (self->AnimM3D) {
		CAnimacion_Write(self->AnimM3D, f);
	}
	CArchIO_EndBlockWrite(f, "SCN");
}

/* the render nodes of the model of the context, up to the first of a NODO_NO_TABNODOR node */
static CNodoR *Escena_NodoR(CEscena *self, uint32_t i) {
	CTabla *t = self->anim->modelo->nodosR;
	TABLA_CHECK(t->p, i, t->n);
	CNodoR *r = &((CNodoR *)t->p)[i];
	return (r->nodo->flags & NODO_NO_TABNODOR) ? NULL : r;
}

void __thiscall CEscena_Render(CEscena *self, NodoAnimCtx *ctx) {
	self->anim = ctx;
	if (!ctx->nodosR) {
		for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
			if (!(CNodo3D_Clase(n) & 0xe0)) {
				CNodo3D_Render(n, g_pd3dDevice, NULL);
			}
		}
		for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
			if (CNodo3D_Clase(n) & 0xe0) {
				CNodo3D_Render(n, g_pd3dDevice, NULL);
			}
		}
	} else {
		int num = ctx->modelo->nodosR->n;
		for (int i = 0; i < num; i++) {
			CNodoR *r = Escena_NodoR(self, (uint32_t)i);
			if (!r) {
				break;
			}
			CNodo3D_Render(r->nodo, g_pd3dDevice, r);
		}
	}
	self->anim = NULL;
}

void __thiscall CEscena_Update(CEscena *self, NodoAnimCtx *ctx) {
	self->anim = ctx;
	if (!ctx->nodosR) {
		for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
			CNodo3D_Update(n, NULL);
		}
	} else {
		int num = ctx->modelo->nodosR->n;
		for (int i = 0; i < num; i++) {
			CNodoR *r = Escena_NodoR(self, (uint32_t)i);
			if (!r) {
				break;
			}
			CNodo3D_Update(r->nodo, r);
		}
	}
	self->anim = NULL;
}

void __fastcall CEscena_UpdateNodes(CEscena *self) {
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		CNodo3D_Update(n, NULL);
	}
}

void __thiscall CEscena_AddNodo(CEscena *self, CNodo3D *nodo) {
	CHECK(nodo && !nodo->sig && !nodo->ant && !nodo->escena);
	CNodo3D *last = self->Nodo3D;
	if (!last) {
		nodo->Refs++;
		self->Nodo3D = nodo;
	} else {
		while (last->sig) {
			last = last->sig;
		}
		CNodo3D_SetAnt(nodo, last);
		CNodo3D_SetSig(last, nodo);
	}
	nodo->escena = self;
}

void __thiscall CEscena_RemoveNodo(CEscena *self, CNodo3D *nodo) {
	CHECK(nodo && nodo->escena == self);
	CHECK((nodo != self->Nodo3D) != (nodo->ant == NULL));
	if (self->Nodo3D == nodo) {
		CNodo3D *sig = nodo->sig;
		if (sig) {
			sig->Refs++;
		}
		self->Nodo3D = sig;
		nodo->Refs--;
	}
	nodo->escena = NULL;
	CNodo3D_Unlink(nodo);
}

void __fastcall CEscena_CompactMats(CEscena *self) {
	int j = 0;

	for (int i = 0; i < self->mats.n; i++) {
		TABLA_CHECK(self->mats.p, i, self->mats.n);
		if (!(((CMaterial **)self->mats.p)[i]->params.atrib & 4)) {
			TABLA_CHECK(self->mats.p, i, self->mats.n);
			CListaMat_Delete(g_ListaMat, ((CMaterial **)self->mats.p)[i]);
			TABLA_CHECK(self->mats.p, i, self->mats.n);
			((CMaterial **)self->mats.p)[i] = NULL;
		} else {
			TABLA_CHECK(self->mats.p, i, self->mats.n);
			CMaterial *mat = ((CMaterial **)self->mats.p)[i];
			TABLA_CHECK(self->mats.p, i, self->mats.n);
			((CMaterial **)self->mats.p)[i] = NULL;
			TABLA_CHECK(self->mats.p, j, self->mats.n);
			((CMaterial **)self->mats.p)[j] = mat;
			j++;
		}
	}
	Tabla_Resize4(&self->mats, j);
}

void __fastcall CEscena_DeleteNodes(CEscena *self) {
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		CNodo3D_Free(n);
	}
	while (self->Nodo3D) {
		CNodo3D *n = self->Nodo3D;
		CEscena_RemoveNodo(self, n);
		((objDelete_t)n->vftable[0])(n, 1);
	}
}

CNodo3D *__thiscall CEscena_NodoObjeto(CEscena *self, int indice) {
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		if (n->indice == indice) {
			return n;
		}
	}
	return NULL;
}

CNodo3D *__thiscall CEscena_FindClase(CEscena *self, CNodo3D *from, uint32_t clase) {
	CNodo3D *n = from ? from->sig : self->Nodo3D;
	if (clase) {
		while (n && CNodo3D_Clase(n) != clase) {
			n = n->sig;
		}
	}
	return n;
}

CNodo3D *__thiscall CEscena_FindNodo(CEscena *self, const char *name) {
	CHECK(self->Nodo3D);
	return CNodo3D_Find(self->Nodo3D, name, 1);
}

void __thiscall CEscena_UpdateBox(CEscena *self, box_t *out, byte first) {
	box_t *box = &self->box;

	box->min = (vec3_t){ 3.4028234663852886e+38f, 3.4028234663852886e+38f, 3.4028234663852886e+38f };
	box->max = (vec3_t){ -3.4028234663852886e+38f, -3.4028234663852886e+38f, -3.4028234663852886e+38f };
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		CNodo3D_Update(n, NULL);
	}
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		matrix_t m;
		box_t bb = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }, t;
		if (!CNodo3D_GetBBox(n, &m, &bb, 1)) {
			continue;
		}
		const box_t *w = Box_Transform(&t, &bb, &m);
		box->min.x = box->min.x < w->min.x ? box->min.x : w->min.x;
		box->min.y = box->min.y < w->min.y ? box->min.y : w->min.y;
		box->min.z = box->min.z < w->min.z ? box->min.z : w->min.z;
		box->max.x = box->max.x > w->max.x ? box->max.x : w->max.x;
		box->max.y = box->max.y > w->max.y ? box->max.y : w->max.y;
		box->max.z = box->max.z > w->max.z ? box->max.z : w->max.z;
	}
	if (box->min.x > box->max.x && box->min.y > box->max.y && box->min.z > box->max.z) {
		if (!self->AnimM3D || self->Nodo3D) {
			E3D_LogInfo(g_FileLog, "ERROR: CEscena: Invalid BBox: %s", self->name);
		}
		if (first) {
			*box = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
		} else {
			CHECK(first);
		}
	}
	Box_Merge(out, box, first);
}

void __fastcall CEscena_CalcPesos(CEscena *self) {
	if (!CEscena_CountClase(self, CLASE_OBJ3D_MALLAD3D)) {
		return;
	}
	int n = 0;
	uint32_t t0 = exe_timeGetTime();
	CEscena_UpdateNodes(self);
	for (CNodo3D *nodo = self->Nodo3D; nodo; nodo = nodo->sig) {
		if (CNodo3D_Clase(nodo) == CLASE_OBJ3D_MALLAD3D) {
			n += CMallaD3D_CalcPesos(Obj3D_Cast(nodo->obj3D, CLASE_OBJ3D_MALLAD3D), nodo) != 0;
		}
	}
	if (n) {
		E3D_DebugPrintf("\n CEscena::calcular_pesos(): %s (%08Xh) [%d nodos]", Ruta_FileName(self->name), self, n);
		E3D_DebugPrintf(" [%.2f seg.]", Escena_Seconds(t0));
	}
}

int __thiscall CEscena_CountClase(CEscena *self, uint32_t clase) {
	int n = 0;
	for (CNodo3D *nodo = self->Nodo3D; nodo; nodo = nodo->sig) {
		if (CNodo3D_Clase(nodo) == clase) {
			n++;
		}
	}
	return n;
}

int __fastcall CEscena_NumNodos(CEscena *self) {
	int n = 0;
	for (CNodo3D *nodo = self->Nodo3D; nodo; nodo = nodo->sig) {
		n++;
	}
	return n;
}

void __thiscall CEscena_ConvertD3D(CEscena *self, uint32_t flags) {
	int n = 0;

	for (CNodo3D *nodo = self->Nodo3D; nodo; nodo = nodo->sig) {
		n += CNodo3D_BuildD3D(nodo, flags) != 0;
	}
	if (self->ini && CArchINI_GetBool(self->ini, "Model", "StaticLighting", 0)) {
		CEscena_PrecalcIlum(self, 1, 0);
	}
	if (n) {
		E3D_DebugPrintf("\n CEscena::convertir_a_D3D(): %s (%08Xh) (f: %Xh) (n: %d)", Ruta_FileName(self->name), self,
			flags, n);
	}
}

void __fastcall CEscena_ToSuelo(CEscena *self) {
	E3D_DebugPrintf("\n CEscena::convertir_a_suelo(): %s (%08Xh)", Ruta_FileName(self->name), self);
	if (self->fromAmc) {
		CNodo3D *n = self->Nodo3D;
		while (n && CNodo3D_Clase(n) != CLASE_OBJ3D_SUELO) {
			n = n->sig;
		}
		CSuelo_Build(Obj3D_Cast(n->obj3D, CLASE_OBJ3D_SUELO), self);
		return;
	}
	void *p = operator_new(sizeof(CSuelo));
	CSuelo *suelo = p ? CSuelo_ctor(p) : NULL;
	CSuelo_Build(suelo, self);
	p = operator_new(sizeof(CNodo3D));
	CNodo3D *nodo = p ? CNodo3D_ctor(p) : NULL;
	Cad_Set(&nodo->name, "suelo");
	CNodo3D_SetObj3D(nodo, (IObjeto3D *)suelo);
	CEscena_AddNodo(self, nodo);
	CEscena_DeleteEmpty(self, 0, 0, 1);
}

void __thiscall CEscena_DeleteEmpty(CEscena *self, uint32_t clase, byte bRefs, byte flagged) {
	byte deleted;

	CHECK(clase == 0);
	CHECK(!bRefs);
	do {
		deleted = 0;
		CNodo3D *n = self->Nodo3D;
		if (!n) {
			return;
		}
		do {
			CNodo3D *sig = n->sig;
			if ((!flagged || (n->flags & NODO_BORRAR)) && CNodo3D_Clase(n) == clase) {
				int refs = n->sig ? (n->ant != NULL) + 1 : (n->ant != NULL);
				if (n->Refs == refs) {
					if (!(n->flags & NODO_HUESO)) {
						CHECK(n->padre);
						E3D_LogWhite(g_FileLog, "DEL_NODE: Node deleted: %s [parent: %s] [f:%08X]", n->name,
							n->padre->name, n->flags);
						deleted = 1;
						CEscena_RemoveNodo(self, n);
						((objDelete_t)n->vftable[0])(n, 1);
					} else {
						E3D_LogInfo(g_FileLog, "DEL_NODE: Node NOT deleted [Bone]: %s [parent: %s] [f:%08X]",
							n->name, n->padre->name, n->flags);
						Con_FatalErrorf("Hidden bone: %s", n->name);
					}
				}
			}
			n = sig;
		} while (n);
	} while (deleted);
}

void __thiscall CEscena_SetEscala(CEscena *self, const vec3_t *s) {
	if (s->x == 1.0f && s->y == 1.0f && s->z == 1.0f) {
		return;
	}
	const char *name = self->name;
	E3D_LogInfo(g_FileLog, "SCENE SCALE: %s [%s]", Vec3_Str(s, 0, 6), name);
	matrix_t m;
	Matrix_LoadIdentity(&m);
	m.m[0] = s->x;
	m.m[5] = s->y;
	m.m[10] = s->z;
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		n->pos.x = n->pos.x * s->x;
		n->pos.y = s->y * n->pos.y;
		n->pos.z = s->z * n->pos.z;
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI) {
			CMallaTri_Transform(Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI), &m);
		}
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_LUZ) {
			CLuz *l = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);
			float k = (s->y + s->x + s->z) * 0.33333334f;
			l->pos.x = l->pos.x * s->x;
			l->pos.y = s->y * l->pos.y;
			l->pos.z = s->z * l->pos.z;
			Vec3_TransformDir0(&l->dir, &l->dir, &m);
			l->f30 = l->f30 * k;
			l->rango = l->rango * k;
		}
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_GRAFICO3D) {
			CGrafico3D *g = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_GRAFICO3D);
			float k = (s->y + s->x + s->z) * 0.33333334f;
			g->ancho = g->ancho * k;
			g->alto = g->alto * k;
		}
	}
	if (self->AnimM3D) {
		CAnimacion_Scale(self->AnimM3D, s);
	}
}

void __thiscall CEscena_Join(CEscena *self, byte sel, const char *name) {
	uint32_t mask = sel ? NODO_JOIN1 : NODO_JOIN2;

	for (CNodo3D *dst = self->Nodo3D; dst; dst = dst->sig) {
		if (!(dst->flags & mask)) {
			continue;
		}
		if (dst->obj3D) {
			return;
		}
		if (name && !strstr(dst->name, name)) {
			continue;
		}
		CMallaTri *acc = NULL;
		Matrix_LoadIdentity(&dst->m);
		for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
			if (n != dst) {
				CNodo3D_Update(n, NULL);
			}
		}
		for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
			if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
				continue;
			}
			for (CNodo3D *p = n->padre; p; p = p->padre) {
				if (p != dst) {
					continue;
				}
				CMallaTri *m = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
				if (m->d3d) {
					m = (CMallaTri *)m->d3d;
				}
				CMallaTri_Transform(m, &n->m);
				if (m->pTri) {
					if (!acc) {
						void *q = operator_new(sizeof(CMallaTri));
						acc = q ? CMallaTri_ctor(q) : NULL;
					}
					CMallaTri_Add(acc, m, 0, 0);
				}
				CNodo3D_FreeObj3D(n);
				n->flags |= NODO_BORRAR;
				break;
			}
		}
		if (acc) {
			CNodo3D_SetObj3D(dst, (IObjeto3D *)acc);
			dst->flags &= ~NODO_COLLIDE;
		}
	}
	CEscena_DeleteEmpty(self, 0, 0, 1);
}

void __thiscall CEscena_SetAlpha(CEscena *self, float alpha, const char *name) {
	int v = (int)((double)alpha * 255.0);
	byte a = v < 1 ? 0 : v >= 0xff ? 0xff : (byte)v;

	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D) {
			continue;
		}
		if (name && !strcmp(n->name, name)) {
			continue;
		}
		CMallaD3D *m = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLAD3D);
		if (!m) {
			continue;
		}
		do {
			CHECK(m->mat);
			m->mat->params.diffuse[3] = (byte)~a;
			m->mat->params.transp = a != 0;
			m->mat->params.srcBlend = 5;
			m->mat->params.dstBlend = 6;
			CMaterial_Prepare(m->mat, NULL);
			m = m->next;
		} while (m);
	}
}

void __thiscall CEscena_SetNodos54(CEscena *self, int v) {
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		n->i54 = v;
	}
}

void __thiscall CEscena_PrecalcIlum(CEscena *self, byte noLuces, byte visible) {
	E3D_DebugPrintf("\n CEscena::precalc_ilum(): %s (%08Xh)", Ruta_FileName(self->name), self);
	uint32_t t0 = exe_timeGetTime();
	for (CNodo3D *n = self->Nodo3D; n; n = n->sig) {
		if (visible && (n->flags & NODO_OCULTO)) {
			continue;
		}
		switch (CNodo3D_Clase(n)) {
		case CLASE_OBJ3D_MALLATRI:
			CHECK(0);
			break;
		case CLASE_OBJ3D_MALLAD3D:
			CMallaD3D_CalcIlum(Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLAD3D), n, noLuces ? NULL : self);
			break;
		case CLASE_OBJ3D_SUELO:
			CSuelo_CalcIlum(Obj3D_Cast(n->obj3D, CLASE_OBJ3D_SUELO), n, noLuces ? NULL : self);
			break;
		}
	}
	E3D_DebugPrintf("  [%.2f seg.]", Escena_Seconds(t0));
}
