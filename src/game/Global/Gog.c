/*
 * The GOG Galaxy parts of the game (gog::GoGScrap, rva 0x136b60..0x1380a0, after Global/Version.cpp; no CHECK string
 * names the source) with the MSVC STL code compiled into it: std::string (small buffer of 16, the pointer when the
 * capacity is 16 or more), std::to_string, the vector of the listeners (unique_ptr) and their allocator. Galaxy.dll
 * through the import table of the exe, its interfaces by their vtables (thiscall).
 *
 * The C++ catch blocks of the exe are done without exceptions: Gog_Init catches the galaxy::api::IError that
 * Galaxy.dll throws with an SEH frame of its own (Gog_CatchError); in the string reallocation the only throw is the
 * length check of the allocation, tested before it; the moves of the vector reallocation do not throw.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/scrap.h"
#include "scrap/win32.h"

/* the MSVC std::string (0x18 bytes) */
typedef struct MsvcString {
	union {
		char buf[16];
		char *ptr;
	};				/* 0x00 */
	uint32_t size;			/* 0x10 */
	uint32_t cap;			/* 0x14 */
} MsvcString;
SD_SIZE(MsvcString, 0x18);

#define MSVC_SSO 0x10
#define MSVC_BIG_ALLOC 0x1000		/* from here on aligned to 32 with the real block before it */
#define MSVC_ALIGN_PAD 0x23
#define MSVC_MAX_PTRS 0x3fffffffu

/* a C++ interface of Galaxy.dll */
typedef struct GalaxyIface {
	void **vftable;
} GalaxyIface;

typedef GalaxyIface *(*GalaxyGetIface_t)(void);
typedef void (*GalaxyProcessData_t)(void);
#define g_GalaxyStats GAME_DATA(GalaxyGetIface_t, 0x3a10b8)
#define g_GalaxyUser GAME_DATA(GalaxyGetIface_t, 0x3a10c0)
#define g_GalaxyApps GAME_DATA(GalaxyGetIface_t, 0x3a10c4)
#define g_GalaxyProcessData GAME_DATA(GalaxyProcessData_t, 0x3a10cc)
#define g_GalaxyListenerRegistrar GAME_DATA(GalaxyGetIface_t, 0x3a10d4)
#define g_GalaxyLogger GAME_DATA(GalaxyGetIface_t, 0x3a10c8)

/* galaxy::api::InitOptions */
typedef struct GalaxyInitOptions {
	const char *clientID;		/* 0x00 */
	const char *clientSecret;	/* 0x04 */
	const char *configFilePath;	/* 0x08 */
	const char *storagePath;	/* 0x0c */
	void *galaxyAllocator;		/* 0x10 */
	void *galaxyThreadFactory;	/* 0x14 */
	const char *host;		/* 0x18 */
	uint16_t port;			/* 0x1c */
} GalaxyInitOptions;
SD_OFFSET(GalaxyInitOptions, port, 0x1c);

typedef void (*GalaxyInit_t)(const GalaxyInitOptions *opts);
#define g_GalaxyInit GAME_DATA(GalaxyInit_t, 0x3a10bc)
#define g_GogClientID GAME_DATA(const char *, 0x428ab8)
#define g_GogClientSecret GAME_DATA(const char *, 0x428abc)
#define GOG_CONFIG_PATH ((const char *)Game_Ptr(0x3bbcb4))	/* "." */
#define GOG_AUTH_FAILED ((const char *)Game_Ptr(0x3bbcb8))	/* "Failed to Auth: %s" */
#define GOG_IERROR_TYPE ((const MsvcTypeDescriptor *)Game_Ptr(0x442264))	/* galaxy::api::IError */

#define GALAXY_LISTENER_AUTH 7

/* the listener vtables of the exe */
#define VT_IGALAXYLISTENER 0x3bbc00
#define VT_SELFREGLISTENER 0x3bbc24
#define VT_AUTHLISTENER 0x3bbc38

/* the authorization has come (the stats may be used) */
#define g_GogSignedIn GAME_DATA(byte, 0x44c108)
#define GOG_EMPTY ((const char *)Game_Ptr(0x3a3228))	/* "" */

