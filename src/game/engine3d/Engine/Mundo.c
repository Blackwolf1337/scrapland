/*
 * Engine/mundo.cpp of the engine3d: CMundo, see include/scrap/mundo.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fuentes.h"
#include "scrap/leem3d.h"
#include "scrap/mundo.h"
#include "scrap/sistpart.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"

#define DEG2RAD_F 0.0174532924f		/* rva 0x3dcc2c */

/* the start of the own camera: 240 behind the origin, 50 up, looking at it */
static void Mundo_CamaraInicio(CMundo *self) {
	self->camara.pos = (vec3_t){ 0.0f, 50.0f, -240.0f };
	self->cam->target = (vec3_t){ 0.0f, 0.0f, 0.0f };
}

CMundo *__fastcall CMundo_ctor(CMundo *self) {
	CNiebla_ctor(&self->niebla);
	CCamara_ctor(&self->camara);
	self->cam = &self->camara;
	self->cieloPos = 1.0f;
	self->cieloRot = 0.0f;
	self->Suelo = NULL;
	self->EscSuelo = NULL;
	self->ObjSuelo = NULL;
	self->Cielo = NULL;
	self->EscCielo = NULL;
	self->tubo = NULL;
	self->ini_D3D = 0;
	self->dummies = NULL;
	Mundo_CamaraInicio(self);
	CCamara_LookAtRot(self->cam);
	CCamara_UpdateVectors(self->cam);
	return self;
}

void __fastcall CMundo_dtor(CMundo *self) {
	BspQ3_Free();
	Bsp_Free();
	Sectores_Free();
	Modelo_Destroy(&self->Suelo, 0);
	Modelo_Destroy(&self->Cielo, 0);
	MundoDummy_FreeList(&self->dummies);
	CCamara_dtor(&self->camara);
	CNiebla_dtor(&self->niebla);
}

static void Dummy_Zero(MundoDummy *d) {
	d->name = NULL;
	d->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	d->rot = (vec3_t){ 0.0f, 0.0f, 0.0f };
	d->next = NULL;
	d->ini = NULL;
}

/* the members the dummy owns */
static void Dummy_Free(MundoDummy *d) {
	if (d->ini) {
		CArchINI *ini = d->ini;
		CArchINI_dtor(ini);
		operator_delete_sized(ini, sizeof(CArchINI));
		d->ini = NULL;
	}
	if (d->name) {
		Cad_Set(&d->name, NULL);
	}
}

static void Dummy_CheckName(const MundoDummy *d, const char *warn) {
	if (crt_strnicmp(d->name, "DM_", 3)) {
		Con_Warnf(warn, d->name);
	}
}

MundoDummy *__thiscall MundoDummy_ctorNodo(MundoDummy *self, CNodo3D *nodo) {
	vec3_t rot;

	Dummy_Zero(self);
	Cad_Set(&self->name, nodo->name);
	self->pos = (vec3_t){ nodo->m.m[12], nodo->m.m[13], nodo->m.m[14] };
	self->rot = *Matrix_ToEuler(&rot, &nodo->m);
	Dummy_CheckName(self, "WARN: Node set as World Dummy: %s\n");
	if (nodo->ini) {
		E3D_LogWhite(g_FileLog, "INFO: World Dummy has UserProp: %s", self->name);
	}
	self->ini = nodo->ini;
	nodo->ini = NULL;
	CHECK(!nodo->obj58);
	return self;
}

MundoDummy *__thiscall MundoDummy_ctor(MundoDummy *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	Dummy_Zero(self);
	Cad_Set(&self->name, name);
	self->pos = *pos;
	self->rot = *rot;
	Dummy_CheckName(self, "WARN: Additional World Dummy: %s\n");
	return self;
}

MundoDummy *__fastcall MundoDummy_ctorEmpty(MundoDummy *self) {
	Dummy_Zero(self);
	return self;
}

