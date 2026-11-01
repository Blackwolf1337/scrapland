/*
 * The draws of the Vulkan backend, see vkdraw.h. The render pass is dynamic rendering on the render target and depth
 * buffer of the device; it starts with the first clear or draw and ends before transfers, a change of targets and
 * the present.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vkdraw.h"

static VkoDev *vkd_dev;
static struct {
	byte active;
	VkoSurf *rt, *zs;
} vkd_pass;

int Vkd_Init(VkoDev *d) {
	vkd_dev = d;
	memset(&vkd_pass, 0, sizeof(vkd_pass));
	return 1;
}

void Vkd_Shutdown(void) {
	Vkd_End();
	vkd_dev = NULL;
}

VkoDev *Vkd_Device(void) {
	return vkd_dev;
}

void Vkd_End(void) {
	if (vkd_pass.active) {
		vkCmdEndRendering(Vk_Cmd());
		vkd_pass.active = 0;
	}
}

/* the render pass on the targets of d, kept while they stay */
static VkCommandBuffer Vkd_Begin(VkoDev *d) {
	VkCommandBuffer cmd = Vk_Cmd();

	if (vkd_pass.active && vkd_pass.rt == d->rt && vkd_pass.zs == d->zs) {
		return cmd;
	}
	Vkd_End();
	VkoSurf *rt = d->rt, *zs = d->zs;
	Vk_Transition(cmd, rt->img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo color = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = rt->img->view,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	VkRenderingAttachmentInfo depth = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	if (zs) {
		Vk_Transition(cmd, zs->img, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		depth.imageView = zs->img->view;
	}
	VkRenderingInfo ri = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = { { 0, 0 }, { rt->width, rt->height } },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color,
		.pDepthAttachment = zs ? &depth : NULL,
		.pStencilAttachment = zs ? &depth : NULL,
	};
	vkCmdBeginRendering(cmd, &ri);
	vkd_pass.active = 1;
	vkd_pass.rt = rt;
	vkd_pass.zs = zs;
	return cmd;
}

/* the rectangles (or the viewport) of the targets, as Direct3D 8 clears them */
int Vkd_Clear(VkoDev *d, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
	VkClearAttachment att[2];
	uint32_t na = 0;

	if (flags & 1) {
		att[na++] = (VkClearAttachment) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.clearValue.color.float32 = { (color >> 16 & 255) / 255.0f, (color >> 8 & 255) / 255.0f,
				(color & 255) / 255.0f, (color >> 24) / 255.0f },
		};
	}
	if ((flags & 6) && d->zs) {
		att[na++] = (VkClearAttachment) {
			.aspectMask = (flags & 2 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (flags & 4 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0),
			.clearValue.depthStencil = { z, stencil },
		};
	}
	if (!na) {
		return 1;
	}
	VkCommandBuffer cmd = Vkd_Begin(d);
	int32_t vx = (int32_t)d->viewport[0], vy = (int32_t)d->viewport[1];
	int32_t vr = vx + (int32_t)d->viewport[2], vb = vy + (int32_t)d->viewport[3];
	if (vr > (int32_t)d->rt->width) {
		vr = (int32_t)d->rt->width;
	}
	if (vb > (int32_t)d->rt->height) {
		vb = (int32_t)d->rt->height;
	}
	VkClearRect cr[16];
	uint32_t n = 0;
	if (!count || !rects) {
		count = 1;
		rects = NULL;
	}
	for (uint32_t i = 0; i < count && n < 16; i++) {
		int32_t l = vx, t = vy, r = vr, b = vb;
		if (rects) {
			const int32_t *q = (const int32_t *)rects + 4 * i;
			l = q[0] > l ? q[0] : l;
			t = q[1] > t ? q[1] : t;
			r = q[2] < r ? q[2] : r;
			b = q[3] < b ? q[3] : b;
		}
		if (r > l && b > t) {
			cr[n++] = (VkClearRect) { { { l, t }, { (uint32_t)(r - l), (uint32_t)(b - t) } }, 0, 1 };
		}
	}
	if (n) {
		vkCmdClearAttachments(cmd, na, att, n, cr);
	}
	return 1;
}

int Vkd_Draw(VkoDev *d, int type, uint32_t primCount, uint32_t start, const void *data, uint32_t stride,
	const void *idx, int idxFmt, int indexed) {
	(void)d, (void)type, (void)primCount, (void)start, (void)data, (void)stride, (void)idx, (void)idxFmt, (void)indexed;
	return 0;
}

int Vkd_DrawIndexedUP(VkoDev *d, int type, uint32_t primCount, uint32_t numVertices, const void *idx, int idxFmt,
	const void *data, uint32_t stride) {
	(void)d, (void)type, (void)primCount, (void)numVertices, (void)idx, (void)idxFmt, (void)data, (void)stride;
	return 0;
}

uint32_t Vkd_CreatePixelShader(const void *code) {
	static uint32_t next;
	(void)code;
	return ++next;
}

/* the back buffer as a 32 bit BMP, bottom up */
static void Vkd_WriteBmp(const char *path, const byte *bgra, uint32_t w, uint32_t h) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		return;
	}
	uint32_t size = w * h * 4;
	byte hdr[54] = { 'B', 'M' };
	uint32_t v[] = { 54 + size, 0, 54, 40, w, h };
	memcpy(hdr + 2, &v[0], 4);
	memcpy(hdr + 10, &v[2], 4);
	memcpy(hdr + 14, &v[3], 4);
	memcpy(hdr + 18, &v[4], 4);
	memcpy(hdr + 22, &v[5], 4);
	hdr[26] = 1;
	hdr[28] = 32;
	fwrite(hdr, 1, 54, f);
	for (uint32_t y = h; y-- > 0;) {
		fwrite(bgra + (size_t)y * w * 4, 1, (size_t)w * 4, f);
	}
	fclose(f);
}

void Vkd_DumpFrame(VkImg *back) {
	static uint32_t frame;
	const char *dir = getenv("SCRAP_VK_DUMP");
	const char *every = getenv("SCRAP_VK_DUMP_EVERY");
	uint32_t n = every ? (uint32_t)atoi(every) : 100;

	frame++;
	if (!dir || !n || frame % n) {
		return;
	}
	VkBuf tmp;
	if (!Vk_CreateBuffer(&tmp, (VkDeviceSize)back->width * back->height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1)) {
		return;
	}
	VkCommandBuffer cmd = Vk_Cmd();
	VkImageLayout keep = back->layout;
	Vk_Transition(cmd, back, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VkBufferImageCopy copy = {
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageExtent = { back->width, back->height, 1 },
	};
	vkCmdCopyImageToBuffer(cmd, back->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tmp.buf, 1, &copy);
	Vk_Transition(cmd, back, keep == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : keep);
	Vk_Flush();
	char path[512];
	snprintf(path, sizeof(path), "%s/frame%05u.bmp", dir, frame);
	Vkd_WriteBmp(path, tmp.map, back->width, back->height);
	Vk_DestroyBuffer(&tmp);
}
