/*
 * RadarScorer.cpp of the game (Menu/): cRadarScorer, the radar of the ship and the map of the menu.
 * See include/scrap/scoreritems.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapworld.h"
#include "scrap/weapons.h"
#include "scrap/crt.h"
#include "scrap/mathlib.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define CRADARSCORER_VFTABLE (&cRadarScorer_vftable)
static const cScorerItemVtbl cRadarScorer_vftable;

/* the turn of the camera of the radar (FCam) */
#define CamEntity_AngY(e) (((cCamControl *)(e))->cam.rot.y)
/* the center of an entity (slot 0x74) */

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

cRadarScorer *cRadarScorer_ctor(cRadarScorer *self) {
	WorldNodeInfo info;
	box_t box;
	char buf[0x200];
	float x0, z0, x1, z1;

	cScorerItem_ctor(&self->base);
	self->base.vftable = CRADARSCORER_VFTABLE;
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
	for (int k = 0; k < g_pWorld->floor.count; k++) {
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

static void cRadarScorer_dtor(cRadarScorer *self) {
	CPsaImage *img = self->radar;
	self->base.vftable = CRADARSCORER_VFTABLE;
	Radar_FreeImage(img);
	Radar_FreeImage(self->mask);
	Radar_FreeImage(self->icons);
	Radar_FreeImage(self->noise);
	Radar_FreeImage(self->cone);
	for (int k = 0; k < g_pWorld->floor.count; k++) {
		Radar_FreeImage(self->floorMaps[k]);
	}
	cScorerItem_dtor(&self->base);
}

cInputAction *InputActionTable_At(void *table, uint32_t i) {
	return TABLE_AT(*(TABLE(cInputAction) *)table, i);
}

cActionSet *ActionSetTable_At(void *table, uint32_t i) {
	return TABLE_AT(*(TABLE(cActionSet) *)table, i);
}

static cScorerItem *cRadarScorer_Delete(cScorerItem *base, byte flags) {
	cRadarScorer *self = (cRadarScorer *)base;
	cRadarScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cRadarScorer));
	}
	return base;
}

/* the floor of height y; the fade starts when it changed */
static void cRadarScorer_UpdateFloor(cRadarScorer *self, float y) {
	int old = self->floor;
	int n = g_pWorld->floor.count;
	int k = 1;

	while (k < n && !(g_pWorld->floor.levels[k].y > y)) {
		k++;
	}
	self->floor = k - 1;
	if (self->floor != old) {
		self->prevFloor = old;
		self->floorFade = 1.0f - self->floorFade;
	}
}

static float g_RadarScale;
/* the view of the radar: the turn and center of the map, its scale */
static float g_RadarAngle;
/* a floor of the world (0x18c bytes from world + 0x2f4, their count at 0x2f0): its lowest height first */
static float g_RadarCenterX;
static float g_RadarCenterZ;

