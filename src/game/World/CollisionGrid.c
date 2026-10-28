/*
 * World/CollisionGrid.cpp of the game: the entity grid of the world, see include/scrap/collisiongrid.h. After it in
 * the exe: the collision masks, the dummy search and the entity lists of the world.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/render.h"
#include "scrap/world.h"

static inline cEntity *Grid_CurEntity(const cCollisionGrid *g) {
	return g->cur ? g->cur->data : NULL;
}

/* the first node of a ring, NULL for an empty one (the head has no data) */
static inline Nto1Node *Ring_First(Nto1Node *head) {
	Nto1Node *n = head->next;

	return n->data ? n : NULL;
}

/* the cell of an inside position */
static inline int Grid_Index(const cCollisionGrid *g, int x, int y, int z) {
	return (g->ny * z + y) * g->nx + x;
}

Nto1Node *__thiscall GridCell_ctor(Nto1Node *self) {
	self->target = NULL;
	self->data = NULL;
	self->next = self;
	self->prev = self;
	return self;
}

void *__thiscall GridCell_VecDelDtor(Nto1Node *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;

		crt_eh_vector_dtor(self, sizeof(Nto1Node), *mem, Game_Ptr(NTO1NODE_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return mem;
	}
	Nto1Node_Done(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(Nto1Node));
	}
	return self;
}

void *__thiscall Grid_ctor(void *grid) {
	cCollisionGrid *g = grid;

	g->min.y = 0.0f;
	g->min.z = 0.0f;
	g->min.x = 0.0f;
	g->cell.x = 0.0f;
	g->outside.target = NULL;
	g->outside.data = NULL;
	g->outside.next = &g->outside;
	g->outside.prev = &g->outside;
	g->cell.y = g->min.y;
	g->cell.z = g->min.z;
	g->nz = 0;
	g->ny = 0;
	g->nx = 0;
	g->cells = NULL;
	g->cur = NULL;
	g->start[2] = 0;
	g->start[1] = 0;
	g->start[0] = 0;
	g->end[2] = 0;
	g->end[1] = 0;
	g->end[0] = 0;
	g->pos[2] = 0;
	g->pos[1] = 0;
	g->pos[0] = 0;
	g->visited = NULL;
	g->stamp = 0;
	g->segStep.y = 0.0f;
	g->segStep.z = 0.0f;
	g->segStep.x = 0.0f;
	g->segPos.x = 0.0f;
	g->segPos.y = g->segStep.y;
	g->segPos.z = g->segStep.z;
	g->ext.x = 0.0f;
	g->ext.y = g->segPos.y;
	g->ext.z = g->segPos.z;
	g->steps = 0;
	return g;
}

void __thiscall Grid_Free(void *grid) {
	cCollisionGrid *g = grid;

	if (!g->cells) {
		return;
	}
	Delete_Objects(g->cells, sizeof(Nto1Node), NTO1NODE_DTOR);
	operator_delete_array(g->visited);
	g->stamp = 0;
	g->min.y = 0.0f;
	g->min.z = 0.0f;
	g->min.x = 0.0f;
	g->cell.x = 0.0f;
	g->cell.y = g->min.y;
	g->cell.z = g->min.z;
	g->nz = 0;
	g->ny = 0;
	g->nx = 0;
	g->cells = NULL;
	g->visited = NULL;
}

void __thiscall Grid_dtor(void *grid) {
	cCollisionGrid *g = grid;

	Grid_Free(g);
	Nto1Node_Done(&g->outside);
}

