/*
 * The AI of the walking characters, original source AI/AIChar.cpp (include/scrap/aichar.h, the struct and the vtable
 * in include/scrap/ai.h): construction, the update and its slots (pause, repositioning, the think of the states),
 * the change of the external and of the internal state, the events of the move, the talks of two characters and the
 * Nto1Links.h copies the AI uses. Every call the original makes through the vtable goes through it here too, the
 * subclasses (cAISentinel, ...) replace slots.
 */
#include <float.h>
#include <string.h>

#include "scrap/aichar.h"
#include "scrap/aivehicle.h"
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/nodesworld2d.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"

#define g_AICRadiusInRoute GAME_GLOBAL_IN(float, GVar_RADIUS_OBJECTIVE_IN_ROUTE, 16, 0x444d60)	/* RADIUS_OBJECTIVE_IN_ROUTE */
#define g_AICRandScale GAME_GLOBAL(double, g_FxRandScale, 0x3a3d60)	/* 1/32767 */
#define AIC_ANG45 ((double)0.785398185f)		/* pi / 4 as a float (rva 0x3a5108) */
#define AIC_ANG90 1.5707963267948966			/* pi / 2 (rva 0x3a5110) */
#define AIC_LOST_TIME 15.0f				/* LOST_PATH gives up (rva 0x3a7728) */

/* ---- helpers ---- */

static inline cEntity *AIC_Target(const cAIChar *self) {
	return self->target.target;
}

static inline cSlotController *AIC_Slot(byte *raw) {
	return (cSlotController *)raw;
}

static inline const vec3_t *AIC_Pos(cEntity *e, vec3_t *buf) {
	return e->vftable->GetPos(e, buf);
}

static inline float AIC_Radius(cEntity *e) {
	return Ent_Primitive(e)->radius;
}

static inline cAIChar *AIC_AI(cEntity *e) {
	return e->vftable->GetAI(e);
}

static inline void *AIC_ActCtrl(cEntity *e) {
	return WithLife_Vtbl(e)->GetActCtrl(e);
}

/* the head node of an entity with life the target links to */
static inline Nto1Node *AIC_Head(cEntity *e) {
	return (Nto1Node *)((byte *)e + 0x254);
}

/* the slots of the move control without a name in aicharcontrol.h */
static inline void *AIC_CtlSlot(cAICharControl *c, int ofs) {
	return ((void *const *)c->vftable)[ofs / 4];
}

static inline byte AIC_CtlByte(cAICharControl *c, int ofs) {
	return ((byte(__thiscall *)(cAICharControl *))AIC_CtlSlot(c, ofs))(c);
}

/* 0x34 the end of the move, 0x44 and 0x48 the two avoid points (out returned) */
static inline vec3_t *AIC_CtlVec(cAICharControl *c, int ofs, vec3_t *out) {
	return ((vec3_t *(__thiscall *)(cAICharControl *, vec3_t *))AIC_CtlSlot(c, ofs))(c, out);
}

/* rand() of the exe in [-1, 1] */
static inline float AIC_RandSigned(void) {
	double r = (double)(crt_rand() & 0x7fff) * g_AICRandScale;

	return (float)(r + r - 1.0);
}

/* the forward of the view of the character: (0, 0, -1) turned by its view angles */
static vec3_t AIC_ViewDir(cEntity *e) {
	vec3_t in = {0.0f, 0.0f, -1.0f};
	vec3_t ang = WChar(e)->view;
	vec3_t out;

	Vec3_Rotate(&out, &in, &ang);
	return out;
}

/* the target node linked to e (out of the ring of the old target first) */
static void AIC_LinkTarget(cAIChar *self, cEntity *e) {
	if (AIC_Target(self) != e) {
		if (AIC_Target(self)) {
			Nto1Node_UnlinkChecked(&self->target);
		}
		Nto1Head_Link(AIC_Head(e), &self->target);
	}
}

/* the name of e into a target field (the original copies it unbounded) */
static void AIC_CopyName(char *dst, int size, const char *src) {
	while (--size > 0 && *src) {
		*dst++ = *src++;
	}
	*dst = 0;
}

static inline void AIC_ResumeRoute(cAIChar *self) {
	self->vftable->SetStateIntern(self, AIC_IN_ROUTE, (double)self->speed, (int)self->pauses);
}

static inline void AIC_ResumePatrol(cAIChar *self) {
	self->vftable->SetStateIntern(self, AIC_PATROL_ZONE, self->center, (double)self->radius, (double)self->speed,
		(int)self->pauses);
}

/* the actions a talk looks at, by index: 1..5 the walk and relax ones, 4 and 5 the relax ones, 8..10 the talks */
static const char *const s_TalkActs[] = {
	"Turn", "Walk", "WalkInit", "WalkEnd", "Relax0", "Relax1", "Crazy", "Talk", "Talk0", "Talk1", "Talk2"
};

/* the action of layer 0 of e is one of n names of s_TalkActs from first (one lookup per name, as the original) */
static int AIC_ActionIs(cEntity *e, int first, int n) {
	for (int i = first; i < first + n; i++) {
		if (!strcmp(cActCtrl_GetActionName(AIC_ActCtrl(e), 0), s_TalkActs[i])) {
			return 1;
		}
	}
	return 0;
}

static inline void AIC_Play(cEntity *e, const char *action) {
	cActCtrl_Play(AIC_ActCtrl(e), action, 0, 1.0f);
}

/* ---- construction ---- */

