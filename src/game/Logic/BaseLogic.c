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

/* the state names of the debug texts (pointer tables of the exe) */
extern const char * LOGIC_GEARSTATES[5];
extern const char * LOGIC_SENTINELSTATES[7];
extern const char * LOGIC_NURSESTATES[4];
extern const char * LOGIC_GENCHARSTATES[11];

cBaseLocalLogic *cBaseLocalLogic_ctor(cBaseLocalLogic *self, cEntity *parent) {
	vec3_t tmp, ang, out;

	self->parent = parent;
	self->vftable = CBASELOCALLOGIC_VFTABLE;
	self->target.target = NULL;
	self->target.data = NULL;
	self->target.next = &self->target;
	self->target.prev = &self->target;
	self->origin = *parent->vftable->GetPos(parent, &tmp);
	CHECK(!strcmp(parent->vftable->GetClassName(parent), TYEN_WALKCHAR));
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

cLocalLogic *cBaseLocalLogic_Delete(cLocalLogic *base, byte flags) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	cBaseLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

cLocalLogic *cLocalLogic_Delete(cLocalLogic *self, byte flags) {
	self->vftable = CLOCALLOGIC_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
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
	const vec3_t *s = Obj_Vtbl(viewer)->GetSndAng(viewer, &sa);
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
		return !Engine_TestLine(lineFrom, viewer->vftable->GetPos(viewer, &tmp), NULL, NULL);
	}
	return 1;
}

byte Logic_InView(cEntity *viewer, const vec3_t *pos, float dist, float fovY, float fovX, byte testLine) {
	vec3_t tmp;

	return Logic_ViewTest(viewer, pos, viewer->vftable->GetPos(viewer, &tmp), dist, fovY, fovX, testLine, pos);
}

