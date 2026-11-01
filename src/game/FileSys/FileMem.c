/*
 * cFileMem, a file in a memory buffer of maxSize bytes; size is the end of the data. Original source:
 * FileSys/FileMem.cpp.
 */
#include <string.h>

#include "scrap/filesys.h"

cFileMem *cFileMem_ctor(cFileMem *self, void *buffer, int maxSize, byte ownsBuffer) {
	self->buffer = buffer;
	self->maxSize = maxSize;
	self->ownsBuffer = ownsBuffer;
	self->base.name = NULL;
	self->base.mode = 0;
	self->base.vftable = cFileMem_vftable;
	self->size = 0;
	self->pos = NULL;
	return self;
}

void cFileMem_dtor(cFileMem *self) {
	self->base.vftable = cFileMem_vftable;
	if (self->ownsBuffer && self->buffer) {
		Mem_Free(self->buffer);
	}
	cAbstractFile_dtor(&self->base);
}

cFileMem *cFileMem_Destroy(cFileMem *self, int flags) {
	cFileMem_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
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
int cFileMem_Open(cFileMem *self, const char *name, uint32_t mode) {
	(void)name;
	self->pos = self->buffer;
	self->size = (mode & FILE_MODE_CREATE) ? 0 : self->maxSize;
	self->base.mode = mode;
	return 1;
}

int cFileMem_Exists(cFileMem *self, const char *name) {
	(void)self;
	(void)name;
	return 1;
}

int cFileMem_Close(cFileMem *self) {
	self->pos = NULL;
	return 1;
}

int cFileMem_Read(cFileMem *self, void *dst, int size) {
	CHECK((int)(self->pos - self->buffer) + size <= self->size);
	memcpy(dst, self->pos, size);
	self->pos += size;
	return size;
}

int cFileMem_Write(cFileMem *self, const void *src, int size) {
	int newpos = (int)(self->pos - self->buffer) + size;
	CHECK(newpos <= self->maxSize);
	memcpy(self->pos, src, size);
	self->pos += size;
	if (self->size < newpos) {
		self->size = newpos;
	}
	return size;
}

int cFileMem_Seek(cFileMem *self, int pos) {
	if (self->size <= pos) {
		return -1;
	}
	self->pos = self->buffer + pos;
	return pos;
}

int cFileMem_Tell(cFileMem *self) {
	return (int)(self->pos - self->buffer);
}

int cFileMem_Size(cFileMem *self) {
	return self->size;
}
