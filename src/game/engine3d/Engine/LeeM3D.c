/*
 * The value reader of the M3D files, original source Engine/lee_M3D.cpp (include/scrap/leem3d.h).
 */
#include <string.h>

#include "scrap/camara.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/flare.h"
#include "scrap/grafico3d.h"
#include "scrap/leedatos.h"
#include "scrap/leem3d.h"
#include "scrap/luz.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/portal.h"
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


/* the materials of the file */

CMaterial *M3D_FindMat(int iMat, int iSubmat) {
	for (int i = 0; i < g_M3DNumMat; i++) {
		if (g_M3DMatId[i] != iMat || g_M3DMatSub[i] != iSubmat) {
			continue;
		}
		CMaterial *res = g_M3DMat[i];
		if (iSubmat != -1) {
			if (!res) {
				CHECK(res);
			} else {
				while (res->params.i2c == 1 && res->sub) {
					res = res->sub->pri;
					Con_FatalErrorf("Hay un multimaterial dentro de otro multimaterial: %s", res->Nombre);
				}
			}
		}
		return res;
	}
	CMaterial *res = NULL;
	if (iSubmat != -1) {
		res = M3D_FindMat(iMat, -1);
		if (res && res->sub) {
			int num = CListaMat_Count(res->sub, 0);
			CHECK(num > 0);
			if (iSubmat < num) {
				return NULL;
			}
			res = M3D_FindMat(iMat, iSubmat % num);
		}
	}
	return res;
}

void M3D_MatUsado(CMaterial *mat) {
	if (!mat) {
		return;
	}
	int cnt = 0;
	for (int i = 0; i < g_M3DNumMat; i++) {
		if (g_M3DMat[i] == mat) {
			g_M3DMatUsado[i] = 1;
			cnt++;
		}
	}
	CHECK(cnt > 0);
}

void M3D_CompartirMats(CListaMat *lista) {
	for (int i = 0; i < g_M3DNumMat; i++) {
		if (g_M3DMat[i]->params.i2c == 1) {
			continue;
		}
		CHECK(!g_M3DMat[i]->sub);
		CMaterial *mat = CListaMat_Find(lista, g_M3DMat[i]);
		CHECK(!mat->sub);
		if (g_M3DMat[i] != mat) {
			E3D_LogDebug(g_FileLog, "WARN: Duplicated material: [%s]->[%s] [%s]", g_M3DMat[i]->Nombre, mat->Nombre,
				CMaterial_GetMapName(mat, 0));
		}
		g_M3DMat[i] = mat;
	}
}

/* a color channel of 0..1 as a byte */
static byte M3D_ColorByte(float f) {
	return (byte)(int)((double)(f * 255.0f) + 0.5);
}

