/*
 * Save variables and save games (Global/SaveVars.cpp). A save variable is a name and a string value; the save
 * game file is "Scrapland savegame", a description and all variables (strings with an int length). Saves are
 * Save0.sav (the autosave) to Save6.sav in the config directory.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/scrap.h"
#include "scrap/staticinit.h"

void App_GetConfigDir(char *buf, int size);

/* an entry of the save list of SaveGames_GetList */
typedef struct cSaveGameInfo {
	char desc[0x40];
	int32_t time;			/* modification time of the file */
	int slot;
} cSaveGameInfo;

#define SAVE_MAGIC "Scrapland savegame"
#define SAVE_SLOTS 7

static cGVar GVar_LastSaveGameName;
#define g_SaveDesc (((cString *)&GVar_LastSaveGameName.value))	/* the description of the save loaded last */
static cGVarFunction GVar_OnCrazyDealTarget;

/* called when the number of finished crazy deals changes: one deal, and all three (with g_SuperDealAvaliable) */
#define g_OnCrazyDeal (((cPy_Function *)&GVar_OnCrazyDealTarget.func))
static cGVarFunction GVar_OnCrazyDealFinished;
#define g_OnAllCrazyDeals (((cPy_Function *)&GVar_OnCrazyDealFinished.func))

static cSaveVar *g_SaveVars;
static int g_SaveVarCount;

static cSaveVar *cSaveVar_ctor(cSaveVar *self, const char *name, const char *value) {
	cString_ctor(&self->name);
	cString_ctor(&self->value);
	self->next = g_SaveVars;
	g_SaveVars = self;
	cString_Set(&self->name, name);
	cString_Set(&self->value, value);
	g_SaveVarCount++;
	return self;
}

static HashTable g_SaveVarTable = { .numSlots = 457 };

static cSaveVar *SaveVar_Find(const char *name) {
	return PtrTable_Value((&g_SaveVarTable), name);
}

/* the value of a variable as int, 0 if it is missing or has no value */
static int SaveVar_Int(const char *name) {
	cSaveVar *v = SaveVar_Find(name);
	return v && v->value ? crt_atoi(v->value) : 0;
}

/* a new variable into list and table */
static cSaveVar *SaveVar_New(const char *name, const char *value) {
	cSaveVar *v = operator_new(sizeof(*v));
	if (v) {
		v = cSaveVar_ctor(v, name, value);
	}
	const char *key = v->name;
	CHECK(!g_SaveVarTable.locked);
	uint32_t slot = HashTable_Slot((&g_SaveVarTable), key);
	if (!PtrTable_Find((&g_SaveVarTable), key, slot)) {
		PtrTable_Insert((&g_SaveVarTable), key, slot, (void *const *)&v);
	}
	return v;
}

/* 0 if the variable exists already */
byte SaveVar_Create(const char *name, const char *value) {
	if (SaveVar_Find(name)) {
		return 0;
	}
	SaveVar_New(name, value);
	return 1;
}

const char *SaveVar_Get(const char *name) {
	cSaveVar *v = SaveVar_Find(name);
	return v ? v->value : NULL;
}

byte SaveVar_Set(const char *name, const char *value) {
	cSaveVar *v = SaveVar_Find(name);
	if (v) {
		cString_Set(&v->value, value);
		return 1;
	}
	return SaveVar_Create(name, value);
}

/* deletes the variables whose name starts with prefix ("" all) */
void SaveVars_Delete(const char *prefix) {
	size_t len = strlen(prefix);
	cSaveVar **link = &g_SaveVars;

	while (*link) {
		if (!crt_strncmp(prefix, (*link)->name, len)) {
			cSaveVar *v = *link;
			*link = v->next;
			PtrTable_Delete((&g_SaveVarTable), v->name);
			g_SaveVarCount--;
			cString_dtor(&v->value);
			cString_dtor(&v->name);
			operator_delete(v);
		} else {
			link = &(*link)->next;
		}
	}
}

/*
 * the crazy deals done: with JackInTheBox.Active "1", deal N counts when CrazyDeal.N.Var names a variable whose
 * value reaches CrazyDeal.N.Tgt
 */
static int CrazyDeals_Count(void) {
	static const char *const deals[3][2] = {
		{ "CrazyDeal.1.Var", "CrazyDeal.1.Tgt" },
		{ "CrazyDeal.2.Var", "CrazyDeal.2.Tgt" },
		{ "CrazyDeal.3.Var", "CrazyDeal.3.Tgt" },
	};
	int n = 0;

	cSaveVar *active = SaveVar_Find("JackInTheBox.Active");
	if (!active || !active->value || strcmp(active->value, "1")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		cSaveVar *var = SaveVar_Find(deals[i][0]);
		if (!var || !var->value || !strcmp(var->value, "")) {
			continue;
		}
		cSaveVar *v = SaveVar_Find(var->value);
		if (!v || !v->value || !strcmp(v->value, "")) {
			continue;
		}
		int value = crt_atoi(v->value);
		cSaveVar *tgt = SaveVar_Find(deals[i][1]);
		if (tgt && tgt->value && strcmp(tgt->value, "") && crt_atoi(tgt->value) <= value) {
			n++;
		}
	}
	return n;
}

