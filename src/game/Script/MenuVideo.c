/*
 * The video options of the menu: PCMenu.VideoMenu of the game's scripts/scorer/pcmenu.py and its handlers in C, the
 * layout in data/menus/video.def. ScrapC adds the Display entry (Windowed, Borderless, Fullscreen: FullScreen 0, 2,
 * 1) and a Desktop entry at the front of the video modes (VideoDesktop: the size of the desktop).
 *
 * Native C (docs/objlib-plan.md, stage 2): the state is the struct video, the functions call the engine in C; the
 * PyMethodDef entries are adapters (id and the control as a text, anything else as ""). The options menu after Back
 * and Apply is called by name (OBJLIB 3), a failure printed as before.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/platform.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/video.h"

static const char video_def[] = {
#embed "../../../data/menus/video.def"
	, 0
};

#define VIDEO_MAX_MODES 256
#define VIDEO_DESKTOP (-1)	/* the Desktop entry of the video modes */

typedef struct {
	char text[32];
	int w, h, bpp;
} videoMode_t;

static struct {
	videoMode_t modes[VIDEO_MAX_MODES];
	int numModes;
	int valid[VIDEO_MAX_MODES + 1];	/* the modes the menu offers, VIDEO_DESKTOP first */
	int numValid;
	int current;			/* the mode of the engine when the menu opened */
	int selected;			/* a mode or VIDEO_DESKTOP */
	byte desktop;			/* VideoDesktop when the menu opened */
	char yes[64], no[64];
	char detail[3][64];		/* high, medium, low */
	char shadows[4][64];		/* high, medium, low, none */
	char display[3][64];		/* windowed, fullscreen, borderless: by FullScreen */
	/* the values the menu changes at once, back with Back */
	double lastTexFilter, lastMipFilter, lastGamma, lastFog, lastLockMode, lastLockBuf;
} video;

static const char *const video_filters[] = { "Point", "Bilinear", "Trilinear", "Anisotropic x2", "Anisotropic x4",
	"Anisotropic x8" };
#define VIDEO_NUM_FILTERS ((int)(sizeof(video_filters) / sizeof(video_filters[0])))

/* d, and NeedEngineReset when it differs from the variable */
static void Video_SetReset(const char *name, double d, byte *reset) {
	if (GVar_GetNum(name, 0) != d) {
		*reset = 1;
	}
	GVar_SetNum(name, d);
}

/* ---- the lists ---- */

static void Video_Fill(void) {
	char buf[256];

	video.current = Video_GetCurrentMode();
	video.desktop = GVar_GetNum("VideoDesktop", 0) == 1.0;
	video.selected = video.desktop ? VIDEO_DESKTOP : video.current;
	video.numModes = Video_GetModes(-1, NULL);
	if (video.numModes > VIDEO_MAX_MODES) {
		video.numModes = VIDEO_MAX_MODES;
	}
	for (int i = 0; i < video.numModes; i++) {
		videoMode_t *m = &video.modes[i];
		Video_GetModes(i, buf);
		Str_Copy(m->text, buf, sizeof(m->text));
		m->w = m->h = m->bpp = 0;
		sscanf(buf, "%dx%d (%d bpp)", &m->w, &m->h, &m->bpp);
	}
	video.numValid = 0;
	video.valid[video.numValid++] = VIDEO_DESKTOP;
	for (int i = 0; i < video.numModes; i++) {
		const videoMode_t *m = &video.modes[i];
		if (i == video.current || (m->bpp == 32 && m->w >= 640 && m->h >= 480 && m->w >= m->h)) {
			video.valid[video.numValid++] = i;
		}
	}
	/* with a problem all of them */
	if (video.numValid <= 2) {
		video.numValid = 1;
		for (int i = 0; i < video.numModes; i++) {
			video.valid[video.numValid++] = i;
		}
	}
	Str_Printf(video.yes, sizeof(video.yes), "\x02 %s", Script_Lang("Menu_Question_Yes"));
	Str_Printf(video.no, sizeof(video.no), "\x03 %s", Script_Lang("Menu_Question_No"));
	static const char *const levels[] = { "Menu_Question_High", "Menu_Question_Medium", "Menu_Question_Low" };
	for (int i = 0; i < 3; i++) {
		Str_Copy(video.detail[i], Script_Lang(levels[i]), sizeof(video.detail[i]));
		Str_Printf(video.shadows[i], sizeof(video.shadows[i]), "\x02 %s", Script_Lang(levels[i]));
	}
	Str_Printf(video.shadows[3], sizeof(video.shadows[3]), "\x03 %s", Script_Lang("Menu_Question_No"));
	Str_Copy(video.display[0], Script_Lang("Menu_Video_Windowed"), sizeof(video.display[0]));
	Str_Copy(video.display[1], Script_Lang("Menu_Video_Fullscreen"), sizeof(video.display[1]));
	Str_Copy(video.display[2], Script_Lang("Menu_Video_Borderless"), sizeof(video.display[2]));
}