void *__thiscall cAIChar_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;
	void *m;

	cAIMove_ctor(mem, e);
	self->vftable = CAICHAR_VFTABLE;
	cSlotController_ctor(AIC_Slot(self->slotAction), 0.2f);
	cSlotController_ctor(AIC_Slot(self->slotPause), 1.0f);
	cSlotController_ctor(AIC_Slot(self->slotRepos), 1.0f);
	cSlotController_ctor(AIC_Slot(self->slotLost), 0.2f);
	if (!e) {
		Con_Bluef("ERROR: Inicializaci\xf3n AI Char. Entidad = NULL.\n");
		CheckFailed("cAIChar_Fail_Entity_NULL", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->ent = e;
	self->ctrl = &WChar(e)->control;
	if (!self->flying) {
		m = operator_new(CAICHARCONTROL_SIZE);
		self->control = m ? cAICharControl2D_ctor(m, e, self, arg) : NULL;
	} else {
		m = operator_new(CAICHARCONTROL_SIZE);
		self->control = m ? cAICharControl3D_ctor(m, e, self, arg) : NULL;
	}
	CHECK(self->control);
	self->state = AIC_STATE_NONE;
	self->internState = AICI_NONE;
	cAIChar_Debugf(self, "Entidad CREADA");
	cAIChar_SetStateIntern(self, AIC_STATIC);
	self->back = 0;
	self->pauseCheck = 0;
	self->paused = 0;
	self->action = 0;
	self->chaseFar = 0.0f;
	self->chaseNear = 0.0f;
	self->actMax = 0.0f;
	self->actMin = 0.0f;
	self->actTime = 0.0f;
	self->pauses = 0;
	self->standTime = 0.0f;
	self->canTalk = 1;
	self->talking = 0;
	self->talkTurn = 0;
	self->numTalks = 0;
	self->talkRun = 0;
	self->lastTalk = 0;
	self->routeTime = 0.0f;
	self->pending.state = AIC_STATE_NONE;
	self->lostPoint.x = -FLT_MAX;
	self->lostPoint.y = -FLT_MAX;
	self->lostPoint.z = -FLT_MAX;
	if (*arg) {
		Con_Bluef("ERROR PARAM.: %s\n", self->ent->name);
		CheckFailed("behavMove.STUPID_PATHFINDING == 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return self;
}

void __fastcall cAIChar_dtor(cAIChar *self) {
	self->vftable = CAICHAR_VFTABLE;
	if (self->control) {
		AICC_Vtbl(self->control)->Delete(self->control, 1);
	}
	cAIChar_Debugf(self, "Entidad DESTRUIDA");
	cAIMove_dtor(self);
}

cAIChar *__thiscall cAIChar_Delete(cAIChar *self, byte flags) {
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHAR_SIZE);
	}
	return self;
}

/* ---- Global/Nto1Links.h and a vector compare, out of line in this part of the exe ---- */

void __thiscall Nto1Head_Link(Nto1Node *head, Nto1Node *node) {
	Nto1Node_LinkTo(head, node);
}

int Vec3_Differ(const float *a, const float *b) {
	return !(a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);
}

/* ---- small slots ---- */

void __thiscall cAIChar_Nop(cAIChar *self) {
	(void)self;
}

byte __thiscall cAIChar_EventNone(cAIChar *self, int ev) {
	(void)self;
	(void)ev;
	return 0;
}

byte __thiscall cAIChar_GetChanged(cAIChar *self) {
	return self->changed;
}

byte __thiscall cAIChar_IsPaused(cAIChar *self) {
	return self->paused;
}

int __thiscall cAIChar_GetState(cAIChar *self) {
	return self->state;
}

int __thiscall cAIChar_GetInternState(cAIChar *self) {
	return self->internState;
}

cAICharControl *__thiscall cAIChar_GetControl(cAIChar *ai) {
	return ai->control;
}

cAICharSaved *__thiscall cAIChar_GetLast(cAIChar *self) {
	return &self->last;
}

cAICharSaved *__thiscall cAIChar_SaveTo(cAIChar *self, cAICharSaved *s) {
	self->vftable->SaveState(self, s);
	return s;
}

byte __thiscall cAIChar_GetRepos(cAIChar *self) {
	return self->repos;
}

byte __thiscall cAIChar_IsTalking(cAIChar *self) {
	return self->talking;
}

byte __thiscall cAIChar_CanTalk(cAIChar *self) {
	return self->canTalk;
}

byte __thiscall cAIChar_GetDirect(cAIChar *self) {
	return AIC_CtlByte(self->control, 0x28);
}

byte __thiscall cAIChar_GetDirect2(cAIChar *self) {
	return AIC_CtlByte(self->control, 0x2c);
}

void __thiscall cAIChar_Draw(cAIChar *self) {
	AICC_Vtbl(self->control)->Draw(self->control);
}

void __thiscall cAIChar_UpdateOff(cAIChar *self, float dt) {
	cAIMove_UpdateOff(self, dt);
}

byte __thiscall cAIChar_SetRepos(cAIChar *self, byte on, byte now) {
	self->repos = on;
	if (on && now) {
		return self->vftable->Repos(self, 0.0f, g_DistMaxCheckReposAI, 0);
	}
	return 0;
}

void __thiscall cAIChar_SetPauseCheck(cAIChar *self, byte on) {
	self->pauseCheck = on;
	if (!on) {
		self->vftable->SetPaused(self, 0);
	}
}

void __thiscall cAIChar_SetPaused(cAIChar *self, byte on) {
	self->paused = on;
	if (on) {
		AICC_Vtbl(self->control)->Stop(self->control);
		self->ctrl->act = 0;
	}
}

void __thiscall cAIChar_UpdateAction0(cAIChar *self) {
	self->ctrl->act = 0;
}

void __thiscall cAIChar_SetAction(cAIChar *self, int action) {
	self->action = action;
	if (!action) {
		self->vftable->UpdateMove(self, 0.0f);
	}
}

void __thiscall cAIChar_CheckParams(cAIChar *self, int stupidPathfinding) {
	if (stupidPathfinding) {
		Con_Bluef("ERROR PARAM.: %s\n", self->ent->name);
		CHECK(stupidPathfinding == 0);
	}
}

byte __thiscall cAIChar_CanRepos(cAIChar *self, int state) {
	(void)self;
	return state == AIC_PATROL_ZONE || state == AIC_IN_ROUTE || state == AIC_STATIC_LOST ||
		state == AIC_STATIC_OBJ_NOT_ACCESS;
}

byte __thiscall cAIChar_IsGroupState(cAIChar *self, int state) {
	(void)self;
	return state != AIC_STATIC && state != AIC_STATIC_LOST && state != AIC_STATIC_OBJ_NOT_ACCESS &&
		state != AIC_STATIC_CHANGE_STATE && state != AIC_LOST_PATH && state != AIC_LOST_PATH_CHASE;
}

void __thiscall cAIChar_SetDists(cAIChar *self) {
	switch (self->state) {
	case AIC_CHASE_WITH_ACTION:
		self->actMin = 400.0f;
		self->actMax = 1000.0f;
		/* fall through */
	case AIC_CHASE_WITHOUT_ACTION:
		self->chaseFar = 1000.0f;
		self->chaseNear = 400.0f;
		break;
	case AIC_CHASE_WITH_ACTION_BACK:
		self->chaseNear = 300.0f;
		self->chaseFar = 900.0f;
		self->actMin = 300.0f;
		self->actMax = 900.0f;
		break;
	}
}

void __thiscall cAIChar_SetDists2(cAIChar *self) {
	self->vftable->SetDists(self);
}

/* ---- script variables ---- */

byte __thiscall cAIChar_VarI(cAIChar *self, const char *name, int *i, byte set) {
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Repos")) {
		if (!set) {
			*i = self->repos;
		} else if (*i == 0) {
			self->vftable->SetRepos(self, 0, 0);
		} else if (*i == 1) {
			self->vftable->SetRepos(self, 1, 0);
		} else if (*i == 2) {
			self->vftable->SetRepos(self, 1, 1);
		}
		return 1;
	}
	return cAIMove_VarI(self, name, i, set);
}

byte __thiscall cAIChar_VarF(cAIChar *self, const char *name, double *d, byte set) {
	if (!*name) {
		return 0;
	}
	return cAIMove_VarF(self, name, d, set);
}

byte __thiscall cAIChar_VarS(cAIChar *self, const char *name, char **s, byte set) {
	if (!*name) {
		return 0;
	}
	return cAIMove_VarS(self, name, s, set);
}

byte __thiscall cAIChar_VarV(cAIChar *self, const char *name, double *x, double *y, double *z, byte set) {
	if (!*name) {
		return 0;
	}
	return cAIMove_VarV(self, name, x, y, z, set);
}

/* ---- the names of the states and the debug text ---- */

static const char *const s_StateNames[] = {
	"STATIC_CHAR", "IN_ROUTE_CHAR", "CHASE_WITH_ACTION_CHAR", "CHASE_WITHOUT_ACTION_CHAR", "GO_TO_CHAR",
	"GET_AWAY_CHAR", "PATROL_ZONE_CHAR", "CHASE_WITH_ACTION_BACK_CHAR", "ORIENT_DINAMIC_OBJECTIVE_CHAR",
	"ORIENT_DINAMIC_OBJECTIVE_WITH_ACTION_CHAR", "LOST_PATH_CHAR", "LOST_PATH_CHASE_CHAR", "STATIC_LOST_CHAR",
	"STATIC_OBJ_NOT_ACCESS_CHAR", "STATIC_CHANGE_STATE_CHAR", "STATE_NONE_CHAR"
};

static const char *const s_InternStateNames[] = {
	"STOPPED_CHAR", "PAUSED_CONTROL_CHAR", "FINISHING_LINE_CHAR", "FINISHING_LINE_DIRECT_CHAR",
	"INT_CHASE_WITH_ACTION_CHAR", "INT_CHASE_WITHOUT_ACTION_CHAR", "STOPPED_ORIENT_STATIC_CHAR",
	"INT_CHASE_WITH_ACTION_BACK_CHAR", "STOPPED_ORIENT_DINAMIC_CHAR", "STOPPED_ORIENT_DINAMIC_WITH_ACTION_CHAR",
	"FOLLOW_THE_PATH_CHAR", "STATE_INTERN_RELAX_CHAR", "TALK_TO_CHAR", "STATE_INTERN_NONE_CHAR"
};

void __thiscall cAIChar_StateName(cAIChar *self, int state, char *buf) {
	(void)self;
	if ((uint32_t)state < sizeof(s_StateNames) / sizeof(s_StateNames[0])) {
		memcpy(buf, s_StateNames[state], strlen(s_StateNames[state]) + 1);
	}
}

void __thiscall cAIChar_GetStateName(cAIChar *self, char *buf) {
	self->vftable->StateName(self, self->state, buf);
}

void __thiscall cAIChar_InternStateName(cAIChar *self, int state, char *buf) {
	(void)self;
	if ((uint32_t)state < sizeof(s_InternStateNames) / sizeof(s_InternStateNames[0])) {
		memcpy(buf, s_InternStateNames[state], strlen(s_InternStateNames[state]) + 1);
	}
}

void __thiscall cAIChar_GetInternStateName(cAIChar *self, char *buf) {
	self->vftable->InternStateName(self, self->internState, buf);
}

void cAIChar_Debugf(cAIChar *self, const char *fmt, ...) {
	char text[1024];
	char intern[256];
	char state[256];
	char time[12];
	va_list ap;

	crt_strtime(time);
	self->vftable->GetStateName(self, state);
	self->vftable->GetInternStateName(self, intern);
	Str_PrintfQuiet(text, sizeof(text), "[%s] (%s) [%s - %s] - ", time, self->ent->name, state, intern);
	va_start(ap, fmt);
	Str_VPrintf(state, 0xff, fmt, ap);
	va_end(ap);
	crt_strncat(text, state, sizeof(text) - strlen(text) - 1);
	crt_strncat(text, "\n", sizeof(text) - strlen(text) - 1);
	Con_Print(0xffffff, text, 1);
}

