/*
 * The Vulkan core of the render device backend, see vkcore.h. Vulkan 1.3 with dynamic rendering, synchronization2 and
 * the extended dynamic state, and VK_KHR_push_descriptor. The loader is vulkan-1.dll, loaded at run time.
 * SCRAP_VK_VALIDATION=1 switches the validation layer on.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "vkcore.h"

#define VKF_GLOBAL(name) PFN_##name name;
#define VKF_INSTANCE(name) PFN_##name name;
#define VKF_DEVICE(name) PFN_##name name;
#include "vkfuncs.h"
#undef VKF_GLOBAL
#undef VKF_INSTANCE
#undef VKF_DEVICE

VkCore vk;

static PFN_vkGetInstanceProcAddr vk_getInstanceProcAddr;

/* the objects to destroy once the GPU is past the frame that used them last */
#define VK_DEFER_MAX 4096
static struct {
	uint64_t serial;
	VkBuf buf;
	VkImg img;
} vk_defer[VK_DEFER_MAX];
static int vk_deferCount;

static int Vk_Check(VkResult r, const char *what) {
	if (r != VK_SUCCESS) {
		Com_Printf("vulkan: %s failed (%d)\n", what, (int)r);
		return 0;
	}
	return 1;
}

int Vk_Init(void) {
	if (vk.instance) {
		return 1;
	}
	HMODULE lib = LoadLibraryA("vulkan-1.dll");
	if (!lib) {
		Com_Printf("vulkan: no vulkan-1.dll\n");
		return 0;
	}
	vk_getInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(void *)GetProcAddress(lib, "vkGetInstanceProcAddr");
	if (!vk_getInstanceProcAddr) {
		return 0;
	}
#define VKF_GLOBAL(name) name = (PFN_##name)vk_getInstanceProcAddr(NULL, #name);
#define VKF_INSTANCE(name)
#define VKF_DEVICE(name)
#include "vkfuncs.h"
#undef VKF_GLOBAL
#undef VKF_INSTANCE
#undef VKF_DEVICE
	uint32_t version = VK_API_VERSION_1_0;
	if (vkEnumerateInstanceVersion) {
		vkEnumerateInstanceVersion(&version);
	}
	if (version < VK_API_VERSION_1_3) {
		Com_Printf("vulkan: the loader has %u.%u, 1.3 needed\n", VK_API_VERSION_MAJOR(version),
			VK_API_VERSION_MINOR(version));
		return 0;
	}
	const char *exts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
	const char *val = getenv("SCRAP_VK_VALIDATION");
	VkApplicationInfo app = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Scrapland",
		.pEngineName = "ScrapC",
		.apiVersion = VK_API_VERSION_1_3,
	};
	VkInstanceCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app,
		.enabledExtensionCount = 2,
		.ppEnabledExtensionNames = exts,
		.enabledLayerCount = val && *val == '1',
		.ppEnabledLayerNames = layers,
	};
	if (!Vk_Check(vkCreateInstance(&ci, NULL, &vk.instance), "vkCreateInstance")) {
		return 0;
	}
#define VKF_GLOBAL(name)
#define VKF_INSTANCE(name) name = (PFN_##name)vk_getInstanceProcAddr(vk.instance, #name);
#define VKF_DEVICE(name)
#include "vkfuncs.h"
#undef VKF_GLOBAL
#undef VKF_INSTANCE
#undef VKF_DEVICE
	return 1;
}

/* SCRAP_VK_DEVICE=n picks the nth GPU, else the first discrete one, else the first */
static VkPhysicalDevice Vk_PickGpu(void) {
	VkPhysicalDevice gpus[8];
	uint32_t n = 8;

	if (vkEnumeratePhysicalDevices(vk.instance, &n, gpus) < 0 || !n) {
		return VK_NULL_HANDLE;
	}
	/* SCRAP_VK_DEVICE: the index of the GPU, or its vendor:device ids in hex (1002:164e) */
	const char *pick = getenv("SCRAP_VK_DEVICE");
	unsigned vendor, device;
	int ids = pick && sscanf(pick, "%x:%x", &vendor, &device) == 2;
	if (pick && !ids && (uint32_t)atoi(pick) < n) {
		return gpus[atoi(pick)];
	}
	for (uint32_t i = 0; ids && i < n; i++) {
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(gpus[i], &p);
		if (p.vendorID == vendor && p.deviceID == device) {
			return gpus[i];
		}
	}
	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceProperties(gpus[i], &p);
		if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && p.apiVersion >= VK_API_VERSION_1_3) {
			return gpus[i];
		}
	}
	return gpus[0];
}

