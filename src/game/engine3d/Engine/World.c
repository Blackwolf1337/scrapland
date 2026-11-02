/*
 * Engine/mundo.cpp of the engine3d: CWorld, see include/scrap/world.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/fonts.h"
#include "scrap/readm3d.h"
#include "scrap/e3dworld.h"
#include "scrap/particlesystem.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"

#define DEG2RAD_F 0.0174532924f

/* the start of the own camera: 240 behind the origin, 50 up, looking at it */
static void World_CameraStart(CWorld *self) {
	self->camera.pos = (vec3_t){ 0.0f, 50.0f, -240.0f };
	self->cam->target = (vec3_t){ 0.0f, 0.0f, 0.0f };
}

CWorld *CWorld_ctor(CWorld *self) {
	CFog_ctor(&self->fog);
	CCamera_ctor(&self->camera);
	self->cam = &self->camera;
	self->skyPos = 1.0f;
	self->skyRot = 0.0f;
	self->Ground = NULL;
	self->GroundScale = NULL;
	self->ObjGround = NULL;
	self->Sky = NULL;
	self->SkyScale = NULL;
	self->tube = NULL;
	self->ini_D3D = 0;
	self->dummies = NULL;
	World_CameraStart(self);
	CCamera_LookAtRot(self->cam);
	CCamera_UpdateVectors(self->cam);
	return self;
}

void CWorld_dtor(CWorld *self) {
	BspQ3_Free();
	Bsp_Free();
	Sectors_FreeAll();
	Model_Destroy(&self->Ground, 0);
	Model_Destroy(&self->Sky, 0);
	WorldDummy_FreeList(&self->dummies);
	CCamera_dtor(&self->camera);
	CFog_dtor(&self->fog);
}

static void Dummy_Zero(WorldDummy *d) {
	d->name = NULL;
	d->pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	d->rot = (vec3_t){ 0.0f, 0.0f, 0.0f };
	d->next = NULL;
	d->ini = NULL;
}

/* the members the dummy owns */
static void Dummy_Free(WorldDummy *d) {
	if (d->ini) {
		CIniFile *ini = d->ini;
		CIniFile_dtor(ini);
		operator_delete_sized(ini, sizeof(CIniFile));
		d->ini = NULL;
	}
	if (d->name) {
		String_Set(&d->name, NULL);
	}
}

static void Dummy_CheckName(const WorldDummy *d, const char *warn) {
	if (crt_strnicmp(d->name, "DM_", 3)) {
		Con_Warnf(warn, d->name);
	}
}

WorldDummy *WorldDummy_ctorNode(WorldDummy *self, CNode3D *node) {
	vec3_t rot;

	Dummy_Zero(self);
	String_Set(&self->name, node->name);
	self->pos = (vec3_t){ node->m.m[12], node->m.m[13], node->m.m[14] };
	self->rot = *Matrix_ToEuler(&rot, &node->m);
	Dummy_CheckName(self, "WARN: Node set as World Dummy: %s\n");
	if (node->ini) {
		E3D_LogWhite(g_FileLog, "INFO: World Dummy has UserProp: %s", self->name);
	}
	self->ini = node->ini;
	node->ini = NULL;
	CHECK(!node->obj58);
	return self;
}

WorldDummy *WorldDummy_ctor(WorldDummy *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	Dummy_Zero(self);
	String_Set(&self->name, name);
	self->pos = *pos;
	self->rot = *rot;
	Dummy_CheckName(self, "WARN: Additional World Dummy: %s\n");
	return self;
}

WorldDummy *WorldDummy_ctorEmpty(WorldDummy *self) {
	Dummy_Zero(self);
	return self;
}

void WorldDummy_dtor(WorldDummy *self) {
	CHECK(self->next == NULL);
	Dummy_Free(self);
}

int WorldDummy_Count(const WorldDummy *list) {
	int n = 0;
	for (; list; list = list->next) {
		n++;
	}
	return n;
}

