/*
 * Typed values in the sections of a cArchDatos, original source Util/Dat_simp.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

#define VALUE_SIZE 0x101

int Dat_SetBool(cArchDatos *dat, const char *sect, const char *key, int v) {
	char buf[4];

	Bprintf(buf, sizeof(buf), "%s", v ? "SI" : "NO");
	return Dat_SetStr(dat, sect, key, buf);
}

int Dat_SetInt(cArchDatos *dat, const char *sect, const char *key, int v) {
	char buf[16];

	Bprintf(buf, sizeof(buf), "%d", v);
	return Dat_SetStr(dat, sect, key, buf);
}

/* a new section at the end, a new key after the last one of the section; 0 if the value was there already */
int Dat_SetStr(cArchDatos *dat, const char *sect, const char *key, const char *value) {
	char buf[0x104];

	int s = Arch_FindSection(dat, sect, 1);
	if (!s) {
		s = Arch_Count(dat);
		Arch_AddSection(dat, sect, s);
		s++;
	}
	int k = Arch_FindKey(dat, key, s + 1, buf, VALUE_SIZE);
	if (!k) {
		Bprintf(buf, VALUE_SIZE, "%s = %s", key, value);
		Arch_AddLine(dat, buf, Arch_SectionEnd(dat, s));
	} else if (strcmp(buf, value) != 0) {
		Arch_SetValue(dat, value, k);
	} else {
		return 0;
	}
	return 1;
}

/* the value of key in sect, NULL if there is none */
static const char *Dat_Find(cArchDatos *dat, const char *sect, const char *key, char *buf) {
	int s = Arch_FindSection(dat, sect, 1);
	if (!s || !Arch_FindKey(dat, key, s + 1, buf, VALUE_SIZE)) {
		return NULL;
	}
	return buf;
}

int Dat_GetBool(cArchDatos *dat, const char *sect, const char *key, int *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&(tipo_si_no)l", out) == 1 ? 0 : -1;
}

int Dat_GetDouble(cArchDatos *dat, const char *sect, const char *key, double *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&d", out) == 1 ? 0 : -1;
}

int Dat_GetFloat(cArchDatos *dat, const char *sect, const char *key, float *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&f", out) == 1 ? 0 : -1;
}

int Dat_GetLong(cArchDatos *dat, const char *sect, const char *key, int *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&l", out) == 1 ? 0 : -1;
}

int Dat_GetText(cArchDatos *dat, const char *sect, const char *key, char *dest, int size) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&.*t", size - 1, dest) == 1 ? 0 : -1;
}

void Dat_GetList(cArchDatos *dat, const char *sect, const char *key, cArchDatos *dest) {
	char buf[0x104];
	char item[0x104];

	CHECK(dest);
	int s = Arch_FindSection(dat, sect, 1);
	if (!s) {
		return;
	}
	for (int k = Arch_FindKey(dat, key, s + 1, buf, VALUE_SIZE); k; k = Arch_FindKey(dat, key, k + 1, buf, VALUE_SIZE)) {
		if (Proc_Scan(buf, "&.*t", 0x100, item) != 1) {
			return;
		}
		Arch_AddLine(dest, item, Arch_Count(dest));
	}
}

int Dat_GetWord(cArchDatos *dat, const char *sect, const char *key, uint16_t *out) {
	char buf[0x104];

	return Dat_Find(dat, sect, key, buf) && Proc_Scan(buf, "&w", out) == 1 ? 0 : -1;
}

int Dat_GetStr(cArchDatos *dat, const char *sect, const char *key, char *dest, int size) {
	CHECK(dest && size > 0);
	int s = Arch_FindSection(dat, sect, 1);
	if (!s) {
		return -1;
	}
	return Arch_FindKey(dat, key, s + 1, dest, size) ? 0 : -1;
}
