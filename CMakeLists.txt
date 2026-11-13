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

if(NOT (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
	message(FATAL_ERROR "ScrapC builds for Windows (cmake/mingw-x86_64.cmake) and Linux (cmake/linux-x86_64.cmake)")
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "ScrapC builds for 64 bit only")
endif()

# the game code of ScrapC.exe, in the folder layout of the original sources (paths from the asserts in Scrap.exe)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/game/*.c)
# the libraries linked into Scrap.exe that are C here (D3DX8, the ASE SDK, the M3D math)
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/lib/*.c)
add_library(scrapcode OBJECT
	src/core/game.c
	src/core/log.c
	${GAME_SOURCES}
	${LIB_SOURCES}
	# the sound output and the .ogg decoder (third_party, public domain), without the warnings of the game code
	src/sound/ma.c
	third_party/stb/stb_vorbis.c
)
set_source_files_properties(src/sound/ma.c third_party/stb/stb_vorbis.c PROPERTIES COMPILE_OPTIONS -w)
target_include_directories(scrapcode PUBLIC
	include
	src
	third_party/vulkan/include
	third_party/miniaudio
	third_party/stb
)
# Scrap.exe does its float math with SSE2 (MSVC /arch:SSE2): same here, x87 80 bit intermediates would change
# the last bits of results
target_compile_options(scrapcode PUBLIC -Wall -Wextra -O2 -g -fno-omit-frame-pointer -fno-strict-aliasing
	-msse2 -mfpmath=sse -ffp-contract=off
	# sources are UTF-8, the game's strings Windows-1252 (Spanish messages of the original)
	-finput-charset=UTF-8 -fexec-charset=CP1252)
if(WIN32)
	target_link_options(scrapcode PUBLIC -static-libgcc -Wl,--enable-stdcall-fixup)
endif()

# the GLSL shaders of the Vulkan backend (shaders/), as SPIR-V words for #include
find_program(GLSLANG glslangValidator REQUIRED)
set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})
file(GLOB SHADER_HEADERS CONFIGURE_DEPENDS shaders/*.glsl shaders/ps/*.glsl)
# the pixel shaders of the game (shaders/ps/<name>.frag for bmp/<name>.psh) and ps_all.inc with their table
file(GLOB PS_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR}/shaders shaders/ps/*.frag)
set(PS_ALL "/* made by CMakeLists.txt: the SPIR-V of shaders/ps/<name>.frag and their names */\n")
set(PS_TABLE "static const VkdPsCode vkd_psCode[] = {\n")
foreach(s ${PS_SOURCES})
	get_filename_component(n ${s} NAME_WE)
	string(APPEND PS_ALL "static const uint32_t vkd_ps_${n}[] = {\n#include \"ps_${n}_frag.inc\"\n};\n")
	string(APPEND PS_TABLE "\t{ \"${n}\", vkd_ps_${n}, sizeof(vkd_ps_${n}) },\n")
endforeach()
file(CONFIGURE OUTPUT ${SHADER_DIR}/ps_all.inc CONTENT "${PS_ALL}${PS_TABLE}};\n")
set(SHADER_INCS)
foreach(s ff.vert ff.frag present.vert present.frag ${PS_SOURCES})
	string(REPLACE "." "_" n ${s})
	string(REPLACE "/" "_" n ${n})
	add_custom_command(OUTPUT ${SHADER_DIR}/${n}.inc
		COMMAND ${GLSLANG} -V --target-env vulkan1.3 -x -o ${SHADER_DIR}/${n}.inc ${CMAKE_SOURCE_DIR}/shaders/${s}
		DEPENDS shaders/${s} ${SHADER_HEADERS}
		COMMENT "SPIR-V of ${s}")
	list(APPEND SHADER_INCS ${SHADER_DIR}/${n}.inc)
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_INCS})

# FFmpeg for the video player (Engine/Video.c), built by tools/ffmpeg.sh (Windows DLLs, or shared libraries for
# Linux with "linux" as its second argument): its headers and libraries
set(FFMPEG_DIR ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg/install CACHE PATH "the FFmpeg of tools/ffmpeg.sh")
if(NOT EXISTS ${FFMPEG_DIR}/include/libavformat/avformat.h)
	message(FATAL_ERROR "no FFmpeg in ${FFMPEG_DIR}: run tools/ffmpeg.sh first")
