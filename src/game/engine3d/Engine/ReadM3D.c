/*
 * The reader of the M3D files and its value readers, original source Engine/lee_M3D.cpp (include/scrap/readm3d.h).
 */
#include <string.h>

#include "scrap/camera.h"
#include "scrap/console.h"
#include "scrap/animgroup.h"
#include "scrap/animvert.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/flare.h"
#include "scrap/fxtexture.h"
#include "scrap/graphic3d.h"
#include "scrap/readdata.h"
#include "scrap/readm3d.h"
#include "scrap/light.h"
#include "scrap/trimesh.h"
#include "scrap/material.h"
#include "scrap/nodefx.h"
#include "scrap/portal.h"
#include "scrap/render.h"
#include "scrap/scrap.h"
#include "scrap/particlesystem.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

/* the constructors and destructors of the reader data, passed to the new[] and delete[] loops of the CRT */
#define M3DTABLE_CTOR 0x2bba90
#define M3DTABLE_DTOR 0x2bbd20		/* Table_Free */
#define STRING_CTOR 0x2bbab0
#define STRING_DTOR 0x233be0		/* the string freed (Material.cpp) */
#define M3DSKINVERT_DTOR 0x2bc160
#define M3DNODE_DTOR 0x2bbf10

static void M3D_StringFree(char **s) {
	if (*s) {
		String_Set(s, NULL);
	}
}

M3DTable *M3DTable_ctor(M3DTable *self) {
	self->p = NULL;
	self->n = 0;
	self->fixed = 0;
	return self;
}

char **String_ctor(char **self) {
	*self = NULL;
	return self;
}

M3DTrack *M3DTrack_ctor(M3DTrack *self) {
	crt_eh_vector_ctor(&self->pos, sizeof(M3DTable), 4, (void *)M3DTable_ctor, (void *)Table_Free);
	self->sgn.p = NULL;
	self->sgn.n = 0;
	self->sgn.fixed = 0;
	crt_eh_vector_ctor(&self->rot, sizeof(M3DTable), 4, (void *)M3DTable_ctor, (void *)Table_Free);
	crt_eh_vector_ctor(&self->rotScale, sizeof(M3DTable), 4, (void *)M3DTable_ctor, (void *)Table_Free);
	crt_eh_vector_ctor(&self->scale, sizeof(M3DTable), 4, (void *)M3DTable_ctor, (void *)Table_Free);
	return self;
}

M3DNode *M3DNode_ctor(M3DNode *self) {
	self->target = NULL;
	self->side2 = NULL;
	self->skin.p = NULL;
	self->skin.n = 0;
	M3DTrack_ctor(&self->track);
	M3DTrack_ctor(&self->track2);
	for (int i = 0; i < 6; i++) {
		self->t2a4[i].p = NULL;
		self->t2a4[i].n = 0;
		self->t2a4[i].fixed = 0;
	}
	return self;
}

M3DSkinVert *M3DSkinVert_ctor(M3DSkinVert *self) {
	crt_eh_vector_ctor(self->bone, sizeof(char *), SKIN_MAX_BONES, (void *)String_ctor, (void *)String_dtor);
	return self;
}

void M3DSkinVert_dtor(M3DSkinVert *self) {
	crt_eh_vector_dtor(self->bone, sizeof(char *), SKIN_MAX_BONES, (void *)String_dtor);
}

void M3DTable4_dtor(M3DTable4 *self) {
	crt_eh_vector_dtor(self->t, sizeof(M3DTable), 4, (void *)Table_Free);
}

void M3DTrack_dtor(M3DTrack *self) {
	crt_eh_vector_dtor(&self->scale, sizeof(M3DTable), 4, (void *)Table_Free);
	crt_eh_vector_dtor(&self->rotScale, sizeof(M3DTable), 4, (void *)Table_Free);
	crt_eh_vector_dtor(&self->rot, sizeof(M3DTable), 4, (void *)Table_Free);
	Table_Free((CTable *)&self->sgn);
	crt_eh_vector_dtor(&self->pos, sizeof(M3DTable), 4, (void *)Table_Free);
}

void M3DTable6_dtor(M3DTable *self) {
	for (int i = 5; i >= 0; i--) {
		Table_Free((CTable *)&self[i]);
	}
}

void M3DSkin_Free(CTable *self) {
	if (self->p) {
		Delete_Objects(self->p, sizeof(M3DSkinVert), (void *)M3DSkinVert_dtor);
		self->p = NULL;
		self->n = 0;
	}
}

void M3DNode_dtor(M3DNode *self) {
	M3DTable6_dtor(self->t2a4);
	M3DTrack_dtor(&self->track2);
	M3DTrack_dtor(&self->track);
	M3DSkin_Free(&self->skin);
	M3D_StringFree(&self->side2);
	M3D_StringFree(&self->target);
}

/* the deleting destructors: flags 2 the array with its count before it, 1 the memory freed */
M3DNode *M3DNode_Delete(M3DNode *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(M3DNode), *mem, (void *)M3DNode_dtor);
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (M3DNode *)mem;
	}
	M3DNode_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(M3DNode));
	}
	return self;
}

M3DSkinVert *M3DSkinVert_Delete(M3DSkinVert *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(M3DSkinVert), *mem, (void *)M3DSkinVert_dtor);
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
	Read_Bytes(dest, n);
	dest[n] = '\0';
}

byte g_M3DBinary;

byte g_M3DLabels;

char g_M3DString[1024];

void M3D_Label(const char *name) {
	if (!g_M3DBinary) {
		if (g_M3DLabels) {
			Read_Expect(name);
		} else {
			Read_NextLine();
		}
		return;
	}
	if (g_M3DLabels) {
		M3D_ReadBinString(g_M3DString, M3D_STRING);
		CHECK(!strcmp(g_M3DString, name));
	}
}

int M3D_ReadInt(const char *tok) {
	int v = 0, n = 0;

	if (tok) {
		M3D_Label(tok);
	}
	if (!g_M3DBinary) {
		if (crt_sscanf(g_ReadString, "%d%n", &v, &n) != 1) {
			Read_Error("n\xfamero entero no v\xe1lido: %s", g_ReadString);
		}
		g_ReadString += n;
		return v;
	}
	uint32_t u = Read_Byte();
	if ((int)u > 0x3f) {
		uint32_t neg = u & 0x40;
		byte c = u;
		byte shift = 6;
		u &= 0x3f;
		while (c & 0x80) {
			uint32_t b = Read_Byte();
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
		Read_Bytes(&f, sizeof(f));
		return f;
	}
	if (crt_sscanf(g_ReadString, "%f%n", &f, &n) != 1) {
		Read_Error("n\xfamero real no v\xe1lido: %s", g_ReadString);
	}
	g_ReadString += n;
	return f;
}

byte M3D_ReadBool(const char *tok) {
	if (g_M3DBinary) {
		uint32_t v = M3D_ReadInt(tok);
		CHECK(v < 2);
		return v != 0;
	}
	if (tok) {
		Read_Expect(tok);
	}
	if (!crt_stricmp(g_ReadString, "si")) {
		g_ReadString += 2;
		return 1;
	}
	if (!crt_stricmp(g_ReadString, "no")) {
		g_ReadString += 2;
		return 0;
	}
	Read_Error("dato \"bool\" no v\xe1lido: %s", g_ReadString);
	return 0;
}

char *M3D_ReadString(const char *tok, char *dest, int size, byte quoted) {
	CHECK(!dest || size > 0);
	if (tok) {
		M3D_Label(tok);
	}
	if (!dest) {
		dest = g_M3DString;
		size = M3D_STRING;
	}
	if (g_M3DBinary) {
		M3D_ReadBinString(dest, size);
	} else if (quoted) {
		Read_QText(NULL, dest, size);
	} else {
		Read_Text(NULL, dest, size);
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
		CHECK(!strcmp(g_M3DString, name));
	}
	M3D_ReadVec3("MT_Pos:", &mt->pos, 1);
	mt->sign = M3D_ReadFloat("MT_Signo:");
	M3D_ReadQuat("MT_Rot:", &mt->rot, 1);
	M3D_ReadQuat("MT_RotEsc:", &mt->rotScale, 1);
	M3D_ReadVec3("MT_Esc:", &mt->scale, 1);
}

char g_M3DName[260];

void M3D_ReadIndex(int expected) {
	if (!g_M3DBinary) {
		Read_NextLine();
	}
	int i = M3D_ReadInt(NULL);
	if (i != expected) {
		Err_Printf(1, "Indice no v\xe1lido: %d != %d\n'%s'", i, expected, g_M3DName);
	}
	if (!g_M3DBinary) {
		CHECK(*g_ReadString == ':');
		g_ReadString++;
	}
}

cLog g_FileLog;

/* the cache path of name opened and its header checked: the open file positioned after it, NULL when there is none
 * or it is out of date while the M3D can make it again */
static CFileIO *M3D_CacheOpen(const char *path, const char *name, const char *id, int checkTime, const char *other) {
	CFileIO *f = operator_new(sizeof(CFileIO));
	f = f ? CFileIO_ctor(f, path, "rb") : NULL;
	if (f->error) {
		CFileIO_dtor(f);
		operator_delete_sized(f, sizeof(CFileIO));
		return NULL;
	}
	E3D_LogDebug((&g_FileLog), id[0] == 'C' ? "CM3: %s" : "SM3: %s", path);
	if (CFileIO_BeginBlock(f, id)) {
		CheckFailed(id[0] == 'C' ? "f->leer_ini_bloque( \"CM3\" ) == 0" : "f->leer_ini_bloque( \"SM3\" ) == 0",
			__FILE__, __TIMESTAMP__, __LINE__);
	}
	int ver = CFileIO_ReadInt(f);
	int timeM3D = CFileIO_ReadInt(f);
	int timeOther = CFileIO_ReadInt(f);
	if ((checkTime && (File_TimeChanged(timeM3D, name, 2) || File_TimeChanged(timeOther, other, 2))) ||
		ver != M3D_CACHE_VER) {
		if (File_Exists(name)) {
			CFileIO_Finish(f);
			CFileIO_dtor(f);
			operator_delete_sized(f, sizeof(CFileIO));
			return NULL;
		}
		Con_Warnf("WARN: Could not rebuild: %s\n", path);
	}
	return f;
}

/* the scene read from the open cache, the file closed */
static CScene *M3D_CacheRead(CFileIO *f, const char *sceneName, const char *id, byte bAnim, byte fromSM3) {
	CScene *scn = operator_new(sizeof(CScene));
	scn = scn ? CScene_ctor(scn) : NULL;
	String_Set(&scn->name, sceneName);
	if (fromSM3) {
		scn->fromAmc = 1;
	}
	CScene_Read(scn, f, bAnim);
	CFileIO_Error(f, id);
	CHECK(!f->error);
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(CFileIO));
	return scn;
}

CScene *g_SceneAnimParent;

CScene *CM3_Load(const char *name) {
	char path[260];

	CHECK(g_SceneAnimParent);
	Path_Format(path, name, "&U&D&N.CM3");
	if (!File_Exists(path)) {
		return NULL;
	}
	CFileIO *f = M3D_CacheOpen(path, name, "CM3", g_CM3CheckTime, g_SceneAnimParent->name);
	return f ? M3D_CacheRead(f, g_M3DName, "CM3", 1, 0) : NULL;
}

CScene *g_SceneLODParent;

CScene *SM3_Load(const char *name) {
	char path[260], ini[260];

	CHECK(!g_SceneAnimParent);
	if (g_SM3UseScenes != 1) {
		return NULL;
	}
	Path_Format(path, name, "&U&D&N.SM3");
	Path_Format(ini, name, "&U&D&N.INI");
	if (!File_Exists(path)) {
		return NULL;
	}
	CFileIO *f = M3D_CacheOpen(path, name, "SM3", g_SM3CheckTime, ini);
	if (!f) {
		return NULL;
	}
	CScene *scn = M3D_CacheRead(f, name, "SM3", 0, 1);
	if (!g_SceneLODParent) {
		Scene_Particles(scn);
	}
	return scn;
}

