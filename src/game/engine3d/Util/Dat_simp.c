/*
 * Typed values in the sections of a cDataFile, original source Util/Dat_simp.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

#define VALUE_SIZE 0x101

int Dat_SetBool(cDataFile *dat, const char *sect, const char *key, int v) {
	char buf[4];

	Bprintf(buf, sizeof(buf), "%s", v ? "SI" : "NO");
	return Dat_SetStr(dat, sect, key, buf);
}

int Dat_SetInt(cDataFile *dat, const char *sect, const char *key, int v) {
	char buf[16];

	Bprintf(buf, sizeof(buf), "%d", v);
	return Dat_SetStr(dat, sect, key, buf);
}

/* a new section at the end, a new key after the last one of the section; 0 if the value was there already */
int Dat_SetStr(cDataFile *dat, const char *sect, const char *key, const char *value) {
	char buf[0x104];

	int s = DataFile_FindSection(dat, sect, 1);
	if (!s) {
		s = DataFile_Count(dat);
		DataFile_AddSection(dat, sect, s);
		s++;
	}
	int k = DataFile_FindKey(dat, key, s + 1, buf, VALUE_SIZE);
	if (!k) {
		Bprintf(buf, VALUE_SIZE, "%s = %s", key, value);
		DataFile_AddLine(dat, buf, DataFile_SectionEnd(dat, s));
	} else if (strcmp(buf, value) != 0) {
		DataFile_SetValue(dat, value, k);
	} else {
		return 0;
	}
	return 1;
}

/* the value of key in sect, NULL if there is none */
static const char *Dat_Find(cDataFile *dat, const char *sect, const char *key, char *buf) {
	int s = DataFile_FindSection(dat, sect, 1);
	if (!s || !DataFile_FindKey(dat, key, s + 1, buf, VALUE_SIZE)) {
		return NULL;
	}
	return buf;
}

int Dat_GetBool(cDataFile *dat, const char *sect, const char *key, int *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&(tipo_si_no)l", out) == 1 ? 0 : -1;
}

int Dat_GetFloat(cDataFile *dat, const char *sect, const char *key, float *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&f", out) == 1 ? 0 : -1;
}

int Dat_GetLong(cDataFile *dat, const char *sect, const char *key, int *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&l", out) == 1 ? 0 : -1;
}

int Dat_GetText(cDataFile *dat, const char *sect, const char *key, char *dest, int size) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&.*t", size - 1, dest) == 1 ? 0 : -1;
}

