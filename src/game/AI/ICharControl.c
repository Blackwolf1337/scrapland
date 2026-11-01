/*
 * The move control of the AI of a character, AI/AICharControl.cpp (include/scrap/aicharcontrol.h): construction, the
 * frame of a move (Update and the handler of each mode), the walk along the path, the turn of the view, the strafe
 * around the characters in the way, the follow and the orbit around the target, the debug view, the change of the
 * move (SetMove) and the setup of each kind, and the small accessors.
 */
#include <float.h>
#include <string.h>

#include "scrap/aicharcontrol.h"
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/cstring.h"
#include "scrap/fxmisc.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"

/* the characters near the control (CharsNear, FindAvoid): the sphere test collects them here */
extern cEntity * g_AICCNear[20];	/* 20 */
extern int g_AICCNumNear;
extern cEntity *g_AICCNearSelf;	/* the character itself, left out */
#define g_AICCNearFunc (GAME_ARRAY_IN(cPy_Function, g_ChTypeSphereFunc, 0))
#define AICC_NEAR_NATIVE ((nativefunc_t)AICC_NearChar)	/* AICC_NearChar (its C version behind the hook) */
#define AICC_MAX_NEAR 20

#define AICC_EXE_EMPTY GAME_STR(str_Empty)	/* "" */

/* the unit vector the view of e looks along: (0, 0, -1) turned by its view angles */
static inline vec3_t *AICC_ViewDir(vec3_t *out, cEntity *e) {
	vec3_t fwd = {0.0f, 0.0f, -1.0f};
	vec3_t ang = WChar(e)->view;

	return Vec3_Rotate(out, &fwd, &ang);
}

/* ---- construction ---- */

cAICharControl *cAICharControl_ctor(void *mem, cEntity *ent, cAIChar *ai, int *arg) {
	cAICharControl *self = mem;

	self->vftable = CAICHARCONTROL_VFTABLE;
	cSlotController_ctor(&self->slotWalk, 0.2f);
	cSlotController_ctor(&self->slotSee2, 0.2f);
	cSlotController_ctor(&self->slotStrafe, 0.13333334f);
	cSlotController_ctor(&self->slotStop, 0.2f);
	Nto1Node_Init(&self->link);
	cSlotController_ctor(&self->slotPath, 0.13333334f);
	memset(&self->path, 0, sizeof(self->path));
	cSlotController_ctor(&self->slotDirect, 0.2f);
	cSlotController_ctor(&self->slotSee, 0.2f);
	cSlotController_ctor(&self->slotRepath, 5.0f);
	cSlotController_ctor(&self->slotNear, 0.2f);
	cSlotController_ctor(&self->slotOrbit, 0.2f);
	cSlotController_ctor(&self->slotLost, 3.0f);
	cSlotController_ctor(&self->slotWait, 0.2f);
	memset(&self->debugStrafe, 0, sizeof(self->debugStrafe));
	memset(&self->debugStrafeInner, 0, sizeof(self->debugStrafeInner));
	memset(&self->debugPath, 0, sizeof(self->debugPath));
	if (!ent || !ai) {
		Con_Bluef("ERROR: Inicializaci\xf3n AI Char Control. Entidad = NULL.\n");
		CheckFailed("cAICharControl_Fail_Entity_NULL", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->ent = ent;
	self->ai = ai;
	self->goal.x = self->goal.y = self->goal.z = 0.0f;
	self->dest.x = self->dest.y = self->dest.z = 0.0f;
	self->v1c4.x = self->v1c4.y = self->v1c4.z = 0.0f;
	self->lastTargetPos.x = self->lastTargetPos.y = self->lastTargetPos.z = 0.0f;
	self->ctrl = &WChar(ent)->control;
	self->radius = Obj_Vtbl(ent)->GetRadius(ent);
	self->soft = *arg;
	self->direct = 0;
	self->waitStart = 0;
	self->waitTime = 0.0f;
	self->waitTargetPos.x = self->waitTargetPos.y = self->waitTargetPos.z = 0.0f;
	self->waitPos.x = self->waitPos.y = self->waitPos.z = 0.0f;
	self->unused_140 = 1;
	self->path.maxFree = GAME_GLOBAL(int, cAICharControl_ctor_3a9a2c);
	self->stopped = 0;
	self->orbitSide = 0;
	self->strafeDir = 0;
	self->side = 0;
	self->hold = 0;
	self->link.data = self->ai;
	self->arriveDist = 0.0f;
	self->arriveR = 0.0f;
	self->dt = 0.0f;
	self->avoidTime = 0.0f;
	self->collEventTime = g_TimeEventCollCharAI;
	self->collStopTime = g_TimeStopCollCharAI;
	self->collTestTime = g_TimeTestCollCharAI;
	self->diffY = 0.0f;
	self->diffX = 0.0f;
	self->turnY = 0.0f;
	self->turnX = 0.0f;
	self->turnYNew = 0.0f;
	self->turnXNew = 0.0f;
	self->speed = 0.0f;
	self->prevMode = 0;
	self->mode = 0;
	self->targetChanged = 0;
	self->flying = 0;
	self->newPath = 0;
	self->goalLost = 0;
	self->routeA = 0;
	self->routeB = 1;
	self->walkDir = 0;
	self->prevPoint = NULL;
	self->pathPoint = NULL;
	self->routeNode = NULL;
	self->nearDist = 0.0f;
	self->farDist = 0.0f;
	self->dir.x = self->dir.y = self->dir.z = -1.0f;
	self->routePoint.x = self->routePoint.y = self->routePoint.z = -1.0f;
	self->targetEnt = NULL;
	self->stuck = 0;
	self->avoidChar = 0;
	self->pathRes = 0;
	self->blocked = 0;
	self->blockedByChar = 0;
	self->avoidDir.x = self->avoidDir.y = self->avoidDir.z = 0.0f;
	self->orbitStart = 1;
	self->orbitDir.x = self->orbitDir.y = self->orbitDir.z = 0.0f;
	self->seen = 0;
	self->lifeMark = WChar(self->ent)->wl.life;
	cAICharControl_ClearAvoid(self);
	cAICharControl_SetTurnSpeed(self, self->speed);
	memset(self->debugSegs, 0, sizeof(self->debugSegs));
	return self;
}

/* the node of the avoided character (the same code as Nto1Node_dtor, used by the 2D and 3D destructors) */

/* the debug segment list and its free nodes */
void DListSeg_Free(cNWSegList *list) {
	DListSeg_Clear(list);
	DList_FreePool(list, sizeof(cNWSegNode));
}

void cAICharControl_dtor(cAICharControl *self) {
	self->vftable = CAICHARCONTROL_VFTABLE;
	DListSeg_Free(&self->debugPath);
	NWVecList_Free(&self->debugStrafeInner);
	NWVecList_Free(&self->debugStrafe);
	NWVecList_Free(&self->path);
	Nto1Node_Done(&self->link);
}

byte Vec3_Equal(const vec3_t *a, const vec3_t *b) {
	return a->x == b->x && a->y == b->y && a->z == b->z;
}

cAICharControl *cAICharControl_Delete(cAICharControl *self, byte flags) {
	cAICharControl_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAICHARCONTROL_SIZE);
	}
	return self;
}

/* the same code as NWVecList_Add and NWVecList_Clear */
void DListVec3_Add(cNWVecList *list, const vec3_t *v) {
	NWVecList_Add(list, v);
}

void DListVec3_Clear(cNWVecList *list) {
	NWVecList_Clear(list);
}

void DListSeg_Clear(cNWSegList *list) {
	DList_Clear(list, sizeof(cNWSegNode));
}

/* ---- the checks of a frame ---- */

byte cAICharControl_CheckTarget(cAICharControl *self) {
	if (self->mode >= AICC_FOLLOW && (self->mode <= AICC_FOLLOW_PATH || self->mode == AICC_AWAY)) {
		cEntity *t = AICC_TargetEnt(self);
		if (!t || !t->active) {
			const char *name = self->ent->name;
			int in = AICC_OwnerVtbl(self->ai)->GetInternState(self->ai);
			int st = AICC_OwnerVtbl(self->ai)->GetState(self->ai);
			Con_Bluef("OBJECTIVE_DIED - cAICharControl::CheckIntensive - Entidad: %s - State: %d - StateIntern: %d\n",
				name, st, in);
			AICC_OwnerVtbl(self->ai)->Event(self->ai, 3);
			return 0;
		}
	}
	return 1;
}

byte cAICharControl_CheckPathEnd(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t buf;

	if (self->pathRes && vt->AtPathEnd(self) && vt->GoalAhead(self)) {
		float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
		if (!vt->TestPath(self, AICC_Pos(self, &buf), &self->dest, r)) {
			self->stuck = 1;
			vt->DirectPath(self, &self->dest);
			self->pathRes = 2;
			return 0;
		}
	}
	return 1;
}

/* the goal on to the furthest point of the path the body reaches (the point of the path walked to, else up to
 * three more); 0 when none is free */