void __fastcall MundoDummy_dtor(MundoDummy *self) {
	CHECK(self->next == NULL);
	Dummy_Free(self);
}

void __fastcall MundoDummy_Clear(MundoDummy *self) {
	Dummy_Zero(self);
}

int __fastcall MundoDummy_Count(const MundoDummy *list) {
	int n = 0;
	for (; list; list = list->next) {
		n++;
	}
	return n;
}

void MundoDummy_FreeList(MundoDummy **list) {
	while (*list) {
		MundoDummy *d = *list;
		*list = d->next;
		d->next = NULL;
		Dummy_Free(d);
		operator_delete_sized(d, sizeof(MundoDummy));
	}
}

MundoDummy *__thiscall MundoDummy_Find(MundoDummy *self, const char *name, byte self_too, byte case_sens) {
	if (!name) {
		return NULL;
	}
	for (MundoDummy *d = self_too ? self : self->next; d; d = d->next) {
		if (!(case_sens ? strcmp(name, d->name) : crt_stricmp(name, d->name))) {
			return d;
		}
	}
	return NULL;
}

void __thiscall CMundo_AddDummy(CMundo *self, MundoDummy *d) {
	CHECK(!d->next);
	if (self->dummies && d->name) {
		for (MundoDummy *e = self->dummies; e; e = e->next) {
			if (!crt_stricmp(d->name, e->name)) {
				Con_Warnf("WARN: duplicated World Dummy: %s\n", d->name);
				break;
			}
		}
	}
	MundoDummy **slot = &self->dummies;
	while (*slot) {
		slot = &(*slot)->next;
	}
	*slot = d;
}

void __fastcall CMundo_ReleaseD3D(CMundo *self) {
	if (!(g_pApp->flags >> 2 & 1)) {
		CHECK(self->ini_D3D);
		CHECK(g_MotorDev);
	}
	self->ini_D3D = 0;
	void *dev = g_MotorDev;
	OBJ3D_CALL((IObjeto3D *)self->cam, OBJ3D_VF_RELEASED3D, objDev_t, dev);
	if (self->Cielo) {
		CModelo_ReleaseD3D(self->Cielo, dev, 1);
	}
	if (self->Suelo) {
		CModelo_ReleaseD3D(self->Suelo, dev, 1);
	}
	BspQ3_ReleaseD3D();
	Bsp_ReleaseD3D();
}

void __fastcall CMundo_InitD3D(CMundo *self) {
	CHECK(!self->ini_D3D);
	self->ini_D3D = 1;
	void *dev = g_MotorDev;
	CHECK(dev);
	OBJ3D_CALL((IObjeto3D *)self->cam, OBJ3D_VF_INITD3D, objDev_t, dev);
	if (self->Cielo) {
		CModelo_InitD3D(self->Cielo, dev, 1);
	}
	if (self->Suelo) {
		CModelo_InitD3D(self->Suelo, dev, 1);
	}
	CNiebla_InitD3D(&self->niebla);
	Bsp_InitD3D();
	BspQ3_InitD3D();
}

int Mundo_LoadDUM(const char *path, MundoDummy **list) {
	char name[MAX_RUTA];

	Ruta_Format(name, path, "&U&D&N.dum");
	if (!File_Exists(name)) {
		Con_FatalErrorf("File not found: %s", name);
		return -1;
	}
	void *mem = operator_new(sizeof(CArchIO));
	CArchIO *f = mem ? CArchIO_ctor(mem, name, "rb") : NULL;
	CHECK(!f->error);
	E3D_LogDebug2(g_FileLog, "Load_DUM: %s", name);
	CArchIO_BeginBlock(f, "DUM");
	CHECK(CArchIO_ReadInt(f) == 1);
	int count = CArchIO_ReadInt(f);
	*list = NULL;
	MundoDummy *last = NULL;
	while (CArchIO_ReadInt(f)) {
		void *p = operator_new(sizeof(MundoDummy));
		MundoDummy *d = p ? MundoDummy_ctorEmpty(p) : NULL;
		Cad_Read(&d->name, f);
		CArchIO_Read(f, &d->pos, sizeof(d->pos));
		CArchIO_Read(f, &d->rot, sizeof(d->rot));
		ArchINI_Read(f, &d->ini);
		*(*list ? &last->next : list) = d;
		last = d;
		count--;
	}
	CHECK(!count);
	CArchIO_Error(f, "DUM");
	CHECK(!f->error);
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(CArchIO));
	return 0;
}

