/*
 * Logic/IntPolice.cpp of the game: cAICtrl (include/scrap/logic.h), the AI control of the indoor police at
 * &g_pWorld->logic.aiCtrl ("cIntPoliceLogic" in its debug texts). Every 0.5 s it updates the sentinels and the gears: the
 * first sentinel that sees the player accuses it, the others go back; a sentinel watches a place, the nearest
 * free gear attacks a target in view (else the nearer police fuses into a gear: the action Fusion, the script
 * CreateGear makes the gear "Gear" + the number of the police). Without the alarm two visible police fuse and the
 * gears fight each other (PoliceLogicBelligerant in percent). The lists "Sentinel" and "Gear" make the logics.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"

/* defined further down or in another file without a header */
void *AICtrl_MakeGearLogic(cEntity *e);
void *AICtrl_MakeSentinelLogic(cEntity *e);
void AICtrl_SentinelDeath(cAbstractFile *params);

#define AICTRL_MAKEGEAR 0x14fc80
#define AICTRL_MAKESENTINEL 0x14fcf0
#define AICTRL_SENTINELDEATH 0x14fd60

/* the AI of the character may act */
static inline byte AICtrl_AIActive(cAIChar *ai) {
	return ai->vftable->GetRepos(ai);
}

/* the sentinel logic: the accuse (no other sentinel accuses) and the think */
static inline byte AICtrl_SentinelAccuses(cBaseLocalLogic *l) {
	return Sentinel_Vtbl(l)->Seen(l);
}

static inline void AICtrl_SentinelUpdate(cBaseLocalLogic *l, float dt, byte other) {
	Sentinel_Vtbl(l)->Think(l, dt, other);
}

/* prefix and the rest of a name, like "Gear" and the number of "Police01" (the original copies unbounded) */
static void AICtrl_Rename(char *buf, const char *prefix, const char *rest) {
	int i = 0;

	for (; prefix[i]; i++) {
		buf[i] = prefix[i];
	}
	for (; *rest && i < 0x7f; rest++) {
		buf[i++] = *rest;
	}
	buf[i] = 0;
}

static inline cEntity *AICtrl_Model(cEntity *e) {
	return (cEntity *)Obj_Vtbl(e)->GetModel(e);
}

void *__fastcall AICtrl_ctor(void *ctrl) {
	cAICtrl *self = ctrl;

	self->watch.target = NULL;
	self->watch.data = NULL;
	self->watch.next = &self->watch;
	self->watch.prev = &self->watch;
	self->sentinels = NULL;
	self->gears = NULL;
	self->alarm = 0;
	self->lastUsr = NULL;
	self->timer = 0.0f;
	self->sameChar = 0;
	self->notDefChar = 0;
	self->alerted = 0;
	self->accusing = 0;
	self->alarmOn = 0;
	self->usr = NULL;
	self->watch.data = self;
	self->watchTime = 0.0f;
	self->attacking = 0;
	self->level = -1;
	cPy_Function_Clear(g_CreateGear);
	cPy_Function_Clear(g_CreatePolice);
	g_PoliceLogicBelligerant = 0;
	self->chased = 0;
	return self;
}

void __fastcall AICtrl_dtor(void *ctrl) {
	Nto1Node_Done(&((cAICtrl *)ctrl)->watch);
}

/* two visible police of the second GenChar list fuse into gears that fight each other, else two free visible
 * gears fight */
