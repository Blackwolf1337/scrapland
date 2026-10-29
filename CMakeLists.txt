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
# the hook installer and the thunks of the imports exist only in the hook DLL (stage 3c2: ScrapC.exe hooks nothing and
# has no thunks)
target_sources(scrapcode PRIVATE src/core/hook.c src/core/imports.c)

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

# ScrapC.exe: the standalone exe (docs/standalone.md), in game/Bin. A normal exe since stage 3c2: the default base
# with dynamic base, the C objects of the original's data are plain globals. Neither the build nor ScrapC.exe needs
# the original exe.
find_program(PYTHON3 python3 REQUIRED)
set(SA_DIR ${CMAKE_CURRENT_BINARY_DIR}/standalone)

# The check against the original (optional, target datacheck): the data sources of src/game/Global again, with
# SD_DATACHECK, which puts every C object into a section .sd.<rva> of its own (include/scrap/shared.h). datagen.py
# --check compares each one with the bytes of Scrap.exe 1.6.1 at its rva, rsrccheck.py the resources of ScrapC.exe
# with the original's. objects.txt lists the objects of scrapcode_sa and of the check library for the generators of
# tools/standalone and tools/test/testsyms.py, which read the .sd sections (build the target scrapdata_check first).
set(SCRAP_EXE ${CMAKE_SOURCE_DIR}/game/Bin/Scrap.exe CACHE FILEPATH "Scrap.exe 1.6.1 for the optional data check")
file(GLOB DATA_SOURCES CONFIGURE_DEPENDS src/game/Global/Globals*.c src/game/Global/Strings*.c
	src/game/Global/Vtables.c)
add_library(scrapdata_check OBJECT EXCLUDE_FROM_ALL ${DATA_SOURCES})
# the includes, options and SD_STANDALONE of scrapcode_sa, without a dependency on its objects
target_include_directories(scrapdata_check PRIVATE $<TARGET_PROPERTY:scrapcode_sa,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(scrapdata_check PRIVATE $<TARGET_PROPERTY:scrapcode_sa,INTERFACE_COMPILE_OPTIONS>)
target_compile_definitions(scrapdata_check PRIVATE $<TARGET_PROPERTY:scrapcode_sa,INTERFACE_COMPILE_DEFINITIONS>
	SD_DATACHECK)
file(GENERATE OUTPUT ${SA_DIR}/objects.txt
	CONTENT "$<JOIN:$<TARGET_OBJECTS:scrapcode_sa>,\n>\n$<JOIN:$<TARGET_OBJECTS:scrapdata_check>,\n>\n")
file(GENERATE OUTPUT ${SA_DIR}/datacheck.txt CONTENT "$<JOIN:$<TARGET_OBJECTS:scrapdata_check>,\n>\n")
add_custom_target(datacheck
	COMMAND ${PYTHON3} -I ${CMAKE_SOURCE_DIR}/tools/standalone/datagen.py --check ${SCRAP_EXE} ${SA_DIR}/datacheck.txt
	COMMAND ${PYTHON3} -I ${CMAKE_SOURCE_DIR}/tools/standalone/rsrccheck.py ${SCRAP_EXE} $<TARGET_FILE:ScrapC>
	DEPENDS scrapdata_check ScrapC
	COMMENT "Check the data and the resources of ScrapC.exe against Scrap.exe"
	VERBATIM)
# the resources of ScrapC.exe (stage 3b3): src/standalone/scrap.rc and its files in res/, made by
# tools/standalone/rsrc.py from the original's .rsrc. An object library of its own, so the C options of scrapcode_sa
# do not reach windres. rsrccheck.py compares them with the original's (target datacheck).
enable_language(RC)
file(GLOB RES_FILES CONFIGURE_DEPENDS src/standalone/res/*)
add_library(scrapres OBJECT src/standalone/scrap.rc)
target_include_directories(scrapres PRIVATE src/standalone) # windres finds the res/ files from the include path
set_source_files_properties(src/standalone/scrap.rc PROPERTIES OBJECT_DEPENDS "${RES_FILES}")
add_executable(ScrapC WIN32 src/standalone/main.c src/standalone/crt.c src/standalone/crtinit.c src/standalone/crtxc.c
	src/standalone/crtmath.c src/standalone/crtvec.c $<TARGET_OBJECTS:scrapres>)
# stage 3c1a: the DLLs of the original are imports of ScrapC.exe itself (GAME_IMPORT of include/scrap/game.h). fmod,
# Galaxy and steam_api (in game/Bin, next to ScrapC.exe) through import libraries made from the .def files of
# src/standalone, the others through the ones of mingw-w64; dinput8 is no import (main.c loads the system one).
# ld sorts the import directory by the path of the import library, so the libraries of the game's DLLs are linked as
# build/implib/<n>-lib<name>.a (dlltool output or a link to the mingw-w64 one), numbered in the order of the
# original's import directory. kernel32, user32, gdi32, advapi32, shell32, comdlg32 and ole32 are in the default
# libraries of the toolchain, so they keep their place in ld's order (a second path of one of them would split its
# import descriptor).
set(IMPLIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/implib)
file(MAKE_DIRECTORY ${IMPLIB_DIR})
set(IMPLIBS)
set(n 10)
foreach(dll xinput9_1_0 winmm wsock32 Galaxy steam_api fmod msvfw32 avifil32 ddraw comctl32 d3d8)
	string(TOLOWER ${dll} lib)
	set(implib ${IMPLIB_DIR}/${n}-lib${lib}.a)
	if(EXISTS ${CMAKE_SOURCE_DIR}/src/standalone/${dll}.def)
		add_custom_command(OUTPUT ${implib}
			COMMAND ${CMAKE_DLLTOOL} -d ${CMAKE_SOURCE_DIR}/src/standalone/${dll}.def -l ${n}-lib${lib}.a
			WORKING_DIRECTORY ${IMPLIB_DIR}
			DEPENDS src/standalone/${dll}.def
			COMMENT "Import library of ${dll}.dll")
	else()
		find_library(IMPLIB_${lib} ${lib} REQUIRED)
		file(CREATE_LINK ${IMPLIB_${lib}} ${implib} SYMBOLIC)
	endif()
	list(APPEND IMPLIBS ${implib})
	math(EXPR n "${n} + 1")
endforeach()
add_custom_target(implibs DEPENDS ${IMPLIBS})
add_dependencies(ScrapC implibs)
target_link_libraries(ScrapC PRIVATE scrapcode_sa python152 ${IMPLIBS} kernel32 user32 gdi32 advapi32 shell32 comdlg32
	ole32)
# stage 3c1b: the data formats c_dfDIKeyboard and c_dfDIMouse of mingw's libdinput8 and their GUIDs, static data
# (no import of dinput8.dll), as -l flags: the target dinput8 is the hook DLL
target_link_libraries(ScrapC PRIVATE -ldinput8 -ldxguid)
# stage 3c2: the default base of ld with dynamic base (ASLR)
target_link_options(ScrapC PRIVATE -Wl,--dynamicbase)
# stage 3c1b: the C computes no address from an rva of the original (GAME_FUNC names its hook, game_base only in the
# allowed places)
add_custom_command(TARGET ScrapC POST_BUILD
	COMMAND ${PYTHON3} -I ${CMAKE_SOURCE_DIR}/tools/standalone/rvacheck.py
	COMMENT "Check that the C of ScrapC.exe uses no rvas")
