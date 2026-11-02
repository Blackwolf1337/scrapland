/*
 * The draws of the Vulkan backend, see vkdraw.h. The render pass is dynamic rendering on the render target and depth
 * buffer of the device; it starts with the first clear or draw and ends before transfers, a change of targets and
 * the present. A draw puts the fixed function state of the device into a uniform block of the frame ring (the
 * layout of shaders/ff.glsl), pushes it with the textures as descriptors, and takes a pipeline for its vertex format,
 * blend state and targets; depth, stencil, culling and the topology are dynamic state.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vkdraw.h"

static const uint32_t vkd_ffVert[] = {
#include "ff_vert.inc"
};
static const uint32_t vkd_ffFrag[] = {
#include "ff_frag.inc"
};

static const uint32_t vkd_presentVert[] = {
#include "present_vert.inc"
};
static const uint32_t vkd_presentFrag[] = {
#include "present_frag.inc"
};

typedef struct VkdPsCode {
	const char *name;
	const uint32_t *code;
	size_t size;
} VkdPsCode;

#include "ps_all.inc"

#define VKD_NUM_PS (sizeof(vkd_psCode) / sizeof(vkd_psCode[0]))

/* files of the game with the same shader as another */
static const char *const vkd_psAlias[][2] = { { "fire", "electric" } };

static VkShaderModule vkd_psModule[VKD_NUM_PS];
/* the handles of CreatePixelShader: each call one of its own, holding the index of its GLSL port */
#define VKD_MAX_PS_HANDLES 128
static uint32_t vkd_psHandle[VKD_MAX_PS_HANDLES];
static uint32_t vkd_numPsHandles;

/* the uniform block of shaders/ff.glsl, std140 */
typedef struct VkdFF {
	float world[16], view[16], proj[16], texm[4][16];
	float viewport[4], matDiffuse[4], matAmbient[4], matSpecular[4], matEmissive[4], misc[4], ambient[4];
	float fogColor[4], fogParams[4], tfactor[4];
	int32_t flags[4], matSource[4], lightCount[4], texDim[4];
	float lightDiffuse[8][4], lightSpecular[8][4], lightAmbient[8][4], lightPos[8][4], lightDir[8][4];
	float lightAtt[8][4], lightSpot[8][4];
	int32_t stage[8][4], stageTex[8][4];
	float bumpMat[8][4], psConst[8][4];
} VkdFF;

enum {
	FL_RHW = 1, FL_NORMAL = 2, FL_DIFFUSE = 4, FL_SPECULAR = 8, FL_LIGHTING = 16, FL_SPECULARENABLE = 32,
	FL_COLORVERTEX = 64, FL_LOCALVIEWER = 128, FL_NORMALIZE = 256, FL_FOG = 512, FL_RANGEFOG = 1024,
};

#define VKD_TEXTURES 4
#define VKD_MAX_PIPES 512
#define VKD_MAX_SAMPLERS 64
#define VKD_UNIFORM_ALIGN 256		/* the largest minUniformBufferOffsetAlignment Vulkan allows */

typedef struct VkdPipeKey {
	uint32_t fvf, stride, topo, blend, writeMask, fill, ps, depth;
} VkdPipeKey;

static VkoDev *vkd_dev;
static struct {
	byte active;
	VkoSurf *rt, *zs;
} vkd_pass;
static VkDescriptorSetLayout vkd_setLayout;
static VkPipelineLayout vkd_layout;
static VkShaderModule vkd_vert, vkd_frag;
static struct {
	VkdPipeKey key;
	VkPipeline pipe;
} vkd_pipes[VKD_MAX_PIPES];
static int vkd_numPipes;
static struct {
	uint64_t key;
	VkSampler s;
} vkd_samplers[VKD_MAX_SAMPLERS];
static int vkd_numSamplers;
static VkImg vkd_white;			/* the texture of a stage without one */
static VkImg vkd_feedback;		/* the copy of the render target for a draw that samples it */
static VkBuf vkd_defaults;		/* the attributes a vertex format lacks, read with stride 0 */
/* the present pass: the back buffer through the gamma ramp into the swapchain image */
static struct {
	VkDescriptorSetLayout setLayout;
	VkPipelineLayout layout;
	VkShaderModule vert, frag;
	VkPipeline pipe;
	VkFormat format;		/* the swapchain format of pipe */
	VkSampler linear, nearest;
	VkImg ramp;
	uint16_t rampData[3][256];	/* what ramp holds */
} vkd_pr;
static int vkd_traceState;		/* 0 not yet, 1 this frame, 2 done */
static DWORD vkd_traceAt;

/* ---- set up ---- */

static VkShaderModule Vkd_Module(const uint32_t *code, size_t size) {
	VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = code };
	VkShaderModule m = VK_NULL_HANDLE;
	vkCreateShaderModule(vk.device, &ci, NULL, &m);
	return m;
}

static int Vkd_InitPresent(void) {
	VkDescriptorSetLayoutBinding b[2] = {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
	};
	VkDescriptorSetLayoutCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = 2,
		.pBindings = b,
	};
	VkPipelineLayoutCreateInfo lci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vkd_pr.setLayout,
	};
	VkSamplerCreateInfo si = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	};
	if (vkCreateDescriptorSetLayout(vk.device, &sci, NULL, &vkd_pr.setLayout) < 0
		|| vkCreatePipelineLayout(vk.device, &lci, NULL, &vkd_pr.layout) < 0
		|| vkCreateSampler(vk.device, &si, NULL, &vkd_pr.linear) < 0) {
		return 0;
	}
	si.magFilter = si.minFilter = VK_FILTER_NEAREST;
	if (vkCreateSampler(vk.device, &si, NULL, &vkd_pr.nearest) < 0) {
		return 0;
	}
	vkd_pr.vert = Vkd_Module(vkd_presentVert, sizeof(vkd_presentVert));
	vkd_pr.frag = Vkd_Module(vkd_presentFrag, sizeof(vkd_presentFrag));
	VkComponentMapping id = { 0 };
	memset(vkd_pr.rampData, 0, sizeof(vkd_pr.rampData));	/* never a ramp of the game: the first present uploads */
	return vkd_pr.vert && vkd_pr.frag && Vk_CreateImage(&vkd_pr.ramp, VK_FORMAT_R16G16B16A16_UNORM, 256, 1, 1,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, id);
}

