/*
 * The video options of the menu: PCMenu.VideoMenu of the game's scripts/scorer/pcmenu.py and its handlers in C, the
 * layout in data/menus/video.def. ScrapC adds the Display entry (Windowed, Borderless, Fullscreen: FullScreen 0, 2,
 * 1) and a Desktop entry at the front of the video modes (VideoDesktop: the size of the desktop).
 *
 * Native C (docs/objlib-plan.md, stage 2): the state is the struct video, the functions call the engine in C; the
 * PyMethodDef entries are adapters (id and the control as a text, anything else as ""). The options menu after Apply
 * is called through its ref (PORT_CALL: directly while PCMenu holds the port's function, else by name), after Back
 * by name through the alias Menu.OptionsMenu (OBJLIB 5 (9c: a variable of Menu, PCMenu's or XBMenu's function)); a
 * failure is printed as before.
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

static void Step_Prepare(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	(void)id;
	Video_Fill();
}

static void Step_Values(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	video.lastTexFilter = GVar_GetNum("R_TexFilter", 0);
	video.lastMipFilter = GVar_GetNum("R_MipFilter", 0);
	video.lastGamma = GVar_GetNum("VideoGammaRGB", 0);
	video.lastFog = GVar_GetNum("R_FogEnable", 0);
	video.lastLockMode = GVar_GetNum("R_VideoLockMode", 0);
	video.lastLockBuf = GVar_GetNum("R_VideoLockBuf", 0);
	Video_Values(id);
}

static void Step_Display(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	Script_SetStr(id, "Display", "Text", video.display[Video_DisplayIndex(id)]);
}

static const scriptStep_t video_steps[] = {
	{ "prepare", Step_Prepare },
	{ "values", Step_Values },
	{ "display", Step_Display },
	{ NULL, NULL },
};

/* ---- the functions of PCMenu ---- */

#define VIDEO_ARGS(a, n) \
	int id; \
	const char *control; \
	if (!Mode_IdControl(a, n, &id, &control)) { \
		return 0; \
	}

static byte PCMenu_VideoMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Script_RunFile(id, "menus/video.def", NULL, video_def, video_steps, NULL);
	return 1;
}

static byte PCMenu_ChangeVideoMode_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	int before = video.selected;
	Video_NextMode(id);
	if (before == video.selected) {
		video.selected = video.valid[0];
	}
	Video_ShowMode(id);
	return 1;
}

static byte PCMenu_PrevVideoMode_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	Video_PrevMode(id);
	return 1;
}

static byte PCMenu_NextVideoMode_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	Video_NextMode(id);
	return 1;
}

static byte PCMenu_ChangeTextureFilter_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	int i = Video_FilterIndex(id) + 1;
	if (i >= VIDEO_NUM_FILTERS) {
		i = 1;
	}
	Script_SetStr(id, "TextureFilter", "Text", video_filters[i]);
	Video_ApplyTextureFilter(id);
	return 1;
}

static byte PCMenu_ChangeTextureDetail_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	int i = (Video_Index(id, "TextureDetail", video.detail, 3) + 1) % 3;
	Script_SetStr(id, "TextureDetail", "Text", video.detail[i]);
	return 1;
}

static byte PCMenu_ChangeShadowsDetail_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	int i = (Video_Index(id, "ShadowsDetail", video.shadows, 4) + 1) % 4;
	Script_SetStr(id, "ShadowsDetail", "Text", video.shadows[i]);
	return 1;
}

/* a Yes/No item: flips, and Lock and Fog apply at once */
static byte Video_ChangeYesNo(const nativeVal_t *a, int n, const char *item, void (*apply)(int id)) {
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	Video_SetYes(id, item, !Video_Yes(id, item));
	if (apply) {
		apply(id);
	}
	return 1;
}

static byte PCMenu_ChangeVSync_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_ChangeYesNo(a, n, "VSync", NULL);
}

static byte PCMenu_ChangeLock_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_ChangeYesNo(a, n, "Lock", Video_ApplyLock);
}

static byte PCMenu_ChangeFog_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_ChangeYesNo(a, n, "Fog", Video_ApplyFog);
}

static byte PCMenu_ChangeBloom_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_ChangeYesNo(a, n, "Bloom", NULL);
}

static byte PCMenu_ChangeMotionBlur_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_ChangeYesNo(a, n, "MotionBlur", NULL);
}

static byte PCMenu_ChangeDuDv_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_ChangeYesNo(a, n, "DuDv", NULL);
}