/* a view cone at pos to the angle, spread wide, range long, mode 1..3 the color */
static void cRadarScorer_DrawCone(cRadarScorer *self, const vec3_t *pos, float ang, float spread, float range,
	int mode) {
	float verts[3][2];
	uint32_t colors[3];
	float y = pos->y;
	int side;

	/* on the floor shown, or on the one before while it fades */
	if (g_pWorld->floor.levels[self->floor].y > y) {
		side = -1;
	} else if (self->floor + 1 >= g_pWorld->floor.count) {
		side = 0;
	} else {
		side = !(g_pWorld->floor.levels[self->floor + 1].y >= y);
	}
	if (side) {
		if (self->floorFade >= 1.0f) {
			return;
		}
		int p = self->prevFloor;
		if (g_pWorld->floor.levels[p].y > y) {
			return;
		}
		if (p + 1 < g_pWorld->floor.count && !(g_pWorld->floor.levels[p + 1].y >= y)) {
			return;
		}
	}
	vec3_t at = *pos;
	float dz = at.z - g_RadarCenterZ;
	float dx = at.x - g_RadarCenterX;
	float s, c;
	Float_SinCos(g_RadarAngle, &s, &c);
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
	Float_SinCos(half + ang, &s, &c);
	verts[1][0] = s * r + cx;
	verts[1][1] = c * r + cy;
	Float_SinCos(ang - half, &s, &c);
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

#define RADAR_ICON_HALF 9.0f		/* the half size of an icon in pixels */
#define RADAR_ICON_MIN_ALPHA 10
#define RADAR_ICON_BIG 2.0f		/* the size factor of RADAR_ICON_OUT_BIG */
/* the color an icon was last drawn with, after the icon indices */
#define Radar_IconColor(self, i) (*(uint32_t *)&(self)->iconIndex[RADAR_ICONS + (i)])

/* the corner (x, y) of an icon turned by ang and scaled by size around the center c */
static void Radar_IconCorner(float *out, float x, float y, float ang, float size, const float *c) {
	float in[2] = {x, y};
	float r[2];

	Vec2_Rotate(r, in, ang);
	out[0] = r[0] * size + c[0];
	out[1] = r[1] * size + c[1];
}

/* the icon of an entity at pos turned by ang. In the radar (a square on the menu map, a circle else) icon with color;
 * out of it, flags RADAR_ICON_OUT_HIDE aside, outIcon with outColor on the border: turned to the center
 * (RADAR_ICON_OUT_POINT), by ang (RADAR_ICON_OUT_ANG) or not, doubled (RADAR_ICON_OUT_BIG), fading with the
 * distance (RADAR_ICON_OUT_FADE). An entity on another floor fades like the floors unless always */
static byte cRadarScorer_OnPrevFloor(cRadarScorer *self, float y);
static void cRadarScorer_DrawIcon(cRadarScorer *self, const vec3_t *pos, int icon, float ang, int outIcon,
	uint32_t flags, uint32_t color, uint32_t outColor, byte always) {
	float c[2], uv0[2], uv1[2], uv[4][2], verts[4][2];
	float y = pos->y;
	float a, dist, size;
	int side;

	if (g_pWorld->floor.levels[self->floor].y > y) {
		side = -1;
	} else if (self->floor + 1 >= g_pWorld->floor.count) {
		side = 0;
	} else {
		side = g_pWorld->floor.levels[self->floor + 1].y < y;
	}
	if (!always && side) {
		if (self->floorFade >= 1.0f) {
			return;
		}
		if (!cRadarScorer_OnPrevFloor(self, y)) {
			return;
		}
	}
	vec3_t p = *pos;
	float dz = p.z - g_RadarCenterZ;
	float dx = p.x - g_RadarCenterX;
	float s, co;
	Float_SinCos(g_RadarAngle, &s, &co);
	float rx = co * dx - s * dz;
	size = 1.0f;
	float ry = co * dz + s * dx;
	rx = rx / g_RadarScale;
	ry = ry / g_RadarScale;
	rx = -rx;
	float hw = self->halfW2;
	byte out;
	if (self->menuMap) {
		float ax = __builtin_fabsf(rx), ay = __builtin_fabsf(ry);
		dist = Float_MaxSS(ax, ay);
		out = ax > hw || ay > self->halfH2;
	} else {
		dist = (float)__builtin_sqrt((double)(ry * ry + rx * rx));
		out = dist > hw;
	}
	uint32_t col;
	int idx;
	if (!out) {
		a = ang - g_RadarAngle;
		idx = icon;
		col = color;
	} else {
		if (flags & RADAR_ICON_OUT_HIDE) {
			return;
		}
		if (self->menuMap) {
			float hh = self->halfH2;
			rx = Float_MaxSS(-hw, Float_MinSS(hw, rx));
			ry = Float_MaxSS(-hh, Float_MinSS(hh, ry));
		} else {
			float k = hw / dist;
			rx = k * rx;
			ry = k * ry;
		}
		if (flags & RADAR_ICON_OUT_POINT) {
			a = (float)-atan2((double)rx, (double)ry);
		} else if (flags & RADAR_ICON_OUT_ANG) {
			a = ang - g_RadarAngle;
		} else {
			a = 0.0f;
		}
		if (flags & RADAR_ICON_OUT_BIG) {
			size = RADAR_ICON_BIG;
		}
		idx = outIcon;
		if (flags & RADAR_ICON_OUT_FADE) {
			int f = self->outAlphaFactor;
			col = (uint32_t)((int)(outColor >> 24) * f / ((int)(dist - hw) + f)) << 24 | (outColor & 0xffffff);
		} else {
			col = outColor;
		}
	}
	Radar_IconColor(self, idx) = col;
	if ((byte)(Radar_IconColor(self, idx) >> 24) < RADAR_ICON_MIN_ALPHA) {
		return;
	}
	c[0] = self->halfW + self->base.x + rx;
	c[1] = self->halfH + self->base.y + ry;
	CPsaImage_GetFrameUV(self->icons, self->iconIndex[idx], uv0, uv1);
	uv[0][0] = uv0[0];
	uv[0][1] = uv0[1];
	uv[1][0] = uv1[0];
	uv[1][1] = uv0[1];
	uv[2][0] = uv1[0];
	uv[2][1] = uv1[1];
	uv[3][0] = uv0[0];
	uv[3][1] = uv1[1];
	Radar_IconCorner(verts[0], -RADAR_ICON_HALF, -RADAR_ICON_HALF, a, size, c);
	Radar_IconCorner(verts[1], RADAR_ICON_HALF, -RADAR_ICON_HALF, a, size, c);
	Radar_IconCorner(verts[2], RADAR_ICON_HALF, RADAR_ICON_HALF, a, size, c);
	Radar_IconCorner(verts[3], -RADAR_ICON_HALF, RADAR_ICON_HALF, a, size, c);
	if (!always) {
		uint32_t ic = Radar_IconColor(self, idx);
		float al = (float)(int)(ic >> 24);
		al = side == 0 ? al * self->floorFade : al * (1.0f - self->floorFade);
		Radar_IconColor(self, idx) = (uint32_t)(int)al << 24 | (ic & 0xffffff);
	}
	self->icons->color = Radar_IconColor(self, idx);
	CPsaImage_DrawPoly(self->icons, &verts[0][0], &uv[0][0], NULL, 4, 6);
}

#define RADAR_MAP_ALPHA 0xa4
#define RADAR_MAP_UV_SCALE 1.4142135f	/* the map square turns inside the radar */
#define RADAR_MAP_UV_ANG0 2.3561945f	/* 3/4 pi and 5/4 pi: the corners of the turned map square */
#define RADAR_MAP_UV_ANG1 3.926991f

static inline byte Radar_IsEMIed(cEntity *e) {
	return WithLife_IsEMIed(e);
}

/* the map image of a floor at the center of the radar with the alpha, turned by the texture coordinates */
static void Radar_DrawMapImage(cRadarScorer *self, CPsaImage *img, uint32_t alpha, const float *verts,
	const float *uv) {
	img->x = self->halfW + self->base.x;
	img->y = self->base.y + self->halfH;
	img->color = alpha << 24 | 0xffffff;
	CPsaImage_DrawPoly(img, verts, uv, NULL, 4, 6);
}

/* the view of the radar (scale by the speed of a car, the turn of the camera, the center at the target) and its
 * map: the floor map of the target fading in over the one before; the noise at random coordinates while the target
 * is EMIed */
static void cRadarScorer_DrawMap(cRadarScorer *self, cEntity *target, cEntity *cam) {
	vec3_t c;
	float verts[4][2], uv[4][2];
	float cu, cv;

	byte car = Entity_IsType(target, "Car");
	float scale = self->map2dScale;
	g_RadarScale = scale;
	if (car) {
		g_RadarScale = self->map2dScaleFactor * ((cVehicleEntity *)target)->smoothSpeed + scale;
	}
	Obj_GetViewPos(target, &c);
	if (self->menuMap) {
		g_RadarAngle = 0.0f;
	} else {
		g_RadarAngle = CamEntity_AngY(cam) + M_PI_F;
	}
	g_RadarCenterX = self->scrollX + c.x;
	g_RadarCenterZ = self->scrollY + c.z;
	float w = -self->halfW, h = -self->halfH;
	verts[0][0] = w;
	verts[0][1] = h;
	verts[1][0] = -w;
	verts[1][1] = h;
	verts[2][0] = -w;
	verts[2][1] = -h;
	verts[3][0] = w;
	verts[3][1] = -h;
	if (Radar_IsEMIed(target) && !self->menuMap) {
		double r0 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		double r1 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		cu = (float)(r1 + r1 - 1.0);
		cv = (float)(r0 + r0 - 1.0);
	} else {
		cu = (g_RadarCenterX - self->mapX0) / self->mapW;
		cv = (g_RadarCenterZ - self->mapZ0) / self->mapH;
	}
	float k = g_RadarScale * RADAR_MAP_UV_SCALE;
	float su = self->halfW / self->mapW * k;
	float sv = self->halfH / self->mapH * k;
	if (Radar_IsEMIed(target) && !self->menuMap) {
		uv[0][0] = cu + 0.0f;
		uv[0][1] = cv + 0.0f;
		uv[1][0] = cu + 0.0f;
		uv[1][1] = cv + 1.0f;
		uv[2][0] = cu + 1.0f;
		uv[2][1] = cv + 1.0f;
		uv[3][0] = cu + 1.0f;
		uv[3][1] = cv + 0.0f;
	} else {
		float s0, c0, s1, c1;
		Float_SinCos(g_RadarAngle + RADAR_MAP_UV_ANG0, &s0, &c0);
		Float_SinCos(g_RadarAngle + RADAR_MAP_UV_ANG1, &s1, &c1);
		uv[0][0] = s0 * su + cu;
		uv[0][1] = c0 * sv + cv;
		uv[1][0] = s1 * su + cu;
		uv[1][1] = c1 * sv + cv;
		uv[2][0] = cu + -s0 * su;
		uv[2][1] = cv + -c0 * sv;
		uv[3][0] = cu + -s1 * su;
		uv[3][1] = cv + -c1 * sv;
	}
	if (Radar_IsEMIed(target) && !self->menuMap) {
		Radar_DrawMapImage(self, self->noise, RADAR_MAP_ALPHA, &verts[0][0], &uv[0][0]);
		return;
	}
	CPsaImage *img = self->floorMaps[self->floor];
	float fade = self->floorFade;
	uint32_t alpha = RADAR_MAP_ALPHA;
	if (1.0f > fade) {
		alpha = (uint32_t)(int)(fade * (float)RADAR_MAP_ALPHA);
		Radar_DrawMapImage(self, self->floorMaps[self->prevFloor], RADAR_MAP_ALPHA - alpha, &verts[0][0], &uv[0][0]);
	}
	Radar_DrawMapImage(self, img, alpha, &verts[0][0], &uv[0][0]);
}

static cGVar GVar_RadarShowAll;

/* ---- the radar ---- */

#define g_RadarShowAll (GVar_RadarShowAll.value.i)		/* RadarShowAll: the neutral vehicles and characters too */
static cGVar GVar_IsSecondMission;
#define g_IsSecondMission (GVar_IsSecondMission.value.i)	/* IsSecondMission */

#define RADAR_PI 3.14159274f
#define RADAR_TEAM_NEUTRAL 3		/* arrow.tgType */
#define RADAR_TEAM_NONE 4

/* the blinking of an icon waiting for the player: the alpha |time * 1024 % 511 - 255| */
static uint32_t Radar_Pulse(cRadarScorer *self) {
	int a = (int)(Scorer_Get(self->base.player)->time * 1024.0f) % 511 - 255;
	return (uint32_t)(a < 0 ? -a : a) << 24 | 0xffffff;
}

/* the icon of e at its view position */
static void Radar_Icon(cRadarScorer *self, cEntity *e, int icon, float ang, int outIcon, uint32_t flags,
	uint32_t color, uint32_t outColor, byte always) {
	vec3_t p;

	cRadarScorer_DrawIcon(self, Obj_GetViewPos(e, &p), icon, ang, outIcon, flags, color, outColor, always);
}

/* the class name of the action controller of e starts with cls (n letters; 0: the whole name) */
static byte Radar_ActIs(cEntity *e, const char *cls, size_t n) {
	if (!WithLife_GetActCtrl(e)) {
		return 0;
	}
	const char *name = WithLife_GetActCtrl(e)->cls->name;
	return n ? !crt_strncmp(name, cls, n) : !strcmp(name, cls);
}

/* the view cones of the characters with life around */
static void Radar_DrawCones(cRadarScorer *self, cEntity *target) {
	vec3_t a, p;

	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e == target || !e->active || !Entity_IsType(e, TYEN_WITH_LIFE)) {
			continue;
		}
		cArrowIndicator *ar = &WLife(e)->arrow;
		int mode = ar->coneStatus;
		if (!mode) {
			continue;
		}
		float c = (float)cos((double)(ar->coneFov * 0.5f));
		float range = ar->coneDist / c;
		float ang = g_RadarAngle - Obj_GetSndAng(e, &a)->y + RADAR_PI;
		cRadarScorer_DrawCone(self, Obj_GetViewPos(e, &p), ang, ar->coneFov, range, mode);
	}
}

