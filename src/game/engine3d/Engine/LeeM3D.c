/*
 * The reader of the M3D files and its value readers, original source Engine/lee_M3D.cpp (include/scrap/leem3d.h).
 */
#include <string.h>

#include "scrap/camara.h"
#include "scrap/console.h"
#include "scrap/animgrupo.h"
#include "scrap/animvert.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/flare.h"
#include "scrap/fxtextura.h"
#include "scrap/grafico3d.h"
#include "scrap/leedatos.h"
#include "scrap/leem3d.h"
#include "scrap/luz.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/nodefx.h"
#include "scrap/portal.h"
#include "scrap/render.h"
#include "scrap/scrap.h"
#include "scrap/sistpart.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"

/* the constructors and destructors of the reader data, passed to the new[] and delete[] loops of the CRT */
#define M3DTABLA_CTOR 0x2bba90
#define M3DTABLA_DTOR 0x2bbd20		/* Tabla_Free */
#define CAD_CTOR 0x2bbab0
#define CAD_DTOR 0x233be0		/* the string freed (Material.cpp) */
#define M3DSKINVERT_DTOR 0x2bc160
#define M3DNODO_DTOR 0x2bbf10

static void M3D_CadFree(char **s) {
	if (*s) {
		Cad_Set(s, NULL);
	}
}

M3DTabla *__fastcall M3DTabla_ctor(M3DTabla *self) {
	self->p = NULL;
	self->n = 0;
	self->fija = 0;
	return self;
}

char **__fastcall Cad_ctor(char **self) {
	*self = NULL;
	return self;
}

M3DPista *__fastcall M3DPista_ctor(M3DPista *self) {
	crt_eh_vector_ctor(&self->pos, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_CTOR), Game_Ptr(M3DTABLA_DTOR));
	self->sgn.p = NULL;
	self->sgn.n = 0;
	self->sgn.fija = 0;
	crt_eh_vector_ctor(&self->rot, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_CTOR), Game_Ptr(M3DTABLA_DTOR));
	crt_eh_vector_ctor(&self->rotEsc, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_CTOR), Game_Ptr(M3DTABLA_DTOR));
	crt_eh_vector_ctor(&self->esc, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_CTOR), Game_Ptr(M3DTABLA_DTOR));
	return self;
}

M3DNodo *__fastcall M3DNodo_ctor(M3DNodo *self) {
	self->objetivo = NULL;
	self->lado2 = NULL;
	self->skin.p = NULL;
	self->skin.n = 0;
	M3DPista_ctor(&self->pista);
	M3DPista_ctor(&self->pista2);
	for (int i = 0; i < 6; i++) {
		self->t2a4[i].p = NULL;
		self->t2a4[i].n = 0;
		self->t2a4[i].fija = 0;
	}
	return self;
}

M3DSkinVert *__fastcall M3DSkinVert_ctor(M3DSkinVert *self) {
	crt_eh_vector_ctor(self->bone, sizeof(char *), SKIN_MAX_BONES, Game_Ptr(CAD_CTOR), Game_Ptr(CAD_DTOR));
	return self;
}

void __fastcall M3DSkinVert_dtor(M3DSkinVert *self) {
	crt_eh_vector_dtor(self->bone, sizeof(char *), SKIN_MAX_BONES, Game_Ptr(CAD_DTOR));
}

void __fastcall M3DTabla4_dtor(M3DTabla4 *self) {
	crt_eh_vector_dtor(self->t, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_DTOR));
}

void __fastcall M3DPista_dtor(M3DPista *self) {
	crt_eh_vector_dtor(&self->esc, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_DTOR));
	crt_eh_vector_dtor(&self->rotEsc, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_DTOR));
	crt_eh_vector_dtor(&self->rot, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_DTOR));
	Tabla_Free((CTabla *)&self->sgn);
	crt_eh_vector_dtor(&self->pos, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_DTOR));
}

void __fastcall M3DTabla6_dtor(M3DTabla *self) {
	for (int i = 5; i >= 0; i--) {
		Tabla_Free((CTabla *)&self[i]);
	}
}

void __fastcall M3DSkin_Free(CTabla *self) {
	if (self->p) {
		Delete_Objects(self->p, sizeof(M3DSkinVert), M3DSKINVERT_DTOR);
		self->p = NULL;
		self->n = 0;
	}
}

void __fastcall M3DNodo_dtor(M3DNodo *self) {
	M3DTabla6_dtor(self->t2a4);
	M3DPista_dtor(&self->pista2);
	M3DPista_dtor(&self->pista);
	M3DSkin_Free(&self->skin);
	M3D_CadFree(&self->lado2);
	M3D_CadFree(&self->objetivo);
}

/* the deleting destructors: flags 2 the array with its count before it, 1 the memory freed */
M3DNodo *__thiscall M3DNodo_Delete(M3DNodo *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(M3DNodo), *mem, Game_Ptr(M3DNODO_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (M3DNodo *)mem;
	}
	M3DNodo_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(M3DNodo));
	}
	return self;
}

M3DSkinVert *__thiscall M3DSkinVert_Delete(M3DSkinVert *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(M3DSkinVert), *mem, Game_Ptr(M3DSKINVERT_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (M3DSkinVert *)mem;
	}
	M3DSkinVert_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(M3DSkinVert));
	}
	return self;
}

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
			n->obj58 = NodeFX_New(tipo, n);
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


/* the scene after the objects are read */

/* CHECK of a decomposed node matrix: no scale, no mirror */
static void M3D_CheckSinEscala(const m3dTransform_t *da) {
	CHECK((double)1e-5f > __builtin_fabs((double)(da->esc.x - 1.0f)) &&
		(double)1e-5f > __builtin_fabs((double)(da->esc.y - 1.0f)) &&
		(double)1e-5f > __builtin_fabs((double)(da->esc.z - 1.0f)));
	CHECK(da->signo == 1.0f);
}

matrix_t *__thiscall M3DNodo_CalcMatrix(M3DNodo *self) {
	return M3DTransform_ToMatrixAnim(&self->mt, &self->mNodo);
}

int M3D_NextLine(void) {
	return g_M3DBinary ? 0 : Lee_NextLine();
}

void Escena_MatricesObj(CEscena *esc) {
	m3dTransform_t da;
	matrix_t inv;

	CEscena_UpdateNodes(esc);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		Matrix_ToTransform(&n->m, &da);
		M3D_CheckSinEscala(&da);
		n->qLook = da.rot;
		CHECK(Matrix_Invert(&inv, &n->m) == 0);
		n->mObj = inv;
	}
}

void Escena_AnimLocal(CEscena *esc, byte anim) {
	m3dTransform_t da;
	matrix_t inv, tmp, loc, mp, mn;
	vec3_t v, pos;
	quat_t q, rot;

	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		CNodo3D *padre = n->padre;
		CNodo3D_SetPadre(n, NULL);
		CNodo3D_Update(n, NULL);
		CNodo3D_SetPadre(n, padre);
		if (!padre) {
			continue;
		}
		CHECK(Matrix_Invert(&inv, &padre->m) == 0);
		loc = *Matrix_MulD3D(&tmp, &n->m, &inv);
		Matrix_ToTransform(&loc, &da);
		n->pos = da.pos;
		n->rot = da.rot;
		M3D_CheckSinEscala(&da);
		CNodo3D_Update(n, NULL);
		if (!anim) {
			continue;
		}
		M3DNodo *dat = M3D_NodoDat(n);
		CHECK(dat->AnimNodo);
		uint32_t flags = CAnimNodo_GetFlags(dat->AnimNodo);
		if (padre->indice >= 0) {
			CAnimNodo_GetFlags(M3D_NodoDat(padre)->AnimNodo);
		}
		for (int frame = g_M3DPriFrame; frame <= g_M3DUltFrame; frame++) {
			uint32_t i = (uint32_t)(frame - g_M3DPriFrame);
			if (padre->indice == -1) {
				mp = padre->m;
			} else {
				pos = *M3DTabla4_GetVec(&M3D_NodoDat(padre)->pista.pos, &v, i);
				rot = *M3DTabla4_GetQuat(&M3D_NodoDat(padre)->pista.rot, &q, i);
				Quat_ToMatrixD3D(&mp, &rot);
				Matrix_AddPos(&mp, &pos);
			}
			pos = *M3DTabla4_GetVec(&dat->pista.pos, &v, i);
			rot = *M3DTabla4_GetQuat(&dat->pista.rot, &q, i);
			Quat_ToMatrixD3D(&mn, &rot);
			Matrix_AddPos(&mn, &pos);
			CHECK(Matrix_Invert(&inv, &mp) == 0);
			loc = *Matrix_MulD3D(&tmp, &mn, &inv);
			Matrix_ToTransform(&loc, &da);
			if (flags & ANIM_POS) {
				CAnimNodo *an = dat->AnimNodo;
				int k = CAnimNodo_FrameIndex(an, frame, an->Pos, ANIM_POS);
				an->Pos[k] = da.pos;
			}
			if (flags & ANIM_ROT) {
				CAnimNodo *an = dat->AnimNodo;
				int k = CAnimNodo_FrameIndex(an, frame, an->Rot, ANIM_ROT);
				an->Rot[k] = da.rot;
			}
			M3D_CheckSinEscala(&da);
		}
	}
}

