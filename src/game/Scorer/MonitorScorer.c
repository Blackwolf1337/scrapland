/*
 * MonitorScorer.cpp of the game (Menu/): cMonitorScorer, the ship monitor with the talking head, and the head
 * of the scripts (Scorer_SetHeadMonitor). See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/embeddedfxchar.h"
#include "scrap/console.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/objentity.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define CMONITORSCORER_VFTABLE (&cMonitorScorer_vftable.base)
#define g_FirstPhotoMessage (GVar_FirstPhotoMessage.value.s)	/* FirstPhotoMessage */
static const cHeadScorerVtbl cMonitorScorer_vftable;
static cGVar GVar_FirstPhotoMessage;

static CPsaImage *Monitor_NewImage(const char *name, int blend, uint32_t flags, uint32_t color) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	if (img) {
		img = blend < 0 ? CPsaImage_ctor(img, name, flags) : CPsaImage_ctorFlags(img, name, 0, blend, flags);
	}
	img->color = color;
	return img;
}

static void Monitor_FreeImage(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
}

static CPsaImage *Monitor_WhiteImage(uint32_t flags) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	return img ? CPsaImage_ctor(img, "2D/white.tga", flags) : NULL;
}

cMonitorScorer *cMonitorScorer_ctor(cMonitorScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CMONITORSCORER_VFTABLE;
	self->panel = Monitor_WhiteImage(0x10000);
	self->mask = Monitor_WhiteImage(0x10000);
	self->frame = Monitor_WhiteImage(0x10000);
	self->staticImg = Monitor_WhiteImage(0x10000);
	self->objective = Monitor_WhiteImage(0x10000);
	self->weapon = Monitor_WhiteImage(0x10000);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->noiseImg = img ? CPsaImage_ctorFlags(img, "2D/white.tga", 0, 3, 0x2030) : NULL;
	self->img350 = Monitor_WhiteImage(0x2024);
	self->textTime = 0.0f;
	self->noise = 4.0f;
	CGameCamera *cam = operator_new(sizeof(CGameCamera));
	self->cam = cam ? CGameCamera_ctor(cam) : NULL;
	self->lastTarget = NULL;
	self->text[0] = 0;
	self->model = NULL;
	self->textShown = 0.0f;
	self->smallFont = g_FontTerminal8;
	self->bigFont = g_FontHoratio;
	self->targetView = 0;
	self->anim = 1;
	self->objPos[1] = 0.0f;
	self->objPos[0] = 0.0f;
	self->objFrom[0] = 0.0f;
	self->objFrom[1] = self->objPos[1];
	self->objName[0] = 0;
	self->objBlend = 1.0f;
	self->objWas = 0;
	self->canPossessIndex = 0;
	self->canPossessGlowIndex = 0;
	return self;
}

static void cMonitorScorer_dtor(cMonitorScorer *self) {
	CPsaImage *img = self->panel;
	self->base.vftable = CMONITORSCORER_VFTABLE;
	Monitor_FreeImage(img);
	Monitor_FreeImage(self->mask);
	Monitor_FreeImage(self->frame);
	Monitor_FreeImage(self->staticImg);
	Monitor_FreeImage(self->objective);
	Monitor_FreeImage(self->weapon);
	if (self->cam) {
		CGameCamera_dtor(self->cam);
		operator_delete_sized(self->cam, sizeof(CGameCamera));
	}
	Monitor_FreeImage(self->noiseImg);
	Monitor_FreeImage(self->img350);
	cScorerItem_dtor(&self->base);
}

static cScorerItem *cMonitorScorer_Delete(cScorerItem *base, byte flags) {
	cMonitorScorer *self = (cMonitorScorer *)base;
	cMonitorScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cMonitorScorer));
	}
	return base;
}

/* vf01: TargetView=1 */
static void cMonitorScorer_SetParam(cScorerItem *base, const char *name, const char *value) {
	cMonitorScorer *self = (cMonitorScorer *)base;
	if (!crt_stricmp(name, "TargetView")) {
		self->targetView = *value == '1';
	}
}

/* a sprite variable of the scripts (set only): the old sheet freed, the new one */
static int Monitor_VarSprite(CPsaImage **img, const char *name, int blend, uint32_t flags, uint32_t color) {
	Monitor_FreeImage(*img);
	*img = Monitor_NewImage(name, blend, flags, color);
	return 1;
}

