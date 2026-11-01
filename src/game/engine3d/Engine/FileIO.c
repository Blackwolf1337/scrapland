/*
 * File access of the engine3d, original source Engine/ArchIO.cpp (include/scrap/archio.h).
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/scrap.h"

enum { CRT_ENOENT = 2 };

/* the engine works with paths relative to the game directory, a drive is fatal (Z: is Wine's root) */
static void File_CheckRelative(const char *name) {
	if (name[0] && name[1] == ':' && name[0] != 'Z') {
		Con_FatalErrorf("El path debe ser relativo: %s", name);
	}
}

static const char *File_LogKind(byte pack) {
	return pack ? "pack" : "file";
}

/* cFilePack_Destroy without the vtable: close was already called */
static void FileIO_Delete(cAbstractFile *f) {
	cFilePack_dtor((cFilePack *)f);
	operator_delete_sized(f, sizeof(cFilePack));
}

CFileIO *__thiscall CFileIO_ctor(CFileIO *self, const char *name, const char *mode) {
	self->f = NULL;
	self->error = 0;
	self->err = 0;
	self->block = NULL;
	CHECK(name);
	self->f = FileIO_Open(name, mode);
	if (!self->f) {
		self->error = 1;
		self->err = *crt_errno();
	}
	return self;
}

void __thiscall CFileIO_dtor(CFileIO *self) {
	CHECK(self->error || !self->block);
	CFileIO_Close(self);
}

void Ptr_Delete(void **p) {
	if (*p) {
		operator_delete_sized(*p, 0);
		*p = NULL;
	}
}

void *FileIO_ReadAll(const char *name, void **buf, uint32_t *size) {
	cAbstractFile *f = FileIO_Open(name, "rb");
	if (!f) {
		Con_FatalErrorf("File not found%s: %s", PackLog_Closed() ? " in packfile" : "", name);
		CHECK(f);
	}
	int len = f->vftable->Size(f);
	CHECK(len > 0);
	void *data = operator_new_array(len);
	CHECK(f->vftable->Read(f, data, len) == len);
	int closed = f->vftable->Close(f);
	FileIO_Delete(f);
	CHECK(closed);
	if (buf) {
		*buf = data;
	}
	if (size) {
		*size = len;
	}
	return data;
}

int __thiscall CFileIO_Finish(CFileIO *self) {
	CFileIO_Close(self);
	return 0;
}

void File_RelPath(char *dest, const char *path) {
	File_CheckRelative(path);
	String_Copy(dest, path, MAX_FILEPATH);
}

/* the open blocks are dropped: the block pointer starts with its id, so it passes for the id */
int __thiscall CFileIO_Close(CFileIO *self) {
	while (self->block) {
		CFileIO_PopBlock(self, (const char *)self->block);
	}
	if (self->f) {
		int closed = self->f->vftable->Close(self->f);
		FileIO_Delete(self->f);
		if (!closed && !self->error) {
			self->error = 1;
			self->err = *crt_errno();
		}
		self->f = NULL;
	}
	return 0;
}

int __thiscall CFileIO_Write(CFileIO *self, const void *buf, int n) {
	if (self->error) {
		return -1;
	}
	if (n) {
		CHECK(self->f->vftable->Write(self->f, buf, n) == n);
	}
	return 0;
}

int __thiscall CFileIO_WriteString(CFileIO *self, const char *s) {
	int len = s ? (int)strlen(s) + 1 : 0;

	if (self->error) {
		return -1;
	}
	CHECK(self->f->vftable->Write(self->f, &len, 4) == 4);
	if (self->error) {
		return -1;
	}
	if (len) {
		CHECK(self->f->vftable->Write(self->f, s, len) == len);
	}
	return 0;
}

/* the size of the block into its placeholder */
int __thiscall CFileIO_EndBlockWrite(CFileIO *self, const char *id) {
	if (self->error) {
		return -1;
	}
	CHECK(self->block);
	int pos = self->f->vftable->Tell(self->f);
	if (pos == -1) {
		return CFileIO_SetError(self, NULL);
	}
	CHECK(!PackLog_Closed());
	if (self->f->vftable->Seek(self->f, self->block->pos) >= 0) {
		if (CFileIO_WriteInt(self, pos - self->block->pos - 4) != 0) {
			return -1;
		}
		CHECK(!PackLog_Closed());
		if (self->f->vftable->Seek(self->f, pos) >= 0) {
			CFileIO_PopBlock(self, id);
			return 0;
		}
	}
	return CFileIO_SetError(self, NULL);
}

