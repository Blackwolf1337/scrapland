cmake_minimum_required(VERSION 3.25)
project(scrapdecomp C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # gnu23: naked functions, inline asm, calling convention attributes
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd in the editor, linked to the repo root

find_program(CCACHE ccache)
if(CCACHE)
	set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif()

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "The hook DLL is 32 bit Windows only: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake")
endif()

# the C code shared by the hook DLL and the standalone exe
add_library(scrapcode OBJECT
	src/core/d3dlog.c
	src/core/game.c
	src/core/hook.c
	src/core/imports.c
	src/core/libm.c
	src/core/log.c
	src/core/testmod.c
)
# the decompiled game code, in the folder layout of the original sources (paths from the asserts in Scrap.exe)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/game/*.c)
target_sources(scrapcode PRIVATE ${GAME_SOURCES})
target_include_directories(scrapcode PUBLIC
	include
	src
	third_party/python-1.5.2/config
	third_party/python-1.5.2/Include
)
# Scrap.exe does its float math with SSE2 (MSVC /arch:SSE2): same here, x87 80 bit intermediates would change
# the last bits of results
target_compile_options(scrapcode PUBLIC -Wall -Wextra -O2 -g -fno-omit-frame-pointer -fno-strict-aliasing
	-msse2 -mfpmath=sse -ffp-contract=off
	# sources are UTF-8, the game's strings Windows-1252 (Spanish messages of the original)
	-finput-charset=UTF-8 -fexec-charset=CP1252)
target_link_options(scrapcode PUBLIC -static-libgcc -Wl,--enable-stdcall-fixup)

# dinput8.dll: proxy for the system dinput8 that replaces Scrap.exe functions with the C versions of src/
add_library(dinput8 SHARED src/loader/dinput8.c src/loader/dinput8.def)
target_link_libraries(dinput8 PRIVATE scrapcode)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME dinput8)

# ScrapC.exe: the standalone loader (docs/standalone.md), next to Scrap.exe in game/Bin. Fixed base 0x400000; the
# linker script puts an empty section the size of the original first, the loader copies the original into it.
add_executable(ScrapC WIN32 src/standalone/main.c)
target_link_libraries(ScrapC PRIVATE scrapcode)
target_link_options(ScrapC PRIVATE -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase
	-Wl,-T,${CMAKE_SOURCE_DIR}/cmake/ScrapC.ld)
set_target_properties(ScrapC PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/cmake/ScrapC.ld)
