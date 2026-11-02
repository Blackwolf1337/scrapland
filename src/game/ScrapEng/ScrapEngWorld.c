/*
 * ScrapEngWorld.cpp of the game and the code after it up to ScrapEngMain.cpp, see include/scrap/scrapworld.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/archio.h"
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/groundportal.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"

/* the methods of IDirect3DDevice8, IDirect3DSurface8 and IDirect3D8 only this file calls */
#define D3DDEVTYPE_HAL 1
#define D3D_SDK_VERSION 0xdc

/* D3DSURFACE_DESC as words */
enum { SURFDESC_FORMAT = 0, SURFDESC_WIDTH = 6, SURFDESC_HEIGHT = 7, SURFDESC_WORDS = 8 };

/* the d3d8 import of the exe (user32 and kernel32 in win32.h) */

#define VK_RETURN 0x0d
#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f

/* the world node info */

void WorldInfo_FromDummy(WorldNodeInfo *info, const WorldDummy *dummy) {
	matrix_t m;

	if (!info || !dummy) {
		return;
	}
	if (strlen(dummy->name) >= sizeof(info->Name)) {
		CheckFailed("strlen( dummy->Name ) < sizeof( info->Name )", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Str_Copy(info->Name, dummy->name, sizeof(info->Name));
	Matrix_LoadIdentity(&m);
	Matrix_PostRotate(&m, &dummy->rot);
	Matrix_AddPos(&m, &dummy->pos);
	Matrix_Copy(&info->m, Matrix_Self(&m));
	info->pos = dummy->pos;
	info->dummy = 1;
}

void WorldInfo_FromNode(WorldNodeInfo *info, CNode3D *node) {
	if (!info || !node) {
		return;
	}
	if (strlen(node->name) >= sizeof(info->Name)) {
		CheckFailed("strlen( node->Nombre ) < sizeof( info->Name )", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Str_Copy(info->Name, node->name, sizeof(info->Name));
	Matrix_Copy(&info->m, Matrix_Self(&node->m));
	info->pos.x = info->m.m[12];
	info->pos.y = info->m.m[13];
	info->pos.z = info->m.m[14];
	info->dummy = CNode3D_Class(node) == 0;
}

box_t *World_GetBox(box_t *out) {
	*out = World_Get()->GroundScale->box;
	return out;
}

box_t *World_GetCollideBox(box_t *out) {
	box_t box;

	CGround_GetCollideExt(World_Ground(), &box);
	*out = box;
	return out;
}

byte Sector_Visible(int sector) {
	return Sector_IsVisible(sector);
}

CNode3D *g_WorldInfoNode;
WorldDummy *g_WorldInfoDummy;
byte g_WorldInfoWild;
char g_WorldInfoPattern[260];

void WorldInfo_Begin(const char *pattern, byte wild) {
	g_WorldInfoNode = NULL;
	g_WorldInfoDummy = NULL;
	Str_Copy(g_WorldInfoPattern, pattern ? pattern : "", MAX_FILEPATH);
	if (wild) {
		crt_strlwr(g_WorldInfoPattern);
	}
	g_WorldInfoWild = wild;
}

byte WorldInfo_Next(WorldNodeInfo *info) {
	if (WorldInfo_NextDummy(info)) {
		return 1;
	}
	if (WorldInfo_NextNode(info)) {
		CheckFailed("!_GetWorldNodeInfoNext( info )", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return 0;
}

byte WorldInfo_HasUserProp(void) {
	CHECK(g_WorldInfoDummy);
	return g_WorldInfoDummy->ini != NULL;
}

static CIniFile *WorldInfo_UserProp(void) {
	if (!g_WorldInfoDummy || !g_WorldInfoDummy->ini) {
		CheckFailed("DummyInfo && DummyInfo->UserProp", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return g_WorldInfoDummy->ini;
}

byte WorldInfo_GetBool(const char *sect, const char *key, byte def) {
	return CIniFile_GetBool(WorldInfo_UserProp(), sect, key, def);
}

float WorldInfo_GetFloat(const char *sect, const char *key, float def) {
	return CIniFile_GetFloat(WorldInfo_UserProp(), sect, key, def);
}

int WorldInfo_GetInt(const char *sect, const char *key, int def) {
	return CIniFile_GetInt(WorldInfo_UserProp(), sect, key, def);
}

char *WorldInfo_GetText(const char *sect, const char *key, char *dest, int size, const char *def) {
	return CIniFile_GetText(WorldInfo_UserProp(), sect, key, dest, size, def);
}

vec3_t *WorldInfo_GetVec3(vec3_t *out, const char *sect, const char *key, const vec3_t *def) {
	CIniFile_GetVec3(WorldInfo_UserProp(), out, sect, key, def);
	return out;
}

int Sector_At(const vec3_t *pos) {
	int sector = -2;

	Sector_FindPos(pos, &sector, NULL);
	return sector;
}

matrix_t *Matrix_LoadIdentity(matrix_t *m) {
	float *f = m->m;

	f[1] = f[2] = f[3] = f[4] = 0.0f;
	f[6] = f[7] = f[8] = f[9] = 0.0f;
	f[11] = f[12] = f[13] = f[14] = 0.0f;
	f[0] = f[5] = f[10] = f[15] = 1.0f;
	return m;
}

void WorldInfo_Reset(void) {
	g_WorldInfoNode = NULL;
	g_WorldInfoDummy = NULL;
	Str_Copy(g_WorldInfoPattern, "", MAX_FILEPATH);
	g_WorldInfoWild = 0;
	CHECK(g_CountSprite2D == 0);
}

/* whether name passes the pattern of the walk */
static byte WorldInfo_Match(const char *name) {
	char low[MAX_FILEPATH];

	if (!g_WorldInfoPattern[0]) {
		return 1;
	}
	if (!g_WorldInfoWild) {
		return crt_stricmp(g_WorldInfoPattern, name) == 0;
	}
	Str_Copy(low, name, sizeof(low));
	crt_strlwr(low);
	return Wild_Match(g_WorldInfoPattern, low) != 0;
}

byte WorldInfo_NextDummy(WorldNodeInfo *info) {
	do {
		g_WorldInfoDummy = g_WorldInfoDummy ? g_WorldInfoDummy->next : World_Get()->dummies;
		if (!g_WorldInfoDummy) {
			return 0;
		}
	} while (!WorldInfo_Match(g_WorldInfoDummy->name));
	WorldInfo_FromDummy(info, g_WorldInfoDummy);
	return 1;
}

byte WorldInfo_NextNode(WorldNodeInfo *info) {
	do {
		CWorld *world = World_Get();
		g_WorldInfoNode = CWorld_FindClass(world, g_WorldInfoNode, 0);
		if (!g_WorldInfoNode) {
			return 0;
		}
	} while (!WorldInfo_Match(g_WorldInfoNode->name));
	WorldInfo_FromNode(info, g_WorldInfoNode);
	return 1;
}

box_t *CGround_GetCollideExt(const CGround *self, box_t *out) {
	*out = self->CollideExt;
	return out;
}

matrix_t *Matrix_Self(matrix_t *m) {
	return m;
}

/* RenderMode and the rest */

int Bool_GetSet(byte *flag, int value, byte set) {
	byte old = *flag;

	if (set) {
		*flag = value != 0;
	}
	return old != 0;
}

byte g_TileOn;
byte g_VertexSpecular;
byte g_RenderMode100;

int RenderMode(int mode, int value, byte set) {
	static const char *const filters[3] = { "Point", "Linear", "Anisotropic" };

	switch (mode) {
	case RENDERMODE_PROFILER:
		if (set) {
			g_ProfMode = (g_ProfMode + 1) % 3;
		}
		return g_ProfMode;
	case RENDERMODE_TILE:
		g_TileOn = set ? 1 : g_TileOn;
		return g_TileOn;
	case RENDERMODE_100:
		return Bool_GetSet(&g_RenderMode100, value, set);
	case RENDERMODE_SPECULAR:
		if (set) {
			g_VertexSpecular = !g_VertexSpecular;
			Con_Verbosef("Vertex specular: %d\n", g_VertexSpecular);
		}
		return g_VertexSpecular;
	case RENDERMODE_TEXFILTER:
		if (set) {
			g_TexFilter = (g_TexFilter + 1) % 3;
			Con_Verbosef("Texture Filter Mode: %s\n", filters[g_TexFilter]);
		}
		return g_TexFilter;
	case RENDERMODE_SHADOWS:
		if (set) {
			ShadowsStencil_Set(g_ShadowsStencil == 0);
		}
		return g_ShadowsStencil;
	default:
		Con_Warnf("%sRenderMode(): Invalid RenderMode\n", set ? "Set" : "Get");
		return 0;
	}
}

void File_FreeIndex(const char *fmt, int *n) {
	char name[MAX_FILEPATH];

	for (int i = 0; i < 100; i++) {
		*n = i;
		Str_PrintfQuiet(name, sizeof(name), fmt, i);
		if (!File_Exists(name)) {
			break;
		}
	}
}

byte Render_ClearRect(float x, float y, float w, float h, byte target, uint32_t color) {
	box_t box;

	box.min.x = x / SCREEN_W;
	box.min.y = y / SCREEN_H;
	box.min.z = 0.0f;
	box.max.x = (x + w) / SCREEN_W;
	box.max.y = (y + h) / SCREEN_H;
	box.max.z = 0.0f;
	return Render_Clear(&box, (target != 0) + D3DCLEAR_ZBUFFER, color, 1.0f, 0);
}

uint32_t Color_ToD3D(const ColorIntens *c) {
	const byte *b = (const byte *)&c->rgba;
	float f[4];

	if (c->intens == 1.0f) {
		return (uint32_t)b[3] << 24 | (uint32_t)b[0] << 16 | (uint32_t)b[1] << 8 | b[2];
	}
	float s = c->intens * (1.0f / 255.0f);
	for (int i = 0; i < 4; i++) {
		f[i] = (float)b[i] * s;
	}
	return Color_FromFloat(f);
}

void Render_CopyBackBuffer(void) {
	void *img = NULL, *back = NULL;
	uint32_t desc[SURFDESC_WORDS];

	Rd_DevGetBackBuffer(g_pd3dDevice, 0, 0, &back);
	if (Rd_SurfGetDesc(back, desc) < 0) {
		return;
	}
	if (Rd_DevCreateImageSurface(g_pd3dDevice,
		desc[SURFDESC_WIDTH], desc[SURFDESC_HEIGHT], (int)desc[SURFDESC_FORMAT], &img) < 0) {
		return;
	}
	Rd_DevPresent(g_pd3dDevice, NULL, NULL, NULL, NULL);
	Rd_DevCopyRects(g_pd3dDevice, back, NULL, 0, img, NULL);
	if (back) {
		Rd_Release(back);
		back = NULL;
	}
	Rd_DevPresent(g_pd3dDevice, NULL, NULL, NULL, NULL);
	Rd_DevGetBackBuffer(g_pd3dDevice, 0, 0, &back);
	Rd_DevCopyRects(g_pd3dDevice, img, NULL, 0, back, NULL);
	if (back) {
		Rd_Release(back);
		back = NULL;
	}
	if (img) {
		Rd_Release(img);
	}
}

/* the splash screens */

void Splash_Black(void) {
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
}

void Splash_Promos(void) {
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
	if (g_IsDemo) {
		Splash_Show("2D/Loading/00Promo01.dds", 1, 6000, 1);
		if (g_IsDemo) {
			Splash_Show("2D/Loading/00Promo02.dds", 1, 20000, 1);
		}
	}
}

void Splash_Logos(void) {
	if (g_IsDemo) {
		Splash_Show("2D/Loading/00LogoEnlight.dds", 1, 4000, 1);
		if (g_IsDemo) {
			Splash_Show("2D/Loading/00LogoMSE.dds", 1, 4000, 1);
		}
	}
	Splash_Show("2D/Loading/00Legal_global.dds", 1, 0, 0);
}

/* the gray of a fade at dt of fade ms (the uint conversions of the original go through double) */
static int Splash_Gray(uint32_t dt, float fade) {
	return (int)((float)(double)dt / fade * 255.0f);
}

static uint32_t Splash_Color(int c) {
	return ((uint32_t)c << 8 | (uint32_t)c) << 8 | (uint32_t)c;
}

char g_SplashLast[260];
uint32_t g_SplashTime;

void Splash_Show(const char *name, byte fadeIn, uint32_t waitMs, byte fadeOut) {
	char path[MAX_FILEPATH];
	void *tex, *data;
	uint32_t size;

	if (!g_SplashOn || g_RenderOff) {
		return;
	}
	if (!name) {
		Loading_RandomName(path, sizeof(path));
	} else {
		Str_PrintfQuiet(path, sizeof(path), "%s", *name ? name : g_SplashLast);
	}
	for (int i = 0; (g_SplashLast[i] = path[i]) != 0; i++) {
	}
	if (!File_Exists(path)) {
		Con_Verbosef("WARN: SplashScreen not found: %s\n", path);
		return;
	}
	E3D_LogPrint((&g_FileLog), "SplashScreen: %s", path);
	tex = NULL;
	FileIO_ReadAll(path, &data, &size);
	D3DX_CreateTextureFromFileInMemoryEx(g_Engine->device, data, size, 0, 0, 1, 0, 0, D3DPOOL_MANAGED, 1, 1, 0, NULL, NULL,
		&tex);
	Ptr_Delete(&data);
	const box_t rect = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
	uint32_t fade = g_SplashFade;
	if (fadeIn) {
		uint32_t start = Plat_Ticks();
		uint32_t end = start + fade;
		if (Plat_Ticks() < start + fade) {
			float fadeF = (float)(double)fade;
			do {
				Splash_Draw(&rect, Splash_Color(Splash_Gray(Plat_Ticks() - start, fadeF)), tex);
			} while (Plat_Ticks() < end);
		}
	}
	for (int i = 4; i != 0; i--) {
		Splash_Draw(&rect, 0xffffff, tex);
	}
	uint32_t wait = waitMs;
	if (name && !*name) {
		int left = (int)(g_SplashTime + waitMs - Plat_Ticks());
		wait = left < 0 ? 0 : (uint32_t)left;
	}
	g_SplashTime = Plat_Ticks();
	if (wait) {
		uint32_t end = wait + Plat_Ticks();
		while (Plat_Ticks() < end) {
			if (Plat_KeyDown(VK_RETURN) || Plat_KeyDown(VK_ESCAPE) || Plat_KeyDown(VK_SPACE)) {
				break;
			}
			/* differs from the original on purpose (no sleeps): the picture again instead of Sleep(10). The loop
			 * stays: the callers (engine start and end, level end, loading screen, device reset) go on with the
			 * splash on the screen */
			Splash_Draw(&rect, 0xffffff, tex);
		}
	}
	if (fadeOut) {
		uint32_t start = Plat_Ticks();
		uint32_t end = start + fade;
		if (Plat_Ticks() < end) {
			float fadeF = (float)(double)fade;
			do {
				Splash_Draw(&rect, Splash_Color(0xff - Splash_Gray(Plat_Ticks() - start, fadeF)), tex);
			} while (Plat_Ticks() < end);
		}
		Splash_Draw(&rect, 0, tex);
	}
	if (tex) {
		Rd_Release(tex);
	}
}

void Splash_Draw(const box_t *rect, uint32_t color, void *tex) {
	Rd_DevBeginScene(g_Engine->device);
	Render_DrawTexRect(rect, color, color, tex);
	Rd_DevEndScene(g_Engine->device);
	Rd_DevPresent(g_Engine->device, NULL, NULL, NULL, NULL);
}

void Splash_Reset(void) {
	Splash_Show("2D/Loading/00Reset.dds", 0, 0, 0);
}

/* the loading screen */

/* one frame of the loading screen drawn by draw */
static void Loading_Frame(loadingDraw_t draw) {
	Rd_DevBeginScene(g_pd3dDevice);
	Render_BeginFrame();
	draw();
	Render_EndView();
	Rd_DevEndScene(g_pd3dDevice);
	Rd_DevPresent(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Loading_DrawBack2(void) {
	if (!g_LoadingDraw2 || !g_LoadingDrawOn) {
		return;
	}
	for (int i = 2; i != 0; i--) {
		Loading_Frame(g_LoadingDraw2);
	}
}

int g_LoadingCount;

void Loading_Step(int percent) {
	box_t r;

	if (!g_LoadingOn || !g_LoadingBar) {
		Loading_DrawBack();
		return;
	}
	if (percent == -1) {
		int n = ++g_LoadingCount;
		percent = n * 100 / g_LoadingTotal;
		if (percent > 1) {
			if (percent >= 99) {
				percent = 99;
			}
		} else {
			percent = 1;
		}
		if ((n & 7) != 1) {
			return;
		}
	}
	CD3DApplication *engine = g_Engine;
	int x0 = (int)engine->backBufferDesc.Width * 0x74 / 0x500;
	if (!engine->device || !g_D3DOn) {
		return;
	}
	float p = (float)percent / 100.0f;
	int w = (int)engine->backBufferDesc.Width;
	float fw = (float)w;
	r.min.x = (float)x0 / fw;
	r.max.x = (float)(((w * 0x48c + 0x280) / 0x500 - x0) * percent / 100 + x0) / fw;
	int h = (int)engine->backBufferDesc.Height;
	r.min.y = (float)(h * 0x398 / 1024) / (float)h;
	r.min.z = 0.0f;
	r.max.y = 0.90478515625f;	/* 0x3f67a000 */
	r.max.z = 0.0f;
	Rd_DevBeginScene(engine->device);
	float q = 1.0f - p;
	int green = (int)(q * 200.0f + p * 255.0f);
	int red = (int)(q * 255.0f + p * 200.0f);
	Render_DrawTexRect(&r, 0xffc800, ((uint32_t)red << 8 | (uint32_t)green) << 8, NULL);
	Rd_DevEndScene(g_Engine->device);
	Rd_DevPresent(g_Engine->device, NULL, NULL, NULL, NULL);
}

void Loading_DrawBack(void) {
	if (g_LoadingDraw && g_LoadingDrawOn) {
		Loading_Frame(g_LoadingDraw);
	}
}

cGVar GVar_ShowProgressBar;
cGVar GVar_SplashScreenFade;
cGVar GVar_R_TexFilter;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_SplashScreenFade, &GVar_SplashScreenFade, "SplashScreenFade", 0, "Splash screens fade time", 400)
INIT_GVAR_INT(Init_GVar_ShowProgressBar, &GVar_ShowProgressBar, "ShowProgressBar", 0, "Show loading progress bar (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_TexFilter, &GVar_R_TexFilter, "R_TexFilter", 2, "Texture filter (0=point,1=linear,2=anisotropic)", 2)