/* the icon of an entity of the world: the places of the level, the doors, the characters and vehicles by team
 * (enemies counted in g_NumEnemiesInRadar) */
static void Radar_DrawEntity(cRadarScorer *self, cEntity *target, cEntity *e, byte isCar, cEntity *scrapyard,
	uint32_t friendColor, uint32_t enemyColor) {
	vec3_t a, p;
	float ang = g_RadarAngle;

	if (isCar && !strcmp(Entity_GetClassName(e), "Obj")) {
		if (!crt_strncmp(e->name, "DM_AreaToPark_", 14)) {
			cRadarScorer_DrawIcon(self, Entity_GetPos(e, &p), 15, ang, 15, 0x11, 0xffffffff, 0xffffffff, 0);
		}
		return;
	}
	if (Entity_IsType(e, "item")) {
		if (!strcmp(e->name, "ParkParking_Junkyard")) {
			const char *bp = SaveVar_Get("NewBluePrintAvaliable");
			uint32_t c = !bp || *bp == '0' ? 0xffffffff : Radar_Pulse(self);
			Radar_Icon(self, e, 20, g_RadarAngle, 20, 0, c, c, 0);
		}
		if (scrapyard && !strcmp(e->name, "DM_OutLink_Outskirts")) {
			uint32_t c = g_SuperDealAvaliable && !g_IsDemo ? Radar_Pulse(self) : 0xffffffff;
			Radar_Icon(self, e, 21, g_RadarAngle, 21, 0, c, c, 0);
		}
		return;
	}
	if (!Entity_IsType(e, TYEN_WITH_LIFE)) {
		return;
	}
	if (!strcmp(e->name, "DM_Door_Elevator_GamblinDen")) {
		uint32_t c = g_SuperDealAvaliable ? Radar_Pulse(self) : 0xffffffff;
		Radar_Icon(self, e, 21, g_RadarAngle, 21, 0, c, c, 0);
		return;
	}
	if (Radar_ActIs(e, "DoorMetro", 9)) {
		Radar_Icon(self, e, 19, g_RadarAngle, 19, 0, 0xffffffff, 0xffffffff, 0);
		return;
	}
	if (Radar_ActIs(e, "DoorTeleporter", 14)) {
		Radar_Icon(self, e, 16, g_RadarAngle, 16, 0x11, 0xffffffff, 0xffffffff, 0);
		return;
	}
	if (Radar_ActIs(e, "Conversor", 0)) {
		Radar_Icon(self, e, 18, Obj_GetViewAng(e, &a)->y, 18, 0x11, 0xffffffff, 0xffffffff, 0);
		return;
	}
	if (self->menuMap) {
		return;
	}
	int team = WLife(e)->arrow.tgType;
	if (team == RADAR_TEAM_NEUTRAL) {
		if (!g_RadarShowAll) {
			return;
		}
		if (Entity_IsType(e, TYEN_WALKCHAR) || Entity_IsType(e, TYEN_CAR)) {
			Radar_Icon(self, e, 5, Obj_GetViewAng(e, &a)->y, 4, 5, 0xffffffff, 0xffffffff, 0);
		} else {
			Radar_Icon(self, e, 4, Obj_GetViewAng(e, &a)->y, 4, 5, 0xffffffff, 0xffffffff, 0);
		}
		return;
	}
	if (team == WLife(target)->arrow.tgType && team != RADAR_TEAM_NONE) {
		Radar_Icon(self, e, 5, Obj_GetViewAng(e, &a)->y, 4, 5, friendColor, friendColor, 0);
		return;
	}
	Radar_Icon(self, e, 5, Obj_GetViewAng(e, &a)->y, 4, 5, enemyColor, enemyColor, 0);
	g_NumEnemiesInRadar++;
}

