/*
 * Render targets of the engine3d (original source Engine/RenderTarget.cpp): the scene drawn into a texture and the
 * post effects drawn from it (DuDv deformation, bloom, motion blur, radial blur, blur), the NewsPanel target (the
 * news screen of the game drawn into a texture) and the Credits target (the credits text with a soft trail).
 * RenderTarget_Begin(name) puts the render target of "Scene", "NewsPanel" or "Credits" on the device,
 * RenderTarget_End(name) (NULL: the one begun) draws its effects and puts the back buffer back.
 *
 * The scene uses two textures of the viewport size (three in split screen) that swap after each effect: the target
 * the next effect draws into and the source it reads. The effects come in the order of two tables of the exe: whether
 * each is on, and its render function; the last one on draws into the back buffer.
 */
#ifndef SCRAP_RENDERTARGET_H
#define SCRAP_RENDERTARGET_H

#include "scrap/gvar.h"
#include "scrap/game.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"

/* the targets of RenderTarget_Begin */
enum {
	RTARGET_NONE = 0,
	RTARGET_SCENE = 1,
	RTARGET_NEWSPANEL = 2,
	RTARGET_CREDITS = 3,
};

/* the scene effects in the order of the tables */
enum {
	RTFX_DUDV,
	RTFX_BLOOM,
	RTFX_MOTIONBLUR,
	RTFX_RADIALBLUR,
	RTFX_BLUR,
	RTFX_TOTAL
};