static void Vkd_ShutdownPresent(void) {
	vkDestroyPipeline(vk.device, vkd_pr.pipe, NULL);
	vkDestroySampler(vk.device, vkd_pr.linear, NULL);
	vkDestroySampler(vk.device, vkd_pr.nearest, NULL);
	vkDestroyShaderModule(vk.device, vkd_pr.vert, NULL);
	vkDestroyShaderModule(vk.device, vkd_pr.frag, NULL);
	vkDestroyPipelineLayout(vk.device, vkd_pr.layout, NULL);
	vkDestroyDescriptorSetLayout(vk.device, vkd_pr.setLayout, NULL);
	Vk_DestroyImage(&vkd_pr.ramp);
	memset(&vkd_pr, 0, sizeof(vkd_pr));
}

int Vkd_Init(VkoDev *d) {
	vkd_dev = d;
	memset(&vkd_pass, 0, sizeof(vkd_pass));
	vk.endPass = Vkd_End;
	const char *trace = getenv("SCRAP_VK_TRACE");
	if (trace && !vkd_traceAt) {
		vkd_traceAt = GetTickCount() + (DWORD)(atof(trace) * 1000.0);
	}
	if (vkd_layout) {
		return 1;
	}
	VkDescriptorSetLayoutBinding b[2] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VKD_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
	};
	VkDescriptorSetLayoutCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = 2,
		.pBindings = b,
	};
	VkPipelineLayoutCreateInfo lci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vkd_setLayout,
	};
	if (vkCreateDescriptorSetLayout(vk.device, &sci, NULL, &vkd_setLayout) < 0
		|| vkCreatePipelineLayout(vk.device, &lci, NULL, &vkd_layout) < 0) {
		return 0;
	}
	vkd_vert = Vkd_Module(vkd_ffVert, sizeof(vkd_ffVert));
	vkd_frag = Vkd_Module(vkd_ffFrag, sizeof(vkd_ffFrag));
	VkComponentMapping id = { 0 };
	if (!vkd_vert || !vkd_frag || !Vk_CreateImage(&vkd_white, VK_FORMAT_B8G8R8A8_UNORM, 1, 1, 1,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, id)
		|| !Vk_CreateBuffer(&vkd_defaults, 64, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1)) {
		return 0;
	}
	/* 0: (0, 0, 0, 1) for missing coordinates, 16: a zero normal, 32: white diffuse, 36: black specular */
	static const float tc[4] = { 0, 0, 0, 1 };
	memset(vkd_defaults.map, 0, 64);
	memcpy(vkd_defaults.map, tc, 16);
	memset(vkd_defaults.map + 32, 0xff, 4);
	VkCommandBuffer cmd = Vk_Cmd();
	Vk_Transition(cmd, &vkd_white, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkClearColorValue white = { .float32 = { 1, 1, 1, 1 } };
	VkImageSubresourceRange all = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdClearColorImage(cmd, vkd_white.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &white, 1, &all);
	Vk_Transition(cmd, &vkd_white, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return Vkd_InitPresent();
}

void Vkd_Shutdown(void) {
	Vkd_End();
	Vk_Flush();
	for (int i = 0; i < vkd_numPipes; i++) {
		vkDestroyPipeline(vk.device, vkd_pipes[i].pipe, NULL);
	}
	for (int i = 0; i < vkd_numSamplers; i++) {
		vkDestroySampler(vk.device, vkd_samplers[i].s, NULL);
	}
	vkd_numPipes = vkd_numSamplers = 0;
	Vk_DestroyImage(&vkd_white);
	Vk_DestroyImage(&vkd_feedback);
	Vk_DestroyBuffer(&vkd_defaults);
	Vkd_ShutdownPresent();
	vkDestroyShaderModule(vk.device, vkd_vert, NULL);
	vkDestroyShaderModule(vk.device, vkd_frag, NULL);
	for (size_t i = 0; i < VKD_NUM_PS; i++) {
		vkDestroyShaderModule(vk.device, vkd_psModule[i], NULL);
		vkd_psModule[i] = VK_NULL_HANDLE;
	}
	vkDestroyPipelineLayout(vk.device, vkd_layout, NULL);
	vkDestroyDescriptorSetLayout(vk.device, vkd_setLayout, NULL);
	vkd_layout = VK_NULL_HANDLE;
	vkd_setLayout = VK_NULL_HANDLE;
	vkd_dev = NULL;
	vk.endPass = NULL;
}

VkoDev *Vkd_Device(void) {
	return vkd_dev;
}

/* ---- the render pass ---- */

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
		.pStencilAttachment = zs && Vk_HasStencil(zs->img->format) ? &depth : NULL,
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
	VkImageAspectFlags zmask = !d->zs ? 0 : (flags & 2 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
		| (flags & 4 && Vk_HasStencil(d->zs->img->format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
	if (zmask) {
		att[na++] = (VkClearAttachment) {
			.aspectMask = zmask,
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

/* ---- vertex formats ---- */

typedef struct VkdLayout {
	VkVertexInputAttributeDescription attr[8];
	uint32_t texDim[4];
	uint32_t flags;			/* FL_RHW, FL_NORMAL, FL_DIFFUSE, FL_SPECULAR */
} VkdLayout;

static const VkFormat vkd_floats[5] = { VK_FORMAT_UNDEFINED, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };

/* the attributes of an FVF at their offsets in binding 0, the ones it lacks from the defaults in binding 1 */
static void Vkd_Fvf(uint32_t fvf, VkdLayout *l) {
	uint32_t off = 0;
	uint32_t pos = fvf & 0xe;

	memset(l, 0, sizeof(*l));
	for (uint32_t i = 0; i < 8; i++) {
		l->attr[i] = (VkVertexInputAttributeDescription) { i, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 };
	}
	l->attr[1] = (VkVertexInputAttributeDescription) { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 16 };
	l->attr[2] = (VkVertexInputAttributeDescription) { 2, 1, VK_FORMAT_B8G8R8A8_UNORM, 32 };
	l->attr[3] = (VkVertexInputAttributeDescription) { 3, 1, VK_FORMAT_B8G8R8A8_UNORM, 36 };
	if (pos == 4) {
		l->attr[0] = (VkVertexInputAttributeDescription) { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 };
		l->flags |= FL_RHW;
		off = 16;
	} else if (pos) {
		l->attr[0] = (VkVertexInputAttributeDescription) { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
		off = 12 + (pos > 4 ? (pos - 4) / 2 * 4 : 0);	/* XYZB1..5: the blend weights after xyz */
	}
	if (fvf & 0x10) {
		l->attr[1] = (VkVertexInputAttributeDescription) { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, off };
		l->flags |= FL_NORMAL;
		off += 12;
	}
	if (fvf & 0x20) {
		off += 4;			/* point size */
	}
	if (fvf & 0x40) {
		l->attr[2] = (VkVertexInputAttributeDescription) { 2, 0, VK_FORMAT_B8G8R8A8_UNORM, off };
		l->flags |= FL_DIFFUSE;
		off += 4;
	}
	if (fvf & 0x80) {
		l->attr[3] = (VkVertexInputAttributeDescription) { 3, 0, VK_FORMAT_B8G8R8A8_UNORM, off };
		l->flags |= FL_SPECULAR;
		off += 4;
	}
	uint32_t ntex = fvf >> 8 & 15;
	for (uint32_t i = 0; i < ntex && i < 8; i++) {
		static const uint32_t dims[4] = { 2, 3, 4, 1 };
		uint32_t n = dims[fvf >> (16 + 2 * i) & 3];
		if (i < 4) {
			l->attr[4 + i] = (VkVertexInputAttributeDescription) { 4 + i, 0, vkd_floats[n], off };
			l->texDim[i] = n;
		}
		off += 4 * n;
	}
}

/* ---- pipelines ---- */

static const VkBlendFactor vkd_blend[14] = {
	VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_DST_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_SRC_ALPHA,
	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
};

static VkBlendFactor Vkd_BlendFactor(uint32_t f) {
	return f < 14 ? vkd_blend[f] : VK_BLEND_FACTOR_ONE;
}

static VkPipeline Vkd_Pipeline(const VkdPipeKey *k) {
	for (int i = 0; i < vkd_numPipes; i++) {
		if (!memcmp(&vkd_pipes[i].key, k, sizeof(*k))) {
			return vkd_pipes[i].pipe;
		}
	}
	if (vkd_numPipes == VKD_MAX_PIPES) {
		Com_Printf("vulkan: too many pipelines\n");
		return VK_NULL_HANDLE;
	}
	VkdLayout l;
	Vkd_Fvf(k->fvf, &l);
	VkVertexInputBindingDescription binds[2] = {
		{ 0, k->stride, VK_VERTEX_INPUT_RATE_VERTEX },
		{ 1, 0, VK_VERTEX_INPUT_RATE_VERTEX },
	};
	VkPipelineVertexInputStateCreateInfo vi = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 2,
		.pVertexBindingDescriptions = binds,
		.vertexAttributeDescriptionCount = 8,
		.pVertexAttributeDescriptions = l.attr,
	};
	static const VkPrimitiveTopology topo[3] = { VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	VkPipelineInputAssemblyStateCreateInfo ia = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = topo[k->topo],
	};
	VkPipelineViewportStateCreateInfo vp = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	VkPipelineRasterizationStateCreateInfo rs = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = k->fill == 2 ? VK_POLYGON_MODE_LINE : k->fill == 1 ? VK_POLYGON_MODE_POINT : VK_POLYGON_MODE_FILL,
		.lineWidth = 1.0f,
	};
	VkPipelineMultisampleStateCreateInfo ms = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	VkPipelineDepthStencilStateCreateInfo ds = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	static const VkBlendOp ops[6] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT,
		VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
	uint32_t src = k->blend >> 1 & 31, dst = k->blend >> 6 & 31, op = k->blend >> 11 & 7;
	if (src == 12) {
		src = 5, dst = 6;		/* BOTHSRCALPHA */
	} else if (src == 13) {
		src = 6, dst = 5;		/* BOTHINVSRCALPHA */
	}
	VkPipelineColorBlendAttachmentState cba = {
		.blendEnable = k->blend & 1,
		.srcColorBlendFactor = Vkd_BlendFactor(src),
		.dstColorBlendFactor = Vkd_BlendFactor(dst),
		.colorBlendOp = ops[op < 6 ? op : 1],
		.srcAlphaBlendFactor = Vkd_BlendFactor(src),
		.dstAlphaBlendFactor = Vkd_BlendFactor(dst),
		.alphaBlendOp = ops[op < 6 ? op : 1],
		.colorWriteMask = k->writeMask,
	};
	VkPipelineColorBlendStateCreateInfo cb = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &cba,
	};
	static const VkDynamicState dyn[] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE,
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
		VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE, VK_DYNAMIC_STATE_DEPTH_BIAS,
		VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE, VK_DYNAMIC_STATE_STENCIL_OP, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE,
	};
	VkPipelineDynamicStateCreateInfo dy = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = sizeof(dyn) / sizeof(dyn[0]),
		.pDynamicStates = dyn,
	};
	VkPipelineShaderStageCreateInfo st[2] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vkd_vert, .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = vkd_frag, .pName = "main" },
	};
	if (k->ps) {
		VkShaderModule *m = &vkd_psModule[k->ps - 1];
		if (!*m) {
			*m = Vkd_Module(vkd_psCode[k->ps - 1].code, vkd_psCode[k->ps - 1].size);
		}
		if (!*m) {
			return VK_NULL_HANDLE;
		}
		st[1].module = *m;
	}
	VkFormat color = VK_FORMAT_B8G8R8A8_UNORM;
	VkPipelineRenderingCreateInfo pr = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &color,
		.depthAttachmentFormat = (VkFormat)k->depth,
		.stencilAttachmentFormat = Vk_HasStencil((VkFormat)k->depth) ? (VkFormat)k->depth : VK_FORMAT_UNDEFINED,
	};
	VkGraphicsPipelineCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &pr,
		.stageCount = 2,
		.pStages = st,
		.pVertexInputState = &vi,
		.pInputAssemblyState = &ia,
		.pViewportState = &vp,
		.pRasterizationState = &rs,
		.pMultisampleState = &ms,
		.pDepthStencilState = &ds,
		.pColorBlendState = &cb,
		.pDynamicState = &dy,
		.layout = vkd_layout,
	};
	VkPipeline p = VK_NULL_HANDLE;
	if (vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &ci, NULL, &p) < 0) {
		Com_Printf("vulkan: pipeline for fvf %x failed\n", k->fvf);
		return VK_NULL_HANDLE;
	}
	vkd_pipes[vkd_numPipes].key = *k;
	vkd_pipes[vkd_numPipes].pipe = p;
	vkd_numPipes++;
	return p;
}

