/*
 * RadarScorer.cpp of the game (Menu/): cRadarScorer, the radar of the ship and the map of the menu. The radar
 * itself (cRadarScorer_DrawRadar and its helpers) is still the exe's. See include/scrap/scoreritems.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/scoreritems.h"
#include "scrap/scrapworld.h"
#include "scrap/weapons.h"

/* the turn of the camera of the radar (FCam) */
#define CamEntity_AngY(e) (*(float *)((byte *)(e) + 0x8c))
/* the center of an entity (slot 0x74) */
#define Entity_GetCenter(e, out) ENT_VF(e, 0x74, vec3_t * (__thiscall *)(cEntity *, vec3_t *))(e, out)

static CPsaImage *Radar_White(uint32_t flags) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	return img ? CPsaImage_ctor(img, "2D/white.tga", flags) : NULL;
}

static void Radar_FreeImage(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
}

cRadarScorer *__fastcall cRadarScorer_ctor(cRadarScorer *self) {
	WorldNodeInfo info;
	box_t box;
	char buf[0x200];
	float x0, z0, x1, z1;

	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CRADARSCORER_VFTABLE);
	self->noise = Radar_White(0x2034);
	self->cone = Radar_White(0x2024);
	self->radar = Radar_White(0x20);
	self->mask = Radar_White(0x20);
	self->icons = Radar_White(0x20);
	self->map2dScale = 512.0f;
	self->map2dScaleFactor = 0.01f;
	self->menuMap = 0;
	self->menuMinScale = 512.0f;
	self->menuMaxScale = 512.0f;
	self->menuScaleSpeed = 10.0f;
	self->scrollX = 0.0f;
	self->scrollY = 0.0f;
	WorldInfo_Begin("DM_2dMap0", 0);
	byte found = WorldInfo_Next(&info);
	if (found) {
		x0 = info.pos.x;
		z0 = info.pos.z;
		WorldInfo_Begin("DM_2dMap1", 0);
		found = WorldInfo_Next(&info);
		x1 = info.pos.x;
		z1 = info.pos.z;
	}
	if (!found) {
		const box_t *b = World_GetBox(&box);
		z1 = b->max.z;
		x1 = b->max.x;
		z0 = b->min.z;
		x0 = b->min.x;
	}
	self->mapW = x1 - x0;
	self->mapH = z1 - z0;
	self->mapX0 = x0;
	self->mapZ0 = z0;
	for (int k = 0; k < World_NumFloors(); k++) {
		snprintf(buf, sizeof(buf), g_FileNameMap2D, k);
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		self->floorMaps[k] = img ? CPsaImage_ctor(img, buf, 0x2024) : NULL;
	}
	self->floor = 0;
	self->prevFloor = 0;
	self->floorFade = 0.0f;
	self->outAlphaFactor = 100;
	return self;
}

void __fastcall cRadarScorer_dtor(cRadarScorer *self) {
	CPsaImage *img = self->radar;
	self->base.vftable = Game_Ptr(CRADARSCORER_VFTABLE);
	Radar_FreeImage(img);
	Radar_FreeImage(self->mask);
	Radar_FreeImage(self->icons);
	Radar_FreeImage(self->noise);
	Radar_FreeImage(self->cone);
	for (int k = 0; k < World_NumFloors(); k++) {
		Radar_FreeImage(self->floorMaps[k]);
	}
	cScorerItem_dtor(&self->base);
}

cInputAction *__thiscall InputActionTable_At(void *table, uint32_t i) {
	return TABLE_AT(*(TABLE(cInputAction) *)table, i);
}

cActionSet *__thiscall ActionSetTable_At(void *table, uint32_t i) {
	return TABLE_AT(*(TABLE(cActionSet) *)table, i);
}

cRadarScorer *__thiscall cRadarScorer_Delete(cRadarScorer *self, byte flags) {
	cRadarScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cRadarScorer));
	}
	return self;
}

void __thiscall cRadarScorer_UpdateFloor(cRadarScorer *self, float y) {
	int old = self->floor;
	int n = World_NumFloors();
	int k = 1;

	while (k < n && !(World_FloorHeight(k) > y)) {
		k++;
	}
	self->floor = k - 1;
	if (self->floor != old) {
		self->prevFloor = old;
		self->floorFade = 1.0f - self->floorFade;
	}
}

