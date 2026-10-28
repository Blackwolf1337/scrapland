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
#include "scrap/grafico2d.h"
#include "scrap/input.h"
#include "scrap/leem3d.h"
#include "scrap/mallad3d.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/mundo.h"
#include "scrap/niebla.h"
#include "scrap/render.h"
#include "scrap/rendertarget.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/sistpart.h"
#include "scrap/suelo.h"
#include "scrap/texto.h"
#include "scrap/textura.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"
#include "scrap/video.h"
#include "scrap/win32.h"

#define g_IsViewer GAME_DATA(int, 0x467b34)
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

static D3DAdapterInfo *Motor_AdapterInfo(void) {
	CHECK(g_Motor);
	D3DAdapterInfo *ai = MOTOR_ADAPTERINFO(g_Motor);
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
};
static int eng_wait;
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
	}
	return 1;
}

void Engine_Close(void) {
	CHECK(g_Motor);
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
	if (g_IsDemo) {
		Splash_Show("2D/Loading/00Promo01.dds", 1, 6000, 1);
		if (g_IsDemo) {
			Splash_Show("2D/Loading/00Promo02.dds", 1, 20000, 1);
		}
	}
	CD3DApp_Cleanup(g_Motor);
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
	Camera_SetActual(NULL);
	CGameCamera *cam = g_ViewerCamera;
	if (cam) {
		CGameCamera_dtor(cam);
		operator_delete_sized(cam, 0x3c);
		g_ViewerCamera = NULL;
	}
	Fonts_Unload();
	CMundo *m = g_Mundo;
	if (m) {
		CMundo_dtor(m);
		operator_delete_sized(m, sizeof(CMundo));
		g_Mundo = NULL;
	}
	WorldInfo_Reset();
	SistPart_LoadIni(NULL, NULL);
	return 1;
}

CGameCamera *Camera_GetActual(void) {
	return g_CamActual;
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
	D3DAdapterInfo *ai = Motor_AdapterInfo();
	return ai->devices[ai->dwCurrentDevice].dwCurrentMode;
}

int Video_GetModes(int i, char *buf) {
	D3DAdapterInfo *ai = Motor_AdapterInfo();
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
	g_SueloTrisPerNode = 8000000;
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
	g_SombrasStencil = g_StencilShadows > 0;
	g_CieloPath[0] = 0;
	g_CieloPosScale = 0.0f;
	g_CieloRotation = 0.0f;
	g_NieblaColor->intens = 0.0f;
	g_ModelAmbientIni->intens = 0.0f;
	Engine_LoadIni(g_M3dIni, NULL);
	Engine_LoadIni(g_SueloPath, ".ini");
	SistPart_LoadIni(NULL, NULL);
	SistPart_LoadIni(g_SueloPath, "particles.ini");
	SistPart_LoadIni("bmp/particles.ini", NULL);
	void *p = operator_new(sizeof(CMundo));
	g_Mundo = p ? CMundo_ctor(p) : NULL;
	WorldInfo_Reset();
	CMundo_LoadCielo(g_Mundo, g_CieloPath);
	CMundo_LoadSueloEscala(g_Mundo, g_SueloPath, g_SueloEscala);
	CMundo_SetCieloPos(g_Mundo, g_CieloPosScale);
	CMundo_SetCieloRot(g_Mundo, g_CieloRotation);
	CMundo_SetClip(g_Mundo, g_ClipNear, g_ClipFar);
	g_Mundo->niebla.activa = g_NieblaActiva != 0;
	CNiebla_SetRango(&g_Mundo->niebla, g_NieblaRangoIni, g_NieblaRangoFin);
	g_Mundo->niebla.tabla = g_NieblaModoTabla != 0;
	g_Mundo->niebla.densidad = g_NieblaDensidad;
	g_Mundo->niebla.modo = g_NieblaLineal ? 3 : 1;	/* D3DFOG_LINEAR, D3DFOG_EXP */
	if (g_NieblaColor->intens != 0.0f) {
		CNiebla_SetColor(&g_Mundo->niebla, g_NieblaColor->rgba, g_NieblaColor->intens);
	}
	Cmd_ReadFile("engine.txt");
	Fonts_Load();
	Grafico2D_SetPantalla(640, 480);
	CHECK(!g_ViewerCamera);
	p = operator_new(0x3c);
	g_ViewerCamera = p ? CGameCamera_ctor(p) : NULL;
	CCamara *cam = CMundo_GetCamaraSuelo(Mundo_Get(), 1);
	if (!cam) {
		cam = Mundo_Get()->cam;
	}
	CGameCamera_FromCamara(g_ViewerCamera, cam);
	p = operator_new(0x44);
	g_Watermark = p ? CPsaImage_ctor(p, "bmp/norm1.alpha.psa", 0x10008) : NULL;
	return 1;
}

