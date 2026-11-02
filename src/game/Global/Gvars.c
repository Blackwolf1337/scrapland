/*
 * Engine variables (Global/Gvars.cpp): a list of cGVar objects made by static constructors, a hash table of them
 * by name (GVars_Init), the config file Scrap.cfg and the console listing.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"

void Str_TrimBlanks(char *s);
void App_GetConfigDir(char *buf, int size);	/* the directory of Scrap.cfg with its separator */

#define GVAR_READONLY 0x1
#define GVAR_SAVE 0x2		/* set from the config file or changed: written by Config_Save */

cGVar *g_GVars;

/* the base constructor: puts the variable in front of g_GVars; the derived class sets the type */
cGVar *cGVar_ctor(cGVar *self, const char *name, byte flags, const char *description) {
	self->base.next = g_GVars;
	self->base.name = (char *)name;
	self->base.flags = flags;
	self->base.description = (char *)description;
	g_GVars = self;
	self->base.type = 0;
	return self;
}

cGVar *GVar_InitInt(cGVar *v, const char *name, byte flags, const char *description, int value) {
	cGVar_ctor(v, name, flags, description);
	v->value.i = value;
	v->def.i = value;
	v->base.type = GVAR_INT;
	return v;
}

/* the other variable classes of the static initializers (src/game/Global/StaticInit.c), in their order of writes */
cGVar *GVar_InitFloat(cGVar *v, const char *name, byte flags, const char *description, float value) {
	cGVar_ctor(v, name, flags, description);
	v->value.f = value;
	v->def.f = value;
	v->base.type = GVAR_FLOAT;
	return v;
}

cGVar *GVar_InitString(cGVar *v, const char *name, byte flags, const char *description, const char *value) {
	cGVar_ctor(v, name, flags, description);
	cString_ctor(&v->value.s);
	cString_ctor(&v->def.s);
	cString_Set(&v->value.s, value);
	cString_Set(&v->def.s, value);
	v->base.type = GVAR_STRING;
	return v;
}

/* the default first */
void GVar_FiniString(cGVar *v) {
	cString_dtor(&v->def.s);
	cString_dtor(&v->value.s);
}

cGVar *GVar_InitFunction(cGVar *v, const char *name, byte flags, const char *description, const char *func) {
	cGVar_ctor(v, name, flags, description);
	cPy_Function_ctor(GVar_Function(v));
	cPy_Function_SetFunc(GVar_Function(v), func);
	v->base.type = GVAR_SCRIPT_FUNCTION;
	return v;
}

/* sets or clears a flag bit; a read only variable takes only GVAR_READONLY */
byte cGVar_SetFlag(cGVar *self, byte bit, byte on) {
	if (bit != GVAR_READONLY && (self->base.flags & GVAR_READONLY)) {
		return 0;
	}
	if (on) {
		self->base.flags |= bit;
	} else {
		self->base.flags &= ~bit;
	}
	return 1;
}

HashTable *g_GVarTable;

void GVars_Init(void) {
	CHECK(g_GVarTable == NULL);
	HashTable *t = operator_new(sizeof(*t));
	if (t) {
		t->numSlots = 0x1c9;
		t->slots = operator_new_array(0x1c9 * sizeof(*t->slots));
		memset(t->slots, 0, 0x1c9 * sizeof(*t->slots));
		t->locked = 0;
		t->caseSensitive = 0;
	}
	g_GVarTable = t;
	for (cGVar *v = g_GVars; v; v = v->base.next) {
		CHECK(!g_GVarTable->locked);
		uint32_t slot = HashTable_Slot(g_GVarTable, v->base.name);
		if (!PtrTable_Find(g_GVarTable, v->base.name, slot)) {
			PtrTable_Insert(g_GVarTable, v->base.name, slot, (void *const *)&v);
		} else {
			Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", v->base.name);
			Con_FatalErrorf("InitGVars: variable already defined: %s", v->base.name);
		}
	}
}

void GVars_Done(void) {
	HashTable *t = g_GVarTable;
	if (!t) {
		CHECK(g_GVarTable != NULL);
		if (!g_GVarTable) {
			g_GVarTable = NULL;
			return;
		}
		t = g_GVarTable;
	}
	PtrTable_Clear(t);
	operator_delete_sized(t, sizeof(*t));
	g_GVarTable = NULL;
}

/* by the hash table, or along the list before GVars_Init */
cGVar *GVar_Find(const char *name) {
	if (g_GVarTable) {
		return PtrTable_Value(g_GVarTable, name);
	}
	g_GVarTable = NULL;
	for (cGVar *v = g_GVars; v; v = v->base.next) {
		if (!crt_stricmp(v->base.name, name)) {
			return v;
		}
	}
	return NULL;
}

/* the name of the only variable starting with prefix (console completion), else NULL */
const char *GVar_Complete(const char *prefix) {
	cGVar *found = NULL;
	int n = 0;

	for (cGVar *v = g_GVars; v; v = v->base.next) {
		if (!crt_strnicmp(v->base.name, prefix, strlen(prefix))) {
			n++;
			found = v;
		}
	}
	return n == 1 ? found->base.name : NULL;
}