static int AICC_SkipPoints(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t buf;
	float r = self->radius;
	const vec3_t *p, *prev;

	if (vt->TestPath(self, AICC_Pos(self, &buf), &self->goal, r)) {
		return 1;
	}
	self->goalLost = 1;
	r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
	if (vt->TestPath(self, AICC_Pos(self, &buf), self->pathPoint, r)) {
		p = self->pathPoint;
		goto found;
	}
	cNWVecNode *node = self->path.iter->next;
	self->path.iter = node;
	if (node == self->path.head) {
		return 0;
	}
	p = &node->v;
	prev = p;
	for (int k = 0; k < 2; k++) {
		r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
		if (vt->TestPath(self, AICC_Pos(self, &buf), p, r)) {
			self->prevPoint = (vec3_t *)prev;
			self->pathPoint = (vec3_t *)p;
			goto found;
		}
		prev = p;
		node = self->path.iter->next;
		self->path.iter = node;
		if (node == self->path.head) {
			return 0;
		}
		p = &node->v;
	}
	return 0;
found:
	self->goal = *p;
	self->newPath = 1;
	return 1;
}

byte cAICharControl_UpdatePath(cAICharControl *self) {
	self->newPath = 0;
	self->goalLost = 0;
	self->prevPoint = NULL;
	self->stuck = 0;
	if (AICC_SkipPoints(self)) {
		return 1;
	}
	return AICC_Vtbl(self)->FindPath(self, &self->dest);
}

byte cAICharControl_UpdateRoutePath(cAICharControl *self) {
	vec3_t ang, out;
	vec3_t fwd = {0.0f, 0.0f, -1.0f};

	self->newPath = 0;
	self->goalLost = 0;
	self->prevPoint = NULL;
	if (AICC_SkipPoints(self)) {
		return 1;
	}
	self->routeA = 0;
	Obj_Vtbl(self->ent)->GetAng(self->ent, &ang);
	self->routePoint = *Vec3_Rotate(&out, &fwd, &ang);
	return AICC_Vtbl(self)->FindRoute(self);
}

byte cAICharControl_UpdateFollowB(cAICharControl *self) {
	return AICC_Vtbl(self)->UpdateFollow(self) != 0;
}

/* the sphere test of CharsNear: the walking characters but the control's own one, up to 20 */
void AICC_NearChar(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = World_FindEntity(name);
	if (e && e != g_AICCNearSelf && Entity_IsType(e, TYEN_WALKCHAR) && g_AICCNumNear < AICC_MAX_NEAR) {
		g_AICCNear[g_AICCNumNear] = e;
		g_AICCNumNear++;
	}
}

/* the walking characters within MarginDetectCollCharAI of the body into g_AICCNear */
static void AICC_FindNear(cAICharControl *self) {
	vec3_t buf;

	g_AICCNumNear = 0;
	g_AICCNearSelf = self->ent;
	cPy_Function_SetNative(g_AICCNearFunc, AICC_NEAR_NATIVE);
	float r = g_MarginDetectCollCharAI + WChar(self->ent)->wl.prim.radius;
	cWorld_SphereCallF(g_pWorld, 0x10, AICC_Pos(self, &buf), r, g_AICCNearFunc, 1);
}

byte cAICharControl_CharsNear(cAICharControl *self) {
	AICC_FindNear(self);
	return g_AICCNumNear > 0;
}

void cAICharControl_AwayCheck(cAICharControl *self) {
	vec3_t fwd, d, b1, b2;

	AICC_ViewDir(&fwd, self->ent);
	cEntity *t = AICC_TargetEnt(self);
	const vec3_t *s = AICC_Pos(self, &b1);
	const vec3_t *p = AICC_EntPos(t, &b2);
	d.x = p->x - s->x;
	d.y = p->y - s->y;
	d.z = p->z - s->z;
	double a = Vec3_Angle(&d, &fwd);
	if ((double)g_OffsetAngOrientCharAI > a) {
		AICC_OwnerVtbl(self->ai)->Event(self->ai, 6);
	}
}

void cAICharControl_DirCheck(cAICharControl *self) {
	vec3_t fwd;

	AICC_ViewDir(&fwd, self->ent);
	double a = Vec3_Angle(&self->dir, &fwd);
	if ((double)g_OffsetAngOrientCharAI > a) {
		AICC_OwnerVtbl(self->ai)->Event(self->ai, 6);
	}
}

byte cAICharControl_CanMove(cAICharControl *self) {
	cSlotController *slot = &self->slotStop;
	vec3_t buf, hit;
	float f;

	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			cNWVecNode *cur = self->path.iter;
			self->stopped = 0;
			if (!cur) {
				return 1;
			}
			const vec3_t *a = &cur->v;
			cNWVecNode *prev = cur->prev;
			self->path.iter = prev;
			self->path.iter = cur;
			if (prev == self->path.head->prev) {
				return 1;
			}
			const vec3_t *b = &prev->v;
			float r = AICC_X87Add(Obj_Vtbl(self->ent)->GetRadius(self->ent), 25.0f);
			float d = AICC_Vtbl(self)->DistSq(self, &self->goal, AICC_Pos(self, &buf));
			if (r * r > d && Engine_TestLine(a, b, &hit, NULL)) {
				r = AICC_X87Add(Obj_Vtbl(self->ent)->GetRadius(self->ent), 25.0f);
				d = AICC_Vtbl(self)->DistSq(self, &hit, AICC_Pos(self, &buf));
				if (r * r > d) {
					AICC_Vtbl(self)->Stop(self);
					self->stopped = 1;
					return 0;
				}
			}
		} while (cSlotController_FireReset(slot, &f));
	}
	return self->stopped == 0;
}

/* ---- the frame ---- */

byte cAICharControl_Update(cAICharControl *self, float dt) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	self->dt = dt;
	if (self->hold) {
		return 1;
	}
	if (!AICC_OwnerVtbl(self->ai)->GetChanged2(self->ai)) {
		if (!AICC_OwnerVtbl(self->ai)->React(self->ai) || !vt->CheckTarget(self)) {
			return 0;
		}
	}
	if (!vt->CanMove(self)) {
		return 1;
	}
	switch (self->mode) {
	case AICC_GO_TO:
		vt->UpdateGoTo(self, dt);
		break;
	case AICC_GO_TO_DIRECT:
		vt->UpdateGoToDirect(self, dt);
		break;
	case AICC_FOLLOW:
		vt->UpdateFollowMode(self, dt);
		break;
	case AICC_FOLLOW_PATH:
		vt->UpdateFollowPath(self, dt);
		break;
	case AICC_DIR:
		vt->UpdateDir(self, dt);
		break;
	case AICC_AWAY:
		vt->UpdateAway(self, dt);
		break;
	case AICC_ROUTE:
		vt->UpdateRoute(self, dt);
		break;
	}
	return 1;
}

/* the turn of one axis to the angle v: v * gain / pi, changed by step at most against the turn of the last frame */
static float AICC_TurnTo(float v, float gain, float step, float turn) {
	float t = v * gain / 3.14159274f;

	if (__builtin_fabs((double)(t - turn)) > (double)step) {
		t = t > turn ? step + turn : turn - step;
	}
	return t;
}

/* the view turned to the direction d: the angles of d against the view angles na */
static void AICC_TurnView(cAICharControl *self, const vec3_t *na, const vec3_t *d, float gain, float step) {
	vec3_t ra;

	Vec3_GetRotAng(&ra, d);
	float v = NormAng(na->x - ra.x);
	self->diffX = v;
	self->turnXNew = AICC_TurnTo(v, gain, step, self->turnX);
	self->turnX = self->turnXNew;
	self->ctrl->vRot = -self->turnXNew;
	v = NormAng(na->y - ra.y);
	self->diffY = v;
	self->turnYNew = AICC_TurnTo(v, gain, step, self->turnY);
	self->turnY = self->turnYNew;
	self->ctrl->hRot = -self->turnYNew;
}

void cAICharControl_TurnFollow(cAICharControl *self) {
	vec3_t ang = WChar(self->ent)->view;
	vec3_t na, d, b1, b2;

	Vec3_NormAng(&na, &ang);
	if (self->avoidDir.x == 0.0f && self->avoidDir.y == 0.0f && self->avoidDir.z == 0.0f) {
		cEntity *t = AICC_TargetEnt(self);
		const vec3_t *tp = AICC_EntPos(t, &b1);
		const vec3_t *s = AICC_Pos(self, &b2);
		d.x = s->x - tp->x;
		d.y = s->y - tp->y;
		d.z = s->z - tp->z;
	} else {
		d = self->avoidDir;
	}
	AICC_TurnView(self, &na, &d, self->viewGain, self->viewStep);
}

/* the segment of the last path check: the character and the goal */
static void AICC_SetCheckSeg(cAICharControl *self) {
	vec3_t buf;

	self->avoidA = *AICC_Pos(self, &buf);
	self->avoidB = self->goal;
}

