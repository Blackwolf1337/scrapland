/*
 * The menu drawing helpers of Menu/ between TimerScorer.cpp and WaypointScorer.cpp (no CHECK names their file):
 * a stretched frame, the 3x3 frame of a box, and the native OnRender functions of the menu items ("!ScorerSpriteFadeIn"
 * and the others: the parameters are the player and the item name in a memory file). See include/scrap/scorerhud.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/scorerhud.h"
#include "scrap/vehicleentity.h"
#include "scrap/staticinit.h"

void Menu_DrawStretch(CPsaImage *img, int frame, float x, float y, float w, float h) {
	img->scaleX = (w - 18.0f) / 9.0f;
	img->scaleY = h;
	CPsaImage_DrawFrameFlip(img, frame + 1, x + 9.0f, y, 0, 0);
	img->scaleY = h;
	img->scaleX = 1.0f;
	CPsaImage_DrawFrameFlip(img, frame, (x + w) - 9.0f, y, 0, 1);
	CPsaImage_DrawFrameFlip(img, frame, x, y, 0, 0);
	img->scaleX = 1.0f;
	img->scaleY = 1.0f;
}

void Menu_DrawFrame(void *image, int frame, float x, float y, float w, float h) {
	CPsaImage *img = image;

	/* the corners */
	img->scaleX = 1.0f;
	img->scaleY = 1.0f;
	CPsaImage_DrawFrameFlip(img, frame, x, y, 0, 0);
	CPsaImage_DrawFrameFlip(img, frame, (w + x) - 7.0f, y, 1, 0);
	CPsaImage_DrawFrameFlip(img, frame, (w + x) - 7.0f, (h + y) - 7.0f, 2, 0);
	CPsaImage_DrawFrameFlip(img, frame, x, (h + y) - 7.0f, 3, 0);
	/* the sides, then the middle */
	img->scaleY = 1.0f;
	img->scaleX = (w - 14.0f) / 7.0f;
	CPsaImage_DrawFrameFlip(img, frame + 1, x + 7.0f, y, 0, 0);
	CPsaImage_DrawFrameFlip(img, frame + 1, x + 7.0f, (h + y) - 7.0f, 0, 2);
	img->scaleY = 1.0f;
	img->scaleX = (h - 14.0f) / 7.0f;
	CPsaImage_DrawFrameFlip(img, frame + 1, x, y + 7.0f, 1, 2);
	CPsaImage_DrawFrameFlip(img, frame + 1, (w + x) - 7.0f, y + 7.0f, 1, 0);
	img->scaleX = (w - 14.0f) * 0.142857149f;
	img->scaleY = (h - 14.0f) * 0.142857149f;
	CPsaImage_DrawFrameFlip(img, frame + 2, x + 7.0f, y + 7.0f, 0, 0);
	img->scaleX = 1.0f;
	img->scaleY = 1.0f;
}

/* a quad of the bar: pos as given, the texture rectangle at (bu, bv) of size (ud, vd) from the factors fu0..fu1 and
 * fv0..fv1 of it; the color of the image col, its old color when col is 0 */
static void Bar_Quad(CPsaImage *img, const float *pos, float bu, float bv, float ud, float vd, float fu0, float fu1,
	float fv0, float fv1, uint32_t col, uint32_t old) {
	float uv[8];

	img->color = col ? col : old;
	uv[0] = bu + ud * fu0;
	uv[1] = bv + vd * fv0;
	uv[2] = bu + ud * fu1;
	uv[3] = bv + vd * fv0;
	uv[4] = bu + ud * fu1;
	uv[5] = bv + vd * fv1;
	uv[6] = bu + ud * fu0;
	uv[7] = bv + vd * fv1;
	CPsaImage_DrawPoly(img, pos, uv, NULL, 4, 6);
}

/* mode 1: the bar grows down (else right), 2: the slant of f the other way, 4: the bar from the other end. The
 * rest of the bar (the black rectangle) first, the bar after it; f slants the end by its part of the size. The
 * color of the image is left at the last one */
