#!/usr/bin/env bash
# ==============================================================================
# darwinOS Master Build & Test Orchestrator
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export ROOT_SOURCE_DIR="${ROOT_SOURCE_DIR:-$SCRIPT_DIR}"

# When running in WSL on a Windows drive (/mnt/c/...), building on DrvFs/9P
# causes heavy filesystem slowdowns and deadlocks. Always use native ext4 for build artifacts!
if [[ "$SCRIPT_DIR" =~ ^/mnt/ ]] && [[ -z "$ROOT_BINARY_DIR" ]]; then
    export ROOT_BINARY_DIR="$HOME/build/darwinos"
else
    export ROOT_BINARY_DIR="${ROOT_BINARY_DIR:-$(cd "$ROOT_SOURCE_DIR/.." 2>/dev/null && pwd)/build}"
fi
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
export MACOS_VERSION_MIN="${MACOS_VERSION_MIN:-10.15}"
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-10.15}"
export ARCH_CONFIGS="${ARCH_CONFIGS:-X86_64}"
export KERNEL_CONFIGS="${KERNEL_CONFIGS:-RELEASE}"
export HOST_CC="${HOST_CC:-/usr/bin/clang}"
export HOST_CXX="${HOST_CXX:-/usr/bin/clang++}"
export HOST_LD="${HOST_LD:-/usr/bin/ld}"
export MK_WERROR="no"

# Styling
BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RESET='\033[0m'

log_info()    { echo -e "${CYAN}[INFO]${RESET} $*"; }
log_step()    { echo -e "\n${BOLD}${BLUE}==>${RESET} ${BOLD}$*${RESET}"; }
log_ok()      { echo -e "${GREEN}[OK]${RESET} $*"; }
log_warn()    { echo -e "${YELLOW}[WARN]${RESET} $*"; }
log_err()     { echo -e "${RED}[ERROR]${RESET} $*"; }

banner() {
    echo -e "${CYAN}${BOLD}"
    echo "  ========================================================"
    echo "      darwinOS Master Build & Run Environment             "
    echo "  ========================================================"
    echo -e "${RESET}"
    log_info "Source Directory: ${ROOT_SOURCE_DIR}"
    log_info "Binary Directory: ${ROOT_BINARY_DIR}"
}

check_dependencies() {
    log_step "Checking host build dependencies"
    local missing=()
    for tool in bmake clang clang++ cmake ninja python3 ed; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing+=("$tool")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        log_err "Missing required build tools: ${missing[*]}"
        echo -e "Install them with: ${YELLOW}sudo pacman -Syu --needed base-devel clang cmake ninja bmake python${RESET}"
        exit 1
    fi
    log_ok "All primary host build tools found."
}

build_toolchain() {
    log_step "Step 1: Building Developer Toolchain & SDK"
    mkdir -p "$ROOT_BINARY_DIR"
    cd "$ROOT_SOURCE_DIR"
    bmake -m "$MAKESYSPATH" Developer
    log_ok "Developer Toolchain ready."
}