void __thiscall Grid_Init(void *grid, const vec3_t *min, const vec3_t *max, const vec3_t *cell) {
	cCollisionGrid *g = grid;

	Grid_Free(g);
	g->min = *min;
	g->cell = *cell;
	g->max = *max;
	g->nx = (int)((max->x - min->x) / cell->x) + 1;
	g->ny = (int)((max->y - min->y) / cell->y) + 1;
	g->nz = (int)((max->z - min->z) / cell->z) + 1;
	int count = g->ny * g->nz * g->nx;
	g->cells = New_Objects(count, sizeof(Nto1Node), GRIDCELL_CTOR, NTO1NODE_DTOR);
	for (int i = 0; i < count; i++) {
		g->cells[i].target = &g->cells[i];
	}
	g->visited = operator_new_array(count);
	g->stamp = 0;
	memset(g->visited, 0, count);
	g->outside.target = &g->outside;
}

void __thiscall Grid_Set(void *grid, const vec3_t *size) {
	cCollisionGrid *g = grid;
	vec3_t min = g->min, max = g->max;

	if (g_GridUseTopLimit) {
		max.y = g_TopLimit;
	}
	Grid_Init(g, &min, &max, size);
}

cEntity *__thiscall Grid_GetCell(void *grid, int x, int y, int z) {
	cCollisionGrid *g = grid;
	Nto1Node *n;

	if (x >= 0 && x < g->nx && y >= 0 && y < g->ny && z >= 0 && z < g->nz) {
		int i = Grid_Index(g, x, y, z);
		if (g->steps) {
			if (g->visited[i] >= g->stamp) {
				return NULL;
			}
			g->visited[i] = g->stamp;
		}
		n = g->cells[i].next;
	} else {
		if (g->steps) {
			return NULL;
		}
		n = g->outside.next;
	}
	g->cur = n->data ? n : NULL;
	return Grid_CurEntity(g);
}

cEntity *__thiscall Grid_GetCellAt(void *grid, const vec3_t *pos) {
	cCollisionGrid *g = grid;

	CHECK(g->cells);
	return Grid_GetCell(g, (int)((pos->x - g->min.x) / g->cell.x), (int)((pos->y - g->min.y) / g->cell.y),
		(int)((pos->z - g->min.z) / g->cell.z));
}

void __thiscall Grid_EndFrame(void *grid) {
	((cCollisionGrid *)grid)->cur = NULL;
}

/* the next cell of the box query; 1 after the last */
static inline int Grid_Advance(cCollisionGrid *g) {
	g->pos[0]++;
	if (g->pos[0] <= g->end[0]) {
		return 0;
	}
	g->pos[1]++;
	g->pos[0] = g->start[0];
	if (g->pos[1] <= g->end[1]) {
		return 0;
	}
	g->pos[2]++;
	g->pos[1] = g->start[1];
	return g->pos[2] > g->end[2];
}

byte __thiscall Grid_StepCell(void *grid) {
	return (byte)Grid_Advance(grid);
}

cEntity *__thiscall Grid_FirstInBox(void *grid, const vec3_t *min, const vec3_t *max) {
	cCollisionGrid *g = grid;

	g->pos[0] = g->start[0] = (int)((min->x - g->min.x) / g->cell.x);
	g->pos[1] = g->start[1] = (int)((min->y - g->min.y) / g->cell.y);
	g->pos[2] = g->start[2] = (int)((min->z - g->min.z) / g->cell.z);
	g->end[0] = (int)((max->x - g->min.x) / g->cell.x);
	g->end[1] = (int)((max->y - g->min.y) / g->cell.y);
	g->end[2] = (int)((max->z - g->min.z) / g->cell.z);
	while (!Grid_GetCell(g, g->pos[0], g->pos[1], g->pos[2]) && !Grid_Advance(g)) {
	}
	return Grid_CurEntity(g);
}

cEntity *__thiscall Grid_NextInBox(void *grid) {
	cCollisionGrid *g = grid;

	if (g->cur) {
		g->cur = Ring_First(g->cur);
		if (g->cur && g->cur->target && g->cur->data) {
			return Grid_CurEntity(g);
		}
	}
	do {
		if (Grid_Advance(g)) {
			break;
		}
	} while (!Grid_GetCell(g, g->pos[0], g->pos[1], g->pos[2]));
	return Grid_CurEntity(g);
}