/* the cache path of esc written: the header, then the scene */
static void M3D_CacheWrite(const char *path, CScene *scn, const char *id, const char *name, const char *other,
	byte bAnim) {
	E3D_LogVerbose((&g_FileLog), id[0] == 'C' ? "Save_CM3: %s" : "Save_SM3: %s", path);
	CFileIO *f = operator_new(sizeof(CFileIO));
	f = f ? CFileIO_ctor(f, path, "wb") : NULL;
	if (!f->error) {
		CFileIO_BeginBlockWrite(f, id);
		CFileIO_WriteInt(f, M3D_CACHE_VER);
		CFileIO_WriteInt(f, File_Time(name));
		CFileIO_WriteInt(f, File_Time(other));
		CScene_Write(scn, f, bAnim);
		CFileIO_EndBlockWrite(f, id);
		if (!f->error) {
			CFileIO_dtor(f);
			operator_delete_sized(f, sizeof(CFileIO));
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
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(CFileIO));
}

void CM3_Save(const char *name, CScene *scn) {
	char path[260];

	CHECK(g_SceneAnimParent);
	Path_Format(path, name, "&U&D&N.CM3");
	M3D_CacheWrite(path, scn, "CM3", name, g_SceneAnimParent->name, 1);
}

void SM3_Save(const char *name, CScene *scn) {
	char path[260], ini[260];

	CHECK(!g_SceneAnimParent);
	if (scn->fromAmc) {
		CheckFailed("!Escena->ReadFromSM3", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!g_SM3UseScenes) {
		return;
	}
	Path_Format(path, name, "&U&D&N.SM3");
	Path_Format(ini, name, "&U&D&N.INI");
	M3D_CacheWrite(path, scn, "SM3", name, ini, 0);
}

byte g_M3DOk = 1;

void M3D_SetOk(byte ok) {
	if (ok) {
		g_M3DOk = 1;
		return;
	}
	if (g_M3DOk) {
		g_M3DOk = 0;
		E3D_LogVerbose((&g_FileLog), "ERROR: lee_M3D(): '%s'", g_M3DName);
	}
}

void Scene_CheckDuplicates(CScene *scn, CNode3D *node) {
	for (CNode3D *n = scn->Node3D; n;) {
		CNode3D *n2 = CNode3D_Find(n, node->name, 1);
		CHECK(n2);
		if (n2 == node) {
			return;
		}
		E3D_LogVerbose((&g_FileLog), "Duplicated node: '%s' [%s] (%d,%d) [%s] [%s]", node->name, scn->name, node->index,
			n2->index, node->parent ? node->parent->name : "(null)", n2->parent ? n2->parent->name : "(null)");
		if (CNode3D_Class(node) || node->flags & NODE_JOIN2 || CNode3D_Class(n2) || n2->flags & NODE_JOIN2) {
			Con_Warnf("Duplicated node: %s\n", node->name);
		}
		n = n2->next;
	}
}

void Node_MarkBip(CNode3D *node) {
	static const char *const pre[4] = { "Bip ", "Bip_", "BipB", "Bone" };
	for (int i = 0; i < 4; i++) {
		if (!crt_strncmp(node->name, pre[i], 4)) {
			node->flags |= NODE_BIP;
			return;
		}
	}
}

/* the material of the first triangle of the mesh of the node */
static CMaterial *Node_MatTri0(CNode3D *n, CTriMesh **mesh) {
	*mesh = Obj3D_TriMesh(n->obj3D);
	return CTriMesh_GetTri(*mesh, 0)->mat;
}

void Scene_Flares(CScene *scn) {
	if (!scn || !scn->Node3D) {
		return;
	}
	for (CNode3D *n = CNode3D_FindWild(scn->Node3D, "flare*", 1); n; n = CNode3D_FindWild(n, "flare*", 0)) {
		if (!n->obj3D) {
			continue;
		}
		CTriMesh *mt;
		CMaterial *mat = Node_MatTri0(n, &mt);
		CFlare *f = operator_new(sizeof(CFlare));
		f = f ? CFlare_ctor(f) : NULL;
		byte opac = CMaterial_GetMapName(mat, 1) != NULL;
		byte alpha = mat->params.diffuse[3];
		int src = opac ? mat->params.srcBlend : 2, dst = opac ? mat->params.dstBlend : 6;
		const char *op = CMaterial_GetMapName(mat, opac);
		CFlare_SetTexture(f, CMaterial_GetMapName(mat, 0), op, src, dst, alpha);
		f->pos = n->pos;
		CObjBase3D_SetNode((CObjBase3D *)f, n);
		CNode3D_FreeObj3D(n);
		CNode3D_SetObj3D(n, (IObject3D *)f);
		int size;
		if (Node_ParamInt(n->name, "tam", &size)) {
			f->scale[0] = (float)size;
			f->scale[1] = (float)size;
		}
	}
}

void Scene_Sprites(CScene *scn) {
	if (!scn || !scn->Node3D) {
		return;
	}
	for (CNode3D *n = CNode3D_FindWild(scn->Node3D, "sprite*", 1); n; n = CNode3D_FindWild(n, "sprite*", 0)) {
		CTriMesh *mt;
		CMaterial *mat = Node_MatTri0(n, &mt);
		CGraphic3D *g = operator_new(sizeof(CGraphic3D));
		g = g ? CGraphic3D_ctor(g) : NULL;
		const char *op = CMaterial_GetMapName(mat, 1);
		CGraphic3D_CreateMat(g, CMaterial_GetMapName(mat, 0), op, mat->params.srcBlend, mat->params.dstBlend,
			mat->params.diffuse[3]);
		CTriMesh_CalcBox(mt, NULL, 1);
		g->pos = n->pos;
		const box_t *b = &mt->box;
		float dx = b->max.x - b->min.x, dy = b->max.y - b->min.y, dz = b->max.z - b->min.z;
		float r = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f;
		g->width = r;
		g->height = r;
		CGraphic3D_SetNode(g, n);
		CNode3D_FreeObj3D(n);
		CNode3D_SetObj3D(n, (IObject3D *)g);
	}
}

void Scene_Particles(CScene *scn) {
	if (!g_SceneLODParent && !scn->fromAmc) {
		CIniFile *ini = operator_new(sizeof(CIniFile));
		scn->userProp = ini ? CIniFile_ctor(ini) : NULL;
		if (!CIniFile_Load(scn->userProp, scn->name, "model_particles.ini", 0)) {
			E3D_LogDebug((&g_FileLog), "INI: %s: %s", scn->name, "model_particles.ini");
		} else if (scn->userProp) {
			CIniFile_dtor(scn->userProp);
			operator_delete_sized(scn->userProp, sizeof(CIniFile));
			scn->userProp = NULL;
		}
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (!n->ini || !CIniFile_FindSection(n->ini, "Particles", 1)) {
			continue;
		}
		if (n->renderNode != -1) {
			CheckFailed("nodo->iNodeXRef == -1", __FILE__, __TIMESTAMP__, __LINE__);
		}
		if (!g_SceneLODParent) {
			if (n->obj3D) {
				CNode3D_FreeObj3D(n);
				Con_Bluef("ERROR: (Particle System) Node Material not supported (Node: %s)\n", n->name);
			}
			CParticleSystem *sp = ParticleSystem_FromNode(n, scn->userProp);
			if (!sp) {
				CheckFailed("sistpart", __FILE__, __TIMESTAMP__, __LINE__);
			}
			CNode3D_SetObj3D(n, (IObject3D *)sp);
			continue;
		}
		CNode3D *n2 = CScene_FindNode(g_SceneLODParent, n->name);
		if (!n2) {
			E3D_LogBlue((&g_FileLog), "ERROR: LOD particles node not found in main LOD: %s", n->name);
			continue;
		}
		if (CNode3D_Class(n2) != CLASS_OBJ3D_PARTICLESYSTEM) {
			CheckFailed("nodo2->clase_obj3D() == CLASE_OBJ3D_SISTPART", __FILE__, __TIMESTAMP__, __LINE__);
		}
		n->renderNode = n2->slot;
		if (n->obj3D) {
			CheckFailed("!nodo->obj3D", __FILE__, __TIMESTAMP__, __LINE__);
		}
		n->flags |= 0x80;
		n->index = -1;
	}
}

void Scene_Clear(CScene *scn, const char *pass) {
	E3D_LogWhite((&g_FileLog), "cleaning nodes (pass %s):", pass);
	CScene_DeleteEmpty(scn, 0, 0, 1);
	if (g_ShowNodesTree && !g_SceneAnimParent) {
		Scene_ShowTree(scn, 0, 0);
	}
}

byte Float_AlmostEqual(float a, float b) {
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

byte Node_IsAncestor(const CNode3D *node, const CNode3D *prev, byte cross) {
	CHECK(node && prev);
	for (const CNode3D *p = node->parent; p; p = p->parent) {
		if (p == prev) {
			return 1;
		}
		if (!cross && p->flags & NODE_DELETE) {
			return 0;
		}
	}
	return 0;
}

byte Node_IsOfGame(const CNode3D *node) {
	static const struct {
		const char *pre;
		int n;
	} pre[8] = { { "DR_", 3 }, { "DM_", 3 }, { "DC_", 3 }, { "DF_", 3 }, { "D_Cam-", 6 }, { "Actor_", 6 },
		{ "Traffic_", 8 }, { "NAVEBERTO", 10 } };
	for (int i = 0; i < 8; i++) {
		if (!crt_strnicmp(node->name, pre[i].pre, pre[i].n)) {
			return 1;
		}
	}
	return 0;
}

byte Node_ParamInt(const char *name, const char *key, int *out) {
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

void Node_ClearName(char *name) {
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

byte g_M3DUseOpacity;

byte g_M3DUseEnvmap;

byte g_M3DUseBumpmap;

byte g_M3DUseGlowmap;

int g_UseEnvMap = 1;

int g_UseBumpMap = 1;

int g_UseGlowMap = 1;

byte M3D_ActiveMap(const CMaterial *mat, int mapid) {
	if (mat->params.attrib & MAT_SHADER) {
		return 1;
	}
	switch (mapid) {
	case MAP_OPACITY:
		return g_M3DUseOpacity;
	case MAP_ENVIRONMENT:
		return g_UseEnvMap && g_M3DUseEnvmap;
	case MAP_BUMP:
		return g_UseBumpMap && g_M3DUseBumpmap;
	case MAP_GLOW:
		return g_UseGlowMap && g_M3DUseGlowmap;
	default:
		return 1;
	}
}


int g_M3DNumMat;

CMaterial *g_M3DMat[512];

int g_M3DMatId[512];

int g_M3DMatSub[512];

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
				while (res->params.matClass == 1 && res->sub) {
					res = res->sub->first;
					Con_FatalErrorf("Hay un multimaterial dentro de otro multimaterial: %s", res->Name);
				}
			}
		}
		return res;
	}
	CMaterial *res = NULL;
	if (iSubmat != -1) {
		res = M3D_FindMat(iMat, -1);
		if (res && res->sub) {
			int num = CMatList_Count(res->sub, 0);
			CHECK(num > 0);
			if (iSubmat < num) {
				return NULL;
			}
			res = M3D_FindMat(iMat, iSubmat % num);
		}
	}
	return res;
}

byte g_M3DMatUsado[512];

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

void M3D_ShareMats(CMatList *list) {
	for (int i = 0; i < g_M3DNumMat; i++) {
		if (g_M3DMat[i]->params.matClass == 1) {
			continue;
		}
		CHECK(!g_M3DMat[i]->sub);
		CMaterial *mat = CMatList_Find(list, g_M3DMat[i]);
		CHECK(!mat->sub);
		if (g_M3DMat[i] != mat) {
			E3D_LogDebug((&g_FileLog), "WARN: Duplicated material: [%s]->[%s] [%s]", g_M3DMat[i]->Name, mat->Name,
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

void M3D_ReadColors(CMaterial *mat) {
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
		Read_Bytes(dest, sizeof(*dest));
		return;
	}
	if (crt_sscanf(g_ReadString, "%f%n", dest, &n) != 1) {
		Read_Error("n\xfamero real no v\xe1lido: %s", g_ReadString);
	}
	g_ReadString += n;
}


M3DNode *g_M3DNodes;

/* the objects of the file and their animation tables */

M3DNode *M3D_NodeDat(const CNode3D *node) {
	CHECK(g_M3DNodes && node);
	CHECK(node->index >= 0);
	return &g_M3DNodes[node->index];
}

vec3_t *M3DTable4_GetVec(M3DTable4 *self, vec3_t *out, uint32_t i) {
	TABLE_CHECK(self->t[2].p, i, self->t[2].n);
	const float *z = (const float *)self->t[2].p + i;
	TABLE_CHECK(self->t[1].p, i, self->t[1].n);
	const float *y = (const float *)self->t[1].p + i;
	TABLE_CHECK(self->t[0].p, i, self->t[0].n);
	out->x = ((const float *)self->t[0].p)[i];
	out->y = *y;
	out->z = *z;
	return out;
}

quat_t *M3DTable4_GetQuat(M3DTable4 *self, quat_t *out, uint32_t i) {
	TABLE_CHECK(self->t[3].p, i, self->t[3].n);
	const float *w = (const float *)self->t[3].p + i;
	TABLE_CHECK(self->t[2].p, i, self->t[2].n);
	const float *z = (const float *)self->t[2].p + i;
	TABLE_CHECK(self->t[1].p, i, self->t[1].n);
	const float *y = (const float *)self->t[1].p + i;
	TABLE_CHECK(self->t[0].p, i, self->t[0].n);
	out->x = ((const float *)self->t[0].p)[i];
	out->y = *y;
	out->z = *z;
	out->w = *w;
	return out;
}

void M3DTable4_SetVec(M3DTable4 *self, uint32_t i, const vec3_t *v) {
	TABLE_CHECK(self->t[0].p, i, self->t[0].n);
	((float *)self->t[0].p)[i] = v->x;
	TABLE_CHECK(self->t[1].p, i, self->t[1].n);
	((float *)self->t[1].p)[i] = v->y;
	TABLE_CHECK(self->t[2].p, i, self->t[2].n);
	((float *)self->t[2].p)[i] = v->z;
}

void M3DTable4_RotToQuat(M3DTable4 *self, uint32_t i) {
	vec3_t v, axis;
	quat_t q;

	TABLE_CHECK(self->t[3].p, i, self->t[3].n);
	const float *ang = (const float *)self->t[3].p + i;
	const vec3_t *r = M3DTable4_GetVec(self, &v, i);
	axis.x = -r->x;
	axis.y = -r->y;
	axis.z = -r->z;
	Quat_FromAxisAngle(&q, &axis, *ang);
	v.x = q.x;
	v.y = q.y;
	v.z = q.z;
	M3DTable4_SetVec(self, i, &v);
	TABLE_CHECK(self->t[3].p, i, self->t[3].n);
	((float *)self->t[3].p)[i] = q.w;
}

matrix_t *M3DNode_FrameMatrix(M3DNode *self, matrix_t *out, uint32_t frame) {
	m3dTransform_t mt;
	vec3_t v;
	quat_t q;

	mt.pos = *M3DTable4_GetVec(&self->track.pos, &v, frame);
	mt.rot = *M3DTable4_GetQuat(&self->track.rot, &q, frame);
	mt.rotScale = *M3DTable4_GetQuat(&self->track.rotScale, &q, frame);
	mt.scale = *M3DTable4_GetVec(&self->track.scale, &v, frame);
	TABLE_CHECK(self->track.sgn.p, frame, self->track.sgn.n);
	mt.sign = ((const float *)self->track.sgn.p)[frame];
	M3DTransform_ToMatrixAnim(&mt, out);
	return out;
}

/* a value of a table: an int or the bits of a float */
static void M3DTable_ReadValue(uint32_t *dest, byte flt) {
	if (flt) {
		M3D_ReadFloatTo((float *)dest);
	} else {
		*dest = (uint32_t)M3D_ReadInt(NULL);
	}
}

int g_M3DFrames;

/* an "Animacion:" table of the frames: runs of values (count, values; 20 a line in text) or of one value (-count,
 * value); a single run over all frames makes it fixed */
static void M3DTable_Read(M3DTable *self, const char *name, byte flt) {
	M3D_ReadString("Animacion:", NULL, 0, 0);
	CHECK(!strcmp(g_M3DString, name));
	CHECK(M3D_ReadInt("Tabla:") == 1);
	Table_Alloc4((CTable *)self, g_M3DFrames);
	int fr = 0;
	if (self->n <= 0) {
		return;
	}
	do {
		if (!g_M3DBinary) {
			Read_NextLine();
		}
		int num = M3D_ReadInt(NULL);
		int end;
		if (num < 0) {
			uint32_t v;
			num = -num;
			if (!fr && num == self->n) {
				self->fixed = 1;
			}
			M3DTable_ReadValue(&v, flt);
			end = num + fr;
			CHECK(end <= self->n);
			for (int i = 0; i < num; i++) {
				TABLE_CHECK(self->p, fr + i, self->n);
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
						Read_NextLine();
					}
					k = 0;
				}
				TABLE_CHECK(self->p, fr + i, self->n);
				uint32_t *p = (uint32_t *)self->p + fr + i;
				M3DTable_ReadValue(&v, flt);
				*p = v;
			}
		}
		fr = end;
	} while (fr < self->n);
}

void M3DTable_ReadInt(M3DTable *self, const char *name) {
	M3DTable_Read(self, name, 0);
}

void M3DTable_ReadFloat(M3DTable *self, const char *name) {
	M3DTable_Read(self, name, 1);
}

void M3DTable4_Read(M3DTable4 *self, const char *n1, const char *n2, const char *n3, const char *n4,
	byte inv23) {
	CHECK(!inv23 || (n2 && n3));
	if (n1) {
		M3DTable_ReadFloat(&self->t[0], n1);
	}
	if (n2) {
		M3DTable_ReadFloat(&self->t[inv23 ? 2 : 1], n2);
	}
	if (n3) {
		M3DTable_ReadFloat(&self->t[inv23 ? 1 : 2], n3);
	}
	if (n4) {
		M3DTable_ReadFloat(&self->t[3], n4);
	}
}

void M3DTrack_Read(M3DTrack *self) {
	M3DTable_ReadFloat(&self->pos.t[0], "PosX");
	M3DTable_ReadFloat(&self->pos.t[2], "PosY");
	M3DTable_ReadFloat(&self->pos.t[1], "PosZ");
	M3DTable_ReadFloat(&self->sgn, "Sgn");
	M3DTable_ReadFloat(&self->rot.t[0], "QRotX");
	M3DTable_ReadFloat(&self->rot.t[2], "QRotY");
	M3DTable_ReadFloat(&self->rot.t[1], "QRotZ");
	M3DTable_ReadFloat(&self->rot.t[3], "QRotA");
	M3DTable_ReadFloat(&self->rotScale.t[0], "URotX");
	M3DTable_ReadFloat(&self->rotScale.t[2], "URotY");
	M3DTable_ReadFloat(&self->rotScale.t[1], "URotZ");
	M3DTable_ReadFloat(&self->rotScale.t[3], "URotA");
	M3DTable_ReadFloat(&self->scale.t[0], "EscX");
	M3DTable_ReadFloat(&self->scale.t[2], "EscY");
	M3DTable_ReadFloat(&self->scale.t[1], "EscZ");
	for (int i = 0; i < g_M3DFrames; i++) {
		M3DTable4_RotToQuat(&self->rot, (uint32_t)i);
		M3DTable4_RotToQuat(&self->rotScale, (uint32_t)i);
	}
}


/* the nodes */

void Node_FlagName(CNode3D *node, const char *on, const char *off, uint32_t flag) {
	if (on && strstr(node->name, on)) {
		node->flags |= flag;
		return;
	}
	if (off && strstr(node->name, off)) {
		node->flags &= ~flag;
		return;
	}
	if (node->parent && node->parent->flags & flag) {
		node->flags |= flag;
	}
}

void Scene_FlagNames(CScene *scn) {
	static const struct {
		const char *on, *off;
		uint32_t flag;
	} tab[8] = { { "(+collide)", "(-collide)", 0x1000 }, { "(+no_collide)", "(-no_collide)", 0x2000 },
		{ "(+rt_lighting)", "(-rt_lighting)", 0x2000000 }, { "(+rt_shadows)", "(-rt_shadows)", 0x4000000 },
		{ "(+no_lightmap)", "(-no_lightmap)", 0x8000000 }, { "(+no_sector)", "(-no_sector)", 0x10000000 },
		{ "(+area_light)", "(-area_light)", 0x20000000 }, { "(+dudv_pass)", "(-dudv_pass)", 0x40000000 } };
	char type[32];

	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		for (int i = 0; i < 8; i++) {
			Node_FlagName(n, tab[i].on, tab[i].off, tab[i].flag);
		}
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (strstr(n->name, "(+ilum_dinam)")) {
			n->flags |= 0x2000000;
		}
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (!n->ini) {
			continue;
		}
		if (CIniFile_GetInt(n->ini, "Animation", "slerp", 1) == 2) {
			n->flags |= NODE_SLERP2;
		}
		if (!CIniFile_GetText(n->ini, "NodeEffect", "Type", type, sizeof(type), NULL)) {
			continue;
		}
		n->flags |= NODE_FX;
		if (!g_SceneLODParent) {
			n->obj58 = NodeFX_New(type, n);
		} else {
			E3D_LogWhite((&g_FileLog), "INFO: LOD Node effect skipped in node: %s", n->name);
		}
	}
}

int g_M3DVersion;

void Scene_FlagsIni(CScene *scn) {
	if (g_M3DVersion <= 12) {
		return;
	}
	CIniFile *ini = g_SceneLODParent ? g_SceneLODParent->ini : scn->ini;
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (!ini) {
			continue;
		}
		if (CIniFile_GetInt(ini, "SlerpNodes", n->name, 1) == 2) {
			E3D_LogInfo((&g_FileLog), "INFO: slerp2 node: %s", n->name);
			n->flags |= NODE_SLERP2;
		}
		if (CIniFile_GetInt(ini, "MorphNodes", n->name, 0) && !g_SceneLODParent) {
			E3D_LogInfo((&g_FileLog), "INFO: morph node: %s", n->name);
			n->flags |= 0x800000;
		}
		if (CIniFile_GetInt(ini, "HideNodes", n->name, 0)) {
			E3D_LogInfo((&g_FileLog), "INFO: hidding node: %s", n->name);
			n->flags |= NODE_DELETE;
		}
		if (CIniFile_GetInt(ini, "UnhideNodes", n->name, 0)) {
			E3D_LogInfo((&g_FileLog), "INFO: unhidding node: %s", n->name);
			n->flags &= ~NODE_DELETE;
		}
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (n->index < 0) {
			continue;
		}
		M3DNode *dat = M3D_NodeDat(n);
		if (!dat->skin.n || CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			continue;
		}
		for (int i = 0; i < dat->skin.n; i++) {
			TABLE_CHECK(dat->skin.p, i, dat->skin.n);
			const M3DSkinVert *sv = (const M3DSkinVert *)dat->skin.p + i;
			for (int k = 0; k < sv->NumBones; k++) {
				CNode3D *bone = CNode3D_Find(scn->Node3D, sv->bone[k], 1);
				CHECK(bone && (uint32_t)bone->index < 256);
				if (bone->flags & NODE_DELETE) {
					Con_Warnf("WARN: Hidden bone: %s [unhidden]\n", bone->name);
					bone->flags &= ~NODE_DELETE;
				}
			}
		}
	}
}