/* the 3 channels, the 4th byte 0 */
static uint32_t M3D_Color(const vec3_t *c) {
	byte b[4];

	b[3] = 0;
	b[0] = M3D_ColorByte(c->x);
	b[1] = M3D_ColorByte(c->y);
	b[2] = M3D_ColorByte(c->z);
	return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

void M3D_ReadColores(CMaterial *mat) {
	vec3_t c;

	M3D_ReadVec3("Ambient:", &c, 0);
	*(uint32_t *)mat->params.ambient = M3D_Color(&c);
	M3D_ReadVec3("Diffuse:", &c, 0);
	*(uint32_t *)mat->params.diffuse = M3D_Color(&c);
	M3D_ReadVec3("Specular:", &c, 0);
	*(uint32_t *)mat->params.specular = M3D_Color(&c);
	mat->params.power = M3D_ReadFloat("Shine:");
	mat->params.specLevel = M3D_ReadFloat("ShineStrength:");
	float t = M3D_ReadFloat("Transparency:");
	byte a = (byte)(int)((1.0 - (double)t) * 255.0 + 0.5);
	mat->params.diffuse[3] = a;
	if (a != 0xff) {
		mat->params.transp = 1;
		return;
	}
	mat->params.diffuse[3] = 0;
}


/* the readers of single values */

void M3D_ReadMatrix(const char *name, matrix_t *out) {
	m3dTransform_t mt;

	M3D_ReadTransform(name, &mt);
	M3DTransform_ToMatrix(&mt, out);
}

void M3D_ReadVec2Next(float *v) {
	M3D_ReadVec2(NULL, v);
}

vec3_t *M3D_ReadVec3Ret(vec3_t *v, const char *tok, byte swapYZ) {
	M3D_ReadVec3(tok, v, swapYZ);
	return v;
}

void M3D_ReadVec3Next(vec3_t *v, byte swapYZ) {
	M3D_ReadVec3(NULL, v, swapYZ);
}

char *M3D_ReadQString(const char *tok, char *dest, int size) {
	return M3D_ReadString(tok, dest, size, 1);
}

/* the float of M3D_ReadFloat(NULL) stored as read (the inlined copies of the original never pass it through x87) */
static void M3D_ReadFloatTo(float *dest) {
	int n;

	if (g_M3DBinary) {
		Lee_Bytes(dest, sizeof(*dest));
		return;
	}
	if (crt_sscanf(g_LeeCad, "%f%n", dest, &n) != 1) {
		Lee_Error("n\xfamero real no v\xe1lido: %s", g_LeeCad);
	}
	g_LeeCad += n;
}


/* the objects of the file and their animation tables */

M3DNodo *M3D_NodoDat(const CNodo3D *nodo) {
	CHECK(g_M3DNodos && nodo);
	CHECK(nodo->indice >= 0);
	return &g_M3DNodos[nodo->indice];
}

vec3_t *__thiscall M3DTabla4_GetVec(M3DTabla4 *self, vec3_t *out, uint32_t i) {
	TABLA_CHECK(self->t[2].p, i, self->t[2].n);
	const float *z = (const float *)self->t[2].p + i;
	TABLA_CHECK(self->t[1].p, i, self->t[1].n);
	const float *y = (const float *)self->t[1].p + i;
	TABLA_CHECK(self->t[0].p, i, self->t[0].n);
	out->x = ((const float *)self->t[0].p)[i];
	out->y = *y;
	out->z = *z;
	return out;
}

quat_t *__thiscall M3DTabla4_GetQuat(M3DTabla4 *self, quat_t *out, uint32_t i) {
	TABLA_CHECK(self->t[3].p, i, self->t[3].n);
	const float *w = (const float *)self->t[3].p + i;
	TABLA_CHECK(self->t[2].p, i, self->t[2].n);
	const float *z = (const float *)self->t[2].p + i;
	TABLA_CHECK(self->t[1].p, i, self->t[1].n);
	const float *y = (const float *)self->t[1].p + i;
	TABLA_CHECK(self->t[0].p, i, self->t[0].n);
	out->x = ((const float *)self->t[0].p)[i];
	out->y = *y;
	out->z = *z;
	out->w = *w;
	return out;
}

void __thiscall M3DTabla4_SetVec(M3DTabla4 *self, uint32_t i, const vec3_t *v) {
	TABLA_CHECK(self->t[0].p, i, self->t[0].n);
	((float *)self->t[0].p)[i] = v->x;
	TABLA_CHECK(self->t[1].p, i, self->t[1].n);
	((float *)self->t[1].p)[i] = v->y;
	TABLA_CHECK(self->t[2].p, i, self->t[2].n);
	((float *)self->t[2].p)[i] = v->z;
}

void __thiscall M3DTabla4_RotToQuat(M3DTabla4 *self, uint32_t i) {
	vec3_t v, axis;
	quat_t q;

	TABLA_CHECK(self->t[3].p, i, self->t[3].n);
	const float *ang = (const float *)self->t[3].p + i;
	const vec3_t *r = M3DTabla4_GetVec(self, &v, i);
	axis.x = -r->x;
	axis.y = -r->y;
	axis.z = -r->z;
	Quat_FromAxisAngle(&q, &axis, *ang);
	v.x = q.x;
	v.y = q.y;
	v.z = q.z;
	M3DTabla4_SetVec(self, i, &v);
	TABLA_CHECK(self->t[3].p, i, self->t[3].n);
	((float *)self->t[3].p)[i] = q.w;
}

matrix_t *__thiscall M3DNodo_FrameMatrix(M3DNodo *self, matrix_t *out, uint32_t frame) {
	m3dTransform_t mt;
	vec3_t v;
	quat_t q;

	mt.pos = *M3DTabla4_GetVec(&self->pista.pos, &v, frame);
	mt.rot = *M3DTabla4_GetQuat(&self->pista.rot, &q, frame);
	mt.rotEsc = *M3DTabla4_GetQuat(&self->pista.rotEsc, &q, frame);
	mt.esc = *M3DTabla4_GetVec(&self->pista.esc, &v, frame);
	TABLA_CHECK(self->pista.sgn.p, frame, self->pista.sgn.n);
	mt.signo = ((const float *)self->pista.sgn.p)[frame];
	M3DTransform_ToMatrixAnim(&mt, out);
	return out;
}

/* a value of a table: an int or the bits of a float */
static void M3DTabla_ReadValor(uint32_t *dest, byte flt) {
	if (flt) {
		M3D_ReadFloatTo((float *)dest);
	} else {
		*dest = (uint32_t)M3D_ReadInt(NULL);
	}
}

/* an "Animacion:" table of the frames: runs of values (count, values; 20 a line in text) or of one value (-count,
 * value); a single run over all frames makes it fija */
static void M3DTabla_Read(M3DTabla *self, const char *nombre, byte flt) {
	M3D_ReadString("Animacion:", NULL, 0, 0);
	CHECK(!strcmp(g_M3DCad, nombre));
	CHECK(M3D_ReadInt("Tabla:") == 1);
	Tabla_Alloc4((CTabla *)self, g_M3DFrames);
	int fr = 0;
	if (self->n <= 0) {
		return;
	}
	do {
		if (!g_M3DBinary) {
			Lee_NextLine();
		}
		int num = M3D_ReadInt(NULL);
		int end;
		if (num < 0) {
			uint32_t v;
			num = -num;
			if (!fr && num == self->n) {
				self->fija = 1;
			}
			M3DTabla_ReadValor(&v, flt);
			end = num + fr;
			CHECK(end <= self->n);
			for (int i = 0; i < num; i++) {
				TABLA_CHECK(self->p, fr + i, self->n);
				((uint32_t *)self->p)[fr + i] = v;
			}
		} else {
			int k = 0;
			end = num + fr;
			CHECK(end <= self->n);
			for (int i = 0; i < num; i++) {
				uint32_t v;
				if (++k == 20) {
					if (!g_M3DBinary) {
						Lee_NextLine();
					}
					k = 0;
				}
				TABLA_CHECK(self->p, fr + i, self->n);
				uint32_t *p = (uint32_t *)self->p + fr + i;
				M3DTabla_ReadValor(&v, flt);
				*p = v;
			}
		}
		fr = end;
	} while (fr < self->n);
}

void __thiscall M3DTabla_ReadInt(M3DTabla *self, const char *nombre) {
	M3DTabla_Read(self, nombre, 0);
}

void __thiscall M3DTabla_ReadFloat(M3DTabla *self, const char *nombre) {
	M3DTabla_Read(self, nombre, 1);
}

void __thiscall M3DTabla4_Read(M3DTabla4 *self, const char *n1, const char *n2, const char *n3, const char *n4,
	byte inv23) {
	CHECK(!inv23 || (n2 && n3));
	if (n1) {
		M3DTabla_ReadFloat(&self->t[0], n1);
	}
	if (n2) {
		M3DTabla_ReadFloat(&self->t[inv23 ? 2 : 1], n2);
	}
	if (n3) {
		M3DTabla_ReadFloat(&self->t[inv23 ? 1 : 2], n3);
	}
	if (n4) {
		M3DTabla_ReadFloat(&self->t[3], n4);
	}
}

void __thiscall M3DPista_Read(M3DPista *self) {
	M3DTabla_ReadFloat(&self->pos.t[0], "PosX");
	M3DTabla_ReadFloat(&self->pos.t[2], "PosY");
	M3DTabla_ReadFloat(&self->pos.t[1], "PosZ");
	M3DTabla_ReadFloat(&self->sgn, "Sgn");
	M3DTabla_ReadFloat(&self->rot.t[0], "QRotX");
	M3DTabla_ReadFloat(&self->rot.t[2], "QRotY");
	M3DTabla_ReadFloat(&self->rot.t[1], "QRotZ");
	M3DTabla_ReadFloat(&self->rot.t[3], "QRotA");
	M3DTabla_ReadFloat(&self->rotEsc.t[0], "URotX");
	M3DTabla_ReadFloat(&self->rotEsc.t[2], "URotY");
	M3DTabla_ReadFloat(&self->rotEsc.t[1], "URotZ");
	M3DTabla_ReadFloat(&self->rotEsc.t[3], "URotA");
	M3DTabla_ReadFloat(&self->esc.t[0], "EscX");
	M3DTabla_ReadFloat(&self->esc.t[2], "EscY");
	M3DTabla_ReadFloat(&self->esc.t[1], "EscZ");
	for (int i = 0; i < g_M3DFrames; i++) {
		M3DTabla4_RotToQuat(&self->rot, (uint32_t)i);
		M3DTabla4_RotToQuat(&self->rotEsc, (uint32_t)i);
	}
}


/* the nodes */

void Nodo_FlagNombre(CNodo3D *nodo, const char *on, const char *off, uint32_t flag) {
	if (on && strstr(nodo->name, on)) {
		nodo->flags |= flag;
		return;
	}
	if (off && strstr(nodo->name, off)) {
		nodo->flags &= ~flag;
		return;
	}
	if (nodo->padre && nodo->padre->flags & flag) {
		nodo->flags |= flag;
	}
}

void Escena_FlagsNombres(CEscena *esc) {
	static const struct {
		const char *on, *off;
		uint32_t flag;
	} tab[8] = { { "(+collide)", "(-collide)", 0x1000 }, { "(+no_collide)", "(-no_collide)", 0x2000 },
		{ "(+rt_lighting)", "(-rt_lighting)", 0x2000000 }, { "(+rt_shadows)", "(-rt_shadows)", 0x4000000 },
		{ "(+no_lightmap)", "(-no_lightmap)", 0x8000000 }, { "(+no_sector)", "(-no_sector)", 0x10000000 },
		{ "(+area_light)", "(-area_light)", 0x20000000 }, { "(+dudv_pass)", "(-dudv_pass)", 0x40000000 } };
	char tipo[32];

	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		for (int i = 0; i < 8; i++) {
			Nodo_FlagNombre(n, tab[i].on, tab[i].off, tab[i].flag);
		}
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (strstr(n->name, "(+ilum_dinam)")) {
			n->flags |= 0x2000000;
		}
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (!n->ini) {
			continue;
		}
		if (CArchINI_GetInt(n->ini, "Animation", "slerp", 1) == 2) {
			n->flags |= NODO_SLERP2;
		}
		if (!CArchINI_GetText(n->ini, "NodeEffect", "Type", tipo, sizeof(tipo), NULL)) {
			continue;
		}
		n->flags |= NODO_FX;
		if (!g_EscenaLODPadre) {
			n->obj58 = NodeFX_New((int)(uintptr_t)tipo, n);
		} else {
			E3D_LogWhite(g_FileLog, "INFO: LOD Node effect skipped in node: %s", n->name);
		}
	}
}

