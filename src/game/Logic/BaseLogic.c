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
#define LOGIC_GEARSTATES ((const char **)Game_Ptr(0x428b38))
#define LOGIC_SENTINELSTATES ((const char **)Game_Ptr(0x428b4c))
#define LOGIC_NURSESTATES ((const char **)Game_Ptr(0x428b68))
#define LOGIC_GENCHARSTATES ((const char **)Game_Ptr(0x428b78))

cBaseLocalLogic *__thiscall cBaseLocalLogic_ctor(cBaseLocalLogic *self, cEntity *parent) {
	vec3_t tmp, ang, out;

	self->parent = parent;
	self->vftable = Game_Ptr(CBASELOCALLOGIC_VFTABLE);
	self->target.target = NULL;
	self->target.data = NULL;
	self->target.next = &self->target;
	self->target.prev = &self->target;
	self->origin = *parent->vftable->GetPos(parent, &tmp);
	CHECK(!strcmp(parent->vftable->GetClassName(parent), TYEN_WALKCHAR));
	ang = *WChar_Ang(parent);
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

void __fastcall cBaseLocalLogic_dtor(cBaseLocalLogic *self) {
	self->vftable = Game_Ptr(CBASELOCALLOGIC_VFTABLE);
	Nto1Node_Done(&self->target);
	self->vftable = Game_Ptr(CLOCALLOGIC_VFTABLE);
}

void __fastcall cLocalLogic_dtor(cLocalLogic *self) {
	self->vftable = Game_Ptr(CLOCALLOGIC_VFTABLE);
}

cBaseLocalLogic *__thiscall cBaseLocalLogic_Delete(cBaseLocalLogic *self, byte flags) {
	cBaseLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cLocalLogic *__thiscall cLocalLogic_Delete(cLocalLogic *self, byte flags) {
	self->vftable = Game_Ptr(CLOCALLOGIC_VFTABLE);
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
byte __thiscall cBaseLocalLogic_UpdateBusy(cBaseLocalLogic *self) {
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
	byte goal[0xc8];
	vec3_t p, lp, tmp;
	CFont *font;

	cEntity *e = node->data;
	cBaseLocalLogic *l = e->lLogic;
	e->vftable->GetPos(e, &p);
	int st = l->state;
	void *ai = e->vftable->GetAI(e);
	Logic_CharAIVtbl(ai)->GetStateName(ai, state);
	Logic_CharAIVtbl(ai)->GetSubStateName(ai, sub);
	cEntity *t = l->target.target;
	if (t) {
		DebugUtils_DrawLine(&p, t->vftable->GetPos(t, &tmp), 0xff0000);
	} else {
		ai = e->vftable->GetAI(e);
		if (ai && Logic_CharAIVtbl(ai)->GetState(ai) == 4) {
			Logic_CharAIVtbl(ai)->GetGoal(ai, goal);
			DebugUtils_DrawLine(&p, (vec3_t *)(goal + 8), 0xff0000);
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
	if ((st == AIV_STATE_ATTACK || st == AIV_STATE_CHASE) && Logic_AIVtbl(ai)->GetTarget(ai)) {
		cEntity *t = Logic_AIVtbl(ai)->GetTarget(ai);
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
	Logic_ShowChars(Logic_ListFirst(World_CharList(WORLD_GEARS)), LOGIC_GEARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(World_CharList(WORLD_SENTINELS)), LOGIC_SENTINELSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(World_CharList(WORLD_NURSES)), LOGIC_NURSESTATES, visible);
	Logic_ShowChars(Logic_ListFirst(World_CharList(WORLD_GENCHARS1)), LOGIC_GENCHARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(World_CharList(WORLD_GENCHARS2)), LOGIC_GENCHARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(World_CharList(WORLD_GENCHARS3)), LOGIC_GENCHARSTATES, visible);
	Logic_ShowChars(Logic_ListFirst(*(cEntityListData **)((byte *)World_CharList(WORLD_GENCHARS4) + 0x10)),
		LOGIC_GENCHARSTATES, visible);
}

/* ---- the variables ---- */
byte __thiscall cBaseLocalLogic_VarF(cBaseLocalLogic *self, const char *name, double *d, byte set) {
	if (strcmp(name, "ActionRadius")) {
		return 0;
	}
	if (set) {
		self->vftable->SetActionRadius(self, (float)*d);
		return 1;
	}
	*d = (double)self->actionRadius;
	return 1;
}

byte __thiscall cLocalLogic_VarNone(cLocalLogic *self, const char *name, void *v, byte set) {
	(void)self;
	(void)name;
	(void)v;
	(void)set;
	return 0;
}

byte __thiscall cLocalLogic_VarVNone(cLocalLogic *self, const char *name, double *x, double *y, double *z, byte set) {
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

const char *__thiscall cBaseLocalLogic_GetName(cBaseLocalLogic *self) {
	(void)self;
	return Game_Ptr(0x3bd4a0);
}

const char *__thiscall cLocalLogic_GetName(cLocalLogic *self) {
	(void)self;
	return LOGIC_EMPTY;
}

/* back to the state when idle in AI state 6 */
void __thiscall cBaseLocalLogic_Refresh(cBaseLocalLogic *self) {
	cEntity *p = self->parent;

	if (self->idle || self->alwaysLock || self->busy || !p->vftable->IsType(p, TYEN_WALKCHAR)) {
		return;
	}
	void *ai = p->vftable->GetAI(p);
	if (Logic_CharAIVtbl(ai)->GetState(ai) == 6) {
		self->idle = 1;
		self->vftable->SetState(self, self->state, self->target.target);
	}
}

byte __thiscall cBaseLocalLogic_VarI(cBaseLocalLogic *self, const char *name, int *i, byte set) {
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
			self->vftable->SetIdle(self, *i != 0);
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
void __thiscall cBaseLocalLogic_Update(cBaseLocalLogic *self, float dt) {
	byte busy = self->busy;
	cEntity *p = self->parent;

	if (self->vftable->UpdateBusy(self) != busy) {
		self->vftable->SetIdle(self, self->busy);
	}
	if ((self->idle || self->alwaysLock) && !self->busy && cActCtrl_IsActive(ChType_ActCtrl(p), "Relax")) {
		float ax = WChar_Ang(p)->x;
		if (__builtin_fabs((double)ax) > 0x1.41b2f769cf0e0p-3) {
			WChar_ControlTurn(p) = WChar_Ang(p)->x / -5.0f;
		} else {
			WChar_ControlTurn(p) = 0.0f;
		}
	}
	if (!*Nto1Node_TargetName(WChar_Link(p))) {
		self->rageTime = 0.0f;
	} else {
		self->rageTime = dt + self->rageTime;
		if (self->rageTime > g_GearRageMaxTime && !g_pWorld->alarmOn) {
			cEntity *t = World_FindEntity(Nto1Node_TargetName(WChar_Link(p)));
			if (t && t != cWorld_GetUsrEntity(g_pWorld, 0) &&
				strcmp(*(char **)(ChType_ActClass(ChType_ActCtrl(t)) + 4), g_NotCleanableByGear)) {
				cEntity *victim = t;
				if (t->vftable->IsType(t, TYEN_WALKCHAR) &&
					(!strcmp(*(char **)(ChType_ActClass(ChType_ActCtrl(t)) + 4), "Gear") ||
					((cObjEntity *)t)->radarShow)) {
					victim = p;
				}
				AICtrl_GearAttack(g_pWorld->aiCtrl, victim, 0);
				AICtrl_SendSentinelToNPC(g_pWorld->aiCtrl, victim);
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

void __thiscall cBaseLocalLogic_OnShot(cBaseLocalLogic *self, cEntity *attacker) {
	self->vftable->OnHit(self, attacker);
}

/* the origin and its angles from the position and the angles of e */
void __thiscall cBaseLocalLogic_SetOriginFrom(cBaseLocalLogic *self, cEntity *e) {
	vec3_t tmp;

	const vec3_t *p = e->vftable->GetPos(e, &tmp);
	self->vftable->SetOrigin(self, p->x, p->y, p->z);
	p = Obj_Vtbl(e)->GetAng(e, &tmp);
	self->vftable->SetOriginAng(self, p->x, p->y, p->z);
}

/* a random true or false (arg unused) */
int __thiscall cBaseLocalLogic_Vf15(cBaseLocalLogic *self, int arg) {
	(void)self;
	(void)arg;
	double r = (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
	return (float)(r + r - 1.0) > 0.0f;
}

void __thiscall cBaseLocalLogic_SetActionRadius(cBaseLocalLogic *self, float radius) {
	if (self->actionRadius != radius) {
		self->actionRadius = radius;
		self->vftable->Refresh(self);
	}
}

void __thiscall cBaseLocalLogic_RestartState(cBaseLocalLogic *self) {
	self->vftable->SetState(self, self->state, self->target.target);
}

/* not idle: the state again; idle: the AI of the character in state 0 */
void __thiscall cBaseLocalLogic_SetIdle(cBaseLocalLogic *self, byte idle) {
	if (!idle && !self->busy && !self->alwaysLock) {
		self->vftable->SetState(self, self->state, self->target.target);
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

void __thiscall cBaseLocalLogic_SetOrigin(cBaseLocalLogic *self, float x, float y, float z) {
	if (self->origin.x != x || self->origin.y != y || self->origin.z != z) {
		self->origin.x = x;
		self->origin.y = y;
		self->origin.z = z;
		self->vftable->Refresh(self);
	}
}

void __thiscall cBaseLocalLogic_SetOriginAng(cBaseLocalLogic *self, float x, float y, float z) {
	if (self->originAng.x != x || self->originAng.y != y || self->originAng.z != z) {
		self->originAng.x = x;
		self->originAng.y = y;
		self->originAng.z = z;
		self->vftable->Refresh(self);
	}
}

/* the state and its target; 1 if the character runs it now (not idle), 0 also when nothing changed */
int __thiscall cBaseLocalLogic_SetState(cBaseLocalLogic *self, int state, cEntity *target) {
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

byte __thiscall cBaseLocalLogic_VarS(cBaseLocalLogic *self, const char *name, char **s, byte set) {
	if (strcmp(name, "Target")) {
		return 0;
	}
	if (set) {
		self->vftable->SetState(self, self->state, World_FindEntity(*s));
		return 1;
	}
	cEntity *t = self->target.target;
	*s = t ? t->name : LOGIC_EMPTY;
	return 1;
}

byte __thiscall cBaseLocalLogic_VarV(cBaseLocalLogic *self, const char *name, double *x, double *y, double *z,
	byte set) {
	if (!strcmp(name, "Origin")) {
		if (set) {
			self->vftable->SetOrigin(self, (float)*x, (float)*y, (float)*z);
			return 1;
		}
		*x = (double)self->origin.x;
		*y = (double)self->origin.y;
		*z = (double)self->origin.z;
		return 1;
	}
	if (!strcmp(name, "OriginAng")) {
		if (set) {
			self->vftable->SetOriginAng(self, (float)*x, (float)*y, (float)*z);
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
byte __thiscall Logic_True(void *self) {
	(void)self;
	return 1;
}

byte __thiscall Logic_False(void *self) {
	(void)self;
	return 0;
}

/* vf30 of the logics that search no target */
cEntity *__thiscall Logic_NoTarget(void *self) {
	(void)self;
	return NULL;
}

void __thiscall Logic_Nop1(void *self, int arg) {
	(void)self;
	(void)arg;
}

byte __thiscall Logic_False1(void *self, int arg) {
	(void)self;
	(void)arg;
	return 0;
}