void WorldDummy_FreeList(WorldDummy **list) {
	while (*list) {
		WorldDummy *d = *list;
		*list = d->next;
		d->next = NULL;
		Dummy_Free(d);
		operator_delete_sized(d, sizeof(WorldDummy));
	}
}

WorldDummy *WorldDummy_Find(WorldDummy *self, const char *name, byte self_too, byte case_sens) {
	if (!name) {
		return NULL;
	}
	for (WorldDummy *d = self_too ? self : self->next; d; d = d->next) {
		if (!(case_sens ? strcmp(name, d->name) : crt_stricmp(name, d->name))) {
			return d;
		}
	}
	return NULL;
}

void CWorld_AddDummy(CWorld *self, WorldDummy *d) {
	CHECK(!d->next);
	if (self->dummies && d->name) {
		for (WorldDummy *e = self->dummies; e; e = e->next) {
			if (!crt_stricmp(d->name, e->name)) {
				Con_Warnf("WARN: duplicated World Dummy: %s\n", d->name);
				break;
			}
		}
	}
	WorldDummy **slot = &self->dummies;
	while (*slot) {
		slot = &(*slot)->next;
	}
	*slot = d;
}

void CWorld_ReleaseD3D(CWorld *self) {
	if (!(g_pApp->flags >> 2 & 1)) {
		CHECK(self->ini_D3D);
		CHECK(g_Engine->device);
	}
	self->ini_D3D = 0;
	void *dev = g_Engine->device;
	((IObject3D *)self->cam)->vftable->ReleaseD3D((IObject3D *)self->cam, dev);
	if (self->Sky) {
		CModel_ReleaseD3D(self->Sky, dev, 1);
	}
	if (self->Ground) {
		CModel_ReleaseD3D(self->Ground, dev, 1);
	}
	BspQ3_ReleaseD3D();
	Bsp_ReleaseD3D();
}

void CWorld_InitD3D(CWorld *self) {
	CHECK(!self->ini_D3D);
	self->ini_D3D = 1;
	void *dev = g_Engine->device;
	CHECK(dev);
	((IObject3D *)self->cam)->vftable->InitD3D((IObject3D *)self->cam, dev);
	if (self->Sky) {
		CModel_InitD3D(self->Sky, dev, 1);
	}
	if (self->Ground) {
		CModel_InitD3D(self->Ground, dev, 1);
	}
	CFog_InitD3D(&self->fog);
	Bsp_InitD3D();
	BspQ3_InitD3D();
}

int World_LoadDUM(const char *path, WorldDummy **list) {
	char name[MAX_FILEPATH];

	Path_Format(name, path, "&U&D&N.dum");
	if (!File_Exists(name)) {
		Con_FatalErrorf("File not found: %s", name);
		return -1;
	}
	void *mem = operator_new(sizeof(CFileIO));
	CFileIO *f = mem ? CFileIO_ctor(mem, name, "rb") : NULL;
	CHECK(!f->error);
	E3D_LogDebug2(g_FileLog, "Load_DUM: %s", name);
	CFileIO_BeginBlock(f, "DUM");
	CHECK(CFileIO_ReadInt(f) == 1);
	int count = CFileIO_ReadInt(f);
	*list = NULL;
	WorldDummy *last = NULL;
	while (CFileIO_ReadInt(f)) {
		void *p = operator_new(sizeof(WorldDummy));
		WorldDummy *d = p ? WorldDummy_ctorEmpty(p) : NULL;
		String_Read(&d->name, f);
		CFileIO_Read(f, &d->pos, sizeof(d->pos));
		CFileIO_Read(f, &d->rot, sizeof(d->rot));
		IniFile_Read(f, &d->ini);
		*(*list ? &last->next : list) = d;
		last = d;
		count--;
	}
	CHECK(!count);
	CFileIO_Error(f, "DUM");
	CHECK(!f->error);
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(CFileIO));
	return 0;
}

