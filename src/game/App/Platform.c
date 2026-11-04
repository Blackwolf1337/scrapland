/*
 * The platform layer on SDL3, see include/scrap/platform.h. The events go straight to the game: the window events to
 * the application and the engine (what WM_ACTIVATEAPP, WM_ACTIVATE, WM_SIZE and WM_EXITSIZEMOVE did in the window
 * procedures), the keys and the text to the keyboard of the engine and the console (WM_KEYDOWN, WM_KEYUP, WM_CHAR),
 * and the mouse motion and the pads to the input state the devices of the game read. The hotkeys of the accelerator table of the original stay: Shift+Esc quits, F12 starts the tiled screenshot,
 * Ctrl+Shift+Alt+Enter switches fullscreen.
 */
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/dxutil.h"
#include "scrap/input.h"
#include "scrap/platform.h"
#include "scrap/rdev.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/staticinit.h"

#define g_InactiveBackgroundApp (GVar_InactiveBackgroundApp.value.i)	/* the frames go on without the focus */
static cGVar GVar_InactiveBackgroundApp;
#define PLAT_PADS 4
#define PLAT_NOTCONNECTED 0x48f		/* ERROR_DEVICE_NOT_CONNECTED of XInputGetState */

static struct {
	SDL_Window *window;
	byte modeSet;
	int mode, reqW, reqH;		/* of the last Plat_SetWindowMode */
	uint64_t start;
	int clientW, clientH;		/* of the last resize handled */
	float mouseX, mouseY, wheel;	/* the motion not read yet */
	SDL_Gamepad *pads[PLAT_PADS];
	exe_xinput_state padLast[PLAT_PADS];	/* for the packet number */
} plat;

/* ---- the keys: SDL scancode to the DirectInput code (the scan code, bit 7 for the extended keys) and the VK ---- */

typedef struct {
	byte dik, vk;
} PlatKey;

