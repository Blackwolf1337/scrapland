/*
 * The Vulkan core of the render device backend (VkCore.c): instance, device, swapchain, the frames in flight with
 * their command buffers and a ring buffer each for the data of one frame (vertices of the UP draws, uniforms,
 * uploads), and the helpers for memory, buffers, images and layout transitions.
 */
#ifndef VKCORE_H
#define VKCORE_H

#define VK_NO_PROTOTYPES
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include "scrap/shared.h"

#define VKF_GLOBAL(name) extern PFN_##name name;
#define VKF_INSTANCE(name) extern PFN_##name name;
#define VKF_DEVICE(name) extern PFN_##name name;
#include "vkfuncs.h"
#undef VKF_GLOBAL
#undef VKF_INSTANCE
#undef VKF_DEVICE

#define VK_FRAMES 2				/* frames in flight */
#define VK_RING_SIZE (16u << 20)		/* the per frame ring buffer */
#define VK_MAX_SWAP_IMAGES 8

/* a buffer with its memory, mapped when host visible */
typedef struct VkBuf {
	VkBuffer buf;
	VkDeviceMemory mem;
	VkDeviceSize size;
	byte *map;
} VkBuf;

/* an image with its memory and a view of all levels */
typedef struct VkImg {
	VkImage img;
	VkDeviceMemory mem;
	VkImageView view;
	VkFormat format;
	VkImageAspectFlags aspect;
	uint32_t width, height, levels;
	VkImageLayout layout;			/* the layout of all levels after the last command */
} VkImg;

typedef struct VkFrame {
	VkCommandBuffer cmd;
	VkFence done;
	VkSemaphore acquired;
	VkBuf ring;
	VkDeviceSize ringUsed;
	uint64_t serial;			/* the frame counter when it was submitted */
} VkFrame;

typedef struct VkCore {
	VkInstance instance;
	VkPhysicalDevice gpu;
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceMemoryProperties memProps;
	VkDevice device;
	VkQueue queue;
	uint32_t queueFamily;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkFormat swapFormat;
	VkExtent2D swapExtent;
	uint32_t swapCount;
	VkImage swapImages[VK_MAX_SWAP_IMAGES];
	VkImageView swapViews[VK_MAX_SWAP_IMAGES];
	VkSemaphore swapRendered[VK_MAX_SWAP_IMAGES];	/* per image: a present may still wait on it */
	byte vsync;
	VkCommandPool pool;
	VkFrame frames[VK_FRAMES];
	uint32_t frame;				/* index into frames */
	uint64_t serial;			/* frames begun */
	uint64_t completed;			/* the last serial known to be done on the GPU */
	byte recording;
	VkFormat depthFormat;			/* D24S8 or D32S8, what the GPU has */
	void *hwnd;
	void (*endPass)(void);			/* ends an open render pass before a submit */
} VkCore;

extern VkCore vk;

/* the loader and the instance; 0 when there is no Vulkan */
int Vk_Init(void);
/* depth formats: whether one has stencil, whether the GPU renders into it */
int Vk_HasStencil(VkFormat f);
int Vk_IsDepth(VkFormat f);
int Vk_DepthSupported(VkFormat f);
/* the properties of the GPU Vk_CreateDevice will take (after Vk_Init); 0 when there is none */
int Vk_GpuProperties(VkPhysicalDeviceProperties *out);
/* the device and the swapchain on the window */
int Vk_CreateDevice(void *hwnd, uint32_t width, uint32_t height, byte vsync);
void Vk_DestroyDevice(void);
int Vk_ResizeSwapchain(uint32_t width, uint32_t height, byte vsync);

/* the command buffer of the frame being recorded, started on demand */
VkCommandBuffer Vk_Cmd(void);
/* the swapchain image to present into; 0 when there is none now (what is recorded is submitted then) */
int Vk_Acquire(uint32_t *index);
/* submits the frame, which has drawn into swapchain image index, and presents it; 0 on failure */
int Vk_QueuePresent(uint32_t index);
/* submits what is recorded and waits until the GPU is done (for a readback) */
void Vk_Flush(void);

/* memory and objects */
uint32_t Vk_MemoryType(uint32_t bits, VkMemoryPropertyFlags flags);
int Vk_CreateBuffer(VkBuf *b, VkDeviceSize size, VkBufferUsageFlags usage, int hostVisible);
void Vk_DestroyBuffer(VkBuf *b);
int Vk_CreateImage(VkImg *im, VkFormat format, uint32_t w, uint32_t h, uint32_t levels, VkImageUsageFlags usage,
	VkComponentMapping swizzle);
void Vk_DestroyImage(VkImg *im);
/* destroys the object once the frames that may use it are done */
void Vk_DeferBuffer(VkBuf *b);
void Vk_DeferImage(VkImg *im);

/*
 * Room in the ring of the current frame: the mapped pointer, the buffer and offset. A full ring submits what is
 * recorded and goes on in the next frame, so take the command buffer after it. NULL when size does not fit at all.
 */
void *Vk_RingAlloc(VkDeviceSize size, VkDeviceSize align, VkBuffer *buf, VkDeviceSize *off);
/* makes sure that size bytes (with the alignments of the allocations) fit into the ring without a submit */
int Vk_RingReserve(VkDeviceSize size);

/* a barrier that moves all levels of im to layout */
void Vk_Transition(VkCommandBuffer cmd, VkImg *im, VkImageLayout layout);

#endif
