/*
 * The radiosity data of the light maps, original source Engine/radiosidad.cpp (include/scrap/bsp.h): the region
 * tree and links of a list, the area lights of the materials and the table of the sample normals.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/bsp.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/readdata.h"
#include "scrap/misc.h"

#define RAD_NAME 0x40
/* the default end of the attenuation, 1.8441e19 */
#define RAD_ATTEN_MAX 0x5f7ffd5bu

static float Rad_Float(uint32_t bits) {
	float f;

	memcpy(&f, &bits, sizeof(f));
	return f;
}

BspLink *__thiscall BspLink_ctor(BspLink *self, BspLink *next, BspRegion *region, float ff) {
	g_BspNumLinks++;
	self->next = next;
	self->region = region;
	self->ff = ff;
	return self;
}

RadAreaLight *__fastcall RadAreaLight_ctor(RadAreaLight *self) {
	self->name = NULL;
	self->rgbi[0] = self->rgbi[1] = self->rgbi[2] = self->rgbi[3] = 0;
	self->intensity = 1.0f;
	self->next = NULL;
	self->unused_10 = 0.0f;
	self->atten[1] = 0.0f;
	self->atten[0] = 0.0f;
	self->falloff = 1.0f;
	return self;
}

CRadiosity *__thiscall CRadiosity_ctor(CRadiosity *self, const char *dir) {
	char name[MAX_FILEPATH];

	CIniFile_ctor(&self->ini);
	if (dir) {
		Path_Merge(name, ".rad", dir);
		int r = CIniFile_Load(&self->ini, name, NULL, 0);
		E3D_DebugPrintf("\n%s: %s", name, r ? "no encontrado" : "ok");
		Path_Merge(name, "luces.rad", dir);
		r = CIniFile_Load(&self->ini, name, NULL, 0);
		E3D_DebugPrintf("\n%s: %s", name, r ? "no encontrado" : "ok");
	}
	/* the global file, a string of the exe's data */
	Path_Copy(name, GAME_STR(str_432a88, 0x432a88), MAX_FILEPATH);
	int r = CIniFile_Load(&self->ini, name, NULL, 0);
	E3D_DebugPrintf("\n%s: %s", name, r ? "no encontrado" : "ok");
	CHashTable *t = operator_new(sizeof(CHashTable));
	self->table = t ? CHashTable_ctor(t, 0x1f) : NULL;
	self->list = NULL;
	return self;
}

BspRegion *__thiscall BspRegion_ctor(BspRegion *self, int x, int y, int lx, int ly, BspRegion *parent) {
	self->x = (uint16_t)x;
	self->y = (uint16_t)y;
	self->lx = (uint16_t)lx;
	self->ly = (uint16_t)ly;
	self->color[0] = self->color[1] = self->color[2] = 0.0f;
	self->color[3] = 1.0f;
	self->links = NULL;
	self->interior = 1;
	self->child[0] = self->child[1] = self->child[2] = self->child[3] = NULL;
	self->parent = parent;
	self->accum[0] = self->accum[1] = self->accum[2] = 0.0f;
	self->accum[3] = 1.0f;
	if (parent) {
		for (int i = 0; i < 4; i++) {
			self->color[i] = parent->color[i];
		}
		self->plane = parent->plane;
	}
	return self;
}

void __fastcall BspLink_dtor(BspLink *self) {
	g_BspNumLinks--;
	if (self->next) {
		BspLink *e = self->next;
		BspLink_dtor(e);
		operator_delete_sized(e, sizeof(BspLink));
		self->next = NULL;
	}
	self->region = NULL;
}

void __fastcall CRadiosity_dtor(CRadiosity *self) {
	if (self->table) {
		CHashTable *t = self->table;
		CHashTable_dtor(t);
		operator_delete_sized(t, sizeof(CHashTable));
		self->table = NULL;
	}
	if (self->list) {
		RadAreaLight *l = self->list;
		if (l->next) {
			RadAreaLight_Delete(l->next, 1);
			l->next = NULL;
		}
		if (l->name) {
			String_Set(&l->name, NULL);
		}
		operator_delete_sized(l, sizeof(RadAreaLight));
		self->list = NULL;
	}
	CIniFile_dtor(&self->ini);
}