static const PlatKey plat_keys[SDL_SCANCODE_COUNT] = {
	[SDL_SCANCODE_ESCAPE] = { 0x01, 0x1b }, [SDL_SCANCODE_1] = { 0x02, '1' }, [SDL_SCANCODE_2] = { 0x03, '2' },
	[SDL_SCANCODE_3] = { 0x04, '3' }, [SDL_SCANCODE_4] = { 0x05, '4' }, [SDL_SCANCODE_5] = { 0x06, '5' },
	[SDL_SCANCODE_6] = { 0x07, '6' }, [SDL_SCANCODE_7] = { 0x08, '7' }, [SDL_SCANCODE_8] = { 0x09, '8' },
	[SDL_SCANCODE_9] = { 0x0a, '9' }, [SDL_SCANCODE_0] = { 0x0b, '0' }, [SDL_SCANCODE_MINUS] = { 0x0c, 0xbd },
	[SDL_SCANCODE_EQUALS] = { 0x0d, 0xbb }, [SDL_SCANCODE_BACKSPACE] = { 0x0e, 0x08 },
	[SDL_SCANCODE_TAB] = { 0x0f, 0x09 }, [SDL_SCANCODE_Q] = { 0x10, 'Q' }, [SDL_SCANCODE_W] = { 0x11, 'W' },
	[SDL_SCANCODE_E] = { 0x12, 'E' }, [SDL_SCANCODE_R] = { 0x13, 'R' }, [SDL_SCANCODE_T] = { 0x14, 'T' },
	[SDL_SCANCODE_Y] = { 0x15, 'Y' }, [SDL_SCANCODE_U] = { 0x16, 'U' }, [SDL_SCANCODE_I] = { 0x17, 'I' },
	[SDL_SCANCODE_O] = { 0x18, 'O' }, [SDL_SCANCODE_P] = { 0x19, 'P' }, [SDL_SCANCODE_LEFTBRACKET] = { 0x1a, 0xdb },
	[SDL_SCANCODE_RIGHTBRACKET] = { 0x1b, 0xdd }, [SDL_SCANCODE_RETURN] = { 0x1c, 0x0d },
	[SDL_SCANCODE_LCTRL] = { 0x1d, 0x11 }, [SDL_SCANCODE_A] = { 0x1e, 'A' }, [SDL_SCANCODE_S] = { 0x1f, 'S' },
	[SDL_SCANCODE_D] = { 0x20, 'D' }, [SDL_SCANCODE_F] = { 0x21, 'F' }, [SDL_SCANCODE_G] = { 0x22, 'G' },
	[SDL_SCANCODE_H] = { 0x23, 'H' }, [SDL_SCANCODE_J] = { 0x24, 'J' }, [SDL_SCANCODE_K] = { 0x25, 'K' },
	[SDL_SCANCODE_L] = { 0x26, 'L' }, [SDL_SCANCODE_SEMICOLON] = { 0x27, 0xba },
	[SDL_SCANCODE_APOSTROPHE] = { 0x28, 0xde }, [SDL_SCANCODE_GRAVE] = { 0x29, 0xc0 },
	[SDL_SCANCODE_LSHIFT] = { 0x2a, 0x10 }, [SDL_SCANCODE_BACKSLASH] = { 0x2b, 0xdc }, [SDL_SCANCODE_Z] = { 0x2c, 'Z' },
	[SDL_SCANCODE_X] = { 0x2d, 'X' }, [SDL_SCANCODE_C] = { 0x2e, 'C' }, [SDL_SCANCODE_V] = { 0x2f, 'V' },
	[SDL_SCANCODE_B] = { 0x30, 'B' }, [SDL_SCANCODE_N] = { 0x31, 'N' }, [SDL_SCANCODE_M] = { 0x32, 'M' },
	[SDL_SCANCODE_COMMA] = { 0x33, 0xbc }, [SDL_SCANCODE_PERIOD] = { 0x34, 0xbe }, [SDL_SCANCODE_SLASH] = { 0x35, 0xbf },
	[SDL_SCANCODE_RSHIFT] = { 0x36, 0x10 }, [SDL_SCANCODE_KP_MULTIPLY] = { 0x37, 0x6a },
	[SDL_SCANCODE_LALT] = { 0x38, 0x12 }, [SDL_SCANCODE_SPACE] = { 0x39, 0x20 },
	[SDL_SCANCODE_CAPSLOCK] = { 0x3a, 0x14 }, [SDL_SCANCODE_F1] = { 0x3b, 0x70 }, [SDL_SCANCODE_F2] = { 0x3c, 0x71 },
	[SDL_SCANCODE_F3] = { 0x3d, 0x72 }, [SDL_SCANCODE_F4] = { 0x3e, 0x73 }, [SDL_SCANCODE_F5] = { 0x3f, 0x74 },
	[SDL_SCANCODE_F6] = { 0x40, 0x75 }, [SDL_SCANCODE_F7] = { 0x41, 0x76 }, [SDL_SCANCODE_F8] = { 0x42, 0x77 },
	[SDL_SCANCODE_F9] = { 0x43, 0x78 }, [SDL_SCANCODE_F10] = { 0x44, 0x79 },
	[SDL_SCANCODE_NUMLOCKCLEAR] = { 0x45, 0x90 }, [SDL_SCANCODE_SCROLLLOCK] = { 0x46, 0x91 },
	[SDL_SCANCODE_KP_7] = { 0x47, 0x67 }, [SDL_SCANCODE_KP_8] = { 0x48, 0x68 }, [SDL_SCANCODE_KP_9] = { 0x49, 0x69 },
	[SDL_SCANCODE_KP_MINUS] = { 0x4a, 0x6d }, [SDL_SCANCODE_KP_4] = { 0x4b, 0x64 }, [SDL_SCANCODE_KP_5] = { 0x4c, 0x65 },
	[SDL_SCANCODE_KP_6] = { 0x4d, 0x66 }, [SDL_SCANCODE_KP_PLUS] = { 0x4e, 0x6b }, [SDL_SCANCODE_KP_1] = { 0x4f, 0x61 },
	[SDL_SCANCODE_KP_2] = { 0x50, 0x62 }, [SDL_SCANCODE_KP_3] = { 0x51, 0x63 }, [SDL_SCANCODE_KP_0] = { 0x52, 0x60 },
	[SDL_SCANCODE_KP_PERIOD] = { 0x53, 0x6e }, [SDL_SCANCODE_NONUSBACKSLASH] = { 0x56, 0xe2 },
	[SDL_SCANCODE_F11] = { 0x57, 0x7a }, [SDL_SCANCODE_F12] = { 0x58, 0x7b },
	[SDL_SCANCODE_KP_ENTER] = { 0x9c, 0x0d }, [SDL_SCANCODE_RCTRL] = { 0x9d, 0x11 },
	[SDL_SCANCODE_KP_DIVIDE] = { 0xb5, 0x6f }, [SDL_SCANCODE_PRINTSCREEN] = { 0xb7, 0x2c },
	[SDL_SCANCODE_RALT] = { 0xb8, 0x12 }, [SDL_SCANCODE_PAUSE] = { 0xc5, 0x13 }, [SDL_SCANCODE_HOME] = { 0xc7, 0x24 },
	[SDL_SCANCODE_UP] = { 0xc8, 0x26 }, [SDL_SCANCODE_PAGEUP] = { 0xc9, 0x21 }, [SDL_SCANCODE_LEFT] = { 0xcb, 0x25 },
	[SDL_SCANCODE_RIGHT] = { 0xcd, 0x27 }, [SDL_SCANCODE_END] = { 0xcf, 0x23 }, [SDL_SCANCODE_DOWN] = { 0xd0, 0x28 },
	[SDL_SCANCODE_PAGEDOWN] = { 0xd1, 0x22 }, [SDL_SCANCODE_INSERT] = { 0xd2, 0x2d },
	[SDL_SCANCODE_DELETE] = { 0xd3, 0x2e }, [SDL_SCANCODE_LGUI] = { 0xdb, 0x5b }, [SDL_SCANCODE_RGUI] = { 0xdc, 0x5c },
	[SDL_SCANCODE_APPLICATION] = { 0xdd, 0x5d },
};

