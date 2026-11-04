/*
 * The base class of the graphs CNodesWorld and CNodesWorld2D, original source AI/NodesWorldSup.cpp with the
 * instances of Global/DList.h and Global/GridElements.h it uses (include/scrap/nodesworldsup.h).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/nodesworld2d.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"

#define NWGRID_VFTABLE (&cGridElements_InfPointVis_vftable)
#define CNODESWORLDSUP_VFTABLE (&CNodesWorldSup_vftable)
static const cGridElementsVtbl cGridElements_InfPointVis_vftable;
static const CNodesWorldVtbl CNodesWorldSup_vftable;

/* ---- Global/DList.h ---- */

/* the free nodes of the point list deleted with their links */
static void NWPointList_FreePool(cNWPointList *list) {
	while (list->freeNodes) {
		cNWPointNode *p = list->freeNodes;
		list->freeNodes = p->next;
		NWIntList_Free(&p->links);
		operator_delete(p);
	}
	list->numFree = 0;
}

static void NWVisList_Add(cNWVisList *list, const cNWVis *vis) {
	cNWVisNode *node;

	if (list->freeNodes) {
		node = DList_NewNode(list, sizeof(*node));
	} else {
		list->iter = NULL;
		node = operator_new(sizeof(*node));
		if (node) {
			memset(&node->vis.points, 0, sizeof(node->vis.points));
		}
	}
	node->vis = *vis;
	DList_LinkFirst(list, node);
}

/* to the free list while it has room, else the points and the node deleted */
static void NWVisList_Release(cNWVisList *list, cNWVisNode *node) {
	if (list->numFree < list->maxFree) {
		node->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = node;
		return;
	}
	if (node) {
		NWIntList_Free(&node->vis.points);
		operator_delete(node);
	}
}

static void NWVisList_RemoveHead(cNWVisList *list) {
	cNWVisNode *p = list->head;

	list->iter = NULL;
	if (!p) {
		return;
	}
	if (p == p->next) {
		list->head = NULL;
	} else {
		list->head = p->next;
		p->prev->next = p->next;
		p->next->prev = p->prev;
	}
	NWVisList_Release(list, p);
	list->count--;
}

static void NWVisList_RemoveIter(cNWVisList *list) {
	cNWVisNode *cur = list->iter;

	if (!cur) {
		return;
	}
	if (list->head == cur) {
		NWVisList_RemoveHead(list);
		list->iter = list->head;
		return;
	}
	cNWVisNode *next = cur->next;
	cur->prev->next = next;
	cur->next->prev = cur->prev;
	NWVisList_Release(list, cur);
	if (next == list->head) {
		next = NULL;
	}
	list->count--;
	list->iter = next;
}

static void NWVisList_Clear(cNWVisList *list) {
	while (list->head) {
		NWVisList_RemoveHead(list);
	}
}

void NWVecList_Append(cNWVecList *list, const vec3_t *v) {
	cNWVecNode *node = DList_NewNode(list, sizeof(*node));

	node->v = *v;
	DList_LinkLast(list, node);
}

static void NWIntList_Append(cNWIntList *list, void *const *data) {
	cNWIntNode *node = DList_NewNode(list, sizeof(*node));

	node->data = *data;
	DList_LinkLast(list, node);
}

/* ---- Global/GridElements.h ---- */

static cGridElements_InfPointVisPtr *NWGrid_ctor(cGridElements_InfPointVisPtr *self, const vec3_t *max,
	const vec3_t *min, int nx, int ny, int nz) {
	return cGridElements_Init(self, NWGRID_VFTABLE, max, min, nx, ny, nz);
}

static void NWGrid_Clear(cGridElements_InfPointVisPtr *self) {
	cGridElements_FreeCells(self, (void(*)(void *))NWIntList_Clear);
}

static void NWGrid_Free(cGridElements_InfPointVisPtr *self) {
	self->vftable = NWGRID_VFTABLE;
	NWGrid_Clear(self);
	operator_delete(self);
}

static byte NWGrid_Add(cGridElements_InfPointVisPtr *self, const vec3_t *p, const void *data) {
	CHECK(!((p->x > self->max.x) || (p->x < self->min.x) || (p->y > self->max.y) || (p->y < self->min.y) ||
		(p->z > self->max.z) || (p->z < self->min.z)));
	int i = GridElems_CellIndex(self, p);
	cNWIntList **cells = (cNWIntList **)self->cells;
	if (!cells[i]) {
		cNWIntList *cell = operator_new(sizeof(*cell));
		if (cell) {
			memset(cell, 0, sizeof(*cell));
		}
		cells[i] = cell;
	}
	NWIntList_Add(cells[i], (void *const *)data);
	return 1;
}

/* ---- CNodesWorldSup ---- */

CNodesWorld *CNodesWorldSup_ctor(CNodesWorld *self) {
	self->sup.vftable = CNODESWORLDSUP_VFTABLE;
	/* the fields from halfNode to nodeInit, grid to extraPoints and numFiles to i358; the padding, the rest of
	 * name and fileStart stay as they were */
	memset(&self->sup.halfNode, 0, offsetof(CNodesWorld, sup.created) - offsetof(CNodesWorld, sup.halfNode));
	self->sup.created = 0;
	memset(&self->sup.min, 0, sizeof(self->sup.min) + sizeof(self->sup.max));
	self->sup.name[0] = 0;
	self->sup.nodeInitDummy = 1;
	memset(&self->sup.grid, 0, offsetof(CNodesWorld, sup.fileStart) - offsetof(CNodesWorld, sup.grid));
	memset(&self->sup.numFiles, 0, offsetof(CNodesWorld, sup.unused_35c) - offsetof(CNodesWorld, sup.numFiles));
	self->sup.unused_35c = 0;
	return self;
}

/* a list of 0x14 nodes emptied and its free nodes deleted */
static void NWSup_FreeVecList(cNWVecList *list, void(*clear)(cNWVecList *list)) {
	clear(list);
	DList_FreePool(list);
}

void CNodesWorldSup_dtor(CNodesWorld *self) {
	self->sup.vftable = CNODESWORLDSUP_VFTABLE;
	NWSup_FreeVecList(&self->sup.errOneWay, DListVec3_Clear);
	NWSup_FreeVecList(&self->sup.errUnlinked, DListVec3_Clear);
	NWSup_FreeVecList(&self->sup.errCollide, DListVec3_Clear);
	for (int i = 4; i >= 0; i--) {
		NWSup_FreeVecList(&self->sup.cellLists[i], DListVec3_Clear);
	}
	NWSup_FreeVecList(&self->sup.foundNodes[1], NWVecList_Clear);
	NWSup_FreeVecList(&self->sup.foundNodes[0], NWVecList_Clear);
	NWSup_FreeVecList(&self->sup.extraPoints, DListVec3_Clear);
	NWIntList_Free(&self->sup.edgeCells);
	NWPointList_Clear(&self->sup.points);
	NWPointList_FreePool(&self->sup.points);
}

/* CNodesWorldSup_ctor, _dtor, _FreeGraph and _LoadGraph: nodesworld.h */
static void CNodesWorldSup_Free(CNodesWorld *self) {
	CNodesWorldSup_dtor(self);
	operator_delete(self);
}

void CNodesWorldSup_GetBox(CNodesWorld *self, vec3_t *min, vec3_t *max) {
	*min = self->sup.min;
	*max = self->sup.max;
}

/* the path of graph file i: <path>.pth, <path>1.pth ..., cut at the 0x100 bytes of the original buffer */
static void NWSup_FilePath(char *buf, const char *path, int i) {
	char num[8];
	size_t n = strnlen(path, 0xff);

	memcpy(buf, path, n);
	buf[n] = 0;
	if (i > 0) {
		crt_sprintf(num, "%d", i);
		size_t k = strnlen(num, sizeof(num) - 1);
		if (n + k > 0xff) {
			k = 0xff - n;
		}
		memcpy(buf + n, num, k);
		n += k;
		buf[n] = 0;
	}
	size_t k = n + 4 > 0xff ? 0xff - n : 4;
	memcpy(buf + n, ".pth", k);
	buf[n + k] = 0;
}

/* the first point of the graph file with the position of p, NULL if there is none */
static cNWNode *NWSup_FindNode(cNWNode *nodes, int first, int n, const vec3_t *p) {
	for (int m = first; m < first + n; m++) {
		cNWNode *q = &nodes[m];
		if (p->x == q->pos.x && p->y == q->pos.y && p->z == q->pos.z) {
			return q;
		}
	}
	return NULL;
}

/* the inf file of the graph: the node size and the box of the level, after the path itself is saved */
byte CNodesWorldSup_SaveInfPath(CNodesWorld *self, const char *path) {
	cFilePack f;
	byte ok;

	if (!NodesWorld_SavePath(self)) {
		return 0;
	}
	Con_Printf("Saving Inf. AI...\n");
	crt_chmod(path, 0x180);
	cFilePack_ctor(&f);
	if (!cFilePack_Open(&f.file.base, path, 6)) {
		Con_Bluef("ERROR AI: File Inf. Path AI no abierto.\n");
		ok = 0;
	} else {
		cFilePack_Write(&f.file.base, &self->sup.nodeSize, sizeof(self->sup.nodeSize));
		cFilePack_Write(&f.file.base, &self->sup.min, sizeof(self->sup.min));
		cFilePack_Write(&f.file.base, &self->sup.max, sizeof(self->sup.max));
		if (!cFilePack_Close(&f.file.base)) {
			Con_Bluef("ERROR AI: File Inf. Path AI no cerrado.\n");
			ok = 0;
		} else {
			ok = 1;
		}
	}
	cFilePack_dtor(&f);
	return ok;
}