void Menu_DrawBar(void *image, float x, float y, float frac, int mode, float f, float u0, float v0, float u1, float v1,
	float bu0, float bv0, float bu1, float bv1, uint32_t color, uint32_t black) {
	CPsaImage *img = image;
	float du = u1 - u0;
	float dv = v1 - v0;
	uint32_t old = img->color;
	byte rev = (mode & 4) != 0;
	byte b2 = (mode & 2) != 0;
	float pos[8];
	float t, s, w, h;

	img->x = x;
	img->y = y;
	t = rev ? 1.0f - frac : frac;
	if (mode & 1) {
		float a, b;

		s = (img->scaleX / img->scaleY) * f;
		w = du * (1.0f - s);
		h = dv * (1.0f - s);
		a = b2 ? s : 0.0f;
		b = b2 ? 0.0f : s;
		if (1.0f > t) {
			pos[0] = w * 0.0f;
			pos[1] = h * (a + t);
			pos[2] = w;
			pos[3] = h * (b + t);
			pos[4] = w;
			pos[5] = h * (b + 1.0f);
			pos[6] = w * 0.0f;
			pos[7] = h * (a + 1.0f);
			if (rev) {
				Bar_Quad(img, pos, u0, v0, du, dv, 0.0f, 1.0f, t, 1.0f, color, old);
			} else {
				Bar_Quad(img, pos, bu0, bv0, bu1 - bu0, bv1 - bv0, 0.0f, 1.0f, t, 1.0f, black, old);
			}
		}
		pos[0] = w * 0.0f;
		pos[1] = h * a;
		pos[2] = w;
		pos[3] = h * b;
		pos[4] = w;
		pos[5] = h * (b + t);
		pos[6] = w * 0.0f;
		pos[7] = (a + t) * h;
		if (rev) {
			Bar_Quad(img, pos, bu0, bv0, bu1 - bu0, bv1 - bv0, 0.0f, 1.0f, 0.0f, t, black, old);
		} else {
			Bar_Quad(img, pos, u0, v0, u1 - u0, v1 - v0, 0.0f, 1.0f, 0.0f, t, color, old);
		}
	} else {
		float a, c;

		s = (img->scaleY / img->scaleX) * f;
		h = dv * (1.0f - s);
		w = du * (1.0f - s);
		a = b2 ? s : 0.0f;
		c = b2 ? 0.0f : s;
		if (1.0f > t) {
			pos[0] = (a + t) * w;
			pos[1] = h * 0.0f;
			pos[2] = (a + 1.0f) * w;
			pos[3] = h * 0.0f;
			pos[4] = (c + 1.0f) * w;
			pos[5] = h;
			pos[6] = (c + t) * w;
			pos[7] = h;
			if (rev) {
				Bar_Quad(img, pos, u0, v0, du, dv, t, 1.0f, 0.0f, 1.0f, color, old);
			} else {
				Bar_Quad(img, pos, bu0, bv0, bu1 - bu0, bv1 - bv0, t, 1.0f, 0.0f, 1.0f, black, old);
			}
		}
		pos[0] = w * a;
		pos[1] = h * 0.0f;
		pos[2] = (a + t) * w;
		pos[3] = h * 0.0f;
		pos[4] = (c + t) * w;
		pos[5] = h;
		pos[6] = w * c;
		pos[7] = h;
		if (rev) {
			Bar_Quad(img, pos, bu0, bv0, bu1 - bu0, bv1 - bv0, 0.0f, t, 0.0f, 1.0f, black, old);
		} else {
			Bar_Quad(img, pos, u0, v0, u1 - u0, v1 - v0, 0.0f, t, 0.0f, 1.0f, color, old);
		}
	}
}

/* the parameters of the native functions: the player, the item name (size bytes) */
static cScorer *Menu_ReadParams(cAbstractFile *params, int *player, char *name, int size) {
	params->vftable->Read(params, player, 4);
	cAbstractFile_ReadString(params, name, size);
	return Scorer_Get(*player);
}

static int MenuItem_SetInt(cScorerItem *item, const char *var, int *v) {
	return item->vftable->Var(item, var, 1, NULL, NULL, v);
}

static int MenuItem_SetFloat(cScorerItem *item, const char *var, double *v) {
	return item->vftable->Var(item, var, 1, NULL, v, NULL);
}

static int MenuItem_GetAlpha(cScorerItem *item) {
	int a = 0;
	item->vftable->Var(item, "Alpha", 0, NULL, NULL, &a);
	return a;
}

/* the fade step of this frame in alpha units, at least 1 */
static int Menu_FadeStep(cScorer *sc) {
	int step = (int)((sc->time - sc->lastTime) * 1000.0f);
	return step < 1 ? 1 : step;
}

