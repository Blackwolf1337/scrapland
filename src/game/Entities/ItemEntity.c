/*
 * cItemEntity, the pickups of the levels, see include/scrap/itementity.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/fxmisc.h"
#include "scrap/itementity.h"
#include "scrap/net.h"
#include "scrap/scrapcollision.h"
#include "scrap/sound.h"
#include "scrap/withlifeentity.h"
#include "scrap/staticinit.h"

#define CITEMENTITY_VFTABLE (&cItemEntity_vftable.base)
#define g_ItemFxDist (GVar_FXItemFadeMaxDist.value.f)		/* the fade effects only nearer (60000) */
#define g_ItemSyncDist (GVar_NetItemDistAct.value.f)
static const cObjEntityVtbl cItemEntity_vftable;
static cGVar GVar_NetItemDistAct;
static cGVar GVar_FXItemFadeMaxDist;
#define ITEM_CLASSNAME "item"

/* the entity callback of the sphere test: other touched self at point */

int Vec3_Differ(const float *a, const float *b);

static inline cEntity *Item_Ent(cItemEntity *e) {
	return &e->base;
}

/* World.h: the entity of local player 0 */
static cEntity *Item_Usr0(void) {
	cWorld *w = g_pWorld;
	cEntity *pl = w->owners[0];

	if (pl && !Entity_IsType(pl, TYEN_WITH_LIFE)) {
		CheckFailed("UsrEntities[i]->IsType(\"WithLife\")", "World.h", __TIMESTAMP__, __LINE__);
	}
	return w->owners[0];
}

/* the reset to the origin with OnReset */
static void Item_Reset(cItemEntity *self) {
	Entity_SetPos(self, &self->origin);
	cPy_Function_AddParamS(&self->onReset, self->base.name);
	cPy_Function_Execute(&self->onReset, 0);
}

/* the fade effects only when the sphere of the item is in view */
static byte Item_FxVisible(cItemEntity *self) {
	vec3_t tmp;
	const vec3_t *p = Entity_GetPos(self, &tmp);
	float r = Obj_GetRadius(self);

	return FX_IsVisible(p->x, p->y, p->z, r + r, g_ItemFxDist, NULL, 0);
}

cEntity *cItemEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cItemEntity *self = mem;

	cObjEntity_ctor(mem, name, x, y, z);
	self->base.vftable = CITEMENTITY_VFTABLE;
	Nto1Node_Init(&self->taker);
	cPy_Function_ctor(&self->onTake);
	cPy_Function_ctor(&self->onReset);
	self->base.mask |= 4;
	self->radius = 1000.0f;
	self->respawn = 0.0f;
	self->obj.slot->minDiff = 0.05f;
	self->syncTicks = 5;
	self->owner[0] = 0;
	self->origin.x = x;
	self->origin.y = y;
	self->origin.z = z;
	self->backTime = 0.0f;
	self->appearsIfFree = 0;
	self->autoFace = 0;
	self->takeSoundVol = 0.0f;
	self->takeSound[0] = 0;
	self->typeMask = 0xffff;
	self->onlyUser = 0;
	self->noFx = 1;
	self->blink = 0;
	self->fadeIn = 1;
	self->fadeOut = 1;
	self->taker.data = self;
	return &self->base;
}

static void cItemEntity_dtor(cItemEntity *self) {
	self->base.vftable = CITEMENTITY_VFTABLE;
	cPy_Function_dtor(&self->onReset);
	cPy_Function_dtor(&self->onTake);
	Nto1Node_Done(&self->taker);
	cObjEntity_dtor(&self->base);
}

static cEntity *cItemEntity_Delete(cEntity *base, uint32_t flags) {
	cItemEntity *self = (cItemEntity *)base;
	cItemEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CITEMENTITY_SIZE);
	}
	return base;
}

