/*
 * Logic/MessengerLogic.cpp of the game: the messengers (cMessengers of the GenChar group, the list "Messenger")
 * and their logic (a GenChar logic that ignores the alarm). The messengers carry the data pack "MessengerDataPack"
 * between the desks of the tube state (functionaries or the door makers); the think follows the pack and its
 * carrier (the player or a messenger) and hands it over at the desk.
 */
#include <string.h>

#include "scrap/arrowindicator.h"
#include "scrap/chartype.h"
#include "scrap/embeddedfxmisc.h"
#include "scrap/itementity.h"
#include "scrap/modmove.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"
#include "scrap/withlifeentity.h"

/* defined further down or in another file without a header */
void *GenCharGroup_MakeLogic(cEntity *e);

#define g_MessengerPrepareDistance GAME_GLOBAL_IN(float, GVar_MessengerPrepareDistance, 16)	/* MessengerPrepareDistance */
#define g_MessengerObjectiveDistance GAME_GLOBAL_IN(float, GVar_MessengerObjectiveDistance, 16)	/* MessengerObjetiveDistance */
#define MSG_THINK_STEP 0.13f				/* rva 0x3b60e0, the interval of the think */
#define MSG_EMPTY GAME_STR(str_Empty)	/* "" */

#define MSG_VT(self) ((self)->base.vftable)

void *cMessengers_ctor(void *self) {
	return self;
}

void cMessengers_dtor(void *self) {
	(void)self;
}

cEntity *cMessengers_FindDesk(cMessengers *self) {
	const char *type = g_MessengerTubeState ? "BureaucracyDesktopFunctionary" : "DoorDataTMaker";
	float count = (float)self->count;
	int k = (int)(float)(Weap_Rand01() * (double)count + 0.0);
	cEntity *best = NULL;
	int found = 0;

	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->active || !e->vftable->IsType(e, TYEN_WITH_LIFE) || Obj(e)->radarShow || !ChType_ActCtrl(e) ||
			strcmp(Logic_TypeName(e), type)) {
			continue;
		}
		if (!best || k <= 0) {
			best = e;
			if (k <= 0) {
				k = self->count;
			}
		}
		k--;
		found++;
	}
	if (found > self->count) {
		self->count = found;
	}
	return best;
}

/* the GenChar character of the type "Messenger" (WalkChar) */
static int Msg_IsMessenger(cEntity *e) {
	return e->vftable->IsType(e, TYEN_WALKCHAR) && !strcmp(Logic_TypeName(e), "Messenger");
}

/* messenger m with the pack at the desk: near it the drop starts (DropDataInit, then the DropData loop); at the node
 * DC_DataPack of the desk the pack goes onto it (the price and the stat for the player, the callback, the desk
 * closes). 1 while the drop goes on, 0 out of range (the drop is ended) */