void Engine_Init(void) {
	int64_t freq;

	CHECK(!g_Motor);
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
	Engine3D_Create(hInst, hWnd, Game_Ptr(RVA_ENGINE_ONETIMEINIT), Game_Ptr(RVA_ENGINE_FRAMEMOVE),
		Game_Ptr(RVA_ENGINE_FINALCLEANUP), 0);
	MOTOR_FULLSCREEN(g_Motor) = g_FullScreen && !APP_DEDICATED(g_pApp);
	MOTOR_WIDTH(g_Motor) = g_VideoWidth;
	MOTOR_HEIGHT(g_Motor) = g_VideoHeight;
	MOTOR_BPP(g_Motor) = g_VideoBPP;
	g_MapTextSavingOn = g_ComputeMapTextSaving == 1;
	byte *m = g_Motor;
	if (!APP_DEDICATED(g_pApp)) {
		if (((int(__thiscall *)(byte *, void *))(*(void ***)m)[MOTOR_VT_CREATE / 4])(m, hInst) < 0) {
			Err_Printf(1, "Engine3D initialization error");
		}
	} else {
		MOTOR_CALL(m, MOTOR_VT_RESTORE);
	}
	/* differs from the original on purpose (no blocking waits): a video plays on from the message loop, the steps after
	 * it run from Engine_VideoResume */
	eng_initStep = 0;
	if (!Engine_InitVideos()) {
		eng_wait = ENGWAIT_INIT;
	}
}

/* al from the window flag with the rest of eax from g_Motor, like the original's sete */
int Engine_IsFullScreen(void) {
	byte *m = g_Motor;
	if (m) {
		return (int)(((uintptr_t)m & ~(uintptr_t)0xff) | (MOTOR_WINDOWED(m) == 0));
	}
	if (g_FullScreen && !APP_DEDICATED(g_pApp)) {
		return 1;
	}
	return 0;
}

void Engine_LevelEnd(void) {
	CHECK(g_Motor);
	if (MOTOR_CALL(g_Motor, MOTOR_VT_INVALIDATE) < 0) {
		Err_Printf(1, "ERROR: LevelEndEngine3D()");
	}
	if (MOTOR_CALL(g_Motor, MOTOR_VT_DELETE) < 0) {
		Err_Printf(1, "ERROR: LevelEndEngine3D()");
	}
	Splash_Show("2D/Loading/00Black.dds", 0, 0, 0);
	if (*g_StartVideo) {
		if (**g_StartVideo && CD3DApp_ResetDevice(g_Motor) < 0) {
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
	char path[MAX_RUTA];

	CHECK(g_Motor);
	g_VideoBufferSize = 0;
	g_MapTextSaving = 0;
	const char *sep = strrchr(g_SueloPath, '/');
	if (g_ComputeMapTextSaving) {
		static const char name[] = "/MapTextData.ini";
		size_t n = sep ? (size_t)(sep - g_SueloPath) : 0;
		/* the original copies without a bound (strncpy, strcat) */
		if (n > sizeof(path) - sizeof(name)) {
			n = sizeof(path) - sizeof(name);
		}
		memset(path, 0, sizeof(path));
		memcpy(path, g_SueloPath, n);
		memcpy(path + strlen(path), name, sizeof(name));
		g_MapTextDataFile = File_OpenRel(path, "wt");
	}
	g_DrawProgressBar = 1;
	g_ProgressBarCount = 0;
	Loading_Step(0);
	void *dev = MOTOR_DEV(g_Motor);
	MOTOR_DEV(g_Motor) = NULL;
	if (MOTOR_CALL(g_Motor, MOTOR_VT_RESTORE) < 0) {
		Err_Printf(1, "ERROR: LevelInitEngine3D()");
	}
	MOTOR_DEV(g_Motor) = dev;
	if (!APP_DEDICATED(g_pApp)) {
		if (MOTOR_CALL(g_Motor, MOTOR_VT_RESTORE2) < 0) {
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
	char name[MAX_RUTA] = "camera.set";
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

void Escena_ShowTree(CEscena *esc, CNodo3D *padre, int indent) {
	CHECK(esc);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (n->padre != padre) {
			continue;
		}
		Con_Verbosef("%*s%s", indent + 2, "", n->name);
		int len = (int)strlen(n->name);
		Con_Debugf("%*s  %3d %3d %3d  c:%-8s  f:%08X  a:%04X", 0x30 - len - indent, "", n->indice, n->i48, n->nodoR,
			Obj3D_ClaseName(CNodo3D_Clase(n)), n->flags, (uint16_t)n->i50);
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI) {
			CMallaTri *mt = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
			if (mt->pSkin) {
				Con_Debugf("  [skin]");
			}
		}
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLAD3D) {
			CMallaD3D *md = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLAD3D);
			if (md->pSkin) {
				Con_Debugf("  [skin]");
			}
		}
		Con_Verbosef("\n");
		Escena_ShowTree(esc, n, indent + 2);
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
		Texto_Printf3D(pos, "%s", buf);
	}
}

void Eng_PrintfRel(int dx, int dy, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		VBprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		Texto_PrintfRel(dx, dy, "%s", buf);
	}
}

