/*
 * cFileMem, a file in a memory buffer of maxSize bytes; size is the end of the data. Original source:
 * FileSys/FileMem.cpp.
 */
#include <string.h>

#include "scrap/filesys.h"

#define CFILEMEM_VFTABLE (&cFileMem_vftable)
static const cAbstractFileVtbl cFileMem_vftable;

cFileMem *cFileMem_ctor(cFileMem *self, void *buffer, int maxSize, byte ownsBuffer) {
	self->buffer = buffer;
	self->maxSize = maxSize;
	self->ownsBuffer = ownsBuffer;
	self->base.name = NULL;
	self->base.mode = 0;
	self->base.vftable = CFILEMEM_VFTABLE;
	self->size = 0;
	self->pos = NULL;
	return self;
}

void cFileMem_dtor(cFileMem *self) {
	self->base.vftable = CFILEMEM_VFTABLE;
	if (self->ownsBuffer && self->buffer) {
		Mem_Free(self->buffer);
	}
	cAbstractFile_dtor(&self->base);
}

static cAbstractFile *cFileMem_Destroy(cAbstractFile *base, int flags) {
	cFileMem *self = (cFileMem *)base;
	cFileMem_dtor(self);
	if (flags & 1) {
		operator_delete(self);
	}
	return base;
}

/* a new buffer with size bytes of data in it */
void cFileMem_SetBuffer(cFileMem *self, void *buffer, int maxSize, byte ownsBuffer, int size) {
	self->buffer = buffer;
	self->size = size;
	self->maxSize = maxSize;
	self->ownsBuffer = ownsBuffer;
	self->pos = NULL;
}

/* the name is not kept; FILE_MODE_CREATE starts empty, else the whole buffer is data */
int cFileMem_Open(cAbstractFile *elem, const char *name, uint32_t mode) {
	cFileMem *self = (cFileMem *)elem;
	(void)name;
	self->pos = self->buffer;
	self->size = (mode & FILE_MODE_CREATE) ? 0 : self->maxSize;
	self->base.mode = mode;
	return 1;
}

int cFileMem_Close(cAbstractFile *base) {
	cFileMem *self = (cFileMem *)base;
	self->pos = NULL;
	return 1;
}

int cFileMem_Read(cAbstractFile *base, void *dst, int size) {
	cFileMem *self = (cFileMem *)base;
	CHECK((int)(self->pos - self->buffer) + size <= self->size);
	memcpy(dst, self->pos, size);
	self->pos += size;
	return size;
}

int cFileMem_Write(cAbstractFile *base, const void *src, int size) {
	cFileMem *self = (cFileMem *)base;
	int newpos = (int)(self->pos - self->buffer) + size;
	CHECK(newpos <= self->maxSize);
	memcpy(self->pos, src, size);
	self->pos += size;
	if (self->size < newpos) {
		self->size = newpos;
	}
	return size;
}

int cFileMem_Seek(cAbstractFile *base, int pos) {
	cFileMem *self = (cFileMem *)base;
	if (self->size <= pos) {
		return -1;
	}
	self->pos = self->buffer + pos;
	return pos;
}

int cFileMem_Tell(cAbstractFile *base) {
	cFileMem *self = (cFileMem *)base;
	return (int)(self->pos - self->buffer);
}

int cFileMem_Size(cAbstractFile *base) {
	cFileMem *self = (cFileMem *)base;
	return self->size;
}

/* the vtable of cFileMem */
static const cAbstractFileVtbl cFileMem_vftable = {
	.Open = cFileMem_Open,
	.Exists = cAbstractFile_ExistsTrue,
	.Close = cFileMem_Close,
	.Read = cFileMem_Read,
	.Write = cFileMem_Write,
	.Seek = cFileMem_Seek,
	.Tell = cFileMem_Tell,
	.Size = cFileMem_Size,
	.Destroy = cFileMem_Destroy,
};
