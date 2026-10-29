/*
 * ScrapEngMain.cpp of the game: the glue between the game and the engine3d, see include/scrap/scrapmain.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/cmd.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filesys.h"
#include "scrap/graphic2d.h"
#include "scrap/input.h"
#include "scrap/readm3d.h"
#include "scrap/meshd3d.h"
#include "scrap/trimesh.h"
#include "scrap/material.h"
#include "scrap/e3dworld.h"
#include "scrap/fog.h"
#include "scrap/render.h"
#include "scrap/rendertarget.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/particlesystem.h"
#include "scrap/ground.h"
#include "scrap/text.h"
#include "scrap/texture.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"
#include "scrap/video.h"
#include "scrap/win32.h"

#define g_IsViewer GAME_GLOBAL_IN(int, GVar_Viewer, 16, 0x467b34)
#define D3DFMT_A8R8G8B8 21
#define D3DFMT_X8R8G8B8 22
#define D3DFMT_R5G6B5 23
#define D3DFMT_X1R5G5B5 24
#define D3DFMT_A1R5G5B5 25
#define APP_DEDICATED(app) (((app)->flags >> 2) & 1)

/* the engine callbacks as the exe passes them */
#define RVA_ENGINE_ONETIMEINIT 0x215e70
#define RVA_ENGINE_FRAMEMOVE 0x2171e0
#define RVA_ENGINE_FINALCLEANUP 0x215ad0
#define RVA_MODELDLG_PROC 0x22dcf0
#define RVA_CON_VERBOSEF 0xbc240

#define VK_RETURN 0x0d
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20
#define VK_F8 0x77

/* the debug actions of the second action set */
enum {
	DBG_STENCIL = 0x18,
	DBG_SPECULAR = 0x19,
	DBG_ENVMAP = 0x1a,
	DBG_ENVBUMP = 0x1b,
	DBG_GLOWMAP = 0x1c,
	DBG_CAMERA = 0x1d,
};

static D3DAdapterInfo *Engine_AdapterInfo(void) {
	CHECK(g_Engine);
	D3DAdapterInfo *ai = ENGINE_ADAPTERINFO(g_Engine);
	CHECK(ai);
	return ai;
}

static void StartVideo_End(void) {
	if (!(g_StartVideoFlags & 1)) {
		cString_Set(g_StartVideo, "");
	}
}

/* 0 while the video plays on (StartVideo_End after it, from Engine_VideoResume) */
static int StartVideo_Play(void) {
	if (Video_PlayFile(*g_StartVideo, 0) == VIDEO_PENDING) {
		return 0;
	}
	StartVideo_End();
	return 1;
}

/* the rest of Engine_Init, Engine_LevelEnd or Video_PlayStart after a video of theirs, run by Engine_VideoResume */
enum {
	ENGWAIT_NONE,
	ENGWAIT_INIT,
	ENGWAIT_LEVELEND,
	ENGWAIT_PLAYSTART,
	ENGWAIT_PLAYER,
};
static int eng_wait;
static void (*eng_playerDone)(int result);	/* of Video_PlayThen, called with the result after the video */
static int eng_initStep;		/* the next step of Engine_InitVideos */
static int eng_videoResult;		/* of the video of the step before */

/* the steps of Engine_InitVideos: the four intro videos, MSE-Short, the start video */
#define ENGINIT_STARTVIDEO 5

/* the videos of Engine_Init and the rest after them; 0 while a video plays on (Engine_VideoResume goes on with the
 * next step) */
static int Engine_InitVideos(void) {
	static const char *const intro[ENGINIT_STARTVIDEO] = { "Video/Enlight.avi", "Video/Distrib.avi",
		"Video/Mauretania.avi", "Video/MSE.avi", "Video/MSE-Short.avi" };

	while (eng_initStep <= ENGINIT_STARTVIDEO) {
		int step = eng_initStep++;
		if (step == ENGINIT_STARTVIDEO) {
			if (*g_StartVideo && **g_StartVideo && !StartVideo_Play()) {
				return 0;
			}
			continue;
		}
		/* MSE-Short when MSE.avi did not run to its end */
		if (APP_DEDICATED(g_pApp) || (step == 4 && eng_videoResult == 1)) {
			continue;
		}
		int r = Video_PlayFile(intro[step], 0);
		if (r == VIDEO_PENDING) {
			return 0;
		}
		eng_videoResult = r;
	}
	if (g_IsDemo) {
		Splash_Show("2D/Loading/00LogoEnlight.dds", 1, 4000, 1);
		if (g_IsDemo) {
			Splash_Show("2D/Loading/00LogoMSE.dds", 1, 4000, 1);
		}
	}
	Splash_Show("2D/Loading/00Legal_global.dds", 1, 0, 0);
	Con_Printf("Engine 3D initialized\n");
	return 1;
}

int Engine_VideoWait(void) {
	return eng_wait != ENGWAIT_NONE;
}

int Engine_VideoResume(void) {
	if (Video_Busy()) {
		return 0;
	}
	int wait = eng_wait;
	eng_wait = ENGWAIT_NONE;
	switch (wait) {
	case ENGWAIT_INIT:
		if (eng_initStep > ENGINIT_STARTVIDEO) {
			StartVideo_End();
		} else {
			eng_videoResult = Video_Result();
		}
		if (!Engine_InitVideos()) {
			eng_wait = ENGWAIT_INIT;
			return 0;
		}
		break;
	case ENGWAIT_LEVELEND:
		StartVideo_End();
		Splash_Show(NULL, 1, 0, 0);
		break;
	case ENGWAIT_PLAYSTART:
		StartVideo_End();
		break;
	case ENGWAIT_PLAYER: {
		void (*done)(int) = eng_playerDone;
		eng_playerDone = NULL;
		if (done) {
			done(Video_Result());
		}
		break;
	}
	}
	return 1;
}

void Engine_Close(void) {
	CHECK(g_Engine);
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
	if (g_IsDemo) {
		Splash_Show("2D/Loading/00Promo01.dds", 1, 6000, 1);
		if (g_IsDemo) {
			Splash_Show("2D/Loading/00Promo02.dds", 1, 20000, 1);
		}
	}
	CD3DApp_Cleanup(g_Engine);
	Engine3D_Shutdown();
	Con_Printf("Engine 3D closed\n");
}

byte Engine_FinalCleanup(void) {
	CPsaImage *w = g_Watermark;
	if (w) {
		CPsaImage_dtor(w);
		operator_delete_sized(w, 0x44);
		g_Watermark = NULL;
	}
	Camera_SetCurrent(NULL);
	CGameCamera *cam = g_ViewerCamera;
	if (cam) {
		CGameCamera_dtor(cam);
		operator_delete_sized(cam, 0x3c);
		g_ViewerCamera = NULL;
	}
	Fonts_Unload();
	CWorld *m = g_World;
	if (m) {
		CWorld_dtor(m);
		operator_delete_sized(m, sizeof(CWorld));
		g_World = NULL;
	}
	WorldInfo_Reset();
	ParticleSystem_LoadIni(NULL, NULL);
	return 1;
}

