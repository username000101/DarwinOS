#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
TOOLCHAIN="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain"
TOOLS="$TOOLCHAIN/usr/bin"
SYSROOT_DIR="$ROOT_BINARY_DIR/sysroot"

echo "=== Building and Installing launchd (PID 1) ==="
mkdir -p "${SYSROOT_DIR}/sbin" "${SYSROOT_DIR}/dev" "${SYSROOT_DIR}/tmp" "${SYSROOT_DIR}/var" "${SYSROOT_DIR}/etc"
"$TOOLS/clang" -target x86_64-apple-darwin19 -fuse-ld="$TOOLS/ld64" -static -nostdlib -fno-stack-protector -e __start \
    "$ROOT_SOURCE_DIR/SysApps/launchd/launchd.c" -o "${SYSROOT_DIR}/sbin/launchd"
chmod +x "${SYSROOT_DIR}/sbin/launchd"

echo "=== Building ISO ==="
cd "$ROOT_SOURCE_DIR"
./ISO/build_linux.sh

echo "=== Quick Rebuild Done ==="