/* the lparam of a key message: the scan code in bits 16..23, the extended flag in bit 24, the repeat in bit 30 */
static uint32_t Plat_KeyParam(byte dik, byte repeat) {
	return (uint32_t)(dik & 0x7f) << 16 | (dik & 0x80 ? 1u << 24 : 0) | (repeat ? 1u << 30 : 0) | 1;
}

/* ---- the start ---- */

int Plat_Init(void) {
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		Com_Printf("SDL_Init failed: %s\n", SDL_GetError());
		return 0;
	}
	plat.start = SDL_GetTicks();
	return 1;
}

void Plat_Shutdown(void) {
	for (int i = 0; i < PLAT_PADS; i++) {
		if (plat.pads[i]) {
			SDL_CloseGamepad(plat.pads[i]);
			plat.pads[i] = NULL;
		}
	}
	if (plat.window) {
		SDL_DestroyWindow(plat.window);
		plat.window = NULL;
	}
	SDL_Quit();
}

int Plat_CreateWindow(const char *title, int w, int h, byte fixed) {
	SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | (fixed ? 0 : SDL_WINDOW_RESIZABLE);
	plat.window = SDL_CreateWindow(title, w, h, flags);
	if (!plat.window) {
		Com_Printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 0;
	}
	SDL_SetWindowMinimumSize(plat.window, 100, 100);
	SDL_StartTextInput(plat.window);
	plat.clientW = w;
	plat.clientH = h;
	return 1;
}

void Plat_ShowWindow(void) {
	SDL_ShowWindow(plat.window);
	SDL_RaiseWindow(plat.window);
}

void *Plat_NativeWindow(void) {
#ifdef _WIN32
	return plat.window ? SDL_GetPointerProperty(SDL_GetWindowProperties(plat.window), SDL_PROP_WINDOW_WIN32_HWND_POINTER,
		NULL) : NULL;
#else
	return NULL;
#endif
}

void Plat_ClientSize(int *w, int *h) {
	*w = *h = 0;
	if (plat.window) {
		SDL_GetWindowSizeInPixels(plat.window, w, h);
	}
}

void Plat_WindowRect(int *r) {
	int x = 0, y = 0, w = 0, h = 0;
	if (plat.window) {
		SDL_GetWindowPosition(plat.window, &x, &y);
		SDL_GetWindowSize(plat.window, &w, &h);
	}
	r[0] = x;
	r[1] = y;
	r[2] = x + w;
	r[3] = y + h;
}

