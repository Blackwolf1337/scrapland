#!/bin/bash
# Builds the SDL3 that ScrapC.exe uses for its window, input, timers and Vulkan surface: a shared SDL3.dll for mingw
# x86_64 from the signed release source (the Linux build takes the SDL3 of the system).
#
# usage: tools/sdl3.sh [build dir]   default build-win64/sdl3; the result is in <build dir>/install (bin/SDL3.dll,
#        lib/libSDL3.dll.a, include, lib/cmake), which CMakeLists.txt takes from there
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(realpath -m "${1:-$root/build-win64/sdl3}")
ver=3.4.18
sha=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
tar=$out/SDL3-$ver.tar.gz
arch=x86_64

mkdir -p "$out"
if [ ! -f "$tar" ]; then
	curl -fL -o "$tar.part" "https://github.com/libsdl-org/SDL/releases/download/release-$ver/SDL3-$ver.tar.gz"
	mv "$tar.part" "$tar"
fi
echo "$sha  $tar" | sha256sum -c --quiet
rm -rf "$out/src" "$out/obj"
mkdir -p "$out/src"
tar -xf "$tar" -C "$out/src" --strip-components=1
cmake -S "$out/src" -B "$out/obj" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-$arch.cmake" -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_COMPILER=$arch-w64-mingw32-g++ -DCMAKE_INSTALL_PREFIX="$out/install" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF \
	-DSDL_EXAMPLES=OFF -DCMAKE_SHARED_LINKER_FLAGS=-static-libgcc
cmake --build "$out/obj" -j"$(nproc)"
cmake --install "$out/obj"
ls "$out/install/bin"
