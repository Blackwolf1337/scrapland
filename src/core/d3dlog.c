/*
 * The D3D log of SDTest (src/core/testmod.c): a proxy in place of the IDirect3DDevice8 of the engine (g_pd3dDevice
 * and the device of the D3D application) that writes each call into a text and then passes it on to the real device.
 * Draw calls come with their vertices, state calls with their values, as hex words; pointers to objects only as
 * whether they are set. Calls of other methods are written as their vtable slot.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"

#define D3DDEV_METHODS 97
#define STUB_SIZE 16

typedef void (*d3dlogOut_t)(const char *s);

void *d3dlog_real;
static void *d3dlog_vtbl[D3DDEV_METHODS];
static struct {
	void **vtbl;
} d3dlog_fake = { d3dlog_vtbl };
static d3dlogOut_t d3dlog_out;
static void *d3dlog_engine;		/* the device of the D3D application was the same, swapped too */

void D3DLog_Record(uint32_t slot, const uint32_t *a);

/* slot i: push i, then the common part, which logs the call and jumps to the real method with the real device as
 * this; the stack is that of the caller again, so the stdcall method pops the arguments */
__asm__(
	".text\n"
	".balign 16\n"
	"_d3dlog_stubs:\n"
	".set d3dlog_i, 0\n"
	".rept 97\n"
	".balign 16\n"
	"pushl $d3dlog_i\n"
	"jmp d3dlog_common\n"
	".set d3dlog_i, d3dlog_i + 1\n"
	".endr\n"
	"d3dlog_common:\n"
	"movl (%esp), %eax\n"
	"leal 12(%esp), %ecx\n"
	"pushl %ecx\n"
	"pushl %eax\n"
	"call _D3DLog_Record\n"
	"addl $8, %esp\n"
	"popl %eax\n"
	"movl _d3dlog_real, %ecx\n"
	"movl %ecx, 4(%esp)\n"
	"movl (%ecx), %edx\n"
	"jmp *(%edx,%eax,4)\n");
extern const byte d3dlog_stubs[] __asm__("_d3dlog_stubs");

static void Log_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void Log_Printf(const char *fmt, ...) {
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	d3dlog_out(buf);
}

static void Log_Words(const void *p, uint32_t n) {
	const uint32_t *w = p;
	for (uint32_t i = 0; i < n; i++) {
		Log_Printf(" %x", w[i]);
	}
}

/* the vertices of count primitives of type */
static uint32_t Prim_Vertices(uint32_t type, uint32_t count) {
	switch (type) {
	case 2:
		return 2 * count;
	case 3:
		return count + 1;
	case 4:
		return 3 * count;
	case 5:
	case 6:
		return count + 2;
	default:
		return count;
	}
}