/* ---- the update ---- */

void __thiscall cAIChar_Update(cAIChar *self, float dt) {
	if (self->pending.state != AIC_STATE_NONE && g_CharGroup->vftable->CanChange(g_CharGroup)) {
		self->vftable->RestoreState(self, &self->pending);
	}
	self->dt = dt;
	self->routeTime = self->routeTime + dt;
	self->vftable->CheckRepos(self);
	if (self->state == AIC_STATIC) {
		return;
	}
	self->vftable->CheckPause(self);
	if (self->paused) {
		return;
	}
	cAIMove_Update(self, dt);
	self->vftable->React(self);
	if (self->changed) {
		return;
	}
	self->vftable->UpdateThink(self);
	if (self->changed) {
		return;
	}
	((void(__thiscall *)(cAICharControl *, float))AIC_CtlSlot(self->control, 0x14))(self->control, dt);
	if (self->changed) {
		return;
	}
	self->vftable->UpdateMove(self, dt);
}

void __thiscall cAIChar_UpdateMove(cAIChar *self, float dt) {
	(void)dt;
	if (g_NotShootAI) {
		self->ctrl->act = 0;
		return;
	}
	if (!self->changed && (!self->vftable->React(self) || !self->vftable->CheckAction(self))) {
		return;
	}
	if (self->action == 0) {
		self->vftable->UpdateAction0(self);
	} else if (self->action == 1) {
		self->vftable->UpdateAction1(self);
	}
	self->vftable->UpdateAction(self);
}

void __thiscall cAIChar_UpdateThink(cAIChar *self) {
	cSlotController *s = AIC_Slot(self->slotThink);
	float t;

	if (!cSlotController_Tick(s, self->dt)) {
		return;
	}
	while (cSlotController_FireReset(s, &t)) {
		self->vftable->Think(self);
	}
}

void __thiscall cAIChar_UpdateAction1(cAIChar *self) {
	cSlotController *s = AIC_Slot(self->slotAction);
	cWalkCharEntity *t = WChar(AIC_Target(self));
	float tm;

	if (t->control.walk == 0.0f && t->control.strafe == 0.0f) {
		self->standTime = self->dt + self->standTime;
	} else {
		self->standTime = 0.0f;
	}
	if (self->ctrl->act) {
		self->actTime = self->dt + self->actTime;
	}
	if (!cSlotController_Tick(s, self->dt)) {
		return;
	}
	while (cSlotController_FireReset(s, &tm)) {
		self->vftable->CheckActDist(self);
	}
}

void __thiscall cAIChar_CheckActDist(cAIChar *self) {
	cActCtrl *ac = AIC_ActCtrl(self->ent);
	byte act = 0;

	if (ac->layers[0].action->priority <= 0x65 && AIC_CtlByte(self->control, 0x2c)) {
		cEntity *t = AIC_Target(self);
		vec3_t a, b;
		const vec3_t *p = AIC_Pos(self->ent, &a);
		const vec3_t *q = AIC_Pos(t, &b);
		float dx = q->x - p->x;
		float dy = q->y - p->y;
		float dz = q->z - p->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));

		if (self->actMax > d) {
			act = d > self->actMin;
		}
	}
	self->ctrl->act = act;
}

/* paused: out of view and far from the camera (in PATROL_ZONE not while the camera is near the zone on the same
 * floor), and not in view by InView */
static byte AIC_PauseWanted(cAIChar *self) {
	cWorld *w;
	vec3_t cam, a, b;
	const vec3_t *p;
	float dx, dy, dz, d;

	if (CGameModel_IsVisible(Obj_Vtbl(self->ent)->GetModel(self->ent))) {
		return 0;
	}
	w = g_pWorld;
	cam = w->camPos[w->curCam];
	if (self->state == AIC_PATROL_ZONE) {
		dx = self->center.x - cam.x;
		dy = self->center.y - cam.y;
		dz = self->center.z - cam.z;
		d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (self->radius + g_DistRadPauseCharAI > d) {
			int f = Floor_Get(&g_pWorld->floor, cam.y);

			p = AIC_Pos(self->ent, &a);
			if (f == Floor_Get(&g_pWorld->floor, p->y)) {
				return 0;
			}
		}
	}
	p = AIC_Pos(self->ent, &b);
	dx = p->x - cam.x;
	dy = p->y - cam.y;
	dz = p->z - cam.z;
	d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!(g_DistMaxCheckPauseAI > d)) {
		return 1;
	}
	if (!(d > g_DistMinCheckPauseAI)) {
		int f = Floor_Get(&g_pWorld->floor, cam.y);

		p = AIC_Pos(self->ent, &a);
		if (f == Floor_Get(&g_pWorld->floor, p->y)) {
			return 0;
		}
	}
	return !self->vftable->InView(self);
}

void __thiscall cAIChar_CheckPause(cAIChar *self) {
	cSlotController *s = AIC_Slot(self->slotPause);
	float t;

	if (!self->pauseCheck || !cSlotController_Tick(s, self->dt)) {
		return;
	}
	while (cSlotController_FireReset(s, &t)) {
		self->vftable->SetPaused(self, AIC_PauseWanted(self));
	}
}

void __thiscall cAIChar_CheckRepos(cAIChar *self) {
	cSlotController *s = AIC_Slot(self->slotRepos);
	float t;

	if (!self->repos || !cSlotController_Tick(s, self->dt)) {
		return;
	}
	while (cSlotController_FireReset(s, &t)) {
		cWorld *w = g_pWorld;
		vec3_t cam = w->camPos[w->curCam];
		CNodesWorld *nw = self->flying ? g_NodesWorld : (CNodesWorld *)g_NodesWorld2D;

		if (!nw->vftable->InGrid(nw, &cam)) {
			return;
		}
		if (self->vftable->CanRepos(self, self->state) &&
			!CGameModel_IsVisible(Obj_Vtbl(self->ent)->GetModel(self->ent))) {
			vec3_t a, b;
			const vec3_t *p;
			float dx, dy, dz, max;

			w = g_pWorld;
			cam = w->camPos[w->curCam];
			p = AIC_Pos(self->ent, &a);
			dx = p->x - cam.x;
			dy = p->y - cam.y;
			dz = p->z - cam.z;
			max = g_DistMaxCheckReposAI;
			if (max * max > dy * dy + dx * dx + dz * dz) {
				int f = Floor_Get(&g_pWorld->floor, cam.y);

				p = AIC_Pos(self->ent, &b);
				if (f == Floor_Get(&g_pWorld->floor, p->y) || self->vftable->InView(self)) {
					continue;
				}
			}
			self->vftable->Repos(self, g_DistMinCheckReposAI, g_DistMaxCheckReposAI, 1);
		}
	}
}

byte __thiscall cAIChar_Repos(cAIChar *self, float minDist, float maxDist, byte flag) {
	vec3_t pos, d, ang, zero, tmp;
	cWorld *w;
	byte r = cAICharGroup_FindRepos(g_CharGroup, &pos, self->ent, minDist, maxDist, flag);
	byte (*origin)(cEntity *e);

	if (!r) {
		return 0;
	}
	AICC_Vtbl(self->control)->ClearAvoid(self->control);
	self->reposing = 1;
	self->ent->vftable->SetPos(self->ent, &pos);
	w = g_pWorld;
	d.x = pos.x - w->camPos[w->curCam].x;
	d.y = pos.y - w->camPos[w->curCam].y;
	d.z = pos.z - w->camPos[w->curCam].z;
	Vec3_GetRotAng(&ang, &d);
	ang.x = 0.0f;
	ang.z = 0.0f;
	Obj_Vtbl(self->ent)->SetAng(self->ent, &ang);
	AICC_Vtbl(self->control)->Stop(self->control);
	zero.x = 0.0f;
	zero.y = 0.0f;
	zero.z = 0.0f;
	Obj_Vtbl(self->ent)->SetVel(self->ent, &zero);
	Logic_CharOnFloor(self->ent);
	if (!self->ent->flag_4c) {
		self->ent->vftable->SetActive(self->ent, 1);
	}
	self->vftable->AfterRepos(self);
	origin = g_SetOriginFunc;
	if (!origin || !origin(self->ent)) {
		self->center = *AIC_Pos(self->ent, &tmp);
		self->vftable->Resume(self);
	}
	self->reposing = 0;
	return r;
}