int Mundo_SaveDUM(const char *path, const MundoDummy *list) {
	char name[MAX_RUTA];

	Ruta_Format(name, path, "&U&D&N.dum");
	E3D_LogDebug2(g_FileLog, "Save_DUM: %s", name);
	void *mem = operator_new(sizeof(CArchIO));
	CArchIO *f = mem ? CArchIO_ctor(mem, name, "wb") : NULL;
	CHECK(!f->error);
	CArchIO_BeginBlockWrite(f, "DUM");
	CArchIO_WriteInt(f, 1);
	CArchIO_WriteInt(f, MundoDummy_Count(list));
	for (const MundoDummy *d = list; d; d = d->next) {
		CArchIO_WriteInt(f, 1);
		Cad_Write((char **)&d->name, f);
		CArchIO_Write(f, &d->pos, sizeof(d->pos));
		CArchIO_Write(f, &d->rot, sizeof(d->rot));
		ArchINI_Write(f, d->ini);
	}
	CArchIO_WriteInt(f, 0);
	CArchIO_EndBlockWrite(f, "DUM");
	CHECK(!f->error);
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(CArchIO));
	return 0;
}

byte __thiscall CMundo_LoadMoreDummies(CMundo *self, const char *ini) {
	char name[MAX_RUTA];
	char sect[0x80];
	CArchINI f;

	Ruta_Merge(name, ini, self->EscSuelo->name);
	CArchINI_ctorLoad(&f, name, NULL);
	int n = CArchINI_Count(&f);
	int added = 0;
	for (int i = 1; i <= n; i++) {
		if (!CArchINI_GetSectionName(&f, i, sect, sizeof(sect))) {
			continue;
		}
		vec3_t tmp, def = { 0.0f, 0.0f, 0.0f };
		vec3_t pos = *CArchINI_GetVec3(&f, &tmp, sect, "Pos", &def);
		vec3_t def2 = { 0.0f, 0.0f, 0.0f };
		vec3_t rot = *CArchINI_GetVec3(&f, &tmp, sect, "Rot", &def2);
		/* in the coordinates of 3ds max, for a script that makes them there */
		Con_Debugf("Dummy boxsize:[100,100,100] pos:[%f,%f,%f] isSelected:on name:\"%s\" rotation:(eulerAngles 0 0 %f) \n",
			(double)pos.x, (double)pos.z, (double)pos.y, sect, (double)-rot.y);
		void *p = operator_new(sizeof(MundoDummy));
		MundoDummy *d = NULL;
		if (p) {
			vec3_t r = { rot.x * DEG2RAD_F, rot.y * DEG2RAD_F, rot.z * DEG2RAD_F };
			d = MundoDummy_ctor(p, sect, &pos, &r);
		}
		CMundo_AddDummy(self, d);
		added++;
	}
	Con_Verbosef("Additional World Dummies: %d\n", added);
	CArchINI_dtor(&f);
	return 1;
}

