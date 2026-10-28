#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
TOOLCHAIN="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain"
TOOLS="$TOOLCHAIN/usr/bin"
RAVYN_SDKROOT="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk"
SYSROOT_DIR="$ROOT_BINARY_DIR/sysroot"

echo "Building monolithic libSystem.B.dylib..."
cd "$ROOT_SOURCE_DIR/Libraries/Libsystem"

# Compile monolithic_stubs.c
"$TOOLS/clang" -c "$ROOT_SOURCE_DIR/Libraries/Libsystem/monolithic_stubs.c" \
    -o "$ROOT_SOURCE_DIR/Libraries/Libsystem/monolithic_stubs.o" \
    --sysroot="$RAVYN_SDKROOT" \
    -D_FORTIFY_SOURCE=0 \
    -mmacosx-version-min=10.15 \
    -O2

# Compile dyld_stub_binder.s
"$TOOLS/clang" -c "$ROOT_SOURCE_DIR/Libraries/dyld/src/dyld_stub_binder.s" \
    -o "$ROOT_SOURCE_DIR/Libraries/dyld/src/dyld_stub_binder.o" \
    --sysroot="$RAVYN_SDKROOT" \
    -mmacosx-version-min=10.15

# Compile icu_stubs.c
"$TOOLS/clang" -c "$ROOT_SOURCE_DIR/Libraries/Libsystem/icu_stubs.c" \
    -o "$ROOT_SOURCE_DIR/Libraries/Libsystem/icu_stubs.o" \
    --sysroot="$RAVYN_SDKROOT" \
    -D_FORTIFY_SOURCE=0 \
    -mmacosx-version-min=10.15 \
    -O2

BUILD_TMP="$ROOT_BINARY_DIR/Libraries/libsystem_tmp"
rm -rf "$BUILD_TMP"
mkdir -p "$BUILD_TMP/kernel" "$BUILD_TMP/c" "$BUILD_TMP/pthread" "$BUILD_TMP/platform"

cd "$BUILD_TMP/kernel" && "$TOOLS/ar" -x "$RAVYN_SDKROOT/usr/local/lib/kernel/libkernel.a" && "$TOOLS/llvm-objcopy" --redefine-sym _bcopy=_libkernel_bcopy memcpy.o
cd "$BUILD_TMP/c" && "$TOOLS/ar" -x "$RAVYN_SDKROOT/usr/local/lib/dyld/libc.a"
cd "$BUILD_TMP/pthread" && "$TOOLS/ar" -x "$RAVYN_SDKROOT/usr/local/lib/dyld/libpthread.a" && rm -f pthread_tsd.o
cd "$BUILD_TMP/platform" && "$TOOLS/ar" -x "$RAVYN_SDKROOT/usr/local/lib/dyld/libplatform.a"

echo "Extracted objects, linking libSystem.B.dylib..."
cd "$BUILD_TMP"

"$TOOLS/clang" -shared -dylib -fuse-ld="$TOOLS/ld64" \
    --ld-path="$TOOLS/ld64" \
    --sysroot="$RAVYN_SDKROOT" \
    -mmacosx-version-min=10.15 \
    -Wl,-install_name,/usr/lib/libSystem.B.dylib \
    -Wl,-current_version,1281.100.1 \
    -Wl,-compatibility_version,1.0.0 \
    -nodefaultlibs \
    -o "$BUILD_TMP/libSystem.B.dylib" \
    "$ROOT_SOURCE_DIR/Libraries/Libsystem/monolithic_stubs.o" \
    "$ROOT_SOURCE_DIR/Libraries/Libsystem/icu_stubs.o" \
    "$ROOT_SOURCE_DIR/Libraries/dyld/src/dyld_stub_binder.o" \
    kernel/*.o c/*.o pthread/*.o platform/*.o \
    "$RAVYN_SDKROOT/usr/lib/libunwind.a" \
    "$RAVYN_SDKROOT/usr/lib/system/libCrashReporterClient.a"

echo "Monolithic libSystem.B.dylib successfully linked!"
"$TOOLS/llvm-otool" -hv "$BUILD_TMP/libSystem.B.dylib"

echo "Installing libSystem to SDK and Sysroot..."
mkdir -p "$RAVYN_SDKROOT/usr/lib" "$SYSROOT_DIR/usr/lib"
cp -fv "$BUILD_TMP/libSystem.B.dylib" "$RAVYN_SDKROOT/usr/lib/libSystem.B.dylib"
cp -fv "$BUILD_TMP/libSystem.B.dylib" "$SYSROOT_DIR/usr/lib/libSystem.B.dylib"

ln -sf libSystem.B.dylib "$RAVYN_SDKROOT/usr/lib/libSystem.dylib"
ln -sf libSystem.B.dylib "$SYSROOT_DIR/usr/lib/libSystem.dylib"

echo "Done!"
