/*
 * Logic/BaseLogic.cpp of the game: cLocalLogic, the base of every local logic (no variables), and cBaseLocalLogic,
 * the logic of a character (include/scrap/logic.h): its state with a target entity, the origin it keeps, the idle
 * and lock flags, the rage of the gears at a character that keeps a link, the wake up after Dazed. Also the view
 * tests the character logics search their targets with, Logic_GetLocalLogic and the debug texts of the AI
 * (ShowAI: the state and the target of the characters or the ships of the lists).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/staticinit.h"

#define CLOCALLOGIC_VFTABLE (&cLocalLogic_vftable)
#define CBASELOCALLOGIC_VFTABLE (&cBaseLocalLogic_vftable)
#define g_DefActionRadius (GVar_DefActionRadius.value.f)
#define g_GearRageMaxTime (GVar_GearRageMaxTime.value.f)
#define g_LogicAwakeTime (GVar_LogicAwakeTime.value.f)
static const cLocalLogicVtbl cLocalLogic_vftable;
static const cBaseLocalLogicVtbl cBaseLocalLogic_vftable;
static cGVar GVar_LogicAwakeTime;
static cGVar GVar_GearRageMaxTime;
static cGVar GVar_DefActionRadius;

/* the state names of the debug texts (pointer tables of the exe) */

cBaseLocalLogic *cBaseLocalLogic_ctor(cBaseLocalLogic *self, cEntity *parent) {
	vec3_t tmp, ang, out;

	self->parent = parent;
	self->vftable = CBASELOCALLOGIC_VFTABLE;
	self->target.target = NULL;
	self->target.data = NULL;
	self->target.next = &self->target;
	self->target.prev = &self->target;
	self->origin = *Entity_GetPos(parent, &tmp);
	CHECK(!strcmp(Entity_GetClassName(parent), TYEN_WALKCHAR));
	ang = WChar(parent)->view;
	vec3_t dir = { 0.0f, 0.0f, -1.0f };
	self->originAng = *Vec3_Rotate(&out, &dir, &ang);
	self->state = 0;
	self->target.data = self;
	self->idle = 1;
	self->busy = 0;
	self->alwaysLock = 0;
	self->dazed = 0;
	self->dazedTime = 0.0f;
	self->rageTime = 0.0f;
	self->actionRadius = g_DefActionRadius;
	return self;
}

void cBaseLocalLogic_dtor(cBaseLocalLogic *self) {
	self->vftable = CBASELOCALLOGIC_VFTABLE;
	Nto1Node_Done(&self->target);
	self->logic.vftable = CLOCALLOGIC_VFTABLE;
}

void cLocalLogic_dtor(cLocalLogic *self) {
	self->vftable = CLOCALLOGIC_VFTABLE;
}

static void cBaseLocalLogic_Free(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	cBaseLocalLogic_dtor(self);
	operator_delete(self);
}

static void cLocalLogic_Free(cLocalLogic *self) {
	self->vftable = CLOCALLOGIC_VFTABLE;
	operator_delete(self);
}

/* the view test of the original: distance, the angles of the way from pos to the viewer against its sound angles
 * (half the field of view each side), a free line */
static byte Logic_ViewTest(cEntity *viewer, const vec3_t *pos, const vec3_t *vp, float dist, float fovY, float fovX,
	byte testLine, const vec3_t *lineFrom) {
	vec3_t d, rot, sa, diff, n, tmp;

	d.x = vp->x - pos->x;
	d.y = vp->y - pos->y;
	d.z = vp->z - pos->z;
	if (d.y * d.y + d.x * d.x + d.z * d.z > dist * dist) {
		return 0;
	}
	const vec3_t *a = Vec3_GetRotAng(&rot, &d);
	const vec3_t *s = Obj_GetSndAng(viewer, &sa);
	diff.y = s->y - a->y;
	diff.x = s->x - a->x;
	diff.z = s->z - a->z;
	Vec3_NormAng(&n, &diff);
	if (__builtin_fabs((double)n.y) > (double)(fovY * 0.5f)) {
		return 0;
	}
	if (__builtin_fabs((double)n.x) > (double)(fovX * 0.5f)) {
		return 0;
	}
	if (testLine) {
		return !Engine_TestLine(lineFrom, Entity_GetPos(viewer, &tmp), NULL, NULL);
	}
	return 1;
}