/* ---- samplers ---- */

static VkSamplerAddressMode Vkd_Address(uint32_t a) {
	switch (a) {
	case 2:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case 3:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case 4:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case 5:
		return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	default:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

static VkSampler Vkd_Sampler(const uint32_t *tss) {
	uint32_t aniso = tss[21] > 16 ? 16 : tss[21];
	uint64_t key = (uint64_t)tss[13] | (uint64_t)tss[14] << 4 | (uint64_t)tss[16] << 8 | (uint64_t)tss[17] << 12
		| (uint64_t)tss[18] << 16 | (uint64_t)aniso << 20 | (uint64_t)tss[19] << 32;
	for (int i = 0; i < vkd_numSamplers; i++) {
		if (vkd_samplers[i].key == key) {
			return vkd_samplers[i].s;
		}
	}
	float bias;
	memcpy(&bias, &tss[19], 4);
	int anisotropic = tss[17] == 3 || tss[16] == 3;
	VkSamplerCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = tss[16] >= 2 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
		.minFilter = tss[17] >= 2 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
		.mipmapMode = tss[18] == 2 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = Vkd_Address(tss[13]),
		.addressModeV = Vkd_Address(tss[14]),
		.addressModeW = Vkd_Address(tss[25]),
		.mipLodBias = isfinite(bias) ? bias : 0.0f,
		.anisotropyEnable = anisotropic && aniso > 1,
		.maxAnisotropy = (float)(aniso ? aniso : 1),
		.minLod = 0.0f,
		.maxLod = tss[18] ? VK_LOD_CLAMP_NONE : 0.25f,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
	};
	VkSampler s = VK_NULL_HANDLE;
	vkCreateSampler(vk.device, &ci, NULL, &s);
	if (vkd_numSamplers < VKD_MAX_SAMPLERS) {
		vkd_samplers[vkd_numSamplers].key = key;
		vkd_samplers[vkd_numSamplers].s = s;
		vkd_numSamplers++;
	}
	return s;
}

/* ---- trace: every call of the first frame that starts SCRAP_VK_TRACE seconds after the device into the log ---- */

static void Vkd_FrameDone(void) {
	if (vkd_traceState == 1) {
		vkd_traceState = 2;
	} else if (!vkd_traceState && vkd_traceAt && GetTickCount() >= vkd_traceAt) {
		vkd_traceState = 1;
	}
}

void Vkd_TraceStart(void) {
	vkd_traceState = 1;
}

int Vkd_Tracing(void) {
	return vkd_traceState == 1;
}

static void Vkd_Trace(const VkoDev *d, uint32_t fvf, uint32_t stride, uint32_t prims, int indexed) {
	if (!Vkd_Tracing()) {
		return;
	}
	const uint32_t *rs = d->rs;
	Com_Printf("draw rt %ux%u fmt %u zs %d fvf %x stride %u prims %u idx %d ps %u | z %u/%u/%u blend %u %u/%u "
		"cull %u light %u fog %u/%u/%u at %u/%u/%u wm %x tf %08x\n", d->rt->width, d->rt->height, d->rt->format,
		d->zs != NULL, fvf, stride, prims, indexed, d->ps, rs[7], rs[14], rs[23], rs[27], rs[19], rs[20], rs[22],
		rs[137], rs[28], rs[140], rs[35], rs[15], rs[25], rs[24], rs[168], rs[60]);
	for (int s = 0; s < VKO_STAGES && d->tss[s][1] != 1; s++) {
		const uint32_t *t = d->tss[s];
		const VkoTex *x = s < VKD_TEXTURES ? d->tex[s] : NULL;
		Com_Printf("  st%d c %u %x %x %x a %u %x %x %x tci %x ttf %x res %u tex %ux%u fmt %u\n", s, t[1], t[2], t[3],
			t[26], t[4], t[5], t[6], t[27], t[11], t[24], t[28], x ? x->width : 0, x ? x->height : 0,
			x ? x->format : 0);
	}
}

/* ---- the state of a draw ---- */

static void Vkd_Color(float *out, uint32_t c) {
	out[0] = (c >> 16 & 255) / 255.0f;
	out[1] = (c >> 8 & 255) / 255.0f;
	out[2] = (c & 255) / 255.0f;
	out[3] = (c >> 24) / 255.0f;
}

static float Vkd_Float(uint32_t v) {
	float f;
	memcpy(&f, &v, 4);
	return f;
}

/* p * m of D3D (row vectors), w = 1 or 0 */
static void Vkd_Transform(float *out, const float *p, const float *m, float w) {
	for (int j = 0; j < 3; j++) {
		out[j] = p[0] * m[j] + p[1] * m[4 + j] + p[2] * m[8 + j] + w * m[12 + j];
	}
}

static void Vkd_FillFF(VkdFF *f, VkoDev *d, uint32_t lflags, const uint32_t *texDim) {
	const uint32_t *rs = d->rs;

	memcpy(f->world, d->transform[256], 64);
	memcpy(f->view, d->transform[2], 64);
	memcpy(f->proj, d->transform[3], 64);
	for (int s = 0; s < 4; s++) {
		memcpy(f->texm[s], d->transform[16 + s], 64);
	}
	f->viewport[0] = (float)d->viewport[0];
	f->viewport[1] = (float)d->viewport[1];
	f->viewport[2] = (float)(d->viewport[2] ? d->viewport[2] : 1);
	f->viewport[3] = (float)(d->viewport[3] ? d->viewport[3] : 1);
	memcpy(f->matDiffuse, &d->material[0], 16);
	memcpy(f->matAmbient, &d->material[4], 16);
	memcpy(f->matSpecular, &d->material[8], 16);
	memcpy(f->matEmissive, &d->material[12], 16);
	f->misc[0] = d->material[16];
	f->misc[1] = (rs[24] & 255) / 255.0f;
	static int dbg = -1;
	if (dbg < 0) {
		const char *e = getenv("SCRAP_VK_DEBUG");
		dbg = e ? atoi(e) : 0;
	}
	f->misc[2] = (float)dbg;
	Vkd_Color(f->ambient, rs[139]);
	Vkd_Color(f->fogColor, rs[34]);
	float fs = Vkd_Float(rs[36]), fe = Vkd_Float(rs[37]);
	f->fogParams[0] = fs;
	f->fogParams[1] = fe;
	f->fogParams[2] = Vkd_Float(rs[38]);
	f->fogParams[3] = fe != fs ? 1.0f / (fe - fs) : 0.0f;
	Vkd_Color(f->tfactor, rs[60]);
	uint32_t fl = lflags;
	if (rs[137] && !(lflags & FL_RHW)) {
		fl |= FL_LIGHTING;
	}
	if (rs[29]) {
		fl |= FL_SPECULARENABLE;
	}
	if (rs[141]) {
		fl |= FL_COLORVERTEX;
	}
	if (rs[142]) {
		fl |= FL_LOCALVIEWER;
	}
	if (rs[143]) {
		fl |= FL_NORMALIZE;
	}
	if (rs[28]) {
		fl |= FL_FOG;
	}
	if (rs[48]) {
		fl |= FL_RANGEFOG;
	}
	f->flags[0] = (int32_t)fl;
	f->flags[1] = (int32_t)rs[140];
	f->flags[2] = (int32_t)rs[35];
	f->flags[3] = rs[15] ? (int32_t)rs[25] : 0;
	f->matSource[0] = (int32_t)rs[145];
	f->matSource[1] = (int32_t)rs[146];
	f->matSource[2] = (int32_t)rs[147];
	f->matSource[3] = (int32_t)rs[148];
	for (int i = 0; i < 4; i++) {
		f->texDim[i] = (int32_t)texDim[i];
	}
	int n = 0;
	const float *view = d->transform[2];
	for (int i = 0; i < VKO_LIGHTS; i++) {
		if (!d->lightOn[i] || !(fl & FL_LIGHTING)) {
			continue;
		}
		const float *l = d->light[i];
		uint32_t type;
		memcpy(&type, &l[0], 4);
		memcpy(f->lightDiffuse[n], &l[1], 16);
		memcpy(f->lightSpecular[n], &l[5], 16);
		memcpy(f->lightAmbient[n], &l[9], 16);
		Vkd_Transform(f->lightPos[n], &l[13], view, 1.0f);
		f->lightPos[n][3] = (float)type;
		float dir[3];
		Vkd_Transform(dir, &l[16], view, 0.0f);
		float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
		for (int c = 0; c < 3; c++) {
			f->lightDir[n][c] = len > 0.0f ? -dir[c] / len : 0.0f;
		}
		f->lightAtt[n][0] = l[19];
		f->lightAtt[n][1] = l[21];
		f->lightAtt[n][2] = l[22];
		f->lightAtt[n][3] = l[23];
		f->lightSpot[n][0] = l[20];
		f->lightSpot[n][1] = cosf(l[24] * 0.5f);
		f->lightSpot[n][2] = cosf(l[25] * 0.5f);
		n++;
	}
	f->lightCount[0] = n;
	for (int s = 0; s < VKO_STAGES; s++) {
		const uint32_t *t = d->tss[s];
		f->stage[s][0] = (int32_t)t[1];
		f->stage[s][1] = (int32_t)((t[2] & 0xff) | (t[3] & 0xff) << 8 | (t[26] & 0xff) << 16);
		f->stage[s][2] = (int32_t)t[4];
		f->stage[s][3] = (int32_t)((t[5] & 0xff) | (t[6] & 0xff) << 8 | (t[27] & 0xff) << 16);
		f->stageTex[s][0] = (int32_t)((t[11] & 0xffff) | (t[11] >> 16 & 0xf) << 16);
		f->stageTex[s][1] = (int32_t)t[24];
		f->stageTex[s][2] = s < VKD_TEXTURES && d->tex[s] != NULL;
		f->stageTex[s][3] = t[28] == 5;		/* RESULTARG temp */
		f->bumpMat[s][0] = Vkd_Float(t[7]);
		f->bumpMat[s][1] = Vkd_Float(t[8]);
		f->bumpMat[s][2] = Vkd_Float(t[9]);
		f->bumpMat[s][3] = Vkd_Float(t[10]);
	}
	memcpy(f->psConst, d->psConst, sizeof(f->psConst));
}

static VkCompareOp Vkd_Cmp(uint32_t f) {
	static const VkCompareOp ops[9] = { VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS,
		VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL,
		VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS };
	return f < 9 ? ops[f] : VK_COMPARE_OP_ALWAYS;
}

static VkStencilOp Vkd_StencilOp(uint32_t o) {
	static const VkStencilOp ops[9] = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_ZERO,
		VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_DECREMENT_AND_CLAMP,
		VK_STENCIL_OP_INVERT, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_DECREMENT_AND_WRAP };
	return o < 9 ? ops[o] : VK_STENCIL_OP_KEEP;
}

