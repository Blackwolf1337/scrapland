/*
 * The draws of the Vulkan backend (VkDraw.c): the render pass on the targets of the device, clears, and the draws with
 * the fixed function and pixel shader state of Direct3D 8.
 */
#ifndef VKDRAW_H
#define VKDRAW_H

#include "vkobj.h"

int Vkd_Init(VkoDev *d);
void Vkd_Shutdown(void);
/* the device of Vkd_Init */
VkoDev *Vkd_Device(void);
/* ends the render pass, before a transfer or a change of targets */
void Vkd_End(void);
int Vkd_Clear(VkoDev *d, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z, uint32_t stencil);
/* type: D3DPRIMITIVETYPE; data with stride: the vertices of an UP draw, else the stream source; indexed: from the
 * index buffer, start the first index */
int Vkd_Draw(VkoDev *d, int type, uint32_t primCount, uint32_t start, const void *data, uint32_t stride,
	const void *idx, int idxFmt, int indexed);
int Vkd_DrawIndexedUP(VkoDev *d, int type, uint32_t primCount, uint32_t numVertices, const void *idx, int idxFmt,
	const void *data, uint32_t stride);
/* the end of a frame, for SCRAP_VK_TRACE */
void Vkd_FrameDone(void);
/* this frame is the one of SCRAP_VK_TRACE: the calls go into the log */
int Vkd_Tracing(void);
/* a handle for the ps.1.1 tokens (the GLSL port of that shader) */
uint32_t Vkd_CreatePixelShader(const void *code);

#endif