static const char *Video_ModeText(int mode) {
	if (mode == VIDEO_DESKTOP) {
		return Script_Lang("Menu_Video_Desktop");
	}
	return mode >= 0 && mode < video.numModes ? video.modes[mode].text : "";
}

/* the index of the item text in list (0 when it is not there) */
static int Video_Index(int id, const char *item, const char (*list)[64], int n) {
	const char *t = Script_GetStr(id, item, "Text");
	for (int i = 0; i < n; i++) {
		if (!strcmp(list[i], t)) {
			return i;
		}
	}
	return 0;
}

static int Video_FilterIndex(int id) {
	const char *t = Script_GetStr(id, "TextureFilter", "Text");
	for (int i = 0; i < VIDEO_NUM_FILTERS; i++) {
		if (!strcmp(video_filters[i], t)) {
			return i;
		}
	}
	return 0;
}

static byte Video_Yes(int id, const char *item) {
	return !strcmp(Script_GetStr(id, item, "Text"), video.yes);
}

static void Video_SetYes(int id, const char *item, byte yes) {
	Script_SetStr(id, item, "Text", yes ? video.yes : video.no);
}

/* the Display index: by the text of the item, else by FullScreen */
static int Video_DisplayIndex(int id) {
	const char *t = Script_GetStr(id, "Display", "Text");
	for (int i = 0; i < 3; i++) {
		if (!strcmp(video.display[i], t)) {
			return i;
		}
	}
	int i = (int)GVar_GetNum("FullScreen", 0);
	return i < 0 || i > 2 ? 1 : i;
}

/* ---- the handlers ---- */

static float Video_GammaToValue(float gamma) {
	return gamma <= 1.0f ? gamma - 0.5f : 0.5f + (gamma - 1.0f) / 4.0f;
}

static float Video_ValueToGamma(float value) {
	return value < 0.5f ? 0.5f + value : 1.0f + (value - 0.5f) * 4.0f;
}

/* the Apply button flashes once something changed */
static void Video_ApplyActive(int id) {
	Script_SetStr(id, "Apply", "Effect", "Outline");
	Script_SetNum(id, "Apply", "Red", 255);
	Script_SetNum(id, "Apply", "Green", 255);
	Script_SetNum(id, "Apply", "Blue", 180);
	Script_SetStr(id, "Apply", "OnRender", "!ScorerSpriteFlash");
}

static void Video_ShowMode(int id) {
	Script_SetStr(id, "VideoMode", "Text", Video_ModeText(video.selected));
}

static int Video_ValidPos(void) {
	for (int i = 0; i < video.numValid; i++) {
		if (video.valid[i] == video.selected) {
			return i;
		}
	}
	return 0;
}

static void Video_NextMode(int id) {
	int p = Video_ValidPos();
	if (p + 1 < video.numValid) {
		video.selected = video.valid[p + 1];
	}
	Video_ShowMode(id);
}

static void Video_PrevMode(int id) {
	int p = Video_ValidPos();
	if (p > 0) {
		video.selected = video.valid[p - 1];
	}
	Video_ShowMode(id);
}