static void cItemEntity_Touch(cEntity *elem, cEntity *other, const vec3_t *point, const vec3_t *dir) {
	cItemEntity *self = (cItemEntity *)elem;
	cEntity *pl = Item_Usr0();

	(void)point;
	(void)dir;
	if (self->onlyUser && (!pl || pl != other)) {
		return;
	}
	if (self->typeMask != 0xffff && (!Entity_IsType(other, TYEN_WITH_LIFE) ||
		!(self->typeMask & (1u << (WLife(other)->arrow.tgType & 31))))) {
		return;
	}
	if (self->base.deadNode.target || !(other->mask & 0x11) || !(byte)cPy_Function_IsValid(&self->onTake)) {
		return;
	}
	if (self->taker.target) {
		CHECK(self->taker.data);
		Nto1Node_Unlink(&self->taker);
	}
	cEntity_AddLink(other, &self->taker);
	cPy_Function_AddParamS(&self->onTake, self->base.name);
	cPy_Function_AddParamS(&self->onTake, other->name);
	cPy_Function_Execute(&self->onTake, 0);
}

static void cItemEntity_SetBackTime(cItemEntity *self, float t);
static byte cItemEntity_VarF(cEntity *elem, const char *name, double *d, int set) {
	cItemEntity *self = (cItemEntity *)elem;
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "TakeSoundVol")) {
		if ((byte)set) {
			self->takeSoundVol = (float)*d;
		} else {
			*d = self->takeSoundVol;
		}
		return 1;
	}
	if (!strcmp(name, "Radius")) {
		if ((byte)set) {
			self->radius = (float)*d;
		} else {
			*d = self->radius;
		}
		return 1;
	}
	if (!strcmp(name, "rTime")) {
		if ((byte)set) {
			cItemEntity_SetRespawn(self, (float)*d);
		} else {
			*d = self->respawn;
		}
		return 1;
	}
	if (!strcmp(name, "BackTime")) {
		if ((byte)set) {
			cItemEntity_SetBackTime(self, (float)*d);
		} else {
			*d = self->backTime;
		}
		return 1;
	}
	return cObjEntity_VarF(&self->base, name, d, set);
}

static const char *cItemEntity_GetClassName(cEntity *base) {
	cItemEntity *self = (cItemEntity *)base;
	(void)self;
	return ITEM_CLASSNAME;
}

static float cItemEntity_GetRadius(cEntity *base) {
	cItemEntity *self = (cItemEntity *)base;
	return self->radius;
}

/* a flag of the item from the int variable */
static inline void Item_VarFlag(byte *flag, int *i, int set) {
	if ((byte)set) {
		*flag = *i != 0;
	} else {
		*i = *flag != 0;
	}
}

static byte cItemEntity_VarI(cEntity *elem, const char *name, int *i, int set) {
	cItemEntity *self = (cItemEntity *)elem;
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "FadeInEffect")) {
		if ((byte)set) {
			self->fadeIn = *i;
		} else {
			*i = self->fadeIn;
		}
		return 1;
	}
	if (!strcmp(name, "FadeOutEffect")) {
		if ((byte)set) {
			self->fadeOut = *i;
		} else {
			*i = self->fadeOut;
		}
		return 1;
	}
	if (!strcmp(name, "Blink")) {
		Item_VarFlag(&self->blink, i, set);
		return 1;
	}
	if (!strcmp(name, "AutoFace")) {
		Item_VarFlag(&self->autoFace, i, set);
		return 1;
	}
	if (!strcmp(name, "AppearsIfFree")) {
		Item_VarFlag(&self->appearsIfFree, i, set);
		return 1;
	}
	if (!strcmp(name, "OnlyUser")) {
		Item_VarFlag(&self->onlyUser, i, set);
		return 1;
	}
	return cObjEntity_VarI(&self->base, name, i, set);
}

static byte cItemEntity_IsType(cEntity *elem, const char *type) {
	cItemEntity *self = (cItemEntity *)elem;
	if (!strcmp(type, ITEM_CLASSNAME)) {
		return 1;
	}
	return cObjEntity_IsType(&self->base, type);
}

static void cItemEntity_NetRead(cEntity *base, void *msg, int unused) {
	cItemEntity *self = (cItemEntity *)base;
	(void)unused;
	NetCtl_Read(self->obj.net, msg);
	if (self->obj.net->flags & 1) {
		cItemEntity_SetRespawn(self, 0.0f);
	} else {
		cItemEntity_SetRespawn(self, 3600.0f);
	}
}