CNode3D *Node_FindDescendant(CNode3D *node, const CNode3D *ancestor) {
	for (; node; node = node->next) {
		if (node->flags & NODE_DELETE) {
			continue;
		}
		CHECK(ancestor);
		for (const CNode3D *p = node->parent; p; p = p->parent) {
			if (p == ancestor) {
				return node;
			}
			if (p->flags & NODE_DELETE) {
				break;
			}
		}
	}
	return NULL;
}

CNode3D *Node_FindDescendantPrev(CNode3D *node, const CNode3D *ancestor) {
	CHECK(node && ancestor);
	for (CNode3D *n = node->prev; n; n = n->prev) {
		if (n->flags & NODE_DELETE) {
			continue;
		}
		CHECK(ancestor);
		for (const CNode3D *p = n->parent; p; p = p->parent) {
			if (p == ancestor) {
				return n;
			}
		}
	}
	return NULL;
}

CNode3D *Scene_FindCollapsible(CNode3D *node, const CNode3D *parent) {
	CIniFile *ini = g_SceneLODParent ? g_SceneLODParent->ini : node->scene->ini;
	CNode3D *res = NULL;

	for (; node; node = node->next) {
		if (node->parent != parent || CNode3D_Class(node) != CLASS_OBJ3D_TRIMESH || node->flags & 0xbd1088) {
			continue;
		}
		if (node->Refs - (node->prev != NULL) - (node->next != NULL) || node->ini) {
			continue;
		}
		if (ini && CIniFile_GetInt(ini, "SuperNodes", node->name, 0)) {
			continue;
		}
		if (!g_SceneLODParent) {
			return node;
		}
		res = node;
		if (!CScene_FindNode(g_SceneLODParent, node->name)) {
			return node;
		}
	}
	return res;
}

CMatList *g_M3DLMat;

void Scene_UsedMats(CScene *scn) {
	CTable *mats = &scn->mats;
	int included = 0;

	CHECK(!mats->p);
	mats->n = M3D_MAX_MAT_SCENE;
	mats->p = New_Array(M3D_MAX_MAT_SCENE, sizeof(CMaterial *));
	CHECK(mats->p);
	for (int i = 0; i < g_M3DNumMat; i++) {
		CMaterial *mat = g_M3DMat[i];
		if (!g_M3DMatUsado[i]) {
			const char *type = mat->params.matClass == 1 ? "multimat" : "material";
			E3D_LogDebug((&g_FileLog), "WARN: Unused %s: [%s] [%s]", type, mat->Name, CMaterial_GetMapName(mat, 0));
			continue;
		}
		mat = CMatList_Extract(g_M3DLMat, mat, 1);
		if (!mat) {
			continue;
		}
		CHECK(!mat->sub);
		CHECK(included < M3D_MAX_MAT_SCENE);
		mat->params.attrib |= MAT_NOSHARE;
		CMatList_Add((&g_MatList), mat);
		TABLE_CHECK(mats->p, included, mats->n);
		((CMaterial **)mats->p)[included] = mat;
		included++;
	}
	if (g_M3DLMat) {
		E3D_LogWhite((&g_FileLog), "Scene materials: %d included, %d deleted", included, CMatList_Count(g_M3DLMat, 1));
		if (g_M3DLMat) {
			g_M3DLMat->vftable->Delete(g_M3DLMat, 1);
			g_M3DLMat = NULL;
		}
	}
	Table_ResizeN(mats, included, sizeof(CMaterial *));
}


/* the objects: cameras, lights, portals (anim: the values of a frame of an animation file) */

/* the target name ("NombreObjetivo:") of a camera or light into the reader data */
static void M3D_ReadTarget(CNode3D *node) {
	M3D_ReadString("NombreObjetivo:", NULL, 0, 1);
	Node_ClearName(g_M3DString);
	String_Set(&M3D_NodeDat(node)->target, g_M3DString);
}

void M3D_ReadCamera(CNode3D *node, byte anim, int frame) {
	if (anim) {
		CHECK(CNode3D_Class(node) == CLASS_OBJ3D_CAMERA);
		float fov = M3D_ReadFloat("FOV:");
		M3DNode *dat = M3D_NodeDat(node);
		CHECK(dat->AnimNode);
		CAnimNode *an = dat->AnimNode;
		int i = CAnimNode_FrameIndex(an, frame, an->FOV, ANIM_FOV);
		an->FOV[i] = fov;
		return;
	}
	CCamera *cam = operator_new(sizeof(CCamera));
	cam = cam ? CCamera_ctor(cam) : NULL;
	CNode3D_SetObj3D(node, (IObject3D *)cam);
	vec3_t dir = { 0.0f, -1.0f, 0.0f };
	matrix_t m;
	Quat_ToMatrixD3D(&m, &node->rot);
	Vec3_TransformDir0(&dir, &dir, &m);
	CCamera_SetNode(cam, node);
	cam->pos = node->pos;
	cam->target.x = dir.x + node->pos.x;
	cam->target.y = node->pos.y + dir.y;
	cam->target.z = node->pos.z + dir.z;
	if (M3D_ReadBool("TieneObjetivo:")) {
		M3D_ReadTarget(node);
	}
	cam->fov = M3D_ReadFloat("FOV:");
	if (M3D_ReadBool("InfoRecorte:")) {
		cam->zNear = M3D_ReadFloat("RecorteCercano:");
		cam->zFar = M3D_ReadFloat("RecorteLejano:");
	}
	cam->rangeNear = M3D_ReadFloat("RangoCerca:");
	cam->rangeFar = M3D_ReadFloat("RangoLejos:");
	M3D_ReadFloat("DistObjetivo:");
	if ((double)(cam->zFar / cam->zNear) > 1000000.0) {
		E3D_LogDebug((&g_FileLog), "WARN: Invalid camera clipping range: %s", node->name);
		cam->zNear = 1.0f;
		cam->zFar = 100000.0f;
	}
}

void M3D_ReadLight(CNode3D *node, byte anim, int frame) {
	vec3_t c;

	if (anim) {
		CHECK(CNode3D_Class(node) == CLASS_OBJ3D_LIGHT);
		M3D_ReadVec3("Color:", &c, 0);
		float intens = M3D_ReadFloat("Intens:");
		M3DNode *dat = M3D_NodeDat(node);
		CHECK(dat->AnimNode);
		CAnimNode *an = dat->AnimNode;
		uint32_t color = M3D_Color(&c);
		int i = CAnimNode_FrameIndex(an, frame, an->Color, ANIM_COLOR);
		((uint32_t *)an->Color)[i] = color;
		an = dat->AnimNode;
		i = CAnimNode_FrameIndex(an, frame, an->Intens, ANIM_INTENS);
		an->Intens[i] = intens;
		return;
	}
	CLight *light = operator_new(sizeof(CLight));
	light = light ? CLight_ctor(light) : NULL;
	CNode3D_SetObj3D(node, (IObject3D *)light);
	M3DNode *dat = M3D_NodeDat(node);
	CLight_SetNode(light, node);
	light->pos = node->pos;
	Vec3_TransformDir0(&light->dir, &light->dir, &dat->mNode);
	M3D_ReadString("TipoLuz:", NULL, 0, 0);
	if (!strcmp(g_M3DString, "omni")) {
		light->type = LIGHT_OMNI;
	} else if (!strcmp(g_M3DString, "tspot")) {
		light->type = LIGHT_SPOT;
	} else if (!strcmp(g_M3DString, "dir")) {
		light->type = LIGHT_DIR;
	} else if (!strcmp(g_M3DString, "fspot")) {
		light->type = LIGHT_SPOT;
	} else if (!strcmp(g_M3DString, "tdir")) {
		light->type = LIGHT_DIR;
	} else {
		Err_Printf(1, "Tipo de luz no v\xe1lido: '%s'", g_M3DString);
	}
	if (M3D_ReadBool("TieneObjetivo:")) {
		M3D_ReadTarget(node);
	}
	M3D_ReadString("Sombra:", NULL, 0, 0);
	light->shadow = strcmp(g_M3DString, "Off") != 0;
	CLight_SetActive(light, M3D_ReadBool("Activa:"));
	M3D_ReadBool("Rebasar:");
	M3D_ReadVec3("Color:", &c, 0);
	uint32_t color = M3D_Color(&c);
	float intens = M3D_ReadFloat("Intens:");
	*(uint32_t *)light->color = color;
	light->intensity = intens;
	if (light->type != LIGHT_OMNI) {
		light->theta = M3D_ReadFloat("Hotspot:");
		light->phi = M3D_ReadFloat("Falloff:");
	}
	if (M3D_ReadBool("Atenuacion:")) {
		float scale = (dat->mt.scale.y + dat->mt.scale.x + dat->mt.scale.z) * 0.33333334f;
		light->rangeStart = M3D_ReadFloat("AtenIni:") * scale;
		light->range = M3D_ReadFloat("AtenFin:") * scale;
	}
	int v;
	if (Node_ParamInt(node->name, "mult", &v)) {
		light->mult = (float)v / 100.0f;
	}
	if (Node_ParamInt(node->name, "radcoef", &v)) {
		light->radCoef = (float)v / 100.0f;
	}
	if (light->type != LIGHT_DIR && 0.0f >= light->range) {
		Con_Warnf("  WARN: No End value in Far Attenuation [Light: %s]\n", node->name);
	}
}

void M3D_ReadPortal(CNode3D *node, byte anim) {
	if (anim) {
		return;
	}
	CPortal *p = operator_new(sizeof(CPortal));
	p = p ? CPortal_ctor(p) : NULL;
	CNode3D_SetObj3D(node, (IObject3D *)p);
	M3DNode *dat = M3D_NodeDat(node);
	CObjBase3D_SetNode((CObjBase3D *)p, node);
	CHECK(M3D_ReadInt("Version:") == 1);
	p->width = M3D_ReadFloat("Ancho:");
	p->height = M3D_ReadFloat("Alto:");
	vec3_t v = { p->width, 0.0f, 0.0f }, t;
	Vec3_TransformDir0(&t, &v, &dat->mNode);
	p->width = (float)__builtin_sqrt((double)(t.y * t.y + t.x * t.x + t.z * t.z));
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = p->height;
	Vec3_TransformDir0(&t, &v, &dat->mNode);
	p->height = (float)__builtin_sqrt((double)(t.y * t.y + t.x * t.x + t.z * t.z));
	M3D_ReadString("Lado1:", NULL, 0, 1);
	Node_ClearName(g_M3DString);
	String_Set(&M3D_NodeDat(node)->target, g_M3DString);
	M3D_ReadString("Lado2:", NULL, 0, 1);
	Node_ClearName(g_M3DString);
	String_Set(&M3D_NodeDat(node)->side2, g_M3DString);
	if (0.0f >= p->width || 0.0f >= p->height) {
		M3D_SetOk(0);
		E3D_LogBlue((&g_FileLog), "  WARN: Portal: '%s'  Width: %f  Height: %f", node->name, (double)p->width,
			(double)p->height);
	}
}


/* the scene after the objects are read */