byte Logic_InView(cEntity *viewer, const vec3_t *pos, float dist, float fovY, float fovX, byte testLine) {
	vec3_t tmp;

	return Logic_ViewTest(viewer, pos, Entity_GetPos(viewer, &tmp), dist, fovY, fovX, testLine, pos);
}

cEntity *Logic_FindInView(cEntity *viewer, cEntityListData *list, float dist, float fovY, float fovX, byte testLine,
	byte (*filter)(cEntity *)) {
	vec3_t t1, t2;

	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *c = n->data;
		if (viewer == c || !Entity_IsType(c, TYEN_WALKCHAR)) {
			continue;
		}
		const vec3_t *from = Entity_GetPos(c, &t1);
		const vec3_t *vp = Entity_GetPos(viewer, &t2);
		if (!Logic_ViewTest(viewer, from, vp, dist, fovY, fovX, testLine, from)) {
			continue;
		}
		if (!filter || filter(c)) {
			return c;
		}
	}
	return NULL;
}

/* "Lock" while the action of layer 0 is Crazy, Flashed or Dazed (dazed for the last) */
byte cBaseLocalLogic_UpdateBusy(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	cEntity *p = self->parent;

	self->dazed = 0;
	if (!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Crazy", 5) ||
		!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Flashed", 7)) {
		self->busy = 1;
		return self->busy;
	}
	if (!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Dazed", 5)) {
		self->dazed = 1;
		self->busy = 1;
		return self->busy;
	}
	self->busy = 0;
	return self->busy;
}

/* ---- the debug texts (ShowAI) ---- */
/* the character of the node: a line to its target (or the goal of AI state 4), the logic state and the name, the
 * AI states */
static void Logic_ShowChar(Nto1Node *node, const char **names) {
	char state[0x100], sub[0x100];
	cAICharSaved goal;
	vec3_t p, lp, tmp;
	CFont *font;

	cEntity *e = node->data;
	cBaseLocalLogic *l = e->lLogic;
	Entity_GetPos(e, &p);
	int st = l->state;
	cAIChar *ai = Entity_GetAI(e);
	AIChar_GetStateName(ai, state);
	AIChar_GetInternStateName(ai, sub);
	cEntity *t = l->target.target;
	if (t) {
		DebugUtils_DrawLine(&p, Entity_GetPos(t, &tmp), 0xff0000);
	} else {
		ai = Entity_GetAI(e);
		if (ai && AIChar_GetState(ai) == AIC_GO_TO) {
			AIChar_SaveTo(ai, &goal);
			DebugUtils_DrawLine(&p, &goal.dest, 0xff0000);
		}
	}
	if (l->busy) {
		g_FontTerminal8->color = 0xff0000;
	} else if (!l->idle && !l->alwaysLock) {
		g_FontTerminal8->color = 0xffffff;
	} else {
		g_FontTerminal8->color = 0xffff00;
	}
	lp.x = p.x + 0.0f;
	lp.y = p.y + 100.0f;
	lp.z = p.z + 0.0f;
	font = g_FontTerminal8;
	CFont_Printf3D(font, &lp, 3, "%s (%s)", names[st], e->name);
	g_FontTerminal8->color = 0xffffff;
	lp.x = p.x + 0.0f;
	lp.y = p.y - 100.0f;
	lp.z = p.z + 0.0f;
	CFont_Printf3D(g_FontTerminal8, &lp, 3, "%s (%s)", state, sub);
}

/* the characters from node on, only those with a visible model if visible */
static void Logic_ShowChars(Nto1Node *node, const char **names, byte visible) {
	for (; node; node = Logic_ListNext(node)) {
		cEntity *e = node->data;
		if (!visible || CGameModel_IsVisible(Obj_GetModel(e))) {
			Logic_ShowChar(node, names);
		}
	}
}