/* the engine variables (Gvar values) */
extern cGVar GVar_R_NewsPanelRTarget;
#define g_NewsPanelRTarget (GVar_R_NewsPanelRTarget.value.i)		/* R_NewsPanelRTarget */
extern cGVar GVar_R_NewsPanelSize;
#define g_NewsPanelSize (GVar_R_NewsPanelSize.value.i)		/* R_NewsPanelSize, pow2 */
extern cGVar GVar_R_NewsPanelWire;
#define g_NewsPanelWire (GVar_R_NewsPanelWire.value.i)		/* R_NewsPanelWire */
extern cGVar GVar_R_SceneRTarget;
#define g_SceneRTarget (GVar_R_SceneRTarget.value.i)		/* R_SceneRTarget, 3 skips the caps check memory */
extern cGVar GVar_R_SceneRTargetSwap;
#define g_SceneRTargetSwap (GVar_R_SceneRTargetSwap.value.i)		/* R_SceneRTargetSwap */
extern cGVar GVar_R_SceneForceLinPow2;
#define g_SceneForceLinPow2 (GVar_R_SceneForceLinPow2.value.i)		/* R_SceneForceLinPow2 -1/0/1 */
extern cGVar GVar_R_SceneForceLinNonPow2;
#define g_SceneForceLinNonPow2 (GVar_R_SceneForceLinNonPow2.value.i)	/* R_SceneForceLinNonPow2 -1/0/1 */
extern cGVar GVar_R_SceneCheckPixels;
#define g_SceneCheckPixels (GVar_R_SceneCheckPixels.value.i)		/* R_SceneCheckPixels */
extern cGVar GVar_R_SceneDuDvRTarget;
#define g_SceneDuDvRTarget (GVar_R_SceneDuDvRTarget.value.i)		/* R_SceneDuDvRTarget 0..3 */
extern cGVar GVar_R_SceneDuDvFilter;
#define g_SceneDuDvFilter (GVar_R_SceneDuDvFilter.value.i)		/* R_SceneDuDvFilter */
extern cGVar GVar_R_SceneDuDvScale;
#define g_SceneDuDvScale (GVar_R_SceneDuDvScale.value.f)		/* R_SceneDuDvScale */
extern cGVar GVar_R_SceneDuDvForced;
#define g_SceneDuDvForced (GVar_R_SceneDuDvForced.value.i)		/* R_SceneDuDvForced */
extern cGVar GVar_R_SceneDuDvCheckPixels;
#define g_SceneDuDvCheckPixels (GVar_R_SceneDuDvCheckPixels.value.i)	/* R_SceneDuDvCheckPixels */
extern cGVar GVar_R_SceneBloomRTarget;
#define g_SceneBloomRTarget (GVar_R_SceneBloomRTarget.value.i)		/* R_SceneBloomRTarget */
extern cGVar GVar_R_SceneBloomBlurPasses;
#define g_SceneBloomBlurPasses (GVar_R_SceneBloomBlurPasses.value.i)	/* R_SceneBloomBlurPasses */
extern cGVar GVar_R_SceneBloomBlurOffset;
#define g_SceneBloomBlurOffset (GVar_R_SceneBloomBlurOffset.value.f)	/* R_SceneBloomBlurOffset, texels */
extern cGVar GVar_R_SceneBloomBlurSize;
#define g_SceneBloomBlurSize (GVar_R_SceneBloomBlurSize.value.f)	/* R_SceneBloomBlurSize, of the scene size */
extern cGVar GVar_R_SceneMotionBlurRTarget;
#define g_SceneMotionBlurRTarget (GVar_R_SceneMotionBlurRTarget.value.i)	/* R_SceneMotionBlurRTarget */
extern cGVar GVar_R_SceneMotionBlurValue;
#define g_SceneMotionBlurValue (GVar_R_SceneMotionBlurValue.value.f)	/* R_SceneMotionBlurValue */
extern cGVar GVar_R_SceneMotionBlurOff;
#define g_SceneMotionBlurOff (GVar_R_SceneMotionBlurOff.value.f)	/* R_SceneMotionBlurOff */
extern cGVar GVar_R_SceneMotionBlurOffset;
#define g_SceneMotionBlurOffset (GVar_R_SceneMotionBlurOffset.value.f)	/* R_SceneMotionBlurOffset, texels */
extern cGVar GVar_R_SceneMotionBlurSize;
#define g_SceneMotionBlurSize (GVar_R_SceneMotionBlurSize.value.f)	/* R_SceneMotionBlurSize, of the scene size */
extern cGVar GVar_R_SceneRadialBlurRTarget;
#define g_SceneRadialBlurRTarget (GVar_R_SceneRadialBlurRTarget.value.i)	/* R_SceneRadialBlurRTarget */
extern cGVar GVar_R_SceneRadialBlurValue;
#define g_SceneRadialBlurValue (GVar_R_SceneRadialBlurValue.value.f)	/* R_SceneRadialBlurValue */
extern cGVar GVar_R_SceneRadialBlurOffset;
#define g_SceneRadialBlurOffset (GVar_R_SceneRadialBlurOffset.value.f)	/* R_SceneRadialBlurOffset */
extern cGVar GVar_R_SceneBlurRTarget;
#define g_SceneBlurRTarget (GVar_R_SceneBlurRTarget.value.i)		/* R_SceneBlurRTarget */
extern cGVar GVar_R_SceneBlurValue;
#define g_SceneBlurValue (GVar_R_SceneBlurValue.value.f)		/* R_SceneBlurValue */
extern cGVar GVar_R_SceneBlurOffset;
#define g_SceneBlurOffset (GVar_R_SceneBlurOffset.value.f)		/* R_SceneBlurOffset, texels */
extern cGVar GVar_R_CreditsRTarget;
#define g_CreditsRTarget (GVar_R_CreditsRTarget.value.i)		/* R_CreditsRTarget */
extern cGVar GVar_R_CreditsSizeX;
#define g_CreditsSizeX (GVar_R_CreditsSizeX.value.i)		/* R_CreditsSizeX, pow2 */
extern cGVar GVar_R_CreditsSizeY;
#define g_CreditsSizeY (GVar_R_CreditsSizeY.value.i)		/* R_CreditsSizeY, pow2 */
extern cGVar GVar_R_CreditsX;
#define g_CreditsX (GVar_R_CreditsX.value.i)			/* R_CreditsX, in 640 x 480 */
extern cGVar GVar_R_CreditsY;
#define g_CreditsY (GVar_R_CreditsY.value.i)			/* R_CreditsY */
extern cGVar GVar_R_CreditsClear;
#define g_CreditsClear (GVar_R_CreditsClear.value.i)		/* R_CreditsClear */
extern cGVar GVar_R_CreditsPhase;
#define g_CreditsPhase (GVar_R_CreditsPhase.value.i)		/* R_CreditsPhase 0 horizontal, 1 vertical */
extern cGVar GVar_R_CreditsAlpha;
#define g_CreditsAlpha (GVar_R_CreditsAlpha.value.i)		/* R_CreditsAlpha 0..255 */
extern cGVar GVar_R_SplitScreenCamera;
#define g_SplitScreenCamera (GVar_R_SplitScreenCamera.value.i)		/* R_SplitScreenCamera, the half drawn */
extern cGVar GVar_R_SplitScreenUpdate;
#define g_SplitScreenUpdate (GVar_R_SplitScreenUpdate.value.i)		/* R_SplitScreenUpdate: the halves alternate */
/* R_SplitScreen is g_SplitScreen (scrapmodel.h) */