cEntity *Logic_FindInView(cEntity *viewer, cEntityListData *list, float dist, float fovY, float fovX, byte testLine,
	byte (*filter)(cEntity *)) {
	vec3_t t1, t2;

	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *c = n->data;
		if (viewer == c || !c->vftable->IsType(c, TYEN_WALKCHAR)) {
			continue;
		}
		const vec3_t *from = c->vftable->GetPos(c, &t1);
		const vec3_t *vp = viewer->vftable->GetPos(viewer, &t2);
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
void Logic_ShowChar(Nto1Node *node, const char **names) {
	char state[0x100], sub[0x100];
	cAICharSaved goal;
	vec3_t p, lp, tmp;
	CFont *font;

	cEntity *e = node->data;
	cBaseLocalLogic *l = e->lLogic;
	e->vftable->GetPos(e, &p);
	int st = l->state;
	cAIChar *ai = e->vftable->GetAI(e);
	ai->vftable->GetStateName(ai, state);
	ai->vftable->GetInternStateName(ai, sub);
	cEntity *t = l->target.target;
	if (t) {
		DebugUtils_DrawLine(&p, t->vftable->GetPos(t, &tmp), 0xff0000);
	} else {
		ai = e->vftable->GetAI(e);
		if (ai && ai->vftable->GetState(ai) == 4) {
			ai->vftable->SaveTo(ai, &goal);
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
void Logic_ShowChars(Nto1Node *node, const char **names, byte visible) {
	for (; node; node = Logic_ListNext(node)) {
		cEntity *e = node->data;
		if (!visible || CGameModel_IsVisible(Obj_Vtbl(e)->GetModel(e))) {
			Logic_ShowChar(node, names);
		}
	}
}

/* a ship: a line to its target in the attack and chase states, the list and the name, the AI states */
void Logic_ShowShip(cEntity *e) {
	char state[0x100], sub[0x100];
	vec3_t p, lp;

	if (!e) {
		return;
	}
	e->vftable->GetPos(e, &p);
	cAIVehicle *ai = e->vftable->GetAI(e);
	cAIVehicle_GetStateName(ai, state);
	cAIVehicle_GetInternStateName(ai, sub);
	int st = cAIVehicle_GetState(ai);
	if ((st == AIV_STATE_ATTACK || st == AIV_STATE_CHASE) && ai->vftable->GetTargetEnt(ai)) {
		cEntity *t = ai->vftable->GetTargetEnt(ai);
		DebugUtils_DrawLine(&p, t->vftable->GetPos(t, &lp), 0xff0000);
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
		if (!e->vftable->IsType(e, TYEN_CAR) || !e->vftable->IsType(e, TYEN_CAR) || !e->vftable->GetAI(e)) {
			continue;
		}
		CGameModel *m = Obj_Vtbl(e)->GetModel(e);
		if (m && CGameModel_IsVisible(m)) {
			Logic_ShowShip(e);
		}
	}
}

/* the ships of the list of that name */
void Logic_ShowShipList(const char *name, byte visible) {
	cEntityListData *list = (cEntityListData *)cWorld_GetList(g_pWorld, name);

	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		if (visible && !CGameModel_IsVisible(Obj_Vtbl(e)->GetModel(e))) {
			continue;
		}
		Logic_ShowShip(Logic_AsCar(e));
	}
}

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
		self->vftable->SetActionRadius(&self->logic, (float)*d);
		return 1;
	}
	*d = (double)self->actionRadius;
	return 1;
}

byte cLocalLogic_VarNone(cLocalLogic *self, const char *name, void *v, byte set) {
	(void)self;
	(void)name;
	(void)v;
	(void)set;
	return 0;
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
	if (!e || !e->vftable->IsType(e, TYEN_WALKCHAR)) {
		return NULL;
	}
	cLocalLogic *l = e->lLogic;
	if (l && !strcmp(l->vftable->GetName(l), "BaseLogic")) {
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
	return LOGIC_EMPTY;
}

/* back to the state when idle in AI state 6 */
void cBaseLocalLogic_Refresh(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	cEntity *p = self->parent;

	if (self->idle || self->alwaysLock || self->busy || !p->vftable->IsType(p, TYEN_WALKCHAR)) {
		return;
	}
	cAIChar *ai = p->vftable->GetAI(p);
	if (ai->vftable->GetState(ai) == 6) {
		self->idle = 1;
		self->vftable->SetState(&self->logic, self->state, self->target.target);
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
			self->vftable->SetIdle(&self->logic, *i != 0);
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

	if (self->vftable->UpdateBusy(&self->logic) != busy) {
		self->vftable->SetIdle(&self->logic, self->busy);
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
				if (t->vftable->IsType(t, TYEN_WALKCHAR) &&
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
	self->vftable->OnHit(&self->logic, attacker);
}

/* the origin and its angles from the position and the angles of e */
void cBaseLocalLogic_SetOriginFrom(cLocalLogic *base, cEntity *e) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	vec3_t tmp;

	const vec3_t *p = e->vftable->GetPos(e, &tmp);
	self->vftable->SetOrigin(&self->logic, p->x, p->y, p->z);
	p = Obj_Vtbl(e)->GetAng(e, &tmp);
	self->vftable->SetOriginAng(&self->logic, p->x, p->y, p->z);
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
		self->vftable->Refresh(&self->logic);
	}
}

void cBaseLocalLogic_RestartState(cLocalLogic *base) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	self->vftable->SetState(&self->logic, self->state, self->target.target);
}

/* not idle: the state again; idle: the AI of the character in state 0 */
void cBaseLocalLogic_SetIdle(cLocalLogic *base, byte idle) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (!idle && !self->busy && !self->alwaysLock) {
		self->vftable->SetState(&self->logic, self->state, self->target.target);
		return;
	}
	CHECK(self->parent);
	CHECK(self->parent->vftable->IsType(self->parent, TYEN_WALKCHAR));
	if (self->parent->vftable->GetAI(self->parent)) {
		cAIChar *ai = self->parent->vftable->GetAI(self->parent);
		ai->vftable->SetState(ai, 0);
	}
	self->idle = 1;
}

void cBaseLocalLogic_SetOrigin(cLocalLogic *base, float x, float y, float z) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (self->origin.x != x || self->origin.y != y || self->origin.z != z) {
		self->origin.x = x;
		self->origin.y = y;
		self->origin.z = z;
		self->vftable->Refresh(&self->logic);
	}
}