byte CNodesWorldSup_LoadGraph(CNodesWorld *self, const char *path) {
	char buf[0x100];
	cFilePack f;
	int n, file, links = 0;

	self->sup.created = 0;
	self->sup.numNodes = 0;
	/* the number of nodes of all files */
	for (file = 0;; file++) {
		NWSup_FilePath(buf, path, file);
		cFilePack_ctor(&f);
		if (!cFilePack_Exists(&f.file.base, buf)) {
			cFilePack_dtor(&f);
			if (!file) {
				return 0;
			}
			break;
		}
		if (!cFilePack_Open(&f.file.base, buf, 1)) {
			Con_Bluef("ERROR AI: File Path AI no abierto.\n");
			CHECK(!"CNodesWorldSup_LoadPath_File_Path_Not_Open");
			cFilePack_dtor(&f);
			return 0;
		}
		cFilePack_Read(&f.file.base, &n, 4);
		self->sup.numNodes += n;
		if (!cFilePack_Close(&f.file.base)) {
			Con_Bluef("ERROR AI: File Path AI no cerrado.\n");
			CHECK(!"CNodesWorldSup_LoadPath_File_Path_Not_Closed");
			cFilePack_dtor(&f);
			return 0;
		}
		cFilePack_dtor(&f);
	}
	self->sup.nodes = Mem_Alloc(self->sup.numNodes * sizeof(cNWNode));
	if (!self->sup.nodes) {
		Con_Bluef("ERROR: LoadPathAI - Insuficiente memoria Buffer Points Vis.\n");
		CHECK(!"cNodesWorldSup_LoadPathAI_new_BufferPointVis_Fail");
	}
	/* the positions of the nodes of a file, then their links to nodes of the same file */
	int first = 0;
	for (file = 0;; file++) {
		NWSup_FilePath(buf, path, file);
		cFilePack_ctor(&f);
		if (!cFilePack_Exists(&f.file.base, buf)) {
			cFilePack_dtor(&f);
			if (!file) {
				return 0;
			}
			break;
		}
		if (!cFilePack_Open(&f.file.base, buf, 1)) {
			Con_Bluef("ERROR AI: File Path AI no abierto.\n");
			CHECK(!"CNodesWorldSup_LoadPath_File_Path_Not_Open");
			cFilePack_dtor(&f);
			return 0;
		}
		cFilePack_Read(&f.file.base, &n, 4);
		for (int k = first; k < n + first; k++) {
			cFilePack_Read(&f.file.base, &self->sup.nodes[k].pos, sizeof(vec3_t));
		}
		self->sup.fileStart[self->sup.numFiles] = &self->sup.nodes[first];
		self->sup.numFiles++;
		for (int k = first; k < n + first; k++) {
			cNWNode *node = &self->sup.nodes[k];
			cFilePack_Read(&f.file.base, &node->numLinks, 4);
			node->links = Mem_Alloc(node->numLinks * sizeof(*node->links));
			if (!node->links) {
				Con_Bluef("ERROR: LoadPathAI - Insuficiente memoria Buffer Links Points Vis.\n");
				CHECK(!"cNodesWorldSup_LoadPathAI_new_BufferPointVis_Fail");
			}
			for (int j = 0; j < self->sup.nodes[k].numLinks; j++) {
				vec3_t p;
				cFilePack_Read(&f.file.base, &p, sizeof(p));
				cNWNode *q = NWSup_FindNode(self->sup.nodes, first, n, &p);
				if (q) {
					self->sup.nodes[k].links[j] = q;
					continue;
				}
				Con_Bluef("ERROR: LoadPathAI - Error b\xfa" "squeda links.\n");
				CHECK(!"cNodesWorldSup_LoadPathAI_Load_Links_Fail");
			}
			links += self->sup.nodes[k].numLinks;
		}
		if (!cFilePack_Close(&f.file.base)) {
			Con_Bluef("ERROR AI: File Path AI no cerrado.\n");
			CHECK(!"CNodesWorldSup_LoadPath_File_Path_Not_Closed");
			cFilePack_dtor(&f);
			return 0;
		}
		first += n;
		cFilePack_dtor(&f);
	}
	Con_Printf("\nNUM. POINTS OF VISIBILITY LOADING GRAPH: %d\n", self->sup.numNodes);
	Con_Printf("NUM. LINKS BETWEEN POINTS VIS. LOADING GRAPH: %d\n", links / 2);
	Con_Printf("\nMEMORY USED BY GRAPH: NODES: %d LINKS: %d bytes\n", self->sup.numNodes * 0x14, links * 4);
	Con_Printf("TOTAL MEMORY USED BY GRAPH: %d bytes\n", (links + self->sup.numNodes * 5) * 4);
	return 1;
}

byte CNodesWorldSup_Create(CNodesWorld *self) {
	if (!NodesWorld_LoadGraph(self)) {
		return 0;
	}
	self->sup.created = 1;
	return 1;
}

int CNodesWorldSup_FileOf(CNodesWorld *self, const cNWNode *node) {
	uintptr_t p = (uintptr_t)node;
	uintptr_t base = (uintptr_t)self->sup.nodes;

	if (p < base || p > base + (uintptr_t)(self->sup.numNodes * 5 - 5) * 4) {
		return -1;
	}
	if (self->sup.numFiles == 1) {
		return 0;
	}
	int n = self->sup.numFiles - 1;
	for (int i = 0; i < n; i++) {
		if (p >= (uintptr_t)self->sup.fileStart[i] && p < (uintptr_t)self->sup.fileStart[i + 1]) {
			return i;
		}
	}
	return n;
}

int CNodesWorldSup_CommonFile(CNodesWorld *self, cNWNode *const *nodes, int n) {
	if (n <= 0) {
		return -1;
	}
	int file = NodesWorld_FileOf(self, nodes[0]);
	for (int i = 1; i < n; i++) {
		if (file != NodesWorld_FileOf(self, nodes[i])) {
			return -1;
		}
	}
	return file;
}

/* the middle of the buffer cell fx fy fz */
static vec3_t *NWSup_CellPos(const CNodesWorld *self, vec3_t *out, float fx, float fy, float fz) {
	out->x = ((self->sup.min.x + self->sup.axis[0].x * fx) + self->sup.axis[1].x * fy) + self->sup.axis[2].x * fz + self->sup.halfNode.x;
	out->y = self->sup.halfNode.y + (((self->sup.min.y + self->sup.axis[0].y * fx) + self->sup.axis[1].y * fy) + self->sup.axis[2].y * fz);
	out->z = self->sup.halfNode.z + (((self->sup.min.z + self->sup.axis[0].z * fx) + self->sup.axis[1].z * fy) + self->sup.axis[2].z * fz);
	return out;
}

vec3_t *CNodesWorldSup_SnapPos(CNodesWorld *self, vec3_t *out, const vec3_t *pos) {
	float fz = (float)(int)((pos->z - self->sup.min.z) / self->sup.nodeSize.z);
	float fy = (float)(int)((pos->y - self->sup.min.y) / self->sup.nodeSize.y);
	float fx = (float)(int)((pos->x - self->sup.min.x) / self->sup.nodeSize.x);

	return NWSup_CellPos(self, out, fx, fy, fz);
}

void CNodesWorldSup_OtherLinks(CNodesWorld *self, const cNWNode *node, const vec3_t *pos, cNWNode **list,
	int max, int *count) {
	(void)self;
	*count = 0;
	for (int i = 0; i < node->numLinks; i++) {
		cNWNode *l = node->links[i];
		if (l->pos.x != pos->x || l->pos.y != pos->y || l->pos.z != pos->z) {
			list[*count] = l;
			(*count)++;
			if (*count >= max) {
				return;
			}
		}
	}
}

byte CNodesWorldSup_IsCreated(CNodesWorld *self) {
	return self->sup.created;
}

vec3_t *CNodesWorldSup_LastNodePos(CNodesWorld *self, vec3_t *out) {
	self->sup.nodeInit = self->sup.nodes[self->sup.numNodes - 1].pos;
	*out = self->sup.nodeInit;
	return out;
}

const vec3_t *CNodesWorldSup_GetVisPoint(CNodesWorld *self, int i) {
	CHECK(i >= 0 && i < self->sup.numNodes);
	return &self->sup.nodes[i].pos;
}

int CNodesWorldSup_NumVisPoints(CNodesWorld *self) {
	return self->sup.numNodes;
}

float CNodesWorldSup_GetMaxVisDist(CNodesWorld *self) {
	return self->sup.maxVisDist;
}

byte CNodesWorldSup_InGrid(CNodesWorld *self, const vec3_t *pos) {
	return GridElems_IsInside(self->sup.grid, pos);
}

byte CNodesWorldSup_CheckBuffer(CNodesWorld *self) {
	vec3_t p;

	for (int i = 0; i < self->sup.nx * self->sup.ny * self->sup.nz; i++) {
		NodesWorld_BufferPos(self, &p, i);
		int k = NodesWorld_BufferIndex(self, &p);
		if (i != k) {
			Con_FatalErrorf("ERROR CheckGetsBuffer: Indice Preguntado: %d !=  Indice Respuesta: %d\n", i, k);
			return 0;
		}
	}
	return 1;
}