static int cMonitorScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cMonitorScorer *self = (cMonitorScorer *)base;
	if (s && !crt_stricmp(name, "PanelSprite") && set) {
		return Monitor_VarSprite(&self->panel, *s, -1, 0x10040, 0xffffffff);
	}
	if (i && !crt_stricmp(name, "PanelIndex")) {
		return ScorerVar_Int(&self->panelIndex, set, i);
	}
	if (s && !crt_stricmp(name, "MaskSprite") && set) {
		return Monitor_VarSprite(&self->mask, *s, 2, 0x11040, 0xffffffff);
	}
	if (i && !crt_stricmp(name, "MaskIndex")) {
		return ScorerVar_Int(&self->maskIndex, set, i);
	}
	if (s && !crt_stricmp(name, "FrameSprite") && set) {
		return Monitor_VarSprite(&self->frame, *s, -1, 0x10060, 0xffffffff);
	}
	if (i && !crt_stricmp(name, "FrameIndex")) {
		return ScorerVar_Int(&self->frameIndex, set, i);
	}
	if (s && !crt_stricmp(name, "ObjetiveSprite") && set) {
		return Monitor_VarSprite(&self->objective, *s, 3, 0x10048, 0xffffff);
	}
	if (i) {
		if (!crt_stricmp(name, "ObjetiveIndex")) {
			return ScorerVar_Int(&self->objectiveIndex, set, i);
		}
		if (!crt_stricmp(name, "ObjetiveChargeIndex")) {
			return ScorerVar_Int(&self->objectiveChargeIndex, set, i);
		}
		if (!crt_stricmp(name, "ObjetiveBlackIndex")) {
			return ScorerVar_Int(&self->objectiveBlackIndex, set, i);
		}
		if (!crt_stricmp(name, "ObjetiveWhiteIndex")) {
			return ScorerVar_Int(&self->objectiveWhiteIndex, set, i);
		}
	}
	if (s && !crt_stricmp(name, "StaticSprite") && set) {
		return Monitor_VarSprite(&self->staticImg, *s, 3, 0x2064, 0xffffffff);
	}
	if (i && !crt_stricmp(name, "StaticIndex")) {
		return ScorerVar_Int(&self->staticIndex, set, i);
	}
	if (s && !crt_stricmp(name, "NoiseSprite")) {
		if (set) {
			return Monitor_VarSprite(&self->noiseImg, *s, 3, 0x2030, 0xffffffff);
		}
	} else if (set && s) {
		if (!crt_stricmp(name, "BigFont")) {
			CFont *f = Font_Get(*s);
			if (f) {
				self->bigFont = f;
				return 0;
			}
			return 1;
		}
		if (!crt_stricmp(name, "SmallFont")) {
			CFont *f = Font_Get(*s);
			if (f) {
				self->smallFont = f;
				return 0;
			}
			return 1;
		}
		if (!crt_stricmp(name, "HeadModel")) {
			HeadScorer_SetModel(self, *s);
			return 1;
		}
	}
	if (i && !crt_stricmp(name, "HeadAnimIndex")) {
		if (set) {
			HeadScorer_SetAnim(self, *i);
		} else {
			*i = self->anim;
		}
		return 3;
	}
	if (d && !crt_stricmp(name, "Noise")) {
		return ScorerVar_Float(&self->noise, set, d);
	}
	if (s && !crt_stricmp(name, "Text")) {
		if (set) {
			Str_Copy(self->text, *s, sizeof(self->text));
			self->textTime = 0.0f;
			self->textShown = 0.0f;
		} else {
			*s = self->text;
		}
		return 1;
	}
	if (i && !crt_stricmp(name, "TargetView")) {
		if (set) {
			self->targetView = *i != 0;
		} else {
			*i = self->targetView != 0;
		}
		return 3;
	}
	if (s && !crt_stricmp(name, "WeaponSprite") && set) {
		return Monitor_VarSprite(&self->weapon, *s, 3, 0x10040, 0xffffffff);
	}
	if (d) {
		if (!crt_stricmp(name, "WeaponOffX")) {
			return ScorerVar_Float(&self->weaponOffX, set, d);
		}
		if (!crt_stricmp(name, "WeaponOffY")) {
			return ScorerVar_Float(&self->weaponOffY, set, d);
		}
		if (!crt_stricmp(name, "WeaponIconScale")) {
			if (set) {
				double v = *d;
				CPsaImage *img = self->weapon;
				img->scaleX = (float)v;
				img->scaleY = (float)v;
			} else {
				*d = (double)self->weapon->scaleX;
			}
			return 2;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "WeaponIconAc")) {
			return ScorerVar_Int(&self->weaponIconAc, set, i);
		}
		if (!crt_stricmp(name, "WeaponIconIndex")) {
			return ScorerVar_Int(&self->weaponIcons[self->weaponIconAc], set, i);
		}
		if (!crt_stricmp(name, "UpgradeIconAc")) {
			return ScorerVar_Int(&self->upgradeIconAc, set, i);
		}
		if (!crt_stricmp(name, "UpgradeIconIndex")) {
			return ScorerVar_Int(&self->upgradeIcons[self->upgradeIconAc], set, i);
		}
		if (!crt_stricmp(name, "CanPossessIndex")) {
			return ScorerVar_Int(&self->canPossessIndex, set, i);
		}
		if (!crt_stricmp(name, "CanPossessGlowIndex")) {
			return ScorerVar_Int(&self->canPossessGlowIndex, set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* ---- the monitor ---- */

#define MONITOR_W 118.0f		/* the view of the monitor */
#define MONITOR_H 96.0f
#define MONITOR_BORDER 5.5f		/* the panel and the mask corners around the view */
#define MONITOR_CORNER 27.0f
#define MONITOR_HEAD_SWAY 25.0f		/* the camera of the head: sway and height */
#define MONITOR_HEAD_BACK -5.0f
#define MONITOR_HEAD_FOV 0.87266463f	/* 50 degrees */
#define MONITOR_VIEW_FOV 0.34906584f	/* 20 degrees, the view of the target */
#define MONITOR_VIEW_ASPECT 1.22916663f
#define MONITOR_VIEW_FAR 2000.0f	/* the camera of the target: at most this far from it */
#define MONITOR_TEXT_SPEED 20.0f	/* letters a second */
#define MONITOR_SIGNAL_X 59.0f		/* NO SIGNAL, the place of the objective mark when there is no target */
#define MONITOR_SIGNAL_Y 48.0f
#define MONITOR_STATIC_H 24.0f		/* the static band runs over the view every 6 seconds */

/* the frame of the mask in the four corners of the view, mirrored */
static void Monitor_DrawMask(cMonitorScorer *self) {
	CPsaImage *m = self->mask;
	float x = self->base.x, y = self->base.y;

	CPsaImage_DrawFrameFlip(m, self->maskIndex, x - MONITOR_BORDER, y - MONITOR_BORDER, 0, 0);
	CPsaImage_DrawFrameFlip(m, self->maskIndex, x + MONITOR_W + MONITOR_BORDER - MONITOR_CORNER, y - MONITOR_BORDER, 1,
		0);
	CPsaImage_DrawFrameFlip(m, self->maskIndex, x - MONITOR_BORDER, y + MONITOR_H + MONITOR_BORDER - MONITOR_CORNER, 0,
		2);
	CPsaImage_DrawFrameFlip(m, self->maskIndex, x + MONITOR_W + MONITOR_BORDER - MONITOR_CORNER,
		y + MONITOR_H + MONITOR_BORDER - MONITOR_CORNER, 1, 1);
}

/* the entity the target of the camera looks at: the target of a ship, the lock or the listened one of a character
 * (an entity with life); NULL else */
static cEntity *Monitor_FindLooked(cEntity *target, byte isCar, byte isWalk) {
	const char *name;

	if (isCar) {
		name = Veh(target)->target;
	} else if (isWalk) {
		cWalkCharEntity *w = (cWalkCharEntity *)target;
		if (g_WalkCameraCenter != 0.0f && w->control.lock) {
			cEntity *t = w->control.target.target;
			if (!t) {
				return NULL;
			}
			name = t->name;
		} else {
			name = Nto1Node_TargetName(&w->listen);
		}
	} else {
		return NULL;
	}
	if (!name || !*name) {
		return NULL;
	}
	cEntity *e = World_FindEntity(name);
	return e && Entity_IsType(e, TYEN_WITH_LIFE) ? e : NULL;
}

/* the head and text of Scorer_SetHeadMonitor */
static char g_HeadMonitorName[128];

/* the head of e in the monitor when no talk shows one, its descriptor as the text */
static void Monitor_SetHead(cMonitorScorer *self, cEntity *e) {
	cWithLifeEntity *w = WLife(e);

	if (g_HeadMonitorName[0]) {
		return;
	}
	const char *head = w->headModel;
	if (!head || !*head) {
		return;
	}
	if (!self->model || strcmp(self->model->name, head)) {
		HeadScorer_SetModel(self, w->headModel);
		self->noise = 0.0f;
		self->textTime = 1.0f;
		self->anim = 0;
		Str_Copy(self->text, w->usrName ? w->usrName : str_Empty, sizeof(self->text));
	}
	if (self->model && self->anim != w->headAnim) {
		HeadScorer_SetAnim(self, w->headAnim);
	}
}

/* the talking head through the camera of the monitor, swaying */
static void Monitor_DrawHead(cMonitorScorer *self, float frameTime) {
	vec3_t t;

	CGameCamera *cur = Camera_GetCurrent();
	float tt = frameTime * 3.0f + self->textTime;
	self->textTime = tt;
	if (tt > 1.0f) {
		self->textTime = 1.0f;
	}
	if (!cur) {
		return;
	}
	CGameCamera_Copy(self->cam, cur);
	CGameCamera_SetLookAt(self->cam, 1);
	float sx = (float)sin(g_pApp->vftable->GetGameTime(g_pApp)) * MONITOR_HEAD_SWAY;
	double t2 = ((g_pApp->vftable->GetGameTime(g_pApp)) * 0.5);
	float cy = (float)cos(t2) * MONITOR_HEAD_SWAY;
	cScorerModel *m = self->model;
	float hy = m->height + MONITOR_HEAD_SWAY + cy;
	float hz = m->radius * MONITOR_HEAD_BACK;
	const vec3_t *p = CGameModel_GetPos(&m->model, &t);
	CGameCamera *cam = self->cam;
	cam->pos.z = p->z + hz;
	cam->pos.x = p->x + sx;
	cam->pos.y = p->y + hy;
	float h = self->model->height;
	p = CGameModel_GetPos(&self->model->model, &t);
	cam->target.x = p->x + 0.0f;
	cam->target.z = p->z + 0.0f;
	cam->target.y = p->y + h;
	self->cam->fov = MONITOR_HEAD_FOV;
	Camera_SetCurrent(self->cam);
	CGameModel_RootMotion(&self->model->model, frameTime, NULL, NULL);
	CGameModel_Enable(&self->model->model);
	CGameModel_RenderNow(&self->model->model, 2);
	cEmbeddedFX *extra = self->model->extra;
	if (extra) {
		extra->vftable->Tick(extra);
	}
	CGameModel_Disable(&self->model->model);
	Camera_SetCurrent(cur);
}

/* the view of the looked entity e: its camera at a distance by its size, the mark of a mission ship, the life and
 * the weapon of a ship; the text changes with the entity. Returns the color of the text */
static uint32_t Monitor_DrawLooked(cMonitorScorer *self, cEntity *target, cEntity *e, byte isCar, byte isWalk,
	float time, float frameTime, uint32_t textColor) {
	vec3_t t, d;

	int team = WLife(e)->arrow.tgType;
	uint32_t lifeColor = team == 2 ? 0x578057 : 0x778057;
	textColor = team == 2 ? 0xaeffae : 0xeeffae;
	float noise = self->noise;
	if (2.0f > noise) {
		float tt = frameTime * 3.0f + self->textTime;
		self->textTime = tt;
		if (tt > 1.0f) {
			self->textTime = 1.0f;
		}
	} else {
		float tt = self->textTime;
		if (tt > 0.5f) {
			self->textTime = 0.5f;
		} else {
			self->textTime = tt - frameTime;
		}
		if (0.0f > self->textTime) {
			self->textTime = 0.0f;
		}
	}
	if (!self->targetView) {
		self->noise = noise + frameTime;
	} else {
		self->noise = 0.0f;
	}
	if (!(self->textTime > 0.5f)) {
		return textColor;
	}
	CGameCamera *cur = Camera_GetCurrent();
	if (!cur) {
		return textColor;
	}
	byte eIsCar = Entity_IsType(e, TYEN_CAR);
	int show = Obj(e)->radarShow;
	if (isCar && eIsCar && show) {
		/* a mission ship blinks yellow (main) or cyan */
		int k = (int)(time * 1024.0f) % 512;
		int a = 0xff - k;
		a = a < 0 ? -a : a;
		CPsaImage *img = self->img350;
		img->color = (uint32_t)(a / 2) << 24 | (Obj(e)->radarShow == 1 ? 0xffff80 : 0x80ffff);
		img->scaleX = MONITOR_W;
		img->scaleY = MONITOR_H;
		CPsaImage_Draw(img, self->base.x, self->base.y);
	}
	CGameCamera_Copy(self->cam, cur);
	CGameCamera_SetLookAt(self->cam, 1);
	self->cam->aspectScreen = MONITOR_VIEW_ASPECT;
	const cObjEntityVtbl *vt = Obj_Vtbl(e);
	const vec3_t *p = vt->GetViewPos(e, &t);
	float dx = p->x - cur->pos.x, dy = p->y - cur->pos.y, dz = p->z - cur->pos.z;
	float dist;
	if (!isWalk) {
		float l = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		float f = (float)exp((double)(l * -1.5f / MONITOR_VIEW_FAR));
		dist = (1.0f - f) * MONITOR_VIEW_FAR;
	} else if (Entity_IsType(e, TYEN_WITH_LIFE)) {
		CGameModel *gm = vt->GetModel(e);
		if (eIsCar && gm && CGameModel_IsLoaded(gm)) {
			dist = (float)((CGameModel_Radius(gm)) * 7.5f);
		} else {
			dist = WLife(e)->prim.radius * 10.0f;
		}
	} else {
		dist = 600.0f;
	}
	p = vt->GetViewPos(e, &t);
	d.x = p->x - cur->pos.x;
	d.y = p->y - cur->pos.y;
	d.z = p->z - cur->pos.z;
	vec3_t n;
	const vec3_t *u = Vec3_NormalizeTo(&n, &d);
	float ox = u->x * dist, oy = u->y * dist, oz = u->z * dist;
	p = vt->GetViewPos(e, &t);
	CGameCamera *cam = self->cam;
	cam->pos.x = p->x - ox;
	cam->pos.y = p->y - oy;
	cam->pos.z = p->z - oz;
	self->cam->fov = MONITOR_VIEW_FOV;
	p = vt->GetViewPos(e, &t);
	self->cam->target = *p;
	Camera_SetCurrent(self->cam);
	vt->Render(e);
	Camera_SetCurrent(cur);
	if (e != self->lastTarget) {
		self->lastTarget = e;
		Str_Copy(self->text, WLife(e)->usrName ? WLife(e)->usrName : str_Empty, sizeof(self->text));
		self->textTime = 0.5f;
		self->textShown = 0.0f;
		self->noise = 2.0f;
	}
	if (!isCar) {
		return textColor;
	}
	uint32_t c = cVehicleO648_IsStale(&Veh(target)->o648) ? textColor : lifeColor;
	CFont_PrintfColorShadow(self->bigFont, self->base.x + 8.0f, self->base.y + 6.0f, c, "%d", WLife(e)->life);
	if (!eIsCar) {
		return textColor;
	}
	cWeaponManager *wm = &Veh(e)->weapons;
	int w = wm->curWeap;
	int up = cWeaponManager_GetUpgrade(wm, w);
	int frame;
	if (!(up > 0 && (frame = self->upgradeIcons[up]) > 0) && (frame = self->weaponIcons[w]) <= 0) {
		return textColor;
	}
	CPsaImage_DrawFrame(self->weapon, frame, self->weaponOffX + self->base.x, self->weaponOffY + self->base.y);
	return textColor;
}

/* the text of the monitor typed in letter by letter (the \001 color codes skipped whole), the noise and NO SIGNAL
 * over it, the frame, the static band */
static void Monitor_DrawText(cMonitorScorer *self, cEntity *e, float frameTime, uint32_t textColor) {
	float pos[2], dim[2], uv0[2], uv1[2];
	float x = self->base.x, y = self->base.y;

	float shown = frameTime + self->textShown;
	self->textShown = shown;
	int n = (int)(shown * MONITOR_TEXT_SPEED);
	int len = (int)strlen(self->text);
	if (len < n) {
		n = len;
	}
	if (n > 0 && Fonts_Unifont()) {
		int i = 0, end = n, j;
		do {
			if (self->text[i] == 1) {
				i += 4;
				end += 4;
			}
			j = i;
			i++;
		} while (j < end && i < len);
		n = i > len ? len : i;
	}
	if (e && Entity_IsType(e, TYEN_WALKCHAR)) {
		textColor = Obj(e)->radarShow == 1 ? 0xeeffae : 0xaeffae;
	}
	CFont *f = self->smallFont;
	f->shadow[0] = 1.0f;
	f->shadow[1] = 1.0f;
	CFont_PrintfColorShadow(f, self->base.x + 8.0f, self->base.y + MONITOR_H - 20.0f, textColor, "%.*s", n, self->text);
	f = self->smallFont;
	CFont_PrintfColorShadow(f, f->x, f->y, 0xffffff, "%c", self->text[n]);
	f = self->smallFont;
	CFont_PrintfColorShadow(f, f->x, f->y, 0xffffff, "%c", self->text[n] ? 0x80 : 0);
	CFont_ResetEffects(self->smallFont);
	if (self->noise > 0.1f) {
		CPsaImage *img = self->noiseImg;
		img->f18 = 0.8f;
		img->f1c = 0.8f;
		float r = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE + 0.0);
		img->uv0[0] = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE + 0.0);
		img->uv0[1] = r;
		img = self->noiseImg;
		float du = MONITOR_W / img->f18, dv = MONITOR_H / img->f1c;
		float v0 = img->uv0[1], u0 = img->uv0[0];
		img->uv1[0] = u0 + du * 0.0078125f;
		img->uv1[1] = v0 + dv * 0.0078125f;
		int a = (int)(self->noise * 128.0f);
		if (a > 0xc0) {
			a = 0xc0;
		}
		self->noiseImg->color = (uint32_t)a << 24 | 0xffffff;
		CPsaImage_Draw(self->noiseImg, self->base.x, self->base.y);
		if (self->noise > 1.0f) {
			self->smallFont->align = 4;
			Font_TextSizeAlign(self->smallFont, pos, dim, self->base.x + MONITOR_SIGNAL_X,
				self->base.y + MONITOR_SIGNAL_Y, 1, "NO SIGNAL");
			pos[0] = pos[0] - 2.0f;
			pos[1] = pos[1] - 2.0f;
			dim[0] = dim[0] + 4.0f;
			dim[1] = dim[1] + 4.0f;
			self->img350->color = 0xc0000000;
			self->img350->scaleX = dim[0];
			self->img350->scaleY = dim[1];
			CPsaImage_Draw(self->img350, pos[0], pos[1]);
			double b = ((g_pApp->vftable->GetGameTime(g_pApp)) * 8.0);
			if (!((int)b & 1)) {
				CFont_PrintfAlignColor(self->smallFont, self->base.x + MONITOR_SIGNAL_X,
					self->base.y + MONITOR_SIGNAL_Y, 4, 0xff0000, "NO SIGNAL");
			}
		}
	}
	int r = crt_rand() & 0x7fff;
	self->frame->color = (uint32_t)(int)(float)((double)r * RAND_SCALE * 128.0 + 128.0) << 24 | 0xffffff;
	CPsaImage_DrawFrame(self->frame, self->frameIndex, x, y);
	CPsaImage_GetFrameUV(self->staticImg, self->staticIndex, uv0, uv1);
	r = crt_rand() & 0x7fff;
	CPsaImage *s = self->staticImg;
	s->color = (uint32_t)(int)(float)((double)r * RAND_SCALE * 192.0 + 0.0) << 24 | 0xffffff;
	s->scaleX = 123.5f / (uv1[0] - uv0[0]);
	s->scaleY = MONITOR_STATIC_H / (uv1[1] - uv0[1]);
	double g = ((g_pApp->vftable->GetGameTime(g_pApp)) * 20.0);
	int k = (int)g % 120;
	CPsaImage_DrawFrame(self->staticImg, self->staticIndex, self->base.x,
		self->base.y + ((float)k - MONITOR_STATIC_H));
}