void Plat_SetWindowMode(int mode, int w, int h) {
	if (!plat.window || (plat.modeSet && mode == plat.mode && (mode == PLAT_BORDERLESS || (w == plat.reqW &&
		h == plat.reqH)))) {
		return;
	}
	SDL_DisplayID display = SDL_GetDisplayForWindow(plat.window);
	if (mode == PLAT_WINDOWED) {
		SDL_SetWindowFullscreen(plat.window, false);
		SDL_SyncWindow(plat.window);
		SDL_SetWindowSize(plat.window, w, h);
		SDL_SetWindowPosition(plat.window, (int)SDL_WINDOWPOS_CENTERED_DISPLAY(display),
			(int)SDL_WINDOWPOS_CENTERED_DISPLAY(display));
	} else {
		SDL_DisplayMode m;
		const SDL_DisplayMode *use = NULL;		/* NULL: the desktop, no mode switch */
		if (mode == PLAT_FULLSCREEN && SDL_GetClosestFullscreenDisplayMode(display, w, h, 0.0f, false, &m)) {
			use = &m;
		}
		/* a hidden window goes fullscreen only when it is shown: the size of the desktop already, so the first
		 * back buffer has it */
		const SDL_DisplayMode *desk = SDL_GetDesktopDisplayMode(display);
		if (mode == PLAT_BORDERLESS && desk) {
			SDL_SetWindowSize(plat.window, desk->w, desk->h);
		}
		SDL_SetWindowFullscreenMode(plat.window, use);
		SDL_SetWindowFullscreen(plat.window, true);
	}
	SDL_SyncWindow(plat.window);
	plat.modeSet = 1;
	plat.mode = mode;
	plat.reqW = w;
	plat.reqH = h;
	Plat_ClientSize(&plat.clientW, &plat.clientH);
}

int Plat_DisplayModes(int *wh, int max) {
	SDL_DisplayID display = SDL_GetPrimaryDisplay();
	int count = 0, n = 0;
	SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &count);

	for (int i = 0; modes && i < count && n < max; i++) {
		int k;
		for (k = 0; k < n && (wh[2 * k] != modes[i]->w || wh[2 * k + 1] != modes[i]->h); k++) {
		}
		if (k == n) {
			wh[2 * n] = modes[i]->w;
			wh[2 * n + 1] = modes[i]->h;
			n++;
		}
	}
	SDL_free(modes);
	if (!n && max > 0) {
		const SDL_DisplayMode *d = SDL_GetDesktopDisplayMode(display);
		if (d) {
			wh[0] = d->w;
			wh[1] = d->h;
			n = 1;
		}
	}
	return n;
}

void Plat_DesktopSize(int *w, int *h) {
	const SDL_DisplayMode *d = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	*w = d ? d->w : 640;
	*h = d ? d->h : 480;
}

void Plat_SetTitle(const char *title) {
	if (plat.window) {
		SDL_SetWindowTitle(plat.window, title);
	}
}

void Plat_ShowCursor(byte on) {
	if (on) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

/* ---- the input ---- */

void Plat_KeyStates(byte *keys) {
	int n = 0;
	const bool *down = SDL_GetKeyboardState(&n);

	memset(keys, 0, 256);
	for (int sc = 0; sc < n && sc < SDL_SCANCODE_COUNT; sc++) {
		if (down[sc] && plat_keys[sc].dik) {
			keys[plat_keys[sc].dik] = 0x80;
		}
	}
}

const char *Plat_KeyLabel(int dik) {
	static char label[64];

	label[0] = 0;
	for (int sc = 0; sc < SDL_SCANCODE_COUNT; sc++) {
		if (dik && plat_keys[sc].dik == dik) {
			SDL_strlcpy(label, SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, false)),
				sizeof(label));
			break;
		}
	}
	return label;
}

byte Plat_KeyDown(int vk) {
	int n = 0;

	SDL_PumpEvents();
	const bool *down = SDL_GetKeyboardState(&n);
	for (int sc = 0; sc < n && sc < SDL_SCANCODE_COUNT; sc++) {
		if (down[sc] && plat_keys[sc].vk == vk) {
			return 1;
		}
	}
	return 0;
}

void Plat_GrabMouse(byte on) {
	if (plat.window) {
		SDL_SetWindowRelativeMouseMode(plat.window, on && Plat_HasFocus());
	}
}

byte Plat_MouseGrabbed(void) {
	return plat.window && SDL_GetWindowRelativeMouseMode(plat.window);
}

void Plat_MouseState(int32_t *dx, int32_t *dy, int32_t *dz, byte *buttons) {
	SDL_MouseButtonFlags b = SDL_GetMouseState(NULL, NULL);

	*dx = (int32_t)plat.mouseX;
	*dy = (int32_t)plat.mouseY;
	*dz = (int32_t)(plat.wheel * 120.0f);
	plat.mouseX -= (float)*dx;
	plat.mouseY -= (float)*dy;
	plat.wheel -= (float)*dz / 120.0f;
	buttons[0] = b & SDL_BUTTON_LMASK ? 0x80 : 0;
	buttons[1] = b & SDL_BUTTON_RMASK ? 0x80 : 0;
	buttons[2] = b & SDL_BUTTON_MMASK ? 0x80 : 0;
	buttons[3] = b & SDL_BUTTON_X1MASK ? 0x80 : 0;
}

