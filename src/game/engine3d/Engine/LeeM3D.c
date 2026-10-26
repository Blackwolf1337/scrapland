/*
 * The value reader of the M3D files, original source Engine/lee_M3D.cpp (include/scrap/leem3d.h).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/flare.h"
#include "scrap/grafico3d.h"
#include "scrap/leedatos.h"
#include "scrap/leem3d.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/render.h"
#include "scrap/sistpart.h"
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

/* the cache path of esc written: the header, then the scene */
static void M3D_CacheWrite(const char *path, CEscena *esc, const char *id, const char *name, const char *other,
	byte bAnim) {
	E3D_LogVerbose(g_FileLog, id[0] == 'C' ? "Save_CM3: %s" : "Save_SM3: %s", path);
	CArchIO *f = operator_new(sizeof(CArchIO));
	f = f ? CArchIO_ctor(f, path, "wb") : NULL;
	if (!f->error) {
		CArchIO_BeginBlockWrite(f, id);
		CArchIO_WriteInt(f, M3D_CACHE_VER);
		CArchIO_WriteInt(f, File_Time(name));
		CArchIO_WriteInt(f, File_Time(other));
		CEscena_Write(esc, f, bAnim);
		CArchIO_EndBlockWrite(f, id);
		if (!f->error) {
			CArchIO_dtor(f);
			operator_delete_sized(f, sizeof(CArchIO));
			if (bAnim && g_CM3DeleteM3D) {
				Con_Verbosef("INFO: Delete M3D: %s\n", name);
				if (crt_remove(name)) {
					CheckFailed("!unlink( NameM3D )", __FILE__, __TIMESTAMP__, __LINE__);
				}
			}
			return;
		}
	}
	Err_Report(f->err, 0, "\n'%s'", path);
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(CArchIO));
}

void CM3_Save(const char *name, CEscena *esc) {
	char path[260];

	CHECK(g_EscenaAnimPadre);
	Ruta_Format(path, name, "&U&D&N.CM3");
	M3D_CacheWrite(path, esc, "CM3", name, g_EscenaAnimPadre->name, 1);
}