/* the way up (up) or down to the floor of the mission object from the floor of the radar, nearest to the target:
 * its point and angle */
static float Radar_FloorWay(cRadarScorer *self, cEntity *target, byte up, vec3_t *p) {
	vec3_t t;

	const vec3_t *tp = Entity_GetPos(target, &t);
	if (up) {
		const MapLevel *l = (&g_pWorld->floor.levels[self->floor]);
		int k = MapLevel_NearestUp(l, tp);
		l = (&g_pWorld->floor.levels[self->floor]);
		*p = l->up[k];
		return l->upAng[k];
	}
	const MapLevel *l = (&g_pWorld->floor.levels[self->floor + 1]);
	int k = MapLevel_NearestDown(l, tp);
	l = (&g_pWorld->floor.levels[self->floor + 1]);
	*p = l->down[k];
	return l->downAng[k];
}

/* the mark of a mission object (RadarShow 1 the main missions, 2 and 3 the others): on another floor the way
 * there, blinking while its RadarShow changed; flags 9 for the nearest main mission */
static int cRadarScorer_FloorSide(cRadarScorer *self, float y);
static void Radar_DrawMission(cRadarScorer *self, cEntity *target, cEntity *e, cEntity *nearest) {
	vec3_t p, a;
	cObjEntity *o = (cObjEntity *)e;

	Obj_GetViewPos(e, &p);
	float ang = g_RadarAngle;
	int outIcon = 8, icon = 6;
	int kind = o->radarShow;
	if (kind && o->radarTime > 0.0f && ((int)(o->radarTime * 10.0f) & 1)) {
		return;
	}
	if (kind == 1) {
		byte isNearest = !nearest || nearest == e;
		const MapLevel *l = (&g_pWorld->floor.levels[self->floor]);
		if (l->y > p.y) {
			ang = Radar_FloorWay(self, target, 1, &p) + RADAR_PI;
			outIcon = 22;
			icon = 9;
		} else if (self->floor + 1 < g_pWorld->floor.count && !((&g_pWorld->floor.levels[self->floor + 1])->y >= p.y)) {
			ang = Radar_FloorWay(self, target, 0, &p) + RADAR_PI;
			outIcon = 22;
			icon = 9;
		} else if (!crt_strncmp(e->name, "DM_OutLink_", 11)) {
			p = *Obj_GetViewPos(e, &a);
			ang = Obj_GetViewAng(e, &a)->y;
			outIcon = 22;
			icon = 9;
		} else {
			outIcon = 8;
			icon = 12;
		}
		uint32_t c = g_IsSecondMission ? 0x80ffff00 : 0xffffff00;
		icon = (int)(Scorer_Get(self->base.player)->time * 8.0f) % 2 + icon;
		cRadarScorer_DrawIcon(self, &p, icon, ang, outIcon, isNearest ? 9 : 1, c, c, 0);
		return;
	}
	if (kind != 2 && kind != 3) {
		return;
	}
	switch (cRadarScorer_FloorSide(self, p.y)) {
	case -1:
		ang = Radar_FloorWay(self, target, 1, &p) + RADAR_PI;
		icon = 9;
		outIcon = 22;
		break;
	case 0:
		icon = 12;
		outIcon = 8;
		break;
	case 1:
		ang = Radar_FloorWay(self, target, 0, &p) + RADAR_PI;
		icon = 9;
		outIcon = 22;
		break;
	}
	uint32_t c = o->radarShow == 2 ? 0xff00ffff : 0xffff0000;
	icon = (int)(Scorer_Get(self->base.player)->time * 8.0f) % 3 + icon;
	cRadarScorer_DrawIcon(self, &p, icon, ang, outIcon, 1, c, c, 0);
}

