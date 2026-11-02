/*
 * Entities/Char/Type/Banker.cpp of the game: the bank director and the banker, who steal money from the character
 * they talk to, and the shared steps of the type natives (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"

void ChType_ReadParams(cAbstractFile *f, char *name, char *event) {
	float unused;

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, event, 0x80);
	f->vftable->Read(f, &unused, 4);
}

cEntity *ChType_CheckChar(const char *name) {
	cEntity *v = World_FindEntity(name);

	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	return v;
}

/* params: the bank director */
void ChType_BankDirectorActionPress(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	void *ctrl = ChType_ActCtrl(v);
	CHECK(ctrl);
	cActCtrl_Play(ctrl, "ActionInit", 0, 1.0f);
	if (!strcmp(cActCtrl_GetActionName(ctrl, 0), "ActionInit")) {
		cActCtrl_SetDefLoop(ctrl, "Action", 0);
	}
}

void ChType_BankDirectorActionRelease(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	void *ctrl = ChType_ActCtrl(v);
	CHECK(ctrl);
	if (!strncmp(cActCtrl_GetActionName(ctrl, 0), "Action", 6)) {
		cActCtrl_SetDefLoop(ctrl, "Relax", 1);
		cActCtrl_Play(ctrl, "ActionEnd", 0, 1.0f);
	}
}

void ChType_BankDirectorEndActionInit(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	void *ctrl = ChType_ActCtrl(v);
	CHECK(ctrl);
	if (!WChar(v)->control.act) {
		cActCtrl_SetDefLoop(ctrl, "Relax", 1);
		cActCtrl_Play(ctrl, "ActionEnd", 0, 1.0f);
	}
}

/* params: the banker, an event, a float. The player steals from its victim, an AI banker from the victim of its
 * logic, which gets told by how much the victim looks away (0 face to face, 100 from behind). */
void ChType_BankerAttack(cAbstractFile *params) {
	char name[0x80], event[0x80];
	cEntity *victim = NULL;

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	cEntity *thief = v;
	CHECK(v && Entity_IsType(v, TYEN_WALKCHAR));
	if (v == ChType_Usr0()) {
		victim = ChType_BankerVictim(v);
	}
	ChType_Steal(&thief, &victim, g_BankerStealAmount, "Stats.BankDirector", 1.0f);
	cLocalLogic *l = Logic_GetLocalLogic(victim);
	if (l) {
		float d = NormAng(WChar(victim)->view.y - WChar(v)->view.y);
		float f = (float)((__builtin_fabs((double)d) * 100.0) / M_PI_D);
		if (25.0f > f) {
			f = 0.0f;
		} else if (f > 75.0f) {
			f = 100.0f;
		}
		Logic_Vtbl(l)->OnAttack(l, v, f);
	}
	if (v && victim) {
		cObjElementFXBankerSteal *el = FX_DropNew(g_FXBankerStealFile, 1.35f);
		if (el) {
			if (!el->started) {
				el->started = 1;
			}
			cObjElementFXBankerSteal_SetEntity(el, v);
			((cObjElementFXBankerSteal *)el)->target = victim;
		}
	}
}

cEntity *ChType_BankerVictim(cEntity *v) {
	cEntity *e = WChar_Victim(v);

	if (!e || !e->vftable->IsType(e, TYEN_WALKCHAR)) {
		return NULL;
	}
	if (WChar(e)->money <= 0 && e != cWorld_GetUsrEntity(g_pWorld, 0)) {
		return NULL;
	}
	if (g_pWorld->money <= 0 && e == cWorld_GetUsrEntity(g_pWorld, 0)) {
		return NULL;
	}
	return e;
}

byte ChType_CanRob(cEntity *thief, cEntity *v) {
	(void)thief;
	if (!v || !v->vftable->IsType(v, TYEN_WALKCHAR)) {
		return 0;
	}
	if (WChar(v)->money <= 0 && v != cWorld_GetUsrEntity(g_pWorld, 0)) {
		return 0;
	}
	if (g_pWorld->money <= 0 && v == cWorld_GetUsrEntity(g_pWorld, 0)) {
		return 0;
	}
	return 1;
}

cEntity *cWorld_GetUsrEntity(cWorld *world, int player) {
	cEntity *e = world->owners[player];

	if (e) {
		CHECK(Entity_IsType(e, TYEN_WITH_LIFE));
		return world->owners[player];
	}
	return NULL;
}

byte ChType_Steal(cEntity **thief, cEntity **victim, int amount, const char *stat, float frac) {
	if (*thief == ChType_Usr0()) {
		cEntity *v = *victim;
		if (!v) {
			return 0;
		}
		int n = (int)((float)WChar(v)->moneyReward * frac);
		if (n <= 0) {
			n = 1;
		}
		if (WChar(v)->money < n) {
			n = WChar(v)->money;
		}
		g_pWorld->money += n;
		if (stat) {
			SaveVar_Inc(stat, n);
		}
		WChar(*victim)->money -= n;
		return n != 0;
	}
	cLocalLogic *l = Logic_GetLocalLogic(*thief);
	if (!l) {
		return 0;
	}
	/* the character the logic deals with */
	*victim = ChLogic_Target(l);
	if (!ChType_CanRob(*thief, *victim)) {
		return 0;
	}
	cEntity *v = *victim;
	if (v == cWorld_GetUsrEntity(g_pWorld, 0)) {
		if (g_pWorld->money <= 0) {
			return 0;
		}
		g_pWorld->money = g_pWorld->money - amount;
		if (g_pWorld->money < 0) {
			g_pWorld->money = 0;
		}
		WChar(*thief)->money += amount;
		WChar(*thief)->moneyReward += amount / 10;
		return 1;
	}
	int m = WChar(v)->money;
	int n = WChar(v)->moneyReward;
	if (m < n) {
		n = m;
	}
	WChar(v)->money = m - n;
	WChar(*thief)->money += n;
	WChar(*thief)->moneyReward += n / 10;
	return n != 0;
}

cGVar GVar_FXBankerStealFile;