int World_SaveDUM(const char *path, const WorldDummy *list) {
	char name[MAX_FILEPATH];

	Path_Format(name, path, "&U&D&N.dum");
	E3D_LogDebug2(g_FileLog, "Save_DUM: %s", name);
	void *mem = operator_new(sizeof(CFileIO));
	CFileIO *f = mem ? CFileIO_ctor(mem, name, "wb") : NULL;
	CHECK(!f->error);
	CFileIO_BeginBlockWrite(f, "DUM");
	CFileIO_WriteInt(f, 1);
	CFileIO_WriteInt(f, WorldDummy_Count(list));
	for (const WorldDummy *d = list; d; d = d->next) {
		CFileIO_WriteInt(f, 1);
		String_Write((char **)&d->name, f);
		CFileIO_Write(f, &d->pos, sizeof(d->pos));
		CFileIO_Write(f, &d->rot, sizeof(d->rot));
		IniFile_Write(f, d->ini);
	}
	CFileIO_WriteInt(f, 0);
	CFileIO_EndBlockWrite(f, "DUM");
	CHECK(!f->error);
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(CFileIO));
	return 0;
}

byte CWorld_LoadMoreDummies(CWorld *self, const char *ini) {
	char name[MAX_FILEPATH];
	char sect[0x80];
	CIniFile f;

	Path_Merge(name, ini, self->GroundScale->name);
	CIniFile_ctorLoad(&f, name, NULL);
	int n = CIniFile_Count(&f);
	int added = 0;
	for (int i = 1; i <= n; i++) {
		if (!CIniFile_GetSectionName(&f, i, sect, sizeof(sect))) {
			continue;
		}
		vec3_t tmp, def = { 0.0f, 0.0f, 0.0f };
		vec3_t pos = *CIniFile_GetVec3(&f, &tmp, sect, "Pos", &def);
		vec3_t def2 = { 0.0f, 0.0f, 0.0f };
		vec3_t rot = *CIniFile_GetVec3(&f, &tmp, sect, "Rot", &def2);
		/* in the coordinates of 3ds max, for a script that makes them there */
		Con_Debugf("Dummy boxsize:[100,100,100] pos:[%f,%f,%f] isSelected:on name:\"%s\" rotation:(eulerAngles 0 0 %f) \n",
			(double)pos.x, (double)pos.z, (double)pos.y, sect, (double)-rot.y);
		void *p = operator_new(sizeof(WorldDummy));
		WorldDummy *d = NULL;
		if (p) {
			vec3_t r = { rot.x * DEG2RAD_F, rot.y * DEG2RAD_F, rot.z * DEG2RAD_F };
			d = WorldDummy_ctor(p, sect, &pos, &r);
		}
		CWorld_AddDummy(self, d);
		added++;
	}
	Con_Verbosef("Additional World Dummies: %d\n", added);
	CIniFile_dtor(&f);
	return 1;
}

static void D3D_SetRenderState(void *dev, uint32_t state, uint32_t value) {
	Rd_DevSetRenderState(dev, state, value);
}

static void D3D_SetProj(const matrix_t *m) {
	Rd_DevSetTransform(g_pd3dDevice, D3DTS_PROJECTION, m);
}

/* sky and floor: not lit by the lights of the level, never culled */
static void World_ModelFixed(CModel *m) {
	m->dynLight = 0;
	m->alwaysVisible = 1;
}

