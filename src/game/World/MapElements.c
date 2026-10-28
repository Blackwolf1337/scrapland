/*
 * World/MapElements.cpp of the game: cMapElement, a model of the level, and the list of the elements in
 * g_pWorld->elements (Elements), made at the level start from the dummies DM_Element_* (see
 * include/scrap/fxelements.h).
 */
#include <string.h>

#include "scrap/filesys.h"
#include "scrap/fxelements.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"
#include "scrap/world.h"

#define CMAPELEMENT_VFTABLE 0x3cde68

/* ElementModelPath, the folder of the element models (a cString, "Models/Elements/") */
#define g_ElementModelPath GAME_DATA(char *, 0x4673d8)

/* ---- cMapElement ---- */

cMapElement *__thiscall cMapElement_ctor(cMapElement *self, const char *name, const vec3_t *pos, const vec3_t *rot) {
	char buf[128];

	self->vftable = Game_Ptr(CMAPELEMENT_VFTABLE);
	CGameModel_ctor(&self->Mod, NULL);
	cString_ctor(&self->name);
	if (name) {
		cString_Set(&self->name, name);
		Str_PrintfQuiet(buf, sizeof(buf), "%s%s/%s.m3d", g_ElementModelPath, self->name, self->name);
		CGameModel_LoadName(&self->Mod, buf);
		CGameModel_SetPivotNodo(&self->Mod, "DC_Root");
		CGameModel_SetPos(&self->Mod, pos);
		CGameModel_SetRot(&self->Mod, rot);
		CGameModel_EnableCollision(&self->Mod, 1, 1);
		Str_PrintfQuiet(buf, sizeof(buf), "%s%s/Anm/Loop.m3d", g_ElementModelPath, self->name);
		self->anim = CGameModel_AddAnim(&self->Mod, buf);
		CGameModel_SetAnim(&self->Mod, self->anim, 0, 0);
		self->next = NULL;
	}
	return self;
}

void __thiscall cMapElement_dtor(cMapElement *self) {
	self->vftable = Game_Ptr(CMAPELEMENT_VFTABLE);
	cString_dtor(&self->name);
	CGameModel_Reset(&self->Mod);
}