static void Video_ApplyTextureFilter(int id) {
	static const double values[VIDEO_NUM_FILTERS][3] = {
		{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 2, 1, 2 }, { 2, 1, 4 }, { 2, 1, 8 },
	};
	const double *v = values[Video_FilterIndex(id)];
	GVar_SetNum("R_TexFilter", v[0]);
	GVar_SetNum("R_MipFilter", v[1]);
	GVar_SetNum("R_Anisotropy", v[2]);
}

static void Video_ApplyLock(int id) {
	GVar_SetNum("R_VideoLockMode", Video_Yes(id, "Lock") ? 1 : 2);
	GVar_SetNum("R_VideoLockBuf", 1);
}

static void Video_ApplyFog(int id) {
	GVar_SetNum("R_FogEnable", Video_Yes(id, "Fog") ? 1 : 0);
}

static void Video_ApplyGamma(int id) {
	GVar_SetNum("VideoGammaRGB", Video_ValueToGamma((float)Script_GetNum(id, "Gamma", "Value")));
}

/* the values of the engine into the items (UpdateVideoOptions) */
static void Video_Values(int id) {
	Script_SetNum(id, "Gamma", "Value", Video_GammaToValue((float)GVar_GetNum("VideoGammaRGB", 0)));
	int filter = 2;
	switch ((int)GVar_GetNum("R_TexFilter", 0)) {
	case 0:
		filter = 0;
		break;
	case 1:
		filter = GVar_GetNum("R_MipFilter", 0) == 0 ? 1 : 2;
		break;
	case 2: {
		int a = (int)GVar_GetNum("R_Anisotropy", 0);
		filter = a == 4 ? 4 : a == 8 ? 5 : 3;
		break;
	}
	}
	Script_SetStr(id, "TextureFilter", "Text", video_filters[filter]);
	int lod = (int)GVar_GetNum("TextureLOD", 0);
	if (lod == 0 || lod == -1) {
		Script_SetStr(id, "TextureDetail", "Text", video.detail[0]);
	} else if (lod == 1 || lod == 2) {
		Script_SetStr(id, "TextureDetail", "Text", video.detail[lod]);
	}
	Video_ShowMode(id);
	if (GVar_GetNum("R_StencilShadows", 0) == -1) {
		static const char *const items[] = { "TitleShadowsDetail", "ShadowsDetail" };
		Script_SetStr(id, "ShadowsDetail", "Text", video.shadows[3]);
		for (int i = 0; i < 2; i++) {
			Script_SetStr(id, items[i], "OnAccept", "");
			Script_SetNum(id, items[i], "Red", Script_GetNum(id, items[i], "Red") * 0.75);
			Script_SetNum(id, items[i], "Green", Script_GetNum(id, items[i], "Green") * 0.75);
			Script_SetNum(id, items[i], "Blue", Script_GetNum(id, items[i], "Blue") * 0.75);
		}
	} else if (GVar_GetNum("R_StencilShadows", 0) == 1) {
		int s = GVar_GetNum("R_StencilSelf", 0) == 1 ? 0 : GVar_GetNum("R_StencilMult", 0) >= 1.0 ? 1 : 2;
		Script_SetStr(id, "ShadowsDetail", "Text", video.shadows[s]);
	} else {
		Script_SetStr(id, "ShadowsDetail", "Text", video.shadows[3]);
	}
	byte scene = GVar_GetNum("R_SceneRTarget", 0) == 1;
	Video_SetYes(id, "VSync", GVar_GetNum("VSync", 0) == 1);
	Video_SetYes(id, "Lock", GVar_GetNum("R_VideoLockMode", 0) == 1);
	Video_SetYes(id, "Fog", GVar_GetNum("R_FogEnable", 0) == 1);
	Video_SetYes(id, "Bloom", scene && GVar_GetNum("R_SceneBloomRTarget", 0) == 1);
	Video_SetYes(id, "MotionBlur", scene && GVar_GetNum("R_SceneMotionBlurRTarget", 0) == 1);
	Video_SetYes(id, "DuDv", scene && GVar_GetNum("R_SceneDuDvRTarget", 0) == 1);
}