void __thiscall cAIChar_AfterRepos(cAIChar *self) {
	if (self->state == AIC_IN_ROUTE) {
		if (self->internState == AICI_TALK_TO) {
			AIC_ResumeRoute(self);
			self->talking = 0;
		}
	} else if (self->state == AIC_PATROL_ZONE && self->internState == AICI_TALK_TO) {
		AIC_ResumePatrol(self);
		self->talking = 0;
	}
}

void __thiscall cAIChar_Resume(cAIChar *self) {
	if (self->state == AIC_IN_ROUTE) {
		AIC_ResumeRoute(self);
	} else if (self->state == AIC_PATROL_ZONE) {
		AIC_ResumePatrol(self);
	}
}

cEntity *__thiscall cAIChar_GetCollide(cAIChar *self) {
	const char *name = Nto1Node_TargetName(&WChar(self->ent)->collide);
	cEntity *e;

	if (!*name) {
		return NULL;
	}
	e = World_FindEntity(name);
	if (!e) {
		return NULL;
	}
	AIC_ActCtrl(e);
	return e;
}

/* ---- the reactions and the think of the states ---- */

byte __thiscall cAIChar_React(cAIChar *self) {
	cEntity *t;

	switch (self->state) {
	case AIC_IN_ROUTE:
		self->vftable->ReactInRoute(self);
		break;
	case AIC_CHASE_WITH_ACTION:
	case AIC_CHASE_WITHOUT_ACTION:
	case AIC_GET_AWAY:
	case AIC_CHASE_WITH_ACTION_BACK:
	case AIC_ORIENT_DYNAMIC_OBJECTIVE:
	case AIC_ORIENT_DYNAMIC_OBJECTIVE_WITH_ACTION:
		t = AIC_Target(self);
		if (!t || !t->flag_4c) {
			self->vftable->Event(self, 3);
		}
		break;
	case AIC_PATROL_ZONE:
		self->vftable->ReactPatrol(self);
		break;
	case AIC_LOST_PATH:
		self->vftable->ReactLostPath(self);
		break;
	case AIC_LOST_PATH_CHASE:
		t = AIC_Target(self);
		if (t && t->flag_4c) {
			self->vftable->ReactLostPathChase(self);
		} else {
			self->vftable->Event(self, 3);
		}
		break;
	}
	return !self->changed;
}

byte __thiscall cAIChar_CheckAction(cAIChar *self) {
	cEntity *t;

	if (self->action == 1) {
		t = AIC_Target(self);
		if (!t || !t->flag_4c) {
			Con_Bluef("OBJECTIVE_DIED - cAIChar::CheckIntensiveAction - Entidad: %s - State: %d - StateIntern: %d\n",
				self->ent->name, self->state, self->internState);
			self->vftable->Event(self, 3);
			return 0;
		}
	}
	return 1;
}

void __thiscall cAIChar_ReactPatrol(cAIChar *self) {
	cEntity *t = AIC_Target(self);

	if (self->internState == AICI_TALK_TO && (!t || !t->flag_4c)) {
		AIC_ResumePatrol(self);
		self->talking = 0;
	}
	self->vftable->CheckTalk(self);
}

void __thiscall cAIChar_ReactInRoute(cAIChar *self) {
	cEntity *t = AIC_Target(self);

	if (self->internState == AICI_TALK_TO && (!t || !t->flag_4c)) {
		AIC_ResumeRoute(self);
		self->talking = 0;
	}
	self->vftable->CheckTalk(self);
}

void __thiscall cAIChar_ReactLostPath(cAIChar *self) {
	cSlotController *s = AIC_Slot(self->slotLost);
	float t;

	if (!cSlotController_Tick(s, self->dt)) {
		return;
	}
	while (cSlotController_FireReset(s, &t)) {
		cEntity *e = self->ent;
		vec3_t a;
		byte on;

		if (self->flying) {
			float r = Obj_Vtbl(e)->GetRadius(e);

			on = CNodesWorld_HasPointVis(g_NodesWorld, AIC_Pos(e, &a), r);
		} else {
			cActCtrl *ac = AIC_ActCtrl(e);
			float h2 = ac->cls->sRadius;
			float h1 = WChar(e)->floorDist;
			float r = Obj_Vtbl(e)->GetRadius(e);

			on = CNodesWorld2D_HasPointVis(g_NodesWorld2D, AIC_Pos(e, &a), r, h1, h2);
		}
		if (on) {
			self->vftable->RestoreStateIntern(self, &self->resume);
		}
	}
}

void __thiscall cAIChar_Think(cAIChar *self) {
	switch (self->state) {
	case AIC_IN_ROUTE:
		self->vftable->ThinkInRoute(self);
		return;
	case AIC_CHASE_WITH_ACTION:
		self->vftable->ThinkChase(self);
		return;
	case AIC_CHASE_WITHOUT_ACTION:
		self->vftable->ThinkChaseNoAction(self);
		return;
	case AIC_GO_TO:
		self->vftable->ThinkGoTo(self);
		return;
	case AIC_GET_AWAY:
		self->vftable->ThinkGetAway(self);
		return;
	case AIC_PATROL_ZONE:
		self->vftable->ThinkPatrol(self);
		return;
	case AIC_CHASE_WITH_ACTION_BACK:
		self->vftable->ThinkChaseBack(self);
		return;
	case AIC_LOST_PATH:
		self->vftable->ThinkLostPath(self);
		return;
	case AIC_LOST_PATH_CHASE:
		self->vftable->ThinkLostPathChase(self);
		return;
	}
}

void __thiscall cAIChar_ThinkLostPath(cAIChar *self) {
	if (self->stateTime > AIC_LOST_TIME) {
		self->vftable->SetStateIntern(self, AIC_STATIC_LOST);
	}
}

void __thiscall cAIChar_ThinkLostPathChase(cAIChar *self) {
	vec3_t a, b;
	const vec3_t *tp = AIC_Pos(AIC_Target(self), &a);
	const vec3_t *p = AIC_Pos(self->ent, &b);

	if (AICC_Vtbl(self->control)->TestLine(self->control, p, tp)) {
		const vec3_t *q = AIC_Pos(AIC_Target(self), &b);

		self->vftable->SetInternState(self, AICI_FINISHING_LINE_DIRECT, *q, (double)self->chaseFar,
			(double)self->speed);
		return;
	}
	self->vftable->SetInternState(self, AICI_STOPPED);
}

/* PAUSED_CONTROL ends: back to the kept state */
static void AIC_EndPause(cAIChar *self) {
	self->stateTime = 0.0f;
	AICC_Vtbl(self->control)->SetHold(self->control, 0);
	self->vftable->RestoreSaved(self, &self->resume);
}

/* FOLLOW_THE_PATH of a state that pauses: now and then PAUSED_CONTROL (the state kept); 1 then */
static int AIC_MaybePause(cAIChar *self) {
	if (!self->pauses || !(self->stateTime > g_MinTimeStopTempCharAI) || crt_rand() % 20 != 0) {
		return 0;
	}
	self->stateTime = 0.0f;
	self->vftable->SaveState(self, &self->resume);
	self->vftable->SetInternState(self, AICI_PAUSED_CONTROL);
	return 1;
}

/* TALK_TO of IN_ROUTE and PATROL_ZONE: the talk actions while the other one talks to this one; 1 when the talk
 * ends (too far, the other one does not talk to it, enough talks). A run of talk actions ends after more than
 * rand() % kinds + 1 of them (that many with atLeast) when the other one relaxes */
static int AIC_Talk(cAIChar *self, int kinds, int atLeast) {
	cAIChar *ai;
	vec3_t a, b;
	const vec3_t *tp, *ep;
	float r, dx, dy, dz;
	int k, n;

	r = AIC_Radius(AIC_Target(self)) + AIC_Radius(self->ent) + 100.0f;
	tp = AIC_Pos(AIC_Target(self), &a);
	ep = AIC_Pos(self->ent, &b);
	dx = ep->x - tp->x;
	dy = ep->y - tp->y;
	dz = ep->z - tp->z;
	if (dy * dy + dx * dx + dz * dz > r * r) {
		return 1;
	}
	WChar(self->ent)->relaxTime = 2.0f;
	ai = AIC_AI(AIC_Target(self));
	self->talking = !ai->vftable->IsTalking(ai);
	if (!self->talking || AIC_ActionIs(self->ent, 0, 11)) {
		ai = AIC_AI(AIC_Target(self));
		if (ai->vftable->GetTarget(ai)->target == self->ent) {
			ai = AIC_AI(AIC_Target(self));
			if (ai->vftable->GetInternState(ai) == AICI_TALK_TO) {
				return 0;
			}
		}
	}
	ai = AIC_AI(AIC_Target(self));
	if (ai->vftable->GetInternState(ai) != AICI_TALK_TO) {
		return 1;
	}
	ai = AIC_AI(AIC_Target(self));
	if (ai->vftable->GetTarget(ai)->target != self->ent) {
		return 1;
	}
	if (self->numTalks > g_MinNumTalksCharAI && crt_rand() % 5 == 0) {
		return 1;
	}
	k = crt_rand() % kinds + 1;
	if (atLeast ? self->talkRun >= k : self->talkRun > k) {
		if (!AIC_ActionIs(AIC_Target(self), 4, 2)) {
			self->talking = 0;
			self->talkRun = 0;
			return 0;
		}
	}
	n = crt_rand() % 3;
	if (self->lastTalk == n) {
		n = (n + 1) % 3;
	}
	if (self->talking) {
		AIC_Play(self->ent, s_TalkActs[8 + n]);
		self->numTalks++;
		self->talkRun++;
		self->lastTalk = n;
	}
	return 0;
}