/* the target of RenderTarget_Begin, RTARGET_NONE between Begin and End */
extern int g_RTMode;
/* the back buffer and depth buffer of the device while a target is on (referenced) */
extern void *g_RTBackBuffer;
extern void *g_RTZBuffer;
/* the viewports (6 words) of before a NewsPanel or Credits target: the current one and the whole one */
extern uint32_t g_RTSavedViewport[6];
extern uint32_t g_RTSavedViewportFull[6];
/* the pixel texture coordinates: the device needs them on pow2 textures (the Credits, NewsPanel and bloom sizes)
 * and on the other ones (the scene size) */
extern byte g_RTLinPow2;
extern byte g_RTLinNonPow2;

/* NewsPanel: the texture and its depth buffer */
extern void *g_NewsPanelTarget;
extern void *g_NewsPanelZ;

/* Credits: the texture, its materials, the 2D screen size of before Begin, the last blur time */
extern void *g_CreditsTarget;
extern CMaterial *g_CreditsMat;
extern CMaterial *g_CreditsMatBlur;
extern int g_CreditsSavedW;
extern int g_CreditsSavedH;
extern uint32_t g_CreditsBlurTime;

/* Scene: on, the size of the textures, the textures (the third one in split screen), the copy material */
extern byte g_SceneOn;
extern float g_SceneW;
extern float g_SceneH;
extern void *g_SceneRT1;
extern void *g_SceneRT2;
extern void *g_SceneRT3;
extern CMaterial *g_SceneMatCopy;
/* the source the next effect reads (the scene is drawn into it) and the target it draws into (textures and their
 * surface 0, referenced); they swap after each effect */
extern void *g_SceneSourceSurface;
extern void *g_SceneTargetSurface;
extern void *g_SceneSourceTexture;
extern void *g_SceneTargetTexture;
/* the next swap is skipped (the DuDv effect kept its target) */
extern byte g_SceneNoSwap;
/* the effects on in this frame and the last one of them, -1 none */
extern byte g_SceneFxOn[5];
extern int g_SceneFxLast;
/* the full viewport of before the split screen halved it */
extern uint32_t g_SceneViewport[6];
/* the target of before the DuDv target became the source (texture, surface) */
extern void *g_SceneDuDvSavedTexture;
extern void *g_SceneDuDvSavedSurface;
/* the R_SceneRTarget and R_SceneDuDvRTarget the caps were checked for, -1 never */
extern int g_SceneCheckedMode;
extern int g_SceneDuDvCheckedMode;
/* the tables of the effects: on (byte), render */
typedef byte (*rtFxOn_t)(void);
typedef void (*rtFxRender_t)(void);
extern rtFxOn_t g_RTFxOn[5];
extern rtFxRender_t g_RTFxRender[5];

/* DuDv: the deformation target, the frame it was drawn in, the bump target, the materials */
extern void *g_SceneDuDvTarget;
extern int g_SceneDuDvFrame;
extern void *g_SceneDuDvBumpTarget;
extern CMaterial *g_SceneDuDvMatRTarget;
extern CMaterial *g_SceneDuDvMatFilter;
extern CMaterial *g_SceneDuDvMatCopy;
extern CMaterial *g_SceneDuDvMatDebug;

/* Bloom: the size of its targets, the two targets, the materials */
extern float g_BloomW;
extern float g_BloomH;
extern void *g_BloomTarget1;
extern void *g_BloomTarget2;
extern CMaterial *g_BloomMatShow;
extern CMaterial *g_BloomMatTarget;
extern CMaterial *g_BloomMatFilter;
extern CMaterial *g_BloomMatBlur;

/* Motion blur: the size, the target, the materials, the first frame (the target is not filled yet) */
extern float g_MotionBlurW;
extern float g_MotionBlurH;
extern void *g_MotionBlurTarget;
extern CMaterial *g_MotionBlurMatTarget;
extern CMaterial *g_MotionBlurMatAdd;
extern byte g_MotionBlurFilled;

/* Blur and radial blur: the materials */
extern CMaterial *g_BlurMatTarget;
extern CMaterial *g_BlurMatCopy;
extern CMaterial *g_RadialBlurMat;