CGameCamera *Camera_GetCurrent(void) {
	return g_CamCurrent;
}

byte Keys_SkipPressed(void) {
	byte any = 0;
	if (exe_GetAsyncKeyState(VK_ESCAPE) < 0) {
		any = 1;
	}
	if (exe_GetAsyncKeyState(VK_SPACE) < 0) {
		any = 1;
	}
	if (exe_GetAsyncKeyState(VK_RETURN) < 0) {
		any = 1;
	}
	return any;
}

int Format_Bpp(int fmt) {
	switch (fmt) {
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		return 32;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
		return 16;
	default:
		return 0;
	}
}

int Engine_GetOption(int opt) {
	return RenderMode(opt, 0, 0);
}

int Video_GetCurrentMode(void) {
	D3DAdapterInfo *ai = Engine_AdapterInfo();
	return ai->devices[ai->dwCurrentDevice].dwCurrentMode;
}

int Video_GetModes(int i, char *buf) {
	D3DAdapterInfo *ai = Engine_AdapterInfo();
	int n = ai->devices[ai->dwCurrentDevice].dwNumModes;
	if (i >= 0 && buf) {
		CHECK(i < n);
		D3DModeInfo *mi = &ai->devices[ai->dwCurrentDevice].modes[i];
		CHECK(mi);
		Str_PrintfQuiet(buf, 0x100, "%dx%d (%d bpp)", mi->Width, mi->Height, Format_Bpp(mi->Format));
	}
	return n;
}

int Video_GetStatus(void) {
	return g_VideoStatus;
}

byte Engine_OneTimeInit(void) {
	g_RenderFlag6e0 = g_RenderFlag6e4;
	g_GroundTrisPerNode = 8000000;
	g_CollisionEpsilon = g_GvCollisionEpsilon;
	int lm = g_RenderLightmap;
	g_LightmapMode2 = lm == 2;
	g_LightmapOff = lm == 0;
	if (g_LightmapOff) {
		g_MaxLightmap = 0;
	} else {
		g_MaxLightmap = g_IsXbox ? 0 : 2;
	}
	g_ClipNear = 0.0f;
	g_ClipFar = 0.0f;
	g_ShadowsStencil = g_StencilShadows > 0;
	g_SkyPath[0] = 0;
	g_SkyPosScaleIni = 0.0f;
	g_SkyRotationIni = 0.0f;
	g_FogColorIni->intens = 0.0f;
	g_ModelAmbientIni->intens = 0.0f;
	Engine_LoadIni(g_M3dIni, NULL);
	Engine_LoadIni(g_GroundPath, ".ini");
	ParticleSystem_LoadIni(NULL, NULL);
	ParticleSystem_LoadIni(g_GroundPath, "particles.ini");
	ParticleSystem_LoadIni("bmp/particles.ini", NULL);
	void *p = operator_new(sizeof(CWorld));
	g_World = p ? CWorld_ctor(p) : NULL;
	WorldInfo_Reset();
	CWorld_LoadSky(g_World, g_SkyPath);
	CWorld_LoadGroundScale(g_World, g_GroundPath, g_GroundScale);
	CWorld_SetSkyPos(g_World, g_SkyPosScaleIni);
	CWorld_SetSkyRot(g_World, g_SkyRotationIni);
	CWorld_SetClip(g_World, g_ClipNear, g_ClipFar);
	g_World->fog.active = g_FogActiveIni != 0;
	CFog_SetRange(&g_World->fog, g_FogRangeIni, g_FogRangeEnd);
	g_World->fog.table = g_FogTableMode != 0;
	g_World->fog.density = g_FogDensityIni;
	g_World->fog.mode = g_FogLinear ? 3 : 1;	/* D3DFOG_LINEAR, D3DFOG_EXP */
	if (g_FogColorIni->intens != 0.0f) {
		CFog_SetColor(&g_World->fog, g_FogColorIni->rgba, g_FogColorIni->intens);
	}
	Cmd_ReadFile("engine.txt");
	Fonts_Load();
	Graphic2D_SetScreen(640, 480);
	CHECK(!g_ViewerCamera);
	p = operator_new(0x3c);
	g_ViewerCamera = p ? CGameCamera_ctor(p) : NULL;
	CCamera *cam = CWorld_GetCameraGround(World_Get(), 1);
	if (!cam) {
		cam = World_Get()->cam;
	}
	CGameCamera_FromCamera(g_ViewerCamera, cam);
	p = operator_new(0x44);
	g_Watermark = p ? CPsaImage_ctor(p, "bmp/norm1.alpha.psa", 0x10008) : NULL;
	return 1;
}

void Engine_Init(void) {
	int64_t freq;

	CHECK(!g_Engine);
	Con_Printf("Engine 3D initializing (Version %d)\n", g_EngineVersion);
	if (APP_DEDICATED(g_pApp)) {
		g_RenderOff = 1;
	}
	if (!APP_DEDICATED(g_pApp)) {
		Video_CheckCaps();
	}
	void *hInst = App_GetHInstance();
	void *hWnd = App_GetHWnd();
	g_TimerQPMult = g_TimerFreqMult;
	Timer_GetQPFreq(&freq);
	E3D_LogDebug(g_FileLog, "TimerQPFreq: %d", (int)freq);
	Engine3D_Create(hInst, hWnd, GAME_FUNC(Engine_OneTimeInit, RVA_ENGINE_ONETIMEINIT), GAME_FUNC(Engine_FrameMove, RVA_ENGINE_FRAMEMOVE),
		GAME_FUNC(Engine_FinalCleanup, RVA_ENGINE_FINALCLEANUP), 0);
	ENGINE_FULLSCREEN(g_Engine) = g_FullScreen && !APP_DEDICATED(g_pApp);
	ENGINE_WIDTH(g_Engine) = g_VideoWidth;
	ENGINE_HEIGHT(g_Engine) = g_VideoHeight;
	ENGINE_BPP(g_Engine) = g_VideoBPP;
	g_MapTextSavingOn = g_ComputeMapTextSaving == 1;
	byte *m = g_Engine;
	if (!APP_DEDICATED(g_pApp)) {
		if (((int(__thiscall *)(byte *, void *))(*(void ***)m)[ENGINE_VT_CREATE / 4])(m, hInst) < 0) {
			Err_Printf(1, "Engine3D initialization error");
		}
	} else {
		ENGINE_CALL(m, ENGINE_VT_RESTORE);
	}
	/* differs from the original on purpose (no blocking waits): a video plays on from the message loop, the steps after
	 * it run from Engine_VideoResume */
	eng_initStep = 0;
	if (!Engine_InitVideos()) {
		eng_wait = ENGWAIT_INIT;
	}
}

