/*
 * MonitorScorer.cpp of the game (Menu/): cMonitorScorer, the ship monitor with the talking head, and the head
 * of the scripts (Scorer_SetHeadMonitor). The monitor itself (cMonitorScorer_DrawMonitor) is still the exe's. See
 * include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmodel.h"
#include "scrap/world.h"

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

cMonitorScorer *__fastcall cMonitorScorer_ctor(cMonitorScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CMONITORSCORER_VFTABLE);
	self->panel = Monitor_WhiteImage(0x10000);
	self->mask = Monitor_WhiteImage(0x10000);
	self->frame = Monitor_WhiteImage(0x10000);
	self->staticImg = Monitor_WhiteImage(0x10000);
	self->objetive = Monitor_WhiteImage(0x10000);
	self->weapon = Monitor_WhiteImage(0x10000);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->noiseImg = img ? CPsaImage_ctorFlags(img, "2D/white.tga", 0, 3, 0x2030) : NULL;
	self->img350 = Monitor_WhiteImage(0x2024);
	self->textTime = 0.0f;
	self->noise = 4.0f;
	CGameCamera *cam = operator_new(sizeof(CGameCamera));
	self->cam = cam ? CGameCamera_ctor(cam) : NULL;
	self->fb8 = 0.0f;
	self->text[0] = 0;
	self->model = NULL;
	self->textPos = 0;
	self->smallFont = g_FontTerminal8;
	self->bigFont = g_FontHoratio;
	self->targetView = 0;
	self->anim = 1;
	self->i220 = 0;
	self->i21c = 0;
	self->i214 = 0;
	self->i218 = self->i220;
	self->b224 = 0;
	self->f2a4 = 1.0f;
	self->b2a8 = 0;
	self->canPossessIndex = 0;
	self->canPossessGlowIndex = 0;
	return self;
}

void __fastcall cMonitorScorer_dtor(cMonitorScorer *self) {
	CPsaImage *img = self->panel;
	self->base.vftable = Game_Ptr(CMONITORSCORER_VFTABLE);
	Monitor_FreeImage(img);
	Monitor_FreeImage(self->mask);
	Monitor_FreeImage(self->frame);
	Monitor_FreeImage(self->staticImg);
	Monitor_FreeImage(self->objetive);
	Monitor_FreeImage(self->weapon);
	if (self->cam) {
		CGameCamera_dtor(self->cam);
		operator_delete_sized(self->cam, sizeof(CGameCamera));
	}
	Monitor_FreeImage(self->noiseImg);
	Monitor_FreeImage(self->img350);
	cScorerItem_dtor(&self->base);
}

cMonitorScorer *__thiscall cMonitorScorer_Delete(cMonitorScorer *self, byte flags) {
	cMonitorScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cMonitorScorer));
	}
	return self;
}

void __thiscall cMonitorScorer_SetParam(cMonitorScorer *self, const char *name, const char *value) {
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

int __thiscall cMonitorScorer_Var(cMonitorScorer *self, const char *name, byte set, char **s, double *d, int *i) {
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
		return Monitor_VarSprite(&self->objetive, *s, 3, 0x10048, 0xffffff);
	}
	if (i) {
		if (!crt_stricmp(name, "ObjetiveIndex")) {
			return ScorerVar_Int(&self->objetiveIndex, set, i);
		}
		if (!crt_stricmp(name, "ObjetiveChargeIndex")) {
			return ScorerVar_Int(&self->objetiveChargeIndex, set, i);
		}
		if (!crt_stricmp(name, "ObjetiveBlackIndex")) {
			return ScorerVar_Int(&self->objetiveBlackIndex, set, i);
		}
		if (!crt_stricmp(name, "ObjetiveWhiteIndex")) {
			return ScorerVar_Int(&self->objetiveWhiteIndex, set, i);
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
			self->textPos = 0;
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

void __thiscall cMonitorScorer_Draw(cMonitorScorer *self, int player) {
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
			self->textPos = 0;
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

byte __thiscall cMonitorScorer_SetModel(cMonitorScorer *self, const char *name) {
	cScorerModel *m = cScorer_FindModel(Scorer_Get(self->base.player), name);

	self->model = m;
	if (!m) {
		self->fb8 = 0.0f;
		self->textTime = 0.0f;
		self->textPos = 0;
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

void __thiscall cMonitorScorer_SetAnim(cMonitorScorer *self, int anim) {
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

void Monitor_RepeatPhotoMessage(void) {
	const char *msg = g_FirstPhotoMessage;

	if (!msg || !*msg) {
		return;
	}
	float t = Timer_GetTime(&g_pWorld->timer);
	if (t > g_MsgTime) {
		Scorer_SetMsgText(g_FirstPhotoMessage, t + 0.1f, "Horatio");
	}
}
