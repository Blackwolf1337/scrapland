/*
 * Entities/Char/WalkCharEntity.cpp of the game, its first part: the walking character (include/scrap/walkcharentity.h).
 */
#include "scrap/chartype.h"
#include "scrap/arrowindicator.h"
#include "scrap/fxelements.h"
#include "scrap/missileentity.h"
#include "scrap/walkcharentity.h"

#define WC_F(e, o) (*(float *)((byte *)(e) + (o)))
#define WC_I(e, o) (*(int *)((byte *)(e) + (o)))
#define WC_B(e, o) (*(byte *)((byte *)(e) + (o)))
#define WC_P(e, o) (*(void **)((byte *)(e) + (o)))
#define WC_NODE(e, o) ((Nto1Node *)((byte *)(e) + (o)))
#define WC_FUNC(e, o) ((cPy_Function *)((byte *)(e) + (o)))

/* the action controller of the character and the callbacks of its class (cActClass) */
#define WChar_ActCtrlPtr(e) (*(void **)((byte *)(e) + 0x234))
#define WChar_OnEnergyFull(e) WC_FUNC(e, 0x49c)		/* the energy used up while an action takes it */
#define WChar_EnergyTime(e) WC_F(e, 0x4b4)		/* the seconds an action takes the energy in, 0 recharges */
#define WChar_ViewLockF(e) WC_F(e, 0x4d8)		/* the blend of the view lock, 0..1 */
#define ActClass_OnPress(c) WC_FUNC(c, 0x190)
#define ActClass_OnRelease(c) WC_FUNC(c, 0x1a4)
#define ActClass_OnTalk(c) WC_FUNC(c, 0x1b8)
#define ActClass_OnVictim(c) WC_FUNC(c, 0x1cc)

/* the turn of the head of the controller towards ang (x, y) in dt (rva 0x62dc0) and the camera of the class
 * (rva 0x616e0) */
void __thiscall cActCtrl_TurnHead(void *ctrl, const float *ang, float dt);
void __thiscall cActClassCam_Update(void *cam, cEntity *followCam, cEntity *e);

cEntity *__thiscall cWalkCharEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cEntity *self = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->vftable = Game_Ptr(CWALKCHARENTITY_VFTABLE);
	Nto1Node_Init(WC_NODE(self, 0x3e8));
	WC_I(self, 0x404) = 0;
	WC_I(self, 0x408) = 0;
	cPy_Function_ctor(WChar_OnEnergyFull(self));
	Nto1Node_Init(WC_NODE(self, 0x4bc));
	cWalkCharControl_ctor(WChar_Control(self));
	Nto1Node_Init(WChar_ListenNode(self));
	Nto1Node_Init(WC_NODE(self, 0x534));
	cPy_Function_ctor(WC_FUNC(self, 0x544));
	cPy_Function_ctor(WC_FUNC(self, 0x558));
	WChar_OwnerSlot(self) = -1;
	for (int o = 0x428; o < 0x458; o += 4) {
		WC_I(self, o) = 0;
	}
	float r = FxElem_Rand(4.0, 2.0);
	self->mask |= 0x10;
	WC_F(self, 0x458) = r;
	WC_F(self, 0x45c) = 128.0f;
	*(uint16_t *)((byte *)self + 0x464) = 0;
	for (int o = 0x468; o <= 0x478; o += 4) {
		WC_I(self, o) = 0;
	}
	WC_I(self, 0x490) = 0;
	WC_I(self, 0x480) = 0;
	WC_I(self, 0x484) = 0;
	WC_I(self, 0x488) = 0;
	WC_B(self, 0x48c) = 0;
	WChar_ListenNode(self)->data = self;
	WC_NODE(self, 0x534)->data = self;
	WChar_ListenOn(self) = 0;
	WC_I(self, 0x494) = 0;
	WC_I(self, 0x47c) = 0;
	WC_I(self, 0x498) = 0;
	WC_F(self, 0x230) = 5000.0f;
	WChar_Money(self) = 0;
	WChar_MoneyReward(self) = 0;
	WC_F(self, 0x3f8) = 1000.0f;
	WC_B(self, 0x4dc) = 0;
	WC_I(self, 0x4e0) = 0;
	WC_I(self, 0x4e4) = 0;
	WC_I(self, 0x4e8) = 0;
	WChar_MaxLife(self) = WChar_Life(self);
	WChar_EnergyBarDecr(self) = 0.200000003f;
	WChar_EnergyTime(self) = 0.0f;
	WC_NODE(self, 0x3e8)->data = self;
	WC_I(self, 0x56c) = 0;
	WC_I(self, 0x570) = 0;
	WC_I(self, 0x3fc) = 0;
	WC_NODE(self, 0x4bc)->target = self;
	WChar_ViewLock(self) = 1;
	WChar_ViewLockF(self) = 1.0f;
	WC_I(self, 0x4b8) = 0;
	WC_I(self, 0x460) = 0;
	return self;
}