void __fastcall AICtrl_Belligerant(void *ctrl) {
	cAICtrl *self = ctrl;
	cEntity *first = NULL, *second = NULL;
	Nto1Node *n;

	for (n = Logic_ListFirst(g_pWorld->logic.genChars.police); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		if (CGameModel_IsVisible((CGameModel *)AICtrl_Model(e))) {
			if (first) {
				second = e;
			} else {
				first = e;
			}
		}
	}
	if (first && second && AICtrl_CanFuse(self, first) && AICtrl_CanFuse(self, second)) {
		cEntity *g1 = NULL, *g2 = NULL;
		AICtrl_Fuse(self, first, &g1);
		AICtrl_Fuse(self, second, &g2);
		if (g1 && g2) {
			cLocalLogic *l1 = g1->lLogic, *l2 = g2->lLogic;
			if (l1 && l2) {
				l1->vftable->SetState(l1, 2, g2);
				l2->vftable->SetState(l2, 2, g1);
				return;
			}
		}
	}
	cBaseLocalLogic *fl = NULL, *sl = NULL;
	first = NULL;
	second = NULL;
	for (n = Logic_ListFirst(self->gears); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		if (!CGameModel_IsVisible((CGameModel *)AICtrl_Model(e))) {
			continue;
		}
		cBaseLocalLogic *l = e->lLogic;
		if (l->idle || l->alwaysLock || l->busy || Obj(e)->radarShow || l->state == 1 || l->state == 2) {
			continue;
		}
		if (first) {
			second = e;
			sl = l;
		} else {
			first = e;
			fl = l;
		}
	}
	if (first && second) {
		fl->vftable->SetState(fl, 2, second);
		sl->vftable->SetState(sl, 2, first);
	}
}

byte __thiscall AICtrl_CanFuse(void *ctrl, cEntity *e) {
	char name[0x80];
	vec3_t t, c;
	cEntity *hit;

	(void)ctrl;
	if (!e || !e->vftable->IsType(e, TYEN_WALKCHAR) || WChar(e)->wl.life <= 0 || strcmp(Logic_TypeName(e),
		"Police")) {
		return 0;
	}
	cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(e);
	if (!l || !AICtrl_AIActive(e->vftable->GetAI(e)) || l->busy) {
		return 0;
	}
	int st = l->state;
	if (st < 0 || (st > 2 && st != 6)) {
		return 0;
	}
	AICtrl_Rename(name, "Gear", e->name + 6);
	if (World_FindEntity(name)) {
		return 0;
	}
	const vec3_t *p = e->vftable->GetPos(e, &t);
	c.x = p->x + 0.0f;
	c.y = p->y + 145.0f;
	c.z = p->z + 0.0f;
	if (Engine_TestSphere(&c, &c, 180.0f, NULL, NULL)) {
		return 0;
	}
	return !cWorld_TestSphereEntities(g_pWorld, e, (void *)0x10, &c, 180.0f, &hit, 1, 0);
}

/* the gear may become a police again: no police of its number */
byte __thiscall AICtrl_CanBePolice(void *ctrl, cEntity *e) {
	char name[0x80];

	(void)ctrl;
	if (!e || !e->vftable->IsType(e, TYEN_WALKCHAR) || WChar(e)->wl.life <= 0 || strcmp(Logic_TypeName(e),
		"Gear")) {
		return 0;
	}
	AICtrl_Rename(name, "Police", e->name + 4);
	return !World_FindEntity(name);
}

/* the script CreatePolice (name, position) makes the police, the gear goes */
byte __thiscall AICtrl_GearToPolice(void *ctrl, cEntity *gear) {
	char name[0x80];
	vec3_t t;

	if (!AICtrl_CanBePolice(ctrl, gear)) {
		return 0;
	}
	AICtrl_Rename(name, "Police", gear->name + 4);
	cPy_Function_AddParamS(g_CreatePolice, name);
	cPy_Function_AddParamF(g_CreatePolice, gear->vftable->GetPos(gear, &t)->x);
	cPy_Function_AddParamF(g_CreatePolice, gear->vftable->GetPos(gear, &t)->y);
	cPy_Function_AddParamF(g_CreatePolice, gear->vftable->GetPos(gear, &t)->z);
	cPy_Function_Execute(g_CreatePolice, 0);
	(void)World_FindEntity(name);
	cEntity_Del(gear);
	return 1;
}

void __fastcall AICtrl_UpdateSentinels(void *ctrl);
void __fastcall AICtrl_UpdateGears(void *ctrl);

/* the alarm state, the voices of the gear nearest to the player ("Chase" at random with the alarm, "Abort" after
 * it), the sentinels and the gears */
