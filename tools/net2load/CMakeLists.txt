# net2load: load tester of Net2 (fake clients and a reference server), links net2 only. Part of the main build
# (add_subdirectory in the top CMakeLists.txt) or a project of its own with net2:
#   cmake -S tools/net2load -B build-net2load && cmake --build build-net2load
cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(net2load C)
	set(CMAKE_C_STANDARD 23)
	set(CMAKE_C_STANDARD_REQUIRED ON)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../src/net2 net2)
endif()

set(NET2LOAD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_executable(net2load Main.c Server.c Client.c)
# net2local.h for the byte reader and writer and the packet window; Monocypher (inside net2) for the identity keys
target_include_directories(net2load PRIVATE ${NET2LOAD_ROOT}/src/net2 ${NET2LOAD_ROOT}/third_party/monocypher)
target_link_libraries(net2load PRIVATE net2)
target_compile_options(net2load PRIVATE -Wall -Wextra -O2 -g)
if(WIN32)
	target_link_options(net2load PRIVATE -static-libgcc)
endif()