/* al from the window flag with the rest of eax from g_Engine, like the original's sete */
int Engine_IsFullScreen(void) {
	byte *m = g_Engine;
	if (m) {
		return (int)(((uintptr_t)m & ~(uintptr_t)0xff) | (ENGINE_WINDOWED(m) == 0));
	}
	if (g_FullScreen && !APP_DEDICATED(g_pApp)) {
		return 1;
	}
	return 0;
}

void Engine_LevelEnd(void) {
	CHECK(g_Engine);
	if (ENGINE_CALL(g_Engine, ENGINE_VT_INVALIDATE) < 0) {
		Err_Printf(1, "ERROR: LevelEndEngine3D()");
	}
	if (ENGINE_CALL(g_Engine, ENGINE_VT_DELETE) < 0) {
		Err_Printf(1, "ERROR: LevelEndEngine3D()");
	}
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
	if (*g_StartVideo) {
		if (**g_StartVideo && CD3DApp_ResetDevice(g_Engine) < 0) {
			E3D_LogBlue(g_FileLog, "ERROR: 'ResetDevice()' failed");
		}
		/* differs from the original on purpose (no blocking waits): the rest after the video from
		 * Engine_VideoResume */
		if (*g_StartVideo && **g_StartVideo && !StartVideo_Play()) {
			eng_wait = ENGWAIT_LEVELEND;
			return;
		}
	}
	Splash_Show(NULL, 1, 0, 0);
}

void Engine_LevelInit(void) {
	char path[MAX_FILEPATH];

	CHECK(g_Engine);
	g_VideoBufferSize = 0;
	g_MapTextSaving = 0;
	const char *sep = strrchr(g_GroundPath, '/');
	if (g_ComputeMapTextSaving) {
		static const char name[] = "/MapTextData.ini";
		size_t n = sep ? (size_t)(sep - g_GroundPath) : 0;
		/* the original copies without a bound (strncpy, strcat) */
		if (n > sizeof(path) - sizeof(name)) {
			n = sizeof(path) - sizeof(name);
		}
		memset(path, 0, sizeof(path));
		memcpy(path, g_GroundPath, n);
		memcpy(path + strlen(path), name, sizeof(name));
		g_MapTextDataFile = File_OpenRel(path, "wt");
	}
	g_DrawProgressBar = 1;
	g_ProgressBarCount = 0;
	Loading_Step(0);
	void *dev = ENGINE_DEV(g_Engine);
	ENGINE_DEV(g_Engine) = NULL;
	if (ENGINE_CALL(g_Engine, ENGINE_VT_RESTORE) < 0) {
		Err_Printf(1, "ERROR: LevelInitEngine3D()");
	}
	ENGINE_DEV(g_Engine) = dev;
	if (!APP_DEDICATED(g_pApp)) {
		if (ENGINE_CALL(g_Engine, ENGINE_VT_RESTORE2) < 0) {
			Err_Printf(1, "ERROR: LevelInitEngine3D()");
		}
	}
}

void Engine_LevelLoaded(void) {
	char buf[0x80];

	if (g_ComputeMapTextSaving && g_MapTextDataFile) {
		snprintf(buf, sizeof(buf), "[video]\n;Map textures memory saving: %d\n", g_MapTextSaving);
		crt_fwrite(buf, 1, strlen(buf), g_MapTextDataFile);
		snprintf(buf, sizeof(buf), "VideoBufferSize = %d", g_VideoBufferSize);
		crt_fwrite(buf, 1, strlen(buf), g_MapTextDataFile);
		crt_fclose(g_MapTextDataFile);
		g_MapTextDataFile = NULL;
		g_VideoBufferSize = 0;
		g_MapTextSaving = 0;
	}
	Loading_Step(100);
	g_DrawProgressBar = 0;
	Con_Verbosef("ProgressBarCount: %d (%d)\n", g_ProgressBarCount, g_ProgressBar);
}

/* CGameCamera: position at 0x0, angles at 0xc, target at 0x18 */
void Camera_SaveRestore(CGameCamera *cam, byte save) {
	char name[MAX_FILEPATH] = "camera.set";
	byte *c = (byte *)cam;

	if (save) {
		void *f = File_OpenRel(name, "wb");
		if (!f) {
			return;
		}
		crt_fwrite(c + 0xc, 0xc, 1, f);
		crt_fwrite(c, 0xc, 1, f);
		crt_fwrite(c + 0x18, 0xc, 1, f);
		crt_fclose(f);
		Con_Verbosef("Camera saved: %s\n", name);
	} else {
		void *f = File_OpenRel(name, "rb");
		if (!f) {
			return;
		}
		crt_fread(c + 0xc, 0xc, 1, f);
		crt_fread(c, 0xc, 1, f);
		crt_fread(c + 0x18, 0xc, 1, f);
		crt_fclose(f);
		Con_Verbosef("Camera restored: %s\n", name);
	}
}

void Scene_ShowTree(CScene *scn, CNode3D *parent, int indent) {
	CHECK(scn);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (n->parent != parent) {
			continue;
		}
		Con_Verbosef("%*s%s", indent + 2, "", n->name);
		int len = (int)strlen(n->name);
		Con_Debugf("%*s  %3d %3d %3d  c:%-8s  f:%08X  a:%04X", 0x30 - len - indent, "", n->index, n->i48, n->renderNode,
			Obj3D_ClassName(CNode3D_Class(n)), n->flags, (uint16_t)n->i50);
		if (CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH) {
			CTriMesh *mt = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
			if (mt->pSkin) {
				Con_Debugf("  [skin]");
			}
		}
		if (CNode3D_Class(n) == CLASS_OBJ3D_MESHD3D) {
			CMeshD3D *md = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_MESHD3D);
			if (md->pSkin) {
				Con_Debugf("  [skin]");
			}
		}
		Con_Verbosef("\n");
		Scene_ShowTree(scn, n, indent + 2);
	}
}

const void *Eng_Identity(const void *v) {
	return v;
}

byte Music_Play(const char *name, const char *cmd, float volume) {
	CHECK(cmd);
	if (!crt_stricmp(cmd, "play")) {
		return Music_PlayFile(name, NULL);
	}
	if (!crt_stricmp(cmd, "pause")) {
		return Music_Pause();
	}
	if (!crt_stricmp(cmd, "stop")) {
		return Music_Stop();
	}
	if (!crt_stricmp(cmd, "volume")) {
		return Music_SetVolume(volume);
	}
	if (!crt_stricmp(cmd, "mainvol")) {
		return Music_SetMainVolume(volume);
	}
	if (!crt_stricmp(cmd, "fadeout")) {
		return Music_FadeOut(0.0f, volume);
	}
	E3D_LogBlue(g_FileLog, "MusicPlayer: invalid command: %s", cmd);
	return 0;
}