void __fastcall AICtrl_Think(void *ctrl) {
	cAICtrl *self = ctrl;
	vec3_t t;

	self->level = -1;
	self->alarmOn = g_pWorld->alarm.on;
	self->usr = ChType_Usr0();
	self->sameChar = self->alarmOn && !strcmp(g_pWorld->alarm.lastChars, g_pWorld->alarm.chars);
	self->notDefChar = cString_CmpStr(g_UsrDefChar, g_pWorld->alarm.chars) != 0;
	if (!self->usr) {
		return;
	}
	if (!self->alarmOn) {
		if (self->alarm) {
			self->chased = 1;
		}
	} else {
		if ((float)(Weap_Rand01() * 4.5 - 3.5) > 0.0f) {
			cEntity *g = cEntityList_Nearest(self->gears, self->usr->vftable->GetPos(self->usr, &t), NULL, NULL);
			if (g) {
				Sound_PlayVoice(g->name, "Chase", 0, 1);
			}
		}
		self->chased = 0;
	}
	if (self->chased) {
		cEntity *g = cEntityList_Nearest(self->gears, self->usr->vftable->GetPos(self->usr, &t), NULL, NULL);
		if (!g) {
			g = cEntityList_Nearest(g_pWorld->logic.genChars.police, self->usr->vftable->GetPos(self->usr, &t), NULL,
				NULL);
		}
		if (g && Sound_PlayVoice(g->name, "Abort", 0, 1)) {
			self->chased = 0;
		}
	}
	AICtrl_UpdateSentinels(self);
	AICtrl_UpdateGears(self);
	self->alarm = self->alarmOn;
	self->lastUsr = self->usr;
}

byte __thiscall AICtrl_Fuse(void *ctrl, cEntity *police, cEntity **out) {
	char name[0x80];
	vec3_t t;
	cObjEntity *o = (cObjEntity *)police;

	if (out) {
		*out = NULL;
	}
	if (!AICtrl_CanFuse(ctrl, police)) {
		return 0;
	}
	cActCtrl_Play(ChType_ActCtrl(police), "Fusion", 1, 1.0f);
	o->move.movePos = *police->vftable->GetPos(police, &t);
	o->move.moveAng = *Obj_Vtbl(police)->GetAng(police, &t);
	cObjMove_SetTime(&o->move, 0.01f);
	cEntity_CollisionOff(police);
	cPrimitive_SetSize(&WChar(police)->wl.prim, 0.0f);
	*(uint16_t *)&WChar(police)->wl.arrow.canUse = 0;
	WChar(police)->wl.god = 1;
	AICtrl_Rename(name, "Gear", police->name + 6);
	cPy_Function_AddParamS(g_CreateGear, name);
	cPy_Function_AddParamF(g_CreateGear, police->vftable->GetPos(police, &t)->x);
	cPy_Function_AddParamF(g_CreateGear, police->vftable->GetPos(police, &t)->y);
	cPy_Function_AddParamF(g_CreateGear, police->vftable->GetPos(police, &t)->z);
	cPy_Function_Execute(g_CreateGear, 0);
	cEntity *gear = World_FindEntity(name);
	CHECK(gear);
	CHECK(gear->vftable->IsType(gear, TYEN_WALKCHAR));
	Obj_Vtbl(gear)->SetEffect(gear, 3);
	Obj_Vtbl(gear)->SetEffectParam(gear, 0, 1.0f);
	Obj_Vtbl(gear)->SetAng(gear, Obj_Vtbl(police)->GetAng(police, &t));
	cActCtrl_Play(ChType_ActCtrl(gear), "PostFusion", 1, 1.0f);
	cTimer_SetSpeedChecked(&Obj(gear)->timer, 0.001f);
	WChar(gear)->wl.god = 1;
	if (out) {
		*out = gear;
	}
	return 1;
}

/* the nearest gear with a free line attacks the visible target (not with seen 1 behind a wall); a nearer police
 * fuses into a gear instead; the bishops of the third list attack a player near them (BishopAttackOnAlarm) */
