#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
. ./config.env
[ "$(uname -s)" = Darwin ] || { echo "on Linux use the distro package: apt install tegrarcm"; exit 1; }
command -v brew >/dev/null || { echo "Homebrew is required"; exit 1; }
brew install -q autoconf automake libtool pkg-config libusb cryptopp
B=$(brew --prefix)
rm -rf build/tegrarcm && mkdir -p build
git clone -q --depth 1 -b "$TEGRARCM_TAG" https://github.com/NVIDIA/tegrarcm.git build/tegrarcm
cd build/tegrarcm
patch -p1 -s < "$ROOT/scripts/tegrarcm-macos.patch"
autoreconf -i >/dev/null 2>&1
./configure -q --prefix="$ROOT/tools" CPPFLAGS="-I$B/include" LDFLAGS="-L$B/lib" PKG_CONFIG_PATH="$B/lib/pkgconfig"
make -s -j"$(sysctl -n hw.ncpu)"
make -s install
echo "tegrarcm: tools/bin/tegrarcm"