int Vk_GpuProperties(VkPhysicalDeviceProperties *out) {
	VkPhysicalDevice gpu = vk.instance ? Vk_PickGpu() : VK_NULL_HANDLE;
	if (!gpu) {
		return 0;
	}
	vkGetPhysicalDeviceProperties(gpu, out);
	return 1;
}

int Vk_HasStencil(VkFormat f) {
	return f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D16_UNORM_S8_UINT;
}

int Vk_IsDepth(VkFormat f) {
	return Vk_HasStencil(f) || f == VK_FORMAT_D16_UNORM || f == VK_FORMAT_X8_D24_UNORM_PACK32 || f == VK_FORMAT_D32_SFLOAT;
}

int Vk_DepthSupported(VkFormat f) {
	VkFormatProperties fp;
	vkGetPhysicalDeviceFormatProperties(vk.gpu, f, &fp);
	return (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

static VkFormat Vk_PickDepth(void) {
	static const VkFormat cands[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
	for (int i = 0; i < 2; i++) {
		VkFormatProperties fp;
		vkGetPhysicalDeviceFormatProperties(vk.gpu, cands[i], &fp);
		if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			return cands[i];
		}
	}
	return VK_FORMAT_D32_SFLOAT_S8_UINT;
}

int Vk_CreateDevice(void *hwnd, uint32_t width, uint32_t height, byte vsync) {
	if (!Vk_Init()) {
		return 0;
	}
	vk.hwnd = hwnd;
	VkWin32SurfaceCreateInfoKHR sci = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = GetModuleHandleA(NULL),
		.hwnd = hwnd,
	};
	if (!Vk_Check(vkCreateWin32SurfaceKHR(vk.instance, &sci, NULL, &vk.surface), "vkCreateWin32SurfaceKHR")) {
		return 0;
	}
	vk.gpu = Vk_PickGpu();
	if (!vk.gpu) {
		Com_Printf("vulkan: no GPU\n");
		return 0;
	}
	vkGetPhysicalDeviceProperties(vk.gpu, &vk.props);
	vkGetPhysicalDeviceMemoryProperties(vk.gpu, &vk.memProps);
	Com_Printf("vulkan: %s, %u.%u.%u\n", vk.props.deviceName, VK_API_VERSION_MAJOR(vk.props.apiVersion),
		VK_API_VERSION_MINOR(vk.props.apiVersion), VK_API_VERSION_PATCH(vk.props.apiVersion));
	VkQueueFamilyProperties qf[16];
	uint32_t nq = 16;
	vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &nq, qf);
	vk.queueFamily = UINT32_MAX;
	for (uint32_t i = 0; i < nq; i++) {
		VkBool32 present = 0;
		vkGetPhysicalDeviceSurfaceSupportKHR(vk.gpu, i, vk.surface, &present);
		if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
			vk.queueFamily = i;
			break;
		}
	}
	if (vk.queueFamily == UINT32_MAX) {
		Com_Printf("vulkan: no queue that draws and presents\n");
		return 0;
	}
	VkPhysicalDeviceVulkan13Features f13 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	VkPhysicalDeviceFeatures2 f2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f13 };
	vkGetPhysicalDeviceFeatures2(vk.gpu, &f2);
	if (!f13.dynamicRendering || !f13.synchronization2) {
		Com_Printf("vulkan: no dynamic rendering or synchronization2\n");
		return 0;
	}
	VkPhysicalDeviceVulkan13Features want13 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.dynamicRendering = 1,
		.synchronization2 = 1,
		.shaderDemoteToHelperInvocation = f13.shaderDemoteToHelperInvocation,
	};
	VkPhysicalDeviceFeatures2 want = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &want13,
		.features = {
			.robustBufferAccess = f2.features.robustBufferAccess,
			.fillModeNonSolid = f2.features.fillModeNonSolid,
			.samplerAnisotropy = f2.features.samplerAnisotropy,
			.textureCompressionBC = f2.features.textureCompressionBC,
		},
	};
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = vk.queueFamily,
		.queueCount = 1,
		.pQueuePriorities = &prio,
	};
	const char *exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };
	VkDeviceCreateInfo dci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &want,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &qci,
		.enabledExtensionCount = 2,
		.ppEnabledExtensionNames = exts,
	};
	if (!Vk_Check(vkCreateDevice(vk.gpu, &dci, NULL, &vk.device), "vkCreateDevice")) {
		return 0;
	}