cEntity *__thiscall Grid_NextInCell(void *grid) {
	cCollisionGrid *g = grid;

	if (!g->cur) {
		return NULL;
	}
	g->cur = Ring_First(g->cur);
	if (g->cur && g->cur->target) {
		return g->cur->data;
	}
	return NULL;
}

static inline void Grid_Stamp(cCollisionGrid *g) {
	CHECK(g->visited);
	if (g->stamp == 0xff) {
		g->stamp = 1;
		memset(g->visited, 0, g->nz * g->ny * g->nx);
	} else {
		g->stamp++;
	}
}

void __thiscall Grid_NewQuery(void *grid) {
	Grid_Stamp(grid);
}

cEntity *__thiscall Grid_NextOnSegment(void *grid) {
	cCollisionGrid *g = grid;
	cEntity *e = Grid_NextInBox(g);

	while (!e) {
		g->steps--;
		if (g->steps <= 0) {
			g->steps = 0;
			break;
		}
		g->segPos.x = g->segStep.x + g->segPos.x;
		g->segPos.y = g->segStep.y + g->segPos.y;
		g->segPos.z = g->segStep.z + g->segPos.z;
		vec3_t hi = { g->segPos.x + g->ext.x, g->segPos.y + g->ext.y, g->segPos.z + g->ext.z };
		e = Grid_FirstInBox(g, &g->segPos, &hi);
	}
	return Grid_CurEntity(g);
}

cEntity *__thiscall Grid_FirstOnSegment(void *grid, const vec3_t *from, const vec3_t *to, const vec3_t *ext) {
	cCollisionGrid *g = grid;
	float dy = (to->y - from->y) / g->cell.y;
	float dx = (to->x - from->x) / g->cell.x;
	float dz = (to->z - from->z) / g->cell.z;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));

	g->ext = *ext;
	g->segPos.x = from->x - g->ext.x * 0.5f;
	g->segPos.z = from->z - g->ext.z * 0.5f;
	g->segPos.y = from->y - g->ext.y * 0.5f;
	float inv = 1.0f / len;
	g->segStep.x = g->cell.x * dx * inv;
	g->segStep.y = g->cell.y * dy * inv;
	g->segStep.z = g->cell.z * dz * inv;
	g->steps = (int)len + 2;
	Grid_Stamp(g);
	vec3_t hi = { g->ext.x + g->segPos.x, g->segPos.y + g->ext.y, g->segPos.z + g->ext.z };
	if (!Grid_FirstInBox(g, &g->segPos, &hi)) {
		Grid_NextOnSegment(g);
	}
	return Grid_CurEntity(g);
}

void __thiscall Grid_Add(void *grid, cEntity *e) {
	cCollisionGrid *g = grid;
	Nto1Node *node = Entity_GridNode(e);
	Nto1Node *old = node->target;
	Nto1Node *cell;

	CHECK(g->cells);
	int x = (int)((Entity_Pos(e)->x - g->min.x) / g->cell.x);
	int y = (int)((Entity_Pos(e)->y - g->min.y) / g->cell.y);
	int z = (int)((Entity_Pos(e)->z - g->min.z) / g->cell.z);
	if (!e->flag_4c) {
		cell = NULL;
	} else if (x < 0 || x >= g->nx || y < 0 || y >= g->ny || z < 0 || z >= g->nz) {
		cell = &g->outside;
	} else {
		cell = &g->cells[Grid_Index(g, x, y, z)];
	}
	if (cell == old) {
		return;
	}
	if (g->cur == node) {
		g->cur = Ring_First(g->cur);
	}
	if (old) {
		CHECK(node->data);
		Nto1Node_Unlink(node);
	}
	if (cell) {
		CHECK(!node->target);
		node->target = cell->target;
		node->next = cell->next;
		node->prev = cell;
		cell->next->prev = node;
		cell->next = node;
	}
}