static inline int AICC_GoalIsDest(const cAICharControl *self) {
	return self->goal.x == self->dest.x && self->goal.y == self->dest.y && self->goal.z == self->dest.z;
}

byte cAICharControl_CheckRoute(cAICharControl *self) {
	cSlotController *slot = &self->slotPath;
	float f;

	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			if (!AICC_Vtbl(self)->UpdateRoutePath(self)) {
				return 0;
			}
			if (!AICC_GoalIsDest(self)) {
				AICC_Vtbl(self)->NextGoal(self);
			}
			AICC_SetCheckSeg(self);
		} while (cSlotController_FireReset(slot, &f));
	}
	return 1;
}

void cAICharControl_DirTurn(cAICharControl *self) {
	vec3_t ang = WChar(self->ent)->view;
	vec3_t na, d, buf;

	Vec3_NormAng(&na, &ang);
	const vec3_t *s = AICC_Pos(self, &buf);
	d.y = s->y - self->goal.y;
	d.z = s->z - self->goal.z;
	d.x = s->x - self->goal.x;
	AICC_TurnView(self, &na, &d, self->turnGain, self->turnStep);
}

/* the step of CheckGoTo and CheckFollow on the path; 0 when no path is left */
static int AICC_CheckStep(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	if (vt->CheckPathEnd(self)) {
		if (!vt->UpdatePath(self)) {
			return 0;
		}
		if (!AICC_GoalIsDest(self)) {
			vt->NextGoalPath(self);
		}
		AICC_SetCheckSeg(self);
	}
	return 1;
}

/* the checks of the path (slotPath); when the way was lost the path to to (NULL: the target) searched again
 * every 3 s */
static byte AICC_CheckPath(cAICharControl *self, int toTarget) {
	cSlotController *slot = &self->slotPath;
	vec3_t buf;
	float f;

	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			if (!AICC_CheckStep(self)) {
				return 0;
			}
		} while (cSlotController_FireReset(slot, &f));
	}
	slot = &self->slotLost;
	if (self->stuck && cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			const vec3_t *to = &self->dest;
			if (toTarget) {
				cEntity *t = AICC_TargetEnt(self);
				to = AICC_EntPos(t, &buf);
			}
			if (!AICC_Vtbl(self)->FindPath(self, to)) {
				return 0;
			}
			if (!AICC_CheckStep(self)) {
				return 0;
			}
		} while (cSlotController_FireReset(slot, &f));
	}
	return 1;
}

byte cAICharControl_CheckGoTo(cAICharControl *self) {
	return AICC_CheckPath(self, 0);
}

byte cAICharControl_CheckFollow(cAICharControl *self) {
	return AICC_CheckPath(self, 1);
}

/* ---- the strafe around the characters in the way ---- */

/* the head of the links to a character (the controls that strafe around it) */
static inline Nto1Node *AICC_CharHead(cEntity *e) {
	return &WLife(e)->targets;
}

/* the name of the action class of a character */
static inline const char *AICC_ClassName(cEntity *e) {
	cActCtrl *ac = WithLife_Vtbl(e)->GetActCtrl(e);
	return ac->cls->name;
}

/* the avoided character becomes e */
static void AICC_LinkAvoid(cAICharControl *self, cEntity *e) {
	if (self->link.target != e) {
		if (self->link.target) {
			Nto1Node_UnlinkChecked(&self->link);
		}
		Nto1Node_LinkTo(AICC_CharHead(e), &self->link);
	}
}

void cAICharControl_UpdateAvoid(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t toE, toGoal;

	self->avoidDir.x = 0.0f;
	self->avoidDir.y = 0.0f;
	self->avoidDir.z = 0.0f;
	cEntity *e = self->link.target;
	if (!e) {
		self->side = 0;
		self->blocked = 0;
	}
	if ((self->side || self->blocked) && e && vt->Near(self, e)) {
		self->side = 0;
		self->blocked = 0;
		self->avoidChar = 0;
	}
	if (!self->side && !self->blocked) {
		self->blocked = 0;
		self->side = 0;
		if (strcmp(Nto1Node_TargetName(&WChar(self->ent)->collide), AICC_EXE_EMPTY) && g_AICCNumNear == 1 && g_AICCNear[0]) {
			cEntity *n = g_AICCNear[0];
			if (self->link.target != n) {
				if (self->link.target) {
					Nto1Node_UnlinkChecked(&self->link);
				}
				const char *cls = AICC_ClassName(n);
				if (strcmp(cls, "GateKeeper") && strcmp(cls, "Sebastian")) {
					Nto1Node_LinkTo(AICC_CharHead(n), &self->link);
				}
			}
		} else if (self->link.target) {
			Nto1Node_UnlinkChecked(&self->link);
		}
	}
	if (!self->link.target) {
		return;
	}
	float keep = vt->KeepDist(self, self->link.target);
	if (vt->InWay(self, self->dest, self->link.target)) {
		if (self->mode != AICC_FOLLOW && self->mode != AICC_FOLLOW_PATH) {
			self->blocked = 1;
			self->blockedByChar = 1;
			self->side = 0;
		}
		return;
	}
	void *eai = ((cEntity *)self->link.target)->vftable->GetAI(self->link.target);
	if (!vt->Facing(self, self->link.target, &toE, &toGoal)) {
		self->blocked = 0;
		self->side = 0;
		return;
	}
	if (self->walkDir == 2) {
		goto blocked;
	}
	if (eai) {
		cAICharControl *other = cAIChar_GetControl(eai);
		if (AICC_Vtbl(other)->Busy(other)) {
			goto blocked;
		}
	}
	self->blocked = 0;
	toE.y = 0.0f;
	toGoal.y = 0.0f;
	self->side = (toGoal.x * toE.z - toE.x * toGoal.z > 0.0f) ? 1 : 2;
	DListVec3_Clear(&self->debugStrafe);
	DListVec3_Clear(&self->debugStrafeInner);
	float sd = vt->StrafeDist(self, self->link.target);
	if (vt->TryStrafe(self, self->side, self->link.target, 1, keep, self->avoidChar, sd, 1)) {
		return;
	}
	self->side = self->side == 1 ? 2 : 1;
	self->avoidDir.x = -toE.x;
	self->avoidDir.y = -toE.y;
	self->avoidDir.z = -toE.z;
	if (vt->TryStrafe(self, self->side, self->link.target, 1, keep, self->avoidChar, sd, 0)) {
		return;
	}
	self->avoidDir.x = 0.0f;
	self->avoidDir.y = 0.0f;
	self->avoidDir.z = 0.0f;
	self->blocked = 1;
	self->blockedByChar = 1;
	self->side = 0;
	return;

blocked:
	self->blocked = 1;
	self->side = 0;
}

void cAICharControl_Strafe2(cAICharControl *self) {
	AICC_Vtbl(self)->Strafe(self);
}

void cAICharControl_StrafeWalk(cAICharControl *self) {
	switch (self->strafeDir) {
	case 1:
		self->ctrl->strafe = self->speed;
		break;
	case 2:
		self->ctrl->strafe = -self->speed;
		break;
	case 0:
		self->ctrl->strafe = 0.0f;
		break;
	}
	self->ctrl->jump = 0.0f;
}

byte cAICharControl_FindAvoid(cAICharControl *self) {
	self->blockedByChar = 0;
	AICC_FindNear(self);
	if (g_AICCNumNear <= 1) {
		return 0;
	}
	DListVec3_Clear(&self->debugStrafe);
	DListVec3_Clear(&self->debugStrafeInner);
	int found = 0;
	for (int i = 0; i < g_AICCNumNear; i++) {
		cEntity *e = g_AICCNear[i];
		byte f = 0;
		if (AICC_Vtbl(self)->Avoid(self, e, &f)) {
			AICC_LinkAvoid(self, e);
			self->avoidChar = 1;
			return 1;
		}
		if (f) {
			found++;
		}
	}
	if (found > 0) {
		self->blocked = 1;
		self->blockedByChar = 1;
	} else {
		self->blocked = 0;
	}
	self->side = 0;
	if (self->link.target) {
		Nto1Node_UnlinkChecked(&self->link);
	}
	self->avoidChar = 0;
	return 1;
}