byte __thiscall CMundo_SaveMoreDummies(CMundo *self, const char *ini) {
	char name[MAX_RUTA];
	CArchINI f;

	Ruta_Merge(name, ini, self->EscSuelo->name);
	CArchINI_ctorLoad(&f, name, NULL);
	int changed = 0;
	if (self->dummies) {
		for (MundoDummy *d = self->dummies; d; d = d->next) {
			changed += CArchINI_SetStr(&f, d->name, "Pos", Vec3_Str(&d->pos, 0, 6));
			changed += CArchINI_SetStr(&f, d->name, "Rot", Vec3_StrDeg(&d->rot, 0, 2));
		}
		if (changed && CArchINI_Save(&f, name, NULL, 0)) {
			Err_Report(*crt_errno(), 0, "\n'%s'", name);
		}
	}
	CArchINI_dtor(&f);
	return changed != 0;
}

static void D3D_SetRenderState(void *dev, uint32_t state, uint32_t value) {
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

static void D3D_SetProj(const matrix_t *m) {
	D3D_METHOD(g_pd3dDevice, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(g_pd3dDevice, D3DTS_PROJECTION, m);
}

/* sky and floor: not lit by the lights of the level, never culled */
static void Mundo_ModeloFijo(CModelo *m) {
	m->dynLight = 0;
	m->alwaysVisible = 1;
}

static void Mundo_RenderCielo(CMundo *self, void *dev) {
	int pass = g_RenderPass;
	if (pass == 11) {
		g_RenderPass = 1;
	}
	E3D_FlushBatch();
	if (!g_SkyZBuffer) {
		D3D_SetRenderState(dev, D3DRS_ZENABLE, 0);
	}
	D3D_SetRenderState(dev, D3DRS_AMBIENT, Color_ToD3D(&self->EscCielo->ambiente));
	int fog = g_FogActive;
	g_FogActive = 0;
	CModelo *cielo = self->Cielo;
	Mundo_ModeloFijo(cielo);
	const CCamara *cam = self->cam;
	float k = self->cieloPos;
	cielo->pos = (vec3_t){ cam->pos.x * k * g_SkyPosScale, cam->pos.y * k * g_SkyPosScale,
		cam->pos.z * k * g_SkyPosScale };
	cielo->rot.y = (float)((double)g_SkyRotation * M_PI_D / DEG_PER_PI) + self->cieloRot;
	CModelo_UpdateAnims(cielo, 0.0f, -1);
	if (Portales_SetView(g_PintarTodas)) {
		CModelo_Render(self->Cielo);
	}
	E3D_FlushBatch();
	if (g_SkyZBuffer) {
		Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
	}
	Portales_SetView(NULL);
	g_FogActive = fog;
	D3D_SetRenderState(dev, D3DRS_AMBIENT, g_AmbientD3D);
	if (!g_SkyZBuffer) {
		D3D_SetRenderState(dev, D3DRS_ZENABLE, g_ZEnable);
	}
	if (self->tubo) {
		matrix_t m = *Render_GetProj();
		float f = g_SkyTubeTest;
		m.m[11] = 1.0f;
		m.m[10] = f / (f - 1.0f);
		m.m[14] = -m.m[10];
		D3D_SetProj(&m);
		CModelo_Enable(self->tubo);
		CModelo_Render(self->tubo);
		CModelo_Disable(self->tubo);
		m = *Render_GetProj();
		D3D_SetProj(&m);
		Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
	}
	g_RenderPass = pass;
}

void __fastcall CMundo_Render(CMundo *self) {
	CHECK(self->ini_D3D);
	void *dev = g_MotorDev;
	CHECK(dev);
	g_FogActive = 0;
	if (g_ModelosOn) {
		CNiebla_Apply(&self->niebla);
	}
	if (self->Cielo && g_RenderSky && g_PintarNum) {
		Mundo_RenderCielo(self, dev);
	}
	if (self->Suelo && g_RenderWorld) {
		Mundo_ModeloFijo(self->Suelo);
		CModelo_UpdateAnims(self->Suelo, 0.0f, -1);
		CModelo_Render(self->Suelo);
	}
	Bsp_Render();
	BspQ3_Render();
	CMundo_DrawDummies(self);
}

void __fastcall CMundo_DrawDummies(CMundo *self) {
	if (!*g_ShowWorldDummies) {
		return;
	}
	int shown = 0, total = 0;
	for (MundoDummy *d = self->dummies; d; d = d->next, total++) {
		if (!Wild_MatchLower(g_ShowWorldDummies, d->name)) {
			continue;
		}
		matrix_t m;
		Matrix_LoadIdentity(&m);
		Matrix_PostRotate(&m, &d->rot);
		Matrix_AddPos(&m, &d->pos);
		vec3_t o = { 0.0f, 0.0f, 0.0f };
		Debug_DrawAxes(&m, &o, 100.0f);
		vec3_t mn = { -50.0f, -50.0f, -50.0f };
		vec3_t mx = { 50.0f, 50.0f, 50.0f };
		Debug_DrawBoxM(&m, &mn, &mx, 0xffff00);
		Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &d->pos, d->ini ? 0xffff : 0xffffff, GAME_STR(str_3bae88, 0x3bae88), d->name);
		shown++;
	}
	Font_PrintfXY(g_FntComic14, 10.0f, 260.0f, "world dummies: %d / %d", shown, total);
}

void __thiscall CMundo_SetCieloPos(CMundo *self, float pct) {
	self->cieloPos = 1.0f - pct / 100.0f;
}

void __thiscall CMundo_SetCieloRot(CMundo *self, float deg) {
	self->cieloRot = (float)((double)deg * M_PI_D / DEG_PER_PI);
}

/* the lights of esc but the ones of the nodes with a flag of skip */
static void Mundo_SetLuces(CEscena *esc, uint32_t skip, byte activa) {
	if (!esc) {
		return;
	}
	for (CNodo3D *n = CEscena_FindClase(esc, NULL, CLASE_OBJ3D_LUZ); n; n = CEscena_FindClase(esc, n, CLASE_OBJ3D_LUZ)) {
		if (!(n->flags & skip)) {
			CLuz_SetActiva(Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ), activa);
		}
	}
}

