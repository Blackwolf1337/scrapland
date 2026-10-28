/*
 * The radiosity data of the light maps, original source Engine/radiosidad.cpp (include/scrap/bsp.h): the region
 * tree and links of a list, the area lights of the materials and the table of the sample normals.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/bsp.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/leedatos.h"
#include "scrap/varios.h"

#define RAD_NAME 0x40
/* the default end of the attenuation, 1.8441e19 */
#define RAD_ATEN_MAX 0x5f7ffd5bu

static float Rad_Float(uint32_t bits) {
	float f;

	memcpy(&f, &bits, sizeof(f));
	return f;
}

BspEnlace *__thiscall BspEnlace_ctor(BspEnlace *self, BspEnlace *sig, BspRegion *region, float ff) {
	g_BspNumEnlaces++;
	self->sig = sig;
	self->region = region;
	self->ff = ff;
	return self;
}

RadAreaLuz *__fastcall RadAreaLuz_ctor(RadAreaLuz *self) {
	self->nombre = NULL;
	self->rgbi[0] = self->rgbi[1] = self->rgbi[2] = self->rgbi[3] = 0;
	self->intensidad = 1.0f;
	self->sig = NULL;
	self->f10 = 0.0f;
	self->aten[1] = 0.0f;
	self->aten[0] = 0.0f;
	self->falloff = 1.0f;
	return self;
}

CRadiosidad *__thiscall CRadiosidad_ctor(CRadiosidad *self, const char *dir) {
	char name[MAX_RUTA];

	CArchINI_ctor(&self->ini);
	if (dir) {
		Ruta_Merge(name, ".rad", dir);
		int r = CArchINI_Load(&self->ini, name, NULL, 0);
		E3D_DebugPrintf("\n%s: %s", name, r ? "no encontrado" : "ok");
		Ruta_Merge(name, "luces.rad", dir);
		r = CArchINI_Load(&self->ini, name, NULL, 0);
		E3D_DebugPrintf("\n%s: %s", name, r ? "no encontrado" : "ok");
	}
	/* the global file, a string of the exe's data */
	Ruta_Copy(name, GAME_STR(str_432a88, 0x432a88), MAX_RUTA);
	int r = CArchINI_Load(&self->ini, name, NULL, 0);
	E3D_DebugPrintf("\n%s: %s", name, r ? "no encontrado" : "ok");
	CTablaHash *t = operator_new(sizeof(CTablaHash));
	self->tabla = t ? CTablaHash_ctor(t, 0x1f) : NULL;
	self->lista = NULL;
	return self;
}

BspRegion *__thiscall BspRegion_ctor(BspRegion *self, int x, int y, int lx, int ly, BspRegion *padre) {
	self->x = (uint16_t)x;
	self->y = (uint16_t)y;
	self->lx = (uint16_t)lx;
	self->ly = (uint16_t)ly;
	self->color[0] = self->color[1] = self->color[2] = 0.0f;
	self->color[3] = 1.0f;
	self->enlaces = NULL;
	self->interior = 1;
	self->hijo[0] = self->hijo[1] = self->hijo[2] = self->hijo[3] = NULL;
	self->padre = padre;
	self->acum[0] = self->acum[1] = self->acum[2] = 0.0f;
	self->acum[3] = 1.0f;
	if (padre) {
		for (int i = 0; i < 4; i++) {
			self->color[i] = padre->color[i];
		}
		self->plano = padre->plano;
	}
	return self;
}

void __fastcall BspEnlace_dtor(BspEnlace *self) {
	g_BspNumEnlaces--;
	if (self->sig) {
		BspEnlace *e = self->sig;
		BspEnlace_dtor(e);
		operator_delete_sized(e, sizeof(BspEnlace));
		self->sig = NULL;
	}
	self->region = NULL;
}

