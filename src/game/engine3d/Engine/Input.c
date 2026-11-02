/*
 * The base of the input of the engine3d, original source Engine/entrada.cpp (include/scrap/e3dinput.h). The engine
 * only makes the input that reads nothing (CEntradaNula in Engine.c); the game reads its devices in src/game/Input.
 */
#include <string.h>

#include "scrap/e3dinput.h"

/* one at a time, the state cleared */
IInput *IInput_ctor(IInput *self) {
	self->vftable = IINPUT_VFTABLE;
	CHECK(!g_InputInit);
	g_InputInit = 1;
	memset(self->mouse, 0, sizeof(*self) - offsetof(IInput, mouse));
	return self;
}

void IInput_dtor(IInput *self) {
	self->vftable = IINPUT_VFTABLE;
	g_InputInit = 0;
}

IInput *IInput_Delete(IInput *self, byte flags) {
	IInput_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(IInput));
	}
	return self;
}