void __thiscall CMundo_SetLucesCielo(CMundo *self, byte activa) {
	Mundo_SetLuces(self->EscCielo, 0, activa);
}

void __thiscall CMundo_SetLucesSuelo(CMundo *self, byte activa) {
	Mundo_SetLuces(self->EscSuelo, 0x6000000, activa);
}

void __thiscall CMundo_SetClip(CMundo *self, float zNear, float zFar) {
	if (self->EscSuelo) {
		int a, b;
		memcpy(&a, &zNear, sizeof(a));
		memcpy(&b, &zFar, sizeof(b));
		CEscena_Msg(self->EscSuelo, ESCENA_MSG_CAMARAS, a, b, 0, 0);
	}
	if (zNear != 0.0f) {
		self->camara.zNear = zNear;
	}
	if (zFar != 0.0f) {
		self->camara.zFar = zFar;
	}
}

void __thiscall CMundo_SetCamara(CMundo *self, CCamara *cam) {
	self->cam = cam;
	if (!cam) {
		self->cam = &self->camara;
		Mundo_CamaraInicio(self);
		CCamara_LookAtRot(self->cam);
	} else if (cam->mode == CAMARA_LOOKAT) {
		CCamara_LookAtRot(cam);
	}
	CCamara_UpdateVectors(self->cam);
}

void __thiscall CMundo_LoadCielo(CMundo *self, const char *name) {
	vec3_t escala = { 1.0f, 1.0f, 1.0f };
	CMundo_LoadCieloEscala(self, name, &escala);
}

static void Mundo_AnimEvents(CModelo *m) {
	CModelo_AddAnimEvents(m, CModelo_GetAnimName(m, 0), -1.0f, -1.0f);
}