void Eng_PrintfXY(int x, int y, const char *fmt, ...) {
	char buf[0x400];
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		VBprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		Texto_Printf(x, y, "%s", buf);
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
	Grafico2D_GetPantalla(&w, &h);
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
		CFont_PrintfAlignColorSombra(font, x, y, (align & 2) * 2, color, "%s", buf);
		return;
	}
	CFont_PrintfAlignColor(font, x, y, (align & 2) * 2, color, "%s", buf);
}

byte Engine_FrameMove(void) {
	if (g_EngCallbacks[0] && g_CallbacksOn) {
		g_EngCallbacks[0]();
	}
	Modelos_UpdateAnims(0.0f);
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
	CHECK(g_Motor);
	CHECK(!g_DrawProgressBar);
	Camera_SetActual(g_CamActual);
	CD3DApp_Render(g_Motor);
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
#define g_ViewerClipNear GAME_DATA(float, 0x4a87b0)
#define g_ViewerClipFar GAME_DATA(float, 0x4a87c8)
#define g_ViewerFOV GAME_DATA(float, 0x4a8610)	/* degrees, 0: the camera keeps its own */
#define g_ViewerCameraTgt GAME_DATA(int, 0x4a8660)	/* orbits around the target */
#define g_ViewerCameraVel GAME_DATA(float, 0x4a86f0)
#define g_ViewerCameraInv GAME_DATA(int, 0x4a85b0)	/* the pitch not inverted */
#define g_ViewerSpeed GAME_DATA(float, 0x431b04)	/* doubled and halved by the debug keys */
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
		Texto_Mensaje(250, "velocity: x%g", (double)g_ViewerSpeed);
	}
	if (DebugAction(DBG_VIEWER_SLOWER)) {
		g_ViewerSpeed = (float)((double)g_ViewerSpeed * 0.5);
		Texto_Mensaje(250, "velocity: x%g", (double)g_ViewerSpeed);
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
		Camera_SetActual(g_ViewerCamera);
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
		SombrasStencil_Set(g_SombrasStencil == 0);
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
	CHECK(MOTOR_DEV(g_Motor));
	Splash_Show("2D/Loading/00Reset.dds", 1, 500, 0);
	byte *m = g_Motor;
	MOTOR_VSYNC(m) = g_VSync;
	if (MOTOR_WINDOWED(m)) {
		MOTOR_PP(m)[12] = 0;	/* FullScreen_PresentationInterval */
	} else {
		MOTOR_PP(m)[12] = MOTOR_VSYNC(m) ? 0 : 0x80000000;	/* D3DPRESENT_INTERVAL_IMMEDIATE */
	}
	if (CD3DApp_Reset(g_Motor) < 0) {
		E3D_LogBlue(g_FileLog, "ERROR: ResetEngine3D_All()");
	}
}