static void World_RenderSky(CWorld *self, void *dev) {
	int pass = g_RenderPass;
	if (pass == 11) {
		g_RenderPass = 1;
	}
	E3D_FlushBatch();
	if (!g_SkyZBuffer) {
		D3D_SetRenderState(dev, D3DRS_ZENABLE, 0);
	}
	D3D_SetRenderState(dev, D3DRS_AMBIENT, Color_ToD3D(&self->SkyScale->ambient));
	int fog = g_FogActive;
	g_FogActive = 0;
	CModel *sky = self->Sky;
	World_ModelFixed(sky);
	const CCamera *cam = self->cam;
	float k = self->skyPos;
	sky->pos = (vec3_t){ cam->pos.x * k * g_SkyPosScale, cam->pos.y * k * g_SkyPosScale,
		cam->pos.z * k * g_SkyPosScale };
	sky->rot.y = (float)((double)g_SkyRotation * M_PI_D / DEG_PER_PI) + self->skyRot;
	CModel_UpdateAnims(sky, 0.0f, -1);
	if (Portals_SetView(g_PaintAll)) {
		CModel_Render(self->Sky);
	}
	E3D_FlushBatch();
	if (g_SkyZBuffer) {
		Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
	}
	Portals_SetView(NULL);
	g_FogActive = fog;
	D3D_SetRenderState(dev, D3DRS_AMBIENT, g_AmbientD3D);
	if (!g_SkyZBuffer) {
		D3D_SetRenderState(dev, D3DRS_ZENABLE, g_ZEnable);
	}
	if (self->tube) {
		matrix_t m = *Render_GetProj();
		float f = g_SkyTubeTest;
		m.m[11] = 1.0f;
		m.m[10] = f / (f - 1.0f);
		m.m[14] = -m.m[10];
		D3D_SetProj(&m);
		CModel_Enable(self->tube);
		CModel_Render(self->tube);
		CModel_Disable(self->tube);
		m = *Render_GetProj();
		D3D_SetProj(&m);
		Render_Clear(NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
	}
	g_RenderPass = pass;
}

void CWorld_Render(CWorld *self) {
	CHECK(self->ini_D3D);
	void *dev = g_Engine->device;
	CHECK(dev);
	g_FogActive = 0;
	if (g_ModelsOn) {
		CFog_Apply(&self->fog);
	}
	if (self->Sky && g_RenderSky && g_PaintNum) {
		World_RenderSky(self, dev);
	}
	if (self->Ground && g_RenderWorld) {
		World_ModelFixed(self->Ground);
		CModel_UpdateAnims(self->Ground, 0.0f, -1);
		CModel_Render(self->Ground);
	}
	Bsp_Render();
	BspQ3_Render();
	CWorld_DrawDummies(self);
}

void CWorld_DrawDummies(CWorld *self) {
	if (!*g_ShowWorldDummies) {
		return;
	}
	int shown = 0, total = 0;
	for (WorldDummy *d = self->dummies; d; d = d->next, total++) {
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
		Debug_Text3D(g_FntTerm8, 0.0f, 0.0f, &d->pos, d->ini ? 0xffff : 0xffffff, "%s", d->name);
		shown++;
	}
	Font_PrintfXY(g_FntComic14, 10.0f, 260.0f, "world dummies: %d / %d", shown, total);
}

void CWorld_SetSkyPos(CWorld *self, float pct) {
	self->skyPos = 1.0f - pct / 100.0f;
}

void CWorld_SetSkyRot(CWorld *self, float deg) {
	self->skyRot = (float)((double)deg * M_PI_D / DEG_PER_PI);
}

/* the lights of esc but the ones of the nodes with a flag of skip */
static void World_SetLights(CScene *scn, uint32_t skip, byte active) {
	if (!scn) {
		return;
	}
	for (CNode3D *n = CScene_FindClass(scn, NULL, CLASS_OBJ3D_LIGHT); n; n = CScene_FindClass(scn, n, CLASS_OBJ3D_LIGHT)) {
		if (!(n->flags & skip)) {
			CLight_SetActive(Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT), active);
		}
	}
}

void CWorld_SetSkyLights(CWorld *self, byte active) {
	World_SetLights(self->SkyScale, 0, active);
}

void CWorld_SetGroundLights(CWorld *self, byte active) {
	World_SetLights(self->GroundScale, 0x6000000, active);
}