void Escena_ColapsarBip(CEscena *esc) {
	matrix_t tmp, m;

	CEscena_UpdateNodes(esc);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (n->indice < 0 || !(n->flags & NODO_BIP) || n->obj3D) {
			continue;
		}
		CHECK(CNodo3D_Clase(n) == 0);
		CNodo3D *n2 = Escena_BuscarColapsable(esc->Nodo3D, n);
		if (!n2) {
			continue;
		}
		E3D_LogInfo(g_FileLog, "BIP: Colapsed node: %s -> %s [f:%08X]", n2->name, n->name, n2->flags);
		CHECK(!(n->flags & 0xc30000));
		CHECK(!(n2->flags & 0xc30000));
		if (n2->flags & 0xfffffe8b) {
			E3D_LogBlue(g_FileLog, "WARN: Colapsed node: %s [f:%08X]", n2->name, n2->flags);
		}
		CNodo3D_SetObj3D(n, n2->obj3D);
		n->flags &= 0xfffdff77;
		if (n2->flags & 0x20) {
			n->flags |= 0x20;
		} else {
			n->flags &= ~0x20u;
		}
		if (n2->flags & 0x40) {
			n->flags |= 0x40;
		} else {
			n->flags &= ~0x40u;
		}
		n2->obj3D = NULL;
		n2->flags |= 0x420088;
		CMallaTri *malla = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
		if (!malla->pSkin) {
			m = *Matrix_MulD3D(&tmp, &n2->m, &n->mObj);
			CMallaTri_Transform(malla, &m);
		}
	}
}

void Escena_SkinMesh(CEscena *esc) {
	CNodo3D *n = esc->Nodo3D;

	for (; n; n = n->sig) {
		if (n->indice >= 0 && M3D_NodoDat(n)->skin.n) {
			break;
		}
	}
	if (!n) {
		return;
	}
	CMallaTri *todo = operator_new(sizeof(CMallaTri));
	todo = todo ? CMallaTri_ctor(todo) : NULL;
	CEscena_UpdateNodes(esc);
	for (n = esc->Nodo3D; n; n = n->sig) {
		if (n->flags & NODO_HIDE || CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
			continue;
		}
		CMallaTri *malla = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
		if (!malla->pSkin) {
			CMallaTri_Transform(malla, &n->m);
			if (!malla->pSkin) {
				CMallaTri_AllocSkin(malla);
				for (int i = 0; i < malla->nVert; i++) {
					byte *s = CMallaTri_GetSkin(malla, (uint32_t)i);
					s[0] = 1;
					CHECK((uint32_t)n->indice < 256);
					n->flags |= NODO_HUESO;
					s[1] = (byte)n->indice;
					*(float *)(s + 4) = 1.0f;
				}
			}
		}
		CHECK(malla->pTri);
		CMallaTri_Add(todo, malla, 0, 0);
		CNodo3D_FreeObj3D(n);
		if (!(n->flags & NODO_HUESO)) {
			n->flags |= 0x420008;
		}
	}
	if (todo->nTri && todo->nVert) {
		CNodo3D *nodo = operator_new(sizeof(CNodo3D));
		nodo = nodo ? CNodo3D_ctor(nodo) : NULL;
		Cad_Set(&nodo->name, "SkinMesh");
		CNodo3D_SetObj3D(nodo, (IObjeto3D *)todo);
		CEscena_AddNodo(esc, nodo);
		return;
	}
	((objDelete_t)((void **)todo->vftable)[0])(todo, 1);
}

void Escena_TablaNodos(CEscena *esc) {
	int k = 0;

	CEscena_NumNodos(esc);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (!(n->flags & NODO_NO_TABNODOR)) {
			n->i48 = k++;
		}
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (n->flags & NODO_NO_TABNODOR) {
			n->i48 = k++;
		}
	}
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		CHECK(CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D);
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
			continue;
		}
		CMallaTri *malla = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
		if (!malla->pSkin) {
			continue;
		}
		for (int v = 0; v < malla->nVert; v++) {
			byte *s = CMallaTri_GetSkin(malla, (uint32_t)v);
			if (!s[0]) {
				continue;
			}
			byte *p = s + 1;
			do {
				CNodo3D *bone = CEscena_NodoObjeto(esc, *p);
				CHECK(bone && (uint32_t)bone->i48 < 256);
				CHECK(bone->flags & NODO_HUESO);
				*p++ = (byte)bone->i48;
			} while ((int)(p - s - 1) < (int)s[0]);
		}
	}
	if (g_EscenaAnimPadre || g_EscenaLODPadre) {
		return;
	}
	CArchINI *ini = esc->ini;
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		uint32_t mask = 0xffffffff;
		int v = ini ? CArchINI_GetInt(ini, "AnimNodes", n->name, -1) : -1;
		if (g_OptimizeNodes & 4) {
			if (v == 0) {
				mask = 0;
				E3D_LogWhite(g_FileLog, "AniMask: IniValue %d: %s (node animation discarded)", 0, n->name);
			} else if (v > 0) {
				E3D_LogWhite(g_FileLog, "AniMask: IniValue %d: %s (node animation NOT discarded)", v, n->name);
			} else if ((n->flags & 0x600000) == 0x600000) {
				mask = 0;
				E3D_LogInfo(g_FileLog, "AniMask: BIPED+NO_TABNODOR node: %s (node animation discarded)", n->name);
			} else if (!(n->flags & NODO_BIP)) {
				CHECK(CNodo3D_Clase(n) != CLASE_OBJ3D_MALLAD3D);
				if (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI) {
					CMallaTri *malla = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
					if (malla->pSkin) {
						mask = 0;
						if (n->flags & 0x3d1000) {
							Con_Warnf("AniMask: skinned mesh node: %s [f:%08X] (node animation NOT discarded)\n",
								n->name, n->flags);
							mask = 0xffffffff;
						} else if (n->Refs - (n->ant != NULL) - (n->sig != NULL)) {
							E3D_LogInfo(g_FileLog, "AniMask: skinned mesh node: %s (ref-node animation NOT discarded)",
								n->name);
							mask = 0xffffffff;
						} else {
							E3D_LogWhite(g_FileLog, "AniMask: skinned mesh node: %s (node animation discarded)", n->name);
						}
					}
				}
				mask |= 0x80;
			}
		}
		n->i50 = (int)~mask;
	}
}

void Escena_SetupLOD(CEscena *esc) {
	E3D_LogWhite(g_FileLog, "LOD Model setup: %s", Ruta_FileName(esc->name));
	Cad_Set(&esc->mainLOD, g_EscenaLODPadre->name);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		CNodo3D *n2 = CEscena_FindNodo(g_EscenaLODPadre, n->name);
		CArchINI *ini = g_EscenaLODPadre->ini;
		if (ini && CArchINI_GetInt(ini, "XRefNodes", n->name, 0)) {
			CHECK(n2);
			n->nodoR = n2->i48;
		}
		if (n->flags & NODO_FX) {
			CHECK(n2);
			n->nodoR = n2->i48;
		}
		if (n->nodoR >= 0) {
			CHECK(n->padre);
			CHECK(n2->padre);
			CHECK(!(n->flags & 0x420000));
			CHECK(!(n->padre->flags & 0x420000));
			CHECK(!(n2->padre->flags & 0x420000));
			E3D_LogWhite(g_FileLog, "LOD node XRef: %s [f:%08X] [c:%s]", n->name, n->flags,
				Obj3D_ClaseName(CNodo3D_Clase(n)));
		}
		if (CNodo3D_Clase(n) && CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
			Con_Bluef("ERROR: LOD node class is invalid: %s [c:%s]\n", n->name, Obj3D_ClaseName(CNodo3D_Clase(n)));
		}
		if (n->flags & NODO_COLISION) {
			Con_Warnf("WARN: LOD node is invalid: %s [f:%08X]\n", n->name, n->flags);
		}
		if (n->flags & 0x420000) {
			Con_Warnf("WARN: LOD node is invalid: %s [f:%08X]\n", n->name, n->flags);
		}
		n->indice = -1;
		if (!n2) {
			if (Wild_Match("*LOD[1-9]*", n->name)) {
				E3D_LogInfo(g_FileLog, "WARN: LOD node not found in MainLOD [Node: %s]", n->name);
			} else {
				Con_Warnf("WARN: LOD node not found in MainLOD [Node: %s]\n", n->name);
			}
			continue;
		}
		if (CNodo3D_Clase(n2) && CNodo3D_Clase(n2) != CLASE_OBJ3D_MALLAD3D &&
			CNodo3D_Clase(n2) != CLASE_OBJ3D_SISTPART) {
			Con_Bluef("ERROR: LOD main-node class is invalid: %s [c:%s]\n", n2->name,
				Obj3D_ClaseName(CNodo3D_Clase(n2)));
		}
		n->indice = n2->indice;
		if (!n->padre && !n2->padre) {
			continue;
		}
		if (!n->padre || !n2->padre || strcmp(n->padre->name, n2->padre->name)) {
			Con_Warnf("WARN: LOD node parent mismatch [Node: %s]\n", n->name);
		}
		if (!(n2->i50 & 1) && (n->pos.x != n2->pos.x || n->pos.y != n2->pos.y || n->pos.z != n2->pos.z)) {
			if (!((double)0.025f > __builtin_fabs((double)(n->pos.x - n2->pos.x)) &&
				    (double)0.025f > __builtin_fabs((double)(n->pos.y - n2->pos.y)) &&
				    (double)0.025f > __builtin_fabs((double)(n->pos.z - n2->pos.z)))) {
				vec3_t d = { n->pos.x - n2->pos.x, n->pos.y - n2->pos.y, n->pos.z - n2->pos.z };
				Con_Warnf("WARN: LOD node Pos mismatch [Node: %s] (%s)\n", n->name, Vec3_Str(&d, 0, 6));
			}
		}
		if (!(n2->i50 & 2) && (n->rot.x != n2->rot.x || n->rot.y != n2->rot.y || n->rot.z != n2->rot.z ||
					     n->rot.w != n2->rot.w)) {
			double d = Quat_DotAbs(&n->rot, &n2->rot);
			if (Game_Cos(0x1.c98710add7e36p-10) > d) {
				Con_Warnf("WARN: LOD node QRot mismatch [Node: %s]\n", n->name);
			}
		}
	}
}