/*
 * adds n to a variable (made with "0" if missing) and returns the new value. A change of the crazy deals done
 * calls their callbacks; a new blueprint makes the Mission item of the HUD blink (Status2).
 */
int SaveVar_Inc(const char *name, int n) {
	int deals = CrazyDeals_Count();
	int blueprints = SaveVar_Int("NewBluePrintAvaliable");

	cSaveVar *v = SaveVar_Find(name);
	if (!v) {
		v = SaveVar_New(name, "0");
	}
	int value = crt_atoi(v->value) + n;
	cString_Printf(&v->value, "%d", value);
	int now = CrazyDeals_Count();
	if (deals != now) {
		if (now > 0 && cPy_Function_IsValid(g_OnCrazyDeal)) {
			cPy_Function_Execute(g_OnCrazyDeal, 0);
		}
		if (now > 2 && cPy_Function_IsValid(g_OnAllCrazyDeals)) {
			g_SuperDealAvaliable = (g_SuperDealAvaliable & ~0xff) | 1;	/* the original stores the low byte */
			cPy_Function_Execute(g_OnAllCrazyDeals, 1);
		}
	}
	if (blueprints < SaveVar_Int("NewBluePrintAvaliable")) {
		cScorer *sc = Scorer_Get(0);
		cScorerItem *item = sc ? cScorer_FindItem(sc, "Mission") : NULL;
		if (item) {
			int on = 1;
			double blink = 15.0, solid = 5.0;
			ScorerItem_Var(item, "Status2", 1, NULL, NULL, &on);
			ScorerItem_Var(item, "BlinkTime", 1, NULL, &blink, NULL);
			ScorerItem_Var(item, "SolidTime", 1, NULL, &solid, NULL);
			item->visible = 1;
		}
	}
	return value;
}

/* the description of a save game into desc (64 bytes, like strncpy) */
static byte SaveGame_ReadDesc(const char *path, char *desc) {
	char buf[0x200];
	byte ok;
	cFile f = { { CFILE_VFTABLE, NULL, 0 }, NULL };

	cFile_Open(&f.base, path, FILE_MODE_READ | FILE_MODE_ABSOLUTE);
	cAbstractFile_ReadString(&f.base, buf, sizeof(buf));
	if (!strcmp(SAVE_MAGIC, buf)) {
		cAbstractFile_ReadString(&f.base, buf, sizeof(buf));
		/* strncpy: not terminated at 64 characters */
		size_t n = strlen(buf);
		memset(desc, 0, 0x40);
		memcpy(desc, buf, n < 0x40 ? n : 0x40);
		cFile_Close(&f.base);
		ok = 1;
	} else {
		Con_Print(0xff, "Invalid Savegame file.\n", 0);
		ok = 0;
	}
	cFile_dtor(&f);
	return ok;
}

/* qsort order of the saves: by time, as a "less than" like the original (not a real compare) */
static int SaveGame_CmpTime(const void *a, const void *b) {
	return ((const cSaveGameInfo *)a)->time < ((const cSaveGameInfo *)b)->time;
}

/* [(desc, slot), ...] of the saves; the autosave first as "AutoSave-<desc>", slot 0. path is not used. */
PyObject *SaveGames_GetList(const char *path) {
	char base[0x200];
	char file[0x200];
	char desc[0x40];
	byte st[0x24];		/* struct _stat32, st_mtime at 0x1c */
	cSaveGameInfo saves[SAVE_SLOTS];
	int n = 0, autosave = 0;

	(void)path;
	App_GetConfigDir(base, sizeof(base));
	Str_Copy(base + strlen(base), "Save", (int)(sizeof(base) - strlen(base)));
	Str_PrintfQuiet(file, sizeof(file), "%s0.sav", base);
	if (!crt_stat32(file, st)) {
		memcpy(&saves[0].time, st + 0x1c, 4);
		if (SaveGame_ReadDesc(file, desc)) {
			Str_Copy(saves[0].desc, "AutoSave-", sizeof(saves[0].desc));
			autosave = 1;
			size_t l = strlen(saves[0].desc);
			Str_Copy(saves[0].desc + l, desc, (int)(sizeof(saves[0].desc) - l));
			n = 1;
			saves[0].slot = 0;
		}
	}
	cSaveGameInfo *s = &saves[n];
	for (int slot = 1; slot < SAVE_SLOTS; slot++) {
		Str_PrintfQuiet(file, sizeof(file), "%s%d.sav", base, slot);
		if (!crt_stat32(file, st)) {
			memcpy(&s->time, st + 0x1c, 4);
			if (SaveGame_ReadDesc(file, s->desc)) {
				s->slot = slot;
				n++;
				s++;
			}
		}
	}
	if (autosave) {
		crt_qsort(&saves[1], n - 1, sizeof(saves[0]), SaveGame_CmpTime);
	} else {
		crt_qsort(saves, n, sizeof(saves[0]), SaveGame_CmpTime);
	}
	PyObject *list = PyList_New(n);
	CHECK(list);
	for (int i = 0; i < n; i++) {
		PyObject *t = PyTuple_New(2);
		CHECK(t);
		CHECK(PyTuple_SetItem(t, 0, PyString_FromString(saves[i].desc)) == 0);
		CHECK(PyTuple_SetItem(t, 1, PyInt_FromLong(saves[i].slot)) == 0);
		CHECK(PyList_SetItem(list, i, t) == 0);
	}
	return list;
}

