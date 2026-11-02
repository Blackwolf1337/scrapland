#!/bin/bash
# FX textures, particles, input, binary heap, sound interface, gvar functions (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
import re
def ed(p, a, b):
    s = open(p).read(); assert a in s, (p, a); s = s.replace(a, b, 1); open(p, 'w').write(s)
ed('include/scrap/fxtexture.h', """/* the vftable of IFXTexture: [0] the deleting destructor, [1] int Update(CMap *), [2] void *GetTexture() */
typedef struct IFXTexture {
	void **vftable;			/* 0x0 */
} IFXTexture;""", """typedef struct IFXTexture IFXTexture;

typedef struct IFXTextureVtbl {
	IFXTexture *(*Delete)(IFXTexture *self, byte flags);	/* 0x0 */
	int(*Update)(IFXTexture *self, CMap *map);		/* 0x4 */
	void *(*GetTexture)(IFXTexture *self);			/* 0x8 */
} IFXTextureVtbl;

struct IFXTexture {
	const IFXTextureVtbl *vftable;	/* 0x0 */
};""")
ed('include/scrap/particlesystem.h', """/* the slots of the vftable of the particles */
enum { PARTICLE_VF_TYPE = 0, PARTICLE_VF_INIT = 1, PARTICLE_VF_UPDATE = 2 };
enum { PARTICLE_STATIC = 1, PARTICLE_ACTIVE = 2 };

typedef struct CParticle {
	void **vftable;			/* 0x00 PARTICULA_VF_* */""", """enum { PARTICLE_STATIC = 1, PARTICLE_ACTIVE = 2 };

typedef struct CParticle CParticle;
struct CParticleSystem;

typedef struct CParticleVtbl {
	int(*Type)(CParticle *self);								/* 0x0 PARTICLE_ */
	void(*Init)(CParticle *self);								/* 0x4 */
	void(*Update)(CParticle *self, struct CParticleSystem *sp, float dt, void *multiSpr);	/* 0x8 */
} CParticleVtbl;

struct CParticle {
	const CParticleVtbl *vftable;	/* 0x00 */""")
ed('include/scrap/particlesystem.h', """	float uv1[2];			/* 0x2c */
} CParticle;
SD_SIZE(CParticle, 0x34);
typedef int(*partType_t)(CParticle *self);
#define PARTICLE_TYPE(p) (((partType_t)(p)->vftable[PARTICLE_VF_TYPE])(p))""", """	float uv1[2];			/* 0x2c */
};
SD_SIZE(CParticle, 0x34);
#define PARTICLE_TYPE(p) ((p)->vftable->Type(p))""")
ed('include/scrap/e3dinput.h', """/* the vftable of IInput */
enum {
	INPUT_VF_DELETE = 0,		/* (byte flags) */
	INPUT_VF_INIT = 1,		/* int (void *hwnd, void *hinst, byte exclusive) */
	INPUT_VF_RELEASE = 2,		/* int () */
	INPUT_VF_READMOUSE = 3,	/* int (int *out) dx, dy, dz and the buttons */
	INPUT_VF_READKEYS = 4,	/* int () */
	INPUT_VF_ACQUIREMOUSE = 5,	/* int (byte on) */
	INPUT_VF_ACQUIREKEYS = 6,	/* int (byte on) */
	INPUT_VF_WNDPROC = 7,		/* (void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) */
};
""", """typedef struct IInput IInput;

/* the slots of IInput; only the null input is made (the platform layer reads the devices) */
typedef struct IInputVtbl {
	IInput *(*Delete)(IInput *self, byte flags);						/* 0x00 */
	int(*Init)(IInput *self, void *hwnd, void *hinst, byte exclusive);			/* 0x04 */
	int(*Release)(IInput *self);								/* 0x08 */
	int(*ReadMouse)(IInput *self, int *out);						/* 0x0c dx, dy, dz and the buttons */
	int(*ReadKeys)(IInput *self);								/* 0x10 */
	int(*AcquireMouse)(IInput *self, byte on);						/* 0x14 */
	int(*AcquireKeys)(IInput *self, byte on);						/* 0x18 */
	void(*WndProc)(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);	/* 0x1c */
} IInputVtbl;
""")
ed('include/scrap/e3dinput.h', 'typedef struct IInput {\n\tvoid **vftable;\t\t\t/* 0x000 */', 'struct IInput {\n\tconst IInputVtbl *vftable;\t/* 0x000 */')
s = open('include/scrap/e3dinput.h').read()
s = re.sub(r'(struct IInput \{.*?\n)\} IInput;', r'\1};', s, count=1, flags=re.S); open('include/scrap/e3dinput.h', 'w').write(s)
ed('include/scrap/sound.h', """typedef struct cEntity cEntity;
typedef struct cSndInterface {
	const void *vftable;		/* 0x00 */""", """typedef struct cEntity cEntity;
typedef struct cSndInterface cSndInterface;

/* the deleting destructor and the variables of the sounds */
typedef struct cSndInterfaceVtbl {
	cSndInterface *(*Delete)(cSndInterface *self, byte flags);					/* 0x00 */
	byte(*VarI)(cSndInterface *self, const char *name, uint32_t *v, byte set);			/* 0x04 */
	byte(*VarF)(cSndInterface *self, const char *name, double *v, byte set);			/* 0x08 */
	byte(*VarS)(cSndInterface *self, const char *name, const char **v, byte set);			/* 0x0c */
	byte(*VarV)(cSndInterface *self, const char *name, double *x, double *y, double *z, byte set);	/* 0x10 */
} cSndInterfaceVtbl;

struct cSndInterface {
	const cSndInterfaceVtbl *vftable;	/* 0x00 */""")
s = open('include/scrap/sound.h').read()
s = re.sub(r'(struct cSndInterface \{.*?\n)\} cSndInterface;', r'\1};', s, count=1, flags=re.S); open('include/scrap/sound.h', 'w').write(s)
ed('src/game/engine3d/Engine/ParticleSystem.c', '#define PARTICLE_UPDATE(p, sp, dt, ms) (((partUpdate_t)(p)->vftable[PARTICLE_VF_UPDATE])((p), (sp), (dt), (ms)))', '#define PARTICLE_UPDATE(p, sp, dt, ms) ((p)->vftable->Update((p), (sp), (dt), (ms)))')
PY
e() { python3 tools/cleanup/vtgen.py emit "$@" || exit 1; }
e IFXTextureVtbl IFXTexture_vftable CFXTexLava_vftable CFXTexScroll_vftable CFXNewsPanel_vftable
e CParticleVtbl CParticleBase_vftable@src/game/engine3d/Engine/ParticleSystem.c CParticleStatic_vftable CParticleActive_vftable
e IInputVtbl IInput_vftable CNullInput_vftable
e cBinaryHeapVtbl cBinaryHeap_ElemAStar_vftable
e cSndInterfaceVtbl cSndInterface_vftable
e cGVarListVtbl cFuncGvar_vftable@src/game/Input/IDevice.c
python3 tools/cleanup/vtgen.py stubs $(grep -rlE "const (IFXTexture|CParticle|IInput|cBinaryHeap|cSndInterface|cGVarList)Vtbl \w+_vftable = \{" src)