void __fastcall CRadiosidad_dtor(CRadiosidad *self) {
	if (self->tabla) {
		CTablaHash *t = self->tabla;
		CTablaHash_dtor(t);
		operator_delete_sized(t, sizeof(CTablaHash));
		self->tabla = NULL;
	}
	if (self->lista) {
		RadAreaLuz *l = self->lista;
		if (l->sig) {
			RadAreaLuz_Delete(l->sig, 1);
			l->sig = NULL;
		}
		if (l->nombre) {
			Cad_Set(&l->nombre, NULL);
		}
		operator_delete_sized(l, sizeof(RadAreaLuz));
		self->lista = NULL;
	}
	CArchINI_dtor(&self->ini);
}

void __fastcall BspRegion_dtor(BspRegion *self) {
	if (self->enlaces) {
		BspEnlace *e = self->enlaces;
		BspEnlace_dtor(e);
		operator_delete_sized(e, sizeof(BspEnlace));
		self->enlaces = NULL;
	}
	for (int i = 0; i < 4; i++) {
		if (self->hijo[i]) {
			BspRegion *h = self->hijo[i];
			BspRegion_dtor(h);
			operator_delete_sized(h, sizeof(BspRegion));
			self->hijo[i] = NULL;
		}
	}
	self->padre = NULL;
}

BspEnlace *__thiscall BspEnlace_Delete(BspEnlace *self, byte flags) {
	g_BspNumEnlaces--;
	if (self->sig) {
		BspEnlace_Delete(self->sig, 1);
		self->sig = NULL;
	}
	self->region = NULL;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(BspEnlace));
	}
	return self;
}

RadAreaLuz *__thiscall RadAreaLuz_Delete(RadAreaLuz *self, byte flags) {
	if (self->sig) {
		RadAreaLuz_Delete(self->sig, 1);
		self->sig = NULL;
	}
	if (self->nombre) {
		Cad_Set(&self->nombre, NULL);
	}
	if (flags & 1) {
		operator_delete_sized(self, sizeof(RadAreaLuz));
	}
	return self;
}

RadAreaLuz *__thiscall CRadiosidad_Get(CRadiosidad *self, CMaterial *Mat) {
	RadAreaLuz *l = CRadiosidad_Find(self, Mat);

	return l ? l : CRadiosidad_Add(self, Mat);
}

/* the warnings of a material whose flag and definition do not match; the result of the read */
static byte Rad_Warn(const CMaterial *Mat, byte found) {
	if (Mat->params.atrib & MAT_AREALIGHT) {
		if (!found) {
			E3D_LogBlue(g_FileLog, "WARN: Area light data not found: %s", Mat->Nombre);
		}
	} else if (found) {
		E3D_LogBlue(g_FileLog, "WARN: Old area light definition found: %s", Mat->Nombre);
	}
	return found;
}

RadAreaLuz *__thiscall CRadiosidad_Add(CRadiosidad *self, CMaterial *Mat) {
	CHECK(Mat);
	if (!Rad_Warn(Mat, CRadiosidad_ReadMat(self, Mat, NULL))) {
		return NULL;
	}
	RadAreaLuz *l = operator_new(sizeof(RadAreaLuz));
	if (l) {
		l = RadAreaLuz_ctor(l);
	}
	byte found = Rad_Warn(Mat, CRadiosidad_ReadMat(self, Mat, l));
	CHECK(found);
	l->sig = self->lista;
	self->lista = l;
	CTablaHash_AddInt(self->tabla, (int)(uintptr_t)Mat, l, 0);
	return l;
}

RadAreaLuz *__thiscall CRadiosidad_Find(CRadiosidad *self, CMaterial *Mat) {
	void *l;

	CHECK(Mat);
	CHECK(self->tabla);
	l = NULL;
	CTablaHash_GetInt(self->tabla, (int)(uintptr_t)Mat, &l);
	return l;
}

/* (byte)(int)((double)v + 0.5) of a color byte through 0..1 */
static byte Rad_Byte(byte v) {
	return (byte)(int)((double)((float)v * (1.0f / 255.0f) * 255.0f) + 0.5);
}