void cAICharControl_Strafe(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	cSlotController *slot = &self->slotStrafe;
	float dt = self->dt;
	float f;

	self->avoidTime = dt + self->avoidTime;
	self->collEventTime = self->collEventTime + dt;
	self->collStopTime = self->collStopTime + dt;
	self->collTestTime = self->collTestTime + dt;
	if (!cSlotController_Tick(slot, dt) || !cSlotController_FireReset(slot, &f)) {
		return;
	}
	do {
		if (self->collEventTime > g_TimeEventCollCharAI && self->collStopTime > g_TimeStopCollCharAI &&
				self->collTestTime > g_TimeTestCollCharAI) {
			if (self->walkDir == 0) {
				if (self->link.target) {
					Nto1Node_UnlinkChecked(&self->link);
				}
				goto clear;
			}
			self->avoidFound = vt->FindAvoid(self);
			if (!self->avoidFound) {
				vt->UpdateAvoid(self);
			}
			if (self->blocked) {
				self->collStopTime = 0.0f;
				if (self->blockedByChar && self->collEventTime > g_TimeBetweenEventCollCharAI &&
						AICC_OwnerVtbl(self->ai)->Event(self->ai, 10)) {
					self->collEventTime = 0.0f;
				clear:
					self->blocked = 0;
					self->side = 0;
				}
			}
			switch (self->side) {
			case 0:
				self->ctrl->strafe = 0.0f;
				self->avoidTime = 0.0f;
				break;
			case 1:
				self->ctrl->strafe = self->speed * 0.9f;
				self->collTestTime = 0.0f;
				break;
			case 2:
				self->ctrl->strafe = self->speed * -0.9f;
				self->collTestTime = 0.0f;
				break;
			}
		}
		if (AICC_OwnerVtbl(self->ai)->CanJump(self->ai) && self->blocked && !self->side && !vt->SideBlocked(self, 3)) {
			self->ctrl->jump = self->speed;
		} else {
			self->ctrl->jump = 0.0f;
		}
	} while (cSlotController_FireReset(slot, &f));
}

void cAICharControl_Walk2(cAICharControl *self) {
	AICC_Vtbl(self)->WalkFwd(self);
}

/* walkDir between 0 (stand) and 2 (back away) by the distance to the target and the way back */
static void AICC_AwayWalkDir(cAICharControl *self, float dist) {
	cSlotController *slot = &self->slotWalk;
	vec3_t b1, b2;
	float f;

	if (self->walkDir == 0) {
		if (!cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &f)) {
			return;
		}
		do {
			if (self->nearDist > dist) {
				cEntity *t = AICC_TargetEnt(self);
				float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
				const vec3_t *tp = AICC_EntPos(t, &b2);
				if (AICC_Vtbl(self)->TestSides(self, AICC_Pos(self, &b1), tp, r)) {
					self->walkDir = 2;
				}
			}
		} while (cSlotController_FireReset(slot, &f));
	} else if (self->walkDir == 2) {
		if (dist > (self->nearDist + self->farDist) * 0.5f) {
			self->walkDir = 0;
			return;
		}
		if (!cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &f)) {
			return;
		}
		do {
			cEntity *t = AICC_TargetEnt(self);
			float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
			const vec3_t *tp = AICC_EntPos(t, &b2);
			if (!AICC_Vtbl(self)->TestSides(self, AICC_Pos(self, &b1), tp, r)) {
				self->walkDir = 0;
			}
		} while (cSlotController_FireReset(slot, &f));
	}
}

void cAICharControl_AwayWalk(cAICharControl *self) {
	cSlotController *slot = &self->slotSee2;
	vec3_t b1, b2;
	float f;

	if (!self->awayFlag) {
		self->ctrl->walk = 0.0f;
		return;
	}
	cEntity *t = AICC_TargetEnt(self);
	const vec3_t *s = AICC_Pos(self, &b1);
	float dist = AICC_Vtbl(self)->Dist(self, AICC_EntPos(t, &b2), s);
	AICC_AwayWalkDir(self, dist);
	if (self->walkDir == 0) {
		self->ctrl->walk = 0.0f;
	} else if (self->walkDir == 2) {
		self->ctrl->walk = -self->speed;
	}
	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			self->seen = 0;
			if (!AICC_Vtbl(self)->CharsNear(self)) {
				continue;
			}
			if (g_AICCNumNear <= 1 && g_AICCNear[0] == ChType_Usr0()) {
				continue;
			}
			for (int i = 0; i < g_AICCNumNear; i++) {
				cEntity *usr = ChType_Usr0();
				if (g_AICCNear[i] != usr && AICC_Vtbl(self)->InFront(self, g_AICCNear[i], 0.785398185f)) {
					self->seen = 1;
					break;
				}
			}
		} while (cSlotController_FireReset(slot, &f));
	}
	if (self->seen) {
		self->ctrl->walk = 0.0f;
	}
}

void cAICharControl_WalkFwd(cAICharControl *self) {
	self->walkDir = 1;
	if (!self->blocked && !self->stuck) {
		self->ctrl->walk = self->speed;
	} else {
		self->ctrl->walk = 0.0f;
	}
}

/* ---- the wait and the orbit around the target ---- */

/* (m - a) - k in the x87 precision the game set */
static inline float AICC_X87SubRSub(float a, float m, float k) {
	float r;

	__asm__("fsubrs %2\n\tfsubs %3" : "=t"(r) : "0"(a), "m"(m), "m"(k));
	return r;
}

/* (x, 0, z) made a unit vector, the length from the squares of x and z */
static inline vec3_t AICC_FlatNorm(float x, float z) {
	vec3_t r;
	float len = (float)__builtin_sqrt((double)(x * x + 0.0f + z * z));

	if (len == 0.0f) {
		r.x = x;
		r.y = 0.0f;
		r.z = z;
	} else {
		float inv = 1.0f / len;
		r.x = x * inv;
		r.y = inv * 0.0f;
		r.z = inv * z;
	}
	return r;
}

/* the name of the action of layer 0 of the character */
static inline const char *AICC_ActionName(cEntity *e) {
	return cActCtrl_GetActionName(WithLife_Vtbl(e)->GetActCtrl(e), 0);
}

byte cAICharControl_CheckWait(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t d, u, tmp, b1, b2;
	float f;

	if (self->waitTime == 0.0f && self->direct) {
		goto start;
	}
	if (self->waitStart && strcmp(AICC_ActionName(self->ent), "ActionInit") &&
			strcmp(AICC_ActionName(self->ent), "ActionEnd")) {
		if (self->waitTime == 0.0f) {
			byte inLine;
			float c2;
			d.x = self->waitTargetPos.x - self->waitPos.x;
			d.y = 0.0f;
			d.z = self->waitTargetPos.z - self->waitPos.z;
			u = *Vec3_NormalizeTo(&tmp, &d);
			cEntity *t = AICC_TargetEnt(self);
			const vec3_t *tp = AICC_EntPos(t, &b1);
			if (5.0 > __builtin_fabs((double)(tp->x - self->waitTargetPos.x)) &&
					5.0 > __builtin_fabs((double)(tp->y - self->waitTargetPos.y)) &&
					5.0 > __builtin_fabs((double)(tp->z - self->waitTargetPos.z))) {
				/* the target stays: the side of the character against the line from waitPos */
				const vec3_t *s = AICC_Pos(self, &b1);
				d.x = s->x - self->waitPos.x;
				d.y = 0.0f;
				d.z = s->z - self->waitPos.z;
				const vec3_t *v = Vec3_NormalizeTo(&tmp, &d);
				float c1 = u.z * v->y - v->z * u.y;
				c2 = v->z * u.x - v->x * u.z;
				float c3 = v->x * u.y - u.x * v->y;
				inLine = 0.75f > c2 * c2 + c1 * c1 + c3 * c3;
			} else {
				/* the target moved: its way against the line back */
				u.x = -u.x;
				u.y = -u.y;
				u.z = -u.z;
				tp = AICC_EntPos(AICC_TargetEnt(self), &b1);
				d.x = tp->x - self->waitTargetPos.x;
				d.y = 0.0f;
				d.z = tp->z - self->waitTargetPos.z;
				const vec3_t *v = Vec3_NormalizeTo(&tmp, &d);
				float c1 = v->z * u.y - u.z * v->y;
				c2 = v->x * u.z - v->z * u.x;
				float c3 = u.x * v->y - v->x * u.y;
				inLine = 0.75f > c2 * c2 + c1 * c1 + c3 * c3;
			}
			if (c2 > 0.0f && !inLine) {
				self->strafeDir = 1;
			} else if (0.0f > c2 && !inLine) {
				self->strafeDir = 2;
			} else {
				goto lost;
			}
		}
		cSlotController *slot = &self->slotWait;
		if (self->waitTime > 0.0f && cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
			do {
				cEntity *t = AICC_TargetEnt(self);
				float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
				const vec3_t *tp = AICC_EntPos(t, &b1);
				if (vt->TestPath(self, AICC_Pos(self, &b2), tp, r) || vt->CharsNear(self) ||
						vt->SideBlocked(self, self->strafeDir)) {
					goto lost;
				}
			} while (cSlotController_FireReset(slot, &f));
		}
		if (self->waitTime > g_TimeStrafeLostObjCharAI) {
			goto lost;
		}
		return 1;
	lost:
		self->waitStart = 0;
		return 0;
	}
	if (!self->direct) {
		goto done;
	}
start:
	self->waitPos = *AICC_Pos(self, &b1);
	self->waitTargetPos = *AICC_EntPos(AICC_TargetEnt(self), &b1);
	self->waitStart = 1;
done:
	self->waitTime = 0.0f;
	return 0;
}

