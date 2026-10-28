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
#define g_NewsPanelRTarget GAME_DATA(int, 0x4a8bfc)		/* R_NewsPanelRTarget */
#define g_NewsPanelSize GAME_DATA(int, 0x4a8d9c)		/* R_NewsPanelSize, pow2 */
#define g_NewsPanelWire GAME_DATA(int, 0x4a8cbc)		/* R_NewsPanelWire */
#define g_SceneRTarget GAME_DATA(int, 0x4a8b1c)		/* R_SceneRTarget, 3 skips the caps check memory */
#define g_SceneRTargetSwap GAME_DATA(int, 0x4a8db4)		/* R_SceneRTargetSwap */
#define g_SceneForceLinPow2 GAME_DATA(int, 0x4a8ad4)		/* R_SceneForceLinPow2 -1/0/1 */
#define g_SceneForceLinNonPow2 GAME_DATA(int, 0x4a8b4c)	/* R_SceneForceLinNonPow2 -1/0/1 */
#define g_SceneCheckPixels GAME_DATA(int, 0x4a8c5c)		/* R_SceneCheckPixels */
#define g_SceneDuDvRTarget GAME_DATA(int, 0x4a8b34)		/* R_SceneDuDvRTarget 0..3 */
#define g_SceneDuDvFilter GAME_DATA(int, 0x4a8d3c)		/* R_SceneDuDvFilter */
#define g_SceneDuDvScale GAME_DATA(float, 0x4a8abc)		/* R_SceneDuDvScale */
#define g_SceneDuDvForced GAME_DATA(int, 0x4a8e04)		/* R_SceneDuDvForced */
#define g_SceneDuDvCheckPixels GAME_DATA(int, 0x4a8c14)	/* R_SceneDuDvCheckPixels */
#define g_SceneBloomRTarget GAME_DATA(int, 0x4a8a5c)		/* R_SceneBloomRTarget */
#define g_SceneBloomBlurPasses GAME_DATA(int, 0x4a8b9c)	/* R_SceneBloomBlurPasses */
#define g_SceneBloomBlurOffset GAME_DATA(float, 0x4a8ca4)	/* R_SceneBloomBlurOffset, texels */
#define g_SceneBloomBlurSize GAME_DATA(float, 0x4a8a74)	/* R_SceneBloomBlurSize, of the scene size */
#define g_SceneMotionBlurRTarget GAME_DATA(int, 0x4a8cd4)	/* R_SceneMotionBlurRTarget */
#define g_SceneMotionBlurValue GAME_DATA(float, 0x4a8c44)	/* R_SceneMotionBlurValue */
#define g_SceneMotionBlurOff GAME_DATA(float, 0x4a8b04)	/* R_SceneMotionBlurOff */
#define g_SceneMotionBlurOffset GAME_DATA(float, 0x4a8d84)	/* R_SceneMotionBlurOffset, texels */
#define g_SceneMotionBlurSize GAME_DATA(float, 0x4a8d6c)	/* R_SceneMotionBlurSize, of the scene size */
#define g_SceneRadialBlurRTarget GAME_DATA(int, 0x4a8bcc)	/* R_SceneRadialBlurRTarget */
#define g_SceneRadialBlurValue GAME_DATA(float, 0x4a8d24)	/* R_SceneRadialBlurValue */
#define g_SceneRadialBlurOffset GAME_DATA(float, 0x4a8b64)	/* R_SceneRadialBlurOffset */
#define g_SceneBlurRTarget GAME_DATA(int, 0x4a8aec)		/* R_SceneBlurRTarget */
#define g_SceneBlurValue GAME_DATA(float, 0x4a8cec)		/* R_SceneBlurValue */
#define g_SceneBlurOffset GAME_DATA(float, 0x4a8bb4)		/* R_SceneBlurOffset, texels */
#define g_CreditsRTarget GAME_DATA(int, 0x4a8dcc)		/* R_CreditsRTarget */
#define g_CreditsSizeX GAME_DATA(int, 0x4a8be4)		/* R_CreditsSizeX, pow2 */
#define g_CreditsSizeY GAME_DATA(int, 0x4a8dec)		/* R_CreditsSizeY, pow2 */
#define g_CreditsX GAME_DATA(int, 0x4a8b7c)			/* R_CreditsX, in 640 x 480 */
#define g_CreditsY GAME_DATA(int, 0x4a8c2c)			/* R_CreditsY */
#define g_CreditsClear GAME_DATA(int, 0x4a8d04)		/* R_CreditsClear */
#define g_CreditsPhase GAME_DATA(int, 0x4a8aa4)		/* R_CreditsPhase 0 horizontal, 1 vertical */
#define g_CreditsAlpha GAME_DATA(int, 0x4a8d54)		/* R_CreditsAlpha 0..255 */
#define g_SplitScreenCamera GAME_DATA(int, 0x4a8a8c)		/* R_SplitScreenCamera, the half drawn */
#define g_SplitScreenUpdate GAME_DATA(int, 0x4a8c74)		/* R_SplitScreenUpdate: the halves alternate */
/* R_SplitScreen is g_SplitScreen (scrapmodel.h) */