void Escena_PhysiqueSkin(CEscena *esc) {
	if (g_M3DVersion <= 12) {
		return;
	}
	CEscena_UpdateNodes(esc);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (n->indice < 0) {
			continue;
		}
		M3DNodo *dat = M3D_NodoDat(n);
		if (!dat->skin.n) {
			continue;
		}
		CHECK(CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI);
		if (CNodo3D_Clase(n) != CLASE_OBJ3D_MALLATRI) {
			continue;
		}
		CMallaTri *malla = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLATRI);
		CMallaTri_AllocSkin(malla);
		for (int v = 0; v < malla->nVert; v++) {
			TABLA_CHECK(dat->skin.p, v, dat->skin.n);
			const M3DSkinVert *sv = (const M3DSkinVert *)dat->skin.p + v;
			byte *s = CMallaTri_GetSkin(malla, (uint32_t)v);
			s[0] = (byte)sv->NumBones;
			for (int k = 0; k < sv->NumBones; k++) {
				CNodo3D *bone = CNodo3D_Find(esc->Nodo3D, sv->bone[k], 1);
				if (!bone) {
					Con_FatalErrorf("\nFATAL ERROR: Check failed!\n\nError: %s\n\nExpression: %s\n\nFile: '%s'\nDate: %s\n"
						"Line: %d", sv->bone[k], "bone && \"_bone_not_found_\"", __FILE__, __TIMESTAMP__, __LINE__);
				}
				CHECK(bone && (uint32_t)bone->indice < 256);
				bone->flags |= NODO_HUESO;
				s[k + 1] = (byte)bone->indice;
				memcpy(s + 4 + 4 * k, &sv->peso[k], sizeof(float));
			}
		}
		CMallaTri_Transform(malla, &n->m);
	}
}

/* a node and its parent of the reordering */
typedef struct M3DNodoPadre {
	CNodo3D *nodo;			/* 0x0 */
	CNodo3D *padre;			/* 0x4 */
} M3DNodoPadre;

/* tabla.h at rva 0x2bc1b0 */
static M3DNodoPadre *M3DNodoPadre_Get(CTabla *t, uint32_t i) {
	return Tabla_Get8(t, i);
}

/* the nodes ordered so that the parents come first, when one is not */
static void Escena_OrdenarNodos(CEscena *esc) {
	byte cambio = 0;
	int k = 1;

	CEscena_SetNodos54(esc, 0);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		if (n->padre && !n->padre->i54) {
			cambio = 1;
		}
		n->i54 = k++;
	}
	if (!cambio) {
		return;
	}
	CTabla t = { 0, NULL };
	int num = CEscena_NumNodos(esc);
	CHECK(num > 0);
	t.n = num;
	t.p = New_Array((uint32_t)num, sizeof(M3DNodoPadre));
	CHECK(t.p);
	CNodo3D *n = esc->Nodo3D;
	for (int i = 0; i < num; i++) {
		CHECK(n);
		TABLA_CHECK(t.p, i, num);
		((M3DNodoPadre *)t.p)[i].nodo = n;
		TABLA_CHECK(t.p, i, num);
		((M3DNodoPadre *)t.p)[i].padre = n->padre;
		n = n->sig;
	}
	M3DNodoPadre *tab = t.p;
	do {
		cambio = 0;
		for (int i = 0; i < num; i++) {
			TABLA_CHECK(t.p, i, num);
			CNodo3D *nodo = tab[i].nodo;
			while (tab[i].padre) {
				int j = 0;
				while (j < num && M3DNodoPadre_Get(&t, (uint32_t)j)->nodo != tab[i].padre) {
					j++;
				}
				if (j <= i) {
					break;
				}
				CHECK(M3DNodoPadre_Get(&t, (uint32_t)j)->nodo != nodo);
				M3DNodoPadre a = *M3DNodoPadre_Get(&t, (uint32_t)i);
				M3DNodoPadre b = *M3DNodoPadre_Get(&t, (uint32_t)j);
				*M3DNodoPadre_Get(&t, (uint32_t)i) = b;
				*M3DNodoPadre_Get(&t, (uint32_t)j) = a;
				cambio = 1;
			}
		}
	} while (cambio);
	for (int i = 0; i < num; i++) {
		TABLA_CHECK(t.p, i, num);
		CEscena_RemoveNodo(esc, tab[i].nodo);
	}
	for (int i = 0; i < num; i++) {
		TABLA_CHECK(t.p, i, num);
		CEscena_AddNodo(esc, tab[i].nodo);
		TABLA_CHECK(t.p, i, num);
		TABLA_CHECK(t.p, i, num);
		CNodo3D_SetPadre(tab[i].nodo, tab[i].padre);
	}
	if (t.p) {
		operator_delete_array(t.p);
	}
}

void Escena_Ordenar(CEscena *esc) {
	Escena_OrdenarNodos(esc);
}

void Escena_PadresAnim(CEscena *esc, CEscena *estatica) {
	CHECK(estatica->Nodo3D);
	for (CNodo3D *n = esc->Nodo3D; n; n = n->sig) {
		CNodo3D *n2 = CNodo3D_Find(estatica->Nodo3D, n->name, 1);
		if (!n2) {
			continue;
		}
		CNodo3D *padre = n->padre;
		if (!n2->padre) {
			padre = NULL;
		} else {
			CNodo3D *p = CNodo3D_Find(esc->Nodo3D, n2->padre->name, 1);
			if (p) {
				padre = p;
			} else {
				E3D_LogBlue(g_FileLog, "  Parent not found: '%s'  Node: '%s'  Parent: '%s'", n2->padre->name, n2->name,
					padre ? padre->name : "(null)");
			}
		}
		if (n->padre != padre) {
			CNodo3D_SetPadre(n, padre);
		}
	}
	Escena_OrdenarNodos(esc);
}

void Escena_Jerarquia(CEscena *esc) {
	if (!g_OptimizeNodes) {
		return;
	}
	Escena_Limpiar(esc, "1");
	CTabla t = { 0, NULL };
	Tabla_Alloc8(&t, CEscena_NumNodos(esc));
	CNodo3D *n = esc->Nodo3D;
	int num = t.n;
	for (int i = 0; i < num; i++, n = n->sig) {
		CHECK(n);
		TABLA_CHECK(t.p, i, num);
		((M3DNodoPadre *)t.p)[i].nodo = n;
		TABLA_CHECK(t.p, i, num);
		((M3DNodoPadre *)t.p)[i].padre = n->padre;
		if (n->flags & NODO_BORRAR) {
			continue;
		}
		CNodo3D *pp = n->padre;
		if (pp) {
			while (pp->flags & NODO_BORRAR && pp->Refs - (pp->ant != NULL) - (pp->sig != NULL) == 1 && pp->padre) {
				pp = pp->padre;
			}
		}
		if (pp != n->padre) {
			E3D_LogInfo(g_FileLog, "HIERARCHY__: New parent for node: %s [new: %s] [prev: %s] [f:%08X]", n->name,
				pp->name, n->padre->name, n->flags);
			TABLA_CHECK(t.p, i, num);
			((M3DNodoPadre *)t.p)[i].padre = pp;
			continue;
		}
		CNodo3D *p1 = n->padre;
		if (!p1 || !(p1->flags & NODO_BORRAR)) {
			continue;
		}
		for (CNodo3D *a = p1->padre; a && a->indice >= 0; a = a->padre) {
			CNodo3D *d = Nodo_BuscarDescendiente(esc->Nodo3D, a);
			if (d) {
				E3D_LogInfo(g_FileLog, "HIERARCHY: (disabled): new parent for node: %s [new: %s] [prev: %s] [f:%08X]",
					n->name, d->name, p1->name, n->flags);
				break;
			}
		}
	}
	M3DNodoPadre *tab = t.p;
	for (int i = 0; i < num; i++) {
		TABLA_CHECK(t.p, i, num);
		TABLA_CHECK(t.p, i, num);
		CNodo3D_SetPadre(tab[i].nodo, tab[i].padre);
	}
	Escena_Ordenar(esc);
	Escena_Limpiar(esc, "2");
	if (tab) {
		operator_delete_array(tab);
	}
}


/* the meshes */

/* the Physique weights of the vertices: per vertex its bones (equal names add up) and weights; weights below 0.01
 * are spread over the other bones of the vertex and the bone removed */