void cAICharControl_Orbit(cAICharControl *self) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t d, w, out, b1, b2;
	vec3_t fwd = {0.0f, 0.0f, -1.0f};
	float f, v;

	vt->Strafe2(self);
	if (self->side || self->blocked) {
		return;
	}
	cEntity *t = AICC_TargetEnt(self);
	const vec3_t *tp = AICC_EntPos(t, &b1);
	float dist = vt->Dist(self, AICC_Pos(self, &b2), tp);
	if (self->orbitSide) {
		if (!self->direct || dist > self->farDist + self->farDist) {
			self->orbitSide = 0;
		} else {
			tp = AICC_EntPos(AICC_TargetEnt(self), &b1);
			const vec3_t *s = AICC_Pos(self, &b2);
			d.x = s->x - tp->x;
			d.y = 0.0f;
			d.z = s->z - tp->z;
			if ((double)g_AngAttackBackCharAI > Vec3_Angle(&d, &self->orbitDir)) {
				self->orbitSide = 0;
			} else {
				vec3_t ang = WChar(AICC_TargetEnt(self))->view;
				const vec3_t *tv = Vec3_Rotate(&out, &fwd, &ang);
				w.x = -tv->x;
				w.z = -tv->z;
				w.y = 0.0f;
				if ((double)g_AngAttackBackCharAI > Vec3_Angle(&d, &w)) {
					self->orbitSide = 0;
				}
			}
		}
	}
	if (!self->orbitStart && (!self->direct || dist > self->farDist * 1.1f)) {
		self->orbitStart = 1;
	}
	if (self->walkDir == 0 && self->orbitSide == 0 && self->orbitStart) {
		vt->StartOrbit(self, dist - 4.0f);
		self->orbitStart = 0;
		self->orbitDir = WChar(AICC_TargetEnt(self))->view;
		const vec3_t *tv = Vec3_Rotate(&out, &fwd, &self->orbitDir);
		self->orbitDir.x = -tv->x;
		self->orbitDir.y = -tv->y;
		self->orbitDir.z = -tv->z;
		self->orbitDir.y = 0.0f;
	}
	cSlotController *slot = &self->slotOrbit;
	if (cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &f)) {
		do {
			if (vt->SideBlocked(self, self->orbitSide)) {
				self->orbitSide = 0;
			}
		} while (cSlotController_FireReset(slot, &f));
	}
	/* orbitSide is 0, 1 or 2 (the original strafes by a stale local for another value) */
	if (self->orbitSide == 0) {
		v = 0.0f;
	} else {
		v = self->orbitSide == 1 ? self->speed : -self->speed;
		if (v != 0.0f) {
			if (self->walkDir == 0) {
				if (Vec3_Equal(&self->lastTargetPos, AICC_EntPos(AICC_TargetEnt(self), &b1))) {
					self->turnGain = 25.0f;
					self->turnStep = 2.0f;
				} else {
					vt->SetTurnSpeed(self, self->speed);
				}
			} else if (self->walkDir == 1 || self->walkDir == 2) {
				vt->SetTurnSpeed(self, self->speed);
			}
		}
	}
	self->ctrl->strafe = v;
	self->ctrl->jump = 0.0f;
	self->lastTargetPos = *AICC_EntPos(AICC_TargetEnt(self), &b1);
}

byte cAICharControl_StartOrbit(cAICharControl *self, float dist) {
	vec3_t r, wv, p, out, ang, rot, b1, b2;
	vec3_t fwd = {0.0f, 0.0f, -1.0f};

	self->orbitSide = 0;
	const vec3_t *tp = AICC_EntPos(AICC_TargetEnt(self), &b1);
	const vec3_t *s = AICC_Pos(self, &b2);
	vec3_t n = AICC_FlatNorm(s->x - tp->x, s->z - tp->z);
	r.x = n.x * dist;
	r.y = n.y * dist;
	r.z = n.z * dist;
	ang = WChar(AICC_TargetEnt(self))->view;
	float sgn = -1.0f;
	const vec3_t *tv = Vec3_Rotate(&out, &fwd, &ang);
	n = AICC_FlatNorm(-tv->x, -tv->z);
	wv.x = n.x * dist;
	wv.y = n.y * dist;
	wv.z = n.z * dist;
	if (!(Vec3_Angle(&r, &wv) > (double)g_AngAttackBackCharAI)) {
		return 1;
	}
	byte left;
	if (wv.x * r.z - wv.z * r.x > 0.0f) {
		left = 1;
	} else {
		sgn = 1.0f;
		left = 0;
	}
	DListVec3_Clear(&self->debugStrafe);
	rot.x = 0.0f;
	rot.y = g_AngAttackBackCharAI * sgn;
	rot.z = 0.0f;
	Vec3_Rotate(&p, &wv, &rot);
	tp = AICC_EntPos(AICC_TargetEnt(self), &b1);
	if (!AICC_Vtbl(self)->StrafeBack(self, tp, &r, &p, dist, left, 1)) {
		left ^= 1;
		rot.x = 0.0f;
		rot.y = -sgn * g_AngAttackBackCharAI;
		rot.z = 0.0f;
		Vec3_Rotate(&p, &wv, &rot);
		float a = (float)Vec3_Angle(&r, &wv);
		byte flag = 3.14159274f > AICC_X87SubRSub(a, 6.28318548f, g_AngAttackBackCharAI);
		tp = AICC_EntPos(AICC_TargetEnt(self), &b1);
		if (!AICC_Vtbl(self)->StrafeBack(self, tp, &r, &p, dist, left, flag)) {
			left ^= 1;
		}
	}
	self->orbitSide = left ? 2 : 1;
	return 1;
}

/* ---- the debug view ---- */


/* ShowPathAI: the corridor of the last path test, the view, the goal, the velocity and the path */
static void AICC_DrawPath(cAICharControl *self) {
	vec3_t pos, dir, q, vel, n, prev;

	for (int i = 0; i < 9; i++) {
		DebugUtils_DrawLine(&self->debugSegs[i].a, &self->debugSegs[i].b, 0xff0000);
	}
	AICC_Pos(self, &pos);
	AICC_ViewDir(&dir, self->ent);
	q.x = pos.x + dir.x * 200.0f;
	q.y = pos.y + dir.y * 200.0f;
	q.z = pos.z + dir.z * 200.0f;
	DebugUtils_DrawLine(&pos, &q, 0xff00ff);
	if (!(self->goal.x == -FLT_MAX && self->goal.y == -FLT_MAX && self->goal.z == -FLT_MAX)) {
		DebugUtils_DrawLine(&pos, &self->goal, 0xffff);
	}
	Obj_Vtbl(self->ent)->GetVel(self->ent, &vel);
	float s = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z;
	if (s > 1000.0f) {
		float len = (float)__builtin_sqrt((double)s);
		if (len == 0.0f) {
			n = vel;
		} else {
			float inv = 1.0f / len;
			n.x = vel.x * inv;
			n.y = vel.y * inv;
			n.z = vel.z * inv;
		}
		vel.x = n.x * 200.0f + pos.x;
		vel.y = n.y * 200.0f + pos.y;
		vel.z = n.z * 200.0f + pos.z;
		DebugUtils_DrawLine(&pos, &vel, 0xffff00);
	}
	if (!self->path.head) {
		return;
	}
	cNWVecNode *saved = self->path.iter;
	cNWVecNode *node = self->path.head;
	self->path.iter = node;
	const vec3_t *p = &node->v;
	prev = *p;
	for (;;) {
		DebugUtils_DrawSphere(p, 50.0f, 0xffffff, 2, 2);
		if (!(p->x == prev.x && p->y == prev.y && p->z == prev.z)) {
			DebugUtils_DrawLine(&prev, p, 0xffffff);
		}
		prev = *p;
		node = self->path.iter->next;
		self->path.iter = node;
		if (node == self->path.head) {
			break;
		}
		p = &node->v;
	}
	self->path.iter = saved;
}

/* ShowShirkAI: the points of the strafe tests (debugStrafe the radius of the character, debugStrafeInner smaller) */
static void AICC_DrawShirk(cAICharControl *self) {
	const char *dbg = g_NameDebugShirkAI;

	if (strcmp(dbg, self->ent->name) && strcmp(dbg, AICC_EXE_EMPTY)) {
		return;
	}
	if (self->debugStrafe.head) {
		cNWVecNode *node = self->debugStrafe.head->prev;
		self->debugStrafe.iter = node;
		for (int i = 0; node; i++) {
			uint32_t color = i == 0 ? 0xffffff : 0xff;
			float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
			DebugUtils_DrawSphere(&node->v, r, color, 20, 20);
			node = self->debugStrafe.iter->prev;
			self->debugStrafe.iter = node;
			if (node == self->debugStrafe.head->prev) {
				break;
			}
		}
	}
	if (self->debugStrafeInner.head) {
		cNWVecNode *node = self->debugStrafeInner.head->prev;
		self->debugStrafeInner.iter = node;
		while (node) {
			float r = WChar(self->ent)->wl.prim.radius - (g_MarginCollCharAI + 5.0f);
			DebugUtils_DrawSphere(&node->v, r, 0xff00, 20, 20);
			node = self->debugStrafeInner.iter->prev;
			self->debugStrafeInner.iter = node;
			if (node == self->debugStrafeInner.head->prev) {
				break;
			}
		}
	}
}