endif()
set(FFMPEG_LIBS)
foreach(l avformat avcodec avutil swscale)
	if(WIN32)
		list(APPEND FFMPEG_LIBS ${FFMPEG_DIR}/bin/${l}.lib)
	else()
		list(APPEND FFMPEG_LIBS ${FFMPEG_DIR}/lib/lib${l}.so)
	endif()
endforeach()

# SDL3 for the window, the input, the timers and the Vulkan surface: built by tools/sdl3.sh for Windows, the
# system's (lib32-sdl3) on Linux
if(WIN32)
	set(SDL3_DIR ${CMAKE_CURRENT_BINARY_DIR}/sdl3/install CACHE PATH "the SDL3 of tools/sdl3.sh")
	if(NOT EXISTS ${SDL3_DIR}/include/SDL3/SDL.h)
		message(FATAL_ERROR "no SDL3 in ${SDL3_DIR}: run tools/sdl3.sh first")
	endif()
	set(SDL3_INCLUDE ${SDL3_DIR}/include)
	set(SDL3_LIB ${SDL3_DIR}/lib/libSDL3.dll.a)
else()
	find_path(SDL3_INCLUDE SDL3/SDL.h REQUIRED)
	find_library(SDL3_LIB SDL3 REQUIRED)
endif()
add_dependencies(scrapcode shaders)
target_include_directories(scrapcode PRIVATE ${SHADER_DIR} ${FFMPEG_DIR}/include ${SDL3_INCLUDE})

# ScrapC: the standalone game (docs/standalone.md), in game/Bin; neither the build nor the game needs the original
# exe
if(WIN32)
	# the resources of ScrapC.exe: src/standalone/scrap.rc and its files in res/. An object library of its own, so
	# the C options of scrapcode do not reach windres.
	enable_language(RC)
	file(GLOB RES_FILES CONFIGURE_DEPENDS src/standalone/res/*)
	add_library(scrapres OBJECT src/standalone/scrap.rc)
	target_include_directories(scrapres PRIVATE src/standalone) # windres finds the res/ files from the include path
	set_source_files_properties(src/standalone/scrap.rc PROPERTIES OBJECT_DEPENDS "${RES_FILES}")
	add_executable(ScrapC WIN32 src/standalone/main.c src/standalone/crt.c src/standalone/crtwin.c
		src/standalone/crtinit.c src/standalone/crtxc.c $<TARGET_OBJECTS:scrapres>)
	target_link_libraries(ScrapC PRIVATE scrapcode coord faction net2 netrep ${FFMPEG_LIBS} ${SDL3_LIB} wsock32 advapi32 user32
		kernel32)
	# the default base of ld with dynamic base (ASLR)
	target_link_options(ScrapC PRIVATE -Wl,--dynamicbase)
else()
	add_executable(ScrapC src/standalone/main.c src/standalone/crt.c src/standalone/crtposix.c
		src/standalone/crtinit.c src/standalone/crtxc.c)
	target_link_libraries(ScrapC PRIVATE scrapcode coord faction net2 netrep ${FFMPEG_LIBS} ${SDL3_LIB} m dl pthread)
	# the FFmpeg libraries next to the game
	set_target_properties(ScrapC PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
endif()

# Net2, the transport of the new multiplayer (include/scrap/net2.h; ScrapC's headless instance serves with it, ws2_32
# and bcrypt on Windows come with the library), and its tests (ctest: net2_test, net2_loopback)
enable_testing()
add_subdirectory(src/net2)
# its load tester (tools/net2load, a host tool: not part of the game)
add_subdirectory(tools/net2load)
# NetRep, the replication of a headless instance's world to its watchers (include/scrap/netrep.h), and its unit test
add_subdirectory(src/netrep)
# the coordinator of a multiplayer world (include/scrap/coord.h), the dedicated ScrapCoord and the unit test
add_subdirectory(src/coord)
# the faction table of the freeroam mode (include/scrap/faction.h) and its unit test
add_subdirectory(src/faction)