/* CHECK of a decomposed node matrix: no scale, no mirror */
static void M3D_CheckNoScale(const m3dTransform_t *da) {
	CHECK((double)1e-5f > __builtin_fabs((double)(da->scale.x - 1.0f)) &&
		(double)1e-5f > __builtin_fabs((double)(da->scale.y - 1.0f)) &&
		(double)1e-5f > __builtin_fabs((double)(da->scale.z - 1.0f)));
	CHECK(da->sign == 1.0f);
}

matrix_t *M3DNode_CalcMatrix(M3DNode *self) {
	return M3DTransform_ToMatrixAnim(&self->mt, &self->mNode);
}

int M3D_NextLine(void) {
	return g_M3DBinary ? 0 : Read_NextLine();
}

void Scene_MatricesObj(CScene *scn) {
	m3dTransform_t da;
	matrix_t inv;

	CScene_UpdateNodes(scn);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		Matrix_ToTransform(&n->m, &da);
		M3D_CheckNoScale(&da);
		n->qLook = da.rot;
		CHECK(Matrix_Invert(&inv, &n->m) == 0);
		n->mObj = inv;
	}
}

int g_M3DFirstFrame;

int g_M3DLastFrame;

void Scene_AnimLocal(CScene *scn, byte anim) {
	m3dTransform_t da;
	matrix_t inv, tmp, loc, mp, mn;
	vec3_t v, pos;
	quat_t q, rot;

	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		CNode3D *parent = n->parent;
		CNode3D_SetParent(n, NULL);
		CNode3D_Update(n, NULL);
		CNode3D_SetParent(n, parent);
		if (!parent) {
			continue;
		}
		CHECK(Matrix_Invert(&inv, &parent->m) == 0);
		loc = *Matrix_MulD3D(&tmp, &n->m, &inv);
		Matrix_ToTransform(&loc, &da);
		n->pos = da.pos;
		n->rot = da.rot;
		M3D_CheckNoScale(&da);
		CNode3D_Update(n, NULL);
		if (!anim) {
			continue;
		}
		M3DNode *dat = M3D_NodeDat(n);
		CHECK(dat->AnimNode);
		uint32_t flags = CAnimNode_GetFlags(dat->AnimNode);
		if (parent->index >= 0) {
			CAnimNode_GetFlags(M3D_NodeDat(parent)->AnimNode);
		}
		for (int frame = g_M3DFirstFrame; frame <= g_M3DLastFrame; frame++) {
			uint32_t i = (uint32_t)(frame - g_M3DFirstFrame);
			if (parent->index == -1) {
				mp = parent->m;
			} else {
				pos = *M3DTable4_GetVec(&M3D_NodeDat(parent)->track.pos, &v, i);
				rot = *M3DTable4_GetQuat(&M3D_NodeDat(parent)->track.rot, &q, i);
				Quat_ToMatrixD3D(&mp, &rot);
				Matrix_AddPos(&mp, &pos);
			}
			pos = *M3DTable4_GetVec(&dat->track.pos, &v, i);
			rot = *M3DTable4_GetQuat(&dat->track.rot, &q, i);
			Quat_ToMatrixD3D(&mn, &rot);
			Matrix_AddPos(&mn, &pos);
			CHECK(Matrix_Invert(&inv, &mp) == 0);
			loc = *Matrix_MulD3D(&tmp, &mn, &inv);
			Matrix_ToTransform(&loc, &da);
			if (flags & ANIM_POS) {
				CAnimNode *an = dat->AnimNode;
				int k = CAnimNode_FrameIndex(an, frame, an->Pos, ANIM_POS);
				an->Pos[k] = da.pos;
			}
			if (flags & ANIM_ROT) {
				CAnimNode *an = dat->AnimNode;
				int k = CAnimNode_FrameIndex(an, frame, an->Rot, ANIM_ROT);
				an->Rot[k] = da.rot;
			}
			M3D_CheckNoScale(&da);
		}
	}
}

void Scene_CollapseBip(CScene *scn) {
	matrix_t tmp, m;

	CScene_UpdateNodes(scn);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (n->index < 0 || !(n->flags & NODE_BIP) || n->obj3D) {
			continue;
		}
		CHECK(CNode3D_Class(n) == 0);
		CNode3D *n2 = Scene_FindCollapsible(scn->Node3D, n);
		if (!n2) {
			continue;
		}
		E3D_LogInfo((&g_FileLog), "BIP: Colapsed node: %s -> %s [f:%08X]", n2->name, n->name, n2->flags);
		CHECK(!(n->flags & 0xc30000));
		CHECK(!(n2->flags & 0xc30000));
		if (n2->flags & 0xfffffe8b) {
			E3D_LogBlue((&g_FileLog), "WARN: Colapsed node: %s [f:%08X]", n2->name, n2->flags);
		}
		CNode3D_SetObj3D(n, n2->obj3D);
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
		CTriMesh *mesh = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
		if (!mesh->pSkin) {
			m = *Matrix_MulD3D(&tmp, &n2->m, &n->mObj);
			CTriMesh_Transform(mesh, &m);
		}
	}
}

void Scene_SkinMesh(CScene *scn) {
	CNode3D *n = scn->Node3D;

	for (; n; n = n->next) {
		if (n->index >= 0 && M3D_NodeDat(n)->skin.n) {
			break;
		}
	}
	if (!n) {
		return;
	}
	CTriMesh *all = operator_new(sizeof(CTriMesh));
	all = all ? CTriMesh_ctor(all) : NULL;
	CScene_UpdateNodes(scn);
	for (n = scn->Node3D; n; n = n->next) {
		if (n->flags & NODE_HIDE || CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			continue;
		}
		CTriMesh *mesh = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
		if (!mesh->pSkin) {
			CTriMesh_Transform(mesh, &n->m);
			if (!mesh->pSkin) {
				CTriMesh_AllocSkin(mesh);
				for (int i = 0; i < mesh->nVert; i++) {
					byte *s = CTriMesh_GetSkin(mesh, (uint32_t)i);
					s[0] = 1;
					CHECK((uint32_t)n->index < 256);
					n->flags |= NODE_BONE;
					s[1] = (byte)n->index;
					*(float *)(s + 4) = 1.0f;
				}
			}
		}
		CHECK(mesh->pTri);
		CTriMesh_Add(all, mesh, 0, 0);
		CNode3D_FreeObj3D(n);
		if (!(n->flags & NODE_BONE)) {
			n->flags |= 0x420008;
		}
	}
	if (all->nTri && all->nVert) {
		CNode3D *node = operator_new(sizeof(CNode3D));
		node = node ? CNode3D_ctor(node) : NULL;
		String_Set(&node->name, "SkinMesh");
		CNode3D_SetObj3D(node, (IObject3D *)all);
		CScene_AddNode(scn, node);
		return;
	}
	all->vftable->Delete(&all->base, 1);
}

void Scene_NodeTable(CScene *scn) {
	int k = 0;

	CScene_NumNodes(scn);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (!(n->flags & NODE_NO_RENDERNODE)) {
			n->slot = k++;
		}
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (n->flags & NODE_NO_RENDERNODE) {
			n->slot = k++;
		}
	}
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		CHECK(CNode3D_Class(n) != CLASS_OBJ3D_MESHD3D);
		if (CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			continue;
		}
		CTriMesh *mesh = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
		if (!mesh->pSkin) {
			continue;
		}
		for (int v = 0; v < mesh->nVert; v++) {
			byte *s = CTriMesh_GetSkin(mesh, (uint32_t)v);
			if (!s[0]) {
				continue;
			}
			byte *p = s + 1;
			do {
				CNode3D *bone = CScene_NodeObject(scn, *p);
				CHECK(bone && (uint32_t)bone->slot < 256);
				CHECK(bone->flags & NODE_BONE);
				*p++ = (byte)bone->slot;
			} while ((int)(p - s - 1) < (int)s[0]);
		}
	}
	if (g_SceneAnimParent || g_SceneLODParent) {
		return;
	}
	CIniFile *ini = scn->ini;
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		uint32_t mask = 0xffffffff;
		int v = ini ? CIniFile_GetInt(ini, "AnimNodes", n->name, -1) : -1;
		if (g_OptimizeNodes & 4) {
			if (v == 0) {
				mask = 0;
				E3D_LogWhite((&g_FileLog), "AniMask: IniValue %d: %s (node animation discarded)", 0, n->name);
			} else if (v > 0) {
				E3D_LogWhite((&g_FileLog), "AniMask: IniValue %d: %s (node animation NOT discarded)", v, n->name);
			} else if ((n->flags & 0x600000) == 0x600000) {
				mask = 0;
				E3D_LogInfo((&g_FileLog), "AniMask: BIPED+NO_TABNODOR node: %s (node animation discarded)", n->name);
			} else if (!(n->flags & NODE_BIP)) {
				CHECK(CNode3D_Class(n) != CLASS_OBJ3D_MESHD3D);
				if (CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH) {
					CTriMesh *mesh = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
					if (mesh->pSkin) {
						mask = 0;
						if (n->flags & 0x3d1000) {
							Con_Warnf("AniMask: skinned mesh node: %s [f:%08X] (node animation NOT discarded)\n",
								n->name, n->flags);
							mask = 0xffffffff;
						} else if (n->Refs - (n->prev != NULL) - (n->next != NULL)) {
							E3D_LogInfo((&g_FileLog), "AniMask: skinned mesh node: %s (ref-node animation NOT discarded)",
								n->name);
							mask = 0xffffffff;
						} else {
							E3D_LogWhite((&g_FileLog), "AniMask: skinned mesh node: %s (node animation discarded)", n->name);
						}
					}
				}
				mask |= 0x80;
			}
		}
		n->aniLocked = (int)~mask;
	}
}

void Scene_SetupLOD(CScene *scn) {
	E3D_LogWhite((&g_FileLog), "LOD Model setup: %s", Path_FileName(scn->name));
	String_Set(&scn->mainLOD, g_SceneLODParent->name);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		CNode3D *n2 = CScene_FindNode(g_SceneLODParent, n->name);
		CIniFile *ini = g_SceneLODParent->ini;
		if (ini && CIniFile_GetInt(ini, "XRefNodes", n->name, 0)) {
			CHECK(n2);
			n->renderNode = n2->slot;
		}
		if (n->flags & NODE_FX) {
			CHECK(n2);
			n->renderNode = n2->slot;
		}
		if (n->renderNode >= 0) {
			CHECK(n->parent);
			CHECK(n2->parent);
			CHECK(!(n->flags & 0x420000));
			CHECK(!(n->parent->flags & 0x420000));
			CHECK(!(n2->parent->flags & 0x420000));
			E3D_LogWhite((&g_FileLog), "LOD node XRef: %s [f:%08X] [c:%s]", n->name, n->flags,
				Obj3D_ClassName(CNode3D_Class(n)));
		}
		if (CNode3D_Class(n) && CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			Con_Bluef("ERROR: LOD node class is invalid: %s [c:%s]\n", n->name, Obj3D_ClassName(CNode3D_Class(n)));
		}
		if (n->flags & NODE_COLLISION) {
			Con_Warnf("WARN: LOD node is invalid: %s [f:%08X]\n", n->name, n->flags);
		}
		if (n->flags & 0x420000) {
			Con_Warnf("WARN: LOD node is invalid: %s [f:%08X]\n", n->name, n->flags);
		}
		n->index = -1;
		if (!n2) {
			if (Wild_Match("*LOD[1-9]*", n->name)) {
				E3D_LogInfo((&g_FileLog), "WARN: LOD node not found in MainLOD [Node: %s]", n->name);
			} else {
				Con_Warnf("WARN: LOD node not found in MainLOD [Node: %s]\n", n->name);
			}
			continue;
		}
		if (CNode3D_Class(n2) && CNode3D_Class(n2) != CLASS_OBJ3D_MESHD3D &&
			CNode3D_Class(n2) != CLASS_OBJ3D_PARTICLESYSTEM) {
			Con_Bluef("ERROR: LOD main-node class is invalid: %s [c:%s]\n", n2->name,
				Obj3D_ClassName(CNode3D_Class(n2)));
		}
		n->index = n2->index;
		if (!n->parent && !n2->parent) {
			continue;
		}
		if (!n->parent || !n2->parent || strcmp(n->parent->name, n2->parent->name)) {
			Con_Warnf("WARN: LOD node parent mismatch [Node: %s]\n", n->name);
		}
		if (!(n2->aniLocked & 1) && (n->pos.x != n2->pos.x || n->pos.y != n2->pos.y || n->pos.z != n2->pos.z)) {
			if (!((double)0.025f > __builtin_fabs((double)(n->pos.x - n2->pos.x)) &&
				    (double)0.025f > __builtin_fabs((double)(n->pos.y - n2->pos.y)) &&
				    (double)0.025f > __builtin_fabs((double)(n->pos.z - n2->pos.z)))) {
				vec3_t d = { n->pos.x - n2->pos.x, n->pos.y - n2->pos.y, n->pos.z - n2->pos.z };
				Con_Warnf("WARN: LOD node Pos mismatch [Node: %s] (%s)\n", n->name, Vec3_Str(&d, 0, 6));
			}
		}
		if (!(n2->aniLocked & 2) && (n->rot.x != n2->rot.x || n->rot.y != n2->rot.y || n->rot.z != n2->rot.z ||
					     n->rot.w != n2->rot.w)) {
			double d = Quat_DotAbs(&n->rot, &n2->rot);
			if (cos(0x1.c98710add7e36p-10) > d) {
				Con_Warnf("WARN: LOD node QRot mismatch [Node: %s]\n", n->name);
			}
		}
	}
}

void Scene_PhysiqueSkin(CScene *scn) {
	if (g_M3DVersion <= 12) {
		return;
	}
	CScene_UpdateNodes(scn);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (n->index < 0) {
			continue;
		}
		M3DNode *dat = M3D_NodeDat(n);
		if (!dat->skin.n) {
			continue;
		}
		CHECK(CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH);
		if (CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			continue;
		}
		CTriMesh *mesh = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH);
		CTriMesh_AllocSkin(mesh);
		for (int v = 0; v < mesh->nVert; v++) {
			TABLE_CHECK(dat->skin.p, v, dat->skin.n);
			const M3DSkinVert *sv = (const M3DSkinVert *)dat->skin.p + v;
			byte *s = CTriMesh_GetSkin(mesh, (uint32_t)v);
			s[0] = (byte)sv->NumBones;
			for (int k = 0; k < sv->NumBones; k++) {
				CNode3D *bone = CNode3D_Find(scn->Node3D, sv->bone[k], 1);
				if (!bone) {
					Con_FatalErrorf("\nFATAL ERROR: Check failed!\n\nError: %s\n\nExpression: %s\n\nFile: '%s'\nDate: %s\n"
						"Line: %d", sv->bone[k], "bone && \"_bone_not_found_\"", __FILE__, __TIMESTAMP__, __LINE__);
				}
				CHECK(bone && (uint32_t)bone->index < 256);
				bone->flags |= NODE_BONE;
				s[k + 1] = (byte)bone->index;
				memcpy(s + 4 + 4 * k, &sv->weight[k], sizeof(float));
			}
		}
		CTriMesh_Transform(mesh, &n->m);
	}
}

/* a node and its parent of the reordering */
typedef struct M3DNodeParent {
	CNode3D *node;			/* 0x0 */
	CNode3D *parent;			/* 0x4 */
} M3DNodeParent;

/* table.h at rva 0x2bc1b0 */
static M3DNodeParent *M3DNodeParent_Get(CTable *t, uint32_t i) {
	return Table_GetN(t, i, sizeof(M3DNodeParent));
}