void Eng_Printf3D(const vec3_t *pos, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		VBprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		Text_Printf3D(pos, "%s", buf);
	}
}

void Eng_PrintfRel(int dx, int dy, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		VBprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		Text_PrintfRel(dx, dy, "%s", buf);
	}
}

void Eng_PrintfXY(int x, int y, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		VBprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		Text_Printf(x, y, "%s", buf);
	}
}

byte Video_PlayStart(byte play) {
	if (!*g_StartVideo || !**g_StartVideo) {
		return 0;
	}
	/* differs from the original on purpose (no blocking waits): StartVideo_End after the video from
	 * Engine_VideoResume */
	if (play && !StartVideo_Play()) {
		eng_wait = ENGWAIT_PLAYSTART;
	}
	return 1;
}

/* view times projection, called in the order of the original (projection first) */
static void ViewProj(matrix_t *out) {
	matrix_t tmp;
	const matrix_t *proj = Render_GetProj();
	const matrix_t *view = Render_GetView();
	*out = *Matrix_MulD3D(&tmp, view, proj);
}

/* x, y from -1..1 to the 2D screen */
static void ToScreen(float x, float y, float *out) {
	int w, h;
	Graphic2D_GetScreen(&w, &h);
	out[0] = (x + 1.0f) * (float)w * 0.5f;
	out[1] = (1.0f - y) * (float)h * 0.5f;
}

byte Engine_WorldToScreen(const vec3_t *pos, float *out, float *depth) {
	matrix_t m;
	float x, y, w;

	CHECK(g_pd3dDevice);
	ViewProj(&m);
	Matrix_TransformParts(&x, &y, NULL, &w, pos, &m);
	byte visible = 1;
	float ax = (float)__builtin_fabs((double)x);
	float ay = (float)__builtin_fabs((double)y);
	if (ax >= w || ay >= w) {
		visible = 0;
		if (ay + ax == 0.0f) {
			x = 0.0f;
			y = 1.0f;
		} else {
			float s = 1.0f / Float_MaxSS(ax, ay);
			y = s * y;
			x = s * x;
		}
	} else {
		x = x / w;
		y = y / w;
	}
	if (out) {
		ToScreen(x, y, out);
	}
	if (visible && depth) {
		*depth = 1.0f / w;
	}
	return visible;
}

void Engine_DirToScreen(const vec3_t *pos, float *dest) {
	matrix_t m;
	float x, y;

	CHECK(g_pd3dDevice);
	CHECK(dest);
	ViewProj(&m);
	Matrix_TransformParts(&x, &y, NULL, NULL, pos, &m);
	float len = (float)__builtin_sqrt((double)(y * y + x * x));
	if (len != 0.0f) {
		float s = 1.0f / len;
		y = s * y;
		x = s * x;
	}
	ToScreen(x, y, dest);
}

void Eng_Printf(int fnt, float x, float y, int align, uint32_t color, const char *fmt, ...) {
	CFont *fnttab[3];
	char buf[0x100];
	va_list ap;

	fnttab[1] = g_FontBigNumbers;
	fnttab[2] = g_FontHoratio;
	fnttab[0] = NULL;
	if ((uint32_t)(fnt - 1) > 1 || !fnttab[fnt] || !fmt) {
		CheckFailed("fnt>0 && fnt<3 && fnttab[fnt] && frmt", __FILE__, __TIMESTAMP__, __LINE__);
	}
	/* the original reads its stack for a bad fnt */
	CFont *font = (uint32_t)fnt < 3 ? fnttab[fnt] : NULL;
	va_start(ap, fmt);
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (align & 1) {
		CFont_PrintfAlignColorShadow(font, x, y, (align & 2) * 2, color, "%s", buf);
		return;
	}
	CFont_PrintfAlignColor(font, x, y, (align & 2) * 2, color, "%s", buf);
}

byte Engine_FrameMove(void) {
	if (g_EngCallbacks[0] && g_CallbacksOn) {
		g_EngCallbacks[0]();
	}
	Models_UpdateAnims(0.0f);
	Music_Update();
	CamComposite_DrawDebug();
	if (g_IsViewer) {
		Engine_DebugKeys();
	}
	if (g_Watermark && g_ShowSplashScreen == 100) {
		PSAIMAGE_COLOR(g_Watermark) = 0;
		CPsaImage_Draw(g_Watermark, 321.0f, 11.0f);
		PSAIMAGE_COLOR(g_Watermark) = 0xffffff;
		CPsaImage_Draw(g_Watermark, 320.0f, 10.0f);
	}
	return 1;
}

void Engine_Render(void) {
	CHECK(g_Engine);
	CHECK(!g_DrawProgressBar);
	Camera_SetCurrent(g_CamCurrent);
	CD3DApp_Render(g_Engine);
	if (g_ResetPending) {
		Engine_ResetAll();
		g_ResetPending = 0;
	}
}

static byte DebugAction(uint32_t i) {
	return cInputAction_Pressed(TABLE_AT(TABLE_AT(g_ActionSets, 1)->actions, i), 0);
}

/* the value of an action of the debug set for player 0 */
static float DebugValue(uint32_t i) {
	return TABLE_AT(TABLE_AT(g_ActionSets, 1)->actions, i)->value[0];
}

/* the free camera of the viewer (Gvars) */
#define g_ViewerClipNear GAME_GLOBAL_IN(float, GVar_ViewerClipNear, 16, 0x4a87b0)
#define g_ViewerClipFar GAME_GLOBAL_IN(float, GVar_ViewerClipFar, 16, 0x4a87c8)
#define g_ViewerFOV GAME_GLOBAL_IN(float, GVar_ViewerFOV, 16, 0x4a8610)	/* degrees, 0: the camera keeps its own */
#define g_ViewerCameraTgt GAME_GLOBAL_IN(int, GVar_ViewerCameraTgt, 16, 0x4a8660)	/* orbits around the target */
#define g_ViewerCameraVel GAME_GLOBAL_IN(float, GVar_ViewerCameraVel, 16, 0x4a86f0)
#define g_ViewerCameraInv GAME_GLOBAL_IN(int, GVar_ViewerCameraInv, 16, 0x4a85b0)	/* the pitch not inverted */
#define g_ViewerSpeed GAME_GLOBAL(float, g_ViewerSpeed, 0x431b04)	/* doubled and halved by the debug keys */
#define VIEWER_MIN_DIST 20.0f
/* the actions of the debug set */
#define DBG_VIEWER_TGT 23
#define DBG_VIEWER_FASTER 14
#define DBG_VIEWER_SLOWER 15
#define DBG_VIEWER_UP 2
#define DBG_VIEWER_DOWN 3
#define DBG_VIEWER_LEFT 4
#define DBG_VIEWER_RIGHT 5
#define DBG_VIEWER_FWD 16
#define DBG_VIEWER_BACK 9
#define DBG_VIEWER_PAN 18