/* a ship: a line to its target in the attack and chase states, the list and the name, the AI states */
static void Logic_ShowShip(cEntity *e) {
	char state[0x100], sub[0x100];
	vec3_t p, lp;

	if (!e) {
		return;
	}
	Entity_GetPos(e, &p);
	cAIVehicle *ai = Entity_GetAI(e);
	cAIVehicle_GetStateName(ai, state);
	cAIVehicle_GetInternStateName(ai, sub);
	int st = cAIVehicle_GetState(ai);
	if ((st == AIV_STATE_ATTACK || st == AIV_STATE_CHASE) && AIMove_GetTargetEnt(ai)) {
		cEntity *t = AIMove_GetTargetEnt(ai);
		DebugUtils_DrawLine(&p, Entity_GetPos(t, &lp), 0xff0000);
	}
	g_FontTerminal8->color = 0xffffff;
	lp.x = p.x + 0.0f;
	lp.y = p.y + 600.0f;
	lp.z = p.z + 0.0f;
	const char *list = cEntity_GetListName(e);
	CFont_Printf3D(g_FontTerminal8, &lp, 3, "%s (%s)", list, e->name);
	lp.x = p.x + 0.0f;
	lp.y = p.y - 600.0f;
	lp.z = p.z + 0.0f;
	CFont_Printf3D(g_FontTerminal8, &lp, 3, "%s (%s)", state, sub);
}

/* every ship of the world with an AI and a visible model */
void Logic_ShowShips(void) {
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!Entity_IsType(e, TYEN_CAR) || !Entity_IsType(e, TYEN_CAR) || !Entity_GetAI(e)) {
			continue;
		}
		CGameModel *m = Obj_GetModel(e);
		if (m && CGameModel_IsVisible(m)) {
			Logic_ShowShip(e);
		}
	}
}

/* the ships of the list of that name */
static void Logic_ShowShipList(const char *name, byte visible) {
	cEntityListData *list = (cEntityListData *)cWorld_GetList(g_pWorld, name);

	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		if (visible && !CGameModel_IsVisible(Obj_GetModel(e))) {
			continue;
		}
		Logic_ShowShip(Logic_AsCar(e));
	}
}

static const char str_GEARRELAX[];
static const char str_GEARCHASE[];
static const char str_GEARATTACK[];
static const char str_GEARSTAYINPLACE[];
static const char str_GEARSCORT[];
static const char *LOGIC_GEARSTATES[5] = { (void *)str_GEARRELAX, (void *)str_GEARCHASE, (void *)str_GEARATTACK, (void *)str_GEARSTAYINPLACE, (void *)str_GEARSCORT };

static const char str_SENTINELRELAX[];
static const char str_SENTINELSEARCH[];
static const char str_SENTINELACUSE[];
static const char str_SENTINELINSPECT[];
static const char str_SENTINELSUSPECT[];
static const char str_SENTINELGOTOWATCH[];
static const char str_SENTINELATTACKNPC[];
static const char *LOGIC_SENTINELSTATES[7] = {
	(void *)str_SENTINELRELAX, (void *)str_SENTINELSEARCH, (void *)str_SENTINELACUSE, (void *)str_SENTINELINSPECT,
	(void *)str_SENTINELSUSPECT, (void *)str_SENTINELGOTOWATCH, (void *)str_SENTINELATTACKNPC,
};

static const char str_NURSERELAX[];
static const char str_NURSEALERT[];
static const char str_NURSEESCAPE[];
static const char str_NURSEATTACK[];
static const char *LOGIC_NURSESTATES[4] = { (void *)str_NURSERELAX, (void *)str_NURSEALERT, (void *)str_NURSEESCAPE, (void *)str_NURSEATTACK };