void cAICharControl_Draw(cAICharControl *self) {
	vec3_t buf;

	if (g_ShowPathAI == 1) {
		AICC_DrawPath(self);
	}
	if (cString_EqStr(&g_ShowCharAI, self->ent->name)) {
		CFont *font = g_FontTerminal8;
		CFont_PrintfColor(font, 10.0f, 10.0f, 0xffffff, "ENTIDAD: %s", self->ent->name);
		const vec3_t *v = Obj_Vtbl(self->ent)->GetVel(self->ent, &buf);
		float len = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 30.0f, 0xffff00, "Velocity (Km/h): %.2f", (double)(len * 0.036f));
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 50.0f, 0xffff00, "Control Turn Horizontal: %.2f",
			(double)self->ctrl->hRot);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 70.0f, 0xffff00, "Control Turn Vertical: %.2f",
			(double)self->ctrl->vRot);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 90.0f, 0xffff00, "Control Strafe Horizontal: %.2f",
			(double)self->ctrl->strafe);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 130.0f, 0xffff00, "Control Walk: %.2f", (double)self->ctrl->walk);
	}
	if (g_NWDebugSegs) {
		cNWSegNode *node = self->debugPath.head;
		self->debugPath.iter = node;
		while (node) {
			DebugUtils_DrawLine(&node->a, &node->b, 0xff0000);
			node = self->debugPath.iter->next;
			self->debugPath.iter = node;
			if (node == self->debugPath.head) {
				break;
			}
		}
	}
	if (g_ShowShirkAI == 1) {
		AICC_DrawShirk(self);
	}
}

byte cAICharControl_DirectPath(cAICharControl *self, const vec3_t *p) {
	vec3_t pos;

	self->dest = *p;
	AICC_Pos(self, &pos);
	DListVec3_Clear(&self->path);
	DListVec3_Add(&self->path, &pos);
	DListVec3_Add(&self->path, &self->dest);
	vec3_t *pt = NULL;
	if (self->path.head) {
		self->path.iter = self->path.head->prev;
		if (self->path.iter) {
			pt = &self->path.iter->v;
		}
	}
	self->pathPoint = pt;
	self->goal = self->dest;
	return 1;
}

/* ---- the goal along the path ---- */

void cAICharControl_Stop(cAICharControl *self) {
	cWalkCharControl *c = self->ctrl;

	c->walk = 0.0f;
	c->vRot = 0.0f;
	c->hRot = 0.0f;
	c->strafe = 0.0f;
	c->jump = 0.0f;
}

byte cAICharControl_Vf89(cAICharControl *self) {
	(void)self;
	return 0;
}

void cAICharControl_RestartSlots(cAICharControl *self) {
	cSlotController_Restart(&self->slotWalk);
	cSlotController_Restart(&self->slotStrafe);
	cSlotController_Restart(&self->slotWait);
	cSlotController_Restart(&self->slotPath);
	cSlotController_Restart(&self->slotStop);
	cSlotController_Restart(&self->slotLost);
	cSlotController_Restart(&self->slotDirect);
	cSlotController_Restart(&self->slotSee);
	cSlotController_Restart(&self->slotRepath);
	cSlotController_Restart(&self->slotNear);
	cSlotController_Restart(&self->slotOrbit);
}

int cAICharControl_GetWalkDir(cAICharControl *self) {
	return self->walkDir;
}

void *cAICharControl_GetAvoidEnt(cAICharControl *self) {
	return self->link.target;
}

/* all three components of a and b within 50 */
static inline int AICC_Close50(const vec3_t *a, const vec3_t *b) {
	return 50.0 > __builtin_fabs((double)(a->x - b->x)) && 50.0 > __builtin_fabs((double)(a->y - b->y)) &&
		50.0 > __builtin_fabs((double)(a->z - b->z));
}

/* the goal moved from the reachable goal towards the point p the body does not reach, halved while the angle at
 * the character stays above MinAngleSearchPointCharAI and the step above 50; once (every time a new point was taken
 * without countAlways) the middle is reachable the search ends */
static void AICC_Bisect(cAICharControl *self, const vec3_t *p, int countAlways) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t a, b, lo, hi, b1, b2;

	vec3_t g0 = self->goal;
	const vec3_t *s = AICC_Pos(self, &b1);
	a.x = self->goal.x - s->x;
	a.y = self->goal.y - s->y;
	a.z = self->goal.z - s->z;
	s = AICC_Pos(self, &b2);
	b.x = p->x - s->x;
	b.y = p->y - s->y;
	b.z = p->z - s->z;
	if (!(Vec3_Angle(&b, &a) > (double)g_MinAngleSearchPointCharAI) || AICC_Close50(p, &self->goal)) {
		return;
	}
	lo = g0;
	hi = *p;
	for (int k = 0; k < 1;) {
		self->goal.x = (lo.x + hi.x) * 0.5f;
		self->goal.y = (lo.y + hi.y) * 0.5f;
		self->goal.z = (lo.z + hi.z) * 0.5f;
		s = AICC_Pos(self, &b2);
		a.x = self->goal.x - s->x;
		a.y = self->goal.y - s->y;
		a.z = self->goal.z - s->z;
		s = AICC_Pos(self, &b1);
		b.x = lo.x - s->x;
		b.y = lo.y - s->y;
		b.z = lo.z - s->z;
		if (!(Vec3_Angle(&b, &a) > (double)g_MinAngleSearchPointCharAI) || AICC_Close50(&lo, &self->goal)) {
			self->goal = lo;
			return;
		}
		float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
		if (vt->TestPath(self, AICC_Pos(self, &b1), &self->goal, r)) {
			lo = self->goal;
			if (countAlways || !self->newPath) {
				k++;
			}
		} else {
			hi = self->goal;
		}
	}
}

/* the point after the iterator of the path (towards the end), NULL at the end */
static inline vec3_t *AICC_IterNext(cAICharControl *self) {
	cNWVecNode *node = self->path.iter->prev;

	self->path.iter = node;
	return node != self->path.head->prev ? &node->v : NULL;
}

void cAICharControl_NextGoal(cAICharControl *self) {
	vec3_t b1, b2;

	float px = AICC_Pos(self, &b1)->x;
	const vec3_t *q = AICC_Pos(self, &b2);
	float k = g_DistNextPosGoTargetCharAI;
	byte near = (double)k > __builtin_fabs((double)(self->goal.x - px)) &&
		(double)k > __builtin_fabs((double)(self->goal.z - q->z));
	if (!self->goalLost && !near) {
		return;
	}
	cNWVecNode *saved = self->path.iter;
	vec3_t *p = AICC_IterNext(self);
	if (!p) {
		self->path.iter = saved;
		self->goal = self->dest;
		return;
	}
	float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
	if (AICC_Vtbl(self)->TestPath(self, AICC_Pos(self, &b1), p, r)) {
		self->pathPoint = p;
		self->goal = *p;
		cNWVecNode *at = self->path.iter;
		vec3_t *n = AICC_IterNext(self);
		if (n) {
			AICC_Vtbl(self)->StepGoal(self, n);
		}
		self->path.iter = at;
		return;
	}
	AICC_Bisect(self, p, 0);
	self->path.iter = saved;
}

void cAICharControl_NextGoalPath(cAICharControl *self) {
	vec3_t buf;
	byte moved = 0;

	cNWVecNode *saved = self->path.iter;
	vec3_t *p = AICC_IterNext(self);
	while (p) {
		float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
		if (!AICC_Vtbl(self)->TestPath(self, AICC_Pos(self, &buf), p, r)) {
			if (moved) {
				AICC_Vtbl(self)->StepGoal(self, p);
			}
			AICC_Bisect(self, p, 1);
			self->path.iter = saved;
			return;
		}
		self->pathPoint = p;
		self->goal = *p;
		saved = self->path.iter;
		cNWVecNode *node = saved->prev;
		self->path.iter = node;
		if (node == self->path.head->prev) {
			break;
		}
		moved = 1;
		p = &node->v;
	}
	self->path.iter = saved;
	self->goal = self->dest;
}

void cAICharControl_GetGoal(cAICharControl *self, vec3_t *out) {
	*out = self->goal;
}

void cAICharControl_GetDest(cAICharControl *self, vec3_t *out) {
	*out = self->dest;
}

byte cAICharControl_GoalOnLine(cAICharControl *self, const vec3_t *a, const vec3_t *b) {
	vec3_t d, u, pt, buf;

	if (AICC_Close50(a, b)) {
		return 0;
	}
	d.x = b->x - a->x;
	d.y = b->y - a->y;
	d.z = b->z - a->z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	int n = g_NumPointsSearchSegmentCharAI;
	float step = len / (float)(n + 1);
	if (len == 0.0f) {
		u = d;
	} else {
		float inv = 1.0f / len;
		u.x = d.x * inv;
		u.y = d.y * inv;
		u.z = d.z * inv;
	}
	float sx = u.x * step;
	float sy = u.y * step;
	float sz = u.z * step;
	for (int i = 1; i <= g_NumPointsSearchSegmentCharAI; i++) {
		float fi = (float)i;
		pt.x = a->x + sx * fi;
		pt.y = sy * fi + a->y;
		pt.z = a->z + sz * fi;
		float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
		if (AICC_Vtbl(self)->TestPath(self, AICC_Pos(self, &buf), &pt, r)) {
			self->goal = pt;
			return 1;
		}
	}
	return 0;
}

