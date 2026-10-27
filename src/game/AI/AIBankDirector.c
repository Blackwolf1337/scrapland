/*
 * AI/AIBankDirector.cpp of the game: the AI of the bank director (cAIBankDirector, a cAIChar). Its attack check and
 * its state changes are the same code as the ones of the bank master (AIBankMaster.c). See include/scrap/ai.h.
 */
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIBANK_SIZE 0x384
#define CAICONTROL_SIZE 0x360

void *__thiscall cAIBankDirector_ctor(void *mem, cEntity *e, int *arg) {
	cAICharView *self = mem;

	cAIChar_ctor(mem, e, arg);
	*(const void **)self = Game_Ptr(CAIBANKDIRECTOR_VFTABLE);
	if (self->control) {
		((void *(__thiscall *)(void *, byte))AI_VSlot(self->control, 0))(self->control, 1);
	}
	void *ctl = operator_new(CAICONTROL_SIZE);
	self->control = ctl ? cAIControlBankDirector_ctor(ctl, e, self, arg) : NULL;
	CHECK(self->control);
	return self;
}

void __fastcall cAIBankDirector_dtor(void *self) {
	*(const void **)self = Game_Ptr(CAIBANKDIRECTOR_VFTABLE);
	cAIChar_dtor((cAIChar *)self);
}

void *__thiscall cAIBankDirector_Delete(void *self, byte flags) {
	*(const void **)self = Game_Ptr(CAIBANKDIRECTOR_VFTABLE);
	cAIChar_dtor((cAIChar *)self);
	if (flags & 1) {
		operator_delete_sized(self, CAIBANK_SIZE);
	}
	return self;
}

static int Layer0_Priority(cEntity *e) {
	return ((cActCtrl *)WithLife_Vtbl(e)->GetActCtrl(e))->layers[0].action->priority;
}

void __thiscall cAIBank_CheckAttack(cAIBank *self) {
	cAICharView *c = &self->base;
	byte go;
	vec3_t a, b;

	if (((int(__thiscall *)(void *))AI_VSlot(c->control, 0x08))(c->control) == 0 &&
		((int(__thiscall *)(void *))AI_VSlot(c->control, 0x10))(c->control) == 0 && !c->attack[0x18]) {
		go = 1;
	} else if (c->attack[0x18]) {
		go = 1;
	} else {
		go = self->money > WChar(c->own)->money;
	}
	cEntity *tgt = c->target;
	const vec3_t *op = c->own->vftable->GetPos(c->own, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
	if (go && ((byte(__thiscall *)(void *))AI_VSlot(c->control, 0x2c))(c->control)) {
		tp = c->target->vftable->GetPos(c->target, &b);
		op = c->own->vftable->GetPos(c->own, &a);
		if ((double)g_MaxHeightChaseCharAI > __builtin_fabs((double)(tp->y - op->y)) &&
			Layer0_Priority(c->own) <= 0x65 && dist > c->attackMin && c->attackMax > dist) {
			c->attack[0x18] = 1;
			return;
		}
	}
	c->attack[0x18] = 0;
}

void __thiscall cAIBankDirector_SetRanges(cAIBank *self) {
	cAICharView *c = &self->base;

	if (c->state != 3) {
		if (c->state != 7) {
			cAIChar_SetDists((cAIChar *)self);
			return;
		}
		c->attackMin = 0.0f;
		c->attackMax = 1000.0f;
	}
	c->rangeMax = 550.0f;
	c->rangeMin = 300.0f;
}

/* the group of the characters (g_CharGroup): a change of state allowed now, and one done */
static byte CharGroup_CanChange(void) {
	void *g = g_CharGroup;
	return ((byte(__thiscall *)(void *))AI_VSlot(g, 0x14))(g);
}

static void CharGroup_Changed(void) {
	void *g = g_CharGroup;
	((void(__thiscall *)(void *))AI_VSlot(g, 0x1c))(g);
}

static void Bank_DebugState(cAIBank *self, const char *fmt, int state) {
	char name[0x100];

	AICharView_Vtbl(self)->GetStateName(self, state, name);
	AICharView_Vtbl(self)->Debugf(self, fmt, name);
}

byte cAIBank_SetState(cAIBank *self, int state, ...) {
	va_list ap;

	Bank_DebugState(self, "Demanda externa de CAMBIO de ESTADO -> %s", state);
	if (state == 2) {
		self->money = WChar(self->base.own)->money;
		state = 7;
	}
	if (!AICharView_Vtbl(self)->IsGroupState(self, state) || CharGroup_CanChange()) {
		AICharView_Vtbl(self)->SwitchState(self, state);
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState((cAIChar *)self, state, ap);
	va_end(ap);
	return r;
}

byte cAIBank_SetStateIntern(cAIBank *self, int state, ...) {
	va_list ap;

	Bank_DebugState(self, "Demanda interna de CAMBIO de ESTADO -> %s", state);
	if (AICharView_Vtbl(self)->IsGroupState(self, state)) {
		CharGroup_Changed();
	}
	if (state == 2) {
		self->money = WChar(self->base.own)->money;
		state = 7;
	}
	AICharView_Vtbl(self)->SwitchState(self, state);
	va_start(ap, state);
	byte r = cAIChar_ChangeState((cAIChar *)self, state, ap);
	va_end(ap);
	return r;
}