/* the viewer camera by the debug actions: turned (or panned with the pan action) in free mode, orbiting the
 * target (or moved up and down with the pan action) in target mode */
void ViewerCamera_Move(CGameCamera *cam) {
	vec3_t old = cam->pos, d;
	float speed, fwd, side, mv, r, len, k, a, b;
	byte pan;

	cam->zNear = g_ViewerClipNear;
	cam->zFar = g_ViewerClipFar;
	if (g_ViewerFOV > 0.0f) {
		cam->fov = (float)((double)g_ViewerFOV * 3.141592653589793 / 180.0);
	}
	CGameCamera_SetLookAt(cam, g_ViewerCameraTgt != 0);
	if (DebugAction(DBG_VIEWER_TGT)) {
		g_ViewerCameraTgt = g_ViewerCameraTgt == 0;
	}
	if (DebugAction(DBG_VIEWER_FASTER)) {
		g_ViewerSpeed = (float)((double)g_ViewerSpeed + (double)g_ViewerSpeed);
		Text_Message(250, "velocity: x%g", (double)g_ViewerSpeed);
	}
	if (DebugAction(DBG_VIEWER_SLOWER)) {
		g_ViewerSpeed = (float)((double)g_ViewerSpeed * 0.5);
		Text_Message(250, "velocity: x%g", (double)g_ViewerSpeed);
	}
	speed = g_ViewerCameraVel * 100.0f * g_FrameMs * g_ViewerSpeed;
	a = DebugValue(DBG_VIEWER_UP);
	b = DebugValue(DBG_VIEWER_DOWN);
	fwd = a - b;
	a = DebugValue(DBG_VIEWER_RIGHT);
	b = DebugValue(DBG_VIEWER_LEFT);
	side = a - b;
	a = DebugValue(DBG_VIEWER_FWD);
	b = DebugValue(DBG_VIEWER_BACK);
	mv = (a - b) * speed;
	CGameCamera_Move(cam, mv);
	pan = DebugValue(DBG_VIEWER_PAN) > 0.5f;
	if (!g_ViewerCameraTgt) {
		if (pan) {
			CGameCamera_Strafe(cam, side * speed, fwd * speed);
			return;
		}
		cam->rot.x = (float)(g_ViewerCameraInv != 0 ? 1 : -1) * (g_FrameMs * fwd) + cam->rot.x;
		cam->rot.y = g_FrameMs * side + cam->rot.y;
		return;
	}
	if (pan) {
		Debug_DrawAxes(g_MatIdentity, &cam->target, 100.0f);
		mv = fwd * speed;
		cam->target.y = mv + cam->target.y;
		cam->pos.y = mv + cam->pos.y;
		return;
	}
	/* the distance kept, at least VIEWER_MIN_DIST; a move through the target is taken back */
	d.x = cam->pos.x - cam->target.x;
	d.y = cam->pos.y - cam->target.y;
	d.z = cam->pos.z - cam->target.z;
	r = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (0.0f > (cam->pos.y - cam->target.y) * (old.y - cam->target.y) +
			(cam->pos.x - cam->target.x) * (old.x - cam->target.x) +
			(cam->pos.z - cam->target.z) * (old.z - cam->target.z)) {
		cam->pos = old;
		r = VIEWER_MIN_DIST;
	} else if (!(r > VIEWER_MIN_DIST)) {
		r = VIEWER_MIN_DIST;
	}
	k = g_FrameMs * r;
	CGameCamera_Strafe(cam, k * side, k * fwd);
	d.y = cam->pos.y - cam->target.y;
	d.x = cam->pos.x - cam->target.x;
	d.z = cam->pos.z - cam->target.z;
	len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if ((double)len != 0.0) {
		k = (float)(1.0 / (double)len);
		d.x = d.x * k;
		d.y = d.y * k;
		d.z = d.z * k;
	}
	cam->pos.x = d.x * r + cam->target.x;
	cam->pos.z = cam->target.z + d.z * r;
	cam->pos.y = cam->target.y + d.y * r;
}

byte Engine_DebugKeys(void) {
	if (!g_ViewerCameraAnim) {
		Camera_SetCurrent(g_ViewerCamera);
		ViewerCamera_Move(g_ViewerCamera);
	}
	if (exe_GetAsyncKeyState(VK_CONTROL) < 0) {
		return 1;
	}
	byte shift = exe_GetAsyncKeyState(VK_SHIFT) < 0;
	if (shift && exe_GetAsyncKeyState(VK_F8) < 0) {
		Engine_ResetTextures();
		Con_Verbosef("Textures reloaded\n");
	}
	if (DebugAction(DBG_STENCIL)) {
		ShadowsStencil_Set(g_ShadowsStencil == 0);
	}
	if (DebugAction(DBG_SPECULAR)) {
		g_VertexSpecular = g_VertexSpecular == 0;
		Con_Verbosef("Vertex specular: %d\n", g_VertexSpecular);
	}
	if (DebugAction(DBG_ENVMAP)) {
		GVar_SetIntVerbose(g_GvEnvMap, g_GvEnvMap->value.i == 0);
	}
	if (DebugAction(DBG_ENVBUMP)) {
		GVar_SetIntVerbose(g_GvEnvBump, g_GvEnvBump->value.i == 0);
	}
	if (DebugAction(DBG_GLOWMAP)) {
		GVar_SetIntVerbose(g_GvGlowMap, g_GvGlowMap->value.i == 0);
	}
	if (DebugAction(DBG_CAMERA) && !shift) {
		Camera_SaveRestore(g_ViewerCamera, 0);
	}
	if (DebugAction(DBG_CAMERA) && shift) {
		Camera_SaveRestore(g_ViewerCamera, 1);
	}
	return 1;
}

void DVF_Process(const char *name, int arg) {
	if (!name || !*name) {
		name = "";
	}
	Str_Copy(g_DVFName, name, 0x100);
	if (arg == 1) {
		g_TileOn = 1;
		return;
	}
	if (arg == 2) {
		Avi_Save(g_DVFName);
		g_DVFName[0] = 0;
		return;
	}
	if (arg == 3) {
		Avi_Frame(g_DVFName, 0);
		return;
	}
	if (arg == 4) {
		Avi_Load(g_DVFName);
		g_DVFName[0] = 0;
	}
}

void Loading_Fade(void) {
	Splash_Show("", 0, 3000, 1);
}