/* the target of RenderTarget_Begin, RTARGET_NONE between Begin and End */
#define g_RTMode GAME_DATA(int, 0x4a8a00)
/* the back buffer and depth buffer of the device while a target is on (referenced) */
#define g_RTBackBuffer GAME_DATA(void *, 0x4a8e0c)
#define g_RTZBuffer GAME_DATA(void *, 0x4a8e10)
/* the viewports (6 words) of before a NewsPanel or Credits target: the current one and the whole one */
#define g_RTSavedViewport ((uint32_t *)Game_Ptr(0x4a8a1c))
#define g_RTSavedViewportFull ((uint32_t *)Game_Ptr(0x4a8a34))
/* the pixel texture coordinates: the device needs them on pow2 textures (the Credits, NewsPanel and bloom sizes)
 * and on the other ones (the scene size) */
#define g_RTLinPow2 GAME_DATA(byte, 0x4a8e1d)
#define g_RTLinNonPow2 GAME_DATA(byte, 0x4a8e1c)

/* NewsPanel: the texture and its depth buffer */
#define g_NewsPanelTarget GAME_DATA(void *, 0x4a8e14)
#define g_NewsPanelZ GAME_DATA(void *, 0x4a8e18)

/* Credits: the texture, its materials, the 2D screen size of before Begin, the last blur time */
#define g_CreditsTarget GAME_DATA(void *, 0x4a8e98)
#define g_CreditsMat GAME_DATA(CMaterial *, 0x4a8e9c)
#define g_CreditsMatBlur GAME_DATA(CMaterial *, 0x4a8ea0)
#define g_CreditsSavedW GAME_DATA(int, 0x4a8ea4)
#define g_CreditsSavedH GAME_DATA(int, 0x4a8ea8)
#define g_CreditsBlurTime GAME_DATA(uint32_t, 0x4a8eb0)

/* Scene: on, the size of the textures, the textures (the third one in split screen), the copy material */
#define g_SceneOn GAME_DATA(byte, 0x4a8e1f)
#define g_SceneW GAME_DATA(float, 0x4a8d0c)
#define g_SceneH GAME_DATA(float, 0x4a8d10)
#define g_SceneRT1 GAME_DATA(void *, 0x4a8e30)
#define g_SceneRT2 GAME_DATA(void *, 0x4a8e34)
#define g_SceneRT3 GAME_DATA(void *, 0x4a8e3c)
#define g_SceneMatCopy GAME_DATA(CMaterial *, 0x4a8e38)
/* the source the next effect reads (the scene is drawn into it) and the target it draws into (textures and their
 * surface 0, referenced); they swap after each effect */