vec3_t *__thiscall Grid_ClampPos(void *grid, vec3_t *out, const vec3_t *pos) {
	const cCollisionGrid *g = grid;

	out->x = g->min.x >= pos->x ? g->min.x : Float_MinSS(g->max.x, pos->x);
	out->z = g->min.z >= pos->z ? g->min.z : Float_MinSS(g->max.z, pos->z);
	out->y = Float_MaxSS(g->min.y, pos->y);
	return out;
}

void __thiscall Grid_Draw(void *grid) {
	const cCollisionGrid *g = grid;
	const float m = 0.05f;

	CFont_PrintfAlign(g_FontHoratioSmall, 20.0f, 20.0f, 0, "HiValidCollide : %d", g->stamp);
	for (float x = 0.0f; (float)g->nx > x; x = x + 1.0f) {
		for (float y = 0.0f; (float)g->ny > y; y = y + 1.0f) {
			float x1 = x + 1.0f, y1 = y + 1.0f;
			for (float z = 0.0f; (float)g->nz > z;) {
				int i = (int)(((float)g->ny * z + y) * (float)g->nx + x);
				uint32_t v = g->visited[i] + 1;
				float z1 = z + 1.0f;
				vec3_t hi, lo;
				hi.x = (g->min.x + x1 * g->cell.x) - g->cell.x * m;
				hi.z = (g->min.z + g->cell.z * z1) - g->cell.z * m;
				hi.y = (g->min.y + g->cell.y * y1) - g->cell.y * m;
				lo.x = (g->min.x + g->cell.x * x) + g->cell.x * m;
				lo.z = (g->min.z + g->cell.z * z) + g->cell.z * m;
				lo.y = (g->min.y + g->cell.y * y) + g->cell.y * m;
				DebugUtils_DrawBox(&lo, &hi, ((v << 14) & 0x8000) | ((v << 5) & 0x80) | (v << 23));
				z = z1;
			}
		}
	}
}


void Dummies_Draw(void) {
	box_t box;
	WorldNodeInfo info;
	vec3_t ang, p, top;
	char name[100];

	if (!strcmp(g_ShowDummy, "")) {
		return;
	}
	World_GetCollideBox(&box);
	WorldInfo_Begin(g_ShowDummy, 1);
	while (WorldInfo_Next(&info)) {
		Matrix_GetRotAng(&info.m, &ang);
		p = info.pos;
		Str_Copy(name, info.Name, 100);
		DebugUtils_DrawSphere(&p, g_IsMapOutDoor ? 750.0f : 130.0f, 0xff0000, 10, 10);
		top = p;
		top.y = box.max.y;
		Render_Line(&p, &top, 0xff0000);
	}
}

byte DummySearch_NextEuler(char *buf, int size, vec3_t *pos, vec3_t *ang) {
	WorldNodeInfo info;
	vec3_t tmp;
	byte r = WorldInfo_Next(&info);

	if (r) {
		*pos = info.pos;
		*ang = *Matrix_GetEuler(&info.m, &tmp);
		Str_Copy(buf, info.Name, size);
	}
	return r;
}

byte DummySearch_Next(char *buf, int size, vec3_t *pos, float *ang, byte *flag) {
	WorldNodeInfo info;
	vec3_t tmp;
	byte r = WorldInfo_Next(&info);

	if (r) {
		const vec3_t *a = Matrix_GetRotAng(&info.m, &tmp);
		pos->x = a->x;
		pos->y = a->y;
		ang[0] = pos->x;
		ang[1] = pos->y;
		*pos = info.pos;
		Str_Copy(buf, info.Name, size);
		*flag = info.dummy;
	}
	return r;
}

void DummySearch_Start(const char *name, int flag) {
	WorldInfo_Begin(name, (byte)flag);
}