void Engine_ResetAll(void) {
	Con_Printf("ResetEngine3D_All\n");
	CHECK(ENGINE_DEV(g_Engine));
	Splash_Show("2D/Loading/00Reset.dds", 1, 500, 0);
	byte *m = g_Engine;
	ENGINE_VSYNC(m) = g_VSync;
	if (ENGINE_WINDOWED(m)) {
		ENGINE_PP(m)[12] = 0;	/* FullScreen_PresentationInterval */
	} else {
		ENGINE_PP(m)[12] = ENGINE_VSYNC(m) ? 0 : 0x80000000;	/* D3DPRESENT_INTERVAL_IMMEDIATE */
	}
	if (CD3DApp_Reset(g_Engine) < 0) {
		E3D_LogBlue(g_FileLog, "ERROR: ResetEngine3D_All()");
	}
}

void Engine_ResetTextures(void) {
	Con_Printf("ResetEngine3D_Textures\n");
	CHECK(ENGINE_DEV(g_Engine));
	Texture_ReleaseAllD3D();
	CMatList_Release(g_MatList);
	CMatList_Prepare(g_MatList, ENGINE_DEV(g_Engine));
}

int Eng_RenderTargetEnd(void) {
	return RenderTarget_End(NULL);
}

byte Eng_ResetViewRect(void) {
	return Render_SetViewRect(NULL, 1);
}

byte Engine_FrameSetup(void) {
	g_ShowCollision = g_ShowMapCollision;
	g_WireFrame = g_ShowMapWireFrame;
	g_CollisionEpsilon = g_GvCollisionEpsilon;
	int lm = g_RenderLightmap;
	g_LightmapMode2 = lm == 2;
	g_LightmapOff = lm == 0;
	g_ModelsOn = g_RenderModels != 0;
	g_UseVB = g_RenderUseVB != 0;
	if (!g_PSOn && !g_CalcLmap2) {
		g_LightmapOff = 1;
	}
	uint32_t amb = g_ModelAmbient;
	if (g_ModelAmbientIni->intens == 0.0f) {
		if (!amb) {
			goto done;
		}
	} else if (!amb) {
		amb = Color_ToD3D(g_ModelAmbientIni);
	}
	g_AmbientD3D = amb;
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_AMBIENT, amb);
done:
	if (g_EngCallbacks[1] && g_CallbacksOn) {
		g_EngCallbacks[1]();
	}
	return 1;
}

void Screenshot_Take(const char *name) {
	g_TileOn = 1;
	if (!name || !*name) {
		name = "";
	}
	Str_Copy(g_ScreenshotName, name, 0x100);
}

void Loading_RandomName(char *buf, int size) {
	char level[MAX_FILEPATH], next[MAX_FILEPATH], fmt[MAX_FILEPATH];
	int count = 0;

	Path_Part(level, g_pApp->levelPath, PATH_NAME);
	Path_Part(next, g_pApp->newLevelPath, PATH_NAME);
	Str_PrintfQuiet(fmt, sizeof(fmt), "2D/Loading/%s_%s_%s.dds", level, next, "%02d");
	File_FreeIndex(fmt, &count);
	if (!count) {
		Str_PrintfQuiet(fmt, sizeof(fmt), "2D/Loading/%s_%s.dds", next, "%02d");
		File_FreeIndex(fmt, &count);
		if (!count) {
			Str_PrintfQuiet(fmt, sizeof(fmt), "2D/Loading/Default.dds");
			goto done;
		}
	}
	g_LoadingScreen = Rand_Other(0, count - 1, g_LoadingScreen, 10);
done:
	Str_PrintfQuiet(buf, size, fmt, g_LoadingScreen);
}

void Camera_SetCurrent(CGameCamera *cam) {
	if (!cam && !g_CamCurrent) {
		return;
	}
	CCamera *c = NULL;
	g_CamCurrent = cam;
	if (g_pd3dDevice) {
		E3D_FlushBatch();
	}
	if (cam) {
		c = CGameCamera_Update(cam);
	}
	CWorld *m = World_Get();
	if (c != m->cam) {
		CWorld_SetCamera(m, c);
	}
	if (g_pd3dDevice) {
		Render_SetCamera(World_Get()->cam);
	}
}

void GVar_SetIntVerbose(cGVar *v, int value) {
	v->value.i = value;
	Con_Verbosef("%s = %d\n", v->name, value);
}

char *Engine_SetIniName(const char *name) {
	return Path_Merge(g_M3dIni, name, ".ini");
}

void Engine_SetCallback(void (*fn)(void), int which) {
	switch (which) {
	case 1:
	case 2:
	case 3:
	case 4:
		g_EngCallbacks[which - 1] = fn;
		return;
	default:
		CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	}
}

int Engine_SetOption(int opt, int value) {
	return RenderMode(opt, value, 1);
}

byte Engine_Unsupported(void) {
	CheckFailed("0", __FILE__, __TIMESTAMP__, __LINE__);
	return 0;
}

int Eng_RenderTargetBegin(const char *name) {
	return RenderTarget_Begin(name);
}

byte Eng_SetViewRect(float x, float y, float w, float h) {
	box_t r;
	r.min.x = x / 640.0f;
	r.min.z = 0.0f;
	r.max.z = 0.0f;
	r.min.y = y / 480.0f;
	r.max.x = (x + w) / 640.0f;
	r.max.y = (y + h) / 480.0f;
	return Render_SetViewRect(&r, 1);
}

void Video_SetCurrentMode(int mode) {
	D3DAdapterInfo *ai = Engine_AdapterInfo();
	if (mode < 0 || mode >= ai->devices[ai->dwCurrentDevice].dwNumModes) {
		CheckFailed("NumMode>=0 && NumMode<NumModes", __FILE__, __TIMESTAMP__, __LINE__);
	}
	D3DDeviceInfo *dev = &ai->devices[ai->dwCurrentDevice];
	D3DModeInfo *mi = &dev->modes[mode];
	CHECK(mi);
	g_VideoWidth = (int)mi->Width;
	g_VideoHeight = (int)mi->Height;
	g_VideoBPP = Format_Bpp((int)mi->Format);
	dev->dwCurrentMode = mode;
	uint32_t *pp = ENGINE_PP(g_Engine);
	pp[9] = mi->DepthStencilFormat;		/* AutoDepthStencilFormat */
	pp[0] = mi->Width;
	pp[1] = mi->Height;
	pp[2] = mi->Format;
	g_ResetPending = 1;
}

void Engine_SetGround(const char *dir, const char *name, const vec3_t *scale) {
	Str_PrintfQuiet(g_GroundPath, MAX_FILEPATH, "%s/%s", dir, name);
	g_GroundScale->x = scale->x;
	g_GroundScale->y = scale->y;
	g_GroundScale->z = scale->z;
}