/* a stick axis of SDL (down positive) as XInput's (up positive) */
static int16_t Plat_AxisUp(Sint16 v) {
	return (int16_t)(v == -32768 ? 32767 : -v);
}

uint32_t Plat_PadState(int i, void *state) {
	static const struct {
		SDL_GamepadButton b;
		uint16_t x;
	} map[] = {
		{ SDL_GAMEPAD_BUTTON_DPAD_UP, 0x1 }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0x2 },
		{ SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0x4 }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0x8 },
		{ SDL_GAMEPAD_BUTTON_START, 0x10 }, { SDL_GAMEPAD_BUTTON_BACK, 0x20 },
		{ SDL_GAMEPAD_BUTTON_LEFT_STICK, 0x40 }, { SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0x80 },
		{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0x100 }, { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0x200 },
		{ SDL_GAMEPAD_BUTTON_SOUTH, 0x1000 }, { SDL_GAMEPAD_BUTTON_EAST, 0x2000 },
		{ SDL_GAMEPAD_BUTTON_WEST, 0x4000 }, { SDL_GAMEPAD_BUTTON_NORTH, 0x8000 },
	};
	exe_xinput_state *st = state;
	SDL_Gamepad *g = i >= 0 && i < PLAT_PADS ? plat.pads[i] : NULL;

	memset(st, 0, sizeof(*st));
	if (!g) {
		return PLAT_NOTCONNECTED;
	}
	for (size_t k = 0; k < sizeof(map) / sizeof(map[0]); k++) {
		if (SDL_GetGamepadButton(g, map[k].b)) {
			st->buttons |= map[k].x;
		}
	}
	st->leftTrigger = (byte)(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) * 255 / 32767);
	st->rightTrigger = (byte)(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) * 255 / 32767);
	st->thumbLX = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX);
	st->thumbLY = Plat_AxisUp(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY));
	st->thumbRX = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX);
	st->thumbRY = Plat_AxisUp(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY));
	/* the packet number changes with the state, as XInput's */
	exe_xinput_state *last = &plat.padLast[i];
	st->packet = last->packet;
	if (memcmp(st, last, sizeof(*st))) {
		st->packet++;
		*last = *st;
	}
	return 0;
}

byte Plat_HasFocus(void) {
	return plat.window && (SDL_GetWindowFlags(plat.window) & SDL_WINDOW_INPUT_FOCUS);
}

/* a pad connected: the first free slot; one removed: its slot free */
static void Plat_PadAdded(SDL_JoystickID id) {
	for (int i = 0; i < PLAT_PADS; i++) {
		if (!plat.pads[i]) {
			plat.pads[i] = SDL_OpenGamepad(id);
			return;
		}
	}
}

static void Plat_PadRemoved(SDL_JoystickID id) {
	for (int i = 0; i < PLAT_PADS; i++) {
		if (plat.pads[i] && SDL_GetGamepadID(plat.pads[i]) == id) {
			SDL_CloseGamepad(plat.pads[i]);
			plat.pads[i] = NULL;
		}
	}
}

/* ---- the events ---- */

/* the focus: the input released, the app (in)active (without the focus only with InactiveBackgroundApp), the
 * engine told (WM_ACTIVATE and WM_ACTIVATEAPP of the original) */
static void Plat_Focus(byte on) {
	if (on) {
		Input_ReleaseAll(0);
	}
	if (g_pApp && (g_InactiveBackgroundApp || on)) {
		g_pApp->active = on;
	}
	D3DApp_OnActivate();
}

/* a new client size: the back buffer of that size for a window (WM_SIZE and WM_EXITSIZEMOVE of the original) */
static void Plat_Resized(void) {
	CD3DApplication *e = g_Engine;
	int w, h;

	Plat_ClientSize(&w, &h);
	if (!e || !w || !h || (w == plat.clientW && h == plat.clientH)) {
		return;
	}
	plat.clientW = w;
	plat.clientH = h;
	int r[4];
	Plat_WindowRect(r);
	e->windowBounds = (exe_rect) { r[0], r[1], r[2], r[3] };
	e->windowClient = (exe_rect) { 0, 0, w, h };
	if (e->active && e->windowed && e->ready && e->device) {
		e->ready = 0;
		e->presentParams.BackBufferWidth = (uint32_t)w;
		e->presentParams.BackBufferHeight = (uint32_t)h;
		if (CD3DApp_Reset(e) < 0) {
			CD3DApp_DisplayErrorMsg(0x8200000c);		/* D3DAPPERR_RESIZEFAILED */
			return;
		}
		e->ready = 1;
	}
}

