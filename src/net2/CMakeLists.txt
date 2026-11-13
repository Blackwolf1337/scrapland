# Net2, the transport of the new multiplayer (include/scrap/net2.h), with Monocypher, and its tests. Part of the
# main build (add_subdirectory in the top CMakeLists.txt), or a project of its own for the 32 bit and other builds
# the game does not have:
#   cmake -S src/net2 -B build-net2 && cmake --build build-net2 && ctest --test-dir build-net2
#   cmake -S src/net2 -B build-net2-32 -DCMAKE_C_FLAGS=-m32
#   cmake -S src/net2 -B build-net2-win32 -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc
cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(net2 C)
	set(CMAKE_C_STANDARD 23)
	set(CMAKE_C_STANDARD_REQUIRED ON)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	enable_testing()
endif()

set(NET2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_library(net2 STATIC
	Conn.c
	Crypto.c
	Handshake.c
	Host.c
	Lan.c
	Packet.c
	Reliable.c
	Sock.c
	${NET2_ROOT}/third_party/monocypher/monocypher.c
)
target_include_directories(net2 PUBLIC ${NET2_ROOT}/include PRIVATE ${NET2_ROOT}/third_party/monocypher)
target_compile_options(net2 PRIVATE -Wall -Wextra -O2 -g)
if(WIN32)
	target_link_libraries(net2 PUBLIC ws2_32 bcrypt)
endif()

# unit tests of the pure parts and the handshake over an in-memory link; the loopback run uses real UDP sockets on
# 127.0.0.1 and ::1 (ports 29000-29099)
add_executable(net2_test test/net2_test.c)
target_include_directories(net2_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${NET2_ROOT}/third_party/monocypher)
target_link_libraries(net2_test PRIVATE net2)
target_compile_options(net2_test PRIVATE -Wall -Wextra -O2 -g)
add_executable(net2_loopback test/loopback.c)
target_link_libraries(net2_loopback PRIVATE net2)
target_compile_options(net2_loopback PRIVATE -Wall -Wextra -O2 -g)
# a client for a running server (ScrapC -instance): query, connect, echo, leave; not a ctest target
add_executable(net2_probe test/probe.c)
target_link_libraries(net2_probe PRIVATE net2)
target_compile_options(net2_probe PRIVATE -Wall -Wextra -O2 -g)
if(WIN32)
	target_link_options(net2_test PRIVATE -static-libgcc)
	target_link_options(net2_loopback PRIVATE -static-libgcc)
	target_link_options(net2_probe PRIVATE -static-libgcc)
endif()
add_test(NAME net2_test COMMAND net2_test)
add_test(NAME net2_loopback COMMAND net2_loopback)