void CNodesWorldSup_BuildGrid(CNodesWorld *self) {
	vec3_t top = self->sup.max;

	if (g_IsMapOutDoor) {
		char buf[20];
		vec3_t pos;
		float ang[3];
		byte flag;

		DummySearch_Start("DM_MapTop", 0);
		if (!DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
			Con_Bluef("ERROR AI: Dummy DM_MapTop no encontrado.\n");
			CHECK(!"CNodesWorld_LoadGridNodes_DM_MapTop_Not_Found");
		}
		top.y = pos.y + 5000.0f;
	}
	int nx = (int)ceil((double)((top.x - self->sup.min.x) / self->sup.gridCell));
	double ny = ceil((double)((top.y - self->sup.min.y) / self->sup.gridCell));
	double nz = ceil((double)((top.z - self->sup.min.z) / self->sup.gridCell));
	cGridElements *grid = operator_new(sizeof(cGridElements));
	if (grid) {
		grid = NWGrid_ctor(grid, &top, &self->sup.min, nx, (int)ny, (int)nz);
	}
	self->sup.grid = grid;
	CHECK(self->sup.grid != NULL);
	for (int i = 0; i < self->sup.numNodes; i++) {
		cNWNode *node = &self->sup.nodes[i];
		GridElems_Add(self->sup.grid, &node->pos, (void *const *)&node);
	}
}

int CNodesWorldSup_BufferIndex(CNodesWorld *self, const vec3_t *pos) {
	int i = ((int)((pos->x - self->sup.min.x) / self->sup.nodeSize.x) * self->sup.ny + (int)((pos->y - self->sup.min.y) /
		self->sup.nodeSize.y)) * self->sup.nz + (int)((pos->z - self->sup.min.z) / self->sup.nodeSize.z);

	CHECK((i < self->sup.dimBuffer) && (i >= 0));
	return i;
}

vec3_t *CNodesWorldSup_BufferPos(CNodesWorld *self, vec3_t *out, int index) {
	CHECK((index < self->sup.dimBuffer) && (index >= 0));
	int plane = self->sup.ny * self->sup.nz;
	int x = index / plane;
	int y = (index - plane * x) / self->sup.nz;
	int z = index - (self->sup.ny * x + y) * self->sup.nz;

	return NWSup_CellPos(self, out, (float)x, (float)y, (float)z);
}

void CNodesWorldSup_GetInfoNodes(CNodesWorld *self) {
	vec3_t p;
	byte closed = 1;

	Con_Printf("Get Info Nodes...\n");
	DListVec3_Clear(&self->sup.cellLists[1]);
	DListVec3_Clear(&self->sup.cellLists[2]);
	DListVec3_Clear(&self->sup.cellLists[3]);
	DListVec3_Clear(&self->sup.cellLists[0]);
	DListVec3_Clear(&self->sup.cellLists[4]);
	for (int i = 0; i < self->sup.dimBuffer; i++) {
		cNWVecList *l;
		switch (self->sup.buffer[i].state) {
		case 4:
			l = &self->sup.cellLists[1];
			break;
		case 5:
			l = &self->sup.cellLists[2];
			break;
		case 0:
			l = &self->sup.cellLists[3];
			break;
		case 3:
			l = &self->sup.cellLists[0];
			break;
		case 6:
			closed = 0;
			l = &self->sup.cellLists[4];
			break;
		default:
			continue;
		}
		DListVec3_Add(l, NodesWorld_BufferPos(self, &p, i));
	}
	if (closed && self->sup.nodeInitDummy) {
		Con_Warnf("WARNING AI: ESCENARIO PODRIA NO ESTAR CERRADO.\n");
		CHECK(!"CNodesWorldSup_GetInfNodes_Warning_Esc_Not_Closed");
	}
}

/* the set d is the set of the cell with the point p added (count words of d, merged in address order) */
static int NWSup_SameSet(CNodesWorld *self, int index, cNWVis *d, const vec3_t *p, int count) {
	if (count == 1) {
		d->points.iter = d->points.head;
		return d->points.head->data == p;
	}
	cNWVis *a = self->sup.buffer[index].vis;
	cNWIntNode *va = a->points.iter = a->points.head;
	cNWIntNode *vd = d->points.iter = d->points.head;
	while (vd) {
		if (!va) {
			return vd->data == p;
		}
		void *x = vd->data;
		if (va->data == x) {
			va = a->points.iter = a->points.iter->next;
			if (va == a->points.head) {
				va = NULL;
			}
		} else if (x != p) {
			return 0;
		}
		vd = d->points.iter = d->points.iter->next;
		if (vd == d->points.head) {
			vd = NULL;
		}
	}
	return 1;
}

void CNodesWorldSup_AddVis(CNodesWorld *self, const vec3_t *point, int index) {
	cNWVis *vis = self->sup.buffer[index].vis;
	cNWVis tmp;
	int count = 1, key = 0;

	if (vis) {
		count = vis->points.count + 1;
		key = vis->hash;
	}
	int h = NodesWorld_Hash(self, key, (int)(uintptr_t)point);
	cNWVisList *set = &self->sup.sets[h];
	cNWVisNode *n = set->iter = set->head;
	while (n) {
		cNWVis *d = &n->vis;
		if (count == d->points.count && NWSup_SameSet(self, index, d, point, count)) {
			NodesWorld_ReleaseVis(self, index);
			self->sup.buffer[index].vis = d;
			self->sup.buffer[index].vis->refs++;
			return;
		}
		n = set->iter = set->iter->next;
		if (n == set->head) {
			break;
		}
	}
	memset(&tmp, 0, sizeof(tmp));
	vis = self->sup.buffer[index].vis;
	if (!vis) {
		/* a new set with the point */
		NWVisList_Add(set, &tmp);
		set->iter = set->head;
		cNWVis *d = &set->head->vis;
		d->refs = 1;
		d->hash = h;
		NWIntList_Append(&d->points, (void *const *)&point);
		d->node = set->head;
		self->sup.buffer[index].vis = d;
		NWIntList_Free(&tmp.points);
		return;
	}
	if (vis->refs == 1) {
		/* the only user: the point into the set, the set moved to the list of the new hash */
		cNWIntNode *it = vis->points.iter = vis->points.head;
		while (it) {
			if ((uintptr_t)point < (uintptr_t)it->data) {
				NWIntList_AddBefore(&vis->points, (void *const *)&point);
				goto moved;
			}
			it = vis->points.iter = vis->points.iter->next;
			if (it == vis->points.head) {
				break;
			}
		}
		NWIntList_Append(&vis->points, (void *const *)&point);
	moved:
		vis = self->sup.buffer[index].vis;
		cNWVisNode *old = vis->node;
		NWVisList_Add(set, vis);
		self->sup.sets[key].iter = old;
		vis = self->sup.buffer[index].vis;
		vis->points.head = NULL;
		vis->points.iter = NULL;
		vis->points.count = 0;
		NWVisList_RemoveIter(&self->sup.sets[key]);
		set->iter = set->head;
		self->sup.buffer[index].vis = set->head ? &set->head->vis : NULL;
		self->sup.buffer[index].vis->hash = h;
		self->sup.buffer[index].vis->node = set->head;
		return;
	}
	/* shared: a copy with the point */
	NWVisList_Add(set, &tmp);
	set->iter = set->head;
	cNWVis *d = &set->head->vis;
	d->refs = 1;
	d->hash = h;
	d->node = set->head;
	vis = self->sup.buffer[index].vis;
	cNWIntNode *it = vis->points.iter = vis->points.head;
	byte done = 0;
	while (it) {
		if ((uintptr_t)point < (uintptr_t)it->data && !done) {
			NWIntList_Append(&d->points, (void *const *)&point);
			done = 1;
		}
		NWIntList_Append(&d->points, &it->data);
		vis = self->sup.buffer[index].vis;
		it = vis->points.iter = vis->points.iter->next;
		if (it == vis->points.head) {
			break;
		}
	}
	if (!done) {
		NWIntList_Append(&d->points, (void *const *)&point);
	}
	NodesWorld_ReleaseVis(self, index);
	self->sup.buffer[index].vis = d;
	NWIntList_Free(&tmp.points);
}

int CNodesWorldSup_Hash(CNodesWorld *self, int a, int b) {
	return (int)((uint32_t)a + (uint32_t)b) % self->sup.numSets;
}

void CNodesWorldSup_ReleaseVis(CNodesWorld *self, int index) {
	cNWVis *vis = self->sup.buffer[index].vis;

	if (!vis || --vis->refs != 0) {
		return;
	}
	NWIntList_Clear(&self->sup.buffer[index].vis->points);
	vis = self->sup.buffer[index].vis;
	self->sup.sets[vis->hash].iter = vis->node;
	NWVisList_RemoveIter(&self->sup.sets[self->sup.buffer[index].vis->hash]);
}

/* v in the list; the iterator kept */
static byte NWSup_ListHas(cNWIntList *l, const void *v) {
	cNWIntNode *saved = l->iter;
	cNWIntNode *it = l->iter = l->head;
	byte r = 0;

	while (it) {
		if (it->data == v) {
			r = 1;
			break;
		}
		it = l->iter = l->iter->next;
		if (it == l->head) {
			break;
		}
	}
	l->iter = saved;
	return r;
}

