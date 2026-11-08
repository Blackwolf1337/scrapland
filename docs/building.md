# Building and running

## Setup

```
innoextract -g -d game/ 'setup_scrapland_remastered_1.6.1_(52159).exe'
tools/test/prefix.sh                          # creates prefix/ (own Wine prefix, audio off)
```

## Build

There are two builds to play, each in its own folder, from the same sources: Windows 64 bit and Linux 64 bit. CMake refuses a 32 bit build. A third folder, `build-linux64-asan`, is the Linux build with AddressSanitizer for the level test (see [testing.md](testing.md)).

The build needs `glslangValidator` (Arch package `glslang`) for the shaders, SDL3 and an FFmpeg. cmake stops without them. Running needs a Vulkan 1.3 driver (under Wine: winevulkan).

Windows 64 bit:

```
tools/sdl3.sh                                 # build-win64/sdl3
tools/ffmpeg.sh                               # build-win64/ffmpeg, win64
cmake -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake && cmake --build build-win64
```

Put `SDL3.dll` and the FFmpeg DLLs from `build-win64/sdl3/install/bin` and `build-win64/ffmpeg/install/bin` next to `ScrapC.exe`.

Linux 64 bit (system SDL3):

```
tools/ffmpeg.sh build-linux64/ffmpeg linux64
cmake -B build-linux64 -DCMAKE_TOOLCHAIN_FILE=cmake/linux-x86_64.cmake && cmake --build build-linux64
```

The binary is `build-linux64/ScrapC`. Put the FFmpeg `.so` files from `build-linux64/ffmpeg/install/lib` next to it.

The builds have no Python interpreter by default (`SCRAP_NO_PYTHON` is ON since 2026-11-08). For one round a reference build with the interpreter can be configured, only for the test tools that need it (see [testing.md](testing.md)):

```
cmake -B build-py64 -DCMAKE_TOOLCHAIN_FILE=cmake/linux-x86_64.cmake -DSCRAP_NO_PYTHON=OFF && cmake --build build-py64
```

## Run

The game data are the packs of an installed Scrapland Remastered. The binary runs from the game's `Bin` folder, as the original `Scrap.exe`. On Linux the config and saves go to the SDL user data folder (`XDG_DATA_HOME`).

## Running under Wine

With the Windows 64 bit build:

```
tools/test/install.sh                         # copies build-win64/ScrapC.exe to game/Bin
tools/test/wine.sh ScrapC.exe -fullscreen:0   # hidden (gamescope headless) and muted
```

`install.sh` also copies the FFmpeg DLLs (`avcodec-61`, `avformat-61`, `avutil-59`, `swscale-8`) and `SDL3.dll` to `game/Bin`. A game install outside the repo needs them next to `ScrapC.exe` too.

`wine.sh` runs for `HEADLESS_TIMEOUT` seconds (default 300). Set `WINE_LOG=file` to keep the output.

## Renderer environment variables

See [vulkan.md](vulkan.md).

- `SCRAP_VK_DEVICE`: the GPU, as an index or as `vendor:device` in hex. `wine.sh` defaults to `1002:164e`.
- `SCRAP_VK_VALIDATION=1`: the Vulkan validation layer. `wine.sh` turns it on for the game only.
- `SCRAP_VK_TRACE=<seconds>`: the render calls of the first frame that starts that many seconds after the device is created go into `scrapdecomp.log`.
- `SCRAP_RD_DUMP=<dir>` with `SCRAP_RD_DUMP_EVERY=<n>`: every nth presented frame (default 100) as a BMP in `<dir>`.
