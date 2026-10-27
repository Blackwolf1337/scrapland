/*
 * Actions/ActClass.cpp of the game: the variables, sounds, model and camera of an action class, with the instance of
 * Global/DList.h for its sound names. See include/scrap/actions.h.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/cstring.h"
#include "scrap/hashtable.h"
#include "scrap/sound.h"
#include "scrap/walkcharentity.h"

/* ---- DList<cString> ---- */

/* to the free list while it has room, else freed */
static void StrNode_Release(cStringList *list, cStringNode *node) {
	if (list->numFree < list->maxFree) {
		node->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = node;
	} else {
		cString_dtor(&node->str);
		operator_delete_sized(node, sizeof(*node));
	}
}

cStringNode *__thiscall cStringList_NewNode(cStringList *list) {
	cStringNode *node = list->freeNodes;

	if (node) {
		list->numFree--;
		list->freeNodes = node->next;
		return node;
	}
	node = operator_new(sizeof(*node));
	if (!node) {
		return NULL;
	}
	cString_ctor(&node->str);
	return node;
}

void __fastcall cStringList_RemoveHead(cStringList *list) {
	cStringNode *p = list->head;

	list->iter = NULL;
	if (!p) {
		return;
	}
	if (p != p->next) {
		list->head = p->next;
		p->prev->next = p->next;
		p->next->prev = p->prev;
		StrNode_Release(list, p);
	} else {
		StrNode_Release(list, p);
		list->head = NULL;
	}
	list->count--;
}

void __fastcall cStringList_Clear(cStringList *list) {
	while (list->head) {
		cStringList_RemoveHead(list);
	}
}

void __fastcall cStringList_FreePool(cStringList *list) {
	cStringNode *p;

	while ((p = list->freeNodes) != NULL) {
		list->freeNodes = p->next;
		cString_dtor(&p->str);
		operator_delete_sized(p, sizeof(*p));
	}
	list->numFree = 0;
}

/* the list destructor: the nodes and the free pool */
void __fastcall cStringList_dtor(cStringList *list) {
	cStringList_Clear(list);
	cStringList_FreePool(list);
}

/* ---- cActClass ---- */

#define ACTCLASS_SLOTS 0x1c9

cActNodeMod *__fastcall cActNodeMod_ctor(cActNodeMod *self) {
	memset(self->ang, 0, sizeof(self->ang));
	self->node = ACT_NODE_NONE;
	return self;
}

cActClass *__thiscall cActClass_ctor(cActClass *self, const char *name) {
	self->vftable = Game_Ptr(CACTCLASS_VFTABLE);
	cString_ctor(&self->name);
	self->actions.numSlots = ACTCLASS_SLOTS;
	self->actions.slots = New_Array(ACTCLASS_SLOTS, sizeof(HashNode *));
	memset(self->actions.slots, 0, ACTCLASS_SLOTS * sizeof(HashNode *));
	self->actions.locked = 0;
	self->actions.caseSensitive = 1;
	CGameModel_ctor(&self->model, NULL);
	memset(&self->sounds, 0, sizeof(self->sounds));
	for (int i = 0; i < ACT_NODEMODS; i++) {
		cActNodeMod_ctor(&self->nodes[i]);
	}
	self->cam.camDist = 200.0f;
	self->cam.camAng = 0.0f;
	self->cam.camRot = 0.0f;
	self->cam.fov = 1.5707964f;
	self->cam.minFov = 1.5707964f;
	self->cam.maxFov = 2.0943952f;
	/* (float)fabs(the double at rva 0x3a36f0), the range of the two above */
	self->cam.fovRange = 0.52359879f;
	self->cam.fovTime = 0.5f;
	self->cam.fovLeft = 0.0f;
	self->cam.fovStart = 0.0f;
	cString_ctor(&self->fallSound);
	cPy_Function_ctor(&self->actionPress);
	cPy_Function_ctor(&self->actionRelease);
	cPy_Function_ctor(&self->photoPress);
	cPy_Function_ctor(&self->possessionPress);
	cPy_Function_ctor(&self->onCreate);
	cPy_Function_ctor(&self->onFreeResources);
	cString_Set(&self->name, name);
	self->modelFile = NULL;
	self->defAction = NULL;
	self->first = NULL;
	self->last = NULL;
	self->turnAngLimit = 0.78539819f;
	self->maxTimeToExtraRelax = 2.0f;
	self->minTimeToExtraRelax = 5.0f;
	self->minFallDist = 30.0f;
	self->wRadius = 60.0f;
	self->vRadius = 60.0f;
	self->sRadius = 10.0f;
	self->flyAccel = 600.0f;
	self->flyFric = 3.0f;
	self->flySpeed = 1000.0f;
	self->flyFallAccel = 200.0f;
	self->minTimeWalking = 0.3f;
	self->canBePossessed = 0;
	self->discardable = 0;
	self->canFly = 0;
	self->canJump = 0;
	self->yModelDisp = 0.0f;
	self->initEndWalkInterpFactor = 2.0f;
	self->fireAsSelect = 0;
	self->skipInitOnChange = 1;
	self->dinaShip = 0;
	self->fallSoundVol = 0.0f;
	self->users = 0;
	self->soundDisp.x = 0.0f;
	self->soundDisp.y = 0.0f;
	self->soundDisp.z = 0.0f;
	self->pretransformChan = 0;
	self->strafeChar = 1;
	self->relaxAlarm = 0;
	return self;
}