static void M3D_LeePhysique(CNodo3D *nodo, M3DNodo *dat, int nVert) {
	int version = M3D_ReadInt("Version:");
	M3D_ReadInt("PhyVersion:");
	int vertices = M3D_ReadInt("NumVert:");
	CHECK(version == 0);
	CHECK(vertices == nVert);
	CTabla *skin = &dat->skin;
	Tabla_New1C(skin, vertices);
	int pesos = 0, borrados = 0;
	for (int i = 0; i < vertices; i++) {
		TABLA_CHECK(skin->p, i, skin->n);
		M3DSkinVert *vskin = (M3DSkinVert *)skin->p + i;
		M3D_ReadIndex(i);
		int bones = M3D_ReadInt(NULL);
		CHECK(bones > 0);
		vskin->NumBones = 0;
		for (int b = bones; b > 0; b--) {
			float peso;
			M3D_ReadString("Bone:", NULL, 0, 1);
			Nodo_LimpiarNombre(g_M3DCad);
			int k = 0;
			while (k < vskin->NumBones && strcmp(g_M3DCad, vskin->bone[k])) {
				k++;
			}
			M3D_ReadFloatTo(&peso);
			int n = vskin->NumBones;
			if (k < n) {
				vskin->peso[k] = peso + vskin->peso[k];
				continue;
			}
			if (n >= SKIN_MAX_BONES) {
				M3D_SetOk(0);
				E3D_LogVerbose(g_FileLog, "  physique:  objeto: '%s'  vertice: %d  pesos: %d  peso: %f", nodo->name, i,
					bones, (double)peso);
				CHECK(vskin->NumBones < SKIN_MAX_BONES);
			}
			Cad_Set(&vskin->bone[n], g_M3DCad);
			vskin->peso[n] = peso;
			vskin->NumBones++;
		}
		pesos += vskin->NumBones;
		for (int k = 0; k < vskin->NumBones; k++) {
			if (!(0.01 > (double)vskin->peso[k])) {
				continue;
			}
			CHECK(vskin->NumBones > 1);
			float reparto = vskin->peso[k] / (float)(vskin->NumBones - 1);
			for (int j = 0; j < vskin->NumBones; j++) {
				vskin->peso[j] = reparto + vskin->peso[j];
			}
			int ult = vskin->NumBones - 1;
			if (k != ult) {
				Cad_Set(&vskin->bone[k], vskin->bone[ult]);
				memcpy(&vskin->peso[k], &vskin->peso[ult], sizeof(float));
			}
			k--;
			vskin->NumBones--;
			borrados++;
		}
	}
	E3D_LogWhite(g_FileLog, "  INFO: Physique:  vertices: %4d  pesos borrados: %4d / %-4d  objeto: '%s'", vertices,
		borrados, pesos, nodo->name);
}

/* the default material of the faces without one, from ColorMalla */
static CMaterial *M3D_MatColorMalla(const vec3_t *c) {
	CMaterial *mat = operator_new(sizeof(CMaterial));
	mat = mat ? CMaterial_ctor(mat) : NULL;
	mat->params.power = 0.2f;
	mat->params.specLevel = 0.2f;
	mat->params.ambient[0] = M3D_ColorByte(c->x * 0x1.23d70ap-2f);
	mat->params.ambient[1] = M3D_ColorByte(c->y * 0x1.23d70ap-2f);
	mat->params.ambient[2] = M3D_ColorByte(c->z * 0x1.23d70ap-2f);
	mat->params.ambient[3] = 0;
	*(uint32_t *)mat->params.diffuse = M3D_Color(c);
	*(uint32_t *)mat->params.specular = 0xffffff;
	Cad_Printf(&mat->Nombre, "color_malla [%d,%d,%d]", mat->params.diffuse[0], mat->params.diffuse[1],
		mat->params.diffuse[2]);
	CMaterial *found = CListaMat_Find(g_ListaMat, mat);
	if (found) {
		((objDelete_t)mat->vftable[0])(mat, 1);
		return found;
	}
	CListaMat_Add(g_ListaMat, mat);
	return mat;
}

/* v normalized unless its length is 0 */
static void M3D_Normalize(vec3_t *v) {
	float x = v->x, y = v->y, z = v->z;
	float len = (float)__builtin_sqrt((double)(y * y + x * x + z * z));
	if ((double)len == 0.0) {
		return;
	}
	float s = (float)(1.0 / (double)len);
	v->x = x * s;
	v->y = y * s;
	v->z = z * s;
}