static const char str_GENCHARRELAX[];
static const char str_GENCHARALERT[];
static const char str_GENCHARACUSE[];
static const char str_GENCHARESCAPE[];
static const char str_GENCHARATTACK[];
static const char str_GENCHARCHASE[];
static const char str_GENCHARSTAY[];
static const char str_GENCHARSPECIALBEHAVIOUR1[];
static const char str_GENCHARSPECIALBEHAVIOUR2[];
static const char str_GENCHARSPECIALBEHAVIOUR3[];
static const char str_GENCHARGETIT[];
static const char *LOGIC_GENCHARSTATES[11] = {
	(void *)str_GENCHARRELAX, (void *)str_GENCHARALERT, (void *)str_GENCHARACUSE, (void *)str_GENCHARESCAPE,
	(void *)str_GENCHARATTACK, (void *)str_GENCHARCHASE, (void *)str_GENCHARSTAY, (void *)str_GENCHARSPECIALBEHAVIOUR1,
	(void *)str_GENCHARSPECIALBEHAVIOUR2, (void *)str_GENCHARSPECIALBEHAVIOUR3, (void *)str_GENCHARGETIT,
};

/* ShowAI: the ship lists of an outdoor map, else the character lists */
void Logic_ShowAI(byte visible) {
	static const char *const shipLists[] = { "DeathMatch", "CTFFriends", "CTFEnemies", "DominationEnemies",
		"DominationFriends", "Enemies", "Friends", "Traffic", "Mosquitos", "MadHunters", "EnemiesTraffic",
		"FightingTraffic", "NormalTraffic", "MediumTraffic", "HeavyTraffic" };

	g_FontTerminal8->align = 4;
	if (g_IsMapOutDoor == 1) {
		Logic_ShowShipList("OutCop", visible);
		for (size_t i = 0; i < sizeof(shipLists) / sizeof(shipLists[0]); i++) {
			Logic_ShowShipList(shipLists[i], visible);
		}
		return;
	}
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.aiCtrl.gears), LOGIC_GEARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.aiCtrl.sentinels), LOGIC_SENTINELSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.nurses.list), LOGIC_NURSESTATES, visible);
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.genChars.genChars), LOGIC_GENCHARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.genChars.police), LOGIC_GENCHARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.genChars.bishops), LOGIC_GENCHARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(g_pWorld->logic.genChars.messengers->list),
		LOGIC_GENCHARSTATES, visible);
}

/* ---- the variables ---- */
byte cBaseLocalLogic_VarF(cLocalLogic *base, const char *name, double *d, byte set) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (strcmp(name, "ActionRadius")) {
		return 0;
	}
	if (set) {
		Logic_SetActionRadius(self, (float)*d);
		return 1;
	}
	*d = (double)self->actionRadius;
	return 1;
}

byte cLocalLogic_VarVNone(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set) {
	(void)self;
	(void)name;
	(void)x;
	(void)y;
	(void)z;
	(void)set;
	return 0;
}

cLocalLogic *Logic_GetLocalLogic(cEntity *e) {
	if (!e || !Entity_IsType(e, TYEN_WALKCHAR)) {
		return NULL;
	}
	cLocalLogic *l = e->lLogic;
	if (l && !strcmp(LocalLogic_GetName(l), "BaseLogic")) {
		return l;
	}
	return NULL;
}

const char *cBaseLocalLogic_GetName(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	(void)self;
	return "BaseLogic";
}

const char *cLocalLogic_GetName(cLocalLogic *self) {
	(void)self;
	return str_Empty;
}

/* back to the state when idle in AI state 6 */
void cBaseLocalLogic_Refresh(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	cEntity *p = self->parent;

	if (self->idle || self->alwaysLock || self->busy || !Entity_IsType(p, TYEN_WALKCHAR)) {
		return;
	}
	cAIChar *ai = Entity_GetAI(p);
	if (AIChar_GetState(ai) == AIC_PATROL_ZONE) {
		self->idle = 1;
		Logic_SetState(self, self->state, self->target.target);
	}
}

