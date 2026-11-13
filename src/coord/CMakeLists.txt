# The coordinator of a multiplayer world (include/scrap/coord.h): the library (tickets, the player store, child
# processes, the coordinator), linked into ScrapC (in-game hosting, the instances' ticket checks) and into the
# dedicated ScrapCoord; the list server ScrapList (include/scrap/coordlist.h); the unit test (ctest coord_test).
cmake_minimum_required(VERSION 3.25)

set(COORD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_library(coord STATIC Cli.c Coord.c Kv.c List.c Proc.c Store.c Sys.c Ticket.c)
target_include_directories(coord PUBLIC ${COORD_ROOT}/include)
target_link_libraries(coord PUBLIC net2 netrep arena)
target_compile_options(coord PRIVATE -Wall -Wextra -O2 -g)

add_executable(ScrapCoord main.c)
target_link_libraries(ScrapCoord PRIVATE coord)
target_compile_options(ScrapCoord PRIVATE -Wall -Wextra -O2 -g)

add_executable(ScrapList listmain.c)
target_link_libraries(ScrapList PRIVATE coord)
target_compile_options(ScrapList PRIVATE -Wall -Wextra -O2 -g)

add_executable(coord_test test/coord_test.c)
target_link_libraries(coord_test PRIVATE coord)
target_compile_options(coord_test PRIVATE -Wall -Wextra -O2 -g)
if(WIN32)
	target_link_options(ScrapCoord PRIVATE -static-libgcc)
	target_link_options(ScrapList PRIVATE -static-libgcc)
	target_link_options(coord_test PRIVATE -static-libgcc)
endif()
add_test(NAME coord_test COMMAND coord_test)