/* gog::GoGScrap::AuthListener, a SelfRegisteringListener<IAuthListener> */
typedef struct GogAuthListener {
	const void *vftable;		/* 0x0 */
	cGogListeners *owner;		/* 0x4 */
} GogAuthListener;

/* the MSVC C++ exception as _CxxThrowException raises it (SEH code 0xe06d7363, parameters: magic, the object, its
 * ThrowInfo), the parts a catch reads */
#define MSVC_CXX_EXCEPTION 0xe06d7363u
#define MSVC_CXX_MAGIC1 0x19930520u
#define MSVC_CXX_MAGIC3 0x19930522u

typedef struct MsvcTypeDescriptor {
	const void *vftable;
	void *spare;
	char name[];			/* decorated, ".?AVIError@api@galaxy@@" */
} MsvcTypeDescriptor;

typedef struct MsvcCatchableType {
	uint32_t properties;
	const MsvcTypeDescriptor *type;
	int32_t mdisp, pdisp, vdisp;	/* where this base is in the object */
	uint32_t size;
	void *copy;
} MsvcCatchableType;

typedef struct MsvcCatchableTypes {
	int n;
	const MsvcCatchableType *types[];
} MsvcCatchableTypes;

typedef struct MsvcThrowInfo {
	uint32_t attributes;
	void *unwind;			/* the destructor of the object (thiscall) */
	void *forward;
	const MsvcCatchableTypes *catchable;
} MsvcThrowInfo;

/* EXCEPTION_RECORD */
typedef struct SehRecord {
	uint32_t code;
	uint32_t flags;			/* SEH_UNWINDING while the frames are unwound */
	struct SehRecord *next;
	void *address;
	uint32_t nparams;
	uintptr_t info[15];
} SehRecord;

#define SEH_UNWINDING 0x6		/* EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND */
#define SEH_CONTINUE_SEARCH 1

/* an EXCEPTION_REGISTRATION_RECORD on fs:0 with the place to go on after the catch */
typedef struct SehCatchFrame {
	struct SehCatchFrame *prev;
	void *handler;
	void *resume[5];		/* __builtin_setjmp */
} SehCatchFrame;

/* the noreturn helpers of the STL and the CRT */
[[noreturn]] void crt_Xlength_error(const char *msg);
[[noreturn]] void crt_Xout_of_range(const char *msg);
[[noreturn]] void crt_invalid_parameter_noinfo_noreturn(void);
[[noreturn]] void crt_Throw_bad_array_new_length(void);

static inline char *Msvc_Ptr(MsvcString *s) {
	return s->cap >= MSVC_SSO ? s->ptr : s->buf;
}

/* ---- the allocator ---- */

/* _Deallocate of n bytes: the real block of a big one is before it */
void __thiscall MsvcAlloc_Deallocate(void *alloc, void *p, uint32_t n) {
	(void)alloc;
	if (n >= MSVC_BIG_ALLOC) {
		if ((uintptr_t)p & 0x1f) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		void *real = ((void **)p)[-1];
		if ((uintptr_t)real >= (uintptr_t)p) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		uintptr_t d = (uintptr_t)p - (uintptr_t)real;
		if (d < 4) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		if (d > MSVC_ALIGN_PAD) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		p = real;
	}
	operator_delete(p);
}

/* the same for n pointers */
void __thiscall PtrVector_Deallocate(void *alloc, void **p, int n) {
	(void)alloc;
	if ((uint32_t)n > MSVC_MAX_PTRS) {
		crt_invalid_parameter_noinfo_noreturn();
	}
	void *q = p;
	if ((uint32_t)n << 2 >= MSVC_BIG_ALLOC) {
		if ((uintptr_t)q & 0x1f) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		void *real = ((void **)q)[-1];
		if ((uintptr_t)real >= (uintptr_t)q) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		uintptr_t d = (uintptr_t)q - (uintptr_t)real;
		if (d < 4) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		if (d > MSVC_ALIGN_PAD) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		q = real;
	}
	operator_delete(q);
}

