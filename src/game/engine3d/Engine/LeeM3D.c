/*
 * The value reader of the M3D files, original source Engine/lee_M3D.cpp (include/scrap/leem3d.h).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/leedatos.h"
#include "scrap/leem3d.h"
#include "scrap/material.h"
#include "scrap/varios.h"

/* a binary string: its length, then the characters */
static void M3D_ReadBinString(char *dest, int size) {
	int n = M3D_ReadInt(NULL);

	CHECK(n < size);
	Lee_Bytes(dest, n);
	dest[n] = '\0';
}

void M3D_Label(const char *name) {
	if (!g_M3DBinary) {
		if (g_M3DLabels) {
			Lee_Expect(name);
		} else {
			Lee_NextLine();
		}
		return;
	}
	if (g_M3DLabels) {
		M3D_ReadBinString(g_M3DCad, M3D_CAD);
		CHECK(!strcmp(g_M3DCad, name));
	}
}

int M3D_ReadInt(const char *tok) {
	int v = 0, n = 0;

	if (tok) {
		M3D_Label(tok);
	}
	if (!g_M3DBinary) {
		if (crt_sscanf(g_LeeCad, "%d%n", &v, &n) != 1) {
			Lee_Error("n\xfamero entero no v\xe1lido: %s", g_LeeCad);
		}
		g_LeeCad += n;
		return v;
	}
	uint32_t u = Lee_Byte();
	if ((int)u > 0x3f) {
		uint32_t neg = u & 0x40;
		byte c = u;
		byte shift = 6;
		u &= 0x3f;
		while (c & 0x80) {
			uint32_t b = Lee_Byte();
			u |= (b & 0x7f) << (shift & 31);
			shift += 7;
			c = b;
		}
		if (neg) {
			u = ~u;
		}
	}
	return u;
}

float M3D_ReadFloat(const char *tok) {
	float f = 0.0f;
	int n = 0;

	if (tok) {
		M3D_Label(tok);
	}
	if (g_M3DBinary) {
		Lee_Bytes(&f, sizeof(f));
		return f;
	}
	if (crt_sscanf(g_LeeCad, "%f%n", &f, &n) != 1) {
		Lee_Error("n\xfamero real no v\xe1lido: %s", g_LeeCad);
	}
	g_LeeCad += n;
	return f;
}

byte M3D_ReadBool(const char *tok) {
	if (g_M3DBinary) {
		uint32_t v = M3D_ReadInt(tok);
		CHECK(v < 2);
		return v != 0;
	}
	if (tok) {
		Lee_Expect(tok);
	}
	if (!crt_stricmp(g_LeeCad, "si")) {
		g_LeeCad += 2;
		return 1;
	}
	if (!crt_stricmp(g_LeeCad, "no")) {
		g_LeeCad += 2;
		return 0;
	}
	Lee_Error("dato \"bool\" no v\xe1lido: %s", g_LeeCad);
	return 0;
}

char *M3D_ReadString(const char *tok, char *dest, int size, byte quoted) {
	CHECK(!dest || size > 0);
	if (tok) {
		M3D_Label(tok);
	}
	if (!dest) {
		dest = g_M3DCad;
		size = M3D_CAD;
	}
	if (g_M3DBinary) {
		M3D_ReadBinString(dest, size);
	} else if (quoted) {
		Lee_QText(NULL, dest, size);
	} else {
		Lee_Text(NULL, dest, size);
	}
	return dest;
}

void M3D_ReadVec3(const char *tok, vec3_t *v, byte swapYZ) {
	CHECK(v);
	float x = M3D_ReadFloat(tok);
	float a = M3D_ReadFloat(NULL);
	float b = M3D_ReadFloat(NULL);
	v->x = x;
	v->y = swapYZ ? b : a;
	v->z = swapYZ ? a : b;
}

void M3D_ReadVec2(const char *tok, float *v) {
	CHECK(v);
	v[0] = M3D_ReadFloat(tok);
	v[1] = M3D_ReadFloat(NULL);
}

void M3D_ReadQuat(const char *tok, quat_t *q, byte swapYZ) {
	CHECK(q);
	float x = M3D_ReadFloat(tok);
	float a = M3D_ReadFloat(NULL);
	float b = M3D_ReadFloat(NULL);
	float ang = M3D_ReadFloat(NULL);
	vec3_t axis = { -x, -(swapYZ ? b : a), -(swapYZ ? a : b) };
	quat_t r;
	*q = *Quat_FromAxisAngle(&r, &axis, ang);
}