int __thiscall CFileIO_WriteInt(CFileIO *self, int32_t v) {
	if (self->error) {
		return -1;
	}
	CHECK(self->f->vftable->Write(self->f, &v, 4) == 4);
	return 0;
}

int __thiscall CFileIO_BeginBlockWrite(CFileIO *self, const char *id) {
	if (self->error) {
		return -1;
	}
	uint32_t v = IdToDword(id);
	if (self->error) {
		return -1;
	}
	CHECK(self->f->vftable->Write(self->f, &v, 4) == 4);
	int pos = self->f->vftable->Tell(self->f);
	if (pos == -1) {
		CFileIO_SetError(self, NULL);
		return -1;
	}
	if (CFileIO_WriteInt(self, 0) != 0) {
		return -1;
	}
	CFileIO_PushBlock(self, id, pos);
	return 0;
}

int FileIO_Close(void *f) {
	cAbstractFile *file = f;
	int closed = file->vftable->Close(file);
	FileIO_Delete(file);
	return closed ? 0 : -1;
}

int FileIO_Getc(void *f) {
	cAbstractFile *file = f;
	char ch;

	CHECK(file->vftable->Read(file, &ch, 1) == 1);
	return (byte)ch;
}

char *FileIO_Gets(char *buf, int size, void *f) {
	cAbstractFile *file = f;

	CHECK(buf && size > 0);
	int left = file->vftable->Size(file) - file->vftable->Tell(file);
	int n = size - 1 <= left ? size - 1 : left;
	if (left < 1) {
		return NULL;
	}
	char *p = buf;
	char ch;
	do {
		if (n < 1) {
			break;
		}
		CHECK(file->vftable->Read(file, &ch, 1) == 1);
		n--;
		*p++ = ch;
	} while (ch != '\n');
	*p = '\0';
	return buf;
}

int __thiscall CFileIO_Unsupported(CFileIO *self) {
	if (!self->error) {
		CHECK(0);
		self->error = 1;
		self->err = *crt_errno();
	}
	return -1;
}

int FileIO_Read(void *buf, int size, int count, void *f) {
	cAbstractFile *file = f;

	CHECK(file->vftable->Read(file, buf, size * count) == size * count);
	return count;
}

int FileIO_Size(void *f) {
	cAbstractFile *file = f;
	return file->vftable->Size(file);
}

int FileIO_Tell(void *f) {
	cAbstractFile *file = f;
	return file->vftable->Tell(file);
}

int File_AccessPack(const char *name, int mode) {
	return File_Check(name, mode, 1);
}

uint32_t IdToDword(const char *id) {
	CHECK(id);
	const char *p1 = id[0] ? id + 1 : id;
	const char *p2 = p1[0] ? p1 + 1 : p1;
	const char *p3 = p2[0] ? p2 + 1 : p2;
	return (byte)id[0] | (byte)p1[0] << 8 | (byte)p2[0] << 16 | (uint32_t)(byte)p3[0] << 24;
}

int File_Access(const char *name, int mode) {
	return File_Check(name, mode, 0);
}

void File_MkDirRel(const char *dir) {
	char buf[MAX_FILEPATH];

	File_CheckRelative(dir);
	String_Copy(buf, dir, sizeof(buf));
	int r = crt_mkdir(buf);
	E3D_Log(g_FileLog, "FILE MKDIR: %s <%s>", buf, r ? "error" : "ok");
}

void *File_OpenRel(const char *name, const char *mode) {
	char buf[MAX_FILEPATH];

	File_CheckRelative(name);
	String_Copy(buf, name, sizeof(buf));
	CHECK(!strcmp(mode, "wb") || !strcmp(mode, "wt") || !strcmp(mode, "rb") || !strcmp(mode, "rt"));
	void *f = crt_fopen(buf, mode);
	E3D_Log(g_FileLog, "FILE OPEN: %s [%s] <%s>", buf, mode, f ? "ok" : "not found");
	return f;
}

int File_Stat(const char *name, void *st) {
	return File_StatX(name, st, 0);
}

/* "wb" makes a file on disk, "rb" reads one from the packs or the disk */
void *FileIO_Open(const char *name, const char *mode) {
	uint32_t fmode;

	CHECK(!*g_PackName || !**g_PackName);
	if (!strcmp(mode, "wb")) {
		fmode = FILE_MODE_WRITE | FILE_MODE_CREATE;
	} else {
		CHECK(!strcmp(mode, "rb"));
		if (File_Check(name, 0, 1) != 0) {
			E3D_Log(g_FileLog, "file open: %s <%s>", name, "not found");
			return NULL;
		}
		fmode = FILE_MODE_READ;
	}
	cFilePack *f = cFilePack_ctor(operator_new(sizeof(cFilePack)));
	f->file.base.vftable->Open(&f->file.base, name, fmode);
	return f;
}