void *EntityLists_Get(const char *name) {
	uint32_t mask = 0;

	for (; *name; name++) {
		switch (*name) {
		case 'V':
		case 'v':
			mask |= 1;
			break;
		case 'W':
		case 'w':
			mask |= 2;
			break;
		case 'I':
		case 'i':
			mask |= 4;
			break;
		case 'L':
		case 'l':
			mask |= 8;
			break;
		case 'R':
		case 'r':
			mask |= 0x10;
			break;
		default:
			Con_Warnf("WARNING : '%c' is not a valid collision mask\n", *name);
			break;
		}
	}
	return (void *)(uintptr_t)mask;
}

/* the entity lists */

void *__thiscall cEntityList_ctor(void *mem, const char *name) {
	cEntityListData *self = mem;

	cString_ctor(&self->name);
	cPy_Function_ctor(&self->onDeath);
	cPy_Function_ctor(&self->onDamage);
	self->ring.target = NULL;
	self->ring.data = NULL;
	self->ring.next = &self->ring;
	self->ring.prev = &self->ring;
	self->numElems = NULL;
	cString_Set(&self->name, name);
	self->ring.target = self;
	CHECK(!self->numElems);
	int *n = operator_new(4);
	if (n) {
		*n = 0;
	}
	self->numElems = n;
	self->func = (int (*)(void))Game_Ptr(CENTITYLIST_FUNC);
	return self;
}

cEntity *__thiscall cEntityList_GetFirst(cEntityList *list) {
	Nto1Node *n = ((cEntityListData *)list)->ring.next;

	if (n->data && n) {
		return n->data;
	}
	return NULL;
}

cEntity *__thiscall cEntityList_Nearest(void *list, const vec3_t *pos, cEntity *exclude, float *dist) {
	cEntityListData *self = list;
	cEntity *best = NULL;
	float level = (float)Floor_Get(g_pWorld->floor, pos->y);
	float bestD = 3.4028235e38f;

	for (Nto1Node *n = Ring_First(&self->ring); n; n = Ring_First(n)) {
		cEntity *e = n->data;
		vec3_t tmp, tmp2;

		if (exclude && exclude == e) {
			continue;
		}
		const vec3_t *p = e->vftable->GetPos(e, &tmp);
		float dx = p->x - pos->x;
		float dy = p->y - pos->y;
		float dz = p->z - pos->z;
		p = e->vftable->GetPos(e, &tmp2);
		float dl = level - (float)Floor_Get(g_pWorld->floor, p->y);
		float d = (float)__builtin_fabs((double)dl) * 400000000.0f + (dy * dy + dx * dx + dz * dz);
		if (bestD > d) {
			best = e;
			bestD = d;
		}
	}
	if (dist) {
		*dist = (float)__builtin_sqrt((double)bestD);
	}
	return best;
}

int __thiscall cEntityList_Var(void *list, const char *name, int set, char **s, double *d, int *i) {
	cEntityListData *self = list;

	(void)d;
	if (!(byte)set) {
		if (s) {
			if (!crt_strnicmp(name, "Near", 4)) {
				cEntity *e = World_FindEntity(name + 4);
				if (e) {
					vec3_t tmp;
					cEntity *near = cEntityList_Nearest(self, e->vftable->GetPos(e, &tmp), e, NULL);
					if (near) {
						*s = near->name;
						return 1;
					}
				}
				*s = Game_Ptr(0x3a3228);	/* "" */
				return 1;
			}
			if (!crt_stricmp(name, "First")) {
				Nto1Node *n = self->ring.next;
				if (n->data && n && n->data) {
					*s = ((cEntity *)n->data)->name;
					return 1;
				}
				*s = Game_Ptr(0x3a3228);
				return 1;
			}
		}
		if (i && !crt_stricmp(name, "Num")) {
			*i = self->numElems ? *self->numElems : -1;
			return 3;
		}
		return 0;
	}
	if (s) {
		if (!crt_stricmp(name, "OnDeath")) {
			cPy_Function_SetFunc(&self->onDeath, *s);
			return 1;
		}
		if (s && !crt_stricmp(name, "OnDamage")) {
			cPy_Function_SetFunc(&self->onDamage, *s);
			return 1;
		}
	}
	return 0;
}
