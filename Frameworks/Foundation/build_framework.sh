#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export ROOT_SOURCE_DIR="${ROOT_SOURCE_DIR:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
export ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
export MAKEOBJDIRPREFIX="$ROOT_BINARY_DIR"
export MAKESYSPATH="$ROOT_SOURCE_DIR/BSD/share/mk"
export TOOLCHAIN="${TOOLCHAIN:-$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain}"
export TOOLS="${TOOLS:-$TOOLCHAIN/usr/bin}"
export RAVYN_SDKROOT="${RAVYN_SDKROOT:-$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk}"
export SYSROOT_DIR="${SYSROOT_DIR:-$ROOT_BINARY_DIR/sysroot}"
export PATH="$TOOLS:$PATH"
export CC="${CC:-$TOOLS/clang}"
export CXX="${CXX:-$TOOLS/clang++}"
export LD="${LD:-$TOOLS/ld64}"
export AR="${AR:-$TOOLS/llvm-ar --format=darwin}"
export MACOS_VERSION_MIN="10.15"
export MACOSX_DEPLOYMENT_TARGET="10.15"
export MK_WERROR="no"

echo "Rebuilding Foundation.framework..."
bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Frameworks/Foundation" all

if [ -d "$RAVYN_SDKROOT/System/Library/Frameworks/Foundation.framework" ]; then
    mkdir -p "$SYSROOT_DIR/System/Library/Frameworks"
    cp -a "$RAVYN_SDKROOT/System/Library/Frameworks/Foundation.framework" "$SYSROOT_DIR/System/Library/Frameworks/"
    cd "$RAVYN_SDKROOT/System/Library/Frameworks/Foundation.framework" && ln -sf Versions/C/Foundation Foundation
    cd "$SYSROOT_DIR/System/Library/Frameworks/Foundation.framework" && ln -sf Versions/C/Foundation Foundation
fi

if [ -d "$ROOT_BINARY_DIR/iso_staging/System/Library/Frameworks" ]; then
    cp -a "$RAVYN_SDKROOT/System/Library/Frameworks/Foundation.framework" "$ROOT_BINARY_DIR/iso_staging/System/Library/Frameworks/"
    cd "$ROOT_BINARY_DIR/iso_staging/System/Library/Frameworks/Foundation.framework" && ln -sf Versions/C/Foundation Foundation
fi

echo "Foundation.framework successfully rebuilt and installed!"
