/*
 * The texts of the game (Global/Language.cpp): Language/<lang>.txt holds "id<TAB>text" lines. The whole file
 * stays in g_LangBuffer and the table points into it; every entry also keeps the id of the line after it, so the
 * entries can be walked in file order.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/scrap.h"

typedef struct cLangText {
	char *text;			/* 0x0 */
	char *nextId;			/* 0x4 the id of the next line, NULL after the last */
} cLangText;

typedef struct cLangItem {
	cLangText value;		/* 0x0 */
	const char *key;		/* 0x8 */
	struct cLangItem *next;		/* 0xc */
} cLangItem;
SD_SIZE(cLangItem, 0x10);

cGVar GVar_SelectedLanguage;

#define g_LangName (((cString *)&GVar_SelectedLanguage.value))		/* the language asked for */
#define g_LangDefault (GVar_SelectedLanguage.def.s)	/* the language when that file is missing */

cLangItem *LangTable_Find(HashTable *self, const char *key, uint32_t slot) {
	for (cLangItem *it = (cLangItem *)self->slots[slot]; it; it = it->next) {
		if (HashTable_KeyEq(self, it->key, key)) {
			return it;
		}
	}
	return NULL;
}

cLangItem *LangTable_Insert(HashTable *self, const char *key, uint32_t slot, const cLangText *value) {
	CHECK(!self->locked);
	cLangItem *it = HashItem_Alloc(sizeof(*it));
	if (it) {
		it->value = *value;
		it->next = NULL;
		it->key = NULL;
	}
	it->next = (cLangItem *)self->slots[slot];
	it->key = key;
	self->slots[slot] = (HashNode *)it;
	return it;
}

/* takes the item of key out of its slot, NULL if there is none */
cLangItem *LangTable_Unlink(HashTable *self, const char *key, uint32_t slot) {
	cLangItem **link = (cLangItem **)&self->slots[slot];
	CHECK(!self->locked);
	for (; *link; link = &(*link)->next) {
		if (HashTable_KeyEq(self, (*link)->key, key)) {
			cLangItem *it = *link;
			*link = it->next;
			it->next = NULL;
			return it;
		}
	}
	return NULL;
}

byte LangTable_Delete(HashTable *self, const char *key) {
	CHECK(!self->locked);
	cLangItem *it = LangTable_Unlink(self, key, HashTable_Slot(self, key));
	if (!it) {
		return 0;
	}
	HashItem_Free(it);
	return 1;
}

void LangTable_Clear(HashTable *self) {
	CHECK(!self->locked);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < (int)self->numSlots; i++) {
		while (self->slots[i]) {
			cLangItem *it = (cLangItem *)self->slots[i];
			self->slots[i] = (HashNode *)it->next;
			HashItem_Free(it);
		}
	}
	operator_delete_array(self->slots);
	self->numSlots = 0;
	self->slots = NULL;
}

char *g_LangBuffer;

char *g_IdLang;

HashTable g_LangTable = { .numSlots = 457 };

void Lang_Free(void) {
	HashTable *t = (&g_LangTable);

	for (;;) {
		int i = 0;
		if ((int)t->numSlots <= 0) {
			break;
		}
		while (!t->slots[i]) {
			if (++i >= (int)t->numSlots) {
				goto done;
			}
		}
		cLangItem *it = (cLangItem *)t->slots[i];
		if (!it) {
			break;
		}
		CHECK(LangTable_Delete(t, it->key));
	}
done:
	if (g_LangBuffer) {
		Mem_Free(g_LangBuffer);
		g_LangBuffer = NULL;
	}
	if (g_IdLang) {
		Mem_Free(g_IdLang);
		g_IdLang = NULL;
	}
}

const char *Lang_GetStr(const char *id) {
	cLangItem *it = LangTable_Find((&g_LangTable), id, HashTable_Slot((&g_LangTable), id));
	return it ? it->value.text : NULL;
}

const char *Lang_GetLanguage(void) {
	return g_IdLang;
}

/*
 * GetLangEntries(id, n): the ids from id on in file order while they keep the prefix of id without its last 4
 * characters; of every group of n lines the first id goes into the list, cut to its length - 3 (the group
 * number). At most 10.
 */