void Escena_FlagsIni(CEscena *esc) {
	if (g_M3DVersion <= 12) {
		return;
	}
	CArchINI *ini = g_EscenaLODPadre ? g_EscenaLODPadre->ini : esc->ini;
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (!ini) {
			continue;
		}
		if (CArchINI_GetInt(ini, "SlerpNodes", n->name, 1) == 2) {
			E3D_LogInfo(g_FileLog, "INFO: slerp2 node: %s", n->name);
			n->flags |= NODO_SLERP2;
		}
		if (CArchINI_GetInt(ini, "MorphNodes", n->name, 0) && !g_EscenaLODPadre) {
			E3D_LogInfo(g_FileLog, "INFO: morph node: %s", n->name);
			n->flags |= 0x800000;
		}
		if (CArchINI_GetInt(ini, "HideNodes", n->name, 0)) {
			E3D_LogInfo(g_FileLog, "INFO: hidding node: %s", n->name);
			n->flags |= NODO_BORRAR;
		}
		if (CArchINI_GetInt(ini, "UnhideNodes", n->name, 0)) {
			E3D_LogInfo(g_FileLog, "INFO: unhidding node: %s", n->name);
			n->flags &= ~NODO_BORRAR;
		}
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (n->indice < 0) {
			continue;
		}
		M3DNodo *dat = M3D_NodoDat(n);
		if (!dat->skin.n || CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
			continue;
		}
		for (int i = 0; i < dat->skin.n; i++) {
			TABLA_CHECK(dat->skin.p, i, dat->skin.n);
			const M3DSkinVert *sv = (const M3DSkinVert *)dat->skin.p + i;
			for (int k = 0; k < sv->NumBones; k++) {
				CNodo3D *bone = CNodo3D_Find(esc->Nodo3D, sv->bone[k], 1);
				CHECK(bone && (uint32_t)bone->indice < 256);
				if (bone->flags & NODO_BORRAR) {
					Con_Warnf("WARN: Hidden bone: %s [unhidden]\n", bone->name);
					bone->flags &= ~NODO_BORRAR;
				}
			}
		}
	}
}