/* ---- the steps of video.def ---- */

static void Step_Prepare(int id, PyObject *args) {
	(void)args;
	(void)id;
	Video_Fill();
}

static void Step_Values(int id, PyObject *args) {
	(void)args;
	video.lastTexFilter = GVar_GetNum("R_TexFilter", 0);
	video.lastMipFilter = GVar_GetNum("R_MipFilter", 0);
	video.lastGamma = GVar_GetNum("VideoGammaRGB", 0);
	video.lastFog = GVar_GetNum("R_FogEnable", 0);
	video.lastLockMode = GVar_GetNum("R_VideoLockMode", 0);
	video.lastLockBuf = GVar_GetNum("R_VideoLockBuf", 0);
	Video_Values(id);
}

static void Step_Display(int id, PyObject *args) {
	(void)args;
	Script_SetStr(id, "Display", "Text", video.display[Video_DisplayIndex(id)]);
}

static const scriptStep_t video_steps[] = {
	{ "prepare", Step_Prepare },
	{ "values", Step_Values },
	{ "display", Step_Display },
	{ NULL, NULL },
};

/* ---- the functions of PCMenu ---- */

#define VIDEO_ARGS(args) \
	int id; \
	const char *control; \
	if (!Script_Args(args, &id, &control)) { \
		return NULL; \
	}

static PyObject *PCMenu_VideoMenu(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Script_RunFile(id, "menus/video.def", NULL, video_def, video_steps, NULL);
	return Py_ReturnNone();
}

