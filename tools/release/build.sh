#!/bin/bash
# build.sh <version> <nz_w32c.sfx> -- run from the top of a clean checkout of the release commit.
# Writes to out/: the four static release binaries, the two Windows zips (nz-re.exe, the original's
# nz_w32c.sfx, README.txt, LICENSE.txt) and SHA256SUMS.txt over all six.
#
# The stub is NanoZip 0.09a's own (the same file in its win32, win64 and linux zips):
#   gh release download 0.09a --repo YadeWira/nanozip --pattern '*win64.zip'
# and take nz_w32c.sfx out of it. Its checksum is checked below.
#
# The builds run one after another: with `&` + `wait` under set -e and 2>/dev/null they once
# produced nothing, silently.
set -e
VER=${1:?usage: build.sh <version> <nz_w32c.sfx>}; STUB=${2:?usage: build.sh <version> <nz_w32c.sfx>}
STUB_SHA=9cb7e8b78531b9092a7fb0d4ea055ab8bb94e0b8aa62b0eca9e7aa5b9a9b29d8
[ "$(sha256sum < "$STUB" | cut -d' ' -f1)" = "$STUB_SHA" ] || { echo "build.sh: $STUB is not NanoZip 0.09a's nz_w32c.sfx" >&2; exit 1; }
mkdir -p out
C="-std=c++17 -O2 -DNDEBUG -D_FILE_OFFSET_BITS=64 -Iinclude"
W="-D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -Wl,--major-subsystem-version=6 -Wl,--minor-subsystem-version=0"
g++                         $C -static -pthread -o out/nz-re-linux-x64 src/*.cpp
g++ -m32 -msse2             $C -static -pthread -o out/nz-re-linux-x86 src/*.cpp
x86_64-w64-mingw32-g++-win32 $C $W -static -static-libgcc -static-libstdc++ -o out/nz-re-windows-x64.exe src/*.cpp
# The 32-bit one runs on Windows XP (SP3) too, as the original's does: subsystem 5.01, and
# NZ_XP_COMPAT, which compiles src/nz_xp_compat.cpp -- the five Vista entry points the win32 thread
# model's libstdc++ imports (CONDITION_VARIABLE, GetThreadId), the real ones on Vista and later.
# Its headers still need _WIN32_WINNT=0x0600 (they hide std::condition_variable below it); the
# import table is what proves nothing else from Vista is used. Not the POSIX-threads g++: its
# emulated TLS runs thread_local destructors over released storage and corrupted -cD decodes.
# Large-address-aware, as the original's 32-bit nz.exe: 4 GB of address space on 64-bit Windows.
XP="-DNZ_XP_COMPAT -Wl,--major-subsystem-version=5 -Wl,--minor-subsystem-version=1 -Wl,--major-os-version=5 -Wl,--minor-os-version=1"
i686-w64-mingw32-g++-win32 -msse2 $C -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 $XP -Wl,--large-address-aware -static -static-libgcc -static-libstdc++ -o out/nz-re-windows-x86.exe src/*.cpp
strip out/nz-re-linux-x64 out/nz-re-linux-x86
x86_64-w64-mingw32-strip out/nz-re-windows-x64.exe
i686-w64-mingw32-strip   out/nz-re-windows-x86.exe

for a in x64 x86; do
  d=out/zip/nz-re-$VER-windows-$a
  rm -rf "$d"; mkdir -p "$d"
  cp out/nz-re-windows-$a.exe "$d/nz-re.exe"
  cp "$STUB" "$d/nz_w32c.sfx"
  # text files with CRLF, for Notepad
  # UTF-8 with a BOM (the Spanish half has accents; older Notepads guess the encoding otherwise)
  { printf '\xef\xbb\xbf'; sed -e "s/@VERSION@/$VER/g" -e "s/@ARCH@/$a/g" -e 's/$/\r/' tools/release/README-windows.txt; } > "$d/README.txt"
  sed -e 's/$/\r/' LICENSE > "$d/LICENSE.txt"
  touch -d @0 "$d"/*   # fixed timestamps: the same commit gives the same zip
  rm -f "out/nz-re-$VER-windows-$a.zip"
  (cd out/zip && zip -q -X -9 "../nz-re-$VER-windows-$a.zip" "nz-re-$VER-windows-$a"/*)
done
rm -rf out/zip
cd out && sha256sum nz-re-linux-x64 nz-re-linux-x86 nz-re-windows-x64.exe nz-re-windows-x86.exe \
  "nz-re-$VER-windows-x64.zip" "nz-re-$VER-windows-x86.zip" > SHA256SUMS.txt && ls -l
