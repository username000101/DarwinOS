#!/usr/bin/env bash
set -e

ROOT_SOURCE_DIR="${ROOT_SOURCE_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
export ROOT_BINARY_DIR="$HOME/build/darwinos"
export MAKEOBJDIRPREFIX="$ROOT_BINARY_DIR"
export MAKESYSPATH="$ROOT_SOURCE_DIR/BSD/share/mk"
export TOOLCHAIN="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain"
export TOOLS="$TOOLCHAIN/usr/bin"
export RAVYN_SDKROOT="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk"
export SYSROOT_DIR="$ROOT_BINARY_DIR/sysroot"
export PATH="$TOOLS:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export CC="$TOOLS/clang"
export CXX="$TOOLS/clang++"
export LD="$TOOLS/ld64"
export AR="$TOOLS/llvm-ar --format=darwin"
export MACOS_VERSION_MIN="10.15"
export MACOSX_DEPLOYMENT_TARGET="10.15"
export ARCH_CONFIGS="X86_64"
export KERNEL_CONFIGS="RELEASE"
export HOST_CC="/usr/bin/clang"
export HOST_CXX="/usr/bin/clang++"
export HOST_LD="/usr/bin/ld"
export MK_WERROR="no"

echo "Building RavynAHCIPort..."
cd "$ROOT_SOURCE_DIR/Kernel/Extensions/RavynAHCIPort"
bmake -m "$MAKESYSPATH" \
    ROOT_SOURCE_DIR="$ROOT_SOURCE_DIR" \
    ROOT_BINARY_DIR="$ROOT_BINARY_DIR" \
    SYSROOT_DIR="$SYSROOT_DIR" \
    RAVYN_SDKROOT="$RAVYN_SDKROOT" \
    clean all

echo "Copying to sysroot..."
OBJDIR="$ROOT_BINARY_DIR$ROOT_SOURCE_DIR/Kernel/Extensions/RavynAHCIPort"
if [ -d "$OBJDIR/RavynAHCIPort.kext" ]; then
    cp -Rf "$OBJDIR/RavynAHCIPort.kext" "$SYSROOT_DIR/System/Library/Extensions/"
elif [ -d "RavynAHCIPort.kext" ]; then
    cp -Rf "RavynAHCIPort.kext" "$SYSROOT_DIR/System/Library/Extensions/"
fi

echo "Building ISO..."
cd "$ROOT_SOURCE_DIR"
./ISO/build_linux.sh

echo "Done!"
