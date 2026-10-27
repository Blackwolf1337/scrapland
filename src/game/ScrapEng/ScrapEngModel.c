/*
 * ScrapEngModel.cpp of the game: CGameModel, see include/scrap/scrapmodel.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/listavert.h"
#include "scrap/luz.h"
#include "scrap/mallad3d.h"
#include "scrap/mallatri.h"
#include "scrap/mundo.h"
#include "scrap/render.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/varios.h"

#define GM_CHECK(self) \
	do { \
		if (!(self)->Model) { \
			CheckFailed("Model", __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

void __fastcall CCollideMesh_dtor(CCollideMesh *self) {
	if (self->pTri) {
		operator_delete_array(self->pTri);
		self->pTri = NULL;
		self->nTri = 0;
	}
	if (self->pVert) {
		operator_delete_array(self->pVert);
		self->pVert = NULL;
		self->nVert = 0;
	}
	if (self->Refs) {
		CheckFailed("!Refs", "objref.h", __TIMESTAMP__, __LINE__);
	}
}

void __thiscall CAnimMat_SetFrame(CAnimMat *self, int frame) {
	int *p = (int *)self;
	p[4] = frame;
	((float *)p)[5] = (float)frame;
}

CGameModel *__thiscall CGameModel_ctor(CGameModel *self, const char *name) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	self->Model = NULL;
	self->CollideMesh = NULL;
	self->animBaseFPS = 0.0f;
	self->frame = 0;
	CGameModel_Load(self, name, NULL, &one);
	return self;
}

void __fastcall CGameModel_Reset(CGameModel *self) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	CGameModel_Load(self, NULL, NULL, &one);
}

void __thiscall CGameModel_LoadName(CGameModel *self, const char *name) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	CGameModel_Load(self, name, NULL, &one);
}

void __thiscall CGameModel_LoadScaled(CGameModel *self, const char *name, const vec3_t *scale) {
	CGameModel_Load(self, name, NULL, scale);
}

/* the LOD distance of the model times the coefficient of its LOD group */
static void CGameModel_LodGroup(CGameModel *self, CArchINI *ini) {
	static const struct {
		const char *name;
		uint32_t rva;
	} grupos[7] = { { "Ships", 0x4a80f4 }, { "Pilots", 0x4a80dc }, { "Engines", 0x4a8064 }, { "Traffic", 0x4a804c },
		{ "Characters", 0x4a80ac }, { "OutElement", 0x4a807c }, { "MenuElement", 0x4a8094 } };
	char grupo[260];

	if (!ini || !CArchINI_GetText(ini, "Model", "LOD_Group", grupo, sizeof(grupo), NULL)) {
		if (self->Model->lods.n) {
			Con_Warnf("Model without LOD group: %s\n", self->Model->name);
		}
		return;
	}
	E3D_LogWhite(g_FileLog, "LOD_%s: %s", grupo, self->Model->name);
	for (int i = 0; i < 7; i++) {
		if (!crt_stricmp(grupo, grupos[i].name)) {
			self->Model->lodDist *= *(float *)Game_Ptr(grupos[i].rva);
			return;
		}
	}
	E3D_LogBlue(g_FileLog, "LOD group not found: %s [Model: %s]", grupo, self->Model->name);
}

void __thiscall CGameModel_Load(CGameModel *self, const char *name, const char *name2, const vec3_t *scale) {
	char alt[262], fmt[260], path[260];
	CModelo *lods[10];

	CGameModel_Release(self, 0);
	if (!name) {
		return;
	}
	vec3_t esc = *scale;
	const char *usar = name;
	if (g_pApp->flags & 4) {
		size_t len = strlen(name);
		Str_Copy(alt, name, 0x104);
		char *ext = alt + len - 3;
		if (ext[0] == 'S' || ext[0] == 's') {
			ext[0] = 'm';
			ext[1] = '3';
			ext[2] = 'd';
		} else {
			ext[0] = 's';
			ext[1] = 'm';
			ext[2] = '3';
		}
		/* the exe keeps name when only the other extension exists */
		if (!File_Exists(name)) {
			usar = File_Exists(alt) ? name : "Models/Vehicles/Characters/P_CrazyGambler/P_CrazyGambler.m3d";
		}
	}
	const char *base = usar;
	self->Model = Crear_Modelo3(usar, name2, &esc, 0x37);
	CModelo_BuildNodosR(self->Model, 0);
	if (CAnimGrupos_Count(&self->Model->anims)) {
		CEscena *e = self->Model->escena;
		CHECK(e->AnimM3D);
		CModelo_AddAnimEvents(self->Model, *(const char **)e->AnimM3D, -1.0f, -1.0f);
		CModelo_SetAnim(self->Model, 0, 0);
	}
	int n = 0;
	for (int i = 1; i < 11; i++) {
		Bprintf(fmt, sizeof(fmt), "&U&D&N.LOD%d.SM3", i);
		Ruta_Format(path, base, fmt);
		byte sm3 = File_Exists(path);
		Bprintf(fmt, sizeof(fmt), "&U&D&N.LOD%d&E", i);
		Ruta_Format(path, base, fmt);
		if (!File_Exists(path) && !sm3) {
			break;
		}
		lods[n++] = Crear_ModeloLOD(path, name2, &esc, self->Model, 0x37);
	}
	if (n) {
		CModelo_SetNumLods(self->Model, n);
		for (int i = 0; i < self->Model->lods.n; i++) {
			CModelo_SetLod(self->Model, (uint32_t)i, lods[i]);
			CModelo_BuildNodosR(lods[i], 0);
			CModelo_Disable(lods[i]);
		}
	}
	CArchINI *ini = self->Model->escena->ini;
	CGameModel_LodGroup(self, ini);
	if (ini) {
		self->Model->lodDist = CArchINI_GetFloat(ini, "Model", "LOD_Mult", 1.0f) * self->Model->lodDist;
		self->animBaseFPS = CArchINI_GetFloat(ini, "Model", "AnimBaseFPS", 0.0f);
	}
}