CNodo3D *Nodo_BuscarDescendiente(CNodo3D *nodo, const CNodo3D *antecesor) {
	for (; nodo; nodo = nodo->sig) {
		if (nodo->flags & NODO_BORRAR) {
			continue;
		}
		CHECK(antecesor);
		for (const CNodo3D *p = nodo->padre; p; p = p->padre) {
			if (p == antecesor) {
				return nodo;
			}
			if (p->flags & NODO_BORRAR) {
				break;
			}
		}
	}
	return NULL;
}

CNodo3D *Nodo_BuscarDescendienteAnt(CNodo3D *nodo, const CNodo3D *antecesor) {
	CHECK(nodo && antecesor);
	for (CNodo3D *n = nodo->ant; n; n = n->ant) {
		if (n->flags & NODO_BORRAR) {
			continue;
		}
		CHECK(antecesor);
		for (const CNodo3D *p = n->padre; p; p = p->padre) {
			if (p == antecesor) {
				return n;
			}
		}
	}
	return NULL;
}

CNodo3D *Escena_BuscarColapsable(CNodo3D *nodo, const CNodo3D *padre) {
	CArchINI *ini = g_EscenaLODPadre ? g_EscenaLODPadre->ini : nodo->escena->ini;
	CNodo3D *res = NULL;

	for (; nodo; nodo = nodo->sig) {
		if (nodo->padre != padre || CNodo3D_Clase(nodo) != CLASE_OBJ3D_MALLATRI || nodo->flags & 0xbd1088) {
			continue;
		}
		if (nodo->Refs - (nodo->ant != NULL) - (nodo->sig != NULL) || nodo->ini) {
			continue;
		}
		if (ini && CArchINI_GetInt(ini, "SuperNodes", nodo->name, 0)) {
			continue;
		}
		if (!g_EscenaLODPadre) {
			return nodo;
		}
		res = nodo;
		if (!CEscena_FindNodo(g_EscenaLODPadre, nodo->name)) {
			return nodo;
		}
	}
	return res;
}

void Escena_MatsUsados(CEscena *esc) {
	CTabla *mats = &esc->mats;
	int incluidos = 0;

	CHECK(!mats->p);
	mats->n = M3D_MAX_MAT_ESCENA;
	mats->p = New_Array(M3D_MAX_MAT_ESCENA, sizeof(CMaterial *));
	CHECK(mats->p);
	for (int i = 0; i < g_M3DNumMat; i++) {
		CMaterial *mat = g_M3DMat[i];
		if (!g_M3DMatUsado[i]) {
			const char *tipo = mat->params.i2c == 1 ? "multimat" : "material";
			E3D_LogDebug(g_FileLog, "WARN: Unused %s: [%s] [%s]", tipo, mat->Nombre, CMaterial_GetMapName(mat, 0));
			continue;
		}
		mat = CListaMat_Extract(g_M3DLMat, mat, 1);
		if (!mat) {
			continue;
		}
		CHECK(!mat->sub);
		CHECK(incluidos < M3D_MAX_MAT_ESCENA);
		mat->params.atrib |= MAT_NOSHARE;
		CListaMat_Add(g_ListaMat, mat);
		TABLA_CHECK(mats->p, incluidos, mats->n);
		((CMaterial **)mats->p)[incluidos] = mat;
		incluidos++;
	}
	if (g_M3DLMat) {
		E3D_LogWhite(g_FileLog, "Scene materials: %d included, %d deleted", incluidos, CListaMat_Count(g_M3DLMat, 1));
		if (g_M3DLMat) {
			((objDelete_t)g_M3DLMat->vftable[0])(g_M3DLMat, 1);
			g_M3DLMat = NULL;
		}
	}
	Tabla_Resize4(mats, incluidos);
}


/* the objects: cameras, lights, portals (anim: the values of a frame of an animation file) */

/* the target name ("NombreObjetivo:") of a camera or light into the reader data */
static void M3D_ReadObjetivo(CNodo3D *nodo) {
	M3D_ReadString("NombreObjetivo:", NULL, 0, 1);
	Nodo_LimpiarNombre(g_M3DCad);
	Cad_Set(&M3D_NodoDat(nodo)->objetivo, g_M3DCad);
}

