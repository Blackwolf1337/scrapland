/*
 * Engine variables (Global/Gvars.cpp): a list of cGVar objects made by static constructors, a hash table of them
 * by name (GVars_Init), the config file Scrap.cfg and the console listing.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/scrap.h"

int crt_strnicmp(const char *a, const char *b, size_t n);
int crt_stricmp(const char *a, const char *b);
int crt_fprintf(void *f, const char *fmt, ...);
int crt_fputs(const char *s, void *f);
int crt_fclose(void *f);
int crt_feof(void *f);
char *crt_fgets(char *buf, int size, void *f);
int crt_atoi(const char *s);
double crt_atof(const char *s);
void Str_TrimBlanks(char *s);
void App_GetConfigDir(char *buf, int size);	/* the directory of Scrap.cfg with its separator (rva 0x20d570) */

/* the HashTable.h parts shared with the other instances (src/game/FileSys/FilePak.c) */
HashTable *__thiscall HashTable_ctor(HashTable *self, int numSlots, byte caseSensitive);
uint32_t __thiscall HashTable_Slot(HashTable *self, const char *key);
void HashItem_Free(void *p);

/* the instance of this file: value a cGVar *, the key the name of the variable (not copied) */
typedef struct cGVarItem {
	cGVar *value;			/* 0x0 */
	const char *key;		/* 0x4 */
	struct cGVarItem *next;		/* 0x8 */
} cGVarItem;
SD_SIZE(cGVarItem, 0xc);

#define GVAR_READONLY 0x1
#define GVAR_SAVE 0x2		/* set from the config file or changed: written by Config_Save */

/* the same code as the cNameItem instance */
cGVarItem *__thiscall GVarTable_Find(HashTable *self, const char *key, uint32_t slot) {
	for (cGVarItem *it = (cGVarItem *)self->slots[slot]; it; it = it->next) {
		if (self->caseSensitive ? strcmp(it->key, key) == 0 : crt_stricmp(it->key, key) == 0) {
			return it;
		}
	}
	return NULL;
}

cGVarItem *__thiscall GVarTable_Insert(HashTable *self, const char *key, uint32_t slot, cGVar *const *value) {
	CHECK(!self->locked);
	if (!g_HashItemPool) {
		cMemPool *mem = operator_new(sizeof(*mem));
		g_HashItemPool = mem ? cMemPool_ctor(mem, "HashItem", g_HashItemValues) : NULL;
	}
	g_HashItemRefs++;
	cGVarItem *it = cMemPool_Alloc(g_HashItemPool, sizeof(*it));
	if (it) {
		it->value = *value;
		it->next = NULL;
		it->key = NULL;
	}
	it->next = (cGVarItem *)self->slots[slot];
	it->key = key;
	self->slots[slot] = (HashNode *)it;
	return it;
}

byte __thiscall GVarTable_Add(HashTable *self, const char *key, cGVar *const *value, byte warn) {
	CHECK(!self->locked);
	uint32_t slot = HashTable_Slot(self, key);
	if (GVarTable_Find(self, key, slot)) {
		if (warn) {
			Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", key);
		}
		return 0;
	}
	GVarTable_Insert(self, key, slot, value);
	return 1;
}

byte __thiscall GVarTable_Get(HashTable *self, const char *key, cGVar **out) {
	cGVarItem *it = GVarTable_Find(self, key, HashTable_Slot(self, key));
	if (!it) {
		return 0;
	}
	*out = it->value;
	return 1;
}

void __fastcall GVarTable_Clear(HashTable *self) {
	CHECK(!self->locked);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < (int)self->numSlots; i++) {
		while (self->slots[i]) {
			cGVarItem *it = (cGVarItem *)self->slots[i];
			self->slots[i] = (HashNode *)it->next;
			HashItem_Free(it);
		}
	}
	operator_delete_array(self->slots);
	self->numSlots = 0;
	self->slots = NULL;
}

void __fastcall GVarTable_dtor(HashTable *self) {
	GVarTable_Clear(self);
}

/* the base constructor: puts the variable in front of g_GVars; the derived class sets the type */
cGVar *__thiscall cGVar_ctor(cGVar *self, const char *name, byte flags, const char *description) {
	self->next = g_GVars;
	self->name = (char *)name;
	self->flags = flags;
	self->description = (char *)description;
	g_GVars = self;
	self->type = 0;
	return self;
}

/* sets or clears a flag bit; a read only variable takes only GVAR_READONLY */
byte __thiscall cGVar_SetFlag(cGVar *self, byte bit, byte on) {
	if (bit != GVAR_READONLY && (self->flags & GVAR_READONLY)) {
		return 0;
	}
	if (on) {
		self->flags |= bit;
	} else {
		self->flags &= ~bit;
	}
	return 1;
}