void __thiscall CGameModel_Release(CGameModel *self, byte free) {
	CGameModel_FreeCollision(self);
	if (free) {
		Mundo_Get();
		Con_Verbosef("Release model: %s\n", self->Model->name);
		for (int i = 0; i < MAX_ANIMGRUPOS; i++) {
			CAnimGrupo *g = CModelo_GetAnim(self->Model, i);
			if (g && !Escena_Release(g->Anim, 1)) {
				CheckFailed("!link || EscenaElim(link->obtEscena(), true)", __FILE__, __TIMESTAMP__, __LINE__);
			}
		}
		for (uint32_t i = 0; (int)i < self->Model->lods.n; i++) {
			TABLA_CHECK(self->Model->lods.p, i, self->Model->lods.n);
			Modelo_Destroy(&((CLod *)self->Model->lods.p)[i].model, 1);
		}
		Modelo_Destroy(&self->Model, 1);
	}
	if (self->Model) {
		Mundo_Get();
		Modelo_Destroy(&self->Model, 0);
	}
}

byte __fastcall CGameModel_IsLoaded(const CGameModel *self) {
	return self->Model != NULL;
}

const char *__fastcall CGameModel_Name(const CGameModel *self) {
	GM_CHECK(self);
	return self->Model->name;
}

const char *__fastcall CGameModel_SceneName(const CGameModel *self) {
	GM_CHECK(self);
	return self->Model->escena->name;
}

void __fastcall CGameModel_Enable(CGameModel *self) {
	GM_CHECK(self);
	CModelo_Enable(self->Model);
}

void __fastcall CGameModel_Disable(CGameModel *self) {
	GM_CHECK(self);
	CModelo_Disable(self->Model);
}

byte __fastcall CGameModel_IsEnabled(const CGameModel *self) {
	GM_CHECK(self);
	return self->Model->enable;
}

byte __fastcall CGameModel_IsVisible(const CGameModel *self) {
	GM_CHECK(self);
	if (g_SplitScreen) {
		return 1;
	}
	return self->Model->visible & 1;
}

int __fastcall CGameModel_GetSector(CGameModel *self) {
	GM_CHECK(self);
	return CModelo_GetSector(self->Model);
}