/* the view opening from a line in the middle while the text time grows to 1 */
static void Monitor_DrawOpening(cMonitorScorer *self, float tt) {
	float a, b;

	self->textShown = 0.0f;
	if (tt > 0.5f) {
		a = 1.0f;
		b = 1.0f;
	} else if (tt > 0.25f) {
		b = (tt - 0.25f) * 4.0f;
		a = 1.0f;
	} else {
		a = tt * 4.0f;
		b = 0.0f;
	}
	float g = 1.0f;
	if (!(0.5f > tt)) {
		float h = tt - 0.5f;
		g = 1.0f - (h + h);
	}
	uint32_t c = (uint32_t)(int)(g * 255.0f);
	CPsaImage *img = self->img350;
	img->color = ((c << 8 | c) << 8 | c) << 8 | c;
	img->scaleY = b * MONITOR_H + 1.0f;
	img->scaleX = a * MONITOR_W;
	CPsaImage_Draw(self->img350, (1.0f - a) * MONITOR_W * 0.5f + self->base.x,
		(1.0f - b) * MONITOR_H * 0.5f + self->base.y);
	self->frame->color = 0x80ffffff;
	CPsaImage_DrawFrame(self->frame, self->frameIndex, self->base.x, self->base.y);
}

/* the objective mark of a character: the four corners around the entity that can be photographed (or the place
 * of NO SIGNAL), moving there smoothly when it changes */