/* the hotkeys of the accelerator table 200 of the original; 1 when the key was one */
static byte Plat_Hotkey(SDL_Scancode sc, SDL_Keymod mod) {
	byte shift = (mod & SDL_KMOD_SHIFT) != 0, ctrl = (mod & SDL_KMOD_CTRL) != 0, alt = (mod & SDL_KMOD_ALT) != 0;
	CD3DApplication *e = g_Engine;

	if (sc == SDL_SCANCODE_ESCAPE && shift && !ctrl && !alt) {
		Plat_Quit();
		return 1;
	}
	if (sc == SDL_SCANCODE_F12 && !shift && !ctrl && !alt) {
		Engine_ToggleOption(RENDERMODE_TILE);
		return 1;
	}
	if (sc == SDL_SCANCODE_RETURN && shift && ctrl && alt) {
		if (e && e->active && e->ready && CD3DApp_ToggleFullscreen(e) < 0) {
			Con_FatalErrorf("Could not reset the Direct3D device.");
		}
		return 1;
	}
	return 0;
}

/* a character of the text in Windows-1252, the font of the game; '?' for one it has not */
static int Plat_Cp1252(uint32_t cp) {
	static const uint16_t high[32] = { 0x20ac, 0, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030,
		0x0160, 0x2039, 0x0152, 0, 0x017d, 0, 0, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc,
		0x2122, 0x0161, 0x203a, 0x0153, 0, 0x017e, 0x0178 };
	if (cp < 0x80 || (cp >= 0xa0 && cp <= 0xff)) {
		return (int)cp;
	}
	for (int i = 0; i < 32; i++) {
		if (high[i] && high[i] == cp) {
			return 0x80 + i;
		}
	}
	return '?';
}

static void Plat_Text(const char *s) {
	while (*s) {
		uint32_t cp = (byte)*s++;
		int more = cp >= 0xf0 ? 3 : cp >= 0xe0 ? 2 : cp >= 0xc0 ? 1 : 0;
		cp &= more ? 0x3f >> more : 0x7f;
		for (; more && (*s & 0xc0) == 0x80; more--) {
			cp = cp << 6 | (byte)(*s++ & 0x3f);
		}
		Input_OnChar(Plat_Cp1252(cp));
	}
}

static void Plat_Key(const SDL_KeyboardEvent *k) {
	PlatKey key = plat_keys[k->scancode];
	byte alt = (k->mod & SDL_KMOD_ALT) != 0, ctrl = (k->mod & SDL_KMOD_CTRL) != 0;

	if (k->down && !k->repeat && Plat_Hotkey(k->scancode, k->mod)) {
		return;
	}
	if (!key.dik) {
		return;
	}
	/* the keys with Alt (and F10) were WM_SYSKEYDOWN in the original, which the keyboard of the engine did not see */
	if (alt || k->scancode == SDL_SCANCODE_F10) {
		return;
	}
	if (!k->down) {
		Keyboard_KeyUp(key.vk, Plat_KeyParam(key.dik, 1));
		return;
	}
	Keyboard_KeyDown(key.vk, Plat_KeyParam(key.dik, k->repeat));
	/* the characters WM_CHAR had for these keys and for Ctrl with a letter, which the text events do not have */
	if (key.vk == 0x08 || key.vk == 0x09 || key.vk == 0x0d || key.vk == 0x1b) {
		Input_OnChar(key.vk);
	} else if (ctrl && key.vk >= 'A' && key.vk <= 'Z') {
		Input_OnChar(key.vk - 'A' + 1);
	}
}