void Model_Info(const char *name, int n) {
	char pat[MAX_FILEPATH], nm[MAX_FILEPATH];

	Str_Copy(pat, name, sizeof(pat));
	crt_strlwr(pat);
	CScene *e;
	for (e = Scene_Next(NULL); e; e = Scene_Next(e)) {
		Path_Part(nm, e->name, PATH_NAME);
		crt_strlwr(nm);
		if (Wild_Match(pat, nm) && --n < 1) {
			break;
		}
	}
	if (!e) {
		Con_Bluef("Modelo no encontrado: %s\n", name);
		return;
	}
	Con_Verbosef("\nEscena: %s\n", e->name);
	Scene_ShowTree(e, NULL, 0);
	Con_Verbosef("\n");
	void *hInst = App_GetHInstance();
	void *hWnd = App_GetHWnd();
	exe_CreateDialogParamA(hInst, (const char *)0x99, hWnd, GAME_FUNC(Engine_SceneDlgProc, RVA_MODELDLG_PROC), (intptr_t)e);
}

void Models_List(void) {
	UtilScene_Dump((utilPrintf_t)GAME_FUNC(Con_Verbosef, RVA_CON_VERBOSEF));
}

/* the original copies its local also when the timer has no value (then uninitialized) */
int Timer_GetFreq64(int64_t *out) {
	int64_t f = 0;
	int r = Timer_GetFreq(&f);
	*out = f;
	return r != 0;
}

int Timer_GetQPFreq64(int64_t *out) {
	int64_t f = 0;
	g_TimerQPMult = g_TimerFreqMult;
	int r = Timer_GetQPFreq(&f);
	if (out) {
		*out = f;
		return r != 0;
	}
	E3D_LogDebug(g_FileLog, "TimerQPFreq: %d", (int)f);
	return r != 0;
}

int Engine_ToggleOption(int opt) {
	int v = RenderMode(opt, 0, 0);
	RenderMode(opt, v == 0, 1);
	return RenderMode(opt, 0, 0);
}

/*
 * differs from the original on purpose (no blocking waits): the video plays on from the message loop, done (if not
 * NULL) is called with the result of Video_PlayFile when it ends, at once when the video did not start. The frame
 * waits until then, as for the other videos; the caller goes on at once (the original returned after the video).
 */
byte Video_PlayThen(const char *name, void (*done)(int result)) {
	if (Video_Busy() || eng_wait != ENGWAIT_NONE) {
		E3D_LogBlue(g_FileLog, "VideoPlayer: a video plays already: %s", name);
		if (done) {
			done(0);
		}
		return 0;
	}
	int r = Video_PlayFile(name, 0);
	if (r == VIDEO_PENDING) {
		eng_playerDone = done;
		eng_wait = ENGWAIT_PLAYER;
		return 1;
	}
	if (done) {
		done(r);
	}
	return r > 0;
}

byte Video_Play(const char *name, const char *cmd, float arg) {
	(void)arg;
	CHECK(cmd);
	if (!crt_stricmp(cmd, "play")) {
		return Video_PlayThen(name, NULL);
	}
	E3D_LogBlue(g_FileLog, "VideoPlayer: invalid command: %s", cmd);
	return 0;
}

byte Wild_MatchCase(const char *pattern, const char *name, byte ignoreCase) {
	CHECK(!ignoreCase);
	return Wild_Match(pattern, name) != 0;
}

void Ini_GetColor(const char *sect, const char *key, ColorIntens *color, CIniFile *ini) {
	char buf[0x40];
	byte *c = (byte *)&color->rgba;

	if (!CIniFile_ReadText(ini, sect, key, buf, sizeof(buf))) {
		if (Proc_Scan(buf, "&b&b&b&f", c, c + 1, c + 2, &color->intens) != 4) {
			CheckFailed("IniGetColor == 4", __FILE__, __TIMESTAMP__, __LINE__);
		}
	}
}

int Ini_GetPath(const char *sect, const char *key, char *dest, int size, const char *base, CIniFile *ini) {
	char dir[MAX_FILEPATH], rel[MAX_FILEPATH];

	if (CIniFile_ReadText(ini, sect, key, rel, sizeof(rel))) {
		return -1;
	}
	Path_Part(dir, base, PATH_DRIVE | PATH_DIR);
	Bprintf(dest, size, "%s%s", dir, rel);
	return 0;
}

/* the globals of m3d.ini without a better home */
#define g_ShadowMapMultitext GAME_GLOBAL(int, g_ShadowMapMultitext, 0x4b2b00)
#define g_ShadowMapSoft GAME_GLOBAL(int, g_ShadowMapSoft, 0x4b2b04)
#define g_ShadowMapWriteTemp GAME_GLOBAL(int, g_ShadowMapWriteTemp, 0x4b2b08)
#define g_ShadowMapAtten GAME_GLOBAL(float, g_ShadowMapAtten, 0x4b2b0c)
#define g_ShadowMapScale GAME_GLOBAL(float, g_ShadowMapScale, 0x432950)
#define g_ShadowMapRes GAME_GLOBAL(float, g_ShadowMapRes, 0x432954)
#define g_ShadowMapTexX GAME_GLOBAL(int, g_ShadowMapTexX, 0x432958)
#define g_ShadowMapTexY GAME_GLOBAL(int, g_ShadowMapTexY, 0x43295c)
#define g_ShadowMapGamma GAME_GLOBAL(float, g_ShadowMapGamma, 0x432960)
#define g_ShadowMapBrightness GAME_GLOBAL(float, g_ShadowMapBrightness, 0x432964)
#define g_ShadowMapRadiosity GAME_GLOBAL(int, g_ShadowMapRadiosity, 0x4b2b10)
#define g_ShadowMapIllumGlobal GAME_GLOBAL(int, g_ShadowMapIllumGlobal, 0x4b2b14)
#define g_ShadowMapNoLightAmb GAME_GLOBAL(int, g_ShadowMapNoLightAmb, 0x4b2b18)
#define g_ShadowMapModulate4x GAME_GLOBAL(int, g_ShadowMapModulate4x, 0x4b2b1c)
#define g_ShadowMapBounces GAME_GLOBAL(int, g_ShadowMapBounces, 0x432968)
#define g_ShadowMapCoefRad GAME_GLOBAL(float, g_ShadowMapCoefRad, 0x43296c)
#define g_ShadowMapUseTxf GAME_GLOBAL(int, g_ShadowMapUseTxf, 0x4b2b24)
#define g_ShadowsDestAlpha GAME_GLOBAL(int, g_ShadowsDestAlpha, 0x4afdb4)
#define g_FlaresSizeDef GAME_GLOBAL(float, g_FlareSize, 0x432998)
#define g_FlaresAttenDist GAME_GLOBAL(int, g_FlareAttenDist, 0x4d6d90)
#define g_FlaresAttenIni GAME_GLOBAL(float, g_FlareAttenDistIni, 0x43299c)
#define g_FlaresAttenEnd GAME_GLOBAL(float, g_FlareAttenDistEnd, 0x4329a0)
#define g_FlaresFadeIn GAME_GLOBAL(float, g_FlareFadeIn, 0x4329a4)
#define g_FlaresFadeOut GAME_GLOBAL(float, g_FlareFadeOut, 0x4329a8)
#define g_FlaresShowInfo GAME_GLOBAL(int, g_FlareShowInfo, 0x4d6d94)