/* the nodes ordered so that the parents come first, when one is not */
static void Scene_SortNodes(CScene *scn) {
	byte change = 0;
	int k = 1;

	CScene_SetMarks(scn, 0);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		if (n->parent && !n->parent->mark) {
			change = 1;
		}
		n->mark = k++;
	}
	if (!change) {
		return;
	}
	CTable t = { 0, NULL };
	int num = CScene_NumNodes(scn);
	CHECK(num > 0);
	t.n = num;
	t.p = New_Array((uint32_t)num, sizeof(M3DNodeParent));
	CHECK(t.p);
	CNode3D *n = scn->Node3D;
	for (int i = 0; i < num; i++) {
		CHECK(n);
		TABLE_CHECK(t.p, i, num);
		((M3DNodeParent *)t.p)[i].node = n;
		TABLE_CHECK(t.p, i, num);
		((M3DNodeParent *)t.p)[i].parent = n->parent;
		n = n->next;
	}
	M3DNodeParent *tab = t.p;
	do {
		change = 0;
		for (int i = 0; i < num; i++) {
			TABLE_CHECK(t.p, i, num);
			CNode3D *node = tab[i].node;
			while (tab[i].parent) {
				int j = 0;
				while (j < num && M3DNodeParent_Get(&t, (uint32_t)j)->node != tab[i].parent) {
					j++;
				}
				if (j <= i) {
					break;
				}
				CHECK(M3DNodeParent_Get(&t, (uint32_t)j)->node != node);
				M3DNodeParent a = *M3DNodeParent_Get(&t, (uint32_t)i);
				M3DNodeParent b = *M3DNodeParent_Get(&t, (uint32_t)j);
				*M3DNodeParent_Get(&t, (uint32_t)i) = b;
				*M3DNodeParent_Get(&t, (uint32_t)j) = a;
				change = 1;
			}
		}
	} while (change);
	for (int i = 0; i < num; i++) {
		TABLE_CHECK(t.p, i, num);
		CScene_RemoveNode(scn, tab[i].node);
	}
	for (int i = 0; i < num; i++) {
		TABLE_CHECK(t.p, i, num);
		CScene_AddNode(scn, tab[i].node);
		TABLE_CHECK(t.p, i, num);
		TABLE_CHECK(t.p, i, num);
		CNode3D_SetParent(tab[i].node, tab[i].parent);
	}
	if (t.p) {
		operator_delete_array(t.p);
	}
}

void Scene_Sort(CScene *scn) {
	Scene_SortNodes(scn);
}

void Scene_AnimParents(CScene *scn, CScene *staticScn) {
	CHECK(staticScn->Node3D);
	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		CNode3D *n2 = CNode3D_Find(staticScn->Node3D, n->name, 1);
		if (!n2) {
			continue;
		}
		CNode3D *parent = n->parent;
		if (!n2->parent) {
			parent = NULL;
		} else {
			CNode3D *p = CNode3D_Find(scn->Node3D, n2->parent->name, 1);
			if (p) {
				parent = p;
			} else {
				E3D_LogBlue((&g_FileLog), "  Parent not found: '%s'  Node: '%s'  Parent: '%s'", n2->parent->name, n2->name,
					parent ? parent->name : "(null)");
			}
		}
		if (n->parent != parent) {
			CNode3D_SetParent(n, parent);
		}
	}
	Scene_SortNodes(scn);
}

void Scene_Hierarchy(CScene *scn) {
	if (!g_OptimizeNodes) {
		return;
	}
	Scene_Clear(scn, "1");
	CTable t = { 0, NULL };
	Table_AllocN(&t, CScene_NumNodes(scn), sizeof(M3DNodeParent));
	CNode3D *n = scn->Node3D;
	int num = t.n;
	for (int i = 0; i < num; i++, n = n->next) {
		CHECK(n);
		TABLE_CHECK(t.p, i, num);
		((M3DNodeParent *)t.p)[i].node = n;
		TABLE_CHECK(t.p, i, num);
		((M3DNodeParent *)t.p)[i].parent = n->parent;
		if (n->flags & NODE_DELETE) {
			continue;
		}
		CNode3D *pp = n->parent;
		if (pp) {
			while (pp->flags & NODE_DELETE && pp->Refs - (pp->prev != NULL) - (pp->next != NULL) == 1 && pp->parent) {
				pp = pp->parent;
			}
		}
		if (pp != n->parent) {
			E3D_LogInfo((&g_FileLog), "HIERARCHY__: New parent for node: %s [new: %s] [prev: %s] [f:%08X]", n->name,
				pp->name, n->parent->name, n->flags);
			TABLE_CHECK(t.p, i, num);
			((M3DNodeParent *)t.p)[i].parent = pp;
			continue;
		}
		CNode3D *p1 = n->parent;
		if (!p1 || !(p1->flags & NODE_DELETE)) {
			continue;
		}
		for (CNode3D *a = p1->parent; a && a->index >= 0; a = a->parent) {
			CNode3D *d = Node_FindDescendant(scn->Node3D, a);
			if (d) {
				E3D_LogInfo((&g_FileLog), "HIERARCHY: (disabled): new parent for node: %s [new: %s] [prev: %s] [f:%08X]",
					n->name, d->name, p1->name, n->flags);
				break;
			}
		}
	}
	M3DNodeParent *tab = t.p;
	for (int i = 0; i < num; i++) {
		TABLE_CHECK(t.p, i, num);
		TABLE_CHECK(t.p, i, num);
		CNode3D_SetParent(tab[i].node, tab[i].parent);
	}
	Scene_Sort(scn);
	Scene_Clear(scn, "2");
	if (tab) {
		operator_delete_array(tab);
	}
}


/* the meshes */

/* the Physique weights of the vertices: per vertex its bones (equal names add up) and weights; weights below 0.01
 * are spread over the other bones of the vertex and the bone removed */
static void M3D_ReadPhysique(CNode3D *node, M3DNode *dat, int nVert) {
	int version = M3D_ReadInt("Version:");
	M3D_ReadInt("PhyVersion:");
	int vertices = M3D_ReadInt("NumVert:");
	CHECK(version == 0);
	CHECK(vertices == nVert);
	CTable *skin = &dat->skin;
	Table_New1C(skin, vertices);
	int weights = 0, deleted = 0;
	for (int i = 0; i < vertices; i++) {
		TABLE_CHECK(skin->p, i, skin->n);
		M3DSkinVert *vskin = (M3DSkinVert *)skin->p + i;
		M3D_ReadIndex(i);
		int bones = M3D_ReadInt(NULL);
		CHECK(bones > 0);
		vskin->NumBones = 0;
		for (int b = bones; b > 0; b--) {
			float weight;
			M3D_ReadString("Bone:", NULL, 0, 1);
			Node_ClearName(g_M3DString);
			int k = 0;
			while (k < vskin->NumBones && strcmp(g_M3DString, vskin->bone[k])) {
				k++;
			}
			M3D_ReadFloatTo(&weight);
			int n = vskin->NumBones;
			if (k < n) {
				vskin->weight[k] = weight + vskin->weight[k];
				continue;
			}
			if (n >= SKIN_MAX_BONES) {
				M3D_SetOk(0);
				E3D_LogVerbose((&g_FileLog), "  physique:  objeto: '%s'  vertice: %d  pesos: %d  peso: %f", node->name, i,
					bones, (double)weight);
				CHECK(vskin->NumBones < SKIN_MAX_BONES);
			}
			String_Set(&vskin->bone[n], g_M3DString);
			vskin->weight[n] = weight;
			vskin->NumBones++;
		}
		weights += vskin->NumBones;
		for (int k = 0; k < vskin->NumBones; k++) {
			if (!(0.01 > (double)vskin->weight[k])) {
				continue;
			}
			CHECK(vskin->NumBones > 1);
			float share = vskin->weight[k] / (float)(vskin->NumBones - 1);
			for (int j = 0; j < vskin->NumBones; j++) {
				vskin->weight[j] = share + vskin->weight[j];
			}
			int last = vskin->NumBones - 1;
			if (k != last) {
				String_Set(&vskin->bone[k], vskin->bone[last]);
				memcpy(&vskin->weight[k], &vskin->weight[last], sizeof(float));
			}
			k--;
			vskin->NumBones--;
			deleted++;
		}
	}
	E3D_LogWhite((&g_FileLog), "  INFO: Physique:  vertices: %4d  pesos borrados: %4d / %-4d  objeto: '%s'", vertices,
		deleted, weights, node->name);
}

/* the default material of the faces without one, from ColorMalla */
static CMaterial *M3D_MatColorMesh(const vec3_t *c) {
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
	String_Printf(&mat->Name, "color_malla [%d,%d,%d]", mat->params.diffuse[0], mat->params.diffuse[1],
		mat->params.diffuse[2]);
	CMaterial *found = CMatList_Find((&g_MatList), mat);
	if (found) {
		mat->vftable->Delete(mat, 1);
		return found;
	}
	CMatList_Add((&g_MatList), mat);
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

float g_M3DPreScaleU;

float g_M3DPreScaleV;

void M3D_ReadMesh(CNode3D *node, byte anim) {
	m3dTransform_t mt;
	matrix_t tmp, m;
	vec3_t color, v;

	if (!anim) {
		M3D_ReadString("TipoGeom:", NULL, 0, 0);
		if (!strcmp(g_M3DString, "Malla")) {
			CTriMesh *mt3 = operator_new(sizeof(CTriMesh));
			mt3 = mt3 ? CTriMesh_ctor(mt3) : NULL;
			CNode3D_SetObj3D(node, (IObject3D *)mt3);
		}
	}
	if (CNode3D_Class(node) != CLASS_OBJ3D_TRIMESH) {
		return;
	}
	M3DNode *dat = M3D_NodeDat(node);
	CTriMesh *mesh = Obj3D_Cast(node->obj3D, CLASS_OBJ3D_TRIMESH);
	if (anim) {
		return;
	}
	int nVert = M3D_ReadInt("NumVerts:");
	int nTVert = M3D_ReadInt("NumTVerts:");
	int nTri = M3D_ReadInt("NumCaras:");
	M3D_ReadBool("DefMalla:");
	if (!nVert || !nTri) {
		M3D_SetOk(0);
		E3D_LogVerbose((&g_FileLog), "  objeto: '%s'  caras: %d  nVert: %d  nTVert: %d", node->name, nTri, nVert, nTVert);
	}
	CTriMesh_SetNumVert(mesh, nVert, 0);
	CTriMesh_SetNumTVert(mesh, nTVert, 0);
	CTriMesh_SetNumTri(mesh, nTri, 0);
	if (nTVert > nTri * 3 && nTVert > 5000) {
		M3D_SetOk(0);
		E3D_LogVerbose((&g_FileLog), "  objeto: '%s'  caras: %d  nTVert: %d  (3*caras: %d)", node->name, nTri, nTVert,
			nTri * 3);
	}
	M3D_ReadTransform("local_a_pivote", &mt);
	M3DTransform_ToMatrix(&mt, &dat->mPivot);
	if (g_M3DVersion < 11 || !g_UseWSM) {
		m = *Matrix_MulD3D(&tmp, &dat->mPivot, &dat->mNode);
	} else {
		m = dat->mNode;
	}
	int iMat = M3D_ReadInt("Material:");
	M3D_ReadVec3("ColorMalla:", &color, 0);
	for (int i = 0; i < nVert; i++) {
		M3D_ReadIndex(i);
		M3D_ReadVec3(NULL, &v, 1);
		Vec3_TransformH(&v, &v, &m);
		CTriMesh_SetVert(mesh, (uint32_t)i, &v);
	}
	if (g_M3DVersion > 12 && M3D_ReadBool("Physique:")) {
		M3D_ReadPhysique(node, dat, nVert);
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
				defMat = M3D_MatColorMesh(&color);
			}
			mat = defMat;
		}
		plane_t plane;
		if (a == c || c == b || b == a || !Plane_Calc(&plane, &mesh->pVert[a], &mesh->pVert[c], &mesh->pVert[b])) {
			degen++;
		}
		CTriMesh_SetTriVerts(mesh, (uint32_t)i, a, c, b);
		CTriMesh_SetTriMat(mesh, (uint32_t)i, mat);
	}
	if (degen) {
		M3D_SetOk(0);
		E3D_LogVerbose((&g_FileLog), "  caras degeneradas: %d  caras: %d  objeto: '%s'", degen, nTri, node->name);
	}
	if (nTVert) {
		float maxU = 0.0f, maxV = 0.0f;
		for (int i = 0; i < nTVert; i++) {
			uv_t uv;
			M3D_ReadIndex(i);
			M3D_ReadVec2(NULL, &uv.u);
			uv.u = g_M3DPreScaleU * uv.u;
			uv.v = g_M3DPreScaleV * (1.0f - uv.v);
			CTriMesh_SetTVert(mesh, (uint32_t)i, &uv);
			maxU = Float_MaxSS((float)__builtin_fabs((double)uv.u), maxU);
			maxV = Float_MaxSS((float)__builtin_fabs((double)uv.v), maxV);
		}
		if ((maxU > 16.0f || maxV > 16.0f) && !(node->flags & 0x80)) {
			M3D_SetOk(0);
			E3D_LogVerbose((&g_FileLog), "  WARN: coord. de textura: %f %f  objeto: '%s'", (double)maxU, (double)maxV,
				node->name);
		}
		for (int i = 0; i < nTri; i++) {
			M3D_ReadIndex(i);
			int a = M3D_ReadInt(NULL);
			int b = M3D_ReadInt(NULL);
			int c = M3D_ReadInt(NULL);
			CTriMesh_SetTriTVerts(mesh, (uint32_t)i, a, c, b);
		}
	}
	for (int i = 0; i < nTri; i++) {
		vec3_t n0, n1, n2;
		M3D_ReadIndex(mesh->pTri[i].v[0]);
		M3D_ReadVec3(NULL, &n0, 1);
		M3D_ReadIndex(mesh->pTri[i].v[2]);
		M3D_ReadVec3(NULL, &n2, 1);
		M3D_ReadIndex(mesh->pTri[i].v[1]);
		M3D_ReadVec3(NULL, &n1, 1);
		Vec3_TransformDir0(&n0, &n0, &m);
		Vec3_TransformDir0(&n1, &n1, &m);
		Vec3_TransformDir0(&n2, &n2, &m);
		M3D_Normalize(&n0);
		M3D_Normalize(&n1);
		M3D_Normalize(&n2);
		CTriMesh_SetTriNormals(mesh, (uint32_t)i, &n0, &n1, &n2);
	}
}


/* the materials */

/* the texture map tipos of the "Mapa:" names */
enum {
	M3D_MAP_DIFFUSE = 100,
	M3D_MAP_OPACITY = 101,
	M3D_MAP_REFLECTION = 102,
	M3D_MAP_BUMP = 103,
	M3D_MAP_SELFILLUM = 104,
	M3D_MAP_SPECLEVEL = 105
};

/* the map mapid of mat set to name: CHECK it has none, and the switches of the ini and the video (MAT_SHADER: all) */
static CMap *M3D_SetMap(CMaterial *mat, uint32_t mapid, const char *name) {
	CHECK(!CMaterial_GetMapName(mat, mapid));
	return CMaterial_SetMap(mat, mapid, name);
}

char g_M3DTexturePath[260];

