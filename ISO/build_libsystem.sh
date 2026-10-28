#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
MAKEOBJDIRPREFIX="$ROOT_BINARY_DIR"
MAKESYSPATH="$ROOT_SOURCE_DIR/BSD/share/mk"
TOOLCHAIN="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain"
TOOLS="$TOOLCHAIN/usr/bin"
RAVYN_SDKROOT="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk"
SYSROOT_DIR="$ROOT_BINARY_DIR/sysroot"
export PATH="$TOOLS:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
CC="$TOOLS/clang"
CXX="$TOOLS/clang++"
LD="$TOOLS/ld64"
DEVEL="$ROOT_SOURCE_DIR/Developer"
MACOS_VERSION_MIN="10.15"

echo "Building libSystem..."
cd "$ROOT_SOURCE_DIR/Libraries/Libsystem"
bmake -m "$MAKESYSPATH" \
    ROOT_SOURCE_DIR="$ROOT_SOURCE_DIR" \
    ROOT_BINARY_DIR="$ROOT_BINARY_DIR" \
    SYSROOT_DIR="$SYSROOT_DIR" \
    RAVYN_SDKROOT="$RAVYN_SDKROOT" \
    SDK_SOURCE_DIR="$RAVYN_SDKROOT" \
    TOOLCHAIN="$TOOLCHAIN" \
    TOOLS="$TOOLS" \
    DEVEL="$DEVEL" \
    CC="$CC" \
    CXX="$CXX" \
    LD="$LD" \
    CpuArch="x86_64" \
    BuildArch="X86" \
    MACOS_VERSION_MIN="$MACOS_VERSION_MIN" \
    "$@"