float __fastcall CGameModel_Radius(const CGameModel *self) {
	if (!self->Model || !self->Model->escena) {
		CheckFailed("Model && Model->obtEscena()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	const box_t *b = &self->Model->escena->box;
	float dx = b->max.x - b->min.x, dy = b->max.y - b->min.y, dz = b->max.z - b->min.z;
	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
}

vec3_t *__thiscall CGameModel_GetPos(const CGameModel *self, vec3_t *out) {
	GM_CHECK(self);
	*out = self->Model->pos;
	return out;
}

void __thiscall CGameModel_SetPos(CGameModel *self, const vec3_t *v) {
	GM_CHECK(self);
	self->Model->pos = *v;
}

vec3_t *__thiscall CGameModel_GetRot(const CGameModel *self, vec3_t *out) {
	GM_CHECK(self);
	*out = self->Model->rot;
	return out;
}

void __thiscall CGameModel_SetRot(CGameModel *self, const vec3_t *v) {
	GM_CHECK(self);
	self->Model->rot = *v;
}

void __thiscall CGameModel_SetRot2(CGameModel *self, const vec3_t *v) {
	GM_CHECK(self);
	self->Model->rot2 = *v;
}

void __thiscall CGameModel_MoveForward(CGameModel *self, float d) {
	vec3_t a, b;

	GM_CHECK(self);
	b = (vec3_t){ 0.0f, 0.0f, d };
	Vec3_Rotate(&a, &b, &self->Model->rot2);
	const vec3_t *r = Vec3_Rotate(&b, &a, &self->Model->rot);
	CModelo *m = self->Model;
	m->pos.x = m->pos.x + r->x;
	m->pos.y = r->y + m->pos.y;
	m->pos.z = r->z + m->pos.z;
}

void __thiscall CGameModel_SetPivot(CGameModel *self, const vec3_t *v, byte rel, byte recalcBB) {
	GM_CHECK(self);
	if (recalcBB) {
		CheckFailed("!RecalcBB", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CModelo_SetPivot(self->Model, v, rel, recalcBB);
}

void __thiscall CGameModel_SetPivotNodo(CGameModel *self, const char *name) {
	GM_CHECK(self);
	CModelo_SetPivotNodo(self->Model, name);
}

void __thiscall CGameModel_SetSortZ(CGameModel *self, float bias, float mult) {
	GM_CHECK(self);
	self->Model->sortZBias = bias;
	self->Model->sortZMult = mult;
}

uint32_t __thiscall CGameModel_GetEffect(const CGameModel *self, uint32_t *out) {
	GM_CHECK(self);
	uint32_t fx = (uint32_t)self->Model->f10c;
	if (fx > MODEL_EFFECT_LAST) {
		CheckFailed("fx <= MODEL_EFFECT_LAST", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (out) {
		*out = fx;
	}
	return fx;
}

void __thiscall CGameModel_SetEffect(CGameModel *self, uint32_t fx) {
	GM_CHECK(self);
	if (fx > MODEL_EFFECT_LAST) {
		CheckFailed("(dword)RenderEffect <= MODEL_EFFECT_LAST", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if ((uint32_t)self->Model->f10c != fx) {
		self->Model->f10c = (int)fx;
	}
}

void __thiscall CGameModel_GetEffectParam(const CGameModel *self, int i, uint32_t *out) {
	if (!self->Model || !out) {
		CheckFailed("Model && Value", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (i) {
		CheckFailed("(dword)Param < MAX_RENDERFX_PARAM", __FILE__, __TIMESTAMP__, __LINE__);
	}
	*out = (uint32_t)(&self->Model->f110)[i];
}

void __thiscall CGameModel_SetEffectParam(CGameModel *self, int i, uint32_t v) {
	GM_CHECK(self);
	if (i) {
		CheckFailed("(dword)Param < MAX_RENDERFX_PARAM", __FILE__, __TIMESTAMP__, __LINE__);
	}
	(&self->Model->f110)[i] = (int)v;
}

/* the render state an immediate draw clears */

void __thiscall CGameModel_RenderNow(CGameModel *self, byte flags) {
	GM_CHECK(self);
	if (!Render_GetViewportW()) {
		CheckFailed("obt_ancho_render()", __FILE__, __TIMESTAMP__, __LINE__);
	}
	int pass = g_RenderPass, fog = g_FogActive, r5fc = g_RenderFlagMask, r568 = g_RenderClaseMask;
	byte r4ec = g_RenderFlagNeg;
	g_RenderFlagMask = 0;
	g_RenderFlagNeg = 0;
	g_RenderClaseMask = 0;
	g_RenderPass = 0;
	g_FogActive = 0;
	CModelo *m = self->Model;
	byte always = m->alwaysVisible;
	m->alwaysVisible = 1;
	byte en = m->enable;
	if (!en) {
		CModelo_Enable(m);
	}
	byte dyn = self->Model->dynLight;
	if ((flags & 3) == 2) {
		self->Model->dynLight = 0;
	}
	if (flags & 2) {
		Luz_SetDefault(1);
	}
	CModelo_Render(self->Model);
	self->Model->dynLight = dyn;
	if (flags & 2) {
		Luz_SetDefault(0);
	}
	if (!en) {
		CModelo_Disable(self->Model);
	}
	self->Model->alwaysVisible = always;
	g_FogActive = fog;
	g_RenderFlagMask = r5fc;
	g_RenderFlagNeg = r4ec;
	g_RenderClaseMask = r568;
	g_RenderPass = pass;
}

int __thiscall CGameModel_AddAnim(CGameModel *self, const char *name) {
	GM_CHECK(self);
	Crear_Animacion(self->Model, name, 3);
	int i = CAnimGrupos_Count(&self->Model->anims) - 1;
	if (!CModelo_GetAnimName(self->Model, i)) {
		CheckFailed("Model->obtNombreAnim(index)", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CModelo_AddAnimEvents(self->Model, CModelo_GetAnimName(self->Model, i), -1.0f, -1.0f);
	return i;
}

int __thiscall CGameModel_FindAnim(CGameModel *self, const char *name) {
	GM_CHECK(self);
	return CModelo_FindAnim(self->Model, name);
}

const char *__thiscall CGameModel_AnimName(CGameModel *self, int i) {
	GM_CHECK(self);
	return CModelo_GetAnimName(self->Model, i);
}

/* animation i, CHECK("link") without one */
static CAnimGrupo *CGameModel_Link(CGameModel *self, int i) {
	CAnimGrupo *g = CModelo_GetAnim(self->Model, i);
	if (!g) {
		CheckFailed("link", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return g;
}

byte __thiscall CGameModel_AnimInfo(CGameModel *self, int i, float *dur, float *fps) {
	GM_CHECK(self);
	CAnimGrupo *g = CModelo_GetAnim(self->Model, i);
	if (!g) {
		CheckFailed("link", __FILE__, __TIMESTAMP__, __LINE__);
		return 0;
	}
	if (fps) {
		*fps = g->ctrl.vel;
	}
	if (dur) {
		*dur = (float)(g->ctrl.frames - 1) / g->ctrl.vel;
	}
	return 1;
}

byte __thiscall CGameModel_SetAnimVel(CGameModel *self, int i, float vel) {
	GM_CHECK(self);
	CGameModel_Link(self, i)->ctrl.vel = vel;
	return 1;
}

byte __thiscall CGameModel_SetAnimFactor(CGameModel *self, int i, float factor) {
	GM_CHECK(self);
	CGameModel_Link(self, i)->ctrl.factor = factor;
	return 1;
}

byte __thiscall CGameModel_SetAnimSpeed(CGameModel *self, int i, float speed) {
	GM_CHECK(self);
	CAnimGrupo *g = CGameModel_Link(self, i);
	if (0.01f > speed) {
		speed = 0.0f;
	}
	g->ctrl.speed = speed;
	return 1;
}

byte __thiscall CGameModel_SetAnimLoop(CGameModel *self, int i, byte loop) {
	GM_CHECK(self);
	CGameModel_Link(self, i)->ctrl.bucle = !loop;
	return 1;
}

void __thiscall CGameModel_AnimCmd(CGameModel *self, const char *cmd) {
	char tag[32];
	int slot;

	GM_CHECK(self);
	if (!cmd) {
		CheckFailed("AnimCmd", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (Proc_Scan(cmd, "Morph,&d,&.*t", &slot, 0x1f, tag) == 2) {
		CModelo_SetMorphSubset(self->Model, (uint32_t)slot, tag);
		return;
	}
	E3D_LogBlue(g_FileLog, "Invalid anim command: %s", cmd);
}

byte __thiscall CGameModel_AnimTime(CGameModel *self, int i, float frame, float *time) {
	if (!self->Model || !time) {
		CheckFailed("Model && Time", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CAnimGrupo *g = CGameModel_Link(self, i);
	float base = self->animBaseFPS, vel = g->ctrl.vel;
	int frames = g->ctrl.frames;
	if (base != 0.0f && base != vel) {
		float f = vel * frame / base - (float)g->ctrl.primero;
		if (!(f >= -0.5f) || !((float)frames >= f)) {
			Con_FatalErrorf("Frame out of range:  orig.Frame: %f  frame: %f  nframes: %d  priframe: %d",
				(double)frame, (double)f, frames, g->ctrl.primero);
		}
		float v = 0.0f;
		if (!(0.0f >= f)) {
			float last = (float)(frames - 1);
			v = last < f ? last : f;
		}
		*time = v / vel;
		return 1;
	}
	float f = frame - (float)g->ctrl.primero;
	*time = f / vel;
	if (0.0f > f || f > (float)(frames - 1)) {
		return 0;
	}
	return 1;
}

void __thiscall CGameModel_SetAnim(CGameModel *self, int i, uint32_t chan, int unused) {
	GM_CHECK(self);
	(void)unused;
	CModelo_SetAnim(self->Model, i, chan);
	self->Model->animate = 0;
}

void __thiscall CGameModel_SetAnimName(CGameModel *self, const char *name, uint32_t chan, int unused) {
	GM_CHECK(self);
	(void)unused;
	CModelo_SetAnimName(self->Model, name, chan);
	self->Model->animate = 0;
}

void __fastcall CGameModel_StopAnims(CGameModel *self) {
	GM_CHECK(self);
	for (uint32_t i = 0; i < 4; i++) {
		CModelo_SetAnim(self->Model, 0, i);
	}
}

void __thiscall CGameModel_StopAnim(CGameModel *self, uint32_t chan) {
	GM_CHECK(self);
	CModelo_SetAnim(self->Model, -1, chan);
}

CAnimGrupo *__thiscall CGameModel_ChanAnim(CGameModel *self, uint32_t chan) {
	GM_CHECK(self);
	TABLA_CHECK(self->Model->slots.p, chan, self->Model->slots.n);
	return ((CAnimSlot *)self->Model->slots.p)[chan].anim;
}

byte __thiscall CGameModel_ChanEnd(CGameModel *self, uint32_t chan) {
	CAnimGrupo *g = CGameModel_ChanAnim(self, chan);
	if (!g) {
		CheckFailed("link", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return g->ctrl.state == 3;
}

byte __thiscall CGameModel_ChanInfo(CGameModel *self, uint32_t chan, int *mode, int *anim) {
	CAnimGrupo *g = CGameModel_ChanAnim(self, chan);
	if (!g) {
		CheckFailed("link", __FILE__, __TIMESTAMP__, __LINE__);
		return 0;
	}
	if (mode) {
		*mode = g->ctrl.state == 3 ? 4 : 1;
	}
	if (anim) {
		*anim = CModelo_FindAnim(self->Model, g->name);
	}
	return 1;
}

byte __thiscall CGameModel_ChanTime(CGameModel *self, uint32_t chan, float *t) {
	CAnimGrupo *g = CGameModel_ChanAnim(self, chan);
	if (!g) {
		CheckFailed("link", __FILE__, __TIMESTAMP__, __LINE__);
		return 0;
	}
	if (t) {
		float f = g->ctrl.frame - (float)g->ctrl.primero;
		f = f > 0.0f ? f : 0.0f;
		*t = f / g->ctrl.vel;
	}
	return 1;
}

void __thiscall CGameModel_RootMotion(CGameModel *self, float t, vec3_t *move, int *saltos) {
	byte first = 0;

	GM_CHECK(self);
	for (uint32_t i = 0; i < 4; i++) {
		CAnimSlot *s = &((CAnimSlot *)self->Model->slots.p)[i];
		TABLA_CHECK(self->Model->slots.p, i, self->Model->slots.n);
		if (!s->anim) {
			continue;
		}
		CAnimCtrl_Update(&s->anim->ctrl, &self->Model->rot2, t);
		if (i == 0) {
			if (move) {
				*move = s->anim->ctrl.move;
			}
			if (saltos) {
				*saltos = s->anim->ctrl.saltos;
			}
			first = 1;
		}
		TABLA_CHECK(self->Model->slots.p, i, self->Model->slots.n);
		if (s->prev) {
			CAnimCtrl_Update(&s->prev->ctrl, &self->Model->rot2, t);
		}
	}
	if (!first) {
		if (move) {
			*move = (vec3_t){ 0.0f, 0.0f, 0.0f };
		}
		if (saltos) {
			*saltos = 0;
		}
	}
}

byte __thiscall CGameModel_AnimDelta(CGameModel *self, uint32_t chan, float t, vec3_t *move) {
	vec3_t p0, p1, a;
	quat_t q0, q1;

	GM_CHECK(self);
	CAnimGrupo *g = CGameModel_ChanAnim(self, chan);
	if (!g) {
		if (move) {
			*move = (vec3_t){ 0.0f, 0.0f, 0.0f };
		}
		return 0;
	}
	float pri = (float)g->ctrl.primero;
	float f = t * g->ctrl.vel + pri;
	float fr = pri;
	if (pri < f) {
		float last = (float)(g->ctrl.primero - 1 + g->ctrl.frames);
		fr = last < f ? last : f;
	}
	CAnimCtrl_GetPosRot(&g->ctrl, g->ctrl.frame, &p0, &q0);
	CAnimCtrl_GetPosRot(&g->ctrl, fr, &p1, &q1);
	vec3_t mv = { p1.x - p0.x, 0.0f, p1.z - p0.z };
	vec3_t ofs = { 0.0f, p1.y, 0.0f };
	const vec3_t *r2 = &self->Model->rot2;
	if (r2->x != 0.0f || r2->y != 0.0f || r2->z != 0.0f) {
		mv = *Vec3_Rotate(&a, &mv, r2);
		ofs = *Vec3_Rotate(&a, &ofs, r2);
	}
	CAnimCtrl_Reset(&g->ctrl, fr);
	g->ctrl.ofs = ofs;
	g->ctrl.move = mv;
	g->ctrl.saltos = 0;
	if (move) {
		*move = mv;
	}
	return 1;
}

int __thiscall CGameModel_FindNodoR(CGameModel *self, const char *name) {
	uint32_t i;

	GM_CHECK(self);
	return CModelo_FindNodoR(self->Model, name, &i, 0, 1) ? (int)i : -1;
}

int __thiscall CGameModel_FindNodoRWild(CGameModel *self, const char *name) {
	uint32_t i;

	GM_CHECK(self);
	return CModelo_FindNodoR(self->Model, name, &i, 1, 1) ? (int)i : -1;
}

/* render node i of the model */
static CNodoR *CGameModel_NodoR(const CGameModel *self, uint32_t i) {
	CTabla *t = self->Model->nodosR;
	TABLA_CHECK(t->p, i, t->n);
	return (CNodoR *)t->p + i;
}

CNodoR *__thiscall CGameModel_NodoRRef(CGameModel *self, uint32_t i) {
	GM_CHECK(self);
	CNodoR *r = CGameModel_NodoR(self, i);
	if (!r) {
		CheckFailed("RNode", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!(r->flags & 2)) {
		CheckFailed("RNode->Flags & RNODE_UNIQUE", __FILE__, __TIMESTAMP__, __LINE__);
	}
	(*(int *)r)++;
	return r;
}

vec3_t *__thiscall CGameModel_NodoPos(CGameModel *self, vec3_t *out, uint32_t i, byte animated) {
	GM_CHECK(self);
	if (animated) {
		CheckFailed("bAnimated == false", __FILE__, __TIMESTAMP__, __LINE__);
	}
	*out = CGameModel_NodoR(self, i)->nodo->pos;
	return out;
}

matrix_t *__thiscall CGameModel_NodoMatrix(CGameModel *self, matrix_t *out, uint32_t i, byte anim, byte *visible,
	uint32_t slot) {
	matrix_t m;

	GM_CHECK(self);
	Matrix_Copy(out, Matrix_Self(CModelo_GetNodeMatrix(self->Model, &m, i, anim, slot, visible, NULL)));
	return out;
}

byte __thiscall CGameModel_NodoLook(CGameModel *self, uint32_t i, int mode, const vec3_t *v) {
	GM_CHECK(self);
	CNodoR *r = CGameModel_NodoR(self, i);
	switch (mode) {
	case 0:
		CNodoR_SetLook(r, 0);
		break;
	case 1:
		CNodoR_SetLook(r, 1);
		CRotacion_SetEuler(&r->look->r1, v);
		r->look->r2.ang = (vec3_t){ 0.0f, 0.0f, 0.0f };
		r->look->r2.flags |= ROT_DIRTY | ROT_ZERO;
		break;
	case 2:
		CNodoR_SetLook(r, 1);
		r->look->r1.ang = (vec3_t){ 0.0f, 0.0f, 0.0f };
		r->look->r1.flags |= ROT_DIRTY | ROT_ZERO;
		CRotacion_SetEuler(&r->look->r2, v);
		break;
	case 3:
	case 5:
		CNodoR_SetLook(r, mode == 3 ? 4 : 5);
		CRotacion_SetEuler(&r->look->r1, v);
		break;
	case 4:
	case 6:
		CNodoR_SetLook(r, mode == 4 ? 2 : 3);
		CRotacion_SetEuler(&r->look->r2, v);
		break;
	default:
		CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return 1;
}

/* the ini of the node of render node i */
static CArchINI *CGameModel_NodoIni(const CGameModel *self, uint32_t i) {
	return CGameModel_NodoR(self, i)->nodo->ini;
}

float __thiscall CGameModel_NodoIniFloat(CGameModel *self, uint32_t i, const char *sect, const char *key, float def) {
	return CArchINI_GetFloat(CGameModel_NodoIni(self, i), sect, key, def);
}

int __thiscall CGameModel_NodoIniInt(CGameModel *self, uint32_t i, const char *sect, const char *key, int def) {
	return CArchINI_GetInt(CGameModel_NodoIni(self, i), sect, key, def);
}

uint32_t __thiscall CGameModel_NodoIniColor(CGameModel *self, uint32_t i, const char *sect, const char *key,
	uint32_t def) {
	return CArchINI_GetColor(CGameModel_NodoIni(self, i), sect, key, def);
}

vec3_t *__thiscall CGameModel_NodoIniVec3(CGameModel *self, vec3_t *out, uint32_t i, const char *sect, const char *key,
	const vec3_t *def) {
	CArchINI_GetVec3(CGameModel_NodoIni(self, i), out, sect, key, def);
	return out;
}

/* the mesh of a node, NULL with the objeto3d.h CHECK for another class */
static CMallaD3D *Nodo_MallaD3D(CNodo3D *n) {
	IObjeto3D *o = n->obj3D;
	if (o->clase != CLASE_OBJ3D_MALLAD3D) {
		CheckFailed("devNULL", "objeto3d.h", __TIMESTAMP__, __LINE__);
		return NULL;
	}
	return (CMallaD3D *)o;
}

byte __thiscall CGameModel_SetTexFrame(CGameModel *self, const char *name, int frame) {
	for (CNodo3D *n = self->Model->escena->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D) {
			continue;
		}
		CMallaD3D *m = Nodo_MallaD3D(n);
		CMapa *mapa = m->mat->mapa[0];
		if (mapa && !crt_stricmp(m->mat->Nombre, name)) {
			CAnimMat_SetFrame(mapa->AnimMat, frame);
			return 1;
		}
	}
	return 0;
}

/* the render node transforms of this frame (or always with R_ForcePreRenderTransform) */
static void CGameModel_UpdateTransforms(CGameModel *self) {
	static const vec3_t one = { 1.0f, 1.0f, 1.0f };
	NodoAnimCtx ctx;

	if (!g_ForcePreRenderTransform && self->frame == g_FrameCount) {
		return;
	}
	self->frame = g_FrameCount;
	for (int i = 0; i < 4; i++) {
		RenderCtx_SetCanal(&ctx.chan[i], NULL);
	}
	RenderCtx_Init(&ctx, NULL, &one);
	CModelo_SetupRender(self->Model, &ctx);
	CEscena_Update(self->Model->escena, &ctx);
}

/* the meshes of the material give vertices */
static byte Malla_ConVertices(const CMallaD3D *m) {
	return !m->mat || !(m->mat->params.atrib & MAT_SINVERTICES);
}

/* the nodes of render node r that are meshes (not hidden unless any) */
static CMallaD3D *NodoR_Malla(CNodoR *r, byte hidden) {
	CNodo3D *n = r->nodo;
	if ((!hidden && n->flags & NODO_HIDE) || CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D) {
		return NULL;
	}
	return Nodo_MallaD3D(n);
}

/* the world matrix of a mesh of render node r: the skinned one with a skin */
static const matrix_t *NodoR_MatrizMalla(const CNodoR *r, const CMallaD3D *m) {
	return m->pSkin ? &r->mObj : &r->m;
}

int __thiscall CGameModel_VertexCounts(CGameModel *self, int *out, int max) {
	int n = self->Model->nodosR->n;
	if (!out) {
		return n;
	}
	for (int i = 0; i < n; i++, out++) {
		CNodoR *r = CGameModel_NodoR(self, (uint32_t)i);
		if (i >= max) {
			CheckFailed("n < MaxNodes", __FILE__, __TIMESTAMP__, __LINE__);
		}
		*out = 0;
		CNodo3D *nd = r->nodo;
		if (!(nd->flags & NODO_HIDE) && CNodo3D_Clase(nd) == CLASE_OBJ3D_MALLAD3D) {
			for (CMallaD3D *m = Nodo_MallaD3D(nd); m; m = m->next) {
				*out += (int)m->vert.nVert;
			}
		}
	}
	return n;
}

static void Pos_Repetir(vec3_t *pos, int count, const vec3_t *p, int *index) {
	for (int i = 0; i < count; i++) {
		pos[i] = *p;
		if (index) {
			index[i] = 0;
		}
	}
}

/* the CHECKs of the vertex functions */
static void CGameModel_CheckVertArgs(const CGameModel *self, const void *pos, const void *normal, const void *area,
	int count) {
	GM_CHECK(self);
	if (count < 1) {
		CheckFailed("Count > 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!pos) {
		CheckFailed("Pos", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (normal) {
		CheckFailed("!Normal", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (area) {
		CheckFailed("!fArea", __FILE__, __TIMESTAMP__, __LINE__);
	}
}

void __thiscall CGameModel_RandomVertices(CGameModel *self, vec3_t *pos, void *normal, void *area, int count,
	uint32_t nodo) {
	byte validos[MAX_VALIDOS];
	int nValidos = 0;

	CGameModel_CheckVertArgs(self, pos, normal, area, count);
	if (!g_GetModelVertex) {
		Pos_Repetir(pos, count, &self->Model->pos, NULL);
		return;
	}
	CGameModel_UpdateTransforms(self);
	if (nodo == 0xffffffff) {
		for (uint32_t i = 0; (int)i < self->Model->nodosR->n; i++) {
			CMallaD3D *m = NodoR_Malla(CGameModel_NodoR(self, i), 0);
			for (; m; m = m->next) {
				if (Malla_ConVertices(m)) {
					if (nValidos >= MAX_VALIDOS) {
						CheckFailed("nValidos < MAX_VALIDOS", __FILE__, __TIMESTAMP__, __LINE__);
					}
					if (i >= 256) {
						CheckFailed("n < 256", __FILE__, __TIMESTAMP__, __LINE__);
					}
					validos[nValidos++] = (byte)i;
					break;
				}
			}
		}
		if (nValidos <= 0) {
			CheckFailed("nValidos > 0", __FILE__, __TIMESTAMP__, __LINE__);
		}
	}
	for (int k = 0; k < count; k++) {
		uint32_t idx = nodo;
		if (nodo == 0xffffffff) {
			idx = validos[crt_rand() % nValidos];
		}
		CNodoR *r = CGameModel_NodoR(self, idx);
		if (CNodo3D_Clase(r->nodo) != CLASE_OBJ3D_MALLAD3D) {
			CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
			continue;
		}
		CMallaD3D *best = NULL;
		int bestR = 0;
		for (CMallaD3D *m = Nodo_MallaD3D(r->nodo); m; m = m->next) {
			if (Malla_ConVertices(m)) {
				int rr = crt_rand();
				if (bestR <= rr) {
					best = m;
					bestR = rr;
				}
			}
		}
		int nv = (int)best->vert.nVert;
		vec3_t p;
		CListaVert_GetPos(&best->vert, &p, (uint32_t)(crt_rand() % nv));
		Vec3_TransformH(&p, &p, NodoR_MatrizMalla(r, best));
		pos[k] = p;
	}
}

void __thiscall CGameModel_Vertices(CGameModel *self, vec3_t *pos, void *normal, void *area, int count, int first,
	int *index, uint32_t nodo, int *indices) {
	byte validos[MAX_VALIDOS];
	int base[MAX_VALIDOS + 1];
	int nValidos = 0, total = 0, total2 = 0;

	CGameModel_CheckVertArgs(self, pos, normal, area, count);
	if (!g_GetModelVertex) {
		Pos_Repetir(pos, count, &self->Model->pos, index);
		return;
	}
	CGameModel_UpdateTransforms(self);
	for (uint32_t i = 0; (int)i < self->Model->nodosR->n; i++) {
		CNodoR *r = CGameModel_NodoR(self, i);
		CMallaD3D *m0 = NodoR_Malla(r, nodo != 0xffffffff);
		for (CMallaD3D *m = m0; m; m = m->next) {
			if (!Malla_ConVertices(m)) {
				continue;
			}
			if (nValidos >= MAX_VALIDOS) {
				CheckFailed("nValidos < MAX_VALIDOS", __FILE__, __TIMESTAMP__, __LINE__);
			}
			if (i >= MAX_VALIDOS) {
				CheckFailed("n < MAX_VALIDOS", __FILE__, __TIMESTAMP__, __LINE__);
			}
			if (nodo == 0xffffffff || nodo == i) {
				validos[nValidos] = (byte)i;
				base[nValidos] = total;
				nValidos++;
				for (CMallaD3D *m2 = Nodo_MallaD3D(r->nodo); m2; m2 = m2->next) {
					if (Malla_ConVertices(m2)) {
						total += (int)m2->vert.nVert;
					}
				}
				total2 = total;
			}
			break;
		}
	}
	base[nValidos] = total;
	if (nValidos < 1) {
		CheckFailed("nValidos > 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (total < 1) {
		CheckFailed("nVertex > 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	for (int k = 0; k < count; k++) {
		int v;
		if (indices) {
			v = *indices++;
			if (v >= total2) {
				v = total2 - 1;
			}
		} else if (first == -1) {
			v = crt_rand() % total;
		} else {
			v = first >= total ? total - 1 : first;
			first++;
		}
		int lo = 0, hi = nValidos;
		while (lo != hi) {
			int mid = (hi - lo) / 2 + lo;
			if (base[mid] <= v) {
				if (v < base[mid + 1]) {
					uint32_t idx = validos[mid];
					int iv = v - base[mid];
					CNodoR *r = CGameModel_NodoR(self, idx);
					if (CNodo3D_Clase(r->nodo) != CLASE_OBJ3D_MALLAD3D) {
						CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
						break;
					}
					CMallaD3D *m = Nodo_MallaD3D(r->nodo);
					int acc = 0;
					for (; m; m = m->next) {
						if (Malla_ConVertices(m)) {
							if (acc <= iv && iv < (int)m->vert.nVert + acc) {
								iv -= acc;
								break;
							}
							acc += (int)m->vert.nVert;
						}
					}
					vec3_t p;
					CListaVert_GetPos(&m->vert, &p, (uint32_t)iv);
					Vec3_TransformH(&p, &p, NodoR_MatrizMalla(r, m));
					pos[k] = p;
					if (index) {
						index[k] = v;
					}
					break;
				}
				lo = mid;
			} else {
				hi = mid;
			}
		}
	}
}

void __thiscall CGameModel_ClosestVertex(CGameModel *self, float x, float y, float z, vec3_t *pos, int *index,
	uint32_t nodo) {
	GM_CHECK(self);
	if (!pos) {
		CheckFailed("Pos", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!g_GetModelVertex) {
		const vec3_t *mp = &self->Model->pos;
		float dx = (x - mp->x) * 0.5f, dy = (y - mp->y) * 0.5f, dz = (z - mp->z) * 0.5f;
		pos->y = mp->y + dy;
		pos->x = dx + mp->x;
		pos->z = mp->z + dz;
		if (index) {
			*index = 0;
		}
		return;
	}
	CGameModel_UpdateTransforms(self);
	const vec3_t q = { x, y, z };
	float best = 3.4028234663852886e+38f;
	int total = 0;
	for (uint32_t i = 0; (int)i < self->Model->nodosR->n; i++) {
		CNodoR *r = CGameModel_NodoR(self, i);
		CMallaD3D *m0 = NodoR_Malla(r, nodo != 0xffffffff);
		for (CMallaD3D *m = m0; m; m = m->next) {
			if (!Malla_ConVertices(m)) {
				continue;
			}
			if (nodo == 0xffffffff || nodo == i) {
				for (CMallaD3D *m2 = Nodo_MallaD3D(r->nodo); m2; m2 = m2->next) {
					if (!Malla_ConVertices(m2)) {
						continue;
					}
					const matrix_t *mt = NodoR_MatrizMalla(r, m2);
					matrix_t inv;
					vec3_t lp;
					Matrix_Invert(&inv, mt);
					Vec3_TransformH(&lp, &q, &inv);
					for (int v = 0; v < (int)m2->vert.nVert; v++) {
						vec3_t vp;
						CListaVert_GetPos(&m2->vert, &vp, (uint32_t)v);
						float dy = lp.y - vp.y, dx = lp.x - vp.x, dz = lp.z - vp.z;
						float d = dy * dy + dx * dx + dz * dz;
						if (best > d) {
							best = d;
							vec3_t w;
							Vec3_TransformH(&w, &vp, mt);
							*pos = w;
							if (index) {
								*index = total + v;
							}
						}
					}
					total += (int)m2->vert.nVert;
				}
			}
			break;
		}
	}
	if (total < 1) {
		CheckFailed("nVertex > 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
}

void __fastcall CGameModel_FreeCollision(CGameModel *self) {
	if (self->CollideMesh) {
		GM_CHECK(self);
		E3D_LogWhite(g_FileLog, "~CollisionMesh: %s (ID: %d)", self->Model->name, self->CollisionMeshID);
		CSuelo_RemoveMesh(Mundo_Suelo(), self->CollideMesh, self->CollisionMeshID, self->CollisionIni);
		if (self->CollideMesh) {
			CCollideMesh_dtor(self->CollideMesh);
			operator_delete_sized(self->CollideMesh, sizeof(CCollideMesh));
			self->CollideMesh = NULL;
		}
	}
	self->CollisionMeshID = 0;
	self->CollisionIni = 0;
	self->collide = 0;
}

void __thiscall CGameModel_EnableCollision(CGameModel *self, byte on, byte create) {
	if (self->CollisionMeshID == -1) {
		return;
	}
	if (!self->CollideMesh) {
		if (create && !CGameModel_CreateCollision(self, 0)) {
			return;
		}
		if (!self->CollideMesh) {
			CheckFailed("CollideMesh", __FILE__, __TIMESTAMP__, __LINE__);
		}
	}
	if (!self->CollisionMeshID) {
		CheckFailed("CollisionMeshID", __FILE__, __TIMESTAMP__, __LINE__);
	}
	E3D_LogWhite(g_FileLog, "Collision Mesh: %s [%s]", self->Model->name, on ? "ENABLE" : "DISABLE");
	if (self->collide != on) {
		self->collide = on;
		CSuelo_SetMeshCollide(Mundo_Suelo(), self->CollideMesh, self->collide, self->CollisionMeshID,
			self->CollisionIni);
	}
}

byte __thiscall CGameModel_CreateCollision(CGameModel *self, byte all) {
	matrix_t mw;
	byte ok;

	GM_CHECK(self);
	if (self->CollisionMeshID) {
		CheckFailed("!CollisionMeshID", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (self->CollideMesh) {
		CheckFailed("!CollideMesh", __FILE__, __TIMESTAMP__, __LINE__);
	}
	const vec3_t *mp = &self->Model->pos;
	int s1 = Sector_At(mp);
	vec3_t up = { mp->x + 0.0f, mp->y + 20.0f, mp->z + 0.0f };
	if (s1 != Sector_At(&up)) {
		Con_Warnf("WARN: Possible wrong sector for Model: [%s]  Pos: %s\n", self->Model->name,
			Vec3_Str(&self->Model->pos, 0, 1));
	}
	CModelo_GetMatrix(self->Model, &mw);
	CEscena *esc = self->Model->escena;
	if (!esc->CollideMesh) {
		CMallaTri *t = operator_new(sizeof(CMallaTri));
		esc->CollideMesh = t ? (IObjeto3D *)CMallaTri_ctor(t) : NULL;
		CEscena_UpdateNodes(esc);
		for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
			if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D || (!(n->flags & NODO_COLISION) && !all)) {
				continue;
			}
			CMallaD3D *m = Nodo_MallaD3D(n);
			if (m->pSkin) {
				CheckFailed("!Mesh2->pSkin", __FILE__, __TIMESTAMP__, __LINE__);
			}
			if (!m->pTri) {
				CheckFailed("Mesh2->pTri", __FILE__, __TIMESTAMP__, __LINE__);
				if (!m->pTri) {
					continue;
				}
			}
			CMallaTri tm;
			CMallaTri_ctor(&tm);
			CMallaTri_AddD3D(&tm, m, 1);
			CMallaTri_Transform(&tm, &n->m);
			CMallaTri_Add((CMallaTri *)esc->CollideMesh, &tm, 0, 0);
			CMallaTri_dtor(&tm);
		}
	}
	CMallaTri tw;
	CMallaTri_ctorCopy(&tw, (CMallaTri *)esc->CollideMesh);
	CMallaTri_Transform(&tw, &mw);
	E3D_LogDebug(g_FileLog, "StaticCollisionInit: %s, %d tri, %d vert", self->Model->name, tw.nTri, tw.nVert);
	if (!tw.nTri || !tw.nVert) {
		E3D_LogWhite(g_FileLog, "Model has no collision mesh: %s", self->Model->name);
		self->CollisionMeshID = -1;
		ok = 0;
	} else {
		CCollideMesh *cm = operator_new(sizeof(CCollideMesh));
		if (cm) {
			byte param = cm->Param;
			memset(cm, 0, sizeof(*cm));
			cm->Param = param;
		}
		self->CollideMesh = cm;
		cm->flags |= 2;
		CSuelo_AddMesh(Mundo_Suelo(), cm, &tw, (uint32_t *)&self->CollisionMeshID, &self->CollisionIni);
		if (!self->CollisionMeshID) {
			CheckFailed("CollisionMeshID", __FILE__, __TIMESTAMP__, __LINE__);
		}
		self->collide = 1;
		ok = 1;
	}
	CMallaTri_dtor(&tw);
	return ok;
}

byte __fastcall CGameModel_CollideOn(const CGameModel *self) {
	return self->CollideMesh && self->collide;
}

void __thiscall CRotacion_SetEuler(CRotacion *self, const vec3_t *ang) {
	self->ang = *ang;
	self->flags |= ROT_DIRTY;
	if (ang->x == 0.0f && ang->y == 0.0f && ang->z == 0.0f) {
		self->flags |= ROT_ZERO;
		return;
	}
	self->flags &= ~ROT_ZERO;
}