void NWIntList_AddKeepIter(cNWIntList *l, const void *v) {
	cNWIntNode *saved = l->iter;

	NWIntList_Add(l, (void *const *)&v);
	l->iter = saved;
}

void CNodesWorldSup_LinkPoints(CNodesWorld *self) {
	Con_Printf("Linking Points of Visibility...\n");
	cNWPointNode *saved = self->sup.points.iter;
	self->sup.numPointLinks = 0;
	cNWPointNode *pn = self->sup.points.iter = self->sup.points.head;
	while (pn) {
		const vec3_t *pos = &pn->pos;
		int i = NodesWorld_BufferIndex(self, pos);
		cNWVis *vis = self->sup.buffer[i].vis;
		if (!vis) {
			Con_Bluef("ERROR AI: Existe un Nodo que No es Visible desde ning\xfa" "n punto de visibilidad.\n");
			CHECK(!"CNodesWorldSup_LinkPointsVis_Node_Not_Visible");
		} else {
			cNWIntNode *vsaved = vis->points.iter;
			cNWIntNode *it = vis->points.iter = vis->points.head;
			while (it) {
				if (pos != it->data) {
					NWIntList_AddKeepIter(&pn->links, it->data);
					self->sup.numPointLinks++;
				}
				vis = self->sup.buffer[i].vis;
				it = vis->points.iter = vis->points.iter->next;
				if (it == vis->points.head) {
					break;
				}
			}
			self->sup.buffer[i].vis->points.iter = vsaved;
		}
		pn = self->sup.points.iter = self->sup.points.iter->next;
		if (pn == self->sup.points.head) {
			break;
		}
	}
	self->sup.points.iter = saved;
	NodesWorld_LinkBack(self);
	self->sup.numPointLinks /= 2;
}

void CNodesWorldSup_LinkBack(CNodesWorld *self) {
	cNWPointNode *saved = self->sup.points.iter;
	cNWPointNode *pn = self->sup.points.iter = self->sup.points.head;

	while (pn) {
		const vec3_t *p = &pn->pos;
		cNWIntList *l = &pn->links;
		cNWIntNode *lsaved = l->iter;
		cNWIntNode *it = l->iter = l->head;
		while (it) {
			const vec3_t *q = it->data;
			if (!NodesWorld_PointLinked(self, q, p)) {
				NWIntList_AddKeepIter(&NW_POINT(q)->links, p);
				self->sup.numPointLinks++;
			}
			it = l->iter = l->iter->next;
			if (it == l->head) {
				break;
			}
		}
		l->iter = lsaved;
		pn = self->sup.points.iter = self->sup.points.iter->next;
		if (pn == self->sup.points.head) {
			break;
		}
	}
	self->sup.points.iter = saved;
}

byte CNodesWorldSup_PointLinked(CNodesWorld *self, const vec3_t *point, const vec3_t *other) {
	(void)self;
	return NWSup_ListHas(&NW_POINT(point)->links, other);
}

void CNodesWorldSup_LinkPoint(CNodesWorld *self, const vec3_t *point) {
	int i = NodesWorld_BufferIndex(self, point);
	cNWVis *vis = self->sup.buffer[i].vis;

	if (!vis) {
		Con_Bluef("ERROR AI: Existe un Nodo que No es Visible desde ning\xfa" "n punto de visibilidad.\n");
		CHECK(!"CNodesWorldSup_LinkPointsVis_Node_Not_Visible");
		return;
	}
	cNWIntNode *saved = vis->points.iter;
	cNWIntNode *it = vis->points.iter = vis->points.head;
	while (it) {
		const vec3_t *q = it->data;
		if (point != q) {
			NWIntList_AddKeepIter(&NW_POINT(point)->links, q);
			NWIntList_AddKeepIter(&NW_POINT(q)->links, point);
			self->sup.numPointLinks++;
		}
		vis = self->sup.buffer[i].vis;
		it = vis->points.iter = vis->points.iter->next;
		if (it == vis->points.head) {
			break;
		}
	}
	self->sup.buffer[i].vis->points.iter = saved;
}

static int NWSup_InSet(CNodesWorld *self, const vec3_t *q, int index) {
	return NodesWorld_VisHas(self, q, index);
}

byte CNodesWorldSup_Unreachable(CNodesWorld *self, const vec3_t *from, const vec3_t *to, int index) {
	return NWSup_UnreachableBy(self, from, to, index, NWSup_InSet);
}

byte NWSup_UnreachableBy(CNodesWorld *self, const vec3_t *from, const vec3_t *to, int index, nwLinkOk_t ok) {
	cNWIntList queue, seen;
	cNWIntNode *at = NULL;
	byte res = 1;

	memset(&queue, 0, sizeof(queue));
	memset(&seen, 0, sizeof(seen));
	NWIntList_Add(&seen, (void *const *)&from);
	/* the linked points of from into the queue (not to) */
	cNWIntList *l = &NW_POINT(from)->links;
	cNWIntNode *saved = l->iter;
	cNWIntNode *it = l->iter = l->head;
	while (it) {
		const vec3_t *q = it->data;
		if (q != to && ok(self, q, index)) {
			NWIntList_Append(&queue, &it->data);
			NodesWorld_FindSorted(self, &seen, q, &at);
			if (at) {
				seen.iter = at;
				NWIntList_AddBefore(&seen, &it->data);
			} else {
				NWIntList_Append(&seen, &it->data);
			}
		}
		it = l->iter = l->iter->next;
		if (it == l->head) {
			break;
		}
	}
	l->iter = saved;
	/* breadth first over the points not seen yet */
	while (queue.head) {
		queue.iter = queue.head;
		const vec3_t *cur = queue.head->data;
		NWIntList_RemoveHead(&queue);
		if (cur == to) {
			res = 0;
			break;
		}
		l = &NW_POINT(cur)->links;
		saved = l->iter;
		it = l->iter = l->head;
		while (it) {
			const vec3_t *q = it->data;
			if (ok(self, q, index) && !NodesWorld_FindSorted(self, &seen, q, &at)) {
				NWIntList_Append(&queue, &it->data);
				seen.iter = at;
				NWIntList_AddBefore(&seen, &it->data);
			}
			it = l->iter = l->iter->next;
			if (it == l->head) {
				break;
			}
		}
		l->iter = saved;
	}
	NWIntList_Free(&seen);
	NWIntList_Free(&queue);
	return res;
}

int CNodesWorldSup_BoxCells(CNodesWorld *self, const cNWBox *box) {
	int nz = (int)(((self->sup.margin + box->b.z) - box->a.z) / self->sup.nodeSize.z);
	int ny = (int)(((self->sup.margin + box->b.y) - box->a.y) / self->sup.nodeSize.y);
	int nx = (int)(((self->sup.margin + box->b.x) - box->a.x) / self->sup.nodeSize.x);

	/* wraps like the imul of the original */
	return (int)((uint32_t)nz * (uint32_t)ny * (uint32_t)nx);
}

byte CNodesWorldSup_FindSorted(CNodesWorld *self, cNWIntList *list, const void *key, cNWIntNode **at) {
	(void)self;
	if (!list->count) {
		*at = NULL;
		return 0;
	}
	cNWIntNode *saved = list->iter;
	cNWIntNode *head = list->head;
	cNWIntNode *it = list->iter = head;
	while (it) {
		uintptr_t d = (uintptr_t)it->data;
		if ((uintptr_t)key < d) {
			break;
		}
		if ((uintptr_t)key == d) {
			list->iter = saved;
			return 1;
		}
		it = list->iter = list->iter->next;
		if (it == head) {
			list->iter = head->prev;
			break;
		}
	}
	*at = list->iter;
	list->iter = saved;
	return 0;
}

/* the pair list: the vector list functions with the node of a pair */
static void NWPairList_Add(cNWPairList *list, const cNWPair *pair) {
	cNWPairNode *node = DList_NewNode(list, sizeof(*node));
	node->p = *pair;
	DList_LinkFirst(list, node);
}

static void NWPairList_AddBefore(cNWPairList *list, const cNWPair *pair) {
	cNWPairNode *head = list->head;

	if (list->iter == head) {
		NWPairList_Add(list, pair);
		return;
	}
	CHECK(head != NULL);
	CHECK(list->iter != NULL);
	list->head = list->iter;
	NWPairList_Add(list, pair);
	list->head = head;
}

static void NWPairList_Append(cNWPairList *list, const cNWPair *pair) {
	cNWPairNode *node = DList_NewNode(list, sizeof(*node));

	node->p = *pair;
	DList_LinkLast(list, node);
}

/* word 1 or 2 of the original's three words of a pair (b, the bits of dist): HasPair compares those */
static uintptr_t NWPair_Word(const cNWPair *p, int i) {
	union { float f; uint32_t u; } d = { p->dist };

	return i == 1 ? (uintptr_t)p->b : d.u;
}