void __thiscall CMundo_LoadCieloEscala(CMundo *self, const char *name, const vec3_t *escala) {
	CHECK(!self->ini_D3D);
	CHECK(!self->Cielo);
	if (!g_SkyInit || !name || !*name) {
		return;
	}
	self->Cielo = Crear_Modelo3(name, NULL, escala, ESCENA_SM3 | ESCENA_USARANIM);
	self->EscCielo = self->Cielo->escena;
	CHECK(self->EscCielo);
	if (CAnimGrupos_Count(&self->Cielo->anims)) {
		CHECK(self->EscCielo->AnimM3D);
		Mundo_AnimEvents(self->Cielo);
	} else {
		char anim[MAX_RUTA];
		Ruta_Format(anim, self->EscCielo->name, "&U&D&N.Anim00.CM3");
		byte cm3 = File_Exists(anim);
		Ruta_Format(anim, self->EscCielo->name, "&U&D&N.Anim00&E");
		if (File_Exists(anim) || cm3) {
			Crear_Animacion(self->Cielo, anim, 2);
			Mundo_AnimEvents(self->Cielo);
		}
	}
	if (!CAnimGrupos_Count(&self->Cielo->anims)) {
		CEscena_Join(self->EscCielo, 1, NULL);
	}
	CEscena_ConvertD3D(self->EscCielo, 0);
	CEscena_PrecalcIlum(self->EscCielo, 1, 0);
	CMundo_SetLucesCielo(self, 0);
	if (CAnimGrupos_Count(&self->Cielo->anims)) {
		CModelo_SetAnim(self->Cielo, 0, 0);
	}
	if (!self->EscCielo->fromAmc) {
		SM3_Save(name, self->EscCielo);
	}
}

void __thiscall CMundo_LoadSuelo(CMundo *self, const char *name) {
	vec3_t escala = { 1.0f, 1.0f, 1.0f };
	CMundo_LoadSueloEscala(self, name, &escala);
}

/* the references to a node beyond the ones of its neighbours in the list */
static int Nodo_RefsFuera(const CNodo3D *n) {
	return n->Refs - (n->ant != NULL) - (n->sig != NULL);
}

/* the DM_ nodes of the floor scene made dummies, out of the scene */
static void Mundo_TakeDummies(CMundo *self) {
	CNodo3D *next;
	for (CNodo3D *n = self->EscSuelo->Nodo3D; n; n = next) {
		next = n->sig;
		int refs = Nodo_RefsFuera(n);
		if (!crt_strnicmp(n->name, "DM_", 3)) {
			if (!refs && !CNodo3D_Clase(n)) {
				void *p = operator_new(sizeof(MundoDummy));
				CMundo_AddDummy(self, p ? MundoDummy_ctorNodo(p, n) : NULL);
				CEscena_RemoveNodo(self->EscSuelo, n);
				((objDelete_t)n->vftable[0])(n, 1);
			} else {
				Con_FatalErrorf("Only non-ref dummies can be named DM_*\n\nNode: %s\n", n->name);
			}
		} else if (!CNodo3D_Clase(n) && !refs) {
			E3D_LogInfo(g_FileLog, "WARN: Node NOT set as World Dummy: %s", n->name);
		}
	}
}

/* the lights of the floor scene get their sector (the .sm3 has it already) */
static void Mundo_LucesSector(CMundo *self) {
	int luces = 0, asignadas = 0;
	for (CNodo3D *n = self->EscSuelo->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_LUZ) {
			continue;
		}
		luces++;
		CLuz *luz = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);
		int sector = Sector_OfNode(n);
		if (!self->EscSuelo->fromAmc) {
			CHECK(luz->sector < 0);
			luz->sector = sector;
		} else {
			CHECK(luz->sector == sector);
		}
		if (sector >= 0) {
			asignadas++;
			E3D_LogWhite(g_FileLog, "Light sector %d: %s", sector, luz->nodo->name);
		}
	}
	Con_Verbosef("World lights: %d (asigned: %d, unasigned: %d)\n", luces, asignadas, luces - asignadas);
}