byte cBaseLocalLogic_VarI(cLocalLogic *base, const char *name, int *i, byte set) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (!set && !strcmp(name, "Lock")) {
		*i = self->busy != 0;
		return 1;
	}
	if (!strcmp(name, "AlwaysLock")) {
		if (set) {
			self->alwaysLock = *i != 0;
			return 1;
		}
		*i = self->alwaysLock != 0;
		return 1;
	}
	if (!strcmp(name, "Idle")) {
		if (set) {
			Logic_SetIdle(self, *i != 0);
			return 1;
		}
		*i = self->idle || self->alwaysLock;
		return 1;
	}
	if (!strcmp(name, "State")) {
		if (set) {
			self->state = *i;
			return 1;
		}
		*i = self->state;
		return 1;
	}
	return 0;
}

/* the lock, the relax view of an idle character, the rage of the gears and the end of Dazed */
void cBaseLocalLogic_Update(cLocalLogic *base, float dt) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	byte busy = self->busy;
	cEntity *p = self->parent;

	if (Logic_UpdateBusy(self) != busy) {
		Logic_SetIdle(self, self->busy);
	}
	if ((self->idle || self->alwaysLock) && !self->busy && cActCtrl_IsActive(ChType_ActCtrl(p), "Relax")) {
		float ax = WChar(p)->view.x;
		if (__builtin_fabs((double)ax) > 0x1.41b2f769cf0e0p-3) {
			WChar(p)->control.vRot = WChar(p)->view.x / -5.0f;
		} else {
			WChar(p)->control.vRot = 0.0f;
		}
	}
	if (!*Nto1Node_TargetName(&WChar(p)->collide)) {
		self->rageTime = 0.0f;
	} else {
		self->rageTime = dt + self->rageTime;
		if (self->rageTime > g_GearRageMaxTime && !g_pWorld->alarm.on) {
			cEntity *t = World_FindEntity(Nto1Node_TargetName(&WChar(p)->collide));
			if (t && t != cWorld_GetUsrEntity(g_pWorld, 0) &&
				strcmp(ChType_ActClass(ChType_ActCtrl(t))->name, g_NotCleanableByGear)) {
				cEntity *victim = t;
				if (Entity_IsType(t, TYEN_WALKCHAR) &&
					(!strcmp(ChType_ActClass(ChType_ActCtrl(t))->name, "Gear") ||
					Obj(t)->radarShow)) {
					victim = p;
				}
				AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, victim, 0);
				AICtrl_SendSentinelToNPC(&g_pWorld->logic.aiCtrl, victim);
				self->rageTime = 0.0f;
			}
		}
	}
	if (!self->dazed) {
		self->dazedTime = 0.0f;
		return;
	}
	self->dazedTime = dt + self->dazedTime;
	if (!(self->dazedTime > g_LogicAwakeTime)) {
		return;
	}
	void *ctrl = ChType_ActCtrl(p);
	cActCtrl_SetDefLoop(ctrl, "Relax", 1);
	cActCtrl_Play(ctrl, "DazedEnd", 1, 1.0f);
	self->dazedTime = 0.0f;
}

void cBaseLocalLogic_OnShot(cLocalLogic *base, cEntity *attacker) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	Logic_OnHit(self, attacker);
}

/* the origin and its angles from the position and the angles of e */
void cBaseLocalLogic_SetOriginFrom(cLocalLogic *base, cEntity *e) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	vec3_t tmp;

	const vec3_t *p = Entity_GetPos(e, &tmp);
	Logic_SetOrigin(self, p->x, p->y, p->z);
	p = Obj_GetAng(e, &tmp);
	Logic_SetOriginAng(self, p->x, p->y, p->z);
}

/* a random true or false (arg unused) */
byte cBaseLocalLogic_Vf15(cLocalLogic *base, cEntity *arg) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	(void)self;
	(void)arg;
	double r = (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
	return (float)(r + r - 1.0) > 0.0f;
}

void cBaseLocalLogic_SetActionRadius(cLocalLogic *base, float radius) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (self->actionRadius != radius) {
		self->actionRadius = radius;
		Logic_Refresh(self);
	}
}