static byte PCMenu_ChangeGamma_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Mode_ApplyFlash(id);
	Video_ApplyGamma(id);
	return 1;
}

/* the dummies left and right of the gamma title step the slider and give the focus back */
static byte Video_GammaStep(const nativeVal_t *a, int n, double sign) {
	int id;
	const char *control, *prev;
	if (!Mode_Parse(a, n, "iss", &id, &control, &prev)) {
		return 0;
	}
	double v = Script_GetNum(id, "Gamma", "Value");
	Script_SetNum(id, "Gamma", "Value", v + sign * Script_GetNum(id, "Gamma", "ValueStep"));
	Script_SetDefault(id, prev);
	Mode_ApplyFlash(id);
	Video_ApplyGamma(id);
	return 1;
}

static byte PCMenu_GammaSliderFocus_Sub_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_GammaStep(a, n, -1.0);
}

static byte PCMenu_GammaSliderFocus_Add_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Video_GammaStep(a, n, 1.0);
}

static byte PCMenu_ChangeDisplay_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	static const int next[3] = { 2, 0, 1 };	/* windowed, borderless, fullscreen */
	Mode_ApplyFlash(id);
	Script_SetStr(id, "Display", "Text", video.display[next[Video_DisplayIndex(id)]]);
	return 1;
}

/* Back: the values the menu set at once go back */
static byte PCMenu_VideoMenuBack_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	GVar_SetNum("R_TexFilter", video.lastTexFilter);
	GVar_SetNum("R_MipFilter", video.lastMipFilter);
	GVar_SetNum("VideoGammaRGB", video.lastGamma);
	GVar_SetNum("R_FogEnable", video.lastFog);
	GVar_SetNum("R_VideoLockMode", video.lastLockMode);
	GVar_SetNum("R_VideoLockBuf", video.lastLockBuf);
	/* OBJLIB 5 (9c): the alias Menu.OptionsMenu (= MLib.OptionsMenu of menu.mod) by name */
	Mode_Printed(Native_CallMod("Menu", "OptionsMenu", NULL, "is", (long)id, control), "Menu", "OptionsMenu");
	return 1;
}

static byte PCMenu_VideoDefaults_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
	(void)control;
	Script_SetNum(id, "Gamma", "Value", Video_GammaToValue(1.0f));
	Mode_ApplyFlash(id);
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
	return 1;
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

static byte PCMenu_ApplyChanges_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	VIDEO_ARGS(a, n);
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
	MODE_PRINTED(PCMenu, OptionsMenu, "is", (long)id, control);
	if (reset || mode != video.current) {
		Video_SetCurrentMode(mode);
	}
	return 1;
}

const portFn_t g_MenuVideoMethods[] = {
	PORT_FN(VideoMenu, PCMenu_VideoMenu),
	PORT_FN(VideoMenuBack, PCMenu_VideoMenuBack),
	PORT_FN(VideoDefaults, PCMenu_VideoDefaults),
	PORT_FN(ApplyChanges, PCMenu_ApplyChanges),
	PORT_FN(ChangeVideoMode, PCMenu_ChangeVideoMode),
	PORT_FN(PrevVideoMode, PCMenu_PrevVideoMode),
	PORT_FN(NextVideoMode, PCMenu_NextVideoMode),
	PORT_FN(ChangeTextureFilter, PCMenu_ChangeTextureFilter),
	PORT_FN(ChangeTextureDetail, PCMenu_ChangeTextureDetail),
	PORT_FN(ChangeShadowsDetail, PCMenu_ChangeShadowsDetail),
	PORT_FN(ChangeVSync, PCMenu_ChangeVSync),
	PORT_FN(ChangeLock, PCMenu_ChangeLock),
	PORT_FN(ChangeFog, PCMenu_ChangeFog),
	PORT_FN(ChangeBloom, PCMenu_ChangeBloom),
	PORT_FN(ChangeMotionBlur, PCMenu_ChangeMotionBlur),
	PORT_FN(ChangeDuDv, PCMenu_ChangeDuDv),
	PORT_FN(ChangeGamma, PCMenu_ChangeGamma),
	PORT_FN(GammaSliderFocus_Sub, PCMenu_GammaSliderFocus_Sub),
	PORT_FN(GammaSliderFocus_Add, PCMenu_GammaSliderFocus_Add),
	PORT_FN(ChangeDisplay, PCMenu_ChangeDisplay),
	PORT_FNEND
};

PORTS(MenuVideo)