void M3D_LeeCamara(CNodo3D *nodo, byte anim, int frame) {
	if (anim) {
		CHECK(CNodo3D_Clase(nodo) == CLASE_OBJ3D_CAMARA);
		float fov = M3D_ReadFloat("FOV:");
		M3DNodo *dat = M3D_NodoDat(nodo);
		CHECK(dat->AnimNodo);
		CAnimNodo *an = dat->AnimNodo;
		int i = CAnimNodo_FrameIndex(an, frame, an->FOV, ANIM_FOV);
		an->FOV[i] = fov;
		return;
	}
	CCamara *cam = operator_new(sizeof(CCamara));
	cam = cam ? CCamara_ctor(cam) : NULL;
	CNodo3D_SetObj3D(nodo, (IObjeto3D *)cam);
	vec3_t dir = { 0.0f, -1.0f, 0.0f };
	matrix_t m;
	Quat_ToMatrixD3D(&m, &nodo->rot);
	Vec3_TransformDir0(&dir, &dir, &m);
	CCamara_SetNodo(cam, (CNodoRef *)nodo);
	cam->pos = nodo->pos;
	cam->target.x = dir.x + nodo->pos.x;
	cam->target.y = nodo->pos.y + dir.y;
	cam->target.z = nodo->pos.z + dir.z;
	if (M3D_ReadBool("TieneObjetivo:")) {
		M3D_ReadObjetivo(nodo);
	}
	cam->fov = M3D_ReadFloat("FOV:");
	if (M3D_ReadBool("InfoRecorte:")) {
		cam->zNear = M3D_ReadFloat("RecorteCercano:");
		cam->zFar = M3D_ReadFloat("RecorteLejano:");
	}
	cam->f34 = M3D_ReadFloat("RangoCerca:");
	cam->f38 = M3D_ReadFloat("RangoLejos:");
	M3D_ReadFloat("DistObjetivo:");
	if ((double)(cam->zFar / cam->zNear) > 1000000.0) {
		E3D_LogDebug(g_FileLog, "WARN: Invalid camera clipping range: %s", nodo->name);
		cam->zNear = 1.0f;
		cam->zFar = 100000.0f;
	}
}

void M3D_LeeLuz(CNodo3D *nodo, byte anim, int frame) {
	vec3_t c;

	if (anim) {
		CHECK(CNodo3D_Clase(nodo) == CLASE_OBJ3D_LUZ);
		M3D_ReadVec3("Color:", &c, 0);
		float intens = M3D_ReadFloat("Intens:");
		M3DNodo *dat = M3D_NodoDat(nodo);
		CHECK(dat->AnimNodo);
		CAnimNodo *an = dat->AnimNodo;
		uint32_t color = M3D_Color(&c);
		int i = CAnimNodo_FrameIndex(an, frame, an->Color, ANIM_COLOR);
		((uint32_t *)an->Color)[i] = color;
		an = dat->AnimNodo;
		i = CAnimNodo_FrameIndex(an, frame, an->Intens, ANIM_INTENS);
		an->Intens[i] = intens;
		return;
	}
	CLuz *luz = operator_new(sizeof(CLuz));
	luz = luz ? CLuz_ctor(luz) : NULL;
	CNodo3D_SetObj3D(nodo, (IObjeto3D *)luz);
	M3DNodo *dat = M3D_NodoDat(nodo);
	CLuz_SetNodo(luz, nodo);
	luz->pos = nodo->pos;
	Vec3_TransformDir0(&luz->dir, &luz->dir, &dat->mNodo);
	M3D_ReadString("TipoLuz:", NULL, 0, 0);
	if (!strcmp(g_M3DCad, "omni")) {
		luz->tipo = LUZ_OMNI;
	} else if (!strcmp(g_M3DCad, "tspot")) {
		luz->tipo = LUZ_FOCO;
	} else if (!strcmp(g_M3DCad, "dir")) {
		luz->tipo = LUZ_DIREC;
	} else if (!strcmp(g_M3DCad, "fspot")) {
		luz->tipo = LUZ_FOCO;
	} else if (!strcmp(g_M3DCad, "tdir")) {
		luz->tipo = LUZ_DIREC;
	} else {
		Err_Printf(1, "Tipo de luz no v\xe1lido: '%s'", g_M3DCad);
	}
	if (M3D_ReadBool("TieneObjetivo:")) {
		M3D_ReadObjetivo(nodo);
	}
	M3D_ReadString("Sombra:", NULL, 0, 0);
	luz->sombra = strcmp(g_M3DCad, "Off") != 0;
	CLuz_SetActiva(luz, M3D_ReadBool("Activa:"));
	M3D_ReadBool("Rebasar:");
	M3D_ReadVec3("Color:", &c, 0);
	uint32_t color = M3D_Color(&c);
	float intens = M3D_ReadFloat("Intens:");
	*(uint32_t *)luz->color = color;
	luz->intensidad = intens;
	if (luz->tipo != LUZ_OMNI) {
		luz->theta = M3D_ReadFloat("Hotspot:");
		luz->phi = M3D_ReadFloat("Falloff:");
	}
	if (M3D_ReadBool("Atenuacion:")) {
		float esc = (dat->mt.esc.y + dat->mt.esc.x + dat->mt.esc.z) * 0.33333334f;
		luz->f30 = M3D_ReadFloat("AtenIni:") * esc;
		luz->rango = M3D_ReadFloat("AtenFin:") * esc;
	}
	int v;
	if (Nodo_ParamInt(nodo->name, "mult", &v)) {
		luz->mult = (float)v / 100.0f;
	}
	if (Nodo_ParamInt(nodo->name, "radcoef", &v)) {
		luz->f44 = (float)v / 100.0f;
	}
	if (luz->tipo != LUZ_DIREC && 0.0f >= luz->rango) {
		Con_Warnf("  WARN: No End value in Far Attenuation [Light: %s]\n", nodo->name);
	}
}