void cBaseLocalLogic_RestartState(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	Logic_SetState(self, self->state, self->target.target);
}

/* not idle: the state again; idle: the AI of the character in state 0 */
void cBaseLocalLogic_SetIdle(cLocalLogic *base, byte idle) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (!idle && !self->busy && !self->alwaysLock) {
		Logic_SetState(self, self->state, self->target.target);
		return;
	}
	CHECK(self->parent);
	CHECK(Entity_IsType(self->parent, TYEN_WALKCHAR));
	if (Entity_GetAI(self->parent)) {
		cAIChar *ai = Entity_GetAI(self->parent);
		AIChar_SetState(ai, AIC_STATIC);
	}
	self->idle = 1;
}

void cBaseLocalLogic_SetOrigin(cLocalLogic *base, float x, float y, float z) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (self->origin.x != x || self->origin.y != y || self->origin.z != z) {
		self->origin.x = x;
		self->origin.y = y;
		self->origin.z = z;
		Logic_Refresh(self);
	}
}

void cBaseLocalLogic_SetOriginAng(cLocalLogic *base, float x, float y, float z) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (self->originAng.x != x || self->originAng.y != y || self->originAng.z != z) {
		self->originAng.x = x;
		self->originAng.y = y;
		self->originAng.z = z;
		Logic_Refresh(self);
	}
}

/* the state and its target; 1 if the character runs it now (not idle), 0 also when nothing changed */
byte cBaseLocalLogic_SetState(cLocalLogic *base, int state, cEntity *target) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (state == self->state && target == self->target.target && !self->idle && !self->alwaysLock) {
		return 0;
	}
	if (target) {
		cEntity_AddLink(target, &self->target);
	} else {
		CHECK(self->target.data);
		Nto1Node_Unlink(&self->target);
	}
	self->state = state;
	self->idle = self->busy || self->alwaysLock;
	return !self->idle && !self->alwaysLock;
}

byte cBaseLocalLogic_VarS(cLocalLogic *base, const char *name, char **s, byte set) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (strcmp(name, "Target")) {
		return 0;
	}
	if (set) {
		Logic_SetState(self, self->state, World_FindEntity(*s));
		return 1;
	}
	cEntity *t = self->target.target;
	*s = t ? t->name : str_Empty;
	return 1;
}

byte cBaseLocalLogic_VarV(cLocalLogic *base, const char *name, double *x, double *y, double *z, byte set) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (!strcmp(name, "Origin")) {
		if (set) {
			Logic_SetOrigin(self, (float)*x, (float)*y, (float)*z);
			return 1;
		}
		*x = (double)self->origin.x;
		*y = (double)self->origin.y;
		*z = (double)self->origin.z;
		return 1;
	}
	if (!strcmp(name, "OriginAng")) {
		if (set) {
			Logic_SetOriginAng(self, (float)*x, (float)*y, (float)*z);
			return 1;
		}
		*x = (double)self->originAng.x;
		*y = (double)self->originAng.y;
		*z = (double)self->originAng.z;
		return 1;
	}
	return 0;
}

/* the slot CanTalk of a class that does only return 1 */
byte cLocalLogic_CanTalkTrue(cLocalLogic *self) {
	(void)self;
	return 1;
}

/* the slot FastEscape of a class that does only return 0 */
byte cLocalLogic_FastEscapeFalse(cLocalLogic *self) {
	(void)self;
	return 0;
}

/* the slot FindTarget of a class that does only return NULL */
cEntity *cLocalLogic_FindTargetNull(cLocalLogic *self) {
	(void)self;
	return NULL;
}

/* the slot Listens of a class that does only return 0 */
byte cLocalLogic_ListensFalse(cLocalLogic *self, cEntity *talker) {
	(void)self;
	(void)talker;
	return 0;
}

/* the slot Listens of a class that does only return 1 */
byte cLocalLogic_ListensTrue(cLocalLogic *self, cEntity *talker) {
	(void)self;
	(void)talker;
	return 1;
}