int File_StatPack(const char *name, void *st) {
	return File_StatX(name, st, 1);
}

byte __thiscall CFileIO_ReadBool(CFileIO *self) {
	int v = -1;

	if (!self->error) {
		CHECK(self->f->vftable->Read(self->f, &v, 4) == 4);
	} else {
		CHECK(!"leer( &dato, sizeof(dato) ) == 0");
	}
	return v != 0;
}

int __thiscall CFileIO_ReadInt(CFileIO *self) {
	int v = -1;

	if (!self->error) {
		CHECK(self->f->vftable->Read(self->f, &v, 4) == 4);
	} else {
		CHECK(!"leer( &dato, sizeof(dato) ) == 0");
	}
	return v;
}

int __thiscall CFileIO_Read(CFileIO *self, void *buf, int n) {
	if (self->error) {
		return -1;
	}
	if (n) {
		CHECK(self->f->vftable->Read(self->f, buf, n) == n);
	}
	return 0;
}

int __thiscall CFileIO_ReadStringNull(CFileIO *self, char *buf, uint32_t size, byte *isNull) {
	uint32_t len;

	*isNull = 1;
	if (self->error) {
		return -1;
	}
	CHECK(self->f->vftable->Read(self->f, &len, 4) == 4);
	if (!len) {
		*buf = '\0';
		return 0;
	}
	*isNull = 0;
	CHECK(len <= size);
	if (self->error) {
		return -1;
	}
	CHECK(self->f->vftable->Read(self->f, buf, len) == (int)len);
	return 0;
}

int __thiscall CFileIO_ReadString(CFileIO *self, char *buf, uint32_t size) {
	byte isNull;

	return CFileIO_ReadStringNull(self, buf, size, &isNull);
}

int __thiscall CFileIO_Error(CFileIO *self, const char *what) {
	(void)what;
	return self->error ? -1 : 0;
}

int __thiscall CFileIO_Read4(CFileIO *self, void *out) {
	if (self->error) {
		return -1;
	}
	CHECK(self->f->vftable->Read(self->f, out, 4) == 4);
	return 0;
}

int __thiscall CFileIO_BeginBlockSize(CFileIO *self, const char *id, int *size) {
	if (self->error) {
		return -1;
	}
	if (CFileIO_ReadInt(self) != (int)IdToDword(id)) {
		Con_FatalErrorf("CArchIO: block id mismatch: %s", id);
	}
	int n = CFileIO_ReadInt(self);
	if (size) {
		*size = n;
	}
	return 0;
}

int __thiscall CFileIO_BeginBlock(CFileIO *self, const char *id) {
	return CFileIO_BeginBlockSize(self, id, NULL);
}

void __thiscall CFileIO_PushBlock(CFileIO *self, const char *id, int pos) {
	cBlockFile *b = operator_new(sizeof(*b));
	b->prev = self->block;
	b->id = IdToDword(id);
	b->pos = pos;
	self->block = b;
}

int __thiscall CFileIO_SetError(CFileIO *self, const char *what) {
	(void)what;
	self->error = 1;
	self->err = *crt_errno();
	return -1;
}

void __thiscall CFileIO_PopBlock(CFileIO *self, const char *id) {
	CHECK(self->block);
	CHECK(self->block->id == IdToDword(id));
	cBlockFile *prev = self->block->prev;
	operator_delete_sized(self->block, sizeof(cBlockFile));
	self->block = prev;
}

int File_Check(const char *name, int mode, byte pack) {
	char buf[MAX_FILEPATH];

	File_CheckRelative(name);
	String_Copy(buf, name, sizeof(buf));
	if (pack && PackLog_Closed()) {
		if (Pack_FileExists(name)) {
			return 0;
		}
		*crt_errno() = CRT_ENOENT;
		return -1;
	}
	int r = crt_access(buf, mode);
	E3D_Log(g_FileLog, "FILE ACCESS: %s <%s> <%s>", buf, r ? "not found" : "ok", File_LogKind(pack));
	return r;
}

int File_StatX(const char *name, void *st, byte pack) {
	char buf[MAX_FILEPATH];

	File_CheckRelative(name);
	String_Copy(buf, name, sizeof(buf));
	if (pack && PackLog_Closed()) {
		return -1;
	}
	int r = crt_stat32(buf, st);
	E3D_Log(g_FileLog, "FILE STAT: %s <%s> <%s>", buf, r ? "not found" : "ok", File_LogKind(pack));
	return r;
}