void M3D_ReadMap(CMaterial *mat, int iMat, int iSubmat, int idx, int type) {
	char buf[0x100], name[0x100], class[0x100];
	float quantity = 1.0f;

	(void)idx;
	CHECK(mat);
	M3D_ReadString("Mapa:", buf, sizeof(buf), 0);
	if (!strcmp(buf, "nulo") || !strcmp(buf, "otro")) {
		return;
	}
	if (!strcmp(buf, "diffuse")) {
		type = M3D_MAP_DIFFUSE;
	} else if (!strcmp(buf, "opacity")) {
		type = M3D_MAP_OPACITY;
	} else if (!strcmp(buf, "reflection")) {
		type = M3D_MAP_REFLECTION;
	} else if (!strcmp(buf, "bump")) {
		type = M3D_MAP_BUMP;
	} else if (!strcmp(buf, "selfillum")) {
		type = M3D_MAP_SELFILLUM;
	} else if (!strcmp(buf, "speclevel")) {
		type = M3D_MAP_SPECLEVEL;
	} else if (strcmp(buf, "TintRGB") && strcmp(buf, "Mascara")) {
		Err_Printf(1, "Invalid texture map class: '%s'", buf);
		CHECK(0);
	}
	M3D_ReadString("Nombre:", name, sizeof(name), 1);
	M3D_ReadString("Clase:", class, sizeof(class), 1);
	if (g_M3DVersion > 8) {
		quantity = M3D_ReadFloat("Cantidad:");
	}
	if (!strcmp(class, "Bitmap")) {
		CMap *map = NULL;
		M3D_ReadString("Bitmap:", buf, sizeof(buf), 1);
		Path_Part(buf, buf, PATH_NAME | PATH_EXT);
		if (buf[0] == '$') {
			Path_Merge(buf, ".bmp", buf + 1);
			Path_Merge(buf, g_SharedTexturesPath, buf);
		} else {
			Path_Merge(buf, ".bmp", buf);
			Path_Merge(buf, g_M3DTexturePath, buf);
		}
		byte shader = (mat->params.attrib & MAT_SHADER) != 0;
		switch (type) {
		case M3D_MAP_DIFFUSE:
			map = M3D_SetMap(mat, MAP_DIFFUSE, buf);
			break;
		case M3D_MAP_OPACITY:
			if (shader || g_M3DUseOpacity) {
				map = M3D_SetMap(mat, MAP_OPACITY, buf);
			}
			break;
		case M3D_MAP_REFLECTION:
			if (shader || (g_UseEnvMap && g_M3DUseEnvmap)) {
				CHECK(!CMaterial_GetMapName(mat, MAP_ENVIRONMENT));
				map = CMaterial_SetMap(mat, MAP_ENVIRONMENT, buf);
				mat->params.reflection = buf[0] != 0;
				if (strstr(name, "(env2)")) {
					map->env2 = 1;
				}
			}
			break;
		case M3D_MAP_BUMP:
			if (shader || (g_UseBumpMap && g_M3DUseBumpmap)) {
				map = M3D_SetMap(mat, MAP_BUMP, buf);
			}
			break;
		case M3D_MAP_SELFILLUM:
			if (shader || (g_UseGlowMap && g_M3DUseGlowmap)) {
				map = M3D_SetMap(mat, MAP_GLOW, buf);
			}
			break;
		case M3D_MAP_SPECLEVEL:
			CHECK(0);
			map = M3D_SetMap(mat, MAP_OPACITY, buf);
			break;
		}
		if (map) {
			map->amount = quantity;
			map->fx = FXTexture_Kind(name);
		}
		if (g_M3DVersion > 8) {
			byte tu = M3D_ReadBool("TileU:");
			byte tv = M3D_ReadBool("TileV:");
			byte eu = M3D_ReadBool("EspejoU:");
			byte ev = M3D_ReadBool("EspejoV:");
			if (map) {
				map->tile = (tv ? 2 : 0) | (tu != 0);
				map->mirror = (ev ? 2 : 0) | (eu != 0);
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
				if (map) {
					map->uvOffset[0] = du;
					map->uvOffset[1] = dv;
					map->uvScale[0] = su;
					map->uvScale[1] = sv;
					map->uvRot[0] = 0.0f;
					map->uvRot[1] = 0.0f;
					map->uvRot[2] = ang;
				}
			}
			M3D_ReadBool("TexInvert:");
			M3D_ReadString("Filter:", buf, sizeof(buf), 0);
			if (map && !strcmp(buf, "no")) {
				map->filter = 0;
			}
		}
	} else if (!strcmp(class, "Mask")) {
		CHECK(type == M3D_MAP_REFLECTION);
		Err_Printf(1, "El tipo de mapa 'Mask' est\xe1 sin terminar");
	}
	int n = M3D_ReadInt("NumSubTexmaps:");
	for (int i = 0; i < n; i++) {
		M3D_ReadMap(mat, iMat, iSubmat, i, type);
	}
}