byte Messenger_Deliver(cEntity *m, cEntity *desk, cEntity *pack) {
	vec3_t tmp;
	matrix_t mat;

	if (!desk || !pack || !m) {
		return 0;
	}
	CGameModel *model = Obj_Vtbl(desk)->GetModel(desk);
	byte dropping = cActCtrl_IsActive(ChType_ActCtrl(m), "DropData");
	const matrix_t *mm = CGameModel_NodeMatrix(model, &mat, CGameModel_FindRenderNode(model, "DC_DataPack"), 0, NULL, 0);
	float px = mm->m[12];
	float py = mm->m[13];
	float pz = mm->m[14];
	const vec3_t *mp = m->vftable->GetPos(m, &tmp);
	float dx = mp->x - px;
	float dy = mp->y - py;
	float dz = mp->z - pz;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!(g_MessengerPrepareDistance > dist)) {
		if (dropping) {
			cActCtrl_Play(ChType_ActCtrl(m), "DropDataEnd", 1, 1.0f);
			cActCtrl_SetDefLoop(ChType_ActCtrl(m), "Relax", 1);
		}
		return 0;
	}
	if (crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(m), 0), "DropData", 8) &&
		cActCtrl_Play(ChType_ActCtrl(m), "DropDataInit", 0, 1.0f)) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(m), "DropData", 1);
		return 1;
	}
	if (!dropping || !(g_MessengerObjectiveDistance > dist)) {
		return 1;
	}
	cActCtrl_Play(ChType_ActCtrl(m), "Relax", 1, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(m), "Relax", 1);
	if (cPy_Function_IsValid(g_MessengerDropCallback)) {
		cPy_Function_AddParamS(g_MessengerDropCallback, m->name);
		cPy_Function_AddParamI(g_MessengerDropCallback, g_MessengerTubeState);
		cPy_Function_Execute(g_MessengerDropCallback, 0);
	}
	if (g_MessengerTubeState) {
		if (cWalkCharEntity_GetOwnerSlot(m) == 0) {
			g_pWorld->money += g_MessengerDropPrice;
			SaveVar_Inc("Stats.Messenger", 1);
		}
		if (g_MessengerTubeState && Obj(pack)->fx) {
			cEmbeddedFXDataPack_SetOn((cEmbeddedFXDataPack *)Obj(pack)->fx, 0);
		}
	}
	pack->vftable->SetPos(pack, CGameModel_GetPos(Obj_Vtbl(pack)->GetModel(pack), &tmp));
	cObjEntity *po = (cObjEntity *)pack;
	cModMove_Link(&po->mod, MSG_EMPTY, MSG_EMPTY);
	po->mod.offMul = 0.0f;
	po->mod.deltaOffMul = 4.0f;
	po->mod.useLinkAng = 1;
	cModMove_Link(&po->mod, desk->name, "DC_DataPack");
	cPy_Function_Clear(&((cItemEntity *)pack)->onTake);
	cActCtrl_Play(ChType_ActCtrl(desk), "Close", 0, 1.0f);
	if (!strcmp(cActCtrl_GetActionName(ChType_ActCtrl(desk), 0), "Close")) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(desk), "Closed", 0);
		cPy_Function_Clear(&Obj(desk)->onObjSlot);
	}
	return 1;
}

/* every 0.13 s: the pack on a desk (the door of the desk closes, after 5 s the pack goes back out, the tube state
 * changes), carried by the player (the messengers follow, the mission arrow on the desk) or by a messenger (to a
 * desk of the tube state), or free (the messengers go for it) */
