/*
 * The news panel of the scorer (the code between NetStatsScorer.cpp and RadarScorer.cpp, the source file name is
 * not known): the texture "NewsPanel" with the news head, or the entities the police watch while the alarm is on.
 * See include/scrap/scoreritems.h.
 */
#include "scrap/fonts.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/weapons.h"

#define NEWS_FOV 0.87266463f				/* 50 degrees */
extern const double g_HeadRandRange;
#define g_NewsRandRange g_HeadRandRange	/* 2 pi as a float */
extern const double g_NewsRandOfs;	/* pi as a float */
/* the model of an entity with life (slot 0x98) */
/* its render (slot 0xb8) */

cNewsPanel *cNewsPanel_ctor(cNewsPanel *self) {
	cString_ctor(&self->head);
	cString_ctor(&self->logo);
	cString_ctor(&self->back);
	CGameCamera *cam = operator_new(sizeof(CGameCamera));
	self->cam = cam ? CGameCamera_ctor(cam) : NULL;
	self->logoImg = NULL;
	self->backImg = NULL;
	self->lastTime = 0.0f;
	self->open = 0.0f;
	self->closedDrawn = 0;
	self->engineInits = g_EngineInits;
	return self;
}

static void News_FreeImage(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
}

void cNewsPanel_dtor(cNewsPanel *self) {
	if (self->cam) {
		CGameCamera_dtor(self->cam);
		operator_delete_sized(self->cam, sizeof(CGameCamera));
	}
	News_FreeImage(self->logoImg);
	News_FreeImage(self->backImg);
	cString_dtor(&self->back);
	cString_dtor(&self->logo);
	cString_dtor(&self->head);
}

cEntity *cNewsPanel_GetWatched(cNewsPanel *self, int list) {
	(void)self;
	switch (list) {
	case 0:
		return cEntityList_GetFirst(g_pWorld->logic.aiCtrl.sentinels);
	case 1:
		return cEntityList_GetFirst(g_pWorld->logic.aiCtrl.gears);
	default:
		return cEntityList_GetFirst(g_pWorld->logic.genChars.police);
	}
}

void cNewsPanel_Reset(cNewsPanel *self) {
	g_NewsPanelActive = g_NewsPanelActiveDef;
	g_NewsPanelShowHead = g_NewsPanelShowHeadDef;
	self->camPos.x = 0.0f;
	self->camPos.y = 0.0f;
	self->camPos.z = 0.0f;
	self->model = NULL;
	if (self->logoImg) {
		News_FreeImage(self->logoImg);
		self->logoImg = NULL;
	}
	if (self->backImg) {
		News_FreeImage(self->backImg);
		self->backImg = NULL;
	}
	self->lastTime = 0.0f;
	self->open = 0.0f;
	self->closedDrawn = 0;
	cString_Set(&self->head, "");
	cString_Set(&self->logo, "");
	cString_Set(&self->back, "");
}

void cNewsPanel_Update(cNewsPanel *self) {
	float t = Scorer_Get(0)->time;
	float dt = t - self->lastTime;

	if (dt == 0.0f) {
		return;
	}
	if (g_NewsPanelActive) {
		if (self->engineInits != g_EngineInits) {
			self->closedDrawn = 0;
		}
		self->engineInits = g_EngineInits;
		int v = Scorer_Abs(0xff - (int)(Float_ToU32(t * 1024.0f) % 0x1ff));
		if (Eng_RenderTargetBegin("NewsPanel")) {
			if (!g_pWorld->alarm.on) {
				cNewsPanel_DrawNews(self, dt);
			} else {
				self->closedDrawn = 0;
				/* the alarm is on for the character type of now: the own ship instead of the watched entities */
				if (g_pWorld->alarm.sameChar) {
					Render_ClearRect(0.0f, 0.0f, 640.0f, 480.0f, 1, (uint32_t)v << 16);
					cNewsPanel_DrawEntity(self, cWorld_GetUsrEntity(g_pWorld, 0));
				} else {
					Render_ClearRect(0.0f, 0.0f, 640.0f, 480.0f, 1, ((uint32_t)v | 0xff00) << 8);
					cEntity *e = cNewsPanel_GetWatched(self, (int)(t * 0.5f) % 3);
					if (!e) {
						cEntityList_GetFirst(g_pWorld->logic.aiCtrl.sentinels);
					}
					cNewsPanel_DrawEntity(self, e);
				}
			}
		}
		Eng_RenderTargetEnd();
	}
	self->lastTime = t;
}

void cNewsPanel_DrawEntity(cNewsPanel *self, cEntity *e) {
	vec3_t tmp;

	if (!e || !e->vftable->IsType(e, TYEN_WITH_LIFE) || !Obj_Vtbl(e)->GetModel(e)) {
		return;
	}
	g_NewsPanelAspect = 1.33333004f;
	CGameCamera *cur = Camera_GetCurrent();
	CGameCamera_Copy(self->cam, cur);
	CGameCamera_SetLookAt(self->cam, 1);
	const vec3_t *p = CGameModel_GetPos(Obj_Vtbl(e)->GetModel(e), &tmp);
	CGameCamera *cam = self->cam;
	cam->target = *p;
	float dy = self->camPos.y - cam->target.y;
	float dx = self->camPos.x - cam->target.x;
	float dz = self->camPos.z - cam->target.z;
	float r = WLife(e)->prim.radius * 4.0f;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (d > r + r) {
		/* a new point around the entity */
		double a = (double)(float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * g_NewsRandRange - g_NewsRandOfs);
		float cx = (float)cos(a) * r;
		float cy = r * 0.5f;
		float sz = (float)sin(a) * r;
		cam = self->cam;
		self->camPos.x = cam->target.x + cx;
		self->camPos.y = cam->target.y + cy;
		self->camPos.z = cam->target.z + sz;
	}
	cam = self->cam;
	cam->pos = self->camPos;
	cam->fov = NEWS_FOV;
	self->cam->aspectScreen = g_NewsPanelAspect;
	Camera_SetCurrent(self->cam);
	Obj_Vtbl(e)->Render(e);
	Camera_SetCurrent(cur);
}