void M3D_ReadTransform(const char *name, m3dTransform_t *mt) {
	CHECK(mt);
	if (g_M3DLabels) {
		M3D_Label("MT:");
		M3D_ReadString(NULL, NULL, 0, 0);
		CHECK(!strcmp(g_M3DCad, name));
	}
	M3D_ReadVec3("MT_Pos:", &mt->pos, 1);
	mt->signo = M3D_ReadFloat("MT_Signo:");
	M3D_ReadQuat("MT_Rot:", &mt->rot, 1);
	M3D_ReadQuat("MT_RotEsc:", &mt->rotEsc, 1);
	M3D_ReadVec3("MT_Esc:", &mt->esc, 1);
}

void M3D_ReadIndex(int expected) {
	if (!g_M3DBinary) {
		Lee_NextLine();
	}
	int i = M3D_ReadInt(NULL);
	if (i != expected) {
		Err_Printf(1, "Indice no v\xe1lido: %d != %d\n'%s'", i, expected, g_M3DName);
	}
	if (!g_M3DBinary) {
		CHECK(*g_LeeCad == ':');
		g_LeeCad++;
	}
}

/* the cache path of name opened and its header checked: the open file positioned after it, NULL when there is none
 * or it is out of date while the M3D can make it again */
static CArchIO *M3D_CacheOpen(const char *path, const char *name, const char *id, int checkTime, const char *other) {
	CArchIO *f = operator_new(sizeof(CArchIO));
	f = f ? CArchIO_ctor(f, path, "rb") : NULL;
	if (f->error) {
		CArchIO_dtor(f);
		operator_delete_sized(f, sizeof(CArchIO));
		return NULL;
	}
	E3D_LogDebug(g_FileLog, id[0] == 'C' ? "CM3: %s" : "SM3: %s", path);
	if (CArchIO_BeginBlock(f, id)) {
		CheckFailed(id[0] == 'C' ? "f->leer_ini_bloque( \"CM3\" ) == 0" : "f->leer_ini_bloque( \"SM3\" ) == 0",
			__FILE__, __TIMESTAMP__, __LINE__);
	}
	int ver = CArchIO_ReadInt(f);
	int timeM3D = CArchIO_ReadInt(f);
	int timeOther = CArchIO_ReadInt(f);
	if ((checkTime && (File_TimeChanged(timeM3D, name, 2) || File_TimeChanged(timeOther, other, 2))) ||
		ver != M3D_CACHE_VER) {
		if (File_Exists(name)) {
			CArchIO_Finish(f);
			CArchIO_dtor(f);
			operator_delete_sized(f, sizeof(CArchIO));
			return NULL;
		}
		Con_Warnf("WARN: Could not rebuild: %s\n", path);
	}
	return f;
}

/* the scene read from the open cache, the file closed */
static CEscena *M3D_CacheRead(CArchIO *f, const char *escName, const char *id, byte bAnim, byte fromSM3) {
	CEscena *esc = operator_new(sizeof(CEscena));
	esc = esc ? CEscena_ctor(esc) : NULL;
	Cad_Set(&esc->name, escName);
	if (fromSM3) {
		esc->fromAmc = 1;
	}
	CEscena_Read(esc, f, bAnim);
	CArchIO_Error(f, id);
	CHECK(!f->error);
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(CArchIO));
	return esc;
}

CEscena *CM3_Load(const char *name) {
	char path[260];

	CHECK(g_EscenaAnimPadre);
	Ruta_Format(path, name, "&U&D&N.CM3");
	if (!File_Exists(path)) {
		return NULL;
	}
	CArchIO *f = M3D_CacheOpen(path, name, "CM3", g_CM3CheckTime, g_EscenaAnimPadre->name);
	return f ? M3D_CacheRead(f, g_M3DName, "CM3", 1, 0) : NULL;
}

CEscena *SM3_Load(const char *name) {
	char path[260], ini[260];

	CHECK(!g_EscenaAnimPadre);
	if (g_SM3UseScenes != 1) {
		return NULL;
	}
	Ruta_Format(path, name, "&U&D&N.SM3");
	Ruta_Format(ini, name, "&U&D&N.INI");
	if (!File_Exists(path)) {
		return NULL;
	}
	CArchIO *f = M3D_CacheOpen(path, name, "SM3", g_SM3CheckTime, ini);
	if (!f) {
		return NULL;
	}
	CEscena *esc = M3D_CacheRead(f, name, "SM3", 0, 1);
	if (!g_EscenaLODPadre) {
		Escena_Particles(esc);
	}
	return esc;
}