void Engine_LoadIni(const char *name, const char *dir) {
	static const char *const paths[4] = { "ruta1", "ruta2", "ruta3", "ruta4" };
	static const char *const ext[4] = { "extens1", "extens2", "extens3", "extens4" };
	char tex[MAX_FILEPATH];
	CIniFile ini;

	CIniFile_ctor(&ini);
	if (!CIniFile_Load(&ini, name, dir, 1)) {
		CIniFile_ReadText(&ini, "suelo", "ruta_m3d", g_GroundPath, MAX_FILEPATH);
		Ini_GetColor("suelo", "model_ambient", g_ModelAmbientIni, &ini);
		CIniFile_ReadFloat(&ini, "camara", "forzar_clipNear", &g_ClipNear);
		CIniFile_ReadFloat(&ini, "camara", "forzar_clipFar", &g_ClipFar);
		CIniFile_ReadText(&ini, "cielo", "path", g_SkyPath, MAX_FILEPATH);
		CIniFile_ReadText(&ini, "cielo", "ruta_m3d", g_SkyPath, MAX_FILEPATH);
		Ini_GetPath("cielo", "ruta_rel", g_SkyPath, MAX_FILEPATH, name, &ini);
		CIniFile_ReadFloat(&ini, "cielo", "pos_scale", &g_SkyPosScaleIni);
		CIniFile_ReadFloat(&ini, "cielo", "rotation", &g_SkyRotationIni);
		CIniFile_ReadBool(&ini, "niebla", "activa", &g_FogActiveIni);
		CIniFile_ReadBool(&ini, "niebla", "modo_tabla", &g_FogTableMode);
		CIniFile_ReadBool(&ini, "niebla", "lineal", &g_FogLinear);
		CIniFile_ReadFloat(&ini, "niebla", "rango_ini", &g_FogRangeIni);
		CIniFile_ReadFloat(&ini, "niebla", "rango_fin", &g_FogRangeEnd);
		CIniFile_ReadFloat(&ini, "niebla", "densidad", &g_FogDensityIni);
		Ini_GetColor("niebla", "color", g_FogColorIni, &ini);
		CIniFile_ReadBool(&ini, "mapa_sombras", "usar", &g_ShadowMapUse);
		CIniFile_ReadBool(&ini, "mapa_sombras", "dlg", &g_ShadowMapDlg);
		CIniFile_ReadBool(&ini, "mapa_sombras", "calcular", &g_ShadowMapCalculate);
		CIniFile_ReadBool(&ini, "mapa_sombras", "multitext", &g_ShadowMapMultitext);
		CIniFile_ReadBool(&ini, "mapa_sombras", "sombras_suaves", &g_ShadowMapSoft);
		CIniFile_ReadBool(&ini, "mapa_sombras", "escr_mapas_temp", &g_ShadowMapWriteTemp);
		CIniFile_ReadFloat(&ini, "mapa_sombras", "aten_sombras", &g_ShadowMapAtten);
		CIniFile_ReadFloat(&ini, "mapa_sombras", "escala", &g_ShadowMapScale);
		CIniFile_ReadFloat(&ini, "mapa_sombras", "resolucion", &g_ShadowMapRes);
		CIniFile_ReadInt(&ini, "mapa_sombras", "text_tam_x", &g_ShadowMapTexX);
		CIniFile_ReadInt(&ini, "mapa_sombras", "text_tam_y", &g_ShadowMapTexY);
		CIniFile_ReadFloat(&ini, "mapa_sombras", "gamma", &g_ShadowMapGamma);
		CIniFile_ReadFloat(&ini, "mapa_sombras", "brillo", &g_ShadowMapBrightness);
		CIniFile_ReadBool(&ini, "mapa_sombras", "radiosidad", &g_ShadowMapRadiosity);
		CIniFile_ReadBool(&ini, "mapa_sombras", "ilum_global", &g_ShadowMapIllumGlobal);
		CIniFile_ReadBool(&ini, "mapa_sombras", "no_luz_amb", &g_ShadowMapNoLightAmb);
		CIniFile_ReadBool(&ini, "mapa_sombras", "modulate4x", &g_ShadowMapModulate4x);
		CIniFile_ReadInt(&ini, "mapa_sombras", "rebotes", &g_ShadowMapBounces);
		CIniFile_ReadFloat(&ini, "mapa_sombras", "coef_rad", &g_ShadowMapCoefRad);
		CIniFile_ReadBool(&ini, "mapa_sombras", "calc_lmap2", &g_CalcLmap2);
		CIniFile_ReadBool(&ini, "mapa_sombras", "usar_txf", &g_ShadowMapUseTxf);
		CIniFile_ReadBool(&ini, "sombras_stencil", "usar", &g_ShadowsStencil);
		CIniFile_ReadBool(&ini, "sombras_stencil", "dest_alpha", &g_ShadowsDestAlpha);
		CIniFile_ReadFloat(&ini, "flares", "tam_def", &g_FlaresSizeDef);
		CIniFile_ReadBool(&ini, "flares", "aten_dist", &g_FlaresAttenDist);
		CIniFile_ReadFloat(&ini, "flares", "aten_dist_ini", &g_FlaresAttenIni);
		CIniFile_ReadFloat(&ini, "flares", "aten_dist_fin", &g_FlaresAttenEnd);
		CIniFile_ReadFloat(&ini, "flares", "fundido_ent", &g_FlaresFadeIn);
		CIniFile_ReadFloat(&ini, "flares", "fundido_sal", &g_FlaresFadeOut);
		CIniFile_ReadBool(&ini, "flares", "ver_info", &g_FlaresShowInfo);
		for (int i = 0; i < 4; i++) {
			if (!CIniFile_ReadText(&ini, "texturas", paths[i], tex, sizeof(tex))) {
				Tex_AddPath(tex);
			}
		}
		for (int i = 0; i < 4; i++) {
			if (!CIniFile_ReadText(&ini, "texturas", ext[i], tex, sizeof(tex))) {
				Tex_AddExtension(tex);
			}
		}
	}
	CIniFile_dtor(&ini);
}