void __thiscall cRadarScorer_DrawCone(cRadarScorer *self, const vec3_t *pos, float ang, float spread, float range,
	int mode) {
	float verts[3][2];
	uint32_t colors[3];
	float y = pos->y;
	int side;

	/* on the floor shown, or on the one before while it fades */
	if (World_FloorHeight(self->floor) > y) {
		side = -1;
	} else if (self->floor + 1 >= World_NumFloors()) {
		side = 0;
	} else {
		side = !(World_FloorHeight(self->floor + 1) >= y);
	}
	if (side) {
		if (self->floorFade >= 1.0f) {
			return;
		}
		int p = self->prevFloor;
		if (World_FloorHeight(p) > y) {
			return;
		}
		if (p + 1 < World_NumFloors() && !(World_FloorHeight(p + 1) >= y)) {
			return;
		}
	}
	vec3_t at = *pos;
	float dz = at.z - g_RadarCenterZ;
	float dx = at.x - g_RadarCenterX;
	float s, c;
	X87_SinCos(g_RadarAngle, &s, &c);
	float rx = dx * c - dz * s;
	float cx = self->halfW + self->base.x;
	float ry = dx * s + dz * c;
	float scale = g_RadarScale;
	float half = spread * 0.5f;
	rx = Float_Keep(-(rx / scale));
	float cy = self->base.y + self->halfH;
	ry = ry / scale;
	cx = cx + rx;
	float r = range / scale;
	cy = cy + ry;
	verts[0][0] = cx;
	verts[0][1] = cy;
	X87_SinCos(half + ang, &s, &c);
	verts[1][0] = s * r + cx;
	verts[1][1] = c * r + cy;
	X87_SinCos(ang - half, &s, &c);
	verts[2][0] = s * r + cx;
	verts[2][1] = c * r + cy;
	uint32_t color;
	switch (mode) {
	case 1:
		color = 0xff00ff00;
		break;
	case 2:
		color = 0xffffff00;
		break;
	case 3:
		color = 0xffff0000;
		break;
	default:
		CHECK(0);
		memcpy(&color, &at.x, sizeof(color));	/* what the register held */
		break;
	}
	float a = (float)(int)(color >> 24);
	a = side == 0 ? a * self->floorFade : a * (1.0f - self->floorFade);
	colors[0] = (uint32_t)(int)a << 24 | (color & 0xffffff);
	colors[1] = colors[0] & 0xffffff;
	colors[2] = colors[0] & 0xffffff;
	CPsaImage_DrawPoly(self->cone, &verts[0][0], NULL, colors, 3, 6);
}

byte __thiscall cRadarScorer_OnPrevFloor(cRadarScorer *self, float y) {
	int p = self->prevFloor;

	if (World_FloorHeight(p) > y) {
		return 0;
	}
	if (p + 1 >= World_NumFloors()) {
		return 1;
	}
	return World_FloorHeight(p + 1) >= y;
}

int __thiscall cRadarScorer_FloorSide(cRadarScorer *self, float y) {
	int f = self->floor;

	if (World_FloorHeight(f) > y) {
		return -1;
	}
	if (f + 1 >= World_NumFloors()) {
		return 0;
	}
	return !(World_FloorHeight(f + 1) >= y);
}