/* light_vertex_mult of the FileINI of the floor scene, the multiplier of each light by its name */
static void Mundo_LucesMult(CMundo *self) {
	CEscena *esc = self->EscSuelo;
	if (!esc->ini) {
		return;
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_LUZ) {
			continue;
		}
		CLuz *luz = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);
		Con_Verbosef("light: '%s' (vertex_mult: %f)\n", n->name, (double)luz->mult);
		float k = CArchINI_GetFloat(self->EscSuelo->ini, "light_vertex_mult", n->name, 1.0f);
		if (k != 1.0f) {
			Con_Warnf(" light_vertex_mult: %f -> %f [%s]\n", (double)luz->mult, (double)(luz->mult * k), n->name);
			luz->mult *= k;
		}
	}
}

void __thiscall CMundo_LoadSueloEscala(CMundo *self, const char *name, const vec3_t *escala) {
	CHECK(!self->ini_D3D);
	CHECK(!self->Suelo);
	if (!name || !*name) {
		return;
	}
	byte sombras = g_MapaSombrasUsar && !g_MapaSombrasCalcular && !g_MapaSombrasDlg;
	self->Suelo = Crear_Modelo3(name, NULL, escala, sombras ? ESCENA_SM3 | ESCENA_USARANIM : ESCENA_USARANIM);
	Mundo_AnimEvents(self->Suelo);
	CModelo_SetAnim(self->Suelo, 0, 0);
	self->EscSuelo = self->Suelo->escena;
	CHECK(!self->EscSuelo->nodo);
	CMundo_SetCamara(self, (CCamara *)CNodo3D_FindCamara(self->EscSuelo->Nodo3D, NULL));
	Sectores_Crear(self->EscSuelo);
	CEscena_ToSuelo(self->EscSuelo);
	self->ObjSuelo = Obj3D_Cast(CEscena_FindClase(self->EscSuelo, NULL, CLASE_OBJ3D_SUELO)->obj3D, CLASE_OBJ3D_SUELO);
	CHECK(self->ObjSuelo);
	for (CNodo3D *n = self->EscSuelo->Nodo3D; n; n = n->sig) {
		if (!CNodo3D_Clase(n) && !Nodo_RefsFuera(n)) {
			n->flags |= NODO_NOUPDATE | NODO_BORRAR;
		}
	}
	Mundo_TakeDummies(self);
	if (self->EscSuelo->fromAmc) {
		Mundo_LoadDUM(name, &self->dummies);
	} else {
		Mundo_SaveDUM(name, self->dummies);
	}
	Con_Verbosef("World Dummies: %d\n", MundoDummy_Count(self->dummies));
	CMundo_LoadMoreDummies(self, "MoreDummies.ini");
	Mundo_LucesSector(self);
	if (sombras) {
		CSuelo_FreeNodos(self->ObjSuelo);
	}
	CEscena_ConvertD3D(self->EscSuelo, 0);
	if (g_WorldPreIllum) {
		CEscena_PrecalcIlum(self->EscSuelo, g_WorldPreIllum == 2, 0);
	}
	CMundo_SetLucesSuelo(self, 0);
	Bsp_Crear();
	BspQ3_Crear();
	if (!self->EscSuelo->fromAmc) {
		SM3_Save(name, self->EscSuelo);
	}
	Mundo_LucesMult(self);
}

CCamara *__thiscall CMundo_GetCamaraSuelo(CMundo *self, byte update) {
	CCamara *cam = (CCamara *)CNodo3D_FindCamara(self->EscSuelo->Nodo3D, NULL);
	if (cam && update) {
		if (cam->mode == CAMARA_LOOKAT) {
			CCamara_LookAtRot(cam);
		}
		CCamara_UpdateVectors(cam);
	}
	return cam;
}