/* _Allocate of n bytes */
void *__thiscall MsvcAlloc_Allocate(void *alloc, uint32_t n) {
	(void)alloc;
	if (!n) {
		return NULL;
	}
	if (n >= MSVC_BIG_ALLOC) {
		uint32_t m = n + MSVC_ALIGN_PAD;
		if (m <= n) {
			crt_Throw_bad_array_new_length();
		}
		void *raw = operator_new(m);
		if (!raw) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		void **p = (void **)(((uintptr_t)raw + MSVC_ALIGN_PAD) & ~(uintptr_t)0x1f);
		p[-1] = raw;
		return p;
	}
	void *p = operator_new(n);
	if (!p) {
		crt_invalid_parameter_noinfo_noreturn();
	}
	return p;
}

/* the same for n pointers */
void **__thiscall PtrVector_Allocate(void *alloc, uint32_t n) {
	(void)alloc;
	if (!n) {
		return NULL;
	}
	if (n > MSVC_MAX_PTRS) {
		crt_Throw_bad_array_new_length();
	}
	n <<= 2;
	if (n >= MSVC_BIG_ALLOC) {
		uint32_t m = n + MSVC_ALIGN_PAD;
		if (m <= n) {
			crt_Throw_bad_array_new_length();
		}
		void *raw = operator_new(m);
		if (!raw) {
			crt_invalid_parameter_noinfo_noreturn();
		}
		void **p = (void **)(((uintptr_t)raw + MSVC_ALIGN_PAD) & ~(uintptr_t)0x1f);
		p[-1] = raw;
		return p;
	}
	void **p = operator_new(n);
	if (!p) {
		crt_invalid_parameter_noinfo_noreturn();
	}
	return p;
}

/* ---- std::string ---- */

/* _Tidy_deallocate: back to the empty small buffer */
void __fastcall MsvcString_Tidy(MsvcString *s) {
	if (s->cap >= MSVC_SSO) {
		MsvcAlloc_Deallocate(s, s->ptr, s->cap + 1);
	}
	s->cap = 0xf;
	s->size = 0;
	Msvc_Ptr(s)[0] = 0;
}

static inline void MsvcString_InitEmpty(MsvcString *s) {
	s->cap = 0xf;
	s->size = 0;
	s->buf[0] = 0;
}

/* the move constructor: the small buffer copied, a big one taken over; other is empty after it */
void __thiscall MsvcString_ctorMove(MsvcString *self, MsvcString *other) {
	if (other->cap < MSVC_SSO) {
		if (other->size + 1) {
			memmove(self, other, other->size + 1);
		}
	} else {
		if (self) {
			self->ptr = other->ptr;
		}
		other->ptr = NULL;
	}
	self->size = other->size;
	self->cap = other->cap;
	other->cap = 0xf;
	other->size = 0;
	Msvc_Ptr(other)[0] = 0;
}

/* an allocation of n bytes that throws bad_array_new_length (the pad of the 32 alignment overflows) */
static inline int MsvcAlloc_Overflows(uint32_t n) {
	return n >= MSVC_BIG_ALLOC && n + MSVC_ALIGN_PAD <= n;
}

/*
 * _Reallocate_for: a new buffer for n characters (the capacity grows by half at least), the first keep characters
 * copied. The catch(...) of the exe takes the capacity n when the grown one throws; when that throws too, the string
 * is emptied and the exception goes on.
 */
void __thiscall MsvcString_ReallocFor(MsvcString *self, uint32_t n, uint32_t keep) {
	uint32_t cap = n | 0xf;

	if (cap > 0xfffffffe) {
		cap = n;
	} else {
		uint32_t old = self->cap;
		if (old >> 1 > cap / 3) {
			cap = (old >> 1) + old;
			if (old > 0xfffffffe - (old >> 1)) {
				cap = 0xfffffffe;
			}
		}
	}
	if (MsvcAlloc_Overflows(cap + 1)) {
		cap = n;
		if (MsvcAlloc_Overflows(cap + 1)) {
			MsvcString_Tidy(self);
			crt_Throw_bad_array_new_length();
		}
	}
	char *p = MsvcAlloc_Allocate(self, cap + 1);
	if (keep) {
		memcpy(p, Msvc_Ptr(self), keep);
	}
	MsvcString_Tidy(self);
	self->ptr = p;
	self->cap = cap;
	self->size = keep;
	Msvc_Ptr(self)[keep] = 0;
}