cGVar GVar_FadeOutFinalTime;

#define g_FadeOutFinalTime (GVar_FadeOutFinalTime.value.f)	/* the time of ScorerSpriteFadeOutFinal */

float g_MetroRing;

/* the four rings name0..name3 (the last letter of the name replaced) growing and fading around the player mark */
void Scorer_MetroUsrHere(cAbstractFile *params) {
	char name[0x14];
	cScorer *sc;
	double scale;
	size_t len;
	int player, alpha;
	float v;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc) {
		return;
	}
	v = sc->frameTime + g_MetroRing;
	g_MetroRing = v;
	if (0.0f > v) {
		g_MetroRing = 0.0f;
	} else if (v > 1.0f) {
		g_MetroRing = 2.0f > v ? v - 1.0f : 0.0f;
	}
	len = strlen(name);
	for (int k = 0; k < 4; k++) {
		cScorerItem *item;
		float q;

		v = g_MetroRing;
		if (k == 0) {
			alpha = (int)(v * 255.0f);
		} else if (k == 3) {
			alpha = 0xff - (int)(v * 255.0f);
		} else {
			alpha = 0xff;
		}
		name[len - 1] = (char)('0' + k);
		q = (float)k * 0.25f;
		scale = (double)((q + q) + v * 0.5f);
		item = cScorer_FindItem(sc, name);
		if (item) {
			MenuItem_SetInt(item, "Alpha", &alpha);
			MenuItem_SetFloat(item, "ScaleX", &scale);
			MenuItem_SetFloat(item, "ScaleY", &scale);
		}
	}
}

/* the item fades in; at full alpha the OnRender is dropped and the item shown */
void Scorer_SpriteFadeIn(cAbstractFile *params) {
	char name[0x14];
	cScorerItem *item;
	cScorer *sc;
	int player, a;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc || !(item = cScorer_FindItem(sc, name))) {
		return;
	}
	a = MenuItem_GetAlpha(item) + Menu_FadeStep(sc);
	if (a > 0xff) {
		a = 0xff;
	}
	MenuItem_SetInt(item, "Alpha", &a);
	if (a >= 0xff) {
		if (cPy_Function_IsValid(&item->onRender)) {
			cPy_Function_Clear(&item->onRender);
		}
		item->visible = 1;
	}
}

/* the item fades out; at 0 the OnRender is dropped and the item hidden */
void Scorer_SpriteFadeOut(cAbstractFile *params) {
	char name[0x14];
	cScorerItem *item;
	cScorer *sc;
	int player, a;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc || !(item = cScorer_FindItem(sc, name))) {
		return;
	}
	a = MenuItem_GetAlpha(item) - Menu_FadeStep(sc);
	if (a <= 0) {
		a = 0;
	}
	MenuItem_SetInt(item, "Alpha", &a);
	if (a <= 0) {
		if (cPy_Function_IsValid(&item->onRender)) {
			cPy_Function_Clear(&item->onRender);
		}
		item->visible = 0;
	}
}

float g_FadeOutFinalAcc;

/* the item comes in over g_FadeOutFinalTime (the time summed in a global); the OnRender is dropped at full alpha */
void Scorer_SpriteFadeOutFinal(cAbstractFile *params) {
	char name[0x14];
	cScorerItem *item;
	cScorer *sc;
	float step;
	int player, a;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc || !(item = cScorer_FindItem(sc, name))) {
		return;
	}
	step = g_FadeOutFinalTime / 255.0f;
	MenuItem_GetAlpha(item);
	g_FadeOutFinalAcc = (sc->time - sc->lastTime) + g_FadeOutFinalAcc;
	a = (int)(g_FadeOutFinalAcc / step);
	if (a > 0xff) {
		a = 0xff;
	}
	MenuItem_SetInt(item, "Alpha", &a);
	if (a >= 0xff) {
		g_FadeOutFinalAcc = 0.0f;
		if (cPy_Function_IsValid(&item->onRender)) {
			cPy_Function_Clear(&item->onRender);
		}
	}
}

