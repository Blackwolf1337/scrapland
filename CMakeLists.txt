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

# the C code shared by the hook DLL and the standalone exe, built twice: scrapcode for the DLL (the libraries are the
# exe's, Python through thunks), scrapcode_sa with SD_STANDALONE for ScrapC.exe (the real Python of python152)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/game/*.c)
# the libraries linked into Scrap.exe that are C here (D3DX8, the ASE SDK, the M3D math)
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/lib/*.c)
foreach(lib scrapcode scrapcode_sa)
	add_library(${lib} OBJECT
		src/core/d3dlog.c
		src/core/game.c
		src/core/hook.c
		src/core/imports.c
		src/core/libm.c
		src/core/log.c
		src/core/testmod.c
	)
	# the decompiled game code, in the folder layout of the original sources (paths from the asserts in Scrap.exe)
	target_sources(${lib} PRIVATE ${GAME_SOURCES})
	target_sources(${lib} PRIVATE ${LIB_SOURCES})
	target_include_directories(${lib} PUBLIC
		include
		src
		third_party/python-1.5.2/config
		third_party/python-1.5.2/Include
	)
	# Scrap.exe does its float math with SSE2 (MSVC /arch:SSE2): same here, x87 80 bit intermediates would change
	# the last bits of results
	target_compile_options(${lib} PUBLIC -Wall -Wextra -O2 -g -fno-omit-frame-pointer -fno-strict-aliasing
		-msse2 -mfpmath=sse -ffp-contract=off
		# sources are UTF-8, the game's strings Windows-1252 (Spanish messages of the original)
		-finput-charset=UTF-8 -fexec-charset=CP1252)
	target_link_options(${lib} PUBLIC -static-libgcc -Wl,--enable-stdcall-fixup)
endforeach()
target_compile_definitions(scrapcode_sa PUBLIC SD_STANDALONE)

# Python 1.5.2 for ScrapC.exe from third_party/python-1.5.2 (unchanged), configured like the one in Scrap.exe: our
# config.h and inittab, pyshim.h for its libc (the exe's C runtime and the engine's Python pool) and the files of
# src/python that add the engine's changes around the vendored ones (docs/standalone.md)
set(PY152 third_party/python-1.5.2)
add_library(python152 STATIC
	src/python/classobject.c
	src/python/complexobject.c
	src/python/config.c
	src/python/dictobject.c
	src/python/floatobject.c
	src/python/getpathp.c
	src/python/import.c
	src/python/intobject.c
	src/python/listobject.c
	src/python/pythonrun.c
	src/python/stringobject.c
	${PY152}/Modules/errnomodule.c
	${PY152}/Modules/getbuildinfo.c
	${PY152}/Modules/_localemodule.c
	${PY152}/Modules/mathmodule.c
	${PY152}/Modules/newmodule.c
	${PY152}/Modules/regexmodule.c
	${PY152}/Modules/regexpr.c
	${PY152}/Modules/signalmodule.c
	${PY152}/Modules/stropmodule.c
	${PY152}/Objects/abstract.c
	${PY152}/Objects/bufferobject.c
	${PY152}/Objects/cobject.c
	${PY152}/Objects/fileobject.c
	${PY152}/Objects/frameobject.c
	${PY152}/Objects/funcobject.c
	${PY152}/Objects/longobject.c
	${PY152}/Objects/methodobject.c
	${PY152}/Objects/moduleobject.c
	${PY152}/Objects/object.c
	${PY152}/Objects/rangeobject.c
	${PY152}/Objects/sliceobject.c
	${PY152}/Objects/tupleobject.c
	${PY152}/Objects/typeobject.c
	${PY152}/Parser/acceler.c
	${PY152}/Parser/bitset.c
	${PY152}/Parser/grammar1.c
	${PY152}/Parser/listnode.c
	${PY152}/Parser/myreadline.c
	${PY152}/Parser/node.c
	${PY152}/Parser/parser.c
	${PY152}/Parser/parsetok.c
	${PY152}/Parser/tokenizer.c
	${PY152}/PC/dl_nt.c
	${PY152}/PC/import_nt.c
	${PY152}/Python/bltinmodule.c
	${PY152}/Python/ceval.c
	${PY152}/Python/compile.c
	${PY152}/Python/errors.c
	${PY152}/Python/frozen.c
	${PY152}/Python/getargs.c
	${PY152}/Python/getcompiler.c
	${PY152}/Python/getcopyright.c
	${PY152}/Python/getmtime.c
	${PY152}/Python/getplatform.c
	${PY152}/Python/getversion.c
	${PY152}/Python/graminit.c
	${PY152}/Python/importdl.c
	${PY152}/Python/marshal.c
	${PY152}/Python/modsupport.c
	${PY152}/Python/mystrtoul.c
	${PY152}/Python/pyfpe.c
	${PY152}/Python/pystate.c
	${PY152}/Python/structmember.c
	${PY152}/Python/sysmodule.c
	${PY152}/Python/traceback.c
)
set_target_properties(python152 PROPERTIES C_STANDARD 90 C_EXTENSIONS ON) # gnu89
target_compile_definitions(python152 PRIVATE NDEBUG) # a release build in the exe: no assert of longobject.c and regexpr.c
target_include_directories(python152 PRIVATE src/python ${PY152}/Include ${PY152} ${PY152}/Python include)
# -fwrapv: the overflow checks of 1.5.2 (i_divmod of INT_MIN, int_mul ...) rely on signed overflow wrapping, as MSVC
# does; without it gcc may drop them
target_compile_options(python152 PRIVATE -include ${CMAKE_SOURCE_DIR}/src/python/pyshim.h -O2 -g
	-fno-strict-aliasing -fwrapv -msse2 -mfpmath=sse -ffp-contract=off -fno-builtin)
# sys.version of the exe: "1.5.2 (#0, Nov 24 2021, 15:23:52) [MSC 32 bit (Intel)]"
set_source_files_properties(${PY152}/Modules/getbuildinfo.c PROPERTIES
	COMPILE_DEFINITIONS "DATE=\"Nov 24 2021\";TIME=\"15:23:52\"")

# dinput8.dll: proxy for the system dinput8 that replaces Scrap.exe functions with the C versions of src/
add_library(dinput8 SHARED src/loader/dinput8.c src/loader/dinput8.def)
target_link_libraries(dinput8 PRIVATE scrapcode)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME dinput8)

# ScrapC.exe: the standalone exe (docs/standalone.md), in game/Bin. Fixed base 0x400000; the linker script puts the
# room of the original first: its data at their rvas, read from Scrap.exe 1.6.1 at build time by datagen.py (stage
# 3a), and the C objects of SD_AT at theirs (stage 3b). The build needs the original exe, ScrapC.exe does not.
# datagen.py runs after the objects of scrapcode_sa (ScrapC links them) and again when one of them changes, it reads
# their .sd.<rva> sections from the list in objects.txt.
set(SCRAP_EXE ${CMAKE_SOURCE_DIR}/game/Bin/Scrap.exe CACHE FILEPATH "Scrap.exe 1.6.1, whose data ScrapC.exe builds in")
find_program(PYTHON3 python3 REQUIRED)
set(ROOM_DIR ${CMAKE_CURRENT_BINARY_DIR}/standalone)
file(GENERATE OUTPUT ${ROOM_DIR}/objects.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:scrapcode_sa>,\n>\n")
add_custom_command(OUTPUT ${ROOM_DIR}/room.c ${ROOM_DIR}/room.h ${ROOM_DIR}/room.ld ${ROOM_DIR}/room.bin
	COMMAND ${PYTHON3} -I ${CMAKE_SOURCE_DIR}/tools/standalone/datagen.py ${SCRAP_EXE} ${ROOM_DIR}
		${ROOM_DIR}/objects.txt
	DEPENDS tools/standalone/datagen.py tools/standalone/sdexe.py tools/standalone/datamap.py
		tools/standalone/datamap.txt ${SCRAP_EXE} include/scrap/game.h
		include/scrap/origcrt.h include/scrap/origpython.h src/hooks.inc src/imports.inc src/standalone/crthooks.inc
		${ROOM_DIR}/objects.txt $<TARGET_OBJECTS:scrapcode_sa>
	COMMENT "Data of Scrap.exe for ScrapC.exe")
# the resources of ScrapC.exe (stage 3b3): src/standalone/scrap.rc and its files in res/, made by
# tools/standalone/rsrc.py from the original's .rsrc. An object library of its own, so the C options of scrapcode_sa
# do not reach windres. rsrccheck.py compares them with the original's after the link.
enable_language(RC)
file(GLOB RES_FILES CONFIGURE_DEPENDS src/standalone/res/*)
add_library(scrapres OBJECT src/standalone/scrap.rc)
target_include_directories(scrapres PRIVATE src/standalone) # windres finds the res/ files from the include path
set_source_files_properties(src/standalone/scrap.rc PROPERTIES OBJECT_DEPENDS "${RES_FILES}")
add_executable(ScrapC WIN32 src/standalone/main.c src/standalone/crt.c src/standalone/crtinit.c
	src/standalone/crtmath.c src/standalone/crtvec.c src/standalone/dataprobe.c ${ROOM_DIR}/room.c ${ROOM_DIR}/room.h
	$<TARGET_OBJECTS:scrapres>)
target_include_directories(ScrapC PRIVATE ${ROOM_DIR})
target_link_libraries(ScrapC PRIVATE scrapcode_sa python152)
target_link_options(ScrapC PRIVATE -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase
	-Wl,-T,${CMAKE_SOURCE_DIR}/cmake/ScrapC.ld -L${ROOM_DIR})
set_target_properties(ScrapC PROPERTIES LINK_DEPENDS "${CMAKE_SOURCE_DIR}/cmake/ScrapC.ld;${ROOM_DIR}/room.ld")
add_custom_command(TARGET ScrapC POST_BUILD
	COMMAND ${PYTHON3} -I ${CMAKE_SOURCE_DIR}/tools/standalone/rsrccheck.py ${SCRAP_EXE} $<TARGET_FILE:ScrapC>
	COMMENT "Resources of ScrapC.exe against Scrap.exe")
# stage 3b2: every rva of the C and the tests lies in the data that datagen.py builds in (tools/standalone/datamap.txt)
add_custom_command(TARGET ScrapC POST_BUILD
	COMMAND ${PYTHON3} -I ${CMAKE_SOURCE_DIR}/tools/standalone/datamap.py --check -e ${SCRAP_EXE} -c $<TARGET_FILE:ScrapC>
	COMMENT "Check the rvas of ScrapC.exe against the data map")