/* the variables of a save game file, after deleting all; 1 if the count matches */
static int SaveGame_ReadVars(cAbstractFile *f) {
	char name[0x200];
	char value[0x1000];
	int count;

	SaveVars_Delete("");
	cAbstractFile_ReadString(f, name, sizeof(name));
	if (strcmp(SAVE_MAGIC, name)) {
		Con_Print(0xff, "Invalid Savegame file.\n", 0);
		return 0;
	}
	cAbstractFile_ReadString(f, name, sizeof(name));
	cString_Set(g_SaveDesc, name);
	VFile_Read(f, &count, 4);
	Con_Debugf("Loading %d values:\n", count);
	for (int i = 0; i < count; i++) {
		cAbstractFile_ReadString(f, name, sizeof(name));
		cAbstractFile_ReadString(f, value, sizeof(value));
		if (!SaveVar_Create(name, value)) {
			return 0;
		}
		Con_Debugf("'%s' = '%s'\n", name, value);
	}
	return count == g_SaveVarCount;
}

/* desc NULL: the time of now (asctime without its newline) */
static byte SaveGame_WriteVars(cAbstractFile *f, const char *desc) {
	cSaveVar *v = g_SaveVars;

	cAbstractFile_WriteString(f, SAVE_MAGIC);
	if (!desc) {
		int32_t t;
		crt_time32(&t);
		char *s = crt_asctime(crt_gmtime32(&t));
		s[strlen(s) - 1] = 0;
		cAbstractFile_WriteString(f, s);
	} else {
		cAbstractFile_WriteString(f, desc);
	}
	int count = g_SaveVarCount;
	VFile_Write(f, &count, 4);
	Con_Debugf("Saving %d values:\n", g_SaveVarCount);
	for (; v; v = v->next) {
		cAbstractFile_WriteString(f, v->name);
		cAbstractFile_WriteString(f, v->value);
		Con_Debugf("'%s' = '%s'\n", v->name, v->value);
	}
	return 1;
}

/* name is "Save/<file>": the file in the config directory */
static void SaveGame_Path(char *path, int size, const char *name) {
	App_GetConfigDir(path, size);
	size_t l = strlen(path);
	Str_Copy(path + l, name + 5, (int)(size - l));
}

byte SaveGame_Load(const char *name) {
	char path[0x200];
	cFile f = { { CFILE_VFTABLE, NULL, 0 }, NULL };

	SaveGame_Path(path, sizeof(path), name);
	cFile_Open(&f.base, path, FILE_MODE_READ | FILE_MODE_ABSOLUTE);
	SaveGame_ReadVars(&f.base);
	cFile_Close(&f.base);
	cFile_dtor(&f);
	return 1;
}

byte SaveGame_Save(const char *name, const char *desc) {
	char path[0x200];
	cFile f = { { CFILE_VFTABLE, NULL, 0 }, NULL };

	SaveGame_Path(path, sizeof(path), name);
	cFile_Open(&f.base, path, FILE_MODE_WRITE | FILE_MODE_CREATE | FILE_MODE_ABSOLUTE);
	SaveGame_WriteVars(&f.base, desc);
	cFile_Close(&f.base);
	cFile_dtor(&f);
	return 1;
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_LastSaveGameName, &GVar_LastSaveGameName)
INIT_GVAR_STRING(Init_GVar_LastSaveGameName, Fini_GVar_LastSaveGameName, &GVar_LastSaveGameName, "LastSaveGameName", 0, "Last SaveGame Name", "")
FINI_CALL(Fini_SaveVarTable, &g_SaveVarTable, PtrTable_Clear)
INIT_HASHTABLE(Init_SaveVarTable, Fini_SaveVarTable, &g_SaveVarTable, 0x1c9, 4, 0x724, 0, 1)
FINI_CALL(Fini_GVar_OnCrazyDealFinished, &GVar_OnCrazyDealFinished.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnCrazyDealFinished, Fini_GVar_OnCrazyDealFinished, &GVar_OnCrazyDealFinished, "OnCrazyDealFinished", 0, "Function to be called when a taunt is to be launched", "")
FINI_CALL(Fini_GVar_OnCrazyDealTarget, &GVar_OnCrazyDealTarget.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnCrazyDealTarget, Fini_GVar_OnCrazyDealTarget, &GVar_OnCrazyDealTarget, "OnCrazyDealTarget", 0, "Function to be called when a crazy deal target is reached", "")