void cAICharControl_GetAvoidB(cAICharControl *self, vec3_t *out) {
	*out = self->avoidB;
}

void cAICharControl_GetAvoidA(cAICharControl *self, vec3_t *out) {
	*out = self->avoidA;
}

int cAICharControl_Busy(cAICharControl *self) {
	return self->side;
}

int cAICharControl_GetOrbitSide(cAICharControl *self) {
	return self->orbitSide;
}

vec3_t *cAICharControl_SideDir(cAICharControl *self, vec3_t *out, int side) {
	vec3_t c, buf;

	if (!side) {
		out->z = 0.0f;
		out->y = 0.0f;
		out->x = 0.0f;
		return out;
	}
	const vec3_t *s = AICC_Pos(self, &buf);
	float gy = self->goal.y - s->y;
	float gx = self->goal.x - s->x;
	float gz = self->goal.z - s->z;
	float t1 = gy * 0.0f;
	float t2 = gx * 0.0f;
	c.z = gx - t1;
	c.x = t1 - gz;
	c.y = gz * 0.0f - t2;
	if (side == 1) {
		c.x = -c.x;
		c.y = -c.y;
		c.z = -c.z;
	}
	float len = (float)__builtin_sqrt((double)(c.y * c.y + c.x * c.x + c.z * c.z));
	if (len != 0.0f) {
		float inv = 1.0f / len;
		c.x = c.x * inv;
		c.y = c.y * inv;
		c.z = c.z * inv;
	}
	*out = c;
	return out;
}

byte cAICharControl_GoalAtStart(cAICharControl *self) {
	const vec3_t *p = NULL;

	if (self->path.head && self->path.head->prev) {
		p = &self->path.head->prev->v;
	}
	return self->goal.x == p->x && self->goal.y == p->y && self->goal.z == p->z;
}

byte cAICharControl_AtPathEnd(cAICharControl *self) {
	cNWVecNode *node = self->path.head->next;

	if (node == self->path.head) {
		return self->pathPoint == NULL;
	}
	return &node->v == self->pathPoint;
}

byte cAICharControl_IsDirect(cAICharControl *self) {
	return self->direct;
}

/* the follow of a frame (FOLLOW: update the follow, FOLLOW_PATH: the one of slot 0x178); straight to the target
 * it orbits (FOLLOW_PATH) or strafes, else it walks along the path */
static byte AICC_FollowFrame(cAICharControl *self, float dt, int path) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	if (self->path.count <= 0) {
		return 1;
	}
	if (!(path ? vt->UpdateFollowB(self) : vt->UpdateFollow(self))) {
		return 0;
	}
	if (vt->Wait(self, dt)) {
		return 1;
	}
	if (!self->direct) {
		if (!vt->CheckFollow(self)) {
			return 0;
		}
	} else {
		vt->ClearAvoid(self);
	}
	if (self->direct) {
		if (path) {
			vt->Orbit(self);
		} else {
			vt->Strafe2(self);
		}
		vt->TurnFollow(self);
		vt->Walk(self);
		return 1;
	}
	vt->Strafe(self);
	vt->Turn(self);
	vt->Walk2(self);
	return 1;
}

byte cAICharControl_UpdateFollowPath(cAICharControl *self, float dt) {
	return AICC_FollowFrame(self, dt, 1);
}

byte cAICharControl_UpdateFollowMode(cAICharControl *self, float dt) {
	return AICC_FollowFrame(self, dt, 0);
}

byte cAICharControl_UpdateAway(cAICharControl *self, float dt) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t buf;

	(void)dt;
	self->goal = *AICC_EntPos(AICC_TargetEnt(self), &buf);
	self->dest = self->goal;
	vt->AwayCheck(self);
	if (!AICC_OwnerVtbl(self->ai)->GetChanged(self->ai)) {
		vt->TurnFollow(self);
		self->ctrl->strafe = 0.0f;
		self->ctrl->jump = 0.0f;
		vt->AwayWalk(self);
	}
	return 1;
}

/* GO_TO, GO_TO_DIRECT and ROUTE: the arrival, the check of the path (none for GO_TO_DIRECT), the strafe, the turn
 * and the walk */
static byte AICC_PathFrame(cAICharControl *self, byte(*check)(cAICharControl *self)) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	if (self->path.count > 0) {
		vt->CheckArrived(self);
		if (!AICC_OwnerVtbl(self->ai)->GetChanged(self->ai)) {
			if (check && !check(self)) {
				return 0;
			}
			vt->Strafe(self);
			vt->Turn(self);
			vt->WalkFwd(self);
		}
	}
	return 1;
}

byte cAICharControl_UpdateRoute(cAICharControl *self, float dt) {
	(void)dt;
	return AICC_PathFrame(self, AICC_Vtbl(self)->CheckRoute);
}

byte cAICharControl_UpdateDir(cAICharControl *self, float dt) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	(void)dt;
	vt->DirCheck(self);
	if (!AICC_OwnerVtbl(self->ai)->GetChanged(self->ai)) {
		vt->DirTurn(self);
		self->ctrl->strafe = 0.0f;
		self->ctrl->jump = 0.0f;
		vt->WalkFwd(self);
	}
	return 1;
}

byte cAICharControl_UpdateGoTo(cAICharControl *self, float dt) {
	(void)dt;
	return AICC_PathFrame(self, AICC_Vtbl(self)->CheckGoTo);
}

byte cAICharControl_UpdateGoToDirect(cAICharControl *self, float dt) {
	(void)dt;
	return AICC_PathFrame(self, NULL);
}

byte cAICharControl_Wait(cAICharControl *self, float dt) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);

	byte r = vt->CheckWait(self);
	if (!r) {
		return r;
	}
	vt->StrafeWalk(self);
	vt->TurnFollow(self);
	self->ctrl->walk = 0.0f;
	self->waitTime = self->waitTime + dt;
	return 1;
}

vec3_t *Vec3_NormalizeTo(vec3_t *out, const vec3_t *in) {
	float len = (float)__builtin_sqrt((double)(in->x * in->x + in->y * in->y + in->z * in->z));
	vec3_t r;

	if (len == 0.0f) {
		r = *in;
	} else {
		float inv = 1.0f / len;
		r.x = in->x * inv;
		r.y = in->y * inv;
		r.z = in->z * inv;
	}
	*out = r;
	return out;
}

void cAICharControl_ClearAvoid(cAICharControl *self) {
	self->avoidA.x = -FLT_MAX;
	self->avoidA.y = -FLT_MAX;
	self->avoidA.z = -FLT_MAX;
	self->avoidB.x = -FLT_MAX;
	self->avoidB.y = -FLT_MAX;
	self->avoidB.z = -FLT_MAX;
}

byte cAICharControl_Avoid(cAICharControl *self, cEntity *e, byte *found) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	vec3_t toE, toGoal;
	byte res = 0;
	int side;

	self->avoidDir.x = 0.0f;
	self->avoidDir.y = 0.0f;
	self->avoidDir.z = 0.0f;
	*found = 0;
	float keep = vt->KeepDist(self, e);
	if (vt->InWay(self, self->dest, e)) {
		if (self->mode != AICC_FOLLOW && self->mode != AICC_FOLLOW_PATH) {
			self->side = 0;
			self->blocked = 1;
			self->blockedByChar = 1;
			return 1;
		}
		return res;
	}
	void *eai = e->vftable->GetAI(e);
	if (!vt->Facing(self, e, &toE, &toGoal)) {
		return res;
	}
	*found = 1;
	if (self->walkDir == 2) {
		goto blocked;
	}
	if (eai) {
		cAICharControl *other = cAIChar_GetControl(eai);
		if (AICC_Vtbl(other)->Busy(other)) {
			goto blocked;
		}
	}
	toE.y = 0.0f;
	toGoal.y = 0.0f;
	float cross = toGoal.x * toE.z - toE.x * toGoal.z;
	if (cross > 0.0f) {
		side = 1;
	} else if (0.0f >= cross) {
		side = 2;
	} else {
		/* NaN: the original keeps the register with e */
		side = (int)(uintptr_t)e;
	}
	float sd = vt->StrafeDist(self, e);
	if (!vt->TryStrafe(self, side, e, 1, keep, 1, sd, 1)) {
		side = side == 1 ? 2 : 1;
		if (!vt->TryStrafe(self, side, e, 1, keep, 1, sd, 0)) {
			return res;
		}
		self->avoidDir.x = -toE.x;
		self->avoidDir.y = -toE.y;
		self->avoidDir.z = -toE.z;
	}
	self->side = side;
	self->blocked = 0;
	return 1;

