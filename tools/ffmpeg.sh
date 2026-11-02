#!/bin/bash
# Builds the FFmpeg the video player of ScrapC.exe needs (src/game/engine3d/Engine/Video.c): shared LGPL DLLs for
# mingw i686 with only the AVI demuxer, the MPEG-4 part 2 (XviD) and PCM decoders and swscale. The videos of the game
# (game/Video/*.vds) are AVI files with XviD video and 16 bit PCM sound.
#
# usage: tools/ffmpeg.sh [build dir]   default build/ffmpeg; the result is in <build dir>/install (bin/*.dll,
#        lib/*.dll.a, include), which CMakeLists.txt takes from there
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
./configure --prefix="$out/install" --target-os=mingw32 --arch=x86 --cpu=i686 \
	--cross-prefix=i686-w64-mingw32- --enable-cross-compile --pkg-config=false \
	--enable-shared --disable-static --disable-programs --disable-doc --disable-network --disable-autodetect \
	--disable-everything --disable-avdevice --disable-avfilter --disable-swresample --disable-postproc \
	--enable-avformat --enable-avcodec --enable-swscale \
	--enable-demuxer=avi --enable-decoder=mpeg4,pcm_s16le --enable-parser=mpeg4video --enable-protocol=file \
	--extra-ldflags=-static-libgcc --extra-libs=-l:libwinpthread.a
make -j"$(nproc)"
make install
ls "$out/install/bin"