void M3D_LeePortal(CNodo3D *nodo, byte anim) {
	if (anim) {
		return;
	}
	CPortal *p = operator_new(sizeof(CPortal));
	p = p ? CPortal_ctor(p) : NULL;
	CNodo3D_SetObj3D(nodo, (IObjeto3D *)p);
	M3DNodo *dat = M3D_NodoDat(nodo);
	CObjBase3D_SetNodo((IObjeto3D *)p, nodo);
	CHECK(M3D_ReadInt("Version:") == 1);
	p->ancho = M3D_ReadFloat("Ancho:");
	p->alto = M3D_ReadFloat("Alto:");
	vec3_t v = { p->ancho, 0.0f, 0.0f }, t;
	Vec3_TransformDir0(&t, &v, &dat->mNodo);
	p->ancho = (float)__builtin_sqrt((double)(t.y * t.y + t.x * t.x + t.z * t.z));
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = p->alto;
	Vec3_TransformDir0(&t, &v, &dat->mNodo);
	p->alto = (float)__builtin_sqrt((double)(t.y * t.y + t.x * t.x + t.z * t.z));
	M3D_ReadString("Lado1:", NULL, 0, 1);
	Nodo_LimpiarNombre(g_M3DCad);
	Cad_Set(&M3D_NodoDat(nodo)->objetivo, g_M3DCad);
	M3D_ReadString("Lado2:", NULL, 0, 1);
	Nodo_LimpiarNombre(g_M3DCad);
	Cad_Set(&M3D_NodoDat(nodo)->lado2, g_M3DCad);
	if (0.0f >= p->ancho || 0.0f >= p->alto) {
		M3D_SetOk(0);
		E3D_LogBlue(g_FileLog, "  WARN: Portal: '%s'  Width: %f  Height: %f", nodo->name, (double)p->ancho,
			(double)p->alto);
	}
}


/* the materials */

/* the texture map tipos of the "Mapa:" names */
enum {
	M3D_MAPA_DIFFUSE = 100,
	M3D_MAPA_OPACITY = 101,
	M3D_MAPA_REFLECTION = 102,
	M3D_MAPA_BUMP = 103,
	M3D_MAPA_SELFILLUM = 104,
	M3D_MAPA_SPECLEVEL = 105
};

/* the map idmapa of mat set to name: CHECK it has none, and the switches of the ini and the video (MAT_SHADER: all) */
static CMapa *M3D_SetMapa(CMaterial *mat, uint32_t idmapa, const char *name) {
	CHECK(!CMaterial_GetMapName(mat, idmapa));
	return CMaterial_SetMap(mat, idmapa, name);
}