static void Plat_Event(const SDL_Event *ev, byte *quit) {
	switch (ev->type) {
	case SDL_EVENT_QUIT:
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		*quit = 1;
		break;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		Plat_Focus(1);
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		Plat_Focus(0);
		break;
	case SDL_EVENT_WINDOW_MINIMIZED:
		if (g_Engine) {
			g_Engine->active = 0;
		}
		break;
	case SDL_EVENT_WINDOW_RESTORED:
	case SDL_EVENT_WINDOW_MAXIMIZED:
		if (g_Engine) {
			g_Engine->active = 1;
		}
		Plat_Resized();
		break;
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		Plat_Resized();
		break;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		Plat_Key(&ev->key);
		break;
	case SDL_EVENT_TEXT_INPUT:
		Plat_Text(ev->text.text);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		plat.wheel += ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -ev->wheel.y : ev->wheel.y;
		break;
	case SDL_EVENT_GAMEPAD_ADDED:
		Plat_PadAdded(ev->gdevice.which);
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		Plat_PadRemoved(ev->gdevice.which);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		plat.mouseX += ev->motion.xrel;
		plat.mouseY += ev->motion.yrel;
		if (g_Engine && g_Engine->active && g_Engine->ready && g_Engine->device) {
			Rd_DevSetCursorPosition(g_Engine->device, (int)ev->motion.x, (int)ev->motion.y, 0);
		}
		break;
	}
}

byte Plat_PumpEvents(byte waiting) {
	SDL_Event ev;
	byte quit = 0;

	SDL_PumpEvents();
	if (!waiting) {
		while (SDL_PollEvent(&ev)) {
			Plat_Event(&ev, &quit);
		}
		return quit;
	}
	/* the keyboard and mouse events (SDL_EVENT_KEY_DOWN .. SDL_EVENT_MOUSE_REMOVED) stay for after the wait */
	while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_KEY_DOWN - 1) > 0
		|| SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_EVENT_MOUSE_REMOVED + 1, SDL_EVENT_LAST) > 0) {
		Plat_Event(&ev, &quit);
	}
	return quit;
}

void Plat_Quit(void) {
	SDL_Event q = { .type = SDL_EVENT_QUIT };
	SDL_PushEvent(&q);
}

/* ---- message boxes and threads ---- */

int Plat_MessageBox(const char *text, const char *title, uint32_t flags) {
	static const SDL_MessageBoxButtonData ok[] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "OK" },
	};
	static const SDL_MessageBoxButtonData yesNo[] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 6, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 7, "No" },
	};
	byte yn = (flags & 0xf) == 4;
	uint32_t icon = flags & 0xf0;
	SDL_MessageBoxData box = {
		.flags = icon == 0x10 ? SDL_MESSAGEBOX_ERROR : icon == 0x30 ? SDL_MESSAGEBOX_WARNING
			: SDL_MESSAGEBOX_INFORMATION,
		.window = plat.window,
		.title = title,
		.message = text,
		.numbuttons = yn ? 2 : 1,
		.buttons = yn ? yesNo : ok,
	};
	int id = -1;
	byte grabbed = Plat_MouseGrabbed(), shown = SDL_CursorVisible();
	const char *quiet = SDL_getenv("SCRAP_NO_MSGBOX");

	/* the tests run without anybody to press a button: the text into the log and the default answer */
	if (quiet && *quiet == '1') {
		Com_Printf("message box: %s: %s\n", title, text);
		return yn ? 7 : 1;
	}

	/* the box needs the cursor (ShowCursor(1) around it in the original) */
	Plat_GrabMouse(0);
	SDL_ShowCursor();
	if (!SDL_ShowMessageBox(&box, &id)) {
		Com_Printf("%s: %s\n", title, text);
	}
	if (!shown) {
		SDL_HideCursor();
	}
	Plat_GrabMouse(grabbed);
	return id < 0 ? (yn ? 7 : 1) : id;
}

void Plat_SetThreadPriority(int prio) {
	SDL_SetCurrentThreadPriority(prio < 0 ? SDL_THREAD_PRIORITY_LOW : SDL_THREAD_PRIORITY_NORMAL);
}

/* ---- paths ---- */

void Plat_ExePath(char *buf, int size) {
	buf[0] = 0;
#ifdef _WIN32
	DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)size);
	if (n >= (DWORD)size) {
		buf[0] = 0;
	}
#else
	ssize_t n = readlink("/proc/self/exe", buf, (size_t)size - 1);
	buf[n > 0 ? n : 0] = 0;
#endif
}