/* the slot OnAlarm of a class that does nothing there */
void cLocalLogic_OnAlarmNop(cLocalLogic *self, cEntity *e) {
	(void)self;
	(void)e;
}

/* the slot OnAttack of a class that does nothing there */
void cLocalLogic_OnAttackNop(cLocalLogic *self, cEntity *attacker, float value) {
	(void)self;
	(void)attacker;
	(void)value;
}

/* the slot OnHammer of a class that does nothing there */
void cLocalLogic_OnHammerNop(cLocalLogic *self, cEntity *nurse) {
	(void)self;
	(void)nurse;
}

/* the slot OnHit of a class that does nothing there */
void cLocalLogic_OnHitNop(cLocalLogic *self, cEntity *attacker) {
	(void)self;
	(void)attacker;
}

/* the slot OnSeen of a class that does nothing there */
void cLocalLogic_OnSeenNop(cLocalLogic *self, cEntity *e) {
	(void)self;
	(void)e;
}

/* the slot SetOriginFrom of a class that does nothing there */
void cLocalLogic_SetOriginFromNop(cLocalLogic *self, cEntity *e) {
	(void)self;
	(void)e;
}

/* the slot SetState of a class that does only return 1 */
byte cLocalLogic_SetStateTrue(cLocalLogic *self, int state, cEntity *target) {
	(void)self;
	(void)state;
	(void)target;
	return 1;
}

/* the slot TargetDone of a class that does only return 0 */
byte cLocalLogic_TargetDoneFalse(cLocalLogic *self) {
	(void)self;
	return 0;
}

/* the slot TargetDone of a class that does only return 1 */
byte cLocalLogic_TargetDoneTrue(cLocalLogic *self) {
	(void)self;
	return 1;
}

/* the slot Update of a class that does nothing there */
void cLocalLogic_UpdateNop(cLocalLogic *self, float dt) {
	(void)self;
	(void)dt;
}

/* the slot VarF of a class that does only return 0 */
byte cLocalLogic_VarFFalse(cLocalLogic *self, const char *name, double *d, byte set) {
	(void)self;
	(void)name;
	(void)d;
	(void)set;
	return 0;
}

/* the slot VarI of a class that does only return 0 */
byte cLocalLogic_VarIFalse(cLocalLogic *self, const char *name, int *i, byte set) {
	(void)self;
	(void)name;
	(void)i;
	(void)set;
	return 0;
}

/* the slot VarS of a class that does only return 0 */
byte cLocalLogic_VarSFalse(cLocalLogic *self, const char *name, char **s, byte set) {
	(void)self;
	(void)name;
	(void)s;
	(void)set;
	return 0;
}

/* the slot AcceptsArrow of a class that does only return 1 */
byte cLocalLogic_AcceptsArrowTrue(cLocalLogic *self) {
	(void)self;
	return 1;
}

/* the slot BreakWork of a class that does nothing there */
void cLocalLogic_BreakWorkNop(cLocalLogic *self, byte restart) {
	(void)self;
	(void)restart;
}

/* the slot Work of a class that does nothing there */
void cLocalLogic_WorkNop(cLocalLogic *self, float dt) {
	(void)self;
	(void)dt;
}

/* the vtable of cLocalLogic */
static const cLocalLogicVtbl cLocalLogic_vftable = {
	.Free = cLocalLogic_Free,
	.GetName = cLocalLogic_GetName,
	.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
	.VarI = cLocalLogic_VarIFalse,
	.VarF = cLocalLogic_VarFFalse,
	.VarS = cLocalLogic_VarSFalse,
	.VarV = cLocalLogic_VarVNone,
};

