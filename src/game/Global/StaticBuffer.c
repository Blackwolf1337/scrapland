/*
 * Global/StaticBuffer.h: a buffer that only grows, locked between Get and the caller's unlock (lock = 0). The
 * instances of the header share this code.
 */
#include "scrap/scrap.h"

cStaticBuffer *__fastcall cStaticBuffer_ctor(cStaticBuffer *self) {
	self->size = 0;
	self->data = NULL;
	self->lock = 0;
	return self;
}

void __fastcall cStaticBuffer_dtor(cStaticBuffer *self) {
	CHECK(!self->lock);
	Mem_Free(self->data);
	self->data = NULL;
	self->size = 0;
}

void *__thiscall cStaticBuffer_Get(cStaticBuffer *self, uint32_t size) {
	CHECK(!self->lock);
	if (!self->data) {
		self->size = size;
		self->data = Mem_Alloc(size);
	} else if (self->size < size) {
		self->size = size;
		self->data = Mem_Realloc(self->data, size);
	}
	self->lock = 1;
	return self->data;
}

void __fastcall cStaticBuffer_Unlock(cStaticBuffer *self) {
	CHECK(self->lock);
	self->lock = 0;
}