/* a short flash at the start of each 255/450 seconds */
void Scorer_SpriteFlash(cAbstractFile *params) {
	char name[0x14];
	cScorerItem *item;
	cScorer *sc;
	float f;
	int player, a;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc || !(item = cScorer_FindItem(sc, name))) {
		return;
	}
	f = (float)Scorer_Abs((int)((double)sc->time * 450.0) % 0xff) / 255.0f;
	if (0.05f >= f) {
		a = (int)(f * 255.0f * 20.0f);
	} else if (0.5f >= f) {
		float g = f * 255.0f;
		a = 0xff - (int)(g + g);
	} else {
		a = 0;
	}
	MenuItem_SetInt(item, "Alpha", &a);
}

/* the alpha swings between 128 and 255 */
void Scorer_SpriteGlow(cAbstractFile *params) {
	char name[0x14];
	cScorerItem *item;
	cScorer *sc;
	int player, a;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc || !(item = cScorer_FindItem(sc, name))) {
		return;
	}
	a = (Scorer_Abs((int)((double)sc->time * 680.0) % 0xff - 0x7f) & 0xff) + 0x80;
	MenuItem_SetInt(item, "Alpha", &a);
}

/* the boost of the ship of the camera into the status item: BoostStatus 0xff ready, 0x80/0x40 blinking while the
 * clouds are on, 0x40 in use; BoostRemaining the part left */
void Scorer_BoostScorer(cAbstractFile *params) {
	char name[0x80];
	cScorerItem *item;
	cEntity *ship;
	cScorer *sc;
	double rem;
	int player, status;

	sc = Menu_ReadParams(params, &player, name, sizeof(name));
	if (!sc || !(item = cScorer_FindItem(sc, name)) || Input_GetActionSet() != 1) {
		return;
	}
	if (!ScorerHud_CamShip(player, &ship)) {
		return;
	}
	cVehicleEntity *v = (cVehicleEntity *)ship;
	if (v->boost == 0.0f) {
		if (v->cloudOn) {
			double t = ((g_pApp->vftable->GetGameTime(g_pApp)) * 4.0);
			status = (int)t & 1 ? 0x40 : 0x80;
		} else {
			status = 0xff;
		}
	} else {
		status = 0x40;
	}
	rem = (double)(v->boost / v->boostTime);
	MenuItem_SetInt(item, "BoostStatus", &status);
	MenuItem_SetFloat(item, "BoostRemaining", &rem);
}

cPy_NativeFunc NativeFunc_ScorerMetroUsrHere;
cPy_NativeFunc NativeFunc_ScorerSpriteGlow;
cPy_NativeFunc NativeFunc_ScorerSpriteFlash;
cPy_NativeFunc NativeFunc_ScorerSpriteFadeIn;
cPy_NativeFunc NativeFunc_ScorerSpriteFadeOut;
cPy_NativeFunc NativeFunc_ScorerSpriteFadeOutFinal;
cPy_NativeFunc NativeFunc_BoostScorer;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_FadeOutFinalTime, &GVar_FadeOutFinalTime, "FadeOutFinalTime", 0, "Fade out final time", 5.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_ScorerMetroUsrHere, &NativeFunc_ScorerMetroUsrHere, "ScorerMetroUsrHere", Scorer_MetroUsrHere)
INIT_NATIVE_FUNC(Init_NativeFunc_ScorerSpriteGlow, &NativeFunc_ScorerSpriteGlow, "ScorerSpriteGlow", Scorer_SpriteGlow)
INIT_NATIVE_FUNC(Init_NativeFunc_ScorerSpriteFlash, &NativeFunc_ScorerSpriteFlash, "ScorerSpriteFlash", Scorer_SpriteFlash)
INIT_NATIVE_FUNC(Init_NativeFunc_ScorerSpriteFadeIn, &NativeFunc_ScorerSpriteFadeIn, "ScorerSpriteFadeIn", Scorer_SpriteFadeIn)
INIT_NATIVE_FUNC(Init_NativeFunc_ScorerSpriteFadeOut, &NativeFunc_ScorerSpriteFadeOut, "ScorerSpriteFadeOut", Scorer_SpriteFadeOut)
INIT_NATIVE_FUNC(Init_NativeFunc_ScorerSpriteFadeOutFinal, &NativeFunc_ScorerSpriteFadeOutFinal, "ScorerSpriteFadeOutFinal", Scorer_SpriteFadeOutFinal)
INIT_NATIVE_FUNC(Init_NativeFunc_BoostScorer, &NativeFunc_BoostScorer, "BoostScorer", Scorer_BoostScorer)