void Engine_ResetTextures(void) {
	Con_Printf("ResetEngine3D_Textures\n");
	CHECK(MOTOR_DEV(g_Motor));
	Textura_ReleaseAllD3D();
	CListaMat_Release(g_ListaMat);
	CListaMat_Prepare(g_ListaMat, MOTOR_DEV(g_Motor));
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
	g_ModelosOn = g_RenderModels != 0;
	g_UsarVB = g_RenderUseVB != 0;
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
	char level[MAX_RUTA], next[MAX_RUTA], fmt[MAX_RUTA];
	int count = 0;

	Ruta_Part(level, g_pApp->levelPath, RUTA_NAME);
	Ruta_Part(next, g_pApp->newLevelPath, RUTA_NAME);
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

void Camera_SetActual(CGameCamera *cam) {
	if (!cam && !g_CamActual) {
		return;
	}
	CCamara *c = NULL;
	g_CamActual = cam;
	if (g_pd3dDevice) {
		E3D_FlushBatch();
	}
	if (cam) {
		c = CGameCamera_Update(cam);
	}
	CMundo *m = Mundo_Get();
	if (c != m->cam) {
		CMundo_SetCamara(m, c);
	}
	if (g_pd3dDevice) {
		Render_SetCamara(Mundo_Get()->cam);
	}
}

void GVar_SetIntVerbose(cGVar *v, int value) {
	v->value.i = value;
	Con_Verbosef("%s = %d\n", v->name, value);
}

char *Engine_SetIniName(const char *name) {
	return Ruta_Merge(g_M3dIni, name, ".ini");
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
	D3DAdapterInfo *ai = Motor_AdapterInfo();
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
	uint32_t *pp = MOTOR_PP(g_Motor);
	pp[9] = mi->DepthStencilFormat;		/* AutoDepthStencilFormat */
	pp[0] = mi->Width;
	pp[1] = mi->Height;
	pp[2] = mi->Format;
	g_ResetPending = 1;
}

void Engine_SetSuelo(const char *dir, const char *name, const vec3_t *escala) {
	Str_PrintfQuiet(g_SueloPath, MAX_RUTA, "%s/%s", dir, name);
	g_SueloEscala->x = escala->x;
	g_SueloEscala->y = escala->y;
	g_SueloEscala->z = escala->z;
}

void Model_Info(const char *name, int n) {
	char pat[MAX_RUTA], nm[MAX_RUTA];

	Str_Copy(pat, name, sizeof(pat));
	crt_strlwr(pat);
	CEscena *e;
	for (e = Escena_Next(NULL); e; e = Escena_Next(e)) {
		Ruta_Part(nm, e->name, RUTA_NAME);
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
	Escena_ShowTree(e, NULL, 0);
	Con_Verbosef("\n");
	void *hInst = App_GetHInstance();
	void *hWnd = App_GetHWnd();
	exe_CreateDialogParamA(hInst, (const char *)0x99, hWnd, Game_Ptr(RVA_MODELDLG_PROC), (intptr_t)e);
}

void Models_List(void) {
	UtilEscena_Dump((utilPrintf_t)Game_Ptr(RVA_CON_VERBOSEF));
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

byte Video_Play(const char *name, const char *cmd, float arg) {
	(void)arg;
	CHECK(cmd);
	if (!crt_stricmp(cmd, "play")) {
		/* Scrap.VideoPlayer: the script goes on after the video, so this one still waits for it as the original */
		return Video_PlayFileWait(name, 0) > 0;
	}
	E3D_LogBlue(g_FileLog, "VideoPlayer: invalid command: %s", cmd);
	return 0;
}

byte Wild_MatchCase(const char *pattern, const char *name, byte ignoreCase) {
	CHECK(!ignoreCase);
	return Wild_Match(pattern, name) != 0;
}

void Ini_GetColor(const char *sect, const char *key, ColorIntens *color, CArchINI *ini) {
	char buf[0x40];
	byte *c = (byte *)&color->rgba;

	if (!CArchINI_ReadText(ini, sect, key, buf, sizeof(buf))) {
		if (Proc_Scan(buf, "&b&b&b&f", c, c + 1, c + 2, &color->intens) != 4) {
			CheckFailed("IniGetColor == 4", __FILE__, __TIMESTAMP__, __LINE__);
		}
	}
}

int Ini_GetPath(const char *sect, const char *key, char *dest, int size, const char *base, CArchINI *ini) {
	char dir[MAX_RUTA], rel[MAX_RUTA];

	if (CArchINI_ReadText(ini, sect, key, rel, sizeof(rel))) {
		return -1;
	}
	Ruta_Part(dir, base, RUTA_DRIVE | RUTA_DIR);
	Bprintf(dest, size, "%s%s", dir, rel);
	return 0;
}

/* the globals of m3d.ini without a better home */
#define g_MapaSombrasMultitext GAME_DATA(int, 0x4b2b00)
#define g_MapaSombrasSuaves GAME_DATA(int, 0x4b2b04)
#define g_MapaSombrasEscrTemp GAME_DATA(int, 0x4b2b08)
#define g_MapaSombrasAten GAME_DATA(float, 0x4b2b0c)
#define g_MapaSombrasEscala GAME_DATA(float, 0x432950)
#define g_MapaSombrasResol GAME_DATA(float, 0x432954)
#define g_MapaSombrasTexX GAME_DATA(int, 0x432958)
#define g_MapaSombrasTexY GAME_DATA(int, 0x43295c)
#define g_MapaSombrasGamma GAME_DATA(float, 0x432960)
#define g_MapaSombrasBrillo GAME_DATA(float, 0x432964)
#define g_MapaSombrasRadiosidad GAME_DATA(int, 0x4b2b10)
#define g_MapaSombrasIlumGlobal GAME_DATA(int, 0x4b2b14)
#define g_MapaSombrasNoLuzAmb GAME_DATA(int, 0x4b2b18)
#define g_MapaSombrasModulate4x GAME_DATA(int, 0x4b2b1c)
#define g_MapaSombrasRebotes GAME_DATA(int, 0x432968)
#define g_MapaSombrasCoefRad GAME_DATA(float, 0x43296c)
#define g_MapaSombrasUsarTxf GAME_DATA(int, 0x4b2b24)
#define g_SombrasDestAlpha GAME_DATA(int, 0x4afdb4)
#define g_FlaresTamDef GAME_DATA(float, 0x432998)
#define g_FlaresAtenDist GAME_DATA(int, 0x4d6d90)
#define g_FlaresAtenIni GAME_DATA(float, 0x43299c)
#define g_FlaresAtenFin GAME_DATA(float, 0x4329a0)
#define g_FlaresFundidoEnt GAME_DATA(float, 0x4329a4)
#define g_FlaresFundidoSal GAME_DATA(float, 0x4329a8)
#define g_FlaresVerInfo GAME_DATA(int, 0x4d6d94)

void Engine_LoadIni(const char *name, const char *dir) {
	static const char *const rutas[4] = { "ruta1", "ruta2", "ruta3", "ruta4" };
	static const char *const extens[4] = { "extens1", "extens2", "extens3", "extens4" };
	char tex[MAX_RUTA];
	CArchINI ini;

	CArchINI_ctor(&ini);
	if (!CArchINI_Load(&ini, name, dir, 1)) {
		CArchINI_ReadText(&ini, "suelo", "ruta_m3d", g_SueloPath, MAX_RUTA);
		Ini_GetColor("suelo", "model_ambient", g_ModelAmbientIni, &ini);
		CArchINI_ReadFloat(&ini, "camara", "forzar_clipNear", &g_ClipNear);
		CArchINI_ReadFloat(&ini, "camara", "forzar_clipFar", &g_ClipFar);
		CArchINI_ReadText(&ini, "cielo", "path", g_CieloPath, MAX_RUTA);
		CArchINI_ReadText(&ini, "cielo", "ruta_m3d", g_CieloPath, MAX_RUTA);
		Ini_GetPath("cielo", "ruta_rel", g_CieloPath, MAX_RUTA, name, &ini);
		CArchINI_ReadFloat(&ini, "cielo", "pos_scale", &g_CieloPosScale);
		CArchINI_ReadFloat(&ini, "cielo", "rotation", &g_CieloRotation);
		CArchINI_ReadBool(&ini, "niebla", "activa", &g_NieblaActiva);
		CArchINI_ReadBool(&ini, "niebla", "modo_tabla", &g_NieblaModoTabla);
		CArchINI_ReadBool(&ini, "niebla", "lineal", &g_NieblaLineal);
		CArchINI_ReadFloat(&ini, "niebla", "rango_ini", &g_NieblaRangoIni);
		CArchINI_ReadFloat(&ini, "niebla", "rango_fin", &g_NieblaRangoFin);
		CArchINI_ReadFloat(&ini, "niebla", "densidad", &g_NieblaDensidad);
		Ini_GetColor("niebla", "color", g_NieblaColor, &ini);
		CArchINI_ReadBool(&ini, "mapa_sombras", "usar", &g_MapaSombrasUsar);
		CArchINI_ReadBool(&ini, "mapa_sombras", "dlg", &g_MapaSombrasDlg);
		CArchINI_ReadBool(&ini, "mapa_sombras", "calcular", &g_MapaSombrasCalcular);
		CArchINI_ReadBool(&ini, "mapa_sombras", "multitext", &g_MapaSombrasMultitext);
		CArchINI_ReadBool(&ini, "mapa_sombras", "sombras_suaves", &g_MapaSombrasSuaves);
		CArchINI_ReadBool(&ini, "mapa_sombras", "escr_mapas_temp", &g_MapaSombrasEscrTemp);
		CArchINI_ReadFloat(&ini, "mapa_sombras", "aten_sombras", &g_MapaSombrasAten);
		CArchINI_ReadFloat(&ini, "mapa_sombras", "escala", &g_MapaSombrasEscala);
		CArchINI_ReadFloat(&ini, "mapa_sombras", "resolucion", &g_MapaSombrasResol);
		CArchINI_ReadInt(&ini, "mapa_sombras", "text_tam_x", &g_MapaSombrasTexX);
		CArchINI_ReadInt(&ini, "mapa_sombras", "text_tam_y", &g_MapaSombrasTexY);
		CArchINI_ReadFloat(&ini, "mapa_sombras", "gamma", &g_MapaSombrasGamma);
		CArchINI_ReadFloat(&ini, "mapa_sombras", "brillo", &g_MapaSombrasBrillo);
		CArchINI_ReadBool(&ini, "mapa_sombras", "radiosidad", &g_MapaSombrasRadiosidad);
		CArchINI_ReadBool(&ini, "mapa_sombras", "ilum_global", &g_MapaSombrasIlumGlobal);
		CArchINI_ReadBool(&ini, "mapa_sombras", "no_luz_amb", &g_MapaSombrasNoLuzAmb);
		CArchINI_ReadBool(&ini, "mapa_sombras", "modulate4x", &g_MapaSombrasModulate4x);
		CArchINI_ReadInt(&ini, "mapa_sombras", "rebotes", &g_MapaSombrasRebotes);
		CArchINI_ReadFloat(&ini, "mapa_sombras", "coef_rad", &g_MapaSombrasCoefRad);
		CArchINI_ReadBool(&ini, "mapa_sombras", "calc_lmap2", &g_CalcLmap2);
		CArchINI_ReadBool(&ini, "mapa_sombras", "usar_txf", &g_MapaSombrasUsarTxf);
		CArchINI_ReadBool(&ini, "sombras_stencil", "usar", &g_SombrasStencil);
		CArchINI_ReadBool(&ini, "sombras_stencil", "dest_alpha", &g_SombrasDestAlpha);
		CArchINI_ReadFloat(&ini, "flares", "tam_def", &g_FlaresTamDef);
		CArchINI_ReadBool(&ini, "flares", "aten_dist", &g_FlaresAtenDist);
		CArchINI_ReadFloat(&ini, "flares", "aten_dist_ini", &g_FlaresAtenIni);
		CArchINI_ReadFloat(&ini, "flares", "aten_dist_fin", &g_FlaresAtenFin);
		CArchINI_ReadFloat(&ini, "flares", "fundido_ent", &g_FlaresFundidoEnt);
		CArchINI_ReadFloat(&ini, "flares", "fundido_sal", &g_FlaresFundidoSal);
		CArchINI_ReadBool(&ini, "flares", "ver_info", &g_FlaresVerInfo);
		for (int i = 0; i < 4; i++) {
			if (!CArchINI_ReadText(&ini, "texturas", rutas[i], tex, sizeof(tex))) {
				Tex_AddPath(tex);
			}
		}
		for (int i = 0; i < 4; i++) {
			if (!CArchINI_ReadText(&ini, "texturas", extens[i], tex, sizeof(tex))) {
				Tex_AddExtension(tex);
			}
		}
	}
	CArchINI_dtor(&ini);
}