PyObject *Lang_GetEntries(const char *name, int n) {
	char prefix[0x100];
	char key[0x100];
	PyObject *items[10];
	int len = (int)strlen(name);
	int count = 0, j = 0;

	CHECK(len < 255);
	int pre = len - 4;
	Str_Copy(prefix, name, sizeof(prefix));
	Str_Copy(key, prefix, sizeof(key));
	if ((uint32_t)pre >= 0xff) {
		crt_report_rangecheckfailure();
	}
	prefix[pre] = 0;
	for (;;) {
		cLangItem *it = LangTable_Find((&g_LangTable), key, HashTable_Slot((&g_LangTable), key));
		if (!it) {
			break;
		}
		const char *next = it->value.nextId;
		if (++count == 1) {
			if ((uint32_t)(len - 3) >= 0xff) {
				crt_report_rangecheckfailure();
			}
			key[len - 3] = 0;
			items[j++] = PyString_FromString(key);
			CHECK(j < 10);
		} else if (count == n) {
			count = 0;
			len = (int)strlen(next);
		}
		if (crt_strncmp(next, prefix, pre)) {
			break;
		}
		Str_Copy(key, next, sizeof(key));
	}
	PyObject *list = PyList_New(j);
	CHECK(list);
	for (int i = 0; i < j; i++) {
		PyList_SetItem(list, i, items[i]);
	}
	return list;
}

/* parses the file into the table; a line goes up to CR, or to LF when nothing follows the CR */
void Lang_Load(const char *lang, cAbstractFile *f) {
	cLangText value;

	Lang_Free();
	CHECK(g_LangBuffer == NULL);
	CHECK(g_IdLang == NULL);
	for (int i = 0; i < (int)g_LangTable.numSlots; i++) {
		if (g_LangTable.slots[i]) {
			CHECK(g_LangTable.slots[i] == NULL);
			break;
		}
	}
	size_t n = strlen(lang) + 1;
	g_IdLang = Mem_Alloc(n);
	memcpy(g_IdLang, lang, n);
	int size = f->vftable->Size(f);
	g_LangBuffer = Mem_Alloc(size + 1);
	f->vftable->Read(f, g_LangBuffer, size);
	g_LangBuffer[size] = 0;
	char *id = g_LangBuffer;
	for (char *tab = strchr(id, '\t'); tab; tab = strchr(id, '\t')) {
		char *cr = strchr(id, '\r');
		char *lf = strchr(id, '\n');
		*tab = 0;
		char *text = tab + 1;
		if (cr) {
			*cr++ = 0;
		}
		if (lf) {
			*lf++ = 0;
		}
		int valid = *id && *text;
		char *next;
		if (cr && *cr) {
			next = cr;
		} else if (lf && *lf) {
			next = lf;
		} else {
			next = lf < cr ? cr : lf;
		}
		value.text = text;
		value.nextId = next;
		if (valid) {
			CHECK(!g_LangTable.locked);
			uint32_t slot = HashTable_Slot((&g_LangTable), id);
			if (!LangTable_Find((&g_LangTable), id, slot)) {
				LangTable_Insert((&g_LangTable), id, slot, &value);
			} else {
				Con_Bluef("ERROR in language (%s):\n\tCan't be included '%s':'%s' \n", g_IdLang, id, text);
			}
		}
		if (!next) {
			break;
		}
		id = next;
	}
}

/*
 * loads Language/<lang>.txt (lang NULL: the one asked for before), falling back to the default language and to
 * Strings.txt; 0 if that language is already loaded and force is 0
 */
byte Lang_Set(const char *lang, byte force) {
	char path[0x200];

	if (!force && g_IdLang && !strcmp(g_IdLang, lang)) {
		return 0;
	}
	const char *name = *g_LangName;
	if (lang) {
		cString_Set(g_LangName, lang);
		name = lang;
	}
	cFile f = { { CFILE_VFTABLE, NULL, 0 }, NULL };
	Str_PrintfQuiet(path, sizeof(path), "Language/%s.txt", name);
	if (!cFile_Exists(&f.base, path)) {
		name = g_LangDefault;
		Str_PrintfQuiet(path, sizeof(path), "Language/%s.txt", name);
	}
	if (!cFile_Exists(&f.base, path)) {
		name = "Strings";
		Str_PrintfQuiet(path, sizeof(path), "Language/%s.txt", name);
	}
	if (!cFile_Exists(&f.base, path)) {
		Con_FatalErrorf("Language file not found: %s\n\nReinstalling Scrapland should solve this problem.\n",
			*g_LangName);
	}
	cFile_Open(&f.base, path, FILE_MODE_READ);
	Lang_Load(name, &f.base);
	cFile_Close(&f.base);
	cFile_dtor(&f);
	return 1;
}