blocked:
	self->side = 0;
	self->blocked = 1;
	return 1;
}

/* the common part of Follow and FollowPath after the target is set */
static byte AICC_FollowStart(cAICharControl *self, Nto1Node *target) {
	vec3_t t;

	self->target = target;
	self->targetEnt = target->target;
	float r = Obj_Vtbl(target->target)->GetRadius(target->target);
	self->arriveDist = r;
	self->arriveR = Float_MinSS(self->radius, r);
	cEntity *te = AICC_TargetEnt(self);
	byte ok = AICC_Vtbl(self)->FindPath(self, Obj_Vtbl(te)->base.GetPos(te, &t));
	if (!ok) {
		return ok;
	}
	if (self->goal.x == self->dest.x && self->goal.y == self->dest.y && self->goal.z == self->dest.z) {
		self->direct = 1;
		return 1;
	}
	self->direct = 0;
	return 1;
}

byte cAICharControl_FollowPath(cAICharControl *self, Nto1Node *target, float near, float far, float speed) {
	CHECK(target != NULL);
	CHECK(target->target != NULL);
	self->nearDist = near;
	self->farDist = far;
	AICC_Vtbl(self)->SetSpeed(self, speed);
	if (self->mode == self->prevMode && self->targetEnt == target->target) {
		return 1;
	}
	self->targetChanged = 1;
	return AICC_FollowStart(self, target);
}

byte cAICharControl_Follow(cAICharControl *self, Nto1Node *target, float near, float far, float speed) {
	CHECK(target != NULL);
	CHECK(target->target != NULL);
	self->nearDist = near;
	self->farDist = far;
	AICC_Vtbl(self)->SetSpeed(self, speed);
	if (self->mode == self->prevMode && self->targetEnt == target->target) {
		return 1;
	}
	return AICC_FollowStart(self, target);
}

byte cAICharControl_SetMove(cAICharControl *self, int mode, ...) {
	const cAICharControlVtbl *vt = AICC_Vtbl(self);
	va_list ap;
	byte r = 1;

	va_start(ap, mode);
	self->prevMode = self->mode;
	self->mode = mode;
	self->walkDir = 0;
	self->side = 0;
	self->strafeDir = 0;
	self->orbitSide = 0;
	self->hold = 0;
	self->avoidTime = 0.0f;
	self->stuck = 0;
	if (self->link.target) {
		CHECK(self->link.data);
		Nto1Node_Unlink(&self->link);
	}
	self->direct = 0;
	self->waitStart = 0;
	self->waitTime = 0.0f;
	self->avoidDir.x = 0.0f;
	self->avoidDir.y = 0.0f;
	self->avoidDir.z = 0.0f;
	self->lifeMark = WChar(self->ent)->wl.life;
	switch (self->mode) {
	case AICC_STOP:
		vt->Stop2(self);
		break;
	case AICC_GO_TO: {
		vec3_t p = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		double speed = va_arg(ap, double);
		r = vt->GoTo(self, &p, (float)dist, (float)speed);
		break;
	}
	case AICC_GO_TO_DIRECT: {
		vec3_t p = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		double speed = va_arg(ap, double);
		r = vt->GoToDirect(self, &p, (float)dist, (float)speed);
		break;
	}
	case AICC_FOLLOW:
	case AICC_FOLLOW_PATH: {
		Nto1Node *t = AICC_OwnerVtbl(self->ai)->GetTarget(self->ai);
		double near = va_arg(ap, double);
		double far = va_arg(ap, double);
		double speed = va_arg(ap, double);
		self->flying = (byte)va_arg(ap, int);
		if (self->mode == AICC_FOLLOW) {
			r = vt->Follow(self, t, (float)near, (float)far, (float)speed);
		} else {
			r = vt->FollowPath(self, t, (float)near, (float)far, (float)speed);
		}
		break;
	}
	case AICC_DIR: {
		vec3_t p = va_arg(ap, vec3_t);
		vt->SetDir(self, &p);
		break;
	}
	case AICC_AWAY: {
		self->direct = 1;
		Nto1Node *t = AICC_OwnerVtbl(self->ai)->GetTarget(self->ai);
		vt->Away(self, t, (byte)va_arg(ap, int));
		break;
	}
	case AICC_ROUTE: {
		vec3_t p = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		double speed = va_arg(ap, double);
		int a = va_arg(ap, int);
		int b = va_arg(ap, int);
		r = vt->Route(self, &p, (float)dist, (float)speed, (byte)a, (byte)b);
		break;
	}
	}
	va_end(ap);
	return r;
}

void cAICharControl_Away(cAICharControl *self, Nto1Node *target, byte flag) {
	CHECK(target != NULL);
	CHECK(target->target != NULL);
	AICC_Vtbl(self)->SetSpeed(self, 0.0f);
	self->target = target;
	self->awayFlag = flag;
	float near = WChar(target->target)->wl.prim.radius * 1.5f + WChar(self->ent)->wl.prim.radius;
	self->speed = 1.0f;
	self->nearDist = near;
	self->farDist = near * 3.0f;
}

byte cAICharControl_Route(cAICharControl *self, const vec3_t *p, float dist, float speed, byte a, byte b) {
	self->arriveDist = dist;
	self->arriveR = Float_MinSS(self->radius, dist);
	AICC_Vtbl(self)->SetSpeed(self, speed);
	self->routeA = a;
	self->routeB = b;
	self->routePoint = *p;
	return AICC_Vtbl(self)->FindRoute(self);
}

void cAICharControl_SetDir(cAICharControl *self, const vec3_t *dir) {
	vec3_t n, t, pos;

	self->dir = *dir;
	AICC_Vtbl(self)->SetSpeed(self, 0.0f);
	Vec3_NormalizeTo(&n, dir);
	t.x = n.x * 10000.0f;
	t.y = n.y * 10000.0f;
	t.z = n.z * 10000.0f;
	const vec3_t *p = Obj_Vtbl(self->ent)->base.GetPos(self->ent, &pos);
	self->goal.x = p->x + t.x;
	self->goal.y = p->y + t.y;
	self->goal.z = p->z + t.z;
}

void cAICharControl_Nop(cAICharControl *self) {
	(void)self;
}

/* the arrive distance and the speed of GoTo and GoToDirect; 1 when the move to p runs already */
static int AICC_GoToSame(cAICharControl *self, const vec3_t *p, float dist, float speed) {
	self->arriveDist = dist;
	self->arriveR = Float_MinSS(self->radius, dist);
	AICC_Vtbl(self)->SetSpeed(self, speed);
	return self->mode == self->prevMode && self->dest.x == p->x && self->dest.y == p->y && self->dest.z == p->z;
}

byte cAICharControl_GoTo(cAICharControl *self, const vec3_t *p, float dist, float speed) {
	if (AICC_GoToSame(self, p, dist, speed)) {
		return 1;
	}
	byte r = AICC_Vtbl(self)->FindPath(self, p);
	if (r && self->pathRes == 2) {
		AICC_OwnerVtbl(self->ai)->Event(self->ai, 1);
	}
	return r;
}

byte cAICharControl_GoToDirect(cAICharControl *self, const vec3_t *p, float dist, float speed) {
	if (AICC_GoToSame(self, p, dist, speed)) {
		return 1;
	}
	return AICC_Vtbl(self)->DirectPath(self, p);
}

void cAICharControl_SetSpeed(cAICharControl *self, float speed) {
	self->speed = speed;
	AICC_Vtbl(self)->SetTurnSpeed(self, speed);
}

void cAICharControl_SetTurnSpeed(cAICharControl *self, float speed) {
	if (0.55f >= speed) {
		self->turnGain = 5.0f;
		self->turnStep = 1.0f;
	} else {
		float t = speed - 0.5f;
		float a = t * 9.0f;
		float b = t * 2.5f;
		a = a + a;
		b = b + b;
		self->turnStep = a + 1.0f;
		self->turnGain = b + 5.0f;
	}
	self->viewGain = 15.0f;
	self->viewStep = 1.0f;
}

void cAICharControl_Stop2(cAICharControl *self) {
	AICC_Vtbl(self)->Reset(self);
}

void cAICharControl_Reset(cAICharControl *self) {
	AICC_Vtbl(self)->Stop(self);
	self->dest.x = -FLT_MAX;
	self->dest.y = -FLT_MAX;
	self->dest.z = -FLT_MAX;
	self->goal.x = -FLT_MAX;
	self->goal.y = -FLT_MAX;
	self->goal.z = -FLT_MAX;
}

byte cAICharControl_IsBlocked(cAICharControl *self) {
	return self->blocked;
}

byte cAICharControl_GetAvoidFound(cAICharControl *self) {
	return self->avoidFound;
}

void cAICharControl_SetHold(cAICharControl *self, byte on) {
	self->hold = on;
	if (on) {
		AICC_Vtbl(self)->Stop(self);
	}
}