/* "name = value # comment": cuts the comment, splits at '=' and trims both; 0 without '=' */
byte Cfg_ParseLine(char *line, char **value) {
	char *p = strstr(line, "#");
	*value = p;
	if (p) {
		*p = 0;
	}
	p = strstr(line, "=");
	*value = p;
	if (!p) {
		return 0;
	}
	*p = 0;
	*value = p + 1;
	Str_TrimBlanks(*value);
	Str_TrimBlanks(line);
	return 1;
}

void Config_Load(const char *name) {
	char path[0x400];
	char line[0x200];

	App_GetConfigDir(path, 0x200);
	size_t len = strlen(path);
	Str_Copy(path + len, name, (int)(sizeof(path) - len));
	Con_Printf("Opening confing '%s'\n", path);
	void *f = crt_fopen(path, "rt");
	if (!f) {
		Con_Warnf("Warning : '%s' not found!\n", "Scrap.cfg");
		return;
	}
	while (crt_fgets(line, sizeof(line), f)) {
		char *p = strstr(line, "#");
		if (p) {
			*p = 0;
		}
		p = strstr(line, "=");
		if (!p) {
			continue;
		}
		*p = 0;
		char *value = p + 1;
		Str_TrimBlanks(value);
		Str_TrimBlanks(line);
		cGVar *v = GVar_Find(line);
		if (!v) {
			Con_Warnf("Warning : '%s' variable <%s> not found!\n", "Scrap.cfg", line);
			continue;
		}
		switch (v->base.type) {
		case GVAR_STRING:
			if (!(v->base.flags & GVAR_READONLY)) {
				v->base.flags |= GVAR_SAVE;
			}
			if (!(v->base.flags & GVAR_READONLY)) {
				cString_Set(&v->value.s, value);
			}
			break;
		case GVAR_INT: {
			if (!(v->base.flags & GVAR_READONLY)) {
				v->base.flags |= GVAR_SAVE;
			}
			int i = crt_atoi(value);
			if (!(v->base.flags & GVAR_READONLY)) {
				v->value.i = i;
			}
			break;
		}
		case GVAR_FLOAT: {
			if (!(v->base.flags & GVAR_READONLY)) {
				v->base.flags |= GVAR_SAVE;
			}
			float x = (float)crt_atof(value);
			if (!(v->base.flags & GVAR_READONLY)) {
				v->value.f = x;
			}
			break;
		}
		case GVAR_FUNCTION:
			GVar_ListVtbl(v)->Load(GVar_ListObj(v), f);
			break;
		default:
			CHECK(0);
			break;
		}
	}
	CHECK(crt_feof(f));
	crt_fclose(f);
}

void Config_Save(void) {
	char path[0x200];

	App_GetConfigDir(path, sizeof(path));
	size_t len = strlen(path);
	Str_Copy(path + len, "Scrap.cfg", (int)(sizeof(path) - len));
	void *f = crt_fopen(path, "wt");
	if (!f) {
		Con_Warnf("Warning : '%s' can not be created!\n", path);
		return;
	}
	crt_fputs("## Begin\n\n", f);
	for (cGVar *v = g_GVars; v; v = v->base.next) {
		if (!(v->base.flags & GVAR_SAVE)) {
			continue;
		}
		switch (v->base.type) {
		case GVAR_STRING:
			crt_fprintf(f, "%-30s = %-32s    ### %s\n", v->base.name, v->value.s, v->base.description);
			break;
		case GVAR_INT:
			crt_fprintf(f, "%-30s = %-32i    ### %s\n", v->base.name, v->value.i, v->base.description);
			break;
		case GVAR_FLOAT:
			crt_fprintf(f, "%-30s = %-32f    ### %s\n", v->base.name, (double)v->value.f, v->base.description);
			break;
		case GVAR_FUNCTION:
			crt_fprintf(f, "\n\n%-30s = <begin> # %s\n", v->base.name, v->base.description);
			GVar_ListVtbl(v)->Save(GVar_ListObj(v), f);
			break;
		default:
			CHECK(0);
			break;
		}
	}
	crt_fputs("\n## End", f);
	crt_fclose(f);
}

/* lists the variables starting with filter (all for NULL) in the console */
void GVars_Show(const char *filter) {
	int n = 0;

	Con_Print(0, "\n", 0);
	for (cGVar *v = g_GVars; v; v = v->base.next) {
		if (filter && crt_strnicmp(v->base.name, filter, strlen(filter))) {
			continue;
		}
		switch (v->base.type) {
		case GVAR_STRING:
			Con_Printf("%-30s = %-32s    ### %s\n", v->base.name, v->value.s, v->base.description);
			break;
		case GVAR_INT:
			Con_Printf("%-30s = %-32i    ### %s\n", v->base.name, v->value.i, v->base.description);
			break;
		case GVAR_FLOAT:
			Con_Printf("%-30s = %-32f    ### %s\n", v->base.name, (double)v->value.f, v->base.description);
			break;
		case GVAR_FUNCTION:
			Con_Printf("\n\n%-30s = <List of defines>                   ### %s\n", v->base.name, v->base.description);
			break;
		case GVAR_SCRIPT_FUNCTION:
			Con_Printf("\n\n%-30s = <Callback function>                 ### %s\n", v->base.name, v->base.description);
			break;
		default:
			Con_Print(0, "??????????????????????????????????\n", 0);
			break;
		}
		n++;
	}
	if (filter) {
		Con_Printf(" %d vars with \"%s\"\n", n, filter);
	}
}
