/*
 * FXElementsOrbit.cpp of the game: the orbit buoys, lights over their models that blink in turns, see
 * include/scrap/fxelements.h.
 */
#include <string.h>

#include "scrap/fxelements.h"
#include "scrap/scrap.h"

#define ORBIT_NAME_SKIP 11	/* "DM_Element_" before the name of the element in the dummy */

/* ---- construction ---- */

cFXElementsOrbitBoya *__thiscall cFXElementsOrbitBoya_ctor(cFXElementsOrbitBoya *self, const char *name,
	const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSORBITBOYA_VFTABLE;
	CGameParticles_ctor(&self->light);
	return self;
}

/* no buoys yet, the fades of a column 1/8 apart */
cFXElementsOrbitBoyaController *__thiscall cFXElementsOrbitBoyaController_ctor(cFXElementsOrbitBoyaController *self,
	const char *name, const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSORBITBOYACONTROLLER_VFTABLE;
	for (int col = 0; col < FXORBITBOYACOLUMNS; col++) {
		for (int i = 0; i < FXORBITBOYACOLUMNCOUNT; i++) {
			self->boya[col][i] = NULL;
			self->type[col][i] = 0;
			self->fade[col][i] = 1.0f - (float)i * 0.125f;
		}
	}
	return self;
}

/* the light is released twice, by the class and by the member destructor */
void __fastcall cFXElementsOrbitBoya_dtor(cFXElementsOrbitBoya *self) {
	self->base.vftable = CFXELEMENTSORBITBOYA_VFTABLE;
	CGameParticles_Release(&self->light);
	CGameParticles_Release(&self->light);
	cMapElement_dtor(&self->base);
}

void __fastcall cFXElementsOrbitBoyaController_dtor(cFXElementsOrbitBoyaController *self) {
	self->base.vftable = CFXELEMENTSORBITBOYACONTROLLER_VFTABLE;
	cMapElement_dtor(&self->base);
}