void GVars_Init(void) {
	CHECK(g_GVarTable == NULL);
	HashTable *t = operator_new(sizeof(*t));
	if (t) {
		t->numSlots = 0x1c9;
		t->slots = operator_new_array(0x1c9 * 4);
		memset(t->slots, 0, 0x1c9 * 4);
		t->locked = 0;
		t->caseSensitive = 0;
	}
	g_GVarTable = t;
	for (cGVar *v = g_GVars; v; v = v->next) {
		CHECK(!g_GVarTable->locked);
		uint32_t slot = HashTable_Slot(g_GVarTable, v->name);
		if (!GVarTable_Find(g_GVarTable, v->name, slot)) {
			GVarTable_Insert(g_GVarTable, v->name, slot, &v);
		} else {
			Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", v->name);
			Con_SysErrorf("InitGVars: variable already defined: %s", v->name);
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
	GVarTable_Clear(t);
	operator_delete_sized(t, sizeof(*t));
	g_GVarTable = NULL;
}

/* by the hash table, or along the list before GVars_Init */
cGVar *GVar_Find(const char *name) {
	if (g_GVarTable) {
		cGVarItem *it = GVarTable_Find(g_GVarTable, name, HashTable_Slot(g_GVarTable, name));
		return it ? it->value : NULL;
	}
	g_GVarTable = NULL;
	for (cGVar *v = g_GVars; v; v = v->next) {
		if (!crt_stricmp(v->name, name)) {
			return v;
		}
	}
	return NULL;
}

/* the name of the only variable starting with prefix (console completion), else NULL */
const char *GVar_Complete(const char *prefix) {
	cGVar *found = NULL;
	int n = 0;

	for (cGVar *v = g_GVars; v; v = v->next) {
		if (!crt_strnicmp(v->name, prefix, strlen(prefix))) {
			n++;
			found = v;
		}
	}
	return n == 1 ? found->name : NULL;
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
		switch (v->type) {
		case GVAR_STRING:
			if (!(v->flags & GVAR_READONLY)) {
				v->flags |= GVAR_SAVE;
			}
			if (!(v->flags & GVAR_READONLY)) {
				cString_Set(&v->value.s, value);
			}
			break;
		case GVAR_INT: {
			if (!(v->flags & GVAR_READONLY)) {
				v->flags |= GVAR_SAVE;
			}
			int i = crt_atoi(value);
			if (!(v->flags & GVAR_READONLY)) {
				v->value.i = i;
			}
			break;
		}
		case GVAR_FLOAT: {
			if (!(v->flags & GVAR_READONLY)) {
				v->flags |= GVAR_SAVE;
			}
			float x = (float)crt_atof(value);
			if (!(v->flags & GVAR_READONLY)) {
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
	for (cGVar *v = g_GVars; v; v = v->next) {
		if (!(v->flags & GVAR_SAVE)) {
			continue;
		}
		switch (v->type) {
		case GVAR_STRING:
			crt_fprintf(f, "%-30s = %-32s    ### %s\n", v->name, v->value.s, v->description);
			break;
		case GVAR_INT:
			crt_fprintf(f, "%-30s = %-32i    ### %s\n", v->name, v->value.i, v->description);
			break;
		case GVAR_FLOAT:
			crt_fprintf(f, "%-30s = %-32f    ### %s\n", v->name, (double)v->value.f, v->description);
			break;
		case GVAR_FUNCTION:
			crt_fprintf(f, "\n\n%-30s = <begin> # %s\n", v->name, v->description);
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
	for (cGVar *v = g_GVars; v; v = v->next) {
		if (filter && crt_strnicmp(v->name, filter, strlen(filter))) {
			continue;
		}
		switch (v->type) {
		case GVAR_STRING:
			Con_Printf("%-30s = %-32s    ### %s\n", v->name, v->value.s, v->description);
			break;
		case GVAR_INT:
			Con_Printf("%-30s = %-32i    ### %s\n", v->name, v->value.i, v->description);
			break;
		case GVAR_FLOAT:
			Con_Printf("%-30s = %-32f    ### %s\n", v->name, (double)v->value.f, v->description);
			break;
		case GVAR_FUNCTION:
			Con_Printf("\n\n%-30s = <List of defines>                   ### %s\n", v->name, v->description);
			break;
		case GVAR_SCRIPT_FUNCTION:
			Con_Printf("\n\n%-30s = <Callback function>                 ### %s\n", v->name, v->description);
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
