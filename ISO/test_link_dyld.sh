#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$HOME/build/darwinos}"
TOOLCHAIN="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain"
TOOLS="$TOOLCHAIN/usr/bin"
RAVYN_SDKROOT="$ROOT_BINARY_DIR/Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk"

cd "$ROOT_SOURCE_DIR/Libraries/dyld/dyld"
rm -f dyld_test dyld

"$TOOLS/clang" -fuse-ld="$TOOLS/ld64" -o dyld \
    dyld3/BootArgs.o dyld3/Closure.o dyld3/ClosureBuilder.o \
    dyld3/ClosureFileSystemPhysical.o dyld3/ClosureWriter.o \
    dyld3/Diagnostics.o dyld3/Loading.o dyld3/MachOAnalyzer.o \
    dyld3/MachOFile.o dyld3/MachOLoaded.o dyld3/PathOverrides.o \
    dyld3/SharedCacheRuntime.o dyld3/Tracing.o \
    dyld3/shared-cache/DyldSharedCache.o dyld3/shared-cache/FileUtils.o \
    src/ImageLoader.o src/ImageLoaderMachO.o src/ImageLoaderMachOClassic.o \
    src/ImageLoaderMachOCompressed.o src/ImageLoaderMegaDylib.o \
    src/cc_stubs.o src/cxx_stubs.o src/dyld2.o src/dyldAPIs.o \
    src/dyldExceptions.o src/dyldInitialization.o src/dyldNew.o \
    src/dyldStartup.o src/dyld_debugger.o src/glue.o \
    src/mh_dylib_header_alias.o src/missing_symbol_stubs.o \
    src/stub_binding_helper.o \
    -Wl,-static -Wl,-dylinker -Wl,-data_const \
    -Wl,-pagezero_size,0 -Wl,-image_base,0x7fff5fc00000 -nodefaultlibs \
    -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-dead_strip \
    -Wl,-exported_symbol,__dyld_start -e __dyld_start \
    -Wl,-exported_symbols_list,"$ROOT_SOURCE_DIR/Libraries/dyld/src/dyld.exp" \
    -Wl,-section_order,__DATA,__all_image_info:__nl_symbol_ptr:__got:__auth_ptr:__const:__crash_info:__data:__bss:__common \
    -mmacos-version-min=10.15 \
    -L"$RAVYN_SDKROOT/usr/local/lib/dyld" -lc -lpthread -lplatform \
    -L"$RAVYN_SDKROOT/usr/lib/system" -lCrashReporterClient \
    -L"$RAVYN_SDKROOT/usr/lib" -lunwind \
    -L"$RAVYN_SDKROOT/usr/local/lib/kernel" -lkernel \
    --ld-path="$TOOLS/ld64" --sysroot="$RAVYN_SDKROOT" \
    -mmacosx-version-min=10.15 -Wl,-not_for_dyld_shared_cache

ls -la dyld