/* the radar: its map, the view cones, the places, doors, characters and vehicles, the target and the mission
 * objects */
static void cRadarScorer_DrawRadar(cRadarScorer *self, cEntity *target, cEntity *cam) {
	vec3_t t, a;

	byte isCar = Entity_IsType(target, TYEN_CAR);
	cRadarScorer_UpdateFloor(self, Obj_GetViewPos(target, &t)->y);
	float h = self->halfH + self->halfH;
	float w = self->halfW + self->halfW;
	Render_ClearRect(self->base.x, self->base.y, w, h, 0, 0);
	Eng_SetViewRect(self->base.x, self->base.y, w, h);
	if (!self->menuMap) {
		self->mask->color = 0xffffffff;
		CPsaImage_DrawFrame(self->mask, self->maskIndex, self->base.x - 1.5f, self->base.y - 7.0f);
	}
	cRadarScorer_DrawMap(self, target, cam);
	if (!self->menuMap) {
		Radar_DrawCones(self, target);
	}
	Eng_ResetViewRect();
	if (!self->menuMap) {
		self->radar->color = 0xffffffff;
		CPsaImage_DrawFrame(self->radar, self->radarIndex, self->base.x - 9.0f, self->base.y - 7.0f);
	}
	if (Radar_IsEMIed(target) && !self->menuMap) {
		return;
	}
	/* the own team in red and the others in green for the team 0 of a network game, else the other way round */
	uint32_t friendColor, enemyColor;
	if (WLife(target)->arrow.tgType == 0 && (g_pApp->flags & (APPF_CLIENT | APPF_SERVER))) {
		friendColor = 0xffff0000;
		enemyColor = 0xff00ff00;
	} else {
		enemyColor = 0xffff0000;
		friendColor = 0xff00ff00;
	}
	cEntity *scrapyard = World_FindEntity("DM_OutLink_Scrapyard");
	g_NumEnemiesInRadar = 0;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e != target && e->active) {
			Radar_DrawEntity(self, target, e, isCar, scrapyard, friendColor, enemyColor);
		}
	}
	/* the target of the ship */
	if (isCar && Veh(target)->target[0]) {
		cEntity *e = World_FindEntity(Veh(target)->target);
		if (e && Entity_IsType(e, TYEN_WITH_LIFE)) {
			uint32_t c = cVehicleO648_IsStale(&Veh(target)->o648) ? 0xffffff00 : 0xffc0c000;
			Radar_Icon(self, e, 1, g_RadarAngle, 3, 1, c, c, 1);
		}
	}
	/* the nearest main mission object over the floors */
	cEntity *nearest = NULL;
	if (g_RadarMMSelect) {
		float best = 3.40282347e+38f;
		for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
			if (e == target || !e->active || !Entity_IsType(e, "Obj") || Obj(e)->radarShow != 1) {
				continue;
			}
			const vec3_t *tp = Entity_GetPos(target, &t);
			const vec3_t *ep = Entity_GetPos(e, &a);
			float d = Floor_Dist(&g_pWorld->floor, *ep, *tp);
			if (best > d) {
				nearest = e;
				best = d;
			}
		}
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e != target && e->active && Entity_IsType(e, "Obj")) {
			Radar_DrawMission(self, target, e, nearest);
		}
	}
	/* the target itself in the middle, pulsing in the menu map */
	if (!self->menuMap) {
		float y = Obj_GetViewAng(target, &a)->y;
		cRadarScorer_DrawIcon(self, Obj_GetViewPos(target, &t), 0, y, 0, 8, 0xffffffff, 0x80ff0000, 1);
		return;
	}
	double s = sin((double)(Scorer_Get(self->base.player)->time * 8.0f));
	uint32_t v = (uint32_t)(int)((float)((s + 1.0) * 0.5) * 255.0f);
	uint32_t c = (v << 8 | v) << 8 | 0xff0000ff;
	float y = Obj_GetViewAng(target, &a)->y;
	cRadarScorer_DrawIcon(self, Obj_GetViewPos(target, &t), 5, y, 4, 8, c, c, 1);
}

