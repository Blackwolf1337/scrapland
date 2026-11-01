/*
 * cAbstractFile, the base class of the engine files: the name of the open file and its mode, and the string
 * helpers on top of Read/Write. Original source: FileSys/AbstractFile.cpp.
 */
#include <string.h>

#include "scrap/filesys.h"

void cAbstractFile_dtor(cAbstractFile *self) {
	self->vftable = cAbstractFile_vftable;
	if (self->name) {
		Mem_Free(self->name);
	}
}

cAbstractFile *cAbstractFile_Destroy(cAbstractFile *self, int flags) {
	cAbstractFile_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* keeps a copy of the name; the derived classes open the file */
int cAbstractFile_Open(cAbstractFile *self, const char *name, uint32_t mode) {
	CHECK(self->name == NULL);
	CHECK(name != NULL);
	size_t size = strlen(name) + 1;
	self->name = Mem_Alloc(size);
	memcpy(self->name, name, size);
	self->mode = mode;
	return 1;
}

/* 1 if a file was open */
int cAbstractFile_Close(cAbstractFile *self) {
	if (!self->name) {
		return 0;
	}
	Mem_Free(self->name);
	self->name = NULL;
	return 1;
}

/* a string with a length byte into buf of size bytes */
int cAbstractFile_ReadString8(cAbstractFile *self, char *buf, byte size) {
	byte len = 0;
	int res = self->vftable->Read(self, &len, sizeof(len));
	CHECK(res == sizeof(len));
	CHECK(size > len);
	int n = self->vftable->Read(self, buf, len);
	buf[len] = 0;
	return res + n;
}

/* a string with an int length into buf of size bytes */
int cAbstractFile_ReadString(cAbstractFile *self, char *buf, int size) {
	int len = 0;
	int res = self->vftable->Read(self, &len, sizeof(len));
	CHECK(res == sizeof(len));
	CHECK(size > len);
	int n = self->vftable->Read(self, buf, len);
	buf[len] = 0;
	return res + n;
}

/* a string of less than 256 characters with a length byte */
int cAbstractFile_WriteString8(cAbstractFile *self, const char *s) {
	int len = (int)strlen(s);
	CHECK(len < 256);
	byte b = (byte)len;
	int res = self->vftable->Write(self, &b, 1);
	return self->vftable->Write(self, s, b) + res;
}

int cAbstractFile_WriteString(cAbstractFile *self, const char *s) {
	int len = (int)strlen(s);
	int res = self->vftable->Write(self, &len, sizeof(len));
	return self->vftable->Write(self, s, len) + res;
}