static void cItemEntity_NetWrite(cEntity *base, float dist2, void *msg, int full) {
	cItemEntity *self = (cItemEntity *)base;
	(void)dist2;
	byte all = (0.0f >= (float)self->syncTicks) | (byte)full;
	NetCtl_Write(self->obj.net, (0.0f >= self->respawn) + 2, Item_Ent(self), msg, all);
	if (self->syncTicks > 0) {
		self->syncTicks--;
	}
}

static void cItemEntity_Tick(cEntity *elem, float dt) {
	cItemEntity *self = (cItemEntity *)elem;
	vec3_t to, hit;

	if (Net_IsNetEntity(Item_Ent(self))) {
		cNetController *net = self->obj.net;
		if (self->base.pos.x != net->pos.x || self->base.pos.y != net->pos.y || self->base.pos.z != net->pos.z) {
			self->base.pos.x = net->pos.x;
			self->base.pos.y = net->pos.y;
			self->base.pos.z = net->pos.z;
			self->syncTicks = Net_GetSendRate() * 5;
		}
	} else if (self->owner[0]) {
		/* an owner that is gone frees the item */
		cEntity *o = World_FindEntity(self->owner);
		if (o && !o->active) {
			self->owner[0] = 0;
		}
	} else {
		if (!(self->backTime > 0.0f)) {
			self->obj.vel.x = 0.0f;
			self->obj.vel.y = 0.0f;
			self->obj.vel.z = 0.0f;
		} else if (self->obj.vel.x != 0.0f || self->obj.vel.y != 0.0f || self->obj.vel.z != 0.0f) {
			/* pushed: slowed down and moved as a sphere */
			float f = dt * 0.5f;
			float mx = self->obj.vel.x * f;
			float my = self->obj.vel.y * f;
			float mz = self->obj.vel.z * f;
			self->obj.vel.x = self->obj.vel.x - mx;
			self->obj.vel.y = self->obj.vel.y - my;
			self->obj.vel.z = self->obj.vel.z - mz;
			to.x = self->obj.vel.x + self->base.pos.x;
			to.y = self->base.pos.y + self->obj.vel.y;
			to.z = self->base.pos.z + self->obj.vel.z;
			self->syncTicks = Net_GetSendRate() * 5;
			Engine_TestSphere(&self->base.pos, &to, self->radius, &hit, NULL);
			if ((byte)Vec3_Differ(&to.x, &hit.x)) {
				self->obj.vel.x = 0.0f;
				self->obj.vel.y = 0.0f;
				self->obj.vel.z = 0.0f;
			} else {
				self->base.pos.x = hit.x;
				self->base.pos.y = hit.y;
				self->base.pos.z = hit.z;
				if (200.0f > self->obj.vel.x * self->obj.vel.x + self->obj.vel.y * self->obj.vel.y + self->obj.vel.z * self->obj.vel.z) {
					self->obj.vel.x = 0.0f;
					self->obj.vel.y = 0.0f;
					self->obj.vel.z = 0.0f;
				}
			}
		}
		float bt = self->backTime;
		float left = bt - dt;
		if (left > 0.0f) {
			self->backTime = left;
		} else {
			if (bt > 0.0f) {
				Item_Reset(self);
			}
			self->backTime = 0.0f;
		}
		if (self->respawn <= 0.0f) {
			cWorld_TestSphereEntities(g_pWorld, Item_Ent(self), 0x11, &self->base.pos, self->radius, NULL, 0,
				0);
		} else {
			float t = self->respawn - dt;
			if (self->appearsIfFree && t <= 0.0f &&
				cWorld_TestSphereEntities(g_pWorld, Item_Ent(self), 0x11, &self->base.pos, self->radius, NULL,
					1, 0)) {
				/* somebody is on it: one second more */
				cItemEntity_SetRespawn(self, 1.0f);
			} else {
				cItemEntity_SetRespawn(self, t);
			}
		}
	}
	Grid_Add(&g_pWorld->grid, Item_Ent(self));
	cObjEntity_Tick(Item_Ent(self), dt);
	self->noFx = 0;
}

static void cItemEntity_SetOwner(cItemEntity *self, const char *name) {
	Str_Copy(self->owner, name, 0x80);
	if (self->respawn <= 0.0f) {
		cItemEntity_SetRespawn(self, 1.0f);
	}
}