void cMessengers_Think(void *obj) {
	cMessengers *self = obj;
	vec3_t old, p;
	matrix_t mat;
	byte opened;

	cEntity *pack = World_FindEntity(g_MessengerDataPack);
	if (pack && !pack->vftable->IsType(pack, "item")) {
		pack = NULL;
	}
	cEntity *user = ChType_Usr0();
	if (user && !Msg_IsMessenger(user)) {
		user = NULL;
	}
	cEntity *carrier = pack ? Obj(pack)->mod.link.target : NULL;
	cEntity *msg = carrier;
	cEntity *desk = carrier;
	if (carrier) {
		if (Msg_IsMessenger(carrier)) {
			desk = NULL;
		} else {
			msg = NULL;
			if (!carrier->vftable->IsType(carrier, TYEN_WITH_LIFE) || !ChType_ActCtrl(carrier)) {
				desk = NULL;
			}
		}
	}
	if (!pack) {
		goto update;
	}
	byte flag = msg == NULL;
	if (desk) {
		if (!strcmp(Logic_TypeName(desk), "DoorDataTMaker")) {
			flag = cPy_Function_IsValid(&Obj(desk)->onObjSlot);
		} else {
			flag = crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(desk), 1), "Close", 5) != 0;
		}
		if (!flag && cActCtrl_IsActive(ChType_ActCtrl(desk), "Closed")) {
			self->timer = self->timer - MSG_THINK_STEP;
			if (0.0f >= self->timer) {
				self->desk = NULL;
				self->target = NULL;
				self->timer = 5.0f;
				CGameModel_GetPos(Obj_Vtbl(pack)->GetModel(pack), &old);
				int node = CGameModel_FindRenderNode(Obj_Vtbl(desk)->GetModel(desk), "DC_DataPack");
				const matrix_t *mm = CGameModel_NodeMatrix(Obj_Vtbl(desk)->GetModel(desk), &mat, node, 0, NULL, 0);
				p.x = mm->m[12];
				p.y = mm->m[13];
				p.z = mm->m[14];
				pack->vftable->SetPos(pack, &p);
				CGameModel_SetPos(Obj_Vtbl(pack)->GetModel(pack), &old);
				g_MessengerTubeState = g_MessengerTubeState == 0;
				if (Obj(pack)->fx) {
					cEmbeddedFXDataPack_SetOn((cEmbeddedFXDataPack *)Obj(pack)->fx,
						g_MessengerTubeState != 0);
				}
				if (!strcmp(cActCtrl_GetActionName(ChType_ActCtrl(desk), 0), "Closed")) {
					cPy_Function_SetFunc(&Obj(desk)->onObjSlot, "!AutoDoorSlot");
				} else {
					cActCtrl_Play(ChType_ActCtrl(desk), "Open", 0, 1.0f);
				}
			}
		}
		opened = cActCtrl_IsActive(ChType_ActCtrl(desk), "Opened");
	} else {
		opened = flag;
	}
	/* the pack can be taken while it is free or its desk is open */
	cPy_Function *take = &((cItemEntity *)pack)->onTake;
	if (opened) {
		if (!cPy_Function_IsValid(take)) {
			cPy_Function_SetNative(take, (nativefunc_t)ChType_MessengerTakeDataPack);
		}
	} else if (cPy_Function_IsValid(take)) {
		cPy_Function_Clear(take);
	}
	if (!user) {
		cMessengers_SetStateAll(self, -1, NULL);
		Obj(pack)->radarShow = g_MessengerDataPackMissionArrow == 1;
		if (self->desk) {
			((cWithLifeEntity *)self->desk)->arrow.canMission = 0;
			Obj(self->desk)->radarShow = 0;
		}
		goto update;
	}
	if (msg == user) {
		self->target = NULL;
		cMessengers_SetStateAll(self, GENCHAR_ATTACK, user);
		if (Obj(pack)->radarShow) {
			Obj(pack)->radarShow = 0;
			if (!self->desk) {
				self->desk = cMessengers_FindDesk(self);
			}
			if (self->desk && g_MessengerDataPackMissionArrow) {
				Obj(self->desk)->radarShow = g_MessengerDataPackMissionArrow;
				((cWithLifeEntity *)self->desk)->arrow.canMission = 1;
				cArrowIndicator_LoadMission(&((cWithLifeEntity *)self->desk)->arrow,
					g_MessengerDataPackMissionArrow == 1 ? *g_MissionArrowOn : *g_MissionArrowOff);
			}
		}
		Messenger_Deliver(user, self->desk, pack);
		goto update;
	}
	Obj(pack)->radarShow = g_MessengerDataPackMissionArrow;
	if (self->desk) {
		((cWithLifeEntity *)self->desk)->arrow.canMission = 0;
		Obj(self->desk)->radarShow = 0;
	}
	if (msg) {
		cMessengers_SetStateAll(self, GENCHAR_ATTACK, msg);
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(msg);
		if (l && !Obj(msg)->radarShow) {
			if (!self->target) {
				cEntity *t = cMessengers_FindDesk(self);
				self->target = t;
				if (t) {
					l->vftable->SetState(l, GENCHAR_GET_IT, t);
				} else {
					l->vftable->RestartState(l);
				}
			} else if (l->state == GENCHAR_ATTACK) {
				self->target = NULL;
			} else {
				Messenger_Deliver(msg, self->target, pack);
			}
		}
	} else if (flag) {
		cMessengers_SetStateAll(self, GENCHAR_GET_IT, pack);
		self->target = NULL;
	} else {
		cMessengers_SetStateAll(self, -1, NULL);
		self->target = NULL;
	}
update:
	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		l->vftable->Update(l, MSG_THINK_STEP);
	}
}