static void Monitor_RepeatPhotoMessage(void);
static void Monitor_DrawObjective(cMonitorScorer *self, cEntity *e, float time, float frameTime) {
	vec3_t t;
	const char *name;
	byte on = 0;

	CPsaImage *o = self->objective;
	if (e && Entity_IsType(e, TYEN_WITH_LIFE) && WLife(e)->arrow.canPhoto) {
		on = 1;
		o->color = ((int)(time * 10.0f) & 1) ? 0x80ff : 0xffff;
		name = e->name;
	} else {
		o->color = 0x808080;
		name = str_Empty;
	}
	if (strcmp(self->objName, name)) {
		self->objWas = self->objName[0] != 0;
		Str_Copy(self->objName, name, sizeof(self->objName));
		self->objFrom[0] = self->objPos[0];
		self->objFrom[1] = self->objPos[1];
		self->objBlend = 0.0f;
	}
	if (on) {
		Monitor_RepeatPhotoMessage();
		Engine_WorldToScreen(Obj_Vtbl(e)->GetViewPos(e, &t), self->objPos, NULL);
	} else {
		self->objPos[0] = self->base.x + MONITOR_SIGNAL_X;
		self->objPos[1] = self->base.y + MONITOR_SIGNAL_Y;
	}
	float bl = frameTime * 4.0f + self->objBlend;
	self->objBlend = bl;
	if (bl > 1.0f) {
		self->objBlend = 1.0f;
	}
	float s = self->objBlend;
	s = (3.0f - (s + s)) * (s * s);
	float u = 1.0f - s;
	float px = self->objPos[0] * s + u * self->objFrom[0];
	float py = self->objPos[1] * s + self->objFrom[1] * u;
	float k1 = on ? 1.5f : 0.5f;
	float k0 = self->objWas ? 1.5f : 0.5f;
	float sc = k1 * s + u * k0;
	o = self->objective;
	o->scaleX = sc;
	o->scaleY = sc;
	float h = sc * 11.0f;
	o->pivotX = 0.5f;
	o->pivotY = 0.5f;
	uint32_t c = o->color;
	/* the shadowed corners: the colors at half, no alpha */
	uint32_t dark = (((c >> 16 & 0xfe) << 8 | (c >> 8 & 0xff)) & ~1u) << 7 | (c & 0xff) >> 1;
	o->color = dark;
	CPsaImage_DrawFrameFlip(o, self->objectiveIndex, px - h, py - h, 0, 0);
	self->objective->color = c;
	CPsaImage_DrawFrameFlip(self->objective, self->objectiveIndex, px + h, py - h, 1, 0);
	self->objective->color = dark;
	CPsaImage_DrawFrameFlip(self->objective, self->objectiveIndex, px + h, py + h, 2, 0);
	self->objective->color = c;
	CPsaImage_DrawFrameFlip(self->objective, self->objectiveIndex, px - h, py + h, 3, 0);
	self->objective->scaleX = 1.0f;
	self->objective->scaleY = 1.0f;
}

