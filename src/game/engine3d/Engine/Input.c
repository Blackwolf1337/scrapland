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

/* the slot AcquireKeys of a class that does only return 0 */
int IInput_AcquireKeysFalse(IInput *self, byte on) {
	(void)self;
	(void)on;
	return 0;
}

/* the slot AcquireMouse of a class that does only return 0 */
int IInput_AcquireMouseFalse(IInput *self, byte on) {
	(void)self;
	(void)on;
	return 0;
}

/* the slot Init of a class that does only return 0 */
int IInput_InitFalse(IInput *self, void *hwnd, void *hinst, byte exclusive) {
	(void)self;
	(void)hwnd;
	(void)hinst;
	(void)exclusive;
	return 0;
}

/* the slot ReadKeys of a class that does only return 0 */
int IInput_ReadKeysFalse(IInput *self) {
	(void)self;
	return 0;
}

/* the slot ReadMouse of a class that does only return 0 */
int IInput_ReadMouseFalse(IInput *self, int *out) {
	(void)self;
	(void)out;
	return 0;
}

/* the slot Release of a class that does only return 0 */
int IInput_ReleaseFalse(IInput *self) {
	(void)self;
	return 0;
}

/* the slot WndProc of a class that does nothing there */
void IInput_WndProcNop(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) {
	(void)self;
	(void)hwnd;
	(void)msg;
	(void)wparam;
	(void)lparam;
}

/* the vtable of IInput */
const IInputVtbl IInput_vftable = {
	.Delete = IInput_Delete,
	.Init = (void *)crt_purecall,
	.Release = (void *)crt_purecall,
	.ReadMouse = (void *)crt_purecall,
	.ReadKeys = (void *)crt_purecall,
	.AcquireMouse = (void *)crt_purecall,
	.AcquireKeys = (void *)crt_purecall,
	.WndProc = (void *)crt_purecall,
};
