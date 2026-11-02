/*
 * AI/AI.cpp of the game: cAI, the base of every AI. See include/scrap/ai.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/slotcontroller.h"

#define CAI_VFTABLE (&cAI_vftable)
static const cAIBaseVtbl cAI_vftable;

#define AI_SLOT_TIME 0.5f
#define cAI_Fail_Entity_NULL 0		/* the CHECK of the original that always fails */

static void cAI_Init(cAI *self) {
	self->vftable = CAI_VFTABLE;
	cSlotController_ctor(&self->slotThink, AI_SLOT_TIME);
}

cAI *cAI_ctor(cAI *self, cEntity *e) {
	cAI_Init(self);
	if (!e) {
		Con_Bluef("ERROR: Inicializaci\xf3n AI. Entidad = NULL.\n");
		CHECK(cAI_Fail_Entity_NULL);
	}
	self->entity = e;
	self->started = 0;
	self->waitTime = 0.0f;
	self->dt = 0.0f;
	self->changed = 0;
	self->changing = 0;
	self->seen = 0;
	self->relaxCount = 0;
	self->stateTime = 0.0f;
	return self;
}

cAI *cAI_ctorDefault(cAI *self) {
	cAI_Init(self);
	return self;
}

void cAI_dtor(cAI *self) {
	self->vftable = CAI_VFTABLE;
}

static cAI *cAI_Delete(cAI *self, byte flags) {
	self->vftable = CAI_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void cAI_Update(cAI *self, float dt) {
	self->dt = dt;
	self->stateTime = self->stateTime + dt;
	if (self->seen == 0) {
		self->waitTime = self->waitTime + dt;
	} else {
		self->waitTime = 0.0f;
	}
	self->changed = 0;
	if (!self->started) {
		self->started = 1;
	}
}

void cAI_UpdateOff(cAI *self, float dt) {
	self->dt = dt;
	self->changed = 0;
	self->stateTime = self->stateTime + dt;
	if (!self->started) {
		self->started = 1;
	}
}

void cAI_Draw(cAI *self) {
	(void)self;
}

byte cAI_GetChanged(cAI *self) {
	return self->changed;
}

byte cAI_IsVisible(cAI *self) {
	byte vis = 1;
	int sector = CGameModel_GetSector(Obj_Vtbl(self->entity)->GetModel(self->entity));

	if (sector < 0) {
		return vis;
	}
	cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
	vec3_t camPos, pos;
	CamCtl_Vtbl(cam)->GetCamPos(cam, &camPos.x, &camPos.y, &camPos.z);
	int camSector = Video_GetStatus();
	int atCam = Sector_At(&camPos);
	if (camSector >= 0 && atCam >= 0 && sector != camSector && sector != atCam) {
		int s = Collision_Sector(sector, self->entity->vftable->GetPos(self->entity, &pos), &camPos, 1);
		if (s != camSector && s != atCam) {
			vis = 0;
		}
	}
	return vis;
}

byte cAI_VarI(cAI *self, const char *name, int *i, byte set) {
	(void)self;
	(void)name;
	(void)i;
	(void)set;
	return 0;
}

byte cAI_VarF(cAI *self, const char *name, double *d, byte set) {
	(void)self;
	(void)name;
	(void)d;
	(void)set;
	return 0;
}

byte cAI_VarS(cAI *self, const char *name, char **s, byte set) {
	(void)self;
	(void)name;
	(void)s;
	(void)set;
	return 0;
}

byte cAI_VarV(cAI *self, const char *name, double *x, double *y, double *z, byte set) {
	(void)self;
	(void)name;
	(void)x;
	(void)y;
	(void)z;
	(void)set;
	return 0;
}

void cAI_Print(cAI *self, const char *text) {
	(void)self;
	Con_Bluef(text);
}

void cAI_GetEventName(cAI *self, int ev, char *out) {
	static const char *const names[] = {
		"LOST_AGENT", "OBJECTIVE_NOT_FOUND", "OBJECTIVE_REACHED", "OBJECTIVE_DIED", "OBJECTIVE_NEAR",
		"OBJECTIVE_VIS", "ORIENT_REACHED", "DONT_GO", "PROBLEM_MOV", "NO_PROBLEM", "COLLISION_AGENTS",
	};

	(void)self;
	if ((uint32_t)ev < sizeof(names) / sizeof(names[0])) {
		memcpy(out, names[ev], strlen(names[ev]) + 1);
	}
}

void cAI_OnEvent(cAI *self, int ev) {
	char name[0x100];

	self->vftable->GetEventName(self, ev, name);
	self->vftable->Debugf(self, "EVENTO interno -> %s", name);
}

/* the slot EventChase of a class that does only return 0 */
byte cAI_EventChaseFalse(cAI *self, int ev) {
	(void)self;
	(void)ev;
	return 0;
}

/* the slot EventChaseNoAction of a class that does only return 0 */
byte cAI_EventChaseNoActionFalse(cAI *self, int ev) {
	(void)self;
	(void)ev;
	return 0;
}

/* the slot Event of a class that does only return 1 */
byte cAI_EventTrue(cAI *self, int ev) {
	(void)self;
	(void)ev;
	return 1;
}

/* the slot ThinkChaseBack of a class that does nothing there */
void cAI_ThinkChaseBackNop(cAI *self) {
	(void)self;
}

/* the slot ThinkChaseNoAction of a class that does nothing there */
void cAI_ThinkChaseNoActionNop(cAI *self) {
	(void)self;
}

/* the slot ThinkChase of a class that does nothing there */
void cAI_ThinkChaseNop(cAI *self) {
	(void)self;
}

/* the slot ThinkGetAway of a class that does nothing there */
void cAI_ThinkGetAwayNop(cAI *self) {
	(void)self;
}

/* the slot ThinkGoTo of a class that does nothing there */
void cAI_ThinkGoToNop(cAI *self) {
	(void)self;
}

/* the slot UpdateAction of a class that does nothing there */
void cAI_UpdateActionNop(cAI *self) {
	(void)self;
}

/* the vtable of cAI */
static const cAIBaseVtbl cAI_vftable = {
	.Delete = cAI_Delete,
	.Update = cAI_Update,
	.UpdateOff = cAI_UpdateOff,
	.Draw = cAI_Draw,
	.GetChanged = cAI_GetChanged,
	.IsVisible = cAI_IsVisible,
	.Debugf = (void *)crt_purecall,
	.VarI = cAI_VarI,
	.VarF = cAI_VarF,
	.VarS = cAI_VarS,
	.VarV = cAI_VarV,
	.Print = cAI_Print,
	.GetEventName = cAI_GetEventName,
	.OnEvent = cAI_OnEvent,
};