/* the monitor of the target of the camera of player: the head of the talk or the view of the entity it looks at
 * with its text, the noise, the objective mark of a character */
static void cMonitorScorer_DrawMonitor(cMonitorScorer *self, int player, cEntity *target) {
	float frameTime = Scorer_Get(player)->frameTime;
	uint32_t textColor = 0xaeffae;
	float time = Scorer_Get(player)->time;
	byte isCar = Entity_IsType(target, TYEN_CAR);
	byte isWalk = Entity_IsType(target, TYEN_WALKCHAR);

	CPsaImage_DrawFrame(self->panel, self->panelIndex, self->base.x - MONITOR_BORDER, self->base.y - MONITOR_BORDER);
	Render_ClearRect(self->base.x, self->base.y, MONITOR_W, MONITOR_H, 0, 0);
	Eng_SetViewRect(self->base.x, self->base.y, MONITOR_W - 0.5f, MONITOR_H - 0.5f);
	Monitor_DrawMask(self);
	cEntity *e = Monitor_FindLooked(target, isCar, isWalk);
	if (e) {
		Monitor_SetHead(self, e);
	}
	if (self->model) {
		Monitor_DrawHead(self, frameTime);
	} else if (e) {
		textColor = Monitor_DrawLooked(self, target, e, isCar, isWalk, time, frameTime, textColor);
	} else {
		float tt = self->textTime;
		if (0.0f >= tt) {
			self->textTime = 0.0f;
		} else if (tt > 0.5f) {
			self->textTime = 0.5f;
		} else {
			self->textTime = tt - (frameTime + frameTime);
		}
	}
	if (isCar && ((const cWithLifeEntityVtbl *)target->vftable)->IsEMIed(target)) {
		/* the EMI: noise and no text */
		self->noise = 0.9f;
		self->textTime = 1.0f;
		self->text[0] = 0;
	}
	if (isWalk && self->canPossessIndex > 0 && e && Entity_IsType(e, TYEN_WALKCHAR)) {
		byte can = cWalkCharEntity_CanPossess(target, 1);
		int k = (int)(time * 1024.0f) % 512;
		float f = (float)k * 0.001953125f;
		float a;
		if (0.5f > f) {
			a = f + f;
		} else {
			float h = f - 0.5f;
			a = 1.0f - (h + h);
		}
		uint32_t c = can ? (uint32_t)(0x5c - (int)(a * -64.0f)) << 24 | 0xfbf1ae
			: (uint32_t)(int)(a * 128.0f) << 24 | 0xff2020;
		self->objective->color = c;
		CPsaImage_DrawFrameFlip(self->objective, self->canPossessIndex, self->base.x + MONITOR_W - 22.0f,
			self->base.y + 12.0f, 0, 0);
	}
	float tt = self->textTime;
	if (tt >= 1.0f) {
		Monitor_DrawText(self, e, frameTime, textColor);
	} else {
		Monitor_DrawOpening(self, tt);
	}
	Eng_ResetViewRect();
	if (isWalk) {
		Monitor_DrawObjective(self, e, time, frameTime);
	}
}