CNodo3D *__thiscall CMundo_FindClase(CMundo *self, CNodo3D *from, uint32_t clase) {
	return self->EscSuelo ? CEscena_FindClase(self->EscSuelo, from, clase) : NULL;
}

CNodo3D *__thiscall CMundo_FindNodo(CMundo *self, const char *name) {
	return self->Suelo ? CModelo_FindNodo(self->Suelo, name) : NULL;
}

CNodo3D *__thiscall CMundo_FindNodoWild(CMundo *self, const char *pat) {
	return self->Suelo ? CModelo_FindNodoWild(self->Suelo, pat) : NULL;
}

/* the debug points */
typedef struct DebugPunto {
	vec3_t pos;			/* 0x00 */
	uint32_t color;			/* 0x0c */
	float tipo;			/* 0x10 see Debug_Punto */
} DebugPunto;
SD_SIZE(DebugPunto, 0x14);

#define g_Puntos (GAME_GLOBAL(CTabla, g_Puntos, 0x4afec4))	/* DebugPunto[], grown by a quarter */
#define g_NumPuntos GAME_GLOBAL(int, g_NumPuntos, 0x4afecc)
#define g_PuntosOn GAME_GLOBAL_IN(byte, group_4a8ef8, 2, 0x4a8efa)	/* set by the first point */
#define g_MaxPuntos GAME_GLOBAL(int, g_MaxPuntos, 0x432584)	/* 4000: all cleared at that count, 0 no limit */

static DebugPunto *Punto(int i) {
	TABLA_CHECK(g_Puntos.p, i, g_Puntos.n);
	return (DebugPunto *)g_Puntos.p + i;
}

int Debug_PuntoLinea(const vec3_t *a, const vec3_t *b, uint32_t ca, uint32_t cb) {
	Debug_Punto(a, ca, -1.0f);
	Debug_Punto(b, cb, 1.0f);
	return 0;
}

int Debug_Punto(const vec3_t *pos, uint32_t color, float tipo) {
	g_PuntosOn = 1;
	if (g_MaxPuntos && g_NumPuntos >= g_MaxPuntos) {
		Debug_PuntosClear();
	}
	int n = ++g_NumPuntos;
	if (n > g_Puntos.n) {
		Tabla_Resize14(&g_Puntos, n + (n >> 2));
	}
	Punto(g_NumPuntos - 1)->pos = *pos;
	Punto(g_NumPuntos - 1)->color = color;
	Punto(g_NumPuntos - 1)->tipo = tipo;
	return 0;
}

void Debug_PuntosClear(void) {
	Tabla_Free(&g_Puntos);
	g_NumPuntos = 0;
}

int Debug_PuntosDraw(void) {
	if (!g_PuntosOn) {
		return 0;
	}
	Font_SetColor(g_FntComic14, 0xff);
	Font_Printf(g_FntComic14, 10.0f, 100.0f, 2, "puntos: %d", g_NumPuntos);
	for (int i = 0; i < g_NumPuntos; i++) {
		const DebugPunto *p = Punto(i);
		if (-1.0f > p->tipo) {
			if (++i >= g_NumPuntos) {
				break;
			}
			Debug_DrawBox(&Punto(i - 1)->pos, &Punto(i)->pos, Punto(i - 1)->color);
		} else if (0.0f > p->tipo) {
			if (++i >= g_NumPuntos) {
				break;
			}
			const DebugPunto *a = Punto(i - 1), *b = Punto(i);
			vec3_t mid = { (b->pos.x + a->pos.x) * 0.5f, (a->pos.y + b->pos.y) * 0.5f, (a->pos.z + b->pos.z) * 0.5f };
			Debug_DrawLine(&a->pos, &mid, a->color);
			Debug_DrawLine(&mid, &b->pos, b->color);
		} else {
			Debug_DrawPoint(&p->pos, p->tipo, p->color);
		}
	}
	return 0;
}