void __thiscall cAIChar_ThinkInRoute(cAIChar *self) {
	switch (self->internState) {
	case AICI_PAUSED_CONTROL:
		if (self->pauses) {
			WChar(self->ent)->relaxTime = 0.0f;
			if (!(self->stateTime > g_MinTimeStoppedCharAI) || crt_rand() % 10 != 0) {
				return;
			}
		}
		AIC_EndPause(self);
		return;
	case AICI_FOLLOW_THE_PATH:
		AIC_MaybePause(self);
		return;
	case AICI_TALK_TO:
		if (self->talkTurn && AIC_Talk(self, 2, 0)) {
			AIC_ResumeRoute(self);
			self->talking = 0;
		}
		return;
	}
}

void __thiscall cAIChar_ThinkPatrol(cAIChar *self) {
	vec3_t a;
	const vec3_t *p;

	switch (self->internState) {
	case AICI_PAUSED_CONTROL:
		if (self->pauses) {
			if (AIC_ActionIs(self->ent, 1, 5)) {
				return;
			}
			if (++self->relaxCount < 3) {
				AIC_Play(self->ent, s_TalkActs[crt_rand() % 2 == 0 ? 4 : 5]);
				return;
			}
		}
		AIC_EndPause(self);
		return;
	case AICI_FOLLOW_THE_PATH:
		if (AIC_MaybePause(self) || !self->inPath) {
			return;
		}
		p = AIC_Pos(self->ent, &a);
		{
			float dx = self->center.x - p->x;
			float dy = self->center.y - p->y;
			float dz = self->center.z - p->z;

			if ((float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) > self->radius) {
				self->stateTime = 0.0f;
				self->vftable->SetInternState(self, AICI_RELAX);
			}
		}
		return;
	case AICI_RELAX:
		if (!g_pWorld->alarm.on) {
			AICC_Vtbl(self->control)->SetHold(self->control, 1);
			if (AIC_ActionIs(self->ent, 1, 3)) {
				return;
			}
			if (self->relaxCount == 0) {
				AIC_Play(self->ent, s_TalkActs[4]);
				self->relaxCount++;
				return;
			}
			if (AIC_ActionIs(self->ent, 4, 1)) {
				return;
			}
		}
		AIC_Pos(self->ent, &a);
		self->inPath = 0;
		self->vftable->SetInternState(self, AICI_FINISHING_LINE, self->center, (double)g_AICRadiusInRoute,
			(double)self->speed);
		return;
	case AICI_TALK_TO:
		if (self->talkTurn && AIC_Talk(self, 4, 1)) {
			AIC_ResumePatrol(self);
			self->talking = 0;
		}
		return;
	}
}

void __thiscall cAIChar_CheckTalk(cAIChar *self) {
	cEntity *o;
	cAIChar *ai;
	vec3_t a, b;
	float r1, r2, y1;

	if (!self->canTalk ||
		(self->internState != AICI_FINISHING_LINE && self->internState != AICI_FOLLOW_THE_PATH) ||
		g_pWorld->alarm.on || !(self->routeTime > 5.0f)) {
		return;
	}
	o = self->vftable->GetCollide(self);
	AIC_ActCtrl(self->ent);
	if (!o) {
		return;
	}
	AIC_ActCtrl(o);
	if (!o->vftable->GetAI(o)) {
		return;
	}
	ai = AIC_AI(o);
	if (!ai->vftable->CanTalk(ai)) {
		return;
	}
	ai = AIC_AI(o);
	if (ai->vftable->GetState(ai) != AIC_PATROL_ZONE) {
		ai = AIC_AI(o);
		if (ai->vftable->GetState(ai) != AIC_IN_ROUTE) {
			return;
		}
	}
	ai = AIC_AI(o);
	if (ai->vftable->GetInternState(ai) != AICI_FOLLOW_THE_PATH) {
		ai = AIC_AI(o);
		if (ai->vftable->GetInternState(ai) != AICI_FINISHING_LINE) {
			return;
		}
	}
	r1 = AIC_Radius(self->ent);
	r2 = AIC_Radius(o);
	y1 = AIC_Pos(self->ent, &a)->y + r1;
	if ((double)g_MinTalkCharAI > __builtin_fabs((double)(y1 - (AIC_Pos(o, &b)->y + r2)))) {
		ai = AIC_AI(o);
		ai->vftable->SetInternState(ai, AICI_TALK_TO, self->ent);
		self->vftable->SetInternState(self, AICI_TALK_TO, o);
	}
}

byte __thiscall cAIChar_Facing(cAIChar *self, cEntity *a, cEntity *b) {
	vec3_t t1, t2, d, nd, r;
	const vec3_t *pa = AIC_Pos(a, &t1);
	const vec3_t *pb = AIC_Pos(b, &t2);

	d.x = pb->x - pa->x;
	d.y = pb->y - pa->y;
	d.z = pb->z - pa->z;
	r = AIC_ViewDir(a);
	if (!self->flying) {
		r.y = 0.0f;
		d.y = 0.0f;
	}
	if (AIC_ANG45 > Vec3_Angle(&r, &d)) {
		return 1;
	}
	nd.x = -d.x;
	nd.y = -d.y;
	nd.z = -d.z;
	r = AIC_ViewDir(b);
	if (!self->flying) {
		r.y = 0.0f;
		nd.y = 0.0f;
	}
	return AIC_ANG45 > Vec3_Angle(&r, &nd);
}

/* ---- the events of the move ---- */

byte __thiscall cAIChar_Event(cAIChar *self, int ev) {
	byte (__thiscall *f)(cAIChar *self, int ev);
	byte r;

	switch (self->state) {
	case AIC_IN_ROUTE:
		f = self->vftable->EventInRoute;
		break;
	case AIC_CHASE_WITH_ACTION:
		f = self->vftable->EventChase;
		break;
	case AIC_CHASE_WITHOUT_ACTION:
		f = self->vftable->EventChaseNoAction;
		break;
	case AIC_GO_TO:
		f = self->vftable->EventGoTo;
		break;
	case AIC_GET_AWAY:
		f = self->vftable->EventGetAway;
		break;
	case AIC_PATROL_ZONE:
		f = self->vftable->EventPatrol;
		break;
	case AIC_CHASE_WITH_ACTION_BACK:
		f = self->vftable->EventChaseBack;
		break;
	case AIC_LOST_PATH:
		f = self->vftable->EventLostPath;
		break;
	default:
		return self->vftable->EventDefault(self, ev);
	}
	r = f(self, ev);
	if (!r) {
		r = self->vftable->EventDefault(self, ev);
	}
	return r;
}

byte __thiscall cAIChar_EventDefault(cAIChar *self, int ev) {
	switch (ev) {
	case 0:
		if (self->state != AIC_LOST_PATH && self->state != AIC_LOST_PATH_CHASE) {
			self->vftable->SaveState(self, &self->resume);
			if (self->state == AIC_CHASE_WITH_ACTION || self->state == AIC_CHASE_WITHOUT_ACTION ||
				self->state == AIC_CHASE_WITH_ACTION_BACK) {
				self->vftable->OnEvent(self, 0);
				self->vftable->SetStateIntern(self, AIC_LOST_PATH_CHASE);
			} else if (!self->vftable->SetStateIntern(self, AIC_LOST_PATH)) {
				self->vftable->OnEvent(self, 0);
				self->vftable->SetStateIntern(self, AIC_STATIC_LOST);
			}
		}
		return 1;
	case 1:
		self->vftable->OnEvent(self, 1);
		self->vftable->SetStateIntern(self, AIC_STATIC_OBJ_NOT_ACCESS);
		return 1;
	case 3:
		self->vftable->OnEvent(self, 3);
		self->vftable->SetStateIntern(self, AIC_STATIC);
		return 1;
	case 7:
		self->vftable->OnEvent(self, 7);
		self->vftable->SaveState(self, &self->resume);
		self->vftable->SetStateIntern(self, AIC_STATIC);
		return 1;
	}
	return 0;
}

