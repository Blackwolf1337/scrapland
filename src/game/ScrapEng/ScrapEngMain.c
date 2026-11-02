/*
 * ScrapEngMain.cpp of the game: the glue between the game and the engine3d, see include/scrap/scrapmain.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/cmd.h"
#include "scrap/app.h"
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
#include "scrap/platform.h"
#include "scrap/staticinit.h"

#define g_StartVideo (((cString *)&GVar_PlayVideo.value))
#define g_StartVideoFlags (GVar_PlayVideo.base.flags)
#define g_RenderLightmap (GVar_RenderLightmap.value.i)
#define g_GvCollisionEpsilon (GVar_CollisionEpsilon.value.f)	/* CollisionEpsilon */
#define g_FullScreen (GVar_FullScreen.value.i)
#define g_ComputeMapTextSaving (GVar_ComputeMapTextSaving.value.i)
#define g_ViewerCameraAnim (GVar_ViewerCameraAnim.value.i)
#define g_ShowMapCollision (GVar_ShowMapCollision.value.i)
#define g_ShowMapWireFrame (GVar_ShowMapWireFrame.value.i)
#define g_RenderUseVB (GVar_RenderUseVB.value.i)
#define g_RenderModels (GVar_RenderModels.value.i)
#define g_ModelAmbient (GVar_ModelAmbient.value.u)	/* D3D color, 0: the one of m3d.ini */
#define g_VideoWidth (GVar_VideoWidth.value.i)
#define g_VideoHeight (GVar_VideoHeight.value.i)
#define g_VideoBPP (GVar_VideoBPP.value.i)
static cGVar GVar_ViewerCameraAnim;
static cGVar GVar_VideoWidth;
static cGVar GVar_VideoHeight;
static cGVar GVar_VideoBPP;
static cGVar GVar_ShowMapWireFrame;
static cGVar GVar_ShowMapCollision;
static cGVar GVar_RenderUseVB;
static cGVar GVar_RenderModels;
static cGVar GVar_RenderLightmap;
static cGVar GVar_PlayVideo;
static cGVar GVar_ModelAmbient;
static cGVar GVar_FullScreen;
static cGVar GVar_ComputeMapTextSaving;
static cGVar GVar_CollisionEpsilon;

extern cGVar GVar_Viewer;
#define g_IsViewer (GVar_Viewer.value.i)
#define D3DFMT_A8R8G8B8 21
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

struct CD3DApplication *g_Engine;

