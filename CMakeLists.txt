cmake_minimum_required(VERSION 3.25)
project(scrapdecomp C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # gnu23: naked functions, inline asm, calling convention attributes

find_program(CCACHE ccache)
if(CCACHE)
	set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif()

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "The hook DLL is 32 bit Windows only: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake")
endif()

# dinput8.dll: proxy for the system dinput8 that replaces Scrap.exe functions with the C versions of src/
add_library(dinput8 SHARED
	src/loader/dinput8.c
	src/loader/dinput8.def
	src/core/game.c
	src/core/hook.c
	src/core/imports.c
	src/core/log.c
	src/py/scrap.c
	src/py/sweap.c
)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME dinput8)
target_include_directories(dinput8 PRIVATE
	include
	src
	third_party/python-1.5.2/config
	third_party/python-1.5.2/Include
)
target_compile_options(dinput8 PRIVATE -Wall -Wextra -O2 -g -fno-omit-frame-pointer -fno-strict-aliasing)
target_link_options(dinput8 PRIVATE -static-libgcc -Wl,--enable-stdcall-fixup)