byte CNodesWorldSup_HasPair(CNodesWorld *self, void *vlist, const void *vpair) {
	cNWPairList *list = vlist;
	const cNWPair *pair = vpair;
	(void)self;
	cNWPairNode *saved = list->iter;
	cNWPairNode *head = list->head;
	cNWPairNode *it = list->iter = head;

	while (it) {
		uintptr_t k1 = NWPair_Word(pair, 1), k2 = NWPair_Word(pair, 2);
		uintptr_t w1 = NWPair_Word(&it->p, 1), w2 = NWPair_Word(&it->p, 2);
		if ((k1 == w1 || k1 == w2) && (k2 == w1 || k2 == w2)) {
			list->iter = saved;
			return 1;
		}
		it = list->iter = list->iter->next;
		if (it == head) {
			break;
		}
	}
	list->iter = saved;
	return 0;
}

void CNodesWorldSup_InsertPair(CNodesWorld *self, void *vlist, const void *vpair) {
	cNWPairList *list = vlist;
	const cNWPair *pair = vpair;
	(void)self;
	cNWPairNode *saved = list->iter;
	cNWPairNode *head = list->head;
	cNWPairNode *it = list->iter = head;

	while (it) {
		if (it->p.dist > pair->dist) {
			NWPairList_AddBefore(list, pair);
			list->iter = saved;
			return;
		}
		if (pair->dist == it->p.dist && (pair->a == it->p.a || pair->a == it->p.b) &&
			(pair->b == it->p.a || pair->b == it->p.b)) {
			list->iter = saved;
			return;
		}
		it = list->iter = list->iter->next;
		if (it == head) {
			break;
		}
	}
	NWPairList_Append(list, pair);
	list->iter = saved;
}

byte CNodesWorldSup_VisHas(CNodesWorld *self, const vec3_t *point, int index) {
	cNWVis *vis = self->sup.buffer[index].vis;

	if (!vis) {
		return 0;
	}
	return NWSup_ListHas(&vis->points, point);
}

byte CNodesWorldSup_Linked(CNodesWorld *self, const vec3_t *a, const vec3_t *b) {
	(void)self;
	if (NWSup_ListHas(&NW_POINT(a)->links, b)) {
		return 1;
	}
	return NWSup_ListHas(&NW_POINT(b)->links, a);
}

void CNodesWorldSup_FreeGraph(CNodesWorld *self) {
	Con_Printf("Restoring Memory...\n");
	if (self->sup.buffer) {
		Mem_Free(self->sup.buffer);
		self->sup.buffer = NULL;
	}
	if (self->sup.nodes) {
		for (int i = 0; i < self->sup.numNodes; i++) {
			Mem_Free(self->sup.nodes[i].links);
		}
		Mem_Free(self->sup.nodes);
		self->sup.nodes = NULL;
	}
	if (self->sup.reachedCells) {
		Mem_Free(self->sup.reachedCells);
		self->sup.reachedCells = NULL;
		self->sup.numReachedCells = 0;
	}
	if (self->sup.sets) {
		for (int i = 0; i < self->sup.numSets; i++) {
			NWVisList_Clear(&self->sup.sets[i]);
		}
		Mem_Free(self->sup.sets);
		self->sup.sets = NULL;
	}
	if (self->sup.grid) {
		GridElems_Free(self->sup.grid);
		self->sup.grid = NULL;
	}
	NodesWorld_ClearLists(self);
}

cMemStats g_MemStats;

void CNodesWorldSup_FreeTemp(CNodesWorld *self) {
	Con_Printf("Restoring Memory Temp...\n");
	int mem = g_MemStats.bytes;
	if (self->sup.buffer) {
		Mem_Free(self->sup.buffer);
		self->sup.buffer = NULL;
	}
	Con_Printf("Restoring Memory Buffer Nodes: %d bytes\n", mem - g_MemStats.bytes);
	Con_Printf("Restoring Memory Set Points Visibility: %d bytes\n", 0);
	if (self->sup.reachedCells) {
		Mem_Free(self->sup.reachedCells);
		self->sup.reachedCells = NULL;
		self->sup.numReachedCells = 0;
	}
	if (self->sup.sets) {
		for (int i = 0; i < self->sup.numSets; i++) {
			NWVisList_Clear(&self->sup.sets[i]);
		}
		Mem_Free(self->sup.sets);
		self->sup.sets = NULL;
	}
	Con_Printf("Memory Restored: %d bytes\n", mem - g_MemStats.bytes);
}

byte CNodesWorldSup_InList(CNodesWorld *self, cNWNode *const *list, int n, const cNWNode *node) {
	(void)self;
	for (int i = 0; i < n; i++) {
		if (list[i] == node) {
			return 1;
		}
	}
	return 0;
}

/* ---- the cells that link two points of the generation (the searches of CNodesWorld and CNodesWorld2D) ---- */