int __thiscall cRadarScorer_Var(cRadarScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	if (d) {
		if (!crt_stricmp(name, "MenuScaleSpeed")) {
			return ScorerVar_Float(&self->menuScaleSpeed, set, d);
		}
		if (!crt_stricmp(name, "MenuMaxScale")) {
			return ScorerVar_Float(&self->menuMaxScale, set, d);
		}
		if (!crt_stricmp(name, "MenuMinScale")) {
			return ScorerVar_Float(&self->menuMinScale, set, d);
		}
	}
	if (i) {
		if (!crt_stricmp(name, "MenuMap")) {
			if (set) {
				self->menuMap = *i != 0;
			} else {
				*i = self->menuMap != 0;
			}
			return 3;
		}
		if (!crt_stricmp(name, "OutAlphaFactor")) {
			return ScorerVar_Int(&self->outAlphaFactor, set, i);
		}
	}
	if (d) {
		if (!crt_stricmp(name, "Map2dScale")) {
			return ScorerVar_Float(&self->map2dScale, set, d);
		}
		if (!crt_stricmp(name, "Map2dScaleFactor")) {
			return ScorerVar_Float(&self->map2dScaleFactor, set, d);
		}
	}
	if (s && !crt_stricmp(name, "RadarSprite") && set) {
		Radar_FreeImage(self->radar);
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		img = img ? CPsaImage_ctor(img, *s, 0x10060) : NULL;
		self->radar = img;
		img->color = 0xffffffff;
		return 1;
	}
	if (i && !crt_stricmp(name, "RadarIndex")) {
		return ScorerVar_Int(&self->radarIndex, set, i);
	}
	if (s && !crt_stricmp(name, "MaskSprite") && set) {
		Radar_FreeImage(self->mask);
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		img = img ? CPsaImage_ctorFlags(img, *s, 0, 2, 0x11040) : NULL;
		self->mask = img;
		img->color = 0xffffffff;
		return 1;
	}
	if (i && !crt_stricmp(name, "MaskIndex")) {
		return ScorerVar_Int(&self->maskIndex, set, i);
	}
	if (s) {
		if (!crt_stricmp(name, "NoiseSprite") && set) {
			Radar_FreeImage(self->noise);
			CPsaImage *img = operator_new(sizeof(CPsaImage));
			img = img ? CPsaImage_ctor(img, *s, 0x2034) : NULL;
			self->noise = img;
			img->color = 0xffffffff;
			return 1;
		}
		if (!crt_stricmp(name, "IconsSprite") && set) {
			Radar_FreeImage(self->icons);
			CPsaImage *img = operator_new(sizeof(CPsaImage));
			img = img ? CPsaImage_ctor(img, *s, 100) : NULL;
			self->icons = img;
			img->color = 0xffffffff;
			return 1;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "IconAc")) {
			return ScorerVar_Int(&self->iconAc, set, i);
		}
		if (!crt_stricmp(name, "IconIndex")) {
			return ScorerVar_Int(&self->iconIndex[self->iconAc], set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* the value of action k of the first action set for the player */
static float Radar_Action(int k, int player) {
	return InputActionTable_At(&ActionSetTable_At(&g_ActionSets, 0)->actions, k)->value[player];
}

/* the scroll of one axis kept in the map: lo..hi the map edges from the center, half the radar size */
static float Radar_ClampScroll(cRadarScorer *self, float a, float b, float half, float scroll) {
	float big = a, small = b;

	if (b > a) {
		big = b;
		small = a;
	}
	float fit = (big - small) / (half + half);
	if (self->map2dScale > fit) {
		self->map2dScale = fit;
	}
	float m = half * self->map2dScale;
	float v = m + small;
	if (v >= scroll) {
		return v;
	}
	return Float_MinSS(big - m, scroll);
}

void __thiscall cRadarScorer_Draw(cRadarScorer *self, int player) {
	const char *camName = cWorld_GetCamName(g_pWorld, player);
	cEntity *cam = World_FindEntity(camName);
	vec3_t c;

	float ft = Scorer_Get(player)->frameTime;
	float fade = ft + ft + self->floorFade;
	self->floorFade = fade;
	if (fade >= 1.0f) {
		self->floorFade = 1.0f;
	}
	if (!cam || !cam->flag_4c || !cam->vftable->IsType(cam, "FCam")) {
		return;
	}
	cEntity *target = cCamEntity_GetTarget(cam);
	if (!target || !target->vftable->IsType(target, TYEN_WITH_LIFE)) {
		return;
	}
	if (!self->menuMap) {
		self->halfW = 55.0f;
		self->halfW2 = 56.25f;
		self->halfH = 55.0f;
		self->halfH2 = 56.25f;
		cRadarScorer_DrawRadar(self, target, cam);
		return;
	}
	Entity_GetCenter(target, &c);
	self->halfW = 200.0f;
	self->halfW2 = 200.0f;
	self->halfH = 100.0f;
	self->halfH2 = 100.0f;
	float zoom = Radar_Action(6, player);
	float v = (zoom - Radar_Action(7, player)) * self->menuScaleSpeed * ft + self->map2dScale;
	self->map2dScale = v;
	self->map2dScale = self->menuMinScale >= v ? self->menuMinScale : Float_MinSS(self->menuMaxScale, v);
	if (Radar_Action(10, player) > 0.5f) {
		float right = Radar_Action(5, player);
		float dx = (right - Radar_Action(4, player)) * ft;
		float down = Radar_Action(2, player);
		float dy = (down - Radar_Action(3, player)) * ft;
		self->scrollX = self->map2dScale * dx * 640.0f + self->scrollX;
		self->scrollY = self->map2dScale * dy * 480.0f + self->scrollY;
		if (self->menuMap) {
			g_RadarAngle = 0.0f;
		} else {
			g_RadarAngle = CamEntity_AngY(cam) + 3.1415927f;
		}
	}
	self->scrollX = Radar_ClampScroll(self, self->mapX0 - c.x, (self->mapW + self->mapX0) - c.x, self->halfW,
		self->scrollX);
	self->scrollY = Radar_ClampScroll(self, self->mapZ0 - c.z, (self->mapH + self->mapZ0) - c.z, self->halfH,
		self->scrollY);
	if (self->menuMinScale > self->map2dScale) {
		self->menuMinScale = self->map2dScale;
	}
	cRadarScorer_DrawRadar(self, target, cam);
}