#define VKF_GLOBAL(name)
#define VKF_INSTANCE(name)
#define VKF_DEVICE(name) name = (PFN_##name)vkGetDeviceProcAddr(vk.device, #name);
#include "vkfuncs.h"
#undef VKF_GLOBAL
#undef VKF_INSTANCE
#undef VKF_DEVICE
	vkGetDeviceQueue(vk.device, vk.queueFamily, 0, &vk.queue);
	vk.depthFormat = Vk_PickDepth();
	VkCommandPoolCreateInfo pci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.queueFamily,
	};
	if (!Vk_Check(vkCreateCommandPool(vk.device, &pci, NULL, &vk.pool), "vkCreateCommandPool")) {
		return 0;
	}
	for (int i = 0; i < VK_FRAMES; i++) {
		VkFrame *f = &vk.frames[i];
		VkCommandBufferAllocateInfo ai = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = vk.pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
		VkSemaphoreCreateInfo si = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		if (vkAllocateCommandBuffers(vk.device, &ai, &f->cmd) < 0 || vkCreateFence(vk.device, &fi, NULL, &f->done) < 0
			|| vkCreateSemaphore(vk.device, &si, NULL, &f->acquired) < 0
			|| !Vk_CreateBuffer(&f->ring, VK_RING_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
				| VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
				| VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 1)) {
			Com_Printf("vulkan: the frame resources failed\n");
			return 0;
		}
	}
	for (int i = 0; i < VK_MAX_SWAP_IMAGES; i++) {
		VkSemaphoreCreateInfo si = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		if (vkCreateSemaphore(vk.device, &si, NULL, &vk.swapRendered[i]) < 0) {
			return 0;
		}
	}
	return Vk_ResizeSwapchain(width, height, vsync);
}

static void Vk_DestroySwapViews(void) {
	for (uint32_t i = 0; i < VK_MAX_SWAP_IMAGES; i++) {
		if (vk.swapViews[i]) {
			vkDestroyImageView(vk.device, vk.swapViews[i], NULL);
			vk.swapViews[i] = VK_NULL_HANDLE;
		}
	}
}

