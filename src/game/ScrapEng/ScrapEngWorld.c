/*
 * ScrapEngWorld.cpp of the game and the code after it up to ScrapEngMain.cpp, see include/scrap/scrapworld.h.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/sueloportal.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

/* D3DX 8, linked into the exe (the same prototype as in Engine/Textura.c) */
int __stdcall D3DX_CreateTextureFromFileInMemoryEx(void *dev, const void *data, uint32_t size, uint32_t w, uint32_t h,
	uint32_t levels, uint32_t usage, int fmt, int pool, uint32_t filter, uint32_t mipFilter, uint32_t colorKey,
	void *srcInfo, void *palette, void **tex);

/* the methods of IDirect3DDevice8, IDirect3DSurface8 and IDirect3D8 only this file calls */
#define D3DDEV_PRESENT 0x3c
#define D3DDEV_BEGINSCENE 0x88
#define D3DDEV_ENDSCENE 0x8c
#define D3DSURF_GETDESC 0x20
#define D3D_GETADAPTERIDENTIFIER 0x14
#define D3D_GETDEVICECAPS 0x34
typedef int(__stdcall *d3dScene_t)(void *dev);
typedef int(__stdcall *d3dPresent_t)(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty);
typedef int(__stdcall *d3dCreateImageSurface_t)(void *dev, uint32_t w, uint32_t h, int fmt, void **surf);
typedef int(__stdcall *d3dCopyRects_t)(void *dev, void *src, const void *rects, uint32_t n, void *dst,
	const void *points);
typedef int(__stdcall *d3dGetDesc_t)(void *surf, uint32_t *desc);
typedef int(__stdcall *d3dGetAdapterIdentifier_t)(void *d3d, uint32_t adapter, uint32_t flags, D3DAdapterId *id);
typedef int(__stdcall *d3dGetDeviceCaps_t)(void *d3d, uint32_t adapter, int type, uint32_t *caps);
#define D3DDEVTYPE_HAL 1
#define D3D_SDK_VERSION 0xdc

/* D3DSURFACE_DESC as words */
enum { SURFDESC_FORMAT = 0, SURFDESC_WIDTH = 6, SURFDESC_HEIGHT = 7, SURFDESC_WORDS = 8 };

/* the imports of the exe this file calls (user32, kernel32, d3d8) */
typedef short(__stdcall *getasynckeystate_t)(int key);
typedef void(__stdcall *sleep_t)(uint32_t ms);
typedef int(__stdcall *bringwindowtotop_t)(void *hwnd);
typedef void *(__stdcall *direct3dcreate8_t)(uint32_t sdk);
#define exe_GetAsyncKeyState GAME_DATA(getasynckeystate_t, 0x3a1424)
#define exe_Sleep GAME_DATA(sleep_t, 0x3a118c)
#define exe_BringWindowToTop GAME_DATA(bringwindowtotop_t, 0x3a13f4)
#define exe_Direct3DCreate8 GAME_DATA(direct3dcreate8_t, 0x3a14c4)

#define VK_RETURN 0x0d
#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20

#define SCREEN_W 640.0f		/* rva 0x3c3274 */
#define SCREEN_H 480.0f		/* rva 0x3c3270 */

/* the world node info */

