/*
 * Logic/GenCharGroup.cpp of the game: the lists of the generic characters (cGenCharGroup at g_pWorld + 0x2238,
 * include/scrap/logic.h): "GenChars", "PoliceSmall", "Bishop" and the messengers. Their logics update every 0.13 s
 * and report the highest level of the round; the maker of the logics picks the class by the character type.
 */
#include <string.h>

#include "scrap/chartype.h"

/* defined further down or in another file without a header */
void *GenCharGroup_MakeLogic(cEntity *e);

cGVar GVar_BankDirectorAgressive;

cGVar GVar_BishopAttackOnAlarm;

cGVar GVar_GenCharAttackActive;

cGenCharGroup *cGenCharGroup_ctor(cGenCharGroup *self) {
	self->level = -1;
	self->timer = 0.0f;
	self->attackers = 0;
	g_BishopAttackOnAlarm = (GVar_BishopAttackOnAlarm.def.i);
	void *m = operator_new(sizeof(cMessengers));
	self->messengers = m ? cMessengers_ctor(m) : NULL;
	g_BankDirectorAgressive = (GVar_BankDirectorAgressive.def.i);
	g_GenCharAttackActive = (GVar_GenCharAttackActive.def.i);
	return self;
}

void cGenCharGroup_dtor(cGenCharGroup *self) {
	void *m = self->messengers;

	if (m) {
		cMessengers_dtor(m);
		operator_delete_sized(m, 0x14);
	}
}

/* one more attacker; 1 while fewer than two attack */
byte cGenCharGroup_TakeAttack(cGenCharGroup *self) {
	int n = self->attackers;

	self->attackers = n + 1;
	return n < 2;
}

static void GenCharGroup_UpdateList(cEntityListData *list) {
	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		Logic_Vtbl(l)->Update(l, 0.13f);
	}
}

void cGenCharGroup_Think(cGenCharGroup *self) {
	self->level = -1;
	GenCharGroup_UpdateList(self->genChars);
	GenCharGroup_UpdateList(self->police);
	GenCharGroup_UpdateList(self->bishops);
	cMessengers_Think(self->messengers);
}

int cGenCharGroup_GetLevel(cGenCharGroup *self) {
	return self->level;
}

void cGenCharGroup_Init(cGenCharGroup *self) {
	int (*make)(void) = (int (*)(void))(void *)GenCharGroup_MakeLogic;
	cEntityListData *l = cWorld_NewEntityList(g_pWorld, "Bishop");

	self->bishops = l;
	l->func = make;
	l = cWorld_NewEntityList(g_pWorld, "PoliceSmall");
	self->police = l;
	l->func = make;
	l = cWorld_NewEntityList(g_pWorld, "GenChars");
	self->genChars = l;
	l->func = make;
	cMessengers_Init(self->messengers);
}

void cGenCharGroup_RaiseLevel(cGenCharGroup *self, int level) {
	if (self->level < level) {
		self->level = level;
	}
}

void cGenCharGroup_Update(cGenCharGroup *self, float dt) {
	float t = self->timer + dt;

	self->attackers = 0;
	self->timer = t;
	if (!(0.13f >= t)) {
		self->timer = 0.0f;
		cGenCharGroup_Think(self);
	}
}

typedef void *(*GenChar_Ctor)(void *self, cEntity *parent);

/* the class of the logic by the type of the character, in the order the original compares (Mercenary as a
 * prefix) */
static const struct {
	const char *type;
	size_t prefix;
	uint32_t size;
	GenChar_Ctor ctor;
} g_GenCharTypes[] = {
	{ "Police", 0, sizeof(cLittlePoliceLocalLogic), cLittlePoliceLocalLogic_ctor },
	{ "BankDirector", 0, sizeof(cBankDirectorLocalLogic), (GenChar_Ctor)cBankDirectorLocalLogic_ctor },
	{ "BankMaster", 0, sizeof(cBankMasterLocalLogic), (GenChar_Ctor)cBankMasterLocalLogic_ctor },
	{ "Desktop", 0, sizeof(cDesktopLocalLogic), (GenChar_Ctor)cDesktopLocalLogic_ctor },
	{ "Mayor", 0, sizeof(cMayorLocalLogic), cMayorLocalLogic_ctor },
	{ "Berto", 0, sizeof(cBertoLocalLogic), (GenChar_Ctor)cBertoLocalLogic_ctor },
	{ "Betty", 0, sizeof(cBettyLocalLogic), (GenChar_Ctor)cBettyLocalLogic_ctor },
	{ "Bishop", 0, sizeof(cBishopLocalLogic), (GenChar_Ctor)cBishopLocalLogic_ctor },
	{ "Functionary", 0, sizeof(cFunctionaryLocalLogic), (GenChar_Ctor)cFunctionaryLocalLogic_ctor },
	{ "Rusty", 0, sizeof(cRustyLocalLogic), cRustyLocalLogic_ctor },
	{ "SputnikJunkyard", 0, sizeof(cSputnikJunkyardLocalLogic), (GenChar_Ctor)cSputnikJunkyardLocalLogic_ctor },
	{ "Maintenance", 0, sizeof(cMaintenanceLocalLogic), cMaintenanceLocalLogic_ctor },
	{ "CrazyGambler", 0, sizeof(cCrazyDealerLocalLogic), (GenChar_Ctor)cCrazyDealerLocalLogic_ctor },
	{ "PoliceBoss", 0, sizeof(cPoliceBossLocalLogic), (GenChar_Ctor)cPoliceBossLocalLogic_ctor },
	{ "Messenger", 0, sizeof(cMessengerLocalLogic), (GenChar_Ctor)cMessengerLocalLogic_ctor },
	{ "Mercenary", 9, sizeof(cMercenaryLocalLogic), (GenChar_Ctor)cMercenaryLocalLogic_ctor },
	{ "Boss", 0, sizeof(cBossLocalLogic), (GenChar_Ctor)cBossLocalLogic_ctor },
	{ "Humphrey", 0, sizeof(cHumphreyLocalLogic), (GenChar_Ctor)cHumphreyLocalLogic_ctor },
	{ "Human", 0, sizeof(cHumanLocalLogic), cHumanLocalLogic_ctor },
};

/* the logic of a character of the lists (cEntityListData.func) */
void *GenCharGroup_MakeLogic(cEntity *e) {
	CHECK(e && "e");
	CHECK(e->vftable->IsType(e, TYEN_WALKCHAR) && "e->IsType(TYEN_WALKCHAR)");
	const char *type = ChType_ActClass(ChType_ActCtrl(e))->name;
	for (size_t i = 0; i < sizeof(g_GenCharTypes) / sizeof(g_GenCharTypes[0]); i++) {
		size_t n = g_GenCharTypes[i].prefix;
		if (n ? strncmp(type, g_GenCharTypes[i].type, n) : strcmp(type, g_GenCharTypes[i].type)) {
			continue;
		}
		void *mem = operator_new(g_GenCharTypes[i].size);
		return mem ? g_GenCharTypes[i].ctor(mem, e) : NULL;
	}
	CHECK(0);
	return NULL;
}