int Vk_ResizeSwapchain(uint32_t width, uint32_t height, byte vsync) {
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR fmts[32];
	uint32_t nf = 32;

	vkDeviceWaitIdle(vk.device);
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.gpu, vk.surface, &caps);
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &nf, fmts);
	vk.swapFormat = fmts[0].format;
	VkColorSpaceKHR space = fmts[0].colorSpace;
	for (uint32_t i = 0; i < nf; i++) {
		if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
			vk.swapFormat = fmts[i].format;
			space = fmts[i].colorSpace;
			break;
		}
	}
	VkExtent2D ext = caps.currentExtent;
	if (ext.width == UINT32_MAX) {
		ext.width = width;
		ext.height = height;
	}
	if (!ext.width || !ext.height) {
		return 1;		/* minimized: no swapchain until the window has a size again */
	}
	VkPresentModeKHR modes[8];
	uint32_t nm = 8;
	vkGetPhysicalDeviceSurfacePresentModesKHR(vk.gpu, vk.surface, &nm, modes);
	VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
	for (uint32_t i = 0; !vsync && i < nm; i++) {
		if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR || modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			mode = modes[i];
		}
	}
	uint32_t count = caps.minImageCount + 1;
	if (caps.maxImageCount && count > caps.maxImageCount) {
		count = caps.maxImageCount;
	}
	Vk_DestroySwapViews();
	VkSwapchainKHR old = vk.swapchain;
	VkSwapchainCreateInfoKHR ci = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vk.surface,
		.minImageCount = count,
		.imageFormat = vk.swapFormat,
		.imageColorSpace = space,
		.imageExtent = ext,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = caps.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = mode,
		.clipped = 1,
		.oldSwapchain = old,
	};
	if (!Vk_Check(vkCreateSwapchainKHR(vk.device, &ci, NULL, &vk.swapchain), "vkCreateSwapchainKHR")) {
		vk.swapchain = VK_NULL_HANDLE;
		return 0;
	}
	if (old) {
		vkDestroySwapchainKHR(vk.device, old, NULL);
	}
	vk.swapExtent = ext;
	vk.vsync = vsync;
	vk.swapCount = VK_MAX_SWAP_IMAGES;
	vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapCount, vk.swapImages);
	for (uint32_t i = 0; i < vk.swapCount; i++) {
		VkImageViewCreateInfo vi = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vk.swapImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = vk.swapFormat,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		};
		if (!Vk_Check(vkCreateImageView(vk.device, &vi, NULL, &vk.swapViews[i]), "vkCreateImageView")) {
			return 0;
		}
	}
	return 1;
}

/* frees the deferred objects whose frames are done */
static void Vk_ReleaseDeferred(void) {
	int j = 0;
	for (int i = 0; i < vk_deferCount; i++) {
		if (vk_defer[i].serial <= vk.completed) {
			Vk_DestroyBuffer(&vk_defer[i].buf);
			Vk_DestroyImage(&vk_defer[i].img);
		} else {
			vk_defer[j++] = vk_defer[i];
		}
	}
	vk_deferCount = j;
}

VkCommandBuffer Vk_Cmd(void) {
	VkFrame *f = &vk.frames[vk.frame];

	if (!vk.recording) {
		vkWaitForFences(vk.device, 1, &f->done, 1, UINT64_MAX);
		if (f->serial > vk.completed) {
			vk.completed = f->serial;
		}
		Vk_ReleaseDeferred();
		vkResetCommandBuffer(f->cmd, 0);
		VkCommandBufferBeginInfo bi = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		vkBeginCommandBuffer(f->cmd, &bi);
		f->ringUsed = 0;
		f->serial = ++vk.serial;
		vk.recording = 1;
	}
	return f->cmd;
}

static void Vk_Submit(VkSemaphore wait, VkSemaphore signal) {
	VkFrame *f = &vk.frames[vk.frame];
	VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	if (vk.endPass) {
		vk.endPass();
	}
	vkEndCommandBuffer(f->cmd);
	VkSubmitInfo si = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = wait != VK_NULL_HANDLE,
		.pWaitSemaphores = &wait,
		.pWaitDstStageMask = &stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &f->cmd,
		.signalSemaphoreCount = signal != VK_NULL_HANDLE,
		.pSignalSemaphores = &signal,
	};
	vkResetFences(vk.device, 1, &f->done);
	vkQueueSubmit(vk.queue, 1, &si, f->done);
	vk.recording = 0;
	vk.frame = (vk.frame + 1) % VK_FRAMES;
}

void Vk_Flush(void) {
	if (vk.recording) {
		Vk_Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
	}
	vkDeviceWaitIdle(vk.device);
	vk.completed = vk.serial;
	Vk_ReleaseDeferred();
}

int Vk_Acquire(uint32_t *index) {
	Vk_Cmd();
	if (!vk.swapchain) {
		Vk_Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
		return 0;
	}
	VkResult r = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.frames[vk.frame].acquired,
		VK_NULL_HANDLE, index);
	if (r < 0) {
		Vk_Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
		if (r == VK_ERROR_OUT_OF_DATE_KHR) {
			Vk_ResizeSwapchain(vk.swapExtent.width, vk.swapExtent.height, vk.vsync);
		}
		return 0;
	}
	return 1;
}