/* the materials of the effects, each made once (CHECK that it is not made) */
void RenderTarget_MakeCreditsMatBlur(void);
void RenderTarget_MakeCreditsMat(void);
void RenderTarget_MakeBloomMatBlur(void);
void RenderTarget_MakeBloomMatFilter(void);
void RenderTarget_MakeBloomMatShow(void);
void RenderTarget_MakeBloomMatTarget(void);
void RenderTarget_MakeBlurMatCopy(void);
void RenderTarget_MakeBlurMatTarget(void);
void RenderTarget_MakeDuDvMatCopy(void);
void RenderTarget_MakeDuDvMatDebug(void);
void RenderTarget_MakeDuDvMatFilter(void);
void RenderTarget_MakeDuDvMatRTarget(void);
void RenderTarget_MakeMotionBlurMatAdd(void);
void RenderTarget_MakeMotionBlurMatTarget(void);
void RenderTarget_MakeSceneMatCopy(void);

/* Credits: the target on (the 2D screen of its size), off with the text drawn to the screen */
byte Credits_Begin(void);
void Credits_End(void);
void Credits_Release(void);
void Credits_Init(void);
/* the trail: the target drawn over itself shifted (about every 1/20 s of game time) */
void Credits_Blur(void);
/* the target drawn to the screen at R_CreditsX, R_CreditsY with R_CreditsAlpha */
void Credits_Draw(void);

/* the bytes of video memory: 512 x 512 render targets made until it fails (1 MB each), then released */
int RenderTarget_VideoMemory(void);

byte NewsPanel_Begin(void);
void NewsPanel_End(void);
void NewsPanel_Release(void);
void *NewsPanel_GetTarget(void);
void NewsPanel_Init(void);

/* the pow2 size of a target for n pixels: 256, 512 or 1024 */
int RenderTarget_Pow2Size(int n);
/* RTARGET_* of a name, the target begun for NULL; a fatal error for an unknown name */
int RenderTarget_Mode(const char *name);
byte RenderTarget_Begin(const char *name);
byte RenderTarget_End(const char *name);
/* RenderTarget_InitD3D and RenderTarget_ReleaseD3D are in render.h */

/* the effects: the frame of the effect (render), whether it is on in this frame, its targets made and released */
void Bloom_Render(void);
void Bloom_Release(void);
void Bloom_Init(void);
byte Bloom_IsOn(void);
void Blur_Render(void);
void Blur_Release(void);
void Blur_Init(void);
byte Blur_IsOn(void);
void DuDv_Render(void);
void DuDv_Release(void);
void DuDv_Init(void);
byte DuDv_IsOn(void);
/* the models and trails of the deformation pass drawn into the bump target */
void DuDv_RenderBump(void);
void MotionBlur_Render(void);
void MotionBlur_Release(void);
void MotionBlur_Init(void);
byte MotionBlur_IsOn(void);
void RadialBlur_Render(void);
void RadialBlur_Release(void);
byte RadialBlur_IsOn(void);

/* the draws of the effects into the scene target */
void Bloom_Filter(void);
void Bloom_Blur(void);
void Bloom_Show(void);
void Blur_Draw(void);
void DuDv_Copy(void);
void DuDv_CopyToTarget(void);
void DuDv_Debug(void);
void DuDv_Filter(void);
void DuDv_Draw(byte check);
/* the deformation through the device on test textures, compared with the expected result */
byte DuDv_CheckCaps(void);
void MotionBlur_Add(void);
void MotionBlur_Draw(void);
void RadialBlur_Draw(void);
/* the scene target drawn into and read back on a test scene, compared with the back buffer */
byte Scene_CheckCaps(void);

/* Scene: on with the target of the effects, off with the effects drawn */
byte Scene_Begin(void);
void Scene_End(void);
void Scene_Release(void);
void Scene_Init(void);
/* the target is the back buffer (for the last effect) */
void Scene_TargetBackBuffer(void);
/* target and source swapped (with R_SceneRTargetSwap, unless g_SceneNoSwap) */
void Scene_Swap(void);
/* the two halves of the split screen drawn to the back buffer */
void Scene_DrawSplit(void);
/* the pixel (x, y) of two locked surfaces (pitch, bits; bpp bytes a pixel) has the same r, g and b, each within
 * tol when tol > 0 */
byte RenderTarget_PixelEqual(int pitchA, const byte *bitsA, int pitchB, const byte *bitsB, int x, int bpp, int y,
	int tol);
/* the texture tex as the rectangle quad (0..1 of the whole viewport) in color */
void RenderTarget_DrawTexQuad(const box_t *quad, uint32_t color, void *tex);

/* other modules */
/* ScreenCapture.c: the surface as the next ShotNNNN.pcx; ShotCopy through a copy made on its device */
byte Surface_SaveShot(void *surf);
byte Surface_ShotCopy(void *surf);

#endif