void M3D_LeeMalla(CNodo3D *nodo, byte anim) {
	m3dTransform_t mt;
	matrix_t tmp, m;
	vec3_t color, v;

	if (!anim) {
		M3D_ReadString("TipoGeom:", NULL, 0, 0);
		if (!strcmp(g_M3DCad, "Malla")) {
			CMallaTri *mt3 = operator_new(sizeof(CMallaTri));
			mt3 = mt3 ? CMallaTri_ctor(mt3) : NULL;
			CNodo3D_SetObj3D(nodo, (IObjeto3D *)mt3);
		}
	}
	if (CNodo3D_Clase(nodo) != CLASE_OBJ3D_MALLATRI) {
		return;
	}
	M3DNodo *dat = M3D_NodoDat(nodo);
	CMallaTri *malla = Obj3D_Cast(nodo->obj3D, CLASE_OBJ3D_MALLATRI);
	if (anim) {
		return;
	}
	int nVert = M3D_ReadInt("NumVerts:");
	int nTVert = M3D_ReadInt("NumTVerts:");
	int nTri = M3D_ReadInt("NumCaras:");
	M3D_ReadBool("DefMalla:");
	if (!nVert || !nTri) {
		M3D_SetOk(0);
		E3D_LogVerbose(g_FileLog, "  objeto: '%s'  caras: %d  nVert: %d  nTVert: %d", nodo->name, nTri, nVert, nTVert);
	}
	CMallaTri_SetNumVert(malla, nVert, 0);
	CMallaTri_SetNumTVert(malla, nTVert, 0);
	CMallaTri_SetNumTri(malla, nTri, 0);
	if (nTVert > nTri * 3 && nTVert > 5000) {
		M3D_SetOk(0);
		E3D_LogVerbose(g_FileLog, "  objeto: '%s'  caras: %d  nTVert: %d  (3*caras: %d)", nodo->name, nTri, nTVert,
			nTri * 3);
	}
	M3D_ReadTransform("local_a_pivote", &mt);
	M3DTransform_ToMatrix(&mt, &dat->mPivote);
	if (g_M3DVersion < 11 || !g_UseWSM) {
		m = *Matrix_MulD3D(&tmp, &dat->mPivote, &dat->mNodo);
	} else {
		m = dat->mNodo;
	}
	int iMat = M3D_ReadInt("Material:");
	M3D_ReadVec3("ColorMalla:", &color, 0);
	for (int i = 0; i < nVert; i++) {
		M3D_ReadIndex(i);
		M3D_ReadVec3(NULL, &v, 1);
		Vec3_TransformH(&v, &v, &m);
		CMallaTri_SetVert(malla, (uint32_t)i, &v);
	}
	if (g_M3DVersion > 12 && M3D_ReadBool("Physique:")) {
		M3D_LeePhysique(nodo, dat, nVert);
	}
	CMaterial *defMat = NULL, *lastRes = NULL;
	int lastMat = -1, lastSub = -1, degen = 0;
	for (int i = 0; i < nTri; i++) {
		M3D_ReadIndex(i);
		int a = M3D_ReadInt(NULL);
		int b = M3D_ReadInt(NULL);
		int c = M3D_ReadInt(NULL);
		int sub = M3D_ReadInt(NULL);
		CHECK(sub >= 0);
		CMaterial *mat;
		if (iMat == lastMat && sub == lastSub) {
			mat = lastRes;
		} else {
			mat = M3D_FindMat(iMat, sub);
			M3D_MatUsado(mat);
			lastMat = iMat;
			lastRes = mat;
			lastSub = sub;
		}
		if (!mat) {
			if (!defMat) {
				defMat = M3D_MatColorMalla(&color);
			}
			mat = defMat;
		}
		plane_t plano;
		if (a == c || c == b || b == a || !Plane_Calc(&plano, &malla->pVert[a], &malla->pVert[c], &malla->pVert[b])) {
			degen++;
		}
		CMallaTri_SetTriVerts(malla, (uint32_t)i, a, c, b);
		CMallaTri_SetTriMat(malla, (uint32_t)i, mat);
	}
	if (degen) {
		M3D_SetOk(0);
		E3D_LogVerbose(g_FileLog, "  caras degeneradas: %d  caras: %d  objeto: '%s'", degen, nTri, nodo->name);
	}
	if (nTVert) {
		float maxU = 0.0f, maxV = 0.0f;
		for (int i = 0; i < nTVert; i++) {
			uv_t uv;
			M3D_ReadIndex(i);
			M3D_ReadVec2(NULL, &uv.u);
			uv.u = g_M3DPreScaleU * uv.u;
			uv.v = g_M3DPreScaleV * (1.0f - uv.v);
			CMallaTri_SetTVert(malla, (uint32_t)i, &uv);
			maxU = Float_MaxSS((float)__builtin_fabs((double)uv.u), maxU);
			maxV = Float_MaxSS((float)__builtin_fabs((double)uv.v), maxV);
		}
		if ((maxU > 16.0f || maxV > 16.0f) && !(nodo->flags & 0x80)) {
			M3D_SetOk(0);
			E3D_LogVerbose(g_FileLog, "  WARN: coord. de textura: %f %f  objeto: '%s'", (double)maxU, (double)maxV,
				nodo->name);
		}
		for (int i = 0; i < nTri; i++) {
			M3D_ReadIndex(i);
			int a = M3D_ReadInt(NULL);
			int b = M3D_ReadInt(NULL);
			int c = M3D_ReadInt(NULL);
			CMallaTri_SetTriTVerts(malla, (uint32_t)i, a, c, b);
		}
	}
	for (int i = 0; i < nTri; i++) {
		vec3_t n0, n1, n2;
		M3D_ReadIndex(malla->pTri[i].v[0]);
		M3D_ReadVec3(NULL, &n0, 1);
		M3D_ReadIndex(malla->pTri[i].v[2]);
		M3D_ReadVec3(NULL, &n2, 1);
		M3D_ReadIndex(malla->pTri[i].v[1]);
		M3D_ReadVec3(NULL, &n1, 1);
		Vec3_TransformDir0(&n0, &n0, &m);
		Vec3_TransformDir0(&n1, &n1, &m);
		Vec3_TransformDir0(&n2, &n2, &m);
		M3D_Normalize(&n0);
		M3D_Normalize(&n1);
		M3D_Normalize(&n2);
		CMallaTri_SetTriNormals(malla, (uint32_t)i, &n0, &n1, &n2);
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
			mapa->fx = FXTextura_Kind(nombre);
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
			sub->vftable = CLISTAMAT_VFTABLE;
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


/* the reader of the M3D files */

#define M3DNODO_CTOR 0x2bbb90
#define M3D_HDR 0x20			/* the header "[M3D,BIN,ETIQ:si]" */
#define M3D_NOMBRE 0x100
/* the node flags the reader sets (NODO_*) */
#define NODO_RAIZ 0x1
#define NODO_GRUPO 0x2
#define NODO_SELEC 0x4
#define NODO_HVISIBLE 0x10
#define NODO_NO_CASTSHADOWS 0x20
#define NODO_NO_RCVSHADOWS 0x40
#define NODO_NO_RENDER 0x80
#define NODO_BOXMODE 0x100
#define NODO_BONE (1u << 20)
#define NODO_EFECTO (1u << 19)
#define NODO_NO_TRANS_TAB (NODO_NOUPDATE | NODO_NO_TABNODOR)
/* the kinds of objects ("Tipo:") */
enum { M3D_GEOMETRIA = 1, M3D_CAMARA, M3D_LUZ, M3D_OBJETIVO, M3D_DUMMY, M3D_PORTAL };

/* the bits of the animation of a node (CAnimNodo_Init flags) */
#define ANIM_POS 0x1
#define ANIM_ROT 0x2
#define ANIM_FOV 0x4
#define ANIM_COLOR 0x8
#define ANIM_INTENS 0x10
#define ANIM_VIS 0x80

static const matrix_t g_M3DIdentidad = { { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f } };

/* the vertex animation of a morph node: the vertices of each frame through the pivot */
static void M3D_LeeEVA(CNodo3D *nodo, M3DNodo *dat) {
	m3dTransform_t mt;
	M3DTabla4 t;
	matrix_t piv, m, tmp, res;
	CAnimVert *av;
	vec3_t v;
	int nv, i, f, antes, despues;

	if (g_M3DVersion > 9) {
		M3D_ReadTransform("local_a_pivote", &mt);
		M3DTransform_ToMatrix(&mt, &piv);
	} else {
		piv = dat->mPivote;
	}
	if (g_M3DVersion >= 11 && g_UseWSM) {
		Matrix_LoadIdentity(&piv);
	}
	nv = M3D_ReadInt("NumVerts:");
	CAnimNodo_SetEVA(dat->AnimNodo, nv);
	m = *Matrix_MulD3D(&res, &piv, M3DNodo_FrameMatrix(dat, &tmp, 0));
	for (i = 0; i < nv; i++) {
		CHECK(M3D_ReadInt("Vertice:") == i);
		crt_eh_vector_ctor(t.t, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_CTOR), Game_Ptr(M3DTABLA_DTOR));
		M3DTabla_ReadFloat(&t.t[0], "EVA_X");
		M3DTabla_ReadFloat(&t.t[2], "EVA_Y");
		M3DTabla_ReadFloat(&t.t[1], "EVA_Z");
		av = CAnimNodo_GetVert(dat->AnimNodo, (uint32_t)i);
		CHECK(av);
		CAnimVert_AllocTr(av, g_M3DFrames);
		for (f = 0; f < g_M3DFrames; f++) {
			M3DTabla4_GetVec(&t, &v, (uint32_t)f);
			Vec3_TransformH(&v, &v, &m);
			*CAnimVert_Frame(av, (uint32_t)f) = v;
		}
		crt_eh_vector_dtor(t.t, sizeof(M3DTabla), 4, Game_Ptr(M3DTABLA_DTOR));
	}
	antes = CEVA_MemSize(dat->AnimNodo->EVA);
	CAnimNodo_OptimizeEVA(dat->AnimNodo);
	despues = dat->AnimNodo->EVA ? CEVA_MemSize(dat->AnimNodo->EVA) : 0;
	E3D_DebugPrintf("\noptimizar EVA: '%s'  %d -> %d  (%.2f %%)", nodo->name, antes, despues,
		(double)despues * 100.0 / (double)antes);
}

/* the animation tables of an object: its tracks and visibility, the values of its kind; the mask of the channels
 * that stay */
static uint32_t M3D_LeeAnimNodo(CEscena *esc, CNodo3D *nodo, M3DNodo *dat, int k, int tipo, byte exportado,
	byte bEstatica) {
	CNodo3D *m;
	uint32_t mask, extra, base;

	CHECK(esc->AnimM3D);
	dat->AnimNodo = CAnimacion_GetNodo(esc->AnimM3D, (uint32_t)k);
	CHECK(dat->AnimNodo);
	Bprintf(g_AnimNodoName, 0x100, "%s", nodo->name);
	if (g_M3DVersion >= 11 && g_UseWSM) {
		M3DPista_Read(&dat->pista2);
		M3DPista_Read(&dat->pista);
	} else {
		M3DPista_Read(&dat->pista);
		M3DPista_Read(&dat->pista2);
	}
	M3DTabla_ReadFloat(&dat->t2a4[0], "NodoVis");
	if (exportado) {
		switch (tipo) {
		case M3D_GEOMETRIA:
		case M3D_OBJETIVO:
		case M3D_DUMMY:
		case M3D_PORTAL:
			break;
		case M3D_CAMARA:
			M3DTabla_ReadFloat(&dat->t2a4[5], "FOV");
			break;
		case M3D_LUZ:
			M3DTabla_ReadInt(&dat->t2a4[1], "LuzR");
			M3DTabla_ReadInt(&dat->t2a4[2], "LuzG");
			M3DTabla_ReadInt(&dat->t2a4[3], "LuzB");
			M3DTabla_ReadFloat(&dat->t2a4[4], "LuzI");
			break;
		default:
			CHECK(0);
		}
	}
	/* position and rotation; the field of view of a camera, the color and intensity of a light; the visibility
	 * when it changes */
	extra = tipo == M3D_LUZ ? ANIM_COLOR | ANIM_INTENS : 0;
	base = (tipo == M3D_CAMARA) * ANIM_FOV + (ANIM_POS | ANIM_ROT);
	mask = (dat->t2a4[0].fija ? 0 : ANIM_VIS) | base | extra;
	TABLA_CHECK(dat->t2a4[0].p, 0, dat->t2a4[0].n);
	if (*(float *)dat->t2a4[0].p != 1.0f) {
		mask = base | extra | ANIM_VIS;
	}
	if (!exportado) {
		mask = 0;
		E3D_LogWhite(g_FileLog, "Animation node hidden: %s (node animation discarded)", nodo->name);
	} else if (mask && !bEstatica && g_EscenaAnimPadre) {
		m = CNodo3D_Find(g_EscenaAnimPadre->Nodo3D, nodo->name, 1);
		if (!m) {
			E3D_LogWhite(g_FileLog, "Animation node not found in Model: %s (node animation discarded)", nodo->name);
			if (mask & ANIM_VIS) {
				Con_Warnf("Visibility animation discarded: %s\n", nodo->name);
			}
			mask = 0;
		} else {
			uint32_t v = ~(uint32_t)m->i50 & mask;

			if (v != mask) {
				E3D_LogWhite(g_FileLog, "Animation node masked: %s (node animation discarded)", nodo->name);
			}
			mask = v;
		}
	}
	CAnimNodo_Init(dat->AnimNodo, (uint32_t)g_M3DFrames, (uint32_t)g_M3DPriFrame, mask);
	if (g_M3DVersion > 8 && M3D_ReadBool("DefMalla:")) {
		CHECK(exportado);
		if (!Wild_MatchLower("*morph*", g_M3DBaseName)) {
			Con_Warnf("WARN: Animation with morphing: %s\n", g_M3DName);
		}
		mask = 0;
		CAnimNodo_Init(dat->AnimNodo, (uint32_t)g_M3DFrames, (uint32_t)g_M3DPriFrame, 0);
		E3D_LogWhite(g_FileLog, "Animation node for EVA mesh: %s (node animation discarded)", nodo->name);
		CHECK(g_EscenaAnimPadre);
		CHECK(CNodo3D_Find(g_EscenaAnimPadre->Nodo3D, nodo->name, 1));
		M3D_LeeEVA(nodo, dat);
	}
	return mask;
}

/* the frames of the channels of mask into the animation node */
static void M3D_FramesAnimNodo(CNodo3D *nodo, uint32_t mask) {
	M3DNodo *dat = M3D_NodoDat(nodo);
	CAnimNodo *an;
	vec3_t v;
	quat_t q;
	uint32_t idx, c;
	int f, i, b;
	float x;

	CHECK(dat->AnimNodo);
	for (f = g_M3DPriFrame; f <= g_M3DUltFrame; f++) {
		idx = (uint32_t)(f - g_M3DPriFrame);
		if (mask & ANIM_POS) {
			M3DTabla4_GetVec(&dat->pista.pos, &v, idx);
			an = dat->AnimNodo;
			i = CAnimNodo_FrameIndex(an, f, an->Pos, ANIM_POS);
			an->Pos[i] = v;
		}
		if (mask & ANIM_ROT) {
			M3DTabla4_GetQuat(&dat->pista.rot, &q, idx);
			an = dat->AnimNodo;
			i = CAnimNodo_FrameIndex(an, f, an->Rot, ANIM_ROT);
			an->Rot[i] = q;
		}
		if (mask & ANIM_VIS) {
			TABLA_CHECK(dat->t2a4[0].p, idx, dat->t2a4[0].n);
			b = (int)((double)(((float *)dat->t2a4[0].p)[idx] * 255.0f) + 0.5);
			if (b <= 0) {
				b = 0;
			} else if (b >= 0xff) {
				b = 0xff;
			}
			an = dat->AnimNodo;
			i = CAnimNodo_FrameIndex(an, f, an->Visible, ANIM_VIS);
			an->Visible[i] = (byte)b;
		}
		if (mask & ANIM_COLOR) {
			TABLA_CHECK(dat->t2a4[3].p, idx, dat->t2a4[3].n);
			const int *pb = (const int *)dat->t2a4[3].p + idx;
			TABLA_CHECK(dat->t2a4[2].p, idx, dat->t2a4[2].n);
			const int *pg = (const int *)dat->t2a4[2].p + idx;
			TABLA_CHECK(dat->t2a4[1].p, idx, dat->t2a4[1].n);
			const int *pr = (const int *)dat->t2a4[1].p + idx;
			c = (uint32_t)(byte)*pr | (uint32_t)(byte)*pg << 8 | (uint32_t)(byte)*pb << 16;
			an = dat->AnimNodo;
			i = CAnimNodo_FrameIndex(an, f, an->Color, ANIM_COLOR);
			((uint32_t *)an->Color)[i] = c;
		}
		if (mask & ANIM_INTENS) {
			TABLA_CHECK(dat->t2a4[4].p, idx, dat->t2a4[4].n);
			an = dat->AnimNodo;
			x = ((float *)dat->t2a4[4].p)[idx];
			i = CAnimNodo_FrameIndex(an, f, an->Intens, ANIM_INTENS);
			an->Intens[i] = x;
		}
		if (mask & ANIM_FOV) {
			TABLA_CHECK(dat->t2a4[5].p, idx, dat->t2a4[5].n);
			an = dat->AnimNodo;
			x = ((float *)dat->t2a4[5].p)[idx];
			i = CAnimNodo_FrameIndex(an, f, an->FOV, ANIM_FOV);
			an->FOV[i] = x;
		}
	}
}

/* the kind of object of "Tipo:", 0 (reported) for an unknown one */
static int M3D_LeeTipo(void) {
	M3D_ReadString("Tipo:", g_M3DCad, M3D_CAD, 0);
	if (!strcmp(g_M3DCad, "Geometria")) {
		return M3D_GEOMETRIA;
	}
	if (!strcmp(g_M3DCad, "Camara")) {
		return M3D_CAMARA;
	}
	if (!strcmp(g_M3DCad, "Luz")) {
		return M3D_LUZ;
	}
	if (!strcmp(g_M3DCad, "Objetivo")) {
		return M3D_OBJETIVO;
	}
	if (!strcmp(g_M3DCad, "Dummy")) {
		return M3D_DUMMY;
	}
	if (!strcmp(g_M3DCad, "Portal")) {
		return M3D_PORTAL;
	}
	Err_Printf(1, "Tipo de nodo no v\xe1lido: '%s'", g_M3DCad);
	return 0;
}

static void Nodo_SetFlag(CNodo3D *nodo, uint32_t flag, byte on) {
	if (on) {
		nodo->flags |= flag;
	} else {
		nodo->flags &= ~flag;
	}
}

/* object k of the file: the node, its flags and user properties, transform and object, its animation */
static void M3D_LeeObjeto(CEscena *esc, int k, byte bEstatica, byte bAnim, byte castShadows, byte rcvShadows) {
	char buf[M3D_NOMBRE];
	CNodo3D *nodo;
	M3DNodo *dat;
	void *p;
	float vis;
	uint32_t mask;
	int tipo = 0, lines;
	byte exportado;

	p = operator_new(sizeof(CNodo3D));
	nodo = p ? CNodo3D_ctor(p) : NULL;
	nodo->indice = k;
	CEscena_AddNodo(esc, nodo);
	dat = M3D_NodoDat(nodo);
	dat->AnimNodo = NULL;
	dat->mPivote = g_M3DIdentidad;
	M3D_ReadString("Objeto:", buf, sizeof(buf), 1);
	Nodo_LimpiarNombre(buf);
	Cad_Set(&nodo->name, buf);
	CHECK(strlen(buf) > 0);
	M3D_ReadString("Padre:", buf, sizeof(buf), 1);
	Nodo_LimpiarNombre(buf);
	CNodo3D_SetPadreName(nodo, buf);
	CHECK(nodo->padre);
	exportado = M3D_ReadBool("Exportado:");
	Nodo_SetFlag(nodo, NODO_GRUPO, M3D_ReadBool("Grupo:"));
	Nodo_SetFlag(nodo, NODO_SELEC, M3D_ReadBool("Seleccionado:"));
	Nodo_SetFlag(nodo, NODO_BORRAR, M3D_ReadBool("Oculto:"));
	Nodo_SetFlag(nodo, NODO_HVISIBLE, M3D_ReadBool("HeredarVisible:"));
	if (g_M3DVersion > 11) {
		Nodo_SetFlag(nodo, NODO_NO_CASTSHADOWS, !M3D_ReadBool("CastShadows:"));
		Nodo_SetFlag(nodo, NODO_NO_RCVSHADOWS, !M3D_ReadBool("RcvShadows:"));
		Nodo_SetFlag(nodo, NODO_NO_RENDER, !M3D_ReadBool("Renderable:"));
		Nodo_SetFlag(nodo, NODO_BOXMODE, M3D_ReadBool("BoxMode:"));
	}
	if (!castShadows) {
		nodo->flags |= NODO_NO_CASTSHADOWS;
	}
	if (!rcvShadows) {
		nodo->flags |= NODO_NO_RCVSHADOWS;
	}
	Escena_CheckDuplicados(esc, nodo);
	if (!crt_strncmp(nodo->name, "Bip ", 4) || !crt_strncmp(nodo->name, "Bip_", 4) ||
		!crt_strncmp(nodo->name, "BipB", 4) || !crt_strncmp(nodo->name, "Bone", 4)) {
		nodo->flags |= NODO_BIP;
	}
	if (g_M3DVersion > 13) {
		lines = M3D_ReadInt("UserPropBufferLines:");
		if (lines > 0) {
			p = operator_new(sizeof(CArchINI));
			nodo->ini = p ? CArchINI_ctor(p) : NULL;
			for (; lines > 0; lines--) {
				M3D_ReadString("Line:", g_M3DCad, M3D_CAD, 1);
				CArchINI_AddLine(nodo->ini, g_M3DCad, -1);
			}
		}
		if (g_EscenaAnimPadre && nodo->ini) {
			CArchINI *ini = nodo->ini;

			CArchINI_dtor(ini);
			operator_delete_sized(ini, sizeof(CArchINI));
			nodo->ini = NULL;
		}
	}
	if (exportado) {
		tipo = M3D_LeeTipo();
	}
	if (bEstatica) {
		if (g_M3DVersion < 11) {
			M3D_ReadTransform("pivote_a_mundo", &dat->mt);
			M3D_ReadTransform("pivote_a_padre", &dat->mt2);
		} else {
			M3D_ReadTransform("pivote_a_mundo", g_UseWSM ? &dat->mt2 : &dat->mt);
			M3D_ReadTransform("ObjTMAfterWSM", g_UseWSM ? &dat->mt : &dat->mt2);
		}
		M3DTransform_ToMatrixAnim(&dat->mt, &dat->mNodo);
		nodo->pos = dat->mt.pos;
		nodo->rot = dat->mt.rot;
		nodo->escala = dat->mt.esc;
		M3D_Label("Visible:");
		M3D_ReadFloatTo(&vis);
		nodo->visible = vis;
		if (exportado) {
			switch (tipo) {
			case M3D_GEOMETRIA:
				M3D_LeeMalla(nodo, 0);
				break;
			case M3D_CAMARA:
				M3D_LeeCamara(nodo, 0, 0);
				break;
			case M3D_LUZ:
				M3D_LeeLuz(nodo, 0, 0);
				break;
			case M3D_OBJETIVO:
			case M3D_DUMMY:
				break;
			case M3D_PORTAL:
				M3D_LeePortal(nodo, 0);
				break;
			default:
				CHECK(0);
			}
		}
	}
	if (bAnim) {
		mask = M3D_LeeAnimNodo(esc, nodo, dat, k, tipo, exportado, bEstatica);
		M3D_FramesAnimNodo(nodo, mask);
	}
}

/* the root of a game model: the node named name2 or the one with MakeRootNode Enable in its user properties, under
 * the scene root, the other children of the scene root moved below it; NULL without one */
static CNodo3D *M3D_RaizModelo(CEscena *esc, const char *name2) {
	char initPos[0x80];
	CNodo3D *n, *root = NULL, *padre;

	memset(initPos, 0, sizeof(initPos));
	for (n = esc->Nodo3D; n; n = n->sig) {
		if (name2 && *name2) {
			if (!crt_stricmp(n->name, name2)) {
				root = n;
				break;
			}
		} else if (n->ini && CArchINI_GetBool(n->ini, "MakeRootNode", "Enable", 0)) {
			CArchINI_GetText(n->ini, "MakeRootNode", "InitPos", initPos, sizeof(initPos), NULL);
			root = n;
			break;
		}
	}
	if (root) {
		if (strcmp(root->name, "DC_Root")) {
			Con_Warnf("WARN: RootNode: %s\n", root->name);
		}
		E3D_LogWhite(g_FileLog, "RootNode: %s", root->name);
		Cad_Set(&esc->nodo, root->name);
		padre = CNodo3D_FindPrev(root, "_raiz_escena");
		CHECK(padre);
		CNodo3D_SetPadre(root, padre);
	}
	for (n = esc->Nodo3D; n; n = n->sig) {
		if (root && n != root && n->padre && !crt_stricmp(n->padre->name, "_raiz_escena")) {
			CNodo3D_SetPadre(n, root);
		}
	}
	return root;
}

/* the zones of the portals and the targets of the cameras and lights */
static void M3D_Objetivos(CEscena *esc) {
	CNodo3D *n, *t;
	M3DNodo *dat;

	for (n = esc->Nodo3D; n; n = n->sig) {
		if (n->indice < 0) {
			continue;
		}
		dat = M3D_NodoDat(n);
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_PORTAL) {
			CPortal *portal = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_PORTAL);

			CPortal_SetZona(portal, 0, CNodo3D_Find(esc->Nodo3D, dat->objetivo, 1));
			CPortal_SetZona(portal, 1, CNodo3D_Find(esc->Nodo3D, dat->lado2, 1));
			continue;
		}
		if (!dat->objetivo) {
			continue;
		}
		t = CNodo3D_Find(esc->Nodo3D, dat->objetivo, 1);
		CHECK(t);
		if (CNodo3D_Find(t, dat->objetivo, 0)) {
			E3D_LogBlue(g_FileLog, "ERROR: Duplicated target node: %s", t->name);
		}
		if (CNodo3D_Clase(n) == CLASE_OBJ3D_CAMARA) {
			CCamara *cam = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_CAMARA);

			cam->target = t->pos;
			t->flags |= NODO_BORRAR;
		} else if (CNodo3D_Clase(n) == CLASE_OBJ3D_LUZ) {
			Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);
			t->flags |= NODO_BORRAR;
		} else {
			CHECK(0);
		}
	}
}

