#!/usr/bin/env bash
set -ex

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
TOOLCHAIN="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain"
TOOLS="$TOOLCHAIN/usr/bin"
RAVYN_SDKROOT="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk"
SYSROOT_DIR="$ROOT_BINARY_DIR/sysroot"

echo "Compiling objc-os.mm..."
"$TOOLS/clang++" -ObjC++ --sysroot="$RAVYN_SDKROOT" \
    -mmacos-version-min=10.15 \
    -momit-leaf-frame-pointer \
    -fdollars-in-identifiers \
    -fobjc-legacy-dispatch \
    -fno-objc-convert-messages-to-runtime-calls \
    -fblocks \
    -Wno-deprecated-declarations \
    -I"$SCRIPT_DIR/runtime" \
    -I"$SCRIPT_DIR/../../Kernel/xnu/bsd" \
    -I"$SCRIPT_DIR/../Libsystem/libsystem_kernel/mig_hdr/include" \
    -I"$SCRIPT_DIR/../Libsystem/private" \
    -I"$SCRIPT_DIR/../Libsystem/libsystem_blocks" \
    -I"$RAVYN_SDKROOT/usr/local/include" \
    -I"$RAVYN_SDKROOT/usr/local/include/kernel" \
    -I"$RAVYN_SDKROOT/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders" \
    -I"$ROOT_BINARY_DIR/x86_64" \
    -D_LIBC_NO_FEATURE_VERIFICATION -DTARGET_OS_IOSMAC=0 \
    -DTARGET_OS_OSX=1 -D__OBJC2__ -DSUPPORT_PREOPT -DNDEBUG \
    -DSUPPORT_DIRECT_THREAD_KEYS -DSUPPORT_RETURN_AUTORELEASE \
    -DSUPPORT_NONPOINTER_ISA \
    -std=gnu++14 \
    -c "$SCRIPT_DIR/runtime/objc-os.mm" \
    -o "$SCRIPT_DIR/runtime/objc-os.o"

echo "Compiling objc-runtime-new.mm..."
"$TOOLS/clang++" -ObjC++ --sysroot="$RAVYN_SDKROOT" \
    -mmacos-version-min=10.15 \
    -momit-leaf-frame-pointer \
    -fdollars-in-identifiers \
    -fobjc-legacy-dispatch \
    -fno-objc-convert-messages-to-runtime-calls \
    -fblocks \
    -Wno-deprecated-declarations \
    -I"$SCRIPT_DIR/runtime" \
    -I"$SCRIPT_DIR/../../Kernel/xnu/bsd" \
    -I"$SCRIPT_DIR/../Libsystem/libsystem_kernel/mig_hdr/include" \
    -I"$SCRIPT_DIR/../Libsystem/private" \
    -I"$SCRIPT_DIR/../Libsystem/libsystem_blocks" \
    -I"$RAVYN_SDKROOT/usr/local/include" \
    -I"$RAVYN_SDKROOT/usr/local/include/kernel" \
    -I"$RAVYN_SDKROOT/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders" \
    -I"$ROOT_BINARY_DIR/x86_64" \
    -D_LIBC_NO_FEATURE_VERIFICATION -DTARGET_OS_IOSMAC=0 \
    -DTARGET_OS_OSX=1 -D__OBJC2__ -DSUPPORT_PREOPT -DNDEBUG \
    -DSUPPORT_DIRECT_THREAD_KEYS -DSUPPORT_RETURN_AUTORELEASE \
    -DSUPPORT_NONPOINTER_ISA \
    -std=gnu++14 \
    -c "$SCRIPT_DIR/runtime/objc-runtime-new.mm" \
    -o "$SCRIPT_DIR/runtime/objc-runtime-new.o"

echo "Linking libobjc.A.dylib..."
OBJS=("$SCRIPT_DIR"/runtime/*.o "$SCRIPT_DIR"/runtime/*/*.o)

"$TOOLS/clang" -dynamiclib -fuse-ld="$TOOLS/ld64" --ld-path="$TOOLS/ld64" \
    --sysroot="$RAVYN_SDKROOT" \
    -mmacos-version-min=10.15 \
    -L"$RAVYN_SDKROOT/usr/lib" \
    -Wl,-not_for_dyld_shared_cache \
    -Wl,-undefined,dynamic_lookup \
    -Wl,-install_name,/usr/lib/libobjc.dylib \
    -Wl,-current_version,228 \
    -Wl,-compatibility_version,1 \
    -Wl,-sectalign,__DATA,__data,0x1000 \
    -lobjc-trampolines -lSystem \
    -o "$SCRIPT_DIR/libobjc.A.dylib" \
    "${OBJS[@]}"

echo "Installing libobjc.A.dylib..."
sync
cp -fv "$SCRIPT_DIR/libobjc.A.dylib" "$RAVYN_SDKROOT/usr/lib/"
cp -fv "$SCRIPT_DIR/libobjc.A.dylib" "$SYSROOT_DIR/usr/lib/"
if [ -d "$ROOT_BINARY_DIR/iso_staging/usr/lib" ]; then
    cp -fv "$SCRIPT_DIR/libobjc.A.dylib" "$ROOT_BINARY_DIR/iso_staging/usr/lib/"
    ln -sf libobjc.A.dylib "$ROOT_BINARY_DIR/iso_staging/usr/lib/libobjc.dylib"
fi
ln -sf libobjc.A.dylib "$RAVYN_SDKROOT/usr/lib/libobjc.dylib"
ln -sf libobjc.A.dylib "$SYSROOT_DIR/usr/lib/libobjc.dylib"
echo "Done!"