void __fastcall cActClass_dtor(cActClass *self) {
	self->vftable = Game_Ptr(CACTCLASS_VFTABLE);
	if (self->users) {
		Con_Bluef("ERROR : The class %s is going to be deleted but the reference counter is %d\n", self->name,
			self->users);
	}
	if (self->discardable) {
		cActClass_FreeResources(self);
	}
	for (;;) {
		uint32_t i;
		HashNode *it = NULL;

		for (i = 0; (int)i < (int)self->actions.numSlots; i++) {
			if (self->actions.slots[i]) {
				it = self->actions.slots[i];
				break;
			}
		}
		if (!it) {
			break;
		}
		cAction *data = it->value;
		if (!PtrTable_Delete(&self->actions, it->key)) {
			CheckFailed("Actions.Delete( item->ident)", __FILE__, __TIMESTAMP__, __LINE__);
		}
		if (!data) {
			CheckFailed("data!=NULL", __FILE__, __TIMESTAMP__, __LINE__);
			continue;
		}
		data->vftable->Delete(data, 1);
	}
	if (self->modelFile) {
		Mem_Free(self->modelFile);
	}
	cPy_Function_dtor(&self->onFreeResources);
	cPy_Function_dtor(&self->onCreate);
	cPy_Function_dtor(&self->possessionPress);
	cPy_Function_dtor(&self->photoPress);
	cPy_Function_dtor(&self->actionRelease);
	cPy_Function_dtor(&self->actionPress);
	cString_dtor(&self->fallSound);
	cStringList_dtor(&self->sounds);
	CGameModel_Reset(&self->model);
	PtrTable_Clear(&self->actions);
	cString_dtor(&self->name);
}

