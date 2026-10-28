/*
 * The money string of the world (cStrMoney at g_pWorld + 0x2078, include/scrap/logic.h; the source between
 * MessengerLogic.cpp and NurseLogic.cpp): every StrMoneyUpdate seconds the first digit (from the right) that differs
 * from cWorld.money changes, so the shown money counts towards the real one. OnMoneyReachCallback runs when the
 * money is at least OnMoneyReachValue.
 */
#include <string.h>

#include "scrap/logic.h"
#include "scrap/world.h"

void *__thiscall cStrMoney_ctor(void *obj) {
	cStrMoney *self = obj;

	self->money = 0;
	self->end = 0;
	self->len = 1;
	memcpy(self->digits, "0000000000000000", 16);
	self->timer = 0.0f;
	cPy_Function_SetNative(g_OnMoneyReachCallback, NULL);
	return self;
}

/* the digits from the first one not "0", "0" for none */
const char *__fastcall cStrMoney_GetStr(cStrMoney *self) {
	if (!self->len) {
		return GAME_STR(str_3ba568, 0x3ba568);
	}
	return (const char *)self + 0x10 - self->len;
}

/* the lowest digit that differs from the money takes its value */
void __fastcall cStrMoney_UpdateDigits(cStrMoney *self) {
	byte first = 1;
	int v = self->money;
	char *p = &self->digits[15];

	self->len = 0;
	for (int i = 1; i < 16; i++, p--) {
		int q = v / 10;
		char c = (char)((byte)v - (byte)(q * 10) + '0');
		if (first && c != *p) {
			*p = c;
			first = 0;
		}
		if (*p > '0') {
			self->len = (char)i;
		}
		v = q;
	}
}

void __thiscall cStrMoney_Update(void *obj, float dt) {
	cStrMoney *self = obj;
	int money = self->money;
	float t = self->timer - dt;

	self->timer = t;
	if (money < 0) {
		self->money = 0;
	}
	if (0.0f > t) {
		cStrMoney_UpdateDigits(self);
		self->timer = g_StrMoneyUpdate + self->timer;
		if ((byte)cPy_Function_IsValid(g_OnMoneyReachCallback) && self->money >= g_OnMoneyReachValue) {
			cPy_Function_Execute(g_OnMoneyReachCallback, 1);
		}
	}
}