int Vk_QueuePresent(uint32_t index) {
	VkSemaphore rendered = vk.swapRendered[index];
	Vk_Submit(vk.frames[vk.frame].acquired, rendered);
	VkPresentInfoKHR pi = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &rendered,
		.swapchainCount = 1,
		.pSwapchains = &vk.swapchain,
		.pImageIndices = &index,
	};
	VkResult r = vkQueuePresentKHR(vk.queue, &pi);
	if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
		return Vk_ResizeSwapchain(vk.swapExtent.width, vk.swapExtent.height, vk.vsync);
	}
	return r >= 0;
}

void Vk_DestroyDevice(void) {
	if (!vk.device) {
		return;
	}
	Vk_Flush();
	for (int i = 0; i < VK_FRAMES; i++) {
		VkFrame *f = &vk.frames[i];
		Vk_DestroyBuffer(&f->ring);
		vkDestroySemaphore(vk.device, f->acquired, NULL);
		vkDestroyFence(vk.device, f->done, NULL);
	}
	for (int i = 0; i < VK_MAX_SWAP_IMAGES; i++) {
		vkDestroySemaphore(vk.device, vk.swapRendered[i], NULL);
	}
	vkDestroyCommandPool(vk.device, vk.pool, NULL);
	Vk_DestroySwapViews();
	if (vk.swapchain) {
		vkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
	}
	vkDestroyDevice(vk.device, NULL);
	vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
	VkInstance inst = vk.instance;
	memset(&vk, 0, sizeof(vk));
	vk.instance = inst;
}

uint32_t Vk_MemoryType(uint32_t bits, VkMemoryPropertyFlags flags) {
	for (uint32_t i = 0; i < vk.memProps.memoryTypeCount; i++) {
		if ((bits & 1u << i) && (vk.memProps.memoryTypes[i].propertyFlags & flags) == flags) {
			return i;
		}
	}
	return UINT32_MAX;
}

int Vk_CreateBuffer(VkBuf *b, VkDeviceSize size, VkBufferUsageFlags usage, int hostVisible) {
	VkBufferCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	memset(b, 0, sizeof(*b));
	if (vkCreateBuffer(vk.device, &ci, NULL, &b->buf) < 0) {
		return 0;
	}
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(vk.device, b->buf, &req);
	VkMemoryPropertyFlags want = hostVisible
		? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		: VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkMemoryAllocateInfo ai = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = req.size,
		.memoryTypeIndex = Vk_MemoryType(req.memoryTypeBits, want),
	};
	if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(vk.device, &ai, NULL, &b->mem) < 0) {
		vkDestroyBuffer(vk.device, b->buf, NULL);
		b->buf = VK_NULL_HANDLE;
		return 0;
	}
	vkBindBufferMemory(vk.device, b->buf, b->mem, 0);
	if (hostVisible) {
		vkMapMemory(vk.device, b->mem, 0, size, 0, (void **)&b->map);
	}
	b->size = size;
	return 1;
}

void Vk_DestroyBuffer(VkBuf *b) {
	if (b->buf) {
		vkDestroyBuffer(vk.device, b->buf, NULL);
	}
	if (b->mem) {
		vkFreeMemory(vk.device, b->mem, NULL);
	}
	memset(b, 0, sizeof(*b));
}

