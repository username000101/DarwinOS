#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
export ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
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

echo "=== Building XNU Kernel ==="
cd "$ROOT_SOURCE_DIR/Kernel"
bmake -m "$MAKESYSPATH" \
    ROOT_SOURCE_DIR="$ROOT_SOURCE_DIR" \
    ROOT_BINARY_DIR="$ROOT_BINARY_DIR" \
    SYSROOT_DIR="$SYSROOT_DIR" \
    RAVYN_SDKROOT="$RAVYN_SDKROOT" \
    xnu_all

echo "=== Installing Kernel ==="
mkdir -p "$SYSROOT_DIR/boot" "$SYSROOT_DIR/System/Library/Kernels"
cp -f "$SYSROOT_DIR/System/Library/Kernels/kernel" "$SYSROOT_DIR/boot/kernel"

echo "=== Building Kernel Extensions ==="
bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Kernel/Extensions" all || true

echo "=== Installing Kernel Extensions to Sysroot ==="
mkdir -p "${SYSROOT_DIR}/System/Library/Extensions"
find "$ROOT_SOURCE_DIR/Kernel/Extensions" -maxdepth 3 -type d -name "*.kext" -exec cp -Rf {} "${SYSROOT_DIR}/System/Library/Extensions/" \;

echo "=== Building and Installing dyld ==="
bash "$ROOT_SOURCE_DIR/ISO/build_dyld.sh"

echo "=== Building and Installing libSystem ==="
bash "$ROOT_SOURCE_DIR/ISO/build_monolithic_libsystem.sh"

echo "=== Installing WindowServer ==="
mkdir -p "${SYSROOT_DIR}/System/Library/CoreServices"
if [ -d "$ROOT_SOURCE_DIR/CoreServices/WindowServer/WindowServer.app" ]; then
    cp -Rf "$ROOT_SOURCE_DIR/CoreServices/WindowServer/WindowServer.app" "${SYSROOT_DIR}/System/Library/CoreServices/"
fi

echo "=== Building and Installing launchd (PID 1) ==="
mkdir -p "${SYSROOT_DIR}/sbin" "${SYSROOT_DIR}/dev" "${SYSROOT_DIR}/tmp" "${SYSROOT_DIR}/var" "${SYSROOT_DIR}/etc"
"$TOOLS/clang" -target x86_64-apple-darwin19 -fuse-ld="$TOOLS/ld64" -static -nostdlib -fno-stack-protector -e __start \
    "$ROOT_SOURCE_DIR/SysApps/launchd/launchd.c" -o "${SYSROOT_DIR}/sbin/launchd"
chmod +x "${SYSROOT_DIR}/sbin/launchd"

echo "=== Building ISO ==="
cd "$ROOT_SOURCE_DIR"
./ISO/build_linux.sh

echo "=== All Done ==="