void CWorld_SetClip(CWorld *self, float zNear, float zFar) {
	if (self->GroundScale) {
		int a, b;
		memcpy(&a, &zNear, sizeof(a));
		memcpy(&b, &zFar, sizeof(b));
		CScene_Msg(self->GroundScale, SCENE_MSG_CAMERAS, a, b, 0, 0);
	}
	if (zNear != 0.0f) {
		self->camera.zNear = zNear;
	}
	if (zFar != 0.0f) {
		self->camera.zFar = zFar;
	}
}

void CWorld_SetCamera(CWorld *self, CCamera *cam) {
	self->cam = cam;
	if (!cam) {
		self->cam = &self->camera;
		World_CameraStart(self);
		CCamera_LookAtRot(self->cam);
	} else if (cam->mode == CAMERA_LOOKAT) {
		CCamera_LookAtRot(cam);
	}
	CCamera_UpdateVectors(self->cam);
}

void CWorld_LoadSky(CWorld *self, const char *name) {
	vec3_t scale = { 1.0f, 1.0f, 1.0f };
	CWorld_LoadSkyScale(self, name, &scale);
}

static void World_AnimEvents(CModel *m) {
	CModel_AddAnimEvents(m, CModel_GetAnimName(m, 0), -1.0f, -1.0f);
}

void CWorld_LoadSkyScale(CWorld *self, const char *name, const vec3_t *scale) {
	CHECK(!self->ini_D3D);
	CHECK(!self->Sky);
	if (!g_SkyInit || !name || !*name) {
		return;
	}
	self->Sky = Create_Model3(name, NULL, scale, SCENE_SM3 | SCENE_USEANIM);
	self->SkyScale = self->Sky->scene;
	CHECK(self->SkyScale);
	if (CAnimGroups_Count(&self->Sky->anims)) {
		CHECK(self->SkyScale->AnimM3D);
		World_AnimEvents(self->Sky);
	} else {
		char anim[MAX_FILEPATH];
		Path_Format(anim, self->SkyScale->name, "&U&D&N.Anim00.CM3");
		byte cm3 = File_Exists(anim);
		Path_Format(anim, self->SkyScale->name, "&U&D&N.Anim00&E");
		if (File_Exists(anim) || cm3) {
			Create_Animation(self->Sky, anim, 2);
			World_AnimEvents(self->Sky);
		}
	}
	if (!CAnimGroups_Count(&self->Sky->anims)) {
		CScene_Join(self->SkyScale, 1, NULL);
	}
	CScene_ConvertD3D(self->SkyScale, 0);
	CScene_PrecalcIllum(self->SkyScale, 1, 0);
	CWorld_SetSkyLights(self, 0);
	if (CAnimGroups_Count(&self->Sky->anims)) {
		CModel_SetAnim(self->Sky, 0, 0);
	}
	if (!self->SkyScale->fromAmc) {
		SM3_Save(name, self->SkyScale);
	}
}

/* the references to a node beyond the ones of its neighbours in the list */
static int Node_RefsOutside(const CNode3D *n) {
	return n->Refs - (n->prev != NULL) - (n->next != NULL);
}

/* the DM_ nodes of the floor scene made dummies, out of the scene */
static void World_TakeDummies(CWorld *self) {
	CNode3D *next;
	for (CNode3D *n = self->GroundScale->Node3D; n; n = next) {
		next = n->next;
		int refs = Node_RefsOutside(n);
		if (!crt_strnicmp(n->name, "DM_", 3)) {
			if (!refs && !CNode3D_Class(n)) {
				void *p = operator_new(sizeof(WorldDummy));
				CWorld_AddDummy(self, p ? WorldDummy_ctorNode(p, n) : NULL);
				CScene_RemoveNode(self->GroundScale, n);
				((objDelete_t)n->vftable[0])(n, 1);
			} else {
				Con_FatalErrorf("Only non-ref dummies can be named DM_*\n\nNode: %s\n", n->name);
			}
		} else if (!CNode3D_Class(n) && !refs) {
			E3D_LogInfo(g_FileLog, "WARN: Node NOT set as World Dummy: %s", n->name);
		}
	}
}