byte __thiscall CRadiosidad_ReadMat(CRadiosidad *self, CMaterial *Mat, RadAreaLuz *dest) {
	char name[RAD_NAME], buf[RAD_NAME];

	CHECK(Mat);
	const char *s = Mat->Nombre;
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
				Cad_Copy(name, start, len > RAD_NAME ? RAD_NAME : (int)len);
				if (Arch_FindSection(self->ini.dat, name, 1)) {
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
		Cad_Set(&dest->nombre, name);
		byte r = Rad_Byte(Mat->params.diffuse[0]);
		byte g = Rad_Byte(Mat->params.diffuse[1]);
		byte b = Rad_Byte(Mat->params.diffuse[2]);
		dest->f10 = 0.0f;
		dest->aten[0] = 0.0f;
		dest->aten[1] = Rad_Float(RAD_ATEN_MAX);
		dest->falloff = 1.0f;
		dest->rgbi[0] = r;
		dest->rgbi[1] = g;
		dest->rgbi[2] = b;
		dest->rgbi[3] = 0;
		dest->intensidad = 1.0f;
		if (!CArchINI_ReadText(&self->ini, name, "rgbi", buf, sizeof(buf))) {
			Proc_Scan(buf, "&b&b&b&f", &dest->rgbi[0], &dest->rgbi[1], &dest->rgbi[2], &dest->intensidad);
		}
		if (!CArchINI_ReadText(&self->ini, name, "aten", buf, sizeof(buf))) {
			Proc_Scan(buf, "&f&f", &dest->aten[0], &dest->aten[1]);
		}
		if (!CArchINI_ReadFloat(&self->ini, name, "falloff", &dest->falloff)) {
			CHECK(dest->falloff > 0.0f);
		}
	}
	return 1;
}

byte __thiscall CRadiosidad_GetColor(CRadiosidad *self, CMaterial *Mat, void *dest) {
	CHECK(Mat && dest);
	RadAreaLuz *l = CRadiosidad_Find(self, Mat);
	if (!l) {
		return 0;
	}
	uint32_t *d = dest;
	d[0] = *(const uint32_t *)l->rgbi;
	*(float *)&d[1] = l->intensidad;
	return 1;
}

byte __thiscall CRadiosidad_ReadMatWarn(CRadiosidad *self, CMaterial *Mat, RadAreaLuz *dest) {
	return Rad_Warn(Mat, CRadiosidad_ReadMat(self, Mat, dest));
}

vec3_t *Rad_Normales(uint32_t *num) {
	CTabla *t = &g_RadNormales;

	CHECK(num);
	if (!t->n) {
		int count, n;
		float x, y, z;
		Lee_Open(NULL, "bmp\\normales.rad", 1);
		Lee_Int("rad_normales:", &count);
		Tabla_AllocC(t, count);
		for (int i = 0; i < count; i++) {
			Lee_NextLine();
			int nc = crt_sscanf(g_LeeCad, "%d:%f%f%f", &n, &x, &y, &z);
			CHECK(nc == 4 && n == i);
			float len = (float)__builtin_sqrt((double)(x * x + y * y + z * z));
			vec3_t v = { x, y, z };
			if ((double)len != 0.0) {
				float k = (float)(1.0 / (double)len);
				v.x = k * x;
				v.y = k * y;
				v.z = k * z;
			}
			TABLA_CHECK(t->p, i, t->n);
			((vec3_t *)t->p)[i] = v;
		}
		Lee_Close();
	}
	*num = t->n;
	return t->p;
}

/* the atexit destructor of g_RadNormales (rva 0x38f070) and the static initializer that registers it (rva 0x05f7e0) */
void Radiosidad_FiniNormales(void) {
	Tabla_Free(&g_RadNormales);
}

void Radiosidad_InitNormales(void) {
	crt_atexit(Radiosidad_FiniNormales);
}