void Plat_ConfigDir(char *buf, int size) {
	buf[0] = 0;
#ifdef _WIN32
	const char *docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
	if (docs) {
		SDL_snprintf(buf, (size_t)size, "%sMercurySteam\\Scrapland\\", docs);
		SDL_CreateDirectory(buf);
	}
#else
	char *pref = SDL_GetPrefPath("MercurySteam", "Scrapland");
	if (pref) {
		SDL_strlcpy(buf, pref, (size_t)size);
		SDL_free(pref);
	}
#endif
}

const char *Plat_InstallLanguage(void) {
#ifdef _WIN32
	static char lang[0x200];
	HKEY key;
	DWORD type = 0, n = sizeof(lang) - 1;

	lang[0] = 0;
	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "Software\\MercurySteam Entertainment\\Scrapland", &key)) {
		return NULL;
	}
	LONG r = RegQueryValueExA(key, "LANGUAGE", NULL, &type, (BYTE *)lang, &n);
	RegCloseKey(key);
	if (r || type != REG_SZ) {
		return NULL;
	}
	lang[n < sizeof(lang) ? n : sizeof(lang) - 1] = 0;
	return lang[0] ? lang : NULL;
#else
	return NULL;
#endif
}

/* ---- files, memory, errors ---- */

uint32_t Plat_FileAttributes(const char *name) {
#ifdef _WIN32
	return GetFileAttributesA(name);
#else
	(void)name;
	return 0xffffffff;
#endif
}

int Plat_SetFileAttributes(const char *name, uint32_t attr) {
#ifdef _WIN32
	return SetFileAttributesA(name, attr) != 0;
#else
	(void)name, (void)attr;
	return 0;
#endif
}

uint64_t Plat_VirtualMemoryUsed(void) {
#ifdef _WIN32
	MEMORYSTATUSEX ms = { .dwLength = sizeof(ms) };
	return GlobalMemoryStatusEx(&ms) ? ms.ullTotalVirtual - ms.ullAvailVirtual : 0;
#else
	unsigned long pages = 0;
	FILE *f = fopen("/proc/self/statm", "r");
	if (f) {
		if (fscanf(f, "%lu", &pages) != 1) {
			pages = 0;
		}
		fclose(f);
	}
	return (uint64_t)pages * (uint64_t)sysconf(_SC_PAGESIZE);
#endif
}

uint32_t Plat_LastError(void) {
#ifdef _WIN32
	return GetLastError();
#else
	return (uint32_t)errno;
#endif
}

byte Plat_DebuggerPresent(void) {
#ifdef _WIN32
	return IsDebuggerPresent() != 0;
#else
	char line[128];
	int pid = 0;
	FILE *f = fopen("/proc/self/status", "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			if (sscanf(line, "TracerPid: %d", &pid) == 1) {
				break;
			}
		}
		fclose(f);
	}
	return pid != 0;
#endif
}

void Plat_DebugBreak(void) {
	SDL_TriggerBreakpoint();
}

/* ---- the timers ---- */

uint32_t Plat_Ticks(void) {
	return (uint32_t)SDL_GetTicks();
}

/* the counter in 100 ns ticks (10 MHz) on every system, as QueryPerformanceCounter on Windows: the game's integer
 * time math was made for that frequency (SDL's counter on Linux counts nanoseconds) */
int Plat_Counter(int64_t *out) {
	*out = (int64_t)(SDL_GetTicksNS() / 100);
	return 1;
}

int Plat_Frequency(int64_t *out) {
	*out = 10000000;
	return 1;
}

/* ---- Vulkan ---- */

void *Plat_VulkanGetInstanceProcAddr(void) {
	if (!SDL_Vulkan_LoadLibrary(NULL)) {
		Com_Printf("vulkan: %s\n", SDL_GetError());
		return NULL;
	}
	return (void *)SDL_Vulkan_GetVkGetInstanceProcAddr();
}

const char *const *Plat_VulkanExtensions(uint32_t *count) {
	Uint32 n = 0;
	const char *const *ext = SDL_Vulkan_GetInstanceExtensions(&n);
	*count = n;
	return ext;
}

int Plat_VulkanSurface(void *instance, void *surface) {
	if (!SDL_Vulkan_CreateSurface(plat.window, (VkInstance)instance, NULL, (VkSurfaceKHR *)surface)) {
		Com_Printf("vulkan: SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
		return 0;
	}
	return 1;
}

static cGVar GVar_InactiveBackgroundApp;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_InactiveBackgroundApp, &GVar_InactiveBackgroundApp, "InactiveBackgroundApp", 4, "1=application inactive in background", 1)