void D3DLog_Record(uint32_t slot, const uint32_t *a) {
	switch (slot) {
	case D3DDEV_SETTRANSFORM / 4:
		Log_Printf("SetTransform(%u,", a[0]);
		Log_Words((const void *)(uintptr_t)a[1], 16);
		break;
	case D3DDEV_CLEAR / 4:
		Log_Printf("Clear(%u, %x, %x, %x, %u,", a[0], a[2], a[3], a[4], a[5]);
		if (a[1]) {
			Log_Words((const void *)(uintptr_t)a[1], 4 * a[0]);
		}
		break;
	case 40:
		Log_Printf("SetViewport(");
		Log_Words((const void *)(uintptr_t)a[0], 6);
		break;
	case D3DDEV_SETLIGHT / 4:
		Log_Printf("SetLight(%u,", a[0]);
		Log_Words((const void *)(uintptr_t)a[1], 26);
		break;
	case D3DDEV_LIGHTENABLE / 4:
		Log_Printf("LightEnable(%u, %u", a[0], a[1]);
		break;
	case D3DDEV_SETRENDERSTATE / 4:
		Log_Printf("SetRenderState(%u, %x", a[0], a[1]);
		break;
	case D3DDEV_GETRENDERSTATE / 4:
		Log_Printf("GetRenderState(%u", a[0]);
		break;
	case D3DDEV_SETGAMMARAMP / 4: {
		/* the 3 x 256 words as a hash and every 32nd word */
		const uint16_t *r = (const uint16_t *)(uintptr_t)a[1];
		uint32_t h = 2166136261u;
		for (int i = 0; i < 768; i++) {
			h = (h ^ r[i]) * 16777619u;
		}
		Log_Printf("SetGammaRamp(%u, %x,", a[0], h);
		for (int i = 0; i < 768; i += 32) {
			Log_Printf(" %x", r[i]);
		}
		break;
	}
	case 61:
		Log_Printf("SetTexture(%u, %d", a[0], a[1] != 0);
		break;
	case D3DDEV_SETTEXTURESTAGESTATE / 4:
		Log_Printf("SetTextureStageState(%u, %u, %x", a[0], a[1], a[2]);
		break;
	case 70:
		Log_Printf("DrawPrimitive(%u, %u, %u", a[0], a[1], a[2]);
		break;
	case D3DDEV_DRAWINDEXEDPRIMITIVE / 4:
		Log_Printf("DrawIndexedPrimitive(%u, %u, %u, %u, %u", a[0], a[1], a[2], a[3], a[4]);
		break;
	case D3DDEV_DRAWPRIMITIVEUP / 4:
		Log_Printf("DrawPrimitiveUP(%u, %u, %u,", a[0], a[1], a[3]);
		Log_Words((const void *)(uintptr_t)a[2], Prim_Vertices(a[0], a[1]) * a[3] / 4);
		break;
	case 73: {
		uint32_t ni = Prim_Vertices(a[0], a[3]);
		Log_Printf("DrawIndexedPrimitiveUP(%u, %u, %u, %u, %u, %u,", a[0], a[1], a[2], a[3], a[5], a[7]);
		Log_Words((const void *)(uintptr_t)a[4], (a[5] == D3DFMT_INDEX16 ? 2 : 4) * ni / 4);
		Log_Printf(" |");
		Log_Words((const byte *)(uintptr_t)a[6] + a[1] * a[7], a[2] * a[7] / 4);
		break;
	}
	case D3DDEV_SETPIXELSHADER / 4:
		Log_Printf("SetPixelShader(%x", a[0]);
		break;
	case D3DDEV_SETPIXELSHADERCONSTANT / 4:
		Log_Printf("SetPixelShaderConstant(%u,", a[0]);
		Log_Words((const void *)(uintptr_t)a[1], 4 * a[2]);
		break;
	case D3DDEV_SETMATERIAL / 4:
		Log_Printf("SetMaterial(");
		Log_Words((const void *)(uintptr_t)a[0], 17);
		break;
	case D3DDEV_SETVERTEXSHADER / 4:
		Log_Printf("SetVertexShader(%x", a[0]);
		break;
	case D3DDEV_SETSTREAMSOURCE / 4:
		Log_Printf("SetStreamSource(%u, %d, %u", a[0], a[1] != 0, a[2]);
		break;
	case D3DDEV_SETINDICES / 4:
		Log_Printf("SetIndices(%d, %u", a[0] != 0, a[1]);
		break;
	default:
		Log_Printf("#%u(", slot);
		break;
	}
	d3dlog_out(")\n");
}

/* the proxy in place with an output, the real device back with NULL; nothing without a device */
void D3DLog_Set(d3dlogOut_t out) {
	if (out && !d3dlog_out && g_pd3dDevice) {
		for (int i = 0; i < D3DDEV_METHODS; i++) {
			d3dlog_vtbl[i] = (void *)(d3dlog_stubs + STUB_SIZE * i);
		}
		d3dlog_real = g_pd3dDevice;
		d3dlog_engine = g_Engine->device == d3dlog_real ? &d3dlog_fake : NULL;
		d3dlog_out = out;
		g_pd3dDevice = &d3dlog_fake;
		if (d3dlog_engine) {
			g_Engine->device = &d3dlog_fake;
		}
	} else if (!out && d3dlog_out) {
		g_pd3dDevice = d3dlog_real;
		if (d3dlog_engine) {
			g_Engine->device = d3dlog_real;
		}
		d3dlog_out = NULL;
	}
}