static char g_HeadMonitorText[128];
static int g_HeadMonitorAnim = 1;

static void cMonitorScorer_Draw(cScorerItem *base, int player) {
	cMonitorScorer *self = (cMonitorScorer *)base;
	if (!(byte)Talk_IsShown() || !g_HeadMonitorName[0]) {
		self->model = NULL;
		if (g_HeadMonitorName[0]) {
			g_HeadMonitorName[0] = 0;
			self->text[0] = 0;
		}
	} else {
		if (!self->model || strcmp(self->model->name, g_HeadMonitorName)) {
			HeadScorer_SetModel(self, g_HeadMonitorName);
			byte noisy = g_HeadMonitorAnim < 0;
			self->textTime = 0.0f;
			self->textShown = 0.0f;
			self->noise = noisy ? 1.0f : 0.0f;
			Str_Copy(self->text, g_HeadMonitorText, sizeof(self->text));
		}
		if (self->model) {
			int a = Scorer_Abs(g_HeadMonitorAnim);
			if (self->anim != a) {
				HeadScorer_SetAnim(self, a);
			}
		}
	}
	const char *camName = cWorld_GetCamName(g_pWorld, player);
	if (!*camName) {
		return;
	}
	cEntity *cam = World_FindEntity(camName);
	if (!cam || !cam->vftable->IsType(cam, "FCam")) {
		return;
	}
	cEntity *target = cCamEntity_GetTarget(cam);
	if (target && target->vftable->IsType(target, TYEN_WITH_LIFE)) {
		cMonitorScorer_DrawMonitor(self, player, target);
	}
}