/* the lights of the floor scene get their sector (the .sm3 has it already) */
static void World_LightsSector(CWorld *self) {
	int lights = 0, assigned = 0;
	for (CNode3D *n = self->GroundScale->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) != CLASS_OBJ3D_LIGHT) {
			continue;
		}
		lights++;
		CLight *light = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);
		int sector = Sector_OfNode(n);
		if (!self->GroundScale->fromAmc) {
			CHECK(light->sector < 0);
			light->sector = sector;
		} else {
			CHECK(light->sector == sector);
		}
		if (sector >= 0) {
			assigned++;
			E3D_LogWhite(g_FileLog, "Light sector %d: %s", sector, light->node->name);
		}
	}
	Con_Verbosef("World lights: %d (asigned: %d, unasigned: %d)\n", lights, assigned, lights - assigned);
}

/* light_vertex_mult of the FileINI of the floor scene, the multiplier of each light by its name */
static void World_LightsMult(CWorld *self) {
	CScene *scn = self->GroundScale;
	if (!scn->ini) {
		return;
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (CNode3D_Class(n) != CLASS_OBJ3D_LIGHT) {
			continue;
		}
		CLight *light = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);
		Con_Verbosef("light: '%s' (vertex_mult: %f)\n", n->name, (double)light->mult);
		float k = CIniFile_GetFloat(self->GroundScale->ini, "light_vertex_mult", n->name, 1.0f);
		if (k != 1.0f) {
			Con_Warnf(" light_vertex_mult: %f -> %f [%s]\n", (double)light->mult, (double)(light->mult * k), n->name);
			light->mult *= k;
		}
	}
}

void CWorld_LoadGroundScale(CWorld *self, const char *name, const vec3_t *scale) {
	CHECK(!self->ini_D3D);
	CHECK(!self->Ground);
	if (!name || !*name) {
		return;
	}
	byte shadows = g_ShadowMapUse && !g_ShadowMapCalculate && !g_ShadowMapDlg;
	self->Ground = Create_Model3(name, NULL, scale, shadows ? SCENE_SM3 | SCENE_USEANIM : SCENE_USEANIM);
	World_AnimEvents(self->Ground);
	CModel_SetAnim(self->Ground, 0, 0);
	self->GroundScale = self->Ground->scene;
	CHECK(!self->GroundScale->node);
	CWorld_SetCamera(self, (CCamera *)CNode3D_FindCamera(self->GroundScale->Node3D, NULL));
	Sectors_Create(self->GroundScale);
	CScene_ToGround(self->GroundScale);
	self->ObjGround = Obj3D_Cast(CScene_FindClass(self->GroundScale, NULL, CLASS_OBJ3D_GROUND)->obj3D, CLASS_OBJ3D_GROUND);
	CHECK(self->ObjGround);
	for (CNode3D *n = self->GroundScale->Node3D; n; n = n->next) {
		if (!CNode3D_Class(n) && !Node_RefsOutside(n)) {
			n->flags |= NODE_NOUPDATE | NODE_DELETE;
		}
	}
	World_TakeDummies(self);
	if (self->GroundScale->fromAmc) {
		World_LoadDUM(name, &self->dummies);
	} else {
		World_SaveDUM(name, self->dummies);
	}
	Con_Verbosef("World Dummies: %d\n", WorldDummy_Count(self->dummies));
	CWorld_LoadMoreDummies(self, "MoreDummies.ini");
	World_LightsSector(self);
	if (shadows) {
		CGround_FreeNodes(self->ObjGround);
	}
	CScene_ConvertD3D(self->GroundScale, 0);
	if (g_WorldPreIllum) {
		CScene_PrecalcIllum(self->GroundScale, g_WorldPreIllum == 2, 0);
	}
	CWorld_SetGroundLights(self, 0);
	Bsp_Create();
	BspQ3_Create();
	if (!self->GroundScale->fromAmc) {
		SM3_Save(name, self->GroundScale);
	}
	World_LightsMult(self);
}