/* the vertices of count primitives of a D3DPRIMITIVETYPE, its topology and class (0 points, 1 lines, 2 triangles) */
static uint32_t Vkd_Prims(int type, uint32_t count, VkPrimitiveTopology *topo, uint32_t *cls) {
	switch (type) {
	case 1:
		*topo = VK_PRIMITIVE_TOPOLOGY_POINT_LIST, *cls = 0;
		return count;
	case 2:
		*topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST, *cls = 1;
		return count * 2;
	case 3:
		*topo = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, *cls = 1;
		return count + 1;
	case 5:
		*topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, *cls = 2;
		return count + 2;
	case 6:
		*topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, *cls = 2;
		return count + 2;
	default:
		*topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, *cls = 2;
		return count * 3;
	}
}

/*
 * A draw that samples its own render target (DuDv_Filter works on the bump target in place, each pixel reading its
 * own texel, which Direct3D 8 allows): the target is copied into vkd_feedback first and the draw samples the copy.
 * The view of the copy, VK_NULL_HANDLE when it cannot be made.
 */
static VkImageView Vkd_Feedback(VkImg *rt) {
	if (vkd_feedback.img && (vkd_feedback.width != rt->width || vkd_feedback.height != rt->height
		|| vkd_feedback.format != rt->format)) {
		Vk_DeferImage(&vkd_feedback);
	}
	if (!vkd_feedback.img && !Vk_CreateImage(&vkd_feedback, rt->format, rt->width, rt->height, 1,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, (VkComponentMapping) { 0 })) {
		return VK_NULL_HANDLE;
	}
	Vkd_End();
	VkCommandBuffer cmd = Vk_Cmd();
	Vk_Transition(cmd, rt, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	Vk_Transition(cmd, &vkd_feedback, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkImageCopy c = {
		.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.extent = { rt->width, rt->height, 1 },
	};
	vkCmdCopyImage(cmd, rt->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkd_feedback.img,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
	Vk_Transition(cmd, &vkd_feedback, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return vkd_feedback.view;
}

/*
 * Everything of a draw up to the draw command: the textures in the shader read layout (outside the render pass), the
 * render pass, the uniform block, the pipeline and the dynamic state, the descriptors and the vertex buffers.
 * vb/vbOff the vertices; 0 when the draw cannot happen.
 */
static int Vkd_Prepare(VkoDev *d, uint32_t fvf, uint32_t stride, uint32_t cls, VkPrimitiveTopology topo, VkBuffer vb,
	VkDeviceSize vbOff) {
	VkdLayout l;
	VkDescriptorImageInfo img[VKD_TEXTURES];

	Vkd_Fvf(fvf, &l);
	VkBuffer ub;
	VkDeviceSize uo;
	VkdFF *f = Vk_RingAlloc(sizeof(VkdFF), vk.props.limits.minUniformBufferOffsetAlignment, &ub, &uo);
	if (!f) {
		return 0;
	}
	Vkd_FillFF(f, d, l.flags, l.texDim);
	for (int s = 0; s < VKD_TEXTURES; s++) {
		VkoTex *t = d->tex[s];
		img[s] = (VkDescriptorImageInfo) { Vkd_Sampler(d->tss[s]), vkd_white.view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		if (!t) {
			continue;
		}
		if (t->img.img == d->rt->img->img) {
			VkImageView v = Vkd_Feedback(d->rt->img);
			if (v) {
				img[s].imageView = v;
			}
			continue;
		}
		if (t->img.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			Vkd_End();
			Vk_Transition(Vk_Cmd(), &t->img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		img[s].imageView = t->img.view;
	}
	VkCommandBuffer cmd = Vkd_Begin(d);
	const uint32_t *rs = d->rs;
	VkdPipeKey key = {
		.fvf = fvf,
		.stride = stride,
		.topo = cls,
		.blend = (rs[27] ? 1 : 0) | (rs[19] & 31) << 1 | (rs[20] & 31) << 6 | (rs[171] & 7) << 11,
		.writeMask = rs[168] & 15,
		.fill = rs[8] == 1 ? 1 : rs[8] == 2 ? 2 : 3,
		.ps = d->ps && d->ps <= vkd_numPsHandles ? vkd_psHandle[d->ps - 1] + 1 : 0,
		.depth = d->zs ? (uint32_t)d->zs->img->format : VK_FORMAT_UNDEFINED,
	};
	VkPipeline p = Vkd_Pipeline(&key);
	if (!p) {
		return 0;
	}
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
	/* the viewport upside down: the D3D convention of y down on the screen with D3D clip space */
	float minz = Vkd_Float(d->viewport[4]), maxz = Vkd_Float(d->viewport[5]);
	VkViewport v = {
		(float)d->viewport[0], (float)(d->viewport[1] + d->viewport[3]), (float)d->viewport[2],
		-(float)d->viewport[3], minz, maxz,
	};
	VkRect2D sc = { { (int32_t)d->viewport[0], (int32_t)d->viewport[1] }, { d->viewport[2], d->viewport[3] } };
	vkCmdSetViewport(cmd, 0, 1, &v);
	vkCmdSetScissor(cmd, 0, 1, &sc);
	vkCmdSetCullMode(cmd, rs[22] == 2 ? VK_CULL_MODE_FRONT_BIT : rs[22] == 3 ? VK_CULL_MODE_BACK_BIT
		: VK_CULL_MODE_NONE);
	vkCmdSetFrontFace(cmd, VK_FRONT_FACE_CLOCKWISE);
	vkCmdSetPrimitiveTopology(cmd, topo);
	int depth = d->zs && rs[7];
	vkCmdSetDepthTestEnable(cmd, depth);
	vkCmdSetDepthWriteEnable(cmd, depth && rs[14]);
	vkCmdSetDepthCompareOp(cmd, Vkd_Cmp(rs[23]));
	vkCmdSetDepthBiasEnable(cmd, rs[47] != 0);
	vkCmdSetDepthBias(cmd, -(float)rs[47], 0.0f, -(float)rs[47] * 0.5f);
	vkCmdSetStencilTestEnable(cmd, d->zs && Vk_HasStencil(d->zs->img->format) && rs[52]);
	vkCmdSetStencilOp(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, Vkd_StencilOp(rs[53]), Vkd_StencilOp(rs[55]),
		Vkd_StencilOp(rs[54]), Vkd_Cmp(rs[56]));
	vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, rs[58] & 0xff);
	vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, rs[59] & 0xff);
	vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, rs[57] & 0xff);
	VkDescriptorBufferInfo bi = { ub, uo, sizeof(VkdFF) };
	VkWriteDescriptorSet w[2] = {
		{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = 0, .descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &bi },
		{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = 1, .descriptorCount = VKD_TEXTURES,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = img },
	};
	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkd_layout, 0, 2, w);
	VkBuffer bufs[2] = { vb, vkd_defaults.buf };
	VkDeviceSize offs[2] = { vbOff, 0 };
	vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
	return 1;
}

int Vkd_Draw(VkoDev *d, int type, uint32_t primCount, uint32_t start, const void *data, uint32_t stride,
	const void *idx, int idxFmt, int indexed) {
	VkPrimitiveTopology topo;
	uint32_t cls;
	uint32_t n = Vkd_Prims(type, primCount, &topo, &cls);
	VkBuffer vb;
	VkDeviceSize vbOff;

	(void)idx, (void)idxFmt;
	if (!primCount || !d->fvf) {
		return 0;
	}
	/* the vertices and the uniform block in the same frame */
	if (!Vk_RingReserve((data ? (VkDeviceSize)n * stride + 16 : 0) + sizeof(VkdFF) + VKD_UNIFORM_ALIGN)) {
		return 0;
	}
	if (data) {
		void *p = Vk_RingAlloc((VkDeviceSize)n * stride, 16, &vb, &vbOff);
		if (!p) {
			return 0;
		}
		memcpy(p, data, (size_t)n * stride);
	} else {
		if (!d->stream) {
			return 0;
		}
		vb = d->stream->mem.buf;
		vbOff = 0;
		stride = d->streamStride;
	}
	if (indexed && (!d->indices || data)) {
		return 0;
	}
	Vkd_Trace(d, d->fvf, stride, primCount, indexed);
	if (!Vkd_Prepare(d, d->fvf, stride, cls, topo, vb, vbOff)) {
		return 0;
	}
	VkCommandBuffer cmd = Vk_Cmd();
	if (indexed) {
		VkoBuf *ib = d->indices;
		ib->used = vk.serial;
		vkCmdBindIndexBuffer(cmd, ib->mem.buf, 0, ib->format == 102 ? VK_INDEX_TYPE_UINT32
			: VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(cmd, n, 1, start, (int32_t)d->baseVertex, 0);
	} else {
		vkCmdDraw(cmd, n, 1, data ? 0 : start, 0);
	}
	if (d->stream && !data) {
		d->stream->used = vk.serial;
	}
	return 0;
}

int Vkd_DrawIndexedUP(VkoDev *d, int type, uint32_t primCount, uint32_t numVertices, const void *idx, int idxFmt,
	const void *data, uint32_t stride) {
	VkPrimitiveTopology topo;
	uint32_t cls;
	uint32_t n = Vkd_Prims(type, primCount, &topo, &cls);
	uint32_t isize = idxFmt == 102 ? 4 : 2;
	VkBuffer vb, ib;
	VkDeviceSize vbOff, ibOff;

	if (!primCount || !d->fvf) {
		return 0;
	}
	if (!Vk_RingReserve((VkDeviceSize)numVertices * stride + (VkDeviceSize)n * isize + 32 + sizeof(VkdFF)
		+ VKD_UNIFORM_ALIGN)) {
		return 0;
	}
	void *v = Vk_RingAlloc((VkDeviceSize)numVertices * stride, 16, &vb, &vbOff);
	void *i = Vk_RingAlloc((VkDeviceSize)n * isize, 16, &ib, &ibOff);
	if (!v || !i) {
		return 0;
	}
	memcpy(v, data, (size_t)numVertices * stride);
	memcpy(i, idx, (size_t)n * isize);
	Vkd_Trace(d, d->fvf, stride, primCount, 2);
	if (!Vkd_Prepare(d, d->fvf, stride, cls, topo, vb, vbOff)) {
		return 0;
	}
	VkCommandBuffer cmd = Vk_Cmd();
	vkCmdBindIndexBuffer(cmd, ib, ibOff, isize == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(cmd, n, 1, 0, 0, 0);
	return 0;
}

uint32_t Vkd_CreatePixelShader(const char *name) {
	char base[64];
	const char *b = strrchr(name, '/');
	size_t n = 0;

	for (b = b ? b + 1 : name; *b && *b != '.' && n < sizeof(base) - 1; b++) {
		base[n++] = (char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);
	}
	base[n] = 0;
	for (size_t i = 0; i < sizeof(vkd_psAlias) / sizeof(vkd_psAlias[0]); i++) {
		if (!strcmp(base, vkd_psAlias[i][0])) {
			snprintf(base, sizeof(base), "%s", vkd_psAlias[i][1]);
		}
	}
	for (size_t i = 0; i < VKD_NUM_PS; i++) {
		if (!strcmp(base, vkd_psCode[i].name) && vkd_numPsHandles < VKD_MAX_PS_HANDLES) {
			vkd_psHandle[vkd_numPsHandles++] = (uint32_t)i;
			return vkd_numPsHandles;
		}
	}
	Com_Printf("vulkan: no GLSL port of the pixel shader %s\n", name);
	return 0;
}

/* ---- the present ---- */

static VkPipeline Vkd_PresentPipeline(void) {
	if (vkd_pr.pipe && vkd_pr.format == vk.swapFormat) {
		return vkd_pr.pipe;
	}
	if (vkd_pr.pipe) {
		Vk_Flush();
		vkDestroyPipeline(vk.device, vkd_pr.pipe, NULL);
		vkd_pr.pipe = VK_NULL_HANDLE;
	}
	VkPipelineVertexInputStateCreateInfo vi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
	VkPipelineViewportStateCreateInfo vp = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	VkPipelineRasterizationStateCreateInfo rs = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.0f,
	};
	VkPipelineMultisampleStateCreateInfo ms = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	VkPipelineColorBlendAttachmentState cba = { .colorWriteMask = 15 };
	VkPipelineColorBlendStateCreateInfo cb = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &cba,
	};
	static const VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dy = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dyn,
	};
	VkPipelineShaderStageCreateInfo st[2] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vkd_pr.vert, .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = vkd_pr.frag, .pName = "main" },
	};
	VkPipelineRenderingCreateInfo pr = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &vk.swapFormat,
	};
	VkGraphicsPipelineCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &pr,
		.stageCount = 2,
		.pStages = st,
		.pVertexInputState = &vi,
		.pInputAssemblyState = &ia,
		.pViewportState = &vp,
		.pRasterizationState = &rs,
		.pMultisampleState = &ms,
		.pColorBlendState = &cb,
		.pDynamicState = &dy,
		.layout = vkd_pr.layout,
	};
	if (vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &ci, NULL, &vkd_pr.pipe) < 0) {
		vkd_pr.pipe = VK_NULL_HANDLE;
		return VK_NULL_HANDLE;
	}
	vkd_pr.format = vk.swapFormat;
	return vkd_pr.pipe;
}