static PyObject *PCMenu_ChangeVideoMode(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	int before = video.selected;
	Video_NextMode(id);
	if (before == video.selected) {
		video.selected = video.valid[0];
	}
	Video_ShowMode(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_PrevVideoMode(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	Video_PrevMode(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_NextVideoMode(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	Video_NextMode(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_ChangeTextureFilter(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	int i = Video_FilterIndex(id) + 1;
	if (i >= VIDEO_NUM_FILTERS) {
		i = 1;
	}
	Script_SetStr(id, "TextureFilter", "Text", video_filters[i]);
	Video_ApplyTextureFilter(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_ChangeTextureDetail(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	int i = (Video_Index(id, "TextureDetail", video.detail, 3) + 1) % 3;
	Script_SetStr(id, "TextureDetail", "Text", video.detail[i]);
	return Py_ReturnNone();
}

static PyObject *PCMenu_ChangeShadowsDetail(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	int i = (Video_Index(id, "ShadowsDetail", video.shadows, 4) + 1) % 4;
	Script_SetStr(id, "ShadowsDetail", "Text", video.shadows[i]);
	return Py_ReturnNone();
}

/* a Yes/No item: flips, and Lock and Fog apply at once */
static PyObject *Video_ChangeYesNo(PyObject *args, const char *item, void (*apply)(int id)) {
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	Video_SetYes(id, item, !Video_Yes(id, item));
	if (apply) {
		apply(id);
	}
	return Py_ReturnNone();
}

static PyObject *PCMenu_ChangeVSync(PyObject *self, PyObject *args) {
	(void)self;
	return Video_ChangeYesNo(args, "VSync", NULL);
}

static PyObject *PCMenu_ChangeLock(PyObject *self, PyObject *args) {
	(void)self;
	return Video_ChangeYesNo(args, "Lock", Video_ApplyLock);
}

static PyObject *PCMenu_ChangeFog(PyObject *self, PyObject *args) {
	(void)self;
	return Video_ChangeYesNo(args, "Fog", Video_ApplyFog);
}

static PyObject *PCMenu_ChangeBloom(PyObject *self, PyObject *args) {
	(void)self;
	return Video_ChangeYesNo(args, "Bloom", NULL);
}

static PyObject *PCMenu_ChangeMotionBlur(PyObject *self, PyObject *args) {
	(void)self;
	return Video_ChangeYesNo(args, "MotionBlur", NULL);
}

static PyObject *PCMenu_ChangeDuDv(PyObject *self, PyObject *args) {
	(void)self;
	return Video_ChangeYesNo(args, "DuDv", NULL);
}

static PyObject *PCMenu_ChangeGamma(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Video_ApplyActive(id);
	Video_ApplyGamma(id);
	return Py_ReturnNone();
}

/* the dummies left and right of the gamma title step the slider and give the focus back */
static PyObject *Video_GammaStep(PyObject *args, double sign) {
	int id;
	char *control, *prev;
	if (!PyArg_ParseTuple(args, "iss", &id, &control, &prev)) {
		return NULL;
	}
	double v = Script_GetNum(id, "Gamma", "Value");
	Script_SetNum(id, "Gamma", "Value", v + sign * Script_GetNum(id, "Gamma", "ValueStep"));
	Script_SetDefault(id, prev);
	Video_ApplyActive(id);
	Video_ApplyGamma(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_GammaSliderFocus_Sub(PyObject *self, PyObject *args) {
	(void)self;
	return Video_GammaStep(args, -1.0);
}

static PyObject *PCMenu_GammaSliderFocus_Add(PyObject *self, PyObject *args) {
	(void)self;
	return Video_GammaStep(args, 1.0);
}

static PyObject *PCMenu_ChangeDisplay(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	static const int next[3] = { 2, 0, 1 };	/* windowed, borderless, fullscreen */
	Video_ApplyActive(id);
	Script_SetStr(id, "Display", "Text", video.display[next[Video_DisplayIndex(id)]]);
	return Py_ReturnNone();
}

/* Back: the values the menu set at once go back */
static PyObject *PCMenu_VideoMenuBack(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	GVar_SetNum("R_TexFilter", video.lastTexFilter);
	GVar_SetNum("R_MipFilter", video.lastMipFilter);
	GVar_SetNum("VideoGammaRGB", video.lastGamma);
	GVar_SetNum("R_FogEnable", video.lastFog);
	GVar_SetNum("R_VideoLockMode", video.lastLockMode);
	GVar_SetNum("R_VideoLockBuf", video.lastLockBuf);
	Mode_Printed(Native_CallMod("Menu", "OptionsMenu", NULL, "is", (long)id, control), "Menu", "OptionsMenu");
	return Py_ReturnNone();
}

static PyObject *PCMenu_VideoDefaults(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	(void)control;
	Script_SetNum(id, "Gamma", "Value", Video_GammaToValue(1.0f));
	Video_ApplyActive(id);
	Video_ApplyGamma(id);
	Script_SetStr(id, "TextureFilter", "Text", video_filters[2]);
	Video_ApplyTextureFilter(id);
	Script_SetStr(id, "TextureDetail", "Text", video.detail[0]);
	Script_SetStr(id, "ShadowsDetail", "Text", video.shadows[0]);
	Video_SetYes(id, "VSync", 0);
	Video_SetYes(id, "Lock", 0);
	Video_ApplyLock(id);
	Video_SetYes(id, "Fog", 1);
	Video_ApplyFog(id);
	Video_SetYes(id, "Bloom", 1);
	Video_SetYes(id, "MotionBlur", 1);
	Video_SetYes(id, "DuDv", 1);
	return Py_ReturnNone();
}

/* the mode of the desktop size, 32 bpp first (else the largest one) */
static int Video_DesktopMode(void) {
	int w, h, best = video.current, score = -1;
	Plat_DesktopSize(&w, &h);
	for (int i = 0; i < video.numModes; i++) {
		const videoMode_t *m = &video.modes[i];
		int s = m->w * m->h + (m->w == w && m->h == h ? 1 << 30 : 0) + (m->bpp == 32);
		if (s > score) {
			score = s;
			best = i;
		}
	}
	return best;
}

static PyObject *PCMenu_ApplyChanges(PyObject *self, PyObject *args) {
	(void)self;
	VIDEO_ARGS(args);
	byte reset = 0;

	/* ScrapC: the display mode first, the engine reset applies it */
	int display = Video_DisplayIndex(id);
	if (display != (int)GVar_GetNum("FullScreen", 0)) {
		reset = 1;
	}
	GVar_SetNum("FullScreen", display);
	byte desktop = video.selected == VIDEO_DESKTOP;
	int mode = desktop ? Video_DesktopMode() : video.selected;
	if (desktop != video.desktop) {
		reset = 1;
	}
	GVar_SetNum("VideoDesktop", desktop);

	Video_ApplyGamma(id);
	Video_ApplyTextureFilter(id);
	Video_SetReset("TextureLOD", Video_Index(id, "TextureDetail", video.detail, 3), &reset);
	int shadows = Video_Index(id, "ShadowsDetail", video.shadows, 4);
	if (shadows < 3) {
		Video_SetReset("R_StencilShadows", 1, &reset);
		GVar_SetNum("R_StencilSelf", shadows == 0);
		GVar_SetNum("R_StencilMult", shadows == 2 ? 0.8 : 1.0);
	} else {
		Video_SetReset("R_StencilShadows", 0, &reset);
	}
	Video_SetReset("VSync", Video_Yes(id, "VSync"), &reset);
	Video_ApplyLock(id);
	Video_ApplyFog(id);
	byte bloom = Video_Yes(id, "Bloom"), blur = Video_Yes(id, "MotionBlur"), dudv = Video_Yes(id, "DuDv");
	Video_SetReset("R_SceneBloomRTarget", bloom, &reset);
	Video_SetReset("R_SceneMotionBlurRTarget", blur, &reset);
	Video_SetReset("R_SceneDuDvRTarget", dudv, &reset);
	Video_SetReset("R_SceneRTarget", bloom || blur || dudv, &reset);
	Mode_Printed(Native_CallMod("PCMenu", "OptionsMenu", NULL, "is", (long)id, control), "PCMenu", "OptionsMenu");
	if (reset || mode != video.current) {
		Video_SetCurrentMode(mode);
	}
	return Py_ReturnNone();
}

PyMethodDef g_MenuVideoMethods[] = {
	{ "VideoMenu", PCMenu_VideoMenu, 1, NULL },
	{ "VideoMenuBack", PCMenu_VideoMenuBack, 1, NULL },
	{ "VideoDefaults", PCMenu_VideoDefaults, 1, NULL },
	{ "ApplyChanges", PCMenu_ApplyChanges, 1, NULL },
	{ "ChangeVideoMode", PCMenu_ChangeVideoMode, 1, NULL },
	{ "PrevVideoMode", PCMenu_PrevVideoMode, 1, NULL },
	{ "NextVideoMode", PCMenu_NextVideoMode, 1, NULL },
	{ "ChangeTextureFilter", PCMenu_ChangeTextureFilter, 1, NULL },
	{ "ChangeTextureDetail", PCMenu_ChangeTextureDetail, 1, NULL },
	{ "ChangeShadowsDetail", PCMenu_ChangeShadowsDetail, 1, NULL },
	{ "ChangeVSync", PCMenu_ChangeVSync, 1, NULL },
	{ "ChangeLock", PCMenu_ChangeLock, 1, NULL },
	{ "ChangeFog", PCMenu_ChangeFog, 1, NULL },
	{ "ChangeBloom", PCMenu_ChangeBloom, 1, NULL },
	{ "ChangeMotionBlur", PCMenu_ChangeMotionBlur, 1, NULL },
	{ "ChangeDuDv", PCMenu_ChangeDuDv, 1, NULL },
	{ "ChangeGamma", PCMenu_ChangeGamma, 1, NULL },
	{ "GammaSliderFocus_Sub", PCMenu_GammaSliderFocus_Sub, 1, NULL },
	{ "GammaSliderFocus_Add", PCMenu_GammaSliderFocus_Add, 1, NULL },
	{ "ChangeDisplay", PCMenu_ChangeDisplay, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