void __fastcall cWalkCharEntity_dtor(cEntity *self) {
	self->vftable = Game_Ptr(CWALKCHARENTITY_VFTABLE);
	if (WChar_ActCtrlPtr(self)) {
		void *ai = self->ai;
		if (ai) {
			/* the deleting destructor of the AI */
			(*(void(__thiscall **)(void *, uint32_t))*(void **)ai)(ai, 1);
		}
		self->ai = NULL;
	}
	cPy_Function_dtor(WC_FUNC(self, 0x558));
	cPy_Function_dtor(WC_FUNC(self, 0x544));
	Nto1Node_Done(WC_NODE(self, 0x534));
	Nto1Node_Done(WChar_ListenNode(self));
	Nto1Node_Done(&WChar_Control(self)->target);
	Nto1Node_Done(WC_NODE(self, 0x4bc));
	cPy_Function_dtor(WChar_OnEnergyFull(self));
	Nto1Node_Done(WC_NODE(self, 0x3e8));
	cWithLifeEntity_dtor(self);
}

cEntity *__thiscall cWalkCharEntity_Delete(cEntity *self, uint32_t flags) {
	cWalkCharEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CWALKCHARENTITY_SIZE);
	}
	return self;
}

void __thiscall cWalkCharEntity_ActionTick(cEntity *self, float dt) {
	void *ctrl = WChar_ActCtrlPtr(self);

	if (!ctrl) {
		return;
	}
	int slot = WChar_OwnerSlot(self);
	if ((g_EnergyBarActive || slot < 0 || slot >= 4) && WC_B(self, 0x505) != WChar_ActionPress(self)) {
		byte *cls = ChType_ActClass(ctrl);
		if (WChar_ActionPress(self)) {
			cPy_Function_AddParamS(ActClass_OnPress(cls), self->name);
			cPy_Function_Execute(ActClass_OnPress(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
		} else {
			cPy_Function_AddParamS(ActClass_OnRelease(cls), self->name);
			cPy_Function_Execute(ActClass_OnRelease(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
		}
	}
	if (!WC_B(self, 0x508)) {
		if (!WChar_ActionPress(self) && WC_B(self, 0x506) && !WC_B(self, 0x507) &&
			cWalkCharEntity_CanActOnVictim(self, 0)) {
			cPy_Function_AddParamS(ActClass_OnVictim(ChType_ActClass(WChar_ActCtrlPtr(self))), self->name);
			byte *cls = ChType_ActClass(WChar_ActCtrlPtr(self));
			cPy_Function_AddParamS(ActClass_OnVictim(cls), Nto1Node_TargetName(WC_NODE(self, 0x534)));
			cPy_Function_Execute(ActClass_OnVictim(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
		}
		if (!WC_B(self, 0x508)) {
			goto energy;
		}
	}
	if (!WC_B(self, 0x509) && !ChType_ActPriority(WChar_ActCtrlPtr(self))) {
		cEntity *t;
		byte ok;
		if (WChar_ListenOn(self) && (t = WChar_Listen(self)) != NULL) {
			ok = t->vftable->IsType(t, TYEN_WITH_LIFE);
		} else {
			t = WChar_Victim(self);
			if (!t || !t->vftable->IsType(t, TYEN_WITH_LIFE)) {
				goto energy;
			}
			ok = WC_B(t, 0x384);
		}
		if (ok) {
			if (WC_B(t, 0x398)) {
				cPy_Function_AddParamS(ActClass_OnTalk(ChType_ActClass(WChar_ActCtrlPtr(self))), self->name);
				cPy_Function_Execute(ActClass_OnTalk(ChType_ActClass(WChar_ActCtrlPtr(self))), 0);
			} else {
				void *l = t->lLogic;
				if (!l || (*(byte(__thiscall **)(void *))(*(byte **)l + 8))(l)) {
					cArrowIndicator_Execute(Ent_Arrow(t), self);
				}
			}
		}
	}
energy:
	if (!g_EnergyBarActive) {
		WChar_EnergyUsed(self) = 1.0f;
		return;
	}
	float time = WChar_EnergyTime(self);
	if (time > 0.0f) {
		float used = dt / time + WChar_EnergyUsed(self);
		WChar_EnergyUsed(self) = used;
		if (used >= 1.0f) {
			WChar_EnergyUsed(self) = 1.0f;
			if (cPy_Function_IsValid(WChar_OnEnergyFull(self))) {
				cPy_Function_AddParamS(WChar_OnEnergyFull(self), self->name);
				cPy_Function_Execute(WChar_OnEnergyFull(self), 0);
			}
		}
	} else {
		float used = WChar_EnergyUsed(self);
		int i = (int)(used - 0.999899983f);
		int lo = 0;
		if (i <= 0) {
			lo = i;
		}
		used = used - dt / g_EnergyRechargeTime;
		WChar_EnergyUsed(self) = used;
		if ((float)lo >= used) {
			WChar_EnergyUsed(self) = (float)lo;
		}
	}
}

void __thiscall cWalkCharEntity_UpdateModel(cEntity *self) {
	vec3_t n, d, pos;
	float head[2];
	cObjEntity *o = (cObjEntity *)self;

	vec3_t ang = o->renderAng;
	d.x = WC_F(self, 0x44c) - o->renderAng.x;
	d.y = WC_F(self, 0x450) - o->renderAng.y;
	d.z = WC_F(self, 0x454) - o->renderAng.z;
	Vec3_NormAng(&n, &d);
	float dt = cTimer_GetDelta(&o->timer);
	if (!(0.00000099999999747f >= dt)) {
		CGameModel_SetRot(&o->model, &ang);
		pos.x = o->renderPos.x + 0.0f;
		pos.y = o->renderPos.y + WC_F(self, 0x494);
		pos.z = o->renderPos.z + 0.0f;
		CGameModel_SetPos(&o->model, &pos);
		void *ctrl = WChar_ActCtrlPtr(self);
		if (ctrl) {
			head[0] = n.x;
			head[1] = WChar_ViewLockF(self) * n.y;
			cActCtrl_TurnHead(ctrl, head, dt);
			if (WChar_Cam(self)) {
				cActClassCam_Update(ChType_ActClass(WChar_ActCtrlPtr(self)) + 0x110, WChar_Cam(self), self);
			}
		}
		void **fx = WChar_FX(self);
		if (fx) {
			(*(void(__thiscall **)(void *))(*(byte **)fx + 0x10))(fx);
		}
	}
}

void __thiscall cWalkCharEntity_UpdateViewLock(cEntity *self, float dt) {
	void *ctrl = WChar_ActCtrlPtr(self);

	if (ctrl && ChType_ActClass(ctrl)) {
		byte b = ChType_ActClass(ctrl)[0x188];
		byte old = WChar_ViewLock(self);
		WChar_ViewLock(self) = b;
		if (b && (g_WalkCameraCenter == 0.0f || WChar_OwnerSlot(self) == -1 || WC_B(self, 0x50c))) {
			b = 1;
		} else {
			b = 0;
		}
		WChar_ViewLock(self) = b;
		if (!old && b) {
			WC_B(self, 0x465) = 1;
		}
	}
	float target = 0.0f;
	if (WChar_ViewLock(self)) {
		target = 1.0f;
	}
	Float_Approach(&WChar_ViewLockF(self), target, dt + dt);
}

byte __thiscall cWalkCharEntity_CanActOnVictim(cEntity *self, byte checkClass) {
	if (!(0.0f >= WChar_EnergyUsed(self))) {
		return 0;
	}
	cEntity *v = WChar_Victim(self);
	if (!v || !WChar_ActCtrlPtr(self) || ChType_ActPriority(WChar_ActCtrlPtr(self))) {
		return 0;
	}
	if (!v->vftable->IsType(v, TYEN_WALKCHAR)) {
		return 0;
	}
	if (checkClass && !ChType_ActClass(ChType_ActCtrl(v))[0x184]) {
		return 0;
	}
	return 1;
}

void __fastcall cWalkCharEntity_Unlink3e8(cEntity *self) {
	Nto1Node *n = WC_NODE(self, 0x3e8);

	CHECK(n->data);
	Nto1Node_Unlink(n);
}