void M3D_ReadMaterial(CMatList *list, int iMat, int iSubmat) {
	char buf[0x100];

	CHECK(list);
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
	CMatList_Add(list, mat);
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
	String_Set(&mat->Name, buf);
	crt_strlwr(buf);
	static const struct {
		const char *s;
		uint16_t attrib;
	} flags[12] = { { "(nc)", MAT_NOCOLLIDE }, { "(+no_collide)", MAT_NOCOLLIDE }, { "(agua)", 0x2 }, { "(niebla)", 0x4 },
		{ "(+shader:clouds)", 0x4 }, { "(+sort_zbias)", 0x10 }, { "(+transp_before)", 0x10 },
		{ "(+alpha_test)", 0x20 }, { "(+area_light:", 0x100 }, { "(+shader:", MAT_SHADER }, { "(+zbias)", 0x800 },
		{ "(+xshadow)", 0x4000 } };
	for (int i = 0; i < 12; i++) {
		if (strstr(buf, flags[i].s)) {
			mat->params.attrib |= flags[i].attrib;
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
	int class = 2;
	if (!strcmp(buf, "Standard")) {
		class = 0;
	}
	if (!strcmp(buf, "Multi/Sub-Object")) {
		class = 1;
	}
	mat->params.matClass = class;
	M3D_ReadColors(mat);
	if (!class) {
		M3D_ReadString("Shading:", NULL, 0, 0);
		float si = M3D_ReadFloat("SelfIllum:");
		byte b = M3D_ColorByte(si);
		mat->params.emissive[3] = 0;
		mat->params.emissive[0] = b;
		mat->params.emissive[1] = b;
		mat->params.emissive[2] = b;
		mat->params.twoSided = M3D_ReadBool("TwoSided:");
		M3D_ReadBool("FaceMap:");
		M3D_ReadString("TransparencyType:", NULL, 0, 0);
		if (!strcmp(g_M3DString, "Additive")) {
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
			mat->params.attrib |= MAT_1000;
			mat->params.diffuse[3] = 0;
			mat->params.srcBlend = 2;
		}
	}
	int num = M3D_ReadInt("Mapas:");
	for (int i = 0; i < num; i++) {
		M3D_ReadMap(mat, iMat, iSubmat, i, 0);
	}
	num = M3D_ReadInt("Submateriales:");
	if (num) {
		CHECK(!mat->sub);
		CMatList *sub = operator_new(sizeof(CMatList));
		if (sub) {
			sub->Name = NULL;
			sub->vftable = CMATLIST_VFTABLE;
			sub->last = NULL;
			sub->first = NULL;
		}
		mat->sub = sub;
	}
	for (int i = 0; i < num; i++) {
		M3D_ReadMaterial(mat->sub, iMat, iSubmat == -1 ? i : iSubmat);
	}
	if (g_Overdraw) {
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
		if (mat->params.reflection) {
			*(uint32_t *)mat->params.diffuse = 0x301060;
		}
		mat->params.reflection = 0;
		mat->params.zWrite = 0;
	}
	mat->params.fog = 1;
	if (Mat_IsTransp(mat)) {
		mat->params.zWrite = 0;
	}
	if (!mat->params.transp && mat->map[MAP_OPACITY] && !mat->params.reflection && mat->map[MAP_GLOW]) {
		mat->params.diffuse[3] = 0xff;
		mat->params.transp = 1;
	}
	if (mat->params.transp && mat->params.reflection && mat->params.srcBlend == 5) {
		E3D_LogVerbose((&g_FileLog), "WARN: transparent environment mapped material without (+transp:premult)  [%s]",
			mat->Name);
		mat->params.srcBlend = 2;
		if (!(mat->params.attrib & MAT_SHADER)) {
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
		mat->params.fog = 0;
		mat->params.zWrite = 0;
		mat->params.diffuse[3] = e;
	}
	if (!g_UseTextures && !texsi && mat->map[MAP_DIFFUSE] && mat->map[MAP_DIFFUSE]->filter == 1) {
		CMaterial_MapToColor(mat);
	}
}


/* the reader of the M3D files */

#define M3DNODE_CTOR 0x2bbb90
#define M3D_HDR 0x20			/* the header "[M3D,BIN,ETIQ:si]" */
#define M3D_NAME 0x100
/* the node flags the reader sets (NODO_*) */
#define NODE_ROOT 0x1
#define NODE_GROUP 0x2
#define NODE_SELECT 0x4
#define NODE_HVISIBLE 0x10
#define NODE_NO_CASTSHADOWS 0x20
#define NODE_NO_RCVSHADOWS 0x40
#define NODE_NO_RENDER 0x80
#define NODE_BOXMODE 0x100
#define NODE_EFFECT (1u << 19)
#define NODE_NO_TRANS_TAB (NODE_NOUPDATE | NODE_NO_RENDERNODE)
/* the kinds of objects ("Tipo:") */
enum { M3D_GEOMETRY = 1, M3D_CAMERA, M3D_LIGHT, M3D_TARGET, M3D_DUMMY, M3D_PORTAL };

/* the bits of the animation of a node (CAnimNode_Init flags) */
#define ANIM_POS 0x1
#define ANIM_ROT 0x2
#define ANIM_FOV 0x4
#define ANIM_COLOR 0x8
#define ANIM_INTENS 0x10
#define ANIM_VIS 0x80

static const matrix_t g_M3DIdentity = { .m = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f } };

/* the vertex animation of a morph node: the vertices of each frame through the pivot */
static void M3D_ReadEVA(CNode3D *node, M3DNode *dat) {
	m3dTransform_t mt;
	M3DTable4 t;
	matrix_t piv, m, tmp, res;
	CAnimVert *av;
	vec3_t v;
	int nv, i, f, before, after;

	if (g_M3DVersion > 9) {
		M3D_ReadTransform("local_a_pivote", &mt);
		M3DTransform_ToMatrix(&mt, &piv);
	} else {
		piv = dat->mPivot;
	}
	if (g_M3DVersion >= 11 && g_UseWSM) {
		Matrix_LoadIdentity(&piv);
	}
	nv = M3D_ReadInt("NumVerts:");
	CAnimNode_SetEVA(dat->AnimNode, nv);
	m = *Matrix_MulD3D(&res, &piv, M3DNode_FrameMatrix(dat, &tmp, 0));
	for (i = 0; i < nv; i++) {
		CHECK(M3D_ReadInt("Vertice:") == i);
		crt_eh_vector_ctor(t.t, sizeof(M3DTable), 4, (void *)M3DTable_ctor, (void *)Table_Free);
		M3DTable_ReadFloat(&t.t[0], "EVA_X");
		M3DTable_ReadFloat(&t.t[2], "EVA_Y");
		M3DTable_ReadFloat(&t.t[1], "EVA_Z");
		av = CAnimNode_GetVert(dat->AnimNode, (uint32_t)i);
		CHECK(av);
		CAnimVert_AllocTr(av, g_M3DFrames);
		for (f = 0; f < g_M3DFrames; f++) {
			M3DTable4_GetVec(&t, &v, (uint32_t)f);
			Vec3_TransformH(&v, &v, &m);
			*CAnimVert_Frame(av, (uint32_t)f) = v;
		}
		crt_eh_vector_dtor(t.t, sizeof(M3DTable), 4, (void *)Table_Free);
	}
	before = CEVA_MemSize(dat->AnimNode->EVA);
	CAnimNode_OptimizeEVA(dat->AnimNode);
	after = dat->AnimNode->EVA ? CEVA_MemSize(dat->AnimNode->EVA) : 0;
	E3D_DebugPrintf("\noptimizar EVA: '%s'  %d -> %d  (%.2f %%)", node->name, before, after,
		(double)after * 100.0 / (double)before);
}

char g_M3DBaseName[260];

/* the animation tables of an object: its tracks and visibility, the values of its kind; the mask of the channels
 * that stay */
static uint32_t M3D_ReadAnimNode(CScene *scn, CNode3D *node, M3DNode *dat, int k, int type, byte exported,
	byte bStatic) {
	CNode3D *m;
	uint32_t mask, extra, base;

	CHECK(scn->AnimM3D);
	dat->AnimNode = CAnimation_GetNode(scn->AnimM3D, (uint32_t)k);
	CHECK(dat->AnimNode);
	Bprintf(g_AnimNodeName, 0x100, "%s", node->name);
	if (g_M3DVersion >= 11 && g_UseWSM) {
		M3DTrack_Read(&dat->track2);
		M3DTrack_Read(&dat->track);
	} else {
		M3DTrack_Read(&dat->track);
		M3DTrack_Read(&dat->track2);
	}
	M3DTable_ReadFloat(&dat->t2a4[0], "NodoVis");
	if (exported) {
		switch (type) {
		case M3D_GEOMETRY:
		case M3D_TARGET:
		case M3D_DUMMY:
		case M3D_PORTAL:
			break;
		case M3D_CAMERA:
			M3DTable_ReadFloat(&dat->t2a4[5], "FOV");
			break;
		case M3D_LIGHT:
			M3DTable_ReadInt(&dat->t2a4[1], "LuzR");
			M3DTable_ReadInt(&dat->t2a4[2], "LuzG");
			M3DTable_ReadInt(&dat->t2a4[3], "LuzB");
			M3DTable_ReadFloat(&dat->t2a4[4], "LuzI");
			break;
		default:
			CHECK(0);
		}
	}
	/* position and rotation; the field of view of a camera, the color and intensity of a light; the visibility
	 * when it changes */
	extra = type == M3D_LIGHT ? ANIM_COLOR | ANIM_INTENS : 0;
	base = (type == M3D_CAMERA) * ANIM_FOV + (ANIM_POS | ANIM_ROT);
	mask = (dat->t2a4[0].fixed ? 0 : ANIM_VIS) | base | extra;
	TABLE_CHECK(dat->t2a4[0].p, 0, dat->t2a4[0].n);
	if (*(float *)dat->t2a4[0].p != 1.0f) {
		mask = base | extra | ANIM_VIS;
	}
	if (!exported) {
		mask = 0;
		E3D_LogWhite((&g_FileLog), "Animation node hidden: %s (node animation discarded)", node->name);
	} else if (mask && !bStatic && g_SceneAnimParent) {
		m = CNode3D_Find(g_SceneAnimParent->Node3D, node->name, 1);
		if (!m) {
			E3D_LogWhite((&g_FileLog), "Animation node not found in Model: %s (node animation discarded)", node->name);
			if (mask & ANIM_VIS) {
				Con_Warnf("Visibility animation discarded: %s\n", node->name);
			}
			mask = 0;
		} else {
			uint32_t v = ~(uint32_t)m->aniLocked & mask;

			if (v != mask) {
				E3D_LogWhite((&g_FileLog), "Animation node masked: %s (node animation discarded)", node->name);
			}
			mask = v;
		}
	}
	CAnimNode_Init(dat->AnimNode, (uint32_t)g_M3DFrames, (uint32_t)g_M3DFirstFrame, mask);
	if (g_M3DVersion > 8 && M3D_ReadBool("DefMalla:")) {
		CHECK(exported);
		if (!Wild_MatchLower("*morph*", g_M3DBaseName)) {
			Con_Warnf("WARN: Animation with morphing: %s\n", g_M3DName);
		}
		mask = 0;
		CAnimNode_Init(dat->AnimNode, (uint32_t)g_M3DFrames, (uint32_t)g_M3DFirstFrame, 0);
		E3D_LogWhite((&g_FileLog), "Animation node for EVA mesh: %s (node animation discarded)", node->name);
		CHECK(g_SceneAnimParent);
		CHECK(CNode3D_Find(g_SceneAnimParent->Node3D, node->name, 1));
		M3D_ReadEVA(node, dat);
	}
	return mask;
}

/* the frames of the channels of mask into the animation node */
static void M3D_FramesAnimNode(CNode3D *node, uint32_t mask) {
	M3DNode *dat = M3D_NodeDat(node);
	CAnimNode *an;
	vec3_t v;
	quat_t q;
	uint32_t idx, c;
	int f, i, b;
	float x;

	CHECK(dat->AnimNode);
	for (f = g_M3DFirstFrame; f <= g_M3DLastFrame; f++) {
		idx = (uint32_t)(f - g_M3DFirstFrame);
		if (mask & ANIM_POS) {
			M3DTable4_GetVec(&dat->track.pos, &v, idx);
			an = dat->AnimNode;
			i = CAnimNode_FrameIndex(an, f, an->Pos, ANIM_POS);
			an->Pos[i] = v;
		}
		if (mask & ANIM_ROT) {
			M3DTable4_GetQuat(&dat->track.rot, &q, idx);
			an = dat->AnimNode;
			i = CAnimNode_FrameIndex(an, f, an->Rot, ANIM_ROT);
			an->Rot[i] = q;
		}
		if (mask & ANIM_VIS) {
			TABLE_CHECK(dat->t2a4[0].p, idx, dat->t2a4[0].n);
			b = (int)((double)(((float *)dat->t2a4[0].p)[idx] * 255.0f) + 0.5);
			if (b <= 0) {
				b = 0;
			} else if (b >= 0xff) {
				b = 0xff;
			}
			an = dat->AnimNode;
			i = CAnimNode_FrameIndex(an, f, an->Visible, ANIM_VIS);
			an->Visible[i] = (byte)b;
		}
		if (mask & ANIM_COLOR) {
			TABLE_CHECK(dat->t2a4[3].p, idx, dat->t2a4[3].n);
			const int *pb = (const int *)dat->t2a4[3].p + idx;
			TABLE_CHECK(dat->t2a4[2].p, idx, dat->t2a4[2].n);
			const int *pg = (const int *)dat->t2a4[2].p + idx;
			TABLE_CHECK(dat->t2a4[1].p, idx, dat->t2a4[1].n);
			const int *pr = (const int *)dat->t2a4[1].p + idx;
			c = (uint32_t)(byte)*pr | (uint32_t)(byte)*pg << 8 | (uint32_t)(byte)*pb << 16;
			an = dat->AnimNode;
			i = CAnimNode_FrameIndex(an, f, an->Color, ANIM_COLOR);
			((uint32_t *)an->Color)[i] = c;
		}
		if (mask & ANIM_INTENS) {
			TABLE_CHECK(dat->t2a4[4].p, idx, dat->t2a4[4].n);
			an = dat->AnimNode;
			x = ((float *)dat->t2a4[4].p)[idx];
			i = CAnimNode_FrameIndex(an, f, an->Intens, ANIM_INTENS);
			an->Intens[i] = x;
		}
		if (mask & ANIM_FOV) {
			TABLE_CHECK(dat->t2a4[5].p, idx, dat->t2a4[5].n);
			an = dat->AnimNode;
			x = ((float *)dat->t2a4[5].p)[idx];
			i = CAnimNode_FrameIndex(an, f, an->FOV, ANIM_FOV);
			an->FOV[i] = x;
		}
	}
}

/* the kind of object of "Tipo:", 0 (reported) for an unknown one */
static int M3D_ReadType(void) {
	M3D_ReadString("Tipo:", g_M3DString, M3D_STRING, 0);
	if (!strcmp(g_M3DString, "Geometria")) {
		return M3D_GEOMETRY;
	}
	if (!strcmp(g_M3DString, "Camara")) {
		return M3D_CAMERA;
	}
	if (!strcmp(g_M3DString, "Luz")) {
		return M3D_LIGHT;
	}
	if (!strcmp(g_M3DString, "Objetivo")) {
		return M3D_TARGET;
	}
	if (!strcmp(g_M3DString, "Dummy")) {
		return M3D_DUMMY;
	}
	if (!strcmp(g_M3DString, "Portal")) {
		return M3D_PORTAL;
	}
	Err_Printf(1, "Tipo de nodo no v\xe1lido: '%s'", g_M3DString);
	return 0;
}

static void Node_SetFlag(CNode3D *node, uint32_t flag, byte on) {
	if (on) {
		node->flags |= flag;
	} else {
		node->flags &= ~flag;
	}
}

/* object k of the file: the node, its flags and user properties, transform and object, its animation */
static void M3D_ReadObject(CScene *scn, int k, byte bStatic, byte bAnim, byte castShadows, byte rcvShadows) {
	char buf[M3D_NAME];
	CNode3D *node;
	M3DNode *dat;
	void *p;
	float vis;
	uint32_t mask;
	int type = 0, lines;
	byte exported;

	p = operator_new(sizeof(CNode3D));
	node = p ? CNode3D_ctor(p) : NULL;
	node->index = k;
	CScene_AddNode(scn, node);
	dat = M3D_NodeDat(node);
	dat->AnimNode = NULL;
	dat->mPivot = g_M3DIdentity;
	M3D_ReadString("Objeto:", buf, sizeof(buf), 1);
	Node_ClearName(buf);
	String_Set(&node->name, buf);
	CHECK(strlen(buf) > 0);
	M3D_ReadString("Padre:", buf, sizeof(buf), 1);
	Node_ClearName(buf);
	CNode3D_SetParentName(node, buf);
	CHECK(node->parent);
	exported = M3D_ReadBool("Exportado:");
	Node_SetFlag(node, NODE_GROUP, M3D_ReadBool("Grupo:"));
	Node_SetFlag(node, NODE_SELECT, M3D_ReadBool("Seleccionado:"));
	Node_SetFlag(node, NODE_DELETE, M3D_ReadBool("Oculto:"));
	Node_SetFlag(node, NODE_HVISIBLE, M3D_ReadBool("HeredarVisible:"));
	if (g_M3DVersion > 11) {
		Node_SetFlag(node, NODE_NO_CASTSHADOWS, !M3D_ReadBool("CastShadows:"));
		Node_SetFlag(node, NODE_NO_RCVSHADOWS, !M3D_ReadBool("RcvShadows:"));
		Node_SetFlag(node, NODE_NO_RENDER, !M3D_ReadBool("Renderable:"));
		Node_SetFlag(node, NODE_BOXMODE, M3D_ReadBool("BoxMode:"));
	}
	if (!castShadows) {
		node->flags |= NODE_NO_CASTSHADOWS;
	}
	if (!rcvShadows) {
		node->flags |= NODE_NO_RCVSHADOWS;
	}
	Scene_CheckDuplicates(scn, node);
	if (!crt_strncmp(node->name, "Bip ", 4) || !crt_strncmp(node->name, "Bip_", 4) ||
		!crt_strncmp(node->name, "BipB", 4) || !crt_strncmp(node->name, "Bone", 4)) {
		node->flags |= NODE_BIP;
	}
	if (g_M3DVersion > 13) {
		lines = M3D_ReadInt("UserPropBufferLines:");
		if (lines > 0) {
			p = operator_new(sizeof(CIniFile));
			node->ini = p ? CIniFile_ctor(p) : NULL;
			for (; lines > 0; lines--) {
				M3D_ReadString("Line:", g_M3DString, M3D_STRING, 1);
				CIniFile_AddLine(node->ini, g_M3DString, -1);
			}
		}
		if (g_SceneAnimParent && node->ini) {
			CIniFile *ini = node->ini;

			CIniFile_dtor(ini);
			operator_delete_sized(ini, sizeof(CIniFile));
			node->ini = NULL;
		}
	}
	if (exported) {
		type = M3D_ReadType();
	}
	if (bStatic) {
		if (g_M3DVersion < 11) {
			M3D_ReadTransform("pivote_a_mundo", &dat->mt);
			M3D_ReadTransform("pivote_a_padre", &dat->mt2);
		} else {
			M3D_ReadTransform("pivote_a_mundo", g_UseWSM ? &dat->mt2 : &dat->mt);
			M3D_ReadTransform("ObjTMAfterWSM", g_UseWSM ? &dat->mt : &dat->mt2);
		}
		M3DTransform_ToMatrixAnim(&dat->mt, &dat->mNode);
		node->pos = dat->mt.pos;
		node->rot = dat->mt.rot;
		node->scale = dat->mt.scale;
		M3D_Label("Visible:");
		M3D_ReadFloatTo(&vis);
		node->visible = vis;
		if (exported) {
			switch (type) {
			case M3D_GEOMETRY:
				M3D_ReadMesh(node, 0);
				break;
			case M3D_CAMERA:
				M3D_ReadCamera(node, 0, 0);
				break;
			case M3D_LIGHT:
				M3D_ReadLight(node, 0, 0);
				break;
			case M3D_TARGET:
			case M3D_DUMMY:
				break;
			case M3D_PORTAL:
				M3D_ReadPortal(node, 0);
				break;
			default:
				CHECK(0);
			}
		}
	}
	if (bAnim) {
		mask = M3D_ReadAnimNode(scn, node, dat, k, type, exported, bStatic);
		M3D_FramesAnimNode(node, mask);
	}
}

/* the root of a game model: the node named name2 or the one with MakeRootNode Enable in its user properties, under
 * the scene root, the other children of the scene root moved below it; NULL without one */
static CNode3D *M3D_ModelRoot(CScene *scn, const char *name2) {
	char initPos[0x80];
	CNode3D *n, *root = NULL, *parent;

	memset(initPos, 0, sizeof(initPos));
	for (n = scn->Node3D; n; n = n->next) {
		if (name2 && *name2) {
			if (!crt_stricmp(n->name, name2)) {
				root = n;
				break;
			}
		} else if (n->ini && CIniFile_GetBool(n->ini, "MakeRootNode", "Enable", 0)) {
			CIniFile_GetText(n->ini, "MakeRootNode", "InitPos", initPos, sizeof(initPos), NULL);
			root = n;
			break;
		}
	}
	if (root) {
		if (strcmp(root->name, "DC_Root")) {
			Con_Warnf("WARN: RootNode: %s\n", root->name);
		}
		E3D_LogWhite((&g_FileLog), "RootNode: %s", root->name);
		String_Set(&scn->node, root->name);
		parent = CNode3D_FindPrev(root, "_raiz_escena");
		CHECK(parent);
		CNode3D_SetParent(root, parent);
	}
	for (n = scn->Node3D; n; n = n->next) {
		if (root && n != root && n->parent && !crt_stricmp(n->parent->name, "_raiz_escena")) {
			CNode3D_SetParent(n, root);
		}
	}
	return root;
}

/* the zones of the portals and the targets of the cameras and lights */
static void M3D_Targets(CScene *scn) {
	CNode3D *n, *t;
	M3DNode *dat;

	for (n = scn->Node3D; n; n = n->next) {
		if (n->index < 0) {
			continue;
		}
		dat = M3D_NodeDat(n);
		if (CNode3D_Class(n) == CLASS_OBJ3D_PORTAL) {
			CPortal *portal = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_PORTAL);

			CPortal_SetZone(portal, 0, CNode3D_Find(scn->Node3D, dat->target, 1));
			CPortal_SetZone(portal, 1, CNode3D_Find(scn->Node3D, dat->side2, 1));
			continue;
		}
		if (!dat->target) {
			continue;
		}
		t = CNode3D_Find(scn->Node3D, dat->target, 1);
		CHECK(t);
		if (CNode3D_Find(t, dat->target, 0)) {
			E3D_LogBlue((&g_FileLog), "ERROR: Duplicated target node: %s", t->name);
		}
		if (CNode3D_Class(n) == CLASS_OBJ3D_CAMERA) {
			CCamera *cam = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_CAMERA);

			cam->target = t->pos;
			t->flags |= NODE_DELETE;
		} else if (CNode3D_Class(n) == CLASS_OBJ3D_LIGHT) {
			Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);
			t->flags |= NODE_DELETE;
		} else {
			CHECK(0);
		}
	}
}

/* the nodes without their own transform taken out of the render tables (OptimizeNodes 1) */
static void M3D_OptimizeNodes(CScene *scn) {
	CNode3D *n, *c;
	int pass, cnt;
	byte change;

	for (n = scn->Node3D; n; n = n->next) {
		if (n->flags & NODE_BONE) {
			CHECK(!(n->flags & NODE_NOUPDATE));
			CHECK(!(n->flags & NODE_NO_RENDERNODE));
		}
		if (n->index < 0) {
			continue;
		}
		if (n->obj3D) {
			if ((n->flags & NODE_BIP) && !(n->flags & NODE_NO_RENDER)) {
				E3D_LogBlue((&g_FileLog), "WARN: BIP node with NO_RENDER not set: %s", n->name);
			}
			if (n->flags & NODE_NO_RENDER) {
				if (n->flags & NODE_BIP) {
					CHECK(CNode3D_Class(n) == CLASS_OBJ3D_TRIMESH);
					CNode3D_FreeObj3D(n);
				} else {
					E3D_LogInfo((&g_FileLog), "NOT deleted object from node: %s", n->name);
				}
			}
			if (n->obj3D) {
				continue;
			}
		}
		n->flags |= NODE_NO_RENDER;
		if (!(n->flags & NODE_BONE) && n->Refs - (n->prev != NULL) - (n->next != NULL) == 0) {
			if (n->flags & NODE_EFFECT) {
				Con_Warnf("WARN: NODO_NO_TRANS+NODO_EFFECT: %s\n", n->name);
			}
			n->flags |= NODE_NO_TRANS_TAB;
			if (!Node_IsOfGame(n)) {
				E3D_LogInfo((&g_FileLog), "NODO_NO_TRANS -> hidden: %s", n->name);
				n->flags |= NODE_DELETE;
			} else {
				E3D_LogWhite((&g_FileLog), "NODO_NO_TRANS: %s", n->name);
			}
		}
	}
	/* then the empty nodes whose children all left the tables, until none is left */
	pass = 1;
	do {
		change = 0;
		for (n = scn->Node3D; n; n = n->next) {
			uint32_t flags = n->flags;

			if (n->index < 0 || n->obj3D || (flags & (NODE_BONE | NODE_NO_RENDERNODE))) {
				continue;
			}
			cnt = n->Refs - (n->prev != NULL) - (n->next != NULL);
			for (c = scn->Node3D; c; c = c->next) {
				if (c->parent == n && (c->flags & NODE_NO_RENDERNODE)) {
					cnt--;
				}
			}
			if (cnt) {
				continue;
			}
			CHECK(flags & NODE_NO_RENDER);
			n->flags |= NODE_NO_TRANS_TAB;
			change = 1;
			E3D_LogInfo((&g_FileLog), "NODO_NO_TRANS__%d -> hidden: %s", pass, n->name);
			n->flags |= NODE_DELETE;
		}
		pass++;
	} while (change);
	if (g_OptimizeNodes & 2) {
		Scene_CollapseBip(scn);
	}
}

/* the color of "LuzAmbiente:" or "ColorFondo:" with the intensity 1 */
static void M3D_ReadSceneColor(const char *tok, ColorIntens *c) {
	vec3_t v;

	M3D_ReadVec3(tok, &v, 0);
	c->rgba = M3D_Color(&v);
	c->intens = 1.0f;
}

char g_M3DDir[260];

