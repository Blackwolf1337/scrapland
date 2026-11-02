/*
 * Actions/Actions.cpp of the game: an action (cActionMove) of a class with its timed events. See
 * include/scrap/actions.h.
 */
#include <stddef.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/cstring.h"

cAction *cAction_ctor(cAction *self, cActClass *parent, const char *name) {
	self->vftable = CACTIONMOVE_VFTABLE;
	cString_ctor(&self->name);
	CHECK(parent);
	CHECK(name);
	cString_Set(&self->name, name);
	self->parent = parent;
	self->anmFile = NULL;
	self->next = NULL;
	self->isLoop = 0;
	self->rotSpeed = 0.0f;
	self->angRotLimit = M_2PI_F;
	self->useDisp = 0;
	self->useFlyDisp = 1;
	self->maxSpeed = 1.0f;
	self->minSpeed = 1.0f;
	self->nodeModifier = -1;
	self->angleModifierSpeed = 5.0f;
	self->priority = 0;
	self->events = NULL;
	self->useFlyFall = 1;
	self->autoFaceSpeed = 0.0f;
	self->autoFaceOffsetH = 0.0f;
	self->autoFaceOffsetV = 0.0f;
	self->interpTime = g_DefInterpTime;
	self->anim = -1;
	self->anmTotalTime = -1.0f;
	self->anmFps = -1.0f;
	self->chanel = 0;
	self->slideAsFly = 0;
	self->anmConeView = 0;
	return self;
}

cActEvent *cActEvent_ctor(cActEvent *self, float time, cActEvent *next) {
	cString_ctor(&self->name);
	cPy_Function_ctor(&self->func);
	self->time = time;
	self->next = next;
	cString_Set(&self->name, "Noname");
	self->discardable = 1;
	return self;
}

void cAction_dtor(cAction *self) {
	self->vftable = CACTIONMOVE_VFTABLE;
	if (self->anmFile) {
		Mem_Free(self->anmFile);
	}
	if (self->anmName) {
		Mem_Free(self->anmName);
	}
	while (self->events) {
		cActEvent *ev = self->events;
		self->events = ev->next;
		cPy_Function_dtor(&ev->func);
		cString_dtor(&ev->name);
		operator_delete_sized(ev, sizeof(*ev));
	}
	cString_dtor(&self->name);
}

