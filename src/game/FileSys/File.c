/*
 * cFile, a file on disk through the CRT of the exe. Paths are relative to the game unless FILE_MODE_ABSOLUTE is
 * set. Original source: FileSys/File.cpp.
 */
#include "scrap/console.h"
#include "scrap/filesys.h"

enum { CRT_SEEK_SET = 0, CRT_SEEK_END = 2 };

void __fastcall cFile_dtor(cFile *self) {
	self->base.vftable = cFile_vftable;
	if (self->f) {
		crt_fclose(self->f);
	}
	cAbstractFile_dtor(&self->base);
}

cFile *__thiscall cFile_Destroy(cFile *self, int flags) {
	cFile_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/*
 * Always 1: a file that does not open only logs "File could not be opened" and leaves f NULL. An unknown access
 * mode is a failed CHECK; the original then passes the mode number as the fopen mode string.
 */
int __thiscall cFile_Open(cFile *self, const char *name, uint32_t mode) {
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
		Con_SysErrorf("\nFile could not be opened: '%s'\n", self->base.name);
	}
	return 1;
}

int __thiscall cFile_Exists(cFile *self, const char *name) {
	(void)self;
	return File_Access(name, 0) == 0;
}

int __thiscall cFile_Close(cFile *self) {
	if (cAbstractFile_Close(&self->base)) {
		int res = crt_fclose(self->f);
		self->f = NULL;
		return res == 0;
	}
	CHECK(!self->f);
	return 0;
}

int __thiscall cFile_Read(cFile *self, void *dst, int size) {
	CHECK(self->f);
	int n = (int)crt_fread(dst, 1, size, self->f);
	CHECK(n == size);
	return n;
}

int __thiscall cFile_Write(cFile *self, const void *src, int size) {
	CHECK(self->f);
	int n = (int)crt_fwrite(src, 1, size, self->f);
	CHECK(n == size);
	return n;
}

int __thiscall cFile_Seek(cFile *self, int pos) {
	CHECK(self->f);
	return crt_fseek(self->f, pos, CRT_SEEK_SET) == 0 ? pos : -1;
}

int __thiscall cFile_Tell(cFile *self) {
	CHECK(self->f);
	return crt_ftell(self->f);
}

int __thiscall cFile_Size(cFile *self) {
	CHECK(self->f);
	long pos = crt_ftell(self->f);
	crt_fseek(self->f, 0, CRT_SEEK_END);
	int size = crt_ftell(self->f);
	crt_fseek(self->f, pos, CRT_SEEK_SET);
	return size;
}