cFXElementsOrbitBoya *__thiscall cFXElementsOrbitBoya_Delete(cFXElementsOrbitBoya *self, byte flags) {
	cFXElementsOrbitBoya_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cFXElementsOrbitBoyaController *__thiscall cFXElementsOrbitBoyaController_Delete(cFXElementsOrbitBoyaController *self,
	byte flags) {
	cFXElementsOrbitBoyaController_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall cFXElementsOrbitBoya_ReleaseLight(cFXElementsOrbitBoya *self) {
	CGameParticles_Release(&self->light);
}

void __fastcall cFXElementsOrbitBoyaController_Release(cFXElementsOrbitBoyaController *self) {
	(void)self;
}

/* ---- the elements ---- */

byte __fastcall cFXElementsOrbitBoya_Init(cFXElementsOrbitBoya *self) {
	vec3_t tmp;

	cMapElement_Init(&self->base);
	CHECK(CGameModel_IsLoaded(&self->base.Mod));
	self->pos = *CGameModel_GetPos(&self->base.Mod, &tmp);
	self->dcRoot = CGameModel_FindNodoR(&self->base.Mod, "DC_Root");
	CHECK(self->dcRoot != -1);
	CGameParticles_Attach(&self->light, &self->base.Mod, CGameModel_FindNodoR(&self->base.Mod, "DF_Light"));
	CHECK(self->light.SistPart);
	return 1;
}

void __thiscall cFXElementsOrbitBoya_Update(cFXElementsOrbitBoya *self, float dt) {
	cMapElement_Update(&self->base, dt);
}

byte __fastcall cFXElementsOrbitBoyaController_Init(cFXElementsOrbitBoyaController *self) {
	cMapElement_Init(&self->base);
	return 1;
}

/* the lights of the buoys sized by the distance to the camera, faded by their turn */
void __thiscall cFXElementsOrbitBoyaController_Update(cFXElementsOrbitBoyaController *self, float dt) {
	cMapElement_Update(&self->base, dt);
	for (int col = 0; col < FXORBITBOYACOLUMNS; col++) {
		for (int i = 0; i < FXORBITBOYACOLUMNCOUNT; i++) {
			cFXElementsOrbitBoya *b = self->boya[col][i];
			if (!b) {
				continue;
			}
			float f = self->fade[col][i] - dt * 0.8f;
			self->fade[col][i] = f;
			if (0.0f > f) {
				self->fade[col][i] = 1.0f;
			}
			if (!CGameModel_IsVisible(&b->base.Mod)) {
				CGameParticles_SetVisible(&b->light, 0);
				continue;
			}
			CGameParticles_SetVisible(&b->light, 1);
			f = self->fade[col][i];
			float a;
			if (0.2f > f) {
				float t = f * 5.0f;
				a = (3.0f - (t + t)) * (t * t);
			} else if (0.6f > f) {
				a = 1.0f - (f - 0.2f) * 2.5f;
			} else {
				a = 0.0f;
			}
			vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
			vec3_t tmp;
			const vec3_t *p = CGameModel_GetPos(&b->base.Mod, &tmp);
			float dy = p->y - cam.y;
			float dx = p->x - cam.x;
			float dz = p->z - cam.z;
			float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			float size = dist * 0.01f + (a * 90.0f + 250.0f);
			uint32_t color = (uint32_t)(0xff - (int)(a * 255.0f)) << 24 | 0xffe6be;
			int type = self->type[col][i];
			if (type == 0) {
				CGameParticles_SetParticle(&b->light, 0, -10.0f, -195.0f, 0.0f, size, color, 5, 0.0f);
				CGameParticles_SetParticle(&b->light, 1, 10.0f, 200.0f, 0.0f, size, color, 5, 0.0f);
			} else if (type == 1) {
				CGameParticles_SetParticle(&b->light, 0, 5.0f, -305.0f, -45.0f, size, color, 5, 0.0f);
				CGameParticles_SetParticle(&b->light, 1, 5.0f, 255.0f, 40.0f, size, color, 5, 0.0f);
			}
		}
	}
}

void __thiscall cFXElementsOrbitBoyaController_SetBoya(cFXElementsOrbitBoyaController *self, int type, int column,
	int number, cFXElementsOrbitBoya *boya) {
	CHECK(column >= 0 && column < FXORBITBOYACOLUMNS);
	CHECK(number >= 0 && number < FXORBITBOYACOLUMNCOUNT);
	self->type[column][number] = type;
	self->boya[column][number] = boya;
}

/* the controllers first, then the buoys "OrbitBoya<type>_<column><number>" added to the last controller */
void FXElementsOrbit_Create(void) {
	char buf[0x80];
	char *name = buf + ORBIT_NAME_SKIP;
	vec3_t pos, rot;
	float ang[2];
	byte flag;
	cFXElementsOrbitBoyaController *ctrl = NULL;

	DummySearch_Start("DM_Element_OrbitBoyaController", 0);
	if (!DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		return;
	}
	do {
		char *p = strrchr(name, '_');
		if (p) {
			*p = 0;
		}
		void *mem = operator_new(sizeof(cFXElementsOrbitBoyaController));
		if (mem) {
			rot.x = ang[0];
			rot.y = ang[1];
			rot.z = 0.0f;
			ctrl = cFXElementsOrbitBoyaController_ctor(mem, name, &pos, &rot);
		} else {
			ctrl = NULL;
		}
		CHECK(ctrl);
		MapElement_Add(&ctrl->base);
	} while (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag));

	DummySearch_Start("DM_Element_OrbitBoya*", 1);
	while (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		char *p = strrchr(name, '_');
		if (p) {
			*p = 0;
			p++;
		}
		if (!strcmp(name, "OrbitBoyaController")) {
			continue;
		}
		cFXElementsOrbitBoya *b = operator_new(sizeof(cFXElementsOrbitBoya));
		if (b) {
			rot.x = ang[0];
			rot.y = ang[1];
			rot.z = 0.0f;
			cMapElement_ctor(&b->base, name, &pos, &rot);
			b->base.vftable = CFXELEMENTSORBITBOYA_VFTABLE;
			CGameParticles_ctor(&b->light);
		}
		/* the original reads the digits also without '_' */
		int number = p[1] - '0';
		int column = p[0] - '0';
		int type = p[-2] - '0';
		CHECK(column >= 0 && column < FXORBITBOYACOLUMNS);
		CHECK(number >= 0 && number < FXORBITBOYACOLUMNCOUNT);
		ctrl->type[column][number] = type;
		ctrl->boya[column][number] = b;
		CHECK(b);
		MapElement_Add(&b->base);
	}
}