void __thiscall AICtrl_GearAttack(void *ctrl, cEntity *target, int seen) {
	cAICtrl *self = ctrl;
	vec3_t t1, t2, t3;
	float d1, d2, d;

	if (self->attacking < 2 && AICtrl_Model(target) && CGameModel_IsVisible((CGameModel *)AICtrl_Model(target))) {
		cEntity *gear = cEntityList_Nearest(self->gears, target->vftable->GetPos(target, &t1), target, &d1);
		byte free = 0;
		if (gear) {
			const vec3_t *tp = target->vftable->GetPos(target, &t2);
			if (!Engine_TestLine(gear->vftable->GetPos(gear, &t3), tp, NULL, NULL)) {
				free = 1;
				goto call;
			}
		}
		cEntity *police = cEntityList_Nearest(g_pWorld->logic.genChars.police, target->vftable->GetPos(target, &t3),
			target, &d2);
		if (d1 > d2) {
			if (police) {
				AICtrl_Fuse(self, police, NULL);
			}
			goto bishops;
		}
call:
		if (gear && (!(byte)seen || free) && cGearLocalLogic_Call(gear->lLogic, target)) {
			Con_Debugf("cIntPoliceLogic : Gear:'%s' Attack '%s'\n", gear->name, target->name);
		}
	}
bishops:
	if (g_BishopAttackOnAlarm && target && target == cWorld_GetUsrEntity(g_pWorld, 0)) {
		cEntity *b = cEntityList_Nearest(g_pWorld->logic.genChars.bishops, target->vftable->GetPos(target, &t1),
			target, &d);
		cBaseLocalLogic *l;
		if (b && (l = (cBaseLocalLogic *)Logic_GetLocalLogic(b)) && l->state != 4 && 3000.0f > d) {
			l->vftable->SetState(l, 4, target);
		}
	}
}

/* the gears update with 0.5 s, then the ones in state 1 or 2 are counted */
void __fastcall AICtrl_UpdateGears(void *ctrl) {
	cAICtrl *self = ctrl;
	Nto1Node *n;

	for (n = Logic_ListFirst(self->gears); n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		l->vftable->Update(l, 0.5f);
	}
	self->attacking = 0;
	for (n = Logic_ListFirst(self->gears); n; n = Logic_ListNext(n)) {
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(n->data);
		if ((unsigned)(l->state - 1) < 2) {
			self->attacking++;
		}
	}
}

void __fastcall AICtrl_Init(void *ctrl) {
	cAICtrl *self = ctrl;

	self->sentinels = cWorld_NewEntityList(g_pWorld, "Sentinel");
	self->sentinels->func = (int (*)(void))(void *)AICtrl_MakeSentinelLogic;
	cPy_Function_SetNative(&self->sentinels->onDeath, (nativefunc_t)AICtrl_SentinelDeath);
	self->gears = cWorld_NewEntityList(g_pWorld, "Gear");
	self->gears->func = (int (*)(void))(void *)AICtrl_MakeGearLogic;
}

/* a sentinel to watch pos now */
void __thiscall AICtrl_WatchNow(void *ctrl, const vec3_t *pos) {
	((cAICtrl *)ctrl)->watchTime = 0.0f;
	AICtrl_SendSentinelToWatch(ctrl, pos);
}

/* the script C function Police2GearEnd (name, event, float): the gear of the police is done, the police goes */
void AICtrl_Police2GearEnd(cAbstractFile *params) {
	char name[0x80], event[0x80], gname[0x80];
	float f;

	cAbstractFile_ReadString(params, name, sizeof(name));
	cAbstractFile_ReadString(params, event, sizeof(event));
	params->vftable->Read(params, &f, 4);
	cEntity *poli = World_FindEntity(name);
	if (!poli) {
		Con_Bluef("ERROR!:Police2GearEnd('%s','%s',%f):\n\tEntity Not found!\n", name, event, (double)f);
		return;
	}
	CHECK(poli->vftable->IsType(poli, TYEN_WALKCHAR));
	AICtrl_Rename(gname, "Gear", poli->name + 6);
	cEntity *gear = World_FindEntity(gname);
	CHECK(gear);
	Obj_Vtbl(gear)->SetEffect(gear, 0);
	Obj_Vtbl(gear)->SetEffectParam(gear, 0, 0.0f);
	cTimer_SetSpeedChecked(&Obj(gear)->timer, 1.0f);
	WChar(gear)->wl.god = 0;
	cEntity_Del(poli);
}

