/*
 * Engine/MultiSprite.cpp of the engine3d: the sprite tables of the textures, see include/scrap/texture.h.
 */
#include <string.h>

#include "scrap/bmp.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filesys.h"
#include "scrap/table.h"
#include "scrap/texture.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

#define MAX_SPRITES 256

CMultiSprite *CMultiSprite_ctor(CMultiSprite *self) {
	self->p = NULL;
	self->n = 0;
	self->width = 0;
	self->height = 0;
	return self;
}

void CMultiSprite_dtor(CMultiSprite *self) {
	if (self->p) {
		operator_delete_array(self->p);
		self->p = NULL;
		self->n = 0;
	}
}

byte CMultiSprite_Build(CMultiSprite *self, const char *name) {
	char mst[260];

	CHECK(!self->n);
	Path_Format(mst, name, "&U&D&N.mst");
	if (File_TimeDiff(mst, name, 0) < 0) {
		E3D_LogBlue((&g_FileLog), "WARN: [%s] is older than [%s]", mst, name);
	} else if (CMultiSprite_Load(self, mst)) {
		return 1;
	}
	Con_Verbosef("CMultiSprite: BuildMultiSpriteTable: Building table: '%s'\n", mst);
	if (!CMultiSprite_BuildTable(self, name)) {
		CheckFailed("BuildTable( Texture )", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!CMultiSprite_Save(self, mst)) {
		Con_Bluef("CMultiSprite: BuildMultiSpriteTable: Could not write to file: '%s'\n", mst);
		return 0;
	}
	return 1;
}

static byte Index_Alpha(const CBmp *b, uint32_t x, uint32_t y) {
	return ((const byte *)Img_Pixel(b, x, y, 4))[3];
}

byte CMultiSprite_BuildTable(CMultiSprite *self, const char *name) {
	CBmp bmp = { 0, 0, NULL }, pal = { 0, 0, NULL };
	MSprite info[MAX_SPRITES];
	char path[260];
	byte ok = 0;

	const char *ext = strrchr(name, '.');
	if ((!ext || crt_stricmp(ext, ".dds")) && CBmp_Load(&bmp, name, 0)) {
		Con_Bluef("CMultiSprite: BuildMultiSpriteTable: File not found: '%s'\n", name);
	}
	Path_Format(path, name, "&U&D&N.index.bmp");
	if (CBmp_Load(&pal, path, 0x100)) {
		Con_Bluef("CMultiSprite: BuildMultiSpriteTable: File not found: '%s'\n", path);
		goto done;
	}
	if (bmp.img) {
		CHECK(bmp.width == pal.width);
		CHECK(bmp.height == pal.height);
	}
	self->width = pal.width;
	self->height = pal.height;
	memset(info, 0, sizeof(info));
	int maxIndex = 0;
	for (int y = 0; y < pal.height; y++) {
		Img_Pixel(&pal, 0, (uint32_t)y, 4);
		for (int x = 0; x < pal.width; x++) {
			byte idx = ((const byte *)pal.img)[((uint32_t)y * (uint32_t)pal.width + (uint32_t)x) * 4 + 3];
			if (!idx) {
				continue;
			}
			MSprite *s = &info[idx];
			if (!s->cx && !s->cy) {
				int x2 = x, y2 = y;
				while (x2 < pal.width && Index_Alpha(&pal, (uint32_t)x2, (uint32_t)y) == idx) {
					x2++;
				}
				while (y2 < pal.height && Index_Alpha(&pal, (uint32_t)x, (uint32_t)y2) == idx) {
					y2++;
				}
				s->x = (uint16_t)x;
				s->y = (uint16_t)y;
				s->cx = (uint16_t)(x2 - x);
				s->cy = (uint16_t)(y2 - y);
			} else {
				CHECK(s->cx);
				CHECK(s->cy);
				CHECK(x >= s->x);
				CHECK(y >= s->y);
				CHECK(x <= s->x + s->cx - 1);
				CHECK(y <= s->y + s->cy - 1);
			}
			if (idx > maxIndex) {
				maxIndex = idx;
			}
		}
	}
	CHECK(maxIndex);
	Table_Alloc8((CTable *)&self->n, maxIndex + 1);
	memcpy(self->p, info, (uint32_t)self->n * sizeof(MSprite));
	ok = 1;
done:
	Img_Free(&pal, 4);
	Img_Free(&bmp, 4);
	return ok;
}

byte CMultiSprite_GetSize(const CMultiSprite *self, uint32_t i, uint32_t *cx, uint32_t *cy) {
	CHECK(cx && cy);
	if (!i || !self->n) {
		*cx = (uint32_t)self->width;
		*cy = (uint32_t)self->height;
		return 0;
	}
	TABLE_CHECK(self->p, i, self->n);
	*cx = self->p[i].cx;
	*cy = self->p[i].cy;
	return 1;
}

byte CMultiSprite_GetUV(const CMultiSprite *self, uint32_t i, float *texMin, float *texMax) {
	CHECK(texMin && texMax);
	if (!i || !self->n) {
		texMin[0] = 0.0f;
		texMin[1] = 0.0f;
		texMax[0] = 1.0f;
		texMax[1] = 1.0f;
		return 0;
	}
	CHECK(i < (uint32_t)self->n);
	const MSprite *s = &self->p[i];
	texMin[0] = (float)s->x / (float)self->width;
	texMin[1] = (float)s->y / (float)self->height;
	texMax[0] = (float)((uint32_t)s->cx + s->x) / (float)self->width;
	texMax[1] = (float)((uint32_t)s->cy + s->y) / (float)self->height;
	return 1;
}

byte CMultiSprite_Load(CMultiSprite *self, const char *name) {
	cFileBlock f;
	byte ok = 0;

	cFileBlock_ctor(&f);
	if (g_BuildMST && cFileBlock_Open(&f, name, 1, 0x12)) {
		cFileBlock_OpenBlockRead(&f, "MST");
		CHECK(cFileBlock_ReadInt(&f) == MST_VER);
		self->width = cFileBlock_ReadInt(&f);
		self->height = cFileBlock_ReadInt(&f);
		Table_Alloc8((CTable *)&self->n, cFileBlock_ReadInt(&f));
		cFileBlock_Read(&f, self->p, self->n * (int)sizeof(MSprite));
		cFileBlock_CloseBlockRead(&f, "MST");
		cFileBlock_Close(&f);
		ok = 1;
	}
	cFileBlock_dtor(&f);
	return ok;
}

byte CMultiSprite_Save(const CMultiSprite *self, const char *name) {
	cFileBlock f;

	cFileBlock_ctor(&f);
	int ok = cFileBlock_Open(&f, name, 6, 1);
	if (ok) {
		cFileBlock_OpenBlockWrite(&f, "MST");
		cFileBlock_WriteInt(&f, MST_VER);
		cFileBlock_WriteInt(&f, self->width);
		cFileBlock_WriteInt(&f, self->height);
		cFileBlock_WriteInt(&f, self->n);
		cFileBlock_Write(&f, self->p, self->n * (int)sizeof(MSprite));
		cFileBlock_CloseBlockWrite(&f, "MST");
		cFileBlock_Close(&f);
	}
	cFileBlock_dtor(&f);
	return ok != 0;
}

cGVar GVar_BuildMST;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_BuildMST, &GVar_BuildMST, "BuildMST", 0, "Build and load MultiSprite MST files (0/1)", 1)
