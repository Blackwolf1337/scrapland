# The faction table and the wanted level rules of the freeroam mode (include/scrap/faction.h): the pure part, without
# the engine, linked into ScrapC; its unit test (ctest faction_test).
cmake_minimum_required(VERSION 3.25)

set(FACTION_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_library(faction STATIC FactionDef.c)
target_include_directories(faction PUBLIC ${FACTION_ROOT}/include)
target_compile_options(faction PRIVATE -Wall -Wextra -O2 -g)
# the built-in table (#embed)
set_source_files_properties(FactionDef.c PROPERTIES OBJECT_DEPENDS ${FACTION_ROOT}/data/factions/factions.def)

add_executable(faction_test test/faction_test.c)
target_link_libraries(faction_test PRIVATE faction)
target_compile_options(faction_test PRIVATE -Wall -Wextra -O2 -g)
if(WIN32)
	target_link_options(faction_test PRIVATE -static-libgcc)
endif()
add_test(NAME faction_test COMMAND faction_test)