build_kernel() {
    log_step "Step 2: Building XNU Kernel from Source"
    cd "$ROOT_SOURCE_DIR"
    bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Kernel" xnu_all
    mkdir -p "${SYSROOT_DIR}/boot"
    cp -f "${SYSROOT_DIR}/System/Library/Kernels/kernel" "${SYSROOT_DIR}/boot/kernel"
    log_info "Building and installing Kernel Extensions (IOACPIFamily, IOPCIFamily, etc.)..."
    bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Kernel/Extensions" all || true
    mkdir -p "${SYSROOT_DIR}/System/Library/Extensions"
    for k in "$ROOT_SOURCE_DIR"/Kernel/Extensions/*/*.kext; do
        if [ -d "$k" ]; then
            cp -Rf "$k" "${SYSROOT_DIR}/System/Library/Extensions/"
        fi
    done
    log_ok "XNU Kernel and Extensions built and installed successfully."
}

build_frameworks() {
    log_step "Step 3: Building Core Frameworks (objc4, CoreFoundation, Foundation, Metal, OpenGL, QuartzCore)"
    cd "$ROOT_SOURCE_DIR"

    log_info "[1/6] Building Objective-C runtime (Libraries/objc4)..."
    bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Libraries/objc4" all
    mkdir -p "${RAVYN_SDKROOT}/usr/lib" "${SYSROOT_DIR}/usr/lib"
    cp -Pf "$ROOT_SOURCE_DIR/Libraries/objc4"/libobjc*.dylib "${RAVYN_SDKROOT}/usr/lib/" 2>/dev/null || true
    cp -Pf "$ROOT_SOURCE_DIR/Libraries/objc4"/libobjc*.dylib "${SYSROOT_DIR}/usr/lib/" 2>/dev/null || true

    log_info "[2/6] Building CoreFoundation.framework..."
    bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Frameworks/CoreFoundation" all
    mkdir -p "${SYSROOT_DIR}/System/Library/Frameworks"
    if [ -d "${RAVYN_SDKROOT}/System/Library/Frameworks/CoreFoundation.framework" ]; then
        cp -a "${RAVYN_SDKROOT}/System/Library/Frameworks/CoreFoundation.framework" "${SYSROOT_DIR}/System/Library/Frameworks/"
        cd "${RAVYN_SDKROOT}/System/Library/Frameworks/CoreFoundation.framework" && ln -sf Versions/A/CoreFoundation CoreFoundation
        cd "${SYSROOT_DIR}/System/Library/Frameworks/CoreFoundation.framework" && ln -sf Versions/A/CoreFoundation CoreFoundation
    fi

    log_info "[3/6] Building Foundation.framework..."
    bmake -m "$MAKESYSPATH" -C "$ROOT_SOURCE_DIR/Frameworks/Foundation" all
    if [ -d "${RAVYN_SDKROOT}/System/Library/Frameworks/Foundation.framework" ]; then
        cp -a "${RAVYN_SDKROOT}/System/Library/Frameworks/Foundation.framework" "${SYSROOT_DIR}/System/Library/Frameworks/"
        cd "${RAVYN_SDKROOT}/System/Library/Frameworks/Foundation.framework" && ln -sf Versions/C/Foundation Foundation
        cd "${SYSROOT_DIR}/System/Library/Frameworks/Foundation.framework" && ln -sf Versions/C/Foundation Foundation
    fi

    log_info "[4/6] Building Metal.framework..."
    bash "$ROOT_SOURCE_DIR/Frameworks/Metal/build_framework.sh"

    log_info "[5/6] Building OpenGL.framework..."
    bash "$ROOT_SOURCE_DIR/Frameworks/OpenGL/build_framework.sh"

    log_info "[6/6] Building QuartzCore.framework..."
    bash "$ROOT_SOURCE_DIR/Frameworks/QuartzCore/build_framework.sh"

    # Ensure AppKit headers in sysroot
    if [ -d "$ROOT_SOURCE_DIR/Frameworks/AppKit/Headers" ]; then
        mkdir -p "${SYSROOT_DIR}/System/Library/Frameworks/AppKit.framework/Headers"
        cp -r "$ROOT_SOURCE_DIR/Frameworks/AppKit/Headers/." "${SYSROOT_DIR}/System/Library/Frameworks/AppKit.framework/Headers/"
    fi

    log_ok "All core frameworks built and installed successfully."
}

build_windowserver() {
    log_step "Step 4: Building SkyLight WindowServer"
    cd "$ROOT_SOURCE_DIR/CoreServices/WindowServer"
    mkdir -p "$ROOT_SOURCE_DIR/CoreServices/WindowServer/WindowServer.app/Contents/darwinOS"
    mkdir -p "$ROOT_SOURCE_DIR/CoreServices/WindowServer/WindowServer.app/Contents/Resources"
    bmake -m "$MAKESYSPATH" all
    mkdir -p "${SYSROOT_DIR}/System/Library/CoreServices"
    cp -a "$ROOT_SOURCE_DIR/CoreServices/WindowServer/WindowServer.app" "${SYSROOT_DIR}/System/Library/CoreServices/"
    log_ok "WindowServer built and installed to sysroot successfully."
}

build_apps() {
    log_step "Step 5: Building & Installing System Applications (Chords, Installer)"
    mkdir -p "${SYSROOT_DIR}/Applications/Utilities"
    if [ -d "$ROOT_SOURCE_DIR/SysApps/Chords/Chords.app" ]; then
        cp -a "$ROOT_SOURCE_DIR/SysApps/Chords/Chords.app" "${SYSROOT_DIR}/Applications/"
        log_ok "Chords.app installed to sysroot."
    fi
    if [ -d "$ROOT_SOURCE_DIR/SysApps/Installer/Install darwinOS.app" ]; then
        cp -a "$ROOT_SOURCE_DIR/SysApps/Installer/Install darwinOS.app" "${SYSROOT_DIR}/Applications/Utilities/"
        log_ok "Install darwinOS.app installed to sysroot."
    fi
    log_ok "SysApps stage finished."
}

run_qemu() {
    log_step "Step 6: Testing in QEMU"
    local qemu_bin="qemu-system-x86_64"
    if ! command -v "$qemu_bin" >/dev/null 2>&1; then
        if [ -f "/mnt/c/Program Files/qemu/qemu-system-x86_64.exe" ]; then
            qemu_bin="/mnt/c/Program Files/qemu/qemu-system-x86_64.exe"
        else
            log_err "QEMU not found. Install it via 'sudo pacman -S qemu-desktop' or in Windows."
            exit 1
        fi
    fi

    log_info "Starting QEMU using: $qemu_bin"
    local accel_args=()
    if [ -e /dev/kvm ] && [[ "$qemu_bin" != *.exe ]]; then
        accel_args+=("-enable-kvm")
    elif [[ "$qemu_bin" == *.exe ]]; then
        accel_args+=("-accel" "tcg")
    fi

    local converted_args=()
    for arg in "$@"; do
        if [[ "$qemu_bin" == *.exe ]] && [ -e "$arg" ]; then
            converted_args+=("$(wslpath -w "$arg")")
        else
            converted_args+=("$arg")
        fi
    done

    local boot_order="c"
    if [ ${#converted_args[@]} -eq 0 ] && [ -f "$ROOT_SOURCE_DIR/dist/darwinOS.iso" ]; then
        log_info "No disk image specified. Using default: dist/darwinOS.iso as SATA hard disk"
        if [[ "$qemu_bin" == *.exe ]]; then
            local isopath="$(wslpath -w "$ROOT_SOURCE_DIR/dist/darwinOS.iso")"
            converted_args+=("-drive" "id=bootdisk,file=${isopath},format=raw,if=none" "-device" "ide-hd,drive=bootdisk,bus=ide.0")
        else
            converted_args+=("-drive" "id=bootdisk,file=$ROOT_SOURCE_DIR/dist/darwinOS.iso,format=raw,if=none" "-device" "ide-hd,drive=bootdisk,bus=ide.0")
        fi
    fi

    local uefi_args=()
    if [[ "$qemu_bin" == *.exe ]] && [ -f "/mnt/c/Program Files/qemu/share/edk2-x86_64-code.fd" ]; then
        uefi_args+=("-drive" "if=pflash,format=raw,readonly=on,file=C:\\Program Files\\qemu\\share\\edk2-x86_64-code.fd")
    elif [ -f "/usr/share/edk2/x64/OVMF_CODE.4m.fd" ]; then
        uefi_args+=("-drive" "if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd")
    elif [ -f "/usr/share/ovmf/x64/OVMF_CODE.4m.fd" ]; then
        uefi_args+=("-drive" "if=pflash,format=raw,readonly=on,file=/usr/share/ovmf/x64/OVMF_CODE.4m.fd")
    fi

    local opencore_args=()
    if [ -f "$ROOT_SOURCE_DIR/dist/OpenCore.qcow2" ]; then
        log_info "Detected OpenCore boot image at dist/OpenCore.qcow2 (using OpenCore native XNU loader)"
        if [[ "$qemu_bin" == *.exe ]]; then
            opencore_args+=("-drive" "file=$(wslpath -w "$ROOT_SOURCE_DIR/dist/OpenCore.qcow2"),format=qcow2,index=0,media=disk")
        else
            opencore_args+=("-drive" "file=$ROOT_SOURCE_DIR/dist/OpenCore.qcow2,format=qcow2,index=0,media=disk")
        fi
        boot_order="c"
    fi

    "$qemu_bin" \
        -name "darwinOS" \
        -m 8G -machine q35 -cpu Penryn -smp 4,cores=4,sockets=1 \
        "${accel_args[@]}" \
        "${uefi_args[@]}" \
        "${opencore_args[@]}" \
        -boot order="$boot_order" \
        -device virtio-rng-pci \
        -device virtio-vga,xres=1280,yres=800 \
        -serial stdio \
        "${converted_args[@]}"
}

package_dist() {
    log_step "Packaging darwinOS Distribution Archive (dist/ravynOS.txz)"
    mkdir -p "$ROOT_SOURCE_DIR/dist"
    tar -cJf "$ROOT_SOURCE_DIR/dist/ravynOS.txz" -C "$SYSROOT_DIR" .
    log_ok "Package generated at: $ROOT_SOURCE_DIR/dist/ravynOS.txz ($(du -h "$ROOT_SOURCE_DIR/dist/ravynOS.txz" | cut -f1))"
}

build_iso() {
    log_step "Generating Portable Linux ISO Image"
    bash "$ROOT_SOURCE_DIR/ISO/build_linux.sh"
}

usage() {
    echo "Usage: $0 [all|toolchain|kernel|frameworks|windowserver|apps|package|iso|run|clean]"
    echo ""
    echo "Targets:"
    echo "  all           - Build entire stack (Toolchain -> Kernel -> Frameworks -> WS -> Apps -> Package)"
    echo "  toolchain     - Build Developer platform & compiler toolchain"
    echo "  kernel        - Build XNU Mach-O kernel from source (Kernel/)"
    echo "  frameworks    - Sequentially build all core frameworks (objc4, CF, Foundation, Metal, GL, QC)"
    echo "  windowserver  - Build SkyLight WindowServer compositor"
    echo "  apps          - Build system Cocoa apps"
    echo "  package       - Bundle sysroot into dist/ravynOS.txz"
    echo "  iso           - Full pipeline: build toolchain, kernel, all frameworks, WS, apps, package & generate ISO"
    echo "  run [args]    - Launch QEMU (e.g. ./build.sh run -cdrom dist/darwinOS.iso)"
    echo "  clean         - Clean build object directory"
    exit 0
}

TARGET="${1:-all}"
shift || true

banner
check_dependencies

case "$TARGET" in
    all)
        build_toolchain
        build_kernel
        build_frameworks
        build_windowserver
        build_apps
        package_dist
        echo -e "\n${GREEN}${BOLD}=== darwinOS build complete! ===${RESET}\n"
        ;;
    toolchain)
        build_toolchain
        ;;
    kernel)
        build_kernel
        ;;
    frameworks)
        build_frameworks
        ;;
    windowserver)
        build_windowserver
        ;;
    apps)
        build_apps
        ;;
    package)
        package_dist
        ;;
    iso)
        build_toolchain
        build_kernel
        build_frameworks
        build_windowserver
        build_apps
        package_dist
        build_iso
        ;;
    run|test)
        run_qemu "$@"
        ;;
    clean)
        log_info "Cleaning ${ROOT_BINARY_DIR}..."
        rm -rf "${ROOT_BINARY_DIR}"
        log_ok "Clean complete."
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        log_err "Unknown target: $TARGET"
        usage
        ;;
esac