CIniFile *g_M3DFileINI;

char g_M3DSceneMax[260];

M3DTable *g_M3DSceneTables;

/* the M3D file name (the model or, with g_SceneAnimParent set, an animation of it; the .CM3 of an animation first),
 * name2 the node of a game model to make its root; arg is not used */
CScene *Scene_Load(const char *name, int arg, const char *name2) {
	char hdr[M3D_HDR], type[0x10];
	CScene *scn, *cm3;
	CMatList *lm;
	CNode3D *node, *root;
	void *p;
	int labels, nObj, nMat, fps = 0, i;
	byte castShadows, rcvShadows, bStatic, bAnim;

	(void)arg;
	String_Copy(g_M3DName, name, MAX_FILEPATH);
	Path_Part(g_M3DDir, g_M3DName, 3);
	Path_Part(g_M3DBaseName, g_M3DName, 4);
	String_Copy(g_M3DTexturePath, g_M3DDir, MAX_FILEPATH);
	g_M3DOk = 1;
	if (g_SceneAnimParent && g_CM3UseScenes == 1 && (cm3 = CM3_Load(g_M3DName)) != NULL) {
		return cm3;
	}
	E3D_LogDebug((&g_FileLog), "M3D: %s", g_M3DName);
	castShadows = 1;
	rcvShadows = 1;
	CHECK(!g_M3DFileINI);
	p = operator_new(sizeof(CIniFile));
	g_M3DFileINI = p ? CIniFile_ctor(p) : NULL;
	if (!CIniFile_Load(g_M3DFileINI, g_M3DName, ".ini", 0)) {
		E3D_LogDebug((&g_FileLog), "INI: %s.ini", g_M3DBaseName);
		castShadows = CIniFile_GetBool(g_M3DFileINI, "Model", "CastShadows", 1);
		rcvShadows = CIniFile_GetBool(g_M3DFileINI, "Model", "RcvShadows", 1);
		CIniFile_GetText(g_M3DFileINI, "Model", "TexturePath", g_M3DTexturePath, MAX_FILEPATH, NULL);
	} else if (g_M3DFileINI) {
		CIniFile *ini = g_M3DFileINI;

		CIniFile_dtor(ini);
		operator_delete_sized(ini, sizeof(CIniFile));
		g_M3DFileINI = NULL;
	}
	g_M3DUseOpacity = 1;
	g_M3DUseEnvmap = 1;
	g_M3DUseBumpmap = 1;
	g_M3DUseGlowmap = 1;
	g_M3DPreScaleU = 1.0f;
	g_M3DPreScaleV = 1.0f;
	if (g_M3DFileINI) {
		g_M3DUseOpacity = CIniFile_GetBool(g_M3DFileINI, "Model", "UseOpacity", 1);
		g_M3DUseEnvmap = CIniFile_GetBool(g_M3DFileINI, "Model", "UseEnvmap", 1);
		g_M3DUseBumpmap = CIniFile_GetBool(g_M3DFileINI, "Model", "UseBumpmap", 1);
		g_M3DUseGlowmap = CIniFile_GetBool(g_M3DFileINI, "Model", "UseGlowmap", 1);
		g_M3DPreScaleU = CIniFile_GetFloat(g_M3DFileINI, "Model", "PreScaleU", 1.0f);
		g_M3DPreScaleV = CIniFile_GetFloat(g_M3DFileINI, "Model", "PreScaleV", 1.0f);
	}
	/* the header "[M3D,BIN,ETIQ:si]", read as binary */
	Read_Open(NULL, g_M3DName, 1);
	i = 0;
	do {
		int c = Read_Byte();

		hdr[i++] = (char)c;
		if (c == ']') {
			break;
		}
	} while (i < M3D_HDR - 1);
	hdr[i] = '\0';
	if (Proc_Scan(hdr, "[M3D,&.*t,ETIQ:&(tipo_si_no)l]", sizeof(type) - 1, type, &labels) != 2) {
		Err_Report(g_ErrPcxFormat, 1, "\narchivo: '%s'", g_M3DName);
	}
	if (strcmp(type, "TXT")) {
		CHECK(!strcmp(type, "BIN"));
	}
	g_M3DBinary = !strcmp(type, "BIN");
	g_M3DLabels = labels != 0;
	if (!g_M3DBinary) {
		/* a text file again from the start, after the header */
		Read_Close();
		Read_Open(NULL, g_M3DName, 0);
		while (Read_Byte() != ']') {
		}
	}
	if (!g_M3DBinary || g_M3DLabels) {
		Err_Printf(0, "File exported in text mode:\nM3D: %s\ncab: %s", g_M3DName, hdr);
	}
	g_M3DNumMat = 0;
	g_M3DNodes = NULL;
	M3D_ReadString("Formato:", g_M3DString, M3D_STRING, 0);
	CHECK(!strcmp(g_M3DString, "M3D"));
	g_M3DVersion = M3D_ReadInt("Version:");
	CHECK(g_M3DVersion >= 5 && g_M3DVersion <= 14);
	M3D_ReadString("Generado:", g_M3DString, M3D_STRING, 0);
	M3D_ReadString("Fecha:", g_M3DString, M3D_STRING, 0);
	M3D_ReadString("Hora:", g_M3DString, M3D_STRING, 0);
	M3D_ReadString("Escena:", g_M3DSceneMax, MAX_FILEPATH, 0);
	bStatic = M3D_ReadBool("Estatica:");
	if (bStatic) {
		M3D_ReadInt("Frame:");
	}
	bAnim = M3D_ReadBool("Animacion:");
	if (bAnim) {
		g_M3DFirstFrame = M3D_ReadInt("Primer_frame:");
		g_M3DLastFrame = M3D_ReadInt("Ultimo_frame:");
		fps = M3D_ReadInt("Frames_por_segundo:");
		CHECK(g_M3DFirstFrame <= g_M3DLastFrame);
		g_M3DFrames = g_M3DLastFrame - g_M3DFirstFrame + 1;
	}
	nObj = M3D_ReadInt("Num_objetos:");
	CHECK(nObj > 0);
	if (g_SceneAnimParent) {
		if (bStatic) {
			Err_Printf(0, "Animation file with model information:\nM3D: %s\nMAX: %s", g_M3DName, g_M3DSceneMax);
		}
		if (!bAnim) {
			Err_Printf(0, "Animation file without animation information:\nM3D: %s\nMAX: %s", g_M3DName,
				g_M3DSceneMax);
		}
	} else if (!bStatic) {
		Err_Printf(0, "Model file without model information:\nM3D: %s\nMAX: %s", g_M3DName, g_M3DSceneMax);
	}
	p = operator_new(sizeof(CScene));
	scn = p ? CScene_ctor(p) : NULL;
	String_Set(&scn->name, g_M3DName);
	scn->ini = g_M3DFileINI;
	g_M3DFileINI = NULL;
	if (bAnim) {
		M3DTable *t = operator_new(6 * sizeof(M3DTable));

		if (t) {
			for (i = 0; i < 6; i++) {
				t[i].n = 0;
				t[i].p = NULL;
				t[i].fixed = 0;
			}
		}
		g_M3DSceneTables = t;
		p = operator_new(sizeof(CAnimation));
		scn->AnimM3D = p ? CAnimation_ctor(p) : NULL;
		String_Set(&scn->AnimM3D->name, g_M3DBaseName);
		CAnimation_Init(scn->AnimM3D, nObj, g_M3DFrames, g_M3DFirstFrame);
		scn->AnimM3D->fps = (float)fps;
	}
	g_M3DNodes = New_Objects((uint32_t)nObj, sizeof(M3DNode), (void *)M3DNode_ctor, (void *)M3DNode_dtor);
	if (bStatic) {
		M3D_ReadSceneColor("LuzAmbiente:", &scn->ambient);
	}
	if (bAnim) {
		M3DTable_ReadInt(&g_M3DSceneTables[0], "LuzR");
		M3DTable_ReadInt(&g_M3DSceneTables[1], "LuzG");
		M3DTable_ReadInt(&g_M3DSceneTables[2], "LuzB");
	}
	if (g_M3DVersion > 5) {
		CHECK(!M3D_ReadBool("MapaEntorno:"));
		if (bStatic) {
			M3D_ReadSceneColor("ColorFondo:", &scn->fog);
		}
		if (bAnim) {
			M3DTable_ReadInt(&g_M3DSceneTables[3], "LuzR");
			M3DTable_ReadInt(&g_M3DSceneTables[4], "LuzG");
			M3DTable_ReadInt(&g_M3DSceneTables[5], "LuzB");
		}
	}
	CHECK(!g_M3DLMat);
	lm = operator_new(sizeof(CMatList));
	if (lm) {
		lm->Name = NULL;
		lm->vftable = CMATLIST_VFTABLE;
		lm->last = NULL;
		lm->first = NULL;
	}
	g_M3DLMat = lm;
	nMat = M3D_ReadInt("Materiales:");
	for (i = 0; i < nMat; i++) {
		M3D_ReadMaterial(g_M3DLMat, i, -1);
	}
	if (!bStatic) {
		if (g_M3DLMat) {
			g_M3DLMat->vftable->Delete(g_M3DLMat, 1);
			g_M3DLMat = NULL;
		}
		g_M3DNumMat = 0;
	}
	M3D_ShareMats(g_M3DLMat);
	p = operator_new(sizeof(CNode3D));
	node = p ? CNode3D_ctor(p) : NULL;
	String_Set(&node->name, "_raiz_escena");
	node->flags |= NODE_ROOT;
	CScene_AddNode(scn, node);
	for (i = 0; i < nObj; i++) {
		M3D_ReadObject(scn, i, bStatic, bAnim, castShadows, rcvShadows);
	}
	M3D_NextLine();
	M3D_ReadString(NULL, g_M3DString, M3D_STRING, 0);
	CHECK(!strcmp(g_M3DString, "Fin"));
	Read_Close();
	Scene_UsedMats(scn);
	Scene_FlagNames(scn);
	if (!g_SceneAnimParent) {
		Scene_FlagsIni(scn);
		Scene_Hierarchy(scn);
	} else {
		Scene_AnimParents(scn, g_SceneAnimParent);
	}
	root = NULL;
	if (!g_SceneAnimParent) {
		root = M3D_ModelRoot(scn, name2);
	}
	Scene_Sort(scn);
	Scene_AnimLocal(scn, bAnim);
	if (root) {
		root->flags |= NODE_POSTFIX;
		root->pos.x = 0.0f;
		root->pos.y = 0.0f;
		root->pos.z = 0.0f;
	}
	M3D_Targets(scn);
	Scene_MatricesObj(scn);
	Scene_PhysiqueSkin(scn);
	if (g_M3DNodes) {
		Delete_Objects(g_M3DNodes, sizeof(M3DNode), (void *)M3DNode_dtor);
		g_M3DNodes = NULL;
	}
	if (g_M3DSceneTables) {
		M3DTable *t = g_M3DSceneTables;

		M3DTable6_dtor(t);
		operator_delete_sized(t, 6 * sizeof(M3DTable));
		g_M3DSceneTables = NULL;
	}
	E3D_LogWhite((&g_FileLog), "cleaning nodes (pass %s):", "3");
	CScene_DeleteEmpty(scn, 0, 0, 1);
	if (g_ShowNodesTree && !g_SceneAnimParent) {
		Scene_ShowTree(scn, NULL, 0);
	}
	if (bStatic && !g_SceneAnimParent) {
		Scene_Sprites(scn);
		Scene_Flares(scn);
		Scene_Particles(scn);
	}
	if ((g_OptimizeNodes & 1) && !g_SceneAnimParent) {
		M3D_OptimizeNodes(scn);
	}
	for (node = scn->Node3D; node; node = node->next) {
		Scene_CheckDuplicates(scn, node);
	}
	CScene_Join(scn, 0, "(+merge_nodes)");
	E3D_LogWhite((&g_FileLog), "cleaning nodes (pass %s):", "4");
	CScene_DeleteEmpty(scn, 0, 0, 1);
	if (g_ShowNodesTree && !g_SceneAnimParent) {
		Scene_ShowTree(scn, NULL, 0);
	}
	if (g_SceneAnimParent) {
		AnimGroup_Compact(scn);
		AnimGroup_AlignNode(g_SceneAnimParent, scn, g_SceneAnimParent->node);
		AnimGroup_MapNodes(g_SceneAnimParent, scn);
		CScene_DeleteNodes(scn);
	} else if (bAnim) {
		CHECK(bStatic);
		AnimGroup_MapNodes(scn, scn);
		E3D_LogVerbose((&g_FileLog), "INFO: Model with embedded animation: %s", scn->name);
	}
	Scene_NodeTable(scn);
	if (g_SceneLODParent) {
		CHECK(!g_SceneAnimParent);
		CHECK(bStatic && !bAnim);
		Scene_SetupLOD(scn);
	}
	if (g_SceneAnimParent && g_CM3UseScenes) {
		CM3_Save(g_M3DName, scn);
	}
	return scn;
}

/* ---- the static initializers; name and description are the strings of the exe ---- */


cGVar g_ShowNodesTreeVar;

void ReadM3D_InitShowNodesTree(void) {
	/* "ShowNodesTree", "Show nodes tree at model M3D loading time (0/1)" */
	GVar_InitInt((&g_ShowNodesTreeVar), "ShowNodesTree", 0, "Show nodes tree at model M3D loading time (0/1)", 0);
}

cGVar g_UseWSMVar;

void ReadM3D_InitUseWSM(void) {
	/* "UseWSM", "Use ObjTMAfterWSM (0/1)" */
	GVar_InitInt((&g_UseWSMVar), "UseWSM", 0, "Use ObjTMAfterWSM (0/1)", 0);
}

cGVar GVar_CM3_DeleteM3D;

cGVar GVar_OptimizeNodes;

cGVar GVar_CM3_UseScenes;

cGVar GVar_SM3_CheckTime;

cGVar GVar_SharedTexuresPath;

cGVar GVar_CM3_CheckTime;

cGVar GVar_SM3_UseScenes;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_OptimizeNodes, &GVar_OptimizeNodes, "OptimizeNodes", 0, "0=none, 1=delete unused nodes, 2=colapse to bip nodes, 4=AniMask, 7=all", 7)
INIT_GVAR_INT(Init_GVar_CM3_UseScenes, &GVar_CM3_UseScenes, "CM3_UseScenes", 0, "Read/Write CM3 files (0/1/2)", 1)
INIT_GVAR_INT(Init_GVar_CM3_CheckTime, &GVar_CM3_CheckTime, "CM3_CheckTime", 0, "Check filetime of CM3 files (0/1)", 1)
INIT_GVAR_INT(Init_GVar_CM3_DeleteM3D, &GVar_CM3_DeleteM3D, "CM3_DeleteM3D", 0, "Delete M3D file after CM3 creation (0/1)", 0)
INIT_GVAR_INT(Init_GVar_SM3_UseScenes, &GVar_SM3_UseScenes, "SM3_UseScenes", 0, "Read/Write SM3 files (0/1/2)", 1)
INIT_GVAR_INT(Init_GVar_SM3_CheckTime, &GVar_SM3_CheckTime, "SM3_CheckTime", 0, "Check filetime of SM3 files (0/1)", 1)
FINI_GVAR_STRING(Fini_GVar_SharedTexuresPath, &GVar_SharedTexuresPath)
INIT_GVAR_STRING(Init_GVar_SharedTexuresPath, Fini_GVar_SharedTexuresPath, &GVar_SharedTexuresPath, "SharedTexuresPath", 0, "Default path for shared textures ($name.bmp)", "2D/Textures/Shared/")
