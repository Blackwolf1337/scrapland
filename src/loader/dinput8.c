/*
 * dinput8.dll proxy. Scrap.exe imports only DirectInput8Create from dinput8, and dinput8 is no KnownDLL, so this
 * file next to the exe is loaded instead of the system one (under Wine: WINEDLLOVERRIDES=dinput8=n,b). The COM
 * exports are forwarded too, for anything else that loads dinput8. The system DLL is loaded on the first call,
 * not in DllMain (loader lock).
 */
#include <stdio.h>
#include <windows.h>

#include "scrap/game.h"

static HMODULE real_dinput8;

static FARPROC Real(const char *name) {
	if (!real_dinput8) {
		char sys[MAX_PATH], path[MAX_PATH];
		UINT n = GetSystemDirectoryA(sys, sizeof(sys));
		if (n == 0 || n >= sizeof(sys)) {
			return NULL;
		}
		if (snprintf(path, sizeof(path), "%s\\dinput8.dll", sys) >= (int)sizeof(path)) {
			return NULL;
		}
		real_dinput8 = LoadLibraryA(path);
		if (!real_dinput8) {
			Com_Printf("dinput8: cannot load %s (%lu)\n", path, GetLastError());
			return NULL;
		}
	}
	return GetProcAddress(real_dinput8, name);
}

HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD version, const IID *riid, void **out, IUnknown *outer) {
	typedef HRESULT(WINAPI * fn_t)(HINSTANCE, DWORD, const IID *, void **, IUnknown *);
	fn_t fn = (fn_t)(void *)Real("DirectInput8Create");
	return fn ? fn(inst, version, riid, out, outer) : E_FAIL;
}

HRESULT WINAPI DllCanUnloadNow(void) {
	typedef HRESULT(WINAPI * fn_t)(void);
	fn_t fn = (fn_t)(void *)Real("DllCanUnloadNow");
	return fn ? fn() : S_FALSE;
}

HRESULT WINAPI DllGetClassObject(const CLSID *clsid, const IID *riid, void **out) {
	typedef HRESULT(WINAPI * fn_t)(const CLSID *, const IID *, void **);
	fn_t fn = (fn_t)(void *)Real("DllGetClassObject");
	return fn ? fn(clsid, riid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllRegisterServer(void) {
	typedef HRESULT(WINAPI * fn_t)(void);
	fn_t fn = (fn_t)(void *)Real("DllRegisterServer");
	return fn ? fn() : E_FAIL;
}

HRESULT WINAPI DllUnregisterServer(void) {
	typedef HRESULT(WINAPI * fn_t)(void);
	fn_t fn = (fn_t)(void *)Real("DllUnregisterServer");
	return fn ? fn() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved) {
	(void)reserved;
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(inst);
		Game_Init();
	}
	return TRUE;
}