cMapElement *__thiscall cMapElement_Delete(cMapElement *self, byte flags) {
	cMapElement_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte __thiscall cMapElement_Init(cMapElement *self) {
	(void)self;
	return 1;
}

void __thiscall cMapElement_Update(cMapElement *self, float dt) {
	if (CGameModel_IsVisible(&self->Mod)) {
		CGameModel_RootMotion(&self->Mod, dt, NULL, NULL);
	}
}

/* ---- the list (one pointer, the newest element first) ---- */

void *__thiscall Elements_ctor(void *elements) {
	*(cMapElement **)elements = NULL;
	return elements;
}

void __thiscall Elements_dtor(void *elements) {
	cMapElement **list = elements;

	while (*list) {
		cMapElement *e = *list;

		*list = e->next;
		if (e) {
			e->vftable->Delete(e, 1);
		}
	}
}

/* the script function name with each element: name, position, the angles x and y in degrees */
void __thiscall Elements_Call(void *elements, const char *name) {
	cPy_Function func;
	cMapElement *e;
	vec3_t v;

	cPy_Function_ctor(&func);
	cPy_Function_SetFunc(&func, name);
	for (e = *(cMapElement **)elements; e; e = e->next) {
		cPy_Function_AddParamS(&func, e->name);
		cPy_Function_AddParamF(&func, CGameModel_GetPos(&e->Mod, &v)->x);
		cPy_Function_AddParamF(&func, CGameModel_GetPos(&e->Mod, &v)->y);
		cPy_Function_AddParamF(&func, CGameModel_GetPos(&e->Mod, &v)->z);
		cPy_Function_AddParamF(&func, (float)(((double)CGameModel_GetRot(&e->Mod, &v)->x * DEG_PER_PI) /
			M_PI_D));
		cPy_Function_AddParamF(&func, (float)(((double)CGameModel_GetRot(&e->Mod, &v)->y * DEG_PER_PI) /
			M_PI_D));
		cPy_Function_Execute(&func, 0);
	}
	cPy_Function_dtor(&func);
}

void __thiscall Elements_Create(void *elements) {
	cMapElement **list = elements;
	char buf[128];
	char *name = buf + 11;		/* behind "DM_Element_" */
	vec3_t pos, rot;
	byte mental = 0, orbit = 0, ab = 0;

	CHECK(*list == NULL);
	DummySearch_Start("DM_Element_*", 1);
	if (!DummySearch_NextEuler(buf, sizeof(buf), &pos, &rot)) {
		return;
	}
	do {
		cMapElement *e;
		char *p;
		void *mem;

		p = strchr(name, '_');
		if (p) {
			*p = 0;
		}
		if (strstr(name, "FXLensFlare")) {
			mem = operator_new(sizeof(cFXElementsLensFlare));
			e = mem ? &cFXElementsLensFlare_ctor(mem, NULL, &pos, &rot)->base : NULL;
		} else if (strstr(name, "FXLighting")) {
			mem = operator_new(sizeof(cFXElementsLighting));
			e = mem ? &cFXElementsLighting_ctor(mem, name, &pos, &rot)->base : NULL;
		} else if (strstr(name, "FXFireDrop")) {
			mem = operator_new(sizeof(cFXElementsFireDrop));
			e = mem ? &cFXElementsFireDrop_ctor(mem, name, &pos, &rot)->base : NULL;
		} else if (strstr(name, "MentalController")) {
			mental = 1;
			continue;
		} else if (strstr(name, "OrbitBoya")) {
			orbit = 1;
			continue;
		} else if (strstr(name, "ABFakeMeteors") || strstr(name, "ABFakeLimit")) {
			ab = 1;
			continue;
		} else {
			mem = operator_new(sizeof(cMapElement));
			e = mem ? cMapElement_ctor(mem, name, &pos, &rot) : NULL;
		}
		CHECK(e);
		e->vftable->Init(e);
		e->next = *list;
		*list = e;
	} while (DummySearch_NextEuler(buf, sizeof(buf), &pos, &rot));
	if (mental) {
		FXElementsPress_Create();
	}
	if (orbit) {
		FXElementsOrbit_Create();
	}
	if (ab) {
		FXElementsAB_Create();
	}
}

/* the first element with the model of name within maxDist of pos */
cMapElement *__thiscall Elements_Find(void *elements, const char *name, vec3_t pos, float maxDist) {
	cMapElement *e;

	for (e = *(cMapElement **)elements; e; e = e->next) {
		vec3_t p;
		float dx, dy, dz;

		if (strcmp(e->name, name) || !CGameModel_IsLoaded(&e->Mod)) {
			continue;
		}
		CGameModel_GetPos(&e->Mod, &p);
		dy = pos.y - p.y;
		dx = pos.x - p.x;
		dz = pos.z - p.z;
		if (maxDist >= (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz))) {
			return e;
		}
	}
	return NULL;
}

void __thiscall Elements_Update(void *elements, float dt) {
	cMapElement *e;

	for (e = *(cMapElement **)elements; e; e = e->next) {
		e->vftable->Update(e, dt);
	}
}

/* the state of an element from a saved game: name, position, the search radius and on or off */
void Elements_Load(cAbstractFile *f) {
	char name[128];
	vec3_t pos;
	float maxDist;
	int on;
	cMapElement *e;

	cAbstractFile_ReadString(f, name, sizeof(name));
	f->vftable->Read(f, &pos.x, 4);
	f->vftable->Read(f, &pos.y, 4);
	f->vftable->Read(f, &pos.z, 4);
	f->vftable->Read(f, &maxDist, 4);
	f->vftable->Read(f, &on, 4);
	e = Elements_Find(g_pWorld->elements, name, pos, maxDist);
	if (!e) {
		return;
	}
	CHECK(CGameModel_IsLoaded(&e->Mod));
	if (on) {
		CGameModel_Enable(&e->Mod);
	} else {
		CGameModel_Disable(&e->Mod);
	}
}