void __fastcall BspRegion_dtor(BspRegion *self) {
	if (self->links) {
		BspLink *e = self->links;
		BspLink_dtor(e);
		operator_delete_sized(e, sizeof(BspLink));
		self->links = NULL;
	}
	for (int i = 0; i < 4; i++) {
		if (self->child[i]) {
			BspRegion *h = self->child[i];
			BspRegion_dtor(h);
			operator_delete_sized(h, sizeof(BspRegion));
			self->child[i] = NULL;
		}
	}
	self->parent = NULL;
}

BspLink *__thiscall BspLink_Delete(BspLink *self, byte flags) {
	g_BspNumLinks--;
	if (self->next) {
		BspLink_Delete(self->next, 1);
		self->next = NULL;
	}
	self->region = NULL;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(BspLink));
	}
	return self;
}

RadAreaLight *__thiscall RadAreaLight_Delete(RadAreaLight *self, byte flags) {
	if (self->next) {
		RadAreaLight_Delete(self->next, 1);
		self->next = NULL;
	}
	if (self->name) {
		String_Set(&self->name, NULL);
	}
	if (flags & 1) {
		operator_delete_sized(self, sizeof(RadAreaLight));
	}
	return self;
}

RadAreaLight *__thiscall CRadiosity_Get(CRadiosity *self, CMaterial *Mat) {
	RadAreaLight *l = CRadiosity_Find(self, Mat);

	return l ? l : CRadiosity_Add(self, Mat);
}

/* the warnings of a material whose flag and definition do not match; the result of the read */
static byte Rad_Warn(const CMaterial *Mat, byte found) {
	if (Mat->params.attrib & MAT_AREALIGHT) {
		if (!found) {
			E3D_LogBlue(g_FileLog, "WARN: Area light data not found: %s", Mat->Name);
		}
	} else if (found) {
		E3D_LogBlue(g_FileLog, "WARN: Old area light definition found: %s", Mat->Name);
	}
	return found;
}

RadAreaLight *__thiscall CRadiosity_Add(CRadiosity *self, CMaterial *Mat) {
	CHECK(Mat);
	if (!Rad_Warn(Mat, CRadiosity_ReadMat(self, Mat, NULL))) {
		return NULL;
	}
	RadAreaLight *l = operator_new(sizeof(RadAreaLight));
	if (l) {
		l = RadAreaLight_ctor(l);
	}
	byte found = Rad_Warn(Mat, CRadiosity_ReadMat(self, Mat, l));
	CHECK(found);
	l->next = self->list;
	self->list = l;
	CHashTable_AddInt(self->table, (int)(uintptr_t)Mat, l, 0);
	return l;
}

RadAreaLight *__thiscall CRadiosity_Find(CRadiosity *self, CMaterial *Mat) {
	void *l;

	CHECK(Mat);
	CHECK(self->table);
	l = NULL;
	CHashTable_GetInt(self->table, (int)(uintptr_t)Mat, &l);
	return l;
}

/* (byte)(int)((double)v + 0.5) of a color byte through 0..1 */
static byte Rad_Byte(byte v) {
	return (byte)(int)((double)((float)v * (1.0f / 255.0f) * 255.0f) + 0.5);
}