void M3D_LeeMapa(CMaterial *mat, int iMat, int iSubmat, int idx, int tipo) {
	char buf[0x100], nombre[0x100], clase[0x100];
	float cantidad = 1.0f;

	(void)idx;
	CHECK(mat);
	M3D_ReadString("Mapa:", buf, sizeof(buf), 0);
	if (!strcmp(buf, "nulo") || !strcmp(buf, "otro")) {
		return;
	}
	if (!strcmp(buf, "diffuse")) {
		tipo = M3D_MAPA_DIFFUSE;
	} else if (!strcmp(buf, "opacity")) {
		tipo = M3D_MAPA_OPACITY;
	} else if (!strcmp(buf, "reflection")) {
		tipo = M3D_MAPA_REFLECTION;
	} else if (!strcmp(buf, "bump")) {
		tipo = M3D_MAPA_BUMP;
	} else if (!strcmp(buf, "selfillum")) {
		tipo = M3D_MAPA_SELFILLUM;
	} else if (!strcmp(buf, "speclevel")) {
		tipo = M3D_MAPA_SPECLEVEL;
	} else if (strcmp(buf, "TintRGB") && strcmp(buf, "Mascara")) {
		Err_Printf(1, "Invalid texture map class: '%s'", buf);
		CHECK(0);
	}
	M3D_ReadString("Nombre:", nombre, sizeof(nombre), 1);
	M3D_ReadString("Clase:", clase, sizeof(clase), 1);
	if (g_M3DVersion > 8) {
		cantidad = M3D_ReadFloat("Cantidad:");
	}
	if (!strcmp(clase, "Bitmap")) {
		CMapa *mapa = NULL;
		M3D_ReadString("Bitmap:", buf, sizeof(buf), 1);
		Ruta_Part(buf, buf, RUTA_NAME | RUTA_EXT);
		if (buf[0] == '$') {
			Ruta_Merge(buf, ".bmp", buf + 1);
			Ruta_Merge(buf, g_SharedTexturesPath, buf);
		} else {
			Ruta_Merge(buf, ".bmp", buf);
			Ruta_Merge(buf, g_M3DTexturePath, buf);
		}
		byte shader = (mat->params.atrib & MAT_SHADER) != 0;
		switch (tipo) {
		case M3D_MAPA_DIFFUSE:
			mapa = M3D_SetMapa(mat, MAPA_DIFFUSE, buf);
			break;
		case M3D_MAPA_OPACITY:
			if (shader || g_M3DUseOpacity) {
				mapa = M3D_SetMapa(mat, MAPA_OPACITY, buf);
			}
			break;
		case M3D_MAPA_REFLECTION:
			if (shader || (g_UsarEnvMap && g_M3DUseEnvmap)) {
				CHECK(!CMaterial_GetMapName(mat, MAPA_ENTORNO));
				mapa = CMaterial_SetMap(mat, MAPA_ENTORNO, buf);
				mat->params.b3b = buf[0] != 0;
				if (strstr(nombre, "(env2)")) {
					mapa->bb = 1;
				}
			}
			break;
		case M3D_MAPA_BUMP:
			if (shader || (g_UsarBumpMap && g_M3DUseBumpmap)) {
				mapa = M3D_SetMapa(mat, MAPA_BUMP, buf);
			}
			break;
		case M3D_MAPA_SELFILLUM:
			if (shader || (g_UsarGlowMap && g_M3DUseGlowmap)) {
				mapa = M3D_SetMapa(mat, MAPA_GLOW, buf);
			}
			break;
		case M3D_MAPA_SPECLEVEL:
			CHECK(0);
			mapa = M3D_SetMapa(mat, MAPA_OPACITY, buf);
			break;
		}
		if (mapa) {
			mapa->f2c = cantidad;
			mapa->fx = FXTextura_Tipo(nombre);
		}
		if (g_M3DVersion > 8) {
			byte tu = M3D_ReadBool("TileU:");
			byte tv = M3D_ReadBool("TileV:");
			byte eu = M3D_ReadBool("EspejoU:");
			byte ev = M3D_ReadBool("EspejoV:");
			if (mapa) {
				mapa->bc = (tv ? 2 : 0) | (tu != 0);
				mapa->bd = (ev ? 2 : 0) | (eu != 0);
			}
		}
		if (g_M3DVersion > 7) {
			if (M3D_ReadBool("UVGen:")) {
				M3D_ReadString("Tipo:", buf, sizeof(buf), 0);
				float du = M3D_ReadFloat("DesplazU:");
				float dv = M3D_ReadFloat("DesplazV:");
				float su = M3D_ReadFloat("EscalaU:");
				float sv = M3D_ReadFloat("EscalaV:");
				float ang = M3D_ReadFloat("Angulo:");
				if (mapa) {
					mapa->f10[0] = du;
					mapa->f10[1] = dv;
					mapa->f18[0] = su;
					mapa->f18[1] = sv;
					mapa->v20[0] = 0.0f;
					mapa->v20[1] = 0.0f;
					mapa->v20[2] = ang;
				}
			}
			M3D_ReadBool("TexInvert:");
			M3D_ReadString("Filter:", buf, sizeof(buf), 0);
			if (mapa && !strcmp(buf, "no")) {
				mapa->b8 = 0;
			}
		}
	} else if (!strcmp(clase, "Mask")) {
		CHECK(tipo == M3D_MAPA_REFLECTION);
		Err_Printf(1, "El tipo de mapa 'Mask' est\xe1 sin terminar");
	}
	int n = M3D_ReadInt("NumSubTexmaps:");
	for (int i = 0; i < n; i++) {
		M3D_LeeMapa(mat, iMat, iSubmat, i, tipo);
	}
}