/* event 6 of IN_ROUTE and PATROL_ZONE: the turn to the other one at the start of a talk */
static byte AIC_EventTurn(cAIChar *self, int ev) {
	if (self->internState != AICI_TALK_TO || self->talkTurn) {
		return 0;
	}
	self->vftable->OnEvent(self, ev);
	self->talking = 1;
	self->talkTurn = 1;
	self->internState = AICI_TALK_TO;
	if (cActCtrl_Play(AIC_ActCtrl(self->ent), s_TalkActs[0], 0, 1.0f)) {
		WChar(self->ent)->displAng = 0.0f;
	}
	return 1;
}

byte __thiscall cAIChar_EventInRoute(cAIChar *self, int ev) {
	vec3_t d;

	switch (ev) {
	case 2:
	case 4:
		self->vftable->OnEvent(self, ev);
		d = AIC_ViewDir(self->ent);
		self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
			(double)self->speed, 1, 1);
		return 1;
	case 6:
		return AIC_EventTurn(self, ev);
	case 10:
		self->vftable->OnEvent(self, ev);
		d = AIC_ViewDir(self->ent);
		d.x = -d.x;
		d.y = -d.y;
		d.z = -d.z;
		self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
			(double)self->speed, 0, 1);
		return 1;
	}
	return 0;
}

byte __thiscall cAIChar_EventPatrol(cAIChar *self, int ev) {
	vec3_t d, nd, a;
	const vec3_t *p;
	float dist;

	switch (ev) {
	case 2:
	case 4:
		if (self->internState == AICI_FINISHING_LINE) {
			self->vftable->OnEvent(self, ev);
			d.x = AIC_RandSigned();
			d.z = AIC_RandSigned();
			d.y = 0.0f;
			self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
				(double)self->speed, 0, 1);
		} else {
			self->vftable->OnEvent(self, ev);
			d = AIC_ViewDir(self->ent);
			self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
				(double)self->speed, 1, 1);
		}
		return 1;
	case 6:
		return AIC_EventTurn(self, ev);
	case 10:
		nd = AIC_ViewDir(self->ent);
		nd.x = -nd.x;
		nd.y = -nd.y;
		nd.z = -nd.z;
		p = AIC_Pos(self->ent, &a);
		d.x = p->x - self->center.x;
		d.y = p->y - self->center.y;
		d.z = p->z - self->center.z;
		dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z)) + 500.0f;
		if (dist > self->radius && AIC_ANG90 > Vec3_Angle(&nd, &d)) {
			return 0;
		}
		self->vftable->OnEvent(self, ev);
		self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, nd, (double)g_AICRadiusInRoute,
			(double)self->speed, 0, 1);
		return 1;
	}
	return 0;
}

byte __thiscall cAIChar_EventGoTo(cAIChar *self, int ev) {
	if (ev == 2) {
		if (self->internState == AICI_FINISHING_LINE) {
			self->vftable->OnEvent(self, 2);
			self->vftable->SetInternState(self, AICI_STOPPED_ORIENT_STATIC, self->dir);
			return 1;
		}
	} else if (ev == 6 && self->internState == AICI_STOPPED_ORIENT_STATIC) {
		self->vftable->OnEvent(self, 6);
		self->arrived = 1;
		self->vftable->SetStateIntern(self, AIC_STATIC);
		return 1;
	}
	return 0;
}

byte __thiscall cAIChar_EventGetAway(cAIChar *self, int ev) {
	vec3_t a, b, d;
	const vec3_t *tp, *p;

	switch (ev) {
	case 2:
	case 4:
	case 5:
		self->vftable->OnEvent(self, ev);
		tp = AIC_Pos(AIC_Target(self), &a);
		p = AIC_Pos(self->ent, &b);
		d.x = p->x - tp->x;
		d.y = p->y - tp->y;
		d.z = p->z - tp->z;
		self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
			(double)self->speed, 1, 0);
		return 1;
	case 10:
		self->vftable->OnEvent(self, ev);
		d = AIC_ViewDir(self->ent);
		d.x = -d.x;
		d.y = -d.y;
		d.z = -d.z;
		self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
			(double)self->speed, 0, 0);
		return 1;
	}
	return 0;
}

byte __thiscall cAIChar_EventChaseBack(cAIChar *self, int ev) {
	if (ev == 8) {
		self->vftable->OnEvent(self, 8);
		self->back = 1;
		return 1;
	}
	if (ev == 9) {
		self->vftable->OnEvent(self, 9);
		self->back = 0;
		return 1;
	}
	return 0;
}

byte __thiscall cAIChar_EventLostPath(cAIChar *self, int ev) {
	vec3_t buf;
	const vec3_t *g;

	if (ev == 7) {
		self->vftable->OnEvent(self, 7);
		self->vftable->SaveState(self, &self->resume);
		self->vftable->SetStateIntern(self, AIC_STATIC_LOST);
		return 1;
	}
	if (ev != 2) {
		return 0;
	}
	g = AIC_CtlVec(self->control, 0x34, &buf);
	if (self->lostPoint.x == g->x && self->lostPoint.y == g->y && self->lostPoint.z == g->z) {
		return 1;
	}
	if (self->lostPoint.x == -FLT_MAX && self->lostPoint.y == -FLT_MAX && self->lostPoint.z == -FLT_MAX) {
		return 1;
	}
	self->vftable->OnEvent(self, 2);
	self->vftable->SetInternState(self, AICI_FINISHING_LINE_DIRECT, self->lostPoint, 50.0, (double)self->speed);
	return 1;
}

/* ---- the state kept and restored ---- */

void __thiscall cAIChar_SaveState(cAIChar *self, cAICharSaved *s) {
	cEntity *t = AIC_Target(self);

	if (t) {
		AIC_CopyName(s->target, sizeof(s->target), t->name);
	} else {
		s->target[0] = 0;
	}
	s->dir = self->dir;
	s->dest = self->dest;
	s->dist = self->dist;
	s->state = self->state;
	s->internState = self->internState;
	s->pathA = self->pathA;
	s->speed = self->speed;
	s->center = self->center;
	s->radius = self->radius;
	s->pauses = self->pauses;
	s->action = self->action;
	s->stateTime = self->stateTime;
	s->changing = self->changing;
	s->changed = self->changed;
	s->back = self->back;
	s->orient = self->orient;
}

void __thiscall cAIChar_RestoreSaved(cAIChar *self, const cAICharSaved *s) {
	cEntity *e;

	self->changed = 1;
	self->dir = s->dir;
	self->dest = s->dest;
	self->dist = s->dist;
	self->state = s->state;
	self->internState = s->internState;
	self->pathA = s->pathA;
	self->speed = s->speed;
	self->center = s->center;
	self->radius = s->radius;
	self->pauses = s->pauses;
	self->action = s->action;
	self->stateTime = s->stateTime;
	self->changing = s->changing;
	self->changed = s->changed;
	self->back = s->back;
	self->orient = s->orient;
	e = World_FindEntity(s->target);
	if (e && e->vftable->IsType(e, TYEN_WITH_LIFE)) {
		AIC_LinkTarget(self, e);
		return;
	}
	CHECK(self->target.data);
	Nto1Node_Unlink(&self->target);
}

/* RestoreState and RestoreStateIntern: the state of s again through SetState (or SetStateIntern) */
static byte AIC_Restore(cAIChar *self, const cAICharSaved *s, int intern) {
	byte (*set)(cAIChar *self, int state, ...);

	set = intern ? self->vftable->SetStateIntern : self->vftable->SetState;
	switch (s->state) {
	case AIC_STATIC:
	case AIC_STATIC_LOST:
	case AIC_STATIC_OBJ_NOT_ACCESS:
	case AIC_STATIC_CHANGE_STATE:
		return set(self, s->state);
	case AIC_IN_ROUTE:
		return set(self, AIC_IN_ROUTE, (double)s->speed, (int)s->pauses);
	case AIC_CHASE_WITH_ACTION:
	case AIC_CHASE_WITHOUT_ACTION:
	case AIC_GET_AWAY:
	case AIC_CHASE_WITH_ACTION_BACK:
		return set(self, s->state, World_FindEntity(s->target), (double)s->speed);
	case AIC_GO_TO:
		return set(self, AIC_GO_TO, s->dest, s->dir, (double)s->dist, (double)s->speed);
	case AIC_PATROL_ZONE:
		return set(self, AIC_PATROL_ZONE, s->center, (double)s->radius, (double)s->speed, (int)s->pauses);
	case AIC_ORIENT_DYNAMIC_OBJECTIVE:
	case AIC_ORIENT_DYNAMIC_OBJECTIVE_WITH_ACTION:
		return set(self, s->state, World_FindEntity(s->target), (int)s->orient);
	case AIC_LOST_PATH:
		return set(self, AIC_LOST_PATH);
	case AIC_LOST_PATH_CHASE:
		return set(self, AIC_LOST_PATH_CHASE);
	}
	return 1;
}

