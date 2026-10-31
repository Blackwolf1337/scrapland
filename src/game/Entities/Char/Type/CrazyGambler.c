/*
 * Entities/Char/Type/CrazyGambler.cpp of the game: the crazy gambler of the meeting, whose hair glows on the head
 * model it wears (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/embeddedfxchar.h"

/* params: an event, the head entity, a float. The hair glow of the gambler goes to the head node of that model. */
void ChType_FXCrazyGamblerSetHeadModel(cAbstractFile *params) {
	char event[0x80], head[0x80];
	float unused;
	const char *node;

	cAbstractFile_ReadString(params, event, 0x80);
	cAbstractFile_ReadString(params, head, 0x80);
	params->vftable->Read(params, &unused, 4);
	cEntity *e = World_FindEntity("MeetGamblerCrazyGambler");
	CHECK(e);
	CHECK(Entity_IsType(e, TYEN_WITH_LIFE));
	cEmbeddedFXCrazyGambler *fx = WChar_FX(e);
	CHECK(fx);
	cEntity *h = World_FindEntity(head);
	CHECK(h);
	CHECK(Entity_IsType(h, TYEN_WITH_LIFE));
	CHECK(CGameModel_IsLoaded(Obj_Vtbl(h)->GetModel(h)));
	if (!strcmp(head, "MeetGamblerCrazyHead")) {
		node = "_R_Gambler_Cabeza-Pelos";
	} else if (!strcmp(head, "MeetGamblerCrazyGambler")) {
		node = "R_Gambler_Cabeza-Pelos";
	} else {
		return;
	}
	cEmbeddedFXCrazyGambler_SetHead(fx, Obj_Vtbl(h)->GetModel(h), node);
}