static void cItemEntity_SetPos(cEntity *base, const vec3_t *pos) {
	cItemEntity *self = (cItemEntity *)base;
	self->syncTicks = Net_GetSendRate() * 5;
	cObjEntity_SetPos(Item_Ent(self), pos);
}

static void cItemEntity_SetBackTime(cItemEntity *self, float t) {
	if (t > 0.0f) {
		self->backTime = t;
		return;
	}
	if (self->backTime > 0.0f) {
		Item_Reset(self);
	}
	self->backTime = 0.0f;
}

void cItemEntity_SetRespawn(cItemEntity *self, float t) {
	vec3_t tmp;

	if (t > 0.0f) {
		if (self->respawn <= 0.0f) {
			self->syncTicks = Net_GetSendRate() * 5;
			if (CGameModel_IsLoaded(&self->obj.model)) {
				byte hide = 1;
				if (self->fadeOut && !self->noFx && (CGameModel_IsVisible(&self->obj.model) || self->blink) &&
					Item_FxVisible(self)) {
					/* the take effect towards the player that took it */
					cEntity *taker = self->taker.target;
					if (!g_SplitScreenGame && cWorld_GetUsrEntity(g_pWorld, 0) != taker) {
						taker = NULL;
					}
					hide = 0;
					if (self->fadeOut == 1) {
						FX_ItemFade(Item_Ent(self), 1, g_IsMapOutDoor ? 8.0f : 1.0f, taker);
					} else if (self->fadeOut == 2) {
						FX_ItemBlink(Item_Ent(self));
						hide = 1;
					}
				}
				if (hide) {
					CGameModel_Disable(&self->obj.model);
				}
			}
			if (self->takeSoundVol > 0.0f && self->takeSound[0]) {
				int vol = Snd_Volume(self->takeSoundVol);
				SoundSys_PlayAt(g_pSound, self->takeSound, Entity_GetPos(self, &tmp), vol);
			}
		}
		self->respawn = t;
	} else {
		if (self->respawn > 0.0f) {
			self->syncTicks = Net_GetSendRate() * 5;
			if (CGameModel_IsLoaded(&self->obj.model)) {
				byte show = 1;
				if (self->fadeIn && !self->noFx && Item_FxVisible(self)) {
					show = 0;
					if (self->fadeIn == 1) {
						FX_ItemFade(Item_Ent(self), 2, g_IsMapOutDoor ? 8.0f : 1.0f, NULL);
					} else if (self->fadeIn == 2) {
						FX_ItemBlink(Item_Ent(self));
						show = 1;
					}
				}
				if (show) {
					CGameModel_Enable(&self->obj.model);
				}
			}
		}
		self->respawn = 0.0f;
	}
	if (self->taker.target) {
		CHECK(self->taker.data);
		Nto1Node_Unlink(&self->taker);
	}
}

/* the teams of the letters R, B, G, N, K (either case); "" all */
static void cItemEntity_SetTypeMarker(cItemEntity *self, const char *types) {
	if (!*types) {
		self->typeMask = 0xffff;
		return;
	}
	self->typeMask = 0;
	for (const char *p = types; *p; p++) {
		switch (*p) {
		case 'R':
		case 'r':
			self->typeMask |= 1;
			break;
		case 'B':
		case 'b':
			self->typeMask |= 2;
			break;
		case 'G':
		case 'g':
			self->typeMask |= 4;
			break;
		case 'N':
		case 'n':
			self->typeMask |= 8;
			break;
		case 'K':
		case 'k':
			self->typeMask |= 0x10;
			break;
		default:
			Con_Warnf("Warning : SetTgTypeMarker, some types are erroneus at '%s'.\n", types);
			break;
		}
	}
}

