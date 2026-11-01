# Scrap.exe triage

## Files

Files in `game/Bin`:

| File | Size | Notes |
|------|------|-------|
| Scrap.exe | 4841984 | sha256 `127f04faa5324b0610f854e9fa0061987933972231c6f1f8b3e9349cad29ba33` |
| Dedicated.exe | 4793344 | sha256 `86642f743e7e4529186fee606af21c4ec2ac577ff7997d2c5ec8c3c3db27bb3b` |
| fmod.dll | 160768 | UPX packed, PE32 |
| Galaxy.dll | | GOG Galaxy SDK |
| steam_api.dll | | |

`game/Scrap.exe` in the root is byte identical to `Bin/Scrap.exe`. The GOG launcher playTasks path is `Bin/Scrap.exe`.

Dedicated.exe is the dedicated server. It has 323 RTTI names, imports XINPUT1_4 instead of XINPUT9_1_0, and has no Galaxy.dll. The rest of the DLL set is the same.

## PE header

| Field | Value |
|-------|-------|
| Format | PE32, Intel i386, GUI subsystem 6.0 |
| Linker | 14.0 (MSVC 2015 or newer toolset) |
| Link timestamp | 0x61b304cc (2021-12-10 07:42:04 UTC) |
| ImageBase | 0x00400000 |
| SizeOfImage | 0x55e000 |
| Entry point RVA | 0x314919 |
| DllCharacteristics | 0x8140: DYNAMIC_BASE (ASLR, has .reloc), NX_COMPAT, TERMINAL_SERVER_AWARE |

Hook code must compute addresses as module base + RVA. Never hardcode 0x4xxxxx absolute addresses. Ghidra addresses are stated relative to base 0x400000.

The CRT is statically linked (/MT). There are no UCRT or VCRUNTIME imports.

## Sections

| Name | VA | Virtual size | Notes |
|------|----|--------------|-------|
| .text | 0x401000 | 0x38e097 | |
| .bss_ste | 0x790000 | 0x102c0 | no raw data, name unexplained, maybe Steam related |
| .rdata | 0x7a1000 | 0x86f8a | |
| .data | 0x828000 | 0x1c200 | |
| .gfids | 0x8ee000 | 0x4f8 | Control Flow Guard function ids |
| .tls | 0x8ef000 | 0x41 | |
| _RDATA | 0x8f0000 | 0x12e0 | |
| .data1 | 0x8f2000 | 0x8e0 | |
| .rsrc | 0x8f3000 | 0x33680 | |
| .reloc | 0x927000 | 0x36b08 | |

## Debug info

CodeView RSDS, GUID `8239560daf44458095ecf404f284e79d`, age 14, PDB path `C:\Proyect6\SCRAPLAND\Game\Bin\Scrap.pdb`. The PDB is not shipped. There are also Feature and CoffGrp (POGO) debug entries.

## Imports

| DLL | Count | Notes |
|-----|-------|-------|
| KERNEL32 | 147 | |
| USER32 | 74 | |
| fmod.dll | 47 | FMOD 3.x style API, verify |
| GDI32 | 18 | |
| WSOCK32 | 16 | networking, Winsock 1.1 |
| AVIFIL32 | 11 | AVI video playback, plus 1 from MSVFW32 |
| Galaxy.dll | 8 | |
| steam_api.dll | 6 | |
| ADVAPI32 | 6 | |
| ole32 | 3 | |
| WINMM | 3 | timeGetTime, timeBeginPeriod, timeEndPeriod |
| d3d8 | 1 | Direct3DCreate8 |
| DDRAW | 1 | DirectDrawCreate |
| DINPUT8 | 1 | DirectInput8Create |
| XINPUT9_1_0 | 1 | XInputGetState |
| COMCTL32 | 1 | |
| COMDLG32 | 1 | |
| SHELL32 | 1 | |

## Engine

### Renderer

Direct3D 8. The engine log prints "Engine 3D initializing (Version 5624)" and reports vertex shader 1.1 and pixel shader 1.4 caps.