static D3DAdapterInfo *Engine_AdapterInfo(void) {
	CHECK(g_Engine);
	D3DAdapterInfo *ai = &g_Engine->adapters[g_Engine->adapter];
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

static CGameCamera *g_ViewerCamera;
static CPsaImage *g_Watermark;
CWorld *g_World;

static byte Engine_FinalCleanup(void) {
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

/* the state of the engine glue */
static CGameCamera *g_CamCurrent;

CGameCamera *Camera_GetCurrent(void) {
	return g_CamCurrent;
}

/* the bits of the color of a D3DFORMAT (32 or 16, 0 for the others) */
static int Format_Bpp(int fmt) {
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

int g_LightmapOff;
static float g_ClipNear;
static float g_ClipFar;
static char g_SkyPath[260];
static float g_SkyPosScaleIni;
/* m3d.ini of the level (Engine_LoadIni) */
static char g_GroundPath[260];
static float g_SkyRotationIni;
static int g_FogActiveIni;
static int g_FogTableMode;
static ColorIntens g_FogColorIni;
static ColorIntens g_ModelAmbientIni;
static int g_FogLinear = 1;
static float g_FogRangeIni = 0.5f;
static float g_FogRangeEnd = 1.0f;
static float g_FogDensityIni = 0.5f;

static vec3_t g_GroundScale = { .x = 1.0f, .y = 1.0f, .z = 1.0f };

int g_MaxLightmap = 2;

/* the three callbacks of the engine: once after the device, each frame, at the end */
static void Engine_LoadIni(const char *name, const char *dir);
static byte Engine_OneTimeInit(void) {
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
	g_FogColorIni.intens = 0.0f;
	g_ModelAmbientIni.intens = 0.0f;
	Engine_LoadIni(g_M3dIni, NULL);
	Engine_LoadIni(g_GroundPath, ".ini");
	ParticleSystem_LoadIni(NULL, NULL);
	ParticleSystem_LoadIni(g_GroundPath, "particles.ini");
	ParticleSystem_LoadIni("bmp/particles.ini", NULL);
	void *p = operator_new(sizeof(CWorld));
	g_World = p ? CWorld_ctor(p) : NULL;
	WorldInfo_Reset();
	CWorld_LoadSky(g_World, g_SkyPath);
	CWorld_LoadGroundScale(g_World, g_GroundPath, (&g_GroundScale));
	CWorld_SetSkyPos(g_World, g_SkyPosScaleIni);
	CWorld_SetSkyRot(g_World, g_SkyRotationIni);
	CWorld_SetClip(g_World, g_ClipNear, g_ClipFar);
	g_World->fog.active = g_FogActiveIni != 0;
	CFog_SetRange(&g_World->fog, g_FogRangeIni, g_FogRangeEnd);
	g_World->fog.table = g_FogTableMode != 0;
	g_World->fog.density = g_FogDensityIni;
	g_World->fog.mode = g_FogLinear ? 3 : 1;	/* D3DFOG_LINEAR, D3DFOG_EXP */
	if (g_FogColorIni.intens != 0.0f) {
		CFog_SetColor(&g_World->fog, g_FogColorIni.rgba, g_FogColorIni.intens);
	}
	Cmd_ReadFile("engine.txt");
	Fonts_Load();
	Graphic2D_SetScreen(640, 480);
	CHECK(!g_ViewerCamera);
	p = operator_new(sizeof(CGameCamera));
	g_ViewerCamera = p ? CGameCamera_ctor(p) : NULL;
	CCamera *cam = CWorld_GetCameraGround(World_Get(), 1);
	if (!cam) {
		cam = World_Get()->cam;
	}
	CGameCamera_FromCamera(g_ViewerCamera, cam);
	p = operator_new(sizeof(CPsaImage));
	g_Watermark = p ? CPsaImage_ctor(p, "bmp/norm1.alpha.psa", 0x10008) : NULL;
	return 1;
}

const int g_EngineVersion = 5624;
static byte g_MapTextSavingOn;
int g_TimerQPMult = 1;

static byte Engine_FrameMove(void);
void Engine_Init(void) {
	int64_t freq;

	CHECK(!g_Engine);
	Con_Printf("Engine 3D initializing (Version %d)\n", g_EngineVersion);
	if (APP_DEDICATED(g_pApp)) {
		g_RenderOff = 1;
	}
	void *hInst = App_GetHInstance();
	void *hWnd = App_GetHWnd();
	g_TimerQPMult = g_TimerFreqMult;
	Timer_GetQPFreq(&freq);
	E3D_LogDebug((&g_FileLog), "TimerQPFreq: %d", (int)freq);
	Engine3D_Create(hInst, hWnd, (void *)Engine_OneTimeInit, (void *)Engine_FrameMove,
		(void *)Engine_FinalCleanup, 0);
	g_Engine->fullscreen = g_FullScreen && !APP_DEDICATED(g_pApp);
	g_Engine->width = g_VideoWidth;
	g_Engine->height = g_VideoHeight;
	g_Engine->bpp = g_VideoBPP;
	g_MapTextSavingOn = g_ComputeMapTextSaving == 1;
	CD3DApplication *m = g_Engine;
	if (!APP_DEDICATED(g_pApp)) {
		if (m->vftable->Create(m, hInst) < 0) {
			Err_Printf(1, "Engine3D initialization error");
		}
	} else {
		m->vftable->OneTimeSceneInit(m);
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
	CD3DApplication *m = g_Engine;
	if (m) {
		return (int)(((uintptr_t)m & ~(uintptr_t)0xff) | (m->windowed == 0));
	}
	if (g_FullScreen && !APP_DEDICATED(g_pApp)) {
		return 1;
	}
	return 0;
}

void Engine_LevelEnd(void) {
	CHECK(g_Engine);
	if (g_Engine->vftable->DeleteDeviceObjects(g_Engine) < 0) {
		Err_Printf(1, "ERROR: LevelEndEngine3D()");
	}
	if (g_Engine->vftable->FinalCleanup(g_Engine) < 0) {
		Err_Printf(1, "ERROR: LevelEndEngine3D()");
	}
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
	if (*g_StartVideo) {
		if (**g_StartVideo && CD3DApp_ResetDevice(g_Engine) < 0) {
			E3D_LogBlue((&g_FileLog), "ERROR: 'ResetDevice()' failed");
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

static int g_VideoBufferSize;
static int g_MapTextSaving;
byte g_DrawProgressBar;
static void *g_MapTextDataFile;

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
	void *dev = g_Engine->device;
	g_Engine->device = NULL;
	if (g_Engine->vftable->OneTimeSceneInit(g_Engine) < 0) {
		Err_Printf(1, "ERROR: LevelInitEngine3D()");
	}
	g_Engine->device = dev;
	if (!APP_DEDICATED(g_pApp)) {
		if (g_Engine->vftable->InitDeviceObjects(g_Engine) < 0) {
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
static void Camera_SaveRestore(CGameCamera *cam, byte save) {
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
		Con_Debugf("%*s  %3d %3d %3d  c:%-8s  f:%08X  a:%04X", 0x30 - len - indent, "", n->index, n->slot, n->renderNode,
			Obj3D_ClassName(CNode3D_Class(n)), n->flags, (uint16_t)n->aniLocked);
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
	E3D_LogBlue((&g_FileLog), "MusicPlayer: invalid command: %s", cmd);
	return 0;
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

typeof(void (*)(void)) g_EngCallbacks[4];

byte g_CallbacksOn = 1;

static byte Engine_DebugKeys(void);
static byte Engine_FrameMove(void) {
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
		g_Watermark->color = 0;
		CPsaImage_Draw(g_Watermark, 321.0f, 11.0f);
		g_Watermark->color = 0xffffff;
		CPsaImage_Draw(g_Watermark, 320.0f, 10.0f);
	}
	return 1;
}

static byte g_ResetPending;

static void Engine_ResetAll(void);
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

static cGVar GVar_ViewerClipNear;

/* the free camera of the viewer (Gvars) */
#define g_ViewerClipNear (GVar_ViewerClipNear.value.f)
static cGVar GVar_ViewerClipFar;

#define g_ViewerClipFar (GVar_ViewerClipFar.value.f)
static cGVar GVar_ViewerFOV;

#define g_ViewerFOV (GVar_ViewerFOV.value.f)	/* degrees, 0: the camera keeps its own */
static cGVar GVar_ViewerCameraTgt;

#define g_ViewerCameraTgt (GVar_ViewerCameraTgt.value.i)	/* orbits around the target */
static cGVar GVar_ViewerCameraVel;

#define g_ViewerCameraVel (GVar_ViewerCameraVel.value.f)
static cGVar GVar_ViewerCameraInv;

#define g_ViewerCameraInv (GVar_ViewerCameraInv.value.i)	/* the pitch not inverted */
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

float g_FrameMs;
static float g_ViewerSpeed = 1.0f;

/* the viewer camera by the debug actions: turned (or panned with the pan action) in free mode, orbiting the
 * target (or moved up and down with the pan action) in target mode */
static void ViewerCamera_Move(CGameCamera *cam) {
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
		Debug_DrawAxes((&g_MatIdentity), &cam->target, 100.0f);
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

cGVar g_GvEnvBump;
cGVar g_GvGlowMap;
cGVar g_GvEnvMap;

static void Engine_ResetTextures(void);
static void GVar_SetIntVerbose(cGVar *v, int value);
static byte Engine_DebugKeys(void) {
	if (!g_ViewerCameraAnim) {
		Camera_SetCurrent(g_ViewerCamera);
		ViewerCamera_Move(g_ViewerCamera);
	}
	if (Plat_KeyDown(VK_CONTROL)) {
		return 1;
	}
	byte shift = Plat_KeyDown(VK_SHIFT);
	if (shift && Plat_KeyDown(VK_F8)) {
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
		GVar_SetIntVerbose((&g_GvEnvMap), g_GvEnvMap.value.i == 0);
	}
	if (DebugAction(DBG_ENVBUMP)) {
		GVar_SetIntVerbose((&g_GvEnvBump), g_GvEnvBump.value.i == 0);
	}
	if (DebugAction(DBG_GLOWMAP)) {
		GVar_SetIntVerbose((&g_GvGlowMap), g_GvGlowMap.value.i == 0);
	}
	if (DebugAction(DBG_CAMERA) && !shift) {
		Camera_SaveRestore(g_ViewerCamera, 0);
	}
	if (DebugAction(DBG_CAMERA) && shift) {
		Camera_SaveRestore(g_ViewerCamera, 1);
	}
	return 1;
}

static char g_DVFName[256];

/* Scrap.ProcessDVF: 1 starts the tiled shot; the AVI recorder of 2, 3 and 4 is gone (2 and 4 still clear the name) */
void DVF_Process(const char *name, int arg) {
	if (!name || !*name) {
		name = "";
	}
	Str_Copy(g_DVFName, name, 0x100);
	if (arg == 1) {
		g_TileOn = 1;
		return;
	}
	if (arg == 2 || arg == 4) {
		g_DVFName[0] = 0;
	}
}

void Loading_Fade(void) {
	Splash_Show("", 0, 3000, 1);
}

static void Engine_ResetAll(void) {
	Con_Printf("ResetEngine3D_All\n");
	CHECK(g_Engine->device);
	Splash_Show("2D/Loading/00Reset.dds", 1, 500, 0);
	CD3DApplication *m = g_Engine;
	m->vsync = g_VSync;
	if (m->windowed) {
		m->presentParams.FullScreen_PresentationInterval = 0;
	} else {
		m->presentParams.FullScreen_PresentationInterval = m->vsync ? 0 : D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	if (CD3DApp_Reset(g_Engine) < 0) {
		E3D_LogBlue((&g_FileLog), "ERROR: ResetEngine3D_All()");
	}
}

static void Engine_ResetTextures(void) {
	Con_Printf("ResetEngine3D_Textures\n");
	CHECK(g_Engine->device);
	Texture_ReleaseAllD3D();
	CMatList_Release((&g_MatList));
	CMatList_Prepare((&g_MatList), g_Engine->device);
}

int Eng_RenderTargetEnd(void) {
	return RenderTarget_End(NULL);
}

byte Eng_ResetViewRect(void) {
	return Render_SetViewRect(NULL, 1);
}

uint32_t g_AmbientD3D;
byte g_UseVB = 1;
byte g_ModelsOn = 1;

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
	if (g_ModelAmbientIni.intens == 0.0f) {
		if (!amb) {
			goto done;
		}
	} else if (!amb) {
		amb = Color_ToD3D((&g_ModelAmbientIni));
	}
	g_AmbientD3D = amb;
	void *dev = g_pd3dDevice;
	Rd_DevSetRenderState(dev, D3DRS_AMBIENT, amb);
done:
	if (g_EngCallbacks[1] && g_CallbacksOn) {
		g_EngCallbacks[1]();
	}
	return 1;
}

char g_ScreenshotName[256];

void Screenshot_Take(const char *name) {
	g_TileOn = 1;
	if (!name || !*name) {
		name = "";
	}
	Str_Copy(g_ScreenshotName, name, 0x100);
}

static int g_LoadingScreen = -1;

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

static void GVar_SetIntVerbose(cGVar *v, int value) {
	v->value.i = value;
	Con_Verbosef("%s = %d\n", v->base.name, value);
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
	D3DPresentParams *pp = &g_Engine->presentParams;
	pp->AutoDepthStencilFormat = mi->DepthStencilFormat;
	pp->BackBufferWidth = mi->Width;
	pp->BackBufferHeight = mi->Height;
	pp->BackBufferFormat = mi->Format;
	g_ResetPending = 1;
}

void Engine_SetGround(const char *dir, const char *name, const vec3_t *scale) {
	Str_PrintfQuiet(g_GroundPath, MAX_FILEPATH, "%s/%s", dir, name);
	g_GroundScale.x = scale->x;
	g_GroundScale.y = scale->y;
	g_GroundScale.z = scale->z;
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
}

void Models_List(void) {
	UtilScene_Dump((utilPrintf_t)Con_Verbosef);
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
	E3D_LogDebug((&g_FileLog), "TimerQPFreq: %d", (int)f);
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
		E3D_LogBlue((&g_FileLog), "VideoPlayer: a video plays already: %s", name);
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
	E3D_LogBlue((&g_FileLog), "VideoPlayer: invalid command: %s", cmd);
	return 0;
}

static void Ini_GetColor(const char *sect, const char *key, ColorIntens *color, CIniFile *ini) {
	char buf[0x40];
	byte *c = (byte *)&color->rgba;

	if (!CIniFile_ReadText(ini, sect, key, buf, sizeof(buf))) {
		if (Proc_Scan(buf, "&b&b&b&f", c, c + 1, c + 2, &color->intens) != 4) {
			CheckFailed("IniGetColor == 4", __FILE__, __TIMESTAMP__, __LINE__);
		}
	}
}

/* the path of key relative to the directory of base into dest; 0 or -1 */
static int Ini_GetPath(const char *sect, const char *key, char *dest, int size, const char *base, CIniFile *ini) {
	char dir[MAX_FILEPATH], rel[MAX_FILEPATH];

	if (CIniFile_ReadText(ini, sect, key, rel, sizeof(rel))) {
		return -1;
	}
	Path_Part(dir, base, PATH_DRIVE | PATH_DIR);
	Bprintf(dest, size, "%s%s", dir, rel);
	return 0;
}

/* the globals of m3d.ini without a better home */
extern int g_ShadowMapMultitext;
extern float g_ShadowMapScale;
extern float g_ShadowMapRes;
extern int g_ShadowMapTexX;
extern int g_ShadowMapTexY;
extern float g_ShadowMapGamma;
extern float g_ShadowMapBrightness;
extern int g_ShadowMapRadiosity;
extern int g_ShadowMapIllumGlobal;
extern int g_ShadowMapModulate4x;
float g_FlareSize = 200.0f;

#define g_FlaresSizeDef g_FlareSize
int g_FlareAttenDist;

#define g_FlaresAttenDist g_FlareAttenDist
static float g_FlareAttenDistIni = 200.0f;

#define g_FlaresAttenIni g_FlareAttenDistIni
float g_FlareAttenDistEnd = 3000.0f;

#define g_FlaresAttenEnd g_FlareAttenDistEnd
float g_FlareFadeIn = 8.0f;

#define g_FlaresFadeIn g_FlareFadeIn
float g_FlareFadeOut = 8.0f;

#define g_FlaresFadeOut g_FlareFadeOut
int g_FlareShowInfo;

#define g_FlaresShowInfo g_FlareShowInfo

int g_ShadowMapSoft;
static int g_ShadowMapWriteTemp;
float g_ShadowMapAtten;
int g_ShadowMapNoLightAmb;
int g_ShadowMapUseTxf;
static int g_ShadowsDestAlpha;
int g_ShadowMapDlg = 1;
int g_ShadowMapBounces = 1;
float g_ShadowMapCoefRad = 0.1f;

/* m3d.ini: [suelo], [camara], [cielo], [niebla], [mapa_sombras], [sombras_stencil], [flares], [texturas] */
static void Engine_LoadIni(const char *name, const char *dir) {
	static const char *const paths[4] = { "ruta1", "ruta2", "ruta3", "ruta4" };
	static const char *const ext[4] = { "extens1", "extens2", "extens3", "extens4" };
	char tex[MAX_FILEPATH];
	CIniFile ini;

	CIniFile_ctor(&ini);
	if (!CIniFile_Load(&ini, name, dir, 1)) {
		CIniFile_ReadText(&ini, "suelo", "ruta_m3d", g_GroundPath, MAX_FILEPATH);
		Ini_GetColor("suelo", "model_ambient", (&g_ModelAmbientIni), &ini);
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
		Ini_GetColor("niebla", "color", (&g_FogColorIni), &ini);
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

char str_M3dIni[128] = "m3d.ini";
static cGVar GVar_RenderUseVB;
cGVar GVar_ShowSplashScreen;
static cGVar GVar_ModelAmbient;
static cGVar GVar_ShowMapWireFrame;
/* the engine variables (Gvars) */
static cGVar GVar_RenderLightmap;
static cGVar GVar_ComputeMapTextSaving;
static cGVar GVar_RenderModels;
cGVar GVar_ProgressBar;
static cGVar GVar_PlayVideo;
static cGVar GVar_VideoHeight;
static cGVar GVar_VideoWidth;
static cGVar GVar_VideoBPP;
static cGVar GVar_CollisionEpsilon;
static cGVar GVar_ViewerCameraAnim;
static cGVar GVar_ShowMapCollision;
static cGVar GVar_FullScreen;
cGVar GVar_VSync;
cGVar GVar_isDemo;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_isDemo, &GVar_isDemo, "isDemo", 1, "Retail version", 0)
INIT_GVAR_INT(Init_GVar_ComputeMapTextSaving, &GVar_ComputeMapTextSaving, "ComputeMapTextSaving", 0, "if 1 writes the map text saving info", 0)
INIT_GVAR_INT(Init_GVar_ViewerCameraAnim, &GVar_ViewerCameraAnim, "ViewerCameraAnim", 0, "Viewer camera anim (0/1)", 0)
INIT_GVAR_FLOAT(Init_GVar_ViewerCameraVel, &GVar_ViewerCameraVel, "ViewerCameraVel", 0, "Viewer camera velocity (m/s)\n", 1.0f)
INIT_GVAR_INT(Init_GVar_ViewerCameraTgt, &GVar_ViewerCameraTgt, "ViewerCameraTgt", 0, "Viewer camera mode: 0 = free, 1 = target", 0)
INIT_GVAR_INT(Init_GVar_ViewerCameraInv, &GVar_ViewerCameraInv, "ViewerCameraInv", 0, "Viewer camera rotation: 0 = normal, 1 = inverted", 1)
INIT_GVAR_FLOAT(Init_GVar_ViewerClipNear, &GVar_ViewerClipNear, "ViewerClipNear", 0, "Viewer camera near clip plane", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_ViewerClipFar, &GVar_ViewerClipFar, "ViewerClipFar", 0, "Viewer camera far clip plane", 800000.0f)
INIT_GVAR_FLOAT(Init_GVar_ViewerFOV, &GVar_ViewerFOV, "ViewerFOV", 0, "Viewer camera fov (0.0 = original fov)", 0.0f)
INIT_GVAR_INT(Init_GVar_ShowMapWireFrame, &GVar_ShowMapWireFrame, "ShowMapWireFrame", 0, "Show map wireframe mode (0/1/2)\n", 0)
INIT_GVAR_INT(Init_GVar_RenderLightmap, &GVar_RenderLightmap, "RenderLightmap", 0, "Render lightmap mode (0/1/2)", 1)
INIT_GVAR_INT(Init_GVar_ShowMapCollision, &GVar_ShowMapCollision, "ShowMapCollision", 0, "Show map collision planes (0/1)", 0)
INIT_GVAR_INT(Init_GVar_RenderModels, &GVar_RenderModels, "RenderModels", 0, "Render models (0/1)", 1)
INIT_GVAR_INT(Init_GVar_RenderUseVB, &GVar_RenderUseVB, "RenderUseVB", 0, "Use Vertex Buffers (0/1)", 1)
INIT_GVAR_INT(Init_GVar_ModelAmbient, &GVar_ModelAmbient, "ModelAmbient", 0, "Extra ambient light for models (def. 0)", 0)
INIT_GVAR_FLOAT(Init_GVar_CollisionEpsilon, &GVar_CollisionEpsilon, "CollisionEpsilon", 0, "Collision epsilon\n", 1.0f)
INIT_GVAR_INT(Init_GVar_VideoBPP, &GVar_VideoBPP, "VideoBPP", 6, "Bits per pixel\n", 32)
INIT_GVAR_INT(Init_GVar_VideoHeight, &GVar_VideoHeight, "VideoHeight", 6, "Screen height", 1080)
INIT_GVAR_INT(Init_GVar_VideoWidth, &GVar_VideoWidth, "VideoWidth", 6, "Screen width", 1920)
INIT_GVAR_INT(Init_GVar_FullScreen, &GVar_FullScreen, "FullScreen", 6, "0 = Windowed mode, 1 = Fullscreen mode", 1)
FINI_GVAR_STRING(Fini_GVar_PlayVideo, &GVar_PlayVideo)
INIT_GVAR_STRING(Init_GVar_PlayVideo, Fini_GVar_PlayVideo, &GVar_PlayVideo, "PlayVideo", 4, "Video played at init.", "")
INIT_GVAR_INT(Init_GVar_ShowSplashScreen, &GVar_ShowSplashScreen, "ShowSplashScreen", 0, "Show loading screen (0/1)", 1)
INIT_GVAR_INT(Init_GVar_ProgressBar, &GVar_ProgressBar, "ProgressBar", 0, "Progress bar reference value (def. 1000)", 1000)
INIT_GVAR_INT(Init_GVar_VSync, &GVar_VSync, "VSync", 2, "Fullscreen Vertical Sync (0/1)", 0)
INIT_GVAR_INT(Init_GVar_R_EnvMap, &g_GvEnvMap, "R_EnvMap", 0, "Use EnvMap (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_EnvBump, &g_GvEnvBump, "R_EnvBump", 0, "Use EnvBump (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_GlowMap, &g_GvGlowMap, "R_GlowMap", 0, "Use GlowMap (0/1)", 1)