float NWSup_Dist(const vec3_t *c, const vec3_t *p) {
	float dx = c->x - p->x, dy = c->y - p->y, dz = c->z - p->z;

	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

float NWSup_PairDist(const vec3_t *c, const vec3_t *const *pair, byte flat) {
	if (flat) {
		float az = c->z - pair[0]->z, ax = c->x - pair[0]->x;
		float bx = c->x - pair[1]->x, bz = c->z - pair[1]->z;
		float da = (float)__builtin_sqrt((double)(az * az + ax * ax));
		float db = (float)__builtin_sqrt((double)(bz * bz + bx * bx));
		return da + db;
	}
	float da = NWSup_Dist(c, pair[0]);
	float db = NWSup_Dist(c, pair[1]);
	return da + db;
}

byte NWSup_Near(const CNodesWorld *self, const vec3_t *c, const vec3_t *const *pair) {
	return self->sup.pairRange > NWSup_Dist(c, pair[0]) && self->sup.pairRange > NWSup_Dist(c, pair[1]);
}

/* both points in the set of the cell, not reachable from each other over the links of the set */
static byte NWSup_LinkVis(CNodesWorld *self, const vec3_t *const *pair, int idx) {
	cNWIntNode *at;

	if (!NodesWorld_FindSorted(self, &self->sup.buffer[idx].vis->points, pair[0], &at)) {
		return 0;
	}
	if (!NodesWorld_FindSorted(self, &self->sup.buffer[idx].vis->points, pair[1], &at)) {
		return 0;
	}
	return NodesWorld_Unreachable(self, pair[0], pair[1], idx);
}

void NWSup_LinkFound(CNodesWorld *self, const int *best, int drawMode) {
	if (drawMode) {
		NWVecList_Add(&self->sup.foundNodes[0], (const vec3_t *)best);
	}
}

byte NWSup_FindLinkBox(CNodesWorld *self, const vec3_t *const *pair, const cNWBox *box, int *out, byte is2D) {
	int best[3];
	float bestDist = 3.4028234663852886e+38f;
	byte found = 0;
	float x0 = box->a.x + self->sup.halfNode.x;
	float y0 = self->sup.halfNode.y + box->a.y;
	float z0 = self->sup.halfNode.z + box->a.z;
	float x1 = (box->b.x - self->sup.halfNode.x) + self->sup.margin;
	float y1 = (box->b.y - self->sup.halfNode.y) + self->sup.margin;
	float z1 = (box->b.z - self->sup.halfNode.z) + self->sup.margin;

	for (float x = x0; x1 >= x; x = x + self->sup.axis[0].x) {
		for (float y = y0; y1 >= y; y = self->sup.axis[1].y + y) {
			for (float z = z0; z1 >= z; z = self->sup.axis[2].z + z) {
				vec3_t p = { x, y, z }, c;
				int idx = NodesWorld_BufferIndex(self, &p);
				int st = self->sup.buffer[idx].state;
				if (st == 3 || st == 6 || st == 5 || (is2D && st == 0)) {
					continue;
				}
				NodesWorld_BufferPos(self, &c, idx);
				if (!NWSup_Near(self, &c, pair) || !NWSup_LinkVis(self, pair, idx)) {
					continue;
				}
				float d = NWSup_PairDist(&c, pair, is2D);
				if (bestDist > d) {
					bestDist = d;
					found = 1;
					best[0] = idx;
					*out = idx;
					best[1] = (int)(uintptr_t)pair[0];
					best[2] = (int)(uintptr_t)pair[1];
				}
			}
		}
	}
	if (found) {
		NWSup_LinkFound(self, best, is2D ? g_NW2DDrawMode : g_NWDrawMode);
	}
	return found;
}

byte NWSup_FindLinkCell(CNodesWorld *self, const vec3_t *const *pair, int *out, byte is2D) {
	int best[3];
	float bestDist = 3.4028234663852886e+38f;
	byte found = 0;
	const int *cells = self->sup.reachedCells;

	for (int k = 0; k < self->sup.numReachedCells; k++) {
		vec3_t c, c2;
		int idx = cells[k];
		int st = self->sup.buffer[idx].state;
		if (st == 5 || (is2D && st == 0)) {
			continue;
		}
		NodesWorld_BufferPos(self, &c, idx);
		if (!NWSup_Near(self, &c, pair) || !NWSup_LinkVis(self, pair, idx)) {
			continue;
		}
		NodesWorld_BufferPos(self, &c2, idx);
		float d = NWSup_PairDist(&c2, pair, is2D);
		if (bestDist > d) {
			bestDist = d;
			found = 1;
			best[0] = idx;
			*out = idx;
			best[1] = (int)(uintptr_t)pair[0];
			best[2] = (int)(uintptr_t)pair[1];
		}
	}
	if (found) {
		NWSup_LinkFound(self, best, is2D ? g_NW2DDrawMode : g_NWDrawMode);
	}
	return found;
}

/* the neighbour n of the cell center c as the next start when it is better than best */
static void NWSup_TryInit(CNodesWorld *self, const vec3_t *n, const vec3_t *c, nwInitFree_t isFree,
	nwInitDist_t dist, float *best, vec3_t *out, byte *found) {
	int idx = NodesWorld_BufferIndex(self, n);

	if (self->sup.buffer[idx].state != 4 || !isFree(self, n, c)) {
		return;
	}
	float d = dist(self, n);
	if (*best > d) {
		*found = 1;
		*out = *n;
		*best = d;
	}
}

byte NWSup_NextInit(CNodesWorld *self, vec3_t *out, nwInitFree_t isFree, nwInitDist_t dist) {
	byte found = 0;
	float best = 3.4028234663852886e+38f;

	Con_Printf("Selecting Next Node Init...\n");
	cNWIntNode *it = self->sup.edgeCells.iter = self->sup.edgeCells.head;
	while (it) {
		int idx = (int)(intptr_t)it->data;
		if (self->sup.buffer[idx].state == 5) {
			vec3_t c, n;
			NodesWorld_BufferPos(self, &c, idx);
			for (int k = 0; k < 3; k++) {
				const vec3_t *ax = &self->sup.axis[k];
				n.x = ax->x + c.x;
				n.y = ax->y + c.y;
				n.z = ax->z + c.z;
				if ((&self->sup.max.x)[k] >= (&n.x)[k] + (&self->sup.halfNode.x)[k]) {
					NWSup_TryInit(self, &n, &c, isFree, dist, &best, out, &found);
				}
				n.x = c.x - ax->x;
				n.y = c.y - ax->y;
				n.z = c.z - ax->z;
				if ((&n.x)[k] - (&self->sup.halfNode.x)[k] >= (&self->sup.min.x)[k]) {
					NWSup_TryInit(self, &n, &c, isFree, dist, &best, out, &found);
				}
			}
		}
		it = self->sup.edgeCells.iter = self->sup.edgeCells.iter->next;
		if (it == self->sup.edgeCells.head) {
			break;
		}
	}
	return found;
}

/* ---- the buffer of the generation: the cells reached from the start ---- */

/* the neighbour n of the cell c, in the box: a cell still unknown (6) is tested by step */
static void NWSup_FloodCell(CNodesWorld *self, cNWVecList *queue, const vec3_t *c, const vec3_t *n,
	nwFloodStep_t step) {
	int idx = NodesWorld_BufferIndex(self, n);

	if (self->sup.buffer[idx].state == 6) {
		step(self, queue, c, n);
	}
}

byte NWSup_IniBuffer(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max, nwFloodStep_t step,
	byte is3D) {
	cNWVecList queue;
	byte ok;

	self->sup.nx = (int)((max->x - min->x) / self->sup.nodeSize.x);
	self->sup.ny = (int)((max->y - min->y) / self->sup.nodeSize.y);
	self->sup.nz = (int)((max->z - min->z) / self->sup.nodeSize.z);
	self->sup.dimBuffer = self->sup.nz * self->sup.ny * self->sup.nx;
	if (is3D) {
		Con_Printf("Region Nodes: PMin: %f %f %f PMax: %f %f %f\n", (double)min->x, (double)min->y, (double)min->z,
			(double)max->x, (double)max->y, (double)max->z);
	}
	Con_Printf("Number of Nodes Map: %d...\n", self->sup.dimBuffer);
	Con_Printf("Allocating Buffer Nodes Size: %d bytes...\n", self->sup.dimBuffer << 3);
	self->sup.buffer = Mem_Alloc(self->sup.dimBuffer << 3);
	if (!self->sup.buffer) {
		Con_Bluef("ERROR: IniBuffNodesPointVis - Insuficiente memoria Buffer Nodes\n");
		CHECK(!"cNodesWorld_IniBuffNodesPointVis_new_BufferNodes_Fail");
		return 0;
	}
	Con_Printf("Allocated Buffer Nodes.\n");
	Con_Printf("Initializing Buffer Nodes...\n");
	memset(self->sup.buffer, 0, self->sup.dimBuffer << 3);
	for (int i = 0; i < self->sup.dimBuffer; i++) {
		self->sup.buffer[i].state = 6;
		self->sup.buffer[i].vis = NULL;
	}
	int idx = NodesWorld_BufferIndex(self, init);
	memset(&queue, 0, sizeof(queue));
	self->sup.buffer[idx].state = 0;
	DListVec3_Add(&queue, init);
	DListVec3_Clear(&self->sup.cellLists[0]);
	self->sup.numReachedCells = 0;
	self->sup.openPos.x = 1.17549435e-38f;
	self->sup.openPos.y = 1.17549435e-38f;
	self->sup.openPos.z = 1.17549435e-38f;
	if (is3D) {
		self->open = 0;
	}
	while (queue.head) {
		vec3_t c, n;

		queue.iter = queue.head;
		const vec3_t *p = &queue.head->v;
		if (is3D && self->analyzing) {
			vec3_t hit;
			CCollisionTri tri;
			if (Sector_At(p) < 0 && self->sup.errUnlinked.count < 100) {
				DListVec3_Add(&self->sup.errUnlinked, p);
			}
			if (CNodesWorld_TestSurfaceAround(self, p, &hit, &tri)) {
				if (!self->firstSaved) {
					memcpy(self->firstTri, &tri, sizeof(tri));
					self->firstSaved = 1;
				}
				CNodesWorld_LogTri(self, p, &hit, &tri);
			}
		}
		c = *p;
		NWVecList_RemoveHead(&queue);
		self->sup.numReachedCells++;
		for (int k = 0; k < 3; k++) {
			const vec3_t *ax = &self->sup.axis[k];
			for (int s = 0; s < 2; s++) {
				int inside;
				if (!s) {
					n.x = ax->x + c.x;
					n.y = ax->y + c.y;
					n.z = ax->z + c.z;
					inside = (&max->x)[k] >= (&n.x)[k] + (&self->sup.halfNode.x)[k];
				} else {
					n.x = c.x - ax->x;
					n.y = c.y - ax->y;
					n.z = c.z - ax->z;
					inside = (&n.x)[k] - (&self->sup.halfNode.x)[k] >= (&min->x)[k];
				}
				if (inside) {
					NWSup_FloodCell(self, &queue, &c, &n, step);
				} else if (is3D && self->sup.nodeInitDummy && (k != 1 || s || !g_IsMapOutDoor)) {
					/* the region reaches the side of the box: the level is open there */
					self->sup.openPos = n;
					self->open = 1;
					Con_Bluef("ERROR AI: ESCENARIO ABIERTO!.\n");
					ok = 0;
					goto end;
				}
			}
		}
	}
	self->sup.reachedCells = Mem_Alloc(self->sup.numReachedCells << 2);
	if (!self->sup.reachedCells) {
		Con_Bluef("ERROR: GenPointsVisInterRegNoLinkPVisDistMin - Insuficiente memoria Buffer Ind Nodes\n");
		CHECK(!"cNodesWorld_GenPointsVisInterRegNoLinkPVisDistMin_new_BufferIndNodes_Fail");
		ok = 0;
		goto end;
	}
	int *cells = self->sup.reachedCells;
	for (int i = 0; i < self->sup.dimBuffer; i++) {
		int st = self->sup.buffer[i].state;
		if (st != 3 && st != 6) {
			*cells++ = i;
		}
	}
	self->sup.numSets = self->sup.numReachedCells / 2 + 1;
	self->sup.sets = Mem_Alloc(self->sup.numSets * sizeof(cNWVisList));
	if (!self->sup.sets) {
		Con_Bluef("ERROR: GenPointsVisInterRegNoLinkPVisDistMin - Insuficiente memoria Hash Lists Point Vis.\n");
		CHECK(!"cNodesWorld_GenPointsVisInterRegNoLinkPVisDistMin_new_BufferHash_Fail");
		ok = 0;
		goto end;
	}
	memset(self->sup.sets, 0, self->sup.numSets * sizeof(cNWVisList));
	Con_Printf("Buffer Nodes Initializated\n");
	Con_Printf("Num. Nodes Trans.: %ld\n", (long)self->sup.numReachedCells);
	Con_Printf("Num. Nodes No Trans.: %ld\n", (long)(self->sup.dimBuffer - self->sup.numReachedCells));
	if (is3D) {
		Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
	}
	ok = 1;
end:
	DListVec3_Clear(&queue);
	DList_FreePool(&queue);
	return ok;
}

/* ---- the region of a new point of visibility ---- */

/* the neighbour n of a cell of the region: a free cell (0) or one of another region (4, 5) joins the region when
 * reach lets it, else it is a border cell (2), the free ones also start cells of later points (edgeCells) */
static void NWSup_RegionCell(CNodesWorld *self, const vec3_t *start, const vec3_t *n, cNWVecList *queue,
	cNWIntList *cells, nwRegionReach_t reach) {
	int idx = NodesWorld_BufferIndex(self, n);
	int st = self->sup.buffer[idx].state;

	if (st != 5 && st != 0 && st != 4) {
		return;
	}
	if (reach(self, start, n)) {
		self->sup.buffer[idx].state = 1;
		DListVec3_Add(queue, n);
		return;
	}
	st = self->sup.buffer[idx].state;
	if (st == 4) {
		return;
	}
	if (st != 5) {
		NWIntList_Add(&self->sup.edgeCells, (void *const *)&idx);
	}
	self->sup.buffer[idx].state = 2;
	NWIntList_Add(cells, (void *const *)&idx);
}

void NWSup_CreateRegion(CNodesWorld *self, const vec3_t *start, nwRegionReach_t reach, byte is2D) {
	cNWVecList queue;
	cNWIntList cells;

	memset(&queue, 0, sizeof(queue));
	memset(&cells, 0, sizeof(cells));
	DListVec3_Add(&queue, start);
	if (is2D && g_NW2DDrawMode) {
		cNW2DRegion r;
		memset(&r, 0, sizeof(r));
		r.id = self->sup.points.count - 1;
		NW2DRegionList_Add(&((CNodesWorld2D *)self)->regions, &r);
		DListVec3_Clear(&r.points);
		DList_FreePool(&r.points);
	}
	cNWPointNode *pn = self->sup.points.iter = self->sup.points.head;
	cNWPointNode *P = pn;
	while (queue.head) {
		vec3_t c, n;

		queue.iter = queue.head;
		c = queue.head->v;
		NWVecList_RemoveHead(&queue);
		if (is2D && g_NW2DDrawMode) {
			cNW2DRegionList *regions = &((CNodesWorld2D *)self)->regions;
			cNW2DRegionNode *rn = regions->iter = regions->head;
			NWVecList_Add(&rn->r.points, &c);
		}
		int idx = NodesWorld_BufferIndex(self, &c);
		NWIntList_Add(&cells, (void *const *)&idx);
		self->sup.buffer[idx].state = 1;
		NodesWorld_AddVis(self, &P->pos, idx);
		P->boxA.x = Float_MinSS(P->boxA.x, c.x - self->sup.halfNode.x);
		P->boxA.y = Float_MinSS(P->boxA.y, c.y - self->sup.halfNode.y);
		P->boxA.z = Float_MinSS(P->boxA.z, c.z - self->sup.halfNode.z);
		P->boxB.x = Float_MaxSS(P->boxB.x, c.x + self->sup.halfNode.x);
		P->boxB.y = Float_MaxSS(P->boxB.y, self->sup.halfNode.y + c.y);
		P->boxB.z = Float_MaxSS(P->boxB.z, self->sup.halfNode.z + c.z);
		for (int k = 0; k < 3; k++) {
			const vec3_t *ax = &self->sup.axis[k];
			n.x = ax->x + c.x;
			n.y = ax->y + c.y;
			n.z = ax->z + c.z;
			if ((&self->sup.max.x)[k] >= (&n.x)[k] + (&self->sup.halfNode.x)[k]) {
				NWSup_RegionCell(self, start, &n, &queue, &cells, reach);
			}
			n.x = c.x - ax->x;
			n.y = c.y - ax->y;
			n.z = c.z - ax->z;
			if ((&n.x)[k] - (&self->sup.halfNode.x)[k] >= (&self->sup.min.x)[k]) {
				NWSup_RegionCell(self, start, &n, &queue, &cells, reach);
			}
		}
	}
	/* the region done: its cells 4, its border 5 */
	cNWIntNode *it = cells.iter = cells.head;
	while (it) {
		int i = (int)(intptr_t)it->data;
		if (self->sup.buffer[i].state == 1) {
			self->sup.buffer[i].state = 4;
		} else if (self->sup.buffer[i].state == 2) {
			self->sup.buffer[i].state = 5;
		}
		it = it->next;
		if (it == cells.head) {
			break;
		}
	}
	cells.iter = it;
	NWIntList_Clear(&cells);
	DList_FreePool(&cells);
	DListVec3_Clear(&queue);
	DList_FreePool(&queue);
}

/* ---- the points of visibility between the regions ---- */

/* "hh:mm:ss" of the seconds from t0 to t1 into the console with fmt */
static void NWSup_PrintTime(const char *fmt, int32_t t1, int32_t t0) {
	int s = (int)crt_difftime32(t1, t0);
	int h = s / 3600, r = s % 3600;

	Con_Printf(fmt, (long)h, (long)(r / 60), (long)(r % 60));
}

void NWSup_InterRegion(CNodesWorld *self, const nwGenOps *ops) {
	cNWPairList pairs;
	int32_t t0, t1;
	const vec3_t *const *pp = NULL;
	byte first = 1;
	int made = 0;

	Con_Printf("\nGenerating Points Visibility Inter-Region No Linked Mem. Din...\n");
	if (!ops->is2D) {
		Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
	}
	NWVecList_Clear(&self->sup.foundNodes[0]);
	crt_time32(&t0);
	memset(&pairs, 0, sizeof(pairs));
	ops->getPairs(self, &pairs);
	int total = pairs.count;
	Con_Printf("Searching Points Visibility No Linked...\n");
	while (pairs.head) {
		cNWBox box;
		int idx;

		if (first) {
			pairs.iter = pairs.head;
			pp = pairs.head ? &pairs.head->p.a : NULL;
		}
		const cNWPointNode *a = NW_POINT(pp[0]), *b = NW_POINT(pp[1]);
		box.a.x = Float_MaxSS(a->boxA.x, b->boxA.x);
		box.a.y = Float_MaxSS(a->boxA.y, b->boxA.y);
		box.a.z = Float_MaxSS(a->boxA.z, b->boxA.z);
		box.b.x = Float_MinSS(a->boxB.x, b->boxB.x);
		box.b.y = Float_MinSS(a->boxB.y, b->boxB.y);
		box.b.z = Float_MinSS(a->boxB.z, b->boxB.z);
		byte found;
		if (NodesWorld_BoxCells(self, &box) < self->sup.numReachedCells) {
			found = ops->findBox(self, pp, &box, &idx);
		} else {
			found = ops->findCell(self, pp, &idx);
		}
		if (!found) {
			first = 1;
			DList_RemoveHead(&pairs);
			Con_Printf("Num. Total Pairs PVis Remain: %d of %d\n", pairs.count, total);
			if (!ops->is2D) {
				Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
			}
			continue;
		}
		/* a new point of visibility in the cell that links the pair */
		cNWPointNode tmp;
		vec3_t c;
		memset(&tmp, 0, sizeof(tmp));
		tmp.pos = *NodesWorld_BufferPos(self, &c, idx);
		NWPointList_Add(&self->sup.points, &tmp.pos);
		made++;
		Con_Printf("Creating Point of Visibility Inter-Region No Link Num: %d...\n", made);
		Con_Printf("Point of Visibility: %.2f %.2f %.2f\n", (double)tmp.pos.x, (double)tmp.pos.y, (double)tmp.pos.z);
		Con_Bluef("%s", str_Empty);
		Con_Printf("Creating Region...\n");
		if (!ops->is2D) {
			Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
		}
		cNWPointNode *pn = self->sup.points.iter = self->sup.points.head;
		const vec3_t *P = pn ? &pn->pos : NULL;
		ops->createRegion(self, &tmp.pos);
		if (!ops->is2D) {
			NWIntList_Clear(&self->sup.edgeCells);
			Con_Printf("Point of Visibility created...\n");
			Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
			NodesWorld_LinkPoint(self, P);
		} else {
			NodesWorld_LinkPoint(self, P);
			Con_Printf("Point of Visibility created...\n");
		}
		first = 0;
		crt_time32(&t1);
		NWSup_PrintTime("Time Creating Point Inter Reg. No Link: %02ld:%02ld:%02ld\n", t1, t0);
		Con_Printf("\nTime Act: %s\n", crt_ctime32(&t1));
		if (!ops->is2D) {
			Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
		}
		Con_Printf("Searching New Point Visibility No Linked...\n");
		crt_time32(&t0);
		NWIntList_Free(&tmp.links);
	}
	Con_Printf("Num. Points Visibility Inter-Region No Linked: %d\n", made);
	DList_Clear(&pairs);
	DList_FreePool(&pairs);
}

/* ---- the generation of the graph ---- */

byte NWSup_Generate(CNodesWorld *self, const vec3_t *init, const vec3_t *min, const vec3_t *max, const nwGenOps *ops) {
	vec3_t p = *init, snap;
	int32_t t0, t1, t2;
	byte more;

	p = *NodesWorld_SnapPos(self, &snap, &p);
	if (self->sup.halfNode.x + p.x > max->x || min->x > p.x - self->sup.halfNode.x ||
		self->sup.halfNode.y + p.y > max->y || min->y > p.y - self->sup.halfNode.y ||
		self->sup.halfNode.z + p.z > max->z || min->z > p.z - self->sup.halfNode.z || !ops->initOk(self, &p)) {
		Con_Bluef("ERROR: GenerateRegPointVis - Nodo Inicial Incorrecto: %f %f %f\n", (double)p.x, (double)p.y,
			(double)p.z);
		CHECK(!"Incorrect_Initial_Node");
		return 0;
	}
	crt_time32(&t0);
	Con_Printf(ops->is2D ? "\nTime Ini Creating Graph 2D: %s\n" : "\nTime Ini Creating Graph: %s\n", crt_ctime32(&t0));
	self->sup.min = *min;
	self->sup.max = *max;
	if (!ops->iniBuffer(self, &p, min, max)) {
		return 0;
	}
	NWIntList_Clear(&self->sup.edgeCells);
	do {
		/* the new point of visibility: links empty, the box FLT_MAX FLT_MAX FLT_MAX -FLT_MAX -FLT_MAX -FLT_MAX */
		cNWPointNode tmp;
		memset(&tmp, 0, sizeof(tmp));
		tmp.pos = p;
		NWBox_Init((cNWBox *)&tmp.boxA);
		NWPointList_Add(&self->sup.points, &tmp.pos);
		Con_Printf("Created Point of Visibility Num: %d\n", self->sup.points.count);
		Con_Printf("Point of Visibility: %.2f %.2f %.2f\n", (double)tmp.pos.x, (double)tmp.pos.y, (double)tmp.pos.z);
		Con_Bluef("%s", str_Empty);
		Con_Printf("Creating Region...\n");
		ops->createRegion(self, &p);
		if (!ops->is2D) {
			Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
		}
		more = ops->nextInit(self, &p);
		Con_Printf(more ? "New Node Init Selected.\n" : "Last Node Init.\n");
		NWIntList_Free(&tmp.links);
	} while (more);
	NWIntList_Clear(&self->sup.edgeCells);
	NodesWorld_LinkPoints(self);
	crt_time32(&t1);
	NWSup_PrintTime("\nTime Creating Graph Points Vis. Min.: %02ld:%02ld:%02ld\n", t1, t0);
	ops->interRegion(self);
	ops->linkExtra(self);
	crt_time32(&t2);
	NWSup_PrintTime("\nTime Creating Graph Linking Points Vis. No Vis.: %02ld:%02ld:%02ld\n", t2, t1);
	crt_time32(&t1);
	Con_Printf("\nTime End Creating Graph: %s", crt_ctime32(&t1));
	NWSup_PrintTime(ops->is2D ? "Time Creating Graph 2D: %02ld:%02ld:%02ld\n" : "Time Creating Graph: %02ld:%02ld:%02ld\n",
		t1, t0);
	if (!ops->is2D) {
		Con_Printf("Mem. used: %d bytes\n", g_MemStats.bytes);
	}
	if (ops->is2D ? g_NW2DDrawMode : g_NWDrawMode) {
		NodesWorld_GetInfoNodes(self);
	}
	return 1;
}

/* ---- the debug view of the graphs (g_NWDrawMode, g_NW2DDrawMode) ---- */

void NWSup_DrawNodeText(const vec3_t *n, int i) {
	vec3_t p;

	p.x = n->x + 0.0f;
	p.y = n->y - 25.0f;
	p.z = n->z + 0.0f;
	CFont_Printf3D(g_FontTerminal8, &p, 3, "%d", i);
}

void NWSup_DrawFound(CNodesWorld *self, cNWVecList *list, float r, uint32_t color) {
	cNWVecNode *n = list->iter = list->head;

	while (n) {
		const int *d = (const int *)&n->v;
		vec3_t c;
		NodesWorld_BufferPos(self, &c, d[0]);
		DebugUtils_DrawSphere(&c, r, color, 2, 2);
		n = list->iter = list->iter->next;
		if (n == list->head) {
			break;
		}
	}
}

void NWSup_DrawCells(CNodesWorld *self, cNWVecList *list, uint32_t color, const char *fmt) {
	cNWVecNode *n = list->iter = list->head;

	while (n) {
		const vec3_t *v = &n->v;
		vec3_t mn, mx;
		mx.x = v->x + self->sup.halfNode.x;
		mn.x = v->x - self->sup.halfNode.x;
		mx.y = self->sup.halfNode.y + v->y;
		mn.y = v->y - self->sup.halfNode.y;
		mx.z = self->sup.halfNode.z + v->z;
		mn.z = v->z - self->sup.halfNode.z;
		DebugUtils_DrawBox(&mn, &mx, color);
		n = list->iter = list->iter->next;
		if (n == list->head) {
			break;
		}
	}
	if (fmt) {
		Con_Printf(fmt, list->count);
	}
}

/* the next point of a ring of points, NULL at its head */
static const vec3_t *NWSup_NextPoint(cNWVecList *list) {
	cNWVecNode *n = list->iter = list->iter->next;

	return n == list->head ? NULL : &n->v;
}

void NWSup_DrawErrors(CNodesWorld *self, float r) {
	cNWVecList *l = &self->sup.errUnlinked;
	cNWVecNode *n = l->iter = l->head;

	while (n) {
		DebugUtils_DrawSphere(&n->v, r, 0xff00, 10, 10);
		n = l->iter = l->iter->next;
		if (n == l->head) {
			break;
		}
	}
	l = &self->sup.errOneWay;
	n = l->iter = l->head;
	while (n) {
		const vec3_t *a = &n->v;
		DebugUtils_DrawSphere(a, r, 0xff00, 10, 10);
		const vec3_t *b = NWSup_NextPoint(l);
		DebugUtils_DrawSphere(b, r, 0xff, 10, 10);
		DebugUtils_DrawLine(a, b, 0xff0000);
		n = l->iter = l->iter->next;
		if (n == l->head) {
			break;
		}
	}
}

void NWSup_DrawCollide(CNodesWorld *self, float r, byte triples) {
	cNWVecList *l = &self->sup.errCollide;
	cNWVecNode *n = l->iter = l->head;

	while (n) {
		const vec3_t *a = &n->v;
		DebugUtils_DrawSphere(a, triples ? 25.0f : r, 0xff0000, triples ? 2 : 10, triples ? 2 : 10);
		const vec3_t *b = NWSup_NextPoint(l);
		DebugUtils_DrawSphere(b, triples ? 25.0f : r, 0xff0000, triples ? 2 : 10, triples ? 2 : 10);
		DebugUtils_DrawLine(a, b, 0xff0000);
		if (triples) {
			DebugUtils_DrawSphere(NWSup_NextPoint(l), r, 0xff0000, 10, 10);
		}
		n = l->iter = l->iter->next;
		if (n == l->head) {
			break;
		}
	}
}

void NWSup_DrawExtra(CNodesWorld *self, float r, uint32_t color) {
	cNWVecList *l = &self->sup.extraPoints;
	cNWVecNode *n = l->iter = l->head;

	while (n) {
		DebugUtils_DrawSphere(&n->v, r, color, 2, 2);
		n = l->iter = l->iter->next;
		if (n == l->head) {
			break;
		}
	}
}

/* the vtable of CNodesWorldSup */
static const CNodesWorldVtbl CNodesWorldSup_vftable = {
	.Free = CNodesWorldSup_Free,
	.GetBox = CNodesWorldSup_GetBox,
	.LoadFiles = CNodesWorldSup_LoadGraph,
	.LoadGraph = (void *)crt_purecall,
	.Create = CNodesWorldSup_Create,
	.FileOf = CNodesWorldSup_FileOf,
	.CommonFile = CNodesWorldSup_CommonFile,
	.SnapPos = CNodesWorldSup_SnapPos,
	.OtherLinks = CNodesWorldSup_OtherLinks,
	.IsCreated = CNodesWorldSup_IsCreated,
	.LastNodePos = CNodesWorldSup_LastNodePos,
	.GetVisPoint = CNodesWorldSup_GetVisPoint,
	.NumVisPoints = CNodesWorldSup_NumVisPoints,
	.GetMaxVisDist = CNodesWorldSup_GetMaxVisDist,
	.InGrid = CNodesWorldSup_InGrid,
	.SavePath = (void *)crt_purecall,
	.CheckBuffer = CNodesWorldSup_CheckBuffer,
	.BuildGrid = CNodesWorldSup_BuildGrid,
	.BufferIndex = CNodesWorldSup_BufferIndex,
	.BufferPos = CNodesWorldSup_BufferPos,
	.GetInfoNodes = CNodesWorldSup_GetInfoNodes,
	.AddVis = CNodesWorldSup_AddVis,
	.Hash = CNodesWorldSup_Hash,
	.ReleaseVis = CNodesWorldSup_ReleaseVis,
	.LinkPoints = CNodesWorldSup_LinkPoints,
	.LinkBack = CNodesWorldSup_LinkBack,
	.PointLinked = CNodesWorldSup_PointLinked,
	.LinkPoint = CNodesWorldSup_LinkPoint,
	.Unreachable = CNodesWorldSup_Unreachable,
	.BoxCells = CNodesWorldSup_BoxCells,
	.FindSorted = CNodesWorldSup_FindSorted,
	.HasPair = CNodesWorldSup_HasPair,
	.InsertPair = CNodesWorldSup_InsertPair,
	.VisHas = CNodesWorldSup_VisHas,
	.Linked = CNodesWorldSup_Linked,
	.FreeGraph = CNodesWorldSup_FreeGraph,
	.FreeTemp = CNodesWorldSup_FreeTemp,
	.InList = CNodesWorldSup_InList,
	.ClearLists = (void *)crt_purecall,
	.SaveInfPath = CNodesWorldSup_SaveInfPath,
};

/* the vtable of cGridElements_InfPointVis */
static const cGridElementsVtbl cGridElements_InfPointVis_vftable = {
	.Free = NWGrid_Free,
	.Add = NWGrid_Add,
	.GetCellAt = cGridElements_GetCellAt,
	.GetCell = cGridElements_GetCell,
	.Draw = cGridElements_Draw,
	.CellCoords = cGridElements_CellCoords,
	.GetDivs = cGridElements_GetDivs,
	.GetCellSize = cGridElements_GetCellSize,
	.IsInside = cGridElements_IsInside,
	.Clear = NWGrid_Clear,
	.CellIndex = cGridElements_CellIndex,
};