byte __thiscall CRadiosity_ReadMat(CRadiosity *self, CMaterial *Mat, RadAreaLight *dest) {
	char name[RAD_NAME], buf[RAD_NAME];

	CHECK(Mat);
	const char *s = Mat->Name;
	for (; *s; s++) {
		if (*s == '(') {
			s++;
			while (*s == ' ') {
				s++;
			}
			if (!*s) {
				return 0;
			}
			if (*s == '+') {
				s++;
				if (!crt_strnicmp(s, "area_light:", 11)) {
					s += 11;
				}
				while (*s == ' ') {
					s++;
				}
				if (!*s) {
					return 0;
				}
				const char *start = s;
				while (*s != ' ' && *s != ')') {
					if (!*++s) {
						break;
					}
				}
				uint32_t len = (uint32_t)(s - start) + 1;
				String_Copy(name, start, len > RAD_NAME ? RAD_NAME : (int)len);
				if (DataFile_FindSection(self->ini.dat, name, 1)) {
					goto found;
				}
			}
		}
		if (!*s) {
			return 0;
		}
	}
	return 0;
found:
	if (dest) {
		String_Set(&dest->name, name);
		byte r = Rad_Byte(Mat->params.diffuse[0]);
		byte g = Rad_Byte(Mat->params.diffuse[1]);
		byte b = Rad_Byte(Mat->params.diffuse[2]);
		dest->unused_10 = 0.0f;
		dest->atten[0] = 0.0f;
		dest->atten[1] = Rad_Float(RAD_ATTEN_MAX);
		dest->falloff = 1.0f;
		dest->rgbi[0] = r;
		dest->rgbi[1] = g;
		dest->rgbi[2] = b;
		dest->rgbi[3] = 0;
		dest->intensity = 1.0f;
		if (!CIniFile_ReadText(&self->ini, name, "rgbi", buf, sizeof(buf))) {
			Proc_Scan(buf, "&b&b&b&f", &dest->rgbi[0], &dest->rgbi[1], &dest->rgbi[2], &dest->intensity);
		}
		if (!CIniFile_ReadText(&self->ini, name, "aten", buf, sizeof(buf))) {
			Proc_Scan(buf, "&f&f", &dest->atten[0], &dest->atten[1]);
		}
		if (!CIniFile_ReadFloat(&self->ini, name, "falloff", &dest->falloff)) {
			CHECK(dest->falloff > 0.0f);
		}
	}
	return 1;
}

byte __thiscall CRadiosity_GetColor(CRadiosity *self, CMaterial *Mat, void *dest) {
	CHECK(Mat && dest);
	RadAreaLight *l = CRadiosity_Find(self, Mat);
	if (!l) {
		return 0;
	}
	uint32_t *d = dest;
	d[0] = *(const uint32_t *)l->rgbi;
	*(float *)&d[1] = l->intensity;
	return 1;
}

byte __thiscall CRadiosity_ReadMatWarn(CRadiosity *self, CMaterial *Mat, RadAreaLight *dest) {
	return Rad_Warn(Mat, CRadiosity_ReadMat(self, Mat, dest));
}

vec3_t *Rad_Normals(uint32_t *num) {
	CTable *t = &g_RadNormals;

	CHECK(num);
	if (!t->n) {
		int count, n;
		float x, y, z;
		Read_Open(NULL, "bmp\\normales.rad", 1);
		Read_Int("rad_normales:", &count);
		Table_AllocC(t, count);
		for (int i = 0; i < count; i++) {
			Read_NextLine();
			int nc = crt_sscanf(g_ReadString, "%d:%f%f%f", &n, &x, &y, &z);
			CHECK(nc == 4 && n == i);
			float len = (float)__builtin_sqrt((double)(x * x + y * y + z * z));
			vec3_t v = { x, y, z };
			if ((double)len != 0.0) {
				float k = (float)(1.0 / (double)len);
				v.x = k * x;
				v.y = k * y;
				v.z = k * z;
			}
			TABLE_CHECK(t->p, i, t->n);
			((vec3_t *)t->p)[i] = v;
		}
		Read_Close();
	}
	*num = t->n;
	return t->p;
}

/* the atexit destructor of g_RadNormals (rva 0x38f070) and the static initializer that registers it (rva 0x05f7e0) */
void Radiosity_FiniNormals(void) {
	Table_Free(&g_RadNormals);
}

void Radiosity_InitNormals(void) {
	crt_atexit(Radiosity_FiniNormals);
}