cAction *cAction_Delete(cAction *self, byte flags) {
	cAction_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void cAction_AddEvent(cAction *self, float time) {
	cActEvent **link = &self->events;

	while (*link && !((*link)->time >= time)) {
		link = &(*link)->next;
	}
	cActEvent *ev = operator_new(sizeof(cActEvent));
	if (ev) {
		ev = cActEvent_ctor(ev, time, *link);
	}
	*link = ev;
	self->lastEvent = ev;
}

void cActEvent_Fire(cActEvent *self, const char *entity) {
	cPy_Function_AddParamS(&self->func, entity);
	cPy_Function_AddParamS(&self->func, self->name);
	cPy_Function_AddParamF(&self->func, self->time);
	cPy_Function_Execute(&self->func, 0);
}

int cActEvent_Var(cActEvent *self, const char *name, int set, char **s, double *d, int *i) {
	if (i && !crt_stricmp(name, "Discardable")) {
		if ((byte)set) {
			self->discardable = *i != 0;
		} else {
			*i = self->discardable != 0;
		}
		return ACTVAR_INT;
	}
	if (!(byte)set) {
		if (d && !crt_stricmp(name, "Time")) {
			*d = (double)self->time;
			return ACTVAR_DOUBLE;
		}
	} else {
		if (!s) {
			return 0;
		}
		if (!crt_stricmp(name, "Func")) {
			cPy_Function_SetFunc(&self->func, *s);
			return ACTVAR_STRING;
		}
	}
	if (s && !crt_stricmp(name, "Name")) {
		if ((byte)set) {
			cString_Set(&self->name, *s);
		} else {
			*s = self->name;
		}
		return ACTVAR_STRING;
	}
	return 0;
}

void cAction_SetAnmFile(cAction *self, const char *file) {
	if (self->anmFile) {
		Con_Warnf("WARNING: '%s' action already has assigned '%s' as animation.\n '%s' can't be assigned.\n",
			self->name, self->anmFile, file);
		return;
	}
	CGameModel *m = &self->parent->model;
	size_t n = strlen(file) + 1;
	self->anmName = Mem_Alloc(n);
	Path_Split(file, self->anmName, NULL, NULL);
	self->anim = CGameModel_FindAnim(m, self->anmName);
	if (self->anim == -1) {
		self->anim = CGameModel_AddAnim(&self->parent->model, file);
	}
	CGameModel_AnimInfo(&self->parent->model, self->anim, &self->anmTotalTime, &self->anmFps);
	self->anmFile = Mem_Alloc(n);
	memcpy(self->anmFile, file, n);
}

static float Deg_ToRad(double d) {
	return (float)(d * M_PI_D / DEG_PER_PI);
}

/* the float variables in degrees, in the order the original tests them after "AnmFPS" */
typedef struct {
	const char *name;
	uint16_t off;
	byte deg;			/* the script value in degrees */
} cActionVarF;

static int ActionVar_F(cAction *self, int set, double *d, const cActionVarF *v) {
	float *f = (float *)((byte *)self + v->off);

	if ((byte)set) {
		*f = v->deg ? Deg_ToRad(*d) : (float)*d;
	} else {
		*d = v->deg ? (double)*f * DEG_PER_PI / M_PI_D : (double)*f;
	}
	return ACTVAR_DOUBLE;
}

int cAction_Var(cAction *act, const char *name, int set, char **s, double *d, int *i) {
	static const cActionVarF autoFace[] = {
		{ "AutoFaceOffsetV", offsetof(cAction, autoFaceOffsetV), 1 }, { "AutoFaceOffsetH", offsetof(cAction, autoFaceOffsetH), 1 },
		{ "AutoFaceSpeed", offsetof(cAction, autoFaceSpeed), 1 },
	};
	static const cActionVarF rot[] = {
		{ "AngRotLimit", offsetof(cAction, angRotLimit), 1 }, { "AngleModifierSpeed", offsetof(cAction, angleModifierSpeed), 0 },
	};
	static const cActionVarF speeds[] = {
		{ "MaxSpeed", offsetof(cAction, maxSpeed), 0 }, { "MinSpeed", offsetof(cAction, minSpeed), 0 },
	};
	static const struct {
		const char *name;
		uint16_t off;
	} flags[] = {
		{ "anmConeView", offsetof(cAction, anmConeView) }, { "isLoop", offsetof(cAction, isLoop) }, { "SlideAsFly", offsetof(cAction, slideAsFly) },
		{ "UseFlyFall", offsetof(cAction, useFlyFall) }, { "UseFlyDisp", offsetof(cAction, useFlyDisp) }, { "UseDisp", offsetof(cAction, useDisp) },
	};
	byte *base = (byte *)act;

	if (!crt_strnicmp(name, "Event", 5) && act->lastEvent) {
		return cActEvent_Var(act->lastEvent, name + 5, set, s, d, i);
	}
	if (!(byte)set) {
		if (!d) {
			goto ints;
		}
		if (!crt_strnicmp(name, "TimeOfFrame", 11)) {
			const char *frame = name + 11;
			float t;
			if (!CGameModel_AnimTime(&act->parent->model, act->anim, (float)crt_atoi(frame), &t)) {
				Con_Bluef("The frame %s doesn\xb4t exist at action (%s)->%s \n", frame, act->parent->name, act->name);
				return 0;
			}
			*d = (double)t;
			return ACTVAR_DOUBLE;
		}
	} else {
		if (!d) {
			goto floats;
		}
		if (!crt_stricmp(name, "AddEventAtTime")) {
			cAction_AddEvent(act, (float)*d);
			return ACTVAR_DOUBLE;
		}
	}
	if (!crt_stricmp(name, "InterpTime")) {
		if ((byte)set) {
			act->interpTime = (float)*d;
		} else {
			*d = (double)act->interpTime;
		}
		return ACTVAR_DOUBLE;
	}
floats:
	if (!(byte)set) {
		if (!d) {
			goto ints;
		}
		if (!crt_stricmp(name, "AnmTotalTime")) {
			*d = (double)act->anmTotalTime;
			return ACTVAR_DOUBLE;
		}
		if (!crt_stricmp(name, "AnmFPS")) {
			*d = (double)act->anmFps;
			return ACTVAR_DOUBLE;
		}
	}
	if (d) {
		for (size_t k = 0; k < sizeof(autoFace) / sizeof(autoFace[0]); k++) {
			if (!crt_stricmp(name, autoFace[k].name)) {
				return ActionVar_F(act, set, d, &autoFace[k]);
			}
		}
	}
ints:
	if (i) {
		if (!crt_stricmp(name, "Chanel")) {
			if ((byte)set) {
				act->chanel = *i;
			} else {
				*i = act->chanel;
			}
			return ACTVAR_INT;
		}
		if (!crt_stricmp(name, "PriorityLevel")) {
			if ((byte)set) {
				act->priority = *i;
			} else {
				*i = act->priority;
			}
			return ACTVAR_INT;
		}
	}
	if (d) {
		for (size_t k = 0; k < sizeof(rot) / sizeof(rot[0]); k++) {
			if (!crt_stricmp(name, rot[k].name)) {
				return ActionVar_F(act, set, d, &rot[k]);
			}
		}
	}
	if (i && !crt_stricmp(name, "NodeModifier")) {
		if ((byte)set) {
			act->nodeModifier = *i;
		} else {
			*i = act->nodeModifier;
		}
		return ACTVAR_INT;
	}
	if (d) {
		for (size_t k = 0; k < sizeof(speeds) / sizeof(speeds[0]); k++) {
			if (!crt_stricmp(name, speeds[k].name)) {
				return ActionVar_F(act, set, d, &speeds[k]);
			}
		}
	}
	if (s) {
		if (!crt_stricmp(name, "AnmName")) {
			if ((byte)set) {
				return 0;
			}
			*s = act->anmName;
			return ACTVAR_STRING;
		}
		if (!crt_stricmp(name, "Name")) {
			if ((byte)set) {
				return 0;
			}
			*s = act->name;
			return ACTVAR_STRING;
		}
		if (!crt_stricmp(name, "AnmFilePath")) {
			if ((byte)set) {
				cAction_SetAnmFile(act, *s);
			} else {
				*s = act->anmFile ? act->anmFile : ACT_EMPTY;
			}
			return ACTVAR_STRING;
		}
	}
	if (i) {
		for (size_t k = 0; k < sizeof(flags) / sizeof(flags[0]); k++) {
			if (crt_stricmp(name, flags[k].name)) {
				continue;
			}
			if (flags[k].off == offsetof(cAction, slideAsFly)) {
				/* SlideAsFly is an int */
				if ((byte)set) {
					act->slideAsFly = *i;
				} else {
					*i = act->slideAsFly;
				}
			} else if ((byte)set) {
				base[flags[k].off] = *i != 0;
			} else {
				*i = base[flags[k].off] != 0;
			}
			return ACTVAR_INT;
		}
	}
	if (d && !crt_stricmp(name, "RotSpeed")) {
		static const cActionVarF rs = { "RotSpeed", offsetof(cAction, rotSpeed), 1 };
		return ActionVar_F(act, set, d, &rs);
	}
	return 0;
}

/* the vtable of cActionMove */
const cActionVtbl cActionMove_vftable = {
	.Delete = cAction_Delete,
};