void cBaseLocalLogic_SetOriginAng(cLocalLogic *base, float x, float y, float z) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (self->originAng.x != x || self->originAng.y != y || self->originAng.z != z) {
		self->originAng.x = x;
		self->originAng.y = y;
		self->originAng.z = z;
		self->vftable->Refresh(&self->logic);
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
		self->vftable->SetState(&self->logic, self->state, World_FindEntity(*s));
		return 1;
	}
	cEntity *t = self->target.target;
	*s = t ? t->name : LOGIC_EMPTY;
	return 1;
}

byte cBaseLocalLogic_VarV(cLocalLogic *base, const char *name, double *x, double *y, double *z, byte set) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	if (!strcmp(name, "Origin")) {
		if (set) {
			self->vftable->SetOrigin(&self->logic, (float)*x, (float)*y, (float)*z);
			return 1;
		}
		*x = (double)self->origin.x;
		*y = (double)self->origin.y;
		*z = (double)self->origin.z;
		return 1;
	}
	if (!strcmp(name, "OriginAng")) {
		if (set) {
			self->vftable->SetOriginAng(&self->logic, (float)*x, (float)*y, (float)*z);
			return 1;
		}
		*x = (double)self->originAng.x;
		*y = (double)self->originAng.y;
		*z = (double)self->originAng.z;
		return 1;
	}
	return 0;
}

/* ---- the small slots shared by the logic classes ---- */
byte Logic_True(void *self) {
	(void)self;
	return 1;
}

byte Logic_False(void *self) {
	(void)self;
	return 0;
}

/* vf30 of the logics that search no target */
cEntity *Logic_NoTarget(void *self) {
	(void)self;
	return NULL;
}

void Logic_Nop1(void *self, int arg) {
	(void)self;
	(void)arg;
}

byte Logic_False1(void *self, int arg) {
	(void)self;
	(void)arg;
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

/* the slot Vf02 of a class that does only return 1 */
byte cLocalLogic_Vf02True(cLocalLogic *self) {
	(void)self;
	return 1;
}

/* the slot Vf33 of a class that does nothing there */
void cLocalLogic_Vf33Nop(cLocalLogic *self, byte restart) {
	(void)self;
	(void)restart;
}

/* the slot Vf42 of a class that does nothing there */
void cLocalLogic_Vf42Nop(cLocalLogic *self, int arg) {
	(void)self;
	(void)arg;
}

/* the slot Vf43 of a class that does nothing there */
void cLocalLogic_Vf43Nop(cLocalLogic *self, int arg) {
	(void)self;
	(void)arg;
}

/* the slot Vf44 of a class that does nothing there */
void cLocalLogic_Vf44Nop(cLocalLogic *self, int arg) {
	(void)self;
	(void)arg;
}

/* the slot Work of a class that does nothing there */
void cLocalLogic_WorkNop(cLocalLogic *self, float dt) {
	(void)self;
	(void)dt;
}

/* the vtable of cLocalLogic */
const cLocalLogicVtbl cLocalLogic_vftable = {
	.Delete = cLocalLogic_Delete,
	.GetName = cLocalLogic_GetName,
	.Vf02 = cLocalLogic_Vf02True,
	.VarI = cLocalLogic_VarIFalse,
	.VarF = cLocalLogic_VarFFalse,
	.VarS = cLocalLogic_VarSFalse,
	.VarV = cLocalLogic_VarVNone,
};

/* the vtable of cBaseLocalLogic */
const cBaseLocalLogicVtbl cBaseLocalLogic_vftable = {
	.base.Delete = cBaseLocalLogic_Delete,
	.base.GetName = cBaseLocalLogic_GetName,
	.base.Vf02 = cLocalLogic_Vf02True,
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