byte __thiscall cAIChar_RestoreState(cAIChar *self, const cAICharSaved *s) {
	return AIC_Restore(self, s, 0);
}

byte __thiscall cAIChar_RestoreStateIntern(cAIChar *self, const cAICharSaved *s) {
	return AIC_Restore(self, s, 1);
}

/* ---- the change of state ---- */

void __thiscall cAIChar_BeginChange(cAIChar *self, int state) {
	self->vftable->SaveState(self, &self->last);
	self->stateTime = 0.0f;
	self->changing = 1;
	self->changed = 1;
	self->back = 0;
	self->state = state;
	self->talking = 0;
}

byte cAIChar_SetState(cAIChar *self, int state, ...) {
	char buf[256];
	va_list ap;
	byte r;

	self->vftable->StateName(self, state, buf);
	self->vftable->Debugf(self, "Demanda externa de CAMBIO de ESTADO -> %s", buf);
	if (!self->vftable->IsGroupState(self, state) || g_CharGroup->vftable->CanChange(g_CharGroup)) {
		self->vftable->BeginChange(self, state);
	}
	va_start(ap, state);
	r = self->vftable->ChangeState(self, state, ap);
	va_end(ap);
	return r;
}

byte cAIChar_SetStateIntern(cAIChar *self, int state, ...) {
	char buf[256];
	va_list ap;
	byte r;

	self->vftable->StateName(self, state, buf);
	self->vftable->Debugf(self, "Demanda interna de CAMBIO de ESTADO -> %s", buf);
	if (self->vftable->IsGroupState(self, state)) {
		g_CharGroup->vftable->AllowChange(g_CharGroup);
	}
	self->vftable->BeginChange(self, state);
	va_start(ap, state);
	r = self->vftable->ChangeState(self, state, ap);
	va_end(ap);
	return r;
}

/* IN_ROUTE and PATROL_ZONE start walking: the speed of a FOLLOW_THE_PATH of the same state goes on, else a new one
 * in a random direction */
static void AIC_StartWalk(cAIChar *self, float speed) {
	if (self->last.state == self->state && !self->reposing && self->internState == AICI_FOLLOW_THE_PATH) {
		self->speed = speed;
		AICC_Vtbl(self->control)->SetSpeed(self->control, speed);
	} else {
		vec3_t d;

		d.x = AIC_RandSigned();
		d.z = AIC_RandSigned();
		d.y = 0.0f;
		self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute, (double)speed, 0, 1);
	}
}

byte __thiscall cAIChar_ChangeState(cAIChar *self, int state, va_list ap) {
	char buf[256];
	byte allowed = 1;
	byte ok = 1;
	cEntity *e;
	cEntity *t;
	float speed;
	int intern;

	if (self->vftable->IsGroupState(self, state) && !(allowed = g_CharGroup->vftable->CanChange(g_CharGroup))) {
		self->pending.state = state;
	} else if (state != AIC_STATIC_CHANGE_STATE) {
		self->pending.state = AIC_STATE_NONE;
	}
	if (AIC_Target(self)) {
		CHECK(self->target.data);
		Nto1Node_Unlink(&self->target);
	}
	switch (state) {
	case AIC_STATIC:
		self->vftable->SetPauseCheck(self, 0);
		self->vftable->SetInternState(self, AICI_STOPPED);
		break;
	case AIC_IN_ROUTE:
		if (!allowed) {
			self->pending.speed = (float)va_arg(ap, double);
			self->pending.pauses = (byte)va_arg(ap, int);
			goto wait;
		}
		self->routeTime = 0.0f;
		self->vftable->SetPauseCheck(self, 1);
		speed = (float)va_arg(ap, double);
		self->pauses = (byte)va_arg(ap, int);
		AIC_StartWalk(self, speed);
		break;
	case AIC_PATROL_ZONE:
		if (!allowed) {
			self->pending.center = va_arg(ap, vec3_t);
			self->pending.radius = (float)va_arg(ap, double);
			self->pending.speed = (float)va_arg(ap, double);
			self->pending.pauses = (byte)va_arg(ap, int);
			goto wait;
		}
		self->routeTime = 0.0f;
		self->vftable->SetPauseCheck(self, 1);
		self->center = va_arg(ap, vec3_t);
		self->radius = (float)va_arg(ap, double);
		speed = (float)va_arg(ap, double);
		self->pauses = (byte)va_arg(ap, int);
		AIC_StartWalk(self, speed);
		break;
	case AIC_GO_TO:
		if (!allowed) {
			self->pending.dest = va_arg(ap, vec3_t);
			self->pending.dir = va_arg(ap, vec3_t);
			self->pending.dist = (float)va_arg(ap, double);
			self->pending.speed = (float)va_arg(ap, double);
			goto wait;
		} else {
			vec3_t dest;
			float dist;

			self->arrived = 0;
			self->vftable->SetPauseCheck(self, 0);
			dest = va_arg(ap, vec3_t);
			self->dir = va_arg(ap, vec3_t);
			dist = (float)va_arg(ap, double);
			speed = (float)va_arg(ap, double);
			self->vftable->SetInternState(self, AICI_FINISHING_LINE, dest, (double)dist, (double)speed);
		}
		break;
	case AIC_GET_AWAY:
		e = va_arg(ap, cEntity *);
		if (!self->vftable->CanTarget(self, e)) {
			ok = 0;
			break;
		}
		if (!allowed) {
			AIC_CopyName(self->pending.target, sizeof(self->pending.target), e->name);
			self->pending.speed = (float)va_arg(ap, double);
			goto wait;
		} else {
			vec3_t a, b, d;
			const vec3_t *tp, *p;

			self->vftable->SetPauseCheck(self, 0);
			speed = (float)va_arg(ap, double);
			AIC_LinkTarget(self, e);
			self->vftable->SetDists2(self);
			tp = AIC_Pos(e, &a);
			p = AIC_Pos(self->ent, &b);
			d.x = p->x - tp->x;
			d.y = p->y - tp->y;
			d.z = p->z - tp->z;
			self->vftable->SetInternState(self, AICI_FOLLOW_THE_PATH, d, (double)g_AICRadiusInRoute,
				(double)speed, 0, 0);
		}
		break;
	case AIC_CHASE_WITH_ACTION:
	case AIC_CHASE_WITHOUT_ACTION:
	case AIC_CHASE_WITH_ACTION_BACK:
		e = va_arg(ap, cEntity *);
		if (!self->vftable->CanTarget(self, e)) {
			ok = 0;
			break;
		}
		if (!allowed) {
			AIC_CopyName(self->pending.target, sizeof(self->pending.target), e->name);
			self->pending.speed = (float)va_arg(ap, double);
			goto wait;
		}
		self->vftable->SetPauseCheck(self, 0);
		AIC_LinkTarget(self, e);
		speed = (float)va_arg(ap, double);
		self->vftable->SetDists2(self);
		if (self->last.state == self->state && !strcmp(self->last.target, e->name)) {
			self->speed = speed;
			AICC_Vtbl(self->control)->SetSpeed(self->control, speed);
		} else {
			intern = state == AIC_CHASE_WITH_ACTION ? AICI_INT_CHASE_WITH_ACTION :
				state == AIC_CHASE_WITHOUT_ACTION ? AICI_INT_CHASE_WITHOUT_ACTION : AICI_INT_CHASE_WITH_ACTION_BACK;
			self->vftable->SetInternState(self, intern, (double)speed);
		}
		AICC_Vtbl(self->control)->Nop(self->control);
		break;
	case AIC_ORIENT_DYNAMIC_OBJECTIVE:
	case AIC_ORIENT_DYNAMIC_OBJECTIVE_WITH_ACTION:
		e = va_arg(ap, cEntity *);
		if (!self->vftable->CanTarget(self, e)) {
			ok = 0;
			break;
		}
		if (!allowed) {
			AIC_CopyName(self->pending.target, sizeof(self->pending.target), e->name);
			self->pending.orient = (byte)va_arg(ap, int);
			goto wait;
		} else {
			byte orient;

			self->vftable->SetPauseCheck(self, 0);
			AIC_LinkTarget(self, e);
			orient = (byte)va_arg(ap, int);
			self->vftable->SetDists2(self);
			self->vftable->SetInternState(self, state == AIC_ORIENT_DYNAMIC_OBJECTIVE ? AICI_STOPPED_ORIENT_DYNAMIC :
				AICI_STOPPED_ORIENT_DYNAMIC_WITH_ACTION, (int)orient);
			AICC_Vtbl(self->control)->Nop(self->control);
		}
		break;
	default:
		ok = self->vftable->ChangeStateMore(self, state, allowed, ap);
		break;
	}
	if (allowed) {
		self->changing = 0;
		if (!ok) {
			goto fail;
		}
		if (self->vftable->IsGroupState(self, state)) {
			g_CharGroup->vftable->ResetChange(g_CharGroup);
		}
		self->vftable->StateName(self, state, buf);
		t = AIC_Target(self);
		if (!t) {
			self->vftable->Debugf(self, "CAMBIO DE ESTADO REALIZADO -> %s", buf);
		} else {
			self->vftable->Debugf(self, "CAMBIO DE ESTADO REALIZADO -> %s - OBJETIVO: %s", buf, t->name);
		}
		return ok;
	}
	if (!ok) {
		goto fail;
	}
wait:
	self->vftable->Debugf(self, "En ESPERA CAMBIO de ESTADO -> STATIC_CHANGE_STATE_CHAR");
	self->vftable->SetStateIntern(self, AIC_STATIC_CHANGE_STATE);
	return ok;
fail:
	Con_Bluef("CAMBIO de ESTADO con PARAMETROS INCORRECTOS\n");
	self->vftable->SetStateIntern(self, AIC_STATIC);
	return ok;
}