/* the watching sentinel, else the one nearest to pos, goes to watch it (not while it accuses or attacks);
 * SentinelNextGotoTime before the next */
void __thiscall AICtrl_SendSentinelToWatch(void *ctrl, const vec3_t *pos) {
	cAICtrl *self = ctrl;

	if (!(0.0f >= self->watchTime)) {
		return;
	}
	cEntity *s = self->watch.target;
	if (!s) {
		s = cEntityList_Nearest(self->sentinels, pos, NULL, NULL);
	}
	cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(s);
	if (l && (l->state < 2 || l->state > 4)) {
		Con_Debugf("Sending %s to search the area.\n", l->parent->name);
		self->watchTime = g_SentinelNextGotoTime;
		cSentinelLocalLogic_Watch(l, pos);
	}
}

/* the sentinels update with 0.5 s; the watch link goes to the one in state 5, the first that accuses sends the
 * others back */
void __fastcall AICtrl_UpdateSentinels(void *ctrl) {
	cAICtrl *self = ctrl;
	cBaseLocalLogic *found = NULL;
	Nto1Node *n;
	Nto1Node *w = &self->watch;

	self->alerted = 0;
	self->accusing = 0;
	CHECK(w->data);
	Nto1Node_Unlink(w);
	for (n = Logic_ListFirst(self->sentinels); n; n = Logic_ListNext(n)) {
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(n->data);
		AICtrl_SentinelUpdate(l, 0.5f, found != NULL);
		if (l->state == 5) {
			cEntity *e = n->data;
			CHECK(w->data);
			Nto1Node_Unlink(w);
			Nto1Node *h = &e->links;
			w->target = h->target;
			w->next = h->next;
			w->prev = h;
			h->next->prev = w;
			h->next = w;
		} else if (l->state == 6) {
			self->alerted = 1;
		}
		if (AICtrl_SentinelAccuses(l) && !found) {
			self->accusing = 1;
			found = l;
		}
	}
	float wt = self->watchTime - 0.5f;
	self->watchTime = wt;
	if (0.0f >= wt) {
		self->watchTime = 0.0f;
	}
	if (!found) {
		return;
	}
	self->watchTime = g_SentinelNextGotoTime;
	cBaseLocalLogic *lw = (cBaseLocalLogic *)Logic_GetLocalLogic(w->target);
	if (lw && lw != found) {
		cSentinelLocalLogic_Return(lw);
		Nto1Node_UnlinkChecked(w);
	}
	for (n = Logic_ListFirst(self->sentinels); n; n = Logic_ListNext(n)) {
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(n->data);
		if (found != l && (unsigned)(l->state - 2) <= 2) {
			cSentinelLocalLogic_Return(l);
		}
	}
}

/* with the alarm the sentinel nearest to the player accuses it (check: with a free line only) */
byte __thiscall AICtrl_SendAccuser(void *ctrl, byte check) {
	cAICtrl *self = ctrl;
	vec3_t t, t2, t3;

	if (!g_pWorld->alarm.on) {
		return 0;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr || !usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
		return 0;
	}
	cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(
		cEntityList_Nearest(self->sentinels, usr->vftable->GetPos(usr, &t), NULL, NULL));
	if (!l) {
		return 0;
	}
	if (check) {
		cEntity *s = l->parent;
		const vec3_t *up = usr->vftable->GetPos(usr, &t2);
		if (Engine_TestLine(s->vftable->GetPos(s, &t3), up, NULL, NULL)) {
			return 0;
		}
	}
	if (l->state == 2 || self->accusing) {
		return 0;
	}
	Con_Debugf("Sending %s to acuse the main character.\n", l->parent->name);
	l->vftable->SetState(l, 2, usr);
	return 1;
}

