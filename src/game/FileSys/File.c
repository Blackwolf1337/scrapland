/*
 * cFile, a file on disk through the CRT of the exe. Paths are relative to the game unless FILE_MODE_ABSOLUTE is
 * set. Original source: FileSys/File.cpp.
 */
#include "scrap/console.h"
#include "scrap/filesys.h"

enum { CRT_SEEK_SET = 0, CRT_SEEK_END = 2 };

void cFile_dtor(cFile *self) {
	if (self->f) {
		crt_fclose(self->f);
	}
	cAbstractFile_dtor(&self->base);
}

cAbstractFile *cFile_Destroy(cAbstractFile *base, int flags) {
	cFile *self = (cFile *)base;
	cFile_dtor(self);
	if (flags & 1) {
		operator_delete(self);
	}
	return base;
}

/*
 * Always 1: a file that does not open only logs "File could not be opened" and leaves f NULL. An unknown access
 * mode is a failed CHECK; the original then passes the mode number as the fopen mode string.
 */
int cFile_Open(cAbstractFile *elem, const char *name, uint32_t mode) {
	cFile *self = (cFile *)elem;
	const char *m;

	cAbstractFile_Open(&self->base, name, mode);
	switch (self->base.mode & 7) {
	case 1:
		m = "rb";
		break;
	case 2:
	case 6:
		m = "wb";
		break;
	case 3:
		m = "r+b";
		break;
	case 7:
		m = "w+b";
		break;
	default:
		CHECK(0);
		m = (const char *)(uintptr_t)mode;
		break;
	}
	if (self->base.mode & FILE_MODE_ABSOLUTE) {
		self->f = crt_fopen(self->base.name, m);
	} else {
		self->f = File_OpenRel(self->base.name, m);
	}
	if (!self->f) {
		Con_FatalErrorf("\nFile could not be opened: '%s'\n", self->base.name);
	}
	return 1;
}

int cFile_Exists(cAbstractFile *base, const char *name) {
	cFile *self = (cFile *)base;
	(void)self;
	return File_Access(name, 0) == 0;
}

int cFile_Close(cAbstractFile *elem) {
	cFile *self = (cFile *)elem;
	if (cAbstractFile_Close(&self->base)) {
		int res = crt_fclose(self->f);
		self->f = NULL;
		return res == 0;
	}
	CHECK(!self->f);
	return 0;
}

int cFile_Read(cAbstractFile *base, void *dst, int size) {
	cFile *self = (cFile *)base;
	CHECK(self->f);
	int n = (int)crt_fread(dst, 1, size, self->f);
	CHECK(n == size);
	return n;
}

int cFile_Write(cAbstractFile *base, const void *src, int size) {
	cFile *self = (cFile *)base;
	CHECK(self->f);
	int n = (int)crt_fwrite(src, 1, size, self->f);
	CHECK(n == size);
	return n;
}

int cFile_Seek(cAbstractFile *base, int pos) {
	cFile *self = (cFile *)base;
	CHECK(self->f);
	return crt_fseek(self->f, pos, CRT_SEEK_SET) == 0 ? pos : -1;
}

int cFile_Tell(cAbstractFile *base) {
	cFile *self = (cFile *)base;
	CHECK(self->f);
	return crt_ftell(self->f);
}

int cFile_Size(cAbstractFile *base) {
	cFile *self = (cFile *)base;
	CHECK(self->f);
	long pos = crt_ftell(self->f);
	crt_fseek(self->f, 0, CRT_SEEK_END);
	int size = crt_ftell(self->f);
	crt_fseek(self->f, pos, CRT_SEEK_SET);
	return size;
}