int Vk_CreateImage(VkImg *im, VkFormat format, uint32_t w, uint32_t h, uint32_t levels, VkImageUsageFlags usage,
	VkComponentMapping swizzle) {
	int depth = Vk_IsDepth(format);
	VkImageCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = { w, h, 1 },
		.mipLevels = levels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	memset(im, 0, sizeof(*im));
	if (vkCreateImage(vk.device, &ci, NULL, &im->img) < 0) {
		return 0;
	}
	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(vk.device, im->img, &req);
	VkMemoryAllocateInfo ai = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = req.size,
		.memoryTypeIndex = Vk_MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};
	if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(vk.device, &ai, NULL, &im->mem) < 0) {
		vkDestroyImage(vk.device, im->img, NULL);
		im->img = VK_NULL_HANDLE;
		return 0;
	}
	vkBindImageMemory(vk.device, im->img, im->mem, 0);
	im->format = format;
	im->aspect = !depth ? VK_IMAGE_ASPECT_COLOR_BIT : Vk_HasStencil(format)
		? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
	im->width = w;
	im->height = h;
	im->levels = levels;
	im->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageViewCreateInfo vi = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = im->img,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = swizzle,
		.subresourceRange = { depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1 },
	};
	if (vkCreateImageView(vk.device, &vi, NULL, &im->view) < 0) {
		Vk_DestroyImage(im);
		return 0;
	}
	return 1;
}

void Vk_DestroyImage(VkImg *im) {
	if (im->view) {
		vkDestroyImageView(vk.device, im->view, NULL);
	}
	if (im->img) {
		vkDestroyImage(vk.device, im->img, NULL);
	}
	if (im->mem) {
		vkFreeMemory(vk.device, im->mem, NULL);
	}
	memset(im, 0, sizeof(*im));
}

static void Vk_Defer(const VkBuf *b, const VkImg *im) {
	if (vk_deferCount == VK_DEFER_MAX) {
		Vk_Flush();
	}
	vk_defer[vk_deferCount].serial = vk.serial;
	vk_defer[vk_deferCount].buf = b ? *b : (VkBuf) { 0 };
	vk_defer[vk_deferCount].img = im ? *im : (VkImg) { 0 };
	vk_deferCount++;
}

void Vk_DeferBuffer(VkBuf *b) {
	if (b->buf) {
		Vk_Defer(b, NULL);
	}
	memset(b, 0, sizeof(*b));
}

void Vk_DeferImage(VkImg *im) {
	if (im->img) {
		Vk_Defer(NULL, im);
	}
	memset(im, 0, sizeof(*im));
}

int Vk_RingReserve(VkDeviceSize size) {
	Vk_Cmd();
	if (size > VK_RING_SIZE) {
		return 0;
	}
	if (vk.frames[vk.frame].ringUsed + size > VK_RING_SIZE) {
		/* the other frame's ring is free once its fence is done, which Vk_Cmd waits for */
		Vk_Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
		Vk_Cmd();
	}
	return 1;
}

void *Vk_RingAlloc(VkDeviceSize size, VkDeviceSize align, VkBuffer *buf, VkDeviceSize *off) {
	if (!Vk_RingReserve(size + align)) {
		return NULL;
	}
	VkFrame *f = &vk.frames[vk.frame];
	VkDeviceSize at = (f->ringUsed + align - 1) & ~(align - 1);
	f->ringUsed = at + size;
	*buf = f->ring.buf;
	*off = at;
	return f->ring.map + at;
}

/* the stages and accesses that use an image in layout */
static void Vk_LayoutUse(VkImageLayout layout, VkPipelineStageFlags2 *stage, VkAccessFlags2 *access) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		*stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		*access = VK_ACCESS_2_TRANSFER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		*stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		*access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		*stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		*access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		*stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		*access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		*stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		*access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		*stage = VK_PIPELINE_STAGE_2_NONE;
		*access = VK_ACCESS_2_NONE;
		break;
	default:
		*stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		break;
	}
}

void Vk_Transition(VkCommandBuffer cmd, VkImg *im, VkImageLayout layout) {
	if (im->layout == layout && layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		&& layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		return;
	}
	VkImageMemoryBarrier2 b = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.oldLayout = im->layout,
		.newLayout = layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = im->img,
		.subresourceRange = { im->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, 1 },
	};
	Vk_LayoutUse(im->layout, &b.srcStageMask, &b.srcAccessMask);
	Vk_LayoutUse(layout, &b.dstStageMask, &b.dstAccessMask);
	if (im->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
		b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		b.srcAccessMask = VK_ACCESS_2_NONE;
	}
	if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	VkDependencyInfo di = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &b,
	};
	vkCmdPipelineBarrier2(cmd, &di);
	im->layout = layout;
}
