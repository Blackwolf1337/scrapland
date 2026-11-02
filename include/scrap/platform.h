/*
 * The platform layer on SDL3 (src/game/App/Platform.c): the window, the events of the message loop, the input, the
 * timers and the Vulkan surface, in place of the Win32 window procedures, the message loop, DirectInput, XInput and
 * the timer functions.
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
 * The events since the last call, handled: the window (focus, size, close), the keys, the text, the mouse motion
 * and the pads.
 * While waiting the keyboard and mouse events stay queued for later (the original handled no message while it
 * waited). 1 when the window was closed.
 */
byte Plat_PumpEvents(byte waiting);
/* the window closes at the next Plat_PumpEvents */
void Plat_Quit(void);

/*
 * The input in the forms the game reads (the DirectInput and XInput states of the original): the keys by DirectInput
 * code, 0x80 when down (256 bytes); the name of a key in the keyboard layout; a key by VK code down now, the events
 * pumped first (GetAsyncKeyState, for the loops that wait for a key without a frame).
 */
void Plat_KeyStates(byte *keys);
const char *Plat_KeyLabel(int dik);
byte Plat_KeyDown(int vk);
/* the mouse held by the game: relative motion, no cursor (DirectInput exclusive); only with the focus */
void Plat_GrabMouse(byte on);
byte Plat_MouseGrabbed(void);
/* the motion and the wheel (120 a notch) since the last call; the buttons left, right, middle, X1 (0x80 when down) */
void Plat_MouseState(int32_t *dx, int32_t *dy, int32_t *dz, byte *buttons);
/* pad i (0..3, in the order they were connected) in the layout of XINPUT_STATE; 0, else 0x48f (not connected) */
uint32_t Plat_PadState(int i, void *state);
byte Plat_HasFocus(void);

/*
 * A message box over the window with the flags of MessageBoxA (the icon 0x10 error, 0x20 question, 0x30 warning, 0x40
 * information; the buttons 0 OK, 4 Yes and No); its answer as MessageBoxA's: 1 OK, 6 Yes, 7 No.
 */
int Plat_MessageBox(const char *text, const char *title, uint32_t flags);
/* the priority of the calling thread: below normal (-1) or normal (0) */
void Plat_SetThreadPriority(int prio);

/* ms since the start (timeGetTime), the performance counter and its frequency (QueryPerformanceCounter/Frequency) */
uint32_t Plat_Ticks(void);
int Plat_Counter(int64_t *out);
int Plat_Frequency(int64_t *out);

/* Vulkan through SDL: the loader, the instance extensions the surface needs, the surface of the window */
void *Plat_VulkanGetInstanceProcAddr(void);
const char *const *Plat_VulkanExtensions(uint32_t *count);
int Plat_VulkanSurface(void *instance, void *surface);

#endif
