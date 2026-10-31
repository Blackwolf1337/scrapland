#!/bin/bash
# Proves that a change of names or types leaves the code unchanged: builds the base commit (in a worktree) and the
# working tree with SD_CODECMP, where CHECK has no expression text and __FILE__, __LINE__ and __TIMESTAMP__ are
# empty, and compares the sections of ScrapC.exe and dinput8.dll. Exit code 0 when they are identical.
# The base must have SD_CODECMP in include/scrap/global.h (since the cleanup/readable branch).
#
# usage: tools/codecmp.sh [base ref, default HEAD]
root=$(cd "$(dirname "$0")/.." && pwd)
base=${1:-HEAD}
out=$root/build/codecmp
flags="-DSD_CODECMP -Wno-builtin-macro-redefined -D__FILE__=\\\"\\\" -D__TIMESTAMP__=\\\"\\\" -D__LINE__=0"

# build <source dir> <build dir>
build() {
	if [ ! -f "$2/CMakeCache.txt" ]; then
		cmake -S "$1" -B "$2" -DCMAKE_TOOLCHAIN_FILE="$1/cmake/mingw-i686.cmake" -DCMAKE_C_FLAGS="$flags" \
			-DCMAKE_C_COMPILER_LAUNCHER=ccache > /dev/null || return 1
	fi
	cmake --build "$2" -j"$(nproc)" --target ScrapC dinput8 > "$2.log" 2>&1 || { tail -20 "$2.log"; return 1; }
}

rev=$(git -C "$root" rev-parse --verify "$base^{commit}") || exit 1
if [ ! -d "$out/src" ]; then
	git -C "$root" worktree add --detach "$out/src" "$rev" > /dev/null || exit 1
else
	git -C "$out/src" checkout -q --detach "$rev" || exit 1
fi
build "$out/src" "$out/base" || exit 1
build "$root" "$out/work" || exit 1

rc=0
for f in ScrapC.exe dinput8.dll; do
	for sec in .text .rdata .data .bss; do
		a=$(i686-w64-mingw32-objcopy -O binary --only-section=$sec "$out/base/$f" /dev/stdout | sha1sum)
		b=$(i686-w64-mingw32-objcopy -O binary --only-section=$sec "$out/work/$f" /dev/stdout | sha1sum)
		if [ "$a" = "$b" ]; then
			echo "$f $sec identical"
		else
			echo "$f $sec DIFFERS"
			rc=1
		fi
	done
done
exit $rc