void cMessengers_Init(void *obj) {
	cMessengers *self = obj;

	self->list = cWorld_NewEntityList(g_pWorld, "Messenger");
	self->list->func = (int (*)(void))(void *)GenCharGroup_MakeLogic;
	g_MessengerDataPackMissionArrow = g_MessengerDataPackMissionArrowDef;
	g_MessengerTubeState = g_MessengerTubeStateDef;
	cPy_Function_Clear(g_MessengerDropCallback);
	g_MessengerDropPrice = g_MessengerDropPriceDef;
	self->timer = 5.0f;
	self->count = 5;
	self->desk = NULL;
	self->target = NULL;
}

void cMessengers_SetStateAll(cMessengers *self, int state, cEntity *e) {
	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cEntity *m = n->data;
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(m);
		if (m == e || Obj(m)->radarShow) {
			continue;
		}
		if (state == -1) {
			if (!l->idle && !l->alwaysLock) {
				l->vftable->RestartState(l);
			}
		} else {
			l->vftable->SetState(l, state, e);
		}
	}
}

cMessengerLocalLogic *cMessengerLocalLogic_ctor(cMessengerLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	MSG_VT(self) = CMESSENGERLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_SetState(self, GENCHAR_RELAX, NULL);
	self->ignoreAlarm = 1;
	return self;
}

cMessengerLocalLogic *cMessengerLocalLogic_Delete(cMessengerLocalLogic *self, byte flags) {
	MSG_VT(self) = CMESSENGERLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf28 and vf27: the same walk in alert and relaxed */
void cMessengerLocalLogic_ToAlert(cMessengerLocalLogic *self, cAIChar *ai) {
	(void)self;
	ai->vftable->SetState(ai, 1, 1.0, 0);
}

/* vf17: team 3, 0 while it attacks a player */
void cMessengerLocalLogic_Update(cMessengerLocalLogic *self, float dt) {
	cGenCharLocalLogic_Update(self, dt);
	WLife(self->base.parent)->arrow.tgType = 3;
	if (self->base.state == GENCHAR_ATTACK) {
		cEntity *t = self->base.target.target;
		if (t && t->vftable->IsType(t, TYEN_WALKCHAR) && cWalkCharEntity_GetOwnerSlot(t) != -1) {
			WLife(self->base.parent)->arrow.tgType = 0;
		}
	}
}

/* vf34: to the node DC_DataPack of e when it has one (MessengerGotoSpeed, 0.25 in the easy single game), else the
 * walk of the GenChars */
void cMessengerLocalLogic_GoTo(cMessengerLocalLogic *self, cAIChar *ai, cEntity *e) {
	vec3_t t1, t2, d, dir;
	matrix_t m;

	if (!e->vftable->IsType(e, TYEN_WITH_LIFE) || !Obj_Vtbl(e)->GetModel(e)) {
		cGenCharLocalLogic_GoTo(self, ai, e);
		return;
	}
	int node = CGameModel_FindRenderNode(Obj_Vtbl(e)->GetModel(e), "DC_DataPack");
	if (node == -1) {
		cGenCharLocalLogic_GoTo(self, ai, e);
		return;
	}
	float speed = World_IsEasySingle() ? 0.25f : g_MessengerGotoSpeed;
	cEntity *p = self->base.parent;
	const vec3_t *pp = p->vftable->GetPos(p, &t1);
	const vec3_t *ep = e->vftable->GetPos(e, &t2);
	d.y = ep->y - pp->y;
	d.x = ep->x - pp->x;
	d.z = ep->z - pp->z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len == 0.0f) {
		dir = d;
	} else {
		float inv = 1.0f / len;
		dir.x = d.x * inv;
		dir.y = d.y * inv;
		dir.z = d.z * inv;
	}
	const matrix_t *mm = CGameModel_NodeMatrix(Obj_Vtbl(e)->GetModel(e), &m, node, 0, NULL, 0);
	vec3_t np = { mm->m[12], mm->m[13], mm->m[14] };
	ai->vftable->SetState(ai, 4, np, dir, 70.0, (double)speed);
}