/* y is on the floor shown before */
static byte cRadarScorer_OnPrevFloor(cRadarScorer *self, float y) {
	int p = self->prevFloor;

	if (g_pWorld->floor.levels[p].y > y) {
		return 0;
	}
	if (p + 1 >= g_pWorld->floor.count) {
		return 1;
	}
	return g_pWorld->floor.levels[p + 1].y >= y;
}

/* y below the floor (-1), on it (0) or above it (1) */
static int cRadarScorer_FloorSide(cRadarScorer *self, float y) {
	int f = self->floor;

	if (g_pWorld->floor.levels[f].y > y) {
		return -1;
	}
	if (f + 1 >= g_pWorld->floor.count) {
		return 0;
	}
	return !(g_pWorld->floor.levels[f + 1].y >= y);
}

static int cRadarScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cRadarScorer *self = (cRadarScorer *)base;
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

static void cRadarScorer_Draw(cScorerItem *base, int player) {
	cRadarScorer *self = (cRadarScorer *)base;
	const char *camName = cWorld_GetCamName(g_pWorld, player);
	cEntity *cam = World_FindEntity(camName);
	vec3_t c;

	float ft = Scorer_Get(player)->frameTime;
	float fade = ft + ft + self->floorFade;
	self->floorFade = fade;
	if (fade >= 1.0f) {
		self->floorFade = 1.0f;
	}
	if (!cam || !cam->active || !Entity_IsType(cam, "FCam")) {
		return;
	}
	cEntity *target = cCamEntity_GetTarget(cam);
	if (!target || !Entity_IsType(target, TYEN_WITH_LIFE)) {
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
	Obj_GetViewPos(target, &c);
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

/* the vtable of cRadarScorer */
static const cScorerItemVtbl cRadarScorer_vftable = {
	.Draw = cRadarScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cRadarScorer_Var,
	.Delete = cRadarScorer_Delete,
};

cGVar GVar_SuperDealAvaliable;
cGVar GVar_RadarMMSelect;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_RadarShowAll, &GVar_RadarShowAll, "RadarShowAll", 0, "Show all the vehicles and chars at the radar", 0)
INIT_GVAR_INT(Init_GVar_RadarMMSelect, &GVar_RadarMMSelect, "RadarMMSelect", 0, "1 if Show only one main mission target (nearest)", 1)
INIT_GVAR_INT(Init_GVar_SuperDealAvaliable, &GVar_SuperDealAvaliable, "SuperDealAvaliable", 0, "1 if superdeal is avaliable", 0)
INIT_GVAR_INT(Init_GVar_IsSecondMission, &GVar_IsSecondMission, "IsSecondMission", 0, "Is active any secondary mission?", 0)