static byte cMonitorScorer_SetModel(cScorerItem *base, const char *name) {
	cMonitorScorer *self = (cMonitorScorer *)base;
	cScorerModel *m = cScorer_FindModel(Scorer_Get(self->base.player), name);

	self->model = m;
	if (!m) {
		self->lastTarget = NULL;
		self->textTime = 0.0f;
		self->textShown = 0.0f;
		return 0;
	}
	CHECK(m->numAnims > 0);
	if (self->model->numAnims <= self->anim) {
		Con_Debugf("Minority Report : Head with invalid animation (%d), reseted\n", self->anim);
		self->anim = 0;
	}
	CGameModel_SetAnimSpeed(&self->model->model, self->model->anims[self->anim], 0.0f);
	CGameModel_SetAnim(&self->model->model, self->model->anims[self->anim], 0, 0);
	CGameModel_SetAnimLoop(&self->model->model, self->model->anims[self->anim], 1);
	return self->model != NULL;
}

static void cMonitorScorer_SetAnim(cScorerItem *base, int anim) {
	cMonitorScorer *self = (cMonitorScorer *)base;
	cScorerModel *m = self->model;

	if (!m) {
		return;
	}
	CHECK(anim >= 0 && anim < m->numAnims);
	byte changed = self->anim != anim && self->model;
	self->anim = anim;
	if (changed) {
		int aidx = self->model->anims[anim];
		CHECK(aidx >= 0);
		CGameModel_SetAnimSpeed(&self->model->model, aidx, 0.0f);
		CGameModel_SetAnim(&self->model->model, aidx, 0, 0);
		CGameModel_SetAnimLoop(&self->model->model, aidx, 1);
	}
}