void SM3_Save(const char *name, CEscena *esc) {
	char path[260], ini[260];

	CHECK(!g_EscenaAnimPadre);
	if (esc->fromAmc) {
		CheckFailed("!Escena->ReadFromSM3", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!g_SM3UseScenes) {
		return;
	}
	Ruta_Format(path, name, "&U&D&N.SM3");
	Ruta_Format(ini, name, "&U&D&N.INI");
	M3D_CacheWrite(path, esc, "SM3", name, ini, 0);
}

void M3D_SetOk(byte ok) {
	if (ok) {
		g_M3DOk = 1;
		return;
	}
	if (g_M3DOk) {
		g_M3DOk = 0;
		E3D_LogVerbose(g_FileLog, "ERROR: lee_M3D(): '%s'", g_M3DName);
	}
}

void Escena_CheckDuplicados(CEscena *esc, CNodo3D *nodo) {
	for (CNodo3D *n = esc->Nodo3D; n;) {
		CNodo3D *n2 = CNodo3D_Find(n, nodo->name, 1);
		CHECK(n2);
		if (n2 == nodo) {
			return;
		}
		E3D_LogVerbose(g_FileLog, "Duplicated node: '%s' [%s] (%d,%d) [%s] [%s]", nodo->name, esc->name, nodo->indice,
			n2->indice, nodo->padre ? nodo->padre->name : "(null)", n2->padre ? n2->padre->name : "(null)");
		if (CNodo3D_Clase(nodo) || nodo->flags & NODO_JOIN2 || CNodo3D_Clase(n2) || n2->flags & NODO_JOIN2) {
			Con_Warnf("Duplicated node: %s\n", nodo->name);
		}
		n = n2->sig;
	}
}

void Nodo_MarcarBip(CNodo3D *nodo) {
	static const char *const pre[4] = { "Bip ", "Bip_", "BipB", "Bone" };
	for (int i = 0; i < 4; i++) {
		if (!crt_strncmp(nodo->name, pre[i], 4)) {
			nodo->flags |= NODO_BIP;
			return;
		}
	}
}

/* the material of the first triangle of the mesh of the node */
static CMaterial *Nodo_MatTri0(CNodo3D *n, CMallaTri **malla) {
	*malla = Obj3D_MallaTri(n->obj3D);
	return CMallaTri_GetTri(*malla, 0)->mat;
}

void Escena_Flares(CEscena *esc) {
	if (!esc || !esc->Nodo3D) {
		return;
	}
	for (CNodo3D *n = CNodo3D_FindWild(esc->Nodo3D, "flare*", 1); n; n = CNodo3D_FindWild(n, "flare*", 0)) {
		if (!n->obj3D) {
			continue;
		}
		CMallaTri *mt;
		CMaterial *mat = Nodo_MatTri0(n, &mt);
		CFlare *f = operator_new(sizeof(CFlare));
		f = f ? CFlare_ctor(f) : NULL;
		byte opac = CMaterial_GetMapName(mat, 1) != NULL;
		byte alpha = mat->params.diffuse[3];
		int src = opac ? mat->params.srcBlend : 2, dst = opac ? mat->params.dstBlend : 6;
		const char *op = CMaterial_GetMapName(mat, opac);
		CFlare_SetTexture(f, CMaterial_GetMapName(mat, 0), op, src, dst, alpha);
		f->pos = n->pos;
		CObjBase3D_SetNodo((IObjeto3D *)f, n);
		CNodo3D_FreeObj3D(n);
		CNodo3D_SetObj3D(n, (IObjeto3D *)f);
		int tam;
		if (Nodo_ParamInt(n->name, "tam", &tam)) {
			f->escala[0] = (float)tam;
			f->escala[1] = (float)tam;
		}
	}
}

void Escena_Sprites(CEscena *esc) {
	if (!esc || !esc->Nodo3D) {
		return;
	}
	for (CNodo3D *n = CNodo3D_FindWild(esc->Nodo3D, "sprite*", 1); n; n = CNodo3D_FindWild(n, "sprite*", 0)) {
		CMallaTri *mt;
		CMaterial *mat = Nodo_MatTri0(n, &mt);
		CGrafico3D *g = operator_new(sizeof(CGrafico3D));
		g = g ? CGrafico3D_ctor(g) : NULL;
		const char *op = CMaterial_GetMapName(mat, 1);
		CGrafico3D_CreateMat(g, CMaterial_GetMapName(mat, 0), op, mat->params.srcBlend, mat->params.dstBlend,
			mat->params.diffuse[3]);
		CMallaTri_CalcBox(mt, NULL, 1);
		g->pos = n->pos;
		const box_t *b = &mt->box;
		float dx = b->max.x - b->min.x, dy = b->max.y - b->min.y, dz = b->max.z - b->min.z;
		float r = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
		g->ancho = r;
		g->alto = r;
		CGrafico3D_SetNodo(g, n);
		CNodo3D_FreeObj3D(n);
		CNodo3D_SetObj3D(n, (IObjeto3D *)g);
	}
}

void Escena_Particles(CEscena *esc) {
	if (!g_EscenaLODPadre && !esc->fromAmc) {
		CArchINI *ini = operator_new(sizeof(CArchINI));
		esc->userProp = ini ? CArchINI_ctor(ini) : NULL;
		if (!CArchINI_Load(esc->userProp, esc->name, "model_particles.ini", 0)) {
			E3D_LogDebug(g_FileLog, "INI: %s: %s", esc->name, "model_particles.ini");
		} else if (esc->userProp) {
			CArchINI_dtor(esc->userProp);
			operator_delete_sized(esc->userProp, sizeof(CArchINI));
			esc->userProp = NULL;
		}
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (!n->ini || !CArchINI_FindSection(n->ini, "Particles", 1)) {
			continue;
		}
		if (n->nodoR != -1) {
			CheckFailed("nodo->iNodeXRef == -1", __FILE__, __TIMESTAMP__, __LINE__);
		}
		if (!g_EscenaLODPadre) {
			if (n->obj3D) {
				CNodo3D_FreeObj3D(n);
				Con_Bluef("ERROR: (Particle System) Node Material not supported (Node: %s)\n", n->name);
			}
			CSistPart *sp = SistPart_FromNode(n, esc->userProp);
			if (!sp) {
				CheckFailed("sistpart", __FILE__, __TIMESTAMP__, __LINE__);
			}
			CNodo3D_SetObj3D(n, (IObjeto3D *)sp);
			continue;
		}
		CNodo3D *n2 = CEscena_FindNodo(g_EscenaLODPadre, n->name);
		if (!n2) {
			E3D_LogBlue(g_FileLog, "ERROR: LOD particles node not found in main LOD: %s", n->name);
			continue;
		}
		if (CNodo3D_Clase(n2) != CLASE_OBJ3D_SISTPART) {
			CheckFailed("nodo2->clase_obj3D() == CLASE_OBJ3D_SISTPART", __FILE__, __TIMESTAMP__, __LINE__);
		}
		n->nodoR = n2->i48;
		if (n->obj3D) {
			CheckFailed("!nodo->obj3D", __FILE__, __TIMESTAMP__, __LINE__);
		}
		n->flags |= 0x80;
		n->indice = -1;
	}
}

void Escena_Limpiar(CEscena *esc, const char *pass) {
	E3D_LogWhite(g_FileLog, "cleaning nodes (pass %s):", pass);
	CEscena_DeleteEmpty(esc, 0, 0, 1);
	if (g_ShowNodesTree && !g_EscenaAnimPadre) {
		Escena_ShowTree(esc, 0, 0);
	}
}

byte Float_CasiIgual(float a, float b) {
	if (a == b) {
		return 1;
	}
	if (a == 0.0f || b == 0.0f) {
		return 0;
	}
	if ((a > 0.0f && 0.0f > b) || (0.0f > a && b > 0.0f)) {
		return 0;
	}
	double r = (double)(a / b);
	if (r > 0x1.000004p+0) {
		return 0;
	}
	return !(0x1.fffff8p-1 > r);
}

byte Nodo_EsAntecesor(const CNodo3D *nodo, const CNodo3D *ant, byte cruzar) {
	CHECK(nodo && ant);
	for (const CNodo3D *p = nodo->padre; p; p = p->padre) {
		if (p == ant) {
			return 1;
		}
		if (!cruzar && p->flags & NODO_BORRAR) {
			return 0;
		}
	}
	return 0;
}

byte Nodo_EsDeJuego(const CNodo3D *nodo) {
	static const struct {
		const char *pre;
		int n;
	} pre[8] = { { "DR_", 3 }, { "DM_", 3 }, { "DC_", 3 }, { "DF_", 3 }, { "D_Cam-", 6 }, { "Actor_", 6 },
		{ "Traffic_", 8 }, { "NAVEBERTO", 10 } };
	for (int i = 0; i < 8; i++) {
		if (!crt_strnicmp(nodo->name, pre[i].pre, pre[i].n)) {
			return 1;
		}
	}
	return 0;
}

byte Nodo_ParamInt(const char *name, const char *key, int *out) {
	const char *s = name;
	size_t len = strlen(key);

	if (!*s) {
		return 0;
	}
	do {
		if (*s == '(') {
			const char *p = s + 1;
			while (*p == ' ') {
				p++;
			}
			s = *p == '+' ? p + 1 : p;
			if (!*s) {
				return 0;
			}
			if (!crt_strnicmp(s, key, (int)len)) {
				s += len;
				while (*s == ' ') {
					s++;
				}
				if (*s == '=') {
					s++;
					if (!*s) {
						return 0;
					}
					int v;
					if (Proc_Scan(s, "&l", &v) == 1) {
						if (out) {
							*out = v;
						}
						return 1;
					}
				}
			}
		}
		if (!*s) {
			return 0;
		}
		s++;
	} while (*s);
	return 0;
}

void Nodo_LimpiarNombre(char *name) {
	int i = 0;

	if (!*name) {
		return;
	}
	int last;
	do {
		last = i;
		if ((byte)name[i] > 0x7f) {
			name[i] = '_';
		}
		i++;
	} while (name[i]);
	if (i > 0 && name[last] == ' ') {
		Con_Verbosef("WARN: Space at end of node name: '%s'\n", name);
	}
}

byte M3D_MapaActivo(const CMaterial *mat, int idmapa) {
	if (mat->params.atrib & MAT_SHADER) {
		return 1;
	}
	switch (idmapa) {
	case MAPA_OPACITY:
		return g_M3DUseOpacity;
	case MAPA_ENTORNO:
		return g_UsarEnvMap && g_M3DUseEnvmap;
	case MAPA_BUMP:
		return g_UsarBumpMap && g_M3DUseBumpmap;
	case MAPA_GLOW:
		return g_UsarGlowMap && g_M3DUseGlowmap;
	default:
		return 1;
	}
}