CCamera *CWorld_GetCameraGround(CWorld *self, byte update) {
	CCamera *cam = (CCamera *)CNode3D_FindCamera(self->GroundScale->Node3D, NULL);
	if (cam && update) {
		if (cam->mode == CAMERA_LOOKAT) {
			CCamera_LookAtRot(cam);
		}
		CCamera_UpdateVectors(cam);
	}
	return cam;
}

CNode3D *CWorld_FindClass(CWorld *self, CNode3D *from, uint32_t class) {
	return self->GroundScale ? CScene_FindClass(self->GroundScale, from, class) : NULL;
}

CNode3D *CWorld_FindNode(CWorld *self, const char *name) {
	return self->Ground ? CModel_FindNode(self->Ground, name) : NULL;
}

CNode3D *CWorld_FindNodeWild(CWorld *self, const char *pat) {
	return self->Ground ? CModel_FindNodeWild(self->Ground, pat) : NULL;
}

/* the debug points */
typedef struct DebugPoint {
	vec3_t pos;			/* 0x00 */
	uint32_t color;			/* 0x0c */
	float type;			/* 0x10 see Debug_Point */
} DebugPoint;
SD_SIZE(DebugPoint, 0x14);

extern CTable g_Points;	/* DebugPoint[], grown by a quarter */
extern int g_NumPoints;
#define g_PointsOn GAME_GLOBAL_IN(byte, group_4a8ef8, 2)	/* set by the first point */
extern int g_MaxPoints;	/* 4000: all cleared at that count, 0 no limit */

static DebugPoint *Point(int i) {
	TABLE_CHECK(g_Points.p, i, g_Points.n);
	return (DebugPoint *)g_Points.p + i;
}

int Debug_PointLine(const vec3_t *a, const vec3_t *b, uint32_t ca, uint32_t cb) {
	Debug_Point(a, ca, -1.0f);
	Debug_Point(b, cb, 1.0f);
	return 0;
}

int Debug_Point(const vec3_t *pos, uint32_t color, float type) {
	g_PointsOn = 1;
	if (g_MaxPoints && g_NumPoints >= g_MaxPoints) {
		Debug_PointsClear();
	}
	int n = ++g_NumPoints;
	if (n > g_Points.n) {
		Table_Resize14(&g_Points, n + (n >> 2));
	}
	Point(g_NumPoints - 1)->pos = *pos;
	Point(g_NumPoints - 1)->color = color;
	Point(g_NumPoints - 1)->type = type;
	return 0;
}

void Debug_PointsClear(void) {
	Table_Free(&g_Points);
	g_NumPoints = 0;
}

int Debug_PointsDraw(void) {
	if (!g_PointsOn) {
		return 0;
	}
	Font_SetColor(g_FntComic14, 0xff);
	Font_Printf(g_FntComic14, 10.0f, 100.0f, 2, "puntos: %d", g_NumPoints);
	for (int i = 0; i < g_NumPoints; i++) {
		const DebugPoint *p = Point(i);
		if (-1.0f > p->type) {
			if (++i >= g_NumPoints) {
				break;
			}
			Debug_DrawBox(&Point(i - 1)->pos, &Point(i)->pos, Point(i - 1)->color);
		} else if (0.0f > p->type) {
			if (++i >= g_NumPoints) {
				break;
			}
			const DebugPoint *a = Point(i - 1), *b = Point(i);
			vec3_t mid = { (b->pos.x + a->pos.x) * 0.5f, (a->pos.y + b->pos.y) * 0.5f, (a->pos.z + b->pos.z) * 0.5f };
			Debug_DrawLine(&a->pos, &mid, a->color);
			Debug_DrawLine(&mid, &b->pos, b->color);
		} else {
			Debug_DrawPoint(&p->pos, p->type, p->color);
		}
	}
	return 0;
}
