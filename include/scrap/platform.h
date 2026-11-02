/*
 * The platform layer on SDL3 (src/game/App/Platform.c): the window, the events of the message loop, the timers and
 * the Vulkan surface, in place of the Win32 window procedures, the message loop and the timer functions.
 */
#ifndef SCRAP_PLATFORM_H
#define SCRAP_PLATFORM_H

#include "scrap/shared.h"

/* SDL with its video and gamepad parts; 0 on failure (the reason in the log) */
int Plat_Init(void);
void Plat_Shutdown(void);
/* the window with a client area of w x h (resizable unless fixed), hidden until Plat_ShowWindow; 0 on failure */
int Plat_CreateWindow(const char *title, int w, int h, byte fixed);
void Plat_ShowWindow(void);
/* the SDL_Window, and the HWND of it on Windows (NULL elsewhere) for the Win32 code that is left */
void *Plat_Window(void);
void *Plat_NativeWindow(void);
/* the client area in pixels, the window rectangle on the desktop (left, top, right, bottom) */
void Plat_ClientSize(int *w, int *h);
void Plat_WindowRect(int *r);
/* the window over the whole screen (borderless, the present scales the back buffer) or back */
void Plat_SetFullscreen(byte on);
void Plat_SetTitle(const char *title);
void Plat_ShowCursor(byte on);

/*
 * The events since the last call, handled: the window (focus, size, close), the keys, the text and the mouse buttons.
 * While waiting the keyboard and mouse events stay queued for later (the original handled no message while it
 * waited). 1 when the window was closed.
 */
byte Plat_PumpEvents(byte waiting);
/* the window closes at the next Plat_PumpEvents */
void Plat_Quit(void);

/* ms since the start (timeGetTime), the performance counter and its frequency (QueryPerformanceCounter/Frequency) */
uint32_t Plat_Ticks(void);
int Plat_Counter(int64_t *out);
int Plat_Frequency(int64_t *out);

/* Vulkan through SDL: the loader, the instance extensions the surface needs, the surface of the window */
void *Plat_VulkanGetInstanceProcAddr(void);
const char *const *Plat_VulkanExtensions(uint32_t *count);
int Plat_VulkanSurface(void *instance, void *surface);

#endif