void cNewsPanel_DrawNews(cNewsPanel *self, float dt) {
	vec3_t tmp;
	float v;

	if (g_NewsPanelShowHead) {
		v = g_NewsPanelOpenSpeed * dt;
	} else {
		v = Float_Keep(-(g_NewsPanelCloseSpeed * dt));
	}
	v = v + self->open;
	self->open = v;
	if (v > 1.0f) {
		self->open = 1.0f;
	} else if (0.0f > v) {
		self->open = 0.0f;
	}
	if (cString_Cmp(&self->head, g_NewsHead)) {
		if (self->model) {
			self->model = NULL;
		}
		cString_Assign(&self->head, g_NewsHead);
	}
	if (cString_Cmp(&self->logo, g_NewsLogoBmp)) {
		if (self->logoImg) {
			News_FreeImage(self->logoImg);
			self->logoImg = NULL;
		}
		cString_Assign(&self->logo, g_NewsLogoBmp);
	}
	if (cString_Cmp(&self->back, g_BackNewsBmp)) {
		if (self->backImg) {
			News_FreeImage(self->backImg);
			self->backImg = NULL;
		}
		cString_Assign(&self->back, g_BackNewsBmp);
	}
	if (!self->model) {
		cScorerModel *m = cScorer_FindModel(Scorer_Get(0), *g_NewsHead);
		if (m) {
			g_NewsPanelAspect = 1.75f;
			self->model = &m->model;
		} else {
			g_NewsPanelAspect = 1.33333337f;
			cEntity *e = World_FindEntity(*g_NewsHead);
			if (e && e->vftable->IsType(e, TYEN_WITH_LIFE) && Obj_Vtbl(e)->GetModel(e)) {
				self->model = Obj_Vtbl(e)->GetModel(e);
			}
		}
	}
	if (!self->backImg) {
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		self->backImg = img ? CPsaImage_ctorTex(img, *g_BackNewsBmp, 0, 0x24) : NULL;
	}
	if (!self->logoImg) {
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		self->logoImg = img ? CPsaImage_ctorTex(img, *g_NewsLogoBmp, 0, 0x24) : NULL;
	}
	float closed = 1.0f - self->open;
	if (!self->model || !self->logoImg) {
		return;
	}
	if (self->open > 0.0f) {
		Render_ClearRect(0.0f, 0.0f, 640.0f, 480.0f, 0, 0);
		CPsaImage *back = self->backImg;
		back->color = 0xffffffff;
		back = self->backImg;
		back->scaleX = 680.0f;
		back->scaleY = 480.0f;
		CPsaImage_Draw(self->backImg, self->open * -40.0f, 0.0f);
		CGameCamera *cur = Camera_GetCurrent();
		CGameCamera_Copy(self->cam, cur);
		CGameCamera_SetLookAt(self->cam, 1);
		float mx = g_NewsPanelMove * self->open;
		float disp = g_NewsPanelDisplacement;
		const vec3_t *p = CGameModel_GetPos(self->model, &tmp);
		float py = p->y;
		CGameCamera *cam = self->cam;
		cam->target.x = mx + p->x;
		cam->target.z = p->z + 0.0f;
		cam->target.y = py + disp;
		cam = self->cam;
		float dist = Float_Keep(-g_NewsPanelDistance);
		cam->pos.z = cam->target.z + dist;
		cam->pos.x = cam->target.x + 0.0f;
		cam->pos.y = cam->target.y + 0.0f;
		self->cam->fov = NEWS_FOV;
		self->cam->aspectScreen = g_NewsPanelAspect;
		Camera_SetCurrent(self->cam);
		CGameModel_RenderNow(self->model, 2);
		Camera_SetCurrent(cur);
		CPsaImage *logo = self->logoImg;
		logo->color = (uint32_t)(0x7f - (int)(closed * -128.0f)) << 24 | 0xffffff;
		float sx = closed * 640.0f + self->open * g_NewsPanelW;
		float sy = closed * 480.0f + self->open * g_NewsPanelH;
		logo = self->logoImg;
		logo->scaleX = sx;
		logo->scaleY = sy;
		CPsaImage_Draw(self->logoImg, self->open * g_NewsPanelX, self->open * g_NewsPanelY);
		self->closedDrawn = 0;
		return;
	}
	if (!self->closedDrawn) {
		CPsaImage *logo = self->logoImg;
		logo->color = 0xffffffff;
		logo = self->logoImg;
		logo->scaleX = 640.0f;
		logo->scaleY = 480.0f;
		CPsaImage_Draw(self->logoImg, 0.0f, 0.0f);
		self->closedDrawn = 1;
	}
}