/* erase(off, count) */
MsvcString *__thiscall MsvcString_Erase(MsvcString *self, uint32_t off, uint32_t count) {
	uint32_t size = self->size;

	if (size < off) {
		crt_Xout_of_range("invalid string position");
	}
	if (size - off <= count) {
		self->size = off;
		Msvc_Ptr(self)[off] = 0;
		return self;
	}
	if (count) {
		char *p = Msvc_Ptr(self) + off;
		uint32_t n = size - count;
		if (n - off) {
			memmove(p, p + count, n - off);
		}
		self->size = n;
		Msvc_Ptr(self)[n] = 0;
	}
	return self;
}

/* assign(src, off, count) */
MsvcString *__thiscall MsvcString_AssignSub(MsvcString *self, MsvcString *src, uint32_t off, uint32_t count) {
	if (src->size < off) {
		crt_Xout_of_range("invalid string position");
	}
	if (count > src->size - off) {
		count = src->size - off;
	}
	if (self == src) {
		uint32_t end = off + count;
		if (self->size < end) {
			crt_Xout_of_range("invalid string position");
		}
		self->size = end;
		Msvc_Ptr(self)[end] = 0;
		MsvcString_Erase(self, 0, off);
		return self;
	}
	if (count > 0xfffffffe) {
		crt_Xlength_error("string too long");
	}
	if (self->cap < count) {
		MsvcString_ReallocFor(self, count, self->size);
		if (!count) {
			return self;
		}
	} else if (!count) {
		self->size = 0;
		Msvc_Ptr(self)[0] = 0;
		return self;
	}
	const char *from = src->cap >= MSVC_SSO ? src->ptr : src->buf;
	char *to = Msvc_Ptr(self);
	if (count) {
		memcpy(to, from + off, count);
	}
	self->size = count;
	Msvc_Ptr(self)[count] = 0;
	return self;
}

/* assign(p, n): from inside the string through AssignSub */
MsvcString *__thiscall MsvcString_Assign(MsvcString *self, const char *p, uint32_t n) {
	if (p && p >= Msvc_Ptr(self) && p < Msvc_Ptr(self) + self->size) {
		return MsvcString_AssignSub(self, self, (uint32_t)(p - Msvc_Ptr(self)), n);
	}
	if (n > 0xfffffffe) {
		crt_Xlength_error("string too long");
	}
	if (self->cap < n) {
		MsvcString_ReallocFor(self, n, self->size);
		if (!n) {
			return self;
		}
	} else if (!n) {
		self->size = 0;
		Msvc_Ptr(self)[0] = 0;
		return self;
	}
	char *to = Msvc_Ptr(self);
	if (n) {
		memcpy(to, p, n);
	}
	self->size = n;
	Msvc_Ptr(self)[n] = 0;
	return self;
}

/* insert(off, src, srcoff, count) */
MsvcString *__thiscall MsvcString_InsertSub(MsvcString *self, uint32_t off, MsvcString *src, uint32_t srcoff,
	uint32_t count) {
	uint32_t size = self->size;

	if (size < off) {
		crt_Xout_of_range("invalid string position");
	}
	if (src->size < srcoff) {
		crt_Xout_of_range("invalid string position");
	}
	if (count > src->size - srcoff) {
		count = src->size - srcoff;
	}
	if (~size <= count) {
		crt_Xlength_error("string too long");
	}
	uint32_t n = size + count;
	if (!count) {
		return self;
	}
	if (n > 0xfffffffe) {
		crt_Xlength_error("string too long");
	}
	if (self->cap < n) {
		MsvcString_ReallocFor(self, n, size);
		if (!n) {
			return self;
		}
	} else if (!n) {
		self->size = 0;
		Msvc_Ptr(self)[0] = 0;
		return self;
	}
	char *p = Msvc_Ptr(self);
	if (self->size - off) {
		memmove(p + off + count, p + off, self->size - off);
	}
	if (self == src) {
		uint32_t from = off >= srcoff ? srcoff : srcoff + count;
		p = Msvc_Ptr(self);
		if (count) {
			memmove(p + off, p + from, count);
		}
	} else {
		const char *q = src->cap >= MSVC_SSO ? src->ptr : src->buf;
		p = Msvc_Ptr(self);
		if (count) {
			memcpy(p + off, q + srcoff, count);
		}
	}
	self->size = n;
	Msvc_Ptr(self)[n] = 0;
	return self;
}