/* the nodes without their own transform taken out of the render tables (OptimizeNodes 1) */
static void M3D_OptimizarNodos(CEscena *esc) {
	CNodo3D *n, *c;
	int pass, cnt;
	byte cambio;

	for (n = esc->Nodo3D; n; n = n->sig) {
		if (n->flags & NODO_BONE) {
			CHECK(!(n->flags & NODO_NOUPDATE));
			CHECK(!(n->flags & NODO_NO_TABNODOR));
		}
		if (n->indice < 0) {
			continue;
		}
		if (n->obj3D) {
			if ((n->flags & NODO_BIP) && !(n->flags & NODO_NO_RENDER)) {
				E3D_LogBlue(g_FileLog, "WARN: BIP node with NO_RENDER not set: %s", n->name);
			}
			if (n->flags & NODO_NO_RENDER) {
				if (n->flags & NODO_BIP) {
					CHECK(CNodo3D_Clase(n) == CLASE_OBJ3D_MALLATRI);
					CNodo3D_FreeObj3D(n);
				} else {
					E3D_LogInfo(g_FileLog, "NOT deleted object from node: %s", n->name);
				}
			}
			if (n->obj3D) {
				continue;
			}
		}
		n->flags |= NODO_NO_RENDER;
		if (!(n->flags & NODO_BONE) && n->Refs - (n->ant != NULL) - (n->sig != NULL) == 0) {
			if (n->flags & NODO_EFECTO) {
				Con_Warnf("WARN: NODO_NO_TRANS+NODO_EFFECT: %s\n", n->name);
			}
			n->flags |= NODO_NO_TRANS_TAB;
			if (!Nodo_EsDeJuego(n)) {
				E3D_LogInfo(g_FileLog, "NODO_NO_TRANS -> hidden: %s", n->name);
				n->flags |= NODO_BORRAR;
			} else {
				E3D_LogWhite(g_FileLog, "NODO_NO_TRANS: %s", n->name);
			}
		}
	}
	/* then the empty nodes whose children all left the tables, until none is left */
	pass = 1;
	do {
		cambio = 0;
		for (n = esc->Nodo3D; n; n = n->sig) {
			uint32_t flags = n->flags;

			if (n->indice < 0 || n->obj3D || (flags & (NODO_BONE | NODO_NO_TABNODOR))) {
				continue;
			}
			cnt = n->Refs - (n->ant != NULL) - (n->sig != NULL);
			for (c = esc->Nodo3D; c; c = c->sig) {
				if (c->padre == n && (c->flags & NODO_NO_TABNODOR)) {
					cnt--;
				}
			}
			if (cnt) {
				continue;
			}
			CHECK(flags & NODO_NO_RENDER);
			n->flags |= NODO_NO_TRANS_TAB;
			cambio = 1;
			E3D_LogInfo(g_FileLog, "NODO_NO_TRANS__%d -> hidden: %s", pass, n->name);
			n->flags |= NODO_BORRAR;
		}
		pass++;
	} while (cambio);
	if (g_OptimizeNodes & 2) {
		Escena_ColapsarBip(esc);
	}
}

