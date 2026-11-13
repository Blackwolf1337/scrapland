# The PvP modes of the original on the new network (include/scrap/arena.h): the pure part (the modes, the rules of the
# scores, the scorer's order, the wire), linked into ScrapC; its unit test (ctest arena_test).
cmake_minimum_required(VERSION 3.25)

set(ARENA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_library(arena STATIC ArenaDef.c)
target_include_directories(arena PUBLIC ${ARENA_ROOT}/include)
target_link_libraries(arena PUBLIC netrep)
target_compile_options(arena PRIVATE -Wall -Wextra -O2 -g)

add_executable(arena_test test/arena_test.c)
target_link_libraries(arena_test PRIVATE arena)
target_compile_options(arena_test PRIVATE -Wall -Wextra -O2 -g)
if(WIN32)
	target_link_options(arena_test PRIVATE -static-libgcc)
endif()
add_test(NAME arena_test COMMAND arena_test)