/* insert(off, p, n): from inside the string through InsertSub */
MsvcString *__thiscall MsvcString_Insert(MsvcString *self, uint32_t off, const char *p, uint32_t n) {
	if (p && p >= Msvc_Ptr(self) && p < Msvc_Ptr(self) + self->size) {
		return MsvcString_InsertSub(self, off, self, (uint32_t)(p - Msvc_Ptr(self)), n);
	}
	uint32_t size = self->size;
	if (size < off) {
		crt_Xout_of_range("invalid string position");
	}
	if (n >= ~size) {
		crt_Xlength_error("string too long");
	}
	uint32_t total = size + n;
	if (!n) {
		return self;
	}
	if (total > 0xfffffffe) {
		crt_Xlength_error("string too long");
	}
	if (self->cap < total) {
		MsvcString_ReallocFor(self, total, size);
		if (!total) {
			return self;
		}
	} else if (!total) {
		self->size = 0;
		Msvc_Ptr(self)[0] = 0;
		return self;
	}
	char *q = Msvc_Ptr(self);
	if (self->size - off) {
		memmove(q + off + n, q + off, self->size - off);
	}
	q = Msvc_Ptr(self);
	if (n) {
		memcpy(q + off, p, n);
	}
	self->size = total;
	Msvc_Ptr(self)[total] = 0;
	return self;
}

/* operator+(const char *, string &&): the text put in front, the result moved out */
MsvcString *MsvcString_PrependMove(MsvcString *out, const char *l, MsvcString *r) {
	uint32_t n = *l ? (uint32_t)strlen(l) : 0;
	MsvcString *s = MsvcString_Insert(r, 0, l, n);
	MsvcString_InitEmpty(out);
	MsvcString_ctorMove(out, s);
	return out;
}

/* _UIntegral_to_buff: the digits before end */
char *MsvcStr_UIntToBuff(char *end, uint32_t v) {
	do {
		*--end = (char)('0' + v % 10);
		v /= 10;
	} while (v);
	return end;
}

char *MsvcStr_UInt64ToBuff(char *end, uint64_t v) {
	uint32_t lo = (uint32_t)v;

	while (v > 0xffffffffu) {
		uint64_t q = v / 1000000000u;
		uint32_t r = (uint32_t)v - (uint32_t)q * 1000000000u;
		for (int i = 0; i < 9; i++) {
			*--end = (char)('0' + r % 10);
			r /= 10;
		}
		v = q;
	}
	lo = (uint32_t)v;
	do {
		*--end = (char)('0' + lo % 10);
		lo /= 10;
	} while (lo);
	return end;
}

/* _Integral_to_string<int> */
MsvcString *MsvcString_FromInt(MsvcString *out, int v) {
	char buf[11];
	char *end = buf + sizeof(buf);
	char *p;

	if (v < 0) {
		p = MsvcStr_UIntToBuff(end, 0u - (uint32_t)v);
		*--p = '-';
	} else {
		p = MsvcStr_UIntToBuff(end, (uint32_t)v);
	}
	MsvcString_InitEmpty(out);
	if (p != end) {
		MsvcString_Assign(out, p, (uint32_t)(end - p));
	}
	return out;
}

MsvcString *MsvcString_FromUInt64(MsvcString *out, uint64_t v) {
	char buf[21];
	char *end = buf + sizeof(buf);

	char *p = MsvcStr_UInt64ToBuff(end, v);
	MsvcString_InitEmpty(out);
	if (p != end) {
		MsvcString_Assign(out, p, (uint32_t)(end - p));
	}
	return out;
}

/* std::to_string */
MsvcString *MsvcString_ToString(MsvcString *out, int v) {
	MsvcString_FromInt(out, v);
	return out;
}

MsvcString *MsvcString_ToStringU64(MsvcString *out, uint64_t v) {
	MsvcString_FromUInt64(out, v);
	return out;
}

/* ---- the listeners ---- */

/* _Uninitialized_move of unique_ptrs: the pointers taken over, the old ones cleared */
void **PtrVector_UninitMove(void **first, void **last, void **dest) {
	uint32_t n = first <= last ? (uint32_t)((char *)last - (char *)first + 3) >> 2 : 0;

	for (uint32_t i = 0; i < n; i++, first++, dest++) {
		if (dest) {
			*dest = *first;
			*first = NULL;
		}
	}
	return dest;
}