void Scorer_SetHeadMonitor(const char *a, const char *b, int c) {
	Str_Copy(g_HeadMonitorName, a, 0x80);
	Str_Copy(g_HeadMonitorText, b, 0x80);
	g_HeadMonitorAnim = c;
}

/* the message FirstPhotoMessage again when the message text ended */
static void Monitor_RepeatPhotoMessage(void) {
	const char *msg = g_FirstPhotoMessage;

	if (!msg || !*msg) {
		return;
	}
	float t = Timer_GetTime(&g_pWorld->timer);
	if (t > g_MsgTime) {
		Scorer_SetMsgText(g_FirstPhotoMessage, t + 0.1f, "Horatio");
	}
}

/* the vtable of cMonitorScorer */
static const cHeadScorerVtbl cMonitorScorer_vftable = {
	.base.Draw = cMonitorScorer_Draw,
	.base.SetParam = cMonitorScorer_SetParam,
	.base.Var = cMonitorScorer_Var,
	.base.Delete = cMonitorScorer_Delete,
	.SetAnim = cMonitorScorer_SetAnim,
	.SetModel = cMonitorScorer_SetModel,
};

static cGVar GVar_FirstPhotoMessage;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FirstPhotoMessage, &GVar_FirstPhotoMessage)
INIT_GVAR_STRING(Init_GVar_FirstPhotoMessage, Fini_GVar_FirstPhotoMessage, &GVar_FirstPhotoMessage, "FirstPhotoMessage", 0, "Message that appears every time you must to use the camere", "")