byte __thiscall cAIChar_ChangeStateMore(cAIChar *self, int state, int allowed, va_list ap) {
	vec3_t a, b, none;

	(void)allowed;
	(void)ap;
	switch (state) {
	case AIC_LOST_PATH:
		AIC_CtlVec(self->control, 0x44, &a);
		self->lostPoint = *AIC_CtlVec(self->control, 0x48, &b);
		none.x = -FLT_MAX;
		none.y = -FLT_MAX;
		none.z = -FLT_MAX;
		if ((byte)Vec3_Differ(&a.x, &none.x)) {
			self->vftable->SetPauseCheck(self, 0);
			self->vftable->SetInternState(self, AICI_FINISHING_LINE_DIRECT, a, 50.0, (double)self->speed);
		} else {
			self->vftable->SetStateIntern(self, AIC_STATIC);
		}
		return 1;
	case AIC_LOST_PATH_CHASE:
	case AIC_STATIC_CHANGE_STATE:
		self->vftable->SetPauseCheck(self, 0);
		self->vftable->SetInternState(self, AICI_STOPPED);
		return 1;
	case AIC_STATIC_LOST:
	case AIC_STATIC_OBJ_NOT_ACCESS:
		self->vftable->SetPauseCheck(self, 1);
		self->vftable->SetInternState(self, AICI_STOPPED);
		return 1;
	}
	Con_Bluef("ERROR PARAM. AI: ESTADO INEXISTENTE.\n");
	return 0;
}

/* ---- the internal state ---- */

void __thiscall cAIChar_SetInternStateV(cAIChar *self, int state, va_list ap) {
	char buf[256];
	cAICharControl *c;
	cEntity *e;
	byte ok;
	int act = 0;

	self->vftable->InternStateName(self, state, buf);
	self->vftable->Debugf(self, "CAMBIO de ESTADO INTERNO -> %s", buf);
	self->changed = 1;
	self->relaxCount = 0;
	self->internState = state;
	c = self->control;
	switch (state) {
	case AICI_STOPPED:
		self->moveMode = AICC_STOP;
		ok = AICC_Vtbl(c)->SetMove(c, AICC_STOP);
		break;
	case AICI_PAUSED_CONTROL:
		AICC_Vtbl(c)->SetHold(c, 1);
		ok = 1;
		break;
	case AICI_FINISHING_LINE:
	case AICI_FINISHING_LINE_DIRECT:
		self->moveMode = state == AICI_FINISHING_LINE ? AICC_GO_TO : AICC_GO_TO_DIRECT;
		self->dest = va_arg(ap, vec3_t);
		self->dist = (float)va_arg(ap, double);
		self->speed = (float)va_arg(ap, double);
		ok = AICC_Vtbl(c)->SetMove(c, self->moveMode, self->dest, (double)self->dist, (double)self->speed);
		break;
	case AICI_INT_CHASE_WITH_ACTION:
	case AICI_INT_CHASE_WITHOUT_ACTION:
	case AICI_INT_CHASE_WITH_ACTION_BACK:
		self->moveMode = state == AICI_INT_CHASE_WITH_ACTION_BACK ? AICC_FOLLOW_PATH : AICC_FOLLOW;
		self->speed = (float)va_arg(ap, double);
		ok = AICC_Vtbl(c)->SetMove(c, self->moveMode, (double)self->chaseNear, (double)self->chaseFar,
			(double)self->speed, 1);
		act = state != AICI_INT_CHASE_WITHOUT_ACTION;
		break;
	case AICI_STOPPED_ORIENT_STATIC:
		self->moveMode = AICC_DIR;
		self->dir = va_arg(ap, vec3_t);
		ok = AICC_Vtbl(c)->SetMove(c, AICC_DIR, self->dir);
		break;
	case AICI_STOPPED_ORIENT_DYNAMIC:
	case AICI_STOPPED_ORIENT_DYNAMIC_WITH_ACTION:
		self->moveMode = AICC_AWAY;
		self->orient = (byte)va_arg(ap, int);
		ok = AICC_Vtbl(c)->SetMove(c, AICC_AWAY, (int)self->orient);
		act = state == AICI_STOPPED_ORIENT_DYNAMIC_WITH_ACTION;
		break;
	case AICI_FOLLOW_THE_PATH:
		self->moveMode = AICC_ROUTE;
		self->inPath = 1;
		self->dir = va_arg(ap, vec3_t);
		self->dist = (float)va_arg(ap, double);
		self->speed = (float)va_arg(ap, double);
		self->pathA = (byte)va_arg(ap, int);
		self->pathB = (byte)va_arg(ap, int);
		ok = AICC_Vtbl(c)->SetMove(c, AICC_ROUTE, self->dir, (double)self->dist, (double)self->speed,
			(int)self->pathA, (int)self->pathB);
		break;
	case AICI_TALK_TO:
		e = va_arg(ap, cEntity *);
		self->talking = 0;
		self->talkTurn = 0;
		self->numTalks = 0;
		self->moveMode = AICC_AWAY;
		AIC_LinkTarget(self, e);
		c = self->control;
		ok = AICC_Vtbl(c)->SetMove(c, self->moveMode, 0);
		break;
	default:
		return;
	}
	if (ok) {
		self->vftable->SetAction(self, act);
	}
}

void cAIChar_SetInternState(cAIChar *self, int state, ...) {
	va_list ap;

	va_start(ap, state);
	self->vftable->SetInternStateV(self, state, ap);
	va_end(ap);
}

/* ---- the action of the state ---- */

byte __thiscall cAIChar_IsMoving(cAIChar *self) {
	cWalkCharEntity *w;

	switch (self->internState) {
	case AICI_STOPPED:
	case AICI_PAUSED_CONTROL:
	case AICI_STOPPED_ORIENT_STATIC:
	case AICI_STOPPED_ORIENT_DYNAMIC:
	case AICI_STOPPED_ORIENT_DYNAMIC_WITH_ACTION:
	case AICI_RELAX:
	case AICI_TALK_TO:
		return 0;
	}
	if (AICC_Vtbl(self->control)->IsBlocked(self->control)) {
		return 0;
	}
	switch (self->state) {
	case AIC_CHASE_WITH_ACTION:
	case AIC_CHASE_WITHOUT_ACTION:
	case AIC_CHASE_WITH_ACTION_BACK:
		if (AIC_CtlByte(self->control, 0x2c)) {
			return 0;
		}
		/* fall through */
	case AIC_IN_ROUTE:
	case AIC_GO_TO:
	case AIC_GET_AWAY:
	case AIC_PATROL_ZONE:
		w = WChar(self->ent);
		return w->control.walk != 0.0f || w->control.strafe != 0.0f;
	}
	return 0;
}