void M3D_LeeMaterial(CListaMat *lista, int iMat, int iSubmat) {
	char buf[0x100];

	CHECK(lista);
	CHECK(iMat >= 0 && iMat <= 0xffff);
	if (iSubmat == -1) {
		CHECK(M3D_ReadInt("Material:") == iMat);
	} else {
		CHECK(iSubmat == -1 || (iSubmat >= 0 && iSubmat <= 0xffff));
		M3D_ReadInt("Submaterial:");
	}
	if (!M3D_ReadBool("Valido:")) {
		return;
	}
	CMaterial *mat = operator_new(sizeof(CMaterial));
	mat = mat ? CMaterial_ctor(mat) : NULL;
	CListaMat_Add(lista, mat);
	CHECK(g_M3DNumMat < M3D_MAX_MAT);
	uint32_t n = (uint32_t)g_M3DNumMat;
	g_M3DMat[n] = mat;
	g_M3DMatId[n] = iMat;
	g_M3DMatSub[n] = iSubmat;
	if (n >= M3D_MAX_MAT) {
		crt_report_rangecheckfailure();
	}
	g_M3DMatUsado[n] = 0;
	g_M3DNumMat = (int)n + 1;
	M3D_ReadString("Nombre:", buf, sizeof(buf), 1);
	Cad_Set(&mat->Nombre, buf);
	crt_strlwr(buf);
	static const struct {
		const char *s;
		uint16_t atrib;
	} flags[12] = { { "(nc)", 0x1 }, { "(+no_collide)", 0x1 }, { "(agua)", 0x2 }, { "(niebla)", 0x4 },
		{ "(+shader:clouds)", 0x4 }, { "(+sort_zbias)", 0x10 }, { "(+transp_before)", 0x10 },
		{ "(+alpha_test)", 0x20 }, { "(+area_light:", 0x100 }, { "(+shader:", MAT_SHADER }, { "(+zbias)", 0x800 },
		{ "(+xshadow)", 0x4000 } };
	for (int i = 0; i < 12; i++) {
		if (strstr(buf, flags[i].s)) {
			mat->params.atrib |= flags[i].atrib;
		}
	}
	int transp = strstr(buf, "(tf)") ? 1 : 0;
	if (strstr(buf, "(+transp:filter)")) {
		transp = 1;
	}
	if (strstr(buf, "(+transp:premult)")) {
		transp = 2;
	}
	if (strstr(buf, "(+transp:oneone)")) {
		transp = 3;
	}
	if (strstr(buf, "(+dudv_pass)")) {
		transp = 10;
	}
	byte texsi = strstr(buf, "(texsi)") ? 1 : 0;
	M3D_ReadString("Clase:", buf, sizeof(buf), 1);
	int clase = 2;
	if (!strcmp(buf, "Standard")) {
		clase = 0;
	}
	if (!strcmp(buf, "Multi/Sub-Object")) {
		clase = 1;
	}
	mat->params.i2c = clase;
	M3D_ReadColores(mat);
	if (!clase) {
		M3D_ReadString("Shading:", NULL, 0, 0);
		float si = M3D_ReadFloat("SelfIllum:");
		byte b = M3D_ColorByte(si);
		mat->params.emissive[3] = 0;
		mat->params.emissive[0] = b;
		mat->params.emissive[1] = b;
		mat->params.emissive[2] = b;
		mat->params.b38 = M3D_ReadBool("TwoSided:");
		M3D_ReadBool("FaceMap:");
		M3D_ReadString("TransparencyType:", NULL, 0, 0);
		if (!strcmp(g_M3DCad, "Additive")) {
			mat->params.srcBlend = 5;
			mat->params.dstBlend = 2;
		}
		if (transp == 1) {
			mat->params.srcBlend = 1;
			mat->params.dstBlend = 3;
		} else if (transp == 2) {
			mat->params.dstBlend = 6;
			mat->params.srcBlend = 2;
		} else if (transp == 3) {
			mat->params.dstBlend = 2;
			mat->params.atrib |= MAT_1000;
			mat->params.diffuse[3] = 0;
			mat->params.srcBlend = 2;
		}
	}
	int num = M3D_ReadInt("Mapas:");
	for (int i = 0; i < num; i++) {
		M3D_LeeMapa(mat, iMat, iSubmat, i, 0);
	}
	num = M3D_ReadInt("Submateriales:");
	if (num) {
		CHECK(!mat->sub);
		CListaMat *sub = operator_new(sizeof(CListaMat));
		if (sub) {
			sub->Nombre = NULL;
			sub->vftable = Game_Ptr(CLISTAMAT_VFTABLE);
			sub->ult = NULL;
			sub->pri = NULL;
		}
		mat->sub = sub;
	}
	for (int i = 0; i < num; i++) {
		M3D_LeeMaterial(mat->sub, iMat, iSubmat == -1 ? i : iSubmat);
	}
	if (g_Sobredib) {
		mat->params.srcBlend = 2;
		mat->params.dstBlend = 2;
		mat->params.transp = 1;
		CMaterial_SetMapCopy(mat, 0, NULL);
		CMaterial_SetMapCopy(mat, 1, NULL);
		CMaterial_SetMapCopy(mat, 2, NULL);
		CMaterial_SetMapCopy(mat, 3, NULL);
		*(uint32_t *)mat->params.ambient = 0;
		*(uint32_t *)mat->params.diffuse = 0x301010;
		*(uint32_t *)mat->params.emissive = 0xffffff;
		if (mat->params.b3b) {
			*(uint32_t *)mat->params.diffuse = 0x301060;
		}
		mat->params.b3b = 0;
		mat->params.w3e &= 0xff;
	}
	mat->params.w3e = (uint16_t)((mat->params.w3e & 0xff00) | 1);
	if (Mat_IsTransp(mat)) {
		mat->params.w3e &= 0xff;
	}
	if (!mat->params.transp && mat->mapa[MAPA_OPACITY] && !mat->params.b3b && mat->mapa[MAPA_GLOW]) {
		mat->params.diffuse[3] = 0xff;
		mat->params.transp = 1;
	}
	if (mat->params.transp && mat->params.b3b && mat->params.srcBlend == 5) {
		E3D_LogVerbose(g_FileLog, "WARN: transparent environment mapped material without (+transp:premult)  [%s]",
			mat->Nombre);
		mat->params.srcBlend = 2;
		if (!(mat->params.atrib & MAT_SHADER)) {
			float a = (float)(int)mat->params.diffuse[3] / 255.0f;
			byte *col = mat->params.color;
			byte c0 = (byte)(int)((double)(a * ((float)(int)col[0] * 0x1.010102p-8f) * 255.0f) + 0.5);
			byte c1 = (byte)(int)((double)(a * ((float)(int)col[1] * 0x1.010102p-8f) * 255.0f) + 0.5);
			byte c2 = (byte)(int)((double)(a * ((float)(int)col[2] * 0x1.010102p-8f) * 255.0f) + 0.5);
			col[0] = c0;
			col[1] = c1;
			col[2] = c2;
		}
	}
	if (transp == 10) {
		byte e = mat->params.emissive[0];
		mat->params.srcBlend = 2;
		mat->params.dstBlend = 2;
		mat->params.transp = 1;
		mat->params.w3e = 0;
		mat->params.diffuse[3] = e;
	}
	if (!g_UsarTexturas && !texsi && mat->mapa[MAPA_DIFFUSE] && mat->mapa[MAPA_DIFFUSE]->b8 == 1) {
		CMaterial_MapToColor(mat);
	}
}