static void cItemEntity_Update(cEntity *elem) {
	cItemEntity *self = (cItemEntity *)elem;
	vec3_t tmp, ang;

	cObjEntity_Update(Item_Ent(self));
	if (self->autoFace) {
		cWorld *w = g_pWorld;
		vec3_t cam = w->camPos[w->curCam];
		const vec3_t *p = Entity_GetPos(self, &tmp);
		double dx = (double)(p->x - cam.x);
		double dz = (double)(p->z - cam.z);
		ang.x = 0.0f;
		ang.y = (float)atan2(dx, dz);
		ang.z = 0.0f;
		CGameModel_SetRot(&self->obj.model, &ang);
	}
	if (self->blink && self->respawn == 0.0f) {
		float t = Timer_GetTime(&g_pWorld->timer);
		float s = cTimer_GetSpeed(&g_pWorld->timer);
		/* five blink phases a second of game time */
		int phase = (int)(float)(t / s * 5.0f);
		if (phase & 1) {
			if (!CGameModel_IsEnabled(&self->obj.model)) {
				CGameModel_Enable(&self->obj.model);
			}
		} else if (CGameModel_IsEnabled(&self->obj.model)) {
			if (CGameModel_IsVisible(&self->obj.model)) {
				FX_ItemBlink(Item_Ent(self));
			}
			CGameModel_Disable(&self->obj.model);
		}
	}
}

static byte cItemEntity_TestSphere(cEntity *elem, cEntity *other, const vec3_t *center, float radius, const vec3_t *arg) {
	cItemEntity *self = (cItemEntity *)elem;
	vec3_t tmp, dir, point;

	(void)arg;
	const vec3_t *p = Entity_GetPos(self, &tmp);
	float r = radius + self->radius;
	float dy = center->y - p->y;
	float dx = center->x - p->x;
	float dz = center->z - p->z;
	if (!(r * r >= dy * dy + dx * dx + dz * dz)) {
		return 0;
	}
	if (!g_pWorld->testing && other) {
		p = Entity_GetPos(self, &tmp);
		dx = center->x - p->x;
		dy = center->y - p->y;
		dz = center->z - p->z;
		float l2 = dx * dx + dy * dy + dz * dz;
		if (!(1.0f >= l2)) {
			float inv = 1.0f / (float)__builtin_sqrt((double)l2);
			dir.x = dx * inv;
			dir.y = dy * inv;
			dir.z = dz * inv;
		} else {
			dir.x = 0.0f;
			dir.y = 1.0f;
			dir.z = 0.0f;
		}
		float ox = dir.x * self->radius;
		float oy = dir.y * self->radius;
		float oz = dir.z * self->radius;
		p = Entity_GetPos(self, &tmp);
		point.x = p->x + ox;
		point.y = p->y + oy;
		point.z = p->z + oz;
		/* slot 11 of the other entity: touched by the item */
		Entity_Touch(other, Item_Ent(self), &point, &dir);
	}
	return 1;
}

static byte cItemEntity_VarS(cEntity *base, const char *name, char **s, int set) {
	cItemEntity *self = (cItemEntity *)base;
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "TakeSound")) {
		if ((byte)set) {
			Str_Copy(self->takeSound, *s, 0x80);
		} else {
			*s = self->takeSound;
		}
		return 1;
	}
	if ((byte)set) {
		if (!strcmp(name, "OnReset")) {
			cPy_Function_SetFunc(&self->onReset, *s);
			return 1;
		}
		if (!strcmp(name, "OnTake")) {
			cPy_Function_SetFunc(&self->onTake, *s);
			return 1;
		}
	}
	if (!strcmp(name, "Owner")) {
		if ((byte)set) {
			cItemEntity_SetOwner(self, *s);
		} else {
			*s = self->owner;
		}
		return 1;
	}
	if ((byte)set && !strcmp(name, "TgTypeMarker")) {
		cItemEntity_SetTypeMarker(self, *s);
		return 1;
	}
	return cObjEntity_VarS(Item_Ent(self), name, s, set);
}

static uint32_t g_ItemSyncCount;

static byte cItemEntity_NetNeedsSend(cEntity *base, float dist2) {
	cItemEntity *self = (cItemEntity *)base;
	if (self->syncTicks > 0) {
		return 1;
	}
	if (g_ItemSyncDist * g_ItemSyncDist > dist2) {
		/* near ones now and then: the count and ten ticks a second of game time */
		double t = ((App_GetGameTime(g_pApp)) * 10.0);
		uint32_t n = g_ItemSyncCount++;
		if ((int)((double)n + t) & 1) {
			return 1;
		}
	}
	return 0;
}