void __thiscall AICtrl_SendSentinelToNPC(void *ctrl, cEntity *e) {
	cAICtrl *self = ctrl;
	vec3_t t, t2, t3;

	if (!cEntityList_GetFirst((cEntityList *)self->sentinels) || self->alerted || g_pWorld->alarm.on) {
		return;
	}
	if (e->vftable->IsType(e, TYEN_WALKCHAR) &&
		(!strcmp(Logic_TypeName(e), "Gear") || !strcmp(Logic_TypeName(e), "Sentinel"))) {
		return;
	}
	if (!Logic_GetLocalLogic(e)) {
		return;
	}
	float d = cWorld_GetMinCamDistSq(g_pWorld, e->vftable->GetPos(e, &t));
	if (!(g_SentinelAbortyNPCAttDist * g_SentinelAbortyNPCAttDist > d)) {
		return;
	}
	cBaseLocalLogic *s = (cBaseLocalLogic *)Logic_GetLocalLogic(
		cEntityList_Nearest(self->sentinels, e->vftable->GetPos(e, &t), e, NULL));
	if (s->state == 6) {
		return;
	}
	cEntity *sp = s->parent;
	const vec3_t *ep = e->vftable->GetPos(e, &t2);
	if (!Engine_TestLine(sp->vftable->GetPos(sp, &t3), ep, NULL, NULL)) {
		s->vftable->SetState(s, 6, e);
	}
}

void __thiscall AICtrl_RaiseLevel(void *ctrl, int level) {
	cAICtrl *self = ctrl;

	if (self->level < level) {
		self->level = level;
	}
}

/* every 0.5 s the think; without the alarm the fights (PoliceLogicBelligerant percent) */
void __thiscall AICtrl_Update(void *ctrl, float dt) {
	cAICtrl *self = ctrl;
	float t = dt + self->timer;

	self->timer = t;
	if (0.5f >= t) {
		return;
	}
	self->timer = 0.0f;
	AICtrl_Think(self);
	if (!g_pWorld->alarm.on) {
		float r = (float)(Weap_Rand01() * 99.0 + 1.0);
		if ((float)g_PoliceLogicBelligerant > r) {
			AICtrl_Belligerant(self);
		}
	}
}

void __thiscall AICtrl_GearsChase(void *ctrl, cEntity *e) {
	cAICtrl *self = ctrl;
	byte found = 0;

	for (Nto1Node *n = Logic_ListFirst(self->gears); n; n = Logic_ListNext(n)) {
		if (n->data == e) {
			continue;
		}
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(n->data);
		if (l->state != 1 && l->state != 2) {
			continue;
		}
		cEntity *t = l->target.target;
		if (t == ChType_Usr0()) {
			l->vftable->SetState(l, l->state, e);
			found = 1;
		}
	}
	if (!found) {
		AICtrl_GearAttack(self, e, 0);
	}
}

/* the makers of the logics of the lists "Gear" and "Sentinel" (cEntityListData.func) */
void *AICtrl_MakeGearLogic(cEntity *e) {
	void *m = operator_new(sizeof(cGearLocalLogic));

	return m ? cGearLocalLogic_ctor(m, e) : NULL;
}

void *AICtrl_MakeSentinelLogic(cEntity *e) {
	void *m = operator_new(0x50);

	return m ? cSentinelLocalLogic_ctor(m, e) : NULL;
}

/* the death native of the list "Sentinel": it leaves the list, a sentinel goes to watch the place at once */
void AICtrl_SentinelDeath(cAbstractFile *params) {
	char name[0x80];
	vec3_t t;

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *v = ChType_CheckChar(name);
	cEntity_SetList(v, NULL);
	const vec3_t *p = v->vftable->GetPos(v, &t);
	g_pWorld->logic.aiCtrl.watchTime = 0.0f;
	AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, p);
}
