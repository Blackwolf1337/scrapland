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

extern cGVar GVar_R_SceneDuDvRTarget;
#define g_SceneDuDvRTarget (GVar_R_SceneDuDvRTarget.value.i)		/* R_SceneDuDvRTarget 0..3 */
extern cGVar GVar_R_SceneMotionBlurValue;
#define g_SceneMotionBlurValue (GVar_R_SceneMotionBlurValue.value.f)	/* R_SceneMotionBlurValue */
extern cGVar GVar_R_SceneMotionBlurOff;
#define g_SceneMotionBlurOff (GVar_R_SceneMotionBlurOff.value.f)	/* R_SceneMotionBlurOff */
extern cGVar GVar_R_CreditsSizeX;
#define g_CreditsSizeX (GVar_R_CreditsSizeX.value.i)		/* R_CreditsSizeX, pow2 */
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

/* the tables of the effects: on (byte), render */
typedef byte (*rtFxOn_t)(void);
typedef void (*rtFxRender_t)(void);

void *NewsPanel_GetTarget(void);

byte RenderTarget_Begin(const char *name);
byte RenderTarget_End(const char *name);
/* RenderTarget_InitD3D and RenderTarget_ReleaseD3D are in render.h */

/* other modules */
/* ScreenCapture.c: the surface as the next ShotNNNN.pcx; ShotCopy through a copy made on its device */
byte Surface_SaveShot(void *surf);
byte Surface_ShotCopy(void *surf);

#endif