/* the SelfRegisteringListener part: registered as an auth listener while the registrar is there */
static void GogListener_Register(GogAuthListener *l) {
	l->vftable = Game_Ptr(VT_SELFREGLISTENER);
	if (g_GalaxyListenerRegistrar()) {
		GalaxyIface *r = g_GalaxyListenerRegistrar();
		((void(__thiscall *)(GalaxyIface *, int, void *))r->vftable[1])(r, GALAXY_LISTENER_AUTH, l);
	}
}

static void GogListener_Unregister(GogAuthListener *l) {
	l->vftable = Game_Ptr(VT_SELFREGLISTENER);
	if (g_GalaxyListenerRegistrar()) {
		GalaxyIface *r = g_GalaxyListenerRegistrar();
		((void(__thiscall *)(GalaxyIface *, int, void *))r->vftable[2])(r, GALAXY_LISTENER_AUTH, l);
	}
	l->vftable = Game_Ptr(VT_IGALAXYLISTENER);
}

/* std::make_unique<AuthListener>(owner) */
GogAuthListener **GogAuthListener_MakeUnique(GogAuthListener **out, cGogListeners *owner) {
	GogAuthListener *l = operator_new(sizeof(*l));

	if (!l) {
		*out = NULL;
		return out;
	}
	GogListener_Register(l);
	l->owner = owner;
	l->vftable = Game_Ptr(VT_AUTHLISTENER);
	*out = l;
	return out;
}

GogAuthListener *__thiscall GogAuthListener_ctor(GogAuthListener *self, cGogListeners *owner) {
	GogListener_Register(self);
	self->owner = owner;
	self->vftable = Game_Ptr(VT_AUTHLISTENER);
	return self;
}

cGogListeners *__thiscall GogListeners_ctor(cGogListeners *self) {
	self->begin = NULL;
	self->end = NULL;
	self->cap = NULL;
	return self;
}

/* ~unique_ptr */
void __fastcall GogListenerPtr_dtor(void **p) {
	void **l = *p;

	if (l) {
		((void *(__thiscall *)(void *, byte))(*(void ***)l)[0])(l, 1);
	}
}

/* ~IGalaxyListener and its deleting destructor */
void __fastcall IGalaxyListener_dtor(void *self) {
	*(void **)self = Game_Ptr(VT_IGALAXYLISTENER);
}

void *__thiscall IGalaxyListener_Delete(void *self, byte flags) {
	*(void **)self = Game_Ptr(VT_IGALAXYLISTENER);
	if (flags & 1) {
		operator_delete_sized(self, 4);
	}
	return self;
}

void *__thiscall GogSelfRegListener_Delete(GogAuthListener *self, byte flags) {
	GogListener_Unregister(self);
	if (flags & 1) {
		operator_delete_sized(self, 4);
	}
	return self;
}