/* the gamma ramp of the device into the ramp texture when it changed */
static void Vkd_UploadRamp(const VkoDev *d) {
	if (!memcmp(vkd_pr.rampData, d->gamma, sizeof(vkd_pr.rampData))) {
		return;
	}
	VkBuffer buf;
	VkDeviceSize off;
	uint16_t *p = Vk_RingAlloc(256 * 8, 16, &buf, &off);
	if (!p) {
		return;
	}
	for (int i = 0; i < 256; i++) {
		p[4 * i] = d->gamma[0][i];
		p[4 * i + 1] = d->gamma[1][i];
		p[4 * i + 2] = d->gamma[2][i];
		p[4 * i + 3] = 0xffff;
	}
	VkCommandBuffer cmd = Vk_Cmd();
	Vk_Transition(cmd, &vkd_pr.ramp, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkBufferImageCopy copy = {
		.bufferOffset = off,
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageExtent = { 256, 1, 1 },
	};
	vkCmdCopyBufferToImage(cmd, buf, vkd_pr.ramp.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	Vk_Transition(cmd, &vkd_pr.ramp, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	memcpy(vkd_pr.rampData, d->gamma, sizeof(vkd_pr.rampData));
}

int Vkd_Present(VkoDev *d) {
	VkImg *bb = d->backBuffer->img;
	uint32_t index;

	Vkd_End();
	Vkd_FrameDone();
	Vkd_UploadRamp(d);
	VkPipeline pipe = Vkd_PresentPipeline();
	if (!Vk_Acquire(&index)) {
		return 1;
	}
	VkCommandBuffer cmd = Vk_Cmd();
	if (!pipe) {
		return Vk_QueuePresent(index);
	}
	VkImg swap = {
		.img = vk.swapImages[index],
		.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
		.levels = 1,
		.layout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	Vk_Transition(cmd, bb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	Vk_Transition(cmd, &swap, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo color = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = vk.swapViews[index],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	VkRenderingInfo ri = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = { { 0, 0 }, vk.swapExtent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color,
	};
	vkCmdBeginRendering(cmd, &ri);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
	VkViewport v = { 0, 0, (float)vk.swapExtent.width, (float)vk.swapExtent.height, 0, 1 };
	VkRect2D sc = { { 0, 0 }, vk.swapExtent };
	vkCmdSetViewport(cmd, 0, 1, &v);
	vkCmdSetScissor(cmd, 0, 1, &sc);
	VkDescriptorImageInfo img[2] = {
		{ vkd_pr.linear, bb->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ vkd_pr.nearest, vkd_pr.ramp.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
	};
	VkWriteDescriptorSet w[2] = {
		{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = 0, .descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &img[0] },
		{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = 1, .descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &img[1] },
	};
	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkd_pr.layout, 0, 2, w);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRendering(cmd);
	Vk_Transition(cmd, &swap, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	return Vk_QueuePresent(index);
}