#define g_SceneSourceSurface GAME_DATA(void *, 0x4a8e20)
#define g_SceneTargetSurface GAME_DATA(void *, 0x4a8e24)
#define g_SceneSourceTexture GAME_DATA(void *, 0x4a8e28)
#define g_SceneTargetTexture GAME_DATA(void *, 0x4a8e2c)
/* the next swap is skipped (the DuDv effect kept its target) */
#define g_SceneNoSwap GAME_DATA(byte, 0x4a8e1e)
/* the effects on in this frame and the last one of them, -1 none */
#define g_SceneFxOn ((byte *)Game_Ptr(0x4a8e40))
#define g_SceneFxLast GAME_DATA(int, 0x4a8e48)
/* the full viewport of before the split screen halved it */
#define g_SceneViewport ((uint32_t *)Game_Ptr(0x4a8a04))
/* the target of before the DuDv target became the source (texture, surface) */
#define g_SceneDuDvSavedTexture GAME_DATA(void *, 0x4a89f8)
#define g_SceneDuDvSavedSurface GAME_DATA(void *, 0x4a89fc)
/* the R_SceneRTarget and R_SceneDuDvRTarget the caps were checked for, -1 never */
#define g_SceneCheckedMode GAME_DATA(int, 0x431b80)
#define g_SceneDuDvCheckedMode GAME_DATA(int, 0x431b84)
/* the tables of the effects: on (byte), render */
typedef byte (*rtFxOn_t)(void);
typedef void (*rtFxRender_t)(void);
#define g_RTFxOn ((rtFxOn_t *)Game_Ptr(0x431b58))
#define g_RTFxRender ((rtFxRender_t *)Game_Ptr(0x431b6c))

/* DuDv: the deformation target, the frame it was drawn in, the bump target, the materials */
#define g_SceneDuDvTarget GAME_DATA(void *, 0x4a8e4c)
#define g_SceneDuDvFrame GAME_DATA(int, 0x4a8e50)
#define g_SceneDuDvBumpTarget GAME_DATA(void *, 0x4a8e54)
#define g_SceneDuDvMatRTarget GAME_DATA(CMaterial *, 0x4a8e58)
#define g_SceneDuDvMatFilter GAME_DATA(CMaterial *, 0x4a8e5c)
#define g_SceneDuDvMatCopy GAME_DATA(CMaterial *, 0x4a8e60)
#define g_SceneDuDvMatDebug GAME_DATA(CMaterial *, 0x4a8e64)

/* Bloom: the size of its targets, the two targets, the materials */
#define g_BloomW GAME_DATA(float, 0x4a8b84)
#define g_BloomH GAME_DATA(float, 0x4a8b88)
#define g_BloomTarget1 GAME_DATA(void *, 0x4a8e68)
#define g_BloomTarget2 GAME_DATA(void *, 0x4a8e6c)
#define g_BloomMatShow GAME_DATA(CMaterial *, 0x4a8e70)
#define g_BloomMatTarget GAME_DATA(CMaterial *, 0x4a8e74)
#define g_BloomMatFilter GAME_DATA(CMaterial *, 0x4a8e78)
#define g_BloomMatBlur GAME_DATA(CMaterial *, 0x4a8e7c)

/* Motion blur: the size, the target, the materials, the first frame (the target is not filled yet) */
#define g_MotionBlurW GAME_DATA(float, 0x4a8dd4)
#define g_MotionBlurH GAME_DATA(float, 0x4a8dd8)
#define g_MotionBlurTarget GAME_DATA(void *, 0x4a8e80)
#define g_MotionBlurMatTarget GAME_DATA(CMaterial *, 0x4a8e84)
#define g_MotionBlurMatAdd GAME_DATA(CMaterial *, 0x4a8e88)
#define g_MotionBlurFilled GAME_DATA(byte, 0x4a8e45)

/* Blur and radial blur: the materials */
#define g_BlurMatTarget GAME_DATA(CMaterial *, 0x4a8e8c)
#define g_BlurMatCopy GAME_DATA(CMaterial *, 0x4a8e90)
#define g_RadialBlurMat GAME_DATA(CMaterial *, 0x4a8e94)

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
void RenderTarget_MakeRadialBlurMat(void);
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
/* CaptPant.c: the surface as the next ShotNNNN.pcx (or a frame of the AVI capture); ShotCopy through a copy made
 * on its device */
byte Surface_SaveShot(void *surf);
byte Surface_ShotCopy(void *surf);

#endif