void WorldInfo_FromDummy(WorldNodeInfo *info, const MundoDummy *dummy) {
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

void WorldInfo_FromNode(WorldNodeInfo *info, CNodo3D *node) {
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
	info->dummy = CNodo3D_Clase(node) == 0;
}

box_t *World_GetBox(box_t *out) {
	*out = Mundo_Get()->EscSuelo->box;
	return out;
}

box_t *World_GetCollideBox(box_t *out) {
	box_t box;

	CSuelo_GetCollideExt(Mundo_Suelo(), &box);
	*out = box;
	return out;
}

byte Sector_Visible(int sector) {
	return Sector_IsVisible(sector);
}

void WorldInfo_Begin(const char *pattern, byte wild) {
	g_WorldInfoNode = NULL;
	g_WorldInfoDummy = NULL;
	Str_Copy(g_WorldInfoPattern, pattern ? pattern : "", MAX_RUTA);
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

static CArchINI *WorldInfo_UserProp(void) {
	if (!g_WorldInfoDummy || !g_WorldInfoDummy->ini) {
		CheckFailed("DummyInfo && DummyInfo->UserProp", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return g_WorldInfoDummy->ini;
}

byte WorldInfo_GetBool(const char *sect, const char *key, byte def) {
	return CArchINI_GetBool(WorldInfo_UserProp(), sect, key, def);
}

float WorldInfo_GetFloat(const char *sect, const char *key, float def) {
	return CArchINI_GetFloat(WorldInfo_UserProp(), sect, key, def);
}

int WorldInfo_GetInt(const char *sect, const char *key, int def) {
	return CArchINI_GetInt(WorldInfo_UserProp(), sect, key, def);
}

char *WorldInfo_GetText(const char *sect, const char *key, char *dest, int size, const char *def) {
	return CArchINI_GetText(WorldInfo_UserProp(), sect, key, dest, size, def);
}

vec3_t *WorldInfo_GetVec3(vec3_t *out, const char *sect, const char *key, const vec3_t *def) {
	CArchINI_GetVec3(WorldInfo_UserProp(), out, sect, key, def);
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
	Str_Copy(g_WorldInfoPattern, "", MAX_RUTA);
	g_WorldInfoWild = 0;
	CHECK(g_CountSprite2D == 0);
}

/* whether name passes the pattern of the walk */
static byte WorldInfo_Match(const char *name) {
	char low[MAX_RUTA];

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
		g_WorldInfoDummy = g_WorldInfoDummy ? g_WorldInfoDummy->next : Mundo_Get()->dummies;
		if (!g_WorldInfoDummy) {
			return 0;
		}
	} while (!WorldInfo_Match(g_WorldInfoDummy->name));
	WorldInfo_FromDummy(info, g_WorldInfoDummy);
	return 1;
}

byte WorldInfo_NextNode(WorldNodeInfo *info) {
	do {
		CMundo *mundo = Mundo_Get();
		g_WorldInfoNode = CMundo_FindClase(mundo, g_WorldInfoNode, 0);
		if (!g_WorldInfoNode) {
			return 0;
		}
	} while (!WorldInfo_Match(g_WorldInfoNode->name));
	WorldInfo_FromNode(info, g_WorldInfoNode);
	return 1;
}

box_t *__thiscall CSuelo_GetCollideExt(const CSuelo *self, box_t *out) {
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
			SombrasStencil_Set(g_SombrasStencil == 0);
		}
		return g_SombrasStencil;
	default:
		Con_Warnf("%sRenderMode(): Invalid RenderMode\n", set ? "Set" : "Get");
		return 0;
	}
}

void File_FreeIndex(const char *fmt, int *n) {
	char name[MAX_RUTA];

	for (int i = 0; i < 100; i++) {
		*n = i;
		Str_PrintfQuiet(name, sizeof(name), fmt, i);
		if (!File_Exists(name)) {
			break;
		}
	}
}

/* src appended to dst of size bytes, cut to fit (the original appends unbounded) */
static void Str_Append(char *dst, int size, const char *src) {
	int len = (int)strlen(dst);

	while (*src && len < size - 1) {
		dst[len++] = *src++;
	}
	dst[len] = 0;
}

void Video_CheckCaps(void) {
	uint32_t caps[D3DCAPS_WORDS];
	D3DAdapterId id;
	char text[0x400];

	void *d3d = exe_Direct3DCreate8(D3D_SDK_VERSION);
	if (!d3d) {
		return;
	}
	int hr = D3D_METHOD(d3d, d3dGetDeviceCaps_t, D3D_GETDEVICECAPS)(d3d, 0, D3DDEVTYPE_HAL, caps);
	byte ok = hr >= 0 && caps[D3DCAPS_PIXELSHADERVERSION] >= D3DPS_VERSION(1, 1);
	id.Description[0] = 0;
	D3D_METHOD(d3d, d3dGetAdapterIdentifier_t, D3D_GETADAPTERIDENTIFIER)(d3d, 0, 0, &id);
	if (!ok) {
		const char *msg = Lang_GetStr("Menu_Video_Warning");
		if (!msg) {
			msg = "Warning: Your graphics card doesn't meet the hardware requirements.\nYou can notice an important "
				"decrement on the quality of the image.";
		}
		Str_Copy(text, msg, sizeof(text));
		Str_Append(text, sizeof(text), "\n");
		if (id.Description[0]) {
			Str_Append(text, sizeof(text), "\nVideo device: ");
			Str_Append(text, sizeof(text), id.Description);
		}
		exe_MessageBoxA(NULL, text, "Warning", 0x11030);
		exe_BringWindowToTop(App_GetHWnd());
	}
	COM_RELEASE(d3d);
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

	D3D_METHOD(g_pd3dDevice, d3dGetBackBuffer_t, D3DDEV_GETBACKBUFFER)(g_pd3dDevice, 0, 0, &back);
	if (D3D_METHOD(back, d3dGetDesc_t, D3DSURF_GETDESC)(back, desc) < 0) {
		return;
	}
	if (D3D_METHOD(g_pd3dDevice, d3dCreateImageSurface_t, D3DDEV_CREATEIMAGESURFACE)(g_pd3dDevice,
		desc[SURFDESC_WIDTH], desc[SURFDESC_HEIGHT], (int)desc[SURFDESC_FORMAT], &img) < 0) {
		return;
	}
	D3D_METHOD(g_pd3dDevice, d3dPresent_t, D3DDEV_PRESENT)(g_pd3dDevice, NULL, NULL, NULL, NULL);
	D3D_METHOD(g_pd3dDevice, d3dCopyRects_t, D3DDEV_COPYRECTS)(g_pd3dDevice, back, NULL, 0, img, NULL);
	if (back) {
		COM_RELEASE(back);
		back = NULL;
	}
	D3D_METHOD(g_pd3dDevice, d3dPresent_t, D3DDEV_PRESENT)(g_pd3dDevice, NULL, NULL, NULL, NULL);
	D3D_METHOD(g_pd3dDevice, d3dGetBackBuffer_t, D3DDEV_GETBACKBUFFER)(g_pd3dDevice, 0, 0, &back);
	D3D_METHOD(g_pd3dDevice, d3dCopyRects_t, D3DDEV_COPYRECTS)(g_pd3dDevice, img, NULL, 0, back, NULL);
	if (back) {
		COM_RELEASE(back);
		back = NULL;
	}
	if (img) {
		COM_RELEASE(img);
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

void Splash_Show(const char *name, byte fadeIn, uint32_t waitMs, byte fadeOut) {
	char path[MAX_RUTA];
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
	E3D_LogPrint(g_FileLog, "SplashScreen: %s", path);
	tex = NULL;
	ArchIO_ReadAll(path, &data, &size);
	D3DX_CreateTextureFromFileInMemoryEx(g_MotorDev, data, size, 0, 0, 1, 0, 0, D3DPOOL_MANAGED, 1, 1, 0, NULL, NULL,
		&tex);
	Ptr_Delete(&data);
	const box_t rect = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
	uint32_t fade = g_SplashFade;
	if (fadeIn) {
		uint32_t start = exe_timeGetTime();
		uint32_t end = start + fade;
		if (exe_timeGetTime() < start + fade) {
			float fadeF = (float)(double)fade;
			do {
				Splash_Draw(&rect, Splash_Color(Splash_Gray(exe_timeGetTime() - start, fadeF)), tex);
			} while (exe_timeGetTime() < end);
		}
	}
	for (int i = 4; i != 0; i--) {
		Splash_Draw(&rect, 0xffffff, tex);
	}
	uint32_t wait = waitMs;
	if (name && !*name) {
		int left = (int)(g_SplashTime + waitMs - exe_timeGetTime());
		wait = left < 0 ? 0 : (uint32_t)left;
	}
	g_SplashTime = exe_timeGetTime();
	if (wait) {
		/* the presses before the splash screen do not count */
		exe_GetAsyncKeyState(VK_ESCAPE);
		exe_GetAsyncKeyState(VK_SPACE);
		exe_GetAsyncKeyState(VK_RETURN);
		uint32_t end = wait + exe_timeGetTime();
		while (exe_timeGetTime() < end) {
			short esc = exe_GetAsyncKeyState(VK_ESCAPE);
			short space = exe_GetAsyncKeyState(VK_SPACE);
			short enter = exe_GetAsyncKeyState(VK_RETURN);
			if (enter < 0 || esc < 0 || space < 0) {
				break;
			}
			exe_Sleep(10);
		}
	}
	if (fadeOut) {
		uint32_t start = exe_timeGetTime();
		uint32_t end = start + fade;
		if (exe_timeGetTime() < end) {
			float fadeF = (float)(double)fade;
			do {
				Splash_Draw(&rect, Splash_Color(0xff - Splash_Gray(exe_timeGetTime() - start, fadeF)), tex);
			} while (exe_timeGetTime() < end);
		}
		Splash_Draw(&rect, 0, tex);
	}
	if (tex) {
		COM_RELEASE(tex);
	}
}

void Splash_Draw(const box_t *rect, uint32_t color, void *tex) {
	D3D_METHOD(g_MotorDev, d3dScene_t, D3DDEV_BEGINSCENE)(g_MotorDev);
	Render_DrawTexRect(rect, color, color, tex);
	D3D_METHOD(g_MotorDev, d3dScene_t, D3DDEV_ENDSCENE)(g_MotorDev);
	D3D_METHOD(g_MotorDev, d3dPresent_t, D3DDEV_PRESENT)(g_MotorDev, NULL, NULL, NULL, NULL);
}

void Splash_Reset(void) {
	Splash_Show("2D/Loading/00Reset.dds", 0, 0, 0);
}

/* the loading screen */

/* one frame of the loading screen drawn by draw */
static void Loading_Frame(loadingDraw_t draw) {
	D3D_METHOD(g_pd3dDevice, d3dScene_t, D3DDEV_BEGINSCENE)(g_pd3dDevice);
	Render_BeginFrame();
	draw();
	Render_EndView();
	D3D_METHOD(g_pd3dDevice, d3dScene_t, D3DDEV_ENDSCENE)(g_pd3dDevice);
	D3D_METHOD(g_pd3dDevice, d3dPresent_t, D3DDEV_PRESENT)(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

void Loading_DrawBack2(void) {
	if (!g_LoadingDraw2 || !g_LoadingDrawOn) {
		return;
	}
	for (int i = 2; i != 0; i--) {
		Loading_Frame(g_LoadingDraw2);
	}
}

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
	byte *motor = g_Motor;
	int x0 = (int)MOTOR_MODE(motor)[6] * 0x74 / 0x500;
	if (!MOTOR_DEV(motor) || !g_D3DOn) {
		return;
	}
	float p = (float)percent / 100.0f;
	int w = (int)MOTOR_MODE(motor)[6];
	float fw = (float)w;
	r.min.x = (float)x0 / fw;
	r.max.x = (float)(((w * 0x48c + 0x280) / 0x500 - x0) * percent / 100 + x0) / fw;
	int h = (int)MOTOR_MODE(motor)[7];
	r.min.y = (float)(h * 0x398 / 1024) / (float)h;
	r.min.z = 0.0f;
	r.max.y = 0.90478515625f;	/* 0x3f67a000 */
	r.max.z = 0.0f;
	D3D_METHOD(MOTOR_DEV(motor), d3dScene_t, D3DDEV_BEGINSCENE)(MOTOR_DEV(motor));
	float q = 1.0f - p;
	int green = (int)(q * 200.0f + p * 255.0f);
	int red = (int)(q * 255.0f + p * 200.0f);
	Render_DrawTexRect(&r, 0xffc800, ((uint32_t)red << 8 | (uint32_t)green) << 8, NULL);
	D3D_METHOD(g_MotorDev, d3dScene_t, D3DDEV_ENDSCENE)(g_MotorDev);
	D3D_METHOD(g_MotorDev, d3dPresent_t, D3DDEV_PRESENT)(g_MotorDev, NULL, NULL, NULL, NULL);
}

void Loading_DrawBack(void) {
	if (g_LoadingDraw && g_LoadingDrawOn) {
		Loading_Frame(g_LoadingDraw);
	}
}
