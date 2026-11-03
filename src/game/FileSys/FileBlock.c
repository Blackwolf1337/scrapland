/*
 * cFileBlock (CFileBlock in its messages): files of nested blocks with a 4 character id and a size. Original
 * source: FileSys/FileBlock.cpp.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/filesys.h"
#include "scrap/scrap.h"

/* the first 4 characters of id in a dword; a shorter id repeats its terminator */
static uint32_t ID_to_DWORD(const char *id) {
	CHECK(id);
	const char *p0 = id;
	const char *p1 = *p0 ? p0 + 1 : p0;
	const char *p2 = *p1 ? p1 + 1 : p1;
	const char *p3 = *p2 ? p2 + 1 : p2;
	return (byte)*p0 | (uint32_t)(byte)*p1 << 8 | (uint32_t)(byte)*p2 << 16 | (uint32_t)(byte)*p3 << 24;
}

static const char *FileName(cFileBlock *self) {
	return self->f ? self->f->name : NULL;
}

/* "File access error" with the name of the file; always 0 */
static int cFileBlock_Error(cFileBlock *self, const char *msg) {
	Con_FatalErrorf("File access error: CFileBlock: '%s' %s\n", FileName(self), msg ? msg : "");
	return 0;
}

cFileBlock *cFileBlock_ctor(cFileBlock *self) {
	self->f = NULL;
	self->block = NULL;
	return self;
}

void cFileBlock_dtor(cFileBlock *self) {
	CHECK(!self->f);
}

/* the file object for the mode: a pack reader for reading only, else a file on disk */
static void cFileBlock_CreateFile(cFileBlock *self, uint32_t mode, byte flags) {
	if (self->f) {
		CHECK(!self->f);
		if (self->f) {
			return;
		}
	}
	byte read = (~flags & 1) | ((flags & FILEBLOCK_READ) != 0);
	if (mode == FILE_MODE_READ && read) {
		void *mem = operator_new(sizeof(cFilePack));
		if (mem) {
			cFilePack_ctor(mem);
			self->f = mem;
			return;
		}
	} else {
		cFile *file = operator_new(sizeof(cFile));
		if (file) {
			file->base.name = NULL;
			file->base.mode = 0;
			file->base.vftable = CFILE_VFTABLE;
			file->f = NULL;
			self->f = &file->base;
			return;
		}
	}
	self->f = NULL;
}

static void cFileBlock_DeleteFile(cFileBlock *self) {
	if (self->f) {
		VFile_Destroy(self->f, 1);
		self->f = NULL;
	}
}

int cFileBlock_Open(cFileBlock *self, const char *filename, uint32_t mode, uint32_t flags) {
	CHECK(!self->block);
	CHECK(filename);
	cFileBlock_CreateFile(self, mode, (byte)flags);
	if ((flags & FILEBLOCK_MUST_EXIST) && !VFile_Exists(self->f, filename)) {
		cFileBlock_DeleteFile(self);
		return 0;
	}
	CHECK(VFile_Open(self->f, filename, mode));
	return 1;
}

int cFileBlock_Close(cFileBlock *self) {
	CHECK(!self->block);
	if (!self->f) {
		return 1;
	}
	int res = VFile_Close(self->f);
	cFileBlock_DeleteFile(self);
	return res;
}

static void cFileBlock_PushBlock(cFileBlock *self, const char *id, int pos) {
	cFileBlockNode *node = operator_new(sizeof(*node));
	node->id = ID_to_DWORD(id);
	node->pos = pos;
	node->prev = self->block;
	self->block = node;
}

static void cFileBlock_PopBlock(cFileBlock *self, const char *id) {
	CHECK(self->block);
	CHECK(self->block->id == ID_to_DWORD(id));
	cFileBlockNode *prev = self->block->prev;
	operator_delete_sized(self->block, sizeof(cFileBlockNode));
	self->block = prev;
}

int cFileBlock_Read(cFileBlock *self, void *mem, int size) {
	CHECK(self->f);
	CHECK(mem);
	if (size && VFile_Read(self->f, mem, size) != size) {
		return cFileBlock_Error(self, "");
	}
	return 1;
}

int cFileBlock_Write(cFileBlock *self, const void *mem, int size) {
	CHECK(self->f);
	CHECK(mem);
	if (size && VFile_Write(self->f, mem, size) != size) {
		return cFileBlock_Error(self, "");
	}
	return 1;
}

int cFileBlock_ReadInt(cFileBlock *self) {
	int data = -1;
	CHECK(cFileBlock_Read(self, &data, sizeof(data)));
	return data;
}

int cFileBlock_WriteInt(cFileBlock *self, int v) {
	return cFileBlock_Write(self, &v, sizeof(v));
}

/* enters a block: its id must follow, then its size (not kept) */
int cFileBlock_OpenBlockRead(cFileBlock *self, const char *id) {
	cFileBlock_PushBlock(self, id, 0);
	CHECK((uint32_t)cFileBlock_ReadInt(self) == self->block->id);
	cFileBlock_ReadInt(self);
	return 1;
}

int cFileBlock_CloseBlockRead(cFileBlock *self, const char *id) {
	cFileBlock_PopBlock(self, id);
	return 1;
}

/* writes the id and room for the size, filled in by CloseBlockWrite */
int cFileBlock_OpenBlockWrite(cFileBlock *self, const char *id) {
	CHECK(self->f);
	uint32_t dw = ID_to_DWORD(id);
	if (!cFileBlock_Write(self, &dw, sizeof(dw))) {
		return cFileBlock_Error(self, "");
	}
	int pos = VFile_Tell(self->f);
	if (pos != -1) {
		int zero = 0;
		if (cFileBlock_Write(self, &zero, sizeof(zero))) {
			cFileBlock_PushBlock(self, id, pos);
			return 1;
		}
	}
	return cFileBlock_Error(self, "");
}

/*
 * writes the size of the block at its start. The original then seeks to the size field again, not back to the
 * end, so data written after the block replaces what follows the size; kept as it is.
 */
int cFileBlock_CloseBlockWrite(cFileBlock *self, const char *id) {
	CHECK(self->f);
	CHECK(self->block);
	int end = VFile_Tell(self->f);
	if (end != -1 && VFile_Seek(self->f, self->block->pos) != -1) {
		int size = end - self->block->pos - 4;
		if (cFileBlock_Write(self, &size, sizeof(size)) &&
			VFile_Seek(self->f, self->block->pos) != -1) {
			cFileBlock_PopBlock(self, id);
			return 1;
		}
	}
	return cFileBlock_Error(self, "");
}