void *__thiscall GogAuthListener_Delete(GogAuthListener *self, byte flags) {
	GogListener_Unregister(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* _Reallocate to n: the unique_ptrs moved into a new block (the catch(...) of the exe frees it when a move throws,
 * a move of a pointer does not) */
void __thiscall PtrVector_Reallocate(cGogListeners *self, uint32_t n) {
	void **p = PtrVector_Allocate(self, n);

	PtrVector_UninitMove(self->begin, self->end, p);
	void **begin = self->begin, **end = self->end;
	uint32_t count = (uint32_t)(end - begin);
	if (begin) {
		for (void **q = begin; q != end; q++) {
			if (*q) {
				((void *(__thiscall *)(void *, byte))(*(void ***)*q)[0])(*q, 1);
			}
		}
		PtrVector_Deallocate(self, self->begin, (int)(self->cap - self->begin));
	}
	self->cap = p + n;
	self->end = p + count;
	self->begin = p;
}

/* _Reserve for count more */
void __thiscall PtrVector_Reserve(cGogListeners *self, uint32_t count) {
	if ((uint32_t)(self->cap - self->end) >= count) {
		return;
	}
	uint32_t size = (uint32_t)(self->end - self->begin);
	if (MSVC_MAX_PTRS - size < count) {
		crt_Xlength_error("vector<T> too long");
	}
	uint32_t want = size + count;
	uint32_t old = (uint32_t)(self->cap - self->begin);
	uint32_t grow = 0;
	if (MSVC_MAX_PTRS - (old >> 1) >= old) {
		grow = (old >> 1) + old;
	}
	if (grow >= want) {
		want = grow;
	}
	PtrVector_Reallocate(self, want);
}

/* a new auth listener at the end of the vector */
void __thiscall GogListeners_AddAuth(cGogListeners *self) {
	GogAuthListener *tmp;

	GogAuthListener_MakeUnique(&tmp, self);
	if (self->end == self->cap) {
		PtrVector_Reserve(self, 1);
	}
	if (self->end) {
		*self->end = tmp;
		tmp = NULL;
	}
	self->end++;
	if (tmp) {
		((void *(__thiscall *)(void *, byte))(*(void ***)tmp)[0])(tmp, 1);
	}
}

/* AuthListener::OnAuthSuccess (vf02) */
void __thiscall GogAuthListener_OnAuthSuccess(GogAuthListener *self, int arg) {
	(void)self;
	(void)arg;
	g_GogSignedIn = 1;
}

/* AuthListener::OnAuthLost (vf01): the stats of the user requested again */
void __thiscall GogAuthListener_OnAuthLost(GogAuthListener *self) {
	(void)self;
	GalaxyIface *st = g_GalaxyStats();
	((void(__thiscall *)(GalaxyIface *, uint32_t, uint32_t, void *))st->vftable[1])(st, 0, 0, NULL);
	g_GogSignedIn = 1;
}

/* ---- gog::GoGScrap ---- */

/* the object of an MSVC exception as the class of type, NULL when it is none (__AdjustPointer for the base) */
static void *Msvc_CatchAs(byte *obj, const MsvcThrowInfo *ti, const MsvcTypeDescriptor *type) {
	for (int i = 0; i < ti->catchable->n; i++) {
		const MsvcCatchableType *ct = ti->catchable->types[i];
		if (ct->type != type && strcmp(ct->type->name, type->name)) {
			continue;
		}
		byte *p = obj + ct->mdisp;
		if (ct->pdisp >= 0) {
			p += *(const int32_t *)(*(const byte **)(obj + ct->pdisp) + ct->vdisp) + ct->pdisp;
		}
		return p;
	}
	return NULL;
}

static void Seh_Push(SehCatchFrame *frame) {
	__asm__ volatile("movl %%fs:0, %0" : "=r"(frame->prev) : : "memory");
	__asm__ volatile("movl %0, %%fs:0" : : "r"(frame) : "memory");
}

static void Seh_Pop(SehCatchFrame *frame) {
	__asm__ volatile("movl %0, %%fs:0" : : "r"(frame->prev) : "memory");
}

/*
 * catch (const galaxy::api::IError &e) of Gog_Init: the frames of Galaxy.dll unwound, then the catch block (no
 * authorization, the message to the Galaxy log) while the exception object is still on the stack below, the object
 * destroyed, and on after the try in Gog_Init.
 */
static int __cdecl Gog_CatchError(SehRecord *rec, SehCatchFrame *frame, void *context, void *dispatch) {
	(void)context;
	(void)dispatch;
	if ((rec->flags & SEH_UNWINDING) || rec->code != MSVC_CXX_EXCEPTION || rec->nparams != 3
		|| rec->info[0] < MSVC_CXX_MAGIC1 || rec->info[0] > MSVC_CXX_MAGIC3) {
		return SEH_CONTINUE_SEARCH;
	}
	byte *obj = (byte *)rec->info[1];
	const MsvcThrowInfo *ti = (const MsvcThrowInfo *)rec->info[2];
	GalaxyIface *err = Msvc_CatchAs(obj, ti, GOG_IERROR_TYPE);
	if (!err) {
		return SEH_CONTINUE_SEARCH;
	}
	exe_RtlUnwind(frame, NULL, rec, NULL);
	g_GogSignedIn = 0;
	GalaxyIface *log = g_GalaxyLogger();
	const char *msg = ((const char *(__thiscall *)(GalaxyIface *))err->vftable[2])(err);
	((void(__cdecl *)(GalaxyIface *, const char *, ...))log->vftable[5])(log, GOG_AUTH_FAILED, msg);
	if (ti->unwind) {
		((void(__thiscall *)(void *))ti->unwind)(obj);
	}
	__builtin_longjmp(frame->resume, 1);
}

/* galaxy::api::Init, the sign in and the auth listener */
void __thiscall Gog_Init(cGogListeners *self) {
	SehCatchFrame frame;
	GalaxyInitOptions opts;

	opts.clientID = g_GogClientID;
	opts.clientSecret = g_GogClientSecret;
	opts.configFilePath = GOG_CONFIG_PATH;
	opts.storagePath = NULL;
	opts.galaxyAllocator = NULL;
	opts.galaxyThreadFactory = NULL;
	opts.host = NULL;
	opts.port = 0;
	frame.handler = (void *)Gog_CatchError;
	if (__builtin_setjmp(frame.resume)) {
		Seh_Pop(&frame);
		return;
	}
	Seh_Push(&frame);
	g_GalaxyInit(&opts);
	GalaxyIface *u = g_GalaxyUser();
	((void(__thiscall *)(GalaxyIface *, uint32_t, uint32_t))u->vftable[7])(u, 0, 0);
	GogListeners_AddAuth(self);
	Seh_Pop(&frame);
}

void __thiscall Gog_Done(cGogListeners *self) {
	(void)self;
}

const char *Gog_GetLanguage(void) {
	GalaxyIface *a = g_GalaxyApps();
	return ((const char *(__thiscall *)(GalaxyIface *, uint32_t, uint32_t))a->vftable[2])(a, 0, 0);
}

/* the GalaxyID in decimal; the pointer is into the temporary the original destroys before it returns (NULL when
 * not signed in or without an ID) */
const char *Gog_GetID(void) {
	uint32_t id[2];
	MsvcString num, str;

	if (!g_GogSignedIn) {
		return NULL;
	}
	GalaxyIface *u = g_GalaxyUser();
	const uint32_t *pid = ((const uint32_t *(__thiscall *)(GalaxyIface *, uint32_t *))u->vftable[2])(u, id);
	if (!(pid[0] | pid[1])) {
		return NULL;
	}
	u = g_GalaxyUser();
	pid = ((const uint32_t *(__thiscall *)(GalaxyIface *, uint32_t *))u->vftable[2])(u, id);
	MsvcString *s = MsvcString_PrependMove(&str, GOG_EMPTY,
		MsvcString_ToStringU64(&num, ((uint64_t)pid[1] << 32) | pid[0]));
	const char *r = Msvc_Ptr(s);
	if (str.cap >= MSVC_SSO) {
		MsvcAlloc_Deallocate(&str, str.ptr, str.cap + 1);
	}
	str.cap = 0xf;
	str.size = 0;
	str.buf[0] = 0;
	if (num.cap >= MSVC_SSO) {
		MsvcAlloc_Deallocate(&num, num.ptr, num.cap + 1);
	}
	return r;
}

/* "ACHIEVEMENT_n" set and stored, while signed in */
void Achievement_Unlock(int n) {
	MsvcString num, name;

	if (!g_GogSignedIn) {
		return;
	}
	GalaxyIface *st = g_GalaxyStats();
	MsvcString *s = MsvcString_PrependMove(&name, "ACHIEVEMENT_", MsvcString_ToString(&num, (int16_t)n));
	((void(__thiscall *)(GalaxyIface *, const char *))st->vftable[8])(st, Msvc_Ptr(s));
	if (name.cap >= MSVC_SSO) {
		MsvcAlloc_Deallocate(&name, name.ptr, name.cap + 1);
	}
	name.cap = 0xf;
	name.size = 0;
	name.buf[0] = 0;
	if (num.cap >= MSVC_SSO) {
		MsvcAlloc_Deallocate(&num, num.ptr, num.cap + 1);
	}
	num.cap = 0xf;
	num.size = 0;
	num.buf[0] = 0;
	st = g_GalaxyStats();
	((void(__thiscall *)(GalaxyIface *, void *))st->vftable[10])(st, NULL);
}

void __thiscall Gog_ProcessData(cGogListeners *self) {
	(void)self;
	g_GalaxyProcessData();
}