cActClass *__thiscall cActClass_Delete(cActClass *self, byte flags) {
	cActClass_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cAction *__thiscall cActClass_CreateAction(cActClass *self, const char *name) {
	cAction *act = operator_new(sizeof(cAction));

	if (act) {
		act = cAction_ctor(act, self, name);
	}
	const char *key = act->name;
	if (self->actions.locked) {
		CheckFailed("!bLocked", __FILE__, __TIMESTAMP__, __LINE__);
	}
	uint32_t slot = HashTable_Slot(&self->actions, key);
	if (PtrTable_Find(&self->actions, key, slot)) {
		Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", key);
		CheckFailed("Actions.Add(act->GetName(),act,true)", __FILE__, __TIMESTAMP__, __LINE__);
	} else {
		PtrTable_Insert(&self->actions, key, slot, (void *const *)&act);
	}
	if (self->first) {
		self->last->next = act;
		self->last = self->last->next;
	} else {
		self->first = act;
		self->last = act;
		act->next = NULL;
	}
	return act;
}

void __thiscall cActClass_AddSoundRes(cActClass *self, const char *file) {
	char name[0x80];
	cString str;

	g_pSound->vftable->LoadSound(g_pSound, file);
	g_pSound->vftable->GetSoundName(g_pSound, file, name, sizeof(name));
	cString_ctorStr(&str, name);
	self->sounds.iter = NULL;
	cStringNode *node = cStringList_NewNode(&self->sounds);
	cString_Assign(&node->str, &str);
	DList_LinkFirst(&self->sounds, node);
	cString_dtor(&str);
}

void __fastcall cActClass_FreeResources(cActClass *self) {
	cStringList *l = &self->sounds;

	cPy_Function_Execute(&self->onFreeResources, 0);
	while ((l->iter = l->head) != NULL) {
		l->iter = l->head;
		g_pSound->vftable->DeleteSound(g_pSound, l->iter->str);
		cStringList_RemoveHead(l);
	}
	CGameModel_Release(&self->model, 1);
}

cAction *__thiscall cActClass_GetAction(cActClass *cls, const char *name) {
	return PtrTable_Value(&cls->actions, name);
}

cAction *__thiscall cActClass_SetDefAction(cActClass *self, const char *name) {
	cAction *a = PtrTable_Value(&self->actions, name);

	self->defAction = a;
	return a;
}

void __thiscall cActClass_SetModelFile(cActClass *self, const char *file) {
	if (self->modelFile) {
		Con_Warnf("WARNING: '%s' Active class already has assigned '%s' as model.\n '%s' can't be assigned.\n",
			self->name, self->modelFile, file);
		return;
	}
	CGameModel_LoadName(&self->model, file);
	CGameModel_Disable(&self->model);
	size_t n = strlen(file) + 1;
	self->modelFile = Mem_Alloc(n);
	memcpy(self->modelFile, file, n);
}

void __thiscall cActNodeMod_SetNode(int *node, CGameModel *model, const char *name) {
	if (strcmp(name, "_Move_The_Model_") == 0) {
		*node = ACT_NODE_MODEL;
		return;
	}
	*node = CGameModel_FindNodoR(model, name);
	if (*node < 0) {
		Con_Bluef("CNodeAngleModifier::SetNode : %s do not exist!!!\n", name);
	}
}

/* the degrees of the scripts to the radians of the class and back */
static float Deg_ToRad(double d) {
	return (float)(d * M_PI_D / DEG_PER_PI);
}

static double Rad_ToDeg(float r) {
	return (double)r * DEG_PER_PI / M_PI_D;
}

int __thiscall cActClassCam_Var(cActClassCam *self, const char *name, int set, char **s, double *d, int *i) {
	float v;

	(void)s;
	(void)i;
	if (!d) {
		return 0;
	}
	if (!crt_stricmp(name, "CamDist")) {
		if ((byte)set) {
			self->camDist = (float)*d;
		} else {
			*d = (double)self->camDist;
		}
		return ACTVAR_DOUBLE;
	}
	if (!crt_stricmp(name, "CamAng")) {
		if ((byte)set) {
			self->camAng = Deg_ToRad(*d);
			return ACTVAR_DOUBLE;
		}
		v = self->camAng;
	} else if (!crt_stricmp(name, "CamRot")) {
		if ((byte)set) {
			self->camRot = Deg_ToRad(*d);
			return ACTVAR_DOUBLE;
		}
		v = self->camRot;
	} else if (!crt_stricmp(name, "FOV")) {
		if ((byte)set) {
			self->fov = Deg_ToRad(*d);
			return ACTVAR_DOUBLE;
		}
		v = self->fov;
	} else if (!crt_stricmp(name, "MinFOV")) {
		if ((byte)set) {
			float f = Deg_ToRad(*d);
			self->minFov = f;
			self->fovRange = (float)__builtin_fabs((double)(self->maxFov - f));
			return ACTVAR_DOUBLE;
		}
		v = self->minFov;
	} else if (!crt_stricmp(name, "MaxFOV")) {
		if ((byte)set) {
			float f = Deg_ToRad(*d);
			self->maxFov = f;
			self->fovRange = (float)__builtin_fabs((double)(f - self->minFov));
			return ACTVAR_DOUBLE;
		}
		v = self->maxFov;
	} else if (!crt_stricmp(name, "FOVInterpTime")) {
		if ((byte)set) {
			CHECK(*d > 0.0);
			self->fovTime = (float)*d;
		} else {
			*d = (double)self->fovTime;
		}
		return ACTVAR_DOUBLE;
	} else {
		return 0;
	}
	*d = (double)(float)Rad_ToDeg(v);
	return ACTVAR_DOUBLE;
}

/* the class of the action controller of e */
static cActClass *Ent_ActClass(cEntity *e) {
	return ((cActCtrl *)WithLife_Vtbl(e)->GetActCtrl(e))->cls;
}

void __thiscall cActClassCam_Update(void *cam, cEntity *followCam, cEntity *e) {
	cActClassCam *c = cam;
	cFollowCamEntity *fc = (cFollowCamEntity *)followCam;
	float *fov = &fc->base.cam.fov;

	fc->camDist = c->camDist;
	fc->camAng = c->camAng;
	fc->camRot = c->camRot;
	if (e && strcmp(Ent_ActClass(e)->name, "Functionary") == 0) {
		cWalkCharEntity *w = WChar(e);
		float dt = cTimer_GetDelta(&w->obj.timer);
		byte act = w->control.act;

		/* the zoom goes back while the action button is up (or the energy bar does not run) */
		if ((!act || w->energyTime == 0.0f) && *fov >= c->minFov) {
			float mn = c->minFov;
			float t = c->fovLeft - dt;
			c->fovLeft = t;
			if (t <= 0.0f) {
				c->fovLeft = 0.0f;
				c->fovStart = 0.0f;
				*fov = mn;
				return;
			}
			t = t / c->fovStart;
			t = (3.0f - (t + t)) * (t * t);
			*fov = (3.0f - (t + t)) * (t * t) * c->fovRange + mn;
			return;
		}
		if (act && c->maxFov >= *fov) {
			float mx = c->maxFov;
			float t = c->fovLeft + dt;
			float total = c->fovTime;
			c->fovLeft = t;
			c->fovStart = t;
			if (t >= total) {
				c->fovLeft = total;
				c->fovStart = total;
				*fov = mx;
				return;
			}
			float p = (float)Game_Pow(1.0 - (double)(t / total), 6.0);
			*fov = (1.0f - p) * c->fovRange + c->minFov;
			return;
		}
	}
	*fov = c->fov;
}

/* the variables of the class by kind, in the order the original tests them */
typedef struct {
	const char *name;
	uint16_t off;
	byte isInt;			/* int, else a byte flag */
} cActVarI;

typedef struct {
	const char *name;
	uint16_t off;
} cActVarF;

static const cActVarI g_ClsInts[] = {
	{ "SkipInitOnChange", 0x18a, 0 }, { "DinaShip", 0x208, 0 }, { "Discardable", 0x185, 0 },
	{ "CanBePossessed", 0x184, 0 }, { "RelaxAlarm", 0x18b, 0 }, { "FireAsSelect", 0x189, 0 },
	{ "StrafeChar", 0x188, 0 }, { "PretransformChan", 0x148, 1 }, { "CanFly", 0x186, 0 }, { "CanJump", 0x187, 0 },
};

static const cActVarF g_ClsFloats1[] = {
	{ "InitEndWalkInterpFactor", 0x18c }, { "YModelDisp", 0x180 }, { "MinTimeWalking", 0x14c },
};

static const cActVarF g_ClsFloats2[] = {
	{ "FlyFallAccel", 0x17c }, { "FlySpeed", 0x178 }, { "FlyFric", 0x174 }, { "FlyAccel", 0x170 },
	{ "SRadius", 0x16c }, { "VRadius", 0x168 }, { "WRadius", 0x164 }, { "MinFallDist", 0x158 },
	{ "MaxTimeToExtraRelax", 0x150 }, { "MinTimeToExtraRelax", 0x154 },
};

static const cActVarF g_ClsFuncs[] = {
	{ "OnCreate", 0x1e0 }, { "PossessionPress", 0x1cc }, { "PhotoPress", 0x1b8 }, { "ActionRelease", 0x1a4 },
	{ "ActionPress", 0x190 }, { "OnFreeResources", 0x1f4 },
};

/* a float variable of a table: 1 if name is one of them */
static int ActVar_Float(void *obj, const cActVarF *t, size_t n, const char *name, int set, double *d) {
	for (size_t k = 0; k < n; k++) {
		if (!crt_stricmp(name, t[k].name)) {
			float *f = (float *)((byte *)obj + t[k].off);
			if ((byte)set) {
				*f = (float)*d;
			} else {
				*d = (double)*f;
			}
			return 1;
		}
	}
	return 0;
}

int __thiscall cActClass_Var(cActClass *cls, const char *name, int set, char **s, double *d, int *i) {
	byte *base = (byte *)cls;
	int r = cActClassCam_Var(&cls->cam, name, set, s, d, i);

	if (r) {
		return r;
	}
	/* "NodeN_Name", "NodeN_Mx", "NodeN_My" (N 0..2, M 0..6) */
	if ((byte)set && !crt_strnicmp(name, "Node", 4)) {
		uint32_t n = (uint32_t)((int)(signed char)name[4] - '0');
		if (n <= 2) {
			if (s && !crt_stricmp(name + 5, "Name")) {
				cActNodeMod_SetNode(&cls->nodes[n].node, &cls->model, *s);
				return ACTVAR_STRING;
			}
			if (name[5] == '_') {
				uint32_t m = (uint32_t)((int)(signed char)name[6] - '0');
				if (m <= 6 && d) {
					char c = name[7];
					if ((c == 'x' || c == 'X') && name[8] == 0) {
						cls->nodes[n].ang[m][0] = (float)*d;
						return ACTVAR_DOUBLE;
					}
					if ((c == 'y' || c == 'Y') && name[8] == 0) {
						cls->nodes[n].ang[m][1] = (float)*d;
						return ACTVAR_DOUBLE;
					}
				}
			}
		}
	}
	if (d) {
		static const cActVarF disp[] = { { "SoundDispX", 0x13c }, { "SoundDispY", 0x140 }, { "SoundDispZ", 0x144 } };
		if (ActVar_Float(cls, disp, 3, name, set, d)) {
			return ACTVAR_DOUBLE;
		}
	}
	if ((byte)set) {
		if (s) {
			if (!crt_stricmp(name, "AddSoundRes")) {
				cActClass_AddSoundRes(cls, *s);
				return ACTVAR_STRING;
			}
			if (!crt_stricmp(name, "FallSound")) {
				cString_Set(&cls->fallSound, *s);
				return ACTVAR_STRING;
			}
		}
		if (d && !crt_stricmp(name, "FallSoundVol")) {
			cls->fallSoundVol = (float)*d;
			return ACTVAR_DOUBLE;
		}
	}
	if (i) {
		for (size_t k = 0; k < sizeof(g_ClsInts) / sizeof(g_ClsInts[0]); k++) {
			if (crt_stricmp(name, g_ClsInts[k].name)) {
				continue;
			}
			if (g_ClsInts[k].isInt) {
				if ((byte)set) {
					*(int *)(base + g_ClsInts[k].off) = *i;
				} else {
					*i = *(int *)(base + g_ClsInts[k].off);
				}
			} else if ((byte)set) {
				base[g_ClsInts[k].off] = *i != 0;
			} else {
				*i = base[g_ClsInts[k].off] != 0;
			}
			return ACTVAR_INT;
		}
	}
	if (d && ActVar_Float(cls, g_ClsFloats1, sizeof(g_ClsFloats1) / sizeof(g_ClsFloats1[0]), name, set, d)) {
		return ACTVAR_DOUBLE;
	}
	if ((byte)set && s) {
		for (size_t k = 0; k < sizeof(g_ClsFuncs) / sizeof(g_ClsFuncs[0]); k++) {
			if (!crt_stricmp(name, g_ClsFuncs[k].name)) {
				cPy_Function_SetFunc((cPy_Function *)(base + g_ClsFuncs[k].off), *s);
				return ACTVAR_STRING;
			}
		}
	}
	if (d) {
		if (ActVar_Float(cls, g_ClsFloats2, sizeof(g_ClsFloats2) / sizeof(g_ClsFloats2[0]), name, set, d)) {
			return ACTVAR_DOUBLE;
		}
		if (!crt_stricmp(name, "TurnAngLimit")) {
			if ((byte)set) {
				cls->turnAngLimit = Deg_ToRad(*d);
			} else {
				*d = Rad_ToDeg(cls->turnAngLimit);
			}
			return ACTVAR_DOUBLE;
		}
	}
	if (s) {
		if (!crt_stricmp(name, "DefAction")) {
			if ((byte)set) {
				cActClass_SetDefAction(cls, *s);
			} else {
				*s = cls->defAction ? cls->defAction->name : ACT_EMPTY;
			}
			return ACTVAR_STRING;
		}
		if (!crt_stricmp(name, "ModelFilePath")) {
			if ((byte)set) {
				cActClass_SetModelFile(cls, *s);
			} else {
				*s = cls->modelFile ? cls->modelFile : ACT_EMPTY;
			}
			return ACTVAR_STRING;
		}
		if (!crt_stricmp(name, "Name") && !(byte)set) {
			*s = cls->name;
			return ACTVAR_STRING;
		}
	}
	return 0;
}