/* the color of "LuzAmbiente:" or "ColorFondo:" with the intensity 1 */
static void M3D_LeeColorEscena(const char *tok, ColorIntens *c) {
	vec3_t v;

	M3D_ReadVec3(tok, &v, 0);
	c->rgba = M3D_Color(&v);
	c->intens = 1.0f;
}

/* the M3D file name (the model or, with g_EscenaAnimPadre set, an animation of it; the .CM3 of an animation first),
 * name2 the node of a game model to make its root; arg is not used */
CEscena *Escena_Load(const char *name, int arg, const char *name2) {
	char hdr[M3D_HDR], tipo[0x10];
	CEscena *esc, *cm3;
	CListaMat *lm;
	CNodo3D *nodo, *root;
	void *p;
	int etiq, nObj, nMat, fps = 0, i;
	byte castShadows, rcvShadows, bEstatica, bAnim;

	(void)arg;
	Cad_Copy(g_M3DName, name, MAX_RUTA);
	Ruta_Part(g_M3DDir, g_M3DName, 3);
	Ruta_Part(g_M3DBaseName, g_M3DName, 4);
	Cad_Copy(g_M3DTexturePath, g_M3DDir, MAX_RUTA);
	g_M3DOk = 1;
	if (g_EscenaAnimPadre && g_CM3UseScenes == 1 && (cm3 = CM3_Load(g_M3DName)) != NULL) {
		return cm3;
	}
	E3D_LogDebug(g_FileLog, "M3D: %s", g_M3DName);
	castShadows = 1;
	rcvShadows = 1;
	CHECK(!g_M3DFileINI);
	p = operator_new(sizeof(CArchINI));
	g_M3DFileINI = p ? CArchINI_ctor(p) : NULL;
	if (!CArchINI_Load(g_M3DFileINI, g_M3DName, ".ini", 0)) {
		E3D_LogDebug(g_FileLog, "INI: %s.ini", g_M3DBaseName);
		castShadows = CArchINI_GetBool(g_M3DFileINI, "Model", "CastShadows", 1);
		rcvShadows = CArchINI_GetBool(g_M3DFileINI, "Model", "RcvShadows", 1);
		CArchINI_GetText(g_M3DFileINI, "Model", "TexturePath", g_M3DTexturePath, MAX_RUTA, NULL);
	} else if (g_M3DFileINI) {
		CArchINI *ini = g_M3DFileINI;

		CArchINI_dtor(ini);
		operator_delete_sized(ini, sizeof(CArchINI));
		g_M3DFileINI = NULL;
	}
	g_M3DUseOpacity = 1;
	g_M3DUseEnvmap = 1;
	g_M3DUseBumpmap = 1;
	g_M3DUseGlowmap = 1;
	g_M3DPreScaleU = 1.0f;
	g_M3DPreScaleV = 1.0f;
	if (g_M3DFileINI) {
		g_M3DUseOpacity = CArchINI_GetBool(g_M3DFileINI, "Model", "UseOpacity", 1);
		g_M3DUseEnvmap = CArchINI_GetBool(g_M3DFileINI, "Model", "UseEnvmap", 1);
		g_M3DUseBumpmap = CArchINI_GetBool(g_M3DFileINI, "Model", "UseBumpmap", 1);
		g_M3DUseGlowmap = CArchINI_GetBool(g_M3DFileINI, "Model", "UseGlowmap", 1);
		g_M3DPreScaleU = CArchINI_GetFloat(g_M3DFileINI, "Model", "PreScaleU", 1.0f);
		g_M3DPreScaleV = CArchINI_GetFloat(g_M3DFileINI, "Model", "PreScaleV", 1.0f);
	}
	/* the header "[M3D,BIN,ETIQ:si]", read as binary */
	Lee_Open(NULL, g_M3DName, 1);
	i = 0;
	do {
		int c = Lee_Byte();

		hdr[i++] = (char)c;
		if (c == ']') {
			break;
		}
	} while (i < M3D_HDR - 1);
	hdr[i] = '\0';
	if (Proc_Scan(hdr, "[M3D,&.*t,ETIQ:&(tipo_si_no)l]", sizeof(tipo) - 1, tipo, &etiq) != 2) {
		Err_Report(g_ErrPcxFormat, 1, "\narchivo: '%s'", g_M3DName);
	}
	if (strcmp(tipo, "TXT")) {
		CHECK(!strcmp(tipo, "BIN"));
	}
	g_M3DBinary = !strcmp(tipo, "BIN");
	g_M3DLabels = etiq != 0;
	if (!g_M3DBinary) {
		/* a text file again from the start, after the header */
		Lee_Close();
		Lee_Open(NULL, g_M3DName, 0);
		while (Lee_Byte() != ']') {
		}
	}
	if (!g_M3DBinary || g_M3DLabels) {
		Err_Printf(0, "File exported in text mode:\nM3D: %s\ncab: %s", g_M3DName, hdr);
	}
	g_M3DNumMat = 0;
	g_M3DNodos = NULL;
	M3D_ReadString("Formato:", g_M3DCad, M3D_CAD, 0);
	CHECK(!strcmp(g_M3DCad, "M3D"));
	g_M3DVersion = M3D_ReadInt("Version:");
	CHECK(g_M3DVersion >= 5 && g_M3DVersion <= 14);
	M3D_ReadString("Generado:", g_M3DCad, M3D_CAD, 0);
	M3D_ReadString("Fecha:", g_M3DCad, M3D_CAD, 0);
	M3D_ReadString("Hora:", g_M3DCad, M3D_CAD, 0);
	M3D_ReadString("Escena:", g_M3DEscenaMax, MAX_RUTA, 0);
	bEstatica = M3D_ReadBool("Estatica:");
	if (bEstatica) {
		M3D_ReadInt("Frame:");
	}
	bAnim = M3D_ReadBool("Animacion:");
	if (bAnim) {
		g_M3DPriFrame = M3D_ReadInt("Primer_frame:");
		g_M3DUltFrame = M3D_ReadInt("Ultimo_frame:");
		fps = M3D_ReadInt("Frames_por_segundo:");
		CHECK(g_M3DPriFrame <= g_M3DUltFrame);
		g_M3DFrames = g_M3DUltFrame - g_M3DPriFrame + 1;
	}
	nObj = M3D_ReadInt("Num_objetos:");
	CHECK(nObj > 0);
	if (g_EscenaAnimPadre) {
		if (bEstatica) {
			Err_Printf(0, "Animation file with model information:\nM3D: %s\nMAX: %s", g_M3DName, g_M3DEscenaMax);
		}
		if (!bAnim) {
			Err_Printf(0, "Animation file without animation information:\nM3D: %s\nMAX: %s", g_M3DName,
				g_M3DEscenaMax);
		}
	} else if (!bEstatica) {
		Err_Printf(0, "Model file without model information:\nM3D: %s\nMAX: %s", g_M3DName, g_M3DEscenaMax);
	}
	p = operator_new(sizeof(CEscena));
	esc = p ? CEscena_ctor(p) : NULL;
	Cad_Set(&esc->name, g_M3DName);
	esc->ini = g_M3DFileINI;
	g_M3DFileINI = NULL;
	if (bAnim) {
		M3DTabla *t = operator_new(6 * sizeof(M3DTabla));

		if (t) {
			for (i = 0; i < 6; i++) {
				t[i].n = 0;
				t[i].p = NULL;
				t[i].fija = 0;
			}
		}
		g_M3DTablasEscena = t;
		p = operator_new(sizeof(CAnimacion));
		esc->AnimM3D = p ? CAnimacion_ctor(p) : NULL;
		Cad_Set(&esc->AnimM3D->name, g_M3DBaseName);
		CAnimacion_Init(esc->AnimM3D, nObj, g_M3DFrames, g_M3DPriFrame);
		esc->AnimM3D->fps = (float)fps;
	}
	g_M3DNodos = New_Objects((uint32_t)nObj, sizeof(M3DNodo), M3DNODO_CTOR, M3DNODO_DTOR);
	if (bEstatica) {
		M3D_LeeColorEscena("LuzAmbiente:", &esc->ambiente);
	}
	if (bAnim) {
		M3DTabla_ReadInt(&g_M3DTablasEscena[0], "LuzR");
		M3DTabla_ReadInt(&g_M3DTablasEscena[1], "LuzG");
		M3DTabla_ReadInt(&g_M3DTablasEscena[2], "LuzB");
	}
	if (g_M3DVersion > 5) {
		CHECK(!M3D_ReadBool("MapaEntorno:"));
		if (bEstatica) {
			M3D_LeeColorEscena("ColorFondo:", &esc->niebla);
		}
		if (bAnim) {
			M3DTabla_ReadInt(&g_M3DTablasEscena[3], "LuzR");
			M3DTabla_ReadInt(&g_M3DTablasEscena[4], "LuzG");
			M3DTabla_ReadInt(&g_M3DTablasEscena[5], "LuzB");
		}
	}
	CHECK(!g_M3DLMat);
	lm = operator_new(sizeof(CListaMat));
	if (lm) {
		lm->Nombre = NULL;
		lm->vftable = CLISTAMAT_VFTABLE;
		lm->ult = NULL;
		lm->pri = NULL;
	}
	g_M3DLMat = lm;
	nMat = M3D_ReadInt("Materiales:");
	for (i = 0; i < nMat; i++) {
		M3D_LeeMaterial(g_M3DLMat, i, -1);
	}
	if (!bEstatica) {
		if (g_M3DLMat) {
			((CListaMat *(__thiscall *)(CListaMat *, byte))g_M3DLMat->vftable[0])(g_M3DLMat, 1);
			g_M3DLMat = NULL;
		}
		g_M3DNumMat = 0;
	}
	M3D_CompartirMats(g_M3DLMat);
	p = operator_new(sizeof(CNodo3D));
	nodo = p ? CNodo3D_ctor(p) : NULL;
	Cad_Set(&nodo->name, "_raiz_escena");
	nodo->flags |= NODO_RAIZ;
	CEscena_AddNodo(esc, nodo);
	for (i = 0; i < nObj; i++) {
		M3D_LeeObjeto(esc, i, bEstatica, bAnim, castShadows, rcvShadows);
	}
	M3D_NextLine();
	M3D_ReadString(NULL, g_M3DCad, M3D_CAD, 0);
	CHECK(!strcmp(g_M3DCad, "Fin"));
	Lee_Close();
	Escena_MatsUsados(esc);
	Escena_FlagsNombres(esc);
	if (!g_EscenaAnimPadre) {
		Escena_FlagsIni(esc);
		Escena_Jerarquia(esc);
	} else {
		Escena_PadresAnim(esc, g_EscenaAnimPadre);
	}
	root = NULL;
	if (!g_EscenaAnimPadre) {
		root = M3D_RaizModelo(esc, name2);
	}
	Escena_Ordenar(esc);
	Escena_AnimLocal(esc, bAnim);
	if (root) {
		root->flags |= NODO_POSFIJA;
		root->pos.x = 0.0f;
		root->pos.y = 0.0f;
		root->pos.z = 0.0f;
	}
	M3D_Objetivos(esc);
	Escena_MatricesObj(esc);
	Escena_PhysiqueSkin(esc);
	if (g_M3DNodos) {
		Delete_Objects(g_M3DNodos, sizeof(M3DNodo), M3DNODO_DTOR);
		g_M3DNodos = NULL;
	}
	if (g_M3DTablasEscena) {
		M3DTabla *t = g_M3DTablasEscena;

		M3DTabla6_dtor(t);
		operator_delete_sized(t, 6 * sizeof(M3DTabla));
		g_M3DTablasEscena = NULL;
	}
	E3D_LogWhite(g_FileLog, "cleaning nodes (pass %s):", "3");
	CEscena_DeleteEmpty(esc, 0, 0, 1);
	if (g_ShowNodesTree && !g_EscenaAnimPadre) {
		Escena_ShowTree(esc, NULL, 0);
	}
	if (bEstatica && !g_EscenaAnimPadre) {
		Escena_Sprites(esc);
		Escena_Flares(esc);
		Escena_Particles(esc);
	}
	if ((g_OptimizeNodes & 1) && !g_EscenaAnimPadre) {
		M3D_OptimizarNodos(esc);
	}
	for (nodo = esc->Nodo3D; nodo; nodo = nodo->sig) {
		Escena_CheckDuplicados(esc, nodo);
	}
	CEscena_Join(esc, 0, "(+merge_nodes)");
	E3D_LogWhite(g_FileLog, "cleaning nodes (pass %s):", "4");
	CEscena_DeleteEmpty(esc, 0, 0, 1);
	if (g_ShowNodesTree && !g_EscenaAnimPadre) {
		Escena_ShowTree(esc, NULL, 0);
	}
	if (g_EscenaAnimPadre) {
		AnimGrupo_Compact(esc);
		AnimGrupo_AlignNode(g_EscenaAnimPadre, esc, g_EscenaAnimPadre->nodo);
		AnimGrupo_MapNodes(g_EscenaAnimPadre, esc);
		CEscena_DeleteNodes(esc);
	} else if (bAnim) {
		CHECK(bEstatica);
		AnimGrupo_MapNodes(esc, esc);
		E3D_LogVerbose(g_FileLog, "INFO: Model with embedded animation: %s", esc->name);
	}
	Escena_TablaNodos(esc);
	if (g_EscenaLODPadre) {
		CHECK(!g_EscenaAnimPadre);
		CHECK(bEstatica && !bAnim);
		Escena_SetupLOD(esc);
	}
	if (g_EscenaAnimPadre && g_CM3UseScenes) {
		CM3_Save(g_M3DName, esc);
	}
	return esc;
}

/* ---- the static initializers (rva 0x05f6e0, 0x05f720); name and description are the strings of the exe ---- */

#define g_ShowNodesTreeVar ((cGVar *)Game_Ptr(0x4e1ee0))
#define g_UseWSMVar ((cGVar *)Game_Ptr(0x4e1f70))

void LeeM3D_InitShowNodesTree(void) {
	/* "ShowNodesTree", "Show nodes tree at model M3D loading time (0/1)" */
	GVar_InitInt(g_ShowNodesTreeVar, Game_Ptr(0x3e4870), 0, Game_Ptr(0x3e4840), 0);
}

void LeeM3D_InitUseWSM(void) {
	/* "UseWSM", "Use ObjTMAfterWSM (0/1)" */
	GVar_InitInt(g_UseWSMVar, Game_Ptr(0x3e47dc), 0, Game_Ptr(0x3e47c4), 0);
}