Engine self id from `log.html`: "SCRAP Ver: 1.1 (GMC3)  Nov 24 2021  15:24:17", "(c) MercurySteam Entertainment".

### Scripting

Python 1.5.2 is statically linked (version string 1.5.2, "[MSC 32 bit (Intel)]", Stichting Mathematisch Centrum copyright). These engine modules are visible as strings: Scrap, SAct, SInput, SLogic, SMerc, SNet, SPoli, SScorer, SSound, SVec, SWeap, SRadius, SMosquito, SLifeBoat, STrafficCapsule. To be confirmed in Ghidra by finding the Py_InitModule calls.

### RTTI

321 type descriptors (`.?AV` classes and `.?AU` structs), saved in `symbols/rtti_classes.txt`. Examples: cActClass, cAI, cAIChar, cAIVehicle, cAIPolice, CAStar, cBinaryHeap<ElemAStar>, cNetController. This gives class names and vtables for Ghidra.

## Hook DLL plan

This section is history. The hook DLL and the proxy are gone since 2026-11-01, see tag `hooked-final`. ScrapC.exe is the only build target.

### Proxy

The proxy is `dinput8.dll`. Scrap.exe imports only DirectInput8Create from it. It is not a KnownDLL, so a copy in `game/Bin` is loaded first. Under Wine it needs `WINEDLLOVERRIDES=dinput8=n,b`.

The proxy loads the system dinput8 and forwards DirectInput8Create, DllCanUnloadNow, DllGetClassObject, DllRegisterServer and DllUnregisterServer. Before installing hooks it checks the sha256 of Scrap.exe, or a cheaper fingerprint: link timestamp 0x61b304cc plus SizeOfImage 0x55e000.

### Toolchain

- The exe is 32-bit, so `i686-w64-mingw32-gcc`, C23.
- MSVC member functions are `__thiscall` (this in ECX). Use `__attribute__((thiscall))` on function pointer types and replacement functions.
- Static functions may be `__cdecl`, `__stdcall`, `__fastcall`, or a custom convention from MSVC whole-program optimization. The CoffGrp/POGO entry suggests PGO or LTCG. Check each function's convention in Ghidra and write it down.
- Struct layouts follow MSVC rules (default 8-byte packing, 4-byte pointers). Every struct gets `_Static_assert` checks on size and member offsets.

## Wine baseline

Setup: wine-11.20, own prefix `prefix/`, Windows 10 default, run headless in gamescope with audio disabled via `HKCU\Software\Wine\Drivers` Audio="".

The game starts. `log.html` (written to `game/`, the working directory's parent; also `logAI.html`) shows:

- config read from `C:\users\<user>\Documents\MercurySteam\Scrapland\Scrap.cfg`
- all packs mounted (Data, Data00-03, english)
- input devices (mouse, 4 pads, keyboard)
- D3D8 HAL on the NVIDIA GPU at 640x480 (window mode via `-fullscreen:0`)
- FMOD init with Direct Sound
- Python pools 621036 bytes, world and scorer initialized
- "Loading Graph 3D AI..."

After 90 seconds the process still ran and the log had no further lines. Whether it reaches the main menu is not confirmed (no screenshot from headless gamescope yet).

Intro videos fail: `Video/MSE.vds`, AMStream OpenFile HRESULT 0x80040217 (VFW_E_CANNOT_CONNECT). winegstreamer is missing a GStreamer plugin for AVI (video/x-msvideo). This matches WineHQ bug 57646 (intro videos broken on Wine 10).

The first test run with only the Wine audio driver DLLs disabled through WINEDLLOVERRIDES was still audible. The registry setting plus PULSE_SERVER and PIPEWIRE_REMOTE pointing to nonexistent sockets fixed it.

## Open questions

- Purpose of `.bss_ste`.
- Exact FMOD version.
- Whether the Python module list above is complete.
- Main menu reachability under Wine.
- How much of the 2004 ScrapHacks naming transfers to this build.
