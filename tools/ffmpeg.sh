#!/bin/bash
# Builds the FFmpeg the video player of ScrapC needs (src/game/engine3d/Engine/Video.c): shared LGPL libraries
# (DLLs for mingw i686 or x86_64, or Linux .so files) with only the AVI demuxer, the MPEG-4 part 2 (XviD) and PCM decoders
# and swscale. The videos of the game (game/Video/*.vds) are AVI files with XviD video and 16 bit PCM sound.
#
# usage: tools/ffmpeg.sh [build dir [linux]]   default build/ffmpeg for Windows; the result is in <build dir>/install
#        (bin/*.dll and lib/*.dll.a, or lib/*.so; include), which CMakeLists.txt takes from there. For the Linux
#        build: tools/ffmpeg.sh build-linux/ffmpeg linux, for the 64 bit one build-linux64/ffmpeg linux64, for the 64
#        bit Windows build build-win64/ffmpeg win64
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(realpath -m "${1:-$root/build/ffmpeg}")
ver=7.1.1
sha=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
tar=$out/ffmpeg-$ver.tar.xz

mkdir -p "$out"
if [ ! -f "$tar" ]; then
	curl -fL -o "$tar.part" "https://ffmpeg.org/releases/ffmpeg-$ver.tar.xz"
	mv "$tar.part" "$tar"
fi
echo "$sha  $tar" | sha256sum -c --quiet
rm -rf "$out/src"
mkdir -p "$out/src"
tar -xf "$tar" -C "$out/src" --strip-components=1
cd "$out/src"
if [ "$2" = linux ]; then
	target=(--target-os=linux --arch=x86 --cpu=i686 --cc="gcc -m32" --extra-ldflags=-m32 --disable-x86asm)
elif [ "$2" = linux64 ]; then
	target=(--target-os=linux --arch=x86_64 --disable-x86asm)
elif [ "$2" = win64 ]; then
	target=(--target-os=mingw32 --arch=x86_64 --cross-prefix=x86_64-w64-mingw32- --enable-cross-compile --disable-x86asm
		--extra-ldflags=-static-libgcc --extra-libs=-l:libwinpthread.a)
else
	target=(--target-os=mingw32 --arch=x86 --cpu=i686 --cross-prefix=i686-w64-mingw32- --enable-cross-compile
		--extra-ldflags=-static-libgcc --extra-libs=-l:libwinpthread.a)
fi
./configure --prefix="$out/install" "${target[@]}" --pkg-config=false \
	--enable-shared --disable-static --disable-programs --disable-doc --disable-network --disable-autodetect \
	--disable-everything --disable-avdevice --disable-avfilter --disable-swresample --disable-postproc \
	--enable-avformat --enable-avcodec --enable-swscale \
	--enable-demuxer=avi --enable-decoder=mpeg4,pcm_s16le --enable-parser=mpeg4video --enable-protocol=file
make -j"$(nproc)"
make install
ls "$out/install/bin" "$out/install/lib" 2>/dev/null || true