static byte cItemEntity_VarV(cEntity *elem, const char *name, double *x, double *y, double *z, int set) {
	cItemEntity *self = (cItemEntity *)elem;
	vec3_t v;

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Origin")) {
		if ((byte)set) {
			double vy = *y, vz = *z;
			self->origin.x = (float)*x;
			self->origin.y = (float)vy;
			self->origin.z = (float)vz;
		} else {
			v = self->origin;
			*x = v.x;
			*y = v.y;
			*z = v.z;
		}
		return 1;
	}
	if (!strcmp(name, "Vel")) {
		if ((byte)set) {
			v.x = (float)*x;
			v.y = (float)*y;
			v.z = (float)*z;
			Obj_SetVel(self, &v);
		} else {
			Obj_GetVel(self, &v);
			*x = v.x;
			*y = v.y;
			*z = v.z;
		}
		return 1;
	}
	return cObjEntity_VarV(Item_Ent(self), name, x, y, z, set);
}

/* the vtable of cItemEntity */
static const cObjEntityVtbl cItemEntity_vftable = {
	.base.Update = cItemEntity_Update,
	.base.Render = cEntity_Render,
	.base.UpdateOff = cObjEntity_UpdateOff,
	.base.Delete = cItemEntity_Delete,
	.base.GetClassName = cItemEntity_GetClassName,
	.base.IsType = cItemEntity_IsType,
	.base.GetPos = cObjEntity_GetPos,
	.base.SetPos = cItemEntity_SetPos,
	.base.SetActive = cObjEntity_SetActive,
	.base.TestLine = cEntity_TestLine,
	.base.TestSphere = cItemEntity_TestSphere,
	.base.Touch = cItemEntity_Touch,
	.base.SetModel = cObjEntity_SetModel,
	.base.GetTargetAngle = cObjEntity_GetTargetAngle,
	.base.AimWeapon = cEntity_AimWeapon,
	.base.SetEmbeddedFX = cObjEntity_SetEmbeddedFX,
	.base.VarI = cItemEntity_VarI,
	.base.VarF = cItemEntity_VarF,
	.base.VarS = cItemEntity_VarS,
	.base.VarV = cItemEntity_VarV,
	.base.Vf20 = cEntity_Vf20,
	.base.Vf21 = cEntity_Vf21,
	.base.GetAI = cEntity_GetAI,
	.Tick = cItemEntity_Tick,
	.UpdateModel = cObjEntity_UpdateModel,
	.GetNodeSlot = cObjEntity_GetNodeSlot,
	.GetRenderPos = cObjEntity_GetRenderPos,
	.GetRenderAng = cObjEntity_GetRenderAng,
	.GetSndAng = cObjEntity_GetSndAng,
	.GetViewPos = cObjEntity_GetRenderPos,
	.GetViewAng = cObjEntity_GetRenderAng,
	.GetSndPos = cObjEntity_GetSndPos,
	.GetAng = cObjEntity_GetAng,
	.SetAng = cObjEntity_SetAng,
	.GetVel = cObjEntity_GetVel,
	.SetVel = cObjEntity_SetVel,
	.GetRotVel = cObjEntity_GetRotVel,
	.SetRotVel = cObjEntity_SetRotVel,
	.GetModel = cObjEntity_GetModel,
	.GetCamera = cObjEntity_GetCamera,
	.GetRadius = cItemEntity_GetRadius,
	.NetWrite = cItemEntity_NetWrite,
	.NetNeedsSend = cItemEntity_NetNeedsSend,
	.NetRead = cItemEntity_NetRead,
	.SetEffect = cObjEntity_SetEffect,
	.SetEffectParam = cObjEntity_SetEffectParam,
	.Render = cObjEntity_Render,
};

static cGVar GVar_FXItemFadeMaxDist;
static cGVar GVar_NetItemDistAct;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_FXItemFadeMaxDist, &GVar_FXItemFadeMaxDist, "FXItemFadeMaxDist", 0, "Maximun Distance to show the Item Fade Effect", 60000.0f)
INIT_GVAR_FLOAT(Init_GVar_NetItemDistAct, &GVar_NetItemDistAct, "NetItemDistAct", 0, "Distance in cm to actualize the item data.", 8000.0f)