/* the vtable of cBaseLocalLogic */
static const cBaseLocalLogicVtbl cBaseLocalLogic_vftable = {
	.base.Free = cBaseLocalLogic_Free,
	.base.GetName = cBaseLocalLogic_GetName,
	.base.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
	.base.VarI = cBaseLocalLogic_VarI,
	.base.VarF = cBaseLocalLogic_VarF,
	.base.VarS = cBaseLocalLogic_VarS,
	.base.VarV = cBaseLocalLogic_VarV,
	.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.Refresh = cBaseLocalLogic_Refresh,
	.Listens = (void *)crt_purecall,
	.OnHit = (void *)crt_purecall,
	.OnSeen = (void *)crt_purecall,
	.OnAttack = (void *)crt_purecall,
	.OnShot = cBaseLocalLogic_OnShot,
	.OnHammer = (void *)crt_purecall,
	.GivesMoney = cBaseLocalLogic_Vf15,
	.OnAlarm = (void *)crt_purecall,
	.Update = cBaseLocalLogic_Update,
	.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.SetState = cBaseLocalLogic_SetState,
	.SetIdle = cBaseLocalLogic_SetIdle,
	.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.SetOrigin = cBaseLocalLogic_SetOrigin,
	.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.RestartState = cBaseLocalLogic_RestartState,
};

static const char str_GEARRELAX[] = "GEAR_RELAX";
static const char str_GEARCHASE[] = "GEAR_CHASE";
static const char str_GEARATTACK[] = "GEAR_ATTACK";
static const char str_GEARSTAYINPLACE[] = "GEAR_STAY_IN_PLACE";
static const char str_GEARSCORT[] = "GEAR_SCORT";
static const char str_SENTINELRELAX[] = "SENTINEL_RELAX";
static const char str_SENTINELSEARCH[] = "SENTINEL_SEARCH";
static const char str_SENTINELACUSE[] = "SENTINEL_ACUSE";
static const char str_SENTINELINSPECT[] = "SENTINEL_INSPECT";
static const char str_SENTINELSUSPECT[] = "SENTINEL_SUSPECT";
static const char str_SENTINELGOTOWATCH[] = "SENTINEL_GOTO_WATCH";
static const char str_SENTINELATTACKNPC[] = "SENTINEL_ATTACK_NPC";
static const char str_NURSERELAX[] = "NURSE_RELAX";
static const char str_NURSEALERT[] = "NURSE_ALERT";
static const char str_NURSEESCAPE[] = "NURSE_ESCAPE";
static const char str_NURSEATTACK[] = "NURSE_ATTACK";
static const char str_GENCHARRELAX[] = "GENCHAR_RELAX";
static const char str_GENCHARALERT[] = "GENCHAR_ALERT";
static const char str_GENCHARACUSE[] = "GENCHAR_ACUSE";
static const char str_GENCHARESCAPE[] = "GENCHAR_ESCAPE";
static const char str_GENCHARATTACK[] = "GENCHAR_ATTACK";
static const char str_GENCHARCHASE[] = "GENCHAR_CHASE";
static const char str_GENCHARSTAY[] = "GENCHAR_STAY";
static const char str_GENCHARSPECIALBEHAVIOUR1[] = "GENCHAR_SPECIAL_BEHAVIOUR_1";
static const char str_GENCHARSPECIALBEHAVIOUR2[] = "GENCHAR_SPECIAL_BEHAVIOUR_2";
static const char str_GENCHARSPECIALBEHAVIOUR3[] = "GENCHAR_SPECIAL_BEHAVIOUR_3";
static const char str_GENCHARGETIT[] = "GENCHAR_GET_IT";
static cGVar GVar_LogicAwakeTime;
/* engine variables of the logics */
static cGVar GVar_DefActionRadius;
static cGVar GVar_GearRageMaxTime;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_LogicAwakeTime, &GVar_LogicAwakeTime, "LogicAwakeTime", 0, "Max time dazed for the logical managed characters.", 90.0f)
INIT_GVAR_FLOAT(Init_GVar_GearRageMaxTime, &GVar_GearRageMaxTime, "GearRageMaxTime", 0, "Time to get te rage and attack the stupid guys!", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_DefActionRadius, &GVar_DefActionRadius, "DefActionRadius", 0, "Default action radius for characters logic", 5000.0f)
